#!/usr/bin/env python3
"""Build subtle neon animation frames from the finished Night City rooms.

The room art remains the generated raster source.  This tool only varies
existing emissive pixels; it does not draw replacement scenery or props.
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont


SIZE = (320, 220)
FRAMES = 12
PLAYER_W = 52
PLAYER_H = 56
PLAYER_STEP = (0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 1, 1)


def wave(frame: int, offset: float = 0.0) -> float:
    return 0.5 + 0.5 * math.sin((frame / FRAMES + offset) * math.tau)


def scaled_mask(mask: Image.Image, strength: float) -> Image.Image:
    return mask.point(lambda value: round(value * strength))


def emissive_mask(image: Image.Image, box: tuple[int, int, int, int],
                  hue: str = "all") -> Image.Image:
    hsv = image.convert("HSV")
    source = hsv.load()
    mask = Image.new("L", SIZE, 0)
    target = mask.load()
    left, top, right, bottom = box
    for y in range(max(0, top), min(SIZE[1], bottom)):
        for x in range(max(0, left), min(SIZE[0], right)):
            h, s, v = source[x, y]
            if s < 92 or v < 76:
                continue
            if hue == "red" and not (h < 20 or h > 235):
                continue
            target[x, y] = min(230, round((s + v) * 0.58))
    return mask.filter(ImageFilter.GaussianBlur(2.1))


def glow_layer(base: Image.Image, amount: float) -> Image.Image:
    saturated = ImageEnhance.Color(base).enhance(1.10 + amount * 0.18)
    return ImageEnhance.Brightness(saturated).enhance(1.05 + amount * 0.22)


def paper_doll_frame(avatar_dir: Path, color: int,
                     slots: tuple[int, int, int, int],
                     frame: int) -> Image.Image:
    """Composite one real paper-doll frame using the runtime layer order."""

    def crop(path: Path) -> Image.Image:
        with Image.open(path) as source:
            image = source.convert("RGBA").crop(
                (frame * PLAYER_W, 0, (frame + 1) * PLAYER_W, PLAYER_H)
            )
        pixels = image.load()
        for y in range(PLAYER_H):
            for x in range(PLAYER_W):
                red, green, blue, _alpha = pixels[x, y]
                pixels[x, y] = (
                    red, green, blue,
                    0 if red > 245 and green < 12 and blue > 245 else 255,
                )
        return image

    result = crop(avatar_dir / f"color_{color}.bmp")
    # feet, body, face, head -- identical to cp_load_player_asset().
    for page, slot in zip((2, 1, 4, 0), slots):
        result.alpha_composite(crop(avatar_dir / f"page{page}_{slot}.bmp"))
    return result


def composite_players(image: Image.Image, avatar_dir: Path, font_path: Path,
                      players: list[tuple], room_frame: int) -> None:
    font = ImageFont.truetype(str(font_path), 8)
    draw = ImageDraw.Draw(image)
    for (name, color, slots, x, y, direction, phase, scale) in sorted(
            players, key=lambda player: player[4]):
        step = PLAYER_STEP[(room_frame + phase) % FRAMES]
        sprite = paper_doll_frame(
            avatar_dir, color, slots, direction * 3 + step
        )
        height = round(PLAYER_H * scale)
        width = round(PLAYER_W * scale)
        sprite = sprite.resize((width, height), Image.Resampling.LANCZOS)
        image.alpha_composite(sprite, (x - width // 2, y - height))

        box = draw.textbbox((0, 0), name, font=font, stroke_width=1)
        name_x = x - (box[2] - box[0]) // 2
        name_y = y - height - 10
        draw.text(
            (name_x, name_y), name, font=font, fill=(245, 252, 255, 255),
            stroke_width=1, stroke_fill=(4, 10, 19, 255),
        )


def animate_room(base_path: Path, output_dir: Path,
                 zones: list[tuple[tuple[int, int, int, int], float]],
                 red_phase: float, avatar_dir: Path, font_path: Path,
                 players: list[tuple]) -> None:
    with Image.open(base_path) as source:
        base = source.convert("RGB").resize(SIZE, Image.Resampling.LANCZOS)

    masks = [(emissive_mask(base, box), phase) for box, phase in zones]
    red = emissive_mask(base, (0, 0, SIZE[0], 150), "red")
    output_dir.mkdir(parents=True, exist_ok=True)

    for frame in range(FRAMES):
        result = base.copy()
        for mask, phase in masks:
            amount = wave(frame, phase)
            result.paste(
                glow_layer(base, amount),
                (0, 0),
                scaled_mask(mask, 0.18 + amount * 0.42),
            )

        # Existing red obstruction lamps blink on a different, slow phase.
        red_amount = wave(frame, red_phase)
        result.paste(
            glow_layer(base, red_amount),
            (0, 0),
            scaled_mask(red, 0.08 + red_amount * 0.32),
        )
        result = result.convert("RGBA")
        composite_players(result, avatar_dir, font_path, players, frame)
        result.convert("RGB").save(output_dir / f"{frame}.bmp", "BMP")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()

    room_dir = args.assets / "rooms"
    night_city = room_dir / "night_city"
    avatar_dir = args.assets / "avatar"
    font_path = night_city / "BlenderPro-Bold.ttf"
    animate_room(
        room_dir / "town.bmp",
        night_city / "town_frames",
        [
            ((113, 26, 211, 93), 0.00),  # Afterlife facade
            ((20, 22, 74, 125), 0.34),  # Coffee facade
            ((213, 30, 300, 121), 0.67),  # Gift Shop facade
            ((0, 0, 320, 70), 0.18),  # skyline signage
        ],
        0.52,
        avatar_dir,
        font_path,
        [
            ("ByteBrr", 6, (1, 4, 2, 2), 74, 170, 2, 0, 0.92),
            ("NeonFlipper", 8, (5, 7, 4, 1), 247, 170, 1, 5, 0.94),
        ],
    )
    animate_room(
        room_dir / "night_club.bmp",
        night_city / "afterlife_frames",
        [
            ((112, 22, 214, 82), 0.00),  # Afterlife emblem and exit
            ((0, 55, 145, 139), 0.38),  # bar and bottle wall
            ((250, 66, 320, 145), 0.70),  # lounge stair
            ((0, 132, 75, 220), 0.18),  # DJ console
        ],
        0.58,
        avatar_dir,
        font_path,
        [
            ("RogueWaddle", 4, (3, 6, 1, 2), 76, 169, 2, 1, 0.94),
            ("ChromePuffle", 10, (7, 4, 7, 4), 226, 174, 1, 6, 0.98),
            ("Afterglow", 12, (1, 9, 3, 1), 260, 139, 1, 3, 0.82),
        ],
    )
    animate_room(
        room_dir / "lounge.bmp",
        night_city / "lounge_frames",
        [
            ((112, 12, 212, 70), 0.00),  # lounge emblem and exit
            ((0, 38, 122, 128), 0.36),  # service bar and pool table
            ((200, 40, 320, 135), 0.68),  # arcade terminals and skyline
            ((0, 0, 320, 58), 0.20),  # skyline and ceiling strips
        ],
        0.54,
        avatar_dir,
        font_path,
        [
            ("PoolByte", 5, (4, 8, 2, 2), 89, 170, 2, 2, 0.94),
            ("NetWaddler", 11, (6, 5, 5, 3), 232, 165, 1, 7, 0.92),
        ],
    )
    animate_room(
        room_dir / "coffee_shop.bmp",
        night_city / "coffee_frames",
        [
            ((0, 18, 92, 117), 0.08),  # Tom's Diner wall neon
            ((206, 20, 320, 126), 0.43),  # counter screens and stair sign
            ((96, 16, 212, 96), 0.72),  # skyline window and exit lamp
        ],
        0.62,
        avatar_dir,
        font_path,
        [
            ("LatteByte", 7, (2, 5, 1, 2), 124, 195, 2, 1, 0.92),
            ("BoothBrr", 13, (5, 6, 4, 3), 188, 195, 1, 7, 0.94),
        ],
    )
    animate_room(
        room_dir / "plaza.bmp",
        night_city / "plaza_frames",
        [
            ((8, 19, 111, 113), 0.18),  # Pet Shop facade
            ((102, 8, 220, 112), 0.00),  # Oliver Tree Arena marquee
            ((214, 17, 318, 114), 0.52),  # Buck-A-Slice facade
            ((0, 0, 320, 52), 0.74),  # Night City skyline signs
        ],
        0.46,
        avatar_dir,
        font_path,
        [
            ("SliceWaddle", 3, (2, 7, 4, 1), 75, 177, 2, 2, 0.92),
            ("ArenaPal", 9, (6, 4, 2, 2), 207, 179, 1, 8, 0.95),
            ("NeonPaws", 11, (1, 9, 6, 4), 270, 158, 1, 5, 0.82),
        ],
    )
    animate_room(
        room_dir / "dock.bmp",
        night_city / "dock_frames",
        [
            ((0, 35, 104, 218), 0.12),  # reservoir reflections
            ((95, 29, 233, 107), 0.48),  # cottage lights and cyan door
            ((215, 74, 320, 218), 0.76),  # water and dive equipment
        ],
        0.65,
        avatar_dir,
        font_path,
        [
            ("DiveChoom", 6, (4, 5, 3, 2), 143, 184, 2, 3, 0.90),
            ("LagunaGlow", 12, (7, 8, 5, 1), 252, 194, 1, 9, 0.90),
        ],
    )
    animate_room(
        room_dir / "pizza_parlor.bmp",
        night_city / "pizza_frames",
        [
            ((0, 5, 111, 119), 0.16),  # drinks and order screens
            ((99, 4, 207, 86), 0.47),  # exit lighting
            ((198, 3, 320, 123), 0.72),  # sign, oven, and vending screens
        ],
        0.57,
        avatar_dir,
        font_path,
        [
            ("CheeseByte", 8, (4, 6, 1, 2), 113, 150, 2, 0, 0.92),
            ("OneEdWaddle", 5, (6, 4, 3, 3), 188, 150, 1, 6, 0.94),
        ],
    )
    animate_room(
        room_dir / "lighthouse.bmp",
        night_city / "cottage_frames",
        [
            ((0, 16, 90, 118), 0.09),  # rainy window and warm lounge
            ((104, 21, 200, 91), 0.44),  # cottage exit light
            ((197, 25, 320, 145), 0.73),  # editing terminal and stairs
        ],
        0.61,
        avatar_dir,
        font_path,
        [
            ("LakeDreamer", 4, (2, 8, 5, 1), 125, 178, 2, 4, 0.91),
            ("Braindance", 10, (7, 5, 2, 4), 205, 178, 1, 10, 0.93),
        ],
    )
    animate_room(
        room_dir / "beacon.bmp",
        night_city / "cottage_roof_frames",
        [
            ((0, 13, 100, 125), 0.12),  # reservoir and deck lights
            ((99, 17, 227, 115), 0.46),  # cottage door and string lights
            ((220, 20, 320, 137), 0.77),  # reservoir and antenna lights
        ],
        0.64,
        avatar_dir,
        font_path,
        [
            ("RoofRunner", 2, (3, 7, 4, 2), 124, 181, 2, 5, 0.91),
            ("DamWatcher", 9, (6, 5, 2, 1), 215, 181, 1, 11, 0.92),
        ],
    )
    animate_room(
        room_dir / "beach.bmp",
        night_city / "beach_frames",
        [
            ((0, 0, 104, 136), 0.10),  # Migrator and reservoir lights
            ((91, 8, 236, 101), 0.45),  # cottage lighting
            ((210, 30, 320, 219), 0.75),  # path lamps and water edge
        ],
        0.59,
        avatar_dir,
        font_path,
        [
            ("ShoreChoom", 7, (3, 5, 1, 2), 94, 184, 2, 2, 0.91),
            ("ReservoirBrr", 13, (6, 8, 4, 3), 231, 187, 1, 8, 0.92),
        ],
    )
    animate_room(
        room_dir / "stadium.bmp",
        night_city / "stadium_frames",
        [
            ((82, 0, 238, 62), 0.00),  # scoreboard and centre display
            ((0, 30, 159, 158), 0.34),  # Montreal home LEDs and stands
            ((161, 30, 320, 158), 0.67),  # Toronto away LEDs and stands
            ((0, 145, 320, 220), 0.18),  # rink-board and ice reflections
        ],
        0.51,
        avatar_dir,
        font_path,
        [
            ("HabsChoom", 3, (2, 7, 1, 2), 91, 181, 2, 1, 0.90),
            ("LeafBrr", 11, (5, 4, 4, 1), 231, 178, 1, 7, 0.90),
        ],
    )
    animate_room(
        room_dir / "snow_forts.bmp",
        night_city / "snow_forts_frames",
        [
            ((55, 18, 128, 121), 0.08),  # Velvet Ice facade
            ((126, 0, 194, 123), 0.42),  # clock and arena tunnel
            ((192, 18, 276, 124), 0.73),  # Chrome Clinic facade
            ((0, 0, 320, 75), 0.22),  # layered city skyline
        ],
        0.60,
        avatar_dir,
        font_path,
        [
            ("ClockWork", 5, (4, 7, 2, 2), 62, 178, 2, 1, 0.90),
            ("VelvetQueue", 12, (1, 9, 5, 1), 193, 183, 1, 7, 0.91),
            ("NeonMedic", 8, (6, 5, 4, 3), 266, 168, 1, 4, 0.84),
        ],
    )
    animate_room(
        room_dir / "velvet_ice.bmp",
        night_city / "velvet_ice_frames",
        [
            ((105, 0, 215, 102), 0.04),  # venue sign and exit
            ((186, 50, 320, 165), 0.39),  # stage and bar lighting
            ((0, 0, 112, 140), 0.69),  # windows and coat check
            ((0, 130, 320, 220), 0.20),  # polished floor reflections
        ],
        0.56,
        avatar_dir,
        font_path,
        [
            ("CabaretBrr", 4, (3, 8, 1, 2), 82, 178, 2, 2, 0.90),
            ("SnowVelvet", 10, (7, 5, 5, 4), 184, 190, 1, 8, 0.92),
            ("NightOwl", 13, (2, 6, 4, 1), 268, 172, 1, 5, 0.84),
        ],
    )
    animate_room(
        room_dir / "chrome_clinic.bmp",
        night_city / "chrome_clinic_frames",
        [
            ((105, 0, 214, 106), 0.02),  # clinic sign and exit
            ((0, 20, 116, 154), 0.35),  # reception diagnostics
            ((202, 18, 320, 166), 0.68),  # care bay and workbench
            ((0, 136, 320, 220), 0.18),  # clean floor reflections
        ],
        0.63,
        avatar_dir,
        font_path,
        [
            ("PatchByte", 6, (4, 5, 3, 2), 82, 181, 2, 3, 0.90),
            ("PuffleDoc", 11, (6, 8, 5, 1), 217, 180, 1, 9, 0.91),
        ],
    )
    animate_room(
        room_dir / "dojo_courtyard.bmp",
        night_city / "dojo_courtyard_frames",
        [
            ((102, 0, 219, 118), 0.03),  # tower logo and main doors
            ((50, 70, 133, 143), 0.34),  # left holographic maple
            ((188, 70, 270, 143), 0.68),  # right holographic maple
            ((0, 0, 320, 73), 0.19),  # skyline and warning lights
        ],
        0.61,
        avatar_dir,
        font_path,
        [
            ("SakaGuard", 4, (3, 7, 1, 2), 74, 181, 2, 2, 0.90),
            ("AVRunner", 10, (6, 5, 5, 4), 192, 187, 1, 8, 0.92),
            ("RedMaple", 13, (2, 9, 4, 1), 254, 188, 1, 5, 0.86),
        ],
    )
    animate_room(
        room_dir / "dojo.bmp",
        night_city / "dojo_frames",
        [
            ((105, 0, 216, 111), 0.04),  # lobby logo and exit
            ((0, 39, 119, 157), 0.37),  # reception and visitor access
            ((201, 35, 320, 160), 0.71),  # security bank
            ((0, 130, 320, 220), 0.20),  # polished floor reflections
        ],
        0.57,
        avatar_dir,
        font_path,
        [
            ("VisitorV", 6, (4, 5, 3, 2), 79, 181, 2, 3, 0.90),
            ("CounterIntel", 11, (7, 8, 5, 1), 216, 184, 1, 9, 0.91),
            ("BlackSuit", 5, (1, 6, 2, 3), 271, 165, 1, 6, 0.84),
        ],
    )
    animate_room(
        room_dir / "forest.bmp",
        night_city / "forest_frames",
        [
            ((0, 0, 320, 70), 0.09),  # Corpo Plaza skyline and AV lights
            ((40, 33, 143, 113), 0.37),  # conservatory and left approach
            ((174, 32, 265, 118), 0.68),  # memorial and NCART entrance
            ((0, 70, 320, 220), 0.20),  # holo trees and wet reflections
        ],
        0.60,
        avatar_dir,
        font_path,
        [
            ("ParkRunner", 6, (4, 7, 3, 2), 67, 184, 2, 2, 0.90),
            ("HoloLeaf", 12, (7, 5, 5, 1), 183, 190, 1, 8, 0.92),
            ("CorpoBreak", 5, (2, 9, 4, 3), 263, 181, 1, 5, 0.86),
        ],
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
