import {Budget,Denied,requireThat,token,digest,fields} from './policy.mjs';
export class Auth {
  constructor({oauth,invited,now=()=>Date.now(),log=()=>{}}){
    requireThat(invited instanceof Set&&invited.size>0&&invited.size<=8,'Configuration');
    this.oauth=oauth;this.invited=invited;this.now=now;this.log=log;this.challenges=new Map();this.sessions=new Map();this.tickets=new Map();
    this.identities=new Map();this.loginBudget=new Budget(8,60000,now);
  }
  prune(){const now=this.now();for(const [k,v]of this.challenges)if(v<=now)this.challenges.delete(k);
    for(const [k,v]of this.tickets)if(v.until<=now)this.tickets.delete(k);
    for(const [k,v]of this.sessions)if(v.until<=now)this.remove(k,'expired');}
  challenge(){this.prune();requireThat(this.challenges.size<16,'Capacity');const nonce=token();this.challenges.set(digest(nonce),this.now()+60000);return nonce;}
  async login(q){fields(q,['challenge','code']);requireThat(typeof q.challenge==='string'&&/^[0-9a-f]{64}$/.test(q.challenge)&&typeof q.code==='string'&&/^[\x21-\x7e]{1,2048}$/.test(q.code),'InvalidRequest');
    this.loginBudget.take();this.prune();const key=digest(q.challenge);requireThat(this.challenges.has(key),'ChallengeRequired');this.challenges.delete(key);
    const identity=await this.oauth.exchange(q.code);
    if(!this.invited.has(identity.id)){await this.oauth.revoke(identity.accessToken).catch(()=>{});throw new Denied('NotInvited');}
    // One active session per verified Discord identity. A fresh verified login
    // invalidates its previous sockets, but retains that identity's own player.
    for(const [k,v]of this.sessions)if(v.identity.id===identity.id)this.remove(k,'replaced');
    requireThat(this.sessions.size<8,'Capacity');
    const secret=token(),session={identity,until:Math.min(identity.expires,this.now()+900000),checked:this.now(),sockets:new Set(),checking:null};
    requireThat(session.until>this.now(),'Expired');this.sessions.set(digest(secret),session);
    if(!this.identities.has(identity.id))this.identities.set(identity.id,{budget:new Budget(16,1000,this.now),proof:'',active:false});
    this.log('login_ok');return {session:secret,access_token:identity.accessToken};
  }
  get(secret){this.prune();requireThat(typeof secret==='string'&&/^[0-9a-f]{64}$/.test(secret),'AuthenticationRequired');const s=this.sessions.get(digest(secret));requireThat(s&&this.invited.has(s.identity.id),'AuthenticationRequired');return s;}
  async validate(s){requireThat(s.until>this.now()&&this.invited.has(s.identity.id)&&[...this.sessions.values()].includes(s),'AuthenticationRequired');
    if(this.now()-s.checked>=30000){if(!s.checking)s.checking=this.oauth.verify(s.identity.accessToken).then(v=>{requireThat(v.id===s.identity.id&&v.expires>this.now(),'InvalidIdentity');s.checked=this.now();}).finally(()=>{s.checking=null;});
      try {await s.checking;}catch{for(const[k,v]of this.sessions)if(v===s)this.remove(k,'revoked');throw new Denied('AuthenticationRequired');}}
    // Logout/replacement/expiry may happen while the trusted API call awaited.
    requireThat(s.until>this.now()&&this.invited.has(s.identity.id)&&[...this.sessions.values()].includes(s),'AuthenticationRequired');
  }
  async ticket(secret){const s=this.get(secret);await this.validate(s);this.prune();requireThat(this.tickets.size<16,'Capacity');const t=token();this.tickets.set(digest(t),{session:s,until:this.now()+10000});return t;}
  async consume(t){requireThat(typeof t==='string'&&/^[0-9a-f]{64}$/.test(t),'AuthenticationRequired');this.prune();const key=digest(t),v=this.tickets.get(key);this.tickets.delete(key);requireThat(v,'AuthenticationRequired');await this.validate(v.session);return v.session;}
  remove(key,event){const s=this.sessions.get(key);if(!s)return;this.sessions.delete(key);for(const[k,v]of this.tickets)if(v.session===s)this.tickets.delete(k);for(const ws of s.sockets)ws.terminate();this.log(event);}
  async logout(secret){const s=this.get(secret);this.remove(digest(secret),'logout');await this.oauth.revoke(s.identity.accessToken).catch(()=>this.log('revoke_unavailable'));}
  clear(){for(const k of this.sessions.keys())this.remove(k,'shutdown');this.challenges.clear();this.tickets.clear();}
}
