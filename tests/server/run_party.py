"""Authenticated production party exploration over the real loopback protocol.
No gameplay debug endpoint, package installation, persistent secret or public bind.
"""
import base64
import hashlib
import json
import os
from pathlib import Path
import secrets
import struct
import subprocess
import sys
import time
sys.dont_write_bytecode=True
from run_playable import Client
from run_security import line_with_timeout,listening_addresses,stop

def check_audio(reply, expected):
    assert ('audio' in reply)==expected,reply
    if not expected:return
    audio=reply['audio'];assert set(audio)=={'cursor','now_ms','frequency','duty','reset','events'}
    assert str(int(audio['cursor']))==audio['cursor'] and int(audio['cursor'])>=0
    assert 0<=audio['now_ms']<=4294967295 and 0<=audio['frequency']<=65535
    assert audio['duty'] in (128,256,512,768) and len(audio['events'])<=32
    if audio['reset']:assert not audio['events']
    previous=0
    for event in audio['events']:
        assert len(event)==4 and previous<int(event[0])<=int(audio['cursor'])
        previous=int(event[0]);assert 0<=event[1]<=4294967295 and 0<=event[2]<=65535 and event[3] in (128,256,512,768)

class View:
    def __init__(self,client):
        self.client=client;self.since='0';self.pixels=bytearray(115200)
    def frame(self):
        data=bytearray();offset=0
        for _ in range(32):
            reply=self.client.call('screen',since=self.since,offset=offset)
            assert reply['ok'],reply
            check_audio(reply,True)
            frame=reply['data']
            assert frame['base']==self.since and frame['offset']==offset and frame['total']<=230400
            data+=base64.b64decode(frame['pixels'],validate=True);offset=len(data)
            if frame['more']:continue
            assert offset==frame['total'];at=0
            while at<len(data):
                first,encoded=struct.unpack_from('<HH',data,at);at+=4
                count=encoded&32767;repeat=bool(encoded&32768);size=2 if repeat else count*2
                assert count and first+count<=57600 and at+size<=len(data)
                self.pixels[first*2:(first+count)*2]=data[at:at+2]*count if repeat else data[at:at+size]
                at+=size
            self.since=frame['frame'];return bytes(self.pixels)
        raise AssertionError('Unbounded frame')

_party_heartbeats=[]
_party_last_heartbeat=0

def tap(client,action):
    global _party_last_heartbeat
    if _party_heartbeats and time.monotonic()-_party_last_heartbeat>10:
        for peer in _party_heartbeats:assert peer.call('ping')['ok']
        _party_last_heartbeat=time.monotonic()
    assert client.call('input',action=action,down=True)['ok']
    if action in ('cw','ccw'):
        # Real browser rotary detents send only down; keep below 20 requests/s.
        time.sleep(.14);return
    time.sleep(.04)
    assert client.call('input',action=action,down=False)['ok']
    time.sleep(.34)

def form_town_party(clients):
    global _party_heartbeats,_party_last_heartbeat
    _party_heartbeats=list(clients);_party_last_heartbeat=0
    # Town: Bulletin Board -> Create Party. Read only this caller's party code.
    for _ in range(4):tap(clients[0],'cw')
    tap(clients[0],'a');tap(clients[0],'a')
    party=clients[0].call('party_status');assert party['ok'],party
    code=party['data']['code'];tap(clients[0],'b');tap(clients[0],'b')
    alphabet='23456789ABCDEFGHJKMNPQRSTUVWXYZ'
    for client in clients[1:]:
        for _ in range(4):tap(client,'cw')
        tap(client,'a');tap(client,'cw');tap(client,'a')
        for character in code:
            for _ in range(alphabet.index(character)):tap(client,'cw')
            tap(client,'a')
        joined=client.call('party_status');assert joined['ok'] and joined['data']['code']==code,joined
        tap(client,'b');tap(client,'b')
    # Bulletin Board selection -> Enter Dungeon (counterclockwise twice).
    for client in clients:
        tap(client,'ccw');tap(client,'ccw')
    _party_heartbeats=[]
    return code

