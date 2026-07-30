"""Calm sound library/sync and YouTube audio-only Store panels."""

from __future__ import annotations

import os

from PySide6.QtCore import QProcess, QUrl, Signal
from PySide6.QtGui import QDesktopServices
from PySide6.QtWidgets import (
    QComboBox,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)

from services.calm import CALM_CATEGORIES


CALM_CYAN = "#3bbeec"
CALM_BLUE = "#509ae7"
CALM_INDIGO = "#4e60e2"
CALM_NAVY = "#102454"


def _duration_text(seconds):
    seconds = max(0, int(seconds or 0))
    return f"{seconds // 60}:{seconds % 60:02d}"


class CalmLibraryPanel(QWidget):
    """Local Calm library with explicit device/simulator sync."""

    def __init__(self, service, profiles, parent=None):
        super().__init__(parent)
        self.service = service
        self.profiles = profiles

        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 18, 20, 18)
        layout.setSpacing(10)

        hero = QFrame()
        hero.setStyleSheet(
            "QFrame {"
            f"background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            f"stop:0 {CALM_CYAN}, stop:.55 {CALM_BLUE}, stop:1 {CALM_INDIGO});"
            "border-radius: 10px; color: white; }"
        )
        hero_layout = QVBoxLayout(hero)
        title = QLabel("Calm Sync")
        title.setStyleSheet("font-size: 27px; font-weight: 600; color: white;")
        subtitle = QLabel("Take a deep breath. Keep your offline sounds close.")
        subtitle.setStyleSheet("font-size: 13px; color: white;")
        hero_layout.addWidget(title)
        hero_layout.addWidget(subtitle)
        layout.addWidget(hero)

        self.tree = QTreeWidget()
        self.tree.setHeaderLabels(["Sound", "Section", "Length", "Size"])
        self.tree.setAlternatingRowColors(True)
        layout.addWidget(self.tree, 1)

        controls = QHBoxLayout()
        self.target = QComboBox()
        self.target.addItem("Connected iPod", "device")
        self.target.addItem("Simulator", "simulator")
        self.refresh_button = QPushButton("Refresh")
        self.sync_button = QPushButton("Sync Calm")
        self.sync_button.setStyleSheet(
            f"QPushButton {{ background: {CALM_BLUE}; color: white;"
            "padding: 6px 14px; border-radius: 5px; font-weight: 600; }}"
        )
        controls.addWidget(QLabel("Target:"))
        controls.addWidget(self.target)
        controls.addStretch(1)
        controls.addWidget(self.refresh_button)
        controls.addWidget(self.sync_button)
        layout.addLayout(controls)

        self.status = QLabel("")
        self.status.setStyleSheet("color: #617086;")
        layout.addWidget(self.status)

        self.refresh_button.clicked.connect(self.refresh)
        self.sync_button.clicked.connect(self.sync)
        self.refresh()

    def refresh(self):
        items = self.service.scan()
        self.tree.clear()
        for item in items:
            row = QTreeWidgetItem(
                [
                    item["title"],
                    item["category"],
                    _duration_text(item["duration"]),
                    f"{item['size'] / (1024 * 1024):.1f} MB",
                ]
            )
            row.setData(0, 256, item["id"])
            self.tree.addTopLevelItem(row)
        for column in range(4):
            self.tree.resizeColumnToContents(column)
        self.status.setText(
            f"{len(items)} offline sound{'s' if len(items) != 1 else ''}"
            if items
            else "No sounds yet. Add one from Store → Calm."
        )

    def sync(self):
        profile = self.profiles.current_profile()
        mode = self.target.currentData()
        try:
            result = self.service.sync(profile, target_mode=mode)
        except (OSError, ValueError) as exc:
            QMessageBox.warning(self, "Calm Sync", str(exc))
            return
        self.status.setText(
            f"Synced {result['count']} sound"
            f"{'s' if result['count'] != 1 else ''} to {result['target']}"
        )


