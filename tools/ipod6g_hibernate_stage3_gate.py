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


def data_symbol_accesses(
    body: str, symbol_address: int,
) -> list[tuple[int, str, str]]:
    """Resolve linked ARM literal-base accesses to a data symbol.

    Final linked objdump output does not annotate local BSS operands with their
    symbol names. GCC also folds nearby globals under one literal base, so a
    source-level name search cannot prove which byte an ldrb/strb touches.
    Resolve the PC-relative literal and displacement instead.
    """
    words: dict[int, int] = {}
    lines = list(re.finditer(r"^.*$", body, re.MULTILINE))
    for line_match in lines:
        match = re.match(
            r"\s*([0-9a-fA-F]+):\s+[0-9a-fA-F]+\s+\.word\s+"
            r"0x([0-9a-fA-F]+)",
            line_match.group(0),
        )
        if match:
            words[int(match.group(1), 16)] = int(match.group(2), 16)

    bases: dict[str, tuple[int, int]] = {}
    accesses: list[tuple[int, str, str]] = []
    for line_match in lines:
        line = line_match.group(0)
        literal = re.match(
            r"\s*([0-9a-fA-F]+):\s+[0-9a-fA-F]+\s+ldr"
            r"(?:eq|ne|cs|cc|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?\s+"
            r"(r[0-9]+),\s*\[pc,\s*#([0-9]+)\]",
            line,
        )
        if literal:
            instruction = int(literal.group(1), 16)
            literal_address = instruction + 8 + int(literal.group(3))
            if literal_address in words:
                bases[literal.group(2)] = (
                    words[literal_address], line_match.start()
                )
            continue

        access = re.match(
            r"\s*[0-9a-fA-F]+:\s+[0-9a-fA-F]+\s+"
            r"(ldrb|strb|ldrh|strh|ldr|str)"
            r"(?:eq|ne|cs|cc|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?\s+"
            r"(r[0-9]+),\s*\[(r[0-9]+)(?:,\s*#(-?[0-9]+))?\]",
            line,
        )
        if not access or access.group(3) not in bases:
            continue
        base, base_position = bases[access.group(3)]
        if base_position >= line_match.start():
            continue
        displacement = int(access.group(4) or "0")
        if base + displacement == symbol_address:
            accesses.append(
                (line_match.start(), access.group(1), line.rstrip())
            )
    return accesses


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


def require_call_before_targets_on_all_paths(
    body: str, prerequisite: str, targets: tuple[str, ...], owner: str,
) -> None:
    require_any_call_before_targets_on_all_paths(
        body, (prerequisite,), targets, owner,
    )


