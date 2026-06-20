"""Defensive UI model tests."""

import os
import sys
from io import BytesIO

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from PySide6.QtCore import QPoint, Qt, QTimer
from PySide6.QtWidgets import QApplication
from PIL import Image

from services.device_detector import DeviceDetector
from services.library_scanner import LibraryScanner
from services.artwork_manager import ArtworkManager
from ui.library_views import AlbumGridView, GroupedTrackView
from ui.main_window import MainWindow
from ui.track_table import COLUMNS, TrackTable, TrackTableModel


def test_incomplete_track_dict_does_not_crash(tmp_dir):
    app = QApplication.instance() or QApplication([])
    model = TrackTableModel()
    model.set_artwork_manager(ArtworkManager(os.path.join(tmp_dir, "artwork")))
    model.set_tracks([{"title": "Partial Track"}])

    assert model.rowCount() == 1

    for column in range(len(COLUMNS)):
        index = model.index(0, column)
        model.data(index, Qt.DisplayRole)
        model.data(index, Qt.DecorationRole)
        model.data(index, Qt.TextAlignmentRole)

    model.sort(0, Qt.AscendingOrder)
    assert model.track_at(0)["file_path"] == ""
    assert model.track_at(0)["synced_to_device"] is False


def test_artwork_manager_returns_placeholder_for_missing_artwork(tmp_dir):
    manager = ArtworkManager(os.path.join(tmp_dir, "artwork"))
    audio_path = os.path.join(tmp_dir, "song.mp3")
    with open(audio_path, "wb") as f:
        f.write(b"not real audio")

    art_path = manager.get_artwork_path({
        "file_path": audio_path,
        "has_embedded_artwork": 0,
    }, "thumb")

    assert art_path is not None
    assert os.path.exists(art_path)


def test_album_artwork_reuses_embedded_art_from_one_track(tmp_dir, monkeypatch):
    manager = ArtworkManager(os.path.join(tmp_dir, "artwork"))
    album_dir = os.path.join(tmp_dir, "Album")
    os.makedirs(album_dir, exist_ok=True)
    a = os.path.join(album_dir, "01.mp3")
    b = os.path.join(album_dir, "02.mp3")
    with open(a, "wb") as f:
        f.write(b"a")
    with open(b, "wb") as f:
        f.write(b"b")

    calls = []

    def fake_extract(path):
        calls.append(path)
        if path == b:
            return b"image-bytes", "image/jpeg"
        return None, None

    monkeypatch.setattr("services.artwork_manager.extract_artwork_data", fake_extract)
    art_path = manager.get_artwork_for_album(
        {
            "group_key": "artist\0album",
            "tracks": [
                {"file_path": a, "has_embedded_artwork": 0, "album": "Album", "artist": "Artist"},
                {"file_path": b, "has_embedded_artwork": 1, "album": "Album", "artist": "Artist"},
            ],
        },
        "thumb",
    )

    assert os.path.exists(art_path)
    assert calls == [b]


def test_album_artwork_uses_folder_jpg_fallback(tmp_dir):
    manager = ArtworkManager(os.path.join(tmp_dir, "artwork"))
    album_dir = os.path.join(tmp_dir, "Album")
    os.makedirs(album_dir, exist_ok=True)
    audio = os.path.join(album_dir, "01.mp3")
    cover = os.path.join(album_dir, "folder.jpg")
    with open(audio, "wb") as f:
        f.write(b"audio")
    Image.new("RGB", (4, 4), "#336699").save(cover, "JPEG")

    art_path = manager.get_artwork_for_album(
        {
            "group_key": "artist\0album",
            "tracks": [
                {"file_path": audio, "has_embedded_artwork": 0, "album": "Album", "artist": "Artist"},
            ],
        },
        "thumb",
    )

    assert os.path.exists(art_path)
    assert "placeholder" not in os.path.basename(art_path)


