#!/usr/bin/env python3
"""Generate SM64 build assets from an exact, user-owned US v1.0 ROM."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
from pathlib import Path

ROM_SHA1 = "9bef1128717f958171a4afac3ed78ee2bb4e86ce"
REQUIRED = (
    "build/us_pc/assets/mario_anim_data.c",
    "build/us_pc/assets/demo_data.c",
    "build/us_pc/include/level_headers.h",
    "build/us_pc/include/text_menu_strings.h",
    "build/us_pc/include/text_strings.h",
    "build/us_pc/sound/sound_data.ctl.inc.c",
    "build/us_pc/sound/sound_data.tbl.inc.c",
    "build/us_pc/sound/sequences.bin.inc.c",
    "build/us_pc/sound/bank_sets.inc.c",
    "build/us_rockbox32/sound32/sound_data.ctl.inc.c",
    "build/us_rockbox32/sound32/sound_data.tbl.inc.c",
    "build/us_rockbox32/sound32/sequences.bin.inc.c",
    "build/us_rockbox32/sound32/bank_sets.inc.c",
)


def sha1(path: Path) -> str:
    digest = hashlib.sha1()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "rom",
        nargs="?",
        type=Path,
        default=Path("/home/david/Downloads/Super Mario 64 (USA).z64"),
    )
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    upstream = root / "apps/plugins/sm64/upstream"
    rom = args.rom.expanduser().resolve()
    baserom = upstream / "baserom.us.z64"

    if not rom.is_file() or rom.stat().st_size != 8 * 1024 * 1024:
        raise SystemExit("expected an 8 MiB Super Mario 64 US ROM")
    actual = sha1(rom)
    if actual != ROM_SHA1:
        raise SystemExit(f"wrong ROM revision: expected {ROM_SHA1}, got {actual}")

    shutil.copy2(rom, baserom)
    try:
        subprocess.run(
            [
                "make", "-C", str(upstream),
                "VERSION=us", "TARGET_DOS=0", "ENABLE_SOFTRAST=1",
                f"-j{max(1, args.jobs)}",
            ],
            check=True,
        )
        subprocess.run(
            [str(root / "tools/sm64_prepare_audio32.py")],
            check=True,
        )
    finally:
        baserom.unlink(missing_ok=True)

    missing = [path for path in REQUIRED if not (upstream / path).is_file()]
    if missing:
        raise SystemExit(f"asset preparation incomplete: {', '.join(missing)}")
    print("PASS: exact US v1.0 ROM verified and local SM64 assets generated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
