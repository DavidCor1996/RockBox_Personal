#!/usr/bin/env python3
"""Simulator acceptance gate for the Pokedex plugin.

Mirrors tools/cps1_sim_gate.py's shape (write a fixture, open the plugin
directly via a synthetic plugin.dat entry, drive it, assert real pixel
colours) and tools/rockachievements_ui_sim_gate.py's UI-driving helpers
(xdotool taps, ROCKPOD_SIM_PREVIEW_BMP framebuffer capture).

This gate writes a small *fixture* pack (a few fixture species + tiny
solid-colour placeholder sprites) rather than depending on real PokeAPI
data being fetched -- fixture test art is not a "real asset" violation,
the same way Live TV's gates use fixture channel/guide TSVs. See
docs/pokedex-spec.md section 9.
"""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import tempfile
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

PLUGIN_PATH = "/.rockbox/rocks/apps/pokedex.rock"

# Sampled from a real screencap of the original-series anime Pokedex
# actively scanning a Pokemon -- see docs/pokedex-spec.md section 6.
# (x, y) are in the plugin's own 320x240 drawing, which is also the
# simulator's frame size for this target.
PALETTE_PROBES = [
    ("title bar (shell red)", 4, 4, (210, 28, 46)),
    ("screen card (sage green)", 40, 60, (188, 206, 131)),
    ("bottom bar (shell red)", 10, 225, (210, 28, 46)),
]
TOLERANCE = 12


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


def write_solid_bmp32(path: Path, width: int, height: int,
                       rgba: tuple[int, int, int, int]) -> None:
    """Write a bottom-up 32bpp BI_RGB BGRA BMP, same layout as
    tools/sitekick_package_assets.py's write_bmp32()."""
    r, g, b, a = rgba
    row = bytes((b, g, r, a)) * width
    body = row * height
    header = struct.pack("<2sIHHI", b"BM", 14 + 40 + len(body), 0, 0, 14 + 40)
    info = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 32, 0,
                        len(body), 2835, 2835, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + info + body)


FIXTURE_SPECIES = [
    (1, "Fixturesaur", "Fixture Pokemon", "grass", "poison", "2'04\"",
     "15.2 lbs.", 45, 49, 49, 65, 65, 45,
     "A fixture species used only by the simulator gate."),
    (2, "Testmander", "Fixture Pokemon", "fire", "",
     "2'00\"", "18.7 lbs.", 39, 52, 43, 60, 50, 65,
     "A second fixture species used only by the simulator gate."),
]


def write_fixture_pack(root: Path) -> None:
    bundle_dir = root / ".rockbox" / "pokedex"
    sprites_dir = bundle_dir / "sprites"
    sprites_dir.mkdir(parents=True, exist_ok=True)

    lines = ["pokedex_v1\t2000-01-01T00:00:00Z\tfixture\t2"]
    for row in FIXTURE_SPECIES:
        lines.append("\t".join(str(field) for field in row))
        dex_id = row[0]
        write_solid_bmp32(sprites_dir / f"{dex_id:03d}.bmp", 32, 32,
                           (80, 160, 80, 255))
    (bundle_dir / "pokedex.v1.tsv").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")
    (bundle_dir / "source.manifest").write_text(
        "pokedex.v1.tsv\tfixture\n", encoding="utf-8")


def prepare_root(build_dir: Path, root: Path) -> Path:
    simdisk = build_dir / "simdisk"
    plugin_source = build_dir / "apps" / "plugins" / "pokedex.rock"
    if not plugin_source.is_file():
        raise SystemExit(f"missing simulator plugin: {plugin_source}")

    rockbox = root / ".rockbox"
    shutil.copytree(simdisk / ".rockbox", rockbox, dirs_exist_ok=True)

    plugin_target = root / PLUGIN_PATH.lstrip("/")
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin_source, plugin_target)

    write_fixture_pack(root)

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build_dir)
    struct.pack_into(
        "<IiI", entry, 0, START_SCREEN_HASH,
        start_screen_lang_id(build_dir), checksum or OPEN_PLUGIN_CHECKSUM,
    )
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE,
                  "pokedex.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE,
                  PLUGIN_PATH)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, "")
    (rockbox / "rocks" / "plugin.dat").parent.mkdir(parents=True,
                                                     exist_ok=True)
    (rockbox / "rocks" / "plugin.dat").write_bytes(entry)
    (rockbox / "config.cfg").write_text(
        "start in screen: plugin\nresume: off\ntagcache_autoupdate: off\n",
        encoding="utf-8",
    )
    return root


