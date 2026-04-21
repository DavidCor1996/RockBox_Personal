"""Compact iTunes-style library browsing views."""

from collections import defaultdict
import re
import unicodedata

from PySide6.QtCore import Qt, QSize, Signal, QTimer
from PySide6.QtGui import QIcon, QPixmap, QPainter, QColor, QFont
from PySide6.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout, QLabel, QListWidget, QListWidgetItem,
    QAbstractItemView, QSplitter,
)

from ui.track_table import TrackTable, TrackTableModel
from ui.track_adapter import normalize_track_for_ui, normalize_tracks_for_ui


_PUNCT_TRANSLATION = str.maketrans(
    {
        "\u2018": "'",
        "\u2019": "'",
        "\u201a": "'",
        "\u201b": "'",
        "\u2032": "'",
        "\u201c": '"',
        "\u201d": '"',
        "\u201e": '"',
        "\u2033": '"',
        "\u2010": "-",
        "\u2011": "-",
        "\u2012": "-",
        "\u2013": "-",
        "\u2014": "-",
        "\u2212": "-",
        "\u00a0": " ",
    }
)


def _row_dict(row):
    return normalize_track_for_ui(row)


def _normalize_album_text(value):
    if not value:
        return ""
    value = unicodedata.normalize("NFKC", str(value)).translate(_PUNCT_TRANSLATION)
    value = value.strip().casefold()
    value = re.sub(r"\b(feat|ft)\.?\b", "featuring", value)
    value = re.sub(r"\bfeaturing\b.*$", "", value)
    value = re.sub(r"\s*&\s*", " and ", value)
    value = re.sub(r"[`´]", "'", value)
    value = re.sub(r"\s*[-_/]+\s*", " ", value)
    value = re.sub(r"\s+", " ", value)
    return value.strip()


def _primary_artist_identity(value):
    value = _normalize_album_text(value)
    if not value:
        return ""
    parts = re.split(r"\s+(?:and|x)\s+|,|;|/", value, maxsplit=1)
    primary = parts[0].strip()
    return primary or value


def album_grouping_artist(row):
    d = _row_dict(row)
    album_artist = (d.get("album_artist") or "").strip()
    if album_artist:
        return album_artist
    if d.get("compilation"):
        return "Various Artists"
    artist = (d.get("artist") or "").strip()
    if not artist:
        return "Unknown Artist"
    cleaned = re.sub(r"\b(feat|ft|featuring)\.?\b.*$", "", artist, flags=re.IGNORECASE).strip(" -")
    primary = re.split(r"\s*(?:,|/|;|\s+&\s+|\s+and\s+|\s+x\s+)\s*", cleaned, maxsplit=1, flags=re.IGNORECASE)[0].strip()
    return primary or cleaned or artist or "Unknown Artist"


def album_group_key(row):
    d = _row_dict(row)
    album = d.get("album") or "Unknown Album"
    grouping_artist = album_grouping_artist(d)
    return (
        _normalize_album_text(album),
        _primary_artist_identity(grouping_artist),
    )


def album_display_label(album, artist):
    album_text = str(album or "Unknown Album").strip() or "Unknown Album"
    artist_text = str(artist or "Unknown Artist").strip() or "Unknown Artist"
    if _normalize_album_text(album_text) == _normalize_album_text(artist_text):
        return f"{album_text}\nby {artist_text}"
    return f"{album_text}\n{artist_text}"


def album_group_diagnostics(album_groups):
    diagnostics = []
    for album in album_groups:
        diagnostics.append(
            {
                "album": album.get("album", ""),
                "display_artist": album.get("artist", ""),
                "group_key": album.get("group_key", ""),
                "source_artists": list(album.get("source_artists", [])),
                "compilation": bool(album.get("compilation")),
            }
        )
    return diagnostics


