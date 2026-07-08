#!/usr/bin/env python3
"""Repair "Rani's Playlist" metadata and duplicates in the RockPod DB.

This script:
- removes duplicate playlist rows for the same title/artist pair, preferring the
  original source copy over RockPod Media cache copies,
- fixes track metadata from the local playlist source and known overrides for known
  wrong entries,
- updates album metadata for tracks currently stored as "Rani's playlist" by
  inferring from folder layout and optional online lookup,
- refreshes artwork for the resolved Rani album entries so the next sync exports the
  corrected covers.
"""

import argparse
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import defaultdict
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from app.config import Config  # noqa: E402
from app.database import Database  # noqa: E402
from services.artwork_manager import ArtworkManager  # noqa: E402
from services.metadata_reader import extract_artwork_data  # noqa: E402


RANI_ALBUM = "Rani's playlist"
MUSIC_ROOT = "/home/david/Music"
ROCKPOD_MEDIA_MARKER = "Playlists/RockPod Media"
MIN_INTERVAL_SECONDS = 0.65

TRACK_ARTIST_FIXES_BY_TITLE_ARTIST = {
}

TRACK_ARTIST_FIXES_BY_TITLE = {
}

INVALID_ARTIST_VALUES = {
    "unknown",
    "unknown artist",
    "",
}

ALBUM_INVALID_VALUES = {
    "rani's playlist",
    "rani's playlist.m3u",
    "rani's playlist.m3u8",
    "unknown album",
    "unknown",
    "",
}


def _normalize(value):
    value = str(value or "")
    value = value.replace("’", "'").replace("‘", "'").replace("`", "'").replace("´", "'")
    value = value.strip().casefold()
    value = re.sub(r"\s+", " ", value)
    return value


def _normalize_path(value):
    return os.path.normcase(os.path.abspath(os.path.realpath(str(value or ""))))


def _is_ranis_album(value):
    return _normalize(value) == _normalize(RANI_ALBUM)


def _is_generated_media(path):
    return ROCKPOD_MEDIA_MARKER in str(path or "")


def _parse_album_from_path(file_path):
    folder = os.path.basename(os.path.dirname(file_path))
    match = re.match(r"^.+? - (.+?) \(\d{4}\)$", folder)
    if not match:
        if " - " not in folder:
            return ""
        fallback = folder.split(" - ", 1)[1].strip()
        return re.sub(r"\s+", " ", fallback).strip()
    album = match.group(1).strip()
    return re.sub(r"\s+", " ", album).strip()


def _is_valid_album(value):
    value_norm = _normalize(value)
    return bool(value_norm) and value_norm not in ALBUM_INVALID_VALUES


def _is_valid_artist(value):
    return _normalize(value) not in INVALID_ARTIST_VALUES


def _read_embedded_album(file_path):
    from services.metadata_reader import read_metadata

    if not os.path.isfile(file_path):
        return ""

    try:
        metadata = read_metadata(file_path)
    except Exception:
        return ""
    album = (metadata.album or "").strip()
    return re.sub(r"\s+", " ", album).strip()


def _artwork_extension(mime_type):
    text = str(mime_type or "").strip().lower()
    if "png" in text:
        return ".png"
    return ".jpg"


def _safe_artwork_stem(track_id, artist, album, title):
    text = f"{track_id}-{artist}-{album}-{title}"
    text = re.sub(r'[<>:"/\\|?*\x00-\x1f]+', "_", text)
    text = re.sub(r"\s+", " ", text).strip(" ._")
    return (text or str(track_id))[:96]


def _write_embedded_artwork_file(track_id, file_path, artist, album, title):
    data, mime_type = extract_artwork_data(file_path)
    if not data:
        return ""
    art_dir = os.path.join(os.path.dirname(file_path), ".rockpod-artwork")
    os.makedirs(art_dir, exist_ok=True)
    art_path = os.path.join(
        art_dir,
        _safe_artwork_stem(track_id, artist, album, title) + _artwork_extension(mime_type),
    )
    if os.path.isfile(art_path):
        try:
            if os.path.getsize(art_path) == len(data):
                return art_path
        except OSError:
            pass
    with open(art_path, "wb") as handle:
        handle.write(data)
    return art_path


