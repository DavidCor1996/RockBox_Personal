"""Lucida Grande glyph atlases for Desktop Mode.

The previous atlas builder centred every glyph vertically inside its own cell
using that glyph's own bounding box, so no two glyphs shared a baseline: "p"
and "P" were drawn at the same height and words came out as "APPlicatio".  It
also positioned each glyph at `column * cell - bbox.x0` while advancing the pen
by the glyph's full advance, so drawn position and advance disagreed by the
left side bearing.  Finally it thresholded Lucida Grande's antialiasing to
fully opaque, which turned every soft edge into a black blot.

This builder renders the real font with PIL's "la" anchor, which puts the
baseline at a fixed offset in every cell, keeps the left side bearing inside
the cell so the pen position is the cell origin, and keeps the 8-bit coverage
Apple's outlines actually produce.  Desktop Mode blends that coverage over
whatever it has already composed, so text is antialiased against real chrome
instead of against one guessed backdrop colour.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FIRST_GLYPH = 32
LAST_GLYPH = 126
GLYPH_COUNT = LAST_GLYPH - FIRST_GLYPH + 1
ATLAS_COLUMNS = 16
ATLAS_ROWS = (GLYPH_COUNT + ATLAS_COLUMNS - 1) // ATLAS_COLUMNS
METRICS_MAGIC = b"DMF1"
METRICS_HEADER = 8
METRICS_BYTES = METRICS_HEADER + GLYPH_COUNT


@dataclass(frozen=True)
class FontAtlas:
    cell_w: int
    cell_h: int
    ascent: int
    coverage: bytes
    metrics: bytes

    @property
    def width(self) -> int:
        return self.cell_w * ATLAS_COLUMNS

    @property
    def height(self) -> int:
        return self.cell_h * ATLAS_ROWS


def build_atlas(source: Path, *, size: int, face: int) -> FontAtlas:
    """Render one Lucida Grande face into a common-baseline coverage atlas."""
    font = ImageFont.truetype(str(source), size=size, index=face)
    ascent, descent = font.getmetrics()
    cell_h = ascent + descent

    advances = []
    extent = 1
    for codepoint in range(FIRST_GLYPH, LAST_GLYPH + 1):
        char = chr(codepoint)
        advance = int(round(font.getlength(char)))
        advances.append(max(0, min(255, advance)))
        right = font.getbbox(char, anchor="la")[2]
        extent = max(extent, advance, int(right))
    cell_w = extent + 1

    atlas = Image.new("L", (cell_w * ATLAS_COLUMNS, cell_h * ATLAS_ROWS), 0)
    draw = ImageDraw.Draw(atlas)
    for index, codepoint in enumerate(range(FIRST_GLYPH, LAST_GLYPH + 1)):
        column = index % ATLAS_COLUMNS
        row = index // ATLAS_COLUMNS
        draw.text(
            (column * cell_w, row * cell_h),
            chr(codepoint),
            fill=255,
            font=font,
            anchor="la",
        )

    metrics = bytearray(
        struct.pack(
            "<4sBBBB",
            METRICS_MAGIC,
            cell_w,
            cell_h,
            min(255, ascent),
            GLYPH_COUNT,
        )
    )
    metrics.extend(advances)
    return FontAtlas(
        cell_w=cell_w,
        cell_h=cell_h,
        ascent=ascent,
        coverage=atlas.tobytes(),
        metrics=bytes(metrics),
    )


__all__ = [
    "ATLAS_COLUMNS",
    "ATLAS_ROWS",
    "FIRST_GLYPH",
    "FontAtlas",
    "GLYPH_COUNT",
    "LAST_GLYPH",
    "METRICS_BYTES",
    "build_atlas",
]
