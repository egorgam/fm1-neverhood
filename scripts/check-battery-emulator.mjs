// Exercise the real SARADC -> platform cache -> app -> LCD path in the firmware.
import {readFileSync,writeFileSync} from 'node:fs';
import {jitImports} from '../../.deps/fm1-emu/web/jit.js';
const root=new URL('../',import.meta.url);
let x;
const {instance}=await WebAssembly.instantiate(readFileSync(new URL('build/installer/emu/fm1_web.wasm',root)),{env:jitImports(()=>x)});
x=instance.exports;x.fm1_jit(1,16);
const bytes=readFileSync(new URL('build/neverhood.fwsc',root));
const ptr=x.fm1_alloc(bytes.length);new Uint8Array(x.memory.buffer,ptr,bytes.length).set(bytes);
if(x.fm1_boot_fwsc(ptr,bytes.length,0,0,240)) throw new Error('Boot failed');
function runUntil(ms) {
  let n=0;
  while(x.fm1_ms()<ms) {
    x.fm1_render(128,240000);
    if(x.fm1_halted() || ++n>150000) throw new Error('Firmware halted/timed out');
  }
}
function lcd(){return new Uint16Array(x.memory.buffer,x.fm1_lcd(),57600).slice();}
runUntil(1800);
const report=[];
for(const page of ["HOME","FX","SEQ"]){
 if(page!=="HOME"){const id=page==="FX"?2:11;x.fm1_key(id,1);runUntil(x.fm1_ms()+60);x.fm1_key(id,0);}
 x.fm1_analog(3,640);runUntil(x.fm1_ms()+1800);
 const full=lcd();
for(const [raw,level] of [[640,3],[570,2],[540,1],[510,0],[640,3]]) {
  x.fm1_analog(3,raw);runUntil(x.fm1_ms()+1800);
  const frame=lcd();let changes=0;
  for(let i=0;i<frame.length;i++) if(frame[i]!==full[i]) {
    const a=i%240,b=Math.floor(i/240);
    if(a<223||a>231||b<55||b>97) throw new Error(`Unexpected pixel change ${a},${b}`);
    changes++;
  }
  for(let hole=0;hole<3;hole++) {
    const index=(92-hole*16)*240+227;
    if((frame[index]===full[index])!==(hole<level)) throw new Error(`Wrong battery hole: ADC ${raw}, hole ${hole}`);
  }
  report.push({page,raw,level,changedPixels:changes,ms:x.fm1_ms()});
}
}
writeFileSync(new URL('build/check/battery/emulator.json',root),JSON.stringify(report,null,2)+'\n');
console.log('Actual firmware battery: HOME/FX/SEQ: ADC 3 updates 3/2/1/0/3 holes, no other pixels changed');