class GroupedTrackView(QWidget):
    """Source-list-style group selector with tracks on the right."""

    group_selected = Signal(str)
    track_double_clicked = Signal(object)
    selection_changed = Signal(list)

    def __init__(self, title, parent=None):
        super().__init__(parent)
        self._title = title
        self._groups = {}

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        header = QLabel(title)
        header.setObjectName("browser_header")
        layout.addWidget(header)

        splitter = QSplitter(Qt.Horizontal)
        layout.addWidget(splitter, 1)

        self._group_list = QListWidget()
        self._group_list.setObjectName("library_group_list")
        self._group_list.setSelectionMode(QAbstractItemView.SingleSelection)
        self._group_list.setVerticalScrollMode(QAbstractItemView.ScrollPerPixel)
        self._group_list.currentItemChanged.connect(self._on_group_changed)
        splitter.addWidget(self._group_list)

        self._track_model = TrackTableModel()
        self._track_table = TrackTable()
        self._track_table.setModel(self._track_model)
        self._track_table.track_double_clicked.connect(self.track_double_clicked)
        self._track_table.track_activated.connect(self.track_double_clicked)
        self._track_table.selection_changed.connect(self.selection_changed)
        splitter.addWidget(self._track_table)
        splitter.setSizes([205, 760])
        self._splitter = splitter

    def set_artwork_manager(self, manager):
        self._track_model.set_artwork_manager(manager)

    def set_groups(self, groups):
        self._groups = groups
        current_key = ""
        current = self._group_list.currentItem()
        if current:
            current_key = current.data(Qt.UserRole) or ""

        self._group_list.blockSignals(True)
        self._group_list.clear()
        all_count = sum(len(v) for v in groups.values())
        all_item = QListWidgetItem(f"All ({all_count})")
        all_item.setData(Qt.UserRole, "")
        self._group_list.addItem(all_item)

        for name in sorted(groups, key=lambda x: x.lower()):
            item = QListWidgetItem(f"{name} ({len(groups[name])})")
            item.setData(Qt.UserRole, name)
            self._group_list.addItem(item)

        row = 0
        if current_key:
            for i in range(self._group_list.count()):
                if self._group_list.item(i).data(Qt.UserRole) == current_key:
                    row = i
                    break
        self._group_list.setCurrentRow(row)
        self._group_list.blockSignals(False)
        self._apply_current_selection()

    def _on_group_changed(self, current, previous):
        self._apply_current_selection()
        value = current.data(Qt.UserRole) if current else ""
        self.group_selected.emit(value or "")

    def _apply_current_selection(self):
        current = self._group_list.currentItem()
        key = current.data(Qt.UserRole) if current else ""
        if key:
            tracks = self._groups.get(key, [])
        else:
            tracks = []
            for group_tracks in self._groups.values():
                tracks.extend(group_tracks)
        self._track_model.set_tracks(normalize_tracks_for_ui(tracks))

    def current_tracks(self):
        return [self._track_model.track_at(i) for i in range(self._track_model.rowCount())]

    def set_current_track_id(self, track_id):
        self._track_model.set_current_track_id(track_id)


