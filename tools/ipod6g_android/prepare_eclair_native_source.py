#!/usr/bin/env python3
"""Prepare the pinned, native-only Android 2.0 source tree without networking."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import posixpath
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tarfile


SCRIPT_DIR = Path(__file__).resolve().parent
ECLAIR_DIR = SCRIPT_DIR / "eclair"
LOCK_PATH = ECLAIR_DIR / "source-lock.json"
PATCH_PATH = ECLAIR_DIR / "patches" / "aosp-native-only-modern-host.patch"
PROBE_DIR = ECLAIR_DIR / "probe"
LAUNCHER_DIR = ECLAIR_DIR / "launcher"
ASHMEM_OVERLAY = ECLAIR_DIR / "overlay" / "ashmem-dev.c"
MARKER_NAME = ".ipod6g-eclair-source.json"


class PreparationError(RuntimeError):
    """A source archive or destination failed a safety check."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def sha256_source_tree(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(root.rglob("*"), key=lambda item: item.as_posix()):
        relative = path.relative_to(root)
        if relative.parts[0] == "out" or relative.as_posix() == MARKER_NAME:
            continue
        if path.is_symlink() or not (path.is_dir() or path.is_file()):
            raise PreparationError(f"unsafe source-tree entry: {relative}")
        kind = b"d" if path.is_dir() else b"f"
        mode = stat.S_IMODE(path.stat().st_mode)
        digest.update(kind + b"\0" + relative.as_posix().encode() + b"\0")
        digest.update(f"{mode:o}".encode() + b"\0")
        if path.is_file():
            with path.open("rb") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(block)
    return digest.hexdigest()


def load_lock() -> dict:
    with LOCK_PATH.open("r", encoding="utf-8") as stream:
        lock = json.load(stream)
    if lock.get("schema") != 1 or lock.get("aosp_tag") != "android-2.0_r1":
        raise PreparationError("unsupported Eclair source lock")
    return lock


def validate_destination(destination: Path) -> None:
    resolved = destination.resolve()
    if resolved == Path("/") or resolved == Path.home().resolve():
        raise PreparationError("refusing broad source destination")
    if destination.exists() and any(destination.iterdir()):
        raise PreparationError("source destination must be absent or empty")


def checked_members(archive: tarfile.TarFile, strip_components: int):
    for member in archive.getmembers():
        source = PurePosixPath(member.name)
        if source.is_absolute() or ".." in source.parts:
            raise PreparationError(f"unsafe archive path: {member.name}")
        if member.islnk() or member.isdev() or member.isfifo():
            raise PreparationError(f"unsupported archive entry: {member.name}")
        if member.issym():
            link = PurePosixPath(member.linkname)
            if link.is_absolute():
                raise PreparationError(f"unsafe archive link: {member.name}")
            resolved_link = PurePosixPath(
                posixpath.normpath((source.parent / link).as_posix())
            )
            if resolved_link.is_absolute() or ".." in resolved_link.parts:
                raise PreparationError(f"unsafe archive link: {member.name}")
            try:
                link_target = archive.getmember(resolved_link.as_posix())
            except KeyError as error:
                raise PreparationError(
                    f"missing archive link target: {member.name}"
                ) from error
            if not link_target.isfile():
                raise PreparationError(
                    f"unsupported archive link target: {member.name}"
                )
        parts = source.parts[strip_components:]
        if not parts:
            continue
        target = PurePosixPath(*parts)
        if target.is_absolute() or ".." in target.parts:
            raise PreparationError(f"unsafe stripped archive path: {member.name}")
        yield member, target


def extract_archive(archive_path: Path, destination: Path,
                    strip_components: int) -> None:
    destination.mkdir(parents=True, exist_ok=False)
    with tarfile.open(archive_path, "r:gz") as archive:
        for member, relative in checked_members(archive, strip_components):
            target = destination.joinpath(*relative.parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            if not (member.isfile() or member.issym()):
                raise PreparationError(f"unsupported tar type: {member.name}")
            target.parent.mkdir(parents=True, exist_ok=True)
            source = archive.extractfile(member)
            if source is None:
                raise PreparationError(f"cannot read archive entry: {member.name}")
            # Gitiles archives can repeat an identical path.  The archive is
            # checksum-pinned, so deterministic last-entry-wins extraction is
            # both safe and consistent with tar(1).
            with source, target.open("wb") as output:
                shutil.copyfileobj(source, output)
            os.chmod(target, stat.S_IMODE(member.mode) & 0o777)


def prepare(archive_dir: Path, destination: Path) -> dict:
    lock = load_lock()
    validate_destination(destination)

    verified = []
    for entry in lock["archives"]:
        archive_path = archive_dir / entry["file"]
        if not archive_path.is_file() or archive_path.is_symlink():
            raise PreparationError(f"missing regular archive: {archive_path}")
        actual = sha256_file(archive_path)
        if actual != entry["sha256"]:
            raise PreparationError(f"archive checksum mismatch: {entry['file']}")
        verified.append({"file": entry["file"], "sha256": actual})

    destination.mkdir(parents=True, exist_ok=True)
    for entry in lock["archives"]:
        extract_archive(
            archive_dir / entry["file"],
            destination / entry["destination"],
            int(entry.get("strip_components", 0)),
        )

    shutil.copyfile(destination / "build" / "core" / "root.mk",
                    destination / "Makefile")
    shutil.copytree(PROBE_DIR, destination / "ipod6g_probe")
    shutil.copytree(LAUNCHER_DIR, destination / "ipod6g_launcher")
    subprocess.run(
        ["patch", "--batch", "--forward", "-p1", "-i", str(PATCH_PATH)],
        cwd=destination,
        check=True,
    )
    shutil.copyfile(
        ASHMEM_OVERLAY,
        destination / "system" / "core" / "libcutils" / "ashmem-dev.c",
    )

    marker = {
        "schema": 1,
        "aosp_tag": lock["aosp_tag"],
        "platform_version": lock["platform_version"],
        "platform_sdk": lock["platform_sdk"],
        "build_id": lock["build_id"],
        "archives": verified,
        "port_patch_sha256": sha256_file(PATCH_PATH),
        "probe_android_mk_sha256": sha256_file(PROBE_DIR / "Android.mk"),
        "probe_source_sha256": sha256_file(PROBE_DIR / "probe.c"),
        "zygote_gate_source_sha256": sha256_file(
            PROBE_DIR / "zygote_gate.c"
        ),
        "launcher_android_mk_sha256": sha256_file(LAUNCHER_DIR / "Android.mk"),
        "launcher_manifest_sha256": sha256_file(
            LAUNCHER_DIR / "AndroidManifest.xml"
        ),
        "launcher_source_sha256": sha256_file(
            LAUNCHER_DIR / "src/org/rockpod/eclair/launcher/"
            "RockpodLauncherActivity.java"
        ),
        "ashmem_overlay_sha256": sha256_file(ASHMEM_OVERLAY),
        "prepared_source_tree_sha256": sha256_source_tree(destination),
        "network_used": False,
        "hardware_accessed": False,
    }
    marker_path = destination / MARKER_NAME
    with marker_path.open("w", encoding="utf-8") as stream:
        json.dump(marker, stream, indent=2, sort_keys=True)
        stream.write("\n")
    return marker


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive_dir", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        marker = prepare(args.archive_dir.resolve(), args.destination.resolve())
    except (OSError, PreparationError, subprocess.CalledProcessError,
            tarfile.TarError) as error:
        parser.exit(1, f"prepare_eclair_native_source: {error}\n")
    print(json.dumps(marker, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
