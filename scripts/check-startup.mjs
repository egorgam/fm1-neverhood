// Verify the first complete LCD frame of the actual firmware is the clay room, not a plain splash.
import {readFileSync,writeFileSync} from 'node:fs';
import {jitImports} from '../../.deps/fm1-emu/web/jit.js';
const root=new URL('../',import.meta.url);
let x;
const {instance}=await WebAssembly.instantiate(readFileSync(new URL('build/installer/emu/fm1_web.wasm',root)),{env:jitImports(()=>x)});
x=instance.exports;x.fm1_jit(1,16);
const bytes=readFileSync(new URL('build/neverhood.fwsc',root));
const ptr=x.fm1_alloc(bytes.length);new Uint8Array(x.memory.buffer,ptr,bytes.length).set(bytes);
if(x.fm1_boot_fwsc(ptr,bytes.length,0,0,240)) throw new Error('Boot failed');
let first;
for(let i=0;i<2000&&!first;i++) {
  x.fm1_render(128,240000);
  const pixels=new Uint16Array(x.memory.buffer,x.fm1_lcd(),57600);
  const counts=new Map();let lit=0;
  for(const color of pixels) if(color) {lit++;counts.set(color,(counts.get(color)||0)+1);}
  if(lit>57000) {
    first={ms:x.fm1_ms(),colors:counts.size,dominantFraction:Math.max(...counts.values())/57600};
    if(counts.size<100||first.dominantFraction>.5) throw new Error('First complete frame is a flat splash: '+JSON.stringify(first));
    writeFileSync(new URL('build/check/startup-lcd.raw',root),Buffer.from(pixels.buffer,pixels.byteOffset,pixels.byteLength));
  }
  if(x.fm1_halted()) throw new Error('Firmware halted');
}
if(!first) throw new Error('No complete startup frame');
writeFileSync(new URL('build/check/startup.json',root),JSON.stringify(first,null,2)+'\n');
console.log('First complete frame is the clay room:',first);
