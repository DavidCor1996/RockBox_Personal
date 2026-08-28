"""Sync confirmation dialog — structured iTunes-style summary and progress sheet."""

import os

from PySide6.QtCore import QRectF, Qt, QTimer, Signal
from PySide6.QtGui import QColor, QFont, QLinearGradient, QPainter, QPen
from PySide6.QtWidgets import (
    QDialog,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QProgressBar,
    QVBoxLayout,
    QWidget,
)

from ui.ipod_art import draw_plugged_ipod


class SyncAnimationWidget(QWidget):
    """Small iTunes-style device well shown while a sync is running."""

    def __init__(self, parent=None, asset_path=""):
        super().__init__(parent)
        self.setObjectName("sync_animation_widget")
        self.setMinimumHeight(82)
        self.setMaximumHeight(94)
        self._asset_path = asset_path
        self._frame = 0
        self._active = False
        self._complete = False
        self._cancelled = False
        self._progress = 0.0
        self._timer = QTimer(self)
        self._timer.setInterval(70)
        self._timer.timeout.connect(self._tick)

    def start(self):
        self._active = True
        self._complete = False
        self._cancelled = False
        self._frame = 0
        self._timer.start()
        self.update()

    def set_progress(self, current, total):
        if total:
            self._progress = max(0.0, min(float(current) / float(total), 1.0))
        self.update()

    def finish(self, cancelled=False):
        self._active = False
        self._cancelled = bool(cancelled)
        self._complete = not cancelled
        self._timer.stop()
        self.update()

    def _tick(self):
        self._frame = (self._frame + 1) % 60
        self.update()

    def paintEvent(self, event):
        del event
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing, True)

        rect = QRectF(0.5, 0.5, self.width() - 1, self.height() - 1)
        bg = QLinearGradient(rect.topLeft(), rect.bottomLeft())
        bg.setColorAt(0.0, QColor("#f7f7f7"))
        bg.setColorAt(0.16, QColor("#e6e6e6"))
        bg.setColorAt(0.48, QColor("#d0d0d0"))
        bg.setColorAt(0.49, QColor("#b9b9b9"))
        bg.setColorAt(1.0, QColor("#dddddd"))
        painter.setPen(QPen(QColor("#7f7f7f"), 1))
        painter.setBrush(bg)
        painter.drawRoundedRect(rect, 4, 4)

        center_x = self.width() / 2
        center_y = self.height() / 2 + 2
        line_y = center_y - 2

        lcd = QRectF(center_x - 132, center_y - 18, 264, 31)
        lcd_grad = QLinearGradient(lcd.topLeft(), lcd.bottomLeft())
        lcd_grad.setColorAt(0.0, QColor("#fbfff4"))
        lcd_grad.setColorAt(0.22, QColor("#edf5dc"))
        lcd_grad.setColorAt(0.50, QColor("#dce9c4"))
        lcd_grad.setColorAt(0.51, QColor("#c7d5ab"))
        lcd_grad.setColorAt(1.0, QColor("#edf5da"))
        painter.setPen(QPen(QColor("#7b836d"), 1))
        painter.setBrush(lcd_grad)
        painter.drawRoundedRect(lcd, 11, 11)

        lcd_gloss = QRectF(lcd.left() + 5, lcd.top() + 3, lcd.width() - 10, 9)
        painter.setPen(Qt.NoPen)
        painter.setBrush(QColor(255, 255, 255, 62))
        painter.drawRoundedRect(lcd_gloss, 5, 5)

        self._draw_transfer_notes(painter, center_x, line_y)
        self._draw_ipod(painter, center_x, center_y)

    def _draw_transfer_notes(self, painter, center_x, line_y):
        if self._complete:
            colors = [QColor("#5f944a"), QColor("#77ad5e"), QColor("#8bbf73")]
            positions = [center_x - 38, center_x, center_x + 38]
            notes = ["♪", "♫", "♪"]
        elif self._cancelled:
            colors = [QColor("#b8b8b8"), QColor("#a8a8a8"), QColor("#989898")]
            positions = [center_x - 32, center_x, center_x + 32]
            notes = ["♪", "♫", "♪"]
        elif self._active:
            colors = [QColor("#6f879e"), QColor("#486d94"), QColor("#2f557f")]
            lead = (self._frame * 5) % 190
            positions = [
                center_x - 126 + lead,
                center_x - 96 + lead,
                center_x - 66 + lead,
                center_x - 36 + lead,
                center_x - 6 + lead,
            ]
            notes = ["♪", "♫", "♬", "♪", "♫"]
        else:
            colors = [QColor("#aeb5bd"), QColor("#9fa8b1"), QColor("#929ca7")]
            positions = [center_x - 42, center_x, center_x + 42]
            notes = ["♪", "♫", "♪"]

        font = QFont("Lucida Grande")
        font.setPixelSize(18)
        font.setBold(True)
        painter.setFont(font)
        for index, x in enumerate(positions):
            if x < center_x - 108 or x > center_x + 108:
                continue
            if center_x - 36 < x < center_x + 36:
                continue
            color = QColor(colors[index % len(colors)])
            if self._active:
                color.setAlpha(145 + min(index, 3) * 28)
            y_offset = -3 if index % 2 else 2
            note_rect = QRectF(x - 10, line_y - 14 + y_offset, 20, 22)
            painter.setPen(QColor(255, 255, 255, 96))
            painter.drawText(note_rect.translated(0, 1), Qt.AlignCenter, notes[index % len(notes)])
            painter.setPen(color)
            painter.drawText(
                note_rect,
                Qt.AlignCenter,
                notes[index % len(notes)],
            )

    def _draw_ipod(self, painter, center_x, center_y):
        draw_plugged_ipod(
            painter,
            QRectF(center_x - 35, center_y - 39, 70, 78),
            connected=True,
            asset_path=self._asset_path,
        )

