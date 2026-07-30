"""Gameplay-first Neon Nook apartment and open-city construction."""

from __future__ import annotations

import json
import os

from services.maker_lite_assets import BUNDLED_NEON_NOOK_KIT_ID


CITY_SIZE = [80, 56]
APARTMENT_BOUNDS = [0, 0, 16, 14]


def _entity(asset: dict, kind: str, x: int, y: int, **overrides) -> dict:
    result = {
        "kind": kind,
        "x": x,
        "y": y,
        "params": list(asset.get("params", [16, 16, 0, 0])),
        "flags": int(asset.get("flags", 0)),
        "render_cell": int(asset["cell"]),
        "asset_id": asset["id"],
    }
    result.update(overrides)
    return result


def create_neon_nook_city(store, kit):
    """Create or upgrade the persistent gameplay-first Neon Nook city."""

    with open(
        os.path.join(kit.directory, "kit.mlk"),
        "r",
        encoding="utf-8",
    ) as source:
        manifest = json.load(source)
    existing = next(
        (
            record
            for record in store.list()
            if record.source.get("kit_id") == BUNDLED_NEON_NOOK_KIT_ID
        ),
        None,
    )
    if (
        existing is not None
        and existing.source.get("gameplay") == "life_sim"
        and existing.source.get("size") == CITY_SIZE
        and existing.source.get("city_revision") == 2
    ):
        return existing, False, manifest

    catalog = {
        asset["id"]: asset for asset in manifest.get("asset_catalog", [])
    }

    def required(asset_id):
        try:
            return catalog[asset_id]
        except KeyError as error:
            raise ValueError(
                f"Neon Nook kit is missing required part: {asset_id}"
            ) from error

    def assets(category, *, kind=None):
        return [
            asset
            for asset in manifest.get("asset_catalog", [])
            if asset.get("category") == category
            and (kind is None or asset.get("kind") == kind)
        ]

    grass = required("grass-1")
    grass_alt = required("grass-3")
    asphalt = required("wet-asphalt-1")
    asphalt_alt = required("wet-asphalt-3")
    sidewalk = required("sidewalk-1")
    sidewalk_alt = required("sidewalk-3")
    water = required("canal-water-1")
    pier = required("pier-wood-1")
    interior = required("interior-1")
    kitchen = required("tile-floor-2")
    roof = required("roof-1")
    roof_alt = required("roof-3")
    wall = required("wall-1")
    fence = required("fence-1")
    house = required("player-house")
    shop = required("neon-nook-shop")
    car = required("city-car")
    villagers = assets("Villagers", kind="npc")
    furniture = assets("Furniture & Decor", kind="furniture")
    salvage_parts = assets("Tools & Collectibles", kind="collectible")
    door_parts = assets("Architecture", kind="door")
    decor_parts = assets("Nature & Street") + assets("Architecture")
    if len(villagers) < 8:
        raise ValueError("Neon Nook kit needs at least eight resident sprites")
    if len(furniture) < 10:
        raise ValueError("Neon Nook kit needs ten apartment furnishings")
    if not salvage_parts or not door_parts or len(decor_parts) < 16:
        raise ValueError(
            "Neon Nook kit needs collectible, doorway, and street art"
        )
    salvage = salvage_parts[0]
    doorway = door_parts[-1]

    record = existing or store.create(
        "zelda", BUNDLED_NEON_NOOK_KIT_ID, "Neon Nook"
    )

    terrain = [
        {"rect": [0, 0, 80, 56], "tile": grass["cell"], "collision": []},
        {"rect": [16, 13, 64, 6], "tile": asphalt["cell"], "collision": []},
        {"rect": [16, 35, 64, 6], "tile": asphalt_alt["cell"], "collision": []},
        {"rect": [29, 0, 6, 56], "tile": asphalt["cell"], "collision": []},
        {"rect": [53, 0, 6, 56], "tile": asphalt_alt["cell"], "collision": []},
        {"rect": [16, 12, 64, 1], "tile": sidewalk["cell"], "collision": []},
        {"rect": [16, 19, 64, 1], "tile": sidewalk_alt["cell"], "collision": []},
        {"rect": [16, 34, 64, 1], "tile": sidewalk["cell"], "collision": []},
        {"rect": [16, 41, 64, 1], "tile": sidewalk_alt["cell"], "collision": []},
        {"rect": [28, 0, 1, 56], "tile": sidewalk["cell"], "collision": []},
        {"rect": [35, 0, 1, 56], "tile": sidewalk_alt["cell"], "collision": []},
        {"rect": [52, 0, 1, 56], "tile": sidewalk["cell"], "collision": []},
        {"rect": [59, 0, 1, 56], "tile": sidewalk_alt["cell"], "collision": []},
        {"rect": [68, 0, 4, 56], "tile": water["cell"], "collision": ["water"]},
        {"rect": [68, 14, 4, 4], "tile": pier["cell"], "collision": []},
        {"rect": [68, 36, 4, 4], "tile": pier["cell"], "collision": []},
        {"rect": [60, 20, 8, 14], "tile": grass_alt["cell"], "collision": []},
        {"rect": [72, 20, 8, 14], "tile": grass_alt["cell"], "collision": []},
        {"rect": [36, 20, 16, 14], "tile": kitchen["cell"], "collision": []},
        {"rect": [0, 0, 16, 14], "tile": interior["cell"], "collision": []},
        {"rect": [9, 1, 6, 5], "tile": kitchen["cell"], "collision": []},
        {"rect": [0, 0, 16, 1], "tile": wall["cell"], "collision": ["solid"]},
        {"rect": [0, 13, 16, 1], "tile": wall["cell"], "collision": ["solid"]},
        {"rect": [0, 0, 1, 14], "tile": wall["cell"], "collision": ["solid"]},
        {"rect": [15, 0, 1, 56], "tile": wall["cell"], "collision": ["solid"]},
    ]

    buildings = (
        (18, 2, 8, 6, roof),
        (39, 2, 10, 6, roof_alt),
        (61, 2, 7, 8, roof),
        (72, 2, 7, 8, roof_alt),
        (18, 22, 9, 9, roof_alt),
        (38, 23, 11, 8, roof),
        (18, 44, 9, 10, roof),
        (37, 44, 12, 10, roof_alt),
        (61, 44, 7, 10, roof),
        (72, 44, 7, 10, roof_alt),
    )
    for x, y, width, height, material in buildings:
        terrain.extend(
            [
                {
                    "rect": [x, y, width, height],
                    "tile": wall["cell"],
                    "collision": ["solid"],
                },
                {
                    "rect": [x + 1, y + 1, width - 2, height - 2],
                    "tile": material["cell"],
                    "collision": ["solid"],
                },
            ]
        )
    terrain.extend(
        [
            {"rect": [60, 20, 1, 14], "tile": fence["cell"], "collision": ["solid"]},
            {"rect": [67, 20, 1, 14], "tile": fence["cell"], "collision": ["solid"]},
            {"rect": [72, 20, 1, 14], "tile": fence["cell"], "collision": ["solid"]},
            {"rect": [79, 20, 1, 14], "tile": fence["cell"], "collision": ["solid"]},
            {"rect": [16, 0, 64, 1], "tile": wall["cell"], "collision": ["solid"]},
            {"rect": [16, 55, 64, 1], "tile": wall["cell"], "collision": ["solid"]},
            {"rect": [79, 0, 1, 56], "tile": wall["cell"], "collision": ["solid"]},
        ]
    )

    entities = [
        {"kind": "player", "x": 8 * 16, "y": 10 * 16},
        _entity(
            doorway,
            "door",
            8 * 16,
            12 * 16,
            params=[14, 14, 21 * 16, 10 * 16],
        ),
        _entity(
            doorway,
            "door",
            21 * 16,
            8 * 16,
            params=[14, 14, 8 * 16, 11 * 16],
        ),
        _entity(house, "house", 25 * 16, 9 * 16),
        _entity(shop, "shop", 44 * 16, 9 * 16),
        _entity(car, "car", 38 * 16, 16 * 16),
    ]

    furniture_positions = (
        (3, 3),
        (5, 3),
        (11, 3),
        (13, 3),
        (3, 7),
        (6, 7),
        (10, 7),
        (13, 7),
        (3, 10),
        (12, 10),
    )
    for index, (tile_x, tile_y) in enumerate(furniture_positions):
        entities.append(
            _entity(
                furniture[index],
                "furniture",
                tile_x * 16,
                tile_y * 16,
                params=[14, 14, 55 + index * 10, 0],
            )
        )

    decor_positions = (
        (17, 10), (24, 10), (28, 9), (36, 10),
        (51, 10), (60, 10), (66, 10), (73, 10), (78, 10),
        (17, 21), (26, 21), (36, 21), (51, 21),
        (60, 21), (66, 21), (73, 21), (78, 21),
        (17, 32), (26, 32), (36, 32), (51, 32),
        (60, 32), (66, 32), (73, 32), (78, 32),
        (17, 43), (26, 43), (36, 43), (51, 43),
        (60, 43), (66, 43), (73, 43), (78, 43),
    )
    entities.extend(
        _entity(
            decor_parts[index % len(decor_parts)],
            "decoration",
            tile_x * 16,
            tile_y * 16,
            params=[14, 16, 0, 0],
        )
        for index, (tile_x, tile_y) in enumerate(decor_positions)
    )

    resident_positions = (
        (17, 9), (27, 10), (30, 5), (31, 9),
        (34, 9), (37, 11), (48, 10), (52, 9),
        (56, 9), (59, 11), (63, 12), (65, 11),
        (72, 11), (75, 11), (78, 12),
        (20, 16), (25, 17), (32, 16), (44, 17),
        (47, 16), (50, 17), (56, 16), (61, 17),
        (65, 17), (73, 17), (77, 16),
        (30, 25), (36, 21), (36, 31), (40, 21),
        (46, 21), (50, 31), (55, 30), (62, 25),
        (66, 30), (74, 25), (77, 31),
        (20, 38), (25, 38), (32, 38), (38, 38),
        (45, 38), (50, 38), (56, 38), (61, 38),
        (66, 38), (73, 38), (77, 38),
        (30, 48), (33, 53), (54, 46), (57, 52),
    )
    entities.extend(
        _entity(
            villagers[index % len(villagers)],
            "npc",
            tile_x * 16,
            tile_y * 16,
            params=[14, 16, 48 + (index % 8) * 8, 0],
            flags=index & 3,
        )
        for index, (tile_x, tile_y) in enumerate(resident_positions)
    )

    salvage_positions = (
        (17, 11), (23, 11), (28, 11), (36, 11), (42, 11), (51, 11),
        (60, 11), (67, 11), (73, 11), (78, 11),
        (17, 20), (24, 20), (28, 20), (36, 20), (43, 20), (51, 20),
        (60, 19), (66, 19), (73, 19), (78, 19),
        (28, 5), (35, 5), (52, 5), (59, 5),
        (28, 27), (35, 27), (52, 27), (59, 27),
        (17, 33), (24, 33), (28, 33), (36, 33), (43, 33), (51, 33),
        (60, 33), (66, 33), (73, 33), (78, 33),
        (17, 42), (25, 42), (28, 42), (36, 42), (44, 42), (51, 42),
        (60, 42), (67, 42), (73, 42), (78, 42),
    )
    entities.extend(
        _entity(
            salvage,
            "collectible",
            tile_x * 16,
            tile_y * 16,
            params=[12, 12, 10, 0],
        )
        for tile_x, tile_y in salvage_positions
    )

    record.source.update(
        {
            "gameplay": "life_sim",
            "city_revision": 2,
            "title": "Neon Nook",
            "size": CITY_SIZE,
            "view": [256, 224],
            "apartment": {"bounds": APARTMENT_BOUNDS, "owned": True},
            "terrain": terrain,
            "entities": entities,
            "events": [],
            "paths": [],
            "rooms": [],
            "room_links": [],
        }
    )
    record.source["metadata"].update(
        {
            "show_in_steam": True,
            "cover_source": os.path.join(kit.directory, "source-cover.png"),
            "cover_fit": "crop",
            "cover_background": "#07162b",
            "genre": "Cozy cyberpunk life sim",
            "author": "Rockpod",
            "description": (
                "Arrange and sell apartment furniture, repay upgrade debt, "
                "trade salvage, upgrade and drive a car, work city shifts, "
                "and meet roaming residents across a neon harbor."
            ),
        }
    )
    store.save(record)
    return record, True, manifest
