"""Persistent per-device inventory verification.

The verifier runs with its own SQLite connection when used from a worker
thread. It reconciles the cached device inventory with the filesystem without
blocking the UI thread.
"""

import csv
import logging
import os
from pathlib import Path

from PySide6.QtCore import QObject, QThread, Signal, Slot, Qt

from app.config import SUPPORTED_FORMATS, SUPPORTED_VIDEO_FORMATS
from app.database import Database
from services.metadata_reader import compute_file_hash, read_metadata
from services.rockbox_device import detect_rockbox_database_state
from services.rockbox_playlists import import_device_playlists
from services.rockbox_runtime import import_runtime_data_for_device
from services.rockbox_tagcache import TagcacheError, read_rockbox_tagcache_tracks
from services.track_matcher import TrackMatcher
from models.track import compute_metadata_hash

logger = logging.getLogger(__name__)
DEVICE_TRACK_FORMATS = frozenset(SUPPORTED_FORMATS) | frozenset(SUPPORTED_VIDEO_FORMATS) | {".rvp"}


class DeviceInventoryCancelled(Exception):
    """Raised when inventory verification reaches a safe cancellation point."""


def device_music_roots(device):
    """Return unique existing music roots to scan, preferring Rockbox Music/."""
    mount_path = getattr(device, "mount_path", "")
    candidates = [
        getattr(device, "music_path", ""),
        os.path.join(mount_path, "Music"),
        os.path.join(mount_path, "MUSIC"),
        os.path.join(mount_path, "music"),
        os.path.join(mount_path, "iPod_Control", "Music"),
        os.path.join(mount_path, "iTunes_Control", "Music"),
        os.path.join(mount_path, "Videos"),
        os.path.join(mount_path, "VIDEOS"),
        os.path.join(mount_path, "videos"),
    ]
    roots = []
    seen = set()
    for path in candidates:
        if not path or not os.path.isdir(path):
            continue
        real = os.path.realpath(path)
        key = os.path.normcase(real)
        try:
            stat = os.stat(path)
            key = (stat.st_dev, stat.st_ino)
        except OSError:
            pass
        if key in seen:
            continue
        seen.add(key)
        roots.append(path)
    return roots


def device_record_from_info(device):
    """Return a database-ready row for a DeviceInfo-like object."""
    mount_path = os.path.normpath(getattr(device, "mount_path", "") or "")
    key = getattr(device, "stable_device_key", "") or f"rockbox:{mount_path}"
    return {
        "stable_device_key": key,
        "display_name": getattr(device, "name", "iPod") or "iPod",
        "mount_path_last_seen": mount_path,
        "rockbox_detected": 1 if getattr(device, "is_rockbox", False) else 0,
        "serial_or_signature": getattr(device, "serial_or_signature", "") or key,
        "capacity_bytes": getattr(device, "total_space", 0) or 0,
        "free_bytes_last_seen": getattr(device, "free_space", 0) or 0,
    }


def _track_row_from_metadata(track, device_key, rel_path, file_hash=""):
    return {
        "device_id": device_key,
        "device_path": rel_path,
        "file_size": track.file_size,
        "title": track.title,
        "artist": track.artist,
        "album": track.album,
        "album_artist": track.album_artist,
        "genre": track.genre,
        "year": track.year,
        "track_number": track.track_number,
        "disc_number": track.disc_number,
        "duration": track.duration,
        "bitrate": track.bitrate,
        "codec": track.codec,
        "metadata_hash": track.metadata_hash,
        "file_hash": file_hash,
        "present_on_device": 1,
    }


def _cached_track_row(cached):
    row = {
        k: v
        for k, v in dict(cached).items()
        if k
        not in (
            "id",
            "last_scan",
            "last_verified_at",
            "last_synced_at",
        )
    }
    row["present_on_device"] = 1
    return row


def _count_device_audio_files(scan_roots):
    """Count supported media files under the known device roots."""
    total = 0
    for scan_root in scan_roots:
        for root, dirs, files in os.walk(scan_root):
            dirs[:] = [d for d in dirs if not d.startswith(".")]
            for fn in files:
                if Path(fn).suffix.lower() in DEVICE_TRACK_FORMATS:
                    total += 1
    return total


