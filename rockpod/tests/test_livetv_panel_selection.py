"""The Live TV Shows/Commercials tree: Rename and Split/Trim must act on
exactly the row the user meant.

Qt's QTreeWidget.selectedItems() is not click order, so with more than one
row selected, silently taking the first path can rename or edit a
completely different recording than the one the user intended. This is how
a rename meant for a batch of WWE episodes once landed on an unrelated
Game Show Network recording (its titles.json override read "WWE Monday
Night Raw"), which the device guide then displayed under the wrong
channel.
"""

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication, QTreeWidgetItem

from ui.livetv_panel import LiveTvPanel


def _application():
    return QApplication.instance() or QApplication([])


def _add_row(tree, path):
    item = QTreeWidgetItem([path])
    item.setData(0, Qt.UserRole, path)
    tree.addTopLevelItem(item)
    return item


def test_rename_and_split_trim_no_op_when_selection_is_ambiguous():
    _application()
    panel = LiveTvPanel()
    panel._tab = "shows"

    row_a = _add_row(panel._show_tree, "/live/show-a.mp4")
    row_b = _add_row(panel._show_tree, "/live/show-b.mp4")

    row_a.setSelected(True)
    row_b.setSelected(True)

    rename_calls = []
    edit_calls = []
    panel.rename_requested.connect(lambda kind, path: rename_calls.append(path))
    panel.edit_media_requested.connect(lambda kind, path: edit_calls.append(path))

    panel._emit_rename()
    panel._emit_edit_media()

    assert rename_calls == []
    assert edit_calls == []

    row_a.setSelected(False)

    panel._emit_rename()
    panel._emit_edit_media()

    assert rename_calls == ["/live/show-b.mp4"]
    assert edit_calls == ["/live/show-b.mp4"]


def test_delete_media_button_emits_all_selected_paths():
    """Unlike Rename/Split-Trim, Delete is a bulk action - all selected
    rows should be included, matching Assign/Unassign."""
    _application()
    panel = LiveTvPanel()
    panel._tab = "shows"

    row_a = _add_row(panel._show_tree, "/live/show-a.mp4")
    row_b = _add_row(panel._show_tree, "/live/show-b.mp4")
    row_a.setSelected(True)
    row_b.setSelected(True)

    delete_calls = []
    panel.delete_media_requested.connect(
        lambda kind, paths: delete_calls.append((kind, sorted(paths))))

    panel._emit_delete_media()

    assert delete_calls == [("show", ["/live/show-a.mp4", "/live/show-b.mp4"])]
