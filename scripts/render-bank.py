#!/usr/bin/env python3
"""Render the actual firmware app, validate audio and make a local audition page."""
import array
import hashlib
import html
import json
import math
from pathlib import Path
import re
import subprocess
import wave

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / "audition"
OUT.mkdir(parents=True, exist_ok=True)
instruments = re.findall(r'^\s*\{"([^"]+)",\s*"([^"]+)"', (ROOT / "clay_synth.c").read_text().split("const clay_instrument_t clay_bank")[1].split("};")[0], re.M)
assert len(instruments) == 22
script = ["wait 700", "master 2800"]
paths = []
for i, (name, family) in enumerate(instruments):
    filename = f"{i + 1:02d}-" + name.lower().replace(" ", "-") + ".wav"
    paths.append(filename)
    script += ["midi B0 78 00", f"midi C0 {i:02X} 00", "wait 30", f"expect program {i}", f"wav {filename}", "wait 60"]
    for note in (60, 63, 65, 67):
        script += [f"midi 90 {note:02X} 64", "wait 230", f"midi 80 {note:02X} 00", "wait 100"]
    script += ["midi 90 48 64", "wait 450", "midi 80 48 00", "wait 220"]
    for note in (60, 64, 67):
        script += [f"midi 90 {note:02X} 60"]
    script += ["wait 600"]
    for note in (60, 64, 67):
        script += [f"midi 80 {note:02X} 00"]
    script += ["wait 700", "wavstop", "expect faults 0", "expect dropped 0"]
script += ["midi B0 78 00", "wait 30", "expect voices 0"]
scenario = OUT / "bank.txt"
scenario.write_text("\n".join(script) + "\n")
run = subprocess.run([str(ROOT / "build/host/neverhood_host"), str(scenario), str(OUT)], capture_output=True, text=True)
(OUT / "host.log").write_text(run.stdout + run.stderr)
if run.returncode:
    raise SystemExit(run.stdout + run.stderr)

report, hashes = [], set()
with wave.open(str(OUT / "all-instruments.wav"), "wb") as combined:
    combined.setparams((2, 2, 44100, 0, "NONE", "not compressed"))
    for i, ((name, family), filename) in enumerate(zip(instruments, paths)):
        with wave.open(str(OUT / filename), "rb") as wav:
            assert wav.getparams()[:3] == (2, 2, 44100)
            raw = wav.readframes(wav.getnframes())
        samples = array.array("h", raw)
        peak = max(abs(x) for x in samples)
        rms = math.sqrt(sum(x * x for x in samples) / len(samples))
        peak_db = 20 * math.log10(max(peak, 1) / 32768)
        rms_db = 20 * math.log10(max(rms, 1e-9) / 32768)
        digest = hashlib.sha256(raw).hexdigest()
        if peak >= 32760 or rms_db < -52 or digest in hashes:
            raise SystemExit(f"Audio check failed: {name}: peak={peak}, rms={rms_db:.1f}, duplicate={digest in hashes}")
        hashes.add(digest)
        report.append({"slot": i + 1, "name": name, "family": family, "file": filename,
                       "seconds": len(samples) / 88200, "peak_dbfs": round(peak_db, 2),
                       "rms_dbfs": round(rms_db, 2), "sha256": digest})
        combined.writeframes(raw)
        combined.writeframes(bytes(44100))  # quarter-second stereo silence between patches
(OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
cards = "\n".join(
    f'<article><h2>{item["slot"]:02d} · {html.escape(item["name"])}</h2>'
    f'<p>{item["family"]}</p><audio controls preload="none" src="{item["file"]}"></audio></article>' for item in report)
(OUT / "index.html").write_text('''<!doctype html><meta charset="utf-8"><title>Neverhood · Clay Band</title>
<style>body{background:#382416;color:#f6d9a4;font:17px system-ui;max-width:1000px;margin:40px auto;padding:20px}
h1{font-size:40px}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px}
article{background:#64412b;border:1px solid #a97645;border-radius:18px;padding:18px}h2{font-size:18px}
audio{width:100%}a{color:#b9d2b1}header{margin-bottom:30px;line-height:1.6}</style>
<header><h1>Neverhood · Clay Band / 22</h1>
<p>First synthesized bank. The same original phrase on every instrument; this is not a tune from the game.
Likeness to the soundtrack has not been confirmed by listening.</p>
<p><a href="https://terrytaylor.bandcamp.com/album/neverhood-songs-deluxe">Original album by Terry Scott Taylor</a></p>
<p>All instruments in order:</p><audio controls preload="none" src="all-instruments.wav"></audio></header>
<main>''' + cards + "</main>")
print(f"Audition: {len(report)} distinct non-silent renders; no clipping; {OUT / 'index.html'}")
for item in report:
    print(f'{item["slot"]:02d} {item["name"]:<18} peak {item["peak_dbfs"]:6.1f} dBFS, RMS {item["rms_dbfs"]:6.1f} dBFS')
