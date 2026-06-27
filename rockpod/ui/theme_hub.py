"""Rockbox theme management hub."""

from __future__ import annotations

import os

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)


class ThemeHubWidget(QWidget):
    """Profile-driven Rockbox theme deploy UI."""

    profile_selected = Signal(str)
    profile_saved = Signal(dict)
    theme_selected = Signal(str)
    deploy_requested = Signal()
    delete_requested = Signal()
    restore_requested = Signal()
    reset_default_requested = Signal()
    use_connected_device_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("theme_hub")
        self._profiles = {}
        self._themes = {}
        self._current_theme_id = ""

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(8)

        header = QFrame()
        header.setObjectName("theme_hub_header")
        header_layout = QGridLayout(header)
        header_layout.setContentsMargins(10, 8, 10, 8)
        header_layout.setHorizontalSpacing(8)
        header_layout.setVerticalSpacing(4)

        self._profile_combo = QComboBox()
        self._profile_combo.currentIndexChanged.connect(self._emit_profile_changed)
        self._mount_edit = QLineEdit()
        self._model_edit = QLineEdit()
        self._resolution_combo = QComboBox()
        self._resolution_combo.addItems(["320x240", "176x132"])
        self._source_label = QLabel("")
        self._backup_label = QLabel("")
        self._save_profile_btn = QPushButton("Save Profile")
        self._save_profile_btn.clicked.connect(self._emit_profile_saved)
        self._use_device_btn = QPushButton("Use Connected Device")
        self._use_device_btn.clicked.connect(self.use_connected_device_requested)

        header_layout.addWidget(QLabel("Profile:"), 0, 0)
        header_layout.addWidget(self._profile_combo, 0, 1)
        header_layout.addWidget(self._use_device_btn, 0, 2)
        header_layout.addWidget(QLabel("Mount Path:"), 1, 0)
        header_layout.addWidget(self._mount_edit, 1, 1, 1, 2)
        header_layout.addWidget(QLabel("Device Model:"), 2, 0)
        header_layout.addWidget(self._model_edit, 2, 1)
        header_layout.addWidget(QLabel("Resolution:"), 2, 2)
        header_layout.addWidget(self._resolution_combo, 2, 3)
        header_layout.addWidget(QLabel("Source Repo:"), 3, 0)
        header_layout.addWidget(self._source_label, 3, 1, 1, 3)
        header_layout.addWidget(QLabel("Backup Root:"), 4, 0)
        header_layout.addWidget(self._backup_label, 4, 1, 1, 2)
        header_layout.addWidget(self._save_profile_btn, 4, 3)
        layout.addWidget(header)

        content = QHBoxLayout()
        content.setSpacing(10)

        left_col = QVBoxLayout()
        left_col.setSpacing(6)
        self._theme_list = QListWidget()
        self._theme_list.currentItemChanged.connect(self._on_theme_changed)
        left_col.addWidget(QLabel("Themes"))
        left_col.addWidget(self._theme_list, 1)
        self._status = QLabel("Select a profile and theme.")
        self._status.setObjectName("theme_hub_status")
        self._status.setWordWrap(True)
        left_col.addWidget(self._status)
        content.addLayout(left_col, 1)

        right_col = QVBoxLayout()
        right_col.setSpacing(6)
        self._preview = QLabel("No Preview")
        self._preview.setObjectName("theme_preview")
        self._preview.setAlignment(Qt.AlignCenter)
        self._preview.setMinimumHeight(210)
        right_col.addWidget(self._preview)

        self._asset_tree = QTreeWidget()
        self._asset_tree.setHeaderLabels(["Type", "Status", "Destination"])
        self._asset_tree.setRootIsDecorated(False)
        self._asset_tree.itemSelectionChanged.connect(self._on_asset_selection_changed)
        right_col.addWidget(self._asset_tree, 1)

        deploy_row = QHBoxLayout()
        deploy_row.setSpacing(6)
        self._diff_summary = QLabel("")
        self._diff_summary.setObjectName("theme_hub_diff")
        self._deploy_btn = QPushButton("Deploy Theme")
        self._deploy_btn.clicked.connect(self.deploy_requested)
        self._delete_btn = QPushButton("Delete From Device")
        self._delete_btn.clicked.connect(self.delete_requested)
        self._reset_default_btn = QPushButton("Reset To Default")
        self._reset_default_btn.clicked.connect(self.reset_default_requested)
        self._restore_btn = QPushButton("Restore Previous")
        self._restore_btn.clicked.connect(self.restore_requested)
        deploy_row.addWidget(self._diff_summary, 1)
        deploy_row.addWidget(self._delete_btn)
        deploy_row.addWidget(self._reset_default_btn)
        deploy_row.addWidget(self._restore_btn)
        deploy_row.addWidget(self._deploy_btn)
        right_col.addLayout(deploy_row)

        content.addLayout(right_col, 2)
        layout.addLayout(content, 1)

    def set_profiles(self, profiles, selected_id):
        self._profiles = {item["id"]: item for item in profiles}
        self._profile_combo.blockSignals(True)
        self._profile_combo.clear()
        selected_index = 0
        for index, profile in enumerate(profiles):
            self._profile_combo.addItem(profile["name"], profile["id"])
            if profile["id"] == selected_id:
                selected_index = index
        self._profile_combo.setCurrentIndex(selected_index)
        self._profile_combo.blockSignals(False)
        if profiles:
            self.load_profile(self._profile_combo.currentData())

    def load_profile(self, profile_id):
        profile = self._profiles.get(profile_id)
        if not profile:
            return
        self._mount_edit.setText(profile["device_mount_path"])
        self._model_edit.setText(profile["target_device_model"])
        self._resolution_combo.setCurrentText(profile["screen_resolution"])
        self._source_label.setText(profile["source_repo_path"])
        self._backup_label.setText(profile["backup_location"])

    def current_profile_data(self):
        profile_id = self._profile_combo.currentData() or ""
        existing = dict(self._profiles.get(profile_id, {}))
        existing.update(
            {
                "id": profile_id or "rockbox-profile",
                "name": self._profile_combo.currentText() or existing.get("name") or "Rockbox Device",
                "device_mount_path": self._mount_edit.text().strip(),
                "target_device_model": self._model_edit.text().strip() or existing.get("target_device_model", ""),
                "screen_resolution": self._resolution_combo.currentText().strip(),
                "source_repo_path": existing.get("source_repo_path", ""),
                "selected_theme": self._current_theme_id or existing.get("selected_theme", "iPone"),
                "backup_location": existing.get("backup_location", ""),
            }
        )
        return existing

    def set_themes(self, themes, selected_theme_id):
        self._themes = {item["id"]: item for item in themes}
        self._theme_list.blockSignals(True)
        self._theme_list.clear()
        selected_row = 0
        for row, theme in enumerate(themes):
            label = f"{theme['name']} ({theme['found_count']}/{theme['asset_count']})"
            item = QListWidgetItem(label)
            item.setData(Qt.UserRole, theme["id"])
            item.setToolTip(theme["description"])
            self._theme_list.addItem(item)
            if theme["id"] == selected_theme_id:
                selected_row = row
        if themes:
            self._theme_list.setCurrentRow(selected_row)
        self._theme_list.blockSignals(False)

    def set_theme_details(self, details):
        self._current_theme_id = details["id"]
        self._status.setText(
            f"{details['description']}\nStatus: {details['status']} · "
            f"{details['found_count']}/{details['asset_count']} assets present"
        )
        self._set_preview(details.get("preview_path", ""))
        self._asset_tree.clear()
        for asset in details.get("assets", []):
            item = QTreeWidgetItem(
                [
                    asset["kind"],
                    "Present" if asset["exists"] else "Missing",
                    asset["destination_rel"],
                ]
            )
            item.setData(0, Qt.UserRole, asset.get("preview_path", ""))
            item.setToolTip(0, asset["source_rel"])
            self._asset_tree.addTopLevelItem(item)
        for index in range(3):
            self._asset_tree.resizeColumnToContents(index)

    def set_diff_summary(self, diff_report):
        if not diff_report:
            self._diff_summary.setText("")
            return
        summary = diff_report["summary"]
        self._diff_summary.setText(
            f"Add {summary['add']} · Overwrite {summary['overwrite']} · "
            f"Unchanged {summary['unchanged']} · Missing {summary['missing_source']}"
        )

    def _emit_profile_changed(self):
        profile_id = self._profile_combo.currentData()
        self.load_profile(profile_id)
        if profile_id:
            self.profile_selected.emit(profile_id)

    def _emit_profile_saved(self):
        self.profile_saved.emit(self.current_profile_data())

    def _on_theme_changed(self, current, previous):
        if not current:
            return
        theme_id = current.data(Qt.UserRole)
        self._current_theme_id = theme_id
        self.theme_selected.emit(theme_id)

    def _on_asset_selection_changed(self):
        current = self._asset_tree.currentItem()
        if not current:
            return
        self._set_preview(current.data(0, Qt.UserRole) or "")

    def _set_preview(self, path):
        if path and os.path.isfile(path) and path.lower().endswith((".bmp", ".png", ".jpg", ".jpeg")):
            px = QPixmap(path)
            if not px.isNull():
                self._preview.setPixmap(px.scaled(340, 220, Qt.KeepAspectRatio, Qt.SmoothTransformation))
                return
        self._preview.setPixmap(QPixmap())
        self._preview.setText("No Preview")
