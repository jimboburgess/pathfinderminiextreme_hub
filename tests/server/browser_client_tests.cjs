// Client logic only: fake DOM/timers/socket, no browser or networking.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const events={},elements={},sent=[],timers=new Map();
const textTest=process.argv.includes('--text');
const privateTest=process.argv.includes('--serve');
const fixture=process.argv[3]&&!privateTest&&!textTest?JSON.parse(fs.readFileSync(process.argv[3],'utf8')):null;
const expectedPixels=process.argv[4]&&!process.argv[4].startsWith('--')?fs.readFileSync(process.argv[4]):null;
let now=0,nextTimer=1,lastPixels,socket,characterExists=false,socketCount=0,failOpens=0;
const retryTest=process.argv.includes("--retry");
function element(id){return elements[id]??={value:'',style:{},disabled:false,tagName:'DIV',clientWidth:1200,
  addEventListener(type,fn){events[id+':'+type]=fn;},getBoundingClientRect(){return {top:200};},focus(){},
  closest(){return element(id+'Label');},querySelector(){return element(id+'Button');},querySelectorAll(){return [];}};}
if(privateTest)element('connect').dataset={authMode:'serve'};
element('secret').value='a'.repeat(64);element('name').value='Tester';
const canvas=element('display');canvas.width=canvas.height=240;canvas.tagName='CANVAS';
const context={createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:image=>{lastPixels=Uint8Array.from(image.data);}};
canvas.getContext=()=>context;
function timeout(fn,ms,interval=0){const id=nextTimer++;timers.set(id,{at:now+ms,fn,interval});return id;}
async function flush(){for(let i=0;i<25;i++)await Promise.resolve();}
async function advance(ms){const end=now+ms;while(true){const due=[...timers].filter(([,v])=>v.at<=end).sort((a,b)=>a[1].at-b[1].at)[0];if(!due)break;const [id,t]=due;now=t.at;if(t.interval)t.at+=t.interval;else timers.delete(id);t.fn();await flush();}now=end;await flush();}
// Independent codec fixture: a full blue surface split at the 15-bit run limit.
const patch=Buffer.alloc(12);patch.writeUInt16LE(0,0);patch.writeUInt16LE(0xffff,2);patch.writeUInt16LE(0x001f,4);
patch.writeUInt16LE(32767,6);patch.writeUInt16LE(0x8000|24833,8);patch.writeUInt16LE(0x001f,10);
class Socket {
  static OPEN=1;
  constructor(url){++socketCount;this.url=url;socket=this;this.readyState=1;this.stall=false;queueMicrotask(()=>{if(failOpens>0){--failOpens;this.onerror?.();}else this.onopen?.();});}
  send(text){const request=JSON.parse(text);sent.push(request);if(this.stall){this.blocked=request;return;}this.respond(request);}
  respond(request){let data={};
    if(privateTest&&request.type==='reconnect'&&!characterExists){queueMicrotask(()=>this.onmessage?.({data:JSON.stringify({v:1,id:request.id,ok:false,error:{code:'NoCharacter'}})}));return;}
    if(request.type==='join'){characterExists=true;data=privateTest?{}:{reconnect_token:'b'.repeat(64)};}
    if(request.type==='leave')characterExists=false;
    if(request.type==='screen')data={encoding:'rgb565-patch-rle1',base:request.since,frame:'1',offset:0,total:request.since==='0'?12:0,more:false,pixels:request.since==='0'?patch.toString('base64'):''};
    if(textTest&&data.encoding)data.text_input=true;
    if(request.type==='screen'&&fixture&&request.since==='0')data=fixture.find(p=>p.offset===request.offset);
    queueMicrotask(()=>this.onmessage?.({data:JSON.stringify({v:1,id:request.id,ok:true,data})}));
  }
  close(code=1000,reason=""){this.readyState=3;queueMicrotask(()=>this.onclose?.({code,reason}));}
}
const documentMock={hidden:false,getElementById:element,addEventListener(type,fn){events['document:'+type]=fn;}};
const sandbox={document:documentMock,window:{confirm:()=>true,addEventListener(type,fn){events['window:'+type]=fn;}},innerHeight:1100,location:privateTest?{protocol:'https:',host:'game.test.ts.net'}:{protocol:'http:',host:'127.0.0.1:18765'},
  Date:{now:()=>now},Math:Object.assign(Object.create(Math),{random:()=>.5}),WebSocket:Socket,Uint8Array,Uint8ClampedArray,atob:s=>Buffer.from(s,'base64').toString('binary'),
  setTimeout:(fn,ms)=>timeout(fn,ms),clearTimeout:id=>timers.delete(id),setInterval:(fn,ms)=>timeout(fn,ms,ms),clearInterval:id=>timers.delete(id)};
