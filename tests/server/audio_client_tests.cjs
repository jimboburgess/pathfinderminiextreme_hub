const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
class Context {
  constructor(){this.currentTime=0;this.state='running';this.destination={};this.created=[];this.waveforms=[];}
  createGain(){return {gain:{value:0},connect(){}};}
  createPeriodicWave(real,imaginary){const wave={real:Array.from(real),imaginary:Array.from(imaginary)};this.waveforms.push(wave);return wave;}
  createOscillator(){const context=this;const node={frequency:{events:[],setValueAtTime(value,time){this.events.push([value,time]);}},
    connect(){},disconnect(){this.disconnected=true;},setPeriodicWave(wave){this.wave=wave;},start(time){this.started=time;},stop(time=context.currentTime){this.stopped=time;}};
    this.created.push(node);return node;}
  resume(){this.state='running';return Promise.resolve();}
  suspend(){this.state='suspended';return Promise.resolve();}
  advance(time){this.currentTime=time;for(const node of this.created)if(node.stopped!==undefined&&node.stopped<=time&&!node.ended){node.ended=true;node.onended?.();}}
}
const sandbox={window:{},Float32Array,Set,Map,BigInt,Number,Math,Object,Array,Error};
vm.createContext(sandbox);vm.runInContext(fs.readFileSync(process.argv[2],'utf8'),sandbox);
const Driver=sandbox.window.PfmeAudio;
const packet=(cursor,now,events=[],frequency=0,duty=512,reset=false)=>({cursor:String(cursor),now_ms:now,frequency,duty,reset,events});
async function main(){
 const racingContext=new Context();let finishResume;
 racingContext.resume=()=>new Promise(resolve=>{finishResume=resolve;});
 const racing=new Driver(racingContext);racing.enable(true);const pending=racing.unlock();
 racing.pause();finishResume();assert.equal(await pending,false);assert.equal(racing.paused,true,'lost focus wins over pending resume');
 const context=new Context(),audio=new Driver(context);audio.enable(true);await audio.unlock();
 audio.apply(packet(0,1000,[],0,512,true));
 context.advance(.05);audio.apply(packet(2,1050,[['1',1010,1200,256],['2',1035,0,256]],0,256));
 assert.equal(context.created.length,1);const first=context.created[0];
 assert(Math.abs(first.started-.130)<1e-6);assert(Math.abs(first.stopped-.155)<1e-6);
 assert.equal(first.frequency.events[0][0],1200);
 for(const duty of [128,256,512,768])audio.wave(duty);
 assert.equal(context.waveforms.length,4,'four cached original pulse duties');
 const quarter=audio.wave(256);assert(Math.abs(quarter.real[1]-2/Math.PI)<1e-6);
 assert(Math.abs(quarter.imaginary[1]-2/Math.PI)<1e-6);
 const half=audio.wave(512);assert(Math.abs(half.real[1])<1e-6);assert(Math.abs(half.imaginary[1]-4/Math.PI)<1e-6);
 context.advance(.2);assert.equal(audio.nodes.size,0);
 audio.apply(packet(100,1200,[],2200,512,true));assert.equal(audio.nodes.size,1);
 assert.equal(context.created.at(-1).frequency.events[0][0],2200);
 audio.pause();assert.equal(audio.nodes.size,0);
 audio.apply(packet(101,1220,[['101',1210,1100,128]],1100,128));assert.equal(audio.nodes.size,0,'hidden tab stays silent');
 await audio.unlock();audio.apply(packet(102,1300,[['102',1270,1300,256]],0,256));
 assert.equal(audio.nodes.size,0,'resume resynchronizes current silence instead of replaying history');
 assert.throws(()=>audio.apply(packet(101,1310)),/Stale audio cursor/);
 const invalid=[null,{},packet(103,4294967296),packet(103,1320,[],NaN),packet(103,1320,[],1000,1024),
   packet(103,1320,[['0',1310,1000,512]]),packet(103,1320,[['103',1310,1000,512],['103',1311,0,512]]),
   packet(103,1320,[['103',1400,1000,512]]),packet(104,1320,[['103',1310,1000,512]]),
   packet(103,1320,[['103',1310,1000,512]],1000,512,true),{...packet(103,1320),unexpected:true},
   packet(103,1320,Array.from({length:33},(_,i)=>[String(103+i),1310,1000,512]))];
 for(const bad of invalid)assert.throws(()=>audio.apply(bad),/Invalid/);
 const wrappedContext=new Context(),wrapped=new Driver(wrappedContext);wrapped.enable(true);await wrapped.unlock();
 wrapped.apply(packet(0,4294967294,[],0,512,true));
 wrapped.apply(packet(2,1,[['1',4294967295,600,128],['2',1,0,128]],0,128));
 assert(wrappedContext.created[0].started<.2&&wrappedContext.created[0].stopped<.2,'timestamp wrap never schedules years ahead');
 audio.stopNodes();let bounded=false;
 try {for(let i=0;i<40;++i){audio.event(1000,512,.3+i*.001);audio.event(0,512,.3005+i*.001);}}
 catch(error){bounded=/scheduling limit/.test(error.message);}
 assert(bounded&&audio.nodes.size<=32);audio.pause();assert.equal(audio.nodes.size,0);
 console.log('Web Audio: original pulse duties, exact note/pause scheduling, bounded nodes, malformed packets, wrap, silence, resync and no stale replay passed');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
