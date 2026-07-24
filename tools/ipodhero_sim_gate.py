#!/usr/bin/env python3
"""Install and verify a timing-exact iPod Hero simulator fixture."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import math
import re
import shutil
import struct
import subprocess
import sys
import time
import wave


SONG_NAME = "iPod Hero Timing Fixture.wav"
SONG_PATH = f"/Music/{SONG_NAME}"
SONG_LENGTH_MS = 60_000
PLUGIN_PATH = "/.rockbox/rocks/games/ipodhero.rock"
OPEN_PLUGIN_ENTRY_SIZE = 568
OPEN_PLUGIN_NAME_OFFSET = 12
OPEN_PLUGIN_NAME_SIZE = 33
OPEN_PLUGIN_PATH_OFFSET = 45
OPEN_PLUGIN_PATH_SIZE = 261
OPEN_PLUGIN_PARAM_OFFSET = 306
OPEN_PLUGIN_PARAM_SIZE = 261
START_SCREEN_HASH = 0x8E2A0CC9
OPEN_PLUGIN_CHECKSUM = (
    (OPEN_PLUGIN_ENTRY_SIZE << 16)
    + 0
    + 4
    + 8
    + OPEN_PLUGIN_NAME_OFFSET
    + OPEN_PLUGIN_PATH_OFFSET
    + OPEN_PLUGIN_PARAM_OFFSET
)


def note_rows(song_length_ms: int) -> list[str]:
    lanes = ("g", "r", "y", "b", "o")
    rows = []
    phrase = 1
    for number, time_ms in enumerate(range(2000, song_length_ms - 1999, 500)):
        lane = lanes[number % len(lanes)]
        duration = 750 if number % 16 == 12 else 0
        flags = []
        if number % 2:
            flags.append("hopo")
        if number % 8 in (0, 1, 2, 3):
            flags.append("star")
        if number % 8 == 3:
            flags.append("phrase_end")
        if number % 12 == 10:
            lane += lanes[(number + 2) % len(lanes)]
        rows.append(
            f"{time_ms}\t{duration}\t{lane}\t{','.join(flags) or '-'}\t{phrase}\n"
        )
        if number % 8 == 3:
            phrase += 1
    return rows


def write_audio(path: Path, song_length_ms: int) -> None:
    rate = 44_100
    frames = rate * song_length_ms // 1000
    beat_frames = rate // 2
    pulse_frames = rate // 20
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        block = bytearray()
        for frame in range(frames):
            beat = frame // beat_frames
            within = frame % beat_frames
            sample = 0
            if beat >= 4 and within < pulse_frames:
                frequency = (330, 392, 440, 494, 554)[beat % 5]
                envelope = (pulse_frames - within) / pulse_frames
                sample = int(12_000 * envelope * math.sin(2 * math.pi * frequency * frame / rate))
            block.extend(struct.pack("<h", sample))
            if len(block) >= 128 * 1024:
                output.writeframesraw(block)
                block.clear()
        if block:
            output.writeframesraw(block)


def prepare(repo: Path, simdisk: Path, assets: Path,
            song_length_ms: int) -> None:
    data = simdisk / ".rockbox" / "rocks.data" / "ipodhero"
    charts = data / "charts"
    music = simdisk / "Music"
    notes = charts / "timing-fixture.notes.tsv"
    chart = charts / "timing-fixture-expert.ihc"
    audio = music / SONG_NAME
    skin = data / "skins" / "live-stage"
    built_plugin = simdisk.parent / "apps" / "plugins" / "ipodhero" / \
        "ipodhero.rock"
    installed_plugin = simdisk / PLUGIN_PATH.lstrip("/")

    if not built_plugin.is_file():
        raise FileNotFoundError(f"missing built simulator plugin: {built_plugin}")
    installed_plugin.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built_plugin, installed_plugin)
    charts.mkdir(parents=True, exist_ok=True)
    notes.write_text(
        "# time_ms\tduration_ms\tlanes\tflags\tphrase\n" +
        "".join(note_rows(song_length_ms)),
        encoding="utf-8",
    )
    write_audio(audio, song_length_ms)
    subprocess.run(
        [
            sys.executable,
            str(repo / "tools" / "ipodhero_chart.py"),
            "--notes", str(notes),
            "--output", str(chart),
            "--song-length-ms", str(song_length_ms),
            "--difficulty", "expert",
            "--origin", "authored",
            "--source-size", str(audio.stat().st_size),
        ],
        check=True,
    )
    subprocess.run(
        [
            sys.executable,
            str(repo / "tools" / "ipodhero_prepare_assets.py"),
            "--source", str(assets),
            "--output", str(skin),
            "--force",
        ],
        check=True,
    )
    row = (
        f"{SONG_PATH}\t{audio.stat().st_size}\t{song_length_ms}\t00000000\t"
        "Timing Fixture\tiPod Hero\t\t\t\t"
        "charts/timing-fixture-expert.ihc\tlive-stage\n"
    )
    (data / "index.tsv").write_text(
        "# path\tsize\tlength_ms\tcrc\ttitle\tartist\teasy\tmedium\thard\texpert\tskin\n" + row,
        encoding="utf-8",
    )


def verify(simdisk: Path, song_length_ms: int) -> None:
    data = simdisk / ".rockbox" / "rocks.data" / "ipodhero"
    audio = simdisk / SONG_PATH.lstrip("/")
    chart = data / "charts" / "timing-fixture-expert.ihc"
    plugin = simdisk / ".rockbox" / "rocks" / "games" / "ipodhero.rock"
    expected_frames = 44_100 * song_length_ms // 1000
    if not audio.is_file() or wave.open(str(audio)).getnframes() != expected_frames:
        raise ValueError("timing fixture audio is missing or malformed")
    payload = chart.read_bytes()
    if len(payload) < 52 or payload[:4] != b"IHC1" or payload[32] != 5:
        raise ValueError("timing fixture chart is malformed")
    if not (data / "skins" / "live-stage" / "SHA256SUMS").is_file():
        raise ValueError("simulator skin is missing")
    if not plugin.is_file():
        raise ValueError("installed simulator plugin is missing")
    built_plugin = simdisk.parent / "apps" / "plugins" / "ipodhero" / \
        "ipodhero.rock"
    if built_plugin.is_file() and plugin.read_bytes() != built_plugin.read_bytes():
        raise ValueError("installed simulator plugin is stale")
    print(f"verified iPod Hero simulator fixture in {simdisk}")


def lang_id(build_dir: Path, name: str) -> int:
    lines = (build_dir / "lang_enum.h").read_text(
        encoding="utf-8", errors="replace"
    ).splitlines()
    for line in lines:
        if name in line:
            match = re.search(r"/\*\s*(\d+)\s*\*/", line)
            if match:
                return int(match.group(1))
    raise ValueError(f"missing simulator language id: {name}")


def lang_last_index(build_dir: Path) -> int:
    last = None
    lines = (build_dir / "lang_enum.h").read_text(
        encoding="utf-8", errors="replace"
    ).splitlines()
    for line in lines:
        match = re.search(r"/\*\s*(\d+)\s*\*/", line)
        if match:
            last = int(match.group(1))
        if "LANG_LAST_INDEX_IN_ARRAY" in line:
            if last is None:
                break
            return last + 1
    raise ValueError("could not derive simulator language-table length")


def write_cstring(entry: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")
    if len(encoded) >= size:
        raise ValueError(f"open-plugin field is too long: {value}")
    entry[offset : offset + size] = b"\0" * size
    entry[offset : offset + len(encoded)] = encoded


def open_plugin_entry(build_dir: Path, autoplay: str | None,
                      parameter: str | None) -> bytes:
    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    start_screen = lang_id(build_dir, "LANG_START_SCREEN")
    checksum = OPEN_PLUGIN_CHECKSUM + lang_last_index(build_dir)
    struct.pack_into("<IiI", entry, 0, START_SCREEN_HASH, start_screen, checksum)
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE,
                  "ipodhero.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE,
                  PLUGIN_PATH)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE,
                  parameter or
                  f"{'autotest-' + autoplay if autoplay else 'practice'}:{SONG_PATH}")
    return bytes(entry)


def capture_has_content(path: Path) -> bool:
    try:
        if not path.is_file() or path.stat().st_size <= 54:
            return False
        payload = path.read_bytes()
    except FileNotFoundError:
        return False
    if payload[:2] != b"BM" or len(payload) < 54:
        return False
    offset = struct.unpack_from("<I", payload, 10)[0]
    bits = struct.unpack_from("<H", payload, 28)[0]
    if bits != 32 or offset >= len(payload):
        return False
    pixels = payload[offset:]
    return any(pixels[index] or pixels[index + 1] or pixels[index + 2]
               for index in range(0, len(pixels) - 3, 4))


def launch(build_dir: Path, simdisk: Path, capture: Path,
           video_driver: str | None, wait_results: bool,
           song_length_ms: int, autoplay: str | None,
           parameter: str | None, expect_error: str | None) -> None:
    simulator = build_dir / "rockboxui"
    if not simulator.is_file():
        raise FileNotFoundError(f"missing simulator executable: {simulator}")
    plugin_dat = simdisk / ".rockbox" / "rocks" / "plugin.dat"
    config = simdisk / ".rockbox" / "config.cfg"
    old_plugin_dat = plugin_dat.read_bytes() if plugin_dat.exists() else None
    old_config = config.read_bytes() if config.exists() else None
    performance = simdisk / ".rockbox" / "rocks.data" / "ipodhero" / \
        "performance.log"
    sim_test = simdisk / ".rockbox" / "rocks.data" / "ipodhero" / \
        "sim-test.log"
    sim_error = simdisk / ".rockbox" / "rocks.data" / "ipodhero" / \
        "sim-error.log"
    sim_library = simdisk / ".rockbox" / "rocks.data" / "ipodhero" / \
        "sim-library.log"
    process = None
    good_frame = None
    performance_seen = None
    lifecycle_fd_counts: dict[int, int] = {}
    library_seen = None
    try:
        plugin_dat.parent.mkdir(parents=True, exist_ok=True)
        plugin_dat.write_bytes(open_plugin_entry(build_dir, autoplay,
                                                 parameter))
        lines = old_config.decode("utf-8", errors="replace").splitlines() \
            if old_config is not None else []
        lines = [line for line in lines
                 if not line.startswith(("start in screen:", "openplugin:"))]
        lines.append("start in screen: plugin")
        config.parent.mkdir(parents=True, exist_ok=True)
        config.write_text("\n".join(lines) + "\n", encoding="utf-8")
        capture.parent.mkdir(parents=True, exist_ok=True)
        if capture.exists():
            capture.unlink()
        if wait_results:
            performance.unlink(missing_ok=True)
            if autoplay:
                sim_test.unlink(missing_ok=True)
        if expect_error:
            sim_error.unlink(missing_ok=True)
        if parameter == "library":
            sim_library.unlink(missing_ok=True)
        environment = os.environ.copy()
        environment.update({
            "SDL_AUDIODRIVER": "dummy",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_PREVIEW_BMP": str(capture),
            "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "250",
        })
        if video_driver:
            environment["SDL_VIDEODRIVER"] = video_driver
        process = subprocess.Popen(
            [str(simulator), "--zoom", "1", "--nobackground", "--root",
             str(simdisk)],
            cwd=build_dir,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        started = time.monotonic()
        deadline = started + (
            song_length_ms / 1000 * (20 if autoplay == "lifecycle" else 1)
            + 30 if wait_results else 12
        )
        captured = False
        while time.monotonic() < deadline:
            if process.poll() is not None:
                output = process.stdout.read() if process.stdout else ""
                raise RuntimeError("simulator exited before gameplay\n" + output[-2000:])
            if capture_has_content(capture):
                try:
                    candidate = capture.read_bytes()
                except FileNotFoundError:
                    time.sleep(0.05)
                    continue
                captured = True
                good_frame = candidate
                if not wait_results and not expect_error:
                    if parameter == "library":
                        if not sim_library.is_file():
                            time.sleep(0.1)
                            continue
                        report = sim_library.read_text(
                            encoding="utf-8", errors="replace"
                        )
                        if "songs=1" not in report or \
                                "first=iPod Hero - Timing Fixture" not in report:
                            raise ValueError(
                                "song library gate failed\n" + report
                            )
                        if library_seen is None:
                            library_seen = time.monotonic()
                        elif time.monotonic() - library_seen >= 3.0:
                            print("verified iPod Hero in-game song library")
                            print(report.rstrip())
                            return
                        time.sleep(0.1)
                        continue
                    if time.monotonic() - started < 5.0:
                        time.sleep(0.1)
                        continue
                    print(f"launched iPod Hero practice fixture; frame: {capture}")
                    return
            if expect_error and sim_error.is_file():
                error_report = sim_error.read_text(
                    encoding="utf-8", errors="replace"
                )
                if expect_error not in error_report:
                    raise ValueError(
                        f"expected error {expect_error!r}\n{error_report}"
                    )
                if captured and time.monotonic() - started >= 2.0:
                    print(f"verified iPod Hero error: {expect_error}")
                    print(error_report.rstrip())
                    return
            if captured and wait_results and performance.is_file():
                report = performance.read_text(encoding="utf-8", errors="replace")
                required = ("frames=", "average_frame_ticks_x100=",
                            "p95_frame_ticks=", "free_headroom=",
                            "package_load_ticks=",
                            "max_input_judgement_ticks=",
                            "max_active_sprites=")
                if all(field in report for field in required):
                    if autoplay:
                        if not sim_test.is_file():
                            time.sleep(0.1)
                            continue
                        test_report = sim_test.read_text(
                            encoding="utf-8", errors="replace"
                        )
                        values = dict(
                            line.split("=", 1) for line in test_report.splitlines()
                            if "=" in line
                        )
                        events = int(values.get("events", "-1"))
                        if autoplay == "lifecycle":
                            runs = int(values.get("runs", "0"))
                            fd_root = Path("/proc") / str(process.pid) / "fd"
                            if runs > 0 and runs not in lifecycle_fd_counts \
                                    and fd_root.is_dir():
                                lifecycle_fd_counts[runs] = sum(
                                    1 for _ in fd_root.iterdir()
                                )
                            if runs < 20:
                                time.sleep(0.1)
                                continue
                        perfect_pass = (
                            autoplay in ("perfect", "play") and events > 0
                            and int(values.get("hits", "-1")) == events
                            and int(values.get("perfect", "-1")) == events
                            and int(values.get("misses", "-1")) == 0
                            and int(values.get("great", "-1")) == 0
                            and int(values.get("good", "-1")) == 0
                            and int(values.get("grace", "-1")) == 0
                            and int(values.get("max_streak", "-1")) == events
                            and int(values.get("sustain_ms", "0")) > 0
                            and values.get("star_activated") == "1"
                            and (song_length_ms != 12000
                                 or autoplay == "play"
                                 or int(values.get("score", "-1")) == 3544)
                        )
                        mixed_pass = (
                            autoplay == "mixed" and events > 0
                            and int(values.get("hits", "-1"))
                            + int(values.get("misses", "-1")) == events
                            and int(values.get("perfect", "0")) > 0
                            and int(values.get("great", "0")) > 0
                            and int(values.get("good", "0")) > 0
                            and int(values.get("grace", "0")) > 0
                            and int(values.get("misses", "0")) > 0
                            and int(values.get("sustain_ms", "0")) > 0
                            and int(values.get("broken_sustains", "0")) > 0
                        )
                        lifecycle_pass = (
                            autoplay == "lifecycle" and events > 0
                            and int(values.get("hits", "-1")) == events
                            and int(values.get("perfect", "-1")) == events
                            and int(values.get("misses", "-1")) == 0
                            and int(values.get("runs", "0")) == 20
                            and int(values.get("pause_resume_cycles", "0")) >= 20
                            and int(values.get("scripted_pause_cycles", "0")) == 20
                            and int(values.get("retries", "0")) == 19
                        )
                        if lifecycle_pass and lifecycle_fd_counts:
                            lifecycle_pass = (
                                len(lifecycle_fd_counts) == 20
                                and max(lifecycle_fd_counts.values())
                                - min(lifecycle_fd_counts.values()) <= 2
                            )
                        if not ((perfect_pass or mixed_pass or lifecycle_pass)
                                and values.get("input_normalizer") == "pass"):
                            raise ValueError(
                                "autoplay judgement gate failed\n" + test_report
                            )
                    if performance_seen is None:
                        performance_seen = time.monotonic()
                    elif time.monotonic() - performance_seen >= 0.5:
                        print(f"completed iPod Hero practice fixture; frame: {capture}")
                        print(report.rstrip())
                        if autoplay:
                            print(test_report.rstrip())
                        if autoplay == "lifecycle" and lifecycle_fd_counts:
                            print("fd_counts=" + ",".join(
                                f"{run}:{lifecycle_fd_counts[run]}"
                                for run in sorted(lifecycle_fd_counts)
                            ))
                        return
            time.sleep(0.1)
        if not captured:
            raise TimeoutError("simulator did not produce a gameplay frame")
        raise TimeoutError("simulator did not reach results or write performance.log")
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)
        if old_plugin_dat is None:
            plugin_dat.unlink(missing_ok=True)
        else:
            plugin_dat.write_bytes(old_plugin_dat)
        if old_config is None:
            config.unlink(missing_ok=True)
        else:
            config.write_bytes(old_config)
        if good_frame is not None:
            capture.write_bytes(good_frame)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--simdisk", type=Path, required=True)
    parser.add_argument("--assets", type=Path)
    parser.add_argument("--verify-only", action="store_true")
    parser.add_argument("--launch", action="store_true",
                        help="boot the simulator into a practice run")
    parser.add_argument("--capture", type=Path,
                        help="gameplay BMP written by --launch")
    parser.add_argument("--video-driver",
                        help="optional SDL video driver, such as dummy or x11")
    parser.add_argument("--wait-results", action="store_true",
                        help="run the fixture through results")
    parser.add_argument("--song-length-ms", type=int,
                        default=SONG_LENGTH_MS,
                        help="fixture duration; use a short value for results gates")
    parser.add_argument("--autoplay",
                        choices=("perfect", "mixed", "lifecycle", "play"),
                        help="drive perfect inputs and validate judgement")
    parser.add_argument("--plugin-parameter",
                        help="override the path/mode passed to the plugin")
    parser.add_argument("--expect-error",
                        help="wait for a simulator error report containing text")
    args = parser.parse_args()
    if args.song_length_ms < 4000:
        parser.error("--song-length-ms must be at least 4000")
    if args.autoplay and (not args.launch or not args.wait_results):
        parser.error("--autoplay requires --launch and --wait-results")
    if args.autoplay and args.plugin_parameter:
        parser.error("--autoplay cannot be combined with --plugin-parameter")
    if args.expect_error and (not args.launch or args.wait_results):
        parser.error("--expect-error requires --launch without --wait-results")
    repo = Path(__file__).resolve().parent.parent
    if not args.verify_only:
        if args.assets is None:
            parser.error("--assets is required unless --verify-only is used")
        prepare(repo, args.simdisk.resolve(), args.assets.resolve(),
                args.song_length_ms)
    verify(args.simdisk.resolve(), args.song_length_ms)
    if args.launch:
        simdisk = args.simdisk.resolve()
        capture = args.capture.resolve() if args.capture else (
            simdisk.parent / "ipodhero-gate.bmp"
        )
        launch(simdisk.parent, simdisk, capture, args.video_driver,
               args.wait_results, args.song_length_ms, args.autoplay,
               args.plugin_parameter, args.expect_error)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
