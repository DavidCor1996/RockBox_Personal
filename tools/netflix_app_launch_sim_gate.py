#!/usr/bin/env python3
"""Verify the Netflix app boot ident before its iPodJS landing screen."""

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

from PIL import Image

from rockachievements_ui_sim_gate import (
    capture,
    changed_pixels,
    tap,
    wait_for_file,
    window_id,
)

VIDEO_PATH = "/Videos/Downloaded/segtest.rvp"
RESUME_MAGIC = 0x52565031


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


def seed_netflix_resume(root: Path) -> None:
    rockbox = root / ".rockbox"
    (rockbox / "videolist/netflix-last.path").write_text(
        VIDEO_PATH + "\n", encoding="utf-8"
    )
    crc = rockbox_crc32(VIDEO_PATH)
    resume = rockbox / "rocks/apps" / f"openh264-resume-{crc:08x}.dat"
    resume.parent.mkdir(parents=True, exist_ok=True)
    resume.write_bytes(struct.pack("<IIii", RESUME_MAGIC, crc, 40, 600))


def seed_netflix_watched(root: Path) -> None:
    # mpegplayer clears its resume point at end of stream, so completion is
    # recorded separately. This is the record it writes.
    (root / ".rockbox" / "videolist" / "netflix-watched.tsv").write_text(
        VIDEO_PATH + "\n", encoding="utf-8"
    )


def prepare_root(repo: Path, build_dir: Path, root: Path) -> None:
    source = build_dir / "simdisk"
    rockbox_source = source / ".rockbox"
    rockbox = root / ".rockbox"
    netflix = repo / "assets" / "ipodjs" / "rockbox" / "netflix"
    for required in (
        build_dir / "rockboxui",
        netflix / "launch" / "intro-320x180.rgb565",
        netflix / "launch" / "intro-20000-mono.mulaw",
    ):
        if not required.exists():
            raise SystemExit(f"missing Netflix gate input: {required}")

    rockbox.mkdir(parents=True)
    for name in ("fonts", "langs", "icons", "videolist"):
        if (rockbox_source / name).is_dir():
            shutil.copytree(rockbox_source / name, rockbox / name)
    shutil.copytree(netflix, rockbox / "ipodjs" / "netflix")
    video = root / VIDEO_PATH.lstrip("/")
    video.parent.mkdir(parents=True)
    video.write_bytes(b"netflix-layout-gate")
    videolist = rockbox / "videolist"
    videolist.mkdir(exist_ok=True)
    banner_id = "simulator-feature"
    banner_dir = videolist / "netflix-banner"
    banner_dir.mkdir()
    with Image.open(
        repo / "rockpod/assets/imdb_video_artwork/recess-banner.jpg"
    ) as source_banner:
        source_banner.convert("RGB").resize(
            (320, 180), Image.Resampling.LANCZOS
        ).save(banner_dir / f"{banner_id}.bmp", "BMP")
    with Image.open(
        repo / "rockpod/assets/imdb_video_artwork/recess-show.jpg"
    ) as source_poster:
        poster = source_poster.convert("RGB")
        for folder, size in (
            ("netflix-landing", (72, 108)),
            ("netflix-detail", (96, 144)),
            ("netflix", (28, 42)),
        ):
            target_dir = videolist / folder
            target_dir.mkdir()
            poster.resize(size, Image.Resampling.LANCZOS).save(
                target_dir / "segtest.bmp", "BMP"
            )
    columns = (
        "video_id", "thumb", "preview", "title", "kind", "group_key",
        "device_path", "show", "season", "episode", "duration", "locked",
        "year", "genre", "rating", "plot_short", "plot_long",
        "content_rating", "netflix_poster", "netflix_detail", "show_art_id",
        "season_art_id", "show_plot", "intro_start", "intro_end",
        "credits_start", "credits_duration", "external_rating_tenths",
        "external_rating_votes", "banner_art_id",
    )
    row = (
        "segtest", "", "", "Simulator Feature", "movie", "segtest",
        "Videos/Downloaded/segtest.rvp", "", "", "", "20", "0", "2008",
        "Drama", "4", "Simulator feature.", "Simulator feature.", "PG",
        "", "", "", "", "Simulator feature.", "", "", "", "", "",
        "", banner_id,
    )
    assert len(columns) == len(row) == 30
    (videolist / "index.tsv").write_text(
        "# rockpod videolist v8\n"
        + "\t".join(columns) + "\n"
        + "\t".join(row) + "\n",
        encoding="utf-8",
    )
    (rockbox / "config.cfg").write_text(
        "ui engine: ipodjs\n"
        "ui engine video appearance: netflix\n"
        "start in screen: root\n"
        "resume: off\n"
        "tagcache_autoupdate: off\n",
        encoding="utf-8",
    )


