#!/usr/bin/env python3
"""Drive the Live TV Weather channel in the iPod 6G simulator and check it.

The Weather channel is an ordinary Live TV channel (recognised by category,
not a new slot kind - see docs/livetv-weather-channel-spec.md) whose shows
are real MPEG files carrying a 64-second broadcast clock and continuous
music. This gate builds a minimal fixture and checks that:

* tuning the channel does not crash or hang;
* the synchronized carrier remains visible instead of being covered by a
  native forecast overlay;
* presenter/report phases decode full-screen.

The no-reopen/no-seek transition contract is also checked structurally in
``rockpod/tests/test_livetv.py``. The simulator's host audio/video sinks can
drain a carrier much faster than wall time, so audio mixing is verified on
the finished MPEG separately.
"""

from __future__ import annotations

import argparse
import datetime
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

PLUGIN_PATH = "/.rockbox/rocks/apps/livetv.rock"
LIVETV_ROOT = "Videos/LiveTV"
WEATHER_CHANNEL_NUMBER = 900
# Header background (LIVETV_HDR_BG, #122549) and ticker background
# (LIVETV_HINT_BG, #0F5689) - if either is missing, the panel chrome never
# actually painted.
HEADER_PROBE = (4, 4, (18, 37, 73), 22)
TICKER_PROBE = (4, 232, (15, 86, 137), 26)
SCENE_CROP = "320x70+0+22"

def build_channel(root: Path) -> None:
    lines = [
        "# number\tcallsign\tname\tcategory\tlogo\tfavourite",
        f"{WEATHER_CHANNEL_NUMBER}\tWX\tWeather\tWeather\t\t1",
    ]
    (root / "channels.tsv").write_text("\n".join(lines) + "\n",
                                       encoding="utf-8")


