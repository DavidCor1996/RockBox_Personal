#!/usr/bin/env python3
"""Run Rockboy in the simulator and compare a dumped LCD frame to a reference."""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from rockboy_accuracy_gate import (
    copy_rom,
    force_dmg_header,
    gate_work_root,
    minimal_simdisk_ignore,
    mirror_hosted_config,
    repo_root,
    terminate_simulator,
)
from rockboy_profile_gate import (
    ROCKBOY_OPTION_INTS,
    ROCKBOY_PLUGIN_PATH,
    write_rockboy_direct_start,
)


LCD_WIDTH = 160
LCD_HEIGHT = 144
LCD_RGB_BYTES = LCD_WIDTH * LCD_HEIGHT * 3
LCD_PPM_HEADER = b"P6\n160 144\n255\n"

OPTION_FRAMESKIP = 9
OPTION_MAXSKIP = 11
OPTION_SOUND = 12
OPTION_SCALING = 13
OPTION_SHOWSTATS = 14
OPTION_ROTATE = 16
OPTION_PAL = 17
OPTION_DIRTY = 18
OPTION_PERFORMANCE_PRESET = 20
OPTION_PROFILE = 21

PALETTES = {
    "brown": 0,
    "gray": 1,
}

PERFORMANCE_PRESETS = {
    "balanced": 0,
    "performance": 1,
    "quality": 2,
}


def hosted_rockboy_dir(simdisk: Path) -> Path:
    return simdisk / ".config" / "rockbox.org" / "rockboy"


def rockbox_rockboy_dir(simdisk: Path) -> Path:
    return simdisk / ".rockbox" / "rockboy"


def dump_paths(simdisk: Path, name: str) -> tuple[Path, Path]:
    return (
        hosted_rockboy_dir(simdisk) / name,
        rockbox_rockboy_dir(simdisk) / name,
    )


def option_paths(simdisk: Path) -> tuple[Path, Path]:
    return (
        rockbox_rockboy_dir(simdisk) / "options",
        hosted_rockboy_dir(simdisk) / "options",
    )


def parse_palette(value: str) -> int:
    if value in PALETTES:
        return PALETTES[value]
    try:
        palette = int(value, 10)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"unknown palette: {value}") from exc
    if palette < 0 or palette > 16:
        raise argparse.ArgumentTypeError("palette must be between 0 and 16")
    return palette


def read_options(path: Path) -> list[int]:
    expected_size = ROCKBOY_OPTION_INTS * 4
    if path.exists():
        data = bytearray(path.read_bytes())
    else:
        data = bytearray()

    if len(data) < expected_size:
        data.extend(b"\0" * (expected_size - len(data)))
    return list(struct.unpack("<" + "i" * ROCKBOY_OPTION_INTS, data[:expected_size]))


def write_lcd_options(path: Path, *, palette: int, performance_preset: int, sound: bool = False) -> None:
    values = read_options(path)
    values[OPTION_FRAMESKIP] = 0
    values[OPTION_MAXSKIP] = 0
    values[OPTION_SOUND] = 1 if sound else 0
    values[OPTION_SCALING] = 2
    values[OPTION_SHOWSTATS] = 0
    values[OPTION_ROTATE] = 0
    values[OPTION_PAL] = palette
    values[OPTION_DIRTY] = 1
    values[OPTION_PERFORMANCE_PRESET] = performance_preset
    values[OPTION_PROFILE] = 0

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(struct.pack("<" + "i" * ROCKBOY_OPTION_INTS, *values))


