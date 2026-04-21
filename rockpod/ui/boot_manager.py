"""Boot / branding management panel."""

from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class BootManagerWidget(QWidget):
    profile_selected = Signal(str)
    target_mode_selected = Signal(str)
    choose_image_requested = Signal()
    dry_run_requested = Signal()
    apply_requested = Signal()
    restore_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("boot_manager")

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
        self._image_edit = QLineEdit()
        self._image_edit.setReadOnly(True)
        self._browse_btn = QPushButton("Choose Image")
        self._browse_btn.clicked.connect(self.choose_image_requested)
        self._required_label = QLabel("")
        self._dest_label = QLabel("")
        self._status_label = QLabel("")
        self._diff_label = QLabel("")
        self._diff_label.setObjectName("theme_hub_diff")

        grid.addWidget(QLabel("Profile:"), 0, 0)
        grid.addWidget(self._profile_combo, 0, 1)
        grid.addWidget(QLabel("Target:"), 0, 2)
        grid.addWidget(self._target_combo, 0, 3)
        grid.addWidget(QLabel("Source Image:"), 1, 0)
        grid.addWidget(self._image_edit, 1, 1, 1, 2)
        grid.addWidget(self._browse_btn, 1, 3)
        grid.addWidget(QLabel("Required Size:"), 2, 0)
        grid.addWidget(self._required_label, 2, 1, 1, 3)
        grid.addWidget(QLabel("Deploy Path:"), 3, 0)
        grid.addWidget(self._dest_label, 3, 1, 1, 3)
        grid.addWidget(QLabel("Status:"), 4, 0)
        grid.addWidget(self._status_label, 4, 1, 1, 3)
        layout.addWidget(header)

        self._preview = QLabel("No Preview")
        self._preview.setObjectName("theme_preview")
        self._preview.setAlignment(Qt.AlignCenter)
        self._preview.setMinimumHeight(240)
        layout.addWidget(self._preview)

        footer = QHBoxLayout()
        footer.setSpacing(6)
        self._dry_run_btn = QPushButton("Dry Run")
        self._apply_btn = QPushButton("Apply Branding")
        self._restore_btn = QPushButton("Restore Previous")
        self._dry_run_btn.clicked.connect(self.dry_run_requested)
        self._apply_btn.clicked.connect(self.apply_requested)
        self._restore_btn.clicked.connect(self.restore_requested)
        footer.addWidget(self._diff_label, 1)
        footer.addWidget(self._restore_btn)
        footer.addWidget(self._dry_run_btn)
        footer.addWidget(self._apply_btn)
        layout.addLayout(footer)

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

    def set_details(self, image_path, required_size, destination_rel, status, preview_path="", diff_summary=None):
        self._image_edit.setText(image_path or "")
        self._required_label.setText(required_size or "")
        self._dest_label.setText(destination_rel or "")
        self._status_label.setText(status or "")
        if preview_path:
            px = QPixmap(preview_path)
            if not px.isNull():
                self._preview.setPixmap(px)
                self._preview.setText("")
            else:
                self._preview.setPixmap(QPixmap())
                self._preview.setText("No Preview")
        else:
            self._preview.setPixmap(QPixmap())
            self._preview.setText("No Preview")
        if diff_summary:
            self._diff_label.setText(
                f"Add {diff_summary['add']} · Overwrite {diff_summary['overwrite']} · "
                f"Unchanged {diff_summary['unchanged']} · Missing {diff_summary['missing_source']}"
            )
        else:
            self._diff_label.setText("")

    def current_profile_id(self):
        return self._profile_combo.currentData() or ""

    def current_target_mode(self):
        return self._target_combo.currentData() or "device"

    def _emit_profile_changed(self):
        profile_id = self.current_profile_id()
        if profile_id:
            self.profile_selected.emit(profile_id)

    def _emit_target_changed(self):
        self.target_mode_selected.emit(self.current_target_mode())
