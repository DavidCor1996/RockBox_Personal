#!/usr/bin/env python3
"""Build, print, and optionally apply a one-ROM SNES Lite RockPod sync plan."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
ROCKPOD_ROOT = REPO_ROOT / "rockpod"
sys.path.insert(0, str(ROCKPOD_ROOT))

from services.rockbox_deploy import RockboxDeployService  # noqa: E402
from services.rockbox_games import RockboxGameService  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--target", type=Path, required=True)
    parser.add_argument("--mock", action="store_true", help="Create/reset an isolated mock target")
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    rom = args.rom.expanduser().resolve()
    target = args.target.expanduser().resolve()
    if not rom.is_file():
        raise SystemExit(f"ROM not found: {rom}")
    if args.mock:
        if target.exists():
            shutil.rmtree(target)
        (target / ".rockbox").mkdir(parents=True)
        (target / "rockbox-info.txt").write_text(
            "Target: ipod6g\nVersion: SNES Lite mock\n", encoding="utf-8"
        )
    if not target.is_dir():
        raise SystemExit(f"Target is not mounted: {target}")

    library = REPO_ROOT / "rockpod" / ".staging" / "snes-lite-gate-library"
    if library.exists():
        shutil.rmtree(library)
    library.mkdir(parents=True)
    shutil.copy2(rom, library / rom.name)

    profile = {
        "id": "snes-lite-gate",
        "name": "SNES Lite Gate",
        "games_library_path": str(library),
        "device_mount_path": str(target),
        "source_repo_path": str(REPO_ROOT),
        "target_device_model": "iPod Classic 6G",
        "screen_resolution": "320x240",
        "selected_theme": "",
    }
    service = RockboxGameService()
    deploy = RockboxDeployService()
    games = service.list_games(profile)
    selected = [
        game for game in games
        if game.get("filename") == rom.name and game.get("source_path")
    ]
    if len(selected) != 1:
        raise SystemExit("RockPod did not resolve exactly the selected SNES ROM")
    game = selected[0]
    bundle = service.build_sync_bundle(profile, selected, "device")
    deploy_profile = service.deploy_profile(profile, "device")
    diff = deploy.build_diff(deploy_profile, bundle)
    report = {
        "game": {
            key: game.get(key)
            for key in (
                "title",
                "internal_title",
                "rom_hash",
                "mapper",
                "special_chip",
                "compatibility",
                "cover_path",
            )
        },
        "mount_path": diff["mount_path"],
        "summary": diff["summary"],
        "items": [
            {
                "kind": item["kind"],
                "status": item["status"],
                "destination": item["destination_rel"],
            }
            for item in diff["items"]
        ],
    }
    if args.apply:
        result = deploy.apply_diff(deploy_profile, diff)
        report["apply"] = result
        if not result.get("success"):
            print(json.dumps(report, indent=2, sort_keys=True))
            return 1
        for relative_dir in (
            ".rockbox/saves/snes",
            ".rockbox/config/snes_lite",
            ".rockbox/logs",
        ):
            (target / relative_dir).mkdir(parents=True, exist_ok=True)
        verify = deploy.build_diff(deploy_profile, bundle)
        report["verify_summary"] = verify["summary"]
        if verify["summary"].get("add") or verify["summary"].get("overwrite"):
            print(json.dumps(report, indent=2, sort_keys=True))
            return 1
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
