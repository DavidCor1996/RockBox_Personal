"""RockPod manager for the standalone offline Reddit application."""

from PySide6.QtCore import QObject, QRunnable, QThreadPool, Signal
from PySide6.QtWidgets import (QCheckBox, QHBoxLayout, QLabel, QLineEdit, QMessageBox,
    QProgressDialog, QPushButton, QSpinBox, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget)


class RedditSignals(QObject):
    progress = Signal(str); finished = Signal(dict); error = Signal(str)


class RedditJob(QRunnable):
    def __init__(self, function, *args, **kwargs):
        super().__init__(); self.function = function; self.args = args; self.kwargs = kwargs; self.signals = RedditSignals()
    def run(self):
        try:
            self.kwargs["progress"] = self.signals.progress.emit
            self.signals.finished.emit(self.function(*self.args, **self.kwargs))
        except Exception as exc:
            self.signals.error.emit(str(exc))


class RedditPanel(QWidget):
    def __init__(self, service, device_provider, parent=None):
        super().__init__(parent); self.service = service; self.device_provider = device_provider; self._job = None; self._progress = None
        layout = QVBoxLayout(self)
        title = QLabel("Reddit · offline communities for iPod"); title.setStyleSheet("font-size: 20px; font-weight: bold;"); layout.addWidget(title)
        note = QLabel("Imports Reddit listings, scores, comments, flair, text, original images and videos through gallery-dl using your Firefox Reddit session. Existing media is skipped on updates."); note.setWordWrap(True); layout.addWidget(note)
        row = QHBoxLayout(); self.url = QLineEdit("https://www.reddit.com/r/ipod/"); self.url.setPlaceholderText("https://www.reddit.com/r/ipod/"); row.addWidget(self.url, 1)
        self.import_button = QPushButton("Import / Update Subreddit"); self.import_button.clicked.connect(self.import_subreddit); row.addWidget(self.import_button); layout.addLayout(row)
        options = QHBoxLayout(); options.addWidget(QLabel("Posts:")); self.limit = QSpinBox(); self.limit.setRange(5, 100); self.limit.setValue(25); options.addWidget(self.limit)
        self.media = QCheckBox("Download images and videos"); self.media.setChecked(True); options.addWidget(self.media); options.addStretch(1); layout.addLayout(options)
        self.table = QTableWidget(0, 5); self.table.setHorizontalHeaderLabels(["Community", "Subscribers", "Posts", "Media", "Cached"]); self.table.horizontalHeader().setStretchLastSection(True); layout.addWidget(self.table, 1)
        buttons = QHBoxLayout(); remove = QPushButton("Remove Community"); remove.clicked.connect(self.remove_subreddit); buttons.addWidget(remove); buttons.addStretch(1)
        self.sync_mpeg_button = QPushButton("Sync as MPEG"); self.sync_mpeg_button.clicked.connect(lambda: self.sync("quality")); buttons.addWidget(self.sync_mpeg_button)
        self.sync_h264_button = QPushButton("Sync as H.264"); self.sync_h264_button.clicked.connect(lambda: self.sync("h264_apple_exact")); buttons.addWidget(self.sync_h264_button); layout.addLayout(buttons)
        self.status = QLabel("Ready"); layout.addWidget(self.status); self.refresh()

    def refresh(self):
        communities = self.service.list_subreddits(); self.table.setRowCount(len(communities))
        for row, community in enumerate(communities):
            posts = community.get("posts") or []
            values = [community.get("display_name") or "r/" + community.get("name", ""), f"{int(community.get('subscribers') or 0):,}", str(len(posts)), str(sum(bool(p.get("source_path")) for p in posts)), "Incremental"]
            for column, value in enumerate(values): self.table.setItem(row, column, QTableWidgetItem(value))
        self.table.resizeColumnsToContents()

    def _start(self, label, function, *args, **kwargs):
        if self._job is not None: return
        self._progress = QProgressDialog(label, None, 0, 0, self); self._progress.setWindowTitle("Reddit"); self._progress.setMinimumDuration(0); self._progress.show()
        self._job = RedditJob(function, *args, **kwargs); self._job.signals.progress.connect(self._status); self._job.signals.finished.connect(self._finished); self._job.signals.error.connect(self._failed); QThreadPool.globalInstance().start(self._job)

    def import_subreddit(self):
        self._start("Reading Reddit…", self.service.import_subreddit, self.url.text().strip(), self.limit.value(), self.media.isChecked())

    def sync(self, video_profile="quality"):
        device = self.device_provider()
        if device is None or not getattr(device, "mount_path", ""):
            QMessageBox.warning(self, "Reddit", "Connect and mount the iPod first."); return
        label = "H.264" if video_profile == "h264_apple_exact" else "MPEG"
        self._start(
            f"Preparing Reddit {label} sync…", self.service.sync,
            device.mount_path, video_profile=video_profile,
        )

    def remove_subreddit(self):
        row = self.table.currentRow()
        if row < 0: return
        self.service.remove_subreddit(self.table.item(row, 0).text().removeprefix("r/")); self.refresh()

    def _status(self, message):
        self.status.setText(str(message)); self._progress.setLabelText(str(message)) if self._progress is not None else None
    def _finished(self, report):
        if self._progress is not None: self._progress.close()
        self._progress = None; self._job = None; self.refresh(); self.status.setText(f"Reddit ready · {report.get('subreddits', 1)} community(s) · {report.get('posts', 0)} posts · {report.get('media', 0)} media")
    def _failed(self, message):
        if self._progress is not None: self._progress.close()
        self._progress = None; self._job = None; self.status.setText("Reddit operation failed"); QMessageBox.warning(self, "Reddit", str(message))
