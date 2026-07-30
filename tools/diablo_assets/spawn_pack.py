"""Small binary pack for a level's gameplay-side data that world.c's
renderer doesn't need to know about: player start/stairs position and
initial monster/item spawns. Kept separate from <name>.meta (which is
pure render/collision data) so game.c can own it without world.c
depending on gameplay concepts.

Format (all little-endian):
  4s  magic "SPWN"
  H   start_x, H start_y      (dPiece coords)
  H   stairs_x, H stairs_y    (dPiece coords)
  B   num_monsters
        num_monsters * (H x, H y, B kind, B hp)
  B   num_items
        num_items * (H x, H y, B kind)
"""
from __future__ import annotations

import struct

SPAWN_MAGIC = b"SPWN"

MONSTER_HP = (20, 40)  # by kind: 0 = fallen, 1 = zombie


def build(start, stairs, monsters, items) -> bytes:
    sx, sy = start
    tx, ty = stairs
    out = bytearray()
    out += struct.pack("<4sHHHH", SPAWN_MAGIC, sx, sy, tx, ty)
    out.append(len(monsters))
    for x, y, kind in monsters:
        out += struct.pack("<HHBB", x, y, kind, MONSTER_HP[kind])
    out.append(len(items))
    for x, y, kind in items:
        out += struct.pack("<HHB", x, y, kind)
    return bytes(out)
