#!/usr/bin/env python3
"""Trace a decrypted iPod eApp with framework imports replaced by safe traps.

This host-only research tool intentionally does not execute retailOS code.  It
maps the eApp at its preferred address, replaces loader-populated import slots
with unique trap addresses, and records calls made by the game's ARM code.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path
from typing import Any

try:
    from .eapp_inspect import EappError, inspect_eapp
except ImportError:
    from eapp_inspect import EappError, inspect_eapp


PAGE_SIZE = 0x1000
IMAGE_RESERVE = 0x01000000
STACK_BASE = 0x1D000000
STACK_SIZE = 0x00100000
HEAP_BASE = 0x1B000000
HEAP_SIZE = 0x01000000
SCRATCH_BASE = 0x1C000000
SCRATCH_SIZE = 0x00010000
TRAP_BASE = 0x1F000000
TRAP_SIZE = 0x00010000
RETURN_SENTINEL = TRAP_BASE + TRAP_SIZE - 4


class TraceError(ValueError):
    """Raised when an eApp cannot be traced safely."""


def _align_up(value: int, alignment: int = PAGE_SIZE) -> int:
    return (value + alignment - 1) & -alignment


def _parse_address(value: str) -> int:
    return int(value, 0)


def trace_eapp(
    path: Path,
    entry_name: str = "init",
    max_instructions: int = 1_000_000,
    stop_on_import: bool = False,
    frames: int = 1,
) -> dict[str, Any]:
    try:
        from unicorn import (
            UC_ARCH_ARM,
            UC_HOOK_CODE,
            UC_HOOK_INTR,
            UC_HOOK_MEM_INVALID,
            UC_MODE_ARM,
            Uc,
            UcError,
        )
        from unicorn.arm_const import (
            UC_ARM_REG_LR,
            UC_ARM_REG_PC,
            UC_ARM_REG_R0,
            UC_ARM_REG_R1,
            UC_ARM_REG_R2,
            UC_ARM_REG_R3,
            UC_ARM_REG_SP,
        )
    except ImportError as error:
        raise TraceError(
            "Unicorn is required; install it in a temporary environment with "
            "`python -m pip install unicorn`"
        ) from error

    report = inspect_eapp(path)
    raw = path.read_bytes()
    load_base = int(report["header"]["inferred_load_base"], 0)
    entry_fields = {
        "init": "word_14",
        "fini": "word_18",
        "event": "word_24",
    }
    if entry_name == "lifecycle":
        entries = [("init", int(report["header"]["word_14"], 0))]
        entries.extend(
            (
                f"event-{frame + 1}",
                int(report["header"]["word_24"], 0),
            )
            for frame in range(frames)
        )
    elif entry_name in entry_fields:
        entries = [(entry_name, int(report["header"][entry_fields[entry_name]], 0))]
    else:
        try:
            entries = [(entry_name, int(entry_name, 0))]
        except ValueError as error:
            raise TraceError(f"unknown entry {entry_name!r}") from error

    image_size = max(IMAGE_RESERVE, _align_up(len(raw)))
    for _phase, entry in entries:
        if not (load_base <= entry < load_base + image_size) or entry & 3:
            raise TraceError(f"entry 0x{entry:08x} is outside the mapped ARM image")

    imports: dict[int, dict[str, Any]] = {}
    slot_patches: list[tuple[int, int]] = []
    trap = TRAP_BASE
    for framework in report["frameworks"]:
        for ordinal in range(framework["import_count"]):
            if trap >= RETURN_SENTINEL:
                raise TraceError("framework imports exceed trap address space")
            imports[trap] = {
                "framework": framework["name"],
                "ordinal": ordinal,
            }
            slot_address = load_base + framework["slots_file_offset"] + ordinal * 4
            slot_patches.append((slot_address, trap))
            trap += 4

    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.mem_map(load_base, image_size)
    uc.mem_write(load_base, raw)
    uc.mem_map(STACK_BASE, STACK_SIZE)
    uc.mem_map(HEAP_BASE, HEAP_SIZE)
    uc.mem_map(SCRATCH_BASE, SCRATCH_SIZE)
    uc.mem_map(TRAP_BASE, TRAP_SIZE)
    # A trap remains safe even if a hook is accidentally omitted.
    uc.mem_write(TRAP_BASE, struct.pack("<I", 0xE12FFF1E) * (TRAP_SIZE // 4))
    for slot_address, trap_address in slot_patches:
        uc.mem_write(slot_address, struct.pack("<I", trap_address))

    stack_top = STACK_BASE + STACK_SIZE - 0x100
    uc.reg_write(UC_ARM_REG_SP, stack_top)
    uc.reg_write(UC_ARM_REG_LR, RETURN_SENTINEL)
    calls: list[dict[str, Any]] = []
    allocations: list[dict[str, Any]] = []
    heap_next = HEAP_BASE
    current_phase = entries[0][0]
    clock_ticks = 0
    next_texture_id = 1
    invalid_access: dict[str, Any] | None = None
    semihosting_output = bytearray()
    unknown_interrupt: dict[str, Any] | None = None

    def code_hook(machine: Any, address: int, _size: int, _data: Any) -> None:
        nonlocal heap_next, clock_ticks, next_texture_id
        if address == RETURN_SENTINEL:
            machine.emu_stop()
            return
        imported = imports.get(address)
        if imported is None:
            return
        sp = machine.reg_read(UC_ARM_REG_SP)
        try:
            stack_words = list(struct.unpack("<8I", machine.mem_read(sp, 32)))
        except UcError:
            stack_words = []
        call = {
                "sequence": len(calls),
                "phase": current_phase,
                **imported,
                "r0": f"0x{machine.reg_read(UC_ARM_REG_R0):08x}",
                "r1": f"0x{machine.reg_read(UC_ARM_REG_R1):08x}",
                "r2": f"0x{machine.reg_read(UC_ARM_REG_R2):08x}",
                "r3": f"0x{machine.reg_read(UC_ARM_REG_R3):08x}",
                "lr": f"0x{machine.reg_read(UC_ARM_REG_LR):08x}",
                "stack": [f"0x{word:08x}" for word in stack_words],
            }
        if imported["framework"] == "AsyncFileIO":
            snapshots = {}
            for register, register_id in (
                ("r1", UC_ARM_REG_R1),
                ("r2", UC_ARM_REG_R2),
                ("r3", UC_ARM_REG_R3),
            ):
                pointer = machine.reg_read(register_id)
                try:
                    snapshots[register] = bytes(
                        machine.mem_read(pointer, 64)
                    ).hex()
                except UcError:
                    continue
            request = machine.reg_read(UC_ARM_REG_R3)
            if request >= 32:
                try:
                    snapshots["r3_minus_32"] = bytes(
                        machine.mem_read(request - 32, 96)
                    ).hex()
                except UcError:
                    pass
            call["pointed_memory"] = snapshots
        elif imported["framework"] == "OpenGLES":
            pointer = None
            if imported["ordinal"] == 137 and len(stack_words) >= 2:
                pointer = stack_words[1]
            elif imported["ordinal"] == 149:
                pointer = machine.reg_read(UC_ARM_REG_R3)
            elif imported["ordinal"] == 165:
                pointer = machine.reg_read(UC_ARM_REG_R3)
            elif imported["ordinal"] == 175:
                pointer = machine.reg_read(UC_ARM_REG_R2)
            if pointer:
                try:
                    call["pointed_memory"] = {
                        "data": bytes(machine.mem_read(pointer, 64)).hex()
                    }
                except UcError:
                    pass
        calls.append(call)
        if stop_on_import:
            machine.emu_stop()
            return
        # The first two miscTBD exports are the per-eApp allocator and free
        # wrappers (confirmed from retailOS control flow and game call sites).
        # Supplying a deterministic arena lets startup progress while every
        # other unknown API still fails conservatively with zero.
        return_value = 0
        if imported["framework"] == "miscTBD" and imported["ordinal"] == 0:
            requested = machine.reg_read(UC_ARM_REG_R0)
            allocated = _align_up(max(requested, 1), 16)
            if heap_next + allocated <= HEAP_BASE + HEAP_SIZE:
                return_value = heap_next
                allocations.append(
                    {
                        "address": f"0x{heap_next:08x}",
                        "requested": requested,
                        "allocated": allocated,
                    }
                )
                heap_next += allocated
        elif imported["framework"] == "miscTBD" and imported["ordinal"] == 9:
            machine.mem_write(
                machine.reg_read(UC_ARM_REG_R0), struct.pack("<I", clock_ticks)
            )
            clock_ticks += 16
        elif imported["framework"] == "miscTBD" and imported["ordinal"] == 10:
            # Timer ticks per second in this retailOS build.
            return_value = 1000
        elif imported["framework"] == "OpenGLES" and imported["ordinal"] == 45:
            count = machine.reg_read(UC_ARM_REG_R0)
            names = machine.reg_read(UC_ARM_REG_R1)
            for index in range(count):
                machine.mem_write(
                    names + index * 4, struct.pack("<I", next_texture_id)
                )
                next_texture_id += 1
        elif imported["framework"] == "Audio" and imported["ordinal"] == 52:
            # retailOS implements this export as `return 255`; callers use it
            # as the denominator for the public 0..255 volume scale.
            return_value = 0xFF
        machine.reg_write(UC_ARM_REG_R0, return_value)
        machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

    def invalid_hook(
        machine: Any,
        access: int,
        address: int,
        size: int,
        value: int,
        _data: Any,
    ) -> bool:
        nonlocal invalid_access
        invalid_access = {
            "access": access,
            "address": f"0x{address:08x}",
            "size": size,
            "value": f"0x{value & 0xFFFFFFFF:08x}",
            "pc": f"0x{machine.reg_read(UC_ARM_REG_PC):08x}",
        }
        return False

    def interrupt_hook(machine: Any, interrupt: int, _data: Any) -> None:
        nonlocal unknown_interrupt
        operation = machine.reg_read(UC_ARM_REG_R0)
        argument = machine.reg_read(UC_ARM_REG_R1)
        # ARM's historical Angel semihosting ABI uses SVC 0x123456. eApps use
        # SYS_WRITEC while reporting diagnostics through their C runtime.
        if operation == 0x03:
            semihosting_output.extend(machine.mem_read(argument, 1))
            machine.reg_write(UC_ARM_REG_R0, 0)
            return
        if operation == 0x04:
            for index in range(4096):
                value = bytes(machine.mem_read(argument + index, 1))
                if value == b"\0":
                    machine.reg_write(UC_ARM_REG_R0, 0)
                    return
                semihosting_output.extend(value)
        unknown_interrupt = {
            "interrupt": interrupt,
            "operation": f"0x{operation:08x}",
            "argument": f"0x{argument:08x}",
            "pc": f"0x{machine.reg_read(UC_ARM_REG_PC):08x}",
        }
        machine.emu_stop()

    uc.hook_add(UC_HOOK_CODE, code_hook)
    uc.hook_add(UC_HOOK_INTR, interrupt_hook)
    uc.hook_add(UC_HOOK_MEM_INVALID, invalid_hook)
    outcome = "returned"
    error_text: str | None = None
    completed_phases: list[str] = []
    for current_phase, entry in entries:
        uc.reg_write(UC_ARM_REG_SP, stack_top)
        uc.reg_write(UC_ARM_REG_LR, RETURN_SENTINEL)
        if current_phase.startswith("event"):
            # retailOS supplies two writable scheduler/event structures. Their
            # exact layouts remain under study; zero-filled oversized buffers
            # are sufficient to expose the initial framework call path.
            uc.reg_write(UC_ARM_REG_R0, SCRATCH_BASE)
            uc.reg_write(UC_ARM_REG_R1, SCRATCH_BASE + PAGE_SIZE)
        try:
            calls_before = len(calls)
            uc.emu_start(entry, RETURN_SENTINEL, count=max_instructions)
            pc = uc.reg_read(UC_ARM_REG_PC)
            if stop_on_import and len(calls) > calls_before:
                outcome = "stopped-on-import"
                break
            if pc != RETURN_SENTINEL and invalid_access is None:
                outcome = "instruction-limit"
                break
            completed_phases.append(current_phase)
        except UcError as error:
            outcome = "invalid-memory" if invalid_access else "emulator-error"
            error_text = str(error)
            break

    result: dict[str, Any] = {
        "format": "ipod-eapp-trace-v1",
        "file": report["file"],
        "entry": entry_name,
        "entry_addresses": [
            {"phase": phase, "address": f"0x{address:08x}"}
            for phase, address in entries
        ],
        "completed_phases": completed_phases,
        "max_instructions": max_instructions,
        "requested_frames": frames if entry_name == "lifecycle" else 0,
        "outcome": outcome,
        "calls": calls,
        "call_count": len(calls),
        "allocations": allocations,
        "final_pc": f"0x{uc.reg_read(UC_ARM_REG_PC):08x}",
    }
    if invalid_access is not None:
        result["invalid_access"] = invalid_access
    if error_text is not None:
        result["emulator_error"] = error_text
    if semihosting_output:
        result["semihosting_output"] = semihosting_output.decode(
            "utf-8", errors="replace"
        )
    if unknown_interrupt is not None:
        result["unknown_interrupt"] = unknown_interrupt
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument(
        "--entry",
        default="init",
        help="init, fini, event, or an address (default: init)",
    )
    parser.add_argument("--max-instructions", type=int, default=1_000_000)
    parser.add_argument(
        "--frames",
        type=int,
        default=1,
        help="event callbacks after init when --entry=lifecycle (default: 1)",
    )
    parser.add_argument("--stop-on-import", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    if args.max_instructions <= 0 or args.frames <= 0:
        parser.error("--max-instructions and --frames must be positive")

    try:
        result = trace_eapp(
            args.executable,
            args.entry,
            args.max_instructions,
            args.stop_on_import,
            args.frames,
        )
    except (EappError, OSError, TraceError) as error:
        print(f"eapp_trace: {error}", file=sys.stderr)
        return 2

    if args.json:
        json.dump(result, sys.stdout, indent=2, sort_keys=True)
        print()
    else:
        print(
            f"Entry {result['entry']}: "
            f"{result['outcome']} after {result['call_count']} framework calls"
        )
        for call in result["calls"]:
            print(
                f"  {call['sequence']:03d} {call['phase']} "
                f"{call['framework']}[{call['ordinal']}] "
                f"r0={call['r0']} r1={call['r1']} r2={call['r2']} r3={call['r3']}"
            )
        if "invalid_access" in result:
            fault = result["invalid_access"]
            print(
                f"  fault: pc={fault['pc']} address={fault['address']} "
                f"size={fault['size']}"
            )
    return 0 if result["outcome"] in {"returned", "stopped-on-import"} else 1


if __name__ == "__main__":
    raise SystemExit(main())