def _playlist_file_candidates(playlist_name, music_root):
    base = os.path.abspath(os.path.expanduser(str(music_root or "").strip() or MUSIC_ROOT))
    filenames = [f"{playlist_name}.m3u8", f"{playlist_name}.m3u"]
    folders = [
        os.path.join(base, "Playlists", "RockPod"),
        os.path.join(base, "Playlists"),
        os.path.join(base, ".rockbox", "playlists", "RockPod"),
        os.path.join(base, ".rockbox", "Playlists"),
    ]
    for folder in folders:
        for filename in filenames:
            yield os.path.normpath(os.path.join(folder, filename))


def _parse_extinf(line):
    payload = line.split(",", 1)[1] if "," in line else ""
    payload = payload.strip()
    if not payload:
        return "", ""
    if " - " in payload:
        artist, title = payload.split(" - ", 1)
        return artist.strip(), title.strip()
    return "", payload


def _load_playlist_reference_tracks(playlist_name, music_root):
    playlist_path = ""
    for candidate in _playlist_file_candidates(playlist_name, music_root):
        if os.path.isfile(candidate):
            playlist_path = candidate
            break
    if not playlist_path:
        return {}

    base_dir = os.path.dirname(playlist_path)
    pending_title = ""
    pending_artist = ""
    tracks = {}
    try:
        with open(playlist_path, "r", encoding="utf-8", errors="replace") as handle:
            for raw in handle:
                line = raw.strip()
                if not line:
                    continue
                if line.startswith("#EXTINF"):
                    pending_artist, pending_title = _parse_extinf(line)
                    continue
                if line.startswith("#"):
                    continue

                resolved = _normalize_path(os.path.join(base_dir, line))
                tracks[resolved] = {
                    "title": pending_title,
                    "artist": pending_artist,
                }
                pending_title = ""
                pending_artist = ""
    except OSError:
        return {}

    return tracks


def _resolve_playlist_metadata(file_title, file_artist, playlist_title, playlist_artist):
    resolved_title = file_title if _normalize(file_title) else playlist_title
    resolved_artist = file_artist if _is_valid_artist(file_artist) else playlist_artist
    return resolved_title or playlist_title, resolved_artist or playlist_artist


def _resolve_known_artist(title, artist):
    title_norm = _normalize(title)
    artist_norm = _normalize(artist)
    if (title_norm, artist_norm) in TRACK_ARTIST_FIXES_BY_TITLE_ARTIST:
        return TRACK_ARTIST_FIXES_BY_TITLE_ARTIST[(title_norm, artist_norm)]
    if title_norm in TRACK_ARTIST_FIXES_BY_TITLE:
        return TRACK_ARTIST_FIXES_BY_TITLE[title_norm]
    return artist


class _RateLimitedItunesSongLookup:
    _last_request = 0.0

    def __init__(self, storefront="us", timeout=12.0):
        self.storefront = storefront or "us"
        self.timeout = float(timeout or 12.0)

    def fetch_album(self, title, artist):
        title = str(title or "").strip()
        artist = str(artist or "").strip()
        if not title or not artist:
            return ""

        self._respect_rate_limit()
        query = f"{title} {artist}".strip()
        params = {
            "term": query,
            "media": "music",
            "entity": "song",
            "country": self.storefront,
            "limit": 12,
        }
        url = f"https://itunes.apple.com/search?{urllib.parse.urlencode(params)}"
        req = urllib.request.Request(url, headers={"User-Agent": "RockPod/1.0"})
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as response:
                payload = json.loads(response.read().decode("utf-8", errors="replace"))
        except (urllib.error.URLError, ValueError):
            return ""

        want_title = _normalize(title)
        want_artist = _normalize(artist)
        best = ""
        best_score = -1
        for item in payload.get("results") or []:
            track_name = _normalize(item.get("trackName") or item.get("track") or "")
            artist_name = _normalize(item.get("artistName") or "")
            if not track_name or not artist_name:
                continue

            score = 0
            if track_name == want_title:
                score += 70
            elif want_title and want_title in track_name:
                score += 40
            if artist_name == want_artist:
                score += 25
            elif want_artist and (want_artist in artist_name or artist_name in want_artist):
                score += 10

            if score <= best_score:
                continue

            album = str(item.get("collectionName") or "").strip()
            if not album:
                continue
            best = album
            best_score = score

        return best

    def _respect_rate_limit(self):
        now = time.monotonic()
        elapsed = now - self._last_request
        wait = MIN_INTERVAL_SECONDS - elapsed
        if wait > 0:
            time.sleep(wait)
        self._last_request = time.monotonic()


