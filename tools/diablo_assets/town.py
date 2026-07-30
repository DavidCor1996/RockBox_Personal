"""Assemble a static crop of Diablo's Tristram town map from DIABDAT.MPQ.

Town is stitched from four independent .dun sector files placed at fixed
offsets in a shared dPiece grid (see levels/town.cpp's FillSector in the
DevilutionX source, used here only as a format reference -- see
dun_tile.py's module docstring for why the original CEL tile encoding
needed to be reverse-verified rather than assumed). This module composites
the whole map at native pixel scale, in isometric (dx-dy, dx+dy) painter's-
algorithm order, and returns a crop suitable for a DPK1 pack.
"""
from __future__ import annotations

from mpq_reader import MpqArchive
from dun_tile import TileSet, dun_tile_layer

TOWN_BLOCKS_PER_PIECE = 16
DEFAULT_PIECE = 218 - 1  # 0-based; the ground-fill piece for empty DUN cells
DPIECE_GRID = 96  # generous bound; town sectors are placed at (0,0)..(46+25,46+25)
STEP_X, STEP_Y = 32, 16  # half-tile screen step per dPiece grid cell

_SECTORS = (
    ("levels\\towndata\\sector1s.dun", 46, 46),
    ("levels\\towndata\\sector2s.dun", 46, 0),
    ("levels\\towndata\\sector3s.dun", 0, 46),
    ("levels\\towndata\\sector4s.dun", 0, 0),
)


def load_town_tileset(archive: MpqArchive) -> TileSet:
    return TileSet(
        cel_data=archive.read("levels\\towndata\\town.cel"),
        min_data=archive.read("levels\\towndata\\town.min"),
        til_data=archive.read("levels\\towndata\\town.til"),
        blocks_per_piece=TOWN_BLOCKS_PER_PIECE,
    )


def build_dpiece_grid(archive: MpqArchive, tileset: TileSet):
    grid = [[DEFAULT_PIECE] * DPIECE_GRID for _ in range(DPIECE_GRID)]

    def fill_sector(path: str, xi: int, yy: int):
        width, height, tiles = dun_tile_layer(archive.read(path))
        for j in range(height):
            xx = xi
            for i in range(width):
                til_value = tiles[j * width + i]
                if til_value:
                    quadrant = tileset.megatile_pieces(til_value - 1)
                    v1, v2, v3, v4 = (p if p is not None else DEFAULT_PIECE for p in quadrant)
                else:
                    v1 = v2 = v3 = v4 = DEFAULT_PIECE
                grid[xx + 0][yy + 0] = v1
                grid[xx + 1][yy + 0] = v2
                grid[xx + 0][yy + 1] = v3
                grid[xx + 1][yy + 1] = v4
                xx += 2
            yy += 2

    for path, xi, yy in _SECTORS:
        fill_sector(path, xi, yy)
    return grid


def render_region(tileset: TileSet, grid, rx0: int, rx1: int, ry0: int, ry1: int):
    """Composite dPiece cells [rx0,rx1) x [ry0,ry1) back-to-front.

    Returns (pixels, width, height) as a flat row-major palette-index
    buffer sized to exactly cover the projected bounds of that region.
    """
    piece_w, piece_h = tileset.piece_w, tileset.piece_h

    cells = sorted(
        (
            (dpx + dpy, dpx, dpy, grid[dpx][dpy])
            for dpx in range(rx0, rx1)
            for dpy in range(ry0, ry1)
        ),
        key=lambda c: c[0],
    )

    min_sx = min((dpx - dpy) * STEP_X for dpx in range(rx0, rx1) for dpy in range(ry0, ry1))
    max_sx = max((dpx - dpy) * STEP_X for dpx in range(rx0, rx1) for dpy in range(ry0, ry1)) + piece_w
    min_sy = min((dpx + dpy) * STEP_Y for dpx in range(rx0, rx1) for dpy in range(ry0, ry1)) - piece_h + 32
    max_sy = max((dpx + dpy) * STEP_Y for dpx in range(rx0, rx1) for dpy in range(ry0, ry1)) + 32
    width, height = max_sx - min_sx, max_sy - min_sy

    pixels = bytearray(width * height)  # index 0 is town.pal's black/void entry
    for _, dpx, dpy, piece in cells:
        idx, mask = tileset.decode_piece(piece)
        sx = (dpx - dpy) * STEP_X - min_sx
        sy = (dpx + dpy) * STEP_Y - piece_h + 32 - min_sy
        for y in range(piece_h):
            py = sy + y
            if py < 0 or py >= height:
                continue
            row_base = y * piece_w
            dst_row = py * width
            for x in range(piece_w):
                if not mask[row_base + x]:
                    continue
                px = sx + x
                if 0 <= px < width:
                    pixels[dst_row + px] = idx[row_base + x]
    return bytes(pixels), width, height


def render_full_town(archive: MpqArchive):
    """Render the entire assembled town at native pixel scale.

    Returns (tileset, pixels, width, height). Expensive (a few minutes,
    pure-Python pixel loops) but this is a one-time host-side asset-prep
    step, not something that runs on-device or repeatedly.
    """
    tileset = load_town_tileset(archive)
    grid = build_dpiece_grid(archive, tileset)
    pixels, width, height = render_region(tileset, grid, 0, DPIECE_GRID, 0, DPIECE_GRID)
    return tileset, pixels, width, height


def crop(pixels: bytes, width: int, height: int, center_x: int, center_y: int, out_w: int, out_h: int):
    """Slice an out_w x out_h window centered on (center_x, center_y)
    out of a full rendered pixel buffer, in that buffer's own coordinates."""
    x0 = center_x - out_w // 2
    y0 = center_y - out_h // 2
    out = bytearray(out_w * out_h)
    for y in range(out_h):
        sy = y0 + y
        if sy < 0 or sy >= height:
            continue
        src_row = sy * width
        dst_row = y * out_w
        for x in range(out_w):
            sx = x0 + x
            if 0 <= sx < width:
                out[dst_row + x] = pixels[src_row + sx]
    return bytes(out)
