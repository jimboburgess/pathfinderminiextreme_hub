"use strict";
(() => {
  const $=id=>document.getElementById(id), canvas=$("display"), ctx=canvas.getContext("2d",{alpha:false,willReadFrequently:true});
  const audio=window.PfmeAudio?new window.PfmeAudio():null;
  const accountMode=$("connect").dataset?.accountAuth==="true";
  if(accountMode)setInterval(()=>{fetch('/accounts/presence/',{credentials:'same-origin',cache:'no-store'}).then(r=>{if(r.status===401){wanted=false;clearTimeout(retryTimer);ws?.close();status('Your login expired. Sign in again.');}}).catch(()=>{});},15000);
  const privateMode=$("connect").dataset?.authMode==="serve";
  if(privateMode){$("secret").required=false;$("secret").closest("label").hidden=true;}
  ctx.imageSmoothingEnabled=false;
  const image=ctx.createImageData(240,240);
  for(let i=3;i<image.data.length;i+=4) image.data[i]=255;
  let leaving=false, interactiveFrames=false, lastSent=0, textInput=false, pumping=false, wanted=false, retryTimer=null, retryAttempt=0, lostAt=0, generation=0;
  const rpcTimes=[];
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
      const requestId=String(++id),target=ws,epoch=generation;
      const timeout=setTimeout(()=>{if(pending?.id===requestId&&target===ws&&epoch===generation){pending=null;reject(new Error("Server response timed out"));if(ws?.readyState===WebSocket.OPEN)ws.close(4000,"rpc_timeout");}},15000);
      pending={id:requestId,resolve,reject,timeout};
      lastSent=Date.now();rpcTimes.push(lastSent);while(rpcTimes.length&&lastSent-rpcTimes[0]>=1005)rpcTimes.shift();
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
  function wake(delay=0) {clearTimeout(scheduled);const epoch=generation;scheduled=setTimeout(()=>{if(epoch===generation)pump();},delay);}
  async function pump() {
    if(!playing||leaving||pumping||pending)return;
    // One in-flight request, no frame queue, and <=18 requests in any rolling
    // second INCLUDING controls and handshake. Preserve the server's 20/s cap.
    const now=Date.now();while(rpcTimes.length&&now-rpcTimes[0]>=1005)rpcTimes.shift();
    const wait=Math.max(0,lastSent+60-now,rpcTimes.length>=18?rpcTimes[0]+1005-now:0);
    if(wait){wake(wait);return;}
    const epoch=generation;pumping=true;
    try {
      if(interactiveFrames){const event=requests.shift();applyFrame(await rpc("interact",{since,offset,...(event||{})}));}
      else if(requests.length){const event=requests.shift();await rpc("input",event);}
      else applyFrame(await rpc("screen",{since,offset}));
    }catch(error){if(epoch!==generation)return;disconnectReason=error.message;status(disconnectReason);playing=false;releaseAll();ws?.close();return;}finally{if(epoch===generation)pumping=false;}
    if(!leaving)wake(interactiveFrames?Math.max(0,lastSent+(requests.length||offset?60:75)-Date.now()):75);
  }
  function enqueue(action,down) {
    if(!playing)return;
    if(requests.length>=8){releaseAll();return;}
    requests.push({action,down});
    if(interactiveFrames&&!pumping&&!pending)wake();
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
  const finalErrors=new Set(["NoCharacter","OwnershipRequired","AuthenticationFailed","IdentityRequired","ReplayRejected","ServerFull"]);
  function retry(){
    if(!wanted||connecting||playing||retryTimer)return;
    if(!lostAt)lostAt=Date.now();
    if(Date.now()-lostAt>=120000){wanted=false;disconnectReason="Reconnect grace ended. Connect again to load a saved character.";status(disconnectReason);return;}
    const delays=[500,1000,2000,4000,5000],base=delays[Math.min(retryAttempt++,4)];
    const delay=Math.round(base*(.9+Math.random()*.2));
    status("Connection lost â€” reconnecting...");clearTimeout(retryTimer);
    retryTimer=setTimeout(()=>{retryTimer=null;connect(true,true);},delay);
  }
  async function connect(resume,automatic=false){
    if(connecting||playing)return;connecting=true;disconnectReason="";$("resume").disabled=true;
    const attempt=++generation;let socket;
    if(!automatic){wanted=true;lostAt=0;retryAttempt=0;clearTimeout(retryTimer);}
    try{
      const name=$("name").value;
      if(!resume){secret=$("secret").value.trim();proof="";}
      $("secret").value="";
      if((!privateMode&&!/^[0-9a-f]{64}$/.test(secret))||(!resume&&!/^[!-~]{1,15}$/.test(name))){wanted=false;throw new Error("Enter the development secret and a name without spaces.");}
      socket=new WebSocket(`${location.protocol==="https:"?"wss":"ws"}://${location.host}/`);ws=socket;
      let rejectOpening=null;
      socket.onclose=event=>{
        if(rejectOpening){rejectOpening(new Error("Connection closed before opening"));rejectOpening=null;}
        if(ws!==socket||attempt!==generation)return;
        playing=false;pumping=false;audio?.pause();releaseAll();clearTimeout(scheduled);
        if(pending){clearTimeout(pending.timeout);pending.reject(new Error("Connection closed"));pending=null;}
        $("disconnect").disabled=true;if($("leave"))$("leave").disabled=true;$("resume").disabled=!proof;$("connect").querySelector("button").disabled=!!proof;
        if(event?.code===4001){wanted=false;disconnectReason="Replaced by another signed-in connection.";}
        if(event?.code===4401){wanted=false;disconnectReason="Your login expired. Sign in again.";}
        if(event?.code===4409){wanted=false;disconnectReason="Server is full.";}
        if(wanted){connecting=false;retry();}else status(disconnectReason||"Disconnected. Reconnect within 120 seconds to restore this character.");
      };
      await new Promise((resolve,reject)=>{rejectOpening=reject;socket.onopen=()=>{rejectOpening=null;resolve();};socket.onerror=()=>{rejectOpening=null;reject(new Error("Connection failed"));};});
      socket.onmessage=event=>{if(ws!==socket||attempt!==generation)return;try{
        const reply=JSON.parse(event.data);if(!reply.id)return;
        if(!pending||reply.id!==pending.id)throw new Error("Unexpected response");
        if(reply.audio)audio?.apply(reply.audio);const p=pending;pending=null;clearTimeout(p.timeout);
        if(reply.ok)p.resolve(reply.data);else{const error=new Error(reply.error?.code||"Server rejected command");error.code=reply.error?.code;p.reject(error);}
      }catch(error){disconnectReason=error.message;socket.close();}};
      id=0;await rpc("authenticate",privateMode?{}:{secret});
      if(privateMode){
        try{await rpc("reconnect");}catch(error){if(error.code!=="NoCharacter"||resume)throw error;await rpc("join",{name});}
        proof="verified-identity";
      }else if(resume)await rpc("reconnect",{token:proof});else{const joined=await rpc("join",{name});proof=joined.reconnect_token;}
      const runtime=await rpc("play");interactiveFrames=runtime.interactive_frames===true;audio?.enable(runtime.audio===true);
      $("runtime-note").textContent=(runtime.persistence==="account-character-slots"?"Three account-owned character saves are stored on the server. ":"This adventure lives in server memory. ")+"Temporary disconnects reserve your character for 120 seconds. Original audio starts with a game control.";
      // Fresh authoritative full frame/audio state; no client snapshot or held
      // controls are replayed. Keep the previous image visible until connected.
      clearFrame();requests=[];playing=true;$("disconnect").disabled=false;$("connect").querySelector("button").disabled=true;
      if($("leave"))$("leave").disabled=false;
      retryAttempt=0;lostAt=0;status(automatic?"Reconnected.":"Connected. A / Enter: Button A; W / Space: encoder click; Q/E or Left/Right: rotate.");canvas.focus();pump();
    }catch(error){
      if(attempt!==generation)return;
      if(finalErrors.has(error.code)){wanted=false;clearTimeout(retryTimer);if(error.code==="NoCharacter"){proof="";$("resume").disabled=true;$("connect").querySelector("button").disabled=false;}}
      disconnectReason=error.code==="ServerFull"?"Server is full.":error.code==="NoCharacter"?"The previous session ended. Connect to load a saved character.":error.message;
      status(disconnectReason);socket?.close();
    }finally{
      if(attempt===generation){connecting=false;if(wanted&&!playing&&!retryTimer)retry();}
    }
  }
  $("connect").onsubmit=event=>{event.preventDefault();audio?.unlock();connect(false);};$("resume").onclick=()=>{audio?.unlock();connect(true);};
  if($("leave"))$("leave").onclick=async()=>{
    if(!playing||!window.confirm("Leave this character permanently for this server session?"))return;
    wanted=false;clearTimeout(retryTimer);leaving=true;releaseAll();clearTimeout(scheduled);
    // Wait for the existing one-in-flight RPC to complete before Leave.
    while(pending)await new Promise(resolve=>setTimeout(resolve,20));
    try{await rpc("input",{action:"release",down:false});await rpc("leave");proof="";audio?.pause();playing=false;ws?.close(1000,"left character");$("leave").disabled=true;}
    catch(error){status(error.message);}
    finally{leaving=false;if(playing)pump();}
  };
  $("disconnect").onclick=()=>{wanted=false;clearTimeout(retryTimer);audio?.pause();releaseAll();playing=false;clearTimeout(scheduled);ws?.close(1000,"user disconnect");};
})();
