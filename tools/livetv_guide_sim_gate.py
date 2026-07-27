#!/usr/bin/env python3
"""Drive the Live TV guide in the iPod 6G simulator and check its look.

The guide's colours were sampled from the DIRECTV receiver user guide
artwork (see docs/livetv-directv-guide-spec.md). This gate launches the app,
captures the guide and verifies that the real palette is on screen in the
right places, that the grid moves when the wheel turns, and that the picture
in guide window keeps decoding while the guide is up.
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

# Sampled DIRECTV colours the guide must actually paint, and where.
# (label, x, y, expected rgb, tolerance)
PALETTE_PROBES = (
    ("description block", 8, 60, (2, 111, 175), 26),
    ("time header", 150, 84, (18, 37, 73), 26),
    ("channel column", 20, 100, (18, 37, 73), 26),
    ("grid row", 150, 136, (9, 72, 113), 26),
    ("hint bar", 100, 232, (15, 86, 137), 30),
)

SELECTED_ROW_Y = 100  # first grid row, which the guide opens on


def build_channels(root: Path, channels) -> None:
    lines = ["# number\tcallsign\tname\tcategory\tlogo\tfavourite"]
    for number, callsign, name in channels:
        lines.append(f"{number}\t{callsign}\t{name}\tSeries\t\t1")
    (root / "channels.tsv").write_text("\n".join(lines) + "\n",
                                       encoding="utf-8")


def build_guide(root: Path, channels, clip_relative: str) -> None:
    """A full week of half-hour shows split by a two advert break."""
    lines = ["# chan\tday\tstart\tdur\tkind\ttitle\trating\tdesc\tpath"
             "\tblockstart\tblockdur"]
    show_len = 1500
    ad_len = 150
    block_len = show_len + ad_len * 2
    for number, callsign, name in channels:
        for day in range(7):
            start = 0
            index = 0
            while start + block_len <= 86400:
                index += 1
                title = f"{name} {index}"
                desc = f"Series, {name}. Episode {index}."
                lines.append(
                    f"{number}\t{day}\t{start}\t{show_len}\tS\t{title}"
                    f"\tTV-PG\t{desc}\t{clip_relative}\t{start}\t{block_len}"
                )
                for advert in range(2):
                    ad_start = start + show_len + advert * ad_len
                    lines.append(
                        f"{number}\t{day}\t{ad_start}\t{ad_len}\tA\t{title}"
                        f"\tTV-PG\t{desc}\t{clip_relative}\t{start}"
                        f"\t{block_len}"
                    )
                start += block_len
    (root / "guide.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")


def find_clip(repo: Path, build_dir: Path) -> Path:
    """Any MPEG the tree already ships is enough to prove decode works."""
    candidates = []
    for base in (build_dir / "simdisk" / "Videos", repo / "test-videos",
                 repo / "testdata"):
        if base.is_dir():
            candidates.extend(sorted(base.rglob("*.mpg")))
            candidates.extend(sorted(base.rglob("*.mpeg")))
    for candidate in candidates:
        if candidate.is_file() and candidate.stat().st_size > 64 * 1024:
            return candidate
    raise SystemExit(
        "no .mpg clip available for the Live TV gate; convert one into "
        "build-sim-ipod6g/simdisk/Videos first"
    )


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

    clip = find_clip(repo, build_dir)
    livetv = root / LIVETV_ROOT
    shows = livetv / "shows"
    shows.mkdir(parents=True)
    shutil.copy2(clip, shows / "gate.mpg")

    channels = [
        (100, "RTRO", "Retro Classics"),
        (101, "GAME", "Game Show Time"),
        (102, "SPRT", "Sports Tonight"),
        (103, "MOVI", "Movie Channel"),
        (104, "NEWS", "News Around"),
        (105, "KIDS", "Kids Corner"),
    ]
    build_channels(livetv, channels)
    build_guide(livetv, channels, "shows/gate.mpg")

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
        "start in screen: plugin\nresume: off\ntagcache_autoupdate: off\n",
        encoding="utf-8",
    )


def pixel(frame: Path, x: int, y: int):
    result = subprocess.run(
        ["magick", str(frame), "-format", f"%[pixel:p{{{x},{y}}}]", "info:"],
        check=False, capture_output=True, text=True,
    )
    match = re.search(r"\((\d+)[,\s]+(\d+)[,\s]+(\d+)", result.stdout)
    if not match:
        return None
    return tuple(int(value) for value in match.groups())


def close_enough(actual, expected, tolerance) -> bool:
    if actual is None:
        return False
    return all(abs(a - e) <= tolerance for a, e in zip(actual, expected))


def wait_for_guide(frame: Path, timeout: float = 45.0) -> None:
    """Wait until the DIRECTV blue description block is on screen."""
    wait_for_file(frame)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        sample = pixel(frame, 8, 60)
        if close_enough(sample, (2, 111, 175), 30):
            return
        time.sleep(0.25)
    raise SystemExit("the Live TV guide never appeared")


def check_palette(frame: Path) -> list:
    failures = []
    for label, x, y, expected, tolerance in PALETTE_PROBES:
        sample = pixel(frame, x, y)
        if not close_enough(sample, expected, tolerance):
            failures.append(
                f"{label} at ({x},{y}) is {sample}, expected ~{expected}")
    return failures


PIG_CROP = "83x64+237+3"
FULL_CROP = "320x200+0+20"


def motion(frame: Path, output: Path, prefix: str, crop: str,
           samples: int = 4, gap: float = 0.7) -> int:
    """Largest pixel difference between repeated crops of the live frame."""
    shots = []
    for index in range(samples):
        shot = output / f"{prefix}-{index}.png"
        subprocess.run(["magick", str(frame), "-crop", crop, "+repage",
                        str(shot)], check=True)
        shots.append(shot)
        time.sleep(gap)
    return max(changed_pixels(shots[0], other) for other in shots[1:])


def gold_pixels(frame: Path) -> int:
    """Count DIRECTV gold (#FEC425) pixels inside the grid area."""
    result = subprocess.run(
        ["magick", str(frame), "-crop", "320x132+0+91", "+repage",
         "-fuzz", "12%", "-fill", "black", "+opaque", "#FEC425",
         "-fill", "white", "-opaque", "#FEC425",
         "-format", "%[fx:int(mean*w*h)]", "info:"],
        check=False, capture_output=True, text=True,
    )
    try:
        return int(result.stdout.strip())
    except ValueError:
        return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path,
                        default=Path("build-sim-ipod6g"))
    parser.add_argument("--output", type=Path,
                        default=Path("/tmp/livetv-guide-gate"))
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
    with tempfile.TemporaryDirectory(prefix="livetv-guide-") as temp:
        root = Path(temp)
        prepare_root(repo, build_dir, root)
        frame = root / "frame.bmp"
        BUTTON_GATES.clear()
        BUTTON_GATES.update({
            "KP_5": gate_root / "select.gate",
            "KP_Decimal": gate_root / "menu.gate",
            "KP_Add": gate_root / "play.gate",
            "KP_2": gate_root / "scroll-forward.gate",
            "KP_8": gate_root / "scroll-back.gate",
        })
        environment = os.environ.copy()
        environment.update({
            # mpegplayer paces video off the PCM clock, so the audio device
            # has to consume in real time. The dummy driver never does, and
            # the picture would sit on its first frame for ever.
            "SDL_AUDIODRIVER": "disk",
            "SDL_DISKAUDIOFILE": str(args.output / "audio.raw"),
            "SDL_VIDEODRIVER": "x11",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
            "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
            "ROCKPOD_SIM_SELECT_GATE": str(BUTTON_GATES["KP_5"]),
            "ROCKPOD_SIM_MENU_GATE": str(BUTTON_GATES["KP_Decimal"]),
            "ROCKPOD_SIM_PLAY_ACTION_GATE": str(BUTTON_GATES["KP_Add"]),
            "ROCKPOD_SIM_SCROLL_FWD_GATE": str(BUTTON_GATES["KP_2"]),
            "ROCKPOD_SIM_SCROLL_BACK_GATE": str(BUTTON_GATES["KP_8"]),
        })
        process = subprocess.Popen(
            [str(simulator), "--zoom", "1", "--nobackground",
             "--root", str(root)],
            cwd=build_dir, env=environment,
        )
        try:
            window_id(process.pid)
            wait_for_guide(frame)
            time.sleep(1.0)

            guide = args.output / "livetv-guide.png"
            capture(frame, guide)
            failures.extend(check_palette(guide))
            gold = gold_pixels(guide)
            if gold < 200:
                failures.append(
                    f"only {gold} DIRECTV gold pixels in the grid; the "
                    "selected cell is missing")

            # The wheel must move the highlight between channels.
            tap(process.pid, "KP_2")
            time.sleep(0.6)
            moved = args.output / "livetv-guide-next-channel.png"
            capture(frame, moved)
            if changed_pixels(guide, moved) < 400:
                failures.append("the wheel did not move the guide highlight")

            pig_in_guide = motion(frame, args.output, "livetv-pig-guide",
                                  PIG_CROP)

            # SELECT tunes full screen, MENU comes back to the guide.
            tap(process.pid, "KP_5")
            time.sleep(2.5)
            watching = args.output / "livetv-watching.png"
            capture(frame, watching)
            if changed_pixels(guide, watching) < 20_000:
                failures.append("SELECT did not tune the channel full screen")
            if motion(frame, args.output, "livetv-full", FULL_CROP) < 200:
                failures.append("the tuned channel is not playing video")

            tap(process.pid, "KP_Decimal")
            time.sleep(2.5)
            back = args.output / "livetv-guide-return.png"
            capture(frame, back)
            if not close_enough(pixel(back, 8, 60), (2, 111, 175), 30):
                failures.append("MENU did not return to the guide")

            # The corner window must keep decoding while the guide is up:
            # that is the whole point of picture in guide.
            pig_after = motion(frame, args.output, "livetv-pig-return",
                               PIG_CROP)
            if max(pig_in_guide, pig_after) < 40:
                failures.append(
                    "the picture in guide window is not decoding video "
                    f"({pig_in_guide} then {pig_after} pixels changed)")
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

    print(f"Live TV guide gate passed; captures in {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
