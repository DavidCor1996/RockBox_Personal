"""Tests for persistent device inventory and optional theme assets."""

import json
import os

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

import services.theme_assets as theme_assets_module
from models.track import Track
from services.device_detector import DeviceInfo, create_mock_device
from services.device_inventory import device_music_roots, device_record_from_info, verify_device_inventory
from services.library_scanner import LibraryScanner
from services.rockbox_playlists import import_device_playlists
from services.reconciliation import build_reconciliation_report
from services.sync_engine import SyncEngine
from services.device_detector import DeviceDetector
from services.theme_assets import ThemeAssetManager
from ui.main_window import MainWindow


def _insert_local_track(db, title="Song", artist="Artist", album="Album", metadata_hash="mh"):
    db.upsert_track(
        {
            "file_path": f"/music/{artist}/{album}/{title}.mp3",
            "file_size": 100,
            "title": title,
            "artist": artist,
            "album": album,
            "album_artist": artist,
            "duration": 120.0,
            "track_number": 1,
            "disc_number": 1,
            "metadata_hash": metadata_hash,
        }
    )
    db.commit()
    return db.get_track_by_path(f"/music/{artist}/{album}/{title}.mp3")


def _device(path):
    create_mock_device(path)
    device = DeviceInfo(path)
    device.name = "Test iPod"
    device.is_rockbox = True
    return device


def test_device_record_and_tracks_persist_across_reopen(config, db):
    device = _device(config.mock_device_path)
    row = db.upsert_device(device_record_from_info(device))
    db.upsert_device_track(
        {
            "device_id": row["stable_device_key"],
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "file_size": 100,
            "metadata_hash": "mh",
        }
    )
    db.commit()

    reopened = type(db)(config.db_path)
    try:
        saved_device = reopened.get_device_by_key(row["stable_device_key"])
        saved_tracks = reopened.get_all_device_tracks(row["stable_device_key"])
        assert saved_device["display_name"] == "Test iPod"
        assert len(saved_tracks) == 1
        assert saved_tracks[0]["title"] == "Song"
    finally:
        reopened.close()


def test_device_info_normalizes_trailing_slash_mount_path(config):
    plain = DeviceInfo(config.mock_device_path)
    slashed = DeviceInfo(config.mock_device_path + "/")

    assert plain.mount_path == slashed.mount_path
    assert plain.name == slashed.name
    assert plain.stable_device_key == slashed.stable_device_key


def test_device_music_roots_dedupes_case_aliases(config, monkeypatch):
    device = _device(config.mock_device_path)
    mount_path = device.mount_path
    music_root = os.path.join(mount_path, "Music")
    real_isdir = os.path.isdir
    real_realpath = os.path.realpath
    real_stat = os.stat
    alias_paths = {
        device.music_path: music_root,
        os.path.join(mount_path, "Music"): music_root,
        os.path.join(mount_path, "MUSIC"): music_root,
        os.path.join(mount_path, "music"): music_root,
    }

    monkeypatch.setattr(
        "services.device_inventory.os.path.isdir",
        lambda path: path in alias_paths or real_isdir(path),
    )
    monkeypatch.setattr(
        "services.device_inventory.os.path.realpath",
        lambda path: alias_paths.get(path, real_realpath(path)),
    )

    class _FakeStat:
        st_dev = 1
        st_ino = 99

    monkeypatch.setattr(
        "services.device_inventory.os.stat",
        lambda path: _FakeStat() if path in alias_paths else real_stat(path),
    )

    roots = device_music_roots(device)

    assert roots == [music_root]


def test_multiple_devices_keep_independent_presence(config, db, tmp_dir):
    dev_a = _device(config.mock_device_path)
    dev_b_path = os.path.join(tmp_dir, "mock_ipod_b")
    dev_b = _device(dev_b_path)

    key_a = db.upsert_device(device_record_from_info(dev_a))["stable_device_key"]
    key_b = db.upsert_device(device_record_from_info(dev_b))["stable_device_key"]
    db.upsert_device_track(
        {
            "device_id": key_a,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "metadata_hash": "same",
        }
    )
    db.commit()

    assert len(db.get_all_device_tracks(key_a)) == 1
    assert len(db.get_all_device_tracks(key_b)) == 0


