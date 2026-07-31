#!/usr/bin/env python3
"""Generate offline rotatable globe frames from the real Blue Marble atlas."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "assets/nb_maps"
TILE_DIR = ASSET_DIR / "world_tiles/1"
WIDTH = 320
HEIGHT = 160
DEFAULT_FRAME_COUNT = 64
RADIUS = 80.0


def load_world() -> np.ndarray:
    """Stitch the real z1 Blue Marble tiles into a world raster."""
    world = np.empty((HEIGHT * 2, WIDTH * 2), dtype="<u2")
    for x in range(2):
        for y in range(2):
            tile = np.fromfile(TILE_DIR / f"{x}_{y}.r16", dtype="<u2")
            if tile.size != WIDTH * HEIGHT:
                raise ValueError(f"invalid source tile {x}_{y}")
            world[y * HEIGHT:(y + 1) * HEIGHT,
                  x * WIDTH:(x + 1) * WIDTH] = tile.reshape(HEIGHT, WIDTH)
    return world


def make_frame(world: np.ndarray, central_longitude: float) -> np.ndarray:
    """Orthographically project one heading, preserving real source pixels."""
    frame = np.full((HEIGHT, WIDTH), 0xdf7e, dtype="<u2")
    y, x = np.indices((HEIGHT, WIDTH), dtype=np.float32)
    globe_x = (x - (WIDTH - 1) / 2.0) / RADIUS
    globe_y = ((HEIGHT - 1) / 2.0 - y) / RADIUS
    rho = np.sqrt(globe_x * globe_x + globe_y * globe_y)
    visible = rho <= 1.0
    safe_rho = np.where(rho == 0.0, 1.0, rho)
    angle = np.arcsin(np.minimum(rho, 1.0))
    latitude = np.arcsin(globe_y * np.sin(angle) / safe_rho)
    longitude = central_longitude + np.arctan2(
        globe_x * np.sin(angle), safe_rho * np.cos(angle)
    )
    source_x = ((longitude / (2.0 * math.pi) + 0.5) % 1.0 * (WIDTH * 2)).astype(int)
    mercator = np.arcsinh(np.tan(np.clip(latitude, -1.484, 1.484)))
    source_y = ((1.0 - mercator / math.pi) * 0.5 * (HEIGHT * 2)).astype(int)
    source_y = np.clip(source_y, 0, HEIGHT * 2 - 1)
    frame[visible] = world[source_y[visible], source_x[visible]]
    return frame


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", type=int, default=DEFAULT_FRAME_COUNT)
    args = parser.parse_args()
    if args.frames < 8:
        raise SystemExit("--frames must be at least 8")
    world = load_world()
    for frame in range(args.frames):
        longitude = -math.pi + frame * (2.0 * math.pi / args.frames)
        image = make_frame(world, longitude)
        image.tofile(ASSET_DIR / f"world_globe_{frame:02d}.r16")


if __name__ == "__main__":
    main()
