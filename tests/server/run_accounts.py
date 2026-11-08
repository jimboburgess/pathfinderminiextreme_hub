"""Real account HTTP/WS -> protected C++ game. Disposable volumes, network=none."""
import subprocess,uuid,json,secrets,time,sys,hashlib,os,argparse
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent))
from run_party import View,tap,form_town_party
parser=argparse.ArgumentParser();parser.add_argument('--image',required=True);parser.add_argument('--game-image',required=True);parser.add_argument('--game-duration-ms',type=int,default=300000)
parser.add_argument('--character-saves',action='store_true')
options=parser.parse_args();assert sys.platform=='linux';assert 0<=options.game_duration_ms<=86400000
IMAGE=options.image;GAME=options.game_image
nonce=uuid.uuid4().hex[:10];prefix='pfme-accounts-test-'+nonce;containers=[];volumes={};bridge=None;game=None
baseline=subprocess.check_output(['docker','ps','-q'],text=True).splitlines()
def run(args,input=None):
    p=subprocess.run(args,input=input,text=True,capture_output=True)
    if p.returncode:raise RuntimeError(p.stdout+p.stderr)
    return p.stdout.strip()
limits=['--network=none','--read-only','--cap-drop=ALL','--security-opt=no-new-privileges','--memory=512m','--cpus=1','--pids-limit=64','--ulimit=nofile=128:128','--label','pfme.accounts.test='+nonce]
def mount(name,ro=False):return ['--mount','type=volume,src='+volumes[name]+',dst=/'+name+(',readonly' if ro else '')]
def oneshot(args,user='995:980',names=('state','policy'),caps=(),input=None):
    return run(['docker','run','--rm','-i',*limits,'--user',user,*sum((['--cap-add='+c] for c in caps),[]),*sum((mount(n) for n in names),[]),IMAGE,*args],input)
def start(image,args,user,names,extra=()):
    cid=run(['docker','run','-d',*limits,'--user',user,*extra,*sum((mount(n,ro) for n,ro in names),[]),image,*args]);containers.append(cid);return cid
def b(**q):
    bridge.stdin.write(json.dumps(q)+'\n');bridge.stdin.flush()
    line=bridge.stdout.readline()
    if not line:raise RuntimeError('Fixture client exited')
    result=json.loads(line)
    return result
class Client:
    def __init__(self,slot):self.slot=slot
    def call(self,kind,**fields):
        r=b(op='rpc',slot=self.slot,type=kind,fields=fields)
        assert 'result' in r,(kind,r)
        reply=r['result']
        if kind=='screen' and reply.get('ok'):self.text_input=reply['data']['text_input']
        return reply
def status(cid):
    return json.loads(run(['docker','exec',cid,'python','-c',"from pathlib import Path;print(Path('/status/current.json').read_text())"]))
