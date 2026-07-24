#!/usr/bin/env python3
"""Prove a live emulator frame can earn an offline RA achievement."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

from snes_lite_sim_gate import prepare


GAME_KEY = "runtime-gate"
ACHIEVEMENT_ID = "77"
ROM_NAME = "rockachievements-runtime-gate.sfc"
ROM_DEVICE_PATH = f"/.rockbox/roms/snes/{ROM_NAME}"


def make_test_rom(path: Path) -> None:
    rom = bytearray([0xFF]) * 0x8000
    # 65C816: enter native mode, clear the first word of SNES work RAM, wait
    # several emulated frames, then set it. This proves the runtime observes a
    # false-to-true condition from the emulator rather than an initial value.
    code = bytes((
        0x78,              # SEI
        0x18,              # CLC
        0xFB,              # XCE
        0xC2, 0x30,        # REP #$30
        0xA9, 0x00, 0x00,  # LDA #$0000
        0x5B,              # TCD
        0x85, 0x00,        # STA $00
        0xA2, 0x00, 0x00,  # LDX #$0000
        0xE8,              # INX
        0xD0, 0xFD,        # BNE to INX
        0xA9, 0x01, 0x00,  # LDA #$0001
        0x85, 0x00,        # STA $00
        0x80, 0xFE,        # BRA to self
    ))
    rom[: len(code)] = code
    title = b"ROCKACHIEVEMENTS GATE"
    rom[0x7FC0:0x7FC0 + 21] = title.ljust(21, b" ")
    rom[0x7FD5:0x7FDC] = bytes((0x20, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00))
    rom[0x7FDC:0x7FE0] = bytes((0xCB, 0xED, 0x34, 0x12))
    for vector in (0x7FEA, 0x7FEE, 0x7FFA, 0x7FFC, 0x7FFE):
        rom[vector:vector + 2] = bytes((0x00, 0x80))
    path.write_bytes(rom)


def stage_catalog(simdisk: Path) -> None:
    root = simdisk / ".rockbox" / "achievements"
    generation = root / "generations" / "runtime-gate"
    game = generation / "games" / GAME_KEY
    state = root / "state"
    runtime = state / "runtime"
    game.mkdir(parents=True, exist_ok=True)
    runtime.mkdir(parents=True, exist_ok=True)
    (root / "current").write_text("runtime-gate\n", encoding="utf-8")
    (generation / "catalog.tsv").write_text(
        "game_key\ttitle\tconsole\tset_kind\tcover_path\tachievement_file\t"
        "unlocked\ttotal\tearned_points\ttotal_points\tlast_played\t"
        "launch_target\tra_game_id\tra_hash\n"
        f"{GAME_KEY}\tRuntime Gate\tSuper Nintendo\tretroachievements\t\t"
        f"/.rockbox/achievements/generations/runtime-gate/games/{GAME_KEY}/"
        "achievements.tsv\t0\t1\t0\t5\t\t"
        f"{ROM_DEVICE_PATH}\t1\t{'0' * 32}\n",
        encoding="utf-8",
    )
    (game / "achievements.tsv").write_text(
        "id\ttitle\tdescription\tpoints\tbadge_unlocked\tbadge_locked\t"
        "state\tmeasured\tunlock_time\tsource\tmemaddr\n"
        f"{ACHIEVEMENT_ID}\tRuntime Gate\tTrigger from the live SNES frame "
        "loop.\t5\t\t\tlocked\t0\t\tretroachievements\t0xH0000=1\n",
        encoding="utf-8",
    )
    (state / "unlocks.v1.tsv").write_text(
        "game_key\tachievement_id\tunlock_time\tsource\n", encoding="utf-8")
    (state / "events.v1.tsv").write_text(
        "sequence\tgame_key\tachievement_id\tevent_time\n", encoding="utf-8")
    (state / "sessions.v1.tsv").write_text(
        "launch_target\tsessions\tseconds\tlast_played\n", encoding="utf-8")
    shutil.rmtree(runtime)
    runtime.mkdir()


def run_gate(build_dir: Path, frames: int) -> None:
    with tempfile.TemporaryDirectory(prefix="rockachievements-gate-") as temp:
        rom = Path(temp) / ROM_NAME
        make_test_rom(rom)
        simdisk = prepare(build_dir, rom)
        stage_catalog(simdisk)
        environment = os.environ.copy()
        environment.update({
            "SNES_LITE_TEST_FRAMES": str(max(2, frames)),
            "SDL_VIDEODRIVER": "dummy",
            "SDL_AUDIODRIVER": "dummy",
        })
        log_path = simdisk / ".rockbox" / "logs" / "snes_lite.log"
        log_path.unlink(missing_ok=True)
        process = subprocess.Popen(
            [str(build_dir / "rockboxui")], cwd=build_dir, env=environment)
        deadline = time.monotonic() + 90
        while process.poll() is None and time.monotonic() < deadline:
            if log_path.is_file() and "exit status=" in log_path.read_text(
                    encoding="utf-8", errors="replace"):
                process.terminate()
                break
            time.sleep(0.1)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()

        log = log_path.read_text(encoding="utf-8", errors="replace")
        unlocks = (simdisk / ".rockbox/achievements/state/unlocks.v1.tsv").read_text(
            encoding="utf-8")
        events = (simdisk / ".rockbox/achievements/state/events.v1.tsv").read_text(
            encoding="utf-8")
        sessions = (
            simdisk / ".rockbox/achievements/state/sessions.v1.tsv"
        ).read_text(encoding="utf-8")
        if "achievements active=1" not in log or "exit status=0" not in log:
            raise SystemExit("SNES Lite did not activate and finish the test set")
        if f"{GAME_KEY}\t{ACHIEVEMENT_ID}\t" not in unlocks:
            raise SystemExit("live emulator frame did not persist the unlock")
        if f"\t{GAME_KEY}\t{ACHIEVEMENT_ID}\t" not in events:
            raise SystemExit("live emulator frame did not append an unlock event")
        if f"{ROM_DEVICE_PATH}\t1\t" not in sessions:
            raise SystemExit("shared launcher telemetry did not record the session")
        print("RockAchievements emulator integration gate: PASS")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument("--frames", type=int, default=40)
    args = parser.parse_args()
    run_gate(args.build_dir.resolve(), args.frames)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
