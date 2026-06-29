"""Video sync screen for selecting videos to copy to an iPod."""

from __future__ import annotations

import os

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)


class VideoSyncPanel(QWidget):
    """Compact iTunes-style video sync picker."""

    preview_requested = Signal(set)
    sync_requested = Signal(set)
    force_repair_requested = Signal(set)
    remove_requested = Signal(set)
    delete_requested = Signal(set)
    refresh_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("video_sync_panel")
        self._videos = []

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 8, 10, 10)
        layout.setSpacing(7)

        header = QFrame()
        header.setObjectName("itunes_store_nav")
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(9, 5, 9, 5)
        title = QLabel("Video Sync")
        title.setObjectName("itunes_store_title")
        header_layout.addWidget(title)
        header_layout.addStretch(1)
        layout.addWidget(header)

        summary = QFrame()
        summary.setObjectName("itunes_store_hero")
        summary_layout = QVBoxLayout(summary)
        summary_layout.setContentsMargins(14, 12, 14, 12)
        self._headline = QLabel("Choose videos to sync")
        self._headline.setObjectName("itunes_store_headline")
        self._subhead = QLabel("")
        self._subhead.setObjectName("itunes_store_subhead")
        self._subhead.setWordWrap(True)
        summary_layout.addWidget(self._headline)
        summary_layout.addWidget(self._subhead)
        layout.addWidget(summary)

        actions = QFrame()
        actions.setObjectName("itunes_store_import_bar")
        action_layout = QHBoxLayout(actions)
        action_layout.setContentsMargins(9, 6, 9, 6)
        action_layout.setSpacing(6)
        self._refresh_btn = QPushButton("Refresh")
        self._select_missing_btn = QPushButton("Select Missing")
        self._select_all_btn = QPushButton("Select All")
        self._clear_btn = QPushButton("Clear")
        self._remove_btn = QPushButton("Remove from iPod")
        self._delete_btn = QPushButton("Delete Local Files")
        self._preview_btn = QPushButton("Preview Sync")
        self._sync_btn = QPushButton("Sync Selected")
        self._repair_btn = QPushButton("Repair Selected")
        for button in (
            self._refresh_btn,
            self._select_missing_btn,
            self._select_all_btn,
            self._clear_btn,
        ):
            button.setObjectName("store_nav_button")
            action_layout.addWidget(button)
        action_layout.addStretch(1)
        for button in (self._preview_btn, self._sync_btn, self._repair_btn):
            button.setObjectName("store_buy_button")
            action_layout.addWidget(button)
        for button in (self._remove_btn, self._delete_btn):
            button.setObjectName("store_nav_button")
            action_layout.addWidget(button)
        layout.addWidget(actions)

        self._tree = QTreeWidget()
        self._tree.setObjectName("itunes_store_downloads")
        self._tree.setHeaderLabels(["Video", "Type", "iPod Status", "iPod Path", "File"])
        self._tree.setRootIsDecorated(False)
        self._tree.itemChanged.connect(lambda _item, _column: self._update_summary())
        layout.addWidget(self._tree, 1)

        self._status = QLabel("")
        self._status.setObjectName("theme_hub_status")
        self._status.setWordWrap(True)
        layout.addWidget(self._status)

        self._refresh_btn.clicked.connect(self.refresh_requested)
        self._select_missing_btn.clicked.connect(self.select_missing)
        self._select_all_btn.clicked.connect(self.select_all)
        self._clear_btn.clicked.connect(self.clear_selection)
        self._preview_btn.clicked.connect(lambda: self.preview_requested.emit(self.selected_track_ids()))
        self._sync_btn.clicked.connect(lambda: self.sync_requested.emit(self.selected_track_ids()))
        self._repair_btn.clicked.connect(lambda: self.force_repair_requested.emit(self.selected_track_ids()))
        self._remove_btn.clicked.connect(lambda: self.remove_requested.emit(self.selected_track_ids()))
        self._delete_btn.clicked.connect(lambda: self.delete_requested.emit(self.selected_track_ids()))

    def set_videos(self, videos):
        self._videos = [dict(video or {}) for video in (videos or [])]
        self._tree.blockSignals(True)
        self._tree.clear()
        for video in self._videos:
            title = str(video.get("title") or os.path.basename(str(video.get("file_path") or "")) or "Untitled Video")
            kind = str(video.get("video_sync_label") or video.get("video_kind") or "video").replace("_", " ").title()
            on_ipod = bool(video.get("synced_to_device"))
            status = "On iPod" if on_ipod else "Not on iPod"
            device_path = str(video.get("device_path") or video.get("device_device_path") or "")
            file_path = str(video.get("file_path") or "")
            item = QTreeWidgetItem([title, kind, status, device_path, file_path])
            item.setData(0, Qt.UserRole, int(video.get("id") or 0))
            item.setData(0, Qt.UserRole + 1, dict(video))
            item.setFlags(item.flags() | Qt.ItemIsUserCheckable)
            item.setCheckState(0, Qt.Unchecked)
            self._tree.addTopLevelItem(item)
        self._tree.blockSignals(False)
        self._resize_columns()
        self._update_summary()

    def selected_track_ids(self):
        ids = set()
        for index in range(self._tree.topLevelItemCount()):
            item = self._tree.topLevelItem(index)
            if item.checkState(0) == Qt.Checked:
                track_id = int(item.data(0, Qt.UserRole) or 0)
                if track_id:
                    ids.add(track_id)
        return ids

    def selected_videos(self):
        videos = []
        for index in range(self._tree.topLevelItemCount()):
            item = self._tree.topLevelItem(index)
            if item.checkState(0) == Qt.Checked:
                videos.append(dict(item.data(0, Qt.UserRole + 1) or {}))
        return videos

    def select_missing(self):
        self._tree.blockSignals(True)
        for index in range(self._tree.topLevelItemCount()):
            item = self._tree.topLevelItem(index)
            item.setCheckState(0, Qt.Checked if item.text(2) == "Not on iPod" else Qt.Unchecked)
        self._tree.blockSignals(False)
        self._update_summary()

    def select_all(self):
        self._set_all(Qt.Checked)

    def clear_selection(self):
        self._set_all(Qt.Unchecked)

    def set_status(self, text):
        self._status.setText(str(text or ""))

    def _set_all(self, state):
        self._tree.blockSignals(True)
        for index in range(self._tree.topLevelItemCount()):
            self._tree.topLevelItem(index).setCheckState(0, state)
        self._tree.blockSignals(False)
        self._update_summary()

    def _update_summary(self):
        total = len(self._videos)
        on_ipod = sum(1 for video in self._videos if video.get("synced_to_device"))
        selected = len(self.selected_track_ids())
        missing = total - on_ipod
        self._subhead.setText(
            f"{total} video{'s' if total != 1 else ''} in library. "
            f"{missing} not on iPod. {selected} selected."
        )
        self._preview_btn.setEnabled(selected > 0)
        self._sync_btn.setEnabled(selected > 0)
        self._repair_btn.setEnabled(selected > 0)
        selected_videos = self.selected_videos()
        self._remove_btn.setEnabled(any(video.get("synced_to_device") for video in selected_videos))
        self._delete_btn.setEnabled(selected > 0)

    def _resize_columns(self):
        for column in range(5):
            self._tree.resizeColumnToContents(column)
