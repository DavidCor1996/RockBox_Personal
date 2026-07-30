"""Split a Live TV recording into programmes and cut sections out of it.

One evening's tape can hold several shows, and a recording often has adverts
or a long lead-in that should not go to the iPod. Both are the same
operation — keep some ranges, drop the rest — so this dialog edits a list of
episodes, each with a start, an end, and any number of cuts inside it.

Times are entered as H:MM:SS and a frame from the recording is shown at the
position being edited, so cut points can be chosen by eye.
"""

from __future__ import annotations

import os
import subprocess
import tempfile

from PySide6.QtCore import Qt
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QAbstractItemView,
    QDialog,
    QDialogButtonBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QSlider,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)


def format_time(seconds) -> str:
    seconds = max(0, int(seconds or 0))
    return f"{seconds // 3600}:{(seconds % 3600) // 60:02d}:{seconds % 60:02d}"


def parse_time(text) -> int:
    """Read H:MM:SS, M:SS or a plain number of seconds."""
    parts = [part.strip() for part in str(text or "").split(":") if
             part.strip() != ""]
    if not parts:
        return 0
    try:
        values = [int(part) for part in parts]
    except ValueError:
        raise ValueError(f"“{text}” is not a time like 1:23:45")
    total = 0
    for value in values:
        total = total * 60 + value
    return max(0, total)


