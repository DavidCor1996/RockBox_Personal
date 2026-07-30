#!/usr/bin/env python3
"""Exercise Neon Nook apartment, city, furniture, car, save, and reload."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

from PIL import Image


REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "rockpod"))
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication  # noqa: E402

from services.maker_lite_export import sync_projects  # noqa: E402
from ui.maker_lite_creator import MakerLiteCreator  # noqa: E402


PLUGIN_PATH = "/.rockbox/rocks/games/maker_lite.rock"


def fields(line: str) -> dict[str, str]:
    return dict(re.findall(r"([A-Za-z0-9_]+)=([^ ]+)", line))


def validate_frame(source: Path, destination: Path) -> int:
    if not source.is_file():
        raise SystemExit(f"native gameplay frame missing: {source}")
    with Image.open(source) as opened:
        frame = opened.convert("RGB")
    colors = frame.getcolors(maxcolors=320 * 240)
    if frame.size != (320, 240) or colors is None or len(colors) < 24:
        raise SystemExit(f"native gameplay frame is visually sparse: {source}")
    frame.save(destination, format="BMP")
    return zlib.crc32(frame.tobytes()) & 0xFFFFFFFF


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir", type=Path, default=Path("build-sim-ipod6g")
    )
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/maker-lite-neon-nook-gate")
    )
    parser.add_argument("--ticks", type=int, default=420)
    args = parser.parse_args()

    build_dir = (REPO / args.build_dir).resolve()
    binary = build_dir / "rockboxui"
    plugin = build_dir / "apps/plugins/maker_lite/maker_lite.rock"
    if not binary.is_file() or not plugin.is_file():
        raise SystemExit("build the iPod 6G simulator and Maker Lite plugin first")

    output = args.output.expanduser().resolve()
    output.mkdir(parents=True, exist_ok=True)
    city_ticks = max(380, args.ticks)
    with tempfile.TemporaryDirectory(
        prefix="maker-lite-neon-nook-", dir="/tmp"
    ) as temporary:
        work = Path(temporary)
        private_root = work / "private"
        device_root = work / "device"
        (device_root / ".rockbox").mkdir(parents=True)

        application = QApplication.instance() or QApplication([])
        creator = MakerLiteCreator(str(private_root), str(REPO))
        try:
            creator._install_neon_nook()
            application.processEvents()
            if creator.record is None:
                raise SystemExit("Neon Nook installer did not create a starter")
            record = creator.record
        finally:
            creator.close()
            application.processEvents()

        source = record.source
        kinds = [entity["kind"] for entity in source["entities"]]
        if source.get("size") != [80, 56] or source.get("city_revision") != 2:
            raise SystemExit("Neon Nook did not install the expanded city")
        if kinds.count("npc") < 20 or kinds.count("furniture") != 10:
            raise SystemExit("Neon Nook population or apartment kit is incomplete")
        player = next(entity for entity in source["entities"] if entity["kind"] == "player")
        if player["x"] >= 256 or player["y"] >= 224:
            raise SystemExit("Neon Nook player does not start in the apartment")

        result = sync_projects(
            [record],
            str(private_root),
            str(device_root),
            plugin_source=str(plugin),
        )
        if result["rows"] != 1 or result["browser_rows"] != 1:
            raise SystemExit("Neon Nook was not registered in both launch indexes")

        manifest = json.loads(
            (
                private_root / "kits/zelda-neon-nook-v1/kit.mlk"
            ).read_text(encoding="utf-8")
        )
        if (
            len(manifest.get("asset_catalog", [])) < 180
            or len(manifest.get("player_frames", [])) != 88
            or sum(
                asset.get("kind") == "furniture"
                for asset in manifest.get("asset_catalog", [])
            )
            < 10
        ):
            raise SystemExit("installed Neon Nook art catalog is incomplete")

        project_id = record.project_id
        device_pack = f"/.rockbox/games/maker_lite/projects/{project_id}/game.mlp"
        log_path = device_root / ".rockbox/logs/maker_lite.log"
        save_path = (
            device_root
            / ".rockbox/games/maker_lite/saves"
            / f"{project_id}.sav"
        )
        process_log = output / "neon-nook-rockboxui.log"
        (device_root / ".rockbox/config.cfg").write_text(
            "start in screen: root\nresume: off\ntagcache_autoupdate: off\n",
            encoding="utf-8",
        )
        base_environment = os.environ.copy()
        base_environment.update(
            {
                "RBROOT": str(device_root),
                "ROCKBOX_SIM_PLUGIN": PLUGIN_PATH,
                "ROCKBOX_SIM_PLUGIN_PARAM": device_pack,
                "ROCKBOX_SIM_PLUGIN_EXIT": "1",
                "MAKER_LITE_TEST_SCRIPTED_INPUT": "1",
                "SDL_VIDEODRIVER": "dummy",
                "SDL_AUDIODRIVER": "dummy",
                "SDL_RENDER_DRIVER": "software",
                "ROCKPOD_SIM_HIDDEN": "1",
            }
        )

        def run_phase(
            name: str, ticks: int, *, expected_debt: str = "500"
        ):
            frame_device = f"/.rockbox/neon-nook-{name}.gate.ppm"
            environment = base_environment.copy()
            environment.update(
                {
                    "MAKER_LITE_TEST_TICKS": str(ticks),
                    "MAKER_LITE_TEST_FRAME": frame_device,
                }
            )
            launched = subprocess.run(
                [
                    str(binary),
                    "--nobackground",
                    "--root",
                    str(device_root),
                    "--zoom",
                    "1",
                ],
                cwd=build_dir,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=40,
                check=False,
            )
            with process_log.open("ab") as stream:
                stream.write(f"\n--- {name} ---\n".encode())
                stream.write(launched.stdout)
            launch_text = launched.stdout.decode("utf-8", errors="replace")
            if (
                launched.returncode != 0
                or "ROCKBOX_SIM_PLUGIN rc=0" not in launch_text
            ):
                raise SystemExit(
                    f"Neon Nook {name} failed (rc={launched.returncode}); "
                    f"see {process_log}"
                )
            rows = log_path.read_text(
                encoding="utf-8", errors="replace"
            ).splitlines()
            report = fields(rows[-1] if rows else "")
            (output / f"{name}-report.json").write_text(
                json.dumps(report, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            expected = {
                "project": project_id,
                "ruleset": "2",
                "playback_start": "0",
                "playback_end": "0",
                "debt": expected_debt,
            }
            for key, value in expected.items():
                if report.get(key) != value:
                    raise SystemExit(
                        f"{name} log {key}={report.get(key)!r}, "
                        f"expected {value!r}"
                    )
            if int(report.get("ticks", "0")) < ticks:
                raise SystemExit(f"{name} ended before its requested ticks")
            if int(report.get("missed", "999999")) > 6:
                raise SystemExit(f"{name} missed too many frame deadlines")
            frame_host = device_root / frame_device.lstrip("/")
            frame_crc = validate_frame(
                frame_host, output / f"neon-nook-{name}.bmp"
            )
            return report, frame_crc

        apartment, apartment_crc = run_phase("apartment", 60)
        if int(apartment.get("life_interactions", "0")) < 1:
            raise SystemExit("apartment phase did not interact with furniture")

        save_data = save_path.read_bytes()
        if (
            len(save_data) != 384
            or save_data[:4] != b"RMLS"
            or int.from_bytes(save_data[4:6], "little") != 5
            or zlib.crc32(save_data[:380]) & 0xFFFFFFFF
            != int.from_bytes(save_data[380:384], "little")
            or save_data[374] != 10
        ):
            raise SystemExit("apartment furniture save failed schema/CRC checks")
        moved_x = int.from_bytes(save_data[360:362], "little", signed=True)
        if moved_x == 12 * 16:
            raise SystemExit("scripted furnishing position did not persist")

        city, city_crc = run_phase("city", city_ticks)
        for key in ("car_entries", "car_exits", "room_transitions"):
            if int(city.get(key, "0")) < 1:
                raise SystemExit(f"city phase did not exercise {key}")
        first_world_tick = int(city.get("world_tick", "0"))

        reloaded, reload_crc = run_phase("reload", 60)
        if int(reloaded.get("world_tick", "0")) <= first_world_tick:
            raise SystemExit("Neon Nook world tick did not survive reload")
        reloaded_save = save_path.read_bytes()
        if reloaded_save[360:362] != save_data[360:362]:
            raise SystemExit("moved furniture did not survive native reload")

        legacy = bytearray(384)
        legacy[:4] = b"RMLS"
        legacy[4:6] = (4).to_bytes(2, "little")
        legacy[6] = 2
        legacy[8:12] = (0xA11CE55).to_bytes(4, "little")
        encoded_project = project_id.encode("ascii")
        legacy[16 : 16 + len(encoded_project)] = encoded_project
        legacy[324:328] = (777).to_bytes(4, "little")
        legacy[328:332] = (321).to_bytes(4, "little")
        legacy[332:334] = (2).to_bytes(2, "little")
        legacy[334:336] = (1).to_bytes(2, "little")
        legacy[336] = 2
        legacy[380:384] = (
            zlib.crc32(legacy[:380]) & 0xFFFFFFFF
        ).to_bytes(4, "little")
        save_path.write_bytes(legacy)
        migration, migration_crc = run_phase(
            "migration", 30, expected_debt="321"
        )
        if migration.get("credits") != "777":
            raise SystemExit("v4 pack-mismatch migration lost credits")
        migrated_save = save_path.read_bytes()
        if (
            int.from_bytes(migrated_save[4:6], "little") != 5
            or int.from_bytes(migrated_save[340:344], "little") >= 256 << 16
            or int.from_bytes(migrated_save[344:348], "little") >= 224 << 16
        ):
            raise SystemExit(
                "v4 migration did not reset safely into the apartment"
            )

        shutil.copy2(log_path, output / "maker_lite.log")
        shutil.copy2(
            device_root / ".rockbox/games/maker_lite/projects.tsv",
            output / "projects.tsv",
        )
        shutil.copy2(
            device_root / ".rockbox/rocks/games/maker_lite/games.tsv",
            output / "games.tsv",
        )
        print(
            "PASS Neon Nook: "
            f"project={project_id} cells={manifest['cell_count']} "
            f"parts={len(manifest['asset_catalog'])} frames=88 "
            f"apartment_interactions={apartment['life_interactions']} "
            f"city_ticks={city['ticks']} "
            f"car={city['car_entries']}/{city['car_exits']} "
            f"doors={city['room_transitions']} "
            f"reload_tick={reloaded['world_tick']} "
            f"migration=v4-v5 "
            f"frame_crc={apartment_crc:08x}/{city_crc:08x}/"
            f"{reload_crc:08x}/{migration_crc:08x}"
        )
        print(f"Neon Nook evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
