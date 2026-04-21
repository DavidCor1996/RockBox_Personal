"""Import Rockbox runtime/playback data from device-side ASCII exports."""

import csv
import json
import logging
import os
from datetime import datetime, timezone
from pathlib import Path


logger = logging.getLogger(__name__)

RUNTIME_EXPORT_CANDIDATES = [
    ".rockbox/database_changelog.txt",
    ".rockbox/database_changelog.json",
    ".rockbox/database_changelog.csv",
]

FIELD_ALIASES = {
    "filename": "source_path",
    "file": "source_path",
    "path": "source_path",
    "file_path": "source_path",
    "title": "title",
    "artist": "artist",
    "album": "album",
    "albumartist": "album_artist",
    "album_artist": "album_artist",
    "playcount": "play_count",
    "play_count": "play_count",
    "lastplayed": "last_played",
    "last_played": "last_played",
    "rating": "rating",
    "pm": "play_time_minutes",
    "ps": "play_time_seconds_component",
    "playtime": "play_time_seconds",
    "play_time": "play_time_seconds",
    "play_time_seconds": "play_time_seconds",
}


def import_runtime_data_for_device(db, device):
    device_key = getattr(device, "stable_device_key", "") or f"rockbox:{device.mount_path}"
    imported = 0
    for path in _runtime_export_paths(device):
        imported += import_runtime_file(db, device_key, getattr(device, "mount_path", ""), path)
    return imported


def _runtime_export_paths(device):
    mount_path = getattr(device, "mount_path", "")
    paths = []
    for rel in RUNTIME_EXPORT_CANDIDATES:
        full = os.path.join(mount_path, rel)
        if os.path.isfile(full):
            paths.append(full)
    return paths


def import_runtime_file(db, device_id, mount_path, source_path):
    source = Path(source_path)
    try:
        stat = source.stat()
    except OSError:
        return 0

    previous = db.runtime_import_state(device_id, str(source))
    if previous and previous["source_mtime"] == stat.st_mtime and previous["source_size"] == stat.st_size:
        return 0

    entries, parser_name = parse_runtime_entries(source)
    db.clear_runtime_stats_for_device(device_id, str(source))
    imported = 0
    for idx, entry in enumerate(entries):
        mapped = _normalize_entry(entry, mount_path)
        if not mapped:
            continue
        local_track_id, device_track_id, confidence, match_basis = _match_runtime_entry(
            db, device_id, mapped
        )
        if local_track_id is None and device_track_id is None:
            continue
        db.upsert_runtime_stat(
            {
                "device_id": device_id,
                "source_path": str(source),
                "source_track_key": mapped.get("source_track_key") or f"{idx}:{mapped.get('source_path', '')}",
                "local_track_id": local_track_id,
                "device_track_id": device_track_id,
                "confidence": confidence,
                "match_basis": match_basis,
                "play_count": mapped.get("play_count"),
                "last_played": mapped.get("last_played"),
                "rating": mapped.get("rating"),
                "play_time_seconds": mapped.get("play_time_seconds"),
                "raw_payload": json.dumps(mapped, sort_keys=True),
            }
        )
        imported += 1

    db.record_runtime_import(
        {
            "device_id": device_id,
            "source_path": str(source),
            "source_mtime": stat.st_mtime,
            "source_size": stat.st_size,
            "parser_name": parser_name,
            "status": "ok",
            "details": f"{imported} matched entries",
        }
    )
    return imported


def parse_runtime_entries(path):
    suffix = path.suffix.lower()
    if suffix == ".json":
        return _parse_json_runtime(path), "json"

    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()

    stripped = [line for line in text.splitlines() if line.strip() and not line.lstrip().startswith("#")]
    if not stripped:
        return [], "empty"

    header = stripped[0].lower()
    if ("\t" in stripped[0] or "," in stripped[0]) and ("filename" in header or "playcount" in header):
        delimiter = "\t" if "\t" in stripped[0] else ","
        return _parse_delimited_runtime(stripped, delimiter), f"delimited:{delimiter}"
    return _parse_key_value_runtime(stripped), "key_value"


