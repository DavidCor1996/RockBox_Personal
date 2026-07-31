#!/usr/bin/env python3
"""Generate RockPod wave-5 Sitekick source art: Maya Higa + Montreal
Canadiens wearable chips, five pet-companion chips, and four new
backgrounds tied to the recent chip waves (Hasan/QTC/Maya/Habs).

Same procedurally-drawn, non-hand-drawn approach as every prior wave:
primitive shapes at a supersampled canvas, chunky dark-purple outlines,
flat cel shading, and the existing YTV palette. Nothing here is a portrait,
a real logo, or a likeness -- the Maya set is a generic ranger hat/glove/
vest, the Habs set is a generic red/white/blue jersey/stick/toque (no team
crest), and the pets are original cartoon critters (small dragon, cat,
dog, turtle, blue fairy), not renders of any copyrighted character.

Usage:
    sitekick_wave5_generate.py [--chips-out <dir>] [--bg-out <dir>]
"""

from __future__ import annotations

import argparse
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

CHIPS_OUT_DEFAULT = (Path(__file__).resolve().parent.parent /
                     "assets/ipodjs/sources/sitekick/ipod-exclusive/generated")
BG_OUT_DEFAULT = (Path(__file__).resolve().parent.parent /
                  "assets/ipodjs/sources/sitekick/backgrounds")

RED = (214, 43, 55, 255)
NAVY = (18, 32, 74, 255)
SKYBLUE = (46, 108, 214, 255)
BROWN = (120, 78, 46, 255)
TAN = (222, 184, 135, 255)
DRAGON_PURPLE = (147, 58, 168, 255)


# ---------------------------------------------------------------------------
# Maya Higa set: ranger hat, falconer gloves, field vest, forest aura
# ---------------------------------------------------------------------------

def maya_ranger_hat() -> Image.Image:
    img, d, s = canvas((88, 52))
    w, h = img.size
    lw = 5 * s
    d.ellipse((2 * s, 24 * s, w - 2 * s, 40 * s), fill=TAN, outline=OUTLINE,
              width=lw)
    d.rounded_rectangle((22 * s, 4 * s, w - 22 * s, 32 * s), radius=10 * s,
                        fill=TAN, outline=OUTLINE, width=lw)
    d.rectangle((22 * s, 24 * s, w - 22 * s, 30 * s), fill=GREEN,
               outline=OUTLINE, width=int(2 * s))
    return img


