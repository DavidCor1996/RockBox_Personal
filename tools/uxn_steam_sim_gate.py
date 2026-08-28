#!/usr/bin/env python3
"""Prove the bundled Uxn library appears and launches through iPodJS Steam."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from maker_lite_launcher_sim_gate import (
    frame_crc,
    last_sequence,
    pulse,
    trace_rows,
    wait_for_screen,
)


ROOT = Path(__file__).resolve().parents[1]
PLUGIN_PATH = "/.rockbox/rocks/viewers/uxn.rock"
GATE_NAMES = {
    "select": "select.gate",
    "menu": "menu.gate",
    "forward": "forward.gate",
    "back": "back.gate",
}


def prepare_root(build: Path, root: Path) -> None:
    source = build / "simdisk/.rockbox"
    rockbox = root / ".rockbox"
    rockbox.mkdir(parents=True)
    if not source.is_dir():
        raise SystemExit(f"missing simulator disk: {source}")
    for entry in source.iterdir():
        if entry.is_file():
            shutil.copy2(entry, rockbox / entry.name)
    for name in ("codecs", "langs", "icons", "fonts", "wps", "themes", "ipodjs"):
        candidate = source / name
        if candidate.exists():
            os.symlink(candidate, rockbox / name, target_is_directory=True)
    (rockbox / "config.cfg").write_text(
        "\n".join(
            (
                "ui engine: ipodjs",
                "ui engine games appearance: steam",
                "start in screen: root",
                "resume: off",
                "tagcache_autoupdate: off",
                "root menu order: applications",
                "",
            )
        ),
        encoding="utf-8",
    )

    plugin = root / PLUGIN_PATH.removeprefix("/")
    plugin.parent.mkdir(parents=True)
    shutil.copy2(build / "apps/plugins/uxn/uxn.rock", plugin)
    metadata = plugin.parent / "uxn"
    metadata.mkdir()
    for name in ("games.tsv", "SOURCES.tsv", "THIRDPARTY-NOTICES.txt"):
        shutil.copy2(ROOT / "assets/uxn_games" / name, metadata / name)
    shutil.copytree(
        ROOT / "assets/game_covers/uxn",
        rockbox / "games/library/covers/uxn",
    )
    shutil.copytree(ROOT / "assets/uxn_games/roms", root / "Uxn")


def run(build: Path, output: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="uxn-steam-", dir="/tmp") as name:
        root = Path(name)
        prepare_root(build, root)
        trace = root / ".rockbox/ipodjs-trace.tsv"
        frame = root / "launcher-frame.bmp"
        gates = output / ".gates"
        shutil.rmtree(gates, ignore_errors=True)
        gates.mkdir(parents=True)
        gate_paths = {
            key: gates / value for key, value in GATE_NAMES.items()
        }
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_AUDIODRIVER": "dummy",
                "SDL_VIDEODRIVER": "dummy",
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_HIDDEN": "1",
                "ROCKPOD_SIM_IPODJS_TRACE": "1",
                "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
                "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
                "ROCKPOD_SIM_SELECT_GATE": str(gate_paths["select"]),
                "ROCKPOD_SIM_MENU_GATE": str(gate_paths["menu"]),
                "ROCKPOD_SIM_SCROLL_FWD_GATE": str(gate_paths["forward"]),
                "ROCKPOD_SIM_SCROLL_BACK_GATE": str(gate_paths["back"]),
                "UXN_TEST_FRAMES": "3",
            }
        )
        log = output / "rockboxui.log"
        with open(log, "wb") as captured:
            process = subprocess.Popen(
                [
                    str(build / "rockboxui"),
                    "--zoom",
                    "1",
                    "--nobackground",
                    "--root",
                    str(root),
                ],
                cwd=build,
                env=environment,
                stdout=captured,
                stderr=subprocess.STDOUT,
            )
            try:
                wait_for_screen(process, trace, "Home")
                sequence = last_sequence(trace)
                pulse(gate_paths["select"])
                wait_for_screen(process, trace, "Extras", sequence)
                pulse(gate_paths["forward"])
                sequence = last_sequence(trace)
                pulse(gate_paths["select"])
                console_sequence = wait_for_screen(
                    process, trace, "Steam Consoles", sequence
                )
                console_row = next(
                    row for row in trace_rows(trace)
                    if row[0] == str(console_sequence)
                )
                if len(console_row) <= 11 or console_row[11] != "2":
                    raise SystemExit("Steam console list does not expose Uxn")
                if console_row[9] != "0":
                    raise SystemExit("Steam console list did not select All Games")
                shutil.copy2(frame, output / "console-frame.bmp")
                sequence = last_sequence(trace)
                pulse(gate_paths["forward"])
                sequence = last_sequence(trace)
                pulse(gate_paths["select"])
                library_sequence = wait_for_screen(
                    process, trace, "Steam Games", sequence
                )
                library_row = next(
                    row for row in trace_rows(trace)
                    if row[0] == str(library_sequence)
                )
                if len(library_row) <= 11 or library_row[11] != "3":
                    raise SystemExit("Uxn console does not contain three games")
                landing_crc = frame_crc(frame)

                # Donsol, Niju, Worm are sorted alphabetically. Select Worm so
                # the launch-return portion stays quick while still proving
                # the full three-game filtered carousel is navigable.
                pulse(gate_paths["forward"])
                pulse(gate_paths["forward"])
                sequence = last_sequence(trace)
                pulse(gate_paths["select"])
                wait_for_screen(process, trace, "Steam Game Details", sequence)
                details_crc = frame_crc(frame)
                if landing_crc == details_crc:
                    raise SystemExit("Steam Uxn landing and details frames match")
                shutil.copy2(frame, output / "details-frame.bmp")
                sequence = last_sequence(trace)
                pulse(gate_paths["select"])
                # Game launchers return GO_TO_ROOT after their plugin exits;
                # the enclosing Extras screen is the established destination.
                wait_for_screen(process, trace, "Extras", sequence)
                shutil.copy2(trace, output / "ipodjs-trace.tsv")
                shutil.copy2(frame, output / "launcher-frame.bmp")
            finally:
                if trace.is_file():
                    shutil.copy2(trace, output / "last-ipodjs-trace.tsv")
                if frame.is_file():
                    shutil.copy2(frame, output / "last-launcher-frame.bmp")
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
                shutil.rmtree(gates, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir", type=Path, default=Path("build-sim-ipod6g")
    )
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/uxn-steam-gate")
    )
    args = parser.parse_args()
    build = (ROOT / args.build_dir).resolve()
    if not (build / "rockboxui").is_file():
        raise SystemExit("build the iPod 6G simulator first")
    args.output.mkdir(parents=True, exist_ok=True)
    run(build, args.output.resolve())
    print(f"Uxn Steam simulator gate: PASS ({args.output.resolve()})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
