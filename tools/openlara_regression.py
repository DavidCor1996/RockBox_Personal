#!/usr/bin/env python3
"""Static/package regression gate for the isolated OpenLara milestone."""

from __future__ import annotations

import argparse
from pathlib import Path
import zipfile


FORBIDDEN_DATA = {".pkd", ".scr", ".ad4", ".psx", ".phd", ".bin", ".cue"}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--sim-build", type=Path, default=Path("build-sim-ipod6g"))
    parser.add_argument("--hw-build", type=Path, default=Path("build-hw-ipod6g"))
    args = parser.parse_args()

    root = args.root.resolve()
    sim = (root / args.sim_build).resolve()
    hw = (root / args.hw_build).resolve()
    launcher = (root / "apps/plugins/rockboy_launcher.c").read_text(encoding="utf-8")
    categories = (root / "apps/plugins/CATEGORIES").read_text(encoding="utf-8")

    required_launcher_fragments = (
        '#define OPENLARA_PLUGIN_PATH  PLUGIN_GAMES_DIR "/openlara.rock"',
        '#define PS1_ROM_DIR           ROCKBOX_DIR "/games/ps1"',
        '"ps1\\tPlayStation\\tOpenLara',
        'add_system_entry("ps1", "PlayStation", "OpenLara"',
        'rb->strlcpy(system->extensions, ".olr"',
        'if (!rb->strcmp(system_id, "ps1"))',
    )
    for fragment in required_launcher_fragments:
        require(fragment in launcher, f"launcher integration missing {fragment!r}")
    require("openlara,games" in categories, "OpenLara is not packaged as a game")

    sim_plugin = sim / "apps/plugins/openlara/openlara.rock"
    hw_plugin = hw / "apps/plugins/openlara/openlara.rock"
    require(sim_plugin.is_file(), "simulator plugin missing")
    require(hw_plugin.is_file(), "ARM plugin missing")
    require(hw_plugin.stat().st_size < 3 * 1024 * 1024, "ARM plugin exceeds 3 MiB")

    engine_root = root / "apps/plugins/openlara"
    leaked = [path for path in engine_root.rglob("*") if path.suffix.lower() in FORBIDDEN_DATA]
    require(not leaked, f"game data leaked into source tree: {leaked}")

    archive = hw / "rockbox.zip"
    require(archive.is_file(), "hardware rockbox.zip missing")
    with zipfile.ZipFile(archive) as package:
        names = [name.lower() for name in package.namelist()]
    require(".rockbox/rocks/games/openlara.rock" in names,
            "OpenLara is not in the Games plugin directory")
    packaged_data = [name for name in names if Path(name).suffix in FORBIDDEN_DATA]
    require(not packaged_data, f"copyrighted data present in firmware ZIP: {packaged_data}")

    print(
        "PASS: launcher=ps1 descriptor=.olr sim=present arm_bytes="
        f"{hw_plugin.stat().st_size} package=games data_leaks=0"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
