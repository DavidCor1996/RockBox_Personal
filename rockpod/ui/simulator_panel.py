"""Rockbox simulator management panel."""

from __future__ import annotations

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class SimulatorPanel(QWidget):
    simulator_selected = Signal(str)
    profile_selected = Signal(str)
    bind_requested = Signal()
    dry_run_requested = Signal()
    apply_requested = Signal()
    launch_requested = Signal()
    capture_requested = Signal()
    open_screenshots_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("simulator_panel")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(8)

        header = QFrame()
        header.setObjectName("theme_hub_header")
        grid = QGridLayout(header)
        grid.setContentsMargins(10, 8, 10, 8)
        grid.setHorizontalSpacing(8)
        grid.setVerticalSpacing(4)

        self._sim_combo = QComboBox()
        self._sim_combo.currentIndexChanged.connect(self._emit_simulator_changed)
        self._profile_combo = QComboBox()
        self._profile_combo.currentIndexChanged.connect(self._emit_profile_changed)
        self._theme_label = QLabel("")
        self._binary_label = QLabel("")
        self._simdisk_label = QLabel("")
        self._shots_label = QLabel("")
        self._diff_label = QLabel("")
        self._diff_label.setObjectName("theme_hub_diff")

        self._bind_btn = QPushButton("Bind To Profile")
        self._dry_run_btn = QPushButton("Dry Run to Simulator")
        self._apply_btn = QPushButton("Apply to Simulator")
        self._launch_btn = QPushButton("Launch Simulator")
        self._capture_btn = QPushButton("Capture Screenshot")
        self._open_shots_btn = QPushButton("Open simshots Folder")
        self._bind_btn.clicked.connect(self.bind_requested)
        self._dry_run_btn.clicked.connect(self.dry_run_requested)
        self._apply_btn.clicked.connect(self.apply_requested)
        self._launch_btn.clicked.connect(self.launch_requested)
        self._capture_btn.clicked.connect(self.capture_requested)
        self._open_shots_btn.clicked.connect(self.open_screenshots_requested)

        grid.addWidget(QLabel("Simulator:"), 0, 0)
        grid.addWidget(self._sim_combo, 0, 1, 1, 2)
        grid.addWidget(QLabel("Profile:"), 1, 0)
        grid.addWidget(self._profile_combo, 1, 1, 1, 2)
        grid.addWidget(QLabel("Theme:"), 2, 0)
        grid.addWidget(self._theme_label, 2, 1, 1, 2)
        grid.addWidget(QLabel("Binary:"), 3, 0)
        grid.addWidget(self._binary_label, 3, 1, 1, 2)
        grid.addWidget(QLabel("simdisk:"), 4, 0)
        grid.addWidget(self._simdisk_label, 4, 1, 1, 2)
        grid.addWidget(QLabel("Screenshots:"), 5, 0)
        grid.addWidget(self._shots_label, 5, 1, 1, 2)
        grid.addWidget(self._bind_btn, 6, 0)
        grid.addWidget(self._dry_run_btn, 6, 1)
        grid.addWidget(self._apply_btn, 6, 2)
        grid.addWidget(self._launch_btn, 7, 0)
        grid.addWidget(self._capture_btn, 7, 1)
        grid.addWidget(self._open_shots_btn, 7, 2)
        layout.addWidget(header)
        layout.addWidget(self._diff_label)
        layout.addStretch(1)

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

    def set_simulators(self, targets, selected_id):
        self._sim_combo.blockSignals(True)
        self._sim_combo.clear()
        selected_row = 0
        for row, target in enumerate(targets):
            label = f"{target['name']} ({target['screen_resolution']})"
            self._sim_combo.addItem(label, target["id"])
            if target["id"] == selected_id:
                selected_row = row
        if targets:
            self._sim_combo.setCurrentIndex(selected_row)
        self._sim_combo.blockSignals(False)

    def set_details(self, target, profile, screenshot_dir, diff_summary=None):
        self._theme_label.setText(profile.get("selected_theme", ""))
        self._binary_label.setText(target.get("binary_path", "") if target else "")
        self._simdisk_label.setText(target.get("simdisk_path", "") if target else "")
        self._shots_label.setText(screenshot_dir or "")
        if diff_summary:
            self._diff_label.setText(
                f"Add {diff_summary['add']} · Overwrite {diff_summary['overwrite']} · "
                f"Unchanged {diff_summary['unchanged']} · Missing {diff_summary['missing_source']}"
            )
        else:
            self._diff_label.setText("")

    def current_simulator_id(self):
        return self._sim_combo.currentData() or ""

    def current_profile_id(self):
        return self._profile_combo.currentData() or ""

    def _emit_simulator_changed(self):
        value = self.current_simulator_id()
        if value:
            self.simulator_selected.emit(value)

    def _emit_profile_changed(self):
        value = self.current_profile_id()
        if value:
            self.profile_selected.emit(value)
