"""Image helpers for low-depth greyscale Rockbox targets."""

from __future__ import annotations

from PIL import Image, ImageOps

GREYSCALE_2BPP_RESOLUTIONS = {"160x128"}
GREYSCALE_2BPP_LEVELS = (0, 85, 170, 255)
_BAYER_4X4 = (
    (0, 8, 2, 10),
    (12, 4, 14, 6),
    (3, 11, 1, 9),
    (15, 7, 13, 5),
)


def should_render_2bpp_greyscale(resolution: str) -> bool:
    return str(resolution or "").strip() in GREYSCALE_2BPP_RESOLUTIONS


def render_2bpp_greyscale(image: Image.Image) -> Image.Image:
    """Map an image to the four iPod 3G LCD grey levels with ordered dither."""

    grey = ImageOps.grayscale(image)
    if grey.getextrema()[0] == grey.getextrema()[1]:
        value = min(GREYSCALE_2BPP_LEVELS, key=lambda level: abs(level - grey.getextrema()[0]))
        return Image.new("RGB", grey.size, (value, value, value))

    grey = ImageOps.autocontrast(grey, cutoff=1)
    source = grey.load()
    width, height = grey.size
    output = Image.new("L", (width, height))
    target = output.load()
    step = GREYSCALE_2BPP_LEVELS[1] - GREYSCALE_2BPP_LEVELS[0]

    for y in range(height):
        row = _BAYER_4X4[y % 4]
        for x in range(width):
            dither = ((row[x % 4] + 0.5) / 16.0 - 0.5) * step
            index = int(round((source[x, y] + dither) / step))
            index = max(0, min(3, index))
            target[x, y] = GREYSCALE_2BPP_LEVELS[index]

    return output.convert("RGB")
