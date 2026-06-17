"""Generic store panel with optional embedded Qt WebEngine view."""

from __future__ import annotations

import os
import zipfile
from urllib.parse import urlparse

from PySide6.QtCore import Qt, QUrl, Signal
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QComboBox,
    QScrollArea,
    QSizePolicy,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)

def _load_webengine_view():
    if os.environ.get("QT_QPA_PLATFORM", "").lower() == "offscreen":
        return None
    try:  # pragma: no cover - optional dependency
        from PySide6.QtWebEngineWidgets import QWebEngineView  # type: ignore
        return QWebEngineView
    except Exception:
        return None


_AD_HOST_KEYWORDS = (
    "doubleclick",
    "googlesyndication",
    "googleadservices",
    "adservice",
    "adnxs",
    "adsystem",
    "taboola",
    "outbrain",
    "zedo",
    "ads.",
    ".ads",
    "tracking",
    "analytics",
    "pixel.",
    "scorecardresearch",
    "quantserve",
    "amazon-adsystem",
)

_AD_PATH_KEYWORDS = (
    "/ads",
    "/ads/",
    "doubleclick",
    "adservice",
    "banner",
    "prebid",
    "googlesyndication",
    "taboola",
    "outbrain",
    "analytics",
    "tracking",
    "pixel",
)


def should_block_browser_url(url):
    parsed = urlparse(str(url or ""))
    host = (parsed.netloc or "").lower()
    path = (parsed.path or "").lower()
    query = (parsed.query or "").lower()
    if not host:
        return False
    if any(token in host for token in _AD_HOST_KEYWORDS):
        return True
    haystack = f"{path}?{query}"
    return any(token in haystack for token in _AD_PATH_KEYWORDS)


def _install_adblock(profile):
    try:  # pragma: no cover - optional dependency
        from PySide6.QtWebEngineCore import QWebEngineUrlRequestInterceptor  # type: ignore
    except Exception:
        return None

    class _AdBlockInterceptor(QWebEngineUrlRequestInterceptor):
        def interceptRequest(self, info):  # noqa: N802 - Qt API
            if should_block_browser_url(info.requestUrl().toString()):
                info.block(True)

    interceptor = _AdBlockInterceptor(profile)
    profile.setUrlRequestInterceptor(interceptor)
    return interceptor


def _is_supported_archive(path):
    return str(path or "").lower().endswith(".zip")


def extract_downloaded_archive(archive_path, destination_dir):
    archive_path = os.path.abspath(str(archive_path))
    destination_dir = os.path.abspath(str(destination_dir))
    if not _is_supported_archive(archive_path):
        return []
    extracted = []
    os.makedirs(destination_dir, exist_ok=True)
    with zipfile.ZipFile(archive_path, "r") as bundle:
        for member in bundle.infolist():
            name = member.filename or ""
            if not name or member.is_dir():
                continue
            normalized = os.path.normpath(name).lstrip(os.sep)
            if normalized.startswith(".."):
                continue
            target_path = os.path.abspath(os.path.join(destination_dir, normalized))
            if os.path.commonpath([destination_dir, target_path]) != destination_dir:
                continue
            os.makedirs(os.path.dirname(target_path), exist_ok=True)
            with bundle.open(member, "r") as source, open(target_path, "wb") as dest:
                dest.write(source.read())
            extracted.append(target_path)
    return extracted


