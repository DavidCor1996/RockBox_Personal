#!/usr/bin/env python3
"""Validate and stage the pinned storage-free Android 2.0 root for N81."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import struct
import tempfile

from qualify_ramdiag import parse_newc


INITRAMFS_MAX = 20 * 1024 * 1024
REQUIRED_ENTRIES = {
    "init",
    "init.rc",
    "system/app/RockpodLauncher.apk",
    "system/bin/app_process",
    "system/bin/dalvikvm",
    "system/bin/installd",
    "system/bin/linker",
    "system/bin/servicemanager",
    "system/framework/core.jar",
    "system/framework/framework.jar",
    "system/framework/services.jar",
    "system/lib/hw/gralloc.default.so",
    "system/lib/libbinder.so",
    "system/lib/libdvm.so",
    "system/lib/libsurfaceflinger.so",
}
FORBIDDEN_PATH_MARKERS = (
    "dev/mmc",
    "dev/mtd",
    "dev/sd",
    "dev/nand",
    "system/bin/mount",
    "system/bin/fsck",
    "system/bin/vold",
)


class StageError(RuntimeError):
    """Raised when the Android root is not the exact qualified RAM-only input."""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def read_report(path: Path, label: str) -> dict[str, object]:
    if path.is_symlink() or not path.is_file():
        raise StageError(f"{label} must be a regular, non-symlink file")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise StageError(f"{label} is invalid: {error}") from error


def require_values(report: dict[str, object], expected: dict[str, object], label: str) -> None:
    for name, value in expected.items():
        if report.get(name) != value:
            raise StageError(f"{label} has unexpected {name}")


def arm_elf(data: bytes, label: str) -> None:
    if (
        len(data) < 52
        or data[:6] != b"\x7fELF\x01\x01"
        or struct.unpack_from("<H", data, 18)[0] != 40
    ):
        raise StageError(f"{label} is not an ELF32 little-endian ARM executable")


def qualify_inputs(
    initramfs: Path, root_report_path: Path, system_report_path: Path
) -> dict[str, object]:
    if initramfs.is_symlink() or not initramfs.is_file():
        raise StageError("Eclair initramfs must be a regular, non-symlink file")
    packed = initramfs.read_bytes()
    if not packed or len(packed) > INITRAMFS_MAX or not packed.startswith(b"\x1f\x8b"):
        raise StageError("Eclair initramfs is empty, oversized, or not gzip")
    if packed[4:8] != b"\0\0\0\0":
        raise StageError("Eclair gzip header is not reproducible")

    root_report = read_report(root_report_path, "Eclair root report")
    require_values(
        root_report,
        {
            "scope": "android-2.0-native-storage-free-root",
            "aosp_tag": "android-2.0_r1",
            "cpu": "ARMv5TE",
            "root_read_only": True,
            "data_cache_metadata": "tmpfs",
            "persistent_storage_nodes": False,
            "storage_tools": False,
            "static_gate_passed": True,
            "emulation_gate_passed": True,
            "app_process_packaged": True,
            "zygote_native_runtime_packaged": True,
            "software_renderer_packaged": True,
            "launcher_packaged": True,
            "hardware_actions_enabled": False,
        },
        "Eclair root report",
    )
    recorded_root = root_report.get("initramfs", {})
    if not isinstance(recorded_root, dict):
        raise StageError("Eclair root report lacks initramfs metadata")
    if (
        recorded_root.get("compressed_size") != len(packed)
        or recorded_root.get("sha256") != sha256_bytes(packed)
    ):
        raise StageError("Eclair initramfs differs from its root report")

    system_report = read_report(system_report_path, "Eclair system report")
    require_values(
        system_report,
        {
            "scope": "android-2.0-native-full-system-emulation",
            "system_boot_gate_passed": True,
            "persistent_storage_attached": False,
            "network_backend_attached": False,
            "binder_positive_path_tested": True,
            "dalvik_vm_tested": True,
            "surfaceflinger_tested": True,
            "system_server_ready": True,
            "launcher_tested": True,
            "zygote_accept_loop_tested": True,
            "hardware_actions_enabled": False,
        },
        "Eclair system report",
    )
    recorded_system = system_report.get("artifacts", {})
    if not isinstance(recorded_system, dict):
        raise StageError("Eclair system report lacks artifact metadata")
    system_initramfs = recorded_system.get("initramfs", {})
    if not isinstance(system_initramfs, dict) or (
        system_initramfs.get("size") != len(packed)
        or system_initramfs.get("sha256") != sha256_bytes(packed)
    ):
        raise StageError("Eclair initramfs differs from its system report")

    try:
        archive = gzip.decompress(packed)
        entries = parse_newc(archive)
    except (OSError, EOFError) as error:
        raise StageError(f"cannot read Eclair initramfs: {error}") from error
    missing = REQUIRED_ENTRIES - set(entries)
    if missing:
        raise StageError(f"Eclair initramfs lacks runtime entries: {sorted(missing)}")
    forbidden = sorted(
        name
        for name in entries
        if any(marker in name.lower() for marker in FORBIDDEN_PATH_MARKERS)
    )
    if forbidden:
        raise StageError(f"Eclair initramfs contains storage paths: {forbidden}")
    for name, (mode, body) in entries.items():
        if stat.S_ISBLK(mode):
            raise StageError(f"Eclair initramfs contains block device: {name}")
        if stat.S_ISLNK(mode):
            try:
                target = PurePosixPath(body.decode("utf-8"))
            except UnicodeDecodeError as error:
                raise StageError(f"Eclair symlink is not UTF-8: {name}") from error
            if target.is_absolute() or ".." in target.parts:
                raise StageError(f"unsafe Eclair symlink: {name}")

    runtime_files = root_report.get("runtime_files", {})
    if not isinstance(runtime_files, dict):
        raise StageError("Eclair root report lacks runtime file hashes")
    for name, recorded in runtime_files.items():
        if name not in entries or not isinstance(recorded, dict):
            raise StageError(f"Eclair runtime report lacks archive entry: {name}")
        body = entries[name][1]
        if recorded.get("size") != len(body) or recorded.get("sha256") != sha256_bytes(
            body
        ):
            raise StageError(f"Eclair runtime entry differs from report: {name}")

    arm_elf(entries["init"][1], "/init")
    arm_elf(entries["system/bin/app_process"][1], "app_process")
    init_rc = entries["init.rc"][1].decode("utf-8").lower()
    for contract in (
        "mount tmpfs tmpfs /data",
        "mount tmpfs tmpfs /cache",
        "mount tmpfs tmpfs /metadata",
        "mount rootfs rootfs / ro remount",
        "start servicemanager",
        "start zygotegate",
    ):
        if contract not in init_rc:
            raise StageError(f"Eclair init.rc lacks contract: {contract}")
    for marker in ("/dev/mmc", "/dev/mtd", "/dev/sd", "mount ext", "mount vfat"):
        if marker in init_rc:
            raise StageError(f"Eclair init.rc contains storage contract: {marker}")

    gralloc = entries["system/lib/hw/gralloc.default.so"][1]
    for marker in (
        b"IPOD6G_ECLAIR_GRALLOC:USING_LINUX_FRAMEBUFFER",
        b"IPOD6G_ECLAIR_GRALLOC:NO_FB_DEVICE_USING_HEADLESS",
    ):
        if marker not in gralloc:
            raise StageError(f"Eclair gralloc lacks marker {marker!r}")

    return {
        "schema": 1,
        "artifact_gate_passed": True,
        "board": "apple-n81-ipod-touch-4g",
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
        "profile": "n81-eclair-2.0-volatile-no-storage",
        "aosp_tag": "android-2.0_r1",
        "cpu_compatible": True,
        "binder_userspace_packaged": True,
        "zygote_packaged": True,
        "surfaceflinger_packaged": True,
        "software_renderer_packaged": True,
        "linux_framebuffer_gralloc_path_packaged": True,
        "tmpfs_mutable_state": True,
        "root_read_only": True,
        "storage_paths_present": False,
        "shell_present": False,
        "host_full_system_gate_passed": True,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "physical_android_tested": False,
        "source_reports": {
            "root": sha256(root_report_path),
            "system": sha256(system_report_path),
        },
        "artifacts": {
            "initramfs": {"size": len(packed), "sha256": sha256_bytes(packed)}
        },
    }


def stage(
    initramfs: Path,
    root_report_path: Path,
    system_report_path: Path,
    destination: Path,
) -> dict[str, object]:
    report = qualify_inputs(initramfs, root_report_path, system_report_path)
    if destination.exists():
        raise StageError(f"destination already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".n81-eclair-", dir=destination.parent))
    try:
        shutil.copyfile(initramfs, temporary / "n81-eclair-initramfs.cpio.gz")
        shutil.copyfile(root_report_path, temporary / "source-root-qualification.json")
        shutil.copyfile(system_report_path, temporary / "source-system-emulation.json")
        (temporary / "eclair-n81-qualification.json").write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        os.replace(temporary, destination)
    except OSError as error:
        shutil.rmtree(temporary, ignore_errors=True)
        raise StageError(f"cannot stage Eclair root: {error}") from error
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("initramfs", type=Path)
    parser.add_argument("root_report", type=Path)
    parser.add_argument("system_report", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        report = stage(
            args.initramfs.resolve(),
            args.root_report.resolve(),
            args.system_report.resolve(),
            args.destination.resolve(),
        )
    except (OSError, StageError) as error:
        parser.error(str(error))
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
