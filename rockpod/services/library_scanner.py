"""Library scanner - walks local media folders and populates the database."""

import logging
import os
import re
import time
from collections import defaultdict
from pathlib import Path

from PySide6.QtCore import QObject, Signal, QThread, Slot, Qt

from app.config import SUPPORTED_FORMATS, SUPPORTED_VIDEO_FORMATS
from app.database import Database
from services.metadata_reader import read_metadata, read_metadata_details, compute_file_hash

logger = logging.getLogger(__name__)
_ORIGINAL_READ_METADATA = read_metadata
_STALE_VIDEO_RELEASE_NOISE_RE = re.compile(
    r"(?i)\b(?:2160p|1080p|720p|480p|4k|x264|x265|h264|h265|webrip|web(?:[ ._-]?dl)?|"
    r"bluray|brrip|dvdrip|hdr10\+?|hdr|sdr|atvp|ddp\d(?:\.\d)?|ac-?3|e-?ac-?3|"
    r"10bit|8bit|yts(?:[ ._-]?am)?|galaxyrg\d*|pophd|nahom|vostf|vf2|multi)\b"
)


def _normalize_video_dirs(video_dirs=None, video_dir=None):
    raw = video_dirs if video_dirs is not None else video_dir
    if raw is None:
        return []
    if isinstance(raw, (list, tuple)):
        items = raw
    else:
        text = str(raw or "")
        items = text.replace(";", "\n").splitlines()
    normalized = []
    seen = set()
    for item in items:
        path = str(item or "").strip()
        if not path:
            continue
        absolute = os.path.abspath(path)
        if absolute in seen:
            continue
        seen.add(absolute)
        normalized.append(absolute)
    return normalized


def _empty_scan_report(music_dir):
    return {
        "music_dir": music_dir,
        "video_dir": "",
        "video_dirs": [],
        "status": "idle",
        "failure_reason": "",
        "elapsed_seconds": 0.0,
        "total_files_seen": 0,
        "total_audio_files_found": 0,
        "total_video_files_found": 0,
        "total_media_files_found": 0,
        "total_files_successfully_parsed": 0,
        "total_files_inserted": 0,
        "total_files_updated": 0,
        "total_files_removed": 0,
        "total_files_skipped": 0,
        "total_non_audio_files_ignored": 0,
        "total_non_video_files_ignored": 0,
        "total_non_media_files_ignored": 0,
        "changed_count": 0,
        "scan_roots": [],
        "scanned_folders": [],
        "partial_import_folders": [],
        "all_skipped_folders": [],
        "skipped_files": [],
        "warnings": [],
        "folder_stats": [],
    }


def _record_issue(target, path, reason, limit=500):
    if len(target) < limit:
        target.append({"path": path, "reason": reason})


def _count_supported_skip_issues(items):
    return sum(1 for item in items if item.get("reason") != "unsupported extension")


def _folder_for(path):
    return os.path.dirname(path)


def _init_folder_stats():
    return {
        "files_seen": 0,
        "audio_files": 0,
        "video_files": 0,
        "supported_audio_files": 0,
        "supported_video_files": 0,
        "supported_media_files": 0,
        "imported": 0,
        "skipped": 0,
        "warnings": 0,
    }


def _finalize_folder_stats(report, folder_stats):
    folders = []
    partial = []
    all_skipped = []
    for folder in sorted(folder_stats):
        stats = dict(folder_stats[folder])
        if stats["files_seen"] == 0:
            continue
        row = {"folder": folder, **stats}
        folders.append(row)
        supported_media_files = stats.get("supported_media_files", stats["supported_audio_files"])
        if supported_media_files > 0 and 0 < stats["imported"] < supported_media_files:
            partial.append(folder)
        if supported_media_files > 0 and stats["imported"] == 0:
            all_skipped.append(folder)
    report["folder_stats"] = folders
    report["scanned_folders"] = [item["folder"] for item in folders]
    report["partial_import_folders"] = partial
    report["all_skipped_folders"] = all_skipped


