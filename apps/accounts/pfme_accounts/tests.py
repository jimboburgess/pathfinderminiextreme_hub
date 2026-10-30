import json,time,os,secrets
from django.test import TestCase,Client,override_settings
from django.contrib.auth.models import User
from django.contrib.auth.hashers import check_password
from django.core.cache import cache
from .models import AccountProfile

class AccountsTests(TestCase):
    @classmethod
    def setUpTestData(cls):
        cls.password=secrets.token_urlsafe(24)
        for i in range(10):
            u=User.objects.create_user('friend'+str(i),password=cls.password)
            AccountProfile.objects.create(user=u)
    def setUp(self):cache.clear()
    def client_for(self):return Client(HTTP_HOST='game.noburdenmusics.com',secure=True)
    def test_ten_independent_accounts_and_stable_ownership(self):
        owners=[]
        for i in range(10):
            c=self.client_for();self.assertTrue(c.login(username='friend'+str(i),password=self.password))
            owners.append(AccountProfile.objects.get(user__username='friend'+str(i)).principal)
        self.assertEqual(len(set(owners)),10)
        p=AccountProfile.objects.get(user__username='friend0');before=(p.principal,p.login)
        u=p.user;u.set_password(secrets.token_urlsafe(24));u.save();p.refresh_from_db()
        self.assertEqual((p.principal,p.login),before)
    def test_argon2_no_plaintext_and_wrong_password(self):
        u=User.objects.get(username='friend0');self.assertTrue(u.password.startswith('argon2$argon2id$'))
        self.assertNotIn('Only-a-generated-test-password',u.password)
        self.assertFalse(self.client_for().login(username='friend0',password='incorrect'))
    def test_disable_and_password_reset_revoke_sessions(self):
        from .asgi import authenticated
        for change in ('disable','password'):
            c=self.client_for();u=User.objects.get(username='friend0');u.is_active=True;u.set_password(self.password);u.save()
            c.login(username=u.username,password=self.password)
            cookie='__Host-pfme_session='+c.cookies['__Host-pfme_session'].value
            self.assertIsNotNone(authenticated(cookie))
            if change=='disable':u.is_active=False
            else:u.set_password(secrets.token_urlsafe(24))
            u.save();self.assertIsNone(authenticated(cookie))
    def test_csrf_and_strict_host_origin(self):
        c=Client(enforce_csrf_checks=True,HTTP_HOST='game.noburdenmusics.com')
        self.assertEqual(c.post('/accounts/login/',{'username':'friend0','password':'incorrect'},secure=True).status_code,403)
        self.assertEqual(c.get('/accounts/login/',secure=True,HTTP_HOST='evil.invalid').status_code,400)
        self.assertEqual(c.get('/accounts/login/',secure=True,HTTP_ORIGIN='https://evil.invalid').status_code,403)
        self.assertEqual(c.get('/accounts/login/',secure=True).status_code,200)
    def test_login_rate_limit(self):
        c=self.client_for()
        for i in range(5):self.assertEqual(c.post('/accounts/login/',{'username':'friend0','password':'wrong'},secure=True).status_code,200)
        self.assertEqual(c.post('/accounts/login/',{'username':'friend0','password':'wrong'},secure=True).status_code,429)
    def test_cookie_flags_and_no_registration_or_public_status(self):
        c=self.client_for();r=c.post('/accounts/login/',{'username':'friend0','password':self.password},secure=True)
        self.assertEqual(r.status_code,302);cookie=r.cookies['__Host-pfme_session']
        self.assertTrue(cookie['secure']);self.assertTrue(cookie['httponly']);self.assertEqual(cookie['samesite'],'Strict');self.assertEqual(cookie['path'],'/')
        for path in ('/register','/status','/admin','/api/status'):
            self.assertEqual(c.get(path,secure=True).status_code,404)

    def test_expiry_and_logout_invalidate_session(self):
        from .asgi import authenticated
        from django.contrib.sessions.models import Session
        from django.utils import timezone
        from datetime import timedelta
        c=self.client_for();c.login(username='friend0',password=self.password)
        key=c.cookies['__Host-pfme_session'].value;cookie='__Host-pfme_session='+key
        Session.objects.filter(session_key=key).update(expire_date=timezone.now()-timedelta(seconds=1))
        self.assertIsNone(authenticated(cookie))
        c.login(username='friend0',password=self.password);cookie='__Host-pfme_session='+c.cookies['__Host-pfme_session'].value
        self.assertEqual(c.post('/accounts/logout/',secure=True).status_code,302)
        self.assertIsNone(authenticated(cookie))

    def test_disable_enable_does_not_restore_revoked_session(self):
        from .asgi import authenticated
        from django.core.management import call_command
        from io import StringIO
        c=self.client_for();c.login(username='friend0',password=self.password)
        cookie='__Host-pfme_session='+c.cookies['__Host-pfme_session'].value
        for operation in ('disable','enable'):call_command('friend',operation,'friend0',stdout=StringIO())
        self.assertIsNone(authenticated(cookie))
        self.assertTrue(self.client_for().login(username='friend0',password=self.password))

    def test_remember_me_is_optional_and_uses_fixed_expiration(self):
        from django.contrib.sessions.models import Session
        from django.utils import timezone
        for remember,seconds in ((False,14400),(True,604800)):
            c=self.client_for()
            page=c.get('/accounts/login/',secure=True)
            self.assertContains(page,'Remember me for 7 days')
            self.assertFalse(page.context['form'].fields['remember_me'].initial)
            data={'username':'friend0','password':self.password}
            if remember:data['remember_me']='on'
            r=c.post('/accounts/login/',data,secure=True)
            self.assertEqual(r.status_code,302)
            cookie=r.cookies['__Host-pfme_session']
            self.assertTrue(cookie['secure']);self.assertTrue(cookie['httponly'])
            self.assertEqual(cookie['samesite'],'Strict')
            self.assertAlmostEqual(int(cookie['max-age']),seconds,delta=2)
            session=Session.objects.get(session_key=cookie.value)
            self.assertAlmostEqual((session.expire_date-timezone.now()).total_seconds(),seconds,delta=2)
            deadline=session.expire_date
            c.get('/accounts/presence/',secure=True)
            session.refresh_from_db();self.assertEqual(session.expire_date,deadline)
            # A new browser client can reuse this device's persistent cookie.
            reopened=self.client_for();reopened.cookies['__Host-pfme_session']=cookie.value
            self.assertEqual(reopened.get('/accounts/presence/',secure=True).status_code,200)

    def test_remembered_session_expiry_and_individual_revocation(self):
        from .asgi import authenticated
        from django.contrib.sessions.models import Session
        from django.core.management import call_command
        from django.utils import timezone
        from datetime import timedelta
        from io import StringIO
        other=self.client_for();other.login(username='friend1',password=self.password)
        other_cookie='__Host-pfme_session='+other.cookies['__Host-pfme_session'].value
        for action in ('expire','logout','password','disable'):
            u=User.objects.get(username='friend0');u.is_active=True;u.set_password(self.password);u.save()
            c=self.client_for();r=c.post('/accounts/login/',{'username':u.username,'password':self.password,'remember_me':'on'},secure=True)
            self.assertEqual(r.status_code,302)
            key=c.cookies['__Host-pfme_session'].value;cookie='__Host-pfme_session='+key
            self.assertIsNotNone(authenticated(cookie))
            if action=='expire':Session.objects.filter(session_key=key).update(expire_date=timezone.now()-timedelta(seconds=1))
            elif action=='logout':c.post('/accounts/logout/',secure=True)
            elif action=='password':u.set_password(secrets.token_urlsafe(24));u.save()
            else:call_command('friend','disable',u.username,stdout=StringIO())
            self.assertIsNone(authenticated(cookie))
            self.assertIsNotNone(authenticated(other_cookie))


