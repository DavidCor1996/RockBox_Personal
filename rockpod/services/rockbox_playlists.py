"""Import device-side Rockbox playlists into cached SQLite state."""

import logging
import os
import re
from pathlib import Path

from services.smart_playlists import evaluate_playlist

logger = logging.getLogger(__name__)

PLAYLIST_EXTENSIONS = {".m3u", ".m3u8"}
PLAYLIST_DIR_CANDIDATES = [
    "",
    "Playlists",
    "playlists",
    ".rockbox/playlists",
]
MANAGED_PLAYLIST_DIR = ".rockbox/playlists/RockPod"


def import_device_playlists(db, device):
    device_id = getattr(device, "stable_device_key", "") or f"rockbox:{device.mount_path}"
    mount_path = getattr(device, "mount_path", "")
    playlist_files = _playlist_files(mount_path)
    existing = {
        row["source_path"]: dict(row)
        for row in db.get_device_playlists(device_id)
    }
    present_tracks = [dict(row) for row in db.get_all_device_tracks(device_id)]
    device_track_map = {
        row["device_path"]: row
        for row in present_tracks
        if row.get("device_path")
    }
    seen_sources = set()

    imported = 0
    for full_path in playlist_files:
        rel_source = os.path.relpath(full_path, mount_path)
        seen_sources.add(rel_source)
        try:
            stat = os.stat(full_path)
        except OSError as exc:
            logger.warning("Could not stat Rockbox playlist %s: %s", full_path, exc)
            continue
        previous = existing.get(rel_source)
        if (
            previous
            and previous.get("source_mtime") == stat.st_mtime
            and previous.get("source_size") == stat.st_size
        ):
            continue
        entries = []
        for position, entry_path in enumerate(_parse_playlist_entries(full_path), start=1):
            matched = _match_playlist_entry(db, device_id, mount_path, entry_path, device_track_map)
            entries.append({"position": position, "entry_path": entry_path, **matched})
        playlist = db.upsert_device_playlist(
            {
                "device_id": device_id,
                "name": Path(full_path).stem,
                "source_path": rel_source,
                "source_mtime": stat.st_mtime,
                "source_size": stat.st_size,
                "track_count": len(entries),
            }
        )
        db.replace_device_playlist_tracks(playlist["id"], entries)
        imported += 1
    db.delete_missing_device_playlists(device_id, seen_sources)
    logger.info("Imported %d Rockbox playlists for %s", imported, device_id)
    return imported


def export_device_playlists(db, device, include_smart=True):
    device_id = getattr(device, "stable_device_key", "") or f"rockbox:{device.mount_path}"
    mount_path = getattr(device, "mount_path", "")
    managed_root = os.path.join(mount_path, MANAGED_PLAYLIST_DIR)
    os.makedirs(managed_root, exist_ok=True)

    device_path_by_local_id = _device_path_map(db, device_id)
    used_names = set()
    seen_sources = set()
    exported = []
    removed = []
    failures = []

    for playlist_row in db.get_all_playlists():
        playlist = dict(playlist_row) if hasattr(playlist_row, "keys") else playlist_row
        if playlist.get("is_smart") and not include_smart:
            continue
        rows = (
            evaluate_playlist(db, playlist, device_id=device_id)
            if playlist.get("is_smart")
            else db.get_playlist_tracks(playlist["id"])
        )
        entries = _playlist_entries(rows, device_path_by_local_id)
        if not entries:
            continue

        filename = _managed_playlist_filename(playlist["name"], used_names)
        rel_source = f"{MANAGED_PLAYLIST_DIR}/{filename}"
        full_path = os.path.join(mount_path, rel_source)
        seen_sources.add(rel_source)
        try:
            _write_playlist_file(full_path, entries)
            exported.append(
                {
                    "playlist_id": playlist["id"],
                    "name": playlist["name"],
                    "source_path": rel_source,
                    "track_count": len(entries),
                    "is_smart": bool(playlist.get("is_smart")),
                }
            )
        except OSError as exc:
            failures.append(f"{rel_source}: {exc}")
            logger.warning("Could not export Rockbox playlist %s: %s", full_path, exc)

    for rel_source in _managed_playlist_sources(mount_path):
        if rel_source in seen_sources:
            continue
        full_path = os.path.join(mount_path, rel_source)
        try:
            os.remove(full_path)
            removed.append(rel_source)
        except OSError as exc:
            failures.append(f"{rel_source}: {exc}")
            logger.warning("Could not remove stale Rockbox playlist %s: %s", full_path, exc)

    imported = import_device_playlists(db, device)
    logger.info(
        "Exported %d Rockbox playlists for %s (%d removed, %d failures)",
        len(exported),
        device_id,
        len(removed),
        len(failures),
    )
    return {
        "success": not failures,
        "exported": exported,
        "removed": removed,
        "failures": failures,
        "imported_count": imported,
    }


