"""Sync engine — copies tracks to device with metadata change detection.

Handles:
  - Copying new/missing tracks to device
  - Re-copying tracks whose metadata or file content has changed
  - Building the device folder layout
  - Force-full-resync mode
  - Cancellation and progress reporting
"""

import logging
import os
import re
import shutil
import time
import unicodedata
from collections import defaultdict
from datetime import datetime
from pathlib import Path

from PySide6.QtCore import QObject, Signal, QThread, Slot, Qt

from app.database import Database
from services.audio_transcode import AudioSyncTranscoder
from services.rockbox_device import detect_rockbox_database_state
from services.metadata_reader import read_metadata, compute_file_hash, CODEC_MAP, VIDEO_EXTENSIONS
from services.reconciliation import summarize_unmatched_tracks
from services.track_matcher import TrackMatcher
from services.device_inventory import device_music_roots, device_record_from_info, verify_device_inventory

logger = logging.getLogger(__name__)
COPY_CHUNK_SIZE = 4 * 1024 * 1024
LEGACY_COPY_MODE = os.environ.get("ROCKPOD_SYNC_LEGACY_COPY", "").strip().lower() in {"1", "true", "yes", "on"}
AUDIO_EXTENSIONS = {ext for ext in CODEC_MAP.keys() if ext not in VIDEO_EXTENSIONS}


def _sanitize_filename(name, max_len=200):
    """Make a string safe for use as a filename on FAT32."""
    if not name:
        return "Unknown"
    # Normalize unicode
    name = unicodedata.normalize("NFC", name)
    # Remove or replace forbidden chars
    name = re.sub(r'[<>:"/\\|?*]', "_", name)
    # Collapse whitespace
    name = re.sub(r"\s+", " ", name).strip()
    # Trim length
    if len(name) > max_len:
        name = name[:max_len].rstrip()
    return name or "Unknown"


def _bump_reason(reason_counts, reason):
    if not reason:
        return
    reason_counts[reason] = reason_counts.get(reason, 0) + 1


def _sync_update_reasons(row, device_row=None, old_dev_path="", new_rel_path=""):
    local = dict(row) if hasattr(row, "keys") else dict(row or {})
    device = dict(device_row) if hasattr(device_row, "keys") else dict(device_row or {})
    reasons = []

    if local.get("sync_transcoded"):
        reasons.append("conversion required")

    local_mh = str(local.get("metadata_hash") or "").strip()
    device_mh = str(device.get("metadata_hash") or "").strip()
    last_synced_mh = str(local.get("last_synced_metadata_hash") or "").strip()
    if (local_mh and device_mh and local_mh != device_mh) or (
        local_mh and last_synced_mh and local_mh != last_synced_mh
    ):
        reasons.append("metadata changed")

    local_fh = str(local.get("file_hash") or "").strip()
    device_fh = str(device.get("file_hash") or "").strip()
    last_synced_fh = str(local.get("last_synced_file_hash") or "").strip()
    if (local_fh and device_fh and local_fh != device_fh) or (
        local_fh and last_synced_fh and local_fh != last_synced_fh
    ):
        reasons.append("file changed")

    if old_dev_path and new_rel_path and old_dev_path != new_rel_path:
        reasons.append("path changed")

    return reasons


def _dedupe_normalize(value):
    if not value:
        return ""
    text = unicodedata.normalize("NFKC", str(value)).strip().casefold()
    text = re.sub(r"\s+", " ", text)
    return text


def _dedupe_artist_key(row):
    artist = _dedupe_normalize(row.get("artist", ""))
    album_artist = _dedupe_normalize(row.get("album_artist", ""))
    keys = sorted({item for item in (artist, album_artist) if item})
    return keys[0] if keys else ""


def _dedupe_duration(value):
    try:
        return round(float(value or 0))
    except (TypeError, ValueError):
        return 0


def _dedupe_group_key(row):
    item = dict(row) if hasattr(row, "keys") else row
    title = _dedupe_normalize(item.get("title", ""))
    artist = _dedupe_artist_key(item)
    album = _dedupe_normalize(item.get("album", ""))
    if title and artist and album:
        return ("identity", artist, album, title)
    return None


def build_device_path(track_row, dir_template, file_template):
    """Build the target path on the device for a track.

    Returns a relative path like 'Music/Artist/Album/01 - Title.mp3'
    """
    d = dict(track_row) if hasattr(track_row, "keys") else track_row

    album_artist = _sanitize_filename(d.get("album_artist") or d.get("artist") or "Unknown Artist")
    album = _sanitize_filename(d.get("album") or "Unknown Album")
    title = _sanitize_filename(d.get("title") or "Unknown")
    ext = d.get("sync_output_ext") or Path(d.get("file_path", "")).suffix or ".mp3"
    tn = d.get("track_number")
    dn = d.get("disc_number", 1) or 1
    media_type = str(d.get("media_type") or "audio").lower()
    source_path = str(d.get("file_path") or "")
    source_parts = [part.casefold() for part in Path(source_path).parts]
    is_downloaded_video = (
        media_type == "video"
        and ext.lower() in {".mpg", ".mpeg", ".mpe"}
        and "youtube" in source_parts
    )
    if is_downloaded_video and _is_generic_downloaded_video_title(title):
        title = _sanitize_filename(Path(source_path).stem or "Downloaded Video")
    video_kind_label = {
        "movie": "Movies",
        "show": "TV Shows",
        "home_video": "Home Videos",
    }.get(str(d.get("video_kind") or "movie"), "Movies")
    video_sync_category = str(d.get("video_sync_category") or "").strip()
    if is_downloaded_video:
        video_sync_category = video_sync_category or "Downloaded"
    if video_sync_category:
        video_kind_label = video_sync_category

    if media_type == "video" and str(dir_template or "").startswith("Music/"):
        dir_template = "Videos/{video_kind}"
        file_template = "{title}{ext}"

    try:
        track_num = int(tn) if tn else 0
    except (ValueError, TypeError):
        track_num = 0

    try:
        dir_path = dir_template.format(
            album_artist=album_artist,
            artist=_sanitize_filename(d.get("artist") or "Unknown Artist"),
            album=album,
            genre=_sanitize_filename(d.get("genre") or "Unknown"),
            video_kind=_sanitize_filename(video_kind_label),
            year=d.get("year") or "0000",
        )
    except (KeyError, ValueError):
        dir_path = f"Music/{album_artist}/{album}"

    try:
        filename = file_template.format(
            track_number=track_num,
            disc_number=dn,
            title=title,
            artist=_sanitize_filename(d.get("artist") or ""),
            ext=ext,
        )
    except (KeyError, ValueError):
        filename = f"{track_num:02d} - {title}{ext}"

    return os.path.join(dir_path, filename)


def _is_generic_downloaded_video_title(title):
    return str(title or "").strip().casefold() in {
        "",
        "unknown",
        "youtube",
        "mpeg video",
        "youtube mpeg video",
    }


def _device_cache_title(row):
    title = str(row.get("title") or "").strip()
    file_path = str(row.get("file_path") or "")
    media_type = str(row.get("media_type") or "audio").lower()
    ext = Path(file_path).suffix.lower()
    source_parts = [part.casefold() for part in Path(file_path).parts]
    if (
        media_type == "video"
        and ext in {".mpg", ".mpeg", ".mpe"}
        and "youtube" in source_parts
        and _is_generic_downloaded_video_title(title)
    ):
        return Path(file_path).stem or "Downloaded Video"
    return title


