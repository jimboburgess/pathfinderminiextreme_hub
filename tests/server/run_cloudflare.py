"""Opt-in Linux real-process/Unix/JWT/game tests, no cloudflared or public access.
Generated RSA keys exist only in test memory. All containers network=none.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'apps/cloudflare'))
from package_gateway import copy_python
from access_jwt import identity_login
from cloudflare_jwt_tests import ISSUER,jwk,token
from cryptography.hazmat.primitives.asymmetric import rsa
from run_serve_unix import Bridge
from run_party import View,tap

HOST='game.example.com'


def run(*args):return subprocess.check_output(args,text=True).strip()


class Client:
    def __init__(self,bridge,slot,jwt,**headers):
        self.bridge,self.slot,self.identifier=bridge,slot,0
        assert bridge.send(op='open',slot=slot,host=HOST,jwt=jwt,**headers)['ok']
    def call(self,kind,**fields):
        self.identifier+=1
        reply=self.bridge.send(op='rpc',slot=self.slot,request=dict(v=1,id=str(self.identifier),type=kind,**fields))
        assert 'response' in reply,reply
        time.sleep(.085);return reply['response']
    def close(self):self.bridge.send(op='close',slot=self.slot)


def main():
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--build',type=Path,required=True);a=p.parse_args()
    assert sys.platform=='linux' and os.getuid()==1000
    source,build=a.source.resolve(),a.build.resolve();nonce=uuid.uuid4().hex[:10]
    security=json.loads(run('docker','info','--format','{{json .SecurityOptions}}'));assert not any('rootless' in s or 'userns' in s for s in security)
    baseline=run('docker','ps','-q').splitlines();containers=[];bridges=[];image='pfme-cloudflare-test:'+nonce;built=False
    key=rsa.generate_private_key(public_exponent=65537,key_size=2048);next_key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
    with tempfile.TemporaryDirectory(prefix='pfme-cloudflare-',dir=Path.home()) as temp:
        work=Path(temp);root=work/'rootfs';root.mkdir();python=copy_python(root)
        shutil.copytree(source/'apps/cloudflare',root/'adapter',ignore=shutil.ignore_patterns('__pycache__'))
        (root/'app').mkdir();shutil.copy2(build/'apps/server/pfme_server',root/'app/pfme_server')
        for name in set(re.findall(r'(/[\w/+.,-]+)',run('ldd',str(root/'app/pfme_server')))):
            f=Path(name)
            if f.is_file():out=root/name.lstrip('/');out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f.resolve(),out)
        cache=(build/'CMakeCache.txt').read_text();m=re.search(r'^FETCHCONTENT_SOURCE_DIR_PFME_BOOST:[^=]*=(.*)$',cache,re.M);boost=Path(m[1]) if m and m[1] else build/'_deps/pfme_boost-src'
        (root/'test').mkdir();run('g++','-static','-O2','-std=c++17','-I'+str(boost),str(source/'tests/server/unix_proxy_fixture.cpp'),'-o',str(root/'test/proxy'))
        run('cc','-static','-O2',str(source/'ops/cloudflare/connector_gate.c'),'-o',str(root/'test/connector_gate'))
        nft=Path('/usr/sbin/nft');assert nft.is_file()
        for f in [nft,*[Path(n) for n in set(re.findall(r'(/[\w/+.,-]+)',run('ldd',str(nft)))) if Path(n).is_file()]]:
            out=root/str(f).lstrip('/');out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f.resolve(),out)
        shutil.copy2(source/'config/cloudflare/connector.namespace.nft',root/'test/connector.nft')
        (work/'Dockerfile').write_text('FROM scratch\nCOPY rootfs/ /\nENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 CRYPTOGRAPHY_OPENSSL_NO_LEGACY=1\n')
        run('docker','build','--network=none','--pull=false','-q','-t',image,str(work));built=True
        for name in ('front','back','config','keys','policy','gate'):(work/name).mkdir(mode=0o755)
        def mount(name,readonly=False):return ['--mount',f'type=bind,src={work/name},dst=/{name}'+(',readonly' if readonly else '')]
        limits=['--network=none','--read-only','--cap-drop=ALL','--security-opt=no-new-privileges','--pids-limit=32','--memory=256m','--cpus=1','--ulimit=nofile=128:128']
        # Root fixture can manage ONLY these task-owned synthetic test directories.
        def manage(files=None,cleanup=False):
            code='''import os,json,sys
d=json.load(sys.stdin)
if d['cleanup']:
 for path in ['/front','/back']:os.chown(path,1000,1000);os.chmod(path,0o700)
else:
 os.chown('/front',1001,1002);os.chmod('/front',0o710);os.chown('/back',999,981);os.chmod('/back',0o710)
 for path,value in d['files'].items():
  assert path in ['/config/gateway.json','/keys/jwks.json','/policy/admission.json','/gate/release']
  temporary=path+'.new';fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o644)
  with os.fdopen(fd,'w') as f:
   if path=='/gate/release':f.write(value)
   else:json.dump(value,f)
  os.replace(temporary,path)
'''
            cmd=['docker','run','--rm','-i',*limits,'--cap-add=CHOWN','--cap-add=FOWNER','--cap-add=DAC_OVERRIDE',*sum((mount(n) for n in ('front','back','config','keys','policy','gate')),[]),image,python,'-c',code]
            result=subprocess.run(cmd,input=json.dumps({'files':files or {},'cleanup':cleanup}),text=True,capture_output=True);assert result.returncode==0,result.stderr
        def claims(sub,seconds=600):return dict(iss=ISSUER,sub=sub,email=sub+'@example.com',iat=int(time.time())-1,nbf=int(time.time())-1,exp=int(time.time())+seconds)
        def proof(index,signer=key,kid='current',seconds=600):return token(signer,claims('fixture-'+str(index),seconds),dict(alg='RS256',kid=kid))
        users=[dict(login=identity_login(ISSUER,'fixture-'+str(i)),principal='fixture-principal-'+str(i)) for i in range(5)]
        conf=dict(version=1,issuer=ISSUER,audience='fixture-audience',origin='https://'+HOST,frontend='/front/gateway.sock',backend='/back/backend.sock',connector_uid=1002,frontend_gid=1002,keys='/keys/jwks.json')
        manage({'/config/gateway.json':conf,'/keys/jwks.json':dict(issuer=ISSUER,fetched_at=int(time.time()),keys=[jwk(key)]),'/policy/admission.json':dict(version=1,revision=1,users=users)})
        try:
            game=run('docker','run','-d','--name','pfme-cf-game-'+nonce,'--label','pfme.cf-test='+nonce,'--user','999:981',*limits,*mount('back'),*mount('policy',True),image,'/app/pfme_server','--runtime','party','--seed','2','--cloudflare-socket','/back/backend.sock','--cloudflare-origin','https://'+HOST,'--cloudflare-policy','/policy/admission.json','--cloudflare-peer-uid','1001','--cloudflare-peer-gid','981','--run-for-ms','240000');containers.append(game)
            gateway=run('docker','run','-d','--name','pfme-cf-gateway-'+nonce,'--label','pfme.cf-test='+nonce,'--user','1001:981','--group-add','1002',*limits,*mount('front'),*mount('back'),*mount('keys',True),*mount('config',True),image,python,'/adapter/gateway.py','--config','/config/gateway.json','--run-for-seconds','240');containers.append(gateway)
            for _ in range(100):
                if 'listening_unix' in run('docker','logs',gateway) and 'listening=unix' in run('docker','logs',game):break
                time.sleep(.1)
            else:raise AssertionError('isolated startup failed: '+run('docker','logs',gateway)+run('docker','logs',game))
            def bridge(uid,group,path,directory):
                name='pfme-cf-client-'+uuid.uuid4().hex[:12]
                b=Bridge(['docker','run','--rm','-i','--name',name,'--label','pfme.cf-test='+nonce,'--user',f'{uid}:{group}',*limits,*mount(directory,True),image,'/test/proxy',path,'/unused',HOST]);b.name=name;bridges.append(b);return b
            front=bridge(1002,1002,'/front/gateway.sock','front')
            # Install/check the deny-by-default template ONLY in our own fake
            # connector's otherwise network-disabled namespace, never the host.
            assert front.send(op='info')['uid']==1002
            target=json.loads(run('docker','inspect',front.name))[0]
            assert target['Config']['Labels']['pfme.cf-test']==nonce and target['HostConfig']['NetworkMode']=='none'
            nft_args=['docker','run','--rm','--network','container:'+front.name,'--read-only','--cap-drop=ALL','--cap-add=NET_ADMIN','--security-opt=no-new-privileges','--pids-limit=16','--memory=64m',image,'/usr/sbin/nft']
            run(*nft_args,'--check','--file','/test/connector.nft');run(*nft_args,'--file','/test/connector.nft')
            rules=run(*nft_args,'list','table','inet','pfme_cf_connector');assert 'policy drop' in rules and 'edges4' in rules
            gate_nonce=uuid.uuid4().hex+uuid.uuid4().hex
            gated=run('docker','run','-d','--label','pfme.cf-test='+nonce,'--user','1002:1002',*limits,*mount('gate',True),image,'/test/connector_gate','/gate/release',gate_nonce,'/app/pfme_server','--help');containers.append(gated)
            time.sleep(.25);assert json.loads(run('docker','inspect',gated))[0]['State']['Running'] and not run('docker','logs',gated)
            manage({'/gate/release':gate_nonce});assert run('docker','wait',gated)=='2'
            assert 'usage:' in subprocess.check_output(['docker','logs',gated],text=True,stderr=subprocess.STDOUT)
            wrong=bridge(1003,1002,'/front/gateway.sock','front');assert not wrong.send(op='open',slot=0,host=HOST,jwt=proof(0))['ok'];wrong.stop();bridges.remove(wrong)
            direct=bridge(1002,981,'/back/backend.sock','back');assert not direct.send(op='open',slot=0,host='localhost',verified_identity=users[0]['login'])['ok'];direct.stop();bridges.remove(direct)
            for fields in ({'email':'fixture-0@example.com'},{'login':'owner@example.com'},
                           {'jwt':proof(0),'origin':'https://wrong.invalid'}, {'jwt':proof(0),'host':'wrong.invalid'},
                           {'jwt':proof(0,seconds=-1)}, {'jwt':token(next_key,claims('fixture-0'))}):
                request=dict(op='open',slot=7,host=HOST);request.update(fields);assert not front.send(**request)['ok']
            page=front.send(op='get',slot=7,host=HOST,jwt=proof(0),verified_identity=users[4]['login'])
            assert page['ok'] and 'connect-src wss://'+HOST in page['headers'] and 'wss://localhost' not in page['headers'] and 'data-auth-mode="serve"' in page['body']
            clients=[];views=[]
            for i in range(4):
                c=Client(front,i,proof(i),verified_identity=users[4]['login']);clients.append(c);assert c.call('screen',since='0',offset=0)['error']['code']=='NotAuthenticated';assert c.call('authenticate')['ok'];assert c.call('join',name='Fixture')['ok'];assert c.call('play')['data']['audio'];views.append(View(c))
                for _ in range(5):tap(c,'a')
                assert any(views[-1].frame())
            forged_claims=claims('fixture-4');forged_claims['email']='fixture-0@example.com'
            fifth=Client(front,4,token(key,forged_claims),verified_identity=users[0]['login']);assert fifth.call('authenticate')['ok'];assert fifth.call('reconnect')['error']['code']=='NoCharacter';assert fifth.call('join',name='Fifth')['error']['code']=='Full';fifth.close()
            assert not front.send(op='open',slot=7,host=HOST,jwt=token(key,claims('unlisted')))['ok']
            saved=views[1].frame();clients[1].close();replacement=Client(front,1,proof(1));assert replacement.call('authenticate')['ok'];assert replacement.call('reconnect')['ok'];assert replacement.call('play')['ok'];clients[1]=replacement;views[1]=View(replacement);assert views[1].frame()==saved
            assert clients[0].call('input',action='encoder',down=False,player_id=1)['error']['code']=='InvalidRequest'
            for i,c in enumerate(clients):
                tap(c,'cw');tap(c,'cw');tap(c,'a')
                if i:tap(c,'a')
            for _ in range(11):tap(clients[3],'encoder')
            room1=views[3].frame();room0=views[0].frame();assert room1!=room0
            assert clients[0].call('ping')['ok'];assert clients[1].call('ping')['ok']
            manage({'/policy/admission.json':dict(version=1,revision=2,users=[u for i,u in enumerate(users) if i!=1])});time.sleep(1.2)
            assert front.send(op='rpc',slot=1,request=dict(v=1,id=str(clients[1].identifier+1),type='ping')).get('closed');assert clients[0].call('ping')['ok']
            short=Client(front,6,proof(4,seconds=2));assert short.call('authenticate')['ok'];time.sleep(2.2);assert front.send(op='rpc',slot=6,request=dict(v=1,id='2',type='ping')).get('closed')
            manage({'/keys/jwks.json':dict(issuer=ISSUER,fetched_at=int(time.time()),keys=[jwk(key),jwk(next_key,'next')])})
            rotated=Client(front,6,proof(4,next_key,'next'));assert rotated.call('authenticate')['ok'];rotated.close();assert clients[0].call('ping')['ok']
            manage({'/keys/jwks.json':dict(issuer=ISSUER,fetched_at=int(time.time()),keys=[jwk(next_key,'next')])});time.sleep(.8)
            assert front.send(op='rpc',slot=0,request=dict(v=1,id=str(clients[0].identifier+1),type='ping')).get('closed')
            recovered=Client(front,7,proof(0,next_key,'next'));assert recovered.call('authenticate')['ok'] and recovered.call('reconnect')['ok'];recovered.close()
            for identity in (game,gateway):
                info=json.loads(run('docker','inspect',identity))[0];h=info['HostConfig'];assert h['NetworkMode']=='none' and h['ReadonlyRootfs'] and h['CapDrop']==['ALL'] and not h.get('CapAdd') and not info['NetworkSettings'].get('Ports')
            logs=run('docker','logs',gateway);assert 'eyJ' not in logs and 'fixture-0@example.com' not in logs and 'Cf-Access' not in logs
            report=dict(connector_namespace_filter_checked=True,connector_preexec_gate_verified=True,forged_private_header_and_email_no_takeover=True,unlisted_signed_identity_denied=True,real_unix_and_rsa=True,wrong_connector_uid_denied=True,direct_backend_uid_denied=True,email_header_not_identity=True,exact_origin_csp=True,four_production_characters=True,fifth_slot_rejected=True,reconnect_retains_frame=True,independent_original_rooms=True,audio_commands=True,individual_revocation=True,jwt_expiry_closes_websocket=True,key_rotation_current_previous=True,removed_key_closes_websocket=True,stable_owner_after_rotation=True,secret_free_logs=True,cloudflared_installed_or_started=False,public_access=False)
        finally:
            for b in bridges:b.stop()
            for identity in containers:
                info=json.loads(run('docker','inspect',identity))[0];assert info['Config']['Labels']['pfme.cf-test']==nonce;run('docker','stop','--timeout','4',identity);run('docker','rm',identity)
            manage(cleanup=True)
            if built:run('docker','image','rm',image)
        assert all(json.loads(run('docker','inspect',c))[0]['State']['Running'] for c in baseline)
        report['existing_containers_unchanged']=True;print(json.dumps(report,sort_keys=True))


if __name__=='__main__':main()
