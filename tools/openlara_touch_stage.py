#!/usr/bin/env python3
"""Validate and stage user-owned OpenLara Touch data files."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import tempfile
from pathlib import Path


REQUIRED = ("TITLE.PKD", "GYM.PKD", "LEVEL1.PKD", "LEVEL2.PKD")
OPTIONAL = ("TITLE.SCR", "TRACKS.AD4")
LEVEL_MIN_SIZE = 172
LEVEL_MAX_SIZE = 8 * 1024 * 1024
MAX_ROOMS = 139
MAX_TEXTURES = 1536
MAX_SPRITES = 180
MAX_ITEMS = 256
MAX_CAMERAS = 16
UPSTREAM_COMMIT = "8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea"


class AssetError(ValueError):
    """Raised when an input asset cannot be safely used by the frontend."""


def validate_pkd(path: Path) -> None:
    size = path.stat().st_size
    if size < LEVEL_MIN_SIZE or size > LEVEL_MAX_SIZE:
        raise AssetError(f"{path.name}: size {size} is outside the supported range")

    with path.open("rb") as stream:
        header = stream.read(LEVEL_MIN_SIZE)
    counts = struct.unpack_from("<14H", header, 4)
    if not counts[0] or not counts[1]:
        raise AssetError(f"{path.name}: tile and room counts must be nonzero")
    limits = ((1, MAX_ROOMS, "rooms"), (8, MAX_TEXTURES, "textures"),
              (9, MAX_SPRITES, "sprites"), (10, MAX_ITEMS, "items"),
              (11, MAX_CAMERAS, "cameras"))
    for index, limit, label in limits:
        if counts[index] > limit:
            raise AssetError(
                f"{path.name}: {label} count {counts[index]} exceeds {limit}"
            )

    offsets = struct.unpack_from("<35I", header, 32)
    for index, offset in enumerate(offsets):
        if offset >= size:
            raise AssetError(f"{path.name}: offset {index} is outside the file")
    room_bytes = counts[1] * 56
    if room_bytes > size - offsets[3]:
        raise AssetError(f"{path.name}: room table extends past end of file")


def validate_title(path: Path) -> None:
    if path.stat().st_size not in (320 * 240, 240 * 160):
        raise AssetError(f"{path.name}: expected a 320x240 or 240x160 indexed image")


def validate_tracks(path: Path) -> None:
    size = path.stat().st_size
    header_size = 14 * 2 * 4
    if size < header_size:
        raise AssetError(f"{path.name}: track table is truncated")
    with path.open("rb") as stream:
        header = stream.read(header_size)
    info = struct.unpack("<28i", header)
    for track in (4, 5, 13):
        offset, length = info[track * 2:track * 2 + 2]
        if offset < 0 or length < 0 or offset > size or length > size - offset:
            raise AssetError(f"{path.name}: track {track} extends past end of file")


def find_assets(source: Path) -> dict[str, Path]:
    found: dict[str, Path] = {}
    wanted = set(REQUIRED + OPTIONAL)
    for directory in (source, source / "levels"):
        if not directory.is_dir():
            continue
        for candidate in directory.iterdir():
            name = candidate.name.upper()
            if candidate.is_file() and name in wanted and name not in found:
                found[name] = candidate
    missing = [name for name in REQUIRED if name not in found]
    if missing:
        raise AssetError("missing required file(s): " + ", ".join(missing))
    return found


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def stage(source: Path, output: Path) -> dict[str, object]:
    source = source.resolve()
    output = output.resolve()
    if not source.is_dir():
        raise AssetError(f"source directory does not exist: {source}")
    if output.exists():
        raise AssetError(f"output already exists (choose a new path): {output}")

    assets = find_assets(source)
    for name in REQUIRED:
        validate_pkd(assets[name])
    if "TITLE.SCR" in assets:
        validate_title(assets["TITLE.SCR"])
    if "TRACKS.AD4" in assets:
        validate_tracks(assets["TRACKS.AD4"])

    output.parent.mkdir(parents=True, exist_ok=True)
    manifest_files = []
    with tempfile.TemporaryDirectory(prefix=".openlara-touch-", dir=output.parent) as temp:
        staged = Path(temp) / output.name
        staged.mkdir()
        for name in REQUIRED + OPTIONAL:
            if name not in assets:
                continue
            destination = staged / name
            shutil.copy2(assets[name], destination)
            manifest_files.append({
                "name": name,
                "bytes": destination.stat().st_size,
                "sha256": digest(destination),
                "required": name in REQUIRED,
            })
        manifest: dict[str, object] = {
            "schema": 1,
            "engine_upstream_commit": UPSTREAM_COMMIT,
            "destination": "/var/mobile/Media/OpenLara",
            "files": manifest_files,
        }
        (staged / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        os.replace(staged, output)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate converted OpenLara assets and make a device-ready directory."
    )
    parser.add_argument("source", type=Path, help="converted asset directory")
    parser.add_argument("output", type=Path, help="new staging directory")
    args = parser.parse_args()
    try:
        manifest = stage(args.source, args.output)
    except (AssetError, OSError) as error:
        parser.exit(1, f"openlara_touch_stage: {error}\n")
    print(f"staged {len(manifest['files'])} file(s) in {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
