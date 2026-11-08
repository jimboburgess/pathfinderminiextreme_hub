"use strict";
(() => {
  const $=id=>document.getElementById(id), canvas=$("display"), ctx=canvas.getContext("2d",{alpha:false,willReadFrequently:true});
  const audio=window.PfmeAudio?new window.PfmeAudio():null;
  const accountMode=$("connect").dataset?.accountAuth==="true";
  if(accountMode)setInterval(()=>{fetch('/accounts/presence/',{credentials:'same-origin',cache:'no-store'}).then(r=>{if(r.status===401){ws?.close();status('Your login expired. Refresh to sign in again.');}}).catch(()=>{});},15000);
  const privateMode=$("connect").dataset?.authMode==="serve";
  if(privateMode){$("secret").required=false;$("secret").closest("label").hidden=true;}
  ctx.imageSmoothingEnabled=false;
  const image=ctx.createImageData(240,240);
  for(let i=3;i<image.data.length;i+=4) image.data[i]=255;
  let leaving=false, textInput=false;
  let ws=null, secret="", proof="", id=0, pending=null, playing=false, since="0", offset=0, chunks=[], total=0;
  let requests=[], held=new Set(), heldSources=new Map(), repeat=new Map(), scheduled=null, connecting=false, disconnectReason="";
  const status=text=>{$("status").textContent=text;};
  function fit() {return Math.max(1,Math.floor(Math.min($("viewport").clientWidth,innerHeight-$("viewport").getBoundingClientRect().top-20)/240));}
  function resize() {const s=$("scale").value==="fit"?fit():Number($("scale").value); canvas.style.width=`${240*s}px`; canvas.style.height=`${240*s}px`;}
  $("scale").value=String(Math.min(3,fit())); $("scale").onchange=resize; window.addEventListener("resize",resize); resize();
  function clearFrame() {
    // Start a new stream with a fresh backing store as well as black pixels.
    // This invalidates stale pixelated compositor tiles on reconnect.
    canvas.width=240;ctx.imageSmoothingEnabled=false;
    since="0";offset=0;chunks=[];total=0;image.data.fill(0);
    for(let i=3;i<image.data.length;i+=4)image.data[i]=255;
    ctx.putImageData(image,0,0);
  }
  function rpc(type,fields={}) {
    return new Promise((resolve,reject)=>{
      if(!ws || ws.readyState!==WebSocket.OPEN || pending) {reject(new Error("Connection not ready"));return;}
      const requestId=String(++id);
      const timeout=setTimeout(()=>{if(pending?.id===requestId){pending=null;reject(new Error("Server response timed out"));ws?.close();}},4000);
      pending={id:requestId,resolve,reject,timeout};
      ws.send(JSON.stringify({v:1,id:requestId,type,...fields}));
    });
  }
  function applyFrame(frame) {
    textInput=frame.text_input===true;
    if(frame.encoding!=="rgb565-patch-rle1" || frame.base!==since || frame.offset!==offset || !Number.isInteger(frame.total) || frame.total<0 || frame.total>230400 || typeof frame.more!=="boolean") throw new Error("Invalid frame sequence");
    const bytes=Uint8Array.from(atob(frame.pixels),c=>c.charCodeAt(0));
    if(bytes.length>9216 || (offset && frame.total!==total) || offset+bytes.length>frame.total) throw new Error("Invalid frame size");
    total=frame.total; chunks.push(bytes); offset+=bytes.length;
    if(frame.more) {if(!bytes.length || offset>=total) throw new Error("Invalid frame continuation");return;}
    if(offset!==total) throw new Error("Incomplete frame");
    const data=new Uint8Array(total);let p=0;for(const chunk of chunks){data.set(chunk,p);p+=chunk.length;}
    // Validate all ranges before presenting an atomic C++-rendered frame.
    const runs=[];
    for(p=0;p<data.length;){
      if(p+4>data.length)throw new Error("Invalid pixel run");
      const start=data[p]|data[p+1]<<8, encoded=data[p+2]|data[p+3]<<8, count=encoded&32767, repeated=!!(encoded&32768);p+=4;
      const size=repeated?2:count*2;
      if(!count || start+count>57600 || p+size>data.length)throw new Error("Invalid pixel range");
      runs.push([start,count,p,repeated]);p+=size;
    }
    for(const [start,count,at,repeated] of runs)for(let i=0;i<count;i++){
      const index=at+(repeated?0:i*2),value=data[index]|data[index+1]<<8,r=value>>11,g=value>>5&63,b=value&31,j=(start+i)*4;
      image.data[j]=(r<<3)|(r>>2);image.data[j+1]=(g<<2)|(g>>4);image.data[j+2]=(b<<3)|(b>>2);
    }
    if(total){
      // Present an immutable complete pixel buffer, rather than reusing the
      // mutable accumulator while a browser compositor may still consume it.
      const completed=ctx.createImageData(240,240);completed.data.set(image.data);
      ctx.putImageData(completed,0,0);
    }
    since=frame.frame;offset=0;total=0;chunks=[];
  }
  async function pump() {
    if(!playing||leaving)return;
    try {
      if(requests.length){const event=requests.shift();await rpc("input",event);}
      else applyFrame(await rpc("screen",{since,offset}));
    }catch(error){disconnectReason=error.message;status(disconnectReason);playing=false;releaseAll();ws?.close();return;}
    // At most ~14 requests/sec including controls, below the existing 20/sec cap.
    if(!leaving)scheduled=setTimeout(pump,75);
  }
  function enqueue(action,down) {
    if(!playing)return;
    if(requests.length>=8){releaseAll();return;}
    requests.push({action,down});
  }
  function press(action,source=action) {
    audio?.unlock();
    if(!playing || heldSources.has(source))return;
    heldSources.set(source,action);
    if(held.has(action))return;
    held.add(action);enqueue(action,true);
    if(!held.has(action))return;
    if(action==="cw" || action==="ccw") {
      const timer=setTimeout(()=>{repeat.set(action,setInterval(()=>{if(requests.length<2)enqueue(action,true);},220));},450);
      repeat.set(action,timer);
    }
  }
  function release(action,source=action) {
    if(heldSources.get(source)!==action)return;
    heldSources.delete(source);
    if([...heldSources.values()].includes(action)||!held.delete(action))return;
    clearTimeout(repeat.get(action));clearInterval(repeat.get(action));repeat.delete(action);
    if(action!=="cw"&&action!=="ccw")enqueue(action,false);
  }
  function releaseAll(){for(const timer of repeat.values()){clearTimeout(timer);clearInterval(timer);}repeat.clear();heldSources.clear();held.clear();requests=[];if(playing)enqueue("release",false);}
  function actionFor(key){return {q:"ccw",Q:"ccw",ArrowLeft:"ccw",w:"encoder",W:"encoder"," ":"encoder",e:"cw",E:"cw",ArrowRight:"cw",a:"a",A:"a",Enter:"a",d:"b",D:"b",Escape:"b",Backspace:"b"}[key];}
  function keySource(event){return "key:"+(event.code||event.key.toLowerCase());}
  document.addEventListener("keydown",event=>{if(/^(INPUT|SELECT|TEXTAREA|BUTTON)$/.test(event.target.tagName)||event.ctrlKey||event.metaKey||event.altKey)return;if(textInput&&playing&&event.key.length===1&&event.key!==" "){
    // Space always clicks the encoder, including character-name entry.
    // Unsupported text and key-repeat must not emit invalid/flooding RPCs.
    event.preventDefault();if(!event.repeat&&/^[A-Za-z0-9]$/.test(event.key))enqueue(`key:${event.key}`,true);return;
  }const a=actionFor(event.key);if(a&&playing){event.preventDefault();if(!event.repeat)press(a,keySource(event));}});
  document.addEventListener("keyup",event=>{const source=keySource(event),a=heldSources.get(source);if(a){release(a,source);if(playing)event.preventDefault();}});
  window.addEventListener("blur",()=>{releaseAll();audio?.pause();});document.addEventListener("visibilitychange",()=>{if(document.hidden){releaseAll();audio?.pause();}});
  for(const button of $("controls").querySelectorAll("button")){
    button.addEventListener("pointerdown",event=>{event.preventDefault();button.setPointerCapture(event.pointerId);press(button.dataset.action,"pointer:"+event.pointerId);});
    button.addEventListener("pointerup",event=>release(button.dataset.action,"pointer:"+event.pointerId));button.addEventListener("pointercancel",releaseAll);
  }
  async function connect(resume){
    if(connecting||playing)return;connecting=true;disconnectReason="";$("resume").disabled=true;
    try{
      const name=$("name").value;
      if(!resume){secret=$("secret").value.trim();proof="";}
      $("secret").value="";
      if((!privateMode&&!/^[0-9a-f]{64}$/.test(secret))||(!resume&&!/^[!-~]{1,15}$/.test(name)))throw new Error("Enter the 64-character development secret and a name without spaces.");
      ws=new WebSocket(`${location.protocol==="https:"?"wss":"ws"}://${location.host}/`);
      await new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=()=>reject(new Error("Connection failed"));});
      ws.onmessage=event=>{try{const reply=JSON.parse(event.data);if(!reply.id)return;if(!pending||reply.id!==pending.id)throw new Error("Unexpected response");if(reply.audio)audio?.apply(reply.audio);const p=pending;pending=null;clearTimeout(p.timeout);if(reply.ok)p.resolve(reply.data);else p.reject(new Error(reply.error?.code==="Full"?"Party Full: all four character slots are occupied. You remain signed in. Retry after someone uses Leave character; disconnect alone retains their slot.":reply.error?.code||"Server rejected command"));}catch(error){status(error.message);ws.close();}};
      ws.onclose=()=>{if($("leave"))$("leave").disabled=true;audio?.pause();playing=false;releaseAll();clearTimeout(scheduled);if(pending){clearTimeout(pending.timeout);pending.reject(new Error("Connection closed"));pending=null;}$("disconnect").disabled=true;$("resume").disabled=!proof;$("connect").querySelector("button").disabled=!!proof;status(disconnectReason||"Disconnected. The server retains this session in memory. Reconnect from this tab to resume.");};
      await rpc("authenticate",privateMode?{}:{secret});
      if(privateMode){
        try{await rpc("reconnect");}catch(error){if(error.message!=="NoCharacter")throw error;await rpc("join",{name});}
        proof="verified-identity";
      }else if(resume)await rpc("reconnect",{token:proof});else{const joined=await rpc("join",{name});proof=joined.reconnect_token;}
      const runtime=await rpc("play");audio?.enable(runtime.audio===true);
      $("runtime-note").textContent=(runtime.audio&&audio?"Original music and sound effects play locally after using a game control. ":"Sound is silent in this runtime. ")+"Saves remain in server memory. Other connected party members continue during a disconnect. Keep this tab open to retain its reconnect credential; it is never saved to browser storage.";
      if(privateMode)$("runtime-note").textContent="Verified private identity. Reconnect restores your character while this server process remains running. Authorization expires after 15 minutes; reconnect to verify again. Original sound starts with a game control.";
      if(accountMode)$("runtime-note").textContent="Signed in to your own Pathfinder account. Refresh or reconnect restores your character while this game process remains running. Disconnect retains your slot; Leave character releases it. Original audio starts with a game control.";
      clearFrame();requests=[];playing=true;$("disconnect").disabled=false;$("connect").querySelector("button").disabled=true;
      if($("leave"))$("leave").disabled=false;
      status("Connected. A / Enter: Button A; W / Space: encoder click; Q/E or Left/Right: rotate. Saves are in memory only.");canvas.focus();pump();
    }catch(error){disconnectReason=error.message;status(disconnectReason);ws?.close();}finally{connecting=false;}
  }
  $("connect").onsubmit=event=>{event.preventDefault();audio?.unlock();connect(false);};$("resume").onclick=()=>{audio?.unlock();connect(true);};
  if($("leave"))$("leave").onclick=async()=>{
    if(!playing||!window.confirm("Leave this character permanently for this server session?"))return;
    leaving=true;releaseAll();clearTimeout(scheduled);
    // Wait for the existing one-in-flight RPC to complete before Leave.
    while(pending)await new Promise(resolve=>setTimeout(resolve,20));
    try{await rpc("input",{action:"release",down:false});await rpc("leave");proof="";audio?.pause();playing=false;ws?.close(1000,"left character");$("leave").disabled=true;}
    catch(error){status(error.message);}
    finally{leaving=false;if(playing)pump();}
  };
  $("disconnect").onclick=()=>{audio?.pause();releaseAll();playing=false;clearTimeout(scheduled);ws?.close(1000,"user disconnect");};
})();
