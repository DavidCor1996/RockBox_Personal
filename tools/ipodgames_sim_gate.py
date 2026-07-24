#!/usr/bin/env python3
"""Run the iPod Games metadata/eApp loader through the iPod 6G simulator."""

from __future__ import annotations

import argparse
import csv
import math
import os
import shutil
import subprocess
import time
from pathlib import Path

try:
    from tools.ipodgames.eapp_inspect import inspect_eapp
except ModuleNotFoundError:
    from ipodgames.eapp_inspect import inspect_eapp


PLUGIN_PATH = "/.rockbox/rocks/games/ipodgames.rock"
LAUNCHER_PATH = "/.rockbox/rocks/games/rockboy_launcher.rock"
LOG_PATH = ".rockbox/ipodgames/ipodgames-sim.log"
COVERFLOW_LOG_PATH = ".rockbox/games/ipodgames/coverflow-sim.log"
CARDINAL_POSITIONS = (0, 64, 128, 192)


def validate_coverflow_entry(simdisk: Path, metadata: Path) -> dict[str, str]:
    index = simdisk / ".rockbox/games/ipodgames/games.tsv"
    if not index.is_file():
        raise SystemExit(f"missing iPod Games cover-flow index: {index}")

    with index.open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    device_metadata = "/" + metadata.relative_to(simdisk).as_posix()
    matches = [row for row in rows if row.get("file") == device_metadata]
    if len(matches) != 1:
        raise SystemExit(
            f"expected one cover-flow row for {device_metadata}, found {len(matches)}"
        )
    row = matches[0]
    cover = row.get("cover", "")
    if not cover or not (simdisk / cover.lstrip("/")).is_file():
        raise SystemExit(f"cover-flow artwork is missing for {row.get('title', '')}")
    return row


def imported_eapp_report(metadata: Path) -> dict:
    fields: dict[str, str] = {}
    for line in metadata.read_text(encoding="utf-8").splitlines()[1:]:
        key, separator, value = line.partition("=")
        if separator:
            fields[key] = value
    executable = fields.get("local_executable", "")
    if not executable:
        raise SystemExit(f"metadata has no local executable: {metadata}")
    return inspect_eapp(metadata.parent / executable)


def install_plugins(build_dir: Path, simdisk: Path) -> None:
    sources = {
        build_dir / "apps/plugins/ipodgames/ipodgames.rock": PLUGIN_PATH,
        build_dir / "apps/plugins/rockboy_launcher.rock": LAUNCHER_PATH,
    }
    for source, device_path in sources.items():
        if not source.is_file():
            raise SystemExit(f"missing simulator plugin: {source}")
        destination = simdisk / device_path.lstrip("/")
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def run_plugin(
    build_dir: Path,
    simdisk: Path,
    plugin: str,
    parameter: str | None,
    extra_environment: dict[str, str],
    timeout: int = 30,
) -> str:
    environment = os.environ.copy()
    environment.update(
        {
            "RBROOT": str(simdisk),
            "ROCKBOX_SIM_PLUGIN": plugin,
            "ROCKBOX_SIM_PLUGIN_EXIT": "1",
            "SDL_VIDEODRIVER": "dummy",
            "SDL_AUDIODRIVER": "dummy",
            "ROCKPOD_SIM_HIDDEN": "1",
        }
    )
    if parameter:
        environment["ROCKBOX_SIM_PLUGIN_PARAM"] = parameter
    environment.update(extra_environment)
    result = subprocess.run(
        [
            str(build_dir / "rockboxui"),
            "--nobackground",
            "--root",
            str(simdisk),
            "--zoom",
            "1",
        ],
        cwd=build_dir,
        env=environment,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
        check=False,
    )
    return result.stdout


