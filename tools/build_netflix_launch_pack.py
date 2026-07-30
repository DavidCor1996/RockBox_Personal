#!/usr/bin/env python3
"""Pack the authentic Netflix launch frames for the iPodJS framebuffer."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


PALETTE_COLORS = 16


def read_bmp(path: Path, expected_width: int,
             expected_height: int) -> list[tuple[int, int, int]]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"not a BMP: {path}")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if (width != expected_width or abs(height) != expected_height or
            bits != 24 or compression):
        raise ValueError(
            f"expected uncompressed {expected_width}x{expected_height}x24 "
            f"BMP: {path}"
        )
    row_bytes = (width * 3 + 3) & ~3
    rows = []
    for output_y in range(expected_height):
        source_y = expected_height - 1 - output_y if height > 0 else output_y
        row = data[
            pixel_offset + source_y * row_bytes :
            pixel_offset + source_y * row_bytes + width * 3
        ]
        rows.extend(
            (row[index + 2], row[index + 1], row[index])
            for index in range(0, len(row), 3)
        )
    return rows


def squared_distance(first: tuple[int, int, int],
                     second: tuple[int, int, int]) -> int:
    return sum((first[channel] - second[channel]) ** 2 for channel in range(3))


def make_palette(pixels: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    sample = pixels[::32]
    centers = [min(sample, key=sum)]
    while len(centers) < PALETTE_COLORS:
        centers.append(
            max(
                sample,
                key=lambda pixel: min(
                    squared_distance(pixel, center) for center in centers
                ),
            )
        )
    for _ in range(20):
        totals = [[0, 0, 0, 0] for _ in centers]
        for pixel in sample:
            choice = min(
                range(len(centers)),
                key=lambda index: squared_distance(pixel, centers[index]),
            )
            total = totals[choice]
            total[0] += pixel[0]
            total[1] += pixel[1]
            total[2] += pixel[2]
            total[3] += 1
        centers = [
            tuple(total[channel] // total[3] for channel in range(3))
            if total[3] else centers[index]
            for index, total in enumerate(totals)
        ]
    return centers


def rgb565(color: tuple[int, int, int]) -> int:
    red, green, blue = color
    return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)


def encode_delta(current: list[int], previous: list[int]) -> bytes:
    encoded = bytearray()
    position = 0
    while position < len(current):
        unchanged = current[position] == previous[position]
        end = position + 1
        while (
            end < len(current)
            and (current[end] == previous[end]) == unchanged
            and end - position < 0x7FFF
        ):
            end += 1
        count = end - position
        if unchanged:
            encoded += struct.pack("<H", 0x8000 | count)
        else:
            encoded += struct.pack("<H", count)
            for index in range(position, end, 2):
                value = current[index] << 4
                if index + 1 < end:
                    value |= current[index + 1]
                encoded.append(value)
        position = end
    return bytes(encoded)


def encode_delta_rgb565(current: list[int], previous: list[int]) -> bytes:
    encoded = bytearray()
    position = 0
    while position < len(current):
        unchanged = current[position] == previous[position]
        end = position + 1
        while (
            end < len(current)
            and (current[end] == previous[end]) == unchanged
            and end - position < 0x7FFF
        ):
            end += 1
        count = end - position
        encoded += struct.pack("<H", (0x8000 if unchanged else 0) | count)
        if not unchanged:
            encoded += struct.pack(f"<{count}H", *current[position:end])
        position = end
    return bytes(encoded)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("frame_dir", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--frame-ms", type=int, default=100)
    parser.add_argument("--expected-frames", type=int)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument(
        "--rgb565", action="store_true",
        help="store full RGB565 delta frames instead of a 16-color palette",
    )
    args = parser.parse_args()
    paths = sorted(args.frame_dir.glob("frame-*.bmp"))
    if not paths:
        raise SystemExit("no launch frames found")
    if args.expected_frames is not None and len(paths) != args.expected_frames:
        raise SystemExit(
            f"expected {args.expected_frames} launch frames, found {len(paths)}"
        )
    if not 1 <= args.frame_ms <= 0xFFFF:
        raise SystemExit("--frame-ms must fit in an unsigned 16-bit value")

    if args.width <= 0 or args.height <= 0:
        raise SystemExit("frame dimensions must be positive")
    rgb_frames = [
        read_bmp(path, args.width, args.height) for path in paths
    ]
    if args.rgb565:
        palette = []
        encoded_frames = [
            [rgb565(pixel) for pixel in frame] for frame in rgb_frames
        ]
    else:
        palette = make_palette(
            [pixel for frame in rgb_frames for pixel in frame]
        )
        encoded_frames = [
            [
                min(
                    range(PALETTE_COLORS),
                    key=lambda index: squared_distance(pixel, palette[index]),
                )
                for pixel in frame
            ]
            for frame in rgb_frames
        ]

    previous = [0] * (args.width * args.height)
    payloads = []
    for frame in encoded_frames:
        payloads.append(
            encode_delta_rgb565(frame, previous)
            if args.rgb565 else encode_delta(frame, previous)
        )
        previous = frame

    palette_count = 0 if args.rgb565 else PALETTE_COLORS
    header_size = 16 + palette_count * 2 + (len(payloads) + 1) * 4
    offsets = [header_size]
    for payload in payloads:
        offsets.append(offsets[-1] + len(payload))
    output = bytearray(
        struct.pack(
            "<4s6H",
            b"NFR1" if args.rgb565 else b"NFX1",
            args.width,
            args.height,
            len(payloads),
            args.frame_ms,
            palette_count,
            0,
        )
    )
    for color in palette:
        output += struct.pack("<H", rgb565(color))
    output += struct.pack(f"<{len(offsets)}I", *offsets)
    output += b"".join(payloads)
    args.output.write_bytes(output)
    print(f"Wrote {args.output} ({len(output)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
