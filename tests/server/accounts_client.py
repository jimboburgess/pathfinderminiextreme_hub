"""Disposable network-disabled test client; JSON stdin/stdout consumed in memory."""
import asyncio,json,sys,socket,http.client,re,contextlib,base64,struct,time,hashlib
from http.cookies import SimpleCookie
from urllib.parse import urlencode
from websockets.asyncio.client import unix_connect
HOST='game.noburdenmusics.com';ORIGIN='https://'+HOST
class HTTP(http.client.HTTPConnection):
    def connect(self):
        self.sock=socket.socket(socket.AF_UNIX);self.sock.settimeout(8);self.sock.connect('/front/gateway.sock')
def http(path,cookie='',form=None,origin=ORIGIN,proto='https'):
    h=HTTP(HOST,timeout=8);headers={'Origin':origin,'X-Forwarded-Proto':proto}
    if cookie:headers['Cookie']=cookie
    body=None
    if form is not None:body=urlencode(form);headers['Content-Type']='application/x-www-form-urlencoded'
    h.request('POST' if form is not None else 'GET',path,body,headers);r=h.getresponse();data=r.read(262145)
    cookies=SimpleCookie()
    for k,v in r.getheaders():
        if k.lower()=='set-cookie':cookies.load(v)
    result={'status':r.status,'body':data.decode('utf-8','replace'),'cookies':{k:v.value for k,v in cookies.items()}}
    h.close();return result
class Client:
    def __init__(self,ws):self.ws=ws;self.identifier=0;self.lock=asyncio.Lock();self.keep=None
    async def call(self,kind,fields,pace=True):
        async with self.lock:
            self.identifier+=1;identifier=str(self.identifier)
            await self.ws.send(json.dumps(dict(v=1,id=identifier,type=kind,**fields)))
            while True:
                reply=json.loads(await asyncio.wait_for(self.ws.recv(),5))
                if reply.get('id')==identifier:break

            if pace:await asyncio.sleep(.085)
            return reply
    async def ping(self):
        try:
            while True:await asyncio.sleep(3);await self.call('ping',{})
        except Exception:pass
async def main():
    clients={};streams={}
    try:
        while True:
            line=await asyncio.to_thread(sys.stdin.readline)
            if not line:break
            q=json.loads(line)
            try:
                op=q['op'];result={}
                if op=='measure':
                    selected=q['slots'];duration=min(10,max(1,q.get('seconds',5)));rtts=[];wire=0;frames=0;requests=0;began=time.monotonic()
                    async def sample(slot):
                        nonlocal wire,frames,requests
                        state=streams.setdefault(slot,dict(since='0',offset=0,total=0,parts=bytearray(),pixels=bytearray(115200)))
                        start=time.monotonic();reply=await clients[slot].call('screen',{'since':state['since'],'offset':state['offset']},pace=False)
                        rtts.append((time.monotonic()-start)*1000);requests+=1;wire+=len(json.dumps(reply,separators=(',',':')).encode())
                        if not reply.get('ok'):raise ValueError('Measurement request rejected: '+reply.get('error',{}).get('code','unknown'))
                        f=reply['data'];chunk=base64.b64decode(f['pixels'],validate=True)
                        assert f['base']==state['since'] and f['offset']==state['offset'] and f['total']<=230400 and len(chunk)<=9216
                        state['parts']+=chunk;state['offset']+=len(chunk);assert len(state['parts'])<=230400
                        if f['more']:return
                        assert state['offset']==f['total'];data=state['parts'];at=0
                        while at<len(data):
                            first,encoded=struct.unpack_from('<HH',data,at);at+=4;count=encoded&32767;repeat=bool(encoded&32768);size=2 if repeat else count*2
                            assert count and first+count<=57600 and at+size<=len(data)
                            state['pixels'][first*2:(first+count)*2]=data[at:at+2]*count if repeat else data[at:at+size];at+=size
                        frames+=1;state.update(since=f['frame'],offset=0,parts=bytearray())
                    while time.monotonic()-began<duration:
                        start=time.monotonic();await asyncio.gather(*(sample(slot) for slot in selected));await asyncio.sleep(max(0,.1-(time.monotonic()-start)))
                    elapsed=time.monotonic()-began;rtts.sort()
                    result={'clients':len(selected),'seconds':round(elapsed,3),'requests':requests,'frames':frames,'bytes':wire,'bytes_per_client_second':round(wire/len(selected)/elapsed),'latency_ms_median':round(rtts[len(rtts)//2],3),'latency_ms_p95':round(rtts[min(len(rtts)-1,int(len(rtts)*.95))],3),'latency_ms_max':round(max(rtts),3),'pending_per_client_max':1}
                elif op=='close_info':
                    result={'close_code':clients[q['slot']].ws.close_code}
                elif op=='login':
                    first=http('/accounts/login/');csrf=first['cookies'].get('__Host-pfme_csrf','')
                    token=re.search(r'name="csrfmiddlewaretoken" value="([^"]+)"',first['body']).group(1)
                    result=http('/accounts/login/','__Host-pfme_csrf='+csrf,{'username':q['username'],'password':q['password'],'csrfmiddlewaretoken':token})
                    result.pop('body',None)
                elif op=='register':
                    first=http('/accounts/register/');csrf=first['cookies'].get('__Host-pfme_csrf','')
                    token=re.search(r'name="csrfmiddlewaretoken" value="([^"]+)"',first['body']).group(1)
                    result=http('/accounts/register/','__Host-pfme_csrf='+csrf,{'token':q['token'],'username':q['username'],'password1':q['password'],'password2':q['password'],'csrfmiddlewaretoken':token})
                    result.pop('body',None)
                elif op=='register_race':
                    def attempt():
                        first=http('/accounts/register/');csrf=first['cookies'].get('__Host-pfme_csrf','')
                        token=re.search(r'name="csrfmiddlewaretoken" value="([^"]+)"',first['body']).group(1)
                        return http('/accounts/register/','__Host-pfme_csrf='+csrf,{'token':q['token'],'username':q['username'],'password1':q['password'],'password2':q['password'],'csrfmiddlewaretoken':token})['status']
                    result={'statuses':await asyncio.gather(asyncio.to_thread(attempt),asyncio.to_thread(attempt))}
                elif op=='http':result=http(q['path'],q.get('cookie',''),q.get('form'),q.get('origin',ORIGIN),q.get('proto','https'))
                elif op=='open':
                    ws=await unix_connect('/front/gateway.sock',uri='ws://'+HOST+'/',origin=q.get('origin',ORIGIN),compression=None,
                      additional_headers={'Cookie':q.get('cookie',''),'X-Forwarded-Proto':'https',**q.get('headers',{})},open_timeout=4,max_size=32768)
                    c=Client(ws);clients[q['slot']]=c;streams.pop(q['slot'],None);c.keep=asyncio.create_task(c.ping());result={'opened':True}
                elif op=='rpc':result=await clients[q['slot']].call(q['type'],q.get('fields',{}))
                elif op=='close':
                    c=clients.pop(q['slot']);c.keep.cancel();await c.ws.close();result={'closed':True}
                else:raise ValueError('Unknown test operation')
                print(json.dumps({'result':result}),flush=True)
            except Exception as error:print(json.dumps({'failed':True,'failure_type':type(error).__name__}),flush=True)
    finally:
        for c in clients.values():
            c.keep.cancel()
            with contextlib.suppress(Exception):await c.ws.close()
if __name__=='__main__':asyncio.run(main())