def _classify_media_type(path):
    ext = Path(path).suffix.lower()
    if ext in SUPPORTED_FORMATS:
        return "audio"
    if ext in SUPPORTED_VIDEO_FORMATS:
        return "video"
    return ""


def collect_library_files(music_dir, video_dir=None, video_dirs=None):
    """Walk configured library roots and classify files for diagnostics."""
    report = _empty_scan_report(music_dir)
    normalized_video_dirs = _normalize_video_dirs(video_dirs=video_dirs, video_dir=video_dir)
    report["video_dirs"] = list(normalized_video_dirs)
    report["video_dir"] = normalized_video_dirs[0] if normalized_video_dirs else ""
    folder_stats = defaultdict(_init_folder_stats)
    supported = []
    seen_roots = set()
    seen_files = set()

    roots = [("audio", str(music_dir or "").strip(), "music directory missing")]
    roots.extend(("video", path, "video directory missing") for path in normalized_video_dirs)

    for root_type, base_dir, missing_reason in roots:
        if not base_dir:
            continue
        normalized = os.path.abspath(base_dir)
        if normalized in seen_roots:
            continue
        seen_roots.add(normalized)
        report["scan_roots"].append({"media_type": root_type, "path": base_dir})
        if not os.path.isdir(base_dir):
            if root_type == "audio":
                logger.error("Music directory does not exist: %s", base_dir)
            else:
                logger.warning("Video directory does not exist: %s", base_dir)
            _record_issue(report["skipped_files"], base_dir, missing_reason)
            continue

        for root, dirs, filenames in os.walk(base_dir, followlinks=True):
            dirs[:] = [d for d in dirs if not d.startswith(".")]
            for fn in sorted(filenames):
                if fn.startswith("."):
                    continue
                full = os.path.join(root, fn)
                real_full = os.path.realpath(full)
                if real_full in seen_files:
                    continue
                seen_files.add(real_full)
                media_type = _classify_media_type(full)
                stats = folder_stats[root]
                stats["files_seen"] += 1
                report["total_files_seen"] += 1
                if media_type == "audio":
                    supported.append(full)
                    stats["audio_files"] += 1
                    stats["supported_audio_files"] += 1
                    stats["supported_media_files"] += 1
                    report["total_audio_files_found"] += 1
                    report["total_media_files_found"] += 1
                elif media_type == "video":
                    supported.append(full)
                    stats["video_files"] += 1
                    stats["supported_video_files"] += 1
                    stats["supported_media_files"] += 1
                    report["total_video_files_found"] += 1
                    report["total_media_files_found"] += 1
                else:
                    stats["skipped"] += 1
                    if root_type == "audio":
                        report["total_non_audio_files_ignored"] += 1
                    else:
                        report["total_non_video_files_ignored"] += 1
                    report["total_non_media_files_ignored"] += 1
                    _record_issue(report["skipped_files"], full, "unsupported extension")

    report["total_files_skipped"] = _count_supported_skip_issues(report["skipped_files"])
    _finalize_folder_stats(report, folder_stats)
    logger.info(
        "Found %d supported media files in audio=%s video=%s",
        len(supported),
        music_dir,
        normalized_video_dirs,
    )
    return supported, report


