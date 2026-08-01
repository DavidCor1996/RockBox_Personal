#!/usr/bin/env python3
"""Generate RockPod wave-7 Sitekick source art: more popular Old School
RuneScape items for the "Runes" set (party hat, santa hat, fire cape,
twisted bow, dragon scimitar, amulet of fury, barrows gloves, max cape).

Same procedurally-drawn, non-hand-drawn approach as every prior wave:
primitive shapes at a supersampled canvas, chunky dark-purple outlines,
flat cel shading, and the existing YTV palette. Original silhouettes in
each item's iconic color scheme -- no traced game textures or icons.

Usage:
    sitekick_wave7_generate.py [--out <generated-dir>]
"""

from __future__ import annotations

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
GOLD = (222, 178, 60, 255)
STEEL_BLUE = (86, 128, 176, 255)
SILVER = (198, 210, 224, 255)
BLACK = (30, 20, 34, 255)
DARK_RED = (110, 30, 36, 255)
ORANGE_FLAME = (255, 137, 29, 255)


# ---------------------------------------------------------------------------
# Party hat, Santa hat
# ---------------------------------------------------------------------------

def osrs_party_hat() -> Image.Image:
    img, d, s = canvas((70, 60))
    w, h = img.size
    lw = int(4 * s)
    cone = [(12 * s, h - 4 * s), (w - 12 * s, h - 4 * s), (w // 2, 4 * s)]
    d.polygon(cone, fill=RED, outline=OUTLINE)
    band_colors = (RED, YELLOW, GREEN, CYAN, PURPLE, PINK)
    for i, color in enumerate(band_colors):
        t0, t1 = i / len(band_colors), (i + 1) / len(band_colors)
        y0 = (h - 4 * s) - t0 * (h - 8 * s)
        y1 = (h - 4 * s) - t1 * (h - 8 * s)
        left = 12 * s + t0 * (w // 2 - 12 * s)
        right = w - 12 * s - t0 * (w // 2 - 12 * s)
        left1 = 12 * s + t1 * (w // 2 - 12 * s)
        right1 = w - 12 * s - t1 * (w // 2 - 12 * s)
        d.polygon([(left, y0), (right, y0), (right1, y1), (left1, y1)],
                 fill=color)
    d.line(cone + [cone[0]], fill=OUTLINE, width=lw, joint="curve")
    d.ellipse((w // 2 - 4 * s, 0, w // 2 + 4 * s, 8 * s), fill=YELLOW,
              outline=OUTLINE, width=int(2 * s))
    return img


def osrs_santa_hat() -> Image.Image:
    img, d, s = canvas((76, 66))
    w, h = img.size
    lw = int(4 * s)
    cone = [(16 * s, h - 14 * s), (w - 16 * s, h - 14 * s),
            (w // 2 + 6 * s, 6 * s), (w // 2, 2 * s), (w // 2 - 8 * s, 10 * s)]
    d.polygon(cone, fill=RED, outline=OUTLINE)
    d.line(cone + [cone[0]], fill=OUTLINE, width=lw, joint="curve")
    d.rounded_rectangle((6 * s, h - 20 * s, w - 6 * s, h - 2 * s),
                        radius=8 * s, fill=WHITE, outline=OUTLINE, width=lw)
    d.ellipse((w // 2 + 6 * s - 7 * s, 6 * s - 7 * s, w // 2 + 6 * s + 7 * s,
              6 * s + 7 * s), fill=WHITE, outline=OUTLINE,
             width=int(2.5 * s))
    d.ellipse((8 * s, h - 30 * s, 20 * s, h - 20 * s), fill=GREEN,
              outline=OUTLINE, width=int(2 * s))
    d.ellipse((w - 22 * s, h - 32 * s, w - 10 * s, h - 22 * s), fill=RED,
              outline=OUTLINE, width=int(2 * s))
    return img


# ---------------------------------------------------------------------------
# Fire cape, Max cape
# ---------------------------------------------------------------------------

def osrs_fire_cape() -> Image.Image:
    img, d, s = canvas((96, 84))
    w, h = img.size
    lw = 5 * s
    valley_y = h - 34 * s
    peak_y = h - 2 * s
    xs = [6 * s, 17 * s, 28 * s, 39 * s, 50 * s, 61 * s, 72 * s, 83 * s,
         90 * s]
    hem = [(x, peak_y if i % 2 == 1 else valley_y) for i, x in enumerate(xs)]
    body = [(20 * s, 2 * s), (w - 20 * s, 2 * s)] + hem
    d.polygon(body, fill=BLACK, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    for i in range(0, len(xs) - 2, 2):
        x0, x1, cx = xs[i], xs[i + 2], xs[i + 1]
        d.polygon([(x0 + 3 * s, valley_y - 4 * s), (cx, peak_y - 6 * s),
                  (x1 - 3 * s, valley_y - 4 * s)], fill=ORANGE_FLAME,
                 outline=OUTLINE, width=int(2 * s))
        d.polygon([(x0 + 7 * s, valley_y - 6 * s), (cx, peak_y - 16 * s),
                  (x1 - 7 * s, valley_y - 6 * s)], fill=YELLOW)
    d.polygon([(w // 2 - 10 * s, 4 * s), (w // 2 + 10 * s, 4 * s),
              (w // 2, 16 * s)], fill=ORANGE_FLAME, outline=OUTLINE,
             width=int(2 * s))
    return img


def osrs_max_cape() -> Image.Image:
    img, d, s = canvas((96, 84))
    w, h = img.size
    lw = 6 * s
    d.rounded_rectangle((w // 2 - 16 * s, 0, w // 2 + 16 * s, 14 * s),
                        radius=6 * s, fill=BLACK, outline=OUTLINE,
                        width=int(4 * s))
    body = [(16 * s, 10 * s), (w - 16 * s, 10 * s), (w - 4 * s, h - 2 * s),
            (4 * s, h - 2 * s)]
    d.polygon(body, fill=BLACK, outline=OUTLINE)
    d.line(body + [body[0]], fill=OUTLINE, width=lw, joint="curve")
    border = [(19 * s, 13 * s), (w - 19 * s, 13 * s), (w - 8 * s, h - 5 * s),
             (8 * s, h - 5 * s)]
    d.line(border + [border[0]], fill=GOLD, width=int(3 * s), joint="curve")
    d.polygon([(w // 2 - 12 * s, h - 30 * s), (w // 2 + 12 * s, h - 30 * s),
              (w // 2, h - 4 * s)], fill=GOLD, outline=OUTLINE,
             width=int(2 * s))
    return img


# ---------------------------------------------------------------------------
# Twisted bow, Dragon scimitar
# ---------------------------------------------------------------------------

def osrs_twisted_bow() -> Image.Image:
    img, d, s = canvas((64, 94))
    w, h = img.size
    lw = int(3 * s)
    cx = w // 2
    top_curl = [(cx, 8 * s), (cx + 12 * s, 3 * s), (cx + 22 * s, 10 * s),
               (cx + 20 * s, 20 * s), (cx + 8 * s, 20 * s)]
    bottom_curl = [(cx, h - 8 * s), (cx - 12 * s, h - 3 * s),
                  (cx - 22 * s, h - 10 * s), (cx - 20 * s, h - 20 * s),
                  (cx - 8 * s, h - 20 * s)]
    spine = [(cx, 8 * s), (cx - 4 * s, h // 2), (cx, h - 8 * s)]
    d.line(spine, fill=STEEL_BLUE, width=int(7 * s), joint="curve")
    d.line(top_curl, fill=STEEL_BLUE, width=int(6 * s), joint="curve")
    d.line(bottom_curl, fill=STEEL_BLUE, width=int(6 * s), joint="curve")
    d.line(spine, fill=WHITE, width=int(2 * s), joint="curve")
    d.line((cx - 3 * s, 10 * s, cx - 3 * s, h - 10 * s),
          fill=(90, 60, 40, 255), width=int(1.5 * s))
    for cy in (18 * s, h // 2, h - 18 * s):
        d.ellipse((cx - 6 * s, cy - 6 * s, cx + 6 * s, cy + 6 * s),
                  fill=GOLD, outline=OUTLINE, width=int(2 * s))
    return img


def osrs_dragon_scimitar() -> Image.Image:
    img, d, s = canvas((90, 80))
    w, h = img.size
    blade = Image.new("RGBA", (int(18 * s), int(64 * s)), (0, 0, 0, 0))
    bd = ImageDraw.Draw(blade)
    bw, bh = blade.size
    bd.polygon([(2 * s, 6 * s), (bw - 2 * s, 0), (bw - 2 * s, 40 * s),
               (bw // 2, 52 * s), (2 * s, 46 * s)], fill=STEEL_BLUE,
              outline=OUTLINE)
    bd.line((bw // 2, 6 * s, bw // 2, 40 * s), fill=WHITE,
           width=int(2 * s))
    bd.rectangle((2 * s, 46 * s, bw - 2 * s, 52 * s), fill=GOLD,
                outline=OUTLINE, width=int(2 * s))
    bd.rectangle((bw // 2 - 3 * s, 52 * s, bw // 2 + 3 * s, bh), fill=BLACK,
                outline=OUTLINE, width=int(1.5 * s))
    bd.ellipse((bw // 2 - 6 * s, bh - 10 * s, bw // 2 + 6 * s, bh),
              fill=GOLD, outline=OUTLINE, width=int(2 * s))
    blade = blade.rotate(-34, expand=True, resample=Image.BICUBIC)
    img.alpha_composite(blade, (w - blade.width, 0))
    return img


# ---------------------------------------------------------------------------
# Amulet of fury, Barrows gloves
# ---------------------------------------------------------------------------

def osrs_amulet_of_fury() -> Image.Image:
    img, d, s = canvas((40, 45))
    w, h = img.size
    lw = int(3 * s)
    d.arc((2 * s, -6 * s, w - 2 * s, 20 * s), 30, 150, fill=GOLD,
         width=int(4 * s))
    cx, cy = w // 2, 26 * s
    d.ellipse((cx - 12 * s, cy - 12 * s, cx + 12 * s, cy + 12 * s),
              fill=GOLD, outline=OUTLINE, width=lw)
    d.ellipse((cx - 7 * s, cy - 7 * s, cx + 7 * s, cy + 7 * s), fill=RED,
              outline=OUTLINE, width=int(2 * s))
    d.ellipse((cx - 3 * s, cy - 5 * s, cx + 1 * s, cy - 1 * s), fill=WHITE)
    return img


def osrs_barrows_gloves() -> Image.Image:
    img, d, s = canvas((110, 64))
    w, h = img.size
    lw = int(4 * s)

    def glove(cx: int) -> None:
        d.rounded_rectangle((cx - 16 * s, 6 * s, cx + 16 * s, 32 * s),
                            radius=8 * s, fill=DARK_RED, outline=OUTLINE,
                            width=lw)
        d.ellipse((cx - 14 * s, 26 * s, cx + 14 * s, h - 2 * s),
                  fill=BLACK, outline=OUTLINE, width=lw)
        for fx in (cx - 8 * s, cx, cx + 8 * s):
            d.line((fx, 28 * s, fx, 38 * s), fill=GOLD, width=int(2 * s))

    glove(w // 4 + 2 * s)
    glove(3 * w // 4 - 2 * s)
    return img


GENERATORS = {
    "osrs-party-hat.png": osrs_party_hat,
    "osrs-santa-hat.png": osrs_santa_hat,
    "osrs-fire-cape.png": osrs_fire_cape,
    "osrs-max-cape.png": osrs_max_cape,
    "osrs-twisted-bow.png": osrs_twisted_bow,
    "osrs-dragon-scimitar.png": osrs_dragon_scimitar,
    "osrs-amulet-of-fury.png": osrs_amulet_of_fury,
    "osrs-barrows-gloves.png": osrs_barrows_gloves,
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