def _remove_audio_with_sidecars(path):
    """Remove a device audio file and its plain-text lyric sidecar when present."""
    removed = []
    targets = [path]
    base, _ext = os.path.splitext(path)
    targets.append(base + ".lrc")

    for target in targets:
        if os.path.exists(target):
            os.remove(target)
            removed.append(target)
    return removed


def _source_lyrics_sidecar_path(track_row):
    row = dict(track_row) if hasattr(track_row, "keys") else dict(track_row or {})
    source_path = str(row.get("file_path") or "").strip()
    if not source_path:
        return ""
    return str(Path(source_path).with_suffix(".lrc"))


def _clear_device_trash(device_mount):
    """Remove desktop trash folders from the device root so Rockbox does not index them."""
    removed = []
    try:
        names = os.listdir(device_mount)
    except OSError:
        return removed

    for name in names:
        if not str(name).startswith(".Trash-"):
            continue
        full = os.path.join(device_mount, name)
        if not os.path.isdir(full):
            continue
        shutil.rmtree(full, ignore_errors=True)
        if not os.path.exists(full):
            removed.append(full)
    return removed


class SyncPlan:
    """Describes what a sync operation will do before executing it."""

    def __init__(self):
        self.to_copy = []       # list of (track_row, device_rel_path)
        self.to_resync = []     # list of (track_row, existing_device_path, new_device_rel_path)
        self.artwork_to_copy = []  # list of (cache_cover_path, device_rel_path, album_key)
        self.to_delete = []     # list of device_paths to remove (orphaned)
        self.up_to_date = []    # list of MatchResult already present on device
        self.total_bytes = 0
        self.transcode_count = 0
        self.errors = []
        self.plan_profile = {}
        self.execution_profile = {}
        self.copy_reason_counts = {}
        self.update_reason_counts = {}

    @property
    def total_operations(self):
        return len(self.to_copy) + len(self.to_resync) + len(self.artwork_to_copy) + len(self.to_delete)

    @property
    def copy_count(self):
        return len(self.to_copy)

    @property
    def resync_count(self):
        return len(self.to_resync)

    @property
    def up_to_date_count(self):
        return len(self.up_to_date)

    def summary(self):
        parts = []
        if self.to_copy:
            parts.append(f"{len(self.to_copy)} new tracks to copy")
        if self.to_resync:
            parts.append(f"{len(self.to_resync)} tracks to update")
        if self.artwork_to_copy:
            parts.append(f"{len(self.artwork_to_copy)} artwork files to update")
        if self.up_to_date:
            parts.append(f"{len(self.up_to_date)} tracks already up to date")
        if self.to_delete:
            parts.append(f"{len(self.to_delete)} orphaned tracks on device")
        if self.transcode_count:
            parts.append(f"{self.transcode_count} tracks converted for device sync")
        mb = self.total_bytes / (1024 * 1024)
        if mb > 0:
            parts.append(f"{mb:.1f} MB to transfer")
        return "; ".join(parts) if parts else "Nothing to sync"


