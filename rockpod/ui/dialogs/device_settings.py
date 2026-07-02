"""Per-device settings dialog."""

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QDoubleSpinBox,
    QFormLayout,
    QFrame,
    QGroupBox,
    QLabel,
    QLineEdit,
    QPushButton,
    QScrollArea,
    QVBoxLayout,
    QWidget,
)


class DeviceSettingsDialog(QDialog):
    """Edit overrides for the currently connected device."""

    settings_saved = Signal(dict)

    @staticmethod
    def _section_label(text):
        label = QLabel(str(text or "").strip())
        label.setStyleSheet("font-weight: bold; margin-top: 6px;")
        return label

    @staticmethod
    def _helper_label(text):
        label = QLabel(str(text or "").strip())
        label.setWordWrap(True)
        label.setStyleSheet("color: palette(mid); margin-left: 22px;")
        return label

    def _add_checkbox_row(self, layout, checkbox, description=""):
        layout.addRow("", checkbox)
        if description:
            layout.addRow("", self._helper_label(description))

    def _update_audio_conversion_controls(self):
        enabled = self._convert_audio.isChecked()
        self._audio_conversion_mode.setEnabled(enabled)
        self._audio_conversion_codec.setEnabled(enabled)
        self._audio_conversion_bitrate.setEnabled(enabled)

    def __init__(self, config, device, default_name="iPod", parent=None):
        super().__init__(parent)
        self.setWindowTitle("Device Settings")
        self.setMinimumSize(640, 640)
        self.setModal(True)
        self._config = config
        self._device = device
        self._default_name = str(default_name or "iPod")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(8)

        info_group = QGroupBox("Detected Device")
        info_layout = QFormLayout(info_group)
        info_layout.addRow("Current Name:", QLabel(getattr(device, "name", "") or self._default_name))
        info_layout.addRow("Model:", QLabel(getattr(device, "detected_model", "") or "Unknown"))
        info_layout.addRow("Target:", QLabel(getattr(device, "rockbox_target", "") or "Unknown"))
        info_layout.addRow("Resolution:", QLabel(getattr(device, "screen_resolution", "") or "Unknown"))
        layout.addWidget(info_group)

        options_area = QScrollArea()
        options_area.setFrameShape(QFrame.NoFrame)
        options_area.setWidgetResizable(True)
        options_content = QWidget()
        options_layout = QVBoxLayout(options_content)
        options_layout.setContentsMargins(0, 0, 0, 0)
        options_layout.setSpacing(12)

        sync_group = QGroupBox("Sync Behavior")
        sync_layout = QFormLayout(sync_group)
        sync_layout.setSpacing(8)

        self._display_name = QLineEdit(self._config.get_device_display_name(device=device))
        self._display_name.setPlaceholderText(self._default_name)
        sync_layout.addRow("Display Name:", self._display_name)

        self._auto_sync = QCheckBox("Sync automatically when this iPod connects")
        self._auto_sync.setChecked(
            bool(self._config.get_effective("auto_sync_on_connect", device=device, default=False))
        )
        self._add_checkbox_row(
            sync_layout,
            self._auto_sync,
            "Starts a sync as soon as this device is detected.",
        )

        self._resync_meta = QCheckBox("Update tracks when tags or file content changes")
        self._resync_meta.setChecked(
            bool(self._config.get_effective("resync_metadata_changes", device=device, default=True))
        )
        self._add_checkbox_row(
            sync_layout,
            self._resync_meta,
            "Re-copy tracks when metadata, hashes, or renamed output paths no longer match the device copy.",
        )

        self._copy_artwork = QCheckBox("Copy album artwork to the iPod")
        self._copy_artwork.setChecked(
            bool(self._config.get_effective("copy_artwork_to_device", device=device, default=True))
        )
        self._add_checkbox_row(
            sync_layout,
            self._copy_artwork,
            "Exports device-ready cover art alongside synced music.",
        )

        self._sync_playlists = QCheckBox("Export playlists as Rockbox .m3u8 files")
        self._sync_playlists.setChecked(
            bool(self._config.get_effective("sync_playlists_to_device", device=device, default=True))
        )
        self._add_checkbox_row(
            sync_layout,
            self._sync_playlists,
            "Writes managed playlist files so Rockbox can browse the same playlists as the desktop library.",
        )

        self._convert_audio = QCheckBox("Convert audio during sync")
        self._convert_audio.setChecked(
            bool(self._config.get_effective("convert_audio_for_device", device=device, default=False))
        )
        self._add_checkbox_row(
            sync_layout,
            self._convert_audio,
            "Creates sync-only converted copies for this device. Your local library files stay unchanged.",
        )

        self._audio_conversion_mode = QComboBox()
        self._audio_conversion_mode.addItems(
            [
                "Unsupported or lossless only",
                "Above bitrate limit only",
                "Unsupported/lossless or above limit",
                "Always convert",
            ]
        )
        conversion_mode_map = {
            "unsupported_or_lossless": 0,
            "above_bitrate_limit": 1,
            "unsupported_lossless_or_above_limit": 2,
            "always": 3,
        }
        self._audio_conversion_mode.setCurrentIndex(
            conversion_mode_map.get(
                self._config.get_effective("audio_conversion_mode", device=device, default="unsupported_or_lossless"),
                0,
            )
        )
        sync_layout.addRow("Audio Conversion:", self._audio_conversion_mode)

        self._audio_conversion_codec = QComboBox()
        self._audio_conversion_codec.addItems(["MP3", "AAC (.m4a)"])
        codec_map = {"mp3": 0, "aac": 1, "m4a": 1}
        self._audio_conversion_codec.setCurrentIndex(
            codec_map.get(
                str(
                    self._config.get_effective("audio_conversion_codec", device=device, default="mp3")
                    or "mp3"
                ).strip().lower(),
                0,
            )
        )
        sync_layout.addRow("Converted Codec:", self._audio_conversion_codec)

        self._audio_conversion_bitrate = QComboBox()
        self._audio_conversion_bitrate.addItems(["128 kbps", "160 kbps", "192 kbps"])
        bitrate_map = {128: 0, 160: 1, 192: 2}
        self._audio_conversion_bitrate.setCurrentIndex(
            bitrate_map.get(
                int(
                    self._config.get_effective("audio_conversion_bitrate_kbps", device=device, default=160)
                    or 160
                ),
                1,
            )
        )
        sync_layout.addRow("Converted Bitrate:", self._audio_conversion_bitrate)

        self._video_sync_profile = QComboBox()
        self._video_sync_profile.addItems(["Compact", "Quality"])
        profile_map = {"compact": 0, "quality": 1}
        self._video_sync_profile.setCurrentIndex(
            profile_map.get(
                str(self._config.get_effective("video_sync_profile", device=device, default="compact")).strip().lower(),
                0,
            )
        )
        sync_layout.addRow("Video Sync Profile:", self._video_sync_profile)

        self._convert_audio.toggled.connect(self._update_audio_conversion_controls)
        self._update_audio_conversion_controls()
        options_layout.addWidget(sync_group)

        ui_group = QGroupBox("Rockbox UI")
        ui_layout = QFormLayout(ui_group)
        ui_layout.setSpacing(8)

        self._rockbox_ui_engine = QComboBox()
        self._rockbox_ui_engine.addItems(["Rockbox", "iPod"])
        self._rockbox_ui_engine.setCurrentIndex(
            1
            if str(self._config.get_effective("rockbox_ui_engine", device=device, default="rockbox")).strip().lower()
            == "ipodjs"
            else 0
        )
        ui_layout.addRow("UI Engine:", self._rockbox_ui_engine)

        self._rockbox_ui_accent = QComboBox()
        self._rockbox_ui_accent.addItems(
            ["Blue", "Graphite", "U2 Red", "Teal", "Green", "Gold",
             "Orange", "Purple", "Pink"]
        )
        accent_map = {
            "blue": 0,
            "graphite": 1,
            "u2": 2,
            "red": 2,
            "teal": 3,
            "green": 4,
            "gold": 5,
            "orange": 6,
            "purple": 7,
            "pink": 8,
        }
        self._rockbox_ui_accent.setCurrentIndex(
            accent_map.get(
                str(self._config.get_effective("rockbox_ui_accent", device=device, default="blue")).strip().lower(),
                0,
            )
        )
        ui_layout.addRow("Engine Accent:", self._rockbox_ui_accent)

        self._rockbox_ui_dark_mode = QCheckBox("Use dark iPodJS surfaces")
        self._rockbox_ui_dark_mode.setChecked(
            str(
                self._config.get_effective("rockbox_ui_dark_mode", device=device, default="off")
            ).strip().lower()
            in {"1", "true", "yes", "on"}
        )
        ui_layout.addRow("Engine Dark Mode:", self._rockbox_ui_dark_mode)

        self._rockbox_ui_density = QComboBox()
        self._rockbox_ui_density.addItems(["Comfortable", "Compact"])
        self._rockbox_ui_density.setCurrentIndex(
            1
            if str(self._config.get_effective("rockbox_ui_density", device=device, default="comfortable")).strip().lower()
            == "compact"
            else 0
        )
        ui_layout.addRow("Engine Density:", self._rockbox_ui_density)

        self._rockbox_ui_font_scale = QComboBox()
        self._rockbox_ui_font_scale.addItems(["Small", "Normal", "Large"])
        font_map = {"small": 0, "normal": 1, "large": 2}
        self._rockbox_ui_font_scale.setCurrentIndex(
            font_map.get(
                str(
                    self._config.get_effective("rockbox_ui_font_scale", device=device, default="normal")
                ).strip().lower(),
                1,
            )
        )
        ui_layout.addRow("Engine Font:", self._rockbox_ui_font_scale)

        self._rockbox_ui_surface = QComboBox()
        self._rockbox_ui_surface.addItems(["Solid", "Soft", "Transparent"])
        self._rockbox_ui_surface.setCurrentIndex(
            {"solid": 0, "soft": 1, "transparent": 2}.get(
                str(self._config.get_effective("rockbox_ui_surface", device=device, default="solid")).strip().lower(),
                0,
            )
        )
        ui_layout.addRow("Engine Surface:", self._rockbox_ui_surface)

        self._rockbox_ui_hold_effect = QComboBox()
        self._rockbox_ui_hold_effect.addItems(["Dim Overlay", "Lockscreen"])
        self._rockbox_ui_hold_effect.setCurrentIndex(
            1
            if str(self._config.get_effective("rockbox_ui_hold_effect", device=device, default="dim")).strip().lower()
            == "lockscreen"
            else 0
        )
        ui_layout.addRow("Hold Effect:", self._rockbox_ui_hold_effect)
        options_layout.addWidget(ui_group)

        app_group = QGroupBox("Applications")
        app_layout = QFormLayout(app_group)
        app_layout.setSpacing(8)

        self._show_applications = QCheckBox("Show Applications on the main menu")
        self._show_applications.setChecked(
            bool(
                self._config.get_effective(
                    "rockbox_show_applications_menu",
                    device=device,
                    default=False,
                )
            )
        )
        self._add_checkbox_row(
            app_layout,
            self._show_applications,
            "Applications always contains Maps and Weather.",
        )
        options_layout.addWidget(app_group)

        weather_group = QGroupBox("Weather")
        weather_layout = QFormLayout(weather_group)
        weather_layout.setSpacing(8)
        self._weather_enabled = QCheckBox("Sync weather for the Weather app")
        self._weather_enabled.setChecked(
            bool(self._config.get_effective("weather_enabled", device=device, default=True))
        )
        self._add_checkbox_row(
            weather_layout,
            self._weather_enabled,
            "RockPod refreshes a 7 day forecast and weather backgrounds during sync.",
        )
        self._weather_location = QLineEdit(
            str(self._config.get_effective("weather_location_name", device=device, default="Moncton, NB") or "")
        )
        weather_layout.addRow("Weather Location:", self._weather_location)
        self._weather_latitude = QDoubleSpinBox()
        self._weather_latitude.setRange(-90.0, 90.0)
        self._weather_latitude.setDecimals(4)
        self._weather_latitude.setValue(
            float(self._config.get_effective("weather_latitude", device=device, default=46.0878) or 46.0878)
        )
        weather_layout.addRow("Latitude:", self._weather_latitude)
        self._weather_longitude = QDoubleSpinBox()
        self._weather_longitude.setRange(-180.0, 180.0)
        self._weather_longitude.setDecimals(4)
        self._weather_longitude.setValue(
            float(self._config.get_effective("weather_longitude", device=device, default=-64.7782) or -64.7782)
        )
        weather_layout.addRow("Longitude:", self._weather_longitude)
        self._weather_units = QComboBox()
        self._weather_units.addItems(["Metric", "Imperial"])
        self._weather_units.setCurrentIndex(
            1
            if str(self._config.get_effective("weather_units", device=device, default="metric")).strip().lower() == "imperial"
            else 0
        )
        weather_layout.addRow("Weather Units:", self._weather_units)
        options_layout.addWidget(weather_group)

        maintenance_group = QGroupBox("Maintenance")
        maintenance_layout = QFormLayout(maintenance_group)
        maintenance_layout.setSpacing(8)
        self._auto_rebuild = QCheckBox("Rebuild the Rockbox database after sync")
        self._auto_rebuild.setChecked(
            bool(self._config.get_effective("auto_rebuild_rockbox_database_after_sync", device=device, default=False))
        )
        self._add_checkbox_row(
            maintenance_layout,
            self._auto_rebuild,
            "Refreshes Rockbox's tag database so new music appears on-device without a manual rebuild.",
        )

        self._verify_background = QCheckBox("Verify device contents in the background")
        self._verify_background.setChecked(
            bool(self._config.get_effective("verify_device_in_background", device=device, default=False))
        )
        self._add_checkbox_row(
            maintenance_layout,
            self._verify_background,
            "Checks the device index after connection without blocking the main window.",
        )
        self._rockbox_db_mode = QComboBox()
        self._rockbox_db_mode.addItems(["Passive", "Assisted", "Active"])
        mode_map = {"passive": 0, "assisted": 1, "active": 2}
        self._rockbox_db_mode.setCurrentIndex(
            mode_map.get(self._config.get_effective("rockbox_database_update_mode", device=device, default="active"), 2)
        )
        maintenance_layout.addRow("Rockbox DB Update:", self._rockbox_db_mode)

        self._duplicate_strictness = QComboBox()
        self._duplicate_strictness.addItems(["Metadata + File Hash", "Metadata Only", "File Hash Only"])
        strictness_map = {"metadata_and_hash": 0, "metadata_only": 1, "file_hash_only": 2}
        self._duplicate_strictness.setCurrentIndex(
            strictness_map.get(
                self._config.get_effective("duplicate_strictness", device=device, default="metadata_and_hash"),
                0,
            )
        )
        maintenance_layout.addRow("Duplicate Detection:", self._duplicate_strictness)

        self._duration_tolerance = QDoubleSpinBox()
        self._duration_tolerance.setRange(0.0, 30.0)
        self._duration_tolerance.setDecimals(1)
        self._duration_tolerance.setSingleStep(0.5)
        self._duration_tolerance.setValue(
            float(
                self._config.get_effective("duration_match_tolerance_seconds", device=device, default=2.0)
                or 2.0
            )
        )
        maintenance_layout.addRow("Duration Tolerance:", self._duration_tolerance)

        self._dir_template = QLineEdit(
            self._config.get_effective("device_music_template", device=device, default="Music/{album_artist}/{album}")
        )
        maintenance_layout.addRow("Device Folder Template:", self._dir_template)
        self._file_template = QLineEdit(
            self._config.get_effective("device_file_template", device=device, default="{track_number:02d} - {title}{ext}")
        )
        maintenance_layout.addRow("Device File Template:", self._file_template)
        note = QLabel("Values that match global preferences will not be stored as device-specific overrides.")
        note.setWordWrap(True)
        maintenance_layout.addRow("", note)
        options_layout.addWidget(maintenance_group)
        options_layout.addStretch(1)

        options_area.setWidget(options_content)
        layout.addWidget(options_area, 1)

        footer = QWidget()
        footer_layout = QVBoxLayout(footer)
        footer_layout.setContentsMargins(0, 0, 0, 0)
        footer_layout.setSpacing(8)

        self._reset_btn = QPushButton("Reset To Global Defaults")
        self._reset_btn.clicked.connect(self._reset_to_global)
        footer_layout.addWidget(self._reset_btn)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self._on_ok)
        buttons.rejected.connect(self.reject)
        footer_layout.addWidget(buttons)
        layout.addWidget(footer)

    def _reset_to_global(self):
        self._display_name.clear()
        self._auto_sync.setChecked(bool(self._config.get("auto_sync_on_connect", False)))
        self._resync_meta.setChecked(bool(self._config.get("resync_metadata_changes", True)))
        self._copy_artwork.setChecked(bool(self._config.get("copy_artwork_to_device", True)))
        self._sync_playlists.setChecked(bool(self._config.get("sync_playlists_to_device", True)))
        self._convert_audio.setChecked(bool(self._config.get("convert_audio_for_device", False)))
        self._audio_conversion_mode.setCurrentIndex(
            {
                "unsupported_or_lossless": 0,
                "above_bitrate_limit": 1,
                "unsupported_lossless_or_above_limit": 2,
                "always": 3,
            }.get(self._config.get("audio_conversion_mode", "unsupported_or_lossless"), 0)
        )
        self._audio_conversion_codec.setCurrentIndex(
            {"mp3": 0, "aac": 1, "m4a": 1}.get(
                str(self._config.get("audio_conversion_codec", "mp3") or "mp3").strip().lower(),
                0,
            )
        )
        self._audio_conversion_bitrate.setCurrentIndex(
            {128: 0, 160: 1, 192: 2}.get(
                int(self._config.get("audio_conversion_bitrate_kbps", 160) or 160),
                1,
            )
        )
        self._video_sync_profile.setCurrentIndex(
            {"compact": 0, "quality": 1}.get(
                str(self._config.get("video_sync_profile", "compact")).strip().lower(),
                0,
            )
        )
        self._update_audio_conversion_controls()
        self._rockbox_ui_engine.setCurrentIndex(
            1
            if str(self._config.get("rockbox_ui_engine", "rockbox")).strip().lower() == "ipodjs"
            else 0
        )
        self._rockbox_ui_accent.setCurrentIndex(
            {
                "blue": 0,
                "graphite": 1,
                "u2": 2,
                "red": 2,
                "teal": 3,
                "green": 4,
                "gold": 5,
                "orange": 6,
                "purple": 7,
                "pink": 8,
            }.get(
                str(self._config.get("rockbox_ui_accent", "blue")).strip().lower(),
                0,
            )
        )
        self._rockbox_ui_dark_mode.setChecked(
            str(self._config.get("rockbox_ui_dark_mode", "off")).strip().lower()
            in {"1", "true", "yes", "on"}
        )
        self._rockbox_ui_density.setCurrentIndex(
            1
            if str(self._config.get("rockbox_ui_density", "comfortable")).strip().lower() == "compact"
            else 0
        )
        self._rockbox_ui_font_scale.setCurrentIndex(
            {"small": 0, "normal": 1, "large": 2}.get(
                str(self._config.get("rockbox_ui_font_scale", "normal")).strip().lower(),
                1,
            )
        )
        self._rockbox_ui_surface.setCurrentIndex(
            {"solid": 0, "soft": 1, "transparent": 2}.get(
                str(self._config.get("rockbox_ui_surface", "solid")).strip().lower(),
                0,
            )
        )
        self._rockbox_ui_hold_effect.setCurrentIndex(
            1
            if str(self._config.get("rockbox_ui_hold_effect", "dim")).strip().lower() == "lockscreen"
            else 0
        )
        self._show_applications.setChecked(bool(self._config.get("rockbox_show_applications_menu", False)))
        self._weather_enabled.setChecked(bool(self._config.get("weather_enabled", True)))
        self._weather_location.setText(str(self._config.get("weather_location_name", "Moncton, NB") or ""))
        self._weather_latitude.setValue(float(self._config.get("weather_latitude", 46.0878) or 46.0878))
        self._weather_longitude.setValue(float(self._config.get("weather_longitude", -64.7782) or -64.7782))
        self._weather_units.setCurrentIndex(
            1
            if str(self._config.get("weather_units", "metric")).strip().lower() == "imperial"
            else 0
        )
        self._auto_rebuild.setChecked(bool(self._config.get("auto_rebuild_rockbox_database_after_sync", False)))
        self._verify_background.setChecked(bool(self._config.get("verify_device_in_background", False)))
        self._rockbox_db_mode.setCurrentIndex(
            {"passive": 0, "assisted": 1, "active": 2}.get(self._config.get("rockbox_database_update_mode", "active"), 2)
        )
        self._duplicate_strictness.setCurrentIndex(
            {"metadata_and_hash": 0, "metadata_only": 1, "file_hash_only": 2}.get(self._config.get("duplicate_strictness", "metadata_and_hash"), 0)
        )
        self._duration_tolerance.setValue(float(self._config.get("duration_match_tolerance_seconds", 2.0) or 2.0))
        self._dir_template.setText(self._config.get("device_music_template", "Music/{album_artist}/{album}"))
        self._file_template.setText(self._config.get("device_file_template", "{track_number:02d} - {title}{ext}"))

    def _on_ok(self):
        settings = {
            "display_name": self._display_name.text().strip(),
            "auto_sync_on_connect": self._auto_sync.isChecked(),
            "resync_metadata_changes": self._resync_meta.isChecked(),
            "copy_artwork_to_device": self._copy_artwork.isChecked(),
            "sync_playlists_to_device": self._sync_playlists.isChecked(),
            "convert_audio_for_device": self._convert_audio.isChecked(),
            "audio_conversion_mode": {
                0: "unsupported_or_lossless",
                1: "above_bitrate_limit",
                2: "unsupported_lossless_or_above_limit",
                3: "always",
            }.get(self._audio_conversion_mode.currentIndex(), "unsupported_or_lossless"),
            "audio_conversion_codec": {0: "mp3", 1: "aac"}.get(self._audio_conversion_codec.currentIndex(), "mp3"),
            "audio_conversion_bitrate_kbps": {0: 128, 1: 160, 2: 192}.get(
                self._audio_conversion_bitrate.currentIndex(),
                160,
            ),
            "video_sync_profile": {0: "compact", 1: "quality"}.get(
                self._video_sync_profile.currentIndex(),
                "compact",
            ),
            "rockbox_ui_engine": {0: "rockbox", 1: "ipodjs"}.get(
                self._rockbox_ui_engine.currentIndex(),
                "rockbox",
            ),
            "rockbox_ui_accent": {
                0: "blue",
                1: "graphite",
                2: "u2",
                3: "teal",
                4: "green",
                5: "gold",
                6: "orange",
                7: "purple",
                8: "pink",
            }.get(
                self._rockbox_ui_accent.currentIndex(),
                "blue",
            ),
            "rockbox_ui_dark_mode": self._rockbox_ui_dark_mode.isChecked(),
            "rockbox_ui_density": {0: "comfortable", 1: "compact"}.get(
                self._rockbox_ui_density.currentIndex(),
                "comfortable",
            ),
            "rockbox_ui_font_scale": {0: "small", 1: "normal", 2: "large"}.get(
                self._rockbox_ui_font_scale.currentIndex(),
                "normal",
            ),
            "rockbox_ui_surface": {0: "solid", 1: "soft", 2: "transparent"}.get(
                self._rockbox_ui_surface.currentIndex(),
                "solid",
            ),
            "rockbox_ui_hold_effect": {0: "dim", 1: "lockscreen"}.get(
                self._rockbox_ui_hold_effect.currentIndex(),
                "dim",
            ),
            "rockbox_show_applications_menu": self._show_applications.isChecked(),
            "weather_enabled": self._weather_enabled.isChecked(),
            "weather_location_name": self._weather_location.text().strip() or "Moncton, NB",
            "weather_latitude": float(self._weather_latitude.value()),
            "weather_longitude": float(self._weather_longitude.value()),
            "weather_units": {0: "metric", 1: "imperial"}.get(
                self._weather_units.currentIndex(),
                "metric",
            ),
            "auto_rebuild_rockbox_database_after_sync": self._auto_rebuild.isChecked(),
            "verify_device_in_background": self._verify_background.isChecked(),
            "rockbox_database_update_mode": {0: "passive", 1: "assisted", 2: "active"}.get(
                self._rockbox_db_mode.currentIndex(),
                "active",
            ),
            "duplicate_strictness": {0: "metadata_and_hash", 1: "metadata_only", 2: "file_hash_only"}.get(
                self._duplicate_strictness.currentIndex(),
                "metadata_and_hash",
            ),
            "duration_match_tolerance_seconds": float(self._duration_tolerance.value()),
            "device_music_template": self._dir_template.text().strip() or "Music/{album_artist}/{album}",
            "device_file_template": self._file_template.text().strip() or "{track_number:02d} - {title}{ext}",
        }
        self.settings_saved.emit(settings)
        self.accept()
