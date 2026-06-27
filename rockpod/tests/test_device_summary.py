"""Tests for the iPod Summary view and storage helpers."""

import os

from PySide6.QtWidgets import QApplication, QMessageBox

from app.database import Database
from services.device_detector import DeviceDetector, DeviceInfo, create_mock_device
from services.rockbox_device import (
    clear_rockbox_database_cache,
    detect_rockbox_database_state,
    enable_rockbox_tagcache_autoupdate,
    invalidate_pictureflow_cache,
)
from ui.device_summary import DeviceSummaryWidget, build_summary_data, compute_device_storage
from ui.storage_bar import storage_segments


def _temp_names(path):
    return [name for name in os.listdir(path) if name.startswith("tmp")]


def test_summary_storage_computes_capacity_used_free():
    class Device:
        total_space = 1000
        used_space = 400
        free_space = 600

    storage = compute_device_storage(
        Device(),
        [{"file_size": 250}, {"file_size": 50}],
    )

    assert storage == {
        "total": 1000,
        "used": 400,
        "free": 600,
        "music": 300,
        "other": 100,
    }


def test_summary_falls_back_with_missing_data():
    data = build_summary_data(None, None, [], None, None)

    assert data["name"] == "No iPod Connected"
    assert data["capacity"] == "Unknown"
    assert data["free"] == "Unknown"
    assert data["track_count"] == 0
    assert data["device_state"] == "Disconnected"
    assert data["last_sync"] == "Never"
    assert data["last_play_sync"] == "Never"
    assert data["playlist_count"] == 0
    assert data["played_on_ipod"] == 0


def test_storage_segments_include_music_and_free():
    segments = storage_segments(1000, 300, 100)

    assert [segment["name"] for segment in segments] == ["Music", "Other", "Free"]
    assert segments[-1]["bytes"] == 600


def test_summary_action_state_follows_connection():
    app = QApplication.instance() or QApplication([])
    widget = DeviceSummaryWidget()

    widget.update_summary(build_summary_data(None, None, [], None, None))
    assert widget._sync_btn.isEnabled() is False
    assert widget._settings_btn.isEnabled() is False
    assert widget._eject_btn.isEnabled() is False

    class Device:
        name = "Mock iPod"
        mount_path = "/tmp/ipod"
        is_rockbox = True
        total_space = 1000
        used_space = 400
        free_space = 600

    widget.update_summary(build_summary_data(Device(), None, [{"file_size": 100}], None, None))
    assert widget._sync_btn.isEnabled() is True
    assert widget._settings_btn.isEnabled() is True
    assert widget._eject_btn.isEnabled() is True


def test_cached_device_data_loads_into_summary(config, tmp_dir):
    create_mock_device(config.mock_device_path)
    device = DeviceInfo(config.mock_device_path)
    device.name = "Cached iPod"
    db = Database(config.db_path)
    try:
        from services.device_inventory import device_record_from_info

        row = db.upsert_device(device_record_from_info(device))
        db.upsert_device_track(
            {
                "device_id": row["stable_device_key"],
                "device_path": "Music/Artist/Album/Song.mp3",
                "title": "Song",
                "artist": "Artist",
                "album": "Album",
                "file_size": 123,
                "metadata_hash": "mh",
            }
        )
        db.mark_device_scanned(row["stable_device_key"])
        db.commit()

        saved = db.get_device_by_key(row["stable_device_key"])
        tracks = db.get_all_device_tracks(row["stable_device_key"])
        data = build_summary_data(device, saved, tracks, None, detect_rockbox_database_state(device, saved))

        assert data["name"] == "Cached iPod"
        assert data["track_count"] == 1
        assert data["last_scan"] != "Never"
    finally:
        db.close()


