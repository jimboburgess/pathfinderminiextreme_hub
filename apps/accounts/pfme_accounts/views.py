import asyncio,html,json,os,time
from pathlib import Path
from django.conf import settings
from django.http import JsonResponse,HttpResponse,HttpResponseForbidden,HttpResponseRedirect
from django.contrib.auth.views import LoginView,LogoutView,PasswordChangeView
from django.core.cache import cache
from django.middleware.csrf import get_token
from asgiref.sync import sync_to_async

class BoundaryMiddleware:
    def __init__(self,get_response):self.get_response=get_response
    def __call__(self,request):
        if request.get_host()!=settings.ALLOWED_HOSTS[0]:return HttpResponseForbidden('Invalid host')
        origin=request.headers.get('Origin')
        if origin and origin!=settings.ORIGIN:return HttpResponseForbidden('Invalid origin')
        response=self.get_response(request)
        response['Content-Security-Policy']="default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self' "+settings.ORIGIN.replace('https:','wss:')+"; form-action 'self'; base-uri 'none'; frame-ancestors 'none'"
        response['Cache-Control']='no-store'
        return response

def limited(key,maximum):
    cache.add(key,0,60)
    try:count=cache.incr(key)
    except ValueError:cache.set(key,1,60);count=1
    return count>maximum

class PlayerLogin(LoginView):
    template_name='login.html'
    redirect_authenticated_user=True
    def post(self,request,*args,**kwargs):
        # Global budget bounds random-user attacks; per-name budget bounds guessing.
        import hashlib
        key=hashlib.sha256(request.POST.get('username','').casefold().encode()).hexdigest()
        if limited('login-global',30) or limited('login-'+key,5):
            return HttpResponse('Too many login attempts. Please wait a minute.',status=429)
        return super().post(request,*args,**kwargs)

class PlayerPassword(PasswordChangeView):
    template_name='password.html'
    success_url='/play'
    def post(self,request,*args,**kwargs):
        if limited('password-global',20):return HttpResponse('Please wait a minute.',status=429)
        return super().post(request,*args,**kwargs)

async def page(request):
    user=await request.auser()
    if not user.is_authenticated:return HttpResponseRedirect(settings.LOGIN_URL)
    if request.path=='/':return HttpResponseRedirect('/play')
    from .models import AccountProfile
    try:profile=await AccountProfile.objects.aget(user_id=user.pk)
    except AccountProfile.DoesNotExist:return HttpResponseForbidden('Account is not configured.')
    writer=None
    try:
        reader,writer=await asyncio.wait_for(asyncio.open_unix_connection(os.environ.get('PFME_BACKEND','/back/backend.sock')),3)
        writer.write(('GET '+request.path+' HTTP/1.1\r\nHost: localhost\r\nPfme-Verified-Identity: '+profile.login+'\r\nConnection: close\r\n\r\n').encode())
        await asyncio.wait_for(writer.drain(),3)
        headers=await asyncio.wait_for(reader.readuntil(b'\r\n\r\n'),3)
        if len(headers)>8192 or not headers.startswith(b'HTTP/1.1 200 '):raise ValueError('backend denied')
        length=None;kind='text/html; charset=utf-8'
        for line in headers.decode('ascii').split('\r\n')[1:]:
            if line.lower().startswith('content-length:'):length=int(line.split(':',1)[1])
            if line.lower().startswith('content-type:'):kind=line.split(':',1)[1].strip()
        if length is None or not 0<=length<=262144:raise ValueError('response bound')
        body=await asyncio.wait_for(reader.readexactly(length),5)
        if request.path=='/play':
            csrf=await sync_to_async(get_token)(request)
            controls=('<nav><a href="/accounts/password/">Change password</a> '
                      '<form method="post" action="/accounts/logout/"><input type="hidden" name="csrfmiddlewaretoken" value="'+html.escape(csrf,quote=True)+'"><button>Sign out</button></form></nav>')
            body=body.replace(b'<main>',('<main>'+controls).encode()).replace(b'data-auth-mode="serve"',b'data-auth-mode="serve" data-account-auth="true"')
        return HttpResponse(body,content_type=kind)
    except Exception:
        return HttpResponse('The game is starting or unavailable. Please retry shortly.',status=503)
    finally:
        if writer:writer.close()

def account_css(request):
    return HttpResponse('body{font:18px system-ui;max-width:30rem;margin:3rem auto;padding:1rem;background:#111;color:#eee}input,button{font:inherit;padding:.5rem;margin:.4rem 0}label{display:block}a{color:#9cf}.errorlist{color:#faa}',content_type='text/css')

async def presence_view(request):
    user=await request.auser()
    if not user.is_authenticated:return JsonResponse({'signed_in':False},status=401)
    from .asgi import presence,write_status
    presence[user.pk]={'username':user.username,'seen':time.time()}
    write_status()
    return JsonResponse({'signed_in':True})
