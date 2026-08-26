#!/usr/bin/env python3
"""Find direct ARM branches and absolute words referencing stock OSOS targets.

The decrypted iPod Classic body mixes executable code, vtables, and data, so
ordinary raw-binary objdump output cannot recover virtual-call relationships.
This helper reports both direct branch targets and little-endian absolute
function pointers.  Addresses are virtual addresses based at 0x08000800.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


ANALYSIS_MODULES = Path("/tmp/ipod-vp-analysis")
if ANALYSIS_MODULES.is_dir():
    sys.path.insert(0, str(ANALYSIS_MODULES))

from capstone import CS_ARCH_ARM, CS_MODE_ARM, Cs  # noqa: E402
from capstone.arm import ARM_OP_IMM  # noqa: E402


IMAGE_BASE = 0x08000800


def parse_address(value: str) -> int:
    return int(value, 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("body", type=Path)
    parser.add_argument("target", nargs="+", type=parse_address)
    parser.add_argument("--context-words", type=int, default=4)
    args = parser.parse_args()

    body = args.body.read_bytes()
    targets = set(args.target)
    disassembler = Cs(CS_ARCH_ARM, CS_MODE_ARM)
    disassembler.detail = True

    for target in sorted(targets):
        print(f"target 0x{target:08x}")
        packed = struct.pack("<I", target)
        start = 0
        while True:
            offset = body.find(packed, start)
            if offset < 0:
                break
            address = IMAGE_BASE + offset
            first = max(0, offset - args.context_words * 4)
            last = min(len(body), offset + (args.context_words + 1) * 4)
            words = [
                struct.unpack_from("<I", body, word_offset)[0]
                for word_offset in range(first, last, 4)
            ]
            print(
                f"  absolute word at body+0x{offset:06x} "
                f"(0x{address:08x}); context "
                + " ".join(f"{word:08x}" for word in words)
            )
            start = offset + 1

    branches: dict[int, list[tuple[int, str, str]]] = {
        target: [] for target in targets
    }
    for offset in range(0, len(body) - 3, 4):
        address = IMAGE_BASE + offset
        instruction = next(
            disassembler.disasm(body[offset : offset + 4], address, 1),
            None,
        )
        if instruction is None or instruction.mnemonic not in {"b", "bl", "blx"}:
            continue
        if not instruction.operands or instruction.operands[0].type != ARM_OP_IMM:
            continue
        target = instruction.operands[0].imm & 0xFFFFFFFF
        if target in branches:
            branches[target].append((offset, instruction.mnemonic, instruction.op_str))

    for target in sorted(targets):
        for offset, mnemonic, operands in branches[target]:
            print(
                f"target 0x{target:08x}: {mnemonic} at body+0x{offset:06x} "
                f"(0x{IMAGE_BASE + offset:08x}): {operands}"
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
