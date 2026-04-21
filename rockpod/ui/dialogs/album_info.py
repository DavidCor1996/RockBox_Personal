"""Read-only album info dialog for the album browser."""

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

from services.metadata_reader import read_lyrics
from ui.track_adapter import normalize_tracks_for_ui


def summarize_album(album):
    tracks = normalize_tracks_for_ui((album or {}).get("tracks") or [])
    first = tracks[0] if tracks else {}
    years = sorted({int(track.get("year") or 0) for track in tracks if track.get("year")})
    genres = sorted({str(track.get("genre") or "").strip() for track in tracks if str(track.get("genre") or "").strip()}, key=str.casefold)
    codecs = sorted({str(track.get("codec") or "").strip() for track in tracks if str(track.get("codec") or "").strip()}, key=str.casefold)
    source_artists = list((album or {}).get("source_artists") or [])
    disc_numbers = {int(track.get("disc_number") or 1) for track in tracks}
    total_duration = sum(float(track.get("duration") or 0) for track in tracks)
    total_size = sum(int(track.get("file_size") or 0) for track in tracks)
    return {
        "album": (album or {}).get("album") or first.get("album") or "Unknown Album",
        "album_artist": (album or {}).get("artist") or first.get("album_artist") or first.get("artist") or "Unknown Artist",
        "track_count": len(tracks),
        "disc_count": max(len(disc_numbers), int(bool(tracks))),
        "years": years,
        "genres": genres,
        "codecs": codecs,
        "source_artists": source_artists,
        "compilation": bool((album or {}).get("compilation") or first.get("compilation")),
        "duration_seconds": total_duration,
        "size_bytes": total_size,
        "tracks": tracks,
    }


class AlbumInfoDialog(QDialog):
    """Read-only dialog for album metadata and track listing."""

    def __init__(self, album, artwork_path="", parent=None):
        super().__init__(parent)
        self._album = dict(album or {})
        self._summary = summarize_album(self._album)
        self.setWindowTitle("Album Info")
        self.setMinimumSize(560, 460)
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
        buttons.button(QDialogButtonBox.Close).setDefault(True)
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
        for label, value in [
            ("Album", self._summary["album"]),
            ("Album Artist", self._summary["album_artist"]),
            ("Track Count", str(self._summary["track_count"])),
            ("Disc Count", str(self._summary["disc_count"])),
            ("Compilation", "Yes" if self._summary["compilation"] else "No"),
            ("Years", _format_joined_values(self._summary["years"])),
            ("Genres", _format_joined_values(self._summary["genres"])),
            ("Codecs", _format_joined_values(self._summary["codecs"])),
            ("Track Artists", _format_joined_values(self._summary["source_artists"])),
            ("Duration", _format_duration(self._summary["duration_seconds"])),
            ("Size", _format_size(self._summary["size_bytes"])),
        ]:
            value_label = QLabel(value)
            value_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
            value_label.setWordWrap(True)
            form.addRow(f"{label}:", value_label)
        layout.addLayout(form, 1)
        return widget

    def _build_tracks_tab(self):
        widget = QWidget()
        layout = QVBoxLayout(widget)
        table = QTableWidget(len(self._summary["tracks"]), 10)
        table.setHorizontalHeaderLabels(
            ["Track", "Disc", "Title", "Artist", "Album Artist", "Year", "Genre", "Duration", "Codec", "File"]
        )
        table.setEditTriggers(QTableWidget.NoEditTriggers)
        table.setSelectionBehavior(QTableWidget.SelectRows)
        table.setSelectionMode(QTableWidget.SingleSelection)
        table.verticalHeader().setVisible(False)
        table.horizontalHeader().setStretchLastSection(True)
        table.horizontalHeader().setSectionResizeMode(2, QHeaderView.Stretch)
        table.horizontalHeader().setSectionResizeMode(3, QHeaderView.Stretch)
        table.horizontalHeader().setSectionResizeMode(4, QHeaderView.Stretch)
        table.horizontalHeader().setSectionResizeMode(9, QHeaderView.Stretch)

        for row_index, track in enumerate(self._summary["tracks"]):
            values = [
                str(track.get("track_number") or ""),
                str(track.get("disc_number") or ""),
                str(track.get("title") or ""),
                str(track.get("artist") or track.get("album_artist") or ""),
                str(track.get("album_artist") or ""),
                str(track.get("year") or ""),
                str(track.get("genre") or ""),
                _format_duration(track.get("duration") or 0),
                str(track.get("codec") or ""),
                str(track.get("file_path") or ""),
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
        for index, track in enumerate(self._summary["tracks"], start=1):
            label = f"{track.get('track_number') or index}. {track.get('title') or 'Untitled'}"
            self._lyrics_track_picker.addItem(label, track)
        layout.addWidget(self._lyrics_track_picker)

        self._lyrics_text = QTextEdit()
        self._lyrics_text.setReadOnly(True)
        self._lyrics_text.setPlaceholderText("No lyrics found for this track.")
        layout.addWidget(self._lyrics_text, 1)

        if self._summary["tracks"]:
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
        lyrics = read_lyrics(track.get("file_path") or "")
        if lyrics:
            self._lyrics_text.setPlainText(lyrics)
        else:
            title = track.get("title") or "this track"
            self._lyrics_text.setPlainText(f"No lyrics found for {title}.")


def _format_joined_values(values):
    cleaned = [str(value) for value in (values or []) if str(value)]
    return ", ".join(cleaned) if cleaned else ""


def _format_duration(seconds):
    total = max(0, int(seconds or 0))
    hours, remainder = divmod(total, 3600)
    minutes, secs = divmod(remainder, 60)
    if hours:
        return f"{hours}:{minutes:02d}:{secs:02d}"
    return f"{minutes}:{secs:02d}"


def _format_size(size_bytes):
    size = float(size_bytes or 0)
    if size >= 1024 ** 3:
        return f"{size / (1024 ** 3):.2f} GB"
    if size >= 1024 ** 2:
        return f"{size / (1024 ** 2):.1f} MB"
    if size >= 1024:
        return f"{size / 1024:.1f} KB"
    return f"{int(size)} B"