class InvitationTests(TestCase):
    def setUp(self):
        import tempfile
        from pathlib import Path
        from .invitations import issue
        cache.clear();self.directory=tempfile.TemporaryDirectory(prefix='pfme-invite-test-');self.addCleanup(self.directory.cleanup)
        self.policy=Path(self.directory.name)/'admission.json'
        self.override=override_settings(PFME_ADMISSION_FILE=str(self.policy));self.override.enable();self.addCleanup(self.override.disable)
        self.invite,self.token=issue('Test friend');self.profile=self.invite.user.accountprofile
        self.policy.write_text(json.dumps({'users':[{'login':self.profile.login,'principal':self.profile.principal}]}))
        self.password=secrets.token_urlsafe(24)
    def client_for(self):return Client(HTTP_HOST='game.noburdenmusics.com',secure=True)
    def post(self,token=None,username='newfriend',**extra):
        return self.client_for().post('/accounts/register/',{'token':self.token if token is None else token,'username':username,'password1':self.password,'password2':self.password,**extra},secure=True)
    def test_reserved_account_and_token_storage(self):
        self.assertFalse(self.invite.user.has_usable_password())
        self.assertNotEqual(self.invite.token_digest,self.token)
        self.assertNotIn(self.token,json.dumps(list(__import__('pfme_accounts.models',fromlist=['Invitation']).Invitation.objects.values()),default=str))
        self.assertFalse(self.invite.user.is_staff);self.assertFalse(self.invite.user.is_superuser)
        from django.utils import timezone
        self.assertAlmostEqual((self.invite.expires_at-timezone.now()).total_seconds(),48*3600,delta=3)
    def test_claim_preserves_native_principal_and_requires_separate_login(self):
        before=self.policy.read_bytes();stable=self.profile.stable_id;uid=self.invite.user_id
        r=self.post(is_staff='true',is_superuser='true',user_id='999',principal='owner')
        self.assertEqual(r.status_code,302);self.assertEqual(r['Location'],'/accounts/login/?registered=1')
        u=User.objects.get(username='newfriend');self.assertEqual(u.pk,uid);self.assertTrue(u.check_password(self.password))
        self.assertFalse(u.is_staff);self.assertFalse(u.is_superuser);self.assertEqual(u.accountprofile.stable_id,stable)
        self.assertEqual(self.policy.read_bytes(),before)
        self.assertNotIn('__Host-pfme_session',r.cookies)
        self.invite.refresh_from_db();self.assertIsNotNone(self.invite.redeemed_at)
        c=self.client_for();self.assertTrue(c.login(username='newfriend',password=self.password))
    def test_replay_cannot_rename_or_take_over_existing_account(self):
        owner=User.objects.create_user('owner',password=secrets.token_urlsafe(24));original=owner.password
        self.assertEqual(self.post(username='owner').status_code,400)
        self.invite.refresh_from_db();self.assertIsNone(self.invite.redeemed_at)
        self.assertEqual(self.post().status_code,302)
        self.assertEqual(self.post(username='thief').status_code,400)
        owner.refresh_from_db();self.assertEqual(owner.password,original);self.assertFalse(User.objects.filter(username='thief').exists())
    def test_missing_malformed_expired_and_revoked_invites(self):
        from django.utils import timezone
        from datetime import timedelta
        self.assertEqual(self.post(token='').status_code,400);self.assertEqual(self.post(token='f'*64).status_code,400)
        self.invite.expires_at=timezone.now()-timedelta(seconds=1);self.invite.save()
        self.assertEqual(self.post().status_code,400)
        self.invite.expires_at=timezone.now()+timedelta(hours=1);self.invite.revoked_at=timezone.now();self.invite.save()
        self.assertEqual(self.post().status_code,400);self.assertFalse(User.objects.filter(username='newfriend').exists())
    def test_unadmitted_invitation_cannot_be_consumed(self):
        self.policy.write_text('{"users":[]}');self.assertEqual(self.post().status_code,400)
        self.invite.refresh_from_db();self.assertIsNone(self.invite.redeemed_at)
    def test_password_rules_csrf_origin_and_no_invitation_in_url(self):
        self.assertEqual(self.post(password1='x',password2='x').status_code,400)
        c=Client(enforce_csrf_checks=True,HTTP_HOST='game.noburdenmusics.com')
        self.assertEqual(c.post('/accounts/register/',{},secure=True).status_code,403)
        self.assertEqual(self.client_for().get('/accounts/register/',secure=True,HTTP_ORIGIN='https://evil.invalid').status_code,403)
        page=self.client_for().get('/accounts/register/',secure=True)
        self.assertContains(page,'id="registration"');self.assertEqual(page['Referrer-Policy'],'same-origin')
        script=self.client_for().get('/accounts/register.js',secure=True)
        self.assertContains(script,'history.replaceState');self.assertNotContains(script,'localStorage')
    def test_revoke_is_individual_and_used_invitation_is_not_account_disable(self):
        from django.core.management import call_command
        from django.core.management.base import CommandError
        from io import StringIO
        from .invitations import issue
        other,_=issue('Other friend')
        call_command('invite','revoke',str(other.pk),stdout=StringIO())
        other.refresh_from_db();self.assertFalse(other.user.is_active)
        self.assertEqual(self.post().status_code,302)
        with self.assertRaises(CommandError):call_command('invite','revoke',str(self.invite.pk),stdout=StringIO())
        self.assertTrue(User.objects.get(username='newfriend').is_active)
    def test_registration_rate_limit(self):
        for _ in range(20):self.assertEqual(self.post(token='f'*64).status_code,400)
        self.assertEqual(self.post(token='f'*64).status_code,429)

    def test_browser_registration_origin_and_csrf(self):
        import re
        from django.conf import settings
        c=Client(enforce_csrf_checks=True,HTTP_HOST='game.noburdenmusics.com')
        page=c.get('/accounts/register/',secure=True)
        self.assertEqual(page['Referrer-Policy'],'same-origin')
        csrf=re.search(r'name="csrfmiddlewaretoken" value="([^"]+)"',page.content.decode()).group(1)
        data={'token':self.token,'username':'browserfriend','password1':self.password,'password2':self.password,'csrfmiddlewaretoken':csrf}
        for origin in ('null','https://evil.invalid'):
            self.assertEqual(c.post('/accounts/register/',data,secure=True,HTTP_ORIGIN=origin).status_code,403)
            self.invite.refresh_from_db();self.assertIsNone(self.invite.redeemed_at)
        response=c.post('/accounts/register/',data,secure=True,HTTP_ORIGIN=settings.ORIGIN,HTTP_REFERER=settings.ORIGIN+'/accounts/register/')
        self.assertEqual(response.status_code,302)
        self.assertTrue(User.objects.get(username='browserfriend').check_password(self.password))

    def test_invitation_registration_accepts_two_numeric_characters(self):
        self.assertEqual(self.post(password1='12',password2='12').status_code,302)
        user=User.objects.get(username='newfriend')
        self.assertTrue(user.check_password('12'))
        self.assertTrue(user.password.startswith('argon2$argon2id$'))


