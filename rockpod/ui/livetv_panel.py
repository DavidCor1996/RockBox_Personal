"""Live TV: channel line-up, show and commercial sync, and the guide.

The guide widget paints the same DIRECTV layout and the same sampled palette
that apps/plugins/mpegplayer/livetv_guide.c draws on the iPod, and it resolves
the clock through the same schedule file, so the PC always shows what the iPod
is playing. See docs/livetv-directv-guide-spec.md.
"""

from __future__ import annotations

import os
from pathlib import Path
import time

from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import (
    QAbstractItemView,
    QComboBox,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QProgressBar,
    QPushButton,
    QSizePolicy,
    QStackedWidget,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)

# Sampled from the DIRECTV receiver user guide artwork at 600 dpi.
DTV_BANNER_TOP = QColor(220, 238, 249)
DTV_BANNER_BOT = QColor(183, 214, 233)
DTV_STRIP_BG = QColor(183, 214, 233)
DTV_STRIP_CELL = QColor(157, 195, 220)
DTV_STRIP_TEXT = QColor(16, 56, 107)
DTV_BRAND_BLUE = QColor(0, 82, 155)
DTV_DESC_BG = QColor(2, 111, 175)
DTV_HDR_BG = QColor(18, 37, 73)
DTV_ROW_BG = QColor(9, 72, 113)
DTV_GRID_LINE = QColor(10, 42, 80)
DTV_SEL_BG = QColor(254, 196, 37)
DTV_SEL_TEXT = QColor(16, 37, 74)
DTV_HINT_BG = QColor(15, 86, 137)
DTV_TEXT = QColor(255, 255, 255)
DTV_DIM_TEXT = QColor(92, 130, 168)
DTV_DOT_RED = QColor(194, 42, 24)
DTV_DOT_GREEN = QColor(46, 161, 92)
DTV_DOT_YELLOW = QColor(249, 198, 60)

SLOT_SECONDS = 1800
GRID_COLUMNS = 3


def _clean_text(value: str) -> str:
    return " ".join(str(value or "").split())


def _title_from_path(path: str) -> str:
    stem = Path(path or "").stem
    return _clean_text(stem.replace("_", " ")) or "Program"


def _slot_title(slot) -> str:
    return _clean_text(slot.title) or _title_from_path(slot.path)


def _format_clock(epoch: float) -> str:
    moment = time.localtime(epoch)
    hour = moment.tm_hour % 12 or 12
    suffix = "p" if moment.tm_hour >= 12 else "a"
    return f"{hour}:{moment.tm_min:02d}{suffix}"


def _format_duration(seconds: int) -> str:
    seconds = max(0, int(seconds or 0))
    if seconds >= 3600:
        return f"{seconds // 3600}:{(seconds % 3600) // 60:02d}:{seconds % 60:02d}"
    return f"{seconds // 60}:{seconds % 60:02d}"


