"""One-byte pixels with a shared RGB565 palette; dimensions and alpha silhouette stay intact."""
import json
import math
from pathlib import Path
from PIL import Image, ImageChops


def c_array(name, values, kind="uint8_t"):
    values = list(values)
    return [f"static const {kind} {name}[] = {{"] + [
        "    " + ",".join(map(str, values[i:i + 24])) + "," for i in range(0, len(values), 24)] + ["};"]


def palette_image(image, rgba=False, colors_count=256):
    mode = "RGBA" if rgba else "RGB"
    original = image.convert(mode)
    quantized = original.quantize(colors=colors_count, method=Image.Quantize.FASTOCTREE if rgba else Image.Quantize.MEDIANCUT,
                                 dither=Image.Dither.NONE)
    palette = quantized.getpalette("RGBA")
    palette += [0] * (1024-len(palette))
    colors, alpha = [], []
    for i in range(256):
        r, g, b, a = palette[4*i:4*i+4]
        c = (r >> 3) << 11 | (g >> 2) << 5 | b >> 3
        colors.append((c >> 8) | ((c & 255) << 8))
        alpha.append(min(15, (a + 8) // 17))
    # Quantize color/edge coverage, but do not create or remove transparent pixels.
    indices = bytearray(quantized.tobytes())
    if rgba:
        transparent = next((i for i, a in enumerate(alpha) if not a), None)
        assert transparent is not None
        opaque = [i for i, a in enumerate(alpha) if a]
        for pos, pixel in enumerate(original.get_flattened_data()):
            if (pixel[3] + 8) // 17 == 0:
                indices[pos] = transparent
            elif not alpha[indices[pos]]:
                indices[pos] = min(opaque, key=lambda i: sum((pixel[k]-palette[4*i+k])**2 for k in range(4)))
    # Preview exactly the RGB565 and alpha4 that firmware will decode.
    decoded_palette = []
    for color, a in zip(colors, alpha):
        c = (color >> 8) | ((color & 255) << 8)
        decoded_palette.append((((c >> 11) & 31)*255//31, ((c >> 5) & 63)*255//63, (c & 31)*255//31, a*17))
    preview = Image.new("RGBA", image.size)
    preview.putdata([decoded_palette[i] for i in indices])
    return indices, colors, alpha, preview


def report(root, name, original, preview, before, after):
    out = Path(root) / "build/check/image-compression"
    out.mkdir(parents=True, exist_ok=True)
    preview.save(out / f"{name}-palette.png")
    background = Image.new("RGBA", original.size, (54, 29, 22, 255))
    before_rgb = Image.alpha_composite(background, original.convert("RGBA")).convert("RGB")
    after_rgb = Image.alpha_composite(background, preview).convert("RGB")
    histogram = ImageChops.difference(before_rgb, after_rgb).histogram()
    mse = sum((i % 256)**2*n for i, n in enumerate(histogram)) / (original.width*original.height*3)
    stats = dict(original_bytes=before, packed_bytes=after, ratio=after/before,
                 psnr_db=round(10*math.log10(255**2/mse), 2) if mse else None)
    (out / f"{name}.json").write_text(json.dumps(stats, indent=2) + "\n")
    print(f"{name}: {before} -> {after} B ({100*after/before:.1f}%), PSNR {stats['psnr_db']} dB")
