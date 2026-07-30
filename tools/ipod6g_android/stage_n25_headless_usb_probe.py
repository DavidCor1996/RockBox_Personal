#!/usr/bin/env python3
"""Create-only staging for the N25 headless USB diagnostic payloads."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess


PAYLOADS = (
    "n25-headless-kernel.ipod",
    "n25-headless-initramfs.ipod",
    "n25-headless-dtb.ipod",
)
TARGET_RELATIVE = Path(".rockbox/android/diagnostic-headless-usb2")


class StageError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def regular(path: Path, root: Path) -> Path:
    if path.is_symlink():
        raise StageError(f"symlink is forbidden: {path}")
    resolved = path.resolve(strict=True)
    try:
        resolved.relative_to(root)
    except ValueError as error:
        raise StageError(f"path escapes its root: {path}") from error
    mode = resolved.stat().st_mode
    if not stat.S_ISREG(mode):
        raise StageError(f"regular file required: {path}")
    return resolved


def snapshot_existing(rockbox: Path, root: Path) -> dict[str, dict[str, object]]:
    result: dict[str, dict[str, object]] = {}
    root_firmware = regular(root / "rockbox.ipod", root)
    result["rockbox.ipod"] = {
        "size": root_firmware.stat().st_size,
        "sha256": sha256(root_firmware),
    }
    for base, directories, files in os.walk(rockbox, followlinks=False):
        base_path = Path(base)
        for name in directories:
            candidate = base_path / name
            if candidate.is_symlink():
                raise StageError(f"existing Rockbox symlink is forbidden: {candidate}")
        for name in files:
            candidate = regular(base_path / name, root)
            relative = candidate.relative_to(root).as_posix()
            result[relative] = {
                "size": candidate.stat().st_size,
                "sha256": sha256(candidate),
            }
    return result


def snapshot_protected(rockbox: Path, root: Path) -> dict[str, dict[str, object]]:
    """Hash only state whose preservation is a hard hardware-test gate."""
    result: dict[str, dict[str, object]] = {}

    def add(candidate: Path) -> None:
        candidate = regular(candidate, root)
        relative = candidate.relative_to(root).as_posix()
        result[relative] = {
            "size": candidate.stat().st_size,
            "sha256": sha256(candidate),
        }

    add(root / "rockbox.ipod")
    add(rockbox / "rockbox.ipod")

    for pattern in ("database*.tcd", "tagcache*.tcd"):
        for candidate in sorted(rockbox.glob(pattern)):
            add(candidate)

    config = rockbox / "config.cfg"
    if config.exists() or config.is_symlink():
        add(config)

    android = rockbox / "android"
    for base, directories, files in os.walk(android, followlinks=False):
        base_path = Path(base)
        for name in directories:
            candidate = base_path / name
            if candidate.is_symlink():
                raise StageError(
                    f"existing Android payload symlink is forbidden: {candidate}"
                )
        for name in files:
            add(base_path / name)

    return result


def parse_manifest(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="ascii").splitlines():
        fields = line.split()
        if len(fields) != 2 or len(fields[0]) != 64:
            raise StageError(f"invalid checksum manifest line: {line!r}")
        if fields[1] in result:
            raise StageError(f"duplicate checksum entry: {fields[1]}")
        result[fields[1]] = fields[0].lower()
    return result


def fsync_directory(path: Path) -> None:
    descriptor = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--targeted-protection",
        action="store_true",
        help=(
            "hash firmware, database/tagcache, config, and existing Android "
            "payloads instead of the complete .rockbox tree"
        ),
    )
    parser.add_argument("bundle", type=Path)
    parser.add_argument("mount_root", type=Path)
    args = parser.parse_args(argv)

    bundle = args.bundle.resolve(strict=True)
    root = args.mount_root.resolve(strict=True)
    if not bundle.is_dir() or not root.is_dir():
        raise StageError("bundle and mount root must be directories")
    mount_report = json.loads(subprocess.check_output(
        ["findmnt", "-J", "-T", str(root), "-o", "TARGET,FSTYPE,OPTIONS"],
        text=True,
    ))
    mounts = mount_report.get("filesystems", ())
    if len(mounts) != 1:
        raise StageError("mount root did not resolve to exactly one filesystem")
    mount = mounts[0]
    if Path(mount.get("target", "")).resolve() != root or mount.get("fstype") != "vfat":
        raise StageError("mount root is not the exact iPod FAT mount")
    if "rw" not in mount.get("options", "").split(","):
        raise StageError(
            "iPod FAT volume is not mounted read-write: "
            + mount.get("options", "")
        )

    qualification_path = regular(bundle / "qualification.json", bundle)
    qualification = json.loads(qualification_path.read_text(encoding="utf-8"))
    gates = {
        "artifact_gate_passed": True,
        "device_test_ready": True,
        "hardware_qualified": False,
        "storage_code_present": False,
        "persistent_storage_available": False,
        "hardware_actions_enabled": False,
        "rockbox_handoff_binary_emulated": True,
        "usb_serial_initcall_reached": True,
        "automatic_recovery_seconds": 60,
    }
    if any(qualification.get(name) is not expected for name, expected in gates.items()):
        raise StageError("headless probe qualification gate failed")
    manifest = parse_manifest(regular(bundle / "SHA256SUMS", bundle))
    expected_bundle = {
        *PAYLOADS,
        "n25-headless-forced-loader.dfu",
        "forced-boot-emulation.json",
        "rockbox-to-gserial-emulation.json",
        "qualification.json",
    }
    if set(manifest) != expected_bundle:
        raise StageError("headless probe bundle manifest is incomplete")
    for name, expected in manifest.items():
        if sha256(regular(bundle / name, bundle)) != expected:
            raise StageError(f"source checksum mismatch: {name}")

    rockbox = root / ".rockbox"
    if rockbox.is_symlink() or not rockbox.is_dir():
        raise StageError("safe .rockbox directory is missing")
    android = rockbox / "android"
    if android.is_symlink() or not android.is_dir():
        raise StageError("safe .rockbox/android directory is missing")
    root_firmware = regular(root / "rockbox.ipod", root)
    dot_firmware = regular(rockbox / "rockbox.ipod", root)
    if sha256(root_firmware) != sha256(dot_firmware):
        raise StageError("installed Rockbox firmware copies differ")

    target = root / TARGET_RELATIVE
    staging = target.with_name(target.name + ".staging")
    if target.exists() or target.is_symlink():
        raise StageError(f"create-only target already exists: {target}")
    if staging.exists() or staging.is_symlink():
        raise StageError(f"stale staging target exists: {staging}")

    snapshot = snapshot_protected if args.targeted_protection else snapshot_existing
    before = snapshot(rockbox, root)
    created: list[Path] = []
    try:
        staging.mkdir(mode=0o755)
        created.append(staging)
        payload_records = []
        for name in PAYLOADS:
            source = regular(bundle / name, bundle)
            destination = staging / name
            with source.open("rb") as input_handle, destination.open("xb") as output_handle:
                shutil.copyfileobj(input_handle, output_handle, 1024 * 1024)
                output_handle.flush()
                os.fsync(output_handle.fileno())
            created.append(destination)
            digest = sha256(destination)
            if digest != manifest[name]:
                raise StageError(f"staged read-back mismatch: {name}")
            payload_records.append(
                {"name": name, "size": destination.stat().st_size, "sha256": digest}
            )
        payload_manifest = "".join(
            f"{record['sha256']}  {record['name']}\n"
            for record in payload_records
        )
        manifest_destination = staging / "SHA256SUMS"
        with manifest_destination.open("x", encoding="ascii") as handle:
            handle.write(payload_manifest)
            handle.flush()
            os.fsync(handle.fileno())
        created.append(manifest_destination)
        fsync_directory(staging)
        os.replace(staging, target)
        created = [target]
        fsync_directory(android)
    except Exception:
        for path in reversed(created):
            if path.is_file() and not path.is_symlink():
                path.unlink()
            elif path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
        raise

    after = snapshot(rockbox, root)
    new_prefix = TARGET_RELATIVE.as_posix() + "/"
    existing_after = {
        name: record for name, record in after.items()
        if not name.startswith(new_prefix)
    }
    if existing_after != before:
        raise StageError("an existing Rockbox file changed during staging")
    staged_names = {
        path.relative_to(target).as_posix()
        for path in target.iterdir()
    }
    if staged_names != {*PAYLOADS, "SHA256SUMS"}:
        raise StageError("unexpected file in staged diagnostic directory")

    print(json.dumps({
        "staged": True,
        "create_only": True,
        "protection_mode": (
            "targeted-critical" if args.targeted_protection else "complete-rockbox"
        ),
        "target": TARGET_RELATIVE.as_posix(),
        "existing_rockbox_files_hash_verified": len(before),
        "rockbox_firmware_written": False,
        "database_files_preserved": True,
        "partition_table_written": False,
        "nor_written": False,
        "files": payload_records,
    }, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (StageError, OSError, json.JSONDecodeError,
            subprocess.CalledProcessError) as error:
        raise SystemExit(f"headless USB probe staging failed: {error}")
