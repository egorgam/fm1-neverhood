#!/usr/bin/env python3
"""Pack original demo frames and PCM footsteps for the LCD and audio callback."""
import argparse
import array
import json
import math
from pathlib import Path
import wave
from PIL import Image
from palette_art import palette_image, report
from lz_art import c_packed

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--out", type=Path, required=True)
parser.add_argument("--colors", type=int, default=64)
parser.add_argument("--detail", type=float, default=.85)
args = parser.parse_args()
assets = ROOT / "assets/klaymen"
manifest = json.loads((assets / "manifest.json").read_text())
frames = manifest["frames"]
atlas = list(frames)
clips = {}
for name, clip in manifest["clips"].items():
    clips[name] = (len(atlas), len(clip))
    atlas.extend(clip)
scale = 80 / (max(f["y"] + f["height"] for f in frames) - min(f["y"] for f in frames))
left = min(f["x"] for f in atlas)
top = min(f["y"] for f in atlas)
right = max(f["x"] + f["width"] for f in atlas)
bottom = max(f["y"] + f["height"] for f in atlas)
height = math.ceil((bottom - top) * scale)
width = math.ceil((right - left) * scale)
display_width, display_height = width, height
width, height = round(width*args.detail), round(height*args.detail)
detail_scale = scale*args.detail
colors, alpha, previews = [], [], []
for f in atlas:
    sprite = Image.open(assets / f["file"]).convert("RGBA")
    # Index 64 in the original actor palette is a scene-defined shadow color.
    # The source PNG retains its green marker; shade our own clay floor instead.
    sprite.putdata([(44, 25, 16, 80) if p[:3] == (0, 255, 0) else p for p in sprite.get_flattened_data()])
    canvas = Image.new("RGBA", (right - left, bottom - top))
    canvas.paste(sprite, (f["x"] - left, f["y"] - top))
    # Pad before resizing so integer width rounding cannot squash the actor.
    target = canvas.transform((width, height), Image.Transform.AFFINE,
                              (1/detail_scale, 0, 0, 0, 1/detail_scale, 0), Image.Resampling.BICUBIC)
    previews.append(target)
    for r, g, b, a in target.get_flattened_data():
        c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        colors.append((c >> 8) | ((c & 255) << 8))
        alpha.append(min(15, (a + 8) // 17))
sheet = Image.new("RGBA", (width, height*len(previews)))
for i, image in enumerate(previews):
    sheet.paste(image, (0, height*i))
indices, palette_colors, palette_alpha, preview = palette_image(sheet, rgba=True, colors_count=args.colors)
# Transparent spans retain every row, frame and visible-pixel position.
# One palette index replaces each RGB565 + alpha4 pair; identical frames share data.
assert width < 256 and height < 120
packed, offsets, unique, legacy_unique = bytearray(), [], {}, set()
for frame in range(len(atlas)):
    encoded, legacy = bytearray(), bytearray()
    for y in range(height):
        spans, x = [], 0
        base = (frame * height + y) * width
        while x < width:
            if not alpha[base + x]:
                x += 1
                continue
            start = x
            while x < width and alpha[base + x]:
                x += 1
            spans.append((start, x - start))
        encoded.append(len(spans))
        legacy.append(len(spans))
        for start, length in spans:
            encoded.extend((start, length))
            encoded.extend(indices[base+start:base+start+length])
            legacy.extend((start, length))
            for x in range(start, start + length):
                c = colors[base + x]
                legacy.extend((c & 255, c >> 8))
            for x in range(0, length, 2):
                a = alpha[base + start + x] << 4
                if x + 1 < length:
                    a |= alpha[base + start + x + 1]
                legacy.append(a)
    legacy_unique.add(bytes(legacy))
    key = bytes(encoded)
    if key not in unique:
        unique[key] = len(packed)
        packed.extend(encoded)
    offsets.append(unique[key])
    # Verify every row/offset against the complete indexed raster before emitting C.
    pos = offsets[-1]
    for y in range(height):
        n = packed[pos]
        pos += 1
        decoded = [None] * width
        for _ in range(n):
            start, length = packed[pos:pos + 2]
            pos += 2
            for x in range(length):
                decoded[start + x] = packed[pos+x]
            pos += length
        base = (frame * height + y) * width
        assert decoded == [indices[base+x] if alpha[base+x] else None for x in range(width)]

def c_array(name, values, kind="uint8_t"):
    return [f"static const {kind} {name}[] = {{"] + [
        "    " + ",".join(map(str, values[i:i + 24])) + "," for i in range(0, len(values), 24)] + ["};"]

common = ["/* Generated from original Neverhood demo assets; see assets/klaymen/README.md. */",
          "#pragma once", f"#define KLW_WIDTH {display_width}", f"#define KLW_HEIGHT {display_height}",
          f"#define KLW_SOURCE_WIDTH {width}",f"#define KLW_SOURCE_HEIGHT {height}",
          f"#define KLW_FRAMES {len(frames)}", f"#define KLW_TICK_RATE {manifest['tick_rate']}",
          f"#define KLW_WATCH_FRAME {clips['think_hold'][0]}", f"#define KLW_ORIGIN_X {round(-left * scale)}"]
for name, (start, count) in clips.items():
    common += [f"#define KLW_{name.upper()} {start}u", f"#define KLW_{name.upper()}_COUNT {count}u"]
art = common + c_array("KLW_PALETTE", palette_colors[:args.colors], "uint16_t") + c_array("KLW_ALPHA", palette_alpha[:args.colors])
art += c_array("KLW_OFFSETS", offsets, "uint32_t") + c_packed("KLW_DATA", packed)
audio = common + c_array("KLW_TICKS", [f["ticks"] for f in atlas])
audio += c_array("KLW_DX", [round(f["dx"] * scale * 256) for f in atlas], "int16_t")
audio += c_array("KLW_STEP", [f["step"] for f in atlas])
for i, sound in enumerate(manifest["sounds"], 1):
    with wave.open(str(assets / sound["file"]), "rb") as wav:
        assert wav.getparams()[:3] == (1, 2, 22050)
        pcm = array.array("h", wav.readframes(wav.getnframes()))
    audio += c_array(f"KLW_SOUND_{i}", list(pcm), "int16_t")
    audio.append(f"#define KLW_SOUND_{i}_LENGTH {len(pcm)}u")
args.out.parent.mkdir(parents=True, exist_ok=True)
args.out.write_text("\n".join(art) + "\n")
args.out.with_name("klaymen_audio.h").write_text("\n".join(audio) + "\n")
preview_path = ROOT / "build/check/klaymen-source.gif"
preview_path.parent.mkdir(parents=True, exist_ok=True)
previews[0].save(preview_path, save_all=True, append_images=previews[1:len(frames)], duration=round(2000 / manifest["tick_rate"]), disposal=2, loop=0)
report(ROOT, "klaymen", sheet, preview, sum(map(len,legacy_unique))+len(offsets)*4,
       len(packed)+len(offsets)*4+len(palette_colors)*3)
print(f"Klaymen: {len(frames)} walk + {len(atlas)-len(frames)} transition frames, {width}x{height} source -> {display_width}x{display_height} display, unchanged frame timing; original 22050 Hz footsteps")