def test_cached_device_inventory_used_without_scan(config, db, monkeypatch):
    local = _insert_local_track(db, metadata_hash="same")
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": local["title"],
            "artist": local["artist"],
            "album": local["album"],
            "album_artist": local["album_artist"],
            "duration": local["duration"],
            "file_size": local["file_size"],
            "metadata_hash": "same",
        }
    )
    db.commit()

    detector = DeviceDetector(config)
    detector._current_device = device
    engine = SyncEngine(db, config, detector)
    engine.set_current_device(device)

    def fail_read(_path):
        raise AssertionError("cached load should not parse device files")

    monkeypatch.setattr("services.device_inventory.read_metadata", fail_read)
    assert len(engine.get_device_tracks()) == 1
    assert engine.get_not_on_device_tracks() == []


def test_incremental_verification_updates_new_and_missing(config, db, monkeypatch):
    device = _device(config.mock_device_path)
    full = os.path.join(config.mock_device_path, "Music", "Artist", "Album", "01 - Song.mp3")
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "wb") as f:
        f.write(b"audio")

    def fake_read_metadata(path):
        stat = os.stat(path)
        return Track(
            file_path=path,
            title="Song",
            artist="Artist",
            album="Album",
            album_artist="Artist",
            duration=120.0,
            file_size=stat.st_size,
            metadata_hash="mh",
        )

    summary = verify_device_inventory(
        db,
        device,
        "metadata_only",
        metadata_reader_func=fake_read_metadata,
    )
    assert summary["new"] == 1
    assert len(db.get_all_device_tracks(summary["device_key"])) == 1

    os.remove(full)
    summary = verify_device_inventory(
        db,
        device,
        "metadata_only",
        metadata_reader_func=fake_read_metadata,
    )
    assert summary["missing"] == 1
    assert len(db.get_all_device_tracks(summary["device_key"])) == 0
    assert len(db.get_all_device_tracks(summary["device_key"], present_only=False)) == 1


def test_noop_verification_skips_full_relink_when_cached_links_are_valid(config, db, monkeypatch):
    local = _insert_local_track(db, title="Song", artist="Artist", album="Album", metadata_hash="mh")
    device = _device(config.mock_device_path)
    full = os.path.join(config.mock_device_path, "Music", "Artist", "Album", "01 - Song.mp3")
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "wb") as f:
        f.write(b"audio")

    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "duration": 120.0,
            "file_size": os.path.getsize(full),
            "metadata_hash": "mh",
            "local_track_id": local["id"],
        }
    )
    db.commit()

    monkeypatch.setattr(
        "services.device_inventory._link_device_to_local",
        lambda *args, **kwargs: (_ for _ in ()).throw(AssertionError("noop verification should skip relink")),
    )
    monkeypatch.setattr(
        "services.device_inventory.read_metadata",
        lambda *args, **kwargs: (_ for _ in ()).throw(AssertionError("noop verification should reuse cached metadata")),
    )

    summary = verify_device_inventory(
        db,
        device,
        "metadata_only",
    )

    assert summary["scanned"] == 1
    assert summary["new"] == 0
    assert summary["changed"] == 0
    assert summary["missing"] == 0


def test_cached_not_on_device_tracks_still_work_after_disconnect(config, db):
    _insert_local_track(db, title="On Device", metadata_hash="same")
    _insert_local_track(db, title="Missing", metadata_hash="missing")
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - On Device.mp3",
            "title": "On Device",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "duration": 120.0,
            "file_size": 100,
            "metadata_hash": "same",
        }
    )
    db.commit()

    detector = DeviceDetector(config)
    detector._current_device = device
    engine = SyncEngine(db, config, detector)
    engine.set_current_device(device)

    detector._current_device = None
    missing = engine.get_not_on_device_tracks()

    assert [dict(row)["title"] for row in missing] == ["Missing"]


def test_not_on_device_query_path_does_not_use_matcher(config, db, monkeypatch):
    _insert_local_track(db, title="Missing")
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.commit()

    detector = DeviceDetector(config)
    detector._current_device = device
    engine = SyncEngine(db, config, detector)
    engine._current_device_key = key

    monkeypatch.setattr(
        "services.track_matcher.TrackMatcher.match_all",
        lambda *args, **kwargs: (_ for _ in ()).throw(AssertionError("should use SQL mapping path")),
    )
    missing = engine.get_not_on_device_tracks()

    assert [dict(row)["title"] for row in missing] == ["Missing"]


