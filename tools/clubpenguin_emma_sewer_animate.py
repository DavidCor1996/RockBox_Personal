#!/usr/bin/env python3
"""Build the layered Emma Blackery Sewer Sessions concert animation.

All scenery, performers and audience pixels come from the approved generated
raster plates.  This tool separates those pixels into softly masked layers and
applies one-pixel, eased stage motion plus emissive-light cycles.  It never
draws replacement characters or instruments.
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

from clubpenguin_night_city_animate import composite_players


SIZE = (320, 220)
FRAMES = 18


def load_resized(path: Path) -> Image.Image:
    with Image.open(path) as source:
        return source.convert("RGBA").resize(SIZE, Image.Resampling.LANCZOS)


def polygon_mask(points: list[tuple[int, int]], blur: float = 0.75) -> Image.Image:
    mask = Image.new("L", SIZE, 0)
    ImageDraw.Draw(mask).polygon(points, fill=255)
    return mask.filter(ImageFilter.GaussianBlur(blur))


def layer(source: Image.Image, mask: Image.Image) -> Image.Image:
    result = source.copy()
    result.putalpha(mask)
    return result


def shifted(source: Image.Image, dx: int, dy: int) -> Image.Image:
    result = Image.new("RGBA", SIZE, (0, 0, 0, 0))
    result.alpha_composite(source, (dx, dy))
    return result


def emissive_mask(image: Image.Image, box: tuple[int, int, int, int]) -> Image.Image:
    hsv = image.convert("RGB").convert("HSV")
    pixels = hsv.load()
    mask = Image.new("L", SIZE, 0)
    target = mask.load()
    left, top, right, bottom = box
    for y in range(max(0, top), min(SIZE[1], bottom)):
        for x in range(max(0, left), min(SIZE[0], right)):
            _hue, saturation, value = pixels[x, y]
            if saturation >= 82 and value >= 86:
                target[x, y] = min(220, round((saturation + value) * 0.52))
    return mask.filter(ImageFilter.GaussianBlur(1.8))


def wave(frame: int, phase: float) -> float:
    return 0.5 + 0.5 * math.sin((frame / FRAMES + phase) * math.tau)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()

    night_city = args.assets / "rooms" / "night_city"
    full = load_resized(night_city / "emma-sewer-stage-generated.png")
    clean = load_resized(night_city / "emma-sewer-clean-plate.png")
    output = night_city / "emma_sewer_frames"
    output.mkdir(parents=True, exist_ok=True)

    performer_masks = [
        polygon_mask([(70, 69), (119, 69), (130, 143), (70, 143)]),
        polygon_mask([(126, 77), (174, 77), (176, 160), (123, 160)]),
        polygon_mask([(150, 49), (203, 49), (204, 111), (148, 111)]),
        polygon_mask([(209, 69), (263, 69), (266, 145), (207, 145)]),
    ]
    performers = [layer(full, mask) for mask in performer_masks]

    crowd_masks = [
        polygon_mask([(0, 108), (56, 108), (104, 151), (105, 220),
                      (0, 220)], 0.6),
        polygon_mask([(55, 145), (139, 172), (143, 220), (85, 220)], 0.6),
        polygon_mask([(320, 105), (272, 105), (235, 151), (225, 220),
                      (320, 220)], 0.6),
        polygon_mask([(267, 145), (181, 172), (177, 220), (235, 220)], 0.6),
    ]
    crowds = [layer(full, mask) for mask in crowd_masks]

    stage_glow = emissive_mask(clean, (0, 0, 320, 115))
    floor_glow = emissive_mask(clean, (0, 105, 320, 220))
    font_path = night_city / "BlenderPro-Bold.ttf"
    avatar_dir = args.assets / "avatar"

    singer_x = (0, 0, 1, 1, 1, 0, 0, -1, -1,
                -1, 0, 0, 1, 1, 0, 0, -1, -1)
    singer_y = (0, -1, -1, 0, 1, 1, 0, 0, -1,
                -1, 0, 1, 1, 0, -1, -1, 0, 1)
    band_y = (0, 0, -1, -1, 0, 1, 1, 0, 0,
              -1, -1, 0, 1, 1, 0, 0, -1, 0)

    for frame in range(FRAMES):
        result = clean.copy()

        bright = ImageEnhance.Brightness(clean).enhance(
            1.03 + wave(frame, 0.04) * 0.13
        )
        result.paste(bright, (0, 0), stage_glow.point(
            lambda value: round(value * (0.20 + wave(frame, 0.04) * 0.28))
        ))
        floor = ImageEnhance.Color(clean).enhance(
            1.02 + wave(frame, 0.56) * 0.12
        )
        result.paste(floor, (0, 0), floor_glow.point(
            lambda value: round(value * (0.12 + wave(frame, 0.56) * 0.20))
        ))

        crowd_offsets = [
            (-1 if frame in (4, 5, 13, 14) else 0,
             -1 if frame in (3, 4, 5, 12, 13, 14) else 0),
            (1 if frame in (0, 1, 9, 10) else 0,
             -1 if frame in (0, 1, 2, 9, 10, 11) else 0),
            (1 if frame in (5, 6, 14, 15) else 0,
             -1 if frame in (4, 5, 6, 13, 14, 15) else 0),
            (-1 if frame in (1, 2, 10, 11) else 0,
             -1 if frame in (1, 2, 3, 10, 11, 12) else 0),
        ]
        for crowd, (dx, dy) in zip(crowds, crowd_offsets):
            result.alpha_composite(shifted(crowd, dx, dy))

        # Guitarist, Emma, drummer and keyboardist move on staggered beats.
        result.alpha_composite(shifted(
            performers[0], -1 if frame in (5, 6, 7) else 0,
            band_y[frame]
        ))
        result.alpha_composite(shifted(
            performers[2], 0, band_y[(frame + 6) % FRAMES]
        ))
        result.alpha_composite(shifted(
            performers[3], 1 if frame in (10, 11, 12) else 0,
            band_y[(frame + 11) % FRAMES]
        ))
        result.alpha_composite(shifted(
            performers[1], singer_x[frame], singer_y[frame]
        ))

        composite_players(
            result,
            avatar_dir,
            font_path,
            [
                ("SewerRat", 7, (3, 6, 1, 2), 35, 215,
                 2, 1, 0.82),
                ("BlackeryFan", 11, (6, 8, 5, 1), 285, 214,
                 1, 7, 0.82),
            ],
            frame,
        )
        result.convert("RGB").save(output / f"{frame}.bmp", "BMP")

    # The room fallback is the first fully composited frame.
    (args.assets / "rooms" / "emma_sewer.bmp").write_bytes(
        (output / "0.bmp").read_bytes()
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
