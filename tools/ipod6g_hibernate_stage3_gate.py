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


def address_table(elf: Path) -> dict[str, int]:
    symbols: dict[str, int] = {}
    for line in command(NM, "-n", str(elf)).splitlines():
        match = re.match(r"^([0-9a-fA-F]+)\s+[A-Za-z?]\s+(\S+)$", line)
        if match:
            symbols[match.group(2)] = int(match.group(1), 16)
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


def require_address(symbols: dict[str, int], name: str) -> int:
    if name not in symbols:
        raise GateError(f"missing linked address symbol: {name}")
    return symbols[name]


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


def callers(disassembly: str, name: str) -> set[str]:
    pattern = re.compile(
        r"^[0-9a-fA-F]+ <([^>]+)>:\n(.*?)"
        r"(?=^[0-9a-fA-F]+ <|\Z)",
        re.MULTILINE | re.DOTALL,
    )
    marker = f"<{name}>"
    return {
        match.group(1)
        for match in pattern.finditer(disassembly)
        if marker in match.group(2)
    }


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


def resume_contract(elf: Path) -> str:
    matches = set(re.findall(
        r"ipod6g-hibernate-abi[0-9]+-record[0-9]+",
        command("strings", str(elf)),
    ))
    if len(matches) != 1:
        rendered = ", ".join(sorted(matches)) or "none"
        raise GateError(f"{elf} has ambiguous resume contract IDs: {rendered}")
    return matches.pop()


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
    app_addresses = address_table(args.app_elf)
    boot_addresses = address_table(args.boot_elf)

    checkpoint = require_symbol(
        app_symbols, "ipod6g_hibernate_stage3_checkpoint", minimum_size=0x60
    )
    require_symbol(
        app_symbols, "ipod6g_hibernate_stage3_enter", minimum_size=0x100
    )
    require_symbol(
        app_symbols, "ipod6g_hibernate_stage3_suspend", minimum_size=0x100
    )
    require_symbol(
        app_symbols, "ipod6g_hibernate_poweroff_try", minimum_size=0x20
    )
    require_symbol(
        app_symbols, "sys_poweroff_handle_request", minimum_size=0x20
    )
    require_symbol(
        app_symbols, "ipod6g_hibernate_tick_probe_task", minimum_size=0x20
    )
    require_symbol(
        app_symbols, "ipod6g_hibernate_switch_probe", minimum_size=0x20
    )
    require_symbol(app_symbols, "sys_poweroff", minimum_size=0x20)
    require_symbol(app_symbols, "button_get_w_tmo", minimum_size=0x20)
    require_symbol(app_symbols, "i2c_bus_lock", minimum_size=0x10)
    require_symbol(app_symbols, "i2c_bus_unlock", minimum_size=0x10)
    for helper in (
        "stage3_mask_vic", "stage3_rearm_tick", "stage3_verify_tick",
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

    boot_bss_start = require_address(boot_addresses, "_edata")
    boot_bss_end = require_address(boot_addresses, "_end")
    app_hibernate_start = require_address(app_addresses, "hibernatebuffer")
    app_hibernate_end = require_address(app_addresses, "hibernatebufferend")
    if boot_bss_start != 0x0BF3C000:
        raise GateError(
            "resume bootloader BSS is not in its private workspace: "
            f"0x{boot_bss_start:08x}"
        )
    if not boot_bss_start < boot_bss_end <= 0x0BFEC000:
        raise GateError(
            "resume bootloader BSS escapes its private workspace: "
            f"0x{boot_bss_start:08x}-0x{boot_bss_end:08x}"
        )
    if (app_hibernate_start, app_hibernate_end) != (
        0x0BF3C000, 0x0BFFC000,
    ):
        raise GateError(
            "application does not reserve the ABI-12 hibernate workspace: "
            f"0x{app_hibernate_start:08x}-0x{app_hibernate_end:08x}"
        )

    app_dis = command(OBJDUMP, "-d", str(args.app_elf))
    boot_dis = command(OBJDUMP, "-d", str(args.boot_elf))
    suspend = function(app_dis, "ipod6g_hibernate_stage3_suspend")
    runtime_poweroff = function(app_dis, "ipod6g_hibernate_poweroff_try")
    system_poweroff = function(app_dis, "sys_poweroff")
    request_handler = function(app_dis, "sys_poweroff_handle_request")
    button_get = function(app_dis, "button_get_w_tmo")
    enter = function(app_dis, "ipod6g_hibernate_stage3_enter")
    rearm_tick = function(app_dis, "stage3_rearm_tick")
    verify_tick = function(app_dis, "stage3_verify_tick")
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
            "stage3_verify_tick",
            "pmu_hibernate_resume_complete",
            "lcd_hibernate_resume_complete",
        ],
        "ipod6g_hibernate_stage3_suspend",
    )
    if "<ipod6g_hibernate_stage3_suspend>" not in runtime_poweroff:
        raise GateError("runtime power-off hook omits retained suspend")
    if "<ipod6g_hibernate_poweroff_should_defer>" not in system_poweroff:
        raise GateError("sys_poweroff does not test the retained mode")
    if "<button_queue_post>" not in system_poweroff:
        raise GateError("sys_poweroff does not defer through the button queue")
    if "<ipod6g_hibernate_poweroff_try>" in system_poweroff:
        raise GateError("sys_poweroff still suspends from its caller context")
    if "<queue_broadcast>" not in system_poweroff:
        raise GateError("sys_poweroff lost its unchanged legacy fallback")
    if "<sys_poweroff_handle_request>" not in button_get:
        raise GateError("button consumer does not handle retained power-off request")
    handler_start = request_handler.find("<ipod6g_hibernate_poweroff_try>")
    fallback_markers = (
        request_handler.find("<sys_poweroff_broadcast>"),
        request_handler.find("<queue_broadcast>"),
    )
    fallback_start = min(
        (position for position in fallback_markers if position >= 0),
        default=-1,
    )
    if handler_start < 0 or fallback_start < 0:
        raise GateError("retained request handler omits suspend or legacy fallback")
    if handler_start >= fallback_start:
        raise GateError("retained request handler broadcasts before suspend attempt")
    for required in ("<button_clear_pressed>", "<reset_poweroff_timer>"):
        if required not in request_handler:
            raise GateError(f"successful retained wake omits {required[1:-1]}")
    handler_callers = callers(app_dis, "sys_poweroff_handle_request")
    if handler_callers != {"button_get_w_tmo"}:
        rendered = ", ".join(sorted(handler_callers)) or "none"
        raise GateError(f"retained request runs outside button consumer: {rendered}")
    suspend_callers = callers(app_dis, "ipod6g_hibernate_stage3_suspend")
    expected_callers = {
        "dbg_hibernate_stage3", "ipod6g_hibernate_poweroff_try",
    }
    if suspend_callers != expected_callers:
        rendered = ", ".join(sorted(suspend_callers)) or "none"
        raise GateError(f"unexpected retained-suspend callers: {rendered}")
    lock_end = suspend.find("<i2c_bus_lock>")
    enter_start = suspend.find("<ipod6g_hibernate_stage3_enter>")
    serialized_boundary = suspend[lock_end:enter_start]
    if "mrs" not in serialized_boundary or "msr" not in serialized_boundary:
        raise GateError("IRQ/FIQ are not masked after I2C ownership and before checkpoint")

    display_start = suspend.find("<stage3_finish_display>")
    tick_start = suspend.find("<stage3_rearm_tick>")
    restore_start = suspend.find("<stage3_restore_all_vic>")
    verify_start = suspend.find("<stage3_verify_tick>")
    pmu_start = suspend.find("<pmu_hibernate_resume_complete>")
    irq_release = suspend[restore_start:pmu_start]
    if not display_start < tick_start < restore_start < verify_start < pmu_start:
        raise GateError("scheduler hardware is exposed before display repair")
    if "msr" not in irq_release:
        raise GateError("CPU IRQ/FIQ are not released after full VIC restore")

    for forbidden in ("<sleep>", "<yield>", "<mutex_lock>"):
        if forbidden in suspend[enter_start:restore_start]:
            raise GateError(
                f"pre-interrupt resume path contains scheduler call {forbidden}"
            )

    if re.search(r"\bblx?\b", rearm_tick):
        raise GateError("retained Timer B repair unexpectedly calls another function")
    for literal in ("#100", "#74", "#4672", "#2", "#1"):
        if literal not in rearm_tick:
            raise GateError(
                f"retained Timer B repair is missing measured literal {literal}"
            )
    if len(re.findall(r"\bstr\b", rearm_tick)) < 10:
        raise GateError("retained Timer B repair omits ordered register writes")
    for forbidden in ("<sleep>", "<yield>", "<mutex_lock>"):
        if forbidden in verify_tick:
            raise GateError(f"Timer B proof contains scheduler call {forbidden}")

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
    if "Stage 3B-R11 / ABI 12 / BSS FIX" not in app_strings:
        raise GateError("application does not identify Stage 3B-R11 / ABI 12")
    if "/.rbtv" not in app_strings:
        raise GateError("application is not isolated to /.rbtv")

    app_version = version(args.app_info)
    boot_version = version(args.boot_info)
    app_contract = resume_contract(args.app_elf)
    boot_contract = resume_contract(args.boot_elf)
    if app_contract != boot_contract:
        raise GateError(
            "resume contracts differ: "
            f"app={app_contract}, boot={boot_contract}"
        )
    if app_contract != "ipod6g-hibernate-abi12-record12":
        raise GateError(f"unexpected Stage 3B-R11 contract: {app_contract}")

    print("PASS: iPod 6G Stage 3B-R11 linked-image gate")
    print(f"  app version: {app_version}")
    print(f"  boot version: {boot_version}")
    print(f"  resume contract: {app_contract}")
    print(f"  app checkpoint: 0x{checkpoint:08x}")
    print(f"  boot direct resume: 0x{resume:08x}")
    print(
        "  boot private BSS: "
        f"0x{boot_bss_start:08x}-0x{boot_bss_end:08x}"
    )
    print(
        "  app hibernate reserve: "
        f"0x{app_hibernate_start:08x}-0x{app_hibernate_end:08x}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except GateError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