def _scan_device_audio_paths(scan_roots, mount_path, cancel_callback=None):
    """Return relative media paths under the known device roots."""
    paths = set()
    for scan_root in scan_roots:
        if cancel_callback and cancel_callback():
            raise DeviceInventoryCancelled()
        for root, dirs, files in os.walk(scan_root):
            if cancel_callback and cancel_callback():
                raise DeviceInventoryCancelled()
            dirs[:] = [d for d in dirs if not d.startswith(".")]
            for fn in files:
                if cancel_callback and cancel_callback():
                    raise DeviceInventoryCancelled()
                if Path(fn).suffix.lower() not in DEVICE_TRACK_FORMATS:
                    continue
                full = os.path.join(root, fn)
                paths.add(os.path.relpath(full, mount_path))
    return paths


def _normalize_device_path(rel_path):
    return str(rel_path or "").replace("\\", "/").lower()


def _read_rockpod_video_manifest(mount_path):
    manifest_path = os.path.join(
        mount_path, ".rockbox", "videolist", "index.tsv"
    )
    try:
        with open(manifest_path, "r", encoding="utf-8", newline="") as handle:
            if not handle.readline().startswith("# rockpod videolist"):
                return {}
            return {
                _normalize_device_path(row.get("device_path")): dict(row)
                for row in csv.DictReader(handle, delimiter="\t")
                if str(row.get("device_path") or "").strip()
            }
    except (OSError, csv.Error):
        return {}


def _manifest_int(value):
    try:
        return int(str(value or "").strip())
    except (TypeError, ValueError):
        return None


def _local_video_id_for_manifest(db, entry):
    show_title = str(entry.get("show") or "").strip()
    season_number = _manifest_int(entry.get("season"))
    episode_number = _manifest_int(entry.get("episode"))
    if show_title and season_number is not None and episode_number is not None:
        rows = db.execute(
            "SELECT id FROM tracks WHERE lower(coalesce(media_type, '')) = 'video' "
            "AND lower(trim(coalesce(show_title, ''))) = lower(trim(?)) "
            "AND season_number = ? AND episode_number = ? ORDER BY id",
            (show_title, season_number, episode_number),
        ).fetchall()
        if len(rows) == 1:
            return rows[0]["id"]

    title = str(entry.get("title") or "").strip()
    if title:
        rows = db.execute(
            "SELECT id FROM tracks WHERE lower(coalesce(media_type, '')) = 'video' "
            "AND lower(trim(coalesce(title, ''))) = lower(trim(?)) ORDER BY id",
            (title,),
        ).fetchall()
        if len(rows) == 1:
            return rows[0]["id"]
    return None


def _rvp_track_row_from_manifest(
    db, device_key, rel_path, stat, entry, file_hash=""
):
    title = str(entry.get("title") or Path(rel_path).stem).strip()
    show_title = str(entry.get("show") or "").strip()
    video_kind = str(entry.get("kind") or "movie").strip()
    season_number = _manifest_int(entry.get("season"))
    episode_number = _manifest_int(entry.get("episode"))
    duration = float(_manifest_int(entry.get("duration")) or 0)
    year = _manifest_int(entry.get("year"))
    genre = str(entry.get("genre") or "").strip()
    artist = show_title
    album = (
        ("Specials" if season_number == 0 else f"Season {season_number}")
        if video_kind == "show" and season_number is not None
        else title
    )
    return {
        "device_id": device_key,
        "device_path": rel_path,
        "file_size": stat.st_size,
        "title": title,
        "artist": artist,
        "album": album,
        "album_artist": artist,
        "genre": genre,
        "year": year,
        "track_number": episode_number,
        "disc_number": 1,
        "duration": duration,
        "bitrate": 0,
        "codec": "RVP",
        "metadata_hash": compute_metadata_hash(
            title,
            artist,
            album,
            artist,
            episode_number,
            1,
            genre,
            year,
            "",
            duration,
            0,
            "RVP",
            "video",
            video_kind,
            show_title,
            season_number,
            episode_number,
        ),
        "file_hash": file_hash,
        "local_track_id": _local_video_id_for_manifest(db, entry),
        "present_on_device": 1,
    }