class BrowserPanel(QWidget):
    open_external_requested = Signal(str)
    store_import_requested = Signal(str, str)
    store_result_import_requested = Signal(dict, str)
    store_search_requested = Signal(str, str)
    store_album_details_requested = Signal(dict)
    store_home_tab_requested = Signal(str)

    STORE_HOME_TABS = [
        ("featured", "Featured"),
        ("new_releases", "New Releases"),
        ("top_albums", "Top Albums"),
        ("just_added", "Just Added"),
        ("alternative", "Alternative"),
        ("rock", "Rock"),
        ("hip_hop", "Hip-Hop/Rap"),
    ]

    def __init__(
        self,
        parent=None,
        music_store=True,
        title="Music Store",
        web_title="Tidal Web Store",
        show_downloads=None,
    ):
        super().__init__(parent)
        self.setObjectName("browser_panel")
        self._music_store = bool(music_store)
        self._show_downloads = self._music_store if show_downloads is None else bool(show_downloads)
        self._home_url = ""
        self._download_dir = ""
        self._download_items = {}
        self._finalized_downloads = set()
        self._store_import_items = {}
        self._active_store_import_item = None
        self._store_results = []
        self._store_detail_result_key = ""
        self._store_home_tab = "featured"
        self._store_home_tab_buttons = {}
        self._web_frame = None
        self._auto_accept_cookies = True
        self._cookie_accept_attempted_hosts = set()

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        shell = QFrame()
        shell.setObjectName("itunes_store_shell")
        shell_layout = QVBoxLayout(shell)
        shell_layout.setContentsMargins(10, 8, 10, 10)
        shell_layout.setSpacing(7)

        chrome = QFrame()
        chrome.setObjectName("itunes_store_nav")
        chrome_layout = QHBoxLayout(chrome)
        chrome_layout.setContentsMargins(9, 5, 9, 5)
        chrome_layout.setSpacing(4)

        self._url_edit = QLineEdit()
        self._url_edit.setObjectName("itunes_store_search")
        self._url_edit.returnPressed.connect(self._go_to_entered_url)
        self._home_btn = QPushButton("Home")
        self._back_btn = QPushButton("Back")
        self._forward_btn = QPushButton("Forward")
        self._reload_btn = QPushButton("Reload")
        self._open_external_btn = QPushButton("Open in Browser")
        self._home_btn.setObjectName("store_nav_button")
        self._back_btn.setObjectName("store_nav_button")
        self._forward_btn.setObjectName("store_nav_button")
        self._reload_btn.setObjectName("store_nav_button")
        self._open_external_btn.setObjectName("store_nav_button")
        self._home_btn.clicked.connect(self.go_home)
        self._back_btn.clicked.connect(self.go_back)
        self._forward_btn.clicked.connect(self.go_forward)
        self._reload_btn.clicked.connect(self.reload)
        self._open_external_btn.clicked.connect(self._emit_open_external)

        title_label = QLabel(str(title or "Store"))
        title_label.setObjectName("itunes_store_title")
        chrome_layout.addWidget(self._back_btn)
        chrome_layout.addWidget(self._forward_btn)
        chrome_layout.addWidget(self._reload_btn)
        chrome_layout.addWidget(self._home_btn)
        chrome_layout.addWidget(title_label)
        chrome_layout.addStretch(1)
        chrome_layout.addWidget(self._url_edit, 1)
        chrome_layout.addWidget(self._open_external_btn)
        shell_layout.addWidget(chrome)
        for widget in (
            self._back_btn,
            self._forward_btn,
            self._reload_btn,
            self._home_btn,
            self._url_edit,
            self._open_external_btn,
        ):
            widget.setVisible(not self._music_store)

        self._store_search_bar = QFrame()
        self._store_search_bar.setObjectName("itunes_store_import_bar")
        search_layout = QHBoxLayout(self._store_search_bar)
        search_layout.setContentsMargins(9, 6, 9, 6)
        search_layout.setSpacing(6)

        search_label = QLabel("Search")
        search_label.setObjectName("itunes_store_small_title")
        self._store_search_edit = QLineEdit()
        self._store_search_edit.setObjectName("itunes_store_import_url")
        self._store_search_edit.setPlaceholderText("Search Tidal albums")
        self._store_search_edit.returnPressed.connect(self._emit_store_search)
        self._store_source = QComboBox()
        self._store_source.addItem("Tidal", "tidal")
        self._store_source.addItem("Qobuz", "qobuz")
        self._store_source.addItem("Deezer", "deezer")
        self._store_search_btn = QPushButton("Search Store")
        self._store_search_btn.setObjectName("store_buy_button")
        self._store_search_btn.clicked.connect(self._emit_store_search)
        search_layout.addWidget(search_label)
        search_layout.addWidget(self._store_search_edit, 1)
        search_layout.addWidget(self._store_source)
        search_layout.addWidget(self._store_search_btn)
        shell_layout.addWidget(self._store_search_bar)

        self._store_tab_bar = QFrame()
        self._store_tab_bar.setObjectName("itunes_store_tabs")
        tab_layout = QHBoxLayout(self._store_tab_bar)
        tab_layout.setContentsMargins(9, 4, 9, 4)
        tab_layout.setSpacing(4)
        for key, label in self.STORE_HOME_TABS:
            button = QPushButton(label)
            button.setObjectName("store_nav_button")
            button.setCheckable(True)
            button.clicked.connect(lambda _checked=False, tab_key=key: self._emit_store_home_tab(tab_key))
            tab_layout.addWidget(button)
            self._store_home_tab_buttons[key] = button
        tab_layout.addStretch(1)
        shell_layout.addWidget(self._store_tab_bar)

        self._store_import_bar = QFrame()
        self._store_import_bar.setObjectName("itunes_store_import_bar")
        import_layout = QHBoxLayout(self._store_import_bar)
        import_layout.setContentsMargins(9, 6, 9, 6)
        import_layout.setSpacing(6)

        self._store_url_edit = QLineEdit()
        self._store_url_edit.setObjectName("itunes_store_import_url")
        self._store_url_edit.setPlaceholderText("Tidal, Qobuz, Deezer, SoundCloud, or Spotify URL")
        self._store_url_edit.returnPressed.connect(self._emit_store_import)
        self._store_format = QComboBox()
        self._store_format.addItem("FLAC", "flac")
        self._store_format.addItem("ALAC", "alac")
        self._store_format.addItem("MP3", "mp3")
        self._store_import_btn = QPushButton("Add to Library")
        self._store_import_btn.setObjectName("store_buy_button")
        self._store_import_btn.clicked.connect(self._emit_store_import)
        self._store_status = QLabel("")
        self._store_status.setObjectName("theme_hub_status")
        self._store_status.setWordWrap(True)
        self._store_status.setTextInteractionFlags(Qt.TextSelectableByMouse | Qt.LinksAccessibleByMouse)
        self._store_status.setVisible(False)

        import_label = QLabel("Import")
        import_label.setObjectName("itunes_store_small_title")
        import_layout.addWidget(import_label)
        import_layout.addWidget(self._store_url_edit, 1)
        import_layout.addWidget(self._store_format)
        import_layout.addWidget(self._store_import_btn)
        shell_layout.addWidget(self._store_import_bar)
        shell_layout.addWidget(self._store_status)
        self._store_search_bar.setVisible(self._music_store)
        self._store_tab_bar.setVisible(self._music_store)
        self._store_import_bar.setVisible(False)
        self.set_store_home_tab("featured", emit=False)

        if self._music_store:
            store_scroll = QScrollArea()
            store_scroll.setObjectName("itunes_store_scroll")
            store_scroll.setWidgetResizable(True)
            store_scroll.setFrameShape(QFrame.NoFrame)
            store_page = QWidget()
            store_page.setObjectName("itunes_store_page")
            store_layout = QVBoxLayout(store_page)
            store_layout.setContentsMargins(0, 0, 0, 0)
            store_layout.setSpacing(8)
            self._build_storefront(store_layout)
            store_scroll.setWidget(store_page)
            shell_layout.addWidget(store_scroll, 1)
            self._web = None
            self._adblock = None
            self._notice = None
        else:
            webengine_view_cls = _load_webengine_view()
            web_frame = QFrame()
            web_frame.setObjectName("itunes_store_web_frame")
            self._web_frame = web_frame
            web_layout = QVBoxLayout(web_frame)
            web_layout.setContentsMargins(7, 6, 7, 7)
            web_layout.setSpacing(5)
            web_title = QLabel(str(web_title or "Web Store"))
            web_title.setObjectName("itunes_store_section_title")
            web_layout.addWidget(web_title)
            if webengine_view_cls is not None:
                self._notice = None
                self._web = webengine_view_cls()
                self._web.setMinimumHeight(520)
                self._web.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
                self._web.urlChanged.connect(self._on_url_changed)
                self._web.loadFinished.connect(self._on_load_finished)
                profile = self._web.page().profile()
                self._adblock = _install_adblock(profile)
                if hasattr(profile, "downloadRequested"):
                    profile.downloadRequested.connect(self._on_download_requested)
                web_layout.addWidget(self._web, 1)
            else:
                self._web = None
                self._adblock = None
                self._notice = QLabel(
                    "Embedded store view unavailable in this build.\n"
                    "Use Open in Browser to open the configured URL in your system browser."
                )
                self._notice.setWordWrap(True)
                self._notice.setObjectName("theme_hub_status")
                self._notice.setMinimumHeight(520)
                self._notice.setAlignment(Qt.AlignCenter)
                web_layout.addWidget(self._notice, 1)
            shell_layout.addWidget(web_frame, 1)

        self._downloads = QTreeWidget()
        self._downloads.setObjectName("itunes_store_downloads")
        self._downloads.setHeaderLabels(["Download", "Status", "Progress", "Folder"])
        self._downloads.setRootIsDecorated(False)
        self._downloads.setMinimumHeight(96)
        self._downloads.setMaximumHeight(150)
        self._downloads.setVisible(self._show_downloads)
        shell_layout.addWidget(self._downloads)
        layout.addWidget(shell)

    def _build_storefront(self, layout):
        hero = QFrame()
        hero.setObjectName("itunes_store_hero")
        hero_layout = QHBoxLayout(hero)
        hero_layout.setContentsMargins(14, 12, 14, 12)
        hero_layout.setSpacing(14)

        hero_text = QVBoxLayout()
        kicker = QLabel("Featured Music")
        kicker.setObjectName("itunes_store_kicker")
        headline = QLabel("New releases, charts, and lossless albums")
        headline.setObjectName("itunes_store_headline")
        subhead = QLabel("Tidal, Qobuz, Deezer, and SoundCloud selections curated for RockPod.")
        subhead.setObjectName("itunes_store_subhead")
        subhead.setWordWrap(True)
        self._store_hero_kicker = kicker
        self._store_hero_headline = headline
        self._store_hero_subhead = subhead
        hero_text.addWidget(kicker)
        hero_text.addWidget(headline)
        hero_text.addWidget(subhead)
        hero_text.addStretch(1)
        hero_layout.addLayout(hero_text, 3)

        covers = QGridLayout()
        covers.setSpacing(7)
        self._store_hero_covers = []
        for index, (title, tone) in enumerate(
            [
                ("Indie", "#8aa6c8"),
                ("Rock", "#c57b6a"),
                ("Pop", "#b39ad4"),
                ("Jazz", "#86a079"),
            ]
        ):
            cover = QLabel(title)
            cover.setObjectName("itunes_store_cover")
            cover.setProperty("tone", tone)
            cover.setAlignment(Qt.AlignCenter)
            cover.setFixedSize(86, 86)
            covers.addWidget(cover, index // 2, index % 2)
            self._store_hero_covers.append(cover)
        hero_layout.addLayout(covers, 1)
        layout.addWidget(hero)

        body = QHBoxLayout()
        body.setSpacing(8)
        main = QFrame()
        main.setObjectName("itunes_store_panel")
        main_layout = QVBoxLayout(main)
        main_layout.setContentsMargins(8, 7, 8, 8)
        main_layout.setSpacing(7)
        self._store_results_header = self._section_header("New Music")
        main_layout.addWidget(self._store_results_header)

        grid = QGridLayout()
        grid.setHorizontalSpacing(10)
        grid.setVerticalSpacing(8)
        self._store_results_grid = grid
        self.set_store_results([])
        main_layout.addLayout(grid)
        body.addWidget(main, 3)

        side = QFrame()
        side.setObjectName("itunes_store_sidebar_panel")
        side_layout = QVBoxLayout(side)
        side_layout.setContentsMargins(8, 7, 8, 8)
        side_layout.setSpacing(5)
        side_layout.addWidget(self._section_header("Top Downloads"))
        self._store_chart_buttons = []
        for index, title in enumerate(
            ["Linkin Park", "Oliver Tree", "The Beatles", "Pink Floyd", "Billie Eilish"],
            start=1,
        ):
            row = QPushButton(f"{index}. {title}")
            row.setObjectName("itunes_store_chart_row")
            row.clicked.connect(lambda _checked=False, query=title: self.search_store(query))
            side_layout.addWidget(row)
            self._store_chart_buttons.append(row)
        side_layout.addStretch(1)
        body.addWidget(side, 1)
        layout.addLayout(body)

    def set_store_home_tab(self, key, emit=False):
        tab_key = str(key or "featured").strip().lower()
        valid = {name for name, _label in self.STORE_HOME_TABS}
        if tab_key not in valid:
            tab_key = "featured"
        self._store_home_tab = tab_key
        for name, button in self._store_home_tab_buttons.items():
            button.setChecked(name == tab_key)
            button.setProperty("active", name == tab_key)
            button.style().unpolish(button)
            button.style().polish(button)
        if emit:
            self.store_home_tab_requested.emit(tab_key)

    def _emit_store_home_tab(self, key):
        self.set_store_home_tab(key, emit=True)

    def _section_header(self, text):
        header = QLabel(text)
        header.setObjectName("itunes_store_section_title")
        return header

    def _album_tile(self, title, subtitle, index):
        tile = QFrame()
        tile.setObjectName("itunes_store_album_tile")
        tile.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        tile_layout = QVBoxLayout(tile)
        tile_layout.setContentsMargins(4, 4, 4, 6)
        tile_layout.setSpacing(4)
        cover = QLabel(title[:2].upper())
        cover.setObjectName("itunes_store_album_art")
        cover.setAlignment(Qt.AlignCenter)
        cover.setFixedSize(82, 82)
        title_label = QLabel(title)
        title_label.setObjectName("itunes_store_album_title")
        subtitle_label = QLabel(subtitle)
        subtitle_label.setObjectName("itunes_store_album_subtitle")
        tile_layout.addWidget(cover, alignment=Qt.AlignHCenter)
        tile_layout.addWidget(title_label)
        tile_layout.addWidget(subtitle_label)
        return tile

    def _set_store_cover(self, label, result, size):
        title = str(result.get("title") or "Album")
        label.setText(title[:2].upper())
        label.setAlignment(Qt.AlignCenter)
        label.setFixedSize(size, size)
        cover_path = str(result.get("cover_path") or "")
        if cover_path and os.path.isfile(cover_path):
            pixmap = QPixmap(cover_path)
            if not pixmap.isNull():
                label.setPixmap(pixmap.scaled(size, size, Qt.KeepAspectRatio, Qt.SmoothTransformation))

    def _store_result_tile(self, result):
        title = str(result.get("title") or "Untitled Album")
        artist = str(result.get("artist") or "Unknown Artist")
        tracks = result.get("tracks") or 0
        date = str(result.get("date") or "")
        source = str(result.get("source") or "tidal").capitalize()
        section = str(result.get("section") or "").strip()
        url = str(result.get("url") or "")

        tile = QFrame()
        tile.setObjectName("itunes_store_album_tile")
        tile.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        tile.setCursor(Qt.PointingHandCursor)
        tile.mousePressEvent = lambda event, item=dict(result): self._handle_store_result_tile_press(event, item)
        tile_layout = QVBoxLayout(tile)
        tile_layout.setContentsMargins(4, 4, 4, 6)
        tile_layout.setSpacing(4)

        cover = QLabel()
        cover.setObjectName("itunes_store_album_art")
        cover.setAttribute(Qt.WA_TransparentForMouseEvents, True)
        self._set_store_cover(cover, result, 82)

        title_label = QLabel(title)
        title_label.setObjectName("itunes_store_album_title")
        title_label.setWordWrap(True)
        title_label.setAttribute(Qt.WA_TransparentForMouseEvents, True)
        subtitle_label = QLabel(artist)
        subtitle_label.setObjectName("itunes_store_album_subtitle")
        subtitle_label.setAttribute(Qt.WA_TransparentForMouseEvents, True)
        detail_bits = [section or source]
        if date:
            detail_bits.append(date[:4])
        if tracks:
            detail_bits.append(f"{tracks} tracks")
        detail_label = QLabel(" - ".join(detail_bits))
        detail_label.setObjectName("itunes_store_album_subtitle")
        detail_label.setAttribute(Qt.WA_TransparentForMouseEvents, True)
        owned = bool(result.get("owned") or result.get("in_library"))
        buy_btn = QPushButton("Owned" if owned else "Buy")
        buy_btn.setObjectName("store_buy_button")
        buy_btn.setEnabled(bool(url) and not owned)
        buy_btn.clicked.connect(lambda _checked=False, item=dict(result): self._emit_store_result_import(item))

        tile_layout.addWidget(cover, alignment=Qt.AlignHCenter)
        tile_layout.addWidget(title_label)
        tile_layout.addWidget(subtitle_label)
        tile_layout.addWidget(detail_label)
        tile_layout.addWidget(buy_btn)
        return tile

    def _handle_store_result_tile_press(self, event, result):
        if event.button() == Qt.LeftButton:
            self._open_store_result_details(result)
            event.accept()

    def show_store_result_details(self, result):
        grid = getattr(self, "_store_results_grid", None)
        if grid is None:
            return
        self._clear_store_results_grid()
        self._store_detail_result_key = self._store_result_key(result)
        if hasattr(self, "_store_results_header"):
            self._store_results_header.setText("Album Details")
        grid.addWidget(self._store_result_details(result), 0, 0, 1, 3)

    def update_store_result_details(self, result):
        incoming = dict(result or {})
        key = self._store_result_key(incoming)
        if not key:
            return
        merged = dict(incoming)
        for index, existing in enumerate(list(self._store_results or [])):
            if self._store_result_key(existing) == key:
                merged = dict(existing)
                merged.update(incoming)
                self._store_results[index] = merged
                break
        if getattr(self, "_store_detail_result_key", "") == key:
            self.show_store_result_details(merged)

    @staticmethod
    def _store_result_key(result):
        item = dict(result or {})
        source = str(item.get("source") or "").strip().lower()
        media_type = str(item.get("media_type") or "album").strip().lower()
        item_id = str(item.get("id") or "").strip()
        if source and media_type and item_id:
            return f"{source}:{media_type}:{item_id}"
        return str(item.get("url") or "").strip()

    @staticmethod
    def _store_result_int(value, default=0):
        try:
            return int(value)
        except (TypeError, ValueError):
            return default

    @classmethod
    def _store_result_track_items(cls, result):
        item = dict(result or {})
        tracks = item.get("track_items") or item.get("tracks_detail") or item.get("tracklist") or []
        if not isinstance(tracks, list):
            return []
        normalized = [dict(track or {}) for track in tracks if isinstance(track, dict)]
        return sorted(
            normalized,
            key=lambda track: (
                cls._store_result_int(
                    track.get("disc_number") or track.get("volume_number") or track.get("disc") or 1,
                    1,
                ),
                cls._store_result_int(
                    track.get("track_number") or track.get("number") or track.get("trackNumber") or 0,
                    0,
                ),
            ),
        )

    @staticmethod
    def _format_track_duration(value):
        try:
            seconds = int(float(value))
        except (TypeError, ValueError):
            return ""
        if seconds <= 0:
            return ""
        return f"{seconds // 60}:{seconds % 60:02d}"

    def _store_result_details(self, result):
        result = dict(result or {})
        title = str(result.get("title") or "Untitled Album")
        artist = str(result.get("artist") or "Unknown Artist")
        date = str(result.get("date") or "")
        tracks = result.get("tracks") or 0
        source = str(result.get("source") or "tidal").capitalize()
        url = str(result.get("url") or "")
        track_items = self._store_result_track_items(result)

        panel = QFrame()
        panel.setObjectName("itunes_store_album_detail")
        panel_layout = QVBoxLayout(panel)
        panel_layout.setContentsMargins(0, 0, 0, 0)
        panel_layout.setSpacing(8)

        top_bar = QFrame()
        top_bar.setObjectName("itunes_store_detail_bar")
        top_layout = QHBoxLayout(top_bar)
        top_layout.setContentsMargins(6, 4, 6, 4)
        back_btn = QPushButton("Back")
        back_btn.setObjectName("store_nav_button")
        back_btn.clicked.connect(self._restore_store_results)
        top_layout.addWidget(back_btn)
        top_layout.addStretch(1)
        panel_layout.addWidget(top_bar)

        body = QHBoxLayout()
        body.setContentsMargins(8, 4, 8, 8)
        body.setSpacing(14)

        cover = QLabel()
        cover.setObjectName("itunes_store_detail_art")
        self._set_store_cover(cover, result, 150)
        body.addWidget(cover, alignment=Qt.AlignTop)

        info = QVBoxLayout()
        info.setSpacing(5)
        title_label = QLabel(title)
        title_label.setObjectName("itunes_store_detail_title")
        title_label.setWordWrap(True)
        artist_label = QLabel(artist)
        artist_label.setObjectName("itunes_store_detail_artist")
        artist_label.setWordWrap(True)
        info.addWidget(title_label)
        info.addWidget(artist_label)

        meta_bits = [source]
        if date:
            meta_bits.append(date[:4])
        if tracks:
            meta_bits.append(f"{tracks} tracks")
        meta = QLabel(" - ".join(meta_bits))
        meta.setObjectName("itunes_store_detail_meta")
        info.addWidget(meta)

        table = QFrame()
        table.setObjectName("itunes_store_detail_table")
        table_layout = QVBoxLayout(table)
        table_layout.setContentsMargins(0, 0, 0, 0)
        table_layout.setSpacing(0)
        rows = [
            ("Album", title),
            ("Artist", artist),
            ("Source", source),
            ("Released", date[:10] if date else ""),
            ("Tracks", str(tracks) if tracks else ""),
        ]
        for label, value in rows:
            if not value:
                continue
            row = QFrame()
            row.setObjectName("itunes_store_detail_row")
            row_layout = QHBoxLayout(row)
            row_layout.setContentsMargins(7, 3, 7, 3)
            name = QLabel(label)
            name.setObjectName("itunes_store_detail_key")
            value_label = QLabel(value)
            value_label.setObjectName("itunes_store_detail_value")
            value_label.setWordWrap(True)
            row_layout.addWidget(name)
            row_layout.addWidget(value_label, 1)
            table_layout.addWidget(row)
        info.addWidget(table)

        track_table = QFrame()
        track_table.setObjectName("itunes_store_detail_table")
        track_layout = QVBoxLayout(track_table)
        track_layout.setContentsMargins(0, 0, 0, 0)
        track_layout.setSpacing(0)
        header = QFrame()
        header.setObjectName("itunes_store_detail_row")
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(7, 3, 7, 3)
        header_number = QLabel("#")
        header_number.setObjectName("itunes_store_detail_key")
        header_title = QLabel("Track")
        header_title.setObjectName("itunes_store_detail_key")
        header_duration = QLabel("Time")
        header_duration.setObjectName("itunes_store_detail_key")
        header_layout.addWidget(header_number)
        header_layout.addWidget(header_title, 1)
        header_layout.addWidget(header_duration)
        track_layout.addWidget(header)
        if track_items:
            multi_disc = len({self._store_result_int(track.get("disc_number") or 1, 1) for track in track_items}) > 1
            for index, track in enumerate(track_items, start=1):
                track_number = self._store_result_int(
                    track.get("track_number") or track.get("number") or track.get("trackNumber") or index,
                    index,
                )
                disc_number = self._store_result_int(
                    track.get("disc_number") or track.get("volume_number") or track.get("disc") or 1,
                    1,
                )
                number_text = f"{disc_number}-{track_number:02d}" if multi_disc else f"{track_number:02d}"
                track_title = str(track.get("title") or track.get("name") or "Untitled Track")
                track_artist = str(track.get("artist") or "")
                if track_artist and track_artist != artist:
                    track_title = f"{track_title} - {track_artist}"
                duration = self._format_track_duration(track.get("duration") or track.get("duration_seconds"))
                track_url = str(track.get("url") or "")
                track_owned = bool(track.get("owned") or track.get("in_library"))
                row = QFrame()
                row.setObjectName("itunes_store_detail_row")
                row_layout = QHBoxLayout(row)
                row_layout.setContentsMargins(7, 3, 7, 3)
                number = QLabel(number_text)
                number.setObjectName("itunes_store_detail_key")
                name = QLabel(track_title)
                name.setObjectName("itunes_store_detail_value")
                name.setWordWrap(True)
                time_label = QLabel(duration)
                time_label.setObjectName("itunes_store_detail_key")
                buy_track_btn = QPushButton("Owned" if track_owned else "Buy")
                buy_track_btn.setObjectName("store_buy_button")
                buy_track_btn.setEnabled(bool(track_url) and not track_owned)
                buy_track_btn.clicked.connect(lambda _checked=False, item=dict(track): self._emit_store_result_import(item))
                row_layout.addWidget(number)
                row_layout.addWidget(name, 1)
                row_layout.addWidget(time_label)
                row_layout.addWidget(buy_track_btn)
                track_layout.addWidget(row)
        else:
            row = QFrame()
            row.setObjectName("itunes_store_detail_row")
            row_layout = QHBoxLayout(row)
            row_layout.setContentsMargins(7, 3, 7, 3)
            loading = QLabel("Loading track list...")
            loading.setObjectName("itunes_store_detail_value")
            row_layout.addWidget(loading, 1)
            track_layout.addWidget(row)
        info.addWidget(track_table)

        buy_row = QHBoxLayout()
        owned = bool(result.get("owned") or result.get("in_library"))
        buy_btn = QPushButton("Owned" if owned else "Buy Album")
        buy_btn.setObjectName("store_buy_button")
        buy_btn.setEnabled(bool(url) and not owned)
        buy_btn.clicked.connect(lambda _checked=False, item=dict(result): self._emit_store_result_import(item))
        buy_row.addWidget(buy_btn)
        buy_row.addStretch(1)
        info.addLayout(buy_row)
        info.addStretch(1)
        body.addLayout(info, 1)
        panel_layout.addLayout(body)
        return panel

    def set_home_url(self, url):
        self._home_url = self._normalize_url(url)
        self._url_edit.setText(self._home_url)
        if self._web is not None and not self._web.url().isValid():
            self._web.setUrl(QUrl(self._home_url))

    def set_download_directory(self, path):
        self._download_dir = os.path.abspath(path) if path else ""

    def set_store_context(self, home_url, download_dir):
        self.set_home_url(home_url)
        self.set_download_directory(download_dir)

    def set_store_import_status(self, status, running=False):
        text = str(status or "")
        self._store_status.setText(text)
        self._store_status.setVisible(bool(text))
        self._store_import_btn.setEnabled(not running)
        self._store_url_edit.setEnabled(not running)
        self._store_format.setEnabled(not running)

    def set_store_search_status(self, status, running=False):
        text = str(status or "")
        self._store_status.setText(text)
        self._store_status.setVisible(bool(text))
        self._store_search_btn.setEnabled(not running)
        self._store_search_edit.setEnabled(not running)
        self._store_source.setEnabled(not running)

    def set_store_results(self, results, header_text="New Music"):
        grid = getattr(self, "_store_results_grid", None)
        if grid is None:
            return
        self._store_results = [dict(item or {}) for item in (results or [])]
        self._store_results_header_text = str(header_text or "New Music")
        self._render_store_results()

    def set_store_home_results(self, results, tab_key=None):
        if tab_key:
            self.set_store_home_tab(tab_key, emit=False)
        items = [dict(item or {}) for item in (results or [])]
        self._update_store_home_chrome(items)
        self.set_store_results(items, self._store_home_header_text())

    def _store_home_tab_label(self):
        labels = dict(self.STORE_HOME_TABS)
        return labels.get(self._store_home_tab, "Featured")

    def _store_home_header_text(self):
        label = self._store_home_tab_label()
        if self._store_home_tab == "featured":
            return "Popular and New on TIDAL"
        return f"{label} on TIDAL"

    def _render_store_results(self):
        grid = getattr(self, "_store_results_grid", None)
        if grid is None:
            return
        self._clear_store_results_grid()
        if hasattr(self, "_store_results_header"):
            self._store_results_header.setText(getattr(self, "_store_results_header_text", "New Music"))
        items = list(self._store_results or [])
        if not items:
            placeholder = QLabel("Search the store to browse albums.")
            placeholder.setObjectName("itunes_store_subhead")
            placeholder.setAlignment(Qt.AlignCenter)
            grid.addWidget(placeholder, 0, 0, 1, 3)
            return
        for index, result in enumerate(items):
            grid.addWidget(self._store_result_tile(result), index // 3, index % 3)

    def _update_store_home_chrome(self, results):
        label = self._store_home_tab_label()
        if hasattr(self, "_store_hero_kicker"):
            self._store_hero_kicker.setText(f"TIDAL Store - {label}")
        if hasattr(self, "_store_hero_headline"):
            headline = self._store_home_header_text()
            if results:
                headline = str(results[0].get("section") or headline)
            self._store_hero_headline.setText(headline)
        if hasattr(self, "_store_hero_subhead"):
            self._store_hero_subhead.setText(f"{label} albums from TIDAL with one-click Buy imports.")
        for index, cover in enumerate(getattr(self, "_store_hero_covers", [])):
            result = results[index] if index < len(results) else {}
            if result:
                self._set_store_cover(cover, result, 86)
            else:
                cover.setText("")
                cover.setPixmap(QPixmap())
        for index, button in enumerate(getattr(self, "_store_chart_buttons", []), start=1):
            result = results[index - 1] if index - 1 < len(results) else {}
            title = str(result.get("title") or "")
            artist = str(result.get("artist") or "")
            if title:
                button.setText(f"{index}. {title}")
                button.clicked.disconnect()
                button.clicked.connect(lambda _checked=False, item=dict(result): self._open_store_result_details(item))
                button.setToolTip(artist)

    def _restore_store_results(self):
        self._store_detail_result_key = ""
        self._render_store_results()

    def _open_store_result_details(self, result):
        self.show_store_result_details(result)
        if not self._store_result_track_items(result):
            self.store_album_details_requested.emit(dict(result or {}))

    def _clear_store_results_grid(self):
        grid = getattr(self, "_store_results_grid", None)
        if grid is None:
            return
        while grid.count():
            item = grid.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()

    def begin_store_import_download(self, url, output_dir):
        self._store_import_items = {}
        label = str(url or "").strip() or "Store import"
        self._active_store_import_item = QTreeWidgetItem(
            [
                label,
                "Starting",
                "0 tracks",
                os.path.abspath(str(output_dir or "")),
            ]
        )
        self._downloads.insertTopLevelItem(0, self._active_store_import_item)
        self._resize_download_columns()

    def update_store_import_downloads(self, audio_files, running=True):
        unique_files = sorted(set(str(path) for path in audio_files if path))
        for path in unique_files:
            display = os.path.basename(path)
            folder = os.path.dirname(path)
            item = self._store_import_items.get(path)
            if item is None:
                item = QTreeWidgetItem([display, "Downloading", "On disk", folder])
                insert_at = len(self._store_import_items)
                if self._active_store_import_item is not None:
                    insert_at += 1
                self._downloads.insertTopLevelItem(insert_at, item)
                self._store_import_items[path] = item
            item.setText(0, display)
            item.setText(1, "Downloading" if running else "Completed")
            item.setText(2, "On disk" if running else "100%")
            item.setText(3, folder)

        if self._active_store_import_item is not None:
            count = len(unique_files)
            suffix = "track" if count == 1 else "tracks"
            self._active_store_import_item.setText(1, "Downloading" if running else "Completed")
            self._active_store_import_item.setText(2, f"{count} {suffix}")
        self._resize_download_columns()

    def finish_store_import_downloads(self, audio_files, success=True):
        self.update_store_import_downloads(audio_files, running=False)
        if self._active_store_import_item is not None:
            count = len(set(str(path) for path in audio_files if path))
            if success and count:
                status = "Completed"
            elif success:
                status = "No Files"
            else:
                status = "Failed"
            suffix = "track" if count == 1 else "tracks"
            self._active_store_import_item.setText(1, status)
            self._active_store_import_item.setText(2, f"{count} {suffix}")
        self._resize_download_columns()

    def set_store_preferred_format(self, output_format):
        wanted = str(output_format or "flac").lower()
        index = self._store_format.findData(wanted)
        self._store_format.setCurrentIndex(index if index >= 0 else 0)

    def search_store(self, query):
        self._store_search_edit.setText(str(query or ""))
        self._emit_store_search()

    def set_auto_accept_cookies(self, enabled):
        self._auto_accept_cookies = bool(enabled)

    def go_home(self):
        if not self._home_url:
            return
        if self._web is not None:
            self._web.setUrl(QUrl(self._home_url))
        self._url_edit.setText(self._home_url)

    def go_back(self):
        if self._web is not None:
            self._web.back()

    def go_forward(self):
        if self._web is not None:
            self._web.forward()

    def reload(self):
        if self._web is not None:
            self._web.reload()

    def current_url(self):
        if self._web is not None and self._web.url().isValid():
            return self._web.url().toString()
        return self._normalize_url(self._url_edit.text())

    def _go_to_entered_url(self):
        url = self._normalize_url(self._url_edit.text())
        self._url_edit.setText(url)
        if self._web is not None:
            self._web.setUrl(QUrl(url))

    def _on_url_changed(self, url):
        self._url_edit.setText(url.toString())

    def _on_load_finished(self, ok):  # pragma: no cover - depends on Qt WebEngine runtime
        if not ok or self._web is None or not self._auto_accept_cookies:
            return
        current = self.current_url()
        if not current or not self._home_url:
            return
        current_host = urlparse(current).netloc.lower()
        home_host = urlparse(self._home_url).netloc.lower()
        if not current_host or current_host != home_host or current_host in self._cookie_accept_attempted_hosts:
            return
        self._cookie_accept_attempted_hosts.add(current_host)
        self._web.page().runJavaScript(_COOKIE_CONSENT_SCRIPT)

    def _emit_open_external(self):
        self.open_external_requested.emit(self.current_url() or self._home_url)

    def _emit_store_import(self):
        url = self._store_url_edit.text().strip() or self.current_url()
        self._emit_store_import_url(url)

    def _emit_store_import_url(self, url):
        output_format = self._store_format.currentData() or "flac"
        self.store_import_requested.emit(url, output_format)

    def _emit_store_result_import(self, result):
        output_format = self._store_format.currentData() or "flac"
        self.store_result_import_requested.emit(dict(result or {}), output_format)

    def _emit_store_search(self):
        query = self._store_search_edit.text().strip()
        source = self._store_source.currentData() or "tidal"
        self.store_search_requested.emit(query, source)

    def _on_download_requested(self, request):  # pragma: no cover - depends on Qt WebEngine runtime
        if not self._download_dir:
            return
        os.makedirs(self._download_dir, exist_ok=True)
        suggested = request.downloadFileName() if hasattr(request, "downloadFileName") else ""
        if not suggested and hasattr(request, "suggestedFileName"):
            suggested = request.suggestedFileName()
        if hasattr(request, "setDownloadDirectory"):
            request.setDownloadDirectory(self._download_dir)
        if hasattr(request, "setDownloadFileName") and suggested:
            request.setDownloadFileName(suggested)

        item = QTreeWidgetItem(
            [
                suggested or "download",
                "Starting",
                "0%",
                self._download_dir,
            ]
        )
        self._downloads.setVisible(True)
        self._downloads.insertTopLevelItem(0, item)
        self._download_items[id(request)] = item
        if hasattr(request, "receivedBytesChanged"):
            request.receivedBytesChanged.connect(lambda r=request: self._update_download_item(r))
        if hasattr(request, "totalBytesChanged"):
            request.totalBytesChanged.connect(lambda r=request: self._update_download_item(r))
        if hasattr(request, "stateChanged"):
            request.stateChanged.connect(lambda _state, r=request: self._update_download_item(r))
        if hasattr(request, "isFinishedChanged"):
            request.isFinishedChanged.connect(lambda r=request: self._update_download_item(r))
        request.accept()
        self._update_download_item(request)

    def _update_download_item(self, request):  # pragma: no cover - depends on Qt WebEngine runtime
        item = self._download_items.get(id(request))
        if item is None:
            return
        received = request.receivedBytes() if hasattr(request, "receivedBytes") else 0
        total = request.totalBytes() if hasattr(request, "totalBytes") else 0
        if total and total > 0:
            progress = f"{int((received / total) * 100)}%"
        elif received > 0:
            progress = f"{received} bytes"
        else:
            progress = "0%"
        state_text = "Downloading"
        if hasattr(request, "state"):
            state = request.state()
            state_name = getattr(state, "name", str(state))
            if "Completed" in state_name:
                state_text = "Completed"
            elif "Cancelled" in state_name:
                state_text = "Cancelled"
            elif "Interrupted" in state_name:
                state_text = "Failed"
        item.setText(1, state_text)
        item.setText(2, progress)
        item.setText(3, self._download_dir)
        if state_text == "Completed":
            self._finalize_download(request, item)
        self._resize_download_columns()

    def _finalize_download(self, request, item):  # pragma: no cover - depends on Qt WebEngine runtime
        request_id = id(request)
        if request_id in self._finalized_downloads:
            return
        self._finalized_downloads.add(request_id)
        file_name = item.text(0)
        archive_path = os.path.join(self._download_dir, file_name)
        if not _is_supported_archive(archive_path) or not os.path.exists(archive_path):
            return
        item.setText(1, "Extracting")
        try:
            extracted = extract_downloaded_archive(archive_path, self._download_dir)
        except Exception:
            item.setText(1, "Extract Failed")
            return
        if extracted:
            os.remove(archive_path)
            item.setText(1, f"Extracted ({len(extracted)})")
            item.setText(2, "100%")
        else:
            item.setText(1, "Completed")

    def _resize_download_columns(self):
        for column in range(4):
            self._downloads.resizeColumnToContents(column)

    @staticmethod
    def _normalize_url(url):
        text = str(url or "").strip()
        if not text:
            return "https://www.rockbox.org/"
        if "://" not in text:
            return f"https://{text}"
        return text


class MovieStorePanel(QWidget):
    movie_import_requested = Signal(str)
    movie_browse_requested = Signal(str)

    MOVIE_BROWSE_TABS = [
        ("featured", "Featured", "public domain full movies"),
        ("new", "New Releases", "creative commons short film"),
        ("trailers", "Trailers", "movie trailers"),
        ("documentaries", "Documentaries", "public domain documentary"),
        ("music", "Music Videos", "official music video"),
        ("animation", "Animation", "public domain animation"),
    ]

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("browser_panel")
        self._movie_item = None
        self._movie_results = []
        self._movie_browse_buttons = {}

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 8, 10, 10)
        layout.setSpacing(7)

        nav = QFrame()
        nav.setObjectName("itunes_store_nav")
        nav_layout = QHBoxLayout(nav)
        nav_layout.setContentsMargins(9, 5, 9, 5)
        title = QLabel("Movies")
        title.setObjectName("itunes_store_title")
        nav_layout.addWidget(title)
        nav_layout.addStretch(1)
        layout.addWidget(nav)

        tab_bar = QFrame()
        tab_bar.setObjectName("itunes_store_tabs")
        tab_layout = QHBoxLayout(tab_bar)
        tab_layout.setContentsMargins(9, 4, 9, 4)
        tab_layout.setSpacing(4)
        for key, label, query in self.MOVIE_BROWSE_TABS:
            button = QPushButton(label)
            button.setObjectName("store_nav_button")
            button.setCheckable(True)
            button.clicked.connect(lambda _checked=False, tab_key=key, text=query: self._emit_movie_browse_tab(tab_key, text))
            tab_layout.addWidget(button)
            self._movie_browse_buttons[key] = button
        tab_layout.addStretch(1)
        layout.addWidget(tab_bar)
        self.set_movie_browse_tab("featured")

        hero = QFrame()
        hero.setObjectName("itunes_store_hero")
        hero_layout = QVBoxLayout(hero)
        hero_layout.setContentsMargins(14, 12, 14, 12)
        kicker = QLabel("YouTube Movies")
        kicker.setObjectName("itunes_store_kicker")
        headline = QLabel("Browse videos to download and convert")
        headline.setObjectName("itunes_store_headline")
        subhead = QLabel("Browse or search YouTube videos you own or have permission to download.")
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
        self._movie_browse_edit = QLineEdit()
        self._movie_browse_edit.setObjectName("itunes_store_import_url")
        self._movie_browse_edit.setPlaceholderText("Search YouTube movies")
        self._movie_browse_edit.returnPressed.connect(self._emit_movie_browse)
        self._movie_browse_btn = QPushButton("Search Movies")
        self._movie_browse_btn.setObjectName("store_buy_button")
        self._movie_browse_btn.clicked.connect(self._emit_movie_browse)
        browse_layout.addWidget(browse_label)
        browse_layout.addWidget(self._movie_browse_edit, 1)
        browse_layout.addWidget(self._movie_browse_btn)
        layout.addWidget(browse_bar)

        import_bar = QFrame()
        import_bar.setObjectName("itunes_store_import_bar")
        import_layout = QHBoxLayout(import_bar)
        import_layout.setContentsMargins(9, 6, 9, 6)
        import_layout.setSpacing(6)
        label = QLabel("YouTube URL")
        label.setObjectName("itunes_store_small_title")
        self._movie_url_edit = QLineEdit()
        self._movie_url_edit.setObjectName("itunes_store_import_url")
        self._movie_url_edit.setPlaceholderText("https://www.youtube.com/watch?v=...")
        self._movie_url_edit.returnPressed.connect(self._emit_movie_import)
        self._movie_import_btn = QPushButton("Add Movie")
        self._movie_import_btn.setObjectName("store_buy_button")
        self._movie_import_btn.clicked.connect(self._emit_movie_import)
        import_layout.addWidget(label)
        import_layout.addWidget(self._movie_url_edit, 1)
        import_layout.addWidget(self._movie_import_btn)
        layout.addWidget(import_bar)

        browse_scroll = QScrollArea()
        browse_scroll.setObjectName("itunes_store_scroll")
        browse_scroll.setWidgetResizable(True)
        browse_scroll.setFrameShape(QFrame.NoFrame)
        browse_page = QWidget()
        browse_page.setObjectName("itunes_store_page")
        browse_page_layout = QVBoxLayout(browse_page)
        browse_page_layout.setContentsMargins(0, 0, 0, 0)
        browse_page_layout.setSpacing(7)
        browse_panel = QFrame()
        browse_panel.setObjectName("itunes_store_panel")
        browse_panel_layout = QVBoxLayout(browse_panel)
        browse_panel_layout.setContentsMargins(8, 7, 8, 8)
        browse_panel_layout.setSpacing(7)
        self._movie_results_header = QLabel("Featured Movies")
        self._movie_results_header.setObjectName("itunes_store_section_title")
        browse_panel_layout.addWidget(self._movie_results_header)
        self._movie_results_grid = QGridLayout()
        self._movie_results_grid.setHorizontalSpacing(10)
        self._movie_results_grid.setVerticalSpacing(8)
        browse_panel_layout.addLayout(self._movie_results_grid)
        browse_page_layout.addWidget(browse_panel)
        browse_scroll.setWidget(browse_page)
        layout.addWidget(browse_scroll, 1)

        self._movie_status = QLabel("")
        self._movie_status.setObjectName("theme_hub_status")
        self._movie_status.setWordWrap(True)
        self._movie_status.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self._movie_status.setVisible(False)
        layout.addWidget(self._movie_status)

        self._movie_downloads = QTreeWidget()
        self._movie_downloads.setObjectName("itunes_store_downloads")
        self._movie_downloads.setHeaderLabels(["Movie", "Status", "Progress", "Folder"])
        self._movie_downloads.setRootIsDecorated(False)
        self._movie_downloads.setMinimumHeight(96)
        self._movie_downloads.setMaximumHeight(150)
        layout.addWidget(self._movie_downloads)
        self.set_movie_results([])

    def _emit_movie_import(self):
        url = self._movie_url_edit.text().strip()
        self.movie_import_requested.emit(url)

    def _emit_movie_browse(self):
        query = self._movie_browse_edit.text().strip()
        self.movie_browse_requested.emit(query)

    def _emit_movie_browse_tab(self, key, query):
        self.set_movie_browse_tab(key)
        self._movie_browse_edit.setText(str(query or ""))
        self.movie_browse_requested.emit(str(query or ""))

    def set_movie_browse_tab(self, key):
        wanted = str(key or "featured")
        for name, button in self._movie_browse_buttons.items():
            active = name == wanted
            button.setChecked(active)
            button.setProperty("active", active)
            button.style().unpolish(button)
            button.style().polish(button)

    def set_movie_browse_status(self, status, running=False):
        self.set_movie_import_status(status, running=False)
        self._movie_browse_btn.setEnabled(not running)
        self._movie_browse_edit.setEnabled(not running)

    def set_movie_results(self, results, header_text="Featured Movies"):
        self._movie_results = [dict(item or {}) for item in (results or [])]
        self._movie_results_header.setText(str(header_text or "Featured Movies"))
        self._render_movie_results()

    def _render_movie_results(self):
        while self._movie_results_grid.count():
            item = self._movie_results_grid.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()
        if not self._movie_results:
            placeholder = QLabel("Browse or search for authorized YouTube videos.")
            placeholder.setObjectName("itunes_store_subhead")
            placeholder.setAlignment(Qt.AlignCenter)
            self._movie_results_grid.addWidget(placeholder, 0, 0, 1, 3)
            return
        for index, result in enumerate(self._movie_results):
            self._movie_results_grid.addWidget(self._movie_result_tile(result), index // 3, index % 3)

    def _movie_result_tile(self, result):
        title = str(result.get("title") or "YouTube Movie")
        uploader = str(result.get("uploader") or "YouTube")
        duration = str(result.get("duration_text") or "")
        url = str(result.get("url") or "")

        tile = QFrame()
        tile.setObjectName("itunes_store_album_tile")
        tile_layout = QVBoxLayout(tile)
        tile_layout.setContentsMargins(4, 4, 4, 6)
        tile_layout.setSpacing(4)

        cover = QLabel(title[:2].upper())
        cover.setObjectName("itunes_store_album_art")
        cover.setAlignment(Qt.AlignCenter)
        cover.setFixedSize(82, 82)
        title_label = QLabel(title)
        title_label.setObjectName("itunes_store_album_title")
        title_label.setWordWrap(True)
        uploader_label = QLabel(uploader)
        uploader_label.setObjectName("itunes_store_album_subtitle")
        detail_label = QLabel(duration or "YouTube")
        detail_label.setObjectName("itunes_store_album_subtitle")
        buy_btn = QPushButton("Buy")
        buy_btn.setObjectName("store_buy_button")
        buy_btn.setEnabled(bool(url))
        buy_btn.clicked.connect(lambda _checked=False, item=dict(result): self.movie_import_requested.emit(str(item.get("url") or "")))

        tile_layout.addWidget(cover, alignment=Qt.AlignHCenter)
        tile_layout.addWidget(title_label)
        tile_layout.addWidget(uploader_label)
        tile_layout.addWidget(detail_label)
        tile_layout.addWidget(buy_btn)
        return tile

    def set_movie_import_status(self, status, running=False):
        text = str(status or "")
        self._movie_status.setText(text)
        self._movie_status.setVisible(bool(text))
        self._movie_url_edit.setEnabled(not running)
        self._movie_import_btn.setEnabled(not running)

    def begin_movie_import(self, url, output_dir):
        label = str(url or "").strip() or "YouTube movie"
        self._movie_item = QTreeWidgetItem([label, "Starting", "0%", os.path.abspath(str(output_dir or ""))])
        self._movie_downloads.insertTopLevelItem(0, self._movie_item)
        self._resize_movie_columns()

    def update_movie_import(self, status, progress=""):
        if self._movie_item is not None:
            self._movie_item.setText(1, str(status or "Working"))
            if progress:
                self._movie_item.setText(2, str(progress))
        self._resize_movie_columns()

    def finish_movie_import(self, output_path="", success=True):
        if self._movie_item is not None:
            self._movie_item.setText(1, "Completed" if success else "Failed")
            self._movie_item.setText(2, "100%" if success else "Stopped")
            if output_path:
                self._movie_item.setText(0, os.path.basename(output_path))
                self._movie_item.setText(3, os.path.dirname(output_path))
        self._resize_movie_columns()

    def _resize_movie_columns(self):
        for column in range(4):
            self._movie_downloads.resizeColumnToContents(column)


_COOKIE_CONSENT_SCRIPT = r"""
(function() {
  const acceptWords = [
    'accept', 'agree', 'allow all', 'accept all', 'ok', 'got it',
    'i agree', 'yes, i agree', 'accept cookies', 'allow cookies'
  ];
  const selectors = [
    'button', 'a', '[role="button"]', 'input[type="button"]', 'input[type="submit"]'
  ];
  function isVisible(el) {
    const rect = el.getBoundingClientRect();
    const style = window.getComputedStyle(el);
    return rect.width > 0 && rect.height > 0 && style.visibility !== 'hidden' && style.display !== 'none';
  }
  function textFor(el) {
    return ((el.innerText || el.value || el.getAttribute('aria-label') || '') + '').trim().toLowerCase();
  }
  for (const selector of selectors) {
    const nodes = document.querySelectorAll(selector);
    for (const node of nodes) {
      const text = textFor(node);
      if (!text || !isVisible(node)) continue;
      if (acceptWords.some(word => text === word || text.includes(word))) {
        node.click();
        return 'clicked';
      }
    }
  }
  return 'no-match';
})();
"""
