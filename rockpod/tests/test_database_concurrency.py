"""SQLite initialization and concurrent access regression tests."""

import os
import threading

from app.database import Database
from services.library_scanner import LibraryScanWorker


def test_wal_and_busy_timeout_enabled(config):
    Database.reset_init_guard_for_tests()
    db = Database(config.db_path)
    try:
        journal = db.fetchone("PRAGMA journal_mode")["journal_mode"]
        timeout = db.fetchone("PRAGMA busy_timeout")["timeout"]
        assert journal.lower() == "wal"
        assert timeout == 5000
    finally:
        db.close()


def test_migration_runs_once_and_not_in_worker_thread(config):
    Database.reset_init_guard_for_tests()
    calls = []
    original = Database._migrate_schema_on_connection.__func__

    def counted(cls, conn, current_version=0):
        calls.append(threading.current_thread().name)
        return original(cls, conn, current_version)

    Database._migrate_schema_on_connection = classmethod(counted)
    try:
        db = Database(config.db_path)
        db.close()

        def open_worker_connection():
            worker_db = Database(config.db_path)
            worker_db.close()

        t = threading.Thread(target=open_worker_connection, name="worker-db")
        t.start()
        t.join()

        assert calls == ["MainThread"]
    finally:
        Database._migrate_schema_on_connection = classmethod(original)


def test_concurrent_reads_and_writes_do_not_lock(config):
    Database.reset_init_guard_for_tests()
    db = Database(config.db_path)
    errors = []

    def writer():
        try:
            worker_db = Database(config.db_path, initialize=False)
            with worker_db.write_lock():
                for i in range(100):
                    worker_db.upsert_track(
                        {
                            "file_path": f"/tmp/song-{i}.mp3",
                            "title": f"Song {i}",
                            "artist": "Artist",
                            "album": "Album",
                            "file_size": i,
                            "metadata_hash": f"hash-{i}",
                        }
                    )
            worker_db.close()
        except Exception as exc:
            errors.append(exc)

    t = threading.Thread(target=writer)
    t.start()
    while t.is_alive():
        db.get_track_count()
    t.join()

    try:
        assert not errors
        assert db.get_track_count() == 100
    finally:
        db.close()


def test_multiple_worker_scans_do_not_trigger_db_lock(config):
    Database.reset_init_guard_for_tests()
    db = Database(config.db_path)
    db.close()

    for idx in range(2):
        album_dir = os.path.join(config.music_dir, "Artist", f"Album {idx}")
        os.makedirs(album_dir, exist_ok=True)
        for track in range(5):
            with open(os.path.join(album_dir, f"{track}.mp3"), "wb") as f:
                f.write(b"not real audio")

    errors = []

    def run_worker():
        worker = LibraryScanWorker(
            config.music_dir,
            config.video_dir,
            config.db_path,
            compute_hashes=False,
            force_full=False,
        )
        worker.error.connect(errors.append)
        worker.run()

    threads = [threading.Thread(target=run_worker) for _ in range(2)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()

    db = Database(config.db_path)
    try:
        assert not errors
        assert db.get_track_count() == 10
    finally:
        db.close()