def build_library_reconciliation_report(db, music_dir, video_dir=None, video_dirs=None, sample_limit=25):
    """Compare supported files on disk to the current DB library view."""
    disk_files, scan_report = collect_library_files(music_dir, video_dir=video_dir, video_dirs=video_dirs)
    db_rows = db.get_all_tracks(order_by="file_path", media_type=None)
    db_paths = {row["file_path"] for row in db_rows}
    disk_set = set(disk_files)
    missing_from_db = sorted(disk_set - db_paths)
    stale_in_db = sorted(db_paths - disk_set)

    folder_rollup = defaultdict(lambda: {"disk": 0, "db": 0, "missing": 0})
    for path in disk_files:
        folder_rollup[_folder_for(path)]["disk"] += 1
    for path in db_paths:
        folder_rollup[_folder_for(path)]["db"] += 1
    for path in missing_from_db:
        folder_rollup[_folder_for(path)]["missing"] += 1

    partial = []
    all_missing = []
    for folder, stats in sorted(folder_rollup.items()):
        if stats["missing"] and stats["db"]:
            partial.append({"folder": folder, **stats})
        elif stats["missing"] and not stats["db"]:
            all_missing.append({"folder": folder, **stats})

    return {
        "disk_media_file_count": len(disk_files),
        "disk_audio_file_count": scan_report.get("total_audio_files_found", 0),
        "db_track_count": len(db_rows),
        "files_missing_from_db_count": len(missing_from_db),
        "files_missing_from_db": missing_from_db[:sample_limit],
        "stale_db_paths_count": len(stale_in_db),
        "stale_db_paths": stale_in_db[:sample_limit],
        "non_audio_files_ignored_count": scan_report["total_non_audio_files_ignored"],
        "non_video_files_ignored_count": scan_report.get("total_non_video_files_ignored", 0),
        "video_files_on_disk_count": scan_report.get("total_video_files_found", 0),
        "skipped_files": list(scan_report["skipped_files"])[:sample_limit],
        "partial_import_folders": partial[:sample_limit],
        "all_missing_folders": all_missing[:sample_limit],
        "scanned_folders": scan_report["scanned_folders"][:sample_limit],
    }


def format_library_reconciliation_report(report):
    lines = [
        "RockPod library reconciliation report",
        f"Media files on disk: {report.get('disk_media_file_count', report['disk_audio_file_count'])}",
        f"Audio files on disk: {report['disk_audio_file_count']}",
        f"Video files on disk: {report.get('video_files_on_disk_count', 0)}",
        f"Tracks in DB: {report['db_track_count']}",
        f"Files missing from DB: {report['files_missing_from_db_count']}",
        f"Stale DB paths: {report['stale_db_paths_count']}",
        f"Non-audio files ignored: {report.get('non_audio_files_ignored_count', 0)}",
        f"Non-video files ignored: {report.get('non_video_files_ignored_count', 0)}",
    ]
    if report.get("files_missing_from_db"):
        lines.append("")
        lines.append("Files missing from DB:")
        for path in report["files_missing_from_db"]:
            lines.append(f"- {path}")
    if report.get("skipped_files"):
        lines.append("")
        lines.append("Skipped files:")
        for item in report["skipped_files"]:
            lines.append(f"- {item['path']}: {item['reason']}")
    if report.get("partial_import_folders"):
        lines.append("")
        lines.append("Partial import folders:")
        for item in report["partial_import_folders"]:
            lines.append(
                f"- {item['folder']}: disk={item['disk']} db={item['db']} missing={item['missing']}"
            )
    if report.get("all_missing_folders"):
        lines.append("")
        lines.append("Folders fully missing from DB:")
        for item in report["all_missing_folders"]:
            lines.append(
                f"- {item['folder']}: disk={item['disk']} missing={item['missing']}"
            )
    return "\n".join(lines)


def _read_track_with_diagnostics(filepath):
    if read_metadata is not _ORIGINAL_READ_METADATA:
        track = read_metadata(filepath)
        return track, {"parsed_ok": True, "warnings": []}
    return read_metadata_details(filepath)


