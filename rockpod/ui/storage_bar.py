"""Classic iTunes-style multi-segment device capacity bar."""

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QLinearGradient, QPainter, QPainterPath
from PySide6.QtWidgets import QGridLayout, QLabel, QWidget, QVBoxLayout


SEGMENT_ORDER = (
    ("music", "Music"),
    ("games_plugins", "Games"),
    ("themes_assets", "Themes"),
    ("rockbox_system", "System"),
    ("linux_system", "Linux"),
    ("other", "Other"),
    ("free", "Free"),
)

SEGMENT_COLORS = {
    "music": ("#6aaee8", "#3a78c0"),
    "games_plugins": ("#88c676", "#5c9d47"),
    "themes_assets": ("#d58bb8", "#a95f8d"),
    "rockbox_system": ("#f0c060", "#d89028"),
    "linux_system": ("#70c8c4", "#2d8c89"),
    "other": ("#a7adb8", "#7f8794"),
    "free": ("#efefef", "#d7d7d7"),
}


def format_bytes(num_bytes):
    gb = (num_bytes or 0) / (1024 ** 3)
    if gb >= 1.0:
        return f"{gb:.2f} GB"
    mb = (num_bytes or 0) / (1024 ** 2)
    return f"{mb:.0f} MB"


def storage_segments(storage_or_total, music_bytes=None, other_bytes=None):
    if isinstance(storage_or_total, dict):
        storage = storage_or_total
    else:
        total = max(int(storage_or_total or 0), 0)
        music = max(int(music_bytes or 0), 0)
        other = max(int(other_bytes or 0), 0)
        free = max(total - music - other, 0)
        storage = {
            "total": total,
            "music": music,
            "other": other,
            "free": free,
        }
    total = max(int((storage or {}).get("total") or 0), 0)
    segments = []
    for key, label in SEGMENT_ORDER:
        amount = max(int((storage or {}).get(key) or 0), 0)
        if key != "free" and amount <= 0:
            continue
        segments.append({"key": key, "label": label, "name": label, "bytes": amount, "total": total})
    if not any(segment["key"] == "free" for segment in segments):
        segments.append({"key": "free", "label": "Free", "name": "Free", "bytes": max(total - sum(s["bytes"] for s in segments), 0), "total": total})
    return segments


class StorageBarWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(14)
        self.setMaximumHeight(14)
        self._segments = []
        self._total = 0

    def set_storage(self, storage):
        self._segments = storage_segments(storage)
        self._total = max(int((storage or {}).get("total") or 0), 1)
        self.update()

    def paintEvent(self, event):
        if self._total <= 0:
            return
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        x = 1
        y = 1
        w = self.width() - 2
        h = self.height() - 2

        base = QLinearGradient(x, y, x, y + h)
        base.setColorAt(0, QColor("#f4f4f4"))
        base.setColorAt(1, QColor("#dddddd"))
        painter.setPen(QColor("#8d8d8d"))
        painter.setBrush(base)
        outline = QPainterPath()
        outline.addRoundedRect(x, y, w, h, 3, 3)
        painter.drawPath(outline)
        painter.setClipPath(outline)

        cursor = x
        remaining_width = w
        non_zero = [segment for segment in self._segments if segment["bytes"] > 0]
        for index, segment in enumerate(non_zero):
            if index == len(non_zero) - 1:
                seg_width = remaining_width
            else:
                seg_width = max(int((segment["bytes"] / float(self._total)) * w), 0)
            if seg_width <= 0:
                continue
            top, bottom = SEGMENT_COLORS.get(segment["key"], SEGMENT_COLORS["other"])
            grad = QLinearGradient(cursor, y, cursor, y + h)
            grad.setColorAt(0, QColor(top))
            grad.setColorAt(1, QColor(bottom))
            painter.fillRect(cursor, y, seg_width, h, grad)
            cursor += seg_width
            remaining_width = max((x + w) - cursor, 0)

        gloss = QLinearGradient(x, y, x, y + max(1, h // 2))
        gloss.setColorAt(0, QColor(255, 255, 255, 125))
        gloss.setColorAt(1, QColor(255, 255, 255, 0))
        painter.fillRect(x + 1, y + 1, max(0, w - 2), max(0, h // 2), gloss)
        painter.setClipping(False)
        painter.setPen(QColor("#8d8d8d"))
        painter.setBrush(Qt.NoBrush)
        painter.drawPath(outline)


class StorageBar(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("storage_bar_container")
        self.setVisible(False)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 3, 8, 3)
        layout.setSpacing(3)

        self._bar = StorageBarWidget()
        layout.addWidget(self._bar)

        self._legend = QGridLayout()
        self._legend.setHorizontalSpacing(8)
        self._legend.setVerticalSpacing(1)
        self._labels = {}
        for index, (key, label) in enumerate(SEGMENT_ORDER):
            widget = QLabel()
            widget.setObjectName("storage_label")
            self._labels[key] = widget
            row = index // 3
            col = index % 3
            self._legend.addWidget(widget, row, col)
        layout.addLayout(self._legend)

    def update_storage(self, storage, device_name="iPod"):
        total = int((storage or {}).get("total") or 0)
        if total <= 0:
            self.setVisible(False)
            return
        self._bar.set_storage(storage)
        for key, label in SEGMENT_ORDER:
            amount = int((storage or {}).get(key) or 0)
            self._labels[key].setText(f"{label}: {format_bytes(amount)}")
        self.setVisible(True)

    def hide_bar(self):
        self.setVisible(False)
