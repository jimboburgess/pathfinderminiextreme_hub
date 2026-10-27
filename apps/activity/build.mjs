import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {build} from 'esbuild';
const out=new URL('./dist/',import.meta.url),source=new URL('../server/web/',import.meta.url);
await mkdir(out,{recursive:true});
let html=await readFile(new URL('play.html',source),'utf8');
html=html.replace(/<form id="connect">[\s\S]*?<\/form>/,'<form id="connect"><button>Sign in with Discord</button></form><button id="logout">Log out</button>');
html=html.replace('<script src="/play.js"></script>','<script type="module" src="/activity.js"></script>');
html=html.replace('Connect through your private SSH tunnel. One playable controller per server.','Discord preview: one playable controller per server. Multiplayer gameplay is not yet implemented.');
html=html.replace('Keep this tab open to retain its reconnect credential; credentials are never saved to browser storage.','Keep this tab open to retain its short-lived Discord session. Nothing is saved to browser storage.');
await writeFile(new URL('index.html',out),html);
await writeFile(new URL('play.css',out),await readFile(new URL('play.css',source)));
let js=await readFile(new URL('play.js',source),'utf8');
const before=js.indexOf('      const name=$("name").value;'),after=js.indexOf('      ws.onmessage=',before);
if(before<0||after<before)throw new Error('Original client authentication boundary changed; review adapter');
js=js.slice(0,before)+'      ws=await window.pfmeActivity.open(resume);id=0;\n'+js.slice(after);
const authStart=js.indexOf('      await rpc("authenticate"'),authEnd=js.indexOf('      await rpc("play")',authStart);
if(authStart<0||authEnd<authStart)throw new Error('Original client ownership boundary changed; review adapter');
js=js.slice(0,authStart)+js.slice(authEnd);
js=js.replace('JSON.stringify({v:1,id:requestId,type,...fields})','JSON.stringify({v:1,id:requestId,type,...fields,epoch:ws.pfmeEpoch})');
js=js.replaceAll('!proof','!window.pfmeActivity.hasSession()').replaceAll('!!proof','window.pfmeActivity.hasSession()');
// Authentication differs; pixel codec, rendering, controls, focus release, queue
// and zoom remain the original client source, with no second renderer.
await writeFile(new URL('play.js',out),js);
// SDK error branches otherwise print raw inbound payloads. Drop browser console
// calls and disable SDK console forwarding; do not log identity/token material.
await build({entryPoints:[new URL('./client.mjs',import.meta.url).pathname.replace(/^\/([A-Za-z]:)/,'$1')],outfile:new URL('activity.js',out).pathname.replace(/^\/([A-Za-z]:)/,'$1'),bundle:true,format:'esm',platform:'browser',external:['./play.js'],drop:['console'],minify:false,sourcemap:false});
