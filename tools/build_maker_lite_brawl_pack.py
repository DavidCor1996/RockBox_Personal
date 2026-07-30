#!/usr/bin/env python3
"""Build four bounded original-generated Maker Brawl fighter kits."""

from __future__ import annotations

import hashlib
import json
import shutil
import struct
import zlib
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (
    ROOT
    / "assets"
    / "maker_lite"
    / "maker_brawl"
    / "source"
    / "roster-concepts.png"
)
PACK_ROOT = ROOT / "assets" / "maker_lite" / "maker_brawl" / "pack"
FIGHTERS = (
    ("volt-jack", "Volt Jack", 100),
    ("mossbyte", "Mossbyte", 125),
    ("cinderwing", "Cinderwing", 82),
    ("bone-corsair", "Bone Corsair", 112),
)
TRANSPARENT_RGB565 = 0xF81F


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fitted_character(image: Image.Image) -> Image.Image:
    image = image.convert("RGBA")
    bbox = image.getbbox()
    if bbox:
        image = image.crop(bbox)
    image.thumbnail((14, 15), Image.Resampling.LANCZOS)
    alpha = image.getchannel("A").point(lambda value: 255 if value >= 80 else 0)
    rgb = image.convert("RGB").quantize(
        colors=32,
        method=Image.Quantize.MEDIANCUT,
    )
    image = rgb.convert("RGBA")
    image.putalpha(alpha)
    result = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    result.alpha_composite(image, ((16 - image.width) // 2, 15 - image.height))
    return result


def rgb565(image: Image.Image) -> bytes:
    output = bytearray()
    pixels = image.convert("RGBA").load()
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, alpha = pixels[x, y]
            value = (
                TRANSPARENT_RGB565
                if alpha < 80
                else ((red & 0xF8) << 8) |
                     ((green & 0xFC) << 3) |
                     (blue >> 3)
            )
            output.extend(struct.pack("<H", value))
    return bytes(output)


def terrain_cells() -> list[Image.Image]:
    cells = []
    for background, edge in (
        ((40, 49, 70, 255), (104, 218, 255, 255)),
        ((58, 45, 82, 255), (241, 170, 255, 255)),
        ((70, 30, 38, 255), (255, 120, 80, 255)),
    ):
        image = Image.new("RGBA", (16, 16), background)
        draw = ImageDraw.Draw(image)
        draw.rectangle((0, 0, 15, 2), fill=edge)
        draw.line((0, 7, 15, 7), fill=(255, 255, 255, 36))
        cells.append(image)
    return cells


def animation_table() -> bytes:
    output = bytearray()
    for action in range(14):
        for direction in range(4):
            output.extend(
                struct.pack(
                    "<HBB",
                    action,
                    1,
                    6 | (0x80 if direction == 2 else 0),
                )
            )
    return bytes(output)


def extract_roster(sheet: Image.Image) -> list[Image.Image]:
    width, height = sheet.size
    result = []
    for index in range(4):
        column = index % 2
        row = index // 2
        crop = sheet.crop(
            (
                column * width // 2,
                row * height // 2,
                (column + 1) * width // 2,
                (row + 1) * height // 2,
            )
        )
        result.append(fitted_character(crop))
    return result


def build() -> None:
    if not SOURCE.is_file():
        raise SystemExit(f"missing generated roster source: {SOURCE}")
    sheet = Image.open(SOURCE).convert("RGBA")
    roster = extract_roster(sheet)
    source_digest = sha256(SOURCE)
    PACK_ROOT.mkdir(parents=True, exist_ok=True)

    for player_index, (fighter_id, fighter_name, _weight) in enumerate(FIGHTERS):
        kit_id = f"maker-brawl-{fighter_id}-v1"
        directory = PACK_ROOT / kit_id
        if directory.exists():
            shutil.rmtree(directory)
        directory.mkdir(parents=True)
        player = roster[player_index]
        action_cells = [
            ImageOps.mirror(player) if action in {6, 10, 11} else player.copy()
            for action in range(14)
        ]
        cells = action_cells + roster + terrain_cells()
        payload = b"".join(rgb565(cell) for cell in cells) + animation_table()
        header = bytearray(64)
        header[:4] = b"MLAR"
        struct.pack_into(
            "<HHHHI",
            header,
            4,
            3,
            16,
            len(cells),
            0,
            zlib.crc32(payload) & 0xFFFFFFFF,
        )
        header[16 : 16 + len(kit_id)] = kit_id.encode("ascii")
        header[48 + 4] = 14
        (directory / "art.mla").write_bytes(bytes(header) + payload)

        catalog = [
            {
                "id": other_id,
                "label": other_name,
                "category": "Fighters",
                "type": "entity",
                "cell": 14 + index,
                "kind": "enemy",
                "params": [14, 26, other_weight, 3],
                "flags": 1024,
            }
            for index, (other_id, other_name, other_weight) in enumerate(FIGHTERS)
        ]
        catalog.extend(
            [
                {
                    "id": "brawl-floor",
                    "label": "Brawl Floor",
                    "category": "Arena",
                    "type": "terrain",
                    "cell": 18,
                    "collision": ["solid"],
                },
                {
                    "id": "brawl-platform",
                    "label": "Brawl Platform",
                    "category": "Arena",
                    "type": "terrain",
                    "cell": 19,
                    "collision": ["one_way"],
                },
                {
                    "id": "brawl-blast-floor",
                    "label": "Blast Floor",
                    "category": "Arena",
                    "type": "terrain",
                    "cell": 20,
                    "collision": ["hazard"],
                },
            ]
        )
        manifest = {
            "kit_id": kit_id,
            "ruleset": "mario",
            "revision_id": "maker-brawl-original-v1",
            "player_name": fighter_name,
            "source_cover_file": "source-cover.png",
            "cell_count": len(cells),
            "catalog_count": len(catalog),
            "entity_cells": {"enemy": 14},
            "asset_catalog": catalog,
            "source": {
                "type": "original-generated",
                "file": "roster-concepts.png",
                "sha256": source_digest,
            },
        }
        (directory / "kit.mlk").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        rows = ["cell\tname\tsource\tsha256\toperation"]
        for index in range(len(cells)):
            rows.append(
                f"{index}\tbrawl-cell-{index}\troster-concepts.png\t"
                f"{source_digest}\tcrop-quantize-rgb565"
            )
        (directory / "provenance.tsv").write_text(
            "\n".join(rows) + "\n",
            encoding="utf-8",
        )
        cover = Image.new("RGB", (320, 240), (15, 18, 28))
        portrait = player.resize((160, 160), Image.Resampling.NEAREST)
        cover.paste(portrait.convert("RGB"), (80, 36))
        ImageDraw.Draw(cover).text((82, 210), fighter_name, fill="white")
        cover.save(directory / "source-cover.png")


if __name__ == "__main__":
    build()
