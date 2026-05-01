"""Dedicated iPone wallpaper picker with previews."""

from __future__ import annotations

import os

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QComboBox,
    QFileDialog,
    QFrame,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class _WallpaperPane(QFrame):
    selection_changed = Signal()
    apply_clicked = Signal()
    add_clicked = Signal()
    remove_clicked = Signal()

    def __init__(self, title: str, empty_text: str, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 8, 10, 8)
        layout.setSpacing(8)
        layout.addWidget(QLabel(title))

        self._list = QListWidget()
        self._list.currentItemChanged.connect(lambda *_: self.selection_changed.emit())
        layout.addWidget(self._list, 1)

        self._preview = QLabel(empty_text)
        self._preview.setAlignment(Qt.AlignCenter)
        self._preview.setMinimumHeight(220)
        self._preview.setObjectName("theme_preview")
        layout.addWidget(self._preview)

        self._detail = QLabel("")
        self._detail.setWordWrap(True)
        layout.addWidget(self._detail)

        buttons = QHBoxLayout()
        self._add_btn = QPushButton("Add...")
        self._add_btn.clicked.connect(self.add_clicked)
        self._remove_btn = QPushButton("Remove")
        self._remove_btn.clicked.connect(self.remove_clicked)
        self._apply_btn = QPushButton("Apply")
        self._apply_btn.clicked.connect(self.apply_clicked)
        buttons.addWidget(self._add_btn)
        buttons.addWidget(self._remove_btn)
        buttons.addStretch(1)
        buttons.addWidget(self._apply_btn)
        layout.addLayout(buttons)

    def set_candidates(self, items):
        previous = self.current_candidate()
        previous_key = self._candidate_key(previous) if previous else ""
        self._list.blockSignals(True)
        self._list.clear()
        selected_row = -1
        for item in items:
            row = QListWidgetItem(item["label"])
            row.setData(Qt.UserRole, dict(item))
            row.setToolTip(item["source_path"])
            self._list.addItem(row)
            if previous_key and self._candidate_key(item) == previous_key:
                selected_row = self._list.count() - 1
        if self._list.count():
            self._list.setCurrentRow(selected_row if selected_row >= 0 else 0)
        self._list.blockSignals(False)
        self.refresh_preview()

    def add_custom_candidate(self, path: str, label: str):
        row = QListWidgetItem(label)
        row.setData(
            Qt.UserRole,
            {
                "id": os.path.abspath(path),
                "label": label,
                "source_path": os.path.abspath(path),
                "preview_path": os.path.abspath(path),
                "origin": "custom",
                "removable": True,
            },
        )
        self._list.insertItem(0, row)
        self._list.setCurrentItem(row)
        self.refresh_preview()

    def current_candidate(self):
        item = self._list.currentItem()
        if not item:
            return None
        return item.data(Qt.UserRole)

    @staticmethod
    def _candidate_key(candidate):
        if not candidate:
            return ""
        return str(candidate.get("id") or candidate.get("source_path") or "").strip()

    def refresh_preview(self):
        candidate = self.current_candidate()
        if not candidate:
            self._preview.setPixmap(QPixmap())
            self._preview.setText("No selection")
            self._detail.setText("")
            self._remove_btn.setEnabled(False)
            return
        preview_path = candidate.get("preview_path", "")
        if preview_path and os.path.isfile(preview_path):
            px = QPixmap(preview_path)
            if not px.isNull():
                self._preview.setPixmap(px.scaled(320, 240, Qt.KeepAspectRatio, Qt.SmoothTransformation))
                self._preview.setText("")
            else:
                self._preview.setPixmap(QPixmap())
                self._preview.setText("Preview unavailable")
        else:
            self._preview.setPixmap(QPixmap())
            self._preview.setText("Preview unavailable")
        removable = bool(candidate.get("removable"))
        self._remove_btn.setEnabled(removable)
        self._detail.setText(
            f"Source: {candidate.get('origin', 'custom').title()}\n"
            f"Size: {candidate.get('width', 0)}x{candidate.get('height', 0)}\n"
            f"{candidate.get('source_path', '')}\n"
            f"Removable: {'Yes' if removable else 'No'}"
        )


