#!/usr/bin/env python3
"""Build the complete public Moncton KartaView drive for Rockbox MPEG."""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import subprocess
import tempfile
import urllib.parse
import urllib.request
from datetime import datetime
from pathlib import Path


SEQUENCE_ID = "2058866"
EXPECTED_FRAMES = 652
API_URL = "https://api.openstreetcam.org/2.0/photo/"
USER_AGENT = "RockPodMaps/2.1 (offline personal device sync)"
TARGET_NAME = "moncton_full_drive.mpg"
SOURCE_FPS = 4
OUTPUT_FPS = 24


def request_json(parameters: dict[str, object]) -> dict:
    url = API_URL + "?" + urllib.parse.urlencode(parameters)
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def complete_manifest() -> list[dict]:
    records: dict[int, dict] = {}
    for page in range(1, 6):
        payload = request_json({
            "sequenceId": SEQUENCE_ID,
            "page": page,
            "itemsPerPage": 150,
        })
        for record in payload.get("result", {}).get("data", []):
            records[int(record["sequenceIndex"])] = record
        if not payload.get("result", {}).get("hasMoreData"):
            break
    ordered = [records[index] for index in sorted(records)]
    if len(ordered) != EXPECTED_FRAMES:
        raise RuntimeError(
            f"expected {EXPECTED_FRAMES} frames, received {len(ordered)}"
        )
    if [int(item["sequenceIndex"]) for item in ordered] != list(
        range(EXPECTED_FRAMES)
    ):
        raise RuntimeError("Moncton sequence is not contiguous")
    if any(item.get("visibility") != "public" for item in ordered):
        raise RuntimeError("Moncton sequence contains a non-public frame")
    return ordered


def download_frame(job: tuple[int, dict, Path]) -> Path:
    index, record, directory = job
    destination = directory / f"{index:04d}.jpg"
    request = urllib.request.Request(
        record["imageLthUrl"], headers={"User-Agent": USER_AGENT}
    )
    with urllib.request.urlopen(request, timeout=60) as response, \
            destination.open("wb") as output:
        while chunk := response.read(256 * 1024):
            output.write(chunk)
    if destination.stat().st_size < 10_000:
        raise RuntimeError(f"incomplete frame {index}")
    return destination


def timestamp(record: dict) -> datetime:
    return datetime.strptime(record["shotDate"], "%Y-%m-%d %H:%M:%S.%f")


def frame_step(records: list[dict], index: int) -> float:
    del records, index
    # KartaView exposes only 652 stills for this complete route.  Replaying
    # their capture timestamps holds many frames for several seconds and is
    # unpleasant on an iPod.  Keep every frame and compress only the missing
    # time between them; offline motion interpolation below then produces a
    # continuous 24 fps drive without increasing decoder work on-device.
    return 1.0 / SOURCE_FPS


def write_concat(records: list[dict], frames: list[Path], target: Path) -> float:
    duration = 0.0
    lines = []
    for index, frame in enumerate(frames):
        step = frame_step(records, index)
        duration += step
        lines.append(f"file '{frame}'\n")
        lines.append(f"duration {step:.3f}\n")
    lines.append(f"file '{frames[-1]}'\n")
    target.write_text("".join(lines), encoding="utf-8")
    return duration


def media_duration(path: Path) -> float:
    result = subprocess.run([
        "ffprobe", "-v", "error", "-show_entries", "format=duration",
        "-of", "default=noprint_wrappers=1:nokey=1", str(path),
    ], check=True, capture_output=True, text=True)
    return float(result.stdout.strip())


def encode(manifest: Path, destination: Path) -> None:
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    subprocess.run([
        "ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0",
        "-i", str(manifest), "-vf",
        "scale=320:240:force_original_aspect_ratio=decrease,"
        "pad=320:240:(ow-iw)/2:(oh-ih)/2:black,"
        f"minterpolate=fps={OUTPUT_FPS}:mi_mode=mci:mc_mode=aobmc:"
        "me_mode=bidir:vsbmc=1",
        "-map", "0:v:0", "-c:v", "mpeg2video", "-pix_fmt", "yuv420p",
        "-bf", "0", "-g", "12", "-flags", "+low_delay", "-sc_threshold",
        "0", "-q:v", "3", "-maxrate", "1400k", "-bufsize", "700k",
        "-packetsize", "2048", "-f", "mpeg", str(temporary),
    ], check=True)
    os.replace(temporary, destination)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mount", type=Path, help="mounted iPod or simdisk")
    parser.add_argument("--workers", type=int, default=12)
    args = parser.parse_args()
    destination = args.mount / ".rockbox" / "maps" / "videos" / TARGET_NAME
    destination.parent.mkdir(parents=True, exist_ok=True)
    records = complete_manifest()
    expected = sum(frame_step(records, index) for index in range(len(records)))
    if destination.is_file() and abs(media_duration(destination) - expected) <= 3:
        destination.with_suffix(destination.suffix + ".complete").write_text(
            f"KartaView sequence {SEQUENCE_ID}; {EXPECTED_FRAMES} frames; "
            f"{media_duration(destination):.3f}s\n",
            encoding="utf-8",
        )
        print(f"kept complete {TARGET_NAME} ({expected:.0f}s)")
        return 0
    with tempfile.TemporaryDirectory(prefix="moncton-full-drive.") as temporary:
        directory = Path(temporary)
        jobs = [(index, record, directory) for index, record in enumerate(records)]
        frames: list[Path] = [directory / "missing"] * len(jobs)
        with concurrent.futures.ThreadPoolExecutor(
            max_workers=max(1, args.workers)
        ) as pool:
            futures = {pool.submit(download_frame, job): job[0] for job in jobs}
            completed = 0
            for future in concurrent.futures.as_completed(futures):
                index = futures[future]
                frames[index] = future.result()
                completed += 1
                if completed % 50 == 0 or completed == len(jobs):
                    print(f"downloaded {completed}/{len(jobs)} frames", flush=True)
        concat = directory / "frames.txt"
        reconstructed = write_concat(records, frames, concat)
        print(
            f"encoding all {EXPECTED_FRAMES} frames as a motion-smoothed "
            f"{reconstructed:.0f}s drive",
            flush=True,
        )
        encode(concat, destination)
    actual = media_duration(destination)
    if abs(actual - expected) > 3:
        raise RuntimeError(
            f"incomplete Moncton video: expected {expected:.1f}s, got {actual:.1f}s"
        )
    destination.with_suffix(destination.suffix + ".complete").write_text(
        f"KartaView sequence {SEQUENCE_ID}; {EXPECTED_FRAMES} frames; {actual:.3f}s\n",
        encoding="utf-8",
    )
    print(f"installed {destination} ({actual:.0f}s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
