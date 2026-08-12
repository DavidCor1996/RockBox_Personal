#!/usr/bin/env python3
"""Download and convert the licensed central Moncton KartaView sequence."""

from __future__ import annotations

import os
import tempfile
import urllib.request
from pathlib import Path

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "assets" / "nb_maps"
FRAME_SIZE = (320, 184)
BASE_URL = (
    "https://storage13.openstreetcam.org/files/photo/2019/11/29/proc/"
)

# Public KartaView sequence 2058866, sequence indices 228..242. These eight
# frames follow one continuous central Moncton drive at two-second intervals.
FRAMES = (
    (559545074, "2058866_4_9cd60_29.jpg"),
    (559545082, "2058866_4_9cd60_31.jpg"),
    (559545090, "2058866_4_9cd60_33.jpg"),
    (559545098, "2058866_4_9cd60_35.jpg"),
    (559545106, "2058866_4_9cd60_37.jpg"),
    (559545114, "2058866_4_9cd60_39.jpg"),
    (559545122, "2058866_4_9cd60_41.jpg"),
    (559545130, "2058866_4_9cd60_43.jpg"),
)


def download(url: str, destination: Path) -> None:
    request = urllib.request.Request(
        url, headers={"User-Agent": "RockPodMaps/2.0 (offline asset sync)"}
    )
    fd, temporary = tempfile.mkstemp(
        prefix=destination.name + ".", suffix=".download",
        dir=destination.parent,
    )
    try:
        with os.fdopen(fd, "wb") as output:
            with urllib.request.urlopen(request, timeout=60) as response:
                while chunk := response.read(1024 * 1024):
                    output.write(chunk)
        os.replace(temporary, destination)
        destination.chmod(0o644)
    except Exception:
        try:
            os.close(fd)
        except OSError:
            pass
        try:
            os.unlink(temporary)
        except OSError:
            pass
        raise


def convert(source: Path, destination: Path) -> None:
    with Image.open(source) as image:
        image = ImageOps.fit(
            image.convert("RGB"), FRAME_SIZE,
            method=Image.Resampling.LANCZOS,
        )
        payload = bytearray(FRAME_SIZE[0] * FRAME_SIZE[1] * 2)
        offset = 0
        for red, green, blue in image.getdata():
            value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            payload[offset] = value & 0xFF
            payload[offset + 1] = value >> 8
            offset += 2
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    temporary.write_bytes(payload)
    if temporary.stat().st_size != FRAME_SIZE[0] * FRAME_SIZE[1] * 2:
        raise RuntimeError(f"invalid RGB565 frame: {destination.name}")
    temporary.replace(destination)


def main() -> None:
    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    for index, (photo_id, filename) in enumerate(FRAMES):
        source = ASSET_DIR / f"moncton_dashcam_{index}.jpg"
        destination = ASSET_DIR / f"moncton_dashcam_{index}.rgb"
        if not source.is_file() or not source.stat().st_size:
            print(f"downloading KartaView photo {photo_id}", flush=True)
            download(BASE_URL + filename, source)
        print(f"converting {destination.name}", flush=True)
        convert(source, destination)


if __name__ == "__main__":
    main()
