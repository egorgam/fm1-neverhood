#!/usr/bin/env python3
"""Verify routed effects move only their pictured source and restore it cleanly."""
from pathlib import Path
import json,subprocess
from PIL import Image,ImageChops
R=Path(__file__).resolve().parents[1];O=R/'build/check/effects-animation';O.mkdir(parents=True,exist_ok=True)
samples=json.loads((R/'assets/effects-kit/manifest.json').read_text())['samples']
boxes=[(19,65,61,116),(62,61,118,86),(110,75,166,114),(54,100,87,126),(202,99,222,118),(163,59,209,115)]
commands=['wait 700']
for route in range(6):
 s=max((s for s in samples if s['included'] and s['visual']==route and (route!=2 or s['short_label']=='INSERT TAPE')),key=lambda s:s['seconds'])
 commands+=['tap PLAY',f'shot {route}-idle.png',f'midi 90 {s["note"]:02X} 64']
 for f in range(24):commands+=['wait 50',f'shot {route}-{f}.png']
 commands+=['tap PLAY','wait 50',f'shot {route}-stop.png']
p=O/'test.txt';p.write_text('\n'.join(commands)+'\n')
r=subprocess.run([str(R/'build/host/neverhood_host'),str(p),str(O)],capture_output=True,text=True);assert not r.returncode,r.stdout+r.stderr
for route,box in enumerate(boxes):
 idle=Image.open(O/f'{route}-idle.png').convert('RGB');different=False
 for f in range(24):
  diff=ImageChops.difference(idle,Image.open(O/f'{route}-{f}.png').convert('RGB'));diff.paste((0,0,0),(0,174,240,240));b=diff.getbbox()
  if b:
   different=True;assert box[0]<=b[0] and box[1]<=b[1] and b[2]<=box[2] and b[3]<=box[3],(route,b,box)
 assert different,('frozen route',route)
 assert not ImageChops.difference(idle.crop((0,0,240,174)),Image.open(O/f'{route}-stop.png').convert('RGB').crop((0,0,240,174))).getbbox()
print('Effects: mouth/cymbal/levers/drum/pipe/horn move inside their source; INSERT TAPE uses levers; clean restoration')
