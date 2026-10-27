import {randomBytes, createHash} from 'node:crypto';
export const token = () => randomBytes(32).toString('hex');
export const digest = value => createHash('sha256').update(value).digest('hex');
export class Denied extends Error { constructor(code='Denied') { super(code); this.code=code; } }
export const requireThat = (condition, code='Denied') => { if (!condition) throw new Denied(code); };
export function fields(value, names) {
  requireThat(value && Object.getPrototypeOf(value)===Object.prototype &&
    Object.keys(value).length===names.length && names.every(k=>Object.hasOwn(value,k)), 'InvalidRequest');
}
// The Activity wire format is deliberately flat. Reject nested values and duplicate
// decoded keys before JSON.parse can silently choose a last value.
export function flatJson(text) {
  requireThat(typeof text==='string' && Buffer.byteLength(text)<=4096, 'InvalidRequest');
  const string='"(?:[^"\\\\\\x00-\\x1f]|\\\\(?:["\\\\/bfnrt]|u[0-9a-fA-F]{4}))*"';
  const pair=new RegExp(`\\s*(${string})\\s*:\\s*(${string}|true|false|null|-?(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)\\s*`, 'y');
  let at=0; const skip=()=>{while(/\s/.test(text[at]??'') && at<text.length)at++;};
  skip();requireThat(text[at++]==='{','InvalidRequest');skip();const keys=new Set();
  if(text[at]!=='}')for(;;){pair.lastIndex=at;const m=pair.exec(text);requireThat(m,'InvalidRequest');
    const key=JSON.parse(m[1]);requireThat(!keys.has(key),'InvalidRequest');keys.add(key);at=pair.lastIndex;
    if(text[at]!==',')break;at++;
  }
  requireThat(text[at++]==='}','InvalidRequest');skip();requireThat(at===text.length,'InvalidRequest');
  return JSON.parse(text);
}
export class Budget {
  constructor(limit, interval=1000, now=()=>Date.now()) {this.limit=limit;this.interval=interval;this.now=now;this.start=now();this.count=0;}
  take() {const n=this.now();if(n-this.start>=this.interval){this.start=n;this.count=0;}requireThat(++this.count<=this.limit,'RateLimited');}
}
export function gameCommand(text, last, epoch) {
  const q=flatJson(text);
  requireThat(typeof epoch==='string'&&q.epoch===epoch,'SequenceRequired');
  requireThat(q.v===1 && typeof q.id==='string' && /^[1-9][0-9]{0,14}$/.test(q.id) && Number(q.id)===last+1, 'SequenceRequired');
  if(q.type==='play'||q.type==='ping')fields(q,['v','id','type','epoch']);
  else if(q.type==='input') {fields(q,['v','id','type','epoch','action','down']);requireThat(['cw','ccw','encoder','a','b','release'].includes(q.action)&&typeof q.down==='boolean','InvalidRequest');}
  else if(q.type==='screen') {fields(q,['v','id','type','epoch','since','offset']);requireThat(typeof q.since==='string'&&/^(0|[1-9][0-9]{0,19})$/.test(q.since)&&BigInt(q.since)<=18446744073709551615n&&Number.isInteger(q.offset)&&q.offset>=0&&q.offset<=230400,'InvalidRequest');}
  else throw new Denied('CommandDenied');
  return q;
}
