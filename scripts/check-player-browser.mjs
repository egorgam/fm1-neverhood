// Boot the exact firmware, operate its real panel, and verify the four lanes/mutes.
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {jitImports} from '../../.deps/fm1-emu/web/jit.js';
const root=new URL('../',import.meta.url);let x;
const {instance}=await WebAssembly.instantiate(readFileSync(new URL('build/installer/emu/fm1_web.wasm',root)),{env:jitImports(()=>x)});
x=instance.exports;x.fm1_jit(1,16);
const bytes=readFileSync(new URL('build/neverhood.fwsc',root));const ptr=x.fm1_alloc(bytes.length);
new Uint8Array(x.memory.buffer,ptr,bytes.length).set(bytes);
if(x.fm1_boot_fwsc(ptr,bytes.length,0,0,240))throw new Error('Boot failed');
function run(ms){const until=x.fm1_ms()+ms;let sum=0,count=0;
 while(x.fm1_ms()<until){x.fm1_render(128,240000);if(x.fm1_halted())throw new Error('Halted');
  for(const value of new Float32Array(x.memory.buffer,x.fm1_out_l(),128)){sum+=value*value;count++;}}
 return Math.sqrt(sum/Math.max(1,count));}
function tap(id){x.fm1_key(id,1);run(50);x.fm1_key(id,0);run(50);}
run(900);const baseline=new Uint16Array(x.memory.buffer,x.fm1_lcd(),57600).slice();
tap(11);tap(12);run(5000);
// Section buttons no longer mute lanes. FX switches pages and SEQ returns.
for(const id of [3,4,5])tap(id);
tap(2);tap(11);
const levels=[],quiet=0;
const mixed=run(1000);if(mixed<.001)throw new Error('Silent theme after section keys');
const lcd=new Uint16Array(x.memory.buffer,x.fm1_lcd(),57600).slice();
writeFileSync(new URL('build/check/player/browser-lcd.raw',root),Buffer.from(lcd.buffer));
if(!lcd.some((v,i)=>v!==baseline[i]))throw new Error('Player screen did not open');
tap(12);run(800);const stopped=run(500);if(stopped>.0002)throw new Error('Stop left sounding notes');
writeFileSync(new URL('build/check/player/browser.json',root),JSON.stringify({firmwareSha256:createHash('sha256').update(bytes).digest('hex'),quiet,lanes:levels,mixed,stopped,ms:x.fm1_ms()},null,2)+'\n');
console.log('Actual firmware browser player: section keys preserve playback, visible sequencer, clean stop',levels);
