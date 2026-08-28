#!/usr/bin/env python3
"""Atomically refresh mounted iPod TikTok MPEGs from preserved originals."""

import argparse
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


def video_fps(path: Path) -> str:
    result = subprocess.run(
        [
            "ffprobe", "-v", "error", "-select_streams", "v:0",
            "-show_entries", "stream=avg_frame_rate", "-of",
            "default=nw=1:nk=1", str(path),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def transcode(source: Path, destination: Path) -> tuple[str, str]:
    if video_fps(destination) in {"30/1", "60/2", "90/3"}:
        return destination.name, "current"

    temporary = destination.with_suffix(destination.suffix + ".fullfps.tmp")
    command = [
        "ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-y",
        "-i", str(source),
        "-vf",
        "scale=136:240:force_original_aspect_ratio=decrease,"
        "pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=30",
        "-map", "0:v:0", "-map", "0:a:0?", "-c:v", "mpeg2video",
        "-pix_fmt", "yuv420p", "-bf", "0", "-g", "12", "-flags",
        "+low_delay", "-q:v", "8", "-maxrate", "900k", "-bufsize",
        "512k", "-c:a", "mp2", "-ar", "44100", "-ac", "2", "-b:a",
        "96k", "-f", "mpeg", str(temporary),
    ]
    try:
        subprocess.run(command, check=True)
        if video_fps(temporary) != "30/1":
            raise RuntimeError("encoded output is not 30 fps")
        os.replace(temporary, destination)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise
    return destination.name, "updated"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mount", type=Path)
    parser.add_argument(
        "--sources", type=Path,
        default=Path.home() / ".rockpod" / "tiktok",
    )
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()

    media_root = args.mount.resolve() / "TikTok" / "videos"
    if not media_root.is_dir() or not args.sources.is_dir():
        parser.error("TikTok source or mounted media directory is missing")

    originals = {
        path.stem: path
        for path in args.sources.rglob("*.mp4")
        if path.is_file()
    }
    tasks = []
    missing = []
    for destination in sorted(media_root.glob("tt_*.mpg")):
        source = originals.get(destination.stem.removeprefix("tt_"))
        if source is None:
            missing.append(destination.name)
        else:
            tasks.append((source, destination))
    if missing:
        print(f"Hard stop: {len(missing)} device videos lack originals")
        return 2

    updated = 0
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as executor:
        futures = [executor.submit(transcode, *task) for task in tasks]
        for completed, future in enumerate(as_completed(futures), 1):
            name, result = future.result()
            updated += result == "updated"
            print(f"[{completed}/{len(tasks)}] {result}: {name}", flush=True)

    print(f"Verified {len(tasks)} TikToks at 30 fps; updated {updated}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
