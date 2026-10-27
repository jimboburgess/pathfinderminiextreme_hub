import http from 'node:http';
import {readFile} from 'node:fs/promises';
import {WebSocketServer,WebSocket} from 'ws';
import {Auth} from './auth.mjs';
import {Upstream} from './upstream.mjs';
import {Budget,Denied,requireThat,fields,flatJson,gameCommand,token} from './policy.mjs';

const assets=new Map([['/activity',['index.html','text/html; charset=utf-8']],['/activity.js',['activity.js','text/javascript; charset=utf-8']],['/play.js',['play.js','text/javascript; charset=utf-8']],['/play.css',['play.css','text/css; charset=utf-8']]]);
// In-memory creation is exported for bounded local tests. The CLI has no fake
// identity, arbitrary upstream, insecure Origin or remote bind switches.
export async function createGateway(config,{oauth,log=event=>console.log(JSON.stringify({event}))}) {
  const sink=log;let logWindow=Date.now(),logCount=0;
  log=event=>{if(Date.now()-logWindow>=60000){logWindow=Date.now();logCount=0;}if(++logCount<=60)sink(event);};
  const {origin,clientId,backendPort,backendSecret}=config,url=new URL(origin);
  requireThat(url.origin===origin&&clientId&&Number.isInteger(backendPort)&&backendPort>1024&&backendPort<=65535&&typeof backendSecret==='string'&&/^[0-9a-f]{64}$/.test(backendSecret),'Configuration');
  const auth=new Auth({oauth,invited:new Set(config.invited),log});
  const staticFiles=new Map();for(const[path,[name,type]]of assets)staticFiles.set(path,{body:await readFile(new URL(`./dist/${name}`,import.meta.url)),type});
  const httpBudget=new Budget(120,60000),connectBudget=new Budget(24,60000);
  let authCalls=0,closing=false;
  const headerOK=req=>{
    for(const name of ['host','origin','authorization'])requireThat(req.rawHeaders.filter((_,i)=>i%2===0&&req.rawHeaders[i].toLowerCase()===name).length<=1,'InvalidRequest');
    requireThat(req.headers.host===url.host,'OriginDenied');
    if(req.headers.origin!==undefined)requireThat(req.headers.origin===origin,'OriginDenied');
  };
  const headers={'Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Referrer-Policy':'no-referrer',
    'Content-Security-Policy':`default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self' ${origin.replace(/^http/,'ws')}; frame-ancestors https://discord.com https://ptb.discord.com https://canary.discord.com; base-uri 'none'; form-action 'none'; object-src 'none'`,
    'Permissions-Policy':'camera=(), microphone=(), geolocation=()'};
  function reply(res,status,body,type='application/json'){res.writeHead(status,{...headers,'Content-Type':type});res.end(typeof body==='string'||Buffer.isBuffer(body)?body:JSON.stringify(body));}
  async function body(req){requireThat(req.headers['content-type']==='application/json','InvalidRequest');requireThat(!req.headers['content-encoding'],'InvalidRequest');let size=0;const chunks=[];for await(const c of req){size+=c.length;requireThat(size<=4096,'InvalidRequest');chunks.push(c);}return flatJson(Buffer.concat(chunks).toString('utf8'));}
  function bearer(req){const v=req.headers.authorization;requireThat(typeof v==='string'&&/^Bearer [0-9a-f]{64}$/.test(v),'AuthenticationRequired');return v.slice(7);}
  const server=http.createServer({maxHeaderSize:8192},async(req,res)=>{
    let reserved=false;
    try {
      headerOK(req);httpBudget.take();requireThat(!closing,'Unavailable');
      // Discord appends launch query parameters to the entry document. They are
      // consumed only by the SDK, never used as server identity or routing data.
      const entry=req.url.length<=2048&&(/^\/(?:activity)?(?:\?|$)/).test(req.url);
      if(req.method==='GET'&&(entry||assets.has(req.url))){const a=staticFiles.get(entry?'/activity':req.url);reply(res,200,a.body,a.type);return;}
      if(req.method==='GET'&&req.url==='/api/config'){reply(res,200,{client_id:clientId,mode:'single-controller-preview'});return;}
      requireThat(req.method==='POST'&&req.headers.origin===origin,'OriginDenied');
      requireThat(['/api/challenge','/api/login','/api/ticket','/api/logout'].includes(req.url),'RouteDenied');
      requireThat(authCalls<4,'Capacity');authCalls++;reserved=true;
      const q=await body(req);
      if(req.url==='/api/login'){const result=await auth.login(q);reply(res,200,result);}
      else {fields(q,[]);
        if(req.url==='/api/challenge')reply(res,200,{challenge:auth.challenge()});
        if(req.url==='/api/ticket')reply(res,200,{ticket:await auth.ticket(bearer(req))});
        if(req.url==='/api/logout'){await auth.logout(bearer(req));reply(res,200,{ok:true});}
      }
    } catch(error){log(error instanceof Denied?'http_rejected':'http_failure');if(!res.headersSent)reply(res,error.code==='RateLimited'?429:403,{error:error instanceof Denied?error.code:'Unavailable'});else res.destroy();}
    finally{if(reserved)authCalls--;}
  });
  server.maxConnections=16;server.headersTimeout=5000;server.requestTimeout=5000;server.timeout=10000;server.keepAliveTimeout=1000;server.maxRequestsPerSocket=32;
  server.on('clientError',(_,socket)=>socket.destroy());
  const wss=new WebSocketServer({noServer:true,maxPayload:4096,perMessageDeflate:false});
  server.on('upgrade',(req,socket,head)=>{
    try {headerOK(req);connectBudget.take();requireThat(!closing&&req.url==='/ws'&&req.headers.origin===origin&&wss.clients.size<8&&!head.length,'OriginDenied');wss.handleUpgrade(req,socket,head,ws=>wss.emit('connection',ws));}
    catch {log('upgrade_rejected');socket.destroy();}
  });
  wss.on('connection',ws=>{
    const epoch=token();
    let session=null,state=null,upstream=null,busy=false,last=0,alive=true,closed=false,ownsLock=false;
    const authDeadline=setTimeout(()=>ws.terminate(),5000),pingBudget=new Budget(40);
    ws.on('error',()=>{});
    ws.on('pong',()=>{try{pingBudget.take();alive=true;}catch{ws.terminate();}});
    ws.on('ping',()=>{try{pingBudget.take();}catch{ws.terminate();}});
    const close=()=>ws.terminate();
    function send(data){const text=JSON.stringify(data);requireThat(Buffer.byteLength(text)<=16384&&ws.bufferedAmount<=16384,'Backpressure');ws.send(text,err=>{if(err)close();});}
    ws.on('message',async(data,binary)=>{
      if(busy){log('pipeline_rejected');close();return;}busy=true;
      try {
        requireThat(!binary,'TextRequired');
        if(!session){const q=flatJson(data.toString());fields(q,['ticket']);session=await auth.consume(q.ticket);requireThat(ws.readyState===WebSocket.OPEN,'Unavailable');
          state=auth.identities.get(session.identity.id);requireThat(!state.active,'AlreadyConnected');state.active=true;ownsLock=true;
          session.sockets.add(ws);upstream=new Upstream(backendPort,backendSecret);await upstream.open(session.identity.id,state);
          requireThat(ws.readyState===WebSocket.OPEN,'Unavailable');await auth.validate(session);clearTimeout(authDeadline);
          upstream.onclose=close;send({v:1,id:'',type:'ready',ok:true,data:{mode:'single-controller-preview',epoch}});log('socket_authenticated');
        }else{
          state.budget.take();await auth.validate(session);const q=gameCommand(data.toString(),last,epoch);last=Number(q.id);
          const {v,id,type,epoch:ignored,...args}=q;const result=await upstream.call(type,args);
          // Never forward backend credentials or arbitrary backend commands.
          requireThat(ws.readyState===WebSocket.OPEN,'Unavailable');await auth.validate(session);
          send({...result,id});
        }
      }catch(error){log(error instanceof Denied?'socket_rejected':'socket_failure');close();}
      finally{busy=false;if(closed)cleanup();}
    });
    function cleanup(){upstream?.close();if(ownsLock){state.active=false;ownsLock=false;}}
    ws.on('close',()=>{closed=true;clearTimeout(authDeadline);session?.sockets.delete(ws);upstream?.close();if(!busy)cleanup();});
    ws.heartbeat=async()=>{try{if(!alive)return close();alive=false;ws.ping();if(session)await auth.validate(session);}catch{close();}};
  });
  const sweep=setInterval(()=>{auth.prune();for(const ws of wss.clients)void ws.heartbeat();},5000);sweep.unref();
  return {server,auth,async listen(port=0){await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(port,'127.0.0.1',()=>{server.removeListener('error',reject);resolve();});});return server.address().port;},
    async close(){closing=true;clearInterval(sweep);auth.clear();for(const ws of wss.clients)ws.terminate();server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}};
}
