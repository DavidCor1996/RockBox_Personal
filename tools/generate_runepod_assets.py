#!/usr/bin/env python3
"""Generate deterministic RunePod bitmap assets."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "rockpod" / "assets" / "runepod"

KEY = (255, 0, 255)
INK = (24, 25, 24)
SKIN = (210, 166, 116)
HAIR = (64, 45, 34)
BLUE = (68, 90, 167)
GUIDE = (86, 116, 74)
SHOP = (126, 88, 48)
ENEMY = (132, 55, 64)
SMITH = (80, 88, 96)
INN = (138, 82, 54)
HEALER = (92, 126, 116)
SLIME = (76, 156, 96)
BANDIT = (96, 62, 72)
BAT = (67, 65, 88)
GUARD = (54, 76, 116)
BAKER = (180, 150, 92)
TRAINER = (112, 76, 116)
SKELETON = (205, 204, 185)
WOLF = (84, 82, 76)
WOOD = (99, 66, 42)
LEAF = (45, 112, 52)
LEAF_DARK = (31, 78, 45)
STONE = (112, 116, 111)
STONE_DARK = (82, 85, 82)
WATER = (47, 99, 146)
WATER_HI = (164, 210, 222)
FIRE = (220, 92, 43)
GOLD = (230, 194, 90)
GRASS = (72, 126, 79)
GRASS_DARK = (48, 92, 58)
PATH = (143, 118, 82)
ROOF = (155, 72, 42)
ROOF_DARK = (104, 48, 36)
CREAM = (224, 205, 164)


def mix(color: tuple[int, int, int], amount: int) -> tuple[int, int, int]:
    return tuple(max(0, min(255, channel + amount)) for channel in color)


def shadow(draw: ImageDraw.ImageDraw, x: int, y: int, w: int = 22) -> None:
    draw.ellipse((x + 16 - w // 2, y + 26, x + 16 + w // 2, y + 31),
                 fill=(38, 48, 39))


def rect3(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int],
          fill: tuple[int, int, int], outline: tuple[int, int, int] = INK) -> None:
    x1, y1, x2, y2 = box
    draw.rectangle(box, fill=fill, outline=outline)
    if x2 - x1 > 5 and y2 - y1 > 5:
        draw.line((x1 + 1, y1 + 1, x2 - 1, y1 + 1), fill=mix(fill, 34))
        draw.line((x1 + 1, y1 + 2, x1 + 1, y2 - 1), fill=mix(fill, 18))
        draw.line((x1 + 1, y2 - 1, x2 - 1, y2 - 1), fill=mix(fill, -38))
        draw.line((x2 - 1, y1 + 2, x2 - 1, y2 - 1), fill=mix(fill, -30))


def roof(draw: ImageDraw.ImageDraw, x: int, y: int, w: int = 28) -> None:
    draw.polygon([(x + 16, y + 2), (x + 2, y + 13), (x + w, y + 13)],
                 fill=ROOF_DARK, outline=INK)
    draw.polygon([(x + 16, y + 4), (x + 5, y + 13), (x + w - 3, y + 13)],
                 fill=ROOF)
    draw.line((x + 6, y + 11, x + w - 4, y + 11), fill=mix(ROOF, 35))


def ensure_dirs() -> None:
    for rel in ("sprites", "tiles"):
        (OUT / rel).mkdir(parents=True, exist_ok=True)


def draw_person(draw: ImageDraw.ImageDraw, x: int, y: int, tunic: tuple[int, int, int],
                facing: str = "south", step: int = 1) -> None:
    arm = 0 if step == 1 else (-2 if step == 0 else 2)
    leg = 0 if step == 1 else (-2 if step == 0 else 2)
    shadow(draw, x, y, 18)
    rect3(draw, (x + 10 + leg, y + 24, x + 14 + leg, y + 30), (42, 42, 44))
    rect3(draw, (x + 18 - leg, y + 24, x + 22 - leg, y + 30), (42, 42, 44))
    rect3(draw, (x + 8, y + 12, x + 23, y + 25), tunic)
    rect3(draw, (x + 5 + arm, y + 13, x + 10 + arm, y + 19), mix(tunic, -12))
    rect3(draw, (x + 22 - arm, y + 13, x + 27 - arm, y + 19), mix(tunic, -12))
    draw.ellipse((x + 10, y + 3, x + 21, y + 14), fill=mix(SKIN, -8), outline=INK)
    draw.ellipse((x + 12, y + 5, x + 20, y + 13), fill=SKIN)
    draw.rectangle((x + 9, y + 4, x + 22, y + 7), fill=HAIR)
    if facing == "north":
        draw.rectangle((x + 10, y + 4, x + 21, y + 11), fill=HAIR)
    elif facing == "east":
        draw.rectangle((x + 18, y + 7, x + 20, y + 9), fill=INK)
    elif facing == "west":
        draw.rectangle((x + 12, y + 7, x + 14, y + 9), fill=INK)
    else:
        draw.rectangle((x + 12, y + 8, x + 14, y + 10), fill=INK)
        draw.rectangle((x + 18, y + 8, x + 20, y + 10), fill=INK)
        draw.point((x + 16, y + 11), fill=mix(SKIN, -45))


def draw_tree(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 24)
    rect3(draw, (x + 13, y + 17, x + 18, y + 30), WOOD)
    for box, color in (
        ((x + 8, y + 2, x + 24, y + 17), LEAF_DARK),
        ((x + 3, y + 9, x + 19, y + 25), LEAF),
        ((x + 13, y + 8, x + 29, y + 24), mix(LEAF, 12)),
        ((x + 7, y + 7, x + 25, y + 23), LEAF),
    ):
        draw.ellipse(box, fill=color, outline=INK)
    draw.arc((x + 7, y + 6, x + 24, y + 19), 205, 300, fill=mix(LEAF, 50), width=2)


def draw_rock(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 22)
    draw.polygon([(x + 4, y + 27), (x + 9, y + 13), (x + 19, y + 7),
                  (x + 29, y + 17), (x + 26, y + 29)],
                 fill=STONE_DARK, outline=INK)
    draw.polygon([(x + 9, y + 14), (x + 19, y + 8), (x + 25, y + 17),
                  (x + 17, y + 20)], fill=mix(STONE, 22))
    draw.polygon([(x + 7, y + 22), (x + 17, y + 20), (x + 26, y + 27),
                  (x + 8, y + 28)], fill=STONE)
    draw.line((x + 10, y + 25, x + 25, y + 25), fill=mix(STONE_DARK, -25))


def draw_pond(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 28)
    draw.ellipse((x + 1, y + 9, x + 30, y + 27), fill=mix(WATER, -20), outline=INK)
    draw.ellipse((x + 4, y + 11, x + 27, y + 24), fill=WATER)
    draw.arc((x + 6, y + 12, x + 25, y + 23), 12, 168, fill=WATER_HI, width=2)
    draw.polygon([(x + 13, y + 18), (x + 18, y + 15), (x + 23, y + 18),
                  (x + 18, y + 21)], fill=(205, 215, 205), outline=INK)


def draw_fire(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 18)
    rect3(draw, (x + 7, y + 25, x + 25, y + 29), WOOD)
    draw.polygon([(x + 16, y + 6), (x + 8, y + 25), (x + 24, y + 25)],
                 fill=mix(FIRE, -18), outline=INK)
    draw.polygon([(x + 16, y + 10), (x + 11, y + 25), (x + 21, y + 25)],
                 fill=FIRE)
    draw.polygon([(x + 16, y + 15), (x + 13, y + 25), (x + 19, y + 25)],
                 fill=GOLD)


def draw_workbench(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 26)
    rect3(draw, (x + 3, y + 14, x + 29, y + 21), WOOD)
    rect3(draw, (x + 6, y + 21, x + 10, y + 29), STONE_DARK)
    rect3(draw, (x + 22, y + 21, x + 26, y + 29), STONE_DARK)
    rect3(draw, (x + 14, y + 7, x + 22, y + 11), STONE)


def draw_combat(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 20)
    draw.line((x + 8, y + 25, x + 24, y + 8), fill=INK, width=5)
    draw.line((x + 8, y + 25, x + 24, y + 8), fill=GOLD, width=3)
    draw.line((x + 9, y + 8, x + 24, y + 24), fill=INK, width=5)
    draw.line((x + 9, y + 8, x + 24, y + 24), fill=STONE, width=3)
    rect3(draw, (x + 13, y + 20, x + 19, y + 28), WOOD)


def draw_icon(draw: ImageDraw.ImageDraw, x: int, y: int, kind: str) -> None:
    if kind == "hp":
        draw.polygon([(x + 16, y + 27), (x + 5, y + 14), (x + 8, y + 6),
                      (x + 16, y + 10), (x + 24, y + 6), (x + 27, y + 14)],
                     fill=(185, 52, 70), outline=INK)
    elif kind == "coins":
        for off in (0, 5, 10):
            draw.ellipse((x + 8 + off, y + 10, x + 20 + off, y + 22),
                         fill=GOLD, outline=INK)
    elif kind == "logs":
        draw.rectangle((x + 5, y + 11, x + 27, y + 17), fill=WOOD, outline=INK)
        draw.rectangle((x + 5, y + 19, x + 27, y + 25), fill=WOOD, outline=INK)
    elif kind == "ore":
        draw_rock(draw, x, y)
    elif kind == "fish":
        draw.polygon([(x + 7, y + 17), (x + 17, y + 10), (x + 27, y + 17),
                      (x + 17, y + 24)], fill=WATER_HI, outline=INK)
        draw.polygon([(x + 7, y + 17), (x + 2, y + 11), (x + 2, y + 23)],
                     fill=WATER, outline=INK)
    elif kind == "food":
        draw.ellipse((x + 8, y + 9, x + 24, y + 25), fill=(180, 88, 45), outline=INK)
        draw.rectangle((x + 14, y + 4, x + 18, y + 12), fill=WOOD)
    elif kind == "combat":
        draw_combat(draw, x, y)


def draw_gate(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 28)
    rect3(draw, (x + 3, y + 11, x + 28, y + 30), STONE)
    roof(draw, x, y - 1, 30)
    rect3(draw, (x + 9, y + 17, x + 22, y + 30), (60, 43, 33))
    draw.rectangle((x + 15, y + 22, x + 17, y + 30), fill=mix(INK, 10))


def draw_market(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 26)
    rect3(draw, (x + 4, y + 16, x + 28, y + 25), WOOD)
    rect3(draw, (x + 7, y + 25, x + 10, y + 30), STONE_DARK)
    rect3(draw, (x + 22, y + 25, x + 25, y + 30), STONE_DARK)
    draw.polygon([(x + 3, y + 15), (x + 9, y + 7), (x + 25, y + 7), (x + 30, y + 15)],
                 fill=mix(GOLD, -16), outline=INK)
    draw.line((x + 9, y + 9, x + 25, y + 9), fill=mix(GOLD, 38), width=2)
    rect3(draw, (x + 8, y + 18, x + 13, y + 22), (180, 88, 45), outline=mix(INK, 20))
    rect3(draw, (x + 18, y + 18, x + 24, y + 22), SLIME, outline=mix(INK, 20))


def draw_inn(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 27)
    rect3(draw, (x + 5, y + 13, x + 27, y + 30), WOOD)
    roof(draw, x, y, 29)
    rect3(draw, (x + 12, y + 20, x + 20, y + 30), (55, 39, 32))
    rect3(draw, (x + 7, y + 16, x + 12, y + 21), WATER_HI)
    rect3(draw, (x + 21, y + 16, x + 26, y + 21), WATER_HI)


def draw_slime(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 24)
    draw.ellipse((x + 5, y + 13, x + 27, y + 29), fill=mix(SLIME, -18), outline=INK)
    draw.ellipse((x + 8, y + 10, x + 24, y + 24), fill=SLIME)
    draw.ellipse((x + 11, y + 8, x + 22, y + 19), fill=(101, 184, 118), outline=INK)
    draw.ellipse((x + 13, y + 10, x + 17, y + 13), fill=mix(SLIME, 70))
    draw.rectangle((x + 11, y + 18, x + 13, y + 20), fill=INK)
    draw.rectangle((x + 20, y + 18, x + 22, y + 20), fill=INK)
    draw.arc((x + 12, y + 20, x + 22, y + 26), 15, 165, fill=INK)


def draw_bandit(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    draw_person(draw, x, y, BANDIT)
    draw.rectangle((x + 10, y + 7, x + 22, y + 10), fill=INK)
    draw.line((x + 24, y + 17, x + 30, y + 9), fill=INK, width=3)
    draw.line((x + 24, y + 17, x + 30, y + 9), fill=mix(STONE, 45), width=1)


def draw_bat(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 20)
    draw.polygon([(x + 2, y + 17), (x + 11, y + 9), (x + 16, y + 16),
                  (x + 21, y + 9), (x + 30, y + 17), (x + 22, y + 24),
                  (x + 16, y + 20), (x + 10, y + 24)],
                 fill=mix(BAT, -12), outline=INK)
    draw.polygon([(x + 5, y + 17), (x + 12, y + 12), (x + 15, y + 17),
                  (x + 10, y + 21)], fill=BAT)
    draw.polygon([(x + 27, y + 17), (x + 20, y + 12), (x + 17, y + 17),
                  (x + 22, y + 21)], fill=BAT)
    draw.ellipse((x + 12, y + 13, x + 20, y + 22), fill=(86, 82, 108), outline=INK)
    draw.rectangle((x + 13, y + 15, x + 15, y + 17), fill=GOLD)
    draw.rectangle((x + 18, y + 15, x + 20, y + 17), fill=GOLD)


def draw_anvil(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 20)
    rect3(draw, (x + 7, y + 14, x + 25, y + 21), STONE)
    draw.polygon([(x + 2, y + 15), (x + 9, y + 12), (x + 9, y + 18)],
                 fill=STONE, outline=INK)
    rect3(draw, (x + 12, y + 21, x + 20, y + 29), STONE_DARK)


def draw_shield(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    draw.polygon([(x + 16, y + 4), (x + 27, y + 9), (x + 24, y + 23),
                  (x + 16, y + 30), (x + 8, y + 23), (x + 5, y + 9)],
                 fill=GOLD, outline=INK)
    draw.line((x + 16, y + 6, x + 16, y + 28), fill=INK)
    draw.line((x + 8, y + 14, x + 24, y + 14), fill=INK)


def draw_counter(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 26)
    rect3(draw, (x + 3, y + 15, x + 29, y + 27), WOOD)
    rect3(draw, (x + 5, y + 10, x + 27, y + 16), SHOP)
    rect3(draw, (x + 8, y + 6, x + 13, y + 11), (180, 88, 45))
    rect3(draw, (x + 18, y + 6, x + 24, y + 11), SLIME)


def draw_forge(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 24)
    rect3(draw, (x + 5, y + 13, x + 27, y + 29), STONE_DARK)
    rect3(draw, (x + 8, y + 8, x + 24, y + 16), STONE)
    draw.polygon([(x + 16, y + 9), (x + 10, y + 27), (x + 22, y + 27)],
                 fill=FIRE, outline=INK)
    draw.polygon([(x + 16, y + 15), (x + 13, y + 27), (x + 19, y + 27)],
                 fill=GOLD)


def draw_bed(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 26)
    rect3(draw, (x + 4, y + 12, x + 29, y + 27), WOOD)
    rect3(draw, (x + 7, y + 8, x + 17, y + 16), WATER_HI)
    rect3(draw, (x + 15, y + 13, x + 28, y + 26), ROOF)
    rect3(draw, (x + 5, y + 27, x + 8, y + 30), STONE_DARK)
    rect3(draw, (x + 25, y + 27, x + 28, y + 30), STONE_DARK)


def draw_table(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 22)
    draw.ellipse((x + 5, y + 9, x + 27, y + 25), fill=mix(WOOD, -12), outline=INK)
    draw.ellipse((x + 8, y + 11, x + 24, y + 22), fill=WOOD)
    draw.ellipse((x + 12, y + 12, x + 20, y + 20), fill=(180, 88, 45), outline=INK)
    rect3(draw, (x + 14, y + 23, x + 18, y + 30), STONE_DARK)


def draw_shrine(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 22)
    rect3(draw, (x + 8, y + 22, x + 24, y + 29), STONE)
    rect3(draw, (x + 12, y + 8, x + 20, y + 23), HEALER)
    draw.polygon([(x + 16, y + 3), (x + 24, y + 10), (x + 8, y + 10)],
                 fill=GOLD, outline=INK)
    draw.line((x + 16, y + 11, x + 16, y + 20), fill=GOLD, width=2)


def draw_well(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 24)
    draw.ellipse((x + 5, y + 14, x + 27, y + 29), fill=STONE, outline=INK)
    draw.ellipse((x + 8, y + 16, x + 24, y + 25), fill=WATER, outline=INK)
    draw.line((x + 8, y + 14, x + 8, y + 6), fill=WOOD, width=2)
    draw.line((x + 24, y + 14, x + 24, y + 6), fill=WOOD, width=2)
    draw.line((x + 7, y + 6, x + 25, y + 6), fill=WOOD, width=2)


def draw_sign(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 18)
    rect3(draw, (x + 14, y + 16, x + 18, y + 30), WOOD)
    rect3(draw, (x + 4, y + 6, x + 28, y + 18), GOLD)
    draw.line((x + 8, y + 10, x + 24, y + 10), fill=INK)
    draw.line((x + 8, y + 14, x + 20, y + 14), fill=INK)


def draw_crate(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 22)
    rect3(draw, (x + 6, y + 11, x + 26, y + 29), WOOD)
    draw.line((x + 6, y + 11, x + 26, y + 29), fill=INK)
    draw.line((x + 26, y + 11, x + 6, y + 29), fill=INK)


def draw_bookshelf(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 22)
    rect3(draw, (x + 5, y + 5, x + 27, y + 30), WOOD)
    for row in (9, 16, 23):
        draw.line((x + 6, y + row, x + 26, y + row), fill=INK)
    for bx, color in ((8, ROOF), (12, WATER_HI), (17, GOLD), (22, HEALER)):
        draw.rectangle((x + bx, y + 6, x + bx + 3, y + 28), fill=color)


def draw_chest(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 22)
    rect3(draw, (x + 6, y + 15, x + 26, y + 29), WOOD)
    draw.arc((x + 6, y + 7, x + 26, y + 23), 180, 360, fill=INK, width=2)
    draw.rectangle((x + 14, y + 18, x + 18, y + 22), fill=GOLD, outline=INK)


def draw_dummy(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 20)
    rect3(draw, (x + 14, y + 8, x + 18, y + 30), WOOD)
    draw.ellipse((x + 10, y + 3, x + 22, y + 14), fill=WOOD, outline=INK)
    draw.ellipse((x + 12, y + 5, x + 20, y + 12), fill=mix(WOOD, 24))
    rect3(draw, (x + 7, y + 14, x + 25, y + 23), SHOP)
    draw.line((x + 7, y + 18, x + 25, y + 18), fill=INK)


def draw_herb_bed(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 26)
    rect3(draw, (x + 3, y + 18, x + 29, y + 29), WOOD)
    for ox in (6, 12, 18, 24):
        draw.line((x + ox, y + 24, x + ox - 3, y + 15), fill=LEAF, width=2)
        draw.line((x + ox, y + 24, x + ox + 3, y + 14), fill=LEAF_DARK, width=2)
    draw.rectangle((x + 4, y + 25, x + 28, y + 29), fill=GRASS_DARK)


def draw_bakery(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 27)
    rect3(draw, (x + 5, y + 13, x + 27, y + 30), BAKER)
    roof(draw, x, y, 29)
    draw.arc((x + 10, y + 17, x + 22, y + 29), 180, 360, fill=INK, width=2)
    draw.ellipse((x + 19, y + 10, x + 25, y + 16), fill=GOLD, outline=INK)


def draw_skeleton(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 18)
    draw.ellipse((x + 11, y + 4, x + 21, y + 14), fill=mix(SKELETON, -18), outline=INK)
    draw.ellipse((x + 13, y + 5, x + 20, y + 12), fill=SKELETON)
    rect3(draw, (x + 14, y + 14, x + 18, y + 25), SKELETON)
    draw.line((x + 14, y + 17, x + 6, y + 24), fill=INK, width=4)
    draw.line((x + 18, y + 17, x + 27, y + 23), fill=INK, width=4)
    draw.line((x + 15, y + 25, x + 10, y + 30), fill=INK, width=4)
    draw.line((x + 17, y + 25, x + 22, y + 30), fill=INK, width=4)
    draw.line((x + 14, y + 17, x + 6, y + 24), fill=SKELETON, width=2)
    draw.line((x + 18, y + 17, x + 27, y + 23), fill=SKELETON, width=2)
    draw.line((x + 15, y + 25, x + 10, y + 30), fill=SKELETON, width=2)
    draw.line((x + 17, y + 25, x + 22, y + 30), fill=SKELETON, width=2)
    draw.rectangle((x + 13, y + 8, x + 15, y + 10), fill=INK)
    draw.rectangle((x + 18, y + 8, x + 20, y + 10), fill=INK)


def draw_wolf(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 23)
    draw.ellipse((x + 7, y + 15, x + 24, y + 25), fill=mix(WOLF, -12), outline=INK)
    draw.ellipse((x + 10, y + 16, x + 22, y + 23), fill=WOLF)
    draw.polygon([(x + 21, y + 15), (x + 29, y + 11), (x + 27, y + 21)],
                 fill=WOLF, outline=INK)
    draw.polygon([(x + 22, y + 13), (x + 24, y + 7), (x + 26, y + 14)],
                 fill=WOLF, outline=INK)
    draw.line((x + 9, y + 24, x + 7, y + 30), fill=INK, width=2)
    draw.line((x + 20, y + 24, x + 22, y + 30), fill=INK, width=2)
    draw.rectangle((x + 26, y + 15, x + 28, y + 17), fill=GOLD)


def draw_sewer_rat(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    shadow(draw, x, y, 20)
    draw.ellipse((x + 7, y + 16, x + 25, y + 26), fill=STONE_DARK, outline=INK)
    draw.ellipse((x + 20, y + 13, x + 29, y + 21), fill=STONE_DARK, outline=INK)
    draw.line((x + 8, y + 20, x + 2, y + 16), fill=INK, width=2)
    draw.rectangle((x + 25, y + 16, x + 27, y + 18), fill=GOLD)


def sprite_sheet() -> None:
    image = Image.new("RGB", (320, 160), KEY)
    draw = ImageDraw.Draw(image)
    draw_person(draw, 0, 0, BLUE)
    draw_person(draw, 32, 0, GUIDE)
    draw_person(draw, 64, 0, SHOP)
    draw_person(draw, 96, 0, ENEMY)
    draw_tree(draw, 128, 0)
    draw_rock(draw, 160, 0)
    draw_pond(draw, 192, 0)
    draw_fire(draw, 224, 0)
    draw_workbench(draw, 256, 0)
    draw_combat(draw, 288, 0)
    for index, kind in enumerate(("hp", "coins", "logs", "ore", "fish", "food", "combat")):
        draw_icon(draw, index * 32, 32, kind)
    draw_gate(draw, 0, 64)
    draw_person(draw, 32, 64, SMITH)
    draw_inn(draw, 64, 64)
    draw_person(draw, 96, 64, HEALER)
    draw_slime(draw, 128, 64)
    draw_bandit(draw, 160, 64)
    draw_bat(draw, 192, 64)
    draw_market(draw, 224, 64)
    draw_anvil(draw, 256, 64)
    draw_shield(draw, 288, 64)
    draw_counter(draw, 0, 96)
    draw_forge(draw, 32, 96)
    draw_bed(draw, 64, 96)
    draw_table(draw, 96, 96)
    draw_shrine(draw, 128, 96)
    draw_well(draw, 160, 96)
    draw_sign(draw, 192, 96)
    draw_crate(draw, 224, 96)
    draw_bookshelf(draw, 256, 96)
    draw_chest(draw, 288, 96)
    draw_person(draw, 0, 128, GUARD)
    draw_person(draw, 32, 128, BAKER)
    draw_person(draw, 64, 128, TRAINER)
    draw_dummy(draw, 96, 128)
    draw_herb_bed(draw, 128, 128)
    draw_bakery(draw, 160, 128)
    draw_chest(draw, 192, 128)
    draw_skeleton(draw, 224, 128)
    draw_wolf(draw, 256, 128)
    draw_sewer_rat(draw, 288, 128)
    image.save(OUT / "sprites" / "runepod_sprites.320x160x24.bmp")


def player_dirs() -> None:
    image = Image.new("RGB", (384, 32), KEY)
    draw = ImageDraw.Draw(image)
    for index, facing in enumerate(("south", "east", "north", "west")):
        for frame in range(3):
            draw_person(draw, (index * 3 + frame) * 32, 0, BLUE, facing, frame)
    image.save(OUT / "sprites" / "runepod_player_dirs.384x32x24.bmp")


def tile(draw: ImageDraw.ImageDraw, x: int, base: tuple[int, int, int],
         accents: list[tuple[int, int, int, int, int, int]]) -> None:
    draw.rectangle((x, 0, x + 31, 31), fill=base)
    for x1, y1, x2, y2, color_i, width in accents:
        colors = [GRASS_DARK, GRASS, PATH, WOOD, STONE, WATER, ROOF, INK]
        draw.line((x + x1, y1, x + x2, y2), fill=colors[color_i], width=width)


def tile_grass(draw: ImageDraw.ImageDraw, x: int, base: tuple[int, int, int],
               dark: tuple[int, int, int], light: tuple[int, int, int]) -> None:
    draw.rectangle((x, 0, x + 31, 31), fill=base)
    for ox, oy in ((4, 7), (14, 5), (24, 9), (8, 21), (20, 24)):
        draw.line((x + ox, oy + 3, x + ox + 3, oy), fill=dark)
        draw.line((x + ox + 1, oy + 3, x + ox + 5, oy + 2), fill=light)
    draw.point((x + 27, 22), fill=GOLD)
    draw.point((x + 5, 18), fill=mix(GRASS, 42))


def tile_path(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.rectangle((x, 0, x + 31, 31), fill=PATH)
    for y in (6, 14, 23):
        draw.line((x + 1, y, x + 30, y - 2), fill=mix(PATH, -28))
        draw.line((x + 1, y + 1, x + 30, y - 1), fill=mix(PATH, 22))
    for ox, oy in ((6, 9), (19, 6), (11, 20), (26, 24)):
        draw.rectangle((x + ox, oy, x + ox + 2, oy + 1), fill=mix(PATH, -38))


def tile_wood(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.rectangle((x, 0, x + 31, 31), fill=WOOD)
    for y in (7, 15, 23):
        draw.line((x, y, x + 31, y), fill=mix(WOOD, -42))
        draw.line((x, y + 1, x + 31, y + 1), fill=mix(WOOD, 24))
    for ox in (8, 18, 27):
        draw.line((x + ox, 1, x + ox - 2, 30), fill=mix(WOOD, -24))


def tile_stone(draw: ImageDraw.ImageDraw, x: int, base: tuple[int, int, int]) -> None:
    draw.rectangle((x, 0, x + 31, 31), fill=base)
    for y in (8, 18, 27):
        draw.line((x, y, x + 31, y), fill=mix(base, -42))
        draw.line((x, y + 1, x + 31, y + 1), fill=mix(base, 22))
    for ox, y1, y2 in ((9, 0, 8), (21, 8, 18), (13, 18, 27), (25, 18, 31)):
        draw.line((x + ox, y1, x + ox, y2), fill=mix(base, -38))
    draw.line((x + 3, 3, x + 10, 1), fill=mix(base, 35))
    draw.line((x + 18, 22, x + 27, 20), fill=mix(base, 32))


def tile_water(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.rectangle((x, 0, x + 31, 31), fill=mix(WATER, -16))
    for y in (6, 14, 22):
        draw.arc((x + 1, y - 4, x + 17, y + 6), 10, 165, fill=WATER_HI, width=2)
        draw.arc((x + 15, y - 2, x + 31, y + 8), 10, 165, fill=mix(WATER, 35), width=2)
    draw.rectangle((x, 28, x + 31, 31), fill=mix(WATER, -34))


def tile_roof(draw: ImageDraw.ImageDraw, x: int) -> None:
    draw.rectangle((x, 0, x + 31, 31), fill=ROOF_DARK)
    for y in range(-8, 40, 8):
        draw.line((x, y, x + 31, y + 31), fill=mix(ROOF, 8), width=4)
        draw.line((x, y + 4, x + 31, y + 35), fill=mix(ROOF_DARK, -18), width=1)
    draw.rectangle((x, 0, x + 31, 2), fill=mix(ROOF, 35))


def terrain_sheet() -> None:
    image = Image.new("RGB", (256, 32), KEY)
    draw = ImageDraw.Draw(image)
    tile_grass(draw, 0, GRASS, GRASS_DARK, mix(GRASS, 34))
    tile_grass(draw, 32, GRASS_DARK, mix(GRASS_DARK, -24), GRASS)
    tile_path(draw, 64)
    tile_wood(draw, 96)
    tile_stone(draw, 128, STONE)
    tile_water(draw, 160)
    tile_roof(draw, 192)
    tile_stone(draw, 224, STONE_DARK)
    image.save(OUT / "tiles" / "runepod_terrain_tiles.256x32x24.bmp")


def main() -> None:
    ensure_dirs()
    sprite_sheet()
    player_dirs()
    terrain_sheet()


if __name__ == "__main__":
    main()
