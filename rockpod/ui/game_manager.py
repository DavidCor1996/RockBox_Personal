"""RockPod game library and sync panel."""

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
    QCheckBox,
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
    default_games_changed = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("game_manager")
        self._games = []

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
        self._show_doom_check = QCheckBox("Doom")
        self._show_stickrpg_check = QCheckBox("Stick RPG")
        self._show_runescape_check = QCheckBox("RuneScape Classic")
        self._show_doom_check.toggled.connect(self.default_games_changed)
        self._show_stickrpg_check.toggled.connect(self.default_games_changed)
        self._show_runescape_check.toggled.connect(self.default_games_changed)

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
        builtins = QHBoxLayout()
        builtins.setSpacing(8)
        builtins.addWidget(self._show_doom_check)
        builtins.addWidget(self._show_stickrpg_check)
        builtins.addWidget(self._show_runescape_check)
        builtins.addStretch(1)
        grid.addWidget(QLabel("Show Defaults:"), 6, 0)
        grid.addLayout(builtins, 6, 1, 1, 3)
        layout.addWidget(header)

        body = QHBoxLayout()
        body.setSpacing(10)
        library_column = QVBoxLayout()
        library_column.setSpacing(6)
        browser_bar = QHBoxLayout()
        browser_bar.setSpacing(6)
        self._search_edit = QLineEdit()
        self._search_edit.setPlaceholderText("Search games")
        self._search_edit.setClearButtonEnabled(True)
        self._platform_filter = QComboBox()
        self._platform_filter.addItem("All Platforms", "")
        self._status_filter = QComboBox()
        self._status_filter.addItem("All Games", "all")
        self._status_filter.addItem("Not on Target", "not_on_target")
        self._status_filter.addItem("On Target", "on_target")
        self._status_filter.addItem("Missing Source", "missing_source")
        self._status_filter.addItem("Missing Cover", "missing_cover")
        self._sort_combo = QComboBox()
        self._sort_combo.addItem("Recently Added", "recent")
        self._sort_combo.addItem("Title A–Z", "title")
        self._sort_combo.addItem("Platform", "platform")
        self._sort_combo.addItem("Sync Status", "status")
        self._result_count = QLabel("")
        self._result_count.setObjectName("theme_hub_status")
        browser_bar.addWidget(self._search_edit, 1)
        browser_bar.addWidget(self._platform_filter)
        browser_bar.addWidget(self._status_filter)
        browser_bar.addWidget(self._sort_combo)
        browser_bar.addWidget(self._result_count)
        library_column.addLayout(browser_bar)
        self._game_list = QListWidget()
        self._game_list.setObjectName("game_list")
        self._game_list.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self._game_list.itemSelectionChanged.connect(self.selection_changed)
        library_column.addWidget(self._game_list, 1)
        body.addLayout(library_column, 3)
        self._search_edit.textChanged.connect(self._on_browser_controls_changed)
        self._platform_filter.currentIndexChanged.connect(self._on_browser_controls_changed)
        self._status_filter.currentIndexChanged.connect(self._on_browser_controls_changed)
        self._sort_combo.currentIndexChanged.connect(self._on_browser_controls_changed)

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
        self._sync_btn = QPushButton("Sync Selected")
        self._dry_run_btn = QPushButton("Preview Changes")
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
        actions.addWidget(self._sync_btn)
        actions.addWidget(self._dry_run_btn)
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

    def set_default_game_visibility(self, show_doom, show_stickrpg, show_runescape):
        checks = [
            (self._show_doom_check, show_doom),
            (self._show_stickrpg_check, show_stickrpg),
            (self._show_runescape_check, show_runescape),
        ]
        for check, enabled in checks:
            check.blockSignals(True)
            check.setChecked(bool(enabled))
            check.blockSignals(False)

    def default_game_visibility(self):
        return {
            "games_show_builtin_doom": self._show_doom_check.isChecked(),
            "games_show_builtin_stickrpg": self._show_stickrpg_check.isChecked(),
            "games_show_builtin_runescape": self._show_runescape_check.isChecked(),
        }

    def set_games(self, games, selected_ids=None):
        selected_ids = set(selected_ids or [])
        self._games = list(games or [])
        current_platform = self._platform_filter.currentData() or ""
        platforms = sorted(
            {str(game.get("platform") or "Game") for game in self._games},
            key=str.casefold,
        )
        self._platform_filter.blockSignals(True)
        self._platform_filter.clear()
        self._platform_filter.addItem("All Platforms", "")
        for platform in platforms:
            self._platform_filter.addItem(platform, platform)
        platform_index = self._platform_filter.findData(current_platform)
        self._platform_filter.setCurrentIndex(max(0, platform_index))
        self._platform_filter.blockSignals(False)
        self._rebuild_game_list(selected_ids)

    def _rebuild_game_list(self, selected_ids=None):
        if selected_ids is None:
            selected_ids = {game["id"] for game in self.selected_games()}
        else:
            selected_ids = set(selected_ids)

        query = self._search_edit.text().strip().casefold()
        platform_filter = str(self._platform_filter.currentData() or "")
        status_filter = str(self._status_filter.currentData() or "all")
        target_key = "on_simulator" if self.current_target_mode() == "simulator" else "on_device"

        visible = []
        for game in self._games:
            searchable = " ".join(
                str(game.get(key) or "")
                for key in (
                    "title", "filename", "platform", "genre", "publisher",
                    "game_type", "input_profile_name",
                )
            ).casefold()
            if query and query not in searchable:
                continue
            if platform_filter and game.get("platform") != platform_filter:
                continue
            on_target = bool(game.get(target_key))
            if status_filter == "on_target" and not on_target:
                continue
            if status_filter == "not_on_target" and (on_target or game.get("missing_source")):
                continue
            if status_filter == "missing_source" and not game.get("missing_source"):
                continue
            if status_filter == "missing_cover" and game.get("cover_path"):
                continue
            visible.append(game)

        sort_mode = str(self._sort_combo.currentData() or "recent")
        if sort_mode == "title":
            visible.sort(key=lambda game: str(game.get("title") or game.get("filename") or "").casefold())
        elif sort_mode == "platform":
            visible.sort(key=lambda game: (str(game.get("platform") or "").casefold(), str(game.get("title") or "").casefold()))
        elif sort_mode == "status":
            visible.sort(key=lambda game: (not bool(game.get(target_key)), str(game.get("title") or "").casefold()))
        else:
            visible.sort(key=lambda game: (-float(game.get("added_time") or 0), str(game.get("title") or "").casefold()))

        self._game_list.blockSignals(True)
        self._game_list.clear()
        for game in visible:
            states = []
            if game.get("missing_source"):
                states.append("Missing source")
            else:
                if game.get("on_device"):
                    states.append("Device")
                if game.get("on_simulator"):
                    states.append("Simulator")
                if not states:
                    states.append("Not synced")
            platform = str(game.get("platform") or "Game")
            item = QListWidgetItem(
                f"{game['title']}  ·  {platform}  ·  {' + '.join(states)}"
            )
            item.setData(Qt.UserRole, game)
            item.setToolTip(str(game.get("filename") or game.get("title") or ""))
            self._game_list.addItem(item)
            if game["id"] in selected_ids:
                item.setSelected(True)
        self._game_list.blockSignals(False)
        self._result_count.setText(f"{len(visible)} of {len(self._games)}")

    def _on_browser_controls_changed(self, *_args):
        self._rebuild_game_list()
        self.selection_changed.emit()

    def set_selection_details(self, games, diff_summary=None):
        if not games:
            self._details.setText("Select one or more ROMs.")
            self._diff_label.setText("")
            self._warning_label.clear()
            self._warning_label.hide()
            self._cover_label.setText("No Cover")
            self._cover_label.setPixmap(QPixmap())
            self._set_selection_actions(0, False)
            return
        first = games[0]
        modified = (
            datetime.fromtimestamp(first["modified_time"]).strftime("%Y-%m-%d %H:%M")
            if first["modified_time"] > 0
            else "Unknown"
        )
        added = (
            datetime.fromtimestamp(first["added_time"]).strftime("%Y-%m-%d %H:%M")
            if first.get("added_time", 0) > 0
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
                    f"Added: {added}",
                    f"Modified: {modified}",
                    f"On Device: {'Yes' if first['on_device'] else 'No'}",
                    f"On Simulator: {'Yes' if first['on_simulator'] else 'No'}",
                    f"Save on Device: {'Yes' if first['device_save_exists'] else 'No'}",
                    f"Save on Simulator: {'Yes' if first['simulator_save_exists'] else 'No'}",
                    f"Cover: {'Yes' if first.get('cover_path') else 'No'}",
                    f"Year: {first.get('year') or 'Unknown'}",
                    f"Genre: {first.get('genre') or 'Unknown'}",
                    f"Publisher: {first.get('publisher') or 'Unknown'}",
                    f"Compatibility: {first.get('compatibility') or 'Untested'}",
                    f"Game Type: {str(first.get('game_type') or 'Unknown').title()}",
                    f"Recommended Controls: {first.get('input_profile_name') or 'Default'}",
                    f"Mapper: {first.get('mapper') or 'Unknown'}",
                    f"Special Chip: {first.get('special_chip') or 'None detected'}",
                    f"ROM Hash: {first.get('rom_hash') or 'Not calculated'}",
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
        self._set_selection_actions(
            len(games),
            any(not game.get("missing_source") for game in games),
        )

    def _set_selection_actions(self, count, has_source):
        any_selected = count > 0
        single = count == 1
        self._sync_btn.setEnabled(any_selected and has_source)
        self._dry_run_btn.setEnabled(any_selected and has_source)
        self._remove_btn.setEnabled(any_selected)
        self._backup_saves_btn.setEnabled(any_selected)
        self._restore_saves_btn.setEnabled(any_selected)
        self._export_saves_btn.setEnabled(any_selected)
        self._fetch_cover_btn.setEnabled(single and has_source)
        self._fetch_metadata_btn.setEnabled(single and has_source)
        self._optimize_cover_btn.setEnabled(single and has_source)
        self._launch_sim_btn.setEnabled(single and has_source)

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
