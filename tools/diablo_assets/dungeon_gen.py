"""Deterministic room-and-corridor generator for a single fixed Cathedral
(level 1) layout.

This is explicitly NOT a port of Diablo's real DRLG (dungeon random level
generator) algorithm -- DRLG is a separate, comparably-sized subsystem that
builds a new random layout from scratch each game using per-tileset
adjacency rules. Porting it is out of scope for "basic features". Instead
this hand-authors one fixed room graph and emits the same kind of
til-index grid that town.py's build_dpiece_grid() produces by stitching
sector files -- from world_pack.py's point of view, a procedurally
authored level and a hand-placed one look identical (a 2D array of
megatile indices), so the existing pack/render/collision pipeline needs
no dungeon-specific code.

Tile choices (verified by standalone render before use, same as
dun_tile.py's shapes -- see /tmp render checks during development):
  - FLOOR_TIL = 140: a plain grey stone floor megatile, all four quadrant
    pieces have sol byte 0x00 (fully passable, no gore/holes).
  - WALL_TIL  = 6: a tall stone wall-facade megatile, all four quadrant
    pieces have sol bit 0x01 set (fully blocking); denser coverage than
    the other all-solid candidates tried (til 25 reads as a colonnade
    with floor visible between pillars when repeated -- fine as a single
    accent piece but not as a continuous boundary).
Both were selected by rendering every megatile in levels\\l1data\\l1.til
to a contact sheet and checking sol bytes programmatically -- not
guessed from Diablo source constants, which describe a different
(preprocessed) tile numbering than the raw MPQ files (see dun_tile.py's
module docstring).
"""
from __future__ import annotations

FLOOR_TIL = 140
WALL_TIL = 6

TIL_W = 20
TIL_H = 20

# (x0, y0, w, h) in til coordinates.
ROOMS = [
    (2, 2, 5, 5),    # Room A: start
    (12, 3, 5, 4),   # Room B
    (7, 13, 6, 5),   # Room C: stairs back to town
]

# Sequential corridors between room centers, each a straight L-shaped path
# two til cells wide.
CORRIDORS = [(0, 1), (0, 2)]


def _room_center(room):
    x0, y0, w, h = room
    return x0 + w // 2, y0 + h // 2


def build_til_grid():
    """Returns a TIL_W x TIL_H grid of til indices (grid[x][y])."""
    grid = [[WALL_TIL] * TIL_H for _ in range(TIL_W)]

    def carve(x0, y0, w, h):
        for x in range(x0, x0 + w):
            for y in range(y0, y0 + h):
                if 0 <= x < TIL_W and 0 <= y < TIL_H:
                    grid[x][y] = FLOOR_TIL

    for room in ROOMS:
        carve(*room)

    def carve_corridor(cx0, cy0, cx1, cy1):
        # Two-til-wide L-shaped path: horizontal leg then vertical leg.
        x, y = cx0, cy0
        while x != cx1:
            carve(x, y, 1, 2)
            x += 1 if cx1 > x else -1
        while y != cy1:
            carve(x, y, 2, 1)
            y += 1 if cy1 > y else -1
        carve(cx1, cy1, 2, 2)

    for a, b in CORRIDORS:
        ax, ay = _room_center(ROOMS[a])
        bx, by = _room_center(ROOMS[b])
        carve_corridor(ax, ay, bx, by)

    return grid


def dpiece_grid_from_til(tileset, til_grid):
    """Expand a TIL_W x TIL_H til-index grid into a (2*TIL_W) x (2*TIL_H)
    dPiece-index grid, the same shape world_pack.build() expects (see
    town.build_dpiece_grid)."""
    dw, dh = TIL_W * 2, TIL_H * 2
    grid = [[0] * dh for _ in range(dw)]
    for x in range(TIL_W):
        for y in range(TIL_H):
            til = til_grid[x][y]
            m1, m2, m3, m4 = tileset.megatile_pieces(til)
            xx, yy = x * 2, y * 2
            grid[xx + 0][yy + 0] = m1 if m1 is not None else 0
            grid[xx + 1][yy + 0] = m2 if m2 is not None else 0
            grid[xx + 0][yy + 1] = m3 if m3 is not None else 0
            grid[xx + 1][yy + 1] = m4 if m4 is not None else 0
    return grid


def start_dpiece():
    x, y = _room_center(ROOMS[0])
    return x * 2, y * 2


def stairs_dpiece():
    x, y = _room_center(ROOMS[-1])
    return x * 2, y * 2


# (dpx, dpy, kind) -- kind: 0 = fallen (weak melee), 1 = zombie (tougher).
MONSTER_SPAWNS = [
    (12 * 2 + 2, 3 * 2 + 1, 0),
    (12 * 2 + 1, 3 * 2 + 2, 0),
    (7 * 2 + 3, 13 * 2 + 2, 1),
]

# (dpx, dpy, kind) -- kind: 0 = healing potion.
ITEM_SPAWNS = [
    (2 * 2 + 1, 2 * 2 + 3, 0),
    (7 * 2 + 4, 13 * 2 + 3, 0),
]
