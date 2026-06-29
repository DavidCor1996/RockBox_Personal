from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from services.device_detector import DeviceDetector
from services.library_scanner import LibraryScanner
from ui.main_window import MainWindow
from ui.video_sync import VideoSyncPanel


def test_video_sync_panel_selects_missing_videos():
    QApplication.instance() or QApplication([])
    panel = VideoSyncPanel()
    panel.set_videos(
        [
            {"id": 1, "title": "On Device", "video_kind": "movie", "synced_to_device": True},
            {
                "id": 2,
                "title": "Missing",
                "video_kind": "movie",
                "video_sync_label": "Downloaded",
                "synced_to_device": False,
            },
        ]
    )

    panel.select_missing()

    assert panel.selected_track_ids() == {2}
    assert "2 videos in library" in panel._subhead.text()
    assert "1 not on iPod" in panel._subhead.text()
    assert "1 selected" in panel._subhead.text()


def test_video_sync_panel_emits_remove_and_delete_for_selected_videos():
    QApplication.instance() or QApplication([])
    panel = VideoSyncPanel()
    panel.set_videos(
        [
            {
                "id": 1,
                "title": "On Device",
                "video_kind": "movie",
                "synced_to_device": True,
                "device_path": "Videos/Downloaded/On Device.mpg",
            },
            {"id": 2, "title": "Missing", "video_kind": "movie", "synced_to_device": False},
        ]
    )
    panel.select_all()

    removed = []
    deleted = []
    panel.remove_requested.connect(lambda ids: removed.append(ids))
    panel.delete_requested.connect(lambda ids: deleted.append(ids))

    assert panel._remove_btn.isEnabled()
    assert panel._delete_btn.isEnabled()
    assert {video["id"] for video in panel.selected_videos()} == {1, 2}

    panel._remove_btn.click()
    panel._delete_btn.click()

    assert removed == [{1, 2}]
    assert deleted == [{1, 2}]


def test_video_sync_panel_emits_repair_for_selected_videos():
    QApplication.instance() or QApplication([])
    panel = VideoSyncPanel()
    panel.set_videos(
        [
            {"id": 1, "title": "Repairable", "video_kind": "movie", "synced_to_device": False},
            {"id": 2, "title": "Also Repairable", "video_kind": "movie", "synced_to_device": False},
        ]
    )
    panel.select_all()

    repaired = []
    panel.force_repair_requested.connect(lambda ids: repaired.append(ids))

    panel._repair_btn.click()

    assert repaired == [{1, 2}]


def test_video_sync_screen_routes_from_sidebar(config, monkeypatch):
    QApplication.instance() or QApplication([])
    monkeypatch.setattr(DeviceDetector, "start_polling", lambda self: None)
    monkeypatch.setattr(QTimer, "singleShot", lambda *args, **kwargs: None)
    monkeypatch.setattr(LibraryScanner, "start_scan", lambda self, force_full=False: None)
    monkeypatch.setattr(MainWindow, "_start_store_homepage", lambda self, force=False, tab_key=None: None)

    window = MainWindow(config)
    try:
        window._db.upsert_track(
            {
                "file_path": f"{config.video_dir}/Movies/movie.mpg",
                "title": "Library Movie",
                "artist": "Director",
                "album": "Movie",
                "media_type": "video",
                "video_kind": "movie",
            }
        )
        window._db.upsert_track(
            {
                "file_path": f"{config.video_dir}/YouTube/downloaded.mpg",
                "title": "YouTube",
                "artist": "YouTube",
                "album": "YouTube",
                "media_type": "video",
                "video_kind": "movie",
            }
        )
        window._db.commit()

        window._on_sidebar_selection("library", "library_video_sync")

        assert window._content_stack.currentWidget() is window._video_sync_panel
        assert window._video_sync_panel._tree.topLevelItemCount() == 1
        assert window._video_sync_panel._tree.topLevelItem(0).text(0) == "downloaded"
        assert window._video_sync_panel._tree.topLevelItem(0).text(1) == "Downloaded"
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()
