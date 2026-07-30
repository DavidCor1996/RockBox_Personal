#!/usr/bin/env python3
"""Prepare a validated, external iPod Hero skin without network access."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import struct
import tempfile


ASSETS = {
    "background.bmp": ("backgrounds/live-rock-band-night.320x240x24.bmp", 320, 240),
    "background-alt.bmp": ("backgrounds/the-killers-concert.320x240x24.bmp", 320, 240),
    "highway.bmp": ("gameplay/highway.200x220x24.bmp", 200, 220),
    "logo.bmp": ("logos/guitar-hero-2005.240x80x24.bmp", 240, 80),
    "gems.bmp": ("gameplay/gems-normal.160x128x24.bmp", 160, 128),
    "gems-hopo.bmp": ("gameplay/gems-hopo.160x128x24.bmp", 160, 128),
    "gems-star.bmp": ("gameplay/gems-star.160x128x24.bmp", 160, 128),
    "hit-rings.bmp": ("gameplay/hit-rings.160x64x24.bmp", 160, 64),
    "hit-flames.bmp": ("gameplay/hit-flames.160x32x24.bmp", 160, 32),
    "hud-panel.bmp": ("gameplay/hud-panel.128x28x24.bmp", 128, 28),
    "rock-meter.bmp": ("gameplay/rock-meter.64x44x24.bmp", 64, 44),
    "star-meter.bmp": ("gameplay/star-meter.80x40x24.bmp", 80, 40),
    "sustains.bmp": ("gameplay/sustains.80x16x24.bmp", 80, 16),
    "results-stars.bmp": ("gameplay/results-stars.150x60x24.bmp", 150, 60),
}

BACKGROUNDS = {
    "live-rock-band-night": "backgrounds/live-rock-band-night.320x240x24.bmp",
    "foo-fighters-live": "backgrounds/foo-fighters-live.320x240x24.bmp",
    "iron-maiden-denver": "backgrounds/iron-maiden-denver.320x240x24.bmp",
    "the-killers-concert": "backgrounds/the-killers-concert.320x240x24.bmp",
    "the-beatles-belfast-1964":
        "backgrounds/the-beatles-belfast-1964.320x240x24.bmp",
    "the-beatles-treslong-1964":
        "backgrounds/the-beatles-treslong-1964.320x240x24.bmp",
    "oliver-tree-oakland-2019":
        "backgrounds/oliver-tree-oakland-2019.320x240x24.bmp",
    "oliver-tree-sydney-2020":
        "backgrounds/oliver-tree-sydney-2020.320x240x24.bmp",
    "emma-blackery-manchester-2016":
        "backgrounds/emma-blackery-manchester-2016.320x240x24.bmp",
    "emma-blackery-manchester-2016-b":
        "backgrounds/emma-blackery-manchester-2016-b.320x240x24.bmp",
    "cage-the-elephant-bonnaroo-2017":
        "backgrounds/cage-the-elephant-bonnaroo-2017.320x240x24.bmp",
    "cage-the-elephant-big-snow-show-2018":
        "backgrounds/cage-the-elephant-big-snow-show-2018.320x240x24.bmp",
}

ALT_BACKGROUNDS = {
    "live-rock-band-night": "the-killers-concert",
    "foo-fighters-live": "live-rock-band-night",
    "iron-maiden-denver": "the-killers-concert",
    "the-killers-concert": "live-rock-band-night",
    "the-beatles-belfast-1964": "the-beatles-treslong-1964",
    "the-beatles-treslong-1964": "the-beatles-belfast-1964",
    "oliver-tree-oakland-2019": "oliver-tree-sydney-2020",
    "oliver-tree-sydney-2020": "oliver-tree-oakland-2019",
    "emma-blackery-manchester-2016": "emma-blackery-manchester-2016-b",
    "emma-blackery-manchester-2016-b": "emma-blackery-manchester-2016",
    "cage-the-elephant-bonnaroo-2017": "cage-the-elephant-big-snow-show-2018",
    "cage-the-elephant-big-snow-show-2018": "cage-the-elephant-bonnaroo-2017",
}

DEFAULT_SKIN_NAME = "Live Stage"

MANIFEST_BASE = """IHS1
name={name}
background=background.bmp
background_alt=background-alt.bmp
highway=highway.bmp
logo=logo.bmp
gems=gems.bmp
hopo=gems-hopo.bmp
star=gems-star.bmp
rings=hit-rings.bmp
flames=hit-flames.bmp
hud=hud-panel.bmp
rock_meter=rock-meter.bmp
star_meter=star-meter.bmp
sustains=sustains.bmp
results_stars=results-stars.bmp
"""

CRC_KEYS = {
    "background.bmp": "background_crc32",
    "background-alt.bmp": "background_alt_crc32",
    "highway.bmp": "highway_crc32",
    "logo.bmp": "logo_crc32",
    "gems.bmp": "gems_crc32",
    "gems-hopo.bmp": "hopo_crc32",
    "gems-star.bmp": "star_crc32",
    "hit-rings.bmp": "rings_crc32",
    "hit-flames.bmp": "flames_crc32",
    "hud-panel.bmp": "hud_crc32",
    "rock-meter.bmp": "rock_meter_crc32",
    "star-meter.bmp": "star_meter_crc32",
    "sustains.bmp": "sustains_crc32",
    "results-stars.bmp": "results_stars_crc32",
}


def bmp_dimensions(path: Path) -> tuple[int, int, int]:
    with path.open("rb") as handle:
        header = handle.read(30)
    if len(header) != 30 or header[:2] != b"BM":
        raise ValueError(f"not a Windows BMP: {path}")
    width, height = struct.unpack_from("<ii", header, 18)
    bits = struct.unpack_from("<H", header, 28)[0]
    return width, abs(height), bits


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def rockbox_crc32(path: Path) -> int:
    crc = 0xFFFFFFFF
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            for byte in block:
                crc ^= byte << 24
                for _ in range(8):
                    if crc & 0x80000000:
                        crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
                    else:
                        crc = (crc << 1) & 0xFFFFFFFF
    return crc


def check_skin_name(name: str) -> str:
    """The runtime stores the skin name in a 48 byte field."""
    if not name.strip() or any(character in name for character in "\r\n="):
        raise ValueError(f"invalid skin name: {name!r}")
    if len(name.encode("utf-8")) > 47:
        raise ValueError(f"skin name is longer than 47 bytes: {name!r}")
    return name


def write_skin_manifest(root: Path, name: str) -> None:
    lines = [MANIFEST_BASE.format(name=check_skin_name(name))]
    for filename, key in CRC_KEYS.items():
        lines.append(f"{key}={rockbox_crc32(root / filename):08x}\n")
    (root / "skin.ihs").write_text("".join(lines), encoding="utf-8")


def selected_assets(background: str):
    assets = dict(ASSETS)
    assets["background.bmp"] = (BACKGROUNDS[background], 320, 240)
    alternate = ALT_BACKGROUNDS[background]
    assets["background-alt.bmp"] = (BACKGROUNDS[alternate], 320, 240)
    return assets


def validate_source(source: Path, background: str) -> None:
    decoded = 0
    for _, (relative, width, height) in selected_assets(background).items():
        path = source / relative
        if not path.is_file():
            raise FileNotFoundError(f"missing required source: {path}")
        actual_width, actual_height, bits = bmp_dimensions(path)
        if (actual_width, actual_height) != (width, height) or bits != 24:
            raise ValueError(
                f"unexpected BMP {path}: {actual_width}x{actual_height}x{bits}"
            )
        decoded += width * height * 2
    if decoded > 1024 * 1024:
        raise ValueError(f"decoded skin is {decoded} bytes; limit is 1048576")


def write_hashes(root: Path) -> None:
    lines = []
    for path in sorted(root.iterdir()):
        if path.is_file() and path.name != "SHA256SUMS":
            lines.append(f"{sha256(path)}  {path.name}\n")
    (root / "SHA256SUMS").write_text("".join(lines), encoding="utf-8")


def verify(root: Path) -> None:
    sums = root / "SHA256SUMS"
    if not sums.is_file():
        raise FileNotFoundError(f"missing {sums}")
    for line in sums.read_text(encoding="utf-8").splitlines():
        expected, name = line.split("  ", 1)
        path = root / name
        if not path.is_file() or sha256(path) != expected:
            raise ValueError(f"checksum mismatch: {path}")
    for output, (_, width, height) in ASSETS.items():
        actual_width, actual_height, bits = bmp_dimensions(root / output)
        if (actual_width, actual_height, bits) != (width, height, 24):
            raise ValueError(f"invalid installed bitmap: {output}")
    manifest = {}
    lines = (root / "skin.ihs").read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "IHS1":
        raise ValueError("invalid skin manifest")
    for line in lines[1:]:
        key, separator, value = line.partition("=")
        if separator:
            manifest[key] = value
    check_skin_name(manifest.get("name", ""))
    for filename, key in CRC_KEYS.items():
        if manifest.get(key) != f"{rockbox_crc32(root / filename):08x}":
            raise ValueError(f"runtime CRC mismatch: {filename}")


def install(source: Path, output: Path, force: bool, background: str,
            name: str) -> None:
    validate_source(source, background)
    check_skin_name(name)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and not force:
        raise FileExistsError(f"output already exists (use --force): {output}")
    temporary = Path(tempfile.mkdtemp(prefix=f".{output.name}.", dir=output.parent))
    backup = output.with_name(f".{output.name}.backup")
    try:
        for filename, (relative, _, _) in selected_assets(background).items():
            shutil.copy2(source / relative, temporary / filename)
        write_skin_manifest(temporary, name)
        local_root = source.parent
        provenance = local_root / "PROVENANCE.md"
        license_file = local_root / "source" / "yarg" / "LICENSE"
        if provenance.is_file():
            shutil.copy2(provenance, temporary / "provenance.txt")
        if license_file.is_file():
            shutil.copy2(license_file, temporary / "LICENSE-YARG.txt")
        write_hashes(temporary)
        verify(temporary)
        if backup.exists():
            shutil.rmtree(backup)
        if output.exists():
            os.replace(output, backup)
        try:
            os.replace(temporary, output)
        except Exception:
            if backup.exists() and not output.exists():
                os.replace(backup, output)
            raise
        if backup.exists():
            shutil.rmtree(backup)
    except Exception:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, help="prepared local asset directory")
    parser.add_argument("--output", type=Path, required=True, help="skin output directory")
    parser.add_argument("--verify", action="store_true", help="verify output only")
    parser.add_argument("--force", action="store_true", help="replace an existing output")
    parser.add_argument("--background", choices=BACKGROUNDS,
                        default="live-rock-band-night",
                        help="licensed concert photograph for this skin")
    parser.add_argument("--name", default=DEFAULT_SKIN_NAME,
                        help="skin name recorded in skin.ihs (max 47 bytes)")
    args = parser.parse_args()

    if args.verify:
        verify(args.output)
        print(f"verified iPod Hero skin: {args.output}")
        return 0
    if args.source is None:
        parser.error("--source is required unless --verify is used")
    install(args.source, args.output, args.force, args.background, args.name)
    print(f"installed iPod Hero skin: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
