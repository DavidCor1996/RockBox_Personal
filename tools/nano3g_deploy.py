#!/usr/bin/env python3
"""Identity-guarded Rockbox package deployment for the owned iPod Nano 3G.

The default mode is a read-only plan.  ``--apply`` overlays the package, then
recreates both firmware paths from separately staged copies and verifies every
installed byte.  It never formats, unmounts, ejects, or writes a raw device.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import errno
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import sys
import zipfile


EXPECTED_NANO_SERIAL = "000A27001AF57313"
EXCLUDED_IPOD6G_SERIAL = "000A27002101824D"
NANO_MIN_BYTES = 3_000_000_000
NANO_MAX_BYTES = 5_000_000_000
MODEL_NUMBER = 117
MODEL_TAG = b"nn3g"
MAX_PACKAGE_FILE = 128 * 1024 * 1024
MAX_PACKAGE_TOTAL = 768 * 1024 * 1024


class DeployError(RuntimeError):
    """A deployment invariant was not met."""


@dataclass(frozen=True)
class DeviceMount:
    disk: Path
    partition: Path
    mountpoint: Path
    serial: str
    size: int
    model: str
    fstype: str


@dataclass(frozen=True)
class PackageEntry:
    relative: PurePosixPath
    data: bytes


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_firmware(path: Path) -> tuple[bytes, str, int]:
    try:
        image = path.read_bytes()
    except OSError as exc:
        raise DeployError(f"cannot read firmware {path}: {exc}") from exc
    if len(image) < 12:
        raise DeployError(f"firmware is too short: {len(image)} bytes")
    if image[4:8] != MODEL_TAG:
        raise DeployError(
            f"firmware model tag is {image[4:8]!r}, expected {MODEL_TAG!r}"
        )
    stored = int.from_bytes(image[0:4], "big")
    calculated = (MODEL_NUMBER + sum(image[8:])) & 0xFFFFFFFF
    if stored != calculated:
        raise DeployError(
            f"firmware checksum mismatch: stored={stored:08x} calculated={calculated:08x}"
        )
    return image, sha256_bytes(image), calculated


def read_lsblk(fixture: Path | None = None) -> dict[str, object]:
    if fixture is not None:
        try:
            return json.loads(fixture.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise DeployError(f"cannot read lsblk fixture {fixture}: {exc}") from exc

    cmd = [
        "lsblk",
        "--json",
        "-b",
        "-o",
        "NAME,KNAME,PATH,TYPE,SIZE,MODEL,SERIAL,TRAN,RM,MOUNTPOINTS,FSTYPE,LABEL,UUID",
    ]
    try:
        proc = subprocess.run(cmd, check=True, text=True, capture_output=True)
        return json.loads(proc.stdout)
    except (OSError, subprocess.CalledProcessError, json.JSONDecodeError) as exc:
        raise DeployError(f"lsblk discovery failed: {exc}") from exc


def _mountpoints(node: dict[str, object]) -> list[Path]:
    raw = node.get("mountpoints")
    if raw is None:
        raw = [node.get("mountpoint")]
    elif isinstance(raw, str):
        raw = [raw]
    if not isinstance(raw, list):
        return []
    return [Path(value).resolve() for value in raw if isinstance(value, str) and value]


def select_device(
    topology: dict[str, object],
    expected_serial: str,
    requested_mountpoint: Path | None,
) -> DeviceMount:
    requested = requested_mountpoint.resolve() if requested_mountpoint else None
    matches: list[DeviceMount] = []
    devices = topology.get("blockdevices", [])
    if not isinstance(devices, list):
        raise DeployError("lsblk output has no blockdevices list")

    for raw_disk in devices:
        if not isinstance(raw_disk, dict) or raw_disk.get("type") != "disk":
            continue
        serial = str(raw_disk.get("serial") or "")
        if serial == EXCLUDED_IPOD6G_SERIAL:
            continue
        if serial != expected_serial:
            continue
        size = int(raw_disk.get("size") or 0)
        model = str(raw_disk.get("model") or "").strip()
        transport = str(raw_disk.get("tran") or "")
        removable = raw_disk.get("rm")
        if not (NANO_MIN_BYTES <= size <= NANO_MAX_BYTES):
            raise DeployError(
                f"serial {serial} has impossible Nano size {size} bytes"
            )
        if model.lower() != "ipod" or transport != "usb" or removable not in (True, 1):
            raise DeployError(
                f"serial {serial} identity mismatch: model={model!r} tran={transport!r} rm={removable!r}"
            )

        children = raw_disk.get("children", [])
        if not isinstance(children, list):
            continue
        for child in children:
            if not isinstance(child, dict) or child.get("type") != "part":
                continue
            fstype = str(child.get("fstype") or "")
            if fstype.lower() not in ("vfat", "fat", "fat32", "msdos"):
                continue
            for mountpoint in _mountpoints(child):
                if requested is not None and mountpoint != requested:
                    continue
                matches.append(
                    DeviceMount(
                        disk=Path(str(raw_disk.get("path") or "")),
                        partition=Path(str(child.get("path") or "")),
                        mountpoint=mountpoint,
                        serial=serial,
                        size=size,
                        model=model,
                        fstype=fstype,
                    )
                )

    if not matches:
        suffix = f" at {requested}" if requested is not None else ""
        raise DeployError(f"mounted Nano serial {expected_serial} not found{suffix}")
    if len(matches) != 1:
        raise DeployError(
            "Nano identity is ambiguous: "
            + ", ".join(str(match.mountpoint) for match in matches)
        )
    selected = matches[0]
    if not selected.mountpoint.is_dir():
        raise DeployError(f"mountpoint is not a directory: {selected.mountpoint}")
    return selected


def _validate_relative(raw: str) -> PurePosixPath:
    relative = PurePosixPath(raw)
    if relative.is_absolute() or not relative.parts:
        raise DeployError(f"unsafe package path: {raw!r}")
    if any(part in ("", ".", "..") for part in relative.parts):
        raise DeployError(f"unsafe package path: {raw!r}")
    if relative.parts[0] != ".rockbox":
        raise DeployError(f"package entry is outside .rockbox: {raw!r}")
    return relative


def load_package(path: Path) -> tuple[list[PackageEntry], str]:
    entries: list[PackageEntry] = []
    total = 0

    if path.is_file() and zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            for info in archive.infolist():
                if info.is_dir():
                    continue
                relative = _validate_relative(info.filename)
                unix_type = stat.S_IFMT(info.external_attr >> 16)
                if unix_type and unix_type != stat.S_IFREG:
                    raise DeployError(
                        f"package special file is not allowed: {relative}"
                    )
                if info.file_size > MAX_PACKAGE_FILE:
                    raise DeployError(f"package file is too large: {relative}")
                if total + info.file_size > MAX_PACKAGE_TOTAL:
                    raise DeployError(
                        f"package expands to too much data: "
                        f"{total + info.file_size} bytes"
                    )
                data = archive.read(info)
                if len(data) != info.file_size:
                    raise DeployError(f"short package read: {relative}")
                total += len(data)
                entries.append(PackageEntry(relative, data))
        source_hash = sha256_file(path)
    elif path.is_dir():
        root = path / ".rockbox" if (path / ".rockbox").is_dir() else path
        prefix = PurePosixPath(".rockbox")
        for source in sorted(root.rglob("*")):
            if source.is_symlink():
                raise DeployError(f"package symlink is not allowed: {source}")
            if source.is_dir():
                continue
            if not source.is_file():
                raise DeployError(f"package special file is not allowed: {source}")
            relative = prefix / PurePosixPath(source.relative_to(root).as_posix())
            source_size = source.stat().st_size
            if source_size > MAX_PACKAGE_FILE:
                raise DeployError(f"package file is too large: {relative}")
            if total + source_size > MAX_PACKAGE_TOTAL:
                raise DeployError(
                    f"package expands to too much data: "
                    f"{total + source_size} bytes"
                )
            data = source.read_bytes()
            if len(data) != source_size:
                raise DeployError(f"package file changed while reading: {source}")
            total += len(data)
            entries.append(PackageEntry(relative, data))
        digest = hashlib.sha256()
        for entry in entries:
            digest.update(str(entry.relative).encode("utf-8") + b"\0")
            digest.update(hashlib.sha256(entry.data).digest())
        source_hash = digest.hexdigest()
    else:
        raise DeployError(f"package is not a Rockbox zip or directory: {path}")

    if total > MAX_PACKAGE_TOTAL:
        raise DeployError(f"package expands to too much data: {total} bytes")
    if not entries:
        raise DeployError(f"package contains no files: {path}")
    names = [entry.relative for entry in entries]
    if len(names) != len(set(names)):
        raise DeployError("package contains duplicate paths")

    # FAT is case-insensitive.  Some legacy Rockbox assets intentionally ship
    # byte-identical aliases for differently cased lookup names.  Install one
    # representative of such aliases, but reject a collision whose contents
    # differ because its result would depend on archive order.
    unique_entries: list[PackageEntry] = []
    folded_entries: dict[str, PackageEntry] = {}
    for entry in entries:
        folded = str(entry.relative).casefold()
        previous = folded_entries.get(folded)
        if previous is None:
            folded_entries[folded] = entry
            unique_entries.append(entry)
        elif previous.data != entry.data:
            raise DeployError(
                f"package contains conflicting FAT case aliases: "
                f"{previous.relative} and {entry.relative}"
            )
    return unique_entries, source_hash


def _fsync_directory(path: Path) -> None:
    try:
        descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
    except OSError as exc:
        if exc.errno in (errno.EINVAL, errno.ENOTSUP, errno.EOPNOTSUPP):
            return
        raise
    try:
        os.fsync(descriptor)
    except OSError as exc:
        if exc.errno not in (errno.EINVAL, errno.ENOTSUP, errno.EOPNOTSUPP):
            raise
    finally:
        os.close(descriptor)


def _stage_bytes(destination: Path, data: bytes, token: str) -> Path:
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = destination.parent / f"N3GSTG{token}.tmp"
    try:
        stage.unlink()
    except FileNotFoundError:
        pass
    with stage.open("xb") as handle:
        handle.write(data)
        handle.flush()
        os.fsync(handle.fileno())
    if sha256_file(stage) != sha256_bytes(data):
        stage.unlink(missing_ok=True)
        raise DeployError(f"staged checksum mismatch for {destination}")
    return stage


def _atomic_install(destination: Path, data: bytes, token: str) -> None:
    stage = _stage_bytes(destination, data, token)
    os.replace(stage, destination)
    _fsync_directory(destination.parent)


def _backup_firmware(device: DeviceMount, backup: Path) -> list[dict[str, object]]:
    backup.mkdir(parents=True, exist_ok=False)
    records: list[dict[str, object]] = []
    paths = (
        (device.mountpoint / "rockbox.ipod", backup / "root-rockbox.ipod"),
        (device.mountpoint / ".rockbox" / "rockbox.ipod", backup / "dot-rockbox.ipod"),
    )
    for source, destination in paths:
        if not source.is_file():
            continue
        shutil.copyfile(source, destination)
        with destination.open("rb") as handle:
            os.fsync(handle.fileno())
        records.append(
            {
                "source": str(source),
                "backup": str(destination),
                "bytes": destination.stat().st_size,
                "sha256": sha256_file(destination),
            }
        )
    _fsync_directory(backup)
    return records


def _destination(device: DeviceMount, relative: PurePosixPath) -> Path:
    destination = device.mountpoint.joinpath(*relative.parts)
    try:
        destination.resolve(strict=False).relative_to(device.mountpoint.resolve())
    except ValueError as exc:
        raise DeployError(f"package path escapes mountpoint: {relative}") from exc
    return destination


def deploy(args: argparse.Namespace) -> dict[str, object]:
    firmware, firmware_hash, checksum = validate_firmware(args.firmware)
    package, package_hash = load_package(args.package)
    device = select_device(
        read_lsblk(args.lsblk_json), EXPECTED_NANO_SERIAL, args.mountpoint
    )

    package_without_firmware: list[PackageEntry] = []
    package_has_firmware = False
    for entry in package:
        if entry.relative == PurePosixPath(".rockbox/rockbox.ipod"):
            package_has_firmware = True
            if entry.data != firmware:
                raise DeployError(
                    "package firmware differs from the explicit rockbox.ipod"
                )
            continue
        package_without_firmware.append(entry)
    if not package_has_firmware:
        raise DeployError("package is missing .rockbox/rockbox.ipod")

    changed = 0
    unchanged = 0
    for entry in package_without_firmware:
        destination = _destination(device, entry.relative)
        if destination.is_file() and sha256_file(destination) == sha256_bytes(entry.data):
            unchanged += 1
        else:
            changed += 1

    result: dict[str, object] = {
        "mode": "apply" if args.apply else "plan",
        "device": {
            "disk": str(device.disk),
            "partition": str(device.partition),
            "mountpoint": str(device.mountpoint),
            "serial": device.serial,
            "size": device.size,
            "model": device.model,
            "fstype": device.fstype,
        },
        "firmware": {
            "source": str(args.firmware),
            "bytes": len(firmware),
            "sha256": firmware_hash,
            "model_tag": MODEL_TAG.decode("ascii"),
            "checksum": f"{checksum:08x}",
        },
        "package": {
            "source": str(args.package),
            "sha256": package_hash,
            "files": len(package_without_firmware),
            "changed": changed,
            "unchanged": unchanged,
        },
        "backups": [],
    }
    if not args.apply:
        return result

    if not os.access(device.mountpoint, os.W_OK):
        raise DeployError(f"mountpoint is not writable: {device.mountpoint}")
    backup = args.backup_dir
    if backup is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%SZ")
        backup = Path("tmp") / f"nano3g-deploy-{stamp}"
    result["backups"] = _backup_firmware(device, backup)
    manifest = backup / "manifest.json"
    result["manifest"] = str(manifest)
    result["status"] = "prepared"
    _atomic_install(
        manifest,
        (json.dumps(result, indent=2, sort_keys=True) + "\n").encode("utf-8"),
        "manifest",
    )

    try:
        token = 0
        for entry in package_without_firmware:
            destination = _destination(device, entry.relative)
            expected = sha256_bytes(entry.data)
            if destination.is_file() and sha256_file(destination) == expected:
                continue
            _atomic_install(destination, entry.data, f"{token:08x}")
            token += 1
            if sha256_file(destination) != expected:
                raise DeployError(f"package verification failed: {destination}")

        root_firmware = device.mountpoint / "rockbox.ipod"
        dot_firmware = device.mountpoint / ".rockbox" / "rockbox.ipod"
        root_stage = _stage_bytes(root_firmware, firmware, f"{token:08x}")
        token += 1
        dot_stage = _stage_bytes(dot_firmware, firmware, f"{token:08x}")
        os.replace(dot_stage, dot_firmware)
        _fsync_directory(dot_firmware.parent)
        os.replace(root_stage, root_firmware)
        _fsync_directory(root_firmware.parent)
        os.sync()

        for installed in (root_firmware, dot_firmware):
            actual = sha256_file(installed)
            if actual != firmware_hash:
                raise DeployError(
                    f"firmware verification failed for {installed}: "
                    f"{actual} != {firmware_hash}"
                )

        for entry in package_without_firmware:
            destination = _destination(device, entry.relative)
            actual = sha256_file(destination)
            expected = sha256_bytes(entry.data)
            if actual != expected:
                raise DeployError(
                    f"post-sync package verification failed for {destination}"
                )

        result["installed_firmware"] = [
            {"path": str(root_firmware), "sha256": sha256_file(root_firmware)},
            {"path": str(dot_firmware), "sha256": sha256_file(dot_firmware)},
        ]
        result["status"] = "complete"
        _atomic_install(
            manifest,
            (json.dumps(result, indent=2, sort_keys=True) + "\n").encode("utf-8"),
            "manifest",
        )
    except BaseException as exc:
        result["status"] = "failed"
        result["error"] = f"{type(exc).__name__}: {exc}"
        try:
            _atomic_install(
                manifest,
                (json.dumps(result, indent=2, sort_keys=True) + "\n").encode("utf-8"),
                "manifest",
            )
        except (DeployError, OSError):
            pass
        raise
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", required=True, type=Path)
    parser.add_argument(
        "--package", required=True, type=Path,
        help="rockbox.zip or a directory containing .rockbox",
    )
    parser.add_argument("--mountpoint", type=Path)
    parser.add_argument(
        "--lsblk-json", type=Path, help=argparse.SUPPRESS
    )
    parser.add_argument("--backup-dir", type=Path)
    parser.add_argument(
        "--apply",
        action="store_true",
        help="perform the verified package and dual-firmware deployment",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        result = deploy(args)
    except (DeployError, OSError) as exc:
        print(f"N3G_DEPLOY_FAIL {exc}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
