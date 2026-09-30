#!/usr/bin/env python3
"""Visually qualify the scaled OS X notification used by Desktop Mode."""

from __future__ import annotations

import argparse
import os
import subprocess
import tempfile
import time
from pathlib import Path

from desktop_mode_sim_gate import (
    PACK,
    PLUGIN_PATH,
    ROOT,
    SESSION_TOKEN,
    bmp_pixel,
    bmp_rgb,
    near,
    stage,
    wait_for_frame,
    write_pointer,
)


def gray_interior_pixels(frame: Path) -> int:
    width, _, pixels = bmp_rgb(frame)
    count = 0
    for y in range(25, 55):
        for x in range(168, 313):
            red, green, blue = pixels[y * width + x]
            if min(red, green, blue) >= 180 and max(red, green, blue) - min(
                red, green, blue
            ) <= 35:
                count += 1
    return count


def icon_color_count(frame: Path) -> int:
    width, _, pixels = bmp_rgb(frame)
    colors = set()
    for y in range(29, 51):
        for x in range(171, 193):
            red, green, blue = pixels[y * width + x]
            colors.add((red // 16, green // 16, blue // 16))
    return len(colors)


def run_gate(build: Path, output: Path, timeout: float) -> None:
    output.mkdir(parents=True, exist_ok=True)
    live_capture = output / ".desktop-notification-live.bmp"
    notification = output / "desktop-notification-frame.bmp"
    log_path = output / "desktop-notification-simulator.log"
    aurora = PACK / "320x240/desktop/aurora.320x240x16.bmp"
    expected = {
        point: bmp_pixel(aurora, *point)
        for point in ((8, 30), (8, 90), (60, 70), (110, 150))
    }

    with tempfile.TemporaryDirectory(prefix="desktop-notification-sim-") as temp:
        sim_root = Path(temp) / "simdisk"
        stage(build, sim_root)
        pointer = sim_root / ".rockbox/host-pointer"
        write_pointer(pointer, 40, 100)
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_VIDEODRIVER": "dummy",
                "SDL_AUDIODRIVER": "dummy",
                "ROCKPOD_SIM_HIDDEN": "1",
                "ROCKPOD_SIM_PREVIEW_BMP": str(live_capture),
                "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "40",
                "ROCKPOD_SIM_HOST_POINTER": str(pointer),
                "ROCKPOD_SIM_NOTIFICATION_TEST": "desktop",
                "ROCKBOX_SIM_PLUGIN": PLUGIN_PATH,
                "ROCKBOX_SIM_PLUGIN_PARAM": SESSION_TOKEN,
            }
        )
        with log_path.open("wb") as log:
            process = subprocess.Popen(
                [
                    str(build / "rockboxui"),
                    "--nobackground",
                    "--root",
                    str(sim_root),
                ],
                cwd=build,
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        try:
            # SDL initializes its host-pointer record once. Replace that
            # sentinel before the desktop finishes loading so the wheel-help
            # tooltip never contaminates the notification capture.
            time.sleep(0.25)
            write_pointer(pointer, 40, 100)

            def desktop_ready(frame: Path) -> bool:
                if process.poll() is not None:
                    raise SystemExit(
                        f"Desktop simulator exited early ({process.returncode}); "
                        f"see {log_path}"
                    )
                return (
                    all(
                        near(bmp_pixel(frame, *point), color)
                        for point, color in expected.items()
                    )
                    and gray_interior_pixels(frame) > 1000
                    and icon_color_count(frame) > 12
                )

            wait_for_frame(
                live_capture,
                notification,
                desktop_ready,
                time.monotonic() + timeout,
                "upper-right Desktop notification",
            )
            if gray_interior_pixels(notification) < 2200:
                raise SystemExit("Desktop notification did not render a gray bubble")
            if icon_color_count(notification) <= 12:
                raise SystemExit("Desktop notification did not use its real source icon")
            for x, y in ((160, 40), (200, 60), (315, 60)):
                if not near(bmp_pixel(notification, x, y), bmp_pixel(aurora, x, y)):
                    raise SystemExit(
                        "Desktop notification escaped its compact upper-right bounds"
                    )
            print(
                "Desktop notification simulator gate passed: compact upper-right "
                f"Apple-style bubble with a real source icon: {notification}"
            )
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build", type=Path, default=ROOT / "build-sim-ipod6g"
    )
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/desktop-notification-sim-gate")
    )
    parser.add_argument("--timeout", type=float, default=25.0)
    args = parser.parse_args()
    run_gate(args.build.resolve(), args.output.resolve(), args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