def _parse_json_runtime(path):
    with open(path, "r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if isinstance(payload, dict):
        if isinstance(payload.get("entries"), list):
            return payload["entries"]
        return [payload]
    if isinstance(payload, list):
        return payload
    return []


def _parse_delimited_runtime(lines, delimiter):
    reader = csv.DictReader(lines, delimiter=delimiter)
    return [row for row in reader]


def _parse_key_value_runtime(lines):
    entries = []
    current = {}
    for line in lines:
        if line.strip() == "---":
            if current:
                entries.append(current)
                current = {}
            continue
        if "=" not in line:
            if current:
                entries.append(current)
                current = {}
            continue
        key, value = line.split("=", 1)
        current[key.strip()] = value.strip()
    if current:
        entries.append(current)
    return entries


def _normalize_entry(entry, mount_path):
    if not isinstance(entry, dict):
        return None
    normalized = {}
    for raw_key, raw_value in entry.items():
        key = FIELD_ALIASES.get(str(raw_key).strip().lower())
        if key:
            normalized[key] = raw_value
    if not normalized:
        return None
    source_path = normalized.get("source_path", "") or ""
    normalized["source_path"] = _normalize_device_path(source_path, mount_path)
    normalized["source_track_key"] = normalized.get("source_path") or "|".join(
        str(normalized.get(part, "")).strip()
        for part in ("artist", "album", "title")
    )
    normalized["play_count"] = _to_int(normalized.get("play_count"))
    normalized["rating"] = _to_int(normalized.get("rating"))
    play_time = normalized.get("play_time_seconds")
    if play_time is None:
        minutes = _to_int(normalized.get("play_time_minutes"))
        seconds = _to_int(normalized.get("play_time_seconds_component"))
        play_time = minutes * 60 + seconds if minutes or seconds else None
    normalized["play_time_seconds"] = _to_float(play_time)
    normalized["last_played"] = _normalize_timestamp(normalized.get("last_played"))
    return normalized


def _normalize_device_path(source_path, mount_path):
    value = str(source_path or "").strip()
    if not value:
        return ""
    value = value.replace("\\", "/")
    mount = str(mount_path or "").replace("\\", "/").rstrip("/")
    if mount and value.startswith(mount):
        value = value[len(mount):]
    return value.lstrip("/")


def _match_runtime_entry(db, device_id, entry):
    source_path = entry.get("source_path")
    if source_path:
        row = db.fetchone(
            "SELECT id, local_track_id FROM device_tracks "
            "WHERE device_id = ? AND device_path = ?",
            (device_id, source_path),
        )
        if row:
            return row["local_track_id"], row["id"], 1.0, "device_path"

    title = entry.get("title") or ""
    artist = entry.get("artist") or ""
    album = entry.get("album") or ""
    if title and artist:
        rows = db.fetchall(
            "SELECT id FROM tracks WHERE title = ? AND artist = ? "
            "AND (? = '' OR album = ?) ORDER BY id LIMIT 2",
            (title, artist, album, album),
        )
        if len(rows) == 1:
            track_id = rows[0]["id"]
            dev_row = db.fetchone(
                "SELECT id FROM device_tracks WHERE device_id = ? AND local_track_id = ? "
                "AND COALESCE(present_on_device, 1) = 1 LIMIT 1",
                (device_id, track_id),
            )
            return track_id, dev_row["id"] if dev_row else None, 0.95, "metadata"
    return None, None, 0.0, ""


def _normalize_timestamp(value):
    if value in (None, "", 0, "0"):
        return ""
    text = str(value).strip()
    if text.isdigit():
        timestamp = int(text)
        if timestamp > 0:
            return datetime.fromtimestamp(timestamp, tz=timezone.utc).replace(microsecond=0).isoformat()
    for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%d", "%Y/%m/%d %H:%M:%S"):
        try:
            return datetime.strptime(text, fmt).replace(tzinfo=timezone.utc).isoformat()
        except ValueError:
            continue
    return text


def _to_int(value):
    try:
        return int(value)
    except (TypeError, ValueError):
        return 0


def _to_float(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return None