def require_any_call_before_targets_on_all_paths(
    body: str, prerequisites: tuple[str, ...], targets: tuple[str, ...],
    owner: str,
) -> None:
    from ipod6g_hibernate_cfg import UnsafePath, require_boundary
    try:
        require_boundary(body, prerequisites, targets, owner)
    except UnsafePath as error:
        raise GateError(str(error)) from error


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
    parser.add_argument("--pictureflow-elf", type=Path, required=True)
    args = parser.parse_args()

    for path in (
        args.app_elf, args.boot_elf, args.app_info, args.boot_info,
        args.pictureflow_elf,
    ):
        if not path.is_file():
            raise GateError(f"missing input: {path}")

    app_symbols = symbol_table(args.app_elf)
    boot_symbols = symbol_table(args.boot_elf)
    pictureflow_symbols = symbol_table(args.pictureflow_elf)
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
        app_symbols, "ipod6g_hibernate_poweroff_last_refusal",
        minimum_size=0x08,
    )
    require_symbol(
        app_symbols, "ipod6g_hibernate_rolo_capability_republished",
        minimum_size=0x08,
    )
    require_symbol(
        app_symbols, "ipod6g_hibernate_log_refusal", minimum_size=0x80
    )
    require_symbol(
        app_symbols, "sys_poweroff_handle_request", minimum_size=0x20
    )
    require_symbol(
        app_symbols, "sys_poweroff_handle_plugin_capable", minimum_size=0x10
    )
    require_symbol(
        app_symbols, "sys_poweroff_handle_plugin_ready", minimum_size=0x20
    )
    require_symbol(app_symbols, "sys_poweroff_plugin_reset", minimum_size=0x10)
    require_symbol(app_symbols, "plugin_is_loaded", minimum_size=0x08)
    require_symbol(app_symbols, "mixer_hibernate_suspend", minimum_size=0x18)
    require_symbol(app_symbols, "mixer_hibernate_resume", minimum_size=0x18)
    require_symbol(
        app_symbols, "notification_manager_hibernate_prepare", minimum_size=0x20
    )
    require_symbol(
        app_symbols, "notification_manager_hibernate_resume", minimum_size=0x18
    )
    require_symbol(
        app_symbols, "notification_manager_hibernate_abort", minimum_size=0x18
    )
    require_symbol(app_symbols, "pcm_hibernate_suspend", minimum_size=0x20)
    require_symbol(app_symbols, "pcm_hibernate_resume", minimum_size=0x20)
    require_symbol(app_symbols, "pcm_hibernate_abort", minimum_size=0x20)
    require_symbol(app_symbols, "ata_sleepnow", minimum_size=0x1c)
    require_symbol(
        app_symbols, "ata_hibernate_suspend_begin", minimum_size=0x30
    )
    require_symbol(app_symbols, "ata_hibernate_resume", minimum_size=0x10)
    require_symbol(
        app_symbols, "ata_hibernate_base_repaired", minimum_size=0x20
    )
    require_symbol(app_symbols, "ata_hibernate_abort", minimum_size=0x20)
    require_symbol(
        app_symbols, "ata_hibernate_event_commit", minimum_size=0x18
    )
    require_symbol(app_symbols, "ata_demand_lock", minimum_size=0x20)
    require_symbol(
        app_symbols, "sys_poweroff_hibernate_event_committed",
        minimum_size=0x08,
    )
    require_symbol(app_symbols, "ata_wait_for_cbr_ready", minimum_size=0x30)
    require_symbol(app_symbols, "button_hibernate_resume_state", minimum_size=0x20)
    require_symbol(
        app_symbols, "button_hibernate_event_filtered", minimum_size=0x30
    )
    require_symbol(
        app_symbols, "button_hibernate_take_action_reset", minimum_size=0x18
    )
    require_symbol(
        app_symbols, "button_hibernate_wheel_sample", minimum_size=0x20
    )
    require_symbol(
        app_symbols, "button_clear_hibernate_wake", minimum_size=0x30
    )
    require_symbol(
        app_symbols, "sys_poweroff_button_is_reserved", minimum_size=0x04
    )
    require_symbol(
        app_symbols, "ipod6g_videoout_hibernate_suspend", minimum_size=0x18
    )
    require_symbol(
        app_symbols, "ipod6g_videoout_hibernate_resume", minimum_size=0x20
    )
    for diagnostic_symbol in (
        "ipod6g_hibernate_runtime_checkpoint",
        "ipod6g_hibernate_tick_probe_start",
        "ipod6g_hibernate_tick_probe_task",
        "ipod6g_hibernate_tick_probe_complete",
        "ipod6g_hibernate_runtime_probe_active",
        "ipod6g_hibernate_switch_probe",
        "pmu_hibernate_runtime_monitor_enable",
    ):
        if diagnostic_symbol in app_symbols:
            raise GateError(
                "production image retains post-wake diagnostic symbol: "
                + diagnostic_symbol
            )
    require_symbol(app_symbols, "sys_poweroff", minimum_size=0x20)
    require_symbol(app_symbols, "button_get_w_tmo", minimum_size=0x20)
    require_symbol(app_symbols, "i2c_bus_lock", minimum_size=0x10)
    require_symbol(app_symbols, "i2c_bus_unlock", minimum_size=0x10)
    require_symbol(
        app_symbols, "stage3_audio_status_supported", minimum_size=0x10
    )
    require_symbol(
        pictureflow_symbols, "pf_hibernate_advertise", minimum_size=0x10
    )
    require_symbol(
        pictureflow_symbols, "pf_hibernate_activate", minimum_size=0x10
    )
    require_symbol(
        pictureflow_symbols, "pf_hibernate_acknowledge", minimum_size=0x30
    )
    require_symbol(app_symbols, "gpio_hibernate_suspend", minimum_size=0x40)
    require_symbol(app_symbols, "gpio_hibernate_resume", minimum_size=0x20)
    require_symbol(app_symbols, "button_hibernate_suspend", minimum_size=0x20)
    require_symbol(app_symbols, "button_hibernate_resume", minimum_size=0x20)
    require_symbol(
        app_symbols, "button_hibernate_resume_complete", minimum_size=0x20
    )
    require_symbol(
        app_symbols, "button_hibernate_wheel_send", minimum_size=0x40
    )
    require_symbol(
        app_symbols, "button_hibernate_wheel_exchange", minimum_size=0x60
    )
    require_symbol(
        app_symbols, "button_hibernate_wheel_observe_response",
        minimum_size=0x28,
    )
    require_symbol(
        app_symbols, "button_hibernate_wheel_init", minimum_size=0x60
    )
    require_symbol(
        app_symbols, "button_hibernate_wheel_restore_command",
        minimum_size=0x40,
    )
    require_symbol(
        app_symbols, "button_hibernate_wheel_buttons_snapshot",
        minimum_size=0x40,
    )
    require_symbol(app_symbols, "eint_hibernate_suspend", minimum_size=0x18)
    require_symbol(app_symbols, "eint_hibernate_resume", minimum_size=0x20)
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
    pictureflow_dis = command(OBJDUMP, "-d", str(args.pictureflow_elf))
    suspend = function(app_dis, "ipod6g_hibernate_stage3_suspend")
    runtime_poweroff = function(app_dis, "ipod6g_hibernate_poweroff_try")
    system_poweroff = function(app_dis, "sys_poweroff")
    request_handler = function(app_dis, "sys_poweroff_handle_request")
    plugin_ready = function(app_dis, "sys_poweroff_handle_plugin_ready")
    poweroff_mode = function(app_dis, "ipod6g_hibernate_set_poweroff_mode")
    refusal_log = function(app_dis, "ipod6g_hibernate_log_refusal")
    audio_guard = function(app_dis, "stage3_audio_status_supported")
    notification_prepare = function(
        app_dis, "notification_manager_hibernate_prepare"
    )
    notification_resume = function(
        app_dis, "notification_manager_hibernate_resume"
    )
    notification_abort = function(
        app_dis, "notification_manager_hibernate_abort"
    )
    notification_service = function(
        app_dis, "notification_manager_service"
    )
    mixer_suspend = function(app_dis, "mixer_hibernate_suspend")
    mixer_resume = function(app_dis, "mixer_hibernate_resume")
    pcm_suspend = function(app_dis, "pcm_hibernate_suspend")
    pcm_resume = function(app_dis, "pcm_hibernate_resume")
    pcm_abort = function(app_dis, "pcm_hibernate_abort")
    ata_sleepnow = function(app_dis, "ata_sleepnow")
    ata_sleepnow_locked = function(app_dis, "ata_sleepnow_locked")
    require_order(
        ata_sleepnow,
        ["ata_demand_lock", "ata_sleepnow_locked", "mutex_unlock"],
        "ATA sleep wrapper ownership",
    )
    ata_demand_lock = function(app_dis, "ata_demand_lock")
    ata_suspend_begin = function(app_dis, "ata_hibernate_suspend_begin")
    ata_hibernate_resume = function(app_dis, "ata_hibernate_resume")
    ata_base_repaired = function(
        app_dis, "ata_hibernate_base_repaired"
    )
    ata_abort = function(app_dis, "ata_hibernate_abort")
    ata_event_commit = function(app_dis, "ata_hibernate_event_commit")
    poweroff_event_commit = function(
        app_dis, "sys_poweroff_hibernate_event_committed"
    )
    ata_wait_cbr = function(app_dis, "ata_wait_for_cbr_ready")
    ata_transfer = function(app_dis, "ata_transfer_sectors")
    ata_init = function(app_dis, "ata_init")
    videoout_suspend = function(
        app_dis, "ipod6g_videoout_hibernate_suspend"
    )
    videoout_resume = function(
        app_dis, "ipod6g_videoout_hibernate_resume"
    )
    button_get = function(app_dis, "button_get_w_tmo")
    button_read = function(app_dis, "button_read")
    button_tick = function(app_dis, "button_tick")
    button_queue_post = function(app_dis, "button_queue_post")
    button_queue_try_post = function(app_dis, "button_queue_try_post")
    button_event_filter = function(
        app_dis, "button_hibernate_event_filtered"
    )
    button_clear_wake = function(app_dis, "button_clear_hibernate_wake")
    button_action_reset = function(
        app_dis, "button_hibernate_take_action_reset"
    )
    button_wheel_sample = function(
        app_dis, "button_hibernate_wheel_sample"
    )
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
    gpio_suspend = function(app_dis, "gpio_hibernate_suspend")
    gpio_resume = function(app_dis, "gpio_hibernate_resume")
    button_suspend = function(app_dis, "button_hibernate_suspend")
    button_resume = function(app_dis, "button_hibernate_resume")
    button_resume_complete = function(
        app_dis, "button_hibernate_resume_complete"
    )
    wheel_send = function(app_dis, "button_hibernate_wheel_send")
    wheel_exchange = function(app_dis, "button_hibernate_wheel_exchange")
    wheel_observe_response = function(
        app_dis, "button_hibernate_wheel_observe_response"
    )
    wheel_init = function(app_dis, "button_hibernate_wheel_init")
    wheel_restore = function(
        app_dis, "button_hibernate_wheel_restore_command"
    )
    wheel_buttons_snapshot = function(
        app_dis, "button_hibernate_wheel_buttons_snapshot"
    )
    eint_suspend = function(app_dis, "eint_hibernate_suspend")
    eint_clear = function(app_dis, "eint_hibernate_clear_pending")
    eint_resume = function(app_dis, "eint_hibernate_resume")
    save_apple_irq = function(app_dis, "stage3_save_apple_irq")
    restore_apple_irq = function(app_dis, "stage3_restore_apple_irq")
    restore_all_vic = function(app_dis, "stage3_restore_all_vic")
    validator = function(boot_dis, "ipod6g_hibernate_validate_after_wake")
    trampoline = function(boot_dis, "ipod6g_hibernate_stage3_resume")
    pictureflow_start = function(pictureflow_dis, "plugin_start")
    pictureflow_advertise = function(
        pictureflow_dis, "pf_hibernate_advertise"
    )
    pictureflow_activate = function(
        pictureflow_dis, "pf_hibernate_activate"
    )
    pictureflow_ack = function(
        pictureflow_dis, "pf_hibernate_acknowledge"
    )

    require_order(
        suspend,
        [
            "ata_hibernate_suspend_begin",
            "ipod6g_videoout_hibernate_suspend", "pcm_hibernate_suspend",
            "uart_hibernate_suspend", "__lcd_wait_for_dma_veneer",
            "backlight_hw_kill", "lcd_sleep",
            "ipod6g_hibernate_stage1_arm",
            "i2c_bus_lock",
            "gpio_hibernate_suspend", "button_hibernate_suspend",
            "stage3_save_apple_irq", "ipod6g_hibernate_stage3_enter",
            "i2c_bus_unlock", "stage3_finish_display",
            "stage3_rearm_tick", "stage3_restore_all_vic",
        ],
        "ipod6g_hibernate_stage3_suspend",
    )
    if "#223" not in suspend or "<pmu_set_wake_condition>" not in suspend:
        raise GateError("retained suspend does not program RetailOS OOCWAKE 0xdf")
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
    for handler in (
        "sys_poweroff_handle_plugin_capable",
        "sys_poweroff_handle_plugin_ready",
    ):
        if f"<{handler}>" not in button_get:
            raise GateError(f"button consumer does not dispatch {handler}")
    if "<ipod6g_hibernate_poweroff_try>" not in request_handler or not any(
        marker in request_handler
        for marker in ("<sys_poweroff_broadcast>", "<queue_broadcast>")
    ):
        raise GateError("retained request handler omits suspend or legacy fallback")
    if "<ipod6g_hibernate_log_refusal>" not in request_handler:
        raise GateError("P16 legacy fallback omits its persistent refusal trace")
    require_order(
        request_handler,
        [
            "notification_manager_hibernate_abort",
            "ipod6g_hibernate_log_refusal",
            ("sys_poweroff_broadcast" if "<sys_poweroff_broadcast>" in
             request_handler else "queue_broadcast"),
        ],
        "retained refusal unwind",
    )
    if request_handler.count("<mixer_hibernate_resume>") < 2:
        raise GateError(
            "retained refusal does not restore the carried PCM service"
        )
    require_order(
        poweroff_mode,
        [
            "ipod6g_hibernate_record_valid", "record_clear",
            "record_commit_crc", "commit_dcache",
        ],
        "P16 Rolo capability bridge",
    )
    require_order(
        refusal_log,
        [
            "ipod6g_hibernate_stage1_get_status",
            "ipod6g_hibernate_poweroff_last_refusal",
            "open", "write", "fsync", "close", "ata_flush", "splashf",
        ],
        "P16 persistent refusal trace",
    )
    # The refusal block has its own PCM resume before the success block in
    # linked address order. Prove execution order, including both branches.
    for prerequisite, target in (
        ("plugin_is_loaded", "notification_manager_hibernate_prepare"),
        ("notification_manager_hibernate_prepare", "mixer_hibernate_suspend"),
        ("audio_status", "mixer_hibernate_suspend"),
        ("mixer_hibernate_suspend", "ipod6g_hibernate_poweroff_try"),
        ("ipod6g_hibernate_poweroff_try", "button_clear_hibernate_wake"),
        ("mixer_hibernate_resume", "button_clear_hibernate_wake"),
        ("button_clear_hibernate_wake", "reset_poweroff_timer"),
        ("reset_poweroff_timer", "notification_manager_hibernate_resume"),
    ):
        require_call_before_targets_on_all_paths(
            request_handler, prerequisite, (target,),
            "sys_poweroff_handle_request",
        )
    require_order(
        notification_prepare,
        [
            "notification_manager_init", "notification_manager_flush",
            "beep_play", "notification_update_overlay",
        ],
        "notification hibernate prepare barrier",
    )
    for owner, body in (
        ("prepare", notification_prepare),
        ("resume", notification_resume),
    ):
        for forbidden in (
            "notification_post", "audio_status", "audio_current_track",
            "audio_pause", "audio_resume",
        ):
            if f"<{forbidden}>" in body:
                raise GateError(
                    f"notification {owner} touches playback through {forbidden}"
                )
    for forbidden in (
        "notification_manager_flush", "beep_play",
    ):
        if f"<{forbidden}>" in notification_resume:
            raise GateError(
                "notification resume synthesizes work through " + forbidden
            )

    # RetailOS sends one synchronous DiskMgr state-2 transaction and does not
    # publish UI event 17 until the retained coordinator has returned. Carry
    # one ATA mutex ownership level over every Rockbox scheduler point and the
    # controller reset so no database/preview client can be frozen mid-I/O.
    # Releasing physical ownership must not admit demand: Apple leaves state
    # 2 in place through UI event 17 and performs state 1 synchronously only
    # after a later client request. Rockbox therefore separates ATA ownership
    # from a sequence-tagged demand-admission state.
    pending_address = require_address(
        app_addresses, "hibernate_storage_reinit_pending"
    )
    io_pending_address = require_address(
        app_addresses, "hibernate_storage_io_pending"
    )
    transaction_address = require_address(
        app_addresses, "hibernate_storage_transaction_owned"
    )
    demand_state_address = require_address(
        app_addresses, "hibernate_storage_demand_state"
    )
    storage_sequence_address = require_address(
        app_addresses, "hibernate_storage_sequence"
    )
    ata_initialized_address = require_address(
        app_addresses, "ata_driver_initialized"
    )
    require_order(
        ata_suspend_begin,
        ["ata_demand_lock", "ata_sleepnow_locked"],
        "serialized ATA hibernate transaction",
    )
    if not any(
        opcode == "strb" for _position, opcode, _line
        in data_symbol_accesses(ata_suspend_begin, transaction_address)
    ):
        raise GateError("ATA suspend transaction does not record ownership")
    if "<mutex_lock>" in ata_sleepnow_locked or "<mutex_unlock>" in ata_sleepnow_locked:
        raise GateError("ATA locked sleep helper changes outer mutex ownership")
    if ata_sleepnow_locked.count("<ata_flush_cache>") != 1:
        raise GateError("ATA hibernate transaction has an ambiguous flush")
    if "<ata_power_up>" in ata_sleepnow_locked:
        raise GateError("ATA sleep eagerly wakes Apple's inactive disk state")
    if "<mutex_unlock>" not in ata_base_repaired:
        raise GateError("ATA base repair cannot release physical ownership")
    for owner, body in (
        ("ATA base repair", ata_base_repaired),
        ("ATA abort", ata_abort),
        ("ATA event commit", ata_event_commit),
    ):
        if "<ata_power_up>" in body:
            raise GateError(owner + " eagerly wakes the retained disk")
    if "<mutex_unlock>" in ata_event_commit:
        raise GateError("event commit still owns the physical ATA mutex")
    if "<mutex_unlock>" not in ata_abort:
        raise GateError("ATA abort cannot release an owned transaction")

    for owner, body, required_addresses in (
        (
            "ATA base repair", ata_base_repaired,
            (transaction_address, demand_state_address,
             storage_sequence_address),
        ),
        (
            "ATA abort", ata_abort,
            (transaction_address, demand_state_address,
             storage_sequence_address),
        ),
        (
            "ATA event commit", ata_event_commit,
            (demand_state_address, storage_sequence_address),
        ),
    ):
        for address in required_addresses:
            if not data_symbol_accesses(body, address):
                raise GateError(owner + " omits sequence/admission state")

    if not all(marker in ata_demand_lock for marker in (
        "<sleep>", "<mutex_lock>", "<mutex_unlock>",
    )):
        raise GateError("ATA demand gate omits wait, lock, or failed recheck")
    demand_accesses = data_symbol_accesses(
        ata_demand_lock, demand_state_address
    )
    if len(demand_accesses) < 2:
        raise GateError("ATA demand gate does not recheck admission after lock")
    required_demand_callers = {
        "ata_soft_reset", "ata_hard_reset", "ata_read_sectors",
        "ata_write_sectors", "ata_flush", "ata_sleepnow",
        "ata_hibernate_suspend_begin", "ata_spin", "ata_init",
        "ata_read_smart", "ata_event",
    }
    missing_demand_callers = required_demand_callers - callers(
        app_dis, "ata_demand_lock"
    )
    if missing_demand_callers:
        raise GateError(
            "wake-capable ATA entry points bypass demand admission: " +
            ", ".join(sorted(missing_demand_callers))
        )
    require_order(
        ata_init,
        ["mutex_init", "ata_demand_lock", "ata_power_up"],
        "runtime-safe ATA initialization",
    )
    initialized_accesses = data_symbol_accesses(
        ata_init, ata_initialized_address
    )
    if not ({opcode for _position, opcode, _line in initialized_accesses} >=
            {"ldrb", "strb"}):
        raise GateError(
            "runtime ATA initialization can recreate the mutex/admission gate"
        )

    # Every raw PATA command-block-register handshake is routed through a
    # bounded Timer-E wait. The first lazy storage demand must propagate a
    # failed full controller initialization instead of falling through into
    # another raw PIO transaction or marking the adapter active.
    if "<yield>" not in ata_wait_cbr:
        raise GateError("ATA PIO-ready wait has no bounded polling loop")
    for literal in ("#999424", "#576"):
        if literal not in ata_wait_cbr:
            raise GateError(
                "ATA PIO-ready wait is missing its one-second timeout " + literal
            )
    power_call = ata_transfer.find("<ata_power_up>")
    active_call = ata_transfer.find("<ata_set_active>", power_call)
    if power_call < 0 or active_call < 0:
        raise GateError("first storage demand omits lazy ATA power-up")
    power_result = ata_transfer[power_call:active_call]
    if (not re.search(r"\bcmp\s+r0,\s*#0\b", power_result) or
            not re.search(r"\bblt\b", power_result)):
        raise GateError("first storage demand ignores ATA power-up failure")

    for required, address in (
        ("hibernate_storage_reinit_pending", pending_address),
        ("hibernate_storage_io_pending", io_pending_address),
    ):
        if not any(
            opcode == "strb"
            for _position, opcode, _line
            in data_symbol_accesses(ata_hibernate_resume, address)
        ):
            raise GateError(
                "ata_hibernate_resume does not preserve " + required
            )
    for body_name, body in (
        ("ata_hibernate_resume", ata_hibernate_resume),
        ("ata_hibernate_base_repaired", ata_base_repaired),
        ("ata_hibernate_abort", ata_abort),
        ("ata_hibernate_event_commit", ata_event_commit),
        ("ipod6g_hibernate_stage3_suspend", suspend),
    ):
        if "<ata_power_up>" in body:
            raise GateError(body_name + " violates Apple's lazy DiskMgr wake")
    if re.search(r"\bblx?\b", notification_resume):
        raise GateError(
            "notification resume calls outside the retained baseline reset"
        )
    if (len(re.findall(r"\bstrb", notification_prepare)) < 3 or
            len(re.findall(r"\bstrb", notification_resume)) < 3):
        raise GateError(
            "notification barrier does not reset and retain its post-wake flags"
        )
    if ("<notification_manager_service.part.0>" not in notification_service or
            len(re.findall(r"\bldrb", notification_service)) < 2 or
            len(re.findall(r"\bstrb", notification_service)) < 2):
        raise GateError(
            "notification service does not consume the first post-wake pass "
            "before reopening"
        )
    mixer_position = request_handler.find("<mixer_hibernate_suspend>")
    second_audio = request_handler.find("<audio_status>", mixer_position)
    if second_audio < 0:
        raise GateError("output-service preflight does not recheck after suspend")
    for forbidden in (
        "talk_force_shutup", "audio_pause", "audio_resume",
        "pcmbuf_fading", "pcmbuf_pause",
    ):
        if f"<{forbidden}>" in request_handler:
            raise GateError(
                "sys_poweroff_handle_request mutates transport through "
                f"{forbidden}; RetailOS long-hold only suspends the output service"
            )
    if "<sys_poweroff_handle_request>" not in plugin_ready:
        raise GateError("plugin READY acknowledgement does not enter retained path")
    if "<pf_hibernate_activate>" not in pictureflow_start:
        raise GateError("PictureFlow does not activate hibernate capability")
    activate_advertise = pictureflow_activate.find(
        "<pf_hibernate_advertise>"
    )
    activate_clear = re.search(r"\bblx\b", pictureflow_activate)
    if (activate_clear is None or activate_advertise < 0 or
            activate_clear.start() >= activate_advertise):
        raise GateError(
            "PictureFlow capability is not advertised after its queue clear"
        )
    end_worker = pictureflow_ack.find("<end_pf_thread")
    restart_worker = pictureflow_ack.find("<create_pf_thread>")
    if end_worker < 0 or restart_worker < 0 or end_worker >= restart_worker:
        raise GateError(
            "PictureFlow hibernate acknowledgement does not stop then "
            "restart its worker"
        )
    if not re.search(r"\bb(?:l)?x\s+r3\b", pictureflow_advertise):
        raise GateError("PictureFlow capability advertisement does not post")
    if len(re.findall(r"\bblx\b", pictureflow_ack)) < 2:
        raise GateError("PictureFlow acknowledgement omits READY post or wait")
    if "<pcm_is_playing>" in request_handler:
        raise GateError(
            "retained request mistakes the mixer's silence tail for a "
            "media-service acknowledgement"
        )
    if suspend.count("<audio_status>") != 1:
        raise GateError(
            "target audio preflight does not use one coherent status snapshot"
        )
    require_order(
        suspend,
        ["audio_status", "stage3_audio_status_supported"],
        "target Apple transport-state preflight",
    )
    if re.search(r"\bblx?\b", audio_guard):
        raise GateError("target audio guard unexpectedly calls another service")
    for literal in ("#3", "#2", "#1"):
        if literal not in audio_guard:
            raise GateError(
                "target audio guard does not distinguish allowed, pause, and "
                f"play bits ({literal})"
            )
    for required in (
        "<button_clear_hibernate_wake>", "<reset_poweroff_timer>",
    ):
        if required not in request_handler:
            raise GateError(f"successful retained wake omits {required[1:-1]}")
    handler_callers = callers(app_dis, "sys_poweroff_handle_request")
    if handler_callers != {
        "button_get_w_tmo", "sys_poweroff_handle_plugin_ready",
    }:
        rendered = ", ".join(sorted(handler_callers)) or "none"
        raise GateError(f"retained request runs outside button consumer: {rendered}")
    ready_callers = callers(app_dis, "sys_poweroff_handle_plugin_ready")
    if ready_callers != {"button_get_w_tmo"}:
        rendered = ", ".join(sorted(ready_callers)) or "none"
        raise GateError(
            "plugin acknowledgement runs outside button consumer: " + rendered
        )

    # RetailOS consumes long-Play repeats inside the power-state controller,
    # then consumes the recovered wake gesture through physical release. No
    # repeat/release may reach a UI keymap, and the retained action mapper must
    # start a fresh prerequisite epoch before exposing event 17.
    for owner, body in (
        ("button_queue_post", button_queue_post),
        ("button_queue_try_post", button_queue_try_post),
    ):
        if "<button_hibernate_event_filtered>" not in body:
            raise GateError(owner + " bypasses the retained input filter")
    for literal in ("04000040", "02000040"):
        if literal not in button_event_filter:
            raise GateError(
                "input filter omits terminal Play gesture " + literal
            )
    if "<sys_poweroff_button_is_reserved>" not in button_event_filter:
        raise GateError("input filter does not reserve exact long-Play repeat")
    if "<ipod6g_hibernate_poweroff_should_defer>" not in function(
        app_dis, "sys_poweroff_button_is_reserved"
    ):
        raise GateError("button reservation is not tied to retained policy")
    wake_filter_address = require_address(
        app_addresses, "hibernate_wake_filter_active"
    )
    committed_address = require_address(
        app_addresses, "hibernate_power_hold_committed"
    )
    if not any(
        opcode == "strb" for _position, opcode, _line
        in data_symbol_accesses(button_tick, committed_address)
    ):
        raise GateError("long-Play threshold does not latch a terminal gesture")
    committed_filter_accesses = data_symbol_accesses(
        button_event_filter, committed_address
    )
    if not ({opcode for _position, opcode, _line in committed_filter_accesses} >=
            {"ldrb", "strb"}):
        raise GateError("long-Play release is not consumed exactly once")
    wake_read_accesses = data_symbol_accesses(button_read, wake_filter_address)
    if not any(opcode == "ldrb" for _position, opcode, _line in wake_read_accesses):
        raise GateError("button reader does not hold the retained wake filter")
    if any(opcode == "strb" for _position, opcode, _line in wake_read_accesses):
        raise GateError("cached button polling can incorrectly open the wake filter")
    wake_sample_accesses = data_symbol_accesses(
        button_wheel_sample, wake_filter_address
    )
    if not ({opcode for _position, opcode, _line in wake_sample_accesses} >=
            {"ldrb", "strb"}):
        raise GateError("hardware wheel sample cannot open a neutral input epoch")
    sample_callers = callers(app_dis, "button_hibernate_wheel_sample")
    if sample_callers != {"INT_WHEEL", "button_hibernate_resume"}:
        rendered = ", ".join(sorted(sample_callers)) or "none"
        raise GateError("wheel-neutral proof has unexpected callers: " + rendered)
    if "<button_hibernate_wheel_exchange>" not in wheel_buttons_snapshot:
        raise GateError("post-mode3 neutrality proof omits the hardware query")
    if "8000023a" not in wheel_buttons_snapshot:
        raise GateError("post-mode3 neutrality proof omits Apple's five-button ID")
    require_order(
        button_resume,
        [
            "button_hibernate_wheel_restore_command",
            "button_hibernate_resume_state",
            "button_hibernate_wheel_buttons_snapshot",
            "button_hibernate_wheel_sample",
        ],
        "post-mode3 wake-input boundary",
    )
    if "<button_read_device>" not in button_read:
        raise GateError("button reader no longer samples the target cache")
    complete_callers = callers(app_dis, "button_hibernate_resume_complete")
    expected_complete_callers = {
        "ipod6g_hibernate_stage3_enter",
        "ipod6g_hibernate_stage3_suspend",
    }
    if complete_callers != expected_complete_callers:
        rendered = ", ".join(sorted(complete_callers)) or "none"
        raise GateError(
            "post-PMU Hold reconciliation has unexpected callers: " + rendered
        )
    if not any(
        marker in button_resume_complete
        for marker in ("<button_hold>", "<pmu_holdswitch_locked>")
    ):
        raise GateError("Hold reconciliation does not sample refreshed PMU state")
    for hook in ("clockgate_enable", "s5l_clickwheel_init"):
        if f"<{hook}>" not in button_resume_complete:
            raise GateError(f"Hold reconciliation omits {hook}")
    # ARMv5 can construct 0x000e0e00 with two immediate ORs instead of a
    # literal-pool load. Require both values on the same register.
    split_gpio = re.search(
        r"orr\s+(r\d+),\s*\1,\s*#917504.*\n"
        r"[^\n]*orr\s+\1,\s*\1,\s*#3584", button_resume_complete,
    )
    if "000e0e00" not in button_resume_complete and not split_gpio:
        raise GateError("Hold reconciliation omits the locked wheel GPIO state")
    for forbidden in ("<sleep>", "<yield>", "<mutex_lock>"):
        if forbidden in button_resume_complete:
            raise GateError(
                "Hold reconciliation calls a scheduler primitive before IRQ reopen"
            )
    require_order(
        button_clear_wake,
        ["queue_count", "queue_wait_w_tmo", "queue_post"],
        "retained wake queue drain",
    )
    if "mrs" not in button_clear_wake or button_clear_wake.count("msr") < 2:
        raise GateError("wake queue drain is not one IRQ-atomic epoch")
    if "<button_get" in button_clear_wake:
        raise GateError("wake queue drain recursively dispatches button events")
    action_reset_callers = callers(
        app_dis, "button_hibernate_take_action_reset"
    )
    if len(action_reset_callers) != 1:
        raise GateError("retained action reset has ambiguous consumers")
    action_owner = next(iter(action_reset_callers))
    if not action_owner.startswith("get_action_worker"):
        raise GateError("retained action reset runs outside get_action")
    action_worker = function(app_dis, action_owner)
    require_order(
        action_worker,
        ["button_get_w_tmo", "button_hibernate_take_action_reset"],
        "post-retained action epoch reset",
    )
    if "a0000009" not in request_handler:
        raise GateError("retained return does not publish UI event 17 analogue")
    if "<button_clear_pressed>" in request_handler:
        raise GateError("retained return still preserves the stale wake release")

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

    if suspend.count("<ata_hibernate_abort>") < 4:
        raise GateError("ATA admission lacks complete refusal/resume cleanup")
    if "<ata_hibernate_base_repaired>" not in suspend:
        raise GateError("successful resume never enters WAIT_EVENT")
    require_call_before_targets_on_all_paths(
        suspend,
        "stage3_verify_tick",
        ("ata_hibernate_base_repaired",),
        "retained storage base-repair boundary",
    )
    require_any_call_before_targets_on_all_paths(
        suspend,
        ("ata_hibernate_base_repaired", "ata_hibernate_abort"),
        (
            "pmu_hibernate_resume_complete",
            "lcd_hibernate_resume_complete",
            "lcd_awake",
            "backlight_hw_on",
            "ipod6g_videoout_hibernate_resume",
        ),
        "retained storage/callback boundary",
    )

    event_commit_callers = callers(app_dis, "ata_hibernate_event_commit")
    expected_event_commit_callers = {
        "dbg_hibernate_stage3",
        "sys_poweroff_hibernate_event_committed",
    }
    if event_commit_callers != expected_event_commit_callers:
        rendered = ", ".join(sorted(event_commit_callers)) or "none"
        raise GateError("unexpected ATA event-commit callers: " + rendered)
    if callers(app_dis, "sys_poweroff_hibernate_event_committed") != {
        "button_get_w_tmo"
    }:
        raise GateError("event-17 commit runs outside the button consumer")
    if "<ata_hibernate_event_commit>" not in poweroff_event_commit:
        raise GateError("poweroff event commit does not open ATA admission")
    # Both handler returns join one stack-store block; the plugin branch
    # commonly jumps backwards to it. Check the actual return edge, not the
    # address of the handler call relative to the commit block.
    lines = button_get.splitlines()
    for handler in (
        "sys_poweroff_handle_request", "sys_poweroff_handle_plugin_ready",
    ):
        call_index = next(i for i, line in enumerate(lines)
                          if f"<{handler}>" in line)
        next_line = lines[call_index + 1]
        jump = re.search(r"\bb\s+([0-9a-f]+)\b", next_line)
        if jump:
            next_line = next(line for line in lines
                             if line.strip().startswith(jump[1] + ":"))
        if not re.search(r"\bstr\s+r0, \[sp(?:, #0)?\]", next_line):
            raise GateError("event-17 result is not stored after " + handler)
        store_index = lines.index(next_line)
        commit_block = "\n".join(lines[store_index:store_index + 6])
        if ("#-1610612727" not in commit_block or
                "<sys_poweroff_hibernate_event_committed>" not in commit_block):
            raise GateError("event-17 commit is not guarded by the stored result")
    for forbidden_owner, body in (
        ("target suspend", suspend),
        ("request handler", request_handler),
    ):
        if "<ata_hibernate_event_commit>" in body:
            raise GateError(forbidden_owner + " opens demand before event 17")

    for forbidden in ("<sleep>", "<yield>", "<mutex_lock>"):
        if forbidden in suspend[enter_start:restore_start]:
            raise GateError(
                f"pre-interrupt resume path contains scheduler call {forbidden}"
            )

    require_order(
        mixer_suspend,
        ["pcm_play_lock", "pcm_play_stop"],
        "mixer_hibernate_suspend",
    )
    if "<pcm_play_unlock>" in mixer_suspend:
        raise GateError(
            "opcode-8 output suspend releases Apple's cross-Standby lock"
        )
    for forbidden in (
        "mixer_channel_stop", "mixer_channel_pause", "mixer_reset",
        "audio_pause", "audio_stop",
    ):
        if f"<{forbidden}>" in mixer_suspend:
            raise GateError(
                f"output-service suspend destroys retained state through {forbidden}"
            )
    require_order(
        mixer_resume,
        ["mixer_start_pcm", "pcm_play_unlock"],
        "mixer_hibernate_resume",
    )
    if "<pcm_play_lock>" in mixer_resume:
        raise GateError(
            "opcode-9 output resume acquires a new lock instead of consuming "
            "the opcode-8 lock"
        )
    if mixer_resume.count("<pcm_play_unlock>") != 1:
        raise GateError(
            "opcode-9 output resume does not perform exactly one paired unlock"
        )
    for forbidden in ("audio_resume", "audio_play", "audio_next"):
        if f"<{forbidden}>" in mixer_resume:
            raise GateError(
                f"output-service resume mutates transport through {forbidden}"
            )
    if "<notification_music_reset_baseline>" not in notification_abort:
        for name in ("notification_music_status", "notification_music_known",
                     "notification_music_path", "notification_hibernate_suspended",
                     "notification_hibernate_resume_pending"):
            accesses = data_symbol_accesses(
                notification_abort, require_address(app_addresses, name),
            )
            if not any(op.startswith("str") for _pos, op, _line in accesses):
                raise GateError("retained refusal does not reset " + name)

    if "<audiohw_idle_powerdown>" not in pcm_suspend:
        raise GateError("PCM suspend does not power down CS42L55 while MCLK is live")
    require_order(pcm_suspend,
                  ["audiohw_hibernate_save", "audiohw_idle_powerdown"],
                  "codec snapshot before suspend mute")
    codec_restore = function(app_dis, "audiohw_hibernate_restore")
    pcm_finish = function(app_dis, "pcm_hibernate_resume_complete")
    require_order(codec_restore,
                  ["cscodec_reset", "sleep", "cscodec_write_checked",
                   "cscodec_read_checked"],
                  "codec reset, startup and verified register restore")
    if codec_restore.count("<sleep>") != 2 or "#196" not in codec_restore:
        raise GateError("codec restore omits reset/ramp waits or muted -60 dB")
    if "<audiohw_preinit>" in codec_restore:
        raise GateError("codec restore resets retained sound bookkeeping")
    if "<audiohw_hibernate_restore>" not in pcm_finish:
        raise GateError("PCM finish omits codec restoration")
    if "<pcm_play_dma_start>" in pcm_finish:
        raise GateError("codec restore starts PCM before the output service")
    if callers(app_dis, "audiohw_hibernate_restore") != {
            "pcm_hibernate_resume_complete"}:
        raise GateError("codec restore has an unexpected caller")
    if callers(app_dis, "pcm_hibernate_resume_complete") != {
            "ipod6g_hibernate_stage3_suspend"}:
        raise GateError("sleeping codec restore is outside the live-thread phase")
    require_call_before_targets_on_all_paths(
        suspend, "stage3_verify_tick", ["pcm_hibernate_resume_complete"],
        "codec restore after retained scheduler repair")
    blocked = require_address(app_addresses, "pcm_hibernate_codec_blocked")
    for body, operation in ((pcm_resume, "str"), (pcm_finish, "str"),
                            (function(app_dis, "pcm_play_dma_start"), "ldr")):
        if not any(op.startswith(operation) for _, op, _ in
                   data_symbol_accesses(body, blocked)):
            raise GateError("PCM codec restore failure does not inhibit starts")
    if "<pcm_play_stop>" in pcm_suspend:
        raise GateError(
            "target PCM suspend duplicates the outer output-service stop"
        )
    for forbidden in ("<sleep>", "<yield>", "<mutex_lock>"):
        if forbidden in pcm_suspend:
            raise GateError(f"PCM suspend calls scheduler primitive {forbidden}")
    require_order(
        pcm_resume,
        ["dmac_ch_init", "dmac_ch_lock_int", "pcm_dma_apply_settings"],
        "carried PCM lock on rebuilt output channel",
    )
    if "<pcm_play_dma_start>" in pcm_resume:
        raise GateError(
            "target PCM resume starts output before the outer opcode-9 service"
        )
    require_order(
        pcm_abort,
        ["pcm_dma_apply_settings", "audiohw_idle_powerup"],
        "pre-entry PCM refusal rollback",
    )
    if "<dmac_ch_init>" in pcm_abort:
        raise GateError("pre-entry PCM rollback recreates a live DMA channel")
    if callers(app_dis, "pcm_hibernate_abort") != {
        "ipod6g_hibernate_stage3_suspend"
    }:
        raise GateError("pre-entry PCM rollback has unexpected callers")
    if suspend.count("<pcm_hibernate_abort>") < 2:
        raise GateError("arm/entry-return PCM refusal rollback is incomplete")
    if "<ipod6g_videoout_disable>" not in videoout_suspend:
        raise GateError("composite suspend does not use the normal disable path")
    if "<svid_apply_policy>" not in videoout_resume:
        raise GateError("composite resume does not reapply retained policy")

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
    lcd_event_end = suspend.find("<lcd_hibernate_resume_complete>")
    composite_restore = suspend.find(
        "<ipod6g_videoout_hibernate_resume>", lcd_event_end
    )
    if lcd_event_end < 0 or composite_restore < 0:
        raise GateError(
            "successful resume does not restore composite after the LCD event"
        )
    composite_result = suspend[composite_restore:composite_restore + 1100]
    # The video result is spilled across the codec restore call. Both bool
    # results must be ANDed and branch to failure before publishing PASSED.
    if not re.search(
            r"str\s+r0, \[sp, #(?P<slot>\d+)\].*"
            r"<pcm_hibernate_resume_complete>.*"
            r"ldr\s+(?P<reg>r\d+), \[sp, #(?P=slot)\].*"
            r"and\s+(?P=reg), (?P=reg), r0.*"
            r"ands\s+\w+, (?P=reg), #255.*\bbeq\b",
            composite_result, re.S):
        raise GateError("production pass ignores video or codec restore failure")

    if "<system_hibernate_resume_usec_timer>" not in system_resume:
        raise GateError("hardware resume omits the RetailOS Timer E repair")
    require_order(
        system_resume,
        [
            "system_hibernate_resume_usec_timer", "clocking_init",
            "clockgate_enable", "vic_init", "dma_init",
        ],
        "system_hibernate_resume",
    )
    if re.search(r"\bblx?\b", usec_timer):
        raise GateError("Timer E repair unexpectedly calls another function")
    for literal in ("#1088", "#11", "#3"):
        if literal not in usec_timer:
            raise GateError(f"Timer E repair is missing stock literal {literal}")
    if "mvn" not in usec_timer or len(re.findall(r"\bstr\b", usec_timer)) < 4:
        raise GateError("Timer E repair is not the four-write RetailOS sequence")

    for owner, body in (
        ("gpio_hibernate_suspend", gpio_suspend),
        ("gpio_hibernate_resume", gpio_resume),
        ("eint_hibernate_suspend", eint_suspend),
        ("eint_hibernate_clear_pending", eint_clear),
        ("eint_hibernate_resume", eint_resume),
        ("stage3_save_apple_irq", save_apple_irq),
        ("stage3_restore_apple_irq", restore_apple_irq),
    ):
        for forbidden in scheduler_calls:
            if f"<{forbidden}>" in body:
                raise GateError(f"{owner} calls scheduler primitive {forbidden}")
    if len(re.findall(r"\bldr\b", gpio_suspend)) < 3:
        raise GateError("GPIO suspend does not read PCON, PDAT, and pull state")
    if len(re.findall(r"\bstr\b", gpio_resume)) < 3:
        raise GateError("GPIO resume does not restore pulls and PCON")
    if len(re.findall(r"\bldr\b", eint_suspend)) < 3:
        raise GateError("EIC suspend does not snapshot enable, level, and type")
    if "<eint_hibernate_clear_pending>" not in eint_suspend:
        raise GateError("EIC suspend omits Apple's pre-entry pending clear")
    if len(re.findall(r"\bstr\b", eint_suspend)) < 7:
        raise GateError("EIC suspend omits Apple's sleep-time VIC/EIC topology")
    if len(re.findall(r"\bstr\b", eint_clear)) < 2:
        raise GateError("EIC pending clear omits group 3 or group 6")
    if len(re.findall(r"\bstr\b", eint_resume)) < 3:
        raise GateError("EIC resume omits enable/level/type restoration")
    require_order(
        save_apple_irq,
        ["eint_hibernate_suspend", "record_commit_crc", "commit_dcache"],
        "RetailOS interrupt snapshot/sleep topology",
    )
    require_order(
        restore_apple_irq,
        ["eint_hibernate_clear_pending", "eint_hibernate_resume"],
        "RetailOS interrupt restore",
    )
    if len(re.findall(r"\bstr\b", restore_apple_irq)) < 4:
        raise GateError("Apple-order interrupt restore omits retained VIC state")

    # RetailOS device-manager mode 2/3 performs a precise wheel transaction.
    # Reject the older approximation that stopped/clock-gated the controller,
    # skipped wake initialization while Hold was locked, and drained one
    # generic interrupt response instead of restoring command state.
    if "<clockgate_enable>" in button_suspend:
        raise GateError("wheel mode 2 still clock-gates the controller")
    for literal in ("000e040e", "000e020e", "#1000"):
        if literal not in button_suspend:
            raise GateError(
                f"wheel mode 2 is missing stock GPIO/delay value {literal}"
            )
    if len(re.findall(r"\bstr\b", button_suspend)) < 2:
        raise GateError("wheel mode 2 omits one of Apple's two GPIO writes")
    if "<pmu_holdswitch_locked>" in button_resume:
        raise GateError("wheel mode 3 is still conditional on retained Hold")
    require_order(
        button_resume,
        [
            "button_hibernate_wheel_init",
            "button_hibernate_wheel_restore_command",
            "button_hibernate_resume_state",
        ],
        "RetailOS click-wheel mode 3",
    )
    if "<INT_WHEEL>" in button_resume or "<s5l_clickwheel_init>" in button_resume:
        raise GateError("wheel resume still uses the generic one-response drain")
    if "000061a8" not in button_resume:
        raise GateError("wheel mode 3 omits Apple's unconditional 25 ms delay")
    require_order(
        wheel_restore,
        ["button_hibernate_wheel_exchange", "button_hibernate_wheel_send"],
        "RetailOS click-wheel command-state restore",
    )
    for literal in ("8000063a", "8000062a", "8000ffff", "00007fff", "#5"):
        if literal not in wheel_restore:
            raise GateError(
                f"wheel command-state restore is missing stock literal {literal}"
            )
    if "subs" not in wheel_restore or len(re.findall(r"\bstr\b", wheel_restore)) < 3:
        raise GateError(
            "wheel command-state restore omits the five-zero-response epoch "
            "or its outer WHEEL10 save/restore"
        )
    if "<button_hibernate_wheel_send>" not in wheel_exchange:
        raise GateError("wheel exchange does not use the stock command sender")
    # Pending-packet handling may be laid out after the receive block.
    # Each WHEELRX load must feed the observer before control leaves that
    # straight-line block, regardless of the blocks' address order.
    response_reads = list(re.finditer(
        r"^.*\bldr\s+r[0-9]+, \[r[0-9]+, #24\].*$",
        wheel_exchange, re.MULTILINE,
    ))
    if len(response_reads) != 2:
        raise GateError("wheel exchange omits pending or returned WHEELRX read")
    for read in response_reads:
        block = "\n".join(wheel_exchange[read.end():].splitlines()[:4])
        if "<button_hibernate_wheel_observe_response>" not in block:
            raise GateError("wheel exchange discards a controller button packet")
    if "<button_hibernate_wheel_sample>" in wheel_observe_response:
        raise GateError(
            "an early wheel response can open the wake gate before the final state"
        )
    for literal in ("8000001a", "8000023a"):
        if literal not in wheel_observe_response:
            raise GateError(
                "synchronous wheel response parser omits packet " + literal
            )
    if ("#2000" not in wheel_exchange or
            len(re.findall(r"\bstr\b", wheel_exchange)) < 5):
        raise GateError(
            "wheel exchange omits the 2 ms receive poll, duplicate interrupt "
            "clear, or WHEEL10 save/restore"
        )
    if "<clockgate_enable>" not in wheel_init:
        raise GateError("wheel full init does not restore the controller clock")
    for literal in (
        "000e0202", "000e0302", "0003a980", "3c200000", "3cf00000",
        "#458752", "#524288", "#4194304", "#1048576", "#2097152", "#7",
    ):
        if literal not in wheel_init:
            raise GateError(f"wheel full init is missing stock literal {literal}")
    if (len(re.findall(r"\bstr\b", wheel_init)) < 13 or
            len(re.findall(r"\bldr\b", wheel_init)) < 8):
        raise GateError(
            "wheel full init collapsed Apple's individual GPIO/WHEEL00 edges"
        )
    if ("3c200000" not in wheel_send or "3c700000" not in wheel_send or
            "000005db" not in wheel_send or "#1000" not in wheel_send):
        raise GateError("wheel sender omits controller or Timer E polling")

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
    if len(re.findall(r"\bstr\b", restore_all_vic)) < 5:
        raise GateError("full VIC restore is missing pending-timer or mask writes")

    for hook in (
        "system_hibernate_resume", "stage3_restore_apple_irq",
        "button_hibernate_resume", "gpio_hibernate_resume",
        "uart_hibernate_resume",
        "pmu_hibernate_resume", "button_hibernate_resume_complete",
        "power_hibernate_resume",
        "pcm_hibernate_resume", "lcd_hibernate_resume",
    ):
        if f"<{hook}>" not in enter:
            raise GateError(f"application continuation omits {hook}")

    require_order(
        enter,
        [
            "system_hibernate_resume", "ata_hibernate_resume",
            "stage3_restore_apple_irq", "button_hibernate_resume",
            "gpio_hibernate_resume", "uart_hibernate_resume",
            "pmu_hibernate_resume", "button_hibernate_resume_complete",
            "power_hibernate_resume",
            "usb_hibernate_resume", "pcm_hibernate_resume",
            "lcd_hibernate_resume", "stage3_mask_vic",
        ],
        "RetailOS-ordered retained hardware continuation",
    )
    if "<pmu_hibernate_runtime_monitor_enable>" in suspend:
        raise GateError("production wake still arms the diagnostic PMU monitor")

    if "<ipod6g_hibernate_stage3_resume>" not in validator:
        raise GateError("bootloader validator does not dispatch direct resume")
    if re.search(r"\bblx?\b", trampoline):
        raise GateError("bootloader resume trampoline unexpectedly calls another function")
    if not re.search(r"\bbx\s+r2\b", trampoline):
        raise GateError("bootloader resume trampoline does not end in direct PC handoff")
    if re.search(r"\bbx\s+lr\b|\bpop\b.*\bpc\b", trampoline):
        raise GateError("bootloader resume trampoline contains a return path")

    # Verify the production transport contains the bounds exercised by the
    # host fault-injection tests (diagnostic-only bounds do not qualify).
    for name in ("wait_rdy_since", "i2c_wait_io"):
        body = function(app_dis, name)
        for literal in ("000186a0", "3c700000"):
            if literal not in body:
                raise GateError(name + " omits the 100-ms Timer-E deadline")
        if not re.search(r"\bsub\s", body) or not re.search(r"\bcmp\s", body):
            raise GateError(name + " omits elapsed-time comparison")
    for wrapper, transfer in (("i2c_read", "i2c_rd"),
                              ("i2c_write", "i2c_wr")):
        require_order(function(app_dis, wrapper),
                      ["mutex_lock", transfer, "mutex_unlock"], wrapper)
        body = function(app_dis, transfer)
        for marker in ("<i2c_abort>", "<i2c_off>", "#96"):
            if marker not in body:
                raise GateError(transfer + " omits bounded error cleanup")
        if body.count("<i2c_on>") != 1 or "<i2c_preinit>" in body:
            raise GateError(transfer + " retries or resets the transaction")
    abort = function(app_dis, "i2c_abort")
    for name in ("wait_rdy", "wait_rdy_since", "i2c_wait_io", "sleep"):
        if f"<{name}>" in abort:
            raise GateError("I2C abort waits on the failed controller")
    adc = function(app_dis, "pmu_read_adc")
    if "0003d08f" not in adc and "0003d090" not in adc:
        raise GateError("production ADC omits its 250-ms conversion bound")
    for marker in ("3c700000", "<sleep>", "<pmu_read_multiple>",
                   "<mutex_unlock>"):
        if marker not in adc:
            raise GateError("production ADC omits checked completion: " + marker)
    if "<pmu_read>" in adc or "<yield>" in adc:
        raise GateError("production ADC ignores transfer errors or starves the UI")

    app_strings = command("strings", str(args.app_elf))
    if "Stage 4-P16 / ABI 12 / CODEC RESTORE" not in app_strings:
        raise GateError("application does not identify Stage 4-P16 / ABI 12")
    if "/.rockbox/hibernate-rolo.cfg" not in app_strings:
        raise GateError(
            "Rolo candidate does not isolate its settings from config.cfg"
        )
    for marker in (
        "DARK HIBERNATE P16", "CODEC RESTORE", "WAKE EVENT 17",
    ):
        if marker not in app_strings:
            raise GateError(f"Rolo candidate identity is missing {marker!r}")
    if "/.rockbox/hibernate-p16-last.txt" not in app_strings:
        raise GateError("P16 candidate omits its persistent refusal trace")

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
        raise GateError(f"unexpected Stage 3B-R12 contract: {app_contract}")

    print("PASS: iPod 6G production retained-resume linked-image gate")
    print(f"  app version: {app_version}")
    print(f"  boot version: {boot_version}")
    print(f"  resume contract: {app_contract}")
    print("  production I2C/ADC: bounded completion and checked cleanup")
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