def pixel_rgb(frame: Path, x: int, y: int) -> tuple[int, int, int] | None:
    result = subprocess.run(
        ["magick", str(frame), "-format", f"%[pixel:p{{{x},{y}}}]", "info:"],
        check=False,
        capture_output=True,
        text=True,
    )
    values = [int(value) for value in re.findall(r"\d+", result.stdout)]
    return tuple(values[:3]) if len(values) >= 3 else None


def wait_for_launch(frame: Path, timeout: float = 8.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        top = pixel_rgb(frame, 160, 5)
        bottom = pixel_rgb(frame, 160, 234)
        center = pixel_rgb(frame, 160, 120)
        if (
            top and bottom and center
            and max(top) < 8
            and max(bottom) < 8
            and max(center) > 30
        ):
            return
        time.sleep(0.04)
    raise SystemExit("aspect-fit modern Netflix app launch frame did not appear")


def wait_for_landing(frame: Path, timeout: float = 8.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        header_left = pixel_rgb(frame, 105, 5)
        header_right = pixel_rgb(frame, 310, 5)
        footer = pixel_rgb(frame, 2, 234)
        if (
            header_left and header_right and footer
            and header_left[0] > 100
            and header_left[0] > header_left[1] * 3
            and header_right[0] > 100
            and header_right[0] > header_right[1] * 3
            and max(footer) < 60
        ):
            return
        time.sleep(0.08)
    raise SystemExit(
        "Netflix landing screen did not follow launch ident: "
        f"header_left={pixel_rgb(frame, 105, 5)} "
        f"header_right={pixel_rgb(frame, 310, 5)} "
        f"footer={pixel_rgb(frame, 2, 234)}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/netflix-app-launch-gate")
    )
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    build_dir = args.build_dir.resolve()
    simulator = build_dir / "rockboxui"
    netflix = repo / "assets" / "ipodjs" / "rockbox" / "netflix"
    args.output.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="netflix-app-launch-") as temp:
        root = Path(temp)
        prepare_root(repo, build_dir, root)
        frame = root / "frame.bmp"
        audio = root / "netflix-audio.raw"
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
        process = subprocess.Popen(
            [str(simulator), "--zoom", "1", "--nobackground", "--root", str(root)],
            cwd=build_dir,
            env=environment,
        )
        try:
            window_id(process.pid)
            wait_for_file(frame)
            time.sleep(1.0)
            tap(process.pid, "KP_2")
            tap(process.pid, "KP_2")
            selected = args.output / "netflix-selected.png"
            capture(frame, selected)
            tap(process.pid, "KP_5")
            wait_for_launch(frame)
            early = args.output / "netflix-launch-early.png"
            late = args.output / "netflix-launch-late.png"
            capture(frame, early)
            time.sleep(0.7)
            capture(frame, late)
            if changed_pixels(early, late) < 100:
                raise SystemExit("modern Netflix app launch animation did not advance")
            wait_for_landing(frame)
            landing = args.output / "netflix-landing.png"
            capture(frame, landing)
            if changed_pixels(late, landing) < 2_000:
                raise SystemExit("Netflix launch did not hand off to its main menu")
            expected = pixel_rgb(
                netflix / "categories" / "movies.96x144x24.bmp", 48, 72
            )
            actual = pixel_rgb(landing, 160, 122)
            if (
                not expected or not actual or
                max(abs(expected[i] - actual[i]) for i in range(3)) > 45
            ):
                raise SystemExit("Movies category did not use its fixed cover")

            tap(process.pid, "KP_Decimal")
            time.sleep(0.5)
            seed_netflix_resume(root)
            seed_netflix_watched(root)
            tap(process.pid, "KP_5")
            wait_for_launch(frame)
            second_early = args.output / "netflix-second-launch-early.png"
            second_late = args.output / "netflix-second-launch-late.png"
            capture(frame, second_early)
            time.sleep(0.7)
            capture(frame, second_late)
            if changed_pixels(second_early, second_late) < 100:
                raise SystemExit("second Netflix launch froze on the boot frame")
            wait_for_landing(frame)
            second_landing = args.output / "netflix-second-landing.png"
            capture(frame, second_landing)
            # The resume title now renders as a wide card carrying both
            # actions: a red progress fill under the title, RESUME focused
            # (white) and PLAY FROM BEGINNING unfocused (dark) beside it.
            # Sample below the pill labels: y=190 lands inside the glyphs.
            progress_fill = pixel_rgb(second_landing, 98, 126)
            resume_pill = pixel_rgb(second_landing, 20, 200)
            restart_pill = pixel_rgb(second_landing, 300, 178)
            if (
                not progress_fill or progress_fill[0] < 120 or
                progress_fill[0] < progress_fill[1] * 3
            ):
                raise SystemExit("Resume card progress fill did not appear")
            if not resume_pill or min(resume_pill) < 200:
                raise SystemExit("Resume action was not focused on the card")
            if not restart_pill or max(restart_pill) > 120:
                raise SystemExit(
                    "Play From Beginning action was not drawn beside Resume"
                )

            # The wheel must reach the second action and then leave the card.
            # KP_2 is BUTTON_SCROLL_FWD in uisimulator/buttonmap/ipod.c.
            tap(process.pid, "KP_2")
            time.sleep(0.4)
            focused = args.output / "netflix-resume-restart-focused.png"
            capture(frame, focused)
            resume_pill = pixel_rgb(focused, 20, 200)
            restart_pill = pixel_rgb(focused, 300, 178)
            if (
                not restart_pill or min(restart_pill) < 200 or
                not resume_pill or max(resume_pill) > 120
            ):
                raise SystemExit(
                    "wheel did not move focus to Play From Beginning"
                )

            # One more step leaves the card for the first category. Select
            # browses immediately, and the movie rail must remain poster-first.
            tap(process.pid, "KP_2")
            time.sleep(0.6)
            tap(process.pid, "KP_5")
            time.sleep(1.5)
            movies = args.output / "netflix-movies-cover-art.png"
            capture(frame, movies)
            expected_poster = pixel_rgb(
                root / ".rockbox/videolist/netflix-detail/segtest.bmp",
                48, 72,
            )
            actual_poster = pixel_rgb(movies, 160, 122)
            if (
                not expected_poster or not actual_poster or
                max(abs(expected_poster[i] - actual_poster[i])
                    for i in range(3)) > 45
            ):
                raise SystemExit(
                    "movie rail did not use its synced photographic cover art"
                )
            badge = pixel_rgb(movies, 197, 61)
            badge_field = pixel_rgb(movies, 193, 57)
            if (
                not badge_field or badge_field[0] < 120 or
                badge_field[0] < badge_field[1] * 3
            ):
                raise SystemExit("watched badge did not appear on the poster")
            if not badge or min(badge) < 180:
                raise SystemExit("watched badge check glyph was not drawn")

            # Select opens the episode/movie Details screen. Only there may
            # the verified 16:9 banner occupy x=0..319, y=32..211.
            tap(process.pid, "KP_5")
            time.sleep(0.8)
            detail = args.output / "netflix-banner-details.png"
            capture(frame, detail)
            banner_path = (
                root / ".rockbox/videolist/netflix-banner"
                / "simulator-feature.bmp"
            )
            with (
                Image.open(banner_path).convert("RGB") as expected_banner,
                Image.open(detail).convert("RGB") as detail_frame,
            ):
                rendered_banner = detail_frame.crop((0, 32, 320, 212))
                mismatches = sum(
                    1
                    for expected, actual in zip(
                        expected_banner.get_flattened_data(),
                        rendered_banner.get_flattened_data(),
                    )
                    if max(abs(expected[i] - actual[i]) for i in range(3)) > 24
                )
                if mismatches > 300:
                    raise SystemExit(
                        "Netflix banner was cropped, overlaid, or misplaced: "
                        f"{mismatches} mismatched pixels"
                    )
                if detail_frame.getpixel((2, 212))[0] > 40:
                    raise SystemExit(
                        "Netflix banner overlapped its separate footer row"
                    )

            # This title has resume state. The compact action bar must start
            # on Resume, then move to Play From Beginning with one wheel step.
            resume_underline = pixel_rgb(detail, 30, 237)
            restart_underline = pixel_rgb(detail, 200, 237)
            if (
                not resume_underline or resume_underline[0] < 120 or
                resume_underline[0] < resume_underline[1] * 3 or
                not restart_underline or max(restart_underline) > 70
            ):
                raise SystemExit("Details did not focus its Resume action")
            tap(process.pid, "KP_2")
            time.sleep(0.4)
            restart = args.output / "netflix-details-play-from-beginning.png"
            capture(frame, restart)
            resume_underline = pixel_rgb(restart, 30, 237)
            restart_underline = pixel_rgb(restart, 200, 237)
            if (
                not restart_underline or restart_underline[0] < 120 or
                restart_underline[0] < restart_underline[1] * 3 or
                not resume_underline or max(resume_underline) > 70
            ):
                raise SystemExit(
                    "Details did not focus Play From Beginning"
                )
        finally:
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if not audio.is_file() or audio.stat().st_size < 100_000:
            raise SystemExit("Netflix simulator audio output was not produced")
        sample = audio.read_bytes()
        if not any(sample):
            raise SystemExit("Netflix simulator audio output was silent")
    print(
        "Netflix app launch simulator gate passed twice with fixed category "
        "art, cover-art browsing, a Details-only 320x180 banner, and "
        "Resume / Play From Beginning actions; "
        f"captures: {args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
