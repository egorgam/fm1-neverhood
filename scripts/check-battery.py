#!/usr/bin/env python3
"""Battery readout fills only the three existing tube holes, bottom up."""
from pathlib import Path
import os,subprocess
from PIL import Image,ImageChops
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/check/battery';OUT.mkdir(parents=True,exist_ok=True)
images={}
for raw,level in ((-1,0),(530,0),(531,1),(560,1),(561,2),(590,2),(591,3),(640,3)):
    script=OUT/f'{raw}.txt';script.write_text(f'wait 1200\nexpect battery {level}\nshot {raw}.png\nexpect faults 0\n')
    r=subprocess.run([str(ROOT/'build/host/neverhood_host'),str(script),str(OUT)],capture_output=True,text=True,
                     env={**os.environ,'FM1_BATTERY_ADC':str(raw)})
    assert not r.returncode,r.stdout+r.stderr
    images[raw]=Image.open(OUT/f'{raw}.png').convert('RGB')
    for hole in range(3):
        point=(227,92-16*hole)
        assert (images[raw].getpixel(point)!=images[-1].getpixel(point))==(hole<level),(raw,hole)
    diff=ImageChops.difference(images[-1],images[raw]).getbbox()
    if diff: assert diff[0]>=223 and diff[2]<=232 and diff[1]>=55 and diff[3]<=98,diff
assert images[530].tobytes()==images[-1].tobytes()
assert images[531].tobytes()==images[560].tobytes()
assert images[561].tobytes()==images[590].tobytes()
assert images[591].tobytes()==images[640].tobytes()
print('Battery: unavailable/empty and stock threshold edges; 1/2/3 holes bottom-up; no unrelated pixels changed')