def build_guide(root: Path, clip_relative: str) -> None:
    """24 hour-long blocks/day, no ads - just enough to tune and watch."""
    lines = ["# chan\tday\tstart\tdur\tkind\ttitle\trating\tdesc\tpath"
             "\tblockstart\tblockdur"]
    now = datetime.datetime.now().astimezone()
    current_day = (now.weekday() + 1) % 7  # Python Monday=0; guide Sunday=0.
    current_seconds = now.hour * 3600 + now.minute * 60 + now.second
    for day in range(7):
        for hour in range(24):
            start = hour * 3600
            duration = 3600
            if day == current_day and hour == now.hour:
                # The simulator's MPEG parser scans this deliberately long
                # fixture for roughly 19 seconds before osd_play(). Starting
                # the live slot 45 seconds behind guide creation makes that
                # delay wrap to the beginning of the 64-second clock.
                start = max(hour * 3600, current_seconds - 46)
                duration = min(3600, 86400 - start)
            lines.append(
                f"{WEATHER_CHANNEL_NUMBER}\t{day}\t{start}\t{duration}\tS"
                f"\tLocal Forecast\tTV-G\tContinuous local weather."
                f"\t{clip_relative}\t{start}\t{duration}"
            )
    (root / "guide.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_bumper_clip(target: Path) -> None:
    """Build the production 16/8/8/8/8/16 carrier rhythm.

    Red and green stand in for the two presenter IDs and testsrc stands in
    for the longer report insert. Flat #094871 phases are where the player
    must hide decoded video and draw native forecast panels.
    """
    command = [
        "ffmpeg", "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "color=c=0x094871:s=320x240:r=20:d=16",
        "-f", "lavfi", "-i", "color=c=red:s=320x240:r=20:d=8",
        "-f", "lavfi", "-i", "color=c=0x094871:s=320x240:r=20:d=8",
        "-f", "lavfi", "-i", "color=c=green:s=320x240:r=20:d=8",
        "-f", "lavfi", "-i", "color=c=0x094871:s=320x240:r=20:d=8",
        "-f", "lavfi", "-i", "color=c=yellow:s=320x240:r=20:d=16",
        "-filter_complex",
        "[0:v][1:v][2:v][3:v][4:v][5:v]"
        "concat=n=6:v=1:a=0[v64];"
        "[v64]loop=loop=7:size=1280:start=0,setpts=N/(20*TB)[v]",
        "-map", "[v]",
        "-t", "512",
        "-c:v", "mpeg2video", "-pix_fmt", "yuv420p",
        "-bf", "0", "-g", "12", "-flags", "+low_delay", "-q:v", "2",
        "-an",
        "-packetsize", "2048", "-f", "mpeg",
        str(target),
    ]
    subprocess.run(command, check=True, capture_output=True)


def install_icons(repo: Path, rockbox_dir: Path) -> None:
    source = repo / "rockpod" / "assets" / "weather" / "icons"
    if not source.is_dir():
        return
    target = rockbox_dir / "rockpod" / "weather" / "icons"
    target.mkdir(parents=True, exist_ok=True)
    for bmp in source.glob("*.bmp"):
        shutil.copy2(bmp, target / bmp.name)


def build_forecast(rockbox_dir: Path) -> None:
    weather_dir = rockbox_dir / "rockpod" / "weather"
    weather_dir.mkdir(parents=True, exist_ok=True)

    today = datetime.date.today()
    now = datetime.datetime.now(datetime.UTC)
    lines = [
        "rockpod_weather_v1\tTest City\t45.0\t-66.0\tAmerica/Moncton\t"
        f"{now.strftime('%Y-%m-%dT%H:%M:%SZ')}\t{today.isoformat()}\tmetric",
    ]
    codes = ["clear", "partly_cloudy", "cloudy", "rain", "snow", "clear",
             "partly_cloudy"]
    for offset in range(7):
        date = today + datetime.timedelta(days=offset)
        code = codes[offset % len(codes)]
        lines.append(
            f"{date.isoformat()}\t{code}\t{code.replace('_', ' ').title()}"
            f"\t{10 + offset}\t{20 + offset}\t{offset * 10}\t{5 + offset}"
            f"\tNW\t06:1{offset}\t20:0{offset}\tsim"
        )
    for hour in range(24):
        stamp = f"{today.isoformat()}T{hour:02d}:00"
        code = codes[hour % len(codes)]
        is_day = 1 if 6 <= hour < 19 else 0
        lines.append(
            f"hourly\t{stamp}\t{code}\t{code.replace('_', ' ').title()}"
            f"\t{15 + (hour % 10)}\t{hour}\t{4 + (hour % 5)}\tNW\t{is_day}"
            "\tsim"
        )
    (weather_dir / "forecast.tsv").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")


def prepare_root(repo: Path, build_dir: Path, root: Path) -> None:
    source = build_dir / "simdisk" / ".rockbox"
    plugin = build_dir / "apps" / "plugins" / "livetv.rock"
    player = build_dir / "apps" / "plugins" / "mpegplayer" / "mpegplayer.rock"
    for required in (plugin, player, source):
        if not required.exists():
            raise SystemExit(f"missing Live TV gate input: {required}")

    rockbox = root / ".rockbox"
    rockbox.mkdir(parents=True)
    for name in ("fonts", "langs", "icons"):
        if (source / name).is_dir():
            shutil.copytree(source / name, rockbox / name)

    plugin_target = root / PLUGIN_PATH.lstrip("/")
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin, plugin_target)
    player_target = rockbox / "rocks" / "viewers" / "mpegplayer.rock"
    player_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(player, player_target)

    livetv = root / LIVETV_ROOT
    shows = livetv / "shows"
    shows.mkdir(parents=True)
    build_bumper_clip(shows / "weather.mpg")

    build_channel(livetv)
    build_forecast(rockbox)
    install_icons(repo, rockbox)

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build_dir)
    struct.pack_into("<IiI", entry, 0, START_SCREEN_HASH,
                     start_screen_lang_id(build_dir),
                     checksum or OPEN_PLUGIN_CHECKSUM)
    write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE,
                  "livetv.rock")
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE,
                  PLUGIN_PATH)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, "")
    (rockbox / "rocks" / "plugin.dat").write_bytes(entry)
    (rockbox / "config.cfg").write_text(
        "start in screen: plugin\nresume: off\nvolume: -80\n"
        "tagcache_autoupdate: off\n",
        encoding="utf-8",
    )


def pixel(frame: Path, x: int, y: int):
    result = subprocess.run(
        ["magick", str(frame), "-format", f"%[pixel:p{{{x},{y}}}]", "info:"],
        check=False, capture_output=True, text=True,
    )
    import re
    match = re.search(r"\((\d+)[,\s]+(\d+)[,\s]+(\d+)", result.stdout)
    if not match:
        return None
    return tuple(int(value) for value in match.groups())


def close_enough(actual, expected, tolerance) -> bool:
    if actual is None:
        return False
    return all(abs(a - e) <= tolerance for a, e in zip(actual, expected))


def changed_scene_pixels(first: Path, second: Path, output: Path) -> int:
    first_crop = output / "weather-scene-a.png"
    second_crop = output / "weather-scene-b.png"
    subprocess.run(
        ["magick", str(first), "-crop", SCENE_CROP, "+repage",
         str(first_crop)],
        check=True,
    )
    subprocess.run(
        ["magick", str(second), "-crop", SCENE_CROP, "+repage",
         str(second_crop)],
        check=True,
    )
    return changed_pixels(first_crop, second_crop)


