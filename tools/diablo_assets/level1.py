"""Assemble the hand-authored Cathedral level-1 dungeon (see
dungeon_gen.py) into the same shape town.py produces: a TileSet plus a
dpiece_w x dpiece_h grid of piece indices, ready for world_pack.build().
"""
from __future__ import annotations

from mpq_reader import MpqArchive
from dun_tile import TileSet
import dungeon_gen

L1_BLOCKS_PER_PIECE = 10


def load_l1_tileset(archive: MpqArchive) -> TileSet:
    return TileSet(
        cel_data=archive.read("levels\\l1data\\l1.cel"),
        min_data=archive.read("levels\\l1data\\l1.min"),
        til_data=archive.read("levels\\l1data\\l1.til"),
        blocks_per_piece=L1_BLOCKS_PER_PIECE,
    )


def build_dpiece_grid(tileset: TileSet):
    til_grid = dungeon_gen.build_til_grid()
    return dungeon_gen.dpiece_grid_from_til(tileset, til_grid)
