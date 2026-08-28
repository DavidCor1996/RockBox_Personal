#!/usr/bin/env python3
"""Build and install authentic Zelda3 assets from a user-owned US ROM."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASSET_TOOLS = ROOT / "apps/plugins/zelda3/assets-tools"
EXPECTED_SHA256 = (
    "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb"
)
ENGINE_COMMIT = "fbbb3f967a51fafe642e6140d0753979e73b4090"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def atomic_copy(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".part")
    shutil.copy2(source, temporary)
    os.replace(temporary, destination)


def make_cover(source: Path, destination: Path) -> None:
    try:
        from PIL import Image, ImageOps
    except ImportError as error:
        raise SystemExit(
            "Pillow is required for cover conversion: python3 -m pip install pillow"
        ) from error

    with Image.open(source) as image:
        image = ImageOps.exif_transpose(image).convert("RGB")
        fitted = ImageOps.contain(image, (144, 108), Image.Resampling.LANCZOS)
        canvas = Image.new("RGB", (144, 108), "black")
        canvas.paste(fitted, ((144 - fitted.width) // 2, (108 - fitted.height) // 2))
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = destination.with_name(destination.name + ".part.bmp")
        canvas.save(temporary, "BMP")
        os.replace(temporary, destination)


def extract_assets(rom: Path, destination: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="zelda3-assets-") as temporary:
        work = Path(temporary)
        tools = work / "assets"
        shutil.copytree(ASSET_TOOLS, tools)
        subprocess.run(
            [
                sys.executable,
                str(tools / "restool.py"),
                "--rom",
                str(rom),
                "--extract-from-rom",
            ],
            cwd=work,
            check=True,
        )
        generated = work / "zelda3_assets.dat"
        if not generated.is_file() or generated.stat().st_size < 1024:
            raise SystemExit("asset extraction did not produce zelda3_assets.dat")
        atomic_copy(generated, destination)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Extract stock A Link to the Past data from a legally-owned US ROM "
            "and install the native iPod port."
        )
    )
    parser.add_argument("--rom", type=Path, required=True, help="clean US .sfc ROM")
    parser.add_argument(
        "--cover",
        type=Path,
        required=True,
        help="authentic box art or game screenshot; no replacement art is generated",
    )
    parser.add_argument(
        "--target", type=Path, required=True, help="mounted iPod or simulator disk root"
    )
    parser.add_argument(
        "--build-dir", type=Path, default=ROOT / "build-hw-ipod6g"
    )
    args = parser.parse_args()

    rom = args.rom.expanduser().resolve()
    cover = args.cover.expanduser().resolve()
    target = args.target.expanduser().resolve()
    build = args.build_dir.expanduser().resolve()
    if not rom.is_file():
        raise SystemExit(f"ROM not found: {rom}")
    if not cover.is_file():
        raise SystemExit(f"cover not found: {cover}")
    if not target.is_dir() or not (target / ".rockbox").is_dir():
        raise SystemExit(f"target is not a Rockbox volume/simdisk: {target}")

    actual_hash = sha256(rom)
    if actual_hash != EXPECTED_SHA256:
        raise SystemExit(
            "unsupported ROM; expected the clean US release with SHA-256 "
            f"{EXPECTED_SHA256}, got {actual_hash}"
        )

    loader = build / "apps/plugins/zelda3.rock"
    overlay = build / "apps/plugins/zelda3/zelda3.ovl"
    for artifact in (loader, overlay):
        if not artifact.is_file():
            raise SystemExit(f"missing build artifact: {artifact}")

    data_dir = target / ".rockbox/zelda3"
    games_dir = target / ".rockbox/rocks/games"
    (data_dir / "saves").mkdir(parents=True, exist_ok=True)
    games_dir.mkdir(parents=True, exist_ok=True)
    extract_assets(rom, data_dir / "zelda3_assets.dat")
    make_cover(cover, data_dir / "cover.bmp")
    atomic_copy(loader, games_dir / "zelda3.rock")
    atomic_copy(overlay, games_dir / "zelda3.ovl")

    receipt = data_dir / "INSTALL.txt"
    receipt.write_text(
        "A Link to the Past native Rockbox port\n"
        f"Engine upstream commit: {ENGINE_COMMIT}\n"
        f"Source ROM SHA-256: {actual_hash}\n"
        "Artwork: user supplied; resized/letterboxed only for the iPod display.\n",
        encoding="utf-8",
    )
    if hasattr(os, "sync"):
        os.sync()
    print(f"Installed native Zelda3 port in {target}")
    print("Steam entry is enabled because plugin, assets, and cover are present.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
