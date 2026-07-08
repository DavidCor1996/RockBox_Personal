"""Rockbox photo sync panel."""

from __future__ import annotations

from datetime import datetime

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QAbstractItemView,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class PhotoManagerWidget(QWidget):
    profile_selected = Signal(str)
    target_mode_selected = Signal(str)
    choose_library_requested = Signal()
    refresh_requested = Signal()
    selection_changed = Signal()
    dry_run_requested = Signal()
    sync_requested = Signal()
    hide_requested = Signal()
    remove_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("photo_manager")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(8)

        header = QFrame()
        header.setObjectName("theme_hub_header")
        grid = QGridLayout(header)
        grid.setContentsMargins(10, 8, 10, 8)
        grid.setHorizontalSpacing(8)
        grid.setVerticalSpacing(4)

        self._profile_combo = QComboBox()
        self._profile_combo.currentIndexChanged.connect(self._emit_profile_changed)
        self._target_combo = QComboBox()
        self._target_combo.currentIndexChanged.connect(self._emit_target_changed)
        self._library_edit = QLineEdit()
        self._library_edit.setReadOnly(True)
        self._browse_btn = QPushButton("Choose Photo Folder")
        self._browse_btn.clicked.connect(self.choose_library_requested)
        self._refresh_btn = QPushButton("Refresh Library")
        self._refresh_btn.clicked.connect(self.refresh_requested)
        self._target_path_label = QLabel("")
        self._status_label = QLabel("")

        grid.addWidget(QLabel("Profile:"), 0, 0)
        grid.addWidget(self._profile_combo, 0, 1)
        grid.addWidget(QLabel("Target:"), 0, 2)
        grid.addWidget(self._target_combo, 0, 3)
        grid.addWidget(QLabel("Photo Library:"), 1, 0)
        grid.addWidget(self._library_edit, 1, 1, 1, 2)
        grid.addWidget(self._browse_btn, 1, 3)
        grid.addWidget(self._refresh_btn, 2, 3)
        grid.addWidget(QLabel("Target Path:"), 2, 0)
        grid.addWidget(self._target_path_label, 2, 1, 1, 2)
        grid.addWidget(QLabel("Status:"), 3, 0)
        grid.addWidget(self._status_label, 3, 1, 1, 3)
        layout.addWidget(header)

        body = QHBoxLayout()
        body.setSpacing(10)
        self._photo_list = QListWidget()
        self._photo_list.setObjectName("photo_list")
        self._photo_list.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self._photo_list.itemSelectionChanged.connect(self.selection_changed)
        body.addWidget(self._photo_list, 1)

        right = QVBoxLayout()
        right.setSpacing(6)
        self._preview_label = QLabel("No Preview")
        self._preview_label.setAlignment(Qt.AlignCenter)
        self._preview_label.setMinimumSize(180, 140)
        self._preview_label.setMaximumSize(240, 220)
        self._preview_label.setObjectName("game_cover_preview")
        self._details = QLabel("")
        self._details.setWordWrap(True)
        self._details.setObjectName("theme_hub_status")
        self._diff_label = QLabel("")
        self._diff_label.setObjectName("theme_hub_diff")
        right.addWidget(self._preview_label, 0, Qt.AlignLeft)
        right.addWidget(self._details)
        right.addWidget(self._diff_label)
        right.addStretch(1)

        actions = QHBoxLayout()
        actions.setSpacing(6)
        self._dry_run_btn = QPushButton("Dry Run")
        self._sync_btn = QPushButton("Sync Selected")
        self._hide_btn = QPushButton("Hide Selected")
        self._remove_btn = QPushButton("Remove Selected")
        self._dry_run_btn.clicked.connect(self.dry_run_requested)
        self._sync_btn.clicked.connect(self.sync_requested)
        self._hide_btn.clicked.connect(self.hide_requested)
        self._remove_btn.clicked.connect(self.remove_requested)
        self._hide_btn.setEnabled(False)
        actions.addWidget(self._dry_run_btn)
        actions.addWidget(self._sync_btn)
        actions.addWidget(self._hide_btn)
        actions.addWidget(self._remove_btn)
        right.addLayout(actions)
        body.addLayout(right, 2)
        layout.addLayout(body, 1)

    def set_profiles(self, profiles, selected_id):
        self._profile_combo.blockSignals(True)
        self._profile_combo.clear()
        selected_row = 0
        for row, profile in enumerate(profiles):
            self._profile_combo.addItem(profile["name"], profile["id"])
            if profile["id"] == selected_id:
                selected_row = row
        if profiles:
            self._profile_combo.setCurrentIndex(selected_row)
        self._profile_combo.blockSignals(False)

    def set_targets(self, target_mode, has_device, has_simulator):
        self._target_combo.blockSignals(True)
        self._target_combo.clear()
        if has_device:
            self._target_combo.addItem("Device Mount", "device")
        if has_simulator:
            self._target_combo.addItem("Bound Simulator", "simulator")
        index = self._target_combo.findData(target_mode)
        if index < 0:
            index = 0
        if self._target_combo.count():
            self._target_combo.setCurrentIndex(index)
        self._target_combo.blockSignals(False)

    def set_library_state(self, library_path, target_path, status_text):
        self._library_edit.setText(library_path or "")
        self._target_path_label.setText(target_path or "")
        self._status_label.setText(status_text or "")

    def set_photos(self, photos, selected_ids=None):
        selected_ids = set(selected_ids or [])
        self._photo_list.blockSignals(True)
        self._photo_list.clear()
        for photo in photos:
            flags = []
            if photo.get("missing_source"):
                flags.append("Missing")
            if photo.get("hidden"):
                flags.append("Hidden")
            if photo["on_device"]:
                flags.append("D")
            if photo["on_simulator"]:
                flags.append("S")
            marker = f"[{'/'.join(flags)}] " if flags else ""
            item = QListWidgetItem(f"{marker}{photo['relative_path']}")
            item.setData(Qt.UserRole, photo)
            self._photo_list.addItem(item)
            if photo["id"] in selected_ids:
                item.setSelected(True)
        self._photo_list.blockSignals(False)

    def set_selection_details(self, photos, diff_summary=None):
        if not photos:
            self._details.setText("Select one or more photos.")
            self._diff_label.setText("")
            self._preview_label.setText("No Preview")
            self._preview_label.setPixmap(QPixmap())
            self._hide_btn.setText("Hide Selected")
            self._hide_btn.setEnabled(False)
            return
        first = photos[0]
        modified = (
            datetime.fromtimestamp(first["modified_time"]).strftime("%Y-%m-%d %H:%M")
            if first["modified_time"] > 0
            else "Unknown"
        )
        self._set_preview(first.get("source_path", ""))
        self._details.setText(
            "\n".join(
                [
                    f"Selected: {len(photos)}",
                    f"File: {first['relative_path']}",
                    f"In Library: {'No' if first.get('missing_source') else 'Yes'}",
                    f"Size: {first['size']} bytes",
                    f"Modified: {modified}",
                    f"On Device: {'Yes' if first['on_device'] else 'No'}",
                    f"On Simulator: {'Yes' if first['on_simulator'] else 'No'}",
                    f"Hidden: {'Yes' if first.get('hidden') else 'No'}",
                ]
            )
        )
        self._hide_btn.setEnabled(True)
        self._hide_btn.setText("Unhide Selected" if all(photo.get("hidden") for photo in photos) else "Hide Selected")
        if diff_summary:
            self._diff_label.setText(
                f"Add {diff_summary['add']} · Overwrite {diff_summary['overwrite']} · "
                f"Remove {diff_summary.get('remove', 0)} · Unchanged {diff_summary['unchanged']}"
            )
        else:
            self._diff_label.setText("")

    def selected_photos(self):
        return [item.data(Qt.UserRole) for item in self._photo_list.selectedItems()]

    def current_profile_id(self):
        return self._profile_combo.currentData() or ""

    def current_target_mode(self):
        return self._target_combo.currentData() or "device"

    def _set_preview(self, path):
        if path:
            pixmap = QPixmap(path)
            if not pixmap.isNull():
                self._preview_label.setPixmap(pixmap.scaled(220, 180, Qt.KeepAspectRatio, Qt.SmoothTransformation))
                self._preview_label.setText("")
                return
        self._preview_label.setPixmap(QPixmap())
        self._preview_label.setText("No Preview")

    def _emit_profile_changed(self):
        profile_id = self.current_profile_id()
        if profile_id:
            self.profile_selected.emit(profile_id)

    def _emit_target_changed(self):
        self.target_mode_selected.emit(self.current_target_mode())
