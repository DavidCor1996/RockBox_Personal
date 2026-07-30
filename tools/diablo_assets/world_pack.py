"""Build an on-device "world" pack: everything needed to render an
arbitrary viewport around a moving player, without holding all of a
level's decoded art in RAM at once (it doesn't fit -- see the header
comment in world.c's loader for the arithmetic).

Two files are produced per level (town, or the level-1 dungeon):
  - <name>.meta: palette, the precomputed dPiece grid, the MIN/SOL
    tables (shipped close to their original compact form), and per-piece
    row bounds for render culling. All small; loaded into RAM whole.
  - <name>.cel.raw: <name>.cel fully decrypted and PKWare-exploded, byte
    for byte, with its frame offset table. Large (~1-2MB); stays on disk
    and is read one frame at a time on demand, cached on-device.

This module is tileset-agnostic: town.py assembles its grid by stitching
four fixed sector files, dungeon_gen.py builds one by hand-authoring a
fixed room graph, but from here on both are just a TileSet plus a
dpiece_w x dpiece_h array of piece indices -- see build().
"""
from __future__ import annotations

import struct

from dun_tile import TileSet

META_MAGIC = b"DWLD"


def _piece_row_bounds(tileset: TileSet, piece_index0based: int):
    """Return (min_block_row, max_block_row), 0..rows-1, of block rows
    that have any non-zero MIN entry -- lets the renderer skip scanning
    rows of a piece that are known to be entirely empty."""
    base = piece_index0based * tileset.blocks
    min_row, max_row = tileset.rows, -1
    for b in range(tileset.blocks):
        raw = struct.unpack_from("<H", tileset.min_data, (base + b) * 2)[0]
        if raw == 0:
            continue
        row = b // tileset.cols
        min_row = min(min_row, row)
        max_row = max(max_row, row)
    if max_row < 0:
        return (0, 0)
    return (min_row, max_row)


def build(tileset: TileSet, grid, palette: bytes, sol_data: bytes) -> tuple[bytes, bytes]:
    """grid is a dpiece_w x dpiece_h array (grid[x][y]) of 0-based piece
    indices, e.g. town.build_dpiece_grid()'s or dungeon_gen.py's output.
    Returns (meta_bytes, cel_raw_bytes)."""
    dpiece_w = len(grid)
    dpiece_h = len(grid[0])
    grid_bytes = bytearray(dpiece_w * dpiece_h * 2)
    for x in range(dpiece_w):
        for y in range(dpiece_h):
            struct.pack_into("<H", grid_bytes, (x * dpiece_h + y) * 2, grid[x][y])

    bounds_bytes = bytearray(tileset.num_pieces * 2)
    for p in range(tileset.num_pieces):
        lo, hi = _piece_row_bounds(tileset, p)
        bounds_bytes[p * 2] = lo
        bounds_bytes[p * 2 + 1] = hi

    assert len(sol_data) == tileset.num_pieces
    assert len(palette) == 768

    num_frames = len(tileset.cel)
    cel_offsets_bytes = b"".join(
        struct.pack("<I", tileset.cel.offsets[i]) for i in range(num_frames + 1)
    )

    header_size = 4 + 2 * 6 + 4 * 6
    palette_off = header_size
    grid_off = palette_off + len(palette)
    min_off = grid_off + len(grid_bytes)
    sol_off = min_off + len(tileset.min_data)
    bounds_off = sol_off + len(sol_data)
    cel_offsets_off = bounds_off + len(bounds_bytes)

    header = struct.pack(
        "<4sHHHHHHIIIIII",
        META_MAGIC,
        dpiece_w, dpiece_h,
        tileset.num_pieces,
        num_frames,
        tileset.blocks,
        0,  # reserved
        palette_off, grid_off, min_off, sol_off, bounds_off, cel_offsets_off,
    )
    meta = header + bytes(palette) + bytes(grid_bytes) + bytes(tileset.min_data) \
        + bytes(sol_data) + bytes(bounds_bytes) + cel_offsets_bytes

    cel_raw = tileset.cel.data
    return meta, cel_raw
