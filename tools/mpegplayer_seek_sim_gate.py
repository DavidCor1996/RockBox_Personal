#!/usr/bin/env python3
"""Stress MPEG Player held-forward seeks with a real MPEG program stream."""

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


PLUGIN_PATH = "/.rockbox/rocks/viewers/mpegplayer.rock"
VIDEO_PATH = "/Videos/mpegplayer-seek-test.mpg"


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


def prepare_root(
    build_dir: Path, root: Path, source_video: Path
) -> None:
    source_rockbox = build_dir / "simdisk" / ".rockbox"
    built_plugin = (
        build_dir / "apps" / "plugins" / "mpegplayer" / "mpegplayer.rock"
    )
    if not built_plugin.is_file():
        raise SystemExit(f"missing simulator plugin: {built_plugin}")
    if not source_video.is_file():
        raise SystemExit(f"missing source video: {source_video}")

    rockbox = root / ".rockbox"
    rockbox.mkdir(parents=True)
    for name in ("fonts", "langs", "icons"):
        source = source_rockbox / name
        if source.is_dir():
            shutil.copytree(source, rockbox / name)
    repo_root = build_dir.parent
    stock_controls = repo_root / "assets" / "ipodjs" / "apple"
    if stock_controls.is_dir():
        shutil.copytree(stock_controls, rockbox / "ipodjs" / "apple")
    volume_controls = repo_root / "wps" / "iPone"
    if volume_controls.is_dir():
        shutil.copytree(volume_controls, rockbox / "wps" / "iPone")

    plugin = root / PLUGIN_PATH.lstrip("/")
    plugin.parent.mkdir(parents=True)
    shutil.copy2(built_plugin, plugin)
    (plugin.parent / "mpegplayer.cfg").write_text(
        "file version:          6\n"
        "Display mode:          1\n"
        "Limit FPS:          1\n"
        "Skip frames:          1\n"
        "Resume options:          2\n",
        encoding="utf-8",
    )

    video = root / VIDEO_PATH.lstrip("/")
    video.parent.mkdir(parents=True)
    video.symlink_to(source_video.resolve())

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
        "mpegplayer.rock",
    )
    write_cstring(
        entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH
    )
    write_cstring(
        entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, VIDEO_PATH
    )
    (rockbox / "rocks" / "plugin.dat").write_bytes(entry)
    (rockbox / "config.cfg").write_text(
        "start in screen: plugin\n"
        "resume: off\n"
        "tagcache_autoupdate: off\n",
        encoding="utf-8",
    )


def wait_for_log(
    path: Path, marker: str, previous_count: int, timeout: float
) -> int:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if path.is_file():
            count = path.read_text(
                encoding="utf-8", errors="replace"
            ).count(marker)
            if count > previous_count:
                return count
        time.sleep(0.05)
    raise SystemExit(f"timed out waiting for simulator log marker: {marker}")


def log_count(path: Path, marker: str) -> int:
    if not path.is_file():
        return 0
    return path.read_text(
        encoding="utf-8", errors="replace"
    ).count(marker)


def latest_playing_seek_time(path: Path) -> int:
    if not path.is_file():
        return -1
    matches = re.findall(
        r"mpegplayer-sm: seek begin time=(\d+) whence=0 status=2",
        path.read_text(encoding="utf-8", errors="replace"),
    )
    return int(matches[-1]) if matches else -1


def verify_playing_volume(
    process: subprocess.Popen[bytes],
    frame: Path,
    log: Path,
    output_dir: Path | None,
) -> None:
    marker = "stock volume overlay suppressed during playback"
    marker_count = log_count(log, marker)

    with tempfile.TemporaryDirectory(
        prefix="mpegplayer-playing-volume-"
    ) as frame_temp:
        frame_temp_path = Path(frame_temp)
        before = frame_temp_path / "before.png"
        after = frame_temp_path / "after.png"
        capture(frame, before)
        for _ in range(3):
            tap(process.pid, "KP_2")
            marker_count = wait_for_log(
                log, marker, marker_count, timeout=3
            )
        time.sleep(1.25)
        capture(frame, after)
        if changed_pixels(before, after) == 0:
            raise SystemExit(
                "video stopped moving after playback volume changes"
            )
        if process.poll() is not None:
            raise SystemExit(
                f"simulator exited after playback volume changes: "
                f"{process.returncode}"
            )
        if output_dir is not None:
            shutil.copy2(before, output_dir / "playing-volume-before.png")
            shutil.copy2(after, output_dir / "playing-volume-after.png")


