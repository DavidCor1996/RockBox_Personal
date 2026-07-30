#!/usr/bin/env python3
"""Build the bundled original Neon Nook Maker Lite asset kit.

The committed source sheets are original generated artwork.  This builder
removes no commercial-content boundary: it converts those sheets into the
same bounded MLAR/MLAU formats used by private Maker Lite kits and records
per-cell provenance for every shipped atlas cell.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import shutil
import struct
import wave
import zlib
from pathlib import Path

from PIL import Image, ImageOps


REPO = Path(__file__).resolve().parents[1]
DEFAULT_ROOT = REPO / "assets/maker_lite/neon_nook"
KIT_ID = "zelda-neon-nook-v1"
RULESET = "zelda"
CELL_SIZE = 16
ATLAS_COLUMNS = 24
MAX_CELLS = 1536
ACTION_NAMES = (
    "idle",
    "walk",
    "run",
    "jump",
    "fall",
    "crouch",
    "skid",
    "swim",
    "sword",
    "item",
    "roll",
    "spindash",
    "hurt",
    "complete",
)
DIRECTION_NAMES = ("right", "down", "left", "up")
EFFECT_NAMES = ("jump", "collect", "hurt", "goal", "action", "spring")
TRANSPARENT_RGB565 = 0xF81F


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def quantize_pixel_art(image: Image.Image) -> Image.Image:
    """Keep a small, cohesive palette and hard alpha edges."""

    image = image.convert("RGBA")
    alpha = image.getchannel("A").point(lambda value: 255 if value >= 72 else 0)
    rgb = image.convert("RGB").quantize(colors=48, method=Image.Quantize.MEDIANCUT)
    result = rgb.convert("RGBA")
    result.putalpha(alpha)
    return result


def fitted_cell(image: Image.Image, *, margin: int = 1) -> Image.Image:
    image = image.convert("RGBA")
    bbox = image.getbbox()
    if not bbox:
        return Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
    image = image.crop(bbox)
    available = CELL_SIZE - margin * 2
    image.thumbnail((available, available), Image.Resampling.LANCZOS)
    image = quantize_pixel_art(image)
    result = Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
    x = (CELL_SIZE - image.width) // 2
    y = CELL_SIZE - margin - image.height
    result.alpha_composite(image, (x, y))
    return result


def fitted_player(image: Image.Image) -> Image.Image:
    image = image.convert("RGBA")
    bbox = image.getbbox()
    if bbox:
        image = image.crop(bbox)
    image.thumbnail((28, 30), Image.Resampling.LANCZOS)
    image = quantize_pixel_art(image)
    result = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    result.alpha_composite(image, ((32 - image.width) // 2, 31 - image.height))
    return remove_alpha_specks(result)


def remove_alpha_specks(
    image: Image.Image, *, minimum_pixels: int = 6
) -> Image.Image:
    """Remove disconnected source-sheet flecks without trimming real tails."""

    image = image.convert("RGBA")
    alpha = image.getchannel("A")
    pixels = alpha.load()
    visited = set()
    remove = []
    for y in range(image.height):
        for x in range(image.width):
            if (x, y) in visited or pixels[x, y] < 96:
                continue
            component = []
            pending = [(x, y)]
            visited.add((x, y))
            while pending:
                current_x, current_y = pending.pop()
                component.append((current_x, current_y))
                for offset_y in (-1, 0, 1):
                    for offset_x in (-1, 0, 1):
                        neighbor = (
                            current_x + offset_x,
                            current_y + offset_y,
                        )
                        if (
                            neighbor in visited
                            or not (0 <= neighbor[0] < image.width)
                            or not (0 <= neighbor[1] < image.height)
                            or pixels[neighbor[0], neighbor[1]] < 96
                        ):
                            continue
                        visited.add(neighbor)
                        pending.append(neighbor)
            if len(component) < minimum_pixels:
                remove.extend(component)
    for x, y in remove:
        image.putpixel((x, y), (0, 0, 0, 0))
    return image


def occupied_runs(values: list[bool], gap: int = 3) -> list[tuple[int, int]]:
    """Return runs, joining only very small transparent gaps."""

    result = []
    start = None
    last = None
    for index, occupied in enumerate(values):
        if occupied:
            if start is None:
                start = index
            last = index
        elif start is not None and index - last > gap:
            result.append((start, last + 1))
            start = None
            last = None
    if start is not None:
        result.append((start, last + 1))
    return result


def sheet_components(
    image: Image.Image,
    *,
    minimum_area: int = 80,
    exclude: tuple[int, int, int, int] | None = None,
) -> list[tuple[tuple[int, int, int, int], Image.Image]]:
    """Split a chroma-keyed sheet using its transparent row/column gutters."""

    image = image.convert("RGBA")
    alpha = image.getchannel("A")
    width, height = image.size
    pixels = alpha.load()
    y_runs = occupied_runs(
        [any(pixels[x, y] >= 96 for x in range(width)) for y in range(height)],
        gap=4,
    )
    result = []
    for top, bottom in y_runs:
        x_runs = occupied_runs(
            [
                any(pixels[x, y] >= 96 for y in range(top, bottom))
                for x in range(width)
            ],
            gap=5,
        )
        for left, right in x_runs:
            box = (
                max(0, left - 2),
                max(0, top - 2),
                min(width, right + 2),
                min(height, bottom + 2),
            )
            if exclude:
                ex_left, ex_top, ex_right, ex_bottom = exclude
                center_x = (box[0] + box[2]) // 2
                center_y = (box[1] + box[3]) // 2
                if ex_left <= center_x < ex_right and ex_top <= center_y < ex_bottom:
                    continue
            crop = image.crop(box)
            opaque = sum(
                value >= 96
                for value in crop.getchannel("A").get_flattened_data()
            )
            if opaque >= minimum_area:
                result.append((box, crop))
    return sorted(result, key=lambda item: (item[0][1], item[0][0]))


def player_sources(sheet: Image.Image) -> dict[str, list[Image.Image]]:
    """Extract the twelve fox poses using their actual alpha boundaries."""

    sheet = sheet.convert("RGBA")
    poses = []
    for top, bottom in ((0, 145), (145, 285)):
        row = sheet.crop((0, top, 528, bottom))
        components = sheet_components(row, minimum_area=1000)
        if len(components) != 6:
            raise ValueError(
                f"expected six fox poses in row {top}, found {len(components)}"
            )
        poses.extend(fitted_player(crop) for _box, crop in components)
    return {
        "down": [poses[0], poses[1], poses[2]],
        "right": [poses[3], poses[4], poses[5]],
        "left": [
            ImageOps.mirror(poses[3]),
            ImageOps.mirror(poses[4]),
            ImageOps.mirror(poses[5]),
        ],
        "up": [poses[6], poses[7], poses[8]],
    }


TEXTURE_KINDS = (
    "wet-asphalt",
    "sidewalk",
    "grass",
    "garden-soil",
    "harbor-sand",
    "pier-wood",
    "canal-water",
    "interior",
    "roof",
    "tile-floor",
    "wall",
    "fence",
)


def texture_cells(sheet: Image.Image) -> dict[str, list[Image.Image]]:
    """Read a strict 4x3 material sheet and derive four clean variants."""

    sheet = sheet.convert("RGB")
    cell_width = sheet.width // 4
    cell_height = sheet.height // 3
    result = {}
    for index, kind in enumerate(TEXTURE_KINDS):
        column = index % 4
        row = index // 4
        left = column * cell_width
        top = row * cell_height
        source = sheet.crop(
            (left, top, left + cell_width, top + cell_height)
        )
        variants = []
        crop_width = source.width * 4 // 5
        crop_height = source.height * 4 // 5
        for x_factor, y_factor in ((0, 0), (1, 0), (0, 1), (1, 1)):
            x = (source.width - crop_width) * x_factor
            y = (source.height - crop_height) * y_factor
            crop = source.crop((x, y, x + crop_width, y + crop_height))
            crop = crop.resize((16, 16), Image.Resampling.LANCZOS)
            variants.append(quantize_pixel_art(crop))
        result[kind] = variants
    return result


def encode_art(
    cells: list[Image.Image],
    player_frames: list[dict],
    animations: dict[str, dict[str, tuple[int, int, int]]],
    entity_cells: dict[str, int],
) -> bytes:
    pixels = bytearray()
    for cell in cells:
        for red, green, blue, alpha in (
            cell.convert("RGBA").get_flattened_data()
        ):
            value = TRANSPARENT_RGB565 if alpha < 128 else rgb565(red, green, blue)
            pixels.extend(struct.pack("<H", value))
    table = bytearray()
    for action in ACTION_NAMES:
        for direction in DIRECTION_NAMES:
            start, count, ticks = animations[action][direction]
            table.extend(struct.pack("<HBB", start, count, ticks))
    frames = bytearray(struct.pack("<HH", len(player_frames), 0))
    for frame in player_frames:
        values = list(frame["cells"])
        values.extend([0xFFFF] * (16 - len(values)))
        frames.extend(
            struct.pack(
                "<BBbb16H",
                frame["columns"],
                frame["rows"],
                frame["offset_x"],
                frame["offset_y"],
                *values,
            )
        )
    payload = bytes(pixels + table + frames)
    header = bytearray(64)
    header[:4] = b"MLAR"
    struct.pack_into(
        "<HHHHI",
        header,
        4,
        4,
        CELL_SIZE,
        len(cells),
        0,
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    encoded_id = KIT_ID.encode("ascii")
    header[16 : 16 + len(encoded_id)] = encoded_id
    kind_ids = {
        "goal": 2,
        "collectible": 3,
        "enemy": 4,
        "checkpoint": 5,
        "key": 6,
        "door": 7,
        "switch": 8,
        "block": 9,
        "spring": 10,
        "item": 11,
        "pot": 12,
    }
    for name, cell in entity_cells.items():
        if cell >= 256:
            raise ValueError(f"fallback entity cell must fit the header: {name}")
        header[48 + kind_ids[name]] = cell
    return bytes(header) + payload


def synth_effect(index: int, rate: int = 44100) -> bytes:
    durations = (0.11, 0.08, 0.16, 0.32, 0.12, 0.18)
    bases = (440, 920, 155, 523, 660, 300)
    samples = int(rate * durations[index])
    output = bytearray()
    for position in range(samples):
        progress = position / max(1, samples - 1)
        envelope = (1.0 - progress) ** (1.4 if index != 3 else 0.65)
        frequency = bases[index] * (1.0 + (0.65 * progress if index in {1, 3} else 0))
        value = math.sin(2 * math.pi * frequency * position / rate)
        value += 0.28 * math.sin(2 * math.pi * frequency * 2.01 * position / rate)
        amplitude = int(max(-1, min(1, value * 0.7)) * envelope * 10500)
        output.extend(struct.pack("<hh", amplitude, amplitude))
    return bytes(output)


def encode_audio() -> bytes:
    payload = bytearray()
    entries = []
    for index in range(len(EFFECT_NAMES)):
        effect = synth_effect(index)
        entries.append((len(payload), len(effect)))
        payload.extend(effect)
    header = bytearray(128)
    header[:4] = b"MLAU"
    struct.pack_into(
        "<HHII",
        header,
        4,
        1,
        len(EFFECT_NAMES),
        44100,
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    encoded_id = KIT_ID.encode("ascii")
    header[16 : 16 + len(encoded_id)] = encoded_id
    for index, (offset, length) in enumerate(entries):
        struct.pack_into("<II", header, 48 + index * 8, offset, length)
    return bytes(header) + bytes(payload)


def build(root: Path) -> dict:
    source = root / "source"
    output = root / "pack"
    output.mkdir(parents=True, exist_ok=True)
    sprite_path = source / "sprites-transparent.png"
    terrain_path = source / "terrain-transparent.png"
    texture_path = source / "textures-v2-source.png"
    cover_path = source / "cover-source.png"
    if not texture_path.is_file():
        raise ValueError(
            "missing original texture sheet: textures-v2-source.png"
        )
    sprites = Image.open(sprite_path).convert("RGBA")
    terrain = Image.open(terrain_path).convert("RGBA")
    textures = texture_cells(Image.open(texture_path))

    cells: list[Image.Image] = []
    provenance: list[dict] = []
    catalog: list[dict] = []
    player_frames: list[dict] = []
    animations: dict[str, dict[str, tuple[int, int, int]]] = {}
    cell_keys: dict[bytes, int] = {}

    def add_cell(
        image: Image.Image,
        name: str,
        source_name: str,
        box=None,
        *,
        deduplicate: bool = False,
    ) -> int:
        image = image.convert("RGBA")
        key = image.tobytes()
        if deduplicate and key in cell_keys:
            return cell_keys[key]
        if len(cells) >= MAX_CELLS:
            raise ValueError("Neon Nook atlas exceeds the Maker Lite limit")
        index = len(cells)
        cells.append(image)
        cell_keys.setdefault(key, index)
        source_file = source / source_name
        row = {
            "index": index,
            "name": name,
            "extractor": "neon-nook-original-builder-v2",
            "source_file": source_name,
            "source_sha256": (
                sha256(source_file)
                if source_file.is_file()
                else "original-procedural-v1"
            ),
            "conversion": (
                "alpha-aware crop, Lanczos reduction, palette "
                "quantization, RGB565 pack"
            ),
        }
        if box:
            row["source_crop"] = list(box)
        provenance.append(row)
        return index

    poses = player_sources(sprites)
    action_pose_indexes = {
        "idle": (0,),
        "walk": (0, 1, 2),
        "run": (0, 1, 2),
        "jump": (1,),
        "fall": (2,),
        "crouch": (0,),
        "skid": (2,),
        "swim": (0, 1),
        "sword": (1,),
        "item": (0,),
        "roll": (0, 1, 2),
        "spindash": (1, 2),
        "hurt": (2,),
        "complete": (1,),
    }
    action_ticks = {
        "idle": 10,
        "walk": 7,
        "run": 4,
        "jump": 6,
        "fall": 6,
        "crouch": 8,
        "skid": 5,
        "swim": 8,
        "sword": 4,
        "item": 8,
        "roll": 4,
        "spindash": 3,
        "hurt": 8,
        "complete": 10,
    }
    for action in ACTION_NAMES:
        animations[action] = {}
        for direction in DIRECTION_NAMES:
            start = len(player_frames)
            for pose_index in action_pose_indexes[action]:
                frame = poses[direction][pose_index % len(poses[direction])]
                frame_cells = []
                for row in range(2):
                    for column in range(2):
                        crop = frame.crop(
                            (
                                column * 16,
                                row * 16,
                                (column + 1) * 16,
                                (row + 1) * 16,
                            )
                        )
                        frame_cells.append(
                            add_cell(
                                crop,
                                f"fox-{action}-{direction}-{pose_index}-{row}-{column}",
                                "sprites-transparent.png",
                                deduplicate=True,
                            )
                        )
                player_frames.append(
                    {
                        "columns": 2,
                        "rows": 2,
                        "offset_x": -16,
                        "offset_y": -31,
                        "cells": frame_cells,
                    }
                )
            animations[action][direction] = (
                start,
                len(player_frames) - start,
                action_ticks[action],
            )

    terrain_start = len(cells)
    walkable_kinds = (
        "wet-asphalt",
        "sidewalk",
        "grass",
        "garden-soil",
        "harbor-sand",
        "pier-wood",
        "canal-water",
        "interior",
        "roof",
        "tile-floor",
    )
    for kind in walkable_kinds:
        for variant in range(4):
            cell = add_cell(
                textures[kind][variant],
                f"{kind}-{variant + 1}",
                "textures-v2-source.png",
            )
            collision = ["water"] if kind == "canal-water" else []
            catalog.append(
                {
                    "id": f"{kind}-{variant + 1}",
                    "label": f"{kind.replace('-', ' ').title()} {variant + 1}",
                    "category": "Terrain",
                    "type": "terrain",
                    "cell": cell,
                    "collision": collision,
                }
            )
    for kind in ("wall", "fence"):
        for variant in range(4):
            cell = add_cell(
                textures[kind][variant],
                f"{kind}-{variant + 1}",
                "textures-v2-source.png",
            )
            catalog.append(
                {
                    "id": f"{kind}-{variant + 1}",
                    "label": f"{kind.title()} {variant + 1}",
                    "category": "Architecture",
                    "type": "terrain",
                    "cell": cell,
                    "collision": ["solid"],
                }
            )

    sprite_components = sheet_components(
        sprites,
        minimum_area=90,
        exclude=(0, 0, 528, 285),
    )
    terrain_components = sheet_components(terrain, minimum_area=140)
    component_rows = [
        ("sprites-transparent.png", sprite_components),
        ("terrain-transparent.png", terrain_components),
    ]
    category_counts: dict[str, int] = {}
    entity_cells: dict[str, int] = {}
    fallback_order = [
        "goal",
        "collectible",
        "enemy",
        "checkpoint",
        "key",
        "door",
        "switch",
        "block",
        "spring",
        "item",
        "pot",
    ]
    for source_name, components in component_rows:
        for box, crop in components:
            center_y = (box[1] + box[3]) // 2
            if source_name.startswith("sprites"):
                if center_y < 190:
                    category, kind = "Villagers", "npc"
                elif center_y < 340:
                    category, kind = "Helpers & Hazards", "enemy"
                elif center_y < 535:
                    category, kind = "Nature & Street", "block"
                elif center_y < 690:
                    category, kind = "Architecture", "door"
                elif center_y < 930:
                    category, kind = "Tools & Collectibles", "collectible"
                else:
                    category, kind = "Furniture & Decor", "furniture"
            else:
                if center_y < 300:
                    category, kind = "Terrain Source", "block"
                elif center_y < 620:
                    category, kind = "Architecture", "door"
                else:
                    category, kind = "Interiors", "item"
            category_counts[category] = category_counts.get(category, 0) + 1
            number = category_counts[category]
            asset_id = (
                category.lower().replace(" & ", "-").replace(" ", "-")
                + f"-{number:03d}"
            )
            label = f"{category} {number}"
            cell = add_cell(
                fitted_cell(crop),
                asset_id,
                source_name,
                box,
            )
            if len(entity_cells) < len(fallback_order):
                entity_cells[fallback_order[len(entity_cells)]] = cell
            catalog.append(
                {
                    "id": asset_id,
                    "label": label,
                    "category": category,
                    "type": "entity",
                    "cell": cell,
                    "kind": kind,
                    "params": (
                        [14, 14, 75, 0]
                        if kind == "furniture"
                        else [16, 16, 0, 0]
                    ),
                    "flags": 0,
                }
            )

    architecture = [
        asset
        for asset in catalog
        if asset["type"] == "entity"
        and asset["category"] == "Architecture"
    ]
    street = [
        asset
        for asset in catalog
        if asset["type"] == "entity"
        and asset["category"] == "Nature & Street"
    ]
    if len(architecture) < 13 or len(street) < 4:
        raise ValueError("Neon Nook needs complete house, shop, and car art")
    house_base = architecture[0]["cell"]
    car_base = street[0]["cell"]
    if [asset["cell"] for asset in architecture[:12]] != list(
        range(house_base, house_base + 12)
    ):
        raise ValueError("Neon Nook house variants must be contiguous")
    if [asset["cell"] for asset in street[:4]] != list(
        range(car_base, car_base + 4)
    ):
        raise ValueError("Neon Nook car upgrades must be contiguous")
    catalog.extend(
        [
            {
                "id": "player-house",
                "label": "Player House",
                "category": "Life Sim",
                "type": "entity",
                "cell": house_base,
                "kind": "house",
                "params": [32, 32, house_base, 12],
                "flags": 0,
            },
            {
                "id": "neon-nook-shop",
                "label": "Neon Nook Shop",
                "category": "Life Sim",
                "type": "entity",
                "cell": architecture[12]["cell"],
                "kind": "shop",
                "params": [32, 32, 0, 0],
                "flags": 0,
            },
            {
                "id": "city-car",
                "label": "City Car",
                "category": "Life Sim",
                "type": "entity",
                "cell": car_base,
                "kind": "car",
                "params": [24, 16, 0, 0],
                "flags": 0,
            },
        ]
    )

    if len(catalog) < 128:
        raise ValueError(f"asset extraction yielded only {len(catalog)} catalog parts")
    if not all(name in entity_cells for name in fallback_order):
        raise ValueError("asset extraction did not provide all runtime fallbacks")

    while len(cells) % ATLAS_COLUMNS:
        add_cell(
            Image.new("RGBA", (16, 16), (0, 0, 0, 0)),
            f"padding-{len(cells)}",
            "procedural-original",
        )
    atlas = Image.new(
        "RGBA",
        (
            ATLAS_COLUMNS * CELL_SIZE,
            (len(cells) // ATLAS_COLUMNS) * CELL_SIZE,
        ),
        (0, 0, 0, 0),
    )
    for index, cell in enumerate(cells):
        atlas.alpha_composite(
            cell,
            ((index % ATLAS_COLUMNS) * 16, (index // ATLAS_COLUMNS) * 16),
        )
    atlas.save(output / "atlas.png", optimize=False)

    art = encode_art(cells, player_frames, animations, entity_cells)
    (output / "art.mla").write_bytes(art)
    (output / "audio.mla").write_bytes(encode_audio())

    with Image.open(cover_path) as opened:
        cover = opened.convert("RGB")
    scale = max(576 / cover.width, 432 / cover.height)
    cover = cover.resize(
        (round(cover.width * scale), round(cover.height * scale)),
        Image.Resampling.LANCZOS,
    )
    cover = cover.crop(
        (
            (cover.width - 576) // 2,
            (cover.height - 432) // 2,
            (cover.width - 576) // 2 + 576,
            (cover.height - 432) // 2 + 432,
        )
    )
    cover.save(output / "source-cover.png", optimize=True)

    manifest = {
        "kit_id": KIT_ID,
        "ruleset": RULESET,
        "revision_id": "neon-nook-original-v2",
        "license": "Original generated artwork for this Rockpod repository",
        "redistribution": "May be redistributed with Rockpod/Maker Lite",
        "source": {
            "type": "original-generated",
            "cover_sha256": sha256(cover_path),
            "sprites_sha256": sha256(sprite_path),
            "terrain_sha256": sha256(terrain_path),
            "textures_sha256": sha256(texture_path),
            "prompt_family": "cozy cyberpunk animal village life game",
        },
        "conversion_operations": [
            "chroma-key removal",
            "alpha-aware Lanczos sprite reduction",
            "48-color per-sprite quantization",
            "original generated 4x3 material-atlas reduction",
            "RGB565 cell-major packing",
            "bounded v4 directional player-frame packing",
            "original synthesized signed 16-bit stereo effects",
        ],
        "cell_count": len(cells),
        "catalog_count": len(catalog),
        "player_frame_count": len(player_frames),
        "player_base": 0,
        "cells": provenance,
        "player_frames": player_frames,
        "entity_cells": entity_cells,
        "asset_catalog": catalog,
        "source_cover_file": "source-cover.png",
        "extractor": {
            "id": "rockpod-neon-nook-original",
            "version": 2,
            "terrain_start": terrain_start,
        },
    }
    (output / "kit.mlk").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (output / "provenance.tsv").write_text(
        "kit_id\truleset\trevision_id\tart_sha256\taudio_sha256\tcatalog_count\n"
        f"{KIT_ID}\t{RULESET}\tneon-nook-original-v2\t"
        f"{sha256(output / 'art.mla')}\t{sha256(output / 'audio.mla')}\t"
        f"{len(catalog)}\n",
        encoding="utf-8",
    )
    files = {
        name: sha256(output / name)
        for name in (
            "art.mla",
            "audio.mla",
            "atlas.png",
            "kit.mlk",
            "provenance.tsv",
            "source-cover.png",
        )
    }
    package = {
        "format": 1,
        "kit_id": KIT_ID,
        "ruleset": RULESET,
        "files": files,
        "cell_count": len(cells),
        "catalog_count": len(catalog),
        "player_frame_count": len(player_frames),
    }
    (output / "pack.json").write_text(
        json.dumps(package, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return package


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    args = parser.parse_args()
    package = build(args.root.resolve())
    print(
        "built {kit_id}: {cell_count} cells, {catalog_count} catalog parts, "
        "{player_frame_count} player frames".format(**package)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
