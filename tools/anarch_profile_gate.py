#!/usr/bin/env python3
"""Validate an Anarch physical-iPod profiling log."""

from __future__ import annotations

import argparse
from pathlib import Path


MINIMUM_FRAMES = 30 * 60 * 5
MAXIMUM_INPUT_QUEUE = 16


def parse_profile(path: Path) -> dict[str, str]:
    lines = path.read_text(encoding="utf-8", errors="strict").splitlines()
    if not lines or lines[0] != "ANARCH_PROFILE_V1":
        raise ValueError("unknown or missing Anarch profile header")
    return dict(line.split("=", 1) for line in lines[1:] if "=" in line)


def number(values: dict[str, str], name: str) -> int:
    try:
        return int(values[name])
    except (KeyError, ValueError) as error:
        raise ValueError(f"missing or invalid {name}") from error


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", type=Path,
                        help="path to .rockbox/games/anarch/profile.log")
    args = parser.parse_args()

    try:
        values = parse_profile(args.profile)
        checks = {
            "balanced profile": values.get("profile") == "balanced",
            "160x120 scaled output":
                values.get("resolution") == "160x120@2x",
            "30 fps target": number(values, "target_fps") == 30,
            "five-minute frame count":
                number(values, "frames") >= MINIMUM_FRAMES,
            "render samples present": number(values, "render_samples") > 0,
            "p95 within frame budget":
                number(values, "render_p95_percent") <= 100,
            "no late frames": number(values, "late_frames") == 0,
            "LCD samples present": number(values, "lcd_samples") > 0,
            "bounded input queue":
                number(values, "input_queue_worst") <= MAXIMUM_INPUT_QUEUE,
            "no audio underruns": number(values, "audio_underruns") == 0,
            "framebuffer guards intact":
                number(values, "framebuffer_guards") == 1,
            "expected arena use": number(values, "arena_used") >= 153600,
            "arena headroom remains": number(values, "arena_free") > 0,
        }
    except (OSError, UnicodeError, ValueError) as error:
        print(f"FAIL: {error}")
        return 2

    for label, passed in checks.items():
        print(f"{'PASS' if passed else 'FAIL'}: {label}")
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
