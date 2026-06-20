"""iTunes 7-style toolbar with grouped transport, device, and search controls."""

from PySide6.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout, QLabel, QPushButton, QLineEdit,
    QSizePolicy, QSlider, QFrame,
)
from PySide6.QtCore import QPoint, Qt, Signal, QSize
from PySide6.QtGui import QColor, QBrush, QFont, QFontMetrics, QIcon, QPainter, QPen, QPixmap, QPolygon

from ui.track_adapter import normalize_track_for_ui


class Toolbar(QWidget):
    """iTunes 7-era toolbar with compact grouped controls."""

    search_changed = Signal(str)
    scan_clicked = Signal()
    sync_clicked = Signal()
    fetch_artwork_clicked = Signal()
    new_playlist_clicked = Signal()
    preferences_clicked = Signal()
    eject_clicked = Signal()
    play_pause_clicked = Signal()
    next_clicked = Signal()
    previous_clicked = Signal()
    seek_requested = Signal(int)
    volume_changed = Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("toolbar_container")
        self.setFixedHeight(44)
        self._theme_assets = None
        self._playback_state = "stopped"
        self._current_art_pixmap = None

        root = QHBoxLayout(self)
        root.setContentsMargins(6, 2, 6, 2)
        root.setSpacing(0)

        self._left_group = self._group_container("toolbar_group_left")
        left_layout = self._left_group.layout()
        left_layout.setSpacing(2)

        self._scan_btn = QPushButton("Refresh")
        self._scan_btn.setObjectName("scan_button")
        self._scan_btn.setToolTip("Refresh your Music folder for new, changed, and removed tracks")
        self._scan_btn.clicked.connect(self.scan_clicked)

        self._sync_btn = QPushButton("Sync to iPod")
        self._sync_btn.setObjectName("sync_button")
        self._sync_btn.setToolTip("Sync missing tracks to connected iPod")
        self._sync_btn.clicked.connect(self.sync_clicked)

        self._fetch_art_btn = QPushButton("Fetch Artwork")
        self._fetch_art_btn.setObjectName("toolbar_button")
        self._fetch_art_btn.setToolTip("Fetch missing album artwork slowly in the background")
        self._fetch_art_btn.clicked.connect(self.fetch_artwork_clicked)

        self._eject_btn = QPushButton("Eject")
        self._eject_btn.setObjectName("eject_button")
        self._eject_btn.setToolTip("Safely eject the iPod")
        self._eject_btn.clicked.connect(self.eject_clicked)
        self._eject_btn.setVisible(False)

        self._new_playlist_btn = QPushButton("+")
        self._new_playlist_btn.setObjectName("new_playlist_button")
        self._new_playlist_btn.setFixedSize(18, 18)
        self._new_playlist_btn.setToolTip("Create a new playlist")
        self._new_playlist_btn.clicked.connect(self.new_playlist_clicked)

        for button in (self._scan_btn, self._sync_btn, self._fetch_art_btn, self._new_playlist_btn, self._eject_btn):
            left_layout.addWidget(button)
        left_layout.addStretch(0)

        root.addWidget(self._left_group, 0, Qt.AlignLeft | Qt.AlignVCenter)
        root.addWidget(self._separator())

        center_shell = QWidget()
        center_shell.setObjectName("toolbar_center_shell")
        center_shell_layout = QHBoxLayout(center_shell)
        center_shell_layout.setContentsMargins(0, 0, 0, 0)
        center_shell_layout.setSpacing(0)
        center_shell_layout.addStretch(1)

        self._center_group = QWidget()
        self._center_group.setObjectName("toolbar_group_center")
        center_layout = QHBoxLayout(self._center_group)
        center_layout.setContentsMargins(18, 0, 24, 0)
        center_layout.setSpacing(7)

        transport = QWidget()
        transport.setObjectName("transport_group")
        transport_layout = QHBoxLayout(transport)
        transport_layout.setContentsMargins(0, 0, 0, 0)
        transport_layout.setSpacing(2)

        self._prev_btn = QPushButton("")
        self._prev_btn.setObjectName("playback_button")
        self._prev_btn.setFixedSize(26, 22)
        self._prev_btn.setToolTip("Previous")
        self._prev_btn.clicked.connect(self.previous_clicked)

        self._play_btn = QPushButton("")
        self._play_btn.setObjectName("playback_button")
        self._play_btn.setFixedSize(29, 24)
        self._play_btn.setToolTip("Play/Pause")
        self._play_btn.clicked.connect(self.play_pause_clicked)

        self._next_btn = QPushButton("")
        self._next_btn.setObjectName("playback_button")
        self._next_btn.setFixedSize(26, 22)
        self._next_btn.setToolTip("Next")
        self._next_btn.clicked.connect(self.next_clicked)

        for button in (self._prev_btn, self._play_btn, self._next_btn):
            transport_layout.addWidget(button)

        center_layout.addWidget(transport, 0, Qt.AlignVCenter)

        self._playback_cluster = QWidget()
        self._playback_cluster.setObjectName("playback_cluster")
        self._playback_cluster.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Preferred)
        self._playback_cluster.setMinimumWidth(430)
        self._playback_cluster.setMaximumWidth(520)
        cluster_layout = QHBoxLayout(self._playback_cluster)
        cluster_layout.setContentsMargins(8, 0, 18, 0)
        cluster_layout.setSpacing(5)
        cluster_layout.addStretch(5)

        self._selected_art = QLabel()
        self._selected_art.setObjectName("selected_art")
        self._selected_art.setFixedSize(24, 24)
        self._selected_art.setAlignment(Qt.AlignCenter)
        self._selected_art.setVisible(False)
        cluster_layout.addWidget(self._selected_art, 0, Qt.AlignVCenter)

        info_stack = QWidget()
        info_stack.setObjectName("now_playing_group")
        info_layout = QVBoxLayout(info_stack)
        info_layout.setContentsMargins(0, 0, 0, 0)
        info_layout.setSpacing(1)

        text_row = QWidget()
        text_row.setObjectName("now_playing_text_row")
        text_row_layout = QHBoxLayout(text_row)
        text_row_layout.setContentsMargins(0, 0, 0, 0)
        text_row_layout.setSpacing(5)

        text_column = QWidget()
        text_column.setObjectName("now_playing_text")
        text_column_layout = QVBoxLayout(text_column)
        text_column_layout.setContentsMargins(0, 0, 0, 0)
        text_column_layout.setSpacing(0)

        self._selected_title = QLabel("")
        self._selected_title.setObjectName("selected_title")
        self._selected_title.setFixedWidth(252)
        self._selected_artist = QLabel("")
        self._selected_artist.setObjectName("selected_artist")
        self._selected_artist.setFixedWidth(252)
        text_column_layout.addWidget(self._selected_title)
        text_column_layout.addWidget(self._selected_artist)
        text_row_layout.addWidget(text_column, 0, Qt.AlignVCenter)

        self._title_logo = QLabel()
        self._title_logo.setObjectName("title_logo")
        self._title_logo.setAlignment(Qt.AlignCenter)
        self._title_logo.setVisible(False)
        self._title_logo.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Preferred)
        text_row_layout.addWidget(self._title_logo, 0, Qt.AlignVCenter)

        self._title_label = QLabel("RockPod")
        title_font = QFont()
        title_font.setPointSize(11)
        title_font.setBold(True)
        self._title_label.setFont(title_font)
        self._title_label.setAlignment(Qt.AlignCenter)
        self._title_label.setObjectName("toolbar_title")
        self._title_label.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Preferred)
        text_row_layout.addWidget(self._title_label, 0, Qt.AlignVCenter)
        text_row_layout.addStretch(0)
        info_layout.addWidget(text_row)

        progress_row = QWidget()
        progress_row.setObjectName("playback_progress_group")
        progress_layout = QHBoxLayout(progress_row)
        progress_layout.setContentsMargins(4, 1, 4, 0)
        progress_layout.setSpacing(4)
        progress_layout.addStretch(1)

        self._progress = QSlider(Qt.Horizontal)
        self._progress.setObjectName("playback_progress")
        self._progress.setRange(0, 1000)
        self._progress.setFixedWidth(368)
        self._progress.sliderReleased.connect(self._on_seek_released)
        progress_layout.addWidget(self._progress, 0)

        self._time_label = QLabel("0:00 / 0:00")
        self._time_label.setObjectName("selected_time")
        self._time_label.setFixedWidth(62)
        progress_layout.addWidget(self._time_label, 0, Qt.AlignRight | Qt.AlignVCenter)
        progress_layout.addStretch(1)
        info_layout.addWidget(progress_row)

        cluster_layout.addWidget(info_stack, 0)
        cluster_layout.addStretch(5)
        center_layout.addWidget(self._playback_cluster, 0, Qt.AlignVCenter)
        center_shell_layout.addWidget(self._center_group, 0, Qt.AlignCenter)
        center_shell_layout.addStretch(1)

        root.addWidget(center_shell, 1)
        root.addWidget(self._separator())

        self._right_group = self._group_container("toolbar_group_right")
        right_layout = self._right_group.layout()
        right_layout.setSpacing(2)

        self._volume = QSlider(Qt.Horizontal)
        self._volume.setObjectName("volume_slider")
        self._volume.setRange(0, 100)
        self._volume.setValue(70)
        self._volume.setFixedWidth(62)
        self._volume.valueChanged.connect(self.volume_changed)
        right_layout.addWidget(self._volume, 0, Qt.AlignVCenter)

        self._prefs_btn = QPushButton("Preferences")
        self._prefs_btn.setObjectName("prefs_button")
        self._prefs_btn.clicked.connect(self.preferences_clicked)
        right_layout.addWidget(self._prefs_btn, 0, Qt.AlignVCenter)

        self._search = QLineEdit()
        self._search.setObjectName("search_field")
        self._search.setPlaceholderText("Search")
        self._search.setClearButtonEnabled(True)
        self._search.setFixedWidth(152)
        self._search.textChanged.connect(self._on_search_changed)
        right_layout.addWidget(self._search, 0, Qt.AlignVCenter)

        root.addWidget(self._right_group, 0, Qt.AlignRight | Qt.AlignVCenter)
        self._set_center_active(False)

    def _group_container(self, object_name):
        group = QWidget()
        group.setObjectName(object_name)
        layout = QHBoxLayout(group)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        return group

    def _separator(self):
        line = QFrame()
        line.setObjectName("toolbar_separator")
        line.setFrameShape(QFrame.VLine)
        line.setFrameShadow(QFrame.Plain)
        line.setFixedWidth(1)
        return line

    def _on_search_changed(self, text):
        self.search_changed.emit(text)

    def set_title(self, text):
        self._title_label.setText(text)

    def set_sync_enabled(self, enabled):
        self._sync_btn.setEnabled(enabled)

    def set_eject_visible(self, visible):
        self._eject_btn.setVisible(visible)

    def set_scanning(self, scanning):
        if scanning:
            self._scan_btn.setText("Refreshing...")
            self._scan_btn.setEnabled(False)
        else:
            self._scan_btn.setText("Refresh")
            self._scan_btn.setEnabled(True)

    def set_syncing(self, syncing):
        if syncing:
            self._sync_btn.setText("Syncing...")
            self._sync_btn.setEnabled(False)
        else:
            self._sync_btn.setText("Sync to iPod")
            self._sync_btn.setEnabled(True)

    def clear_search(self):
        self._search.clear()

    def apply_theme_assets(self, theme_assets):
        self._theme_assets = theme_assets
        self._apply_button_icon(self._sync_btn, theme_assets.asset_path("toolbar_sync"), "Sync to iPod")
        self._apply_button_icon(self._scan_btn, theme_assets.asset_path("toolbar_refresh"), "Refresh")
        self._apply_button_icon(self._new_playlist_btn, theme_assets.asset_path("toolbar_new_playlist"), "+")
        self._apply_transport_icon(self._prev_btn, "previous")
        self._apply_transport_icon(self._play_btn, "pause" if self._playback_state == "playing" else "play")
        self._apply_transport_icon(self._next_btn, "next")

        logo_path = theme_assets.asset_path("branding_title")
        if logo_path:
            px = QPixmap(logo_path)
            if not px.isNull():
                scaled = px.scaledToHeight(18, Qt.SmoothTransformation)
                self._title_logo.setPixmap(scaled)
                self._title_logo.setFixedWidth(scaled.width())
                self._title_logo.setVisible(True)
                self._title_label.setVisible((scaled.width() / max(1, scaled.height())) < 2.2)
                return
        self._title_logo.clear()
        self._title_logo.setVisible(False)
        self._title_label.setVisible(True)

    def set_playback_state(self, state):
        self._playback_state = state or "stopped"
        self._apply_transport_icon(self._play_btn, "pause" if state == "playing" else "play")
        active = state in {"playing", "paused"}
        self._set_center_active(active)
        self._selected_art.setVisible(active and self._current_art_pixmap is not None)

    def set_playback_position(self, position_ms, duration_ms):
        duration_ms = max(0, int(duration_ms or 0))
        position_ms = max(0, int(position_ms or 0))
        if duration_ms > 0:
            self._progress.blockSignals(True)
            self._progress.setValue(min(1000, int(position_ms * 1000 / duration_ms)))
            self._progress.blockSignals(False)
        else:
            self._progress.blockSignals(True)
            self._progress.setValue(0)
            self._progress.blockSignals(False)
        time_text = f"{_format_time(position_ms)} / {_format_time(duration_ms)}"
        metrics = QFontMetrics(self._time_label.font())
        self._time_label.setText(metrics.elidedText(time_text, Qt.ElideRight, self._time_label.width()))

    def set_volume(self, volume):
        self._volume.blockSignals(True)
        self._volume.setValue(max(0, min(100, int(volume))))
        self._volume.blockSignals(False)

    def set_selected_track(self, track=None, artwork_path=None):
        if not track:
            self._selected_title.setText("")
            self._selected_artist.setText("")
            self._time_label.setText("")
            self._current_art_pixmap = None
            self._selected_art.clear()
            self._selected_art.setVisible(False)
            self._set_center_active(False)
            return

        d = normalize_track_for_ui(track)
        title = d.get("title") or "Untitled"
        artist = d.get("artist") or d.get("album_artist") or "Unknown Artist"
        album = d.get("album") or ""
        title_metrics = QFontMetrics(self._selected_title.font())
        artist_metrics = QFontMetrics(self._selected_artist.font())
        self._selected_title.setText(title_metrics.elidedText(title, Qt.ElideRight, self._selected_title.width()))
        artist_line = f"{artist} \u2014 {album}" if album else artist
        self._selected_artist.setText(artist_metrics.elidedText(artist_line, Qt.ElideRight, self._selected_artist.width()))

        self._current_art_pixmap = None
        if artwork_path:
            px = QPixmap(artwork_path)
            if not px.isNull():
                self._current_art_pixmap = px.scaled(24, 24, Qt.KeepAspectRatio, Qt.SmoothTransformation)
                self._selected_art.setPixmap(self._current_art_pixmap)
                self._selected_art.setVisible(self._playback_state in {"playing", "paused"})
                self._set_center_active(self._playback_state in {"playing", "paused"})
                return
        self._selected_art.clear()
        self._selected_art.setVisible(False)

    def _set_center_active(self, active):
        active = bool(active)
        self._playback_cluster.setProperty("active", active)
        self._selected_title.setProperty("active", active)
        self._selected_artist.setProperty("active", active)
        self._time_label.setProperty("active", active)
        self._progress.setEnabled(active)
        for widget in (self._playback_cluster, self._selected_title, self._selected_artist, self._time_label, self._progress):
            style = widget.style()
            style.unpolish(widget)
            style.polish(widget)
            widget.update()

    @property
    def search_text(self):
        return self._search.text()

    def _on_seek_released(self):
        self.seek_requested.emit(self._progress.value())

    def _apply_button_icon(self, button, path, fallback_text):
        if path:
            icon = QIcon(path)
            if not icon.isNull():
                button.setIcon(icon)
                if button.objectName() == "playback_button":
                    button.setIconSize(QSize(16, 16))
                else:
                    button.setIconSize(QSize(14, 14))
                if fallback_text == "+":
                    button.setText("")
                return
        button.setIcon(QIcon())
        button.setText(fallback_text)

    def _apply_transport_icon(self, button, kind):
        button.setText("")
        button.setIcon(_transport_icon(kind))
        button.setIconSize(QSize(19, 19))