class PasswordPolicyTests(TestCase):
    def setUp(self):cache.clear()
    def test_only_minimum_two_characters_is_required(self):
        from django.contrib.auth.password_validation import validate_password
        from django.core.exceptions import ValidationError
        from .forms import InvitationRegistrationForm
        for password in ('12','aa','password','policy_user','  ','éé'):
            validate_password(password,User(username='policy_user'))
            form=InvitationRegistrationForm({'token':'0'*64,'username':'policy_user','password1':password,'password2':password})
            self.assertTrue(form.is_valid(),form.errors.as_json())
        for password in ('','x'):
            with self.assertRaises(ValidationError):validate_password(password)
            form=InvitationRegistrationForm({'token':'0'*64,'username':'policy_user','password1':password,'password2':password})
            self.assertFalse(form.is_valid())
    def test_password_change_accepts_short_numeric_password(self):
        old=secrets.token_urlsafe(24)
        user=User.objects.create_user('policy_user',password=old);AccountProfile.objects.create(user=user)
        c=Client(HTTP_HOST='game.noburdenmusics.com',secure=True);self.assertTrue(c.login(username=user.username,password=old))
        r=c.post('/accounts/password/',{'old_password':old,'new_password1':'12','new_password2':'12'},secure=True)
        self.assertEqual(r.status_code,302)
        user.refresh_from_db();self.assertTrue(user.check_password('12'))
        self.assertTrue(user.password.startswith('argon2$argon2id$'))
    def test_host_create_and_reset_use_same_policy(self):
        from unittest.mock import patch
        from django.core.management import call_command
        from django.core.exceptions import ValidationError
        from io import StringIO
        for operation,password in (('create','aa'),('reset','12')):
            with patch('sys.stdin.isatty',return_value=True),patch('getpass.getpass',side_effect=[password,password]):
                call_command('friend',operation,'policy_user',stdout=StringIO())
            self.assertTrue(User.objects.get(username='policy_user').check_password(password))
        with patch('sys.stdin.isatty',return_value=True),patch('getpass.getpass',side_effect=['x','x']):
            with self.assertRaises(ValidationError):call_command('friend','reset','policy_user',stdout=StringIO())
        self.assertTrue(User.objects.get(username='policy_user').check_password('12'))
