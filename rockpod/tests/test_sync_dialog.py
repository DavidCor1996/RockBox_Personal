from PySide6.QtWidgets import QApplication

from services.sync_engine import SyncPlan
from ui.dialogs.sync_dialog import SyncDialog


def test_sync_summary_offers_separate_repository_build_and_install_button():
    app = QApplication.instance() or QApplication([])
    del app
    dialog = SyncDialog(
        SyncPlan(),
        allow_rockbox_build_sync=True,
        rockbox_build_detail="Target: ipodvideo • Build: build-hw-ipodvideo",
    )

    assert dialog._rockbox_panel.isHidden() is False
    requested = []
    dialog.rockbox_build_sync_requested.connect(lambda: requested.append(True))
    dialog._rockbox_build_sync_btn.click()
    assert requested == [True]
    assert dialog._rockbox_build_sync_btn.isHidden() is True
    assert "Latest Rockbox" in dialog._rockbox_build_sync_btn.text()
    assert "ipodvideo" in dialog._rockbox_build_detail.text()


def test_sync_summary_hides_repository_build_option_when_unavailable():
    app = QApplication.instance() or QApplication([])
    del app
    dialog = SyncDialog(SyncPlan())

    assert dialog._rockbox_panel.isHidden() is True


def test_normal_sync_button_does_not_request_rockbox_build():
    app = QApplication.instance() or QApplication([])
    del app
    dialog = SyncDialog(SyncPlan(), allow_rockbox_build_sync=True)
    media_sync = []
    rockbox_build = []
    dialog.sync_confirmed.connect(lambda: media_sync.append(True))
    dialog.rockbox_build_sync_requested.connect(lambda: rockbox_build.append(True))

    dialog._sync_btn.click()

    assert media_sync == [True]
    assert rockbox_build == []


def test_sync_results_show_file_failure_details():
    app = QApplication.instance() or QApplication([])
    del app
    dialog = SyncDialog(SyncPlan())

    dialog.add_file_error(
        "/music/Artist/Album/Track.mp3",
        "No space left on device",
    )
    dialog.show_results(0, 1, 0)

    assert "1 failed" in dialog._results.text()
    assert "Track.mp3: No space left on device" in dialog._results.text()
