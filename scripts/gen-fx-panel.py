#!/usr/bin/env python3
"""Pack the generated purple clay plaque, retaining its transparent silhouette."""
import argparse
from pathlib import Path
from PIL import Image, ImageOps
from palette_art import palette_image, c_array, report
from lz_art import c_packed

parser = argparse.ArgumentParser()
parser.add_argument('--out', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = Image.open(root/'assets/fx-panel-v2-source.png').convert('RGBA')
# Scale the sculpted edge uniformly to height, then extend only the middle.
# This is the UI's three-slice layout: the side rims retain their proportions.
width, height, rim = 228, 124, 24
scaled = source.resize((round(source.width*height/source.height), height), Image.Resampling.LANCZOS)
panel = Image.new('RGBA', (width, height))
panel.paste(scaled.crop((0,0,rim,height)), (0,0))
panel.paste(scaled.crop((scaled.width-rim,0,scaled.width,height)), (width-rim,0))
center = scaled.crop((rim,0,scaled.width-rim,height))
panel.paste(center.resize((width-2*rim,height), Image.Resampling.LANCZOS), (rim,0))
panel.save(root/'assets/fx-panel.png')
arrow = Image.open(root/'assets/slider-arrow-source.png').convert('RGBA')
bounds = arrow.getchannel('A').point(lambda a: 255 if a > 16 else 0).getbbox()
arrow = ImageOps.contain(arrow.crop(bounds), (8,12), Image.Resampling.LANCZOS)
thumb = Image.new('RGBA',(8,12))
thumb.paste(arrow, ((8-arrow.width)//2,(12-arrow.height)//2))
thumb.save(root/'assets/slider-arrow.png')
lines = ['/* Generated clay panel and side arrows; see the prompts in assets. */', '#pragma once']
for prefix, art in [('FX',panel),('ARROW',thumb)]:
    lines += [f'#define {prefix}_WIDTH {art.width}',f'#define {prefix}_HEIGHT {art.height}']
    if prefix == 'FX':
        indices, colors, alpha, preview = palette_image(art, rgba=True)
        lines += c_packed('FX_PIXELS', indices) + c_array('FX_PALETTE', colors, 'uint16_t') + c_array('FX_ALPHA', alpha)
        report(root, 'fx-panel', art, preview, art.width*art.height*5//2, len(indices)+3*len(colors))
        continue
    colors, alpha = [], []
    for r,g,b,a in art.get_flattened_data():
        c = (r>>3)<<11 | (g>>2)<<5 | b>>3
        colors.append((c>>8) | (c&255)<<8)
        alpha.append(min(15,(a+8)//17))
    packed = [(alpha[i]<<4) | alpha[i+1] for i in range(0,len(alpha),2)]
    for name,kind,values in [(prefix+'_PIXELS','uint16_t',colors),(prefix+'_ALPHA','uint8_t',packed)]:
        lines.append(f'static const {kind} {name}[] = {{')
        lines += ['    '+','.join(map(str,values[i:i+24]))+',' for i in range(0,len(values),24)]
        lines.append('};')
    print(f'{prefix}: {art.width}x{art.height} RGB565/alpha4, {2*len(colors)+len(packed)} B')
args.out.parent.mkdir(parents=True,exist_ok=True)
args.out.write_text('\n'.join(lines)+'\n')
