#!/usr/bin/env python3
"""Run every Maker Lite diagnostic project in an isolated iPod 6G simulator.

The diagnostic art produced by ``maker_lite_stage_sim_fixtures.py`` is only a
renderer/input/storage gate.  It is deliberately not an alternative to a
user-imported private authentic kit.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path

from PIL import Image


REPO = Path(__file__).resolve().parents[1]
PLUGIN_PATH = "/.rockbox/rocks/games/maker_lite.rock"
LOG_PATH = ".rockbox/logs/maker_lite.log"
SAVE_ROOT = ".rockbox/games/maker_lite/saves"
RULESETS = {
    "brawl-test-arena": 1,
    "mario-test-course": 1,
    "zelda-test-dungeon": 2,
    "sonic-test-zone": 3,
}
CONTROL_MAPS = {
    "brawl-test-arena": "16,32,64,128",
    "mario-test-course": "16,32,64,128",
    "zelda-test-dungeon": "32,16,64,128",
    "sonic-test-zone": "16,32,128,64",
}
SAVE_SIZE = 384
SAVE_CRC_OFFSET = 380


def parse_log_line(line: str) -> dict[str, str]:
    return dict(re.findall(r"([A-Za-z0-9_]+)=([^ ]+)", line))


def validate_save(path: Path, pack: Path, project_id: str, ruleset: int) -> None:
    data = path.read_bytes()
    pack_data = pack.read_bytes()
    if len(data) != SAVE_SIZE:
        raise SystemExit(f"{project_id}: save is {len(data)} bytes, expected {SAVE_SIZE}")
    if data[:4] != b"RMLS" or struct.unpack_from("<H", data, 4)[0] != 5:
        raise SystemExit(f"{project_id}: invalid save magic/version")
    if data[6] != ruleset:
        raise SystemExit(f"{project_id}: save ruleset mismatch")
    saved_project = data[16:48].split(b"\0", 1)[0].decode("ascii")
    if saved_project != project_id:
        raise SystemExit(f"{project_id}: save project ID mismatch")
    if struct.unpack_from("<I", data, 8)[0] != struct.unpack_from("<I", pack_data, 40)[0]:
        raise SystemExit(f"{project_id}: save is not bound to its pack digest")
    expected_crc = struct.unpack_from("<I", data, SAVE_CRC_OFFSET)[0]
    if zlib.crc32(data[:SAVE_CRC_OFFSET]) & 0xFFFFFFFF != expected_crc:
        raise SystemExit(f"{project_id}: save CRC mismatch")


def validate_frame(path: Path, project_id: str) -> str:
    if not path.is_file():
        raise SystemExit(f"{project_id}: simulator did not publish a framebuffer")
    with Image.open(path) as opened:
        frame = opened.convert("RGB")
        if frame.size != (320, 240):
            raise SystemExit(f"{project_id}: unexpected framebuffer size {frame.size}")
        colors = frame.getcolors(maxcolors=320 * 240)
        if colors is None or len(colors) < 4:
            raise SystemExit(f"{project_id}: framebuffer is blank or insufficiently rendered")
        digest = f"{zlib.crc32(frame.tobytes()) & 0xFFFFFFFF:08x}"
    return digest


def run_project(
    build_dir: Path,
    simdisk: Path,
    output_dir: Path,
    project_id: str,
    ruleset: int,
    ticks: int,
) -> tuple[dict[str, str], str]:
    device_pack = (
        f"/.rockbox/games/maker_lite/projects/{project_id}/game.mlp"
    )
    pack = simdisk / device_pack.lstrip("/")
    save = simdisk / SAVE_ROOT / f"{project_id}.sav"
    frame = output_dir / f"{project_id}.bmp"
    test_frame_device = f"/.rockbox/{project_id}.gate.ppm"
    test_frame_host = simdisk / test_frame_device.lstrip("/")
    process_log = output_dir / f"{project_id}.rockboxui.log"
    session_log = simdisk / LOG_PATH
    previous_lines = (
        session_log.read_text(encoding="utf-8", errors="replace").splitlines()
        if session_log.is_file()
        else []
    )
    frame.unlink(missing_ok=True)
    test_frame_host.unlink(missing_ok=True)

    environment = os.environ.copy()
    environment.update(
        {
            "RBROOT": str(simdisk),
            "ROCKBOX_SIM_PLUGIN": PLUGIN_PATH,
            "ROCKBOX_SIM_PLUGIN_PARAM": device_pack,
            "ROCKBOX_SIM_PLUGIN_EXIT": "1",
            "MAKER_LITE_TEST_TICKS": str(ticks),
            "MAKER_LITE_TEST_SCRIPTED_INPUT": "1",
            "MAKER_LITE_TEST_FRAME": test_frame_device,
            "SDL_VIDEODRIVER": "dummy",
            "SDL_AUDIODRIVER": "dummy",
            "SDL_RENDER_DRIVER": "software",
            "ROCKPOD_SIM_HIDDEN": "1",
        }
    )
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
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=30,
        check=False,
    )
    process_log.write_bytes(result.stdout)
    text = result.stdout.decode("utf-8", errors="replace")
    if result.returncode != 0 or "ROCKBOX_SIM_PLUGIN rc=0" not in text:
        raise SystemExit(
            f"{project_id}: simulator/plugin failed (rc={result.returncode}); "
            f"see {process_log}"
        )
    if not test_frame_host.is_file():
        raise SystemExit(f"{project_id}: native gameplay framebuffer was not dumped")
    with Image.open(test_frame_host) as opened:
        opened.convert("RGB").save(frame, format="BMP")
    if not session_log.is_file():
        raise SystemExit(f"{project_id}: session log was not written")
    lines = session_log.read_text(
        encoding="utf-8", errors="replace"
    ).splitlines()
    if len(lines) != len(previous_lines) + 1:
        raise SystemExit(f"{project_id}: expected exactly one new session record")
    fields = parse_log_line(lines[-1])
    required = {
        "project": project_id,
        "ruleset": str(ruleset),
        "playback_start": "0",
        "playback_end": "0",
        "controls": CONTROL_MAPS[project_id],
    }
    if project_id != "brawl-test-arena":
        required["complete"] = "1"
    for name, expected in required.items():
        if fields.get(name) != expected:
            raise SystemExit(
                f"{project_id}: log {name}={fields.get(name)!r}, expected {expected!r}"
            )
    if int(fields.get("ticks", "0")) < ticks:
        raise SystemExit(f"{project_id}: native loop stopped before {ticks} ticks")
    if int(fields.get("frames", "0")) < ticks // 3:
        raise SystemExit(f"{project_id}: native renderer produced too few frames")
    if int(fields.get("missed", "999999")) > 4:
        raise SystemExit(f"{project_id}: simulator missed too many 60 Hz deadlines")
    if (
        ruleset == 2
        and int(fields.get("room_transitions", "0")) < 1
    ):
        raise SystemExit(f"{project_id}: Zelda room link was not exercised")
    if project_id == "brawl-test-arena":
        damage = fields.get("brawl_damage", "0,0").split(",")
        stocks = fields.get("brawl_stocks", "0,0").split(",")
        if (
            len(damage) != 2
            or int(damage[1]) <= 0
            or len(stocks) != 2
            or int(stocks[0]) <= 0
            or int(stocks[1]) <= 0
        ):
            raise SystemExit(
                f"{project_id}: native brawl combat was not exercised"
            )

    validate_save(save, pack, project_id, ruleset)
    if save.with_suffix(save.suffix + ".tmp").exists():
        raise SystemExit(f"{project_id}: atomic save temporary file was left behind")
    digest = validate_frame(frame, project_id)
    return fields, digest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir", type=Path, default=Path("build-sim-ipod6g")
    )
    # The longest deterministic fixture (Sonic) reaches its goal just after
    # 400 ticks; a shorter default can report a false gameplay failure.
    parser.add_argument("--ticks", type=int, default=420)
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/maker-lite-sim-gate")
    )
    parser.add_argument("--keep-root", action="store_true")
    args = parser.parse_args()

    build_dir = (REPO / args.build_dir).resolve()
    binary = build_dir / "rockboxui"
    plugin = build_dir / "apps/plugins/maker_lite/maker_lite.rock"
    if not binary.is_file() or not plugin.is_file():
        raise SystemExit("build the iPod 6G simulator and Maker Lite plugin first")
    ticks = max(30, args.ticks)
    output_dir = args.output.expanduser().resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    runtime_root: Path | None = Path(
        tempfile.mkdtemp(prefix="maker-lite-sim-", dir="/tmp")
    )
    try:
        (runtime_root / ".rockbox").mkdir()
        subprocess.run(
            [
                str(REPO / "rockpod/.venv/bin/python"),
                str(REPO / "tools/maker_lite_stage_sim_fixtures.py"),
                str(runtime_root),
                "--plugin",
                str(plugin),
            ],
            cwd=REPO,
            check=True,
        )
        (runtime_root / ".rockbox/config.cfg").write_text(
            "start in screen: root\nresume: off\ntagcache_autoupdate: off\n",
            encoding="utf-8",
        )
        log_path = runtime_root / LOG_PATH
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.unlink(missing_ok=True)
        save_root = runtime_root / SAVE_ROOT
        save_root.mkdir(parents=True, exist_ok=True)
        legacy_save = save_root / "mario-test-course.sav"
        legacy_save.write_bytes(b"RMLS" + b"\0" * 92)

        frame_digests: set[str] = set()
        reports = []
        for project_id, ruleset in RULESETS.items():
            fields, digest = run_project(
                build_dir,
                runtime_root,
                output_dir,
                project_id,
                ruleset,
                ticks,
            )
            frame_digests.add(digest)
            reports.append(
                f"PASS {project_id}: ticks={fields['ticks']} "
                f"frames={fields['frames']} missed={fields['missed']} "
                f"max_render_ticks={fields['max_render_ticks']} "
                f"frame_crc={digest}"
            )
        legacy_corrupt = legacy_save.with_suffix(".sav.corrupt")
        if not legacy_corrupt.is_file() or legacy_corrupt.stat().st_size != 96:
            raise SystemExit("legacy save was not preserved as .corrupt")
        recovery_output = output_dir / "save-recovery"
        recovery_output.mkdir(exist_ok=True)
        valid_save = legacy_save.read_bytes()
        legacy_save.with_suffix(".sav.previous").write_bytes(valid_save)
        legacy_save.write_bytes(b"broken")
        recovery_fields, _digest = run_project(
            build_dir,
            runtime_root,
            recovery_output,
            "mario-test-course",
            RULESETS["mario-test-course"],
            30,
        )
        validate_save(
            legacy_save,
            runtime_root
            / ".rockbox/games/maker_lite/projects/mario-test-course/game.mlp",
            "mario-test-course",
            RULESETS["mario-test-course"],
        )
        if legacy_save.with_suffix(".sav.previous").exists():
            raise SystemExit("recovered save backup was not retired after commit")
        if legacy_corrupt.read_bytes() != b"broken":
            raise SystemExit("corrupt current save was not preserved for diagnosis")
        if len(frame_digests) != len(RULESETS):
            raise SystemExit("ruleset framebuffer captures were not distinct")
        shutil.copy2(log_path, output_dir / "maker_lite.log")
        for report in reports:
            print(report)
        print(
            "PASS save recovery: corrupt current preserved, valid previous "
            f"restored, ticks={recovery_fields['ticks']}"
        )
        print(f"Simulator evidence: {output_dir}")
        if args.keep_root:
            print(f"Preserved isolated simulator root: {runtime_root}")
            runtime_root = None
        return 0
    finally:
        if runtime_root is not None and runtime_root.is_dir():
            shutil.rmtree(runtime_root)


if __name__ == "__main__":
    raise SystemExit(main())
