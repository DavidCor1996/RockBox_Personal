#!/usr/bin/env python3
"""Pack existing Club Penguin animation frames into runtime bitmap strips.

The iPod plugin decodes each strip once when a room opens.  This preserves the
authored frames while avoiding a full filesystem open and BMP decode on every
animation tick.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


SIZE = (320, 220)
ROOM_FRAMES = 12
CONCERT_SOURCE_FRAMES = 24
CONCERT_CACHE_FRAMES = 18


def load_frame(path: Path) -> Image.Image:
    with Image.open(path) as source:
        frame = source.convert("RGB")
    if frame.size != SIZE:
        raise ValueError(f"{path}: expected {SIZE}, got {frame.size}")
    return frame


def write_strip(paths: list[Path], output: Path) -> None:
    strip = Image.new("RGB", (SIZE[0] * len(paths), SIZE[1]))
    for index, path in enumerate(paths):
        strip.paste(load_frame(path), (index * SIZE[0], 0))
    output.parent.mkdir(parents=True, exist_ok=True)
    strip.save(output, "BMP")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()

    night_city = args.assets / "rooms" / "night_city"
    frame_dirs = sorted(
        path for path in night_city.glob("*_frames") if path.is_dir()
    )
    if not frame_dirs:
        raise SystemExit("no Night City frame directories found")

    for frame_dir in frame_dirs:
        count = (CONCERT_CACHE_FRAMES
                 if frame_dir.name == "emma_sewer_frames"
                 else ROOM_FRAMES)
        paths = [frame_dir / f"{index}.bmp" for index in range(count)]
        write_strip(paths, frame_dir / "strip.bmp")

    concert = args.assets / "concert"
    # Keep the original three-second loop but use 18 evenly distributed
    # frames, which fits beside the paper doll in the 3 MiB iPod plugin
    # buffer and plays smoothly at six frames per second.
    indices = [
        round(index * CONCERT_SOURCE_FRAMES / CONCERT_CACHE_FRAMES)
        for index in range(CONCERT_CACHE_FRAMES)
    ]
    paths = [concert / "frames" / f"{index}.bmp" for index in indices]
    write_strip(paths, concert / "strip.bmp")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
