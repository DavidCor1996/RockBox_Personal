#!/usr/bin/env python3
"""Prepare and optionally run a personal PicoDrive simulator smoke test."""

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

PLUGIN_PATH = "/.rockbox/rocks/games/picodrive.rock"


def write_cstring(buffer: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")
    if len(encoded) >= size:
        raise SystemExit(f"plugin start field is too long: {value}")
    buffer[offset:offset + len(encoded)] = encoded


def start_screen_lang_id(build_dir: Path) -> int:
    lang_enum = build_dir / "lang_enum.h"
    if lang_enum.is_file():
        for line in lang_enum.read_text(
            encoding="utf-8", errors="replace"
        ).splitlines():
            if "LANG_START_SCREEN," in line and "/*" in line:
                return int(line.split("/*", 1)[1].split("*/", 1)[0].strip())
    return LANG_START_SCREEN


def inspect_rom(rom: Path) -> None:
    data = rom.read_bytes()
    if not 0x200 <= len(data) <= 10 * 1024 * 1024:
        raise SystemExit("test ROM is outside PicoDrive's 512 B-10 MiB limit")
    if data[0x100:0x104] != b"SEGA":
        raise SystemExit("test ROM has no Sega Genesis header")
    reset = int.from_bytes(data[4:8], "big")
    if reset < 0x100 or reset >= len(data) or reset & 1:
        raise SystemExit("test ROM has an invalid reset vector")


def prepare(build_dir: Path, rom: Path) -> tuple[Path, Path]:
    inspect_rom(rom)
    simdisk = build_dir / "simdisk"
    plugin_source = (
        build_dir / "apps" / "plugins" / "picodrive" / "picodrive.rock"
    )
    plugin_target = simdisk / PLUGIN_PATH.lstrip("/")
    rom_target = (
        simdisk / ".rockbox" / "games" / "genesis" / "roms" / rom.name
    )
    if not plugin_source.is_file():
        raise SystemExit(f"missing simulator plugin: {plugin_source}")
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    rom_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin_source, plugin_target)
    shutil.copy2(rom, rom_target)

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build_dir)
    struct.pack_into(
        "<IiI", entry, 0, START_SCREEN_HASH,
        start_screen_lang_id(build_dir), checksum or OPEN_PLUGIN_CHECKSUM,
    )
    write_cstring(
        entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, "picodrive.rock"
    )
    write_cstring(
        entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH
    )
    rom_arg = "/" + rom_target.relative_to(simdisk).as_posix()
    write_cstring(
        entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, rom_arg
    )
    plugin_dat = simdisk / ".rockbox" / "rocks" / "plugin.dat"
    plugin_dat.parent.mkdir(parents=True, exist_ok=True)
    plugin_dat.write_bytes(entry)

    for config_path in (
        simdisk / ".rockbox" / "config.cfg", simdisk / "config.cfg"
    ):
        lines = config_path.read_text(
            encoding="utf-8", errors="replace"
        ).splitlines() if config_path.exists() else []
        lines = [
            line for line in lines
            if not line.startswith(("start in screen:", "openplugin:"))
        ]
        lines.append("start in screen: plugin")
        lines.append(
            'openplugin: "Start Screen", "picodrive.rock", '
            f'"{PLUGIN_PATH}", "{rom_arg}"'
        )
        config_path.parent.mkdir(parents=True, exist_ok=True)
        config_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return simdisk, rom_target


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir", type=Path, default=Path("build-sim-ipod6g")
    )
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--frames", type=int, default=600)
    args = parser.parse_args()
    build_dir = args.build_dir.resolve()
    rom = args.rom.expanduser().resolve()
    simdisk, rom_target = prepare(build_dir, rom)
    print(f"Prepared PicoDrive simulator ROM: {rom_target}")
    if not args.run:
        return 0

    environment = os.environ.copy()
    environment["PICODRIVE_TEST_FRAMES"] = str(max(1, args.frames))
    environment["PICODRIVE_TEST_UNTHROTTLED"] = "1"
    environment["RBROOT"] = str(simdisk)
    environment["ROCKBOX_SIM_PLUGIN"] = PLUGIN_PATH
    environment["ROCKBOX_SIM_PLUGIN_PARAM"] = (
        "/" + rom_target.relative_to(simdisk).as_posix()
    )
    environment.setdefault("SDL_VIDEODRIVER", "dummy")
    environment.setdefault("SDL_AUDIODRIVER", "dummy")
    log_path = simdisk / ".rockbox" / "games" / "genesis" / "picodrive.log"
    log_path.unlink(missing_ok=True)
    process = subprocess.Popen(
        [
            str(build_dir / "rockboxui"), "--nobackground", "--root",
            str(simdisk), "--zoom", "1",
        ],
        cwd=build_dir,
        env=environment,
    )
    deadline = time.monotonic() + 90
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
    if log_path.exists():
        log_text = log_path.read_text(encoding="utf-8", errors="replace")
        print(log_text)
        return 0 if "exit status=0" in log_text else 1
    return process.returncode or 1


if __name__ == "__main__":
    raise SystemExit(main())
