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
