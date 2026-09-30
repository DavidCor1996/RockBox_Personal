"""RockPod manager for the standalone offline Instagram application."""

import logging

from PySide6.QtCore import QObject, QRunnable, QThreadPool, Signal
from PySide6.QtWidgets import (
    QCheckBox, QHBoxLayout, QLabel, QLineEdit, QMessageBox, QProgressDialog,
    QPushButton, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget,
)


class InstagramSignals(QObject):
    progress = Signal(str)
    finished = Signal(dict)
    error = Signal(str)


class InstagramJob(QRunnable):
    def __init__(self, function, *args, **kwargs):
        super().__init__()
        self.function = function
        self.args = args
        self.kwargs = kwargs
        self.signals = InstagramSignals()

    def run(self):
        try:
            self.kwargs["progress"] = self.signals.progress.emit
            self.signals.finished.emit(self.function(*self.args, **self.kwargs))
        except Exception as exc:
            logging.getLogger(__name__).exception("Instagram operation failed")
            self.signals.error.emit(str(exc))


class InstagramPanel(QWidget):
    def __init__(self, service, device_provider, parent=None):
        super().__init__(parent)
        self.service = service
        self.device_provider = device_provider
        self._job = None
        self._progress = None
        layout = QVBoxLayout(self)
        title = QLabel("Instagram · offline profiles for iPod")
        title.setStyleSheet("font-size: 20px; font-weight: bold;")
        layout.addWidget(title)
        note = QLabel(
            "Imports original photos, carousel media, Reels, captions, profile art, "
            "and metadata through gallery-dl using your Firefox Instagram login. "
            "Existing posts are skipped on later imports."
        )
        note.setWordWrap(True)
        layout.addWidget(note)
        row = QHBoxLayout()
        self.url = QLineEdit()
        self.url.setPlaceholderText("https://www.instagram.com/creator/")
        self.url.setText("https://www.instagram.com/jasmine.in.dreamland/")
        row.addWidget(self.url, 1)
        self.login_button = QPushButton("Refresh Login")
        self.login_button.clicked.connect(self.refresh_login)
        row.addWidget(self.login_button)
        self.import_button = QPushButton("Import / Update Profile")
        self.import_button.clicked.connect(self.import_profile)
        row.addWidget(self.import_button)
        layout.addLayout(row)
        options = QHBoxLayout()
        self.photos = QCheckBox("Photos and carousels")
        self.photos.setChecked(True)
        self.videos = QCheckBox("Reels and videos")
        self.videos.setChecked(True)
        options.addWidget(self.photos)
        options.addWidget(self.videos)
        options.addStretch(1)
        layout.addLayout(options)
        self.table = QTableWidget(0, 6)
        self.table.setHorizontalHeaderLabels(
            ["Profile", "Name", "Followers", "Posts", "Photos", "Videos"]
        )
        self.table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.table, 1)
        buttons = QHBoxLayout()
        remove = QPushButton("Remove Profile")
        remove.clicked.connect(self.remove_profile)
        buttons.addWidget(remove)
        buttons.addStretch(1)
        self.sync_mpeg_button = QPushButton("Sync as MPEG")
        self.sync_mpeg_button.clicked.connect(lambda: self.sync("quality"))
        buttons.addWidget(self.sync_mpeg_button)
        self.sync_h264_button = QPushButton("Sync as H.264")
        self.sync_h264_button.clicked.connect(
            lambda: self.sync("h264_apple_exact")
        )
        buttons.addWidget(self.sync_h264_button)
        layout.addLayout(buttons)
        self.status = QLabel("Ready")
        layout.addWidget(self.status)
        self.refresh()

    def refresh(self):
        profiles = self.service.list_profiles()
        self.table.setRowCount(len(profiles))
        for row, profile in enumerate(profiles):
            media = profile.get("media") or []
            values = [
                "@" + profile.get("username", ""),
                profile.get("display_name", ""),
                f"{int(profile.get('follower_count') or 0):,}",
                str(profile.get("post_count") or len(media)),
                str(sum(i.get("type") == "photo" for i in media)),
                str(sum(i.get("type") == "video" for i in media)),
            ]
            for column, value in enumerate(values):
                self.table.setItem(row, column, QTableWidgetItem(value))
        self.table.resizeColumnsToContents()

    def _start(self, label, function, *args, **kwargs):
        if self._job is not None:
            return
        self._progress = QProgressDialog(label, None, 0, 0, self)
        self._progress.setWindowTitle("Instagram")
        self._progress.setMinimumDuration(0)
        self._progress.show()
        self._job = InstagramJob(function, *args, **kwargs)
        self._job.signals.progress.connect(self._status)
        self._job.signals.finished.connect(self._finished)
        self._job.signals.error.connect(self._failed)
        QThreadPool.globalInstance().start(self._job)

    def import_profile(self):
        if not self.photos.isChecked() and not self.videos.isChecked():
            QMessageBox.warning(self, "Instagram", "Select photos, videos, or both.")
            return
        self._start(
            "Reading Instagram profile…", self.service.import_profile,
            self.url.text().strip(), self.photos.isChecked(), self.videos.isChecked(),
        )

    def refresh_login(self):
        try:
            self.service.open_login()
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, "Instagram Login", str(exc))
            return
        self.status.setText(
            "Sign in to Instagram in Firefox, then import/update the profile."
        )

    def sync(self, video_profile="quality"):
        device = self.device_provider()
        if device is None or not getattr(device, "mount_path", ""):
            QMessageBox.warning(self, "Instagram", "Connect and mount the iPod first.")
            return
        label = "H.264" if video_profile == "h264_apple_exact" else "MPEG"
        self._start(
            f"Preparing Instagram {label} sync…", self.service.sync,
            device.mount_path, video_profile=video_profile,
        )

    def remove_profile(self):
        row = self.table.currentRow()
        if row < 0:
            return
        username = self.table.item(row, 0).text().lstrip("@")
        self.service.remove_profile(username)
        self.refresh()

    def _status(self, message):
        self.status.setText(str(message))
        if self._progress is not None:
            self._progress.setLabelText(str(message))

    def _finished(self, report):
        if self._progress is not None:
            self._progress.close()
        self._progress = None
        self._job = None
        self.refresh()
        self.status.setText(
            f"Instagram ready · {report.get('profiles', 1)} profile(s) · "
            f"{report.get('photos', 0)} photos · {report.get('videos', 0)} videos"
        )

    def _failed(self, message):
        if self._progress is not None:
            self._progress.close()
        self._progress = None
        self._job = None
        self.status.setText("Instagram operation failed")
        QMessageBox.warning(self, "Instagram", str(message))
