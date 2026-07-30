#!/usr/bin/env python3
"""Stage non-shipping diagnostic Maker Lite projects into an iPod simdisk."""

from __future__ import annotations

import argparse
import json
import shutil
import struct
import sys
import zlib
from pathlib import Path

from PIL import Image, ImageDraw


REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "rockpod"))

from services.maker_lite_pack import compile_project  # noqa: E402
from services.maker_lite_settings import compile_device_settings  # noqa: E402


RULESET_COLORS = {
    "mario": ((78, 143, 216), (66, 158, 73)),
    "zelda": ((52, 90, 54), (184, 145, 63)),
    "sonic": ((44, 116, 207), (214, 181, 51)),
}
ENTITY_CELLS = {
    2: 32, 3: 33, 4: 34, 5: 35, 6: 36, 7: 37,
    8: 38, 9: 39, 10: 40, 11: 41, 12: 42,
}


def diagnostic_catalog(ruleset):
    terrain_label = {
        "mario": "Diagnostic Mario Ground",
        "zelda": "Diagnostic Zelda Floor",
        "sonic": "Diagnostic Sonic Ground",
    }[ruleset]
    collectible_label = {
        "mario": "Diagnostic Coin",
        "zelda": "Diagnostic Pickup",
        "sonic": "Diagnostic Ring",
    }[ruleset]
    return [
        {
            "id": "diagnostic-ground",
            "label": terrain_label,
            "category": "Terrain",
            "type": "terrain",
            "cell": 20,
            "collision": ["solid"],
        },
        {
            "id": "diagnostic-enemy",
            "label": "Diagnostic Enemy",
            "category": "Enemies",
            "type": "entity",
            "cell": ENTITY_CELLS[4],
            "kind": "enemy",
            "params": [16, 16, 96, 0],
            "flags": 0,
        },
        {
            "id": "diagnostic-collectible",
            "label": collectible_label,
            "category": "Items",
            "type": "entity",
            "cell": ENTITY_CELLS[3],
            "kind": "collectible",
            "params": [12, 12, 100, 0],
            "flags": 0,
        },
    ]