def verify_pause_resume(
    process: subprocess.Popen[bytes],
    frame: Path,
    log: Path,
    repeats: int,
    output_dir: Path | None,
) -> None:
    pause_count = log_count(log, "mpegplayer-sm: pause end status=2")
    resume_count = log_count(log, "mpegplayer-sm: resume end status=1")

    for cycle in range(1, repeats + 1):
        tap(process.pid, "KP_Add")
        pause_count = wait_for_log(
            log, "mpegplayer-sm: pause end status=2", pause_count,
            timeout=5,
        )
        with tempfile.TemporaryDirectory(
            prefix="mpegplayer-pause-frame-"
        ) as frame_temp:
            frame_temp_path = Path(frame_temp)
            paused_first = frame_temp_path / "paused-first.png"
            paused_second = frame_temp_path / "paused-second.png"
            capture(frame, paused_first)
            stable = False
            for attempt in range(4):
                time.sleep(0.75 if attempt else 1.25)
                capture(frame, paused_second)
                if changed_pixels(paused_first, paused_second) == 0:
                    stable = True
                    break
                shutil.copy2(paused_second, paused_first)
            if not stable:
                raise SystemExit(
                    f"video pixels kept changing while paused on cycle {cycle}"
                )
            if output_dir is not None:
                shutil.copy2(
                    paused_second,
                    output_dir / f"paused-controls-{cycle:02d}.png",
                )
            if cycle == 1:
                tap(process.pid, "KP_2")
                if process.poll() is not None:
                    raise SystemExit(
                        f"simulator exited after volume change: "
                        f"{process.returncode}"
                    )
                if output_dir is not None:
                    capture(
                        frame,
                        output_dir / "volume-controls.png",
                    )

            tap(process.pid, "KP_Add")
            resume_count = wait_for_log(
                log, "mpegplayer-sm: resume end status=1", resume_count,
                timeout=5,
            )
            time.sleep(1.1)
            resumed = frame_temp_path / "resumed.png"
            capture(frame, resumed)
            if changed_pixels(paused_second, resumed) == 0:
                raise SystemExit(
                    f"video did not move after resume on cycle {cycle}"
                )
            if output_dir is not None:
                capture(
                    frame,
                    output_dir / f"after-resume-{cycle:02d}.png",
                )


def run_gate(
    build_dir: Path, source_video: Path, repeats: int, hold_seconds: float,
    pause_repeats: int, output_dir: Path | None,
) -> None:
    with tempfile.TemporaryDirectory(prefix="mpegplayer-seek-") as temp:
        root = Path(temp)
        prepare_root(build_dir, root, source_video)
        gate_root = root / ".button-gates"
        gate_root.mkdir()
        BUTTON_GATES.clear()
        BUTTON_GATES.update(
            {
                "KP_Add": gate_root / "play.gate",
                "KP_Decimal": gate_root / "menu.gate",
            }
        )
        frame = root / "frame.bmp"
        audio = root / "audio.raw"
        log = root / "simulator.log"
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
            }
        )

        with log.open("w", encoding="utf-8") as log_handle:
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
                wait_for_log(
                    log, "mpegplayer: playback controls ready", 0,
                    timeout=30,
                )
                time.sleep(0.5)
                if output_dir is not None:
                    output_dir.mkdir(parents=True, exist_ok=True)
                    capture(frame, output_dir / "before-seek.png")
                verify_playing_volume(
                    process, frame, log, output_dir
                )
                verify_pause_resume(
                    process, frame, log, pause_repeats, output_dir
                )
                seek_count = log_count(log, "mpegplayer-sm: seek end")
                previous_seek_time = -1
                for _ in range(repeats):
                    before = frame.stat().st_mtime_ns
                    hold(process.pid, "KP_6", duration=hold_seconds)
                    seek_count = wait_for_log(
                        log, "mpegplayer-sm: seek end", seek_count,
                        timeout=12,
                    )
                    seek_time = latest_playing_seek_time(log)
                    if seek_time < 60 * 45_000:
                        raise SystemExit(
                            "forward hold did not advance at least one minute"
                        )
                    if seek_time <= previous_seek_time:
                        raise SystemExit(
                            "forward seek timestamp did not increase"
                        )
                    previous_seek_time = seek_time
                    deadline = time.monotonic() + 5
                    while (
                        time.monotonic() < deadline
                        and frame.stat().st_mtime_ns == before
                    ):
                        time.sleep(0.05)
                    if frame.stat().st_mtime_ns == before:
                        raise SystemExit(
                            "framebuffer stopped updating after forward seek"
                        )
                    if process.poll() is not None:
                        raise SystemExit(
                            f"simulator exited after seek: {process.returncode}"
                        )
                    if output_dir is not None:
                        capture(
                            frame,
                            output_dir / f"after-seek-{seek_count:02d}.png",
                        )
                tap(process.pid, "KP_Decimal")
                wait_for_log(
                    log, "button_loop end action=0", 0, timeout=5
                )
            finally:
                if process.poll() is None:
                    process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                if output_dir is not None:
                    output_dir.mkdir(parents=True, exist_ok=True)
                    if log.is_file():
                        shutil.copy2(log, output_dir / "simulator.log")
                    if frame.is_file():
                        capture(frame, output_dir / "last-frame.png")

        if output_dir is not None:
            shutil.copy2(log, output_dir / "simulator.log")

        print(
            f"PASS: {pause_repeats} pause/resume cycles and {repeats} "
            f"held-forward seeks completed, then Menu exited on "
            f"{source_video.name}"
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("video", type=Path)
    parser.add_argument(
        "--build-dir", type=Path, default=Path("build-sim-ipod6g")
    )
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--pause-repeats", type=int, default=5)
    parser.add_argument("--hold-seconds", type=float, default=0.8)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    run_gate(
        args.build_dir.resolve(),
        args.video.resolve(),
        max(0, args.repeats),
        max(0.1, args.hold_seconds),
        max(1, args.pause_repeats),
        args.output_dir.resolve() if args.output_dir else None,
    )


if __name__ == "__main__":
    main()