def maya_falcon_glove() -> Image.Image:
    img, d, s = canvas((110, 64))
    w, h = img.size
    lw = int(4 * s)

    def glove(cx: int) -> None:
        d.rounded_rectangle((cx - 16 * s, 6 * s, cx + 16 * s, 34 * s),
                            radius=8 * s, fill=BROWN, outline=OUTLINE,
                            width=lw)
        d.ellipse((cx - 14 * s, 28 * s, cx + 14 * s, h - 2 * s), fill=TAN,
                  outline=OUTLINE, width=lw)
        for fx in (cx - 8 * s, cx, cx + 8 * s):
            d.line((fx, 30 * s, fx, 40 * s), fill=OUTLINE,
                  width=int(2 * s))

    glove(w // 4 + 2 * s)
    glove(3 * w // 4 - 2 * s)
    return img


def maya_field_vest() -> Image.Image:
    img, d, s = canvas((96, 80))
    w, h = img.size
    lw = 6 * s
    body = [(20 * s, 2 * s), (w - 20 * s, 2 * s), (w - 6 * s, h - 2 * s),
            (6 * s, h - 2 * s)]
    d.polygon(body, fill=GREEN, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    d.line((w // 2, 4 * s, w // 2, h - 4 * s), fill=OUTLINE,
          width=int(2.5 * s))
    for by in (34 * s, 54 * s):
        for bx in (20 * s, w - 34 * s):
            d.rectangle((bx, by, bx + 14 * s, by + 12 * s), fill=TAN,
                       outline=OUTLINE, width=int(2 * s))
    return img


def maya_forest_aura() -> Image.Image:
    img, d, s = canvas((150, 100))
    w, h = img.size
    cx, cy = w // 2, h // 2
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    r = min(w, h) // 2 - 4 * s
    gd.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(*GREEN[:3], 70))
    img = Image.alpha_composite(glow, img)
    d = ImageDraw.Draw(img)
    for ang in range(0, 360, 30):
        rad = math.radians(ang)
        bx = cx + math.cos(rad) * r * 0.9
        by = cy + math.sin(rad) * r * 0.9
        leaf = [(bx, by - 6 * s), (bx + 4 * s, by), (bx, by + 6 * s),
                (bx - 4 * s, by)]
        d.polygon(leaf, fill=GREEN, outline=OUTLINE)
    return img


# ---------------------------------------------------------------------------
# Montreal Canadiens ("Habs") set: no crest, colors + rink motifs only
# ---------------------------------------------------------------------------

def habs_home_jersey() -> Image.Image:
    img, d, s = canvas((96, 80))
    w, h = img.size
    lw = 6 * s
    body = [(20 * s, 2 * s), (w - 20 * s, 2 * s), (w - 6 * s, h - 2 * s),
            (6 * s, h - 2 * s)]
    d.polygon(body, fill=RED, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    d.rectangle((6 * s, 44 * s, w - 6 * s, 54 * s), fill=WHITE,
               outline=OUTLINE, width=int(2.5 * s))
    d.rectangle((6 * s, 54 * s, w - 6 * s, 60 * s), fill=NAVY)
    d.polygon([(w // 2 - 10 * s, 4 * s), (w // 2 + 10 * s, 4 * s),
              (w // 2, 16 * s)], fill=WHITE, outline=OUTLINE,
             width=int(2 * s))
    return img


def habs_hockey_stick() -> Image.Image:
    img, d, s = canvas((100, 90))
    w, h = img.size
    shaft = Image.new("RGBA", (int(14 * s), int(78 * s)), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shaft)
    sw, sh = shaft.size
    sd.rounded_rectangle((2 * s, 0, sw - 2 * s, sh - 12 * s), radius=4 * s,
                         fill=BROWN, outline=OUTLINE, width=int(3 * s))
    sd.rectangle((0, 4 * s, sw, 16 * s), fill=NAVY)
    sd.polygon([(0, sh - 14 * s), (sw, sh - 20 * s), (sw + 6 * s, sh - 2 * s),
               (2 * s, sh)], fill=TAN, outline=OUTLINE, width=int(2.5 * s))
    shaft = shaft.rotate(-24, expand=True, resample=Image.BICUBIC)
    img.alpha_composite(shaft, (w - shaft.width, 0))
    return img


def habs_winter_toque() -> Image.Image:
    img, d, s = canvas((84, 58))
    w, h = img.size
    lw = 5 * s
    d.ellipse((10 * s, 6 * s, w - 10 * s, 44 * s), fill=RED,
              outline=OUTLINE, width=lw)
    d.rounded_rectangle((6 * s, 34 * s, w - 6 * s, 48 * s), radius=6 * s,
                        fill=NAVY, outline=OUTLINE, width=lw)
    d.rectangle((6 * s, 38 * s, w - 6 * s, 40 * s), fill=WHITE)
    d.ellipse((w // 2 - 7 * s, 0, w // 2 + 7 * s, 12 * s), fill=WHITE,
              outline=OUTLINE, width=int(2.5 * s))
    return img


def habs_rink_aura() -> Image.Image:
    img, d, s = canvas((150, 100))
    w, h = img.size
    cx, cy = w // 2, h // 2
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    r = min(w, h) // 2 - 4 * s
    gd.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(*CYAN[:3], 60))
    img = Image.alpha_composite(glow, img)
    d = ImageDraw.Draw(img)
    d.ellipse((cx - r * 0.55, cy - r * 0.55, cx + r * 0.55, cy + r * 0.55),
              outline=RED, width=int(3 * s))
    d.line((cx - r * 0.9, cy, cx + r * 0.9, cy), fill=NAVY,
          width=int(3 * s))
    return img


# ---------------------------------------------------------------------------
# Pet companions: Spyro, Cat, Dog, Turtle, Navi
# ---------------------------------------------------------------------------

def spyro_companion() -> Image.Image:
    img, d, s = canvas((60, 48))
    w, h = img.size
    lw = int(3.5 * s)
    d.polygon([(16 * s, h - 8 * s), (0, h - 20 * s), (2 * s, h - 2 * s),
              (18 * s, h - 1 * s)], fill=DRAGON_PURPLE)
    d.ellipse((10 * s, 14 * s, w - 14 * s, h - 2 * s), fill=DRAGON_PURPLE,
              outline=OUTLINE, width=lw)
    wing = [(24 * s, 16 * s), (34 * s, 2 * s), (40 * s, 12 * s),
            (32 * s, 20 * s)]
    d.polygon(wing, fill=YELLOW, outline=OUTLINE, width=int(2 * s))
    for hx in (16 * s, 22 * s):
        d.polygon([(hx, 14 * s), (hx - 2 * s, 4 * s), (hx + 3 * s, 12 * s)],
                  fill=YELLOW, outline=OUTLINE)
    d.ellipse((14 * s, 20 * s, 20 * s, 26 * s), fill=WHITE, outline=OUTLINE,
              width=int(2 * s))
    return img


def cat_companion() -> Image.Image:
    img, d, s = canvas((50, 40))
    w, h = img.size
    lw = int(3.5 * s)
    d.ellipse((8 * s, 12 * s, w - 8 * s, h - 2 * s), fill=OUTLINE)
    d.ellipse((9 * s, 13 * s, w - 9 * s, h - 3 * s), fill=(255, 137, 29, 255),
              outline=OUTLINE, width=lw)
    for ex in (14 * s, w - 20 * s):
        d.polygon([(ex, 14 * s), (ex + 4 * s, 2 * s), (ex + 9 * s, 13 * s)],
                  fill=(255, 137, 29, 255), outline=OUTLINE)
    tail = [(w - 10 * s, h - 6 * s), (w - 2 * s, h - 20 * s),
            (w - 6 * s, 6 * s)]
    d.line(tail, fill=OUTLINE, width=int(4 * s), joint="curve")
    d.ellipse((17 * s, 20 * s, 23 * s, 25 * s), fill=WHITE)
    d.ellipse((27 * s, 20 * s, 33 * s, 25 * s), fill=WHITE)
    return img


def dog_companion() -> Image.Image:
    img, d, s = canvas((54, 42))
    w, h = img.size
    lw = int(3.5 * s)
    d.polygon([(w - 14 * s, h - 10 * s), (w - 2 * s, h - 24 * s),
              (w - 10 * s, h - 3 * s)], fill=TAN)
    d.ellipse((8 * s, 12 * s, w - 8 * s, h - 2 * s), fill=TAN,
              outline=OUTLINE, width=lw)
    d.ellipse((6 * s, 14 * s, 18 * s, 34 * s), fill=BROWN, outline=OUTLINE,
              width=int(2.5 * s))
    d.ellipse((17 * s, 20 * s, 23 * s, 25 * s), fill=WHITE)
    d.ellipse((27 * s, 20 * s, 33 * s, 25 * s), fill=WHITE)
    d.ellipse((21 * s, 26 * s, 27 * s, 31 * s), fill=OUTLINE)
    return img


def turtle_companion() -> Image.Image:
    img, d, s = canvas((52, 34))
    w, h = img.size
    lw = int(3 * s)
    for lx in (10 * s, 30 * s):
        d.ellipse((lx, h - 10 * s, lx + 10 * s, h - 1 * s), fill=TAN,
                  outline=OUTLINE, width=int(2 * s))
    d.ellipse((-2 * s, 10 * s, 14 * s, 26 * s), fill=TAN, outline=OUTLINE,
              width=lw)
    d.ellipse((0, 15 * s, 4 * s, 19 * s), fill=OUTLINE)
    d.ellipse((10 * s, 1 * s, w - 4 * s, h - 8 * s), fill=GREEN,
              outline=OUTLINE, width=lw)
    darker = (78, 150, 40, 255)
    for cx, cy, r in ((24 * s, 12 * s, 6 * s), (36 * s, 10 * s, 5 * s),
                      (30 * s, 20 * s, 5 * s), (18 * s, 20 * s, 4 * s)):
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=darker)
    return img


def navi_companion() -> Image.Image:
    img, d, s = canvas((40, 40))
    w, h = img.size
    cx, cy = w // 2, h // 2
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((cx - 16 * s, cy - 16 * s, cx + 16 * s, cy + 16 * s),
              fill=(*CYAN[:3], 120))
    img = Image.alpha_composite(glow, img)
    d = ImageDraw.Draw(img)
    for ang in (0, 90, 180, 270):
        rad = math.radians(ang)
        wx = cx + math.cos(rad) * 9 * s
        wy = cy + math.sin(rad) * 9 * s
        d.polygon([(cx, cy), (wx + 5 * s, wy - 3 * s), (wx, wy),
                  (wx - 5 * s, wy + 3 * s)], fill=(*WHITE[:3], 220),
                 outline=OUTLINE)
    d.ellipse((cx - 5 * s, cy - 5 * s, cx + 5 * s, cy + 5 * s), fill=CYAN,
              outline=OUTLINE, width=int(2 * s))
    return img


CHIP_GENERATORS = {
    "maya-ranger-hat.png": maya_ranger_hat,
    "maya-falcon-glove.png": maya_falcon_glove,
    "maya-field-vest.png": maya_field_vest,
    "maya-forest-aura.png": maya_forest_aura,
    "habs-home-jersey.png": habs_home_jersey,
    "habs-hockey-stick.png": habs_hockey_stick,
    "habs-winter-toque.png": habs_winter_toque,
    "habs-rink-aura.png": habs_rink_aura,
    "spyro-companion.png": spyro_companion,
    "cat-companion.png": cat_companion,
    "dog-companion.png": dog_companion,
    "turtle-companion.png": turtle_companion,
    "navi-companion.png": navi_companion,
}


# ---------------------------------------------------------------------------
# Backgrounds: 1200x1200 square scenes, empty centre for the character.
# ---------------------------------------------------------------------------

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


def hasan_news_studio_bg() -> Image.Image:
    img = vertical_gradient((BG_SIZE, BG_SIZE), (24, 14, 34), (54, 16, 26))
    d = ImageDraw.Draw(img)
    cx, cy = BG_SIZE // 2, BG_SIZE * 0.42
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    r = BG_SIZE * 0.42
    gd.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(*RED[:3], 90))
    img = Image.alpha_composite(img, glow)
    d = ImageDraw.Draw(img)
    for x in range(0, BG_SIZE, 90):
        d.line((x, 0, x - 260, BG_SIZE), fill=(*OUTLINE[:3], 40), width=6)
    bar_h = BG_SIZE // 8
    d.rectangle((0, BG_SIZE - bar_h, BG_SIZE, BG_SIZE), fill=RED)
    d.rectangle((0, BG_SIZE - bar_h, BG_SIZE, BG_SIZE - bar_h + 14),
               fill=OUTLINE)
    for i, bw in enumerate((160, 90, 220, 130, 180, 100)):
        x = 40 + i * 210
        d.rounded_rectangle(
            (x, BG_SIZE - bar_h // 2 - 24, x + bw, BG_SIZE - bar_h // 2 + 24),
            radius=10, fill=WHITE,
        )
    return img


def qtc_spotlight_stage_bg() -> Image.Image:
    img = vertical_gradient((BG_SIZE, BG_SIZE), (18, 10, 30), (46, 22, 62))
    cx = BG_SIZE // 2
    cone = Image.new("RGBA", img.size, (0, 0, 0, 0))
    cd = ImageDraw.Draw(cone)
    cd.polygon([(cx - 60, 0), (cx + 60, 0), (cx + 420, BG_SIZE),
               (cx - 420, BG_SIZE)], fill=(*YELLOW[:3], 70))
    img = Image.alpha_composite(img, cone)
    d = ImageDraw.Draw(img)
    for x0 in (60, BG_SIZE - 220):
        d.rounded_rectangle((x0, 0, x0 + 160, BG_SIZE), radius=60,
                            fill=(*PURPLE[:3], 160))
    import random
    rng = random.Random(1234)
    for _ in range(28):
        sx, sy = rng.randint(0, BG_SIZE), rng.randint(0, int(BG_SIZE * 0.55))
        r = rng.randint(6, 16)
        pts = []
        for i in range(8):
            rr = r if i % 2 == 0 else r * 0.4
            ang = math.radians(i * 45 - 90)
            pts.append((sx + math.cos(ang) * rr, sy + math.sin(ang) * rr))
        d.polygon(pts, fill=WHITE)
    floor_y = int(BG_SIZE * 0.82)
    d.rectangle((0, floor_y, BG_SIZE, BG_SIZE), fill=(30, 14, 46, 255))
    d.rectangle((0, floor_y, BG_SIZE, floor_y + 10), fill=(*CYAN[:3], 220))
    return img


def maya_wildlife_perch_bg() -> Image.Image:
    img = vertical_gradient((BG_SIZE, BG_SIZE), (150, 214, 200),
                            (60, 140, 110))
    sun = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(sun)
    sd.ellipse((BG_SIZE * 0.6, BG_SIZE * 0.55, BG_SIZE * 1.05,
               BG_SIZE * 1.0), fill=(*YELLOW[:3], 90))
    img = Image.alpha_composite(img, sun)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((-40, -20, BG_SIZE + 40, 90), radius=30, fill=BROWN,
                        outline=OUTLINE, width=8)
    for bx in range(0, BG_SIZE, 130):
        d.line((bx, 90, bx + 60, 200), fill=BROWN, width=14)
    rng_leaves = ((90, 130), (300, 170), (520, 120), (760, 190), (980, 140),
                 (1120, 210))
    for lx, ly in rng_leaves:
        d.polygon([(lx, ly), (lx + 30, ly - 40), (lx + 60, ly),
                  (lx + 30, ly + 30)], fill=GREEN, outline=OUTLINE,
                 width=4)
    perch_x = BG_SIZE - 200
    d.line((perch_x, 90, perch_x, 260), fill=BROWN, width=16)
    bird = [(perch_x - 10, 260), (perch_x - 70, 200), (perch_x - 30, 190),
            (perch_x + 10, 150), (perch_x + 40, 200), (perch_x + 20, 260)]
    d.polygon(bird, fill=(120, 90, 60, 255), outline=OUTLINE, width=6)
    d.polygon([(perch_x - 20, 175), (perch_x - 4, 160), (perch_x + 4, 178)],
              fill=YELLOW, outline=OUTLINE)
    ground_y = int(BG_SIZE * 0.86)
    d.rectangle((0, ground_y, BG_SIZE, BG_SIZE), fill=(70, 120, 70, 255))
    return img


def habs_home_ice_bg() -> Image.Image:
    img = vertical_gradient((BG_SIZE, BG_SIZE), (222, 236, 246),
                            (176, 206, 228))
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((BG_SIZE * 0.2, -BG_SIZE * 0.2, BG_SIZE * 0.8,
               BG_SIZE * 0.4), fill=(255, 255, 255, 110))
    img = Image.alpha_composite(img, glow)
    d = ImageDraw.Draw(img)
    red_y = int(BG_SIZE * 0.5)
    blue_y1 = int(BG_SIZE * 0.28)
    blue_y2 = int(BG_SIZE * 0.72)
    d.rectangle((0, blue_y1, BG_SIZE, blue_y1 + 22), fill=NAVY)
    d.rectangle((0, blue_y2, BG_SIZE, blue_y2 + 22), fill=NAVY)
    d.rectangle((0, red_y, BG_SIZE, red_y + 14), fill=RED)
    d.ellipse((BG_SIZE // 2 - 140, red_y - 140, BG_SIZE // 2 + 140,
              red_y + 140), outline=RED, width=8)
    for y in range(0, BG_SIZE, 60):
        d.line((0, y, BG_SIZE, y), fill=(255, 255, 255, 26), width=3)
    ground_y = int(BG_SIZE * 0.86)
    d.rectangle((0, ground_y, BG_SIZE, BG_SIZE), fill=(232, 242, 250, 255))
    return img


BG_GENERATORS = {
    "hasan-news-studio.png": hasan_news_studio_bg,
    "qtc-spotlight-stage.png": qtc_spotlight_stage_bg,
    "maya-wildlife-perch.png": maya_wildlife_perch_bg,
    "habs-home-ice.png": habs_home_ice_bg,
}


def main() -> int:
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
