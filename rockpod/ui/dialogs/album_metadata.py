"""Dialog for fetched online album metadata and lyrics."""

from PySide6.QtCore import Qt
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QTabWidget,
    QTextEdit,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
    QHeaderView,
)


class AlbumMetadataDialog(QDialog):
    """Read-only dialog for fetched online album metadata."""

    def __init__(self, metadata, artwork_path="", parent=None):
        super().__init__(parent)
        self._metadata = dict(metadata or {})
        self.setWindowTitle("Fetched Album Metadata")
        self.setMinimumSize(620, 500)
        self.setModal(True)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(8)

        tabs = QTabWidget()
        tabs.addTab(self._build_summary_tab(artwork_path), "Summary")
        tabs.addTab(self._build_tracks_tab(), "Tracks")
        tabs.addTab(self._build_lyrics_tab(), "Lyrics")
        layout.addWidget(tabs)

        buttons = QDialogButtonBox(QDialogButtonBox.Close)
        buttons.rejected.connect(self.reject)
        buttons.accepted.connect(self.accept)
        layout.addWidget(buttons)

    def _build_summary_tab(self, artwork_path):
        widget = QWidget()
        layout = QHBoxLayout(widget)

        art_label = QLabel()
        art_label.setFixedSize(144, 144)
        art_label.setAlignment(Qt.AlignCenter)
        art_label.setStyleSheet("background: #f0f0f0; border: 1px solid #cccccc;")
        if artwork_path:
            px = QPixmap(artwork_path)
            if not px.isNull():
                art_label.setPixmap(px.scaled(142, 142, Qt.KeepAspectRatio, Qt.SmoothTransformation))
            else:
                art_label.setText("No Art")
        else:
            art_label.setText("No Art")
        layout.addWidget(art_label)

        form = QFormLayout()
        form.setSpacing(4)
        values = [
            ("Album", self._metadata.get("album") or ""),
            ("Artist", self._metadata.get("artist") or ""),
            ("Release Date", _format_release_date(self._metadata.get("release_date"))),
            ("Year", str(self._metadata.get("year") or "")),
            ("Genre", self._metadata.get("primary_genre") or ""),
            ("Track Count", str(self._metadata.get("track_count") or len(self._tracks()))),
            ("Source", self._metadata.get("source") or ""),
            ("Store URL", self._metadata.get("collection_view_url") or ""),
            ("Copyright", self._metadata.get("copyright") or ""),
        ]
        for label, value in values:
            value_label = QLabel(str(value or ""))
            value_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
            value_label.setWordWrap(True)
            form.addRow(f"{label}:", value_label)
        layout.addLayout(form, 1)
        return widget

    def _build_tracks_tab(self):
        widget = QWidget()
        layout = QVBoxLayout(widget)
        tracks = self._tracks()
        table = QTableWidget(len(tracks), 7)
        table.setHorizontalHeaderLabels(
            ["Track", "Disc", "Title", "Artist", "Composer", "Duration", "Lyrics"]
        )
        table.setEditTriggers(QTableWidget.NoEditTriggers)
        table.setSelectionBehavior(QTableWidget.SelectRows)
        table.setSelectionMode(QTableWidget.SingleSelection)
        table.verticalHeader().setVisible(False)
        table.horizontalHeader().setSectionResizeMode(2, QHeaderView.Stretch)
        table.horizontalHeader().setSectionResizeMode(3, QHeaderView.Stretch)
        table.horizontalHeader().setSectionResizeMode(4, QHeaderView.Stretch)
        table.horizontalHeader().setStretchLastSection(True)

        for row_index, track in enumerate(tracks):
            values = [
                str(track.get("track_number") or ""),
                str(track.get("disc_number") or ""),
                str(track.get("title") or ""),
                str(track.get("artist") or ""),
                str(track.get("composer") or ""),
                _format_duration_millis(track.get("duration_millis") or 0),
                "Yes" if str(track.get("lyrics") or "").strip() else "",
            ]
            for col_index, value in enumerate(values):
                item = QTableWidgetItem(value)
                item.setFlags(item.flags() & ~Qt.ItemIsEditable)
                table.setItem(row_index, col_index, item)
        layout.addWidget(table)
        return widget

    def _build_lyrics_tab(self):
        widget = QWidget()
        layout = QVBoxLayout(widget)
        self._lyrics_track_picker = QComboBox()
        self._lyrics_track_picker.currentIndexChanged.connect(self._load_selected_track_lyrics)
        for index, track in enumerate(self._tracks(), start=1):
            label = f"{track.get('track_number') or index}. {track.get('title') or 'Untitled'}"
            self._lyrics_track_picker.addItem(label, track)
        layout.addWidget(self._lyrics_track_picker)

        self._lyrics_text = QTextEdit()
        self._lyrics_text.setReadOnly(True)
        self._lyrics_text.setPlaceholderText("No lyrics found for this track.")
        layout.addWidget(self._lyrics_text, 1)

        if self._tracks():
            self._load_selected_track_lyrics(0)
        else:
            self._lyrics_text.setPlainText("No tracks available.")
            self._lyrics_track_picker.setEnabled(False)
        return widget

    def _load_selected_track_lyrics(self, index):
        track = self._lyrics_track_picker.itemData(index) if index >= 0 else None
        if not track:
            self._lyrics_text.setPlainText("No tracks available.")
            return
        lyrics = str(track.get("lyrics") or "").strip()
        if lyrics:
            self._lyrics_text.setPlainText(lyrics)
        else:
            title = track.get("title") or "this track"
            self._lyrics_text.setPlainText(f"No lyrics found for {title}.")

    def _tracks(self):
        return list(self._metadata.get("tracks") or [])


def _format_release_date(value):
    text = str(value or "").strip()
    if "T" in text:
        text = text.split("T", 1)[0]
    return text


def _format_duration_millis(duration_millis):
    total = max(0, int(duration_millis or 0) // 1000)
    minutes, seconds = divmod(total, 60)
    hours, minutes = divmod(minutes, 60)
    if hours:
        return f"{hours}:{minutes:02d}:{seconds:02d}"
    return f"{minutes}:{seconds:02d}"
