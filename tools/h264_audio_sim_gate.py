#!/usr/bin/env python3
"""Prove sustained AAC playback for an Apple-contract H.264 movie.

The iPod Video simulator cannot run the BCM2722 video firmware, but it uses
the same MP4 parser, complete AAC chunk map, Rockbox AAC codec thread, and PCM
mixer service as the supported H.264 path.  This gate is deliberately longer
than the historical one-to-two-second audio failure.
"""

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


PLUGIN_PATH = "/.rockbox/rocks/viewers/openh264_player.rock"
MOVIE_PATH = "/Videos/apple-audio-gate.m4v"
LOG_PATTERN = re.compile(
    r'elapsed_ms=(\d+) duration_ms=(\d+) target_ms=(\d+) '
    r'decoder_failed=(\d+) result=(-?\d+)'
)


def write_cstring(buffer: bytearray, offset: int, size: int, value: str) -> None:
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


def prepare_root(build_dir: Path, root: Path, movie: Path) -> Path:
    plugin = build_dir / "apps/plugins/openh264_player.rock"
    codec = build_dir / "lib/rbcodec/codecs/aac.codec"
    simulator = build_dir / "rockboxui"
    for required in (plugin, codec, simulator, movie):
        if not required.is_file():
            raise SystemExit(f"missing H.264 audio gate input: {required}")

    rockbox = root / ".rockbox"
    (rockbox / "rocks/viewers").mkdir(parents=True)
    (rockbox / "codecs").mkdir(parents=True)
    (root / "Videos").mkdir(parents=True)
    shutil.copy2(plugin, root / PLUGIN_PATH.lstrip("/"))
    shutil.copy2(codec, rockbox / "codecs/aac.codec")
    (root / MOVIE_PATH.lstrip("/")).symlink_to(movie.resolve())

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
        entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, MOVIE_PATH
    )
    (rockbox / "rocks/plugin.dat").write_bytes(entry)
    (rockbox / "config.cfg").write_text(
        "start in screen: plugin\n"
        "resume: off\n"
        "tagcache_autoupdate: off\n",
        encoding="utf-8",
    )
    return rockbox / "openh264/sim_audio.log"


def stop_process(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def wait_for_result(log: Path, process: subprocess.Popen[bytes], timeout: int):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if log.is_file():
            lines = log.read_text(
                encoding="utf-8", errors="replace"
            ).splitlines()
            if lines:
                match = LOG_PATTERN.search(lines[-1])
                if match:
                    return tuple(int(value) for value in match.groups())
        if process.poll() is not None:
            raise SystemExit(
                f"simulator exited before the AAC result (status {process.returncode})"
            )
        time.sleep(0.1)
    raise SystemExit(f"timed out waiting for sustained AAC playback: {log}")


def run_gate(build_dir: Path, movie: Path, target_ms: int, timeout: int) -> None:
    with tempfile.TemporaryDirectory(prefix="h264-audio-sim-") as temp:
        root = Path(temp)
        log = prepare_root(build_dir, root, movie)
        audio = root / "simulator-audio.raw"
        process_log = root / "simulator.log"
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_AUDIODRIVER": "disk",
                "SDL_DISKAUDIOFILE": str(audio),
                "SDL_VIDEODRIVER": "dummy",
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_H264_AUDIO_TEST_MS": str(target_ms),
            }
        )
        with process_log.open("wb") as output:
            process = subprocess.Popen(
                [
                    str(build_dir / "rockboxui"),
                    "--zoom", "1", "--nobackground", "--root", str(root),
                ],
                cwd=build_dir,
                env=environment,
                stdout=output,
                stderr=subprocess.STDOUT,
            )
            try:
                elapsed, duration, requested, failed, result = wait_for_result(
                    log, process, timeout
                )
            finally:
                stop_process(process)

        if requested != target_ms:
            raise SystemExit(
                f"simulator tested {requested} ms instead of {target_ms} ms"
            )
        if failed or result != 0 or elapsed < target_ms:
            raise SystemExit(
                "AAC playback failed: "
                f"elapsed={elapsed} duration={duration} target={requested} "
                f"decoder_failed={failed} result={result}"
            )
        if not audio.is_file() or audio.stat().st_size < target_ms * 44100 * 4 // 1000:
            raise SystemExit("SDL audio sink did not receive the sustained PCM interval")
        if not any(audio.read_bytes()):
            raise SystemExit("SDL audio sink contains only silence")
        print(
            "H.264 AAC simulator gate passed: "
            f"elapsed={elapsed}ms duration={duration}ms "
            f"pcm_bytes={audio.stat().st_size}"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("movie", type=Path)
    parser.add_argument(
        "--build-dir", type=Path, default=Path("build-sim-ipodvideo")
    )
    parser.add_argument("--target-ms", type=int, default=30000)
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args()
    if args.target_ms < 3000:
        raise SystemExit("target must be at least 3000 ms")
    run_gate(
        args.build_dir.resolve(), args.movie.resolve(),
        args.target_ms, args.timeout,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