def _filesystem_only_paths(filesystem_paths, known_paths):
    known = {_normalize_device_path(path) for path in known_paths}
    return sorted(path for path in filesystem_paths if _normalize_device_path(path) not in known)


def _import_device_file(db, cached, device_key, mount_path, rel, force_full,
                        compute_hashes, metadata_reader_func, file_hash_func,
                        video_manifest=None):
    full = os.path.join(mount_path, rel)
    stat = os.stat(full)
    cached_row = cached.get(rel)
    reusable = (
        cached_row
        and not force_full
        and (cached_row.get("file_size") or 0) == stat.st_size
        and cached_row.get("metadata_hash")
        and (not compute_hashes or cached_row.get("file_hash"))
    )
    if reusable:
        return _cached_track_row(cached_row), False, False, not cached_row.get("local_track_id")

    if Path(rel).suffix.lower() == ".rvp":
        manifest_entry = (video_manifest or {}).get(
            _normalize_device_path(rel)
        )
        if manifest_entry:
            file_hash = file_hash_func(full) if compute_hashes else ""
            row = _rvp_track_row_from_manifest(
                db, device_key, rel, stat, manifest_entry, file_hash
            )
            return row, cached_row is None, cached_row is not None, True

    track = metadata_reader_func(full)
    if not track.metadata_hash:
        raise ValueError("parse failure")
    if not track.title or not (track.artist or track.album_artist):
        raise ValueError("metadata incomplete")
    file_hash = file_hash_func(full) if compute_hashes else ""
    row = _track_row_from_metadata(track, device_key, rel, file_hash)
    return row, cached_row is None, cached_row is not None, True