def validate_cardinal_joystick(simdisk: Path) -> bool:
    """Require the retail joystick graphic to point right/up/left/down."""
    centroids: dict[int, tuple[float, float]] = {}
    for position in CARDINAL_POSITIONS:
        path = (
            simdisk / ".rockbox/ipodgames" /
            f"ipodgames-wheel-cardinal-{position}.ppm"
        )
        if not path.is_file():
            return False
        header, pixels = path.read_bytes().split(b"\n255\n", 1)
        width, height = map(int, header.splitlines()[1].split())
        if (width, height) != (320, 240):
            return False
        points: list[tuple[int, int]] = []
        for y in range(178, 224):
            for x in range(242, 284):
                offset = (y * width + x) * 3
                red, green, blue = pixels[offset:offset + 3]
                if red > 140 and green < 100 and blue < 100:
                    points.append((x, y))
        if not points:
            return False
        centroids[position] = (
            sum(x for x, _ in points) / len(points),
            sum(y for _, y in points) / len(points),
        )

    center_x = 261.5
    center_y = 201.5
    passed = (
        centroids[0][0] > center_x + 10
        and abs(centroids[0][1] - center_y) < 4
        and centroids[64][1] < center_y - 10
        and abs(centroids[64][0] - center_x) < 4
        and centroids[128][0] < center_x - 10
        and abs(centroids[128][1] - center_y) < 4
        and centroids[192][1] > center_y + 10
        and abs(centroids[192][0] - center_x) < 4
    )
    print(
        "Cardinal joystick: "
        + ", ".join(
            f"{position}=({centroids[position][0]:.1f},"
            f"{centroids[position][1]:.1f})"
            for position in CARDINAL_POSITIONS
        )
        + (" / pass" if passed else " / FAIL")
    )
    return passed


def largest_colored_actor(
    path: Path, color: bytes, minimum_pixels: int
) -> tuple[float, float] | None:
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    width, height = map(int, header.splitlines()[1].split())
    if (width, height) != (320, 240):
        return None
    remaining = {
        (x, y)
        for y in range(20, 220)
        for x in range(206)
        if pixels[(y * width + x) * 3:(y * width + x + 1) * 3]
        == color
    }
    components: list[list[tuple[int, int]]] = []
    while remaining:
        pending = [remaining.pop()]
        component: list[tuple[int, int]] = []
        while pending:
            point = pending.pop()
            component.append(point)
            x, y = point
            for neighbor in ((x - 1, y), (x + 1, y),
                             (x, y - 1), (x, y + 1)):
                if neighbor in remaining:
                    remaining.remove(neighbor)
                    pending.append(neighbor)
        components.append(component)
    actor = max(components, key=len, default=[])
    if len(actor) < minimum_pixels:
        return None
    return (
        sum(x for x, _ in actor) / len(actor),
        sum(y for _, y in actor) / len(actor),
    )


def largest_yellow_actor(path: Path) -> tuple[float, float] | None:
    # The fully-open mouth frame separates the bow/face into a 24-pixel
    # component.  Pellets are at most five connected yellow pixels.
    return largest_colored_actor(path, b"\xff\xff\x00", 20)


def validate_actor_continuity(simdisk: Path) -> bool:
    paths = sorted(
        (simdisk / ".rockbox/ipodgames").glob(
            "ipodgames-continuity-*.ppm"
        ),
        key=lambda path: int(path.stem.rsplit("-", 1)[1]),
    )
    positions = [(path, largest_yellow_actor(path)) for path in paths]
    detected = [(path, position) for path, position in positions
                if position is not None]
    steps = [
        (current[0].name, math.dist(previous[1], current[1]))
        for previous, current in zip(detected, detected[1:])
        if int(current[0].stem.rsplit("-", 1)[1]) ==
        int(previous[0].stem.rsplit("-", 1)[1]) + 1
    ]
    missing = len(paths) - len(detected)
    maximum = max((distance for _, distance in steps), default=math.inf)
    moved = sum(distance >= 0.25 for _, distance in steps)
    worst = max(steps, key=lambda item: item[1], default=("none", math.inf))
    ghost_results = []
    for name, color, minimum_moving in (
        ("pink", b"\xff\xbe\xde", 50),
        ("cyan", b"\x00\xff\xde", 20),
    ):
        ghost_positions = [
            (path, largest_colored_actor(path, color, 20))
            for path in paths
        ]
        ghost_steps = [
            math.dist(previous[1], current[1])
            for previous, current in zip(
                ghost_positions, ghost_positions[1:]
            )
            if previous[1] is not None and current[1] is not None
        ]
        ghost_missing = sum(
            position is None for _, position in ghost_positions
        )
        ghost_maximum = max(ghost_steps, default=math.inf)
        ghost_moving = sum(distance >= 0.25 for distance in ghost_steps)
        ghost_results.append(
            (name, ghost_missing, ghost_maximum, ghost_moving,
             ghost_missing == 0 and len(ghost_steps) == 150
             and ghost_maximum <= 4.0
             and ghost_moving >= minimum_moving)
        )
    passed = (
        len(paths) == 151
        and missing == 0
        and len(steps) == 150
        and maximum <= 4.0
        and moved >= 50
        and all(result[4] for result in ghost_results)
    )
    print(
        "Ms. Pac-Man consecutive motion: "
        f"frames={len(paths)}, detected={len(detected)}, "
        f"adjacent={len(steps)}, moving={moved}, "
        f"max_step={maximum:.2f} at {worst[0]} / "
        + ("pass" if passed else "FAIL")
    )
    print(
        "Ghost consecutive motion: "
        + ", ".join(
            f"{name}=missing:{missing}/max:{maximum:.2f}/moving:{moving}"
            for name, missing, maximum, moving, _ in ghost_results
        )
        + (" / pass" if all(result[4] for result in ghost_results)
           else " / FAIL")
    )
    return passed


