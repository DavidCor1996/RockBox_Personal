#!/usr/bin/env python3
"""Generate the RockPod wave-2 Sitekick chip source art.

Produces clean, procedurally-drawn (not hand-drawn) PNGs in the same
early-2000s YTV Sitekick/Ooze style as the rest of the iPod-exclusive
series: chunky dark-purple outlines, flat cel shading, and the palette
already used across ``tools/sitekick_package_assets.py`` (slime green,
purple, orange, yellow, cyan, hot pink, plus white highlights).

Every piece is built from primitive shapes (ellipses, rounded rectangles,
polygons) at a large supersampled canvas so the packager's LANCZOS resize
down to chip size stays crisp. Nothing here references a real person's
name; the scuba/fast-food-house set is purely thematic (dive mask, fins,
wetsuit, bubbles) and the doll set is keyed to the "Dollhouse" video's
porcelain-doll aesthetic (space buns, asymmetric doll eyes, pinafore,
cracked porcelain) rather than to any name.

Usage:
    sitekick_wave2_generate.py [--out <generated-dir>]
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw

OUT_DEFAULT = (Path(__file__).resolve().parent.parent /
               "assets/ipodjs/sources/sitekick/ipod-exclusive/generated")

SCALE = 10  # supersample factor: canvas = target chip size * SCALE

# YTV Sitekick palette (matches build_ipod_exclusive_image in
# sitekick_package_assets.py / BODY_TINTS).
OUTLINE = (54, 8, 70, 255)
GREEN = (105, 196, 52, 255)
PURPLE = (151, 72, 181, 255)
ORANGE = (255, 137, 29, 255)
CYAN = (60, 181, 218, 255)
PINK = (235, 75, 156, 255)
YELLOW = (255, 216, 31, 255)
WHITE = (247, 240, 249, 255)


def canvas(target: tuple[int, int]) -> tuple[Image.Image, ImageDraw.ImageDraw, int]:
    w, h = target[0] * SCALE, target[1] * SCALE
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img), SCALE


# ---------------------------------------------------------------------------
# Set 1: unnamed scuba / fast-food-house theme
# ---------------------------------------------------------------------------

def dive_mask_visor() -> Image.Image:
    img, d, s = canvas((48, 22))
    w, h = img.size
    lw = 5 * s
    d.rounded_rectangle((6 * s, 3 * s, w - 12 * s, h - 3 * s), radius=8 * s,
                        fill=CYAN, outline=OUTLINE, width=lw)
    d.ellipse((10 * s, 6 * s, 18 * s, 11 * s), fill=WHITE)
    d.rounded_rectangle((w - 13 * s, 6 * s, w - 2 * s, 13 * s), radius=3 * s,
                        fill=YELLOW, outline=OUTLINE, width=int(3.5 * s))
    return img


def fry_basket_fins() -> Image.Image:
    img, d, s = canvas((112, 52))
    w, h = img.size
    lw = 5 * s

    def fin(cx: int, flip: bool) -> None:
        pts = [(cx, 2 * s), (cx + (11 if not flip else -11) * s, 8 * s),
               (cx + (14 if not flip else -14) * s, 24 * s),
               (cx + (6 if not flip else -6) * s, 36 * s),
               (cx, 38 * s), (cx - (6 if not flip else -6) * s, 36 * s),
               (cx - (14 if not flip else -14) * s, 24 * s),
               (cx - (11 if not flip else -11) * s, 8 * s)]
        d.polygon(pts, fill=ORANGE, outline=OUTLINE)
        d.line(pts + [pts[0]], fill=OUTLINE, width=lw, joint="curve")
        for by in (14 * s, 20 * s):
            d.rectangle((cx - 12 * s, by, cx + 12 * s, by + 3 * s),
                        fill=YELLOW)
        d.rounded_rectangle((cx - 9 * s, -1 * s, cx + 9 * s, 6 * s),
                            radius=3 * s, fill=PURPLE, outline=OUTLINE,
                            width=int(3 * s))

    fin(w // 4, False)
    fin(3 * w // 4, True)
    return img


def drive_thru_wetsuit() -> Image.Image:
    img, d, s = canvas((96, 80))
    w, h = img.size
    lw = 6 * s
    body = [(20 * s, 2 * s), (w - 20 * s, 2 * s), (w - 6 * s, h - 2 * s),
            (6 * s, h - 2 * s)]
    d.polygon(body, fill=CYAN, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    d.polygon([(20 * s, 4 * s), (34 * s, 4 * s), (16 * s, h - 4 * s),
               (4 * s, h - 4 * s)], fill=ORANGE)
    d.rounded_rectangle((w // 2 - 16 * s, 22 * s, w // 2 + 16 * s, 42 * s),
                        radius=4 * s, fill=YELLOW, outline=OUTLINE,
                        width=int(3.5 * s))
    d.line((w // 2 - 10 * s, 32 * s, w // 2 + 10 * s, 32 * s),
          fill=OUTLINE, width=int(2 * s))
    return img


def bubble_trail_aura() -> Image.Image:
    img, d, s = canvas((144, 112))
    w, h = img.size
    cx, cy = w // 2, h - 8 * s
    path = []
    for i in range(9):
        t = i / 8.0
        x = cx - t * (w * 0.34) + math.sin(t * 3.0) * 6 * s
        y = cy - t * (h * 0.82)
        path.append((x, y))
    for i, (x, y) in enumerate(path):
        r = (6 - (i % 3)) * s
        outline_color = CYAN if i % 2 == 0 else PURPLE
        bbox = (x - r, y - r, x + r, y + r)
        d.ellipse(bbox, fill=(*CYAN[:3], 150), outline=outline_color,
                  width=int(2 * s))
        hi = r * 0.35
        d.ellipse((x - hi, y - hi * 1.6, x, y - hi * 0.4), fill=WHITE)
    return img


# ---------------------------------------------------------------------------
# Set 2: Dollhouse-video doll look (no name used)
# ---------------------------------------------------------------------------

def doll_space_buns() -> Image.Image:
    img, d, s = canvas((100, 74))
    w, h = img.size
    lw = 5 * s

    def bun(cx: int) -> None:
        d.ellipse((cx - 17 * s, 6 * s, cx + 17 * s, 40 * s), fill=PINK,
                  outline=OUTLINE, width=lw)
        d.ellipse((cx - 6 * s, 12 * s, cx + 2 * s, 20 * s), fill=WHITE)
        bow = [(cx - 10 * s, 34 * s), (cx - 2 * s, 30 * s),
               (cx - 10 * s, 26 * s)]
        d.polygon(bow, fill=YELLOW, outline=OUTLINE)
        bow2 = [(cx + 10 * s, 34 * s), (cx + 2 * s, 30 * s),
                (cx + 10 * s, 26 * s)]
        d.polygon(bow2, fill=YELLOW, outline=OUTLINE)
        d.ellipse((cx - 4 * s, 27 * s, cx + 4 * s, 33 * s), fill=YELLOW,
                  outline=OUTLINE, width=int(2.5 * s))

    bun(w // 4 + 2 * s)
    bun(3 * w // 4 - 2 * s)
    d.line((w // 2, 4 * s, w // 2, 20 * s), fill=OUTLINE, width=int(2 * s))
    return img


def cracked_doll_eyes() -> Image.Image:
    img, d, s = canvas((48, 20))
    w, h = img.size
    lw = int(2.5 * s)

    lx, rx = w // 4, 3 * w // 4
    cy = h // 2
    r = 6 * s
    d.ellipse((lx - r, cy - r, lx + r, cy + r), fill=WHITE, outline=OUTLINE,
              width=lw)
    d.ellipse((lx - 3 * s, cy - 3 * s, lx + 3 * s, cy + 3 * s), fill=YELLOW)
    wing = [(lx + r - 1 * s, cy - 2 * s), (lx + r + 5 * s, cy - 6 * s),
            (lx + r + 5 * s, cy - 2 * s)]
    d.polygon(wing, fill=OUTLINE)
    for i in range(3):
        lash_x = lx - r + i * 3 * s
        d.line((lash_x, cy - r, lash_x - 2 * s, cy - r - 4 * s),
              fill=OUTLINE, width=int(1.5 * s))

    d.ellipse((rx - r, cy - r, rx + r, cy + r), fill=WHITE, outline=OUTLINE,
              width=lw)
    d.ellipse((rx - 3 * s, cy - 3 * s, rx + 3 * s, cy + 3 * s), fill=YELLOW)
    crack = [(rx - 4 * s, cy + r + 2 * s), (rx - 1 * s, cy + r + 5 * s),
             (rx + 2 * s, cy + r + 2 * s), (rx + 5 * s, cy + r + 6 * s)]
    d.line(crack, fill=OUTLINE, width=int(1.5 * s), joint="curve")

    d.ellipse((lx - 8 * s, cy + r, lx - 2 * s, cy + r + 4 * s),
              fill=(*PINK[:3], 160))
    d.ellipse((rx + 2 * s, cy + r, rx + 8 * s, cy + r + 4 * s),
              fill=(*PINK[:3], 160))
    return img


def dollhouse_pinafore() -> Image.Image:
    img, d, s = canvas((92, 76))
    w, h = img.size
    lw = 6 * s
    body = [(18 * s, 8 * s), (w - 18 * s, 8 * s), (w - 6 * s, h - 2 * s),
            (6 * s, h - 2 * s)]
    d.polygon(body, fill=PINK, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")

    scallop_y = 8 * s
    bumps = 6
    for i in range(bumps):
        cx = 14 * s + i * ((w - 28 * s) / (bumps - 1))
        d.ellipse((cx - 6 * s, scallop_y - 5 * s, cx + 6 * s, scallop_y + 5 * s),
                  fill=WHITE, outline=OUTLINE, width=int(2 * s))

    for cy in (26 * s, 38 * s, 50 * s):
        d.ellipse((w // 2 - 3 * s, cy - 3 * s, w // 2 + 3 * s, cy + 3 * s),
                  fill=YELLOW, outline=OUTLINE, width=int(2 * s))
    return img


def porcelain_crack_halo() -> Image.Image:
    img, d, s = canvas((140, 120))
    w, h = img.size
    cx, cy = w // 2, h // 2
    outer = min(w, h) // 2 - 4 * s
    inner = outer - 12 * s
    d.ellipse((cx - outer, cy - outer, cx + outer, cy + outer), fill=PINK,
              outline=OUTLINE, width=int(3 * s))
    d.ellipse((cx - inner, cy - inner, cx + inner, cy + inner), fill=WHITE,
              outline=OUTLINE, width=int(2.5 * s))
    hole = inner - 12 * s
    d.ellipse((cx - hole, cy - hole, cx + hole, cy + hole), fill=(0, 0, 0, 0))

    for ang in (25, 110, 200, 300):
        rad = math.radians(ang)
        x0 = cx + math.cos(rad) * (hole + 2 * s)
        y0 = cy + math.sin(rad) * (hole + 2 * s)
        x1 = cx + math.cos(rad) * (outer - 3 * s)
        y1 = cy + math.sin(rad) * (outer - 3 * s)
        mx = (x0 + x1) / 2 + math.sin(rad) * 5 * s
        my = (y0 + y1) / 2 - math.cos(rad) * 5 * s
        d.line([(x0, y0), (mx, my), (x1, y1)], fill=OUTLINE,
              width=int(1.5 * s), joint="curve")

    for ang in (60, 150, 240, 330):
        rad = math.radians(ang)
        x = cx + math.cos(rad) * (outer - 3 * s)
        y = cy + math.sin(rad) * (outer - 3 * s)
        notch = 4 * s
        d.polygon([(x, y), (x + notch, y), (x, y + notch)], fill=(0, 0, 0, 0))
    return img


# ---------------------------------------------------------------------------
# Set 3: Polaroid inspired
# ---------------------------------------------------------------------------

def polaroid_frame_backdrop() -> Image.Image:
    img, d, s = canvas((160, 140))
    w, h = img.size
    lw = 6 * s
    d.rounded_rectangle((4 * s, 4 * s, w - 4 * s, h - 4 * s), radius=6 * s,
                        fill=WHITE, outline=OUTLINE, width=lw)
    photo = (14 * s, 14 * s, w - 14 * s, h - 30 * s)
    d.rectangle(photo, fill=CYAN, outline=OUTLINE, width=int(3 * s))
    px0, py0, px1, py1 = photo
    d.polygon([(px0, py1), (px1, py1), (px1, py0)], fill=PURPLE)
    d.polygon([(px0, py1), (px1, py0), (px0, py0)], fill=CYAN)
    d.ellipse((px0 + (px1 - px0) * 0.3, py0 + (py1 - py0) * 0.2,
              px0 + (px1 - px0) * 0.55, py0 + (py1 - py0) * 0.45),
             fill=YELLOW, outline=OUTLINE, width=int(2 * s))

    for cx in (18 * s, w - 18 * s):
        tape = [(cx - 8 * s, 2 * s), (cx + 8 * s, 2 * s),
                (cx + 5 * s, 10 * s), (cx - 5 * s, 10 * s)]
        d.polygon(tape, fill=(*YELLOW[:3], 210), outline=OUTLINE,
                  width=int(1.5 * s))
    return img


def flash_pop_shades() -> Image.Image:
    img, d, s = canvas((46, 18))
    w, h = img.size
    lw = int(3.5 * s)
    d.line((w // 2 - 4 * s, 4 * s, w // 2 + 4 * s, 4 * s), fill=CYAN,
          width=int(3 * s))
    for cx in (12 * s, w - 12 * s):
        d.rounded_rectangle((cx - 9 * s, 3 * s, cx + 9 * s, 15 * s),
                            radius=4 * s, fill=PURPLE, outline=OUTLINE,
                            width=lw)
        star_r_out, star_r_in = 4.5 * s, 1.8 * s
        pts = []
        for i in range(8):
            r = star_r_out if i % 2 == 0 else star_r_in
            ang = math.radians(i * 45 - 90)
            pts.append((cx + math.cos(ang) * r, 9 * s + math.sin(ang) * r))
        d.polygon(pts, fill=WHITE)
    return img


def instant_print_fan() -> Image.Image:
    card_w, card_h = 22 * SCALE, 28 * SCALE
    colors = (CYAN, PINK, YELLOW)

    def card(color: tuple[int, int, int, int]) -> Image.Image:
        c = Image.new("RGBA", (card_w, card_h), (0, 0, 0, 0))
        cd = ImageDraw.Draw(c)
        cd.rectangle((0, 0, card_w - 1, card_h - 1), fill=WHITE,
                    outline=OUTLINE, width=int(1.6 * SCALE))
        cd.rectangle((3 * SCALE, 3 * SCALE, card_w - 4 * SCALE,
                     card_h - 9 * SCALE), fill=color, outline=OUTLINE,
                    width=int(1.2 * SCALE))
        return c

    out = Image.new("RGBA", (60 * SCALE, 74 * SCALE), (0, 0, 0, 0))
    for i, (angle, color) in enumerate(zip((-18, 2, 20), colors)):
        c = card(color).rotate(angle, expand=True, resample=Image.BICUBIC)
        x = 5 * SCALE + i * 10 * SCALE
        y = 4 * SCALE + abs(i - 1) * 3 * SCALE
        out.alpha_composite(c, (x, y))
    return out


GENERATORS = {
    "dive-mask-visor.png": dive_mask_visor,
    "fry-basket-fins.png": fry_basket_fins,
    "drive-thru-wetsuit.png": drive_thru_wetsuit,
    "bubble-trail-aura.png": bubble_trail_aura,
    "doll-space-buns.png": doll_space_buns,
    "cracked-doll-eyes.png": cracked_doll_eyes,
    "dollhouse-pinafore.png": dollhouse_pinafore,
    "porcelain-crack-halo.png": porcelain_crack_halo,
    "polaroid-frame-backdrop.png": polaroid_frame_backdrop,
    "flash-pop-shades.png": flash_pop_shades,
    "instant-print-fan.png": instant_print_fan,
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
