#!/usr/bin/env python3
"""Exercise the windowed Sitekick app launched from Desktop Mode."""

from __future__ import annotations

import os
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from livetv_guide_sim_gate import capture, changed_pixels, pixel, prepare_root
from rockachievements_ui_sim_gate import (
    BUTTON_GATES,
    start_screen_lang_id,
    tap,
    wait_for_file,
    window_id,
    write_cstring,
)
from rockboy_profile_gate import (
    OPEN_PLUGIN_CHECKSUM,
    OPEN_PLUGIN_ENTRY_SIZE,
    OPEN_PLUGIN_NAME_OFFSET,
    OPEN_PLUGIN_NAME_SIZE,
    OPEN_PLUGIN_PARAM_OFFSET,
    OPEN_PLUGIN_PARAM_SIZE,
    OPEN_PLUGIN_PATH_OFFSET,
    OPEN_PLUGIN_PATH_SIZE,
    START_SCREEN_HASH,
    open_plugin_lang_checksum,
)


PLUGIN_PATH = "/.rockbox/rocks/apps/desktop_mode.rock"
DOCK_POINT = (1050, 1028)


def write_pointer(
    path: Path, buttons: int, point: tuple[int, int] = DOCK_POINT
) -> None:
    temporary = path.with_suffix(".new")
    temporary.write_text(
        f"{point[0]:04d} {point[1]:04d} {buttons:02d}\n",
        encoding="ascii",
    )
    os.replace(temporary, path)


def install(repo: Path, build: Path, root: Path) -> None:
    for name in ("desktop_mode", "sitekick"):
        source = build / f"apps/plugins/{name}.rock"
        target = root / f".rockbox/rocks/apps/{name}.rock"
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    pack = repo / "rockpod/.snow_leopard_desktop/packs/ipod-320x240"
    for relative in (
        ".rockbox/rocks/apps/desktop_mode_snow_leopard",
        ".rockbox/rocks.data/desktop_mode_snow_leopard",
    ):
        shutil.copytree(pack, root / relative)
    shutil.copytree(
        repo / "assets/ipodjs/rockbox/sitekick",
        root / ".rockbox/sitekick",
    )
    shutil.copytree(
        repo / "assets/ipodjs/rockbox/netflix",
        root / ".rockbox/ipodjs/netflix",
    )

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build)
    struct.pack_into(
        "<IiI",
        entry,
        0,
        START_SCREEN_HASH,
        start_screen_lang_id(build),
        checksum or OPEN_PLUGIN_CHECKSUM,
    )
    write_cstring(
        entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE,
        "desktop_mode.rock",
    )
    write_cstring(
        entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH
    )
    write_cstring(
        entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, ""
    )
    (root / ".rockbox/rocks/plugin.dat").write_bytes(entry)
    (root / ".rockbox/config.cfg").write_text(
        "start in screen: plugin\nresume: off\ntagcache_autoupdate: off\n",
        encoding="utf-8",
    )


def wait_for_sitekick(frame: Path, timeout: float = 12.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        sample = pixel(frame, 820, 700)
        if sample and sample[1] > sample[0] + 40:
            return
        time.sleep(0.2)
    capture(frame, Path("/tmp/sitekick-desktop-mode-gate/sitekick-timeout.png"))
    raise SystemExit("the windowed Sitekick app never appeared")


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    build = repo / "build-sim-desktop1080"
    simulator = build / "rockboxui"
    output = Path("/tmp/sitekick-desktop-mode-gate")
    output.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="sitekick-desktop-") as temporary:
        root = Path(temporary)
        prepare_root(repo, build, root)
        install(repo, build, root)
        pointer = root / ".rockbox/host-pointer"
        frame = root / "frame.bmp"
        write_pointer(pointer, 0)

        gate_root = output / ".button-gates"
        shutil.rmtree(gate_root, ignore_errors=True)
        gate_root.mkdir()
        BUTTON_GATES.clear()
        BUTTON_GATES.update(
            {
                "KP_5": gate_root / "select.gate",
                "KP_Decimal": gate_root / "menu.gate",
            }
        )
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_AUDIODRIVER": "disk",
                "SDL_DISKAUDIOFILE": str(output / "audio.raw"),
                "SDL_VIDEODRIVER": "x11",
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
                "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
                "ROCKPOD_SIM_SELECT_GATE": str(BUTTON_GATES["KP_5"]),
                "ROCKPOD_SIM_MENU_GATE": str(BUTTON_GATES["KP_Decimal"]),
            }
        )
        with (output / "simulator.log").open("wb") as log:
            process = subprocess.Popen(
                [
                    str(simulator),
                    "--zoom", "1",
                    "--nobackground",
                    "--root", str(root),
                ],
                cwd=build,
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        try:
            window_id(process.pid)
            wait_for_file(frame)
            time.sleep(2.0)
            write_pointer(pointer, 1)
            time.sleep(0.15)
            write_pointer(pointer, 0)
            wait_for_sitekick(frame)
            time.sleep(1.0)
            main_screen = output / "sitekick-main.png"
            capture(frame, main_screen)

            outside = pixel(main_screen, 100, 100)
            if not outside or max(outside) < 16:
                raise SystemExit(
                    "Sitekick blacked out the Desktop outside its window"
                )

            chips_tab = (780, 796)
            write_pointer(pointer, 0, chips_tab)
            time.sleep(0.2)
            write_pointer(pointer, 1, chips_tab)
            time.sleep(0.2)
            write_pointer(pointer, 0, chips_tab)
            time.sleep(1.0)
            slot = output / "sitekick-chip-screen.png"
            capture(frame, slot)
            if changed_pixels(main_screen, slot) < 1000:
                raise SystemExit("Sitekick did not open the CHIPS screen")

            # Catalogue item 11 is the starter-owned wearable Chip 0012.
            chip = (670, 493)
            equip_slot = (620, 330)
            write_pointer(pointer, 0, chip)
            time.sleep(0.2)
            write_pointer(pointer, 1, chip)
            time.sleep(0.3)
            write_pointer(pointer, 1, equip_slot)
            time.sleep(0.4)
            dragging = output / "sitekick-chip-dragging.png"
            capture(frame, dragging)
            write_pointer(pointer, 0, equip_slot)
            time.sleep(1.0)
            equipped = output / "sitekick-chip-equipped.png"
            capture(frame, equipped)
            if changed_pixels(slot, equipped) < 100:
                raise SystemExit("mouse drag did not equip the chip")
            save = root / ".rockbox/sitekick/state/save.v1.dat"
            payload = save.read_bytes()
            if len(payload) < 18 or struct.unpack_from("<H", payload, 16)[0] != 12:
                raise SystemExit("mouse drop did not persist Chip 0012")

            tap(process.pid, "KP_Decimal")
            time.sleep(0.8)
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

    print(f"Sitekick Desktop Mode gate passed; captures in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