class SyncWorker(QObject):
    """Background worker that executes a sync plan.

    Creates its own SQLite connection so all DB writes happen in the
    worker thread — SQLite connections cannot be shared across threads.
    """

    progress = Signal(int, int, str)    # current, total, description
    file_copied = Signal(str, str)      # source, dest
    file_error = Signal(str, str)       # path, error message
    finished = Signal(int, int, int)    # copied, failed, skipped
    cancelled = Signal()

    def __init__(self, plan, device_mount, db_path):
        super().__init__()
        self._plan = plan
        self._device_mount = device_mount
        self._db_path = db_path
        self._cancelled = False

    def cancel(self):
        self._cancelled = True

    def run(self):
        copied = 0
        failed = 0
        skipped = 0
        total = self._plan.total_operations
        db = None
        created_dirs = set()
        synced_updates = []
        stage_times = {
            "track_copy_seconds": 0.0,
            "artwork_copy_seconds": 0.0,
            "local_file_read_seconds": 0.0,
            "device_write_seconds": 0.0,
            "mkdir_seconds": 0.0,
            "open_seconds": 0.0,
            "flush_sync_seconds": 0.0,
            "replace_seconds": 0.0,
            "db_finalize_seconds": 0.0,
            "artwork_generation_seconds": float(self._plan.plan_profile.get("artwork_generation_seconds", 0.0) or 0.0),
        }
        t_sync_start = time.perf_counter()

        try:
            db = Database(self._db_path, initialize=False)

            # Process copies (new tracks)
            for i, (track_row, rel_path) in enumerate(self._plan.to_copy):
                if self._cancelled:
                    self.cancelled.emit()
                    return

                row = dict(track_row) if hasattr(track_row, "keys") else track_row
                src = row.get("sync_source_path") or row.get("file_path", "")
                dest = os.path.join(self._device_mount, rel_path)
                desc = f"Copying tracks: {os.path.basename(row.get('file_path', '') or src)}"
                self.progress.emit(i + 1, total, desc)

                copy_started = time.perf_counter()
                ok, copy_stats = self._copy_file(src, dest, created_dirs)
                stage_times["track_copy_seconds"] += time.perf_counter() - copy_started
                stage_times["local_file_read_seconds"] += copy_stats.get("read_seconds", 0.0)
                stage_times["device_write_seconds"] += copy_stats.get("write_seconds", 0.0)
                stage_times["mkdir_seconds"] += copy_stats.get("mkdir_seconds", 0.0)
                stage_times["open_seconds"] += copy_stats.get("open_seconds", 0.0)
                stage_times["flush_sync_seconds"] += copy_stats.get("flush_sync_seconds", 0.0)
                stage_times["replace_seconds"] += copy_stats.get("replace_seconds", 0.0)
                if ok:
                    copied += 1
                    self.file_copied.emit(src, dest)
                    self._sync_lyrics_sidecar(row, dest, created_dirs)
                    tid = row.get("id")
                    if tid:
                        mh = row.get("metadata_hash", "")
                        fh = row.get("file_hash", "")
                        synced_updates.append((tid, rel_path, mh, fh))
                else:
                    failed += 1

            # Process resyncs (metadata/content changed)
            offset = len(self._plan.to_copy)
            for i, (track_row, old_dev_path, new_rel_path) in enumerate(self._plan.to_resync):
                if self._cancelled:
                    self.cancelled.emit()
                    return

                row = dict(track_row) if hasattr(track_row, "keys") else track_row
                src = row.get("sync_source_path") or row.get("file_path", "")
                desc = f"Copying tracks: {os.path.basename(row.get('file_path', '') or src)}"
                self.progress.emit(offset + i + 1, total, desc)

                dest = os.path.join(self._device_mount, new_rel_path)
                copy_started = time.perf_counter()
                ok, copy_stats = self._copy_file(src, dest, created_dirs)
                stage_times["track_copy_seconds"] += time.perf_counter() - copy_started
                stage_times["local_file_read_seconds"] += copy_stats.get("read_seconds", 0.0)
                stage_times["device_write_seconds"] += copy_stats.get("write_seconds", 0.0)
                stage_times["mkdir_seconds"] += copy_stats.get("mkdir_seconds", 0.0)
                stage_times["open_seconds"] += copy_stats.get("open_seconds", 0.0)
                stage_times["flush_sync_seconds"] += copy_stats.get("flush_sync_seconds", 0.0)
                stage_times["replace_seconds"] += copy_stats.get("replace_seconds", 0.0)
                if ok:
                    copied += 1
                    self.file_copied.emit(src, dest)
                    self._sync_lyrics_sidecar(row, dest, created_dirs)
                    if old_dev_path and old_dev_path != new_rel_path:
                        old_full = os.path.join(self._device_mount, old_dev_path)
                        try:
                            removed = _remove_audio_with_sidecars(old_full)
                            if removed:
                                logger.debug("Removed old device file(s): %s", ", ".join(removed))
                        except OSError as e:
                            logger.warning("Could not remove old file %s: %s", old_full, e)
                    tid = row.get("id")
                    if tid:
                        mh = row.get("metadata_hash", "")
                        fh = row.get("file_hash", "")
                        synced_updates.append((tid, new_rel_path, mh, fh))
                else:
                    failed += 1

            artwork_offset = len(self._plan.to_copy) + len(self._plan.to_resync)
            for i, (src_cover, rel_path, _album_key) in enumerate(self._plan.artwork_to_copy):
                if self._cancelled:
                    self.cancelled.emit()
                    return
                desc = f"Syncing artwork: {os.path.basename(os.path.dirname(rel_path))}"
                self.progress.emit(artwork_offset + i + 1, total, desc)
                dest = os.path.join(self._device_mount, rel_path)
                copy_started = time.perf_counter()
                ok, copy_stats = self._copy_file(src_cover, dest, created_dirs)
                stage_times["artwork_copy_seconds"] += time.perf_counter() - copy_started
                stage_times["local_file_read_seconds"] += copy_stats.get("read_seconds", 0.0)
                stage_times["device_write_seconds"] += copy_stats.get("write_seconds", 0.0)
                stage_times["mkdir_seconds"] += copy_stats.get("mkdir_seconds", 0.0)
                stage_times["open_seconds"] += copy_stats.get("open_seconds", 0.0)
                stage_times["flush_sync_seconds"] += copy_stats.get("flush_sync_seconds", 0.0)
                stage_times["replace_seconds"] += copy_stats.get("replace_seconds", 0.0)
                if ok:
                    copied += 1
                    self.file_copied.emit(src_cover, dest)
                else:
                    failed += 1

            delete_offset = (
                len(self._plan.to_copy)
                + len(self._plan.to_resync)
                + len(self._plan.artwork_to_copy)
            )
            for i, rel_path in enumerate(self._plan.to_delete):
                if self._cancelled:
                    self.cancelled.emit()
                    return
                desc = f"Removing duplicate: {os.path.basename(rel_path)}"
                self.progress.emit(delete_offset + i + 1, total, desc)
                dest = os.path.join(self._device_mount, rel_path)
                try:
                    removed = _remove_audio_with_sidecars(dest)
                    if removed:
                        logger.debug("Removed duplicate device file(s): %s", ", ".join(removed))
                except OSError as exc:
                    failed += 1
                    self.file_error.emit(dest, str(exc))
                    logger.error("Failed to delete duplicate device file %s: %s", dest, exc)

            cleanup_index = total + 1 if total else 1
            self.progress.emit(cleanup_index, total + 2, "Clearing device trash")
            removed_trash = _clear_device_trash(self._device_mount)
            if removed_trash:
                logger.info("Cleared device trash folders: %s", ", ".join(removed_trash))

            finalize_index = total + 2 if total else 2
            self.progress.emit(finalize_index, total + 2, "Finalizing")
            finalize_started = time.perf_counter()
            if synced_updates:
                with db.transaction():
                    for tid, rel_path, mh, fh in synced_updates:
                        db.mark_synced(tid, rel_path, mh, fh)
            stage_times["db_finalize_seconds"] += time.perf_counter() - finalize_started
            self._plan.execution_profile = {
                **stage_times,
                "copied": copied,
                "failed": failed,
                "skipped": skipped,
                "total_seconds": time.perf_counter() - t_sync_start,
            }
            logger.info(
                "Sync timing summary: plan=%.3fs track_copy=%.3fs artwork_gen=%.3fs artwork_copy=%.3fs "
                "read=%.3fs write=%.3fs mkdir=%.3fs open=%.3fs flush_sync=%.3fs replace=%.3fs db_finalize=%.3fs total=%.3fs",
                float(self._plan.plan_profile.get("build_seconds", 0.0) or 0.0),
                stage_times["track_copy_seconds"],
                stage_times["artwork_generation_seconds"],
                stage_times["artwork_copy_seconds"],
                stage_times["local_file_read_seconds"],
                stage_times["device_write_seconds"],
                stage_times["mkdir_seconds"],
                stage_times["open_seconds"],
                stage_times["flush_sync_seconds"],
                stage_times["replace_seconds"],
                stage_times["db_finalize_seconds"],
                self._plan.execution_profile["total_seconds"],
            )
            self.finished.emit(copied, failed, skipped)

        except Exception as e:
            logger.exception("Sync execution failed")
            self.finished.emit(copied, failed, skipped)
        finally:
            if db:
                db.close()

    def _copy_file(self, src, dest, created_dirs=None):
        """Copy a file safely with parent directory creation."""
        stats = {
            "read_seconds": 0.0,
            "write_seconds": 0.0,
            "mkdir_seconds": 0.0,
            "open_seconds": 0.0,
            "flush_sync_seconds": 0.0,
            "replace_seconds": 0.0,
        }
        try:
            if not os.path.isfile(src):
                self.file_error.emit(src, "Source file not found")
                return False, stats

            dest_dir = os.path.dirname(dest)
            if created_dirs is not None:
                if dest_dir not in created_dirs:
                    t_mkdir = time.perf_counter()
                    os.makedirs(dest_dir, exist_ok=True)
                    stats["mkdir_seconds"] += time.perf_counter() - t_mkdir
                    created_dirs.add(dest_dir)
            else:
                t_mkdir = time.perf_counter()
                os.makedirs(dest_dir, exist_ok=True)
                stats["mkdir_seconds"] += time.perf_counter() - t_mkdir

            # Copy to a temp file first, then replace the destination.
            # Rockbox does not need source timestamps preserved on device files,
            # so avoid extra metadata syscalls on slow removable storage.
            tmp = dest + ".rockpod_tmp"
            try:
                t_open = time.perf_counter()
                with open(src, "rb") as src_handle, open(tmp, "wb") as tmp_handle:
                    stats["open_seconds"] += time.perf_counter() - t_open
                    chunk_size = 1024 * 1024 if LEGACY_COPY_MODE else COPY_CHUNK_SIZE
                    while True:
                        t_read = time.perf_counter()
                        chunk = src_handle.read(chunk_size)
                        stats["read_seconds"] += time.perf_counter() - t_read
                        if not chunk:
                            break
                        t_write = time.perf_counter()
                        tmp_handle.write(chunk)
                        stats["write_seconds"] += time.perf_counter() - t_write
                    t_flush = time.perf_counter()
                    tmp_handle.flush()
                    sync_fn = os.fsync if LEGACY_COPY_MODE or not hasattr(os, "fdatasync") else os.fdatasync
                    sync_fn(tmp_handle.fileno())
                    stats["flush_sync_seconds"] += time.perf_counter() - t_flush
                if LEGACY_COPY_MODE:
                    try:
                        shutil.copystat(src, tmp, follow_symlinks=True)
                    except OSError:
                        pass
                    if os.path.exists(dest):
                        os.remove(dest)
                    t_replace = time.perf_counter()
                    os.rename(tmp, dest)
                    stats["replace_seconds"] += time.perf_counter() - t_replace
                else:
                    t_replace = time.perf_counter()
                    os.replace(tmp, dest)
                    stats["replace_seconds"] += time.perf_counter() - t_replace
                return True, stats
            except Exception:
                # Clean up temp file on failure
                if os.path.exists(tmp):
                    try:
                        os.remove(tmp)
                    except OSError:
                        pass
                raise

        except Exception as e:
            self.file_error.emit(src, str(e))
            logger.error("Failed to copy %s -> %s: %s", src, dest, e)
            return False, stats

    def _sync_lyrics_sidecar(self, track_row, audio_dest, created_dirs=None):
        """Mirror a local .lrc sidecar to the device next to the synced audio file."""
        source_sidecar = _source_lyrics_sidecar_path(track_row)
        dest_sidecar = os.path.splitext(audio_dest)[0] + ".lrc"
        if source_sidecar and os.path.isfile(source_sidecar):
            self._copy_file(source_sidecar, dest_sidecar, created_dirs)
            return
        if os.path.exists(dest_sidecar):
            try:
                os.remove(dest_sidecar)
            except OSError as exc:
                self.file_error.emit(dest_sidecar, str(exc))
                logger.warning("Failed to remove stale lyrics sidecar %s: %s", dest_sidecar, exc)


