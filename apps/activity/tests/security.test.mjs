import test from 'node:test';
import assert from 'node:assert/strict';
import {once} from 'node:events';
import {spawn} from 'node:child_process';
import {createServer} from 'node:net';
import {readFile} from 'node:fs/promises';
import WebSocket from 'ws';
import {Auth} from '../auth.mjs';
import {flatJson,gameCommand,token,Denied} from '../policy.mjs';
import {createGateway} from '../gateway.mjs';
import {DiscordOAuth} from '../oauth.mjs';
const ids=['100000000000000001','100000000000000002','100000000000000003'];
const delay=n=>new Promise(resolve=>setTimeout(resolve,n));
class FixtureOAuth {
  constructor(){this.used=new Set();this.revoked=new Set();}
  async exchange(code){assert(!this.used.has(code));this.used.add(code);const index=['Fighter','Wizard','Rogue','Stranger'].indexOf(code);if(index<0)throw new Denied();return {id:ids[index]??'100000000000000004',accessToken:code,expires:Date.now()+3600000};}
  async verify(value){if(this.revoked.has(value))throw new Denied();return {id:ids[['Fighter','Wizard','Rogue'].indexOf(value)],expires:Date.now()+3600000};}
  async revoke(value){this.revoked.add(value);}
}
test('strict parser, command authority and monotonic replay protection',()=>{
  for(const input of ['{"a":1,"a":2}','{"a":1,"\\u0061":2}','{"a":[]}','{"a":{}}','{"a":true,}','{"a":1} false','{"a":"bad\u0000"}', ' '.repeat(4097)])assert.throws(()=>flatJson(input));
  assert.deepEqual(flatJson(JSON.stringify({text: 'escaped " quote',value:true})),{text: 'escaped " quote',value:true});
  const q={epoch:'test-epoch',v:1,id:'1',type:'input',action:'a',down:true};assert.equal(gameCommand(JSON.stringify(q),0,'test-epoch').action,'a');
  for(const changed of [{...q,id:'0'},{...q,id:'2'},{...q,v:2},{...q,player_id:0},{...q,hp:999},{...q,down:1},{...q,type:'reset'},{...q,action:'exec'}])assert.throws(()=>gameCommand(JSON.stringify(changed),0,'test-epoch'));
  assert.throws(()=>gameCommand(JSON.stringify(q),1,'test-epoch'));
});
test('challenges, expiry, upstream revocation, invitation and logout',async()=>{
  let clock=Date.now();const oauth=new FixtureOAuth(),a=new Auth({oauth,invited:new Set(ids),now:()=>clock});
  const expired=a.challenge();clock+=60001;await assert.rejects(a.login({challenge:expired,code:'Fighter'}));
  const challenge=a.challenge(),s=await a.login({challenge,code:'Fighter'});
  await assert.rejects(a.login({challenge,code:'Wizard'}));
  const ticket=await a.ticket(s.session);assert.equal((await a.consume(ticket)).identity.id,ids[0]);await assert.rejects(a.consume(ticket));
  const old=await a.ticket(s.session);clock+=10001;await assert.rejects(a.consume(old));
  oauth.revoked.add('Fighter');clock+=30000;await assert.rejects(a.ticket(s.session));assert.throws(()=>a.get(s.session));
  await assert.rejects(a.login({challenge:a.challenge(),code:'Stranger'}));
  const w=await a.login({challenge:a.challenge(),code:'Wizard'});await a.logout(w.session);assert.throws(()=>a.get(w.session));
  const r=await a.login({challenge:a.challenge(),code:'Rogue'});clock+=900001;assert.throws(()=>a.get(r.session));
  a.clear();assert.equal(a.sessions.size,0);
});
test('build reuses original pixels/controls and contains no backend credentials',async()=>{
  const original=await readFile(new URL('../../server/web/play.js',import.meta.url),'utf8');
  const adapted=await readFile(new URL('../dist/play.js',import.meta.url),'utf8');
  // Compare complete actual renderer/control source boundaries, not a new mock renderer.
  const section=(s,a,b)=>s.slice(s.indexOf(a),s.indexOf(b));
  assert.equal(section(adapted,'  function applyFrame','  async function connect'),section(original,'  function applyFrame','  async function connect'));
  assert(!adapted.includes('rpc("authenticate"'));assert(!adapted.includes('reconnect_token'));
  const html=await readFile(new URL('../dist/index.html',import.meta.url),'utf8');assert(!html.includes('id="secret"'));assert(html.includes('width="240" height="240"'));assert(html.includes('Sign in with Discord'));
  const bundle=await readFile(new URL('../dist/activity.js',import.meta.url),'utf8');assert(!/console\.(log|error|warn)\(/.test(bundle));assert(bundle.includes('disableConsoleLogOverride: true'));
});
test('production OAuth adapter verifies application, scope, expiry and API identity',async()=>{
  let app='100000000000000099',scopes=['identify'],expires=new Date(Date.now()+3600000).toISOString(),user=ids[0];
  const calls=[],request=async(path,method,headers,body)=>{calls.push({path,method,headers,body});
    if(path==='/api/oauth2/token')return {token_type:'Bearer',access_token:'fixture',expires_in:3600};
    if(path==='/api/v10/oauth2/@me')return {application:{id:app},scopes,expires};
    if(path==='/api/v10/users/@me')return {id:user};
    return {};
  };
  const oauth=new DiscordOAuth(app,'fixture-client-secret',request);
  assert.equal((await oauth.exchange('fixture-code')).id,ids[0]);
  const form=new URLSearchParams(calls[0].body);assert.equal(form.get('grant_type'),'authorization_code');assert.equal(form.get('code'),'fixture-code');assert.equal(calls[0].headers['Content-Type'],'application/x-www-form-urlencoded');
  app=ids[0];await assert.rejects(oauth.verify('fixture'));app='100000000000000099';
  scopes=[];await assert.rejects(oauth.verify('fixture'));scopes=['identify'];
  expires='2000-01-01T00:00:00Z';await assert.rejects(oauth.verify('fixture'));expires=new Date(Date.now()+3600000).toISOString();
  user='<script>';await assert.rejects(oauth.verify('fixture'));await oauth.revoke('fixture');assert.equal(calls.at(-1).path,'/api/oauth2/token/revoke');
});
test('real loopback gateway → actual C++ server; Discord boundary is a fixture', {skip:!process.env.PFME_TEST_SERVER,timeout:45000},async t=>{
  const probe=createServer();probe.listen(0,'127.0.0.1');await once(probe,'listening');const backendPort=probe.address().port;await new Promise(resolve=>probe.close(resolve));
  const backendSecret=token(),logs=[];
  const child=spawn(process.env.PFME_TEST_SERVER,['--port',String(backendPort),'--seed','2','--run-for-ms','40000'],{env:{...process.env,PFME_DEV_SECRET:backendSecret},windowsHide:true,stdio:['ignore','pipe','pipe']});
  let output='';child.stdout.on('data',b=>output+=b);child.stderr.on('data',()=>{});
  for(let i=0;i<100&&!output.includes('listening=');i++){if(child.exitCode!==null)throw new Error('Native server failed to start');await delay(30);}
  assert(output.includes('listening=127.0.0.1:'));
  const oauth=new FixtureOAuth();
  // Synthetic Host/Origin kept off real Discord and off all public interfaces.
  const origin='https://100000000000000099.discordsays.com';
  const gateway=await createGateway({origin,clientId:'100000000000000099',invited:ids,backendPort,backendSecret},{oauth,log:e=>logs.push(e)});
  const port=await gateway.listen(),sockets=[];
  t.after(async()=>{for(const ws of sockets)ws.terminate();await gateway.close();if(child.exitCode===null){child.kill(process.platform==='win32'?'SIGTERM':'SIGTERM');await once(child,'exit');}assert(!logs.join().includes(backendSecret));});
  const request=async(path,values,session,extra={})=>{
    const {request}=await import('node:http');return new Promise((resolve,reject)=>{
      const payload=values===undefined?'':typeof values==='string'?values:JSON.stringify(values);
      const req=request({host:'127.0.0.1',port,path,method:values===undefined?'GET':'POST',headers:{Host:new URL(origin).host,Origin:origin,'Content-Type':'application/json',...(session?{Authorization:`Bearer ${session}`} :{}),...extra}},res=>{let text='';res.on('data',b=>text+=b);res.on('end',()=>resolve({status:res.statusCode,text,headers:res.headers,json:()=>JSON.parse(text)}));});req.on('error',reject);req.end(payload);
    });
  };
  const login=async code=>{const challenge=(await request('/api/challenge',{})).json().challenge;const r=await request('/api/login',{challenge,code});assert.equal(r.status,200);return r.json().session;};
  async function connect(ticket){const ws=new WebSocket(`ws://127.0.0.1:${port}/ws`,{headers:{Host:new URL(origin).host,Origin:origin},perMessageDeflate:false});sockets.push(ws);await once(ws,'open');const message=Promise.race([once(ws,'message').then(([b])=>JSON.parse(b)),once(ws,'close').then(()=>null)]);ws.send(JSON.stringify({ticket}));const ready=await message;ws.epoch=ready?.data.epoch;return {ws,ready};}
  const ticket=async s=>(await request('/api/ticket',{},s)).json().ticket;
  let f,w,r;
  await t.test('HTTP origin/route/credential/schema restrictions and headers',async()=>{
    const entry=await request('/?frame_id=untrusted&instance_id=untrusted');assert.equal(entry.status,200);assert(entry.headers['content-security-policy'].includes("default-src 'none'"));assert(!entry.text.includes(backendSecret));
    assert.equal((await request('/api/challenge',{},null,{Origin:'https://evil.invalid'})).status,403);
    assert.equal((await request('/api/ticket',{})).status,403);
    assert.equal((await request('/api/login','{"challenge":{},"code":"x"}')).status,403);
    for(const path of ['/etc/passwd','/../play.js','/api/reset','/play.js?secret=x','/ws?ticket=x'])assert.equal((await request(path)).status,403);
  });
  await t.test('three independently verified fixture identities; no client identity claims',async()=>{
    f=await login('Fighter');w=await login('Wizard');r=await login('Rogue');assert.equal(new Set([f,w,r]).size,3);
    const challenge=(await request('/api/challenge',{})).json().challenge;
    assert.equal((await request('/api/login',{challenge,code:'Unused',user_id:ids[0]})).status,403);
    assert.equal((await request('/api/login',{challenge,code:'Stranger'})).status,403);
  });
  let fighter,used;
  const call=async(ws,q)=>{const answer=once(ws,'message');ws.send(JSON.stringify({...q,epoch:ws.epoch}));return JSON.parse((await answer)[0]);};
  await t.test('first player claims actual runtime; second player cannot read/control it',async()=>{
    used=await ticket(f);fighter=await connect(used);assert.equal(fighter.ready.type,'ready');
    assert((await call(fighter.ws,{v:1,id:'1',type:'play'})).ok);
    const screen=await call(fighter.ws,{v:1,id:'2',type:'screen',since:'0',offset:0});assert(screen.ok&&screen.data.encoding==='rgb565-patch-rle1');
    const wizard=await connect(await ticket(w));assert(wizard.ready);
    const denied=await call(wizard.ws,{v:1,id:'1',type:'play'});assert.equal(denied.error.code,'RuntimeOccupied');
    assert(!(await call(wizard.ws,{v:1,id:'2',type:'screen',since:'0',offset:0})).ok);wizard.ws.terminate();
    const rogue=await connect(await ticket(r));assert(rogue.ready);
    assert(!(await call(rogue.ws,{v:1,id:'1',type:'input',action:'a',down:true})).ok);rogue.ws.terminate();
  });
  await t.test('ticket replay and duplicate live ownership are rejected',async()=>{
    assert.equal((await connect(used)).ready,null);
    assert.equal((await connect(await ticket(f))).ready,null);
  });
  await t.test('replayed command closes connection without applying twice',async()=>{
    assert((await call(fighter.ws,{v:1,id:'3',type:'input',action:'a',down:true})).ok);
    const closed=once(fighter.ws,'close');fighter.ws.send(JSON.stringify({epoch:fighter.ws.epoch,v:1,id:'3',type:'input',action:'a',down:true}));await closed;await delay(120);
  });
  await t.test('a packet from an old socket cannot replay into a new epoch',async()=>{
    const c=await connect(await ticket(f));assert(c.ready&&c.ws.epoch!==fighter.ws.epoch);
    const closed=once(c.ws,'close');c.ws.send(JSON.stringify({epoch:fighter.ws.epoch,v:1,id:'1',type:'input',action:'a',down:true}));await closed;await delay(120);
  });
  await t.test('correct identity reconnects; forged ownership fields and binary/nesting rejected',async()=>{
    for(const payload of [epoch=>JSON.stringify({epoch,v:1,id:'1',type:'input',action:'a',down:true,player_id:0}),epoch=>JSON.stringify({epoch,v:1,id:'1',type:'screen',since:{},offset:0}),()=>Buffer.from([1,2]),()=>'x'.repeat(4097)]){
      const c=await connect(await ticket(f));assert(c.ready);const closed=once(c.ws,'close');c.ws.send(payload(c.ws.epoch));await closed;await delay(120);
    }
  });
  await t.test('WS handshake rejects foreign/null/missing Origin and alternate Host',async()=>{
    for(const headers of [{Host:new URL(origin).host,Origin:'https://evil.invalid'},{Host:new URL(origin).host,Origin:'null'},{Host:new URL(origin).host},{Host:'evil.invalid',Origin:origin}]){
      const ws=new WebSocket(`ws://127.0.0.1:${port}/ws`,{headers,handshakeTimeout:1000});ws.on('error',()=>{});await once(ws,'error');
    }
  });
  await t.test('per-identity rate limit survives new sockets',async()=>{
    // Clear the prior one-second window, then exceed it using real wire calls.
    await delay(1050);const c=await connect(await ticket(f));assert(c.ready);
    for(let i=1;i<=16;i++)assert((await call(c.ws,{v:1,id:String(i),type:'ping'})).ok);
    const closed=once(c.ws,'close');c.ws.send(JSON.stringify({epoch:c.ws.epoch,v:1,id:'17',type:'ping'}));await closed;await delay(80);
    const again=await connect(await ticket(f));assert(again.ready);const denied=once(again.ws,'close');again.ws.send(JSON.stringify({epoch:again.ws.epoch,v:1,id:'1',type:'ping'}));await denied;
  });
  await t.test('logout invalidates tickets and sessions; no state without auth',async()=>{
    const old=await ticket(w);assert.equal((await request('/api/logout',{},w)).status,200);assert.equal((await connect(old)).ready,null);
    assert.equal((await request('/api/ticket',{},w)).status,403);
    assert.equal((await connect(token())).ready,null);
    assert(!JSON.stringify(logs).includes(ids[0]));assert(!JSON.stringify(logs).includes(f));
    const rogue=await connect(await ticket(r));assert(rogue.ready);const closed=once(rogue.ws,'close');assert.equal((await request('/api/logout',{},r)).status,200);await closed;
  });
});
