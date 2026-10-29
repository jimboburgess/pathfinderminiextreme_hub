import os
from pathlib import Path
from urllib.parse import urlsplit
BASE_DIR=Path(__file__).resolve().parent
STATE=Path(os.environ.get('PFME_STATE_DIR','/state'))
ORIGIN=os.environ.get('PFME_ORIGIN','https://game.noburdenmusics.com')
url=urlsplit(ORIGIN)
if url.scheme!='https' or url.netloc!=url.hostname or url.path or url.query or url.fragment:
    raise RuntimeError('Exact HTTPS origin required')
SECRET_KEY=(STATE/'secret.key').read_text().strip()
if len(SECRET_KEY)<48: raise RuntimeError('Missing application secret')
DEBUG=False
ALLOWED_HOSTS=[url.hostname]
ROOT_URLCONF='pfme_accounts.urls'
INSTALLED_APPS=['django.contrib.auth','django.contrib.contenttypes','django.contrib.sessions','pfme_accounts']
MIDDLEWARE=['pfme_accounts.views.BoundaryMiddleware','django.middleware.security.SecurityMiddleware',
 'django.contrib.sessions.middleware.SessionMiddleware','django.middleware.csrf.CsrfViewMiddleware',
 'django.contrib.auth.middleware.AuthenticationMiddleware','django.middleware.clickjacking.XFrameOptionsMiddleware']
DATABASES={'default':{'ENGINE':'django.db.backends.sqlite3','NAME':str(STATE/'accounts.sqlite3'),'OPTIONS':{'timeout':10}}}
PASSWORD_HASHERS=['django.contrib.auth.hashers.Argon2PasswordHasher']
AUTH_PASSWORD_VALIDATORS=[{'NAME':'django.contrib.auth.password_validation.MinimumLengthValidator','OPTIONS':{'min_length':12}},
 {'NAME':'django.contrib.auth.password_validation.CommonPasswordValidator'},
 {'NAME':'django.contrib.auth.password_validation.NumericPasswordValidator'},
 {'NAME':'django.contrib.auth.password_validation.UserAttributeSimilarityValidator'}]
TEMPLATES=[{'BACKEND':'django.template.backends.django.DjangoTemplates','DIRS':[BASE_DIR/'templates'],
 'APP_DIRS':True,'OPTIONS':{'context_processors':['django.template.context_processors.csrf','django.template.context_processors.request']}}]
SESSION_COOKIE_NAME='__Host-pfme_session'
SESSION_COOKIE_SECURE=True
SESSION_COOKIE_HTTPONLY=True
SESSION_COOKIE_SAMESITE='Strict'
SESSION_COOKIE_AGE=4*3600
SESSION_SAVE_EVERY_REQUEST=False
CSRF_COOKIE_NAME='__Host-pfme_csrf'
CSRF_COOKIE_SECURE=True
CSRF_COOKIE_HTTPONLY=True
CSRF_COOKIE_SAMESITE='Strict'
CSRF_TRUSTED_ORIGINS=[ORIGIN]
SECURE_CONTENT_TYPE_NOSNIFF=True
SECURE_SSL_REDIRECT=True
SECURE_HSTS_SECONDS=3600
SECURE_REFERRER_POLICY='same-origin'
X_FRAME_OPTIONS='DENY'
LOGIN_URL='/accounts/login/'
LOGIN_REDIRECT_URL='/play'
LOGOUT_REDIRECT_URL=LOGIN_URL
CACHES={'default':{'BACKEND':'django.core.cache.backends.locmem.LocMemCache','LOCATION':'pfme-login','OPTIONS':{'MAX_ENTRIES':1024}}}
USE_TZ=True
DEFAULT_AUTO_FIELD='django.db.models.BigAutoField'
# Never log requests, cookies, form bodies, database contents or auth exceptions.
LOGGING={'version':1,'disable_existing_loggers':True,'handlers':{'null':{'class':'logging.NullHandler'}},'root':{'handlers':['null']}}
