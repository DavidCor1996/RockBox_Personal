"""Dedicated Rockbox theme wallpaper picker with previews."""

from __future__ import annotations

import os

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFileDialog,
    QFormLayout,
    QFrame,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)


class _WallpaperPane(QFrame):
    selection_changed = Signal()
    apply_clicked = Signal()
    add_clicked = Signal()
    hide_clicked = Signal()
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
        self._hide_btn = QPushButton("Hide")
        self._hide_btn.clicked.connect(self.hide_clicked)
        self._remove_btn = QPushButton("Remove")
        self._remove_btn.clicked.connect(self.remove_clicked)
        self._apply_btn = QPushButton("Apply")
        self._apply_btn.clicked.connect(self.apply_clicked)
        buttons.addWidget(self._add_btn)
        buttons.addWidget(self._hide_btn)
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
            self._hide_btn.setEnabled(False)
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
        hidden = bool(candidate.get("hidden"))
        self._hide_btn.setEnabled(True)
        self._hide_btn.setText("Unhide" if hidden else "Hide")
        self._remove_btn.setEnabled(removable)
        self._detail.setText(
            f"Source: {candidate.get('origin', 'custom').title()}\n"
            f"Size: {candidate.get('width', 0)}x{candidate.get('height', 0)}\n"
            f"{candidate.get('source_path', '')}\n"
            f"Hidden: {'Yes' if hidden else 'No'}\n"
            f"Removable: {'Yes' if removable else 'No'}"
        )


