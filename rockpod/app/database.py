"""SQLite database layer for the RockPod library."""

import sqlite3
import os
import logging
import threading
from contextlib import contextmanager

from models.track import compute_metadata_hash

logger = logging.getLogger(__name__)

SCHEMA_VERSION = 13

SCHEMA_SQL = """
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY
);

CREATE TABLE IF NOT EXISTS tracks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    media_type TEXT DEFAULT 'audio',
    video_kind TEXT DEFAULT '',
    file_path TEXT UNIQUE NOT NULL,
    file_hash TEXT,
    file_size INTEGER,
    last_modified REAL,
    title TEXT DEFAULT '',
    artist TEXT DEFAULT '',
    album TEXT DEFAULT '',
    album_artist TEXT DEFAULT '',
    show_title TEXT DEFAULT '',
    genre TEXT DEFAULT '',
    year INTEGER,
    season_number INTEGER,
    episode_number INTEGER,
    track_number INTEGER,
    track_total INTEGER,
    disc_number INTEGER DEFAULT 1,
    disc_total INTEGER,
    duration REAL DEFAULT 0.0,
    bitrate INTEGER DEFAULT 0,
    sample_rate INTEGER DEFAULT 0,
    channels INTEGER DEFAULT 2,
    codec TEXT DEFAULT '',
    composer TEXT DEFAULT '',
    comment TEXT DEFAULT '',
    compilation INTEGER DEFAULT 0,
    rating INTEGER DEFAULT 0,
    play_count INTEGER DEFAULT 0,
    last_played TEXT,
    artwork_path TEXT,
    has_embedded_artwork INTEGER DEFAULT 0,
    date_added TEXT DEFAULT (datetime('now')),
    synced_to_device INTEGER DEFAULT 0,
    device_path TEXT,
    metadata_hash TEXT DEFAULT '',
    artwork_hash TEXT DEFAULT '',
    last_synced_metadata_hash TEXT DEFAULT '',
    last_synced_file_hash TEXT DEFAULT '',
    imdb_id TEXT DEFAULT '',
    tmdb_id TEXT DEFAULT '',
    metadata_source TEXT DEFAULT '',
    metadata_confidence REAL DEFAULT 0.0,
    metadata_locked INTEGER DEFAULT 0,
    video_hidden INTEGER DEFAULT 0,
    video_locked INTEGER DEFAULT 0,
    plot_short TEXT DEFAULT '',
    plot_long TEXT DEFAULT '',
    content_rating TEXT DEFAULT '',
    show_plot TEXT DEFAULT ''
);

CREATE TABLE IF NOT EXISTS playlists (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    is_smart INTEGER DEFAULT 0,
    rules_json TEXT,
    sort_order TEXT DEFAULT 'manual',
    sync_to_rockbox INTEGER DEFAULT 1,
    date_created TEXT DEFAULT (datetime('now')),
    date_modified TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS playlist_tracks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    playlist_id INTEGER NOT NULL,
    track_id INTEGER NOT NULL,
    position INTEGER NOT NULL,
    date_added TEXT DEFAULT (datetime('now')),
    FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
    FOREIGN KEY (track_id) REFERENCES tracks(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    stable_device_key TEXT UNIQUE NOT NULL,
    display_name TEXT DEFAULT '',
    mount_path_last_seen TEXT DEFAULT '',
    rockbox_detected INTEGER DEFAULT 0,
    serial_or_signature TEXT DEFAULT '',
    capacity_bytes INTEGER DEFAULT 0,
    free_bytes_last_seen INTEGER DEFAULT 0,
    first_seen_at TEXT DEFAULT (datetime('now')),
    last_seen_at TEXT DEFAULT (datetime('now')),
    last_scan_at TEXT,
    last_sync_at TEXT
);

CREATE TABLE IF NOT EXISTS device_tracks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT DEFAULT '',
    device_path TEXT NOT NULL,
    file_hash TEXT,
    file_size INTEGER,
    title TEXT DEFAULT '',
    artist TEXT DEFAULT '',
    album TEXT DEFAULT '',
    album_artist TEXT DEFAULT '',
    genre TEXT DEFAULT '',
    year INTEGER,
    track_number INTEGER,
    disc_number INTEGER DEFAULT 1,
    duration REAL DEFAULT 0.0,
    bitrate INTEGER DEFAULT 0,
    codec TEXT DEFAULT '',
    local_track_id INTEGER,
    metadata_hash TEXT DEFAULT '',
    last_synced_metadata_hash TEXT DEFAULT '',
    last_synced_file_hash TEXT DEFAULT '',
    artwork_hint TEXT DEFAULT '',
    present_on_device INTEGER DEFAULT 1,
    last_verified_at TEXT,
    last_synced_at TEXT,
    last_scan TEXT DEFAULT (datetime('now')),
    FOREIGN KEY (local_track_id) REFERENCES tracks(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS runtime_imports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT DEFAULT '',
    source_path TEXT DEFAULT '',
    source_mtime REAL DEFAULT 0,
    source_size INTEGER DEFAULT 0,
    imported_at TEXT DEFAULT (datetime('now')),
    parser_name TEXT DEFAULT '',
    status TEXT DEFAULT 'ok',
    details TEXT DEFAULT ''
);

CREATE TABLE IF NOT EXISTS runtime_stats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT DEFAULT '',
    source_path TEXT DEFAULT '',
    source_track_key TEXT DEFAULT '',
    local_track_id INTEGER,
    device_track_id INTEGER,
    confidence REAL DEFAULT 0.0,
    match_basis TEXT DEFAULT '',
    play_count INTEGER,
    last_played TEXT,
    rating INTEGER,
    play_time_seconds REAL,
    imported_at TEXT DEFAULT (datetime('now')),
    raw_payload TEXT DEFAULT '',
    FOREIGN KEY (local_track_id) REFERENCES tracks(id) ON DELETE SET NULL,
    FOREIGN KEY (device_track_id) REFERENCES device_tracks(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS device_playlists (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    name TEXT NOT NULL,
    source_path TEXT DEFAULT '',
    source_mtime REAL DEFAULT 0,
    source_size INTEGER DEFAULT 0,
    track_count INTEGER DEFAULT 0,
    imported_at TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS device_playlist_tracks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_playlist_id INTEGER NOT NULL,
    position INTEGER NOT NULL,
    device_track_id INTEGER,
    local_track_id INTEGER,
    entry_path TEXT DEFAULT '',
    title TEXT DEFAULT '',
    artist TEXT DEFAULT '',
    album TEXT DEFAULT '',
    FOREIGN KEY (device_playlist_id) REFERENCES device_playlists(id) ON DELETE CASCADE,
    FOREIGN KEY (device_track_id) REFERENCES device_tracks(id) ON DELETE SET NULL,
    FOREIGN KEY (local_track_id) REFERENCES tracks(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_tracks_artist ON tracks(artist);
CREATE INDEX IF NOT EXISTS idx_tracks_album ON tracks(album);
CREATE INDEX IF NOT EXISTS idx_tracks_album_artist ON tracks(album_artist);
CREATE INDEX IF NOT EXISTS idx_tracks_genre ON tracks(genre);
CREATE INDEX IF NOT EXISTS idx_tracks_year ON tracks(year);
CREATE INDEX IF NOT EXISTS idx_tracks_media_type ON tracks(media_type);
CREATE INDEX IF NOT EXISTS idx_tracks_file_hash ON tracks(file_hash);
CREATE INDEX IF NOT EXISTS idx_tracks_synced ON tracks(synced_to_device);
CREATE INDEX IF NOT EXISTS idx_tracks_metadata_hash ON tracks(metadata_hash);
CREATE INDEX IF NOT EXISTS idx_tracks_title_artist_album ON tracks(title, artist, album);
CREATE INDEX IF NOT EXISTS idx_tracks_play_count ON tracks(play_count);
CREATE INDEX IF NOT EXISTS idx_tracks_rating ON tracks(rating);
CREATE INDEX IF NOT EXISTS idx_tracks_last_played ON tracks(last_played);
CREATE INDEX IF NOT EXISTS idx_tracks_date_added ON tracks(date_added);
CREATE INDEX IF NOT EXISTS idx_device_tracks_hash ON device_tracks(file_hash);
CREATE INDEX IF NOT EXISTS idx_device_tracks_metadata_hash ON device_tracks(metadata_hash);
CREATE INDEX IF NOT EXISTS idx_device_tracks_path ON device_tracks(device_path);
CREATE INDEX IF NOT EXISTS idx_device_tracks_device ON device_tracks(device_id);
CREATE INDEX IF NOT EXISTS idx_device_tracks_device_local ON device_tracks(device_id, local_track_id);
CREATE INDEX IF NOT EXISTS idx_device_tracks_device_present_path ON device_tracks(device_id, present_on_device, device_path);
CREATE INDEX IF NOT EXISTS idx_device_tracks_device_present_local ON device_tracks(device_id, present_on_device, local_track_id);
CREATE UNIQUE INDEX IF NOT EXISTS uq_device_tracks_device_path ON device_tracks(device_id, device_path);
CREATE INDEX IF NOT EXISTS idx_playlist_tracks_playlist ON playlist_tracks(playlist_id);
CREATE INDEX IF NOT EXISTS idx_playlist_tracks_track ON playlist_tracks(track_id);
CREATE INDEX IF NOT EXISTS idx_playlist_tracks_playlist_position ON playlist_tracks(playlist_id, position);
CREATE INDEX IF NOT EXISTS idx_runtime_imports_device_path ON runtime_imports(device_id, source_path);
CREATE INDEX IF NOT EXISTS idx_runtime_stats_local_track ON runtime_stats(local_track_id);
CREATE INDEX IF NOT EXISTS idx_runtime_stats_device_track ON runtime_stats(device_track_id);
CREATE INDEX IF NOT EXISTS idx_runtime_stats_device_source ON runtime_stats(device_id, source_path);
CREATE INDEX IF NOT EXISTS idx_runtime_stats_play_count ON runtime_stats(play_count);
CREATE INDEX IF NOT EXISTS idx_runtime_stats_last_played ON runtime_stats(last_played);
CREATE INDEX IF NOT EXISTS idx_runtime_stats_rating ON runtime_stats(rating);
CREATE INDEX IF NOT EXISTS idx_device_playlists_device ON device_playlists(device_id);
CREATE UNIQUE INDEX IF NOT EXISTS uq_device_playlists_device_source ON device_playlists(device_id, source_path);
CREATE INDEX IF NOT EXISTS idx_device_playlist_tracks_playlist ON device_playlist_tracks(device_playlist_id, position);
"""


