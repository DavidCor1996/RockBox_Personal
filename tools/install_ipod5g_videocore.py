#!/usr/bin/env python3
"""Extract the iPod Video 5G Apple VideoCore runtime for Rockbox.

Apple binaries are not redistributed with Rockbox.  This read-only extractor
accepts Apple's official ``iPod_13.1.3.ipsw``, pins its SHA-256, extracts the
intact iPodResources FAT volume, then installs only the verified VideoCore
files needed by ``video_playback_5g.c``.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import tempfile
import zipfile
from pathlib import Path


IPSW_SHA256 = (
    "66aad071f960061dcfbdfe69773a698a59b9635c18ba9cb4478f57fd69306cb7"
)
FIRMWARE_NAME = "Firmware-13.6.3"
PARTITION_TABLE_OFFSET = 0x4200
PARTITION_ENTRY = struct.Struct("<4sI4sIIIIIII")
RESOURCE_IMAGE_ID = int.from_bytes(b"crsr", "little")

VIDEOCORE_FILES = {
    "vmcs.bin": (
        "::/Resources/VIDEOCORE/Boot/vmcs.bin",
        201376,
        "cde4b5a9c9a020b6e23de1be0492d9a853b7d974c4060462d5f57a80e47f55be",
    ),
    "aacdec.vll": (
        "::/Resources/VIDEOCORE/Library/aacdec.vll",
        52664,
        "0a8c63080120a8af36a2d68ed4ade7eae81eee15f1e344f66cd49a508162c2cc",
    ),
    "h264dec.vll": (
        "::/Resources/VIDEOCORE/Library/h264dec.vll",
        106960,
        "3915ec9295dc0d9cf9405810a66788e5b544c819347e05a350df6857120a9e8f",
    ),
    "mpg4dec.vll": (
        "::/Resources/VIDEOCORE/Library/mpg4dec.vll",
        147232,
        "08ba2f33f267803746c3b3de025e615f4a4b899611d7a4e3eea819f39d3697fe",
    ),
    "mplayer.vll": (
        "::/Resources/VIDEOCORE/Library/mplayer.vll",
        51620,
        "c100e0cb0782277f81d2b9bceb29340df70d2993e34f77248a8e3c1979bc1fc6",
    ),
    "passthruhandler.vll": (
        "::/Resources/VIDEOCORE/Library/passthruhandler.vll",
        6528,
        "0b657b1a4b50198da3ad3d55ecf9d239ffbae8d7da7b3aaca138dcdae5b6d019",
    ),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def require_hash(path: Path, expected: str, label: str) -> None:
    actual = sha256(path)
    if actual != expected:
        raise SystemExit(
            f"refusing unrecognized {label}: expected sha256={expected}, "
            f"got {actual}"
        )


def extract_resource_volume(ipsw: Path, destination: Path) -> None:
    with zipfile.ZipFile(ipsw) as archive:
        payloads = [
            name for name in archive.namelist() if name.startswith("Firmware-")
        ]
        if payloads != [FIRMWARE_NAME]:
            raise SystemExit(f"unexpected Apple firmware payloads: {payloads!r}")
        firmware = archive.read(FIRMWARE_NAME)

    for index in range(20):
        fields = PARTITION_ENTRY.unpack_from(
            firmware, PARTITION_TABLE_OFFSET + index * PARTITION_ENTRY.size
        )
        magic, image_id = fields[:2]
        device_offset, length = fields[3], fields[4]
        if magic != b"!ATA":
            break
        if image_id != RESOURCE_IMAGE_ID:
            continue
        start = device_offset + 512
        end = start + length
        if length <= 0 or end > len(firmware):
            raise SystemExit("Apple iPodResources partition has invalid bounds")
        destination.write_bytes(firmware[start:end])
        return
    raise SystemExit("Apple iPodResources partition was not found")


def extract_videocore(resource_volume: Path, staging: Path) -> None:
    mcopy = shutil.which("mcopy")
    if mcopy is None:
        raise SystemExit("mcopy is required (install the mtools package)")
    for output_name, (source_name, _size, _digest) in VIDEOCORE_FILES.items():
        subprocess.run(
            [mcopy, "-o", "-i", str(resource_volume), source_name,
             str(staging / output_name)],
            check=True,
        )


def verify_videocore(directory: Path) -> None:
    for name, (_source, expected_size, expected_hash) in VIDEOCORE_FILES.items():
        path = directory / name
        if not path.is_file() or path.stat().st_size != expected_size:
            actual = path.stat().st_size if path.exists() else "missing"
            raise SystemExit(
                f"invalid Apple VideoCore file {name}: expected "
                f"{expected_size} bytes, got {actual}"
            )
        require_hash(path, expected_hash, f"Apple VideoCore file {name}")


def install_videocore(staging: Path, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    for name in VIDEOCORE_FILES:
        source = staging / name
        temporary = output / f".{name}.rockpod-new"
        shutil.copyfile(source, temporary)
        os.replace(temporary, output / name)

    lines = [
        "Private Apple iPod Video 5G VideoCore extraction",
        "",
        "Source: official Apple iPod_13.1.3.ipsw",
        f"IPSW SHA-256: {IPSW_SHA256}",
        "Method: read-only extraction from the intact iPodResources volume",
        "No Apple binary is stored in or distributed by the Rockbox source tree.",
        "",
        "Installed files:",
    ]
    for name, (_source, _size, digest) in VIDEOCORE_FILES.items():
        lines.append(f"{name}\t{digest}")
    lines.append("")
    provenance = output / ".PROVENANCE.txt.rockpod-new"
    provenance.write_text("\n".join(lines), encoding="utf-8")
    os.replace(provenance, output / "PROVENANCE.txt")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ipsw", type=Path,
                        help="official Apple iPod_13.1.3.ipsw")
    parser.add_argument(
        "output", type=Path,
        help="destination directory (normally DEVICE/.rockbox/videocore)",
    )
    args = parser.parse_args()

    ipsw = args.ipsw.expanduser().resolve()
    output = args.output.expanduser().resolve()
    if not ipsw.is_file():
        raise SystemExit(f"IPSW not found: {ipsw}")
    require_hash(ipsw, IPSW_SHA256, "Apple iPod_13.1.3.ipsw")

    with tempfile.TemporaryDirectory(prefix="ipod5g-videocore-") as temp_name:
        temporary = Path(temp_name)
        resource_volume = temporary / "ipod-resources.fat"
        staging = temporary / "videocore"
        staging.mkdir()
        extract_resource_volume(ipsw, resource_volume)
        extract_videocore(resource_volume, staging)
        verify_videocore(staging)
        install_videocore(staging, output)

    verify_videocore(output)
    print(f"installed {len(VIDEOCORE_FILES)} verified files in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
