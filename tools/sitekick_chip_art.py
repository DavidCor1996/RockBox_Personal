#!/usr/bin/env python3
"""Draw the RockPod-original Sitekick chips (ids 980-1049) as real vector art.

Waves 3-9 of ``IPOD_EXCLUSIVE_CHIPS`` shipped as crude filled primitives --
a shell was literally the same green/red/blue trapezoid with a circle
punched out of it, an aura was a soft blob, a hat was a triangle.  Next to
the authored Sitekick catalogue (chips 1-899) and the earlier RockPod waves
(900-979) they read as placeholder clip art rather than chips.

This module redraws all seventy of them in the catalogue's own house style:

  * one bold near-black outline of even weight around every shape;
  * flat saturated fills, not gradients;
  * an explicit lighter highlight facet and darker shade facet, drawn as
    real geometry rather than an overlaid diagonal sheen;
  * a readable silhouette that says what the item is at 40x40 icon size.

Every drawing routine works in *chip pixel space* -- the same coordinates
``sitekick_package_assets.py`` uses for its ``clear_alpha_ellipse`` neck and
head punches -- and is rendered supersampled, so the neck opening of a shell
lines up with the hole the packager cuts.

Real-world reference was used for colour and silhouette only (for example
the Old School RuneScape wiki's item detail renders, sampled for their
palettes); nothing here traces or embeds third-party art, which keeps the
same line the tree's existing wave comments already commit to.

Usage:
    sitekick_chip_art.py [--out <generated dir>] [--only kind[,kind...]]
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

# Supersampling factor used while drawing, and the scale of the PNG written
# out.  The packager's contain_alpha() does the final LANCZOS step down to
# the chip's declared size, so the source only has to be comfortably larger.
SS = 10
OS = 6

OUT_DEFAULT = (Path(__file__).resolve().parent.parent /
               "assets/ipodjs/sources/sitekick/ipod-exclusive/generated")

# Outline weight in chip pixels.  The authored catalogue sits around two
# pixels at a 96x80 chip, and the whole set reads as one family only if
# every chip keeps the same weight.
OW = 2.0
INK = (22, 15, 28)


# ---------------------------------------------------------------------------
# Colour helpers
# ---------------------------------------------------------------------------

def mix(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def dark(c, t=0.34):
    """The shade facet of a fill."""
    return mix(c, (18, 10, 24), t)


def lite(c, t=0.34):
    """The highlight facet of a fill."""
    return mix(c, (255, 252, 244), t)


def ink(c, t=0.80):
    """A near-black outline that still carries the fill's hue."""
    return mix(c, INK, t)


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def catmull(points, closed=False, steps=14):
    """Smooth a handful of control points into a cartoon-clean polyline."""
    pts = list(points)
    if closed:
        pts = [pts[-1]] + pts + [pts[0], pts[1]]
    else:
        pts = [pts[0]] + pts + [pts[-1]]
    out = []
    for i in range(len(pts) - 3):
        p0, p1, p2, p3 = pts[i:i + 4]
        for s in range(steps):
            t = s / steps
            t2, t3 = t * t, t * t * t
            out.append((
                0.5 * ((2 * p1[0]) + (-p0[0] + p2[0]) * t +
                       (2 * p0[0] - 5 * p1[0] + 4 * p2[0] - p3[0]) * t2 +
                       (-p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0]) * t3),
                0.5 * ((2 * p1[1]) + (-p0[1] + p2[1]) * t +
                       (2 * p0[1] - 5 * p1[1] + 4 * p2[1] - p3[1]) * t2 +
                       (-p0[1] + 3 * p1[1] - 3 * p2[1] + p3[1]) * t3),
            ))
    if not closed:
        out.append(pts[-1])
    return out


def arc_points(cx, cy, rx, ry, a0, a1, steps=48):
    return [(cx + rx * math.cos(math.radians(a)),
             cy + ry * math.sin(math.radians(a)))
            for a in (a0 + (a1 - a0) * i / steps for i in range(steps + 1))]


def star(cx, cy, r_out, r_in, points=5, rot=-90.0):
    pts = []
    for i in range(points * 2):
        r = r_out if i % 2 == 0 else r_in
        a = math.radians(rot + i * 180.0 / points)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def mirror(pts, axis):
    return [(2 * axis - x, y) for x, y in pts]


# ---------------------------------------------------------------------------
# Drawing surface
# ---------------------------------------------------------------------------