def test_device_views_load_from_cache_without_triggering_scan(config, db, monkeypatch):
    app = QApplication.instance() or QApplication([])
    local = _insert_local_track(db, metadata_hash="same")
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": local["title"],
            "artist": local["artist"],
            "album": local["album"],
            "album_artist": local["album_artist"],
            "duration": local["duration"],
            "file_size": local["file_size"],
            "metadata_hash": "same",
        }
    )
    db.commit()

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._on_device_connected(device)
        monkeypatch.setattr("services.device_inventory.read_metadata", lambda _path: (_ for _ in ()).throw(AssertionError("device view should use cached DB state")))

        window._on_sidebar_selection("device", "device_music")
        tracks = window._tracks_for_current_view(include_search=False)

        assert len(tracks) == 1
        assert tracks[0]["title"] == "Song"
    finally:
        window.close()


def test_opening_not_on_ipod_view_does_not_start_scan(config, db, monkeypatch):
    app = QApplication.instance() or QApplication([])
    _insert_local_track(db, title="Missing")
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.commit()

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._sync_engine.load_cached_device_inventory(key)
        window._sync_engine._current_device_key = key
        starts = []
        monkeypatch.setattr(window._device_inventory, "start", lambda *args, **kwargs: starts.append((args, kwargs)))

        window._on_sidebar_selection("device", "device_not_on_ipod")
        tracks = window._tracks_for_current_view(include_search=False)

        assert starts == []
        assert len(tracks) == 1
        assert tracks[0]["title"] == "Missing"
    finally:
        window.close()


def test_opening_not_on_ipod_view_defers_when_connected_cache_is_unlinked(config, db, monkeypatch):
    app = QApplication.instance() or QApplication([])
    device = _device(config.mock_device_path)

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._device_detector._current_device = device
        window._sync_engine._current_device_key = "rockbox:test"
        window._sync_engine._device_tracks = [{"device_path": "Music/Test.mp3", "local_track_id": None}]
        starts = []
        monkeypatch.setattr(
            window,
            "_scan_device",
            lambda: starts.append("scan") or setattr(window, "_device_verification_running", True),
        )
        monkeypatch.setattr(
            window._sync_engine,
            "get_not_on_device_tracks",
            lambda *args, **kwargs: (_ for _ in ()).throw(AssertionError("should not compute missing tracks on the UI thread")),
        )

        window._on_sidebar_selection("device", "device_not_on_ipod")
        tracks = window._tracks_for_current_view(include_search=False)

        assert starts == ["scan"]
        assert tracks == []
    finally:
        window.close()


def test_opening_not_on_ipod_view_uses_verified_partial_inventory(config, db, monkeypatch):
    app = QApplication.instance() or QApplication([])
    on_device = _insert_local_track(db, title="On Device", metadata_hash="same")
    _insert_local_track(db, title="Missing", metadata_hash="missing")
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.mark_device_scanned(key)
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - On Device.mp3",
            "title": "On Device",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "duration": 120.0,
            "file_size": 100,
            "metadata_hash": "same",
            "local_track_id": on_device["id"],
        }
    )
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/02 - Device Only.mp3",
            "title": "Device Only",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "duration": 120.0,
            "file_size": 100,
            "metadata_hash": "other",
            "local_track_id": None,
        }
    )
    db.commit()

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._device_detector._current_device = device
        window._sync_engine._current_device_key = key
        window._sync_engine.load_cached_device_inventory(key)
        monkeypatch.setattr(
            window,
            "_scan_device",
            lambda: (_ for _ in ()).throw(AssertionError("verified inventory should load directly")),
        )

        window._on_sidebar_selection("device", "device_not_on_ipod")
        tracks = window._tracks_for_current_view(include_search=False)

        assert [dict(row)["title"] for row in tracks] == ["Missing"]
    finally:
        window.close()


