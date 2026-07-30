#!/usr/bin/env python3
"""Prepare and run an isolated CPS1 simulator ROM/frame smoke gate."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import struct
import subprocess
import time
from pathlib import Path

from rockboy_profile_gate import (
    LANG_START_SCREEN,
    OPEN_PLUGIN_CHECKSUM,
    OPEN_PLUGIN_ENTRY_SIZE,
    OPEN_PLUGIN_NAME_OFFSET,
    OPEN_PLUGIN_NAME_SIZE,
    OPEN_PLUGIN_PARAM_OFFSET,
    OPEN_PLUGIN_PARAM_SIZE,
    OPEN_PLUGIN_PATH_OFFSET,
    OPEN_PLUGIN_PATH_SIZE,
    START_SCREEN_HASH,
    open_plugin_lang_checksum,
)

PLUGIN_PATH = "/.rockbox/rocks/games/cps1.rock"


def write_cstring(buffer: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")[: size - 1]
    buffer[offset : offset + len(encoded)] = encoded


def start_screen_lang_id(build_dir: Path) -> int:
    lang_enum = build_dir / "lang_enum.h"
    if lang_enum.is_file():
        for line in lang_enum.read_text(
            encoding="utf-8", errors="replace"
        ).splitlines():
            if "LANG_START_SCREEN," in line and "/*" in line:
                return int(line.split("/*", 1)[1].split("*/", 1)[0].strip())
    return LANG_START_SCREEN


def prepare(build_dir: Path, rom: Path) -> tuple[Path, Path]:
    simdisk = build_dir / "simdisk"
    plugin_source = build_dir / "apps/plugins/cps1/cps1.rock"
    plugin_target = simdisk / PLUGIN_PATH.lstrip("/")
    rom_target = simdisk / ".rockbox/games/cps1/roms" / rom.name
    chips_source = rom.with_suffix("")
    chips_target = rom_target.with_suffix("")

    if not plugin_source.is_file():
        raise SystemExit(f"missing simulator plugin: {plugin_source}")
    if not rom.is_file():
        raise SystemExit(f"missing CPS1 ROM archive: {rom}")

    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    rom_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin_source, plugin_target)
    shutil.copy2(rom, rom_target)
    if chips_source.is_dir():
        shutil.copytree(chips_source, chips_target, dirs_exist_ok=True)

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build_dir)
    struct.pack_into(
        "<IiI",
        entry,
        0,
        START_SCREEN_HASH,
        start_screen_lang_id(build_dir),
        checksum or OPEN_PLUGIN_CHECKSUM,
    )
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE,
                  "cps1.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE,
                  PLUGIN_PATH)
    rom_arg = "/" + rom_target.relative_to(simdisk).as_posix()
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE,
                  rom_arg)
    plugin_dat = simdisk / ".rockbox/rocks/plugin.dat"
    plugin_dat.parent.mkdir(parents=True, exist_ok=True)
    plugin_dat.write_bytes(entry)

    for config_path in (simdisk / ".rockbox/config.cfg",
                        simdisk / "config.cfg"):
        lines = (
            config_path.read_text(
                encoding="utf-8", errors="replace"
            ).splitlines()
            if config_path.exists()
            else []
        )
        lines = [
            line for line in lines
            if not line.startswith(("start in screen:", "openplugin:"))
        ]
        lines.append("start in screen: plugin")
        config_path.parent.mkdir(parents=True, exist_ok=True)
        config_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    return simdisk, simdisk / ".rockbox/logs/cps1.log"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path,
                        default=Path("build-sim-ipod6g"))
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=120)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    _, log_path = prepare(build_dir, args.rom.expanduser().resolve())
    print(f"Prepared CPS1 simulator ROM: {args.rom.name}")
    if not args.run:
        return 0

    log_path.unlink(missing_ok=True)
    environment = os.environ.copy()
    environment["CPS1_TEST_FRAMES"] = str(max(1, args.frames))
    environment["CPS1_TEST_UNTHROTTLED"] = "1"
    environment["CPS1_TEST_AUTOSTART"] = "1"
    process = subprocess.Popen(
        [str(build_dir / "rockboxui")],
        cwd=build_dir,
        env=environment,
    )
    deadline = time.monotonic() + 120
    while process.poll() is None and time.monotonic() < deadline:
        if log_path.exists() and "exit status=" in log_path.read_text(
            encoding="utf-8", errors="replace"
        ):
            process.terminate()
            break
        time.sleep(0.1)
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()

    if not log_path.is_file():
        return process.returncode or 1
    log = log_path.read_text(encoding="utf-8", errors="replace")
    print(log, end="")
    result = re.search(
        r"exit status=0 initialized=1 frames=(\d+) "
        r"frame_crc=([0-9a-fA-F]{8}) video_nonzero=(\d+) "
        r"audio_nonzero=(\d+) allocation_failed=0",
        log,
    )
    return 0 if (
        result is not None
        and int(result.group(1)) == max(1, args.frames)
        and int(result.group(2), 16) != 0
        and int(result.group(3)) > 0
        and int(result.group(4)) > 0
    ) else 1


if __name__ == "__main__":
    raise SystemExit(main())
