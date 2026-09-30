#!/usr/bin/env python3
"""Launch the Rockbox AGDS bootstrap against synthetic NiBiRu-shaped data."""

from __future__ import annotations

import argparse
import os
import json
import re
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
AGDS_KEY = b"Vyvojovy tym AGDS varuje: Hackerovani skodi obchodu!"
AGDS_SIGNATURE = b"AGDS group file\x1a"
BUTTON_GATES: dict[str, Path] = {}


def encrypt_agds(data: bytes) -> bytes:
    return bytes(
        value ^ (0xFF ^ AGDS_KEY[index % len(AGDS_KEY)])
        for index, value in enumerate(data)
    )


def synthetic_bmp(width: int = 1024, height: int = 768) -> bytes:
    stride = ((width * 3 + 3) // 4) * 4
    pixels = bytearray(stride * height)
    colors = (
        (0, 0, 255),      # top-left red, stored BGR
        (0, 255, 0),      # top-right green
        (255, 0, 0),      # bottom-left blue
        (0, 255, 255),    # bottom-right yellow
    )
    for file_y in range(height):
        screen_y = height - 1 - file_y
        bottom = screen_y >= height // 2
        for x in range(width):
            right = x >= width // 2
            blue, green, red = colors[(2 if bottom else 0) + right]
            offset = file_y * stride + x * 3
            pixels[offset : offset + 3] = bytes((blue, green, red))
    image_offset = 54
    file_size = image_offset + len(pixels)
    header = struct.pack(
        "<2sIHHIIIIHHIIIIII",
        b"BM",
        file_size,
        0,
        0,
        image_offset,
        40,
        width,
        height,
        1,
        24,
        0,
        len(pixels),
        2835,
        2835,
        0,
        0,
    )
    return header + pixels


def synthetic_pcx(width: int = 1024, height: int = 768) -> bytes:
    header = bytearray(128)
    header[0:4] = bytes((0x0A, 5, 1, 8))
    struct.pack_into("<HHHH", header, 4, 0, 0, width - 1, height - 1)
    struct.pack_into("<HH", header, 12, width, height)
    header[64] = 0
    header[65] = 1
    struct.pack_into("<H", header, 66, width)
    struct.pack_into("<H", header, 68, 1)
    encoded = bytearray()
    for y in range(height):
        left = 3 if y >= height // 2 else 1
        right = 4 if y >= height // 2 else 2
        for value, count in ((left, width // 2), (right, width - width // 2)):
            while count:
                run = min(63, count)
                encoded.extend((0xC0 | run, value))
                count -= run
    palette = bytearray(768)
    palette[3:6] = bytes((255, 0, 0))
    palette[6:9] = bytes((0, 255, 0))
    palette[9:12] = bytes((0, 0, 255))
    palette[12:15] = bytes((255, 255, 0))
    return bytes(header + encoded + b"\x0c" + palette)


def synthetic_adb(startup_name: str) -> bytes:
    name_size = 31
    enter = struct.pack("<H", 4445) + struct.pack(
        "<HHHHHHHH", 0xDEAD, 12, 1, 0, 0, 0, 0, 0
    )
    main = struct.pack("<HHHB", 1, 0, len(enter), 1) + enter
    plain = startup_name.encode("ascii")
    startup = encrypt_agds(plain) + b"\x00"
    entries = ((b"main", main), (b"main.1008.p", startup))
    header = struct.pack("<IIIII", 666, 1, len(entries), len(entries), name_size)
    records = bytearray()
    data = bytearray()
    for name, payload in entries:
        records.extend(struct.pack("<I", len(data)))
        records.extend(name.ljust(name_size + 1, b"\x00"))
        records.extend(struct.pack("<I", len(payload)))
        data.extend(payload)
    return header + records + data


def synthetic_grp(member_name: str, payload: bytes) -> bytes:
    encoded_name = member_name.encode("ascii")
    if len(encoded_name) >= 0x21:
        raise ValueError("synthetic member name is too long")
    data_offset = 0x2C + 0x31
    header = (
        encrypt_agds(AGDS_SIGNATURE)
        + struct.pack("<IIIIIII", 44, 0x1A03C9E6, 2, 1, 0, 0, 0)
    )
    name = encrypt_agds(encoded_name) + b"\x00"
    name = name.ljust(0x21, b"\x00")
    record = name + struct.pack("<II", data_offset, len(payload)) + b"\x00" * 8
    return header + record + payload


def bmp_data_pixel(data: bytes, x: int, y: int) -> tuple[int, int, int]:
    if data[:2] != b"BM" or len(data) < 54:
        raise ValueError("capture is not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<I", data, 18)[0]
    signed_height = struct.unpack_from("<i", data, 22)[0]
    bits = struct.unpack_from("<H", data, 28)[0]
    if bits not in (24, 32):
        raise ValueError(f"unsupported capture depth {bits}")
    height = abs(signed_height)
    if not (0 <= x < width and 0 <= y < height):
        raise ValueError("capture coordinate is outside the frame")
    stride = ((width * bits + 31) // 32) * 4
    file_y = height - 1 - y if signed_height > 0 else y
    index = offset + file_y * stride + x * (bits // 8)
    blue, green, red = data[index : index + 3]
    return red, green, blue


def bmp_pixel(path: Path, x: int, y: int) -> tuple[int, int, int]:
    return bmp_data_pixel(path.read_bytes(), x, y)


def near(actual: tuple[int, int, int], expected: tuple[int, int, int]) -> bool:
    return all(abs(left - right) <= 24 for left, right in zip(actual, expected))


def composed_bmp_pixel(
    layers: list[tuple[bytes, int, int, bool]], x: int, y: int
) -> tuple[int, int, int]:
    """Sample the runtime's 1024x768 canvas composition at an iPod pixel."""
    canvas_x = x * 1024 // 320
    canvas_y = y * 768 // 240
    result = (0, 0, 0)
    for payload, layer_x, layer_y, transparent in layers:
        if payload[:2] != b"BM" or len(payload) < 54:
            raise ValueError("retail composition gate requires BMP layers")
        width = struct.unpack_from("<I", payload, 18)[0]
        height = abs(struct.unpack_from("<i", payload, 22)[0])
        source_x = canvas_x - layer_x
        source_y = canvas_y - layer_y
        if not (0 <= source_x < width and 0 <= source_y < height):
            continue
        color = bmp_data_pixel(payload, source_x, source_y)
        if not transparent or color != (255, 0, 255):
            result = color
    return result


def composed_expectations(
    layers: list[tuple[bytes, int, int, bool]],
    *,
    compare_layers: list[tuple[bytes, int, int, bool]] | None = None,
    avoid: tuple[int, int] | None = None,
    max_y: int = 236,
) -> dict[tuple[int, int], tuple[int, int, int]]:
    """Pick visible, spatially separated pixels from a retail composition."""
    candidates = []
    for y in range(4, max_y, 2):
        for x in range(4, 316, 2):
            if avoid and (x - avoid[0]) ** 2 + (y - avoid[1]) ** 2 < 12 ** 2:
                continue
            color = composed_bmp_pixel(layers, x, y)
            if compare_layers is not None:
                previous = composed_bmp_pixel(compare_layers, x, y)
                difference = sum(abs(a - b) for a, b in zip(color, previous))
                if difference < 72:
                    continue
                score = difference + sum(color)
            else:
                if sum(color) < 120:
                    continue
                score = sum(color)
            candidates.append((score, x, y, color))

    selected: dict[tuple[int, int], tuple[int, int, int]] = {}
    spacing = 5 if compare_layers is not None else 22
    for _, x, y, color in sorted(candidates, reverse=True):
        if all(
            (x - old_x) ** 2 + (y - old_y) ** 2 >= spacing ** 2
            for old_x, old_y in selected
        ):
            selected[(x, y)] = color
            if len(selected) == 8:
                break
    minimum = 3 if compare_layers is not None else 6
    if len(selected) < minimum:
        raise ValueError("retail composition has too few validation points")
    return selected


def simulator_window(pid: int) -> str:
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        result = subprocess.run(
            ["xdotool", "search", "--pid", str(pid)],
            check=False,
            capture_output=True,
            text=True,
        )
        windows = result.stdout.splitlines()
        if windows:
            window = windows[-1]
            subprocess.run(
                ["xdotool", "windowactivate", window], check=False
            )
            time.sleep(0.2)
            return window
        time.sleep(0.1)
    raise SystemExit("could not find the NiBiRu simulator window")


def tap_simulator(window: str | None, key: str, repeat: int = 1) -> None:
    gate = BUTTON_GATES.get(key)
    if gate is not None:
        for _ in range(repeat):
            gate.touch()
            time.sleep(0.08)
            gate.unlink(missing_ok=True)
            time.sleep(0.08)
        time.sleep(0.19)
        return
    if window is None:
        raise SystemExit(f"headless simulator has no button gate for {key}")
    result = subprocess.run(
        [
            "xdotool",
            "key",
            "--window",
            window,
            "--repeat",
            str(repeat),
            "--delay",
            "70",
            key,
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode:
        # SDL can replace its X11 child window while changing fullscreen or
        # focus state. The simulator was activated immediately before this
        # call, so retry against the active window instead of retaining a
        # now-invalid child id.
        subprocess.run(
            [
                "xdotool",
                "key",
                "--repeat",
                str(repeat),
                "--delay",
                "70",
                key,
            ],
            check=True,
        )
    time.sleep(0.35)


def tap_simulator_chord(first: str, second: str) -> None:
    first_gate = BUTTON_GATES[first]
    second_gate = BUTTON_GATES[second]

    first_gate.touch()
    second_gate.touch()
    time.sleep(0.16)
    first_gate.unlink(missing_ok=True)
    second_gate.unlink(missing_ok=True)
    time.sleep(0.35)


def real_adb_text(game_dir: Path, wanted: str) -> str:
    blob = (game_dir / "data.adb").read_bytes()
    if len(blob) < 20:
        raise ValueError("data.adb: truncated header")
    magic, _, total, used, name_size = struct.unpack_from("<5I", blob)
    if magic != 666 or used > total or not 0 < name_size <= 255:
        raise ValueError("data.adb: invalid header")
    record_size = name_size + 9
    data_offset = 20 + total * record_size
    for index in range(used):
        record = blob[20 + index * record_size : 20 + (index + 1) * record_size]
        name = record[4 : 5 + name_size].rstrip(b"\0").decode("ascii")
        if name != wanted:
            continue
        offset = struct.unpack_from("<I", record)[0]
        size = struct.unpack_from("<I", record, name_size + 5)[0]
        payload = blob[data_offset + offset : data_offset + offset + size].rstrip(b"\0")
        if payload and (payload[0] < 32 or payload[0] >= 127):
            payload = encrypt_agds(payload)
        return payload.decode("ascii")
    raise ValueError(f"data.adb: missing text {wanted}")


def real_picture(game_dir: Path, wanted: str | None = None) -> tuple[str, bytes]:
    config = (game_dir / "agds.cfg").read_text(
        encoding="ascii", errors="strict"
    )
    archives = [
        line.split("=", 1)[1].strip()
        for line in config.splitlines()
        if line.strip().lower().startswith("path=")
    ]
    for archive in archives:
        blob = (game_dir / archive).read_bytes()
        if len(blob) < 0x2C:
            raise ValueError(f"{archive}: truncated GRP header")
        encrypted = blob[:16] != AGDS_SIGNATURE
        signature = encrypt_agds(blob[:16]) if encrypted else blob[:16]
        if signature != AGDS_SIGNATURE:
            raise ValueError(f"{archive}: invalid GRP signature")
        count = struct.unpack_from("<I", blob, 0x1C)[0]
        for index in range(count):
            record = blob[0x2C + index * 0x31 : 0x2C + (index + 1) * 0x31]
            if len(record) != 0x31:
                raise ValueError(f"{archive}: truncated GRP record")
            end = record[:0x21].find(b"\0")
            if end < 0:
                raise ValueError(f"{archive}: unterminated GRP member")
            encoded = record[:end]
            name_data = encrypt_agds(encoded) if encrypted else encoded
            name = name_data.decode("ascii")
            offset, size = struct.unpack_from("<II", record, 0x21)
            if offset + size > len(blob):
                raise ValueError(f"{archive}: GRP member outside file")
            if (wanted is not None and name.lower() == wanted.lower()) or (
                wanted is None and name.lower().endswith((".bmp", ".pcx"))
            ):
                return name, blob[offset : offset + size]
    raise ValueError(f"configured GRP archives lack {wanted or 'a BMP/PCX'}")


def scaled_bmp_expectations(
    payload: bytes, canvas_x: int = 0, canvas_y: int = 0
) -> dict[tuple[int, int], tuple[int, int, int]]:
    if payload[:2] != b"BM" or len(payload) < 54:
        raise ValueError("real-data gate currently requires a first BMP member")
    width = struct.unpack_from("<I", payload, 18)[0]
    height = abs(struct.unpack_from("<i", payload, 22)[0])
    candidates = []
    for y in range(8, 232, 4):
        for x in range(8, 312, 4):
            if 148 <= x <= 172 and 108 <= y <= 132:
                continue
            source_x = x * 1024 // 320 - canvas_x
            source_y = y * 768 // 240 - canvas_y
            if not (0 <= source_x < width and 0 <= source_y < height):
                continue
            color = bmp_data_pixel(
                payload,
                source_x,
                source_y,
            )
            candidates.append((sum(color), x, y, color))
    selected: dict[tuple[int, int], tuple[int, int, int]] = {}
    for brightness, x, y, color in sorted(candidates, reverse=True):
        if brightness < 96:
            break
        if all((x - px) ** 2 + (y - py) ** 2 >= 18 ** 2 for px, py in selected):
            selected[(x, y)] = color
            if len(selected) == 8:
                break
    if len(selected) < 4:
        raise ValueError("startup BMP has too few visible validation points")
    return selected


def direct_scene_expectations(
    path: Path,
    room_layers: list[tuple[bytes, int, int, bool]] | None = None,
) -> dict[tuple[int, int], tuple[int, int, int]]:
    """Select model pixels stable across the opening seated animation."""
    data = path.read_bytes()
    if len(data) < 24 or data[:4] != b"NBS1":
        raise ValueError(f"{path}: invalid direct scene stream")
    version, width, height, frame_count, _, entry_size = struct.unpack_from(
        "<6H", data, 4
    )
    table_offset, _ = struct.unpack_from("<II", data, 16)
    if version not in (1, 2, 3) or (width, height) != (320, 240) or entry_size != 16:
        raise ValueError(f"{path}: unsupported direct scene stream")
    candidates = []
    first_frame = -1
    first_changed_pixels = 0
    for frame in range(frame_count):
        entry = table_offset + frame * entry_size
        if entry + 16 > len(data):
            raise ValueError(f"{path}: truncated scene table")
        offset, size, x, y, crop_width, crop_height = struct.unpack_from(
            "<IIHHHH", data, entry
        )
        if not size:
            continue
        pixels = crop_width * crop_height
        mask_size = (pixels + 7) // 8
        if offset + size > len(data) or size < mask_size:
            raise ValueError(f"{path}: invalid scene payload")
        mask = data[offset : offset + mask_size]
        colors = memoryview(data)[offset + mask_size : offset + size]
        color_offset = 0
        for pixel in range(pixels):
            if not (mask[pixel >> 3] & (1 << (pixel & 7))):
                continue
            if color_offset + 2 > len(colors):
                raise ValueError(f"{path}: truncated scene colors")
            rgb565 = struct.unpack_from("<H", colors, color_offset)[0]
            color_offset += 2
            red = ((rgb565 >> 11) & 31) * 255 // 31
            green = ((rgb565 >> 5) & 63) * 255 // 63
            blue = (rgb565 & 31) * 255 // 31
            screen_x = x + pixel % crop_width
            screen_y = y + pixel // crop_width
            if (screen_x - 160) ** 2 + (screen_y - 120) ** 2 < 14 ** 2:
                continue
            candidates.append((red + green + blue, screen_x, screen_y,
                               (red, green, blue)))
        if color_offset != len(colors):
            raise ValueError(f"{path}: overlong scene colors")
        first_frame = frame
        first_changed_pixels = color_offset // 2
        if (
            x != 0
            or y > 24
            or crop_width != width
            or y + crop_height < 218
            or first_changed_pixels < 40_000
        ):
            raise ValueError(
                f"{path}: first scene pose is not a self-contained room "
                f"keyframe ({x},{y} {crop_width}x{crop_height}, "
                f"{first_changed_pixels} pixels)"
            )
        break
    # The gate polls a 24 fps stream every 50 ms.  A face or hand pixel can
    # legitimately move between polls, so remove every first-pose pixel that
    # any later delta touches. Remaining candidates are immutable authored
    # room pixels and prove the complete stream stays composited over the
    # office rather than reverting to black.
    first_candidates = list(candidates)
    touched = set()
    early_touched = set()
    # V3 speaking clips begin with independent full keyframes and are
    # selected later by the script. Check stability across the opening clip.
    stable_frames = min(frame_count, 316) if version == 3 else frame_count
    for frame in range(first_frame + 1, stable_frames):
        if version == 3 and frame == 6:
            continue  # Independent first authored pose, identical to frame 0.
        entry = table_offset + frame * entry_size
        offset, size, x, y, crop_width, crop_height = struct.unpack_from(
            "<IIHHHH", data, entry
        )
        if not size:
            continue
        pixels = crop_width * crop_height
        mask_size = (pixels + 7) // 8
        if offset + size > len(data) or size < mask_size:
            raise ValueError(f"{path}: invalid stable-scene payload")
        mask = data[offset : offset + mask_size]
        for pixel in range(pixels):
            if mask[pixel >> 3] & (1 << (pixel & 7)):
                point = (x + pixel % crop_width, y + pixel // crop_width)
                touched.add(point)
                if frame < first_frame + 1 + 5 * 24:
                    early_touched.add(point)
    candidates = [
        candidate for candidate in candidates
        if (candidate[1], candidate[2]) not in touched
    ]
    selected: dict[tuple[int, int], tuple[int, int, int]] = {}
    # Spread validation across the office. Selecting only globally brightest
    # pixels clusters around the left desk lamp and lets a sparse Martin-only
    # delta over black masquerade as a complete room frame.
    for row in range(2):
        top = 18 + row * 100
        bottom = 118 + row * 100
        for column in range(4):
            left = column * 80
            right = left + 80
            sector = [
                candidate for candidate in candidates
                if left <= candidate[1] < right
                and top <= candidate[2] < bottom
            ]
            if not sector:
                raise ValueError(
                    f"{path}: no stable room pixel in sector {column},{row}"
                )
            _, x, y, color = max(sector)
            selected[(x, y)] = color
    if room_layers is not None:
        model_candidates = []
        for brightness, x, y, color in first_candidates:
            if not (110 <= x <= 190 and 80 <= y <= 180):
                continue
            if (x, y) in early_touched:
                continue
            room_color = composed_bmp_pixel(room_layers, x, y)
            difference = sum(
                abs(left - right) for left, right in zip(color, room_color)
            )
            if difference >= 72:
                model_candidates.append(
                    (difference + brightness, x, y, color)
                )
        model_selected = 0
        for _, x, y, color in sorted(model_candidates, reverse=True):
            if all(
                (x - old_x) ** 2 + (y - old_y) ** 2 >= 10 ** 2
                for old_x, old_y in selected
                if 110 <= old_x <= 190 and 80 <= old_y <= 180
            ):
                selected[(x, y)] = color
                model_selected += 1
                if model_selected == 4:
                    break
        if model_selected < 4:
            raise ValueError(f"{path}: too few stable seated Martin pixels")
    if len(selected) < 8:
        raise ValueError(f"{path}: incomplete direct scene room coverage")
    return selected


def stage(
    build: Path,
    destination: Path,
    image_format: str,
    game_dir: Path | None,
    expect_menu: bool,
    expect_selector: bool,
    expect_room: bool,
    expect_scene: bool,
) -> tuple[
    Path,
    dict[tuple[int, int], tuple[int, int, int]],
    dict[tuple[int, int], tuple[int, int, int]] | None,
    dict[tuple[int, int], tuple[int, int, int]] | None,
    dict[tuple[int, int], tuple[int, int, int]] | None,
    dict[tuple[int, int], tuple[int, int, int]] | None,
    dict[tuple[int, int], tuple[int, int, int]] | None,
    dict[tuple[int, int], tuple[int, int, int]] | None,
    str,
]:
    plugin = build / "apps/plugins/scummvm/scummvm.rock"
    if not (build / "rockboxui").is_file():
        raise SystemExit(f"incomplete simulator build: {build}")
    if not plugin.is_file():
        raise SystemExit(f"build the focused ScummVM plugin first: {plugin}")
    destination.mkdir(parents=True, exist_ok=True)
    plugin_target = destination / ".rockbox/rocks/apps/scummvm.rock"
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin, plugin_target)
    scummvm_dir = destination / "ScummVM"
    scummvm_dir.mkdir(parents=True, exist_ok=True)
    game = scummvm_dir / "nibiru"
    if game_dir is not None:
        if not (game_dir / "data.adb").is_file():
            raise SystemExit(f"not an installed NiBiRu folder: {game_dir}")
        game.symlink_to(game_dir, target_is_directory=True)
        text_entry = "1009.1011.1012.p" if expect_menu else "main.1008.p2"
        startup_name = real_adb_text(game_dir, text_entry)
        member_name, picture = real_picture(game_dir, startup_name)
        expected = scaled_bmp_expectations(
            picture,
            0 if expect_menu else 32,
            0 if expect_menu else 159,
        )
        mode = f"retail {'menu ' if expect_menu else ''}{member_name}"
        selector_expected = None
        room_expected = None
        scene_expected = None
        final_room_expected = None
        playable_room_expected = None
        destination_room_expected = None
        if expect_menu:
            # Native captions replace the unreadable text baked into the
            # retail bitmap, while the same retail regions handle clicks.
            expected = {
                point: color for point, color in expected.items()
                if not (24 <= point[0] < 89 and
                        (88 <= point[1] < 148 or 158 <= point[1] < 170))
            }
            expected.update({
                (27, 89): (255, 255, 255),   # New game under the pointer
                (27, 101): (239, 223, 165),  # Load game
                (28, 113): (239, 223, 165),  # Save game
                (28, 125): (239, 223, 165),  # Options
                (28, 137): (239, 223, 165),  # Credits
                (28, 159): (239, 223, 165),  # Quit
            })
        if expect_selector:
            selector_expected = dict(expected)
        if expect_room:
            backdrop_name = real_adb_text(game_dir, "1864.1012.p")
            foreground_name = real_adb_text(game_dir, "1864.1354.p")
            _, backdrop = real_picture(game_dir, backdrop_name)
            _, foreground = real_picture(game_dir, foreground_name)
            room_layers = [
                (backdrop, 0, 56, False),
                (foreground, 335, 511, True),
            ]
            room_expected = composed_expectations(
                room_layers,
                avoid=(44, 92),
            )
        if expect_scene:
            scene_expected = direct_scene_expectations(
                game_dir / "rockpod/intro1864.nbs", room_layers
            )
            final_backdrop_name = real_adb_text(
                game_dir, "10e0.1012.13c1"
            )
            final_foreground_name = real_adb_text(
                game_dir, "10e0.145e.13c1"
            )
            _, final_backdrop = real_picture(game_dir, final_backdrop_name)
            _, final_foreground = real_picture(game_dir, final_foreground_name)
            final_room_expected = composed_expectations(
                [
                    (final_backdrop, 0, 56, False),
                    (final_foreground, 441, 588, True),
                ],
                avoid=(44, 92),
            )
            playable_layers = []
            for entry, x, y, transparent in (
                ("10f9.1012.p", 0, 56, False),
                ("10f9.1353.p", 57, 286, True),
                ("10f9.1361.p", 336, 478, True),
                ("10f9.1362.p", 671, 475, True),
                ("10f9.1354.p", 425, 435, True),
                ("10f9.1355.p", 312, 390, True),
                ("10f9.1356.p", 609, 376, True),
                ("10f9.1357.p", 482, 304, True),
            ):
                picture_name = real_adb_text(game_dir, entry)
                _, payload = real_picture(game_dir, picture_name)
                playable_layers.append((payload, x, y, transparent))
            playable_room_expected = composed_expectations(
                playable_layers,
                avoid=(44, 92),
                max_y=190,
            )
            destination_layers = []
            for entry, x, y, transparent in (
                ("10e6.1012.p", 0, 56, False),
                ("10e6.17f3.p", 0, 376, True),
                ("10e6.199b.p", 789, 358, True),
            ):
                picture_name = real_adb_text(game_dir, entry)
                _, payload = real_picture(game_dir, picture_name)
                destination_layers.append((payload, x, y, transparent))
            destination_room_expected = composed_expectations(
                destination_layers,
                avoid=(44, 92),
                max_y=190,
            )
    else:
        game.mkdir(parents=True, exist_ok=True)
        if image_format == "pcx":
            member_name = "gate.pcx"
            picture = synthetic_pcx()
        else:
            member_name = "gate.bmp"
            picture = synthetic_bmp()
        (game / "data.adb").write_bytes(synthetic_adb(member_name))
        (game / "gfx1.grp").write_bytes(synthetic_grp(member_name, picture))
        (game / "agds.cfg").write_text(
            "AGDS.CFG\n\nvideomode=1024x768x32\npath=gfx1.grp\n",
            encoding="ascii",
        )
        expected = {
            (20, 20): (255, 0, 0),
            (300, 20): (0, 255, 0),
            (20, 220): (0, 0, 255),
            (300, 220): (255, 255, 0),
        }
        mode = image_format.upper()
        selector_expected = None
        room_expected = None
        scene_expected = None
        final_room_expected = None
        playable_room_expected = None
        destination_room_expected = None
    descriptor = scummvm_dir / "nibiru.scummvm"
    descriptor.write_text(
        "gameid=nibiru\nengine=agds\npath=/ScummVM/nibiru\n"
        "savepath=/.rockbox/scummvm/saves/nibiru\n",
        encoding="ascii",
    )
    return (descriptor, expected, selector_expected, room_expected,
            scene_expected, final_room_expected, playable_room_expected,
            destination_room_expected, mode)


def wait_for_expected_frame(
    process: subprocess.Popen,
    capture: Path,
    expected: dict[tuple[int, int], tuple[int, int, int]],
    deadline: float,
    log_path: Path,
    label: str,
    allowed_misses: int = 0,
) -> None:
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise SystemExit(
                f"simulator exited early ({process.returncode}); see {log_path}"
            )
        try:
            actual = {point: bmp_pixel(capture, *point) for point in expected}
        except (OSError, ValueError, struct.error):
            time.sleep(0.05)
            continue
        misses = sum(
            not near(actual[point], color)
            for point, color in expected.items()
        )
        if misses <= allowed_misses:
            return
        time.sleep(0.05)
    colors = {
        point: bmp_pixel(capture, *point)
        for point in expected
        if capture.is_file()
    }
    raise SystemExit(
        f"timed out waiting for {label}; colors={colors}; see {log_path}"
    )


def wait_for_log_markers(
    process: subprocess.Popen,
    log_path: Path,
    markers: tuple[str, ...],
    deadline: float,
) -> None:
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise SystemExit(
                f"simulator exited early ({process.returncode}); see {log_path}"
            )
        text = log_path.read_text(encoding="utf-8", errors="replace")
        if all(marker in text for marker in markers):
            return
        time.sleep(0.05)
    text = log_path.read_text(encoding="utf-8", errors="replace")
    missing = [marker for marker in markers if marker not in text]
    raise SystemExit("cutscene audio trace missing: " + ", ".join(missing))


def copy_live_capture(capture: Path, destination: Path, deadline: float) -> None:
    while time.monotonic() < deadline:
        try:
            shutil.copy2(capture, destination)
            return
        except FileNotFoundError:
            time.sleep(0.025)
    raise SystemExit(f"simulator did not publish {capture}")


def wait_for_progression_markers(
    process: subprocess.Popen,
    log_path: Path,
    markers: tuple[str, ...],
    deadline: float,
    window: str | None,
    auto_advance: bool,
    advanced_subtitles: int = 0,
    capture: Path | None = None,
    subtitle_capture: Path | None = None,
) -> int:
    if not auto_advance:
        wait_for_log_markers(process, log_path, markers, deadline)
        return advanced_subtitles
    progress_text = ""
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise SystemExit(
                "simulator exited during retail progression; "
                f"see {log_path}"
            )
        progress_text = log_path.read_text(
            encoding="utf-8", errors="replace"
        )
        if all(marker in progress_text for marker in markers):
            return advanced_subtitles
        subtitle_count = progress_text.count(" subtitle ready (")
        if subtitle_count > advanced_subtitles:
            if window is None and "KP_5" not in BUTTON_GATES:
                raise SystemExit(
                    "fast dialogue requires a simulator window or button gate"
                )
            # VM text timers now advance on elapsed ticks.  Let dialogue
            # finish at that pace; synthetic clicks can skip an entire short
            # transition room while the button helper is settling.
            if (
                advanced_subtitles == 0
                and capture is not None
                and subtitle_capture is not None
            ):
                time.sleep(0.04)
                copy_live_capture(capture, subtitle_capture, deadline)
            advanced_subtitles = subtitle_count
        time.sleep(0.05)
    missing = [marker for marker in markers if marker not in progress_text]
    raise SystemExit(
        "retail progression trace missing: " + ", ".join(missing)
    )


def run_gate(
    build: Path,
    output: Path,
    timeout: float,
    image_format: str,
    game_dir: Path | None,
    expect_menu: bool,
    expect_selector: bool,
    expect_room: bool,
    expect_inventory: bool,
    expect_film: bool,
    expect_scene: bool,
    expect_audio: bool,
    expect_scene_complete: bool,
    expect_retail_progression: bool,
    expect_save_roundtrip: bool,
    fast_intro: bool,
    headless: bool,
    expect_phone_timing: bool = False,
) -> None:
    output.mkdir(parents=True, exist_ok=True)
    capture = output / ".nibiru-live.bmp"
    final_capture = output / "nibiru-frame.bmp"
    log_path = output / "nibiru-simulator.log"
    with tempfile.TemporaryDirectory(prefix="nibiru-agds-sim-") as temporary:
        sim_root = Path(temporary) / "simdisk"
        gate_root = Path(temporary) / "button-gates"
        gate_root.mkdir()
        BUTTON_GATES.clear()
        BUTTON_GATES.update({
            "KP_5": gate_root / "select.gate",
            "Return": gate_root / "select.gate",
            "KP_Decimal": gate_root / "menu.gate",
            "KP_Add": gate_root / "play.gate",
            "Right": gate_root / "right.gate",
            "Left": gate_root / "left.gate",
            "Down": gate_root / "scroll-forward.gate",
            "Up": gate_root / "scroll-back.gate",
        })
        (_, expected, selector_expected, room_expected, scene_expected,
         final_room_expected, playable_room_expected,
         destination_room_expected, mode) = stage(
            build,
            sim_root,
            image_format,
            game_dir,
            expect_menu,
            expect_selector,
            expect_room,
            expect_scene,
        )
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_VIDEODRIVER": (
                    "dummy" if headless or not expect_selector else "x11"
                ),
                "SDL_AUDIODRIVER": os.environ.get("SDL_AUDIODRIVER", "dummy"),
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_HIDDEN": (
                    "1" if headless or not expect_selector else "0"
                ),
                "ROCKPOD_SIM_PREVIEW_BMP": str(capture),
                "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "40",
                "ROCKPOD_SIM_SELECT_GATE":
                    str(BUTTON_GATES["KP_5"]),
                "ROCKPOD_SIM_MENU_GATE":
                    str(BUTTON_GATES["KP_Decimal"]),
                "ROCKPOD_SIM_PLAY_ACTION_GATE":
                    str(BUTTON_GATES["KP_Add"]),
                "ROCKPOD_SIM_SCROLL_FWD_GATE":
                    str(BUTTON_GATES["Down"]),
                "ROCKPOD_SIM_SCROLL_BACK_GATE":
                    str(BUTTON_GATES["Up"]),
                "ROCKPOD_SIM_LEFT_GATE":
                    str(BUTTON_GATES["Left"]),
                "ROCKPOD_SIM_RIGHT_GATE":
                    str(BUTTON_GATES["Right"]),
                **(
                    {"ROCKPOD_AGDS_TEST_SPEED": "8"}
                    if fast_intro else {}
                ),
                "ROCKBOX_SIM_PLUGIN": "/.rockbox/rocks/apps/scummvm.rock",
                "ROCKBOX_SIM_PLUGIN_PARAM": "/ScummVM/nibiru.scummvm",
                "ROCKPOD_SCUMMVM_CURSOR_X": "40",
                "ROCKPOD_SCUMMVM_CURSOR_Y": "92",
                **(
                    {"ROCKPOD_AGDS_AUTOSTART": "1"}
                    if expect_scene and not expect_retail_progression
                    else {}
                ),
                **(
                    {"ROCKPOD_AGDS_SAVE_ROUNDTRIP": "1"}
                    if expect_save_roundtrip else {}
                ),
                **(
                    {"ROCKPOD_AGDS_FILM_TEST": "1"}
                    if expect_film else {}
                ),
                **(
                    {"ROCKPOD_AGDS_SCENE_REDRAW_TEST": "1"}
                    if expect_scene_complete else {}
                ),
            }
        )
        with log_path.open("wb") as log:
            process = subprocess.Popen(
                [
                    str(build / "rockboxui"),
                    "--nobackground",
                    # A missing named device exercises the existing clocked
                    # PCM sink without SDL dummy's callback pacing delay.
                    *(["--audiodev", "nibiru-clock-test"]
                      if os.environ.get("ROCKPOD_SIM_CLOCKED_AUDIO") == "1"
                      else []),
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
            if scene_expected is not None:
                if room_expected is None:
                    raise SystemExit("direct scene gate lacks room expectations")
                window = None
                if expect_retail_progression:
                    if selector_expected is None:
                        raise SystemExit(
                            "retail progression gate lacks menu expectations"
                        )
                    if not headless and shutil.which("xdotool") is None:
                        raise SystemExit(
                            "--expect-retail-progression requires xdotool"
                        )
                    window = None if headless else simulator_window(process.pid)
                    wait_for_expected_frame(
                        process, capture, selector_expected, deadline, log_path,
                        "retail New Game selector",
                    )
                    tap_simulator(window, "KP_5")
                # intro1864.nbs begins with a self-contained room+Martin
                # keyframe.  Once that stream is requested, validate the
                # frame that is actually presented instead of racing the
                # short-lived script-only backdrop immediately before it.
                wait_for_log_markers(
                    process,
                    log_path,
                    ("agds: direct scene first pose presented during room load",),
                    deadline,
                )
                wait_for_expected_frame(
                    process, capture, scene_expected, deadline, log_path,
                    "retail room 1864 plus seated Martin",
                    allowed_misses=1,
                )
                shutil.copy2(capture, output / "checkpoint-room1864.bmp")
                log_text = log_path.read_text(encoding="utf-8", errors="replace")
                script_markers = (
                    "agds: screen 1864 VM built",
                    "agds: direct scene first pose presented during room load",
                    "agds: retail light 0 type=0 position=0,40,70 "
                    "ambient=140a0a diffuse=fcd35a specular=ffffff",
                    "agds: script picture 1864.1012 1864.1012.p at 0,56",
                    "agds: script picture 1864.1354 1864.1354.p at 335,511",
                    "agds: sample 1Telefon_zvedne.wav queued "
                    "phase=1122.10e1.13e9 cycles=1",
                )
                if expect_retail_progression:
                    script_markers = (
                        "agds: inventory add inv.10bb slot=0 count=1",
                        "agds: inventory add inv.112c slot=1 count=1",
                    ) + script_markers
                if expect_save_roundtrip:
                    script_markers += (
                        "agds: simulator seeded retail save state inventory=2",
                        "agds: retail save requested slot 0",
                        "agds: saved slot 0 screen=1864",
                        "agds: simulator scrambled save state inventory=0",
                        "agds: retail load requested slot 0",
                        "agds: loaded slot 0 screen=1864",
                        "agds: restored screen 1864",
                        "agds: simulator retail save roundtrip passed "
                        "inventory=2",
                    )
                    wait_for_log_markers(
                        process, log_path, script_markers, deadline
                    )
                    log_text = log_path.read_text(
                        encoding="utf-8", errors="replace"
                    )
                missing_markers = [
                    marker for marker in script_markers if marker not in log_text
                ]
                if missing_markers:
                    raise SystemExit(
                        "script VM trace missing: " + ", ".join(missing_markers)
                    )
                if expect_audio:
                    wait_for_log_markers(
                        process,
                        log_path,
                        (
                            "agds: sample 1Telefon_zvoni.wav play "
                            "phase=1122.10e1.11a0 cycles=5",
                            "agds: sample martin_Intro.ogg queued "
                            "phase=1122.10e1.19fa cycles=1",
                        ),
                        deadline,
                    )
                if expect_film:
                    wait_for_log_markers(
                        process,
                        log_path,
                        (
                            "agds: film stream dolesa.mjpg",
                            "agds: film first JPEG decoded",
                        ),
                        deadline,
                    )
                    copy_live_capture(
                        capture, output / "checkpoint-film.bmp", deadline
                    )
                # Progression follows rooms as they appear.  Waiting for the
                # separate full-scene gate first can miss a short-lived room
                # now that VM timers follow elapsed time instead of frames.
                if expect_scene_complete and not expect_retail_progression:
                    wait_for_log_markers(
                        process,
                        log_path,
                        (
                            "agds: sample phase=1122.10e1.11a0 stop",
                            "agds: sample phase=1122.10e1.13e9 "
                            "restart playing=1",
                            "agds: sample phase=1122.10e1.19fa "
                            "restart playing=1",
                            "agds: direct scene complete (",
                            "agds: direct scene post-complete redraw preserved",
                        ),
                        deadline,
                    )
                if expect_phone_timing:
                    wait_for_log_markers(process, log_path,
                        ("agds: SetNextScreen -> 10bf",), deadline)
                    text = log_path.read_text(errors="replace")
                    text = text.split("agds: SetNextScreen -> 10bf", 1)[0]
                    events = re.findall(
                        r"agds: voice (begin (\S+) tick=(\d+) duration_ms=(\d+)|complete tick=(\d+))",
                        text)
                    lines = []
                    active = None
                    last_end = None
                    for _, name, start, duration, end in events:
                        if name:
                            if active is not None:
                                raise SystemExit("phone voices overlap")
                            active = (name, int(start), int(duration))
                            if last_end is not None and int(start) - last_end > 10:
                                raise SystemExit("phone dialogue has an artificial gap")
                        else:
                            if active is None:
                                raise SystemExit("voice completion without a line")
                            name, start, duration = active
                            elapsed = (int(end) - start) * 10
                            if not duration - 20 <= elapsed <= duration + 200:
                                raise SystemExit(f"voice duration mismatch: {name}: {elapsed}/{duration} ms")
                            lines.append({"sample": name, "source_ms": duration,
                                          "played_ms": elapsed})
                            active = None
                            last_end = int(end)
                    if active is not None or len(lines) != 15:
                        raise SystemExit(f"incomplete phone conversation: {len(lines)} lines")
                    if "instruction limit" in text or "room VM tick failed" in text:
                        raise SystemExit("phone scene VM failed")
                    (output / "phone-timing.json").write_text(
                        json.dumps({"lines": lines, "overlaps": 0}, indent=2) + "\n")
                    copy_live_capture(capture, final_capture, deadline)
                    print(f"NiBiRu natural phone timing passed: 15 complete voices, no overlap; {output}")
                    return
                if expect_retail_progression:
                    progression_markers = (
                        "agds: click handler 1009.1011.new at 128,294",
                        "agds: SetNextScreen -> 1864",
                        "agds: dialog 10de.10e1 loaded by 1864.100a",
                        "agds: SetNextScreen -> 10bf",
                        "agds: SetNextScreen -> 10e0",
                    )
                    advanced_subtitles = wait_for_progression_markers(
                        process, log_path, progression_markers, deadline,
                        window, fast_intro, capture=capture,
                        subtitle_capture=output / "checkpoint-subtitle.bmp",
                    )
                    if final_room_expected is None:
                        raise SystemExit(
                            "retail progression gate lacks room 10e0 expectations"
                        )
                    # At 8x, transient cutscene rooms can finish between
                    # captures; their script transitions are checked above.
                    if not fast_intro:
                        wait_for_expected_frame(
                            process, capture, final_room_expected, deadline,
                            log_path, "retail room 10e0 composition",
                        )
                        wait_for_log_markers(
                            process, log_path,
                            ("agds: character animation complete 1080",),
                            deadline,
                        )
                        shutil.copy2(capture, output / "checkpoint-room10e0.bmp")
                    playable_markers = (
                        "agds: SetNextScreen -> 10fb",
                        "agds: SetNextScreen -> 1352",
                        "agds: dialog complete for 1352.10c0",
                        "agds: SetNextScreen -> 10f9",
                    )
                    advanced_subtitles = wait_for_progression_markers(
                        process, log_path, playable_markers, deadline,
                        window, fast_intro, advanced_subtitles,
                    )
                    if playable_room_expected is None:
                        raise SystemExit(
                            "retail progression gate lacks room 10f9 expectations"
                        )
                    # At 8x, transient cutscene rooms can finish between
                    # captures; their script transitions are checked above.
                    if not fast_intro:
                        wait_for_expected_frame(
                            process, capture, playable_room_expected, deadline,
                            log_path, "retail room 10f9 composition",
                        )
                        shutil.copy2(capture, output / "checkpoint-room10f9.bmp")
                    final_markers = (
                        "agds: dialog complete for 10f9.10c0",
                        "agds: SetNextScreen -> 10e6",
                    )
                    advanced_subtitles = wait_for_progression_markers(
                        process, log_path, final_markers, deadline,
                        window, fast_intro, advanced_subtitles,
                    )
                    if destination_room_expected is None:
                        raise SystemExit(
                            "retail progression gate lacks room 10e6 expectations"
                        )
                    wait_for_expected_frame(
                        process, capture, destination_room_expected, deadline,
                        log_path, "retail room 10e6 composition",
                    )
                    shutil.copy2(capture, output / "checkpoint-room10e6.bmp")
                    destination_markers = (
                        "agds: retail light 0 type=0 position=50,20,80 "
                        "ambient=140a0a diffuse=92dcee specular=ffffff",
                        "agds: sample martin_mlg000205.ogg play ",
                        "agds: user enabled by 10e6.10c0 at ",
                    )
                    advanced_subtitles = wait_for_progression_markers(
                        process, log_path, destination_markers, deadline,
                        window, fast_intro, advanced_subtitles,
                    )
                    copy_live_capture(
                        capture, output / "checkpoint-room10e6-ready.bmp",
                        deadline,
                    )
                    if window is None and "KP_5" not in BUTTON_GATES:
                        raise SystemExit(
                            "retail room interaction requires a window or "
                            "button gate"
                        )
                    # From the menu cursor (40,92), target the retail
                    # 10e6.1334 hotspot center near iPod pixel (115,126).
                    tap_simulator(window, "Right", repeat=19)
                    tap_simulator(window, "Down", repeat=9)
                    tap_simulator(window, "KP_5")
                    interaction_markers = (
                        "agds: click handler 10e6.1334 at ",
                        "agds: npc subtitle ready (",
                        "voice=10e6.1334.t1.s",
                        "agds: user enabled by 10e6.1334 at ",
                    )
                    advanced_subtitles = wait_for_progression_markers(
                        process, log_path, interaction_markers, deadline,
                        window, fast_intro, advanced_subtitles,
                    )
                    if expect_inventory:
                        # Room 10e6 has restored user control. Enter the exact
                        # y=696 strip, move over the first authored item, then
                        # issue the iPod Play+Select right-click chord.
                        tap_simulator(window, "Down", repeat=24)
                        wait_for_log_markers(
                            process,
                            log_path,
                            ("agds: retail inventory page=0 items=2",),
                            deadline,
                        )
                        tap_simulator(window, "Left", repeat=16)
                        tap_simulator_chord("KP_5", "KP_Add")
                        wait_for_log_markers(
                            process,
                            log_path,
                            ("agds: inventory look handler inv.10bb",),
                            deadline,
                        )
                        shutil.copy2(
                            capture, output / "checkpoint-inventory.bmp"
                        )
                    log_text = log_path.read_text(
                        encoding="utf-8", errors="replace"
                    )
                    retail_motion_markers = (
                        "agds: loaded 5 retail motion clips, 118 samples "
                        "at 24 fps",
                        "agds: character 1080 begins retail move from "
                        "1020,763 to 684,608",
                        "agds: character 1080 retail locomotion clip "
                        "1093.1096 at 996,751 remaining=365",
                        "agds: character 1080 movement complete at "
                        "684,608 direction=103",
                        "agds: character 1080 begins retail leave from "
                        "684,608 to 379,559",
                        "agds: character 1080 movement complete at "
                        "379,559 direction=270",
                        "agds: character 1080 begins retail move from "
                        "715,462 to 546,504",
                        "agds: character 1080 movement complete at "
                        "546,504 direction=270",
                        "agds: character 1080 begins retail move from "
                        "546,504 to 399,503",
                        "agds: character 1080 retail locomotion clip "
                        "1093.1099 at 399,503 remaining=0",
                        "agds: character 1080 movement complete at "
                        "399,503 direction=135",
                    )
                    motion_position = -1
                    for marker in retail_motion_markers:
                        marker_position = log_text.find(
                            marker, motion_position + 1
                        )
                        if marker_position < 0:
                            raise SystemExit(
                                "retail root-motion trace missing or out of "
                                f"order: {marker}"
                            )
                        motion_position = marker_position
                    if (
                        "VM failed" in log_text
                        or (
                            " opcode " in log_text
                            and " pending at " in log_text
                        )
                    ):
                        raise SystemExit(
                            "retail progression VM reported a failure; see "
                            f"{log_path}"
                        )
            else:
                wait_for_expected_frame(
                    process, capture, expected, deadline, log_path,
                    f"scaled AGDS {mode} frame",
                )
            if selector_expected is not None and scene_expected is None:
                if not headless and shutil.which("xdotool") is None:
                    raise SystemExit("--expect-selector requires xdotool")
                window = None if headless else simulator_window(process.pid)
                wait_for_expected_frame(
                    process, capture, selector_expected, deadline, log_path,
                    "retail New Game selector",
                )
                if room_expected is not None:
                    tap_simulator(window, "KP_5")
                    wait_for_expected_frame(
                        process, capture, room_expected, deadline, log_path,
                        "retail room 1864 composition",
                    )
                    wait_for_log_markers(
                        process,
                        log_path,
                        (
                            "agds: click handler 1009.1011.new at 128,294",
                            "agds: inventory add inv.10bb slot=0 count=1",
                            "agds: inventory add inv.112c slot=1 count=1",
                            "agds: SetNextScreen -> 1864",
                        ),
                        deadline,
                    )
                    log_text = log_path.read_text(
                        encoding="utf-8", errors="replace"
                    )
                    if (
                        "VM failed" in log_text
                        or (
                            " opcode " in log_text
                            and " pending at " in log_text
                        )
                    ):
                        raise SystemExit(
                            "retail New Game VM reported a failure; see "
                            f"{log_path}"
                        )
            # The simulator publishes previews with an atomic
            # remove/rename pair.  A validated frame can briefly disappear
            # between those operations, so final capture must tolerate that
            # writer window rather than misreport a game failure.
            for attempt in range(40):
                try:
                    shutil.copy2(capture, final_capture)
                    break
                except FileNotFoundError:
                    if attempt == 39:
                        raise
                    time.sleep(0.025)
            suffix = " + selector" if selector_expected is not None else ""
            if room_expected is not None:
                suffix += " + room 1864"
            if expect_inventory:
                suffix += " + retail inventory"
            if expect_film:
                suffix += " + original MJPG film"
            if scene_expected is not None:
                suffix += " + direct scene"
            if expect_audio:
                suffix += " + script audio"
            if expect_scene_complete:
                suffix += " + complete timeline"
            if expect_retail_progression:
                suffix += " + retail progression and interaction in room 10e6"
            if fast_intro:
                suffix += " + accelerated simulator timeline"
            print(
                f"NiBiRu AGDS {mode}{suffix} simulator gate "
                f"passed: {final_capture}"
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
        default=Path("/tmp/nibiru-agds-sim-gate"),
    )
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument(
        "--image-format",
        choices=("bmp", "pcx"),
        default="bmp",
    )
    parser.add_argument(
        "--game-dir",
        type=Path,
        help="private installed NiBiRu folder; mounted read-only by symlink",
    )
    parser.add_argument(
        "--expect-menu",
        action="store_true",
        help="wait for the authentic retail main menu after both logos",
    )
    parser.add_argument(
        "--expect-selector",
        action="store_true",
        help="drive the cursor to and validate the retail New Game selector",
    )
    parser.add_argument(
        "--expect-room",
        action="store_true",
        help="click New Game and validate the static room 1864 composition",
    )
    parser.add_argument(
        "--expect-inventory",
        action="store_true",
        help="open and validate the owned retail bottom inventory assets",
    )
    parser.add_argument(
        "--expect-film",
        action="store_true",
        help="decode an original owned 24fps MJPG stream in the simulator",
    )
    parser.add_argument(
        "--expect-scene",
        action="store_true",
        help="validate a model-derived scene delta after entering room 1864",
    )
    parser.add_argument(
        "--expect-audio",
        action="store_true",
        help="wait for script-timed phone and voice sample events",
    )
    parser.add_argument(
        "--expect-scene-complete",
        action="store_true",
        help="run the complete 1,524-frame direct scene and process timeline",
    )
    parser.add_argument(
        "--expect-retail-progression",
        action="store_true",
        help=(
            "click retail New Game, run the intro/dialog chain, and require "
            "the original cinematics and a retail interaction in room 10e6"
        ),
    )
    parser.add_argument(
        "--fast-intro",
        action="store_true",
        help="run a complete authored scene timeline at 8x in the simulator",
    )
    parser.add_argument(
        "--expect-save-roundtrip",
        action="store_true",
        help=(
            "run the retail save/load opcodes and require inventory, scene, "
            "object, and character restoration"
        ),
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        help=(
            "drive Rockbox button gates under SDL dummy video instead of "
            "requiring an unlocked X11 desktop"
        ),
    )
    parser.add_argument("--expect-phone-timing", action="store_true",
                        help="check all 15 phone voices at normal speed with the clocked PCM sink")
    args = parser.parse_args()
    if args.expect_phone_timing:
        if args.fast_intro:
            parser.error("phone timing must run at normal speed")
        args.expect_retail_progression = True
        os.environ["ROCKPOD_SIM_CLOCKED_AUDIO"] = "1"
        if args.timeout == 20.0:
            args.timeout = 120.0
    if args.expect_inventory:
        args.expect_retail_progression = True
        args.fast_intro = True
        if args.timeout == 20.0:
            args.timeout = 300.0
    if args.expect_film:
        args.expect_scene = True
        if args.timeout == 20.0:
            args.timeout = 60.0
    if args.expect_retail_progression:
        args.expect_menu = True
        args.expect_selector = True
        args.expect_room = True
        args.expect_scene = True
        args.expect_scene_complete = True
    if args.fast_intro:
        if not args.expect_scene_complete:
            parser.error("--fast-intro requires --expect-scene-complete")
    if (args.expect_menu or args.expect_selector or args.expect_room or
            args.expect_scene) and not args.game_dir:
        parser.error("retail expectations require --game-dir")
    if args.expect_selector or (args.expect_room and not args.expect_scene):
        args.expect_menu = True
    if args.expect_room and not args.expect_scene:
        args.expect_selector = True
    if args.expect_scene:
        args.expect_room = True
    if args.expect_audio and not args.expect_scene:
        parser.error("--expect-audio requires --expect-scene")
    if args.expect_scene_complete and not args.expect_scene:
        parser.error("--expect-scene-complete requires --expect-scene")
    if args.expect_save_roundtrip and not args.expect_scene:
        parser.error("--expect-save-roundtrip requires --expect-scene")
    run_gate(
        args.build.resolve(), args.output.resolve(), args.timeout,
        args.image_format,
        args.game_dir.resolve() if args.game_dir else None,
        args.expect_menu,
        args.expect_selector,
        args.expect_room,
        args.expect_inventory,
        args.expect_film,
        args.expect_scene,
        args.expect_audio,
        args.expect_scene_complete,
        args.expect_retail_progression,
        args.expect_save_roundtrip,
        args.fast_intro,
        args.headless,
        args.expect_phone_timing,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
