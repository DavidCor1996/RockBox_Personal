#!/usr/bin/env python3
"""Generate RockPod weather icon BMP assets."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


OUT = Path("rockpod/assets/weather/icons")
SIZES = (40, 64)


def _new(size: int) -> Image.Image:
    return Image.new("RGB", (size, size), (248, 0, 248))


def _scale(size: int) -> float:
    return size / 64.0


def _ellipse(draw: ImageDraw.ImageDraw, box, fill, outline=None, width=1):
    draw.ellipse(tuple(int(v) for v in box), fill=fill, outline=outline, width=width)


def _line(draw: ImageDraw.ImageDraw, points, fill, width=2):
    draw.line(tuple((int(x), int(y)) for x, y in points), fill=fill, width=width)


def _sun(draw: ImageDraw.ImageDraw, s: float, cx=24, cy=24):
    for i in range(8):
        angle = math.pi * 2 * i / 8
        r1 = 18 * s
        r2 = 25 * s
        x1 = cx * s + math.cos(angle) * r1
        y1 = cy * s + math.sin(angle) * r1
        x2 = cx * s + math.cos(angle) * r2
        y2 = cy * s + math.sin(angle) * r2
        _line(draw, ((x1, y1), (x2, y2)), (255, 190, 48), max(1, int(3 * s)))
    _ellipse(draw, (cx * s - 14 * s, cy * s - 14 * s,
                    cx * s + 14 * s, cy * s + 14 * s),
             (255, 205, 64), (255, 244, 155), max(1, int(2 * s)))
    _ellipse(draw, (cx * s - 9 * s, cy * s - 11 * s,
                    cx * s + 4 * s, cy * s + 2 * s),
             (255, 229, 102))


def _moon(draw: ImageDraw.ImageDraw, size: int, s: float):
    _ellipse(draw, (17 * s, 7 * s, 48 * s, 39 * s),
             (245, 240, 190), (255, 252, 224), max(1, int(2 * s)))
    _ellipse(draw, (30 * s, 3 * s, 57 * s, 34 * s), (248, 0, 248))
    for x, y, r in ((13, 10, 2), (50, 15, 2), (42, 46, 2)):
        _ellipse(draw, ((x - r) * s, (y - r) * s, (x + r) * s, (y + r) * s),
                 (232, 238, 255))


def _cloud(draw: ImageDraw.ImageDraw, s: float, x=11, y=26):
    shadow = (162, 174, 190)
    main = (237, 241, 247)
    edge = (202, 210, 222)
    draw.rounded_rectangle((x * s, (y + 2) * s, (x + 42) * s, (y + 18) * s),
                           radius=int(8 * s), fill=shadow)
    draw.rounded_rectangle((x * s, y * s, (x + 42) * s, (y + 16) * s),
                           radius=int(8 * s), fill=main, outline=edge,
                           width=max(1, int(2 * s)))
    _ellipse(draw, ((x + 7) * s, (y - 9) * s, (x + 25) * s, (y + 9) * s),
             main, edge, max(1, int(2 * s)))
    _ellipse(draw, ((x + 20) * s, (y - 13) * s, (x + 40) * s, (y + 9) * s),
             main, edge, max(1, int(2 * s)))


def _rain(draw: ImageDraw.ImageDraw, s: float):
    for x in (22, 32, 42):
        _line(draw, ((x * s, 45 * s), ((x - 4) * s, 55 * s)),
              (58, 156, 245), max(1, int(3 * s)))


def _snow(draw: ImageDraw.ImageDraw, s: float):
    for x in (22, 33, 44):
        y = 50
        col = (236, 246, 255)
        _line(draw, (((x - 4) * s, y * s), ((x + 4) * s, y * s)), col,
              max(1, int(2 * s)))
        _line(draw, ((x * s, (y - 4) * s), (x * s, (y + 4) * s)), col,
              max(1, int(2 * s)))
        _line(draw, (((x - 3) * s, (y - 3) * s),
                     ((x + 3) * s, (y + 3) * s)), col, max(1, int(1 * s)))
        _line(draw, (((x + 3) * s, (y - 3) * s),
                     ((x - 3) * s, (y + 3) * s)), col, max(1, int(1 * s)))


def _fog(draw: ImageDraw.ImageDraw, s: float):
    for y in (40, 47, 54):
        _line(draw, ((12 * s, y * s), (52 * s, y * s)),
              (190, 198, 210), max(1, int(3 * s)))


def _bolt(draw: ImageDraw.ImageDraw, s: float):
    pts = [(34 * s, 38 * s), (25 * s, 56 * s), (35 * s, 52 * s),
           (30 * s, 64 * s), (47 * s, 44 * s), (37 * s, 47 * s)]
    draw.polygon(pts, fill=(255, 202, 54))


def _render(kind: str, size: int) -> Image.Image:
    canvas = _new(size * 4)
    draw = ImageDraw.Draw(canvas)
    s = _scale(size * 4)
    if kind == "clear_day":
        _sun(draw, s, 32, 32)
    elif kind == "clear_night":
        _moon(draw, size * 4, s)
    elif kind == "partly_cloudy":
        _sun(draw, s, 24, 23)
        _cloud(draw, s, 13, 31)
    elif kind == "cloudy":
        _cloud(draw, s, 11, 28)
        _cloud(draw, s, 6, 35)
    elif kind == "rain" or kind == "drizzle":
        _cloud(draw, s, 11, 24)
        _rain(draw, s)
    elif kind == "snow":
        _cloud(draw, s, 11, 24)
        _snow(draw, s)
    elif kind == "fog":
        _cloud(draw, s, 11, 20)
        _fog(draw, s)
    elif kind == "thunderstorm":
        _cloud(draw, s, 11, 22)
        _rain(draw, s)
        _bolt(draw, s)
    else:
        _cloud(draw, s, 11, 28)
    canvas = canvas.filter(ImageFilter.SMOOTH_MORE)
    return canvas.resize((size, size), Image.Resampling.LANCZOS)


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    kinds = (
        "clear_day",
        "clear_night",
        "partly_cloudy",
        "cloudy",
        "rain",
        "drizzle",
        "snow",
        "fog",
        "thunderstorm",
        "unknown",
    )
    count = 0
    for size in SIZES:
        for kind in kinds:
            image = _render(kind, size)
            image.save(OUT / f"{kind}.{size}x{size}x24.bmp", "BMP")
            count += 1
    print(f"generated {count} weather icons in {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
