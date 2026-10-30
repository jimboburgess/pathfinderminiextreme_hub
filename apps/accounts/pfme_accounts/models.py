import uuid,hashlib
from django.conf import settings
from django.db import models

class AccountProfile(models.Model):
    user=models.OneToOneField(settings.AUTH_USER_MODEL,on_delete=models.CASCADE)
    stable_id=models.UUIDField(default=uuid.uuid4,unique=True,editable=False)
    @property
    def login(self):
        # Legacy native private-hop syntax; identity comes ONLY from Django auth.
        return 'cf-'+hashlib.sha256(('pfme-account-v1\0'+self.stable_id.hex).encode()).hexdigest()
    @property
    def principal(self): return 'acct-'+self.stable_id.hex


class Invitation(models.Model):
    id=models.UUIDField(primary_key=True,default=uuid.uuid4,editable=False)
    user=models.OneToOneField(settings.AUTH_USER_MODEL,on_delete=models.CASCADE)
    label=models.CharField(max_length=48)
    token_digest=models.CharField(max_length=64,unique=True)
    created_at=models.DateTimeField(auto_now_add=True)
    expires_at=models.DateTimeField()
    redeemed_at=models.DateTimeField(null=True,blank=True)
    revoked_at=models.DateTimeField(null=True,blank=True)
