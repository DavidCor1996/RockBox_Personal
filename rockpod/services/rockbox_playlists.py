"""Import device-side Rockbox playlists into cached SQLite state."""

import logging
import os
import re
import hashlib
import json
import shutil
from pathlib import Path

from services.command_runner import CommandRunner
from services.file_safety import atomic_write_text
from services.smart_playlists import evaluate_playlist
from services.track_matcher import TrackMatcher

logger = logging.getLogger(__name__)

PLAYLIST_EXTENSIONS = {".m3u", ".m3u8"}
PLAYLIST_DIR_CANDIDATES = [
    "",
    "Playlists",
    "playlists",
    ".rockbox/playlists",
    ".rockbox/Playlists",
]
MANAGED_PLAYLIST_DIR = "Playlists"
LEGACY_MANAGED_PLAYLIST_DIR = ".rockbox/playlists/RockPod"
MANAGED_PLAYLIST_MANIFEST = ".rockbox/rockpod-playlists.json"
LOCAL_MANAGED_PLAYLIST_DIR = "Playlists/RockPod"
LOCAL_APPLE_MUSIC_MEDIA_DIR = "Playlists/RockPod Media"
APPLE_MUSIC_DIRECT_EXTENSIONS = {".mp3", ".m4a", ".aac", ".aiff", ".aif", ".wav"}


def import_device_playlists(db, device, force_reparse=False):
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
    device_track_by_basename = _device_basename_map(present_tracks)
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
            not force_reparse
            and
            previous
            and previous.get("source_mtime") == stat.st_mtime
            and previous.get("source_size") == stat.st_size
        ):
            continue
        entries = []
        for position, entry_path in enumerate(_parse_playlist_entries(full_path), start=1):
            matched = _match_playlist_entry(
                db,
                device_id,
                mount_path,
                entry_path,
                device_track_map,
                device_track_by_basename,
            )
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


def export_device_playlists(
    db,
    device,
    include_smart=True,
    duplicate_strictness="metadata_and_hash",
    duration_tolerance=2.0,
):
    device_id = getattr(device, "stable_device_key", "") or f"rockbox:{device.mount_path}"
    mount_path = getattr(device, "mount_path", "")
    managed_root = os.path.join(mount_path, MANAGED_PLAYLIST_DIR)
    os.makedirs(managed_root, exist_ok=True)

    device_tracks = [dict(row) for row in db.get_all_device_tracks(device_id)]
    device_path_by_local_id = _device_path_map_from_rows(device_tracks)
    matcher = TrackMatcher(duplicate_strictness, duration_tolerance)
    previous_manifest = _read_managed_playlist_manifest(mount_path)
    previous_sources = set(previous_manifest.get("sources") or [])
    used_names = _existing_catalog_playlist_names(mount_path, previous_sources)
    seen_sources = set()
    exported = []
    removed = []
    failures = []

    for playlist_row in db.get_all_playlists():
        playlist = dict(playlist_row) if hasattr(playlist_row, "keys") else playlist_row
        if not playlist.get("sync_to_rockbox", 1):
            continue
        if playlist.get("is_smart") and not include_smart:
            continue
        rows = (
            evaluate_playlist(db, playlist, device_id=device_id)
            if playlist.get("is_smart")
            else db.get_playlist_tracks(playlist["id"])
        )
        entries = _playlist_entries(rows, device_path_by_local_id, device_tracks, matcher)
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

    for rel_source in _managed_playlist_sources(mount_path, previous_sources):
        if rel_source in seen_sources:
            continue
        full_path = os.path.join(mount_path, rel_source)
        try:
            os.remove(full_path)
            removed.append(rel_source)
        except OSError as exc:
            failures.append(f"{rel_source}: {exc}")
            logger.warning("Could not remove stale Rockbox playlist %s: %s", full_path, exc)

    if not failures:
        try:
            _write_managed_playlist_manifest(mount_path, seen_sources)
        except OSError as exc:
            failures.append(f"{MANAGED_PLAYLIST_MANIFEST}: {exc}")
            logger.warning("Could not write Rockbox playlist manifest: %s", exc)

    imported = import_device_playlists(db, device, force_reparse=True)
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


