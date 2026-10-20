#!/usr/bin/env python3
"""Empty purple variant of the clay workshop, replacing the obsolete FX plaque."""
import argparse
from pathlib import Path
from PIL import Image
from palette_art import palette_image,c_array,report
from lz_art import c_packed
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];size=240
art=Image.open(r/'assets/menu-room-source.png').convert('RGB').resize((size,size),Image.Resampling.LANCZOS)
indices,colors,_,preview=palette_image(art,colors_count=64)
packed=c_packed('MENU_ROOM',indices)
# Full-resolution menu pixels use ordinary RAM; reverb occupies the pool instead.
packed[1]=packed[1].replace(' PLAT_POOL','')
lines=['#pragma once',f'#define MENU_SIZE {size}']+c_array('MENU_PALETTE',colors[:64],'uint16_t')+packed
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text('\n'.join(lines))
report(r,'menu-background',art,preview,size*size*2,len(indices)+len(colors)*2)
