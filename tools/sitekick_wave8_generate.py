#!/usr/bin/env python3
"""Generate RockPod wave-8 Sitekick source art: more Polaroid pieces, a
Killer Instinct set, a Cyberpunk 2077 set, and one new background per
theme.

Same procedurally-drawn, non-hand-drawn approach as every prior wave:
primitive shapes at a supersampled canvas, chunky dark-purple outlines,
flat cel shading, and the existing YTV palette. Nothing here copies a
real game/franchise logo or character render -- the Killer Instinct
pieces are a generic lightning "combo" burst, a glowing ninja visor and
energy blades; the Cyberpunk pieces are a generic neon visor, chrome
arm, retractable arm blades and a neon skyline glow.

Usage:
    sitekick_wave8_generate.py [--chips-out <dir>] [--bg-out <dir>]
"""

from __future__ import annotations

import math
import random
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

CHIPS_OUT_DEFAULT = (Path(__file__).resolve().parent.parent /
                     "assets/ipodjs/sources/sitekick/ipod-exclusive/generated")
BG_OUT_DEFAULT = (Path(__file__).resolve().parent.parent /
                  "assets/ipodjs/sources/sitekick/backgrounds")

RED = (214, 43, 55, 255)
SEPIA = (196, 154, 108, 255)
AMBER = (224, 168, 80, 255)
STEEL = (150, 160, 172, 255)
CHROME = (210, 218, 226, 255)
NEON_PINK = (240, 60, 170, 255)
NEON_CYAN = (60, 220, 230, 255)
DARK_METAL = (60, 60, 70, 255)


# ---------------------------------------------------------------------------
# Polaroid: camera, photo strip, filmstrip aura
# ---------------------------------------------------------------------------