def verify_device_inventory(
    db,
    device,
    duplicate_strictness="metadata_only",
    force_full=False,
    metadata_reader_func=None,
    file_hash_func=None,
    duration_tolerance=2.0,
    status_callback=None,
    cancel_callback=None,
):
    """Reconcile cached inventory for one connected device.

    Returns a summary dict with scanned/new/changed/missing counts. The caller
    owns the supplied DB connection.
    """
    def check_cancelled():
        if cancel_callback and cancel_callback():
            raise DeviceInventoryCancelled()

    check_cancelled()
    device_row = db.upsert_device(device_record_from_info(device))
    device_key = device_row["stable_device_key"]
    mount_path = getattr(device, "mount_path", "")
    roots = device_music_roots(device)
    skipped = []
    if status_callback:
        status_callback("Loading cached device inventory...")
    if not roots:
        return {
            "device_key": device_key,
            "scanned": 0,
            "new": 0,
            "changed": 0,
            "missing": 0,
            "skipped": [{"path": getattr(device, "music_path", ""), "reason": "music root missing"}],
            "scan_roots": [],
        }

    cached = db.get_device_track_states(device_key)
    # Presence must come from the canonical paths returned by os.walk, not
    # cached spellings. FAT is case-insensitive, so an obsolete cached path can
    # still pass os.path.exists() after a case-only rename and remain as a
    # duplicate inventory row indefinitely.
    present_paths = set()
    scanned = new = changed = 0
    needs_relink = bool(force_full)
    compute_hashes = duplicate_strictness in ("metadata_and_hash", "file_hash_only")
    metadata_reader_func = metadata_reader_func or read_metadata
    file_hash_func = file_hash_func or compute_file_hash
    video_manifest = _read_rockpod_video_manifest(mount_path)

    imported_from_tagcache = False
    inventory_confirmed = False
    tagcache_track_count = 0
    filesystem_track_count = 0
    confirmation_reason = ""
    filesystem_paths = set()
    tagcache_state = detect_rockbox_database_state(device, device_row)
    if tagcache_state["database_present"]:
        try:
            if status_callback:
                status_callback("Reading Rockbox database...")
            imported_tracks = read_rockbox_tagcache_tracks(mount_path)
            if imported_tracks:
                imported_from_tagcache = True
                tagcache_track_count = len(imported_tracks)
                if status_callback:
                    status_callback("Counting music files on device...")
                filesystem_paths = _scan_device_audio_paths(
                    roots,
                    mount_path,
                    cancel_callback=cancel_callback,
                )
                canonical_paths = {
                    _normalize_device_path(path): path
                    for path in filesystem_paths
                }
                for dev_data in imported_tracks:
                    check_cancelled()
                    rel = canonical_paths.get(
                        _normalize_device_path(dev_data["device_path"])
                    )
                    if not rel:
                        continue
                    dev_data = {**dev_data, "device_path": rel}
                    cached_row = cached.get(rel)
                    if cached_row and not force_full:
                        if not cached_row.get("local_track_id"):
                            needs_relink = True
                        if _device_row_changed(cached_row, dev_data):
                            changed += 1
                    else:
                        if cached_row:
                            changed += 1
                        else:
                            new += 1
                        needs_relink = True
                    db.upsert_device_track({"device_id": device_key, **dev_data})
                    present_paths.add(rel)
                    scanned += 1
                filesystem_track_count = len(filesystem_paths)
                extra_paths = _filesystem_only_paths(filesystem_paths, present_paths)
                if extra_paths:
                    if status_callback:
                        status_callback("Reconciling files missing from Rockbox database...")
                    for rel in extra_paths:
                        check_cancelled()
                        present_paths.add(rel)
                        try:
                            dev_data, is_new, is_changed, relink_needed = _import_device_file(
                                db,
                                cached,
                                device_key,
                                mount_path,
                                rel,
                                force_full,
                                compute_hashes,
                                metadata_reader_func,
                                file_hash_func,
                                video_manifest,
                            )
                            db.upsert_device_track(dev_data)
                            scanned += 1
                            if is_new:
                                new += 1
                            elif is_changed:
                                changed += 1
                            if relink_needed:
                                needs_relink = True
                        except Exception as exc:
                            _record_skip(skipped, os.path.join(mount_path, rel), f"parse failure: {exc}")
                            logger.warning("Could not reconcile device track %s: %s", rel, exc)
                inventory_confirmed = {
                    _normalize_device_path(path) for path in present_paths
                } == {
                    _normalize_device_path(path) for path in filesystem_paths
                }
                confirmation_reason = "matched" if inventory_confirmed else "path_mismatch"
                logger.info("Imported %d device tracks from Rockbox tagcache", scanned)
        except TagcacheError as exc:
            logger.warning("Could not import Rockbox tagcache: %s", exc)
            confirmation_reason = "tagcache_error"

    if not imported_from_tagcache:
        if status_callback:
            status_callback("Verifying device files...")
        for scan_root in roots:
            check_cancelled()
            logger.info("Scanning device music root: %s", scan_root)
            for root, dirs, files in os.walk(scan_root):
                check_cancelled()
                dirs[:] = [d for d in dirs if not d.startswith(".")]
                for fn in files:
                    check_cancelled()
                    full = os.path.join(root, fn)
                    if Path(fn).suffix.lower() not in DEVICE_TRACK_FORMATS:
                        _record_skip(skipped, full, "unsupported format")
                        logger.debug("Skipped device file %s: unsupported format", full)
                        continue
                    rel = os.path.relpath(full, mount_path)
                    if rel in present_paths:
                        _record_skip(skipped, full, "duplicate scan path")
                        continue
                    present_paths.add(rel)
                    try:
                        dev_data, is_new, is_changed, relink_needed = _import_device_file(
                            db,
                            cached,
                            device_key,
                            mount_path,
                            rel,
                            force_full,
                            compute_hashes,
                            metadata_reader_func,
                            file_hash_func,
                            video_manifest,
                        )
                        if relink_needed:
                            needs_relink = True
                        if is_new:
                            new += 1
                        elif is_changed:
                            changed += 1
                        db.upsert_device_track(dev_data)
                        scanned += 1
                    except Exception as exc:
                        _record_skip(skipped, full, f"parse failure: {exc}")
                        logger.warning("Could not verify device track %s: %s", full, exc)
        inventory_confirmed = True
        confirmation_reason = "filesystem_scan"
        filesystem_track_count = scanned

    if status_callback:
        status_callback("Matching device tracks to library...")
    missing = 0
    if imported_from_tagcache and not inventory_confirmed:
        logger.warning(
            "Rockbox tagcache path mismatch for %s: tagcache=%d filesystem=%d; "
            "reconciling cache using filesystem paths without full metadata scan",
            device_key,
            tagcache_track_count,
            filesystem_track_count,
        )
    check_cancelled()
    missing = db.mark_missing_device_tracks(device_key, present_paths)
    if missing:
        needs_relink = True
    if needs_relink:
        _link_device_to_local(
            db,
            device_key,
            duplicate_strictness,
            duration_tolerance,
            cancel_callback=cancel_callback,
        )
    check_cancelled()
    if status_callback:
        status_callback("Importing Rockbox play stats...")
    runtime_imported = import_runtime_data_for_device(db, device)
    check_cancelled()
    if status_callback:
        status_callback("Importing Rockbox playlists...")
    playlists_imported = import_device_playlists(db, device)
    if status_callback:
        status_callback("Updating cached device inventory...")
    db.mark_device_scanned(device_key)
    db.commit()
    return {
        "device_key": device_key,
        "scanned": scanned,
        "new": new,
        "changed": changed,
        "missing": missing,
        "inventory_source": "rockbox_tagcache" if imported_from_tagcache else "filesystem",
        "inventory_confirmed": inventory_confirmed,
        "inventory_confirmation_reason": confirmation_reason,
        "tagcache_track_count": tagcache_track_count,
        "filesystem_track_count": filesystem_track_count,
        "runtime_imported": runtime_imported,
        "playlists_imported": playlists_imported,
        "skipped": skipped,
        "scan_roots": roots,
    }


