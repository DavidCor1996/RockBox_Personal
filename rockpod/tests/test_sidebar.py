"""Sidebar playlist behavior tests."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from PySide6.QtWidgets import QApplication

from ui.sidebar import Sidebar


def test_sidebar_playlist_selection_emits_playlist_id():
    app = QApplication.instance() or QApplication([])
    sidebar = Sidebar()
    seen = []
    sidebar.item_selected.connect(lambda category, item_id: seen.append((category, item_id)))

    sidebar.add_playlist("Road Mix", 42)
    sidebar.select_item("playlist_42")
    sidebar.item_selected.emit("playlist", "playlist_42")

    assert seen == [("playlist", "playlist_42")]


def test_sidebar_playlist_count_label_updates():
    app = QApplication.instance() or QApplication([])
    sidebar = Sidebar()
    sidebar.add_playlist("Road Mix", 42)
    sidebar.set_playlist_label(42, "Road Mix", 7)

    item = sidebar._find_item("playlist_42")
    assert item.text(0) == "Road Mix (7)"


def test_sidebar_not_on_ipod_label_can_show_pending_state():
    app = QApplication.instance() or QApplication([])
    sidebar = Sidebar()

    sidebar.update_not_on_ipod_count(None)

    item = sidebar._find_item("device_not_on_ipod")
    assert item.text(0) == "Not on iPod"
    assert item.toolTip(0) == "Verifying device inventory"
