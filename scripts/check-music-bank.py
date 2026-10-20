#!/usr/bin/env python3
"""Exercise held sample notes, pedal release, drum mappings and bank navigation."""
import array
import hashlib
import json
from pathlib import Path
import subprocess
import wave

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/check/music-playback"
OUT.mkdir(parents=True, exist_ok=True)
commands = ["wait 700", "master 2800"]
for program, category in ((19,5),(20,4),(21,6),(2,6),(1,1)):
    commands += ["tap PLAY", f"midi C0 {program:02X} 00", "wait 30", f"expect category {category}",
                 "expect tone 50", "expect decay 50", "peakreset", f"wav {program+1}.wav",
                 "midi 90 3C 64", "wait 900", "expect voices 1",
                 "midi 80 3C 00", f"wait {1800 if program==2 else 600}", "expect voices 0", "wavstop", "expect peak_db_min -55"]
# Sax must sustain across thousands of cycle boundaries, then release normally.
commands += ["tap PLAY", "midi C0 01 00", "wait 2000", "wav sax-sustain.wav", "midi 90 3C 64",
             "wait 12000", "expect voices 1", "midi 80 3C 00", "wait 600", "expect voices 0", "wavstop"]
commands += ["midi C0 13 00", "wait 30", "midi 90 3C 64", "midi B0 40 7F", "midi 80 3C 00",
             "wait 700", "expect voices 1", "midi B0 40 00", "wait 600", "expect voices 0",
             "turn K1 -25", "expect tone 0", "midi C0 14 00", "wait 30", "expect tone 50"]
for note in (60,64,67,71,74,77): commands += [f"midi 90 {note:02X} 64"]
commands += ["wait 500", "expect voices 4", "tap PLAY", "wait 30", "expect voices 0",
             "shot room-keys.png", "expect faults 0", "expect dropped 0"]
path=OUT/'music.txt';path.write_text('\n'.join(commands)+'\n')
r=subprocess.run([str(ROOT/'build/host/neverhood_host'),str(path),str(OUT)],capture_output=True,text=True)
(OUT/'host.log').write_text(r.stdout+r.stderr)
assert not r.returncode,r.stdout+r.stderr
hashes=set()
for path in (OUT/f'{n}.wav' for n in (20,21,22,3,2)):
    with wave.open(str(path)) as wav: raw=wav.readframes(wav.getnframes())
    samples=array.array('h',raw)
    assert max(abs(x) for x in samples)<32760,path
    digest=hashlib.sha256(raw).hexdigest();assert digest not in hashes,path
    hashes.add(digest)
def pcm(name):
    with wave.open(str(OUT/(name+'.wav'))) as w: return array.array('h',w.readframes(w.getnframes()))
# Identical fresh boots avoid random per-note vibrato and reverb history differences.
for name, tap in (("box-tap",True),("box-held",False)):
    commands = ["wait 700", "master 2800", "midi C0 02 00", "wait 100", f"wav {name}.wav",
                "midi 90 3C 64", "wait 80"]
    commands += ["midi 80 3C 00"] if tap else ["wait 2"]
    commands += ["wait 700", "expect voices 1", "wait 2200", "expect voices 0", "wavstop"]
    path=OUT/(name+'.txt');path.write_text('\n'.join(commands)+'\n')
    r=subprocess.run([str(ROOT/'build/host/neverhood_host'),str(path),str(OUT)],capture_output=True,text=True)
    assert not r.returncode,r.stdout+r.stderr
a,b=pcm('box-tap'),pcm('box-held')
assert a==b, 'Music Box tap was shortened'
s=pcm('sax-sustain');rate=44100
for sec in range(1,12):
    window=s[sec*rate*2:(sec+1)*rate*2]
    assert sum(x*x for x in window)/len(window)>100, f'Sax stopped at {sec}s'
print('Music bank: bass/guitar/keys, Music Box and Weasel Sax, full Music Box tap decay, 12s continuous Sax sustain, note-off, pedal, stealing and panic passed')
