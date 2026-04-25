"""Per-device settings dialog."""

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QDoubleSpinBox,
    QFormLayout,
    QGroupBox,
    QLabel,
    QLineEdit,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class DeviceSettingsDialog(QDialog):
    """Edit overrides for the currently connected device."""

    settings_saved = Signal(dict)

    def __init__(self, config, device, default_name="iPod", parent=None):
        super().__init__(parent)
        self.setWindowTitle("Device Settings")
        self.setMinimumSize(520, 420)
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

        options_group = QGroupBox("Overrides For This Device")
        options_layout = QFormLayout(options_group)
        options_layout.setSpacing(8)

        self._display_name = QLineEdit(self._config.get_device_display_name(device=device))
        self._display_name.setPlaceholderText(self._default_name)
        options_layout.addRow("Display Name:", self._display_name)

        self._auto_sync = QCheckBox("Automatically sync when this iPod is connected")
        self._auto_sync.setChecked(bool(self._config.get_effective("auto_sync_on_connect", device=device, default=False)))
        options_layout.addRow("", self._auto_sync)

        self._resync_meta = QCheckBox("Resync metadata changes")
        self._resync_meta.setChecked(bool(self._config.get_effective("resync_metadata_changes", device=device, default=True)))
        options_layout.addRow("", self._resync_meta)

        self._copy_artwork = QCheckBox("Copy artwork to device")
        self._copy_artwork.setChecked(bool(self._config.get_effective("copy_artwork_to_device", device=device, default=True)))
        options_layout.addRow("", self._copy_artwork)

        self._sync_playlists = QCheckBox("Sync playlists to device as Rockbox .m3u8 files")
        self._sync_playlists.setChecked(bool(self._config.get_effective("sync_playlists_to_device", device=device, default=True)))
        options_layout.addRow("", self._sync_playlists)

        self._convert_audio = QCheckBox("Convert audio for this iPod during sync")
        self._convert_audio.setChecked(bool(self._config.get_effective("convert_audio_for_device", device=device, default=False)))
        options_layout.addRow("", self._convert_audio)

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
        options_layout.addRow("Audio Conversion:", self._audio_conversion_mode)

        self._audio_conversion_codec = QComboBox()
        self._audio_conversion_codec.addItems(["MP3", "AAC (.m4a)"])
        codec_map = {"mp3": 0, "aac": 1, "m4a": 1}
        self._audio_conversion_codec.setCurrentIndex(
            codec_map.get(
                str(self._config.get_effective("audio_conversion_codec", device=device, default="mp3") or "mp3").strip().lower(),
                0,
            )
        )
        options_layout.addRow("Converted Codec:", self._audio_conversion_codec)

        self._audio_conversion_bitrate = QComboBox()
        self._audio_conversion_bitrate.addItems(["128 kbps", "160 kbps", "192 kbps"])
        bitrate_map = {128: 0, 160: 1, 192: 2}
        self._audio_conversion_bitrate.setCurrentIndex(
            bitrate_map.get(
                int(self._config.get_effective("audio_conversion_bitrate_kbps", device=device, default=160) or 160),
                1,
            )
        )
        options_layout.addRow("Converted Bitrate:", self._audio_conversion_bitrate)

        self._auto_rebuild = QCheckBox("Auto rebuild Rockbox database after sync")
        self._auto_rebuild.setChecked(bool(self._config.get_effective("auto_rebuild_rockbox_database_after_sync", device=device, default=False)))
        options_layout.addRow("", self._auto_rebuild)

        self._verify_background = QCheckBox("Verify device contents in background")
        self._verify_background.setChecked(bool(self._config.get_effective("verify_device_in_background", device=device, default=False)))
        options_layout.addRow("", self._verify_background)

        self._rockbox_db_mode = QComboBox()
        self._rockbox_db_mode.addItems(["Passive", "Assisted", "Active"])
        mode_map = {"passive": 0, "assisted": 1, "active": 2}
        self._rockbox_db_mode.setCurrentIndex(
            mode_map.get(self._config.get_effective("rockbox_database_update_mode", device=device, default="active"), 2)
        )
        options_layout.addRow("Rockbox DB Update:", self._rockbox_db_mode)

        self._duplicate_strictness = QComboBox()
        self._duplicate_strictness.addItems(["Metadata + File Hash", "Metadata Only", "File Hash Only"])
        strictness_map = {"metadata_and_hash": 0, "metadata_only": 1, "file_hash_only": 2}
        self._duplicate_strictness.setCurrentIndex(
            strictness_map.get(self._config.get_effective("duplicate_strictness", device=device, default="metadata_and_hash"), 0)
        )
        options_layout.addRow("Duplicate Detection:", self._duplicate_strictness)

        self._duration_tolerance = QDoubleSpinBox()
        self._duration_tolerance.setRange(0.0, 30.0)
        self._duration_tolerance.setDecimals(1)
        self._duration_tolerance.setSingleStep(0.5)
        self._duration_tolerance.setValue(
            float(self._config.get_effective("duration_match_tolerance_seconds", device=device, default=2.0) or 2.0)
        )
        options_layout.addRow("Duration Tolerance:", self._duration_tolerance)

        self._dir_template = QLineEdit(self._config.get_effective("device_music_template", device=device, default="Music/{album_artist}/{album}"))
        options_layout.addRow("Device Folder Template:", self._dir_template)

        self._file_template = QLineEdit(self._config.get_effective("device_file_template", device=device, default="{track_number:02d} - {title}{ext}"))
        options_layout.addRow("Device File Template:", self._file_template)

        note = QLabel("Values that match global preferences will not be stored as device-specific overrides.")
        note.setWordWrap(True)
        options_layout.addRow("", note)

        layout.addWidget(options_group)

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