def export_local_music_playlists(
    db,
    music_root,
    include_smart=True,
    device_id="",
    convert_for_apple_music=False,
    ffmpeg_path="",
):
    """Export RockPod playlists into the shared music folder as relative .m3u8 files."""
    used_names = set()
    seen_sources = set()
    exported = []
    removed = []
    failures = []

    if not str(music_root or "").strip():
        return {
            "success": False,
            "exported": [],
            "removed": [],
            "failures": ["music folder is not configured"],
        }

    music_root = os.path.abspath(os.path.expanduser(str(music_root)))
    managed_root = os.path.join(music_root, LOCAL_MANAGED_PLAYLIST_DIR)
    media_root = os.path.join(music_root, LOCAL_APPLE_MUSIC_MEDIA_DIR)

    for playlist_row in db.get_all_playlists():
        playlist = dict(playlist_row) if hasattr(playlist_row, "keys") else playlist_row
        if playlist.get("is_smart") and not include_smart:
            continue
        rows = (
            evaluate_playlist(db, playlist, device_id=device_id)
            if playlist.get("is_smart")
            else db.get_playlist_tracks(playlist["id"])
        )
        try:
            entries = _local_playlist_entries(
                rows,
                music_root,
                managed_root,
                media_root=media_root,
                convert_for_apple_music=convert_for_apple_music,
                ffmpeg_path=ffmpeg_path,
            )
        except OSError as exc:
            failures.append(f"{playlist['name']}: {exc}")
            logger.warning("Could not prepare local playlist %s: %s", playlist["name"], exc)
            continue
        if not entries:
            continue

        filename = _managed_playlist_filename(playlist["name"], used_names)
        rel_source = f"{LOCAL_MANAGED_PLAYLIST_DIR}/{filename}"
        full_path = os.path.join(music_root, rel_source)
        seen_sources.add(rel_source)
        try:
            _write_playlist_file(full_path, entries, path_key="path", absolute_prefix="")
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
            logger.warning("Could not export local playlist %s: %s", full_path, exc)

    if not failures:
        for rel_source in _local_managed_playlist_sources(music_root):
            if rel_source in seen_sources:
                continue
            full_path = os.path.join(music_root, rel_source)
            try:
                os.remove(full_path)
                removed.append(rel_source)
            except OSError as exc:
                failures.append(f"{rel_source}: {exc}")
                logger.warning("Could not remove stale local playlist %s: %s", full_path, exc)

    logger.info(
        "Exported %d local music playlists to %s (%d removed, %d failures)",
        len(exported),
        managed_root,
        len(removed),
        len(failures),
    )
    return {
        "success": not failures,
        "exported": exported,
        "removed": removed,
        "failures": failures,
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


def _managed_playlist_sources(mount_path, previous_sources=None):
    sources = set(str(path) for path in (previous_sources or []) if str(path or "").strip())
    legacy_root = os.path.join(mount_path, LEGACY_MANAGED_PLAYLIST_DIR)
    if os.path.isdir(legacy_root):
        for walk_root, _dirs, names in os.walk(legacy_root):
            for name in sorted(names):
                if Path(name).suffix.lower() not in PLAYLIST_EXTENSIONS:
                    continue
                full = os.path.join(walk_root, name)
                sources.add(os.path.relpath(full, mount_path).replace("\\", "/"))
    return sorted(sources)


def _existing_catalog_playlist_names(mount_path, managed_sources=None):
    managed_sources = set(managed_sources or [])
    root = os.path.join(mount_path, MANAGED_PLAYLIST_DIR)
    used = set()
    if not os.path.isdir(root):
        return used
    for name in os.listdir(root):
        full = os.path.join(root, name)
        rel_source = f"{MANAGED_PLAYLIST_DIR}/{name}".replace("\\", "/")
        if (
            os.path.isfile(full)
            and Path(name).suffix.lower() in PLAYLIST_EXTENSIONS
            and rel_source not in managed_sources
        ):
            used.add(name.casefold())
    return used


def _read_managed_playlist_manifest(mount_path):
    path = os.path.join(mount_path, MANAGED_PLAYLIST_MANIFEST)
    try:
        with open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, json.JSONDecodeError):
        return {"sources": []}
    sources = [
        str(item).replace("\\", "/")
        for item in (data.get("sources") or [])
        if str(item or "").strip()
    ]
    return {"sources": sources}


def _write_managed_playlist_manifest(mount_path, sources):
    path = os.path.join(mount_path, MANAGED_PLAYLIST_MANIFEST)
    data = {
        "version": 1,
        "sources": sorted(str(source).replace("\\", "/") for source in sources),
    }
    atomic_write_text(path, json.dumps(data, indent=2) + "\n")


def _local_managed_playlist_sources(music_root):
    root = os.path.join(music_root, LOCAL_MANAGED_PLAYLIST_DIR)
    if not os.path.isdir(root):
        return []
    sources = []
    for walk_root, _dirs, names in os.walk(root):
        for name in sorted(names):
            if Path(name).suffix.lower() not in PLAYLIST_EXTENSIONS:
                continue
            full = os.path.join(walk_root, name)
            sources.append(os.path.relpath(full, music_root).replace("\\", "/"))
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
    return _device_path_map_from_rows(db.get_all_device_tracks(device_id))


def _device_path_map_from_rows(rows):
    selected = {}
    for row in rows:
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


def _normalize_text(value):
    return str(value or "").strip().casefold()


