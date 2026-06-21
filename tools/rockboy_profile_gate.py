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
OPEN_PLUGIN_ENTRY_SIZE = 568
OPEN_PLUGIN_NAME_OFFSET = 12
OPEN_PLUGIN_NAME_SIZE = 33
OPEN_PLUGIN_PATH_OFFSET = 45
OPEN_PLUGIN_PATH_SIZE = 261
OPEN_PLUGIN_PARAM_OFFSET = 306
OPEN_PLUGIN_PARAM_SIZE = 261
START_SCREEN_HASH = 0x8E2A0CC9
LANG_START_SCREEN = 215
OPEN_PLUGIN_CHECKSUM = 0x02380507
ROCKBOY_PLUGIN_PATH = "/.rockbox/rocks/viewers/rockboy.rock"
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
    "dmg_bg_only_used",
    "dmg_bg_only_rejected",
    "dmg_bg_window_no_spr_used",
    "dmg_bg_window_no_spr_rejected",
    "cgb_bg_only_eligible",
    "cgb_bg_only_used",
    "cgb_bg_only_rejected",
    "cgb_bg_window_no_spr_eligible",
    "cgb_bg_window_no_spr_used",
    "cgb_bg_window_no_spr_rejected",
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


def _write_cstring(entry: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")
    if len(encoded) >= size:
        raise ValueError(f"open plugin field is too long: {value}")
    entry[offset : offset + size] = b"\0" * size
    entry[offset : offset + len(encoded)] = encoded


def _start_screen_metadata(plugin_dat: Path) -> tuple[int, int, int]:
    if plugin_dat.exists():
        data = plugin_dat.read_bytes()
        for offset in range(0, len(data) - OPEN_PLUGIN_ENTRY_SIZE + 1, OPEN_PLUGIN_ENTRY_SIZE):
            hash_value, lang_id, checksum = struct.unpack_from("<IiI", data, offset)
            if lang_id == LANG_START_SCREEN:
                return hash_value, lang_id, checksum
    return START_SCREEN_HASH, LANG_START_SCREEN, OPEN_PLUGIN_CHECKSUM


def _replace_config_prefix(lines: list[str], prefix: str, replacement: str) -> list[str]:
    filtered = [line for line in lines if not line.startswith(prefix)]
    filtered.append(replacement)
    return filtered


def write_start_screen_config(simdisk: Path, rom_arg: str) -> Path:
    config_path = simdisk / ".rockbox" / "config.cfg"
    if config_path.exists():
        lines = config_path.read_text(encoding="utf-8", errors="replace").splitlines()
    else:
        lines = []

    lines = _replace_config_prefix(lines, "start in screen:", "start in screen: plugin")
    lines = _replace_config_prefix(
        lines,
        "openplugin:",
        f'openplugin: "Start Screen", "rockboy.rock", "{ROCKBOY_PLUGIN_PATH}", "{rom_arg}"',
    )
    config_path.parent.mkdir(parents=True, exist_ok=True)
    config_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return config_path


def write_rockboy_direct_start(simdisk: Path, rom: Path) -> Path:
    plugin_dat = simdisk / ".rockbox" / "rocks" / "plugin.dat"
    hash_value, lang_id, checksum = _start_screen_metadata(plugin_dat)
    rom_arg = "@" + str((Path("/") / rom.relative_to(simdisk)).as_posix()).lstrip("/")

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    struct.pack_into("<IiI", entry, 0, hash_value, lang_id, checksum)
    _write_cstring(entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, "rockboy.rock")
    _write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, ROCKBOY_PLUGIN_PATH)
    _write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, rom_arg)

    plugin_dat.parent.mkdir(parents=True, exist_ok=True)
    plugin_dat.write_bytes(entry)
    write_start_screen_config(simdisk, rom_arg)
    return plugin_dat


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
    built_rockboy_plugin = build_dir / "apps" / "plugins" / "rockboy" / "rockboy.rock"
    if built_rockboy_plugin.is_file():
        shutil.copy2(built_rockboy_plugin, active_simdisk / ".rockbox" / "rocks" / "viewers" / "rockboy.rock")
    enable_profile_logging(active_simdisk / ".rockbox" / "rockboy" / "options")
    (active_simdisk / ".rockbox" / "rockboy" / "profile.log").unlink(missing_ok=True)

    manifest = gate_root / "rockboy-profile-roms.txt"
    manifest.write_text(
        "\n".join(str(Path("/") / rom.relative_to(source_simdisk)) for rom in roms) + "\n",
        encoding="utf-8",
    )

    if args.direct_start:
        direct_rom = active_simdisk / roms[0].relative_to(source_simdisk)
        plugin_dat = write_rockboy_direct_start(active_simdisk, direct_rom)
        print(f"Direct-start plugin entry: {plugin_dat}")
        print(f"Direct-start ROM: {Path('/') / roms[0].relative_to(source_simdisk)}")

    command = [str(binary), "--nobackground", "--root", str(active_simdisk), "--zoom", "1"]
    print(f"Prepared isolated Rockboy profile simdisk: {active_simdisk}")
    print(f"ROM manifest: {manifest}")
    print("Run simulator:")
    if args.autowrite_frames:
        print(f"ROCKBOY_PROFILE_AUTOWRITE_FRAMES={args.autowrite_frames} " + " ".join(command))
    else:
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
    parser.add_argument("--rom-only", action="append")
    parser.add_argument("--prepare", action="store_true")
    parser.add_argument("--validate")
    parser.add_argument("--direct-start", action="store_true")
    parser.add_argument("--autowrite-frames", type=int, default=0)
    args = parser.parse_args(argv)
    if args.rom_only:
        args.rom = args.rom_only

    if args.validate:
        validate(args)
    else:
        prepare(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
