#!/usr/bin/env python3
"""Create sanitized Rockbox metadata for a user-owned Click Wheel game."""

from __future__ import annotations

import argparse
import hashlib
import os
import plistlib
import posixpath
import re
import shutil
import sys
import zipfile
from pathlib import Path
from pathlib import PurePosixPath

try:
    from .ipg_inspect import PackageError, inspect_package
except ImportError:
    from ipg_inspect import PackageError, inspect_package


SAFE_VALUE = re.compile(r"^[\x20-\x7e]*$")
PLATFORM_PREFERENCE = (2, 3, 4, 1)
MAX_ASSET_SIZE = 64 * 1024 * 1024
MAX_ASSET_TOTAL = 256 * 1024 * 1024


class ImportError(ValueError):
    """Raised when a package cannot be normalized safely."""


def _clean_value(value: object, field: str) -> str:
    text = "" if value is None else str(value)
    if not SAFE_VALUE.fullmatch(text) or "\n" in text or "\r" in text:
        raise ImportError(f"unsafe {field} value")
    return text


def _safe_guid(value: object) -> str:
    guid = _clean_value(value, "GUID")
    if not guid or not re.fullmatch(r"[A-Za-z0-9._-]+", guid):
        raise ImportError(f"unsupported GUID: {guid!r}")
    return guid


def _select_platform(report: dict, requested_id: int | None) -> dict:
    platforms = report["platforms"]
    if requested_id is not None:
        matches = [item for item in platforms if item["platform_id"] == requested_id]
        if not matches:
            raise ImportError(f"package has no Platform ID {requested_id}")
        return matches[0]

    for platform_id in PLATFORM_PREFERENCE:
        for item in platforms:
            if item["platform_id"] == platform_id:
                return item
    raise ImportError("package has no supported platform record")