class LibraryScanWorker(QObject):
    """Background worker that scans a music directory tree.

    Creates its own SQLite connection so all DB writes happen in the
    worker thread — SQLite connections cannot be shared across threads.
    """

    progress = Signal(int, int, str)   # current, total, current_file
    track_found = Signal(dict)         # emitted per track dict (for UI updates)
    finished = Signal(dict)            # detailed report
    error = Signal(str)

    def __init__(self, music_dir, video_dir, db_path, compute_hashes=False, force_full=False):
        super().__init__()
        self._music_dir = music_dir
        self._video_dirs = _normalize_video_dirs(video_dirs=video_dir)
        self._db_path = db_path
        self._compute_hashes = compute_hashes
        self._force_full = force_full
        self._cancelled = False

    def cancel(self):
        self._cancelled = True

    def run(self):
        t0 = time.time()
        db = None
        report = _empty_scan_report(self._music_dir)
        report["video_dirs"] = list(self._video_dirs)
        report["video_dir"] = self._video_dirs[0] if self._video_dirs else ""
        try:
            db = Database(self._db_path, initialize=False)
            files, report = collect_library_files(self._music_dir, video_dirs=self._video_dirs)
            report["status"] = "running"
            cached = db.get_track_file_states()
            file_set = set(files)
            removed = sorted(set(cached) - file_set)
            to_read = self._files_requiring_metadata(files, cached)
            to_read_set = set(to_read)
            folder_stats = {
                item["folder"]: {
                    "files_seen": item["files_seen"],
                    "audio_files": item["audio_files"],
                    "supported_audio_files": item["supported_audio_files"],
                    "imported": item["imported"],
                    "skipped": item["skipped"],
                    "warnings": item["warnings"],
                }
                for item in report["folder_stats"]
            }
            for filepath in files:
                if filepath not in to_read_set and filepath in cached:
                    folder_stats.setdefault(_folder_for(filepath), _init_folder_stats())["imported"] += 1

            if report["total_media_files_found"] == 0:
                if removed:
                    with db.transaction():
                        db.delete_tracks_by_paths(removed)
                report["total_files_removed"] = len(removed)
                report["changed_count"] = len(removed)
                report["elapsed_seconds"] = time.time() - t0
                report["status"] = "ok"
                _finalize_folder_stats(report, defaultdict(_init_folder_stats, folder_stats))
                self.finished.emit(report)
                return

            read_total = len(to_read)
            upsert_batch = []
            for i, filepath in enumerate(to_read):
                if self._cancelled:
                    logger.info("Library scan cancelled at %d/%d", i, read_total)
                    break

                self.progress.emit(i + 1, read_total, os.path.basename(filepath))
                folder = _folder_for(filepath)
                stats = folder_stats.setdefault(folder, _init_folder_stats())

                try:
                    track, info = _read_track_with_diagnostics(filepath)
                    track.media_type = _classify_media_type(filepath) or "audio"
                    if self._compute_hashes:
                        track.file_hash = compute_file_hash(filepath)
                    else:
                        track.file_hash = ""
                    track_data = track.to_dict()
                    if not track_data.get("file_path"):
                        stats["skipped"] += 1
                        _record_issue(report["skipped_files"], filepath, "missing required fields")
                        continue
                    upsert_batch.append((track_data, cached.get(filepath) is None))
                    if info.get("parsed_ok"):
                        report["total_files_successfully_parsed"] += 1
                    for warning in info.get("warnings", []):
                        stats["warnings"] += 1
                        _record_issue(report["warnings"], filepath, warning)
                except Exception as e:
                    stats["skipped"] += 1
                    logger.warning("Skipping %s: %s", filepath, e)
                    _record_issue(report["skipped_files"], filepath, f"unexpected exception: {e}")

            with db.transaction():
                if removed:
                    db.delete_tracks_by_paths(removed)
                for track_data, is_new in upsert_batch:
                    folder = _folder_for(track_data.get("file_path", ""))
                    stats = folder_stats.setdefault(folder, _init_folder_stats())
                    try:
                        db.upsert_track(track_data)
                        stats["imported"] += 1
                        if is_new:
                            report["total_files_inserted"] += 1
                        else:
                            report["total_files_updated"] += 1
                        self.track_found.emit(track_data)
                    except Exception as e:
                        stats["skipped"] += 1
                        logger.warning("DB write failed for %s: %s", track_data.get("file_path", ""), e)
                        _record_issue(
                            report["skipped_files"],
                            track_data.get("file_path", ""),
                            f"DB write failure: {e}",
                        )
            elapsed = time.time() - t0
            report["total_files_removed"] = len(removed)
            report["total_files_skipped"] = _count_supported_skip_issues(report["skipped_files"])
            report["changed_count"] = (
                report["total_files_inserted"]
                + report["total_files_updated"]
                + report["total_files_removed"]
            )
            report["elapsed_seconds"] = elapsed
            report["status"] = "ok"
            _finalize_folder_stats(report, defaultdict(_init_folder_stats, folder_stats))
            logger.info(
                "Library refresh complete: scanned=%d imported=%d skipped=%d removed=%d in %.1fs",
                report["total_media_files_found"],
                report["total_files_inserted"] + report["total_files_updated"],
                report["total_files_skipped"],
                report["total_files_removed"],
                elapsed,
            )
            self.finished.emit(report)

        except Exception as e:
            logger.exception("Library scan failed")
            report["status"] = "failed"
            report["failure_reason"] = str(e)
            report["elapsed_seconds"] = time.time() - t0
            self.error.emit(str(e))
        finally:
            if db:
                db.close()

    def _files_requiring_metadata(self, files, cached):
        """Return files that are new, changed, or part of a forced full scan."""
        if self._force_full:
            return files

        to_read = []
        for filepath in files:
            row = cached.get(filepath)
            if row is None:
                to_read.append(filepath)
                continue

            classified_media_type = _classify_media_type(filepath)
            if classified_media_type and row.get("media_type") != classified_media_type:
                to_read.append(filepath)
                continue

            if row.get("media_type") == "video":
                if not row.get("video_kind"):
                    to_read.append(filepath)
                    continue
                if row.get("show_title") == "" and (
                    row.get("season_number") or row.get("episode_number") or row.get("track_number")
                ):
                    to_read.append(filepath)
                    continue
                if row.get("show_title") == "" and re.search(r"(?i)s\d{1,2}[ ._-]*e\d{1,3}", str(row.get("title") or "")):
                    to_read.append(filepath)
                    continue
                show_title = str(row.get("show_title") or "").strip().casefold()
                if row.get("video_kind") == "show" and show_title in {"special", "specials"}:
                    to_read.append(filepath)
                    continue
                if row.get("video_kind") == "show" and any(token in show_title for token in ("complete series", "full series")):
                    to_read.append(filepath)
                    continue
                title_text = str(row.get("title") or "").strip().casefold()
                artist_text = str(row.get("artist") or "").strip().casefold()
                album_text = str(row.get("album") or "").strip().casefold()
                album_artist_text = str(row.get("album_artist") or "").strip().casefold()
                if any(
                    marker in value
                    for value in (title_text, show_title, artist_text, album_text, album_artist_text)
                    for marker in ("archive.org/details/", "quicktime", "full-series", "complete series")
                ):
                    to_read.append(filepath)
                    continue
                if any(
                    re.search(r"(?i)(?:[\[(](19\d{2}|20\d{2})(?:\s*[-/]\s*(19\d{2}|20\d{2}))?[\])]|[A-Za-z].*\s+(19\d{2}|20\d{2}))\s*$", value)
                    for value in (title_text, show_title, artist_text, album_text, album_artist_text)
                    if value
                ):
                    to_read.append(filepath)
                    continue
                if any(
                    _STALE_VIDEO_RELEASE_NOISE_RE.search(value)
                    for value in (title_text, show_title, artist_text, album_text, album_artist_text)
                    if value
                ):
                    to_read.append(filepath)
                    continue
                if any(
                    re.search(r"\[[a-z0-9_-]{6,16}\]\s*$", value)
                    for value in (title_text, show_title, artist_text, album_text, album_artist_text)
                    if value
                ):
                    to_read.append(filepath)
                    continue
                stem_text = os.path.splitext(os.path.basename(filepath))[0]
                if (
                    row.get("video_kind") == "show"
                    and not row.get("episode_number")
                    and re.search(r"(?i)s\d{1,2}[ ._-]*e\d{1,3}", stem_text)
                ):
                    to_read.append(filepath)
                    continue
                if album_text in {"archive", "season 20"} or artist_text == "archive":
                    to_read.append(filepath)
                    continue
                path_text = str(filepath).replace(os.sep, " ").casefold()
                if row.get("video_kind") == "movie" and not row.get("show_title"):
                    if any(
                        marker in path_text
                        for marker in (" tv shows ", " shows ", " series ", " anime ", " specials ", " special ")
                    ):
                        to_read.append(filepath)
                        continue

            try:
                stat = os.stat(filepath)
            except OSError:
                continue

            cached_mtime = row.get("last_modified") or 0.0
            cached_size = row.get("file_size") or 0
            if stat.st_mtime != cached_mtime or stat.st_size != cached_size:
                to_read.append(filepath)

        return to_read