class AlbumGridView(QWidget):
    """Album artwork grid with a track list for the selected album."""

    album_selected = Signal(str, str)
    album_context_requested = Signal(object, object)
    track_double_clicked = Signal(object)
    selection_changed = Signal(list)

    def __init__(self, artwork_manager, parent=None):
        super().__init__(parent)
        self._artwork = artwork_manager
        self._albums = []
        self._album_tracks = {}
        self._populate_index = 0
        self._album_placeholder_path = ""
        self._album_frame_path = ""

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        header = QLabel("Albums")
        header.setObjectName("browser_header")
        layout.addWidget(header)

        splitter = QSplitter(Qt.Vertical)
        layout.addWidget(splitter, 1)

        self._grid = QListWidget()
        self._grid.setObjectName("album_grid")
        self._grid.setViewMode(QListWidget.IconMode)
        self._grid.setResizeMode(QListWidget.Adjust)
        self._grid.setMovement(QListWidget.Static)
        self._grid.setIconSize(QSize(116, 116))
        self._grid.setGridSize(QSize(166, 154))
        self._grid.setSpacing(6)
        self._grid.setSelectionMode(QAbstractItemView.SingleSelection)
        self._grid.setVerticalScrollMode(QAbstractItemView.ScrollPerPixel)
        self._grid.setContextMenuPolicy(Qt.CustomContextMenu)
        self._grid.currentItemChanged.connect(self._on_album_changed)
        self._grid.itemDoubleClicked.connect(self._on_album_double_clicked)
        self._grid.customContextMenuRequested.connect(self._on_album_context_menu)
        splitter.addWidget(self._grid)

        self._track_model = TrackTableModel()
        self._track_model.set_artwork_manager(artwork_manager)
        self._track_table = TrackTable()
        self._track_table.setModel(self._track_model)
        self._track_table.track_double_clicked.connect(self.track_double_clicked)
        self._track_table.track_activated.connect(self.track_double_clicked)
        self._track_table.selection_changed.connect(self.selection_changed)
        splitter.addWidget(self._track_table)
        splitter.setSizes([330, 260])
        self._splitter = splitter

    def apply_theme_assets(self, theme_assets):
        self._album_placeholder_path = theme_assets.asset_path("album_placeholder")
        self._album_frame_path = theme_assets.asset_path("album_frame")

    def set_albums(self, album_groups):
        current_key = None
        current = self._grid.currentItem()
        if current:
            current_key = current.data(Qt.UserRole)

        self._albums = album_groups
        self._album_tracks = {
            album["key"]: [
                {**track, "album_group_key": album["group_key"]}
                for track in normalize_tracks_for_ui(album["tracks"])
            ]
            for album in album_groups
        }
        self._grid.clear()
        self._populate_index = 0
        self._populate_batch(current_key)

    def _populate_batch(self, preferred_key=None):
        batch = self._albums[self._populate_index:self._populate_index + 24]
        icon_extent = self._grid.iconSize().width()
        for album in batch:
            item = QListWidgetItem()
            item.setText(album["label"])
            item.setData(Qt.UserRole, album["key"])
            item.setToolTip(album["label"])
            item.setTextAlignment(Qt.AlignCenter)

            art_path = self._artwork.get_artwork_for_album(album, "display")
            if art_path:
                item.setIcon(QIcon(art_path))
            else:
                item.setIcon(QIcon(make_album_placeholder(icon_extent, self._album_placeholder_path, self._album_frame_path)))
            self._grid.addItem(item)

        self._populate_index += len(batch)
        if self._populate_index < len(self._albums):
            QTimer.singleShot(0, lambda: self._populate_batch(preferred_key))
            return

        row = 0
        if preferred_key is not None:
            for i in range(self._grid.count()):
                if self._grid.item(i).data(Qt.UserRole) == preferred_key:
                    row = i
                    break
        if self._grid.count():
            self._grid.setCurrentRow(row)
        else:
            self._track_model.set_tracks([])

    def _on_album_changed(self, current, previous):
        if not current:
            self._track_model.set_tracks([])
            return
        key = current.data(Qt.UserRole)
        tracks = self._album_tracks.get(key, [])
        self._track_model.set_tracks(normalize_tracks_for_ui(tracks))
        album = _row_dict(tracks[0]).get("album", "") if tracks else ""
        artist = _row_dict(tracks[0]).get("album_artist") or _row_dict(tracks[0]).get("artist", "") if tracks else ""
        self.album_selected.emit(artist, album)

    def current_tracks(self):
        return [self._track_model.track_at(i) for i in range(self._track_model.rowCount())]

    def set_current_track_id(self, track_id):
        self._track_model.set_current_track_id(track_id)

    def _on_album_double_clicked(self, item):
        if item:
            self._splitter.setSizes([150, 470])

    def _on_album_context_menu(self, pos):
        item = self._grid.itemAt(pos)
        if not item:
            return
        self._grid.setCurrentItem(item)
        key = item.data(Qt.UserRole)
        album = next((entry for entry in self._albums if entry["key"] == key), None)
        if album is None:
            return
        self.album_context_requested.emit(album, self._grid.viewport().mapToGlobal(pos))


def group_tracks_by_artist(tracks):
    groups = defaultdict(list)
    for row in normalize_tracks_for_ui(tracks):
        d = _row_dict(row)
        artist = d.get("album_artist") or d.get("artist") or "Unknown Artist"
        groups[artist].append(row)
    return dict(groups)


def group_tracks_by_genre(tracks):
    groups = defaultdict(list)
    for row in normalize_tracks_for_ui(tracks):
        d = _row_dict(row)
        genre = d.get("genre") or "Unknown Genre"
        groups[genre].append(row)
    return dict(groups)