def _device_basename_map(rows):
    by_basename = {}
    for item in (rows or []):
        row = dict(item) if hasattr(item, "keys") else item
        path = str(row.get("device_path") or "").replace("\\", "/")
        basename = os.path.basename(path).casefold()
        if not basename:
            continue
        by_basename.setdefault(basename, []).append(row)
    return by_basename


def _playlist_fallback_exact_match(row, candidate):
    local_title = _normalize_text(row.get("title"))
    local_artist = _normalize_text(row.get("artist") or row.get("album_artist"))
    local_album = _normalize_text(row.get("album"))
    local_duration = _duration_seconds(row.get("duration"))

    candidate_title = _normalize_text(candidate.get("title"))
    candidate_artist = _normalize_text(
        candidate.get("artist") or candidate.get("album_artist")
    )
    candidate_album = _normalize_text(candidate.get("album"))
    candidate_duration = _duration_seconds(candidate.get("duration"))

    if local_title and not candidate_title:
        return False
    if local_title and local_title != candidate_title:
        return False
    if local_artist and not candidate_artist:
        return False
    if local_artist and local_artist != candidate_artist:
        return False
    if local_album and candidate_album and local_album != candidate_album:
        return False
    if local_duration and candidate_duration:
        if abs(local_duration - candidate_duration) > 2:
            return False
    elif local_duration and not candidate_duration:
        return False
    return True


def _playlist_fallback_path(row, by_basename):
    file_path = str(row.get("file_path") or "")
    basename = os.path.basename(file_path).casefold()
    if not basename:
        return ""

    candidates = by_basename.get(basename)
    if not candidates or len(candidates) != 1:
        return ""

    candidate = candidates[0]
    if not _playlist_fallback_exact_match(row, candidate):
        return ""

    return str(candidate.get("device_path") or "").replace("\\", "/").lstrip("/")


def _playlist_entries(rows, device_path_by_local_id, device_tracks=None, matcher=None):
    entries = []
    playlist_rows = [dict(row) if hasattr(row, "keys") else dict(row) for row in rows]
    matched_paths = _playlist_matched_device_paths(playlist_rows, device_tracks or [], matcher)
    device_rows_by_local_id = _device_rows_by_local_id(device_tracks or [])
    by_basename = _device_basename_map(device_tracks or [])
    for item in playlist_rows:
        track_id = item.get("id")
        device_path = ""
        linked_device = device_rows_by_local_id.get(track_id)
        if linked_device and _playlist_fallback_exact_match(item, linked_device):
            device_path = device_path_by_local_id.get(track_id)
        if not device_path:
            device_path = matched_paths.get(track_id)
        if not device_path:
            device_path = _playlist_fallback_path(item, by_basename)
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


def _device_rows_by_local_id(rows):
    selected = {}
    for row in rows:
        item = dict(row) if hasattr(row, "keys") else row
        local_track_id = item.get("local_track_id")
        device_path = str(item.get("device_path") or "").replace("\\", "/").lstrip("/")
        if not local_track_id or not device_path:
            continue
        current = selected.get(local_track_id)
        if current is None or _device_path_rank(device_path) < _device_path_rank(
            str(current.get("device_path") or "")
        ):
            selected[local_track_id] = item
    return selected


def _playlist_matched_device_paths(rows, device_tracks, matcher=None):
    if not rows or not device_tracks:
        return {}
    matcher = matcher or TrackMatcher()
    matched, _unmatched, _orphaned, _resync = matcher.match_all(rows, device_tracks)
    paths = {}
    for result in matched:
        local = dict(result.local_track) if hasattr(result.local_track, "keys") else dict(result.local_track)
        device = dict(result.device_track) if hasattr(result.device_track, "keys") else dict(result.device_track)
        track_id = local.get("id")
        device_path = str(device.get("device_path") or "").replace("\\", "/").lstrip("/")
        if track_id and device_path and _playlist_fallback_exact_match(local, device):
            paths[track_id] = device_path
    return paths


def _local_playlist_entries(
    rows,
    music_root,
    playlist_dir,
    media_root="",
    convert_for_apple_music=False,
    ffmpeg_path="",
):
    entries = []
    for row in rows:
        item = dict(row) if hasattr(row, "keys") else row
        file_path = os.path.abspath(os.path.expanduser(str(item.get("file_path") or "")))
        if not file_path or not os.path.isfile(file_path):
            continue
        try:
            if os.path.commonpath([music_root, file_path]) != music_root:
                continue
        except ValueError:
            continue
        playlist_source = file_path
        if convert_for_apple_music and Path(file_path).suffix.lower() not in APPLE_MUSIC_DIRECT_EXTENSIONS:
            playlist_source = _ensure_apple_music_transcode(file_path, media_root, ffmpeg_path)
        try:
            rel_path = os.path.relpath(playlist_source, playlist_dir)
        except ValueError:
            rel_path = playlist_source
        entries.append(
            {
                "path": rel_path.replace("\\", "/"),
                "title": item.get("title", "") or Path(file_path).stem,
                "artist": item.get("artist", "") or item.get("album_artist", ""),
                "duration": item.get("duration", 0) or 0,
            }
        )
    return entries


