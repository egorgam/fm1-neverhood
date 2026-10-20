#!/usr/bin/env python3
"""Copy the lettering pixels from the user's reference into one LCD glyph.

No letter is traced, moved or resized separately. Only the green background
is separated from the dark ink; the whole word then gets one uniform scale.
"""
import argparse
import math
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
HEIGHT = 48
parser = argparse.ArgumentParser()
parser.add_argument("--out", type=Path, required=True)
args = parser.parse_args()
source = Image.open(ROOT / "assets/neverhood-lettering-reference.png").convert("RGB")
sw, sh = source.size
values = [max(pixel) for pixel in source.get_flattened_data()]
remaining = {i for i, value in enumerate(values) if value < 90}
alpha = [0] * (sw * sh)
components = 0
# Find the word's bounds from its connected ink shapes, without glyph positions.
while remaining:
    start = remaining.pop()
    queue, component = [start], [start]
    while queue:
        i = queue.pop()
        x, y = i % sw, i // sw
        for nx, ny in ((x-1, y), (x+1, y), (x, y-1), (x, y+1)):
            if 0 <= nx < sw and 0 <= ny < sh:
                j = ny * sw + nx
                if j in remaining:
                    remaining.remove(j)
                    queue.append(j)
                    component.append(j)
    if sum(values[i] < 45 for i in component) < 20:
        continue
    components += 1
    for i in component:
        alpha[i] = min(255, (90 - values[i]) * 255 // 45)
assert components == 12, f"Expected 9 letters and the small 'the', got {components} shapes"
original_mask = Image.new("L", source.size)
original_mask.putdata(alpha)
bounds = original_mask.getbbox()
# Inside those bounds retain every dark source pixel, including tiny detached
# details in the lettering. Only background flecks outside the word are dropped.
for y in range(bounds[1], bounds[3]):
    for x in range(bounds[0], bounds[2]):
        i = y * sw + x
        alpha[i] = max(0, min(255, (90 - values[i]) * 255 // 45))
original_mask.putdata(alpha)
original_mask.save(ROOT / "assets/neverhood-lettering-alpha.png")
cropped = original_mask.crop(bounds)
fit = HEIGHT / cropped.height
WIDTH = math.ceil(cropped.width * fit)
# Both axes use exactly the same scale. The rounded-up width is padding only.
mask = cropped.transform((WIDTH, HEIGHT), Image.Transform.AFFINE,
                         (1 / fit, 0, 0, 0, 1 / fit, 0), Image.Resampling.BICUBIC)
mask.save(ROOT / "assets/neverhood-wordmark-mask.png")
pixels = list(mask.getdata())
data = []
for y in range(HEIGHT):
    row = [min(15, (v + 8) // 17) for v in pixels[y * WIDTH:(y + 1) * WIDTH]] + [0]
    data.extend((row[x] << 4) | row[x + 1] for x in range(0, WIDTH, 2))
lines = ["/* Copied from assets/neverhood-lettering-reference.png; whole word in glyph N. */",
         "static const uint8_t NEVERHOOD_MARK_DATA[] = {"]
lines += ["    " + ",".join(map(str, data[i:i + 24])) + "," for i in range(0, len(data), 24)]
lines += ["};", f"static const uint8_t NEVERHOOD_MARK_ADV[] = {{{WIDTH}}};",
          f"static const uint8_t NEVERHOOD_MARK_BW[] = {{{WIDTH}}};",
          "static const uint16_t NEVERHOOD_MARK_OFF[] = {0};",
          f"static const felucca_font_t NEVERHOOD_MARK = {{ {HEIGHT}, 0, 'N', 'N', "
          "NEVERHOOD_MARK_ADV, NEVERHOOD_MARK_BW, NEVERHOOD_MARK_OFF, NEVERHOOD_MARK_DATA };", ""]
args.out.parent.mkdir(parents=True, exist_ok=True)
args.out.write_text("\n".join(lines))
print(f"wordmark: source pixels {bounds}, uniform scale {fit:.6f}; {WIDTH}x{HEIGHT}, {len(data)} B")
