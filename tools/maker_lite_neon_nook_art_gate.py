#!/usr/bin/env python3
"""Validate and render the packed Neon Nook fox metasprite animations."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

from PIL import Image, ImageDraw


REPO = Path(__file__).resolve().parents[1]
DEFAULT_PACK = REPO / "assets/maker_lite/neon_nook/pack"
ACTIONS = (
    "idle",
    "walk",
    "run",
    "jump",
    "fall",
    "crouch",
    "skid",
    "swim",
    "sword",
    "item",
    "roll",
    "spindash",
    "hurt",
    "complete",
)
DIRECTIONS = ("right", "down", "left", "up")
TRANSPARENT = 0xF81F


def decode_rgb565(value: int) -> tuple[int, int, int, int]:
    if value == TRANSPARENT:
        return (0, 0, 0, 0)
    return (
        ((value >> 11) & 31) * 255 // 31,
        ((value >> 5) & 63) * 255 // 63,
        (value & 31) * 255 // 31,
        255,
    )


def decode(pack: Path):
    art = (pack / "art.mla").read_bytes()
    manifest = json.loads((pack / "kit.mlk").read_text(encoding="utf-8"))
    if art[:4] != b"MLAR" or int.from_bytes(art[4:6], "little") != 4:
        raise SystemExit("Neon Nook art is not bounded MLAR v4")
    cell_size = int.from_bytes(art[6:8], "little")
    cell_count = int.from_bytes(art[8:10], "little")
    if cell_size != 16 or cell_count != manifest["cell_count"]:
        raise SystemExit("art and manifest cell geometry disagree")

    cells = []
    cursor = 64
    for _ in range(cell_count):
        cell = Image.new("RGBA", (16, 16))
        pixels = [
            decode_rgb565(value[0])
            for value in struct.iter_unpack("<H", art[cursor : cursor + 512])
        ]
        cell.putdata(pixels)
        cells.append(cell)
        cursor += 512

    animations = {}
    for action in ACTIONS:
        animations[action] = {}
        for direction in DIRECTIONS:
            start, count, ticks = struct.unpack_from("<HBB", art, cursor)
            cursor += 4
            animations[action][direction] = (start, count, ticks & 0x7F)

    frame_count, reserved = struct.unpack_from("<HH", art, cursor)
    cursor += 4
    if reserved or frame_count != manifest["player_frame_count"]:
        raise SystemExit("metasprite frame header disagrees with manifest")
    frames = []
    for index in range(frame_count):
        columns, rows, offset_x, offset_y, *indexes = struct.unpack_from(
            "<BBbb16H", art, cursor
        )
        cursor += 36
        if (columns, rows, offset_x, offset_y) != (2, 2, -16, -31):
            raise SystemExit(
                f"frame {index} has unexpected geometry "
                f"{columns}x{rows}@{offset_x},{offset_y}"
            )
        composed = Image.new("RGBA", (32, 32))
        for cell_index, atlas_index in enumerate(indexes[:4]):
            if atlas_index >= cell_count:
                raise SystemExit(f"frame {index} references bad cell {atlas_index}")
            composed.alpha_composite(
                cells[atlas_index],
                ((cell_index % 2) * 16, (cell_index // 2) * 16),
            )
        bbox = composed.getbbox()
        if not bbox:
            raise SystemExit(f"frame {index} is empty")
        opaque = sum(
            alpha >= 128
            for alpha in composed.getchannel("A").get_flattened_data()
        )
        if opaque < 90:
            raise SystemExit(f"frame {index} is suspiciously sparse")
        if (
            bbox[0] == 0
            or bbox[1] == 0
            or bbox[2] == 32
            or bbox[3] == 32
        ):
            raise SystemExit(
                f"frame {index} touches a cut edge: bbox={bbox}"
            )
        frames.append(composed)
    if cursor != len(art):
        raise SystemExit("unexpected trailing bytes in art.mla")
    return frames, animations


def contact_sheet(frames, animations, output: Path) -> None:
    scale = 4
    selected_actions = ("idle", "walk", "run", "item", "hurt")
    slots_per_row = sum(
        max(animations[action][direction][1] for direction in DIRECTIONS)
        for action in selected_actions
    )
    sheet = Image.new(
        "RGB",
        (84 + slots_per_row * 32 * scale, 28 + 4 * 32 * scale),
        (9, 13, 28),
    )
    draw = ImageDraw.Draw(sheet)
    for row, direction in enumerate(DIRECTIONS):
        draw.text((8, 18 + row * 32 * scale), direction, fill=(240, 240, 255))
        slot = 0
        for action in selected_actions:
            start, count, _ticks = animations[action][direction]
            for frame_index in range(start, start + count):
                enlarged = frames[frame_index].resize(
                    (32 * scale, 32 * scale), Image.Resampling.NEAREST
                )
                sheet.paste(
                    enlarged,
                    (84 + slot * 32 * scale, 18 + row * 32 * scale),
                    enlarged,
                )
                slot += 1
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pack", type=Path, default=DEFAULT_PACK)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("/tmp/maker-lite-neon-nook-art/player-contact.png"),
    )
    args = parser.parse_args()
    frames, animations = decode(args.pack.resolve())
    contact_sheet(frames, animations, args.output.resolve())
    print(
        f"PASS Neon Nook art: {len(frames)} complete fox frames; "
        f"contact={args.output.resolve()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
