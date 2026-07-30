#!/usr/bin/env python3
"""Generate Diablo plugin build assets from a user-owned DIABDAT.MPQ.

This never modifies, commits, or bundles Blizzard's game data anywhere
in the repository -- it reads your own MPQ and writes a small derived
pack file on disk, next to the Rockbox game data folders, exactly like
tools/sm64_prepare_assets.py does for a user-owned N64 ROM and
tools/openlara_extract_psx.py does for a user-owned Tomb Raider disc.

Current scope (see apps/plugins/diablo/UPSTREAM.md): extracts the title
screen and a static crop of Tristram town, as a vertical-slice proof that
MPQ parsing, decryption, PKWare DCL decompression, and the MIN/TIL/DUN
level-tile format are correct end to end. There is no interactivity yet
-- no movement, monsters, inventory, or save/load.

Usage:
    tools/diablo_prepare_assets.py [mpq] [--out DIR] [--selftest]
"""
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "diablo_assets"))

import blast_build  # noqa: E402
import pcx  # noqa: E402
import town  # noqa: E402
import level1  # noqa: E402
import dungeon_gen  # noqa: E402
import world_pack  # noqa: E402
import spawn_pack  # noqa: E402
from mpq_reader import MpqArchive  # noqa: E402

DEFAULT_MPQ = Path("/home/david/Downloads/DIABDAT.MPQ")
PACK_MAGIC = b"DPK1"

# Picked by rendering the full town and eyeballing candidate crops: an open
# plaza with a torch-lit glow and a couple of building corners in frame.
DEFAULT_TOWN_CENTER = (2750, 2150)


def sha1(path: Path) -> str:
    digest = hashlib.sha1()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def downscale_2x(pixels: bytes, width: int, height: int) -> tuple[bytes, int, int]:
    """Nearest-neighbour halve: Diablo's 640x480 art onto a 320x240 LCD."""
    out_w, out_h = width // 2, height // 2
    out = bytearray(out_w * out_h)
    for y in range(out_h):
        src_row = (y * 2) * width
        dst_row = y * out_w
        for x in range(out_w):
            out[dst_row + x] = pixels[src_row + x * 2]
    return bytes(out), out_w, out_h


def extract_title_screen(mpq_path: Path) -> pcx.PcxImage:
    archive = MpqArchive(mpq_path, explode=blast_build.explode)
    try:
        raw = archive.read("ui_art\\title.pcx")
    finally:
        archive.close()
    return pcx.decode(raw)


def extract_town_crop(mpq_path: Path, center: tuple[int, int], out_w: int, out_h: int):
    """Returns (palette, pixels) for an out_w x out_h town screen crop."""
    archive = MpqArchive(mpq_path, explode=blast_build.explode)
    try:
        palette = archive.read("levels\\towndata\\town.pal")
        _tileset, pixels, width, height = town.render_full_town(archive)
    finally:
        archive.close()
    cropped = town.crop(pixels, width, height, center[0], center[1], out_w, out_h)
    return palette, cropped


def write_pack(out_path: Path, width: int, height: int, palette: bytes, pixels: bytes):
    assert len(palette) == 768
    assert len(pixels) == width * height
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("wb") as f:
        f.write(PACK_MAGIC)
        f.write(width.to_bytes(2, "little"))
        f.write(height.to_bytes(2, "little"))
        f.write(palette)
        f.write(pixels)


