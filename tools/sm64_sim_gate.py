#!/usr/bin/env python3
"""Prepare and run an isolated Super Mario 64 simulator smoke test."""

from __future__ import annotations

import argparse
import shutil
import struct
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

PLUGIN_PATH = "/.rockbox/rocks/games/sm64.rock"
ROM_SHA1 = "9bef1128717f958171a4afac3ed78ee2bb4e86ce"


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


def sha1(path: Path) -> str:
    import hashlib

    digest = hashlib.sha1()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def prepare(build_dir: Path, rom: Path) -> tuple[Path, Path, Path]:
    simdisk = build_dir / "simdisk"
    plugin_source = build_dir / "apps/plugins/sm64/sm64.rock"
    plugin_target = simdisk / PLUGIN_PATH.lstrip("/")
    rom_target = simdisk / ".rockbox/games/n64/Super Mario 64 (USA).z64"
    log_path = simdisk / ".rockbox/rocks/games/sm64/sm64.log"
    dump_path = simdisk / ".rockbox/rocks/games/sm64/test-frame.ppm"

    if not plugin_source.is_file():
        raise SystemExit(f"missing simulator plugin: {plugin_source}")
    if not rom.is_file() or sha1(rom) != ROM_SHA1:
        raise SystemExit("SM64 gate requires the exact owned US v1.0 ROM")

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
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, "sm64.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, str(rom_target))
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

    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.unlink(missing_ok=True)
    dump_path.unlink(missing_ok=True)
    return simdisk, log_path, dump_path


def valid_ppm(path: Path, expect_gameplay: bool) -> bool:
    if not path.is_file():
        return False
    data = path.read_bytes()
    header = b"P6\n160 120\n255\n"
    if not data.startswith(header) or len(data) != len(header) + 160 * 120 * 3:
        return False
    pixels = data[len(header) :]
    if len(set(pixels[::97])) <= 8:
        return False
    if expect_gameplay:
        rgb = zip(pixels[0::3], pixels[1::3], pixels[2::3])
        green_pixels = sum(
            green > red * 1.25 and green > blue * 1.1 and green > 60
            for red, green, blue in rgb
        )
        # Castle grounds provides a stable distinction from the yellow file
        # select scene and title head reached earlier in the scripted run.
        return green_pixels > 2000
    return True


def validate(build_dir: Path, frames: int) -> int:
    simdisk = build_dir / "simdisk"
    log_path = simdisk / ".rockbox/rocks/games/sm64/sm64.log"
    dump_path = simdisk / ".rockbox/rocks/games/sm64/test-frame.ppm"
    save_path = simdisk / ".rockbox/games/n64/saves/Super Mario 64.sav"

    if not log_path.is_file():
        print("FAIL: plugin log missing")
        return 1
    log = log_path.read_text(encoding="utf-8", errors="replace")
    print(log, end="")
    expected = f"exit frames={max(1, frames)} "
    passed = (
        "start arena=" in log
        and "audio init requested=32000" in log
        and expected in log
        and "fatal:" not in log
        and valid_ppm(dump_path, expect_gameplay=frames >= 1800)
        and save_path.is_file()
        and save_path.stat().st_size == 512
    )
    if not passed:
        print("FAIL: launch/render/audio/save/clean-exit gate did not pass")
        return 1
    suffix = " through castle-ground gameplay" if frames >= 1800 else ""
    print(f"PASS: SM64 launched, rendered, saved, and exited cleanly{suffix}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument(
        "--rom",
        type=Path,
        default=Path("/home/david/Downloads/Super Mario 64 (USA).z64"),
    )
    parser.add_argument("--frames", type=int, default=1800)
    parser.add_argument("--validate", action="store_true")
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    if args.validate:
        return validate(build_dir, args.frames)

    simdisk, _, _ = prepare(build_dir, args.rom.expanduser().resolve())
    print(f"Prepared SM64 simulator disk: {simdisk}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
