"""iTunes 7-style metadata editor dialog (Get Info)."""

from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QFormLayout, QLabel, QLineEdit,
    QSpinBox, QComboBox, QTextEdit, QCheckBox, QPushButton, QTabWidget,
    QWidget, QGroupBox, QDialogButtonBox,
)
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QPixmap


class MetadataEditor(QDialog):
    """Get Info dialog for editing track metadata, styled after iTunes 7."""

    metadata_saved = Signal(int, dict)  # track_id, updated_fields

    def __init__(self, track_row, artwork_path=None, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Track Info")
        self.setMinimumSize(480, 420)
        self.setModal(True)

        self._track = dict(track_row) if hasattr(track_row, "keys") else track_row
        self._track_id = self._track.get("id")
        self._is_video = str(self._track.get("media_type") or "audio") == "video"

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(8)

        # Tab widget
        tabs = QTabWidget()
        layout.addWidget(tabs)

        # Summary tab
        summary_tab = self._build_summary_tab(artwork_path)
        tabs.addTab(summary_tab, "Summary")

        # Info tab (editable fields)
        info_tab = self._build_info_tab()
        tabs.addTab(info_tab, "Info")

        # Details tab
        details_tab = self._build_details_tab()
        tabs.addTab(details_tab, "Details")

        # Button box
        btn_box = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        btn_box.accepted.connect(self._on_ok)
        btn_box.rejected.connect(self.reject)
        btn_box.button(QDialogButtonBox.Ok).setDefault(True)
        layout.addWidget(btn_box)

    def _build_summary_tab(self, artwork_path):
        w = QWidget()
        layout = QHBoxLayout(w)

        # Artwork
        art_label = QLabel()
        art_label.setFixedSize(120, 120)
        art_label.setAlignment(Qt.AlignCenter)
        art_label.setStyleSheet("background: #f0f0f0; border: 1px solid #cccccc;")
        if artwork_path:
            px = QPixmap(artwork_path)
            if not px.isNull():
                art_label.setPixmap(px.scaled(118, 118, Qt.KeepAspectRatio, Qt.SmoothTransformation))
            else:
                art_label.setText("No Art")
        else:
            art_label.setText("No Art")
        layout.addWidget(art_label)

        # Info column
        info_layout = QFormLayout()
        info_layout.setSpacing(4)

        fields = [
            ("title", "Name"),
            ("artist", "Artist"),
            ("album", "Album"),
            ("album_artist", "Album Artist"),
            ("genre", "Genre"),
            ("year", "Year"),
        ]
        if self._is_video:
            fields.extend([
                ("video_kind", "Video Type"),
                ("show_title", "Show"),
                ("season_number", "Season"),
                ("episode_number", "Episode"),
            ])
        fields.append(("codec", "Kind"))

        for field, label in fields:
            val = str(self._track.get(field, "") or "")
            lbl = QLabel(val)
            lbl.setTextInteractionFlags(Qt.TextSelectableByMouse)
            info_layout.addRow(f"{label}:", lbl)

        # Duration and bitrate
        dur = self._track.get("duration", 0) or 0
        mins = int(dur) // 60
        secs = int(dur) % 60
        info_layout.addRow("Duration:", QLabel(f"{mins}:{secs:02d}"))

        br = self._track.get("bitrate", 0) or 0
        info_layout.addRow("Bit Rate:", QLabel(f"{br} kbps" if br else ""))

        layout.addLayout(info_layout)
        return w

    def _build_info_tab(self):
        w = QWidget()
        layout = QFormLayout(w)
        layout.setSpacing(6)

        self._title_edit = QLineEdit(str(self._track.get("title", "") or ""))
        layout.addRow("Name:", self._title_edit)

        self._artist_edit = QLineEdit(str(self._track.get("artist", "") or ""))
        layout.addRow("Artist:", self._artist_edit)

        self._album_edit = QLineEdit(str(self._track.get("album", "") or ""))
        layout.addRow("Album:", self._album_edit)

        self._album_artist_edit = QLineEdit(str(self._track.get("album_artist", "") or ""))
        layout.addRow("Album Artist:", self._album_artist_edit)

        self._genre_edit = QLineEdit(str(self._track.get("genre", "") or ""))
        layout.addRow("Genre:", self._genre_edit)

        self._composer_edit = QLineEdit(str(self._track.get("composer", "") or ""))
        layout.addRow("Composer:", self._composer_edit)

        self._year_spin = QSpinBox()
        self._year_spin.setRange(0, 9999)
        self._year_spin.setValue(int(self._track.get("year") or 0))
        self._year_spin.setSpecialValueText("")
        layout.addRow("Year:", self._year_spin)

        track_row = QHBoxLayout()
        self._track_num_spin = QSpinBox()
        self._track_num_spin.setRange(0, 999)
        self._track_num_spin.setValue(int(self._track.get("track_number") or 0))
        self._track_num_spin.setSpecialValueText("")
        track_row.addWidget(self._track_num_spin)
        track_row.addWidget(QLabel("of"))
        self._track_total_spin = QSpinBox()
        self._track_total_spin.setRange(0, 999)
        self._track_total_spin.setValue(int(self._track.get("track_total") or 0))
        self._track_total_spin.setSpecialValueText("")
        track_row.addWidget(self._track_total_spin)
        layout.addRow("Track Number:" if not self._is_video else "Track/Episode:", track_row)

        self._video_kind_combo = None
        self._show_title_edit = None
        self._season_spin = None
        self._episode_spin = None
        if self._is_video:
            self._video_kind_combo = QComboBox()
            self._video_kind_combo.addItem("", "")
            self._video_kind_combo.addItem("TV Show", "show")
            self._video_kind_combo.addItem("Movie", "movie")
            self._video_kind_combo.addItem("Home Video", "home_video")
            current_kind = str(self._track.get("video_kind", "") or "")
            index = max(self._video_kind_combo.findData(current_kind), 0)
            self._video_kind_combo.setCurrentIndex(index)
            layout.addRow("Video Type:", self._video_kind_combo)

            self._show_title_edit = QLineEdit(str(self._track.get("show_title", "") or ""))
            layout.addRow("Show:", self._show_title_edit)

            self._season_spin = QSpinBox()
            self._season_spin.setRange(0, 999)
            self._season_spin.setValue(int(self._track.get("season_number") or 0))
            self._season_spin.setSpecialValueText("")
            layout.addRow("Season:", self._season_spin)

            self._episode_spin = QSpinBox()
            self._episode_spin.setRange(0, 9999)
            self._episode_spin.setValue(int(self._track.get("episode_number") or 0))
            self._episode_spin.setSpecialValueText("")
            layout.addRow("Episode:", self._episode_spin)

        disc_row = QHBoxLayout()
        self._disc_num_spin = QSpinBox()
        self._disc_num_spin.setRange(0, 99)
        self._disc_num_spin.setValue(int(self._track.get("disc_number") or 1))
        disc_row.addWidget(self._disc_num_spin)
        disc_row.addWidget(QLabel("of"))
        self._disc_total_spin = QSpinBox()
        self._disc_total_spin.setRange(0, 99)
        self._disc_total_spin.setValue(int(self._track.get("disc_total") or 0))
        self._disc_total_spin.setSpecialValueText("")
        disc_row.addWidget(self._disc_total_spin)
        if not self._is_video:
            layout.addRow("Disc Number:", disc_row)

        self._compilation_check = QCheckBox("Part of a compilation")
        self._compilation_check.setChecked(bool(self._track.get("compilation")))
        if not self._is_video:
            layout.addRow("", self._compilation_check)

        self._comment_edit = QLineEdit(str(self._track.get("comment", "") or ""))
        layout.addRow("Comments:", self._comment_edit)

        self._rating_spin = QSpinBox()
        self._rating_spin.setRange(0, 5)
        self._rating_spin.setValue(int(self._track.get("rating") or 0))
        layout.addRow("Rating:", self._rating_spin)

        return w

    def _build_details_tab(self):
        w = QWidget()
        layout = QFormLayout(w)
        layout.setSpacing(6)

        info = QLabel(
            "Library metadata can be edited here. Technical file properties stay read-only."
        )
        info.setWordWrap(True)
        layout.addRow(info)

        self._play_count_spin = QSpinBox()
        self._play_count_spin.setRange(0, 999999)
        self._play_count_spin.setValue(int(self._track.get("play_count") or 0))
        layout.addRow("Play Count:", self._play_count_spin)

        self._last_played_edit = QLineEdit(str(self._track.get("last_played", "") or ""))
        self._last_played_edit.setPlaceholderText("YYYY-MM-DD HH:MM:SS")
        layout.addRow("Last Played:", self._last_played_edit)

        self._date_added_edit = QLineEdit(str(self._track.get("date_added", "") or ""))
        self._date_added_edit.setPlaceholderText("YYYY-MM-DD HH:MM:SS")
        layout.addRow("Date Added:", self._date_added_edit)

        layout.addRow("File:", QLabel(str(self._track.get("file_path", ""))))
        layout.addRow("Device Path:", QLabel(str(self._track.get("device_path", "") or "")))
        layout.addRow("Codec:", QLabel(str(self._track.get("codec", ""))))

        br = self._track.get("bitrate", 0)
        layout.addRow("Bit Rate:", QLabel(f"{br} kbps" if br else ""))

        sr = self._track.get("sample_rate", 0)
        layout.addRow("Sample Rate:", QLabel(f"{sr} Hz" if sr else ""))

        channels = self._track.get("channels", 0)
        layout.addRow("Channels:", QLabel(str(channels or "")))

        dur = self._track.get("duration", 0) or 0
        mins = int(dur) // 60
        secs = int(dur) % 60
        layout.addRow("Duration:", QLabel(f"{mins}:{secs:02d}" if dur else ""))

        fs = self._track.get("file_size", 0) or 0
        mb = fs / (1024 * 1024)
        layout.addRow("Size:", QLabel(f"{mb:.1f} MB" if mb > 0 else ""))

        return w

    def _on_ok(self):
        updates = {}
        for field, widget in [
            ("title", self._title_edit),
            ("artist", self._artist_edit),
            ("album", self._album_edit),
            ("album_artist", self._album_artist_edit),
            ("genre", self._genre_edit),
            ("composer", self._composer_edit),
            ("comment", self._comment_edit),
            ("last_played", self._last_played_edit),
            ("date_added", self._date_added_edit),
        ]:
            new_val = widget.text()
            old_val = str(self._track.get(field, "") or "")
            if new_val != old_val:
                updates[field] = new_val

        if self._is_video:
            for field, widget in [
                ("show_title", self._show_title_edit),
            ]:
                new_val = widget.text()
                old_val = str(self._track.get(field, "") or "")
                if new_val != old_val:
                    updates[field] = new_val

            new_kind = self._video_kind_combo.currentData()
            old_kind = str(self._track.get("video_kind", "") or "")
            if new_kind != old_kind:
                updates["video_kind"] = new_kind

        for field, widget in [
            ("year", self._year_spin),
            ("track_number", self._track_num_spin),
            ("track_total", self._track_total_spin),
            ("disc_number", self._disc_num_spin),
            ("disc_total", self._disc_total_spin),
            ("rating", self._rating_spin),
            ("play_count", self._play_count_spin),
        ]:
            new_val = widget.value()
            old_val = int(self._track.get(field) or 0)
            if new_val != old_val:
                updates[field] = new_val

        if self._is_video:
            for field, widget in [
                ("season_number", self._season_spin),
                ("episode_number", self._episode_spin),
            ]:
                new_val = widget.value()
                old_val = int(self._track.get(field) or 0)
                if new_val != old_val:
                    updates[field] = new_val

        if not self._is_video:
            comp = 1 if self._compilation_check.isChecked() else 0
            if comp != int(self._track.get("compilation", 0)):
                updates["compilation"] = comp

        if updates and self._track_id:
            self.metadata_saved.emit(self._track_id, updates)

        self.accept()
