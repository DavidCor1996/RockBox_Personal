#!/usr/bin/env python3
"""Force-resync RockPod video rows as RVP bundles for the mounted iPod."""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
ROCKPOD_ROOT = REPO_ROOT / "rockpod"
sys.path.insert(0, str(ROCKPOD_ROOT))

from app.config import Config  # noqa: E402
from app.database import Database  # noqa: E402
from services.device_detector import DeviceDetector, DeviceInfo  # noqa: E402
from services.sync_engine import SyncEngine, SyncWorker, _remove_media_with_sidecars  # noqa: E402


def _default_mount(config: Config) -> str:
    selected = config.get("rockbox_selected_profile_id", "")
    for profile in config.get("rockbox_profiles", []) or []:
        if profile.get("id") == selected and profile.get("device_mount_path"):
            return profile["device_mount_path"]
    return config.get("device_mount_path", "")


def _video_ids_to_replace(db: Database):
    rows = db.execute(
        "SELECT id, title, file_path, device_path FROM tracks "
        "WHERE lower(coalesce(media_type, '')) = 'video' "
        "AND (synced_to_device = 1 OR coalesce(device_path, '') != '') "
        "ORDER BY id"
    ).fetchall()
    return [int(row["id"]) for row in rows], [dict(row) for row in rows]


def _write_device_log(mount: str, lines: list[str]):
    log_dir = os.path.join(mount, ".rockbox", "openh264")
    os.makedirs(log_dir, exist_ok=True)
    path = os.path.join(log_dir, "rockpod_rvp_sync.log")
    with open(path, "a", encoding="utf-8") as handle:
        for line in lines:
            handle.write(line.rstrip() + "\n")
    return path


def _remove_replaced_device_files(mount: str, old_rows: list[dict], planned_paths: list[str]) -> list[str]:
    planned = {str(path or "") for path in planned_paths}
    removed = []
    for row in old_rows:
        old_rel = str(row.get("device_path") or "")
        if not old_rel or old_rel in planned:
            continue
        old_full = os.path.join(mount, old_rel)
        for target in _remove_media_with_sidecars(old_full):
            removed.append(os.path.relpath(target, mount))
    return removed


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default=str(Path.home() / ".rockpod" / "config.json"))
    parser.add_argument("--mount", default="")
    parser.add_argument("--ids", default="", help="Comma-separated video track IDs. Defaults to synced videos.")
    args = parser.parse_args(argv)

    config = Config(args.config)
    mount = os.path.abspath(args.mount or _default_mount(config))
    if not mount or not os.path.isdir(mount):
        print(f"Mounted iPod path not found: {mount}", file=sys.stderr)
        return 2

    db = Database(config.db_path)
    ids, rows = _video_ids_to_replace(db)
    if args.ids.strip():
        ids = [int(part.strip()) for part in args.ids.split(",") if part.strip()]
        placeholders = ",".join("?" for _ in ids)
        rows = [
            dict(row)
            for row in db.execute(
                f"SELECT id, title, file_path, device_path FROM tracks WHERE id IN ({placeholders}) ORDER BY id",
                tuple(ids),
            ).fetchall()
        ]
    if not ids:
        print("No synced RockPod video rows found.")
        db.close()
        return 0

    detector = DeviceDetector(config)
    device = DeviceInfo(mount)
    device.name = "Mounted iPod"
    device.is_rockbox = True
    detector._current_device = device

    engine = SyncEngine(db, config, detector)
    engine.set_current_device(device)

    started = time.strftime("%Y-%m-%d %H:%M:%S")
    log_lines = [
        f"started={started}",
        f"mount={mount}",
        f"db={config.db_path}",
        f"ids={','.join(str(track_id) for track_id in ids)}",
    ]
    for row in rows:
        log_lines.append(
            "source id={id} title={title!r} file={file_path} old_device_path={device_path}".format(**row)
        )
    log_path = _write_device_log(mount, log_lines)
    print(f"Device log: {log_path}")

    plan = engine.build_sync_plan(track_ids=set(ids), force_full=True, media_type="video")
    print(plan.summary())
    if plan.errors:
        for error in plan.errors:
            print(f"ERROR: {error}", file=sys.stderr)
        _write_device_log(mount, [f"plan_error={error}" for error in plan.errors])
        db.close()
        return 3

    planned_paths = [path for _row, _old, path in plan.to_resync] + [path for _row, path in plan.to_copy]
    worker = SyncWorker(plan, mount, config.db_path)
    results = {}
    worker.finished.connect(lambda copied, failed, skipped: results.update(copied=copied, failed=failed, skipped=skipped))
    worker.file_error.connect(lambda path, message: print(f"ERROR: {path}: {message}", file=sys.stderr))
    worker.file_copied.connect(lambda src, dest: print(f"copied {os.path.basename(dest)}"))
    worker.run()

    copied = int(results.get("copied", 0))
    failed = int(results.get("failed", 0))
    skipped = int(results.get("skipped", 0))
    removed = []
    if (plan.to_copy or plan.to_resync) and copied and not failed:
        removed = _remove_replaced_device_files(mount, rows, planned_paths)
    finished = time.strftime("%Y-%m-%d %H:%M:%S")
    _write_device_log(
        mount,
        [
            f"finished={finished}",
            f"copied={copied}",
            f"failed={failed}",
            f"skipped={skipped}",
            "planned_paths=" + json.dumps(planned_paths),
            "removed_replaced_paths=" + json.dumps(removed),
        ],
    )
    db.close()
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
