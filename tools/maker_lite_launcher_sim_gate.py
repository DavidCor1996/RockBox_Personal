#!/usr/bin/env python3
"""Qualify Maker Lite through both iPodJS Steam and Classic Games paths."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import tempfile
import time
import zlib
from pathlib import Path

from PIL import Image


REPO = Path(__file__).resolve().parents[1]
GATE_NAMES = {
    "select": "select.gate",
    "menu": "menu.gate",
    "forward": "forward.gate",
    "back": "back.gate",
}


def prepare_root(build_dir: Path, root: Path, appearance: str) -> None:
    source = build_dir / "simdisk/.rockbox"
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
            [
                "ui engine: ipodjs",
                f"ui engine games appearance: {appearance}",
                "start in screen: root",
                "resume: off",
                "tagcache_autoupdate: off",
                "root menu is customized: off",
                "",
            ]
        ),
        encoding="utf-8",
    )
    subprocess.run(
        [
            str(REPO / "rockpod/.venv/bin/python"),
            str(REPO / "tools/maker_lite_stage_sim_fixtures.py"),
            str(root),
            "--plugin",
            str(build_dir / "apps/plugins/maker_lite/maker_lite.rock"),
        ],
        cwd=REPO,
        check=True,
    )
    if appearance == "classic":
        # Prove the native browser uses the complete all-project index rather
        # than accidentally succeeding through the cover-gated Steam catalog.
        (rockbox / "rocks/games/maker_lite/games.tsv").write_text(
            "", encoding="utf-8"
        )
        browser = rockbox / "games/maker_lite/projects.tsv"
        rows = []
        for line in browser.read_text(encoding="utf-8").splitlines():
            fields = line.split("\t")
            if len(fields) == 11:
                fields[2] = ""
                rows.append("\t".join(fields))
        browser.write_text("\n".join(rows) + "\n", encoding="utf-8")
        for cover in (rockbox / "games/maker_lite/projects").glob(
            "*/cover.144x108x24.bmp"
        ):
            cover.unlink()


def trace_rows(path: Path) -> list[list[str]]:
    if not path.is_file():
        return []
    return [
        line.split("\t")
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines()
        if line
    ]


def wait_for_screen(
    process: subprocess.Popen, trace: Path, name: str, after: int = 0
) -> int:
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        rows = trace_rows(trace)
        for row in rows:
            try:
                sequence = int(row[0])
            except (IndexError, ValueError):
                continue
            if sequence > after and len(row) > 3 and row[2] == "screen" and row[3] == name:
                return sequence
        if process.poll() is not None:
            raise SystemExit(
                f"simulator exited before reaching {name!r} (rc={process.returncode})"
            )
        time.sleep(0.05)
    raise SystemExit(f"simulator did not reach {name!r}")


def last_sequence(trace: Path) -> int:
    rows = trace_rows(trace)
    for row in reversed(rows):
        try:
            return int(row[0])
        except (IndexError, ValueError):
            continue
    return 0


def pulse(gate: Path, duration: float = 0.14, settle: float = 0.30) -> None:
    gate.touch()
    time.sleep(duration)
    gate.unlink(missing_ok=True)
    time.sleep(settle)


def hold(gate: Path, duration: float = 0.65) -> None:
    pulse(gate, duration=duration, settle=0.35)


def wait_for_session(process: subprocess.Popen, log: Path, previous: int) -> str:
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if log.is_file():
            lines = log.read_text(
                encoding="utf-8", errors="replace"
            ).splitlines()
            if len(lines) > previous:
                return lines[-1]
        if process.poll() is not None:
            raise SystemExit("simulator exited before Maker Lite logged a session")
        time.sleep(0.05)
    raise SystemExit("Maker Lite did not return from its launcher path")


def frame_crc(path: Path) -> str:
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if path.is_file() and path.stat().st_size > 1000:
            try:
                with Image.open(path) as opened:
                    image = opened.convert("RGB")
                    if image.size == (320, 240):
                        return f"{zlib.crc32(image.tobytes()) & 0xFFFFFFFF:08x}"
            except OSError:
                pass
        time.sleep(0.05)
    raise SystemExit("simulator did not publish a valid 320x240 launcher frame")


def run_mode(build_dir: Path, output: Path, appearance: str) -> str:
    with tempfile.TemporaryDirectory(
        prefix=f"maker-lite-{appearance}-", dir="/tmp"
    ) as temporary:
        root = Path(temporary)
        prepare_root(build_dir, root, appearance)
        trace = root / ".rockbox/ipodjs-trace.tsv"
        log = root / ".rockbox/logs/maker_lite.log"
        frame = root / "launcher-frame.bmp"
        gates = output / f".{appearance}-gates"
        shutil.rmtree(gates, ignore_errors=True)
        gates.mkdir(parents=True)
        gate_paths = {
            name: gates / filename for name, filename in GATE_NAMES.items()
        }
        hold_gate = gates / "hold.gate"
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
                "ROCKPOD_SIM_PLAY_ACTION_GATE": str(gates / "play.gate"),
                "ROCKPOD_SIM_SCROLL_FWD_GATE": str(gate_paths["forward"]),
                "ROCKPOD_SIM_SCROLL_BACK_GATE": str(gate_paths["back"]),
                "ROCKPOD_SIM_HOLD_GATE": str(hold_gate),
                "MAKER_LITE_TEST_TICKS": "180",
            }
        )
        process_log = output / f"{appearance}-rockboxui.log"
        with open(process_log, "wb") as captured:
            process = subprocess.Popen(
                [
                    str(build_dir / "rockboxui"),
                    "--zoom",
                    "1",
                    "--nobackground",
                    "--root",
                    str(root),
                ],
                cwd=build_dir,
                env=environment,
                stdout=captured,
                stderr=subprocess.STDOUT,
            )
            try:
                wait_for_screen(process, trace, "Home")
                for _ in range(4):
                    pulse(gate_paths["forward"])
                sequence = last_sequence(trace)
                pulse(gate_paths["select"])
                wait_for_screen(process, trace, "Extras", sequence)
                pulse(gate_paths["forward"])
                sequence = last_sequence(trace)
                pulse(gate_paths["select"])

                if appearance == "steam":
                    wait_for_screen(process, trace, "Steam Games", sequence)
                    landing_crc = frame_crc(frame)
                    sequence = last_sequence(trace)
                    hold(gate_paths["select"])
                    wait_for_screen(process, trace, "Steam Consoles", sequence)
                    pulse(gate_paths["forward"])
                    sequence = last_sequence(trace)
                    pulse(gate_paths["select"])
                    wait_for_screen(process, trace, "Steam Games", sequence)
                    sequence = last_sequence(trace)
                    pulse(gate_paths["select"])
                    wait_for_screen(
                        process, trace, "Steam Game Details", sequence
                    )
                    details_crc = frame_crc(frame)
                    if landing_crc == details_crc:
                        raise SystemExit("Steam cover landing and details frames match")
                    previous_sessions = 0
                    pulse(gate_paths["select"], settle=0.35)
                    pulse(hold_gate, duration=0.55, settle=0.25)
                    pulse(gate_paths["menu"])
                    pulse(gates / "play.gate")
                    pulse(gate_paths["menu"])
                    pulse(gate_paths["select"])
                else:
                    wait_for_screen(process, trace, "Classic Games", sequence)
                    previous_sessions = 0
                    pulse(gate_paths["select"], settle=0.75)
                    pulse(gate_paths["select"], settle=0.35)
                    pulse(hold_gate, duration=0.55, settle=0.25)
                    pulse(gate_paths["menu"])
                    pulse(gates / "play.gate")
                    pulse(gate_paths["menu"])
                    pulse(gate_paths["select"])

                line = wait_for_session(process, log, previous_sessions)
                required = (
                    "project=mario-test-course",
                    "ruleset=1",
                    "ticks=180",
                    "playback_start=0",
                    "playback_end=0",
                    "hold_pauses=1",
                    "menu_pauses=1",
                    "controls_screens=1",
                )
                if any(field not in line for field in required):
                    raise SystemExit(
                        f"{appearance}: invalid native session record: {line}"
                    )
                shutil.copy2(trace, output / f"{appearance}-ipodjs-trace.tsv")
                shutil.copy2(log, output / f"{appearance}-maker_lite.log")
                shutil.copy2(frame, output / f"{appearance}-launcher-frame.bmp")
                return line
            finally:
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
        "--output", type=Path, default=Path("/tmp/maker-lite-launcher-gate")
    )
    args = parser.parse_args()
    build_dir = (REPO / args.build_dir).resolve()
    if not (build_dir / "rockboxui").is_file():
        raise SystemExit("build the iPod 6G simulator first")
    args.output.mkdir(parents=True, exist_ok=True)
    for appearance in ("steam", "classic"):
        result = run_mode(build_dir, args.output.resolve(), appearance)
        print(f"PASS {appearance}: {result}")
    print(f"Launcher evidence: {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
