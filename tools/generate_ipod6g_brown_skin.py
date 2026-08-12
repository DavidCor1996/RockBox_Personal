#!/usr/bin/env python3
"""Generate the brown iPod 6G simulator body (uisimulator/bitmaps/UI-ipod6g.bmp).

The stock artwork is a silver body overprinted with red keypad numbers, which
are a developer aid rather than part of a real iPod.  This draws the body from
scratch instead, so the panel looks like the physical player and carries no
annotations.

Every control position is taken from uisimulator/buttonmap/ipod.c and the LCD
window from firmware/target/hosted/sdl/sim-ui-defines.h.  Those tables drive
the simulator's hit testing, so the artwork and the click regions must keep
matching: change one and you must change the other.
"""

import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

WIDTH, HEIGHT = 350, 591
LCD_X, LCD_Y, LCD_W, LCD_H = 14, 12, 320, 240

WHEEL_CX, WHEEL_CY = 175, 432       # SDLK_KP_5 "Select" centre
WHEEL_OUTER = 118
WHEEL_INNER = 45                    # SDLK_KP_5 radius
MENU_Y, PLAY_Y = 350, 539           # SDLK_KP_PERIOD / SDLK_KP_PLUS centres
PREV_X, NEXT_X = 75, 275            # SDLK_KP_4 / SDLK_KP_6 centres

# Elite Obsolete Electronics "Earth Brown Metal" shell: a warm, matte,
# medium-dark anodised brown.  EOE publish no hex value, so this is matched to
# their product photography; adjust here if it reads too light or too red.
BODY_TOP = (122, 86, 60)
BODY_BOTTOM = (98, 68, 46)

# White click wheel, as on the two-tone Earth Brown builds.
WHEEL_EDGE = (222, 220, 215)
WHEEL_FACE = (246, 245, 242)

# Select button matches the shell.
CENTRE_EDGE = (104, 72, 49)
CENTRE_FACE = (128, 91, 63)

# Wheel legends are engraved into the white wheel, so they read as grey.
LABEL = (128, 124, 118)

FONT_CANDIDATES = [
    "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
]


def load_font(size):
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def vertical_gradient(size, top, bottom):
    width, height = size
    image = Image.new("RGB", (1, height))
    pixels = image.load()
    for y in range(height):
        t = y / max(1, height - 1)
        pixels[0, y] = tuple(int(top[i] + (bottom[i] - top[i]) * t)
                             for i in range(3))
    return image.resize((width, height))


def radial_shade(draw, cx, cy, outer, inner, edge, face):
    """Shade the wheel from its rim inwards so it reads as a dished disc."""
    steps = outer - inner
    for step in range(steps):
        radius = outer - step
        t = step / max(1, steps - 1)
        colour = tuple(int(edge[i] + (face[i] - edge[i]) * t) for i in range(3))
        draw.ellipse([cx - radius, cy - radius, cx + radius, cy + radius],
                     fill=colour)


def play_glyph(draw, cx, cy, colour):
    draw.polygon([(cx - 11, cy - 7), (cx - 11, cy + 7), (cx - 1, cy)],
                 fill=colour)
    draw.rectangle([cx + 3, cy - 7, cx + 5, cy + 7], fill=colour)
    draw.rectangle([cx + 8, cy - 7, cx + 10, cy + 7], fill=colour)


def skip_glyph(draw, cx, cy, colour, forward):
    d = 1 if forward else -1
    for offset in (-6, 1):
        tip = cx + d * (offset + 7)
        base = cx + d * offset
        draw.polygon([(base, cy - 7), (base, cy + 7), (tip, cy)], fill=colour)
    bar = cx + d * 10
    draw.rectangle([min(bar, bar + d * 2), cy - 7,
                    max(bar, bar + d * 2), cy + 7], fill=colour)


def brushed_metal(size, base, sweep, seed=7):
    """Anodised aluminium: a broad diagonal sheen plus fine brushing.

    A flat gradient reads as a drawing.  Real anodised aluminium has a wide
    specular sweep across the face and very fine directional grain, so both
    are generated here rather than approximated with a single ramp.
    """
    width, height = size
    rnd = random.Random(seed)
    image = Image.new("RGB", (width, height))
    px = image.load()

    # One noise row reused per scanline gives horizontal brushing.
    grain = [rnd.gauss(0, 3.1) for _ in range(width)]
    for y in range(height):
        # Diagonal specular sweep, brightest toward the upper left.
        v = (y / height) * 0.72 + 0.14
        row_shift = rnd.gauss(0, 1.1)
        for x in range(width):
            d = (x / width) * 0.4 + (1.0 - v)
            sheen = math.sin(d * math.pi) ** 2
            k = 1.0 - 0.28 * (y / height) + sweep * sheen * 0.34
            n = grain[x] + row_shift
            px[x, y] = (
                _clamp(base[0] * k + n),
                _clamp(base[1] * k + n),
                _clamp(base[2] * k + n),
            )
    return image


