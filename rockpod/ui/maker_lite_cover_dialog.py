"""Exact 144x108 Maker Lite Steam cover preview and crop controls."""

from __future__ import annotations

import os
import tempfile

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPixmap
from PySide6.QtWidgets import (
    QColorDialog,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QVBoxLayout,
)

from services.maker_lite_export import render_cover


class MakerLiteCoverDialog(QDialog):
    def __init__(
        self,
        source_path="",
        fit="contain",
        background="#000000",
        restore_source="",
        parent=None,
    ):
        super().__init__(parent)
        self.setWindowTitle("Maker Lite Steam Cover")
        self.setMinimumWidth(460)
        self._preview_path = os.path.join(
            tempfile.gettempdir(), f"maker-lite-cover-preview-{os.getpid()}.bmp"
        )
        layout = QVBoxLayout(self)
        self.preview = QLabel("Choose a cover image")
        self.preview.setFixedSize(288, 216)
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setStyleSheet("background:#151a21; border:1px solid #596775;")
        layout.addWidget(self.preview, alignment=Qt.AlignHCenter)
        form = QFormLayout()
        source_row = QHBoxLayout()
        self.source = QLineEdit(source_path)
        choose = QPushButton("Choose…")
        choose.clicked.connect(self._choose)
        source_row.addWidget(self.source, 1)
        source_row.addWidget(choose)
        if restore_source:
            restore = QPushButton("Restore Source Cover")
            restore.clicked.connect(lambda: self.source.setText(restore_source))
            source_row.addWidget(restore)
        form.addRow("Source artwork", source_row)
        self.fit = QComboBox()
        self.fit.addItem("Contain entire cover", "contain")
        self.fit.addItem("Crop to fill", "crop")
        self.fit.setCurrentIndex(max(0, self.fit.findData(fit)))
        self.fit.currentIndexChanged.connect(self._update_preview)
        form.addRow("Framing", self.fit)
        color_row = QHBoxLayout()
        self.background = QLineEdit(background)
        color = QPushButton("Color…")
        color.clicked.connect(self._choose_color)
        color_row.addWidget(self.background, 1)
        color_row.addWidget(color)
        form.addRow("Letterbox color", color_row)
        layout.addLayout(form)
        buttons = QDialogButtonBox(QDialogButtonBox.Cancel | QDialogButtonBox.Save)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        self.source.textChanged.connect(self._update_preview)
        self.background.editingFinished.connect(self._update_preview)
        self._update_preview()

    def _choose(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Choose Cover", "", "Images (*.png *.jpg *.jpeg *.webp *.bmp)"
        )
        if path:
            self.source.setText(path)

    def _choose_color(self):
        color = QColorDialog.getColor(QColor(self.background.text()), self)
        if color.isValid():
            self.background.setText(color.name())
            self._update_preview()

    def _update_preview(self):
        path = self.source.text().strip()
        if not os.path.isfile(path):
            self.preview.setText("Choose a readable image")
            self.preview.setPixmap(QPixmap())
            return
        try:
            render_cover(
                path,
                self._preview_path,
                self.fit.currentData(),
                self.background.text().strip() or "#000000",
            )
        except (OSError, ValueError):
            self.preview.setText("Cover cannot be rendered")
            return
        pixmap = QPixmap(self._preview_path)
        self.preview.setPixmap(
            pixmap.scaled(288, 216, Qt.KeepAspectRatio, Qt.FastTransformation)
        )

    def values(self):
        return {
            "cover_source": self.source.text().strip(),
            "cover_fit": self.fit.currentData(),
            "cover_background": self.background.text().strip() or "#000000",
        }