def _record_skip(skipped, path, reason, limit=100):
    if len(skipped) < limit:
        skipped.append({"path": path, "reason": reason})


def _device_row_changed(cached_row, new_row):
    keys = (
        "file_size",
        "title",
        "artist",
        "album",
        "album_artist",
        "genre",
        "year",
        "track_number",
        "disc_number",
        "duration",
        "bitrate",
        "codec",
        "metadata_hash",
    )
    for key in keys:
        if (cached_row.get(key) or "") != (new_row.get(key) or ""):
            return True
    return False


def _link_device_to_local(db, device_key, duplicate_strictness, duration_tolerance=2.0,
                          cancel_callback=None):
    if cancel_callback and cancel_callback():
        raise DeviceInventoryCancelled()
    device_tracks = db.get_all_device_tracks(device_key)
    # Link all local media types so videos on device are correctly reflected
    # in sync status and "not on iPod" indicators.
    local_tracks = db.get_all_tracks(media_type=None)
    matcher = TrackMatcher(duplicate_strictness, duration_tolerance)
    matched, unmatched, _, _ = matcher.match_all(local_tracks, device_tracks)

    synced_updates = [
        (
            row.get("local_track_id"),
            row.get("device_path", ""),
            row.get("metadata_hash", ""),
            row.get("file_hash", ""),
        )
        for row in (dict(track) for track in device_tracks)
        if row.get("local_track_id") and row.get("present_on_device", 1)
    ]
    for result in matched:
        if cancel_callback and cancel_callback():
            raise DeviceInventoryCancelled()
        lt = dict(result.local_track)
        dt = dict(result.device_track)
        local_id = lt.get("id")
        dev_id = dt.get("id")
        if local_id and dev_id:
            db.execute(
                "UPDATE device_tracks SET local_track_id = ? WHERE id = ?",
                (local_id, dev_id),
            )
            synced_updates.append((
                local_id,
                dt.get("device_path", ""),
                dt.get("metadata_hash", ""),
                dt.get("file_hash", ""),
            ))

    db.mark_synced_many(synced_updates)

    for result in unmatched:
        if cancel_callback and cancel_callback():
            raise DeviceInventoryCancelled()
        lt = dict(result.local_track)
        local_id = lt.get("id")
        if local_id:
            linked_elsewhere = db.execute(
                "SELECT 1 FROM device_tracks "
                "WHERE local_track_id = ? AND present_on_device = 1 LIMIT 1",
                (local_id,),
            ).fetchone()
            if linked_elsewhere:
                continue
            db.execute(
                "UPDATE tracks SET synced_to_device = 0, device_path = NULL, "
                "last_synced_metadata_hash = '', last_synced_file_hash = '' "
                "WHERE id = ?",
                (local_id,),
            )


