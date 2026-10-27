import https from 'node:https';
import {Denied,requireThat} from './policy.mjs';
// Fixed Discord endpoint only; no redirects, arbitrary URLs, proxy environment,
// response bodies in errors, or credential logging. TLS verification is default.
function discord(path, method, headers, body='') {
  return new Promise((resolve,reject)=>{
    const req=https.request({hostname:'discord.com',port:443,path,method,headers,
      timeout:4000,maxHeaderSize:8192},res=>{
      let size=0;const chunks=[];
      res.on('data',chunk=>{size+=chunk.length;if(size>32768){res.destroy();reject(new Denied('IdentityUnavailable'));}else chunks.push(chunk);});
      res.on('error',()=>reject(new Denied('IdentityUnavailable')));
      res.on('end',()=>{try {requireThat(res.statusCode>=200&&res.statusCode<300,'IdentityUnavailable');resolve(size?JSON.parse(Buffer.concat(chunks).toString('utf8')):{});}catch {reject(new Denied('IdentityUnavailable'));}});
    });
    // Absolute deadline as well as inactivity timeout: a slow trickle must not
    // reserve an OAuth operation indefinitely.
    const deadline=setTimeout(()=>req.destroy(new Error('deadline')),4000);
    req.on('close',()=>clearTimeout(deadline));
    req.on('timeout',()=>req.destroy());req.on('error',()=>reject(new Denied('IdentityUnavailable')));req.end(body);
  });
}
export class DiscordOAuth {
  constructor(clientId,clientSecret,request=discord){this.clientId=clientId;this.clientSecret=clientSecret;this.request=request;}
  async form(path, values) {const body=new URLSearchParams({client_id:this.clientId,client_secret:this.clientSecret,...values}).toString();return this.request(path,'POST',{'Content-Type':'application/x-www-form-urlencoded','Content-Length':Buffer.byteLength(body)},body);}
  async verify(accessToken) {
    const headers={Authorization:`Bearer ${accessToken}`};
    const auth=await this.request('/api/v10/oauth2/@me','GET',headers);
    requireThat(auth.application?.id===this.clientId&&Array.isArray(auth.scopes)&&auth.scopes.includes('identify')&&Date.parse(auth.expires)>Date.now(),'InvalidIdentity');
    const user=await this.request('/api/v10/users/@me','GET',headers);
    requireThat(typeof user.id==='string'&&/^[0-9]{17,20}$/.test(user.id),'InvalidIdentity');
    return {id:user.id, expires:Date.parse(auth.expires)};
  }
  async exchange(code) {
    const value=await this.form('/api/oauth2/token',{grant_type:'authorization_code',code});
    requireThat(value.token_type?.toLowerCase()==='bearer'&&typeof value.access_token==='string'&&value.access_token.length<=2048&&Number.isFinite(value.expires_in)&&value.expires_in>0,'InvalidIdentity');
    const identity=await this.verify(value.access_token);
    // Refresh tokens are not retained. A new authorization is needed at expiry.
    return {...identity,accessToken:value.access_token,expires:Math.min(identity.expires,Date.now()+value.expires_in*1000)};
  }
  async revoke(accessToken) {await this.form('/api/oauth2/token/revoke',{token:accessToken,token_type_hint:'access_token'});}
}