class Art:
    """A supersampled canvas addressed in final chip pixels."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.im = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.im)

    # -- primitives ------------------------------------------------------
    def _s(self, pts):
        return [(x * SS, y * SS) for x, y in pts]

    def _w(self, w):
        return max(1, round(w * SS))

    def poly(self, pts, fill=None, outline=None, w=OW):
        s = self._s(pts)
        if fill is not None:
            self.d.polygon(s, fill=fill)
        if outline is not None:
            self.d.line(s + [s[0]], fill=outline, width=self._w(w),
                        joint="curve")

    def smooth(self, pts, fill=None, outline=None, w=OW, steps=14):
        self.poly(catmull(pts, closed=True, steps=steps), fill, outline, w)

    def ell(self, box, fill=None, outline=None, w=OW):
        x0, y0, x1, y1 = (v * SS for v in box)
        self.d.ellipse((x0, y0, x1, y1), fill=fill, outline=outline,
                       width=self._w(w))

    def rrect(self, box, r, fill=None, outline=None, w=OW):
        x0, y0, x1, y1 = (v * SS for v in box)
        self.d.rounded_rectangle((x0, y0, x1, y1), radius=r * SS, fill=fill,
                                 outline=outline, width=self._w(w))

    def stroke(self, pts, color, w, round_cap=True):
        s = self._s(pts)
        self.d.line(s, fill=color, width=self._w(w), joint="curve")
        if round_cap:
            r = w * SS / 2.0
            for x, y in (s[0], s[-1]):
                self.d.ellipse((x - r, y - r, x + r, y + r), fill=color)

    def cord(self, pts, fill, w, outline=None, smooth_steps=14):
        """An outlined rope/strap: ink casing first, fill on top."""
        line = catmull(pts, steps=smooth_steps) if len(pts) > 2 else pts
        self.stroke(line, outline if outline is not None else ink(fill),
                    w + OW)
        self.stroke(line, fill, max(0.6, w - OW * 0.15))

    def glow(self, box, color, alpha=110, blur=6):
        layer = Image.new("RGBA", self.im.size, (0, 0, 0, 0))
        ImageDraw.Draw(layer).ellipse([v * SS for v in box],
                                      fill=(*color, alpha))
        layer = layer.filter(ImageFilter.GaussianBlur(blur * SS / 4.0))
        self.im.alpha_composite(layer)
        self.d = ImageDraw.Draw(self.im)

    # -- compound shapes -------------------------------------------------
    def facet(self, pts, base, hi=None, lo=None, outline=None, w=OW,
              smooth_steps=0):
        """Fill + outline in one call, the workhorse of every chip."""
        self.poly(catmull(pts, closed=True, steps=smooth_steps)
                  if smooth_steps else pts,
                  fill=base, outline=outline if outline is not None
                  else ink(base), w=w)

    def out(self):
        return self.im.resize((self.w * OS, self.h * OS), Image.LANCZOS)


# ---------------------------------------------------------------------------
# Shared silhouettes
# ---------------------------------------------------------------------------

# Every shell chip is a 96x80 canvas whose neck opening is punched by the
# packager at (34, 0, 62, 24), so all torso art shares one body outline.
SHELL_W, SHELL_H = 96, 80
NECK = (34, 0, 62, 24)


def shell_body(a, base, *, collar=None, sleeve=None, hem=None,
               shoulder=6.0, waist=None):
    """The common torso: shoulders, tapered sleeves, flared hem.

    Returns the torso polygon so callers can keep decorating inside it.
    """
    sleeve = sleeve if sleeve is not None else dark(base, 0.16)
    hem = hem if hem is not None else dark(base, 0.30)
    waist = waist if waist is not None else 30.0
    edge = ink(base)

    # sleeves (drawn first so the torso overlaps their inner edge)
    for sx in (1, -1):
        cx = 48 + sx * 26
        pts = [(48 + sx * 12, 16), (cx + sx * 14, 22), (cx + sx * 15, 48),
               (cx + sx * 3, 52), (48 + sx * 16, 40)]
        a.poly(pts, fill=sleeve, outline=edge)

    torso = [(48 - 20, 10), (48 - 12, 6), (48 - 6, 12), (48, 14), (48 + 6, 12),
             (48 + 12, 6), (48 + 20, 10),
             (48 + 22, 30), (48 + waist * 0.62, 72), (48 - waist * 0.62, 72),
             (48 - 22, 30)]
    a.poly(torso, fill=base, outline=edge)

    # highlight facet down the wearer's right, shade facet down the left
    a.poly([(48 - 18, 12), (48 - 8, 16), (48 - 10, 70), (48 - 19, 70)],
           fill=lite(base, 0.22))
    a.poly([(48 + 12, 16), (48 + 21, 12), (48 + 19, 70), (48 + 11, 70)],
           fill=dark(base, 0.20))
    a.poly(torso, outline=edge)

    if collar is not None:
        a.poly([(34, 4), (40, 16), (48, 20), (56, 16), (62, 4), (56, 2),
                (48, 8), (40, 2)], fill=collar, outline=ink(collar))
    if hem is not None:
        a.poly([(48 - waist * 0.62 - 1, 62), (48 + waist * 0.62 + 1, 62),
                (48 + waist * 0.62, 72), (48 - waist * 0.62, 72)],
               fill=hem, outline=edge)
    return torso


def cape_body(a, base, *, trim=None, clasp=None, texture=None):
    """A 96x80 cape: shoulder yoke plus a wide falling drape."""
    edge = ink(base)
    drape = [(30, 14), (24, 40), (20, 72), (48, 78), (76, 72), (72, 40),
             (66, 14), (48, 20)]
    a.poly(drape, fill=base, outline=edge)
    a.poly([(30, 16), (26, 44), (24, 70), (40, 74), (40, 20)],
           fill=lite(base, 0.20))
    a.poly([(66, 16), (70, 44), (72, 70), (58, 74), (58, 20)],
           fill=dark(base, 0.22))
    if texture:
        texture(a)
    a.poly(drape, outline=edge)
    if trim is not None:
        a.stroke([(24, 40), (20, 72)], ink(trim), OW * 2.2)
        a.stroke([(72, 40), (76, 72)], ink(trim), OW * 2.2)
        a.stroke([(24, 40), (20, 72)], trim, OW * 1.2)
        a.stroke([(72, 40), (76, 72)], trim, OW * 1.2)
        a.poly([(21, 70), (75, 70), (76, 76), (20, 76)], fill=trim,
               outline=ink(trim))
    # shoulder yoke, its inner edge following the punched neck opening
    a.poly([(30, 14), (36, 6), (48, 2), (60, 6), (66, 14), (58, 20),
            (48, 24), (38, 20)], fill=dark(base, 0.12), outline=edge)
    if clasp is not None:
        a.ell((44, 16, 52, 24), fill=clasp, outline=ink(clasp), w=OW * 0.8)


def eyewear(a, lens_shape, *, lens, frame, bridge_y=10.0, glint=True):
    """A 46x20 eyes-slot piece: two lenses joined by a bridge."""
    a.stroke([(2, bridge_y), (10, bridge_y)], frame, OW * 1.6)
    a.stroke([(36, bridge_y), (44, bridge_y)], frame, OW * 1.6)
    a.stroke([(20, bridge_y - 1), (26, bridge_y - 1)], frame, OW * 1.8)
    for pts in (lens_shape, mirror(lens_shape, 23)):
        a.poly(pts, fill=lens, outline=frame, w=OW * 1.1)
    if glint:
        a.poly([(7, 6), (11, 5), (9.5, 8), (6, 8.5)],
               fill=(255, 255, 255, 130))
        a.poly([(30, 6), (34, 5), (32.5, 8), (29, 8.5)],
               fill=(255, 255, 255, 95))


def glove_pair(a, base, *, cuff=None, talons=None, palm=None):
    """A 110x64 arms-slot pair of hands, mirrored about x=55.

    Four separated fingers plus a thumb, because a single rounded slab
    reads as a brick at icon size no matter how it is shaded.
    """
    cuff = cuff if cuff is not None else dark(base, 0.30)
    palm = palm if palm is not None else lite(base, 0.18)
    edge = ink(base)
    for sx, cx in ((1, 27), (-1, 83)):
        # thumb, tucked against the outer edge and drawn under the palm
        a.smooth([(cx - sx * 13, 30), (cx - sx * 20, 34),
                  (cx - sx * 21, 42), (cx - sx * 13, 42)],
                 fill=dark(base, 0.14), outline=edge, steps=12)
        for i in range(4):
            x = cx + sx * (i * 6.6 - 9.9)
            top = 16 + abs(i - 1.5) * 3.0
            a.rrect((min(x - 2.9, x + 2.9), top, max(x - 2.9, x + 2.9), 36),
                    2.6, fill=base, outline=edge, w=OW * 0.85)
            a.poly([(x - 1.8, top + 3), (x - 0.2, top + 3),
                    (x - 0.2, 33), (x - 1.8, 33)], fill=lite(base, 0.24))
            if talons is not None:
                a.poly([(x - 3.0, top + 1), (x + 3.0, top + 1),
                        (x + 0.8, top - 15), (x - 1.6, top - 12)],
                       fill=talons, outline=ink(talons), w=OW * 0.75)
                a.poly([(x - 1.8, top - 1), (x + 0.4, top - 1),
                        (x + 0.5, top - 11)], fill=lite(talons, 0.45))
        knuckle = [(cx - sx * 14, 32), (cx + sx * 14, 30), (cx + sx * 15, 44),
                   (cx - sx * 15, 46)]
        a.poly(knuckle, fill=base, outline=edge)
        a.poly([(cx - sx * 12, 34), (cx - sx * 2, 33), (cx - sx * 3, 43),
                (cx - sx * 12, 44)], fill=palm)
        a.poly(knuckle, outline=edge)
        a.poly([(cx - sx * 15, 44), (cx + sx * 14, 42), (cx + sx * 15, 56),
                (cx - sx * 16, 58)], fill=cuff, outline=ink(cuff))
        a.poly([(cx - sx * 13, 46), (cx - sx * 2, 45), (cx - sx * 3, 55),
                (cx - sx * 13, 56)], fill=lite(cuff, 0.24))


def blade_weapon(a, *, tip, guard, grip, blade_col, hilt_col, width=7.0,
                 curve=0.0):
    """A straight or curved blade running from `grip` through `guard` to
    `tip`, drawn as an outlined lens with a lit edge."""
    gx, gy = guard
    tx, ty = tip
    dx, dy = tx - gx, ty - gy
    length = math.hypot(dx, dy) or 1.0
    nx, ny = -dy / length, dx / length
    bow = (curve * nx, curve * ny)
    left = [(gx + nx * width, gy + ny * width),
            (gx + dx * 0.5 + nx * width * 0.9 + bow[0],
             gy + dy * 0.5 + ny * width * 0.9 + bow[1]),
            (tx, ty)]
    right = [(tx, ty),
             (gx + dx * 0.5 - nx * width * 0.55 + bow[0],
              gy + dy * 0.5 - ny * width * 0.55 + bow[1]),
             (gx - nx * width, gy - ny * width)]
    a.poly(catmull(left + right, closed=True, steps=10), fill=blade_col,
           outline=ink(blade_col))
    a.poly(catmull([(gx + nx * width * 0.5, gy + ny * width * 0.5),
                    (gx + dx * 0.5 + nx * width * 0.5 + bow[0],
                     gy + dy * 0.5 + ny * width * 0.5 + bow[1]),
                    (tx, ty),
                    (gx + dx * 0.55 + bow[0] * 0.6,
                     gy + dy * 0.55 + bow[1] * 0.6)],
                   closed=True, steps=10),
           fill=lite(blade_col, 0.42))
    px, py = grip
    a.cord([(gx, gy), (px, py)], hilt_col, 4.2)
    a.poly([(gx - nx * 11 - dx / length * 2, gy - ny * 11 - dy / length * 2),
            (gx + nx * 11 - dx / length * 2, gy + ny * 11 - dy / length * 2),
            (gx + nx * 9 + dx / length * 3, gy + ny * 9 + dy / length * 3),
            (gx - nx * 9 + dx / length * 3, gy - ny * 9 + dy / length * 3)],
           fill=hilt_col, outline=ink(hilt_col))
    a.ell((px - 4, py - 4, px + 4, py + 4), fill=lite(hilt_col, 0.2),
          outline=ink(hilt_col), w=OW * 0.8)


# ---------------------------------------------------------------------------
# Wave 3 -- Hero of Time
# ---------------------------------------------------------------------------

HERO_GREEN = (74, 148, 54)
HERO_BROWN = (126, 84, 44)
GOLD = (232, 178, 46)
STEEL = (176, 186, 198)


def hero_cap(a):
    """A floppy pointed cap: a wide cone rising off the head band, with the
    long tip folding away to the left."""
    edge = ink(HERO_GREEN)
    # the packager punches the head opening out of the middle, so this is a
    # solid dome hugging the skull with a long point flopping off the right
    tip = [(56, 20), (68, 12), (80, 8), (78, 2), (64, 2), (50, 10), (44, 18)]
    a.poly(catmull(tip, closed=True, steps=10), fill=dark(HERO_GREEN, 0.18),
           outline=edge)
    dome = arc_points(40, 46, 34, 36, 180, 360, 36) + [(74, 48), (6, 48)]
    a.poly(dome, fill=HERO_GREEN, outline=edge)
    a.poly(arc_points(40, 46, 29, 31, 190, 268, 18) + [(30, 46), (12, 46)],
           fill=lite(HERO_GREEN, 0.28))
    a.poly(arc_points(40, 46, 33, 35, 320, 360, 12) + [(74, 46), (60, 30)],
           fill=dark(HERO_GREEN, 0.2))
    a.poly(dome, fill=None, outline=edge)
    band = [(6, 38), (78, 36), (78, 52), (6, 54)]
    a.poly(band, fill=dark(HERO_GREEN, 0.24), outline=edge)
    a.poly([(6, 38), (78, 36), (78, 43), (6, 45)], fill=lite(HERO_GREEN, 0.14))
    a.poly(band, outline=edge)


def hero_tunic(a):
    shell_body(a, HERO_GREEN, collar=(238, 232, 214), sleeve=(236, 230, 212),
               hem=dark(HERO_GREEN, 0.26), waist=34)
    # chainmail cuffs peeking from the sleeves
    for sx in (1, -1):
        cx = 48 + sx * 26
        a.poly([(cx + sx * 15, 42), (cx + sx * 3, 46), (cx + sx * 2, 54),
                (cx + sx * 15, 50)], fill=STEEL, outline=ink(STEEL))
    belt = [(28, 46), (68, 46), (68, 56), (28, 56)]
    a.poly(belt, fill=HERO_BROWN, outline=ink(HERO_BROWN))
    a.poly([(28, 46), (68, 46), (68, 50), (28, 50)],
           fill=lite(HERO_BROWN, 0.2))
    a.poly(belt, outline=ink(HERO_BROWN))
    a.rrect((42, 44, 56, 58), 2.5, fill=GOLD, outline=ink(GOLD), w=OW * 0.9)
    a.rrect((46, 48, 52, 54), 1.2, fill=dark(GOLD, 0.4))
    # sword baldric across the chest
    a.cord([(32, 12), (62, 60)], HERO_BROWN, 5.0)


def hylian_shield(a):
    edge = ink((44, 74, 158))
    body = [(26, 2), (46, 6), (50, 24), (46, 48), (26, 68), (6, 48), (2, 24),
            (6, 6)]
    a.poly(body, fill=(44, 74, 158), outline=edge)
    a.poly([(26, 6), (43, 9), (46, 25), (43, 46), (26, 62), (9, 46), (6, 25),
            (9, 9)], fill=STEEL, outline=ink(STEEL), w=OW * 0.8)
    a.poly([(26, 10), (40, 13), (42, 26), (39, 44), (26, 57), (13, 44),
            (10, 26), (12, 13)], fill=(44, 74, 158), outline=edge, w=OW * 0.8)
    a.poly([(26, 10), (12, 13), (10, 26), (13, 44), (26, 57)],
           fill=lite((44, 74, 158), 0.22))
    # crest: wing pair over a downward triangle
    a.poly([(26, 20), (38, 22), (30, 28), (26, 26), (22, 28), (14, 22)],
           fill=GOLD, outline=ink(GOLD), w=OW * 0.7)
    a.poly([(18, 30), (34, 30), (26, 48)], fill=(196, 44, 44),
           outline=ink((196, 44, 44)), w=OW * 0.7)
    a.poly([(21, 32), (26, 32), (24, 42)], fill=lite((196, 44, 44), 0.3))


def forest_eye_mask(a):
    """A domino mask, not goggles: one green band with two cut eye holes."""
    edge = ink(HERO_GREEN)
    band = [(1, 8), (10, 2), (23, 5), (36, 2), (45, 8), (43, 15), (32, 19),
            (23, 15), (14, 19), (3, 15)]
    a.poly(catmull(band, closed=True, steps=12), fill=HERO_GREEN,
           outline=edge)
    a.poly(catmull([(2, 8), (10, 3), (22, 6), (22, 10), (12, 8), (4, 13)],
                   closed=True, steps=12), fill=lite(HERO_GREEN, 0.28))
    a.poly(catmull([(44, 8), (43, 14), (33, 18), (26, 14), (26, 7),
                    (36, 4)], closed=True, steps=12),
           fill=dark(HERO_GREEN, 0.2))
    a.poly(catmull(band, closed=True, steps=12), outline=edge)
    for cx in (12, 34):
        a.smooth([(cx, 5), (cx + 6, 9), (cx, 14), (cx - 6, 9)],
                 fill=(28, 24, 32), outline=edge, w=OW * 0.6, steps=12)
        a.ell((cx - 2.4, 7, cx + 2.4, 12), fill=(248, 248, 242))
        a.ell((cx - 1.2, 8.4, cx + 1.2, 10.8), fill=(32, 28, 36))


def ocarina_charm(a):
    """A vessel flute: fat teardrop chamber, tapered neck and mouthpiece."""
    body = (86, 122, 158)
    edge = ink(body)
    # one continuous silhouette: egg chamber flowing into a neck and a
    # flattened mouthpiece, so it never reads as a ball with a stick
    shell = [(2, 34), (4, 20), (14, 12), (26, 12), (33, 18), (38, 12),
             (44, 8), (45, 16), (38, 21), (36, 32), (30, 46), (16, 51),
             (5, 45)]
    a.poly(catmull(shell, closed=True, steps=12), fill=body, outline=edge)
    a.poly(catmull([(5, 33), (8, 21), (17, 15), (27, 17), (29, 26),
                    (25, 38), (17, 45), (9, 41)], closed=True, steps=12),
           fill=lite(body, 0.26))
    a.poly(catmull([(33, 26), (34, 36), (28, 45), (18, 49), (26, 41),
                    (30, 31)], closed=True, steps=12), fill=dark(body, 0.24))
    a.poly(catmull(shell, closed=True, steps=12), outline=edge)
    a.poly([(36, 13), (44, 8), (45, 16), (38, 20)], fill=dark(body, 0.34),
           outline=edge, w=OW * 0.7)
    for cx, cy in ((11, 27), (19, 24), (25, 30), (13, 37), (21, 36)):
        a.ell((cx - 2.4, cy - 2.4, cx + 2.4, cy + 2.4), fill=dark(body, 0.62),
              outline=edge, w=OW * 0.35)


def heart_container(a):
    red = (216, 46, 62)
    shape = [(22, 12), (30, 2), (40, 4), (43, 16), (34, 34), (22, 46),
             (10, 34), (1, 16), (4, 4), (14, 2)]
    a.smooth(shape, fill=GOLD, outline=ink(GOLD), steps=12)
    inner = [(22, 16), (29, 8), (37, 10), (39, 18), (32, 32), (22, 41),
             (12, 32), (5, 18), (7, 10), (15, 8)]
    a.smooth(inner, fill=red, outline=ink(red), w=OW * 0.8, steps=12)
    a.smooth([(15, 12), (21, 15), (18, 24), (11, 22), (9, 15)],
             fill=lite(red, 0.42), steps=12)
    a.smooth([(30, 13), (35, 15), (33, 22), (28, 21)],
             fill=(255, 255, 255, 170), steps=12)


def triforce_halo(a):
    def tri(cx, cy, s):
        return [(cx, cy - s), (cx + s * 0.87, cy + s * 0.5),
                (cx - s * 0.87, cy + s * 0.5)]
    a.glow((22, 8, 118, 90), (255, 224, 120), 90, 9)
    top = tri(70, 32, 21)
    a.poly(top, fill=GOLD, outline=ink(GOLD))
    a.poly([top[0], (70 + 10, 32 + 6), (70 - 10, 32 + 6)],
           fill=lite(GOLD, 0.34))
    a.poly(top, outline=ink(GOLD))
    for cx in (52, 88):
        t = tri(cx, 68, 21)
        a.poly(t, fill=GOLD, outline=ink(GOLD))
        a.poly([t[0], (cx + 10, 74), (cx - 10, 74)], fill=lite(GOLD, 0.26))
        a.poly(t, outline=ink(GOLD))
    for cx, cy, r in ((26, 22, 3.2), (118, 30, 2.6), (34, 82, 2.4),
                      (112, 78, 3.0)):
        a.poly(star(cx, cy, r * 2, r * 0.8, 4, -90), fill=(255, 240, 180))


def fairy_companion(a):
    blue = (96, 190, 240)
    a.glow((26, 16, 74, 62), (150, 220, 255), 120, 8)
    for sx, cx in ((1, 44), (-1, 56)):
        a.smooth([(cx, 34), (cx - sx * 12, 22), (cx - sx * 20, 8),
                  (cx - sx * 10, 4), (cx - sx * 2, 20)],
                 fill=(214, 240, 255, 220), outline=ink(blue), w=OW * 0.8,
                 steps=12)
    a.ell((40, 30, 60, 50), fill=blue, outline=ink(blue))
    a.ell((44, 33, 52, 41), fill=(240, 252, 255))
    for cx, cy, r in ((70, 58, 3.4), (80, 70, 2.6), (88, 80, 2.0),
                      (28, 60, 2.2), (20, 72, 1.6)):
        a.ell((cx - r, cy - r, cx + r, cy + r), fill=(190, 235, 255),
              outline=ink(blue), w=OW * 0.5)


# ---------------------------------------------------------------------------
# Wave 4 -- streamer homages
# ---------------------------------------------------------------------------

NAVY = (38, 52, 92)
CRIMSON = (196, 44, 50)
BLONDE = (238, 196, 96)


def hasan_cap(a):
    """A cap worn backwards: crown forward, brim sticking out to the left."""
    crown = [(70, 40), (68, 22), (54, 10), (36, 8), (20, 14), (12, 28),
             (12, 42)]
    a.poly(catmull(crown, closed=True, steps=12), fill=NAVY, outline=ink(NAVY))
    a.poly(catmull([(54, 12), (66, 24), (68, 40), (52, 40), (48, 20)],
                   closed=True, steps=12), fill=dark(NAVY, 0.26))
    a.poly(catmull([(34, 9), (20, 15), (13, 29), (14, 40), (30, 40),
                    (30, 18)], closed=True, steps=12), fill=lite(NAVY, 0.2))
    a.poly(catmull(crown, closed=True, steps=12), outline=ink(NAVY))
    for x in (30, 48):
        a.stroke([(x, 10 + abs(x - 40) * 0.05), (x, 40)], ink(NAVY), OW * 0.7)
    # snapback strap and opening at the front of a reversed cap
    a.poly([(12, 28), (12, 44), (26, 46), (26, 30)], fill=dark(NAVY, 0.4),
           outline=ink(NAVY), w=OW * 0.8)
    for y in (33, 39):
        a.ell((16, y - 1.6, 19, y + 1.6), fill=lite(NAVY, 0.45))
    # brim pointing back
    brim = [(66, 34), (82, 36), (83, 46), (64, 46)]
    a.poly(brim, fill=dark(NAVY, 0.34), outline=ink(NAVY))
    a.ell((44, 40, 84, 54), fill=None, outline=None)
    a.poly([(12, 40), (70, 38), (70, 50), (12, 50)], fill=dark(NAVY, 0.14),
           outline=ink(NAVY))


def hasan_headset(a):
    black = (48, 46, 58)
    a.cord([(6, 26), (10, 8), (27, 3), (44, 8), (48, 26)], black, 6.0)
    for cx in (7, 47):
        a.rrect((cx - 7, 22, cx + 7, 44), 4, fill=black, outline=ink(black))
        a.rrect((cx - 4, 26, cx + 4, 40), 3, fill=lite(black, 0.24))
    a.cord([(12, 40), (18, 52), (28, 56)], black, 3.4)
    a.ell((26, 50, 38, 62), fill=(70, 68, 82), outline=ink(black))
    a.ell((29, 52, 34, 57), fill=lite(black, 0.4))
    a.poly([(18, 6), (34, 4), (34, 9), (18, 11)], fill=CRIMSON,
           outline=ink(CRIMSON), w=OW * 0.7)


def hasan_hoodie(a):
    base = (176, 44, 48)
    shell_body(a, base, sleeve=dark(base, 0.14), hem=dark(base, 0.34),
               waist=34)
    # hood bunched behind the neck
    a.smooth([(28, 16), (34, 2), (48, -2), (62, 2), (68, 16), (58, 22),
              (48, 24), (38, 22)], fill=dark(base, 0.28), outline=ink(base),
             steps=12)
    a.smooth([(34, 12), (40, 4), (48, 2), (56, 4), (62, 12), (52, 18),
              (44, 18)], fill=dark(base, 0.48), steps=12)
    for sx in (1, -1):
        a.cord([(48 + sx * 6, 20), (48 + sx * 9, 34), (48 + sx * 6, 44)],
               (242, 238, 230), 2.6)
        a.ell((48 + sx * 6 - 2.4, 43, 48 + sx * 6 + 2.4, 48),
              fill=(242, 238, 230), outline=ink((200, 196, 188)), w=OW * 0.6)
    # kangaroo pocket
    a.poly([(32, 50), (64, 50), (62, 66), (34, 66)], fill=dark(base, 0.18),
           outline=ink(base))
    a.stroke([(38, 50), (38, 62)], ink(base), OW * 0.7)
    a.stroke([(58, 50), (58, 62)], ink(base), OW * 0.7)


def hasan_news_aura(a):
    red = (204, 32, 40)
    a.glow((6, 20, 144, 76), (255, 90, 90), 70, 10)
    a.rrect((10, 34, 140, 62), 3, fill=(22, 26, 44),
            outline=ink((22, 26, 44)))
    a.rrect((10, 34, 52, 62), 3, fill=red, outline=ink(red))
    a.poly([(14, 40), (24, 40), (24, 44), (14, 44)], fill=(255, 250, 240))
    a.poly([(14, 48), (34, 48), (34, 52), (14, 52)], fill=(255, 250, 240))
    for i, w in enumerate((66, 50, 74)):
        y = 39 + i * 7
        a.poly([(58, y), (58 + w, y), (58 + w, y + 4), (58, y + 4)],
               fill=(196, 204, 226) if i else (255, 250, 240))
    a.rrect((10, 64, 140, 70), 2, fill=red, outline=ink(red), w=OW * 0.7)
    for cx in (24, 60, 96, 132):
        a.poly(star(cx, 26, 6, 2, 4, -90), fill=(255, 214, 150))


def qtc_ponytail(a):
    """Hair swept up off the head opening at (29,31,71,95), gathered into a
    tail that falls down the right."""
    edge = ink(BLONDE)
    # the tail first, so the crown covers where it is gathered
    tail = [(74, 24), (92, 32), (96, 52), (88, 72), (74, 84), (62, 82),
            (70, 68), (78, 52), (74, 36)]
    a.poly(catmull(tail, closed=True, steps=14), fill=BLONDE, outline=edge)
    a.poly(catmull([(78, 32), (90, 38), (91, 54), (84, 70), (72, 78),
                    (70, 70), (80, 56), (80, 40)], closed=True, steps=14),
           fill=lite(BLONDE, 0.24))
    a.poly(catmull(tail, closed=True, steps=14), outline=edge)
    # crown: a solid mass over the skull.  The packager punches the face
    # opening at (29,31,71,95), so nothing here needs to leave a hole.
    crown = ([(6, 78)] + arc_points(50, 62, 44, 58, 180, 360, 40) +
             [(94, 78), (80, 72), (68, 76), (50, 72), (30, 76), (18, 72)])
    a.poly(crown, fill=BLONDE, outline=edge)
    a.poly(arc_points(50, 62, 38, 50, 184, 300, 26) + [(48, 62), (14, 66)],
           fill=lite(BLONDE, 0.3))
    a.poly(crown, outline=edge)
    for r in (0.60, 0.76, 0.92):
        a.stroke(arc_points(50, 63, 44 * r, 57 * r, 194, 322, 20),
                 dark(BLONDE, 0.24), OW * 0.6)
    a.cord([(70, 20), (82, 30)], (214, 74, 122), 5.0)


def qtc_visor(a):
    pink = (236, 116, 158)
    lens = [(4, 6), (16, 2), (21, 6), (19, 14), (9, 16), (3, 12)]
    eyewear(a, lens, lens=pink, frame=ink(pink), bridge_y=8)
    for pts in ([(2, 6), (0, 1), (6, 4)], [(44, 6), (46, 1), (40, 4)]):
        a.poly(pts, fill=pink, outline=ink(pink), w=OW * 0.6)


def qtc_trophy(a):
    cup = [(9, 4), (37, 4), (35, 22), (28, 34), (18, 34), (11, 22)]
    a.poly(cup, fill=GOLD, outline=ink(GOLD))
    a.poly([(12, 6), (20, 6), (19, 24), (14, 20)], fill=lite(GOLD, 0.4))
    a.poly([(30, 6), (34, 6), (32, 24), (28, 30)], fill=dark(GOLD, 0.28))
    a.poly(cup, outline=ink(GOLD))
    for sx, cx in ((1, 37), (-1, 9)):
        a.cord([(cx, 8), (cx + sx * 8, 12), (cx + sx * 5, 22), (cx - sx * 1,
                                                                24)],
               GOLD, 3.4)
    a.poly([(21, 34), (25, 34), (26, 44), (20, 44)], fill=dark(GOLD, 0.2),
           outline=ink(GOLD), w=OW * 0.8)
    a.poly([(12, 44), (34, 44), (36, 52), (10, 52)], fill=dark(GOLD, 0.3),
           outline=ink(GOLD))
    a.poly([(14, 46), (24, 46), (24, 50), (13, 50)], fill=lite(GOLD, 0.2))
    a.poly(star(23, 16, 7, 2.8), fill=(255, 246, 214), outline=ink(GOLD),
           w=OW * 0.6)


def qtc_spotlight_aura(a):
    a.glow((30, 40, 110, 108), (255, 226, 150), 90, 10)
    beam = [(56, 0), (84, 0), (124, 104), (16, 104)]
    a.poly(beam, fill=(255, 232, 168, 120))
    a.poly([(60, 0), (72, 0), (92, 104), (48, 104)],
           fill=(255, 244, 206, 130))
    a.poly(beam, outline=(255, 214, 120, 190), w=OW * 0.8)
    for cx, cy, r in ((26, 30, 3.4), (116, 24, 2.8), (18, 78, 2.4),
                      (128, 66, 3.0), (70, 14, 2.2)):
        a.poly(star(cx, cy, r * 2.2, r * 0.7, 4, -90), fill=(255, 246, 210))


# ---------------------------------------------------------------------------
# Wave 5 -- ranger, hockey, pet companions
# ---------------------------------------------------------------------------

OLIVE = (110, 122, 72)
KHAKI = (188, 168, 116)
HABS_RED = (175, 30, 45)
HABS_BLUE = (25, 42, 110)
WHITE = (244, 242, 236)


def maya_ranger_hat(a):
    """A campaign hat: pinched crown over a wide flat brim."""
    brim = [(4, 34), (20, 26), (44, 24), (68, 26), (84, 34), (68, 44),
            (44, 47), (20, 44)]
    a.poly(catmull(brim, closed=True, steps=12), fill=KHAKI,
           outline=ink(KHAKI))
    a.poly(catmull([(6, 35), (22, 42), (44, 45), (44, 47), (20, 44)],
                   closed=True, steps=12), fill=dark(KHAKI, 0.22))
    crown = [(24, 32), (26, 14), (36, 6), (44, 10), (52, 6), (62, 14),
             (64, 32)]
    a.poly(catmull(crown, closed=True, steps=12), fill=KHAKI,
           outline=ink(KHAKI))
    a.poly(catmull([(28, 30), (30, 15), (38, 8), (42, 12), (40, 30)],
                   closed=True, steps=12), fill=lite(KHAKI, 0.24))
    a.poly(catmull([(54, 9), (61, 16), (63, 30), (52, 30), (50, 12)],
                   closed=True, steps=12), fill=dark(KHAKI, 0.2))
    a.poly(catmull(crown, closed=True, steps=12), outline=ink(KHAKI))
    a.poly([(23, 28), (65, 28), (65, 36), (23, 36)], fill=OLIVE,
           outline=ink(OLIVE))
    a.poly(star(44, 32, 5, 2), fill=GOLD, outline=ink(GOLD), w=OW * 0.6)


def maya_falcon_glove(a):
    """A falconry gauntlet on the left with a small falcon perched on it."""
    leather = (140, 98, 56)
    edge = ink(leather)
    # forearm cuff
    a.poly([(2, 28), (24, 24), (28, 30), (28, 48), (24, 54), (2, 50)],
           fill=leather, outline=edge)
    a.poly([(4, 30), (22, 27), (24, 32), (22, 38), (4, 37)],
           fill=lite(leather, 0.26))
    a.poly([(2, 28), (24, 24), (28, 30), (28, 48), (24, 54), (2, 50)],
           outline=edge)
    for x in (10, 18):
        a.stroke([(x, 26.5 + (24 - x) * 0.18), (x, 51)],
                 dark(leather, 0.32), OW * 0.7)
    # gloved fist: knuckle block, four finger ridges, thumb underneath
    a.rrect((26, 24, 52, 46), 6, fill=dark(leather, 0.1), outline=edge)
    a.rrect((29, 27, 42, 36), 4, fill=lite(leather, 0.2))
    for i in range(4):
        y = 26.5 + i * 5.0
        a.stroke([(40, y), (51, y)], dark(leather, 0.4), OW * 0.6)
    a.smooth([(28, 40), (40, 44), (39, 53), (27, 51)],
             fill=dark(leather, 0.24), outline=edge, w=OW * 0.85, steps=10)
    # falcon: body, folded wing, hooked beak
    body = (122, 96, 74)
    a.smooth([(66, 8), (82, 14), (88, 30), (82, 44), (66, 46), (56, 34),
              (56, 18)], fill=body, outline=ink(body), steps=12)
    a.smooth([(70, 14), (82, 20), (84, 34), (74, 42), (64, 36), (64, 20)],
             fill=dark(body, 0.3), steps=12)
    a.smooth([(60, 6), (72, 4), (76, 12), (66, 16), (56, 14)],
             fill=lite(body, 0.36), outline=ink(body), w=OW * 0.7, steps=12)
    a.ell((58, 6, 64, 12), fill=(250, 246, 236), outline=ink(body),
          w=OW * 0.6)
    a.ell((59.6, 7.6, 62.4, 10.4), fill=(30, 24, 30))
    a.poly([(56, 9), (48, 12), (56, 14)], fill=GOLD, outline=ink(GOLD),
           w=OW * 0.6)
    a.cord([(64, 44), (62, 52)], GOLD, 2.2)
    a.cord([(78, 44), (80, 52)], GOLD, 2.2)


def maya_field_vest(a):
    shell_body(a, OLIVE, collar=None, sleeve=(238, 236, 228),
               hem=dark(OLIVE, 0.28), waist=32)
    # open front over a white tee
    a.poly([(40, 12), (56, 12), (54, 72), (42, 72)], fill=(238, 236, 228),
           outline=ink((214, 212, 204)))
    a.poly([(28, 10), (42, 14), (40, 72), (28, 72)], fill=OLIVE,
           outline=ink(OLIVE))
    a.poly([(68, 10), (54, 14), (56, 72), (68, 72)], fill=dark(OLIVE, 0.16),
           outline=ink(OLIVE))
    for y in (30, 50):
        a.rrect((29, y, 39, y + 13), 1.5, fill=dark(OLIVE, 0.24),
                outline=ink(OLIVE), w=OW * 0.8)
        a.rrect((57, y, 67, y + 13), 1.5, fill=dark(OLIVE, 0.3),
                outline=ink(OLIVE), w=OW * 0.8)
        a.stroke([(29, y + 4), (39, y + 4)], ink(OLIVE), OW * 0.6)
        a.stroke([(57, y + 4), (67, y + 4)], ink(OLIVE), OW * 0.6)


def maya_forest_aura(a):
    green = (72, 148, 66)
    a.glow((14, 10, 136, 92), (140, 220, 120), 80, 10)
    for i in range(11):
        ang = -90 + i * 360.0 / 11
        cx = 75 + 58 * math.cos(math.radians(ang))
        cy = 50 + 40 * math.sin(math.radians(ang))
        leaf = [(cx, cy - 11), (cx + 7, cy - 2), (cx, cy + 11),
                (cx - 7, cy - 2)]
        a.poly(catmull(leaf, closed=True, steps=10), fill=green,
               outline=ink(green), w=OW * 0.8)
        a.poly(catmull([(cx, cy - 9), (cx + 4, cy - 2), (cx, cy + 8)],
                       closed=True, steps=10), fill=lite(green, 0.28))
        a.stroke([(cx, cy - 9), (cx, cy + 9)], dark(green, 0.4), OW * 0.5)


def habs_home_jersey(a):
    shell_body(a, HABS_RED, sleeve=dark(HABS_RED, 0.12),
               hem=dark(HABS_RED, 0.3), waist=34)
    # the chest band: white-blue-white, and matching sleeve stripes
    for y0, y1, col in ((36, 39, WHITE), (39, 49, HABS_BLUE),
                        (49, 52, WHITE)):
        a.poly([(25, y0), (71, y0), (71, y1), (25, y1)], fill=col)
    a.poly([(25, 36), (71, 36), (71, 52), (25, 52)], outline=ink(HABS_RED),
           w=OW * 0.9)
    for sx in (1, -1):
        cx = 48 + sx * 26
        a.poly([(cx + sx * 15, 36), (cx + sx * 4, 39), (cx + sx * 3, 49),
                (cx + sx * 15, 46)], fill=HABS_BLUE, outline=ink(HABS_RED),
               w=OW * 0.8)
    a.poly([(34, 2), (62, 2), (60, 12), (48, 18), (36, 12)], fill=WHITE,
           outline=ink((190, 188, 182)), w=OW * 0.9)


def habs_hockey_stick(a):
    wood = (198, 158, 96)
    tape = (44, 42, 50)
    a.poly([(58, 2), (70, 6), (34, 74), (24, 68)], fill=wood,
           outline=ink(wood))
    a.poly([(58, 2), (64, 4), (30, 72), (24, 68)], fill=lite(wood, 0.24))
    a.poly([(58, 2), (70, 6), (34, 74), (24, 68)], outline=ink(wood))
    a.poly([(60, 4), (69, 8), (58, 28), (49, 24)], fill=tape,
           outline=ink(tape), w=OW * 0.8)
    a.poly([(60, 5), (64, 7), (55, 25), (51, 23)], fill=lite(tape, 0.24))
    blade = [(34, 74), (24, 68), (6, 74), (4, 84), (18, 86), (34, 82)]
    a.poly(catmull(blade, closed=True, steps=10), fill=tape, outline=ink(tape))
    a.poly(catmull([(30, 74), (22, 70), (8, 75), (8, 80), (26, 80)],
                   closed=True, steps=10), fill=lite(tape, 0.22))
    a.poly(catmull(blade, closed=True, steps=10), outline=ink(tape))
    a.ell((66, 72, 92, 86), fill=(38, 36, 44), outline=ink((38, 36, 44)))
    a.ell((68, 71, 90, 82), fill=(62, 60, 70), outline=ink((38, 36, 44)),
          w=OW * 0.8)


def habs_winter_toque(a):
    dome = arc_points(42, 46, 34, 34, 180, 360, 36) + [(76, 48), (8, 48)]
    a.poly(dome, fill=HABS_RED, outline=ink(HABS_RED))
    for y0, y1, col in ((22, 30, HABS_BLUE), (33, 38, WHITE)):
        a.poly([(9, y0), (75, y0), (75, y1), (9, y1)], fill=col)
    a.poly(arc_points(42, 46, 30, 30, 190, 260, 16) + [(20, 46), (16, 46)],
           fill=lite(HABS_RED, 0.22))
    a.poly(dome, fill=None, outline=ink(HABS_RED))
    a.rrect((5, 42, 79, 55), 4, fill=WHITE, outline=ink((186, 184, 178)))
    a.rrect((9, 44, 40, 49), 2, fill=(255, 255, 252))
    a.ell((32, 0, 52, 20), fill=WHITE, outline=ink((186, 184, 178)))
    a.ell((36, 3, 45, 12), fill=(255, 255, 252))


def habs_rink_aura(a):
    ice = (222, 238, 250)
    a.glow((6, 8, 144, 94), (170, 210, 250), 70, 10)
    a.ell((6, 8, 144, 94), fill=ice, outline=ink((120, 150, 180)))
    a.ell((10, 12, 140, 90), fill=None, outline=(160, 190, 220), w=OW * 0.7)
    a.stroke([(75, 12), (75, 90)], HABS_RED, OW * 1.1)
    for x in (44, 106):
        a.stroke([(x, 14), (x, 88)], HABS_BLUE, OW * 1.4)
    a.ell((60, 36, 90, 66), fill=None, outline=HABS_RED, w=OW * 0.9)
    a.ell((72, 48, 78, 54), fill=HABS_RED)
    for cx, cy in ((26, 30), (26, 72), (124, 30), (124, 72)):
        a.ell((cx - 8, cy - 8, cx + 8, cy + 8), fill=None, outline=HABS_RED,
              w=OW * 0.7)


def spyro_companion(a):
    """A small purple dragon in profile: body left, head right, wing above,
    tail curling off the back."""
    purple = (146, 82, 206)
    belly = (236, 204, 128)
    edge = ink(purple)
    # tail curling off to the left, with a gold spade
    a.cord([(18, 34), (8, 32), (5, 40)], purple, 5.0)
    a.poly([(6, 44), (1, 36), (9, 33), (11, 41)], fill=belly,
           outline=ink(belly), w=OW * 0.7)
    # wing folded on the back
    a.poly([(20, 20), (28, 6), (34, 8), (32, 18), (38, 16), (32, 24)],
           fill=(198, 150, 242), outline=edge, w=OW * 0.9)
    a.ell((12, 18, 42, 44), fill=purple, outline=edge)
    a.ell((17, 26, 37, 43), fill=belly)
    a.ell((12, 18, 42, 44), fill=None, outline=edge)
    for cx in (20, 34):
        a.poly([(cx - 4, 40), (cx + 4, 40), (cx + 5, 46), (cx - 5, 46)],
               fill=dark(purple, 0.2), outline=edge, w=OW * 0.7)
    # head: round skull plus a blunt snout to the right
    a.ell((30, 6, 56, 32), fill=purple, outline=edge)
    a.ell((34, 10, 47, 21), fill=lite(purple, 0.3))
    a.ell((30, 6, 56, 32), fill=None, outline=edge)
    a.poly([(50, 15), (60, 18), (60, 25), (49, 27)], fill=lite(purple, 0.1),
           outline=edge, w=OW * 0.8)
    a.ell((55, 19, 58, 22), fill=dark(purple, 0.55))
    a.ell((39, 12, 48, 21), fill=(250, 248, 242), outline=edge, w=OW * 0.6)
    a.ell((42, 15, 46, 19), fill=(32, 26, 38))
    for sx, hx in ((1, 44), (-1, 38)):
        a.poly([(hx, 8), (hx + sx * 4, 7), (hx + sx * 7, 0),
                (hx + sx * 1, 4)], fill=GOLD, outline=ink(GOLD), w=OW * 0.6)
    for x in (20, 26):
        a.poly([(x - 3, 21), (x + 3, 21), (x, 13)], fill=belly,
               outline=ink(belly), w=OW * 0.5)


def cat_companion(a):
    fur = (226, 146, 62)
    body = [(25, 36), (12, 34), (8, 22), (14, 12), (26, 9), (38, 13),
            (42, 24), (38, 35)]
    a.poly(catmull(body, closed=True, steps=12), fill=fur, outline=ink(fur))
    a.poly(catmull([(16, 32), (11, 22), (16, 13), (26, 11), (28, 22),
                    (24, 33)], closed=True, steps=12), fill=lite(fur, 0.26))
    a.poly(catmull(body, closed=True, steps=12), outline=ink(fur))
    for sx, cx in ((1, 33), (-1, 17)):
        a.poly([(cx - sx * 4, 12), (cx + sx * 5, 12), (cx + sx * 2, 1)],
               fill=fur, outline=ink(fur), w=OW * 0.8)
        a.poly([(cx - sx * 1, 11), (cx + sx * 3, 11), (cx + sx * 2, 5)],
               fill=(246, 178, 172))
    for x in (16, 22, 28, 34):
        a.stroke([(x, 14), (x + 1, 30)], dark(fur, 0.28), OW * 0.55)
    for cx in (19, 31):
        a.ell((cx - 3.4, 18, cx + 3.4, 25), fill=(250, 246, 226),
              outline=ink(fur), w=OW * 0.6)
        a.ell((cx - 1.4, 20, cx + 1.4, 24), fill=(52, 108, 62))
    a.poly([(22, 27), (28, 27), (25, 30)], fill=(224, 132, 140))
    a.cord([(40, 30), (48, 22), (46, 10)], fur, 3.4)


def dog_companion(a):
    fur = (168, 118, 74)
    body = [(28, 38), (14, 36), (9, 24), (16, 12), (30, 8), (43, 14),
            (47, 26), (42, 37)]
    a.poly(catmull(body, closed=True, steps=12), fill=fur, outline=ink(fur))
    a.poly(catmull([(18, 34), (12, 24), (18, 13), (30, 10), (32, 22),
                    (28, 35)], closed=True, steps=12), fill=lite(fur, 0.24))
    a.poly(catmull(body, closed=True, steps=12), outline=ink(fur))
    for sx, cx in ((1, 44), (-1, 12)):
        a.poly(catmull([(cx, 14), (cx + sx * 6, 20), (cx + sx * 4, 32),
                        (cx - sx * 3, 30)], closed=True, steps=10),
               fill=dark(fur, 0.26), outline=ink(fur), w=OW * 0.8)
    a.smooth([(20, 26), (36, 26), (38, 34), (28, 38), (19, 34)],
             fill=(244, 238, 222), outline=ink(fur), w=OW * 0.8, steps=12)
    for cx in (22, 34):
        a.ell((cx - 3.4, 17, cx + 3.4, 24), fill=(250, 246, 236),
              outline=ink(fur), w=OW * 0.6)
        a.ell((cx - 1.6, 19, cx + 1.6, 23), fill=(44, 32, 30))
    a.ell((25, 28, 32, 34), fill=(42, 32, 34))
    a.poly([(26, 34), (32, 34), (30, 40), (28, 40)], fill=(226, 140, 146))


def turtle_companion(a):
    shell = (86, 140, 62)
    skin = (168, 196, 106)
    a.smooth([(20, 30), (10, 26), (12, 20), (22, 20), (30, 22)], fill=skin,
             outline=ink(skin), w=OW * 0.8, steps=10)
    for cx in (16, 34):
        a.ell((cx - 5, 24, cx + 5, 32), fill=skin, outline=ink(skin),
              w=OW * 0.8)
    a.ell((6, 10, 46, 30), fill=shell, outline=ink(shell))
    a.ell((10, 12, 34, 24), fill=lite(shell, 0.3))
    a.ell((6, 10, 46, 30), fill=None, outline=ink(shell))
    for cx, cy in ((16, 17), (26, 15), (36, 18), (21, 24), (32, 24)):
        a.poly(star(cx, cy, 4.4, 2.4, 6, -90), fill=dark(shell, 0.3),
               outline=ink(shell), w=OW * 0.5)
    a.ell((42, 14, 52, 24), fill=skin, outline=ink(skin), w=OW * 0.8)
    a.ell((46, 16, 49, 19), fill=(38, 32, 34))


def navi_companion(a):
    blue = (110, 200, 246)
    a.glow((2, 2, 38, 38), (170, 230, 255), 130, 6)
    for sx, cx in ((1, 16), (-1, 24)):
        a.smooth([(cx, 20), (cx - sx * 8, 10), (cx - sx * 13, 2),
                  (cx - sx * 5, 2), (cx - sx * 1, 12)],
                 fill=(220, 244, 255, 210), outline=ink(blue), w=OW * 0.7,
                 steps=10)
    a.ell((12, 14, 28, 30), fill=blue, outline=ink(blue), w=OW * 0.9)
    a.ell((15, 17, 21, 23), fill=(244, 252, 255))


# ---------------------------------------------------------------------------
# Wave 6 -- Pink Floyd, Portal, and the first OSRS set
# ---------------------------------------------------------------------------

# Colours sampled from the OSRS wiki's own item detail renders, then pushed
# up in saturation so they survive the chip's tiny final size.
RUNE_BLUE = (78, 116, 132)
DRAGON_RED = (150, 34, 20)
OSRS_SILVER = (166, 170, 186)
OSRS_GOLD = (198, 164, 82)


def pink_floyd_pig(a):
    pig = (238, 132, 176)
    body = [(40, 20), (72, 14), (104, 20), (118, 38), (108, 60), (74, 68),
            (40, 62), (26, 44)]
    a.poly(catmull(body, closed=True, steps=14), fill=pig, outline=ink(pig))
    a.poly(catmull([(44, 24), (72, 18), (98, 24), (104, 38), (92, 32),
                    (60, 28), (36, 38)], closed=True, steps=14),
           fill=lite(pig, 0.3))
    a.poly(catmull([(48, 58), (80, 64), (106, 56), (112, 44), (100, 60),
                    (70, 60)], closed=True, steps=14), fill=dark(pig, 0.22))
    a.poly(catmull(body, closed=True, steps=14), outline=ink(pig))
    # snout, eye, ears, curly tail, dangling tethers
    a.ell((16, 32, 40, 52), fill=lite(pig, 0.12), outline=ink(pig))
    for cx in (23, 32):
        a.ell((cx - 2.6, 38, cx + 2.6, 46), fill=dark(pig, 0.5))
    a.ell((44, 26, 54, 36), fill=(252, 248, 244), outline=ink(pig),
          w=OW * 0.8)
    a.ell((46, 29, 51, 34), fill=(38, 28, 36))
    for cx in (58, 76):
        a.poly([(cx, 20), (cx + 12, 14), (cx + 8, 2), (cx - 2, 12)],
               fill=lite(pig, 0.1), outline=ink(pig), w=OW * 0.8)
    a.cord([(118, 40), (130, 34), (132, 46), (124, 48)], pig, 4.0)
    for x in (56, 92):
        a.cord([(x, 64), (x - 4, 86)], (90, 96, 112), 2.0)


def pink_floyd_prism(a):
    a.rrect((4, 6, 146, 94), 5, fill=(20, 18, 30), outline=ink((20, 18, 30)))
    tri = [(75, 20), (110, 76), (40, 76)]
    a.poly(tri, fill=(46, 44, 62), outline=(228, 228, 236), w=OW * 0.9)
    a.poly([(75, 20), (92, 48), (75, 76), (58, 48)], fill=(64, 62, 84))
    a.poly(tri, outline=(228, 228, 236), w=OW * 0.9)
    a.stroke([(4, 40), (62, 52)], (255, 255, 255), OW * 1.3)
    spectrum = ((236, 46, 52), (244, 132, 40), (250, 216, 60),
                (96, 196, 92), (70, 150, 236), (146, 88, 210))
    for i, col in enumerate(spectrum):
        a.stroke([(90, 56 + i * 1.6), (148, 34 + i * 9.0)], col, OW * 1.5)


def pink_floyd_hammers(a):
    """Two crossed claw hammers: long handles, blocky heads with a claw."""
    handle = (150, 112, 68)
    head = (188, 44, 46)
    for sx, cx in ((1, 14), (-1, 50)):
        hx, hy = cx + sx * 24, 14
        a.cord([(cx - sx * 4, 62), (hx, hy)], handle, 5.4)
        a.ell((cx - sx * 4 - 3.4, 58, cx - sx * 4 + 3.4, 65),
              fill=dark(handle, 0.24), outline=ink(handle), w=OW * 0.7)
        # one blocky head straddling the handle, striking face outward
        block = [(hx - sx * 13, hy - 9), (hx + sx * 11, hy - 11),
                 (hx + sx * 13, hy + 5), (hx - sx * 11, hy + 7)]
        a.poly(block, fill=head, outline=ink(head))
        a.poly([(hx - sx * 11, hy - 7), (hx + sx * 5, hy - 8),
                (hx + sx * 6, hy - 1), (hx - sx * 10, hy)],
               fill=lite(head, 0.32))
        a.poly([(hx + sx * 7, hy - 10), (hx + sx * 11, hy - 11),
                (hx + sx * 13, hy + 5), (hx + sx * 9, hy + 6)],
               fill=dark(head, 0.26))
        a.poly(block, outline=ink(head))


def portal_gun(a):
    body = (238, 238, 242)
    a.poly([(6, 14), (40, 8), (54, 16), (54, 34), (40, 42), (6, 36)],
           fill=body, outline=ink((150, 150, 160)))
    a.poly([(8, 16), (38, 11), (48, 17), (46, 24), (8, 26)],
           fill=(255, 255, 255))
    a.poly([(6, 14), (40, 8), (54, 16), (54, 34), (40, 42), (6, 36)],
           fill=None, outline=ink((150, 150, 160)))
    a.poly([(10, 34), (24, 34), (26, 50), (14, 50)], fill=(84, 86, 98),
           outline=ink((84, 86, 98)))
    for sx, cy in ((1, 16), (-1, 32)):
        a.poly([(52, cy - 6), (60, cy - 10), (58, cy + 2), (52, cy + 4)],
               fill=(196, 198, 206), outline=ink((130, 132, 142)),
               w=OW * 0.8)
    a.ell((44, 14, 54, 24), fill=(64, 152, 240), outline=ink((30, 70, 130)),
          w=OW * 0.8)
    a.ell((46, 16, 50, 20), fill=(198, 232, 255))
    a.ell((44, 26, 54, 36), fill=(246, 146, 40), outline=ink((150, 74, 16)),
          w=OW * 0.8)
    a.ell((46, 28, 50, 32), fill=(255, 226, 178))


def companion_cube(a):
    grey = (168, 172, 180)
    top = [(28, 3), (50, 14), (28, 25), (6, 14)]
    a.poly([(6, 14), (28, 25), (28, 50), (6, 39)], fill=dark(grey, 0.24),
           outline=ink(grey))
    a.poly([(50, 14), (28, 25), (28, 50), (50, 39)], fill=dark(grey, 0.06),
           outline=ink(grey))
    a.poly(top, fill=lite(grey, 0.24), outline=ink(grey))
    for face, cx, cy, sq in (("l", 17, 32, 7), ("r", 39, 32, 7),
                             ("t", 28, 14, 7)):
        col = (238, 238, 242)
        a.poly([(cx - sq, cy - sq * 0.7), (cx + sq, cy - sq * 0.7),
                (cx + sq, cy + sq * 0.7), (cx - sq, cy + sq * 0.7)],
               fill=col, outline=ink(grey), w=OW * 0.7)
        heart = [(cx, cy - 3), (cx + 2, cy - 5), (cx + 4.5, cy - 3),
                 (cx + 4, cy + 1), (cx, cy + 5), (cx - 4, cy + 1),
                 (cx - 4.5, cy - 3), (cx - 2, cy - 5)]
        a.poly(catmull(heart, closed=True, steps=10), fill=(238, 120, 158),
               outline=ink((238, 120, 158)), w=OW * 0.5)
    for cx, cy in ((6, 14), (50, 14), (28, 3), (28, 25), (6, 39), (50, 39)):
        a.ell((cx - 4, cy - 3, cx + 4, cy + 3), fill=(120, 124, 134),
              outline=ink(grey), w=OW * 0.6)


def long_fall_boots(a):
    orange = (232, 132, 40)
    metal = (192, 196, 206)
    for cx in (28, 82):
        a.poly([(cx - 16, 6), (cx + 16, 6), (cx + 14, 22), (cx - 14, 22)],
               fill=orange, outline=ink(orange))
        a.poly([(cx - 14, 8), (cx - 2, 8), (cx - 3, 20), (cx - 13, 20)],
               fill=lite(orange, 0.28))
        a.poly([(cx - 16, 6), (cx + 16, 6), (cx + 14, 22), (cx - 14, 22)],
               fill=None, outline=ink(orange))
        # spring shin section
        for i in range(3):
            a.poly([(cx - 12, 24 + i * 8), (cx + 12, 24 + i * 8),
                    (cx + 11, 30 + i * 8), (cx - 11, 30 + i * 8)],
                   fill=metal, outline=ink(metal), w=OW * 0.8)
        a.poly([(cx - 15, 46), (cx + 15, 46), (cx + 17, 56), (cx - 19, 56)],
               fill=(58, 58, 70), outline=ink((58, 58, 70)))
        a.poly([(cx - 13, 47), (cx - 1, 47), (cx - 2, 54), (cx - 15, 54)],
               fill=(92, 92, 106))


def aperture_turret(a):
    body = (238, 240, 244)
    a.smooth([(27, 2), (40, 8), (43, 26), (38, 40), (27, 44), (16, 40),
              (11, 26), (14, 8)], fill=body, outline=ink((140, 142, 152)),
             steps=14)
    a.smooth([(24, 5), (34, 9), (36, 24), (31, 38), (22, 38), (18, 24),
              (19, 9)], fill=(255, 255, 255), steps=14)
    a.smooth([(27, 2), (40, 8), (43, 26), (38, 40), (27, 44), (16, 40),
              (11, 26), (14, 8)], outline=ink((140, 142, 152)), steps=14)
    a.stroke([(20, 20), (34, 20)], (150, 152, 162), OW * 0.7)
    a.glow((18, 12, 36, 30), (255, 60, 60), 150, 4)
    a.ell((22, 16, 32, 26), fill=(230, 40, 44), outline=ink((160, 24, 26)),
          w=OW * 0.8)
    a.ell((24, 18, 28, 22), fill=(255, 196, 190))
    for sx, cx in ((1, 38), (-1, 16)):
        a.poly([(cx, 34), (cx + sx * 12, 44), (cx + sx * 13, 50),
                (cx + sx * 2, 42)], fill=(214, 216, 224),
               outline=ink((140, 142, 152)), w=OW * 0.8)
    a.poly([(22, 42), (32, 42), (33, 52), (21, 52)], fill=(214, 216, 224),
           outline=ink((140, 142, 152)), w=OW * 0.8)


def _platebody(a, base, *, trim, emblem=None):
    """A plate torso: rounded pauldrons, banded chest, skirt tassets."""
    edge = ink(base)
    for sx in (1, -1):
        cx = 48 + sx * 28
        a.smooth([(48 + sx * 14, 14), (cx + sx * 2, 10), (cx + sx * 15, 22),
                  (cx + sx * 14, 40), (cx + sx * 2, 44), (48 + sx * 16, 34)],
                 fill=dark(base, 0.14), outline=edge, steps=12)
        a.smooth([(48 + sx * 16, 18), (cx + sx * 2, 14), (cx + sx * 11, 24),
                  (cx + sx * 8, 34), (48 + sx * 18, 32)],
                 fill=lite(base, 0.2) if sx < 0 else dark(base, 0.3),
                 steps=12)
        # bracer
        a.poly([(cx + sx * 14, 40), (cx + sx * 2, 44), (cx + sx * 4, 58),
                (cx + sx * 15, 54)], fill=dark(base, 0.24), outline=edge,
               w=OW * 0.9)
    torso = [(28, 12), (38, 6), (48, 14), (58, 6), (68, 12), (72, 34),
             (68, 62), (48, 70), (28, 62), (24, 34)]
    a.poly(catmull(torso, closed=True, steps=12), fill=base, outline=edge)
    a.poly(catmull([(30, 14), (40, 9), (44, 18), (40, 62), (30, 58),
                    (27, 34)], closed=True, steps=12), fill=lite(base, 0.22))
    a.poly(catmull([(66, 14), (69, 34), (66, 60), (54, 64), (56, 18)],
                   closed=True, steps=12), fill=dark(base, 0.22))
    a.poly(catmull(torso, closed=True, steps=12), outline=edge)
    a.poly([(38, 6), (48, 16), (58, 6), (56, 4), (48, 10), (40, 4)],
           fill=trim, outline=ink(trim), w=OW * 0.9)
    if emblem:
        emblem(a)
    for y in (44, 58):
        a.stroke([(27, y), (69, y)], edge, OW * 0.8)
    a.poly([(28, 62), (68, 62), (66, 74), (30, 74)], fill=dark(base, 0.3),
           outline=edge)


def osrs_rune_platebody(a):
    _platebody(a, RUNE_BLUE, trim=lite(RUNE_BLUE, 0.42))
    a.poly([(40, 20), (56, 20), (48, 34)], fill=dark(RUNE_BLUE, 0.3),
           outline=ink(RUNE_BLUE), w=OW * 0.8)


def _dragon_emblem(a):
    black = (32, 16, 16)
    a.poly([(48, 22), (60, 26), (66, 20), (64, 34), (48, 48), (32, 34),
            (30, 20), (36, 26)], fill=black, outline=ink(black), w=OW * 0.8)
    a.poly([(48, 28), (56, 30), (48, 42), (40, 30)],
           fill=lite(DRAGON_RED, 0.1))


def osrs_dragon_platebody(a):
    _platebody(a, DRAGON_RED, trim=(48, 22, 18), emblem=_dragon_emblem)


def osrs_godsword(a):
    hilt = (74, 80, 108)
    blade_weapon(a, tip=(74, 4), guard=(34, 56), grip=(22, 74),
                 blade_col=OSRS_SILVER, hilt_col=hilt, width=9.0)
    # winged crossguard flourishes
    for sx in (1, -1):
        a.smooth([(34, 56), (34 + sx * 16, 46), (34 + sx * 22, 52),
                  (34 + sx * 12, 58), (34 + sx * 14, 64)],
                 fill=hilt, outline=ink(hilt), w=OW * 0.8, steps=10)
    a.ell((28, 50, 40, 62), fill=(96, 150, 232), outline=ink(hilt),
          w=OW * 0.9)
    a.ell((31, 53, 36, 58), fill=(206, 232, 255))
    for t in (0.34, 0.56, 0.78):
        px = 34 + (74 - 34) * t
        py = 56 + (4 - 56) * t
        a.poly([(px - 4, py + 3), (px + 4, py - 3), (px + 7, py + 1),
                (px - 1, py + 7)], fill=dark(OSRS_SILVER, 0.36),
               outline=ink(OSRS_SILVER), w=OW * 0.5)


def osrs_abyssal_whip(a):
    black = (44, 30, 32)
    red = (128, 42, 38)
    coil = [(30, 6), (48, 14), (44, 30), (24, 34), (14, 48), (26, 62),
            (46, 66), (52, 76)]
    a.cord(coil, black, 6.0, smooth_steps=16)
    for i, (cx, cy) in enumerate(catmull(coil, steps=6)):
        if i % 4 == 1:
            a.ell((cx - 2.6, cy - 2.6, cx + 2.6, cy + 2.6), fill=red)
    a.poly([(52, 76), (44, 70), (58, 62), (58, 74)], fill=red,
           outline=ink(red), w=OW * 0.9)
    a.poly([(30, 4), (22, 2), (24, 12), (34, 10)], fill=dark(black, 0.1),
           outline=ink(black), w=OW * 0.9)


def osrs_dragon_claws(a):
    glove_pair(a, DRAGON_RED, cuff=dark(DRAGON_RED, 0.42),
               talons=lite(DRAGON_RED, 0.16))


# ---------------------------------------------------------------------------
# Wave 7 -- more OSRS gear
# ---------------------------------------------------------------------------

PARTY_RED = (198, 42, 24)


def osrs_party_hat(a):
    """The paper crown, not a cone: five jagged points off a flat band."""
    pts = [(2, 54), (2, 26), (11, 40), (19, 8), (28, 34), (37, 4), (46, 32),
           (55, 10), (63, 40), (68, 24), (68, 54)]
    a.poly(pts, fill=PARTY_RED, outline=ink(PARTY_RED))
    a.poly([(2, 52), (2, 27), (11, 40), (19, 10), (24, 26), (18, 30),
            (14, 52)], fill=lite(PARTY_RED, 0.28))
    a.poly([(52, 14), (55, 12), (63, 40), (68, 26), (68, 52), (56, 52)],
           fill=dark(PARTY_RED, 0.26))
    a.poly(pts, outline=ink(PARTY_RED))
    a.stroke([(3, 46), (67, 46)], dark(PARTY_RED, 0.36), OW * 0.8)


def osrs_santa_hat(a):
    red = (192, 40, 24)
    fur = (242, 240, 234)
    # the cone rises straight out of the fur band and flops to the right
    cone = [(6, 52), (44, 52), (48, 40), (54, 26), (60, 18), (52, 14),
            (40, 26), (26, 38), (14, 46)]
    a.poly(catmull(cone, closed=True, steps=12), fill=red, outline=ink(red))
    a.poly(catmull([(10, 50), (24, 39), (40, 27), (52, 17), (56, 20),
                    (44, 30), (28, 43), (16, 51)], closed=True, steps=12),
           fill=lite(red, 0.26))
    a.poly(catmull(cone, closed=True, steps=12), outline=ink(red))
    a.ell((50, 2, 74, 26), fill=fur, outline=ink((188, 186, 180)))
    a.ell((54, 6, 65, 17), fill=(255, 255, 252))
    a.rrect((1, 46, 47, 63), 8, fill=fur, outline=ink((188, 186, 180)))
    a.rrect((5, 49, 30, 57), 4, fill=(255, 255, 252))


def _fire_cape_texture(a):
    hot = (232, 148, 44)
    for cx, cy, r in ((34, 30, 5), (52, 26, 4), (64, 40, 6), (30, 52, 6),
                      (48, 44, 4), (58, 60, 5), (36, 66, 4), (66, 24, 3),
                      (26, 40, 3.4), (46, 62, 3.6)):
        a.smooth([(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)],
                 fill=hot, steps=10)


def osrs_fire_cape(a):
    cape_body(a, (176, 128, 54), trim=(226, 168, 60),
              clasp=None, texture=_fire_cape_texture)
    a.poly([(38, 8), (58, 8), (56, 16), (40, 16)], fill=(120, 90, 40),
           outline=ink((120, 90, 40)), w=OW * 0.9)


def osrs_max_cape(a):
    cape_body(a, (154, 44, 38), trim=OSRS_GOLD, clasp=OSRS_GOLD)
    for sx in (1, -1):
        a.poly([(48 + sx * 12, 30), (48 + sx * 20, 30), (48 + sx * 18, 62),
                (48 + sx * 10, 62)], fill=OSRS_GOLD, outline=ink(OSRS_GOLD),
               w=OW * 0.8)
        a.poly([(48 + sx * 14, 34), (48 + sx * 18, 34), (48 + sx * 17, 58),
                (48 + sx * 13, 58)], fill=dark(OSRS_GOLD, 0.34))
    a.smooth([(34, 12), (48, 4), (62, 12), (58, 26), (48, 30), (38, 26)],
             fill=dark((154, 44, 38), 0.3), outline=ink((154, 44, 38)),
             steps=12)


def osrs_twisted_bow(a):
    wood = (92, 88, 70)
    string = (176, 200, 96)
    limb = [(30, 4), (44, 14), (36, 30), (26, 45), (36, 60), (44, 76),
            (30, 86)]
    a.cord(limb, wood, 6.0, smooth_steps=16)
    for cx, cy in ((40, 15), (30, 45), (40, 75)):
        a.ell((cx - 4, cy - 4, cx + 4, cy + 4), fill=OSRS_GOLD,
              outline=ink(OSRS_GOLD), w=OW * 0.7)
    a.stroke([(30, 5), (30, 85)], ink(string), OW * 1.1)
    a.stroke([(30, 5), (30, 85)], string, OW * 0.6)
    for cy in (24, 66):
        a.poly([(34, cy - 5), (44, cy), (34, cy + 5)], fill=dark(wood, 0.2),
               outline=ink(wood), w=OW * 0.6)


def osrs_dragon_scimitar(a):
    hilt = (104, 100, 108)
    a.poly(catmull([(20, 62), (34, 40), (56, 20), (82, 8), (74, 26),
                    (58, 42), (40, 58), (26, 68)], closed=True, steps=14),
           fill=DRAGON_RED, outline=ink(DRAGON_RED))
    a.poly(catmull([(28, 60), (40, 42), (58, 26), (76, 14), (68, 26),
                    (50, 44), (34, 62)], closed=True, steps=14),
           fill=lite(DRAGON_RED, 0.32))
    a.poly(catmull([(20, 62), (34, 40), (56, 20), (82, 8), (74, 26),
                    (58, 42), (40, 58), (26, 68)], closed=True, steps=14),
           outline=ink(DRAGON_RED))
    a.poly([(12, 54), (28, 68), (22, 76), (6, 62)], fill=hilt,
           outline=ink(hilt))
    a.cord([(14, 66), (4, 76)], hilt, 4.6)
    a.ell((0, 72, 10, 82), fill=lite(hilt, 0.2), outline=ink(hilt),
          w=OW * 0.8)


def osrs_amulet_of_fury(a):
    cord = (56, 58, 78)
    gem = (176, 40, 52)
    a.cord([(20, 2), (5, 16), (8, 30)], cord, 3.4)
    a.cord([(20, 2), (35, 16), (32, 30)], cord, 3.4)
    a.poly([(20, 22), (32, 28), (28, 40), (12, 40), (8, 28)], fill=cord,
           outline=ink(cord))
    a.poly([(20, 25), (28, 29), (25, 37), (14, 37), (12, 29)], fill=gem,
           outline=ink(gem), w=OW * 0.8)
    a.poly([(19, 27), (24, 30), (21, 35), (16, 33)], fill=lite(gem, 0.4))


def osrs_barrows_gloves(a):
    glove_pair(a, (78, 78, 62), cuff=OSRS_GOLD, palm=(104, 104, 84))
    for cx in (27, 83):
        a.stroke([(cx - 12, 50), (cx + 12, 49)], dark(OSRS_GOLD, 0.34),
                 OW * 0.7)


# ---------------------------------------------------------------------------
# Wave 8 -- Polaroid, Killer Instinct, Cyberpunk
# ---------------------------------------------------------------------------

CYAN = (86, 226, 236)
NEON_YELLOW = (250, 218, 48)


def polaroid_camera(a):
    shell = (238, 238, 236)
    a.rrect((2, 10, 52, 50), 4, fill=shell, outline=ink((150, 150, 152)))
    a.rrect((2, 10, 52, 20), 4, fill=(216, 216, 214))
    for i, col in enumerate(((216, 54, 52), (244, 152, 44), (250, 214, 60),
                             (96, 190, 96), (72, 148, 226))):
        a.poly([(6 + i * 4, 12), (10 + i * 4, 12), (10 + i * 4, 20),
                (6 + i * 4, 20)], fill=col)
    a.rrect((2, 10, 52, 50), 4, fill=None, outline=ink((150, 150, 152)))
    a.ell((14, 22, 38, 46), fill=(58, 58, 68), outline=ink((58, 58, 68)))
    a.ell((18, 26, 34, 42), fill=(96, 150, 200), outline=ink((40, 60, 90)),
          w=OW * 0.8)
    a.ell((21, 29, 27, 35), fill=(214, 236, 255))
    a.rrect((40, 24, 50, 32), 1.6, fill=(250, 226, 96),
            outline=ink((180, 150, 40)), w=OW * 0.7)
    a.rrect((6, 48, 48, 58), 1.5, fill=(252, 252, 250),
            outline=ink((160, 160, 158)), w=OW * 0.8)


def photo_strip(a):
    a.rrect((2, 2, 38, 68), 2, fill=(250, 250, 246),
            outline=ink((150, 150, 148)))
    tints = ((236, 176, 132), (152, 200, 226), (222, 156, 190))
    for i, col in enumerate(tints):
        y = 5 + i * 21
        a.poly([(6, y), (34, y), (34, y + 17), (6, y + 17)], fill=col,
               outline=ink(col), w=OW * 0.7)
        a.ell((15, y + 3, 25, y + 12), fill=dark(col, 0.34))
        a.poly([(11, y + 17), (29, y + 17), (26, y + 11), (14, y + 11)],
               fill=dark(col, 0.2))


def retro_filmstrip(a):
    film = (42, 40, 52)
    a.poly([(2, 26), (148, 14), (148, 84), (2, 72)], fill=film,
           outline=ink(film))
    for y_off in (4, 56):
        for i in range(11):
            x = 8 + i * 13
            a.rrect((x, y_off + 22 - x * 0.08, x + 7,
                     y_off + 30 - x * 0.08), 1.2, fill=(232, 232, 238))
    frames = ((150, 200, 226), (236, 176, 132), (200, 176, 226),
              (168, 212, 168))
    for i, col in enumerate(frames):
        x = 10 + i * 35
        a.poly([(x, 40 - x * 0.08), (x + 30, 38 - x * 0.08),
                (x + 30, 62 - x * 0.08), (x, 64 - x * 0.08)], fill=col,
               outline=ink(film), w=OW * 0.8)
        a.ell((x + 8, 44 - x * 0.08, x + 22, 58 - x * 0.08),
              fill=dark(col, 0.3))


def ki_ultra_combo(a):
    a.glow((10, 10, 140, 90), (120, 200, 255), 100, 10)
    for i in range(9):
        ang = -90 + i * 40
        r0, r1 = 20, 66
        cx, cy = 75, 50
        p0 = (cx + r0 * math.cos(math.radians(ang - 7)),
              cy + r0 * 0.7 * math.sin(math.radians(ang - 7)))
        p1 = (cx + r1 * math.cos(math.radians(ang)),
              cy + r1 * 0.7 * math.sin(math.radians(ang)))
        p2 = (cx + r0 * math.cos(math.radians(ang + 7)),
              cy + r0 * 0.7 * math.sin(math.radians(ang + 7)))
        a.poly([p0, p1, p2], fill=(120, 214, 255),
               outline=ink((40, 120, 200)), w=OW * 0.7)
    a.ell((54, 32, 96, 68), fill=(226, 246, 255), outline=ink((40, 120, 200)))
    bolt = [(78, 34), (66, 50), (74, 50), (70, 66), (86, 46), (78, 46)]
    a.poly(bolt, fill=NEON_YELLOW, outline=ink((176, 130, 20)), w=OW * 0.9)


def ki_ninja_visor(a):
    band = (38, 40, 56)
    a.rrect((1, 4, 45, 16), 4, fill=band, outline=ink(band))
    a.glow((6, 6, 40, 14), (80, 220, 240), 150, 3)
    a.rrect((5, 7, 41, 13), 2.5, fill=CYAN, outline=ink((20, 110, 130)),
            w=OW * 0.7)
    a.poly([(7, 8), (18, 8), (15, 12), (7, 12)], fill=(226, 252, 255))


def ki_energy_blades(a):
    fist = (72, 74, 96)
    for sx, cx in ((1, 26), (-1, 84)):
        a.rrect((cx - 12, 40, cx + 12, 60), 4, fill=fist, outline=ink(fist))
        a.rrect((cx - 9, 43, cx - 1, 56), 2, fill=lite(fist, 0.2))
        blade = [(cx - sx * 5, 42), (cx + sx * 6, 40), (cx + sx * 12, 14),
                 (cx + sx * 4, 2), (cx - sx * 6, 16)]
        a.glow((cx - 16, 0, cx + 16, 46), (100, 230, 250), 130, 5)
        a.poly(catmull(blade, closed=True, steps=12), fill=CYAN,
               outline=ink((18, 120, 140)), w=OW * 0.9)
        a.poly(catmull([(cx - sx * 2, 40), (cx + sx * 4, 38),
                        (cx + sx * 6, 18), (cx + sx * 2, 8),
                        (cx - sx * 2, 20)], closed=True, steps=12),
               fill=(230, 252, 255))


def cyberpunk_neon_visor(a):
    frame = (36, 34, 46)
    # kept a pixel clear of the canvas edge so the chip keeps a true
    # transparent margin at every corner
    a.poly([(2, 5), (44, 3), (44, 13), (33, 17), (13, 17), (2, 13)],
           fill=frame, outline=ink(frame))
    a.glow((6, 5, 40, 13), (250, 60, 90), 140, 3)
    a.poly([(5, 7), (41, 5), (41, 11), (31, 14), (15, 14), (5, 11)],
           fill=(48, 20, 40), outline=ink(frame), w=OW * 0.7)
    a.stroke([(7, 9.5), (39, 8)], (248, 56, 92), OW * 0.8)
    a.poly([(7, 6.5), (16, 6), (13, 9.5), (7, 10)],
           fill=(255, 190, 200, 150))
    for x in (4, 42):
        a.poly([(x - 1.6, 5), (x + 1.6, 5), (x + 1.6, 13), (x - 1.6, 13)],
               fill=NEON_YELLOW)


def _chrome_arm(a, base, accent):
    edge = ink(base)
    a.rrect((16, 2, 44, 22), 5, fill=base, outline=edge)
    a.rrect((19, 5, 30, 19), 3, fill=lite(base, 0.34))
    for i in range(3):
        y = 24 + i * 15
        a.rrect((18, y, 42, y + 12), 4, fill=base, outline=edge)
        a.rrect((21, y + 2, 30, y + 10), 2.5, fill=lite(base, 0.3))
        a.stroke([(19, y + 12), (41, y + 12)], dark(base, 0.4), OW * 0.7)
    a.rrect((14, 68, 46, 88), 5, fill=dark(base, 0.16), outline=edge)
    for x in (20, 28, 36):
        a.rrect((x, 70, x + 6, 86), 2, fill=lite(base, 0.24),
                outline=edge, w=OW * 0.6)
    a.poly([(44, 8), (52, 6), (52, 16), (44, 14)], fill=accent,
           outline=ink(accent), w=OW * 0.8)
    a.stroke([(16, 26), (16, 62)], accent, OW * 0.7)


def cyberpunk_cyberarm(a):
    _chrome_arm(a, (150, 154, 168), (248, 56, 92))


def cyberpunk_mantis_blades(a):
    metal = (176, 180, 194)
    for sx, cx in ((1, 16), (-1, 54)):
        a.rrect((cx - 8, 42, cx + 8, 58), 3, fill=(78, 80, 96),
                outline=ink((78, 80, 96)))
        blade = [(cx - sx * 4, 44), (cx + sx * 6, 42), (cx + sx * 22, 18),
                 (cx + sx * 30, 2), (cx + sx * 16, 12), (cx + sx * 2, 30)]
        a.poly(catmull(blade, closed=True, steps=12), fill=metal,
               outline=ink(metal))
        a.poly(catmull([(cx, 42), (cx + sx * 4, 41), (cx + sx * 18, 18),
                        (cx + sx * 26, 5), (cx + sx * 14, 18),
                        (cx + sx * 2, 34)], closed=True, steps=12),
               fill=lite(metal, 0.4))
        a.stroke([(cx + sx * 3, 38), (cx + sx * 24, 8)], (248, 56, 92),
                 OW * 0.6)


def cyberpunk_night_city(a):
    a.rrect((2, 6, 148, 94), 4, fill=(20, 16, 34), outline=ink((20, 16, 34)))
    a.glow((10, 30, 140, 92), (180, 60, 220), 90, 10)
    towers = ((10, 44, 16), (28, 30, 14), (44, 52, 18), (64, 22, 16),
              (82, 40, 14), (98, 28, 20), (120, 48, 16), (138, 36, 10))
    cols = ((236, 60, 120), (86, 226, 236), (150, 96, 236), (250, 218, 48))
    for i, (x, top, w) in enumerate(towers):
        col = cols[i % len(cols)]
        a.poly([(x, top), (x + w, top), (x + w, 90), (x, 90)],
               fill=(34, 26, 54), outline=ink(col), w=OW * 0.7)
        for r in range(int((90 - top) // 9)):
            for c in range(max(1, w // 6)):
                a.poly([(x + 2 + c * 6, top + 4 + r * 9),
                        (x + 5 + c * 6, top + 4 + r * 9),
                        (x + 5 + c * 6, top + 8 + r * 9),
                        (x + 2 + c * 6, top + 8 + r * 9)], fill=col)
        a.stroke([(x, top), (x + w, top)], col, OW * 0.8)
    a.stroke([(4, 90), (146, 90)], (86, 226, 236), OW * 0.9)


# ---------------------------------------------------------------------------
# Wave 9 -- Silverhand, Judy, Spinal
# ---------------------------------------------------------------------------

def silverhand_jacket(a):
    leather = (44, 42, 54)
    shell_body(a, leather, sleeve=dark(leather, 0.1), hem=dark(leather, 0.3),
               waist=32)
    # open front over a white tee
    a.poly([(40, 12), (56, 12), (55, 72), (41, 72)], fill=(236, 234, 228),
           outline=ink((190, 188, 182)))
    a.poly([(26, 8), (44, 16), (42, 72), (28, 72)], fill=leather,
           outline=ink(leather))
    a.poly([(70, 8), (52, 16), (54, 72), (68, 72)], fill=dark(leather, 0.16),
           outline=ink(leather))
    # lapels and shoulder studs
    a.poly([(28, 8), (44, 16), (38, 32), (26, 24)], fill=lite(leather, 0.16),
           outline=ink(leather), w=OW * 0.8)
    a.poly([(68, 8), (52, 16), (58, 32), (70, 24)], fill=dark(leather, 0.3),
           outline=ink(leather), w=OW * 0.8)
    for cx, cy in ((24, 22), (30, 18), (72, 22), (66, 18), (27, 30),
                   (69, 30)):
        a.ell((cx - 2, cy - 2, cx + 2, cy + 2), fill=(206, 208, 218),
              outline=ink((150, 152, 162)), w=OW * 0.5)
    a.poly([(44, 20), (52, 20), (52, 26), (44, 26)], fill=(214, 44, 60),
           outline=ink((214, 44, 60)), w=OW * 0.6)


def chrome_rock_arm(a):
    _chrome_arm(a, (188, 192, 204), (236, 64, 52))


def aviator_shades(a):
    lens = [(3, 3), (20, 3), (21, 9), (14, 17), (5, 15), (2, 8)]
    eyewear(a, lens, lens=(58, 52, 46), frame=OSRS_GOLD, bridge_y=6)
    a.stroke([(18, 4), (28, 4)], OSRS_GOLD, OW * 0.9)


def rockerboy_guitar(a):
    """A double-cutaway electric: waisted body, long neck, angled headstock."""
    black = (38, 36, 48)
    wood = (104, 74, 48)
    edge = ink(black)
    # neck and headstock, drawn first so the body sits over the heel
    a.poly([(26, 52), (37, 48), (52, 8), (43, 5)], fill=wood,
           outline=ink(wood))
    a.poly([(29, 50), (34, 49), (49, 9), (45, 8)], fill=lite(wood, 0.26))
    a.poly([(41, 10), (58, 2), (62, 12), (46, 20)], fill=black,
           outline=edge, w=OW * 0.9)
    for i in range(3):
        a.ell((47 + i * 4.4, 5 + i * 2.0, 51 + i * 4.4, 9 + i * 2.0),
              fill=(208, 210, 220), outline=edge, w=OW * 0.45)
    for i in range(4):
        a.stroke([(28.5 + i * 2.0, 51), (44.5 + i * 1.6, 9)],
                 (220, 220, 226), OW * 0.3)
    for t in (0.25, 0.45, 0.65, 0.85):
        a.stroke([(26 + (43 - 26) * t, 52 - (52 - 5) * t),
                  (37 + (52 - 37) * t, 48 - (48 - 8) * t)],
                 (170, 172, 182), OW * 0.35)
    # waisted double-cutaway: two bouts pinched at y=64, horns at the top
    body = [(36, 44), (44, 52), (46, 58), (42, 63), (48, 70), (46, 79),
            (36, 84), (22, 84), (11, 78), (8, 69), (14, 62), (10, 57),
            (12, 50), (20, 45), (26, 52), (30, 52)]
    a.poly(catmull(body, closed=True, steps=8), fill=black, outline=edge)
    a.poly(catmull([(34, 48), (41, 55), (38, 63), (43, 71), (36, 80),
                    (24, 80), (15, 73), (17, 63), (14, 55), (21, 49),
                    (28, 54)], closed=True, steps=8), fill=lite(black, 0.22))
    a.poly(catmull(body, closed=True, steps=8), outline=edge)
    # pickguard, pickups, bridge
    a.poly([(30, 54), (40, 66), (34, 79), (21, 79), (16, 66), (22, 55)],
           fill=(214, 40, 58), outline=ink((214, 40, 58)), w=OW * 0.8)
    for y in (62, 71):
        a.rrect((19, y, 33, y + 4), 1.2, fill=(206, 208, 218),
                outline=ink((140, 142, 152)), w=OW * 0.5)
    a.rrect((17, 77, 35, 81), 1.2, fill=(180, 182, 194),
            outline=ink((130, 132, 142)), w=OW * 0.5)


def judy_twin_braids(a):
    hair = (52, 44, 72)
    teal = (58, 190, 196)
    # crown sweeping over the head opening at (29, 4, 71, 50)
    a.smooth([(50, 0), (74, 8), (82, 30), (78, 50), (68, 32), (54, 22),
              (42, 22), (28, 32), (20, 50), (16, 28), (26, 8)],
             fill=hair, outline=ink(hair), steps=14)
    a.smooth([(46, 4), (66, 12), (74, 30), (70, 38), (58, 20), (40, 18),
              (26, 30), (24, 20), (32, 8)], fill=lite(hair, 0.24), steps=14)
    for sx, cx in ((1, 76), (-1, 22)):
        pts = [(cx, 34), (cx + sx * 8, 50), (cx + sx * 2, 66),
               (cx + sx * 8, 82)]
        a.cord(pts, teal, 7.0)
        for i, (bx, by) in enumerate(catmull(pts, steps=5)):
            if i % 3 == 1:
                a.ell((bx - 4, by - 3, bx + 4, by + 3), fill=dark(teal, 0.24),
                      outline=ink(teal), w=OW * 0.5)
        a.cord([(cx + sx * 7, 80), (cx + sx * 9, 88)], (236, 72, 120), 3.0)


def welding_goggles(a):
    brass = (192, 148, 62)
    strap = (92, 68, 44)
    a.poly([(0, 6), (46, 6), (46, 14), (0, 14)], fill=strap,
           outline=ink(strap), w=OW * 0.8)
    for cx in (12, 34):
        a.ell((cx - 10, 1, cx + 10, 21), fill=brass, outline=ink(brass))
        a.ell((cx - 6.5, 4.5, cx + 6.5, 17.5), fill=(52, 68, 58),
              outline=ink(brass), w=OW * 0.7)
        a.ell((cx - 4.5, 6, cx - 0.5, 10), fill=(150, 200, 176))
    a.poly([(20, 8), (26, 8), (26, 13), (20, 13)], fill=brass,
           outline=ink(brass), w=OW * 0.6)


def mechanic_overalls(a):
    denim = (58, 88, 148)
    tee = (232, 230, 224)
    shell_body(a, tee, sleeve=tee, hem=dark(tee, 0.16), waist=32)
    # bib and straps over the tee
    a.poly([(34, 30), (62, 30), (64, 72), (32, 72)], fill=denim,
           outline=ink(denim))
    a.poly([(34, 30), (46, 30), (45, 72), (32, 72)], fill=lite(denim, 0.2))
    a.poly([(34, 30), (62, 30), (64, 72), (32, 72)], outline=ink(denim))
    for sx in (1, -1):
        a.poly([(48 + sx * 12, 30), (48 + sx * 18, 30), (48 + sx * 21, 8),
                (48 + sx * 15, 8)], fill=denim, outline=ink(denim),
               w=OW * 0.9)
        a.rrect((48 + sx * 12 - 3, 26, 48 + sx * 12 + 3, 32), 1,
                fill=(212, 176, 76), outline=ink((160, 130, 50)), w=OW * 0.5)
    a.rrect((38, 40, 58, 56), 1.5, fill=dark(denim, 0.2),
            outline=ink(denim), w=OW * 0.8)
    a.stroke([(38, 44), (58, 44)], lite(denim, 0.3), OW * 0.5)


def spinal_skull_mask(a):
    bone = (238, 236, 226)
    a.smooth([(20, 1), (32, 5), (37, 13), (32, 22), (20, 26), (8, 22),
              (3, 13), (8, 5)], fill=bone, outline=ink((150, 146, 136)),
             steps=14)
    a.smooth([(18, 4), (28, 7), (31, 13), (26, 20), (16, 22), (9, 17),
              (8, 8)], fill=(255, 254, 248), steps=14)
    a.smooth([(20, 1), (32, 5), (37, 13), (32, 22), (20, 26), (8, 22),
              (3, 13), (8, 5)], outline=ink((150, 146, 136)), steps=14)
    for cx in (13, 27):
        a.smooth([(cx, 7), (cx + 5, 11), (cx + 3, 17), (cx - 3, 17),
                  (cx - 5, 11)], fill=(30, 24, 34), steps=12)
        a.ell((cx - 2, 10, cx + 1, 13), fill=(230, 60, 50))
    a.poly([(17, 17), (23, 17), (20, 22)], fill=(30, 24, 34))
    for i in range(5):
        x = 10 + i * 5
        a.poly([(x, 22), (x + 3.4, 22), (x + 3.4, 26.5), (x, 26.5)],
               fill=(214, 212, 202), outline=ink((150, 146, 136)),
               w=OW * 0.4)


def pirate_bandana(a):
    red = (196, 44, 46)
    wrap = [(4, 44), (6, 24), (22, 12), (42, 8), (62, 12), (76, 24), (78, 44)]
    a.poly(catmull(wrap, closed=True, steps=14), fill=red, outline=ink(red))
    a.poly(catmull([(8, 42), (10, 26), (24, 15), (42, 12), (40, 22),
                    (22, 28), (14, 42)], closed=True, steps=14),
           fill=lite(red, 0.26))
    a.poly(catmull([(62, 14), (74, 26), (76, 42), (58, 42), (56, 20)],
                   closed=True, steps=14), fill=dark(red, 0.24))
    a.poly(catmull(wrap, closed=True, steps=14), outline=ink(red))
    for cx, cy, r in ((20, 22, 3.4), (34, 17, 3.4), (48, 17, 3.4),
                      (62, 22, 3.4), (27, 32, 3), (55, 32, 3), (41, 28, 3)):
        a.ell((cx - r, cy - r, cx + r, cy + r), fill=(248, 246, 240),
              outline=ink(red), w=OW * 0.5)
    # knot and tails on the left
    a.ell((2, 32, 16, 46), fill=dark(red, 0.14), outline=ink(red), w=OW * 0.9)
    for pts in ([(6, 42), (0, 52), (10, 54), (12, 44)],
                [(10, 44), (10, 56), (20, 52), (16, 44)]):
        a.poly(catmull(pts, closed=True, steps=10), fill=red,
               outline=ink(red), w=OW * 0.9)


def twin_cutlasses(a):
    steel = (198, 202, 214)
    hilt = (150, 116, 60)
    for sx, cx in ((1, 20), (-1, 90)):
        a.poly(catmull([(cx - sx * 2, 62), (cx + sx * 10, 40),
                        (cx + sx * 20, 12), (cx + sx * 14, 4),
                        (cx + sx * 2, 30), (cx - sx * 8, 56)],
                       closed=True, steps=14), fill=steel, outline=ink(steel))
        a.poly(catmull([(cx, 58), (cx + sx * 10, 38), (cx + sx * 17, 12),
                        (cx + sx * 12, 12), (cx + sx * 3, 36),
                        (cx - sx * 4, 54)], closed=True, steps=14),
               fill=lite(steel, 0.42))
        a.poly(catmull([(cx - sx * 2, 62), (cx + sx * 10, 40),
                        (cx + sx * 20, 12), (cx + sx * 14, 4),
                        (cx + sx * 2, 30), (cx - sx * 8, 56)],
                       closed=True, steps=14), outline=ink(steel))
        a.poly([(cx - sx * 12, 56), (cx + sx * 2, 62), (cx - sx * 2, 68),
                (cx - sx * 14, 62)], fill=hilt, outline=ink(hilt))
        a.cord([(cx - sx * 6, 64), (cx - sx * 12, 68)], hilt, 4.2)
        a.poly(catmull([(cx - sx * 12, 60), (cx - sx * 20, 62),
                        (cx - sx * 18, 68), (cx - sx * 10, 68)],
                       closed=True, steps=10), fill=dark(hilt, 0.2),
               outline=ink(hilt), w=OW * 0.8)


def ghost_flame_aura(a):
    green = (110, 226, 150)
    a.glow((14, 10, 136, 92), (130, 255, 180), 100, 11)
    for i, (x, h, w) in enumerate(((22, 40, 12), (44, 62, 15), (68, 74, 17),
                                   (94, 58, 15), (118, 38, 12))):
        base_y = 94
        flame = [(x, base_y), (x + w, base_y - h * 0.35),
                 (x + w * 0.45, base_y - h * 0.7),
                 (x + w * 0.75, base_y - h),
                 (x - w * 0.2, base_y - h * 0.75),
                 (x - w * 0.55, base_y - h * 0.3)]
        a.poly(catmull(flame, closed=True, steps=14),
               fill=(*green, 190), outline=ink(green), w=OW * 0.9)
        a.poly(catmull([(x, base_y - 4), (x + w * 0.5, base_y - h * 0.4),
                        (x + w * 0.2, base_y - h * 0.72),
                        (x - w * 0.2, base_y - h * 0.4)],
                       closed=True, steps=12), fill=(214, 255, 226, 210))
    for cx, cy, r in ((30, 24, 3.4), (74, 12, 3.0), (112, 26, 2.6),
                      (52, 20, 2.2), (94, 18, 2.4)):
        a.ell((cx - r, cy - r, cx + r, cy + r), fill=(190, 255, 214),
              outline=ink(green), w=OW * 0.5)


# ---------------------------------------------------------------------------
# Registry and entry point
# ---------------------------------------------------------------------------

# kind -> (canvas width, canvas height, draw function).  The canvas size is
# the chip's declared size from IPOD_EXCLUSIVE_CHIPS, or the sub-size the
# packager containers the source into before compositing a hand onto it.
CHIPS = {
    "hero-cap": (84, 56, hero_cap),
    "hero-tunic": (SHELL_W, SHELL_H, hero_tunic),
    "hylian-shield": (52, 70, hylian_shield),
    "forest-eye-mask": (46, 20, forest_eye_mask),
    "ocarina-charm": (46, 54, ocarina_charm),
    "heart-container": (44, 58, heart_container),
    "triforce-halo": (140, 96, triforce_halo),
    "fairy-companion": (100, 86, fairy_companion),

    "hasan-cap": (84, 58, hasan_cap),
    "hasan-headset": (54, 66, hasan_headset),
    "hasan-hoodie": (SHELL_W, SHELL_H, hasan_hoodie),
    "hasan-news-aura": (150, 90, hasan_news_aura),
    "qtc-ponytail": (100, 94, qtc_ponytail),
    "qtc-visor": (46, 20, qtc_visor),
    "qtc-trophy": (46, 58, qtc_trophy),
    "qtc-spotlight-aura": (140, 110, qtc_spotlight_aura),

    "maya-ranger-hat": (88, 52, maya_ranger_hat),
    "maya-falcon-glove": (110, 64, maya_falcon_glove),
    "maya-field-vest": (SHELL_W, SHELL_H, maya_field_vest),
    "maya-forest-aura": (150, 100, maya_forest_aura),
    "habs-home-jersey": (SHELL_W, SHELL_H, habs_home_jersey),
    "habs-hockey-stick": (100, 90, habs_hockey_stick),
    "habs-winter-toque": (84, 58, habs_winter_toque),
    "habs-rink-aura": (150, 100, habs_rink_aura),
    "spyro-companion": (60, 48, spyro_companion),
    "cat-companion": (50, 40, cat_companion),
    "dog-companion": (54, 42, dog_companion),
    "turtle-companion": (52, 34, turtle_companion),
    "navi-companion": (40, 40, navi_companion),

    "pink-floyd-pig": (140, 90, pink_floyd_pig),
    "pink-floyd-prism": (150, 100, pink_floyd_prism),
    "pink-floyd-hammers": (64, 64, pink_floyd_hammers),
    "portal-gun": (60, 50, portal_gun),
    "companion-cube": (56, 56, companion_cube),
    "long-fall-boots": (110, 60, long_fall_boots),
    "aperture-turret": (54, 54, aperture_turret),
    "osrs-rune-platebody": (SHELL_W, SHELL_H, osrs_rune_platebody),
    "osrs-dragon-platebody": (SHELL_W, SHELL_H, osrs_dragon_platebody),
    "osrs-godsword": (100, 90, osrs_godsword),
    "osrs-abyssal-whip": (60, 80, osrs_abyssal_whip),
    "osrs-dragon-claws": (110, 64, osrs_dragon_claws),

    "osrs-party-hat": (70, 60, osrs_party_hat),
    "osrs-santa-hat": (76, 66, osrs_santa_hat),
    "osrs-fire-cape": (SHELL_W, SHELL_H, osrs_fire_cape),
    "osrs-max-cape": (SHELL_W, SHELL_H, osrs_max_cape),
    "osrs-twisted-bow": (60, 90, osrs_twisted_bow),
    "osrs-dragon-scimitar": (90, 80, osrs_dragon_scimitar),
    "osrs-amulet-of-fury": (40, 45, osrs_amulet_of_fury),
    "osrs-barrows-gloves": (110, 64, osrs_barrows_gloves),

    "polaroid-camera": (54, 60, polaroid_camera),
    "photo-strip": (40, 70, photo_strip),
    "retro-filmstrip": (150, 100, retro_filmstrip),
    "ki-ultra-combo": (150, 100, ki_ultra_combo),
    "ki-ninja-visor": (46, 20, ki_ninja_visor),
    "ki-energy-blades": (110, 70, ki_energy_blades),
    "cyberpunk-neon-visor": (46, 20, cyberpunk_neon_visor),
    "cyberpunk-cyberarm": (60, 90, cyberpunk_cyberarm),
    "cyberpunk-mantis-blades": (70, 60, cyberpunk_mantis_blades),
    "cyberpunk-night-city": (150, 100, cyberpunk_night_city),

    "silverhand-jacket": (SHELL_W, SHELL_H, silverhand_jacket),
    "chrome-rock-arm": (60, 90, chrome_rock_arm),
    "aviator-shades": (46, 20, aviator_shades),
    "rockerboy-guitar": (60, 84, rockerboy_guitar),
    "judy-twin-braids": (100, 90, judy_twin_braids),
    "welding-goggles": (46, 22, welding_goggles),
    "mechanic-overalls": (SHELL_W, SHELL_H, mechanic_overalls),
    "spinal-skull-mask": (40, 28, spinal_skull_mask),
    "pirate-bandana": (84, 58, pirate_bandana),
    "twin-cutlasses": (110, 70, twin_cutlasses),
    "ghost-flame-aura": (150, 100, ghost_flame_aura),
}


def render(kind: str) -> Image.Image:
    w, h, fn = CHIPS[kind]
    art = Art(w, h)
    fn(art)
    return art.out()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=OUT_DEFAULT)
    ap.add_argument("--only", default=None,
                    help="comma-separated subset of chip kinds to render")
    args = ap.parse_args()

    kinds = (args.only.split(",") if args.only else list(CHIPS))
    args.out.mkdir(parents=True, exist_ok=True)
    for kind in kinds:
        image = render(kind.strip())
        path = args.out / f"{kind.strip()}.png"
        image.save(path)
        print(f"  {path.name}: {image.size}")
    print(f"rendered {len(kinds)} chip sources into {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