def test_summary_uses_cached_device_row_when_disconnected():
    device_row = {
        "display_name": "Cached iPod",
        "mount_path_last_seen": "/tmp/ipod",
        "rockbox_detected": 1,
        "capacity_bytes": 1000,
        "free_bytes_last_seen": 250,
        "last_scan_at": "2026-04-17 10:00:00",
        "last_sync_at": "2026-04-17 09:00:00",
    }

    data = build_summary_data(None, device_row, [{"file_size": 400}], {"last_imported_at": "2026-04-17 08:00:00", "total_play_count": 22, "rated_tracks": 3}, {"database_status": "Needs refresh", "runtime_data_available": True}, "Sync Complete (DB stale)")

    assert data["name"] == "Cached iPod"
    assert data["connected"] is False
    assert data["rockbox"] == "Yes"
    assert data["capacity"] != "Unknown"
    assert data["free"] != "Unknown"
    assert data["track_count"] == 1
    assert data["last_play_sync"] == "2026-04-17 08:00:00"
    assert data["play_count_total"] == 22
    assert data["device_state"] == "Sync Complete (DB stale)"
    assert data["rockbox_database"] == "Needs refresh"
    assert data["runtime_data"] == "Available"


def test_summary_uses_runtime_insights():
    device_row = {"display_name": "Cached iPod", "rockbox_detected": 1}
    data = build_summary_data(
        None,
        device_row,
        [{"file_size": 400}],
        {"last_imported_at": "2026-04-17 08:00:00", "total_play_count": 22, "rated_tracks": 3},
        {"database_status": "Ready", "runtime_data_available": True},
        "Ready",
        device_playlist_count=4,
        runtime_insights={
            "ipod_play_count_total": 18,
            "desktop_play_count_total": 7,
            "combined_play_count_total": 25,
            "most_played_artist": "Artist",
            "most_played_album": "Album",
        },
    )
    assert data["playlist_count"] == 4
    assert data["played_on_ipod"] == 18
    assert data["played_on_desktop"] == 7
    assert data["played_combined"] == 25
    assert data["most_played_artist"] == "Artist"
    assert data["most_played_album"] == "Album"


def test_summary_uses_detected_device_metadata():
    class Device:
        name = "Classic"
        mount_path = "/tmp/ipod"
        is_rockbox = True
        total_space = 1000
        used_space = 400
        free_space = 600
        detected_model = "iPod Classic 6G"
        device_type = "classic"
        rockbox_target = "ipod6g"
        screen_resolution = "320x240"

    data = build_summary_data(Device(), None, [], None, None)

    assert data["model"] == "iPod Classic 6G"
    assert data["device_type"] == "Classic"
    assert data["rockbox_target"] == "ipod6g"
    assert data["screen_resolution"] == "320x240"


def test_enable_rockbox_tagcache_autoupdate_updates_config(tmp_dir):
    path = os.path.join(tmp_dir, "ipod")
    create_mock_device(path)
    ok = enable_rockbox_tagcache_autoupdate(DeviceInfo(path))
    assert ok is True
    config_path = os.path.join(path, ".rockbox", "config.cfg")
    assert os.path.isfile(config_path)
    assert not _temp_names(os.path.dirname(config_path))
    state = detect_rockbox_database_state(DeviceInfo(path), {"last_sync_at": "2026-04-17 12:00:00"})
    assert state["tagcache_autoupdate"] is True


def test_clear_rockbox_database_cache_removes_only_tagcache_files(tmp_dir):
    path = os.path.join(tmp_dir, "ipod")
    create_mock_device(path)
    db_dir = os.path.join(path, ".rockbox", "database")
    os.makedirs(db_dir, exist_ok=True)
    db_file = os.path.join(db_dir, "database_0.tcd")
    tagcache_file = os.path.join(path, ".rockbox", "tagcache_1.tcd")
    keep_file = os.path.join(path, "Music", "Artist", "Album", "Song.mp3")
    os.makedirs(os.path.dirname(keep_file), exist_ok=True)
    for file_path, content in (
        (db_file, b"db"),
        (tagcache_file, b"tagcache"),
        (keep_file, b"music"),
    ):
        with open(file_path, "wb") as handle:
            handle.write(content)

    result = clear_rockbox_database_cache(DeviceInfo(path))

    assert result["success"] is True
    assert sorted(os.path.basename(item) for item in result["removed"]) == ["database_0.tcd", "tagcache_1.tcd"]
    assert not os.path.exists(db_file)
    assert not os.path.exists(tagcache_file)
    assert os.path.isfile(keep_file)


