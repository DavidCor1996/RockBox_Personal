"""Tests for the album info dialog."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from PySide6.QtWidgets import QApplication

from ui.dialogs.album_info import AlbumInfoDialog
from ui.dialogs.album_metadata import AlbumMetadataDialog


def test_album_info_dialog_loads_track_lyrics(monkeypatch):
    app = QApplication.instance() or QApplication([])
    album = {
        "album": "Album",
        "artist": "Artist",
        "tracks": [
            {
                "title": "Song One",
                "artist": "Artist",
                "album_artist": "Artist",
                "album": "Album",
                "track_number": 1,
                "disc_number": 1,
                "file_path": "/music/Artist/Album/01 - Song One.mp3",
            },
            {
                "title": "Song Two",
                "artist": "Artist",
                "album_artist": "Artist",
                "album": "Album",
                "track_number": 2,
                "disc_number": 1,
                "file_path": "/music/Artist/Album/02 - Song Two.mp3",
            },
        ],
    }

    monkeypatch.setattr(
        "ui.dialogs.album_info.read_lyrics",
        lambda path: "lyrics for song two" if path.endswith("Song Two.mp3") else "lyrics for song one",
    )

    dialog = AlbumInfoDialog(album)
    try:
        assert dialog._lyrics_text.toPlainText() == "lyrics for song one"

        dialog._lyrics_track_picker.setCurrentIndex(1)

        assert dialog._lyrics_text.toPlainText() == "lyrics for song two"
    finally:
        dialog.deleteLater()


def test_album_metadata_dialog_loads_fetched_track_lyrics():
    app = QApplication.instance() or QApplication([])
    metadata = {
        "album": "Album",
        "artist": "Artist",
        "tracks": [
            {
                "title": "Song One",
                "artist": "Artist",
                "track_number": 1,
                "disc_number": 1,
                "lyrics": "online lyrics one",
            },
            {
                "title": "Song Two",
                "artist": "Artist",
                "track_number": 2,
                "disc_number": 1,
                "lyrics": "online lyrics two",
            },
        ],
    }

    dialog = AlbumMetadataDialog(metadata)
    try:
        assert dialog._lyrics_text.toPlainText() == "online lyrics one"

        dialog._lyrics_track_picker.setCurrentIndex(1)

        assert dialog._lyrics_text.toPlainText() == "online lyrics two"
    finally:
        dialog.deleteLater()