class SyncEngine(QObject):
    """High-level sync coordinator.

    This is a QObject so that signals from the worker thread are delivered
    to slots on this object via Qt's queued connection mechanism, ensuring
    all callbacks run on the main thread where the UI and main-thread DB
    connection live.
    """

    # Re-emitted on the main thread for UI consumers
    sync_progress = Signal(int, int, str)    # current, total, description
    sync_file_copied = Signal(str, str)      # source, dest
    sync_file_error = Signal(str, str)       # path, error message
    sync_finished = Signal(int, int, int)    # copied, failed, skipped
    sync_cancelled = Signal()
    sync_error = Signal(str)

    def __init__(self, db, config, device_detector, artwork_manager=None, parent=None):
        super().__init__(parent)
        self._db = db
        self._config = config
        self._device_detector = device_detector
        self._artwork_manager = artwork_manager
        self._audio_transcoder = AudioSyncTranscoder(
            os.path.join(self._config.cache_dir, "device_transcodes")
        )
        self._thread = None
        self._worker = None
        self._device_tracks = []
        self._device_file_state = {}
        self._current_device_key = ""

    @property
    def is_syncing(self):
        return self._thread is not None and self._thread.isRunning()

    @property
    def current_device_key(self):
        return self._current_device_key

    def _config_value(self, key, default=None):
        device = self._device_detector.current_device
        if device is not None:
            return self._config.get_effective(key, device=device, default=default)
        if self._current_device_key:
            return self._config.get_effective(
                key,
                stable_device_key=self._current_device_key,
                default=default,
            )
        return self._config.get(key, default)

    def _match_settings(self):
        return (
            self._config_value("duplicate_strictness", "metadata_and_hash"),
            self._config_value("duration_match_tolerance_seconds", 2.0),
        )

    def _matcher(self):
        duplicate_strictness, duration_tolerance = self._match_settings()
        return TrackMatcher(duplicate_strictness, duration_tolerance)

    def _ensure_current_device_key(self):
        if self._current_device_key:
            return self._current_device_key
        device = self._device_detector.current_device
        if not device:
            return ""
        return self.set_current_device(device)

    def _device_inventory_needs_scan(self):
        """True when the current device has never been verified into the cache."""
        if not self._current_device_key:
            return False
        device = self._device_detector.current_device
        if not device:
            return False
        device_row = self._db.get_device_by_key(self._current_device_key)
        if not device_row:
            return True
        row = dict(device_row) if hasattr(device_row, "keys") else device_row
        if self._rockbox_database_newer_than_scan(device, row):
            return True
        if self._device_tracks:
            return False
        if row.get("last_scan_at"):
            return False
        return self._device_has_indexable_media_on_disk(device)

    @staticmethod
    def _rockbox_database_newer_than_scan(device, device_row):
        state = detect_rockbox_database_state(device, device_row)
        latest_mtime = str(state.get("database_latest_mtime") or "").strip()
        if not latest_mtime:
            return False
        last_scan = str((device_row or {}).get("last_scan_at") or "").strip()
        if not last_scan:
            return bool(state.get("database_present"))
        try:
            latest_dt = datetime.fromisoformat(latest_mtime)
            scan_dt = datetime.fromisoformat(last_scan.replace(" ", "T"))
        except ValueError:
            return False
        return latest_dt > scan_dt

    def _device_mount_unavailable(self):
        device = self._device_detector.current_device
        if not device:
            return False
        mount_path = str(getattr(device, "mount_path", "") or "").strip()
        return not mount_path or not os.path.isdir(mount_path)

    @staticmethod
    def _device_has_indexable_media_on_disk(device):
        roots = device_music_roots(device)
        if not roots:
            return False
        for scan_root in roots:
            for root, dirs, files in os.walk(scan_root):
                dirs[:] = [name for name in dirs if not name.startswith(".")]
                for name in files:
                    ext = os.path.splitext(name)[1].lower()
                    if ext in AUDIO_EXTENSIONS:
                        return True
        return False

    def build_sync_plan(self, track_ids=None, force_full=False, status_callback=None):
        """Analyze what needs to be synced and return a SyncPlan.

        Args:
            track_ids: optional set of track IDs to sync (None = all missing)
            force_full: if True, re-copy everything regardless of match state
        """
        def emit_status(message):
            if status_callback:
                status_callback(str(message or "").strip())

        t0 = time.perf_counter()
        device = self._device_detector.current_device
        if not device:
            plan = SyncPlan()
            plan.errors.append("No device connected")
            plan.plan_profile = {"build_seconds": time.perf_counter() - t0}
            return plan
        if self._device_mount_unavailable():
            plan = SyncPlan()
            plan.errors.append("Device mount path is unavailable")
            plan.plan_profile = {"build_seconds": time.perf_counter() - t0}
            return plan

        if self._device_inventory_needs_scan():
            logger.info("Device inventory missing for %s; scanning before sync planning", self._current_device_key)
            emit_status("Scanning device contents...")
            if status_callback:
                self.scan_device(force_full=False, status_callback=status_callback)
            else:
                self.scan_device(force_full=False)

        plan = SyncPlan()
        self._ensure_current_device_key()
        dir_template = self._config_value("device_music_template", "Music/{album_artist}/{album}")
        file_template = self._config_value("device_file_template", "{track_number:02d} - {title}{ext}")
        resync_meta = bool(self._config_value("resync_metadata_changes", True))
        force = force_full or bool(self._config_value("force_full_resync", False))

        stage_times = {
            "local_fetch_seconds": 0.0,
            "device_fetch_seconds": 0.0,
            "match_seconds": 0.0,
            "audio_prepare_seconds": 0.0,
            "artwork_generation_seconds": 0.0,
        }

        # Get local and device tracks
        stage_start = time.perf_counter()
        emit_status("Loading library tracks...")
        if track_ids:
            local_tracks = self._db.get_tracks_by_ids(track_ids)
        else:
            local_tracks = self._db.get_all_tracks()
        stage_times["local_fetch_seconds"] = time.perf_counter() - stage_start
        all_local_tracks = local_tracks if not track_ids else self._db.get_all_tracks()

        stage_start = time.perf_counter()
        emit_status("Preparing tracks for device sync...")
        local_tracks, transcode_errors, transcode_count = self._prepare_tracks_for_sync(local_tracks)
        stage_times["audio_prepare_seconds"] = time.perf_counter() - stage_start
        plan.errors.extend(transcode_errors)
        plan.transcode_count = transcode_count

        stage_start = time.perf_counter()
        emit_status("Loading cached iPod inventory...")
        device_tracks = self.get_device_tracks()
        stage_times["device_fetch_seconds"] = time.perf_counter() - stage_start
        copy_reason_counts = {}
        update_reason_counts = {}

        matched = []
        resync_list = []
        if force:
            # Force mode: re-copy every selected local track
            for lt in local_tracks:
                row = dict(lt)
                rel_path = build_device_path(row, dir_template, file_template)
                old_dev = row.get("device_path", "")
                if old_dev:
                    plan.to_resync.append((row, old_dev, rel_path))
                    for reason in _sync_update_reasons(row, old_dev_path=old_dev, new_rel_path=rel_path):
                        _bump_reason(update_reason_counts, reason)
                    _bump_reason(update_reason_counts, "force full resync")
                else:
                    plan.to_copy.append((row, rel_path))
                    _bump_reason(copy_reason_counts, "force full resync")
                    if row.get("sync_transcoded"):
                        _bump_reason(copy_reason_counts, "conversion required")
                plan.total_bytes += row.get("file_size", 0)
        else:
            # Normal mode: use matcher
            stage_start = time.perf_counter()
            emit_status("Matching library against iPod...")
            matcher = self._matcher()
            matched, unmatched, orphaned, resync_list = matcher.match_all(
                local_tracks, device_tracks
            )
            stage_times["match_seconds"] = time.perf_counter() - stage_start

            used_paths = {dict(dt).get("device_path", "") for dt in device_tracks}
            device_by_path = {
                dict(dt).get("device_path", ""): dict(dt)
                for dt in device_tracks
                if dict(dt).get("device_path", "")
            }
            matched_paths = {
                dict(result.device_track).get("device_path", "")
                for result in matched
                if getattr(result, "device_track", None) is not None
            }

            # New tracks to copy
            for result in unmatched:
                row = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
                desired_rel_path = build_device_path(row, dir_template, file_template)
                existing_row = device_by_path.get(desired_rel_path)
                if (
                    existing_row
                    and desired_rel_path not in matched_paths
                    and not existing_row.get("local_track_id")
                ):
                    used_paths.add(desired_rel_path)
                    plan.to_resync.append((row, desired_rel_path, desired_rel_path))
                    for reason in _sync_update_reasons(
                        row,
                        device_row=existing_row,
                        old_dev_path=desired_rel_path,
                        new_rel_path=desired_rel_path,
                    ):
                        _bump_reason(update_reason_counts, reason)
                else:
                    rel_path = self._unique_device_path(
                        desired_rel_path,
                        used_paths,
                    )
                    used_paths.add(rel_path)
                    plan.to_copy.append((row, rel_path))
                    _bump_reason(
                        copy_reason_counts,
                        matcher.explain_unmatched(row, device_tracks),
                    )
                    if row.get("sync_transcoded"):
                        _bump_reason(copy_reason_counts, "conversion required")
                plan.total_bytes += row.get("file_size", 0) if isinstance(row, dict) else 0

            # Tracks needing resync due to metadata/content changes
            if resync_meta:
                for result in resync_list:
                    row = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
                    device_row = dict(result.device_track) if hasattr(result.device_track, "keys") else result.device_track
                    old_dev = device_row.get("device_path", "") if device_row else row.get("device_path", "")
                    rel_path = self._unique_device_path(
                        build_device_path(row, dir_template, file_template),
                        used_paths,
                        allow_existing=old_dev,
                    )
                    used_paths.add(rel_path)
                    plan.to_resync.append((row, old_dev, rel_path))
                    for reason in _sync_update_reasons(
                        row,
                        device_row=device_row,
                        old_dev_path=old_dev,
                        new_rel_path=rel_path,
                    ):
                        _bump_reason(update_reason_counts, reason)
                    plan.total_bytes += row.get("file_size", 0) if isinstance(row, dict) else 0

            resync_ids = {id(result) for result in resync_list}
            plan.up_to_date = [
                result for result in matched
                if not (resync_meta and id(result) in resync_ids)
            ]

            # Also check for artwork changes (if track matched but artwork_hash differs)
            if resync_meta:
                for result in matched:
                    if result in resync_list:
                        continue
                    row = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
                    ah = row.get("artwork_hash", "")
                    lsah = row.get("last_synced_metadata_hash", "")
                    # Artwork changes are caught by metadata_hash already since
                    # the artwork_hash is separate. Check explicitly.
                    # For now metadata_hash covers tag changes; artwork_hash
                    # requires its own comparison if we want artwork-only resync.
                    # This is a future enhancement — log for now.
                    pass

        duplicate_groups = self._find_duplicate_groups(all_local_tracks, device_tracks)
        for group in duplicate_groups:
            for row in group["remove"]:
                rel_path = row.get("device_path", "")
                if rel_path:
                    plan.to_delete.append(rel_path)

        stage_start = time.perf_counter()
        emit_status("Planning artwork sync...")
        self._populate_artwork_sync_plan(plan, matched, local_tracks)
        stage_times["artwork_generation_seconds"] = time.perf_counter() - stage_start
        plan.plan_profile = {
            **stage_times,
            "build_seconds": time.perf_counter() - t0,
        }
        plan.copy_reason_counts = copy_reason_counts
        plan.update_reason_counts = update_reason_counts
        logger.info(
            "Sync plan timing: total=%.3fs local_fetch=%.3fs audio_prepare=%.3fs device_fetch=%.3fs match=%.3fs artwork=%.3fs "
            "ops(copy=%d,resync=%d,artwork=%d,delete=%d,up_to_date=%d)",
            plan.plan_profile["build_seconds"],
            stage_times["local_fetch_seconds"],
            stage_times["audio_prepare_seconds"],
            stage_times["device_fetch_seconds"],
            stage_times["match_seconds"],
            stage_times["artwork_generation_seconds"],
            len(plan.to_copy),
            len(plan.to_resync),
            len(plan.artwork_to_copy),
            len(plan.to_delete),
            len(plan.up_to_date),
        )
        return plan

    def get_device_tracks(self):
        """Return the current device index, preferring the in-memory cache."""
        self._ensure_current_device_key()
        if self._device_tracks:
            return list(self._device_tracks)
        if not self._current_device_key:
            return []
        return self._db.get_all_device_tracks(self._current_device_key)

    def device_inventory_has_local_links(self):
        """Return whether the cached device inventory can answer match-based counts cheaply."""
        if self._device_inventory_needs_scan():
            return False
        device_row = self._db.get_device_by_key(self._current_device_key) if self._current_device_key else None
        device_tracks = self._device_tracks
        if not device_tracks:
            if not device_row:
                return False
            return bool(dict(device_row).get("last_scan_at"))
        return all(dict(track).get("local_track_id") for track in device_tracks)

    def device_inventory_is_verified(self):
        """Return whether the current device inventory has been scanned into the cache."""
        if not self._ensure_current_device_key():
            return False
        return not self._device_inventory_needs_scan()

    def set_current_device(self, device):
        """Select a connected device and load its cached inventory from SQLite."""
        device_row = self._db.upsert_device(device_record_from_info(device))
        self._db.commit()
        self._current_device_key = device_row["stable_device_key"]
        self.load_cached_device_inventory()
        return self._current_device_key

    def load_cached_device_inventory(self, device_key=None):
        if device_key is not None:
            self._current_device_key = device_key
        self._device_tracks = [
            dict(row) for row in self._db.get_all_device_tracks(self._current_device_key)
        ] if self._current_device_key else []
        self._device_file_state = {
            row.get("device_path", ""): {"track": dict(row), "file_size": row.get("file_size")}
            for row in self._device_tracks
        }
        return list(self._device_tracks)

    def clear_device_index(self, forget_current=False):
        if forget_current:
            self._device_tracks = []
            self._device_file_state = {}
            self._current_device_key = ""
            return

        if self._current_device_key:
            self.load_cached_device_inventory(self._current_device_key)
        else:
            self._device_tracks = []
            self._device_file_state = {}

    def get_not_on_device_tracks(self, track_ids=None):
        """Return local tracks missing from the current device index."""
        if not self._ensure_current_device_key():
            return []
        if self._device_mount_unavailable():
            return []
        if self._device_inventory_needs_scan():
            return []
        device_tracks = self.get_device_tracks()
        if not device_tracks:
            return self._db.get_tracks_not_on_device(self._current_device_key, track_ids=track_ids)
        if all(dict(track).get("local_track_id") for track in device_tracks):
            return self._db.get_tracks_not_on_device(self._current_device_key, track_ids=track_ids)

        if track_ids:
            local_tracks = self._db.get_tracks_by_ids(track_ids)
        else:
            local_tracks = self._db.get_all_tracks()

        matcher = self._matcher()
        _matched, unmatched, _orphaned, _resync = matcher.match_all(
            local_tracks, device_tracks
        )
        return [result.local_track for result in unmatched]

    def get_not_on_device_reason_counts(self, track_ids=None):
        """Return grouped reasons explaining why local tracks are not on the device."""
        if not self._ensure_current_device_key():
            return {}
        if self._device_mount_unavailable():
            return {}
        if self._device_inventory_needs_scan():
            return {}

        missing_tracks = self.get_not_on_device_tracks(track_ids=track_ids)
        if not missing_tracks:
            return {}

        summary = summarize_unmatched_tracks(
            missing_tracks,
            self.get_device_tracks(),
            matcher=self._matcher(),
            sample_limit=0,
        )
        return summary["reason_counts"]

    def get_sync_status_counts(self):
        if not self._ensure_current_device_key():
            return {"missing": 0, "resync": 0}
        if self._device_mount_unavailable():
            return {"missing": 0, "resync": 0}
        if self._device_inventory_needs_scan():
            return {"missing": 0, "resync": 0}
        if self._audio_conversion_enabled():
            plan = self.build_sync_plan()
            return {"missing": plan.copy_count, "resync": plan.resync_count}
        device_tracks = self.get_device_tracks()
        if not device_tracks:
            return self._db.get_sync_status_counts(
                self._current_device_key,
                include_resync=bool(self._config_value("resync_metadata_changes", True)),
            )

        if all(dict(track).get("local_track_id") for track in device_tracks):
            return self._db.get_sync_status_counts(
                self._current_device_key,
                include_resync=bool(self._config_value("resync_metadata_changes", True)),
            )

        missing = len(self.get_not_on_device_tracks())
        resync = 0
        if self._config_value("resync_metadata_changes", True):
            row = self._db.fetchone(
                "SELECT COUNT(*) AS cnt FROM tracks "
                "WHERE synced_to_device = 1 "
                "AND (metadata_hash != last_synced_metadata_hash "
                "OR (last_synced_file_hash != '' AND file_hash != '' "
                "AND file_hash != last_synced_file_hash))"
            )
            resync = row["cnt"] if row else 0
        return {"missing": missing, "resync": resync}

    def _audio_conversion_enabled(self):
        return bool(self._config_value("convert_audio_for_device", False))

    def _audio_conversion_settings(self):
        return {
            "enabled": self._audio_conversion_enabled(),
            "mode": self._config_value("audio_conversion_mode", "unsupported_or_lossless"),
            "target_codec": self._config_value("audio_conversion_codec", "mp3"),
            "target_bitrate_kbps": self._config_value("audio_conversion_bitrate_kbps", 160),
        }

    def _prepare_tracks_for_sync(self, rows):
        settings = self._audio_conversion_settings()
        if not settings["enabled"]:
            return [dict(row) if hasattr(row, "keys") else dict(row) for row in rows], [], 0

        prepared = []
        errors = []
        transcode_count = 0
        for row in rows:
            item = dict(row) if hasattr(row, "keys") else dict(row)
            if str(item.get("media_type") or "audio") != "audio":
                prepared.append(item)
                continue
            try:
                sync_row, info = self._audio_transcoder.prepare_track_for_sync(
                    item,
                    self._current_device_key or "device",
                    settings,
                )
            except RuntimeError as exc:
                title = item.get("title") or os.path.basename(str(item.get("file_path") or "track"))
                message = f"{title}: {exc}"
                errors.append(message)
                logger.warning("Skipping sync for %s", message)
                continue
            if info.get("converted"):
                transcode_count += 1
            prepared.append(sync_row)
        return prepared, errors, transcode_count

    def _unique_device_path(self, rel_path, used_paths, allow_existing=""):
        """Avoid path collisions while preserving the configured folder layout."""
        if rel_path == allow_existing or rel_path not in used_paths:
            return rel_path

        root, ext = os.path.splitext(rel_path)
        counter = 2
        candidate = f"{root} ({counter}){ext}"
        while candidate in used_paths and candidate != allow_existing:
            counter += 1
            candidate = f"{root} ({counter}){ext}"
        return candidate

    def scan_device(self, force_full=False, status_callback=None):
        """Verify the current device inventory and refresh the cached index."""
        device = self._device_detector.current_device
        if not device:
            return 0
        self._ensure_current_device_key()

        summary = verify_device_inventory(
            self._db,
            device,
            self._config_value("duplicate_strictness", "metadata_and_hash"),
            force_full,
            read_metadata,
            compute_file_hash,
            self._config_value("duration_match_tolerance_seconds", 2.0),
            status_callback=status_callback,
        )
        self._current_device_key = summary.get("device_key", self._current_device_key)
        self.load_cached_device_inventory()

        logger.info("Device scan complete: %d tracks found", summary["scanned"])
        return summary["scanned"]

    def _populate_artwork_sync_plan(self, plan, matched, local_tracks):
        if not self._artwork_manager:
            return
        if not self._config_value("copy_artwork_to_device", True) or not self._config.get("export_device_cover_jpg", True):
            return
        device = self._device_detector.current_device
        if not device:
            return

        album_targets = {}

        def add_target(row, device_rel_path):
            rel_dir = os.path.dirname(device_rel_path)
            if not rel_dir:
                return
            info = {
                "group_key": row.get("album_group_key") or f"{row.get('album_artist') or row.get('artist') or 'Unknown Artist'}\0{row.get('album') or 'Unknown Album'}",
                "album": row.get("album", "") or "Unknown Album",
                "artist": row.get("album_artist", "") or row.get("artist", "") or "Unknown Artist",
                "tracks": [],
            }
            target = album_targets.setdefault(info["group_key"], {**info, "device_dirs": set()})
            target["device_dirs"].add(rel_dir)
            target["tracks"].append(row)

        for row, rel_path in plan.to_copy:
            add_target(dict(row), rel_path)
        for row, _old_path, rel_path in plan.to_resync:
            add_target(dict(row), rel_path)
        for result in matched:
            row = dict(result.local_track) if hasattr(result.local_track, "keys") else dict(result.local_track)
            device_row = dict(result.device_track) if hasattr(result.device_track, "keys") else dict(result.device_track)
            rel_path = device_row.get("device_path", "")
            if rel_path:
                add_target(row, rel_path)

        seen = set()
        existing_cover_hashes = {}
        for album_key, album_info in album_targets.items():
            cover_src, cover_hash = self._artwork_manager.export_device_cover(album_info)
            if not cover_src:
                continue
            for rel_dir in sorted(album_info["device_dirs"]):
                cover_rel = os.path.join(rel_dir, "cover.jpg")
                if cover_rel in seen:
                    continue
                seen.add(cover_rel)
                device_cover = os.path.join(device.mount_path, cover_rel)
                if os.path.exists(device_cover):
                    cached_hash = existing_cover_hashes.get(device_cover)
                    if cached_hash is None:
                        try:
                            cached_hash = compute_file_hash(device_cover)
                        except OSError:
                            cached_hash = ""
                        existing_cover_hashes[device_cover] = cached_hash
                    if cached_hash and cached_hash == cover_hash:
                        continue
                plan.artwork_to_copy.append((cover_src, cover_rel, album_key))

    def apply_successful_sync_to_cache(self, plan, synced_at=None):
        """Update cached DB device state from a completed sync plan."""
        device_key = self._current_device_key
        if not device_key:
            return
        if synced_at is None:
            synced_at = time.strftime("%Y-%m-%dT%H:%M:%S")

        for row, rel_path in plan.to_copy:
            self._upsert_synced_device_row(device_key, row, rel_path, synced_at)
        for row, old_rel_path, rel_path in plan.to_resync:
            if old_rel_path and old_rel_path != rel_path:
                self._db.execute(
                    "UPDATE device_tracks SET present_on_device = 0 "
                    "WHERE device_id = ? AND device_path = ?",
                    (device_key, old_rel_path),
                )
            self._upsert_synced_device_row(device_key, row, rel_path, synced_at)
        for rel_path in plan.to_delete:
            self._db.execute(
                "DELETE FROM device_tracks WHERE device_id = ? AND device_path = ?",
                (device_key, rel_path),
            )
        self._db.mark_device_synced(device_key)
        if plan.to_delete:
            self._link_device_to_local()
        self._db.commit()
        self.load_cached_device_inventory(device_key)

    def _link_device_to_local(self):
        """Try to set local_track_id on device tracks by matching."""
        device_tracks = self._db.get_all_device_tracks(self._current_device_key)
        local_tracks = self._db.get_all_tracks()

        matcher = self._matcher()
        matched, unmatched, _, _ = matcher.match_all(local_tracks, device_tracks)

        for result in matched:
            lt = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
            dt = dict(result.device_track) if hasattr(result.device_track, "keys") else result.device_track
            local_id = lt.get("id")
            dev_id = dt.get("id")
            dev_path = dt.get("device_path", "")
            if local_id and dev_id:
                self._db.execute(
                    "UPDATE device_tracks SET local_track_id = ? WHERE id = ?",
                    (local_id, dev_id),
                )
                # Also mark local track as synced
                mh = dt.get("metadata_hash", "")
                fh = dt.get("file_hash", "")
                self._db.mark_synced(local_id, dev_path, mh, fh)

        for result in unmatched:
            lt = dict(result.local_track) if hasattr(result.local_track, "keys") else result.local_track
            local_id = lt.get("id")
            if local_id:
                self._db.execute(
                    "UPDATE tracks SET synced_to_device = 0, device_path = NULL, "
                    "last_synced_metadata_hash = '', last_synced_file_hash = '' "
                    "WHERE id = ?",
                    (local_id,),
                )

        self._db.commit()

    def _upsert_synced_device_row(self, device_key, row, rel_path, synced_at):
        self._db.upsert_device_track(
            {
                "device_id": device_key,
                "device_path": rel_path,
                "local_track_id": row.get("id"),
                "title": _device_cache_title(row),
                "artist": row.get("artist", ""),
                "album": row.get("album", ""),
                "album_artist": row.get("album_artist", ""),
                "genre": row.get("genre", ""),
                "year": row.get("year"),
                "track_number": row.get("track_number"),
                "disc_number": row.get("disc_number", 1),
                "duration": row.get("duration", 0.0),
                "bitrate": row.get("bitrate", 0),
                "codec": row.get("codec", ""),
                "file_size": row.get("file_size", 0),
                "metadata_hash": row.get("metadata_hash", ""),
                "file_hash": row.get("file_hash", ""),
                "present_on_device": 1,
                "last_synced_at": synced_at,
            }
        )

    def execute_sync(self, plan):
        """Execute a sync plan on a background thread.

        Connect to sync_progress, sync_finished, sync_cancelled, and
        sync_error *before* calling this method.
        """
        device = self._device_detector.current_device
        if not device:
            self.sync_error.emit("No device connected")
            return

        if self.is_syncing:
            self.sync_error.emit("Sync already in progress")
            return

        db_path = self._config.db_path

        self._thread = QThread()
        self._worker = SyncWorker(plan, device.mount_path, db_path)
        self._worker.moveToThread(self._thread)

        self._thread.started.connect(self._worker.run)

        # Worker lives in worker thread; SyncEngine lives in main thread.
        # Qt uses queued connections automatically across threads between
        # QObjects, so these slots execute on the main thread.
        self._worker.progress.connect(
            self._on_worker_progress, Qt.QueuedConnection)
        self._worker.file_copied.connect(
            self._on_worker_file_copied, Qt.QueuedConnection)
        self._worker.file_error.connect(
            self._on_worker_file_error, Qt.QueuedConnection)
        self._worker.finished.connect(
            self._on_worker_finished, Qt.QueuedConnection)
        self._worker.cancelled.connect(
            self._on_worker_cancelled, Qt.QueuedConnection)

        self._thread.start()

    def cancel_sync(self):
        if self._worker:
            self._worker.cancel()

    def shutdown(self):
        if self._worker:
            self._worker.cancel()
        if self._thread and self._thread.isRunning():
            self._thread.quit()
            if not self._thread.wait(5000):
                logger.warning("Sync thread did not stop cleanly; terminating")
                self._thread.terminate()
                self._thread.wait(2000)
        self._thread = None
        self._worker = None

    @Slot(int, int, str)
    def _on_worker_progress(self, current, total, desc):
        """Re-emit progress on the main thread."""
        self.sync_progress.emit(current, total, desc)

    @Slot(str, str)
    def _on_worker_file_copied(self, src, dest):
        """Re-emit file_copied on the main thread."""
        self.sync_file_copied.emit(src, dest)

    @Slot(str, str)
    def _on_worker_file_error(self, path, msg):
        """Re-emit file_error on the main thread."""
        self.sync_file_error.emit(path, msg)

    @Slot(int, int, int)
    def _on_worker_finished(self, copied, failed, skipped):
        """Handle sync completion on the main thread."""
        self._device_detector.refresh_device()
        self._cleanup_thread()
        self.sync_finished.emit(copied, failed, skipped)

    @Slot()
    def _on_worker_cancelled(self):
        """Handle sync cancellation on the main thread."""
        self._cleanup_thread()
        self.sync_cancelled.emit()

    def _cleanup_thread(self):
        if self._thread:
            self._thread.quit()
            self._thread.wait(5000)
            self._thread = None
            self._worker = None

    def delete_device_track(self, device_track_row):
        """Delete a single track from the device filesystem and DB."""
        device = self._device_detector.current_device
        if not device:
            return False, "No device connected"

        d = dict(device_track_row) if hasattr(device_track_row, "keys") else device_track_row
        rel = d.get("device_path", "")
        full = os.path.join(device.mount_path, rel)

        if os.path.isfile(full):
            try:
                removed = _remove_audio_with_sidecars(full)
                logger.info("Deleted device file(s): %s", ", ".join(removed) if removed else full)
            except OSError as e:
                return False, str(e)

        did = d.get("id")
        if did:
            self._db.delete_device_track(did)

        # Clear synced status on the local track if linked
        lid = d.get("local_track_id")
        if lid:
            self._db.execute(
                "UPDATE tracks SET synced_to_device = 0, device_path = NULL, "
                "last_synced_metadata_hash = '', last_synced_file_hash = '' "
                "WHERE id = ?",
                (lid,),
            )

        self._db.commit()
        return True, "Deleted"

    def find_duplicate_device_tracks(self):
        """Return duplicate device groups relative to the local library."""
        device_tracks = [dict(row) for row in self.get_device_tracks()]
        local_tracks = [dict(row) for row in self._db.get_all_tracks()]
        return self._find_duplicate_groups(local_tracks, device_tracks)

    def remove_duplicate_device_tracks(self):
        """Delete extra duplicate device tracks while keeping one canonical copy."""
        device = self._device_detector.current_device
        if not device:
            return {"success": False, "message": "No device connected", "duplicate_groups": [], "deleted": [], "failures": []}

        duplicates = self.find_duplicate_device_tracks()
        if not duplicates:
            return {"success": True, "message": "No duplicate device tracks found", "duplicate_groups": [], "deleted": [], "failures": []}

        deleted = []
        failures = []
        for group in duplicates:
            for row in group["remove"]:
                rel = row.get("device_path", "")
                full = os.path.join(device.mount_path, rel)
                try:
                    _remove_audio_with_sidecars(full)
                    did = row.get("id")
                    if did:
                        self._db.delete_device_track(did)
                    deleted.append(rel)
                except OSError as exc:
                    failures.append(f"{rel}: {exc}")

        self._link_device_to_local()
        self._db.commit()
        if self._current_device_key:
            self.load_cached_device_inventory(self._current_device_key)
        return {
            "success": not failures,
            "message": "Duplicate cleanup completed" if not failures else "Duplicate cleanup completed with errors",
            "duplicate_groups": duplicates,
            "deleted": deleted,
            "failures": failures,
        }

    def _find_duplicate_groups(self, local_tracks, device_tracks):
        if not device_tracks:
            return []

        local_group_counts = defaultdict(int)
        for row in local_tracks:
            key = _dedupe_group_key(row)
            if key is not None:
                local_group_counts[key] += 1

        grouped = defaultdict(list)
        for row in device_tracks:
            key = _dedupe_group_key(row)
            if key is not None:
                grouped[key].append(dict(row) if hasattr(row, "keys") else row)

        duplicates = []
        for key, rows in grouped.items():
            expected_count = max(1, local_group_counts.get(key, 0))
            if len(rows) <= expected_count:
                continue
            ordered = sorted(rows, key=self._duplicate_rank, reverse=True)
            keep = ordered[:expected_count]
            remove = ordered[expected_count:]
            duplicates.append(
                {
                    "group_key": key,
                    "expected_count": expected_count,
                    "device_count": len(rows),
                    "keep": keep,
                    "remove": remove,
                }
            )
        duplicates.sort(
            key=lambda item: (
                item["keep"][0].get("artist", ""),
                item["keep"][0].get("album", ""),
                item["keep"][0].get("title", ""),
            )
        )
        return duplicates

    @staticmethod
    def _duplicate_rank(row):
        item = dict(row) if hasattr(row, "keys") else row
        linked = 1 if item.get("local_track_id") else 0
        synced_path = 1 if linked and item.get("device_path") else 0
        file_hash = 1 if item.get("file_hash") else 0
        metadata_hash = 1 if item.get("metadata_hash") else 0
        path = item.get("device_path", "")
        return (
            linked,
            synced_path,
            file_hash,
            metadata_hash,
            -len(path),
            path,
        )