class LiveTvEditorDialog(QDialog):
    """Edit the episodes and cuts of one recording."""

    def __init__(self, source_path, duration, episodes, title="",
                 ffmpeg="ffmpeg", parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Split and Trim — {os.path.basename(source_path)}")
        self.setMinimumSize(760, 520)
        self._source = source_path
        self._duration = max(1, int(duration or 0))
        self._ffmpeg = ffmpeg or "ffmpeg"
        self._preview_dir = tempfile.mkdtemp(prefix="livetv-preview-")
        self._preview_cache = {}

        layout = QVBoxLayout(self)
        layout.setSpacing(8)

        intro = QLabel(
            f"{os.path.basename(source_path)} — {format_time(self._duration)} "
            "long.\nEach episode below becomes its own programme in the "
            "guide. Cuts are removed from inside an episode."
        )
        intro.setWordWrap(True)
        layout.addWidget(intro)

        body = QHBoxLayout()
        layout.addLayout(body, 1)

        self._tree = QTreeWidget()
        self._tree.setHeaderLabels(["Episode / Cut", "Start", "End", "Length"])
        self._tree.setRootIsDecorated(True)
        self._tree.setSelectionMode(QAbstractItemView.SingleSelection)
        self._tree.currentItemChanged.connect(self._on_selection_changed)
        body.addWidget(self._tree, 3)

        side = QVBoxLayout()
        body.addLayout(side, 2)

        self._preview = QLabel("Preview")
        self._preview.setAlignment(Qt.AlignCenter)
        self._preview.setMinimumSize(320, 240)
        self._preview.setStyleSheet(
            "background:#101418;color:#8fa4b8;border:1px solid #2a3440;")
        side.addWidget(self._preview)

        self._scrub = QSlider(Qt.Horizontal)
        self._scrub.setRange(0, self._duration)
        self._scrub.valueChanged.connect(self._on_scrub)
        side.addWidget(self._scrub)

        self._scrub_label = QLabel(format_time(0))
        self._scrub_label.setAlignment(Qt.AlignCenter)
        side.addWidget(self._scrub_label)

        set_row = QHBoxLayout()
        for label, handler in (("Set Start", self._set_start),
                               ("Set End", self._set_end)):
            button = QPushButton(label)
            button.clicked.connect(handler)
            set_row.addWidget(button)
        side.addLayout(set_row)
        side.addStretch(1)

        edit_row = QHBoxLayout()
        edit_row.addWidget(QLabel("Title"))
        self._title_edit = QLineEdit()
        self._title_edit.editingFinished.connect(self._apply_fields)
        edit_row.addWidget(self._title_edit, 2)
        edit_row.addWidget(QLabel("Start"))
        self._start_edit = QLineEdit()
        self._start_edit.setFixedWidth(84)
        self._start_edit.editingFinished.connect(self._apply_fields)
        edit_row.addWidget(self._start_edit)
        edit_row.addWidget(QLabel("End"))
        self._end_edit = QLineEdit()
        self._end_edit.setFixedWidth(84)
        self._end_edit.editingFinished.connect(self._apply_fields)
        edit_row.addWidget(self._end_edit)
        layout.addLayout(edit_row)

        buttons = QHBoxLayout()
        for label, handler in (
            ("Add Episode", self._add_episode),
            ("Split Here", self._split_here),
            ("Add Cut", self._add_cut),
            ("Remove", self._remove_selected),
            ("Reset to Whole File", self._reset),
        ):
            button = QPushButton(label)
            button.clicked.connect(handler)
            buttons.addWidget(button)
        buttons.addStretch(1)
        layout.addLayout(buttons)

        self._status = QLabel("")
        self._status.setWordWrap(True)
        layout.addWidget(self._status)

        box = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)
        box.accepted.connect(self.accept)
        box.rejected.connect(self.reject)
        layout.addWidget(box)

        self._episodes = self._normalise(episodes, title)
        self._reload_tree()
        self._show_frame(0)

    # -- model -------------------------------------------------------------

    def _normalise(self, episodes, title):
        rows = []
        for index, entry in enumerate(episodes or [], 1):
            rows.append({
                "id": str(entry.get("id") or f"part{index}"),
                "title": str(entry.get("title") or ""),
                "start": int(entry.get("start") or 0),
                "end": int(entry.get("end") or 0) or self._duration,
                "cuts": [[int(a), int(b)] for a, b in
                         (entry.get("cuts") or [])],
            })
        if not rows:
            rows.append({"id": "part1", "title": title or "",
                         "start": 0, "end": self._duration, "cuts": []})
        return rows

    def episodes(self):
        """The edits to save, or an empty list for "use the whole file"."""
        if len(self._episodes) == 1:
            only = self._episodes[0]
            if only["start"] <= 0 and only["end"] >= self._duration \
                    and not only["cuts"]:
                return []
        return [dict(entry) for entry in self._episodes]

    def _episode_length(self, entry) -> int:
        length = max(0, entry["end"] - entry["start"])
        for cut_start, cut_end in entry["cuts"]:
            overlap = min(cut_end, entry["end"]) - max(cut_start,
                                                       entry["start"])
            if overlap > 0:
                length -= overlap
        return max(0, length)

    # -- tree --------------------------------------------------------------

    def _reload_tree(self, select=None):
        self._tree.clear()
        for index, entry in enumerate(self._episodes):
            node = QTreeWidgetItem([
                entry["title"] or f"Episode {index + 1}",
                format_time(entry["start"]),
                format_time(entry["end"]),
                format_time(self._episode_length(entry)),
            ])
            node.setData(0, Qt.UserRole, ("episode", index, -1))
            for cut_index, (cut_start, cut_end) in enumerate(entry["cuts"]):
                child = QTreeWidgetItem([
                    "Cut",
                    format_time(cut_start),
                    format_time(cut_end),
                    format_time(max(0, cut_end - cut_start)),
                ])
                child.setData(0, Qt.UserRole, ("cut", index, cut_index))
                node.addChild(child)
            self._tree.addTopLevelItem(node)
            node.setExpanded(True)

        for column in range(4):
            self._tree.resizeColumnToContents(column)

        if select is not None and 0 <= select < self._tree.topLevelItemCount():
            self._tree.setCurrentItem(self._tree.topLevelItem(select))
        elif self._tree.topLevelItemCount():
            self._tree.setCurrentItem(self._tree.topLevelItem(0))

        total = sum(self._episode_length(entry) for entry in self._episodes)
        self._status.setText(
            f"{len(self._episodes)} programme"
            f"{'s' if len(self._episodes) != 1 else ''}, "
            f"{format_time(total)} of the {format_time(self._duration)} "
            "recording kept.")

    def _selection(self):
        item = self._tree.currentItem()
        if item is None:
            return None
        return item.data(0, Qt.UserRole)

    def _on_selection_changed(self, current, _previous):
        if current is None:
            return
        kind, index, cut_index = current.data(0, Qt.UserRole)
        entry = self._episodes[index]
        if kind == "episode":
            self._title_edit.setEnabled(True)
            self._title_edit.setText(entry["title"])
            start, end = entry["start"], entry["end"]
        else:
            self._title_edit.setEnabled(False)
            self._title_edit.setText("")
            start, end = entry["cuts"][cut_index]
        self._start_edit.setText(format_time(start))
        self._end_edit.setText(format_time(end))
        self._scrub.blockSignals(True)
        self._scrub.setValue(min(start, self._duration))
        self._scrub.blockSignals(False)
        self._scrub_label.setText(format_time(start))
        self._show_frame(start)

    def _apply_fields(self):
        selection = self._selection()
        if selection is None:
            return
        kind, index, cut_index = selection
        try:
            start = parse_time(self._start_edit.text())
            end = parse_time(self._end_edit.text())
        except ValueError as error:
            QMessageBox.warning(self, "Time", str(error))
            return
        if end <= start:
            QMessageBox.warning(self, "Time",
                                "The end must come after the start.")
            return

        entry = self._episodes[index]
        if kind == "episode":
            entry["title"] = self._title_edit.text().strip()
            entry["start"] = start
            entry["end"] = min(end, self._duration)
        else:
            entry["cuts"][cut_index] = [start, end]
            entry["cuts"].sort()
        self._reload_tree(select=index)

    # -- actions -----------------------------------------------------------

    def _add_episode(self):
        last_end = max((entry["end"] for entry in self._episodes), default=0)
        start = min(last_end, self._duration - 1)
        self._episodes.append({
            "id": f"part{len(self._episodes) + 1}",
            "title": "",
            "start": start,
            "end": self._duration,
            "cuts": [],
        })
        self._reload_tree(select=len(self._episodes) - 1)

    def _episode_at(self, position):
        """The episode covering a point in the recording, if any."""
        for index, entry in enumerate(self._episodes):
            if entry["start"] <= position < entry["end"]:
                return index
        return None

    def _split_here(self):
        """Cut an episode in two at the preview position."""
        at = self._scrub.value()
        index = self._episode_at(at)
        if index is None:
            QMessageBox.information(
                self, "Split",
                "Move the preview to a point inside an episode first.")
            return
        entry = self._episodes[index]
        if not entry["start"] < at < entry["end"]:
            QMessageBox.information(
                self, "Split",
                "Move the preview to a point inside an episode first.")
            return

        tail = {
            "id": f"part{len(self._episodes) + 1}",
            "title": "",
            "start": at,
            "end": entry["end"],
            "cuts": [cut for cut in entry["cuts"] if cut[0] >= at],
        }
        entry["end"] = at
        entry["cuts"] = [cut for cut in entry["cuts"] if cut[0] < at]
        self._episodes.insert(index + 1, tail)
        self._reload_tree(select=index + 1)

    def _add_cut(self):
        """Drop a section out of the episode under the playhead."""
        at = self._scrub.value()
        index = self._episode_at(at)
        if index is None:
            QMessageBox.information(
                self, "Add Cut",
                "Move the preview to a point inside an episode first.")
            return
        entry = self._episodes[index]
        start = max(entry["start"], min(at, entry["end"] - 1))
        end = min(entry["end"], start + 30)
        entry["cuts"].append([start, end])
        entry["cuts"].sort()
        self._reload_tree(select=index)

    def _remove_selected(self):
        selection = self._selection()
        if selection is None:
            return
        kind, index, cut_index = selection
        if kind == "cut":
            self._episodes[index]["cuts"].pop(cut_index)
        elif len(self._episodes) > 1:
            self._episodes.pop(index)
        else:
            QMessageBox.information(
                self, "Remove",
                "A recording needs at least one episode. Use "
                "“Reset to Whole File” to undo the split.")
            return
        self._reload_tree(select=min(index, len(self._episodes) - 1))

    def _reset(self):
        self._episodes = [{"id": "part1", "title": "", "start": 0,
                           "end": self._duration, "cuts": []}]
        self._reload_tree(select=0)

    def _set_start(self):
        self._start_edit.setText(format_time(self._scrub.value()))
        self._apply_fields()

    def _set_end(self):
        self._end_edit.setText(format_time(self._scrub.value()))
        self._apply_fields()

    # -- preview -----------------------------------------------------------

    def _on_scrub(self, value):
        self._scrub_label.setText(format_time(value))
        self._show_frame(value)

    def _show_frame(self, seconds):
        path = self._frame_path(int(seconds))
        if path and os.path.isfile(path):
            pixmap = QPixmap(path)
            if not pixmap.isNull():
                self._preview.setPixmap(pixmap.scaled(
                    self._preview.width(), self._preview.height(),
                    Qt.KeepAspectRatio, Qt.SmoothTransformation))
                return
        self._preview.setText("No preview")

    def _frame_path(self, seconds):
        """Pull one frame out of the recording, cached per second."""
        seconds = max(0, min(seconds, self._duration - 1))
        cached = self._preview_cache.get(seconds)
        if cached:
            return cached

        target = os.path.join(self._preview_dir, f"{seconds}.jpg")
        command = [
            self._ffmpeg, "-y", "-loglevel", "error",
            "-ss", str(seconds), "-i", self._source,
            "-frames:v", "1", "-vf", "scale=320:-2", target,
        ]
        try:
            subprocess.run(command, capture_output=True, timeout=25,
                           check=False)
        except (OSError, subprocess.SubprocessError):
            return ""
        if os.path.isfile(target):
            self._preview_cache[seconds] = target
            return target
        return ""

    def done(self, result):
        try:
            import shutil

            shutil.rmtree(self._preview_dir, ignore_errors=True)
        except Exception:
            pass
        super().done(result)
