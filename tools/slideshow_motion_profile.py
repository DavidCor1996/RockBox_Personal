#!/usr/bin/env python3
"""Profile right-pane slideshow motion from a simulator capture."""

from __future__ import annotations

import math
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:  # pragma: no cover - tool-time dependency check
    raise SystemExit(f"missing Pillow: {exc}") from exc


def frame_diff(a: Image.Image, b: Image.Image) -> float:
    pa = a.convert("L").tobytes()
    pb = b.convert("L").tobytes()
    if not pa or len(pa) != len(pb):
        return math.inf
    return sum(abs(x - y) for x, y in zip(pa, pb)) / len(pa)


def frame_mean(frame: Image.Image) -> float:
    pixels = frame.convert("L").tobytes()
    if not pixels:
        return 0.0
    return sum(pixels) / len(pixels)


def extract_frames(video: Path, out_dir: Path) -> list[Path]:
    pattern = out_dir / "frame_%05d.png"
    subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(video),
            "-vf",
            "crop=iw/2:ih:iw/2:0",
            str(pattern),
        ],
        check=True,
    )
    return sorted(out_dir.glob("frame_*.png"))


def profile(video: Path) -> dict[str, str]:
    if not shutil.which("ffmpeg"):
        raise SystemExit("missing ffmpeg")

    with tempfile.TemporaryDirectory(prefix="slideshow-motion-") as tmp:
        frames = extract_frames(video, Path(tmp))
        images = [Image.open(path).convert("RGB") for path in frames]
        diffs = [
            frame_diff(images[i - 1], images[i])
            for i in range(1, len(images))
        ]
        means = [frame_mean(image) for image in images]

    duplicate_threshold = 0.35
    flash_mean_threshold = 4.0
    duplicate_count = sum(1 for value in diffs if value <= duplicate_threshold)
    black_count = sum(1 for value in means if value <= flash_mean_threshold)
    avg_diff = sum(diffs) / len(diffs) if diffs else 0.0
    max_diff = max(diffs) if diffs else 0.0
    duplicate_ratio = duplicate_count / len(diffs) if diffs else 0.0
    black_ratio = black_count / len(means) if means else 0.0

    return {
        "motion_frames": str(len(images)),
        "motion_adjacent_pairs": str(len(diffs)),
        "motion_duplicate_frames": str(duplicate_count),
        "motion_duplicate_ratio": f"{duplicate_ratio:.4f}",
        "motion_black_frames": str(black_count),
        "motion_black_ratio": f"{black_ratio:.4f}",
        "motion_avg_absdiff": f"{avg_diff:.4f}",
        "motion_max_absdiff": f"{max_diff:.4f}",
        "motion_duplicate_threshold": f"{duplicate_threshold:.2f}",
        "motion_black_mean_threshold": f"{flash_mean_threshold:.2f}",
    }


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(
            "usage: slideshow_motion_profile.py <capture.mp4> <motion-report.txt>",
            file=sys.stderr,
        )
        return 2

    video = Path(argv[1])
    report = Path(argv[2])
    data = profile(video)
    report.write_text(
        "".join(f"{key}={value}\n" for key, value in data.items()),
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
