#!/usr/bin/env python3
"""Sync deep World Imagery around Maps cities and the current location."""

from __future__ import annotations

import argparse
import concurrent.futures
import math
from pathlib import Path

from sync_world_satellite_atlas import sync_tile


LOCATIONS = (
    (46.087800, -64.778200), (45.273300, -66.063300),
    (46.238200, -63.131100), (44.648800, -63.575200),
    (46.136800, -60.194200),
    (40.758000, -73.985500), (43.653200, -79.383200),
    (52.374752, 4.895167), (37.781832, -122.415560),
    (38.717854, -9.143727), (48.864811, 2.320894),
    (45.760102, 4.839942), (44.841096, -0.575150),
    (45.963600, -66.643100), (51.500700, -0.124600),
    (52.520000, 13.405000), (48.858400, 2.294500),
    (35.658600, 139.745400), (59.180000, 25.180000),
    (46.240000, 14.360000), (44.880000, 15.620000),
    (37.340000, 127.920000), (-41.510000, 173.960000),
    (39.140000, -77.200000), (35.470000, -97.520000),
)

MARITIME_CITIES = (
    (46.087800, -64.778200), (45.273300, -66.063300),
    (46.238200, -63.131100), (44.648800, -63.575200),
    (46.136800, -60.194200), (45.963600, -66.643100),
    (46.061000, -64.805200), (46.219900, -64.541100),
    (46.039100, -65.046300), (45.993500, -64.551500),
    (46.468400, -64.724900), (45.939400, -65.176600),
    (46.212800, -64.384700), (45.919900, -64.655200),
    (45.722700, -65.510200), (45.432000, -65.946600),
    (45.380700, -65.983800), (45.848400, -66.478800),
    (46.152000, -67.598100), (47.618100, -65.651300),
    (48.007500, -66.672700), (45.360800, -66.241300),
    (45.073000, -67.052600), (45.678000, -62.710000),
    (45.091000, -64.359000),
)

DETAILED_CITIES = MARITIME_CITIES[:6]


def tile_at(latitude: float, longitude: float, zoom: int) -> tuple[int, int]:
    scale = 1 << zoom
    x = int((longitude + 180.0) / 360.0 * scale)
    latitude = max(-85.05112878, min(85.05112878, latitude))
    radians = math.radians(latitude)
    y = int((1.0 - math.asinh(math.tan(radians)) / math.pi) * scale / 2.0)
    return x, y


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mount", type=Path, help="mounted iPod or simdisk")
    parser.add_argument("--workers", type=int, default=12)
    args = parser.parse_args()
    destination = args.mount / ".rockbox" / "maps" / "world"
    jobs: set[tuple[int, int, int]] = set()
    zoom_locations = {
        9: (LOCATIONS, 1),
        10: (LOCATIONS, 1),
        11: (MARITIME_CITIES, 1),
        12: (DETAILED_CITIES, 2),
        13: (DETAILED_CITIES, 3),
        14: (DETAILED_CITIES, 4),
        15: (LOCATIONS[:1], 12),
        16: (LOCATIONS[:1], 18),
        17: (LOCATIONS[:1], 24),
        18: (LOCATIONS[:1], 28),
    }
    for zoom, (locations, radius) in zoom_locations.items():
        scale = 1 << zoom
        for latitude, longitude in locations:
            center_x, center_y = tile_at(latitude, longitude, zoom)
            for dx in range(-radius, radius + 1):
                for dy in range(-radius, radius + 1):
                    jobs.add((zoom, (center_x + dx) % scale,
                              max(0, min(scale - 1, center_y + dy))))
    destination.mkdir(parents=True, exist_ok=True)
    completed = 0
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(sync_tile, destination, *job): job for job in jobs}
        for future in concurrent.futures.as_completed(futures):
            try:
                completed += int(future.result())
            except Exception as error:
                failed += 1
                print(f"failed {futures[future]}: {error}", flush=True)
    print(f"detail tiles {len(jobs)}; wrote {completed}; failures {failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
