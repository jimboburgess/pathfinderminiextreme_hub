import os,asyncio,json,time,contextlib
from websockets.exceptions import ConnectionClosed
from pathlib import Path
os.environ.setdefault('DJANGO_SETTINGS_MODULE','pfme_accounts.settings')
from django.core.asgi import get_asgi_application
http_app=get_asgi_application()
from django.conf import settings
from django.http import HttpRequest
from django.http.cookie import parse_cookie
from django.contrib.sessions.middleware import SessionMiddleware
from django.contrib.auth import get_user
from asgiref.sync import sync_to_async
from websockets.asyncio.client import unix_connect
from .models import AccountProfile

connections={}
presence={}
backend_alive=False
password_budget=asyncio.Semaphore(2)
next_status_at=0.0
def authenticated(cookie):
    request=HttpRequest();request.COOKIES=parse_cookie(cookie)
    SessionMiddleware(lambda r:None).process_request(request)
    user=get_user(request)  # Fresh database session + active account + password hash.
    if not user.is_authenticated:return None
    try:profile=AccountProfile.objects.get(user=user)
    except AccountProfile.DoesNotExist:return None
    return user.pk,user.username,profile.login

def write_status():
    global next_status_at
    now=time.monotonic()
    if now<next_status_at:return
    next_status_at=now+.5
    players=[];seen=set()
    for c in connections.values():
        if c.get('player_id') is not None and c['account_id'] not in seen:
            players.append({'username':c['username'],'player_id':c['player_id'],'character_name':c.get('name'),
                            'character_class':None,'level':None,'location':None});seen.add(c['account_id'])
    data={'updated_at':time.time(),'backend_online':backend_alive,'active_players':len(players),'capacity':20,'party_capacity':4,
          'connected_accounts':len({c['account_id'] for c in connections.values()} | {uid for uid,c in presence.items() if time.time()-c['seen']<60}),
          'players':players,'waiting_usernames':sorted({c['username'] for uid,c in presence.items() if time.time()-c['seen']<60 and uid not in seen})}
    try:
        p=Path(os.environ.get('PFME_STATUS_FILE','/status/current.json'))
        temp=p.with_suffix('.new');temp.write_text(json.dumps(data));temp.chmod(0o640);temp.replace(p)
    except OSError:pass

async def websocket(scope,receive,send):
    headers={}
    for key,value in scope['headers']:
        key=key.decode('ascii').lower()
        if key in headers:return await send({'type':'websocket.close','code':1008})
        headers[key]=value.decode('latin1')
    if scope['path']!='/' or headers.get('origin')!=settings.ORIGIN or headers.get('host')!=settings.ALLOWED_HOSTS[0]:
        return await send({'type':'websocket.close','code':1008})
    cookie=headers.get('cookie','');user=await sync_to_async(authenticated)(cookie)
    if not user:return await send({'type':'websocket.close','code':4401})
    if (await receive())['type']!='websocket.connect':return
    distinct={c['account_id'] for c in connections.values()}
    if len(connections)>=48 or user[0] not in distinct and len(distinct)>=20:
        await send({'type':'websocket.accept'})
        return await send({'type':'websocket.close','code':4409,'reason':'Server is full.'})
    # A verified new cookie session replaces the same account's old transport,
    # never its character. Await only bounded cleanup before opening the backend.
    previous=[c for c in connections.values() if c['account_id']==user[0]]
    for old in previous:old['reason']='replaced_by_new_connection';old['code']=4001;old['stop'].set()
    for old in previous:
        with contextlib.suppress(TimeoutError):await asyncio.wait_for(old['done'].wait(),3)
    key=object();tasks=[];opened_at=time.monotonic();accepted=False
    current={'account_id':user[0],'username':user[1],'player_id':None,'stop':asyncio.Event(),'done':asyncio.Event(),'reason':'transport_closed','code':1013}
    connections[key]=current;write_status()
    def record(event):print(json.dumps({'event':event,'account_id':user[0],'reason':current['reason'],'close_code':current['code'],'duration_ms':int((time.monotonic()-opened_at)*1000)}),flush=True)
    try:
        async with unix_connect(os.environ.get('PFME_BACKEND','/back/backend.sock'),uri='ws://localhost/',origin=settings.ORIGIN,
          additional_headers={'Pfme-Verified-Identity':user[2]},compression=None,max_size=32768,max_queue=8,open_timeout=3,close_timeout=2,ping_interval=None,ping_timeout=None) as backend:
            await send({'type':'websocket.accept'});accepted=True;record('CONNECTED')
            async def upstream():
                try:
                    async for message in backend:
                        if not isinstance(message,str):raise ValueError('Unexpected binary response')
                        reply=json.loads(message)
                        if reply.get('ok'):
                            data=reply.get('data',{});player=data.get('player')
                            if isinstance(player,dict) and reply.get('type') in ('join','reconnect'):
                                current.update(player_id=player.get('player_id'),name=player.get('name'));write_status()
                            if reply.get('type')=='leave':current['player_id']=None;write_status()
                        await send({'type':'websocket.send','text':message})
                except ConnectionClosed as error:
                    if current['stop'].is_set():return
                    code=error.rcvd.code if error.rcvd else 1013
                    current['code']=code if code in (1000,1001,1008,1011,1012,1013,4001,4003) else 1013
                    current['reason']='backend_closed' if code not in (4001,4003) else ('replaced_by_new_connection' if code==4001 else 'authorization_expired_or_revoked')
                else:
                    if current['stop'].is_set():return
                    code=backend.close_code or 1013;current['code']=code if code in (1000,1001,1008,1011,1012,1013,4001,4003) else 1013
                    current['reason']='backend_closed'
            async def downstream():
                while True:
                    event=await receive()
                    if event['type']=='websocket.disconnect':
                        current['reason']='browser_close';current['code']=event.get('code',1000)
                        await backend.close(code=1000 if current['code'] not in (1000,1001,4000) else current['code']);return
                    text=event.get('text')
                    if not isinstance(text,str) or len(text.encode())>4096:raise ValueError('Request bound')
                    await backend.send(text)
            async def lease():
                while True:
                    await asyncio.sleep(1)
                    latest=await sync_to_async(authenticated)(cookie)
                    if not latest or latest[0]!=user[0]:current['reason']='account_session_expired_or_revoked';current['code']=4401;return
                    write_status()
            tasks=[asyncio.create_task(upstream()),asyncio.create_task(downstream()),asyncio.create_task(lease()),asyncio.create_task(current['stop'].wait())]
            done,_=await asyncio.wait(tasks,return_when=asyncio.FIRST_COMPLETED)
            for task in done:
                if not task.cancelled() and task.exception():current['reason']='application_exception';current['code']=1011
    except Exception:
        current['reason']='backend_unavailable' if not accepted else 'transport_error';current['code']=1013
    finally:
        for task in tasks:task.cancel()
        if tasks:await asyncio.gather(*tasks,return_exceptions=True)
        connections.pop(key,None);write_status();record('DISCONNECTED')
        if accepted:
            with contextlib.suppress(Exception):await send({'type':'websocket.close','code':current['code'] if current['code']!=1006 else 1013,'reason':current['reason']})
        else:
            with contextlib.suppress(Exception):await send({'type':'websocket.close','code':1013})
        current['done'].set()

