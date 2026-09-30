#!/usr/bin/env python3
"""Build Desktop Mode Dock icons from the shipped 2001 Netflix wordmark."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = (
    ROOT
    / "assets/ipodjs/rockbox/netflix"
    / "netflix-logo-2001.150x70x24.bmp"
)
DEFAULT_OUTPUT = ROOT / "assets/ipodjs/rockbox/netflix/desktop"
SIZES = (32, 34, 38, 64, 66, 70)


def write_rga(path: Path, image: Image.Image) -> None:
    rgba = image.convert("RGBA")
    source = rgba.tobytes()
    payload = bytearray()
    for offset in range(0, len(source), 4):
        red, green, blue, alpha = source[offset : offset + 4]
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
    radius = max(8, size // 6)
    draw.rounded_rectangle(
        (inset + 1, inset + 2, size - inset, size - inset),
        radius=radius,
        fill=(0, 0, 0, 125),
    )
    draw.rounded_rectangle(
        (inset, inset, size - inset - 1, size - inset - 2),
        radius=radius,
        fill=(180, 19, 29, 255),
        outline=(70, 8, 12, 255),
        width=max(1, size // 32),
    )
    logo_width = size - inset * 3
    logo_height = max(1, round(logo_width * wordmark.height / wordmark.width))
    logo = wordmark.resize((logo_width, logo_height), Image.Resampling.LANCZOS)
    icon.alpha_composite(
        logo,
        ((size - logo_width) // 2, (size - logo_height) // 2 - 1),
    )
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

    checksums = []
    for path in outputs:
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        checksums.append(f"{digest}  {path.name}")
    (args.output / "SHA256SUMS").write_text(
        "\n".join(checksums) + "\n",
        encoding="ascii",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
