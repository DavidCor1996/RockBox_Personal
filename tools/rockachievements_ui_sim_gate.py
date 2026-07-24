#!/usr/bin/env python3
"""Exercise the RockAchievements browser in the iPod 6G simulator."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from rockboy_profile_gate import (
    LANG_START_SCREEN,
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


PLUGIN_PATH = "/.rockbox/rocks/apps/achievements.rock"
BUTTON_GATES: dict[str, Path] = {}


def write_cstring(buffer: bytearray, offset: int, size: int, value: str) -> None:
    encoded = value.encode("utf-8")[: size - 1]
    buffer[offset : offset + len(encoded)] = encoded


def start_screen_lang_id(build_dir: Path) -> int:
    lang_enum = build_dir / "lang_enum.h"
    if lang_enum.is_file():
        for line in lang_enum.read_text(
            encoding="utf-8", errors="replace"
        ).splitlines():
            if "LANG_START_SCREEN," in line and "/*" in line:
                return int(line.split("/*", 1)[1].split("*/", 1)[0].strip())
    return LANG_START_SCREEN


def prepare_root(repo: Path, build_dir: Path, root: Path) -> None:
    source = build_dir / "simdisk" / ".rockbox"
    plugin = build_dir / "apps" / "plugins" / "achievements.rock"
    catalog = source / "achievements"
    sphere = (
        repo
        / "assets"
        / "ipodjs"
        / "rockbox"
        / "achievements"
        / "xbox360-sphere-official.32x32x24.bmp"
    )
    for required in (plugin, catalog / "current", sphere):
        if not required.exists():
            raise SystemExit(f"missing UI gate input: {required}")

    rockbox = root / ".rockbox"
    rockbox.mkdir(parents=True)
    for name in ("fonts", "langs", "icons"):
        if (source / name).is_dir():
            shutil.copytree(source / name, rockbox / name)
    shutil.copytree(catalog, rockbox / "achievements")
    avatar_base = (
        repo / "assets" / "ipodjs" / "rockbox" / "achievements" /
        "avatar" / "base"
    )
    avatar_generation = rockbox / "achievements" / "avatar" / \
        "generations" / "sim-avatar"
    shutil.copytree(avatar_base / "xna-boy" / "clips",
                    avatar_generation / "clips")
    shutil.copytree(avatar_base / "sounds", avatar_generation / "sounds")
    shutil.copy2(avatar_base / "xna-boy" / "portrait.80x80x24.bmp",
                 avatar_generation / "portrait.80x80x24.bmp")
    (avatar_generation / "profile.v1.tsv").write_text(
        "key\tvalue\n"
        "display_name\tSIM PLAYER\n"
        "body\txna-boy\n"
        "favorite_clip\tjump\n"
        "games\t8\n"
        "unlocked\t24\n"
        "achievements\t80\n"
        "gamerscore\t415\n",
        encoding="utf-8",
    )
    (rockbox / "achievements" / "avatar" / "current").write_text(
        "sim-avatar\n", encoding="utf-8"
    )
    state = rockbox / "achievements" / "state"
    state.mkdir(parents=True, exist_ok=True)
    (state / "avatar-preferences.v1.tsv").write_text(
        "key\tvalue\n"
        "motion\tfull\n"
        "sounds\tfull\n"
        "sounds_over_music\ton\n"
        "idle_emotes\ton\n",
        encoding="utf-8",
    )
    plugin_target = root / PLUGIN_PATH.lstrip("/")
    plugin_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin, plugin_target)
    sphere_target = rockbox / "ipodjs" / "achievements" / sphere.name
    sphere_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(sphere, sphere_target)

    entry = bytearray(OPEN_PLUGIN_ENTRY_SIZE)
    checksum = open_plugin_lang_checksum(build_dir)
    struct.pack_into(
        "<IiI",
        entry,
        0,
        START_SCREEN_HASH,
        start_screen_lang_id(build_dir),
        checksum or OPEN_PLUGIN_CHECKSUM,
    )
    write_cstring(
        entry, OPEN_PLUGIN_NAME_OFFSET, OPEN_PLUGIN_NAME_SIZE, "achievements.rock"
    )
    write_cstring(entry, OPEN_PLUGIN_PATH_OFFSET, OPEN_PLUGIN_PATH_SIZE, PLUGIN_PATH)
    write_cstring(entry, OPEN_PLUGIN_PARAM_OFFSET, OPEN_PLUGIN_PARAM_SIZE, "")
    (rockbox / "rocks" / "plugin.dat").write_bytes(entry)
    (rockbox / "config.cfg").write_text(
        "start in screen: plugin\nresume: off\ntagcache_autoupdate: off\n",
        encoding="utf-8",
    )


def wait_for_file(path: Path, timeout: float = 20.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if path.is_file() and path.stat().st_size >= 307_200:
            return
        time.sleep(0.05)
    raise SystemExit(f"framebuffer capture timed out: {path}")


def wait_for_achievements(frame: Path, timeout: float = 35.0) -> None:
    """Wait until the green achievements blade replaces the boot screen."""
    wait_for_file(frame)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = subprocess.run(
            ["magick", str(frame), "-format", "%[pixel:p{36,50}]", "info:"],
            check=False,
            capture_output=True,
            text=True,
        )
        match = re.search(r"\((\d+),(\d+),(\d+)", result.stdout)
        if match:
            red, green, blue = (int(value) for value in match.groups())
            if green > red + 25 and green > blue + 25:
                return
        time.sleep(0.2)
    raise SystemExit("achievements plugin startup timed out")


def window_id(pid: int) -> str:
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        result = subprocess.run(
            ["xdotool", "search", "--pid", str(pid)],
            check=False,
            capture_output=True,
            text=True,
        )
        ids = result.stdout.splitlines()
        if ids:
            window = ids[-1]
            subprocess.run(
                ["xdotool", "windowactivate", window], check=False
            )
            time.sleep(0.25)
            return window
        time.sleep(0.25)
    raise SystemExit("could not find the simulator window")


def tap(pid: int, key: str) -> None:
    gate = BUTTON_GATES.get(key)
    if gate is not None:
        gate.touch()
        time.sleep(0.12)
        gate.unlink(missing_ok=True)
        time.sleep(1.5)
        return
    window = window_id(pid)
    subprocess.run(["xdotool", "keydown", "--window", window, key], check=True)
    time.sleep(0.08)
    subprocess.run(["xdotool", "keyup", "--window", window, key], check=True)
    time.sleep(0.45)


def hold(pid: int, key: str, duration: float = 1.3) -> None:
    gate = BUTTON_GATES.get(key)
    if gate is not None:
        gate.touch()
        time.sleep(duration)
        gate.unlink(missing_ok=True)
        time.sleep(0.9)
        return
    window = window_id(pid)
    subprocess.run(["xdotool", "keydown", "--window", window, key], check=True)
    time.sleep(duration)
    subprocess.run(["xdotool", "keyup", "--window", window, key], check=True)
    time.sleep(0.65)


def capture(frame: Path, output: Path) -> None:
    wait_for_file(frame)
    subprocess.run(["magick", str(frame), str(output)], check=True)


def changed_pixels(first: Path, second: Path) -> int:
    result = subprocess.run(
        ["magick", "compare", "-metric", "AE", str(first), str(second), "null:"],
        check=False,
        capture_output=True,
        text=True,
    )
    value = (result.stderr or result.stdout).strip().split()[0]
    return int(float(value))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/rockachievements-ui-gate")
    )
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    build_dir = args.build_dir.resolve()
    simulator = build_dir / "rockboxui"
    if not simulator.is_file():
        raise SystemExit(f"missing simulator: {simulator}")
    for command in ("magick", "xdotool"):
        if shutil.which(command) is None:
            raise SystemExit(f"missing required command: {command}")

    args.output.mkdir(parents=True, exist_ok=True)
    gate_root = args.output / ".button-gates"
    shutil.rmtree(gate_root, ignore_errors=True)
    gate_root.mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix="rockachievements-ui-", dir="/tmp") as temp:
        root = Path(temp)
        prepare_root(repo, build_dir, root)
        frame = root / "frame.bmp"
        BUTTON_GATES.clear()
        BUTTON_GATES.update({
            "KP_5": gate_root / "select.gate",
            "KP_Decimal": gate_root / "menu.gate",
            "KP_Add": gate_root / "play.gate",
            "KP_2": gate_root / "scroll-forward.gate",
            "KP_8": gate_root / "scroll-back.gate",
        })
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_AUDIODRIVER": "dummy",
                "SDL_VIDEODRIVER": "x11",
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_PREVIEW_BMP": str(frame),
                "ROCKPOD_SIM_PREVIEW_INTERVAL_MS": "16",
                "ROCKPOD_SIM_SELECT_GATE": str(BUTTON_GATES["KP_5"]),
                "ROCKPOD_SIM_MENU_GATE": str(BUTTON_GATES["KP_Decimal"]),
                "ROCKPOD_SIM_PLAY_ACTION_GATE": str(BUTTON_GATES["KP_Add"]),
                "ROCKPOD_SIM_SCROLL_FWD_GATE": str(BUTTON_GATES["KP_2"]),
                "ROCKPOD_SIM_SCROLL_BACK_GATE": str(BUTTON_GATES["KP_8"]),
            }
        )
        process = subprocess.Popen(
            [str(simulator), "--zoom", "1", "--nobackground", "--root", str(root)],
            cwd=build_dir,
            env=environment,
        )
        try:
            window_id(process.pid)
            wait_for_achievements(frame)
            time.sleep(0.5)
            landing = args.output / "achievements-landing.png"
            consoles = args.output / "achievements-consoles.png"
            filtered = args.output / "achievements-filtered.png"
            listing = args.output / "achievements-list.png"
            detail = args.output / "achievements-detail.png"
            avatar = args.output / "achievements-avatar.png"
            avatar_motion = args.output / "achievements-avatar-motion.png"
            avatar_paused = args.output / "achievements-avatar-paused.png"
            avatar_still = args.output / "achievements-avatar-still.png"
            avatar_rotated = args.output / "achievements-avatar-rotated.png"
            avatar_settings = args.output / "achievements-avatar-settings.png"
            returned = args.output / "achievements-returned-detail.png"
            capture(frame, landing)
            hold(process.pid, "KP_5")
            capture(frame, consoles)
            tap(process.pid, "KP_2")
            tap(process.pid, "KP_5")
            capture(frame, filtered)
            tap(process.pid, "KP_5")
            capture(frame, listing)
            tap(process.pid, "KP_5")
            capture(frame, detail)
            hold(process.pid, "KP_Decimal")
            capture(frame, avatar)
            time.sleep(0.6)
            capture(frame, avatar_motion)
            tap(process.pid, "KP_Add")
            capture(frame, avatar_paused)
            time.sleep(0.6)
            capture(frame, avatar_still)
            tap(process.pid, "KP_2")
            capture(frame, avatar_rotated)
            tap(process.pid, "KP_Add")
            hold(process.pid, "KP_5")
            capture(frame, avatar_settings)
            if changed_pixels(landing, consoles) < 10_000:
                raise SystemExit("Hold Select did not open the console filter")
            if changed_pixels(landing, filtered) < 100:
                raise SystemExit("console filter did not update the games blade")
            if changed_pixels(filtered, listing) < 10_000:
                raise SystemExit("Select did not open the achievement list")
            if changed_pixels(listing, detail) < 10_000:
                raise SystemExit("Select did not open achievement detail")
            if changed_pixels(detail, avatar) < 10_000:
                raise SystemExit("Hold Menu did not open the avatar profile")
            if changed_pixels(avatar, avatar_motion) < 100:
                raise SystemExit("authentic avatar animation did not advance")
            if changed_pixels(avatar_paused, avatar_still) > 20:
                raise SystemExit("Play/Pause did not freeze the avatar preview")
            if changed_pixels(avatar_still, avatar_rotated) < 100:
                raise SystemExit("click wheel did not rotate the 3D avatar")
            if changed_pixels(avatar_motion, avatar_settings) < 10_000:
                raise SystemExit("Hold Select did not open avatar settings")
            tap(process.pid, "KP_Decimal")
            tap(process.pid, "KP_Decimal")
            capture(frame, returned)
            if changed_pixels(detail, returned) > 1_000:
                raise SystemExit("Menu did not return to the originating screen")
            tap(process.pid, "KP_Decimal")
        finally:
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
    shutil.rmtree(gate_root, ignore_errors=True)
    print(f"RockAchievements UI simulator gate passed; captures: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
