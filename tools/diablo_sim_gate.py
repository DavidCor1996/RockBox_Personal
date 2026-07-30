#!/usr/bin/env python3
"""Boot the simulator straight into the diablo.rock plugin.

Same trick as tools/openlara_sim_gate.py: rockboxui has no CLI flag to
launch a plugin directly, so this writes the plugin-browser "last opened"
resume state (.rockbox/rocks/plugin.dat) plus "start in screen: plugin"
in config.cfg, so the sim boots directly to the title-screen milestone
instead of requiring manual menu navigation.
"""
from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
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

PLUGIN_PATH = "/.rockbox/rocks/games/diablo.rock"


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


def prepare(build_dir: Path) -> Path:
    simdisk = build_dir / "simdisk"
    plugin_source = build_dir / "apps/plugins/diablo.rock"
    plugin_target = simdisk / PLUGIN_PATH.lstrip("/")

    if not plugin_source.is_file():
        raise SystemExit(f"missing simulator plugin: {plugin_source}")

    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin_source, plugin_target)

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
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, "diablo.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, "")
    plugin_dat = simdisk / ".rockbox/rocks/plugin.dat"
    plugin_dat.parent.mkdir(parents=True, exist_ok=True)
    plugin_dat.write_bytes(entry)

    for config_path in (simdisk / ".rockbox/config.cfg", simdisk / "config.cfg"):
        lines = (
            config_path.read_text(encoding="utf-8", errors="replace").splitlines()
            if config_path.exists()
            else []
        )
        lines = [
            line
            for line in lines
            if not line.startswith(("start in screen:", "openplugin:"))
        ]
        lines.append("start in screen: plugin")
        config_path.parent.mkdir(parents=True, exist_ok=True)
        config_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    return simdisk


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    simdisk = prepare(build_dir)
    print(f"Prepared diablo simulator disk: {simdisk}")
    if not args.run:
        return 0

    process = subprocess.Popen([str(build_dir / "rockboxui")], cwd=build_dir)
    process.wait()
    return process.returncode or 0


if __name__ == "__main__":
    raise SystemExit(main())