def polaroid_camera() -> Image.Image:
    img, d, s = canvas((60, 60))
    w, h = img.size
    lw = int(3.5 * s)
    d.rounded_rectangle((4 * s, 14 * s, w - 4 * s, h - 4 * s), radius=5 * s,
                        fill=WHITE, outline=OUTLINE, width=lw)
    d.ellipse((w // 2 - 13 * s, 18 * s, w // 2 + 13 * s, 44 * s),
              fill=(40, 40, 46, 255), outline=OUTLINE, width=int(2.5 * s))
    d.ellipse((w // 2 - 8 * s, 23 * s, w // 2 + 8 * s, 39 * s), fill=CYAN)
    d.rectangle((8 * s, 6 * s, 24 * s, 16 * s), fill=RED, outline=OUTLINE,
               width=int(2 * s))
    d.ellipse((w - 16 * s, h - 14 * s, w - 8 * s, h - 6 * s), fill=YELLOW,
              outline=OUTLINE, width=int(2 * s))
    return img


def photo_strip() -> Image.Image:
    img, d, s = canvas((40, 70))
    w, h = img.size
    lw = int(3 * s)
    d.line((w // 2, 0, w // 2, 10 * s), fill=OUTLINE, width=int(2 * s))
    d.rounded_rectangle((4 * s, 8 * s, w - 4 * s, h - 2 * s), radius=3 * s,
                        fill=WHITE, outline=OUTLINE, width=lw)
    photo_colors = (SEPIA, AMBER, (170, 200, 210, 255))
    for i, color in enumerate(photo_colors):
        y0 = 12 * s + i * 18 * s
        d.rectangle((7 * s, y0, w - 7 * s, y0 + 14 * s), fill=color,
                   outline=OUTLINE, width=int(1.5 * s))
    return img


def retro_filmstrip() -> Image.Image:
    img, d, s = canvas((150, 100))
    w, h = img.size
    d.rectangle((0, 20 * s, w, h - 20 * s), fill=(20, 14, 26, 235))
    for x in range(6 * s, w, 12 * s):
        d.rectangle((x, 24 * s, x + 6 * s, 30 * s), fill=(50, 40, 56, 255))
        d.rectangle((x, h - 30 * s, x + 6 * s, h - 24 * s),
                   fill=(50, 40, 56, 255))
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((w * 0.3, h * 0.25, w * 0.7, h * 0.75), fill=(*AMBER[:3], 130))
    return Image.alpha_composite(glow, img)


# ---------------------------------------------------------------------------
# Killer Instinct: combo burst aura, ninja visor, energy blades
# ---------------------------------------------------------------------------

def ki_ultra_combo() -> Image.Image:
    img, d, s = canvas((150, 100))
    w, h = img.size
    cx, cy = w // 2, h // 2
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    r = min(w, h) // 2 - 6 * s
    gd.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(*RED[:3], 90))
    img = Image.alpha_composite(glow, img)
    d = ImageDraw.Draw(img)
    rng = random.Random(77)
    for _ in range(6):
        ang = rng.uniform(0, 360)
        rad = math.radians(ang)
        x0 = cx + math.cos(rad) * r * 0.2
        y0 = cy + math.sin(rad) * r * 0.2
        pts = [(x0, y0)]
        for i in range(4):
            t = (i + 1) / 4
            jitter = rng.uniform(-10, 10) * s
            pts.append((cx + math.cos(rad) * r * t + jitter,
                       cy + math.sin(rad) * r * t + jitter))
        d.line(pts, fill=YELLOW, width=int(3 * s), joint="curve")
    return img


def ki_ninja_visor() -> Image.Image:
    img, d, s = canvas((46, 20))
    w, h = img.size
    lw = int(3 * s)
    d.rounded_rectangle((4 * s, 3 * s, w - 4 * s, h - 3 * s), radius=6 * s,
                        fill=(30, 20, 34, 255), outline=OUTLINE, width=lw)
    for cx in (14 * s, w - 14 * s):
        d.ellipse((cx - 6 * s, 5 * s, cx + 6 * s, h - 5 * s), fill=RED,
                  outline=OUTLINE, width=int(2 * s))
    return img


def ki_energy_blades() -> Image.Image:
    img, d, s = canvas((110, 70))
    w, h = img.size
    lw = int(2.5 * s)

    def blade(cx: int, flip: bool) -> None:
        tip = cx + (18 if not flip else -18) * s
        pts = [(cx, 8 * s), (tip, 20 * s), (cx + (6 if not flip else -6) * s,
               60 * s), (cx, 68 * s), (cx - (6 if not flip else -6) * s,
               60 * s)]
        d.polygon(pts, fill=(*CYAN[:3], 235), outline=OUTLINE, width=lw)
        d.line((cx, 12 * s, cx, 58 * s), fill=WHITE, width=int(1.5 * s))
        d.rectangle((cx - 6 * s, 58 * s, cx + 6 * s, h - 2 * s),
                   fill=(60, 40, 70, 255), outline=OUTLINE,
                   width=int(2 * s))

    blade(30 * s, False)
    blade(80 * s, True)
    return img


# ---------------------------------------------------------------------------
# Cyberpunk 2077: neon visor, chrome cyberarm, mantis blades, night city
# ---------------------------------------------------------------------------

def cyberpunk_neon_visor() -> Image.Image:
    img, d, s = canvas((46, 20))
    w, h = img.size
    lw = int(3 * s)
    d.rounded_rectangle((3 * s, 4 * s, w - 3 * s, h - 4 * s), radius=6 * s,
                        fill=(20, 14, 26, 255), outline=OUTLINE, width=lw)
    d.line((6 * s, 10 * s, w - 6 * s, 10 * s), fill=NEON_CYAN,
          width=int(2.5 * s))
    d.line((6 * s, h - 8 * s, w - 6 * s, h - 8 * s), fill=NEON_PINK,
          width=int(2.5 * s))
    return img


def cyberpunk_cyberarm() -> Image.Image:
    img, d, s = canvas((60, 90))
    w, h = img.size
    lw = int(3.5 * s)
    d.rounded_rectangle((14 * s, 2 * s, w - 14 * s, 40 * s), radius=6 * s,
                        fill=CHROME, outline=OUTLINE, width=lw)
    d.rounded_rectangle((10 * s, 40 * s, w - 10 * s, h - 4 * s), radius=8 * s,
                        fill=DARK_METAL, outline=OUTLINE, width=lw)
    for y in (14 * s, 24 * s, 34 * s):
        d.line((16 * s, y, w - 16 * s, y), fill=NEON_CYAN,
              width=int(2 * s))
    for y in (52 * s, 68 * s, 84 * s):
        d.line((14 * s, y, w - 14 * s, y), fill=NEON_PINK,
              width=int(1.5 * s))
    return img


def cyberpunk_mantis_blades() -> Image.Image:
    img, d, s = canvas((70, 60))
    w, h = img.size
    lw = int(2.5 * s)

    def blade(angle: float) -> Image.Image:
        piece = Image.new("RGBA", (int(14 * s), int(50 * s)), (0, 0, 0, 0))
        pd = ImageDraw.Draw(piece)
        pw, ph = piece.size
        pd.polygon([(pw // 2, 0), (pw - 1, 14 * s), (pw // 2 + 2 * s, ph),
                   (pw // 2 - 2 * s, ph), (0, 14 * s)], fill=DARK_METAL,
                  outline=OUTLINE, width=lw)
        pd.line((pw // 2, 4 * s, pw // 2, ph - 4 * s), fill=NEON_CYAN,
               width=int(1.5 * s))
        return piece.rotate(angle, expand=True, resample=Image.BICUBIC)

    b1, b2 = blade(20), blade(-6)
    img.alpha_composite(b1, (4 * s, h - b1.height))
    img.alpha_composite(b2, (w - b2.width - 4 * s, h - b2.height))
    return img


def cyberpunk_night_city() -> Image.Image:
    img, d, s = canvas((150, 100))
    w, h = img.size
    margin = 6 * s
    d.rounded_rectangle((margin, margin, w - margin, h - margin),
                        radius=6 * s, fill=(16, 10, 28, 255))
    rng = random.Random(42)
    for _ in range(9):
        bx = rng.randint(int(margin), int(w - margin - 14 * s))
        bh = rng.randint(int(16 * s), int(46 * s))
        bw = rng.randint(int(8 * s), int(14 * s))
        by = (h - margin - 16 * s) - bh
        color = rng.choice((NEON_CYAN, NEON_PINK, PURPLE))
        d.rectangle((bx, by, bx + bw, h - margin - 16 * s),
                   fill=(24, 18, 34, 255), outline=(*color[:3], 200),
                   width=int(1.5 * s))
        for wy in range(int(by + 4 * s), int(h - margin - 20 * s),
                       int(6 * s)):
            if rng.random() < 0.6:
                d.rectangle((bx + 2 * s, wy, bx + bw - 2 * s, wy + 2 * s),
                           fill=(*color[:3], 200))
    d.rectangle((margin, h - margin - 16 * s, w - margin, h - margin),
               fill=(10, 8, 18, 255))
    d.line((margin, h - margin - 16 * s, w - margin, h - margin - 16 * s),
          fill=(*NEON_PINK[:3], 200), width=int(2 * s))
    return img


CHIP_GENERATORS = {
    "polaroid-camera.png": polaroid_camera,
    "photo-strip.png": photo_strip,
    "retro-filmstrip.png": retro_filmstrip,
    "ki-ultra-combo.png": ki_ultra_combo,
    "ki-ninja-visor.png": ki_ninja_visor,
    "ki-energy-blades.png": ki_energy_blades,
    "cyberpunk-neon-visor.png": cyberpunk_neon_visor,
    "cyberpunk-cyberarm.png": cyberpunk_cyberarm,
    "cyberpunk-mantis-blades.png": cyberpunk_mantis_blades,
    "cyberpunk-night-city.png": cyberpunk_night_city,
}

BG_SIZE = 1200


def vertical_gradient(size: tuple[int, int], top: tuple[int, int, int],
                      bottom: tuple[int, int, int]) -> Image.Image:
    w, h = size
    img = Image.new("RGB", size)
    for y in range(h):
        t = y / max(1, h - 1)
        row = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        ImageDraw.Draw(img).line((0, y, w, y), fill=row)
    return img.convert("RGBA")


def polaroid_darkroom_bg() -> Image.Image:
    img = vertical_gradient((BG_SIZE, BG_SIZE), (54, 30, 20), (18, 10, 12))
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((BG_SIZE * 0.55, -BG_SIZE * 0.1, BG_SIZE * 1.1, BG_SIZE * 0.5),
              fill=(*RED[:3], 90))
    img = Image.alpha_composite(img, glow)
    d = ImageDraw.Draw(img)
    d.line((0, 90, BG_SIZE, 90), fill=(90, 60, 40, 255), width=6)
    rng = random.Random(9)
    colors = (SEPIA, AMBER, (170, 200, 210, 255), SEPIA, AMBER)
    for i, x in enumerate(range(80, BG_SIZE - 80, 220)):
        y0 = 90
        y1 = 90 + rng.randint(120, 200)
        d.line((x, y0, x, y1), fill=(200, 200, 200, 255), width=3)
        photo = colors[i % len(colors)]
        d.rectangle((x - 60, y1, x + 60, y1 + 100), fill=WHITE,
                   outline=(60, 40, 30, 255), width=6)
        d.rectangle((x - 50, y1 + 8, x + 50, y1 + 80), fill=photo)
    ground_y = int(BG_SIZE * 0.86)
    d.rectangle((0, ground_y, BG_SIZE, BG_SIZE), fill=(30, 18, 16, 255))
    return img


def ki_arena_lightning_bg() -> Image.Image:
    img = vertical_gradient((BG_SIZE, BG_SIZE), (18, 10, 30), (46, 12, 26))
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((BG_SIZE * 0.15, BG_SIZE * 0.1, BG_SIZE * 0.85,
               BG_SIZE * 0.7), fill=(*RED[:3], 70))
    img = Image.alpha_composite(img, glow)
    d = ImageDraw.Draw(img)
    rng = random.Random(303)
    for _ in range(7):
        x = rng.randint(0, BG_SIZE)
        y = 0
        pts = [(x, y)]
        while y < BG_SIZE * 0.7:
            x += rng.randint(-70, 70)
            y += rng.randint(60, 120)
            pts.append((x, y))
        d.line(pts, fill=YELLOW, width=8, joint="curve")
        d.line(pts, fill=(*WHITE[:3], 180), width=3, joint="curve")
    floor_y = int(BG_SIZE * 0.82)
    d.rectangle((0, floor_y, BG_SIZE, BG_SIZE), fill=(14, 8, 20, 255))
    d.rectangle((0, floor_y, BG_SIZE, floor_y + 10), fill=(*RED[:3], 220))
    return img


def cyberpunk_night_city_bg() -> Image.Image:
    img = vertical_gradient((BG_SIZE, BG_SIZE), (18, 10, 36), (40, 14, 44))
    d = ImageDraw.Draw(img)
    rng = random.Random(2077)
    horizon = int(BG_SIZE * 0.78)
    for _ in range(16):
        bw = rng.randint(60, 140)
        bx = rng.randint(0, BG_SIZE - bw)
        bh = rng.randint(160, 520)
        by = horizon - bh
        d.rectangle((bx, by, bx + bw, horizon), fill=(20, 14, 32, 255),
                   outline=(60, 40, 70, 255), width=4)
        color = rng.choice((NEON_CYAN, NEON_PINK, PURPLE, CYAN))
        for wy in range(by + 10, horizon - 10, 22):
            for wx in range(bx + 8, bx + bw - 8, 18):
                if rng.random() < 0.35:
                    d.rectangle((wx, wy, wx + 8, wy + 12),
                               fill=(*color[:3], 220))
        if rng.random() < 0.5:
            d.rectangle((bx + bw // 2 - 4, by - 40, bx + bw // 2 + 4, by),
                       fill=(*color[:3], 200))
    d.rectangle((0, horizon, BG_SIZE, BG_SIZE), fill=(10, 8, 20, 255))
    for i in range(6):
        y = horizon + 20 + i * 30
        alpha = max(10, 120 - i * 18)
        d.line((0, y, BG_SIZE, y), fill=(*NEON_PINK[:3], alpha), width=4)
    return img


BG_GENERATORS = {
    "polaroid-darkroom.png": polaroid_darkroom_bg,
    "ki-arena-lightning.png": ki_arena_lightning_bg,
    "cyberpunk-night-city-bg.png": cyberpunk_night_city_bg,
}


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--chips-out", type=Path, default=CHIPS_OUT_DEFAULT)
    ap.add_argument("--bg-out", type=Path, default=BG_OUT_DEFAULT)
    args = ap.parse_args()
    args.chips_out.mkdir(parents=True, exist_ok=True)
    args.bg_out.mkdir(parents=True, exist_ok=True)
    for filename, fn in CHIP_GENERATORS.items():
        img = fn()
        path = args.chips_out / filename
        img.save(path)
        print(f"wrote {path} {img.size}")
    for filename, fn in BG_GENERATORS.items():
        img = fn()
        path = args.bg_out / filename
        img.convert("RGB").save(path)
        print(f"wrote {path} {img.size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
