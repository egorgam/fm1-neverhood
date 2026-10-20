#!/usr/bin/env python3
"""Check navigation and that notes animate only the pictured instrument."""
from pathlib import Path
import subprocess
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/check/ui"
OUT.mkdir(parents=True, exist_ok=True)
HOST = ROOT / "build/host/neverhood_host"

def run(scenario):
    result = subprocess.run([str(HOST), str(scenario), str(OUT)], capture_output=True, text=True)
    (OUT / (scenario.stem + ".log")).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stdout + result.stderr)

run(ROOT / "tests/categories.txt")
# Boxes describe the actual objects on the illustration, in full LCD coordinates.
groups = [(3, "voice", (19, 65, 61, 116)), (1, "horns", (163, 59, 209, 115)),
          (10, "air", (202, 99, 222, 118)), (12, "strings", (92, 104, 115, 144)),
          (13, "guitar", (92, 104, 115, 144)),
          (6, "bass", (0, 32, 27, 119)),
          (2, "keys", (89, 104, 175, 163)),
          (18, "drums", (54, 61, 118, 126))]
commands = ["wait 700"]
for program, name, _ in groups:
    commands += [f"midi C0 {program:02X} 00", "wait 100", f"shot {name}-idle.png",
                 "midi 90 3C 7F"]
    for frame in range(8):
        commands += ["wait 70", f"shot {name}-{frame}.png"]
    commands += ["midi B0 78 00", "wait 150", f"shot {name}-stop.png", "expect voices 0"]
commands += ["expect faults 0", "expect dropped 0"]
scenario = OUT / "animation.txt"
scenario.write_text("\n".join(commands) + "\n")
run(scenario)

for _, name, box in groups:
    idle = Image.open(OUT / f"{name}-idle.png").convert("RGB")
    frames = [Image.open(OUT / f"{name}-{f}.png").convert("RGB") for f in range(8)]
    mechanism = (0, 59, 240, 174)
    assert len({f.crop(mechanism).tobytes() for f in frames}) > 1, f"{name}: frozen animation"
    for frame in frames:
        diff = ImageChops.difference(idle, frame)
        diff.paste((0, 0, 0), (0, 174, 240, 240))  # Live note label is allowed to change.
        changed = diff.getbbox()
        if changed:
            assert (box[0] <= changed[0] and box[1] <= changed[1]
                    and changed[2] <= box[2] and changed[3] <= box[3]), (name, changed, box)
    stopped = Image.open(OUT / f"{name}-stop.png").convert("RGB")
    assert ImageChops.difference(idle, stopped).getbbox() is None, f"{name}: did not return to rest"
    enlarged = [f.resize((720, 720), Image.Resampling.NEAREST) for f in frames]
    enlarged[0].save(OUT / f"{name}.gif", save_all=True, append_images=enlarged[1:], duration=70, loop=0)
    print(f"UI: {name}: moving, confined to its object, clean after panic")
print("UI: category navigation and all six mechanisms passed")
