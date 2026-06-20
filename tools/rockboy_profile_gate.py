#!/usr/bin/env python3
"""Prepare and validate Rockboy simulator profile runs.

rockboxui does not currently expose a command-line way to launch a viewer
plugin with a ROM parameter. This gate handles the repeatable parts around the
manual simulator run: isolated simdisk setup, Rockboy profile-log enablement,
ROM corpus discovery, and profile.log validation after the run.
"""

from __future__ import annotations

import argparse
import re
import shutil
import struct
import sys
import tempfile
from pathlib import Path


PROFILE_OFF = 0
PROFILE_OVERLAY_AND_LOG = 2
ROCKBOY_OPTION_INTS = 22
REQUIRED_PROFILE_FIELDS = {
    "rom",
    "rendered_frames",
    "skipped_frames",
    "frame_avg_ticks",
    "cpu_avg_ticks",
    "lcd_render_avg_ticks",
    "scale_avg_ticks",
    "blit_avg_ticks",
    "audio_mix_avg_ticks",
    "pcm_wait_avg_ticks",
    "pcm_underruns",
    "cpu_ops",
    "slow_mem_reads",
    "slow_mem_writes",
    "vram_dirty_writes",
    "lcd_lines",
    "dmg_lines",
    "cgb_lines",
    "no_sprite_lines",
    "no_window_lines",
    "dmg_bg_only_eligible",
    "dmg_bg_window_no_spr_eligible",
    "cgb_no_sprite_lines",
}

DEFAULT_ROM_PATTERNS = (
    "Tetris*.gb",
    "Pokemon - Red*.gb",
    "Legend of Zelda, The - Oracle*.gbc",
    "Mario Tennis*.gbc",
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def discover_roms(simdisk: Path, patterns: tuple[str, ...]) -> list[Path]:
    rom_root = simdisk / "gameboy"
    roms: list[Path] = []
    for pattern in patterns:
        matches = sorted(rom_root.glob(pattern))
        if not matches:
            raise SystemExit(f"missing Rockboy profile ROM matching {pattern!r} in {rom_root}")
        roms.append(matches[0])
    return roms


def enable_profile_logging(options_path: Path) -> None:
    options_path.parent.mkdir(parents=True, exist_ok=True)
    if options_path.exists():
        data = bytearray(options_path.read_bytes())
    else:
        data = bytearray()

    expected_size = ROCKBOY_OPTION_INTS * 4
    if len(data) < expected_size:
        data.extend(b"\0" * (expected_size - len(data)))

    values = list(struct.unpack("<" + "i" * ROCKBOY_OPTION_INTS, data[:expected_size]))
    values[-1] = PROFILE_OVERLAY_AND_LOG
    options_path.write_bytes(struct.pack("<" + "i" * ROCKBOY_OPTION_INTS, *values))


def parse_profile_log(path: Path) -> list[dict[str, str]]:
    entries: list[dict[str, str]] = []
    if not path.exists():
        return entries
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        fields = dict(re.findall(r"([A-Za-z0-9_]+)=([^ ]+)", line))
        if fields:
            entries.append(fields)
    return entries


def validate_profile_log(path: Path) -> None:
    entries = parse_profile_log(path)
    if not entries:
        raise SystemExit(f"no Rockboy profile entries found in {path}")

    missing_by_line: list[str] = []
    for index, fields in enumerate(entries, 1):
        missing = sorted(REQUIRED_PROFILE_FIELDS - set(fields))
        if missing:
            missing_by_line.append(f"line {index}: {', '.join(missing)}")
    if missing_by_line:
        raise SystemExit("profile.log is missing required fields:\n" + "\n".join(missing_by_line))

    print(f"validated {len(entries)} Rockboy profile entr{'y' if len(entries) == 1 else 'ies'} in {path}")


def prepare(args: argparse.Namespace) -> None:
    root = repo_root()
    build_dir = (root / args.build_dir).resolve()
    source_simdisk = build_dir / "simdisk"
    binary = build_dir / "rockboxui"
    rockboy_plugin = source_simdisk / ".rockbox" / "rocks" / "viewers" / "rockboy.rock"

    if not binary.is_file():
        raise SystemExit(f"missing simulator binary: {binary}")
    if not source_simdisk.is_dir():
        raise SystemExit(f"missing simulator simdisk: {source_simdisk}")
    if not rockboy_plugin.is_file():
        raise SystemExit(f"missing Rockboy plugin in simdisk: {rockboy_plugin}")

    roms = discover_roms(source_simdisk, tuple(args.rom))
    gate_root = Path(tempfile.mkdtemp(prefix="rockboy-profile-gate-"))
    active_simdisk = gate_root / "simdisk"
    shutil.copytree(source_simdisk, active_simdisk, symlinks=True)
    enable_profile_logging(active_simdisk / ".rockbox" / "rockboy" / "options")

    manifest = gate_root / "rockboy-profile-roms.txt"
    manifest.write_text(
        "\n".join(str(Path("/") / rom.relative_to(source_simdisk)) for rom in roms) + "\n",
        encoding="utf-8",
    )

    command = [str(binary), "--nobackground", "--root", str(active_simdisk), "--zoom", "1"]
    print(f"Prepared isolated Rockboy profile simdisk: {active_simdisk}")
    print(f"ROM manifest: {manifest}")
    print("Run simulator:")
    print(" ".join(command))
    print("In simulator, open each ROM listed in the manifest, run long enough to collect frames, then quit Rockboy.")
    print("Validate after the run:")
    print(f"{Path(__file__).relative_to(root)} --validate {active_simdisk}")


def validate(args: argparse.Namespace) -> None:
    simdisk = Path(args.validate).resolve()
    validate_profile_log(simdisk / ".rockbox" / "rockboy" / "profile.log")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build-sim-video-5g")
    parser.add_argument("--rom", action="append", default=list(DEFAULT_ROM_PATTERNS))
    parser.add_argument("--prepare", action="store_true")
    parser.add_argument("--validate")
    args = parser.parse_args(argv)

    if args.validate:
        validate(args)
    else:
        prepare(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
