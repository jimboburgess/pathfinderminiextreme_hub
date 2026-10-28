"""Actual Linux peer-credential/identity/isolation tests, with no Serve activation.
Uses only task-owned temporary scratch images/containers. No pulls or packages.
The root test proxy is a fixture, NOT evidence of live Tailscale authentication.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import uuid
import sys
sys.dont_write_bytecode=True
from run_party import View,tap

HOST='stage1.test.ts.net'

def run(*args,**kwargs):
    return subprocess.run(args,check=True,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,**kwargs).stdout.strip()

class Bridge:
    def __init__(self,args):
        self.process=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    def send(self,**value):
        self.process.stdin.write(json.dumps(value)+'\n');self.process.stdin.flush()
        line=self.process.stdout.readline();assert line, self.process.stderr.read()
        result=json.loads(line);assert not result.get('fixture_error'),value.get('op');return result
    def stop(self):
        if self.process.poll() is None:
            self.process.stdin.close();self.process.wait(timeout=8)
        assert self.process.returncode==0,self.process.stderr.read()

class Client:
    def __init__(self,bridge,slot,login):
        self.bridge=bridge;self.slot=slot;self.identifier=0
        opened=bridge.send(op='open',slot=slot,login=login);assert opened.get('ok'),opened
        assert opened['hello']['data']['authentication']=='serve'
    def call(self,kind,**fields):
        self.identifier+=1
        result=self.bridge.send(op='rpc',slot=self.slot,request=dict(v=1,id=str(self.identifier),type=kind,**fields))
        assert 'response' in result,result;reply=result['response'];assert reply['id']==str(self.identifier)
        time.sleep(.085);return reply
    def close(self):self.bridge.send(op='close',slot=self.slot)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--build',type=Path,required=True)
    args=parser.parse_args();source=args.source.resolve();build=args.build.resolve()
    assert sys.platform.startswith('linux') and os.getuid()==1000, 'Reviewed fixture runtime uses UID 1000'
    security=json.loads(run('docker','info','--format','{{json .SecurityOptions}}'))
    assert not any('userns' in x or 'rootless' in x for x in security),'Peer UID mapping requires explicit review'
    assert any('seccomp' in x for x in security) and any('apparmor' in x for x in security)
    baseline=run('docker','ps','--quiet').splitlines();nonce=uuid.uuid4().hex[:12]
    runtime='pfme-stage1-runtime:'+nonce;verification='pfme-stage1-verification:'+nonce;proxy='pfme-stage1-proxy:'+nonce
    images=[];containers=[];bridge=None;report={}
    with tempfile.TemporaryDirectory(prefix='pfme-stage1-',dir=Path.home()) as work_name:
        work=Path(work_name);rootfs=work/'rootfs';(rootfs/'app').mkdir(parents=True)
        executable=build/'apps/server/pfme_server';expiry=build/'apps/server/pfme_server_expiry_fixture'
        shutil.copy2(executable,rootfs/'app/pfme_server');shutil.copy2(expiry,work/'expiry-fixture')
        dependencies=run('ldd',str(executable));assert 'not found' not in dependencies
        libraries=set(re.findall(r'(/[\w/+.,-]+)',dependencies))
        for name in libraries:
            path=Path(name)
            if path.is_file():
                target=rootfs/name.lstrip('/');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path.resolve(),target)
        cache=(build/'CMakeCache.txt').read_text()
        match=re.search(r'^FETCHCONTENT_SOURCE_DIR_PFME_BOOST:[^=]*=(.*)$',cache,re.MULTILINE)
        boost=Path(match[1]) if match and match[1] else build/'_deps/pfme_boost-src'
        assert (boost/'boost/json/src.hpp').is_file()
        run('g++','-static','-O2','-std=c++17','-I'+str(boost),str(source/'tests/server/unix_proxy_fixture.cpp'),'-o',str(work/'proxy-fixture'))
        (work/'Dockerfile').write_text('FROM scratch AS runtime\nCOPY rootfs/ /\nUSER 1000:1000\nENTRYPOINT ["/app/pfme_server"]\nFROM runtime AS verification\nCOPY expiry-fixture /test/expiry-fixture\nCOPY proxy-fixture /test/proxy-fixture\n')
        (work/'Proxy.Dockerfile').write_text('FROM scratch\nCOPY proxy-fixture /client\nENTRYPOINT ["/client"]\n')
        for tag,target,file in [(runtime,'runtime','Dockerfile'),(verification,'verification','Dockerfile'),(proxy,None,'Proxy.Dockerfile')]:
            cmd=['docker','build','--network=none','--pull=false','-q','-t',tag,'-f',str(work/file)]
            if target:cmd+=['--target',target]
            run(*cmd,str(work));images.append(tag)
        sockets=work/'socket';policy=work/'policy';sockets.mkdir(mode=0o700);policy.mkdir(mode=0o700)
        def mount(path,target,readonly=False):return ['--mount',f'type=bind,src={path},dst={target}'+(',readonly' if readonly else '')]
        bridge=Bridge(['docker','run','--rm','-i','--network=none','--read-only','--cap-drop=ALL','--cap-add=DAC_OVERRIDE','--security-opt=no-new-privileges','--pids-limit=16','--memory=64m',*mount(sockets,'/socket',True),*mount(policy,'/policy'),proxy,'/socket/backend.sock','/policy/admission.json',HOST])
        assert bridge.send(op='info')['uid']==0
        users=[dict(login=f'fixture{i}@example.com',principal=f'fixture-{i}') for i in range(5)]
        def policy_value(revision,entries):return dict(version=1,revision=revision,users=entries)
        bridge.send(op='policy',value=policy_value(1,users))
        def start(expiration=False):
            name='pfme-stage1-'+nonce+('-expiry' if expiration else '-game')
            cmd=['docker','run','--detach','--name',name,'--label','pfme.stage1-test='+nonce,'--network=none','--read-only','--cap-drop=ALL','--security-opt=no-new-privileges','--pids-limit=32','--memory=256m','--cpus=1','--ulimit=nofile=128:128',*mount(sockets,'/socket'),*mount(policy,'/policy',True)]
            if expiration:cmd+=['--entrypoint','/test/expiry-fixture']
            cmd+=[verification,'--serve-socket','/socket/backend.sock','--serve-origin','https://'+HOST,'--serve-policy','/policy/admission.json','--runtime','party','--seed','2','--run-for-ms','180000']
            identity=run(*cmd);containers.append(identity)
            until=time.monotonic()+12
            while time.monotonic()<until:
                if 'listening=unix protocol=1' in run('docker','logs',identity):break
                time.sleep(.1)
            else:raise AssertionError('private runtime failed to start: '+run('docker','logs',identity))
            info=json.loads(run('docker','inspect',identity))[0];config=info['HostConfig']
            assert config['NetworkMode']=='none' and config['ReadonlyRootfs'] and not config['Privileged']
            assert config['CapDrop']==['ALL'] and not config.get('CapAdd')
            assert config['Memory']==268435456 and config['NanoCpus']==1000000000 and config['PidsLimit']==32
            assert info['Config']['User']=='1000:1000' and not info['NetworkSettings'].get('Ports')
            assert all(item['Source'] in (str(sockets),str(policy)) for item in info['Mounts'])
            assert len(info['Mounts'])==2 and any(not item['RW'] and item['Destination']=='/policy' for item in info['Mounts'])
            return identity
        def stop(identity):
            info=json.loads(run('docker','inspect',identity))[0];assert info['Config']['Labels']['pfme.stage1-test']==nonce
            run('docker','stop','-t','4',identity);logs=run('docker','logs',identity)
            assert 'stopped' in logs and 'listening=127.' not in logs
            assert not (sockets/'backend.sock').exists(),'owned socket not removed'
        def rejected_start():
            result=subprocess.run(['docker','run','--rm','--network=none','--read-only','--cap-drop=ALL','--security-opt=no-new-privileges',
                '--pids-limit=32','--memory=256m','--cpus=1','--ulimit=nofile=128:128',*mount(sockets,'/socket'),*mount(policy,'/policy',True),verification,
                '--serve-socket','/socket/backend.sock','--serve-origin','https://'+HOST,'--serve-policy','/policy/admission.json','--runtime','party','--run-for-ms','1000'],capture_output=True,text=True,timeout=10)
            assert result.returncode==1 and 'listening=' not in result.stdout and not (sockets/'backend.sock').is_socket()
        try:
            admission=policy/'admission.json'
            admission.unlink();admission.write_text(json.dumps(policy_value(1,users)))
            assert admission.stat().st_uid==os.getuid();rejected_start()
            admission.unlink();os.mkfifo(admission,0o600);rejected_start()
            bridge.send(op='policy',value=policy_value(1,users),unsafe_mode=True);rejected_start()
            bridge.send(op='policy',value=policy_value(1,users));admission.rename(policy/'root-original.json')
            admission.symlink_to('root-original.json');rejected_start()
            bridge.send(op='policy',value=policy_value(1,users))
            sockets.chmod(0o755);rejected_start();sockets.chmod(0o700)
            (sockets/'backend.sock').write_text('owned collision marker');rejected_start()
            assert (sockets/'backend.sock').read_text()=='owned collision marker';(sockets/'backend.sock').unlink()
            game=start()
            bypass=Bridge(['docker','exec','-i','--user','1000:1000',game,'/test/proxy-fixture','/socket/backend.sock','/policy/admission.json',HOST])
            isolation=bypass.send(op='isolation');assert isolation['uid']==1000
            assert all(v is True for k,v in isolation.items() if k!='uid'),isolation
            assert not bypass.send(op='open',slot=0,login=users[0]['login'])['ok'],'non-root local process bypass'
            bypass.stop()
            for fields in [{},{'login':'unknown@example.com'},{'login':users[0]['login'],'duplicate_login':users[1]['login']},{'login':users[0]['login'],'host':'wrong.invalid'},{'login':users[0]['login'],'origin':'https://wrong.invalid'}]:
                assert not bridge.send(op='open',slot=7,**fields)['ok'],fields
            page=bridge.send(op='get',slot=7,login=users[0]['login']);assert page['ok'] and 'data-auth-mode="serve"' in page['body'] and 'connect-src wss://'+HOST in page['headers']
            assert not bridge.send(op='get',slot=7,login='unknown@example.com')['ok']
            clients=[];views=[]
            for index in range(4):
                client=Client(bridge,index,users[index]['login']);clients.append(client)
                assert client.call('screen',since='0',offset=0)['error']['code']=='NotAuthenticated'
                assert client.call('authenticate',secret='a'*64)['error']['code']=='AuthenticationFailed'
                assert client.call('authenticate')['ok']
                joined=client.call('join',name=f'Player{index}');assert joined['ok'] and 'reconnect_token' not in joined['data']
                assert client.call('play')['ok'];view=View(client);views.append(view);assert any(view.frame())
                for _ in range(5):tap(client,'a')
            fifth=Client(bridge,4,users[4]['login']);assert fifth.call('authenticate')['ok']
            before=clients[0].call('players')['data']['players']
            assert fifth.call('join',name='Fifth')['error']['code']=='Full'
            assert fifth.call('reconnect')['error']['code']=='NoCharacter'
            assert fifth.call('reconnect',player_id=0)['error']['code']=='OwnershipRequired'
            assert clients[0].call('players')['data']['players']==before
            retained=views[1].frame();clients[1].close();time.sleep(.15)
            replacement=Client(bridge,6,users[1]['login']);assert replacement.call('authenticate')['ok']
            assert replacement.call('join',name='Duplicate')['error']['code']=='ResumeRequired'
            assert replacement.call('reconnect')['ok'] and replacement.call('play')['ok']
            clients[1]=replacement;views[1]=View(replacement);assert views[1].frame()==retained
            # Production menus, movement and separate original views through the protected transport.
            for index,client in enumerate(clients):
                tap(client,'cw');tap(client,'cw');tap(client,'a')
                if index:tap(client,'a')
            for _ in range(11):tap(clients[3],'encoder')
            room1=views[3].frame();room0=views[1].frame();assert room1!=room0
            assert clients[0].call('input',action='encoder',down=True,player_id=3)['error']['code']=='InvalidRequest'
            # Duplicate sequence must close without running the replayed action.
            replay_client=Client(bridge,4,users[4]['login']);assert replay_client.call('authenticate')['ok']
            assert replay_client.call('ping')['ok']
            replay=bridge.send(op='rpc',slot=4,request=dict(v=1,id=str(replay_client.identifier),type='join',name='Replay'))
            assert replay.get('response',{}).get('error',{}).get('code')=='ReplayRejected'
            # Any policy revision invalidates all old leases, and removal denies new handshakes.
            assert replacement.call('ping')['ok'],'revocation fixture must be live before policy update'
            bridge.send(op='policy',value=policy_value(2,[u for i,u in enumerate(users) if i!=1]));time.sleep(1.25)
            assert bridge.send(op='rpc',slot=6,request=dict(v=1,id=str(replacement.identifier+1),type='ping')).get('closed')
            assert not bridge.send(op='open',slot=7,login=users[1]['login'])['ok']
            # A stale root policy is not a way to restore authorization.
            bridge.send(op='policy',value=policy_value(1,users));time.sleep(1.25)
            assert not bridge.send(op='open',slot=7,login=users[0]['login'])['ok']
            bridge.send(op='policy',value=policy_value(3,users));time.sleep(1.25)
            restored=Client(bridge,7,users[1]['login']);assert restored.call('authenticate')['ok']
            recovered=restored.call('reconnect');assert recovered['ok'] and recovered['data']['player']['player_id']==1
            assert restored.call('play')['ok'];assert any(View(restored).frame())
            stop(game)
            # The separately compiled two-second fixture uses the same kernel/HTTP/game code.
            expiry_game=start(True);expired=Client(bridge,0,users[0]['login']);assert expired.call('authenticate')['ok']
            assert expired.call('join',name='Expires')['ok'] and expired.call('play')['ok']
            tap(expired,'a') # Stable original class menu, not the animated title.
            saved=View(expired).frame()
            time.sleep(2.15)
            assert bridge.send(op='rpc',slot=0,request=dict(v=1,id=str(expired.identifier+1),type='screen',since='0',offset=0)).get('closed')
            renewed=Client(bridge,1,users[0]['login']);assert renewed.call('authenticate')['ok'] and renewed.call('reconnect')['ok'] and renewed.call('play')['ok']
            assert View(renewed).frame()==saved
            assert renewed.call('leave')['ok'] and renewed.call('reconnect')['error']['code']=='NoCharacter'
            stop(expiry_game)
            report=dict(unsafe_policy_and_socket_paths_denied=True,kernel_uid_denial=True,trusted_root_fixture=True,identity_headers_checked=True,unauthorized_http_denied=True,principal_takeover_denied=True,
                four_owned_characters=True,independent_original_room_frames=True,room0_sha256=hashlib.sha256(room0).hexdigest(),room1_sha256=hashlib.sha256(room1).hexdigest(),
                fifth_slot_preserved=True,reconnect_retained_frame=True,revision_revocation=True,rollback_policy_denied=True,absolute_expiry=True,renewed_character_retained=True,leave_revoked=True,
                replay_rejected=True,isolation=isolation,serve_activated=False,real_tailscale_authentication_tested=False)
        finally:
            if bridge:bridge.stop()
            for identity in containers:
                info=json.loads(run('docker','inspect',identity))[0];assert info['Config']['Labels']['pfme.stage1-test']==nonce
                run('docker','rm','--force',identity)
            for tag in reversed(images):run('docker','image','rm',tag)
        current=set(run('docker','ps','--quiet').splitlines());assert set(baseline)<=current,'existing container stopped'
        report['existing_containers_unchanged']=True
        print(json.dumps(report,sort_keys=True))

if __name__=='__main__':main()
