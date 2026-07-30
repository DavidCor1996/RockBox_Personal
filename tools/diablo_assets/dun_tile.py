"""Decoder for Diablo's level-tile CEL encoding and the MIN/TIL/DUN level
format, validated against the real levels\\towndata assets in DIABDAT.MPQ.

Diablo's dungeon/town tile art is NOT the same encoding as sprite CELs
(see cel_to_clx-style transparent-run-length used for UI/monster frames).
Each 32x32 "micro" block carries a 3-bit type tag that selects one of six
distinct binary layouts -- plain raster, per-row transparent RLE, or one
of four fixed triangle/trapezoid shapes with no transparency markers at
all (the isometric diamond shape is implicit in which pixels are stored,
not encoded). Decoding a floor tile with the wrong type produces a
plausible-looking but wrong image (control bytes read out of a raw
raster), which is why every shape here was verified by rendering it
standalone and inspecting the result (a clean 64x32 diamond for the two
floor-triangle halves, a wood-grained wall silhouette for a multi-row
building piece) before assembling a full town.

Terminology, to match Diablo's own file naming:
  - "micro": one 32x32 (or 32x31) graphic block, referenced by CEL frame
    index and stored in <name>.cel.
  - "piece" (MIN entry): a vertical stack of micro block references --
    16 for town, 10 for cathedral/catacombs/caves/nest/crypt, 12 for hell.
  - "megatile" (TIL entry): four piece indices (micro1..4) forming the
    2x2 dPiece expansion of one dungeon/DUN grid cell.
"""
from __future__ import annotations

import struct

SQUARE, TRANSPARENT_SQUARE, LEFT_TRIANGLE, RIGHT_TRIANGLE, LEFT_TRAPEZOID, RIGHT_TRAPEZOID = range(6)

# 31 rows: width tapers 2..32 (row 15, the middle) ..2, forming a diamond
# half when a Left/Right pair is placed side by side.
_TRIANGLE_ROW_WIDTHS = tuple(2 * (i + 1) if i <= 15 else 2 * (31 - i) for i in range(31))


def _decode_square(src: bytes):
    out = bytearray(src[:1024])
    mask = bytearray(b"\x01" * 1024)
    return out, mask


def _decode_transparent_square(src: bytes):
    out = bytearray(32 * 32)
    mask = bytearray(32 * 32)
    pos = 0
    n = len(src)
    for y in range(32):
        col = 0
        while col < 32 and pos < n:
            control = src[pos]; pos += 1
            v = control if control < 128 else control - 256
            if v > 0:
                out[y * 32 + col: y * 32 + col + v] = src[pos:pos + v]
                mask[y * 32 + col: y * 32 + col + v] = b"\x01" * v
                pos += v
                col += v
            else:
                col += -v
    return out, mask


def _decode_triangle_rows(src: bytes, left: bool, row_count: int):
    """Shared row-walker for Triangle (31 rows) and Trapezoid (lower 16)."""
    out = bytearray(32 * 32)
    mask = bytearray(32 * 32)
    pos = 0
    for i in range(row_count):
        w = _TRIANGLE_ROW_WIDTHS[i]
        if left:
            if i % 2 == 0:
                pos += 2  # padding byte pair before even rows
            row = src[pos:pos + w]; pos += w
            x0 = 32 - w  # right-aligned: tapers away from the diamond seam
        else:
            row = src[pos:pos + w]; pos += w
            if i % 2 == 0:
                pos += 2  # padding byte pair after even rows
            x0 = 0  # left-aligned: tapers away from the diamond seam
        out[i * 32 + x0: i * 32 + x0 + w] = row
        mask[i * 32 + x0: i * 32 + x0 + w] = b"\x01" * w
    return out, mask, pos


def _decode_triangle(src: bytes, left: bool):
    out, mask, _ = _decode_triangle_rows(src, left, 31)
    return out, mask


def _decode_trapezoid(src: bytes, left: bool):
    out, mask, pos = _decode_triangle_rows(src, left, 16)
    for i in range(16, 32):
        row = src[pos:pos + 32]; pos += 32
        out[i * 32:(i + 1) * 32] = row
        mask[i * 32:(i + 1) * 32] = b"\x01" * 32
    return out, mask


_DECODERS = {
    SQUARE: lambda src: _decode_square(src),
    TRANSPARENT_SQUARE: lambda src: _decode_transparent_square(src),
    LEFT_TRIANGLE: lambda src: _decode_triangle(src, left=True),
    RIGHT_TRIANGLE: lambda src: _decode_triangle(src, left=False),
    LEFT_TRAPEZOID: lambda src: _decode_trapezoid(src, left=True),
    RIGHT_TRAPEZOID: lambda src: _decode_trapezoid(src, left=False),
}


