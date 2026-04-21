"""Defensive UI model tests."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication
from PIL import Image

from services.artwork_manager import ArtworkManager
from ui.library_views import AlbumGridView
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


def test_album_grid_uses_larger_album_tiles(tmp_dir):
    app = QApplication.instance() or QApplication([])
    view = AlbumGridView(ArtworkManager(os.path.join(tmp_dir, "artwork")))

    assert view._grid.iconSize().width() >= 112
    assert view._grid.gridSize().width() >= 160
