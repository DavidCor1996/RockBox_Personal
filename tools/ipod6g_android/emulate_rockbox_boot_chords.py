#!/usr/bin/env python3
"""Execute the exact iPod 6G Android and USB boot-chord functions."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


IRAM_BASE = 0x22000000
IRAM_SIZE = 0x00040000
DFU_BODY_ADDRESS = 0x22020800
RETURN_SENTINEL = 0x22000100
BUTTON_MAIN = 0x7F
BUTTON_SELECT_RIGHT = 0x09
BUTTON_MENU_PLAY = 0x42


class ChordGateError(RuntimeError):
    """The linked bootloader does not implement the required chord contract."""


def run(*command: str) -> str:
    return subprocess.check_output(command, text=True)


def symbols(path: Path, nm: str) -> dict[str, int]:
    output = run(nm, "-n", "--defined-only", str(path))
    result: dict[str, int] = {}
    pattern = re.compile(r"^([0-9a-fA-F]+)\s+\S\s+(\S+)$")
    for line in output.splitlines():
        match = pattern.match(line)
        if match:
            result[match.group(2)] = int(match.group(1), 16)
    return result


def execute_chord_function(
    payload: bytes, address: int, buttons: int
) -> tuple[bool, int]:
    """
    Interpret the tiny linked ARMv5 leaf function directly from bootloader.bin.

    This deliberately supports only the instruction sequence GCC emits for an
    exact integer equality predicate: SUB-immediate, CLZ, LSR-immediate, and
    BX LR.  An optimizer or code-shape change therefore fails closed instead
    of silently widening what this qualification gate claims to execute.
    """
    registers = [0] * 16
    registers[0] = buttons
    registers[14] = RETURN_SENTINEL
    registers[15] = address
    returned = False

    for _ in range(64):
        pc = registers[15]
        offset = pc - DFU_BODY_ADDRESS
        if offset < 0 or offset + 4 > len(payload) or offset % 4:
            raise ChordGateError(
                f"chord PC 0x{pc:08x} is outside aligned bootloader.bin"
            )
        instruction = int.from_bytes(payload[offset:offset + 4], "little")
        registers[15] = (pc + 4) & 0xFFFFFFFF

        if instruction & 0xFFFFF000 == 0xE2400000:
            # SUB r0, r0, #imm12, with an unrotated ARM immediate.
            registers[0] = (registers[0] - (instruction & 0xFFF)) & 0xFFFFFFFF
        elif instruction & 0xFFFFF000 == 0xE3A00000:
            # MOV r0, #imm12, with an unrotated ARM immediate.
            registers[0] = instruction & 0xFFF
        elif instruction == 0xE16F0F10:
            # CLZ r0, r0.
            value = registers[0]
            registers[0] = 32 if value == 0 else 32 - value.bit_length()
        elif instruction == 0xE1A002A0:
            # MOV r0, r0, LSR #5.
            registers[0] >>= 5
        elif instruction == 0xE12FFF1E:
            # BX LR.
            registers[15] = registers[14]
            returned = True
            break
        else:
            raise ChordGateError(
                f"unsupported chord instruction 0x{instruction:08x} "
                f"at 0x{pc:08x}"
            )

    if not returned or registers[15] != RETURN_SENTINEL:
        raise ChordGateError(
            f"chord function at 0x{address:08x} did not return within 64 instructions"
        )
    raw_result = registers[0]
    if raw_result not in (0, 1):
        raise ChordGateError(
            f"chord function at 0x{address:08x} returned {raw_result}, not bool"
        )
    return bool(raw_result), raw_result


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bootloader-bin", required=True, type=Path)
    parser.add_argument("--bootloader-elf", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    parser.add_argument("--require-forced-android", action="store_true")
    args = parser.parse_args(argv)

    payload = args.bootloader_bin.resolve(strict=True).read_bytes()
    elf = args.bootloader_elf.resolve(strict=True)
    if DFU_BODY_ADDRESS + len(payload) > IRAM_BASE + IRAM_SIZE:
        raise ChordGateError("bootloader.bin does not fit the modeled IRAM")

    linked_symbols = symbols(elf, args.nm)
    function_names = (
        "n25_android_boot_chord",
        "n25_usb_mode_chord",
    )
    missing = [name for name in function_names if name not in linked_symbols]
    if missing:
        raise ChordGateError("missing linked chord functions: " + ", ".join(missing))

    main_disassembly = run(
        args.objdump, "-d", "--disassemble=main", str(elf)
    )
    for name in function_names:
        if f"<{name}>" not in main_disassembly:
            raise ChordGateError(f"bootloader main does not call {name}")

    forced_cases = None
    if args.require_forced_android:
        force_name = "n25_android_force_volatile_test"
        if force_name not in linked_symbols:
            raise ChordGateError("forced volatile Android function is missing")
        if f"<{force_name}>" not in main_disassembly:
            raise ChordGateError(
                "bootloader main does not call the forced Android function"
            )
        forced_cases = {}
        for buttons in range(BUTTON_MAIN + 1):
            forced, _ = execute_chord_function(
                payload, linked_symbols[force_name], buttons
            )
            if not forced:
                raise ChordGateError(
                    f"volatile Android force disabled for 0x{buttons:02x}"
                )
            forced_cases[f"0x{buttons:02x}"] = True

    cases = {}
    for buttons in range(BUTTON_MAIN + 1):
        android, _ = execute_chord_function(
            payload, linked_symbols["n25_android_boot_chord"], buttons
        )
        usb, _ = execute_chord_function(
            payload, linked_symbols["n25_usb_mode_chord"], buttons
        )
        expected_android = buttons == BUTTON_SELECT_RIGHT
        expected_usb = buttons == BUTTON_MENU_PLAY
        if android != expected_android:
            raise ChordGateError(
                f"Android chord mismatch for button bits 0x{buttons:02x}"
            )
        if usb != expected_usb:
            raise ChordGateError(
                f"USB chord mismatch for button bits 0x{buttons:02x}"
            )
        if android and usb:
            raise ChordGateError(
                f"Android and USB overlap for button bits 0x{buttons:02x}"
            )
        cases[f"0x{buttons:02x}"] = {
            "android": android,
            "usb": usb,
        }

    report = {
        "schema": 1,
        "scope": "ipod6g-rockbox-boot-chord-exact-binary-emulation",
        "cpu": "ARM926EJ-S",
        "button_patterns_tested": BUTTON_MAIN + 1,
        "android_chord": "Select+Right",
        "android_button_bits": f"0x{BUTTON_SELECT_RIGHT:02x}",
        "usb_chord": "Menu+Play",
        "usb_button_bits": f"0x{BUTTON_MENU_PLAY:02x}",
        "exact_chords_only": True,
        "main_calls_qualified_functions": True,
        "android_usb_overlap": False,
        "gate_passed": True,
        "bootloader_bin_sha256": hashlib.sha256(payload).hexdigest(),
        "cases": cases,
    }
    if forced_cases is not None:
        report.update(
            {
                "forced_android_volatile_test": True,
                "forced_button_patterns_tested": BUTTON_MAIN + 1,
                "forced_main_calls_qualified_function": True,
                "forced_cases": forced_cases,
            }
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ChordGateError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"boot chord gate failed: {error}")
