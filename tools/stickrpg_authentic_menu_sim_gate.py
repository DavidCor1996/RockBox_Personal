#!/usr/bin/env python3
"""Boot the unmodified Stick RPG SWF through its authored simulator menu."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import time


REQUIRED_LOG_MARKERS = (
    "stickrpg policy profile=1 fast=1 prime=0 shortcut=0",
    "stickrpg installed host doneIntro callback",
    "gameswf root ready",
    "stickrpg host doneIntro pregame=1 black=1 intro=1",
    "stickrpg host intro finalized black_frame=29 pregame=1",
    "stickrpg authored UI ready",
    "stickrpg authored UI presented",
    "stickrpg cursor snap dir=0,-1",
    "stickrpg authored UI state",
)


class GateError(RuntimeError):
    """The authentic-menu simulator gate did not pass."""


def regular(path: Path, description: str) -> Path:
    path = path.resolve()
    if not path.is_file():
        raise GateError(f"{description} is missing: {path}")
    return path


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    raw = path.read_bytes()
    parts = raw.split(b"\n", 3)
    if len(parts) != 4 or parts[0] != b"P6" or parts[2] != b"255":
        raise GateError(f"unexpected framebuffer format: {path}")
    dimensions = parts[1].split()
    if len(dimensions) != 2:
        raise GateError(f"invalid framebuffer dimensions: {path}")
    width, height = (int(value) for value in dimensions)
    pixels = parts[3]
    if len(pixels) != width * height * 3:
        raise GateError(f"truncated framebuffer: {path}")
    return width, height, pixels


def validate_authored_background(path: Path) -> None:
    width, height, pixels = read_ppm(path)
    if (width, height) != (320, 240):
        raise GateError(
            f"expected a 320x240 iPod framebuffer, got {width}x{height}"
        )

    blue_pixels = 0
    blue_colors: set[bytes] = set()
    for offset in range(0, len(pixels), 3):
        red, green, blue = pixels[offset : offset + 3]
        if blue >= 120 and blue > red + 30 and blue > green:
            blue_pixels += 1
            blue_colors.add(pixels[offset : offset + 3])

    # The authored menu background covers the display with several distinct
    # blue layers. This rejects a black/blank/crash frame while the runtime
    # marker below proves that the actual menu clips are visible.
    if blue_pixels < width * height * 3 // 4:
        raise GateError(
            f"authored blue background is incomplete: {blue_pixels} pixels"
        )
    if len(blue_colors) < 3:
        raise GateError(
            f"authored background lost its color layers: {len(blue_colors)}"
        )


def wait_for_artifacts(
    process: subprocess.Popen[bytes],
    log_path: Path,
    frame_path: Path,
    timeout: float,
) -> str:
    deadline = time.monotonic() + timeout
    latest = ""
    while time.monotonic() < deadline:
        if log_path.is_file():
            latest = log_path.read_text(encoding="utf-8", errors="replace")
        if all(marker in latest for marker in REQUIRED_LOG_MARKERS):
            if frame_path.is_file() and frame_path.stat().st_size > 0:
                return latest
        if process.poll() is not None:
            raise GateError(
                f"simulator exited early with status {process.returncode}"
            )
        time.sleep(0.1)
    missing = [marker for marker in REQUIRED_LOG_MARKERS if marker not in latest]
    raise GateError(
        "timed out waiting for authentic menu; missing: " + ", ".join(missing)
    )


def run_gate(build_dir: Path, timeout: float) -> None:
    build_dir = build_dir.resolve()
    simulator = regular(build_dir / "rockboxui", "Rockbox simulator")
    plugin = regular(
        build_dir / "apps/plugins/flashplayer/flashplayer.rock",
        "built flashplayer plugin",
    )
    simdisk = build_dir / "simdisk"
    swf = regular(
        simdisk / ".rockbox/flash/stickrpg/stickrpg.swf",
        "staged Stick RPG SWF",
    )
    installed_plugin = (
        simdisk / ".rockbox/rocks/viewers/flashplayer.rock"
    )
    installed_plugin.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin, installed_plugin)

    flash_dir = simdisk / ".rockbox/flash"
    log_path = flash_dir / "flashplayer.log"
    frame_path = flash_dir / "frame.ppm"
    log_path.unlink(missing_ok=True)
    frame_path.unlink(missing_ok=True)

    env = os.environ.copy()
    env.update(
        {
            "ROCKBOX_SIM_PLUGIN": "/.rockbox/rocks/viewers/flashplayer.rock",
            "ROCKBOX_SIM_PLUGIN_PARAM": (
                "/.rockbox/flash/stickrpg/" + swf.name
            ),
            "FLASHPLAYER_PRIME_STICKRPG": "0",
            "FLASHPLAYER_STICKRPG_SHORTCUT": "0",
            "FLASHPLAYER_AUTORUN_FRAMES": "120",
            "FLASHPLAYER_AUTORUN_SCROLL_FRAME": "60",
            "FLASHPLAYER_AUTORUN_SCROLL_DIRECTION": "-1",
            "FLASHPLAYER_AUTORUN_SELECT_FRAME": "75",
            "FLASHPLAYER_DUMP_FRAME": "80",
        }
    )
    process = subprocess.Popen(
        [str(simulator)],
        cwd=build_dir,
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        log = wait_for_artifacts(process, log_path, frame_path, timeout)
        ready_line = next(
            (
                line
                for line in log.splitlines()
                if "stickrpg authored UI ready" in line
            ),
            "",
        )
        if "gameswf error:" in log or "errors=0" not in ready_line:
            raise GateError("GameSWF reported a runtime error")
        validate_authored_background(frame_path)
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

    print(
        "Stick RPG authentic-menu simulator gate passed: "
        f"{log_path} and {frame_path}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build",
        type=Path,
        default=Path("build-sim-ipod6g"),
        help="configured iPod 6G simulator build directory",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=45.0,
        help="seconds to wait for the authentic menu",
    )
    args = parser.parse_args()
    try:
        run_gate(args.build, args.timeout)
    except GateError as error:
        parser.exit(1, f"stickrpg authentic-menu gate: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