def _ensure_apple_music_transcode(source_path, media_root, ffmpeg_path="", command_runner=None):
    if not media_root:
        raise OSError("Apple Music media cache is not configured")
    ffmpeg_bin = _ffmpeg_bin(ffmpeg_path)
    if not ffmpeg_bin:
        raise OSError("ffmpeg is required to convert FLAC files for Apple Music")
    os.makedirs(media_root, exist_ok=True)
    target_path = _apple_music_transcode_path(source_path, media_root)
    if os.path.isfile(target_path) and os.path.getsize(target_path) > 0:
        return target_path

    tmp_path = _temporary_playlist_media_path(target_path)
    if os.path.exists(tmp_path):
        os.remove(tmp_path)
    command = [
        ffmpeg_bin,
        "-y",
        "-i",
        source_path,
        "-map",
        "0:a:0",
        "-vn",
        "-map_metadata",
        "0",
        "-c:a",
        "alac",
        tmp_path,
    ]
    runner = command_runner or CommandRunner(log_dir=os.path.join(media_root, "logs"))
    result = runner.run(command, cwd=media_root)
    if result.returncode != 0:
        if os.path.exists(tmp_path):
            try:
                os.remove(tmp_path)
            except OSError:
                pass
        message = (
            result.stderr
            or result.stdout
            or getattr(result, "failure_message", lambda: "")()
            or f"ffmpeg exited with {result.returncode}"
        ).strip()
        raise OSError(f"audio conversion failed for {source_path}: {message}")
    os.replace(tmp_path, target_path)
    return target_path


def _apple_music_transcode_path(source_path, media_root):
    try:
        stat = os.stat(source_path)
        material = f"{source_path}|{int(stat.st_mtime)}|{int(stat.st_size)}"
    except OSError:
        material = source_path
    digest = hashlib.sha256(material.encode("utf-8")).hexdigest()[:16]
    stem = _sanitize_playlist_name(Path(source_path).stem) or "track"
    return os.path.join(media_root, f"{stem[:72]}-{digest}.m4a")


def _temporary_playlist_media_path(target_path):
    root, ext = os.path.splitext(target_path)
    return f"{root}.tmp{ext}" if ext else target_path + ".tmp"


def _ffmpeg_bin(ffmpeg_path=""):
    explicit = os.path.abspath(str(ffmpeg_path or "").strip()) if ffmpeg_path else ""
    if explicit and os.path.isfile(explicit):
        return explicit
    return shutil.which("ffmpeg") or ""


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


def _write_playlist_file(path, entries, path_key="device_path", absolute_prefix="/"):
    lines = ["#EXTM3U"]
    for entry in entries:
        duration = _duration_seconds(entry.get("duration"))
        artist = str(entry.get("artist") or "").strip()
        title = str(entry.get("title") or "").strip()
        if artist and title:
            label = f"{artist} - {title}"
        else:
            label = title or artist or Path(entry[path_key]).stem
        lines.append(f"#EXTINF:{duration},{label}")
        playlist_path = str(entry[path_key]).replace("\\", "/")
        if absolute_prefix:
            playlist_path = absolute_prefix + playlist_path.lstrip("/")
        lines.append(playlist_path)
    atomic_write_text(path, "\n".join(lines) + "\n")


def _duration_seconds(value):
    try:
        return max(0, int(round(float(value or 0))))
    except (TypeError, ValueError):
        return 0


def _match_playlist_entry(
    db,
    device_id,
    mount_path,
    entry_path,
    device_track_map=None,
    device_track_by_basename=None,
):
    normalized = _normalize_playlist_entry(mount_path, entry_path)
    if not normalized:
        return {
            "device_track_id": None,
            "local_track_id": None,
            "title": "",
            "artist": "",
            "album": "",
        }

    candidates = []

    if device_track_map:
        variants = [normalized]
        if normalized.startswith("/"):
            variants.append(normalized[1:])
        stripped = normalized
        while stripped.startswith("./") or stripped.startswith("../"):
            stripped = stripped[3:]
            variants.append(stripped)
            if stripped.startswith("/"):
                stripped = stripped[1:]
                variants.append(stripped)

        for variant in variants:
            row = device_track_map.get(variant)
            if row:
                candidates.append(row)

        if not candidates and device_track_by_basename is not None:
            by_basename = device_track_by_basename.get(os.path.basename(normalized).casefold())
            if by_basename and len(by_basename) == 1:
                candidates.append(by_basename[0])

    if candidates:
        row = dict(candidates[0])
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