def test_not_on_ipod_view_status_shows_reason_breakdown(config, db, monkeypatch):
    app = QApplication.instance() or QApplication([])
    _insert_local_track(db, title="Missing", metadata_hash="missing")
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.mark_device_scanned(key)
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - Other.mp3",
            "title": "Other",
            "artist": "Artist",
            "album": "Album",
            "album_artist": "Artist",
            "duration": 120.0,
            "file_size": 100,
            "metadata_hash": "other",
            "local_track_id": None,
        }
    )
    db.commit()

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._sync_engine._current_device_key = key
        window._sync_engine.load_cached_device_inventory(key)

        window._on_sidebar_selection("device", "device_not_on_ipod")

        status = window._status_bar._left_label.text()
        assert "Not on iPod: 1 track" in status
        assert "title mismatch 1" in status
    finally:
        window.close()


def test_connect_does_not_auto_verify_when_disabled(config, db, monkeypatch):
    app = QApplication.instance() or QApplication([])
    device = _device(config.mock_device_path)
    config.verify_device_in_background = False
    config.scan_on_startup = False

    calls = []
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda delay, callback: calls.append((delay, callback)))
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr("ui.main_window.MainWindow._refresh_device_storage_breakdown", lambda self, device=None: None)

    window = MainWindow(config)
    try:
        startup_calls = len(calls)
        window._on_device_connected(device)
        assert window._device_verification_running is False
        assert all(delay != 50 for delay, _callback in calls[startup_calls:])
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()


def test_connect_auto_verify_runs_only_when_enabled(config, db, monkeypatch):
    app = QApplication.instance() or QApplication([])
    device = _device(config.mock_device_path)
    config.verify_device_in_background = True
    config.scan_on_startup = False

    calls = []

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda delay, callback: calls.append((delay, callback)))
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr("ui.main_window.MainWindow._refresh_device_storage_breakdown", lambda self, device=None: None)

    window = MainWindow(config)
    try:
        startup_calls = len(calls)
        window._on_device_connected(device)
        assert any(delay == 50 for delay, _callback in calls[startup_calls:])
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()


def test_sidebar_not_on_ipod_count_is_suppressed_during_verification(config, db, monkeypatch):
    app = QApplication.instance() or QApplication([])
    _insert_local_track(db, title="Missing")

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._device_verification_running = True
        window._update_status_bar()

        item = window._sidebar._find_item("device_not_on_ipod")
        assert item.text(0) == "Not on iPod"
        assert item.toolTip(0) == "Verifying device inventory"
    finally:
        window.close()


def test_connected_cached_inventory_without_links_skips_missing_count(config, db, monkeypatch):
    app = QApplication.instance() or QApplication([])
    _insert_local_track(db, title="Missing")

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._device_detector._current_device = _device(config.mock_device_path)
        window._sync_engine._current_device_key = "rockbox:test"
        window._sync_engine._device_tracks = [{"device_path": "Music/Test.mp3", "local_track_id": None}]

        monkeypatch.setattr(
            window._sync_engine,
            "get_not_on_device_tracks",
            lambda *args, **kwargs: (_ for _ in ()).throw(AssertionError("expensive missing-count path should be skipped")),
        )
        monkeypatch.setattr(
            window._sync_engine,
            "get_sync_status_counts",
            lambda: (_ for _ in ()).throw(AssertionError("expensive sync-count path should be skipped")),
        )

        window._update_status_bar()

        item = window._sidebar._find_item("device_not_on_ipod")
        assert item.text(0) == "Not on iPod"
        assert item.toolTip(0) == "Verifying device inventory"
        assert window._status_bar._right_label.text() == "Using cached device inventory"
    finally:
        window.close()


def test_device_verification_reports_unsupported_and_parse_failures(config, db):
    device = _device(config.mock_device_path)
    unsupported = os.path.join(config.mock_device_path, "Music", "notes.txt")
    bad_audio = os.path.join(config.mock_device_path, "Music", "bad.mp3")
    os.makedirs(os.path.dirname(unsupported), exist_ok=True)
    with open(unsupported, "w") as f:
        f.write("not audio")
    with open(bad_audio, "wb") as f:
        f.write(b"bad")

    def fail_metadata(_path):
        raise ValueError("cannot parse")

    summary = verify_device_inventory(
        db,
        device,
        "metadata_only",
        metadata_reader_func=fail_metadata,
    )
    reasons = {item["reason"].split(":", 1)[0] for item in summary["skipped"]}
    assert "unsupported format" in reasons
    assert "parse failure" in reasons


