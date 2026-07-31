"""Tabbed video browser with large cover grids."""

import os
import re

from PySide6.QtCore import QSize, Qt, Signal
from PySide6.QtGui import QFont, QIcon
from PySide6.QtWidgets import (
    QAbstractItemView,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from ui.library_views import make_album_placeholder
from ui.track_adapter import normalize_track_for_ui, normalize_tracks_for_ui


_HOME_VIDEO_KEYWORDS = {
    "camera", "dcim", "iphone", "gopro", "phone", "vacation", "holiday",
    "birthday", "wedding", "family", "home video", "home videos", "clip", "clips",
}
_TV_SHOW_FOLDER_KEYWORDS = {"tv shows", "shows", "series", "anime", "season", "specials", "special"}
_GENERIC_SHOW_LABELS = {"", "show", "shows", "series", "tv", "tv shows", "special", "specials"}
_TAB_ORDER = (
    ("show", "TV Shows"),
    ("movie", "Movies"),
    ("concert", "Concerts"),
    ("music_video", "Music Videos"),
    ("home_video", "Home Videos"),
)
_TAB_TITLES = dict(_TAB_ORDER)


def _season_number(value):
    match = re.search(r"(?i)(?:season|s)\s*(\d{1,2})", str(value or ""))
    return int(match.group(1)) if match else 0


def _clean_show_name(value):
    text = str(value or "").strip()
    if not text:
        return ""
    text = re.sub(r"[._]+", " ", text)
    text = re.sub(r"(?i)\b(?:complete|full)\s+series\b", "", text).strip(" -_")
    text = re.sub(r"(?i)\bspecials?\b", "", text).strip(" -_")
    text = re.sub(r"(?i)\bseason\s+\d+\b", "", text).strip(" -_")
    text = re.sub(r"(?i)\b(?:archive|archives)\b", "", text).strip(" -_")
    text = re.sub(r"(?i)[-_ ]\d{6,8}$", "", text).strip(" -_")
    text = re.sub(r"\s+", " ", text).strip()
    return "" if text.casefold() in _GENERIC_SHOW_LABELS else text


def _episode_number(track):
    return int(track.get("episode_number") or track.get("track_number") or 0)


def _explicit_episode_number(track):
    """Episode number the source actually asserted.

    _episode_number() also accepts track_number, which is right for labelling
    an episode but wrong for deciding what something is: a single-file movie
    rip routinely carries track_number 1, and treating that as episode 1 files
    every such movie under TV Shows.
    """
    return int(track.get("episode_number") or 0)


def _infer_show_name_from_track(track):
    path_parts_raw = [part for part in re.split(r"[\\/]+", str(track.get("file_path") or "")) if part]
    path_parts = [part.casefold() for part in path_parts_raw]
    parent_folder = path_parts[-2] if len(path_parts) >= 2 else ""
    grandparent_folder = path_parts[-3] if len(path_parts) >= 3 else ""
    great_grandparent_folder = path_parts[-4] if len(path_parts) >= 4 else ""

    if parent_folder.startswith("season") or parent_folder in {"specials", "special"}:
        return _clean_show_name(path_parts_raw[-3] if len(path_parts_raw) >= 3 else "")
    if grandparent_folder in {"tv shows", "shows", "series", "anime"}:
        return _clean_show_name(path_parts_raw[-2] if len(path_parts_raw) >= 2 else "")
    if great_grandparent_folder in {"tv shows", "shows", "series", "anime"}:
        return _clean_show_name(path_parts_raw[-3] if len(path_parts_raw) >= 3 else "")
    return ""


def classify_video_track(track):
    track = normalize_track_for_ui(track)
    video_kind = str(track.get("video_kind") or "").strip()
    show_title = _clean_show_name(track.get("show_title"))
    album = str(track.get("album") or "").strip()
    artist = _clean_show_name(track.get("artist") or track.get("album_artist")) or str(track.get("artist") or track.get("album_artist") or "").strip()
    genre = str(track.get("genre") or "").strip().casefold()
    path_text = str(track.get("file_path") or "").replace(os.sep, " ").casefold()
    path_parts = [part.casefold() for part in re.split(r"[\\/]+", str(track.get("file_path") or "")) if part]
    title = str(track.get("title") or "").strip()
    season_number = int(track.get("season_number") or 0) or _season_number(album)
    path_show_name = _infer_show_name_from_track(track)
    parent_folder = path_parts[-2] if len(path_parts) >= 2 else ""
    grandparent_folder = path_parts[-3] if len(path_parts) >= 3 else ""

    if video_kind == "music_video":
        return {
            "kind": "music_video",
            "group_key": f"music:{(title or album).casefold()}",
            "label": title or "Untitled Music Video",
            "sub_label": artist or album or "Music Video",
            "episode_label": title or "Untitled Music Video",
        }

    if video_kind == "concert":
        return {
            "kind": "concert",
            "group_key": f"concert:{(title or album).casefold()}",
            "label": title or "Untitled Concert",
            "sub_label": artist or album or "Concert",
            "episode_label": title or "Untitled Concert",
        }

    is_show = video_kind == "show"
    show_name = show_title
    # An explicit "movie" classification is the library's own answer. Only a
    # show title or a real season/episode number may override it, never the
    # weaker path and filename heuristics below - those are what drag a movie
    # sitting beside a TV library into the TV Shows tab.
    claims_movie = video_kind == "movie"
    if not is_show and (show_title or season_number or
                        _explicit_episode_number(track)):
        is_show = True
        show_name = show_title or path_show_name or artist or album or "Unknown Show"
    elif claims_movie:
        pass
    elif not is_show and album and re.match(r"(?i)^season\s+\d+$", album):
        is_show = True
        show_name = show_title or path_show_name or artist or "Unknown Show"
    elif not is_show and re.search(r"(?i)s\d{1,2}e\d{1,3}", title):
        is_show = True
        show_name = show_title or path_show_name or artist or album or "Unknown Show"
    elif not is_show and (
        any(keyword in path_text for keyword in _TV_SHOW_FOLDER_KEYWORDS)
        or grandparent_folder in {"tv shows", "shows", "series", "anime"}
    ):
        is_show = True
        if parent_folder in {"specials", "special"} and grandparent_folder:
            show_name = show_title or path_show_name or grandparent_folder.title()
        else:
            show_name = show_title or path_show_name or artist or album or parent_folder.title() or "Unknown Show"

    if is_show:
        season = album if album else ("Specials" if parent_folder in {"specials", "special"} else (f"Season {season_number}" if season_number else ""))
        show_name = _clean_show_name(show_name) or path_show_name or "Unknown Show"
        return {
            "kind": "show",
            "group_key": f"show:{show_name.casefold()}",
            "label": show_name,
            "sub_label": season,
            "episode_label": _episode_label(track),
        }

    if video_kind == "home_video" or any(keyword in genre or keyword in path_text for keyword in _HOME_VIDEO_KEYWORDS):
        return {
            "kind": "home_video",
            "group_key": f"home:{(album or title).casefold()}",
            "label": title or "Untitled Home Video",
            "sub_label": artist or album or "Home Video",
            "episode_label": title or "Untitled Home Video",
        }

    return {
        "kind": "movie",
        "group_key": f"movie:{(title or album).casefold()}",
        "label": title or album or "Untitled Movie",
        "sub_label": artist or album or "Movie",
        "episode_label": title or "Untitled Movie",
    }


def _episode_label(track):
    track = normalize_track_for_ui(track)
    season = int(track.get("season_number") or 0) or _season_number(track.get("album"))
    episode = _episode_number(track)
    title = track.get("title") or "Untitled Episode"
    if season and episode:
        return f"S{season:02d}E{episode:02d} - {title}"
    if episode:
        return f"{episode:02d} - {title}"
    return title


def build_video_browser_groups(tracks):
    grouped = {
        "show": {},
        "movie": [],
        "concert": [],
        "music_video": [],
        "home_video": [],
    }
    for raw_track in normalize_tracks_for_ui(tracks):
        track = normalize_track_for_ui(raw_track)
        info = classify_video_track(track)
        track["video_kind"] = info["kind"]
        track["video_group_key"] = info["group_key"]
        track["video_label"] = info["label"]
        track["video_sub_label"] = info["sub_label"]
        track["video_episode_label"] = info["episode_label"]
        if info["kind"] == "show":
            bucket = grouped["show"].setdefault(
                info["group_key"],
                {"label": info["label"], "tracks": []},
            )
            bucket["tracks"].append(track)
        else:
            grouped[info["kind"]].append(track)

    for show in grouped["show"].values():
        show["tracks"].sort(
            key=lambda item: (
                int(item.get("season_number") or 0) or _season_number(item.get("album")),
                _episode_number(item),
                str(item.get("title") or "").casefold(),
            )
        )
    grouped["movie"].sort(key=lambda item: str(item.get("title") or "").casefold())
    grouped["concert"].sort(key=lambda item: str(item.get("title") or "").casefold())
    grouped["music_video"].sort(key=lambda item: str(item.get("title") or "").casefold())
    grouped["home_video"].sort(key=lambda item: str(item.get("title") or "").casefold())
    return grouped


class VideoGridView(QWidget):
    """Large-cover video browser split into type tabs."""

    track_double_clicked = Signal(object)
    selection_changed = Signal(list)
    context_requested = Signal(object, object)

    def __init__(self, thumbnail_service, parent=None):
        super().__init__(parent)
        self._thumbnail_service = thumbnail_service
        self._tracks = []
        self._groups = {"show": {}, "movie": [], "concert": [], "music_video": [], "home_video": []}
        self._entries_by_kind = {"show": [], "movie": [], "concert": [], "music_video": [], "home_video": []}
        self._current_track_id = None
        self._album_placeholder_path = ""
        self._album_frame_path = ""
        self._show_scope_key = ""

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self._tabs = QTabWidget()
        self._tabs.setObjectName("video_tabs")
        self._tabs.currentChanged.connect(self._on_tab_changed)
        layout.addWidget(self._tabs, 1)

        self._grids = {}
        self._show_title = QLabel("TV Shows")
        self._show_title.setObjectName("browser_header")
        self._show_back = QPushButton("Back to Shows")
        self._show_back.clicked.connect(self._back_to_show_list)

        for kind, title in _TAB_ORDER:
            page = QWidget()
            page.setProperty("video_kind", kind)
            page_layout = QVBoxLayout(page)
            page_layout.setContentsMargins(0, 0, 0, 0)
            page_layout.setSpacing(0)

            if kind == "show":
                header_row = QHBoxLayout()
                header_row.setContentsMargins(0, 0, 0, 8)
                header_row.setSpacing(8)
                header_row.addWidget(self._show_back)
                header_row.addWidget(self._show_title, 1)
                page_layout.addLayout(header_row)

            grid = QListWidget()
            self._configure_grid(grid)
            self._connect_grid(kind, grid)
            page_layout.addWidget(grid, 1)
            self._tabs.addTab(page, title)
            self._grids[kind] = grid

        self._show_back.hide()
        self._update_show_header()

    @staticmethod
    def _configure_grid(grid):
        grid.setObjectName("video_grid")
        grid.setViewMode(QListWidget.IconMode)
        grid.setResizeMode(QListWidget.Adjust)
        grid.setMovement(QListWidget.Static)
        grid.setIconSize(QSize(180, 270))
        grid.setGridSize(QSize(216, 334))
        grid.setSpacing(10)
        grid.setWordWrap(True)
        grid.setSelectionMode(QAbstractItemView.ExtendedSelection)
        grid.setVerticalScrollMode(QAbstractItemView.ScrollPerPixel)
        grid.setContextMenuPolicy(Qt.CustomContextMenu)

    def _connect_grid(self, kind, grid):
        grid.currentItemChanged.connect(
            lambda current, previous, kind=kind: self._on_current_changed(kind, current, previous)
        )
        grid.itemDoubleClicked.connect(
            lambda item, kind=kind: self._on_item_double_clicked(kind, item)
        )
        grid.itemSelectionChanged.connect(
            lambda kind=kind: self._emit_selection_changed(kind)
        )
        grid.customContextMenuRequested.connect(
            lambda pos, kind=kind: self._on_context_menu(kind, pos)
        )

    def apply_theme_assets(self, theme_assets):
        self._album_placeholder_path = theme_assets.asset_path("album_placeholder")
        self._album_frame_path = theme_assets.asset_path("album_frame")

    def set_tracks(self, tracks):
        selected_ids = self.get_selected_track_ids()
        current_kind = self._current_kind()
        self._tracks = normalize_tracks_for_ui(tracks)
        self._groups = build_video_browser_groups(self._tracks)
        if self._show_scope_key and self._show_scope_key not in self._groups["show"]:
            self._show_scope_key = ""
        self._refresh_tabs()
        self._set_current_kind(current_kind)
        self.select_track_ids(selected_ids)

    def _set_current_kind(self, kind):
        for index in range(self._tabs.count()):
            widget = self._tabs.widget(index)
            if widget.property("video_kind") == kind:
                self._tabs.setCurrentIndex(index)
                return

    def _current_kind(self):
        widget = self._tabs.currentWidget()
        return widget.property("video_kind") if widget else "show"

    def _refresh_tabs(self):
        self._entries_by_kind["show"] = self._show_entries()
        self._entries_by_kind["movie"] = self._track_entries(self._groups["movie"])
        self._entries_by_kind["concert"] = self._track_entries(self._groups["concert"])
        self._entries_by_kind["music_video"] = self._track_entries(
            self._groups["music_video"]
        )
        self._entries_by_kind["home_video"] = self._track_entries(self._groups["home_video"])
        self._update_tab_titles()
        self._update_show_header()
        for kind in self._grids:
            self._populate_grid(kind)

    def _update_tab_titles(self):
        counts = {
            "show": len(self._groups["show"]),
            "movie": len(self._groups["movie"]),
            "concert": len(self._groups["concert"]),
            "music_video": len(self._groups["music_video"]),
            "home_video": len(self._groups["home_video"]),
        }
        for index in range(self._tabs.count()):
            kind = self._tabs.widget(index).property("video_kind")
            self._tabs.setTabText(index, f"{_TAB_TITLES[kind]} ({counts[kind]})")

    def _show_entries(self):
        if self._show_scope_key:
            show = self._groups["show"].get(self._show_scope_key, {"tracks": []})
            return self._track_entries(show["tracks"])
        return [
            {"entry_kind": "show", "key": group_key, "label": data["label"], "tracks": data["tracks"]}
            for group_key, data in sorted(self._groups["show"].items(), key=lambda item: item[1]["label"].casefold())
        ]

    @staticmethod
    def _track_entries(tracks):
        return [{"entry_kind": "track", "tracks": [track], "track": track} for track in tracks]

    def _update_show_header(self):
        if self._show_scope_key:
            show = self._groups["show"].get(self._show_scope_key, {"label": "TV Shows"})
            self._show_title.setText(show["label"])
            self._show_back.show()
        else:
            self._show_title.setText("TV Shows")
            self._show_back.hide()

    def _back_to_show_list(self):
        self._show_scope_key = ""
        self._refresh_tabs()
        self._emit_selection_for_active_tab()

    def _populate_grid(self, kind):
        grid = self._grids[kind]
        entries = self._entries_by_kind[kind]
        grid.blockSignals(True)
        grid.clear()
        size = max(grid.iconSize().width(), grid.iconSize().height())
        for entry in entries:
            item = QListWidgetItem()
            item.setData(Qt.UserRole, entry)
            item.setText(self._label_for(kind, entry))
            item.setToolTip(self._tooltip_for(entry))
            item.setTextAlignment(Qt.AlignCenter)
            item.setIcon(self._icon_for_entry(entry, size))
            self._apply_playing_style(item, entry)
            grid.addItem(item)
        if grid.count():
            grid.setCurrentRow(0)
        grid.blockSignals(False)

    def _icon_for_entry(self, entry, size):
        tracks = entry.get("tracks") or []
        sample = normalize_track_for_ui(tracks[0]) if tracks else {}
        if tracks:
            sample = dict(sample)
            sample["_video_tracks"] = [normalize_track_for_ui(track) for track in tracks]
        if entry.get("entry_kind") == "show":
            sample["_video_scope"] = "show"
        thumb = self._thumbnail_service.thumbnail_path(sample, size=max(size, 232))
        if thumb:
            return QIcon(thumb)
        placeholder = make_album_placeholder(size, self._album_placeholder_path, self._album_frame_path)
        return QIcon(placeholder)

    def _label_for(self, kind, entry):
        if entry.get("entry_kind") == "show":
            count = len(entry.get("tracks") or [])
            label = entry.get("label") or "Unknown Show"
            return f"{label}\n{count} episode{'s' if count != 1 else ''}"

        track = normalize_track_for_ui(entry.get("track"))
        if kind == "show" and self._show_scope_key:
            primary = track.get("video_episode_label") or track.get("title") or "Untitled Episode"
            secondary = track.get("genre") or ""
        else:
            primary = track.get("title") or "Untitled Video"
            secondary = (
                track.get("show_title")
                or track.get("artist")
                or track.get("album_artist")
                or track.get("album")
                or ""
            )
        return f"{primary}\n{secondary}" if secondary else primary

    @staticmethod
    def _tooltip_for(entry):
        if entry.get("entry_kind") == "show":
            tracks = entry.get("tracks") or []
            label = entry.get("label") or "Unknown Show"
            seasons = sorted({track.get("album") or "" for track in tracks if track.get("album")})
            season_text = ", ".join(seasons[:4])
            return f"{label}\n{len(tracks)} episodes" + (f"\n{season_text}" if season_text else "")
        track = normalize_track_for_ui(entry.get("track"))
        parts = [track.get("title") or "Untitled Video"]
        for key in ("show_title", "artist", "album", "genre"):
            value = track.get(key) or ""
            if value:
                parts.append(str(value))
        return "\n".join(parts)

    def _apply_playing_style(self, item, entry):
        font = QFont()
        if any(normalize_track_for_ui(track).get("id") == self._current_track_id for track in entry.get("tracks") or []):
            font.setBold(True)
        item.setFont(font)

    def _on_tab_changed(self, index):
        self._emit_selection_for_active_tab()

    def _on_current_changed(self, kind, current, previous):
        if kind == self._current_kind():
            self._emit_selection_for_active_tab()

    def _emit_selection_changed(self, kind):
        if kind == self._current_kind():
            self._emit_selection_for_active_tab()

    def _emit_selection_for_active_tab(self):
        self.selection_changed.emit(self.get_selected_tracks())

    def _on_item_double_clicked(self, kind, item):
        if not item:
            return
        entry = item.data(Qt.UserRole) or {}
        if kind == "show" and entry.get("entry_kind") == "show":
            self._show_scope_key = entry.get("key") or ""
            self._refresh_tabs()
            return
        track = normalize_track_for_ui(entry.get("track"))
        if track:
            self.track_double_clicked.emit(track)

    def _on_context_menu(self, kind, pos):
        grid = self._grids[kind]
        item = grid.itemAt(pos)
        entry = item.data(Qt.UserRole) if item else {}
        self.context_requested.emit(entry or {}, grid.viewport().mapToGlobal(pos))

    def current_tracks(self):
        tracks = []
        for entry in self._entries_by_kind[self._current_kind()]:
            tracks.extend(normalize_track_for_ui(track) for track in entry.get("tracks") or [])
        return tracks

    def set_current_track_id(self, track_id):
        self._current_track_id = track_id
        for kind, grid in self._grids.items():
            for index in range(grid.count()):
                item = grid.item(index)
                self._apply_playing_style(item, item.data(Qt.UserRole) or {})

    def _active_grid(self):
        kind = self._current_kind()
        grid = self._grids.get(kind)
        if grid is not None:
            return grid
        if self._grids:
            return next(iter(self._grids.values()))
        return None

    def get_selected_tracks(self):
        tracks = []
        grid = self._active_grid()
        if grid is None:
            return []
        for item in grid.selectedItems():
            entry = item.data(Qt.UserRole) or {}
            tracks.extend(normalize_track_for_ui(track) for track in entry.get("tracks") or [])
        deduped = []
        seen = set()
        for track in tracks:
            tid = track.get("id")
            key = tid if tid is not None else track.get("file_path")
            if key in seen:
                continue
            seen.add(key)
            deduped.append(track)
        return deduped

    def get_selected_track_ids(self):
        ids = set()
        for track in self.get_selected_tracks():
            tid = track.get("id")
            if tid:
                ids.add(tid)
        return ids

    def select_track_ids(self, track_ids):
        ids = set(track_ids or [])
        for grid in self._grids.values():
            grid.blockSignals(True)
            grid.clearSelection()
            for index in range(grid.count()):
                item = grid.item(index)
                entry = item.data(Qt.UserRole) or {}
                entry_ids = {
                    normalize_track_for_ui(track).get("id")
                    for track in (entry.get("tracks") or [])
                    if normalize_track_for_ui(track).get("id")
                }
                if entry_ids & ids:
                    item.setSelected(True)
                    if grid.currentItem() is None:
                        grid.setCurrentItem(item)
            grid.blockSignals(False)
        self._emit_selection_for_active_tab()
