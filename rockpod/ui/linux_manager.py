"""RockPod Linux payload manager panel."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QPointF, QRectF, Signal, Qt
from PySide6.QtGui import QColor, QPainter, QPainterPath, QPen, QTextCursor
from PySide6.QtWidgets import (
    QAbstractButton,
    QDialog,
    QDialogButtonBox,
    QFrame,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPlainTextEdit,
    QProgressBar,
    QPushButton,
    QSpinBox,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from services.linux_payload import DEFAULT_ALLOCATION_GB, MAX_ALLOCATION_GB
from ui.storage_bar import format_bytes

try:
    from PySide6.QtSvg import QSvgRenderer
except ImportError:  # pragma: no cover - depends on optional Qt packaging
    QSvgRenderer = None


LINUX_ICON_PATH = Path(__file__).resolve().parents[1] / "assets" / "icons" / "rockpod-linux.svg"


class LinuxManagerWidget(QWidget):
    download_requested = Signal()
    stage_requested = Signal()
    install_requested = Signal(int)
    start_requested = Signal()
    provision_requested = Signal()
    uninstall_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("linux_manager")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(8)

        top = QHBoxLayout()
        self._art = LinuxMascotWidget()
        top.addWidget(self._art)

        header = QFrame()
        header.setObjectName("theme_hub_header")
        grid = QGridLayout(header)
        grid.setContentsMargins(10, 8, 10, 8)
        grid.setHorizontalSpacing(8)
        grid.setVerticalSpacing(4)

        self._device_label = QLabel("No iPod connected")
        self._iso_label = QLabel("Debian ISO: not checked")
        self._stage_label = QLabel("Payload: not staged")
        self._install_label = QLabel("Install: not checked")
        self._space_label = QLabel("Space: unknown")
        self._allocation = QSpinBox()
        self._allocation.setRange(1, MAX_ALLOCATION_GB)
        self._allocation.setValue(DEFAULT_ALLOCATION_GB)
        self._allocation.setSuffix(" GB")
        self._username = QLineEdit("rockpod")

        self._download_btn = QPushButton("Download Debian")
        self._stage_btn = QPushButton("Stage VM (cache)")
        self._install_btn = QPushButton("Install Linux VM to iPod")
        self._provision_btn = QPushButton("Install Apps/Theme")
        self._uninstall_btn = QPushButton("Uninstall from iPod")
        self._start_btn = QPushButton("Start VM")
        self._download_btn.clicked.connect(self.download_requested)
        self._stage_btn.clicked.connect(self.stage_requested)
        self._install_btn.clicked.connect(self._emit_install)
        self._provision_btn.clicked.connect(self.provision_requested)
        self._uninstall_btn.clicked.connect(self.uninstall_requested)
        self._start_btn.clicked.connect(self.start_requested)

        grid.addWidget(QLabel("Device:"), 0, 0)
        grid.addWidget(self._device_label, 0, 1, 1, 3)
        grid.addWidget(QLabel("Allocation:"), 1, 0)
        grid.addWidget(self._allocation, 1, 1)
        grid.addWidget(QLabel(f"Maximum {MAX_ALLOCATION_GB} GB; rest remains for Rockbox/stock."), 1, 2, 1, 2)
        grid.addWidget(QLabel("User:"), 2, 0)
        grid.addWidget(self._username, 2, 1)
        grid.addWidget(QLabel("Passwords are requested when staging or installing."), 2, 2, 1, 2)
        grid.addWidget(QLabel("ISO:"), 3, 0)
        grid.addWidget(self._iso_label, 3, 1, 1, 3)
        grid.addWidget(QLabel("Payload:"), 4, 0)
        grid.addWidget(self._stage_label, 4, 1, 1, 3)
        grid.addWidget(QLabel("Install:"), 5, 0)
        grid.addWidget(self._install_label, 5, 1, 1, 3)
        grid.addWidget(QLabel("Space:"), 6, 0)
        grid.addWidget(self._space_label, 6, 1, 1, 3)

        buttons = QHBoxLayout()
        for button in (self._download_btn, self._stage_btn, self._install_btn, self._provision_btn, self._start_btn):
            buttons.addWidget(button)
        buttons.addWidget(self._uninstall_btn)
        buttons.addStretch(1)

        safety = QGroupBox("Safety")
        safety_layout = QVBoxLayout(safety)
        self._safety_text = QTextEdit()
        self._safety_text.setReadOnly(True)
        self._safety_text.setMaximumHeight(118)
        self._safety_text.setPlainText(
            "Stage VM prepares a local cache only (host storage). "
            "Install Linux VM copies files to the mounted iPod under "
            "`Linux/RockPodVM`, then launches autoinstall from the iPod. "
            "RockPod backs up overwritten Linux-owned paths and removes only manifest-owned files. "
            "The selected allocation caps RockPod-managed Linux payload and persistence space."
        )
        safety_layout.addWidget(self._safety_text)

        top.addWidget(header, 1)
        layout.addLayout(top)
        layout.addLayout(buttons)
        layout.addWidget(safety)
        layout.addStretch(1)

    def set_status(self, status):
        device = status.get("device") or {}
        if device:
            self._device_label.setText(f"{device.get('name', 'iPod')} at {device.get('mount_path', '')}")
        else:
            self._device_label.setText("No iPod connected")
        self._iso_label.setText(status.get("iso", "Debian ISO: not checked"))
        self._stage_label.setText(status.get("stage", "Payload: not staged"))
        self._install_label.setText(status.get("install", "Install: not checked"))
        free = int(status.get("free_bytes") or 0)
        payload = int(status.get("payload_bytes") or 0)
        if free or payload:
            self._space_label.setText(f"Payload {format_bytes(payload)} · Free {format_bytes(free)}")
        else:
            self._space_label.setText("Space: unknown")
        self._download_btn.setEnabled(bool(status.get("download_enabled", True)))
        self._stage_btn.setEnabled(bool(status.get("stage_enabled", True)))
        self._install_btn.setEnabled(bool(status.get("install_enabled", False)))
        self._provision_btn.setEnabled(bool(status.get("provision_enabled", False)))
        self._start_btn.setEnabled(bool(status.get("start_enabled", False)))
        self._uninstall_btn.setEnabled(bool(status.get("uninstall_enabled", False)))

    def allocation_gb(self):
        return int(self._allocation.value())

    def vm_username(self):
        return self._username.text().strip()

    def prompt_vm_user(self):
        dialog = LinuxCredentialsDialog(self.vm_username(), self)
        if dialog.exec() != QDialog.Accepted:
            return None
        credentials = dialog.credentials()
        self._username.setText(credentials["username"])
        return credentials

    def _emit_install(self):
        self.install_requested.emit(self.allocation_gb())


class LinuxInstallProgressDialog(QDialog):
    stop_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("RockPod Linux Install")
        self.setMinimumSize(720, 420)
        self._running = True

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(8)

        self._status = QLabel("Starting Debian autoinstall VM...")
        layout.addWidget(self._status)

        self._progress = QProgressBar()
        self._progress.setRange(0, 0)
        self._progress.setTextVisible(False)
        layout.addWidget(self._progress)

        self._log = QPlainTextEdit()
        self._log.setReadOnly(True)
        self._log.setLineWrapMode(QPlainTextEdit.NoWrap)
        self._log.setPlainText(
            "The unattended Debian installer output will appear here.\n"
            "When it finishes, RockPod will enable Start VM.\n\n"
        )
        layout.addWidget(self._log, 1)

        self._buttons = QDialogButtonBox()
        self._hide_button = self._buttons.addButton("Hide", QDialogButtonBox.RejectRole)
        self._stop_button = self._buttons.addButton("Stop Install", QDialogButtonBox.DestructiveRole)
        self._buttons.clicked.connect(self._on_button_clicked)
        layout.addWidget(self._buttons)

    def append_output(self, text):
        if not text:
            return
        self._log.moveCursor(QTextCursor.MoveOperation.End)
        self._log.insertPlainText(str(text))
        self._log.moveCursor(QTextCursor.MoveOperation.End)

    def set_status(self, text):
        self._status.setText(text)

    def mark_finished(self, message, success=True):
        self._running = False
        self._status.setText(message)
        self._progress.setRange(0, 1)
        self._progress.setValue(1 if success else 0)
        self._stop_button.setEnabled(False)
        self._hide_button.setText("Close")

    def closeEvent(self, event):
        if self._running:
            self.hide()
            event.ignore()
            return
        super().closeEvent(event)

    def _on_button_clicked(self, button: QAbstractButton):
        if button == self._stop_button and self._running:
            self.stop_requested.emit()
            return
        self.close()


class LinuxCredentialsDialog(QDialog):
    def __init__(self, username, parent=None):
        super().__init__(parent)
        self.setWindowTitle("RockPod Linux Passwords")

        layout = QVBoxLayout(self)
        grid = QGridLayout()
        grid.setHorizontalSpacing(8)
        grid.setVerticalSpacing(6)

        self._username = QLineEdit(username or "rockpod")
        self._password = QLineEdit()
        self._password.setEchoMode(QLineEdit.Password)
        self._confirm_password = QLineEdit()
        self._confirm_password.setEchoMode(QLineEdit.Password)
        self._su_password = QLineEdit()
        self._su_password.setEchoMode(QLineEdit.Password)
        self._su_password.setPlaceholderText("Root disabled when blank")
        self._error = QLabel("")
        self._error.setStyleSheet("color: #b00020;")

        grid.addWidget(QLabel("Linux user:"), 0, 0)
        grid.addWidget(self._username, 0, 1)
        grid.addWidget(QLabel("User password:"), 1, 0)
        grid.addWidget(self._password, 1, 1)
        grid.addWidget(QLabel("Confirm password:"), 2, 0)
        grid.addWidget(self._confirm_password, 2, 1)
        grid.addWidget(QLabel("Su password:"), 3, 0)
        grid.addWidget(self._su_password, 3, 1)

        layout.addWidget(QLabel("These values are used for the unattended Debian install."))
        layout.addLayout(grid)
        layout.addWidget(self._error)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self._accept_if_valid)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def credentials(self):
        return {
            "username": self._username.text().strip(),
            "password": self._password.text(),
            "su_password": self._su_password.text(),
        }

    def _accept_if_valid(self):
        credentials = self.credentials()
        if not credentials["username"]:
            self._error.setText("Linux user is required.")
            return
        if not credentials["password"]:
            self._error.setText("User password is required.")
            return
        if credentials["password"] != self._confirm_password.text():
            self._error.setText("User passwords do not match.")
            return
        self.accept()


class LinuxMascotWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(118, 118)
        self.setMaximumSize(150, 150)
        self._renderer = None
        if QSvgRenderer is not None and LINUX_ICON_PATH.is_file():
            renderer = QSvgRenderer(str(LINUX_ICON_PATH))
            if renderer.isValid():
                self._renderer = renderer

    def paintEvent(self, event):
        del event
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        if self._renderer is not None:
            self._renderer.render(p, QRectF(0, 0, self.width(), self.height()))
            p.end()
            return

        w = self.width()
        h = self.height()
        p.translate(w / 2, h / 2)
        scale = min(w, h) / 150.0
        p.scale(scale, scale)
        p.translate(-75, -75)

        p.setPen(QPen(QColor("#34383d"), 2))
        p.setBrush(QColor("#20242a"))
        p.drawEllipse(QRectF(44, 14, 62, 92))
        p.drawEllipse(QRectF(36, 54, 78, 58))

        p.setPen(Qt.NoPen)
        p.setBrush(QColor("#f6f3df"))
        p.drawEllipse(QRectF(52, 50, 46, 56))

        p.setBrush(QColor("#f7f7f7"))
        p.drawEllipse(QRectF(56, 32, 14, 16))
        p.drawEllipse(QRectF(80, 32, 14, 16))
        p.setBrush(QColor("#111111"))
        p.drawEllipse(QRectF(61, 38, 5, 5))
        p.drawEllipse(QRectF(84, 38, 5, 5))

        p.setBrush(QColor("#efb23d"))
        beak = QPainterPath()
        beak.moveTo(70, 47)
        beak.lineTo(80, 47)
        beak.lineTo(75, 55)
        beak.closeSubpath()
        p.drawPath(beak)
        p.drawEllipse(QRectF(42, 106, 28, 12))
        p.drawEllipse(QRectF(80, 106, 28, 12))

        p.setPen(QPen(QColor("#dfe7ef"), 2))
        p.setBrush(QColor("#f5f8fb"))
        p.drawLine(QPointF(64, 50), QPointF(58, 78))
        p.drawLine(QPointF(86, 50), QPointF(92, 78))
        p.drawEllipse(QRectF(58, 48, 8, 8))
        p.drawEllipse(QRectF(84, 48, 8, 8))

        p.setBrush(QColor("#20242a"))
        p.drawEllipse(QRectF(24, 68, 34, 17))
        p.drawEllipse(QRectF(92, 68, 34, 17))

        p.setPen(QPen(QColor("#5d6670"), 2))
        p.setBrush(QColor("#d9d9d9"))
        p.drawRoundedRect(QRectF(48, 74, 54, 62), 8, 8)
        p.setBrush(QColor("#9ec4d8"))
        p.drawRoundedRect(QRectF(57, 83, 36, 22), 3, 3)
        p.setBrush(QColor("#eef2f5"))
        p.drawEllipse(QRectF(64, 112, 22, 22))
        p.setBrush(QColor("#b7c2cc"))
        p.drawEllipse(QRectF(72, 120, 6, 6))
        p.setPen(QPen(QColor("#dfe7ef"), 2))
        p.drawLine(QPointF(52, 100), QPointF(98, 100))
        p.end()