def test_reconciliation_report_counts_and_reasons(config, db):
    local = _insert_local_track(db, title="Local Only", artist="Artist", album="Album")
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/Other.mp3",
            "title": "Other",
            "artist": "Artist",
            "album": "Album",
            "duration": local["duration"],
            "metadata_hash": "other",
        }
    )
    db.commit()

    report = build_reconciliation_report(db, key, sample_limit=5)
    assert report["local_track_count"] == 1
    assert report["device_track_count"] == 1
    assert report["matched_count"] == 0
    assert report["unmatched_count"] == 1
    assert report["unmatched_examples"][0]["reason"] == "title mismatch"


def test_device_playlist_import_maps_cached_device_tracks(config, db):
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    local = _insert_local_track(db, title="Song", artist="Artist", album="Album", metadata_hash="mh")
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "local_track_id": local["id"],
            "metadata_hash": "mh",
        }
    )
    playlist_dir = os.path.join(config.mock_device_path, "Playlists")
    os.makedirs(playlist_dir, exist_ok=True)
    with open(os.path.join(playlist_dir, "Favorites.m3u"), "w", encoding="utf-8") as handle:
        handle.write("Music/Artist/Album/01 - Song.mp3\n")

    imported = import_device_playlists(db, device)
    db.commit()

    playlists = db.get_device_playlists(key)
    tracks = db.get_device_playlist_tracks(playlists[0]["id"])
    assert imported == 1
    assert len(playlists) == 1
    assert playlists[0]["name"] == "Favorites"
    assert len(tracks) == 1
    assert tracks[0]["title"] == "Song"


def test_device_playlist_import_skips_unchanged_files(config, db, monkeypatch):
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    local = _insert_local_track(db, title="Song", artist="Artist", album="Album", metadata_hash="mh")
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "local_track_id": local["id"],
            "metadata_hash": "mh",
        }
    )
    playlist_dir = os.path.join(config.mock_device_path, "Playlists")
    os.makedirs(playlist_dir, exist_ok=True)
    playlist_path = os.path.join(playlist_dir, "Favorites.m3u")
    with open(playlist_path, "w", encoding="utf-8") as handle:
        handle.write("Music/Artist/Album/01 - Song.mp3\n")

    first = import_device_playlists(db, device)
    db.commit()

    monkeypatch.setattr(
        "services.rockbox_playlists._parse_playlist_entries",
        lambda _path: (_ for _ in ()).throw(AssertionError("unchanged playlist should be reused from cache")),
    )
    second = import_device_playlists(db, device)
    db.commit()

    playlists = db.get_device_playlists(key)
    tracks = db.get_device_playlist_tracks(playlists[0]["id"])
    assert first == 1
    assert second == 0
    assert len(playlists) == 1
    assert len(tracks) == 1
    assert tracks[0]["title"] == "Song"


def test_device_playlist_import_removes_deleted_files(config, db):
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    playlist_dir = os.path.join(config.mock_device_path, "Playlists")
    os.makedirs(playlist_dir, exist_ok=True)
    playlist_path = os.path.join(playlist_dir, "Favorites.m3u")
    with open(playlist_path, "w", encoding="utf-8") as handle:
        handle.write("Music/Artist/Album/01 - Song.mp3\n")

    assert import_device_playlists(db, device) == 1
    db.commit()
    assert len(db.get_device_playlists(key)) == 1

    os.remove(playlist_path)

    assert import_device_playlists(db, device) == 0
    db.commit()
    assert db.get_device_playlists(key) == []


