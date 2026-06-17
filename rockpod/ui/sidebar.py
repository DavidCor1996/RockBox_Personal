"""iTunes 7-style source list sidebar."""

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QTreeWidget, QTreeWidgetItem, QLabel,
    QAbstractItemView, QSizePolicy,
)
from PySide6.QtCore import Qt, Signal, QSize
from PySide6.QtGui import QFont, QIcon, QPixmap, QPainter, QColor, QPen, QBrush

from ui.track_table import TRACK_IDS_MIME


# Sidebar icon size matching iTunes 7
ICON_SIZE = QSize(16, 16)


def _make_icon(color_hex, shape="circle"):
    """Generate compact iTunes-style fallback source icons."""
    px = QPixmap(16, 16)
    px.fill(Qt.transparent)
    p = QPainter(px)
    p.setRenderHint(QPainter.Antialiasing)
    tone = QColor(color_hex)
    p.setPen(QPen(QColor("#6f7780"), 1))
    if shape == "music":
        p.setBrush(QBrush(tone))
        p.drawEllipse(3, 9, 4, 4)
        p.drawEllipse(9, 8, 4, 4)
        p.drawLine(7, 10, 7, 4)
        p.drawLine(13, 9, 13, 3)
        p.drawLine(7, 4, 13, 3)
    elif shape == "artist":
        p.setBrush(QBrush(tone))
        p.drawEllipse(5, 2, 6, 6)
        p.drawRoundedRect(3, 9, 10, 4, 2, 2)
    elif shape == "album":
        p.setBrush(QBrush(tone))
        p.drawEllipse(2, 2, 12, 12)
        p.setBrush(QBrush(QColor("#f7f9fb")))
        p.drawEllipse(6, 6, 4, 4)
    elif shape == "genre":
        from PySide6.QtCore import QPoint
        p.setBrush(QBrush(tone))
        p.drawPolygon(
            [
                QPoint(4, 2), QPoint(11, 2), QPoint(14, 5),
                QPoint(14, 9), QPoint(8, 14), QPoint(3, 9),
            ]
        )
        p.setBrush(QBrush(QColor("#f7f9fb")))
        p.drawEllipse(9, 4, 2, 2)
    elif shape == "playlist":
        p.setPen(QPen(QColor("#70767d"), 1))
        p.drawLine(4, 4, 12, 4)
        p.drawLine(4, 7, 12, 7)
        p.drawLine(4, 10, 12, 10)
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(tone))
        p.drawEllipse(2, 3, 2, 2)
        p.drawEllipse(2, 6, 2, 2)
        p.drawEllipse(2, 9, 2, 2)
    elif shape == "store":
        p.setBrush(QBrush(tone))
        p.drawRoundedRect(3, 5, 10, 8, 2, 2)
        p.setBrush(Qt.NoBrush)
        p.setPen(QPen(QColor("#6f7780"), 1))
        p.drawArc(5, 2, 6, 6, 0, 180 * 16)
        p.drawLine(6, 5, 6, 3)
        p.drawLine(10, 5, 10, 3)
    else:
        p.setBrush(QBrush(tone))
        p.drawRoundedRect(4, 1, 8, 14, 2, 2)
        p.setBrush(QBrush(QColor("#edf2f7")))
        p.drawRect(5, 3, 6, 5)
        p.setBrush(QBrush(QColor("#dfe6ed")))
        p.drawEllipse(5, 10, 6, 3)
    p.end()
    return QIcon(px)