def window_id(pid: int) -> str:
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        result = subprocess.run(
            ["xdotool", "search", "--pid", str(pid)],
            check=False, capture_output=True, text=True,
        )
        ids = result.stdout.splitlines()
        if ids:
            window = ids[-1]
            subprocess.run(["xdotool", "windowactivate", window], check=False)
            time.sleep(0.25)
            return window
        time.sleep(0.25)
    raise SystemExit("could not find the simulator window")


def tap(pid: int, key: str) -> None:
    window = window_id(pid)
    subprocess.run(["xdotool", "keydown", "--window", window, key], check=True)
    time.sleep(0.08)
    subprocess.run(["xdotool", "keyup", "--window", window, key], check=True)
    time.sleep(0.45)


def wait_for_file(path: Path, timeout: float = 20.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if path.is_file() and path.stat().st_size >= 307_200:
            return
        time.sleep(0.05)
    raise SystemExit(f"framebuffer capture timed out: {path}")


def capture(frame: Path, output: Path) -> None:
    wait_for_file(frame)
    subprocess.run(["magick", str(frame), str(output)], check=True)


def changed_pixels(first: Path, second: Path) -> int:
    result = subprocess.run(
        ["magick", "compare", "-metric", "AE", str(first), str(second),
         "null:"],
        check=False, capture_output=True, text=True,
    )
    value = (result.stderr or result.stdout).strip().split()[0]
    return int(float(value))


def sample(png: Path, x: int, y: int) -> tuple[int, int, int]:
    result = subprocess.run(
        ["magick", str(png), "-format", f"%[pixel:p{{{x},{y}}}]", "info:"],
        check=True, capture_output=True, text=True,
    )
    digits = [int(v) for v in
              result.stdout.strip().replace("srgb(", "").replace(")", "")
              .split(",")[:3]]
    return tuple(digits)


def close_enough(actual, expected, tolerance) -> bool:
    return all(abs(a - e) <= tolerance for a, e in zip(actual, expected))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path,
                         default=Path("build-sim-ipod6g"))
    parser.add_argument("--output", type=Path,
                         default=Path("/tmp/pokedex-sim-gate"))
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    simulator = build_dir / "rockboxui"
    if not simulator.is_file():
        raise SystemExit(f"missing simulator: {simulator}")
    for command in ("magick", "xdotool"):
        if shutil.which(command) is None:
            raise SystemExit(f"missing required command: {command}")

    args.output.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="pokedex-sim-") as temp:
        root = prepare_root(build_dir, Path(temp))
        frame = root / "frame.bmp"
        environment = os.environ.copy()
        environment.update({
            "SDL_AUDIODRIVER": "dummy",
            "SDL_VIDEODRIVER": "x11",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
            "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
        })
        process = subprocess.Popen(
            [str(simulator), "--zoom", "1", "--nobackground",
             "--root", str(root)],
            cwd=build_dir, env=environment,
        )
        try:
            wait_for_file(frame)
            time.sleep(1.5)
            window_id(process.pid)

            list_png = args.output / "list.png"
            info_png = args.output / "info.png"
            stats_png = args.output / "stats.png"
            capture(frame, list_png)

            tap(process.pid, "KP_5")  # select -> open detail (info page)
            capture(frame, info_png)

            tap(process.pid, "KP_Decimal")  # menu -> flip to stats page
            capture(frame, stats_png)

            if changed_pixels(list_png, info_png) < 5_000:
                raise SystemExit("SELECT did not open the dex detail page")
            if changed_pixels(info_png, stats_png) < 5_000:
                raise SystemExit("MENU did not flip Info <-> Stats")

            for label, x, y, expected in PALETTE_PROBES:
                actual = sample(info_png, x, y)
                if not close_enough(actual, expected, TOLERANCE):
                    raise SystemExit(
                        f"{label} at ({x},{y}): expected {expected}, "
                        f"got {actual}")
        finally:
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()

    print(f"Pokedex simulator gate passed; captures: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
