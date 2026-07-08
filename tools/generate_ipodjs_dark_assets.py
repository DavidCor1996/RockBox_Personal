#!/usr/bin/env python3
"""Generate dark-mode iPodJS BMP asset variants.

Dark variants use the firmware lookup convention:
    name.12x12x24.bmp -> name.12x12x24-dark.bmp
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


TRANSPARENT_KEYS = {
    (255, 0, 255),
    (248, 0, 248),
}


def dark_variant_path(path: Path) -> Path:
    return path.with_name(f"{path.stem}-dark{path.suffix}")


def is_dark_variant(path: Path) -> bool:
    return path.stem.endswith("-dark")


def transform_pixel(rgb: tuple[int, int, int]) -> tuple[int, int, int]:
    r, g, b = rgb
    if rgb in TRANSPARENT_KEYS:
        return rgb

    lum = (r * 299 + g * 587 + b * 114) // 1000
    chroma = max(r, g, b) - min(r, g, b)

    if lum > 220 and chroma < 28:
        base = 26 + (255 - lum) // 10
        return (base, base + 2, base + 6)

    if lum > 176 and chroma < 42:
        base = 38 + (220 - min(lum, 220)) // 8
        return (base, base + 3, base + 8)

    if lum < 42:
        lift = 110 - lum
        return (
            min(255, r + lift),
            min(255, g + lift),
            min(255, b + lift),
        )

    if b > r + 20 and b > g + 8:
        return (
            min(255, int(r * 0.78) + 8),
            min(255, int(g * 0.84) + 12),
            min(255, int(b * 1.04) + 20),
        )

    return (
        min(255, int(r * 0.72) + 22),
        min(255, int(g * 0.72) + 24),
        min(255, int(b * 0.72) + 30),
    )


def convert(path: Path, force: bool = False) -> bool:
    target = dark_variant_path(path)
    if target.exists() and not force:
        return False

    with Image.open(path) as image:
        source = image.convert("RGB")
        out = Image.new("RGB", source.size)
        pixels_in = source.load()
        pixels_out = out.load()
        for y in range(source.height):
            for x in range(source.width):
                pixels_out[x, y] = transform_pixel(pixels_in[x, y])
        out.save(target, "BMP")
    return True


def generate(root: Path, force: bool = False) -> int:
    count = 0
    if not root.exists():
        return 0
    for path in sorted(root.rglob("*.bmp")):
        if is_dark_variant(path):
            continue
        if convert(path, force=force):
            count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("roots", nargs="+", type=Path)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    total = 0
    for root in args.roots:
        count = generate(root, force=args.force)
        print(f"{root}: generated {count} dark assets")
        total += count
    print(f"generated {total} dark assets total")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
