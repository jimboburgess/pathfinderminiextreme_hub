"""Administrator-only interactive account creation, reset and disable."""
import getpass,re
from django.core.management.base import BaseCommand,CommandError
from django.contrib.auth.models import User
from django.contrib.auth.password_validation import validate_password
from django.db import transaction
from pfme_accounts.models import AccountProfile
class Command(BaseCommand):
    help='Create/reset/disable/enable/list invited accounts. Run only from the host Compose CLI.'
    def add_arguments(self,p):
        p.add_argument('operation',choices=['create','reset','disable','enable','list'])
        p.add_argument('username',nargs='?')
    def handle(self,*args,**options):
        operation=options['operation'];name=options.get('username')
        if operation=='list':
            for u in User.objects.order_by('username'):self.stdout.write(u.username+(' enabled' if u.is_active else ' disabled'))
            return
        if not name or not re.fullmatch('[a-z][a-z0-9_-]{2,31}',name):raise CommandError('Use a lowercase username, 3–32 letters/digits/underscore/hyphen.')
        if operation=='create':
            if User.objects.filter(username=name).exists():raise CommandError('Username exists.')
            if User.objects.count()>=32:raise CommandError('Account limit32; gameplay capacity remains4.')
            user=User(username=name)
        else:
            try:user=User.objects.get(username=name)
            except User.DoesNotExist:raise CommandError('Unknown username')
        if operation in ('create','reset'):
            import sys
            if not sys.stdin.isatty():raise CommandError('Use an interactive terminal; never pass passwords as arguments.')
            password=getpass.getpass('New password (hidden): ')
            if password!=getpass.getpass('Repeat password: '):raise CommandError('Passwords differ.')
            validate_password(password,user);user.set_password(password);del password
        if operation=='disable':user.is_active=False
        if operation=='enable':user.is_active=True
        with transaction.atomic():
            user.save();AccountProfile.objects.get_or_create(user=user)
        if operation in ('disable','reset'):
            from django.contrib.sessions.models import Session
            for session in Session.objects.iterator(chunk_size=100):
                if session.get_decoded().get('_auth_user_id')==str(user.pk):session.delete()
        self.stdout.write('Account updated. Sync admission using the host helper; existing password-reset sessions are invalid.')