def _playlist_files(mount_path):
    files = []
    seen = set()
    for rel_root in PLAYLIST_DIR_CANDIDATES:
        root = os.path.join(mount_path, rel_root) if rel_root else mount_path
        if not os.path.isdir(root):
            continue
        for walk_root, dirs, names in os.walk(root):
            dirs[:] = [d for d in dirs if not d.startswith(".") or walk_root == mount_path]
            for name in sorted(names):
                if Path(name).suffix.lower() not in PLAYLIST_EXTENSIONS:
                    continue
                full = os.path.realpath(os.path.join(walk_root, name))
                if full in seen:
                    continue
                seen.add(full)
                files.append(full)
    return sorted(files)


def _managed_playlist_sources(mount_path):
    root = os.path.join(mount_path, MANAGED_PLAYLIST_DIR)
    if not os.path.isdir(root):
        return []
    sources = []
    for walk_root, _dirs, names in os.walk(root):
        for name in sorted(names):
            if Path(name).suffix.lower() not in PLAYLIST_EXTENSIONS:
                continue
            full = os.path.join(walk_root, name)
            sources.append(os.path.relpath(full, mount_path).replace("\\", "/"))
    return sorted(sources)


def _parse_playlist_entries(path):
    entries = []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for raw in handle:
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                entries.append(line)
    except OSError as exc:
        logger.warning("Could not read Rockbox playlist %s: %s", path, exc)
    return entries


def _normalize_playlist_entry(mount_path, entry_path):
    value = str(entry_path or "").strip().replace("\\", "/")
    if not value:
        return ""
    mount = str(mount_path or "").replace("\\", "/").rstrip("/")
    if mount and value.startswith(mount):
        value = value[len(mount):]
    return value.lstrip("/")


def _device_path_map(db, device_id):
    selected = {}
    for row in db.get_all_device_tracks(device_id):
        item = dict(row)
        local_track_id = item.get("local_track_id")
        device_path = str(item.get("device_path") or "").replace("\\", "/").lstrip("/")
        if not local_track_id or not device_path:
            continue
        current = selected.get(local_track_id)
        if current is None or _device_path_rank(device_path) < _device_path_rank(current):
            selected[local_track_id] = device_path
    return selected


def _device_path_rank(device_path):
    return (len(device_path), device_path.casefold())


def _playlist_entries(rows, device_path_by_local_id):
    entries = []
    for row in rows:
        item = dict(row) if hasattr(row, "keys") else row
        track_id = item.get("id")
        device_path = device_path_by_local_id.get(track_id)
        if not device_path:
            continue
        entries.append(
            {
                "device_path": device_path,
                "title": item.get("title", "") or Path(device_path).stem,
                "artist": item.get("artist", "") or item.get("album_artist", ""),
                "duration": item.get("duration", 0) or 0,
            }
        )
    return entries


def _managed_playlist_filename(name, used_names):
    base = _sanitize_playlist_name(name) or "Playlist"
    candidate = f"{base}.m3u8"
    counter = 2
    while candidate.casefold() in used_names:
        candidate = f"{base} ({counter}).m3u8"
        counter += 1
    used_names.add(candidate.casefold())
    return candidate


def _sanitize_playlist_name(name):
    text = str(name or "").strip()
    text = re.sub(r'[<>:"/\\|?*]+', "_", text)
    text = re.sub(r"\s+", " ", text).strip(" .")
    return text[:120].rstrip(" .")


def _write_playlist_file(path, entries):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    lines = ["#EXTM3U"]
    for entry in entries:
        duration = _duration_seconds(entry.get("duration"))
        artist = str(entry.get("artist") or "").strip()
        title = str(entry.get("title") or "").strip()
        if artist and title:
            label = f"{artist} - {title}"
        else:
            label = title or artist or Path(entry["device_path"]).stem
        lines.append(f"#EXTINF:{duration},{label}")
        lines.append("/" + str(entry["device_path"]).replace("\\", "/").lstrip("/"))
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


def _duration_seconds(value):
    try:
        return max(0, int(round(float(value or 0))))
    except (TypeError, ValueError):
        return 0


def _match_playlist_entry(db, device_id, mount_path, entry_path, device_track_map=None):
    normalized = _normalize_playlist_entry(mount_path, entry_path)
    device_track = device_track_map.get(normalized) if device_track_map and normalized else None
    if not device_track and normalized:
        device_track = db.fetchone(
            "SELECT * FROM device_tracks WHERE device_id = ? AND device_path LIKE ? "
            "AND COALESCE(present_on_device, 1) = 1 LIMIT 1",
            (device_id, f"%{normalized}"),
        )
    if device_track:
        row = dict(device_track)
        return {
            "device_track_id": row.get("id"),
            "local_track_id": row.get("local_track_id"),
            "title": row.get("title", ""),
            "artist": row.get("artist", ""),
            "album": row.get("album", ""),
        }
    return {
        "device_track_id": None,
        "local_track_id": None,
        "title": Path(normalized).stem if normalized else "",
        "artist": "",
        "album": "",
    }
