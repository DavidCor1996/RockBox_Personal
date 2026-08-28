#!/usr/bin/env python3
"""Sync only Rani's reviewed playlist without a full-device hash scan."""

import argparse
import json
import os
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from app.config import Config  # noqa: E402
from app.database import Database  # noqa: E402
from services.artwork_manager import ArtworkManager  # noqa: E402
from services.albumlist_export import generate_albumlist_art  # noqa: E402
from services.device_detector import DeviceDetector, DeviceInfo  # noqa: E402
from services.rockbox_device import invalidate_pictureflow_cache  # noqa: E402
from services.rockbox_playlists import export_device_playlists  # noqa: E402
from services.rockbox_wps_art import wps_album_art_sizes_for_config  # noqa: E402
from services.sync_engine import SyncEngine, SyncPlan, SyncWorker, build_device_path  # noqa: E402


PLAYLIST_NAME = "Rani's Playlist"


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device-path", required=True)
    parser.add_argument("--config", default="")
    parser.add_argument("--db", default="")
    parser.add_argument("--albumlist-only", action="store_true")
    return parser.parse_args()


def main():
    args = parse_args()
    mount = Path(args.device_path).resolve()
    if not mount.is_dir():
        raise SystemExit(f"Device mount is unavailable: {mount}")

    config = Config(args.config or None)
    if args.db:
        config.db_path = args.db
    config.mock_device_enabled = False
    config.device_mount_path = str(mount)

    db = Database(config.db_path)
    artwork = ArtworkManager(config.artwork_cache_dir, config)
    try:
        if args.albumlist_only:
            report = generate_albumlist_art(
                db,
                config,
                output_root=Path(config.cache_dir) / "albumlist_export",
                mount=str(mount),
                force=True,
                synced_only=True,
            )
            print(json.dumps({"albumlist": report}, indent=2, ensure_ascii=False))
            return 0
        playlist = db.fetchone(
            "SELECT id,name FROM playlists WHERE lower(name)=lower(?)", (PLAYLIST_NAME,)
        )
        if not playlist:
            raise RuntimeError(f"Playlist not found: {PLAYLIST_NAME}")
        rows = [dict(row) for row in db.fetchall(
            "SELECT t.* FROM playlist_tracks pt JOIN tracks t ON t.id=pt.track_id "
            "WHERE pt.playlist_id=? ORDER BY pt.position,pt.id", (playlist["id"],)
        )]
        if len(rows) != 31:
            raise RuntimeError(f"Expected 31 reviewed tracks, found {len(rows)}")

        detector = DeviceDetector(config)
        device = DeviceInfo(str(mount))
        detector._current_device = device
        engine = SyncEngine(db, config, detector, artwork)
        engine.set_current_device(device)

        dir_template = config.get_effective(
            "device_music_template", device=device, default="Music/{album_artist}/{album}"
        )
        file_template = config.get_effective(
            "device_file_template", device=device, default="{track_number:02d} - {title}{ext}"
        )
        plan = SyncPlan()
        album_groups = defaultdict(lambda: {"tracks": [], "device_dirs": set()})
        destinations = []
        for row in rows:
            source = Path(row["file_path"])
            if not source.is_file():
                raise RuntimeError(f"Missing local source: {source}")
            destination = build_device_path(row, dir_template, file_template)
            old_path = str(row.get("device_path") or "")
            plan.to_resync.append((row, old_path, destination))
            plan.total_bytes += source.stat().st_size
            key = row.get("album_group_key") or (
                f"{row.get('album_artist') or row.get('artist')}\0{row.get('album')}"
            )
            group = album_groups[key]
            group["group_key"] = key
            group["album"] = row.get("album") or "Unknown Album"
            group["artist"] = row.get("album_artist") or row.get("artist") or "Unknown Artist"
            group["tracks"].append(row)
            group["device_dirs"].add(os.path.dirname(destination))
            destinations.append({"track_id": row["id"], "old": old_path, "new": destination})

        fit_mode = config.get_effective("wps_cover_fit_mode", device=device, default="contain")
        wps_sizes = wps_album_art_sizes_for_config(config, str(mount))
        for key, group in album_groups.items():
            cover_src, _cover_hash = artwork.export_device_cover(group)
            if not cover_src:
                raise RuntimeError(f"No cover resolved for {group['artist']} - {group['album']}")
            for device_dir in sorted(group["device_dirs"]):
                plan.artwork_to_copy.append((cover_src, os.path.join(device_dir, "cover.jpg"), key))
            for width, height in wps_sizes:
                wps_src, _wps_hash = artwork.export_rockbox_wps_cover(
                    group, (width, height), fit_mode=fit_mode
                )
                if not wps_src:
                    continue
                for device_dir in sorted(group["device_dirs"]):
                    plan.artwork_to_copy.append((
                        wps_src, os.path.join(device_dir, f"cover.{width}x{height}.bmp"), key
                    ))

        result = {"copied": 0, "failed": 0, "skipped": 0, "fatal": ""}
        worker = SyncWorker(plan, str(mount), config.db_path)
        worker.finished.connect(
            lambda copied, failed, skipped: result.update(
                copied=copied, failed=failed, skipped=skipped
            )
        )
        worker.fatal_error.connect(lambda message: result.update(fatal=str(message)))
        worker.run()
        if result["fatal"] or result["failed"]:
            raise RuntimeError(result["fatal"] or f"{result['failed']} copy operations failed")

        engine.apply_successful_sync_to_cache(plan)
        playlist_report = export_device_playlists(db, device)
        db.commit()
        pictureflow = invalidate_pictureflow_cache(device)
        albumlist = generate_albumlist_art(
            db,
            config,
            output_root=Path(config.cache_dir) / "albumlist_export",
            mount=str(mount),
            force=True,
            synced_only=True,
        )
        print(json.dumps({
            "playlist": PLAYLIST_NAME,
            "tracks": len(rows),
            "albums": len(album_groups),
            "plan": plan.summary(),
            "result": result,
            "destinations": destinations,
            "playlist_export": playlist_report,
            "pictureflow": pictureflow,
            "albumlist": albumlist,
        }, indent=2, ensure_ascii=False))
        return 0
    finally:
        artwork.shutdown()
        db.close()


if __name__ == "__main__":
    raise SystemExit(main())