def _format_time(ms):
    seconds = int(ms or 0) // 1000
    mins = seconds // 60
    secs = seconds % 60
    return f"{mins}:{secs:02d}"


def _transport_icon(kind):
    px = QPixmap(22, 22)
    px.fill(Qt.transparent)
    painter = QPainter(px)
    painter.setRenderHint(QPainter.Antialiasing, True)

    shadow = QColor(255, 255, 255, 190)
    face = QColor("#3f3f3f")
    painter.setPen(Qt.NoPen)

    def triangle(points, color):
        painter.setBrush(QBrush(color))
        painter.drawPolygon(QPolygon([QPoint(int(x), int(y)) for x, y in points]))

    def bar(x, y, w, h, color):
        painter.setBrush(QBrush(color))
        painter.drawRoundedRect(x, y, w, h, 1, 1)

    if kind == "previous":
        triangle([(14, 6), (14, 16), (8, 11)], shadow)
        triangle([(8, 6), (8, 16), (2, 11)], shadow)
        bar(3, 6, 2, 10, shadow)
        triangle([(15, 6), (15, 16), (9, 11)], face)
        triangle([(9, 6), (9, 16), (3, 11)], face)
        bar(4, 6, 2, 10, face)
    elif kind == "next":
        triangle([(8, 6), (8, 16), (14, 11)], shadow)
        triangle([(14, 6), (14, 16), (20, 11)], shadow)
        bar(18, 6, 2, 10, shadow)
        triangle([(7, 6), (7, 16), (13, 11)], face)
        triangle([(13, 6), (13, 16), (19, 11)], face)
        bar(17, 6, 2, 10, face)
    elif kind == "pause":
        bar(8, 6, 3, 10, shadow)
        bar(14, 6, 3, 10, shadow)
        bar(7, 6, 3, 10, face)
        bar(13, 6, 3, 10, face)
    else:
        triangle([(8, 5), (8, 17), (17, 11)], shadow)
        triangle([(7, 5), (7, 17), (16, 11)], face)

    painter.setPen(QPen(QColor(0, 0, 0, 40), 1))
    painter.setBrush(Qt.NoBrush)
    painter.end()
    return QIcon(px)
