#!/usr/bin/env python3
"""Verify all seven former CLAY/DRUMS patches inside the one-shot key kit."""
import hashlib
from pathlib import Path
import subprocess
import wave

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/check/drum-kit';OUT.mkdir(parents=True,exist_ok=True)
commands=['wait 700','master 2800','midi C0 12 00','wait 30','expect program 18','expect category 7']
for note in range(53,61):
    commands += ['tap PLAY','wait 20','peakreset',f'wav {note}.wav',f'midi 90 {note:02X} 64','wait 20',
                 f'midi 80 {note:02X} 00','wait 20','expect voices 1','wait 5000',
                 'expect voices 0','wavstop','expect peak_db_min -55']
commands += ['midi B0 40 7F']
for note in range(53,57): commands += [f'midi 90 {note:02X} 64',f'midi 80 {note:02X} 00']
commands += ['wait 100','expect voices 4','shot drum-kit.png','wait 5000','expect voices 0',
             'tap PLAY','expect faults 0','expect dropped 0']
scenario=OUT/'kit.txt';scenario.write_text('\n'.join(commands)+'\n')
run=subprocess.run([str(ROOT/'build/host/neverhood_host'),str(scenario),str(OUT)],capture_output=True,text=True)
(OUT/'host.log').write_text(run.stdout+run.stderr)
assert not run.returncode,run.stdout+run.stderr
hashes=set()
for note in range(53,61):
    with wave.open(str(OUT/f'{note}.wav')) as wav: raw=wav.readframes(wav.getnframes())
    hashes.add(hashlib.sha256(raw).hexdigest())
assert len(hashes)==8
print('Drum Kit: eight distinct voices on F3–C4, including COUGH DROPS; one shots survive early note-off and pedal; polyphony and decay passed')
