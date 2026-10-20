#!/usr/bin/env python3
"""Verify every kit mapping, independent ADPCM decode, one shots, overlap and boundary notes."""
import hashlib
import json
from pathlib import Path
import subprocess
import wave

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/check/effects-kit"
OUT.mkdir(parents=True, exist_ok=True)
samples = json.loads((ROOT / "build/effects-kit/manifest.json").read_text())["samples"]
exe = OUT / "decoder"
subprocess.run(["cc", "-O2", "-fsanitize=address,undefined", "-I"+str(ROOT), "-I"+str(ROOT/"build/gen"),
                str(ROOT/"tests/effects_decode.c"), str(ROOT/"effects_kit.c"), "-o", str(exe)], check=True)
scenario = ["wait 700", "master 2800", "midi C0 00 00", "wait 30", "expect program 0", "expect category 8",
            "expect decay 100", "expect room 0"]
hashes = set()
assert len(samples)==38
assert [s["note"] for s in samples] == list(range(53,91))
assert samples[0]["short_label"] == "BURP" and samples[8]["short_label"] == "GONG"
assert {s["resource"] for s in samples}.isdisjoint({"C0E4884C","D0A1C348","09A93B96","C0238244"})
for i,sample in enumerate(samples):
    assert sample["rate"]==16000
    assert abs(sample["samples"]/16000-sample["source_samples"]/22050) <= 1/16000+1e-9
    raw = OUT / "decoded.pcm"
    subprocess.run([str(exe),str(i),str(raw)], check=True)
    with wave.open(str(ROOT/"build/effects-kit"/sample["file"])) as f:
        expected = f.readframes(f.getnframes())
    assert raw.read_bytes() == expected, f"firmware decoder differs: {sample['resource']}"
    hashes.add(hashlib.sha256(expected).hexdigest())
    note = sample["note"]
    scenario += ["midi B0 78 00", "wait 20", "peakreset", f"wav {sample['file']}",
                 f"midi 90 {note:02X} 64", "wait 15", f"midi 80 {note:02X} 00", "expect voices 1", f"expect effect {i}",
                 f"wait {int(sample['seconds']*1000)+120}", "expect voices 0", "wavstop", "expect peak_db_min -60"]
scenario += ["shot kit-last.png", "midi 90 34 64", "midi 90 5B 64", "wait 20", "expect voices 0"]
for note in (68,69,70,71): scenario += [f"midi 90 {note:02X} 64"]
scenario += ["wait 30", "expect voices 4", "shot kit-chord.png", "midi 90 47 64", "wait 20", "expect voices 4",
             "midi B0 78 00", "wait 20", "expect voices 0", "peakreset", "wait 120", "expect peak_db_max -80",
             "expect faults 0", "expect dropped 0", "tap FX", "expect controls 1", "shot kit-controls.png",
             "midi C0 03 00", "wait 30", "expect program 3", "expect decay 50", "expect room 25"]
path=OUT/"kit.txt"; path.write_text("\n".join(scenario)+"\n")
r=subprocess.run([str(ROOT/"build/host/neverhood_host"),str(path),str(OUT)],capture_output=True,text=True)
(OUT/"host.log").write_text(r.stdout+r.stderr)
assert not r.returncode,r.stdout+r.stderr
assert len(hashes)==len(samples),"duplicate decoded samples"
print(f"Effects Kit: {len(samples)} exact reference decodes and distinct note mappings; full one shots, four voices, retrigger, panic and bounds passed")
