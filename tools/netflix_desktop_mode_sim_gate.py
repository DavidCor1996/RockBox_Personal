#!/usr/bin/env python3
"""Exercise the Video-Sync-only Netflix app in Desktop Mode at 1080p."""

from __future__ import annotations

import os
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from livetv_guide_sim_gate import capture, changed_pixels, pixel, prepare_root
from rockachievements_ui_sim_gate import (
    BUTTON_GATES,
    start_screen_lang_id,
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


PLUGIN_PATH = "/.rockbox/rocks/apps/desktop_mode.rock"
DOCK_POINT = (1110, 1028)


def write_pointer(
    path: Path, buttons: int, point: tuple[int, int] = DOCK_POINT
) -> None:
    temporary = path.with_suffix(".new")
    temporary.write_text(
        f"{point[0]:04d} {point[1]:04d} {buttons:02d}\n",
        encoding="ascii",
    )
    os.replace(temporary, path)


def click_pointer(path: Path, point: tuple[int, int]) -> None:
    write_pointer(path, 1, point)
    time.sleep(0.15)
    write_pointer(path, 0, point)
    time.sleep(0.2)


def manifest_row(
    video_id: str,
    title: str,
    kind: str,
    path: str,
    show: str = "",
    poster: str = "netflix-detail/gate.bmp",
) -> str:
    fields = [
        video_id,
        f"thumbs/{video_id}.bmp",
        f"previews/{video_id}.bmp",
        title,
        kind,
        f"{kind}:{video_id}",
        path,
        show,
        "1" if show else "",
        "1" if show else "",
        "12",
        "0",
        "2001",
        "Comedy",
        "",
        "A title supplied by the Video Sync catalogue.",
        "",
        "PG",
        f"netflix/{video_id}.bmp",
        poster,
        "",
        "",
        "",
    ]
    return "\t".join(fields)


def install(repo: Path, build: Path, root: Path) -> None:
    for name in ("desktop_mode", "netflix_desktop"):
        source = build / f"apps/plugins/{name}.rock"
        target = root / f".rockbox/rocks/apps/{name}.rock"
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    pack = repo / "rockpod/.snow_leopard_desktop/packs/ipod-320x240"
    for relative in (
        ".rockbox/rocks/apps/desktop_mode_snow_leopard",
        ".rockbox/rocks.data/desktop_mode_snow_leopard",
    ):
        shutil.copytree(pack, root / relative)
    shutil.copytree(
        repo / "assets/ipodjs/rockbox/netflix",
        root / ".rockbox/ipodjs/netflix",
    )
    shutil.copytree(
        repo / "assets/ipodjs/rockbox/sitekick",
        root / ".rockbox/sitekick",
    )

    source_clip = root / "Videos/LiveTV/shows/gate.mpg"
    movies = root / "Videos/Movies"
    shows = root / "Videos/TV Shows/Gate Show/Season 01"
    movies.mkdir(parents=True)
    shows.mkdir(parents=True)
    shutil.copy2(source_clip, movies / "Gate Movie.mpg")
    shutil.copy2(source_clip, shows / "S01E01 - Gate Episode.mpg")

    videolist = root / ".rockbox/videolist"
    details = videolist / "netflix-detail"
    details.mkdir(parents=True, exist_ok=True)
    shutil.copy2(
        repo
        / "assets/ipodjs/rockbox/netflix/categories"
        / "movies.96x144x24.bmp",
        details / "gate.bmp",
    )
    rows = [
        "# rockpod videolist v6",
        "video_id\tthumb\tpreview\ttitle\tkind\tgroup_key\tdevice_path"
        "\tshow\tseason\tepisode\tduration\tlocked\tyear\tgenre\trating"
        "\tplot_short\tplot_long\tcontent_rating\tnetflix_poster"
        "\tnetflix_detail\tshow_art_id\tseason_art_id\tshow_plot",
        manifest_row(
            "gate-movie",
            "Gate Movie",
            "movie",
            "Videos/Movies/Gate Movie.mpg",
        ),
        manifest_row(
            "gate-show",
            "Gate Episode",
            "show",
            "Videos/TV Shows/Gate Show/Season 01/S01E01 - Gate Episode.mpg",
            "Gate Show",
        ),
        manifest_row(
            "youtube",
            "YouTube Download",
            "movie",
            "Videos/Downloaded/youtube.mpg",
        ),
        manifest_row(
            "live",
            "Live TV",
            "movie",
            "Videos/LiveTV/shows/gate.mpg",
        ),
    ]
    (videolist / "index.tsv").write_text(
        "\n".join(rows) + "\n",
        encoding="utf-8",
    )

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build)
    struct.pack_into(
        "<IiI",
        entry,
        0,
        START_SCREEN_HASH,
        start_screen_lang_id(build),
        checksum or OPEN_PLUGIN_CHECKSUM,
    )
    write_cstring(
        entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE,
        "desktop_mode.rock",
    )
    write_cstring(
        entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH
    )
    write_cstring(
        entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, ""
    )
    (root / ".rockbox/rocks/plugin.dat").write_bytes(entry)
    (root / ".rockbox/config.cfg").write_text(
        "start in screen: plugin\nresume: off\ntagcache_autoupdate: off\n",
        encoding="utf-8",
    )