class SyncDialog(QDialog):
    """Dialog showing sync plan and progress, styled like a compact iTunes sheet."""

    sync_confirmed = Signal()
    sync_cancelled = Signal()
    rockbox_build_sync_requested = Signal()

    def __init__(
        self,
        sync_plan,
        parent=None,
        allow_rockbox_build_sync=False,
        rockbox_build_detail="",
    ):
        super().__init__(parent)
        self.setObjectName("sync_dialog")
        self.setWindowTitle("Sync to iPod")
        self.setMinimumSize(470, 370)
        self.setModal(True)
        self._plan = sync_plan
        self._rockbox_sync_result = None
        self._file_errors = []

        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 14, 14, 14)
        layout.setSpacing(10)

        header = QFrame()
        header.setObjectName("sync_dialog_header")
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(12, 9, 12, 9)
        header_layout.setSpacing(10)

        self._icon = QLabel("⇄")
        self._icon.setObjectName("sync_dialog_icon")
        self._icon.setAlignment(Qt.AlignCenter)
        self._icon.setFixedSize(28, 28)
        header_layout.addWidget(self._icon, 0, Qt.AlignTop)

        title_col = QVBoxLayout()
        title_col.setContentsMargins(0, 0, 0, 0)
        title_col.setSpacing(1)
        self._title = QLabel("Sync Summary")
        self._title.setObjectName("sync_dialog_title")
        self._subtitle = QLabel("Preparing to update your iPod")
        self._subtitle.setObjectName("sync_dialog_subtitle")
        title_col.addWidget(self._title)
        title_col.addWidget(self._subtitle)
        header_layout.addLayout(title_col, 1)
        layout.addWidget(header)

        summary_panel = QFrame()
        summary_panel.setObjectName("sync_summary_panel")
        summary_layout = QGridLayout(summary_panel)
        summary_layout.setContentsMargins(12, 10, 12, 10)
        summary_layout.setHorizontalSpacing(18)
        summary_layout.setVerticalSpacing(5)
        self._summary_values = {}

        rows = [
            ("Tracks to copy", "tracks_copy"),
            ("Tracks to update", "tracks_update"),
            ("Album covers to sync", "artwork"),
            ("Already up to date", "up_to_date"),
            ("Orphaned tracks on iPod", "orphaned"),
            ("Total data to transfer", "total_data"),
        ]
        for row, (label_text, key) in enumerate(rows):
            label = QLabel(label_text)
            label.setObjectName("sync_summary_label")
            value = QLabel("")
            value.setObjectName("sync_summary_value")
            value.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
            summary_layout.addWidget(label, row, 0)
            summary_layout.addWidget(value, row, 1)
            self._summary_values[key] = value
        layout.addWidget(summary_panel)

        self._rockbox_panel = QFrame()
        self._rockbox_panel.setObjectName("sync_rockbox_panel")
        rockbox_layout = QVBoxLayout(self._rockbox_panel)
        rockbox_layout.setContentsMargins(12, 8, 12, 8)
        rockbox_layout.setSpacing(2)
        self._rockbox_build_sync_btn = QPushButton(
            "Build & Install Latest Rockbox + Plugins"
        )
        self._rockbox_build_sync_btn.setObjectName("sync_rockbox_build_button")
        self._rockbox_build_sync_btn.setToolTip(
            "Builds the detected iPod target, installs all enabled plugins, and "
            "verifies both rockbox.ipod copies. Music and videos are not changed."
        )
        self._rockbox_build_sync_btn.clicked.connect(self._on_rockbox_build_sync)
        rockbox_layout.addWidget(self._rockbox_build_sync_btn, 0, Qt.AlignLeft)
        self._rockbox_build_detail = QLabel(rockbox_build_detail or "")
        self._rockbox_build_detail.setObjectName("sync_rockbox_build_detail")
        self._rockbox_build_detail.setWordWrap(True)
        self._rockbox_build_detail.setVisible(bool(rockbox_build_detail))
        rockbox_layout.addWidget(self._rockbox_build_detail)
        self._rockbox_panel.setVisible(bool(allow_rockbox_build_sync))
        layout.addWidget(self._rockbox_panel)

        self._warning_panel = QFrame()
        self._warning_panel.setObjectName("sync_warning_panel")
        warning_layout = QVBoxLayout(self._warning_panel)
        warning_layout.setContentsMargins(12, 8, 12, 8)
        warning_layout.setSpacing(3)
        warning_title = QLabel("Warnings")
        warning_title.setObjectName("sync_warning_title")
        warning_layout.addWidget(warning_title)
        self._warning_text = QLabel("")
        self._warning_text.setObjectName("sync_warning_text")
        self._warning_text.setWordWrap(True)
        warning_layout.addWidget(self._warning_text)
        self._warning_panel.setVisible(False)
        layout.addWidget(self._warning_panel)

        progress_panel = QFrame()
        progress_panel.setObjectName("sync_progress_panel")
        progress_layout = QVBoxLayout(progress_panel)
        progress_layout.setContentsMargins(12, 10, 12, 10)
        progress_layout.setSpacing(5)

        self._phase_label = QLabel("Status: Ready to sync")
        self._phase_label.setObjectName("sync_status_label")
        progress_layout.addWidget(self._phase_label)

        self._progress = QProgressBar()
        self._progress.setObjectName("sync_progress_bar")
        self._progress.setTextVisible(True)
        self._progress.setFormat("%p%")
        self._progress.setMaximum(self._plan.total_operations or 1)
        self._progress.setValue(0)
        progress_layout.addWidget(self._progress)

        self._item_label = QLabel("")
        self._item_label.setObjectName("sync_item_label")
        self._item_label.setVisible(False)
        progress_layout.addWidget(self._item_label)

        self._results = QLabel("")
        self._results.setObjectName("sync_results_label")
        self._results.setVisible(False)
        self._results.setWordWrap(True)
        progress_layout.addWidget(self._results)
        layout.addWidget(progress_panel)

        asset_path = ""
        parent_theme = getattr(parent, "_theme_assets", None)
        if parent_theme is not None:
            asset_path = parent_theme.asset_path("device_plugged_ipod")
        self._animation = SyncAnimationWidget(asset_path=asset_path)
        layout.addWidget(self._animation)

        layout.addStretch(1)

        footer = QWidget()
        footer.setObjectName("sync_dialog_footer")
        footer_layout = QHBoxLayout(footer)
        footer_layout.setContentsMargins(0, 0, 0, 0)
        footer_layout.setSpacing(6)
        footer_layout.addStretch(1)

        self._sync_btn = QPushButton("Sync")
        self._sync_btn.setObjectName("sync_dialog_primary_button")
        self._cancel_btn = QPushButton("Cancel")
        self._cancel_btn.setObjectName("sync_dialog_button")
        self._close_btn = QPushButton("Close")
        self._close_btn.setObjectName("sync_dialog_button")
        self._close_btn.setVisible(False)

        self._sync_btn.clicked.connect(self._on_sync)
        self._cancel_btn.clicked.connect(self._on_cancel)
        self._close_btn.clicked.connect(self.accept)

        footer_layout.addWidget(self._sync_btn)
        footer_layout.addWidget(self._cancel_btn)
        footer_layout.addWidget(self._close_btn)
        layout.addWidget(footer)

        self._populate_summary()

    def _populate_summary(self):
        plan = self._plan
        self._summary_values["tracks_copy"].setText(_format_count(len(plan.to_copy)))
        self._summary_values["tracks_update"].setText(_format_count(len(plan.to_resync)))
        generated_count = len(getattr(plan, "generated_to_copy", []) or [])
        self._summary_values["artwork"].setText(
            _format_count(len(plan.artwork_to_copy) + generated_count)
        )
        self._summary_values["up_to_date"].setText(_format_count(len(plan.up_to_date)))
        self._summary_values["orphaned"].setText(_format_count(len(plan.to_delete)))
        self._summary_values["total_data"].setText(_format_bytes(plan.total_bytes))

        warnings = list(getattr(plan, "warnings", []) or [])
        errors = list(getattr(plan, "errors", []) or [])
        notes = [f"Warning: {item}" for item in warnings if str(item).strip()]
        notes.extend(item for item in errors if str(item).strip())

        if notes:
            self._warning_text.setText("\n".join(notes))
            self._warning_panel.setVisible(True)

    def _on_sync(self):
        self._title.setText("Syncing iPod")
        self._subtitle.setText("Updating your iPod")
        self._sync_btn.setVisible(False)
        self._progress.setMaximum(self._plan.total_operations or 1)
        self._animation.start()
        self.sync_confirmed.emit()

    def _on_rockbox_build_sync(self):
        self._title.setText("Updating Rockbox")
        self._subtitle.setText("Building firmware and plugins from this repository")
        self._rockbox_build_sync_btn.setVisible(False)
        self._sync_btn.setVisible(False)
        self._cancel_btn.setEnabled(False)
        self._cancel_btn.setToolTip(
            "The Rockbox build/install phase cannot be interrupted safely."
        )
        self._progress.setMaximum(4)
        self._progress.setValue(0)
        self._animation.start()
        self.rockbox_build_sync_requested.emit()

    def set_rockbox_sync_result(self, result):
        self._rockbox_sync_result = dict(result or {})

    def _on_cancel(self):
        self.sync_cancelled.emit()
        self.reject()

    def update_progress(self, current, total, description):
        total = max(int(total or 0), 1)
        current = max(0, int(current or 0))
        self._progress.setMaximum(total)
        self._progress.setValue(min(current, total))
        self._animation.set_progress(min(current, total), total)

        phase = _infer_phase(description)
        self._phase_label.setText(f"Status: {phase} {min(current, total)} of {total}")
        self._item_label.setText(description or "")
        self._item_label.setVisible(bool(description))

    def add_file_error(self, path, message):
        name = os.path.basename(str(path or "")) or str(path or "file")
        self._file_errors.append(f"{name}: {message}")

    def show_results(self, copied, failed, skipped):
        self._title.setText("Sync Complete")
        self._subtitle.setText("Your iPod has been updated")
        self._animation.set_progress(1, 1)
        self._animation.finish()
        self._phase_label.setText("Status: Finished")
        self._item_label.setVisible(False)
        self._sync_btn.setVisible(False)
        self._cancel_btn.setVisible(False)
        self._close_btn.setVisible(True)

        parts = []
        if copied > 0:
            parts.append(f"{_format_count(copied)} copied")
        if failed > 0:
            parts.append(f"{_format_count(failed)} failed")
        if skipped > 0:
            parts.append(f"{_format_count(skipped)} skipped")
        rockbox_result = self._rockbox_sync_result or {}
        if rockbox_result.get("success"):
            plugin_count = int(rockbox_result.get("plugin_count") or 0)
            parts.append(
                f"Rockbox firmware + {plugin_count:,} plugins updated"
            )
        if not parts:
            parts.append("Nothing needed to change")

        if self._file_errors:
            details = "\n".join(self._file_errors[:5])
            if len(self._file_errors) > 5:
                details += f"\n…and {len(self._file_errors) - 5} more"
            parts.append(details)

        self._results.setText("\n".join(parts))
        self._results.setVisible(True)

    def show_error(self, message):
        self._title.setText("Sync Failed")
        self._subtitle.setText("RockPod did not complete the requested update")
        self._animation.finish(cancelled=True)
        self._phase_label.setText("Status: Failed")
        self._item_label.setVisible(False)
        self._sync_btn.setVisible(False)
        self._cancel_btn.setVisible(False)
        self._close_btn.setVisible(True)
        self._results.setText(str(message or "The sync operation failed."))
        self._results.setVisible(True)

    def show_cancelled(self):
        self._title.setText("Sync Cancelled")
        self._subtitle.setText("Your iPod was not fully updated")
        self._animation.finish(cancelled=True)
        self._phase_label.setText("Status: Cancelled")
        self._item_label.setVisible(False)
        self._sync_btn.setVisible(False)
        self._cancel_btn.setVisible(False)
        self._close_btn.setVisible(True)
        self._results.setText("The sync operation was cancelled.")
        self._results.setVisible(True)


def _format_count(value):
    return f"{int(value or 0):,}"


def _format_bytes(value):
    size = float(value or 0)
    if size >= 1024 ** 3:
        return f"{size / (1024 ** 3):.2f} GB"
    if size >= 1024 ** 2:
        return f"{size / (1024 ** 2):.1f} MB"
    if size >= 1024:
        return f"{size / 1024:.1f} KB"
    return f"{int(size)} B"


def _infer_phase(description):
    text = (description or "").lower()
    if "artwork" in text or "cover" in text:
        return "Syncing artwork"
    if "final" in text or "finish" in text:
        return "Finalizing"
    if "copy" in text or "syncing:" in text or "track" in text:
        return "Copying tracks"
    return "Planning"
