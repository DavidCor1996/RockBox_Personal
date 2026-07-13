#!/usr/bin/env python3
"""Extract the 2048-byte ISO payload from a raw Mode-2/2352 PS1 track.

This never modifies the source image.  It is intentionally small so Tomb Raider
owners can inspect their own disc with ordinary ISO-9660 tools before running the
OpenLara asset converter.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


RAW_SECTOR = 2352
ISO_SECTOR = 2048
SYNC = bytes.fromhex("00ffffffffffffffffffff00")


def extract(source: Path, destination: Path) -> tuple[int, int]:
    size = source.stat().st_size
    if size % RAW_SECTOR:
        raise ValueError(f"{source}: size is not a multiple of {RAW_SECTOR}")

    sectors = size // RAW_SECTOR
    form2 = 0
    with source.open("rb") as src, destination.open("wb") as dst:
        for index in range(sectors):
            sector = src.read(RAW_SECTOR)
            if len(sector) != RAW_SECTOR:
                raise OSError(f"short read at sector {index}")
            if sector[:12] != SYNC:
                raise ValueError(f"invalid CD-ROM sync at sector {index}")
            if sector[15] != 2:
                raise ValueError(
                    f"sector {index} is mode {sector[15]}, expected Mode 2"
                )
            if sector[16:20] != sector[20:24]:
                raise ValueError(f"duplicate XA subheaders differ at sector {index}")
            if sector[18] & 0x20:
                form2 += 1
            # ISO-9660 addresses 2048-byte logical blocks.  Form-2 sectors have
            # additional payload after this range, but filesystem metadata and
            # ordinary game files remain addressable through these first bytes.
            dst.write(sector[24 : 24 + ISO_SECTOR])
    return sectors, form2


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="raw MODE2/2352 Track 01 BIN")
    parser.add_argument("destination", type=Path, help="output 2048-byte-sector ISO")
    args = parser.parse_args()

    try:
        sectors, form2 = extract(args.source, args.destination)
    except (OSError, ValueError) as exc:
        print(f"openlara_extract_psx: {exc}", file=sys.stderr)
        return 1

    print(
        f"extracted {sectors} sectors to {args.destination} "
        f"({form2} XA Form-2 sectors observed)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
