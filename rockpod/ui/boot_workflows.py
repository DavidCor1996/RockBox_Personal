"""UI workflow helpers for Rockbox boot/branding operations."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication, QProgressDialog


class BootProgressController:
    def __init__(self, parent, status_bar):
        self._parent = parent
        self._status_bar = status_bar

    def show(self, title, label, total=4):
        progress = QProgressDialog(label, None, 0, max(total, 1), self._parent)
        progress.setWindowTitle(title)
        progress.setWindowModality(Qt.WindowModal)
        progress.setMinimumDuration(0)
        progress.setAutoClose(False)
        progress.setAutoReset(False)
        progress.setCancelButton(None)
        progress.setValue(0)
        progress.show()
        QApplication.processEvents()
        return progress

    def update(self, progress, current, total, label, busy=False):
        if progress is not None:
            if busy:
                progress.setRange(0, 0)
            else:
                progress.setRange(0, max(total, 1))
                progress.setValue(min(max(current, 0), max(total, 1)))
            progress.setLabelText(label or "Updating Rockbox boot branding...")
        if label and self._status_bar is not None:
            self._status_bar.set_left_text(label)
        QApplication.processEvents()

    def close(self, progress):
        if progress is not None:
            progress.close()
            progress.deleteLater()
            QApplication.processEvents()
