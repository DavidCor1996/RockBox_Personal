"""Set, change, or remove the shared 4-digit device PIN."""

from PySide6.QtCore import Qt
from PySide6.QtGui import QIntValidator
from PySide6.QtWidgets import (
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
)

from services.device_pin import (
    DevicePinError,
    PIN_LENGTH,
    clear_pin,
    is_valid_pin,
    pin_path,
    read_pin,
    write_pin,
)


class DevicePinDialog(QDialog):
    """Edit the PIN that unlocks Settings and locked media on the device."""

    def __init__(self, mount_path, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Device PIN")
        self.setModal(True)
        self.setMinimumWidth(430)
        self._mount_path = str(mount_path or "")

        layout = QVBoxLayout(self)

        intro = QLabel(
            "This 4-digit PIN unlocks the Settings menu, Locked Videos, "
            "locked Magazines and Comics, and parental-locked DIRECTV "
            "channels on the iPod."
        )
        intro.setWordWrap(True)
        layout.addWidget(intro)

        self._status = QLabel()
        self._status.setWordWrap(True)
        self._status.setStyleSheet("font-weight: bold;")
        layout.addWidget(self._status)

        form = QFormLayout()
        self._pin = QLineEdit()
        self._pin.setMaxLength(PIN_LENGTH)
        self._pin.setValidator(QIntValidator(0, 9999, self))
        self._pin.setEchoMode(QLineEdit.Password)
        self._pin.setPlaceholderText("0000")
        self._pin.textChanged.connect(self._update_buttons)
        form.addRow("New PIN:", self._pin)

        self._confirm = QLineEdit()
        self._confirm.setMaxLength(PIN_LENGTH)
        self._confirm.setValidator(QIntValidator(0, 9999, self))
        self._confirm.setEchoMode(QLineEdit.Password)
        self._confirm.setPlaceholderText("0000")
        self._confirm.textChanged.connect(self._update_buttons)
        form.addRow("Confirm PIN:", self._confirm)
        layout.addLayout(form)

        self._show_pin = QPushButton("Show PIN")
        self._show_pin.setCheckable(True)
        self._show_pin.toggled.connect(self._toggle_echo)
        show_row = QHBoxLayout()
        show_row.addStretch(1)
        show_row.addWidget(self._show_pin)
        layout.addLayout(show_row)

        self._path_label = QLabel(pin_path(self._mount_path))
        self._path_label.setWordWrap(True)
        self._path_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self._path_label.setStyleSheet("color: palette(mid);")
        layout.addWidget(self._path_label)

        buttons = QDialogButtonBox(
            QDialogButtonBox.Save | QDialogButtonBox.Cancel
        )
        self._save_button = buttons.button(QDialogButtonBox.Save)
        self._remove_button = buttons.addButton(
            "Remove PIN", QDialogButtonBox.DestructiveRole
        )
        self._remove_button.clicked.connect(self._remove_pin)
        buttons.accepted.connect(self._save_pin)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self._refresh_status()

    def _refresh_status(self):
        current = read_pin(self._mount_path)
        if current:
            self._status.setText("A PIN is currently set on this iPod.")
        else:
            self._status.setText(
                "No PIN is set on this iPod. Anything that asks for it will "
                "report \"Settings lock unavailable\" until you set one."
            )
        self._remove_button.setEnabled(bool(current))
        self._update_buttons()

    def _toggle_echo(self, shown):
        mode = QLineEdit.Normal if shown else QLineEdit.Password
        self._pin.setEchoMode(mode)
        self._confirm.setEchoMode(mode)
        self._show_pin.setText("Hide PIN" if shown else "Show PIN")

    def _update_buttons(self):
        pin = self._pin.text()
        self._save_button.setEnabled(
            is_valid_pin(pin) and pin == self._confirm.text()
        )

    def _save_pin(self):
        pin = self._pin.text()
        if not is_valid_pin(pin):
            QMessageBox.warning(
                self, "Invalid PIN", "The PIN must be exactly 4 digits (0-9)."
            )
            return
        if pin != self._confirm.text():
            QMessageBox.warning(
                self, "PIN Mismatch", "The two PINs do not match."
            )
            return
        try:
            write_pin(self._mount_path, pin)
        except DevicePinError as exc:
            QMessageBox.critical(self, "Device PIN", str(exc))
            return
        QMessageBox.information(
            self,
            "Device PIN",
            "The PIN was saved. Eject the iPod before unplugging it so the "
            "change is flushed to disk.",
        )
        self.accept()

    def _remove_pin(self):
        confirmed = QMessageBox.question(
            self,
            "Remove PIN",
            "Remove the PIN from this iPod? Locked Videos, Magazines, Comics "
            "and locked DIRECTV channels will stay unreachable until a new "
            "PIN is set.",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if confirmed != QMessageBox.Yes:
            return
        try:
            clear_pin(self._mount_path)
        except DevicePinError as exc:
            QMessageBox.critical(self, "Device PIN", str(exc))
            return
        self._pin.clear()
        self._confirm.clear()
        self._refresh_status()
