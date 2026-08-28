#!/usr/bin/env python3
"""Verify Club Penguin's stock-iPod animation cache assets and budget."""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

from PIL import Image, ImageChops


WIDTH = 320
HEIGHT = 220
ROOM_FRAMES = 12
CONCERT_SOURCE_FRAMES = 24
CONCERT_CACHE_FRAMES = 18
PLUGIN_BUFFER_SIZE = 0x300000


def check_equal(left: Image.Image, right: Image.Image, label: str) -> None:
    if ImageChops.difference(left, right).getbbox() is not None:
        raise SystemExit(f"FAIL {label}: strip crop differs from source")


def check_strip(strip_path: Path, paths: list[Path], label: str) -> None:
    with Image.open(strip_path) as source:
        strip = source.convert("RGB")
    expected = (WIDTH * len(paths), HEIGHT)
    if strip.size != expected:
        raise SystemExit(f"FAIL {label}: expected {expected}, got {strip.size}")
    for index, path in enumerate(paths):
        with Image.open(path) as source:
            frame = source.convert("RGB")
        crop = strip.crop((index * WIDTH, 0, (index + 1) * WIDTH, HEIGHT))
        check_equal(crop, frame, f"{label} frame {index}")
    print(f"PASS {label}: {len(paths)} exact cached frames")


def section(source: str, name: str) -> str:
    match = re.search(
        rf"static void {name}\(void\)\n\{{(.*?)\n\}}", source, re.S
    )
    if not match:
        raise SystemExit(f"FAIL missing {name}")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--elf", type=Path, required=True)
    args = parser.parse_args()

    night_city = args.assets / "rooms" / "night_city"
    frame_dirs = sorted(
        path for path in night_city.glob("*_frames") if path.is_dir()
    )
    for frame_dir in frame_dirs:
        count = (CONCERT_CACHE_FRAMES
                 if frame_dir.name == "emma_sewer_frames"
                 else ROOM_FRAMES)
        paths = [frame_dir / f"{index}.bmp" for index in range(count)]
        check_strip(frame_dir / "strip.bmp", paths, frame_dir.name)

    concert = args.assets / "concert"
    indices = [
        round(index * CONCERT_SOURCE_FRAMES / CONCERT_CACHE_FRAMES)
        for index in range(CONCERT_CACHE_FRAMES)
    ]
    paths = [concert / "frames" / f"{index}.bmp" for index in indices]
    check_strip(concert / "strip.bmp", paths, "concert")

    code = args.source.read_text()
    for function in ("cp_night_city_tick", "cp_concert_tick"):
        body = section(code, function)
        for forbidden in ("cp_load_scene", "read_bmp", "rb->open"):
            if forbidden in body:
                raise SystemExit(
                    f"FAIL {function}: steady-state I/O via {forbidden}"
                )
    print("PASS animation ticks perform no filesystem I/O or bitmap decode")

    size_output = subprocess.check_output(["size", str(args.elf)], text=True)
    fields = size_output.splitlines()[1].split()
    plugin_image = int(fields[3])
    player_elems = max(
        52 * 13 * 56 + ((52 * 13 * 4 + 8 + 1) // 2),
        320 * 64 + ((320 * 4 + 8 + 1) // 2),
    )
    animation_elems = (
        WIDTH * CONCERT_CACHE_FRAMES * HEIGHT
        + ((WIDTH * CONCERT_CACHE_FRAMES * 4 + 8 + 1) // 2)
    )
    cache_bytes = (player_elems + animation_elems) * 2
    remaining = PLUGIN_BUFFER_SIZE - plugin_image
    if cache_bytes > remaining:
        raise SystemExit(
            f"FAIL cache needs {cache_bytes} bytes, only {remaining} remain"
        )
    print(
        f"PASS fixed cache budget: {cache_bytes} bytes, "
        f"{remaining - cache_bytes} bytes plugin-arena headroom"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
