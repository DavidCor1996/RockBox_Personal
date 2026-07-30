#!/usr/bin/env python3
"""Build RockPod icons from the dimensional broadcast-weather atlas."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ATLAS = (
    ROOT / "rockpod" / "assets" / "weather" / "source" /
    "broadcast-weather-atlas-v2.png"
)
OUT = ROOT / "rockpod" / "assets" / "weather" / "icons"
SIZES = (40, 64)
COLOR_KEY = (248, 0, 248)

# The source is an evenly-spaced 5 x 2 production atlas. Keep this mapping
# explicit so filenames remain the stable API used by weather.c and Live TV.
ATLAS_CELLS = {
    "clear_day": (0, 0),
    "clear_night": (1, 0),
    "partly_cloudy": (2, 0),
    "cloudy": (3, 0),
    "rain": (4, 0),
    "drizzle": (0, 1),
    "snow": (1, 1),
    "fog": (2, 1),
    "thunderstorm": (3, 1),
    "unknown": (4, 1),
}


def _atlas_cell(atlas: Image.Image, kind: str) -> Image.Image:
    column, row = ATLAS_CELLS[kind]
    left = round(column * atlas.width / 5)
    right = round((column + 1) * atlas.width / 5)
    top = round(row * atlas.height / 2)
    bottom = round((row + 1) * atlas.height / 2)
    cell = atlas.crop((left, top, right, bottom))
    bounds = cell.getchannel("A").getbbox()
    if bounds is None:
        raise ValueError(f"empty weather atlas cell: {kind}")
    return cell.crop(bounds)


def _render(atlas: Image.Image, kind: str, size: int) -> Image.Image:
    """Downsample one CGI symbol onto Rockbox's magenta colour key."""
    source = _atlas_cell(atlas, kind)
    extent = max(1, round(size * 0.94))
    scale = min(extent / source.width, extent / source.height)
    rendered = source.resize(
        (
            max(1, round(source.width * scale)),
            max(1, round(source.height * scale)),
        ),
        Image.Resampling.LANCZOS,
    )

    rgba = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    rgba.alpha_composite(
        rendered,
        ((size - rendered.width) // 2, (size - rendered.height) // 2),
    )
    keyed = Image.new("RGB", (size, size), COLOR_KEY)
    keyed.paste(rgba.convert("RGB"), (0, 0), rgba.getchannel("A"))
    return keyed


def main() -> int:
    if not ATLAS.is_file():
        raise SystemExit(f"missing weather icon atlas: {ATLAS}")

    OUT.mkdir(parents=True, exist_ok=True)
    with Image.open(ATLAS) as opened:
        atlas = opened.convert("RGBA")
        for size in SIZES:
            for kind in ATLAS_CELLS:
                image = _render(atlas, kind, size)
                image.save(OUT / f"{kind}.{size}x{size}x24.bmp", "BMP")

    print(f"generated {len(SIZES) * len(ATLAS_CELLS)} weather icons in {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
