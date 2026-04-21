"""Tests for status-bar feedback states."""

from PySide6.QtWidgets import QApplication

from ui.status_bar import StatusBar


def test_scan_activity_animates_until_progress():
    app = QApplication.instance() or QApplication([])
    bar = StatusBar()
    try:
        bar.start_scan_activity("Refreshing library")
        first = bar._center_label.text()
        bar._advance_scan_activity()
        second = bar._center_label.text()

        assert first.startswith("Refreshing library ")
        assert second.startswith("Refreshing library ")
        assert first != second

        bar.set_scan_progress(3, 10, "Album/Track.mp3")
        assert bar._scan_activity_timer.isActive() is False
        assert bar._center_label.text() == "Refreshing: 3/10 — Album/Track.mp3"
    finally:
        bar.deleteLater()


def test_stop_scan_activity_clears_timer():
    app = QApplication.instance() or QApplication([])
    bar = StatusBar()
    try:
        bar.start_scan_activity("Scanning library")
        assert bar._scan_activity_timer.isActive() is True

        bar.stop_scan_activity()

        assert bar._scan_activity_timer.isActive() is False
    finally:
        bar.deleteLater()
