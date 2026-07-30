"""Minimal 256-color PCX decoder (RLE scanlines + trailing 768-byte palette).

Only the subset needed for Diablo's ui_art/*.pcx splash screens: 8bpp,
1 color plane. This is a generic, well-documented container format (not
Blizzard content) -- see ZSoft's original PCX spec.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass


@dataclass
class PcxImage:
    width: int
    height: int
    palette: bytes  # 768 bytes, RGB order
    pixels: bytes  # width * height palette indices, row-major


def decode(data: bytes) -> PcxImage:
    header = data[:128]
    encoding = header[2]
    bpp = header[3]
    xmin, ymin, xmax, ymax = struct.unpack_from("<4H", header, 4)
    nplanes = header[65]
    bytes_per_line = struct.unpack_from("<H", header, 66)[0]

    if encoding != 1 or bpp != 8 or nplanes != 1:
        raise ValueError(f"unsupported PCX variant: enc={encoding} bpp={bpp} planes={nplanes}")

    width = xmax - xmin + 1
    height = ymax - ymin + 1

    if data[-769] != 0x0C:
        raise ValueError("missing 256-color PCX palette marker")
    palette = data[-768:]

    body = data[128:-769]
    pos = 0
    rows = bytearray()
    for _ in range(height):
        row = bytearray()
        while len(row) < bytes_per_line:
            b = body[pos]
            pos += 1
            if (b & 0xC0) == 0xC0:
                count = b & 0x3F
                val = body[pos]
                pos += 1
                row += bytes([val]) * count
            else:
                row.append(b)
        rows += row[:width]

    if pos != len(body):
        raise ValueError(f"PCX RLE stream left {len(body) - pos} unused bytes")

    return PcxImage(width=width, height=height, palette=palette, pixels=bytes(rows))
