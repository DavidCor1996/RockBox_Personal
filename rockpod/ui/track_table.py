"""iTunes 7-style track list table with sortable columns and artwork thumbnails."""

from PySide6.QtWidgets import (
    QTableView, QAbstractItemView, QHeaderView, QStyledItemDelegate,
    QStyle,
)
from PySide6.QtCore import (
    Qt, QAbstractTableModel, QModelIndex, QSize, Signal, QSortFilterProxyModel,
    QMimeData, QItemSelectionModel,
)
from PySide6.QtGui import QPixmap, QImage, QColor, QPainter, QFont, QDrag

from ui.track_adapter import normalize_track_for_ui, normalize_tracks_for_ui


# Column definitions matching iTunes 7 track list
COLUMNS = [
    {"key": "artwork",       "title": "",           "width": 28,  "sortable": False},
    {"key": "title",         "title": "Name",       "width": 230, "sortable": True},
    {"key": "artist",        "title": "Artist",     "width": 145, "sortable": True},
    {"key": "album",         "title": "Album",      "width": 145, "sortable": True},
    {"key": "duration_str",  "title": "Time",       "width": 50,  "sortable": True},
    {"key": "genre",         "title": "Genre",      "width": 82,  "sortable": True},
    {"key": "year",          "title": "Year",       "width": 42,  "sortable": True},
    {"key": "synced_to_device", "title": "On iPod",  "width": 48,  "sortable": True},
]

# Role for accessing the full track dict from a model item
TRACK_DATA_ROLE = Qt.UserRole + 1
TRACK_IDS_MIME = "application/x-rockpod-track-ids"


