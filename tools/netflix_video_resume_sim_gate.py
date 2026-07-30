#!/usr/bin/env python3
"""Verify fresh Netflix videos show the ident and resumed videos bypass it."""

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
    capture,
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
VIDEO_PATH = "/Videos/Downloaded/segtest.rvp"
RESUME_MAGIC = 0x52565031
SEEDED_FRAME = 40
TOTAL_FRAMES = 600


def write_cstring(buffer: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")[: size - 1]
    buffer[offset : offset + len(encoded)] = encoded


def rockbox_crc32(value: str) -> int:
    crc = 0xFFFFFFFF
    for byte in value.encode("utf-8"):
        crc ^= byte << 24
        for _ in range(8):
            crc = (
                ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
                if crc & 0x80000000
                else (crc << 1) & 0xFFFFFFFF
            )
    return crc


def start_screen_lang_id(build_dir: Path) -> int:
    lang_enum = build_dir / "lang_enum.h"
    if lang_enum.is_file():
        for line in lang_enum.read_text(
            encoding="utf-8", errors="replace"
        ).splitlines():
            if "LANG_START_SCREEN," in line and "/*" in line:
                return int(line.split("/*", 1)[1].split("*/", 1)[0].strip())
    return LANG_START_SCREEN


def prepare_root(
    repo: Path, build_dir: Path, root: Path, *, seed_resume: bool
) -> Path:
    source = build_dir / "simdisk"
    rockbox_source = source / ".rockbox"
    rockbox = root / ".rockbox"
    plugin = build_dir / "apps/plugins/openh264_player.rock"
    ident = repo / "assets/ipodjs/rockbox/netflix/video-launch"
    for required in (
        plugin,
        source / VIDEO_PATH.lstrip("/"),
        ident / "frame-00.320x180x24.bmp",
        ident / "intro-44100-stereo.pcm",
    ):
        if not required.exists():
            raise SystemExit(f"missing video-resume gate input: {required}")

    rockbox.mkdir(parents=True)
    for name in ("fonts", "langs", "icons"):
        if (rockbox_source / name).is_dir():
            shutil.copytree(rockbox_source / name, rockbox / name)
    os.symlink(source / "Videos", root / "Videos")
    shutil.copytree(ident, rockbox / "ipodjs/netflix/video-launch")
    plugin_target = root / PLUGIN_PATH.lstrip("/")
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin, plugin_target)

    parameter = "netflix:" + VIDEO_PATH
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
        entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, parameter
    )
    (rockbox / "rocks/plugin.dat").write_bytes(entry)
    (rockbox / "config.cfg").write_text(
        "start in screen: plugin\nresume: off\ntagcache_autoupdate: off\n",
        encoding="utf-8",
    )

    crc = rockbox_crc32(VIDEO_PATH)
    resume = rockbox / "rocks/apps" / f"openh264-resume-{crc:08x}.dat"
    if seed_resume:
        resume.write_bytes(
            struct.pack("<IIii", RESUME_MAGIC, crc, SEEDED_FRAME, TOTAL_FRAMES)
        )
    return resume


def pixel_rgb(frame: Path, x: int, y: int) -> tuple[int, int, int] | None:
    result = subprocess.run(
        ["magick", str(frame), "-format", f"%[pixel:p{{{x},{y}}}]", "info:"],
        check=False,
        capture_output=True,
        text=True,
    )
    values = [int(value) for value in re.findall(r"\d+", result.stdout)]
    return tuple(values[:3]) if len(values) >= 3 else None


def is_2013_ident(frame: Path) -> bool:
    top = pixel_rgb(frame, 160, 5)
    center = pixel_rgb(frame, 160, 120)
    side = pixel_rgb(frame, 30, 120)
    return bool(
        top and center and side and max(top) < 10
        and min(center) > 150 and min(side) > 120
    )


def wait_for_2013_ident(frame: Path, timeout: float = 10.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if is_2013_ident(frame):
            return
        time.sleep(0.04)
    raise SystemExit("official 2013 Netflix video ident did not appear")


def simulator_environment(frame: Path, audio: Path) -> dict[str, str]:
    environment = os.environ.copy()
    environment.update(
        {
            "SDL_AUDIODRIVER": "disk",
            "SDL_DISKAUDIOFILE": str(audio),
            "SDL_VIDEODRIVER": "x11",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
            "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
        }
    )
    return environment


def stop_simulator(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def run_fresh_case(
    repo: Path, build_dir: Path, output: Path, temp: Path
) -> None:
    root = temp / "fresh"
    prepare_root(repo, build_dir, root, seed_resume=False)
    frame = root / "frame.bmp"
    audio = root / "netflix-video-fresh-audio.raw"
    process = subprocess.Popen(
        [
            str(build_dir / "rockboxui"), "--zoom", "1",
            "--nobackground", "--root", str(root),
        ],
        cwd=build_dir,
        env=simulator_environment(frame, audio),
    )
    try:
        window_id(process.pid)
        wait_for_file(frame)
        wait_for_2013_ident(frame)
        capture(frame, output / "video-ident-2013.png")
        time.sleep(1.5)
    finally:
        stop_simulator(process)

    if not audio.is_file() or audio.stat().st_size < 100_000:
        raise SystemExit("fresh-launch 2013 video-ident audio was not produced")


def run_resume_case(
    repo: Path, build_dir: Path, output: Path, temp: Path
) -> int:
    root = temp / "resume"
    resume = prepare_root(repo, build_dir, root, seed_resume=True)
    frame = root / "frame.bmp"
    audio = root / "netflix-video-resume-audio.raw"
    process = subprocess.Popen(
        [
            str(build_dir / "rockboxui"), "--zoom", "1",
            "--nobackground", "--root", str(root),
        ],
        cwd=build_dir,
        env=simulator_environment(frame, audio),
    )
    try:
        window_id(process.pid)
        wait_for_file(frame)
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            if is_2013_ident(frame):
                raise SystemExit("2013 Netflix ident replayed during resume")
            time.sleep(0.04)
        capture(frame, output / "video-resumed-direct.png")
        tap(process.pid, "KP_Decimal")
        time.sleep(0.8)
    finally:
        stop_simulator(process)

    magic, _crc, saved_frame, total = struct.unpack(
        "<IIii", resume.read_bytes()
    )
    if (
        magic != RESUME_MAGIC or total != TOTAL_FRAMES
        or saved_frame < SEEDED_FRAME
    ):
        raise SystemExit(
            f"RVP resume was not applied/saved: frame={saved_frame}"
        )
    return saved_frame


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/netflix-video-resume-gate")
    )
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    build_dir = args.build_dir.resolve()
    args.output.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="netflix-video-resume-") as temp:
        temp_root = Path(temp)
        run_fresh_case(repo, build_dir, args.output, temp_root)
        saved_frame = run_resume_case(
            repo, build_dir, args.output, temp_root
        )
    print(
        "Netflix video gate passed: fresh launch showed the 2013 ident; "
        f"resume bypassed it and continued at frame {saved_frame}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
