#!/usr/bin/env python3
"""Validate and stage user-owned Tomb Raider I PC data for OpenLara Touch."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import tempfile
from pathlib import Path


TR1_LEVELS = (
    "TITLE.PHD", "GYM.PHD", "LEVEL1.PHD", "LEVEL2.PHD", "LEVEL3A.PHD",
    "LEVEL3B.PHD", "CUT1.PHD", "LEVEL4.PHD", "LEVEL5.PHD", "LEVEL6.PHD",
    "LEVEL7A.PHD", "LEVEL7B.PHD", "CUT2.PHD", "LEVEL8A.PHD",
    "LEVEL8B.PHD", "LEVEL8C.PHD", "LEVEL10A.PHD", "CUT3.PHD",
    "LEVEL10B.PHD", "CUT4.PHD", "LEVEL10C.PHD", "EGYPT.PHD", "CAT.PHD",
    "END.PHD", "END2.PHD",
)
TR1_PC_MAGIC = 0x20
UPSTREAM_COMMIT = "8c40d43834d6d9ce9f174fc3c52b4ccc6502c6ea"


class AssetError(ValueError):
    """Raised when source data cannot be safely staged."""


def find_directory(source: Path, name: str) -> Path | None:
    if source.name.casefold() == name.casefold() and source.is_dir():
        return source
    for child in source.iterdir():
        if child.is_dir() and child.name.casefold() == name.casefold():
            return child
    return None


def index_files(directory: Path) -> dict[str, Path]:
    indexed: dict[str, Path] = {}
    for path in directory.iterdir():
        if path.is_file() and not path.is_symlink():
            name = path.name.upper()
            if name in indexed:
                raise AssetError(f"case-colliding files in {directory}: {name}")
            indexed[name] = path
    return indexed


def validate_level(path: Path) -> None:
    if path.stat().st_size < 4:
        raise AssetError(f"{path.name}: file is truncated")
    with path.open("rb") as stream:
        magic = struct.unpack("<I", stream.read(4))[0]
    if magic != TR1_PC_MAGIC:
        raise AssetError(
            f"{path.name}: expected Tomb Raider I PC PHD magic 0x20, got 0x{magic:X}"
        )


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def copy_tree(source: Path, destination: Path, uppercase: bool,
              manifest_files: list[dict[str, object]], root: Path) -> None:
    for path in sorted(source.rglob("*"), key=lambda item: str(item).casefold()):
        if path.is_symlink():
            raise AssetError(f"symbolic links are not accepted: {path}")
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        parts = [part.upper() for part in relative.parts] if uppercase else list(relative.parts)
        target = destination.joinpath(*parts)
        if target.exists():
            raise AssetError(f"case-normalization collision: {target.relative_to(root)}")
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        manifest_files.append({
            "name": target.relative_to(root).as_posix(),
            "bytes": target.stat().st_size,
            "sha256": digest(target),
            "required": target.parent == root / "DATA" and target.name in TR1_LEVELS,
        })


def stage(source: Path, output: Path) -> dict[str, object]:
    source = source.resolve()
    output = output.resolve()
    if not source.is_dir():
        raise AssetError(f"source directory does not exist: {source}")
    if output.exists():
        raise AssetError(f"output already exists (choose a new path): {output}")

    data_dir = find_directory(source, "DATA")
    if not data_dir:
        raise AssetError("missing Tomb Raider I DATA directory")
    indexed = index_files(data_dir)
    missing = [name for name in TR1_LEVELS if name not in indexed]
    if missing:
        raise AssetError("missing required TR1 level(s): " + ", ".join(missing))
    for name in TR1_LEVELS:
        validate_level(indexed[name])

    output.parent.mkdir(parents=True, exist_ok=True)
    manifest_files: list[dict[str, object]] = []
    with tempfile.TemporaryDirectory(prefix=".openlara-touch-", dir=output.parent) as temp:
        staged = Path(temp) / output.name
        staged.mkdir()
        copy_tree(data_dir, staged / "DATA", True, manifest_files, staged)
        for source_name, output_name, uppercase in (
            ("audio", "audio", False), ("FMV", "FMV", True)
        ):
            optional = find_directory(source, source_name)
            if optional and optional.resolve() != data_dir.resolve():
                copy_tree(optional, staged / output_name, uppercase,
                          manifest_files, staged)

        manifest_files.sort(key=lambda entry: str(entry["name"]).casefold())
        manifest: dict[str, object] = {
            "schema": 2,
            "engine": "OpenLara full",
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
        description="Validate original TR1 PC data and make a device-ready directory."
    )
    parser.add_argument("source", type=Path,
                        help="TR1 installation root, or its DATA directory")
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
