#!/usr/bin/env python3
"""Build small Desktop Mode icons from the bundled official Steam mark."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = (
    ROOT / "assets/ipodjs/rockbox/steam/steam-logo-official.110x32x24.bmp"
)
DEFAULT_OUTPUT = ROOT / "assets/ipodjs/rockbox/steam/desktop"
SIZES = (32, 34, 38, 64, 66, 70)


def write_rga(path: Path, image: Image.Image) -> None:
    rgba = image.convert("RGBA")
    payload = bytearray()
    for red, green, blue, alpha in rgba.getdata():
        rgb565 = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        payload.extend(struct.pack("<HB", rgb565, alpha))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        struct.pack("<4sHH", b"RGA1", rgba.width, rgba.height) + payload
    )


def build_icon(wordmark: Image.Image, size: int) -> Image.Image:
    icon = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(icon)
    inset = max(2, size // 20)
    radius = max(7, size // 6)
    draw.rounded_rectangle(
        (inset + 1, inset + 2, size - inset, size - inset),
        radius=radius,
        fill=(0, 0, 0, 120),
    )
    draw.rounded_rectangle(
        (inset, inset, size - inset - 1, size - inset - 2),
        radius=radius,
        fill=(27, 40, 56, 255),
        outline=(102, 192, 244, 255),
        width=max(1, size // 32),
    )
    source = wordmark.convert("RGBA").crop((0, 0, 32, 32))
    pixels = []
    background = source.getpixel((31, 31))[:3]
    for red, green, blue, _alpha in source.getdata():
        distance = abs(red - background[0]) + abs(green - background[1]) + abs(blue - background[2])
        pixels.append((red, green, blue, 0 if distance < 22 else 255))
    source.putdata(pixels)
    mark_size = round(size * 0.68)
    source = source.resize((mark_size, mark_size), Image.Resampling.LANCZOS)
    icon.alpha_composite(source, ((size - mark_size) // 2, (size - mark_size) // 2 - 1))
    return icon


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    wordmark = Image.open(args.source).convert("RGBA")
    outputs = []
    for size in SIZES:
        name = (
            f"icon.{size}x{size}.rga"
            if size in (32, 64)
            else f"icon-dock-{size}.{size}x{size}.rga"
        )
        path = args.output / name
        write_rga(path, build_icon(wordmark, size))
        outputs.append(path)
    checksums = [
        f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}"
        for path in outputs
    ]
    (args.output / "SHA256SUMS").write_text(
        "\n".join(checksums) + "\n", encoding="ascii"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
