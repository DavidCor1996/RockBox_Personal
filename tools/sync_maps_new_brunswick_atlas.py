#!/usr/bin/env python3
"""Install a compact, resumable New Brunswick satellite atlas.

The atlas uses row-major RGB565 strip packs.  Sixteen tile columns per pack
keep every file below FAT32's 4 GiB ceiling and avoid creating more than a
million individual files on the iPod.  The default stops at z14 (about 8 GB);
deeper province-wide levels must be requested explicitly because raw z15 and
z16 together require roughly 118 GB. Focused city syncs provide deeper detail
without that storage cost.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import io
import math
import os
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image


WEST, EAST = -69.1, -63.7
SOUTH, NORTH = 44.55, 48.1
TILE_WIDTH = 320
TILE_HEIGHT = 160
TILE_BYTES = TILE_WIDTH * TILE_HEIGHT * 2
SOURCE_TILE_SIZE = 256
PACK_COLUMNS = 16
EXPORT_ROWS = 16
WEB_MERCATOR_LIMIT = 20_037_508.342789244
EXPORT_URL = (
    "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/"
    "MapServer/export"
)
TILE_URL = (
    "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/"
    "MapServer/tile/{zoom}/{y}/{x}"
)
USER_AGENT = "RockPod Maps New Brunswick offline atlas/1.0"


@dataclass(frozen=True)
class Bounds:
    zoom: int
    x0: int
    x1: int
    y0: int
    y1: int

    @property
    def rows(self) -> int:
        return self.y1 - self.y0 + 1


def tile_y(latitude: float, zoom: int) -> int:
    count = 1 << zoom
    radians = math.radians(latitude)
    return int((1.0 - math.asinh(math.tan(radians)) / math.pi) * count / 2.0)


def bounds_for(zoom: int) -> Bounds:
    count = 1 << zoom
    return Bounds(
        zoom,
        int((WEST + 180.0) / 360.0 * count),
        int((EAST + 180.0) / 360.0 * count),
        tile_y(NORTH, zoom),
        tile_y(SOUTH, zoom),
    )


def export_bbox(zoom: int, x: int, y: int, width: int, height: int) -> str:
    count = 1 << zoom
    world = WEB_MERCATOR_LIMIT * 2.0
    xmin = -WEB_MERCATOR_LIMIT + x * world / count
    xmax = -WEB_MERCATOR_LIMIT + (x + width) * world / count
    ymax = WEB_MERCATOR_LIMIT - y * world / count
    ymin = WEB_MERCATOR_LIMIT - (y + height) * world / count
    return f"{xmin:.8f},{ymin:.8f},{xmax:.8f},{ymax:.8f}"


def fetch_export(zoom: int, x: int, y: int, width: int,
                 height: int) -> Image.Image:
    parameters = {
        "bbox": export_bbox(zoom, x, y, width, height),
        "bboxSR": "3857",
        "imageSR": "3857",
        "size": f"{width * SOURCE_TILE_SIZE},{height * SOURCE_TILE_SIZE}",
        "format": "jpg",
        "transparent": "false",
        "f": "image",
    }
    request = urllib.request.Request(
        EXPORT_URL + "?" + urllib.parse.urlencode(parameters),
        headers={"User-Agent": USER_AGENT},
    )
    last_error: Exception | None = None
    for attempt in range(5):
        try:
            with urllib.request.urlopen(request, timeout=180) as response:
                image = Image.open(io.BytesIO(response.read())).convert("RGB")
                image.load()
            expected = (width * SOURCE_TILE_SIZE,
                        height * SOURCE_TILE_SIZE)
            if image.size != expected:
                raise ValueError(f"export returned {image.size}, expected {expected}")
            return image
        except Exception as error:  # retry transient network/service failures
            last_error = error
            # The export service uses HTTP 500 for valid offshore extents
            # where it declines to render a bulk image.  Retrying that exact
            # request cannot help; let the caller reduce the batch and use
            # the standard tile endpoint for the affected row.
            if isinstance(error, urllib.error.HTTPError) and error.code == 500:
                break
            if attempt < 4:
                time.sleep(1 << attempt)
    raise RuntimeError(f"failed z{zoom}/{x}/{y}: {last_error}")


def fetch_tile_block(zoom: int, x: int, y: int, width: int,
                     height: int) -> Image.Image:
    """Fetch native tiles when the ArcGIS export endpoint has a gap."""
    def fetch_one(index: int) -> tuple[int, int, Image.Image]:
        local_x = index % width
        local_y = index // width
        tile_x = x + local_x
        tile_y = y + local_y
        request = urllib.request.Request(
            TILE_URL.format(zoom=zoom, y=tile_y, x=tile_x),
            headers={"User-Agent": USER_AGENT},
        )
        last_error: Exception | None = None
        for attempt in range(4):
            try:
                with urllib.request.urlopen(request, timeout=15) as response:
                    tile = Image.open(io.BytesIO(response.read())).convert("RGB")
                    tile.load()
                if tile.size != (SOURCE_TILE_SIZE, SOURCE_TILE_SIZE):
                    raise ValueError(f"tile returned {tile.size}")
                return local_x, local_y, tile
            except Exception as error:
                last_error = error
                if attempt < 3:
                    time.sleep(1 << attempt)
        else:
            raise RuntimeError(
                f"failed tile z{zoom}/{tile_x}/{tile_y}: {last_error}"
            )

    block = Image.new(
        "RGB", (width * SOURCE_TILE_SIZE, height * SOURCE_TILE_SIZE)
    )
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=min(width * height, 8)
    ) as pool:
        for local_x, local_y, tile in pool.map(
            fetch_one, range(width * height)
        ):
            block.paste(
                tile,
                (local_x * SOURCE_TILE_SIZE, local_y * SOURCE_TILE_SIZE),
            )
    return block


def rgb565(tile: Image.Image) -> bytes:
    tile = tile.resize((TILE_WIDTH, TILE_HEIGHT), Image.Resampling.LANCZOS)
    rgb = np.asarray(tile, dtype=np.uint8).reshape(-1, 3)
    pixels = ((rgb[:, 0].astype(np.uint16) & 0xF8) << 8) | \
             ((rgb[:, 1].astype(np.uint16) & 0xFC) << 3) | \
             (rgb[:, 2].astype(np.uint16) >> 3)
    return pixels.astype("<u2", copy=False).tobytes()


def sync_pack(destination: Path, bounds: Bounds, pack_x: int) -> tuple[int, int]:
    width = min(PACK_COLUMNS, bounds.x1 - pack_x + 1)
    target = destination / f"nb_z{bounds.zoom:02d}_x{pack_x:05d}.r16p"
    row_bytes = width * TILE_BYTES
    expected_size = bounds.rows * row_bytes
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.is_file() and target.stat().st_size == expected_size:
        return 0, bounds.rows * width

    mode = "r+b" if target.exists() else "w+b"
    with target.open(mode) as output:
        complete_rows = min(output.seek(0, os.SEEK_END) // row_bytes,
                            bounds.rows)
        output.truncate(complete_rows * row_bytes)
        output.seek(0, os.SEEK_END)
        written_tiles = 0
        while complete_rows < bounds.rows:
            height = min(EXPORT_ROWS, bounds.rows - complete_rows)
            y = bounds.y0 + complete_rows
            try:
                image = fetch_export(bounds.zoom, pack_x, y, width, height)
            except RuntimeError:
                # ArcGIS occasionally rejects an otherwise valid bulk export,
                # particularly over offshore extents. Fetching the exact same
                # official source tiles in parallel preserves native detail.
                image = fetch_tile_block(
                    bounds.zoom, pack_x, y, width, height
                )
            for local_y in range(height):
                top = local_y * SOURCE_TILE_SIZE
                for local_x in range(width):
                    left = local_x * SOURCE_TILE_SIZE
                    tile = image.crop((left, top, left + SOURCE_TILE_SIZE,
                                       top + SOURCE_TILE_SIZE))
                    output.write(rgb565(tile))
                    written_tiles += 1
            output.flush()
            complete_rows += height
    if target.stat().st_size != expected_size:
        raise RuntimeError(f"incomplete pack {target}")
    return written_tiles, bounds.rows * width


def required_bytes(minimum_zoom: int, maximum_zoom: int) -> int:
    total = 0
    for zoom in range(minimum_zoom, maximum_zoom + 1):
        bounds = bounds_for(zoom)
        total += ((bounds.x1 - bounds.x0 + 1) * bounds.rows * TILE_BYTES)
    return total


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mount", type=Path, help="mounted iPod or simdisk")
    parser.add_argument("--min-zoom", type=int, default=9)
    parser.add_argument("--max-zoom", type=int, default=14)
    parser.add_argument("--workers", type=int, default=6)
    parser.add_argument("--reserve-gb", type=float, default=5.0)
    args = parser.parse_args()
    if not 9 <= args.min_zoom <= args.max_zoom <= 16:
        parser.error("zoom range must be within 9 through 16")
    destination = args.mount / ".rockbox" / "maps" / "new_brunswick"
    destination.mkdir(parents=True, exist_ok=True)
    needed = required_bytes(args.min_zoom, args.max_zoom)
    free = os.statvfs(destination).f_bavail * os.statvfs(destination).f_frsize
    reserve = int(args.reserve_gb * 1_000_000_000)
    if free < needed + reserve:
        raise RuntimeError(
            f"atlas needs up to {needed / 1e9:.1f} GB plus a "
            f"{args.reserve_gb:g} GB reserve; "
            f"only {free / 1e9:.1f} GB is free"
        )

    for zoom in range(args.min_zoom, args.max_zoom + 1):
        bounds = bounds_for(zoom)
        pack_starts = list(range(bounds.x0, bounds.x1 + 1, PACK_COLUMNS))
        print(
            f"z{zoom}: {bounds.x1 - bounds.x0 + 1}x{bounds.rows} tiles, "
            f"{len(pack_starts)} packs",
            flush=True,
        )
        written = 0
        covered = 0
        with concurrent.futures.ThreadPoolExecutor(
            max_workers=max(1, args.workers)
        ) as pool:
            futures = {
                pool.submit(sync_pack, destination, bounds, pack_x): pack_x
                for pack_x in pack_starts
            }
            for future in concurrent.futures.as_completed(futures):
                pack_written, pack_covered = future.result()
                written += pack_written
                covered += pack_covered
                print(
                    f"z{zoom}: {covered}/{(bounds.x1 - bounds.x0 + 1) * bounds.rows} "
                    f"covered; {written} newly written",
                    flush=True,
                )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
