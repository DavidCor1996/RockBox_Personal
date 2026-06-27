#!/usr/bin/env python3
"""Generate the Forest Rockbox theme bitmap assets.

The script intentionally reads dimensions from the cloned Forest assets before
overwriting them. That keeps the iPone-derived SBS/WPS/FMS coordinates stable
while replacing the visual language.
"""

from __future__ import annotations

import argparse
import math
import os
import random
import shutil
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter
from PIL import ImageOps


ROOT = Path(__file__).resolve().parents[1]
FOREST_DIR = ROOT / "wps" / "Forest"
ICON_PATH = ROOT / "icons" / "Forest.bmp"
BACKDROP_PATH = ROOT / "backdrops" / "Forest_bd.bmp"
SOURCE_DIR = ROOT / "assets" / "forest_theme"
IPONE_DIR = ROOT / "wps" / "iPone"

PALETTE = {
    "paper": (250, 248, 239),
    "paper2": (244, 239, 224),
    "moss": (74, 119, 73),
    "fern": (126, 166, 91),
    "lichen": (184, 213, 139),
    "forest": (23, 36, 29),
    "bark": (91, 77, 53),
    "ink": (32, 50, 40),
    "muted": (110, 103, 88),
    "mushroom": (199, 88, 68),
    "spore": (215, 168, 78),
    "creek": (79, 140, 163),
    "vellum": (238, 232, 211),
    "shadow": (40, 47, 35),
    "white": (255, 255, 255),
    "black": (0, 0, 0),
}


def lerp(a: int, b: int, t: float) -> int:
    return int(a + (b - a) * t)


