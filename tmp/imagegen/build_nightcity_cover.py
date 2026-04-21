#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageOps


ROOT = Path("/home/david/Documents/RockBox_Personal-master")
RAW = ROOT / "output/imagegen/nightcity_raw"
DOCS = ROOT / "documents/gameboy"
DEVICE_GAMEBOY = Path("/run/media/david/DAVID_S IPO/gameboy")

SOURCE_SIZE = (900, 1200)
DEVICE_JPG_SIZE = (160, 160)
DEVICE_BMP_SIZE = (320, 288)


def load_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ]
    for candidate in candidates:
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size=size)
    return ImageFont.load_default()


def cover_art() -> Image.Image:
    art = Image.open(RAW / "nightcity-cover-art.png").convert("RGB")
    return ImageOps.fit(art, (740, 760), method=Image.Resampling.LANCZOS, centering=(0.5, 0.35))


def build_source_cover() -> Image.Image:
    img = Image.new("RGB", SOURCE_SIZE, (10, 13, 24))
    draw = ImageDraw.Draw(img)

    # Background gradient blocks
    for y in range(SOURCE_SIZE[1]):
        t = y / SOURCE_SIZE[1]
        color = (
            int(10 + 18 * t),
            int(13 + 10 * t),
            int(24 + 30 * t),
        )
        draw.line((0, y, SOURCE_SIZE[0], y), fill=color)

    draw.rectangle((0, 0, SOURCE_SIZE[0], 132), fill=(218, 221, 230))
    draw.rectangle((0, 116, SOURCE_SIZE[0], 132), fill=(62, 68, 92))
    draw.rectangle((0, SOURCE_SIZE[1] - 90, SOURCE_SIZE[0], SOURCE_SIZE[1]), fill=(20, 23, 35))

    mono = load_font(36, bold=False)
    title_font = load_font(120, bold=True)
    sub_font = load_font(34, bold=True)
    body_font = load_font(24, bold=False)
    brand_font = load_font(52, bold=True)

    draw.text((48, 32), "Nintendo", font=mono, fill=(80, 86, 104))
    draw.text((235, 28), "GAME BOY", font=brand_font, fill=(47, 51, 69))
    draw.text((640, 36), "TM", font=mono, fill=(80, 86, 104))

    art = cover_art()
    art_frame = Image.new("RGB", (768, 788), (15, 19, 33))
    frame_draw = ImageDraw.Draw(art_frame)
    frame_draw.rounded_rectangle((0, 0, 767, 787), radius=38, outline=(58, 67, 95), width=4, fill=(15, 19, 33))
    art_frame.paste(art, (14, 14))
    img.paste(art_frame, (66, 186))

    # Neon rails and skyline accents
    draw.rounded_rectangle((82, 890, 818, 916), radius=12, fill=(31, 208, 225))
    draw.rounded_rectangle((82, 928, 640, 946), radius=8, fill=(255, 70, 167))
    draw.rounded_rectangle((656, 928, 818, 946), radius=8, fill=(247, 199, 89))

    draw.text((84, 954), "NIGHT", font=title_font, fill=(237, 241, 250))
    draw.text((84, 1040), "CITY", font=title_font, fill=(237, 241, 250))
    draw.text((90, 1140), "CYBERPUNK STORY RPG", font=sub_font, fill=(88, 219, 232))
    draw.text((90, 1180), "for Game Boy / Game Boy Color", font=body_font, fill=(190, 196, 214))

    return img


def main() -> int:
    DOCS.mkdir(parents=True, exist_ok=True)

    source = build_source_cover()
    source_png = DOCS / "nightcity-cover.png"
    source_bmp = DOCS / "nightcity-cover.bmp"
    jpg = DOCS / "nightcity-cover.jpg"

    source.save(source_png, format="PNG")
    ImageOps.fit(source, DEVICE_BMP_SIZE, method=Image.Resampling.LANCZOS, centering=(0.5, 0.45)).save(source_bmp, format="BMP")
    ImageOps.fit(source, DEVICE_JPG_SIZE, method=Image.Resampling.LANCZOS, centering=(0.5, 0.38)).save(jpg, format="JPEG", quality=88)

    print(source_png)
    print(source_bmp)
    print(jpg)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
