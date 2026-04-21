"""Sync confirmation dialog — structured iTunes-style summary and progress sheet."""

from PySide6.QtCore import Qt, Signal
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


class SyncDialog(QDialog):
    """Dialog showing sync plan and progress, styled like a compact iTunes sheet."""

    sync_confirmed = Signal()
    sync_cancelled = Signal()

    def __init__(self, sync_plan, parent=None):
        super().__init__(parent)
        self.setObjectName("sync_dialog")
        self.setWindowTitle("Sync to iPod")
        self.setMinimumSize(470, 292)
        self.setModal(True)
        self._plan = sync_plan

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

        layout.addStretch(1)

        footer = QWidget()
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
        self._summary_values["artwork"].setText(_format_count(len(plan.artwork_to_copy)))
        self._summary_values["up_to_date"].setText(_format_count(len(plan.up_to_date)))
        self._summary_values["orphaned"].setText(_format_count(len(plan.to_delete)))
        self._summary_values["total_data"].setText(_format_bytes(plan.total_bytes))

        if plan.errors:
            self._warning_text.setText("\n".join(f"• {err}" for err in plan.errors))
            self._warning_panel.setVisible(True)

    def _on_sync(self):
        self._title.setText("Syncing iPod")
        self._subtitle.setText("Updating your iPod")
        self._sync_btn.setVisible(False)
        self._progress.setMaximum(self._plan.total_operations or 1)
        self.sync_confirmed.emit()

    def _on_cancel(self):
        self.sync_cancelled.emit()
        self.reject()

    def update_progress(self, current, total, description):
        total = max(int(total or 0), 1)
        current = max(0, int(current or 0))
        self._progress.setMaximum(total)
        self._progress.setValue(min(current, total))

        phase = _infer_phase(description)
        self._phase_label.setText(f"Status: {phase} {min(current, total)} of {total}")
        self._item_label.setText(description or "")
        self._item_label.setVisible(bool(description))

    def show_results(self, copied, failed, skipped):
        self._title.setText("Sync Complete")
        self._subtitle.setText("Your iPod has been updated")
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
        if not parts:
            parts.append("Nothing needed to change")

        self._results.setText(", ".join(parts))
        self._results.setVisible(True)

    def show_cancelled(self):
        self._title.setText("Sync Cancelled")
        self._subtitle.setText("Your iPod was not fully updated")
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
