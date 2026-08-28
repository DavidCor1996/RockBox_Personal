#!/usr/bin/env python3
"""Capture and validate the animated Clock District rooms in the simulator."""

from __future__ import annotations

import argparse
import os
import subprocess
import tempfile
import time
from pathlib import Path

from PIL import Image

from clubpenguin_hockey_sim_gate import (
    mean_difference,
    prepare_root,
    wait_for_frame,
)


ROOMS = (
    ("snow_forts", "snow_forts_frames", "clock-district.png"),
    ("velvet_ice", "velvet_ice_frames", "velvet-ice.png"),
    ("chrome_clinic", "chrome_clinic_frames", "chrome-clinic.png"),
)


def closest_strip_delta(frame: Image.Image, strip_path: Path) -> float:
    with Image.open(strip_path) as source:
        strip = source.convert("RGB")
    frame_count = strip.width // 320
    if frame_count < 1 or strip.width % 320:
        raise RuntimeError(f"invalid animation strip width: {strip.width}")
    return min(
        mean_difference(
            frame.crop((0, 0, 320, 220)),
            strip.crop((index * 320, 0, (index + 1) * 320, 220)),
        )
        for index in range(frame_count)
    )


def capture_room(repo: Path, output: Path, room: str, strip: str,
                 filename: str) -> None:
    with tempfile.TemporaryDirectory(prefix=f"clubpenguin-{room}-") as temp:
        root = Path(temp)
        build, rockbox = prepare_root(repo, root, room)
        frame_path = root / "frame.bmp"
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_AUDIODRIVER": "dummy",
                "SDL_VIDEODRIVER": "dummy",
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_HIDDEN": "1",
                "ROCKPOD_SIM_PREVIEW_BMP": str(frame_path),
                "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "25",
            }
        )
        process = subprocess.Popen(
            [
                str(build / "rockboxui"),
                "--zoom", "1",
                "--nobackground",
                "--root", str(root),
            ],
            cwd=build,
            env=environment,
        )
        try:
            wait_for_frame(frame_path, process)
            time.sleep(8)
            if process.poll() is not None:
                raise RuntimeError(f"plugin exited before {room} capture")
            with Image.open(frame_path) as source:
                first = source.convert("RGB")
            time.sleep(0.6)
            with Image.open(frame_path) as source:
                second = source.convert("RGB")

            animation_delta = mean_difference(
                first.crop((0, 0, 320, 220)),
                second.crop((0, 0, 320, 220)),
            )
            if animation_delta < 0.05:
                raise RuntimeError(f"{room} animation did not advance")

            strip_path = (
                rockbox / "rocks/games/clubpenguin/rooms/night_city" /
                strip / "strip.bmp"
            )
            frame_delta = closest_strip_delta(first, strip_path)
            if frame_delta > 42:
                raise RuntimeError(
                    f"{room} framebuffer differs unexpectedly "
                    f"({frame_delta:.1f})"
                )

            output.mkdir(parents=True, exist_ok=True)
            first.save(output / filename)
            print(
                f"PASS {room}: frame delta={frame_delta:.1f}; "
                f"animation delta={animation_delta:.2f}"
            )
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("test-artifacts/clubpenguin"),
    )
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    for room, strip, filename in ROOMS:
        capture_room(repo, args.output, room, strip, filename)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
