#!/usr/bin/env python3
"""Check the compiled DEFLATE decoder against the complete pre-pack indexed art."""
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/check/art-lossless'
names=('CLAY_ROOM','MENU_ROOM','KLW_DATA')
code=['#include <stdint.h>','#include <stdio.h>','#define PLAT_POOL',
      '#include "clay_room.h"','#include "menu_room.h"','#include "klaymen_art.h"','#include "art_unpack.h"',
      'int main(void) { FILE *f;']
for name in names:
    args=f'{name}, sizeof {name}, {name}_PACKED, sizeof {name}_PACKED'
    code += [f'if(!art_unpack({args})) return 1;',
             f'f=fopen("{name}.decoded","wb"); if(!f) return 2; fwrite({name},1,sizeof {name},f); fclose(f);',
             f'if(art_unpack({name},sizeof {name},{name}_PACKED,1)) return 3;']
code+=['return 0; }']
OUT.mkdir(parents=True,exist_ok=True)
src=OUT/'check.c';src.write_text('\n'.join(code))
exe=OUT/'check'
subprocess.run(['cc','-O2','-fsanitize=address,undefined','-I'+str(ROOT),'-I'+str(ROOT/'build/gen'),str(src),'-o',str(exe)],check=True)
subprocess.run([str(exe)],cwd=OUT,check=True)
for name in names: assert (OUT/(name+'.raw')).read_bytes()==(OUT/(name+'.decoded')).read_bytes(),name
print('Lossless art: every pixel/span byte matches before compression; C ASan/UBSan and truncated stream checks passed')
