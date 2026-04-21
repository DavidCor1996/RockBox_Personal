"""iTunes 7-style status bar showing track count, total time, and size."""

import time

from PySide6.QtWidgets import QWidget, QHBoxLayout, QLabel, QSizePolicy
from PySide6.QtCore import Qt, QTimer


class StatusBar(QWidget):
    """Bottom status bar mimicking iTunes 7 info display."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("status_bar")
        self.setFixedHeight(20)
        self._scan_activity_base_text = ""
        self._scan_activity_tick = 0
        self._scan_activity_started_at = None
        self._scan_activity_timer = QTimer(self)
        self._scan_activity_timer.setInterval(250)
        self._scan_activity_timer.timeout.connect(self._advance_scan_activity)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(6, 0, 6, 0)
        layout.setSpacing(2)

        self._left_label = QLabel("")
        self._left_label.setObjectName("status_left")
        self._left_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        layout.addWidget(self._left_label)

        self._center_label = QLabel("")
        self._center_label.setObjectName("status_center")
        self._center_label.setAlignment(Qt.AlignCenter)
        self._center_label.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        layout.addWidget(self._center_label)

        self._right_label = QLabel("")
        self._right_label.setObjectName("status_right")
        self._right_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        layout.addWidget(self._right_label)

    def update_library_info(self, track_count, total_duration, total_size, item_label="songs"):
        """Update the status bar with library summary, iTunes style."""
        if not self._scan_activity_timer.isActive():
            self._center_label.setText(format_status_summary(track_count, total_duration, total_size, item_label=item_label))

    def update_selection_info(self, selected_count, total_count, total_duration, total_size, item_label="songs"):
        summary = format_status_summary(total_count, total_duration, total_size, item_label=item_label)
        if self._scan_activity_timer.isActive():
            return
        if selected_count:
            selected = "1 selected" if selected_count == 1 else f"{selected_count} selected"
            self._center_label.setText(f"{selected} — {summary}")
        else:
            self._center_label.setText(summary)

    def set_device_status(self, text):
        self._right_label.setText(text)

    def set_sync_status(self, text):
        self._right_label.setText(text)

    def clear_left_after_summary(self):
        self._left_label.setText("")

    def set_left_text(self, text):
        self._left_label.setText(text)

    def set_right_text(self, text):
        self._right_label.setText(text)

    def set_scan_progress(self, current, total, filename=""):
        self.stop_scan_activity()
        short = filename[:40] + "..." if len(filename) > 40 else filename
        self._center_label.setText(f"Refreshing: {current}/{total} — {short}")

    def set_sync_progress(self, current, total, description=""):
        self._center_label.setText(f"Syncing: {current}/{total} — {description}")

    def start_scan_activity(self, text="Refreshing library"):
        self._scan_activity_base_text = str(text or "Refreshing library")
        self._scan_activity_tick = 0
        self._scan_activity_started_at = time.monotonic()
        self._advance_scan_activity()
        self._scan_activity_timer.start()

    def stop_scan_activity(self):
        if self._scan_activity_timer.isActive():
            self._scan_activity_timer.stop()
        self._scan_activity_base_text = ""
        self._scan_activity_tick = 0
        self._scan_activity_started_at = None

    def _advance_scan_activity(self):
        spinner = "|/-\\"
        frame = spinner[self._scan_activity_tick % len(spinner)]
        self._scan_activity_tick += 1
        elapsed = ""
        if self._scan_activity_started_at is not None:
            elapsed_seconds = max(0, int(time.monotonic() - self._scan_activity_started_at))
            minutes, seconds = divmod(elapsed_seconds, 60)
            elapsed = f" {minutes:02d}:{seconds:02d}"
        self._center_label.setText(f"{self._scan_activity_base_text} {frame}{elapsed}")

    def clear(self):
        self.stop_scan_activity()
        self._left_label.setText("")
        self._center_label.setText("")
        self._right_label.setText("")


def format_status_summary(track_count, total_duration, total_size, item_label="songs"):
    """Format the iTunes-style count/time/size status string."""
    # Format duration
    hours = int(total_duration or 0) // 3600
    mins = (int(total_duration or 0) % 3600) // 60

    if hours > 0:
        time_str = f"{hours}.{mins:02d} hours"
    else:
        time_str = f"{mins} minutes"

    # Format size
    gb = (total_size or 0) / (1024 ** 3)
    if gb >= 1.0:
        size_str = f"{gb:.2f} GB"
    else:
        mb = (total_size or 0) / (1024 ** 2)
        size_str = f"{mb:.1f} MB"

    singular = {
        "songs": "song",
        "videos": "video",
        "items": "item",
    }.get(item_label, item_label[:-1] if item_label.endswith("s") else item_label)
    label = singular if track_count == 1 else item_label
    return f"{track_count} {label}, {time_str}, {size_str}"
