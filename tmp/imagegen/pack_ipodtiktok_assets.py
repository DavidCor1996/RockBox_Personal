#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageFilter, ImageOps


ROOT = Path("/home/david/Documents/RockBox_Personal-master")
RAW = ROOT / "output/imagegen/ipodtiktok_raw"
OUT = ROOT / "apps/plugins/bitmaps/native"
PREVIEW = ROOT / "output/imagegen/ipodtiktok_preview"

ASSETS = {
    "ipodtiktok-header.png": ("ipodtiktok_header.320x40x24.bmp", (320, 40), (0.50, 0.45)),
    "ipodtiktok-scrim.png": ("ipodtiktok_scrim.320x96x24.bmp", (320, 96), (0.50, 0.88)),
    "ipodtiktok-heart.png": ("ipodtiktok_heart.32x32x24.bmp", (32, 32), (0.50, 0.50)),
}


def open_rgb(path: Path) -> Image.Image:
    return Image.open(path).convert("RGB")


def trim_uniform_border(image: Image.Image, tolerance: int = 12) -> Image.Image:
    bg = image.getpixel((0, 0))
    width, height = image.size
    left = width
    top = height
    right = -1
    bottom = -1
    px = image.load()

    for y in range(height):
        for x in range(width):
            color = px[x, y]
            if any(abs(color[i] - bg[i]) > tolerance for i in range(3)):
                left = min(left, x)
                top = min(top, y)
                right = max(right, x)
                bottom = max(bottom, y)

    if right < left or bottom < top:
        return image

    return image.crop((left, top, right + 1, bottom + 1))


def fit(path: Path, size: tuple[int, int], centering: tuple[float, float]) -> Image.Image:
    image = open_rgb(path)
    if "heart" in path.name:
        image = trim_uniform_border(image)
    return ImageOps.fit(image, size, method=Image.Resampling.LANCZOS, centering=centering)


def darken_bottom(image: Image.Image, strength: float = 0.75) -> Image.Image:
    width, height = image.size
    overlay = Image.new("RGB", image.size, (0, 0, 0))
    mask = Image.new("L", image.size, 0)
    px = mask.load()

    for y in range(height):
        t = y / max(1, height - 1)
        alpha = int(255 * max(0.0, min(1.0, (t ** 1.8) * strength)))
        for x in range(width):
            px[x, y] = alpha

    return Image.composite(overlay, image, mask)


def glow_soften(image: Image.Image, radius: float = 1.2) -> Image.Image:
    return image.filter(ImageFilter.GaussianBlur(radius=radius))


def save_bmp(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="BMP")


def build_assets() -> None:
    for src_name, (dst_name, size, centering) in ASSETS.items():
        image = fit(RAW / src_name, size, centering)

        if "scrim" in dst_name:
            image = darken_bottom(image, strength=0.92)
        elif "heart" in dst_name:
            image = glow_soften(image, radius=0.6)

        save_bmp(image, OUT / dst_name)


def build_preview() -> None:
    PREVIEW.mkdir(parents=True, exist_ok=True)
    header = open_rgb(OUT / "ipodtiktok_header.320x40x24.bmp")
    scrim = open_rgb(OUT / "ipodtiktok_scrim.320x96x24.bmp")
    heart = open_rgb(OUT / "ipodtiktok_heart.32x32x24.bmp")

    canvas = Image.new("RGB", (320, 240), (12, 12, 16))
    canvas.paste(header, (0, 0))
    canvas.paste(scrim, (0, 240 - scrim.height))
    canvas.paste(heart, (272, 128))
    canvas.save(PREVIEW / "ipodtiktok_overlay_mock.png")


def main() -> int:
    build_assets()
    build_preview()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
