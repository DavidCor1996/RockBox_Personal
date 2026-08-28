#!/usr/bin/env python3
"""Prepare and run isolated Zelda3 Rockbox simulator qualification gates.

The controls gate needs no copyrighted data.  The frame/performance gate only
accepts a user-extracted zelda3_assets.dat and never downloads game assets.
"""

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

PLUGIN_PATH = "/.rockbox/rocks/games/zelda3.rock"
PROFILE_RE = re.compile(
    r"profile frames=(\d+) ticks=(-?\d+) fps_x1000=(\d+) "
    r"overruns=(\d+) rendered=(\d+) unthrottled=(\d+)"
)


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


def prepare(build_dir: Path, assets: Path | None) -> tuple[Path, Path]:
    simdisk = build_dir / "simdisk"
    plugin_source = build_dir / "apps" / "plugins" / "zelda3" / "zelda3.rock"
    plugin_target = simdisk / PLUGIN_PATH.lstrip("/")
    if not plugin_source.is_file():
        raise SystemExit(f"missing simulator plugin: {plugin_source}")
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin_source, plugin_target)

    if assets is not None:
        if not assets.is_file():
            raise SystemExit(f"missing user-extracted asset archive: {assets}")
        asset_target = simdisk / ".rockbox" / "zelda3" / "zelda3_assets.dat"
        asset_target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(assets, asset_target)

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
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, "zelda3.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, "")
    plugin_dat = simdisk / ".rockbox" / "rocks" / "plugin.dat"
    plugin_dat.parent.mkdir(parents=True, exist_ok=True)
    plugin_dat.write_bytes(entry)

    for config_path in (simdisk / ".rockbox" / "config.cfg", simdisk / "config.cfg"):
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
    return simdisk, simdisk / ".rockbox" / "zelda3" / "zelda3.log"


def run_simulator(build_dir: Path, log_path: Path, environment: dict[str, str]) -> str:
    log_path.unlink(missing_ok=True)
    process = subprocess.Popen([str(build_dir / "rockboxui")], cwd=build_dir, env=environment)
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
    if not log_path.exists():
        raise SystemExit(f"simulator produced no Zelda3 log (return code {process.returncode})")
    return log_path.read_text(encoding="utf-8", errors="replace")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument("--assets", type=Path)
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--controls-only", action="store_true")
    parser.add_argument("--unthrottled", action="store_true")
    parser.add_argument("--dump-frame", type=int)
    parser.add_argument("--dump-output", type=Path)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    assets = args.assets.expanduser().resolve() if args.assets else None
    if not args.controls_only and assets is None:
        raise SystemExit(
            "frame qualification requires a user-extracted zelda3_assets.dat; "
            "run --controls-only without assets"
        )
    simdisk, log_path = prepare(build_dir, assets)
    environment = os.environ.copy()
    if args.controls_only:
        environment["ZELDA3_TEST_CONTROLS"] = "1"
    else:
        environment["ZELDA3_TEST_FRAMES"] = str(max(1, args.frames))
        if args.unthrottled:
            environment["ZELDA3_TEST_UNTHROTTLED"] = "1"
        if args.dump_output:
            environment["ZELDA3_TEST_DUMP_PPM"] = "/.rockbox/zelda3/capture.ppm"
            environment["ZELDA3_TEST_DUMP_FRAME"] = str(
                max(0, args.dump_frame if args.dump_frame is not None else 300)
            )
    log_text = run_simulator(build_dir, log_path, environment)
    print(log_text, end="")

    if args.dump_output:
        capture = simdisk / ".rockbox" / "zelda3" / "capture.ppm"
        if not capture.is_file():
            return 1
        output = args.dump_output.expanduser().resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(capture, output)
        print(f"captured_frame={output}")

    if "exit status=0" not in log_text:
        return 1
    if args.controls_only:
        return 0 if "controls selftest=pass coverage=0xfff expected=0xfff" in log_text else 1

    match = PROFILE_RE.search(log_text)
    if not match:
        return 1
    frames, ticks, fps_x1000, overruns, rendered, unthrottled = map(int, match.groups())
    if frames != max(1, args.frames) or rendered != frames:
        return 1
    if not unthrottled and fps_x1000 < 57000:
        return 1
    if unthrottled and fps_x1000 < 60000:
        return 1
    if unthrottled != int(args.unthrottled) or ticks < 0:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
