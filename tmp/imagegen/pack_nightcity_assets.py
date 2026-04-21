#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageOps


ROOT = Path("/home/david/Documents/RockBox_Personal-master")
RAW = ROOT / "output/imagegen/nightcity_raw"
OUT = ROOT / "apps/plugins/bitmaps/native"
PREVIEW = ROOT / "output/imagegen/nightcity_preview"
PANORAMA_SIZE = (240, 66)
GLITCH_FRAME_SIZE = (320, 32)

PORTRAITS = [
    ("portrait-vesper.png", (0.50, 0.28)),
    ("portrait-juno.png", (0.50, 0.28)),
    ("portrait-mira.png", (0.50, 0.28)),
    ("portrait-sable.png", (0.50, 0.28)),
    ("portrait-rook.png", (0.50, 0.28)),
    ("portrait-kade.png", (0.50, 0.28)),
    ("portrait-hostile.png", (0.50, 0.30)),
    ("portrait-nyra.png", (0.50, 0.27)),
    ("profile-razor.png", (0.50, 0.28)),
    ("profile-velvet.png", (0.50, 0.28)),
    ("profile-drift.png", (0.50, 0.28)),
]

CARDS = [
    ("card-city.png", "nightcity_citycard.62x70x24.bmp", (0.50, 0.45)),
    ("card-ghost.png", "nightcity_ghostcard.62x70x24.bmp", (0.50, 0.45)),
    ("card-clinic.png", "nightcity_cliniccard.62x70x24.bmp", (0.50, 0.45)),
    ("card-board.png", "nightcity_boardcard.62x70x24.bmp", (0.50, 0.45)),
    ("card-convoy.png", "nightcity_convoycard.62x70x24.bmp", (0.50, 0.45)),
    ("card-relay.png", "nightcity_relaycard.62x70x24.bmp", (0.50, 0.45)),
    ("card-afterglow.png", "nightcity_afterglowcard.62x70x24.bmp", (0.50, 0.45)),
]

PANORAMAS = [
    ("pano-city.png", (0.50, 0.46)),
    ("pano-ghost.png", (0.50, 0.48)),
    ("pano-clinic.png", (0.50, 0.48)),
    ("pano-board.png", (0.50, 0.48)),
    ("pano-convoy.png", (0.50, 0.48)),
    ("pano-relay.png", (0.50, 0.48)),
    ("pano-afterglow.png", (0.50, 0.46)),
]


def open_rgb(path: Path) -> Image.Image:
    return Image.open(path).convert("RGB")


def trim_light_border(image: Image.Image, threshold: int = 245) -> Image.Image:
    px = image.load()
    width, height = image.size
    left = width
    top = height
    right = -1
    bottom = -1

    for y in range(height):
        for x in range(width):
            r, g, b = px[x, y]
            if min(r, g, b) < threshold:
                left = min(left, x)
                top = min(top, y)
                right = max(right, x)
                bottom = max(bottom, y)

    if right < left or bottom < top:
        return image

    return image.crop((left, top, right + 1, bottom + 1))


def fit(path: Path, size: tuple[int, int], centering: tuple[float, float]) -> Image.Image:
    image = trim_light_border(open_rgb(path))
    return ImageOps.fit(image, size, method=Image.Resampling.LANCZOS, centering=centering)


def save_bmp(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="BMP")


def build_title() -> None:
    img = fit(RAW / "title.png", (320, 96), (0.58, 0.45))
    save_bmp(img, OUT / "nightcity_title.320x96x24.bmp")


def build_cards() -> None:
    for src, dst, centering in CARDS:
        img = fit(RAW / src, (62, 70), centering)
        save_bmp(img, OUT / dst)


def build_panorama() -> None:
    atlas = Image.new("RGB", (PANORAMA_SIZE[0] * len(PANORAMAS), PANORAMA_SIZE[1]))
    for index, (src, centering) in enumerate(PANORAMAS):
        tile = fit(RAW / src, PANORAMA_SIZE, centering)
        atlas.paste(tile, (index * PANORAMA_SIZE[0], 0))
    save_bmp(atlas, OUT / "nightcity_panorama.1680x66x24.bmp")