class _PlanningDeviceDetector:
    def __init__(self, device):
        self.current_device = device

    @property
    def is_connected(self):
        return self.current_device is not None


class SyncPlanWorker(QObject):
    finished = Signal(object)
    error = Signal(str)
    status = Signal(str)

    def __init__(self, db_path, config, device, artwork_cache_dir, track_ids=None, force_full=False):
        super().__init__()
        self._db_path = db_path
        self._config = config
        self._device = device
        self._artwork_cache_dir = artwork_cache_dir
        self._track_ids = set(track_ids) if track_ids else None
        self._force_full = force_full

    @Slot()
    def run(self):
        db = None
        artwork = None
        try:
            from services.artwork_manager import ArtworkManager

            db = Database(self._db_path, initialize=False)
            artwork = ArtworkManager(self._artwork_cache_dir, self._config)
            detector = _PlanningDeviceDetector(self._device)
            engine = SyncEngine(db, self._config, detector, artwork)
            with db.write_lock():
                plan = engine.build_sync_plan(
                    track_ids=self._track_ids,
                    force_full=self._force_full,
                    status_callback=self.status.emit,
                )
            self.finished.emit(plan)
        except Exception as exc:
            logger.exception("Sync planning failed")
            self.error.emit(str(exc))
        finally:
            if artwork:
                artwork.shutdown()
            if db:
                db.close()


