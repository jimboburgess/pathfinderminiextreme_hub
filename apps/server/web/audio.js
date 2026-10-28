"use strict";
(() => {
  const duties=new Set([128,256,512,768]);
  const integer=(value,max)=>Number.isInteger(value)&&value>=0&&value<=max;
  function sequence(value) {
    if(typeof value!=="string"||!/^(0|[1-9][0-9]{0,19})$/.test(value))throw new Error("Invalid audio cursor");
    const result=BigInt(value);if(result>18446744073709551615n)throw new Error("Invalid audio cursor");return result;
  }
  class PfmeAudio {
    constructor(context=null) {
      this.context=context;this.master=null;this.enabled=false;this.unlocked=false;this.paused=true;
      this.needsReset=true;this.cursor=0n;this.serverOrigin=0;this.audioOrigin=0;this.lookahead=.12;
      this.generation=0;this.nodes=new Set();this.current=null;this.currentDuty=512;this.waves=new Map();
      if(context)this.initialize(context);
    }
    initialize(context) {
      this.context=context;this.master=context.createGain();this.master.gain.value=.06;this.master.connect(context.destination);
    }
    enable(value) {this.enabled=!!value;this.needsReset=true;if(!value)this.pause();}
    async unlock() {
      if(this.unlocked&&!this.paused&&this.context?.state==="running")return true;
      const generation=this.generation;
      try {
        if(!this.context) {
          const Constructor=window.AudioContext||window.webkitAudioContext;if(!Constructor)return false;
          this.initialize(new Constructor({latencyHint:"interactive"}));
        }
        await this.context.resume();
        if(generation!==this.generation)return false;
        this.unlocked=this.context.state==="running";this.paused=!this.unlocked;this.needsReset=true;
        return this.unlocked;
      } catch {this.unlocked=false;return false;}
    }
    stopNodes() {
      for(const node of this.nodes) {try{node.stop();}catch{}node.disconnect();}
      this.nodes.clear();this.current=null;
    }
    pause() {
      ++this.generation;this.paused=true;this.needsReset=true;this.stopNodes();
      if(this.context?.state==="running")this.context.suspend().catch(()=>{});
    }
    wave(duty) {
      if(this.waves.has(duty))return this.waves.get(duty);
      const real=new Float32Array(65),imaginary=new Float32Array(65),fraction=duty/1024;
      // A band-limited, zero-mean pulse with the original LEDC duty. The C++
      // interpreter already supplied all note/modulation/noise frequencies.
      for(let harmonic=1;harmonic<65;harmonic++) {
        const angle=2*Math.PI*harmonic*fraction;
        real[harmonic]=2*Math.sin(angle)/(Math.PI*harmonic);
        imaginary[harmonic]=2*(1-Math.cos(angle))/(Math.PI*harmonic);
      }
      const wave=this.context.createPeriodicWave(real,imaginary);this.waves.set(duty,wave);return wave;
    }
    event(frequency,duty,when) {
      if(!frequency) {
        if(this.current){this.current.stop(when);this.current=null;}return;
      }
      if(!this.current||this.currentDuty!==duty) {
        if(this.current)this.current.stop(when);
        if(this.nodes.size>=32)throw new Error("Audio scheduling limit");
        const node=this.context.createOscillator();node.setPeriodicWave(this.wave(duty));node.connect(this.master);
        node.frequency.setValueAtTime(frequency,when);node.start(when);
        this.nodes.add(node);node.onended=()=>{node.disconnect();this.nodes.delete(node);if(this.current===node)this.current=null;};
        this.current=node;this.currentDuty=duty;
      } else this.current.frequency.setValueAtTime(frequency,when);
    }
    reset(packet,cursor) {
      this.stopNodes();this.cursor=cursor;this.serverOrigin=packet.now_ms;
      this.audioOrigin=this.context.currentTime+this.lookahead;this.needsReset=false;
      if(packet.frequency)this.event(packet.frequency,packet.duty,this.audioOrigin);
    }
    apply(packet) {
      if(!packet||typeof packet!=="object"||Object.keys(packet).length!==6||
         !integer(packet.now_ms,4294967295)||!integer(packet.frequency,65535)||!duties.has(packet.duty)||
         typeof packet.reset!=="boolean"||!Array.isArray(packet.events)||packet.events.length>32)throw new Error("Invalid audio packet");
      const cursor=sequence(packet.cursor);let previous=-1n;
      for(const event of packet.events) {
        if(!Array.isArray(event)||event.length!==4)throw new Error("Invalid audio event");
        const id=sequence(event[0]);
        if(id===0n||id<=previous||id>cursor||!integer(event[1],4294967295)||!integer(event[2],65535)||!duties.has(event[3])||
           ((event[1]-packet.now_ms)|0)>0)throw new Error("Invalid audio event");
        previous=id;
      }
      if((packet.reset&&packet.events.length)||(!packet.reset&&packet.events.length&&previous!==cursor))throw new Error("Invalid audio sequence");
      if(!this.enabled||!this.unlocked||this.paused||!this.context||this.context.state!=="running") {this.needsReset=true;return;}
      const mappedNow=this.audioOrigin+((packet.now_ms-this.serverOrigin)>>>0)/1000;
      if(packet.reset||this.needsReset||mappedNow<this.context.currentTime-.04||mappedNow>this.context.currentTime+.45) {
        this.reset(packet,cursor);return;
      }
      if(cursor<this.cursor)throw new Error("Stale audio cursor");
      const events=packet.events.filter(event=>sequence(event[0])>this.cursor);
      if(events.some(event=>this.audioOrigin+((event[1]-this.serverOrigin)|0)/1000<this.context.currentTime-.02)) {
        this.reset(packet,cursor);return;
      }
      if(events.some(event=>this.audioOrigin+((event[1]-this.serverOrigin)|0)/1000>this.context.currentTime+.5)) {this.reset(packet,cursor);return;}
      for(const event of events) {
        const when=Math.max(this.context.currentTime+.003,this.audioOrigin+((event[1]-this.serverOrigin)|0)/1000);
        this.event(event[2],event[3],when);
      }
      this.cursor=cursor;
    }
  }
  window.PfmeAudio=PfmeAudio;
})();