def build_glitch() -> None:
    atlas = Image.new("RGB", (GLITCH_FRAME_SIZE[0] * 4, GLITCH_FRAME_SIZE[1]), (8, 11, 18))
    for frame in range(4):
        tile = Image.new("RGB", GLITCH_FRAME_SIZE, (8, 11, 18))
        draw = ImageDraw.Draw(tile)
        for band in range(8):
            y = band * 4
            color = (35, 207, 225) if (band + frame) % 2 == 0 else (255, 70, 167)
            draw.rectangle((0, y, GLITCH_FRAME_SIZE[0], y + 1), fill=color)
            draw.rectangle((20 + ((frame * 41 + band * 29) % 180), y + 2,
                            120 + ((frame * 53 + band * 17) % 180), y + 3), fill=color)
        for block in range(10):
            x = (frame * 73 + block * 31) % (GLITCH_FRAME_SIZE[0] - 48)
            y = (block * 3 + frame * 5) % (GLITCH_FRAME_SIZE[1] - 6)
            w = 16 + ((block * 11 + frame * 7) % 42)
            color = (19, 28, 43) if block % 2 else (21, 37, 58)
            draw.rectangle((x, y, x + w, y + 4), fill=color)
        atlas.paste(tile, (frame * GLITCH_FRAME_SIZE[0], 0))
    save_bmp(atlas, OUT / "nightcity_glitch.1280x32x24.bmp")


def build_portraits() -> None:
    atlas = Image.new("RGB", (72 * len(PORTRAITS), 96))
    for index, (src, centering) in enumerate(PORTRAITS):
        tile = fit(RAW / src, (72, 96), centering)
        atlas.paste(tile, (index * 72, 0))
    save_bmp(atlas, OUT / "nightcity_portraits.792x96x24.bmp")


def build_threats() -> None:
    sentinel = fit(RAW / "sentinel.png", (106, 64), (0.50, 0.48))
    aegis = fit(RAW / "aegis.png", (106, 64), (0.50, 0.48))
    save_bmp(sentinel, OUT / "nightcity_sentinel.106x64x24.bmp")
    save_bmp(aegis, OUT / "nightcity_aegis.106x64x24.bmp")


def build_endings() -> None:
    atlas = Image.new("RGB", (68 * 4, 56))
    separate = [
        RAW / "ending-rebel.png",
        RAW / "ending-corp.png",
        RAW / "ending-ghost.png",
        RAW / "ending-cost.png",
    ]
    if all(path.exists() for path in separate):
        sources = separate
        panels = [fit(path, (68, 56), (0.5, 0.5)) for path in sources]
    else:
        sheet = open_rgb(RAW / "ending-sheet.png")
        width, height = sheet.size
        panel_w = width // 4
        panels = []
        for index in range(4):
            panel = sheet.crop((index * panel_w, 0, (index + 1) * panel_w, height))
            panel = ImageOps.fit(panel, (68, 56), method=Image.Resampling.LANCZOS, centering=(0.5, 0.5))
            panels.append(panel)
    for index, panel in enumerate(panels):
        atlas.paste(panel, (index * 68, 0))
    save_bmp(atlas, OUT / "nightcity_endings.272x56x24.bmp")


def build_preview() -> None:
    PREVIEW.mkdir(parents=True, exist_ok=True)
    title = open_rgb(OUT / "nightcity_title.320x96x24.bmp")
    panorama = open_rgb(OUT / "nightcity_panorama.1680x66x24.bmp")
    portraits = open_rgb(OUT / "nightcity_portraits.792x96x24.bmp")
    cards = [open_rgb(OUT / dst) for _, dst, _ in CARDS]
    threats = [
        open_rgb(OUT / "nightcity_sentinel.106x64x24.bmp"),
        open_rgb(OUT / "nightcity_aegis.106x64x24.bmp"),
    ]
    endings = open_rgb(OUT / "nightcity_endings.272x56x24.bmp")

    canvas = Image.new("RGB", (1200, 520), (10, 14, 26))
    canvas.paste(title, (24, 24))
    canvas.paste(panorama, (24, 132))
    canvas.paste(portraits, (24, 222))
    x = 24
    for card in cards:
        canvas.paste(card, (x, 342))
        x += 72
    x = 24
    for threat in threats:
        canvas.paste(threat, (x, 438))
        x += 118
    canvas.paste(endings, (300, 446))
    canvas.save(PREVIEW / "nightcity_montage.png")


def main() -> int:
    build_title()
    build_panorama()
    build_glitch()
    build_cards()
    build_portraits()
    build_threats()
    build_endings()
    build_preview()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
