#!/usr/bin/env python3
"""Verify a complete private iPod35 2.0.4 RetailOS resource dump.

The extractor is deliberately fail-closed, but the generated directory can
still be copied, edited, or partially deleted later.  This verifier checks the
complete 598-image ledger, every stable ordinal RGA, all 227 source animation
frames, every runtime atlas/raw pack, and every named runtime component.  It
also verifies the firmware's exact image-symbol registry associations.  It
uses only the Python standard library so packaging can run it without Pillow.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import struct
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any


RESOURCE_COUNT = 598
ANIMATION_FRAME_COUNT = 227
IMAGE_SYMBOL_COUNT = 658
IMAGE_SYMBOL_RESOURCE_COUNT = 566
RESOURCE_HEADER_LENGTH = 0x176D0
SOURCE_OFFSETS = {
    "f4368251a58b2fdc7b46acf3178dae1d24bc1e029736240741015851256c65c4":
        "0x0040b930",
    "7910c276c85a0aa67c4f36fb2d72476791b7d7fe9a21442dae53bff27b48d684":
        "0x0040b130",
}
FORMAT_COUNTS = {
    "0x0004": 183,
    "0x0008": 35,
    "0x0064": 263,
    "0x0065": 43,
    "0x0565": 9,
    "0x1888": 65,
}
SHA256_RE = re.compile(r"[0-9a-f]{64}\Z")
RGA_HEADER = struct.Struct("<4sHH")
IAF_HEADER = struct.Struct("<4s6H2I")


@dataclass(frozen=True)
class Animation:
    name: str
    symbol: str
    first_ordinal: int
    labels: tuple[str, ...]
    width: int
    height: int
    source_format: str
    first_token: int
    row_bytes: int = 0
    frame_bytes: int = 0

    @property
    def frame_count(self) -> int:
        return len(self.labels)

    @property
    def has_raw_pack(self) -> bool:
        return self.frame_bytes != 0


def number_labels(count: int) -> tuple[str, ...]:
    return tuple(str(index) for index in range(count))


ANIMATIONS = (
    Animation(
        "statusbar-white-battery", "StatusBarWhite_Battery_Image_*",
        33, number_labels(23) + ("plug", "charge"), 26, 13,
        "0x0064", 0x0DAD010F,
    ),
    Animation(
        "statusbar-black-battery", "StatusBarBlack_Battery_Image_*",
        58, number_labels(23) + ("plug", "charge"), 26, 13,
        "0x0064", 0x0DAD0128,
    ),
    Animation(
        "clock-hours", "Clock_Hours_Image_*", 152, number_labels(30),
        73, 73, "0x0004", 0x0DAD045C, 48, 3504,
    ),
    Animation(
        "clock-minutes", "Clock_Mins_Image_*", 182, number_labels(16),
        73, 73, "0x0004", 0x0DAD047A, 48, 3504,
    ),
    Animation(
        "clock-seconds", "Clock_Secs_Image_*", 198, number_labels(16),
        73, 73, "0x0004", 0x0DAD048A, 48, 3504,
    ),
    Animation(
        "now-playing-idle-battery", "NowPlaying_Idle_Battery_*_Image",
        318, number_labels(8), 72, 40, "0x0064", 0x0DAD0863,
    ),
    Animation(
        "radio-scanning", "Radio_Scanning_*_Image", 381,
        tuple(str(index) for index in range(1, 8)), 19, 15,
        "0x0064", 0x0DAD0A62,
    ),
    Animation(
        "now-playing-equalizer", "NowPlaying_Animated_Image", 460,
        number_labels(22), 130, 79, "0x0004", 0x0DAD0BD9, 76, 6004,
    ),
    Animation(
        "stopwatch-minutes", "Stopwatch_Mins_Image_*", 487,
        number_labels(30), 18, 18, "0x0004", 0x0DAD0C9B, 20, 360,
    ),
    Animation(
        "stopwatch-seconds", "Stopwatch_Secs_Image_*", 517,
        number_labels(30), 124, 124, "0x0004", 0x0DAD0CB9, 72, 8928,
    ),
    Animation(
        "disk-mode-sync-arrows", "DiskModeImage_SyncArrow*", 564,
        tuple(str(index) for index in range(1, 36, 2)), 76, 76,
        "0x0004", 0x0DAD0DBB, 48, 3648,
    ),
)


# These are the components consumed by the current native surfaces.  Extra
# proven names may be added without weakening this required baseline.
REQUIRED_NAMED_ASSETS = {
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


def fail(message: str) -> None:
    raise ValueError(message)


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def member(root: Path, relative: str, description: str) -> Path:
    if not relative or "\\" in relative:
        fail(f"{description} has an invalid path: {relative!r}")
    candidate = Path(relative)
    if candidate.is_absolute() or ".." in candidate.parts:
        fail(f"{description} escapes the dump: {relative!r}")
    path = root.joinpath(*candidate.parts)
    if path.is_symlink() or not path.is_file():
        fail(f"{description} is missing or not a regular file: {relative}")
    return path


def require_hash(value: Any, description: str) -> str:
    if not isinstance(value, str) or not SHA256_RE.fullmatch(value):
        fail(f"{description} is not a lowercase SHA-256 digest")
    return value


def require_file_hash(
    root: Path, relative: str, expected: Any, description: str
) -> Path:
    path = member(root, relative, description)
    expected_hash = require_hash(expected, f"{description} hash")
    actual = file_hash(path)
    if actual != expected_hash:
        fail(
            f"{description} hash mismatch: expected {expected_hash}, "
            f"got {actual}"
        )
    return path


def read_json(path: Path, expected_type: type) -> Any:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        fail(f"cannot read {path.name}: {error}")
    if not isinstance(value, expected_type):
        fail(f"{path.name} has the wrong top-level type")
    return value


def read_tsv(path: Path, required_fields: set[str]) -> list[dict[str, str]]:
    try:
        with path.open("r", encoding="utf-8", newline="") as stream:
            reader = csv.DictReader(stream, delimiter="\t")
            if reader.fieldnames is None or not required_fields.issubset(
                reader.fieldnames
            ):
                missing = required_fields - set(reader.fieldnames or ())
                fail(f"{path.name} lacks fields: {', '.join(sorted(missing))}")
            rows = list(reader)
    except (OSError, UnicodeError, csv.Error) as error:
        fail(f"cannot read {path.name}: {error}")
    return rows


def integer(value: Any, description: str) -> int:
    if isinstance(value, bool):
        fail(f"{description} is not an integer")
    try:
        return int(value, 0) if isinstance(value, str) else int(value)
    except (TypeError, ValueError):
        fail(f"{description} is not an integer: {value!r}")


def verify_rga(path: Path, width: int, height: int, description: str) -> None:
    expected_size = RGA_HEADER.size + width * height * 3
    if path.stat().st_size != expected_size:
        fail(
            f"{description} size mismatch: expected {expected_size}, "
            f"got {path.stat().st_size}"
        )
    with path.open("rb") as stream:
        header = stream.read(RGA_HEADER.size)
    if len(header) != RGA_HEADER.size:
        fail(f"{description} has a truncated RGA header")
    magic, actual_width, actual_height = RGA_HEADER.unpack(header)
    if (magic, actual_width, actual_height) != (b"RGA1", width, height):
        fail(f"{description} has an invalid RGA header")


def verify_iaf(path: Path, animation: Animation, description: str) -> None:
    expected_size = IAF_HEADER.size + animation.frame_count * animation.frame_bytes
    if path.stat().st_size != expected_size:
        fail(
            f"{description} size mismatch: expected {expected_size}, "
            f"got {path.stat().st_size}"
        )
    with path.open("rb") as stream:
        header = stream.read(IAF_HEADER.size)
    if len(header) != IAF_HEADER.size:
        fail(f"{description} has a truncated IAF header")
    values = IAF_HEADER.unpack(header)
    expected = (
        b"IAF1", 1, animation.width, animation.height,
        animation.row_bytes, int(animation.source_format, 16),
        animation.frame_count, animation.frame_bytes, animation.first_token,
    )
    if values != expected:
        fail(f"{description} has an invalid IAF header")


def verify_single_iaf(
    path: Path, row: dict[str, str], manifest_row: dict[str, str],
    description: str,
) -> None:
    width = integer(manifest_row["width"], f"{description} width")
    height = integer(manifest_row["height"], f"{description} height")
    row_bytes = integer(manifest_row["row_bytes"], f"{description} row bytes")
    frame_bytes = row_bytes * height
    token = integer(manifest_row["token"], f"{description} token")
    expected_size = IAF_HEADER.size + frame_bytes
    if path.stat().st_size != expected_size:
        fail(f"{description} size mismatch")
    with path.open("rb") as stream:
        header = stream.read(IAF_HEADER.size)
    expected = (
        b"IAF1", 1, width, height, row_bytes,
        integer(manifest_row["format"], f"{description} format"),
        1, frame_bytes, token,
    )
    if len(header) != IAF_HEADER.size or IAF_HEADER.unpack(header) != expected:
        fail(f"{description} has an invalid IAF header")
    require_hash(row["raw_frame_pack_sha256"], f"{description} hash")


def verify_manifest(root: Path, inventory: dict[str, Any]) -> list[dict[str, str]]:
    path = require_file_hash(
        root, "manifest.tsv", inventory.get("manifest_sha256"), "manifest"
    )
    fields = {
        "ordinal", "token", "width", "height", "format", "row_bytes",
        "flags", "unknown", "source_offset", "source_size",
        "source_record_sha256", "payload_sha256", "pixel_mode",
        "normalized_pixel_sha256", "png_sha256", "file", "rga_sha256",
        "rga_file",
    }
    rows = read_tsv(path, fields)
    if len(rows) != RESOURCE_COUNT:
        fail(f"manifest has {len(rows)} resources, expected {RESOURCE_COUNT}")

    formats: Counter[str] = Counter()
    tokens: set[int] = set()
    png_names: set[str] = set()
    rga_names: set[str] = set()
    for expected_ordinal, row in enumerate(rows):
        prefix = f"manifest row {expected_ordinal}"
        ordinal = integer(row["ordinal"], f"{prefix} ordinal")
        if ordinal != expected_ordinal:
            fail(f"{prefix} has ordinal {ordinal}")
        token = integer(row["token"], f"{prefix} token")
        if token in tokens:
            fail(f"{prefix} duplicates token 0x{token:08x}")
        tokens.add(token)
        width = integer(row["width"], f"{prefix} width")
        height = integer(row["height"], f"{prefix} height")
        row_bytes = integer(row["row_bytes"], f"{prefix} row bytes")
        if not (0 < width <= 320 and 0 < height <= 240 and row_bytes > 0):
            fail(f"{prefix} has invalid dimensions or row stride")
        image_format = f"0x{integer(row['format'], f'{prefix} format'):04x}"
        if image_format not in FORMAT_COUNTS:
            fail(f"{prefix} has unsupported format {image_format}")
        formats[image_format] += 1

        filename = row["file"]
        expected_prefix = (
            f"{ordinal:03d}-token-{token:08x}-{width}x{height}-"
            f"{image_format[2:]}.png"
        )
        if filename != expected_prefix:
            fail(f"{prefix} has a noncanonical PNG filename")
        png_path = require_file_hash(
            root, filename, row["png_sha256"], f"resource {ordinal} PNG"
        )
        with png_path.open("rb") as stream:
            if stream.read(8) != b"\x89PNG\r\n\x1a\n":
                fail(f"resource {ordinal} does not have a PNG signature")
        png_names.add(filename)

        rga_name = f"resources/{ordinal:03d}.rga"
        if row["rga_file"] != rga_name:
            fail(f"{prefix} has a noncanonical RGA filename")
        rga_path = require_file_hash(
            root, rga_name, row["rga_sha256"], f"resource {ordinal} RGA"
        )
        verify_rga(rga_path, width, height, f"resource {ordinal} RGA")
        rga_names.add(rga_name)

        for field in (
            "source_record_sha256", "payload_sha256",
            "normalized_pixel_sha256",
        ):
            require_hash(row[field], f"{prefix} {field}")

    if dict(formats) != FORMAT_COUNTS:
        fail(f"manifest format inventory differs: {dict(formats)}")
    actual_pngs = {path.name for path in root.glob("*.png") if path.is_file()}
    if actual_pngs != png_names:
        fail("top-level PNG set does not exactly match the manifest")
    resource_dir = root / "resources"
    actual_rgas = {
        str(path.relative_to(root))
        for path in resource_dir.glob("*.rga") if path.is_file()
    }
    if actual_rgas != rga_names:
        fail("ordinal RGA set does not exactly match the manifest")
    return rows


def verify_animations(
    root: Path, inventory: dict[str, Any], manifest: list[dict[str, str]]
) -> None:
    path = require_file_hash(
        root, "animations.tsv", inventory.get("animations_sha256"),
        "animation ledger",
    )
    fields = {
        "sequence", "symbol", "frame", "source_label", "ordinal",
        "token", "width", "height", "format", "file",
        "normalized_pixel_sha256",
    }
    rows = read_tsv(path, fields)
    if len(rows) != ANIMATION_FRAME_COUNT:
        fail(
            f"animation ledger has {len(rows)} frames, expected "
            f"{ANIMATION_FRAME_COUNT}"
        )
    cursor = 0
    for animation in ANIMATIONS:
        for frame, label in enumerate(animation.labels):
            row = rows[cursor]
            ordinal = animation.first_ordinal + frame
            manifest_row = manifest[ordinal]
            expected = {
                "sequence": animation.name,
                "symbol": animation.symbol,
                "frame": str(frame),
                "source_label": label,
                "ordinal": str(ordinal),
                "token": manifest_row["token"],
                "width": str(animation.width),
                "height": str(animation.height),
                "format": animation.source_format,
                "file": manifest_row["file"],
                "normalized_pixel_sha256": manifest_row[
                    "normalized_pixel_sha256"
                ],
            }
            if any(row.get(key) != value for key, value in expected.items()):
                fail(f"animation frame ledger mismatch at row {cursor}")
            if integer(row["token"], "animation token") != (
                animation.first_token + frame
            ):
                fail(f"{animation.name} token run breaks at frame {frame}")
            cursor += 1

    runtime_path = require_file_hash(
        root, "runtime-animations.json",
        inventory.get("runtime_animations_sha256"),
        "runtime animation inventory",
    )
    runtime = read_json(runtime_path, list)
    if len(runtime) != len(ANIMATIONS):
        fail("runtime animation inventory is incomplete")
    expected_rgas: set[str] = set()
    expected_iafs: set[str] = set()
    for index, (item, animation) in enumerate(zip(runtime, ANIMATIONS)):
        if not isinstance(item, dict):
            fail(f"runtime animation {index} is not an object")
        expected_values = {
            "name": animation.name,
            "symbol": animation.symbol,
            "complete": True,
            "frame_count": animation.frame_count,
            "width": animation.width,
            "height": animation.height,
            "source_format": animation.source_format,
            "first_ordinal": animation.first_ordinal,
            "first_token": f"0x{animation.first_token:08x}",
            "last_token": (
                f"0x{animation.first_token + animation.frame_count - 1:08x}"
            ),
        }
        if any(item.get(key) != value for key, value in expected_values.items()):
            fail(f"runtime metadata mismatch for {animation.name}")

        rga_name = f"frames/{animation.name}.rga"
        if item.get("rga_atlas") != rga_name:
            fail(f"{animation.name} has a noncanonical RGA atlas path")
        rga_path = require_file_hash(
            root, rga_name, item.get("rga_atlas_sha256"),
            f"{animation.name} RGA atlas",
        )
        verify_rga(
            rga_path, animation.width,
            animation.height * animation.frame_count,
            f"{animation.name} RGA atlas",
        )
        expected_rgas.add(rga_name)

        if animation.has_raw_pack:
            iaf_name = f"frames/{animation.name}.iaf"
            if item.get("raw_frame_pack") != iaf_name:
                fail(f"{animation.name} has a noncanonical IAF path")
            iaf_path = require_file_hash(
                root, iaf_name, item.get("raw_frame_pack_sha256"),
                f"{animation.name} IAF pack",
            )
            verify_iaf(iaf_path, animation, f"{animation.name} IAF pack")
            expected_iafs.add(iaf_name)
        elif item.get("raw_frame_pack") or item.get("raw_frame_pack_sha256"):
            fail(f"{animation.name} unexpectedly claims a raw frame pack")

    frame_dir = root / "frames"
    actual_rgas = {
        str(path.relative_to(root))
        for path in frame_dir.glob("*.rga") if path.is_file()
    }
    actual_iafs = {
        str(path.relative_to(root))
        for path in frame_dir.glob("*.iaf") if path.is_file()
    }
    if actual_rgas != expected_rgas or actual_iafs != expected_iafs:
        fail("runtime frame-pack directory is incomplete or has stale files")


def verify_named_assets(
    root: Path, inventory: dict[str, Any], manifest: list[dict[str, str]]
) -> None:
    path = require_file_hash(
        root, "named-assets.tsv", inventory.get("named_assets_sha256"),
        "named asset ledger",
    )
    fields = {
        "name", "ordinal", "token", "width", "height", "format", "file",
        "normalized_pixel_sha256", "rga_file", "rga_sha256",
        "raw_frame_pack", "raw_frame_pack_sha256",
    }
    rows = read_tsv(path, fields)
    if len(rows) != integer(
        inventory.get("named_component_count"), "named component count"
    ):
        fail("named component count does not match its ledger")
    by_name: dict[str, dict[str, str]] = {}
    expected_files: set[str] = set()
    for index, row in enumerate(rows):
        name = row["name"]
        if not re.fullmatch(r"[a-z0-9-]+", name) or name in by_name:
            fail(f"named asset row {index} has an invalid or duplicate name")
        ordinal = integer(row["ordinal"], f"named asset {name} ordinal")
        if not (0 <= ordinal < len(manifest)):
            fail(f"named asset {name} has an invalid ordinal")
        source = manifest[ordinal]
        for field in (
            "token", "width", "height", "format", "file",
            "normalized_pixel_sha256",
        ):
            if row[field] != source[field]:
                fail(f"named asset {name} disagrees with manifest field {field}")

        rga_name = f"named/{name}.rga"
        if row["rga_file"] != rga_name:
            fail(f"named asset {name} has a noncanonical RGA path")
        rga_path = require_file_hash(
            root, rga_name, row["rga_sha256"], f"named asset {name} RGA"
        )
        width = integer(row["width"], f"named asset {name} width")
        height = integer(row["height"], f"named asset {name} height")
        verify_rga(rga_path, width, height, f"named asset {name} RGA")
        if row["rga_sha256"] != source["rga_sha256"]:
            fail(f"named asset {name} is not the exact ordinal RGA")
        expected_files.add(rga_name)

        has_raw = row["format"] in {"0x0004", "0x0008", "0x0565", "0x1888"}
        if has_raw:
            iaf_name = f"named/{name}.iaf"
            if row["raw_frame_pack"] != iaf_name:
                fail(f"named asset {name} has a noncanonical IAF path")
            iaf_path = require_file_hash(
                root, iaf_name, row["raw_frame_pack_sha256"],
                f"named asset {name} IAF",
            )
            verify_single_iaf(
                iaf_path, row, source, f"named asset {name} IAF"
            )
            expected_files.add(iaf_name)
        elif row["raw_frame_pack"] or row["raw_frame_pack_sha256"]:
            fail(f"indexed named asset {name} unexpectedly has an IAF pack")
        by_name[name] = row

    for name, ordinal in REQUIRED_NAMED_ASSETS.items():
        if name not in by_name:
            fail(f"required named runtime asset is missing: {name}")
        if integer(by_name[name]["ordinal"], f"named asset {name} ordinal") != ordinal:
            fail(f"required named runtime asset {name} has the wrong ordinal")

    named_dir = root / "named"
    actual_files = {
        str(path.relative_to(root))
        for path in named_dir.iterdir() if path.is_file()
    }
    if actual_files != expected_files:
        fail("named runtime directory is incomplete or has stale files")


def verify_image_symbols(root: Path, inventory: dict[str, Any]) -> None:
    path = require_file_hash(
        root, "image-symbols.tsv", inventory.get("image_symbols_sha256"),
        "image symbol ledger",
    )
    rows = read_tsv(path, {"index", "source_offset", "symbol"})
    if len(rows) != IMAGE_SYMBOL_COUNT:
        fail(
            f"image symbol ledger has {len(rows)} entries, expected "
            f"{IMAGE_SYMBOL_COUNT}"
        )
    previous_offset = -1
    for expected_index, row in enumerate(rows):
        if integer(row["index"], "image symbol index") != expected_index:
            fail(f"image symbol ledger index breaks at row {expected_index}")
        offset = integer(row["source_offset"], "image symbol source offset")
        if offset <= previous_offset:
            fail("image symbol source offsets are not strictly increasing")
        previous_offset = offset
        symbol = row["symbol"]
        if "_Image" not in symbol and "Image_" not in symbol:
            fail(f"image symbol row {expected_index} is not image-related")
        if any(
            ord(character) < 0x20 or ord(character) > 0x7e
            for character in symbol
        ):
            fail(f"image symbol row {expected_index} is not printable ASCII")


def verify_image_symbol_map(
    root: Path, inventory: dict[str, Any], manifest: list[dict[str, str]]
) -> None:
    symbols_path = member(root, "image-symbols.tsv", "image symbol ledger")
    symbols = read_tsv(symbols_path, {"index", "source_offset", "symbol"})
    path = require_file_hash(
        root, "image-symbol-map.tsv",
        inventory.get("image_symbol_map_sha256"),
        "image symbol registry map",
    )
    fields = {
        "index", "source_offset", "runtime_address", "symbol", "status",
        "reference_count", "reference_offsets", "ordinal", "token",
        "width", "height", "format", "file",
    }
    rows = read_tsv(path, fields)
    if len(rows) != IMAGE_SYMBOL_COUNT:
        fail(
            f"image symbol registry map has {len(rows)} entries, expected "
            f"{IMAGE_SYMBOL_COUNT}"
        )

    mapped = 0
    for expected_index, (row, symbol_row) in enumerate(zip(rows, symbols)):
        prefix = f"image symbol registry row {expected_index}"
        for field in ("index", "source_offset", "symbol"):
            if row[field] != symbol_row[field]:
                fail(f"{prefix} disagrees with the image symbol ledger")
        runtime_address = integer(
            row["runtime_address"], f"{prefix} runtime address"
        )
        if runtime_address <= 0 or row["runtime_address"] != (
            f"0x{runtime_address:08x}"
        ):
            fail(f"{prefix} has a noncanonical runtime address")

        reference_count = integer(
            row["reference_count"], f"{prefix} reference count"
        )
        offsets = row["reference_offsets"].split(",") if row[
            "reference_offsets"
        ] else []
        if len(offsets) != reference_count:
            fail(f"{prefix} reference count disagrees with its offsets")
        parsed_offsets = [
            integer(value, f"{prefix} reference offset") for value in offsets
        ]
        if parsed_offsets != sorted(set(parsed_offsets)) or any(
            value < 0 or value % 4 for value in parsed_offsets
        ):
            fail(f"{prefix} has invalid registry reference offsets")
        if any(value != f"0x{parsed:08x}" for value, parsed in zip(
            offsets, parsed_offsets
        )):
            fail(f"{prefix} has noncanonical registry reference offsets")

        resource_fields = ("ordinal", "token", "width", "height", "format", "file")
        if row["status"] == "unregistered":
            if reference_count != 0 or any(row[field] for field in resource_fields):
                fail(f"{prefix} claims data for an unregistered symbol")
            continue
        if row["status"] != "resource" or reference_count <= 0:
            fail(f"{prefix} has an invalid registry status")

        ordinal = integer(row["ordinal"], f"{prefix} ordinal")
        if not (0 <= ordinal < len(manifest)):
            fail(f"{prefix} has an invalid resource ordinal")
        resource = manifest[ordinal]
        for field in ("token", "width", "height", "format", "file"):
            if row[field] != resource[field]:
                fail(f"{prefix} disagrees with manifest field {field}")
        mapped += 1

    if mapped != IMAGE_SYMBOL_RESOURCE_COUNT:
        fail(
            f"image registry maps {mapped} symbols, expected "
            f"{IMAGE_SYMBOL_RESOURCE_COUNT}"
        )


def verify_dump(root: Path) -> None:
    if root.is_symlink() or not root.is_dir():
        fail(f"dump is missing or not a directory: {root}")
    inventory_path = member(root, "inventory.json", "inventory")
    inventory = read_json(inventory_path, dict)
    source_hash = inventory.get("source_sha256")
    if source_hash not in SOURCE_OFFSETS:
        fail("inventory does not identify a hash-pinned iPod35 2.0.4 image")
    expected_inventory = {
        "schema": 2,
        "retail_os": "iPod35 2.0.4",
        "resource_offset": SOURCE_OFFSETS[source_hash],
        "resource_header_length": RESOURCE_HEADER_LENGTH,
        "resource_count": RESOURCE_COUNT,
        "format_counts": FORMAT_COUNTS,
        "animation_sequence_count": len(ANIMATIONS),
        "animation_source_frame_count": ANIMATION_FRAME_COUNT,
        "image_symbol_count": IMAGE_SYMBOL_COUNT,
        "image_symbol_mapped_count": IMAGE_SYMBOL_RESOURCE_COUNT,
        "image_symbol_unregistered_count": (
            IMAGE_SYMBOL_COUNT - IMAGE_SYMBOL_RESOURCE_COUNT
        ),
        "runtime_animation_count": len(ANIMATIONS),
        "runtime_animation_complete": True,
        "complete": True,
    }
    for key, value in expected_inventory.items():
        if inventory.get(key) != value:
            fail(f"inventory field {key} differs from the complete-port contract")
    if sum(animation.frame_count for animation in ANIMATIONS) != (
        ANIMATION_FRAME_COUNT
    ):
        fail("internal animation-frame contract is inconsistent")

    manifest = verify_manifest(root, inventory)
    verify_animations(root, inventory, manifest)
    verify_named_assets(root, inventory, manifest)
    verify_image_symbols(root, inventory)
    verify_image_symbol_map(root, inventory, manifest)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path, help="RetailOS extraction directory")
    args = parser.parse_args()
    try:
        verify_dump(args.dump)
    except ValueError as error:
        raise SystemExit(f"RetailOS resource verification failed: {error}") from None
    print(
        "PASS: verified 598/598 RetailOS bitmaps, 598/598 ordinal RGAs, "
        "11/11 animation sequences, and 227/227 source frames"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