class LibraryScanner(QObject):
    """High-level scanner that manages the scan worker on a background thread.

    This is a QObject so that signals from the worker thread are delivered
    to slots on this object via Qt's queued connection mechanism, ensuring
    all callbacks run on the main thread where the UI and main-thread DB
    connection live.
    """

    # Re-emitted on the main thread for UI consumers
    scan_progress = Signal(int, int, str)   # current, total, current_file
    scan_track_found = Signal(dict)         # per-track dict
    scan_finished = Signal(int, float)      # total_tracks, elapsed_seconds
    scan_report = Signal(dict)
    scan_error = Signal(str)

    def __init__(self, db, config, parent=None):
        super().__init__(parent)
        self._db = db
        self._config = config
        self._thread = None
        self._worker = None
        self._last_report = _empty_scan_report(self._config.music_dir)
        self._last_report["video_dirs"] = list(getattr(self._config, "video_dirs", []))
        self._last_report["video_dir"] = self._config.video_dir

    @property
    def last_report(self):
        return dict(self._last_report)

    @property
    def is_scanning(self):
        return self._thread is not None and self._thread.isRunning()

    def start_scan(self, force_full=False):
        """Start an asynchronous background library refresh.

        Connect to scan_progress, scan_track_found, scan_finished, and
        scan_error *before* calling this method.
        """
        if self.is_scanning:
            logger.warning("Scan already in progress")
            return

        music_dir = self._config.music_dir
        video_dir = list(getattr(self._config, "video_dirs", []))
        db_path = self._config.db_path
        compute_hashes = self._config.duplicate_strictness in (
            "metadata_and_hash", "file_hash_only"
        )

        self._thread = QThread()
        self._worker = LibraryScanWorker(music_dir, video_dir, db_path, compute_hashes, force_full)
        self._worker.moveToThread(self._thread)

        self._thread.started.connect(self._worker.run)

        # Worker lives in worker thread; LibraryScanner lives in main thread.
        # Qt uses queued connections automatically across threads between
        # QObjects, so these slots execute on the main thread.
        self._worker.progress.connect(
            self._on_worker_progress, Qt.QueuedConnection)
        self._worker.track_found.connect(
            self._on_worker_track_found, Qt.QueuedConnection)
        self._worker.finished.connect(
            self._on_worker_finished, Qt.QueuedConnection)
        self._worker.error.connect(
            self._on_worker_error, Qt.QueuedConnection)

        self._thread.start()

    def cancel_scan(self):
        if self._worker:
            self._worker.cancel()

    def shutdown(self):
        """Stop the worker thread before application exit."""
        if self._worker:
            self._worker.cancel()
        if self._thread and self._thread.isRunning():
            self._thread.quit()
            if not self._thread.wait(5000):
                logger.warning("Library scanner thread did not stop cleanly; terminating")
                self._thread.terminate()
                self._thread.wait(2000)
        self._thread = None
        self._worker = None

    @Slot(int, int, str)
    def _on_worker_progress(self, current, total, filename):
        """Re-emit progress on the main thread."""
        self.scan_progress.emit(current, total, filename)

    @Slot(dict)
    def _on_worker_track_found(self, track_data):
        """Re-emit track_found on the main thread."""
        self.scan_track_found.emit(track_data)

    @Slot(dict)
    def _on_worker_finished(self, report):
        """Handle scan completion on the main thread.

        The worker thread wrote all data via its own DB connection.
        The main-thread DB will see it because SQLite WAL mode allows
        concurrent readers. We just need to re-query.
        """
        self._last_report = dict(report)
        self._cleanup_thread()
        self.scan_report.emit(dict(report))
        self.scan_finished.emit(report.get("changed_count", 0), report.get("elapsed_seconds", 0.0))

    @Slot(str)
    def _on_worker_error(self, msg):
        """Handle scan error on the main thread."""
        self._cleanup_thread()
        self.scan_error.emit(msg)

    def _cleanup_thread(self):
        if self._thread:
            self._thread.quit()
            self._thread.wait(5000)
            self._thread = None
            self._worker = None

    def scan_sync(self, force_full=False):
        """Synchronous refresh for testing / CLI use. Returns changed count."""
        compute_hashes = self._config.duplicate_strictness in (
            "metadata_and_hash", "file_hash_only"
        )
        worker = LibraryScanWorker(
            self._config.music_dir,
            list(getattr(self._config, "video_dirs", [])),
            self._config.db_path,
            compute_hashes,
            force_full,
        )
        files, report = collect_library_files(
            self._config.music_dir,
            video_dirs=list(getattr(self._config, "video_dirs", [])),
        )
        cached = self._db.get_track_file_states()
        file_set = set(files)
        removed = sorted(set(cached) - file_set)
        to_read = worker._files_requiring_metadata(files, cached)
        to_read_set = set(to_read)
        folder_stats = {
            item["folder"]: {
                "files_seen": item["files_seen"],
                "audio_files": item["audio_files"],
                "supported_audio_files": item["supported_audio_files"],
                "imported": item["imported"],
                "skipped": item["skipped"],
                "warnings": item["warnings"],
            }
            for item in report["folder_stats"]
        }
        for filepath in files:
            if filepath not in to_read_set and filepath in cached:
                folder_stats.setdefault(_folder_for(filepath), _init_folder_stats())["imported"] += 1

        report["status"] = "running"
        try:
            with self._db.transaction():
                if removed:
                    self._db.delete_tracks_by_paths(removed)

                for filepath in to_read:
                    folder = _folder_for(filepath)
                    stats = folder_stats.setdefault(folder, _init_folder_stats())
                    try:
                        track, info = _read_track_with_diagnostics(filepath)
                        track.media_type = _classify_media_type(filepath) or "audio"
                        if compute_hashes:
                            track.file_hash = compute_file_hash(filepath)
                        else:
                            track.file_hash = ""
                        track_data = track.to_dict()
                        if not track_data.get("file_path"):
                            stats["skipped"] += 1
                            _record_issue(report["skipped_files"], filepath, "missing required fields")
                            continue
                        self._db.upsert_track(track_data)
                        stats["imported"] += 1
                        if cached.get(filepath) is None:
                            report["total_files_inserted"] += 1
                        else:
                            report["total_files_updated"] += 1
                        if info.get("parsed_ok"):
                            report["total_files_successfully_parsed"] += 1
                        for warning in info.get("warnings", []):
                            stats["warnings"] += 1
                            _record_issue(report["warnings"], filepath, warning)
                    except Exception as e:
                        stats["skipped"] += 1
                        logger.warning("Skipping %s: %s", filepath, e)
                        _record_issue(report["skipped_files"], filepath, f"unexpected exception: {e}")

                if force_full:
                    self._db.delete_tracks_not_in(file_set)
        except Exception as e:
            report["status"] = "failed"
            report["failure_reason"] = str(e)
            self._last_report = report
            raise

        report["total_files_removed"] = len(removed)
        report["total_files_skipped"] = _count_supported_skip_issues(report["skipped_files"])
        report["changed_count"] = (
            report["total_files_inserted"]
            + report["total_files_updated"]
            + report["total_files_removed"]
        )
        report["status"] = "ok"
        _finalize_folder_stats(report, defaultdict(_init_folder_stats, folder_stats))
        self._last_report = report
        return report["changed_count"]
