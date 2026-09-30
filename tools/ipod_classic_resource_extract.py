#!/usr/bin/env python3
"""Extract every bitmap from the official iPod classic 2.0.4 OS image.

This is a read-only, fail-closed extractor for the ``paMB`` section embedded
in Apple's decrypted iPod35 RetailOS resource database.  It accepts only the
known 2.0.4 image (either its 0x800-byte firmware wrapper or the exact body),
requires the complete 598-record table, and writes a deterministic PNG plus
an auditable manifest row for every record.

No artwork is generated, traced, resampled, interpolated, or repaired.  A
malformed, unsupported, missing, duplicate, or partially decoded resource is
a hard error, because a partial dump must never be presented as a full port.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import struct
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


CLASSIC_204_WRAPPED_SHA256 = (
    "f4368251a58b2fdc7b46acf3178dae1d24bc1e029736240741015851256c65c4"
)
CLASSIC_204_BODY_SHA256 = (
    "7910c276c85a0aa67c4f36fb2d72476791b7d7fe9a21442dae53bff27b48d684"
)
RESOURCE_OFFSETS = {
    CLASSIC_204_WRAPPED_SHA256: 0x40B930,
    CLASSIC_204_BODY_SHA256: 0x40B130,
}
RESOURCE_VERSION = 3
RESOURCE_HEADER_LENGTH = 0x176D0
RESOURCE_COUNT = 598
IMAGE_SYMBOL_COUNT = 658
IMAGE_SYMBOL_RESOURCE_COUNT = 566
RESOURCE_HEADER = struct.Struct("<III")
RESOURCE_SECTION = struct.Struct("<4sIII")
RESOURCE_ENTRY = struct.Struct("<III")
PICTURE_HEADER = struct.Struct("<HHHH8sIII")
FRAME_PACK_HEADER = struct.Struct("<4s6H2I")
EXPECTED_SECTIONS = (
    (b"ILAA", 1, 1, 0x001BC),
    (b"TSCA", 1, 1, 0x001C8),
    (b"TVEA", 1, 1, 0x001D4),
    (b"paMB", 598, 0, 0x001E0),
    (b"TVEC", 149, 0, 0x01DE8),
    (b"RLOC", 85, 0, 0x024E4),
    (b"EEEE", 4, 0, 0x028E0),
    (b"TNOF", 65, 1, 0x02910),
    (b"RTSF", 65, 1, 0x02C1C),
    (b"GAMI", 598, 0, 0x02F28),
    (b"METI", 83, 1, 0x04B30),
    (b"mTDL", 1, 1, 0x04F14),
    (b"TSCS", 149, 0, 0x04F20),
    (b"TVES", 336, 0, 0x0561C),
    (b"MEIS", 6, 0, 0x065DC),
    (b"tsLS", 149, 0, 0x06624),
    (b"tyLS", 336, 0, 0x06D20),
    (b"CROS", 1637, 0, 0x07CE0),
    (b"niSS", 337, 0, 0x0C99C),
    (b" rtS", 1112, 0, 0x0D968),
    (b"TVET", 248, 0, 0x10D88),
    (b"TLMT", 149, 0, 0x11928),
    (b"vrCV", 1095, 1, 0x12024),
    (b"svCV", 444, 0, 0x15378),
    (b"tyLV", 132, 0, 0x16848),
    (b"tlSV", 29, 0, 0x16E78),
    (b"weiV", 149, 0, 0x16FD4),
)
EXPECTED_FORMAT_COUNTS = {
    0x0004: 183,
    0x0008: 35,
    0x0064: 263,
    0x0065: 43,
    0x0565: 9,
    0x1888: 65,
}

# iPod35's wrapper describes three loadable ranges.  Registry entries in the
# RetailOS body store runtime pointers to image-symbol strings followed by the
# corresponding paMB token.  Keeping both wrapped/body layouts lets us prove
# symbol-to-resource identity for either accepted source without guessing from
# order, dimensions, or appearance.
SOURCE_RUNTIME_SEGMENTS = {
    CLASSIC_204_WRAPPED_SHA256: (
        (0x00000800, 0x22000000, 0x0000AED8),
        (0x0000B6D8, 0x08000000, 0x00A0FC88),
        (0x00A1B360, 0x08A0FC88, 0x00000A84),
    ),
    CLASSIC_204_BODY_SHA256: (
        (0x00000000, 0x22000000, 0x0000AED8),
        (0x0000AED8, 0x08000000, 0x00A0FC88),
        (0x00A1AB60, 0x08A0FC88, 0x00000A84),
    ),
}


@dataclass(frozen=True)
class SequenceSpec:
    name: str
    first_ordinal: int
    labels: tuple[str, ...]
    symbol: str


def number_labels(count: int) -> tuple[str, ...]:
    return tuple(str(index) for index in range(count))


# These are the complete source-frame runs referenced by RetailOS's named
# image/animated-image views.  Clock quarter/half-turn sprites are transformed
# by RetailOS at presentation time; this ledger records every source sprite,
# not synthetic rotations.  The two battery runs include every level plus the
# distinct plug and charge glyph states.
ANIMATION_SEQUENCES = (
    SequenceSpec(
        "statusbar-white-battery",
        33,
        number_labels(23) + ("plug", "charge"),
        "StatusBarWhite_Battery_Image_*",
    ),
    SequenceSpec(
        "statusbar-black-battery",
        58,
        number_labels(23) + ("plug", "charge"),
        "StatusBarBlack_Battery_Image_*",
    ),
    SequenceSpec(
        "clock-hours",
        152,
        number_labels(30),
        "Clock_Hours_Image_*",
    ),
    SequenceSpec(
        "clock-minutes",
        182,
        number_labels(16),
        "Clock_Mins_Image_*",
    ),
    SequenceSpec(
        "clock-seconds",
        198,
        number_labels(16),
        "Clock_Secs_Image_*",
    ),
    SequenceSpec(
        "now-playing-idle-battery",
        318,
        number_labels(8),
        "NowPlaying_Idle_Battery_*_Image",
    ),
    SequenceSpec(
        "radio-scanning",
        381,
        tuple(str(index) for index in range(1, 8)),
        "Radio_Scanning_*_Image",
    ),
    SequenceSpec(
        "now-playing-equalizer",
        460,
        number_labels(22),
        "NowPlaying_Animated_Image",
    ),
    SequenceSpec(
        "stopwatch-minutes",
        487,
        number_labels(30),
        "Stopwatch_Mins_Image_*",
    ),
    SequenceSpec(
        "stopwatch-seconds",
        517,
        number_labels(30),
        "Stopwatch_Secs_Image_*",
    ),
    SequenceSpec(
        "disk-mode-sync-arrows",
        564,
        tuple(str(index) for index in range(1, 36, 2)),
        "DiskModeImage_SyncArrow*",
    ),
)


# Named, non-sequence components required to compose the matching RetailOS
# surfaces.  Keeping the components separate is important: for example, the
# charging view tiles/masks these resources and does not contain a hidden set
# of full-screen raster frames.
NAMED_ASSETS = {
    "system-scrollbar-top": 0,
    "system-scrollbar-center": 1,
    "system-scrollbar-bottom": 2,
    "system-submenu": 3,
    "system-quick-scroll": 4,
    "system-quick-scroll-123": 5,
    "coverflow-proxy": 6,
    "coverflow-proxy-itunesu": 7,
    "statusbar-white-background": 8,
    "statusbar-white-play-status": 9,
    "statusbar-black-background": 10,
    "statusbar-black-lock": 31,
    "statusbar-white-lock": 32,
    "system-overlay-left": 83,
    "system-overlay-right": 84,
    "system-overlay-middle": 85,
    "optionbar-white-well-left": 95,
    "optionbar-white-well-center": 96,
    "optionbar-white-well-right": 97,
    "optionbar-white-thumb-left": 98,
    "optionbar-white-thumb-center": 99,
    "optionbar-white-thumb-right": 100,
    "optionbar-black-well-left": 101,
    "optionbar-black-well-center": 102,
    "optionbar-black-well-right": 103,
    "optionbar-black-thumb-left": 104,
    "optionbar-black-thumb-center": 105,
    "optionbar-black-thumb-right": 106,
    "optionbar-now-playing-well-left": 107,
    "optionbar-now-playing-well-center": 108,
    "optionbar-now-playing-well-right": 109,
    "optionbar-now-playing-thumb-left": 110,
    "optionbar-now-playing-thumb-center": 111,
    "optionbar-now-playing-thumb-right": 112,
    "system-overlay-brightness-less": 113,
    "system-overlay-brightness-more": 114,
    "system-overlay-volume-left": 115,
    "system-overlay-volume-right": 116,
    "system-input-field-left": 117,
    "system-input-field-right": 118,
    "system-input-field-middle": 119,
    "settings-apple-logo": 427,
    "settings-main-menu": 435,
    "world-clock-graybar": 145,
    "world-clock-map": 146,
    "clock-small": 147,
    "clock-small-night": 148,
    "clock-large": 149,
    "clock-center-cap": 150,
    "clock-shadow": 151,
    "now-playing-statusbar": 265,
    "now-playing-overlay": 266,
    "now-playing-white-shuffle": 267,
    "now-playing-white-repeat": 268,
    "now-playing-white-repeat-once": 269,
    "now-playing-black-shuffle": 270,
    "now-playing-black-repeat": 271,
    "now-playing-black-repeat-once": 272,
    "now-playing-black-pause": 273,
    "now-playing-black-play": 274,
    "now-playing-black-record": 275,
    "now-playing-white-pause": 276,
    "now-playing-white-play": 277,
    "now-playing-black-fast-forward": 278,
    "now-playing-black-rewind": 279,
    "now-playing-white-fast-forward": 280,
    "now-playing-white-rewind": 281,
    "now-playing-progressbar-left": 282,
    "now-playing-progressbar-right": 283,
    "now-playing-progressbar-growth": 284,
    "now-playing-progressfill-left": 285,
    "now-playing-progressfill-right": 286,
    "now-playing-progressfill-growth": 287,
    "now-playing-scrub": 288,
    "now-playing-paused": 289,
    "now-playing-star": 290,
    "now-playing-blue-star": 291,
    "now-playing-blue-dot": 292,
    "now-playing-large-shuffle": 293,
    "now-playing-progressbar-inactive-background": 294,
    "now-playing-progressbar-inactive-fill": 295,
    "now-playing-progressbar-active-background": 296,
    "now-playing-progressbar-active-fill": 297,
    "now-playing-progressbar-scrub": 298,
    "now-playing-small-white-star": 299,
    "now-playing-small-grey-star": 300,
    "now-playing-large-blue-star": 301,
    "now-playing-dot": 302,
    "now-playing-white-volume-low": 303,
    "now-playing-white-volume-high": 304,
    "now-playing-black-volume-locked": 305,
    "now-playing-white-volume-locked": 306,
    "now-playing-idle-digit-0": 307,
    "now-playing-idle-digit-1": 308,
    "now-playing-idle-digit-2": 309,
    "now-playing-idle-digit-3": 310,
    "now-playing-idle-digit-4": 311,
    "now-playing-idle-digit-5": 312,
    "now-playing-idle-digit-6": 313,
    "now-playing-idle-digit-7": 314,
    "now-playing-idle-digit-8": 315,
    "now-playing-idle-digit-9": 316,
    "now-playing-idle-colon": 317,
    "now-playing-idle-lock": 326,
    "now-playing-idle-play": 327,
    "now-playing-idle-radio": 328,
    "radio-bottom-bar": 363,
    "radio-bottom-background": 364,
    "radio-ticks": 365,
    "radio-ticks-japan": 366,
    "radio-digit-0": 367,
    "radio-digit-1": 368,
    "radio-digit-2": 369,
    "radio-digit-3": 370,
    "radio-digit-4": 371,
    "radio-digit-5": 372,
    "radio-digit-6": 373,
    "radio-digit-7": 374,
    "radio-digit-8": 375,
    "radio-digit-9": 376,
    "radio-digit-period": 377,
    "radio-preset": 378,
    "radio-current": 379,
    "radio-mhz": 380,
    "radio-signal-light": 388,
    "radio-signal-dark": 389,
    "radio-icon": 390,
    "stopwatch-play-pause": 482,
    "stopwatch-small-pause": 483,
    "stopwatch-small-play": 484,
    "stopwatch-caps": 485,
    "stopwatch-clock": 486,
    "disk-mode-sync-icon": 392,
    "disk-mode-connected-icon": 562,
    "disk-mode-disconnect-icon": 563,
    "disk-mode-progress-empty-left": 582,
    "disk-mode-progress-empty-right": 583,
    "disk-mode-progress-empty-fill": 584,
    "disk-mode-progress-full-left": 585,
    "disk-mode-progress-full-right": 586,
    "disk-mode-progress-full-fill": 587,
    "charging-empty-cap-left": 588,
    "charging-empty-cap-right": 589,
    "charging-empty-middle": 590,
    "charging-green-cap-left": 591,
    "charging-green-cap-right": 592,
    "charging-green-middle": 593,
    "charging-green-middle-cap": 594,
    "charging-charge": 595,
    "charging-plug": 596,
    "charging-critical": 597,
}


@dataclass(frozen=True)
class ResourceRecord:
    ordinal: int
    token: int
    relative_offset: int
    source_offset: int
    source_size: int
    image_format: int
    unknown: int
    row_bytes: int
    flags: int
    width: int
    height: int
    payload: bytes
    source: bytes


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fail(message: str) -> None:
    raise ValueError(message)


def source_identity(path: Path) -> tuple[bytes, str, int]:
    data = path.read_bytes()
    source_hash = digest(data)
    try:
        resource_offset = RESOURCE_OFFSETS[source_hash]
    except KeyError:
        expected = ", ".join(sorted(RESOURCE_OFFSETS))
        raise SystemExit(
            f"refusing unrecognized RetailOS image: expected sha256 in "
            f"{{{expected}}}, got {source_hash}"
        ) from None
    return data, source_hash, resource_offset


def parse_records(data: bytes, resource_offset: int) -> list[ResourceRecord]:
    if resource_offset + RESOURCE_HEADER.size > len(data):
        fail("RetailOS resource header is truncated")
    version, header_length, section_count = RESOURCE_HEADER.unpack_from(
        data, resource_offset
    )
    if (version, header_length, section_count) != (
        RESOURCE_VERSION,
        RESOURCE_HEADER_LENGTH,
        len(EXPECTED_SECTIONS),
    ):
        fail(
            "unexpected RetailOS resource header: "
            f"version={version} length=0x{header_length:x} "
            f"sections={section_count}"
        )

    section_start = resource_offset + RESOURCE_HEADER.size
    sections = tuple(
        RESOURCE_SECTION.unpack_from(
            data, section_start + index * RESOURCE_SECTION.size
        )
        for index in range(section_count)
    )
    if sections != EXPECTED_SECTIONS:
        fail("RetailOS resource-section inventory does not match iPod35 2.0.4")

    data_start = resource_offset + header_length
    if data_start != resource_offset + RESOURCE_HEADER_LENGTH:
        fail("invalid RetailOS resource data offset")
    pamb = next(section for section in sections if section[0] == b"paMB")
    gami = next(section for section in sections if section[0] == b"GAMI")
    if pamb[1] != RESOURCE_COUNT or gami[1] != RESOURCE_COUNT:
        fail("RetailOS paMB/GAMI resource count is incomplete")

    records: list[ResourceRecord] = []
    seen_tokens: set[int] = set()
    previous_end = -1
    for ordinal in range(RESOURCE_COUNT):
        table_offset = resource_offset + pamb[3] + ordinal * RESOURCE_ENTRY.size
        token, relative, source_size = RESOURCE_ENTRY.unpack_from(data, table_offset)
        if token in seen_tokens:
            fail(f"duplicate paMB token 0x{token:08x}")
        seen_tokens.add(token)
        source_offset = data_start + relative
        source_end = source_offset + source_size
        if source_offset < data_start or source_end > len(data):
            fail(f"paMB[{ordinal}] extends outside the RetailOS image")
        if relative < previous_end:
            fail(f"paMB[{ordinal}] overlaps or is out of source order")
        previous_end = relative + source_size
        if source_size < PICTURE_HEADER.size:
            fail(f"paMB[{ordinal}] has a truncated picture header")

        (
            image_format,
            unknown,
            row_bytes,
            flags,
            zeroes,
            height,
            width,
            payload_length,
        ) = PICTURE_HEADER.unpack_from(data, source_offset)
        if zeroes != bytes(8):
            fail(f"paMB[{ordinal}] has nonzero reserved header bytes")
        if image_format not in EXPECTED_FORMAT_COUNTS:
            fail(f"paMB[{ordinal}] uses unsupported format 0x{image_format:04x}")
        if not (0 < width <= 320 and 0 < height <= 240):
            fail(f"paMB[{ordinal}] has invalid dimensions {width}x{height}")
        expected_flags = {
            0x0004: 4,
            0x0008: 8,
            0x0064: 8,
            0x0065: 16,
            0x0565: 16,
            0x1888: 32,
        }[image_format]
        if flags != expected_flags:
            fail(
                f"paMB[{ordinal}] format 0x{image_format:04x} has "
                f"unexpected flags 0x{flags:04x}"
            )
        minimum_row = (width * flags + 7) // 8
        if row_bytes < minimum_row:
            fail(f"paMB[{ordinal}] row is shorter than its visible pixels")
        if PICTURE_HEADER.size + payload_length != source_size:
            fail(
                f"paMB[{ordinal}] size mismatch: header+payload="
                f"{PICTURE_HEADER.size + payload_length}, table={source_size}"
            )
        payload_start = source_offset + PICTURE_HEADER.size
        payload = data[payload_start : payload_start + payload_length]
        source = data[source_offset:source_end]
        records.append(
            ResourceRecord(
                ordinal=ordinal,
                token=token,
                relative_offset=relative,
                source_offset=source_offset,
                source_size=source_size,
                image_format=image_format,
                unknown=unknown,
                row_bytes=row_bytes,
                flags=flags,
                width=width,
                height=height,
                payload=payload,
                source=source,
            )
        )

    # GAMI is the one-for-one image-token ledger for this archive.  Validate
    # both its table tokens and each four-byte payload so no paMB record can be
    # dropped, reordered, or mistaken for an unrelated binary object.
    gami_tokens: list[int] = []
    for ordinal in range(RESOURCE_COUNT):
        table_offset = resource_offset + gami[3] + ordinal * RESOURCE_ENTRY.size
        token, relative, size = RESOURCE_ENTRY.unpack_from(data, table_offset)
        source_offset = data_start + relative
        if size != 4 or source_offset + size > len(data):
            fail(f"GAMI[{ordinal}] is not an intact four-byte image token")
        payload_token = struct.unpack_from("<I", data, source_offset)[0]
        if payload_token != token:
            fail(f"GAMI[{ordinal}] table/payload token mismatch")
        gami_tokens.append(token)
    if gami_tokens != [record.token for record in records]:
        fail("GAMI and paMB image-token ledgers differ")

    counts: dict[int, int] = {}
    for record in records:
        counts[record.image_format] = counts.get(record.image_format, 0) + 1
    if counts != EXPECTED_FORMAT_COUNTS:
        rendered = {f"0x{key:04x}": value for key, value in sorted(counts.items())}
        fail(f"incomplete RetailOS pixel-format inventory: {rendered}")

    claimed_ordinals: set[int] = set()
    for sequence in ANIMATION_SEQUENCES:
        for frame, _label in enumerate(sequence.labels):
            ordinal = sequence.first_ordinal + frame
            if ordinal >= len(records):
                fail(f"{sequence.name} frame {frame} is outside paMB")
            if ordinal in claimed_ordinals:
                fail(f"paMB[{ordinal}] is assigned to two animation sequences")
            claimed_ordinals.add(ordinal)
            record = records[ordinal]
            expected_token = records[sequence.first_ordinal].token + frame
            if record.token != expected_token:
                fail(
                    f"{sequence.name} is not a complete contiguous token run "
                    f"at frame {frame}"
                )

    for name, ordinal in NAMED_ASSETS.items():
        if not (0 <= ordinal < len(records)):
            fail(f"named asset {name} is outside paMB")
    return records


def rgba_from_bgra(value: bytes) -> tuple[int, int, int, int]:
    if len(value) != 4:
        fail("truncated BGRA palette/pixel entry")
    blue, green, red, alpha = value
    return red, green, blue, alpha


def decode_record(record: ResourceRecord) -> Image.Image:
    payload = record.payload
    width = record.width
    height = record.height
    row_bytes = record.row_bytes
    image_format = record.image_format

    if image_format in (0x0004, 0x0008):
        expected = row_bytes * height
        if len(payload) != expected:
            fail(f"paMB[{record.ordinal}] grayscale payload length mismatch")
        pixels = bytearray(width * height)
        write = 0
        for y in range(height):
            row = payload[y * row_bytes : (y + 1) * row_bytes]
            if image_format == 0x0008:
                pixels[write : write + width] = row[:width]
                write += width
            else:
                for x in range(width):
                    packed = row[x // 2]
                    value = packed >> 4 if x % 2 == 0 else packed & 0x0F
                    pixels[write] = value * 17
                    write += 1
        if write != width * height:
            fail(f"paMB[{record.ordinal}] grayscale decode was incomplete")
        return Image.frombytes("L", (width, height), bytes(pixels))

    if image_format == 0x0565:
        expected = row_bytes * height
        if len(payload) != expected:
            fail(f"paMB[{record.ordinal}] RGB565 payload length mismatch")
        pixels = bytearray(width * height * 3)
        write = 0
        for y in range(height):
            row = payload[y * row_bytes : (y + 1) * row_bytes]
            for x in range(width):
                value = struct.unpack_from("<H", row, x * 2)[0]
                red = ((value >> 11) & 0x1F) * 255 // 31
                green = ((value >> 5) & 0x3F) * 255 // 63
                blue = (value & 0x1F) * 255 // 31
                pixels[write : write + 3] = bytes((red, green, blue))
                write += 3
        if write != width * height * 3:
            fail(f"paMB[{record.ordinal}] RGB565 decode was incomplete")
        return Image.frombytes("RGB", (width, height), bytes(pixels))

    if image_format == 0x1888:
        expected = row_bytes * height
        if len(payload) != expected:
            fail(f"paMB[{record.ordinal}] BGRA8888 payload length mismatch")
        pixels = bytearray(width * height * 4)
        write = 0
        for y in range(height):
            row = payload[y * row_bytes : (y + 1) * row_bytes]
            for x in range(width):
                pixels[write : write + 4] = bytes(
                    rgba_from_bgra(row[x * 4 : x * 4 + 4])
                )
                write += 4
        if write != width * height * 4:
            fail(f"paMB[{record.ordinal}] BGRA8888 decode was incomplete")
        return Image.frombytes("RGBA", (width, height), bytes(pixels))

    if image_format not in (0x0064, 0x0065):
        fail(f"paMB[{record.ordinal}] has no exact decoder")
    if len(payload) < 4:
        fail(f"paMB[{record.ordinal}] has no indexed-color palette length")
    palette_length = struct.unpack_from("<I", payload)[0]
    maximum = 0x100 if image_format == 0x0064 else 0x10000
    if not (0 < palette_length <= maximum):
        fail(f"paMB[{record.ordinal}] has invalid palette size {palette_length}")
    palette_end = 4 + palette_length * 4
    index_bytes = 1 if image_format == 0x0064 else 2
    expected = palette_end + row_bytes * height
    if len(payload) != expected:
        fail(
            f"paMB[{record.ordinal}] indexed payload length mismatch: "
            f"expected={expected}, actual={len(payload)}"
        )
    palette = [
        rgba_from_bgra(payload[offset : offset + 4])
        for offset in range(4, palette_end, 4)
    ]
    indexes = payload[palette_end:]
    pixels = bytearray(width * height * 4)
    write = 0
    for y in range(height):
        row = indexes[y * row_bytes : (y + 1) * row_bytes]
        for x in range(width):
            if index_bytes == 1:
                index = row[x]
            else:
                index = struct.unpack_from("<H", row, x * 2)[0]
            if index >= palette_length:
                fail(
                    f"paMB[{record.ordinal}] pixel references palette "
                    f"index {index}/{palette_length}"
                )
            pixels[write : write + 4] = bytes(palette[index])
            write += 4
    if write != width * height * 4:
        fail(f"paMB[{record.ordinal}] indexed decode was incomplete")
    return Image.frombytes("RGBA", (width, height), bytes(pixels))


def image_to_rga(image: Image.Image) -> bytes:
    """Convert without resizing to Rockbox's RGB565-plus-alpha RGA1 form."""
    rgba = image.convert("RGBA")
    if rgba.width > 0xFFFF or rgba.height > 0xFFFF:
        fail(f"RGA dimensions exceed uint16: {rgba.width}x{rgba.height}")
    payload = bytearray()
    pixels = rgba.tobytes()
    for offset in range(0, len(pixels), 4):
        red, green, blue, alpha = pixels[offset : offset + 4]
        rgb565 = (
            ((red >> 3) << 11)
            | ((green >> 2) << 5)
            | (blue >> 3)
        )
        payload.extend(struct.pack("<HB", rgb565, alpha))
    return struct.pack("<4sHH", b"RGA1", rgba.width, rgba.height) + payload


