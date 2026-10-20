#!/usr/bin/env python3
"""Exercise HOME, FX controls, layering, original footsteps and listening."""
import array
from pathlib import Path
import subprocess
import wave
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/check/walk"
OUT.mkdir(parents=True, exist_ok=True)
commands = ["wait 700", "midi C0 03 00", "wait 30", "tap FX", "expect controls 1", "expect walking 0", "tap FX", "expect controls 0",
            "shot idle.png", "peakreset", "wav walk.wav", "press HOME", "wait 120", "release HOME",
            "expect walking 1", "expect voices 0"]
for i in range(16):
    commands += ["wait 300", f"shot frame-{i:02d}.png"]
commands += ["wait 1000", "expect walking 0", "expect voices 0", "shot finished.png", "wavstop", "expect peak_db_min -50",
             "peakreset", "wait 200", "expect peak_db_max -80",
             # Holding HOME launches one pass, rather than restarting every poll.
             "press HOME", "wait 6000", "expect walking 0", "release HOME",
             # A fresh press starts a new pass; a second press during it is ignored.
             "tap HOME", "wait 200", "expect walking 1", "tap HOME", "wait 200", "expect walking 1",
             "key 7 down", "wait 100", "expect voices 1", "expect walking 1",
             "midi B0 78 00", "wait 20", "expect walking 0", "expect voices 0", "key 7 up",
             "peakreset", "wait 100", "expect peak_db_max -80",
             # Walking at zero master volume must remain silent.
             "master 0", "peakreset", "wav muted.wav", "tap HOME", "wait 1300", "expect walking 1", "wavstop",
             "expect peak_db_max -80", "midi B0 78 00", "wait 20", "expect walking 0",
             "expect faults 0", "expect dropped 0"]
scenario = OUT / "walk.txt"
scenario.write_text("\n".join(commands) + "\n")
run = subprocess.run([str(ROOT / "build/host/neverhood_host"), str(scenario), str(OUT)], capture_output=True, text=True)
(OUT / "host.log").write_text(run.stdout + run.stderr)
if run.returncode:
    raise SystemExit(run.stdout + run.stderr)
idle = Image.open(OUT / "idle.png").convert("RGB")
frames = [Image.open(OUT / f"frame-{i:02d}.png").convert("RGB") for i in range(16)]
centers = []
for frame in frames:
    bounds = ImageChops.difference(idle, frame).getbbox()
    if bounds:
        assert 85 <= bounds[1] < bounds[3] <= 170, bounds
        centers.append((bounds[0] + bounds[2]) / 2)
assert len(centers) >= 10, "Too few visible walk frames"
assert centers[-1] - centers[0] > 150, centers
assert all(b > a for a, b in zip(centers, centers[1:])), centers
assert ImageChops.difference(idle, Image.open(OUT / "finished.png").convert("RGB")).getbbox() is None
with wave.open(str(OUT / "walk.wav")) as wav:
    rate = wav.getframerate()
    samples = array.array("h", wav.readframes(wav.getnframes()))
assert max(map(abs, samples)) < 32760
first = samples[:rate * 2]
last = samples[rate * 7:rate * 9]  # 3.5..4.5 s, while the actor exits right.
def energy(values, channel):
    return sum(x*x for x in values[channel::2])
assert energy(first, 0) > energy(first, 1) * 3
assert energy(last, 1) > energy(last, 0) * 3
previews = [idle] + frames + [idle]
previews[0].save(OUT / "walk.gif", save_all=True, append_images=previews[1:], duration=300, loop=0)
print("Walk: left-to-right, 80px actor in foreground, clean restoration; original footsteps pan with actor.")
print("Walk: HOME binding, restored FX controls, retrigger, notes, panic and muted master passed.")

# Held notes stop him indefinitely; category changes steer his gaze, and the
# sustain pedal keeps him listening until its held sound releases.
commands = ["wait 700", "midi C0 03 00", "wait 30", "tap HOME", "wait 1800", "key 7 down", "wait 200",
            "expect watching 0", "expect klay_state 2", "shot stopping.png", "wait 3300",
            "expect watching 1", "expect look_left 1", "shot watching-left.png",
            "wait 12000", "expect walking 1", "expect watching 1", "shot watching-held.png",
            "tap FX", "expect controls 1", "expect watching 1", "tap FX", "expect controls 0",
            "midi C0 0A 00", "midi 90 3C 64", "wait 3300", "expect watching 1", "expect look_left 0",
            "shot watching-right.png", "midi B0 40 7F", "midi 80 3C 00", "key 7 up",
            "wait 800", "expect watching 1", "midi B0 40 00", "wait 1500", "expect watching 1", "shot waiting.png",
            "wait 4000", "expect watching 0", "shot resumed.png", "wait 6000", "expect walking 0", "expect faults 0", "expect dropped 0",
            # Notes held before HOME must not strand him outside the picture.
            "midi C0 03 00", "key 7 down", "wait 200", "tap HOME", "wait 5000",
            "expect walking 1", "expect watching 1", "midi B0 78 00", "wait 20", "expect walking 0", "expect watching 0"]
scenario = OUT / "watching.txt"
scenario.write_text("\n".join(commands) + "\n")
run = subprocess.run([str(ROOT / "build/host/neverhood_host"), str(scenario), str(OUT)], capture_output=True, text=True)
(OUT / "watching.log").write_text(run.stdout + run.stderr)
if run.returncode:
    raise SystemExit(run.stdout + run.stderr)
a = Image.open(OUT / "watching-left.png").crop((0, 120, 240, 171))
b = Image.open(OUT / "watching-held.png").crop((0, 120, 240, 171))
assert ImageChops.difference(a, b).getbbox() is None, "Actor moved while listening"
print("Watch: animated stop/turn/think/start, 12s hold, both directions, sustain, 2s dwell, FX and resume passed.")

# Transport must preserve the actor's walk and listening animation.
scenario=OUT/'transport.txt'
scenario.write_text("\n".join([
 "wait 700","tap HOME","wait 1500","expect walking 1",
 "tap PLAY","wait 50","expect player 1","expect walking 1","expect walk_x >-70",
 "tap PLAY","wait 50","expect player 0","expect walking 1",
 "wait 6000","expect walking 0",
 "midi C0 03 00","tap HOME","wait 1500","key 7 down","wait 3300","expect watching 1",
 "tap PLAY","wait 50","expect walking 1","expect watching 1",
 "key 7 up","tap PLAY","wait 12000","expect walking 0",
 "expect faults 0","expect dropped 0"
])+"\n")
run=subprocess.run([str(ROOT/"build/host/neverhood_host"),str(scenario),str(OUT)],capture_output=True,text=True)
(OUT/'transport.log').write_text(run.stdout+run.stderr)
assert not run.returncode,run.stdout+run.stderr
print('Walk: PLAY start/stop preserves walking and watching; actor completes his pass naturally')
