import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {jitImports} from '../../.deps/fm1-emu/web/jit.js';
const root=new URL('../',import.meta.url);let x;
x=(await WebAssembly.instantiate(readFileSync(new URL('build/installer/emu/fm1_web.wasm',root)),{env:jitImports(()=>x)})).instance.exports;x.fm1_jit(1,16);
const bytes=readFileSync(new URL('build/neverhood.fwsc',root)),ptr=x.fm1_alloc(bytes.length);new Uint8Array(x.memory.buffer,ptr,bytes.length).set(bytes);assert.equal(x.fm1_boot_fwsc(ptr,bytes.length,0,0,240),0);
function run(ms){let end=x.fm1_ms()+ms;while(x.fm1_ms()<end){x.fm1_render(128,240000);assert.equal(x.fm1_halted(),0);}}
function tap(id){x.fm1_key(id,1);run(40);x.fm1_key(id,0);run(40);}
function notes(){x.fm1_leds();run(40);return [...new Float32Array(x.memory.buffer,x.fm1_leds(),41)].slice(14).map((v,k)=>v>.001?k:-1).filter(k=>k>=0);}
run(800);tap(11);tap(12);run(650);
assert.deepEqual(notes(),[18]); // held bass note71, base53
x.fm1_enc(1,1);run(40);assert.notDeepEqual(notes(),[18]); // now the melody track
// Navigation out of SEQ removes automatic note lights.
tap(2);assert.deepEqual(notes(),[]);
tap(11);tap(12);assert.deepEqual(notes(),[]);
console.log('Actual firmware LEDs: selected bass note, track change, FX exit and stop passed');