def fix_ranis_playlist(
    db,
    playlist_name="Rani's Playlist",
    use_online=True,
    music_root=MUSIC_ROOT,
    artwork_manager=None,
    dry_run=False,
    timeout=12.0,
):
    playlist_row = db.fetchone(
        "SELECT id, name FROM playlists WHERE lower(name) = lower(?)", (playlist_name,),
    )
    if playlist_row is None:
        raise RuntimeError(f"Playlist not found: {playlist_name}")
    playlist_id = playlist_row["id"]

    playlist_tracks = db.fetchall(
        """
        SELECT
            pt.id AS playlist_track_id,
            pt.position AS position,
            pt.track_id,
            t.title AS title,
            t.artist AS artist,
            t.file_path AS file_path
        FROM playlist_tracks pt
        JOIN tracks t ON t.id = pt.track_id
        WHERE pt.playlist_id = ?
        ORDER BY pt.position, pt.id
        """,
        (playlist_id,),
    )

    groups = defaultdict(list)
    for row in playlist_tracks:
        groups[(_normalize(row["title"]), _normalize(row["artist"]))].append(row)

    duplicate_ids = []
    for group in groups.values():
        group_sorted = sorted(
            group,
            key=lambda item: (
                _is_generated_media(item["file_path"]),
                item["position"],
                item["playlist_track_id"],
            ),
        )
        duplicate_ids.extend(item["playlist_track_id"] for item in group_sorted[1:])

    removed = 0
    if duplicate_ids:
        placeholders = ",".join(["?"] * len(duplicate_ids))
        removed = len(duplicate_ids)
        if not dry_run:
            db.execute(
                f"DELETE FROM playlist_tracks WHERE id IN ({placeholders})",
                tuple(duplicate_ids),
            )

    remaining_rows = db.fetchall(
        """
        SELECT id AS playlist_track_id, position
        FROM playlist_tracks
        WHERE playlist_id = ?
        ORDER BY position, id
        """,
        (playlist_id,),
    )

    if not dry_run:
        for idx, row in enumerate(remaining_rows, start=1):
            db.execute(
                "UPDATE playlist_tracks SET position = ? WHERE id = ?",
                (idx, row["playlist_track_id"]),
            )

    playlist_tracks_all = db.fetchall(
        """
        SELECT DISTINCT t.id, t.title, t.artist, t.album_artist, t.album, t.file_path, t.artwork_path
        FROM playlist_tracks pt
        JOIN tracks t ON t.id = pt.track_id
        WHERE pt.playlist_id = ?
        """,
        (playlist_id,),
    )

    playlist_reference = _load_playlist_reference_tracks(playlist_name, music_root)
    lookup = _RateLimitedItunesSongLookup(timeout=timeout) if use_online else None

    albums_updated = 0
    albums_from_path = 0
    albums_from_online = 0
    albums_unresolved = 0
    metadata_updates = 0
    albums_from_embedded = 0
    metadata_examples = []
    album_examples = []
    dry_run_examples = []

    artwork_queued = 0
    artwork_updated = 0
    artwork_failures = 0

    album_targets = {}
    album_lookup_cache = {}

    candidate_tracks = [dict(raw) if hasattr(raw, "keys") else raw for raw in playlist_tracks_all]
    folder_album_keys = defaultdict(set)
    for track in candidate_tracks:
        file_path = track["file_path"] or ""
        if not file_path:
            continue
        folder_album_keys[_normalize_path(os.path.dirname(file_path))].add(
            (_normalize(track["artist"]), _normalize(track["album"]))
        )

    for track in candidate_tracks:
        track_id = track["id"]
        title = track["title"] or ""
        artist = track["artist"] or ""
        file_path = track["file_path"] or ""
        current_album = track["album"] or ""
        current_album_artist = track["album_artist"] or ""
        wanted = playlist_reference.get(_normalize_path(file_path), {})

        corrected_title = wanted.get("title") or title
        corrected_artist = wanted.get("artist") or artist
        resolved_title, resolved_artist = _resolve_playlist_metadata(
            title, artist, corrected_title, corrected_artist
        )
        corrected_title = resolved_title
        corrected_artist = resolved_artist
        corrected_artist = _resolve_known_artist(corrected_title, corrected_artist)
        metadata = {}
        if corrected_title != title:
            metadata["title"] = corrected_title
        if corrected_artist != artist:
            metadata["artist"] = corrected_artist
        if not str(current_album_artist).strip():
            metadata["album_artist"] = corrected_artist
        if metadata:
            metadata_updates += 1
            if dry_run:
                metadata_examples.append(
                    (
                        track_id,
                        title,
                        artist,
                        corrected_title,
                        corrected_artist,
                        file_path,
                    )
                )
            else:
                db.update_track_metadata(track_id, metadata)

        album = ""
        source = ""
        album_changed = False
        if lookup:
            cache_key = (_normalize(corrected_title), _normalize(corrected_artist))
            if cache_key not in album_lookup_cache:
                album_lookup_cache[cache_key] = lookup.fetch_album(corrected_title, corrected_artist)
            candidate = album_lookup_cache[cache_key]
            if _is_valid_album(candidate):
                album = candidate
                source = "online"
                albums_from_online += 1

        if not _is_valid_album(album):
            embedded = _read_embedded_album(file_path)
            if _is_valid_album(embedded):
                album = embedded
                source = "embedded"
                albums_from_embedded += 1

        if not _is_valid_album(album):
            album = _parse_album_from_path(file_path)
            if _is_valid_album(album):
                source = "path"
                albums_from_path += 1

        if not _is_valid_album(album) and _is_valid_album(current_album):
            album = current_album
            source = source or "current"

        if not _is_valid_album(album):
            albums_unresolved += 1
            if dry_run:
                dry_run_examples.append(
                    (track_id, corrected_title, corrected_artist, file_path, album, source or "missing")
                )
            continue

        folder = os.path.dirname(file_path)
        folder_key = _normalize_path(folder)
        mixed_folder = len(folder_album_keys.get(folder_key) or set()) > 1
        art_target = ""
        if mixed_folder:
            art_target = _write_embedded_artwork_file(
                track_id,
                file_path,
                corrected_artist,
                album,
                corrected_title,
            )
            if not art_target and artwork_manager is not None and not dry_run:
                art_target = artwork_manager.fetch_online_artwork_now(
                    {
                        "group_key": f"{corrected_artist}\0{album}",
                        "album": album,
                        "artist": corrected_artist,
                        "tracks": [
                            {
                                "id": track_id,
                                "media_type": "audio",
                                "title": corrected_title,
                                "artist": corrected_artist,
                                "album": album,
                                "file_path": file_path,
                            }
                        ],
                    },
                    force=True,
                )
        else:
            folder_cover = os.path.join(folder, "cover.jpg")
            if os.path.isfile(folder_cover):
                art_target = folder_cover
        if art_target:
            current_artwork = str(track.get("artwork_path") or "").strip()
            if _normalize_path(current_artwork) != _normalize_path(art_target):
                metadata_updates += 1
                if not dry_run:
                    db.update_track_metadata(
                        track_id,
                        {
                            "artwork_path": art_target,
                            "has_embedded_artwork": 1,
                        },
                    )

        if _normalize(album) != _normalize(current_album):
            album_changed = True
            if not dry_run:
                db.update_track_metadata(track_id, {"album": album})

        if album_changed:
            albums_updated += 1
        album_examples.append((corrected_title, corrected_artist, album, file_path, source))

        album_key = (_normalize(corrected_artist), _normalize(album))
        if album_key not in album_targets:
            album_targets[album_key] = {
                "album": album,
                "artist": corrected_artist,
                "tracks": [
                    {
                        "id": track_id,
                        "media_type": "audio",
                        "title": corrected_title,
                        "artist": corrected_artist,
                        "album": album,
                    }
                ],
            }

    if artwork_manager is not None and not dry_run:
        for album_info in album_targets.values():
            artwork_queued += 1
            if artwork_manager.fetch_online_artwork_now(album_info, force=True):
                artwork_updated += 1
            else:
                artwork_failures += 1

    return {
        "playlist_id": playlist_id,
        "name": playlist_row["name"],
        "playlist_entries_before": len(playlist_tracks),
        "playlist_duplicates_removed": removed,
        "playlist_entries_after": len(remaining_rows),
        "tracks_checked": len(candidate_tracks),
        "metadata_updates": metadata_updates,
        "metadata_examples": metadata_examples[:25],
        "albums_updated": albums_updated,
        "albums_from_path": albums_from_path,
        "albums_from_online": albums_from_online,
        "albums_from_embedded": albums_from_embedded,
        "albums_unresolved": albums_unresolved,
        "dry_run_examples": dry_run_examples[:25],
        "artwork_queued": artwork_queued,
        "artwork_updated": artwork_updated,
        "artwork_failures": artwork_failures,
        "album_examples": album_examples[:25],
    }


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description="Fix Rani's Playlist metadata in RockPod")
    parser.add_argument("--db", default=None, help="Database path (default from ~/.rockpod/config.json)")
    parser.add_argument("--config", default=None, help="Config path (default from ~/.rockpod/config.json)")
    parser.add_argument("--playlist", default="Rani's Playlist", help="Playlist name to repair")
    parser.add_argument("--music-root", default=MUSIC_ROOT, help="Music root for resolving local playlist files")
    parser.add_argument("--no-online", action="store_true", help="Skip online album + artwork lookup")
    parser.add_argument("--skip-artwork", action="store_true", help="Skip forcing online artwork lookup")
    parser.add_argument("--timeout", type=float, default=12.0, help="iTunes request timeout seconds")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without writing DB")
    return parser.parse_args(argv)


