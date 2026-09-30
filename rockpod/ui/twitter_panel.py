"""RockPod controls for the offline, classic Twitter iPod application."""

from PySide6.QtCore import QObject, QRunnable, QThreadPool, Signal
from PySide6.QtWidgets import (
    QCheckBox, QFileDialog, QHBoxLayout, QLabel, QLineEdit, QMessageBox,
    QProgressDialog, QPushButton, QSpinBox, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget,
)


class TwitterSignals(QObject):
    progress = Signal(str)
    finished = Signal(dict)
    error = Signal(str)


class TwitterJob(QRunnable):
    def __init__(self, function, *args, **kwargs):
        super().__init__()
        self.function = function
        self.args = args
        self.kwargs = kwargs
        self.signals = TwitterSignals()

    def run(self):
        try:
            self.kwargs["progress"] = self.signals.progress.emit
            self.signals.finished.emit(self.function(*self.args, **self.kwargs))
        except Exception as exc:
            self.signals.error.emit(str(exc))


class TwitterPanel(QWidget):
    def __init__(self, service, device_provider, parent=None):
        super().__init__(parent)
        self.service = service
        self.device_provider = device_provider
        self._job = None
        self._progress = None

        layout = QVBoxLayout(self)
        title = QLabel("Twitter · classic offline timeline for iPod")
        title.setStyleSheet("font-size: 20px; font-weight: bold;")
        layout.addWidget(title)
        note = QLabel(
            "Import a chosen number of past posts from an X profile through "
            "your signed-in Firefox session. Original photos and videos stay "
            "in the external cache; RockPod syncs device-ready copies."
        )
        note.setWordWrap(True)
        layout.addWidget(note)

        cache_row = QHBoxLayout()
        self.cache_label = QLabel()
        self.cache_label.setWordWrap(True)
        cache_row.addWidget(self.cache_label, 1)
        choose = QPushButton("External cache…")
        choose.clicked.connect(self.choose_cache)
        cache_row.addWidget(choose)
        layout.addLayout(cache_row)

        source_row = QHBoxLayout()
        self.url = QLineEdit()
        self.url.setPlaceholderText("https://x.com/username")
        source_row.addWidget(self.url, 1)
        self.import_button = QPushButton("Import / Update Profile")
        self.import_button.clicked.connect(self.import_profile)
        source_row.addWidget(self.import_button)
        layout.addLayout(source_row)

        options = QHBoxLayout()
        options.addWidget(QLabel("Previous posts:"))
        self.limit = QSpinBox()
        self.limit.setRange(1, 500)
        self.limit.setValue(50)
        options.addWidget(self.limit)
        self.media = QCheckBox("Import photos and videos")
        self.media.setChecked(True)
        options.addWidget(self.media)
        options.addStretch(1)
        layout.addLayout(options)

        self.table = QTableWidget(0, 3)
        self.table.setHorizontalHeaderLabels(["Profile", "Posts", "Cached media"])
        self.table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.table, 1)

        buttons = QHBoxLayout()
        remove = QPushButton("Remove Profile")
        remove.clicked.connect(self.remove_profile)
        buttons.addWidget(remove)
        buttons.addStretch(1)
        mpeg = QPushButton("Sync as MPEG")
        mpeg.clicked.connect(lambda: self.sync("quality"))
        buttons.addWidget(mpeg)
        h264 = QPushButton("Sync as H.264")
        h264.clicked.connect(lambda: self.sync("h264_apple_exact"))
        buttons.addWidget(h264)
        layout.addLayout(buttons)
        self.status = QLabel("Ready")
        layout.addWidget(self.status)
        self.refresh()

    def refresh(self):
        try:
            self.cache_label.setText("External cache: " + str(self.service.cache_root))
            accounts = self.service.list_accounts()
        except ValueError:
            self.cache_label.setText("Choose an external Twitter cache folder")
            accounts = []
        self.table.setRowCount(len(accounts))
        for row, account in enumerate(accounts):
            posts = account.get("posts") or []
            values = [
                "@" + str(account.get("handle") or ""),
                str(len(posts)),
                str(sum(len(post.get("media") or []) for post in posts)),
            ]
            for column, value in enumerate(values):
                self.table.setItem(row, column, QTableWidgetItem(value))
        self.table.resizeColumnsToContents()

    def choose_cache(self):
        path = QFileDialog.getExistingDirectory(self, "Choose external Twitter cache")
        if path:
            try:
                self.service.set_cache_root(path)
            except ValueError as exc:
                QMessageBox.warning(self, "Twitter", str(exc))
            self.refresh()

    def _start(self, label, function, *args, **kwargs):
        if self._job is not None:
            return
        self._progress = QProgressDialog(label, None, 0, 0, self)
        self._progress.setWindowTitle("Twitter")
        self._progress.setMinimumDuration(0)
        self._progress.show()
        self._job = TwitterJob(function, *args, **kwargs)
        self._job.signals.progress.connect(self._status)
        self._job.signals.finished.connect(self._finished)
        self._job.signals.error.connect(self._failed)
        QThreadPool.globalInstance().start(self._job)

    def import_profile(self):
        self._start("Reading X profile…", self.service.import_profile,
                    self.url.text().strip(), self.limit.value(),
                    self.media.isChecked())

    def sync(self, video_profile):
        device = self.device_provider()
        if device is None or not getattr(device, "mount_path", ""):
            QMessageBox.warning(self, "Twitter", "Connect and mount the iPod first.")
            return
        self._start("Preparing Twitter sync…", self.service.sync,
                    device.mount_path, video_profile=video_profile)

    def remove_profile(self):
        row = self.table.currentRow()
        if row < 0:
            return
        self.service.remove_account(self.table.item(row, 0).text())
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
            f"Twitter ready · {report.get('posts', 0)} posts · "
            f"{report.get('media', 0)} media"
        )
        if report.get("media_errors"):
            QMessageBox.warning(self, "Twitter",
                                f"{len(report['media_errors'])} post(s) had media download errors")

    def _failed(self, message):
        if self._progress is not None:
            self._progress.close()
        self._progress = None
        self._job = None
        self.status.setText("Twitter operation failed")
        QMessageBox.warning(self, "Twitter", str(message))
