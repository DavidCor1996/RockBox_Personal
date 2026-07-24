#!/usr/bin/env python3
"""Inspect an iPod Click Wheel game package without extracting secrets."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import plistlib
import posixpath
import sys
import zipfile
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Any, BinaryIO


MAX_ENTRIES = 100_000
MAX_MANIFEST_SIZE = 4 * 1024 * 1024
ENTROPY_SAMPLE_SIZE = 64 * 1024


class PackageError(ValueError):
    """Raised when a package is malformed or unsafe to inspect."""


def _safe_member_name(name: str) -> str:
    if not name or "\\" in name or "\x00" in name:
        raise PackageError(f"unsafe package member name: {name!r}")

    path = PurePosixPath(name)
    if path.is_absolute() or any(part in ("", ".", "..") for part in path.parts):
        raise PackageError(f"unsafe package member path: {name!r}")

    return str(path)


def _manifest_member(names: list[str]) -> str:
    manifests = [name for name in names if name.endswith("Manifest.plist")]
    if not manifests:
        raise PackageError("package does not contain Manifest.plist")

    manifests.sort(key=lambda name: (name.count("/"), len(name), name))
    shallowest = manifests[0].count("/")
    candidates = [name for name in manifests if name.count("/") == shallowest]
    if len(candidates) != 1:
        raise PackageError("package contains multiple equally likely manifests")

    return candidates[0]


def _member_path(manifest_name: str, relative_name: str) -> str:
    relative_name = _safe_member_name(relative_name)
    base = posixpath.dirname(manifest_name)
    name = posixpath.join(base, relative_name) if base else relative_name
    return _safe_member_name(name)


def _sha256_stream(stream: BinaryIO) -> str:
    digest = hashlib.sha256()
    while True:
        chunk = stream.read(1024 * 1024)
        if not chunk:
            break
        digest.update(chunk)
    return digest.hexdigest()


def _entropy(data: bytes) -> float:
    if not data:
        return 0.0

    counts = Counter(data)
    length = len(data)
    return -sum(
        (count / length) * math.log2(count / length)
        for count in counts.values()
    )


def _require_mapping(value: Any, description: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise PackageError(f"{description} must be a dictionary")
    return value


def _require_list(value: Any, description: str) -> list[Any]:
    if not isinstance(value, list):
        raise PackageError(f"{description} must be an array")
    return value


def inspect_package(package_path: Path) -> dict[str, Any]:
    """Return a deterministic, JSON-compatible package report."""

    package_path = package_path.resolve()
    if not package_path.is_file():
        raise PackageError(f"package not found: {package_path}")

    try:
        archive = zipfile.ZipFile(package_path)
    except (OSError, zipfile.BadZipFile) as error:
        raise PackageError(f"invalid ZIP package: {error}") from error

    with archive:
        infos = archive.infolist()
        if len(infos) > MAX_ENTRIES:
            raise PackageError(f"package has too many entries: {len(infos)}")

        safe_names: list[str] = []
        info_by_name: dict[str, zipfile.ZipInfo] = {}
        for info in infos:
            safe_name = _safe_member_name(info.filename.rstrip("/"))
            if safe_name in info_by_name:
                raise PackageError(f"duplicate package member: {safe_name}")
            safe_names.append(safe_name)
            info_by_name[safe_name] = info

        manifest_name = _manifest_member(safe_names)
        manifest_info = info_by_name[manifest_name]
        if manifest_info.file_size > MAX_MANIFEST_SIZE:
            raise PackageError("Manifest.plist is unreasonably large")

        try:
            manifest = _require_mapping(
                plistlib.loads(archive.read(manifest_info)), "Manifest.plist"
            )
        except (plistlib.InvalidFileException, ValueError, TypeError) as error:
            raise PackageError(f"invalid Manifest.plist: {error}") from error

        files = _require_list(manifest.get("Files", []), "Files")
        platforms = _require_list(manifest.get("Platforms", []), "Platforms")
        manifest_files: dict[str, dict[str, Any]] = {}
        duplicate_manifest_paths: list[str] = []
        file_errors: list[str] = []

        for index, raw_entry in enumerate(files):
            entry = _require_mapping(raw_entry, f"Files[{index}]")
            relative_name = entry.get("Path")
            if not isinstance(relative_name, str):
                raise PackageError(f"Files[{index}].Path must be a string")
            member_name = _member_path(manifest_name, relative_name)
            if relative_name in manifest_files:
                if manifest_files[relative_name] != entry:
                    raise PackageError(
                        f"conflicting duplicate manifest path: {relative_name}"
                    )
                duplicate_manifest_paths.append(relative_name)
                continue
            manifest_files[relative_name] = entry

            info = info_by_name.get(member_name)
            if info is None:
                file_errors.append(f"missing file: {relative_name}")
                continue

            expected_size = entry.get("Size")
            if isinstance(expected_size, int) and expected_size != info.file_size:
                file_errors.append(
                    f"size mismatch: {relative_name}: "
                    f"manifest={expected_size} zip={info.file_size}"
                )

        executable_cache: dict[str, dict[str, Any]] = {}
        platform_reports: list[dict[str, Any]] = []

        for index, raw_platform in enumerate(platforms):
            platform = _require_mapping(raw_platform, f"Platforms[{index}]")
            executable_path = platform.get("ExecutablePath")
            if not isinstance(executable_path, str):
                raise PackageError(
                    f"Platforms[{index}].ExecutablePath must be a string"
                )

            if executable_path not in executable_cache:
                member_name = _member_path(manifest_name, executable_path)
                info = info_by_name.get(member_name)
                if info is None:
                    executable_cache[executable_path] = {
                        "path": executable_path,
                        "present": False,
                    }
                else:
                    with archive.open(info) as stream:
                        sample = stream.read(ENTROPY_SAMPLE_SIZE)
                    with archive.open(info) as stream:
                        digest = _sha256_stream(stream)

                    file_entry = manifest_files.get(executable_path, {})
                    drm = bool(file_entry.get("DRM", False))
                    magic = sample[:4]
                    state = "decrypted-eapp" if magic == b"eapp" else (
                        "encrypted" if drm else "unknown"
                    )
                    executable_cache[executable_path] = {
                        "path": executable_path,
                        "present": True,
                        "size": info.file_size,
                        "sha256": digest,
                        "magic_hex": magic.hex(),
                        "entropy_sample_bytes": len(sample),
                        "entropy_bits_per_byte": round(_entropy(sample), 5),
                        "drm": drm,
                        "verify": bool(file_entry.get("Verify", False)),
                        "state": state,
                    }

            platform_reports.append(
                {
                    "platform_id": platform.get("PlatformID"),
                    "platform_version": platform.get("PlatformVersion"),
                    "build_id": platform.get("BuildID"),
                    "declared_size": platform.get("Size"),
                    "launching_artwork": platform.get("LaunchingArtwork"),
                    "executable": executable_cache[executable_path],
                }
            )

        with package_path.open("rb") as stream:
            package_sha256 = _sha256_stream(stream)

        return {
            "format": "ipod-clickwheel-package-report-v1",
            "package": {
                "filename": package_path.name,
                "size": package_path.stat().st_size,
                "sha256": package_sha256,
                "entry_count": len(infos),
                "uncompressed_size": sum(info.file_size for info in infos),
            },
            "manifest": {
                "path": manifest_name,
                "name": manifest.get("Name"),
                "guid": manifest.get("GUID"),
                "version": manifest.get("Version"),
                "build_identifier": manifest.get("BuildIdentifier"),
                "file_count": len(files),
                "duplicate_file_paths": duplicate_manifest_paths,
                "file_errors": file_errors,
            },
            "platforms": platform_reports,
            "valid": not file_errors and bool(platform_reports),
        }


def _print_text(report: dict[str, Any]) -> None:
    package = report["package"]
    manifest = report["manifest"]
    print(f"Package: {package['filename']}")
    print(
        f"Game: {manifest.get('name')} "
        f"(GUID {manifest.get('guid')}, version {manifest.get('version')})"
    )
    print(f"SHA-256: {package['sha256']}")
    print(f"Valid: {'yes' if report['valid'] else 'no'}")
    for platform in report["platforms"]:
        executable = platform["executable"]
        print(
            "Platform "
            f"{platform.get('platform_id')} build {platform.get('build_id')}: "
            f"{executable.get('path')} ({executable.get('state', 'missing')})"
        )
    for error in manifest["file_errors"]:
        print(f"Error: {error}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="path to an .ipg package")
    parser.add_argument(
        "--json", action="store_true", help="write the full report as JSON"
    )
    args = parser.parse_args(argv)

    try:
        report = inspect_package(args.package)
    except PackageError as error:
        print(f"ipg_inspect: {error}", file=sys.stderr)
        return 2

    if args.json:
        json.dump(report, sys.stdout, indent=2, sort_keys=True)
        print()
    else:
        _print_text(report)
    return 0 if report["valid"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
