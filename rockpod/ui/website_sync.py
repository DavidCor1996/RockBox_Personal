"""Managed Website Sync page with an on-device preview."""

from __future__ import annotations

import os

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QSplitter,
    QVBoxLayout,
    QWidget,
)


class WebsiteSyncPanel(QWidget):
    add_requested = Signal(str)
    remove_requested = Signal(dict)
    resync_requested = Signal(str)
    open_requested = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._device_root = ""
        self._entries = []
        self._running = False

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 8, 10, 10)
        layout.setSpacing(7)

        header = QFrame()
        header.setObjectName("itunes_store_nav")
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(9, 6, 9, 6)
        header_layout.setSpacing(6)
        title = QLabel("Website Sync")
        title.setObjectName("itunes_store_title")
        self._url_edit = QLineEdit()
        self._url_edit.setObjectName("itunes_store_search")
        self._url_edit.setPlaceholderText("Enter a website URL")
        self._url_edit.returnPressed.connect(self._emit_add)
        self._add_button = QPushButton("Add Website")
        self._add_button.setObjectName("store_buy_button")
        self._add_button.clicked.connect(self._emit_add)
        header_layout.addWidget(title)
        header_layout.addWidget(self._url_edit, 1)
        header_layout.addWidget(self._add_button)
        layout.addWidget(header)

        splitter = QSplitter(Qt.Horizontal)
        self._list = QListWidget()
        self._list.setObjectName("website_sync_list")
        self._list.setMinimumWidth(210)
        self._list.currentRowChanged.connect(self._show_selection)
        splitter.addWidget(self._list)

        preview_frame = QFrame()
        preview_frame.setObjectName("itunes_store_sidebar_panel")
        preview_layout = QVBoxLayout(preview_frame)
        preview_layout.setContentsMargins(12, 10, 12, 10)
        preview_layout.setSpacing(7)
        preview_title = QLabel("iPod Preview")
        preview_title.setObjectName("itunes_store_section_title")
        self._preview = QLabel("Select a website to preview it.")
        self._preview.setObjectName("website_sync_preview")
        self._preview.setAlignment(Qt.AlignCenter)
        self._preview.setMinimumSize(320, 240)
        self._preview.setMaximumHeight(360)
        self._preview.setScaledContents(False)
        self._details = QLabel("")
        self._details.setWordWrap(True)
        self._details.setTextInteractionFlags(Qt.TextSelectableByMouse)
        actions = QHBoxLayout()
        self._resync_button = QPushButton("Resync")
        self._remove_button = QPushButton("Remove")
        self._open_button = QPushButton("Open in Browser")
        for button in (self._resync_button, self._remove_button, self._open_button):
            button.setObjectName("store_nav_button")
            actions.addWidget(button)
        actions.addStretch(1)
        self._resync_button.clicked.connect(self._emit_resync)
        self._remove_button.clicked.connect(self._emit_remove)
        self._open_button.clicked.connect(self._emit_open)
        preview_layout.addWidget(preview_title)
        preview_layout.addWidget(self._preview, 1)
        preview_layout.addWidget(self._details)
        preview_layout.addLayout(actions)
        splitter.addWidget(preview_frame)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 3)
        layout.addWidget(splitter, 1)

        self._status = QLabel("")
        self._status.setObjectName("theme_hub_status")
        self._status.setWordWrap(True)
        self._status.setVisible(False)
        layout.addWidget(self._status)
        self._update_actions()

    def set_websites(self, entries, device_root=""):
        selected_url = ""
        current = self.current_entry()
        if current:
            selected_url = str(current.get("url") or "")
        self._device_root = str(device_root or "")
        self._entries = list(entries or [])
        self._list.clear()
        selected_row = -1
        for row, entry in enumerate(self._entries):
            title = str(entry.get("title") or entry.get("url") or "Website")
            item = QListWidgetItem(title)
            item.setToolTip(str(entry.get("url") or ""))
            self._list.addItem(item)
            if entry.get("url") == selected_url:
                selected_row = row
        if self._entries:
            self._list.setCurrentRow(selected_row if selected_row >= 0 else 0)
        else:
            self._show_selection(-1)

    def set_status(self, text, running=False):
        self._running = bool(running)
        self._status.setText(str(text or ""))
        self._status.setVisible(bool(text))
        self._url_edit.setEnabled(not self._running)
        self._add_button.setEnabled(not self._running)
        self._update_actions()

    def current_entry(self):
        row = self._list.currentRow()
        if 0 <= row < len(self._entries):
            return self._entries[row]
        return None

    def _preview_path(self, entry):
        relative = str(entry.get("preview_path") or "").lstrip("/")
        if not relative or not self._device_root:
            return ""
        path = os.path.abspath(os.path.join(self._device_root, relative))
        if os.path.commonpath([os.path.abspath(self._device_root), path]) != os.path.abspath(self._device_root):
            return ""
        return path

    def _show_selection(self, _row):
        entry = self.current_entry()
        if not entry:
            self._preview.setPixmap(QPixmap())
            self._preview.setText("No synced websites yet.\nAdd a URL above to get started.")
            self._details.setText("")
            self._update_actions()
            return

        path = self._preview_path(entry)
        pixmap = QPixmap(path) if path and os.path.isfile(path) else QPixmap()
        if not pixmap.isNull():
            self._preview.setText("")
            self._preview.setPixmap(
                pixmap.scaled(320, 240, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            )
        else:
            self._preview.setPixmap(QPixmap())
            self._preview.setText(
                "Preview will be captured on the next sync.\n"
                + str(entry.get("title") or entry.get("url") or "")
            )
        self._details.setText(
            f"{entry.get('url', '')}\n"
            f"Last synced: {entry.get('synced_at', 'Unknown')} · "
            f"Capture: {entry.get('source', 'Unknown')} · "
            f"Cached files: {len(entry.get('files') or [])}"
        )
        self._update_actions()

    def _update_actions(self):
        enabled = self.current_entry() is not None and not self._running
        self._resync_button.setEnabled(enabled)
        self._remove_button.setEnabled(enabled)
        self._open_button.setEnabled(self.current_entry() is not None)

    def _emit_add(self):
        value = self._url_edit.text().strip()
        if value and not self._running:
            self.add_requested.emit(value)

    def _emit_resync(self):
        entry = self.current_entry()
        if entry and not self._running:
            self.resync_requested.emit(str(entry.get("url") or ""))

    def _emit_remove(self):
        entry = self.current_entry()
        if entry and not self._running:
            self.remove_requested.emit(dict(entry))

    def _emit_open(self):
        entry = self.current_entry()
        if entry:
            self.open_requested.emit(str(entry.get("url") or ""))