def _write_metadata(path: Path, fields: list[tuple[str, object]]) -> None:
    lines = ["IPODGAMES/1"]
    for key, value in fields:
        lines.append(f"{key}={_clean_value(value, key)}")
    data = ("\n".join(lines) + "\n").encode("utf-8")

    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("wb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


def _read_metadata(path: Path) -> dict[str, str]:
    fields: dict[str, str] = {}
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "IPODGAMES/1":
        raise ImportError(f"invalid generated metadata: {path}")
    for line in lines[1:]:
        key, separator, value = line.partition("=")
        if separator:
            fields[key] = value
    return fields


def _extract_cover(package: Path, manifest_path: str, game_root: Path) -> str:
    manifest_parent = posixpath.dirname(manifest_path)
    candidates = ("iTunesArtwork", "iTunesArtwork.jpg", "iTunesArtwork.png")
    with zipfile.ZipFile(package) as archive:
        data = b""
        for candidate in candidates:
            member = posixpath.join(manifest_parent, candidate)
            try:
                data = archive.read(member)
                break
            except KeyError:
                continue
        if not data:
            return ""

    if data.startswith(b"\xff\xd8\xff"):
        filename = "cover.jpg"
    elif data.startswith(b"\x89PNG\r\n\x1a\n"):
        filename = "cover.png"
    else:
        return ""
    destination = game_root / filename
    destination.write_bytes(data)
    return filename


def _extract_decrypted_executable(
    package: Path,
    manifest_path: str,
    executable_path: str,
    expected_sha256: str,
    destination: Path,
) -> None:
    member = posixpath.join(posixpath.dirname(manifest_path), executable_path)
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    digest = hashlib.sha256()
    with zipfile.ZipFile(package) as archive, archive.open(member) as source:
        with temporary.open("wb") as output:
            while True:
                chunk = source.read(1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
                output.write(chunk)
            output.flush()
            os.fsync(output.fileno())
    if digest.hexdigest() != expected_sha256:
        temporary.unlink(missing_ok=True)
        raise ImportError("decrypted executable hash changed during extraction")
    with temporary.open("rb") as stream:
        if stream.read(4) != b"eapp":
            temporary.unlink(missing_ok=True)
            raise ImportError("package executable does not begin with eapp")
    temporary.replace(destination)


def _extract_assets(
    package: Path,
    manifest_path: str,
    executable_paths: set[str],
    game_root: Path,
) -> int:
    """Extract manifest-declared, non-executable game data under assets/."""

    manifest_parent = posixpath.dirname(manifest_path)
    temporary = game_root / "assets.tmp"
    destination = game_root / "assets"
    shutil.rmtree(temporary, ignore_errors=True)
    temporary.mkdir(parents=True)
    extracted = 0
    total_size = 0

    try:
        with zipfile.ZipFile(package) as archive:
            manifest = plistlib.loads(archive.read(manifest_path))
            for entry in manifest.get("Files", []):
                relative = entry.get("Path") if isinstance(entry, dict) else None
                if not isinstance(relative, str) or relative in executable_paths:
                    continue
                path = PurePosixPath(relative)
                if (path.is_absolute() or not path.parts or
                        any(part in ("", ".", "..") for part in path.parts)):
                    raise ImportError(f"unsafe asset path: {relative!r}")
                member = posixpath.join(manifest_parent, relative)
                info = archive.getinfo(member)
                if info.file_size > MAX_ASSET_SIZE:
                    raise ImportError(f"asset is too large: {relative}")
                total_size += info.file_size
                if total_size > MAX_ASSET_TOTAL:
                    raise ImportError("package assets exceed the import limit")
                output = temporary.joinpath(*path.parts)
                output.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(info) as source, output.open("wb") as target:
                    shutil.copyfileobj(source, target, 1024 * 1024)
                extracted += 1
        if destination.exists():
            shutil.rmtree(destination)
        temporary.replace(destination)
    except Exception:
        shutil.rmtree(temporary, ignore_errors=True)
        raise
    return extracted


def _rebuild_launcher_index(output_root: Path) -> Path:
    game_root = output_root / "games"
    rockbox_root = output_root.parent if output_root.name == "ipodgames" else output_root
    index_root = rockbox_root / "games" / "ipodgames"
    index_root.mkdir(parents=True, exist_ok=True)
    index_path = index_root / "games.tsv"
    rows = ["id\ttitle\tfile\tcover\tfavorite\tlast_played\thaptic_profile"]

    for metadata_path in sorted(game_root.glob("*/game.igame")):
        fields = _read_metadata(metadata_path)
        guid = _safe_guid(fields.get("guid"))
        title = _clean_value(fields.get("name", ""), "name")
        cover_name = fields.get("cover", "")
        device_root = f"/.rockbox/ipodgames/games/{guid}"
        file_path = f"{device_root}/game.igame"
        cover_path = f"{device_root}/{cover_name}" if cover_name else ""
        rows.append(
            "\t".join((guid, title, file_path, cover_path, "0", "", ""))
        )

    temporary = index_path.with_suffix(".tsv.tmp")
    temporary.write_text("\n".join(rows) + "\n", encoding="utf-8")
    temporary.replace(index_path)
    return index_path


def import_package(
    package: Path,
    output_root: Path,
    platform_id: int | None = None,
    decrypted_eapp: Path | None = None,
) -> Path:
    report = inspect_package(package)
    if not report["valid"]:
        errors = "; ".join(report["manifest"]["file_errors"])
        raise ImportError(f"package failed validation: {errors}")

    manifest = report["manifest"]
    platform = _select_platform(report, platform_id)
    executable = platform["executable"]
    guid = _safe_guid(manifest["guid"])
    game_root = output_root.resolve() / "games" / guid
    game_root.mkdir(parents=True, exist_ok=True)

    state = executable["state"]
    local_executable = ""
    cover = _extract_cover(package, manifest["path"], game_root)
    executable_paths = {
        item["executable"]["path"] for item in report["platforms"]
        if item["executable"].get("path")
    }
    asset_count = _extract_assets(
        package, manifest["path"], executable_paths, game_root
    )
    if decrypted_eapp is not None:
        try:
            with decrypted_eapp.open("rb") as stream:
                magic = stream.read(4)
        except OSError as error:
            raise ImportError(f"cannot read decrypted eApp: {error}") from error
        if magic != b"eapp":
            raise ImportError("decrypted executable does not begin with eapp")
        destination = game_root / "executable.eapp"
        shutil.copyfile(decrypted_eapp, destination)
        local_executable = destination.name
        state = "decrypted-eapp"
    elif state == "decrypted-eapp":
        destination = game_root / "executable.eapp"
        _extract_decrypted_executable(
            package,
            manifest["path"],
            executable["path"],
            executable["sha256"],
            destination,
        )
        local_executable = destination.name

    metadata_path = game_root / "game.igame"
    _write_metadata(
        metadata_path,
        [
            ("name", manifest["name"]),
            ("guid", guid),
            ("version", manifest["version"]),
            ("platform_id", platform["platform_id"]),
            ("platform_version", platform["platform_version"]),
            ("build_id", platform["build_id"]),
            ("executable_path", executable["path"]),
            ("executable_size", executable.get("size", "")),
            ("executable_sha256", executable.get("sha256", "")),
            ("executable_state", state),
            ("local_executable", local_executable),
            ("cover", cover),
            ("asset_root", "assets"),
            ("asset_count", asset_count),
            ("package_filename", report["package"]["filename"]),
            ("package_sha256", report["package"]["sha256"]),
        ],
    )
    _rebuild_launcher_index(output_root.resolve())
    return metadata_path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    parser.add_argument("--platform-id", type=int)
    parser.add_argument("--decrypted-eapp", type=Path)
    args = parser.parse_args(argv)

    try:
        path = import_package(
            args.package,
            args.output_root,
            platform_id=args.platform_id,
            decrypted_eapp=args.decrypted_eapp,
        )
    except (PackageError, ImportError) as error:
        print(f"ipg_import: {error}", file=sys.stderr)
        return 2

    print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