def test_device_playlist_view_loads_from_cached_db(config, db, monkeypatch):
    app = QApplication.instance() or QApplication([])
    device = _device(config.mock_device_path)
    key = db.upsert_device(device_record_from_info(device))["stable_device_key"]
    local = _insert_local_track(db, title="Song", artist="Artist", album="Album", metadata_hash="mh")
    db.upsert_device_track(
        {
            "device_id": key,
            "device_path": "Music/Artist/Album/01 - Song.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "local_track_id": local["id"],
            "metadata_hash": "mh",
        }
    )
    playlist = db.upsert_device_playlist(
        {
            "device_id": key,
            "name": "Favorites",
            "source_path": "Playlists/Favorites.m3u",
            "track_count": 1,
        }
    )
    db.replace_device_playlist_tracks(
        playlist["id"],
        [
            {
                "position": 1,
                "device_track_id": db.get_all_device_tracks(key)[0]["id"],
                "local_track_id": local["id"],
                "entry_path": "Music/Artist/Album/01 - Song.mp3",
                "title": "Song",
                "artist": "Artist",
                "album": "Album",
            }
        ],
    )
    db.commit()

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._sync_engine._current_device_key = key
        window._refresh_device_playlists()
        window._on_sidebar_selection("device", f"device_playlist_{playlist['id']}")
        tracks = window._tracks_for_current_view(include_search=False)
        assert len(tracks) == 1
        assert tracks[0]["title"] == "Song"
    finally:
        window.close()


def test_verify_device_inventory_emits_phase_statuses(config, db):
    device = _device(config.mock_device_path)
    phases = []

    summary = verify_device_inventory(
        db,
        device,
        "metadata_only",
        metadata_reader_func=lambda path: (_ for _ in ()).throw(ValueError("cannot parse")),
        status_callback=phases.append,
    )

    assert summary["device_key"]
    assert phases == [
        "Loading cached device inventory...",
        "Verifying device files...",
        "Matching device tracks to library...",
        "Importing Rockbox play stats...",
        "Importing Rockbox playlists...",
        "Updating cached device inventory...",
    ]


def test_theme_asset_manager_falls_back_when_imported_asset_missing(config, tmp_dir, monkeypatch):
    default_dir = os.path.join(tmp_dir, "theme_default")
    personal_dir = os.path.join(tmp_dir, "theme_itunes_personal")
    os.makedirs(os.path.join(default_dir, "toolbar"), exist_ok=True)
    os.makedirs(personal_dir, exist_ok=True)
    with open(os.path.join(default_dir, "theme.json"), "w", encoding="utf-8") as f:
        json.dump({}, f)
    with open(os.path.join(personal_dir, "theme.json"), "w", encoding="utf-8") as f:
        json.dump({}, f)
    with open(os.path.join(default_dir, "toolbar", "sync.png"), "wb") as f:
        f.write(b"default")

    monkeypatch.setattr(theme_assets_module, "DEFAULT_THEME_DIR", theme_assets_module.Path(default_dir))
    monkeypatch.setattr(theme_assets_module, "PERSONAL_THEME_DIR", theme_assets_module.Path(personal_dir))
    config.theme_mode = "personal"
    manager = ThemeAssetManager(config)

    assert manager.active_theme_id == "personal"
    assert manager.asset_path("toolbar_sync").endswith("theme_default/toolbar/sync.png")


def test_theme_asset_manager_rejects_pack_without_manifest(config, tmp_dir):
    pack = os.path.join(tmp_dir, "empty_pack")
    os.makedirs(pack, exist_ok=True)
    manager = ThemeAssetManager(config)
    validation = manager.validate_pack(pack)
    assert validation["valid"] is False
    assert "theme.json" in validation["missing"]


def test_theme_asset_manager_parses_manifest_colors_and_metrics(config, tmp_dir):
    pack = os.path.join(tmp_dir, "manifest_pack")
    os.makedirs(pack, exist_ok=True)
    with open(os.path.join(pack, "theme.json"), "w", encoding="utf-8") as f:
        json.dump(
            {
                "name": "Personal Pack",
                "colors": {"selection_blue_top": "#123456"},
                "metrics": {"toolbar_height": 41},
            },
            f,
        )

    manager = ThemeAssetManager(config)
    validation = manager.validate_pack(pack)

    assert validation["valid"] is True
    assert validation["manifest"]["name"] == "Personal Pack"
    assert validation["manifest"]["colors"]["selection_blue_top"] == "#123456"
    assert validation["manifest"]["metrics"]["toolbar_height"] == 41


