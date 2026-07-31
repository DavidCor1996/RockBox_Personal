#!/usr/bin/env python3
"""Resume a complete offline Esri World Imagery atlas on a mounted iPod.

Tiles are stored in Rockbox's fixed 320x160 RGB565 format. Existing complete
tiles are left untouched, so the tool is safe to rerun after an interruption.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import io
import os
import time
import urllib.request
from pathlib import Path

import numpy as np
from PIL import Image


TILE_BYTES = 320 * 160 * 2
URL = (
    "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/"
    "MapServer/tile/{z}/{y}/{x}"
)


def rgb565(image_data: bytes) -> bytes:
    """Convert packed RGB bytes to little-endian RGB565 without Python loops."""
    rgb = np.frombuffer(image_data, dtype=np.uint8).reshape(-1, 3)
    pixels = ((rgb[:, 0].astype(np.uint16) & 0xF8) << 8) | \
             ((rgb[:, 1].astype(np.uint16) & 0xFC) << 3) | \
             (rgb[:, 2].astype(np.uint16) >> 3)
    return pixels.astype("<u2", copy=False).tobytes()


def sync_tile(destination: Path, z: int, x: int, y: int) -> bool:
    # FAT32 has a 32,768-entry directory ceiling. A full z8 contains 65,536
    # files, so split it by hemisphere while retaining the normal layout for
    # all lower zoom levels.
    if z == 8 and x >= 254:
        tile_directory = f"{z}_hi2"
    elif z == 8 and x >= 127:
        tile_directory = f"{z}_hi"
    else:
        tile_directory = str(z)
    output = destination / tile_directory / f"{x}_{y}.r16"
    if output.is_file() and output.stat().st_size == TILE_BYTES:
        return False
    output.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(URL.format(z=z, x=x, y=y), headers={
        "User-Agent": "RockPod offline-map sync/1.0",
    })
    for attempt in range(4):
        try:
            with urllib.request.urlopen(request, timeout=45) as response:
                image = Image.open(io.BytesIO(response.read())).convert("RGB")
                image = image.resize((320, 160), Image.Resampling.LANCZOS)
                converted = rgb565(image.tobytes())
            if len(converted) != TILE_BYTES:
                raise ValueError(f"unexpected converted size: {len(converted)}")
            temporary = output.with_suffix(".tmp")
            temporary.write_bytes(converted)
            os.replace(temporary, output)
            return True
        except Exception:
            if attempt == 3:
                raise
            time.sleep(1 << attempt)
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mount", type=Path, help="mounted iPod volume")
    parser.add_argument("--min-zoom", type=int, default=1)
    parser.add_argument("--max-zoom", type=int, default=8)
    parser.add_argument("--workers", type=int, default=24)
    args = parser.parse_args()
    if not 0 <= args.min_zoom <= args.max_zoom <= 8:
        parser.error("zoom range must be within 0 through 8")

    destination = args.mount / ".rockbox/maps/world"
    jobs = [
        (z, x, y)
        for z in range(args.min_zoom, args.max_zoom + 1)
        for x in range(1 << z)
        for y in range(1 << z)
    ]
    completed = 0
    failed = 0
    processed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(sync_tile, destination, z, x, y): (z, x, y)
                   for z, x, y in jobs}
        for future in concurrent.futures.as_completed(futures):
            processed += 1
            try:
                completed += int(future.result())
            except Exception as error:
                failed += 1
                z, x, y = futures[future]
                print(f"failed z{z}/{x}/{y}: {error}", flush=True)
            if processed % 500 == 0:
                print(f"processed {processed}/{len(jobs)}; wrote {completed}; "
                      f"failures {failed}", flush=True)
    print(f"wrote {completed}; failures {failed}", flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
