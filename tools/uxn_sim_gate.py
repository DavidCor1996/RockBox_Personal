#!/usr/bin/env python3
"""Launch the Uxn demo ROM through the Rockbox iPod simulator."""

from __future__ import annotations

import argparse
import base64
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
PLUGIN_DEVICE_PATH = "/.rockbox/rocks/viewers/uxn.rock"
ROM_DEVICE_PATH = "/Uxn/rockbox-demo.rom"
FILE_TEST_DEVICE_PATH = "/Uxn/file-device-test.rom"
GAME_ROMS = {
    "donsol.rom": "d86527c1d0597e74a18b8663cc02e7bb0b85e94435ae26cf333d81de26711058",
    "niju.rom": "2b4b977173273f1d3eb4117347cd4888b1f431e288f77a26a59e830409fa1012",
    "worm.rom": "cb43e8ba9375cff8356ca00038233b5579f15ec507bd0cacbffcde3e0ac88a1a",
}


def launch(simulator: Path, build: Path, simdisk: Path,
           environment: dict[str, str], rom: str) -> None:
    environment["ROCKBOX_SIM_PLUGIN_PARAM"] = rom
    result = subprocess.run(
        [str(simulator), "--nobackground", "--root", str(simdisk),
         "--zoom", "1"],
        cwd=build,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=30,
        check=False,
    )
    output = result.stdout.decode("utf-8", errors="replace")
    if result.returncode != 0 or "ROCKBOX_SIM_PLUGIN rc=0" not in output:
        print(output)
        raise SystemExit(
            f"Uxn simulator gate failed for {rom} with "
            f"rc={result.returncode}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path,
                        default=ROOT / "build-sim-ipod6g")
    parser.add_argument("--frames", type=int, default=3)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    simulator = build / "rockboxui"
    plugin = build / "apps/plugins/uxn/uxn.rock"
    if not simulator.is_file():
        raise SystemExit(f"simulator not found: {simulator}")
    if not plugin.is_file():
        raise SystemExit(f"plugin not found: {plugin}")
    if args.frames < 1:
        parser.error("--frames must be positive")

    with tempfile.TemporaryDirectory(prefix="uxn-sim-") as temp_name:
        simdisk = Path(temp_name)
        plugin_target = simdisk / PLUGIN_DEVICE_PATH.lstrip("/")
        rom_target = simdisk / ROM_DEVICE_PATH.lstrip("/")
        plugin_target.parent.mkdir(parents=True)
        rom_target.parent.mkdir(parents=True)
        shutil.copy2(plugin, plugin_target)
        rom_target.write_bytes(
            base64.b64decode(
                (ROOT / "apps/plugins/uxn/roms/rockbox-demo.rom.b64")
                .read_bytes()
            )
        )
        file_test_target = simdisk / FILE_TEST_DEVICE_PATH.lstrip("/")
        file_test_target.write_bytes(
            base64.b64decode(
                (ROOT / "tests/uxn/file-device-test.rom.b64").read_bytes()
            )
        )
        for name, expected_hash in GAME_ROMS.items():
            source = ROOT / "assets/uxn_games/roms" / name
            payload = source.read_bytes()
            actual_hash = hashlib.sha256(payload).hexdigest()
            if actual_hash != expected_hash:
                raise SystemExit(
                    f"Uxn game checksum mismatch for {name}: {actual_hash}"
                )
            shutil.copy2(source, rom_target.parent / name)
        (simdisk / ".rockbox/config.cfg").write_text(
            "start in screen: root\nresume: off\ntagcache_autoupdate: off\n",
            encoding="utf-8",
        )
        environment = os.environ.copy()
        environment.update(
            {
                "RBROOT": str(simdisk),
                "ROCKBOX_SIM_PLUGIN": PLUGIN_DEVICE_PATH,
                "ROCKBOX_SIM_PLUGIN_EXIT": "1",
                "UXN_TEST_FRAMES": str(args.frames),
                "SDL_VIDEODRIVER": "dummy",
                "SDL_AUDIODRIVER": "dummy",
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_HIDDEN": "1",
            }
        )
        launch(simulator, build, simdisk, environment, ROM_DEVICE_PATH)
        for name in GAME_ROMS:
            launch(simulator, build, simdisk, environment, f"/Uxn/{name}")
        launch(simulator, build, simdisk, environment, FILE_TEST_DEVICE_PATH)
        written = simdisk / ".rockbox/uxn/data/gate.txt"
        if not written.is_file() or written.read_bytes() != b"hello":
            raise SystemExit("Varvara file-device write check failed")

    print("Uxn simulator gate: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
