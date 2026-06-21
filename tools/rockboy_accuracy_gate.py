#!/usr/bin/env python3
"""Prepare and validate simulator runs for Game Boy accuracy test ROMs."""

from __future__ import annotations

import argparse
import shutil
import sys
import tempfile
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


def prepare(args: argparse.Namespace) -> None:
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

    gate_root = Path(tempfile.mkdtemp(prefix="rockboy-accuracy-gate-"))
    active_simdisk = gate_root / "simdisk"
    shutil.copytree(source_simdisk, active_simdisk, symlinks=True)

    if staged_plugin.is_file():
        shutil.copy2(staged_plugin, active_simdisk / ROCKBOY_PLUGIN_PATH.lstrip("/"))

    serial_log = active_simdisk / ".rockbox" / "rockboy" / "serial.log"
    serial_log.unlink(missing_ok=True)
    direct_rom = copy_rom(active_simdisk, rom_path)
    write_rockboy_direct_start(active_simdisk, direct_rom)

    command = [str(binary), "--nobackground", "--root", str(active_simdisk), "--zoom", "1"]
    print(f"Prepared isolated Rockboy accuracy simdisk: {active_simdisk}")
    print(f"Direct-start ROM: {Path('/') / direct_rom.relative_to(active_simdisk)}")
    print(f"Serial log: {serial_log}")
    print("Run simulator:")
    print("ROCKBOY_SERIAL_LOG=1 " + " ".join(command))
    print("Validate after the ROM reaches pass/fail output:")
    print(f"{Path(__file__).relative_to(root)} --validate {active_simdisk} --expect {args.expect!r}")


def read_serial_text(simdisk: Path) -> str:
    serial_log = simdisk / ".rockbox" / "rockboy" / "serial.log"
    if not serial_log.exists():
        raise SystemExit(f"missing Rockboy serial log: {serial_log}")
    return serial_log.read_bytes().decode("ascii", errors="replace")


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


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build-sim-video-5g")
    parser.add_argument("--rom")
    parser.add_argument("--expect", default="Passed")
    parser.add_argument("--prepare", action="store_true")
    parser.add_argument("--validate")
    args = parser.parse_args(argv)

    if args.validate:
        validate(args)
    else:
        if not args.prepare:
            parser.error("one of --prepare or --validate is required")
        if not args.rom:
            parser.error("--rom is required with --prepare")
        prepare(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
