"""Live TV: channel line-up, show and commercial sync, and the guide.

The guide widget paints the same DIRECTV layout and the same sampled palette
that apps/plugins/mpegplayer/livetv_guide.c draws on the iPod, and it resolves
the clock through the same schedule file, so the PC always shows what the iPod
is playing. See docs/livetv-directv-guide-spec.md.
"""

from __future__ import annotations

import time

from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import (
    QAbstractItemView,
    QComboBox,
    QFrame,
    QHBoxLayout,
    QLabel,
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
        title = selected_slot.title if selected_slot else "No Programming"
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
                    prefix + slot.title, Qt.ElideRight, max(10, x2 - x1 - 10))
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
    assign_requested = Signal(str, list, int)   # kind, paths, channel number
    logo_requested = Signal(int)                # channel number
    favourite_toggled = Signal(int)             # channel number
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
        action_layout = QHBoxLayout(actions)
        action_layout.setContentsMargins(9, 6, 9, 6)
        action_layout.setSpacing(6)
        self._refresh_btn = QPushButton("Rescan Live Folders")
        self._autobuild_btn = QPushButton("Build Channels")
        self._logo_btn = QPushButton("Set Channel Logo")
        self._favourite_btn = QPushButton("Toggle Favorite")
        self._assign_combo = QComboBox()
        self._assign_btn = QPushButton("Assign Selected")
        self._sync_btn = QPushButton("Generate Schedule & Sync to iPod")
        self._sync_btn.setObjectName("store_buy_button")
        for widget in (self._refresh_btn, self._autobuild_btn, self._logo_btn,
                       self._favourite_btn, self._assign_combo,
                       self._assign_btn):
            action_layout.addWidget(widget)
        action_layout.addStretch(1)
        action_layout.addWidget(self._sync_btn)
        layout.addWidget(actions)

        self._refresh_btn.clicked.connect(self.refresh_requested.emit)
        self._autobuild_btn.clicked.connect(self.autobuild_requested.emit)
        self._sync_btn.clicked.connect(self.sync_requested.emit)
        self._logo_btn.clicked.connect(self._emit_logo)
        self._favourite_btn.clicked.connect(self._emit_favourite)
        self._assign_btn.clicked.connect(self._emit_assign)

        self._stack = QStackedWidget()
        layout.addWidget(self._stack, 1)

        self._channel_tree = self._make_tree(
            ["Ch", "Call Sign", "Channel", "Category", "Shows", "Ads",
             "Favorite", "Logo"])
        self._show_tree = self._make_tree(
            ["Show", "Series", "Length", "Channel", "On iPod As"])
        self._ads_tree = self._make_tree(
            ["Commercial", "Length", "Plays On", "On iPod As"])
        self._guide = LiveTvGuideView()

        self._stack.addWidget(self._channel_tree)
        self._stack.addWidget(self._show_tree)
        self._stack.addWidget(self._ads_tree)
        self._stack.addWidget(self._guide)

        self._channel_tree.currentItemChanged.connect(
            self._on_channel_row_changed)

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
        self._logo_btn.setVisible(key == "channels")
        self._favourite_btn.setVisible(key == "channels")
        self._assign_combo.setVisible(key in {"shows", "ads"})
        self._assign_btn.setVisible(key in {"shows", "ads"})
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
                item.title,
                item.series,
                _format_duration(item.duration),
                str(mapping.get(item.path, "")) or "Unassigned",
                item.device_relative(),
            ])
            row.setData(0, Qt.UserRole, item.path)
            self._show_tree.addTopLevelItem(row)
        for column in range(self._show_tree.columnCount()):
            self._show_tree.resizeColumnToContents(column)

    def set_ads(self, ads, channel_for_path=None):
        self._ads_tree.clear()
        mapping = channel_for_path or {}
        for item in ads or []:
            row = QTreeWidgetItem([
                item.title,
                _format_duration(item.duration),
                str(mapping.get(item.path, "")) or "All channels",
                item.device_relative(),
            ])
            row.setData(0, Qt.UserRole, item.path)
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
                       self._assign_btn, self._logo_btn, self._favourite_btn):
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