vm.createContext(sandbox);vm.runInContext(fs.readFileSync(process.argv[2],'utf8'),sandbox);
function key(type,key,target='CANVAS',repeat=false){let prevented=false;events['document:'+type]({key,repeat,target:{tagName:target},preventDefault(){prevented=true;}});return prevented;}
async function main(){
  element('connect').onsubmit({preventDefault(){}});await flush();
  await advance(1000);
  assert.match(element('status').textContent,/Connected/);assert.equal(element('secret').value,'');
  assert.equal(element('scale').value,'3');assert.equal(context.imageSmoothingEnabled,false);
  assert.equal(lastPixels.length,230400);
  function verifyPixels(){for(let i=0;i<lastPixels.length;i+=4){let expected=[0,0,255,255];if(expectedPixels){const v=expectedPixels.readUInt16LE(i/2),r=v>>11,g=v>>5&63,b=v&31;expected=[r<<3|r>>2,g<<2|g>>4,b<<3|b>>2,255];}assert.deepEqual(Array.from(lastPixels.subarray(i,i+4)),expected,`pixel ${i/4}`);}}
  verifyPixels();
  if(textTest){
    const begin=sent.length;
    key('keydown',' ');await advance(120);key('keyup',' ');await advance(250);
    assert(sent.slice(begin).some(r=>r.action==='encoder'&&r.down),'Space clicks the encoder even if text_input is true');
    assert(!sent.slice(begin).some(r=>r.action==='key: '),'never emit a whitespace-containing action');
    for(const char of ['!','\u00e9']){key('keydown',char);key('keyup',char);}await advance(250);
    assert(!sent.slice(begin).some(r=>r.action==='key:!'||r.action==='key:\u00e9'),'unsupported text ignored locally');
    key('keydown','q');await advance(150);assert(sent.some(r=>r.action==='key:q'),'letters still type into character name');
    assert.equal(socket.readyState,Socket.OPEN,'text controls preserve connection');
  }
  // Contract for the requested browser defaults, including case and aliases.
  const bindings=textTest?[['ArrowLeft','ccw'],[' ','encoder'],['ArrowRight','cw'],['Enter','a'],['Escape','b']]:
    [['q','ccw'],['Q','ccw'],['ArrowLeft','ccw'],['w','encoder'],['W','encoder'],[' ','encoder'],['e','cw'],['E','cw'],['ArrowRight','cw'],['a','a'],['A','a'],['Enter','a'],['d','b'],['D','b'],['Escape','b']];
  for(const [binding,action] of bindings){
    const start=sent.length;assert(key('keydown',binding));await advance(160);key('keyup',binding.toLowerCase());await advance(180);
    assert(sent.slice(start).some(r=>r.action===action&&r.down),binding+' sends '+action);
    if(action!=='cw'&&action!=='ccw')assert(sent.slice(start).some(r=>r.action===action&&!r.down),binding+' releases '+action);
  }
  for(const target of ['INPUT','SELECT','TEXTAREA','BUTTON']){
    const start=sent.length;for(const [binding] of bindings){assert(!key('keydown',binding,target));key('keyup',binding,target);}await advance(180);
    assert(!sent.slice(start).some(r=>r.action),'form controls retain normal keyboard input');
  }
  if(!textTest){
    for(const old of ['ArrowUp','ArrowDown','s','S'])assert(!key('keydown',old),'old binding removed: '+old);
    const start=sent.length;
    key('keydown','w');await advance(160);key('keydown',' ');await advance(800);key('keyup','w');await advance(160);
    assert.equal(sent.slice(start).filter(r=>r.action==='encoder'&&r.down).length,1,'two encoder aliases generate one held press');
    assert(!sent.slice(start).some(r=>r.action==='encoder'&&!r.down),'releasing one alias keeps the other held');
    key('keyup',' ');await advance(180);assert(sent.slice(start).some(r=>r.action==='encoder'&&!r.down),'last alias releases encoder');
  }else{
    for(const letter of ['w','e','a','d']){const start=sent.length;key('keydown',letter);await advance(160);key('keyup',letter);assert(sent.slice(start).some(r=>r.action==='key:'+letter),'name-entry letters override gameplay aliases');}
  }
  const initial=lastPixels;
  for(const scale of ['1','2','3','4','5','fit']){element('scale').value=scale;element('scale').onchange();assert.equal(canvas.width,240);assert.equal(canvas.height,240);const expected=scale==='fit'?3:Number(scale);assert.equal(canvas.style.width,`${expected*240}px`);assert.equal(lastPixels,initial);}
  assert.equal(key('keydown',' ','INPUT'),false);
  const encoderBefore=sent.filter(r=>r.action==='encoder'&&r.down).length;
  assert.equal(key('keydown',' '),true);await advance(100);
  assert.equal(sent.filter(r=>r.type==='input'&&r.action==='encoder'&&r.down).length,encoderBefore+1);
  key('keydown',' ','CANVAS',true);await advance(800);
  assert.equal(sent.filter(r=>r.type==='input'&&r.action==='encoder'&&r.down).length,encoderBefore+1,'holding/repeat does not send movement presses');
  key('keyup',' ');await advance(100);assert(sent.some(r=>r.type==='input'&&r.action==='encoder'&&!r.down));
  key('keydown','ArrowRight');await advance(1000);assert(sent.filter(r=>r.type==='input'&&r.action==='cw').length>1);
  events['window:blur']();await advance(200);const rotary=sent.filter(r=>r.type==='input'&&r.action==='cw').length;
  await advance(1000);assert.equal(sent.filter(r=>r.type==='input'&&r.action==='cw').length,rotary,'blur stops repeat');
  assert(sent.some(r=>r.type==='input'&&r.action==='release'));
  // A stalled response plus rapid inputs must collapse to release, never leave
  // a rotary interval running after its held-key state has been discarded.
  socket.stall=true;await advance(100);key('keydown','ArrowRight');
  for(let i=0;i<10;i++){key('keydown',' ');key('keyup',' ');}
  key('keyup','ArrowRight');socket.stall=false;socket.respond(socket.blocked);await flush();
  const sentBefore=sent.length;await advance(1800);
  assert.equal(sent.slice(sentBefore).filter(r=>r.type==='input'&&r.action==='cw').length,0,'overflow cancels held repeat');
  documentMock.hidden=true;events['document:visibilitychange']();await advance(100);
  element('disconnect').onclick();await flush();assert.equal(element('resume').disabled,false);assert.equal(element('connect').querySelector().disabled,true,'owned session cannot be replaced accidentally');
  element('resume').onclick();await flush();await advance(1000);
  assert.match(element('status').textContent,/Connected/);
  verifyPixels();
  if(retryTest){
    const first=socket,begin=sent.length;first.close(1006,"network interruption");await flush();
    assert.match(element('status').textContent,/reconnecting/);await advance(499);assert.equal(socket,first,'first retry waits 0.5 seconds');
    await advance(700);assert.notEqual(socket,first);assert.match(element('status').textContent,/Reconnected/);
    const recovered=sent.slice(begin);assert(recovered.some(r=>r.type==='reconnect'),'recovers existing ownership');assert(!recovered.some(r=>r.type==='join'),'never creates a replacement character');
    const count=socketCount;socket.close(4001,'replaced');await flush();await advance(6000);assert.equal(socketCount,count,'superseded tab must not fight for control');assert.match(element('status').textContent,/Replaced/);
    element('resume').onclick();await flush();await advance(500);assert.match(element('status').textContent,/Connected/);
    failOpens=5;const before=socketCount;socket.close(1006,'interruption');await flush();
    for(const [index,delay] of [500,1000,2000,4000,5000,5000].entries()){
      await advance(delay-1);assert.equal(socketCount,before+index,'bounded backoff '+delay);
      await advance(1);assert.equal(socketCount,before+index+1,'retry at '+delay);
    }
    await advance(500);assert.match(element('status').textContent,/Reconnected/);
    failOpens=100;socket.close(1006,'long interruption');await flush();await advance(130000);
    const stopped=socketCount;await advance(10000);assert.equal(socketCount,stopped,'retry stops after grace');
    assert.match(element('status').textContent,/grace ended/);failOpens=0;
    element('resume').onclick();await flush();await advance(500);assert.match(element('status').textContent,/Connected/);
    console.log('Automatic reconnect: deterministic first retry, ownership recovery, no Join replay and no duplicate-tab reconnect fight passed.');
  }
  if(privateTest){
    assert.equal(socket.url,'wss://game.test.ts.net/');assert.equal(element('secret').required,false);assert.equal(element('secret').closest().hidden,true);
    assert(sent.filter(r=>r.type==='authenticate').every(r=>!('secret' in r)&&!('identity' in r)));
    assert(sent.filter(r=>r.type==='reconnect').every(r=>!('token' in r)&&!('player_id' in r)));
    const leave=element('leave').onclick();await advance(1000);await leave;await flush();
    assert(sent.some(r=>r.type==='leave'));assert.equal(element('leave').disabled,true);assert.equal(element('connect').querySelector().disabled,false);
    console.log('Private client: WSS, no shared secret or bearer reconnect proof, identity recovery, verified-identity reconnect UI, and confirmed Leave passed.');
  }
  console.log('Playable client: exact RLE color decode, integer zoom, default mappings, form exclusion, held encoder click, focus release, bounded overflow/repeat and reconnect affordance passed.');
}
main().catch(e=>{console.error(e);process.exitCode=1;});
