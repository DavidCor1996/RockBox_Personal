#!/usr/bin/env python3
"""Qualify the storage-free Rockbox-to-Linux N25 USB-serial probe."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess

from qualify_n25_eclair_native import (
    check_boot_chord_report,
    check_no_diagnostic_lcd_handoff,
    check_rockbox_wrapper,
    read_android_image_contract,
)
from qualify_n25_ramdiag import (
    LINUX_FORBIDDEN,
    LINUX_REQUIRED,
    QualificationError,
    check_config,
    check_dfu,
    check_elf_cpu,
    check_initramfs,
    decompile_dtb,
    regular,
    run,
    sha256,
)


def check_headless_dtb(path: Path, dtc: str) -> None:
    source = decompile_dtb(path, dtc)
    required = (
        'compatible = "apple,n25", "samsung,s5l8702"',
        'rockpod,safety-profile = "ram-only-no-storage"',
        'dr_mode = "peripheral"',
        'compatible = "apple,n25-timer", "samsung,s5l8702-timer"',
        'compatible = "apple,s5l8702-watchdog-reset"',
        "apple,test-timeout-seconds = <0x3c>",
    )
    for marker in required:
        if marker not in source:
            raise QualificationError(f"{path}: missing headless marker: {marker}")
    forbidden = re.compile(
        r"(?:ata|ceata|ide|sata|scsi|mmc|nand|flash|spi|i2c|eeprom|storage)@",
        re.IGNORECASE,
    )
    if forbidden.search(source):
        raise QualificationError(f"{path}: persistent or I2C bus node found")


def check_forced_loader(
    dfu: Path,
    payload: Path,
    elf: Path,
    report_path: Path,
    sizes: tuple[int, int, int],
    objdump: str,
    nm: str,
) -> dict:
    check_dfu(dfu, payload)
    body = payload.read_bytes()
    for marker in (
        b"N25 HEADLESS USB2 PROBE",
        b"RAM ONLY / STORAGE ABSENT",
        b"/.rockbox/android/diagnostic-headless-usb2/n25-headless-kernel.ipod",
        b"/.rockbox/android/diagnostic-headless-usb2/n25-headless-initramfs.ipod",
        b"/.rockbox/android/diagnostic-headless-usb2/n25-headless-dtb.ipod",
    ):
        if marker not in body:
            raise QualificationError(
                f"{dfu}: headless loader marker missing: {marker.decode()}"
            )
    for forbidden in (
        b"/.rockbox/android/n25-eclair-kernel.ipod",
        b"/.rockbox/android/diagnostic-trace2/",
        b"/.rockbox/android/diagnostic-headless-usb1/",
    ):
        if forbidden in body:
            raise QualificationError(f"{dfu}: unrelated payload path is linked")

    symbols = run(nm, str(elf))
    for symbol in (
        "n25_android_force_volatile_test",
        "n25_android_boot",
        "n25_android_linux_jump",
        "n25_android_image_contract",
    ):
        if symbol not in symbols:
            raise QualificationError(f"{elf}: missing {symbol}")
    main = run(objdump, "-d", "--disassemble=main", str(elf))
    for symbol in ("n25_android_force_volatile_test", "n25_android_boot"):
        if f"<{symbol}>" not in main:
            raise QualificationError(f"{elf}: main does not call {symbol}")
    check_no_diagnostic_lcd_handoff(elf, objdump)
    compiled_sizes = read_android_image_contract(payload, elf, nm)
    if compiled_sizes != sizes:
        raise QualificationError(
            f"{elf}: compiled sizes {compiled_sizes} do not match {sizes}"
        )

    report = check_boot_chord_report(report_path, payload)
    forced = report.get("forced_cases", {})
    if (
        report.get("forced_android_volatile_test") is not True
        or report.get("forced_button_patterns_tested") != 128
        or report.get("forced_main_calls_qualified_function") is not True
        or set(forced) != {f"0x{buttons:02x}" for buttons in range(128)}
        or not all(value is True for value in forced.values())
    ):
        raise QualificationError(f"{report_path}: forced boot gate failed")
    return report


def check_chain_report(
    path: Path,
    kernel: Path,
    dtb: Path,
    initramfs: Path,
    loader: Path,
) -> dict:
    report = json.loads(path.read_text(encoding="utf-8"))
    required = {
        "gate_passed": True,
        "cpu": "ARM926EJ-S",
        "image_format": "zImage",
        "image_sha256": sha256(kernel),
        "dtb_sha256": sha256(dtb),
        "initramfs_sha256": sha256(initramfs),
        "rockbox_bootloader_sha256": sha256(loader),
        "stop_at": "gserial_init",
        "model_n25_timer": True,
        "verify_n25_irq": True,
        "rockbox_handoff_executed": True,
    }
    for name, expected in required.items():
        if report.get(name) != expected:
            raise QualificationError(f"{path}: chain gate failed: {name}")
    milestones = set(report.get("milestones", ()))
    required_milestones = {
        "stext",
        "__turn_mmu_on",
        "start_kernel",
        "s5l8702_timer_interrupt",
        "of_platform_default_populate_init",
        "gserial_init",
    }
    injections = report.get("timer_irq_injections", 0)
    if (
        not required_milestones.issubset(milestones)
        or injections <= 0
        or report.get("timer_irq_acks") != injections
        or report.get("vic_address_reads", 0) < injections * 2
        or report.get("vic_address_completions", 0) < injections * 2
    ):
        raise QualificationError(f"{path}: chain interrupt details failed")
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--linux-dir", required=True)
    parser.add_argument("--initramfs", required=True)
    parser.add_argument("--kernel-ipod", required=True)
    parser.add_argument("--initramfs-ipod", required=True)
    parser.add_argument("--dtb-ipod", required=True)
    parser.add_argument("--loader-dfu", required=True)
    parser.add_argument("--loader-bin", required=True)
    parser.add_argument("--loader-elf", required=True)
    parser.add_argument("--forced-report", required=True)
    parser.add_argument("--chain-report", required=True)
    parser.add_argument("--dtc", default="dtc")
    parser.add_argument("--readelf", default="arm-none-eabi-readelf")
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    args = parser.parse_args(argv)

    linux = Path(args.linux_dir).resolve(strict=True)
    artifacts = {
        "zimage": regular(str(linux / "arch/arm/boot/zImage")),
        "linux_image": regular(str(linux / "arch/arm/boot/Image")),
        "linux_elf": regular(str(linux / "vmlinux")),
        "linux_dtb": regular(str(
            linux / "arch/arm/boot/dts/samsung/s5l8702-n25-ramdiag.dtb"
        )),
        "initramfs": regular(args.initramfs),
        "kernel_ipod": regular(args.kernel_ipod),
        "initramfs_ipod": regular(args.initramfs_ipod),
        "dtb_ipod": regular(args.dtb_ipod),
        "loader_dfu": regular(args.loader_dfu),
        "loader_bin": regular(args.loader_bin),
        "loader_elf": regular(args.loader_elf),
        "forced_report": regular(args.forced_report),
        "chain_report": regular(args.chain_report),
    }

    check_config(linux / ".config", LINUX_REQUIRED, LINUX_FORBIDDEN)
    check_elf_cpu(artifacts["linux_elf"], args.readelf, args.objdump)
    check_headless_dtb(artifacts["linux_dtb"], args.dtc)
    check_initramfs(artifacts["initramfs"], args.readelf, args.objdump)
    check_rockbox_wrapper(artifacts["kernel_ipod"], artifacts["zimage"])
    check_rockbox_wrapper(artifacts["initramfs_ipod"], artifacts["initramfs"])
    check_rockbox_wrapper(artifacts["dtb_ipod"], artifacts["linux_dtb"])
    sizes = (
        artifacts["kernel_ipod"].stat().st_size - 8,
        artifacts["initramfs_ipod"].stat().st_size - 8,
        artifacts["dtb_ipod"].stat().st_size - 8,
    )
    check_forced_loader(
        artifacts["loader_dfu"],
        artifacts["loader_bin"],
        artifacts["loader_elf"],
        artifacts["forced_report"],
        sizes,
        args.objdump,
        args.nm,
    )
    check_chain_report(
        artifacts["chain_report"],
        artifacts["zimage"],
        artifacts["linux_dtb"],
        artifacts["initramfs"],
        artifacts["loader_bin"],
    )
    if artifacts["zimage"].stat().st_size >= 0x01C00000:
        raise QualificationError("kernel overlaps the fixed initramfs load")

    report = {
        "artifact_gate_passed": True,
        "device_test_ready": True,
        "hardware_qualified": False,
        "profile": "headless-usb-serial-ram-only-no-storage",
        "storage_code_present": False,
        "persistent_storage_available": False,
        "hardware_actions_enabled": False,
        "rockbox_handoff_binary_emulated": True,
        "usb_serial_initcall_reached": True,
        "automatic_recovery_seconds": 60,
        "artifacts": {
            name: {
                "path": path.name,
                "size": path.stat().st_size,
                "sha256": sha256(path),
            }
            for name, path in artifacts.items()
            if not name.endswith("_elf")
        },
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (QualificationError, json.JSONDecodeError,
            subprocess.CalledProcessError) as error:
        print(json.dumps({"artifact_gate_passed": False, "error": str(error)}))
        raise SystemExit(1)
