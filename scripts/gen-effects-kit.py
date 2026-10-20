#!/usr/bin/env python3
"""Encode all complete 16000 Hz prepared demo effects as independent IMA ADPCM streams."""
import argparse
import array
import audioop
import hashlib
import html
import json
import math
from pathlib import Path
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--out", type=Path, required=True)
args = parser.parse_args()
source = ROOT / "assets/effects-kit"
manifest = json.loads((source / "manifest.json").read_text())
samples = [s for s in manifest["samples"] if s["included"]]
assert len(samples) == 38
assert [s["note"] for s in samples] == list(range(53,53+len(samples)))
assert len({s["resource"] for s in samples}) == len(samples)
encoded, table, report = bytearray(), [], []
playback = source / "playback-16000"
prepared = {s["file"]:s for s in json.loads((playback/"manifest.json").read_text())["samples"]}
preview = ROOT / "build/effects-kit"
preview.mkdir(parents=True, exist_ok=True)
for old in preview.glob("*.wav"):
    if old.name not in {s["file"] for s in samples}: old.unlink()
for sample in samples:
    with wave.open(str(source / "originals" / sample["file"]), "rb") as wav:
        assert wav.getparams()[:3] == (1, 2, 22050)
        raw = wav.readframes(wav.getnframes())
    assert hashlib.sha256(raw).hexdigest() == sample["pcm_sha256"]
    assert hashlib.sha256((playback/sample["file"]).read_bytes()).hexdigest() == prepared[sample["file"]]["wav_sha256"]
    with wave.open(str(playback/sample["file"])) as wav:
        assert wav.getparams()[:3] == (1,2,16000)
        frames = wav.getnframes()
        raw = wav.readframes(frames)
    # Pick an initial step size from a bounded prefix. Playback input has already been anti-aliased/resampled; no trimming.
    prefix = raw[:1024]
    target = array.array("h", prefix)
    def error(index):
        packed, _ = audioop.lin2adpcm(prefix, 2, (0, index))
        decoded, _ = audioop.adpcm2lin(packed, 2, (0, index))
        values = array.array("h", decoded)
        return sum((a-b)**2 for a,b in zip(target, values))
    index = min(range(89), key=error)
    # audioop drops an unmatched final nibble: pad one sample, preserve true length separately.
    padded = raw + (raw[-2:] if frames & 1 else b"")
    packed, _ = audioop.lin2adpcm(padded, 2, (0,index))
    decoded, _ = audioop.adpcm2lin(packed, 2, (0,index))
    decoded = decoded[:len(raw)]
    table.append((len(encoded), frames, int(sample["resource"],16), index))
    encoded.extend(packed)
    with wave.open(str(preview / sample["file"]), "wb") as wav:
        wav.setparams((1,2,16000,0,"NONE","not compressed"))
        wav.writeframes(decoded)
    original, restored = array.array("h",raw), array.array("h",decoded)
    signal = sum(x*x for x in original); noise = sum((a-b)**2 for a,b in zip(original,restored))
    report.append({**sample,"source_samples":sample["samples"],"samples":frames,"rate":16000,"seconds":frames/16000,"packed_bytes":len(packed),"initial_step":index,
                   "snr_db":round(10*math.log10(signal/noise),2) if noise and signal else None})
lines = ['/* Original Neverhood demo recordings; source/provenance in assets/effects-kit. */', '#pragma once',
         'typedef struct { uint32_t offset, frames, resource; uint8_t step; } effects_sample_t;',
         'static const effects_sample_t EFFECTS_SAMPLES[] = {']
lines += [f'    {{{off}u,{frames}u,0x{rid:08X}u,{step}u}},' for off,frames,rid,step in table]
lines += ['};', 'static const char *const EFFECTS_LABELS[] = {']
lines += [json.dumps(s['short_label'])+',' for s in samples]
lines += ['};','static const uint8_t EFFECTS_VISUALS[] = {'+','.join(str(s['visual']) for s in samples)+'};','static const uint8_t EFFECTS_DATA[] = {']
lines += ['    '+','.join(map(str,encoded[i:i+32]))+',' for i in range(0,len(encoded),32)]
lines += ['};','']
args.out.parent.mkdir(parents=True,exist_ok=True)
args.out.write_text('\n'.join(lines))
(preview / 'manifest.json').write_text(json.dumps({'rate':16000,'codec':'IMA ADPCM','base_note':53,'samples':report,
                                                  'omitted':[s['resource'] for s in manifest['samples'] if not s['included']]},indent=2)+'\n')
cards=[]
for s in report:
    cards.append(f'<tr><td>{s["note"]}</td><td>{html.escape(s["group_label"])}</td><td>{html.escape(s["label"])}</td><td>{s["resource"]}</td><td>{s["seconds"]} s</td><td>'
                 f'<audio controls preload="none" src="{s["file"]}"></audio></td></tr>')
(preview/'index.html').write_text('<!doctype html><html lang="en"><meta charset="utf-8"><title>Effects Kit</title>'
    '<meta name="viewport" content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;margin:32px;background:#241b24;color:#f4e4be}td{padding:8px}audio{height:32px}</style>'
    f'<h1>Effects Kit · {len(samples)}</h1><table><tr><th>MIDI</th><th>Group</th><th>Name</th><th>Sample</th><th>Length</th><th></th></tr>'+''.join(cards)+'</table></html>')
print(f'Effects Kit: {len(samples)} complete 16000 Hz recordings, {len(encoded)} B ADPCM + {len(table)*16} B table')
