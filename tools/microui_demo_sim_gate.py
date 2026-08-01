#!/usr/bin/env python3
"""Exercise microUI focus, activation, and page rendering in the iPod sim."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from rockachievements_ui_sim_gate import (
    BUTTON_GATES,
    capture,
    changed_pixels,
    start_screen_lang_id,
    tap,
    wait_for_file,
    window_id,
    write_cstring,
)
from rockboy_profile_gate import (
    OPEN_PLUGIN_CHECKSUM,
    OPEN_PLUGIN_ENTRY_SIZE,
    OPEN_PLUGIN_NAME_OFFSET,
    OPEN_PLUGIN_NAME_SIZE,
    OPEN_PLUGIN_PARAM_OFFSET,
    OPEN_PLUGIN_PARAM_SIZE,
    OPEN_PLUGIN_PATH_OFFSET,
    OPEN_PLUGIN_PATH_SIZE,
    START_SCREEN_HASH,
    open_plugin_lang_checksum,
)


PLUGIN_PATH = "/.rockbox/rocks/demos/microui_demo.rock"


def wait_for_demo(frame: Path, timeout: float = 20.0) -> None:
    wait_for_file(frame)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = subprocess.run(
            ["magick", str(frame), "-format", "%[pixel:p{300,10}]", "info:"],
            check=False,
            capture_output=True,
            text=True,
        )
        match = re.search(r"\((\d+),(\d+),(\d+)", result.stdout)
        if match:
            red, green, blue = (int(value) for value in match.groups())
            if 24 <= red <= 130 and max(red, green, blue) - min(
                    red, green, blue) <= 4:
                return
        time.sleep(0.2)
    raise SystemExit("microUI demo startup timed out")


def install(build: Path, root: Path) -> None:
    source = build / "simdisk/.rockbox"
    rockbox = root / ".rockbox"

    rockbox.mkdir(parents=True)
    for name in ("fonts", "langs", "icons"):
        if (source / name).is_dir():
            shutil.copytree(source / name, rockbox / name)
    target = root / PLUGIN_PATH.lstrip("/")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / "apps/plugins/microui_demo/microui_demo.rock",
                 target)

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build)
    struct.pack_into(
        "<IiI", entry, 0, START_SCREEN_HASH, start_screen_lang_id(build),
        checksum or OPEN_PLUGIN_CHECKSUM,
    )
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE,
                  "microui_demo.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE,
                  PLUGIN_PATH)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, "")
    (rockbox / "rocks/plugin.dat").write_bytes(entry)
    (rockbox / "config.cfg").write_text(
        "start in screen: plugin\nresume: off\ntagcache_autoupdate: off\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path,
                        default=Path("build-sim-ipod6g"))
    parser.add_argument("--output", type=Path,
                        default=Path("/tmp/microui-demo-gate"))
    args = parser.parse_args()
    build = args.build_dir.resolve()
    simulator = build / "rockboxui"

    if not simulator.is_file():
        raise SystemExit(f"missing simulator: {simulator}")
    for command in ("magick", "xdotool"):
        if shutil.which(command) is None:
            raise SystemExit(f"missing required command: {command}")
    args.output.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="microui-demo-") as temporary:
        root = Path(temporary)
        install(build, root)
        frame = root / "frame.bmp"
        gate_root = args.output / ".button-gates"
        shutil.rmtree(gate_root, ignore_errors=True)
        gate_root.mkdir()
        BUTTON_GATES.clear()
        BUTTON_GATES.update({
            "KP_5": gate_root / "select.gate",
            "KP_Decimal": gate_root / "menu.gate",
            "KP_2": gate_root / "scroll-forward.gate",
            "KP_8": gate_root / "scroll-back.gate",
        })
        environment = os.environ.copy()
        environment.update({
            "SDL_AUDIODRIVER": "dummy",
            "SDL_VIDEODRIVER": "x11",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
            "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
            "ROCKPOD_SIM_SELECT_GATE": str(BUTTON_GATES["KP_5"]),
            "ROCKPOD_SIM_MENU_GATE": str(BUTTON_GATES["KP_Decimal"]),
            "ROCKPOD_SIM_SCROLL_FWD_GATE": str(BUTTON_GATES["KP_2"]),
            "ROCKPOD_SIM_SCROLL_BACK_GATE": str(BUTTON_GATES["KP_8"]),
        })
        with (args.output / "simulator.log").open("wb") as log:
            process = subprocess.Popen(
                [str(simulator), "--zoom", "1", "--nobackground",
                 "--root", str(root)],
                cwd=build,
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        try:
            window_id(process.pid)
            wait_for_demo(frame)
            time.sleep(0.5)
            landing = args.output / "widgets.png"
            stress = args.output / "stress.png"
            diagnostics = args.output / "diagnostics.png"
            capture(frame, landing)

            # Title drag handle is first, followed by Widgets and Stress.
            tap(process.pid, "KP_2")
            tap(process.pid, "KP_2")
            tap(process.pid, "KP_5")
            capture(frame, stress)
            if changed_pixels(landing, stress) < 20_000:
                raise SystemExit("wheel focus did not activate the stress page")

            # Stable ID reconciliation keeps Stress selected; Diagnostics is next.
            tap(process.pid, "KP_2")
            tap(process.pid, "KP_5")
            capture(frame, diagnostics)
            if changed_pixels(stress, diagnostics) < 20_000:
                raise SystemExit("stable focus did not activate diagnostics")
            tap(process.pid, "KP_Decimal")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

    print(f"microUI demo simulator gate passed; captures in {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
