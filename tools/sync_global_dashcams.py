#!/usr/bin/env python3
"""Install openly licensed, full-length global dashcam videos as MPEG files.

Each entry is a Wikimedia Commons original.  No video is clipped: the MPEG-2
program stream contains the complete published drive.  The output bitrate is
bounded for Rockbox mpegplayer and is roughly 1.7 Mb/s including audio.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import urllib.parse
import urllib.request
from pathlib import Path


SOURCES = (
    ("kose_estonia.mpg", "Dashcam video of driving through Kose in December 2021.webm"),
    ("kranj_slovenia.mpg", "On the Road- Driving through Kranj.webm"),
    ("plitvice_ljubljana.mpg", "Dashcam - Plitvice Lakes, Croatia to just south of Ljubljana, Slovenia - 06 June 2015.webm"),
    ("wonju_korea.mpg", "2020-04-16 원주시 도로주행.webm"),
    ("blenheim_havelock.mpg", "Driving from Blenheim to Havelock in the Marlborough Region of New Zealand.webm"),
    ("gaithersburg_maryland.mpg", "Driving from Leaman Farm Road to Game Preserve Road in Gaithersburg, Maryland (1 June 2026).webm"),
    ("oklahoma_i235.mpg", "I-235 North OKC Full Timelapse.webm"),
)


def commons_original_url(filename: str) -> str:
    """Resolve a file through Commons' API; Special:FilePath rejects bots."""
    query = urllib.parse.urlencode({
        "action": "query", "format": "json", "prop": "imageinfo",
        "titles": "File:" + filename, "iiprop": "url",
    })
    request = urllib.request.Request(
        "https://commons.wikimedia.org/w/api.php?" + query,
        headers={"User-Agent": "RockPodMaps/1.0 (offline personal device sync)"},
    )
    with urllib.request.urlopen(request) as response:
        pages = json.load(response)["query"]["pages"].values()
    for page in pages:
        info = page.get("imageinfo") or []
        if info:
            return info[0]["url"]
    raise RuntimeError(f"Commons file not found: {filename}")


def download(filename: str, destination: Path) -> None:
    request = urllib.request.Request(
        commons_original_url(filename),
        headers={"User-Agent": "RockPodMaps/1.0 (offline personal device sync)"},
    )
    with urllib.request.urlopen(request) as response, destination.open("wb") as output:
        while chunk := response.read(1024 * 1024):
            output.write(chunk)


def convert(source: Path, target: Path) -> None:
    temporary = target.with_suffix(target.suffix + ".tmp")
    command = [
        "ffmpeg", "-y", "-loglevel", "error", "-i", str(source),
        "-vf", "scale=320:240:force_original_aspect_ratio=decrease,"
               "pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=20",
        "-map", "0:v:0", "-map", "0:a:0?", "-c:v", "mpeg2video",
        "-pix_fmt", "yuv420p", "-bf", "0", "-g", "12", "-flags",
        "+low_delay", "-sc_threshold", "0", "-q:v", "2", "-maxrate",
        "1600k", "-bufsize", "800k", "-c:a", "mp2", "-ar", "44100",
        "-ac", "2", "-b:a", "112k", "-packetsize", "2048", "-f", "mpeg",
        str(temporary),
    ]
    subprocess.run(command, check=True)
    temporary.replace(target)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mount", type=Path, help="mounted iPod volume")
    parser.add_argument("--keep-sources", action="store_true")
    args = parser.parse_args()
    target_dir = args.mount / ".rockbox" / "maps" / "videos"
    cache_dir = target_dir / ".source-cache"
    target_dir.mkdir(parents=True, exist_ok=True)
    cache_dir.mkdir(exist_ok=True)
    for target_name, original_name in SOURCES:
        target = target_dir / target_name
        complete = target.with_suffix(target.suffix + ".complete")
        if target.is_file() and target.stat().st_size > 0 and complete.is_file():
            print(f"kept {target_name}")
            continue
        source = cache_dir / original_name
        if not source.is_file() or not source.stat().st_size:
            print(f"downloading {original_name}")
            download(original_name, source)
        print(f"encoding complete drive: {target_name}")
        convert(source, target)
        complete.touch()
        if not args.keep_sources:
            source.unlink(missing_ok=True)
    if not args.keep_sources:
        cache_dir.rmdir()


if __name__ == "__main__":
    main()
