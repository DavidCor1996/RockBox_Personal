#!/usr/bin/env python3
"""Create OpeniBoot's canonical flat A4 image from a little-endian ARM ELF."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct


ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIIIIIII")
PT_LOAD = 1
ET_EXEC = 2
EM_ARM = 40
MAX_IMAGE_BYTES = 1024 * 1024


class FlattenError(RuntimeError):
    """Raised when an ELF cannot be deterministically flattened."""


def flatten(elf: bytes) -> bytes:
    if len(elf) < ELF_HEADER.size:
        raise FlattenError("ELF header is truncated")
    header = ELF_HEADER.unpack_from(elf)
    ident = header[0]
    if ident[:4] != b"\x7fELF" or ident[4:7] != bytes((1, 1, 1)):
        raise FlattenError("expected ELF32 little-endian version 1")
    elf_type, machine = header[1:3]
    entry, phoff = header[4:6]
    ehsize, phentsize, phnum = header[8:11]
    if elf_type != ET_EXEC or machine != EM_ARM:
        raise FlattenError("expected an executable ARM ELF")
    if ehsize != ELF_HEADER.size or phentsize != PROGRAM_HEADER.size or not phnum:
        raise FlattenError("unexpected ELF or program-header layout")
    if phoff + phnum * phentsize > len(elf):
        raise FlattenError("program-header table is truncated")

    segments: list[tuple[int, int, int, int]] = []
    image_size = 0
    for index in range(phnum):
        values = PROGRAM_HEADER.unpack_from(elf, phoff + index * phentsize)
        kind, offset, vaddr, _paddr, file_size, memory_size = values[:6]
        if kind != PT_LOAD:
            raise FlattenError("OpeniBoot image has a non-load program header")
        if file_size > memory_size or offset + file_size > len(elf):
            raise FlattenError("load segment exceeds its ELF bounds")
        if vaddr < entry:
            raise FlattenError("load segment precedes the ELF entry point")
        destination = vaddr - entry
        extent = destination + memory_size
        if extent > MAX_IMAGE_BYTES:
            raise FlattenError("flat OpeniBoot image exceeds the 1 MiB gate")
        image_size = max(image_size, extent)
        segments.append((destination, destination + file_size, offset, file_size))

    file_ranges = sorted((start, end) for start, end, _offset, _size in segments)
    for previous, current in zip(file_ranges, file_ranges[1:]):
        if previous[1] > current[0]:
            raise FlattenError("loadable file ranges overlap")

    image = bytearray(image_size)
    for destination, _end, offset, file_size in segments:
        image[destination:destination + file_size] = elf[offset:offset + file_size]
    return bytes(image)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        image = flatten(args.elf.read_bytes())
        args.output.write_bytes(image)
    except (OSError, FlattenError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

