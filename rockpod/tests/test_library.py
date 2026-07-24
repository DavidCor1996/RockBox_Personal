"""Tests for library database operations."""

import os
import sqlite3
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from app.database import Database, SCHEMA_VERSION
from models.track import Track


class TestDatabase:
    """Test database CRUD operations."""

    def test_upsert_and_retrieve_track(self, db):
        track_data = {
            "file_path": "/music/test.mp3",
            "title": "Test Song",
            "artist": "Test Artist",
            "album": "Test Album",
            "genre": "Rock",
            "year": 2007,
            "track_number": 1,
            "duration": 240.5,
            "bitrate": 320,
            "codec": "MP3",
            "metadata_hash": "abc123",
        }
        db.upsert_track(track_data)
        db.commit()

        row = db.get_track_by_path("/music/test.mp3")
        assert row is not None
        assert row["title"] == "Test Song"
        assert row["artist"] == "Test Artist"
        assert row["metadata_hash"] == "abc123"

    def test_upsert_updates_existing(self, db):
        db.upsert_track({"file_path": "/music/t.mp3", "title": "V1", "artist": "A"})
        db.commit()
        db.upsert_track({"file_path": "/music/t.mp3", "title": "V2", "artist": "A"})
        db.commit()

        row = db.get_track_by_path("/music/t.mp3")
        assert row["title"] == "V2"
        assert db.get_track_count() == 1

    def test_search_tracks(self, db):
        db.upsert_track({"file_path": "/a.mp3", "title": "Hello World", "artist": "Foo"})
        db.upsert_track({"file_path": "/b.mp3", "title": "Goodbye", "artist": "Bar"})
        db.commit()

        results = db.search_tracks("hello")
        assert len(results) == 1
        assert results[0]["title"] == "Hello World"

    def test_audio_queries_exclude_video_rows_by_default(self, db):
        db.upsert_track({"file_path": "/music/a.mp3", "title": "Song", "media_type": "audio"})
        db.upsert_track({"file_path": "/videos/a.mp4", "title": "Movie", "media_type": "video"})
        db.commit()

        audio_rows = db.get_all_tracks()
        video_rows = db.get_tracks_by_media_type("video")

        assert len(audio_rows) == 1
        assert audio_rows[0]["title"] == "Song"
        assert len(video_rows) == 1
        assert video_rows[0]["title"] == "Movie"
        assert db.get_track_count() == 1
        assert db.get_track_count(media_type=None) == 2

    def test_video_metadata_fields_are_persisted(self, db):
        db.upsert_track({
            "file_path": "/videos/show.m4v",
            "title": "Pilot",
            "media_type": "video",
            "video_kind": "show",
            "show_title": "The Show",
            "season_number": 1,
            "episode_number": 2,
        })
        db.commit()

        row = db.get_track_by_path("/videos/show.m4v")

        assert row["video_kind"] == "show"
        assert row["show_title"] == "The Show"
        assert row["season_number"] == 1
        assert row["episode_number"] == 2

    def test_get_unsynced_tracks(self, db):
        db.upsert_track({"file_path": "/a.mp3", "title": "A", "synced_to_device": 0})
        db.upsert_track({"file_path": "/b.mp3", "title": "B", "synced_to_device": 1})
        db.commit()

        unsynced = db.get_unsynced_tracks()
        assert len(unsynced) == 1
        assert unsynced[0]["title"] == "A"

    def test_mark_synced_with_hashes(self, db):
        db.upsert_track({"file_path": "/a.mp3", "title": "A"})
        db.commit()

        row = db.get_track_by_path("/a.mp3")
        tid = row["id"]
        db.mark_synced(tid, "Music/Artist/Album/01 - A.mp3", "meta_hash_1", "file_hash_1")
        db.commit()

        row = db.get_track_by_path("/a.mp3")
        assert row["synced_to_device"] == 1
        assert row["device_path"] == "Music/Artist/Album/01 - A.mp3"
        assert row["last_synced_metadata_hash"] == "meta_hash_1"
        assert row["last_synced_file_hash"] == "file_hash_1"

    def test_mark_synced_many_uses_one_batch(self, db, monkeypatch):
        for index in range(3):
            db.upsert_track({"file_path": f"/batch-{index}.mp3", "title": f"Batch {index}"})
        db.commit()
        tracks = db.get_all_tracks(order_by="id")
        calls = []
        original = db.executemany

        def counted(sql, params):
            rows = list(params)
            calls.append((sql, rows))
            return original(sql, rows)

        monkeypatch.setattr(db, "executemany", counted)
        updates = [
            (row["id"], f"Music/{row['id']}.mp3", f"meta-{row['id']}", f"file-{row['id']}")
            for row in tracks
        ]
        with db.transaction():
            assert db.mark_synced_many(updates) == 3

        assert len(calls) == 1
        for row in db.get_all_tracks(order_by="id"):
            assert row["synced_to_device"] == 1
            assert row["last_synced_metadata_hash"] == f"meta-{row['id']}"
            assert row["last_synced_file_hash"] == f"file-{row['id']}"

    def test_get_tracks_needing_resync(self, db):
        db.upsert_track({
            "file_path": "/a.mp3", "title": "A",
            "synced_to_device": 1,
            "metadata_hash": "new_hash",
            "last_synced_metadata_hash": "old_hash",
        })
        db.upsert_track({
            "file_path": "/b.mp3", "title": "B",
            "synced_to_device": 1,
            "metadata_hash": "same",
            "last_synced_metadata_hash": "same",
        })
        db.commit()

        resync = db.get_tracks_needing_resync()
        assert len(resync) == 1
        assert resync[0]["title"] == "A"

    def test_clear_sync_status(self, db):
        db.upsert_track({
            "file_path": "/a.mp3", "title": "A",
            "synced_to_device": 1,
            "device_path": "some/path",
            "last_synced_metadata_hash": "hash",
            "last_synced_file_hash": "fhash",
        })
        db.commit()
        db.clear_sync_status()
        db.commit()

        row = db.get_track_by_path("/a.mp3")
        assert row["synced_to_device"] == 0
        assert row["device_path"] is None
        assert row["last_synced_metadata_hash"] == ""
        assert row["last_synced_file_hash"] == ""

    def test_distinct_queries(self, db):
        db.upsert_track({"file_path": "/a.mp3", "artist": "Art1", "album": "Alb1", "genre": "Rock"})
        db.upsert_track({"file_path": "/b.mp3", "artist": "Art2", "album": "Alb2", "genre": "Pop"})
        db.upsert_track({"file_path": "/c.mp3", "artist": "Art1", "album": "Alb1", "genre": "Rock"})
        db.commit()

        assert len(db.get_distinct_artists()) == 2
        assert len(db.get_distinct_albums()) == 2
        assert len(db.get_distinct_genres()) == 2

    def test_distinct_queries_can_target_video_metadata(self, db):
        db.upsert_track({"file_path": "/videos/a.mp4", "title": "Movie A", "artist": "Director A", "album": "Series A", "genre": "Sci-Fi", "media_type": "video"})
        db.upsert_track({"file_path": "/videos/b.mp4", "title": "Movie B", "artist": "Director B", "album": "Series B", "genre": "Drama", "media_type": "video"})
        db.commit()

        assert db.get_distinct_artists(media_type="video") == ["Director A", "Director B"]
        assert db.get_distinct_albums(media_type="video") == ["Series A", "Series B"]
        assert db.get_distinct_genres(media_type="video") == ["Drama", "Sci-Fi"]

    def test_playlist_operations(self, db):
        pid = db.create_playlist("My Playlist")
        db.upsert_track({"file_path": "/a.mp3", "title": "Song A"})
        db.commit()

        row = db.get_track_by_path("/a.mp3")
        db.add_track_to_playlist(pid, row["id"])
        db.commit()

        tracks = db.get_playlist_tracks(pid)
        assert len(tracks) == 1
        assert tracks[0]["title"] == "Song A"

        db.remove_track_from_playlist(pid, row["id"])
        db.commit()
        assert len(db.get_playlist_tracks(pid)) == 0

    def test_playlist_rename_and_delete(self, db):
        pid = db.create_playlist("Old Name")
        db.commit()

        db.rename_playlist(pid, "New Name")
        db.commit()

        playlist = db.get_playlist(pid)
        assert playlist["name"] == "New Name"

        db.delete_playlist(pid)
        db.commit()
        assert db.get_playlist(pid) is None

    def test_playlist_add_prevents_duplicates(self, db):
        pid = db.create_playlist("No Duplicates")
        db.upsert_track({"file_path": "/a.mp3", "title": "Song A"})
        db.commit()
        row = db.get_track_by_path("/a.mp3")

        first = db.add_track_to_playlist(pid, row["id"])
        second = db.add_track_to_playlist(pid, row["id"])
        db.commit()

        tracks = db.get_playlist_tracks(pid)
        assert first == second
        assert len(tracks) == 1
        assert db.get_playlist_track_count(pid) == 1

    def test_playlist_persists_across_connections(self, config, db):
        from app.database import Database

        pid = db.create_playlist("Persistent")
        db.upsert_track({"file_path": "/persist.mp3", "title": "Persist"})
        db.commit()
        row = db.get_track_by_path("/persist.mp3")
        db.add_track_to_playlist(pid, row["id"])
        db.commit()
        db.close()

        reopened = Database(config.db_path)
        try:
            playlist = reopened.get_playlist(pid)
            tracks = reopened.get_playlist_tracks(pid)
            assert playlist["name"] == "Persistent"
            assert len(tracks) == 1
            assert tracks[0]["title"] == "Persist"
        finally:
            reopened.close()

    def test_add_tracks_to_playlist_reports_only_new_members(self, db):
        pid = db.create_playlist("Batch")
        db.upsert_track({"file_path": "/a.mp3", "title": "A"})
        db.upsert_track({"file_path": "/b.mp3", "title": "B"})
        db.commit()
        a = db.get_track_by_path("/a.mp3")["id"]
        b = db.get_track_by_path("/b.mp3")["id"]

        assert db.add_tracks_to_playlist(pid, [a, b]) == 2
        assert db.add_tracks_to_playlist(pid, [a, b]) == 0
        db.commit()

        assert db.get_playlist_track_count(pid) == 2

    def test_set_playlist_track_order_reorders_existing_tracks(self, db):
        pid = db.create_playlist("Manual Order")
        for title in ("A", "B", "C"):
            db.upsert_track({"file_path": f"/{title}.mp3", "title": title})
        db.commit()
        ids = [db.get_track_by_path(f"/{title}.mp3")["id"] for title in ("A", "B", "C")]
        db.add_tracks_to_playlist(pid, ids)
        db.commit()

        assert db.set_playlist_track_order(pid, [ids[2], ids[0], ids[1]]) == 3
        db.commit()

        tracks = db.get_playlist_tracks(pid)
        assert [row["title"] for row in tracks] == ["C", "A", "B"]

    def test_get_playlist_track_counts_batches_results(self, db):
        first = db.create_playlist("First")
        second = db.create_playlist("Second")
        db.upsert_track({"file_path": "/a.mp3", "title": "A"})
        db.upsert_track({"file_path": "/b.mp3", "title": "B"})
        db.commit()
        a = db.get_track_by_path("/a.mp3")["id"]
        b = db.get_track_by_path("/b.mp3")["id"]
        db.add_tracks_to_playlist(first, [a, b])
        db.add_tracks_to_playlist(second, [a])
        db.commit()

        counts = db.get_playlist_track_counts([first, second])

        assert counts[first] == 2
        assert counts[second] == 1

    def test_init_schema_is_noop_for_current_readonly_db(self, config):
        db = Database(config.db_path)
        db.commit()
        db.close()

        conn = sqlite3.connect(f"file:{config.db_path}?mode=ro", uri=True, timeout=5.0, isolation_level=None)
        conn.row_factory = sqlite3.Row
        try:
            Database._init_schema_on_connection(conn)
            row = conn.execute("SELECT version FROM schema_version LIMIT 1").fetchone()
            assert row["version"] == SCHEMA_VERSION
        finally:
            conn.close()

    def test_init_schema_migrates_existing_v7_db_with_old_tracks_table(self, tmp_dir):
        db_path = os.path.join(tmp_dir, "legacy-v7.db")
        conn = sqlite3.connect(db_path, timeout=5.0, isolation_level=None)
        conn.row_factory = sqlite3.Row
        try:
            conn.executescript(
                """
                CREATE TABLE schema_version (version INTEGER PRIMARY KEY);
                INSERT INTO schema_version (version) VALUES (7);
                CREATE TABLE tracks (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    media_type TEXT DEFAULT 'audio',
                    file_path TEXT UNIQUE NOT NULL,
                    file_hash TEXT,
                    file_size INTEGER,
                    last_modified REAL,
                    title TEXT DEFAULT '',
                    artist TEXT DEFAULT '',
                    album TEXT DEFAULT '',
                    album_artist TEXT DEFAULT '',
                    genre TEXT DEFAULT '',
                    year INTEGER,
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
                    date_added TEXT,
                    synced_to_device INTEGER DEFAULT 0,
                    device_path TEXT,
                    metadata_hash TEXT DEFAULT '',
                    artwork_hash TEXT DEFAULT '',
                    last_synced_metadata_hash TEXT DEFAULT '',
                    last_synced_file_hash TEXT DEFAULT ''
                );
                """
            )
            Database._init_schema_on_connection(conn)
            cols = {
                row["name"] for row in conn.execute("PRAGMA table_info(tracks)")
            }
            version = conn.execute("SELECT version FROM schema_version LIMIT 1").fetchone()["version"]
        finally:
            conn.close()

        assert version == SCHEMA_VERSION
        assert "video_kind" in cols
        assert "show_title" in cols
        assert "season_number" in cols
        assert "episode_number" in cols
        assert "video_hidden" in cols
        assert "video_locked" in cols

    def test_delete_tracks_not_in(self, db):
        db.upsert_track({"file_path": "/a.mp3", "title": "A"})
        db.upsert_track({"file_path": "/b.mp3", "title": "B"})
        db.upsert_track({"file_path": "/c.mp3", "title": "C"})
        db.commit()

        db.delete_tracks_not_in({"/a.mp3", "/c.mp3"})
        db.commit()

        assert db.get_track_count() == 2
        assert db.get_track_by_path("/b.mp3") is None

    def test_device_track_operations(self, db):
        db.upsert_device_track({
            "device_path": "Music/Artist/Album/song.mp3",
            "title": "Song",
            "artist": "Artist",
            "metadata_hash": "devhash",
        })
        db.commit()

        tracks = db.get_all_device_tracks()
        assert len(tracks) == 1
        assert tracks[0]["metadata_hash"] == "devhash"

        db.clear_device_tracks()
        db.commit()
        assert len(db.get_all_device_tracks()) == 0

    def test_sync_status_is_scoped_to_selected_device(self, db):
        db.upsert_track({
            "file_path": "/music/shared.mp3",
            "title": "Shared",
            "media_type": "audio",
            "metadata_hash": "new-meta",
            "file_hash": "same-file",
        })
        db.commit()
        track_id = db.get_track_by_path("/music/shared.mp3")["id"]
        for device_id, baseline in (("ipod-a", "old-meta"), ("ipod-b", "new-meta")):
            db.upsert_device_track({
                "device_id": device_id,
                "device_path": f"Music/{device_id}/shared.mp3",
                "local_track_id": track_id,
                "metadata_hash": baseline,
                "file_hash": "same-file",
                "last_synced_metadata_hash": baseline,
                "last_synced_file_hash": "same-file",
            })
        db.commit()

        assert db.get_sync_status_counts("ipod-a")["resync"] == 1
        assert db.get_sync_status_counts("ipod-b")["resync"] == 0

    def test_schema_10_migrates_device_sync_baselines(self, tmp_dir):
        db_path = os.path.join(tmp_dir, "schema10.db")
        conn = sqlite3.connect(db_path, timeout=5.0, isolation_level=None)
        conn.row_factory = sqlite3.Row
        try:
            conn.executescript(
                """
                CREATE TABLE schema_version (version INTEGER PRIMARY KEY);
                INSERT INTO schema_version (version) VALUES (10);
                CREATE TABLE device_tracks (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    device_id TEXT DEFAULT '',
                    device_path TEXT NOT NULL,
                    local_track_id INTEGER,
                    metadata_hash TEXT DEFAULT '',
                    file_hash TEXT DEFAULT '',
                    present_on_device INTEGER DEFAULT 1
                );
                INSERT INTO device_tracks (
                    device_id, device_path, local_track_id, metadata_hash, file_hash
                ) VALUES ('ipod-a', 'Music/a.mp3', 4, 'meta-a', 'file-a');
                """
            )
            Database._init_schema_on_connection(conn)
            row = conn.execute(
                "SELECT last_synced_metadata_hash, last_synced_file_hash FROM device_tracks"
            ).fetchone()
            version = conn.execute("SELECT version FROM schema_version").fetchone()["version"]
        finally:
            conn.close()

        assert version == SCHEMA_VERSION
        assert row["last_synced_metadata_hash"] == "meta-a"
        assert row["last_synced_file_hash"] == "file-a"


