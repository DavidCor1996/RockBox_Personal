"""Column browser — the Genre/Artist/Album tri-pane browser above the track list.

This is the "Browser" feature from iTunes 7 that shows three horizontal
lists at the top of the content area for narrowing by genre, artist, album.
"""

from PySide6.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout, QListWidget, QLabel,
    QAbstractItemView, QListWidgetItem, QSizePolicy,
)
from PySide6.QtCore import Qt, Signal


class BrowserPane(QWidget):
    """A single browser column (e.g., Genre, Artist, or Album)."""

    item_selected = Signal(str)  # selected value, or "" for "All"

    def __init__(self, title, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self._header = QLabel(title)
        self._header.setObjectName("browser_header")
        layout.addWidget(self._header)

        self._list = QListWidget()
        self._list.setObjectName("browser_pane")
        self._list.setSelectionMode(QAbstractItemView.SingleSelection)
        self._list.setVerticalScrollMode(QAbstractItemView.ScrollPerPixel)
        self._list.currentItemChanged.connect(self._on_selection)
        layout.addWidget(self._list)

    def set_items(self, items):
        """Replace all items. Adds 'All (N)' at the top."""
        self._list.clear()
        all_item = QListWidgetItem(f"All ({len(items)})")
        all_item.setData(Qt.UserRole, "")
        self._list.addItem(all_item)
        for item_text in items:
            wi = QListWidgetItem(item_text)
            wi.setData(Qt.UserRole, item_text)
            self._list.addItem(wi)
        self._list.setCurrentRow(0)

    def _on_selection(self, current, previous):
        if current:
            value = current.data(Qt.UserRole)
            self.item_selected.emit(value or "")

    def selected_value(self):
        item = self._list.currentItem()
        if item:
            return item.data(Qt.UserRole) or ""
        return ""

    def clear_selection(self):
        self._list.setCurrentRow(0)


class ColumnBrowser(QWidget):
    """iTunes 7-era three-pane column browser (Genre / Artist / Album)."""

    filter_changed = Signal(str, str, str)  # genre, artist, album

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(106)
        self.setMaximumHeight(156)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(1)

        self._genre_pane = BrowserPane("Genre")
        self._artist_pane = BrowserPane("Artist")
        self._album_pane = BrowserPane("Album")

        layout.addWidget(self._genre_pane)
        layout.addWidget(self._artist_pane)
        layout.addWidget(self._album_pane)

        self._genre_pane.item_selected.connect(self._on_filter_change)
        self._artist_pane.item_selected.connect(self._on_filter_change)
        self._album_pane.item_selected.connect(self._on_filter_change)

    def _on_filter_change(self, _):
        genre = self._genre_pane.selected_value()
        artist = self._artist_pane.selected_value()
        album = self._album_pane.selected_value()
        self.filter_changed.emit(genre, artist, album)

    def set_genres(self, genres):
        self._genre_pane.set_items(genres)

    def set_artists(self, artists):
        self._artist_pane.set_items(artists)

    def set_albums(self, albums):
        self._album_pane.set_items(albums)

    def clear_all(self):
        self._genre_pane.clear_selection()
        self._artist_pane.clear_selection()
        self._album_pane.clear_selection()

    @property
    def current_filter(self):
        return (
            self._genre_pane.selected_value(),
            self._artist_pane.selected_value(),
            self._album_pane.selected_value(),
        )