class IPoneWallpaperManagerWidget(QWidget):
    """Manage active iPone lock and charge wallpapers on the mounted device."""

    profile_selected = Signal(str)
    apply_requested = Signal(dict)
    import_requested = Signal(str, str)
    remove_requested = Signal(str, dict)
    clock_position_changed = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._profiles = {}

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(8)

        header = QFrame()
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(10, 8, 10, 8)
        header_layout.setSpacing(8)
        header_layout.addWidget(QLabel("Profile:"))
        self._profile_combo = QComboBox()
        self._profile_combo.currentIndexChanged.connect(self._emit_profile_selected)
        header_layout.addWidget(self._profile_combo, 1)
        header_layout.addWidget(QLabel("Lockscreen Clock:"))
        self._clock_position_combo = QComboBox()
        self._clock_position_combo.addItem("Centered", "center")
        self._clock_position_combo.addItem("Left", "left")
        self._clock_position_combo.currentIndexChanged.connect(self._emit_clock_position_changed)
        header_layout.addWidget(self._clock_position_combo)
        self._apply_both_btn = QPushButton("Apply Both")
        self._apply_both_btn.clicked.connect(self._emit_apply_both)
        header_layout.addWidget(self._apply_both_btn)
        layout.addWidget(header)

        self._summary = QLabel(
            "Pick a lock wallpaper and a charge wallpaper, choose the lockscreen clock position, then apply to the mounted iPod."
        )
        self._summary.setWordWrap(True)
        self._summary.setObjectName("theme_hub_status")
        layout.addWidget(self._summary)

        panes = QHBoxLayout()
        panes.setSpacing(10)
        self._lock_pane = _WallpaperPane("Lockscreen Wallpaper", "No Lock Preview")
        self._charge_pane = _WallpaperPane("Charge Wallpaper", "No Charge Preview")
        self._lock_pane.selection_changed.connect(self._lock_pane.refresh_preview)
        self._charge_pane.selection_changed.connect(self._charge_pane.refresh_preview)
        self._lock_pane.apply_clicked.connect(self._emit_apply_lock)
        self._charge_pane.apply_clicked.connect(self._emit_apply_charge)
        self._lock_pane.add_clicked.connect(self._choose_custom_lock)
        self._charge_pane.add_clicked.connect(self._choose_custom_charge)
        self._lock_pane.remove_clicked.connect(self._emit_remove_lock)
        self._charge_pane.remove_clicked.connect(self._emit_remove_charge)
        panes.addWidget(self._lock_pane, 1)
        panes.addWidget(self._charge_pane, 1)
        layout.addLayout(panes, 1)

    def set_profiles(self, profiles, selected_id):
        self._profiles = {item["id"]: item for item in profiles}
        current_id = self.current_profile_id()
        self._profile_combo.blockSignals(True)
        self._profile_combo.clear()
        selected_index = 0
        for index, profile in enumerate(profiles):
            self._profile_combo.addItem(profile["name"], profile["id"])
            if current_id and profile["id"] == current_id:
                selected_index = index
            elif profile["id"] == selected_id:
                selected_index = index
        self._profile_combo.setCurrentIndex(selected_index)
        self._profile_combo.blockSignals(False)

    def set_candidates(self, lock_items, charge_items):
        self._lock_pane.set_candidates(lock_items)
        self._charge_pane.set_candidates(charge_items)

    def current_profile_id(self):
        return self._profile_combo.currentData() or ""

    def current_selection(self):
        lock = self._lock_pane.current_candidate()
        charge = self._charge_pane.current_candidate()
        return {
            "profile_id": self.current_profile_id(),
            "lock_source": lock.get("source_path", "") if lock else "",
            "charge_source": charge.get("source_path", "") if charge else "",
            "clock_position": self.current_clock_position(),
        }

    def current_clock_position(self):
        return self._clock_position_combo.currentData() or "center"

    def set_clock_position(self, value, supported=True):
        normalized = "left" if str(value or "").strip().lower() == "left" else "center"
        self._clock_position_combo.blockSignals(True)
        index = self._clock_position_combo.findData(normalized)
        self._clock_position_combo.setCurrentIndex(index if index >= 0 else 0)
        self._clock_position_combo.blockSignals(False)
        self._clock_position_combo.setEnabled(bool(supported))
        self._clock_position_combo.setToolTip(
            "" if supported else "Lockscreen clock alignment is currently supported on 320x240 iPone themes."
        )

    def _emit_profile_selected(self):
        profile_id = self.current_profile_id()
        if profile_id:
            self.profile_selected.emit(profile_id)

    def _emit_apply_lock(self):
        selection = self.current_selection()
        selection["charge_source"] = ""
        self.apply_requested.emit(selection)

    def _emit_apply_charge(self):
        selection = self.current_selection()
        selection["lock_source"] = ""
        self.apply_requested.emit(selection)

    def _emit_apply_both(self):
        self.apply_requested.emit(self.current_selection())

    def _emit_clock_position_changed(self):
        self.clock_position_changed.emit(self.current_clock_position())

    def _choose_custom_lock(self):
        path, _ = QFileDialog.getOpenFileName(self, "Choose Lock Wallpaper", "", "Images (*.bmp *.png *.jpg *.jpeg)")
        if path:
            self.import_requested.emit("lock", path)

    def _choose_custom_charge(self):
        path, _ = QFileDialog.getOpenFileName(self, "Choose Charge Wallpaper", "", "Images (*.bmp *.png *.jpg *.jpeg)")
        if path:
            self.import_requested.emit("charge", path)

    def _emit_remove_lock(self):
        candidate = self._lock_pane.current_candidate()
        if candidate:
            self.remove_requested.emit("lock", dict(candidate))

    def _emit_remove_charge(self):
        candidate = self._charge_pane.current_candidate()
        if candidate:
            self.remove_requested.emit("charge", dict(candidate))