class Sidebar(QWidget):
    """iTunes 7-era source list sidebar."""

    item_selected = Signal(str, str)  # category, item_name
    context_requested = Signal(str, object)  # item_id, global_pos
    tracks_dropped_on_playlist = Signal(int, list)  # playlist_id, track_ids

    # Item identifiers
    LIBRARY_MUSIC = "library_music"
    LIBRARY_VIDEOS = "library_videos"
    LIBRARY_VIDEO_SYNC = "library_video_sync"
    LIBRARY_ARTISTS = "library_artists"
    LIBRARY_ALBUMS = "library_albums"
    LIBRARY_GENRES = "library_genres"
    DEVICE_ROOT = "device_root"
    DEVICE_MUSIC = "device_music"
    DEVICE_NOT_ON_IPOD = "device_not_on_ipod"
    DEVICE_PLAYLIST_PREFIX = "device_playlist_"
    PLAYLIST_PREFIX = "playlist_"
    ROCKBOX_THEMES = "rockbox_themes"
    ROCKBOX_WALLPAPERS = "rockbox_wallpapers"
    ROCKBOX_THEME_DESIGNER = "rockbox_theme_designer"
    ROCKBOX_BOOT = "rockbox_boot"
    ROCKBOX_PLUGINS = "rockbox_plugins"
    ROCKBOX_GAMES = "rockbox_games"
    ROCKBOX_PHOTOS = "rockbox_photos"
    ROCKBOX_BROWSER = "rockbox_browser"
    ROCKBOX_SIMULATOR = "rockbox_simulator"

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("sidebar")
        self.setMinimumWidth(140)
        self.setMaximumWidth(260)
        self.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Expanding)
        self._playlist_icon = None

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self._tree = QTreeWidget()
        self._tree.setObjectName("source_list")
        self._tree.setHeaderHidden(True)
        self._tree.setIconSize(ICON_SIZE)
        self._tree.setIndentation(16)
        self._tree.setSelectionMode(QAbstractItemView.SingleSelection)
        self._tree.setRootIsDecorated(False)
        self._tree.setAnimated(False)
        self._tree.setExpandsOnDoubleClick(False)
        self._tree.setFocusPolicy(Qt.NoFocus)
        self._tree.setAcceptDrops(True)
        self._tree.viewport().setAcceptDrops(True)
        self._tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self._tree.customContextMenuRequested.connect(self._on_context_menu)
        self._tree.dragEnterEvent = self._on_drag_enter
        self._tree.dragMoveEvent = self._on_drag_move
        self._tree.dropEvent = self._on_drop

        layout.addWidget(self._tree)

        # Build the source list structure
        self._build_tree()

        self._tree.itemClicked.connect(self._on_item_clicked)

        # Select "Music" by default
        self._select_default()

    def apply_theme_assets(self, theme_assets):
        icon_map = {
            self._music_item: theme_assets.asset_path("sidebar_music"),
            self._artists_item: theme_assets.asset_path("sidebar_artists"),
            self._albums_item: theme_assets.asset_path("sidebar_albums"),
            self._genres_item: theme_assets.asset_path("sidebar_genres"),
            self._device_item: theme_assets.asset_path("sidebar_device"),
        }
        for item, path in icon_map.items():
            if path:
                icon = QIcon(path)
                if not icon.isNull():
                    item.setIcon(0, icon)
        playlist_icon = theme_assets.asset_path("sidebar_playlist")
        if playlist_icon:
            icon = QIcon(playlist_icon)
            if not icon.isNull():
                self._playlist_icon = icon
                for i in range(self._playlist_header.childCount()):
                    self._playlist_header.child(i).setIcon(0, icon)
        elif self._playlist_icon is None:
            self._playlist_icon = _make_icon("#8b96a3", "playlist")
            for i in range(self._playlist_header.childCount()):
                self._playlist_header.child(i).setIcon(0, self._playlist_icon)

    def _build_tree(self):
        """Construct the iTunes 7-era source list hierarchy."""
        self._tree.clear()

        # ── LIBRARY section ──
        lib_header = self._add_section("LIBRARY")
        self._music_item = self._add_item(lib_header, "Music", self.LIBRARY_MUSIC,
                                          _make_icon("#4a90d9", "music"))
        self._videos_item = self._add_item(lib_header, "Videos", self.LIBRARY_VIDEOS,
                                           _make_icon("#7f8fb3", "device"))
        self._video_sync_item = self._add_item(lib_header, "Video Sync", self.LIBRARY_VIDEO_SYNC,
                                               _make_icon("#6f8fb8", "playlist"))
        self._artists_item = self._add_item(lib_header, "Artists", self.LIBRARY_ARTISTS,
                                            _make_icon("#8d72c9", "artist"))
        self._albums_item = self._add_item(lib_header, "Albums", self.LIBRARY_ALBUMS,
                                           _make_icon("#d38b45", "album"))
        self._genres_item = self._add_item(lib_header, "Genres", self.LIBRARY_GENRES,
                                           _make_icon("#67a85e", "genre"))
        lib_header.setExpanded(True)

        # ── PLAYLISTS section ──
        self._playlist_header = self._add_section("PLAYLISTS")
        self._playlist_header.setExpanded(True)

        # ── ROCKBOX section ──
        self._rockbox_header = self._add_section("ROCKBOX")
        self._themes_item = self._add_item(
            self._rockbox_header, "Themes", self.ROCKBOX_THEMES,
            _make_icon("#7e8fbf", "album")
        )
        self._wallpapers_item = self._add_item(
            self._rockbox_header, "Wallpapers", self.ROCKBOX_WALLPAPERS,
            _make_icon("#8f7db8", "album")
        )
        self._theme_designer_item = self._add_item(
            self._rockbox_header, "Theme Designer", self.ROCKBOX_THEME_DESIGNER,
            _make_icon("#8f7db8", "device")
        )
        self._boot_item = self._add_item(
            self._rockbox_header, "Boot / Branding", self.ROCKBOX_BOOT,
            _make_icon("#8395b7", "album")
        )
        self._plugins_item = self._add_item(
            self._rockbox_header, "Plugins", self.ROCKBOX_PLUGINS,
            _make_icon("#7c96ad", "playlist")
        )
        self._games_item = None
        self._photos_item = self._add_item(
            self._rockbox_header, "Photos", self.ROCKBOX_PHOTOS,
            _make_icon("#8c9f6f", "album")
        )
        self._browser_item = self._add_item(
            self._rockbox_header, "Store", self.ROCKBOX_BROWSER,
            _make_icon("#7391a7", "store")
        )
        self._simulator_item = self._add_item(
            self._rockbox_header, "Simulator", self.ROCKBOX_SIMULATOR,
            _make_icon("#6b88b3", "device")
        )
        self._rockbox_header.setExpanded(True)

        # ── DEVICES section ──
        self._device_header = self._add_section("DEVICES")
        self._device_item = self._add_item(
            self._device_header, "No Device", self.DEVICE_ROOT,
            _make_icon("#8f98a1", "device")
        )
        self._device_music_item = self._add_item(
            self._device_header, "On This iPod", self.DEVICE_MUSIC,
            _make_icon("#7b8793", "music")
        )
        self._not_on_ipod_item = self._add_item(
            self._device_header, "Not on iPod", self.DEVICE_NOT_ON_IPOD,
            _make_icon("#c45c5c", "playlist")
        )
        self._device_header.setExpanded(True)
        self._device_dynamic_items = []

    def _add_section(self, title):
        """Add a section header (non-selectable, uppercase label)."""
        item = QTreeWidgetItem(self._tree)
        item.setText(0, title)
        item.setData(0, Qt.UserRole, "section")
        font = QFont()
        font.setPointSize(11)
        font.setBold(True)
        item.setFont(0, font)
        item.setForeground(0, QColor("#636363"))
        item.setFlags(Qt.ItemIsEnabled)  # Not selectable
        return item

    def _add_item(self, parent, text, item_id, icon=None):
        """Add a selectable item under a section."""
        item = QTreeWidgetItem(parent)
        item.setText(0, text)
        item.setData(0, Qt.UserRole, item_id)
        if icon:
            item.setIcon(0, icon)
        item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
        return item

    def _select_default(self):
        """Select the Music item by default."""
        self._tree.setCurrentItem(self._music_item)

    def _on_item_clicked(self, item, column):
        item_id = item.data(0, Qt.UserRole)
        if item_id and item_id != "section":
            # Determine category
            if item_id.startswith("library_"):
                category = "library"
            elif item_id.startswith("device_"):
                category = "device"
            elif item_id.startswith(self.DEVICE_PLAYLIST_PREFIX):
                category = "device"
            elif item_id.startswith("rockbox_"):
                category = "rockbox"
            elif item_id.startswith("playlist_"):
                category = "playlist"
            else:
                category = "unknown"
            self.item_selected.emit(category, item_id)

    def _on_context_menu(self, pos):
        item = self._tree.itemAt(pos)
        item_id = item.data(0, Qt.UserRole) if item else ""
        self.context_requested.emit(item_id or "", self._tree.viewport().mapToGlobal(pos))

    def _on_drag_enter(self, event):
        if event.mimeData().hasFormat(TRACK_IDS_MIME):
            event.acceptProposedAction()
        else:
            event.ignore()

    def _on_drag_move(self, event):
        item = self._tree.itemAt(event.position().toPoint())
        item_id = item.data(0, Qt.UserRole) if item else ""
        if item_id and str(item_id).startswith(self.PLAYLIST_PREFIX):
            event.acceptProposedAction()
        else:
            event.ignore()

    def _on_drop(self, event):
        item = self._tree.itemAt(event.position().toPoint())
        item_id = item.data(0, Qt.UserRole) if item else ""
        if not item_id or not str(item_id).startswith(self.PLAYLIST_PREFIX):
            event.ignore()
            return

        raw = bytes(event.mimeData().data(TRACK_IDS_MIME)).decode("utf-8")
        track_ids = [int(part) for part in raw.split(",") if part.strip().isdigit()]
        playlist_id = int(str(item_id).replace(self.PLAYLIST_PREFIX, ""))
        self.tracks_dropped_on_playlist.emit(playlist_id, track_ids)
        event.acceptProposedAction()

    # ── Public API for updating sidebar state ──

    def set_device_name(self, name):
        """Update the device entry when an iPod is connected."""
        self._device_item.setText(0, name)
        self._device_item.setIcon(0, _make_icon("#4a90d9", "rect"))
        self._device_item.setToolTip(0, "Summary")

    def set_device_disconnected(self):
        self._device_item.setText(0, "No Device")
        self._device_item.setIcon(0, _make_icon("#888888", "rect"))
        self.clear_device_playlists()

    def set_device_playlist_label(self, playlist_id, name, count=0):
        item = self._find_item(f"{self.DEVICE_PLAYLIST_PREFIX}{playlist_id}")
        if item:
            item.setText(0, f"{name} ({count})" if count else name)

    def add_device_playlist(self, name, playlist_id, count=0):
        item_id = f"{self.DEVICE_PLAYLIST_PREFIX}{playlist_id}"
        label = f"{name} ({count})" if count else name
        item = self._add_item(self._device_header, label, item_id, _make_icon("#6d8fb3"))
        self._device_dynamic_items.append(item)
        return item

    def clear_device_playlists(self):
        for item in list(self._device_dynamic_items):
            parent = item.parent()
            if parent is not None:
                parent.removeChild(item)
        self._device_dynamic_items = []

    def add_playlist(self, name, playlist_id):
        """Add a playlist entry to the sidebar."""
        item_id = f"{self.PLAYLIST_PREFIX}{playlist_id}"
        item = self._add_item(self._playlist_header, name, item_id,
                              _make_icon("#4a90d9"))
        if self._playlist_icon:
            item.setIcon(0, self._playlist_icon)
        return item

    def set_playlist_label(self, playlist_id, name, count=0):
        item = self._find_item(f"{self.PLAYLIST_PREFIX}{playlist_id}")
        if item:
            item.setText(0, f"{name} ({count})" if count else name)

    def clear_playlists(self):
        """Remove all playlist items from the sidebar."""
        while self._playlist_header.childCount() > 0:
            self._playlist_header.removeChild(self._playlist_header.child(0))

    def update_not_on_ipod_count(self, count):
        """Show the count of tracks not on the iPod."""
        if count is None:
            self._not_on_ipod_item.setText(0, "Not on iPod")
            self._not_on_ipod_item.setToolTip(0, "Verifying device inventory")
            return
        if count > 0:
            self._not_on_ipod_item.setText(0, f"Not on iPod ({count})")
        else:
            self._not_on_ipod_item.setText(0, "Not on iPod")
        self._not_on_ipod_item.setToolTip(0, "")

    def select_item(self, item_id):
        """Programmatically select an item by its ID."""
        item = self._find_item(item_id)
        if item:
            self._tree.setCurrentItem(item)

    def _find_item(self, item_id):
        def _find(parent):
            for i in range(parent.childCount()):
                child = parent.child(i)
                if child.data(0, Qt.UserRole) == item_id:
                    return child
                found = _find(child)
                if found:
                    return found
            return None

        root = self._tree.invisibleRootItem()
        return _find(root)