class CelFrames:
    """Simple-container CEL parsing (single group): frame count, then
    (count+1) LE u32 offsets, the last equal to the file length."""

    def __init__(self, data: bytes):
        self.data = data
        num_frames = struct.unpack_from("<I", data, 0)[0]
        self.offsets = struct.unpack_from("<%dI" % (num_frames + 1), data, 4)
        if self.offsets[-1] != len(data):
            raise ValueError("grouped CEL containers are not supported here")

    def __len__(self):
        return len(self.offsets) - 1

    def frame_bytes(self, frame_index0based: int) -> bytes:
        return self.data[self.offsets[frame_index0based]:self.offsets[frame_index0based + 1]]


class TileSet:
    """A decoded <name>.min + <name>.til + <name>.cel triple."""

    def __init__(self, cel_data: bytes, min_data: bytes, til_data: bytes, blocks_per_piece: int):
        self.cel = CelFrames(cel_data)
        self.min_data = min_data
        self.til_data = til_data
        self.blocks = blocks_per_piece
        self.cols = 2
        self.rows = blocks_per_piece // self.cols
        self.piece_w = self.cols * 32
        self.piece_h = self.rows * 32
        self.num_pieces = len(min_data) // 2 // blocks_per_piece
        self.num_megatiles = len(til_data) // 8
        self._micro_cache: dict[tuple[int, int], tuple[bytearray, bytearray]] = {}
        self._piece_cache: dict[int, tuple[bytearray, bytearray]] = {}

    @staticmethod
    def _block_source_index(b: int, blocks: int) -> int:
        # Derived from the original engine's raw index arithmetic: MIN
        # entries are stored as (left,right) row pairs in reverse row order.
        return blocks - 2 + (b & 1) - (b & 0xE)

    def _decode_micro(self, frame_index0based: int, tile_type: int):
        key = (frame_index0based, tile_type)
        cached = self._micro_cache.get(key)
        if cached is not None:
            return cached
        decoder = _DECODERS.get(tile_type)
        if decoder is None:
            result = (bytearray(1024), bytearray(1024))
        else:
            result = decoder(self.cel.frame_bytes(frame_index0based))
        self._micro_cache[key] = result
        return result

    def decode_piece(self, piece_index0based: int):
        """Return (index_bytes, mask_bytes) for one MIN piece, as a
        piece_w x piece_h canvas with row 0 = bottom of the piece
        (ground level) and increasing rows extending upward (wall/roof
        height), matching the vertical anchor used when compositing."""
        cached = self._piece_cache.get(piece_index0based)
        if cached is not None:
            return cached
        base = piece_index0based * self.blocks
        canvas_idx = bytearray(self.piece_w * self.piece_h)
        canvas_mask = bytearray(self.piece_w * self.piece_h)
        for b in range(self.blocks):
            src_i = self._block_source_index(b, self.blocks)
            raw = struct.unpack_from("<H", self.min_data, (base + src_i) * 2)[0]
            if raw == 0:
                continue
            frame = raw & 0xFFF
            tile_type = (raw & 0x7000) >> 12
            if frame == 0:
                continue
            micro_idx, micro_mask = self._decode_micro(frame - 1, tile_type)
            bx = (b % self.cols) * 32
            by = (self.rows - 1 - (b // self.cols)) * 32
            for y in range(32):
                drow = (by + y) * self.piece_w + bx
                srow = y * 32
                for x in range(32):
                    if micro_mask[srow + x]:
                        canvas_idx[drow + x] = micro_idx[srow + x]
                        canvas_mask[drow + x] = 1
        result = (canvas_idx, canvas_mask)
        self._piece_cache[piece_index0based] = result
        return result

    def megatile_pieces(self, til_index0based: int):
        """Return (micro1..4) as 0-based piece indices, or None for an
        empty quadrant, for one TIL/megatile entry."""
        m1, m2, m3, m4 = struct.unpack_from("<4H", self.til_data, til_index0based * 8)
        return tuple((v - 1) if v else None for v in (m1, m2, m3, m4))


def dun_size(dun_data: bytes):
    return struct.unpack_from("<2H", dun_data, 0)


def dun_tile_layer(dun_data: bytes):
    width, height = dun_size(dun_data)
    return width, height, struct.unpack_from("<%dH" % (width * height), dun_data, 4)
