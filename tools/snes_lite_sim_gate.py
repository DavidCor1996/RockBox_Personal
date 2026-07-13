#!/usr/bin/env python3
"""Prepare and optionally run an isolated SNES Lite simulator smoke test."""

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

PLUGIN_PATH = "/.rockbox/rocks/games/snes_lite.rock"


def write_cstring(buffer: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")[: size - 1]
    buffer[offset : offset + len(encoded)] = encoded


def start_screen_lang_id(build_dir: Path) -> int:
    lang_enum = build_dir / "lang_enum.h"
    if lang_enum.is_file():
        for line in lang_enum.read_text(encoding="utf-8", errors="replace").splitlines():
            if "LANG_START_SCREEN," in line and "/*" in line:
                return int(line.split("/*", 1)[1].split("*/", 1)[0].strip())
    return LANG_START_SCREEN


def prepare(build_dir: Path, rom: Path) -> Path:
    simdisk = build_dir / "simdisk"
    plugin_source = build_dir / "apps" / "plugins" / "snes_lite" / "snes_lite.rock"
    plugin_target = simdisk / PLUGIN_PATH.lstrip("/")
    rom_target = simdisk / ".rockbox" / "roms" / "snes" / rom.name
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
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, "snes_lite.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH)
    rom_arg = "/" + rom_target.relative_to(simdisk).as_posix()
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, rom_arg)
    plugin_dat = simdisk / ".rockbox" / "rocks" / "plugin.dat"
    plugin_dat.parent.mkdir(parents=True, exist_ok=True)
    plugin_dat.write_bytes(entry)

    for config_path in (simdisk / ".rockbox" / "config.cfg", simdisk / "config.cfg"):
        lines = config_path.read_text(encoding="utf-8", errors="replace").splitlines() if config_path.exists() else []
        lines = [line for line in lines if not line.startswith(("start in screen:", "openplugin:"))]
        lines.append("start in screen: plugin")
        config_path.parent.mkdir(parents=True, exist_ok=True)
        config_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return simdisk


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--frames", type=int, default=180)
    parser.add_argument("--preset", type=int, choices=range(4), default=0)
    parser.add_argument("--video", type=int, choices=range(2))
    parser.add_argument("--audio", choices=("off", "auto", "on", "low"), default="off")
    args = parser.parse_args()
    build_dir = args.build_dir.resolve()
    simdisk = prepare(build_dir, args.rom.expanduser().resolve())
    snes_config = simdisk / ".rockbox" / "config" / "snes_lite.cfg"
    snes_config.parent.mkdir(parents=True, exist_ok=True)
    video_config = (
        f"video_mode={args.video}\n" if args.video is not None else ""
    )
    config_text = (
        "frameskip=auto\n"
        f"audio={args.audio}\n"
        "input_profile=0\n"
        "show_fps=0\n"
        f"performance_mode={1 if args.preset != 3 else 0}\n"
        f"{video_config}"
        f"performance_preset={args.preset}\n"
    )
    snes_config.write_text(config_text, encoding="utf-8")
    per_game_config = snes_config.parent / "snes_lite" / f"{args.rom.stem}.cfg"
    per_game_config.parent.mkdir(parents=True, exist_ok=True)
    per_game_config.write_text(config_text, encoding="utf-8")
    print(f"Prepared SNES Lite simulator disk: {simdisk}")
    if not args.run:
        return 0
    environment = os.environ.copy()
    environment["SNES_LITE_TEST_FRAMES"] = str(max(1, args.frames))
    log_path = simdisk / ".rockbox" / "logs" / "snes_lite.log"
    log_path.unlink(missing_ok=True)
    process = subprocess.Popen([str(build_dir / "rockboxui")], cwd=build_dir, env=environment)
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
