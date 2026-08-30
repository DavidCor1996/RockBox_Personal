#!/usr/bin/env python3
"""Reject an unsafe iPod 6G Stage-3 hibernate app/bootloader pair."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


NM = "arm-elf-eabi-nm"
OBJDUMP = "arm-elf-eabi-objdump"


class GateError(RuntimeError):
    pass


def command(*args: str) -> str:
    result = subprocess.run(
        args, check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise GateError(
            f"{' '.join(args)} failed ({result.returncode}):\n{result.stderr}"
        )
    return result.stdout


def symbol_table(elf: Path) -> dict[str, tuple[int, int | None]]:
    symbols: dict[str, tuple[int, int | None]] = {}
    for line in command(NM, "-S", "-n", str(elf)).splitlines():
        match = re.match(
            r"^([0-9a-fA-F]+)(?:\s+([0-9a-fA-F]+))?\s+[Tt]\s+(\S+)$",
            line,
        )
        if match:
            address = int(match.group(1), 16)
            size = int(match.group(2), 16) if match.group(2) else None
            symbols[match.group(3)] = (address, size)
    return symbols


def require_symbol(
    symbols: dict[str, tuple[int, int | None]], name: str,
    *, minimum_size: int = 0,
) -> int:
    if name not in symbols:
        raise GateError(f"missing linked symbol: {name}")
    address, size = symbols[name]
    if minimum_size and (size is None or size < minimum_size):
        rendered = "unknown" if size is None else f"0x{size:x}"
        raise GateError(
            f"{name} is a disabled/stale stub: size {rendered}, "
            f"need at least 0x{minimum_size:x}"
        )
    return address


def function(disassembly: str, name: str) -> str:
    pattern = re.compile(
        rf"^[0-9a-fA-F]+ <{re.escape(name)}>:\n(.*?)"
        rf"(?=^[0-9a-fA-F]+ <|\Z)",
        re.MULTILINE | re.DOTALL,
    )
    match = pattern.search(disassembly)
    if not match:
        raise GateError(f"cannot isolate disassembly for {name}")
    return match.group(1)


def require_order(body: str, names: list[str], owner: str) -> None:
    positions: list[int] = []
    for name in names:
        marker = f"<{name}>"
        position = body.find(marker)
        if position < 0:
            raise GateError(f"{owner} does not call {name}")
        positions.append(position)
    if positions != sorted(positions):
        raise GateError(f"{owner} has unsafe call order: {' -> '.join(names)}")


def version(info: Path) -> str:
    match = re.search(r"^Version:\s*(\S+)\s*$", info.read_text(), re.MULTILINE)
    if not match:
        raise GateError(f"missing Version field in {info}")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("app_elf", type=Path)
    parser.add_argument("boot_elf", type=Path)
    parser.add_argument("--app-info", type=Path, required=True)
    parser.add_argument("--boot-info", type=Path, required=True)
    args = parser.parse_args()

    for path in (args.app_elf, args.boot_elf, args.app_info, args.boot_info):
        if not path.is_file():
            raise GateError(f"missing input: {path}")

    app_symbols = symbol_table(args.app_elf)
    boot_symbols = symbol_table(args.boot_elf)

    checkpoint = require_symbol(
        app_symbols, "ipod6g_hibernate_stage3_checkpoint", minimum_size=0x60
    )
    require_symbol(
        app_symbols, "ipod6g_hibernate_stage3_enter", minimum_size=0x100
    )
    require_symbol(
        app_symbols, "ipod6g_hibernate_stage3_suspend", minimum_size=0x100
    )
    require_symbol(app_symbols, "i2c_bus_lock", minimum_size=0x10)
    require_symbol(app_symbols, "i2c_bus_unlock", minimum_size=0x10)
    for helper in (
        "stage3_mask_vic", "stage3_rearm_tick",
        "stage3_finish_display", "stage3_lcd_checkpoint",
        "stage3_restore_all_vic",
        "system_hibernate_resume_usec_timer",
        "lcd_hibernate_run_seq8", "lcd_hibernate_run_seq16",
        "lcd_hibernate_run_awake_sequence", "lcd_hibernate_wait_dma",
    ):
        require_symbol(app_symbols, helper, minimum_size=0x08)
    require_symbol(app_symbols, "lcd_hibernate_finish_resume", minimum_size=0x40)
    require_symbol(app_symbols, "lcd_hibernate_resume", minimum_size=0x40)
    require_symbol(app_symbols, "lcd_hibernate_resume_complete", minimum_size=0x08)
    resume = require_symbol(
        boot_symbols, "ipod6g_hibernate_stage3_resume", minimum_size=0x40
    )
    require_symbol(
        boot_symbols, "ipod6g_hibernate_validate_after_wake", minimum_size=0x400
    )

    if not 0x08000000 <= checkpoint < 0x08001000:
        raise GateError(f"checkpoint is outside retained payload: 0x{checkpoint:08x}")
    if not 0x22020000 <= resume < 0x22040000:
        raise GateError(f"resume trampoline is outside IRAM1: 0x{resume:08x}")

    app_dis = command(OBJDUMP, "-d", str(args.app_elf))
    boot_dis = command(OBJDUMP, "-d", str(args.boot_elf))
    suspend = function(app_dis, "ipod6g_hibernate_stage3_suspend")
    enter = function(app_dis, "ipod6g_hibernate_stage3_enter")
    rearm_tick = function(app_dis, "stage3_rearm_tick")
    finish_display = function(app_dis, "stage3_finish_display")
    finish_lcd = function(app_dis, "lcd_hibernate_finish_resume")
    prepare_lcd = function(app_dis, "lcd_hibernate_resume")
    finish_lcd_event = function(app_dis, "lcd_hibernate_resume_complete")
    run_seq8 = function(app_dis, "lcd_hibernate_run_seq8")
    run_seq16 = function(app_dis, "lcd_hibernate_run_seq16")
    run_awake = function(app_dis, "lcd_hibernate_run_awake_sequence")
    poll_dma = function(app_dis, "lcd_hibernate_wait_dma")
    system_resume = function(app_dis, "system_hibernate_resume")
    usec_timer = function(app_dis, "system_hibernate_resume_usec_timer")
    restore_all_vic = function(app_dis, "stage3_restore_all_vic")
    validator = function(boot_dis, "ipod6g_hibernate_validate_after_wake")
    trampoline = function(boot_dis, "ipod6g_hibernate_stage3_resume")

    require_order(
        suspend,
        [
            "i2c_bus_lock", "ipod6g_hibernate_stage3_enter",
            "i2c_bus_unlock", "stage3_finish_display",
            "stage3_rearm_tick", "stage3_restore_all_vic",
            "pmu_hibernate_resume_complete",
            "lcd_hibernate_resume_complete",
        ],
        "ipod6g_hibernate_stage3_suspend",
    )
    lock_end = suspend.find("<i2c_bus_lock>")
    enter_start = suspend.find("<ipod6g_hibernate_stage3_enter>")
    serialized_boundary = suspend[lock_end:enter_start]
    if "mrs" not in serialized_boundary or "msr" not in serialized_boundary:
        raise GateError("IRQ/FIQ are not masked after I2C ownership and before checkpoint")

    display_start = suspend.find("<stage3_finish_display>")
    tick_start = suspend.find("<stage3_rearm_tick>")
    restore_start = suspend.find("<stage3_restore_all_vic>")
    pmu_start = suspend.find("<pmu_hibernate_resume_complete>")
    irq_release = suspend[restore_start:pmu_start]
    if not display_start < tick_start < restore_start:
        raise GateError("scheduler hardware is exposed before display repair")
    if "msr" not in irq_release:
        raise GateError("CPU IRQ/FIQ are not released after full VIC restore")

    for forbidden in ("<sleep>", "<yield>", "<mutex_lock>"):
        if forbidden in suspend[enter_start:restore_start]:
            raise GateError(
                f"pre-interrupt resume path contains scheduler call {forbidden}"
            )

    if "<tick_start>" not in rearm_tick:
        raise GateError("timer evidence helper does not run the target tick setup")

    require_order(
        finish_display,
        ["lcd_hibernate_finish_resume", "backlight_hibernate_resume"],
        "stage3_finish_display",
    )
    require_order(
        finish_lcd,
        [
            "lcd_target_enable_clocks", "__s5l_lcd_write_config_veneer",
            "lcd_hibernate_run_awake_sequence",
            "__displaylcd_setup_veneer", "__displaylcd_dma_veneer",
            "lcd_hibernate_wait_dma",
        ],
        "lcd_hibernate_finish_resume",
    )
    scheduler_calls = ("sleep", "yield", "mutex_lock", "mutex_unlock", "send_event")
    for owner, body in (
        ("lcd_hibernate_finish_resume", finish_lcd),
        ("lcd_hibernate_run_seq8", run_seq8),
        ("lcd_hibernate_run_seq16", run_seq16),
        ("lcd_hibernate_run_awake_sequence", run_awake),
        ("lcd_hibernate_wait_dma", poll_dma),
    ):
        for forbidden in scheduler_calls:
            if f"<{forbidden}>" in body:
                raise GateError(f"{owner} calls scheduler primitive {forbidden}")

    require_order(
        poll_dma,
        ["__dmac_callback_veneer", "dmac_ch_running"],
        "lcd_hibernate_wait_dma",
    )
    if "<send_event>" not in finish_lcd_event:
        raise GateError("deferred LCD activation event is missing")

    if "<system_hibernate_resume_usec_timer>" not in system_resume:
        raise GateError("hardware resume omits the RetailOS Timer E repair")
    if re.search(r"\bblx?\b", usec_timer):
        raise GateError("Timer E repair unexpectedly calls another function")
    for literal in ("#1088", "#11", "#3"):
        if literal not in usec_timer:
            raise GateError(f"Timer E repair is missing stock literal {literal}")
    if "mvn" not in usec_timer or len(re.findall(r"\bstr\b", usec_timer)) < 4:
        raise GateError("Timer E repair is not the four-write RetailOS sequence")

    for literal in ("0x80000000", "80100db1", "16777216", "#51", "00000804"):
        if literal not in prepare_lcd:
            raise GateError(
                f"LCD controller repair is missing stock literal {literal}"
            )
    if len(re.findall(r"\bstr\b", prepare_lcd)) < 5:
        raise GateError("LCD controller repair omits a RetailOS register write")

    if "<stage3_mask_vic>" not in enter:
        raise GateError("application continuation does not close VIC before return")
    if "<stage3_restore_all_vic>" in enter:
        raise GateError("application continuation restores full VIC before I2C unlock")
    if len(re.findall(r"\bstr\b", restore_all_vic)) < 7:
        raise GateError("full VIC restore is missing pending-timer or mask writes")

    for hook in (
        "system_hibernate_resume", "eint_hibernate_resume",
        "button_hibernate_resume", "uart_hibernate_resume",
        "pmu_hibernate_resume", "power_hibernate_resume",
        "pcm_hibernate_resume", "lcd_hibernate_resume",
    ):
        if f"<{hook}>" not in enter:
            raise GateError(f"application continuation omits {hook}")

    if "<ipod6g_hibernate_stage3_resume>" not in validator:
        raise GateError("bootloader validator does not dispatch direct resume")
    if re.search(r"\bblx?\b", trampoline):
        raise GateError("bootloader resume trampoline unexpectedly calls another function")
    if not re.search(r"\bbx\s+r2\b", trampoline):
        raise GateError("bootloader resume trampoline does not end in direct PC handoff")
    if re.search(r"\bbx\s+lr\b|\bpop\b.*\bpc\b", trampoline):
        raise GateError("bootloader resume trampoline contains a return path")

    app_strings = command("strings", str(args.app_elf))
    if "Stage 3B-R10 / ABI 11" not in app_strings:
        raise GateError("application does not identify Stage 3B-R10 / ABI 11")
    if "/.rbtv" not in app_strings:
        raise GateError("application is not isolated to /.rbtv")

    app_version = version(args.app_info)
    boot_version = version(args.boot_info)
    if app_version != boot_version:
        raise GateError(
            f"build fingerprints differ: app={app_version}, boot={boot_version}"
        )

    print("PASS: iPod 6G Stage 3B-R10 linked-image gate")
    print(f"  version: {app_version}")
    print(f"  app checkpoint: 0x{checkpoint:08x}")
    print(f"  boot direct resume: 0x{resume:08x}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except GateError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
