#!/usr/bin/env python3
"""Prepare and validate simulator runs for Game Boy accuracy test ROMs."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rockboy_profile_gate import ROCKBOY_PLUGIN_PATH, write_rockboy_direct_start


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def copy_rom(active_simdisk: Path, rom_path: Path) -> Path:
    target_dir = active_simdisk / "gameboy" / "accuracy"
    target_dir.mkdir(parents=True, exist_ok=True)
    target = target_dir / rom_path.name
    shutil.copy2(rom_path, target)
    return target


def force_dmg_header(rom_path: Path) -> None:
    data = bytearray(rom_path.read_bytes())
    if len(data) <= 0x143:
        raise SystemExit(f"ROM is too small to patch CGB flag: {rom_path}")
    data[0x143] = 0x00
    rom_path.write_bytes(data)


def gate_work_root(root: Path) -> Path:
    path = root / "tmp" / "rockboy-accuracy-gates"
    path.mkdir(parents=True, exist_ok=True)
    return path


def mirror_hosted_config(active_simdisk: Path) -> None:
    hosted = active_simdisk / ".config" / "rockbox.org"
    hosted_rocks = hosted / "rocks"
    hosted_viewers = hosted_rocks / "viewers"
    hosted_rocks.mkdir(parents=True, exist_ok=True)
    hosted_viewers.mkdir(parents=True, exist_ok=True)
    (hosted / "rockboy").mkdir(parents=True, exist_ok=True)

    config = active_simdisk / ".rockbox" / "config.cfg"
    plugin_dat = active_simdisk / ".rockbox" / "rocks" / "plugin.dat"
    rockboy_plugin = active_simdisk / ROCKBOY_PLUGIN_PATH.lstrip("/")
    if config.is_file():
        shutil.copy2(config, hosted / "config.cfg")
    if plugin_dat.is_file():
        shutil.copy2(plugin_dat, hosted_rocks / "plugin.dat")
    if rockboy_plugin.is_file():
        shutil.copy2(rockboy_plugin, hosted_viewers / "rockboy.rock")


def minimal_simdisk_ignore(directory: str, names: list[str]) -> set[str]:
    path = Path(directory)
    ignored: set[str] = set()

    if path.name == "simdisk":
        ignored.update(
            name for name in names
            if name not in {".rockbox", "config.cfg"}
            or (name.startswith("dump") and name.endswith(".bmp"))
        )

    if path.name == ".rockbox":
        ignored.update(
            name for name in names
            if name in {"database_changelog.txt", "rockbox.log", "nvram.bin"}
            or name.startswith("database_")
        )
        ignored.update(name for name in names if name == "rockboy")

    if path.name == "rocks":
        ignored.update(name for name in names if name not in {"viewers", "plugin.dat", "rb_plugins.dat"})

    if path.name == "viewers":
        ignored.update(name for name in names if name != "rockboy.rock")

    return ignored


def prepare(args: argparse.Namespace) -> Path:
    root = repo_root()
    build_dir = (root / args.build_dir).resolve()
    source_simdisk = build_dir / "simdisk"
    binary = build_dir / "rockboxui"
    staged_plugin = build_dir / "apps" / "plugins" / "rockboy" / "rockboy.rock"
    source_plugin = source_simdisk / ROCKBOY_PLUGIN_PATH.lstrip("/")
    rom_path = Path(args.rom).expanduser().resolve()

    if not binary.is_file():
        raise SystemExit(f"missing simulator binary: {binary}")
    if not source_simdisk.is_dir():
        raise SystemExit(f"missing simulator simdisk: {source_simdisk}")
    if not source_plugin.is_file():
        raise SystemExit(f"missing Rockboy plugin in simdisk: {source_plugin}")
    if not rom_path.is_file():
        raise SystemExit(f"missing accuracy ROM: {rom_path}")

    gate_root = Path(tempfile.mkdtemp(
        prefix="rockboy-accuracy-gate-",
        dir=gate_work_root(root),
    ))
    active_simdisk = gate_root / "simdisk"
    shutil.copytree(
        source_simdisk,
        active_simdisk,
        symlinks=True,
        ignore=minimal_simdisk_ignore,
    )

    if staged_plugin.is_file():
        shutil.copy2(staged_plugin, active_simdisk / ROCKBOY_PLUGIN_PATH.lstrip("/"))

    serial_logs = serial_log_paths(active_simdisk)
    for serial_log in serial_logs:
        serial_log.parent.mkdir(parents=True, exist_ok=True)
        serial_log.unlink(missing_ok=True)
    direct_rom = copy_rom(active_simdisk, rom_path)
    if args.force_dmg:
        force_dmg_header(direct_rom)
    write_rockboy_direct_start(active_simdisk, direct_rom, build_dir=build_dir)
    mirror_hosted_config(active_simdisk)

    command = [str(binary), "--nobackground", "--root", str(active_simdisk), "--zoom", "1"]
    print(f"Prepared isolated Rockboy accuracy simdisk: {active_simdisk}")
    print(f"Direct-start ROM: {Path('/') / direct_rom.relative_to(active_simdisk)}")
    if args.force_dmg:
        print("Forced DMG mode: patched staged ROM CGB flag to 0x00")
    print(f"Serial log: {serial_logs[0]}")
    print(f"Fallback serial log: {serial_logs[1]}")
    print("Run simulator:")
    print(
        f"RBROOT={active_simdisk} ROCKBOY_SERIAL_LOG=1 "
        "ROCKBOY_ACCURACY_LOG=1 ROCKBOY_ACCURACY_FAST=1 "
        + " ".join(command)
    )
    print("Validate after the ROM reaches pass/fail output:")
    print(f"{Path(__file__).relative_to(root)} --validate {active_simdisk} --expect {args.expect!r}")
    return active_simdisk


def serial_log_paths(simdisk: Path) -> tuple[Path, Path]:
    return (
        simdisk / ".config" / "rockbox.org" / "rockboy" / "serial.log",
        simdisk / ".rockbox" / "rockboy" / "serial.log",
    )


def read_serial_text(simdisk: Path) -> str:
    serial_logs = serial_log_paths(simdisk)
    for serial_log in serial_logs:
        if serial_log.exists():
            return serial_log.read_bytes().decode("ascii", errors="replace")
    raise SystemExit(
        "missing Rockboy serial log; checked "
        + ", ".join(str(path) for path in serial_logs)
    )


def try_read_serial_text(simdisk: Path) -> str:
    try:
        return read_serial_text(simdisk)
    except SystemExit:
        return ""


def validate(args: argparse.Namespace) -> None:
    simdisk = Path(args.validate).resolve()
    text = read_serial_text(simdisk)
    if args.expect not in text:
        preview = text[-600:].replace("\r", "\\r").replace("\n", "\\n")
        raise SystemExit(
            f"expected {args.expect!r} in Rockboy serial log; "
            f"last output was {preview!r}"
        )
    print(f"validated Rockboy serial output in {simdisk}: found {args.expect!r}")


def terminate_simulator(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return

    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def run(args: argparse.Namespace) -> None:
    root = repo_root()
    build_dir = (root / args.build_dir).resolve()
    binary = build_dir / "rockboxui"
    simdisk = prepare(args)
    staged_rom = simdisk / "gameboy" / "accuracy" / Path(args.rom).name
    rom_param = str(Path("/") / staged_rom.relative_to(simdisk))
    command = [
        str(binary),
        "--nobackground",
        "--root",
        str(simdisk),
        "--zoom",
        "1",
    ]
    env = os.environ.copy()
    env.update(
        {
            "RBROOT": str(simdisk),
            "ROCKBOY_SERIAL_LOG": "1",
            "ROCKBOY_ACCURACY_LOG": "1",
            "ROCKBOY_ACCURACY_FAST": "1",
            "ROCKBOX_SIM_PLUGIN": ROCKBOY_PLUGIN_PATH,
            "ROCKBOX_SIM_PLUGIN_PARAM": rom_param,
        }
    )

    deadline = time.monotonic() + args.timeout
    print(f"Running simulator for up to {args.timeout:.1f}s")
    process = subprocess.Popen(command, cwd=root, env=env, text=True)
    try:
        while time.monotonic() < deadline:
            if args.expect in try_read_serial_text(simdisk):
                print(f"Observed {args.expect!r}; stopping simulator")
                break
            if process.poll() is not None:
                break
            time.sleep(args.poll_interval)
    finally:
        terminate_simulator(process)

    args.validate = str(simdisk)
    validate(args)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build-sim-video-5g")
    parser.add_argument("--rom")
    parser.add_argument("--expect", default="Passed")
    parser.add_argument("--force-dmg", action="store_true")
    parser.add_argument("--prepare", action="store_true")
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--timeout", type=float, default=45.0)
    parser.add_argument("--poll-interval", type=float, default=0.25)
    parser.add_argument("--validate")
    args = parser.parse_args(argv)

    if args.validate:
        validate(args)
    elif args.run:
        if not args.rom:
            parser.error("--rom is required with --run")
        run(args)
    else:
        if not args.prepare:
            parser.error("one of --prepare, --run, or --validate is required")
        if not args.rom:
            parser.error("--rom is required with --prepare")
        prepare(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
