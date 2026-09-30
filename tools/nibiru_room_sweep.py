#!/usr/bin/env python3
"""Smoke-test every owned NiBiRu room through the Rockbox AGDS runtime.

The game directory is mounted into a temporary simulator disk by symlink.  No
owned game file is copied, rewritten, or included in the report.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

try:
    from nibiru_agds_trace import adb_entries, parse_object
    from nibiru_sim_gate import ROOT, stage
except ModuleNotFoundError:
    from tools.nibiru_agds_trace import adb_entries, parse_object
    from tools.nibiru_sim_gate import ROOT, stage


ROOM_OPCODES = frozenset((72, 75, 76))
FAILURE_MARKERS = (
    " opcode ",
    " pending at ",
    "agds: scripted room ",
    " draw failed: ",
    "agds: room VM tick failed:",
    "agds: direct scene tick failed:",
    "agds: retail inventory draw failed:",
)


def owned_room_names(data_adb: Path) -> list[str]:
    """Derive screen roots from the retail bytecode, not a handwritten list."""
    rooms = []
    for name, payload in adb_entries(data_adb):
        if "." in name or name == "main":
            continue
        parsed = parse_object(payload)
        if parsed is None:
            continue
        if ROOM_OPCODES.intersection(parsed["opcodes"]):
            rooms.append(name)
    return sorted(set(rooms))


def copy_capture(source: Path, destination: Path, deadline: float) -> str:
    while time.monotonic() < deadline:
        try:
            payload = source.read_bytes()
        except FileNotFoundError:
            time.sleep(0.025)
            continue
        if payload[:2] == b"BM" and len(payload) > 54:
            destination.write_bytes(payload)
            return hashlib.sha256(payload).hexdigest()
        time.sleep(0.025)
    raise RuntimeError("simulator did not publish a valid BMP frame")


def screen_failure(log_text: str, screen: str) -> str | None:
    scripted_failures = (
        f"agds: scripted room {screen} failed:",
        f"agds: scripted room {screen} draw failed:",
    )
    if any(marker in log_text for marker in scripted_failures):
        line = next(
            line for line in log_text.splitlines()
            if any(marker in line for marker in scripted_failures)
        )
        return line.strip()
    if " pending at " in log_text:
        line = next(
            line for line in log_text.splitlines()
            if " pending at " in line
        )
        return line.strip()
    for marker in FAILURE_MARKERS[3:]:
        if marker in log_text:
            line = next(
                line for line in log_text.splitlines() if marker in line
            )
            return line.strip()
    return None


def run_screen(
    build: Path,
    sim_root: Path,
    output: Path,
    screen: str,
    timeout: float,
    settle: float,
) -> dict[str, object]:
    capture = output / ".live.bmp"
    log_path = output / "logs" / f"{screen}.log"
    frame_path = output / "frames" / f"{screen}.bmp"
    try:
        capture.unlink()
    except FileNotFoundError:
        pass

    environment = os.environ.copy()
    environment.update(
        {
            "SDL_VIDEODRIVER": "dummy",
            "SDL_AUDIODRIVER": "dummy",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_HIDDEN": "1",
            "ROCKPOD_SIM_PREVIEW_BMP": str(capture),
            "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "40",
            "ROCKPOD_AGDS_TEST_SPEED": "8",
            "ROCKPOD_AGDS_AUTOSTART_SCREEN": screen,
            "ROCKBOX_SIM_PLUGIN": "/.rockbox/rocks/apps/scummvm.rock",
            "ROCKBOX_SIM_PLUGIN_PARAM": "/ScummVM/nibiru.scummvm",
            "ROCKPOD_SCUMMVM_CURSOR_X": "40",
            "ROCKPOD_SCUMMVM_CURSOR_Y": "92",
        }
    )
    started = time.monotonic()
    with log_path.open("wb") as log:
        process = subprocess.Popen(
            [
                str(build / "rockboxui"),
                "--nobackground",
                "--root",
                str(sim_root),
            ],
            cwd=build,
            env=environment,
            stdout=log,
            stderr=subprocess.STDOUT,
        )

    marker = f"agds: simulator autostart screen {screen} ready"
    deadline = started + timeout
    ready_at: float | None = None
    failure: str | None = None
    try:
        while time.monotonic() < deadline:
            text = log_path.read_text(encoding="utf-8", errors="replace")
            failure = screen_failure(text, screen)
            if failure is not None:
                break
            if marker in text:
                ready_at = time.monotonic()
                break
            if process.poll() is not None:
                failure = f"simulator exited before room-ready marker ({process.returncode})"
                break
            time.sleep(0.025)

        if failure is None and ready_at is None:
            failure = f"room-ready marker timed out after {timeout:.1f}s"

        if failure is None:
            settle_deadline = time.monotonic() + settle
            while time.monotonic() < settle_deadline:
                if process.poll() is not None:
                    failure = (
                        "simulator exited while room was settling "
                        f"({process.returncode})"
                    )
                    break
                text = log_path.read_text(encoding="utf-8", errors="replace")
                failure = screen_failure(text, screen)
                if failure is not None:
                    break
                time.sleep(0.025)

        frame_sha256 = None
        if failure is None:
            try:
                frame_sha256 = copy_capture(capture, frame_path, deadline)
            except RuntimeError as exc:
                failure = str(exc)
        elapsed = time.monotonic() - started
    finally:
        if process.poll() is None:
            # Each room runs in a disposable simulator process.  Rockbox's
            # graceful SIGTERM path waits for a UI loop that is irrelevant to
            # this read-only gate, adding two seconds to every owned room.
            process.kill()
            process.wait(timeout=2)

    return {
        "screen": screen,
        "passed": failure is None,
        "seconds": round(elapsed, 3),
        "failure": failure,
        "log": str(log_path),
        "frame": str(frame_path) if frame_sha256 else None,
        "frame_sha256": frame_sha256,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build", type=Path, default=ROOT / "build-sim-ipod6g"
    )
    parser.add_argument("--game-dir", type=Path, required=True)
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/nibiru-room-sweep")
    )
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("--settle", type=float, default=0.35)
    parser.add_argument(
        "--screen", action="append", default=[],
        help="test only this screen ID; repeat for more than one",
    )
    parser.add_argument(
        "--limit", type=int,
        help="test only the first N derived screens (for gate development)",
    )
    args = parser.parse_args()

    build = args.build.resolve()
    game_dir = args.game_dir.resolve()
    output = args.output.resolve()
    if not (game_dir / "data.adb").is_file():
        parser.error(f"not an installed NiBiRu folder: {game_dir}")
    derived = owned_room_names(game_dir / "data.adb")
    rooms = args.screen or derived
    unknown = sorted(set(rooms) - set(derived))
    if unknown:
        parser.error("not an owned screen root: " + ", ".join(unknown))
    if args.limit is not None:
        if args.limit < 1:
            parser.error("--limit must be positive")
        rooms = rooms[: args.limit]

    output.mkdir(parents=True, exist_ok=True)
    (output / "logs").mkdir(exist_ok=True)
    (output / "frames").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="nibiru-room-sweep-") as temporary:
        sim_root = Path(temporary) / "simdisk"
        stage(build, sim_root, "bmp", game_dir, False, False, False, False)
        results = []
        for index, screen in enumerate(rooms, 1):
            result = run_screen(
                build, sim_root, output, screen, args.timeout, args.settle
            )
            results.append(result)
            state = "PASS" if result["passed"] else "FAIL"
            detail = "" if result["passed"] else f" — {result['failure']}"
            print(f"[{index:03d}/{len(rooms):03d}] {screen} {state}{detail}", flush=True)

    passed = sum(bool(item["passed"]) for item in results)
    report = {
        "format": 1,
        "engine": "AGDS 2.509",
        "source": str((game_dir / "data.adb").resolve()),
        "derived_room_count": len(derived),
        "tested_room_count": len(results),
        "passed_room_count": passed,
        "failed_room_count": len(results) - passed,
        "copied_game_data": False,
        "results": results,
    }
    report_path = output / "room-sweep.json"
    report_path.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(
        f"NiBiRu room sweep: {passed}/{len(results)} passed; {report_path}",
        flush=True,
    )
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
