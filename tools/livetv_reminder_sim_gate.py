#!/usr/bin/env python3
"""Focused simulator gate for the DIRECTV upcoming-program reminder sheet."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

import livetv_guide_sim_gate as guide


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path,
                        default=Path("build-sim-ipod6g"))
    parser.add_argument("--output", type=Path,
                        default=Path("/tmp/livetv-reminder-gate"))
    args = parser.parse_args()

    repo = Path(__file__).resolve().parent.parent
    build_dir = args.build_dir.resolve()
    simulator = build_dir / "rockboxui"
    args.output.mkdir(parents=True, exist_ok=True)
    gate_root = args.output / ".button-gates"
    shutil.rmtree(gate_root, ignore_errors=True)
    gate_root.mkdir(parents=True)

    with tempfile.TemporaryDirectory(prefix="livetv-reminder-") as temp:
        root = Path(temp)
        frame = root / "frame.bmp"
        guide.prepare_root(repo, build_dir, root)
        guide.BUTTON_GATES.clear()
        guide.BUTTON_GATES.update({
            "KP_5": gate_root / "select.gate",
            "KP_Decimal": gate_root / "menu.gate",
        })
        environment = os.environ.copy()
        environment.update({
            "SDL_AUDIODRIVER": "disk",
            "SDL_DISKAUDIOFILE": str(args.output / "audio.raw"),
            "SDL_VIDEODRIVER": "x11",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
            "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
            "ROCKPOD_SIM_SELECT_GATE": str(guide.BUTTON_GATES["KP_5"]),
            "ROCKPOD_SIM_MENU_GATE":
                str(guide.BUTTON_GATES["KP_Decimal"]),
        })
        process = subprocess.Popen(
            [str(simulator), "--zoom", "1", "--nobackground",
             "--root", str(root)], cwd=build_dir, env=environment,
        )
        try:
            guide.window_id(process.pid)
            guide.wait_for_guide(frame)
            time.sleep(0.5)
            before = args.output / "guide.png"
            guide.capture(frame, before)

            guide.tap(process.pid, "Right")
            time.sleep(0.5)
            guide.tap(process.pid, "KP_5")
            time.sleep(0.6)
            reminder = args.output / "upcoming-program.png"
            guide.capture(frame, reminder)

            if guide.changed_pixels(before, reminder) < 5_000:
                raise SystemExit("future programme did not open reminder sheet")
            if not guide.close_enough(guide.pixel(reminder, 50, 66),
                                      (254, 196, 37), 32):
                raise SystemExit("DIRECTV gold reminder header is missing")
            if not guide.close_enough(guide.pixel(reminder, 50, 90),
                                      (2, 111, 175), 40):
                raise SystemExit("DIRECTV blue reminder body is missing")

            guide.tap(process.pid, "KP_5")
            deadline = time.monotonic() + 4.0
            state = root / ".rockbox" / "notifications" / "state.v1.dat"
            while time.monotonic() < deadline and not state.is_file():
                time.sleep(0.1)
            if not state.is_file() or state.stat().st_size < 64:
                raise SystemExit("setting the reminder did not persist it")
        finally:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)

    print(f"Live TV reminder gate passed: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
