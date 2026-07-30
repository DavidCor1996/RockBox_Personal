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

    assert panel.selected_track_ids() == {1, 2}
    assert panel.syncable_track_ids() == {2}
    assert "2 videos in library" in panel._subhead.text()
    assert "1 not on iPod" in panel._subhead.text()
    assert "2 selected; 1 new to sync" in panel._subhead.text()


def test_video_sync_panel_prechecks_existing_and_emits_distinct_formats():
    QApplication.instance() or QApplication([])
    panel = VideoSyncPanel()
    panel.set_videos(
        [
            {
                "id": 1,
                "title": "Already Synced",
                "synced_to_device": False,
                "device_path": "Videos/Already Synced.mpg",
            },
            {"id": 2, "title": "New Video", "synced_to_device": False},
        ]
    )
    panel.select_all()

    requests = []
    previews = []
    panel.sync_requested.connect(
        lambda ids, profile: requests.append((ids, profile))
    )
    panel.preview_requested.connect(lambda ids: previews.append(ids))

    panel._preview_btn.click()
    panel._sync_rvp_btn.click()
    panel._sync_mpeg_btn.click()

    assert panel.selected_track_ids() == {1, 2}
    assert requests == [
        ({2}, "native_raw"),
        ({2}, "quality"),
    ]
    assert previews == [{2}]
    assert not hasattr(panel, "_repair_btn")


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


def test_video_sync_panel_emits_hide_and_lock_and_shows_privacy_status():
    QApplication.instance() or QApplication([])
    panel = VideoSyncPanel()
    panel.set_videos(
        [
            {
                "id": 7,
                "title": "Private",
                "video_kind": "home_video",
                "video_hidden": 1,
                "video_locked": 1,
            }
        ]
    )
    panel.select_all()
    hidden = []
    locked = []
    panel.hide_requested.connect(lambda ids: hidden.append(ids))
    panel.lock_requested.connect(lambda ids: locked.append(ids))

    assert panel._tree.topLevelItem(0).text(2).endswith("Hidden · Locked")
    assert panel._hide_btn.text() == "Unhide Selected"
    assert panel._lock_btn.text() == "Move to Normal iPod Folder"
    panel._hide_btn.click()
    panel._lock_btn.click()
    assert hidden == [{7}]
    assert locked == [{7}]


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
        window._db.upsert_track(
            {
                "file_path": f"{config.video_dir}/private.mpg",
                "title": "Private Video",
                "media_type": "video",
                "video_kind": "home_video",
                "video_hidden": 1,
                "video_locked": 1,
            }
        )
        window._db.commit()

        window._on_sidebar_selection("library", "library_video_sync")

        assert window._content_stack.currentWidget() is window._video_sync_panel
        assert window._video_sync_panel._tree.topLevelItemCount() == 2
        rows = {
            window._video_sync_panel._tree.topLevelItem(index).text(0):
            window._video_sync_panel._tree.topLevelItem(index).text(1)
            for index in range(window._video_sync_panel._tree.topLevelItemCount())
        }
        assert rows == {
            "Library Movie": "Movies",
            "downloaded": "Downloaded",
        }

        window._show_hidden_wallpapers = True
        window._refresh_video_sync_panel()
        assert window._video_sync_panel._tree.topLevelItemCount() == 3
        private_rows = [
            window._video_sync_panel._tree.topLevelItem(index)
            for index in range(window._video_sync_panel._tree.topLevelItemCount())
            if window._video_sync_panel._tree.topLevelItem(index).text(0) == "Private Video"
        ]
        assert private_rows[0].text(2).endswith("Hidden · Locked")
    finally:
        window._device_storage_analyzer.shutdown()
        window.close()