def wait_for_chrome(frame: Path, timeout: float = 45.0,
                    capture_to: Path | None = None) -> None:
    wait_for_file(frame)
    deadline = time.monotonic() + timeout
    probe_x, probe_y, expected, tolerance = HEADER_PROBE
    ticker_x, ticker_y, ticker_expected, ticker_tolerance = TICKER_PROBE
    while time.monotonic() < deadline:
        if (close_enough(pixel(frame, probe_x, probe_y),
                         expected, tolerance) and
                close_enough(pixel(frame, ticker_x, ticker_y),
                             ticker_expected, ticker_tolerance)):
            if capture_to is not None:
                shutil.copy2(frame, capture_to)
            return
        time.sleep(0.25)
    raise SystemExit("the Weather channel panel chrome never appeared")


def wait_for_native_weather_panel(frame: Path, timeout: float = 70.0,
                                  capture_to: Path | None = None) -> None:
    """Wait past presenter/report video for an actual native forecast phase."""
    fixture_colors = (
        (9, 72, 113),   # flat carrier phase
        (255, 0, 0),    # first presenter stand-in
        (0, 128, 0),    # second presenter stand-in
        (255, 255, 0),  # report stand-in
    )
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        scene = pixel(frame, 270, 30)
        header = pixel(frame, *HEADER_PROBE[:2])
        ticker = pixel(frame, *TICKER_PROBE[:2])
        source_video = any(
            close_enough(scene, color, 16) for color in fixture_colors)
        if (not source_video and
                close_enough(header, HEADER_PROBE[2], HEADER_PROBE[3]) and
                close_enough(ticker, TICKER_PROBE[2],
                             TICKER_PROBE[3])):
            if capture_to is not None:
                shutil.copy2(frame, capture_to)
            return
        time.sleep(0.2)
    raise SystemExit("the native Weather forecast phase never appeared")


def wait_for_presenter_video(frame: Path, timeout: float = 24.0) -> None:
    """Presenter IDs are red/green and the report is yellow in this fixture."""
    wait_for_file(frame)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        sample = pixel(frame, 160, 120)
        if sample is not None:
            red = sample[0] > 180 and sample[1] < 80 and sample[2] < 80
            green = sample[1] > 80 and sample[0] < 80 and sample[2] < 80
            report = sample[0] > 180 and sample[1] > 180 and sample[2] < 80
            if red or green or report:
                return
        time.sleep(0.2)
    raise SystemExit("the Weather presenter video phase never appeared")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path,
                        default=Path("build-sim-ipod6g"))
    parser.add_argument("--output", type=Path,
                        default=Path("/tmp/livetv-weather-gate"))
    args = parser.parse_args()

    repo = Path(__file__).resolve().parent.parent
    build_dir = args.build_dir.resolve()
    simulator = build_dir / "rockboxui"
    if not simulator.is_file():
        raise SystemExit(f"missing simulator: {simulator}")
    for command in ("magick", "xdotool"):
        if shutil.which(command) is None:
            raise SystemExit(f"missing required command: {command}")

    args.output.mkdir(parents=True, exist_ok=True)
    gate_root = args.output / ".button-gates"
    shutil.rmtree(gate_root, ignore_errors=True)
    gate_root.mkdir(parents=True)

    failures = []
    with tempfile.TemporaryDirectory(prefix="livetv-weather-") as temp:
        root = Path(temp)
        prepare_root(repo, build_dir, root)
        frame = root / "frame.bmp"
        BUTTON_GATES.clear()
        environment = os.environ.copy()
        environment.update({
            # This fixture is video-only so the simulator uses its real-time
            # video clock instead of a host audio sink that can drain PCM far
            # faster than wall time. Production carriers still include the
            # user's continuous music bed.
            "SDL_AUDIODRIVER": "dummy",
            "SDL_VIDEODRIVER": "x11",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
            "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
            "ROCKPOD_SIM_SELECT_GATE": str(gate_root / "select.gate"),
            "ROCKPOD_SIM_MENU_GATE": str(gate_root / "menu.gate"),
        })
        # Timestamp the live schedule only after the relatively expensive
        # fixture encode, immediately before launch, so its two-second
        # tune-in offset remains deterministic.
        build_guide(root / LIVETV_ROOT, "shows/weather.mpg")
        process = subprocess.Popen(
            [str(simulator), "--zoom", "1", "--nobackground",
             "--root", str(root)],
            cwd=build_dir, env=environment,
        )
        try:
            window_id(process.pid)
            wait_for_file(frame)
            time.sleep(1.0)

            # The guide opens with the (only) channel already decoding into
            # the corner box. SELECT tunes it full screen, which is where
            # the Weather channel's panel chrome takes over.
            tap(process.pid, "KP_5")
            time.sleep(2.5)

            wait_for_presenter_video(frame)
            presenter = args.output / "weather-presenter.png"
            shutil.copy2(frame, presenter)
            center = pixel(presenter, 160, 120)
            if center is None:
                failures.append("presenter carrier frame was not captured")

        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1

    print(f"Live TV Weather channel gate passed; captures in {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