class Database:
    """SQLite database wrapper for the music library."""

    _init_guard = threading.RLock()
    _initialized_paths = set()
    _write_locks = {}
    _write_locks_guard = threading.RLock()

    def __init__(self, db_path, initialize=None):
        self._path = db_path
        self._conn = None
        self._ensure_dir()
        self._connect()
        if initialize is None:
            initialize = threading.current_thread() is threading.main_thread()
        if initialize:
            self.init_db_once(db_path, self._conn)

    @classmethod
    def init_db_once(cls, db_path, connection=None):
        """Initialize and migrate a DB exactly once per process on the main thread."""
        if threading.current_thread() is not threading.main_thread():
            logger.debug("Skipping schema initialization outside the main thread")
            return

        path = os.path.abspath(db_path)
        with cls._init_guard:
            if path in cls._initialized_paths:
                return
            owns_connection = connection is None
            conn = connection
            if conn is None:
                d = os.path.dirname(path)
                if d:
                    os.makedirs(d, exist_ok=True)
                conn = sqlite3.connect(path, timeout=5.0, isolation_level=None)
                conn.row_factory = sqlite3.Row
                cls._configure_connection(conn)
            try:
                cls._init_schema_on_connection(conn)
                cls._initialized_paths.add(path)
            finally:
                if owns_connection:
                    conn.close()

    @classmethod
    def reset_init_guard_for_tests(cls):
        cls._initialized_paths.clear()

    def _ensure_dir(self):
        d = os.path.dirname(self._path)
        if d:
            os.makedirs(d, exist_ok=True)

    def _connect(self):
        self._conn = sqlite3.connect(self._path, timeout=5.0, isolation_level=None)
        self._conn.row_factory = sqlite3.Row
        self._configure_connection(self._conn)

    @staticmethod
    def _configure_connection(conn):
        conn.execute("PRAGMA busy_timeout = 5000")
        try:
            conn.execute("PRAGMA journal_mode=WAL")
        except sqlite3.OperationalError:
            logger.warning("SQLite WAL mode unavailable for this database; falling back to DELETE journal mode")
            conn.execute("PRAGMA journal_mode=DELETE")
        conn.execute("PRAGMA foreign_keys=ON")
        conn.execute("PRAGMA synchronous=NORMAL")

    @classmethod
    def _init_schema_on_connection(cls, conn):
        current_version = cls._schema_version_on_connection(conn)
        if current_version >= SCHEMA_VERSION:
            return

        conn.executescript(SCHEMA_SQL)
        cls._migrate_schema_on_connection(conn, current_version)
        row = conn.execute(
            "SELECT version FROM schema_version LIMIT 1"
        ).fetchone()
        if row is None:
            conn.execute(
                "INSERT INTO schema_version (version) VALUES (?)",
                (SCHEMA_VERSION,),
            )
        elif row["version"] < SCHEMA_VERSION:
            conn.execute("UPDATE schema_version SET version = ?", (SCHEMA_VERSION,))
        conn.commit()

    @classmethod
    def _schema_version_on_connection(cls, conn):
        try:
            row = conn.execute(
                "SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'schema_version'"
            ).fetchone()
            if row is None:
                return 0
            row = conn.execute("SELECT version FROM schema_version LIMIT 1").fetchone()
            if row is None:
                return 0
            return int(row["version"] or 0)
        except (sqlite3.DatabaseError, TypeError, ValueError):
            return 0

    @classmethod
    def _migrate_schema_on_connection(cls, conn, current_version=0):
        """Apply lightweight migrations for existing RockPod databases."""
        if current_version < 4:
            track_cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(device_tracks)")
            }
            additions = {
                "artwork_hint": "TEXT DEFAULT ''",
                "present_on_device": "INTEGER DEFAULT 1",
                "last_verified_at": "TEXT",
                "last_synced_at": "TEXT",
            }
            for col, spec in additions.items():
                if col not in track_cols:
                    conn.execute(f"ALTER TABLE device_tracks ADD COLUMN {col} {spec}")
            conn.execute(
                "DELETE FROM device_tracks WHERE id NOT IN ("
                "SELECT MAX(id) FROM device_tracks GROUP BY device_id, device_path)"
            )
            conn.execute("DROP INDEX IF EXISTS idx_device_tracks_device_path")
            conn.execute(
                "CREATE UNIQUE INDEX IF NOT EXISTS uq_device_tracks_device_path "
                "ON device_tracks(device_id, device_path)"
            )
            playlist_cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(playlists)")
            }
            if "sort_order" not in playlist_cols:
                conn.execute("ALTER TABLE playlists ADD COLUMN sort_order TEXT DEFAULT 'manual'")

        if current_version < 5:
            device_playlist_cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(device_playlists)")
            }
            if "source_mtime" not in device_playlist_cols:
                conn.execute("ALTER TABLE device_playlists ADD COLUMN source_mtime REAL DEFAULT 0")
            if "source_size" not in device_playlist_cols:
                conn.execute("ALTER TABLE device_playlists ADD COLUMN source_size INTEGER DEFAULT 0")
            conn.execute(
                "CREATE UNIQUE INDEX IF NOT EXISTS uq_device_playlists_device_source "
                "ON device_playlists(device_id, source_path)"
            )

        if current_version < 6:
            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_tracks_title_artist_album "
                "ON tracks(title, artist, album)"
            )
            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_device_tracks_device_present_path "
                "ON device_tracks(device_id, present_on_device, device_path)"
            )
            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_device_tracks_device_present_local "
                "ON device_tracks(device_id, present_on_device, local_track_id)"
            )

        if current_version < 7:
            track_cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(tracks)")
            }
            if "media_type" not in track_cols:
                conn.execute("ALTER TABLE tracks ADD COLUMN media_type TEXT DEFAULT 'audio'")
            conn.execute("UPDATE tracks SET media_type = 'audio' WHERE COALESCE(media_type, '') = ''")
            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_tracks_media_type "
                "ON tracks(media_type)"
            )

        if current_version < 8:
            track_cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(tracks)")
            }
            additions = {
                "video_kind": "TEXT DEFAULT ''",
                "show_title": "TEXT DEFAULT ''",
                "season_number": "INTEGER",
                "episode_number": "INTEGER",
            }
            for col, spec in additions.items():
                if col not in track_cols:
                    conn.execute(f"ALTER TABLE tracks ADD COLUMN {col} {spec}")
            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_tracks_video_kind "
                "ON tracks(video_kind)"
            )
            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_tracks_show_title "
                "ON tracks(show_title)"
            )

        if current_version < 9:
            playlist_cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(playlists)")
            }
            if "sync_to_rockbox" not in playlist_cols:
                conn.execute("ALTER TABLE playlists ADD COLUMN sync_to_rockbox INTEGER DEFAULT 1")

        if current_version < 10:
            track_cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(tracks)")
            }
            additions = {
                "imdb_id": "TEXT DEFAULT ''",
                "tmdb_id": "TEXT DEFAULT ''",
                "metadata_source": "TEXT DEFAULT ''",
                "metadata_confidence": "REAL DEFAULT 0.0",
                "metadata_locked": "INTEGER DEFAULT 0",
                "plot_short": "TEXT DEFAULT ''",
                "plot_long": "TEXT DEFAULT ''",
            }
            for col, spec in additions.items():
                if col not in track_cols:
                    conn.execute(f"ALTER TABLE tracks ADD COLUMN {col} {spec}")

        if current_version < 11:
            device_track_cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(device_tracks)")
            }
            additions = {
                "last_synced_metadata_hash": "TEXT DEFAULT ''",
                "last_synced_file_hash": "TEXT DEFAULT ''",
            }
            for col, spec in additions.items():
                if col not in device_track_cols:
                    conn.execute(f"ALTER TABLE device_tracks ADD COLUMN {col} {spec}")
            conn.execute(
                "UPDATE device_tracks SET "
                "last_synced_metadata_hash = COALESCE(NULLIF(last_synced_metadata_hash, ''), metadata_hash, ''), "
                "last_synced_file_hash = COALESCE(NULLIF(last_synced_file_hash, ''), file_hash, '') "
                "WHERE local_track_id IS NOT NULL"
            )

        if current_version < 12:
            track_cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(tracks)")
            }
            additions = {
                "video_hidden": "INTEGER DEFAULT 0",
                "video_locked": "INTEGER DEFAULT 0",
            }
            for col, spec in additions.items():
                if col not in track_cols:
                    conn.execute(f"ALTER TABLE tracks ADD COLUMN {col} {spec}")
            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_tracks_video_privacy "
                "ON tracks(media_type, video_hidden, video_locked)"
            )

        if current_version < 13:
            track_cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(tracks)")
            }
            additions = {
                # content_rating was already read by the video manifest
                # exporter but never existed as a column, so every synced row
                # shipped an empty rating. show_plot carries the series-level
                # synopsis so a show or season screen has a description even
                # when no episode is selected.
                "content_rating": "TEXT DEFAULT ''",
                "show_plot": "TEXT DEFAULT ''",
            }
            for col, spec in additions.items():
                if col not in track_cols:
                    conn.execute(f"ALTER TABLE tracks ADD COLUMN {col} {spec}")

    @classmethod
    def _write_lock_for_path(cls, path):
        abs_path = os.path.abspath(path)
        with cls._write_locks_guard:
            lock = cls._write_locks.get(abs_path)
            if lock is None:
                lock = threading.RLock()
                cls._write_locks[abs_path] = lock
            return lock

    @contextmanager
    def write_lock(self):
        """Serialize writes to this SQLite file within the RockPod process."""
        lock = self._write_lock_for_path(self._path)
        with lock:
            yield

    @contextmanager
    def transaction(self):
        """Context manager for a database transaction."""
        with self.write_lock():
            try:
                self._conn.execute("BEGIN IMMEDIATE")
                yield self._conn
                self._conn.commit()
            except Exception:
                self._conn.rollback()
                raise

    def execute(self, sql, params=None):
        if self._is_write_sql(sql):
            with self.write_lock():
                if params:
                    return self._conn.execute(sql, params)
                return self._conn.execute(sql)
        if params:
            return self._conn.execute(sql, params)
        return self._conn.execute(sql)

    def executemany(self, sql, params_list):
        if self._is_write_sql(sql):
            with self.write_lock():
                return self._conn.executemany(sql, params_list)
        return self._conn.executemany(sql, params_list)

    def commit(self):
        self._conn.commit()

    def rollback(self):
        self._conn.rollback()

    @staticmethod
    def _is_write_sql(sql):
        op = sql.lstrip().split(None, 1)[0].upper() if sql and sql.strip() else ""
        return op in {
            "INSERT", "UPDATE", "DELETE", "REPLACE", "CREATE", "DROP", "ALTER",
            "PRAGMA", "VACUUM", "BEGIN", "COMMIT", "ROLLBACK",
        }

    def fetchone(self, sql, params=None):
        cur = self.execute(sql, params)
        return cur.fetchone()

    def fetchall(self, sql, params=None):
        cur = self.execute(sql, params)
        return cur.fetchall()

    def close(self):
        if self._conn:
            self._conn.close()
            self._conn = None

    # ---- Track operations ----

    def upsert_track(self, track_data):
        """Insert or update a track. track_data is a dict."""
        columns = list(track_data.keys())
        placeholders = ", ".join(["?"] * len(columns))
        col_str = ", ".join(columns)
        update_parts = ", ".join(
            [f"{c}=excluded.{c}" for c in columns if c != "file_path"]
        )
        sql = (
            f"INSERT INTO tracks ({col_str}) VALUES ({placeholders}) "
            f"ON CONFLICT(file_path) DO UPDATE SET {update_parts}"
        )
        self.execute(sql, tuple(track_data.values()))

    @staticmethod
    def _append_media_type_filter(sql, params=None, media_type="audio", column="media_type"):
        if media_type is None:
            return sql, tuple(params or ())
        params = list(params or ())
        separator = " WHERE " if " WHERE " not in sql.upper() else " AND "
        sql += f"{separator}{column} = ?"
        params.append(media_type)
        return sql, tuple(params)

    def get_all_tracks(self, order_by="artist, album, disc_number, track_number", media_type="audio"):
        sql, params = self._append_media_type_filter("SELECT * FROM tracks", media_type=media_type)
        sql += f" ORDER BY {order_by}"
        return self.fetchall(sql, params)

    def get_tracks_by_media_type(self, media_type, order_by="title, file_path"):
        return self.fetchall(
            f"SELECT * FROM tracks WHERE media_type = ? ORDER BY {order_by}",
            (media_type,),
        )

    def get_tracks_by_ids(self, track_ids, order_by="artist, album, disc_number, track_number", media_type="audio"):
        ids = [int(track_id) for track_id in (track_ids or [])]
        if not ids:
            return []
        placeholders = ",".join(["?"] * len(ids))
        sql = f"SELECT * FROM tracks WHERE id IN ({placeholders})"
        sql, params = self._append_media_type_filter(sql, ids, media_type=media_type)
        sql += f" ORDER BY {order_by}"
        return self.fetchall(sql, params)

    def get_tracks_by_artist(self, artist):
        return self.fetchall(
            "SELECT * FROM tracks WHERE artist = ? ORDER BY album, disc_number, track_number",
            (artist,),
        )

    def get_tracks_by_album(self, album, artist=None):
        if artist:
            return self.fetchall(
                "SELECT * FROM tracks WHERE album = ? AND (artist = ? OR album_artist = ?) "
                "ORDER BY disc_number, track_number",
                (album, artist, artist),
            )
        return self.fetchall(
            "SELECT * FROM tracks WHERE album = ? ORDER BY disc_number, track_number",
            (album,),
        )

    def search_tracks(self, query):
        q = f"%{query}%"
        return self.fetchall(
            "SELECT * FROM tracks WHERE media_type = 'audio' AND (title LIKE ? OR artist LIKE ? "
            "OR album LIKE ? OR album_artist LIKE ? OR genre LIKE ? "
            "OR composer LIKE ?) ORDER BY artist, album, track_number",
            (q, q, q, q, q, q),
        )

    def get_unsynced_tracks(self):
        return self.fetchall(
            "SELECT * FROM tracks WHERE media_type = 'audio' AND synced_to_device = 0 "
            "ORDER BY artist, album, disc_number, track_number"
        )

    def get_distinct_artists(self, media_type="audio"):
        sql, params = self._append_media_type_filter(
            "SELECT DISTINCT COALESCE(NULLIF(album_artist, ''), artist) as display_artist "
            "FROM tracks WHERE display_artist != ''",
            media_type=media_type,
        )
        sql += " ORDER BY display_artist COLLATE NOCASE"
        rows = self.fetchall(sql, params)
        return [r["display_artist"] for r in rows]

    def get_distinct_albums(self, media_type="audio"):
        sql, params = self._append_media_type_filter(
            "SELECT DISTINCT album FROM tracks WHERE album != ''",
            media_type=media_type,
        )
        sql += " ORDER BY album COLLATE NOCASE"
        rows = self.fetchall(sql, params)
        return [r["album"] for r in rows]

    def get_distinct_genres(self, media_type="audio"):
        sql, params = self._append_media_type_filter(
            "SELECT DISTINCT genre FROM tracks WHERE genre != ''",
            media_type=media_type,
        )
        sql += " ORDER BY genre COLLATE NOCASE"
        rows = self.fetchall(sql, params)
        return [r["genre"] for r in rows]

    def get_tracks_for_browser_filters(self, media_type="audio", genre="", artist="", album=""):
        conditions = []
        params = []
        if media_type is not None:
            conditions.append("media_type = ?")
            params.append(media_type)
        if genre:
            conditions.append("genre = ?")
            params.append(genre)
        if artist:
            conditions.append("(artist = ? OR album_artist = ?)")
            params.extend([artist, artist])
        if album:
            conditions.append("album = ?")
            params.append(album)
        sql = "SELECT * FROM tracks"
        if conditions:
            sql += " WHERE " + " AND ".join(conditions)
        sql += " ORDER BY artist, album, disc_number, track_number, title"
        return self.fetchall(sql, tuple(params))

    def get_track_count(self, media_type="audio"):
        sql, params = self._append_media_type_filter("SELECT COUNT(*) as cnt FROM tracks", media_type=media_type)
        row = self.fetchone(sql, params)
        return row["cnt"] if row else 0

    def get_total_duration(self, media_type="audio"):
        sql, params = self._append_media_type_filter("SELECT SUM(duration) as total FROM tracks", media_type=media_type)
        row = self.fetchone(sql, params)
        return row["total"] or 0.0

    def get_total_size(self, media_type="audio"):
        sql, params = self._append_media_type_filter("SELECT SUM(file_size) as total FROM tracks", media_type=media_type)
        row = self.fetchone(sql, params)
        return row["total"] or 0

    def delete_track(self, track_id):
        self.execute("DELETE FROM tracks WHERE id = ?", (track_id,))

    def delete_tracks_by_paths(self, file_paths):
        """Remove tracks whose file_path is in the given iterable."""
        paths = list(file_paths)
        if not paths:
            return
        placeholders = ",".join(["?"] * len(paths))
        self.execute(
            f"DELETE FROM tracks WHERE file_path IN ({placeholders})", tuple(paths)
        )
        logger.info("Removed %d stale tracks from library", len(paths))

    def delete_tracks_not_in(self, file_paths):
        """Remove tracks whose file_path is not in the given set."""
        existing = self.fetchall("SELECT id, file_path FROM tracks")
        to_delete = [r["id"] for r in existing if r["file_path"] not in file_paths]
        if to_delete:
            placeholders = ",".join(["?"] * len(to_delete))
            self.execute(
                f"DELETE FROM tracks WHERE id IN ({placeholders})", tuple(to_delete)
            )
            logger.info("Removed %d stale tracks from library", len(to_delete))

    def get_track_file_states(self):
        """Return cached filesystem state keyed by file path."""
        sql, params = self._append_media_type_filter(
            "SELECT file_path, last_modified, file_size, metadata_hash, file_hash, media_type, "
            "video_kind, title, artist, album, album_artist, year, show_title, "
            "season_number, episode_number, track_number "
            "FROM tracks",
            media_type=None,
        )
        rows = self.fetchall(sql, params)
        return {r["file_path"]: dict(r) for r in rows}

    def mark_synced(self, track_id, device_path, metadata_hash="", file_hash=""):
        self.execute(
            "UPDATE tracks SET synced_to_device = 1, device_path = ?, "
            "last_synced_metadata_hash = ?, last_synced_file_hash = ? WHERE id = ?",
            (device_path, metadata_hash, file_hash, track_id),
        )

    def mark_synced_many(self, updates):
        """Update sync state for many tracks with one prepared statement."""
        rows = [
            (device_path, metadata_hash, file_hash, int(track_id))
            for track_id, device_path, metadata_hash, file_hash in (updates or [])
            if track_id
        ]
        if not rows:
            return 0
        self.executemany(
            "UPDATE tracks SET synced_to_device = 1, device_path = ?, "
            "last_synced_metadata_hash = ?, last_synced_file_hash = ? WHERE id = ?",
            rows,
        )
        return len(rows)

    def clear_sync_status(self):
        self.execute(
            "UPDATE tracks SET synced_to_device = 0, device_path = NULL, "
            "last_synced_metadata_hash = '', last_synced_file_hash = ''"
        )

    def get_tracks_needing_resync(self):
        """Return tracks that are synced but whose metadata or file has changed."""
        return self.fetchall(
            "SELECT * FROM tracks WHERE media_type = 'audio' AND synced_to_device = 1 "
            "AND (metadata_hash != last_synced_metadata_hash "
            "     OR (last_synced_file_hash != '' AND file_hash != '' "
            "         AND file_hash != last_synced_file_hash)) "
            "ORDER BY artist, album, disc_number, track_number"
        )

    def get_track_by_path(self, file_path):
        return self.fetchone("SELECT * FROM tracks WHERE file_path = ?", (file_path,))

    def get_track_by_id(self, track_id):
        return self.fetchone("SELECT * FROM tracks WHERE id = ?", (track_id,))

    def update_track_metadata(self, track_id, updates):
        """Update specific fields on a track."""
        editable_fields = {
            "title",
            "artist",
            "album",
            "album_artist",
            "show_title",
            "genre",
            "year",
            "season_number",
            "episode_number",
            "track_number",
            "track_total",
            "disc_number",
            "disc_total",
            "composer",
            "comment",
            "compilation",
            "rating",
            "play_count",
            "last_played",
            "date_added",
            "video_kind",
            "artwork_path",
            "has_embedded_artwork",
            "imdb_id",
            "tmdb_id",
            "metadata_source",
            "metadata_confidence",
            "metadata_locked",
            "video_hidden",
            "video_locked",
            "plot_short",
            "plot_long",
            "content_rating",
            "show_plot",
        }
        metadata_hash_fields = {
            "title",
            "artist",
            "album",
            "album_artist",
            "show_title",
            "genre",
            "year",
            "season_number",
            "episode_number",
            "track_number",
            "disc_number",
            "composer",
            "video_kind",
        }
        sanitized = {key: value for key, value in (updates or {}).items() if key in editable_fields}
        if not sanitized:
            return

        if metadata_hash_fields.intersection(sanitized):
            row = self.get_track_by_id(track_id)
            if row is not None:
                merged = dict(row)
                merged.update(sanitized)
                sanitized["metadata_hash"] = compute_metadata_hash(
                    merged.get("title", ""),
                    merged.get("artist", ""),
                    merged.get("album", ""),
                    merged.get("album_artist", ""),
                    merged.get("track_number"),
                    merged.get("disc_number"),
                    merged.get("genre", ""),
                    merged.get("year"),
                    merged.get("composer", ""),
                    merged.get("duration", 0.0),
                    merged.get("bitrate", 0),
                    merged.get("codec", ""),
                    merged.get("media_type", ""),
                    merged.get("video_kind", ""),
                    merged.get("show_title", ""),
                    merged.get("season_number"),
                    merged.get("episode_number"),
                )

        parts = ", ".join([f"{k} = ?" for k in sanitized.keys()])
        vals = list(sanitized.values()) + [track_id]
        self.execute(f"UPDATE tracks SET {parts} WHERE id = ?", tuple(vals))

    # ---- Playlist operations ----

    def create_playlist(self, name, is_smart=False, rules_json=None):
        cur = self.execute(
            "INSERT INTO playlists (name, is_smart, rules_json) VALUES (?, ?, ?)",
            (name, int(is_smart), rules_json),
        )
        return cur.lastrowid

    def rename_playlist(self, playlist_id, name):
        self.execute(
            "UPDATE playlists SET name = ?, date_modified = datetime('now') WHERE id = ?",
            (name, playlist_id),
        )

    def set_playlist_sync_to_rockbox(self, playlist_id, enabled):
        self.execute(
            "UPDATE playlists SET sync_to_rockbox = ?, date_modified = datetime('now') WHERE id = ?",
            (1 if enabled else 0, playlist_id),
        )

    def get_playlist(self, playlist_id):
        return self.fetchone("SELECT * FROM playlists WHERE id = ?", (playlist_id,))

    def get_all_playlists(self):
        return self.fetchall(
            "SELECT * FROM playlists ORDER BY is_smart DESC, name COLLATE NOCASE"
        )

    def get_playlist_track_count(self, playlist_id):
        row = self.fetchone(
            "SELECT COUNT(*) as cnt FROM playlist_tracks WHERE playlist_id = ?",
            (playlist_id,),
        )
        return row["cnt"] if row else 0

    def get_playlist_track_counts(self, playlist_ids=None):
        ids = [int(playlist_id) for playlist_id in (playlist_ids or [])]
        sql = "SELECT playlist_id, COUNT(*) AS cnt FROM playlist_tracks"
        params = []
        if ids:
            placeholders = ",".join(["?"] * len(ids))
            sql += f" WHERE playlist_id IN ({placeholders})"
            params.extend(ids)
        sql += " GROUP BY playlist_id"
        rows = self.fetchall(sql, tuple(params) if params else None)
        counts = {row["playlist_id"]: row["cnt"] for row in rows}
        if ids:
            return {playlist_id: counts.get(playlist_id, 0) for playlist_id in ids}
        return counts

    def get_playlist_tracks(self, playlist_id):
        return self.fetchall(
            "SELECT t.* FROM tracks t "
            "JOIN playlist_tracks pt ON t.id = pt.track_id "
            "WHERE pt.playlist_id = ? ORDER BY pt.position",
            (playlist_id,),
        )

    def add_track_to_playlist(self, playlist_id, track_id, position=None):
        existing = self.fetchone(
            "SELECT id FROM playlist_tracks WHERE playlist_id = ? AND track_id = ?",
            (playlist_id, track_id),
        )
        if existing:
            return existing["id"]
        if position is None:
            row = self.fetchone(
                "SELECT MAX(position) as maxp FROM playlist_tracks WHERE playlist_id = ?",
                (playlist_id,),
            )
            position = (row["maxp"] or 0) + 1
        cur = self.execute(
            "INSERT INTO playlist_tracks (playlist_id, track_id, position) VALUES (?, ?, ?)",
            (playlist_id, track_id, position),
        )
        self.execute(
            "UPDATE playlists SET date_modified = datetime('now') WHERE id = ?",
            (playlist_id,),
        )
        return cur.lastrowid

    def add_tracks_to_playlist(self, playlist_id, track_ids):
        ordered_ids = []
        seen = set()
        for track_id in (track_ids or []):
            try:
                normalized = int(track_id)
            except (TypeError, ValueError):
                continue
            if normalized in seen:
                continue
            seen.add(normalized)
            ordered_ids.append(normalized)
        if not ordered_ids:
            return 0

        placeholders = ",".join(["?"] * len(ordered_ids))
        existing_rows = self.fetchall(
            f"SELECT track_id FROM playlist_tracks WHERE playlist_id = ? AND track_id IN ({placeholders})",
            (playlist_id, *ordered_ids),
        )
        existing_ids = {row["track_id"] for row in existing_rows}
        to_add = [track_id for track_id in ordered_ids if track_id not in existing_ids]
        if not to_add:
            return 0

        row = self.fetchone(
            "SELECT MAX(position) as maxp FROM playlist_tracks WHERE playlist_id = ?",
            (playlist_id,),
        )
        start_pos = (row["maxp"] or 0) + 1
        self.executemany(
            "INSERT INTO playlist_tracks (playlist_id, track_id, position) VALUES (?, ?, ?)",
            [
                (playlist_id, track_id, start_pos + index)
                for index, track_id in enumerate(to_add)
            ],
        )
        self.execute(
            "UPDATE playlists SET date_modified = datetime('now') WHERE id = ?",
            (playlist_id,),
        )
        return len(to_add)

    def remove_track_from_playlist(self, playlist_id, track_id):
        self.execute(
            "DELETE FROM playlist_tracks WHERE playlist_id = ? AND track_id = ?",
            (playlist_id, track_id),
        )
        self.execute(
            "UPDATE playlists SET date_modified = datetime('now') WHERE id = ?",
            (playlist_id,),
        )

    def move_playlist_track(self, playlist_id, track_id, new_position):
        """Move a playlist track to a 1-based position."""
        rows = self.fetchall(
            "SELECT track_id FROM playlist_tracks WHERE playlist_id = ? "
            "ORDER BY position, id",
            (playlist_id,),
        )
        ids = [r["track_id"] for r in rows]
        if track_id not in ids:
            return
        ids.remove(track_id)
        pos = max(0, min(new_position - 1, len(ids)))
        ids.insert(pos, track_id)
        for idx, tid in enumerate(ids, 1):
            self.execute(
                "UPDATE playlist_tracks SET position = ? "
                "WHERE playlist_id = ? AND track_id = ?",
                (idx, playlist_id, tid),
            )
        self.execute(
            "UPDATE playlists SET date_modified = datetime('now') WHERE id = ?",
            (playlist_id,),
        )

    def set_playlist_track_order(self, playlist_id, track_ids):
        """Replace manual playlist order with the given track ids."""
        rows = self.fetchall(
            "SELECT track_id FROM playlist_tracks WHERE playlist_id = ? "
            "ORDER BY position, id",
            (playlist_id,),
        )
        existing_ids = [row["track_id"] for row in rows]
        existing_set = set(existing_ids)
        ordered = []
        seen = set()
        for track_id in track_ids or []:
            try:
                normalized = int(track_id)
            except (TypeError, ValueError):
                continue
            if normalized in seen or normalized not in existing_set:
                continue
            seen.add(normalized)
            ordered.append(normalized)
        ordered.extend(track_id for track_id in existing_ids if track_id not in seen)
        if ordered == existing_ids:
            return 0
        for index, track_id in enumerate(ordered, 1):
            self.execute(
                "UPDATE playlist_tracks SET position = ? "
                "WHERE playlist_id = ? AND track_id = ?",
                (index, playlist_id, track_id),
            )
        self.execute(
            "UPDATE playlists SET date_modified = datetime('now') WHERE id = ?",
            (playlist_id,),
        )
        return len(ordered)

    def delete_playlist(self, playlist_id):
        self.execute("DELETE FROM playlist_tracks WHERE playlist_id = ?", (playlist_id,))
        self.execute("DELETE FROM playlists WHERE id = ?", (playlist_id,))

    # ---- Device operations ----

    def upsert_device(self, data):
        """Insert or update a remembered device by stable_device_key."""
        columns = list(data.keys())
        placeholders = ", ".join(["?"] * len(columns))
        col_str = ", ".join(columns)
        update_parts = ", ".join(
            [
                f"{c}=excluded.{c}"
                for c in columns
                if c not in ("id", "stable_device_key", "first_seen_at")
            ]
        )
        if update_parts:
            update_parts += ", last_seen_at=datetime('now')"
        else:
            update_parts = "last_seen_at=datetime('now')"
        sql = (
            f"INSERT INTO devices ({col_str}) VALUES ({placeholders}) "
            f"ON CONFLICT(stable_device_key) DO UPDATE SET {update_parts}"
        )
        self.execute(sql, tuple(data.values()))
        self._merge_duplicate_device_rows(data["stable_device_key"])
        return self.get_device_by_key(data["stable_device_key"])

    def _merge_duplicate_device_rows(self, stable_device_key):
        """Merge remembered rows for the same mounted device into the current key."""
        target = self.get_device_by_key(stable_device_key)
        if not target:
            return
        mount_path = (target["mount_path_last_seen"] or "").rstrip("/\\")
        capacity = target["capacity_bytes"] or 0
        if not mount_path or capacity <= 0:
            return

        duplicates = self.fetchall(
            "SELECT * FROM devices WHERE stable_device_key != ? "
            "AND rtrim(rtrim(mount_path_last_seen, '/'), '\\') = ? "
            "AND capacity_bytes = ?",
            (stable_device_key, mount_path, capacity),
        )
        for duplicate in duplicates:
            old_key = duplicate["stable_device_key"]
            self._merge_device_track_rows(old_key, stable_device_key)
            self._merge_device_playlist_rows(old_key, stable_device_key)
            self.execute(
                "UPDATE runtime_imports SET device_id = ? WHERE device_id = ?",
                (stable_device_key, old_key),
            )
            self.execute(
                "UPDATE runtime_stats SET device_id = ? WHERE device_id = ?",
                (stable_device_key, old_key),
            )
            self.execute(
                "UPDATE devices SET first_seen_at = MIN(first_seen_at, ?), "
                "last_scan_at = COALESCE(last_scan_at, ?), "
                "last_sync_at = COALESCE(last_sync_at, ?) "
                "WHERE stable_device_key = ?",
                (
                    duplicate["first_seen_at"],
                    duplicate["last_scan_at"],
                    duplicate["last_sync_at"],
                    stable_device_key,
                ),
            )
            self.execute("DELETE FROM devices WHERE stable_device_key = ?", (old_key,))

    def _merge_device_track_rows(self, old_key, stable_device_key):
        self.execute(
            "UPDATE device_tracks SET device_id = ? "
            "WHERE device_id = ? "
            "AND NOT EXISTS ("
            "SELECT 1 FROM device_tracks AS current "
            "WHERE current.device_id = ? "
            "AND current.device_path = device_tracks.device_path)",
            (stable_device_key, old_key, stable_device_key),
        )
        self.execute("DELETE FROM device_tracks WHERE device_id = ?", (old_key,))

    def _merge_device_playlist_rows(self, old_key, stable_device_key):
        self.execute(
            "UPDATE device_playlists SET device_id = ? "
            "WHERE device_id = ? "
            "AND NOT EXISTS ("
            "SELECT 1 FROM device_playlists AS current "
            "WHERE current.device_id = ? "
            "AND current.source_path = device_playlists.source_path)",
            (stable_device_key, old_key, stable_device_key),
        )
        self.execute("DELETE FROM device_playlists WHERE device_id = ?", (old_key,))

    def get_device_by_key(self, stable_device_key):
        return self.fetchone(
            "SELECT * FROM devices WHERE stable_device_key = ?",
            (stable_device_key,),
        )

    def get_all_devices(self):
        return self.fetchall("SELECT * FROM devices ORDER BY last_seen_at DESC")

    def mark_device_scanned(self, stable_device_key):
        self.execute(
            "UPDATE devices SET last_scan_at = datetime('now'), "
            "last_seen_at = datetime('now') WHERE stable_device_key = ?",
            (stable_device_key,),
        )

    def mark_device_synced(self, stable_device_key):
        self.execute(
            "UPDATE devices SET last_sync_at = datetime('now'), "
            "last_seen_at = datetime('now') WHERE stable_device_key = ?",
            (stable_device_key,),
        )

    def forget_device(self, stable_device_key):
        self.execute("DELETE FROM device_tracks WHERE device_id = ?", (stable_device_key,))
        self.execute("DELETE FROM devices WHERE stable_device_key = ?", (stable_device_key,))

    # ---- Device track operations ----

    def upsert_device_track(self, data):
        row = dict(data)
        row.setdefault("device_id", "")
        row.setdefault("present_on_device", 1)
        columns = list(row.keys())
        placeholders = ", ".join(["?"] * len(columns))
        col_str = ", ".join(columns)
        update_parts = ", ".join(
            [f"{c}=excluded.{c}" for c in columns if c != "id"]
        )
        sql = (
            f"INSERT INTO device_tracks ({col_str}) VALUES ({placeholders}) "
            f"ON CONFLICT(device_id, device_path) DO UPDATE SET {update_parts}, "
            "present_on_device=1, last_verified_at=datetime('now'), "
            "last_scan=datetime('now')"
        )
        self.execute(sql, tuple(row.values()))

    def get_all_device_tracks(self, device_id=None, present_only=True):
        where = []
        params = []
        if device_id is not None:
            where.append("device_id = ?")
            params.append(device_id)
        if present_only:
            where.append("COALESCE(present_on_device, 1) = 1")
        sql = "SELECT * FROM device_tracks"
        if where:
            sql += " WHERE " + " AND ".join(where)
        sql += " ORDER BY artist, album, track_number"
        return self.fetchall(sql, tuple(params) if params else None)

    def get_device_track_states(self, device_id):
        rows = self.fetchall(
            "SELECT * FROM device_tracks WHERE device_id = ?",
            (device_id,),
        )
        return {r["device_path"]: dict(r) for r in rows}

    def get_tracks_not_on_device(self, device_id, track_ids=None):
        where = [
            "dt.id IS NULL"
        ]
        params = [device_id]
        if track_ids:
            ids = [int(track_id) for track_id in track_ids]
            placeholders = ",".join(["?"] * len(ids))
            where.append(f"t.id IN ({placeholders})")
            params.extend(ids)
        sql = (
            "SELECT t.* FROM tracks t "
            "LEFT JOIN device_tracks dt "
            "ON dt.local_track_id = t.id AND dt.device_id = ? "
            "AND COALESCE(dt.present_on_device, 1) = 1 "
            f"WHERE t.media_type = 'audio' AND {' AND '.join(where)} "
            "ORDER BY t.artist, t.album, t.disc_number, t.track_number"
        )
        return self.fetchall(sql, tuple(params))

    def count_tracks_not_on_device(self, device_id):
        row = self.fetchone(
            "SELECT COUNT(*) AS cnt FROM tracks t "
            "LEFT JOIN device_tracks dt "
            "ON dt.local_track_id = t.id AND dt.device_id = ? "
            "AND COALESCE(dt.present_on_device, 1) = 1 "
            "WHERE t.media_type = 'audio' AND dt.id IS NULL",
            (device_id,),
        )
        return row["cnt"] if row else 0

    def get_sync_status_counts(self, device_id, include_resync=True):
        missing = self.count_tracks_not_on_device(device_id)
        resync = 0
        if include_resync:
            row = self.fetchone(
                "SELECT COUNT(DISTINCT t.id) AS cnt FROM tracks t "
                "JOIN device_tracks dt ON dt.local_track_id = t.id "
                "AND dt.device_id = ? AND COALESCE(dt.present_on_device, 1) = 1 "
                "WHERE t.media_type = 'audio' AND ("
                "(COALESCE(t.metadata_hash, '') != '' "
                "AND COALESCE(NULLIF(dt.last_synced_metadata_hash, ''), dt.metadata_hash, '') != '' "
                "AND t.metadata_hash != COALESCE(NULLIF(dt.last_synced_metadata_hash, ''), dt.metadata_hash, '')) "
                "OR (COALESCE(t.file_hash, '') != '' "
                "AND COALESCE(NULLIF(dt.last_synced_file_hash, ''), dt.file_hash, '') != '' "
                "AND t.file_hash != COALESCE(NULLIF(dt.last_synced_file_hash, ''), dt.file_hash, ''))) ",
                (device_id,),
            )
            resync = row["cnt"] if row else 0
        return {"missing": missing, "resync": resync}

    # ---- Runtime data operations ----

    def record_runtime_import(self, data):
        columns = list(data.keys())
        placeholders = ", ".join(["?"] * len(columns))
        sql = f"INSERT INTO runtime_imports ({', '.join(columns)}) VALUES ({placeholders})"
        cur = self.execute(sql, tuple(data.values()))
        return cur.lastrowid

    def clear_runtime_stats_for_device(self, device_id, source_path=None):
        if source_path:
            self.execute(
                "DELETE FROM runtime_stats WHERE device_id = ? AND source_path = ?",
                (device_id, source_path),
            )
        else:
            self.execute("DELETE FROM runtime_stats WHERE device_id = ?", (device_id,))

    def upsert_runtime_stat(self, data):
        row = dict(data)
        row.setdefault("device_id", "")
        row.setdefault("source_path", "")
        row.setdefault("source_track_key", "")
        existing = self.fetchone(
            "SELECT id FROM runtime_stats "
            "WHERE device_id = ? AND source_path = ? AND source_track_key = ?",
            (row["device_id"], row["source_path"], row["source_track_key"]),
        )
        if existing:
            assignments = ", ".join([f"{col} = ?" for col in row.keys()])
            values = list(row.values()) + [existing["id"]]
            self.execute(
                f"UPDATE runtime_stats SET {assignments}, imported_at = datetime('now') "
                "WHERE id = ?",
                tuple(values),
            )
            return existing["id"]
        columns = list(row.keys())
        placeholders = ", ".join(["?"] * len(columns))
        cur = self.execute(
            f"INSERT INTO runtime_stats ({', '.join(columns)}) VALUES ({placeholders})",
            tuple(row.values()),
        )
        return cur.lastrowid

    def runtime_import_state(self, device_id, source_path):
        return self.fetchone(
            "SELECT * FROM runtime_imports WHERE device_id = ? AND source_path = ? "
            "ORDER BY imported_at DESC, id DESC LIMIT 1",
            (device_id, source_path),
        )

    def runtime_stats_summary(self, device_id=None):
        where = ""
        params = ()
        if device_id:
            where = " WHERE device_id = ?"
            params = (device_id,)
        return self.fetchone(
            "SELECT COUNT(*) AS entries, "
            "COUNT(DISTINCT local_track_id) AS local_matches, "
            "COUNT(DISTINCT device_track_id) AS device_matches, "
            "COALESCE(SUM(COALESCE(play_count, 0)), 0) AS total_play_count, "
            "COUNT(CASE WHEN COALESCE(rating, 0) > 0 THEN 1 END) AS rated_tracks, "
            "COALESCE(SUM(COALESCE(play_time_seconds, 0)), 0) AS total_play_time_seconds, "
            "MAX(COALESCE(last_played, '')) AS last_played, "
            "MAX(COALESCE(imported_at, '')) AS last_imported_at "
            "FROM runtime_stats"
            + where,
            params,
        )

    def runtime_device_insights(self, device_id):
        if not device_id:
            return {
                "ipod_play_count_total": 0,
                "desktop_play_count_total": 0,
                "combined_play_count_total": 0,
                "most_played_artist": "",
                "most_played_album": "",
                "last_play_sync": "",
            }

        totals = self.fetchone(
            "SELECT "
            "COALESCE(SUM(COALESCE(rs.play_count, 0)), 0) AS ipod_play_count_total, "
            "COALESCE(SUM(CASE WHEN rs.local_track_id IS NOT NULL THEN COALESCE(t.play_count, 0) ELSE 0 END), 0) AS desktop_play_count_total, "
            "MAX(COALESCE(rs.imported_at, '')) AS last_play_sync "
            "FROM runtime_stats rs "
            "LEFT JOIN tracks t ON t.id = rs.local_track_id "
            "WHERE rs.device_id = ?",
            (device_id,),
        ) or {}
        most_artist = self.fetchone(
            "SELECT COALESCE(NULLIF(t.album_artist, ''), NULLIF(t.artist, ''), NULLIF(dt.album_artist, ''), dt.artist, '') AS label, "
            "SUM(COALESCE(rs.play_count, 0)) AS total_plays "
            "FROM runtime_stats rs "
            "LEFT JOIN tracks t ON t.id = rs.local_track_id "
            "LEFT JOIN device_tracks dt ON dt.id = rs.device_track_id "
            "WHERE rs.device_id = ? "
            "GROUP BY label "
            "HAVING label != '' "
            "ORDER BY total_plays DESC, label COLLATE NOCASE ASC LIMIT 1",
            (device_id,),
        )
        most_album = self.fetchone(
            "SELECT COALESCE(NULLIF(t.album, ''), dt.album, '') AS label, "
            "SUM(COALESCE(rs.play_count, 0)) AS total_plays "
            "FROM runtime_stats rs "
            "LEFT JOIN tracks t ON t.id = rs.local_track_id "
            "LEFT JOIN device_tracks dt ON dt.id = rs.device_track_id "
            "WHERE rs.device_id = ? "
            "GROUP BY label "
            "HAVING label != '' "
            "ORDER BY total_plays DESC, label COLLATE NOCASE ASC LIMIT 1",
            (device_id,),
        )
        ipod_total = int((totals["ipod_play_count_total"] if totals else 0) or 0)
        desktop_total = int((totals["desktop_play_count_total"] if totals else 0) or 0)
        return {
            "ipod_play_count_total": ipod_total,
            "desktop_play_count_total": desktop_total,
            "combined_play_count_total": ipod_total + desktop_total,
            "most_played_artist": (most_artist["label"] if most_artist else "") or "",
            "most_played_album": (most_album["label"] if most_album else "") or "",
            "last_play_sync": (totals["last_play_sync"] if totals else "") or "",
        }

    def _runtime_join_sql(self):
        return (
            " LEFT JOIN ("
            "   SELECT rs.local_track_id, "
            "          MAX(COALESCE(rs.play_count, 0)) AS runtime_play_count, "
            "          MAX(COALESCE(rs.rating, 0)) AS runtime_rating, "
            "          MAX(COALESCE(rs.play_time_seconds, 0)) AS runtime_play_time_seconds, "
            "          MAX(COALESCE(rs.last_played, '')) AS runtime_last_played "
            "   FROM runtime_stats rs "
            "   WHERE rs.local_track_id IS NOT NULL "
            "   GROUP BY rs.local_track_id"
            " ) rt ON rt.local_track_id = t.id "
        )

    def _device_presence_expr(self, device_id):
        if not device_id:
            return "0"
        safe_device_id = str(device_id).replace("'", "''")
        return (
            "EXISTS (SELECT 1 FROM device_tracks dt "
            f"WHERE dt.local_track_id = t.id AND dt.device_id = '{safe_device_id}' "
            "AND COALESCE(dt.present_on_device, 1) = 1)"
        )

    def smart_playlist_query(self, where_sql="", params=None, order_by="artist, album, disc_number, track_number"):
        sql = (
            "SELECT t.*, "
            "COALESCE(t.play_count, 0) AS desktop_play_count, "
            "COALESCE(rt.runtime_play_count, 0) AS ipod_play_count, "
            "COALESCE(rt.runtime_play_count, t.play_count, 0) AS effective_play_count, "
            "(COALESCE(t.play_count, 0) + COALESCE(rt.runtime_play_count, 0)) AS combined_play_count, "
            "COALESCE(t.last_played, '') AS desktop_last_played, "
            "COALESCE(NULLIF(rt.runtime_last_played, ''), '') AS ipod_last_played, "
            "COALESCE(NULLIF(rt.runtime_last_played, ''), t.last_played, '') AS effective_last_played, "
            "COALESCE(rt.runtime_rating, t.rating, 0) AS effective_rating, "
            "COALESCE(rt.runtime_play_time_seconds, 0) AS effective_play_time_seconds "
            "FROM tracks t "
            + self._runtime_join_sql()
        )
        if where_sql:
            sql += " WHERE t.media_type = 'audio' AND " + where_sql
        else:
            sql += " WHERE t.media_type = 'audio'"
        sql += f" ORDER BY {order_by}"
        return self.fetchall(sql, tuple(params or ()))

    def smart_playlist_count_query(self, where_sql="", params=None):
        sql = (
            "SELECT COUNT(*) AS cnt "
            "FROM tracks t "
            + self._runtime_join_sql()
        )
        if where_sql:
            sql += " WHERE t.media_type = 'audio' AND " + where_sql
        else:
            sql += " WHERE t.media_type = 'audio'"
        row = self.fetchone(sql, tuple(params or ()))
        return row["cnt"] if row else 0

    def mark_missing_device_tracks(self, device_id, present_paths):
        rows = self.fetchall(
            "SELECT id, device_path, local_track_id FROM device_tracks "
            "WHERE device_id = ? AND COALESCE(present_on_device, 1) = 1",
            (device_id,),
        )
        missing = [r for r in rows if r["device_path"] not in present_paths]
        if not missing:
            return 0
        ids = [r["id"] for r in missing]
        placeholders = ",".join(["?"] * len(ids))
        self.execute(
            f"UPDATE device_tracks SET present_on_device = 0, "
            f"last_verified_at = datetime('now') WHERE id IN ({placeholders})",
            tuple(ids),
        )
        local_ids = [r["local_track_id"] for r in missing if r["local_track_id"]]
        if local_ids:
            placeholders = ",".join(["?"] * len(local_ids))
            self.execute(
                f"UPDATE tracks SET synced_to_device = 0, device_path = NULL, "
                f"last_synced_metadata_hash = '', last_synced_file_hash = '' "
                f"WHERE id IN ({placeholders})",
                tuple(local_ids),
            )
        return len(missing)

    def clear_device_tracks(self, device_id=None):
        if device_id is None:
            self.execute("DELETE FROM device_tracks")
        else:
            self.execute("DELETE FROM device_tracks WHERE device_id = ?", (device_id,))

    # ---- Device playlist operations ----

    def clear_device_playlists(self, device_id):
        self.execute("DELETE FROM device_playlists WHERE device_id = ?", (device_id,))

    def upsert_device_playlist(self, data):
        row = dict(data)
        row.setdefault("source_path", "")
        row.setdefault("source_mtime", 0)
        row.setdefault("source_size", 0)
        row.setdefault("track_count", 0)
        existing = self.fetchone(
            "SELECT id FROM device_playlists WHERE device_id = ? AND source_path = ?",
            (row["device_id"], row["source_path"]),
        )
        if existing:
            self.execute(
                "UPDATE device_playlists SET name = ?, source_mtime = ?, source_size = ?, "
                "track_count = ?, imported_at = datetime('now') "
                "WHERE id = ?",
                (
                    row["name"],
                    row["source_mtime"],
                    row["source_size"],
                    row["track_count"],
                    existing["id"],
                ),
            )
            return self.get_device_playlist(existing["id"])
        cur = self.execute(
            "INSERT INTO device_playlists (device_id, name, source_path, source_mtime, source_size, track_count) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            (
                row["device_id"],
                row["name"],
                row["source_path"],
                row["source_mtime"],
                row["source_size"],
                row["track_count"],
            ),
        )
        return self.get_device_playlist(cur.lastrowid)

    def delete_missing_device_playlists(self, device_id, source_paths):
        paths = [str(path) for path in (source_paths or [])]
        if not paths:
            self.execute("DELETE FROM device_playlists WHERE device_id = ?", (device_id,))
            return
        placeholders = ",".join(["?"] * len(paths))
        self.execute(
            f"DELETE FROM device_playlists WHERE device_id = ? AND source_path NOT IN ({placeholders})",
            (device_id, *paths),
        )

    def replace_device_playlist_tracks(self, device_playlist_id, tracks):
        self.execute("DELETE FROM device_playlist_tracks WHERE device_playlist_id = ?", (device_playlist_id,))
        if not tracks:
            return
        self.executemany(
            "INSERT INTO device_playlist_tracks "
            "(device_playlist_id, position, device_track_id, local_track_id, entry_path, title, artist, album) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            [
                (
                    device_playlist_id,
                    item.get("position", 0),
                    item.get("device_track_id"),
                    item.get("local_track_id"),
                    item.get("entry_path", ""),
                    item.get("title", ""),
                    item.get("artist", ""),
                    item.get("album", ""),
                )
                for item in tracks
            ],
        )

    def get_device_playlists(self, device_id):
        return self.fetchall(
            "SELECT * FROM device_playlists WHERE device_id = ? ORDER BY name COLLATE NOCASE, id",
            (device_id,),
        )

    def get_device_playlist(self, playlist_id):
        return self.fetchone("SELECT * FROM device_playlists WHERE id = ?", (playlist_id,))

    def get_device_playlist_tracks(self, playlist_id):
        rows = self.fetchall(
            "SELECT dpt.position, dpt.entry_path, dpt.title AS playlist_title, "
            "dpt.artist AS playlist_artist, dpt.album AS playlist_album, "
            "t.*, dt.device_path AS device_device_path, dt.file_size AS device_file_size "
            "FROM device_playlist_tracks dpt "
            "LEFT JOIN tracks t ON t.id = dpt.local_track_id "
            "LEFT JOIN device_tracks dt ON dt.id = dpt.device_track_id "
            "WHERE dpt.device_playlist_id = ? "
            "ORDER BY dpt.position ASC, dpt.id ASC",
            (playlist_id,),
        )
        hydrated = []
        for row in rows:
            item = dict(row)
            if not item.get("file_path"):
                item["file_path"] = ""
                item["device_path"] = item.get("device_device_path", "") or item.get("entry_path", "")
                item["title"] = item.get("playlist_title") or item.get("title") or ""
                item["artist"] = item.get("playlist_artist") or item.get("artist") or ""
                item["album"] = item.get("playlist_album") or item.get("album") or ""
                item["file_size"] = item.get("device_file_size") or item.get("file_size") or 0
            hydrated.append(item)
        return hydrated

    def delete_device_track(self, device_track_id):
        self.execute("DELETE FROM device_tracks WHERE id = ?", (device_track_id,))