def prepare_lcd(args: argparse.Namespace) -> Path:
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
        raise SystemExit(f"missing LCD reference ROM: {rom_path}")

    gate_root = Path(tempfile.mkdtemp(
        prefix="rockboy-lcd-reference-",
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

    direct_rom = copy_rom(active_simdisk, rom_path)
    if args.force_dmg:
        force_dmg_header(direct_rom)
    write_rockboy_direct_start(active_simdisk, direct_rom, build_dir=build_dir)
    mirror_hosted_config(active_simdisk)

    performance_preset = PERFORMANCE_PRESETS[args.performance_preset]
    for options_path in option_paths(active_simdisk):
        write_lcd_options(
            options_path,
            palette=args.palette,
            performance_preset=performance_preset,
            sound=args.sound,
        )

    for dump_path in dump_paths(active_simdisk, args.dump_name):
        dump_path.unlink(missing_ok=True)

    command = [str(binary), "--nobackground", "--root", str(active_simdisk), "--zoom", "1"]
    print(f"Prepared isolated Rockboy LCD reference simdisk: {active_simdisk}")
    print(f"Direct-start ROM: {Path('/') / direct_rom.relative_to(active_simdisk)}")
    if args.force_dmg:
        print("Forced DMG mode: patched staged ROM CGB flag to 0x00")
    print(f"LCD dump frame: {args.frame}")
    print("Run simulator:")
    env_preview = (
        f"RBROOT={active_simdisk} ROCKBOY_LCD_DUMP_FRAME={args.frame} "
        f"ROCKBOY_LCD_DUMP_NAME={args.dump_name} "
    )
    if args.input_script:
        env_preview += f"ROCKBOY_INPUT_SCRIPT={args.input_script!r} "
    print(env_preview + " ".join(command))
    print("Validate after the dump is written:")
    if args.reference:
        print(
            f"{Path(__file__).relative_to(root)} --validate {active_simdisk} "
            f"--reference {args.reference} --dump-name {args.dump_name}"
        )
    else:
        print(f"Dump-only run: no reference image configured")
    return active_simdisk


def load_rgb(path: Path) -> tuple[bytes, tuple[int, int]]:
    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("Pillow is required for LCD reference image comparison") from exc

    with Image.open(path) as image:
        rgb = image.convert("RGB")
        return rgb.tobytes(), rgb.size


def write_diff_image(actual: Path, reference: Path, diff_path: Path, *, tolerance: int) -> None:
    try:
        from PIL import Image
    except ImportError:
        return

    with Image.open(actual) as actual_image, Image.open(reference) as reference_image:
        actual_rgb = actual_image.convert("RGB")
        reference_rgb = reference_image.convert("RGB")
        if actual_rgb.size != reference_rgb.size:
            return

        actual_bytes = actual_rgb.tobytes()
        reference_bytes = reference_rgb.tobytes()
        diff_bytes = bytearray(len(actual_bytes))
        for offset in range(0, len(actual_bytes), 3):
            pixel_delta = max(
                abs(actual_bytes[offset] - reference_bytes[offset]),
                abs(actual_bytes[offset + 1] - reference_bytes[offset + 1]),
                abs(actual_bytes[offset + 2] - reference_bytes[offset + 2]),
            )
            if pixel_delta <= tolerance:
                diff_bytes[offset : offset + 3] = b"\x00\x00\x00"
            else:
                diff_bytes[offset : offset + 3] = b"\xff\x00\x00"

        diff = Image.frombytes("RGB", actual_rgb.size, bytes(diff_bytes))
        diff_path.parent.mkdir(parents=True, exist_ok=True)
        diff.save(diff_path)


def compare_images(
    actual: Path,
    reference: Path,
    *,
    tolerance: int,
    max_diff_pixels: int,
    diff_output: Path,
) -> None:
    actual_bytes, actual_size = load_rgb(actual)
    reference_bytes, reference_size = load_rgb(reference)

    if actual_size != (LCD_WIDTH, LCD_HEIGHT):
        raise SystemExit(f"dump has size {actual_size}, expected {(LCD_WIDTH, LCD_HEIGHT)}")
    if reference_size != (LCD_WIDTH, LCD_HEIGHT):
        raise SystemExit(f"reference has size {reference_size}, expected {(LCD_WIDTH, LCD_HEIGHT)}")

    diff_pixels = 0
    max_delta = 0
    for offset in range(0, len(actual_bytes), 3):
        channels = (
            abs(actual_bytes[offset] - reference_bytes[offset]),
            abs(actual_bytes[offset + 1] - reference_bytes[offset + 1]),
            abs(actual_bytes[offset + 2] - reference_bytes[offset + 2]),
        )
        pixel_delta = max(channels)
        max_delta = max(max_delta, pixel_delta)
        if pixel_delta > tolerance:
            diff_pixels += 1

    if diff_pixels > max_diff_pixels:
        write_diff_image(actual, reference, diff_output, tolerance=tolerance)
        raise SystemExit(
            f"LCD reference mismatch: {diff_pixels} pixels differ beyond tolerance "
            f"{tolerance}; max channel delta {max_delta}; diff image {diff_output}"
        )

    print(
        f"validated LCD reference frame: {actual} matches {reference} "
        f"(diff pixels {diff_pixels}, max channel delta {max_delta})"
    )


def is_complete_ppm(path: Path) -> bool:
    return path.exists() and path.stat().st_size >= len(LCD_PPM_HEADER) + LCD_RGB_BYTES


def ppm_rgb_body(path: Path) -> bytes:
    data = path.read_bytes()
    if not data.startswith(LCD_PPM_HEADER):
        raise SystemExit(f"LCD dump has unexpected PPM header: {path}")
    body = data[len(LCD_PPM_HEADER):len(LCD_PPM_HEADER) + LCD_RGB_BYTES]
    if len(body) != LCD_RGB_BYTES:
        raise SystemExit(f"LCD dump is incomplete: {path}")
    return body


def unique_rgb_pixels(rgb: bytes, limit: int = 2) -> int:
    seen: set[bytes] = set()
    for offset in range(0, len(rgb), 3):
        seen.add(rgb[offset:offset + 3])
        if len(seen) >= limit:
            break
    return len(seen)


def validate_nonblank_dump(path: Path) -> None:
    rgb = ppm_rgb_body(path)
    unique_pixels = unique_rgb_pixels(rgb)
    if unique_pixels < 2:
        raise SystemExit(f"LCD dump appears blank: {path}")
    print(f"validated LCD dump is non-blank: {path}")


def find_complete_dump(simdisk: Path, name: str) -> Path | None:
    for dump_path in dump_paths(simdisk, name):
        if is_complete_ppm(dump_path):
            return dump_path
    return None


def log_tail(path: Path, limit: int = 4000) -> str:
    if not path.exists():
        return ""
    data = path.read_bytes()
    return data[-limit:].decode("utf-8", errors="replace")


def run(args: argparse.Namespace) -> Path:
    root = repo_root()
    build_dir = (root / args.build_dir).resolve()
    binary = build_dir / "rockboxui"
    simdisk = prepare_lcd(args)
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
            "ROCKBOY_LCD_DUMP_FRAME": str(args.frame),
            "ROCKBOY_LCD_DUMP_NAME": args.dump_name,
            "ROCKBOX_SIM_PLUGIN": ROCKBOY_PLUGIN_PATH,
            "ROCKBOX_SIM_PLUGIN_PARAM": rom_param,
        }
    )
    if args.input_script:
        env["ROCKBOY_INPUT_SCRIPT"] = args.input_script
    if args.dummy_video:
        env.setdefault("SDL_VIDEODRIVER", "dummy")
        env.setdefault("SDL_AUDIODRIVER", "dummy")

    log_path = simdisk.parent / "rockboxui-lcd-reference.log"
    deadline = time.monotonic() + args.timeout
    print(f"Running simulator for up to {args.timeout:.1f}s")
    with log_path.open("wb") as log_file:
        process = subprocess.Popen(command, cwd=root, env=env, stdout=log_file, stderr=subprocess.STDOUT)
        try:
            dump_path: Path | None = None
            while time.monotonic() < deadline:
                dump_path = find_complete_dump(simdisk, args.dump_name)
                if dump_path is not None:
                    print(f"Observed complete LCD dump: {dump_path}")
                    break
                if process.poll() is not None:
                    break
                time.sleep(args.poll_interval)
        finally:
            terminate_simulator(process)

    dump_path = find_complete_dump(simdisk, args.dump_name)
    if dump_path is None:
        tail = log_tail(log_path)
        extra = f"\nSimulator log tail:\n{tail}" if tail else ""
        raise SystemExit(
            "timed out before Rockboy wrote a complete LCD dump; checked "
            + ", ".join(str(path) for path in dump_paths(simdisk, args.dump_name))
            + extra
        )

    if args.require_nonblank:
        validate_nonblank_dump(dump_path)

    if args.dump_only or not args.reference:
        print(f"captured LCD dump: {dump_path}")
    else:
        validate_dump(args, simdisk, dump_path)
    return dump_path


