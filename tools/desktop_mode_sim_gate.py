#!/usr/bin/env python3
"""Launch the explicit Desktop Mode simulator session and verify its frame."""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "rockpod/.snow_leopard_desktop/packs/ipod-320x240"
PLUGIN_PATH = "/.rockbox/rocks/apps/desktop_mode.rock"
SESSION_TOKEN = "simulator-desktop"
DCP750_WIDTH = 720
DCP750_HEIGHT = 480
DCP750_VIEWPORT = (36, 24, 648, 432)


def _mask_channel(value: int, mask: int) -> int:
    if mask == 0:
        return 0
    shift = (mask & -mask).bit_length() - 1
    maximum = mask >> shift
    return ((value & mask) >> shift) * 255 // maximum


def bmp_pixel(path: Path, x: int, y: int) -> tuple[int, int, int]:
    data = path.read_bytes()
    if data[:2] != b"BM" or len(data) < 54:
        raise ValueError("frame is not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<I", data, 18)[0]
    signed_height = struct.unpack_from("<i", data, 22)[0]
    bits = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    height = abs(signed_height)
    if not (0 <= x < width and 0 <= y < height):
        raise ValueError("pixel is outside frame")
    stride = ((width * bits + 31) // 32) * 4
    file_y = height - 1 - y if signed_height > 0 else y
    index = offset + file_y * stride + x * (bits // 8)
    if bits == 16:
        value = struct.unpack_from("<H", data, index)[0]
        if compression == 3 and len(data) >= 66:
            red_mask, green_mask, blue_mask = struct.unpack_from("<III", data, 54)
        else:
            red_mask, green_mask, blue_mask = 0x7C00, 0x03E0, 0x001F
        return (
            _mask_channel(value, red_mask),
            _mask_channel(value, green_mask),
            _mask_channel(value, blue_mask),
        )
    if bits in (24, 32):
        blue, green, red = data[index : index + 3]
        return red, green, blue
    raise ValueError(f"unsupported BMP depth {bits}")


def near(left: tuple[int, int, int], right: tuple[int, int, int]) -> bool:
    return all(abs(a - b) <= 12 for a, b in zip(left, right))


def bmp_rgb(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if data[:2] != b"BM" or len(data) < 54:
        raise ValueError("frame is not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<I", data, 18)[0]
    signed_height = struct.unpack_from("<i", data, 22)[0]
    bits = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    height = abs(signed_height)
    if bits not in (16, 24, 32):
        raise ValueError(f"unsupported BMP depth {bits}")
    stride = ((width * bits + 31) // 32) * 4
    if offset + stride * height > len(data):
        raise ValueError("incomplete BMP frame")
    if bits == 16 and compression == 3 and len(data) >= 66:
        red_mask, green_mask, blue_mask = struct.unpack_from("<III", data, 54)
    else:
        red_mask, green_mask, blue_mask = 0x7C00, 0x03E0, 0x001F
    pixels: list[tuple[int, int, int]] = []
    for y in range(height):
        file_y = height - 1 - y if signed_height > 0 else y
        row = offset + file_y * stride
        for x in range(width):
            index = row + x * (bits // 8)
            if bits == 16:
                value = struct.unpack_from("<H", data, index)[0]
                pixels.append(
                    (
                        _mask_channel(value, red_mask),
                        _mask_channel(value, green_mask),
                        _mask_channel(value, blue_mask),
                    )
                )
            else:
                blue, green, red = data[index : index + 3]
                pixels.append((red, green, blue))
    return width, height, pixels


def different_pixels(left: Path, right: Path, rect: tuple[int, int, int, int]) -> int:
    left_width, left_height, left_pixels = bmp_rgb(left)
    right_width, right_height, right_pixels = bmp_rgb(right)
    if (left_width, left_height) != (right_width, right_height):
        raise ValueError("frame dimensions changed")
    x, y, width, height = rect
    changed = 0
    for row in range(y, min(y + height, left_height)):
        for column in range(x, min(x + width, left_width)):
            index = row * left_width + column
            if not near(left_pixels[index], right_pixels[index]):
                changed += 1
    return changed


def write_pointer(path: Path, x: int, y: int, buttons: int = 0) -> None:
    record = f"{x:04d} {y:04d} {buttons:02d}\n".encode("ascii")
    if len(record) != 13:
        raise ValueError("host pointer record must be exactly 13 bytes")
    temporary = path.with_suffix(".new")
    temporary.write_bytes(record)
    os.replace(temporary, path)


def write_dcp750_projection(source: Path, destination: Path) -> None:
    """Make a test-only view of the VP's qualified NTSC destination."""
    source_width, source_height, source_pixels = bmp_rgb(source)
    if (source_width, source_height) != (320, 240):
        raise ValueError("Desktop Mode source frame is not 320x240")
    viewport_x, viewport_y, viewport_width, viewport_height = DCP750_VIEWPORT
    stride = ((DCP750_WIDTH * 24 + 31) // 32) * 4
    raster = bytearray(stride * DCP750_HEIGHT)
    for output_y in range(viewport_height):
        source_y = output_y * source_height // viewport_height
        file_y = DCP750_HEIGHT - 1 - (viewport_y + output_y)
        row = file_y * stride
        for output_x in range(viewport_width):
            source_x = output_x * source_width // viewport_width
            red, green, blue = source_pixels[source_y * source_width + source_x]
            index = row + (viewport_x + output_x) * 3
            raster[index : index + 3] = bytes((blue, green, red))
    pixel_offset = 54
    file_size = pixel_offset + len(raster)
    header = bytearray(pixel_offset)
    header[:2] = b"BM"
    struct.pack_into("<I", header, 2, file_size)
    struct.pack_into("<I", header, 10, pixel_offset)
    struct.pack_into("<I", header, 14, 40)
    struct.pack_into("<i", header, 18, DCP750_WIDTH)
    struct.pack_into("<i", header, 22, DCP750_HEIGHT)
    struct.pack_into("<H", header, 26, 1)
    struct.pack_into("<H", header, 28, 24)
    struct.pack_into("<I", header, 34, len(raster))
    destination.write_bytes(header + raster)


def wait_for_frame(
    capture: Path,
    destination: Path,
    predicate,
    deadline: float,
    description: str,
) -> None:
    while time.monotonic() < deadline:
        try:
            shutil.copy2(capture, destination)
            bmp_rgb(destination)
            if predicate(destination):
                return
        except (OSError, ValueError, struct.error):
            pass
        time.sleep(0.05)
    raise SystemExit(f"timed out waiting for {description}")


def stage(build: Path, destination: Path, game_dir: Path | None = None) -> None:
    plugin = build / "apps/plugins/desktop_mode.rock"
    if not (build / "rockboxui").is_file() or not plugin.is_file():
        raise SystemExit(f"incomplete Desktop Mode simulator build: {build}")
    if not (PACK / "manifest.json").is_file():
        raise SystemExit(f"verified private Desktop Mode pack is missing: {PACK}")
    destination.mkdir(parents=True, exist_ok=True)
    plugin_target = destination / PLUGIN_PATH.lstrip("/")
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin, plugin_target)
    shutil.copytree(
        PACK,
        destination / ".rockbox/rocks/apps/desktop_mode_snow_leopard",
    )
    notification_font_source = ROOT / "assets/ipodjs/rockbox"
    notification_font_target = destination / ".rockbox/ipodjs"
    notification_font_target.mkdir(parents=True, exist_ok=True)
    for font_name in (
        "12-Adobe-Helvetica.fnt",
        "14-Adobe-Helvetica-Bold.fnt",
    ):
        shutil.copy2(
            notification_font_source / font_name,
            notification_font_target / font_name,
        )
    external = (
        (ROOT / "assets/ipodjs/rockbox/sitekick/desktop",
         destination / ".rockbox/sitekick/desktop"),
        (ROOT / "assets/ipodjs/rockbox/steam/desktop",
         destination / ".rockbox/ipodjs/steam/desktop"),
        (ROOT / "assets/ipodjs/rockbox/netflix/desktop",
         destination / ".rockbox/ipodjs/netflix/desktop"),
    )
    for source, target in external:
        shutil.copytree(source, target)
    if game_dir is not None:
        for plugin_name in ("steam_desktop.rock", "scummvm.rock"):
            source = build / "apps/plugins" / plugin_name
            if not source.is_file():
                raise SystemExit(f"focused simulator plugin missing: {source}")
            shutil.copy2(source, destination / ".rockbox/rocks/apps" / plugin_name)
        logo = ROOT / "assets/ipodjs/rockbox/steam/steam-logo-official.110x32x24.bmp"
        logo_target = destination / ".rockbox/ipodjs/steam" / logo.name
        logo_target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(logo, logo_target)
        scummvm = destination / "ScummVM"
        scummvm.mkdir(parents=True, exist_ok=True)
        (scummvm / "nibiru").symlink_to(game_dir, target_is_directory=True)
        (scummvm / "nibiru.scummvm").write_text(
            "gameid=nibiru\nengine=agds\npath=/ScummVM/nibiru\n"
            "savepath=/.rockbox/scummvm/saves/nibiru\n",
            encoding="ascii",
        )


def run_gate(
    build: Path, output: Path, timeout: float, game_dir: Path | None = None
) -> None:
    output.mkdir(parents=True, exist_ok=True)
    capture = output / ".desktop-mode-live.bmp"
    final_capture = output / "desktop-mode-frame.bmp"
    pointer_capture = output / "desktop-mode-pointer-frame.bmp"
    dashboard_capture = output / "desktop-mode-dashboard-frame.bmp"
    launchpad_capture = output / "desktop-mode-launchpad-frame.bmp"
    dcp750_capture = output / "desktop-mode-dcp750-720x480.bmp"
    steam_capture = output / "desktop-mode-steam-nibiru-frame.bmp"
    log_path = output / "desktop-mode-simulator.log"
    aurora = PACK / "320x240/desktop/aurora.320x240x16.bmp"
    points = ((8, 30), (8, 90), (60, 70), (110, 150))
    expected = {point: bmp_pixel(aurora, *point) for point in points}

    with tempfile.TemporaryDirectory(prefix="desktop-mode-sim-") as temporary:
        sim_root = Path(temporary) / "simdisk"
        stage(build, sim_root, game_dir)
        pointer = sim_root / ".rockbox/host-pointer"
        write_pointer(pointer, 120, 100)
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_VIDEODRIVER": "dummy",
                "SDL_AUDIODRIVER": "dummy",
                "ROCKPOD_SIM_HIDDEN": "1",
                "ROCKPOD_SIM_PREVIEW_BMP": str(capture),
                "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "40",
                "ROCKPOD_SIM_HOST_POINTER": str(pointer),
                "ROCKBOX_SIM_PLUGIN": PLUGIN_PATH,
                "ROCKBOX_SIM_PLUGIN_PARAM": SESSION_TOKEN,
            }
        )
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
        deadline = time.monotonic() + timeout
        try:
            def desktop_ready(frame: Path) -> bool:
                if process.poll() is not None:
                    raise SystemExit(
                        f"Desktop simulator exited early ({process.returncode}); "
                        f"see {log_path}"
                    )
                actual = {point: bmp_pixel(frame, *point) for point in points}
                return all(near(actual[point], expected[point]) for point in points)

            wait_for_frame(
                capture, final_capture, desktop_ready, deadline, "Desktop frame"
            )

            # The plugin starts with its centered cursor and the first host
            # pointer record is intentionally consumed after the desktop
            # baseline. Use that stable default as the old repaint region;
            # subsequent host-pointer samples still exercise direct motion.
            write_pointer(pointer, 160, 120)
            wait_for_frame(
                capture, final_capture,
                lambda frame: different_pixels(aurora, frame,
                                                (156, 116, 22, 28)) > 8,
                time.monotonic() + timeout, "baseline host pointer",
            )

            # The Dock shelf is 288 pixels wide at x=16. Nothing from the
            # fixed eight-slot Dock may paint into the side gutters.
            for x in (4, 12, 307, 315):
                if not near(bmp_pixel(final_capture, x, 230), bmp_pixel(aurora, x, 230)):
                    raise SystemExit("Desktop Dock escaped its 288-pixel shelf")
            if near(bmp_pixel(final_capture, 160, 230), bmp_pixel(aurora, 160, 230)):
                raise SystemExit("Desktop Dock shelf was not rendered")

            # Move the real host pointer and require both the old and new
            # cursor regions to repaint. This proves simulator coordinates
            # feed the same 320x240 hit-testing path as the wheel cursor.
            write_pointer(pointer, 80, 100)
            wait_for_frame(
                capture,
                pointer_capture,
                lambda frame: (
                    different_pixels(final_capture, frame, (156, 116, 22, 28)) > 8
                    and different_pixels(final_capture, frame, (76, 96, 22, 28)) > 8
                ),
                time.monotonic() + timeout,
                "direct host pointer movement",
            )

            # Slot 5 is Dashboard in the bounded stock Dock. Click it through
            # the simulator pointer record and require the real widget layer.
            write_pointer(pointer, 211, 214)
            time.sleep(0.12)
            write_pointer(pointer, 211, 214, 1)
            time.sleep(0.08)
            write_pointer(pointer, 211, 214, 0)

            def dashboard_ready(frame: Path) -> bool:
                # Dashboard owns the work area below the 21-row menu bar and
                # deliberately does not draw a Dock or a window frame.
                return different_pixels(
                    pointer_capture, frame, (0, 21, 320, 177)
                ) > 5000

            wait_for_frame(
                capture,
                dashboard_capture,
                dashboard_ready,
                time.monotonic() + timeout,
                "Dashboard widgets",
            )
            write_dcp750_projection(dashboard_capture, dcp750_capture)
            if bmp_rgb(dcp750_capture)[:2] != (DCP750_WIDTH, DCP750_HEIGHT):
                raise SystemExit("DCP750 projection has the wrong raster")
            for point in ((0, 0), (35, 23), (684, 456), (719, 479)):
                if bmp_pixel(dcp750_capture, *point) != (0, 0, 0):
                    raise SystemExit("DCP750 projection escaped the qualified viewport")
            if not near(
                bmp_pixel(dcp750_capture, 36, 24),
                bmp_pixel(dashboard_capture, 0, 0),
            ):
                raise SystemExit("DCP750 viewport does not map the 320x240 source")

            # Launchpad remains available from an application's View menu.
            # Dashboard's measured menu starts at x=158 in the native source;
            # row 2 is the Launchpad command at y=66..85.
            write_pointer(pointer, 175, 10)
            time.sleep(0.12)
            write_pointer(pointer, 175, 10, 1)
            time.sleep(0.08)
            write_pointer(pointer, 175, 10, 0)
            time.sleep(0.08)
            write_pointer(pointer, 180, 75)
            time.sleep(0.08)
            write_pointer(pointer, 180, 75, 1)
            time.sleep(0.08)
            write_pointer(pointer, 180, 75, 0)
            window_plain = PACK / "320x240/chrome/window-plain.304x174x16.bmp"

            def launchpad_ready(frame: Path) -> bool:
                # The compact Launchpad uses the same 304x174 chrome at the
                # exact native bounds (8,24), with all grid items inside it.
                chrome_points = ((8, 24, 0, 0), (311, 24, 303, 0),
                                 (8, 197, 0, 173), (311, 197, 303, 173))
                if not all(
                    near(bmp_pixel(frame, x, y), bmp_pixel(window_plain, sx, sy))
                    for x, y, sx, sy in chrome_points
                ):
                    return False
                return different_pixels(
                    dashboard_capture, frame, (8, 24, 304, 174)
                ) > 5000

            wait_for_frame(
                capture,
                launchpad_capture,
                launchpad_ready,
                time.monotonic() + timeout,
                "bounded Launchpad window",
            )
            if game_dir is not None:
                # NiBiRu is Launchpad item 8 (row 2, column 0).  Enter the
                # desktop-only Steam handoff and require its one-entry retail
                # library marker plus a materially repainted Aqua window.
                write_pointer(pointer, 40, 160)
                time.sleep(0.12)
                write_pointer(pointer, 40, 160, 1)
                time.sleep(0.08)
                write_pointer(pointer, 40, 160, 0)

                def steam_ready(frame: Path) -> bool:
                    text = log_path.read_text(
                        encoding="utf-8", errors="replace"
                    )
                    return (
                        "steam desktop: NiBiRu-only library count=1" in text
                        and different_pixels(
                            launchpad_capture, frame, (8, 24, 304, 174)
                        ) > 4000
                    )

                wait_for_frame(
                    capture, steam_capture, steam_ready,
                    time.monotonic() + timeout,
                    "desktop-only Steam NiBiRu library",
                )
            print(
                "Desktop Mode simulator gate passed: "
                f"{dashboard_capture}, {launchpad_capture} "
                f"(DCP750 proof: {dcp750_capture})"
                + (f"; NiBiRu-only Steam: {steam_capture}" if game_dir else "")
            )
            return
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build",
        type=Path,
        default=ROOT / "build-sim-ipod6g",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("/tmp/desktop-mode-sim-gate"),
    )
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument(
        "--game-dir", type=Path,
        help="private NiBiRu folder; enables the desktop-only Steam gate",
    )
    args = parser.parse_args()
    run_gate(
        args.build.resolve(), args.output.resolve(), args.timeout,
        args.game_dir.resolve() if args.game_dir else None,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
