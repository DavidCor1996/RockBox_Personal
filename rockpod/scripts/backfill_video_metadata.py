#!/usr/bin/env python3
"""Repair and complete the video library's metadata and artwork.

Three passes, all dry-run unless ``--apply`` is given:

1. ``rescan``   re-derives show, season, episode and title from the filename
                for rows whose stored values are release-group noise, a
                season-pack label or a "Pre-Season" folder read as a series.
2. ``backfill`` resolves each series and film against the online catalogue and
                fills every empty field, then downloads the real show and
                season covers into ``assets/imdb_video_artwork``.
3. ``rename``   with ``--device`` given, renames the matching folders on a
                mounted iPod in place and rewrites the stored device paths and
                the Netflix watched list, so a corrected show name costs a
                directory rename instead of a multi-gigabyte re-copy.
"""

import argparse
import os
import re
import shutil
import sqlite3
import sys
import time
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from services.metadata_reader import (  # noqa: E402
    read_metadata,
    looks_like_release_group,
    looks_like_specials_folder,
)
from services.video_metadata_backfill import (  # noqa: E402
    MovieMetadataBackfill,
    VideoMetadataBackfill,
    group_show_rows,
)

DEFAULT_DB = Path.home() / ".rockpod" / "library.db"
BACKUP_DIR = Path.home() / ".rockpod" / "metadata-backups"
PATH_FIELDS = (
    "show_title", "season_number", "episode_number", "title", "album",
    "artist", "album_artist", "video_kind",
)


def open_db(path):
    connection = sqlite3.connect(str(path))
    connection.row_factory = sqlite3.Row
    return connection


def video_rows(connection):
    return [
        dict(row)
        for row in connection.execute(
            "SELECT * FROM tracks WHERE media_type = 'video' ORDER BY show_title, season_number, episode_number"
        )
    ]


def needs_rescan(row):
    """True when the stored identity predates the current filename rules."""
    if row.get("metadata_locked"):
        return False
    title = str(row.get("title") or "")
    show = str(row.get("show_title") or "")
    if not show and str(row.get("video_kind") or "") == "show":
        return True
    if looks_like_release_group(title) or looks_like_release_group(show):
        return True
    if any(
        re.search(r"(?i)\bseasons?\s+\d{1,2}\s*(?:-|to|thru|through)\s*\d{1,2}\b", value)
        for value in (title, show, str(row.get("album") or ""))
        if value
    ):
        return True
    parent = os.path.basename(os.path.dirname(str(row.get("file_path") or "")))
    if looks_like_specials_folder(parent) and (row.get("season_number") or 0):
        return True
    return False


def rescan_updates(rows):
    updates = []
    for row in rows:
        if not needs_rescan(row):
            continue
        path = str(row.get("file_path") or "")
        if not path or not os.path.isfile(path):
            continue
        try:
            track = read_metadata(path)
        except Exception as exc:  # a single unreadable file must not stop the run
            print(f"  ! could not re-read {path}: {exc}")
            continue
        values = {}
        for field in PATH_FIELDS:
            new = getattr(track, field, None)
            if new in (None, "") or new == row.get(field):
                continue
            values[field] = new
        if values:
            updates.append({"id": row["id"], "file_path": path, "values": values})
    return updates


def apply_updates(connection, updates):
    for update in updates:
        values = update["values"]
        assignments = ", ".join(f"{name} = ?" for name in values)
        connection.execute(
            f"UPDATE tracks SET {assignments} WHERE id = ?",
            tuple(values.values()) + (update["id"],),
        )


def backup_database(path):
    BACKUP_DIR.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%d-%H%M%S")
    target = BACKUP_DIR / f"library.db.bak-video-metadata-{stamp}"
    shutil.copy2(path, target)
    return target


def describe(updates, limit=12):
    for update in updates[:limit]:
        name = os.path.basename(update["file_path"])
        fields = ", ".join(
            f"{key}={value!r}"
            for key, value in update["values"].items()
            if key not in {"metadata_hash", "metadata_confidence"}
        )
        print(f"  {name[:58]:60} {fields[:150]}")
    if len(updates) > limit:
        print(f"  ... and {len(updates) - limit} more")


DEVICE_DIR_TEMPLATE = "Music/{album_artist}/{album}"
DEVICE_FILE_TEMPLATE = "{track_number:02d} - {title}{ext}"


def device_moves(device_mount, rows):
    """Per-file moves that bring the iPod in line with the repaired metadata.

    Working file by file rather than folder by folder lets a corrected show
    merge into a folder that already exists - "Pre" joining Trailer Park Boys -
    and keeps every move a same-volume rename instead of a re-copy of the
    source video.
    """
    from services.sync_engine import build_device_path

    mount = Path(device_mount)
    moves = []
    for row in rows:
        old_rel = str(row.get("device_path") or "").strip()
        if not old_rel or str(row.get("video_kind") or "") != "show":
            continue
        source = mount / old_rel
        if not source.is_file():
            continue
        candidate = dict(row)
        candidate["sync_output_ext"] = source.suffix
        new_rel = build_device_path(candidate, DEVICE_DIR_TEMPLATE, DEVICE_FILE_TEMPLATE)
        if not new_rel or new_rel == old_rel:
            continue
        moves.append({"id": row["id"], "old": old_rel, "new": new_rel})
    return moves