def wait_for_netflix(frame: Path, timeout_capture: Path) -> None:
    deadline = time.monotonic() + 15.0
    while time.monotonic() < deadline:
        sample = pixel(frame, 1250, 232)
        if sample and sample[0] > 120 and sample[1] < 55:
            return
        time.sleep(0.2)
    capture(frame, timeout_capture)
    raise SystemExit("the 1080p Netflix window never appeared")


def wait_for_player(log_path: Path) -> None:
    deadline = time.monotonic() + 15.0
    expected = (
        "ROCKBOX_SIM_PLUGIN chain: "
        "/.rockbox/rocks/viewers/mpegplayer.rock "
        "param=netflix:/Videos/Movies/Gate Movie.mpg"
    )
    while time.monotonic() < deadline:
        if expected in log_path.read_text(errors="replace"):
            return
        time.sleep(0.2)
    raise SystemExit("Netflix did not hand the selected Video Sync file "
                     "to mpegplayer")


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    build = repo / "build-sim-desktop1080"
    simulator = build / "rockboxui"
    output = Path("/tmp/netflix-desktop-mode-gate")
    output.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="netflix-desktop-") as temporary:
        root = Path(temporary)
        prepare_root(repo, build, root)
        install(repo, build, root)
        pointer = root / ".rockbox/host-pointer"
        frame = root / "frame.bmp"
        log_path = output / "simulator.log"
        write_pointer(pointer, 0)

        gate_root = output / ".button-gates"
        shutil.rmtree(gate_root, ignore_errors=True)
        gate_root.mkdir()
        BUTTON_GATES.clear()
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_AUDIODRIVER": "disk",
                "SDL_DISKAUDIOFILE": str(output / "audio.raw"),
                "SDL_VIDEODRIVER": "x11",
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
                "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
                "ROCKBOX_SIM_PLUGIN": PLUGIN_PATH,
            }
        )
        with log_path.open("wb") as log:
            process = subprocess.Popen(
                [
                    str(simulator),
                    "--zoom", "1",
                    "--nobackground",
                    "--root", str(root),
                ],
                cwd=build,
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        try:
            window_id(process.pid)
            wait_for_file(frame)
            time.sleep(2.0)
            click_pointer(pointer, DOCK_POINT)
            wait_for_netflix(
                frame, output / "netflix-window-timeout.png"
            )
            time.sleep(1.0)
            window = output / "netflix-window.png"
            capture(frame, window)
            outside = pixel(window, 100, 100)
            if not outside or max(outside) < 16:
                raise SystemExit(
                    "Netflix blacked out the Desktop outside its window"
                )

            # Exercise every mouse-control family before playback: tabs,
            # cards, carousel arrows, hover feedback, and Play.
            write_pointer(pointer, 0, (827, 523))
            time.sleep(0.3)
            hover = output / "netflix-hover.png"
            capture(frame, hover)
            if changed_pixels(window, hover) < 20:
                raise SystemExit("Netflix Play has no mouse hover feedback")

            click_pointer(pointer, (1015, 245))  # TV SHOWS
            click_pointer(pointer, (710, 245))   # HOME
            click_pointer(pointer, (835, 700))   # second cover
            click_pointer(pointer, (655, 700))   # previous arrow
            click_pointer(pointer, (827, 523))   # PLAY
            wait_for_player(log_path)
            time.sleep(1.0)
            playing = output / "netflix-playing.png"
            capture(frame, playing)
            if changed_pixels(window, playing) < 1000:
                raise SystemExit("Netflix playback did not replace the window")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

        log_text = log_path.read_text(errors="replace")
        if "netflix desktop: catalogue=2 excluded=2" not in log_text:
            raise SystemExit(
                "Netflix did not enforce the Video Sync source filter"
            )
        for evidence in (
            "netflix desktop: category=TV SHOWS",
            "netflix desktop: category=HOME",
            "netflix desktop: mouse card selected=1",
            "netflix desktop: selection step=-1 selected=0",
        ):
            if evidence not in log_text:
                raise SystemExit(
                    f"Netflix mouse control was not exercised: {evidence}"
                )

    print(f"Netflix Desktop Mode gate passed; captures in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
