#!/usr/bin/env python3
"""Launch the Montreal arena in a disposable iPod 6G simulator root."""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import tempfile
import time
import zipfile
from pathlib import Path

from PIL import Image, ImageChops, ImageStat

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


PLUGIN_PATH = "/.rockbox/rocks/games/clubpenguin.rock"


def write_cstring(buffer: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")[: size - 1]
    buffer[offset : offset + len(encoded)] = encoded


def start_screen_lang_id(build: Path) -> int:
    enum = build / "lang_enum.h"
    if enum.is_file():
        for line in enum.read_text(
            encoding="utf-8", errors="replace"
        ).splitlines():
            if "LANG_START_SCREEN," in line and "/*" in line:
                return int(line.split("/*", 1)[1].split("*/", 1)[0])
    return LANG_START_SCREEN


def wait_for_frame(frame: Path, process: subprocess.Popen, timeout: int = 20) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(
                f"simulator exited early with status {process.returncode}"
            )
        if frame.is_file() and frame.stat().st_size >= 300_000:
            return
        time.sleep(0.1)
    raise RuntimeError("simulator framebuffer capture timed out")


def replace_save_location(text: str, room: str, x: int, y: int) -> str:
    replacements = {"room": room, "x": str(x), "y": str(y)}
    lines = []
    for line in text.splitlines():
        key = line.split("=", 1)[0]
        if key in replacements:
            line = f"{key}={replacements[key]}"
        lines.append(line)
    return "\n".join(lines) + "\n"


def prepare_root(repo: Path, root: Path, room: str = "stadium",
                 x: int = 160, y: int = 170) -> tuple[Path, Path]:
    build = repo / "build-sim-ipod6g"
    rockbox = root / ".rockbox"
    source_rockbox = build / "simdisk/.rockbox"
    rockbox.mkdir(parents=True)
    for name in ("fonts", "langs", "icons"):
        if (source_rockbox / name).is_dir():
            shutil.copytree(source_rockbox / name, rockbox / name)

    with zipfile.ZipFile(repo / "build-hw-ipod6g/rockbox.zip") as archive:
        prefix = ".rockbox/rocks/games/clubpenguin/"
        for name in archive.namelist():
            if name.startswith(prefix):
                archive.extract(name, root)

    plugin_target = root / PLUGIN_PATH.lstrip("/")
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / "apps/plugins/clubpenguin.rock", plugin_target)

    original_save = source_rockbox / "rocks/games/clubpenguin/save.dat"
    save_target = rockbox / "rocks/games/clubpenguin/save.dat"
    save_text = original_save.read_text(encoding="utf-8")
    save_target.write_text(
        replace_save_location(save_text, room, x, y),
        encoding="utf-8",
    )

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build) or OPEN_PLUGIN_CHECKSUM
    struct.pack_into(
        "<IiI",
        entry,
        0,
        START_SCREEN_HASH,
        start_screen_lang_id(build),
        checksum,
    )
    write_cstring(
        entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE,
        "clubpenguin.rock",
    )
    write_cstring(
        entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH,
    )
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, "")
    (rockbox / "rocks/plugin.dat").write_bytes(entry)
    (rockbox / "config.cfg").write_text(
        "start in screen: plugin\nresume: off\ntagcache_autoupdate: off\n"
        "weather sync notifications: off\n",
        encoding="utf-8",
    )
    return build, rockbox


def mean_difference(actual: Image.Image, expected: Image.Image) -> float:
    difference = ImageChops.difference(actual.convert("RGB"), expected.convert("RGB"))
    return sum(ImageStat.Stat(difference).mean) / 3


def run(repo: Path, output: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="clubpenguin-hockey-") as temp:
        root = Path(temp)
        build, rockbox = prepare_root(repo, root)
        frame = root / "frame.bmp"
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_AUDIODRIVER": "dummy",
                "SDL_VIDEODRIVER": "dummy",
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_HIDDEN": "1",
                "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
                "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "25",
            }
        )
        process = subprocess.Popen(
            [
                str(build / "rockboxui"),
                "--zoom", "1",
                "--nobackground",
                "--root", str(root),
            ],
            cwd=build,
            env=environment,
        )
        try:
            wait_for_frame(frame, process)
            time.sleep(8)
            if process.poll() is not None:
                raise RuntimeError("plugin exited before the arena test")
            output.mkdir(parents=True, exist_ok=True)
            with Image.open(frame) as image:
                arena = image.convert("RGB")
            arena.save(output / "montreal-arena.png")
            time.sleep(0.6)
            with Image.open(frame) as image:
                animated = image.convert("RGB")
            animated.save(output / "montreal-arena-animated.png")
            animation_delta = mean_difference(
                arena.crop((0, 0, 320, 220)),
                animated.crop((0, 0, 320, 220)),
            )
            if animation_delta < 0.05:
                raise RuntimeError("arena animation did not advance")
            with Image.open(
                rockbox / "rocks/games/clubpenguin/rooms/night_city/"
                "stadium_frames/strip.bmp"
            ) as strip:
                expected = strip.convert("RGB").crop((0, 0, 320, 220))
                arena_delta = mean_difference(
                    arena.crop((0, 0, 320, 220)), expected
                )
            if arena_delta > 42:
                raise RuntimeError(
                    f"arena framebuffer differs unexpectedly ({arena_delta:.1f})"
                )

            interactions = (
                rockbox / "rocks/games/clubpenguin/data/interactions.tsv"
            ).read_text(encoding="utf-8")
            route = [
                line.split("\t")
                for line in interactions.splitlines()
                if line.startswith("stadium\tto_snow_forts\t")
            ]
            if len(route) != 1 or route[0][2:7] != [
                "160", "92", "30", "room", "snow_forts"
            ]:
                raise RuntimeError("top-centre Snow Forts exit metadata changed")
            print(f"PASS arena frame delta={arena_delta:.1f}")
            print(f"PASS animated frame delta={animation_delta:.2f}")
            print("PASS top-centre exit metadata targets Snow Forts")
            print("PASS simulator remained alive with dummy audio output active")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("test-artifacts/clubpenguin"),
    )
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    run(repo, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
