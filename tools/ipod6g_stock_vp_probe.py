#!/usr/bin/env python3
"""Execute the stock iPod 6G VP auto-geometry routine under Unicorn.

This is an analysis helper for the decrypted iPod 2.0.4 OSOS body.  It runs
the exact ARM routine at body offset 0x169074 and reports the SVID VP geometry
registers it writes, without booting or modifying an iPod.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


ANALYSIS_MODULES = Path("/tmp/ipod-vp-analysis")
if ANALYSIS_MODULES.is_dir():
    sys.path.insert(0, str(ANALYSIS_MODULES))

from unicorn import (  # noqa: E402
    Uc,
    UC_ARCH_ARM,
    UC_HOOK_CODE,
    UC_HOOK_MEM_INVALID,
    UC_MODE_ARM,
)
from unicorn.arm_const import (  # noqa: E402
    UC_ARM_REG_CPSR,
    UC_ARM_REG_LR,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_R3,
    UC_ARM_REG_R4,
    UC_ARM_REG_R5,
    UC_ARM_REG_R6,
    UC_ARM_REG_R7,
    UC_ARM_REG_R8,
    UC_ARM_REG_R9,
    UC_ARM_REG_R10,
    UC_ARM_REG_R11,
    UC_ARM_REG_SP,
)


IMAGE_BASE = 0x08000800
GEOMETRY_OFFSET = 0x00169074
GLOBAL_STATE = 0x089CC9B4
VP_BASE = 0x39100000
STACK_BASE = 0x10000000
STACK_SIZE = 0x00010000
RETURN_PC = STACK_BASE + STACK_SIZE - 0x1000

VP_REGISTERS = {
    0x044: "src_x",
    0x048: "src_y",
    0x04C: "src_w",
    0x050: "src_h",
    0x054: "dst_x",
    0x058: "dst_y",
    0x05C: "dst_w",
    0x060: "dst_h",
    0x064: "h_ratio",
    0x068: "v_ratio",
}


def align_up(value: int, alignment: int = 0x1000) -> int:
    return (value + alignment - 1) & -alignment


def pack_u32(value: int) -> bytes:
    return struct.pack("<I", value & 0xFFFFFFFF)


def pack_f32(value: float) -> bytes:
    return struct.pack("<f", value)


def read_u32(emulator: Uc, address: int) -> int:
    return struct.unpack("<I", emulator.mem_read(address, 4))[0]


def probe(
    body: bytes,
    source_width: int,
    source_height: int,
    output_mode: int,
    widescreen: int,
    standard: int,
    frame_flag: int,
    aspect_numerator: int,
    aspect_denominator: int,
    trace: bool = False,
) -> dict[str, int]:
    emulator = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    image_map_base = IMAGE_BASE & ~0xFFF
    image_prefix = IMAGE_BASE - image_map_base
    image_map_size = align_up(image_prefix + len(body))
    emulator.mem_map(image_map_base, image_map_size)
    emulator.mem_write(IMAGE_BASE, body)
    emulator.mem_map(VP_BASE, 0x1000)
    emulator.mem_map(STACK_BASE, STACK_SIZE)

    # The stock draw method writes the incoming frame dimensions to VP+0x4c
    # and VP+0x50 immediately before calling the auto-geometry routine.
    emulator.mem_write(VP_BASE + 0x04C, pack_u32(source_width))
    emulator.mem_write(VP_BASE + 0x050, pack_u32(source_height))

    def invalid_memory(_uc, access, address, size, value, _user_data):
        raise RuntimeError(
            f"unmapped memory access type={access} address=0x{address:08x} "
            f"size={size} value=0x{value:x}"
        )

    emulator.hook_add(UC_HOOK_MEM_INVALID, invalid_memory)

    if trace:
        trace_offsets = {
            0x001690C8,
            0x00169158,
            0x0016918C,
            0x00169294,
            0x001692FC,
            0x00169330,
            0x00169350,
        }

        def trace_code(uc, address, _size, _user_data):
            offset = address - IMAGE_BASE
            if offset not in trace_offsets:
                return
            registers = (
                UC_ARM_REG_R0,
                UC_ARM_REG_R1,
                UC_ARM_REG_R2,
                UC_ARM_REG_R3,
                UC_ARM_REG_R4,
                UC_ARM_REG_R5,
                UC_ARM_REG_R6,
                UC_ARM_REG_R7,
                UC_ARM_REG_R8,
                UC_ARM_REG_R9,
                UC_ARM_REG_R10,
                UC_ARM_REG_R11,
            )
            values = " ".join(
                f"r{index:x}={uc.reg_read(register):08x}"
                for index, register in enumerate(registers)
            )
            print(
                f"stock+0x{offset:06x} {values} "
                f"cpsr={uc.reg_read(UC_ARM_REG_CPSR):08x}",
                file=sys.stderr,
            )

        emulator.hook_add(UC_HOOK_CODE, trace_code)

    # Recreate the fields initialized by the stock NTSC/output-aspect setters.
    emulator.mem_write(GLOBAL_STATE, b"\0" * 0x80)
    emulator.mem_write(GLOBAL_STATE + 0, bytes((output_mode, widescreen, standard)))
    # RetailOS keeps the currently configured source geometry here.  The
    # auto-geometry routine reads the prior width before replacing it with the
    # incoming frame width, so the first call must start with the active frame.
    emulator.mem_write(GLOBAL_STATE + 0x10, pack_u32(source_width))
    emulator.mem_write(GLOBAL_STATE + 0x14, pack_u32(source_height))
    standard_width = 768 if standard else 640
    output_width = 720.0 if output_mode == 0 else float(standard_width)
    pixel_aspect = 0.5625 if widescreen else 0.75
    emulator.mem_write(GLOBAL_STATE + 0x18, pack_f32(output_width))
    standard_height = 576 if standard else 480
    emulator.mem_write(GLOBAL_STATE + 0x1C, pack_u32(standard_height))
    emulator.mem_write(GLOBAL_STATE + 0x20, pack_u32(standard_height))
    # The standard initializer stores the square-pixel width here.  Selecting
    # the 720-sample TV mode only changes +0x18; it deliberately leaves +0x24
    # at 640 (NTSC) or 768 (PAL).
    emulator.mem_write(GLOBAL_STATE + 0x24, pack_u32(standard_width))
    emulator.mem_write(GLOBAL_STATE + 0x28, pack_f32(pixel_aspect))

    entry_sp = STACK_BASE + STACK_SIZE // 2
    extra_arguments = (
        standard,
        frame_flag,
        aspect_numerator,
        aspect_denominator,
    )
    emulator.mem_write(entry_sp, struct.pack("<4I", *extra_arguments))
    emulator.reg_write(UC_ARM_REG_SP, entry_sp)
    emulator.reg_write(UC_ARM_REG_LR, RETURN_PC)
    emulator.reg_write(UC_ARM_REG_R0, source_width)
    emulator.reg_write(UC_ARM_REG_R1, source_height)
    emulator.reg_write(UC_ARM_REG_R2, widescreen)
    emulator.reg_write(UC_ARM_REG_R3, output_mode)
    emulator.emu_start(IMAGE_BASE + GEOMETRY_OFFSET, RETURN_PC, count=200000)

    return {
        name: read_u32(emulator, VP_BASE + offset)
        for offset, name in VP_REGISTERS.items()
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("body", type=Path)
    parser.add_argument("--source", default="640x480")
    parser.add_argument("--output-mode", type=int, choices=(0, 1), default=0)
    parser.add_argument("--widescreen", type=int, choices=(0, 1), default=0)
    parser.add_argument("--standard", type=int, choices=(0, 1), default=0)
    parser.add_argument("--frame-flag", type=int, default=0)
    parser.add_argument("--aspect", default="1:1")
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()

    source_width, source_height = map(int, args.source.lower().split("x", 1))
    aspect_numerator, aspect_denominator = map(int, args.aspect.split(":", 1))
    values = probe(
        args.body.read_bytes(),
        source_width,
        source_height,
        args.output_mode,
        args.widescreen,
        args.standard,
        args.frame_flag,
        aspect_numerator,
        aspect_denominator,
        args.trace,
    )
    print(" ".join(f"{name}={value}" for name, value in values.items()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
