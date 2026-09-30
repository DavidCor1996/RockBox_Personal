#!/usr/bin/env python3
"""Execute the stock iPod 6G VP auto-geometry routine under Unicorn.

This is an analysis helper for the decrypted iPod 2.0.4 OSOS body.  It runs
the exact ARM routine at runtime address 0x0815e19c and reports the SVID VP geometry
registers it writes, without booting or modifying an iPod.
"""

from __future__ import annotations

import argparse
import hashlib
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
    UC_ARM_REG_PC,
)


# The startup relocator loads the DRAM body segment after the IRAM prefix.
# The old flat 0x08000800 mapping shifted code by 0xb6d8 while absolute
# literals still referenced the real runtime addresses.
from ipod6g_stock_hibernate_audit import SEGMENTS, body_offset

BODY_SHA256 = "7910c276c85a0aa67c4f36fb2d72476791b7d7fe9a21442dae53bff27b48d684"
WRAPPED_SHA256 = "f4368251a58b2fdc7b46acf3178dae1d24bc1e029736240741015851256c65c4"
GEOMETRY_ADDRESS = 0x0815E19C


def verified_body(image: bytes) -> bytes:
    digest = hashlib.sha256(image).hexdigest()
    if digest == WRAPPED_SHA256:
        return image[0x800:]
    if digest == BODY_SHA256:
        return image
    raise ValueError(f"unexpected RetailOS image SHA-256: {digest}")

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


def make_emulator(body: bytes) -> Uc:
    emulator = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    body = verified_body(body)
    pages = set()
    for segment in SEGMENTS:
        for page in range(segment.address & ~0xfff,
                          align_up(segment.address + segment.size), 0x1000):
            if page not in pages:
                emulator.mem_map(page, 0x1000)
                pages.add(page)
        emulator.mem_write(segment.address,
                           body[segment.body_offset:segment.body_offset + segment.size])
    emulator.mem_map(VP_BASE, 0x1000)
    emulator.mem_map(STACK_BASE, STACK_SIZE)
    return emulator


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
    emulator = make_emulator(body)

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
            offset = body_offset(address)
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
    emulator.emu_start(GEOMETRY_ADDRESS, RETURN_PC, count=200000)
    if emulator.reg_read(UC_ARM_REG_PC) != RETURN_PC:
        raise RuntimeError(
            "stock geometry routine did not return within instruction budget")

    return {
        name: read_u32(emulator, VP_BASE + offset)
        for offset, name in VP_REGISTERS.items()
    }


def probe_format8(body: bytes, width: int, height: int) -> dict[str, int]:
    """Execute stock image and format-8 descriptor setters, without MMIO hardware."""
    emulator = make_emulator(body)
    emulator.mem_map(0x39200000, 0x1000)
    object_address = STACK_BASE + 0x100
    descriptor = STACK_BASE + 0x400
    # Stock descriptor slots 0, 2, 1 are the qualified Y, Cb, Cr order.
    y = 0x09000000
    cb = y + width * height
    cr = cb + width * height // 4
    emulator.mem_write(descriptor, struct.pack("<3I", y, cr, cb))

    def call(address, *arguments):
        emulator.reg_write(UC_ARM_REG_SP, STACK_BASE + STACK_SIZE // 2)
        emulator.reg_write(UC_ARM_REG_LR, RETURN_PC)
        for register, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1,
                                    UC_ARM_REG_R2, UC_ARM_REG_R3), arguments):
            emulator.reg_write(register, value)
        emulator.emu_start(address, RETURN_PC, count=200000)
        if emulator.reg_read(UC_ARM_REG_PC) != RETURN_PC:
            raise RuntimeError("stock setter exceeded instruction budget")
        if emulator.reg_read(UC_ARM_REG_R0) != 0:
            raise RuntimeError("stock setter rejected the configuration")

    call(0x0815EC2C, object_address, width, height, 8)
    call(0x0815E86C, object_address, 5, descriptor)
    registers = {0x3c: "image_w", 0x40: "image_h", 0x28: "y",
                 0x2c: "cb", 0x30: "cr", 0x34: "unused",
                 0x3c0: "plane_mode", 0x3c4: "y_stride", 0x3c8: "c_stride"}
    return {name: read_u32(emulator, VP_BASE + offset)
            for offset, name in registers.items()}


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
    parser.add_argument("--format8", action="store_true",
                        help="also execute image and planar descriptor setters")
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
    if args.format8:
        values = probe_format8(args.body.read_bytes(), source_width, source_height)
        print(" ".join(f"{name}={value}" for name, value in values.items()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
