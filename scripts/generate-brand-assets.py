"""Generate PeekII's deterministic application icon (resources/peekii.ico, docs/images/peekii-icon.png).

Design: an ink-violet tile with an amber-to-coral ring in the shape of an eye. The pupil is a Roman numeral II
(two serifed bars). Small sizes are rendered separately with heavier strokes so the
mark stays legible as a 16 px tray icon. Needs Pillow only; output is byte-for-byte repeatable.
"""

import io
import struct
from math import pi, sin
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
RESOURCES = ROOT / "resources"
DOC_IMAGES = ROOT / "docs" / "images"

INK_TOP = (38, 24, 74)        # deep violet
INK_BOTTOM = (11, 9, 26)      # near black
AMBER = (255, 197, 61)
CORAL = (255, 84, 120)
WHITE = (255, 255, 255)


def gradient(size: int, start, end, horizontal: bool = False) -> Image.Image:
    ramp = Image.linear_gradient("L").resize((size, size))
    if horizontal:
        ramp = ramp.transpose(Image.Transpose.ROTATE_270)
    return Image.composite(Image.new("RGB", (size, size), end), Image.new("RGB", (size, size), start), ramp)


def almond(draw: ImageDraw.ImageDraw, scale: float, cx: float, cy: float, half_w: float, half_h: float) -> None:
    top, bottom = [], []
    for index in range(129):
        t = index / 128
        x = cx - half_w + 2 * half_w * t
        offset = half_h * sin(pi * t)
        top.append((x * scale, (cy - offset) * scale))
        bottom.append((x * scale, (cy + offset) * scale))
    draw.polygon(top + list(reversed(bottom)), fill=255)


def render(size: int, bold: bool) -> Image.Image:
    """Draw the icon at `size` px (supersampled 4x). `bold` thickens the strokes for tiny sizes."""
    ss = 4
    canvas = size * ss
    scale = canvas / 256.0
    stroke = 21 if bold else 15
    # Roman numeral II: two bars with slab serifs, so it is not mistaken for a "pause" symbol.
    bar_w, bar_gap, total_h = (22, 24, 70) if bold else (18, 26, 70)
    slab_h, slab_w = (13, 40) if bold else (11, 38)

    tile = Image.new("L", (canvas, canvas), 0)
    ImageDraw.Draw(tile).rounded_rectangle((8 * scale, 8 * scale, 248 * scale, 248 * scale), radius=58 * scale, fill=255)

    outer = Image.new("L", (canvas, canvas), 0)
    almond(ImageDraw.Draw(outer), scale, 128, 128, 104, 66)
    inner = Image.new("L", (canvas, canvas), 0)
    almond(ImageDraw.Draw(inner), scale, 128, 128, 104 - stroke * 1.25, 66 - stroke)
    ring = ImageChops.subtract(outer, inner)

    bars = Image.new("L", (canvas, canvas), 0)
    draw = ImageDraw.Draw(bars)
    for center in (128 - (bar_gap + bar_w) / 2 - 0.0, 128 + (bar_gap + bar_w) / 2):
        top, bottom = 128 - total_h / 2, 128 + total_h / 2
        draw.rectangle(((center - bar_w / 2) * scale, top * scale, (center + bar_w / 2) * scale, bottom * scale), fill=255)
        for y in (top, bottom - slab_h):
            draw.rounded_rectangle(((center - slab_w / 2) * scale, y * scale, (center + slab_w / 2) * scale, (y + slab_h) * scale),
                                   radius=slab_h * 0.3 * scale, fill=255)

    image = Image.new("RGBA", (canvas, canvas), (0, 0, 0, 0))
    image.paste(gradient(canvas, INK_TOP, INK_BOTTOM), (0, 0), tile)
    image.paste(gradient(canvas, AMBER, CORAL, horizontal=True), (0, 0), ring)
    image.paste(Image.new("RGB", (canvas, canvas), WHITE), (0, 0), bars)
    return image.resize((size, size), Image.Resampling.LANCZOS)


def write_ico(path: Path, sizes) -> None:
    """Write a multi-image ICO whose entries are PNG-compressed, each rendered natively at its own size."""
    blobs = []
    for size in sizes:
        buffer = io.BytesIO()
        render(size, bold=size <= 32).save(buffer, format="PNG", optimize=True)
        blobs.append((size, buffer.getvalue()))
    header = struct.pack("<HHH", 0, 1, len(blobs))
    offset = 6 + 16 * len(blobs)
    entries = b""
    for size, data in blobs:
        entries += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    path.write_bytes(header + entries + b"".join(data for _, data in blobs))


def main() -> None:
    RESOURCES.mkdir(parents=True, exist_ok=True)
    DOC_IMAGES.mkdir(parents=True, exist_ok=True)
    render(512, bold=False).save(DOC_IMAGES / "peekii-icon.png", optimize=True)
    write_ico(RESOURCES / "peekii.ico", [16, 20, 24, 32, 40, 48, 64, 128, 256])


if __name__ == "__main__":
    main()
