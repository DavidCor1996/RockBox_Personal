#!/usr/bin/env python3
"""Generate offline GeoNB RGB565 tiles for the Rockbox NB maps plugin."""

import argparse
import os
import struct
import subprocess
import sys


NB_EXTENT = (2306915, 7277892, 2710943, 7674855)
TILE_W = 320
TILE_H = 184
SERVICES = (
    ("street", "GeoNB_Basemap_Grey", "png32"),
    ("imagery", "GeoNB_Basemap_Imagery", "jpg"),
)
GRIDS = {
    3: 10,
    4: 20,
    5: 40,
}


def rgb_to_rgb565(data):
    out = bytearray()
    for i in range(0, len(data), 3):
        r, g, b = data[i], data[i + 1], data[i + 2]
        value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out += struct.pack("<H", value)
    return out


def export_url(service, fmt, bbox):
    x0, y0, x1, y1 = bbox
    return (
        f"https://geonb.snb.ca/arcgis/rest/services/{service}/MapServer/export?"
        f"bbox={x0:.0f},{y0:.0f},{x1:.0f},{y1:.0f}"
        f"&bboxSR=2036&imageSR=2036&size={TILE_W},{TILE_H}"
        f"&format={fmt}&transparent=false&f=image"
    )


def tile_bbox(grid, col, row):
    xmin, ymin, xmax, ymax = NB_EXTENT
    x0 = xmin + (xmax - xmin) * col / grid
    x1 = xmin + (xmax - xmin) * (col + 1) / grid
    y1 = ymax - (ymax - ymin) * row / grid
    y0 = ymax - (ymax - ymin) * (row + 1) / grid
    return x0, y0, x1, y1


def run(cmd):
    subprocess.run(cmd, check=True)


def generate_tile(out_dir, zoom, grid, prefix, service, fmt, col, row):
    target = os.path.join(out_dir, f"{prefix}_z{zoom}_{col:02d}_{row:02d}.r16")
    expected_size = TILE_W * TILE_H * 2

    if os.path.exists(target) and os.path.getsize(target) == expected_size:
        return False

    ext = "png" if fmt == "png32" else fmt
    image = f"/tmp/nb_maps_{prefix}_z{zoom}_{col:02d}_{row:02d}.{ext}"
    raw = f"/tmp/nb_maps_{prefix}_z{zoom}_{col:02d}_{row:02d}.rgb"
    url = export_url(service, fmt, tile_bbox(grid, col, row))

    print(f"{prefix} z{zoom} {col:02d},{row:02d}", flush=True)
    run(["curl", "-fsSL", url, "-o", image])
    run(["ffmpeg", "-v", "error", "-y", "-i", image,
         "-f", "rawvideo", "-pix_fmt", "rgb24", raw])

    with open(raw, "rb") as f:
        data = f.read()

    expected_rgb = TILE_W * TILE_H * 3
    if len(data) != expected_rgb:
        raise RuntimeError(f"{raw}: expected {expected_rgb} bytes, got {len(data)}")

    with open(target, "wb") as f:
        f.write(rgb_to_rgb565(data))

    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="assets/nb_maps/full_nb_tiles")
    parser.add_argument("--zoom", type=int, action="append",
                        help="zoom level to generate; may be repeated")
    parser.add_argument("--mode", choices=("street", "imagery"), action="append",
                        help="mode to generate; may be repeated")
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    zooms = args.zoom if args.zoom else sorted(GRIDS)
    modes = set(args.mode) if args.mode else {service[0] for service in SERVICES}
    made = 0

    for zoom in zooms:
        if zoom not in GRIDS:
            raise SystemExit(f"unsupported zoom {zoom}")

        grid = GRIDS[zoom]
        for prefix, service, fmt in SERVICES:
            if prefix not in modes:
                continue

            for row in range(grid):
                for col in range(grid):
                    if generate_tile(args.out, zoom, grid, prefix, service,
                                     fmt, col, row):
                        made += 1

    print(f"generated {made} tiles", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
