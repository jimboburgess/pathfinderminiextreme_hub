"""Host CLI only. Raw links are shown once to the operator; never stored."""
from django.core.management.base import BaseCommand,CommandError
from django.core.exceptions import ValidationError
from django.db import transaction
from django.conf import settings
from django.utils import timezone
from pfme_accounts.invitations import issue
from pfme_accounts.models import Invitation

class Command(BaseCommand):
    help='Create/list/revoke single-use 48-hour invitations. Share links privately.'
    def add_arguments(self,p):
        p.add_argument('operation',choices=['create','list','revoke'])
        p.add_argument('value',nargs='?')
    def handle(self,*args,**options):
        op=options['operation'];value=options.get('value')
        if op=='create':
            try:invite,token=issue(value or '')
            except ValidationError as e:raise CommandError('; '.join(e.messages))
            self.stdout.write('Invitation '+str(invite.pk)+' (48 hours). Synchronize native admission before sharing.')
            self.stdout.write(settings.ORIGIN+'/accounts/register/#'+token)
            return
        if op=='revoke':
            try:invite=Invitation.objects.get(pk=value)
            except (Invitation.DoesNotExist,ValidationError,ValueError,TypeError):raise CommandError('Unknown invitation ID.')
            with transaction.atomic():
                invite=Invitation.objects.select_for_update().get(pk=invite.pk)
                if invite.redeemed_at:raise CommandError('Already redeemed. Disable the chosen username with friend disable instead.')
                invite.revoked_at=timezone.now();invite.save(update_fields=['revoked_at'])
                invite.user.is_active=False;invite.user.save(update_fields=['is_active'])
            self.stdout.write('Invitation revoked. Synchronize native admission.');return
        for invite in Invitation.objects.select_related('user').order_by('created_at'):
            state='used' if invite.redeemed_at else 'revoked' if invite.revoked_at else 'expired' if invite.expires_at<=timezone.now() else 'pending'
            self.stdout.write(f'{invite.pk} | {invite.label} | {state} | '+(invite.user.username if invite.redeemed_at else 'unclaimed'))