class DeviceInventoryWorker(QObject):
    finished = Signal(dict)
    error = Signal(str)
    status = Signal(str)
    cancelled = Signal()

    def __init__(self, db_path, device, duplicate_strictness, force_full=False,
                 duration_tolerance=2.0):
        super().__init__()
        self._db_path = db_path
        self._device = device
        self._duplicate_strictness = duplicate_strictness
        self._force_full = force_full
        self._duration_tolerance = duration_tolerance
        self._cancelled = False

    def cancel(self):
        self._cancelled = True

    @Slot()
    def run(self):
        db = None
        try:
            db = Database(self._db_path, initialize=False)
            with db.write_lock():
                summary = verify_device_inventory(
                    db,
                    self._device,
                    self._duplicate_strictness,
                    self._force_full,
                    duration_tolerance=self._duration_tolerance,
                    status_callback=self.status.emit,
                    cancel_callback=lambda: self._cancelled,
                )
            self.finished.emit(summary)
        except DeviceInventoryCancelled:
            logger.info("Device inventory verification cancelled")
            self.cancelled.emit()
        except Exception as exc:
            logger.exception("Device inventory verification failed")
            self.error.emit(str(exc))
        finally:
            if db:
                db.close()


class DeviceInventoryVerifier(QObject):
    finished = Signal(dict)
    error = Signal(str)
    status = Signal(str)
    cancelled = Signal()

    def __init__(self, config, parent=None):
        super().__init__(parent)
        self._config = config
        self._thread = None
        self._worker = None

    @property
    def is_running(self):
        return self._thread is not None and self._thread.isRunning()

    def start(self, device, force_full=False):
        if self.is_running:
            return False
        self._thread = QThread()
        self._worker = DeviceInventoryWorker(
            self._config.db_path,
            device,
            self._config.duplicate_strictness,
            force_full,
            self._config.get("duration_match_tolerance_seconds", 2.0),
        )
        self._worker.moveToThread(self._thread)
        self._thread.started.connect(self._worker.run)
        self._worker.finished.connect(self._on_finished, Qt.QueuedConnection)
        self._worker.error.connect(self._on_error, Qt.QueuedConnection)
        self._worker.status.connect(self.status, Qt.QueuedConnection)
        self._worker.cancelled.connect(self._on_cancelled, Qt.QueuedConnection)
        self._thread.start()
        return True

    @Slot(dict)
    def _on_finished(self, summary):
        self._cleanup()
        self.finished.emit(summary)

    @Slot(str)
    def _on_error(self, message):
        self._cleanup()
        self.error.emit(message)

    @Slot()
    def _on_cancelled(self):
        self._cleanup()
        self.cancelled.emit()

    def _cleanup(self):
        if self._thread:
            self._thread.quit()
            self._thread.wait(5000)
        self._thread = None
        self._worker = None

    def shutdown(self):
        if self._worker:
            self._worker.cancel()
        if self._thread and self._thread.isRunning():
            self._thread.quit()
            if not self._thread.wait(15000):
                logger.warning("Waiting for device inventory to reach a safe cancellation point")
                self._thread.wait()
        self._thread = None
        self._worker = None