report={}
try:
    for n in ('state','policy','front','back','status','gate','status_secret')+(('characters',) if options.character_saves else ()):
        v=prefix+'-'+n;run(['docker','volume','create','--label','pfme.accounts.test='+nonce,v]);volumes[n]=v
    oneshot(['-m','pfme_accounts.init'],user='0:0',names=tuple(volumes),caps=('CHOWN','FOWNER','DAC_OVERRIDE'))
    if options.character_saves:
        oneshot(['-c',"import os;os.chown('/characters',999,980);os.chmod('/characters',0o700)"],user='0:0',names=('characters',),caps=('CHOWN','FOWNER'))
    oneshot(['manage.py','migrate','--noinput'])
    fixture=[{'username':'friend'+str(i),'password':secrets.token_urlsafe(24)} for i in range(10)]
    code="""import os,json,sys;os.environ['DJANGO_SETTINGS_MODULE']='pfme_accounts.settings'
import django;django.setup()
from pfme_accounts.invitations import issue
result=[]
for f in json.load(sys.stdin):
 invitation,token=issue(f['username']);result.append(token)
print(json.dumps(result))
"""
    invitation_tokens=json.loads(oneshot(['-c',code],input=json.dumps(fixture)))
    oneshot(['manage.py','sync_admission'],user='0:980')
    game=start(GAME,['--runtime','party','--seed','2','--cloudflare-socket','/back/backend.sock','--cloudflare-origin','https://game.noburdenmusics.com','--cloudflare-policy','/policy/admission.json','--cloudflare-peer-uid','995','--cloudflare-peer-gid','980','--run-for-ms',str(options.game_duration_ms)],'999:980',[('back',False),('policy',True)]+([('characters',False)] if options.character_saves else []),['--env=PFME_SAVE_DIR=/characters','--env=PFME_REQUIRE_ACCOUNT_SAVES=1'] if options.character_saves else [])
    app=start(IMAGE,['-m','pfme_accounts.server'],'995:980',[('state',False),('policy',True),('front',False),('back',True),('status',False)],['--group-add=979'])
    time.sleep(3)
    bridge=subprocess.Popen(['docker','run','--rm','-i',*limits,'--user=994:979',*mount('front',True),'--mount','type=bind,src='+str(Path(__file__).with_name('accounts_client.py'))+',dst=/test_client.py,readonly',IMAGE,'/test_client.py'],text=True,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    cookies=[]
    race=b(op='register_race',token=invitation_tokens[0],**fixture[0])['result']
    assert sorted(race['statuses'])==[302,400],race
    report['concurrent_claim_consumes_invitation_once']=True
    for i,f in enumerate(fixture[1:],start=1):
        response=b(op='register',token=invitation_tokens[i],**f)
        assert response.get('result',{}).get('status')==302,'Invitation registration failed'
    assert b(op='register',token=invitation_tokens[0],username='takeover',password=secrets.token_urlsafe(24))['result']['status']==400
    report['ten_single_use_invitations_redeemed_over_real_http']=True
    for f in fixture:
        response=b(op='login',**f);assert 'result' in response,response;result=response['result'];assert result['status']==302,result
        cookies.append('; '.join(k+'='+v for k,v in result['cookies'].items()))
    for cookie in cookies:assert b(op='http',path='/accounts/presence/',cookie=cookie)['result']['status']==200
    report['ten_independent_password_logins']=True
    assert b(op='open',slot=12).get('failed');assert b(op='open',slot=12,cookie=cookies[0],origin='https://evil.invalid').get('failed')
    assert b(op='http',path='/accounts/login/',form={'username':'friend0','password':secrets.token_urlsafe(8)})['result']['status']==403
    assert b(op='http',path='/status',cookie=cookies[0])['result']['status']==404
    assert b(op='http',path='/accounts/login/',proto='http')['result']['status']==301
    assert b(op='http',path='/accounts/login/',form={'oversized':'x'*9000})['result']['status']==413
    assert b(op='open',slot=13,headers={'Pfme-Verified-Identity':'forged','Cf-Access-Jwt-Assertion':'forged'}).get('failed')
    page=b(op='http',path='/play',cookie=cookies[0])['result'];assert page['status']==200 and 'data-account-auth="true"' in page['body']
    js=b(op='http',path='/play.js',cookie=cookies[0])['result'];assert 'Server is full.' in js['body'] and 'reconnecting' in js['body']
    players=[];views=[];ids=[]
    for i in range(4):
        assert b(op='open',slot=i,cookie=cookies[i],headers={'Pfme-Verified-Identity':'forged'})['result']['opened']
        c=Client(i);assert c.call('screen',since='0',offset=0)['error']['code']=='NotAuthenticated'
        assert c.call('authenticate')['ok'];joined=c.call('join',name='Friend'+str(i));assert joined['ok'],joined;ids.append(joined['data']['player']['player_id'])
        assert c.call('play')['data']['audio'];players.append(c);views.append(View(c));assert any(views[-1].frame())
        for _ in range(5):tap(c,'a')
        views[-1].frame();assert c.text_input, 'Original character name entry required'
        for letter in 'Hero'+str(i):assert c.call('input',action='key:'+letter,down=True)['ok']
        tap(c,'encoder')
        views[-1].frame();assert not c.text_input, 'Finish actual character creation before Town/save checks'
        if options.character_saves:
            # Production Home -> Save Game -> Slot 1, with independent account
            # namespaces established only by the real account gateway.
            tap(c,'a');tap(c,'cw');tap(c,'a');tap(c,'a');tap(c,'b')
        assert status(app)['active_players']==i+1
    assert len(set(ids))==4
    if options.character_saves:
        code="from pathlib import Path;p=Path('/characters');a=list(p.iterdir());assert len(a)==4;assert all(x.name.startswith('account-') and sorted(y.name for y in x.iterdir())==['save_slot_1.dat'] for x in a);print('Four separately owned account saves verified')"
        oneshot(['-c',code],user='0:0',names=('characters',),caps=('DAC_OVERRIDE',))
        report['four_authenticated_accounts_separate_durable_character_files']=True
    for i in range(4,10):
        login=b(op='login',**fixture[i])['result'];assert login['status']==302
        cookies[i]='; '.join(k+'='+v for k,v in login['cookies'].items())
        assert b(op='http',path='/accounts/presence/',cookie=cookies[i])['result']['status']==200
        assert b(op='open',slot=i,cookie=cookies[i])['result']['opened'];c=Client(i);assert c.call('authenticate')['ok']
        assert c.call('join',name='Waiting')['ok'];assert c.call('play')['ok'];assert c.call('party_status')['error']['code']=='NotInParty';assert c.call('leave')['ok'];assert b(op='close',slot=i)['result']['closed']
    report['four_original_characters_and_six_waiting_accounts']=True
    assert players[0].call('input',action='a',down=True,player_id=ids[1])['error']['code']=='InvalidRequest'
    form_town_party(players)
    tap(players[0],'a')
    for _ in range(11):tap(players[3],'encoder')
    a=views[0].frame();d=views[3].frame();assert a!=d
    report['separate_rooms_original_rgb565_frames']=[hashlib.sha256(a).hexdigest(),hashlib.sha256(d).hexdigest()]
    b(op='close',slot=1);assert b(op='open',slot=1,cookie=cookies[1])['result']['opened'];c=Client(1)
    assert c.call('authenticate')['ok'];res=c.call('reconnect');assert res['ok'] and res['data']['player']['player_id']==ids[1];assert c.call('play')['ok'];players[1]=c
    assert players[3].call('leave')['ok'];b(op='close',slot=3)
    assert b(op='open',slot=4,cookie=cookies[4])['result']['opened'];waiting=Client(4);assert waiting.call('authenticate')['ok'];assert waiting.call('join',name='Waiting')['ok'];assert waiting.call('play')['ok'];assert any(View(waiting).frame())
    report['waiting_account_joins_after_safe_leave']=True
    # Normal /play tabs refresh account presence every 15s even while Party Full.
    # Rotary join-code entry lengthens this fixture; renew the waiting tabs too.
    for cookie in cookies:assert b(op='http',path='/accounts/presence/',cookie=cookie)['result']['status']==200
    time.sleep(1.2);s=status(app);assert s['active_players']==4,s
    assert s['connected_accounts']==10 and len(s['waiting_usernames'])==6,s
    report.update(private_status_active_players=4,private_status_signed_in_accounts=10,private_status_waiting_accounts=6)
    secret_code="import os,secrets;from pathlib import Path;p=Path('/status_secret');os.chown(p,0,980);p.chmod(0o750);f=p/'status-token';f.write_text(secrets.token_urlsafe(48));os.chown(f,0,980);f.chmod(0o440)"
    oneshot(['-c',secret_code],user='0:0',names=('status_secret',),caps=('CHOWN',))
    monitor=start(IMAGE,['-m','pfme_accounts.status_server'],'995:980',[('status',True)],['--mount','type=volume,src='+volumes['status_secret']+',dst=/run/secrets,readonly'])
    time.sleep(.5)
    check_http="import http.client,json;from pathlib import Path;h=http.client.HTTPConnection('127.0.0.1',8080,timeout=3);h.request('GET','/status');r=h.getresponse();assert r.status==403;r.read();h.close();h=http.client.HTTPConnection('127.0.0.1',8080,timeout=3);h.request('GET','/status',headers={'Authorization':'Bearer '+Path('/run/secrets/status-token').read_text()});r=h.getresponse();d=json.loads(r.read());assert r.status==200 and d['active_players']==4 and d['connected_accounts']==10;print('Private status bearer enforcement and exact counts passed')"
    run(['docker','exec',monitor,'python','-c',check_http]);report['private_status_http_bearer_enforced']=True
    code="import os;os.environ['DJANGO_SETTINGS_MODULE']='pfme_accounts.settings';import django;django.setup();from django.contrib.auth.models import User;User.objects.filter(username='friend0').update(is_active=False)"
    oneshot(['-c',code]);oneshot(['manage.py','sync_admission'],user='0:980');time.sleep(1.6)
    assert b(op='rpc',slot=0,type='ping').get('failed');assert players[1].call('ping')['ok'];assert waiting.call('ping')['ok']
    assert b(op='open',slot=13,cookie=cookies[0]).get('failed')
    report.update(individual_disable_revokes_live_socket=True,other_players_continue=True,refresh_reconnect_same_character=True,forged_identity_does_not_take_over=True,csrf_and_origin_enforced=True,public_monitoring_denied=True,original_audio_payloads=True)
finally:
    if bridge:
        bridge.stdin.close()
        try:bridge.wait(timeout=8)
        except subprocess.TimeoutExpired:bridge.terminate();bridge.wait(timeout=5)
    for cid in reversed(containers):
        info=json.loads(run(['docker','inspect',cid]))[0];assert info['Config']['Labels']['pfme.accounts.test']==nonce
        run(['docker','stop','--timeout','3',cid])
        if cid==game and options.game_duration_ms==0:
            ended=json.loads(run(['docker','inspect',cid]))[0];assert ended['State']['ExitCode']==0,'Untimed game failed graceful stop'
        run(['docker','rm',cid])
    # Client --rm closes on EOF; remove ONLY our labeled leftover on abnormal exit.
    leftovers=run(['docker','ps','-aq','--filter','label=pfme.accounts.test='+nonce]).splitlines()
    for cid in leftovers:
        run(['docker','stop','--timeout','2',cid]);run(['docker','rm',cid])
    for v in volumes.values():
        info=json.loads(run(['docker','volume','inspect',v]))[0];assert info['Labels']['pfme.accounts.test']==nonce;run(['docker','volume','rm',v])
    assert all(json.loads(run(['docker','inspect',cid]))[0]['State']['Running'] for cid in baseline)
report['existing_containers_running']=True
report['game_duration_ms']=options.game_duration_ms
print(json.dumps(report,sort_keys=True))
