#!/usr/bin/env python3
"""Find direct ARM branches and absolute words referencing stock OSOS targets.

The decrypted iPod Classic body mixes executable code, vtables, and data, so
ordinary raw-binary objdump output cannot recover virtual-call relationships.
This helper reports both direct branch targets and little-endian absolute
function pointers.

The 2.0.4 OSOS payload is not a flat image.  Its startup relocator copies the
first 0xaed8 bytes to IRAM, the next 0xa0fc88 bytes to DRAM at 0x08000000, and
the final initialized-data block to 0x08a0fc88.  Decoding the raw file as if it
were based at 0x08000800 shifts every DRAM address by 0xb6d8 and can turn data
or an unrelated routine into a convincing-looking false cross-reference.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import struct
from pathlib import Path


@dataclass(frozen=True)
class Segment:
    raw_offset: int
    address: int
    size: int


STOCK_2_0_4_SEGMENTS = (
    Segment(0x000000, 0x22000000, 0x00AED8),
    Segment(0x00AED8, 0x08000000, 0xA0FC88),
    Segment(0xA1AB60, 0x08A0FC88, 0x000A84),
)


def raw_address(segments: tuple[Segment, ...], offset: int) -> int | None:
    for segment in segments:
        relative = offset - segment.raw_offset
        if 0 <= relative < segment.size:
            return segment.address + relative
    return None


def parse_address(value: str) -> int:
    return int(value, 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("body", type=Path)
    parser.add_argument("target", nargs="+", type=parse_address)
    parser.add_argument("--context-words", type=int, default=4)
    parser.add_argument(
        "--flat-base",
        type=parse_address,
        help="treat the complete body as one flat image at this address",
    )
    args = parser.parse_args()

    body = args.body.read_bytes()
    segments = (
        (Segment(0, args.flat_base, len(body)),)
        if args.flat_base is not None
        else STOCK_2_0_4_SEGMENTS
    )
    if args.flat_base is None:
        required = max(s.raw_offset + s.size for s in segments)
        if len(body) < required:
            parser.error(
                f"body is 0x{len(body):x} bytes; stock layout needs "
                f"at least 0x{required:x}"
            )

    targets = set(args.target)
    for target in sorted(targets):
        print(f"target 0x{target:08x}")
        packed = struct.pack("<I", target)
        start = 0
        while True:
            offset = body.find(packed, start)
            if offset < 0:
                break
            address = raw_address(segments, offset)
            first = max(0, offset - args.context_words * 4)
            last = min(len(body), offset + (args.context_words + 1) * 4)
            words = [
                struct.unpack_from("<I", body, word_offset)[0]
                for word_offset in range(first, last, 4)
            ]
            location = (
                f"0x{address:08x}" if address is not None else "unmapped"
            )
            print(
                f"  absolute word at body+0x{offset:06x} ({location}); "
                "context " + " ".join(f"{word:08x}" for word in words)
            )
            start = offset + 1

    branches: dict[int, list[tuple[int, str, str]]] = {
        target: [] for target in targets
    }
    pc_relative: dict[int, list[tuple[int, str]]] = {
        target: [] for target in targets
    }
    for segment in segments:
        stop = min(len(body), segment.raw_offset + segment.size)
        for offset in range(segment.raw_offset, stop - 3, 4):
            address = segment.address + offset - segment.raw_offset
            word = struct.unpack_from("<I", body, offset)[0]

            # ARM-state ADR is an ADD/SUB-immediate using PC as the base.
            # Stripped RetailOS code uses it for nearby task/service names,
            # so absolute-word and branch scans alone miss those references.
            if (word & 0x0E000000) == 0x02000000:
                opcode = (word >> 21) & 0xF
                rn = (word >> 16) & 0xF
                if rn == 15 and opcode in (0x2, 0x4):
                    imm = word & 0xFF
                    rotate = ((word >> 8) & 0xF) * 2
                    if rotate:
                        imm = ((imm >> rotate) |
                               (imm << (32 - rotate))) & 0xFFFFFFFF
                    base = (address + 8) & 0xFFFFFFFF
                    referenced = ((base - imm) if opcode == 0x2 else
                                  (base + imm)) & 0xFFFFFFFF
                    if referenced in pc_relative:
                        operation = "sub" if opcode == 0x2 else "add"
                        pc_relative[referenced].append(
                            (offset, f"{operation} ..., pc, #0x{imm:x}"))

            # Decode ARM-state B/BL/BLX-immediate directly.  Calling Capstone
            # once per 32-bit word made a full Classic OSOS scan take several
            # minutes; these encodings are fixed-width.
            condition = word >> 28
            if condition != 0xF and (word & 0x0E000000) == 0x0A000000:
                mnemonic = "bl" if word & 0x01000000 else "b"
                displacement = (word & 0x00FFFFFF) << 2
                if displacement & 0x02000000:
                    displacement -= 0x04000000
            elif (word & 0xFE000000) == 0xFA000000:
                mnemonic = "blx"
                displacement = ((word & 0x00FFFFFF) << 2) | (
                    (word >> 23) & 0x2
                )
                if displacement & 0x02000000:
                    displacement -= 0x04000000
            else:
                continue

            target = (address + 8 + displacement) & 0xFFFFFFFF
            if target in branches:
                branches[target].append((offset, mnemonic, f"#0x{target:x}"))

    for target in sorted(targets):
        for offset, instruction in pc_relative[target]:
            address = raw_address(segments, offset)
            assert address is not None
            print(
                f"target 0x{target:08x}: ADR at body+0x{offset:06x} "
                f"(0x{address:08x}): {instruction}"
            )
        for offset, mnemonic, operands in branches[target]:
            address = raw_address(segments, offset)
            assert address is not None
            print(
                f"target 0x{target:08x}: {mnemonic} at body+0x{offset:06x} "
                f"(0x{address:08x}): {operands}"
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
