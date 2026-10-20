// Actual device code: edit a note, save to NOR, restore flash into a fresh CPU and compare the grid.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {jitImports} from '../../.deps/fm1-emu/web/jit.js';
import {changedSectors,packSectors} from '../../.deps/fm1-emu/web/persistence.js';
const root=new URL('../',import.meta.url),bytes=readFileSync(new URL('build/neverhood.fwsc',root));
const mod=await WebAssembly.compile(readFileSync(new URL('build/installer/emu/fm1_web.wasm',root)));
async function boot(sectors=[]){let x;x=(await WebAssembly.instantiate(mod,{env:jitImports(()=>x)})).exports;x.fm1_jit(1,16);
 const copy=b=>{let p=x.fm1_alloc(b.length);new Uint8Array(x.memory.buffer,p,b.length).set(b);return p;};
 assert.equal(x.fm1_boot_fwsc(copy(bytes),bytes.length,0,0,240),0);
 if(sectors.length){let p=packSectors(sectors);assert.equal(x.fm1_flash_restore(copy(p),p.length),0);}
 const run=ms=>{let end=x.fm1_ms()+ms;while(x.fm1_ms()<end){x.fm1_render(128,240000);assert.equal(x.fm1_halted(),0);}};
 const tap=id=>{x.fm1_key(id,1);run(60);x.fm1_key(id,0);run(60);};
 const turn=(id,n)=>{x.fm1_enc(id,n);run(100);};
 const flash=()=>new Uint8Array(x.memory.buffer,x.fm1_flash(),x.fm1_flash_len()).slice();
 const lcd=()=>new Uint16Array(x.memory.buffer,x.fm1_lcd(),57600).slice();
 run(800);return {x,run,tap,turn,flash,lcd};}
let a=await boot(),initial=a.flash();a.tap(11);const pristine=a.lcd();a.tap(2);assert.notDeepEqual(grid(a.lcd()),grid(pristine));a.tap(11);assert.deepEqual(grid(a.lcd()),grid(pristine));a.tap(6);a.turn(3,1);a.turn(4,3);a.turn(6,1);a.turn(2,1);a.turn(3,1);a.turn(2,-1);a.tap(9);a.run(500);
const edited=a.lcd(),saved=a.flash(),delta=changedSectors(initial,saved);
assert(delta.length>=4);assert(delta.every(s=>s.at>=0xD0000&&s.at<0xD8000),'Save wrote outside its own A/B sectors');
let b=await boot(delta);b.tap(11);const restored=b.lcd();
function grid(p){const out=[];for(let y=45;y<139;y++)for(let x=17;x<217;x++)out.push(p[y*240+x]);return out;}
assert.notDeepEqual(grid(edited),grid(pristine));assert.deepEqual(grid(restored),grid(edited));
// Save twice to exercise both A and B copies of each lane, keeping calibration intact.
b.turn(3,1);b.tap(9);b.run(500);const twice=b.flash();const changes=changedSectors(initial,twice);
assert(changes.every(s=>s.at>=0xD0000&&s.at<0xD8000));assert(changes.length===8);
b.tap(10);b.tap(12);b.run(6000);b.tap(12);b.run(200);
writeFileSync(new URL('build/check/player/sequencer-browser.json',root),JSON.stringify({firmwareSha256:createHash('sha256').update(bytes).digest('hex'),sectors:changes.map(s=>s.at.toString(16)),saveReloadGrid:true,halted:b.x.fm1_halted()},null,2)+'\n');
console.log('Actual firmware: note/gate/instrument edit, A/B save, fresh CPU reload, matching grid, bounded flash writes, loop playback passed');