def test_invalidate_pictureflow_cache_removes_generated_slides_and_forces_rebuild(tmp_dir):
    path = os.path.join(tmp_dir, "ipod")
    create_mock_device(path)
    cache_dir = os.path.join(path, ".rockbox", "rocks", "demos", "pictureflow")
    os.makedirs(cache_dir, exist_ok=True)
    pfraw_file = os.path.join(cache_dir, "deadbeefcafebabe.pfraw")
    index_file = os.path.join(cache_dir, "pictureflow_album.idx")
    keep_file = os.path.join(path, ".rockbox", "rocks", "demos", "pictureflow.rock")
    config_file = os.path.join(path, ".rockbox", "rocks", "demos", "pictureflow.cfg")
    for file_path, content in (
        (pfraw_file, b"slide"),
        (index_file, b"index"),
        (keep_file, b"plugin"),
    ):
        with open(file_path, "wb") as handle:
            handle.write(content)
    with open(config_file, "w", encoding="utf-8") as handle:
        handle.write("cache version:          5\nupdate albumart:          1\n")

    result = invalidate_pictureflow_cache(DeviceInfo(path))

    assert result["success"] is True
    assert sorted(os.path.basename(item) for item in result["removed"]) == [
        "deadbeefcafebabe.pfraw",
        "pictureflow_album.idx",
    ]
    assert not os.path.exists(pfraw_file)
    assert not os.path.exists(index_file)
    assert os.path.isfile(keep_file)
    with open(config_file, "r", encoding="utf-8") as handle:
        config_text = handle.read()
    assert "cache version:          0" in config_text
    assert "update albumart:          0" in config_text


def test_device_root_selection_shows_summary(config, db, monkeypatch):
    from PySide6.QtCore import QTimer

    from services.library_scanner import LibraryScanner
    from ui.main_window import MainWindow

    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._on_sidebar_selection("device", "device_root")
        assert window._content_stack.currentWidget() is window._device_summary
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()


def test_device_root_shows_cached_summary_after_disconnect(config, db, monkeypatch):
    from PySide6.QtCore import QTimer

    from services.library_scanner import LibraryScanner
    from services.device_inventory import device_record_from_info
    from ui.main_window import MainWindow

    app = QApplication.instance() or QApplication([])
    create_mock_device(config.mock_device_path)
    device = DeviceInfo(config.mock_device_path)
    device.name = "Cached iPod"
    row = db.upsert_device(device_record_from_info(device))
    db.upsert_device_track(
        {
            "device_id": row["stable_device_key"],
            "device_path": "Music/Artist/Album/Song.mp3",
            "title": "Song",
            "artist": "Artist",
            "album": "Album",
            "file_size": 123,
            "metadata_hash": "mh",
        }
    )
    db.commit()

    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    window = MainWindow(config)
    try:
        window._on_device_connected(device)
        window._on_device_disconnected(device.mount_path)
        window._on_sidebar_selection("device", "device_root")

        assert window._device_summary._name.text() == "Cached iPod"
        assert window._device_summary._connection.text() == "Disconnected"
        assert window._device_summary._fields["track_count"].text() == "1"
    finally:
        window.close()


def test_detect_rockbox_database_state_marks_ready_after_db_update(tmp_dir):
    path = os.path.join(tmp_dir, "ipod")
    create_mock_device(path)
    db_file = os.path.join(path, ".rockbox", "database", "database_0.tcd")
    with open(db_file, "wb") as handle:
        handle.write(b"db")
    state = detect_rockbox_database_state(
        DeviceInfo(path),
        {"last_sync_at": "2000-01-01 00:00:00"},
    )
    assert state["database_needs_refresh"] is False
    assert state["database_status"] == "Ready"


def test_detect_rockbox_database_state_marks_stale_without_fresh_db(tmp_dir):
    path = os.path.join(tmp_dir, "ipod")
    create_mock_device(path)
    state = detect_rockbox_database_state(
        DeviceInfo(path),
        {"last_sync_at": "2026-04-17 12:00:00"},
    )
    assert state["database_needs_refresh"] is True


def test_noop_post_sync_does_not_mark_database_stale(config, monkeypatch):
    from PySide6.QtCore import QTimer

    from services.library_scanner import LibraryScanner
    from ui.main_window import MainWindow

    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    create_mock_device(config.mock_device_path)
    device = DeviceInfo(config.mock_device_path)

    window = MainWindow(config)
    try:
        window._device_detector._current_device = device
        class EmptyPlan:
            to_copy = []
            to_resync = []
            to_delete = []

        window._active_sync_plan = EmptyPlan()
        window._device_state = "Syncing"
        window._post_sync_rockbox_integration()

        assert window._device_state == "Ready"
        assert window._rockbox_db_stale is False
    finally:
        window.close()


