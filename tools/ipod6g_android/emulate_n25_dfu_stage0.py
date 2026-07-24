#!/usr/bin/env python3
"""Execute the complete compressed N25 DFU stage zero in an ARM926 model."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from unicorn import (
    UC_ARCH_ARM,
    UC_HOOK_CODE,
    UC_HOOK_MEM_INVALID,
    UC_HOOK_MEM_WRITE,
    UC_MODE_ARM,
    Uc,
    UcError,
)
from unicorn.arm_const import UC_ARM_REG_CPSR, UC_ARM_REG_PC


IRAM_BASE = 0x22000000
IRAM_SIZE = 0x40000
DRAM_BASE = 0x08000000
DRAM_SIZE = 0x04000000
DRAM_STAGING = 0x08010000
RELOCATOR_ADDRESS = 0x2203C000
STAGE_EXEC_ADDRESS = 0x22020800
CLK_BASE = 0x3C500000
MIU_BASE = 0x38100000
WDT_BASE = 0x3C800000


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--uboot", type=Path, required=True)
    parser.add_argument("--compressed", type=Path, required=True)
    parser.add_argument("--relocator", type=Path, required=True)
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--max-instructions", type=int, default=100_000_000)
    args = parser.parse_args()

    stage0 = args.stage0.read_bytes()
    uboot = args.uboot.read_bytes()
    compressed = args.compressed.read_bytes()
    relocator = args.relocator.read_bytes()
    if len(stage0) > 112 * 1024:
        raise SystemExit("stage zero exceeds the 112 KiB Boot ROM gate")
    if len(uboot) >= RELOCATOR_ADDRESS - IRAM_BASE:
        raise SystemExit("U-Boot overlaps the IRAM relocator window")

    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    uc.mem_map(IRAM_BASE, IRAM_SIZE)
    uc.mem_map(DRAM_BASE, DRAM_SIZE)
    for base in (CLK_BASE, MIU_BASE, WDT_BASE):
        uc.mem_map(base, 0x1000)
    uc.mem_write(IRAM_BASE, stage0)

    instructions = 0
    entry_visits = 0
    reached_relocator = False
    reached_uboot = False
    reached_relocated_stage = False
    modeled_write_buffer_drain = False
    mmio_writes: list[tuple[int, int]] = []

    def code_hook(emulator: Uc, address: int, size: int, data: object) -> None:
        nonlocal instructions, entry_visits, reached_relocator, reached_uboot
        nonlocal reached_relocated_stage
        nonlocal modeled_write_buffer_drain
        del size, data
        instructions += 1
        # Unicorn does not implement ARM926's legacy
        # mrc p15,0,r15,c7,c10,3 write-buffer test. Model its drained result.
        instruction = bytes(emulator.mem_read(address, 4))
        if instruction == b"\x7a\xff\x17\xee":
            modeled_write_buffer_drain = True
            emulator.reg_write(UC_ARM_REG_PC, address + 8)
            return
        if address == STAGE_EXEC_ADDRESS:
            reached_relocated_stage = True
        if address == RELOCATOR_ADDRESS:
            reached_relocator = True
        if address == IRAM_BASE:
            entry_visits += 1
            if entry_visits == 2:
                reached_uboot = True
                emulator.emu_stop()

    def invalid_hook(
        emulator: Uc,
        access: int,
        address: int,
        size: int,
        value: int,
        data: object,
    ) -> bool:
        del emulator, access, size, value, data
        raise RuntimeError(f"unexpected unmapped access at 0x{address:08x}")

    def write_hook(
        emulator: Uc,
        access: int,
        address: int,
        size: int,
        value: int,
        data: object,
    ) -> None:
        del emulator, access, data
        if size == 4 and (
            CLK_BASE <= address < CLK_BASE + 0x1000
            or MIU_BASE <= address < MIU_BASE + 0x1000
            or WDT_BASE <= address < WDT_BASE + 0x1000
        ):
            mmio_writes.append((address, value & 0xFFFFFFFF))

    uc.hook_add(UC_HOOK_CODE, code_hook)
    uc.hook_add(UC_HOOK_MEM_INVALID, invalid_hook)
    uc.hook_add(UC_HOOK_MEM_WRITE, write_hook)
    uc.reg_write(UC_ARM_REG_CPSR, 0xD3)
    uc.reg_write(UC_ARM_REG_PC, IRAM_BASE)

    try:
        uc.emu_start(IRAM_BASE, IRAM_BASE + IRAM_SIZE, count=args.max_instructions)
    except (UcError, RuntimeError) as error:
        pc = uc.reg_read(UC_ARM_REG_PC)
        raise SystemExit(f"stage-zero emulation failed at 0x{pc:08x}: {error}")

    if not reached_relocated_stage:
        raise SystemExit("stage zero never entered its Rockbox-layout IRAM1 copy")
    if not reached_relocator:
        raise SystemExit("stage zero never entered the IRAM relocator")
    if not reached_uboot:
        raise SystemExit("stage zero never reached the reconstructed U-Boot entry")
    if uc.mem_read(DRAM_STAGING, len(compressed)) != compressed:
        raise SystemExit("compressed U-Boot DRAM staging mismatch")
    if uc.mem_read(RELOCATOR_ADDRESS, len(relocator)) != relocator:
        raise SystemExit("IRAM relocator copy mismatch")
    reconstructed = bytes(uc.mem_read(IRAM_BASE, len(uboot)))
    if reconstructed != uboot:
        raise SystemExit("reconstructed U-Boot differs from the qualified binary")

    required_writes = {
        (MIU_BASE + 0x00, 0x0000080D),
        (MIU_BASE + 0x08, 0x0006105D),
        (MIU_BASE + 0x10, 0x001FB621),
        (MIU_BASE + 0x0C, 0x00008040),
    }
    if not required_writes.issubset(set(mmio_writes)):
        raise SystemExit("Rockbox-derived N25 DRAM initialization is incomplete")

    report = {
        "gate_passed": True,
        "cpu": "ARM926EJ-S",
        "stage0_size": len(stage0),
        "bootrom_body_limit": 112 * 1024,
        "instructions": instructions,
        "reached_relocator": reached_relocator,
        "reached_relocated_stage": reached_relocated_stage,
        "reached_uboot_entry": reached_uboot,
        "modeled_arm926_write_buffer_drain": modeled_write_buffer_drain,
        "dram_staging_matches": True,
        "relocator_copy_matches": True,
        "reconstructed_uboot_matches": True,
        "mmio_write_count": len(mmio_writes),
        "artifacts": {
            "stage0": sha256(stage0),
            "uboot": sha256(uboot),
            "compressed": sha256(compressed),
            "relocator": sha256(relocator),
        },
    }
    if args.json_output:
        args.json_output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