def main():
    executable=sys.argv[1];count=int(sys.argv[2]) if len(sys.argv)>2 else 2
    assert 1<=count<=4
    secret=secrets.token_hex(32);env=os.environ.copy();env['PFME_DEV_SECRET']=secret
    process=subprocess.Popen([executable,'--port','0','--seed','2','--runtime','party','--run-for-ms','300000'],
        env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,
        creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name=='nt' else 0)
    clients=[];proofs=[];views=[]
    try:
        line=line_with_timeout(process.stdout);assert line.startswith('listening=127.0.0.1:')
        port=int(line.split(':')[1].split()[0]);origin=f'http://127.0.0.1:{port}'
        assert listening_addresses(process.pid,port)==[f'127.0.0.1:{port}']
        for index in range(count):
            client=Client(port,origin);clients.append(client)
            assert client.call('screen',since='0',offset=0)['error']['code']=='NotAuthenticated'
            assert client.call('party_status')['error']['code']=='NotAuthenticated'
            assert client.call('authenticate',secret=secret)['ok']
            joined=client.call('join',name=f'Party{index}');assert joined['ok'],joined
            proofs.append(joined['data']['reconnect_token'])
            check_audio(client.call('ping'),False)
            played=client.call('play');assert played['data']['mode']=='original-handheld-party' and played['data']['audio']
            check_audio(played,True);assert played['audio']['reset']
            assert client.call('party_status',player_id=0)['error']['code']=='InvalidRequest'
            assert client.call('party_status')['error']['code']=='NotInParty'
            assert client.call('input',action='a',down=True,audio_listener=0)['error']['code']=='InvalidRequest'
            view=View(client);views.append(view);title=view.frame();assert any(title)
            for _ in range(5):tap(client,'a')
            town=view.frame();assert town!=title
        # Each accepted Fighter has a complete production Character; the native
        # gameplay tests assert HP/inventory and exact tile coordinates.
        form_town_party(clients)
        tap(clients[0],'a') # Leader embarks every connected member once.
        for client in clients:
            assert client.call('input',action='encoder',down=True,player_id=0)['error']['code']=='InvalidRequest'
        if count==4:
            fifth=Client(port,origin)
            try:
                assert fifth.call('authenticate',secret=secret)['ok']
                before=clients[0].call('players')['data']['players']
                assert fifth.call('join',name='Fifth')['error']['code']=='Full'
                assert clients[0].call('players')['data']['players']==before
                assert fifth.call('screen',since='0',offset=0)['error']['code']=='NotBound'
            finally:fifth.close()
        # Move the easternmost actor through the authored east corridor while
        # the others remain in the entrance. No coordinate/state RPC is used.
        explorer=3 if count==4 else 0
        original=views[explorer].frame()
        for _ in range(12 if explorer==0 else 11):tap(clients[explorer],'encoder')
        separate=views[explorer].frame();assert separate!=original
        Path(f'party-{count}-room1.rgb565').write_bytes(separate)
        if count>1:
            other=1 if explorer!=1 else 0
            entrance=views[other].frame();assert entrance!=separate
            Path(f'party-{count}-room0.rgb565').write_bytes(entrance)
            # B changes only the other actor's UI/input path; a supplied owner
            # field is rejected by the same strict schema as the legacy mode.
            tap(clients[other],'a');menu=views[other].frame();assert menu!=entrance
            tap(clients[other],'b')
            assert views[other].frame()==entrance
        # Detached sockets cannot create another character; the existing
        # capability restores the same production framebuffer and actor.
        reconnect_index=1 if count>1 else explorer
        owner=clients[reconnect_index];retained=views[reconnect_index].frame()
        assert owner.call('disconnect')['ok'];owner.close()
        replacement=Client(port,origin);clients[reconnect_index]=replacement
        assert replacement.call('authenticate',secret=secret)['ok']
        assert replacement.call('reconnect',token=secrets.token_hex(32))['error']['code']=='OwnershipRequired'
        assert replacement.call('reconnect',token=proofs[reconnect_index])['ok']
        assert replacement.call('play')['ok'];views[reconnect_index]=View(replacement)
        assert views[reconnect_index].frame()==retained
        duplicate=Client(port,origin)
        try:
            assert duplicate.call('authenticate',secret=secret)['ok']
            assert duplicate.call('reconnect',token=proofs[reconnect_index])['error']['code']=='AlreadyConnected'
        finally:duplicate.close()
        if count==4:
            retired=clients[1];old_proof=proofs[1]
            assert retired.call('leave')['ok']
            assert retired.call('screen',since='0',offset=0)['error']['code']=='NotBound'
            assert retired.call('reconnect',token=old_proof)['error']['code']=='OwnershipRequired'
            new=retired.call('join',name='Replacement');assert new['ok']
            proofs[1]=new['data']['reconnect_token']
            assert retired.call('play')['ok'];replacement_view=View(retired)
            assert replacement_view.frame()!=retained
        print(json.dumps(dict(party=count,authenticated_clients=count,shared_dungeon=True,
            separate_original_frames=count>1,room1_sha256=hashlib.sha256(separate).hexdigest(),
            input_owner_field_rejected=True,reconnect_retained_frame=True,fifth_rejected=count==4)))
        for client in clients:client.close()
        clients.clear();stop(process);output,errors=process.communicate(timeout=8)
        assert process.returncode==0 and 'stopped' in output
        assert secret not in output+errors and all(proof not in output+errors for proof in proofs)
        assert not listening_addresses(process.pid,port)
        print('clean_shutdown=true loopback_only=true secret_free_logs=true')
    finally:
        for client in clients:client.close()
        if process.poll() is None:
            stop(process)
            try:process.wait(timeout=8)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)

if __name__=='__main__':main()
