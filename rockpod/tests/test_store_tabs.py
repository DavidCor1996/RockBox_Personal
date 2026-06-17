from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from services.device_detector import DeviceDetector
from services.library_scanner import LibraryScanner
from ui.main_window import MainWindow


def test_store_contains_music_and_ipod_games_browser_tabs(config, monkeypatch):
    QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr(MainWindow, "_start_store_homepage", lambda self, force=False: None)

    window = MainWindow(config)
    try:
        assert window._store_page.tabText(0) == "Music"
        assert window._store_page.tabText(1) == "Movies"
        assert window._store_page.tabText(2) == "iPod Games"
        assert window._store_page.widget(0) is window._browser_panel
        assert window._store_page.widget(1) is window._movie_store_panel
        assert window._store_page.widget(2) is window._game_browser_panel
        assert window._store_page.widget(2) is not window._game_manager

        window._on_sidebar_selection("rockbox", "rockbox_browser")
        assert window._content_stack.currentWidget() is window._store_page
        assert window._store_page.currentWidget() is window._browser_panel

        window._store_page.setCurrentWidget(window._game_browser_panel)
        assert window._current_view == "rockbox_games"
        assert window._game_browser_panel._download_dir == config.get("games_library_path")
        assert window._game_browser_panel.current_url() == config.get("browser_home_url")
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()


def test_store_results_are_marked_owned_from_library_album(config, monkeypatch):
    QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr(MainWindow, "_start_store_homepage", lambda self, force=False: None)

    window = MainWindow(config)
    try:
        window._db.upsert_track(
            {
                "file_path": "/music/owned.flac",
                "title": "Track",
                "artist": "Owned Artist",
                "album": "Owned Album",
                "album_artist": "Owned Artist",
                "media_type": "audio",
            }
        )
        window._db.commit()

        marked = window._mark_store_results_owned(
            [
                {
                    "source": "tidal",
                    "media_type": "album",
                    "id": "123",
                    "title": "Owned Album",
                    "artist": "Owned Artist",
                    "url": "https://tidal.com/album/123",
                }
            ]
        )

        assert marked[0]["owned"] is True
        assert marked[0]["in_library"] is True
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()
