"""Main-window integration test for the Android image qualification page."""

from PySide6.QtWidgets import QApplication

from ui.main_window import MainWindow


def test_main_window_routes_to_android_workspace(config, monkeypatch):
    app = QApplication.instance() or QApplication([])
    monkeypatch.setattr(MainWindow, "_refresh_browser_panel", lambda self: None)
    monkeypatch.setattr(MainWindow, "_refresh_music_sharing_panel", lambda self: None)
    monkeypatch.setattr(MainWindow, "_export_shared_music_playlists", lambda self, **kwargs: None)
    window = MainWindow(config)
    try:
        window._on_sidebar_selection("rockbox", "rockbox_android")

        assert window._content_stack.currentWidget() is window._android_manager
        assert "regular-file-only" in window._android_manager._status.text()
        assert "hardware writes disabled" in window._android_manager._status.text()
    finally:
        window._device_storage_analyzer.shutdown()
        window._db.close()
        window.close()
