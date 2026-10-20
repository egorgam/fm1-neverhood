#!/usr/bin/env python3
"""Boot the actual .fwsc in fm1-emu and play each bank slot with four MIDI voices."""
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/check/emulator-bank"
OUT.mkdir(parents=True, exist_ok=True)
results = []
for program in range(22):
    script = f"650:mC0{program:02X},900:m903C64,920:m904064,940:m904364,960:m904764"
    env = {**os.environ, "FM1_BENCH_KEYS": script}
    run = subprocess.run([str(ROOT / ".deps/fm1-emu/target/release/fm1-live"),
                          "--fwsc", str(ROOT / "build/neverhood.fwsc"), "--bench", "1600", "--midi", "none"],
                         cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)
    log = run.stdout + run.stderr
    (OUT / f"{program + 1:02d}.log").write_text(log)
    speed = re.search(r"steady: Some\(([\d.]+)\)x", log)
    frames = re.search(r"(\d+) real audio frames", log)
    if run.returncode or "halt:" in log or not speed or not frames or int(frames[1]) < 1000:
        raise SystemExit(f"Emulator slot {program + 1} failed:\n{log}")
    results.append({"slot": program + 1, "speed_x": round(float(speed[1]), 3), "audio_frames": int(frames[1])})
(OUT / "report.json").write_text(json.dumps(results, indent=2) + "\n")
print(f"fm1-emu: {len(results)} slots booted and rendered four MIDI notes; "
      f"slowest {min(item['speed_x'] for item in results):.2f}x real time on this computer")