def rgb565(color):
    red, green, blue = color
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def diagnostic_art(kit_id, ruleset):
    """Build unmistakable animated gate art, never a private kit replacement.

    Sonic uses MLAR v4 so every simulator qualification exercises the
    original-size metasprite loader and renderer used by the private Sonic 3
    extractor. Mario and Zelda retain v3 directional-cell coverage.
    """

    primary, secondary = RULESET_COLORS[ruleset]
    cells = 96
    payload = bytearray()
    for cell in range(cells):
        for y in range(16):
            for x in range(16):
                if cell >= 16 and (x in {0, 15} or y in {0, 15}):
                    color = (255, 0, 255)
                elif (x // 4 + y // 4 + cell) & 1:
                    color = primary
                else:
                    color = secondary
                payload.extend(struct.pack("<H", rgb565(color)))
    animations = bytearray()
    for action in range(14):
        for direction in range(4):
            mirror = direction == 2
            animations.extend(
                struct.pack(
                    "<HBB",
                    action,
                    1,
                    (1 + action % 4) | (0x80 if mirror else 0),
                )
            )
    frames = bytearray()
    version = 3
    if ruleset == "sonic":
        version = 4
        frames.extend(struct.pack("<HH", 14, 0))
        for action in range(14):
            frames.extend(
                struct.pack(
                    "<BBbb16H",
                    2,
                    2,
                    -16,
                    -32,
                    action,
                    action + 14,
                    action + 28,
                    action + 42,
                    *([0xFFFF] * 12),
                )
            )
    header = bytearray(64)
    header[:4] = b"MLAR"
    struct.pack_into(
        "<HHHHI",
        header,
        4,
        version,
        16,
        cells,
        0,
        zlib.crc32(payload + animations + frames) & 0xFFFFFFFF,
    )
    encoded = kit_id.encode("ascii")
    header[16:16 + len(encoded)] = encoded
    for kind, cell in ENTITY_CELLS.items():
        header[48 + kind] = cell
    return bytes(header) + bytes(payload) + bytes(animations) + bytes(frames)


def diagnostic_cover(path, title, ruleset):
    primary, secondary = RULESET_COLORS[ruleset]
    image = Image.new("RGB", (144, 108), primary)
    draw = ImageDraw.Draw(image)
    for y in range(0, 108, 12):
        draw.rectangle((0, y, 144, y + 5), fill=secondary)
    draw.rectangle((8, 34, 136, 74), fill=(20, 24, 30))
    draw.text((14, 44), "MAKER LITE SIM TEST", fill=(255, 255, 255))
    draw.text((14, 58), title[:22], fill=(230, 230, 230))
    image.save(path, format="BMP")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("simdisk", type=Path)
    parser.add_argument(
        "--plugin",
        type=Path,
        default=REPO / "build-sim-ipod6g/apps/plugins/maker_lite/maker_lite.rock",
    )
    args = parser.parse_args()
    root = args.simdisk.resolve()
    if not (root / ".rockbox").is_dir():
        raise SystemExit(f"not a Rockbox simdisk: {root}")
    plugin_destination = root / ".rockbox/rocks/games/maker_lite.rock"
    plugin_destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(args.plugin, plugin_destination)
    manifest_dir = root / ".rockbox/rocks/games/maker_lite"
    manifest_dir.mkdir(parents=True, exist_ok=True)
    rows = []
    settings_dir = root / ".rockbox/games/maker_lite/settings"
    settings_dir.mkdir(parents=True, exist_ok=True)
    for source_path in sorted((REPO / "testdata/maker_lite").glob("*-test.json")):
        source = json.loads(source_path.read_text(encoding="utf-8"))
        ruleset = source["ruleset"]
        project_id = source["project_id"]
        kit_id = source["kit_id"]
        project_dir = root / ".rockbox/games/maker_lite/projects" / project_id
        kit_dir = root / ".rockbox/games/maker_lite/kits" / kit_id
        project_dir.mkdir(parents=True, exist_ok=True)
        kit_dir.mkdir(parents=True, exist_ok=True)
        (project_dir / "game.mlp").write_bytes(compile_project(source))
        if ruleset == "zelda":
            controls = {"preset": "left_handed"}
        elif ruleset == "sonic":
            controls = {
                "preset": "custom",
                "mapping": {
                    "select": "primary",
                    "play": "secondary",
                    "previous": "next",
                    "next": "previous",
                },
            }
        else:
            controls = {"preset": "stock"}
        (settings_dir / f"{project_id}.mlc").write_bytes(
            compile_device_settings(project_id, controls)
        )
        (kit_dir / "art.mla").write_bytes(diagnostic_art(kit_id, ruleset))
        (kit_dir / "kit.mlk").write_text(
            json.dumps(
                {
                    "diagnostic_fixture": True,
                    "ruleset": ruleset,
                    "asset_catalog": diagnostic_catalog(ruleset),
                }
            )
            + "\n",
            encoding="utf-8",
        )
        (kit_dir / "provenance.tsv").write_text(
            "diagnostic_fixture\tcommercial_asset\ntrue\tfalse\n",
            encoding="utf-8",
        )
        cover = project_dir / "cover.144x108x24.bmp"
        diagnostic_cover(cover, ruleset.upper(), ruleset)
        rows.append(
            "\t".join(
                [
                    source["title"],
                    "/.rockbox/rocks/games/maker_lite.rock",
                    f"/.rockbox/games/maker_lite/projects/{project_id}/cover.144x108x24.bmp",
                    "0",
                    f"/.rockbox/games/maker_lite/saves/{project_id}.sav",
                    "0",
                    "Simulator diagnostic",
                    "Rockpod",
                    "Rockpod",
                    "Non-shipping simulator fixture; import a private authentic kit for play.",
                    f"/.rockbox/games/maker_lite/projects/{project_id}/game.mlp",
                ]
            )
        )
    (manifest_dir / "games.tsv").write_text(
        "\n".join(sorted(rows, key=str.casefold)) + "\n", encoding="utf-8"
    )
    browser_manifest = root / ".rockbox/games/maker_lite/projects.tsv"
    browser_manifest.parent.mkdir(parents=True, exist_ok=True)
    browser_manifest.write_text(
        "\n".join(sorted(rows, key=str.casefold)) + "\n", encoding="utf-8"
    )
    print(f"staged {len(rows)} Maker Lite simulator fixtures in {root}")


if __name__ == "__main__":
    main()
