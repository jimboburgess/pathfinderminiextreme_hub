"""Disposable network-disabled test client; JSON stdin/stdout consumed in memory."""
import asyncio,json,sys,socket,http.client,re,contextlib
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
    async def call(self,kind,fields):
        async with self.lock:
            self.identifier+=1;identifier=str(self.identifier)
            await self.ws.send(json.dumps(dict(v=1,id=identifier,type=kind,**fields)))
            while True:
                reply=json.loads(await asyncio.wait_for(self.ws.recv(),5))
                if reply.get('id')==identifier:break
            await asyncio.sleep(.085);return reply
    async def ping(self):
        try:
            while True:await asyncio.sleep(3);await self.call('ping',{})
        except Exception:pass
async def main():
    clients={}
    try:
        while True:
            line=await asyncio.to_thread(sys.stdin.readline)
            if not line:break
            q=json.loads(line)
            try:
                op=q['op'];result={}
                if op=='login':
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
                    c=Client(ws);clients[q['slot']]=c;c.keep=asyncio.create_task(c.ping());result={'opened':True}
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
