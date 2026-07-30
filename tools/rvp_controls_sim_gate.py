#!/usr/bin/env python3
"""Exercise RVP pause, seek, volume, resume, and exit on a real bundle."""

from __future__ import annotations

import argparse
import os
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
    hold,
    tap,
    wait_for_file,
    window_id,
)
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


PLUGIN_PATH = "/.rockbox/rocks/apps/openh264_player.rock"
VIDEO_PATH = "/Videos/Downloaded/rvp-controls-test.rvp"


def write_cstring(
    buffer: bytearray, offset: int, size: int, value: str
) -> None:
    encoded = value.encode("utf-8")[: size - 1]
    buffer[offset : offset + len(encoded)] = encoded


def start_screen_lang_id(build_dir: Path) -> int:
    lang_enum = build_dir / "lang_enum.h"
    if lang_enum.is_file():
        for line in lang_enum.read_text(
            encoding="utf-8", errors="replace"
        ).splitlines():
            if "LANG_START_SCREEN," in line and "/*" in line:
                return int(line.split("/*", 1)[1].split("*/", 1)[0])
    return LANG_START_SCREEN


def referenced_sidecars(marker: Path) -> list[Path]:
    paths = []
    for raw_line in marker.read_text(
        encoding="utf-8", errors="strict"
    ).splitlines():
        if "=" not in raw_line:
            continue
        key, value = raw_line.split("=", 1)
        if key == "video" or key == "audio" or (
            key.startswith("segment")
            and key.endswith(("_video", "_audio"))
        ):
            paths.append(marker.parent / value)
    return paths


def prepare_root(build_dir: Path, root: Path, marker: Path) -> None:
    source_rockbox = build_dir / "simdisk" / ".rockbox"
    built_plugin = build_dir / "apps/plugins/openh264_player.rock"
    if not built_plugin.is_file():
        raise SystemExit(f"missing simulator plugin: {built_plugin}")
    if not marker.is_file():
        raise SystemExit(f"missing RVP marker: {marker}")

    sidecars = referenced_sidecars(marker)
    if not sidecars or any(not path.is_file() for path in sidecars):
        raise SystemExit("RVP marker has missing video/audio sidecars")

    rockbox = root / ".rockbox"
    rockbox.mkdir(parents=True)
    for name in ("fonts", "langs", "icons"):
        source = source_rockbox / name
        if source.is_dir():
            shutil.copytree(source, rockbox / name)
    apple_assets = (
        Path(__file__).resolve().parents[1] / "assets/ipodjs/apple"
    )
    if apple_assets.is_dir():
        shutil.copytree(apple_assets, rockbox / "ipodjs/apple")

    plugin = root / PLUGIN_PATH.lstrip("/")
    plugin.parent.mkdir(parents=True)
    shutil.copy2(built_plugin, plugin)

    target_marker = root / VIDEO_PATH.lstrip("/")
    target_marker.parent.mkdir(parents=True)
    shutil.copy2(marker, target_marker)
    for sidecar in sidecars:
        (target_marker.parent / sidecar.name).symlink_to(sidecar.resolve())

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build_dir)
    struct.pack_into(
        "<IiI",
        entry,
        0,
        START_SCREEN_HASH,
        start_screen_lang_id(build_dir),
        checksum or OPEN_PLUGIN_CHECKSUM,
    )
    write_cstring(
        entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE,
        "openh264_player.rock",
    )
    write_cstring(
        entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH
    )
    write_cstring(
        entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, VIDEO_PATH
    )
    (rockbox / "rocks/plugin.dat").write_bytes(entry)
    (rockbox / "config.cfg").write_text(
        "start in screen: plugin\n"
        "resume: off\n"
        "tagcache_autoupdate: off\n",
        encoding="utf-8",
    )


def log_count(path: Path, marker: str) -> int:
    if not path.is_file():
        return 0
    return path.read_text(
        encoding="utf-8", errors="replace"
    ).count(marker)


def wait_for_log(
    path: Path, marker: str, previous_count: int = 0,
    timeout: float = 15.0,
) -> int:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        count = log_count(path, marker)
        if count > previous_count:
            return count
        time.sleep(0.05)
    raise SystemExit(f"timed out waiting for RVP log marker: {marker}")


