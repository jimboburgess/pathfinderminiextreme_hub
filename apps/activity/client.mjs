import {DiscordSDK} from '@discord/embedded-app-sdk';
// Tokens live only in this closure; no cookies, local/session storage or URL tokens.
let session='',sdk=null;
const deadline=(promise,milliseconds)=>new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(new Error('Discord request timed out; retry or reopen the Activity.')),milliseconds);promise.then(value=>{clearTimeout(timer);resolve(value);},error=>{clearTimeout(timer);reject(error);});});
async function post(path,values={},authenticated=false){
  const headers={'Content-Type':'application/json'};if(authenticated)headers.Authorization=`Bearer ${session}`;
  const r=await fetch(path,{method:'POST',headers,body:JSON.stringify(values),cache:'no-store',credentials:'omit',signal:AbortSignal.timeout(10000)});
  if(!r.ok)throw new Error('Activity authentication unavailable or access denied.');return r.json();
}
async function logout(){const old=session;try{if(old)await post('/api/logout',{},true);}finally{session='';}}
window.pfmeActivity=Object.freeze({
  async open(resume){
    if(!resume||!session){
      const config=await fetch('/api/config',{cache:'no-store',credentials:'omit',signal:AbortSignal.timeout(10000)}).then(r=>{if(!r.ok)throw new Error('Configuration unavailable');return r.json();});
      sdk??=new DiscordSDK(config.client_id,{disableConsoleLogOverride:true});await deadline(sdk.ready(),5000);
      const {challenge}=await post('/api/challenge');
      const {code}=await deadline(sdk.commands.authorize({client_id:config.client_id,response_type:'code',state:challenge,prompt:'none',scope:['identify']}),55000);
      const result=await post('/api/login',{challenge,code});session=result.session;
      try{const authenticated=await deadline(sdk.commands.authenticate({access_token:result.access_token}),8000);if(!authenticated)throw new Error('Discord authentication failed');}
      catch(error){await logout();throw error;}
    }
    const {ticket}=await post('/api/ticket',{},true);
    const socket=new WebSocket(`${location.protocol==='https:'?'wss':'ws'}://${location.host}/ws`);
    await new Promise((resolve,reject)=>{
      const timer=setTimeout(()=>{socket.close();reject(new Error('Activity connection timed out'));},5000);
      socket.onopen=()=>socket.send(JSON.stringify({ticket}));
      socket.onerror=()=>{clearTimeout(timer);reject(new Error('Activity connection failed'));};
      socket.onclose=()=>{clearTimeout(timer);reject(new Error('Activity access denied'));};
      socket.onmessage=event=>{try{const value=JSON.parse(event.data);if(value.type!=='ready'||!value.ok||!/^[0-9a-f]{64}$/.test(value.data?.epoch))throw new Error();socket.pfmeEpoch=value.data.epoch;clearTimeout(timer);resolve();}catch{clearTimeout(timer);socket.close();reject(new Error('Activity protocol failed'));}};
    });
    return socket;
  },logout,hasSession:()=>!!session
});
document.getElementById('logout').onclick=async()=>{document.getElementById('disconnect').click();try{await logout();}catch{}location.reload();};
await import('./play.js');
