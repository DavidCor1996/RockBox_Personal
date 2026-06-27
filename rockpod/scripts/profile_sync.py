#!/usr/bin/env python3
"""Build and execute a real RockPod sync while printing timing data."""

import argparse
import json
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from app.config import Config
from app.database import Database
from services.artwork_manager import ArtworkManager
from services.device_detector import DeviceDetector, DeviceInfo
from services.rockbox_device import invalidate_pictureflow_cache
from services.rockbox_playlists import export_device_playlists
from services.sync_engine import SyncEngine, SyncWorker


def parse_args():
    parser = argparse.ArgumentParser(description="Profile a real RockPod sync run")
    parser.add_argument("--config", help="Config file path")
    parser.add_argument("--db", help="Database path override")
    parser.add_argument("--music-dir", help="Music directory override")
    parser.add_argument("--device-path", required=True, help="Mounted device path")
    parser.add_argument("--force-full", action="store_true", help="Force full resync planning")
    parser.add_argument("--limit", type=int, default=0, help="Limit sync operations for a controlled benchmark")
    parser.add_argument("--benchmark-root", help="Override device music root prefix for benchmark copies")
    parser.add_argument("--skip-cache-update", action="store_true", help="Skip cache updates after execution")
    parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")
    return parser.parse_args()


def main():
    args = parse_args()
    config = Config(args.config)
    if args.db:
        config.db_path = args.db
    if args.music_dir:
        config.music_dir = args.music_dir
    config.mock_device_enabled = False
    config.device_mount_path = args.device_path
    config.ensure_dirs()
    Database.init_db_once(config.db_path)

    db = Database(config.db_path)
    artwork = ArtworkManager(config.artwork_cache_dir, config)
    try:
        detector = DeviceDetector(config)
        device = DeviceInfo(args.device_path)
        detector._current_device = device
        engine = SyncEngine(db, config, detector, artwork)
        engine.set_current_device(device)

        original_dir_template = config.device_music_template
        if args.benchmark_root:
            benchmark_root = args.benchmark_root.strip().strip("/").replace("\\", "/")
            config.device_music_template = f"{benchmark_root}/{{album_artist}}/{{album}}"
        plan = engine.build_sync_plan(force_full=args.force_full)
        if args.benchmark_root:
            config.device_music_template = original_dir_template

        if args.limit and args.limit > 0:
            limited = type(plan)()
            limited.to_copy = list(plan.to_copy[:args.limit])
            remaining = max(0, args.limit - len(limited.to_copy))
            if remaining:
                limited.to_resync = list(plan.to_resync[:remaining])
                remaining = max(0, remaining - len(limited.to_resync))
            if remaining:
                limited.artwork_to_copy = list(plan.artwork_to_copy[:remaining])
            limited.total_bytes = sum(
                os.path.getsize(dict(track_row).get("file_path", ""))
                for track_row, _rel_path in limited.to_copy + [(row, rel) for row, _old, rel in limited.to_resync]
                if os.path.isfile(dict(track_row).get("file_path", ""))
            )
            limited.errors = list(plan.errors)
            limited.plan_profile = dict(plan.plan_profile or {})
            plan = limited

        report = {
            "device_path": args.device_path,
            "plan_summary": plan.summary(),
            "plan_profile": dict(plan.plan_profile or {}),
            "operations": {
                "copy": len(plan.to_copy),
                "resync": len(plan.to_resync),
                "artwork": len(plan.artwork_to_copy),
                "delete": len(plan.to_delete),
                "up_to_date": len(plan.up_to_date),
            },
            "errors": list(plan.errors),
            "playlist_sync": {},
        }

        if plan.errors or plan.total_operations == 0:
            report["execution_profile"] = {}
            report["post_sync_cache_update_seconds"] = 0.0
        else:
            worker = SyncWorker(plan, device.mount_path, config.db_path)
            results = {}
            worker.finished.connect(lambda copied, failed, skipped: results.update(copied=copied, failed=failed, skipped=skipped))
            worker.run()
            cache_started = time.perf_counter()
            if not args.skip_cache_update:
                engine.apply_successful_sync_to_cache(plan)
                if config.get("sync_playlists_to_device", True):
                    report["playlist_sync"] = export_device_playlists(db, device)
                    db.commit()
                report["pictureflow_cache"] = invalidate_pictureflow_cache(device)
                report["post_sync_cache_update_seconds"] = time.perf_counter() - cache_started
            else:
                report["post_sync_cache_update_seconds"] = 0.0
            report["execution_profile"] = dict(plan.execution_profile or {})
            report["results"] = results

        if args.json:
            print(json.dumps(report, indent=2, sort_keys=True))
        else:
            print("RockPod sync profile")
            print(f"Device: {report['device_path']}")
            print(f"Plan: {report['plan_summary']}")
            print(f"Operations: {report['operations']}")
            print(f"Plan timing: {report['plan_profile']}")
            print(f"Execution timing: {report.get('execution_profile', {})}")
            print(f"Post-sync cache update: {report.get('post_sync_cache_update_seconds', 0.0):.3f}s")
            if report.get("results"):
                print(f"Results: {report['results']}")
            if report["errors"]:
                print(f"Errors: {report['errors']}")
        return 0
    finally:
        artwork.shutdown()
        db.close()


if __name__ == "__main__":
    raise SystemExit(main())
