#!/usr/bin/env python3
"""Automatically capture Rockbox simulator screenshots while you play.

Designed for Minish Cap validation runs where manual play is needed but
screenshots should be collected automatically.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path


def run_cmd(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, check=False, capture_output=True, text=True)


def find_window_id(window_title: str) -> str:
    result = run_cmd(["xdotool", "search", "--name", window_title])
    if result.returncode != 0 or not result.stdout.strip():
        return ""

    ids = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    return ids[-1] if ids else ""


def sanitize_tag(tag: str) -> str:
    tag = tag.strip().lower()
    tag = re.sub(r"[^a-z0-9._-]+", "_", tag)
    return tag.strip("_") or "frame"


def file_sha1(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def capture_window(window_id: str, path: Path) -> bool:
    result = subprocess.run(
        ["import", "-silent", "-window", window_id, str(path)],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return result.returncode == 0


def read_new_transitions(trace_file: Path, state: dict[str, int]) -> list[str]:
    tags: list[str] = []
    if not trace_file.exists():
        return tags

    with trace_file.open("r", encoding="utf-8", errors="replace") as f:
        f.seek(state["offset"])
        for line in f:
            if "transition post" not in line:
                continue
            idx_match = re.search(r"idx=(\d+)", line)
            room_match = re.search(r"to_room=(\d+)", line)
            idx = idx_match.group(1) if idx_match else "x"
            room = room_match.group(1) if room_match else "x"
            tags.append(f"transition_idx{idx}_room{room}")
        state["offset"] = f.tell()

    return tags


def next_name(counter: int, tag: str) -> str:
    ts = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
    return f"{counter:05d}_{ts}_{sanitize_tag(tag)}.png"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Auto-capture Rockbox simulator screenshots")
    parser.add_argument(
        "--output-dir",
        default="/tmp/minishcap_visual/auto",
        help="Directory for captured PNG files",
    )
    parser.add_argument(
        "--window-title",
        default="iPod Video",
        help="Simulator window title to capture",
    )
    parser.add_argument(
        "--window-id",
        default="",
        help="Explicit X11 window id (skips title lookup)",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=0.35,
        help="Capture interval in seconds",
    )
    parser.add_argument(
        "--trace-file",
        default="build-sim-video-5g/simdisk/.rockbox/rocks/games/minishcap_trace.log",
        help="Optional trace log path for transition-tagged captures",
    )
    parser.add_argument(
        "--allow-duplicates",
        action="store_true",
        help="Keep identical consecutive frames",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    out_dir = Path(args.output_dir).expanduser()
    out_dir.mkdir(parents=True, exist_ok=True)

    trace_file = Path(args.trace_file)
    trace_state = {"offset": 0}

    print(f"Output dir: {out_dir}")
    print(f"Window title: {args.window_title}")
    print("Press Ctrl+C to stop capture.")

    window_id = args.window_id.strip()
    if not window_id:
        while True:
            window_id = find_window_id(args.window_title)
            if window_id:
                break
            print("Waiting for simulator window...")
            time.sleep(0.5)

    print(f"Capturing window id: {window_id}")

    counter = 1
    last_hash = ""

    try:
        while True:
            transition_tags = read_new_transitions(trace_file, trace_state)

            tags = ["periodic"]
            if transition_tags:
                tags.extend(transition_tags)

            captured = 0
            for tag in tags:
                temp_path = out_dir / f".tmp_{counter:05d}.png"
                if not capture_window(window_id, temp_path):
                    temp_path.unlink(missing_ok=True)
                    continue

                current_hash = file_sha1(temp_path)
                if not args.allow_duplicates and current_hash == last_hash:
                    temp_path.unlink(missing_ok=True)
                    continue

                final_path = out_dir / next_name(counter, tag)
                temp_path.rename(final_path)
                last_hash = current_hash
                counter += 1
                captured += 1

            if captured:
                print(f"Captured {captured} frame(s), total={counter - 1}")

            time.sleep(max(0.05, args.interval))
    except KeyboardInterrupt:
        print("Stopped.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
