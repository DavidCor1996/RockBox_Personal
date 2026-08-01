#!/usr/bin/env python3
"""Generate RockPod wave-9 Sitekick source art: more Cyberpunk 2077
(Johnny Silverhand, Judy Alvarez) and more Killer Instinct (Spinal).

Same procedurally-drawn, non-hand-drawn approach as every prior wave:
primitive shapes at a supersampled canvas, chunky dark-purple outlines,
flat cel shading, and the existing YTV palette. No portraits and no
traced game textures -- these are generic rockerboy/mechanic/pirate-
skeleton props evoking each character's silhouette and color scheme,
same restraint as every prior character-inspired wave.

Usage:
    sitekick_wave9_generate.py [--out <generated-dir>]
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
ORANGE_FLAME = (255, 137, 29, 255)
CHROME = (210, 218, 226, 255)
DARK_METAL = (50, 50, 58, 255)
BLACK = (30, 24, 32, 255)
NEON_YELLOW = (255, 232, 90, 255)
JUDY_PINK = (233, 90, 170, 255)
JUDY_PURPLE = (120, 70, 170, 255)
GOGGLE_ORANGE = (255, 170, 40, 255)
BONE = (238, 230, 214, 255)
GHOST_BLUE = (90, 170, 230, 255)


# ---------------------------------------------------------------------------
# Johnny Silverhand: jacket, chrome arm, aviators, guitar
# ---------------------------------------------------------------------------

def silverhand_jacket() -> Image.Image:
    img, d, s = canvas((96, 80))
    w, h = img.size
    lw = 6 * s
    body = [(20 * s, 2 * s), (w - 20 * s, 2 * s), (w - 6 * s, h - 2 * s),
            (6 * s, h - 2 * s)]
    d.polygon(body, fill=BLACK, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    d.line((w // 2, 4 * s, w // 2, h - 6 * s), fill=(60, 54, 62, 255),
          width=int(2 * s))
    flame = [(10 * s, h - 6 * s), (18 * s, h - 26 * s), (12 * s, h - 40 * s),
             (22 * s, h - 24 * s), (16 * s, h - 6 * s)]
    d.polygon(flame, fill=ORANGE_FLAME, outline=OUTLINE, width=int(2 * s))
    d.polygon([(w // 2 - 12 * s, 4 * s), (w // 2 + 12 * s, 4 * s),
              (w // 2, 18 * s)], fill=(60, 54, 62, 255), outline=OUTLINE,
             width=int(2 * s))
    return img


def chrome_rock_arm() -> Image.Image:
    img, d, s = canvas((60, 90))
    w, h = img.size
    lw = int(3.5 * s)
    d.rounded_rectangle((14 * s, 2 * s, w - 14 * s, 36 * s), radius=6 * s,
                        fill=CHROME, outline=OUTLINE, width=lw)
    d.rounded_rectangle((12 * s, 36 * s, w - 12 * s, h - 16 * s),
                        radius=8 * s, fill=DARK_METAL, outline=OUTLINE,
                        width=lw)
    d.ellipse((10 * s, h - 20 * s, w - 10 * s, h - 2 * s), fill=CHROME,
              outline=OUTLINE, width=lw)
    bolt = [(w // 2 + 4 * s, 8 * s), (w // 2 - 4 * s, 20 * s),
            (w // 2 + 1 * s, 20 * s), (w // 2 - 5 * s, 34 * s),
            (w // 2 + 6 * s, 18 * s), (w // 2 + 1 * s, 18 * s)]
    d.polygon(bolt, fill=NEON_YELLOW, outline=OUTLINE, width=int(1.5 * s))
    return img


def aviator_shades() -> Image.Image:
    img, d, s = canvas((46, 20))
    w, h = img.size
    lw = int(2.5 * s)
    d.line((w // 2 - 3 * s, 2 * s, w // 2 + 3 * s, 2 * s), fill=GOLD,
          width=int(3 * s))
    for cx in (13 * s, w - 13 * s):
        d.ellipse((cx - 9 * s, 4 * s, cx + 9 * s, h - 2 * s),
                  fill=(120, 160, 210, 235), outline=(*YELLOW[:3], 255),
                  width=lw)
        d.line((cx - 5 * s, 7 * s, cx - 1 * s, 9 * s), fill=WHITE,
              width=int(2 * s))
    return img


GOLD = (222, 178, 60, 255)


def rockerboy_guitar() -> Image.Image:
    img, d, s = canvas((60, 84))
    w, h = img.size
    lw = int(3 * s)
    d.line((w // 2, 0, w // 2, 24 * s), fill=(60, 40, 30, 255),
          width=int(3 * s))
    d.rectangle((w // 2 - 2 * s, 0, w // 2 + 2 * s, 4 * s), fill=WHITE)
    body = [
        (w // 2, 20 * s),
        (w - 8 * s, 24 * s), (w - 4 * s, 32 * s), (w - 6 * s, 42 * s),
        (w - 10 * s, 56 * s), (w - 18 * s, h - 4 * s),
        (18 * s, h - 4 * s), (10 * s, 56 * s),
        (6 * s, 42 * s), (4 * s, 32 * s), (8 * s, 24 * s),
    ]
    d.polygon(body, fill=RED, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    d.rectangle((w // 2 - 9 * s, 38 * s, w // 2 + 9 * s, 45 * s),
               fill=BLACK, outline=OUTLINE, width=int(2 * s))
    d.rectangle((w // 2 - 9 * s, 56 * s, w // 2 + 9 * s, 63 * s),
               fill=BLACK, outline=OUTLINE, width=int(2 * s))
    for sx in (-6, -2, 2, 6):
        d.line((w // 2 + sx * s, 18 * s, w // 2 + sx * s, h - 8 * s),
              fill=(220, 220, 220, 190), width=int(1 * s))
    return img


# ---------------------------------------------------------------------------
# Judy Alvarez: twin braids, welding goggles, mechanic overalls
# ---------------------------------------------------------------------------

def judy_twin_braids() -> Image.Image:
    img, d, s = canvas((100, 90))
    w, h = img.size
    lw = int(5 * s)
    d.ellipse((16 * s, 4 * s, w - 16 * s, 50 * s), fill=JUDY_PURPLE,
              outline=OUTLINE, width=lw)

    def braid(cx: int) -> None:
        pts = [(cx, 30 * s), (cx - 6 * s, 46 * s), (cx + 4 * s, 60 * s),
               (cx - 6 * s, 74 * s), (cx + 2 * s, h - 2 * s)]
        d.line(pts, fill=JUDY_PURPLE, width=int(11 * s), joint="curve")
        d.line(pts, fill=OUTLINE, width=int(11 * s), joint="curve")
        d.line(pts, fill=JUDY_PURPLE, width=int(8 * s), joint="curve")
        for i in range(1, len(pts) - 1):
            d.ellipse((pts[i][0] - 5 * s, pts[i][1] - 3 * s,
                      pts[i][0] + 5 * s, pts[i][1] + 3 * s), fill=JUDY_PINK)

    braid(20 * s)
    braid(w - 20 * s)
    return img


def welding_goggles() -> Image.Image:
    img, d, s = canvas((46, 22))
    w, h = img.size
    lw = int(3 * s)
    d.rounded_rectangle((3 * s, 2 * s, w - 3 * s, h - 6 * s), radius=6 * s,
                        fill=(60, 54, 40, 255), outline=OUTLINE, width=lw)
    for cx in (14 * s, w - 14 * s):
        d.ellipse((cx - 7 * s, 4 * s, cx + 7 * s, h - 8 * s),
                  fill=GOGGLE_ORANGE, outline=OUTLINE, width=int(2 * s))
    d.rectangle((0, h - 6 * s, w, h - 3 * s), fill=(40, 36, 30, 255))
    return img


def mechanic_overalls() -> Image.Image:
    img, d, s = canvas((96, 80))
    w, h = img.size
    lw = 6 * s
    body = [(20 * s, 2 * s), (w - 20 * s, 2 * s), (w - 6 * s, h - 2 * s),
            (6 * s, h - 2 * s)]
    d.polygon(body, fill=JUDY_PURPLE, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    d.rectangle((6 * s, 46 * s, w - 6 * s, 56 * s), fill=(60, 40, 30, 255),
               outline=OUTLINE, width=int(2.5 * s))
    d.rectangle((w // 2 - 6 * s, 48 * s, w // 2 + 6 * s, 54 * s),
               fill=CHROME, outline=OUTLINE, width=int(1.5 * s))
    d.polygon([(w // 2 - 10 * s, 4 * s), (w // 2 + 10 * s, 4 * s),
              (w // 2, 16 * s)], fill=JUDY_PINK, outline=OUTLINE,
             width=int(2 * s))
    return img


# ---------------------------------------------------------------------------
# Spinal: skull mask, pirate bandana, twin cutlasses, ghost flame aura
# ---------------------------------------------------------------------------

def spinal_skull_mask() -> Image.Image:
    img, d, s = canvas((40, 28))
    w, h = img.size
    lw = int(2.5 * s)
    d.rounded_rectangle((6 * s, 2 * s, w - 6 * s, 16 * s), radius=6 * s,
                        fill=BONE, outline=OUTLINE, width=lw)
    d.rectangle((6 * s, 14 * s, w - 6 * s, h - 2 * s), fill=BONE,
               outline=OUTLINE, width=lw)
    for i in range(4):
        x0 = 9 * s + i * 6 * s
        d.line((x0, 16 * s, x0, h - 3 * s), fill=OUTLINE, width=int(1.5 * s))
    return img


def pirate_bandana() -> Image.Image:
    img, d, s = canvas((84, 58))
    w, h = img.size
    lw = 5 * s
    crown = [(10 * s, h - 8 * s), (w - 10 * s, h - 8 * s),
             (w // 2 + 6 * s, 6 * s), (w // 2, 4 * s),
             (w // 2 - 6 * s, 6 * s)]
    d.polygon(crown, fill=BLACK, outline=OUTLINE)
    d.line(crown + [crown[0]], fill=OUTLINE, width=lw, joint="curve")
    d.rounded_rectangle((4 * s, h - 14 * s, w - 4 * s, h - 4 * s),
                        radius=4 * s, fill=BLACK, outline=OUTLINE, width=lw)
    cx, cy = w // 2, h - 22 * s
    d.ellipse((cx - 8 * s, cy - 8 * s, cx + 8 * s, cy + 8 * s), fill=BONE,
              outline=OUTLINE, width=int(2 * s))
    d.ellipse((cx - 4 * s, cy - 3 * s, cx - 1 * s, cy), fill=OUTLINE)
    d.ellipse((cx + 1 * s, cy - 3 * s, cx + 4 * s, cy), fill=OUTLINE)
    return img


def twin_cutlasses() -> Image.Image:
    img, d, s = canvas((110, 70))
    w, h = img.size
    lw = int(2.5 * s)

    def cutlass(flip: bool) -> Image.Image:
        piece = Image.new("RGBA", (int(26 * s), int(58 * s)), (0, 0, 0, 0))
        pd = ImageDraw.Draw(piece)
        pw, ph = piece.size
        blade = [(pw // 2 - 2 * s, 0), (pw - 2 * s, 6 * s),
                 (pw - 4 * s, 20 * s), (pw - 10 * s, 32 * s),
                 (pw // 2 + 4 * s, 40 * s), (pw // 2 - 4 * s, 40 * s),
                 (4 * s, 34 * s), (2 * s, 20 * s), (6 * s, 8 * s)]
        pd.polygon(blade, fill=CHROME, outline=OUTLINE)
        pd.line(blade + [blade[0]], fill=OUTLINE, width=lw, joint="curve")
        pd.line((pw // 2 - 2 * s, 6 * s, pw // 2 - 6 * s, 32 * s),
               fill=WHITE, width=int(1.5 * s))
        pd.arc((0, 32 * s, pw, 48 * s), 20, 160, fill=GOLD, width=int(3 * s))
        pd.rectangle((pw // 2 - 3 * s, 38 * s, pw // 2 + 3 * s, ph),
                    fill=(60, 40, 30, 255), outline=OUTLINE,
                    width=int(1.5 * s))
        if flip:
            piece = piece.transpose(Image.FLIP_LEFT_RIGHT)
        return piece

    left = cutlass(False).rotate(24, expand=True, resample=Image.BICUBIC)
    right = cutlass(True).rotate(-24, expand=True, resample=Image.BICUBIC)
    img.alpha_composite(left, (2 * s, h - left.height))
    img.alpha_composite(right, (w - right.width - 2 * s, h - right.height))
    return img


def ghost_flame_aura() -> Image.Image:
    img, d, s = canvas((150, 100))
    w, h = img.size
    cx, cy = w // 2, h - 10 * s
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((cx - 60 * s, cy - 70 * s, cx + 60 * s, cy + 10 * s),
              fill=(*GHOST_BLUE[:3], 80))
    img = Image.alpha_composite(glow, img)
    d = ImageDraw.Draw(img)
    for fx in (-34, -12, 12, 34):
        base_x = cx + fx * s
        flame = [(base_x, cy), (base_x - 8 * s, cy - 24 * s),
                 (base_x + 4 * s, cy - 40 * s), (base_x - 4 * s, cy - 60 * s),
                 (base_x + 8 * s, cy - 40 * s), (base_x + 8 * s, cy - 20 * s)]
        d.line(flame, fill=(*GHOST_BLUE[:3], 255), width=int(5 * s),
              joint="curve")
        d.line(flame, fill=(*WHITE[:3], 255), width=int(2 * s),
              joint="curve")
    return img


GENERATORS = {
    "silverhand-jacket.png": silverhand_jacket,
    "chrome-rock-arm.png": chrome_rock_arm,
    "aviator-shades.png": aviator_shades,
    "rockerboy-guitar.png": rockerboy_guitar,
    "judy-twin-braids.png": judy_twin_braids,
    "welding-goggles.png": welding_goggles,
    "mechanic-overalls.png": mechanic_overalls,
    "spinal-skull-mask.png": spinal_skull_mask,
    "pirate-bandana.png": pirate_bandana,
    "twin-cutlasses.png": twin_cutlasses,
    "ghost-flame-aura.png": ghost_flame_aura,
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