def ppm_pixels(path: Path) -> bytes:
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    if header.splitlines()[:2] != [b"P6", b"320 240"]:
        raise SystemExit(f"unexpected simulator frame format: {path}")
    if len(pixels) != 320 * 240 * 3:
        raise SystemExit(f"truncated simulator frame: {path}")
    return pixels


def validate_visual_reference(optimized: bytes, reference: bytes) -> bool:
    changed_pixels = 0
    squared_error = 0
    maximum_delta = 0
    for offset in range(0, len(optimized), 3):
        pixel_changed = False
        for channel in range(3):
            delta = abs(optimized[offset + channel] -
                        reference[offset + channel])
            pixel_changed = pixel_changed or delta != 0
            squared_error += delta * delta
            maximum_delta = max(maximum_delta, delta)
        changed_pixels += pixel_changed
    changed_fraction = changed_pixels / (320 * 240)
    mse = squared_error / len(optimized)
    psnr = math.inf if mse == 0 else 10 * math.log10(255 * 255 / mse)

    def edge_unmatched(source: bytes, target: bytes) -> int:
        unmatched = 0
        for y in range(240):
            for x in range(320):
                source_offset = (y * 320 + x) * 3
                matched = False
                for target_y in range(max(0, y - 1), min(240, y + 2)):
                    for target_x in range(max(0, x - 1), min(320, x + 2)):
                        target_offset = (target_y * 320 + target_x) * 3
                        if max(
                            abs(source[source_offset + channel] -
                                target[target_offset + channel])
                            for channel in range(3)
                        ) <= 32:
                            matched = True
                            break
                    if matched:
                        break
                unmatched += not matched
        return unmatched

    edge_unmatched_pixels = max(
        edge_unmatched(optimized, reference),
        edge_unmatched(reference, optimized),
    )
    edge_unmatched_fraction = edge_unmatched_pixels / (320 * 240)
    passed = (
        changed_fraction <= 0.15
        and edge_unmatched_fraction <= 0.005
    )
    print(
        "Visual reference: "
        f"changed={changed_pixels}/76800 ({changed_fraction:.2%}), "
        f"one-pixel-unmatched={edge_unmatched_pixels}/76800 "
        f"({edge_unmatched_fraction:.2%}), PSNR={psnr:.1f} dB, "
        f"max-channel-delta={maximum_delta} / "
        + ("pass" if passed else "FAIL")
    )
    return passed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir", type=Path, default=Path("build-sim-ipod6g")
    )
    parser.add_argument(
        "--frames", type=int, default=120,
        help="number of eApp event frames to execute (default: 120)",
    )
    parser.add_argument(
        "--metadata",
        type=Path,
        default=Path(
            "build-sim-ipod6g/simdisk/.rockbox/"
            "ipodgames/games/14004/game.igame"
        ),
    )
    parser.add_argument(
        "--wheel-sweep", action="store_true",
        help="sweep all 96 physical wheel positions on the name-entry screen",
    )
    parser.add_argument(
        "--capture-menus", action="store_true",
        help="dump key menu-transition frames for stock visual comparison",
    )
    parser.add_argument(
        "--options-path", action="store_true",
        help="exercise the stock main-menu volume control path",
    )
    parser.add_argument(
        "--pause-path", action="store_true",
        help="exercise the stock gameplay pause/continue path",
    )
    parser.add_argument(
        "--motion-continuity", action="store_true",
        help="capture and validate 151 adjacent fixed-input gameplay frames",
    )
    parser.add_argument(
        "--save-exit-path", action="store_true",
        help="exercise the stock pause-menu Save & Exit path",
    )
    parser.add_argument(
        "--generic", action="store_true",
        help="run the shared lifecycle/render gate without Ms. PAC-MAN probes",
    )
    parser.add_argument(
        "--hardware-path", action="store_true",
        help="use the batched interpreter path used by physical targets",
    )
    parser.add_argument(
        "--visual-reference", action="store_true",
        help="compare the optimized framebuffer with the exact rasterizer",
    )
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    simdisk = (build_dir / "simdisk").resolve()
    metadata = args.metadata.resolve()
    if not metadata.is_relative_to(simdisk):
        raise SystemExit("metadata must be inside the selected simulator disk")
    if not metadata.is_file():
        raise SystemExit(f"missing imported game metadata: {metadata}")

    install_plugins(build_dir, simdisk)
    row = validate_coverflow_entry(simdisk, metadata)
    eapp_report = imported_eapp_report(metadata)
    log_path = simdisk / LOG_PATH
    coverflow_log_path = simdisk / COVERFLOW_LOG_PATH
    log_path.unlink(missing_ok=True)
    coverflow_log_path.unlink(missing_ok=True)
    for path in (simdisk / ".rockbox/ipodgames").glob(
        "ipodgames-continuity-*.ppm"
    ):
        path.unlink()

    output = run_plugin(
        build_dir,
        simdisk,
        LAUNCHER_PATH,
        None,
        {"ROCKBOY_LAUNCHER_TEST_IPODGAMES": "1"},
    )
    if not coverflow_log_path.is_file():
        print(output, end="")
        raise SystemExit("Game Cover Flow produced no iPod Games gate log")
    coverflow_log = coverflow_log_path.read_text(
        encoding="utf-8", errors="replace"
    )
    print(coverflow_log, end="")
    if "status=pass\n" not in coverflow_log:
        return 1

    started = time.monotonic()
    test_environment = {
        "IPODGAMES_TEST": "1",
        "IPODGAMES_TEST_FRAMES": str(args.frames),
        **({"IPODGAMES_TEST_GENERIC": "1"} if args.generic else {}),
        **({"IPODGAMES_TEST_WHEEL_SWEEP": "1"}
           if args.wheel_sweep else {}),
        **({"IPODGAMES_TEST_CAPTURE_MENUS": "1"}
           if args.capture_menus else {}),
        **({"IPODGAMES_TEST_OPTIONS_PATH": "1"}
           if args.options_path else {}),
        **({"IPODGAMES_TEST_PAUSE_PATH": "1"}
           if args.pause_path else {}),
        **({"IPODGAMES_TEST_MOTION_CONTINUITY": "1"}
           if args.motion_continuity else {}),
        **({"IPODGAMES_TEST_SAVE_EXIT_PATH": "1"}
           if args.save_exit_path else {}),
        **({"IPODGAMES_SIM_HARDWARE_EXEC": "1"}
           if args.hardware_path else {}),
    }
    output = run_plugin(
        build_dir,
        simdisk,
        PLUGIN_PATH,
        "/" + metadata.relative_to(simdisk).as_posix(),
        test_environment,
        timeout=max(60 if args.wheel_sweep else 30,
                    (args.frames + (129 if args.wheel_sweep else 0)) // 3),
    )
    elapsed = time.monotonic() - started
    if not log_path.is_file():
        print(output, end="")
        raise SystemExit("iPod Games plugin produced no simulator gate log")
    log = log_path.read_text(encoding="utf-8", errors="replace")
    print(f"Cover Flow: iPod Games / {row['title']} / {row['cover']}")
    print(log, end="")
    log_fields = dict(
        line.split("=", 1)
        for line in log.splitlines()
        if "=" in line
    )
    completed_frames = int(log_fields.get("vm_frames", "0"), 0)
    visual_reference_pass = True
    if args.visual_reference:
        frame_path = simdisk / ".rockbox/ipodgames/ipodgames-frame.ppm"
        optimized_pixels = ppm_pixels(frame_path)
        shutil.copyfile(
            frame_path,
            frame_path.with_name("ipodgames-frame-optimized.ppm"),
        )
        reference_output = run_plugin(
            build_dir,
            simdisk,
            PLUGIN_PATH,
            "/" + metadata.relative_to(simdisk).as_posix(),
            {**test_environment, "IPODGAMES_SIM_REFERENCE_RASTER": "1"},
            timeout=max(60, args.frames),
        )
        if not frame_path.is_file():
            print(reference_output, end="")
            raise SystemExit("exact rasterizer produced no reference frame")
        shutil.copyfile(
            frame_path,
            frame_path.with_name("ipodgames-frame-reference.ppm"),
        )
        visual_reference_pass = validate_visual_reference(
            optimized_pixels, ppm_pixels(frame_path)
        )
    hardware_budget_pass = True
    if args.hardware_path:
        runtime_path = simdisk / ".rockbox/ipodgames/ipodgames-runtime.log"
        runtime_fields = dict(
            line.split("=", 1)
            for line in runtime_path.read_text(
                encoding="utf-8", errors="replace"
            ).splitlines()
            if "=" in line
        )
        instructions = int(runtime_fields.get("instructions", "0"), 0)
        work = [
            int(value, 0)
            for value in runtime_fields.get(
                "render_work", "0:0:0:0"
            ).split(":")
        ]
        opaque, blended, _skipped, general = work
        modeled_us = (
            instructions * 1.0
            + opaque * 0.06
            + blended * 0.25
            + general * 1.18
        )
        modeled_frame_us = modeled_us / max(completed_frames, 1)
        modeled_fps = 1_000_000 / max(modeled_frame_us, 1)
        hardware_budget_pass = modeled_fps >= 30.0
        print(
            "Hardware-path performance: "
            f"host={completed_frames / max(elapsed, 0.001):.1f} fps, "
            f"modeled-ipod6g={modeled_fps:.1f} fps, "
            f"frame-budget={modeled_frame_us:.0f} us / "
            + ("pass" if hardware_budget_pass else "FAIL")
        )
    cardinal_joystick_pass = (
        not args.wheel_sweep or validate_cardinal_joystick(simdisk)
    )
    actor_continuity_pass = (
        not args.motion_continuity or validate_actor_continuity(simdisk)
    )
    passed = (
        "status=pass\n" in log
        and "probe=eApp header valid\n" in log
        and "vm=lifecycle completed\n" in log
        and (
            (not args.save_exit_path and completed_frames == args.frames)
            or (args.save_exit_path and 0 < completed_frames < args.frames)
        )
        and "vm_fault=0\n" in log
        and f"frameworks={eapp_report['framework_count']}\n" in log
        and f"imports={eapp_report['total_import_slots']}\n" in log
        and f"vm_imports={eapp_report['total_import_slots']}\n" in log
        and int(log_fields.get("vm_draw_calls", "0"), 0) > 0
        and int(log_fields.get("vm_presented_frames", "0"), 0) > 0
        and int(log_fields.get("vm_nonblack_pixels", "0"), 0) > 0
        and int(log_fields.get("vm_framebuffer_checksum", "0"), 0) != 0
        and hardware_budget_pass
        and visual_reference_pass
        and (
            args.generic
            or log_fields.get("vm_audio_mixer", "0:0").split(":")[1]
            == "1"
        )
        and (
            not args.generic
            or log_fields.get("vm_generic_gate") == "1"
        )
        and log_fields.get("vm_armemu_signed_byte") == "1"
        and (
            not args.save_exit_path
            or log_fields.get("vm_save_exit") == "1"
        )
        and actor_continuity_pass
        and (
            not args.options_path
            or (
                int(log_fields.get("vm_audio_ordinal_53", "0:")
                    .split(":", 1)[0]) > 0
                and log_fields.get("vm_component_selection", "0/0")
                    .split("/")[0] == "1"
            )
        )
        and (
            not args.pause_path
            or (
                int(log_fields.get("vm_audio_ordinal_3", "0:")
                    .split(":", 1)[0]) > 0
                and int(log_fields.get("vm_audio_ordinal_4", "0:")
                    .split(":", 1)[0]) > 0
            )
        )
        and int(log_fields.get("vm_system", "0:0:0:0:0").split(":")[0]) > 0
        and (
            args.generic
            or int(log_fields.get("vm_system", "0:0:0:0:0")
                   .split(":")[3]) > 0
        )
        and (
            args.generic
            or args.frames < 1000
            or all(
                int(value) > 0
                for value in log_fields.get("vm_system", "0:0:0:0:0")
                .split(":")[1:3]
            )
        )
        and (
            args.generic
            or log_fields.get("vm_save_writes", "0:0:1:0").split(":")
            == ["2", "92", "0", "1"]
        )
        and (
            not args.wheel_sweep
            or (
                cardinal_joystick_pass
                and log_fields.get("vm_wheel_mapping", "0:0:0")
                .split(":")[0:2] == ["1", "129"]
                and int(
                    log_fields.get("vm_wheel_mapping", "0:0:0")
                    .split(":")[2]
                ) >= 26
            )
        )
        and (
            args.generic
            or args.frames < 1000
            or (
                int(log_fields.get("vm_texture_subimages", "0").split(":")[0])
                > 0
                and int(
                    log_fields.get("vm_texture_subimages", "0:0:1")
                    .split(":")[2]
                ) == 0
            )
        )
    )
    if not passed:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
