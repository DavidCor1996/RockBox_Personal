#!/usr/bin/env python3
"""Prepare and run an isolated OpenLara simulator smoke test."""

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

PLUGIN_PATH = "/.rockbox/rocks/games/openlara.rock"


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


def prepare(build_dir: Path, data: Path) -> tuple[Path, Path]:
    simdisk = build_dir / "simdisk"
    plugin_source = build_dir / "apps/plugins/openlara/openlara.rock"
    plugin_target = simdisk / PLUGIN_PATH.lstrip("/")
    game_dir = simdisk / ".rockbox/games/ps1"
    required = ("TITLE.PKD", "GYM.PKD", "LEVEL1.PKD", "LEVEL2.PKD")

    if not plugin_source.is_file():
        raise SystemExit(f"missing simulator plugin: {plugin_source}")
    for name in required:
        if not (data / name).is_file():
            raise SystemExit(f"missing OpenLara test asset: {data / name}")

    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    game_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin_source, plugin_target)
    for name in required + ("TITLE.SCR", "TRACKS.AD4"):
        (game_dir / name).unlink(missing_ok=True)
        source = data / name
        if source.is_file():
            shutil.copy2(source, game_dir / name)

    marker = game_dir / "tomb-raider.olr"
    marker.write_text("OpenLara=1\ngame=TR1\nasset_format=PKD\n", encoding="utf-8")
    (game_dir / "games.tsv").write_text(
        "id\ttitle\tfile\tcover\tfavorite\tlast_played\thaptic_profile\n"
        "tomb-raider\tTomb Raider\ttomb-raider.olr\t\t1\t\topenlara\n",
        encoding="utf-8",
    )

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
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, "openlara.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH)
    marker_arg = "/" + marker.relative_to(simdisk).as_posix()
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, marker_arg)
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
    return simdisk, simdisk / ".rockbox/logs/openlara.log"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=120)
    parser.add_argument("--level", choices=("TITLE", "GYM", "LEVEL1", "LEVEL2"), default="TITLE")
    parser.add_argument("--wheel-position", type=int)
    parser.add_argument("--hold-exit-frame", type=int)
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    simdisk, log_path = prepare(build_dir, args.data.expanduser().resolve())
    print(f"Prepared OpenLara simulator disk: {simdisk}")
    if not args.run:
        return 0

    log_path.unlink(missing_ok=True)
    environment = os.environ.copy()
    environment["OPENLARA_TEST_FRAMES"] = str(max(1, args.frames))
    environment["OPENLARA_TEST_UNTHROTTLED"] = "1"
    environment["OPENLARA_TEST_LEVEL"] = args.level
    environment["OPENLARA_TEST_DUMP"] = "1"
    if args.wheel_position is not None:
        environment["OPENLARA_TEST_WHEEL"] = str(args.wheel_position % 96)
    if args.hold_exit_frame is not None:
        environment["OPENLARA_TEST_HOLD_AFTER"] = str(
            max(1, args.hold_exit_frame)
        )
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

    if not log_path.is_file():
        return process.returncode or 1
    log = log_path.read_text(encoding="utf-8", errors="replace")
    print(log, end="")
    expected = f"frames={max(1, args.frames)}"
    loaded = f"level name={args.level} "
    passed = (
        "exit status=0" in log
        and loaded in log
        and "load_failed=0" in log
    )
    if args.hold_exit_frame is None:
        passed = passed and expected in log
    else:
        frame_match = re.search(r"exit status=0 frames=(\d+).+hold=1", log)
        passed = (
            passed
            and frame_match is not None
            and int(frame_match.group(1)) < max(1, args.frames)
        )
    if args.wheel_position is not None:
        movement = re.search(
            r"input keys=0x([0-9a-f]+) start=(-?\d+),(-?\d+),(-?\d+) "
            r"end=(-?\d+),(-?\d+),(-?\d+)",
            log,
        )
        passed = passed and movement is not None
        if movement and args.level != "TITLE":
            start = movement.group(2, 3, 4)
            end = movement.group(5, 6, 7)
            passed = passed and start != end
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
