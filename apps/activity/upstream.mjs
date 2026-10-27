import WebSocket from 'ws';
import {Denied,digest,requireThat} from './policy.mjs';
export class Upstream {
  constructor(port,secret){requireThat(Number.isInteger(port)&&port>1024&&port<=65535&&/^[0-9a-f]{64}$/.test(secret),'Configuration');this.port=port;this.secret=secret;this.serial=0;this.pending=null;this.socket=null;}
  async open(identity,state){
    const ws=this.socket=new WebSocket(`ws://127.0.0.1:${this.port}/`,{perMessageDeflate:false,maxPayload:16384,handshakeTimeout:3000});
    ws.on('error',()=>{});
    await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(new Denied('BackendUnavailable')),4000);ws.once('message',data=>{clearTimeout(timer);try{const hello=JSON.parse(data);requireThat(hello.type==='hello');resolve();}catch{reject(new Denied('BackendUnavailable'));}});ws.once('error',()=>{clearTimeout(timer);reject(new Denied('BackendUnavailable'));});ws.once('close',()=>{clearTimeout(timer);reject(new Denied('BackendUnavailable'));});});
    ws.on('message',data=>{if(!this.pending)return;const p=this.pending;this.pending=null;clearTimeout(p.timer);try{const r=JSON.parse(data);requireThat(r.id===p.id);p.resolve(r);}catch{p.reject(new Denied('BackendUnavailable'));}});
    ws.on('close',()=>{if(this.pending){clearTimeout(this.pending.timer);this.pending.reject(new Denied('BackendUnavailable'));this.pending=null;}this.onclose?.();});
    await this.checked('authenticate',{secret:this.secret});
    if(state.proof)await this.checked('reconnect',{token:state.proof});
    else {const r=await this.checked('join',{name:'P'+digest(identity).slice(0,10)});state.proof=r.reconnect_token;}
  }
  call(type,fields={}){return new Promise((resolve,reject)=>{if(this.pending||this.socket?.readyState!==WebSocket.OPEN)return reject(new Denied('BackendUnavailable'));
    const id=String(++this.serial),timer=setTimeout(()=>{this.pending=null;this.close();reject(new Denied('BackendUnavailable'));},4000);
    this.pending={id,resolve,reject,timer};this.socket.send(JSON.stringify({v:1,id,type,...fields}));});}
  async checked(type,fields={}){const r=await this.call(type,fields);requireThat(r.ok,r.error?.code==='RuntimeOccupied'?'RuntimeOccupied':'BackendUnavailable');return r.data;}
  close(){this.socket?.terminate();}
}