def test_summary_toggle_writes_per_device_override(config, monkeypatch):
    from PySide6.QtCore import QTimer

    from services.library_scanner import LibraryScanner
    from ui.main_window import MainWindow

    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    create_mock_device(config.mock_device_path)
    device = DeviceInfo(config.mock_device_path)
    config.auto_sync_on_connect = False

    window = MainWindow(config)
    try:
        window._on_device_connected(device)
        window._set_auto_sync_from_summary(True)

        assert config.auto_sync_on_connect is False
        assert config.get_effective("auto_sync_on_connect", device=device) is True
    finally:
        window.close()


def test_rename_connected_device_persists_display_name_override(config, monkeypatch):
    from PySide6.QtCore import QTimer

    from services.library_scanner import LibraryScanner
    from ui.main_window import MainWindow

    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    create_mock_device(config.mock_device_path)
    device = DeviceInfo(config.mock_device_path)

    window = MainWindow(config)
    try:
        window._device_detector._current_device = device
        window._on_device_connected(device)
        window._apply_connected_device_name("Car iPod")

        saved = window._db.get_device_by_key(window._sync_engine.current_device_key)
        assert device.name == "Car iPod"
        assert config.get_device_display_name(device=device) == "Car iPod"
        assert saved["display_name"] == "Car iPod"
    finally:
        window.close()


def test_device_settings_helper_persists_multiple_overrides(config, monkeypatch):
    from PySide6.QtCore import QTimer

    from services.library_scanner import LibraryScanner
    from ui.main_window import MainWindow

    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)

    create_mock_device(config.mock_device_path)
    device = DeviceInfo(config.mock_device_path)

    window = MainWindow(config)
    try:
        window._device_detector._current_device = device
        window._on_device_connected(device)
        window._apply_device_settings(
            {
                "display_name": "Car iPod",
                "sync_playlists_to_device": False,
                "device_music_template": "Music/{artist}",
                "duration_match_tolerance_seconds": 4.5,
            }
        )

        assert config.get_device_display_name(device=device) == "Car iPod"
        assert config.get_effective("sync_playlists_to_device", device=device) is False
        assert config.get_effective("device_music_template", device=device) == "Music/{artist}"
        assert config.get_effective("duration_match_tolerance_seconds", device=device) == 4.5
        assert window._device_summary._name.text() == "Car iPod"
    finally:
        window.close()


def test_clear_rockbox_database_cache_action_updates_state(config, monkeypatch):
    from PySide6.QtCore import QTimer

    from services.library_scanner import LibraryScanner
    from ui.main_window import MainWindow

    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr("ui.main_window.MainWindow._refresh_device_storage_breakdown", lambda self, device=None: None)
    monkeypatch.setattr("ui.main_window.QMessageBox.question", lambda *args, **kwargs: QMessageBox.Yes)
    infos = []
    warnings = []
    monkeypatch.setattr("ui.main_window.QMessageBox.information", lambda *args, **kwargs: infos.append(args[2] if len(args) > 2 else ""))
    monkeypatch.setattr("ui.main_window.QMessageBox.warning", lambda *args, **kwargs: warnings.append(args[2] if len(args) > 2 else ""))

    create_mock_device(config.mock_device_path)
    db_dir = os.path.join(config.mock_device_path, ".rockbox", "database")
    os.makedirs(db_dir, exist_ok=True)
    with open(os.path.join(db_dir, "database_0.tcd"), "wb") as handle:
        handle.write(b"db")
    device = DeviceInfo(config.mock_device_path)

    window = MainWindow(config)
    try:
        window._device_detector._current_device = device
        window._device_state = "Connected"
        window._clear_rockbox_database_cache()

        assert window._rockbox_db_stale is True
        assert window._device_state == "Sync Complete (DB stale)"
        assert warnings == []
        assert any("Rockbox database cache files" in text for text in infos)
        assert not os.path.exists(os.path.join(db_dir, "database_0.tcd"))
    finally:
        window.close()
