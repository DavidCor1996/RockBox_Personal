"""Preferences dialog — styled after iTunes 7 preferences."""

from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QFormLayout, QLabel, QLineEdit,
    QPushButton, QCheckBox, QComboBox, QDialogButtonBox, QFileDialog,
    QGroupBox, QTabWidget, QWidget, QPlainTextEdit,
)
from PySide6.QtCore import Qt, Signal


class PreferencesDialog(QDialog):
    """Settings dialog for RockPod."""

    settings_saved = Signal(dict)  # dict of changed settings

    def __init__(self, config, parent=None):
        super().__init__(parent)
        self.setWindowTitle("RockPod Preferences")
        self.setMinimumSize(520, 400)
        self.setModal(True)
        self._config = config

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(8)

        tabs = QTabWidget()
        layout.addWidget(tabs)

        # General tab
        tabs.addTab(self._build_general_tab(), "General")

        # Sync tab
        tabs.addTab(self._build_sync_tab(), "Sync")

        # Device tab
        tabs.addTab(self._build_device_tab(), "Device")

        # Appearance tab
        tabs.addTab(self._build_appearance_tab(), "Appearance")

        # Advanced tab
        tabs.addTab(self._build_advanced_tab(), "Advanced")

        # Buttons
        btn_box = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        btn_box.accepted.connect(self._on_ok)
        btn_box.rejected.connect(self.reject)
        layout.addWidget(btn_box)

    def _build_general_tab(self):
        w = QWidget()
        layout = QFormLayout(w)
        layout.setSpacing(8)

        # Music folder
        dir_row = QHBoxLayout()
        self._music_dir_edit = QLineEdit(self._config.music_dir)
        dir_row.addWidget(self._music_dir_edit)
        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_music_dir)
        dir_row.addWidget(browse_btn)
        layout.addRow("Music Folder:", dir_row)

        video_col = QVBoxLayout()
        self._video_dirs_edit = QPlainTextEdit()
        self._video_dirs_edit.setPlaceholderText("One video folder per line")
        self._video_dirs_edit.setFixedHeight(82)
        self._video_dirs_edit.setPlainText("\n".join(self._config.get("video_dirs", []) or []))
        video_col.addWidget(self._video_dirs_edit)
        video_buttons = QHBoxLayout()
        video_browse_btn = QPushButton("Add Folder...")
        video_browse_btn.clicked.connect(self._browse_video_dir)
        video_buttons.addWidget(video_browse_btn)
        clear_videos_btn = QPushButton("Clear")
        clear_videos_btn.clicked.connect(self._video_dirs_edit.clear)
        video_buttons.addWidget(clear_videos_btn)
        video_buttons.addStretch(1)
        video_col.addLayout(video_buttons)
        layout.addRow("Video Folders:", video_col)

        self._scan_on_startup = QCheckBox("Scan library on startup")
        self._scan_on_startup.setChecked(self._config.scan_on_startup)
        layout.addRow("", self._scan_on_startup)

        self._browser_home_edit = QLineEdit(self._config.get("browser_home_url", "https://www.rockbox.org/"))
        layout.addRow("Store Home URL:", self._browser_home_edit)

        return w

    def _build_appearance_tab(self):
        w = QWidget()
        layout = QFormLayout(w)
        layout.setSpacing(8)

        self._theme_mode = QComboBox()
        self._theme_mode.addItems(["Default Theme", "Personal iTunes Theme"])
        self._theme_mode.setCurrentIndex(1 if self._config.get("theme_mode", "default") == "personal" else 0)
        layout.addRow("Theme:", self._theme_mode)

        self._personal_path = QLineEdit("assets/theme_itunes_personal/")
        self._personal_path.setReadOnly(True)
        layout.addRow("Personal Pack:", self._personal_path)

        reset_btn = QPushButton("Reset To Default")
        reset_btn.clicked.connect(self._reset_asset_pack)
        layout.addRow("", reset_btn)

        note = QLabel(
            "Drop user-supplied iTunes-era assets into assets/theme_itunes_personal/. "
            "A local theme.json manifest is required. Missing files fall back individually."
        )
        note.setWordWrap(True)
        layout.addRow("", note)

        return w

    def _build_sync_tab(self):
        w = QWidget()
        layout = QFormLayout(w)
        layout.setSpacing(8)

        self._auto_sync = QCheckBox("Automatically sync when device connects")
        self._auto_sync.setChecked(self._config.auto_sync_on_connect)
        layout.addRow("", self._auto_sync)

        self._rockbox_autoupdate = QCheckBox("Auto rebuild Rockbox database after sync")
        self._rockbox_autoupdate.setChecked(self._config.get("auto_rebuild_rockbox_database_after_sync", False))
        layout.addRow("", self._rockbox_autoupdate)

        self._rockbox_db_mode = QComboBox()
        self._rockbox_db_mode.addItems(["Passive", "Assisted", "Active"])
        mode_map = {"passive": 0, "assisted": 1, "active": 2}
        self._rockbox_db_mode.setCurrentIndex(mode_map.get(self._config.get("rockbox_database_update_mode", "active"), 2))
        layout.addRow("Rockbox DB Update:", self._rockbox_db_mode)

        self._resync_meta = QCheckBox("Resync metadata changes (re-copy when tags change)")
        self._resync_meta.setChecked(self._config.resync_metadata_changes)
        layout.addRow("", self._resync_meta)

        self._force_resync = QCheckBox("Force full resync (re-copy everything next sync)")
        self._force_resync.setChecked(self._config.force_full_resync)
        layout.addRow("", self._force_resync)

        self._copy_artwork = QCheckBox("Copy artwork to device")
        self._copy_artwork.setChecked(self._config.copy_artwork_to_device)
        layout.addRow("", self._copy_artwork)

        self._sync_playlists = QCheckBox("Sync playlists to device as Rockbox .m3u8 files")
        self._sync_playlists.setChecked(self._config.get("sync_playlists_to_device", True))
        layout.addRow("", self._sync_playlists)

        self._enable_online_art = QCheckBox("Enable online artwork lookup")
        self._enable_online_art.setChecked(self._config.enable_online_artwork_lookup)
        layout.addRow("", self._enable_online_art)

        self._prefer_local_art = QCheckBox("Prefer local artwork over online matches")
        self._prefer_local_art.setChecked(self._config.prefer_local_artwork)
        layout.addRow("", self._prefer_local_art)

        self._fetch_hires_art = QCheckBox("Fetch hi-res artwork for desktop library")
        self._fetch_hires_art.setChecked(self._config.fetch_hires_online_artwork)
        layout.addRow("", self._fetch_hires_art)

        self._background_art = QCheckBox("Fetch missing artwork in background")
        self._background_art.setChecked(self._config.get("background_artwork_lookup_enabled", True))
        layout.addRow("", self._background_art)

        self._export_device_cover = QCheckBox("Export standard cover.jpg artwork to iPod")
        self._export_device_cover.setChecked(self._config.export_device_cover_jpg)
        layout.addRow("", self._export_device_cover)

        self._dup_strictness = QComboBox()
        self._dup_strictness.addItems([
            "Metadata + File Hash",
            "Metadata Only",
            "File Hash Only",
        ])
        current = self._config.duplicate_strictness
        idx_map = {
            "metadata_and_hash": 0,
            "metadata_only": 1,
            "file_hash_only": 2,
        }
        self._dup_strictness.setCurrentIndex(idx_map.get(current, 0))
        layout.addRow("Duplicate Detection:", self._dup_strictness)

        # Device file layout
        self._dir_template = QLineEdit(self._config.device_music_template)
        layout.addRow("Device Folder Template:", self._dir_template)

        self._file_template = QLineEdit(self._config.device_file_template)
        layout.addRow("Device File Template:", self._file_template)

        return w

    def _build_device_tab(self):
        w = QWidget()
        layout = QFormLayout(w)
        layout.setSpacing(8)

        # Mount path override
        mount_row = QHBoxLayout()
        self._mount_edit = QLineEdit(self._config.device_mount_path)
        mount_row.addWidget(self._mount_edit)
        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_mount)
        mount_row.addWidget(browse_btn)
        layout.addRow("Device Mount Path:", mount_row)

        self._games_device_target_edit = QLineEdit(self._config.get("games_device_target_dir", "gameboy"))
        self._games_device_target_edit.setReadOnly(True)
        layout.addRow("Rockboy ROM Path on iPod:", self._games_device_target_edit)

        self._games_sim_target_edit = QLineEdit(self._config.get("games_simulator_target_dir", "gameboy"))
        self._games_sim_target_edit.setReadOnly(True)
        layout.addRow("Rockboy ROM Path on Simulator:", self._games_sim_target_edit)

        android_group = QGroupBox("Android Import")
        android_layout = QFormLayout()

        android_source_row = QHBoxLayout()
        self._android_source_edit = QLineEdit(self._config.get("android_source_path", ""))
        android_source_row.addWidget(self._android_source_edit)
        android_browse = QPushButton("Browse...")
        android_browse.clicked.connect(self._browse_android_source)
        android_source_row.addWidget(android_browse)
        android_layout.addRow("Android Storage Root:", android_source_row)

        self._android_device_dir_edit = QLineEdit(
            self._config.get("android_import_device_dir", "Videos/Android Phone")
        )
        android_layout.addRow("iPod Video Folder:", self._android_device_dir_edit)

        self._android_include_photos = QCheckBox("Import photos as slideshow videos")
        self._android_include_photos.setChecked(
            self._config.get("android_import_include_photos", True)
        )
        android_layout.addRow("", self._android_include_photos)

        self._android_include_videos = QCheckBox("Import phone videos")
        self._android_include_videos.setChecked(
            self._config.get("android_import_include_videos", True)
        )
        android_layout.addRow("", self._android_include_videos)

        self._android_tiktok_mode = QCheckBox("Format imported clips for the iPodTikTok plugin")
        self._android_tiktok_mode.setChecked(
            self._config.get("android_import_for_tiktok_plugin", False)
        )
        android_layout.addRow("", self._android_tiktok_mode)

        self._android_photo_duration_edit = QLineEdit(
            str(self._config.get("android_photo_duration_seconds", 8.0))
        )
        android_layout.addRow("Photo Clip Length (sec):", self._android_photo_duration_edit)

        android_group.setLayout(android_layout)
        layout.addRow(android_group)

        # Mock device
        group = QGroupBox("Development / Testing")
        group_layout = QFormLayout()
        self._mock_enabled = QCheckBox("Enable mock device mode")
        self._mock_enabled.setChecked(self._config.mock_device_enabled)
        group_layout.addRow("", self._mock_enabled)

        mock_row = QHBoxLayout()
        self._mock_path_edit = QLineEdit(self._config.mock_device_path)
        mock_row.addWidget(self._mock_path_edit)
        mock_browse = QPushButton("Browse...")
        mock_browse.clicked.connect(self._browse_mock)
        mock_row.addWidget(mock_browse)
        group_layout.addRow("Mock Device Path:", mock_row)

        group.setLayout(group_layout)
        layout.addRow(group)

        return w

    def _build_advanced_tab(self):
        w = QWidget()
        layout = QFormLayout(w)
        layout.setSpacing(8)

        self._db_path_label = QLabel(self._config.db_path)
        self._db_path_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
        layout.addRow("Database:", self._db_path_label)

        self._cache_label = QLabel(self._config.artwork_cache_dir)
        self._cache_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
        layout.addRow("Artwork Cache:", self._cache_label)

        return w

    def _browse_music_dir(self):
        d = QFileDialog.getExistingDirectory(self, "Select Music Folder",
                                             self._music_dir_edit.text())
        if d:
            self._music_dir_edit.setText(d)

    def _browse_video_dir(self):
        d = QFileDialog.getExistingDirectory(self, "Select Video Folder",
                                             self._first_video_dir())
        if d:
            paths = self._video_dir_lines()
            if d not in paths:
                paths.append(d)
            self._video_dirs_edit.setPlainText("\n".join(paths))

    def _video_dir_lines(self):
        return [line.strip() for line in self._video_dirs_edit.toPlainText().splitlines() if line.strip()]

    def _first_video_dir(self):
        paths = self._video_dir_lines()
        return paths[0] if paths else self._config.get("video_dir", "")

    def _browse_mount(self):
        d = QFileDialog.getExistingDirectory(self, "Select Device Mount Point",
                                             self._mount_edit.text())
        if d:
            self._mount_edit.setText(d)

    def _browse_mock(self):
        d = QFileDialog.getExistingDirectory(self, "Select Mock Device Folder",
                                             self._mock_path_edit.text())
        if d:
            self._mock_path_edit.setText(d)

    def _browse_android_source(self):
        d = QFileDialog.getExistingDirectory(self, "Select Android Storage Root",
                                             self._android_source_edit.text())
        if d:
            self._android_source_edit.setText(d)

    def _reset_asset_pack(self):
        self._theme_mode.setCurrentIndex(0)

    def _on_ok(self):
        changes = {}
        if self._music_dir_edit.text() != self._config.music_dir:
            changes["music_dir"] = self._music_dir_edit.text()
        video_dirs = self._video_dir_lines()
        if video_dirs != (self._config.get("video_dirs", []) or []):
            changes["video_dirs"] = video_dirs
        if self._scan_on_startup.isChecked() != self._config.scan_on_startup:
            changes["scan_on_startup"] = self._scan_on_startup.isChecked()
        browser_home = self._browser_home_edit.text().strip()
        if browser_home != self._config.get("browser_home_url", "https://www.rockbox.org/"):
            changes["browser_home_url"] = browser_home
        if self._auto_sync.isChecked() != self._config.auto_sync_on_connect:
            changes["auto_sync_on_connect"] = self._auto_sync.isChecked()
        if self._rockbox_autoupdate.isChecked() != self._config.get("auto_rebuild_rockbox_database_after_sync", False):
            changes["auto_rebuild_rockbox_database_after_sync"] = self._rockbox_autoupdate.isChecked()
        mode_value = {0: "passive", 1: "assisted", 2: "active"}.get(self._rockbox_db_mode.currentIndex(), "active")
        if mode_value != self._config.get("rockbox_database_update_mode", "active"):
            changes["rockbox_database_update_mode"] = mode_value
        if self._resync_meta.isChecked() != self._config.resync_metadata_changes:
            changes["resync_metadata_changes"] = self._resync_meta.isChecked()
        if self._force_resync.isChecked() != self._config.force_full_resync:
            changes["force_full_resync"] = self._force_resync.isChecked()
        if self._copy_artwork.isChecked() != self._config.copy_artwork_to_device:
            changes["copy_artwork_to_device"] = self._copy_artwork.isChecked()
        if self._sync_playlists.isChecked() != self._config.get("sync_playlists_to_device", True):
            changes["sync_playlists_to_device"] = self._sync_playlists.isChecked()
        if self._enable_online_art.isChecked() != self._config.enable_online_artwork_lookup:
            changes["enable_online_artwork_lookup"] = self._enable_online_art.isChecked()
        if self._prefer_local_art.isChecked() != self._config.prefer_local_artwork:
            changes["prefer_local_artwork"] = self._prefer_local_art.isChecked()
        if self._fetch_hires_art.isChecked() != self._config.fetch_hires_online_artwork:
            changes["fetch_hires_online_artwork"] = self._fetch_hires_art.isChecked()
        if self._background_art.isChecked() != self._config.get("background_artwork_lookup_enabled", True):
            changes["background_artwork_lookup_enabled"] = self._background_art.isChecked()
        if self._export_device_cover.isChecked() != self._config.export_device_cover_jpg:
            changes["export_device_cover_jpg"] = self._export_device_cover.isChecked()

        strictness_map = {0: "metadata_and_hash", 1: "metadata_only", 2: "file_hash_only"}
        new_strict = strictness_map.get(self._dup_strictness.currentIndex(), "metadata_and_hash")
        if new_strict != self._config.duplicate_strictness:
            changes["duplicate_strictness"] = new_strict

        if self._dir_template.text() != self._config.device_music_template:
            changes["device_music_template"] = self._dir_template.text()
        if self._file_template.text() != self._config.device_file_template:
            changes["device_file_template"] = self._file_template.text()

        if self._mount_edit.text() != self._config.device_mount_path:
            changes["device_mount_path"] = self._mount_edit.text()
        games_device_target = self._games_device_target_edit.text().strip()
        if games_device_target != self._config.get("games_device_target_dir", "gameboy"):
            changes["games_device_target_dir"] = games_device_target
        games_sim_target = self._games_sim_target_edit.text().strip()
        if games_sim_target != self._config.get("games_simulator_target_dir", "gameboy"):
            changes["games_simulator_target_dir"] = games_sim_target
        android_source = self._android_source_edit.text().strip()
        if android_source != self._config.get("android_source_path", ""):
            changes["android_source_path"] = android_source
        android_device_dir = self._android_device_dir_edit.text().strip()
        if android_device_dir != self._config.get("android_import_device_dir", "Videos/Android Phone"):
            changes["android_import_device_dir"] = android_device_dir
        if self._android_include_photos.isChecked() != self._config.get("android_import_include_photos", True):
            changes["android_import_include_photos"] = self._android_include_photos.isChecked()
        if self._android_include_videos.isChecked() != self._config.get("android_import_include_videos", True):
            changes["android_import_include_videos"] = self._android_include_videos.isChecked()
        if self._android_tiktok_mode.isChecked() != self._config.get("android_import_for_tiktok_plugin", False):
            changes["android_import_for_tiktok_plugin"] = self._android_tiktok_mode.isChecked()
        try:
            android_photo_duration = float(self._android_photo_duration_edit.text().strip() or "8")
        except ValueError:
            android_photo_duration = float(self._config.get("android_photo_duration_seconds", 8.0))
        android_photo_duration = max(1.0, android_photo_duration)
        if android_photo_duration != float(self._config.get("android_photo_duration_seconds", 8.0)):
            changes["android_photo_duration_seconds"] = android_photo_duration
        if self._mock_enabled.isChecked() != self._config.mock_device_enabled:
            changes["mock_device_enabled"] = self._mock_enabled.isChecked()
        if self._mock_path_edit.text() != self._config.mock_device_path:
            changes["mock_device_path"] = self._mock_path_edit.text()
        new_theme_mode = "personal" if self._theme_mode.currentIndex() == 1 else "default"
        if new_theme_mode != self._config.get("theme_mode", "default"):
            changes["theme_mode"] = new_theme_mode

        if changes:
            self.settings_saved.emit(changes)

        self.accept()
