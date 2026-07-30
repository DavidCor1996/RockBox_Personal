#!/usr/bin/env python3
"""Exercise the DIRECTV Dock app and windowed playback in both UI profiles."""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from livetv_guide_sim_gate import (
    LIVETV_ROOT,
    capture,
    changed_pixels,
    pixel,
    prepare_root,
    tuned_channel,
)
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
PROFILES = {
    "320": {
        "build": "build-sim-ipod6g",
        "dock": (194, 214),
        "video_crop": "196x146+10+50",
        "guide_probe": (12, 94),
        "desktop_probe": (2, 100),
        "output": "/tmp/livetv-desktop-mode-gate",
    },
    "1080": {
        "build": "build-sim-desktop1080",
        "dock": (990, 1028),
        "video_crop": "892x642+513+196",
        "guide_probe": (520, 240),
        "desktop_probe": (100, 100),
        "output": "/tmp/livetv-desktop-mode-gate-1080",
    },
}


def write_pointer(path: Path, buttons: int, dock: tuple[int, int]) -> None:
    temporary = path.with_suffix(".new")
    temporary.write_text(
        f"{dock[0]:04d} {dock[1]:04d} {buttons:02d}\n",
        encoding="ascii",
    )
    os.replace(temporary, path)


def install_desktop(repo: Path, build: Path, root: Path) -> None:
    plugins = ["desktop_mode", "sitekick"]
    if build.name == "build-sim-desktop1080":
        plugins.append("netflix_desktop")
    for name in plugins:
        source = build / f"apps/plugins/{name}.rock"
        target = root / f".rockbox/rocks/apps/{name}.rock"
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    pack = (
        repo
        / "rockpod/.snow_leopard_desktop/packs/ipod-320x240"
    )
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


def wait_for_guide(
    frame: Path, probe: tuple[int, int], timeout_capture: Path,
    timeout: float = 45.0
) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        sample = pixel(frame, *probe)
        if sample and all(
            abs(actual - expected) <= 30
            for actual, expected in zip(sample, (2, 111, 175))
        ):
            return
        time.sleep(0.2)
    capture(frame, timeout_capture)
    raise SystemExit("the windowed DIRECTV guide never appeared")


def wait_for_playback(
    frame: Path, probe: tuple[int, int], timeout: float = 20.0
) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        sample = pixel(frame, *probe)
        if sample and any(
            abs(actual - expected) > 45
            for actual, expected in zip(sample, (2, 111, 175))
        ):
            return
        time.sleep(0.2)
    raise SystemExit("SELECT did not leave the DIRECTV guide")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--profile", choices=sorted(PROFILES), default="320"
    )
    args = parser.parse_args()
    profile = PROFILES[args.profile]
    repo = Path(__file__).resolve().parent.parent
    build = (repo / profile["build"]).resolve()
    simulator = build / "rockboxui"
    output = Path(profile["output"])
    output.mkdir(parents=True, exist_ok=True)
    failures: list[str] = []

    with tempfile.TemporaryDirectory(prefix="livetv-desktop-") as temporary:
        root = Path(temporary)
        prepare_root(repo, build, root)
        install_desktop(repo, build, root)
        (root / LIVETV_ROOT / ".livetv_state").write_text(
            "100\n", encoding="ascii"
        )
        pointer = root / ".rockbox/host-pointer"
        write_pointer(pointer, 0, profile["dock"])
        frame = root / "frame.bmp"
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
                # Launch the test subject directly.  plugin.dat is retained
                # above to exercise the installed-device contract, but its
                # language checksum can legitimately vary between the 320
                # and desktop1080 build trees.
                "ROCKBOX_SIM_PLUGIN": PLUGIN_PATH,
            }
        )
        log = (output / "simulator.log").open("wb")
        try:
            process = subprocess.Popen(
                [
                    str(simulator),
                    "--zoom",
                    "1",
                    "--nobackground",
                    "--root",
                    str(root),
                ],
                cwd=build,
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        finally:
            log.close()
        try:
            window_id(process.pid)
            wait_for_file(frame)
            time.sleep(2.0)
            write_pointer(pointer, 1, profile["dock"])
            time.sleep(0.15)
            write_pointer(pointer, 0, profile["dock"])
            wait_for_guide(
                frame, profile["guide_probe"],
                output / "directv-window-timeout.png",
            )
            time.sleep(1.0)

            guide = output / "directv-guide.png"
            capture(frame, guide)
            if changed_pixels(guide, guide) != 0:
                failures.append("frame comparison helper is inconsistent")
            outside = pixel(guide, *profile["desktop_probe"])
            if not outside or max(outside) < 16:
                failures.append(
                    "the guide blacked out the Desktop outside its window"
                )

            tap(process.pid, "KP_5")
            wait_for_playback(frame, profile["guide_probe"])
            time.sleep(1.0)
            capture(frame, output / "directv-windowed.png")

            shots = []
            for index in range(3):
                shot = output / f"directv-video-{index}.png"
                subprocess.run(
                    [
                        "magick",
                        str(frame),
                        "-crop",
                        profile["video_crop"],
                        "+repage",
                        str(shot),
                    ],
                    check=True,
                )
                shots.append(shot)
                time.sleep(0.7)
            if max(
                changed_pixels(shots[0], shot) for shot in shots[1:]
            ) < 100:
                failures.append("the DIRECTV window is not decoding video")

            before = tuned_channel(root)
            tap(process.pid, "Right")
            time.sleep(2.5)
            after = tuned_channel(root)
            if after != before + 1:
                failures.append(
                    f"Right did not change the windowed channel "
                    f"({before} -> {after})"
                )
            capture(frame, output / "directv-channel-up.png")

            tap(process.pid, "KP_Decimal")
            time.sleep(2.0)
            wait_for_guide(
                frame, profile["guide_probe"],
                output / "directv-guide-return-timeout.png",
            )
            capture(frame, output / "directv-guide-return.png")

            tap(process.pid, "KP_Decimal")
            time.sleep(2.0)
            capture(frame, output / "directv-closed.png")
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print(f"DIRECTV Desktop Mode gate passed; captures in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
