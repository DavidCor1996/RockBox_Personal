#!/usr/bin/env python3
"""Convert a licensed source photograph into an iPod Hero background BMP.

The prepared backgrounds under ``assets/ipodhero/local/prepared/backgrounds``
were originally produced by hand. This tool makes that step reproducible and
keeps the documented transform in code rather than only in prose:

  1. centre-crop the source to 4:3,
  2. resize to exactly 320x240,
  3. raise saturation to 105 percent,
  4. scale brightness to 72 percent.

Steps 3 and 4 keep the gameplay HUD, note highway and rock meter readable on
top of a photograph. Those transformations are derivative-work changes and
must be disclosed alongside the attribution recorded in PROVENANCE.md.

The output is a bottom-up 24-bit Windows BMP, the only background format the
runtime skin loader accepts. Nothing here touches the network; the source
photograph must already be present locally.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


WIDTH = 320
HEIGHT = 240

CROP = "crop='min(iw,ih*4/3)':'min(ih,iw*3/4)'"
RESIZE = f"scale={WIDTH}:{HEIGHT}:flags=lanczos"
SATURATION = "eq=saturation=1.05"
BRIGHTNESS = "colorchannelmixer=rr=0.72:gg=0.72:bb=0.72"

FILTERS = ",".join((CROP, RESIZE, SATURATION, BRIGHTNESS))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_bmp(path: Path) -> None:
    with path.open("rb") as handle:
        header = handle.read(30)
    if len(header) != 30 or header[:2] != b"BM":
        raise ValueError(f"not a Windows BMP: {path}")
    width, height = struct.unpack_from("<ii", header, 18)
    bits = struct.unpack_from("<H", header, 28)[0]
    if (width, abs(height), bits) != (WIDTH, HEIGHT, 24):
        raise ValueError(
            f"unexpected BMP {path}: {width}x{abs(height)}x{bits}"
        )
    expected = 54 + WIDTH * HEIGHT * 3
    if path.stat().st_size != expected:
        raise ValueError(f"unexpected BMP size {path.stat().st_size}: {path}")


def convert(source: Path, output: Path) -> None:
    if not source.is_file():
        raise FileNotFoundError(f"missing source photograph: {source}")
    output.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary_name = tempfile.mkstemp(
        prefix=f".{output.name}.", suffix=".bmp", dir=output.parent
    )
    os.close(handle)
    temporary = Path(temporary_name)
    try:
        subprocess.run(
            ["ffmpeg", "-v", "error", "-y", "-i", str(source),
             "-vf", FILTERS, "-pix_fmt", "bgr24", "-frames:v", "1",
             "-f", "image2", str(temporary)],
            check=True,
        )
        verify_bmp(temporary)
        temporary.chmod(0o644)
        temporary.replace(output)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path,
                        help="licensed local source photograph")
    parser.add_argument("--output", required=True, type=Path,
                        help="prepared 320x240x24 BMP to write")
    parser.add_argument("--verify", action="store_true",
                        help="verify an existing output instead of writing")
    args = parser.parse_args()

    if args.verify:
        verify_bmp(args.output)
        print(f"verified background: {args.output}  {sha256(args.output)}")
        return 0

    convert(args.source, args.output)
    print(f"prepared background: {args.output}  {sha256(args.output)}")
    print(f"source: {args.source}  {sha256(args.source)}")
    print(f"filters: {FILTERS}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
