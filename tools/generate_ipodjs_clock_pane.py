#!/usr/bin/env python3
"""Rebuild the iPodJS Extras/Clock pane background from the Classic pack.

See assets/ipodjs/source/README-clock-pane.txt for provenance.  The
construction is crop and column replication only: every output pixel is
an unmodified source pixel.
"""

from __future__ import annotations

import argparse
import hashlib
import zipfile
from io import BytesIO
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE_MEMBER = (
    "Classic/5.5/5.5G/resource and images/"
    "Classic Firmware 5.5G resources and images only/images/pic21997.bmp"
)
SOURCE_SHA256 = (
    "d3b238a19f8a102f576b437ed18082f7719de445552e87461147c50b20b41bb2"
)
OUTPUT = ROOT / "assets/ipodjs/rockbox/previews/clock-pane-stock.174x240x24.bmp"
WIDTH, HEIGHT = 174, 240
SHADOW_START, SHADOW_COLUMNS = 190, 10
FLAT_COLUMN = 240


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("classic_zip", type=Path,
                        help="Classic.zip from the Classic firmware pack")
    args = parser.parse_args()

    with zipfile.ZipFile(args.classic_zip) as archive:
        data = archive.read(SOURCE_MEMBER)

    digest = hashlib.sha256(data).hexdigest()
    if digest != SOURCE_SHA256:
        raise SystemExit(
            f"refusing unrecognized source bitmap: sha256={digest}"
        )

    src = Image.open(BytesIO(data)).convert("RGB")
    out = Image.new("RGB", (WIDTH, HEIGHT))
    px_src = src.load()
    px_out = out.load()
    for y in range(HEIGHT):
        for i in range(SHADOW_COLUMNS):
            px_out[i, y] = px_src[SHADOW_START + i, y]
        flat = px_src[FLAT_COLUMN, y]
        for x in range(SHADOW_COLUMNS, WIDTH):
            px_out[x, y] = flat

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    out.save(OUTPUT, format="BMP")
    print(OUTPUT)
    print("sha256:", hashlib.sha256(OUTPUT.read_bytes()).hexdigest())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
