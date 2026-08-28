#!/usr/bin/env python3
"""Capture and validate the animated Arasaka Headquarters rooms."""

from __future__ import annotations

import argparse
from pathlib import Path

from clubpenguin_clock_district_sim_gate import capture_room


ROOMS = (
    (
        "dojo_courtyard",
        "dojo_courtyard_frames",
        "arasaka-headquarters.png",
    ),
    ("dojo", "dojo_frames", "arasaka-lobby.png"),
)


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