def _clamp(v):
    return 0 if v < 0 else (255 if v > 255 else int(v))


def radial_metal(size, centre, outer, base, seed=11):
    """Circular brushing for the click wheel, as on the real part."""
    width, height = size
    rnd = random.Random(seed)
    image = Image.new("RGB", (width, height))
    px = image.load()
    cx, cy = centre
    rings = [rnd.gauss(0, 2.4) for _ in range(outer + 2)]
    for y in range(height):
        for x in range(width):
            dx, dy = x - cx, y - cy
            r = math.hypot(dx, dy)
            a = math.atan2(dy, dx)
            # Light from the upper left across the disc.
            k = 1.0 + 0.055 * math.cos(a - 2.3) - 0.10 * (r / max(1.0, outer))
            n = rings[min(int(r), outer + 1)] + math.sin(a * 9.0) * 1.2
            px[x, y] = (
                _clamp(base[0] * k + n),
                _clamp(base[1] * k + n),
                _clamp(base[2] * k + n),
            )
    return image


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    target = os.path.join(root, "uisimulator", "bitmaps", "UI-ipod6g.bmp")

    image = brushed_metal((WIDTH, HEIGHT), BODY_TOP, sweep=1.0)

    # Draw the wheel on its own layer so it can be softened without blurring
    # the LCD edge or the body gradient.
    wheel = radial_metal((WIDTH, HEIGHT), (WHEEL_CX, WHEEL_CY),
                         WHEEL_OUTER, WHEEL_FACE)
    wheel = wheel.filter(ImageFilter.GaussianBlur(0.6))

    mask = Image.new("L", (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(mask).ellipse(
        [WHEEL_CX - WHEEL_OUTER, WHEEL_CY - WHEEL_OUTER,
         WHEEL_CX + WHEEL_OUTER, WHEEL_CY + WHEEL_OUTER], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(0.8))
    image.paste(wheel, (0, 0), mask)

    draw = ImageDraw.Draw(image)

    # Centre select button, dished the opposite way to catch the light.
    centre = radial_metal((WIDTH, HEIGHT), (WHEEL_CX, WHEEL_CY),
                          WHEEL_INNER, CENTRE_FACE, seed=23)
    centre = centre.filter(ImageFilter.GaussianBlur(0.5))
    cmask = Image.new("L", (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(cmask).ellipse(
        [WHEEL_CX - WHEEL_INNER, WHEEL_CY - WHEEL_INNER,
         WHEEL_CX + WHEEL_INNER, WHEEL_CY + WHEEL_INNER], fill=255)
    cmask = cmask.filter(ImageFilter.GaussianBlur(0.7))
    image.paste(centre, (0, 0), cmask)
    # Shadow line where the button sits below the wheel face.
    draw.arc([WHEEL_CX - WHEEL_INNER, WHEEL_CY - WHEEL_INNER,
              WHEEL_CX + WHEEL_INNER, WHEEL_CY + WHEEL_INNER],
             200, 340, fill=(90, 62, 42))

    font = load_font(15)
    text = "MENU"
    box = draw.textbbox((0, 0), text, font=font)
    draw.text((WHEEL_CX - (box[2] - box[0]) / 2 - box[0],
               MENU_Y - (box[3] - box[1]) / 2 - box[1]),
              text, font=font, fill=LABEL)

    skip_glyph(draw, PREV_X, WHEEL_CY, LABEL, forward=False)
    skip_glyph(draw, NEXT_X, WHEEL_CY, LABEL, forward=True)
    play_glyph(draw, WHEEL_CX, PLAY_Y, LABEL)

    # LCD window: a black panel with a slightly darker bezel.  The simulator
    # composites the real framebuffer over this rectangle.
    draw.rounded_rectangle([LCD_X - 3, LCD_Y - 3,
                            LCD_X + LCD_W + 2, LCD_Y + LCD_H + 2],
                           radius=6, fill=(96, 68, 42))
    draw.rectangle([LCD_X, LCD_Y, LCD_X + LCD_W - 1, LCD_Y + LCD_H - 1],
                   fill=(0, 0, 0))

    image.save(target)
    print("wrote %s (%dx%d)" % (target, WIDTH, HEIGHT))
    preview = os.environ.get("ROCKPOD_SKIN_PREVIEW")
    if preview:
        image.save(preview)
    return 0


if __name__ == "__main__":
    sys.exit(main())
