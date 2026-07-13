#!/usr/bin/env python3
"""Prepare and run an isolated InfoNES simulator frame gate."""

from __future__ import annotations

import argparse
import os
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

PLUGIN_PATH = "/.rockbox/rocks/viewers/infones.rock"


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


def prepare(build_dir: Path, rom: Path, sound: bool) -> tuple[Path, Path]:
    simdisk = build_dir / "simdisk"
    plugin_source = build_dir / "apps" / "plugins" / "infones" / "infones.rock"
    plugin_target = simdisk / PLUGIN_PATH.lstrip("/")
    rom_target = simdisk / ".rockbox" / "roms" / "nes" / rom.name
    if not plugin_source.is_file():
        raise SystemExit(f"missing simulator plugin: {plugin_source}")
    if not rom.is_file():
        raise SystemExit(f"missing test ROM: {rom}")
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    rom_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin_source, plugin_target)
    shutil.copy2(rom, rom_target)

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
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, "infones.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH)
    rom_arg = "/" + rom_target.relative_to(simdisk).as_posix()
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, rom_arg)
    plugin_dat = simdisk / ".rockbox" / "rocks" / "plugin.dat"
    plugin_dat.parent.mkdir(parents=True, exist_ok=True)
    plugin_dat.write_bytes(entry)

    for config_path in (simdisk / ".rockbox" / "config.cfg", simdisk / "config.cfg"):
        lines = config_path.read_text(
            encoding="utf-8", errors="replace"
        ).splitlines() if config_path.exists() else []
        lines = [
            line for line in lines
            if not line.startswith(("start in screen:", "openplugin:"))
        ]
        lines.append("start in screen: plugin")
        config_path.parent.mkdir(parents=True, exist_ok=True)
        config_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    options = simdisk / ".rockbox" / "infones" / "options.cfg"
    options.parent.mkdir(parents=True, exist_ok=True)
    options.write_text(
        f"sound={1 if sound else 0}\nautosave=0\naudio_quality=2\n",
        encoding="utf-8",
    )
    return simdisk, simdisk / ".rockbox" / "infones" / "profile.log"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--sound", action="store_true")
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    simdisk, profile_path = prepare(
        build_dir, args.rom.expanduser().resolve(), args.sound
    )
    print(f"Prepared InfoNES simulator disk: {simdisk}")
    if not args.run:
        return 0

    profile_path.unlink(missing_ok=True)
    environment = os.environ.copy()
    environment["INFONES_TEST_FRAMES"] = str(max(1, args.frames))
    process = subprocess.Popen(
        [str(build_dir / "rockboxui")], cwd=build_dir, env=environment
    )
    deadline = time.monotonic() + 90
    while process.poll() is None and time.monotonic() < deadline:
        time.sleep(0.1)
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()

    if not profile_path.is_file():
        return process.returncode or 1
    lines = profile_path.read_text(
        encoding="utf-8", errors="replace"
    ).splitlines()
    if not lines:
        return 1
    print(lines[-1])
    expected = f"frames_emulated={max(1, args.frames)}"
    return 0 if expected in lines[-1] and "frames_skipped=0" in lines[-1] else 1


if __name__ == "__main__":
    raise SystemExit(main())