def group_tracks_by_album(tracks):
    grouped = defaultdict(list)
    album_buckets = defaultdict(list)
    for row in normalize_tracks_for_ui(tracks):
        d = _row_dict(row)
        album_buckets[_normalize_album_text(d.get("album") or "Unknown Album")].append(row)

    for _album_key, bucket_tracks in album_buckets.items():
        canonical_artist_counts = defaultdict(int)
        canonical_labels = {}
        for row in bucket_tracks:
            key = album_group_key(row)
            canonical_artist_counts[key[1]] += 1
            canonical_labels.setdefault(key[1], album_grouping_artist(row))

        dominant_artist_key = ""
        if canonical_artist_counts:
            dominant_artist_key = sorted(
                canonical_artist_counts.items(),
                key=lambda item: (-item[1], canonical_labels[item[0]].casefold(), item[0]),
            )[0][0]

        for row in bucket_tracks:
            key = album_group_key(row)
            merge_key = key
            if dominant_artist_key:
                row_artist_key = key[1]
                if (
                    row_artist_key == dominant_artist_key
                    or not row_artist_key
                    or row_artist_key in dominant_artist_key
                    or dominant_artist_key in row_artist_key
                ):
                    merge_key = (key[0], dominant_artist_key)
            grouped[merge_key].append(row)

    albums = []
    for key, album_tracks in grouped.items():
        first = _row_dict(album_tracks[0])
        artist = album_grouping_artist(first)
        album = first.get("album") or "Unknown Album"
        source_artists = sorted(
            {
                (_row_dict(track).get("artist") or "").strip()
                for track in album_tracks
                if (_row_dict(track).get("artist") or "").strip()
            },
            key=lambda value: value.casefold(),
        )
        albums.append({
            "key": f"{key[1]}\0{key[0]}",
            "group_key": f"{key[1]}\0{key[0]}",
            "label": album_display_label(album, artist),
            "artist": artist,
            "album": album,
            "compilation": bool(first.get("compilation")),
            "source_artists": source_artists,
            "tracks": sorted(
                album_tracks,
                key=lambda row: (
                    _row_dict(row).get("disc_number") or 1,
                    _row_dict(row).get("track_number") or 0,
                    _row_dict(row).get("title") or "",
                ),
            ),
        })
    albums.sort(key=lambda item: (item["artist"].lower(), item["album"].lower()))
    return albums


def filter_tracks_for_search(tracks, query):
    """Filter normalized UI tracks by iTunes-style text search fields."""
    q = (query or "").strip().lower()
    normalized = normalize_tracks_for_ui(tracks)
    if not q:
        return normalized

    results = []
    for row in normalized:
        haystack = " ".join(
            str(row.get(key) or "")
            for key in ("title", "artist", "album", "album_artist", "genre", "year")
        ).lower()
        if q in haystack:
            results.append(row)
    return results


def summarize_tracks(tracks):
    normalized = normalize_tracks_for_ui(tracks)
    return {
        "count": len(normalized),
        "duration": sum((row.get("duration") or 0) for row in normalized),
        "size": sum((row.get("file_size") or 0) for row in normalized),
    }


def make_album_placeholder(size=88, asset_path="", frame_path=""):
    if asset_path:
        themed = QPixmap(asset_path)
        if not themed.isNull():
            px = themed.scaled(size, size, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            if frame_path:
                frame = QPixmap(frame_path)
                if not frame.isNull():
                    canvas = QPixmap(size, size)
                    canvas.fill(Qt.transparent)
                    painter = QPainter(canvas)
                    x = (size - px.width()) // 2
                    y = (size - px.height()) // 2
                    painter.drawPixmap(x, y, px)
                    painter.drawPixmap(0, 0, frame.scaled(size, size, Qt.IgnoreAspectRatio, Qt.SmoothTransformation))
                    painter.end()
                    return canvas
            return px

    px = QPixmap(size, size)
    px.fill(QColor("#d9d9d9"))
    painter = QPainter(px)
    painter.setRenderHint(QPainter.Antialiasing)
    painter.setPen(QColor("#a0a0a0"))
    painter.setBrush(QColor("#efefef"))
    painter.drawRect(8, 8, size - 16, size - 16)
    painter.setPen(QColor("#777777"))
    font = QFont()
    font.setPointSize(11)
    font.setBold(True)
    painter.setFont(font)
    painter.drawText(px.rect(), Qt.AlignCenter, "No\nArtwork")
    painter.end()
    return px
