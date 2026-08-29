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
        "stage3_enable_tick_vic", "stage3_enable_dma_vic",
        "stage3_finish_display", "stage3_lcd_checkpoint",
        "stage3_restore_all_vic",
    ):
        require_symbol(app_symbols, helper, minimum_size=0x0c)
    require_symbol(app_symbols, "lcd_hibernate_finish_resume", minimum_size=0x40)
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
    restore_all_vic = function(app_dis, "stage3_restore_all_vic")
    validator = function(boot_dis, "ipod6g_hibernate_validate_after_wake")
    trampoline = function(boot_dis, "ipod6g_hibernate_stage3_resume")

    require_order(
        suspend,
        [
            "i2c_bus_lock", "ipod6g_hibernate_stage3_enter",
            "i2c_bus_unlock", "stage3_rearm_tick",
            "stage3_enable_tick_vic", "sleep", "stage3_enable_dma_vic",
            "stage3_finish_display", "stage3_restore_all_vic",
            "pmu_hibernate_resume_complete",
        ],
        "ipod6g_hibernate_stage3_suspend",
    )
    lock_end = suspend.find("<i2c_bus_lock>")
    enter_start = suspend.find("<ipod6g_hibernate_stage3_enter>")
    serialized_boundary = suspend[lock_end:enter_start]
    if "mrs" not in serialized_boundary or "msr" not in serialized_boundary:
        raise GateError("IRQ/FIQ are not masked after I2C ownership and before checkpoint")

    tick_start = suspend.find("<stage3_enable_tick_vic>")
    tick_wait = suspend.find("<sleep>")
    tick_live_boundary = suspend[tick_start:tick_wait]
    if "<tick_start>" not in tick_live_boundary:
        raise GateError("Timer B is not reset immediately before CPU IRQ release")
    if "msr" not in tick_live_boundary:
        raise GateError("CPU IRQ/FIQ are not released after tick VIC bootstrap")

    dma_start = suspend.find("<stage3_enable_dma_vic>")
    display_start = suspend.find("<stage3_finish_display>")
    if not tick_wait < dma_start < display_start:
        raise GateError("LCD DMA is exposed before the scheduler sleep proof")

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
            "mutex_lock", "lcd_target_enable_clocks",
            "__s5l_lcd_write_config_veneer", "__lcd_update_veneer",
            "__displaylcd_wait_dma_veneer", "mutex_unlock", "send_event",
        ],
        "lcd_hibernate_finish_resume",
    )
    callback_calls = len(re.findall(r"\bblx\s+r4\b", finish_lcd))
    callback_tail = bool(re.search(r"\bbx\s+r3\b", finish_lcd))
    if callback_calls != 6 or not callback_tail:
        raise GateError("LCD resume does not commit all seven sub-operation checkpoints")

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
    if "Stage 3B-R9 / ABI 10" not in app_strings:
        raise GateError("application does not identify Stage 3B-R9 / ABI 10")
    if "/.rbtv" not in app_strings:
        raise GateError("application is not isolated to /.rbtv")

    app_version = version(args.app_info)
    boot_version = version(args.boot_info)
    if app_version != boot_version:
        raise GateError(
            f"build fingerprints differ: app={app_version}, boot={boot_version}"
        )

    print("PASS: iPod 6G Stage 3B-R9 linked-image gate")
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
