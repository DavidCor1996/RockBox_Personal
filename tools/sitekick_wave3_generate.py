#!/usr/bin/env python3
"""Generate the RockPod wave-3 Sitekick chip source art (Zelda/Link set).

Same procedurally-drawn, non-hand-drawn approach as wave 2: primitive
shapes at a supersampled canvas, chunky dark-purple outlines, flat cel
shading, and the existing YTV palette. Every piece is an original cartoon
homage to the "hero of time" archetype (green tunic, pointed cap, shield,
triforce, ocarina, heart container, a small fairy companion) built from
generic shapes -- no copied logos, no ripped game assets.

Usage:
    sitekick_wave3_generate.py [--out <generated-dir>]
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

from sitekick_wave2_generate import (
    CYAN,
    GREEN,
    ORANGE,
    OUTLINE,
    PINK,
    PURPLE,
    SCALE,
    WHITE,
    YELLOW,
    canvas,
)

OUT_DEFAULT = (Path(__file__).resolve().parent.parent /
               "assets/ipodjs/sources/sitekick/ipod-exclusive/generated")


def hero_cap() -> Image.Image:
    img, d, s = canvas((84, 56))
    w, h = img.size
    lw = 5 * s
    d.polygon(
        [(10 * s, h - 6 * s), (w - 10 * s, h - 6 * s),
         (w // 2 + 6 * s, 6 * s), (w // 2, 4 * s), (w // 2 - 6 * s, 6 * s)],
        fill=GREEN, outline=OUTLINE,
    )
    d.line([(10 * s, h - 6 * s), (w // 2 - 6 * s, 6 * s), (w // 2, 4 * s),
           (w // 2 + 6 * s, 6 * s), (w - 10 * s, h - 6 * s)],
          fill=OUTLINE, width=lw, joint="curve")
    d.rounded_rectangle((4 * s, h - 12 * s, w - 4 * s, h - 2 * s),
                        radius=4 * s, fill=GREEN, outline=OUTLINE, width=lw)
    d.ellipse((w // 2 - 5 * s, 2 * s, w // 2 + 5 * s, 12 * s), fill=YELLOW,
              outline=OUTLINE, width=int(3 * s))
    return img


def hero_tunic() -> Image.Image:
    img, d, s = canvas((96, 80))
    w, h = img.size
    lw = 6 * s
    body = [(20 * s, 2 * s), (w - 20 * s, 2 * s), (w - 6 * s, h - 2 * s),
            (6 * s, h - 2 * s)]
    d.polygon(body, fill=GREEN, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    d.rectangle((10 * s, h // 2 - 6 * s, w - 10 * s, h // 2 + 6 * s),
               fill=(139, 94, 52, 255), outline=OUTLINE, width=int(3 * s))
    d.ellipse((w // 2 - 6 * s, h // 2 - 6 * s, w // 2 + 6 * s,
              h // 2 + 6 * s), fill=YELLOW, outline=OUTLINE,
             width=int(2.5 * s))
    return img


def hylian_shield() -> Image.Image:
    img, d, s = canvas((54, 66))
    w, h = img.size
    lw = int(4 * s)
    shield = [(w // 2, 2 * s), (w - 4 * s, 12 * s), (w - 4 * s, h - 20 * s),
             (w // 2, h - 2 * s), (4 * s, h - 20 * s), (4 * s, 12 * s)]
    d.polygon(shield, fill=CYAN, outline=OUTLINE)
    d.line(shield + [shield[0]], fill=OUTLINE, width=lw, joint="curve")
    d.polygon([(w // 2, 10 * s), (w - 10 * s, 16 * s), (w // 2, h - 12 * s),
              (10 * s, 16 * s)], fill=ORANGE, outline=OUTLINE,
             width=int(2.5 * s))
    tri_h = 8 * s
    cx = w // 2
    d.polygon([(cx, 20 * s), (cx - 6 * s, 20 * s + tri_h),
              (cx + 6 * s, 20 * s + tri_h)], fill=YELLOW, outline=OUTLINE,
             width=int(2 * s))

    sword = Image.new("RGBA", (int(10 * s), int(46 * s)), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sword)
    sw = sword.size[0]
    sd.polygon([(sw // 2, 0), (sw - 1, 30 * s), (0, 30 * s)], fill=WHITE,
              outline=OUTLINE)
    sd.rectangle((1 * s, 30 * s, sw - 1 * s, 34 * s), fill=(151, 72, 181, 255),
                outline=OUTLINE, width=int(1.5 * s))
    sd.rectangle((sw // 2 - 1 * s, 34 * s, sw // 2 + 1 * s, sword.size[1] - 1),
                fill=(90, 60, 40, 255))
    sword = sword.rotate(-16, expand=True, resample=Image.BICUBIC)
    img.alpha_composite(sword, (w - sword.width + 2 * s, h - sword.height))
    return img


def forest_eye_mask() -> Image.Image:
    img, d, s = canvas((46, 20))
    w, h = img.size
    lw = int(3 * s)
    d.rounded_rectangle((4 * s, 2 * s, w - 4 * s, h - 2 * s), radius=6 * s,
                        fill=GREEN, outline=OUTLINE, width=lw)
    for cx in (w // 3, 2 * w // 3):
        d.ellipse((cx - 5 * s, 5 * s, cx + 5 * s, h - 5 * s), fill=WHITE,
                  outline=OUTLINE, width=int(2 * s))
        d.ellipse((cx - 2 * s, h // 2 - 2 * s, cx + 2 * s, h // 2 + 2 * s),
                  fill=(54, 8, 70, 255))
    return img


def ocarina_charm() -> Image.Image:
    canvas_img = Image.new("RGBA", (50 * SCALE, 58 * SCALE), (0, 0, 0, 0))
    d = ImageDraw.Draw(canvas_img)
    s = SCALE
    cx, cy = 21 * s, 30 * s
    d.ellipse((cx - 16 * s, cy - 16 * s, cx + 16 * s, cy + 16 * s),
              fill=CYAN, outline=OUTLINE, width=int(3.5 * s))
    d.polygon([(cx + 8 * s, cy - 10 * s), (cx + 22 * s, cy - 18 * s),
              (cx + 24 * s, cy - 8 * s)], fill=CYAN, outline=OUTLINE,
             width=int(3 * s))
    for hx, hy in ((cx - 6 * s, cy - 2 * s), (cx, cy + 2 * s),
                  (cx + 6 * s, cy - 2 * s), (cx - 2 * s, cy + 8 * s)):
        d.ellipse((hx - 2 * s, hy - 2 * s, hx + 2 * s, hy + 2 * s),
                  fill=OUTLINE)
    return canvas_img


def heart_container() -> Image.Image:
    img, d, s = canvas((46, 60))
    w, h = img.size
    lw = int(4 * s)
    cx = w // 2
    d.pieslice((6 * s, 6 * s, cx + 4 * s, h - 14 * s), 130, 320, fill=PINK,
              outline=OUTLINE, width=lw)
    d.pieslice((cx - 4 * s, 6 * s, w - 6 * s, h - 14 * s), 220, 50,
              fill=PINK, outline=OUTLINE, width=lw)
    d.polygon([(8 * s, h // 2), (cx, h - 6 * s), (w - 8 * s, h // 2)],
              fill=PINK, outline=OUTLINE)
    d.line([(8 * s, h // 2), (cx, h - 6 * s), (w - 8 * s, h // 2)],
          fill=OUTLINE, width=lw, joint="curve")
    d.ellipse((cx - 6 * s, 14 * s, cx + 2 * s, 22 * s), fill=WHITE)
    return img


def triforce_halo() -> Image.Image:
    img, d, s = canvas((140, 96))
    w, h = img.size
    cx, cy = w // 2, h // 2 + 4 * s
    size = 34 * s
    lw = int(4 * s)

    def tri(x, y, sz, fill):
        d.polygon([(x, y - sz), (x - sz, y + sz), (x + sz, y + sz)],
                  fill=fill, outline=OUTLINE)
        d.line([(x, y - sz), (x - sz, y + sz), (x + sz, y + sz), (x, y - sz)],
              fill=OUTLINE, width=lw, joint="curve")

    tri(cx, cy - size * 0.55, size * 0.62, YELLOW)
    tri(cx - size * 0.62, cy + size * 0.35, size * 0.62, YELLOW)
    tri(cx + size * 0.62, cy + size * 0.35, size * 0.62, YELLOW)

    glow_r = size * 1.9
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((cx - glow_r, cy - glow_r * 0.8, cx + glow_r,
               cy + glow_r * 0.8), fill=(*YELLOW[:3], 70))
    out = Image.alpha_composite(glow, img)
    return out


def fairy_companion() -> Image.Image:
    img, d, s = canvas((100, 86))
    w, h = img.size
    trail = [(w * 0.18, h * 0.85), (w * 0.32, h * 0.62), (w * 0.5, h * 0.42),
            (w * 0.66, h * 0.24)]
    for i, (x, y) in enumerate(trail[:-1]):
        r = (5 - i) * s
        d.ellipse((x - r, y - r, x + r, y + r), fill=(*CYAN[:3], 130))
    fx, fy = trail[-1]
    r = 14 * s
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((fx - r * 1.8, fy - r * 1.8, fx + r * 1.8, fy + r * 1.8),
              fill=(*YELLOW[:3], 90))
    img = Image.alpha_composite(glow, img)
    d = ImageDraw.Draw(img)
    for ang in (0, 90, 180, 270):
        rad = math.radians(ang)
        wx = fx + math.cos(rad) * 9 * s
        wy = fy + math.sin(rad) * 9 * s
        d.polygon([(fx, fy), (wx + 5 * s, wy - 3 * s), (wx, wy),
                  (wx - 5 * s, wy + 3 * s)], fill=(*WHITE[:3], 210),
                 outline=OUTLINE)
    d.ellipse((fx - 5 * s, fy - 5 * s, fx + 5 * s, fy + 5 * s), fill=YELLOW,
              outline=OUTLINE, width=int(2 * s))
    return img


GENERATORS = {
    "hero-cap.png": hero_cap,
    "hero-tunic.png": hero_tunic,
    "hylian-shield.png": hylian_shield,
    "forest-eye-mask.png": forest_eye_mask,
    "ocarina-charm.png": ocarina_charm,
    "heart-container.png": heart_container,
    "triforce-halo.png": triforce_halo,
    "fairy-companion.png": fairy_companion,
}


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=OUT_DEFAULT)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    for filename, fn in GENERATORS.items():
        img = fn()
        path = args.out / filename
        img.save(path)
        print(f"wrote {path} {img.size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
