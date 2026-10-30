"""Invitation claims rename a pre-admitted, password-unusable reserved account."""
import hashlib,json,re,secrets
from datetime import timedelta
from pathlib import Path
from django.conf import settings
from django.contrib.auth.models import User
from django.core.exceptions import ValidationError
from django.db import transaction
from django.utils import timezone
from .models import AccountProfile,Invitation


def digest(token):return hashlib.sha256(token.encode('ascii')).hexdigest()


def issue(label):
    if not re.fullmatch(r'[A-Za-z0-9 ._-]{1,48}',label):raise ValidationError('Use a short label with letters, digits, spaces, dot, underscore or hyphen.')
    token=secrets.token_hex(32)
    with transaction.atomic():
        if User.objects.count()>=32:raise ValidationError('Account limit reached (32, including reserved invitations).')
        user=User(username='invite_'+secrets.token_hex(10));user.set_unusable_password();user.save()
        AccountProfile.objects.create(user=user)
        invitation=Invitation.objects.create(user=user,label=label,token_digest=digest(token),expires_at=timezone.now()+timedelta(hours=48))
    return invitation,token


def admitted(profile):
    try:
        text=Path(settings.PFME_ADMISSION_FILE).read_text()
        if len(text)>32768:return False
        policy=json.loads(text)
        return any(u.get('login')==profile.login and u.get('principal')==profile.principal for u in policy['users'])
    except (OSError,ValueError,KeyError,TypeError,AttributeError):return False


def claim(token,username,password):
    if not re.fullmatch('[0-9a-f]{64}',token):raise ValidationError('This invitation is invalid or unavailable.')
    with transaction.atomic():
        now=timezone.now()
        invite=Invitation.objects.select_related('user','user__accountprofile').filter(token_digest=digest(token)).first()
        if not invite or invite.redeemed_at or invite.revoked_at or invite.expires_at<=now or not invite.user.is_active or invite.user.has_usable_password():
            raise ValidationError('This invitation is invalid, expired or already used. Ask the host for a new link.')
        if not admitted(invite.user.accountprofile):raise ValidationError('This invitation is not ready. Ask the host to synchronize admission.')
        # Conditional write serializes competing claims; all later failures roll it back.
        if Invitation.objects.filter(pk=invite.pk,redeemed_at__isnull=True,revoked_at__isnull=True,expires_at__gt=now).update(redeemed_at=now)!=1:
            raise ValidationError('This invitation is unavailable.')
        user=invite.user;user.username=username;user.set_password(password);user.save(update_fields=['username','password'])
        return user