def test_album_artwork_prefers_rockbox_cover_over_stale_embedded_cache(tmp_dir, monkeypatch):
    manager = ArtworkManager(os.path.join(tmp_dir, "artwork"))
    album_dir = os.path.join(tmp_dir, "Album")
    os.makedirs(album_dir, exist_ok=True)
    audio = os.path.join(album_dir, "01.flac")
    cover = os.path.join(album_dir, "cover.jpg")
    with open(audio, "wb") as f:
        f.write(b"audio")

    embedded = BytesIO()
    Image.new("RGB", (20, 20), "#ff0000").save(embedded, "JPEG")

    monkeypatch.setattr(
        "services.artwork_manager.extract_artwork_data",
        lambda _path: (embedded.getvalue(), "image/jpeg"),
    )
    album_info = {
        "group_key": "artist\0album",
        "tracks": [
            {"file_path": audio, "has_embedded_artwork": 1, "album": "Album", "artist": "Artist"},
        ],
    }

    first = manager.get_artwork_for_album(album_info, "thumb")
    assert os.path.exists(first)
    assert manager._load_album_meta("artist\0album")["source"] == "embedded"

    Image.new("RGB", (10, 10), "#336699").save(cover, "JPEG")
    second = manager.get_artwork_for_album(album_info, "thumb")
    meta = manager._load_album_meta("artist\0album")

    assert os.path.exists(second)
    assert meta["source"] == "folder"
    assert meta["source_provenance"] == cover


def test_album_artwork_missing_uses_placeholder_and_reports_diagnostic(tmp_dir):
    manager = ArtworkManager(os.path.join(tmp_dir, "artwork"))
    audio = os.path.join(tmp_dir, "Album", "01.mp3")
    os.makedirs(os.path.dirname(audio), exist_ok=True)
    with open(audio, "wb") as f:
        f.write(b"audio")

    art_path = manager.get_artwork_for_album(
        {
            "group_key": "artist\0album",
            "tracks": [
                {"file_path": audio, "has_embedded_artwork": 0, "album": "Album", "artist": "Artist"},
            ],
        },
        "thumb",
    )

    diagnostics = manager.album_artwork_diagnostics()
    assert os.path.exists(art_path)
    assert "placeholder" in os.path.basename(art_path)
    assert diagnostics["albums_without_artwork"]


def test_track_table_can_restore_selection_by_track_id():
    app = QApplication.instance() or QApplication([])
    table = TrackTable()
    model = TrackTableModel()
    model.set_tracks([
        {"id": 1, "title": "One"},
        {"id": 2, "title": "Two"},
    ])
    table.setModel(model)

    table.select_track_ids({2})

    selected = table.get_selected_track_ids()
    assert selected == {2}


def test_playlist_reorder_drop_uses_drag_payload_not_highlighted_selection(monkeypatch):
    app = QApplication.instance() or QApplication([])
    table = TrackTable()
    model = TrackTableModel()
    model.set_tracks([
        {"id": 1, "title": "One"},
        {"id": 2, "title": "Two"},
        {"id": 3, "title": "Three"},
    ])
    table.setModel(model)
    table.select_track_ids({3})
    monkeypatch.setattr(table, "_drop_target_row", lambda _position, _current_ids: 3)

    new_order, moved_ids = table._reordered_track_ids_for_drop(QPoint(0, 0), [1])

    assert moved_ids == [1]
    assert new_order == [2, 3, 1]
    assert table.get_selected_track_ids() == {3}


def test_playlist_move_actions_reorder_selected_tracks():
    assert MainWindow._playlist_order_after_selection_move([1, 2, 3], {2}, -1) == [2, 1, 3]
    assert MainWindow._playlist_order_after_selection_move([1, 2, 3], {2}, 1) == [1, 3, 2]
    assert MainWindow._playlist_order_after_selection_move([1, 2, 3, 4], {2, 3}, -1) == [2, 3, 1, 4]
    assert MainWindow._playlist_order_after_selection_move([1, 2, 3, 4], {2, 3}, 1) == [1, 4, 2, 3]