def run_gate(
    build_dir: Path, marker: Path, repeats: int, output_dir: Path
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="rvp-controls-") as temp:
        root = Path(temp)
        prepare_root(build_dir, root, marker)
        gate_root = root / ".button-gates"
        gate_root.mkdir()
        BUTTON_GATES.clear()
        BUTTON_GATES.update(
            {
                "KP_Add": gate_root / "play.gate",
                "KP_Decimal": gate_root / "menu.gate",
                "KP_2": gate_root / "volume-up.gate",
            }
        )
        frame = root / "frame.bmp"
        audio = root / "audio.raw"
        process_log = root / "simulator.log"
        profile = root / ".rockbox/openh264/openh264_profile.log"
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_AUDIODRIVER": "disk",
                "SDL_DISKAUDIOFILE": str(audio),
                "SDL_VIDEODRIVER": "x11",
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
                "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
                "ROCKPOD_SIM_PLAY_ACTION_GATE":
                    str(BUTTON_GATES["KP_Add"]),
                "ROCKPOD_SIM_MENU_GATE":
                    str(BUTTON_GATES["KP_Decimal"]),
                "ROCKPOD_SIM_SCROLL_FWD_GATE":
                    str(BUTTON_GATES["KP_2"]),
            }
        )

        with process_log.open("w", encoding="utf-8") as log_handle:
            process = subprocess.Popen(
                [
                    str(build_dir / "rockboxui"),
                    "--zoom", "1",
                    "--nobackground",
                    "--root", str(root),
                ],
                cwd=build_dir,
                env=environment,
                stdout=log_handle,
                stderr=subprocess.STDOUT,
            )
            try:
                window_id(process.pid)
                wait_for_file(frame, timeout=30)
                wait_for_log(profile, "stage=start-at-frame", timeout=30)
                time.sleep(0.6)
                playing_first = output_dir / "playing-first.png"
                playing_second = output_dir / "playing-second.png"
                capture(frame, playing_first)
                time.sleep(0.7)
                capture(frame, playing_second)
                if changed_pixels(playing_first, playing_second) < 100:
                    raise SystemExit("RVP did not render fluid motion")

                pause_count = 0
                resume_count = 0
                seek_count = 0
                volume_count = 0
                for cycle in range(1, repeats + 1):
                    tap(process.pid, "KP_Add")
                    pause_count = wait_for_log(
                        profile, "stage=pause", pause_count
                    )
                    paused_first = output_dir / (
                        f"paused-{cycle:02d}-first.png"
                    )
                    paused_second = output_dir / (
                        f"paused-{cycle:02d}-second.png"
                    )
                    capture(frame, paused_first)
                    time.sleep(0.7)
                    capture(frame, paused_second)
                    if changed_pixels(paused_first, paused_second) != 0:
                        raise SystemExit(
                            f"RVP changed while paused on cycle {cycle}"
                        )

                    hold(process.pid, "KP_6", duration=0.8)
                    seek_count = wait_for_log(
                        profile, "stage=seek", seek_count
                    )
                    sought = output_dir / f"sought-{cycle:02d}.png"
                    capture(frame, sought)
                    if changed_pixels(paused_second, sought) < 100:
                        raise SystemExit(
                            f"RVP seek did not change frame on "
                            f"cycle {cycle}"
                        )

                    tap(process.pid, "KP_2")
                    volume_count = wait_for_log(
                        profile, "stage=volume", volume_count
                    )
                    tap(process.pid, "KP_Add")
                    resume_count = wait_for_log(
                        profile, "stage=resume", resume_count
                    )
                    time.sleep(0.7)
                    resumed = output_dir / f"resumed-{cycle:02d}.png"
                    capture(frame, resumed)
                    if changed_pixels(sought, resumed) < 100:
                        raise SystemExit(
                            f"RVP did not resume motion on cycle "
                            f"{cycle}"
                        )

                tap(process.pid, "KP_Decimal")
                wait_for_log(profile, "stage=shutdown-after-restore")
            finally:
                if process.poll() is None:
                    process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                if process_log.is_file():
                    shutil.copy2(
                        process_log, output_dir / "simulator.log"
                    )
                if profile.is_file():
                    shutil.copy2(profile, output_dir / "profile.log")

    print(
        f"PASS: RVP completed {repeats} pause/seek/volume/resume "
        f"cycles and Menu exit on {marker.name}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("marker", type=Path)
    parser.add_argument(
        "--build-dir", type=Path, default=Path("build-sim-ipod6g")
    )
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument(
        "--output-dir", type=Path,
        default=Path("/tmp/rvp-controls-gate"),
    )
    args = parser.parse_args()
    run_gate(
        args.build_dir.resolve(),
        args.marker.resolve(),
        max(1, args.repeats),
        args.output_dir.resolve(),
    )


if __name__ == "__main__":
    main()