class SyncPlanBuilder(QObject):
    finished = Signal(object)
    error = Signal(str)
    status = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._thread = None
        self._worker = None

    @property
    def is_running(self):
        return self._thread is not None and self._thread.isRunning()

    def start(self, db_path, config, device, artwork_cache_dir, track_ids=None, force_full=False):
        if self.is_running:
            return False
        self._thread = QThread()
        self._worker = SyncPlanWorker(
            db_path,
            config,
            device,
            artwork_cache_dir,
            track_ids=track_ids,
            force_full=force_full,
        )
        self._worker.moveToThread(self._thread)
        self._thread.started.connect(self._worker.run)
        self._worker.finished.connect(self._on_finished, Qt.QueuedConnection)
        self._worker.error.connect(self._on_error, Qt.QueuedConnection)
        self._worker.status.connect(self.status, Qt.QueuedConnection)
        self._thread.start()
        return True

    @Slot(object)
    def _on_finished(self, plan):
        self._cleanup()
        self.finished.emit(plan)

    @Slot(str)
    def _on_error(self, message):
        self._cleanup()
        self.error.emit(message)

    def _cleanup(self):
        if self._thread:
            self._thread.quit()
            self._thread.wait(3000)
        if self._worker:
            try:
                self._worker.deleteLater()
            except RuntimeError:
                pass
        if self._thread:
            try:
                self._thread.deleteLater()
            except RuntimeError:
                pass
        self._worker = None
        self._thread = None
