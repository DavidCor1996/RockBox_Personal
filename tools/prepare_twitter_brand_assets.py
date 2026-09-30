#!/usr/bin/env python3
"""Render device icons from the sourced 2010 Twitter vector logo."""

from io import BytesIO
from pathlib import Path
import subprocess

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/ipodjs/sources/twitter/twitter-2010-logo.svg"
DEVICE = ROOT / "assets/ipodjs/rockbox"
SIDEBAR = ROOT / "rockpod/assets/icons/twitter-official.png"


def main():
    if not SOURCE.is_file():
        raise SystemExit(f"Missing sourced Twitter logo: {SOURCE}")
    raster = subprocess.run(
        ["rsvg-convert", "-w", "900", str(SOURCE)],
        capture_output=True, check=True,
    ).stdout
    with Image.open(BytesIO(raster)) as image:
        logo = image.convert("RGBA")
    bird_raster = subprocess.run(
        ["rsvg-convert", "-w", "900", str(SOURCE.with_name(
            "twitter-bird-official.svg"))], capture_output=True, check=True,
    ).stdout
    bird = Image.open(BytesIO(bird_raster)).convert("RGBA")
    bird = bird.crop(bird.getchannel("A").getbbox())

    for size, path in (
        (46, DEVICE / "applications/twitter.46x46x24.bmp"),
        (64, SIDEBAR),
    ):
        path.parent.mkdir(parents=True, exist_ok=True)
        canvas = Image.new("RGBA", (size, size), (29, 161, 242, 255))
        shape = bird.copy()
        shape.thumbnail((round(size * .72), round(size * .72)),
                        Image.Resampling.LANCZOS)
        x = (size - shape.width) // 2
        y = (size - shape.height) // 2
        canvas.paste((255, 255, 255), (x, y, x + shape.width, y + shape.height),
                     shape.getchannel("A"))
        # Same 8px rounded tile and magenta color key as the other apps.
        mask = Image.new("L", (size, size), 0)
        ImageDraw.Draw(mask).rounded_rectangle(
            (0, 0, size - 1, size - 1), radius=round(size * 8 / 46), fill=255)
        canvas.putalpha(mask)
        if path.suffix == ".bmp":
            flat = Image.new("RGB", canvas.size, (255, 0, 255))
            flat.paste(canvas, mask=mask)
            flat.save(path, "BMP")
        else:
            canvas.save(path, "PNG")

    title = Image.new("RGB", (90, 16), (61, 164, 217))
    small = logo.resize((90, 16), Image.Resampling.LANCZOS)
    title.paste((255, 255, 255), (0, 0, 90, 16), small.getchannel("A"))
    title_path = DEVICE / "twitter/twitter-logo.90x16.bmp"
    title_path.parent.mkdir(parents=True, exist_ok=True)
    title.save(title_path, "BMP")


if __name__ == "__main__":
    main()