def mix(c1: tuple[int, int, int], c2: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return tuple(lerp(c1[i], c2[i], t) for i in range(3))


def rgba(c: tuple[int, int, int], a: int) -> tuple[int, int, int, int]:
    return (c[0], c[1], c[2], a)


def crop_cover(img: Image.Image, size: tuple[int, int], x_bias: float = 0.5, y_bias: float = 0.5) -> Image.Image:
    src = img.convert("RGB")
    sw, sh = src.size
    tw, th = size
    scale = max(tw / sw, th / sh)
    nw, nh = int(sw * scale + 0.5), int(sh * scale + 0.5)
    src = src.resize((nw, nh), Image.Resampling.LANCZOS)
    left = max(0, min(nw - tw, int((nw - tw) * x_bias)))
    top = max(0, min(nh - th, int((nh - th) * y_bias)))
    return src.crop((left, top, left + tw, top + th))


def paper_texture(size: tuple[int, int], base: tuple[int, int, int] = PALETTE["paper"], seed: int = 0) -> Image.Image:
    random.seed(seed)
    w, h = size
    img = Image.new("RGB", size, base)
    px = img.load()
    for y in range(h):
        for x in range(w):
            n = random.randint(-5, 5)
            wave = int(3 * math.sin((x + seed) / 17.0) + 2 * math.cos((y + seed) / 11.0))
            px[x, y] = tuple(max(0, min(255, v + n + wave)) for v in base)
    return img.filter(ImageFilter.GaussianBlur(0.25))


def tint(img: Image.Image, color: tuple[int, int, int], amount: float) -> Image.Image:
    overlay = Image.new("RGB", img.size, color)
    return Image.blend(img.convert("RGB"), overlay, amount)


def recolor_template(template_name: str,
                     dark: tuple[int, int, int],
                     light: tuple[int, int, int],
                     mid: tuple[int, int, int] | None = None) -> Image.Image:
    template_path = IPONE_DIR / template_name
    if not template_path.exists():
        raise FileNotFoundError(template_path)
    with Image.open(template_path) as src:
        gray = ImageOps.autocontrast(src.convert("L"))
    if mid is None:
        return ImageOps.colorize(gray, black=dark, white=light)
    return ImageOps.colorize(gray, black=dark, white=light, mid=mid)


def vignette(img: Image.Image, color: tuple[int, int, int], strength: int = 120) -> Image.Image:
    w, h = img.size
    mask = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(mask)
    d.ellipse((-w * 0.45, -h * 0.55, w * 1.45, h * 1.55), fill=255)
    mask = ImageChops.invert(mask.filter(ImageFilter.GaussianBlur(max(w, h) // 8)))
    shade = Image.new("RGB", img.size, color)
    alpha = ImageEnhance.Contrast(mask).enhance(1.4).point(lambda p: min(strength, p))
    return Image.composite(shade, img.convert("RGB"), alpha)


def draw_leaf(draw: ImageDraw.ImageDraw, x: int, y: int, s: int, color: tuple[int, int, int], angle: float = 0.0) -> None:
    dx = math.cos(angle) * s
    dy = math.sin(angle) * s
    x_a, x_b = int(x - dx), int(x + dx)
    y_a, y_b = int(y - dy), int(y + dy)
    box = (min(x_a, x_b), min(y_a, y_b), max(x_a, x_b), max(y_a, y_b))
    draw.ellipse(box, fill=color)
    draw.line((x, y, int(x + dx), int(y + dy)), fill=mix(color, PALETTE["bark"], 0.35), width=max(1, s // 7))


def draw_mushroom(draw: ImageDraw.ImageDraw, cx: int, cy: int, s: int) -> None:
    cap = PALETTE["mushroom"]
    stem = (229, 213, 176)
    draw.rounded_rectangle((cx - s // 5, cy - s // 4, cx + s // 5, cy + s // 2), radius=max(1, s // 8), fill=stem, outline=mix(stem, PALETTE["bark"], 0.25))
    draw.pieslice((cx - s, cy - s, cx + s, cy + s // 2), 180, 360, fill=cap, outline=mix(cap, PALETTE["bark"], 0.25))
    for ox, oy in [(-0.45, -0.45), (0.0, -0.62), (0.42, -0.36), (-0.1, -0.22)]:
        r = max(1, s // 8)
        draw.ellipse((cx + int(ox * s) - r, cy + int(oy * s) - r, cx + int(ox * s) + r, cy + int(oy * s) + r), fill=PALETTE["paper"])


def draw_spores(draw: ImageDraw.ImageDraw, size: tuple[int, int], count: int, seed: int) -> None:
    random.seed(seed)
    w, h = size
    for _ in range(count):
        x = random.randrange(max(1, w))
        y = random.randrange(max(1, h))
        r = random.choice([1, 1, 2])
        color = PALETTE["spore"] if random.random() < 0.65 else PALETTE["lichen"]
        draw.ellipse((x - r, y - r, x + r, y + r), fill=color)


def save_bmp(img: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    rgb = img.convert("RGB")
    # 8-bit palette keeps files close to the original iPone asset footprint.
    pal = rgb.quantize(colors=240, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    pal.save(path, format="BMP")


def scene_variant(master: Image.Image, size: tuple[int, int], variant: str) -> Image.Image:
    crop_bias = {
        "lock": (0.52, 0.48),
        "alt": (0.62, 0.48),
        "third": (0.42, 0.52),
        "fourth": (0.70, 0.58),
        "fifth": (0.35, 0.43),
        "sixth": (0.55, 0.62),
        "wps": (0.55, 0.55),
        "fm": (0.52, 0.52),
    }.get(variant, (0.5, 0.5))
    img = crop_cover(master, size, *crop_bias)
    if variant == "wps":
        img = tint(img, PALETTE["forest"], 0.50)
        img = vignette(img, PALETTE["forest"], 160)
    elif variant == "fm":
        img = tint(img, PALETTE["paper"], 0.28)
    elif variant == "charge_morning":
        img = tint(img, (255, 246, 210), 0.22)
    elif variant == "charge_noon":
        img = ImageEnhance.Color(img).enhance(1.15)
    elif variant == "charge_dusk":
        img = tint(img, (210, 132, 82), 0.25)
    elif variant == "charge_night":
        img = tint(img, (26, 44, 54), 0.52)
        img = ImageEnhance.Brightness(img).enhance(0.68)
        d = ImageDraw.Draw(img)
        draw_spores(d, size, 55, 41)
    return img


def sbs_background(master: Image.Image, size: tuple[int, int], fullart: bool = False) -> Image.Image:
    w, h = size
    paper = paper_texture(size, PALETTE["paper"], 13 if not fullart else 14)
    left = Image.new("RGB", size, PALETTE["paper"])
    left = ImageChops.blend(left, paper, 0.45)
    scene = crop_cover(master, size, 0.80 if fullart else 0.72, 0.50)
    scene = ImageEnhance.Color(scene).enhance(1.18 if not fullart else 1.12)
    scene = ImageEnhance.Contrast(scene).enhance(1.06)
    scene = tint(scene, PALETTE["paper"], 0.28 if not fullart else 0.18)
    mask = Image.new("L", size, 0)
    md = ImageDraw.Draw(mask)
    md.rectangle((160, 0, w, h), fill=255)
    md.rectangle((150, 0, 170, h), fill=144 if not fullart else 176)
    mask = mask.filter(ImageFilter.GaussianBlur(14))
    img = Image.composite(scene, left, mask)
    d = ImageDraw.Draw(img)
    d.rectangle((159, 0, 160, h), fill=mix(PALETTE["forest"], PALETTE["paper"], 0.30 if not fullart else 0.20))
    d.rectangle((160, 0, 163, h), fill=mix(PALETTE["paper"], PALETTE["forest"], 0.14))
    if fullart:
        for x in range(184, w, 28):
            draw_leaf(d, x, h - 12, 7, mix(PALETTE["lichen"], PALETTE["moss"], 0.12), -0.42)
    return img


def aod_background(size: tuple[int, int]) -> Image.Image:
    img = paper_texture(size, PALETTE["white"], 22)
    d = ImageDraw.Draw(img)
    w, h = size
    d.rectangle((0, 0, w, h), outline=(228, 224, 212))
    for x in range(0, w, 38):
        draw_leaf(d, x + 7, h - 7, 6, (214, 226, 193), -0.5)
    return img


def panel(size: tuple[int, int], light: bool = True, seed: int = 0) -> Image.Image:
    base = PALETTE["vellum"] if light else PALETTE["forest"]
    img = paper_texture(size, base, seed)
    if light:
        img = ImageEnhance.Contrast(img).enhance(1.05)
    d = ImageDraw.Draw(img)
    w, h = size
    border = mix(PALETTE["moss"], base, 0.45)
    radius = min(7, max(1, min(w, h) // 6))
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=radius, outline=border, fill=None)
    if w > 30 and h > 20:
        d.line((2, 2, w - 3, 2), fill=mix(PALETTE["white"], base, 0.45))
        d.line((2, h - 3, w - 3, h - 3), fill=mix(PALETTE["bark"], base, 0.62))
    return img


def icon_canvas(size: tuple[int, int], bg: tuple[int, int, int] | None = None) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    img = Image.new("RGB", size, bg or PALETTE["forest"])
    return img, ImageDraw.Draw(img)


def draw_play_glyph(d: ImageDraw.ImageDraw, box: tuple[int, int, int, int], state: int, color: tuple[int, int, int]) -> None:
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    if state in (1, 7):
        d.polygon([(x0 + w * 0.32, y0 + h * 0.22), (x0 + w * 0.32, y0 + h * 0.78), (x0 + w * 0.76, y0 + h * 0.50)], fill=color)
    elif state in (2, 8):
        bw = max(1, w // 5)
        d.rectangle((x0 + w // 3 - bw, y0 + h // 4, x0 + w // 3, y1 - h // 4), fill=color)
        d.rectangle((x0 + 2 * w // 3, y0 + h // 4, x0 + 2 * w // 3 + bw, y1 - h // 4), fill=color)
    elif state == 3:
        d.rectangle((x0 + w // 4, y0 + h // 4, x1 - w // 4, y1 - h // 4), fill=color)
    else:
        r = max(2, min(w, h) // 5)
        d.ellipse((x0 + w // 2 - r, y0 + h // 2 - r, x0 + w // 2 + r, y0 + h // 2 + r), outline=color)


def strip_image(size: tuple[int, int], frames: int, draw_frame,
                bg: tuple[int, int, int] = PALETTE["forest"]) -> Image.Image:
    w, h = size
    fh = max(1, h // frames)
    img = Image.new("RGB", size, bg)
    d = ImageDraw.Draw(img)
    for i in range(frames):
        y = i * fh
        draw_frame(d, i, (0, y, w, y + fh), fh)
    return img


def battery(size: tuple[int, int]) -> Image.Image:
    w, h = size
    frames = 24
    fh = h // frames
    img = Image.new("RGB", size, PALETTE["paper"])
    d = ImageDraw.Draw(img)
    for i in range(frames):
        y = i * fh
        pct = i / (frames - 1)
        body = (1, y + 2, w - 5, y + fh - 3)
        d.rounded_rectangle(body, radius=2, outline=PALETTE["ink"], fill=PALETTE["paper2"])
        d.rectangle((w - 5, y + 4, w - 3, y + fh - 5), fill=PALETTE["ink"])
        fill_w = max(1, int((w - 8) * pct))
        color = PALETTE["mushroom"] if pct < 0.22 else (PALETTE["spore"] if pct < 0.45 else PALETTE["moss"])
        d.rounded_rectangle((3, y + 4, 3 + fill_w, y + fh - 5), radius=1, fill=color)
        if i < 2:
            d.line((6, y + fh - 4, w - 8, y + 3), fill=PALETTE["spore"])
    return img


def loading(size: tuple[int, int], aod: bool = False) -> Image.Image:
    w, h = size
    frames = 12
    bg = PALETTE["white"] if aod else PALETTE["forest"]
    fg = PALETTE["ink"] if aod else PALETTE["spore"]
    img = Image.new("RGB", size, bg)
    d = ImageDraw.Draw(img)
    fh = h // frames
    for i in range(frames):
        cy = i * fh + fh // 2
        cx = w // 2
        for j in range(8):
            ang = (j / 8.0) * math.tau + i * 0.35
            r = max(3, min(w, fh) // 3)
            x = int(cx + math.cos(ang) * r)
            y = int(cy + math.sin(ang) * r)
            alpha = j / 7.0
            color = mix(PALETTE["lichen"], fg, alpha)
            d.ellipse((x - 1, y - 1, x + 1, y + 1), fill=color)
    return img


def spinner(size: tuple[int, int]) -> Image.Image:
    w, h = size
    frames = 12
    fh = h // frames
    img = Image.new("RGB", size, PALETTE["forest"])
    d = ImageDraw.Draw(img)
    for i in range(frames):
        ox, oy = w // 2, i * fh + fh // 2
        d.ellipse((ox - fh // 3, oy - fh // 3, ox + fh // 3, oy + fh // 3), outline=PALETTE["lichen"])
        ang = i / frames * math.tau
        x = int(ox + math.cos(ang) * fh * 0.28)
        y = int(oy + math.sin(ang) * fh * 0.28)
        d.ellipse((x - 2, y - 2, x + 2, y + 2), fill=PALETTE["spore"])
    return img


def slider(size: tuple[int, int], name: str) -> Image.Image:
    w, h = size
    is_back = "Backdrop" in name or name in {"PB.bmp", "VolumeSliderBackdrop.bmp", "VolumeSliderBackdropPurple.bmp"}
    is_knob = "PlayerSlider" in name or "End" in name or "Fallback" in name or name == "LargeSliderTop.bmp"
    bg = PALETTE["forest"] if h > 18 and "Large" not in name and name != "PB.bmp" else PALETTE["paper"]
    img = Image.new("RGB", size, bg)
    d = ImageDraw.Draw(img)
    cy = h // 2
    if is_knob:
        d.ellipse((1, 1, w - 2, h - 2), fill=PALETTE["spore"], outline=PALETTE["bark"])
        if w > 7 and h > 7:
            d.ellipse((w // 3, h // 4, w // 3 + 2, h // 4 + 2), fill=PALETTE["paper"])
    elif is_back:
        d.rounded_rectangle((0, max(0, cy - 3), w - 1, min(h - 1, cy + 3)), radius=3, fill=mix(PALETTE["moss"], PALETTE["paper"], 0.65), outline=mix(PALETTE["bark"], PALETTE["paper"], 0.45))
    else:
        d.rounded_rectangle((0, max(0, cy - 3), w - 1, min(h - 1, cy + 3)), radius=3, fill=PALETTE["moss"])
        d.line((2, cy - 1, w - 3, cy - 1), fill=PALETTE["lichen"])
    return img


def frame_asset(size: tuple[int, int], name: str) -> Image.Image:
    img = Image.new("RGB", size, PALETTE["forest"])
    d = ImageDraw.Draw(img)
    w, h = size
    if w <= 8 and h <= 8:
        d.pieslice((0, 0, w * 2, h * 2), 180, 270, fill=PALETTE["moss"])
        d.arc((0, 0, w * 2 - 1, h * 2 - 1), 180, 270, fill=PALETTE["lichen"], width=1)
    else:
        d.rectangle((0, 0, w, h), fill=PALETTE["forest"])
        for i in range(max(w, h)):
            if i % 9 == 0:
                if w > h:
                    draw_leaf(d, i, h // 2, max(2, h // 3), PALETTE["moss"], 0.15)
                else:
                    draw_leaf(d, w // 2, i, max(2, w // 3), PALETTE["moss"], 1.1)
    return img


def status_strip(size: tuple[int, int], frames: int, bg: tuple[int, int, int], fg: tuple[int, int, int]) -> Image.Image:
    return strip_image(size, frames,
                       lambda d, i, box, fh: draw_play_glyph(d, box, i, fg),
                       bg)


def shuffle_repeat(size: tuple[int, int], frames: int, repeat: bool) -> Image.Image:
    w, h = size
    fh = h // frames
    img = Image.new("RGB", size, PALETTE["forest"])
    d = ImageDraw.Draw(img)
    for i in range(frames):
        y = i * fh
        color = PALETTE["lichen"] if i else PALETTE["muted"]
        if repeat:
            d.arc((3, y + 3, w - 4, y + fh - 4), 25, 325, fill=color, width=2)
            d.polygon([(w - 5, y + 3), (w - 2, y + 7), (w - 7, y + 7)], fill=color)
            if i == 2:
                d.text((w // 2 - 2, y + fh // 2 - 4), "1", fill=PALETTE["spore"])
        else:
            d.line((2, y + 5, w - 3, y + fh - 5), fill=color, width=2)
            d.line((2, y + fh - 5, w - 3, y + 5), fill=color, width=2)
            d.polygon([(w - 5, y + 3), (w - 2, y + 5), (w - 5, y + 7)], fill=color)
    return img


def volume_prompt(size: tuple[int, int]) -> Image.Image:
    w, h = size
    frames = 4
    fh = h // frames
    img = Image.new("RGB", size, PALETTE["vellum"])
    d = ImageDraw.Draw(img)
    for i in range(frames):
        y = i * fh
        d.polygon([(3, y + fh // 2 - 4), (8, y + fh // 2 - 4), (14, y + 2), (14, y + fh - 2), (8, y + fh // 2 + 4), (3, y + fh // 2 + 4)], fill=PALETTE["moss"])
        for arc in range(i):
            d.arc((12 + arc * 3, y + 4 - arc, 20 + arc * 5, y + fh - 4 + arc), -40, 40, fill=PALETTE["spore"], width=1)
    return img


def ratings(size: tuple[int, int]) -> Image.Image:
    w, h = size
    frames = 10
    fh = h // frames
    img = Image.new("RGB", size, PALETTE["forest"])
    d = ImageDraw.Draw(img)
    for i in range(frames):
        y = i * fh
        count = min(5, (i + 1) // 2)
        for j in range(5):
            x = 4 + j * max(8, (w - 8) // 5)
            color = PALETTE["spore"] if j < count else PALETTE["muted"]
            draw_leaf(d, x, y + fh // 2, max(3, fh // 4), color, -0.2)
    return img


def tiny_icon(size: tuple[int, int], name: str) -> Image.Image:
    img = Image.new("RGB", size, PALETTE["forest"] if "AOD" not in name else PALETTE["white"])
    d = ImageDraw.Draw(img)
    w, h = size
    fg = PALETTE["lichen"] if "AOD" not in name else PALETTE["ink"]
    if "Hold" in name:
        d.rounded_rectangle((1, h // 3, w - 2, h - 2), radius=2, outline=fg)
        d.arc((w // 4, 0, w - w // 4, h // 2 + 1), 180, 360, fill=fg)
    elif "Sleep" in name:
        d.pieslice((1, 1, w - 2, h - 2), 90, 270, fill=fg)
        d.ellipse((w // 3, 1, w, h - 2), fill=img.getpixel((0, 0)))
    elif "Lossless" in name:
        d.rounded_rectangle((1, 1, w - 2, h - 2), radius=3, outline=fg)
        d.line((4, h - 4, w // 2, 4, w - 4, h - 4), fill=fg, width=1)
    elif "Explicit" in name:
        d.rectangle((1, 1, w - 2, h - 2), outline=fg)
        d.line((4, h // 2, w - 4, h // 2), fill=fg)
        d.line((4, h // 2 + 3, w - 5, h // 2 + 3), fill=fg)
    else:
        draw_leaf(d, w // 2, h // 2, max(3, min(w, h) // 3), fg, -0.35)
    return img


def album_fallback(master: Image.Image, size: tuple[int, int], radio: bool = False) -> Image.Image:
    img = crop_cover(master, size, 0.72 if not radio else 0.58, 0.58)
    img = tint(img, PALETTE["paper"], 0.12)
    d = ImageDraw.Draw(img)
    w, h = size
    d.rounded_rectangle((1, 1, w - 2, h - 2), radius=8, outline=PALETTE["forest"], width=2)
    if radio:
        d.rounded_rectangle((w // 4, h // 3, 3 * w // 4, 2 * h // 3), radius=8, fill=PALETTE["vellum"], outline=PALETTE["bark"], width=2)
        d.line((w // 2, h // 3, 3 * w // 4, h // 7), fill=PALETTE["bark"], width=2)
        d.ellipse((w // 3, h // 2 - 4, w // 3 + 8, h // 2 + 4), fill=PALETTE["spore"])
    return img


def decorative_asset(master: Image.Image, name: str, size: tuple[int, int]) -> Image.Image:
    w, h = size
    if (w, h) == (320, 240):
        mapping = {
            "iPone_bd.bmp": "sbs",
            "iPone_bd_fullart.bmp": "sbs_full",
            "SbsBackdrop.bmp": "sbs",
            "iPone_bg.bmp": "fm",
            "Wallpaper.bmp": "lock",
            "WallpaperAlt.bmp": "alt",
            "WallpaperThird.bmp": "third",
            "WallpaperFourth.bmp": "fourth",
            "WallpaperFifth.bmp": "fifth",
            "WallpaperSixth.bmp": "sixth",
            "ChargeWallpaper.bmp": "charge_morning",
            "ChargeWallpaperAlt.bmp": "charge_noon",
            "ChargeWallpaperThird.bmp": "charge_dusk",
            "ChargeWallpaperFourth.bmp": "charge_night",
        }
        variant = mapping.get(name, "lock")
        if variant == "sbs":
            return sbs_background(master, size, False)
        if variant == "sbs_full":
            return sbs_background(master, size, True)
        return scene_variant(master, size, variant)
    if name == "AODBackdrop.bmp":
        return aod_background(size)
    if name == "SbsIpodLabel.bmp":
        img = Image.new("RGB", size, PALETTE["paper"])
        d = ImageDraw.Draw(img)
        d.rectangle((0, 0, size[0] - 1, size[1] - 1), fill=PALETTE["paper"])
        d.text((2, 1), "iPod", fill=PALETTE["ink"])
        return img
    if name.startswith("ChargeWallpaperPreview"):
        index = {"1": "charge_morning", "2": "charge_noon", "3": "charge_dusk", "4": "charge_night"}[name[-5]]
        return scene_variant(master, size, index)
    if name == "LockscreenStyle.bmp":
        return scene_variant(master, size, "lock")
    if name == "AlwaysOnDisplayStyle.bmp":
        return aod_background(size)
    if name in {"PlayerFallback.bmp", "NotifMusic.bmp"}:
        return album_fallback(master, size)
    if name == "Radio Icon.bmp":
        return album_fallback(master, size, True)
    if "Notification" in name or "Backdrop" in name or "AlbumStage" in name or name == "VolumeBackdrop.bmp" or name == "PlayerStatusButton.bmp":
        return panel(size, light=name != "AlbumStage.bmp", seed=len(name))
    if name.startswith("Wps") or name.startswith("Frame"):
        return frame_asset(size, name)
    return panel(size, light=True, seed=len(name) + w + h)


def generate_one(master: Image.Image, path: Path) -> None:
    name = path.name
    if name == "SbsIpodLabel.bmp":
        size = (58, 14)
    else:
        with Image.open(path) as old:
            size = old.size
    template_sources = {
        "Battery.bmp": "Battery.bmp",
        "LoadingStatus.bmp": "LoadingStatus.bmp",
        "LoadingStatusAOD.bmp": "LoadingStatusAOD.bmp",
        "PlayStatus.bmp": "PlayStatus.bmp",
        "PlayStatusPurple.bmp": "PlayStatusPurple.bmp",
        "PlayStatusPurpleLarge.bmp": "PlayStatusPurpleLarge.bmp",
        "Playing Status.bmp": "Playing Status.bmp",
        "PlayIcon.bmp": "PlayIcon.bmp",
        "NotifMusic.bmp": "NotifMusic.bmp",
        "NotifPlayIcon.bmp": "NotifPlayIcon.bmp",
        "NotifPlayIconLock.bmp": "NotifPlayIconLock.bmp",
        "HoldStatus.bmp": "HoldStatus.bmp",
        "SleepStatus.bmp": "SleepStatus.bmp",
        "SleepStatusAOD.bmp": "SleepStatusAOD.bmp",
        "LosslessIcon.bmp": "LosslessIcon.bmp",
        "LosslessIconLock.bmp": "LosslessIconLock.bmp",
        "ExplicitIcon.bmp": "ExplicitIcon.bmp",
        "Stereo Icon.bmp": "Stereo Icon.bmp",
        "Memory Access.bmp": "Memory Access.bmp",
        "ShuffleStatus.bmp": "ShuffleStatus.bmp",
        "ShuffleStatusSmall.bmp": "ShuffleStatusSmall.bmp",
        "RepeatStatus.bmp": "RepeatStatus.bmp",
        "RepeatStatusSmall.bmp": "RepeatStatusSmall.bmp",
        "RepeatStatusSmallLarge.bmp": "RepeatStatusSmallLarge.bmp",
        "Shuffle Icon.bmp": "Shuffle Icon.bmp",
        "Repeat Icon.bmp": "Repeat Icon.bmp",
        "PlaybackStatusIcons.bmp": "PlaybackStatusIcons.bmp",
        "PlaylistPositionIndicators.bmp": "PlaylistPositionIndicators.bmp",
        "Ratings.bmp": "Ratings.bmp",
        "Radio Icon.bmp": "Radio Icon.bmp",
        "PlayerFallback.bmp": "PlayerFallback.bmp",
        "PlayerSlider.bmp": "PlayerSlider.bmp",
        "PlayerSliderThin.bmp": "PlayerSliderThin.bmp",
        "PlayerSliderThinPurple.bmp": "PlayerSliderThinPurple.bmp",
        "PlayerSliderThinPurple12.bmp": "PlayerSliderThinPurple12.bmp",
        "PlayerStatusButton.bmp": "PlayerStatusButton.bmp",
        "Slider.bmp": "Slider.bmp",
        "SliderThin.bmp": "SliderThin.bmp",
        "SliderThinPurple.bmp": "SliderThinPurple.bmp",
        "SliderThinPurple12.bmp": "SliderThinPurple12.bmp",
        "PB.bmp": "PB.bmp",
        "LargeSlider.bmp": "LargeSlider.bmp",
        "LargeSliderBackdrop.bmp": "LargeSliderBackdrop.bmp",
        "LargeSliderFallback.bmp": "LargeSliderFallback.bmp",
        "LargeSliderTop.bmp": "LargeSliderTop.bmp",
        "SliderBackdrop.bmp": "SliderBackdrop.bmp",
        "SliderBackdrop4Digits.bmp": "SliderBackdrop4Digits.bmp",
        "SliderBackdrop5Digits.bmp": "SliderBackdrop5Digits.bmp",
        "SliderBackdrop6Digits.bmp": "SliderBackdrop6Digits.bmp",
        "SliderBackdropThin.bmp": "SliderBackdropThin.bmp",
        "SliderBackdropThin4Digits.bmp": "SliderBackdropThin4Digits.bmp",
        "SliderBackdropThin5Digits.bmp": "SliderBackdropThin5Digits.bmp",
        "SliderBackdropThin6Digits.bmp": "SliderBackdropThin6Digits.bmp",
        "SliderBackdropThinPurple.bmp": "SliderBackdropThinPurple.bmp",
        "SliderBackdropThinPurple4Digits.bmp": "SliderBackdropThinPurple4Digits.bmp",
        "SliderBackdropThinPurple5Digits.bmp": "SliderBackdropThinPurple5Digits.bmp",
        "SliderBackdropThinPurple6Digits.bmp": "SliderBackdropThinPurple6Digits.bmp",
        "SliderBackdropThinPurple12.bmp": "SliderBackdropThinPurple12.bmp",
        "SliderBackdropThinPurple12_4Digits.bmp": "SliderBackdropThinPurple12_4Digits.bmp",
        "SliderBackdropThinPurple12_5Digits.bmp": "SliderBackdropThinPurple12_5Digits.bmp",
        "SliderBackdropThinPurple12_6Digits.bmp": "SliderBackdropThinPurple12_6Digits.bmp",
        "VolumeBackdrop.bmp": "VolumeBackdrop.bmp",
        "VolumePromptIcons.bmp": "VolumePromptIcons.bmp",
        "VolumeSlider.bmp": "VolumeSlider.bmp",
        "VolumeSliderBackdrop.bmp": "VolumeSliderBackdrop.bmp",
        "VolumeSliderEnd.bmp": "VolumeSliderEnd.bmp",
        "VolumeSliderBackdropPurple.bmp": "VolumeSliderBackdropPurple.bmp",
        "VolumeSliderEndPurple.bmp": "VolumeSliderEndPurple.bmp",
        "VolumeSliderPurple.bmp": "VolumeSliderPurple.bmp",
        "SbsBackdrop.bmp": "iPone_bd.bmp",
    }
    if name in template_sources and (IPONE_DIR / template_sources[name]).exists():
        if name in {"LoadingStatusAOD.bmp"}:
            img = recolor_template(template_sources[name], PALETTE["ink"], PALETTE["white"], PALETTE["paper2"])
        elif name in {"Battery.bmp"}:
            img = recolor_template(template_sources[name], PALETTE["ink"], PALETTE["paper"], PALETTE["moss"])
        elif name in {"LoadingStatus.bmp"}:
            img = recolor_template(template_sources[name], PALETTE["ink"], PALETTE["paper"], PALETTE["moss"])
        elif name in {"VolumeBackdrop.bmp", "VolumePromptIcons.bmp", "PlayerStatusButton.bmp"}:
            img = recolor_template(template_sources[name], PALETTE["forest"], PALETTE["paper2"], PALETTE["spore"])
        elif name in {"PlayerFallback.bmp", "Radio Icon.bmp"}:
            img = recolor_template(template_sources[name], PALETTE["forest"], PALETTE["paper"], PALETTE["bark"])
        elif name in {"PB.bmp"}:
            img = recolor_template(template_sources[name], PALETTE["forest"], PALETTE["paper"], PALETTE["lichen"])
        elif name in {"SbsBackdrop.bmp"}:
            img = recolor_template(template_sources[name], PALETTE["paper"], PALETTE["forest"], PALETTE["lichen"])
        else:
            img = recolor_template(template_sources[name], PALETTE["forest"], PALETTE["paper"], PALETTE["lichen"])
        save_bmp(img.resize(size, Image.Resampling.NEAREST), path)
        return
    if name == "Battery.bmp":
        img = battery(size)
    elif name.startswith("LoadingStatus"):
        img = loading(size, "AOD" in name)
    elif name.startswith("MiniRecordSpin"):
        img = spinner(size)
    elif name in {"PlayStatus.bmp", "PlayStatusPurple.bmp"}:
        img = status_strip(size, 9, PALETTE["forest"], PALETTE["lichen"])
    elif name == "PlayStatusPurpleLarge.bmp":
        img = status_strip(size, 9, PALETTE["forest"], PALETTE["spore"])
    elif name == "Playing Status.bmp":
        img = status_strip(size, 4, PALETTE["forest"], PALETTE["lichen"])
    elif name == "PlayIcon.bmp":
        img = status_strip(size, 2, PALETTE["forest"], PALETTE["lichen"])
    elif name.startswith("Shuffle"):
        img = shuffle_repeat(size, 2, False)
    elif name.startswith("Repeat"):
        img = shuffle_repeat(size, 5, True)
    elif name.startswith("NotifPlayIcon"):
        img = status_strip(size, 2, PALETTE["forest"], PALETTE["spore"])
    elif "Slider" in name or name in {"PB.bmp", "LargeSlider.bmp", "LargeSliderBackdrop.bmp", "LargeSliderFallback.bmp", "LargeSliderTop.bmp"}:
        img = slider(size, name)
    elif name == "VolumePromptIcons.bmp":
        img = volume_prompt(size)
    elif name == "Ratings.bmp":
        img = ratings(size)
    elif name in {"Hold Icon.bmp", "HoldStatus.bmp", "SleepStatus.bmp", "SleepStatusAOD.bmp", "LosslessIcon.bmp", "LosslessIconLock.bmp", "ExplicitIcon.bmp", "Stereo Icon.bmp", "Memory Access.bmp", "Volume Left.bmp", "Volume Right.bmp", "Shuffle Icon.bmp", "Repeat Icon.bmp"}:
        frames = 6 if name == "Memory Access.bmp" else 1
        if frames > 1:
            img = loading(size, False)
        else:
            img = tiny_icon(size, name)
    elif name in {"PlaybackStatusIcons.bmp", "PlaylistPositionIndicators.bmp"}:
        img = status_strip(size, max(1, size[1] // 16), PALETTE["forest"], PALETTE["lichen"])
    else:
        img = decorative_asset(master, name, size)
    save_bmp(img, path)


def generate_iconset() -> None:
    img = Image.new("RGB", (4, 16), PALETTE["paper"])
    d = ImageDraw.Draw(img)
    d.rectangle((0, 0, 3, 7), fill=PALETTE["moss"])
    d.rectangle((0, 8, 3, 15), fill=PALETTE["mushroom"])
    d.point((1, 2), fill=PALETTE["lichen"])
    d.point((2, 10), fill=PALETTE["spore"])
    save_bmp(img, ICON_PATH)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, help="Generated master PNG source")
    args = parser.parse_args()

    source = Path(args.source).expanduser().resolve()
    if not source.exists():
        raise SystemExit(f"source image not found: {source}")
    if not FOREST_DIR.exists():
        raise SystemExit(f"Forest theme directory not found: {FOREST_DIR}")

    SOURCE_DIR.mkdir(parents=True, exist_ok=True)
    saved_source = SOURCE_DIR / "forest-master-source.png"
    if source != saved_source.resolve():
        shutil.copy2(source, saved_source)
    master = Image.open(source).convert("RGB")

    # Forest uses one extra stock-style top label asset that is not present in
    # the cloned iPone source tree.
    (FOREST_DIR / "SbsIpodLabel.bmp").touch(exist_ok=True)

    for path in sorted(FOREST_DIR.glob("*.bmp")):
        generate_one(master, path)

    backdrop = sbs_background(master, (320, 240), False)
    save_bmp(backdrop, BACKDROP_PATH)
    generate_iconset()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
