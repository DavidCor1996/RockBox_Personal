#!/usr/bin/env python3
"""Validate inputs and emit a non-executing N81 OpeniBoot RAM boot plan."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


UPLOAD_ADDRESS = 0x4D000000
KERNEL_MAX = 8 * 1024 * 1024
INITRD_MAX = 20 * 1024 * 1024
ZIMAGE_MAGIC_OFFSET = 0x24
ZIMAGE_MAGIC = bytes.fromhex("18286f01")
DEFAULT_COMMAND_LINE = (
    "console=tty0 console=ttySAC0,115200 rdinit=/init rw "
    "ipodtouch4.storage=disabled"
)
FORBIDDEN_COMMAND_LINE = (
    "/dev/mmc",
    "/dev/mtd",
    "/dev/nvme",
    "/dev/sd",
    "ubi.",
    "nand",
    "hfs",
    "apfs",
    "rootfstype=ext",
)


class PlanError(RuntimeError):
    """Raised when inputs cannot enter the storage-free RAM boot flow."""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def regular_bytes(path: Path, label: str, maximum: int) -> bytes:
    if path.is_symlink() or not path.is_file():
        raise PlanError(f"{label} must be a regular, non-symlink file")
    data = path.read_bytes()
    if not data or len(data) > maximum:
        raise PlanError(f"{label} must be between 1 and {maximum} bytes")
    return data


def check_command_line(command_line: str) -> None:
    if not command_line or len(command_line.encode("utf-8")) > 511:
        raise PlanError("kernel command line must be 1 through 511 UTF-8 bytes")
    if "\0" in command_line or "\n" in command_line or "\r" in command_line:
        raise PlanError("kernel command line contains a control delimiter")
    lowered = command_line.lower()
    found = [marker for marker in FORBIDDEN_COMMAND_LINE if marker in lowered]
    if found:
        raise PlanError(f"kernel command line enables local storage: {found}")
    if not any(marker in lowered for marker in ("root=/dev/ram", "rdinit=/init")):
        raise PlanError("kernel command line lacks a RAM root or initramfs PID 1")
    if "ipodtouch4.storage=disabled" not in lowered:
        raise PlanError("kernel command line lacks the storage-disable marker")


def verified_loader(build: Path) -> dict[str, object]:
    qualification_path = build / "qualification.json"
    try:
        qualification = json.loads(qualification_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise PlanError(f"qualification report is invalid: {error}") from error
    required = {
        "artifact_gate_passed": True,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
        "storage_controller_symbols_present": False,
        "storage_write_commands_present": False,
        "fixed_kernel_initrd_regions": True,
        "load_address": 0x84000000,
        "in_place_shatter_execution": True,
        "in_place_exception_vectors": True,
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
    }
    for name, expected in required.items():
        if qualification.get(name) != expected:
            raise PlanError(f"qualification report has unexpected {name}")
    for name, filename in (
        ("elf", "openiboot-n81-volatile.elf"),
        ("bin", "openiboot-n81-volatile.bin"),
    ):
        data = regular_bytes(build / filename, f"loader {name}", 2 * 1024 * 1024)
        recorded = qualification.get("artifacts", {}).get(name, {})
        if recorded.get("size") != len(data) or recorded.get("sha256") != sha256_bytes(data):
            raise PlanError(f"loader {name} does not match qualification report")
    return qualification


def verified_kernel(kernel_path: Path, kernel: bytes) -> dict[str, object]:
    report_path = kernel_path.parent / "kernel-qualification.json"
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise PlanError(f"kernel qualification report is invalid: {error}") from error
    required = {
        "artifact_gate_passed": True,
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": 3564,
        "storage_subsystems_configured": False,
        "storage_controller_symbols_present": False,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
    }
    for name, expected in required.items():
        if report.get(name) != expected:
            raise PlanError(f"kernel qualification report has unexpected {name}")
    recorded = report.get("artifacts", {}).get("zimage", {})
    if recorded.get("size") != len(kernel) or recorded.get("sha256") != sha256_bytes(
        kernel
    ):
        raise PlanError("kernel does not match qualification report")
    return report


def verified_initramfs(initrd_path: Path, initrd: bytes) -> dict[str, object]:
    ramdiag_path = initrd_path.parent / "ramdiag-qualification.json"
    eclair_path = initrd_path.parent / "eclair-n81-qualification.json"
    report_path = ramdiag_path if ramdiag_path.is_file() else eclair_path
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise PlanError(f"RAM diagnostic qualification report is invalid: {error}") from error
    common = {
        "artifact_gate_passed": True,
        "board": "apple-n81-ipod-touch-4g",
        "storage_paths_present": False,
        "shell_present": False,
        "hardware_actions_enabled": False,
        "device_test_ready": False,
    }
    for name, expected in common.items():
        if report.get(name) != expected:
            raise PlanError(f"initramfs qualification report has unexpected {name}")
    profile = report.get("profile")
    if profile == "n81-volatile-ramdiag-no-storage":
        if report.get("automatic_reboot_seconds") != 30:
            raise PlanError("RAM diagnostic report lacks its 30-second reboot")
    elif profile == "n81-eclair-2.0-volatile-no-storage":
        for name in (
            "host_full_system_gate_passed",
            "binder_userspace_packaged",
            "zygote_packaged",
            "surfaceflinger_packaged",
            "software_renderer_packaged",
            "linux_framebuffer_gralloc_path_packaged",
            "tmpfs_mutable_state",
            "root_read_only",
        ):
            if report.get(name) is not True:
                raise PlanError(f"Eclair qualification report lacks {name}")
    else:
        raise PlanError("initramfs qualification report has an unknown profile")
    recorded = report.get("artifacts", {}).get("initramfs", {})
    if recorded.get("size") != len(initrd) or recorded.get("sha256") != sha256_bytes(
        initrd
    ):
        raise PlanError("initramfs does not match qualification report")
    return report


def make_plan(
    build: Path, kernel_path: Path, initrd_path: Path, command_line: str
) -> dict[str, object]:
    qualification = verified_loader(build)
    kernel = regular_bytes(kernel_path, "kernel", KERNEL_MAX)
    initrd = regular_bytes(initrd_path, "initramfs", INITRD_MAX)
    magic_end = ZIMAGE_MAGIC_OFFSET + len(ZIMAGE_MAGIC)
    if len(kernel) < magic_end or kernel[ZIMAGE_MAGIC_OFFSET:magic_end] != ZIMAGE_MAGIC:
        raise PlanError("kernel is not an ARM zImage")
    if not (initrd.startswith(b"\x1f\x8b") or initrd.startswith(b"070701")):
        raise PlanError("initramfs is neither gzip nor newc cpio")
    kernel_report = verified_kernel(kernel_path, kernel)
    initrd_report = verified_initramfs(initrd_path, initrd)
    check_command_line(command_line)
    eclair = initrd_report["profile"] == "n81-eclair-2.0-volatile-no-storage"
    if eclair:
        for name in (
            "android_binder_compiled",
            "android_logger_compiled",
            "android_lowmemorykiller_compiled",
        ):
            if kernel_report.get(name) is not True:
                raise PlanError(f"Eclair kernel qualification lacks {name}")
        if "androidboot.hardware=n81" not in command_line.lower():
            raise PlanError("Eclair command line lacks androidboot.hardware=n81")

    def upload(kind: str, body: bytes) -> dict[str, object]:
        return {
            "kind": kind,
            "size": len(body),
            "sha256": sha256_bytes(body),
            "sendfile_command": f"sendfile 0x{UPLOAD_ADDRESS:08x} {len(body)}",
        }

    return {
        "schema": 1,
        "profile": initrd_report["profile"],
        "product_type": "iPod4,1",
        "hardware_model": "N81AP",
        "machine_id": qualification["machine_id"],
        "loader_sha256": qualification["artifacts"]["bin"]["sha256"],
        "hardware_actions_enabled": False,
        "transport_implemented": True,
        "device_test_ready": False,
        "command_line": command_line,
        "steps": [
            upload("kernel", kernel),
            {"command": f'kernel "{command_line}"'},
            upload("initramfs", initrd),
            {"command": "initrd"},
            {"command": "boot", "requires_separate_physical_gate": True},
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("build", type=Path)
    parser.add_argument("kernel", type=Path)
    parser.add_argument("initramfs", type=Path)
    parser.add_argument("--command-line", default=DEFAULT_COMMAND_LINE)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        plan = make_plan(
            args.build.resolve(),
            args.kernel.resolve(),
            args.initramfs.resolve(),
            args.command_line,
        )
    except (OSError, PlanError) as error:
        parser.error(str(error))
    rendered = json.dumps(plan, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
