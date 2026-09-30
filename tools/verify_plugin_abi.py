#!/usr/bin/env python3
"""Verify that Rockbox plugins match a firmware's runtime plugin ABI."""

from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path


HEADER = struct.Struct("<IHHIIIII")
# PLUGIN_MAGIC is the integer constant 0x526f634b in apps/plugin.h.  The
# target is little-endian, so its bytes in a .rock file appear as "KcoR".
ROCK_MAGIC = 0x526F634B


def firmware_pluginbuf(map_path: Path) -> int:
    pattern = re.compile(r"^\s*(0x[0-9a-fA-F]+)\s+(?:_?pluginbuf)\s*=\s*\.\s*$")
    for line in map_path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line)
        if match:
            return int(match.group(1), 16)
    raise ValueError(f"pluginbuf symbol not found in {map_path}")


def plugin_header(path: Path) -> tuple[int, int, int, int, int]:
    data = path.read_bytes()[: HEADER.size]
    if len(data) != HEADER.size:
        raise ValueError(f"header is truncated: {path}")
    magic, target, api, load_addr, end_addr, _entry, _api, _api_size = HEADER.unpack(data)
    return magic, target, api, load_addr, end_addr


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--firmware-map", required=True, type=Path)
    parser.add_argument("--target-id", required=True, type=int)
    parser.add_argument("--buffer-size", required=True, type=lambda value: int(value, 0))
    parser.add_argument("plugins", nargs="+", type=Path)
    args = parser.parse_args()

    try:
        load_expected = firmware_pluginbuf(args.firmware_map)
    except (OSError, ValueError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1

    buffer_end = load_expected + args.buffer_size
    failed = False
    for plugin in args.plugins:
        try:
            magic, target, api, load_addr, end_addr = plugin_header(plugin)
        except (OSError, ValueError) as exc:
            print(f"FAIL {plugin}: {exc}", file=sys.stderr)
            failed = True
            continue

        errors: list[str] = []
        if magic != ROCK_MAGIC:
            errors.append(f"magic=0x{magic:08x}")
        if target != args.target_id:
            errors.append(f"target={target}, expected={args.target_id}")
        if load_addr != load_expected:
            errors.append(f"load=0x{load_addr:08x}, expected=0x{load_expected:08x}")
        if end_addr > buffer_end:
            errors.append(f"end=0x{end_addr:08x}, limit=0x{buffer_end:08x}")
        if end_addr < load_addr:
            errors.append(f"end=0x{end_addr:08x} before load address")

        if errors:
            print(f"FAIL {plugin}: " + "; ".join(errors), file=sys.stderr)
            failed = True
        else:
            used = end_addr - load_addr
            print(
                f"PASS {plugin}: target={target} api={api} "
                f"load=0x{load_addr:08x} end=0x{end_addr:08x} "
                f"used=0x{used:x}/0x{args.buffer_size:x}"
            )

    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
