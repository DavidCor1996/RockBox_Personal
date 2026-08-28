"""Remembers what each video clip transcoded to, so the RVP cache is disposable.

``cache/device_video_rvp`` is tens of gigabytes of MPEG that ffmpeg can always
rebuild, but it used to be the only record that a clip had been produced:
``_prepare_mpeg_track()`` had to transcode a video just to learn its size and
hash before the plan could compare them against the copy already on the iPod.
Deleting the cache to reclaim disk therefore cost a full re-transcode of
material that had not changed and was already on the device.

Persisting those few numbers here -- in SQLite, deliberately outside the cache
directory -- lets the comparison survive the cache being cleared. Losing this
file is never fatal: it only costs the transcode it exists to avoid.

The key is the cache file's own basename. ``_cache_mpeg_path()`` already hashes
the source path, mtime, size, the track's file_hash, the device key and the
encode profile into the 16-hex digest embedded in that name, so the basename is
the fingerprint -- no second hashing scheme to keep in step with the first.
"""

import logging
import os
import sqlite3
import threading
from datetime import datetime

logger = logging.getLogger(__name__)

VIDEO_CLIP_DB_NAME = "video_clips.db"


class VideoClipManifest:
    """Size/hash/metadata of transcoded video clips, keyed by cache basename."""

    def __init__(self, path: str):
        self._path = path
        self._conn = None
        self._failed = False
        # Sync planning runs on a worker thread while the engine that owns
        # this manifest is built on the UI thread, so the connection is
        # opened with check_same_thread=False and every use is serialised.
        self._lock = threading.RLock()

    def _connection(self):
        if self._conn is not None:
            return self._conn
        if self._failed:
            return None
        try:
            parent = os.path.dirname(self._path)
            if parent:
                os.makedirs(parent, exist_ok=True)
            conn = sqlite3.connect(self._path, timeout=5.0,
                                   check_same_thread=False)
            conn.row_factory = sqlite3.Row
            conn.execute("PRAGMA busy_timeout = 5000")
            try:
                conn.execute("PRAGMA journal_mode=WAL")
            except sqlite3.DatabaseError:
                pass
            conn.execute("PRAGMA synchronous=NORMAL")
            conn.execute(
                "CREATE TABLE IF NOT EXISTS video_clips ("
                "  device_key TEXT NOT NULL,"
                "  output_name TEXT NOT NULL,"
                "  file_hash TEXT DEFAULT '',"
                "  file_size INTEGER NOT NULL,"
                "  duration REAL DEFAULT 0,"
                "  bitrate INTEGER DEFAULT 0,"
                "  codec TEXT DEFAULT '',"
                "  source_path TEXT DEFAULT '',"
                "  updated_at TEXT DEFAULT '',"
                "  PRIMARY KEY (device_key, output_name)"
                ")"
            )
            conn.commit()
        except (sqlite3.DatabaseError, OSError) as error:
            logger.warning("Video clip manifest unavailable (%s); clips will "
                           "be re-transcoded as before", error)
            self._failed = True
            self._conn = None
            return None
        self._conn = conn
        return conn

    def lookup(self, device_key: str, output_name: str):
        """Recorded details for a clip, or None if never recorded."""
        with self._lock:
            conn = self._connection()
            if conn is None:
                return None
            try:
                row = conn.execute(
                    "SELECT file_hash, file_size, duration, bitrate, codec "
                    "FROM video_clips WHERE device_key = ? AND output_name = ?",
                    (str(device_key or ""), str(output_name or "")),
                ).fetchone()
            except sqlite3.DatabaseError:
                return None
            if row is None:
                return None
            try:
                size = int(row["file_size"] or 0)
            except (TypeError, ValueError):
                return None
            if size <= 0:
                return None
            return {
                "file_hash": str(row["file_hash"] or ""),
                "file_size": size,
                "duration": float(row["duration"] or 0.0),
                "bitrate": int(row["bitrate"] or 0),
                "codec": str(row["codec"] or ""),
            }

    def record(self, device_key: str, output_name: str, file_hash: str,
               file_size: int, duration=0.0, bitrate=0, codec="",
               source_path="") -> None:
        with self._lock:
            conn = self._connection()
            if conn is None:
                return
            try:
                conn.execute(
                    "INSERT INTO video_clips (device_key, output_name, "
                    "file_hash, file_size, duration, bitrate, codec, "
                    "source_path, updated_at) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
                    "ON CONFLICT(device_key, output_name) DO UPDATE SET "
                    "file_hash = excluded.file_hash, "
                    "file_size = excluded.file_size, "
                    "duration = excluded.duration, "
                    "bitrate = excluded.bitrate, "
                    "codec = excluded.codec, "
                    "source_path = excluded.source_path, "
                    "updated_at = excluded.updated_at",
                    (str(device_key or ""), str(output_name or ""),
                     str(file_hash or ""), int(file_size or 0),
                     float(duration or 0.0), int(bitrate or 0),
                     str(codec or ""), str(source_path or ""),
                     datetime.now().isoformat(timespec="seconds")),
                )
                conn.commit()
            except (sqlite3.DatabaseError, TypeError, ValueError) as error:
                logger.warning("Could not record video clip %s: %s",
                               output_name, error)

    def count(self, device_key=None) -> int:
        with self._lock:
            conn = self._connection()
            if conn is None:
                return 0
            try:
                if device_key is None:
                    row = conn.execute(
                        "SELECT COUNT(*) AS n FROM video_clips").fetchone()
                else:
                    row = conn.execute(
                        "SELECT COUNT(*) AS n FROM video_clips "
                        "WHERE device_key = ?", (str(device_key),)).fetchone()
            except sqlite3.DatabaseError:
                return 0
            return int(row["n"] or 0) if row else 0

    def close(self) -> None:
        with self._lock:
            if self._conn is not None:
                try:
                    self._conn.close()
                except sqlite3.DatabaseError:
                    pass
                self._conn = None


class NoVideoClipManifest:
    """Stand-in when no manifest is available: every clip looks unrecorded."""

    def lookup(self, device_key, output_name):
        return None

    def record(self, *args, **kwargs):
        return None

    def count(self, device_key=None):
        return 0

    def close(self):
        return None