class IPoneWallpaperManagerWidget(QWidget):
    """Manage active theme lock and charge wallpapers on the mounted device."""

    profile_selected = Signal(str)
    theme_selected = Signal(str)
    apply_requested = Signal(dict)
    import_requested = Signal(str, str)
    hide_requested = Signal(str, dict)
    remove_requested = Signal(str, dict)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._profiles = {}
        self._themes = {}

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
        header_layout.addWidget(QLabel("Theme:"))
        self._theme_combo = QComboBox()
        self._theme_combo.currentIndexChanged.connect(self._emit_theme_selected)
        header_layout.addWidget(self._theme_combo, 1)
        self._apply_both_btn = QPushButton("Apply Both")
        self._apply_both_btn.clicked.connect(self._emit_apply_both)
        header_layout.addWidget(self._apply_both_btn)
        self._lock_to_charge_btn = QPushButton("Lock -> Charge")
        self._lock_to_charge_btn.clicked.connect(self._emit_apply_lock_as_charge)
        header_layout.addWidget(self._lock_to_charge_btn)
        self._charge_to_lock_btn = QPushButton("Charge -> Lock")
        self._charge_to_lock_btn.clicked.connect(self._emit_apply_charge_as_lock)
        header_layout.addWidget(self._charge_to_lock_btn)
        layout.addWidget(header)

        self._summary = QLabel(
            "Pick a theme, a lock wallpaper, and a charge wallpaper, then apply them to the mounted iPod."
        )
        self._summary.setWordWrap(True)
        self._summary.setObjectName("theme_hub_status")
        layout.addWidget(self._summary)

        panes = QHBoxLayout()
        panes.setSpacing(10)
        self._lock_pane = _WallpaperPane("Lockscreen Wallpaper", "No Lock Preview")
        self._charge_pane = _WallpaperPane("Charge Wallpaper", "No Charge Preview")
        self._pictureflow_pane = _WallpaperPane("PictureFlow Init Wallpaper", "No PictureFlow Preview")
        self._lock_pane.selection_changed.connect(self._lock_pane.refresh_preview)
        self._charge_pane.selection_changed.connect(self._charge_pane.refresh_preview)
        self._pictureflow_pane.selection_changed.connect(self._pictureflow_pane.refresh_preview)
        self._lock_pane.apply_clicked.connect(self._emit_apply_lock)
        self._charge_pane.apply_clicked.connect(self._emit_apply_charge)
        self._pictureflow_pane.apply_clicked.connect(self._emit_apply_pictureflow)
        self._lock_pane.add_clicked.connect(self._choose_custom_lock)
        self._charge_pane.add_clicked.connect(self._choose_custom_charge)
        self._pictureflow_pane.add_clicked.connect(self._choose_custom_pictureflow)
        self._lock_pane.hide_clicked.connect(self._emit_hide_lock)
        self._charge_pane.hide_clicked.connect(self._emit_hide_charge)
        self._pictureflow_pane.hide_clicked.connect(self._emit_hide_pictureflow)
        self._lock_pane.remove_clicked.connect(self._emit_remove_lock)
        self._charge_pane.remove_clicked.connect(self._emit_remove_charge)
        self._pictureflow_pane.remove_clicked.connect(self._emit_remove_pictureflow)

        custom = QGroupBox("Lock Screen")
        custom_layout = QHBoxLayout(custom)
        custom_layout.setContentsMargins(10, 8, 10, 8)
        custom_layout.setSpacing(14)

        clock_form = QFormLayout()
        self._clock_position_combo = QComboBox()
        for label, value in [
            ("Center", "center"),
            ("Top", "top"),
            ("Custom", "custom"),
        ]:
            self._clock_position_combo.addItem(label, value)
        self._clock_y_spin = QSpinBox()
        self._clock_y_spin.setRange(0, 64)
        self._clock_y_spin.setValue(32)
        self._clock_height_spin = QSpinBox()
        self._clock_height_spin.setRange(20, 120)
        self._clock_height_spin.setValue(55)
        self._clock_align_combo = QComboBox()
        for label, value in [("Center", "center"), ("Left", "left")]:
            self._clock_align_combo.addItem(label, value)
        clock_form.addRow("Clock Position", self._clock_position_combo)
        clock_form.addRow("Clock Y", self._clock_y_spin)
        clock_form.addRow("Clock Height", self._clock_height_spin)
        clock_form.addRow("Clock Align", self._clock_align_combo)
        custom_layout.addLayout(clock_form, 1)

        style_form = QFormLayout()
        self._clock_font_combo = QComboBox()
        entries = [
            ("iPone Default", "35-Adobe-Helvetica-Bold.fnt"),
            ("Adobe Helvetica", "16-Adobe-Helvetica-Bold.fnt"),
            ("Cantarell Bold", "18-Cantarell-Bold.fnt"),
            ("Light Poster", "66-Cantarell-Light.fnt"),
        ]
        fonts_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "fonts")
        for size in (50, 55, 60, 72, 78, 84, 90):
            for fname in sorted(os.listdir(fonts_dir), reverse=True):
                if not fname.lower().endswith(".fnt"):
                    continue
                prefix = f"{size}-"
                if not fname.startswith(prefix):
                    continue
                if fname.count("-") < 2:
                    continue
                rest = fname[len(prefix):].replace(".fnt", "")
                label = f"{size}px {rest}"
                entries.append((label, fname))
        for label, value in entries:
            self._clock_font_combo.addItem(label, value)
        self._date_mode_combo = QComboBox()
        for label, value in [("Below", "below"), ("Above", "above"), ("Follow", "follow")]:
            self._date_mode_combo.addItem(label, value)
        style_form.addRow("Clock Font", self._clock_font_combo)
        style_form.addRow("Date", self._date_mode_combo)
        custom_layout.addLayout(style_form, 1)

        card_form = QFormLayout()
        self._auto_contrast_check = QCheckBox("Auto Contrast")
        self._auto_contrast_check.setChecked(True)
        self._mini_blur_combo = QComboBox()
        for label, value in [("Low", "low"), ("Medium", "medium"), ("High", "high")]:
            self._mini_blur_combo.addItem(label, value)
        self._apply_lockscreen_custom_btn = QPushButton("Apply Lock Screen")
        self._apply_lockscreen_custom_btn.clicked.connect(self._emit_apply_lockscreen_customization)
        card_form.addRow("Readability", self._auto_contrast_check)
        card_form.addRow("Mini Blur", self._mini_blur_combo)
        card_form.addRow("", self._apply_lockscreen_custom_btn)
        custom_layout.addLayout(card_form, 1)
        layout.addWidget(custom)

        panes.addWidget(self._lock_pane, 1)
        panes.addWidget(self._charge_pane, 1)
        panes.addWidget(self._pictureflow_pane, 1)
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

    def set_candidates(self, lock_items, charge_items, pictureflow_items=None):
        self._lock_pane.set_candidates(lock_items)
        self._charge_pane.set_candidates(charge_items)
        self._pictureflow_pane.set_candidates(pictureflow_items or [])

    def set_themes(self, themes, selected_theme_id):
        self._themes = {item["id"]: item for item in themes}
        self._theme_combo.blockSignals(True)
        self._theme_combo.clear()
        selected_index = 0
        for index, theme in enumerate(themes):
            label = theme.get("name") or theme["id"]
            self._theme_combo.addItem(label, theme["id"])
            if theme["id"] == selected_theme_id:
                selected_index = index
        if themes:
            self._theme_combo.setCurrentIndex(selected_index)
        self._theme_combo.blockSignals(False)
        self._theme_combo.setEnabled(bool(themes))

    def current_profile_id(self):
        return self._profile_combo.currentData() or ""

    def current_theme_id(self):
        return self._theme_combo.currentData() or ""

    def current_selection(self):
        lock = self._lock_pane.current_candidate()
        charge = self._charge_pane.current_candidate()
        pictureflow = self._pictureflow_pane.current_candidate()
        return {
            "profile_id": self.current_profile_id(),
            "theme_id": self.current_theme_id(),
            "lock_source": lock.get("source_path", "") if lock else "",
            "charge_source": charge.get("source_path", "") if charge else "",
            "pictureflow_source": pictureflow.get("source_path", "") if pictureflow else "",
            "lockscreen_customization": self.current_lockscreen_customization(),
        }

    def set_lockscreen_customization(self, customization):
        clock = (customization or {}).get("clock", {}) if isinstance(customization, dict) else {}
        date = (customization or {}).get("date", {}) if isinstance(customization, dict) else {}
        readability = (customization or {}).get("readability", {}) if isinstance(customization, dict) else {}
        mini = (customization or {}).get("mini_player", {}) if isinstance(customization, dict) else {}
        self._set_combo_value(self._clock_position_combo, clock.get("position", "center"))
        self._clock_y_spin.setValue(int(clock.get("y", 32) or 32))
        self._clock_height_spin.setValue(int(clock.get("height", 55) or 55))
        self._set_combo_value(self._clock_align_combo, clock.get("align", "center"))
        self._set_combo_value(self._clock_font_combo, clock.get("font", "35-Adobe-Helvetica-Bold.fnt"))
        self._set_combo_value(self._date_mode_combo, date.get("mode", "below"))
        self._auto_contrast_check.setChecked(bool(readability.get("auto_contrast", True)))
        self._set_combo_value(self._mini_blur_combo, mini.get("blur_strength", "medium"))

    def current_lockscreen_customization(self):
        position = self._clock_position_combo.currentData() or "center"
        align = self._clock_align_combo.currentData() or "center"
        y = self._clock_y_spin.value()
        height = self._clock_height_spin.value()
        if position == "top":
            y = 24
        elif position == "center":
            y = 55
        if align == "left":
            x = 28
            width = 264
        else:
            x = 0
            width = 320
        return {
            "clock": {
                "position": position,
                "x": x,
                "y": y,
                "width": width,
                "height": height,
                "align": align,
                "font": self._clock_font_combo.currentData() or "35-Adobe-Helvetica-Bold.fnt",
                "color": "FFFFFF",
                "shadow": "soft",
                "opacity": 82,
            },
            "date": {
                "mode": self._date_mode_combo.currentData() or "below",
                "y": y + height + 14,
                "font": "16-Adobe-Helvetica-Bold.fnt",
                "color": "FFFFFF",
            },
            "readability": {
                "auto_contrast": self._auto_contrast_check.isChecked(),
                "min_contrast": 4.5,
                "sample_region": "clock_box",
            },
            "mini_player": {
                "style": "matched_blur",
                "blur_strength": self._mini_blur_combo.currentData() or "medium",
                "tint_source": "wallpaper",
                "tint_color": "2D2936",
                "text_color": "FFFFFF",
                "secondary_text_color": "C8BED7",
            },
        }

    @staticmethod
    def _set_combo_value(combo, value):
        for index in range(combo.count()):
            if combo.itemData(index) == value:
                combo.setCurrentIndex(index)
                return

    def _emit_profile_selected(self):
        profile_id = self.current_profile_id()
        if profile_id:
            self.profile_selected.emit(profile_id)

    def _emit_theme_selected(self):
        theme_id = self.current_theme_id()
        if theme_id:
            self.theme_selected.emit(theme_id)

    def _emit_apply_lock(self):
        selection = self.current_selection()
        selection["charge_source"] = ""
        self.apply_requested.emit(selection)

    def _emit_apply_charge(self):
        selection = self.current_selection()
        selection["lock_source"] = ""
        selection["pictureflow_source"] = ""
        selection["lockscreen_customization"] = {}
        self.apply_requested.emit(selection)

    def _emit_apply_pictureflow(self):
        selection = self.current_selection()
        selection["lock_source"] = ""
        selection["charge_source"] = ""
        selection["lockscreen_customization"] = {}
        self.apply_requested.emit(selection)

    def _emit_apply_lockscreen_customization(self):
        selection = self.current_selection()
        selection["charge_source"] = ""
        selection["pictureflow_source"] = ""
        self.apply_requested.emit(selection)

    def _emit_apply_both(self):
        self.apply_requested.emit(self.current_selection())

    def _emit_apply_lock_as_charge(self):
        selection = self.current_selection()
        selection["charge_source"] = selection.get("lock_source", "")
        selection["lock_source"] = ""
        selection["pictureflow_source"] = ""
        selection["lockscreen_customization"] = {}
        self.apply_requested.emit(selection)

    def _emit_apply_charge_as_lock(self):
        selection = self.current_selection()
        selection["lock_source"] = selection.get("charge_source", "")
        selection["charge_source"] = ""
        selection["pictureflow_source"] = ""
        selection["lockscreen_customization"] = {}
        self.apply_requested.emit(selection)

    def _choose_custom_lock(self):
        path, _ = QFileDialog.getOpenFileName(self, "Choose Lock Wallpaper", "", "Images (*.bmp *.png *.jpg *.jpeg)")
        if path:
            self.import_requested.emit("lock", path)

    def _choose_custom_charge(self):
        path, _ = QFileDialog.getOpenFileName(self, "Choose Charge Wallpaper", "", "Images (*.bmp *.png *.jpg *.jpeg)")
        if path:
            self.import_requested.emit("charge", path)

    def _choose_custom_pictureflow(self):
        path, _ = QFileDialog.getOpenFileName(self, "Choose PictureFlow Init Wallpaper", "", "Images (*.bmp *.png *.jpg *.jpeg)")
        if path:
            self.import_requested.emit("pictureflow", path)

    def _emit_remove_lock(self):
        candidate = self._lock_pane.current_candidate()
        if candidate:
            self.remove_requested.emit("lock", dict(candidate))

    def _emit_remove_charge(self):
        candidate = self._charge_pane.current_candidate()
        if candidate:
            self.remove_requested.emit("charge", dict(candidate))

    def _emit_remove_pictureflow(self):
        candidate = self._pictureflow_pane.current_candidate()
        if candidate:
            self.remove_requested.emit("pictureflow", dict(candidate))

    def _emit_hide_lock(self):
        candidate = self._lock_pane.current_candidate()
        if candidate:
            self.hide_requested.emit("lock", dict(candidate))

    def _emit_hide_charge(self):
        candidate = self._charge_pane.current_candidate()
        if candidate:
            self.hide_requested.emit("charge", dict(candidate))

    def _emit_hide_pictureflow(self):
        candidate = self._pictureflow_pane.current_candidate()
        if candidate:
            self.hide_requested.emit("pictureflow", dict(candidate))
