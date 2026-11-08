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
report={'capacity_cases':[]}
try:
    for n in ('state','policy','front','back','status','gate','status_secret','characters'):
        v=prefix+'-'+n;run(['docker','volume','create','--label','pfme.accounts.test='+nonce,v]);volumes[n]=v
    oneshot(['-m','pfme_accounts.init'],user='0:0',names=tuple(volumes),caps=('CHOWN','FOWNER','DAC_OVERRIDE'))
    oneshot(['-c',"import os;os.chown('/characters',999,980);os.chmod('/characters',0o700)"],user='0:0',names=('characters',),caps=('CHOWN','FOWNER'))
    oneshot(['manage.py','migrate','--noinput'])
    fixture=[{'username':'load'+str(i),'password':secrets.token_urlsafe(24)} for i in range(21)]
    code="""import os,json,sys;os.environ['DJANGO_SETTINGS_MODULE']='pfme_accounts.settings'
import django;django.setup()
from django.contrib.auth.models import User
from pfme_accounts.models import AccountProfile
for f in json.load(sys.stdin):
 u=User.objects.create_user(username=f['username'],password=f['password']);AccountProfile.objects.get_or_create(user=u)
print('Individual fixture accounts created')
"""
    oneshot(['-c',code],input=json.dumps(fixture));oneshot(['manage.py','sync_admission'],user='0:980')
    game=start(GAME,['--runtime','party','--seed','2','--cloudflare-socket','/back/backend.sock','--cloudflare-origin','https://game.noburdenmusics.com','--cloudflare-policy','/policy/admission.json','--cloudflare-peer-uid','995','--cloudflare-peer-gid','980','--run-for-ms','0'],'999:980',[('back',False),('policy',True),('characters',False)],['--env=PFME_SAVE_DIR=/characters','--env=PFME_REQUIRE_ACCOUNT_SAVES=1'])
    app=start(IMAGE,['-m','pfme_accounts.server'],'995:980',[('state',False),('policy',True),('front',False),('back',True),('status',False)],['--group-add=979'])
    time.sleep(2)
    bridge=subprocess.Popen(['docker','run','--rm','-i',*limits,'--user=994:979',*mount('front',True),'--mount','type=bind,src='+str(Path(__file__).with_name('accounts_client.py'))+',dst=/test_client.py,readonly',IMAGE,'/test_client.py'],text=True,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    cookies=[];clients=[];ids=[]
    for f in fixture:
        login=b(op='login',**f)['result'];assert login['status']==302
        cookies.append('; '.join(k+'='+v for k,v in login['cookies'].items()))
    for number in (1,4,8,12,16,20):
        for i in range(len(clients),number):
            assert b(op='open',slot=i,cookie=cookies[i])['result']['opened'];c=Client(i)
            assert c.call('authenticate')['ok'];joined=c.call('join',name='Load'+str(i));assert joined['ok'],joined
            ids.append(joined['data']['player']['player_id']);assert c.call('play')['ok'];clients.append(c)
            own=c.call('players');assert own['ok'] and len(own['data']['players'])==1,'No cross-party account listing'
        measurement=b(op='measure',slots=list(range(number)),seconds=5)['result'];assert measurement['clients']==number;measurement['docker_stats']=run(['docker','stats','--no-stream','--format','{{.CPUPerc}}|{{.MemUsage}}',game]);report['capacity_cases'].append(measurement)
    assert len(set(ids))==20
    rejected=b(op='open',slot=20,cookie=cookies[20])
    if not rejected.get('failed'):
        time.sleep(.3);assert b(op='close_info',slot=20)['result']['close_code']==4409,'21st receives explicit Server Full close'
        assert b(op='rpc',slot=20,type='ping').get('failed')
    for i in [0,3,7,11,19]:
        assert b(op='close',slot=i)['result']['closed'];assert b(op='open',slot=i,cookie=cookies[i])['result']['opened']
        c=Client(i);assert c.call('authenticate')['ok'];res=c.call('reconnect');assert res['ok'] and res['data']['player']['player_id']==ids[i];assert c.call('play')['ok'];clients[i]=c
    report['seeded_reserved_account_reconnects']=5
    assert b(op='open',slot=21,cookie=cookies[0])['result']['opened'];replacement=Client(21);assert replacement.call('authenticate')['ok'];res=replacement.call('reconnect');assert res['ok'] and res['data']['player']['player_id']==ids[0];assert replacement.call('play')['ok'];time.sleep(.3)
    assert b(op='close_info',slot=0)['result']['close_code']==4001,'Old tab receives a specific replacement code'
    report['duplicate_replaces_old_transport_without_new_character']=True
    assert clients[19].call('leave')['ok'];b(op='close',slot=19);time.sleep(.3)
    assert b(op='open',slot=20,cookie=cookies[20])['result']['opened'];newcomer=Client(20);assert newcomer.call('authenticate')['ok'];assert newcomer.call('join',name='NewAccount')['ok'];assert newcomer.call('play')['ok']
    report.update(clean_21st_rejection=True,permanent_leave_reclaims_capacity=True,authenticated_accounts=20,party_limit=4)
    report['after_reconnect']=b(op='measure',slots=[21,*range(1,19),20],seconds=5)['result']
    # Unauthorized direct/forged identity and origin requests still fail.
    assert b(op='open',slot=30,headers={'Pfme-Verified-Identity':'forged'}).get('failed');assert b(op='open',slot=30,cookie=cookies[1],origin='https://evil.invalid').get('failed')
    report['forged_identity_and_origin_rejected']=True
    # A genuine runtime kill must not turn unless-stopped into a stale-socket
    # restart loop. Only this labeled disposable game is killed/restarted.
    before=json.loads(run(['docker','inspect',game]))[0]
    assert before['Config']['Labels']['pfme.accounts.test']==nonce
    run(['docker','exec',game,'/app/pfme_server','--healthcheck','/back/backend.sock.health'])
    duplicate=subprocess.run(['docker','run','--rm',*limits,'--user=999:980',*mount('back'),*mount('policy',True),*mount('characters'),'--env=PFME_SAVE_DIR=/characters','--env=PFME_REQUIRE_ACCOUNT_SAVES=1',GAME,*before['Config']['Cmd']],capture_output=True,text=True)
    assert duplicate.returncode==1,'A second runtime must not retire a live socket'
    run(['docker','exec',game,'/app/pfme_server','--healthcheck','/back/backend.sock.health'])
    run(['docker','kill','--signal=KILL',game]);run(['docker','start',game]);time.sleep(1)
    run(['docker','exec',game,'/app/pfme_server','--healthcheck','/back/backend.sock.health'])
    report['exclusive_runtime_lock_and_crash_stale_socket_recovery']=True

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
print(json.dumps(report,sort_keys=True),flush=True)
