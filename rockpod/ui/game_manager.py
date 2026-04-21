"""Rockboy game sync panel."""

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


class GameManagerWidget(QWidget):
    profile_selected = Signal(str)
    target_mode_selected = Signal(str)
    choose_library_requested = Signal()
    refresh_requested = Signal()
    selection_changed = Signal()
    dry_run_requested = Signal()
    sync_requested = Signal()
    remove_requested = Signal()
    backup_saves_requested = Signal()
    restore_saves_requested = Signal()
    export_saves_requested = Signal()
    import_saves_requested = Signal()
    fetch_cover_requested = Signal()
    fetch_metadata_requested = Signal()
    optimize_cover_requested = Signal()
    launch_simulator_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("game_manager")

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
        self._browse_btn = QPushButton("Choose ROM Folder")
        self._browse_btn.clicked.connect(self.choose_library_requested)
        self._refresh_btn = QPushButton("Refresh Library")
        self._refresh_btn.clicked.connect(self.refresh_requested)
        self._target_path_label = QLabel("")
        self._status_label = QLabel("")
        self._backup_label = QLabel("")
        self._backup_label.setObjectName("theme_hub_diff")
        self._settings_help = QLabel("")
        self._settings_help.setWordWrap(True)
        self._settings_help.setObjectName("theme_hub_status")

        grid.addWidget(QLabel("Profile:"), 0, 0)
        grid.addWidget(self._profile_combo, 0, 1)
        grid.addWidget(QLabel("Target:"), 0, 2)
        grid.addWidget(self._target_combo, 0, 3)
        grid.addWidget(QLabel("ROM Library:"), 1, 0)
        grid.addWidget(self._library_edit, 1, 1, 1, 2)
        grid.addWidget(self._browse_btn, 1, 3)
        grid.addWidget(self._refresh_btn, 2, 3)
        grid.addWidget(QLabel("Target Path:"), 2, 0)
        grid.addWidget(self._target_path_label, 2, 1, 1, 2)
        grid.addWidget(QLabel("Status:"), 3, 0)
        grid.addWidget(self._status_label, 3, 1, 1, 3)
        grid.addWidget(QLabel("Last Save Backup:"), 4, 0)
        grid.addWidget(self._backup_label, 4, 1, 1, 3)
        grid.addWidget(QLabel("Rockboy Tips:"), 5, 0)
        grid.addWidget(self._settings_help, 5, 1, 1, 3)
        layout.addWidget(header)

        body = QHBoxLayout()
        body.setSpacing(10)
        self._game_list = QListWidget()
        self._game_list.setObjectName("game_list")
        self._game_list.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self._game_list.itemSelectionChanged.connect(self.selection_changed)
        body.addWidget(self._game_list, 1)

        right = QVBoxLayout()
        right.setSpacing(6)
        self._cover_label = QLabel("No Cover")
        self._cover_label.setAlignment(Qt.AlignCenter)
        self._cover_label.setMinimumSize(132, 132)
        self._cover_label.setMaximumSize(180, 180)
        self._cover_label.setObjectName("game_cover_preview")
        self._details = QLabel("")
        self._details.setWordWrap(True)
        self._details.setObjectName("theme_hub_status")
        self._warning_label = QLabel("")
        self._warning_label.setWordWrap(True)
        self._warning_label.setObjectName("game_warning_strip")
        self._warning_label.hide()
        self._diff_label = QLabel("")
        self._diff_label.setObjectName("theme_hub_diff")
        right.addWidget(self._cover_label, 0, Qt.AlignLeft)
        right.addWidget(self._warning_label)
        right.addWidget(self._details)
        right.addWidget(self._diff_label)
        right.addStretch(1)

        actions = QHBoxLayout()
        actions.setSpacing(6)
        self._dry_run_btn = QPushButton("Dry Run")
        self._sync_btn = QPushButton("Sync Selected")
        self._remove_btn = QPushButton("Remove Selected")
        self._backup_saves_btn = QPushButton("Backup Saves")
        self._restore_saves_btn = QPushButton("Restore Saves")
        self._export_saves_btn = QPushButton("Export Saves")
        self._import_saves_btn = QPushButton("Import Saves")
        self._fetch_cover_btn = QPushButton("Fetch Cover")
        self._fetch_metadata_btn = QPushButton("Fetch Metadata")
        self._optimize_cover_btn = QPushButton("Optimize Cover")
        self._launch_sim_btn = QPushButton("Launch in Simulator")
        self._dry_run_btn.clicked.connect(self.dry_run_requested)
        self._sync_btn.clicked.connect(self.sync_requested)
        self._remove_btn.clicked.connect(self.remove_requested)
        self._backup_saves_btn.clicked.connect(self.backup_saves_requested)
        self._restore_saves_btn.clicked.connect(self.restore_saves_requested)
        self._export_saves_btn.clicked.connect(self.export_saves_requested)
        self._import_saves_btn.clicked.connect(self.import_saves_requested)
        self._fetch_cover_btn.clicked.connect(self.fetch_cover_requested)
        self._fetch_metadata_btn.clicked.connect(self.fetch_metadata_requested)
        self._optimize_cover_btn.clicked.connect(self.optimize_cover_requested)
        self._launch_sim_btn.clicked.connect(self.launch_simulator_requested)
        actions.addWidget(self._dry_run_btn)
        actions.addWidget(self._sync_btn)
        actions.addWidget(self._remove_btn)
        actions.addWidget(self._launch_sim_btn)
        right.addLayout(actions)
        actions2 = QHBoxLayout()
        actions2.setSpacing(6)
        actions2.addWidget(self._backup_saves_btn)
        actions2.addWidget(self._restore_saves_btn)
        actions2.addWidget(self._export_saves_btn)
        actions2.addWidget(self._import_saves_btn)
        actions2.addWidget(self._fetch_cover_btn)
        actions2.addWidget(self._fetch_metadata_btn)
        actions2.addWidget(self._optimize_cover_btn)
        right.addLayout(actions2)
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

    def set_library_state(self, library_path, target_path, status_text, settings_help=None, last_backup_text=""):
        self._library_edit.setText(library_path or "")
        self._target_path_label.setText(target_path or "")
        self._status_label.setText(status_text or "")
        self._settings_help.setText("\n".join(settings_help or []))
        self._backup_label.setText(last_backup_text or "None")

    def set_games(self, games, selected_ids=None):
        selected_ids = set(selected_ids or [])
        self._game_list.blockSignals(True)
        self._game_list.clear()
        for game in games:
            flags = []
            if game.get("missing_source"):
                flags.append("Missing")
            if game["on_device"]:
                flags.append("D")
            if game["on_simulator"]:
                flags.append("S")
            marker = f"[{'/'.join(flags)}] " if flags else ""
            item = QListWidgetItem(f"{marker}{game['title']}")
            item.setData(Qt.UserRole, game)
            self._game_list.addItem(item)
            if game["id"] in selected_ids:
                item.setSelected(True)
        self._game_list.blockSignals(False)

    def set_selection_details(self, games, diff_summary=None):
        if not games:
            self._details.setText("Select one or more ROMs.")
            self._diff_label.setText("")
            self._warning_label.clear()
            self._warning_label.hide()
            self._cover_label.setText("No Cover")
            self._cover_label.setPixmap(QPixmap())
            return
        first = games[0]
        modified = (
            datetime.fromtimestamp(first["modified_time"]).strftime("%Y-%m-%d %H:%M")
            if first["modified_time"] > 0
            else "Unknown"
        )
        self._set_cover(first.get("cover_path", ""))
        perf = first.get("performance", {})
        perf_level = perf.get("level", "ok").upper()
        perf_notes = perf.get("notes", [])
        if perf_level in {"WARN", "CRITICAL"} and perf_notes:
            self._warning_label.setText("\n".join(perf_notes))
            self._warning_label.show()
        else:
            self._warning_label.clear()
            self._warning_label.hide()
        self._details.setText(
            "\n".join(
                [
                    f"Selected: {len(games)}",
                    f"Title: {first['title']}",
                    f"Filename: {first['filename']}",
                    f"In Library: {'No' if first.get('missing_source') else 'Yes'}",
                    f"Size: {first['size']} bytes",
                    f"Modified: {modified}",
                    f"On Device: {'Yes' if first['on_device'] else 'No'}",
                    f"On Simulator: {'Yes' if first['on_simulator'] else 'No'}",
                    f"Save on Device: {'Yes' if first['device_save_exists'] else 'No'}",
                    f"Save on Simulator: {'Yes' if first['simulator_save_exists'] else 'No'}",
                    f"Cover: {'Yes' if first.get('cover_path') else 'No'}",
                    f"Year: {first.get('year') or 'Unknown'}",
                    f"Genre: {first.get('genre') or 'Unknown'}",
                    f"Publisher: {first.get('publisher') or 'Unknown'}",
                    f"Performance: {perf_level}",
                ]
            )
        )
        if diff_summary:
            diff_text = (
                f"Add {diff_summary['add']} · Overwrite {diff_summary['overwrite']} · "
                f"Remove {diff_summary.get('remove', 0)} · Unchanged {diff_summary['unchanged']} · "
                f"Missing {diff_summary['missing_source']}"
            )
            if perf_level in {"WARN", "CRITICAL"}:
                diff_text = f"{perf_level} · {diff_text}"
            self._diff_label.setText(diff_text)
        else:
            self._diff_label.setText(perf_level if perf_level in {"WARN", "CRITICAL"} else "")

    def selected_games(self):
        return [item.data(Qt.UserRole) for item in self._game_list.selectedItems()]

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

    def _set_cover(self, path):
        if path:
            pixmap = QPixmap(path)
            if not pixmap.isNull():
                self._cover_label.setPixmap(
                    pixmap.scaled(
                        132,
                        132,
                        Qt.KeepAspectRatio,
                        Qt.SmoothTransformation,
                    )
                )
                self._cover_label.setText("")
                return
        self._cover_label.setPixmap(QPixmap())
        self._cover_label.setText("No Cover")
