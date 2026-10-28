"""Real loopback HTTP/WebSocket checks. No packages, saved credentials or public bind."""
import base64
import hashlib
import http.client
import json
import os
import secrets
import socket
import struct
import subprocess
import sys
import time
sys.dont_write_bytecode=True
from run_security import line_with_timeout, listening_addresses, stop

class Client:
    def __init__(self, port, origin=None):
        self.sock=socket.create_connection(('127.0.0.1',port),timeout=4)
        self.sock.settimeout(4)
        self.identifier=0
        key=base64.b64encode(secrets.token_bytes(16)).decode()
        headers=[f'GET / HTTP/1.1',f'Host: 127.0.0.1:{port}','Upgrade: websocket','Connection: Upgrade',f'Sec-WebSocket-Key: {key}','Sec-WebSocket-Version: 13']
        if origin is not None:headers.append('Origin: '+origin)
        self.sock.sendall(('\r\n'.join(headers)+'\r\n\r\n').encode())
        response=b''
        while not response.endswith(b'\r\n\r\n'):
            byte=self.sock.recv(1)
            if not byte:raise ConnectionError('Handshake rejected')
            response+=byte
            if len(response)>4096:raise RuntimeError('Oversized HTTP reply')
        assert response.startswith(b'HTTP/1.1 101 ')
        expected=base64.b64encode(hashlib.sha1((key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest())
        assert expected in response
        assert self.read()['type']=='hello'

    def exact(self,size):
        data=b''
        while len(data)<size:
            part=self.sock.recv(size-len(data))
            if not part:raise ConnectionError('Socket closed')
            data+=part
        return data

    def read(self):
        message=bytearray()
        while True:
            first,second=self.exact(2);size=second&127
            if size==126:size=struct.unpack('!H',self.exact(2))[0]
            elif size==127:size=struct.unpack('!Q',self.exact(8))[0]
            assert not second&128 and size<=16384 and not first&0x70,(first,second,size)
            opcode=first&15;payload=self.exact(size)
            if opcode==9:self.send(payload,10);continue
            if opcode==10:continue
            assert opcode in (0,1) and (opcode==1)==(not message)
            message+=payload;assert len(message)<=16384
            if first&128:return json.loads(message)

    def send(self,body,opcode=1):
        mask=secrets.token_bytes(4)
        header=bytes([0x80|opcode,0x80|len(body)]) if len(body)<126 else bytes([0x80|opcode,0xfe])+struct.pack('!H',len(body))
        self.sock.sendall(header+mask+bytes(b^mask[i%4] for i,b in enumerate(body)))

    def call(self,kind,**fields):
        self.identifier+=1
        body=json.dumps(dict(v=1,id=str(self.identifier),type=kind,**fields)).encode()
        self.send(body)
        response=self.read();assert response['id']==str(self.identifier)
        time.sleep(.08) # Entire test stays below the unchanged command cap.
        return response

    def close(self):self.sock.close()

def main():
    executable=sys.argv[1]
    secret=secrets.token_hex(32);env=os.environ.copy();env['PFME_DEV_SECRET']=secret
    process=subprocess.Popen([executable,'--port','0','--seed','2','--run-for-ms','60000'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,
                             creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name=='nt' else 0)
    clients=[]
    try:
        line=line_with_timeout(process.stdout);assert line.startswith('listening=127.0.0.1:')
        port=int(line.split(':')[1].split()[0]);origin=f'http://127.0.0.1:{port}'
        assert listening_addresses(process.pid,port)==[f'127.0.0.1:{port}']
        for path,kind in [('/play','text/html'),('/play.js','text/javascript'),('/play.css','text/css'),('/audio.js','text/javascript')]:
            connection=http.client.HTTPConnection('127.0.0.1',port,timeout=3);connection.request('GET',path);reply=connection.getresponse()
            body=reply.read();assert reply.status==200 and kind in reply.getheader('Content-Type')
            assert reply.getheader('Cache-Control')=='no-store' and reply.getheader('X-Content-Type-Options')=='nosniff'
            assert all(marker not in body for marker in (b"@discord/embedded-app-sdk",b"discordsays.com",b"/activity.js",b"pfmeActivity")), "Discord dependency in standalone assets"
            assert "frame-ancestors 'none'" in reply.getheader('Content-Security-Policy') and secret.encode() not in body
            if path=='/play':
                assert b'240&times;240' in body and b'/audio.js' in body
                assert all(str(scale).encode()+b'&times;' in body for scale in range(1,6))
            connection.close()
        # The TCP listener cannot attest Serve identity, even for same-origin
        # HTTP assets. Fail closed instead of trusting a plausible local header.
        for header in ('Tailscale-User-Login','tAiLsCaLe-UsEr-LoGiN','Tailscale-App-Capabilities','X-Forwarded-User','Forwarded','X-Real-IP'):
            connection=http.client.HTTPConnection('127.0.0.1',port,timeout=3)
            connection.request('GET','/play',headers={header:'claimed-user@example.com'})
            try:connection.getresponse();raise AssertionError('Untrusted identity header accepted')
            except http.client.RemoteDisconnected:pass
            finally:connection.close()
        print('untrusted_proxy_identity_http=denied')
        print('fixed_http_assets=4 security_headers=passed no_embedded_credentials=true standalone_discord_dependencies=none')
        for invalid in ('null','https://untrusted.invalid',f'http://localhost:{port}',origin+'.invalid'):
            try:
                invalid_client=Client(port,invalid);invalid_client.close();raise AssertionError('Foreign Origin accepted')
            except ConnectionError:pass
        owner=Client(port,origin);clients.append(owner)
        assert owner.call('screen',since='0',offset=0)['error']['code']=='NotAuthenticated'
        assert owner.call('authenticate',secret=secret)['ok']
        joined=owner.call('join',name='WireTester');assert joined['ok'];proof=joined['data']['reconnect_token']
        assert owner.call('play')['ok']
        observer=Client(port,origin);clients.append(observer)
        assert observer.call('authenticate',secret=secret)['ok']
        assert observer.call('join',name='Observer')['ok']
        assert observer.call('play')['error']['code']=='RuntimeOccupied'
        assert observer.call('screen',since='0',offset=0)['error']['code']=='OwnershipRequired'
        assert observer.call('input',action='a',down=True)['error']['code']=='OwnershipRequired'
        assert owner.call('input',action='a',down=True,hp=100)['error']['code']=='InvalidRequest'
        since='0';pixels=bytearray(115200)
        def screen():
            nonlocal since
            data=bytearray();offset=0
            for chunk in range(32):
                reply=owner.call('screen',since=since,offset=offset);assert reply['ok'];frame=reply['data']
                assert frame['base']==since and frame['offset']==offset and frame['total']<=230400
                data+=base64.b64decode(frame['pixels'],validate=True);offset=len(data)
                if frame['more']:continue
                assert offset==frame['total'];cursor=0
                while cursor<len(data):
                    first,encoded=struct.unpack_from('<HH',data,cursor);cursor+=4
                    count=encoded&32767;repeated=bool(encoded&32768);size=2 if repeated else count*2
                    assert count and first+count<=57600 and cursor+size<=len(data)
                    pixels[first*2:(first+count)*2]=data[cursor:cursor+2]*count if repeated else data[cursor:cursor+size]
                    cursor+=size
                since=frame['frame'];return bytes(pixels)
            raise AssertionError('Unbounded frame')
        title=screen();assert any(title)
        assert owner.call('input',action='a',down=True)['ok'];assert owner.call('input',action='a',down=False)['ok']
        time.sleep(.35);creation=screen();assert creation!=title
        assert owner.call('disconnect')['ok'];owner.close();clients.remove(owner)
        owner=Client(port,origin);clients.append(owner)
        assert owner.call('authenticate',secret=secret)['ok']
        assert owner.call('reconnect',token=proof)['ok']
        assert owner.call('play')['ok'];since='0';pixels[:]=bytes(115200)
        assert screen()==creation
        print('same_origin_authentication=passed ownership=passed real_title_input=passed bounded_frame_transfer=passed reconnect=passed')
        for client in clients:client.close()
        clients.clear();stop(process);output,errors=process.communicate(timeout=6)
        assert process.returncode==0 and 'stopped' in output and secret not in output+errors
        assert not listening_addresses(process.pid,port)
        print('clean_shutdown=true no_remaining_listener=true')
    finally:
        for client in clients:client.close()
        if process.poll() is None:
            stop(process)
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)

if __name__=='__main__':main()
