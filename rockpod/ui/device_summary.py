"""Classic iTunes-style iPod Summary screen."""

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPixmap
from PySide6.QtWidgets import (
    QCheckBox,
    QFrame,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from ui.storage_bar import StorageBar, format_bytes


def compute_device_storage(device, device_tracks, device_row=None, storage_breakdown=None):
    if storage_breakdown:
        return dict(storage_breakdown)
    device_row = dict(device_row) if device_row is not None and hasattr(device_row, "keys") else (device_row or {})
    total = int(getattr(device, "total_space", 0) or device_row.get("capacity_bytes") or 0) if device else int(device_row.get("capacity_bytes") or 0)
    free_reported = int(getattr(device, "free_space", 0) or device_row.get("free_bytes_last_seen") or 0) if device else int(device_row.get("free_bytes_last_seen") or 0)
    used_reported = int(getattr(device, "used_space", 0) or 0) if device else 0
    music = sum(int((track.get("file_size") if hasattr(track, "get") else 0) or 0)
                for track in (device_tracks or []))
    if total > 0:
        free = free_reported if free_reported >= 0 else max(total - used_reported, 0)
        used = max(total - free, used_reported, music)
    else:
        free = 0
        used = used_reported or music
    other = max(used - music, 0)
    return {
        "total": total,
        "used": used,
        "free": free,
        "music": music,
        "other": other,
    }


def build_summary_data(device, device_row=None, device_tracks=None, runtime_summary=None,
                       rockbox_state=None, device_state="", device_playlist_count=0,
                       runtime_insights=None, storage_breakdown=None):
    device_row = dict(device_row) if device_row is not None and hasattr(device_row, "keys") else (device_row or {})
    runtime_summary = dict(runtime_summary) if runtime_summary is not None and hasattr(runtime_summary, "keys") else (runtime_summary or {})
    runtime_insights = dict(runtime_insights) if runtime_insights is not None and hasattr(runtime_insights, "keys") else (runtime_insights or {})
    rockbox_state = rockbox_state or {}
    storage = compute_device_storage(device, device_tracks or [], device_row, storage_breakdown)
    name = getattr(device, "name", "") if device else ""
    rockbox_detected = getattr(device, "is_rockbox", False) if device else bool(device_row.get("rockbox_detected"))
    detected_model = getattr(device, "detected_model", "") if device else ""
    rockbox_target = getattr(device, "rockbox_target", "") if device else ""
    device_type = getattr(device, "device_type", "") if device else ""
    screen_resolution = getattr(device, "screen_resolution", "") if device else ""
    model = detected_model or ("Rockbox iPod" if rockbox_detected else "iPod")
    return {
        "name": name or device_row.get("display_name") or "No iPod Connected",
        "model": model,
        "device_type": str(device_type or ("rockbox" if rockbox_detected else "")).replace("_", " ").title() or "Unknown",
        "rockbox_target": rockbox_target or "Unknown",
        "screen_resolution": screen_resolution or "Unknown",
        "connected": bool(device),
        "mount_path": getattr(device, "mount_path", "") if device else device_row.get("mount_path_last_seen", ""),
        "rockbox": "Yes" if rockbox_detected else "No",
        "capacity": format_bytes(storage["total"]) if storage["total"] else "Unknown",
        "used": format_bytes(storage["used"]) if storage["used"] else "0 MB",
        "free": format_bytes(storage["free"]) if storage["total"] else "Unknown",
        "music_usage": format_bytes(storage["music"]) if storage["music"] else "0 MB",
        "games_plugins_usage": format_bytes(storage.get("games_plugins", 0)) if storage.get("games_plugins", 0) else "0 MB",
        "themes_assets_usage": format_bytes(storage.get("themes_assets", 0)) if storage.get("themes_assets", 0) else "0 MB",
        "rockbox_system_usage": format_bytes(storage.get("rockbox_system", 0)) if storage.get("rockbox_system", 0) else "0 MB",
        "other_usage": format_bytes(storage["other"]) if storage["other"] else "0 MB",
        "storage_scanned_at": storage.get("scanned_at") or "Not yet",
        "track_count": len(device_tracks or []),
        "device_state": device_state or ("Connected" if device else "Disconnected"),
        "last_sync": device_row.get("last_sync_at") or "Never",
        "last_scan": device_row.get("last_scan_at") or "Never",
        "last_play_sync": runtime_summary.get("last_imported_at") or "Never",
        "playlist_count": int(device_playlist_count or 0),
        "played_on_ipod": int(runtime_insights.get("ipod_play_count_total") or 0),
        "played_on_desktop": int(runtime_insights.get("desktop_play_count_total") or 0),
        "played_combined": int(runtime_insights.get("combined_play_count_total") or 0),
        "play_count_total": int(runtime_summary.get("total_play_count") or 0),
        "rated_tracks": int(runtime_summary.get("rated_tracks") or 0),
        "most_played_artist": runtime_insights.get("most_played_artist") or "Unknown",
        "most_played_album": runtime_insights.get("most_played_album") or "Unknown",
        "rockbox_database": rockbox_state.get("database_status", "Unavailable"),
        "runtime_data": "Available" if rockbox_state.get("runtime_data_available") else "Not available",
        "storage": storage,
    }


class DeviceSummaryWidget(QWidget):
    """Device root view modeled on the iTunes 7 iPod summary pane."""

    sync_clicked = Signal()
    refresh_clicked = Signal()
    eject_clicked = Signal()
    force_rescan_clicked = Signal()
    open_music_clicked = Signal()
    settings_clicked = Signal()
    auto_sync_changed = Signal(bool)
    resync_metadata_changed = Signal(bool)
    rockbox_autoupdate_changed = Signal(bool)
    verify_background_changed = Signal(bool)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("device_summary")
        self._device_name = "iPod"

        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 10, 14, 10)
        layout.setSpacing(7)

        header = QFrame()
        header.setObjectName("device_summary_header")
        self._header = header
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(10, 6, 10, 6)
        header_layout.setSpacing(8)
        self._icon = QLabel()
        self._icon.setFixedSize(72, 72)
        self._icon.setPixmap(_ipod_icon())
        header_layout.addWidget(self._icon)

        title_col = QVBoxLayout()
        title_col.setSpacing(0)
        self._name = QLabel("No iPod Connected")
        self._name.setObjectName("device_summary_name")
        self._model = QLabel("iPod")
        self._model.setObjectName("device_summary_model")
        self._connection = QLabel("Disconnected")
        self._connection.setObjectName("device_summary_detail")
        self._mount = QLabel("")
        self._mount.setObjectName("device_summary_detail")
        title_col.addWidget(self._name)
        title_col.addWidget(self._model)
        title_col.addWidget(self._connection)
        title_col.addWidget(self._mount)
        title_col.addStretch()
        header_layout.addLayout(title_col, 1)

        action_col = QVBoxLayout()
        action_col.setSpacing(2)
        self._sync_btn = QPushButton("Sync")
        self._refresh_btn = QPushButton("Refresh")
        self._settings_btn = QPushButton("Settings")
        self._eject_btn = QPushButton("Eject")
        self._force_btn = QPushButton("Force Rescan")
        for btn in (self._sync_btn, self._refresh_btn, self._settings_btn, self._eject_btn, self._force_btn):
            btn.setObjectName("device_summary_button")
            btn.setMinimumWidth(92)
            btn.setMaximumWidth(92)
            action_col.addWidget(btn)
        action_col.addStretch(1)
        header_layout.addLayout(action_col)
        layout.addWidget(header)

        middle = QHBoxLayout()
        middle.setSpacing(8)
        self._info_group = QGroupBox("iPod")
        self._info_group.setObjectName("device_summary_group")
        info_layout = QGridLayout(self._info_group)
        info_layout.setContentsMargins(8, 6, 8, 8)
        info_layout.setHorizontalSpacing(8)
        info_layout.setVerticalSpacing(1)
        self._fields = {}
        rows = [
            ("Name", "name"),
            ("Model", "model"),
            ("Type", "device_type"),
            ("Target", "rockbox_target"),
            ("Resolution", "screen_resolution"),
            ("Capacity", "capacity"),
            ("Used", "used"),
            ("Free", "free"),
            ("Music", "music_usage"),
            ("Games/Plugins", "games_plugins_usage"),
            ("Themes/Assets", "themes_assets_usage"),
            ("Rockbox System", "rockbox_system_usage"),
            ("Other", "other_usage"),
            ("Storage Scan", "storage_scanned_at"),
            ("Rockbox", "rockbox"),
            ("Tracks", "track_count"),
            ("Playlists", "playlist_count"),
            ("Status", "device_state"),
            ("Last Sync", "last_sync"),
            ("Last Verified", "last_scan"),
            ("Last Play Sync", "last_play_sync"),
            ("Played on iPod", "played_on_ipod"),
            ("Played on Desktop", "played_on_desktop"),
            ("Played Combined", "played_combined"),
            ("Play Count Total", "play_count_total"),
            ("Rated Tracks", "rated_tracks"),
            ("Most Played Artist", "most_played_artist"),
            ("Most Played Album", "most_played_album"),
            ("Rockbox DB", "rockbox_database"),
            ("Runtime Data", "runtime_data"),
        ]
        for row, (label, key) in enumerate(rows):
            field_label = QLabel(label + ":")
            field_label.setObjectName("device_summary_label")
            info_layout.addWidget(field_label, row, 0, Qt.AlignRight)
            value = QLabel("")
            value.setObjectName("device_summary_value")
            self._fields[key] = value
            info_layout.addWidget(value, row, 1)
        middle.addWidget(self._info_group, 1)

        self._options_group = QGroupBox("Options")
        self._options_group.setObjectName("device_summary_group")
        opt_layout = QVBoxLayout(self._options_group)
        opt_layout.setContentsMargins(8, 6, 8, 8)
        opt_layout.setSpacing(2)
        self._auto_sync = QCheckBox("Automatically sync when this iPod is connected")
        self._manual = QCheckBox("Manually manage music")
        self._resync_meta = QCheckBox("Resync metadata changes")
        self._rockbox_autoupdate = QCheckBox("Auto rebuild Rockbox database after sync")
        self._verify_bg = QCheckBox("Verify device contents in background")
        self._verify_bg.setChecked(False)
        self._open_music_btn = QPushButton("Open Music View")
        self._open_music_btn.setObjectName("device_summary_button")
        self._open_music_btn.setMinimumWidth(126)
        self._open_music_btn.setMaximumWidth(126)
        for widget in (self._auto_sync, self._manual, self._resync_meta, self._rockbox_autoupdate, self._verify_bg):
            opt_layout.addWidget(widget)
        opt_layout.addSpacing(4)
        opt_layout.addWidget(self._open_music_btn, 0, Qt.AlignLeft)
        opt_layout.addStretch()
        middle.addWidget(self._options_group, 1)
        layout.addLayout(middle)

        storage_group = QGroupBox("Capacity")
        storage_group.setObjectName("device_summary_group")
        storage_layout = QVBoxLayout(storage_group)
        storage_layout.setContentsMargins(8, 6, 8, 7)
        self._storage = StorageBar()
        self._storage.setVisible(True)
        storage_layout.addWidget(self._storage)
        layout.addWidget(storage_group)
        layout.addStretch(1)

        self._sync_btn.clicked.connect(self.sync_clicked)
        self._refresh_btn.clicked.connect(self.refresh_clicked)
        self._settings_btn.clicked.connect(self.settings_clicked)
        self._eject_btn.clicked.connect(self.eject_clicked)
        self._force_btn.clicked.connect(self.force_rescan_clicked)
        self._open_music_btn.clicked.connect(self.open_music_clicked)
        self._auto_sync.toggled.connect(self.auto_sync_changed)
        self._resync_meta.toggled.connect(self.resync_metadata_changed)
        self._verify_bg.toggled.connect(self.verify_background_changed)

    def apply_theme_assets(self, theme_assets):
        icon_path = theme_assets.asset_path("device_summary_sidebar_icon")
        if icon_path:
            px = QPixmap(icon_path)
            if not px.isNull():
                self._icon.setPixmap(px.scaled(72, 72, Qt.KeepAspectRatio, Qt.SmoothTransformation))
                return
        self._icon.setPixmap(_ipod_icon())
        self._rockbox_autoupdate.toggled.connect(self.rockbox_autoupdate_changed)

    def set_options(self, auto_sync=False, resync_metadata=True, manual=False,
                    rockbox_autoupdate=False, verify_background=False):
        self._auto_sync.blockSignals(True)
        self._resync_meta.blockSignals(True)
        self._manual.blockSignals(True)
        self._rockbox_autoupdate.blockSignals(True)
        self._verify_bg.blockSignals(True)
        self._auto_sync.setChecked(bool(auto_sync))
        self._resync_meta.setChecked(bool(resync_metadata))
        self._manual.setChecked(bool(manual))
        self._rockbox_autoupdate.setChecked(bool(rockbox_autoupdate))
        self._verify_bg.setChecked(bool(verify_background))
        self._auto_sync.blockSignals(False)
        self._resync_meta.blockSignals(False)
        self._manual.blockSignals(False)
        self._rockbox_autoupdate.blockSignals(False)
        self._verify_bg.blockSignals(False)

    def update_summary(self, data):
        self._device_name = data.get("name") or "iPod"
        connected = bool(data.get("connected"))
        self._name.setText(self._device_name)
        self._model.setText(data.get("model") or "iPod")
        self._connection.setText("Connected" if connected else "Disconnected")
        mount = data.get("mount_path") or "Mount path unavailable"
        self._mount.setText(mount)
        for key, label in self._fields.items():
            label.setText(str(data.get(key, "")))

        storage = data.get("storage") or {}
        self._storage.update_storage(
            storage,
            self._device_name,
        )
        if not storage.get("total"):
            self._storage.hide_bar()

        for btn in (self._sync_btn, self._refresh_btn, self._settings_btn, self._eject_btn, self._force_btn, self._open_music_btn):
            btn.setEnabled(connected)


def _ipod_icon():
    px = QPixmap(72, 72)
    px.fill(Qt.transparent)
    p = QPainter(px)
    p.setRenderHint(QPainter.Antialiasing)
    p.setPen(QColor("#777777"))
    p.setBrush(QColor("#f8f8f8"))
    p.drawRoundedRect(18, 3, 36, 66, 5, 5)
    p.setBrush(QColor("#dfe7ef"))
    p.drawRect(22, 9, 28, 22)
    p.setBrush(QColor("#e5e5e5"))
    p.drawEllipse(24, 38, 24, 24)
    p.setBrush(QColor("#f8f8f8"))
    p.drawEllipse(31, 45, 10, 10)
    p.end()
    return px
