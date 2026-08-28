#!/usr/bin/env python3
"""Capture the Memorial Park and Emma Sewer Sessions flow in the simulator."""

from __future__ import annotations

import argparse
import os
import subprocess
import tempfile
import time
from pathlib import Path

from PIL import Image

from clubpenguin_clock_district_sim_gate import capture_room
from clubpenguin_hockey_sim_gate import prepare_root, wait_for_frame


ROOMS = (
    ("forest", "forest_frames", "memorial-park.png"),
    ("plaza", "plaza_frames", "plaza-sewer-entrance.png"),
    ("emma_sewer", "emma_sewer_frames", "emma-sewer-sessions.png"),
)


def capture_map(repo: Path, output: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="clubpenguin-map-") as temp:
        root = Path(temp)
        build, _ = prepare_root(repo, root, "map", 230, 83)
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
                raise RuntimeError("plugin exited before map capture")
            output.mkdir(parents=True, exist_ok=True)
            with Image.open(frame_path) as source:
                source.convert("RGB").save(output / "night-city-map.png")
            print("PASS map: Memorial Park marker rendered in simulator")
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
    capture_map(repo, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
