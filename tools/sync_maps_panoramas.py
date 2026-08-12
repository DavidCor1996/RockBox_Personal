#!/usr/bin/env python3
"""Download and project the licensed 360 panoramas shipped with Maps."""

from __future__ import annotations

import subprocess
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "assets" / "nb_maps"

# slug, Panoramax item, SD asset URL, source-center azimuth.
PANORAMAS = (
    ("amsterdam", "914edd2f-3efc-4727-953d-c0d52c626f83",
     "https://nl.panoramax.xyz/derivates/91/4e/dd/2f/3efc-4727-953d-c0d52c626f83/sd.jpg", 0),
    ("san_francisco", "0a56dd3b-ddbd-450b-83ff-155e35d88a0b",
     "https://pano.locus.sbs/api/pictures/0a56dd3b-ddbd-450b-83ff-155e35d88a0b/sd.jpg", 276),
    ("lisbon", "4f3087d2-8860-4db5-aa87-fab3f0af5482",
     "https://panoramax-storage-public-fast.s3.gra.perf.cloud.ovh.net/derivates/4f/30/87/d2/8860-4db5-aa87-fab3f0af5482/sd.jpg", 0),
    ("paris", "a43ea4d5-1c2a-4255-9ce9-8ef774bc45f3",
     "https://panoramax-storage-public-fast.s3.gra.perf.cloud.ovh.net/derivates/a4/3e/a4/d5/1c2a-4255-9ce9-8ef774bc45f3/sd.jpg", 282),
    ("lyon", "e1ea9aeb-e274-44d1-8c4f-8fc6ec38365d",
     "https://panoramax.openstreetmap.fr/derivates/e1/ea/9a/eb/e274-44d1-8c4f-8fc6ec38365d/sd.jpg", 274),
    ("bordeaux", "a3655dac-7d3e-47cd-9ecd-0bee54470e68",
     "https://panoramax.openstreetmap.fr/derivates/a3/65/5d/ac/7d3e-47cd-9ecd-0bee54470e68/sd.jpg", 268),
)

HEADINGS = (("north", 0), ("east", 90), ("south", 180), ("west", 270))


def download(url: str, destination: Path) -> None:
    temporary = destination.with_suffix(destination.suffix + ".download")
    request = urllib.request.Request(
        url, headers={"User-Agent": "RockPodMaps/2.0 (offline asset sync)"}
    )
    with urllib.request.urlopen(request, timeout=60) as response, \
            temporary.open("wb") as output:
        while chunk := response.read(1024 * 1024):
            output.write(chunk)
    temporary.replace(destination)


def project(source: Path, destination: Path, yaw: int) -> None:
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    subprocess.run([
        "ffmpeg", "-y", "-loglevel", "error", "-i", str(source),
        "-vf", ("v360=input=equirect:output=flat:"
                f"yaw={yaw}:pitch=0:h_fov=100:v_fov=70:w=320:h=184"),
        "-pix_fmt", "rgb565le", "-f", "rawvideo", str(temporary),
    ], check=True)
    if temporary.stat().st_size != 320 * 184 * 2:
        raise RuntimeError(f"invalid projected frame: {destination.name}")
    temporary.replace(destination)


def main() -> None:
    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    for slug, item_id, url, azimuth in PANORAMAS:
        source = ASSET_DIR / f"{slug}_360_source.jpg"
        if not source.is_file() or not source.stat().st_size:
            print(f"downloading {slug} ({item_id})", flush=True)
            download(url, source)
        for heading, bearing in HEADINGS:
            destination = ASSET_DIR / f"{slug}_360_{heading}.rgb"
            yaw = (bearing - azimuth + 180) % 360 - 180
            print(f"projecting {destination.name}", flush=True)
            project(source, destination, yaw)


if __name__ == "__main__":
    main()