def write_animation_runtime(
    records: list[ResourceRecord], output: Path
) -> tuple[Path, list[dict[str, str | int | bool]]]:
    """Write complete frame atlases and lossless raw packs for draw-time use."""
    frame_dir = output / "frames"
    frame_dir.mkdir()
    inventory: list[dict[str, str | int | bool]] = []

    for sequence in ANIMATION_SEQUENCES:
        sequence_records = [
            records[sequence.first_ordinal + frame]
            for frame in range(len(sequence.labels))
        ]
        images = [
            decode_record(record).convert("RGBA")
            for record in sequence_records
        ]
        first = sequence_records[0]
        if any(
            (record.width, record.height) != (first.width, first.height)
            for record in sequence_records
        ):
            fail(f"{sequence.name} source-frame dimensions are inconsistent")

        atlas = Image.new(
            "RGBA", (first.width, first.height * len(sequence_records))
        )
        for frame, image in enumerate(images):
            atlas.paste(image, (0, frame * first.height))
        atlas_path = frame_dir / f"{sequence.name}.rga"
        atlas_path.write_bytes(image_to_rga(atlas))

        pack_path: Path | None = None
        if first.image_format in (0x0004, 0x0008, 0x0565, 0x1888):
            frame_bytes = len(first.payload)
            if any(
                record.image_format != first.image_format
                or record.row_bytes != first.row_bytes
                or len(record.payload) != frame_bytes
                for record in sequence_records
            ):
                fail(f"{sequence.name} raw source-frame layout is inconsistent")
            pack_path = frame_dir / f"{sequence.name}.iaf"
            header = FRAME_PACK_HEADER.pack(
                b"IAF1",
                1,
                first.width,
                first.height,
                first.row_bytes,
                first.image_format,
                len(sequence_records),
                frame_bytes,
                first.token,
            )
            pack_path.write_bytes(
                header
                + b"".join(record.payload for record in sequence_records)
            )

        inventory.append(
            {
                "name": sequence.name,
                "symbol": sequence.symbol,
                "complete": True,
                "frame_count": len(sequence_records),
                "width": first.width,
                "height": first.height,
                "source_format": f"0x{first.image_format:04x}",
                "first_ordinal": sequence.first_ordinal,
                "first_token": f"0x{first.token:08x}",
                "last_token": f"0x{sequence_records[-1].token:08x}",
                "rga_atlas": str(atlas_path.relative_to(output)),
                "rga_atlas_sha256": digest(atlas_path.read_bytes()),
                "raw_frame_pack": (
                    str(pack_path.relative_to(output)) if pack_path else ""
                ),
                "raw_frame_pack_sha256": (
                    digest(pack_path.read_bytes()) if pack_path else ""
                ),
            }
        )

    runtime_path = output / "runtime-animations.json"
    runtime_path.write_text(
        json.dumps(inventory, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return runtime_path, inventory


def source_runtime_address(source_hash: str, source_offset: int) -> int:
    for file_offset, runtime_address, size in SOURCE_RUNTIME_SEGMENTS[source_hash]:
        if file_offset <= source_offset < file_offset + size:
            return runtime_address + source_offset - file_offset
    fail(f"source offset 0x{source_offset:08x} is outside a loadable segment")


def write_image_symbols(
    source: bytes, source_hash: str, records: list[ResourceRecord], output: Path
) -> tuple[Path, Path, int, int]:
    """Write every image symbol and every provable registry association.

    A mapping is emitted only when the firmware contains an aligned registry
    pair ``[runtime pointer to this exact string, known paMB token]``.  Names
    used only for views/templates remain explicitly unregistered instead of
    being assigned to a bitmap by visual similarity.
    """
    symbols: list[tuple[int, str]] = []
    for match in re.finditer(rb"[ -~]{4,}", source):
        raw = match.group()
        if b"_Image" not in raw and b"Image_" not in raw:
            continue
        symbols.append((match.start(), raw.decode("ascii")))
    if len(symbols) != IMAGE_SYMBOL_COUNT:
        fail(
            f"image-symbol ledger has {len(symbols)} entries, expected "
            f"{IMAGE_SYMBOL_COUNT}"
        )

    path = output / "image-symbols.tsv"
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(
            stream, fieldnames=("index", "source_offset", "symbol"),
            delimiter="\t",
        )
        writer.writeheader()
        for index, (source_offset, symbol) in enumerate(symbols):
            writer.writerow(
                {
                    "index": index,
                    "source_offset": f"0x{source_offset:08x}",
                    "symbol": symbol,
                }
            )

    token_to_record = {record.token: record for record in records}
    registry: dict[int, list[tuple[int, ResourceRecord]]] = {}
    for reference_offset in range(0, len(source) - 7, 4):
        pointer, token = struct.unpack_from("<II", source, reference_offset)
        record = token_to_record.get(token)
        if record is not None:
            registry.setdefault(pointer, []).append((reference_offset, record))

    map_path = output / "image-symbol-map.tsv"
    map_fields = (
        "index", "source_offset", "runtime_address", "symbol", "status",
        "reference_count", "reference_offsets", "ordinal", "token",
        "width", "height", "format", "file",
    )
    mapped_count = 0
    with map_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=map_fields, delimiter="\t")
        writer.writeheader()
        for index, (source_offset, symbol) in enumerate(symbols):
            runtime_address = source_runtime_address(source_hash, source_offset)
            references = registry.get(runtime_address, [])
            ordinals = {record.ordinal for _, record in references}
            if len(ordinals) > 1:
                rendered = ", ".join(str(ordinal) for ordinal in sorted(ordinals))
                fail(
                    f"image symbol {symbol!r} maps to multiple resources: "
                    f"{rendered}"
                )
            row: dict[str, str | int] = {
                "index": index,
                "source_offset": f"0x{source_offset:08x}",
                "runtime_address": f"0x{runtime_address:08x}",
                "symbol": symbol,
                "status": "unregistered",
                "reference_count": 0,
                "reference_offsets": "",
                "ordinal": "",
                "token": "",
                "width": "",
                "height": "",
                "format": "",
                "file": "",
            }
            if ordinals:
                record = references[0][1]
                offsets = sorted({offset for offset, _record in references})
                row.update(
                    {
                        "status": "resource",
                        "reference_count": len(offsets),
                        "reference_offsets": ",".join(
                            f"0x{offset:08x}" for offset in offsets
                        ),
                        "ordinal": record.ordinal,
                        "token": f"0x{record.token:08x}",
                        "width": record.width,
                        "height": record.height,
                        "format": f"0x{record.image_format:04x}",
                        "file": (
                            f"{record.ordinal:03d}-token-{record.token:08x}-"
                            f"{record.width}x{record.height}-"
                            f"{record.image_format:04x}.png"
                        ),
                    }
                )
                mapped_count += 1
            writer.writerow(row)

    if mapped_count != IMAGE_SYMBOL_RESOURCE_COUNT:
        fail(
            f"image registry maps {mapped_count} symbols, expected "
            f"{IMAGE_SYMBOL_RESOURCE_COUNT}"
        )
    return path, map_path, len(symbols), mapped_count


def write_dump(
    source_path: Path,
    source_data: bytes,
    source_hash: str,
    resource_offset: int,
    records: list[ResourceRecord],
    output: Path,
) -> None:
    output.mkdir(parents=True, exist_ok=True)
    resource_dir = output / "resources"
    named_dir = output / "named"
    resource_dir.mkdir()
    named_dir.mkdir()
    rows: list[dict[str, str | int]] = []
    for record in records:
        image = decode_record(record)
        filename = (
            f"{record.ordinal:03d}-token-{record.token:08x}-"
            f"{record.width}x{record.height}-{record.image_format:04x}.png"
        )
        path = output / filename
        image.save(path, format="PNG", optimize=False, compress_level=9)
        rga_filename = f"{record.ordinal:03d}.rga"
        rga_path = resource_dir / rga_filename
        rga_path.write_bytes(image_to_rga(image))
        # Runtime lookup is ordinal based, so consumers never have to guess a
        # token, dimensions, or source format from a filename.
        normalized = image.tobytes()
        rows.append(
            {
                "ordinal": record.ordinal,
                "token": f"0x{record.token:08x}",
                "width": record.width,
                "height": record.height,
                "format": f"0x{record.image_format:04x}",
                "row_bytes": record.row_bytes,
                "flags": f"0x{record.flags:04x}",
                "unknown": f"0x{record.unknown:04x}",
                "source_offset": f"0x{record.source_offset:08x}",
                "source_size": record.source_size,
                "source_record_sha256": digest(record.source),
                "payload_sha256": digest(record.payload),
                "pixel_mode": image.mode,
                "normalized_pixel_sha256": digest(normalized),
                "png_sha256": digest(path.read_bytes()),
                "file": filename,
                "rga_sha256": digest(rga_path.read_bytes()),
                "rga_file": str(rga_path.relative_to(output)),
            }
        )

    manifest_path = output / "manifest.tsv"
    with manifest_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)

    animation_path = output / "animations.tsv"
    with animation_path.open("w", encoding="utf-8", newline="") as stream:
        fieldnames = (
            "sequence",
            "symbol",
            "frame",
            "source_label",
            "ordinal",
            "token",
            "width",
            "height",
            "format",
            "file",
            "normalized_pixel_sha256",
        )
        writer = csv.DictWriter(stream, fieldnames=fieldnames, delimiter="\t")
        writer.writeheader()
        for sequence in ANIMATION_SEQUENCES:
            for frame, label in enumerate(sequence.labels):
                ordinal = sequence.first_ordinal + frame
                row = rows[ordinal]
                writer.writerow(
                    {
                        "sequence": sequence.name,
                        "symbol": sequence.symbol,
                        "frame": frame,
                        "source_label": label,
                        "ordinal": ordinal,
                        "token": row["token"],
                        "width": row["width"],
                        "height": row["height"],
                        "format": row["format"],
                        "file": row["file"],
                        "normalized_pixel_sha256": row[
                            "normalized_pixel_sha256"
                        ],
                    }
                )

    named_path = output / "named-assets.tsv"
    with named_path.open("w", encoding="utf-8", newline="") as stream:
        fieldnames = (
            "name",
            "ordinal",
            "token",
            "width",
            "height",
            "format",
            "file",
            "normalized_pixel_sha256",
            "rga_file",
            "rga_sha256",
            "raw_frame_pack",
            "raw_frame_pack_sha256",
        )
        writer = csv.DictWriter(stream, fieldnames=fieldnames, delimiter="\t")
        writer.writeheader()
        for name, ordinal in NAMED_ASSETS.items():
            row = rows[ordinal]
            record = records[ordinal]
            named_rga = named_dir / f"{name}.rga"
            named_rga.write_bytes((output / str(row["rga_file"])).read_bytes())
            raw_pack = ""
            raw_pack_sha256 = ""
            if record.image_format in (0x0004, 0x0008, 0x0565, 0x1888):
                raw_path = named_dir / f"{name}.iaf"
                raw_path.write_bytes(
                    FRAME_PACK_HEADER.pack(
                        b"IAF1",
                        1,
                        record.width,
                        record.height,
                        record.row_bytes,
                        record.image_format,
                        1,
                        len(record.payload),
                        record.token,
                    )
                    + record.payload
                )
                raw_pack = str(raw_path.relative_to(output))
                raw_pack_sha256 = digest(raw_path.read_bytes())
            writer.writerow(
                {
                    "name": name,
                    "ordinal": ordinal,
                    "token": row["token"],
                    "width": row["width"],
                    "height": row["height"],
                    "format": row["format"],
                    "file": row["file"],
                    "normalized_pixel_sha256": row[
                        "normalized_pixel_sha256"
                    ],
                    "rga_file": str(named_rga.relative_to(output)),
                    "rga_sha256": digest(named_rga.read_bytes()),
                    "raw_frame_pack": raw_pack,
                    "raw_frame_pack_sha256": raw_pack_sha256,
                }
            )

    runtime_path, runtime_inventory = write_animation_runtime(records, output)
    symbol_path, symbol_map_path, symbol_count, symbol_mapped_count = (
        write_image_symbols(
            source_data, source_hash, records, output
        )
    )

    inventory = {
        "schema": 2,
        "source": source_path.name,
        "source_sha256": source_hash,
        "retail_os": "iPod35 2.0.4",
        "resource_offset": f"0x{resource_offset:08x}",
        "resource_header_length": RESOURCE_HEADER_LENGTH,
        "resource_count": len(rows),
        "format_counts": {
            f"0x{key:04x}": value
            for key, value in sorted(EXPECTED_FORMAT_COUNTS.items())
        },
        "animation_sequence_count": len(ANIMATION_SEQUENCES),
        "animation_source_frame_count": sum(
            len(sequence.labels) for sequence in ANIMATION_SEQUENCES
        ),
        "named_component_count": len(NAMED_ASSETS),
        "image_symbol_count": symbol_count,
        "image_symbol_mapped_count": symbol_mapped_count,
        "image_symbol_unregistered_count": symbol_count - symbol_mapped_count,
        "manifest_sha256": digest(manifest_path.read_bytes()),
        "animations_sha256": digest(animation_path.read_bytes()),
        "named_assets_sha256": digest(named_path.read_bytes()),
        "runtime_animations_sha256": digest(runtime_path.read_bytes()),
        "image_symbols_sha256": digest(symbol_path.read_bytes()),
        "image_symbol_map_sha256": digest(symbol_map_path.read_bytes()),
        "runtime_animation_count": len(runtime_inventory),
        "runtime_animation_complete": all(
            bool(item["complete"]) for item in runtime_inventory
        ),
        "complete": len(rows) == RESOURCE_COUNT,
    }
    (output / "inventory.json").write_text(
        json.dumps(inventory, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "source",
        type=Path,
        help="decrypted official iPod35 2.0.4 OS image or exact body",
    )
    parser.add_argument("output", type=Path, help="complete private PNG dump")
    args = parser.parse_args()

    data, source_hash, resource_offset = source_identity(args.source)
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit(
            f"refusing nonempty output directory (prevents stale frames): "
            f"{args.output}"
        )
    try:
        records = parse_records(data, resource_offset)
        # Decode all records before writing any output.  This makes unsupported
        # formats, broken palettes, and truncated frames fail atomically.
        for record in records:
            decode_record(record)
    except (ValueError, struct.error) as error:
        raise SystemExit(f"RetailOS resource extraction failed: {error}") from None
    if len(records) != RESOURCE_COUNT:
        raise SystemExit(
            f"RetailOS resource extraction incomplete: {len(records)}/"
            f"{RESOURCE_COUNT} records"
        )

    write_dump(
        args.source, data, source_hash, resource_offset, records,
        args.output,
    )
    print(
        f"extracted and validated all {len(records)} official iPod35 2.0.4 "
        f"bitmap resources into {args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