async def application(scope,receive,send):
    if scope['type'] in ('http','websocket'):
        proto=[v for k,v in scope['headers'] if k.lower()==b'x-forwarded-proto']
        # Cloudflare overwrites X-Forwarded-Proto; only the protected connector
        # socket reaches this app. No identity is accepted from forwarding headers.
        if proto != [b'https']:
            if scope['type']=='websocket':return await send({'type':'websocket.close','code':1008})
            status=301 if proto==[b'http'] else 400
            headers=[(b'location',(settings.ORIGIN+'/').encode())] if status==301 else []
            await send({'type':'http.response.start','status':status,'headers':headers})
            return await send({'type':'http.response.body','body':b'HTTPS is required.'})
    if scope['type']=='lifespan':
        async def heartbeat():
            global backend_alive
            while True:
                try:
                    _,w=await asyncio.wait_for(asyncio.open_unix_connection(os.environ.get('PFME_BACKEND','/back/backend.sock')),2)
                    w.close();backend_alive=True
                except Exception:backend_alive=False
                write_status();await asyncio.sleep(5)
        task=None
        try:
            while True:
                event=await receive()
                if event['type']=='lifespan.startup':
                    task=asyncio.create_task(heartbeat());await send({'type':'lifespan.startup.complete'})
                elif event['type']=='lifespan.shutdown':return await send({'type':'lifespan.shutdown.complete'})
        finally:
            if task:task.cancel();await asyncio.gather(task,return_exceptions=True)
    elif scope['type']=='websocket':await websocket(scope,receive,send)
    else:
        # This app is Unix-only behind the configured TLS ingress. Ignore
        # browser-supplied forwarding/identity headers; origin is fixed configuration.
        scope=dict(scope,scheme='https')
        if scope.get('method')=='POST':
            body=bytearray()
            while True:
                try:event=await asyncio.wait_for(receive(),3)
                except TimeoutError:
                    await send({'type':'http.response.start','status':408,'headers':[]})
                    return await send({'type':'http.response.body','body':b'Request timed out.'})
                if event['type']=='http.disconnect':return
                body.extend(event.get('body',b''))
                if len(body)>8192:
                    await send({'type':'http.response.start','status':413,'headers':[]})
                    return await send({'type':'http.response.body','body':b'Request too large.'})
                if not event.get('more_body'):break
            original_receive=receive;delivered=False
            async def receive_body():
                nonlocal delivered
                if not delivered:
                    delivered=True
                    return {'type':'http.request','body':bytes(body),'more_body':False}
                return await original_receive()
            receive=receive_body
        if scope.get('method')=='POST' and scope['path'] in ('/accounts/login/','/accounts/password/','/accounts/register/'):
            try:await asyncio.wait_for(password_budget.acquire(),2)
            except TimeoutError:
                await send({'type':'http.response.start','status':503,'headers':[(b'content-type',b'text/plain')]})
                return await send({'type':'http.response.body','body':b'Login busy; please retry shortly.'})
            try:await http_app(scope,receive,send)
            finally:password_budget.release()
        else:await http_app(scope,receive,send)
