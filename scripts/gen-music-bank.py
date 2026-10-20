#!/usr/bin/env python3
"""Pack approved 22050 Hz musical snippets as random-access G.711 mu-law samples."""
import argparse
import array
import audioop
import hashlib
import json
from pathlib import Path
import wave
from palette_art import c_array

ROOT = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
source=ROOT/'assets/music-bank'
manifest=json.loads((source/'manifest.json').read_text())
data=bytearray();table=[];report=[]
selected = [s for s in manifest['samples'] if s['name'] in ('bass','guitar','piano','music-box','weasel-sax')]
for sample in selected:
    path=source/sample['file']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==sample['wav_sha256']
    with wave.open(str(path)) as wav:
        assert wav.getparams()[:3]==(1,2,22050)
        raw=wav.readframes(wav.getnframes())
    packed=audioop.lin2ulaw(raw,2)
    decoded=audioop.ulaw2lin(packed,2)
    table.append((len(data),sample['frames'],sample['loop_start'],sample['loop_end'],sample['root_hz']))
    data.extend(packed)
    original=array.array('h',raw);actual=array.array('h',decoded)
    import math
    snr=10*math.log10(sum(x*x for x in original)/max(1,sum((x-y)**2 for x,y in zip(original,actual))))
    report.append({**sample,'codec':'G.711 mu-law, 22050 Hz','snr_db':round(snr,2)})
lut=array.array('h',audioop.ulaw2lin(bytes(range(256)),2))
lines=['/* Estimated musical sources from the original mono demo mix; assets/music-bank/manifest.json. */',
       '#pragma once','typedef struct { uint32_t offset, frames, loop_start, loop_end; float hz; } music_sample_t;',
       'static const music_sample_t MUSIC_SAMPLES[] = {']
lines += [f'    {{{off}u,{n}u,{a}u,{b}u,{hz:.9f}f}},' for off,n,a,b,hz in table]
lines += ['};']+c_array('MUSIC_DATA',data)+c_array('MUSIC_LUT',lut,'int16_t')
args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text('\n'.join(lines)+'\n')
out=ROOT/'build/check/music-bank';out.mkdir(parents=True,exist_ok=True)
(out/'manifest.json').write_text(json.dumps({'bytes':len(data)+len(table)*20+512,'samples':report},indent=2)+'\n')
print(f'Music bank: {len(table)} samples, {len(data)} B mu-law, {sum(s["frames"] for s in selected)/22050:.2f} s at 22050 Hz')
