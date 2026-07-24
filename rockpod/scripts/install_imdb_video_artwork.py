#!/usr/bin/env python3
"""Install the verified personal IMDb posters and repair known episode IDs."""

import argparse
import os
import sqlite3
import sys
from datetime import datetime
from pathlib import Path


ROCKPOD_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, os.fspath(ROCKPOD_ROOT))

from app.database import Database  # noqa: E402
from services.imdb_video_artwork import IMDbVideoArtworkCatalog  # noqa: E402


_METADATA_REPAIRS = {
    "who can stand up the longest_ - kenny vs. spenny (hd).mpg": {
        "title": "Who Can Stand Up the Longest?",
        "show_title": "Kenny vs. Spenny",
        "season_number": 1,
        "episode_number": 4,
        "year": 2003,
        "imdb_id": "tt0384746",
    },
    "episode 1 _ hey arnold _ full episode _ retro rerun.mpg": {
        "title": "Downtown as Fruits/Eugene's Bike",
        "show_title": "Hey Arnold!",
        "season_number": 1,
        "episode_number": 1,
        "year": 1996,
        "imdb_id": "tt0115200",
    },
    "episode 2 _ hey arnold _ full episode _ retro rerun.mpg": {
        "title": "The Little Pink Book/Field Trip",
        "show_title": "Hey Arnold!",
        "season_number": 1,
        "episode_number": 2,
        "year": 1996,
        "imdb_id": "tt0115200",
    },
    "disney's recess - the spy who came in from the playground.mpg": {
        "title": "The Spy Who Came in from the Playground",
        "show_title": "Recess",
        "season_number": 4,
        "episode_number": 4,
        "year": 1999,
        "imdb_id": "tt0126170",
    },
    "episode 54 - 6teen _full episode_ retro rerun.mpg": {
        "title": "Baby, You Stink",
        "show_title": "6Teen",
        "season_number": 3,
        "episode_number": 2,
        "year": 2007,
        "imdb_id": "tt0439341",
    },
    "episode 57 - 6teen _full episode_ retro rerun.mpg": {
        "title": "Silent Butt Deadly",
        "show_title": "6Teen",
        "season_number": 3,
        "episode_number": 5,
        "year": 2007,
        "imdb_id": "tt0439341",
    },
}


def _backup_database(db, db_path):
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    backup_path = f"{db_path}.before-imdb-art-{stamp}.bak"
    destination = sqlite3.connect(backup_path)
    try:
        db._conn.backup(destination)
    finally:
        destination.close()
    return backup_path


def run(db_path, apply=False):
    db = Database(db_path)
    catalog = IMDbVideoArtworkCatalog()
    changes = []
    try:
        for row in db.get_tracks_by_media_type("video", order_by="id"):
            track = dict(row)
            if str(track.get("video_kind") or "") not in {"movie", "show"}:
                continue
            updates = dict(
                _METADATA_REPAIRS.get(
                    os.path.basename(str(track.get("file_path") or "")).casefold(),
                    {},
                )
            )
            resolved = dict(track)
            resolved.update(updates)
            scope = "season" if resolved.get("video_kind") == "show" else "movie"
            artwork_path, entry = catalog.resolve(resolved, scope=scope)
            if artwork_path:
                updates["artwork_path"] = artwork_path
                updates["imdb_id"] = str(entry.get("imdb_id") or "")
                updates.setdefault("metadata_source", "IMDb")
                updates.setdefault("metadata_confidence", 1.0)
            updates = {
                key: value for key, value in updates.items()
                if track.get(key) != value
            }
            if updates:
                changes.append((int(track["id"]), str(track.get("title") or ""), updates))

        if apply and changes:
            backup_path = _backup_database(db, db_path)
            for track_id, _title, updates in changes:
                db.update_track_metadata(track_id, updates)
            db.commit()
            print(f"Backup: {backup_path}")
        for track_id, title, updates in changes:
            prefix = "Updated" if apply else "Would update"
            print(f"{prefix} {track_id} {title}: {updates}")
        print(f"IMDb artwork rows {'updated' if apply else 'matched'}: {len(changes)}")
    finally:
        db.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", required=True, help="RockPod SQLite library path")
    parser.add_argument("--apply", action="store_true", help="Back up and update the DB")
    args = parser.parse_args(argv)
    run(os.path.abspath(args.db), apply=args.apply)


if __name__ == "__main__":
    main()
