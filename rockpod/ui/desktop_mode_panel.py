"""Rockpod Device > Desktop Mode control panel."""

from __future__ import annotations

import platform

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class DesktopModePanel(QWidget):
    choose_sources_requested = Signal()
    install_requested = Signal()
    display_device_requested = Signal()
    display_local_requested = Signal()
    stop_display_requested = Signal()
    refresh_requested = Signal()
    capture_help_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("desktop_mode_panel")
        self._status = {}

        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 12, 14, 12)
        layout.setSpacing(10)

        title = QLabel("Desktop Mode")
        title.setObjectName("desktop_mode_title")
        title.setStyleSheet("font-size: 20px; font-weight: 600;")
        subtitle = QLabel(
            "A mini Mac OS X Snow Leopard desktop for the iPod. "
            "The click wheel acts as an absolute trackpad."
        )
        subtitle.setWordWrap(True)
        layout.addWidget(title)
        layout.addWidget(subtitle)

        card = QFrame()
        card.setObjectName("theme_hub_header")
        grid = QGridLayout(card)
        grid.setContentsMargins(12, 10, 12, 10)
        grid.setHorizontalSpacing(12)
        grid.setVerticalSpacing(8)

        self._local_state = QLabel("Checking…")
        self._device_state = QLabel("Checking…")
        self._runtime_state = QLabel("Checking…")
        self._preview_state = QLabel("Checking…")
        self._details = QLabel("")
        self._details.setWordWrap(True)
        self._details.setTextInteractionFlags(self._details.textInteractionFlags())

        grid.addWidget(QLabel("Private Snow Leopard pack:"), 0, 0)
        grid.addWidget(self._local_state, 0, 1)
        grid.addWidget(QLabel("Connected iPod pack:"), 1, 0)
        grid.addWidget(self._device_state, 1, 1)
        grid.addWidget(QLabel("Desktop Mode runtime:"), 2, 0)
        grid.addWidget(self._runtime_state, 2, 1)
        grid.addWidget(QLabel("Host parity display:"), 3, 0)
        grid.addWidget(self._preview_state, 3, 1)
        grid.addWidget(self._details, 4, 0, 1, 2)
        layout.addWidget(card)

        personal = QLabel(
            "Apple assets are imported only from your owned Mac OS X 10.6 "
            "installation or a capture made on it. They stay in ignored private "
            "storage and are never bundled with Rockpod."
        )
        personal.setWordWrap(True)
        layout.addWidget(personal)

        row = QHBoxLayout()
        self._source_btn = QPushButton("Import Owned 10.6 Assets…")
        self._help_btn = QPushButton("Capture Instructions")
        self._refresh_btn = QPushButton("Refresh Status")
        row.addWidget(self._source_btn)
        row.addWidget(self._help_btn)
        row.addWidget(self._refresh_btn)
        row.addStretch(1)
        layout.addLayout(row)

        actions = QHBoxLayout()
        self._install_btn = QPushButton("Install Verified Pack to iPod")
        host_label = "This Mac" if platform.system() == "Darwin" else "This Computer"
        self._display_device_btn = QPushButton(f"Display This iPod on {host_label}")
        self._display_local_btn = QPushButton("Display Local Pack")
        self._stop_display_btn = QPushButton("Stop Host Display")
        actions.addWidget(self._install_btn)
        actions.addWidget(self._display_device_btn)
        actions.addWidget(self._display_local_btn)
        actions.addWidget(self._stop_display_btn)
        actions.addStretch(1)
        layout.addLayout(actions)

        self._activity = QLabel("")
        self._activity.setWordWrap(True)
        layout.addWidget(self._activity)
        layout.addStretch(1)

        self._source_btn.clicked.connect(self.choose_sources_requested)
        self._install_btn.clicked.connect(self.install_requested)
        self._display_device_btn.clicked.connect(self.display_device_requested)
        self._display_local_btn.clicked.connect(self.display_local_requested)
        self._stop_display_btn.clicked.connect(self.stop_display_requested)
        self._refresh_btn.clicked.connect(self.refresh_requested)
        self._help_btn.clicked.connect(self.capture_help_requested)

    @staticmethod
    def _pack_text(status):
        if status.get("valid"):
            version = status.get("product_version") or "10.6"
            return f"Verified Mac OS X {version} · {status.get('asset_count', 0)} assets"
        if status.get("path") and not status.get("errors"):
            return "Missing"
        return "Missing or invalid"

    def set_status(self, status):
        self._status = dict(status or {})
        local = status.get("local") or {}
        device = status.get("device") or {}
        connected = bool(status.get("device_connected"))
        self._local_state.setText(self._pack_text(local))
        self._device_state.setText(
            self._pack_text(device) if connected else "No iPod connected"
        )
        self._runtime_state.setText(
            "Installed"
            if status.get("device_plugin_present")
            else ("Not installed in connected firmware" if connected else "No iPod connected")
        )
        preview_count = int(status.get("host_preview_count") or 0)
        self._preview_state.setText(
            f"{preview_count} active session{'s' if preview_count != 1 else ''}"
            if preview_count
            else "Not running"
        )
        details = []
        if local.get("errors"):
            details.append("Local: " + str(local["errors"][0]))
        if connected and device.get("errors"):
            details.append("iPod: " + str(device["errors"][0]))
        self._details.setText("\n".join(details))
        self._install_btn.setEnabled(bool(local.get("valid") and connected))
        self._display_local_btn.setEnabled(bool(local.get("valid")))
        self._display_device_btn.setEnabled(
            bool(connected and device.get("valid"))
        )
        self._stop_display_btn.setText(
            f"Stop Host Display ({preview_count})"
            if preview_count
            else "Stop Host Display"
        )
        self._stop_display_btn.setEnabled(preview_count > 0)

    def set_activity(self, text, *, error=False):
        self._activity.setText(str(text or ""))
        self._activity.setStyleSheet("color: #a32020;" if error else "")

    def set_busy(self, busy):
        for button in (
            self._source_btn,
            self._install_btn,
            self._display_device_btn,
            self._display_local_btn,
            self._stop_display_btn,
            self._refresh_btn,
            self._help_btn,
        ):
            button.setEnabled(not busy)
        if not busy and self._status:
            self.set_status(self._status)
