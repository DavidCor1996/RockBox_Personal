#!/usr/bin/env python3
"""Generate RockPod wave-6 Sitekick source art: Pink Floyd, Portal 2, and
Old School RuneScape ("Runes") wearable/companion chips.

Same procedurally-drawn, non-hand-drawn approach as every prior wave:
primitive shapes at a supersampled canvas, chunky dark-purple outlines,
flat cel shading, and the existing YTV palette. Nothing here copies a real
game texture, film asset, or album cover -- the Pink Floyd pieces are a
generic floating pig silhouette, a prism/spectrum beam and crossed
hammers; the Portal 2 pieces are a generic ray-gun prop, a heart-marked
cube, chunky boots and a small companion turret; the RuneScape pieces are
original armor/weapon silhouettes in the games' iconic color schemes
(silvery blue, red-and-gold-on-black, ornate gold, black-and-red whip,
grey claws) rather than traced game art.

Usage:
    sitekick_wave6_generate.py [--out <generated-dir>]
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

from sitekick_wave2_generate import (
    CYAN,
    GREEN,
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
ORANGE = (255, 137, 29, 255)
SILVER = (198, 210, 224, 255)
STEEL_BLUE = (86, 128, 176, 255)
DRAGONHIDE = (60, 20, 26, 255)
GOLD = (222, 178, 60, 255)
BLACK = (30, 20, 34, 255)
GREY = (150, 150, 160, 255)


# ---------------------------------------------------------------------------
# Pink Floyd: flying pig, prism spectrum, marching hammers
# ---------------------------------------------------------------------------

def pink_floyd_pig() -> Image.Image:
    img, d, s = canvas((140, 90))
    w, h = img.size
    lw = int(4 * s)
    cx, cy = w // 2, h // 2
    body = (cx - 46 * s, cy - 24 * s, cx + 46 * s, cy + 24 * s)
    d.ellipse(body, fill=PINK, outline=OUTLINE, width=lw)
    d.polygon([(cx + 40 * s, cy - 6 * s), (cx + 58 * s, cy - 2 * s),
              (cx + 40 * s, cy + 10 * s)], fill=PINK, outline=OUTLINE)
    for ex in (cx - 20 * s, cx + 6 * s):
        d.polygon([(ex, cy - 22 * s), (ex + 10 * s, cy - 32 * s),
                  (ex + 16 * s, cy - 18 * s)], fill=PINK, outline=OUTLINE)
    snout = (cx - 62 * s, cy - 8 * s, cx - 44 * s, cy + 8 * s)
    d.ellipse(snout, fill=PINK, outline=OUTLINE, width=int(3 * s))
    for nx in (cx - 57 * s, cx - 50 * s):
        d.ellipse((nx - 2 * s, cy - 2 * s, nx + 2 * s, cy + 2 * s),
                  fill=OUTLINE)
    d.ellipse((cx - 32 * s, cy - 14 * s, cx - 24 * s, cy - 6 * s),
              fill=WHITE, outline=OUTLINE, width=int(2 * s))
    for lx in (cx - 8 * s, cx + 4 * s):
        d.line((lx, cy + 22 * s, lx - 4 * s, h - 4 * s), fill=OUTLINE,
              width=int(2.5 * s))
    return img


def pink_floyd_prism() -> Image.Image:
    img, d, s = canvas((150, 100))
    w, h = img.size
    lw = int(4 * s)
    tri = [(6 * s, h - 4 * s), (w // 2, 6 * s), (w - 6 * s, h - 4 * s)]
    d.polygon(tri, fill=(30, 20, 34, 255), outline=OUTLINE)
    d.line(tri + [tri[0]], fill=OUTLINE, width=lw, joint="curve")
    beam_colors = ((214, 43, 55, 235), (255, 137, 29, 235),
                  (255, 216, 31, 235), (105, 196, 52, 235),
                  (60, 181, 218, 235), (151, 72, 181, 235))
    for i, color in enumerate(beam_colors):
        y0 = 14 * s + i * 12 * s
        d.polygon([(w - 8 * s, y0), (w, y0 - 6 * s), (w, y0 + 14 * s)],
                 fill=color)
    d.line((4 * s, h - 2 * s, w // 2 - 8 * s, 12 * s), fill=WHITE,
          width=int(3 * s))
    return img


def pink_floyd_hammers() -> Image.Image:
    img, d, s = canvas((64, 64))
    w, h = img.size
    lw = int(3 * s)

    def hammer(angle: float) -> Image.Image:
        piece = Image.new("RGBA", (int(20 * s), int(56 * s)), (0, 0, 0, 0))
        pd = ImageDraw.Draw(piece)
        pw, ph = piece.size
        pd.rectangle((pw // 2 - 2 * s, 14 * s, pw // 2 + 2 * s, ph),
                    fill=(90, 60, 40, 255), outline=OUTLINE,
                    width=int(1.5 * s))
        pd.rectangle((0, 0, pw, 16 * s), fill=RED, outline=OUTLINE,
                    width=lw)
        return piece.rotate(angle, expand=True, resample=Image.BICUBIC)

    left = hammer(28)
    right = hammer(-28)
    img.alpha_composite(left, (w // 2 - left.width, h // 2 - left.height // 2))
    img.alpha_composite(right, (w // 2, h // 2 - right.height // 2))
    return img


# ---------------------------------------------------------------------------
# Portal 2: portal gun, companion cube, long fall boots, sentry turret
# ---------------------------------------------------------------------------

def portal_gun() -> Image.Image:
    img, d, s = canvas((60, 50))
    w, h = img.size
    lw = int(3 * s)
    body = [(4 * s, 20 * s), (40 * s, 12 * s), (56 * s, 18 * s),
            (56 * s, 32 * s), (40 * s, 38 * s), (4 * s, 30 * s)]
    d.polygon(body, fill=WHITE, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    d.rectangle((8 * s, 30 * s, 18 * s, 46 * s), fill=(60, 60, 66, 255),
               outline=OUTLINE, width=int(2.5 * s))
    d.ellipse((44 * s, 16 * s, 54 * s, 26 * s), fill=(46, 120, 220, 255),
              outline=OUTLINE, width=int(2 * s))
    d.ellipse((44 * s, 26 * s, 54 * s, 36 * s), fill=ORANGE,
              outline=OUTLINE, width=int(2 * s))
    return img


def companion_cube() -> Image.Image:
    img, d, s = canvas((56, 56))
    w, h = img.size
    lw = int(4 * s)
    d.rounded_rectangle((4 * s, 4 * s, w - 4 * s, h - 4 * s), radius=4 * s,
                        fill=SILVER, outline=OUTLINE, width=lw)
    for cx, cy in ((14 * s, 14 * s), (w - 14 * s, 14 * s),
                  (14 * s, h - 14 * s), (w - 14 * s, h - 14 * s)):
        d.rectangle((cx - 5 * s, cy - 5 * s, cx + 5 * s, cy + 5 * s),
                   fill=(150, 160, 172, 255), outline=OUTLINE,
                   width=int(2 * s))
    cx, cy = w // 2, h // 2
    heart = [(cx, cy + 9 * s), (cx - 11 * s, cy - 2 * s),
             (cx - 5 * s, cy - 10 * s), (cx, cy - 4 * s),
             (cx + 5 * s, cy - 10 * s), (cx + 11 * s, cy - 2 * s)]
    d.polygon(heart, fill=PINK, outline=OUTLINE, width=int(2 * s))
    return img


def long_fall_boots() -> Image.Image:
    img, d, s = canvas((110, 60))
    w, h = img.size
    lw = int(3.5 * s)

    def boot(cx: int) -> None:
        d.rounded_rectangle((cx - 16 * s, 2 * s, cx + 16 * s, 30 * s),
                            radius=6 * s, fill=ORANGE, outline=OUTLINE,
                            width=lw)
        d.polygon([(cx - 16 * s, 24 * s), (cx + 24 * s, 24 * s),
                  (cx + 24 * s, h - 4 * s), (cx - 16 * s, h - 4 * s)],
                 fill=(60, 60, 66, 255), outline=OUTLINE)
        d.line((cx - 16 * s, 24 * s, cx + 24 * s, 24 * s), fill=OUTLINE,
              width=int(2.5 * s))
        d.line((cx - 10 * s, 8 * s, cx + 10 * s, 8 * s), fill=WHITE,
              width=int(2 * s))

    boot(28 * s)
    boot(78 * s)
    return img


def aperture_turret() -> Image.Image:
    img, d, s = canvas((54, 54))
    w, h = img.size
    lw = int(3 * s)
    d.rounded_rectangle((12 * s, 6 * s, w - 12 * s, 34 * s), radius=8 * s,
                        fill=WHITE, outline=OUTLINE, width=lw)
    d.ellipse((w // 2 - 6 * s, 14 * s, w // 2 + 6 * s, 26 * s), fill=RED,
              outline=OUTLINE, width=int(2 * s))
    for lx in (16 * s, w - 22 * s):
        d.rounded_rectangle((lx, 30 * s, lx + 6 * s, 44 * s), radius=2 * s,
                            fill=(90, 90, 96, 255), outline=OUTLINE,
                            width=int(2 * s))
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((w // 2 - 3 * s, -4 * s, w // 2 + 3 * s, 20 * s),
              fill=(*RED[:3], 90))
    return Image.alpha_composite(glow, img)


# ---------------------------------------------------------------------------
# Old School RuneScape: rune + dragon armor, godsword, whip, claws
# ---------------------------------------------------------------------------

def osrs_rune_platebody() -> Image.Image:
    img, d, s = canvas((96, 80))
    w, h = img.size
    lw = 6 * s
    body = [(20 * s, 2 * s), (w - 20 * s, 2 * s), (w - 6 * s, h - 2 * s),
            (6 * s, h - 2 * s)]
    d.polygon(body, fill=STEEL_BLUE, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    for y in (18 * s, 34 * s, 50 * s, 64 * s):
        d.line((12 * s, y, w - 12 * s, y), fill=SILVER, width=int(3 * s))
    d.polygon([(w // 2 - 10 * s, 4 * s), (w // 2 + 10 * s, 4 * s),
              (w // 2, 16 * s)], fill=SILVER, outline=OUTLINE,
             width=int(2 * s))
    return img


def osrs_dragon_platebody() -> Image.Image:
    img, d, s = canvas((96, 80))
    w, h = img.size
    lw = 6 * s
    body = [(20 * s, 2 * s), (w - 20 * s, 2 * s), (w - 6 * s, h - 2 * s),
            (6 * s, h - 2 * s)]
    d.polygon(body, fill=DRAGONHIDE, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    d.polygon([(w // 2, 6 * s), (w - 16 * s, 30 * s), (w // 2, 54 * s),
              (16 * s, 30 * s)], fill=RED, outline=GOLD, width=int(3 * s))
    d.polygon([(w // 2 - 10 * s, 4 * s), (w // 2 + 10 * s, 4 * s),
              (w // 2, 16 * s)], fill=GOLD, outline=OUTLINE,
             width=int(2 * s))
    return img


def osrs_godsword() -> Image.Image:
    img, d, s = canvas((100, 90))
    w, h = img.size
    blade = Image.new("RGBA", (int(20 * s), int(76 * s)), (0, 0, 0, 0))
    bd = ImageDraw.Draw(blade)
    bw, bh = blade.size
    bd.polygon([(bw // 2, 0), (bw - 1, 46 * s), (bw // 2 + 3 * s, 56 * s),
               (bw // 2 - 3 * s, 56 * s), (0, 46 * s)], fill=SILVER,
              outline=OUTLINE)
    bd.line((bw // 2, 4 * s, bw // 2, 44 * s), fill=WHITE,
           width=int(2 * s))
    bd.rectangle((2 * s, 56 * s, bw - 2 * s, 62 * s), fill=GOLD,
                outline=OUTLINE, width=int(2.5 * s))
    bd.rectangle((bw // 2 - 3 * s, 62 * s, bw // 2 + 3 * s, bh), fill=BLACK,
                outline=OUTLINE, width=int(1.5 * s))
    bd.ellipse((bw // 2 - 6 * s, bh - 10 * s, bw // 2 + 6 * s, bh),
              fill=GOLD, outline=OUTLINE, width=int(2 * s))
    blade = blade.rotate(-30, expand=True, resample=Image.BICUBIC)
    img.alpha_composite(blade, (w - blade.width, 0))
    return img


def osrs_abyssal_whip() -> Image.Image:
    img, d, s = canvas((60, 80))
    w, h = img.size
    handle = (10 * s, h - 20 * s, 20 * s, h - 2 * s)
    d.rounded_rectangle(handle, radius=3 * s, fill=(90, 60, 40, 255),
                        outline=OUTLINE, width=int(2.5 * s))
    path = []
    for i in range(14):
        t = i / 13.0
        x = 15 * s + math.sin(t * 5.0) * (18 * s * (1 - t * 0.3))
        y = (h - 20 * s) - t * (h - 30 * s)
        path.append((x, y))
    d.line(path, fill=BLACK, width=int(5 * s), joint="curve")
    for i in range(0, 14, 3):
        x, y = path[i]
        d.ellipse((x - 2.5 * s, y - 2.5 * s, x + 2.5 * s, y + 2.5 * s),
                  fill=RED)
    return img


def osrs_dragon_claws() -> Image.Image:
    img, d, s = canvas((110, 64))
    w, h = img.size
    lw = int(3 * s)

    def claw(cx: int) -> None:
        d.rounded_rectangle((cx - 10 * s, 30 * s, cx + 10 * s, h - 2 * s),
                            radius=5 * s, fill=(90, 60, 40, 255),
                            outline=OUTLINE, width=int(2.5 * s))
        for i, dx in enumerate((-10, 0, 10)):
            tip_x = cx + dx * s
            d.polygon([(tip_x - 3 * s, 30 * s), (tip_x + 3 * s, 30 * s),
                      (tip_x, 4 * s + (i % 2) * 4 * s)], fill=GREY,
                     outline=OUTLINE, width=lw)
        d.line((cx - 12 * s, 30 * s, cx + 12 * s, 30 * s), fill=GOLD,
              width=int(3 * s))

    claw(26 * s)
    claw(80 * s)
    return img


GENERATORS = {
    "pink-floyd-pig.png": pink_floyd_pig,
    "pink-floyd-prism.png": pink_floyd_prism,
    "pink-floyd-hammers.png": pink_floyd_hammers,
    "portal-gun.png": portal_gun,
    "companion-cube.png": companion_cube,
    "long-fall-boots.png": long_fall_boots,
    "aperture-turret.png": aperture_turret,
    "osrs-rune-platebody.png": osrs_rune_platebody,
    "osrs-dragon-platebody.png": osrs_dragon_platebody,
    "osrs-godsword.png": osrs_godsword,
    "osrs-abyssal-whip.png": osrs_abyssal_whip,
    "osrs-dragon-claws.png": osrs_dragon_claws,
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