def test_theme_asset_manager_handles_invalid_personal_manifest_safely(config, tmp_dir, monkeypatch):
    default_dir = os.path.join(tmp_dir, "theme_default")
    personal_dir = os.path.join(tmp_dir, "theme_itunes_personal")
    os.makedirs(os.path.join(default_dir, "branding"), exist_ok=True)
    os.makedirs(personal_dir, exist_ok=True)
    with open(os.path.join(default_dir, "theme.json"), "w", encoding="utf-8") as f:
        json.dump({}, f)
    with open(os.path.join(default_dir, "branding", "title.png"), "wb") as f:
        f.write(b"default")
    with open(os.path.join(personal_dir, "theme.json"), "w", encoding="utf-8") as f:
        f.write("{invalid json")

    monkeypatch.setattr(theme_assets_module, "DEFAULT_THEME_DIR", theme_assets_module.Path(default_dir))
    monkeypatch.setattr(theme_assets_module, "PERSONAL_THEME_DIR", theme_assets_module.Path(personal_dir))
    config.theme_mode = "personal"
    manager = ThemeAssetManager(config)

    assert manager.active_theme_id == "default"
    assert "invalid theme.json" in manager.theme_status()["personal_missing"]
    assert manager.asset_path("branding_title").endswith("theme_default/branding/title.png")


def test_theme_asset_manager_supports_partial_personal_pack(config, tmp_dir, monkeypatch):
    default_dir = os.path.join(tmp_dir, "theme_default")
    personal_dir = os.path.join(tmp_dir, "theme_itunes_personal")
    os.makedirs(os.path.join(default_dir, "toolbar"), exist_ok=True)
    os.makedirs(os.path.join(default_dir, "branding"), exist_ok=True)
    os.makedirs(os.path.join(personal_dir, "branding"), exist_ok=True)
    with open(os.path.join(default_dir, "theme.json"), "w", encoding="utf-8") as f:
        json.dump({}, f)
    with open(os.path.join(personal_dir, "theme.json"), "w", encoding="utf-8") as f:
        json.dump({}, f)
    with open(os.path.join(default_dir, "toolbar", "sync.png"), "wb") as f:
        f.write(b"default-sync")
    with open(os.path.join(default_dir, "branding", "title.png"), "wb") as f:
        f.write(b"default-title")
    with open(os.path.join(personal_dir, "branding", "title.png"), "wb") as f:
        f.write(b"personal-title")

    monkeypatch.setattr(theme_assets_module, "DEFAULT_THEME_DIR", theme_assets_module.Path(default_dir))
    monkeypatch.setattr(theme_assets_module, "PERSONAL_THEME_DIR", theme_assets_module.Path(personal_dir))
    config.theme_mode = "personal"
    manager = ThemeAssetManager(config)

    assert manager.asset_path("branding_title").endswith("theme_itunes_personal/branding/title.png")
    assert manager.asset_path("toolbar_sync").endswith("theme_default/toolbar/sync.png")


def test_theme_asset_manager_switches_between_default_and_personal(config, tmp_dir, monkeypatch):
    default_dir = os.path.join(tmp_dir, "theme_default")
    personal_dir = os.path.join(tmp_dir, "theme_itunes_personal")
    os.makedirs(os.path.join(default_dir, "branding"), exist_ok=True)
    os.makedirs(os.path.join(personal_dir, "branding"), exist_ok=True)
    with open(os.path.join(default_dir, "theme.json"), "w", encoding="utf-8") as f:
        json.dump({}, f)
    with open(os.path.join(personal_dir, "theme.json"), "w", encoding="utf-8") as f:
        json.dump({}, f)
    with open(os.path.join(default_dir, "branding", "title.png"), "wb") as f:
        f.write(b"default-title")
    with open(os.path.join(personal_dir, "branding", "title.png"), "wb") as f:
        f.write(b"personal-title")

    monkeypatch.setattr(theme_assets_module, "DEFAULT_THEME_DIR", theme_assets_module.Path(default_dir))
    monkeypatch.setattr(theme_assets_module, "PERSONAL_THEME_DIR", theme_assets_module.Path(personal_dir))

    config.theme_mode = "default"
    default_manager = ThemeAssetManager(config)
    assert default_manager.asset_path("branding_title").endswith("theme_default/branding/title.png")

    config.theme_mode = "personal"
    personal_manager = ThemeAssetManager(config)
    assert personal_manager.asset_path("branding_title").endswith("theme_itunes_personal/branding/title.png")