def validate_dump(args: argparse.Namespace, simdisk: Path, dump_path: Path | None = None) -> None:
    reference = Path(args.reference).expanduser().resolve()
    if not reference.is_file():
        raise SystemExit(f"missing LCD reference image: {reference}")

    if dump_path is None:
        dump_path = find_complete_dump(simdisk, args.dump_name)
    if dump_path is None:
        raise SystemExit(
            "missing complete LCD dump; checked "
            + ", ".join(str(path) for path in dump_paths(simdisk, args.dump_name))
        )

    diff_output = Path(args.diff_output).expanduser().resolve() if args.diff_output else dump_path.with_suffix(".diff.png")
    compare_images(
        dump_path,
        reference,
        tolerance=args.tolerance,
        max_diff_pixels=args.max_diff_pixels,
        diff_output=diff_output,
    )


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build-sim-video-5g")
    parser.add_argument("--rom")
    parser.add_argument("--reference")
    parser.add_argument("--force-dmg", action="store_true")
    parser.add_argument("--frame", type=int, default=90)
    parser.add_argument("--dump-name", default="lcd-reference.ppm")
    parser.add_argument("--palette", type=parse_palette, default=PALETTES["gray"])
    parser.add_argument("--performance-preset", choices=sorted(PERFORMANCE_PRESETS), default="quality")
    parser.add_argument("--tolerance", type=int, default=0)
    parser.add_argument("--max-diff-pixels", type=int, default=0)
    parser.add_argument("--diff-output")
    parser.add_argument("--prepare", action="store_true")
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--dump-only", action="store_true")
    parser.add_argument("--sound", action="store_true")
    parser.add_argument("--require-nonblank", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument(
        "--input-script",
        help="Simulator-only scripted input, e.g. '2500:START:P,2510:START:R'",
    )
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--poll-interval", type=float, default=0.25)
    parser.add_argument("--validate")
    parser.add_argument("--dummy-video", action=argparse.BooleanOptionalAction, default=True)
    args = parser.parse_args(argv)

    if args.validate:
        if not args.reference:
            parser.error("--reference is required with --validate")
        validate_dump(args, Path(args.validate).resolve())
    elif args.run:
        if not args.rom:
            parser.error("--rom is required with --run")
        if not args.dump_only and not args.reference:
            parser.error("--reference is required with --run unless --dump-only is set")
        run(args)
    else:
        if not args.prepare:
            parser.error("one of --prepare, --run, or --validate is required")
        if not args.rom:
            parser.error("--rom is required with --prepare")
        if not args.dump_only and not args.reference:
            parser.error("--reference is required with --prepare unless --dump-only is set")
        prepare_lcd(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