class CalmStorePanel(QWidget):
    """YouTube handoff that persists only its audio stream."""

    library_changed = Signal()

    def __init__(self, service, parent=None):
        super().__init__(parent)
        self.service = service
        self.process = None
        self.pending_category = "Sounds"
        self.before_info = set()

        layout = QVBoxLayout(self)
        layout.setContentsMargins(24, 22, 24, 22)
        layout.setSpacing(12)

        title = QLabel("Calm Sound Store")
        title.setStyleSheet(
            f"font-size: 25px; font-weight: 600; color: {CALM_NAVY};"
        )
        copy = QLabel(
            "Choose a sound on YouTube, paste its URL, and Rockpod will "
            "download only the audio stream for personal offline use."
        )
        copy.setWordWrap(True)
        copy.setStyleSheet("color: #52647a; font-size: 13px;")
        layout.addWidget(title)
        layout.addWidget(copy)

        row = QHBoxLayout()
        self.open_button = QPushButton("Open YouTube")
        self.url = QLineEdit()
        self.url.setPlaceholderText("https://www.youtube.com/watch?v=…")
        row.addWidget(self.open_button)
        row.addWidget(self.url, 1)
        layout.addLayout(row)

        options = QHBoxLayout()
        self.category = QComboBox()
        self.category.addItems(CALM_CATEGORIES)
        self.category.setCurrentText("Sounds")
        self.download = QPushButton("Download Sound")
        self.download.setStyleSheet(
            f"QPushButton {{ background: {CALM_BLUE}; color: white;"
            "padding: 7px 16px; border-radius: 5px; font-weight: 600; }}"
        )
        options.addWidget(QLabel("iPod section:"))
        options.addWidget(self.category)
        options.addStretch(1)
        options.addWidget(self.download)
        layout.addLayout(options)

        note = QLabel(
            "Audio only · no video · no thumbnail · single-item URLs only"
        )
        note.setStyleSheet("color: #75849a;")
        layout.addWidget(note)
        self.status = QLabel("Ready")
        self.status.setWordWrap(True)
        self.status.setStyleSheet("color: #53657a;")
        layout.addWidget(self.status)
        layout.addStretch(1)

        self.open_button.clicked.connect(
            lambda: QDesktopServices.openUrl(QUrl("https://www.youtube.com/"))
        )
        self.download.clicked.connect(self.start_download)

    def start_download(self):
        if self.process is not None:
            return
        try:
            command, self.pending_category = self.service.build_download_command(
                self.url.text(), self.category.currentText()
            )
        except ValueError as exc:
            QMessageBox.warning(self, "Calm Sound Store", str(exc))
            return
        self.before_info = {
            path.name for path in self.service.library_root.glob("*.info.json")
        }
        self.process = QProcess(self)
        self.process.setProcessChannelMode(QProcess.MergedChannels)
        self.process.readyReadStandardOutput.connect(self._read_output)
        self.process.finished.connect(self._finished)
        self.download.setEnabled(False)
        self.status.setText("Downloading and converting audio…")
        self.process.start(command[0], command[1:])

    def _read_output(self):
        if self.process is None:
            return
        text = bytes(self.process.readAllStandardOutput()).decode(
            "utf-8", errors="replace"
        )
        lines = [line.strip() for line in text.splitlines() if line.strip()]
        if lines:
            self.status.setText(lines[-1][-240:])

    def _finished(self, exit_code, _status):
        process = self.process
        self.process = None
        self.download.setEnabled(True)
        if exit_code:
            self.status.setText("Sound download failed. Check the URL and yt-dlp.")
            if process is not None:
                process.deleteLater()
            return
        after = sorted(
            (
                path
                for path in self.service.library_root.glob("*.info.json")
                if path.name not in self.before_info
            ),
            key=lambda path: path.stat().st_mtime,
        )
        if after:
            self.service.set_category(
                after[-1].name[:-10], self.pending_category
            )
        self.status.setText("Sound added. Open Calm Sync to send it to the iPod.")
        self.library_changed.emit()
        if process is not None:
            process.deleteLater()