# ===========================================================================
# Regression: threaded scan does not use main-thread DB connection
# ===========================================================================

class TestScannerThreadSafety:
    """Verify that LibraryScanner runs scans on a worker thread with its own
    DB connection, and that completion signals arrive on the main thread so
    the caller can safely read the main-thread DB afterward.
    """

    def test_scan_finished_signal_delivers_on_main_thread(self, config, db):
        """Run a real async scan via QThread and verify we receive the
        scan_finished signal with correct data, proving the queued-connection
        pipeline works end-to-end.
        """
        import threading
        from PySide6.QtCore import QCoreApplication, QTimer, QEventLoop
        from PySide6.QtWidgets import QApplication

        # Ensure we have a QApplication
        app = QApplication.instance()
        if app is None:
            app = QApplication([])

        from services.library_scanner import LibraryScanner

        scanner = LibraryScanner(db, config)

        results = {}
        thread_ids = {}

        main_thread_id = threading.current_thread().ident

        def on_finished(total, elapsed):
            results["total"] = total
            results["elapsed"] = elapsed
            thread_ids["callback_thread"] = threading.current_thread().ident

        scanner.scan_finished.connect(on_finished)

        def on_error(msg):
            results["error"] = msg

        scanner.scan_error.connect(on_error)

        # Start the scan (the music dir has no valid audio files, so it
        # will complete quickly with 0 tracks or errors from fake files)
        scanner.start_scan()

        # Process events until the scan finishes (timeout after 10s)
        loop = QEventLoop()
        scanner.scan_finished.connect(loop.quit)
        scanner.scan_error.connect(loop.quit)
        QTimer.singleShot(10000, loop.quit)
        loop.exec()

        # The scan should have completed (possibly with 0 tracks since
        # the sample files are not real audio, but the signal must fire)
        assert "total" in results or "error" in results, (
            "Neither scan_finished nor scan_error was received"
        )

        # If finished, verify the callback ran on the main thread
        if "total" in results:
            assert thread_ids["callback_thread"] == main_thread_id, (
                "scan_finished callback ran on wrong thread"
            )
            # The main-thread DB should be readable after scan completion
            count = db.get_track_count()
            assert isinstance(count, int)


