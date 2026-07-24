#!/usr/bin/env python3
"""Fail-closed qualification for the direct N25 visible Linux probe."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

from qualify_n25_ramdiag import (
    QualificationError,
    check_config,
    check_elf_cpu,
    parse_newc,
    regular,
    run,
    sha256,
)


LINUX_REQUIRED = {
    "CONFIG_ARCH_S5L87XX": "y",
    "CONFIG_CPU_S5L8702": "y",
    "CONFIG_CPU_ARM926T": "y",
    "CONFIG_S5L8702_N25_EARLY_LCD_BREADCRUMB": "y",
    "CONFIG_FB": "y",
    "CONFIG_FB_S5L8702": "y",
    "CONFIG_USB_DWC2": "y",
    "CONFIG_USB_DWC2_PERIPHERAL": "y",
    "CONFIG_USB_GADGET": "y",
    "CONFIG_USB_G_SERIAL": "y",
    "CONFIG_BLK_DEV_INITRD": "y",
    "CONFIG_DEVTMPFS_MOUNT": "y",
    "CONFIG_POWER_RESET_S5L8702": "y",
    "CONFIG_HZ_PERIODIC": "y",
    "CONFIG_HZ_100": "y",
}
LINUX_FORBIDDEN = {
    "CONFIG_BLOCK",
    "CONFIG_ATA",
    "CONFIG_SCSI",
    "CONFIG_MMC",
    "CONFIG_MTD",
    "CONFIG_I2C",
    "CONFIG_SPI",
    "CONFIG_USB_MASS_STORAGE",
    "CONFIG_DEVMEM",
    "CONFIG_SOUND",
}
DT_STORAGE_NODE = re.compile(
    r"(?:ata|ceata|ide|sata|scsi|mmc|nand|flash|spi|i2c|eeprom|storage)@",
    re.IGNORECASE,
)


def check_wrapper(wrapper: Path, payload: bytes) -> None:
    padded = payload + b"\0" * ((-len(payload)) % 4)
    data = wrapper.read_bytes()
    if len(data) != len(padded) + 8:
        raise QualificationError(f"{wrapper}: wrong Rockbox container size")
    if data[4:8] != b"ip6g":
        raise QualificationError(f"{wrapper}: wrong Rockbox model tag")
    expected = (71 + sum(padded)) & 0xFFFFFFFF
    if int.from_bytes(data[:4], "big") != expected or data[8:] != padded:
        raise QualificationError(f"{wrapper}: checksum or body mismatch")


def check_dfu(path: Path, payload: bytes) -> None:
    data = path.read_bytes()
    if len(data) < 0x800:
        raise QualificationError(f"{path}: truncated IMG1")
    fields = struct.unpack("<4s3sBIIIII32sHH16s", data[:80])
    magic, version, image_format = fields[:3]
    entrypoint, body_len, data_len, cert_offset, cert_len = fields[3:8]
    if (magic, version, image_format) != (b"8702", b"1.0", 2):
        raise QualificationError(f"{path}: wrong S5L8702 IMG1 header")
    if entrypoint or data_len != body_len or cert_offset != body_len or cert_len:
        raise QualificationError(f"{path}: unsafe IMG1 execution/footer fields")
    if body_len > 0x1C000 or len(data) != 0x800 + body_len:
        raise QualificationError(f"{path}: exceeds conservative 112 KiB DFU body gate")
    body = data[0x800:]
    if body[: len(payload)] != payload or any(body[len(payload) :]):
        raise QualificationError(f"{path}: IMG1 body differs from loader")


def check_zimage(path: Path) -> None:
    data = path.read_bytes()
    if len(data) < 0x30 or int.from_bytes(data[0x24:0x28], "little") != 0x016F2818:
        raise QualificationError(f"{path}: ARM zImage magic missing")
    start = int.from_bytes(data[0x28:0x2C], "little")
    end = int.from_bytes(data[0x2C:0x30], "little")
    if start != 0 or end != len(data):
        raise QualificationError(f"{path}: inconsistent zImage bounds")


def check_image(path: Path) -> None:
    data = path.read_bytes()
    expected_entry = (
        0xE59F3068, 0xE59F4068, 0xE3A05E7E, 0xE3A06B0F,
        0xE5937000, 0xE3170010, 0x1AFFFFFC, 0xE5845000,
        0xE2566001, 0x1AFFFFF9, 0xE321F0D3,
    )
    actual_entry = tuple(
        int.from_bytes(data[offset:offset + 4], "little")
        for offset in range(0, len(expected_entry) * 4, 4)
    )
    if len(data) < 0x1000 or actual_entry != expected_entry:
        raise QualificationError(
            f"{path}: unexpected breadcrumb-enabled ARM Image entry"
        )


def check_dtb(path: Path, dtc: str, initramfs_size: int) -> None:
    source = run(dtc, "-q", "-I", "dtb", "-O", "dts", str(path))
    end = 0x09C00000 + initramfs_size
    markers = (
        'compatible = "apple,n25", "samsung,s5l8702"',
        'rockpod,safety-profile = "ram-only-no-storage"',
        'compatible = "apple,n25-lcd"',
        'compatible = "apple,s5l8702-vic"',
        'compatible = "apple,n25-timer", "samsung,s5l8702-timer"',
        "interrupts = <0x08>",
        "rockpod,preserve-display-handoff",
        "reg = <0x8000000 0x4000000>",
        "linux,initrd-start = <0x9c00000>",
        f"linux,initrd-end = <0x{end:x}>",
        'dr_mode = "peripheral"',
    )
    for marker in markers:
        if marker not in source:
            raise QualificationError(f"{path}: missing marker: {marker}")
    if source.count('compatible = "apple,s5l8702-vic"') != 2:
        raise QualificationError(f"{path}: expected exactly two N25 VIC nodes")
    if 'compatible = "arm,pl192-vic"' in source:
        raise QualificationError(f"{path}: generic PL192 VIC path is forbidden")
    if DT_STORAGE_NODE.search(source):
        raise QualificationError(f"{path}: persistent-bus node present")


def check_initramfs(path: Path, readelf: str, objdump: str) -> None:
    entries = parse_newc(gzip.decompress(path.read_bytes()))
    if set(entries) != {"dev", "init", "proc", "sys"}:
        raise QualificationError(f"{path}: unexpected RAM-root entries")
    mode, init = entries["init"]
    if mode & 0o170000 != 0o100000 or mode & 0o111 == 0:
        raise QualificationError(f"{path}: /init is not executable")
    for marker in (b"/dev/fb0", b"/dev/ttyGS0", b"display bands requested"):
        if marker not in init:
            raise QualificationError(f"{path}: missing PID 1 marker {marker!r}")
    for marker in (b"/dev/sd", b"/dev/mmc", b"/dev/mtd", b"/dev/hd"):
        if marker in init:
            raise QualificationError(f"{path}: storage path embedded in PID 1")
    with tempfile.NamedTemporaryFile(prefix="n25-visible-init-", suffix=".elf") as f:
        f.write(init)
        f.flush()
        check_elf_cpu(Path(f.name), readelf, objdump)


def check_lcd_emulations(
    paths: list[Path], image: Path, dtb: Path, initramfs: Path,
    loader_bin: Path, loader_elf: Path,
) -> None:
    if len(paths) != 4:
        raise QualificationError("exactly four N25 LCD strap reports are required")
    required_milestones = {
        "stext",
        "start_kernel",
        "s5l8702_clkevt_set_periodic",
        "s5l8702_timer_interrupt",
        "s5l_lcd_driver_init",
        "s5l_lcd_probe",
        "s5l_lcd_n25_first_frame_complete",
    }
    expected = {
        "image": sha256(image),
        "dtb": sha256(dtb),
        "initramfs": sha256(initramfs),
        "loader_bin": sha256(loader_bin),
        "loader_elf": sha256(loader_elf),
    }
    seen_straps = set()
    for path in paths:
        report = json.loads(path.read_text(encoding="utf-8"))
        strap = report.get("panel_strap")
        seen_straps.add(strap)
        milestones = set(report.get("milestones", []))
        injected = report.get("timer_irq_injections", 0)
        if report.get("gate_passed") is not True:
            raise QualificationError(f"{path}: ARM926 LCD gate did not pass")
        if report.get("cpu") != "ARM926EJ-S" or report.get("stop_at") != (
            "s5l_lcd_n25_first_frame_complete"
        ):
            raise QualificationError(f"{path}: wrong CPU or stopping milestone")
        if report.get("verify_n25_irq") is not True or report.get(
            "verify_n25_lcd"
        ) is not True:
            raise QualificationError(f"{path}: IRQ/LCD verification was not enabled")
        if report.get("rockbox_handoff_executed") is not True:
            raise QualificationError(f"{path}: Rockbox handoff was bypassed")
        entry_state = report.get("rockbox_linux_entry_state")
        if not isinstance(entry_state, dict) or {
            key: entry_state.get(key) for key in ("r0", "r1", "r2")
        } != {
            "r0": 0,
            "r1": 0xFFFFFFFF,
            "r2": 0x0AD00000,
        } or not isinstance(entry_state.get("cpsr"), int) or (
            entry_state["cpsr"] & 0xFF
        ) != 0xD3:
            raise QualificationError(
                f"{path}: complete Rockbox handoff entry state is wrong"
            )
        if report.get("rockbox_handoff_mmio_write_count", 0) < 20:
            raise QualificationError(
                f"{path}: complete Rockbox quiescence was not executed"
            )
        if not required_milestones.issubset(milestones):
            missing = ", ".join(sorted(required_milestones - milestones))
            raise QualificationError(
                f"{path}: missing execution milestones: {missing}"
            )
        if not isinstance(injected, int) or injected <= 0:
            raise QualificationError(f"{path}: no Timer B IRQ was injected")
        if not injected <= report.get("timer_irq_acks", 0) <= injected + 1:
            raise QualificationError(f"{path}: Timer B IRQ acknowledgement mismatch")
        if report.get("vic_address_reads", 0) < injected * 2 or report.get(
            "vic_address_completions", 0
        ) < injected * 2:
            raise QualificationError(
                f"{path}: PL192 handshake coverage is incomplete"
            )
        if report.get("lcd_pixel_writes") != 320 * 48 or report.get(
            "lcd_pixel_mismatches"
        ) != 0 or report.get("lcd_pixel_rgb565") != "0xfd20":
            raise QualificationError(f"{path}: amber trace band was not written")
        if report.get("loader_marker_pixels") != 320 * 48 or report.get(
            "loader_marker_mismatches"
        ) != 0 or report.get("loader_marker_rgb565") != "0xf800":
            raise QualificationError(
                f"{path}: post-cache loader trace band is incomplete"
            )
        if report.get("kernel_entry_marker_pixels") != 320 * 48 or report.get(
            "kernel_entry_marker_mismatches"
        ) != 0 or report.get("kernel_entry_marker_rgb565") != "0x07e0":
            raise QualificationError(
                f"{path}: pre-MMU kernel trace band is incomplete"
            )
        if report.get("timer_source_marker_pixels") != 320 * 48 or report.get(
            "timer_source_marker_rgb565"
        ) != "0xffe0":
            raise QualificationError(
                f"{path}: Timer E trace band is incomplete"
            )
        if report.get("timer_irq_marker_pixels") != 320 * 48 or report.get(
            "timer_irq_marker_rgb565"
        ) != "0x001f":
            raise QualificationError(
                f"{path}: Timer B IRQ trace band is incomplete"
            )
        if report.get("lcd_trace_band_pixels") != 320 * 48 or report.get(
            "lcd_trace_total_pixels"
        ) != 320 * 240:
            raise QualificationError(
                f"{path}: cumulative panel transaction is incomplete"
            )
        expected_config = (
            ["0x80000c20", "0x80100db0"]
            if strap in (0, 1)
            else ["0x80000da8", "0x80100db0"]
        )
        if report.get("lcd_config_writes") != expected_config:
            raise QualificationError(f"{path}: panel command-width path mismatch")
        actual = report.get("artifacts", {})
        for name, digest in expected.items():
            if actual.get(name) != digest:
                raise QualificationError(
                    f"{path}: {name} hash does not match artifact"
                )
    if seen_straps != {0, 1, 2, 3}:
        raise QualificationError(
            f"N25 LCD reports do not cover all panel straps: {seen_straps}"
        )


def check_n25_timer(vmlinux: Path, nm: str, objdump: str) -> None:
    data = vmlinux.read_bytes()
    for marker in (
        b"s5l8702-timer: clocksource did not advance",
        b"s5l8702-timerB",
    ):
        if marker not in data:
            raise QualificationError(
                f"{vmlinux}: N25 timer invariant missing: {marker!r}"
            )

    symbols = run(nm, "--defined-only", str(vmlinux))
    for symbol in (
        "s5l8702_timer_init",
        "s5l8702_clkevt_set_periodic",
        "s5l8702_clkevt_set_oneshot",
        "s5l8702_timer_interrupt",
    ):
        if symbol not in symbols:
            raise QualificationError(f"{vmlinux}: missing timer symbol {symbol}")

    timer_init = run(
        objdump,
        "-d",
        "--disassemble=s5l8702_timer_init",
        str(vmlinux),
    )
    for marker in (
        "movne\tr3, #32",
        "movne\tr3, #160",
        "movne\tr6, #32",
        "3c50004c",
        "0000ffff",
    ):
        if marker not in timer_init:
            raise QualificationError(
                f"{vmlinux}: N25 timer-init invariant missing: {marker}"
            )

    periodic = run(
        objdump,
        "-d",
        "--disassemble=s5l8702_clkevt_set_periodic",
        str(vmlinux),
    )
    # This pinned ARMv5 build must retain the exact Rockbox transaction:
    # CLR, prescaler 74, control 0x1240, interval 100, START.
    for marker in (
        "mov\tr0, #2",
        "mov\tip, #74",
        "mov\tr0, #4672",
        "mov\tr0, #100",
        "mov\tr3, #1",
    ):
        if marker not in periodic:
            raise QualificationError(
                f"{vmlinux}: periodic Timer B invariant missing: {marker}"
            )

    interrupt = run(
        objdump,
        "-d",
        "--disassemble=s5l8702_timer_interrupt",
        str(vmlinux),
    )
    # N25 clears Timer B exactly as Rockbox does: read TBCON and write the
    # same value back to the same register before calling event_handler.
    if not re.search(
        r"ldr\s+r2, \[r3\].*str\s+r2, \[r3\]", interrupt, re.DOTALL
    ):
        raise QualificationError(
            f"{vmlinux}: Timer B read/write acknowledgement missing"
        )


def check_n25_vic(vmlinux: Path, nm: str, objdump: str) -> None:
    data = vmlinux.read_bytes()
    if b"apple,s5l8702-vic" not in data:
        raise QualificationError(f"{vmlinux}: N25 VIC compatible missing")

    symbols = run(nm, "--defined-only", str(vmlinux))
    for symbol in ("vic_handle_irq", "vic_of_init", "__vic_init"):
        if symbol not in symbols:
            raise QualificationError(f"{vmlinux}: missing VIC symbol {symbol}")

    handler = run(objdump, "-d", "--disassemble=vic_handle_irq", str(vmlinux))
    init = run(objdump, "-d", "--disassemble=__vic_init", str(vmlinux))
    # The N25 quirk must access PL192 VICADDRESS at +0xf00 around status
    # dispatch. This is the priority-stack entry/exit handshake Rockbox uses.
    if not re.search(r"ldr\s+r\d+, \[r\d+, #3840\]", handler) or not re.search(
        r"str\s+r\d+, \[r\d+, #3840\]", handler
    ):
        raise QualificationError(f"{vmlinux}: PL192 VICADDRESS handshake missing")
    for marker in ("38e02000", "#3840", "[r3, #8]", "[r3, #12]"):
        if marker not in init:
            raise QualificationError(
                f"{vmlinux}: N25 VIC initialization invariant missing: {marker}"
            )


def check_loader(
    elf: Path, body: bytes, nm: str, readelf: str, objdump: str
) -> None:
    markers = (
        b"N25 LINUX VISIBLE PROBE",
        b"RAM ONLY / STORAGE ABSENT",
        b"WHITE TEXT = HANDOFF",
        b"AMBER = KERNEL DISPLAY",
        b"3 BANDS = PID 1",
        b"/.rockbox/android/diagnostic-trace2/n25-visible-kernel.ipod",
        b"/.rockbox/android/diagnostic-trace2/n25-visible-initramfs.ipod",
        b"/.rockbox/android/diagnostic-trace2/n25-visible-dtb.ipod",
    )
    for marker in markers:
        if marker not in body:
            raise QualificationError(f"loader marker missing: {marker!r}")
    if b"Android 2.0 RAM boot" in body:
        raise QualificationError("visible probe accidentally contains Android boot mode")
    if b"/android/diagnostic-vic1/" in body:
        raise QualificationError("loader still references the failed VIC1 packet")
    if b"/android/diagnostic-lcd1/" in body:
        raise QualificationError("loader still references the failed LCD1 packet")
    if b"/android/diagnostic-trace1/" in body:
        raise QualificationError("loader still references the failed TRACE1 packet")
    symbols = run(nm, "--defined-only", str(elf))
    for marker in (
        "n25_android_boot",
        "n25_android_handoff",
        "n25_android_quiesce",
        "n25_android_linux_jump",
        "lcd_wait_for_dma",
        "lcd_prepare_for_pio_trace",
        "dmac_open",
    ):
        if marker not in symbols:
            raise QualificationError(f"{elf}: missing {marker}")

    boot = run(objdump, "-d", "--disassemble=n25_android_boot", str(elf))
    wait_call = boot.find("<lcd_wait_for_dma>")
    handoff_call = boot.find("<n25_android_handoff>")
    if wait_call < 0 or handoff_call < 0 or wait_call >= handoff_call:
        raise QualificationError(
            f"{elf}: LCD DMA is not drained before handoff quiesce"
        )

    handoff = run(
        objdump, "-d", "--disassemble=n25_android_handoff", str(elf)
    )
    handoff_calls = [
        handoff.find("<lcd_prepare_for_pio_trace>"),
        handoff.find("<n25_android_quiesce>"),
        handoff.find("<commit_discard_idcache>"),
        handoff.find("<n25_android_linux_jump>"),
    ]
    if any(call < 0 for call in handoff_calls) or handoff_calls != sorted(
        handoff_calls
    ):
        raise QualificationError(
            f"{elf}: handoff order is not panel-prepare/quiesce/cache/jump"
        )

    quiesce = run(
        objdump, "-d", "--disassemble=n25_android_quiesce", str(elf)
    )
    if "<dmac_open>" not in quiesce or struct.pack("<I", 0x38200000) not in body:
        raise QualificationError(f"{elf}: S5L8702 DMA0 shutdown is missing")

    jump = run(
        objdump, "-d", "--disassemble=n25_android_linux_jump", str(elf)
    )
    jump_markers = (
        "mov\tr4, r0",
        "mov\tr2, r1",
        "orr\tr3, r3, #211",
        "mrc\t15, 0, r3, cr1, cr0",
        "bic\tr3, r3, #4096",
        "bic\tr3, r3, #5",
        "mcr\t15, 0, r3, cr1, cr0",
        "mov\tr0, #0",
        "mvn\tr1, #0",
        "bx\tr4",
    )
    for marker in jump_markers:
        if marker not in jump:
            raise QualificationError(
                f"{elf}: ARM Linux entry invariant missing: {marker}"
            )
    sections = run(readelf, "-SW", str(elf))
    match = re.search(
        r"\]\s+\.bss\s+NOBITS\s+([0-9a-fA-F]+)\s+\S+\s+([0-9a-fA-F]+)",
        sections,
    )
    if match is None or int(match.group(1), 16) + int(match.group(2), 16) >= 0x09C00000:
        raise QualificationError(f"{elf}: BSS overlaps the RAM root")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--linux-dir", required=True)
    parser.add_argument("--initramfs", required=True)
    parser.add_argument("--kernel-ipod", required=True)
    parser.add_argument("--initramfs-ipod", required=True)
    parser.add_argument("--dtb-ipod", required=True)
    parser.add_argument("--loader-bin", required=True)
    parser.add_argument("--loader-elf", required=True)
    parser.add_argument("--loader-ipod", required=True)
    parser.add_argument("--loader-dfu", required=True)
    parser.add_argument("--lcd-emulation", action="append", required=True)
    parser.add_argument("--dtc", default="dtc")
    parser.add_argument("--readelf", default="arm-none-eabi-readelf")
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    args = parser.parse_args(argv)

    linux = Path(args.linux_dir).resolve(strict=True)
    artifacts = {
        "zimage": regular(str(linux / "arch/arm/boot/zImage")),
        "image": regular(str(linux / "arch/arm/boot/Image")),
        "dtb": regular(str(linux / "arch/arm/boot/dts/samsung/s5l8702-n25-ramdiag.dtb")),
        "vmlinux": regular(str(linux / "vmlinux")),
        "initramfs": regular(args.initramfs),
        "kernel_ipod": regular(args.kernel_ipod),
        "initramfs_ipod": regular(args.initramfs_ipod),
        "dtb_ipod": regular(args.dtb_ipod),
        "loader_bin": regular(args.loader_bin),
        "loader_elf": regular(args.loader_elf),
        "loader_ipod": regular(args.loader_ipod),
        "loader_dfu": regular(args.loader_dfu),
    }
    lcd_emulations = [regular(path) for path in args.lcd_emulation]
    for index, path in enumerate(lcd_emulations):
        artifacts[f"lcd_emulation_{index}"] = path

    check_config(linux / ".config", LINUX_REQUIRED, LINUX_FORBIDDEN)
    check_elf_cpu(artifacts["vmlinux"], args.readelf, args.objdump)
    check_n25_vic(artifacts["vmlinux"], args.nm, args.objdump)
    check_n25_timer(artifacts["vmlinux"], args.nm, args.objdump)
    check_zimage(artifacts["zimage"])
    check_image(artifacts["image"])
    check_dtb(artifacts["dtb"], args.dtc, artifacts["initramfs"].stat().st_size)
    check_initramfs(artifacts["initramfs"], args.readelf, args.objdump)
    check_lcd_emulations(
        lcd_emulations, artifacts["image"], artifacts["dtb"],
        artifacts["initramfs"], artifacts["loader_bin"],
        artifacts["loader_elf"],
    )
    check_wrapper(artifacts["kernel_ipod"], artifacts["image"].read_bytes())
    check_wrapper(artifacts["initramfs_ipod"], artifacts["initramfs"].read_bytes())
    check_wrapper(artifacts["dtb_ipod"], artifacts["dtb"].read_bytes())
    loader = artifacts["loader_bin"].read_bytes()
    check_wrapper(artifacts["loader_ipod"], loader)
    check_dfu(artifacts["loader_dfu"], loader)
    check_loader(
        artifacts["loader_elf"], loader, args.nm, args.readelf, args.objdump
    )

    kernel_end = 0x08008000 + artifacts["image"].stat().st_size
    if kernel_end >= 0x09C00000:
        raise QualificationError("uncompressed kernel overlaps the RAM root")
    if 0x09C00000 + artifacts["initramfs"].stat().st_size >= 0x0AD00000:
        raise QualificationError("RAM root overlaps the device tree")

    report = {
        "artifact_gate_passed": True,
        "hardware_qualified": False,
        "device_test_ready": False,
        "last_hardware_result": (
            "diagnostic-trace2 retained the Rockbox legend ending at "
            "3 BANDS = PID 1 and is disqualified"
        ),
        "qualification_note": (
            "The host gate passes, but the physical TRACE2 result did not "
            "produce its first red band. No Rockbox-to-Linux visible packet "
            "is approved for another device test. Continue only with the "
            "independent storage-free volatile U-Boot enumeration gate."
        ),
        "board": "apple-n25-ipod-classic-6g-7g",
        "profile": "visible-ram-only-no-storage",
        "kernel_boot_format": "uncompressed-arm-image",
        "kernel_entry_address": "0x08008000",
        "dfu_load_address": "0x22000000",
        "linux_storage_access": False,
        "persistent_writes_after_linux_handoff": False,
        "manual_recovery": "Hold Menu+Select for approximately 8-10 seconds",
        "automatic_recovery_verified": False,
        "milestones": {
            "rockbox_handoff": "white text",
            "post_cache_loader": "top red band",
            "raw_kernel_entry": "red plus green bands",
            "timer_e_live": "red, green, yellow bands",
            "timer_b_irq": "red, green, yellow, blue bands",
            "linux_framebuffer_probe": "five bands ending in amber",
            "linux_pid1": "green-blue-white bands",
            "linux_usb_userspace": "ROCKPOD N25 RAMDIAG heartbeat",
        },
        "artifacts": {
            name: {
                "path": str(path),
                "size": path.stat().st_size,
                "sha256": sha256(path),
            }
            for name, path in artifacts.items()
        },
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (QualificationError, subprocess.CalledProcessError) as error:
        print(json.dumps({"artifact_gate_passed": False, "error": str(error)}))
        raise SystemExit(1)
