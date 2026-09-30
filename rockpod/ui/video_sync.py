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

from services.video_rvp import VideoRvpTranscoder


def mark_tracks_for_device(videos, missing_ids=(), device_tracks=(), verified=False):
    """Replace library-wide sync flags with facts for the selected iPod."""
    missing = {int(value) for value in missing_ids}
    paths = {
        int(dict(row)["local_track_id"]): str(dict(row).get("device_path") or "")
        for row in device_tracks if dict(row).get("local_track_id")
    }
    for video in videos:
        video_id = int(video.get("id") or 0)
        on_device = bool(verified and video_id and video_id not in missing)
        video["synced_to_device"] = on_device
        video["device_path"] = paths.get(video_id, "") if on_device else ""
        video["device_device_path"] = video["device_path"]
        video["device_status_unknown"] = not verified
    return videos


class VideoSyncPanel(QWidget):
    """Compact iTunes-style video sync picker."""

    preview_requested = Signal(set)
    sync_requested = Signal(set, str)
    remove_requested = Signal(set)
    delete_requested = Signal(set)
    hide_requested = Signal(set)
    lock_requested = Signal(set)
    refresh_requested = Signal()
    context_requested = Signal(object, object)

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
        self._hide_btn = QPushButton("Hide Selected")
        self._lock_btn = QPushButton("Lock & Hide on iPod")
        self._preview_btn = QPushButton("Preview Sync")
        self._sync_h264_btn = QPushButton("Sync as H.264")
        self._sync_rvp_btn = QPushButton("Sync as RVP")
        self._sync_mpeg_btn = QPushButton("Sync as MPEG")
        self._sync_h264_btn.setToolTip(
            "The measured iTunes 9.2.1 / QuickTime 7.6.6 iPod recipe: "
            "source aspect ratio up to 640×480, two-slice Constrained "
            "Baseline H.264, and 44.1 kHz AAC-LC."
        )
        self._sync_rvp_btn.setToolTip(
            "Sync selected new videos as native 320×240 RVP. "
            "RVP is very large but uses the RVP player."
        )
        self._sync_mpeg_btn.setToolTip(
            "Sync selected new videos as seekable 320×240 MPEG-2. "
            "MPEG is recommended for high quality at a practical size."
        )
        for button in (
            self._refresh_btn,
            self._select_missing_btn,
            self._select_all_btn,
            self._clear_btn,
        ):
            button.setObjectName("store_nav_button")
            action_layout.addWidget(button)
        action_layout.addStretch(1)
        for button in (
            self._preview_btn,
            self._sync_h264_btn,
            self._sync_rvp_btn,
            self._sync_mpeg_btn,
        ):
            button.setObjectName("store_buy_button")
            action_layout.addWidget(button)
        for button in (self._hide_btn, self._lock_btn, self._remove_btn, self._delete_btn):
            button.setObjectName("store_nav_button")
            action_layout.addWidget(button)
        layout.addWidget(actions)

        self._tree = QTreeWidget()
        self._tree.setObjectName("itunes_store_downloads")
        self._tree.setHeaderLabels(["Video", "Type", "iPod Status", "iPod Path", "File"])
        self._tree.setRootIsDecorated(False)
        self._tree.itemChanged.connect(lambda _item, _column: self._update_summary())
        self._tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self._tree.customContextMenuRequested.connect(self._on_custom_context_menu)
        layout.addWidget(self._tree, 1)

        self._status = QLabel("")
        self._status.setObjectName("theme_hub_status")
        self._status.setWordWrap(True)
        layout.addWidget(self._status)

        self._refresh_btn.clicked.connect(self.refresh_requested)
        self._select_missing_btn.clicked.connect(self.select_missing)
        self._select_all_btn.clicked.connect(self.select_all)
        self._clear_btn.clicked.connect(self.clear_selection)
        self._preview_btn.clicked.connect(
            lambda: self.preview_requested.emit(self.syncable_track_ids())
        )
        self._sync_h264_btn.clicked.connect(
            lambda: self.sync_requested.emit(
                self.syncable_track_ids(), "h264_apple_exact"
            )
        )
        self._sync_rvp_btn.clicked.connect(
            lambda: self.sync_requested.emit(self.syncable_track_ids(), "native_raw")
        )
        self._sync_mpeg_btn.clicked.connect(
            lambda: self.sync_requested.emit(self.syncable_track_ids(), "quality")
        )
        self._remove_btn.clicked.connect(lambda: self.remove_requested.emit(self.selected_track_ids()))
        self._delete_btn.clicked.connect(lambda: self.delete_requested.emit(self.selected_track_ids()))
        self._hide_btn.clicked.connect(lambda: self.hide_requested.emit(self.selected_track_ids()))
        self._lock_btn.clicked.connect(lambda: self.lock_requested.emit(self.selected_track_ids()))

    def set_videos(self, videos):
        self._videos = [dict(video or {}) for video in (videos or [])]
        self._tree.blockSignals(True)
        self._tree.clear()
        for video in self._videos:
            title = str(video.get("title") or os.path.basename(str(video.get("file_path") or "")) or "Untitled Video")
            kind = str(video.get("video_sync_label") or video.get("video_kind") or "video").replace("_", " ").title()
            device_path = str(video.get("device_path") or video.get("device_device_path") or "")
            on_ipod = bool(video.get("synced_to_device") or device_path)
            status = ("Checking iPod" if video.get("device_status_unknown")
                      else "On iPod" if on_ipod else "Not on iPod")
            if video.get("video_hidden"):
                status += " · Hidden"
            if video.get("video_locked"):
                status += " · Locked"
            file_path = str(video.get("file_path") or "")
            item = QTreeWidgetItem([title, kind, status, device_path, file_path])
            item.setData(0, Qt.UserRole, int(video.get("id") or 0))
            item.setData(0, Qt.UserRole + 1, dict(video))
            item.setFlags(item.flags() | Qt.ItemIsUserCheckable)
            # Checked means the video is part of the iPod selection. Existing
            # device files stay selected so a refresh never makes them look
            # absent, but normal sync actions only submit newly selected rows.
            item.setCheckState(0, Qt.Checked if on_ipod else Qt.Unchecked)
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

    def syncable_track_ids(self):
        """Return checked videos that are not already present on the iPod."""
        return {
            int(video.get("id") or 0)
            for video in self.selected_videos()
            if int(video.get("id") or 0)
            and not video.get("device_status_unknown")
            and not (video.get("synced_to_device") or video.get("device_path"))
        }

    def select_missing(self):
        self._tree.blockSignals(True)
        for index in range(self._tree.topLevelItemCount()):
            item = self._tree.topLevelItem(index)
            video = dict(item.data(0, Qt.UserRole + 1) or {})
            item.setCheckState(
                0,
                Qt.Checked
                if (
                    video.get("synced_to_device")
                    or video.get("device_path")
                    or video.get("device_device_path")
                    or (item.text(2) == "Not on iPod"
                        and not video.get("device_status_unknown"))
                )
                else Qt.Unchecked,
            )
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
        on_ipod = sum(
            1
            for video in self._videos
            if (
                video.get("synced_to_device")
                or video.get("device_path")
                or video.get("device_device_path")
            )
        )
        selected = len(self.selected_track_ids())
        syncable = len(self.syncable_track_ids())
        unknown = sum(bool(video.get("device_status_unknown")) for video in self._videos)
        missing = total - on_ipod - unknown
        self._subhead.setText(
            f"{total} video{'s' if total != 1 else ''} in library. "
            f"{missing} not on iPod. "
            + (f"{unknown} awaiting device check. " if unknown else "")
            + f"{selected} selected; {syncable} new to sync. "
            "Existing copies are skipped; remove one before changing its format."
            + self._selected_size_comparison()
        )
        self._preview_btn.setEnabled(syncable > 0)
        self._sync_h264_btn.setEnabled(syncable > 0)
        self._sync_rvp_btn.setEnabled(syncable > 0)
        self._sync_mpeg_btn.setEnabled(syncable > 0)
        selected_videos = self.selected_videos()
        self._remove_btn.setEnabled(any(video.get("synced_to_device") for video in selected_videos))
        self._delete_btn.setEnabled(selected > 0)
        self._hide_btn.setEnabled(selected > 0)
        self._lock_btn.setEnabled(selected > 0)
        self._hide_btn.setText(
            "Unhide Selected"
            if selected_videos and all(video.get("video_hidden") for video in selected_videos)
            else "Hide Selected"
        )
        self._lock_btn.setText(
            "Move to Normal iPod Folder"
            if selected_videos and all(video.get("video_locked") for video in selected_videos)
            else "Lock & Hide on iPod"
        )

    @staticmethod
    def _format_size(value):
        gib = float(value) / (1024.0 * 1024.0 * 1024.0)
        if gib >= 0.1:
            return f"{gib:.1f} GB"
        return f"{float(value) / (1024.0 * 1024.0):.0f} MB"

    def _selected_size_comparison(self):
        duration = 0.0
        for video in self.selected_videos():
            if video.get("synced_to_device") or video.get("device_path"):
                continue
            try:
                duration += max(0.0, float(video.get("duration") or 0))
            except (TypeError, ValueError):
                continue
        if duration <= 0:
            return ""
        h264_size = VideoRvpTranscoder.estimated_profile_bytes(
            "h264_apple_exact", duration
        )
        raw_size = VideoRvpTranscoder.estimated_profile_bytes(
            "raw", duration
        )
        return (
            f" Estimated H.264: {self._format_size(h264_size)}; "
            f"RVP: {self._format_size(raw_size)}."
        )

    def _resize_columns(self):
        for column in range(5):
            self._tree.resizeColumnToContents(column)

    def _on_custom_context_menu(self, pos):
        item = self._tree.itemAt(pos)
        if not item:
            return
        video_data = item.data(0, Qt.UserRole + 1) or {}
        global_pos = self._tree.viewport().mapToGlobal(pos)
        self.context_requested.emit(video_data, global_pos)
