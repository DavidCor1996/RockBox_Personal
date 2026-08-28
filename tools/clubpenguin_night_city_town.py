#!/usr/bin/env python3
"""Composite official Night City references into the preserved Town room."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import (
    Image,
    ImageChops,
    ImageDraw,
    ImageEnhance,
    ImageFilter,
    ImageFont,
    ImageOps,
)


SIZE = (320, 220)


def fitted(image: Image.Image, size: tuple[int, int], centering=(0.5, 0.5)):
    return ImageOps.fit(
        image.convert("RGB"), size, Image.Resampling.LANCZOS,
        centering=centering,
    )


def referenced_cartoon(image: Image.Image) -> Image.Image:
    image = ImageEnhance.Color(image).enhance(1.35)
    image = ImageEnhance.Contrast(image).enhance(1.15)
    image = ImageOps.posterize(image, 5)
    return image.filter(ImageFilter.ModeFilter(3))


def sky_mask(base: Image.Image) -> Image.Image:
    mask = Image.new("L", SIZE, 0)
    source = base.load()
    target = mask.load()
    for y in range(138):
        for x in range(SIZE[0]):
            red, green, blue = source[x, y]
            if red < 28 and blue > 105 and blue > green + 42:
                target[x, y] = 255
    return mask.filter(ImageFilter.MaxFilter(3))


def ground_mask() -> Image.Image:
    mask = Image.new("L", SIZE, 0)
    pixels = mask.load()
    for y in range(105, SIZE[1]):
        strength = min(112, (y - 105) * 2)
        for x in range(SIZE[0]):
            pixels[x, y] = strength
    return mask.filter(ImageFilter.GaussianBlur(5))


def neon_mask(base: Image.Image) -> Image.Image:
    hsv = base.convert("HSV")
    mask = Image.new("L", SIZE, 0)
    source = hsv.load()
    target = mask.load()
    for y in range(SIZE[1]):
        for x in range(SIZE[0]):
            _hue, saturation, value = source[x, y]
            if saturation > 115 and value > 110:
                target[x, y] = min(210, saturation)
    return mask.filter(ImageFilter.GaussianBlur(5))


def texture_building(
    scene: Image.Image,
    base: Image.Image,
    reference: Image.Image,
    polygon: list[tuple[int, int]],
    opacity: int,
) -> None:
    """Re-material an original CP building without changing its silhouette."""
    mask = Image.new("L", SIZE, 0)
    ImageDraw.Draw(mask).polygon(polygon, fill=opacity)
    mask = mask.filter(ImageFilter.GaussianBlur(1.1))

    # Collapse the screenshot into broad, posterized material and light
    # fields, then multiply them by the original building luminance. This
    # preserves Club Penguin's painted forms instead of pasting photo panels.
    texture = fitted(reference, SIZE, (0.52, 0.42))
    texture = texture.filter(ImageFilter.GaussianBlur(2.2))
    texture = ImageOps.posterize(texture, 4)
    luminance = ImageEnhance.Contrast(base.convert("L")).enhance(1.18)
    form = Image.merge("RGB", (luminance, luminance, luminance))
    material = ImageChops.multiply(texture, form)
    material = ImageEnhance.Brightness(material).enhance(1.48)
    material = Image.blend(base, material, 0.68)
    scene.paste(material, (0, 0), mask)


def restore_cp_linework(scene: Image.Image, base: Image.Image) -> None:
    """Keep the original rounded ink and snowy highlights over new material."""
    edge = base.convert("L").filter(ImageFilter.FIND_EDGES)
    edge = ImageEnhance.Contrast(edge).enhance(2.0)
    edge = edge.point(lambda value: 185 if value > 24 else 0)
    scene.paste(base, (0, 0), edge)

    hsv = base.convert("HSV")
    snow = Image.new("L", SIZE, 0)
    src = hsv.load()
    dst = snow.load()
    for y in range(SIZE[1]):
        for x in range(SIZE[0]):
            _hue, saturation, value = src[x, y]
            if saturation < 78 and value > 112:
                dst[x, y] = 175
    scene.paste(base, (0, 0), snow.filter(ImageFilter.GaussianBlur(0.7)))


def add_afterlife_sign(
    scene: Image.Image,
    reference: Image.Image,
    font_path: Path,
) -> None:
    """Replace the Dance Club marquee with an official-font Afterlife sign."""
    panel = referenced_cartoon(fitted(reference, (88, 25), (0.78, 0.48)))
    panel = ImageEnhance.Brightness(panel).enhance(0.42)
    panel_mask = Image.new("L", panel.size, 220)
    scene.paste(panel, (124, 54), panel_mask)

    draw = ImageDraw.Draw(scene)
    font = ImageFont.truetype(str(font_path), 16)
    small = ImageFont.truetype(str(font_path), 7)
    word = "AFTERLIFE"
    box = draw.textbbox((0, 0), word, font=font, stroke_width=1)
    x = 168 - (box[2] - box[0]) // 2
    draw.text((x, 57), word, font=font, fill=(247, 235, 14),
              stroke_width=1, stroke_fill=(21, 19, 37))
    draw.text((129, 56), "THE", font=small, fill=(63, 233, 239),
              stroke_width=1, stroke_fill=(21, 19, 37))


def build(base_path: Path, reference_dir: Path, output: Path) -> None:
    with Image.open(base_path) as image:
        base = image.convert("RGB").resize(SIZE, Image.Resampling.LANCZOS)
    with Image.open(reference_dir / "above-it-all.jpg") as image:
        above = image.convert("RGB")
    with Image.open(reference_dir / "immerse-yourself.jpg") as image:
        immerse = image.convert("RGB")
    with Image.open(reference_dir / "no-regrets.jpg") as image:
        regrets = image.convert("RGB")
    with Image.open(reference_dir / "official-logo.png") as image:
        logo = image.convert("RGBA")
    with Image.open(reference_dir / "afterlife-official.png") as image:
        afterlife = image.convert("RGB")

    skyline = referenced_cartoon(fitted(above, (320, 155), (0.52, 0.36)))
    towers = referenced_cartoon(fitted(immerse, (320, 155), (0.5, 0.34)))
    skyline = Image.blend(skyline, towers, 0.28)
    skyline = ImageEnhance.Brightness(skyline).enhance(0.90)
    skyline = ImageEnhance.Contrast(skyline).enhance(1.25)
    skyline_frame = Image.new("RGB", SIZE, (7, 12, 29))
    skyline_frame.paste(skyline, (0, 0))

    blue_grade = ImageChops.multiply(base, Image.new("RGB", SIZE, (86, 91, 154)))
    magenta_grade = ImageChops.screen(
        blue_grade, Image.new("RGB", SIZE, (34, 4, 38))
    )
    scene = Image.blend(base, magenta_grade, 0.64)
    scene.paste(skyline_frame, (0, 0), sky_mask(base))

    # Re-material all three original Town buildings as distinct Night City
    # blocks while retaining their exact Club Penguin footprints.
    texture_building(
        scene, base, above,
        [(8, 55), (55, 43), (119, 45), (132, 122), (110, 148),
         (35, 151), (10, 131)],
        205,
    )
    texture_building(
        scene, base, immerse,
        [(112, 58), (138, 46), (143, 7), (199, 5), (205, 45),
         (226, 58), (219, 145), (119, 148)],
        218,
    )
    texture_building(
        scene, base, regrets,
        [(216, 59), (239, 48), (298, 51), (315, 68), (303, 139),
         (220, 149)],
        205,
    )

    street_reference = referenced_cartoon(
        fitted(regrets.transpose(Image.Transpose.FLIP_TOP_BOTTOM), SIZE,
               (0.5, 0.62))
    )
    reflected = Image.blend(scene, ImageChops.screen(scene, street_reference),
                            0.52)
    scene = Image.composite(reflected, scene, ground_mask())

    neon_source = referenced_cartoon(fitted(above, SIZE, (0.62, 0.48)))
    neon_lit = ImageChops.screen(scene, neon_source)
    scene = Image.composite(neon_lit, scene, neon_mask(base))

    restore_cp_linework(scene, base)
    add_afterlife_sign(
        scene, afterlife, reference_dir / "BlenderPro-Bold.ttf"
    )

    # Official franchise lockup, reduced into a small facade billboard.
    logo.thumbnail((58, 15), Image.Resampling.LANCZOS)
    billboard = Image.new("RGBA", logo.size, (9, 11, 24, 210))
    billboard.alpha_composite(logo)
    scene.paste(billboard.convert("RGB"), (61, 48), billboard.getchannel("A"))

    # Keep the readable, rounded Club Penguin linework dominant after the
    # photographic reference passes have supplied skyline and lighting.
    scene = Image.blend(scene, ImageOps.posterize(scene, 6), 0.32)
    scene = ImageEnhance.Sharpness(scene).enhance(1.35)
    output.parent.mkdir(parents=True, exist_ok=True)
    scene.save(output, "BMP")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--references", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.base, args.references, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
