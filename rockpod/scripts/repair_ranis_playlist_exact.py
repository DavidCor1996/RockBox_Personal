#!/usr/bin/env python3
"""Repair Rani's Playlist using an exact, reviewed metadata manifest.

This intentionally never matches by title alone.  It also embeds per-album
artwork in the restored M4A files so a future library scan cannot reapply the
shared restore-folder cover to unrelated albums.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
from datetime import datetime
from pathlib import Path

import mutagen
from mutagen.flac import FLAC, Picture
from mutagen.mp4 import MP4, MP4Cover

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from app.config import Config  # noqa: E402
from app.database import Database  # noqa: E402
from services.metadata_reader import compute_file_hash, extract_artwork_data, read_metadata  # noqa: E402
from services.metadata_writer import write_track_metadata_to_file  # noqa: E402
from services.rockbox_playlists import export_local_music_playlists  # noqa: E402


PLAYLIST_NAME = "Rani's Playlist"
MUSIC_ROOT = Path("/home/david/Music")
RESTORE_ROOT = MUSIC_ROOT / "Ranis Playlist Restore"
RESTORE_ART_ROOT = RESTORE_ROOT / ".rockpod-artwork"
GENERATED_MARKER = "/Playlists/RockPod Media/"


def _entry(title, artist, album, track_number, track_total, year, *,
           album_artist=None, disc_number=1, disc_total=1, artwork_hint=""):
    return {
        "title": title,
        "artist": artist,
        "album": album,
        "album_artist": album_artist or artist,
        "track_number": track_number,
        "track_total": track_total,
        "disc_number": disc_number,
        "disc_total": disc_total,
        "year": year,
        "artwork_hint": artwork_hint,
    }


# Reviewed against Apple Music/iTunes, MusicBrainz and the existing Tidal
# catalog cache.  Keying by artist AND title is important: the library has two
# different songs named "Hold Me Down" and two named "Wake Up".
REVIEWED = [
    _entry("Good Day to Be Alive", "Summer Kennedy", "The Bright Side", 7, 10, 2018,
           artwork_hint="02-8095-Summer Kennedy-The Bright Side-Good Day to Be Alive.jpg"),
    _entry("Wake Up", "Run River North", "Wake Up - Single", 1, 1, 2019),
    _entry("Dog Days Are Over", "Florence + the Machine", "Lungs (Digital Deluxe Version)", 1, 13, 2008,
           disc_total=2, artwork_hint="03-8096-Florence + the Machine-Lungs (Digital Deluxe Version)-Dog Days Are Over.jpg"),
    _entry("Sway", "Fitz and The Tantrums", "Let Yourself Free (Deluxe)", 3, 18, 2022,
           artwork_hint="04-8097-Fitz and The Tantrums-Let Yourself Free (Deluxe)-Sway.jpg"),
    _entry("Hold Me Down", "The Happy Fits", "What Could Be Better", 6, 10, 2020),
    _entry("only wanna dance", "almost monday", "Endless Summer 2023 - EP", 5, 6, 2023,
           artwork_hint="06-8098-almost monday-Endless Summer 2023 - EP-only wanna dance.jpg"),
    _entry("Good Company", "Andy Grammer", "Monster (Deluxe)", 23, 27, 2022,
           artwork_hint="07-8099-Andy Grammer-Monster (Deluxe)-Good Company.jpg"),
    _entry("color bomb", "coloia", "i.", 2, 7, 2025,
           artwork_hint="08-8100-coloia-i-color bomb.jpg"),
    _entry("Rosewood", "Morningsiders", "I've Got a Song", 5, 10, 2022,
           artwork_hint="09-8101-Morningsiders-I've Got a Song-Rosewood.jpg"),
    _entry("Like a Radio", "Keelan Donovan", "Like a Radio - Single", 1, 1, 2018,
           artwork_hint="10-8102-Keelan Donovan-Like a Radio - Single-Like a Radio.jpg"),
    _entry("Tease Me", "Nicky Youre", "Tease Me - Single", 1, 1, 2024,
           artwork_hint="11-8103-Nicky Youre-Tease Me - Single-Tease Me.jpg"),
    _entry("Freida", "Morningsiders", "A Little Lift", 8, 12, 2018,
           artwork_hint="12-8104-Morningsiders-A Little Lift-Freida.jpg"),
    _entry("You + the Sun", "Old Daisy", "Truce - EP", 1, 6, 2016,
           artwork_hint="13-8105-Old Daisy-Truce - EP-You + the Sun.jpg"),
    _entry("Sun-Glo", "Jefferson Clay", "Sun-Glo - Single", 1, 1, 2021,
           artwork_hint="14-8106-Jefferson Clay-Sun-Glo - Single-Sun-Glo.jpg"),
    _entry("My My", "MAGIC GIANT", "The Valley", 2, 13, 2021,
           artwork_hint="15-8107-MAGIC GIANT-The Valley-My My.jpg"),
    _entry("I Just Wanna Shine", "Fitz and The Tantrums", "All the Feels", 3, 17, 2019,
           artwork_hint="16-8108-Fitz and The Tantrums-All the Feels-I Just Wanna Shine.jpg"),
    _entry("Chasin' Honey", "Wild Party", "Phantom Pop", 7, 12, 2014,
           artwork_hint="17-8109-Wild Party-Phantom Pop-Chasin' Honey.jpg"),
    _entry("come on come on", "almost monday", "Endless Summer 2023 - EP", 6, 6, 2020,
           artwork_hint="18-8110-almost monday-Endless Summer 2023 - EP-come on come on.jpg"),
    _entry("Take That", "CRUISR", "Take That - Single", 1, 2, 2016),
    _entry("Ace Up My Sleeve", "Lord Huron & Ben Schneider",
           "Music for The Starling Girl (Score & Music from the Original Motion Picture)", 14, 14, 2023,
           artwork_hint="20-8111-Lord Huron, Ben Schneider-Music for The Starling Girl (Score & Music from the Original Motion Picture)-Ace Up My Sleeve.jpg"),
    _entry("Yes I'm A Mess", "AJR", "The Maybe Man", 3, 12, 2023),
    _entry("Oldest I've Ever Felt", "Late Night Thoughts", "Oldest I've Ever Felt - EP", 1, 4, 2025),
    _entry("Mirror", "Scrawny", "Mirror - Single", 1, 1, 2020),
    _entry("All These Things That I've Done", "The Killers", "Direct Hits (Deluxe)", 4, 18, 2004,
           artwork_hint="24-8112-The Killers-Direct Hits (Deluxe)-All These Things That I've Done.jpg"),
    _entry("Figure It Out", "Birds of Bellwoods", "Everything You Want", 6, 10, 2023),
    _entry("SUPERBLOOM", "MisterWives", "SUPERBLOOM", 19, 19, 2020,
           artwork_hint="26-8113-MisterWives-SUPERBLOOM-SUPERBLOOM.jpg"),
    _entry("Still Smiling", "Andy Grammer", "Monster", 9, 11, 2024),
    _entry("Make a Mess", "Matt and Kim", "New Glow", 5, 10, 2015,
           artwork_hint="28-8114-Matt, Kim-New Glow-Make A Mess.jpg"),
    _entry("Left Hand Free", "alt-J", "This Is All Yours", 5, 14, 2014,
           artwork_hint="29-8115-alt-J-This Is All Yours-Left Hand Free.jpg"),
    _entry("How You Love", "BYRNE", "How You Love - Single", 1, 1, 2026),
    _entry("Hall & Oates", "Wild Party", "Get Up - EP", 5, 6, 2022),
]


ALIASES = {
    ("make a mess", "matt, kim"): ("make a mess", "matt and kim"),
    ("ace up my sleeve", "lord huron, ben schneider"):
        ("ace up my sleeve", "lord huron & ben schneider"),
}


def norm(value):
    value = str(value or "").replace("’", "'").replace("‘", "'").strip().casefold()
    return re.sub(r"\s+", " ", value)


MANIFEST = {(norm(row["title"]), norm(row["artist"])): row for row in REVIEWED}


def manifest_for(title, artist):
    key = (norm(title), norm(artist))
    return MANIFEST.get(ALIASES.get(key, key))


def is_generated(path):
    return GENERATED_MARKER.casefold() in str(path or "").replace("\\", "/").casefold()


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def backup_database(db_path, backup_root):
    backup_root.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    target = backup_root / f"library-before-rani-repair-{stamp}.db"
    shutil.copy2(db_path, target)
    return target


def _copy_synced_state(db, old_id, new_id):
    old = db.get_track_by_id(old_id)
    new = db.get_track_by_id(new_id)
    if not old or not new or not old["synced_to_device"] or new["synced_to_device"]:
        return
    db.execute(
        "UPDATE tracks SET synced_to_device=1, device_path=?, "
        "last_synced_metadata_hash=?, last_synced_file_hash=? WHERE id=?",
        (old["device_path"], old["last_synced_metadata_hash"], old["last_synced_file_hash"], new_id),
    )


def consolidate_generated_duplicates(db, dry_run=False):
    generated = [dict(row) for row in db.fetchall(
        "SELECT * FROM tracks WHERE media_type='audio' AND file_path LIKE ?",
        (f"%{GENERATED_MARKER.strip('/')}%",),
    )]
    consolidated = []
    for duplicate in generated:
        candidates = [dict(row) for row in db.fetchall(
            "SELECT * FROM tracks WHERE media_type='audio' AND id<>? "
            "AND lower(title)=lower(?) AND lower(artist)=lower(?) "
            "AND abs(COALESCE(duration,0)-?)<=2",
            (duplicate["id"], duplicate["title"], duplicate["artist"], duplicate["duration"] or 0),
        ) if not is_generated(row["file_path"]) and os.path.isfile(row["file_path"])]
        if not candidates:
            continue
        candidates.sort(key=lambda row: (
            Path(row["file_path"]).suffix.lower() not in {".flac", ".aiff", ".aif"}, row["id"]
        ))
        canonical = candidates[0]
        refs = db.fetchall("SELECT id,playlist_id FROM playlist_tracks WHERE track_id=?", (duplicate["id"],))
        consolidated.append({
            "duplicate_id": duplicate["id"], "canonical_id": canonical["id"],
            "title": duplicate["title"], "artist": duplicate["artist"], "refs": len(refs),
        })
        if dry_run:
            continue
        for ref in refs:
            already = db.fetchone(
                "SELECT id FROM playlist_tracks WHERE playlist_id=? AND track_id=?",
                (ref["playlist_id"], canonical["id"]),
            )
            if already:
                db.execute("DELETE FROM playlist_tracks WHERE id=?", (ref["id"],))
            else:
                db.execute("UPDATE playlist_tracks SET track_id=? WHERE id=?", (canonical["id"], ref["id"]))
        _copy_synced_state(db, duplicate["id"], canonical["id"])
        db.execute("DELETE FROM tracks WHERE id=?", (duplicate["id"],))
    return consolidated


def _artwork_path(entry):
    hint = entry.get("artwork_hint") or ""
    path = RESTORE_ART_ROOT / hint if hint else None
    return path if path and path.is_file() else None


def embed_artwork(media_path, artwork_path):
    data = artwork_path.read_bytes()
    mime = "image/png" if artwork_path.suffix.lower() == ".png" else "image/jpeg"
    audio = mutagen.File(media_path)
    if isinstance(audio, MP4):
        if audio.tags is None:
            audio.add_tags()
        image_format = MP4Cover.FORMAT_PNG if mime == "image/png" else MP4Cover.FORMAT_JPEG
        audio.tags["covr"] = [MP4Cover(data, imageformat=image_format)]
        audio.save()
    elif isinstance(audio, FLAC):
        picture = Picture()
        picture.type = 3
        picture.mime = mime
        picture.desc = "Cover (front)"
        picture.data = data
        audio.clear_pictures()
        audio.add_picture(picture)
        audio.save()
    else:
        raise RuntimeError(f"Artwork embedding is unsupported for {media_path}")


def repair_tracks(db, playlist_id, dry_run=False):
    rows = [dict(row) for row in db.fetchall(
        "SELECT pt.id playlist_track_id,pt.position,t.* FROM playlist_tracks pt "
        "JOIN tracks t ON t.id=pt.track_id WHERE pt.playlist_id=? ORDER BY pt.position,pt.id",
        (playlist_id,),
    )]
    report = []
    seen = set()
    for position, row in enumerate(rows, 1):
        entry = manifest_for(row["title"], row["artist"])
        if not entry:
            raise RuntimeError(f"No exact reviewed metadata for {row['artist']} - {row['title']}")
        key = (norm(entry["title"]), norm(entry["artist"]))
        if key in seen:
            raise RuntimeError(f"Duplicate playlist identity after repair: {entry['artist']} - {entry['title']}")
        seen.add(key)
        media_path = Path(row["file_path"])
        if not media_path.is_file():
            raise RuntimeError(f"Missing source media: {media_path}")
        updates = {field: entry[field] for field in (
            "title", "artist", "album", "album_artist", "year", "track_number",
            "track_total", "disc_number", "disc_total",
        )}
        art_path = _artwork_path(entry)
        if art_path is None:
            embedded_data, _embedded_mime = extract_artwork_data(str(media_path))
            folder_cover = media_path.parent / "cover.jpg"
            if not embedded_data and folder_cover.is_file():
                art_path = folder_cover
        report.append({
            "position": position, "track_id": row["id"], "path": str(media_path),
            "before": {field: row.get(field) for field in updates}, "after": updates,
            "artwork": str(art_path or "embedded"),
        })
        if dry_run:
            continue
        write_track_metadata_to_file(str(media_path), updates)
        if art_path:
            embed_artwork(media_path, art_path)
        db.update_track_metadata(row["id"], {
            **updates,
            "artwork_path": str(art_path) if art_path else row.get("artwork_path") or "",
            "has_embedded_artwork": 1,
            "metadata_source": "rani-reviewed-exact",
            "metadata_confidence": 1.0,
        })
        db.execute("UPDATE tracks SET file_hash=? WHERE id=?", (compute_file_hash(str(media_path)), row["id"]))
        db.execute("UPDATE playlist_tracks SET position=? WHERE id=?", (position, row["playlist_track_id"]))
    if len(report) != len(REVIEWED):
        raise RuntimeError(f"Expected {len(REVIEWED)} playlist tracks, found {len(report)}")
    return report


def quarantine_shared_cover(dry_run=False):
    shared = RESTORE_ROOT / "cover.jpg"
    backup = RESTORE_ROOT / "cover.jpg.shared-sun-glo-backup"
    if not shared.exists():
        return "already absent"
    if dry_run:
        return f"would move to {backup}"
    if backup.exists() and sha256(shared) == sha256(backup):
        shared.unlink()
    elif backup.exists():
        raise RuntimeError(f"Refusing to overwrite different backup: {backup}")
    else:
        shared.replace(backup)
    return str(backup)


def verify(db, playlist_id):
    rows = [dict(row) for row in db.fetchall(
        "SELECT pt.position,t.* FROM playlist_tracks pt JOIN tracks t ON t.id=pt.track_id "
        "WHERE pt.playlist_id=? ORDER BY pt.position,pt.id", (playlist_id,),
    )]
    failures = []
    artwork_hashes = {}
    for expected_position, row in enumerate(rows, 1):
        entry = manifest_for(row["title"], row["artist"])
        if not entry:
            failures.append(f"unreviewed identity: {row['artist']} - {row['title']}")
            continue
        actual = read_metadata(row["file_path"])
        for field in ("title", "artist", "album", "album_artist", "year", "track_number", "track_total", "disc_number", "disc_total"):
            if getattr(actual, field) != entry[field]:
                failures.append(f"{entry['artist']} - {entry['title']}: {field}={getattr(actual, field)!r}, expected {entry[field]!r}")
        if row["position"] != expected_position:
            failures.append(f"position {row['position']} should be {expected_position}")
        data, _mime = extract_artwork_data(row["file_path"])
        if not data:
            failures.append(f"missing embedded artwork: {row['file_path']}")
        else:
            artwork_hashes.setdefault(hashlib.sha256(data).hexdigest(), []).append(
                f"{entry['artist']} - {entry['album']}"
            )
    suspicious = [sorted(set(names)) for names in artwork_hashes.values() if len(set(names)) > 1]
    return {"tracks": len(rows), "failures": failures, "shared_artwork_groups": suspicious}


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", default="")
    parser.add_argument("--config", default="")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--skip-backup", action="store_true")
    return parser.parse_args()


def main():
    args = parse_args()
    config = Config(args.config or None)
    db_path = Path(args.db or config.db_path).expanduser().resolve()
    backup = ""
    if not args.dry_run and not args.skip_backup:
        backup = str(backup_database(db_path, db_path.parent / "backups" / "rani-metadata"))
    db = Database(str(db_path))
    try:
        playlist = db.fetchone("SELECT id,name FROM playlists WHERE lower(name)=lower(?)", (PLAYLIST_NAME,))
        if not playlist:
            raise RuntimeError(f"Playlist not found: {PLAYLIST_NAME}")
        consolidated = consolidate_generated_duplicates(db, dry_run=args.dry_run)
        repaired = repair_tracks(db, playlist["id"], dry_run=args.dry_run)
        shared_cover = quarantine_shared_cover(dry_run=args.dry_run)
        local_export = {"success": True, "exported": [], "failures": []}
        if not args.dry_run:
            db.commit()
            local_export = export_local_music_playlists(db, str(MUSIC_ROOT))
            db.commit()
        verification = verify(db, playlist["id"]) if not args.dry_run else {}
        result = {
            "backup": backup,
            "consolidated": consolidated,
            "repaired_count": len(repaired),
            "shared_cover": shared_cover,
            "local_playlist_export": local_export,
            "verification": verification,
            "changes": repaired,
        }
        print(json.dumps(result, indent=2, ensure_ascii=False))
        if not args.dry_run and (not local_export.get("success") or verification["failures"]):
            return 2
        return 0
    finally:
        db.close()


if __name__ == "__main__":
    raise SystemExit(main())