def apply_device_moves(connection, device_mount, moves):
    mount = Path(device_mount)
    moved = 0
    for move in moves:
        source = mount / move["old"]
        target = mount / move["new"]
        if not source.is_file():
            continue
        case_only = (
            target.exists()
            and str(source).casefold() == str(target).casefold()
        )
        if target.exists() and not case_only:
            print(f"  ! target already present, skipping: {move['new']}")
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        if case_only:
            # The device volume is case-insensitive, so a capitalisation fix
            # has to go through a scratch name or it looks like a clash.
            scratch = source.with_name(source.name + ".case-fix")
            os.rename(source, scratch)
            os.rename(scratch, target)
        else:
            os.rename(source, target)
        for sidecar in (".bmp", ".jpg", ".rvp.json"):
            extra = source.with_suffix(sidecar)
            if extra.is_file():
                os.rename(extra, target.with_suffix(sidecar))
        connection.execute(
            "UPDATE tracks SET device_path = ? WHERE id = ?", (move["new"], move["id"])
        )
        remap_watched_list(mount, move["old"], move["new"])
        moved += 1
    prune_empty_show_dirs(mount)
    return moved


def realign_device_track_paths(connection, device_mount, rows):
    """Point the device index at the files' new locations.

    ``device_tracks`` is the sync planner's picture of what is on the iPod. It
    is keyed by device path, so moving a file without updating it leaves the
    planner believing the old path is still there and the new one is missing -
    which queues a re-copy of content that is already correct.

    Realigning has to come before any cleanup: an entry whose path no longer
    exists is usually a file that moved, not one that was deleted, and dropping
    it first would throw away the very row that needed the new path.
    """
    mount = Path(device_mount)
    realigned = 0
    for row in rows:
        device_path = str(row.get("device_path") or "").strip()
        if not device_path or not (mount / device_path).is_file():
            continue
        existing = connection.execute(
            "SELECT id, device_id, device_path FROM device_tracks WHERE local_track_id = ?",
            (row["id"],),
        ).fetchone()
        if existing is None or str(existing["device_path"]) == device_path:
            continue
        connection.execute(
            "DELETE FROM device_tracks WHERE device_id = ? AND device_path = ? AND id != ?",
            (existing["device_id"], device_path, existing["id"]),
        )
        connection.execute(
            "UPDATE device_tracks SET device_path = ?, present_on_device = 1 WHERE id = ?",
            (device_path, existing["id"]),
        )
        realigned += 1

    removed = 0
    for entry in connection.execute(
        "SELECT id, device_path FROM device_tracks WHERE device_path LIKE 'Videos/%'"
    ).fetchall():
        if not (mount / str(entry["device_path"])).exists():
            connection.execute("DELETE FROM device_tracks WHERE id = ?", (entry["id"],))
            removed += 1

    # A capitalisation fix leaves a second entry behind: the device volume is
    # case-insensitive, so the old spelling still "exists" as a path even
    # though it names the same file.
    for row in rows:
        device_path = str(row.get("device_path") or "").strip()
        if not device_path:
            continue
        cursor = connection.execute(
            "DELETE FROM device_tracks WHERE local_track_id = ? AND device_path != ?",
            (row["id"], device_path),
        )
        removed += cursor.rowcount or 0
    return realigned, removed


def restamp_synced_hashes(connection, device_mount, rows):
    """Record the repaired metadata as already synced for present files.

    Video metadata lives in ``index.tsv``, not inside the media files, and the
    device copies were moved into place above - so a metadata repair does not
    make the copy on the iPod stale. Without this the next sync would see 200+
    changed fingerprints and re-copy gigabytes that are already correct.
    """
    mount = Path(device_mount)
    restamped = 0
    for row in rows:
        if not row.get("synced_to_device"):
            continue
        device_path = str(row.get("device_path") or "").strip()
        metadata_hash = str(row.get("metadata_hash") or "")
        if not device_path or not metadata_hash:
            continue
        if str(row.get("last_synced_metadata_hash") or "") == metadata_hash:
            continue
        if not (mount / device_path).is_file():
            continue
        connection.execute(
            "UPDATE tracks SET last_synced_metadata_hash = ? WHERE id = ?",
            (metadata_hash, row["id"]),
        )
        restamped += 1
    return restamped


def prune_empty_show_dirs(mount):
    root = mount / "Videos" / "TV Shows"
    if not root.is_dir():
        return
    for directory in sorted(root.rglob("*"), key=lambda p: len(p.parts), reverse=True):
        if directory.is_dir() and not any(directory.iterdir()):
            directory.rmdir()


