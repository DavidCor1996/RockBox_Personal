#!/usr/bin/env python3
"""Run Anarch's deterministic simulator gate."""

from __future__ import annotations

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

PLUGIN_PATH = "/.rockbox/rocks/games/anarch.rock"


def write_cstring(buffer: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")[: size - 1]
    buffer[offset : offset + len(encoded)] = encoded


def start_screen_lang_id(build_dir: Path) -> int:
    enum = build_dir / "lang_enum.h"
    if enum.is_file():
        for line in enum.read_text(encoding="utf-8", errors="replace").splitlines():
            if "LANG_START_SCREEN," in line and "/*" in line:
                return int(line.split("/*", 1)[1].split("*/", 1)[0].strip())
    return LANG_START_SCREEN


def plugin_entry(build_dir: Path) -> bytes:
    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build_dir) or OPEN_PLUGIN_CHECKSUM
    struct.pack_into(
        "<IiI",
        entry,
        0,
        START_SCREEN_HASH,
        start_screen_lang_id(build_dir),
        checksum,
    )
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE,
                  "anarch.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE,
                  PLUGIN_PATH)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE,
                  "--sim-test")
    return bytes(entry)


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    build_dir = (repo / "build-sim-ipod6g").resolve()
    simdisk = build_dir / "simdisk"
    source = build_dir / "apps/plugins/anarch/anarch.rock"
    target = simdisk / PLUGIN_PATH.lstrip("/")
    report = simdisk / ".rockbox/games/anarch/sim-test.log"
    save = simdisk / ".rockbox/games/anarch/anarch.sav"
    frontend_config = simdisk / ".rockbox/games/anarch/anarch.cfg"
    plugin_dat = simdisk / ".rockbox/rocks/plugin.dat"
    config = simdisk / ".rockbox/config.cfg"
    old_plugin_dat = plugin_dat.read_bytes() if plugin_dat.exists() else None
    old_config = config.read_bytes() if config.exists() else None
    old_save = save.read_bytes() if save.exists() else None
    old_frontend_config = frontend_config.read_bytes() \
        if frontend_config.exists() else None
    process: subprocess.Popen[str] | None = None
    capture = Path("/tmp/anarch-sim-frame.bmp")

    if not source.is_file():
        raise SystemExit(f"missing simulator plugin: {source}")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    report.unlink(missing_ok=True)
    capture.unlink(missing_ok=True)
    try:
        plugin_dat.write_bytes(plugin_entry(build_dir))
        lines = old_config.decode("utf-8", errors="replace").splitlines() \
            if old_config is not None else []
        lines = [line for line in lines
                 if not line.startswith(("start in screen:", "openplugin:"))]
        lines.append("start in screen: plugin")
        config.write_text("\n".join(lines) + "\n", encoding="utf-8")
        environment = os.environ.copy()
        environment.update({
            "SDL_AUDIODRIVER": "dummy",
            "SDL_VIDEODRIVER": "dummy",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_PREVIEW_BMP": str(capture),
            "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "1",
        })
        process = subprocess.Popen(
            [str(build_dir / "rockboxui"), "--zoom", "1",
             "--nobackground", "--root", str(simdisk)],
            cwd=build_dir,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            if report.is_file() and "pass=" in report.read_text(
                    encoding="utf-8", errors="replace"):
                break
            if process.poll() is not None:
                output = process.stdout.read() if process.stdout else ""
                raise RuntimeError("simulator exited early\n" + output[-2000:])
            time.sleep(0.1)
        else:
            raise TimeoutError("Anarch simulator gate timed out")
        result = report.read_text(encoding="utf-8", errors="replace")
        print(result, end="")
        values = dict(line.split("=", 1) for line in result.splitlines()
                      if "=" in line)
        required = {
            "pass": "1",
            "bindings": "0000ffff",
            "reached_gameplay": "1,1",
            "guards": "1,1",
            "save": "1",
            "config": "1",
            "menu": "1",
            "input": "1",
            "dump": "1",
            "frames": "1854",
            "state_silent": "0f43f45d",
            "state_audio": "0f43f45d",
            "framebuffer_silent": "02b9799b",
            "framebuffer_audio": "02b9799b",
            "map_silent": "868e0c05",
            "map_audio": "868e0c05",
        }
        arena_ok = int(values.get("arena_used", "0")) >= 320 * 240 * 2 \
            and int(values.get("arena_free", "0")) > 0
        diagnostics_ok = int(values.get("render_samples", "0")) > 0 \
            and int(values.get("late_frames", "-1")) >= 0 \
            and int(values.get("input_queue_worst", "-1")) >= 0
        return 0 if arena_ok and diagnostics_ok and all(
            values.get(key) == value for key, value in required.items()
        ) else 1
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if old_plugin_dat is None:
            plugin_dat.unlink(missing_ok=True)
        else:
            plugin_dat.write_bytes(old_plugin_dat)
        if old_config is None:
            config.unlink(missing_ok=True)
        else:
            config.write_bytes(old_config)
        if old_save is None:
            save.unlink(missing_ok=True)
        else:
            save.parent.mkdir(parents=True, exist_ok=True)
            save.write_bytes(old_save)
        if old_frontend_config is None:
            frontend_config.unlink(missing_ok=True)
        else:
            frontend_config.parent.mkdir(parents=True, exist_ok=True)
            frontend_config.write_bytes(old_frontend_config)


if __name__ == "__main__":
    raise SystemExit(main())
