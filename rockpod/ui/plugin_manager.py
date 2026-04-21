"""Rockbox plugin manager panel."""

from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)


class PluginManagerWidget(QWidget):
    profile_selected = Signal(str)
    target_mode_selected = Signal(str)
    plugin_selected = Signal(str)
    dry_run_requested = Signal()
    apply_requested = Signal()
    remove_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("plugin_manager")
        self._plugins = {}

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
        self._status_label = QLabel("")
        self._source_label = QLabel("")
        self._dest_label = QLabel("")
        self._sim_label = QLabel("")

        grid.addWidget(QLabel("Profile:"), 0, 0)
        grid.addWidget(self._profile_combo, 0, 1)
        grid.addWidget(QLabel("Target:"), 0, 2)
        grid.addWidget(self._target_combo, 0, 3)
        grid.addWidget(QLabel("Source:"), 1, 0)
        grid.addWidget(self._source_label, 1, 1, 1, 3)
        grid.addWidget(QLabel("Deploy Path:"), 2, 0)
        grid.addWidget(self._dest_label, 2, 1, 1, 3)
        grid.addWidget(QLabel("Status:"), 3, 0)
        grid.addWidget(self._status_label, 3, 1)
        grid.addWidget(QLabel("Simulator:"), 3, 2)
        grid.addWidget(self._sim_label, 3, 3)
        layout.addWidget(header)

        content = QHBoxLayout()
        content.setSpacing(10)

        self._plugin_list = QListWidget()
        self._plugin_list.currentItemChanged.connect(self._on_plugin_changed)
        content.addWidget(self._plugin_list, 1)

        right = QVBoxLayout()
        right.setSpacing(6)
        self._summary = QLabel("")
        self._summary.setWordWrap(True)
        self._summary.setObjectName("theme_hub_status")
        right.addWidget(self._summary)

        self._asset_tree = QTreeWidget()
        self._asset_tree.setHeaderLabels(["Kind", "Path"])
        self._asset_tree.setRootIsDecorated(False)
        right.addWidget(self._asset_tree, 1)

        self._diff_label = QLabel("")
        self._diff_label.setObjectName("theme_hub_diff")
        right.addWidget(self._diff_label)

        actions = QHBoxLayout()
        actions.setSpacing(6)
        self._dry_run_btn = QPushButton("Dry Run")
        self._apply_btn = QPushButton("Deploy Plugin")
        self._remove_btn = QPushButton("Remove Plugin")
        self._dry_run_btn.clicked.connect(self.dry_run_requested)
        self._apply_btn.clicked.connect(self.apply_requested)
        self._remove_btn.clicked.connect(self.remove_requested)
        actions.addWidget(self._dry_run_btn)
        actions.addWidget(self._apply_btn)
        actions.addWidget(self._remove_btn)
        right.addLayout(actions)

        content.addLayout(right, 2)
        layout.addLayout(content, 1)

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

    def set_plugins(self, plugins, selected_id=""):
        self._plugins = {plugin["id"]: plugin for plugin in plugins}
        self._plugin_list.blockSignals(True)
        self._plugin_list.clear()
        selected_row = 0
        for row, plugin in enumerate(plugins):
            prefix = "★ " if plugin["custom"] else ""
            suffix = "" if plugin["binary_exists"] else " (missing build)"
            item = QListWidgetItem(f"{prefix}{plugin['display_name']}{suffix}")
            item.setData(Qt.UserRole, plugin["id"])
            if plugin["id"] == selected_id:
                selected_row = row
            self._plugin_list.addItem(item)
        if plugins:
            self._plugin_list.setCurrentRow(selected_row)
        self._plugin_list.blockSignals(False)

    def set_plugin_details(self, plugin, diff_summary=None):
        self._status_label.setText(f"{plugin['status']} / {plugin['category']}")
        self._source_label.setText(plugin["binary_path"] or plugin["source_path"])
        self._dest_label.setText(plugin["destination_rel"])
        self._sim_label.setText("Yes" if plugin["simulator_supported"] else "No")
        deps = ", ".join(plugin["dependencies"]) if plugin["dependencies"] else "None"
        self._summary.setText(
            f"{plugin['summary'] or plugin['display_name']}\n"
            f"Dependencies: {deps}"
        )
        self._asset_tree.clear()
        self._asset_tree.addTopLevelItem(QTreeWidgetItem(["binary", plugin["destination_rel"]]))
        for asset_path in plugin.get("asset_source_paths", []):
            self._asset_tree.addTopLevelItem(
                QTreeWidgetItem(["asset", f"{plugin['asset_destination_dir']}/{asset_path.split('/')[-1]}"])
            )
        for idx in range(2):
            self._asset_tree.resizeColumnToContents(idx)
        if diff_summary:
            text = (
                f"Add {diff_summary['add']} · Overwrite {diff_summary['overwrite']} · "
                f"Remove {diff_summary.get('remove', 0)} · Unchanged {diff_summary['unchanged']} · "
                f"Missing {diff_summary['missing_source']}"
            )
        else:
            text = ""
        self._diff_label.setText(text)

    def current_profile_id(self):
        return self._profile_combo.currentData() or ""

    def current_target_mode(self):
        return self._target_combo.currentData() or "device"

    def current_plugin_id(self):
        item = self._plugin_list.currentItem()
        return item.data(Qt.UserRole) if item else ""

    def _emit_profile_changed(self):
        profile_id = self.current_profile_id()
        if profile_id:
            self.profile_selected.emit(profile_id)

    def _emit_target_changed(self):
        self.target_mode_selected.emit(self.current_target_mode())

    def _on_plugin_changed(self, current, _previous):
        if current:
            self.plugin_selected.emit(current.data(Qt.UserRole))
