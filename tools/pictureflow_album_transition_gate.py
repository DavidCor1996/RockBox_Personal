#!/usr/bin/env python3
"""Exercise the real PictureFlow action/render loop in a disposable simulator.

Supply a matching simulator, installed runtime, and host database executable.
This creates its own music/art fixtures; no physical player is touched.
"""
import argparse
import csv
import io
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import time

from PIL import Image, ImageChops, ImageDraw


def wait_until(predicate, timeout=20):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError("simulator condition timed out")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--database-tool", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cycles", type=int, default=30)
    parser.add_argument("--long-titles", action="store_true")
    parser.add_argument("--tracks", type=int, default=3)
    parser.add_argument("--retail-reference", action="store_true",
                        help="Use the album/track labels from the Classic reference")
    args = parser.parse_args()
    if args.cycles < 1 or args.tracks < 1:
        parser.error("--cycles and --tracks must be positive")
    build, source, output = (p.resolve() for p in
                             (args.build, args.runtime, args.output))
    output.mkdir(parents=True, exist_ok=True)
    root = output / "runtime"
    if root.exists():
        raise SystemExit("Use a fresh output directory; fixtures are retained.")
    rbdir = root / ".rockbox"
    shutil.copytree(source / ".rockbox", rbdir, symlinks=True,
                    ignore=shutil.ignore_patterns("previews", "database*.tcd",
                        "*.log", "*.tsv", "pictureflow", "pictureflow.cfg"))
    demo = rbdir / "rocks/demos"
    demo.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / "apps/plugins/pictureflow/pictureflow.rock", demo)
    for asset in (source / ".rockbox/rocks/demos").glob("pictureflow*.bmp"):
        shutil.copy2(asset, demo)
    music = root / "Music"
    music.mkdir()
    sample = next((source / "Music").rglob("*.mp3"))
    colors = [(190, 30, 40), (30, 160, 60), (35, 65, 200), (190, 140, 20),
              (155, 30, 170), (25, 155, 170), (180, 80, 30), (80, 110, 160)]
    for album, color in enumerate(colors):
        directory = music / f"Album {album:02d}"
        directory.mkdir()
        art = Image.new("RGB", (128, 128), color)
        draw = ImageDraw.Draw(art)
        draw.rectangle((12, 12, 115, 115), outline="white", width=3)
        draw.text((45, 55), f"{album:02d}", fill="white")
        art.save(directory / "cover.bmp")
        for track in range(args.tracks):
            title = f"Track {track + 1:02d}"
            album_name = f"Album {album:02d}"
            artist_name = "Transition Test"
            duration = 600
            if args.retail_reference:
                album_name = ("Guitar Hero" if album == 4 else
                              f"{'AA' if album < 4 else 'ZZ'} Album {album:02d}")
                artist_name = "Megadeath"
                labels = [("I Love Rock 'n' Roll", 186), ("I Wanna Be Sedated", 172),
                          ("Thunder Kiss '65", 244), ("Smoke on the Water", 363),
                          ("Infected", 240), ("Iron Man", 252),
                          ("More Than a Feeling", 306)]
                title, duration = labels[track % len(labels)]
            if args.long_titles:
                title += " — Café déjà vu: a very long album track title to scroll"
            subprocess.run(["ffmpeg", "-v", "error", "-i", str(sample),
                "-t", str(duration), "-c", "copy", "-metadata", f"album={album_name}",
                "-metadata", f"album_artist={artist_name}",
                "-metadata", f"artist={artist_name}",
                "-metadata", f"title={title}",
                "-metadata", f"track={track + 1}",
                str(directory / f"{track:02d}.mp3")], check=True)
    with (output / "database.log").open("w") as log:
        subprocess.run([str(args.database_tool.resolve())], cwd=root,
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    config = (rbdir / "config.cfg").read_text()
    config += ("\nui engine: ipodjs\nstart in screen: root\n"
               "root menu order: pictureflow,database,settings,wps\n"
               "notification banners: off\ntagcache autoupdate: off\n"
               "backlight timeout: on\n"
               "font: /.rockbox/fonts/14-Adobe-Helvetica-Bold.fnt\n")
    (rbdir / "config.cfg").write_text(config)
    (rbdir / "pictureflow-trace.enable").touch()
    frame = root / "frame.bmp"
    env = dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy",
        SDL_RENDER_DRIVER="software", ROCKPOD_SIM_IPODJS_TRACE="1",
        ROCKPOD_SIM_PREVIEW_BMP=str(frame), ROCKPOD_SIM_PREVIEW_INTERVAL_MS="0")
    for name, variable in [("select", "SELECT"), ("menu", "MENU"),
                           ("next", "SCROLL_FWD"), ("prev", "SCROLL_BACK")]:
        env[f"ROCKPOD_SIM_{variable}_GATE"] = str(root / f"{name}.gate")
    def tap(name, settle=.4):
        gate = root / f"{name}.gate"
        gate.touch()
        time.sleep(.08)
        gate.unlink()
        time.sleep(settle)
    def capture(name):
        # Retry atomic display-dump publication while the simulator renders.
        for _ in range(30):
            try:
                with Image.open(frame) as image:
                    rgb = image.convert("RGB")
                    assert rgb.getbbox() is not None, "black framebuffer"
                    rgb.save(output / f"{name}.png")
                return
            except (OSError, ValueError):
                time.sleep(.05)
        raise AssertionError("no complete framebuffer capture")
    def capture_motion(name, key):
        gate = root / f"{key}.gate"
        gate.touch()
        started = time.monotonic()
        frames = []
        while time.monotonic() - started < .6:
            if time.monotonic() - started >= .08 and gate.exists():
                gate.unlink()
            try:
                with Image.open(frame) as current:
                    current = current.convert("RGB")
                    if not frames or current.tobytes() != frames[-1].tobytes():
                        frames.append(current.copy())
            except (OSError, ValueError):
                pass
            time.sleep(.008)
        if gate.exists():
            gate.unlink()
        assert len(frames) >= 3, "no visible motion captured"
        directory = output / name
        directory.mkdir()
        for index, picture in enumerate(frames):
            picture.save(directory / f"{index:02d}.png")
        sheet = Image.new("RGB", (320 * len(frames), 240), "white")
        for index, picture in enumerate(frames):
            sheet.paste(picture, (index * 320, 0))
        sheet.save(output / f"{name}.png")
    log = (output / "simulator.log").open("w")
    process = subprocess.Popen([str(build / "rockboxui"), "--zoom", "1",
                               "--nobackground", "--root", str(root)],
                              cwd=build, env=env, stdout=log, stderr=log)
    try:
        wait_until(lambda: frame.exists())
        time.sleep(2)
        tap("select", 8)
        if args.retail_reference:
            for _ in range(4):
                tap("next", .65)
            time.sleep(1)  # Compare settled geometry, not the last wheel glide.
        capture("browse")
        if args.retail_reference:
            with Image.open(output / "browse.png") as shot:
                for x, album in [(65, 3), (37, 2), (255, 5), (283, 6)]:
                    actual = shot.convert("RGB").getpixel((x, 100))
                    assert all(abs(a - b) <= 8 for a, b in
                               zip(actual, colors[album])), (
                        "missing/wrong side cover", x, album, actual)
        capture_motion("open-frames", "select")
        capture("tracks")
        if args.long_titles:
            time.sleep(2)
            capture("tracks-scrolled")
            with Image.open(output / "tracks.png") as first, \
                 Image.open(output / "tracks-scrolled.png") as second:
                assert ImageChops.difference(first.crop((40, 79, 231, 97)),
                    second.crop((40, 79, 231, 97))).getbbox(), "title did not scroll"
                assert not ImageChops.difference(first.crop((250, 79, 284, 97)),
                    second.crop((250, 79, 284, 97))).getbbox(), "duration scrolled"
            for _ in range(min(9, args.tracks - 1)):
                tap("next", .12)
            capture("tracks-next-page")
        capture_motion("close-frames", "menu")
        tap("select", .5)
        tap("select", 2)
        capture("wps")
        trace = rbdir / "ipodjs-trace.tsv"
        assert "\twps\t" in trace.read_text(), "track did not reach WPS"
        tap("menu", 1)
        # WPS preserves its PictureFlow origin; Menu returns straight to
        # browsing, not Home. An extra Select here would reopen the album.
        capture("playing-browse")
        fd_counts = []
        for cycle in range(args.cycles):
            tap("select", .45)
            if cycle == 0:
                capture("playing-tracks")
            tap("menu", .45)
            fd_counts.append(len(list(Path(f"/proc/{process.pid}/fd").iterdir())))
            if cycle % 10 == 9:
                print(f"completed {cycle + 1} album open/close cycles", flush=True)
        capture("playing-return")
        if args.retail_reference:
            for _ in range(4):
                tap("prev", .65)
        # Check actual center-cover identity while traversing every fixture
        # album in both directions, with the original track still playing.
        for direction, albums in (("next", range(1, len(colors))),
                                  ("prev", range(len(colors) - 2, -1, -1))):
            for album in albums:
                tap(direction, .65)
                # Wheel repeats and host scheduling can leave a nearly
                # settled cover shifted a few pixels after the fixed delay.
                # Wait for its full resting bounds, not just its center.
                def cover_settled():
                    try:
                        with Image.open(frame) as shot:
                            shot = shot.convert("RGB")
                            return all(
                                all(abs(a - b) < 12 for a, b in
                                    zip(shot.getpixel(point), colors[album]))
                                for point in [(104, 45), (215, 45),
                                              (104, 160), (215, 160)])
                    except (OSError, ValueError):
                        return False
                wait_until(cover_settled, timeout=2)
                capture(f"{direction}-{album:02d}")
                with Image.open(output / f"{direction}-{album:02d}.png") as shot:
                    actual = shot.getpixel((104, 45))[:3]
                assert all(abs(a - b) < 12 for a, b in
                           zip(actual, colors[album])), (
                    f"wrong center artwork for album {album}: {actual}")
        tap("menu", 1)
        data = (rbdir / "pictureflow-trace.tsv").read_text()
        (output / "pictureflow-trace.tsv").write_text(data)
        rows = "\n".join(line for line in data.splitlines()
                         if not line.startswith("#"))
        samples = list(csv.DictReader(io.StringIO(rows), delimiter="\t"))
        assert any(int(s["position"]) == 1024 for s in samples), "no open endpoint"
        assert any(int(s["position"]) == 0 for s in samples), "no closed endpoint"
        assert all(int(s["audio"]) & 1 for s in samples), "playback stopped"
        assert all(int(s["shown"]) == 1 for s in samples), "scene was not ready"
        assert len({s["playlist"] for s in samples}) == 1, "playlist changed"
        elapsed = [int(s["elapsed"]) for s in samples]
        assert elapsed == sorted(elapsed), "playback position regressed"
        assert max(fd_counts) - min(fd_counts) <= 1, "descriptor count grew"
        assert any((demo / "pictureflow").glob("*.pft")), "no persistent metadata"
        assert "cache_hits=0 " not in data.splitlines()[0], "warm cache was unused"
        for previous, current in zip(samples, samples[1:]):
            if current["state"] == previous["state"] == "2":
                assert int(current["position"]) >= int(previous["position"])
            if current["state"] == previous["state"] == "4":
                assert int(current["position"]) <= int(previous["position"])
        durations = {"2": [], "4": []}
        phase = None
        for sample in samples:
            state, tick = sample["state"], int(sample["tick"])
            if state in durations and state != phase:
                phase, started = state, tick
            elif phase and state not in durations:
                # This simulator target uses HZ=100. Measures motion only,
                # not input-to-first-motion or cold/warm plugin launch.
                durations[phase].append((tick - started) * 10)
                phase = None
        timing = {}
        for state, values in durations.items():
            assert values, "no complete timed transition"
            values.sort()
            timing["open" if state == "2" else "close"] = {
                "samples": len(values), "median_ms": statistics.median(values),
                "p95_ms": values[int(.95 * (len(values) - 1))],
                "worst_ms": max(values)}
        report = {"cycles": args.cycles, "fd_min": min(fd_counts),
                  "fd_max": max(fd_counts), "trace_summary": data.splitlines()[0],
                  "elapsed_first": elapsed[0], "elapsed_last": elapsed[-1],
                  "motion_timing_simulator_only": timing}
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report), flush=True)
    finally:
        process.terminate()
        process.wait(timeout=10)
        log.close()


if __name__ == "__main__":
    main()