def _resolve_db_path(cli_db):
    if cli_db:
        return cli_db
    config_path = os.path.expanduser("~/.rockpod/config.json")
    try:
        with open(config_path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
            return data.get("db_path") or ""
    except Exception:
        return ""


def main(argv=None):
    args = parse_args(argv)
    db_path = _resolve_db_path(args.db)
    if not db_path:
        raise SystemExit("No database path available. Use --db to specify one.")

    music_root = args.music_root or MUSIC_ROOT
    config = Config(args.config)
    artwork = None
    if not args.skip_artwork and not args.no_online and not args.dry_run:
        artwork = ArtworkManager(config.artwork_cache_dir, config)

    db = Database(db_path)
    try:
        report = fix_ranis_playlist(
            db,
            playlist_name=args.playlist,
            use_online=not args.no_online,
            music_root=music_root,
            artwork_manager=artwork,
            dry_run=args.dry_run,
            timeout=args.timeout,
        )
    finally:
        if artwork:
            artwork.shutdown()
        db.close()

    print(f"playlist: {report['name']} (id={report['playlist_id']})")
    print(f"playlist entries before: {report['playlist_entries_before']}")
    print(f"playlist duplicates removed: {report['playlist_duplicates_removed']}")
    print(f"playlist entries after: {report['playlist_entries_after']}")
    print(f"tracks checked: {report['tracks_checked']}")
    print(f"metadata fixes: {report['metadata_updates']}")
    print(f"albums updated: {report['albums_updated']}")
    print(f"albums from path: {report['albums_from_path']}")
    print(f"albums from online: {report['albums_from_online']}")
    print(f"albums from embedded metadata: {report['albums_from_embedded']}")
    print(f"albums unresolved: {report['albums_unresolved']}")
    print(f"artwork queued: {report['artwork_queued']}")
    print(f"artwork updated: {report['artwork_updated']}")
    print(f"artwork failures: {report['artwork_failures']}")

    if report["album_examples"]:
        print("sample album updates:")
        for title, artist, album, file_path, source in report["album_examples"]:
            print(f"  - {artist} - {title} -> {album} ({source})")
            print(f"    {file_path}")

    if report["metadata_examples"]:
        print("sample metadata fixes:")
        for track_id, old_title, old_artist, new_title, new_artist, file_path in report["metadata_examples"]:
            print(f"  - #{track_id} {old_title} - {old_artist} -> {new_title} - {new_artist}")
            print(f"    {file_path}")

    if args.dry_run and report["dry_run_examples"]:
        print("sample album lookup misses:")
        for track_id, title, artist, file_path, album, source in report["dry_run_examples"]:
            print(f"  - #{track_id} | {artist} - {title} -> {album or 'unresolved'} ({source})")
            print(f"    {file_path}")


if __name__ == "__main__":
    main()
