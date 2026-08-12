#!/usr/bin/env python3
"""Provision non-secret iPhone Trust identities for RockPod tether preflight."""

from __future__ import annotations

import argparse
import os
import plistlib
import re
from pathlib import Path


SAFE_ID = re.compile(r"^[A-Za-z0-9-]{8,64}$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ipod-root", required=True, type=Path)
    parser.add_argument(
        "--lockdown-dir", type=Path, default=Path("/var/lib/lockdown")
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    rockbox = args.ipod_root / ".rockbox"
    if not rockbox.is_dir():
        raise SystemExit(f"not a mounted Rockbox volume: {args.ipod_root}")
    if not args.lockdown_dir.is_dir():
        raise SystemExit(f"pairing directory not found: {args.lockdown_dir}")

    destination = rockbox / "iphone-pair"
    destination.mkdir(exist_ok=True)
    installed = 0
    for source in sorted(args.lockdown_dir.glob("*.plist")):
        udid = source.stem
        if not SAFE_ID.fullmatch(udid):
            continue
        try:
            with source.open("rb") as handle:
                record = plistlib.load(handle)
        except (OSError, plistlib.InvalidFileException):
            continue
        host_id = record.get("HostID")
        system_buid = record.get("SystemBUID")
        if not isinstance(host_id, str) or not SAFE_ID.fullmatch(host_id):
            continue
        if not isinstance(system_buid, str) or not SAFE_ID.fullmatch(system_buid):
            continue
        target = destination / f"{udid}.cfg"
        temporary = destination / f".{udid}.cfg.tmp"
        temporary.write_text(
            f"host_id={host_id}\nsystem_buid={system_buid}\n",
            encoding="ascii",
        )
        os.replace(temporary, target)
        installed += 1

    if not installed:
        raise SystemExit("no usable iPhone pairing identities were found")
    print(f"iPhone tether: provisioned {installed} trusted host identities")
    print("iPhone tether: no certificates or private keys were copied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
