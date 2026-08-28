#!/usr/bin/env python3
"""Verify, extract, and narrow the pinned SHAtter source for N81 RAM boot."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tarfile
import tempfile


HERE = Path(__file__).resolve().parent
LOCK_PATH = HERE / "source-lock.json"
PATCH_PATH = HERE / "patches" / "ipwndfu-shatter-n81-volatile-no-nor.patch"
MARKER = ".rockpod-shatter-source-lock.json"


class PreparationError(RuntimeError):
    """Raised when SHAtter source is not exact or cannot be safely narrowed."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def checked_members(archive: tarfile.TarFile) -> tuple[list[tarfile.TarInfo], str]:
    members = archive.getmembers()
    if not members:
        raise PreparationError("ipwndfu archive is empty")
    roots: set[str] = set()
    for member in members:
        path = PurePosixPath(member.name)
        if path.is_absolute() or ".." in path.parts or not path.parts:
            raise PreparationError(f"unsafe archive path: {member.name}")
        roots.add(path.parts[0])
        if member.ischr() or member.isblk() or member.isfifo():
            raise PreparationError(f"unsafe archive member: {member.name}")
        if member.issym() or member.islnk():
            target = path.parent / member.linkname
            if target.is_absolute() or ".." in target.parts:
                raise PreparationError(f"unsafe archive link: {member.name}")
    if len(roots) != 1:
        raise PreparationError("ipwndfu archive must have one top-level directory")
    return members, next(iter(roots))


def prepare(archive_path: Path, destination: Path) -> None:
    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))["dfu_transport"]
    actual = sha256(archive_path)
    if actual != lock["archive_sha256"]:
        raise PreparationError(
            "ipwndfu archive checksum mismatch: "
            f"expected {lock['archive_sha256']}, got {actual}"
        )
    if destination.exists():
        raise PreparationError(f"destination already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".shatter-", dir=destination.parent))
    try:
        with tarfile.open(archive_path, "r:gz") as archive:
            members, root = checked_members(archive)
            archive.extractall(temporary, members=members, filter="data")
        source = temporary / root
        subprocess.run(
            ["patch", "-p1", "--batch", "--forward", "-i", str(PATCH_PATH)],
            cwd=source,
            check=True,
        )
        marker = {
            "schema": 1,
            "archive_sha256": actual,
            "commit": lock["commit"],
            "patch_sha256": sha256(PATCH_PATH),
            "profile": "n81-shatter-volatile-no-storage",
        }
        (source / MARKER).write_text(
            json.dumps(marker, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        os.replace(source, destination)
    except (OSError, subprocess.CalledProcessError, tarfile.TarError) as error:
        raise PreparationError(f"failed to prepare SHAtter source: {error}") from error
    finally:
        shutil.rmtree(temporary, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        prepare(args.archive.resolve(), args.destination.resolve())
    except PreparationError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
