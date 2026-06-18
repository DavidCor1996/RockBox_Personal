#!/usr/bin/env python3
from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: build_rockbox_albumart_bmps.py MANIFEST OUTDIR", file=sys.stderr)
        return 2

    manifest = Path(sys.argv[1])
    outdir = Path(sys.argv[2])
    magick = shutil.which("magick")
    if not magick:
        print("magick not found", file=sys.stderr)
        return 1

    outdir.mkdir(parents=True, exist_ok=True)
    converted = 0
    failed: list[tuple[str, str]] = []

    for line in manifest.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        parts = line.split("\t")
        if len(parts) < 2:
            continue
        src = Path(parts[0])
        name = Path(parts[1]).with_suffix(".bmp").name
        dst = outdir / name
        if not src.exists():
            failed.append((str(src), "missing source"))
            continue

        proc = subprocess.run(
            [
                magick,
                str(src),
                "-auto-orient",
                "-resize",
                "100x100^",
                "-gravity",
                "center",
                "-extent",
                "100x100",
                "-alpha",
                "off",
                "-colorspace",
                "sRGB",
                "-type",
                "TrueColor",
                f"BMP3:{dst}",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if proc.returncode == 0 and dst.exists() and dst.stat().st_size > 0:
            converted += 1
        else:
            failed.append((str(src), proc.stderr.strip() or f"exit {proc.returncode}"))

    print(f"converted={converted} failed={len(failed)} outdir={outdir}")
    for src, reason in failed[:20]:
        print(f"failed\t{src}\t{reason}")
    return 0 if not failed else 1


if __name__ == "__main__":
    raise SystemExit(main())
