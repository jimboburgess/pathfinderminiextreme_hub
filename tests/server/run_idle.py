"""Real RFC6455 idle pong versus silent peer; disposable loopback server only."""
import base64,hashlib,json,os,secrets,select,signal,socket,struct,subprocess,sys,time

def exact(s,n):
    data=b''
    while len(data)<n:
        block=s.recv(n-len(data))
        if not block:raise EOFError
        data+=block
    return data
def frame(s):
    a,b=exact(s,2);n=b&127
    if n==126:n=struct.unpack('!H',exact(s,2))[0]
    elif n==127:n=struct.unpack('!Q',exact(s,8))[0]
    assert not b&128 and n<=32768
    return a&15,exact(s,n)
def send(s,op,data):
    mask=secrets.token_bytes(4);n=len(data)
    header=bytes([128|op,128|n]) if n<126 else bytes([128|op,254])+struct.pack('!H',n)
    s.sendall(header+mask+bytes(c^mask[i%4] for i,c in enumerate(data)))
def open_client(port,secret):
    s=socket.create_connection(('127.0.0.1',port),timeout=5);nonce=base64.b64encode(secrets.token_bytes(16)).decode()
    s.sendall((f'GET / HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {nonce}\r\nSec-WebSocket-Version: 13\r\n\r\n').encode())
    header=b''
    while not header.endswith(b'\r\n\r\n'):header+=exact(s,1)
    assert b'101 Switching Protocols' in header
    assert base64.b64encode(hashlib.sha1((nonce+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()) in header
    assert frame(s)[0]==1
    send(s,1,json.dumps(dict(v=1,id='auth',type='authenticate',secret=secret)).encode())
    assert json.loads(frame(s)[1])['ok'];return s
def main():
    secret=secrets.token_hex(32);env=dict(os.environ,PFME_DEV_SECRET=secret)
    flags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name=='nt' else 0
    p=subprocess.Popen([sys.argv[1],'--port','0','--run-for-ms','60000'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,creationflags=flags)
    peers=[]
    try:
        port=int(p.stdout.readline().split(':')[1].split()[0]);live=open_client(port,secret);silent=open_client(port,secret);peers=[live,silent]
        began=time.monotonic();pongs=0
        # No application requests during this window. A real browser handles pongs
        # in its network stack even when gameplay timers are background-throttled.
        while time.monotonic()-began<49:
            if select.select([live],[],[],.5)[0]:
                op,data=frame(live);assert op==9,'idle authenticated connection unexpectedly closed';send(live,10,data);pongs+=1
        assert pongs>=3
        send(live,1,b'{"v":1,"id":"after-idle","type":"ping"}')
        while True:
            op,data=frame(live)
            if op==9:send(live,10,data);continue
            assert op==1 and json.loads(data)['ok'];break
        # Silent socket never sends pong; consume queued ping frames then EOF.
        dead=False
        try:
            while True:
                op,_=frame(silent)
                if op==8:dead=True;break
        except (EOFError,ConnectionError):dead=True
        assert dead,'45-second dead peer was retained'
        print('PASS real idle protocol pongs preserve gameplay-idle connection; silent peer expires; healthy peer remains responsive')
    finally:
        for s in peers:s.close()
        if p.poll() is None:p.send_signal(signal.CTRL_BREAK_EVENT if os.name=='nt' else signal.SIGTERM)
        out,err=p.communicate(timeout=8)
        assert p.returncode==0 and secret not in out+err
        assert 'heartbeat_timeout' in err
if __name__=='__main__':main()