class LiveTvGuideView(QWidget):
    """A DIRECTV program guide drawn from the generated schedule."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("livetv_guide_view")
        self.setMinimumHeight(360)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self._slots = []
        self._channels = []
        self._selected_row = 0
        self._timer = QTimer(self)
        self._timer.timeout.connect(self.update)
        self._timer.start(1000)

    def set_schedule(self, channels, slots):
        self._channels = list(channels or [])
        self._slots = list(slots or [])
        self._selected_row = min(self._selected_row,
                                 max(0, len(self._channels) - 1))
        self.update()

    def set_selected_row(self, row):
        self._selected_row = max(0, int(row or 0))
        self.update()

    # -- schedule lookup ------------------------------------------------

    def _slot_at(self, channel_number, when):
        from services.livetv import resolve_slot

        return resolve_slot(self._slots, channel_number, when)

    def _row_blocks(self, channel_number, window_start, window_seconds):
        """Programmes overlapping the window, as ``(slot, start, length)``.

        A guide lists programmes, not the commercials inside them, so this
        merges each show with the ad break that follows it.
        """
        results = []
        probe = window_start
        guard = 0
        while probe < window_start + window_seconds and guard < 64:
            guard += 1
            slot, offset = self._slot_at(channel_number, probe)
            if slot is None:
                probe += SLOT_SECONDS
                continue
            into_block = (slot.start - slot.block_start) + offset
            block_start = probe - into_block
            block_length = slot.block_duration or slot.duration
            results.append((slot, block_start, block_length))
            step = block_length - into_block
            probe += step if step > 0 else 1
        return results

    # -- painting --------------------------------------------------------

    def paintEvent(self, event):  # noqa: N802 (Qt naming)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.TextAntialiasing, True)
        width = self.width()
        height = self.height()

        painter.fillRect(0, 0, width, height, DTV_HDR_BG)
        if not self._channels:
            painter.setPen(QPen(DTV_DIM_TEXT))
            painter.drawText(self.rect(), Qt.AlignCenter,
                             "No Live TV channels yet.\n"
                             "Add videos to ~/Videos/Live and build channels.")
            return

        now = time.time()
        window_start = (int(now) // SLOT_SECONDS) * SLOT_SECONDS
        window_seconds = GRID_COLUMNS * SLOT_SECONDS

        banner_h = 34
        strip_h = 22
        desc_h = 46
        hdr_h = 22
        hint_h = 24
        grid_top = banner_h + strip_h + desc_h + hdr_h
        rows_available = max(1, (height - grid_top - hint_h) // 26)
        row_h = 26
        visible = min(rows_available, len(self._channels))
        top_row = 0
        if self._selected_row >= visible:
            top_row = self._selected_row - visible + 1

        selected_channel = self._channels[
            min(self._selected_row, len(self._channels) - 1)]
        selected_slot, selected_offset = self._slot_at(
            selected_channel.number, now)

        chan_col_w = max(96, width // 6)
        col_w = (width - chan_col_w) // GRID_COLUMNS

        small = QFont(self.font())
        small.setPointSizeF(max(7.5, self.font().pointSizeF() - 1))
        bold = QFont(small)
        bold.setBold(True)

        # Banner
        painter.fillRect(0, 0, width, banner_h, DTV_BANNER_TOP)
        painter.fillRect(0, banner_h - 8, width, 8, DTV_BANNER_BOT)
        painter.setFont(bold)
        painter.setPen(QPen(DTV_BRAND_BLUE))
        painter.drawText(10, 22, "DIRECTV")
        title = _slot_title(selected_slot) if selected_slot else "No Programming"
        painter.drawText(96, 22, title)
        painter.setFont(small)
        painter.setPen(QPen(DTV_DIM_TEXT))
        painter.drawText(width - 60, 22, "guide")

        # Information strip
        strip_y = banner_h
        painter.fillRect(0, strip_y, width, strip_h, DTV_STRIP_BG)
        painter.fillRect(0, strip_y, 110, strip_h, DTV_STRIP_CELL)
        painter.setPen(QPen(QColor(255, 255, 255)))
        painter.drawLine(110, strip_y, 110, strip_y + strip_h)
        painter.setPen(QPen(DTV_STRIP_TEXT))
        painter.setFont(bold)
        painter.drawText(8, strip_y + 15,
                         time.strftime("%a ", time.localtime(now)) +
                         _format_clock(now))
        painter.setFont(small)
        if selected_slot:
            # The air window is the programme's, commercial break included.
            into_block = ((selected_slot.start - selected_slot.block_start) +
                          selected_offset)
            block_start = now - into_block
            block_length = (selected_slot.block_duration or
                            selected_slot.duration)
            painter.drawText(122, strip_y + 15,
                             f"{_format_clock(block_start)} - "
                             f"{_format_clock(block_start + block_length)}")
            rating = selected_slot.rating or "--"
            painter.drawText(width - 90, strip_y + 15, rating)

        # Description block
        desc_y = strip_y + strip_h
        painter.fillRect(0, desc_y, width, desc_h, DTV_DESC_BG)
        painter.setPen(QPen(DTV_TEXT))
        painter.setFont(bold)
        painter.drawText(10, desc_y + 17,
                         f"{selected_channel.number} {selected_channel.callsign}"
                         f"  {selected_channel.name}")
        painter.setFont(small)
        if selected_slot:
            painter.drawText(10, desc_y + 34, selected_slot.description or "")

        # Time header
        hdr_y = desc_y + desc_h
        painter.fillRect(0, hdr_y, width, hdr_h, DTV_HDR_BG)
        painter.setPen(QPen(DTV_TEXT))
        painter.setFont(bold)
        painter.drawText(8, hdr_y + 15,
                         time.strftime("%a %-m/%-d",
                                       time.localtime(window_start)))
        for column in range(GRID_COLUMNS):
            x = chan_col_w + column * col_w
            painter.drawText(x + 6, hdr_y + 15,
                             _format_clock(window_start + column * SLOT_SECONDS))

        # Grid
        painter.setFont(small)
        for index in range(visible):
            channel_index = top_row + index
            if channel_index >= len(self._channels):
                break
            channel = self._channels[channel_index]
            y = grid_top + index * row_h

            painter.fillRect(0, y, width, row_h - 1, DTV_ROW_BG)
            painter.fillRect(0, y, chan_col_w, row_h - 1, DTV_HDR_BG)
            painter.setPen(QPen(DTV_GRID_LINE))
            painter.drawLine(0, y + row_h - 1, width, y + row_h - 1)

            painter.setPen(QPen(DTV_TEXT))
            painter.setFont(bold)
            painter.drawText(8, y + 17,
                             f"{channel.number} {channel.callsign}")
            painter.setFont(small)

            for slot, slot_start, slot_length in self._row_blocks(
                    channel.number, window_start, window_seconds):
                x1 = chan_col_w + int(
                    (max(slot_start, window_start) - window_start) *
                    col_w / SLOT_SECONDS)
                x2 = chan_col_w + int(
                    (min(slot_start + slot_length,
                         window_start + window_seconds) - window_start) *
                    col_w / SLOT_SECONDS)
                x2 = min(x2, width)
                if x2 <= x1:
                    continue

                live = slot_start <= now < slot_start + slot_length
                selected = live and channel_index == self._selected_row
                if selected:
                    painter.fillRect(x1, y + 2, x2 - x1 - 2, row_h - 5,
                                     DTV_SEL_BG)
                painter.setPen(QPen(DTV_GRID_LINE))
                if slot_start >= window_start:
                    painter.drawLine(x1, y + 2, x1, y + row_h - 3)

                painter.setPen(QPen(DTV_SEL_TEXT if selected else DTV_TEXT))
                prefix = "‹ " if slot_start < window_start else ""
                label = painter.fontMetrics().elidedText(
                    prefix + _slot_title(slot),
                    Qt.ElideRight, max(10, x2 - x1 - 10))
                painter.drawText(x1 + 5, y + 17, label)

        # Hint bar
        hint_y = height - hint_h
        painter.fillRect(0, hint_y, width, hint_h, DTV_HINT_BG)
        painter.setPen(QPen(DTV_TEXT))
        painter.drawText(10, hint_y + 16, "All Channels")
        for offset, (color, label) in enumerate((
                (DTV_DOT_RED, "-12 hrs"),
                (DTV_DOT_GREEN, "+12 hrs"),
                (DTV_DOT_YELLOW, "Guide Options"))):
            x = width - 320 + offset * 105
            painter.fillRect(x, hint_y + 8, 8, 8, color)
            painter.setPen(QPen(DTV_TEXT))
            painter.drawText(x + 13, hint_y + 16, label)


class LiveTvPanel(QWidget):
    """Live TV sync: channels, shows, commercials, and the live guide."""

    refresh_requested = Signal()
    autobuild_requested = Signal()
    sync_requested = Signal()
    sync_selected_requested = Signal(int)
    assign_requested = Signal(str, list, int)   # kind, paths, channel number
    unassign_requested = Signal(str, list)      # kind, paths
    delete_media_requested = Signal(str, list)  # kind, paths
    rename_requested = Signal(str, str)         # kind, path
    edit_media_requested = Signal(str, str)     # kind, path
    add_channel_requested = Signal()
    edit_channel_requested = Signal(int)        # channel number
    remove_channel_requested = Signal(int)      # channel number
    logo_requested = Signal(int)                # channel number
    favourite_toggled = Signal(int)             # channel number
    parental_lock_toggled = Signal(int)          # channel number
    open_sources_requested = Signal()

    TABS = (
        ("channels", "Channels"),
        ("shows", "Shows"),
        ("ads", "Commercials"),
        ("guide", "Guide"),
    )

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("livetv_panel")
        self._tab = "channels"
        self._tab_buttons = {}
        self._channels = []

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 8, 10, 10)
        layout.setSpacing(7)

        nav = QFrame()
        nav.setObjectName("itunes_store_nav")
        nav_layout = QHBoxLayout(nav)
        nav_layout.setContentsMargins(9, 5, 9, 5)
        title = QLabel("Live TV")
        title.setObjectName("itunes_store_title")
        nav_layout.addWidget(title)
        nav_layout.addStretch(1)
        layout.addWidget(nav)

        hero = QFrame()
        hero.setObjectName("itunes_store_hero")
        hero_layout = QVBoxLayout(hero)
        hero_layout.setContentsMargins(14, 10, 14, 10)
        kicker = QLabel("DIRECTV style programming for your iPod")
        kicker.setObjectName("itunes_store_kicker")
        self._headline = QLabel("Build channels from ~/Videos/Live")
        self._headline.setObjectName("itunes_store_headline")
        self._subhead = QLabel(
            "Shows come from ~/Videos/Live and commercials from "
            "~/Videos/Live/ADS. Two to three commercials play between shows, "
            "and each channel runs on the clock so tuning in feels live."
        )
        self._subhead.setObjectName("itunes_store_subhead")
        self._subhead.setWordWrap(True)
        hero_layout.addWidget(kicker)
        hero_layout.addWidget(self._headline)
        hero_layout.addWidget(self._subhead)
        layout.addWidget(hero)

        tab_bar = QFrame()
        tab_bar.setObjectName("itunes_store_tabs")
        tab_layout = QHBoxLayout(tab_bar)
        tab_layout.setContentsMargins(9, 4, 9, 4)
        tab_layout.setSpacing(4)
        for key, label in self.TABS:
            button = QPushButton(label)
            button.setObjectName("store_nav_button")
            button.setCheckable(True)
            button.clicked.connect(
                lambda _checked=False, tab=key: self.set_tab(tab))
            tab_layout.addWidget(button)
            self._tab_buttons[key] = button
        tab_layout.addStretch(1)
        layout.addWidget(tab_bar)

        actions = QFrame()
        actions.setObjectName("itunes_store_import_bar")
        actions_layout = QVBoxLayout(actions)
        action_layout = QHBoxLayout()
        action_layout.setContentsMargins(9, 6, 9, 6)
        action_layout.setSpacing(6)
        self._refresh_btn = QPushButton("Rescan Live Folders")
        self._autobuild_btn = QPushButton("Build Channels")
        self._add_btn = QPushButton("Add Channel")
        self._edit_btn = QPushButton("Edit Channel")
        self._remove_btn = QPushButton("Remove Channel")
        self._logo_btn = QPushButton("Set Channel Logo")
        self._favourite_btn = QPushButton("Toggle Favorite")
        self._parental_lock_btn = QPushButton("Toggle Parental Lock")
        self._assign_combo = QComboBox()
        self._assign_btn = QPushButton("Assign Selected")
        self._unassign_btn = QPushButton("Unassign")
        self._delete_media_btn = QPushButton("Delete")
        self._delete_media_btn.setToolTip(
            "Remove the selected show(s) or commercial(s) from the Live TV "
            "library and delete their source file(s) from disk.")
        self._rename_btn = QPushButton("Rename")
        self._rename_btn.setToolTip(
            "Change the title this programme shows in the guide. "
            "Double-clicking a row does the same.")
        self._edit_media_btn = QPushButton("Split / Trim")
        self._edit_media_btn.setToolTip(
            "Cut sections out of a recording, or split a long one into "
            "several programmes.")
        self._sync_btn = QPushButton("Generate Schedule and Sync")
        self._sync_selected_btn = QPushButton("Sync Selected Channel")
        self._sync_btn.setObjectName("store_buy_button")
        self._edit_btn.setToolTip(
            "Change this channel's number, call sign and name. "
            "Double-clicking a row does the same.")
        self._assign_btn.setToolTip(
            "Move the selected shows or commercials onto the chosen channel.")
        for widget in (self._refresh_btn, self._autobuild_btn, self._add_btn,
                       self._edit_btn, self._remove_btn, self._logo_btn,
                       self._favourite_btn, self._parental_lock_btn,
                       self._assign_combo,
                       self._assign_btn, self._unassign_btn,
                       self._delete_media_btn,
                       self._rename_btn, self._edit_media_btn):
            action_layout.addWidget(widget)
        actions_layout.addLayout(action_layout)
        sync_layout = QHBoxLayout()
        sync_layout.addStretch(1)
        sync_layout.addWidget(self._sync_selected_btn)
        sync_layout.addWidget(self._sync_btn)
        actions_layout.addLayout(sync_layout)
        layout.addWidget(actions)

        self._refresh_btn.clicked.connect(self.refresh_requested.emit)
        self._autobuild_btn.clicked.connect(self.autobuild_requested.emit)
        self._sync_btn.clicked.connect(self.sync_requested.emit)
        self._sync_selected_btn.clicked.connect(self._emit_sync_selected)
        self._add_btn.clicked.connect(self.add_channel_requested.emit)
        self._edit_btn.clicked.connect(self._emit_edit)
        self._remove_btn.clicked.connect(self._emit_remove)
        self._logo_btn.clicked.connect(self._emit_logo)
        self._favourite_btn.clicked.connect(self._emit_favourite)
        self._parental_lock_btn.clicked.connect(self._emit_parental_lock)
        self._assign_btn.clicked.connect(self._emit_assign)
        self._unassign_btn.clicked.connect(self._emit_unassign)
        self._delete_media_btn.clicked.connect(self._emit_delete_media)
        self._rename_btn.clicked.connect(self._emit_rename)
        self._edit_media_btn.clicked.connect(self._emit_edit_media)

        self._stack = QStackedWidget()
        layout.addWidget(self._stack, 1)

        self._channel_tree = self._make_tree(
            ["Ch", "Call Sign", "Channel", "Category", "Shows", "Ads",
             "Favorite", "Parental Lock", "Logo"])
        self._show_tree = self._make_tree(
            ["Show (guide title)", "Series", "Length", "Channel", "File"])
        self._ads_tree = self._make_tree(
            ["Commercial", "Length", "Plays On", "File"])
        self._guide = LiveTvGuideView()

        self._stack.addWidget(self._channel_tree)
        self._stack.addWidget(self._show_tree)
        self._stack.addWidget(self._ads_tree)
        self._stack.addWidget(self._guide)

        self._channel_tree.currentItemChanged.connect(
            self._on_channel_row_changed)
        self._channel_tree.itemDoubleClicked.connect(
            self._on_channel_activated)
        self._show_tree.itemDoubleClicked.connect(self._on_media_activated)
        self._ads_tree.itemDoubleClicked.connect(self._on_media_activated)

        self._progress = QProgressBar()
        self._progress.setVisible(False)
        layout.addWidget(self._progress)

        self._status = QLabel("")
        self._status.setObjectName("theme_hub_status")
        self._status.setWordWrap(True)
        layout.addWidget(self._status)

        self.set_tab("channels")

    @staticmethod
    def _make_tree(headers):
        tree = QTreeWidget()
        tree.setObjectName("video_sync_tree")
        tree.setHeaderLabels(headers)
        tree.setRootIsDecorated(False)
        tree.setAlternatingRowColors(True)
        tree.setSelectionMode(QAbstractItemView.ExtendedSelection)
        tree.setUniformRowHeights(True)
        return tree

    # -- tabs -------------------------------------------------------------

    def set_tab(self, key):
        keys = [name for name, _label in self.TABS]
        if key not in keys:
            key = "channels"
        self._tab = key
        for name, button in self._tab_buttons.items():
            button.setChecked(name == key)
            button.setProperty("active", name == key)
            button.style().unpolish(button)
            button.style().polish(button)
        self._stack.setCurrentIndex(keys.index(key))

        is_guide = key == "guide"
        on_channels = key == "channels"
        on_media = key in {"shows", "ads"}
        for widget in (self._add_btn, self._edit_btn, self._remove_btn,
                       self._logo_btn, self._favourite_btn,
                       self._parental_lock_btn):
            widget.setVisible(on_channels)
        for widget in (self._assign_combo, self._assign_btn,
                       self._unassign_btn, self._rename_btn,
                       self._edit_media_btn):
            widget.setVisible(on_media)
        self._autobuild_btn.setVisible(not is_guide)

    def current_tab(self):
        return self._tab

    # -- population --------------------------------------------------------

    def set_channels(self, channels, show_counts=None, ad_counts=None):
        self._channels = list(channels or [])
        selected = self._selected_channel_number()

        self._channel_tree.clear()
        for channel in self._channels:
            item = QTreeWidgetItem([
                str(channel.number),
                channel.callsign,
                channel.name,
                channel.category or "Series",
                str((show_counts or {}).get(channel.number,
                                            len(channel.shows))),
                str((ad_counts or {}).get(channel.number,
                                          len(channel.ads)) or "All"),
                "Yes" if channel.favourite else "No",
                "Locked" if channel.parental_locked else "No",
                channel.logo or "Call sign",
            ])
            item.setData(0, Qt.UserRole, channel.number)
            self._channel_tree.addTopLevelItem(item)
        for column in range(self._channel_tree.columnCount()):
            self._channel_tree.resizeColumnToContents(column)

        self._assign_combo.clear()
        for channel in self._channels:
            self._assign_combo.addItem(
                f"{channel.number} {channel.callsign}", channel.number)
        if not self._channels:
            self._assign_combo.addItem("No channels", 0)

        if selected is not None:
            self.select_channel(selected)

    def set_shows(self, shows, channel_for_path=None):
        self._show_tree.clear()
        mapping = channel_for_path or {}
        for item in shows or []:
            row = QTreeWidgetItem([
                _slot_title(item),
                item.series,
                _format_duration(item.duration),
                str(mapping.get(item.path, "")) or "Unassigned",
                os.path.basename(item.path),
            ])
            row.setData(0, Qt.UserRole, item.path)
            row.setToolTip(0, item.path)
            if getattr(item, "renamed", False):
                font = row.font(0)
                font.setItalic(True)
                row.setFont(0, font)
            self._show_tree.addTopLevelItem(row)
        for column in range(self._show_tree.columnCount()):
            self._show_tree.resizeColumnToContents(column)

    def set_ads(self, ads, channel_for_path=None):
        self._ads_tree.clear()
        mapping = channel_for_path or {}
        for item in ads or []:
            row = QTreeWidgetItem([
                _slot_title(item),
                _format_duration(item.duration),
                str(mapping.get(item.path, "")) or "All channels",
                os.path.basename(item.path),
            ])
            row.setData(0, Qt.UserRole, item.path)
            row.setToolTip(0, item.path)
            if getattr(item, "renamed", False):
                font = row.font(0)
                font.setItalic(True)
                row.setFont(0, font)
            self._ads_tree.addTopLevelItem(row)
        for column in range(self._ads_tree.columnCount()):
            self._ads_tree.resizeColumnToContents(column)

    def set_schedule(self, channels, slots):
        self._guide.set_schedule(channels, slots)

    def set_summary(self, headline, detail):
        self._headline.setText(str(headline or ""))
        self._subhead.setText(str(detail or ""))

    def set_status(self, text):
        self._status.setText(str(text or ""))
        self._status.setVisible(bool(text))

    def set_progress(self, value, maximum, label=""):
        if maximum <= 0:
            self._progress.setVisible(False)
            return
        self._progress.setVisible(True)
        self._progress.setMaximum(maximum)
        self._progress.setValue(value)
        self._progress.setFormat(f"%v / %m  {label}"[:120])

    def clear_progress(self):
        self._progress.setVisible(False)

    def set_busy(self, busy):
        for widget in (self._refresh_btn, self._autobuild_btn, self._sync_btn,
                       self._sync_selected_btn,
                       self._assign_btn, self._unassign_btn, self._add_btn,
                       self._edit_btn, self._remove_btn, self._logo_btn,
                       self._favourite_btn, self._rename_btn,
                       self._edit_media_btn):
            widget.setEnabled(not busy)

    # -- selection helpers ---------------------------------------------------

    def _selected_channel_number(self):
        item = self._channel_tree.currentItem()
        if item is None:
            return None
        return item.data(0, Qt.UserRole)

    def select_channel(self, number):
        for index in range(self._channel_tree.topLevelItemCount()):
            item = self._channel_tree.topLevelItem(index)
            if item.data(0, Qt.UserRole) == number:
                self._channel_tree.setCurrentItem(item)
                self._guide.set_selected_row(index)
                return

    def _on_channel_row_changed(self, current, _previous):
        if current is None:
            return
        self._guide.set_selected_row(
            self._channel_tree.indexOfTopLevelItem(current))

    def _selected_paths(self, tree):
        return [item.data(0, Qt.UserRole) for item in tree.selectedItems()
                if item.data(0, Qt.UserRole)]

    def _selected_single_path(self, tree):
        """The one selected row's path, or None when the target is
        ambiguous.

        Rename and Split/Trim act on a single recording. With more than
        one row selected, Qt's selection order does not match click order,
        so silently taking the first path can rename or edit a completely
        different file than the one the user meant.
        """
        paths = self._selected_paths(tree)
        return paths[0] if len(paths) == 1 else None

    def _emit_assign(self):
        number = self._assign_combo.currentData()
        if not number:
            return
        if self._tab == "shows":
            paths = self._selected_paths(self._show_tree)
            if paths:
                self.assign_requested.emit("show", paths, int(number))
        elif self._tab == "ads":
            paths = self._selected_paths(self._ads_tree)
            if paths:
                self.assign_requested.emit("ad", paths, int(number))

    def _emit_logo(self):
        number = self._selected_channel_number()
        if number:
            self.logo_requested.emit(int(number))

    def _emit_favourite(self):
        number = self._selected_channel_number()
        if number:
            self.favourite_toggled.emit(int(number))

    def _emit_parental_lock(self):
        number = self._selected_channel_number()
        if number is not None:
            self.parental_lock_toggled.emit(int(number))

    def _emit_edit(self):
        number = self._selected_channel_number()
        if number:
            self.edit_channel_requested.emit(int(number))

    def _emit_remove(self):
        number = self._selected_channel_number()
        if number:
            self.remove_channel_requested.emit(int(number))

    def _emit_unassign(self):
        if self._tab == "shows":
            paths = self._selected_paths(self._show_tree)
            if paths:
                self.unassign_requested.emit("show", paths)
        elif self._tab == "ads":
            paths = self._selected_paths(self._ads_tree)
            if paths:
                self.unassign_requested.emit("ad", paths)

    def _emit_delete_media(self):
        if self._tab == "shows":
            paths = self._selected_paths(self._show_tree)
            if paths:
                self.delete_media_requested.emit("show", paths)
        elif self._tab == "ads":
            paths = self._selected_paths(self._ads_tree)
            if paths:
                self.delete_media_requested.emit("ad", paths)

    def _on_channel_activated(self, item, _column):
        number = item.data(0, Qt.UserRole)
        if number:
            self.edit_channel_requested.emit(int(number))

    def _on_media_activated(self, item, _column):
        """Double-clicking a show or commercial renames it.

        This matches the Channels tab, where double-clicking edits the row.
        """
        path = item.data(0, Qt.UserRole)
        if path:
            self.rename_requested.emit(
                "show" if self._tab == "shows" else "ad", str(path))

    def _emit_edit_media(self):
        tree = self._show_tree if self._tab == "shows" else self._ads_tree
        path = self._selected_single_path(tree)
        if path:
            self.edit_media_requested.emit(
                "show" if self._tab == "shows" else "ad", str(path))

    def _emit_rename(self):
        tree = self._show_tree if self._tab == "shows" else self._ads_tree
        path = self._selected_single_path(tree)
        if path:
            self.rename_requested.emit(
                "show" if self._tab == "shows" else "ad", str(path))

    def _emit_sync_selected(self):
        number = self._selected_channel_number()
        if number is not None:
            self.sync_selected_requested.emit(number)
        else:
            self.set_status("Select a channel first.")


class LiveTvStorePanel(QWidget):
    """Store tab for downloading Live TV shows and commercials.

    Downloads stay in the Live TV staging folder for additional iPods.
    """

    browse_requested = Signal(str, str)   # query, destination
    import_requested = Signal(str, str)   # url, destination

    BROWSE_TABS = (
        ("shows", "Classic TV", "show",
         "full episode 1980s television broadcast"),
        ("gameshows", "Game Shows", "show",
         "classic game show full episode"),
        ("sports", "Sports", "show", "classic wrestling full broadcast"),
        ("ads", "Commercials", "ad", "1980s tv commercial break"),
        ("promos", "Promos and Bumpers", "ad", "1990s tv station promo bumper"),
    )

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("browser_panel")
        self._tab_buttons = {}
        self._results = []
        self._destination = "show"

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 8, 10, 10)
        layout.setSpacing(7)

        nav = QFrame()
        nav.setObjectName("itunes_store_nav")
        nav_layout = QHBoxLayout(nav)
        nav_layout.setContentsMargins(9, 5, 9, 5)
        title = QLabel("Live TV")
        title.setObjectName("itunes_store_title")
        nav_layout.addWidget(title)
        nav_layout.addStretch(1)
        layout.addWidget(nav)

        tab_bar = QFrame()
        tab_bar.setObjectName("itunes_store_tabs")
        tab_layout = QHBoxLayout(tab_bar)
        tab_layout.setContentsMargins(9, 4, 9, 4)
        tab_layout.setSpacing(4)
        for key, label, destination, query in self.BROWSE_TABS:
            button = QPushButton(label)
            button.setObjectName("store_nav_button")
            button.setCheckable(True)
            button.clicked.connect(
                lambda _checked=False, tab=key, dest=destination, text=query:
                self._select_tab(tab, dest, text))
            tab_layout.addWidget(button)
            self._tab_buttons[key] = button
        tab_layout.addStretch(1)
        layout.addWidget(tab_bar)

        hero = QFrame()
        hero.setObjectName("itunes_store_hero")
        hero_layout = QVBoxLayout(hero)
        hero_layout.setContentsMargins(14, 12, 14, 12)
        kicker = QLabel("Live TV Store")
        kicker.setObjectName("itunes_store_kicker")
        headline = QLabel("Fill your channels with shows and commercials")
        headline.setObjectName("itunes_store_headline")
        subhead = QLabel(
            "Downloads are staged locally and kept for syncing multiple iPods. "
            "Only download material you own or are permitted to download."
        )
        subhead.setObjectName("itunes_store_subhead")
        subhead.setWordWrap(True)
        hero_layout.addWidget(kicker)
        hero_layout.addWidget(headline)
        hero_layout.addWidget(subhead)
        layout.addWidget(hero)

        browse_bar = QFrame()
        browse_bar.setObjectName("itunes_store_import_bar")
        browse_layout = QHBoxLayout(browse_bar)
        browse_layout.setContentsMargins(9, 6, 9, 6)
        browse_layout.setSpacing(6)
        browse_label = QLabel("Browse")
        browse_label.setObjectName("itunes_store_small_title")
        self._browse_edit = QLineEdit()
        self._browse_edit.setObjectName("itunes_store_import_url")
        self._browse_edit.setPlaceholderText("Search for shows or commercials")
        self._browse_edit.returnPressed.connect(self._emit_browse)
        self._destination_combo = QComboBox()
        self._destination_combo.addItem("Add as Show", "show")
        self._destination_combo.addItem("Add as Commercial", "ad")
        self._destination_combo.currentIndexChanged.connect(
            self._on_destination_changed)
        self._browse_btn = QPushButton("Search")
        self._browse_btn.setObjectName("store_buy_button")
        self._browse_btn.clicked.connect(self._emit_browse)
        browse_layout.addWidget(browse_label)
        browse_layout.addWidget(self._browse_edit, 1)
        browse_layout.addWidget(self._destination_combo)
        browse_layout.addWidget(self._browse_btn)
        layout.addWidget(browse_bar)

        import_bar = QFrame()
        import_bar.setObjectName("itunes_store_import_bar")
        import_layout = QHBoxLayout(import_bar)
        import_layout.setContentsMargins(9, 6, 9, 6)
        import_layout.setSpacing(6)
        url_label = QLabel("Video URL")
        url_label.setObjectName("itunes_store_small_title")
        self._url_edit = QLineEdit()
        self._url_edit.setObjectName("itunes_store_import_url")
        self._url_edit.setPlaceholderText(
            "Paste a video URL to add to Live TV")
        self._url_edit.returnPressed.connect(self._emit_import)
        self._import_btn = QPushButton("Download to Live TV")
        self._import_btn.setObjectName("store_buy_button")
        self._import_btn.clicked.connect(self._emit_import)
        import_layout.addWidget(url_label)
        import_layout.addWidget(self._url_edit, 1)
        import_layout.addWidget(self._import_btn)
        layout.addWidget(import_bar)

        self._results_tree = QTreeWidget()
        self._results_tree.setObjectName("video_sync_tree")
        self._results_tree.setHeaderLabels(
            ["Title", "Channel", "Length", "URL"])
        self._results_tree.setRootIsDecorated(False)
        self._results_tree.setAlternatingRowColors(True)
        self._results_tree.itemDoubleClicked.connect(self._on_result_activated)
        layout.addWidget(self._results_tree, 1)

        self._status = QLabel("")
        self._status.setObjectName("theme_hub_status")
        self._status.setWordWrap(True)
        layout.addWidget(self._status)

        self._select_tab("shows", "show", self.BROWSE_TABS[0][3], emit=False)

    def _select_tab(self, key, destination, query, emit=True):
        for name, button in self._tab_buttons.items():
            button.setChecked(name == key)
            button.setProperty("active", name == key)
            button.style().unpolish(button)
            button.style().polish(button)
        self.set_destination(destination)
        self._browse_edit.setText(query)
        if emit:
            self.browse_requested.emit(query, destination)

    def set_destination(self, destination):
        self._destination = "ad" if destination == "ad" else "show"
        index = self._destination_combo.findData(self._destination)
        if index >= 0:
            self._destination_combo.blockSignals(True)
            self._destination_combo.setCurrentIndex(index)
            self._destination_combo.blockSignals(False)

    def destination(self):
        return self._destination

    def _on_destination_changed(self, _index):
        self._destination = self._destination_combo.currentData() or "show"

    def _emit_browse(self):
        query = self._browse_edit.text().strip()
        if query:
            self.browse_requested.emit(query, self._destination)

    def _emit_import(self):
        url = self._url_edit.text().strip()
        if url:
            self.import_requested.emit(url, self._destination)

    def _on_result_activated(self, item, _column):
        url = item.data(0, Qt.UserRole)
        if url:
            self._url_edit.setText(str(url))
            self.import_requested.emit(str(url), self._destination)

    def set_results(self, results):
        self._results = [dict(result or {}) for result in results or []]
        self._results_tree.clear()
        for result in self._results:
            duration = result.get("duration") or 0
            row = QTreeWidgetItem([
                str(result.get("title") or "Untitled"),
                str(result.get("uploader") or ""),
                _format_duration(duration),
                str(result.get("url") or ""),
            ])
            row.setData(0, Qt.UserRole, result.get("url") or "")
            self._results_tree.addTopLevelItem(row)
        for column in range(self._results_tree.columnCount()):
            self._results_tree.resizeColumnToContents(column)

    def set_status(self, text, running=False):
        self._status.setText(str(text or ""))
        self._status.setVisible(bool(text))
        self._browse_btn.setEnabled(not running)
        self._import_btn.setEnabled(not running)
