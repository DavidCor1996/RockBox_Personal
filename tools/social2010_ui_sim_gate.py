#!/usr/bin/env python3
"""Capture Instagram/Reddit landing and feed screens in the iPod 6G simulator."""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from rockboy_profile_gate import (
    LANG_START_SCREEN,
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


BUTTON_GATES: dict[str, Path] = {}


def write_cstring(buffer: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")[: size - 1]
    buffer[offset : offset + len(encoded)] = encoded


def start_screen_lang_id(build_dir: Path) -> int:
    enum = build_dir / "lang_enum.h"
    if enum.is_file():
        for line in enum.read_text(encoding="utf-8", errors="replace").splitlines():
            if "LANG_START_SCREEN," in line and "/*" in line:
                return int(line.split("/*", 1)[1].split("*/", 1)[0].strip())
    return LANG_START_SCREEN


def wait_for_frame(frame: Path, timeout: float = 20.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if frame.is_file() and frame.stat().st_size >= 300_000:
            return
        time.sleep(0.1)
    raise RuntimeError("simulator framebuffer capture timed out")


def window_for(pid: int) -> str:
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        result = subprocess.run(["xdotool", "search", "--pid", str(pid)], capture_output=True, text=True, check=False)
        windows = result.stdout.splitlines()
        if windows:
            return windows[-1]
        time.sleep(0.2)
    raise RuntimeError("simulator window not found")


def tap(window: str, key: str) -> None:
    gate = BUTTON_GATES.get(key)
    if gate is not None:
        gate.touch()
        time.sleep(0.08)
        gate.unlink(missing_ok=True)
        time.sleep(0.8)
        return
    subprocess.run(["xdotool", "keydown", "--window", window, key], check=True)
    time.sleep(0.08)
    subprocess.run(["xdotool", "keyup", "--window", window, key], check=True)
    time.sleep(0.8)


def prepare_root(build: Path, root: Path, app: str, data: Path) -> None:
    repo = build.parent
    source = build / "simdisk/.rockbox"
    rockbox = root / ".rockbox"
    rockbox.mkdir(parents=True)
    for name in ("fonts", "langs", "icons"):
        if (source / name).is_dir():
            shutil.copytree(source / name, rockbox / name)
    shutil.copytree(data, rockbox / app)
    logo_source = repo / f"assets/ipodjs/rockbox/{app}/{app}-logo-official.40x40.bmp"
    logo_target = rockbox / app / "assets" / f"{app}-logo.bmp"
    if logo_source.is_file():
        logo_target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(logo_source, logo_target)
    if app == "instagram":
        launch_source = repo / "assets/ipodjs/rockbox/instagram/instagram-launch-2010.bmp"
        launch_target = rockbox / app / "assets/instagram-launch-2010.bmp"
        if launch_source.is_file():
            launch_target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(launch_source, launch_target)
    plugin_path = f"/.rockbox/rocks/apps/{app}.rock"
    plugin_target = root / plugin_path.lstrip("/")
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / f"apps/plugins/{app}.rock", plugin_target)

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build) or OPEN_PLUGIN_CHECKSUM
    struct.pack_into("<IiI", entry, 0, START_SCREEN_HASH, start_screen_lang_id(build), checksum)
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, f"{app}.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, plugin_path)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, "")
    (rockbox / "rocks/plugin.dat").write_bytes(entry)
    (rockbox / "config.cfg").write_text(
        "start in screen: plugin\n"
        "resume: off\n"
        "tagcache_autoupdate: off\n"
        "weather sync notifications: off\n",
        encoding="utf-8",
    )


def capture(app: str, build: Path, data: Path, output: Path) -> None:
    with tempfile.TemporaryDirectory(prefix=f"{app}-2010-ui-") as temporary:
        root = Path(temporary)
        expected_group_like = None
        if app == "instagram":
            for line in (data / "library.tsv").read_text(
                encoding="utf-8", errors="replace"
            ).splitlines()[1:]:
                fields = line.split("\t")
                if len(fields) >= 14 and int(fields[13] or "1") > 1:
                    expected_group_like = fields[11]
                    break
        prepare_root(build, root, app, data)
        frame = root / "frame.bmp"
        gate_root = root / ".button-gates"
        gate_root.mkdir()
        BUTTON_GATES.clear()
        BUTTON_GATES.update({
            "KP_5": gate_root / "select.gate",
            "KP_Decimal": gate_root / "menu.gate",
            "KP_Add": gate_root / "play.gate",
            "KP_2": gate_root / "scroll-forward.gate",
            "KP_8": gate_root / "scroll-back.gate",
            "KP_4": gate_root / "left.gate",
            "KP_6": gate_root / "right.gate",
        })
        headless = str(os.environ.get("ROCKPOD_SIM_HEADLESS", "")).lower() in {
            "1", "true", "yes",
        }
        environment = os.environ.copy()
        environment.update({
            "SDL_AUDIODRIVER": "dummy",
            "SDL_VIDEODRIVER": "dummy" if headless else "x11",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_HIDDEN": "1" if headless else "0",
            "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
            "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "25",
            "ROCKPOD_SIM_SELECT_GATE": str(BUTTON_GATES["KP_5"]),
            "ROCKPOD_SIM_MENU_GATE": str(BUTTON_GATES["KP_Decimal"]),
            "ROCKPOD_SIM_PLAY_ACTION_GATE": str(BUTTON_GATES["KP_Add"]),
            "ROCKPOD_SIM_SCROLL_FWD_GATE": str(BUTTON_GATES["KP_2"]),
            "ROCKPOD_SIM_SCROLL_BACK_GATE": str(BUTTON_GATES["KP_8"]),
            "ROCKPOD_SIM_LEFT_GATE": str(BUTTON_GATES["KP_4"]),
            "ROCKPOD_SIM_RIGHT_GATE": str(BUTTON_GATES["KP_6"]),
        })
        process = subprocess.Popen([str(build / "rockboxui"), "--zoom", "1", "--nobackground", "--root", str(root)], cwd=build, env=environment)
        try:
            wait_for_frame(frame)
            # The start-screen plugin runs after the normal iPod boot frame.
            # The plugin starts after Rockbox's boot frame and storage init.
            # Leave enough margin for slower hosts before taking UI evidence.
            time.sleep(7.0)
            output.mkdir(parents=True, exist_ok=True)
            first_name = "home" if app == "instagram" else "landing"
            if app != "instagram":
                shutil.copy2(frame, output / f"{app}-{first_name}.bmp")
            window = "" if headless else window_for(process.pid)
            if app == "instagram":
                # Exercise feed wheel navigation in both directions before
                # opening the selected post's author page.
                tap(window, "KP_2")
                tap(window, "KP_8")
                shutil.copy2(frame, output / "instagram-home.bmp")
            tap(window, "KP_6" if app == "instagram" else "KP_5")
            wait_for_frame(frame)
            time.sleep(2.0)
            second_name = "profile" if app == "instagram" else "feed"
            shutil.copy2(frame, output / f"{app}-{second_name}.bmp")
            if app == "instagram":
                tap(window, "KP_5")
                wait_for_frame(frame)
                time.sleep(1.0)
                shutil.copy2(frame, output / "instagram-viewer-1x.bmp")
                tap(window, "KP_6")
                wait_for_frame(frame)
                time.sleep(1.0)
                shutil.copy2(frame, output / "instagram-viewer-next.bmp")
                tap(window, "KP_5")
                wait_for_frame(frame)
                time.sleep(1.0)
                shutil.copy2(frame, output / "instagram-viewer-zoom.bmp")
                tap(window, "KP_Add")
                likes = root / ".rockbox/instagram/likes.tsv"
                deadline = time.monotonic() + 3.0
                while time.monotonic() < deadline and not likes.is_file():
                    time.sleep(0.1)
                liked_text = likes.read_text(
                    encoding="utf-8", errors="replace"
                ) if likes.is_file() else ""
                if "ig_" not in liked_text:
                    raise RuntimeError("Instagram like was not persisted")
                if expected_group_like and expected_group_like not in liked_text:
                    raise RuntimeError("Instagram carousel like was not stored by group")
                time.sleep(1.0)
                shutil.copy2(frame, output / "instagram-viewer-liked.bmp")
                tap(window, "KP_Decimal")
                tap(window, "KP_Decimal")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("app", choices=("instagram", "reddit"))
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("/tmp/social2010-ui"))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    capture(args.app, repo / "build-sim-ipod6g", args.data, args.output)
    if args.app == "instagram":
        print(args.output / "instagram-home.bmp")
        print(args.output / "instagram-profile.bmp")
        print(args.output / "instagram-viewer-1x.bmp")
        print(args.output / "instagram-viewer-next.bmp")
        print(args.output / "instagram-viewer-zoom.bmp")
        print(args.output / "instagram-viewer-liked.bmp")
    else:
        print(args.output / f"{args.app}-landing.bmp")
        print(args.output / f"{args.app}-feed.bmp")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
