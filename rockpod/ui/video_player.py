"""Detached video player window for local library playback."""

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QLabel, QMainWindow, QVBoxLayout, QWidget


class VideoPlayerWindow(QMainWindow):
    """Simple detached window that hosts a QVideoWidget."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("RockPod Video")
        self.resize(960, 640)

        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        try:
            from PySide6.QtMultimediaWidgets import QVideoWidget
        except ImportError:
            self._video_output = None
            fallback = QLabel("Qt video playback is unavailable in this build.")
            fallback.setAlignment(Qt.AlignCenter)
            layout.addWidget(fallback, 1)
        else:
            self._video_output = QVideoWidget(self)
            layout.addWidget(self._video_output, 1)

        self._caption = QLabel("No video loaded")
        self._caption.setAlignment(Qt.AlignCenter)
        self._caption.setObjectName("video_player_caption")
        layout.addWidget(self._caption)

    @property
    def video_output(self):
        return self._video_output

    def set_track(self, track):
        if not track:
            self._caption.setText("No video loaded")
            self.setWindowTitle("RockPod Video")
            return
        title = track.get("title") or "Untitled Video"
        artist = track.get("artist") or track.get("album_artist") or ""
        self._caption.setText(f"{title} • {artist}" if artist else title)
        self.setWindowTitle(f"RockPod Video - {title}")
