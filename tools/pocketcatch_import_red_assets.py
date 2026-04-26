#!/usr/bin/env python3
"""Generate PocketCatch Red tileset and map headers from local pret/pokered assets."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_IMPORT_DIR = Path("/tmp/pocketcatch_red_import")


TILESETS = [
    {
        "comment": "Forest assets.",
        "prefix": "pc_red_forest",
        "tile_macro": "PC_RED_FOREST_TILE_COUNT",
        "block_macro": "PC_RED_FOREST_BLOCK_COUNT",
        "png": "forest.png",
        "bst": "forest.bst",
    },
    {
        "comment": "Gate/Forest Gate assets.",
        "prefix": "pc_red_gate",
        "tile_macro": "PC_RED_GATE_TILE_COUNT",
        "block_macro": "PC_RED_GATE_BLOCK_COUNT",
        "png": "gate.png",
        "bst": "gate.bst",
    },
    {
        "comment": "Oak's Lab assets.",
        "prefix": "pc_red_lab",
        "tile_macro": "PC_RED_LAB_TILE_COUNT",
        "block_macro": "PC_RED_LAB_BLOCK_COUNT",
        "png": "lab.png",
        "bst": "lab.bst",
    },
    {
        "comment": "Red's House assets.",
        "prefix": "pc_red_reds_house",
        "tile_macro": "PC_RED_REDS_HOUSE_TILE_COUNT",
        "block_macro": "PC_RED_REDS_HOUSE_BLOCK_COUNT",
        "png": "reds_house.png",
        "bst": "reds_house.bst",
    },
]


MAPS = [
    ("pc_reds_house_1f_blocks", "RedsHouse1F.blk", 4, 4),
    ("pc_reds_house_2f_blocks", "RedsHouse2F.blk", 4, 4),
    ("pc_oaks_lab_blocks", "OaksLab.blk", 6, 5),
    ("pc_viridian_mart_blocks", "ViridianMart.blk", 4, 4),
    ("pc_blues_house_blocks", "BluesHouse.blk", 4, 4),
    ("pc_viridian_pokecenter_blocks", "ViridianPokecenter.blk", 4, 7),
    ("pc_viridian_school_house_blocks", "ViridianSchoolHouse.blk", 4, 4),
    ("pc_viridian_nickname_house_blocks", "ViridianNicknameHouse.blk", 4, 4),
    ("pc_viridian_gym_blocks", "ViridianGym.blk", 9, 10),
    ("pc_route2_gate_blocks", "Route2Gate.blk", 4, 5),
    ("pc_route2_trade_house_blocks", "Route2TradeHouse.blk", 4, 4),
    ("pc_forest_south_gate_blocks", "ViridianForestSouthGate.blk", 4, 5),
    ("pc_forest_north_gate_blocks", "ViridianForestNorthGate.blk", 4, 5),
    ("pc_viridian_forest_blocks", "ViridianForest.blk", 24, 17),
    ("pc_pewter_blocks", "PewterCity.blk", 18, 20),
    ("pc_pewter_gym_blocks", "PewterGym.blk", 7, 5),
    ("pc_pewter_mart_blocks", "PewterMart.blk", 4, 4),
    ("pc_pewter_nidoran_house_blocks", "PewterNidoranHouse.blk", 4, 4),
    ("pc_pewter_speech_house_blocks", "PewterSpeechHouse.blk", 4, 4),
    ("pc_pewter_pokecenter_blocks", "PewterPokecenter.blk", 4, 7),
]


def image_to_tiles(path: Path) -> list[list[list[int]]]:
    try:
        cmd = ["magick", str(path), "txt:-"]
        output = subprocess.check_output(cmd, text=True, stderr=subprocess.DEVNULL)
    except (FileNotFoundError, subprocess.CalledProcessError):
        cmd = ["convert", str(path), "txt:-"]
        output = subprocess.check_output(cmd, text=True)

    pixels: list[list[int]] = []
    for line in output.splitlines():
        if not line or line.startswith("#") or ":" not in line:
            continue
        coord, rest = line.split(":", 1)
        x_str, y_str = coord.split(",")
        x = int(x_str)
        y = int(y_str)
        if y >= len(pixels):
            pixels.append([])
        gray_start = rest.find("gray(")
        if gray_start == -1:
            raise ValueError(f"Unexpected pixel format in {path}")
        gray_end = rest.find(")", gray_start)
        gray = int(rest[gray_start + 5 : gray_end])
        if gray % 85 != 0:
            raise ValueError(f"Unexpected grayscale value {gray} in {path}")
        while len(pixels[y]) < x:
            pixels[y].append(0)
        pixels[y].append(gray // 85)

    width = len(pixels[0])
    height = len(pixels)
    if width % 8 or height % 8:
        raise ValueError(f"Unexpected image size {width}x{height} in {path}")

    tiles: list[list[list[int]]] = []
    for tile_y in range(0, height, 8):
        for tile_x in range(0, width, 8):
            tile: list[list[int]] = []
            for y in range(tile_y, tile_y + 8):
                tile.append(pixels[y][tile_x : tile_x + 8])
            tiles.append(tile)
    return tiles


def bst_to_blocks(path: Path) -> list[list[list[int]]]:
    raw = path.read_bytes()
    if len(raw) % 16:
        raise ValueError(f"Unexpected blockset size {len(raw)} in {path}")
    blocks: list[list[list[int]]] = []
    for i in range(0, len(raw), 16):
        chunk = raw[i : i + 16]
        block = [list(chunk[row * 4 : row * 4 + 4]) for row in range(4)]
        blocks.append(block)
    return blocks


def blk_to_rows(path: Path, rows: int, cols: int) -> list[list[int]]:
    raw = path.read_bytes()
    if len(raw) != rows * cols:
        raise ValueError(
            f"Unexpected map size {len(raw)} in {path}, expected {rows * cols}"
        )
    return [list(raw[row * cols : row * cols + cols]) for row in range(rows)]


def fmt_tile(tile: list[list[int]], indent: str = "    ") -> str:
    lines = [indent + "{"]
    for row in tile:
        row_text = ", ".join(str(v) for v in row)
        lines.append(indent + "    {" + row_text + "},")
    lines.append(indent + "},")
    return "\n".join(lines)


def fmt_grid(grid: list[list[int]], indent: str = "    ") -> str:
    lines = [indent + "{"]
    for row in grid:
        row_text = ", ".join(f"0x{value:02x}" for value in row)
        lines.append(indent + "    {" + row_text + "},")
    lines.append(indent + "},")
    return "\n".join(lines)


def fmt_row(row: list[int], indent: str = "    ") -> str:
    row_text = ", ".join(f"0x{value:02x}" for value in row)
    return indent + "{" + row_text + "},"


def generate_gfx_header(import_dir: Path, out_path: Path) -> None:
    lines = [
        "/* Auto-generated from pret/pokered source assets. */",
        "#ifndef PC_RED_EXTRA_GFX_H",
        "#define PC_RED_EXTRA_GFX_H",
        "",
    ]

    for spec in TILESETS:
        tiles = image_to_tiles(import_dir / spec["png"])
        blocks = bst_to_blocks(import_dir / spec["bst"])
        lines.append(f"/* {spec['comment']} */")
        lines.append(f"#define {spec['tile_macro']} {len(tiles)}")
        lines.append(f"#define {spec['block_macro']} {len(blocks)}")
        lines.append("")
        lines.append(
            f"static const unsigned char {spec['prefix']}_tiles[{len(tiles)}][8][8] = {{"
        )
        lines.extend(fmt_tile(tile) for tile in tiles)
        lines.append("};")
        lines.append("")
        lines.append(
            f"static const unsigned char {spec['prefix']}_blocks[{len(blocks)}][4][4] = {{"
        )
        lines.extend(fmt_grid(block) for block in blocks)
        lines.append("};")
        lines.append("")

    lines.append("#endif")
    lines.append("")
    out_path.write_text("\n".join(lines))


def generate_maps_header(import_dir: Path, out_path: Path) -> None:
    lines = [
        "/* Auto-generated from pret/pokered map block files. */",
        "#ifndef PC_RED_MAPS_H",
        "#define PC_RED_MAPS_H",
        "",
    ]

    for name, filename, rows, cols in MAPS:
        grid = blk_to_rows(import_dir / filename, rows, cols)
        lines.append(
            f"static const unsigned char {name}[{rows}][{cols}] = {{"
        )
        lines.extend(fmt_row(row, indent="    ") for row in grid)
        lines.append("};")
        lines.append("")

    lines.append("#endif")
    lines.append("")
    out_path.write_text("\n".join(lines))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--import-dir",
        type=Path,
        default=DEFAULT_IMPORT_DIR,
        help="Directory containing downloaded pret/pokered png/bst/blk files.",
    )
    parser.add_argument(
        "--gfx-out",
        type=Path,
        default=ROOT / "apps/plugins/pocketcatch/pc_red_extra_gfx.h",
    )
    parser.add_argument(
        "--maps-out",
        type=Path,
        default=ROOT / "apps/plugins/pocketcatch/pc_red_maps.h",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    generate_gfx_header(args.import_dir, args.gfx_out)
    generate_maps_header(args.import_dir, args.maps_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