class TestIncrementalLibraryRefresh:
    """Regression coverage for cache-first startup and incremental refresh."""

    def _write_file(self, path, data=b"audio"):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data)
        return os.stat(path)

    def _track_for_file(self, path, title="Song"):
        stat = os.stat(path)
        track = Track(
            file_path=path,
            title=title,
            artist="Artist",
            album="Album",
            file_size=stat.st_size,
            last_modified=stat.st_mtime,
            metadata_hash=f"meta-{title}",
        )
        return track

    def test_existing_db_loads_ui_without_startup_scan(self, config, db, monkeypatch):
        from PySide6.QtCore import QTimer
        from PySide6.QtWidgets import QApplication

        from services.device_detector import DeviceDetector
        from services.library_scanner import LibraryScanner
        from ui.main_window import MainWindow

        app = QApplication.instance() or QApplication([])

        db.upsert_track({
            "file_path": "/music/cached.mp3",
            "title": "Cached Song",
            "artist": "Cached Artist",
            "album": "Cached Album",
            "file_size": 123,
            "last_modified": 456.0,
            "metadata_hash": "cached-meta",
        })
        db.commit()
        db.close()

        start_calls = []
        monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
        monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
        monkeypatch.setattr(
            LibraryScanner,
            "start_scan",
            lambda self, force_full=False: start_calls.append(force_full),
        )

        window = MainWindow(config)
        try:
            assert window._track_model.rowCount() == 1
            assert start_calls == []
        finally:
            window.close()

    def test_unchanged_files_do_not_reread_metadata(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        path = os.path.join(config.music_dir, "Artist", "Album", "same.mp3")
        stat = self._write_file(path, b"same")
        db.upsert_track({
            "file_path": path,
            "title": "Same",
            "file_size": stat.st_size,
            "last_modified": stat.st_mtime,
            "metadata_hash": "same-meta",
        })
        db.commit()

        def fail_read_metadata(filepath):
            raise AssertionError(f"Unexpected metadata read for {filepath}")

        monkeypatch.setattr(library_scanner, "read_metadata", fail_read_metadata)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 0
        assert db.get_track_count() == 1

    def test_incremental_detects_new_removed_and_changed_files(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        unchanged = os.path.join(config.music_dir, "Artist", "Album", "unchanged.mp3")
        changed = os.path.join(config.music_dir, "Artist", "Album", "changed.mp3")
        removed = os.path.join(config.music_dir, "Artist", "Album", "removed.mp3")
        new = os.path.join(config.music_dir, "Artist", "Album", "new.mp3")

        unchanged_stat = self._write_file(unchanged, b"unchanged")
        changed_old_stat = self._write_file(changed, b"old")
        removed_stat = self._write_file(removed, b"removed")

        db.upsert_track({
            "file_path": unchanged,
            "title": "Unchanged",
            "file_size": unchanged_stat.st_size,
            "last_modified": unchanged_stat.st_mtime,
            "metadata_hash": "unchanged-meta",
        })
        db.upsert_track({
            "file_path": changed,
            "title": "Changed Old",
            "file_size": changed_old_stat.st_size,
            "last_modified": changed_old_stat.st_mtime,
            "metadata_hash": "changed-old-meta",
        })
        db.upsert_track({
            "file_path": removed,
            "title": "Removed",
            "file_size": removed_stat.st_size,
            "last_modified": removed_stat.st_mtime,
            "metadata_hash": "removed-meta",
        })
        db.commit()

        os.remove(removed)
        changed_stat = self._write_file(changed, b"changed-content")
        self._write_file(new, b"new")

        reads = []

        def fake_read_metadata(filepath):
            reads.append(filepath)
            title = "Changed New" if filepath == changed else "New"
            return self._track_for_file(filepath, title=title)

        monkeypatch.setattr(library_scanner, "read_metadata", fake_read_metadata)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 3

        assert set(reads) == {changed, new}
        assert db.get_track_by_path(unchanged)["title"] == "Unchanged"
        assert db.get_track_by_path(changed)["title"] == "Changed New"
        assert db.get_track_by_path(changed)["file_size"] == changed_stat.st_size
        assert db.get_track_by_path(new)["title"] == "New"
        assert db.get_track_by_path(removed) is None

    def test_hashes_only_computed_for_changed_or_new_files_when_strict(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        config.duplicate_strictness = "metadata_and_hash"
        unchanged = os.path.join(config.music_dir, "unchanged.mp3")
        new = os.path.join(config.music_dir, "new.mp3")
        unchanged_stat = self._write_file(unchanged, b"unchanged")
        self._write_file(new, b"new")

        db.upsert_track({
            "file_path": unchanged,
            "title": "Unchanged",
            "file_size": unchanged_stat.st_size,
            "last_modified": unchanged_stat.st_mtime,
            "metadata_hash": "unchanged-meta",
            "file_hash": "existing-file-hash",
        })
        db.commit()

        hash_calls = []

        monkeypatch.setattr(
            library_scanner,
            "read_metadata",
            lambda filepath: self._track_for_file(filepath, title="New"),
        )

        def fake_compute_file_hash(filepath):
            hash_calls.append(filepath)
            return "new-file-hash"

        monkeypatch.setattr(library_scanner, "compute_file_hash", fake_compute_file_hash)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1

        assert hash_calls == [new]
        assert db.get_track_by_path(unchanged)["file_hash"] == "existing-file-hash"
        assert db.get_track_by_path(new)["file_hash"] == "new-file-hash"

    def test_scan_report_tracks_skips_and_folder_diagnostics(self, config, db):
        from services.library_scanner import LibraryScanner

        artist_dir = os.path.join(config.music_dir, "Artist", "Album")
        os.makedirs(artist_dir, exist_ok=True)
        with open(os.path.join(artist_dir, "song.mp3"), "wb") as f:
            f.write(b"audio")
        with open(os.path.join(artist_dir, "cover.jpg"), "wb") as f:
            f.write(b"image")

        scanner = LibraryScanner(db, config)
        scanner.scan_sync()
        report = scanner.last_report

        assert report["total_audio_files_found"] == 1
        assert report["total_files_skipped"] == 0
        assert report["total_non_audio_files_ignored"] >= 1
        assert any(item["reason"] == "unsupported extension" for item in report["skipped_files"])
        assert any(item["folder"].endswith(os.path.join("Artist", "Album")) for item in report["folder_stats"])

    def test_parser_failure_on_one_file_does_not_abort_album_scan(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        album_dir = os.path.join(config.music_dir, "Artist", "Album")
        bad = os.path.join(album_dir, "01 - Bad.mp3")
        good = os.path.join(album_dir, "02 - Good.mp3")
        self._write_file(bad, b"bad")
        self._write_file(good, b"good")

        def fake_read_metadata_details(filepath):
            if filepath == bad:
                raise ValueError("boom")
            return self._track_for_file(filepath, title="Good"), {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1
        report = scanner.last_report

        assert db.get_track_by_path(good) is not None
        assert db.get_track_by_path(bad) is None
        assert any(item["path"] == bad for item in report["skipped_files"])
        assert any(path.endswith(os.path.join("Artist", "Album")) for path in report["partial_import_folders"])

    def test_duplicate_looking_tracks_are_not_collapsed(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        a = os.path.join(config.music_dir, "Artist", "Album", "01 - Same.mp3")
        b = os.path.join(config.music_dir, "Artist", "Album", "02 - Same.mp3")
        self._write_file(a, b"a")
        self._write_file(b, b"b")

        def fake_read_metadata_details(filepath):
            track = Track(
                file_path=filepath,
                title="Same",
                artist="Artist",
                album="Album",
                album_artist="Artist",
                duration=120.0,
                file_size=os.path.getsize(filepath),
                last_modified=os.stat(filepath).st_mtime,
                metadata_hash="same-meta",
            )
            return track, {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)
        scanner.scan_sync()

        assert db.get_track_count() == 2
        assert db.get_track_by_path(a) is not None
        assert db.get_track_by_path(b) is not None

    def test_reconciliation_report_finds_missing_db_files(self, config, db):
        from services.library_scanner import build_library_reconciliation_report

        on_disk = os.path.join(config.music_dir, "Artist", "Album", "disk_only.mp3")
        in_db = os.path.join(config.music_dir, "Artist", "Album", "db_only.mp3")
        self._write_file(on_disk, b"audio")
        db.upsert_track({
            "file_path": in_db,
            "title": "DB Only",
            "artist": "Artist",
            "album": "Album",
            "file_size": 10,
            "metadata_hash": "db-only",
        })
        db.commit()

        report = build_library_reconciliation_report(db, config.music_dir, sample_limit=10)

        assert report["disk_audio_file_count"] == 1
        assert report["files_missing_from_db_count"] == 1
        assert on_disk in report["files_missing_from_db"]
        assert report["stale_db_paths_count"] == 1
        assert in_db in report["stale_db_paths"]

    def test_scan_sync_indexes_video_library_without_affecting_audio_defaults(self, config, db):
        from services.library_scanner import LibraryScanner

        video_path = os.path.join(config.video_dir, "Movies", "clip.mp4")
        os.makedirs(os.path.dirname(video_path), exist_ok=True)
        with open(video_path, "wb") as f:
            f.write(b"video")

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1

        report = scanner.last_report
        row = db.get_track_by_path(video_path)

        assert report["total_video_files_found"] == 1
        assert report["total_media_files_found"] == 1
        assert row is not None
        assert row["media_type"] == "video"
        assert db.get_track_count() == 0
        assert db.get_track_count(media_type=None) == 1

    def test_scan_sync_indexes_multiple_video_roots(self, config, db):
        from services.library_scanner import LibraryScanner

        videos_a = os.path.join(os.path.dirname(config.video_dir), "Videos A")
        videos_b = os.path.join(os.path.dirname(config.video_dir), "Videos B")
        config.video_dirs = [videos_a, videos_b]
        for root in config.video_dirs:
            os.makedirs(root, exist_ok=True)

        path_a = os.path.join(videos_a, "Movies", "clip-a.mp4")
        path_b = os.path.join(videos_b, "Shows", "clip-b.mkv")
        os.makedirs(os.path.dirname(path_a), exist_ok=True)
        os.makedirs(os.path.dirname(path_b), exist_ok=True)
        with open(path_a, "wb") as f:
            f.write(b"video-a")
        with open(path_b, "wb") as f:
            f.write(b"video-b")

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 2

        report = scanner.last_report
        assert report["total_video_files_found"] == 2
        assert report["video_dirs"] == config.video_dirs
        assert db.get_track_by_path(path_a) is not None
        assert db.get_track_by_path(path_b) is not None

    def test_legacy_video_rows_without_derived_metadata_are_reread(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        video_path = os.path.join(config.video_dir, "The Show", "Season 1", "S01E02 - Pilot.mkv")
        stat = self._write_file(video_path, b"video")
        db.upsert_track({
            "file_path": video_path,
            "title": "Pilot",
            "media_type": "video",
            "file_size": stat.st_size,
            "last_modified": stat.st_mtime,
            "metadata_hash": "legacy-video",
            "video_kind": "",
            "show_title": "",
        })
        db.commit()

        def fake_read_metadata_details(filepath):
            track = Track(
                file_path=filepath,
                media_type="video",
                title="Pilot",
                show_title="The Show",
                season_number=1,
                episode_number=2,
                track_number=2,
                album="Season 1",
                album_artist="The Show",
                video_kind="show",
                file_size=os.path.getsize(filepath),
                last_modified=os.stat(filepath).st_mtime,
                metadata_hash="new-video-meta",
            )
            return track, {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1

        row = db.get_track_by_path(video_path)
        assert row["video_kind"] == "show"
        assert row["show_title"] == "The Show"
        assert row["season_number"] == 1
        assert row["episode_number"] == 2

    def test_video_rows_cached_as_audio_are_reread_and_reclassified(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        video_path = os.path.join(config.video_dir, "TV Shows", "The Show", "Season 1", "S01E02 - Pilot.mkv")
        stat = self._write_file(video_path, b"video")
        db.upsert_track({
            "file_path": video_path,
            "title": "Pilot",
            "artist": "Unknown Artist",
            "album": "Unknown Album",
            "media_type": "audio",
            "file_size": stat.st_size,
            "last_modified": stat.st_mtime,
            "metadata_hash": "legacy-audio-video",
        })
        db.commit()

        def fake_read_metadata_details(filepath):
            track = Track(
                file_path=filepath,
                media_type="video",
                title="Pilot",
                show_title="The Show",
                season_number=1,
                episode_number=2,
                track_number=2,
                album="Season 1",
                album_artist="The Show",
                artist="The Show",
                video_kind="show",
                file_size=os.path.getsize(filepath),
                last_modified=os.stat(filepath).st_mtime,
                metadata_hash="fixed-video-meta",
            )
            return track, {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1

        row = db.get_track_by_path(video_path)
        assert row["media_type"] == "video"
        assert row["video_kind"] == "show"
        assert row["show_title"] == "The Show"
        assert row["season_number"] == 1
        assert row["episode_number"] == 2

    def test_legacy_video_rows_in_tv_folders_are_reread_when_misclassified_as_movie(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        video_path = os.path.join(config.video_dir, "TV Shows", "Death Note", "01 - Rebirth.mkv")
        stat = self._write_file(video_path, b"video")
        db.upsert_track({
            "file_path": video_path,
            "title": "Rebirth",
            "media_type": "video",
            "video_kind": "movie",
            "show_title": "",
            "file_size": stat.st_size,
            "last_modified": stat.st_mtime,
            "metadata_hash": "legacy-movie",
        })
        db.commit()

        def fake_read_metadata_details(filepath):
            track = Track(
                file_path=filepath,
                media_type="video",
                title="Rebirth",
                show_title="Death Note",
                episode_number=1,
                track_number=1,
                album_artist="Death Note",
                video_kind="show",
                file_size=os.path.getsize(filepath),
                last_modified=os.stat(filepath).st_mtime,
                metadata_hash="death-note-meta",
            )
            return track, {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1

        row = db.get_track_by_path(video_path)
        assert row["video_kind"] == "show"
        assert row["show_title"] == "Death Note"
        assert row["track_number"] == 1

    def test_legacy_video_rows_with_specials_show_title_are_reread(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        video_path = os.path.join(config.video_dir, "TV Shows", "Death Note", "Specials", "01 - Special.mkv")
        stat = self._write_file(video_path, b"video")
        db.upsert_track({
            "file_path": video_path,
            "title": "Special",
            "media_type": "video",
            "video_kind": "show",
            "show_title": "Specials",
            "file_size": stat.st_size,
            "last_modified": stat.st_mtime,
            "metadata_hash": "legacy-specials",
        })
        db.commit()

        def fake_read_metadata_details(filepath):
            track = Track(
                file_path=filepath,
                media_type="video",
                title="Special",
                show_title="Death Note",
                album="Specials",
                episode_number=1,
                track_number=1,
                album_artist="Death Note",
                video_kind="show",
                file_size=os.path.getsize(filepath),
                last_modified=os.stat(filepath).st_mtime,
                metadata_hash="death-note-special",
            )
            return track, {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1

        row = db.get_track_by_path(video_path)
        assert row["show_title"] == "Death Note"
        assert row["album"] == "Specials"

    def test_legacy_video_rows_with_noisy_archive_metadata_are_reread(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        video_path = os.path.join(
            config.video_dir,
            "TV Shows",
            "tales-from-the-cryptkeeper-full-series",
            "S01_E01 - While the Cat's Away-quicktime.mov",
        )
        stat = self._write_file(video_path, b"video")
        db.upsert_track({
            "file_path": video_path,
            "title": "S01 E01 - While the Cat's Away-quicktime",
            "artist": "tales-from-the-cryptkeeper-full-series",
            "album": "Season 1",
            "show_title": "tales-from-the-cryptkeeper-full-series",
            "season_number": 1,
            "episode_number": None,
            "media_type": "video",
            "video_kind": "show",
            "file_size": stat.st_size,
            "last_modified": stat.st_mtime,
            "metadata_hash": "legacy-noisy-video",
        })
        db.commit()

        def fake_read_metadata_details(filepath):
            track = Track(
                file_path=filepath,
                media_type="video",
                title="While the Cat's Away",
                show_title="tales from the cryptkeeper",
                album="Season 1",
                artist="tales from the cryptkeeper",
                album_artist="tales from the cryptkeeper",
                season_number=1,
                episode_number=1,
                track_number=1,
                video_kind="show",
                file_size=os.path.getsize(filepath),
                last_modified=os.stat(filepath).st_mtime,
                metadata_hash="clean-video-meta",
            )
            return track, {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1

        row = db.get_track_by_path(video_path)
        assert row["title"] == "While the Cat's Away"
        assert row["show_title"] == "tales from the cryptkeeper"
        assert row["episode_number"] == 1

    def test_legacy_video_rows_with_year_suffixes_are_reread(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        video_path = os.path.join(config.video_dir, "TV Shows", "6teen (2004)", "Season 1", "S01E01 - Pilot.mp4")
        stat = self._write_file(video_path, b"video")
        db.upsert_track({
            "file_path": video_path,
            "title": "Pilot",
            "artist": "6teen (2004)",
            "album_artist": "6teen (2004)",
            "album": "Season 1",
            "show_title": "6teen (2004)",
            "year": 2004,
            "season_number": 1,
            "episode_number": 1,
            "media_type": "video",
            "video_kind": "show",
            "file_size": stat.st_size,
            "last_modified": stat.st_mtime,
            "metadata_hash": "legacy-year-video",
        })
        db.commit()

        def fake_read_metadata_details(filepath):
            track = Track(
                file_path=filepath,
                media_type="video",
                title="Pilot",
                artist="6teen",
                album_artist="6teen",
                album="Season 1",
                show_title="6teen",
                year=2004,
                season_number=1,
                episode_number=1,
                track_number=1,
                video_kind="show",
                file_size=os.path.getsize(filepath),
                last_modified=os.stat(filepath).st_mtime,
                metadata_hash="clean-year-video",
            )
            return track, {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1

        row = db.get_track_by_path(video_path)
        assert row["show_title"] == "6teen"
        assert row["artist"] == "6teen"
        assert row["album_artist"] == "6teen"

    def test_legacy_archive_episode_rows_with_blank_show_title_are_reread(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        video_path = os.path.join(
            config.video_dir,
            "_Archive",
            "6teen_2004_complete_series_202508",
            "6teen S01E01 Take This Job and Squeeze It.mkv",
        )
        stat = self._write_file(video_path, b"video")
        db.upsert_track({
            "file_path": video_path,
            "title": "6teen S01E01 Take This Job and Squeeze It",
            "artist": "",
            "album_artist": "",
            "album": "",
            "show_title": "",
            "year": None,
            "season_number": None,
            "episode_number": None,
            "media_type": "video",
            "video_kind": "",
            "file_size": stat.st_size,
            "last_modified": stat.st_mtime,
            "metadata_hash": "legacy-archive-video",
        })
        db.commit()

        def fake_read_metadata_details(filepath):
            track = Track(
                file_path=filepath,
                media_type="video",
                title="Take This Job and Squeeze It",
                artist="6teen",
                album_artist="6teen",
                album="Season 1",
                show_title="6teen",
                year=2004,
                season_number=1,
                episode_number=1,
                track_number=1,
                video_kind="show",
                file_size=os.path.getsize(filepath),
                last_modified=os.stat(filepath).st_mtime,
                metadata_hash="clean-archive-video",
            )
            return track, {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1

        row = db.get_track_by_path(video_path)
        assert row["show_title"] == "6teen"
        assert row["title"] == "Take This Job and Squeeze It"
        assert row["season_number"] == 1
        assert row["episode_number"] == 1

    def test_legacy_release_noisy_movie_rows_are_reread(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        video_path = os.path.join(
            config.video_dir,
            "Movies",
            "Flow.2024.2160p.4K.WEB.x265.10bit.AAC5.1.mkv",
        )
        stat = self._write_file(video_path, b"video")
        db.upsert_track({
            "file_path": video_path,
            "title": "Flow.2024.2160p.4K.WEB.x265.10bit.AAC5.1",
            "artist": "",
            "album_artist": "",
            "album": "",
            "show_title": "",
            "year": 2024,
            "season_number": None,
            "episode_number": None,
            "media_type": "video",
            "video_kind": "movie",
            "file_size": stat.st_size,
            "last_modified": stat.st_mtime,
            "metadata_hash": "legacy-release-noise-video",
        })
        db.commit()

        def fake_read_metadata_details(filepath):
            track = Track(
                file_path=filepath,
                media_type="video",
                title="Flow",
                artist="",
                album_artist="",
                album="Flow",
                show_title="",
                year=2024,
                season_number=None,
                episode_number=None,
                video_kind="movie",
                file_size=os.path.getsize(filepath),
                last_modified=os.stat(filepath).st_mtime,
                metadata_hash="clean-release-noise-video",
            )
            return track, {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)
        assert scanner.scan_sync() == 1

        row = db.get_track_by_path(video_path)
        assert row["title"] == "Flow"
        assert row["album"] == "Flow"

    def test_transaction_failure_preserves_last_good_library(self, config, db, monkeypatch):
        from services import library_scanner
        from services.library_scanner import LibraryScanner

        existing = os.path.join(config.music_dir, "Artist", "Album", "existing.mp3")
        incoming = os.path.join(config.music_dir, "Artist", "Album", "incoming.mp3")
        self._write_file(existing, b"existing")
        self._write_file(incoming, b"incoming")

        db.upsert_track({
            "file_path": existing,
            "title": "Existing",
            "artist": "Artist",
            "album": "Album",
            "file_size": os.path.getsize(existing),
            "last_modified": os.stat(existing).st_mtime,
            "metadata_hash": "existing-meta",
        })
        db.commit()

        def fake_read_metadata_details(filepath):
            title = "Existing" if filepath == existing else "Incoming"
            return self._track_for_file(filepath, title=title), {"parsed_ok": True, "warnings": []}

        monkeypatch.setattr(library_scanner, "read_metadata_details", fake_read_metadata_details)

        scanner = LibraryScanner(db, config)

        class BrokenTransaction:
            def __enter__(self):
                raise RuntimeError("database is locked")

            def __exit__(self, exc_type, exc, tb):
                return False

        monkeypatch.setattr(db, "transaction", lambda: BrokenTransaction())

        try:
            scanner.scan_sync()
            assert False, "scan_sync should have raised"
        except RuntimeError as exc:
            assert "database is locked" in str(exc)

        assert db.get_track_count() == 1
        assert db.get_track_by_path(existing)["title"] == "Existing"
        assert db.get_track_by_path(incoming) is None
        assert scanner.last_report["status"] == "failed"
        assert "database is locked" in scanner.last_report["failure_reason"]
