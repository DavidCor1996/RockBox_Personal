#!/usr/bin/env python3
"""Generate the RockPod wave-4 Sitekick chip source art (streamer set).

Same procedurally-drawn, non-hand-drawn approach as waves 2 and 3: primitive
shapes at a supersampled canvas, chunky dark-purple outlines, flat cel
shading, and the existing YTV palette. Nothing here is a portrait or a
likeness -- the HasanAbi set is a generic backwards cap, headset and hoodie
(no face, no name spelled out on the art), and the QTCinderella set is a
generic ponytail, cat-eye visor and awards trophy. Purely thematic homages,
same restraint as the Beatles/Oliver/Earl/Pokemon/Zelda sets before them.

Usage:
    sitekick_wave4_generate.py [--out <generated-dir>]
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw

from sitekick_wave2_generate import (
    CYAN,
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

RED = (214, 43, 55, 255)
BLONDE = (255, 204, 102, 255)


# ---------------------------------------------------------------------------
# HasanAbi set: backwards cap, debate headset, hoodie, breaking-news aura
# ---------------------------------------------------------------------------

def hasan_cap() -> Image.Image:
    img, d, s = canvas((84, 58))
    w, h = img.size
    lw = 5 * s
    crown = [(10 * s, h - 8 * s), (w - 10 * s, h - 8 * s),
             (w // 2 + 6 * s, 6 * s), (w // 2, 4 * s),
             (w // 2 - 6 * s, 6 * s)]
    d.polygon(crown, fill=RED, outline=OUTLINE)
    d.line(crown + [crown[0]], fill=OUTLINE, width=lw, joint="curve")
    d.rounded_rectangle((4 * s, h - 14 * s, w - 4 * s, h - 4 * s),
                        radius=4 * s, fill=RED, outline=OUTLINE, width=lw)
    d.ellipse((w // 2 - 4 * s, 2 * s, w // 2 + 4 * s, 10 * s), fill=OUTLINE)
    d.polygon([(2 * s, h - 10 * s), (14 * s, h - 10 * s),
              (10 * s, h - 2 * s), (0, h - 2 * s)], fill=OUTLINE)
    d.polygon([(w - 14 * s, h - 10 * s), (w - 2 * s, h - 10 * s),
              (w, h - 2 * s), (w - 10 * s, h - 2 * s)], fill=OUTLINE)
    return img


def hasan_headset() -> Image.Image:
    img, d, s = canvas((54, 66))
    w, h = img.size
    lw = int(3.5 * s)
    d.arc((4 * s, 2 * s, w - 4 * s, h - 26 * s), start=195, end=345,
         fill=OUTLINE, width=lw)
    d.ellipse((w - 20 * s, 6 * s, w - 2 * s, 26 * s), fill=PURPLE,
              outline=OUTLINE, width=lw)
    d.line((w - 11 * s, 18 * s, 16 * s, h - 16 * s), fill=OUTLINE,
          width=int(2.5 * s))
    d.ellipse((4 * s, h - 24 * s, 22 * s, h - 6 * s), fill=CYAN,
              outline=OUTLINE, width=lw)
    return img


def hasan_hoodie() -> Image.Image:
    img, d, s = canvas((96, 80))
    w, h = img.size
    lw = 6 * s
    body = [(20 * s, 2 * s), (w - 20 * s, 2 * s), (w - 6 * s, h - 2 * s),
            (6 * s, h - 2 * s)]
    d.polygon(body, fill=RED, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    d.rounded_rectangle((26 * s, 40 * s, w - 26 * s, 62 * s), radius=6 * s,
                        fill=OUTLINE)
    d.line((w // 2 - 6 * s, 10 * s, w // 2 - 8 * s, 32 * s), fill=WHITE,
          width=int(2 * s))
    d.line((w // 2 + 6 * s, 10 * s, w // 2 + 8 * s, 32 * s), fill=WHITE,
          width=int(2 * s))
    return img


def hasan_news_aura() -> Image.Image:
    img, d, s = canvas((150, 90))
    w, h = img.size
    cx, cy = w // 2, h // 2 - 6 * s
    r = min(w, h) // 2 - 4 * s
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(*RED[:3], 70))
    img = Image.alpha_composite(glow, img)
    d = ImageDraw.Draw(img)
    for ang in range(0, 360, 45):
        rad = math.radians(ang)
        x0 = cx + math.cos(rad) * r * 0.55
        y0 = cy + math.sin(rad) * r * 0.55
        x1 = cx + math.cos(rad) * r * 0.95
        y1 = cy + math.sin(rad) * r * 0.95
        d.line((x0, y0, x1, y1), fill=(*YELLOW[:3], 210), width=int(2 * s))
    bar_h = 14 * s
    d.rounded_rectangle((4 * s, h - bar_h - 4 * s, w - 4 * s, h - 4 * s),
                        radius=5 * s, fill=RED, outline=OUTLINE,
                        width=int(3 * s))
    for i, bw in enumerate((14, 10, 18, 8, 16)):
        x = 10 * s + i * 26 * s
        d.rectangle((x, h - bar_h + 2 * s, x + bw * s, h - 6 * s),
                    fill=WHITE)
    return img


# ---------------------------------------------------------------------------
# QTCinderella set: ponytail, cat-eye visor, awards trophy, spotlight aura
# ---------------------------------------------------------------------------

def qtc_ponytail() -> Image.Image:
    img, d, s = canvas((100, 94))
    w, h = img.size
    lw = 5 * s
    d.ellipse((14 * s, 4 * s, w - 14 * s, 60 * s), fill=BLONDE,
              outline=OUTLINE, width=lw)
    tail = [(w - 30 * s, 20 * s), (w - 6 * s, 30 * s), (w - 4 * s, 70 * s),
            (w - 20 * s, h - 4 * s), (w - 36 * s, 74 * s),
            (w - 28 * s, 40 * s)]
    d.polygon(tail, fill=BLONDE, outline=OUTLINE)
    d.line(tail + [tail[0]], fill=OUTLINE, width=lw, joint="curve")
    d.ellipse((w - 34 * s, 24 * s, w - 20 * s, 36 * s), fill=PINK,
              outline=OUTLINE, width=int(2.5 * s))
    return img


def qtc_visor() -> Image.Image:
    img, d, s = canvas((46, 20))
    w, h = img.size
    lw = int(3 * s)
    d.line((w // 2 - 4 * s, 4 * s, w // 2 + 4 * s, 4 * s), fill=CYAN,
          width=int(3 * s))
    for cx, tip in ((12 * s, -1), (w - 12 * s, 1)):
        lens = [(cx - 8 * s, h - 4 * s), (cx - 8 * s, 8 * s),
                (cx, 3 * s), (cx + 8 * s + tip * 4 * s, 6 * s),
                (cx + 8 * s, h - 4 * s)]
        d.polygon(lens, fill=PURPLE, outline=OUTLINE)
        d.line(lens + [lens[0]], fill=OUTLINE, width=lw, joint="curve")
    return img


def qtc_trophy() -> Image.Image:
    img, d, s = canvas((46, 58))
    w, h = img.size
    lw = int(3 * s)
    cup = [(10 * s, 6 * s), (w - 10 * s, 6 * s), (w - 14 * s, 26 * s),
           (14 * s, 26 * s)]
    d.polygon(cup, fill=YELLOW, outline=OUTLINE)
    d.line(cup + [cup[0]], fill=OUTLINE, width=lw, joint="curve")
    d.ellipse((0, 8 * s, 10 * s, 22 * s), outline=OUTLINE, width=int(2.5 * s))
    d.ellipse((w - 10 * s, 8 * s, w, 22 * s), outline=OUTLINE,
              width=int(2.5 * s))
    d.rectangle((w // 2 - 3 * s, 26 * s, w // 2 + 3 * s, 40 * s),
               fill=YELLOW, outline=OUTLINE, width=int(2 * s))
    base = [(14 * s, 40 * s), (w - 14 * s, 40 * s), (w - 10 * s, 52 * s),
            (10 * s, 52 * s)]
    d.polygon(base, fill=YELLOW, outline=OUTLINE)
    d.line(base + [base[0]], fill=OUTLINE, width=lw, joint="curve")
    d.rectangle((8 * s, 50 * s, w - 8 * s, 56 * s), fill=PURPLE,
               outline=OUTLINE, width=int(2 * s))
    return img


def qtc_spotlight_aura() -> Image.Image:
    img, d, s = canvas((140, 110))
    w, h = img.size
    cx, cy = w // 2, h - 10 * s
    cone = [(cx - 6 * s, 6 * s), (cx + 6 * s, 6 * s), (cx + 50 * s, h - 6 * s),
            (cx - 50 * s, h - 6 * s)]
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.polygon(cone, fill=(*YELLOW[:3], 90))
    img = Image.alpha_composite(glow, img)
    d = ImageDraw.Draw(img)
    d.line(cone + [cone[0]], fill=OUTLINE, width=int(2 * s), joint="curve")

    def star(cx: float, cy: float, r_out: float, r_in: float) -> None:
        pts = []
        for i in range(8):
            r = r_out if i % 2 == 0 else r_in
            ang = math.radians(i * 45 - 90)
            pts.append((cx + math.cos(ang) * r, cy + math.sin(ang) * r))
        d.polygon(pts, fill=WHITE, outline=OUTLINE)

    for sx, sy, r in ((30 * s, 20 * s, 6 * s), (112 * s, 28 * s, 5 * s),
                     (70 * s, 12 * s, 7 * s), (96 * s, 58 * s, 4 * s),
                     (20 * s, 58 * s, 4 * s)):
        star(sx, sy, r, r * 0.4)
    return img


GENERATORS = {
    "hasan-cap.png": hasan_cap,
    "hasan-headset.png": hasan_headset,
    "hasan-hoodie.png": hasan_hoodie,
    "hasan-news-aura.png": hasan_news_aura,
    "qtc-ponytail.png": qtc_ponytail,
    "qtc-visor.png": qtc_visor,
    "qtc-trophy.png": qtc_trophy,
    "qtc-spotlight-aura.png": qtc_spotlight_aura,
}


def main() -> int:
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
