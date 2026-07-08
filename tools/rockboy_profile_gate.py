#!/usr/bin/env python3
"""Prepare and validate Rockboy simulator profile runs.

rockboxui does not currently expose a command-line way to launch a viewer
plugin with a ROM parameter. This gate handles the repeatable parts around the
manual simulator run: isolated simdisk setup, Rockboy profile-log enablement,
ROM corpus discovery, and profile.log validation after the run.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
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
OPEN_PLUGIN_CHECKSUM = (
    (OPEN_PLUGIN_ENTRY_SIZE << 16)
    + 0
    + 4
    + 8
    + OPEN_PLUGIN_NAME_OFFSET
    + OPEN_PLUGIN_PATH_OFFSET
    + OPEN_PLUGIN_PARAM_OFFSET
)
ROCKBOY_PLUGIN_PATH = "/.rockbox/rocks/viewers/rockboy.rock"
OPTION_SOUND = 12
OPTION_FRAMESKIP = 9
OPTION_MAXSKIP = 11
OPTION_SCALING = 13
OPTION_DIRTY = 18
OPTION_PERFORMANCE_PRESET = 20
TARGET_FPS_X1000 = 59728
DEFAULT_SPEED_TOLERANCE_PERCENT = 8.0
DEFAULT_MAX_SKIP_RATIO = 0.05
REQUIRED_PROFILE_FIELDS = {
    "rom",
    "rendered_frames",
    "skipped_frames",
    "frame_avg_ticks",
    "cpu_avg_ticks",
    "frame_avg_ticks_x1000",
    "target_frame_ticks_x1000",
    "target_fps_x1000",
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


def copy_rom(active_simdisk: Path, rom_path: Path) -> Path:
    target_dir = active_simdisk / "gameboy" / "accuracy"
    target_dir.mkdir(parents=True, exist_ok=True)
    target = target_dir / rom_path.name
    shutil.copy2(rom_path, target)
    return target


def gate_work_root(root: Path) -> Path:
    path = root / "tmp" / "rockboy-profile-gates"
    path.mkdir(parents=True, exist_ok=True)
    return path


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
    set_profile_logging(options_path, PROFILE_OVERLAY_AND_LOG)


def set_profile_logging(options_path: Path, mode: int) -> None:
    options_path.parent.mkdir(parents=True, exist_ok=True)
    if options_path.exists():
        data = bytearray(options_path.read_bytes())
    else:
        data = bytearray()

    expected_size = ROCKBOY_OPTION_INTS * 4
    if len(data) < expected_size:
        data.extend(b"\0" * (expected_size - len(data)))

    values = list(struct.unpack("<" + "i" * ROCKBOY_OPTION_INTS, data[:expected_size]))
    values[-1] = mode
    options_path.write_bytes(struct.pack("<" + "i" * ROCKBOY_OPTION_INTS, *values))


def set_speed_gate_options(options_path: Path, *, sound: bool, performance_preset: int) -> None:
    options_path.parent.mkdir(parents=True, exist_ok=True)
    if options_path.exists():
        data = bytearray(options_path.read_bytes())
    else:
        data = bytearray()

    expected_size = ROCKBOY_OPTION_INTS * 4
    if len(data) < expected_size:
        data.extend(b"\0" * (expected_size - len(data)))

    values = list(struct.unpack("<" + "i" * ROCKBOY_OPTION_INTS, data[:expected_size]))
    values[OPTION_FRAMESKIP] = 0
    values[OPTION_MAXSKIP] = 0
    values[OPTION_SOUND] = 1 if sound else 0
    values[OPTION_SCALING] = 0
    values[OPTION_DIRTY] = 1
    values[OPTION_PERFORMANCE_PRESET] = performance_preset
    values[-1] = PROFILE_OFF
    options_path.write_bytes(struct.pack("<" + "i" * ROCKBOY_OPTION_INTS, *values))


def profile_log_paths(simdisk: Path) -> tuple[Path, Path]:
    return (
        simdisk / ".config" / "rockbox.org" / "rockboy" / "profile.log",
        simdisk / ".rockbox" / "rockboy" / "profile.log",
    )


def performance_log_paths(simdisk: Path) -> tuple[Path, Path]:
    return (
        simdisk / ".config" / "rockbox.org" / "rockboy" / "performance.log",
        simdisk / ".rockbox" / "rockboy" / "performance.log",
    )


def profile_option_paths(simdisk: Path) -> tuple[Path, Path]:
    return (
        simdisk / ".rockbox" / "rockboy" / "options",
        simdisk / ".config" / "rockbox.org" / "rockboy" / "options",
    )


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


def _write_cstring(entry: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")
    if len(encoded) >= size:
        raise ValueError(f"open plugin field is too long: {value}")
    entry[offset : offset + size] = b"\0" * size
    entry[offset : offset + len(encoded)] = encoded


def _read_lang_last_index(build_dir: Path) -> int | None:
    lang_enum = build_dir / "lang_enum.h"
    if not lang_enum.exists():
        return None

    last_number: int | None = None
    for line in lang_enum.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.search(r"/\*\s*(\d+)\s*\*/", line)
        if match:
            last_number = int(match.group(1))
        if "LANG_LAST_INDEX_IN_ARRAY" in line:
            return None if last_number is None else last_number + 1
    return None


def open_plugin_lang_checksum(build_dir: Path | None = None) -> int:
    if build_dir is not None:
        lang_last = _read_lang_last_index(build_dir)
        if lang_last is not None:
            return OPEN_PLUGIN_CHECKSUM + lang_last
    return OPEN_PLUGIN_CHECKSUM


def _start_screen_metadata(
    plugin_dat: Path,
    fallback_checksum: int = OPEN_PLUGIN_CHECKSUM,
    *,
    prefer_fallback_checksum: bool = False,
) -> tuple[int, int, int]:
    valid_lang_checksum: int | None = None
    if plugin_dat.exists():
        data = plugin_dat.read_bytes()
        for offset in range(0, len(data) - OPEN_PLUGIN_ENTRY_SIZE + 1, OPEN_PLUGIN_ENTRY_SIZE):
            hash_value, lang_id, checksum = struct.unpack_from("<IiI", data, offset)
            if lang_id == LANG_START_SCREEN:
                if prefer_fallback_checksum:
                    checksum = fallback_checksum
                return hash_value, lang_id, checksum
            if lang_id >= 0 and checksum:
                valid_lang_checksum = checksum
    return START_SCREEN_HASH, LANG_START_SCREEN, valid_lang_checksum or fallback_checksum


def _replace_config_prefix(lines: list[str], prefix: str, replacement: str) -> list[str]:
    filtered = [line for line in lines if not line.startswith(prefix)]
    filtered.append(replacement)
    return filtered


def write_start_screen_config(simdisk: Path, rom_arg: str) -> Path:
    config_paths = [
        simdisk / ".rockbox" / "config.cfg",
        simdisk / "config.cfg",
    ]
    written_path = config_paths[0]

    for config_path in config_paths:
        if config_path.exists():
            lines = config_path.read_text(encoding="utf-8", errors="replace").splitlines()
        else:
            lines = []

        lines = _replace_config_prefix(lines, "start in screen:", "start in screen: plugin")
        lines = [line for line in lines if not line.startswith("openplugin:")]
        lines.append(
            'openplugin: "Start Screen", "rockboy.rock", '
            f'"{ROCKBOY_PLUGIN_PATH}", "{rom_arg}"'
        )
        config_path.parent.mkdir(parents=True, exist_ok=True)
        config_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    return written_path


def write_rockboy_direct_start(simdisk: Path, rom: Path, *, build_dir: Path | None = None) -> Path:
    plugin_dat = simdisk / ".rockbox" / "rocks" / "plugin.dat"
    fallback_checksum = open_plugin_lang_checksum(build_dir)
    hash_value, lang_id, checksum = _start_screen_metadata(
        plugin_dat,
        fallback_checksum=fallback_checksum,
        prefer_fallback_checksum=build_dir is not None and fallback_checksum != OPEN_PLUGIN_CHECKSUM,
    )
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


def parse_key_value_log(path: Path) -> list[dict[str, str]]:
    entries: list[dict[str, str]] = []
    if not path.exists():
        return entries
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        fields = dict(re.findall(r"([A-Za-z0-9_]+)=([^ ]+)", line))
        if fields:
            entries.append(fields)
    return entries


def parse_profile_log(path: Path) -> list[dict[str, str]]:
    return parse_key_value_log(path)


def parse_performance_log(path: Path) -> list[dict[str, str]]:
    return parse_key_value_log(path)


def _read_number(fields: dict[str, str], name: str, line: int) -> int:
    try:
        return int(fields[name], 10)
    except ValueError as exc:
        raise SystemExit(f"profile.log line {line} has non-numeric {name}: {fields[name]!r}") from exc


def _validate_profile_speed(
    fields: dict[str, str],
    *,
    line: int,
    tolerance_percent: float,
    max_skip_ratio: float,
    log_name: str = "profile.log",
) -> None:
    target_fps_x1000 = _read_number(fields, "target_fps_x1000", line)
    target_frame_ticks_x1000 = _read_number(fields, "target_frame_ticks_x1000", line)
    frame_avg_ticks_x1000 = _read_number(fields, "frame_avg_ticks_x1000", line)
    rendered_frames = _read_number(fields, "rendered_frames", line)
    skipped_frames = _read_number(fields, "skipped_frames", line)
    total_frames = rendered_frames + skipped_frames
    tolerance = tolerance_percent / 100.0
    min_ticks = target_frame_ticks_x1000 * (1.0 - tolerance)
    max_ticks = target_frame_ticks_x1000 * (1.0 + tolerance)

    if target_fps_x1000 != TARGET_FPS_X1000:
        raise SystemExit(
            f"{log_name} line {line} target_fps_x1000={target_fps_x1000}, "
            f"expected {TARGET_FPS_X1000}"
        )
    if not min_ticks <= frame_avg_ticks_x1000 <= max_ticks:
        raise SystemExit(
            f"{log_name} line {line} frame pacing drifted: "
            f"avg={frame_avg_ticks_x1000 / 1000:.3f} ticks, "
            f"target={target_frame_ticks_x1000 / 1000:.3f} ticks, "
            f"tolerance={tolerance_percent:.1f}%"
        )
    if total_frames > 0 and skipped_frames / total_frames > max_skip_ratio:
        raise SystemExit(
            f"{log_name} line {line} skipped too many frames: "
            f"{skipped_frames}/{total_frames} "
            f"({(skipped_frames / total_frames) * 100:.1f}%, "
            f"limit {max_skip_ratio * 100:.1f}%)"
        )


def validate_performance_log(
    path: Path,
    *,
    tolerance_percent: float = DEFAULT_SPEED_TOLERANCE_PERCENT,
    max_skip_ratio: float = DEFAULT_MAX_SKIP_RATIO,
) -> None:
    entries = parse_performance_log(path)
    if not entries:
        raise SystemExit(f"no Rockboy performance entries found in {path}")

    for index, fields in enumerate(entries, 1):
        required = {
            "rom",
            "total_frames",
            "rendered_frames",
            "skipped_frames",
            "elapsed_ticks",
            "frame_avg_ticks_x1000",
            "target_frame_ticks_x1000",
            "effective_fps_x1000",
            "target_fps_x1000",
        }
        missing = sorted(required - set(fields))
        if missing:
            raise SystemExit(
                f"performance.log line {index} is missing required fields: "
                + ", ".join(missing)
            )
        _validate_profile_speed(
            fields,
            line=index,
            tolerance_percent=tolerance_percent,
            max_skip_ratio=max_skip_ratio,
            log_name="performance.log",
        )

    print(f"validated {len(entries)} Rockboy performance entr{'y' if len(entries) == 1 else 'ies'} in {path}")


def validate_profile_log(
    path: Path,
    *,
    validate_speed: bool = False,
    tolerance_percent: float = DEFAULT_SPEED_TOLERANCE_PERCENT,
    max_skip_ratio: float = DEFAULT_MAX_SKIP_RATIO,
) -> None:
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

    if validate_speed:
        for index, fields in enumerate(entries, 1):
            _validate_profile_speed(
                fields,
                line=index,
                tolerance_percent=tolerance_percent,
                max_skip_ratio=max_skip_ratio,
            )

    print(f"validated {len(entries)} Rockboy profile entr{'y' if len(entries) == 1 else 'ies'} in {path}")


def newest_profile_log(simdisk: Path) -> Path:
    logs = [path for path in profile_log_paths(simdisk) if path.exists()]
    if not logs:
        return profile_log_paths(simdisk)[0]
    return max(logs, key=lambda path: path.stat().st_mtime)


def newest_performance_log(simdisk: Path) -> Path:
    logs = [path for path in performance_log_paths(simdisk) if path.exists()]
    if not logs:
        return performance_log_paths(simdisk)[0]
    return max(logs, key=lambda path: path.stat().st_mtime)


def has_profile_entries(simdisk: Path) -> bool:
    return any(parse_profile_log(path) for path in profile_log_paths(simdisk))


def has_performance_entries(simdisk: Path) -> bool:
    return any(parse_performance_log(path) for path in performance_log_paths(simdisk))


def prepare(args: argparse.Namespace) -> Path:
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
    gate_root = Path(tempfile.mkdtemp(
        prefix="rockboy-profile-gate-",
        dir=gate_work_root(root),
    ))
    active_simdisk = gate_root / "simdisk"
    shutil.copytree(
        source_simdisk,
        active_simdisk,
        symlinks=True,
        ignore=minimal_simdisk_ignore,
    )
    built_rockboy_plugin = build_dir / "apps" / "plugins" / "rockboy" / "rockboy.rock"
    if built_rockboy_plugin.is_file():
        shutil.copy2(built_rockboy_plugin, active_simdisk / ".rockbox" / "rocks" / "viewers" / "rockboy.rock")
    staged_roms = [copy_rom(active_simdisk, rom) for rom in roms]
    for options_path in profile_option_paths(active_simdisk):
        enable_profile_logging(options_path)
    for profile_log in profile_log_paths(active_simdisk):
        profile_log.parent.mkdir(parents=True, exist_ok=True)
        profile_log.unlink(missing_ok=True)
    for performance_log in performance_log_paths(active_simdisk):
        performance_log.parent.mkdir(parents=True, exist_ok=True)
        performance_log.unlink(missing_ok=True)

    manifest = gate_root / "rockboy-profile-roms.txt"
    manifest.write_text(
        "\n".join(str(Path("/") / rom.relative_to(active_simdisk)) for rom in staged_roms) + "\n",
        encoding="utf-8",
    )

    if args.direct_start:
        direct_rom = staged_roms[0]
        plugin_dat = write_rockboy_direct_start(active_simdisk, direct_rom, build_dir=build_dir)
        mirror_hosted_config(active_simdisk)
        print(f"Direct-start plugin entry: {plugin_dat}")
        print(f"Direct-start ROM: {Path('/') / direct_rom.relative_to(active_simdisk)}")

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
    return active_simdisk


def validate(args: argparse.Namespace) -> None:
    simdisk = Path(args.validate).resolve()
    validate_profile_log(
        newest_profile_log(simdisk),
        validate_speed=args.validate_speed,
        tolerance_percent=args.speed_tolerance_percent,
        max_skip_ratio=args.max_skip_ratio,
    )


def terminate_simulator(process: subprocess.Popen[bytes]) -> None:
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
    source_simdisk = build_dir / "simdisk"

    args.direct_start = True
    simdisk = prepare(args)
    for options_path in profile_option_paths(simdisk):
        set_speed_gate_options(
            options_path,
            sound=args.sound,
            performance_preset=args.performance_preset,
        )
    roms = discover_roms(source_simdisk, tuple(args.rom))
    direct_rom = simdisk / "gameboy" / "accuracy" / roms[0].name
    rom_param = str(Path("/") / direct_rom.relative_to(simdisk))
    command = [str(binary), "--nobackground", "--root", str(simdisk), "--zoom", "1"]
    env = os.environ.copy()
    env.update(
        {
            "RBROOT": str(simdisk),
            "ROCKBOY_PERF_AUTOWRITE_FRAMES": str(args.autowrite_frames),
            "ROCKBOX_SIM_PLUGIN": ROCKBOY_PLUGIN_PATH,
            "ROCKBOX_SIM_PLUGIN_PARAM": rom_param,
        }
    )
    if args.dummy_video:
        env.setdefault("SDL_VIDEODRIVER", "dummy")
        env.setdefault("SDL_AUDIODRIVER", "dummy")

    log_path = simdisk.parent / "rockboxui-profile.log"
    deadline = time.monotonic() + args.timeout
    print(f"Running simulator for up to {args.timeout:.1f}s")
    with log_path.open("wb") as log_file:
        process = subprocess.Popen(command, cwd=root, env=env, stdout=log_file, stderr=subprocess.STDOUT)
        try:
            while time.monotonic() < deadline:
                if has_performance_entries(simdisk):
                    print("Observed Rockboy performance log; stopping simulator")
                    break
                if process.poll() is not None:
                    break
                time.sleep(args.poll_interval)
        finally:
            terminate_simulator(process)

    validate_performance_log(
        newest_performance_log(simdisk),
        tolerance_percent=args.speed_tolerance_percent,
        max_skip_ratio=args.max_skip_ratio,
    )


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build-sim-video-5g")
    parser.add_argument("--rom", action="append", default=list(DEFAULT_ROM_PATTERNS))
    parser.add_argument("--rom-only", action="append")
    parser.add_argument("--prepare", action="store_true")
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--validate")
    parser.add_argument("--direct-start", action="store_true")
    parser.add_argument("--autowrite-frames", type=int, default=180)
    parser.add_argument("--validate-speed", action="store_true")
    parser.add_argument("--speed-tolerance-percent", type=float, default=DEFAULT_SPEED_TOLERANCE_PERCENT)
    parser.add_argument("--max-skip-ratio", type=float, default=DEFAULT_MAX_SKIP_RATIO)
    parser.add_argument("--timeout", type=float, default=90.0)
    parser.add_argument("--poll-interval", type=float, default=0.25)
    parser.add_argument("--dummy-video", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--sound", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--performance-preset", type=int, choices=(0, 1, 2), default=0)
    args = parser.parse_args(argv)
    if args.rom_only:
        args.rom = args.rom_only

    if args.validate:
        validate(args)
    elif args.run:
        if args.autowrite_frames <= 0:
            parser.error("--autowrite-frames must be greater than 0 with --run")
        run(args)
    else:
        prepare(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