def test_album_grid_uses_larger_album_tiles(tmp_dir):
    app = QApplication.instance() or QApplication([])
    view = AlbumGridView(ArtworkManager(os.path.join(tmp_dir, "artwork")))

    assert view._grid.iconSize().width() >= 112
    assert view._grid.gridSize().width() >= 160


def test_grouped_track_view_proxies_selection_and_context_menu():
    app = QApplication.instance() or QApplication([])
    view = GroupedTrackView("Artists")
    view.set_groups({
        "Artist": [
            {"id": 1, "title": "One", "artist": "Artist"},
            {"id": 2, "title": "Two", "artist": "Artist"},
        ]
    })

    view.select_track_ids({2})

    captured = []
    view.track_context_requested.connect(lambda global_pos: captured.append(global_pos))
    view._on_track_context_menu(QPoint(1, 1))

    assert view.get_selected_track_ids() == {2}
    assert [track["title"] for track in view.get_selected_tracks()] == ["Two"]
    assert len(captured) == 1


def test_main_window_uses_grouped_view_as_active_track_table_in_artists_mode(config, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr(MainWindow, "_start_store_homepage", lambda self, force=False, tab_key=None: None)

    window = MainWindow(config)
    try:
        window._current_view = "library_artists"
        assert window._active_track_table() is window._artist_view
    finally:
        window.close()


def test_main_window_enables_drag_reorder_for_regular_playlist(config, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr(MainWindow, "_start_store_homepage", lambda self, force=False, tab_key=None: None)

    window = MainWindow(config)
    try:
        pid = window._db.create_playlist("Manual")
        window._db.upsert_track({"file_path": "/one.mp3", "title": "One"})
        window._db.commit()
        track_id = window._db.get_track_by_path("/one.mp3")["id"]
        window._db.add_track_to_playlist(pid, track_id)
        window._db.commit()

        window._current_view = f"playlist_{pid}"
        window._refresh_view()
        assert window._track_table._playlist_reorder_enabled is True

        window._current_view = "library_music"
        window._refresh_view()
        assert window._track_table._playlist_reorder_enabled is False
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()


def test_regular_playlist_keeps_manual_order_when_reorder_mode_loads(config, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr(MainWindow, "_start_store_homepage", lambda self, force=False, tab_key=None: None)

    window = MainWindow(config)
    try:
        pid = window._db.create_playlist("Manual")
        window._db.upsert_track({"file_path": "/z.mp3", "title": "Zed"})
        window._db.upsert_track({"file_path": "/a.mp3", "title": "Alpha"})
        window._db.commit()
        zed_id = window._db.get_track_by_path("/z.mp3")["id"]
        alpha_id = window._db.get_track_by_path("/a.mp3")["id"]
        window._db.add_tracks_to_playlist(pid, [zed_id, alpha_id])
        window._db.commit()

        window._track_table.sortByColumn(1, Qt.AscendingOrder)
        window._current_view = f"playlist_{pid}"
        window._refresh_view()

        assert window._track_table._playlist_reorder_enabled is True
        assert [window._track_model.track_at(row)["title"] for row in range(window._track_model.rowCount())] == [
            "Zed",
            "Alpha",
        ]
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()


def test_main_window_move_playlist_selection_up_persists_order(config, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr(MainWindow, "_start_store_homepage", lambda self, force=False, tab_key=None: None)

    window = MainWindow(config)
    try:
        pid = window._db.create_playlist("Manual")
        for title in ("One", "Two", "Three"):
            window._db.upsert_track({"file_path": f"/{title}.mp3", "title": title})
        window._db.commit()
        ids = [window._db.get_track_by_path(f"/{title}.mp3")["id"] for title in ("One", "Two", "Three")]
        window._db.add_tracks_to_playlist(pid, ids)
        window._db.commit()

        window._current_view = f"playlist_{pid}"
        window._refresh_view()
        window._track_table.select_track_ids({ids[1]})
        window._move_selection_in_playlist(-1)

        assert [row["id"] for row in window._db.get_playlist_tracks(pid)] == [ids[1], ids[0], ids[2]]
        assert window._track_table.get_selected_track_ids() == {ids[1]}
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()