def run_selftest(mpq_path: Path) -> None:
    print("[selftest] blast() known-vector check ...", end=" ")
    blast_build.selftest()
    print("ok")

    print("[selftest] opening MPQ and reading tables ...", end=" ")
    archive = MpqArchive(mpq_path, explode=blast_build.explode)
    print(f"ok ({archive.hash_table_entries} hash / {archive.block_table_entries} block entries)")

    print("[selftest] extracting levels\\towndata\\town.pal ...", end=" ")
    palette = archive.read("levels\\towndata\\town.pal")
    assert len(palette) == 768, f"unexpected palette size {len(palette)}"
    assert tuple(palette[0:3]) == (0, 0, 0), "palette index 0 should be black"
    print("ok (768 bytes, index 0 = black)")
    archive.close()

    print("[selftest] extracting + decoding ui_art\\title.pcx ...", end=" ")
    image = extract_title_screen(mpq_path)
    assert (image.width, image.height) == (640, 480), image
    assert len(image.pixels) == image.width * image.height
    print(f"ok ({image.width}x{image.height})")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mpq", nargs="?", type=Path, default=DEFAULT_MPQ)
    parser.add_argument(
        "--games-dir",
        type=Path,
        default=Path("build-sim-ipod6g/simdisk/.rockbox/rocks/games/diablo"),
        help="output directory (default: simulator games folder)",
    )
    parser.add_argument(
        "--skip-world",
        action="store_true",
        help="only regenerate title.dpk, skip the town.meta/town.cel.raw world pack",
    )
    parser.add_argument(
        "--selftest",
        action="store_true",
        help="verify decrypt/decompress pipeline against known-good values, then exit",
    )
    args = parser.parse_args()

    if not args.mpq.exists():
        print(f"error: {args.mpq} not found -- pass your own DIABDAT.MPQ path", file=sys.stderr)
        return 1

    print(f"MPQ: {args.mpq} (sha1 {sha1(args.mpq)})")

    if args.selftest:
        run_selftest(args.mpq)
        print("selftest passed")
        return 0

    image = extract_title_screen(args.mpq)
    pixels, w, h = downscale_2x(image.pixels, image.width, image.height)
    title_path = args.games_dir / "title.dpk"
    write_pack(title_path, w, h, image.palette, pixels)
    print(f"wrote {title_path} ({w}x{h}, {len(image.palette) + len(pixels) + 8} bytes)")

    if not args.skip_world:
        archive = MpqArchive(args.mpq, explode=blast_build.explode)
        try:
            print("building town world pack (dPiece grid + MIN/SOL tables, on-device renders it)...")
            town_tileset = town.load_town_tileset(archive)
            town_palette = archive.read("levels\\towndata\\town.pal")
            town_sol = archive.read("levels\\towndata\\town.sol")
            town_grid = town.build_dpiece_grid(archive, town_tileset)
            meta, cel_raw = world_pack.build(town_tileset, town_grid, town_palette, town_sol)
            _write_world(args.games_dir, "town", meta, cel_raw)

            town_start = (50, 50)
            town_stairs = _find_passable(town_grid, town_sol, town_start[0] + 4, town_start[1])
            town_spawns = spawn_pack.build(town_start, town_stairs, monsters=[], items=[])
            _write_bytes(args.games_dir / "town.spawns", town_spawns)

            print("building level-1 dungeon world pack (hand-authored rooms, see dungeon_gen.py)...")
            l1_tileset = level1.load_l1_tileset(archive)
            l1_palette = archive.read("levels\\l1data\\l1.pal")
            l1_sol = archive.read("levels\\l1data\\l1.sol")
            l1_grid = level1.build_dpiece_grid(l1_tileset)
            meta, cel_raw = world_pack.build(l1_tileset, l1_grid, l1_palette, l1_sol)
            _write_world(args.games_dir, "l1", meta, cel_raw)

            l1_spawns = spawn_pack.build(
                dungeon_gen.start_dpiece(), dungeon_gen.stairs_dpiece(),
                monsters=dungeon_gen.MONSTER_SPAWNS, items=dungeon_gen.ITEM_SPAWNS,
            )
            _write_bytes(args.games_dir / "l1.spawns", l1_spawns)
        finally:
            archive.close()

    return 0


def _write_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    print(f"wrote {path} ({len(data)} bytes)")


def _write_world(games_dir: Path, name: str, meta: bytes, cel_raw: bytes) -> None:
    _write_bytes(games_dir / f"{name}.meta", meta)
    _write_bytes(games_dir / f"{name}.cel.raw", cel_raw)


def _find_passable(grid, sol_data, cx: int, cy: int, max_radius: int = 20):
    """Spiral outward from (cx, cy) for the nearest in-bounds dPiece cell
    whose piece has no SOL_SOLID bit set. Used to place the town-side
    dungeon entrance a few cells from the player start without hardcoding
    a coordinate that might land inside a wall."""
    w, h = len(grid), len(grid[0])

    def passable(x, y):
        if x < 0 or x >= w or y < 0 or y >= h:
            return False
        return not (sol_data[grid[x][y]] & 1)

    if passable(cx, cy):
        return (cx, cy)
    for r in range(1, max_radius):
        for dx in range(-r, r + 1):
            for dy in (-r, r):
                if passable(cx + dx, cy + dy):
                    return (cx + dx, cy + dy)
        for dy in range(-r + 1, r):
            for dx in (-r, r):
                if passable(cx + dx, cy + dy):
                    return (cx + dx, cy + dy)
    raise RuntimeError("no passable cell found near start")


if __name__ == "__main__":
    raise SystemExit(main())
