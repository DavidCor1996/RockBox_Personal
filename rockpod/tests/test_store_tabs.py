from types import SimpleNamespace

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from services.device_detector import DeviceDetector
from services.library_scanner import LibraryScanner
from ui.main_window import MainWindow


class _FakeSignal:
    def __init__(self):
        self.connected = []

    def connect(self, callback):
        self.connected.append(callback)


class _FakeProcess:
    def __init__(self, parent=None):
        self.parent = parent
        self.readyReadStandardOutput = _FakeSignal()
        self.readyReadStandardError = _FakeSignal()
        self.finished = _FakeSignal()
        self.errorOccurred = _FakeSignal()
        self.environment = None
        self.working_directory = ""
        self.started = None

    def setProcessEnvironment(self, env):
        self.environment = env

    def setWorkingDirectory(self, path):
        self.working_directory = path

    def start(self, program, args):
        self.started = (program, list(args))


def test_store_contains_music_and_ipod_games_browser_tabs(config, monkeypatch):
    QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr(MainWindow, "_start_store_homepage", lambda self, force=False: None)

    window = MainWindow(config)
    try:
        assert window._store_page.tabText(0) == "Music"
        assert window._store_page.tabText(1) == "Sharing"
        assert window._store_page.tabText(2) == "Movies"
        assert window._store_page.tabText(3) == "Live TV"
        assert window._store_page.tabText(4) == "Calm"
        assert window._store_page.tabText(5) == "iPod Games"
        assert window._store_page.widget(0) is window._browser_panel
        assert window._store_page.widget(1) is window._music_sharing_panel
        assert window._store_page.widget(2) is window._movie_store_panel
        assert window._store_page.widget(3) is window._livetv_store_panel
        assert window._store_page.widget(4) is window._calm_store_panel
        assert window._store_page.widget(5) is window._game_browser_panel
        assert window._store_page.widget(5) is not window._game_manager

        window._on_sidebar_selection("rockbox", "rockbox_browser")
        assert window._content_stack.currentWidget() is window._store_page
        assert window._store_page.currentWidget() is window._browser_panel

        window._on_sidebar_selection("rockbox", "rockbox_game_sync")
        assert window._content_stack.currentWidget() is window._game_manager

        window._on_sidebar_selection("rockbox", "rockbox_achievements_avatar")
        assert window._content_stack.currentWidget() is window._avatar_editor

        window._on_sidebar_selection("rockbox", "rockbox_browser")
        window._store_page.setCurrentWidget(window._game_browser_panel)
        assert window._current_view == "rockbox_games"
        assert window._game_browser_panel._download_dir == config.get("games_library_path")
        assert window._game_browser_panel.current_url() == config.get("browser_home_url")

        window._store_page.setCurrentWidget(window._music_sharing_panel)
        assert window._current_view == "rockbox_sharing"
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()


def test_main_window_child_process_helper_configures_environment(config, monkeypatch):
    QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr(MainWindow, "_start_store_homepage", lambda self, force=False: None)
    monkeypatch.setattr("ui.process_helpers.QProcess", _FakeProcess)

    window = MainWindow(config)
    try:
        request = SimpleNamespace(command=["tool", "--flag"], env={"ROCKPOD_TEST": "1"})

        process = window._create_child_process(
            request,
            "/tmp/work",
            window._on_store_search_output,
            window._on_store_search_finished,
            window._on_store_search_error,
        )
        process.start(request.command[0], request.command[1:])

        assert process.working_directory == "/tmp/work"
        assert process.environment.value("ROCKPOD_TEST") == "1"
        assert process.readyReadStandardOutput.connected == [window._on_store_search_output]
        assert process.readyReadStandardError.connected == [window._on_store_search_output]
        assert process.finished.connected == [window._on_store_search_finished]
        assert process.errorOccurred.connected == [window._on_store_search_error]
        assert process.started == ("tool", ["--flag"])
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


def test_spotify_playlist_store_import_creates_standard_playlist(config, monkeypatch):
    QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr(MainWindow, "_start_store_homepage", lambda self, force=False: None)

    window = MainWindow(config)
    try:
        first = f"{config.music_dir}/Playlist Name/01. One.flac"
        second = f"{config.music_dir}/Playlist Name/02. Two.flac"
        for path, title in ((first, "One"), (second, "Two")):
            window._db.upsert_track(
                {
                    "file_path": path,
                    "title": title,
                    "artist": "Artist",
                    "album": "Playlist Name",
                    "album_artist": "Artist",
                    "media_type": "audio",
                }
            )
        window._db.commit()
        window._pending_store_playlist_import = {
            "context": {
                "url": "https://open.spotify.com/playlist/37i9dQZF1DXcBWIGoYBM5M",
                "url_info": {
                    "source": "spotify",
                    "media_type": "playlist",
                    "id": "37i9dQZF1DXcBWIGoYBM5M",
                },
                "store_result": {"title": "Playlist Name"},
            },
            "imported_files": [first, second],
        }

        window._finalize_pending_store_playlist_import()

        playlists = [row for row in window._db.get_all_playlists() if row["name"] == "Playlist Name"]
        assert len(playlists) == 1
        playlist = playlists[0]
        tracks = window._db.get_playlist_tracks(playlist["id"])
        assert [row["title"] for row in tracks] == ["One", "Two"]
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()
