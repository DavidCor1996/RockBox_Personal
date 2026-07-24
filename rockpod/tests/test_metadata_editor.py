"""Tests for the Get Info metadata editor."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication, QMessageBox

from services.device_detector import DeviceDetector
from services.library_scanner import LibraryScanner
from models.track import compute_metadata_hash
from models.track import Track
from ui.dialogs.metadata_editor import MetadataEditor
from ui.main_window import MainWindow


def test_metadata_editor_emits_new_audio_library_fields():
    app = QApplication.instance() or QApplication([])
    track = {
        "id": 7,
        "media_type": "audio",
        "title": "Song",
        "artist": "Artist",
        "album": "Album",
        "album_artist": "Artist",
        "genre": "Rock",
        "year": 2020,
        "track_number": 1,
        "disc_number": 1,
        "duration": 240.0,
        "bitrate": 320,
        "codec": "MP3",
        "play_count": 2,
        "last_played": "2026-04-01 10:00:00",
        "date_added": "2026-03-01 09:30:00",
    }
    editor = MetadataEditor(track)
    saved = []
    editor.metadata_saved.connect(lambda track_id, updates: saved.append((track_id, updates)))

    editor._play_count_spin.setValue(9)
    editor._last_played_edit.setText("2026-04-20 22:15:00")
    editor._date_added_edit.setText("2026-02-14 08:45:00")
    editor._on_ok()

    assert saved == [
        (
            7,
            {
                "play_count": 9,
                "last_played": "2026-04-20 22:15:00",
                "date_added": "2026-02-14 08:45:00",
            },
        )
    ]


def test_metadata_editor_offers_and_saves_music_video_type():
    app = QApplication.instance() or QApplication([])
    track = {
        "id": 8,
        "media_type": "video",
        "video_kind": "movie",
        "title": "Performance",
        "disc_number": 1,
    }
    editor = MetadataEditor(track)
    saved = []
    editor.metadata_saved.connect(lambda track_id, updates: saved.append((track_id, updates)))

    music_video_index = editor._video_kind_combo.findData("music_video")
    assert music_video_index >= 0
    assert editor._video_kind_combo.itemText(music_video_index) == "Music Video"

    editor._video_kind_combo.setCurrentIndex(music_video_index)
    editor._on_ok()

    assert saved == [(8, {"video_kind": "music_video"})]


def test_metadata_editor_partial_video_edit_preserves_title_and_type():
    app = QApplication.instance() or QApplication([])
    track = {
        "id": 9,
        "media_type": "video",
        "video_kind": "music_video",
        "title": "Custom Title",
        "comment": "Old comment",
        "disc_number": 1,
    }
    editor = MetadataEditor(track)
    saved = []
    editor.metadata_saved.connect(lambda track_id, updates: saved.append((track_id, updates)))

    editor._comment_edit.setText("New comment")
    editor._on_ok()

    assert editor._video_kind_combo.currentData() == "music_video"
    assert saved == [(9, {"comment": "New comment"})]


def test_update_track_metadata_recomputes_hash_for_tag_changes(db):
    old_hash = compute_metadata_hash(
        "Song", "Artist", "Album", "Artist", 1, 1, "Rock", 2020, "Composer", 240.0, 320, "MP3"
    )
    db.upsert_track(
        {
            "file_path": "/music/test.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "genre": "Rock",
            "year": 2020,
            "track_number": 1,
            "disc_number": 1,
            "composer": "Composer",
            "duration": 240.0,
            "bitrate": 320,
            "codec": "MP3",
            "metadata_hash": old_hash,
        }
    )
    db.commit()

    track_id = db.get_track_by_path("/music/test.mp3")["id"]
    db.update_track_metadata(track_id, {"title": "New Song"})
    db.commit()

    row = db.get_track_by_id(track_id)
    assert row["title"] == "New Song"
    assert row["metadata_hash"] == compute_metadata_hash(
        "New Song", "Artist", "Album", "Artist", 1, 1, "Rock", 2020, "Composer", 240.0, 320, "MP3"
    )
    assert row["metadata_hash"] != old_hash


def test_update_track_metadata_keeps_hash_for_play_stat_changes(db):
    metadata_hash = compute_metadata_hash(
        "Song", "Artist", "Album", "Artist", 1, 1, "Rock", 2020, "Composer", 240.0, 320, "MP3"
    )
    db.upsert_track(
        {
            "file_path": "/music/test.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "genre": "Rock",
            "year": 2020,
            "track_number": 1,
            "disc_number": 1,
            "composer": "Composer",
            "duration": 240.0,
            "bitrate": 320,
            "codec": "MP3",
            "metadata_hash": metadata_hash,
            "play_count": 1,
        }
    )
    db.commit()

    track_id = db.get_track_by_path("/music/test.mp3")["id"]
    db.update_track_metadata(track_id, {"play_count": 12, "last_played": "2026-04-21 12:00:00"})
    db.commit()

    row = db.get_track_by_id(track_id)
    assert row["play_count"] == 12
    assert row["last_played"] == "2026-04-21 12:00:00"
    assert row["metadata_hash"] == metadata_hash


def test_main_window_metadata_save_writes_file_backed_tags_then_refreshes_db(config, monkeypatch):
    app = QApplication.instance() or QApplication([])

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._db.upsert_track(
            {
                "file_path": "/music/test.mp3",
                "title": "Old Song",
                "artist": "Artist",
                "album": "Album",
                "album_artist": "Artist",
                "genre": "Rock",
                "year": 2020,
                "track_number": 1,
                "disc_number": 1,
                "duration": 240.0,
                "bitrate": 320,
                "codec": "MP3",
                "metadata_hash": compute_metadata_hash(
                    "Old Song", "Artist", "Album", "Artist", 1, 1, "Rock", 2020, "", 240.0, 320, "MP3"
                ),
                "file_hash": "old-file-hash",
                "play_count": 1,
            }
        )
        window._db.commit()
        track_id = window._db.get_track_by_path("/music/test.mp3")["id"]

        writes = []
        monkeypatch.setattr(
            "ui.main_window.write_track_metadata_to_file",
            lambda path, updates: writes.append((path, dict(updates))),
        )

        refreshed = Track(
            file_path="/music/test.mp3",
            title="New Song",
            artist="Artist",
            album="Album",
            album_artist="Artist",
            genre="Rock",
            year=2020,
            track_number=1,
            disc_number=1,
            duration=240.0,
            bitrate=320,
            codec="MP3",
        )
        refreshed.recompute_metadata_hash()
        refreshed.file_size = 1234
        refreshed.last_modified = 456.0

        monkeypatch.setattr(
            "ui.main_window.read_metadata_details",
            lambda path: (refreshed, {"parsed_ok": True, "warnings": []}),
        )
        monkeypatch.setattr("ui.main_window.compute_file_hash", lambda path: "new-file-hash")
        monkeypatch.setattr(window, "_refresh_view", lambda: None)

        window._on_metadata_saved(track_id, {"title": "New Song", "play_count": 5})

        row = window._db.get_track_by_id(track_id)
        assert writes == [("/music/test.mp3", {"title": "New Song"})]
        assert row["title"] == "New Song"
        assert row["play_count"] == 5
        assert row["file_hash"] == "new-file-hash"
        assert row["metadata_hash"] == refreshed.metadata_hash
        assert row["file_size"] == 1234
        assert row["last_modified"] == 456.0
    finally:
        window.close()


def test_metadata_file_refresh_preserves_library_video_type(monkeypatch):
    monkeypatch.setattr("ui.main_window.compute_file_hash", lambda path: "new-file-hash")
    existing = {
        "id": 9,
        "file_path": "/videos/performance.m4v",
        "media_type": "video",
        "video_kind": "music_video",
        "title": "Custom Title",
    }
    scanned = {
        "file_path": "/videos/performance.m4v",
        "media_type": "video",
        "video_kind": "movie",
        "title": "Custom Title",
        "comment": "Updated",
    }

    merged = MainWindow._merge_track_row_from_file(existing, scanned)

    assert merged["video_kind"] == "music_video"
    assert merged["title"] == "Custom Title"
    assert merged["comment"] == "Updated"


def test_main_window_bulk_metadata_backfill_processes_library_rows(config, monkeypatch):
    app = QApplication.instance() or QApplication([])

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    class _FakeProgressDialog:
        def __init__(self, *args, **kwargs):
            self._value = 0
            self._canceled = False

        def setWindowTitle(self, *_args):
            pass

        def setMinimumDuration(self, *_args):
            pass

        def setWindowModality(self, *_args):
            pass

        def setAutoClose(self, *_args):
            pass

        def setAutoReset(self, *_args):
            pass

        def show(self):
            pass

        def setValue(self, value):
            self._value = value

        def value(self):
            return self._value

        def setLabelText(self, *_args):
            pass

        def wasCanceled(self):
            return self._canceled

        def close(self):
            pass

        def deleteLater(self):
            pass

    window = MainWindow(config)
    try:
        window._db.upsert_track(
            {
                "file_path": "/music/ok.mp3",
                "title": "Song",
                "artist": "Artist",
                "album": "Album",
                "album_artist": "Artist",
                "genre": "Rock",
                "year": 2020,
                "track_number": 1,
                "disc_number": 1,
                "duration": 240.0,
                "bitrate": 320,
                "codec": "MP3",
                "metadata_hash": "mh1",
            }
        )
        window._db.upsert_track(
            {
                "file_path": "/music/nope.wav",
                "title": "Wave",
                "artist": "Artist",
                "album": "Album",
                "album_artist": "Artist",
                "genre": "Rock",
                "year": 2020,
                "track_number": 2,
                "disc_number": 1,
                "duration": 240.0,
                "bitrate": 320,
                "codec": "WAV",
                "metadata_hash": "mh2",
            }
        )
        window._db.commit()

        monkeypatch.setattr("ui.main_window.QProgressDialog", _FakeProgressDialog)
        monkeypatch.setattr("ui.main_window.QApplication.processEvents", lambda: None)
        monkeypatch.setattr("ui.main_window.QMessageBox.question", lambda *args, **kwargs: QMessageBox.Yes)

        writes = []

        def fake_rewrite(row, updates):
            writes.append((row["file_path"], dict(updates)))
            if row["file_path"].endswith(".wav"):
                raise Exception("unexpected")
            return dict(row)

        def fake_rewrite_checked(row, updates):
            writes.append((row["file_path"], dict(updates)))
            if row["file_path"].endswith(".wav"):
                from services.metadata_writer import MetadataWriteError
                raise MetadataWriteError("Writing tags for .wav is not supported yet")
            return dict(row)

        monkeypatch.setattr(window, "_rewrite_track_row_from_file", fake_rewrite_checked)
        monkeypatch.setattr(window, "_refresh_view", lambda: None)

        messages = []
        monkeypatch.setattr(
            "ui.main_window.QMessageBox.information",
            lambda _parent, title, text: messages.append((title, text)),
        )

        window._write_library_metadata_back_to_files()

        assert [path for path, _updates in writes] == ["/music/ok.mp3", "/music/nope.wav"]
        assert messages
        assert "Updated files: 1" in messages[-1][1]
        assert "Unsupported formats: 1" in messages[-1][1]
    finally:
        window.close()


def test_main_window_bulk_timed_lyrics_fetch_saves_and_mirrors_sidecars(config, monkeypatch):
    app = QApplication.instance() or QApplication([])

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    class _FakeProgressDialog:
        def __init__(self, *args, **kwargs):
            self._value = 0
            self._canceled = False

        def setWindowTitle(self, *_args):
            pass

        def setMinimumDuration(self, *_args):
            pass

        def setWindowModality(self, *_args):
            pass

        def setAutoClose(self, *_args):
            pass

        def setAutoReset(self, *_args):
            pass

        def show(self):
            pass

        def setValue(self, value):
            self._value = value

        def value(self):
            return self._value

        def setLabelText(self, *_args):
            pass

        def wasCanceled(self):
            return self._canceled

        def close(self):
            pass

        def deleteLater(self):
            pass

    window = MainWindow(config)
    try:
        local_dir = os.path.join(config.music_dir, "Artist", "Album")
        os.makedirs(local_dir, exist_ok=True)
        timed_path = os.path.join(local_dir, "Timed.mp3")
        line_path = os.path.join(local_dir, "Line.mp3")
        plain_path = os.path.join(local_dir, "Plain.mp3")
        with open(timed_path, "wb") as f:
            f.write(b"audio")
        with open(line_path, "wb") as f:
            f.write(b"audio")
        with open(plain_path, "wb") as f:
            f.write(b"audio")

        window._db.upsert_track(
            {
                "file_path": timed_path,
                "title": "Timed",
                "artist": "Artist",
                "album": "Album",
                "album_artist": "Artist",
                "genre": "Rock",
                "year": 2020,
                "track_number": 1,
                "disc_number": 1,
                "duration": 240.0,
                "bitrate": 320,
                "codec": "MP3",
                "metadata_hash": "mh1",
                "synced_to_device": 1,
                "device_path": "Music/Artist/Album/01 - Timed.mp3",
            }
        )
        window._db.upsert_track(
            {
                "file_path": line_path,
                "title": "Line",
                "artist": "Artist",
                "album": "Album",
                "album_artist": "Artist",
                "genre": "Rock",
                "year": 2020,
                "track_number": 2,
                "disc_number": 1,
                "duration": 240.0,
                "bitrate": 320,
                "codec": "MP3",
                "metadata_hash": "mh2",
            }
        )
        window._db.upsert_track(
            {
                "file_path": plain_path,
                "title": "Plain",
                "artist": "Artist",
                "album": "Album",
                "album_artist": "Artist",
                "genre": "Rock",
                "year": 2020,
                "track_number": 3,
                "disc_number": 1,
                "duration": 240.0,
                "bitrate": 320,
                "codec": "MP3",
                "metadata_hash": "mh3",
            }
        )
        window._db.commit()

        device_mount = config.mock_device_path
        os.makedirs(os.path.join(device_mount, "Music", "Artist", "Album"), exist_ok=True)
        window._device_detector._current_device = type("Device", (), {"mount_path": device_mount})()

        monkeypatch.setattr("ui.main_window.QProgressDialog", _FakeProgressDialog)
        monkeypatch.setattr("ui.main_window.QApplication.processEvents", lambda: None)
        monkeypatch.setattr("ui.main_window.QMessageBox.question", lambda *args, **kwargs: QMessageBox.Yes)

        def fake_fetch_detail(artist, title):
            if title == "Timed":
                return {
                    "lyrics": "[00:01.00]<00:01.10>Hello",
                    "timed": True,
                    "per_word": True,
                    "source": "lrclib",
                }
            if title == "Line":
                return {
                    "lyrics": "[00:01.00]Hello line timed world",
                    "timed": True,
                    "per_word": False,
                    "source": "lrclib",
                }
            return {
                "lyrics": "plain words only",
                "timed": False,
                "per_word": False,
                "source": "lyrics.ovh",
            }

        monkeypatch.setattr(window._album_metadata_fetcher._lookup_client, "fetch_track_lyrics_detail", fake_fetch_detail)

        messages = []
        monkeypatch.setattr(
            "ui.main_window.QMessageBox.information",
            lambda _parent, title, text: messages.append((title, text)),
        )

        window._fetch_library_timed_lyrics()

        local_sidecar = os.path.splitext(timed_path)[0] + ".lrc"
        line_sidecar = os.path.splitext(line_path)[0] + ".lrc"
        device_sidecar = os.path.join(device_mount, "Music", "Artist", "Album", "01 - Timed.lrc")
        plain_sidecar = os.path.splitext(plain_path)[0] + ".lrc"

        assert os.path.isfile(local_sidecar)
        assert os.path.isfile(line_sidecar)
        assert os.path.isfile(device_sidecar)
        assert not os.path.exists(plain_sidecar)
        with open(local_sidecar, "r", encoding="utf-8") as f:
            assert f.read() == "[00:01.00]<00:01.10>Hello"
        with open(line_sidecar, "r", encoding="utf-8") as f:
            assert f.read() == "[00:01.00]Hello line timed world"
        assert messages
        assert "Timed lyrics saved: 2" in messages[-1][1]
        assert "Per-word timed: 1" in messages[-1][1]
        assert "Line-timed fallback: 1" in messages[-1][1]
        assert "Mirrored to connected iPod: 1" in messages[-1][1]
        assert "Plain lyrics only (not saved): 1" in messages[-1][1]
    finally:
        window.close()