class TrackTableModel(QAbstractTableModel):
    """Model backing the track list table."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self._tracks = []
        self._artwork_cache = {}  # file_path -> QPixmap (thumb)
        self._artwork_manager = None
        self._current_track_id = None

    def set_artwork_manager(self, manager):
        self._artwork_manager = manager

    def set_tracks(self, tracks):
        """Replace all tracks. tracks is a list of sqlite3.Row or dicts."""
        self.beginResetModel()
        self._tracks = normalize_tracks_for_ui(tracks)
        self._artwork_cache.clear()
        self.endResetModel()

    def set_current_track_id(self, track_id):
        self._current_track_id = track_id
        if self._tracks:
            self.dataChanged.emit(
                self.index(0, 0),
                self.index(len(self._tracks) - 1, len(COLUMNS) - 1),
                [Qt.FontRole, Qt.ForegroundRole],
            )

    def track_at(self, row):
        if 0 <= row < len(self._tracks):
            return self._tracks[row]
        return None

    def rowCount(self, parent=QModelIndex()):
        return len(self._tracks)

    def columnCount(self, parent=QModelIndex()):
        return len(COLUMNS)

    def data(self, index, role=Qt.DisplayRole):
        if not index.isValid():
            return None

        row = index.row()
        col = index.column()
        if row < 0 or row >= len(self._tracks):
            return None

        track = normalize_track_for_ui(self._tracks[row])
        col_def = COLUMNS[col]
        key = col_def["key"]

        if role == Qt.DisplayRole:
            if key == "artwork":
                return None  # Handled by delegate
            if key == "synced_to_device":
                val = track.get("synced_to_device", False)
                return "Yes" if val else ""
            if key == "rating":
                val = track.get(key, 0)
                if val and val > 0:
                    return "\u2605" * val  # Star characters
                return ""
            if key in ("duration_str", "bitrate_str"):
                # These are computed properties; handle both dict and Track
                # Compute from raw values
                if key == "duration_str":
                    dur = track.get("duration", 0)
                    dur = dur or 0
                    mins = int(dur) // 60
                    secs = int(dur) % 60
                    return f"{mins}:{secs:02d}"
                if key == "bitrate_str":
                    br = track.get("bitrate", 0)
                    return f"{br} kbps" if br else ""
            val = track.get(key, "")
            if val is None:
                return ""
            return str(val)

        elif role == Qt.DecorationRole:
            if key == "artwork":
                return self._get_artwork_pixmap(track)

        elif role == Qt.TextAlignmentRole:
            if key in ("track_number", "year", "rating"):
                return Qt.AlignCenter
            if key in ("duration_str", "bitrate_str"):
                return Qt.AlignRight | Qt.AlignVCenter
            return Qt.AlignLeft | Qt.AlignVCenter

        elif role == TRACK_DATA_ROLE:
            return track

        elif role == Qt.ForegroundRole:
            if track.get("id") and track.get("id") == self._current_track_id:
                return QColor("#000000")
            if key in ("duration_str", "bitrate_str", "codec", "year"):
                return QColor("#666666")

        elif role == Qt.FontRole:
            if track.get("id") and track.get("id") == self._current_track_id:
                font = QFont()
                font.setBold(True)
                return font

        return None

    def headerData(self, section, orientation, role=Qt.DisplayRole):
        if orientation == Qt.Horizontal and role == Qt.DisplayRole:
            return COLUMNS[section]["title"]
        return None

    def sort(self, column, order):
        if column < 0 or column >= len(COLUMNS):
            return
        key = COLUMNS[column]["key"]
        if not COLUMNS[column]["sortable"]:
            return

        self.beginResetModel()
        reverse = (order == Qt.DescendingOrder)

        def sort_key(track):
            if key in ("duration_str", "bitrate_str"):
                # Sort by underlying numeric value
                actual_key = "duration" if key == "duration_str" else "bitrate"
                val = track.get(actual_key, 0)
                return val or 0
            val = track.get(key, "")
            if val is None:
                return ""
            if isinstance(val, str):
                return val.lower()
            return val

        self._tracks.sort(key=sort_key, reverse=reverse)
        self.endResetModel()

    def _get_artwork_pixmap(self, track):
        """Get a compact thumbnail pixmap for the artwork column."""
        if self._artwork_manager is None:
            return None

        track = normalize_track_for_ui(track)
        fp = track.get("file_path", "")
        if fp in self._artwork_cache:
            return self._artwork_cache[fp]

        path = self._artwork_manager.get_artwork_path(track, "thumb")
        if path:
            px = QPixmap(path)
            if not px.isNull():
                px = px.scaled(20, 20, Qt.KeepAspectRatio, Qt.SmoothTransformation)
                self._artwork_cache[fp] = px
                return px

        self._artwork_cache[fp] = None
        return None


class TrackTable(QTableView):
    """iTunes 7-era track list view with alternating rows and glossy headers."""

    track_double_clicked = Signal(object)  # track dict/row
    selection_changed = Signal(list)       # list of track dicts
    track_activated = Signal(object)       # Enter/Return activation

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("track_table")

        # Table behavior
        self.setAlternatingRowColors(True)
        self.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.setShowGrid(False)
        self.setSortingEnabled(True)
        self.setWordWrap(False)
        self.setVerticalScrollMode(QAbstractItemView.ScrollPerPixel)
        self.setHorizontalScrollMode(QAbstractItemView.ScrollPerPixel)

        # Enable drag for drag-to-playlist / drag-to-device
        self.setDragEnabled(True)
        self.setDragDropMode(QAbstractItemView.DragOnly)

        # Row height
        self.verticalHeader().setDefaultSectionSize(18)
        self.verticalHeader().setVisible(False)

        # Column headers
        h = self.horizontalHeader()
        h.setStretchLastSection(True)
        h.setHighlightSections(True)
        h.setSectionsMovable(True)
        h.setSortIndicatorShown(True)
        h.setDefaultAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        h.setDefaultSectionSize(110)

        self.doubleClicked.connect(self._on_double_click)

    def setModel(self, model):
        super().setModel(model)
        self._apply_column_widths()

    def _apply_column_widths(self):
        """Set initial column widths from the COLUMNS definition."""
        for i, col_def in enumerate(COLUMNS):
            self.setColumnWidth(i, col_def["width"])

    def _on_double_click(self, index):
        model = self.model()
        if model:
            track = model.data(index, TRACK_DATA_ROLE)
            if track:
                self.track_double_clicked.emit(track)

    def keyPressEvent(self, event):
        if event.key() in (Qt.Key_Return, Qt.Key_Enter):
            index = self.currentIndex()
            if index.isValid() and self.model():
                track = self.model().data(index, TRACK_DATA_ROLE)
                if track:
                    self.track_activated.emit(track)
                    return
        super().keyPressEvent(event)

    def selectionChanged(self, selected, deselected):
        super().selectionChanged(selected, deselected)
        tracks = self.get_selected_tracks()
        self.selection_changed.emit(tracks)

    def get_selected_tracks(self):
        """Return list of selected track data objects."""
        model = self.model()
        if not model:
            return []
        tracks = []
        for index in self.selectionModel().selectedRows():
            track = model.data(index, TRACK_DATA_ROLE)
            if track:
                tracks.append(track)
        return tracks

    def get_selected_track_ids(self):
        """Return set of selected track IDs."""
        ids = set()
        for track in self.get_selected_tracks():
            track = normalize_track_for_ui(track)
            tid = track.get("id")
            if tid:
                ids.add(tid)
        return ids

    def select_track_ids(self, track_ids):
        """Restore row selection from a set of track ids."""
        if not track_ids or not self.model():
            return
        selection = self.selectionModel()
        if selection is None:
            return
        first_index = None
        for row in range(self.model().rowCount()):
            track = self.model().track_at(row)
            tid = normalize_track_for_ui(track).get("id")
            if tid in track_ids:
                index = self.model().index(row, 0)
                selection.select(
                    index,
                    QItemSelectionModel.Select | QItemSelectionModel.Rows,
                )
                if first_index is None:
                    first_index = index
        if first_index is not None:
            self.setCurrentIndex(first_index)
            self.scrollTo(first_index, QAbstractItemView.PositionAtCenter)

    def startDrag(self, supported_actions):
        ids = sorted(self.get_selected_track_ids())
        if not ids:
            return

        mime = QMimeData()
        mime.setData(TRACK_IDS_MIME, ",".join(str(tid) for tid in ids).encode("utf-8"))
        drag = QDrag(self)
        drag.setMimeData(mime)
        drag.exec(Qt.CopyAction)
