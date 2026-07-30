#!/usr/bin/env python3
"""Prove Stick RPG's authored save and Continue paths across simulator runs."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import time


class GateError(RuntimeError):
    """The Stick RPG save/reload simulator gate did not pass."""


def regular(path: Path, description: str) -> Path:
    path = path.resolve()
    if not path.is_file():
        raise GateError(f"{description} is missing: {path}")
    return path


def run_until(
    simulator: Path,
    build_dir: Path,
    log_path: Path,
    environment: dict[str, str],
    markers: tuple[str, ...],
    timeout: float,
) -> str:
    log_path.unlink(missing_ok=True)
    process = subprocess.Popen(
        [str(simulator)],
        cwd=build_dir,
        env=environment,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    deadline = time.monotonic() + timeout
    latest = ""
    try:
        while time.monotonic() < deadline:
            if log_path.is_file():
                latest = log_path.read_text(
                    encoding="utf-8", errors="replace"
                )
            if all(marker in latest for marker in markers):
                return latest
            if process.poll() is not None:
                raise GateError(
                    f"simulator exited early with status {process.returncode}"
                )
            time.sleep(0.1)
        missing = [marker for marker in markers if marker not in latest]
        raise GateError("timed out waiting for: " + ", ".join(missing))
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)


def common_environment(swf_name: str) -> dict[str, str]:
    environment = os.environ.copy()
    environment.update(
        {
            "ROCKBOX_SIM_PLUGIN": (
                "/.rockbox/rocks/viewers/flashplayer.rock"
            ),
            "ROCKBOX_SIM_PLUGIN_PARAM": (
                "/.rockbox/flash/stickrpg/" + swf_name
            ),
            "FLASHPLAYER_PRIME_STICKRPG": "0",
            "FLASHPLAYER_STICKRPG_SHORTCUT": "0",
            "FLASHPLAYER_AUTORUN_FRAMES": "130",
        }
    )
    return environment


def validate_save(path: Path) -> None:
    if not path.is_file():
        raise GateError(f"save file was not created: {path}")
    data = path.read_text(encoding="utf-8", errors="strict")
    if not data.startswith("RBSO2\n"):
        raise GateError("save file does not use nested RBSO2 format")
    if "O\tmyObj\n" not in data:
        raise GateError("save file is missing data.myObj")
    if "A\tobjArray\n" not in data:
        raise GateError("save file is missing myObj.objArray")

    value_records = sum(
        line.startswith(("S\t", "B\t", "N\t"))
        for line in data.splitlines()
    )
    if value_records < 75:
        raise GateError(
            f"save file contains only {value_records} value records"
        )


def run_gate(build_dir: Path, timeout: float) -> None:
    build_dir = build_dir.resolve()
    simulator = regular(build_dir / "rockboxui", "Rockbox simulator")
    plugin = regular(
        build_dir / "apps/plugins/flashplayer/flashplayer.rock",
        "built flashplayer plugin",
    )
    simdisk = build_dir / "simdisk"
    swf = regular(
        simdisk / ".rockbox/flash/stickrpg/stickrpg.swf",
        "staged Stick RPG SWF",
    )
    installed_plugin = (
        simdisk / ".rockbox/rocks/viewers/flashplayer.rock"
    )
    installed_plugin.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(plugin, installed_plugin)

    flash_dir = simdisk / ".rockbox/flash"
    log_path = flash_dir / "flashplayer.log"
    save_path = flash_dir / "shared/xgensrpg.rbso"
    save_path.unlink(missing_ok=True)

    save_environment = common_environment(swf.name)
    save_environment.update(
        {
            # Exercise the authored Start -> difficulty -> Done -> Create
            # buttons.  The simulator-only film shortcut skips only the
            # 850-frame noninteractive transition after character creation.
            "FLASHPLAYER_STICKRPG_SHORTCUT": "1",
            "FLASHPLAYER_AUTORUN_FRAMES": "190",
            "FLASHPLAYER_AUTORUN_CLICKS": (
                "50:165:147;70:160:112;90:160:206;110:160:206"
            ),
            "FLASHPLAYER_AUTORUN_CALL_SAVE_FRAME": "150",
        }
    )
    save_log = run_until(
        simulator,
        build_dir,
        log_path,
        save_environment,
        (
            "stickrpg authored UI ready",
            "stickrpg entered gameplay after character creation",
            "autorun saveGame called frame=150",
            "sharedobject flush name=xgensrpg",
        ),
        timeout,
    )
    if "gameswf error:" in save_log:
        raise GateError("GameSWF reported an error while saving")
    validate_save(save_path)

    load_environment = common_environment(swf.name)
    load_environment.update(
        {
            "FLASHPLAYER_AUTORUN_SCROLL_FRAME": "60",
            "FLASHPLAYER_AUTORUN_SCROLL_DIRECTION": "1",
            "FLASHPLAYER_AUTORUN_SELECT_FRAME": "75",
        }
    )
    load_log = run_until(
        simulator,
        build_dir,
        log_path,
        load_environment,
        (
            "stickrpg cursor snap dir=0,1",
            "sharedobject load name=xgensrpg",
        ),
        timeout,
    )
    if "gameswf error:" in load_log:
        raise GateError("GameSWF reported an error while loading")
    if "sharedobject load-miss name=xgensrpg" in load_log:
        raise GateError("Continue missed the save created by the first run")

    print(
        "Stick RPG save/reload simulator gate passed: "
        f"{save_path} and {log_path}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build",
        type=Path,
        default=Path("build-sim-ipod6g"),
        help="configured iPod 6G simulator build directory",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=45.0,
        help="seconds allowed for each simulator run",
    )
    args = parser.parse_args()
    try:
        run_gate(args.build, args.timeout)
    except GateError as error:
        parser.exit(1, f"stickrpg save gate: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
