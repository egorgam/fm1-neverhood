#!/usr/bin/env python3
"""Pack the full-resolution clay illustration as indexed RGB565."""
import argparse
from pathlib import Path
from PIL import Image
from palette_art import palette_image, c_array, report
from lz_art import c_packed

parser = argparse.ArgumentParser()
parser.add_argument("--out", type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
art = Image.open(root / "assets/clay-room-source.png").convert("RGB")
art = art.resize((240, 240), Image.Resampling.LANCZOS)
indices, colors, _, preview = palette_image(art, colors_count=64)
lines = ["/* Full 240x240 geometry; one-byte indices, LCD-order RGB565 palette. */"]
lines += c_array("CLAY_PALETTE", colors[:64], "uint16_t") + c_packed("CLAY_ROOM", indices)

args.out.parent.mkdir(parents=True, exist_ok=True)
args.out.write_text("\n".join(lines))
report(root, "background", art, preview, 240*240*2, len(indices)+len(colors)*2)