def remap_watched_list(mount, old_rel, new_rel):
    """Keep watched history attached to a file whose device path changed."""
    watched = mount / ".rockbox" / "videolist" / "netflix-watched.tsv"
    if not watched.is_file():
        return
    text = watched.read_text(encoding="utf-8", errors="replace")
    updated = text.replace(f"/{old_rel}", f"/{new_rel}")
    if updated != text:
        watched.write_text(updated, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", default=str(DEFAULT_DB))
    parser.add_argument("--apply", action="store_true", help="write the changes")
    parser.add_argument("--device", default="", help="mounted iPod to rename folders on")
    parser.add_argument("--only", default="", help="limit to shows matching this text")
    parser.add_argument("--skip-rescan", action="store_true")
    parser.add_argument("--skip-artwork", action="store_true")
    parser.add_argument("--skip-movies", action="store_true")
    args = parser.parse_args()

    db_path = Path(args.db)
    if not db_path.is_file():
        parser.error(f"library database not found: {db_path}")
    if args.apply:
        print(f"database backup: {backup_database(db_path)}")

    connection = open_db(db_path)
    rows = video_rows(connection)
    print(f"video rows: {len(rows)}")

    if not args.skip_rescan:
        updates = rescan_updates(rows)
        print(f"\n== filename rescan == {len(updates)} rows")
        describe(updates)
        if args.apply and updates:
            apply_updates(connection, updates)
            connection.commit()
            rows = video_rows(connection)
        elif updates:
            # A dry run has to plan against the rescanned identity, or it
            # reports what the provider would return for the broken name.
            by_id = {row["id"]: row for row in rows}
            for update in updates:
                by_id[update["id"]].update(update["values"])

    backfill = VideoMetadataBackfill(dry_run=not args.apply)
    groups = group_show_rows(rows)
    total = 0
    print(f"\n== series backfill == {len(groups)} series")
    for key in sorted(groups):
        group = groups[key]
        if args.only and args.only.casefold() not in group.title.casefold():
            continue
        plan = backfill.plan_show(group, fetch_artwork=not args.skip_artwork)
        if not plan:
            print(f"  {group.title[:40]:42} no confident match - left untouched")
            continue
        entry = plan["entry"]
        seasons = len(entry.get("seasons") or {})
        print(
            f"  {group.title[:40]:42} -> {entry['title']} "
            f"({entry.get('imdb_id') or 'no id'}) art={'yes' if entry.get('show_art') else 'NO'} "
            f"seasons={seasons} rows={len(plan['updates'])}"
        )
        total += len(plan["updates"])
        if args.apply and plan["updates"]:
            apply_updates(connection, plan["updates"])

    if not args.skip_movies:
        from app.config import Config
        from services.online_video_metadata import VideoMetadataService

        config = Config()
        service = VideoMetadataService(config=config)
        movies = MovieMetadataBackfill(service, dry_run=not args.apply)
        movie_rows = [r for r in rows if str(r.get("video_kind") or "") == "movie"]
        print(f"\n== film backfill == {len(movie_rows)} films")
        if not str(config.get("omdb_api_key", "") or "").strip():
            # Apple withdrew films from the iTunes Search API, so OMDb - the
            # IMDb-sourced catalogue - is the only film provider left here.
            print(
                "  no omdb_api_key in ~/.rockpod/config.json: films need a free\n"
                "  OMDb key (https://www.omdbapi.com/apikey.aspx) before their\n"
                "  IMDb posters and synopses can be fetched. Skipping films."
            )
            movie_rows = []
        for row in movie_rows:
            if args.only and args.only.casefold() not in str(row.get("title") or "").casefold():
                continue
            plan = movies.plan_movie(row, fetch_artwork=not args.skip_artwork)
            if not plan:
                print(f"  {str(row.get('title'))[:40]:42} no confident match - left untouched")
                continue
            entry = plan["entry"]
            print(
                f"  {str(row.get('title'))[:40]:42} -> {entry['title']} "
                f"({entry.get('imdb_id') or 'no id'}) art={'yes' if entry.get('show_art') else 'NO'}"
            )
            total += len(plan["updates"])
            if args.apply and plan["updates"]:
                apply_updates(connection, plan["updates"])
        if args.apply:
            movies.save_catalog()

    if args.apply:
        backfill.save_catalog()
        connection.commit()
    print(f"\nrows updated: {total}")

    if args.device:
        rows = video_rows(connection)
        moves = device_moves(args.device, rows)
        print(f"\n== device file moves == {len(moves)}")
        for move in moves[:12]:
            print(f"  {move['old']}\n    -> {move['new']}")
        if len(moves) > 12:
            print(f"  ... and {len(moves) - 12} more")
        if args.apply and moves:
            moved = apply_device_moves(connection, args.device, moves)
            connection.commit()
            print(f"  moved {moved} files in place")
        if args.apply:
            current = video_rows(connection)
            realigned, removed = realign_device_track_paths(
                connection, args.device, current
            )
            restamped = restamp_synced_hashes(connection, args.device, current)
            connection.commit()
            print(
                f"  realigned {realigned} device index rows, "
                f"dropped {removed} pointing at moved files"
            )
            print(f"  marked {restamped} already-correct device files as synced")

    connection.close()
    if not args.apply:
        print("\ndry run - nothing was written. Re-run with --apply.")


if __name__ == "__main__":
    main()
