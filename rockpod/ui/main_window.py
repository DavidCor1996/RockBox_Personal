"""Main application window — ties together all UI components in an iTunes 7 layout."""

import logging
import errno
import base64
import hashlib
import json
import os
import shlex
import secrets
import shutil
import signal
import time
from datetime import datetime

from PySide6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QSplitter,
    QApplication, QMessageBox, QInputDialog, QLineEdit, QMenu, QStackedWidget, QFileDialog,
    QProgressDialog, QTabWidget,
)
from PySide6.QtCore import QEventLoop, QItemSelectionModel, Qt, QTimer, Slot, QProcess
from PySide6.QtGui import QAction, QIcon

from app.config import Config
from app.database import Database
from services.library_scanner import LibraryScanner
from services.metadata_reader import read_metadata_details, compute_file_hash
from services.metadata_writer import (
    FILE_TAG_METADATA_FIELDS,
    MetadataWriteError,
    write_track_metadata_to_file,
)
from services.reconciliation import (
    build_reconciliation_report,
    build_missing_tag_report,
    format_reconciliation_report,
    format_missing_tag_report,
)
from services.artwork_manager import ArtworkManager
from services.device_storage import DeviceStorageAnalyzer
from services.device_detector import DeviceDetector, create_mock_device
from services.device_inventory import DeviceInventoryVerifier, device_record_from_info
from services.android_media import AndroidMediaImporter, discover_android_sources
from services.streamrip_import import (
    StreamripImporter,
    StreamripImportError,
    discover_imported_audio_files,
    ensure_rockbox_cover_files,
    find_existing_streamrip_source_file,
    streamrip_url_info,
)
from services.music_sharing import MusicSharingService, MusicSharingError
from services.youtube_movies import (
    YoutubeMovieImporter,
    YoutubeMovieImportError,
    existing_movie_duplicate,
    parse_movie_import_progress,
    persist_movie_import_poster,
)
from services.playback import PlaybackService, STATE_STOPPED
from services.sync_engine import SyncEngine, SyncPlanBuilder
from services.theme_assets import ThemeAssetManager
from services.video_thumbnails import VideoThumbnailService
from services.smart_playlists import ensure_default_smart_playlists, evaluate_playlist
from services.smart_playlists import evaluate_playlist_count
from services.rockbox_device import (
    clear_rockbox_database_cache,
    detect_rockbox_database_state,
    enable_rockbox_tagcache_autoupdate,
    invalidate_pictureflow_cache,
    set_rockbox_applications_menu,
    set_rockbox_ui_accent,
    set_rockbox_ui_density,
    set_rockbox_ui_dark_mode,
    set_rockbox_ui_engine,
    set_rockbox_ui_font_scale,
    set_rockbox_ui_hold_effect,
    set_rockbox_ui_surface,
)
from services.rockbox_deploy import RockboxDeployService
from services.rockbox_boot import RockboxBootService
from services.rockbox_games import RockboxGameService
from services.rockbox_photos import RockboxPhotoService
from services.ipone_wallpapers import IPoneWallpaperService
from services.rockbox_profiles import RockboxProfileStore
from services.rockbox_plugins import RockboxPluginService
from services.rockbox_runtime import import_runtime_data_for_device
from services.rockbox_playlists import export_device_playlists, export_local_music_playlists
from services.rockbox_tagcache import TagcacheError, write_rockbox_tagcache_from_device_inventory
from services.rockbox_simulator import RockboxSimulatorService
from services.rockbox_themes import RockboxThemeService, THEME_DEFINITIONS
from services.linux_payload import DEBIAN_LIVE_XFCE_ISO, LinuxPayloadService
from services.theme_designer import ThemeDesignerService
from services.online_album_metadata import AlbumMetadataFetcher
from ui.sidebar import Sidebar
from ui.toolbar import Toolbar
from ui.track_table import TrackTable, TrackTableModel, TRACK_DATA_ROLE
from ui.status_bar import StatusBar
from ui.device_summary import DeviceSummaryWidget, build_summary_data
from ui.column_browser import ColumnBrowser
from ui.library_views import (
    GroupedTrackView, AlbumGridView,
    group_tracks_by_artist, group_tracks_by_album, group_tracks_by_genre,
    filter_tracks_for_search, summarize_tracks,
)
from ui.track_adapter import normalize_track_for_ui, normalize_tracks_for_ui
from ui.simulator_panel import SimulatorPanel
from ui.plugin_manager import PluginManagerWidget
from ui.game_manager import GameManagerWidget
from ui.photo_manager import PhotoManagerWidget
from ui.ipone_wallpaper_manager import IPoneWallpaperManagerWidget
from ui.linux_manager import LinuxInstallProgressDialog, LinuxManagerWidget
from ui.web_browser import BrowserPanel, MovieStorePanel, MusicSharingPanel
from ui.boot_manager import BootManagerWidget
from ui.theme_hub import ThemeHubWidget
from ui.theme_designer import ThemeDesignerWidget
from ui.ipodjs_engine_designer import IPodJSEngineDesignerWidget
from ui.video_library import VideoGridView, build_video_browser_groups
from ui.video_player import VideoPlayerWindow
from ui.video_sync import VideoSyncPanel
from ui.android_workflows import summarize_android_import
from ui.boot_workflows import BootProgressController
from ui.store_workflows import (
    store_import_empty_message,
    store_import_failure_message,
    store_import_success_message,
)
from ui.process_helpers import (
    compact_process_text,
    create_child_process,
    process_output_lines,
    read_process_text,
    start_detached_command,
)
from ui.dialogs.metadata_editor import MetadataEditor
from ui.dialogs.album_info import AlbumInfoDialog
from ui.dialogs.album_metadata import AlbumMetadataDialog
from ui.dialogs.device_settings import DeviceSettingsDialog
from ui.dialogs.preferences import PreferencesDialog
from ui.dialogs.sync_dialog import SyncDialog
from services.file_safety import atomic_write_text

logger = logging.getLogger(__name__)


class MainWindow(QMainWindow):
    """RockPod main window — iTunes 7-era layout and behavior."""

    def __init__(self, config=None):
        super().__init__()

        # Core services
        self._config = config or Config()
        self._config.ensure_dirs()
        self._show_hidden_wallpapers = False
        self._db_was_missing = not os.path.exists(self._config.db_path)
        self._db = Database(self._config.db_path)
        self._artwork = ArtworkManager(self._config.artwork_cache_dir, self._config, self)
        self._video_thumbnails = VideoThumbnailService(
            self._config.artwork_cache_dir,
            self._config,
            self._artwork,
        )
        self._android_importer = AndroidMediaImporter(self)
        self._streamrip_importer = StreamripImporter(self._config)
        self._music_sharing = MusicSharingService(self._config)
        self._youtube_movie_importer = YoutubeMovieImporter(self._config)
        self._scanner = LibraryScanner(self._db, self._config)
        self._device_detector = DeviceDetector(self._config)
        self._device_storage_analyzer = DeviceStorageAnalyzer(self)
        self.destroyed.connect(lambda: self._device_storage_analyzer.shutdown())
        self._sync_engine = SyncEngine(self._db, self._config, self._device_detector, self._artwork)
        self._playback = PlaybackService(parent=self)
        self._device_inventory = DeviceInventoryVerifier(self._config, self)
        self._theme_assets = ThemeAssetManager(self._config)
        self._repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        self._rockbox_profiles = RockboxProfileStore(self._config, self._repo_root)
        self._rockbox_themes = RockboxThemeService()
        self._theme_designer_service = ThemeDesignerService(self._rockbox_themes)
        self._rockbox_deploy = RockboxDeployService()
        self._rockbox_boot = RockboxBootService()
        self._rockbox_games = RockboxGameService()
        self._rockbox_photos = RockboxPhotoService()
        self._ipone_wallpapers_service = IPoneWallpaperService()
        self._rockbox_plugins = RockboxPluginService()
        self._rockbox_simulator = RockboxSimulatorService()
        self._linux_payload = LinuxPayloadService()
        self._album_metadata_fetcher = AlbumMetadataFetcher(self._config, self)

        self._current_view = "library_music"
        self._current_filter_genre = ""
        self._current_filter_artist = ""
        self._current_filter_album = ""
        self._sync_dialog = None  # active sync dialog, if any
        self._active_sync_plan = None
        self._last_sync_time = None
        self._device_verification_running = False
        self._current_playing_track_id = None
        self._device_state = "Disconnected"
        self._rockbox_db_stale = False
        self._runtime_refresh_pending = False
        self._device_storage_cache = {}
        self._rockbox_db_monitor = QTimer(self)
        self._rockbox_db_monitor.setInterval(2000)
        self._rockbox_db_monitor.timeout.connect(self._poll_rockbox_database_state)
        self._current_theme_diff = None
        self._current_designer_variant = None
        self._current_designer_draft = None
        self._current_designer_preview_path = ""
        self._pending_designer_preview_variant = None
        self._pending_designer_preview_mode = ""
        self._designer_preview_running = False
        self._designer_preview_rerun_requested = False
        self._designer_preview_timer = QTimer(self)
        self._designer_preview_timer.setSingleShot(True)
        self._designer_preview_timer.setInterval(100)
        self._designer_preview_timer.timeout.connect(self._apply_theme_designer_preview_to_simulator)
        self._current_boot_diff = None
        self._current_boot_target_mode = "device"
        self._current_plugin_diff = None
        self._current_plugin_target_mode = "device"
        self._current_plugin_id = ""
        self._current_game_diff = None
        self._current_game_target_mode = "device"
        self._current_photo_diff = None
        self._current_photo_target_mode = "device"
        self._current_simulator_diff = None
        self._simulator_targets = []
        self._rockbox_db_update_started_at = None
        self._rockbox_db_feedback_tick = 0
        self._scan_ui_refresh_pending = False
        self._album_metadata_progress = None
        self._pending_album_metadata = {}
        self._scan_ui_refresh_timer = QTimer(self)
        self._scan_ui_refresh_timer.setSingleShot(True)
        self._scan_ui_refresh_timer.setInterval(120)
        self._scan_ui_refresh_timer.timeout.connect(self._flush_scan_ui_refresh)
        self._android_import_progress = None
        self._store_import_process = None
        self._store_import_started_at = 0.0
        self._store_import_output_dir = ""
        self._store_import_output = []
        self._store_import_log_path = ""
        self._store_import_context = None
        self._store_import_panel = None
        self._pending_store_playlist_import = None
        self._store_import_poll_timer = QTimer(self)
        self._store_import_poll_timer.setInterval(750)
        self._store_import_poll_timer.timeout.connect(self._poll_store_import_downloads)
        self._store_search_process = None
        self._store_search_output_path = ""
        self._store_search_output = []
        self._store_search_mode = ""
        self._store_search_home_tab = "featured"
        self._store_home_loaded_tabs = set()
        self._store_detail_process = None
        self._store_detail_output_path = ""
        self._store_detail_output = []
        self._movie_import_process = None
        self._movie_import_output = []
        self._movie_import_log_path = ""
        self._movie_import_result = {}
        self._linux_cache_dir_override = None
        self._movie_browse_process = None
        self._movie_browse_output_path = ""
        self._movie_browse_output = []
        self._movie_browse_query = ""
        self._movie_browse_loaded = False
        self._linux_install_process = None
        self._linux_install_dialog = None

        created = ensure_default_smart_playlists(self._db)
        if created:
            self._db.commit()

        self._init_window()
        self._init_ui()
        self._init_menu()
        self._connect_signals()
        self._load_cached_library()
        self._refresh_playlists()
        self._export_shared_music_playlists(silent=True)
        self._refresh_browser_panel()
        self._refresh_music_sharing_panel()
        self._refresh_current_rockbox_panel()

        # Let the window paint before probing the device or refreshing
        # off-screen Rockbox panels. Those paths can touch slow removable
        # storage and make startup feel hung if they run synchronously here.
        QTimer.singleShot(0, self._device_detector.start_polling)
        # Auto-refresh on startup if configured. This loads the DB first and
        # only parses files that are new or changed.
        if self._config.scan_on_startup:
            QTimer.singleShot(300, self._start_startup_refresh)

    def _init_window(self):
        self.setWindowTitle("RockPod")
        self.setMinimumSize(900, 600)
        self.resize(1080, 720)

        # Restore geometry if saved
        geom = self._config.window_geometry
        if geom:
            try:
                from PySide6.QtCore import QByteArray
                self.restoreGeometry(QByteArray.fromBase64(geom.encode()))
            except Exception:
                pass

    def _init_ui(self):
        """Build the iTunes 7-era window layout."""
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        # ── Toolbar ──
        self._toolbar = Toolbar()
        main_layout.addWidget(self._toolbar)

        # ── Main content area (sidebar + content) ──
        self._splitter = QSplitter(Qt.Horizontal)

        # Sidebar
        self._sidebar = Sidebar()
        self._sidebar.setMinimumWidth(140)
        self._sidebar.setMaximumWidth(260)
        self._splitter.addWidget(self._sidebar)

        # Content area (column browser + track table)
        content_widget = QWidget()
        content_layout = QVBoxLayout(content_widget)
        content_layout.setContentsMargins(0, 0, 0, 0)
        content_layout.setSpacing(0)

        # Column browser (initially hidden, toggle via View menu)
        self._column_browser = ColumnBrowser()
        self._column_browser.setVisible(self._config.show_column_browser)
        content_layout.addWidget(self._column_browser)

        # Track table
        self._track_model = TrackTableModel()
        self._track_model.set_artwork_manager(self._artwork)
        self._track_table = TrackTable()
        self._track_table.setModel(self._track_model)
        self._track_table.setContextMenuPolicy(Qt.CustomContextMenu)
        self._track_table.customContextMenuRequested.connect(self._on_table_context_menu)

        self._table_page = QWidget()
        table_layout = QVBoxLayout(self._table_page)
        table_layout.setContentsMargins(0, 0, 0, 0)
        table_layout.setSpacing(0)
        table_layout.addWidget(self._track_table)

        self._artist_view = GroupedTrackView("Artists")
        self._artist_view.set_artwork_manager(self._artwork)
        self._album_view = AlbumGridView(self._artwork)
        self._genre_view = GroupedTrackView("Genres")
        self._genre_view.set_artwork_manager(self._artwork)
        self._video_view = VideoGridView(self._video_thumbnails)
        self._video_sync_panel = VideoSyncPanel()
        self._video_player_window = None
        self._device_summary = DeviceSummaryWidget()
        self._theme_hub = ThemeHubWidget()
        self._ipone_wallpapers = IPoneWallpaperManagerWidget()
        self._theme_designer = ThemeDesignerWidget()
        self._ipodjs_engine_designer = IPodJSEngineDesignerWidget()
        self._boot_manager = BootManagerWidget()
        self._plugin_manager = PluginManagerWidget()
        self._game_manager = GameManagerWidget()
        self._photo_manager = PhotoManagerWidget()
        self._linux_manager = LinuxManagerWidget()
        self._browser_panel = BrowserPanel()
        self._music_sharing_panel = MusicSharingPanel()
        self._movie_store_panel = MovieStorePanel()
        self._game_browser_panel = BrowserPanel(
            music_store=False,
            title="iPod Games",
            web_title="iPod Games Browser",
            show_downloads=True,
        )
        self._store_page = QTabWidget()
        self._store_page.setObjectName("store_tabs")
        self._store_page.addTab(self._browser_panel, "Music")
        self._store_page.addTab(self._music_sharing_panel, "Sharing")
        self._store_page.addTab(self._movie_store_panel, "Movies")
        self._store_page.addTab(self._game_browser_panel, "iPod Games")
        self._store_page.currentChanged.connect(self._on_store_tab_changed)
        self._simulator_panel = SimulatorPanel()
        self._device_summary.set_options(
            self._config.auto_sync_on_connect,
            self._config.resync_metadata_changes,
            False,
            self._config.get("auto_rebuild_rockbox_database_after_sync", False),
            self._config.get("verify_device_in_background", False),
        )

        self._content_stack = QStackedWidget()
        self._content_stack.addWidget(self._table_page)
        self._content_stack.addWidget(self._video_view)
        self._content_stack.addWidget(self._video_sync_panel)
        self._content_stack.addWidget(self._artist_view)
        self._content_stack.addWidget(self._album_view)
        self._content_stack.addWidget(self._genre_view)
        self._content_stack.addWidget(self._device_summary)
        self._content_stack.addWidget(self._theme_hub)
        self._content_stack.addWidget(self._ipone_wallpapers)
        self._content_stack.addWidget(self._theme_designer)
        self._content_stack.addWidget(self._ipodjs_engine_designer)
        self._content_stack.addWidget(self._boot_manager)
        self._content_stack.addWidget(self._plugin_manager)
        self._content_stack.addWidget(self._game_manager)
        self._content_stack.addWidget(self._store_page)
        self._content_stack.addWidget(self._photo_manager)
        self._content_stack.addWidget(self._linux_manager)
        self._content_stack.addWidget(self._simulator_panel)
        content_layout.addWidget(self._content_stack)

        self._splitter.addWidget(content_widget)

        # Set splitter sizes (sidebar width)
        self._splitter.setSizes([self._config.sidebar_width, 800])

        main_layout.addWidget(self._splitter, 1)
        self._apply_theme_assets()

        # ── Status bar ──
        self._status_bar = StatusBar()
        main_layout.addWidget(self._status_bar)

    def _init_menu(self):
        """Build the menu bar."""
        menubar = self.menuBar()

        # File menu
        file_menu = menubar.addMenu("File")
        file_menu.addAction("Refresh Library", self._refresh_library, "Ctrl+R")
        file_menu.addAction("Force Full Rescan", self._force_full_rescan)
        file_menu.addAction("View Last Scan Report", self._show_last_scan_report)
        file_menu.addAction("View Missing FLAC/AIFF Tags", self._show_missing_tag_report)
        file_menu.addAction("Write Library Metadata Back to Files...", self._write_library_metadata_back_to_files)
        file_menu.addAction("Fetch Timed Lyrics for Library...", self._fetch_library_timed_lyrics)
        file_menu.addAction("Fetch Missing Artwork", self._refresh_missing_artwork)
        file_menu.addAction("New Playlist...", self._new_playlist, "Ctrl+N")
        file_menu.addSeparator()
        file_menu.addAction("Preferences...", self._show_preferences, "Ctrl+,")
        file_menu.addSeparator()
        file_menu.addAction("Quit", self.close, "Ctrl+Q")

        # Edit menu
        edit_menu = menubar.addMenu("Edit")
        edit_menu.addAction("Select All", self._select_all, "Ctrl+A")
        edit_menu.addAction("Get Info...", self._show_track_info, "Ctrl+I")

        # View menu
        view_menu = menubar.addMenu("View")
        self._browser_action = QAction("Show Column Browser", self)
        self._browser_action.setCheckable(True)
        self._browser_action.setChecked(self._config.show_column_browser)
        self._browser_action.toggled.connect(self._toggle_column_browser)
        self._browser_action.setShortcut("Ctrl+B")
        view_menu.addAction(self._browser_action)
        self._show_hidden_wallpapers_action = QAction("Show Hidden Wallpapers/Photos", self)
        self._show_hidden_wallpapers_action.setCheckable(True)
        self._show_hidden_wallpapers_action.setChecked(False)
        self._show_hidden_wallpapers_action.toggled.connect(self._toggle_hidden_wallpapers)
        view_menu.addAction(self._show_hidden_wallpapers_action)
        view_menu.addAction("Inspect Selected Artwork", self._show_selected_artwork_debug)

        # Device menu
        device_menu = menubar.addMenu("Device")
        device_menu.addAction("Sync to iPod", self._start_sync, "Ctrl+S")
        device_menu.addAction("Sync Weather", self._sync_weather_only)
        device_menu.addAction("Import Android Photos/Videos...", self._import_android_media)
        device_menu.addAction("Refresh Device", self._scan_device)
        device_menu.addAction("Force Device Rescan", self._force_device_rescan)
        device_menu.addAction("Clear Rockbox Database Cache", self._clear_rockbox_database_cache)
        device_menu.addAction("Remove Duplicate Device Tracks", self._remove_duplicate_device_tracks)
        device_menu.addAction("Regenerate iPod Artwork", self._regenerate_ipod_artwork)
        device_menu.addAction("Show Device Diff", self._show_device_diff)
        device_menu.addAction("Device Settings...", self._show_device_settings)
        device_menu.addAction("Rename Connected iPod...", self._rename_connected_device)
        device_menu.addAction("Forget Device", self._forget_device)
        device_menu.addSeparator()
        device_menu.addAction("Eject", self._eject_device)
        device_menu.addSeparator()
        device_menu.addAction("Create Mock Device...", self._create_mock_device)

    def _connect_signals(self):
        # Toolbar
        self._toolbar.scan_clicked.connect(self._start_scan)
        self._toolbar.sync_clicked.connect(self._start_sync)
        self._toolbar.fetch_artwork_clicked.connect(self._refresh_missing_artwork)
        self._toolbar.new_playlist_clicked.connect(self._new_playlist)
        self._toolbar.preferences_clicked.connect(self._show_preferences)
        self._toolbar.search_changed.connect(self._on_search)
        self._toolbar.eject_clicked.connect(self._eject_device)
        self._toolbar.play_pause_clicked.connect(self._playback.toggle_play_pause)
        self._toolbar.next_clicked.connect(self._playback.next)
        self._toolbar.previous_clicked.connect(self._playback.previous)
        self._toolbar.seek_requested.connect(self._on_seek_requested)
        self._toolbar.volume_changed.connect(self._playback.set_volume)

        # Sidebar
        self._sidebar.item_selected.connect(self._on_sidebar_selection)
        self._sidebar.context_requested.connect(self._on_sidebar_context_menu)
        self._sidebar.tracks_dropped_on_playlist.connect(self._add_track_ids_to_playlist)

        # Column browser
        self._column_browser.filter_changed.connect(self._on_browser_filter)

        # Track table
        self._track_table.track_double_clicked.connect(self._on_track_double_click)
        self._track_table.track_activated.connect(self._on_track_double_click)
        self._track_table.selection_changed.connect(self._on_track_selection_changed)
        self._track_table.playlist_reorder_requested.connect(self._reorder_current_playlist)
        self._video_view.track_double_clicked.connect(self._on_track_double_click)
        self._video_view.selection_changed.connect(self._on_track_selection_changed)
        self._video_view.context_requested.connect(self._on_video_context_menu)
        self._video_sync_panel.refresh_requested.connect(self._refresh_video_sync_panel)
        self._video_sync_panel.preview_requested.connect(self._preview_video_sync)
        self._video_sync_panel.sync_requested.connect(self._sync_video_track_ids)
        self._video_sync_panel.force_repair_requested.connect(self._force_repair_video_track_ids)
        self._video_sync_panel.remove_requested.connect(self._remove_video_track_ids_from_device)
        self._video_sync_panel.delete_requested.connect(self._delete_video_track_ids)

        # Library browser views
        self._artist_view.track_double_clicked.connect(self._on_track_double_click)
        self._artist_view.selection_changed.connect(self._on_track_selection_changed)
        self._artist_view.track_context_requested.connect(self._show_track_context_menu)
        self._album_view.track_double_clicked.connect(self._on_track_double_click)
        self._album_view.selection_changed.connect(self._on_track_selection_changed)
        self._album_view.album_context_requested.connect(self._on_album_context_menu)
        self._album_view.track_context_requested.connect(self._show_track_context_menu)
        self._genre_view.track_double_clicked.connect(self._on_track_double_click)
        self._genre_view.selection_changed.connect(self._on_track_selection_changed)
        self._genre_view.track_context_requested.connect(self._show_track_context_menu)
        self._theme_hub.profile_selected.connect(self._on_theme_profile_selected)
        self._theme_hub.profile_saved.connect(self._on_theme_profile_saved)
        self._theme_hub.theme_selected.connect(self._on_theme_selected)
        self._theme_hub.deploy_requested.connect(self._deploy_selected_theme)
        self._theme_hub.delete_requested.connect(self._delete_selected_theme_from_device)
        self._theme_hub.restore_requested.connect(self._restore_theme_backup)
        self._theme_hub.reset_default_requested.connect(self._reset_device_to_default)
        self._theme_hub.use_connected_device_requested.connect(self._use_connected_device_for_profile)
        self._ipone_wallpapers.profile_selected.connect(self._on_ipone_wallpaper_profile_selected)
        self._ipone_wallpapers.theme_selected.connect(self._on_ipone_wallpaper_theme_selected)
        self._ipone_wallpapers.apply_requested.connect(self._apply_ipone_wallpapers)
        self._ipone_wallpapers.import_requested.connect(self._import_ipone_wallpaper)
        self._ipone_wallpapers.hide_requested.connect(self._hide_ipone_wallpaper)
        self._ipone_wallpapers.remove_requested.connect(self._remove_ipone_wallpaper)
        self._theme_designer.profile_selected.connect(self._on_theme_designer_profile_selected)
        self._theme_designer.variant_selected.connect(self._on_theme_designer_variant_selected)
        self._theme_designer.preview_changed.connect(self._on_theme_designer_preview_changed)
        self._theme_designer.simulator_refresh_requested.connect(self._refresh_theme_designer_preview_now)
        self._theme_designer.save_requested.connect(self._save_theme_designer_variant)
        self._theme_designer.rename_requested.connect(self._rename_theme_designer_variant)
        self._theme_designer.duplicate_requested.connect(self._duplicate_theme_designer_variant)
        self._theme_designer.delete_requested.connect(self._delete_theme_designer_variant)
        self._theme_designer.deploy_device_requested.connect(self._deploy_theme_designer_variant_to_device)
        self._theme_designer.deploy_simulator_requested.connect(self._deploy_theme_designer_variant_to_simulator)
        self._ipodjs_engine_designer.apply_device_requested.connect(self._apply_ipodjs_engine_designer)
        self._boot_manager.profile_selected.connect(self._on_boot_profile_selected)
        self._boot_manager.target_mode_selected.connect(self._on_boot_target_mode_selected)
        self._boot_manager.choose_image_requested.connect(self._choose_boot_image)
        self._boot_manager.dry_run_requested.connect(self._dry_run_boot_deploy)
        self._boot_manager.apply_requested.connect(self._apply_boot_deploy)
        self._boot_manager.restore_requested.connect(self._restore_boot_backup)
        self._plugin_manager.profile_selected.connect(self._on_plugin_profile_selected)
        self._plugin_manager.target_mode_selected.connect(self._on_plugin_target_mode_selected)
        self._plugin_manager.plugin_selected.connect(self._on_plugin_selected)
        self._plugin_manager.dry_run_requested.connect(self._dry_run_plugin_deploy)
        self._plugin_manager.apply_requested.connect(self._apply_plugin_deploy)
        self._plugin_manager.remove_requested.connect(self._remove_plugin)
        self._game_manager.profile_selected.connect(self._on_game_profile_selected)
        self._game_manager.target_mode_selected.connect(self._on_game_target_mode_selected)
        self._game_manager.choose_library_requested.connect(self._choose_game_library)
        self._game_manager.refresh_requested.connect(self._refresh_game_manager)
        self._game_manager.selection_changed.connect(self._on_game_selection_changed)
        self._game_manager.dry_run_requested.connect(self._dry_run_game_sync)
        self._game_manager.sync_requested.connect(self._sync_selected_games)
        self._game_manager.remove_requested.connect(self._remove_selected_games)
        self._game_manager.backup_saves_requested.connect(self._backup_game_saves)
        self._game_manager.restore_saves_requested.connect(self._restore_game_saves)
        self._game_manager.export_saves_requested.connect(self._export_game_saves)
        self._game_manager.import_saves_requested.connect(self._import_game_saves)
        self._game_manager.fetch_cover_requested.connect(self._fetch_selected_game_cover)
        self._game_manager.fetch_metadata_requested.connect(self._fetch_selected_game_metadata)
        self._game_manager.optimize_cover_requested.connect(self._optimize_selected_game_cover)
        self._game_manager.launch_simulator_requested.connect(self._launch_selected_game_in_simulator)
        self._photo_manager.profile_selected.connect(self._on_photo_profile_selected)
        self._photo_manager.target_mode_selected.connect(self._on_photo_target_mode_selected)
        self._photo_manager.choose_library_requested.connect(self._choose_photo_library)
        self._photo_manager.refresh_requested.connect(self._refresh_photo_manager)
        self._photo_manager.selection_changed.connect(self._on_photo_selection_changed)
        self._photo_manager.dry_run_requested.connect(self._dry_run_photo_sync)
        self._photo_manager.sync_requested.connect(self._sync_selected_photos)
        self._photo_manager.hide_requested.connect(self._hide_selected_photos)
        self._photo_manager.remove_requested.connect(self._remove_selected_photos)
        self._browser_panel.open_external_requested.connect(self._open_browser_external)
        self._browser_panel.store_import_requested.connect(self._start_store_import)
        self._browser_panel.store_result_import_requested.connect(self._start_store_result_import)
        self._browser_panel.store_search_requested.connect(self._start_store_search)
        self._browser_panel.store_album_details_requested.connect(self._start_store_album_detail)
        self._browser_panel.store_home_tab_requested.connect(self._on_store_home_tab_requested)
        self._browser_panel.store_preview_requested.connect(self._preview_store_track)
        self._music_sharing_panel.share_settings_changed.connect(self._save_music_sharing_settings)
        self._music_sharing_panel.share_refresh_requested.connect(self._refresh_music_shares_from_relay)
        self._music_sharing_panel.share_send_requested.connect(self._send_music_share)
        self._music_sharing_panel.share_buy_requested.connect(self._start_music_share_import)
        self._movie_store_panel.movie_import_requested.connect(self._start_movie_import)
        self._movie_store_panel.movie_browse_requested.connect(self._start_movie_browse)
        self._game_browser_panel.open_external_requested.connect(self._open_browser_external)
        self._simulator_panel.profile_selected.connect(self._on_simulator_profile_selected)
        self._simulator_panel.simulator_selected.connect(self._on_simulator_target_selected)
        self._simulator_panel.bind_requested.connect(self._bind_simulator_to_profile)
        self._simulator_panel.dry_run_requested.connect(self._dry_run_to_simulator)
        self._simulator_panel.apply_requested.connect(self._apply_to_simulator)
        self._simulator_panel.launch_requested.connect(self._launch_simulator)
        self._simulator_panel.capture_requested.connect(self._capture_simulator_screenshot)
        self._simulator_panel.open_screenshots_requested.connect(self._open_simshots_folder)
        self._linux_manager.download_requested.connect(self._download_linux_iso)
        self._linux_manager.stage_requested.connect(self._stage_linux_payload)
        self._linux_manager.install_requested.connect(self._install_linux_payload)
        self._linux_manager.start_requested.connect(self._start_linux_vm)
        self._linux_manager.provision_requested.connect(self._provision_linux_vm)
        self._linux_manager.uninstall_requested.connect(self._uninstall_linux_payload)

        # Device summary
        self._device_summary.sync_clicked.connect(self._start_sync)
        self._device_summary.refresh_clicked.connect(self._scan_device)
        self._device_summary.eject_clicked.connect(self._eject_device)
        self._device_summary.force_rescan_clicked.connect(self._force_device_rescan)
        self._device_summary.open_music_clicked.connect(self._open_device_music_view)
        self._device_summary.settings_clicked.connect(self._show_device_settings)
        self._device_summary.auto_sync_changed.connect(self._set_auto_sync_from_summary)
        self._device_summary.resync_metadata_changed.connect(self._set_resync_metadata_from_summary)
        self._device_summary.rockbox_autoupdate_changed.connect(self._set_rockbox_autoupdate_from_summary)
        self._device_summary.verify_background_changed.connect(self._set_verify_background_from_summary)

        # Device detector
        self._device_detector.device_connected.connect(self._on_device_connected)
        self._device_detector.device_disconnected.connect(self._on_device_disconnected)
        self._device_detector.device_space_updated.connect(self._on_device_space_update)
        self._device_inventory.status.connect(self._on_device_inventory_status)
        self._device_inventory.finished.connect(self._on_device_inventory_finished)
        self._device_inventory.error.connect(self._on_device_inventory_error)
        self._device_storage_analyzer.finished.connect(self._on_device_storage_finished)
        self._device_storage_analyzer.error.connect(self._on_device_storage_error)

        # Playback
        self._playback.track_changed.connect(self._on_playback_track_changed)
        self._playback.state_changed.connect(self._on_playback_state_changed)
        self._playback.position_changed.connect(self._on_playback_position_changed)
        self._playback.volume_changed.connect(self._toolbar.set_volume)
        self._playback.error.connect(self._on_playback_error)
        self._artwork.album_artwork_updated.connect(self._on_album_artwork_updated)
        self._artwork.artwork_lookup_failed.connect(self._on_album_artwork_failed)
        self._artwork.artwork_lookup_status.connect(self._on_artwork_lookup_status)

        # Library scanner — proper Qt signal connections ensure slots run on
        # the main thread, so all DB reads and UI updates are thread-safe.
        self._scanner.scan_progress.connect(self._on_scan_progress)
        self._scanner.scan_track_found.connect(self._on_scan_track_found)
        self._scanner.scan_report.connect(self._on_scan_report)
        self._scanner.scan_finished.connect(self._on_scan_done)
        self._scanner.scan_error.connect(self._on_scan_error)

        # Sync engine — same queued-connection pattern as the scanner.
        self._sync_engine.sync_progress.connect(self._on_sync_progress)
        self._sync_engine.sync_finished.connect(self._on_sync_done)
        self._sync_engine.sync_cancelled.connect(self._on_sync_cancelled)
        self._sync_engine.sync_error.connect(self._on_sync_error)
        self._album_metadata_fetcher.finished.connect(self._on_album_metadata_fetched)
        self._album_metadata_fetcher.error.connect(self._on_album_metadata_fetch_error)
        self._android_importer.import_progress.connect(self._on_android_import_progress)
        self._android_importer.import_finished.connect(self._on_android_import_finished)
        self._android_importer.import_error.connect(self._on_android_import_error)
        self._android_importer.import_cancelled.connect(self._on_android_import_cancelled)

    # ═══════════════════════════════════════════════════════════════
    # Library scanning
    # ═══════════════════════════════════════════════════════════════

    def _load_cached_library(self):
        """Populate the UI immediately from the SQLite cache."""
        self._refresh_view()
        self._refresh_browser()
        self._update_status_bar()
        if self._db.get_track_count() > 0:
            self._status_bar.set_left_text("Library loaded from cache")

    def _start_startup_refresh(self):
        force_full = self._db_was_missing
        self._start_scan(force_full=force_full)

    def _refresh_library(self):
        self._start_scan(force_full=False)

    def _force_full_rescan(self):
        if self._scanner.is_scanning:
            return
        reply = QMessageBox.question(
            self,
            "Force Full Rescan",
            "Re-read metadata for every file in your Music folder?",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply == QMessageBox.Yes:
            self._start_scan(force_full=True)

    def _start_scan(self, force_full=False):
        if self._scanner.is_scanning:
            return
        self._scan_ui_refresh_pending = False
        self._scan_ui_refresh_timer.stop()
        self._toolbar.set_scanning(True)
        scan_label = "Scanning library" if force_full else "Refreshing library"
        self._status_bar.set_left_text("")
        self._status_bar.start_scan_activity(scan_label)
        self._scanner.start_scan(force_full=force_full)

    def _on_scan_progress(self, current, total, filename):
        self._status_bar.set_scan_progress(current, total, filename)

    def _on_scan_track_found(self, track_data):
        self._scan_ui_refresh_pending = True
        if not self._scan_ui_refresh_timer.isActive():
            self._scan_ui_refresh_timer.start()

    def _flush_scan_ui_refresh(self):
        if not self._scan_ui_refresh_pending:
            return
        self._scan_ui_refresh_pending = False
        self._refresh_view()
        self._refresh_browser()
        self._update_status_bar()

    def _on_scan_report(self, report):
        self._last_scan_report = dict(report)

    def _on_scan_done(self, total, elapsed):
        self._toolbar.set_scanning(False)
        self._status_bar.stop_scan_activity()
        self._scan_ui_refresh_timer.stop()
        self._flush_scan_ui_refresh()
        logger.info("Library refresh complete: %d changes in %.1fs", total, elapsed)
        self._refresh_view()
        self._refresh_browser()
        self._update_status_bar()
        report = getattr(self, "_last_scan_report", self._scanner.last_report)
        scanned = report.get("total_media_files_found", 0)
        audio = report.get("total_audio_files_found", 0)
        video = report.get("total_video_files_found", 0)
        imported = report.get("total_files_inserted", 0) + report.get("total_files_updated", 0)
        skipped = report.get("total_files_skipped", 0)
        ignored = report.get("total_non_media_files_ignored", 0)
        if total == 0 and skipped == 0:
            self._status_bar.set_left_text("Library up to date")
        else:
            parts = [
                f"Scanned {scanned} items",
                f"Imported {imported} library rows",
                f"Skipped {skipped} files",
            ]
            if audio or video:
                parts.append(f"{audio} audio / {video} video")
            if ignored:
                parts.append(f"Ignored {ignored} unsupported files")
            self._status_bar.set_left_text(" · ".join(parts))
        self._finalize_pending_store_playlist_import()
        self._export_shared_music_playlists(silent=True)

    def _on_scan_error(self, msg):
        self._toolbar.set_scanning(False)
        self._status_bar.stop_scan_activity()
        self._scan_ui_refresh_timer.stop()
        self._scan_ui_refresh_pending = False
        self._last_scan_report = self._scanner.last_report
        self._pending_store_playlist_import = None
        self._status_bar.set_left_text(f"Scan failed; cached library preserved ({msg})")
        logger.error("Scan error: %s", msg)

    def _show_last_scan_report(self):
        report = getattr(self, "_last_scan_report", self._scanner.last_report)
        scanned = report.get("total_media_files_found", 0)
        audio = report.get("total_audio_files_found", 0)
        video = report.get("total_video_files_found", 0)
        imported = report.get("total_files_inserted", 0) + report.get("total_files_updated", 0)
        skipped = report.get("total_files_skipped", 0)
        ignored = report.get("total_non_media_files_ignored", 0)
        lines = [
            f"Scanned {scanned} media files",
            f"Audio files found: {audio}",
            f"Video files found: {video}",
            f"Successfully parsed {report.get('total_files_successfully_parsed', 0)} files",
            f"Imported {imported} library rows",
            f"Removed {report.get('total_files_removed', 0)} stale tracks",
            f"Skipped {skipped} files",
        ]
        if ignored:
            lines.append(f"Ignored {ignored} unsupported files")
        partial = report.get("partial_import_folders") or []
        all_skipped = report.get("all_skipped_folders") or []
        if partial:
            lines.append("")
            lines.append("Partial import folders:")
            lines.extend(f"- {path}" for path in partial[:20])
        if all_skipped:
            lines.append("")
            lines.append("Folders with all supported files skipped:")
            lines.extend(f"- {path}" for path in all_skipped[:20])

        details = []
        skipped_files = report.get("skipped_files") or []
        if skipped_files:
            details.append("Skipped files:")
            details.extend(
                f"- {item.get('path', '')}: {item.get('reason', '')}"
                for item in skipped_files
            )
        warnings = report.get("warnings") or []
        if warnings:
            if details:
                details.append("")
            details.append("Imported with warnings:")
            details.extend(
                f"- {item.get('path', '')}: {item.get('reason', '')}"
                for item in warnings
            )

        dialog = QMessageBox(self)
        dialog.setWindowTitle("Last Scan Report")
        dialog.setIcon(QMessageBox.Information)
        dialog.setText("\n".join(lines))
        if details:
            dialog.setDetailedText("\n".join(details))
        dialog.exec()

    # ═══════════════════════════════════════════════════════════════
    # View / display logic
    # ═══════════════════════════════════════════════════════════════

    def _library_media_type(self):
        if self._current_view in {"library_videos", "library_video_sync"}:
            return "video"
        return "audio"

    def _current_status_item_label(self):
        if self._current_view in {"library_videos", "library_video_sync"}:
            return "videos"
        return "songs"

    def _active_track_table(self):
        if self._current_view == "library_videos":
            return self._video_view
        if self._current_view == "library_artists":
            return self._artist_view
        if self._current_view == "library_albums":
            return self._album_view
        if self._current_view == "library_genres":
            return self._genre_view
        return self._track_table

    def _active_track_selection_ids(self):
        if self._current_view == "library_video_sync":
            return set()
        return self._active_track_table().get_selected_track_ids()

    def _refresh_view(self):
        """Reload the track table based on current sidebar selection and filters."""
        selected_ids = self._active_track_selection_ids()
        self._update_playlist_reorder_mode()
        tracks = self._tracks_for_current_view(include_search=True)
        if self._current_view == "library_videos":
            self._video_view.set_tracks(tracks)
            self._video_view.set_current_track_id(self._current_playing_track_id)
            self._video_view.select_track_ids(selected_ids)
        elif self._current_view == "library_video_sync":
            self._refresh_video_sync_panel()
        else:
            self._track_model.set_tracks(tracks)
            self._track_model.set_current_track_id(self._current_playing_track_id)
            self._track_table.select_track_ids(selected_ids)

        self._update_content_mode(tracks)
        self._update_current_view_status(tracks)

    def _tracks_for_current_view(self, include_search=True):
        if self._current_view == "library_music":
            tracks = self._get_filtered_tracks(media_type="audio")
        elif self._current_view == "library_videos":
            tracks = self._get_filtered_tracks(media_type="video")
        elif self._current_view == "library_video_sync":
            tracks = []
        elif self._current_view == "library_artists":
            tracks = self._get_filtered_tracks(media_type="audio")
        elif self._current_view == "library_albums":
            tracks = self._get_filtered_tracks(media_type="audio")
        elif self._current_view == "library_genres":
            tracks = self._get_filtered_tracks(media_type="audio")
        elif self._current_view == "device_music":
            tracks = self._sync_engine.get_device_tracks()
        elif self._current_view == "device_not_on_ipod":
            if self._device_verification_running:
                self._status_bar.set_left_text("Verifying device inventory...")
                tracks = []
            elif not self._sync_engine.device_inventory_is_verified():
                if self._device_detector.is_connected:
                    self._status_bar.set_left_text("Preparing Not on iPod view...")
                    self._scan_device()
                else:
                    self._status_bar.set_left_text("Refresh the connected iPod to load missing tracks")
                tracks = []
            else:
                tracks = self._sync_engine.get_not_on_device_tracks()
        elif self._current_view == "device_root":
            tracks = []
        elif self._current_view == "rockbox_themes":
            tracks = []
        elif self._current_view == "rockbox_wallpapers":
            tracks = []
        elif self._current_view == "rockbox_theme_designer":
            tracks = []
        elif self._current_view == "rockbox_ipodjs_engine_designer":
            tracks = []
        elif self._current_view == "rockbox_boot":
            tracks = []
        elif self._current_view == "rockbox_plugins":
            tracks = []
        elif self._current_view == "rockbox_game_sync":
            tracks = []
        elif self._current_view == "rockbox_games":
            tracks = []
        elif self._current_view == "rockbox_photos":
            tracks = []
        elif self._current_view == "rockbox_linux":
            tracks = []
        elif self._current_view == "rockbox_browser":
            tracks = []
        elif self._current_view == "rockbox_sharing":
            tracks = []
        elif self._current_view == "rockbox_simulator":
            tracks = []
        elif self._current_view.startswith("device_playlist_"):
            try:
                playlist_id = int(self._current_view.replace("device_playlist_", ""))
                tracks = self._db.get_device_playlist_tracks(playlist_id)
            except ValueError:
                tracks = []
        elif self._current_view.startswith("playlist_"):
            try:
                pid = int(self._current_view.replace("playlist_", ""))
                playlist = self._db.get_playlist(pid)
                if playlist and playlist["is_smart"]:
                    tracks = evaluate_playlist(
                        self._db,
                        playlist,
                        device_id=self._sync_engine.current_device_key,
                    )
                else:
                    tracks = self._db.get_playlist_tracks(pid)
            except ValueError:
                tracks = []
        else:
            tracks = self._db.get_all_tracks()

        tracks = normalize_tracks_for_ui(tracks)
        if include_search:
            tracks = filter_tracks_for_search(tracks, self._toolbar.search_text.strip())
        return tracks

    def _current_manual_playlist_id(self):
        if not self._current_view.startswith("playlist_"):
            return None
        try:
            playlist_id = int(self._current_view.replace("playlist_", ""))
        except ValueError:
            return None
        playlist = self._db.get_playlist(playlist_id)
        if not playlist or playlist["is_smart"]:
            return None
        return playlist_id

    def _update_playlist_reorder_mode(self):
        enabled = bool(self._current_manual_playlist_id() and not self._toolbar.search_text.strip())
        self._track_table.set_playlist_reorder_enabled(enabled)

    def _reorder_current_playlist(self, track_ids, moved_track_ids=None):
        playlist_id = self._current_manual_playlist_id()
        if not playlist_id:
            self._status_bar.set_left_text("Only regular playlists can be reordered")
            return
        if self._toolbar.search_text.strip():
            self._status_bar.set_left_text("Clear search before reordering a playlist")
            return
        selected_ids = set(moved_track_ids or [])
        updated = self._db.set_playlist_track_order(playlist_id, track_ids)
        if not updated:
            return
        self._db.commit()
        self._refresh_playlists()
        self._export_shared_music_playlists(silent=True)
        self._refresh_view()
        self._track_table.select_track_ids(selected_ids)
        self._status_bar.set_left_text("Playlist order updated")

    def _get_filtered_tracks(self, media_type="audio"):
        """Get tracks filtered by column browser selections."""
        g = self._current_filter_genre
        a = self._current_filter_artist
        al = self._current_filter_album

        if not g and not a and not al:
            return self._db.get_all_tracks() if media_type == "audio" else self._db.get_tracks_by_media_type(media_type, order_by="artist, album, disc_number, track_number, title")
        return self._db.get_tracks_for_browser_filters(media_type=media_type, genre=g, artist=a, album=al)

    def _refresh_browser(self):
        """Update the column browser pane lists."""
        media_type = self._library_media_type() if self._current_view.startswith("library_") else "audio"
        genres = self._db.get_distinct_genres(media_type=media_type)
        artists = self._db.get_distinct_artists(media_type=media_type)
        albums = self._db.get_distinct_albums(media_type=media_type)
        self._column_browser.set_genres(genres)
        self._column_browser.set_artists(artists)
        self._column_browser.set_albums(albums)

    def _refresh_library_browsers(self, tracks=None):
        tracks = normalize_tracks_for_ui(
            tracks if tracks is not None else self._tracks_for_current_view()
        )
        self._artist_view.set_groups(group_tracks_by_artist(tracks))
        self._album_view.set_albums(group_tracks_by_album(tracks))
        self._genre_view.set_groups(group_tracks_by_genre(tracks))

    def _update_status_bar(self):
        count = self._db.get_track_count()
        duration = self._db.get_total_duration()
        size = self._db.get_total_size()
        self._status_bar.update_library_info(count, duration, size, item_label="songs")

        # Keep the "Not on iPod" label neutral while background verification is
        # reconciling cached inventory with the live device.
        if self._device_verification_running:
            self._sidebar.update_not_on_ipod_count(None)
        elif (
            self._device_detector.is_connected
            and not self._sync_engine.device_inventory_has_local_links()
        ):
            self._sidebar.update_not_on_ipod_count(None)
        else:
            missing = self._sync_engine.get_not_on_device_tracks()
            self._sidebar.update_not_on_ipod_count(len(missing))
        self._update_sync_status()

    def _update_current_view_status(self, tracks):
        summary = summarize_tracks(tracks)
        total = summary["count"]
        if total == self._db.get_track_count() and self._current_view == "library_music":
            return
        self._status_bar.update_library_info(
            total,
            summary["duration"],
            summary["size"],
            item_label=self._current_status_item_label(),
        )
        if self._current_view == "device_not_on_ipod":
            self._status_bar.set_left_text(self._not_on_ipod_status_text(tracks))

    def _not_on_ipod_status_text(self, tracks):
        if self._device_verification_running:
            return "Verifying device inventory..."
        if not self._sync_engine.device_inventory_is_verified():
            if self._device_detector.is_connected:
                return "Preparing Not on iPod view..."
            return "Refresh the connected iPod to load missing tracks"

        normalized = normalize_tracks_for_ui(tracks)
        if not normalized:
            return "All library tracks are already on iPod"

        track_ids = [track.get("id") for track in normalized if track.get("id")]
        reason_counts = self._sync_engine.get_not_on_device_reason_counts(
            track_ids=track_ids or None
        )
        if not reason_counts:
            return f"Not on iPod: {len(normalized)} track{'s' if len(normalized) != 1 else ''}"

        parts = []
        for reason, count in list(reason_counts.items())[:3]:
            parts.append(f"{reason} {count}")
        if len(reason_counts) > 3:
            parts.append(f"+{len(reason_counts) - 3} more")
        return (
            f"Not on iPod: {len(normalized)} track{'s' if len(normalized) != 1 else ''}"
            f" · {' · '.join(parts)}"
        )

    def _on_search(self, text):
        self._refresh_view()

    def _on_sidebar_selection(self, category, item_id):
        self._current_view = item_id
        self._toolbar.clear_search()
        if item_id.startswith("library_"):
            self._current_filter_genre = ""
            self._current_filter_artist = ""
            self._current_filter_album = ""
            self._refresh_browser()
            self._column_browser.clear_all()
        self._refresh_view()

        # Update title
        titles = {
            "library_music": "Music",
            "library_videos": "Videos",
            "library_video_sync": "Video Sync",
            "library_artists": "Artists",
            "library_albums": "Albums",
            "library_genres": "Genres",
            "rockbox_themes": "Themes",
            "rockbox_wallpapers": "Wallpapers",
            "rockbox_theme_designer": "iPone Designer",
            "rockbox_ipodjs_engine_designer": "iPod Engine",
            "rockbox_boot": "Boot / Branding",
            "rockbox_plugins": "Plugins",
            "rockbox_game_sync": "Game Sync",
            "rockbox_games": "Store",
            "rockbox_movies": "Store",
            "rockbox_sharing": "Store",
            "rockbox_photos": "Photos",
            "rockbox_linux": "Linux",
            "rockbox_browser": "Store",
            "rockbox_simulator": "Simulator",
            "device_root": "Device",
            "device_music": "On This iPod",
            "device_not_on_ipod": "Not on iPod",
        }
        title = titles.get(item_id, "RockPod")
        if item_id.startswith("device_playlist_"):
            try:
                playlist = self._db.get_device_playlist(int(item_id.replace("device_playlist_", "")))
                if playlist:
                    title = playlist["name"]
            except ValueError:
                pass
        if item_id.startswith("playlist_"):
            try:
                playlist = self._db.get_playlist(int(item_id.replace("playlist_", "")))
                if playlist:
                    title = playlist["name"]
            except ValueError:
                pass
        self._toolbar.set_title(title)
        self._update_content_mode()

    def _open_device_music_view(self):
        self._sidebar.select_item("device_music")
        self._on_sidebar_selection("device", "device_music")

    def _device_config_value(self, key, default=None):
        device = self._device_detector.current_device
        if device is not None:
            return self._config.get_effective(key, device=device, default=default)
        device_key = self._sync_engine.current_device_key
        if device_key:
            return self._config.get_effective(key, stable_device_key=device_key, default=default)
        return self._config.get(key, default)

    def _set_device_scoped_setting(self, key, value):
        device = self._device_detector.current_device
        device_key = self._sync_engine.current_device_key
        if device is None and not device_key:
            self._config.set(key, value)
            self._config.save()
            return

        if value == self._config.get(key):
            self._config.remove_device_override(key, device=device, stable_device_key=device_key)
        else:
            self._config.set_device_override(key, value, device=device, stable_device_key=device_key)
        self._config.save()

    def _default_connected_device_name(self, device):
        if getattr(device, "detected_model", ""):
            return device.detected_model
        if self._config.mock_device_enabled and getattr(device, "mount_path", "") == self._config.mock_device_path:
            return "Mock iPod"
        return os.path.basename(getattr(device, "mount_path", "") or "") or "iPod"

    def _apply_ipodjs_engine_designer(self, settings):
        payload = {
            "rockbox_ui_engine": "ipodjs",
            "rockbox_ui_accent": "blue",
            "rockbox_ui_density": "comfortable",
            "rockbox_ui_font_scale": "normal",
            "rockbox_ui_surface": "solid",
            "rockbox_ui_hold_effect": "dim",
            "rockbox_ui_dark_mode": False,
        }
        payload.update(dict(settings or {}))
        if self._apply_device_settings(payload):
            self._status_bar.set_left_text("iPod engine settings applied")

    def _apply_device_settings(self, settings):
        device = self._device_detector.current_device
        if not device:
            return False

        for key, value in dict(settings or {}).items():
            baseline = "" if key == "display_name" else self._config.get(key)
            if value == baseline:
                self._config.remove_device_override(key, device=device)
            else:
                self._config.set_device_override(key, value, device=device)
        self._config.save()

        if "rockbox_ui_engine" in dict(settings or {}):
            try:
                set_rockbox_ui_engine(device, settings["rockbox_ui_engine"])
            except OSError as exc:
                QMessageBox.warning(
                    self,
                    "Device Settings",
                    f"Saved the RockPod setting, but could not update Rockbox config.cfg:\n{exc}",
                )
        if "rockbox_ui_accent" in dict(settings or {}):
            try:
                set_rockbox_ui_accent(device, settings["rockbox_ui_accent"])
            except OSError as exc:
                QMessageBox.warning(
                    self,
                    "Device Settings",
                    f"Saved the RockPod setting, but could not update Rockbox config.cfg:\n{exc}",
                )
        if "rockbox_ui_density" in dict(settings or {}):
            try:
                set_rockbox_ui_density(device, settings["rockbox_ui_density"])
            except OSError as exc:
                QMessageBox.warning(
                    self,
                    "Device Settings",
                    f"Saved the RockPod setting, but could not update Rockbox config.cfg:\n{exc}",
                )
        if "rockbox_show_applications_menu" in dict(settings or {}):
            try:
                set_rockbox_applications_menu(
                    device,
                    settings["rockbox_show_applications_menu"],
                )
            except OSError as exc:
                QMessageBox.warning(
                    self,
                    "Device Settings",
                    f"Saved the RockPod setting, but could not update Rockbox config.cfg:\n{exc}",
                )
        if "rockbox_ui_font_scale" in dict(settings or {}):
            try:
                set_rockbox_ui_font_scale(device, settings["rockbox_ui_font_scale"])
            except OSError as exc:
                QMessageBox.warning(
                    self,
                    "Device Settings",
                    f"Saved the RockPod setting, but could not update Rockbox config.cfg:\n{exc}",
                )
        if "rockbox_ui_surface" in dict(settings or {}):
            try:
                set_rockbox_ui_surface(device, settings["rockbox_ui_surface"])
            except OSError as exc:
                QMessageBox.warning(
                    self,
                    "Device Settings",
                    f"Saved the RockPod setting, but could not update Rockbox config.cfg:\n{exc}",
                )
        if "rockbox_ui_hold_effect" in dict(settings or {}):
            try:
                set_rockbox_ui_hold_effect(device, settings["rockbox_ui_hold_effect"])
            except OSError as exc:
                QMessageBox.warning(
                    self,
                    "Device Settings",
                    f"Saved the RockPod setting, but could not update Rockbox config.cfg:\n{exc}",
                )
        if "rockbox_ui_dark_mode" in dict(settings or {}):
            try:
                set_rockbox_ui_dark_mode(device, settings["rockbox_ui_dark_mode"])
            except OSError as exc:
                QMessageBox.warning(
                    self,
                    "Device Settings",
                    f"Saved the RockPod setting, but could not update Rockbox config.cfg:\n{exc}",
                )

        override_name = self._config.get_device_display_name(device=device)
        device.name = override_name or self._default_connected_device_name(device)
        self._db.upsert_device(device_record_from_info(device))
        self._db.commit()
        self._sidebar.set_device_name(device.name)
        self._update_status_bar()
        self._update_sync_status()
        if self._current_view in {"device_music", "device_not_on_ipod"}:
            self._refresh_view()
        self._update_device_summary()
        return True

    def _show_device_settings(self):
        device = self._device_detector.current_device
        if not device:
            QMessageBox.warning(self, "No Device", "No Rockbox device is connected.")
            return
        dialog = DeviceSettingsDialog(
            self._config,
            device,
            default_name=self._default_connected_device_name(device),
            parent=self,
        )
        dialog.settings_saved.connect(self._apply_device_settings)
        dialog.exec()

    def _rename_connected_device(self):
        device = self._device_detector.current_device
        if not device:
            QMessageBox.warning(self, "No Device", "No Rockbox device is connected.")
            return
        current_name = self._config.get_device_display_name(device=device) or getattr(device, "name", "") or "iPod"
        name, ok = QInputDialog.getText(
            self,
            "Rename Device",
            "Device name override (leave blank to reset):",
            text=current_name,
        )
        if not ok:
            return
        self._apply_device_settings({"display_name": str(name or "").strip()})

    def _apply_connected_device_name(self, name):
        self._apply_device_settings({"display_name": str(name or "").strip()})

    def _set_auto_sync_from_summary(self, enabled):
        self._set_device_scoped_setting("auto_sync_on_connect", bool(enabled))

    def _set_resync_metadata_from_summary(self, enabled):
        self._set_device_scoped_setting("resync_metadata_changes", bool(enabled))

    def _set_rockbox_autoupdate_from_summary(self, enabled):
        self._set_device_scoped_setting("auto_rebuild_rockbox_database_after_sync", bool(enabled))

    def _set_verify_background_from_summary(self, enabled):
        self._set_device_scoped_setting("verify_device_in_background", bool(enabled))

    def _update_content_mode(self, tracks=None):
        current_tracks = normalize_tracks_for_ui(
            tracks if tracks is not None else self._tracks_for_current_view()
        )
        if self._current_view == "library_artists":
            self._content_stack.setCurrentWidget(self._artist_view)
            self._refresh_library_browsers(current_tracks)
        elif self._current_view == "library_videos":
            self._content_stack.setCurrentWidget(self._video_view)
        elif self._current_view == "library_video_sync":
            self._content_stack.setCurrentWidget(self._video_sync_panel)
            self._refresh_video_sync_panel()
        elif self._current_view == "library_albums":
            self._content_stack.setCurrentWidget(self._album_view)
            self._refresh_library_browsers(current_tracks)
        elif self._current_view == "library_genres":
            self._content_stack.setCurrentWidget(self._genre_view)
            self._refresh_library_browsers(current_tracks)
        elif self._current_view == "device_root":
            self._content_stack.setCurrentWidget(self._device_summary)
            self._update_device_summary()
        elif self._current_view == "rockbox_themes":
            self._content_stack.setCurrentWidget(self._theme_hub)
            self._refresh_theme_hub()
        elif self._current_view == "rockbox_wallpapers":
            self._content_stack.setCurrentWidget(self._ipone_wallpapers)
            self._refresh_ipone_wallpapers()
        elif self._current_view == "rockbox_theme_designer":
            self._content_stack.setCurrentWidget(self._theme_designer)
            self._refresh_theme_designer()
        elif self._current_view == "rockbox_ipodjs_engine_designer":
            self._content_stack.setCurrentWidget(self._ipodjs_engine_designer)
        elif self._current_view == "rockbox_boot":
            self._content_stack.setCurrentWidget(self._boot_manager)
            self._refresh_boot_manager()
        elif self._current_view == "rockbox_plugins":
            self._content_stack.setCurrentWidget(self._plugin_manager)
            self._refresh_plugin_manager()
        elif self._current_view == "rockbox_game_sync":
            self._content_stack.setCurrentWidget(self._game_manager)
            self._refresh_game_manager()
        elif self._current_view == "rockbox_games":
            self._content_stack.setCurrentWidget(self._store_page)
            self._store_page.setCurrentWidget(self._game_browser_panel)
            self._refresh_game_browser_panel()
        elif self._current_view == "rockbox_movies":
            self._content_stack.setCurrentWidget(self._store_page)
            self._store_page.setCurrentWidget(self._movie_store_panel)
            self._refresh_movie_store_panel()
        elif self._current_view == "rockbox_photos":
            self._content_stack.setCurrentWidget(self._photo_manager)
            self._refresh_photo_manager()
        elif self._current_view == "rockbox_linux":
            self._content_stack.setCurrentWidget(self._linux_manager)
            self._refresh_linux_manager()
        elif self._current_view == "rockbox_browser":
            self._content_stack.setCurrentWidget(self._store_page)
            self._store_page.setCurrentWidget(self._browser_panel)
            self._refresh_browser_panel()
        elif self._current_view == "rockbox_simulator":
            self._content_stack.setCurrentWidget(self._simulator_panel)
            self._refresh_simulator_panel()
        else:
            self._content_stack.setCurrentWidget(self._table_page)

        browser_visible = (
            self._config.show_column_browser
            and self._current_view in {"library_music", "library_artists", "library_albums", "library_genres"}
        )
        self._column_browser.setVisible(browser_visible)
        self._mark_current_playing_track(self._current_playing_track_id)

    def _on_store_tab_changed(self, index):
        if getattr(self, "_content_stack", None) is None:
            return
        if self._content_stack.currentWidget() is not self._store_page:
            return
        current = self._store_page.widget(index)
        self._toolbar.set_title("Store")
        if current is self._movie_store_panel:
            self._current_view = "rockbox_movies"
            self._refresh_movie_store_panel()
            self._status_bar.set_left_text("Store: Movies")
        elif current is self._game_browser_panel:
            self._current_view = "rockbox_games"
            self._refresh_game_browser_panel()
            self._status_bar.set_left_text("Store: iPod Games")
        elif current is self._music_sharing_panel:
            self._current_view = "rockbox_sharing"
            self._refresh_music_sharing_panel()
            self._status_bar.set_left_text("Store: Sharing")
        elif current is self._browser_panel:
            self._current_view = "rockbox_browser"
            self._refresh_browser_panel()
            self._status_bar.set_left_text("Store: Music")

    def _update_device_summary(self):
        device = self._device_detector.current_device
        device_key = self._sync_engine.current_device_key
        device_row = self._db.get_device_by_key(device_key) if device_key else None
        runtime_summary = self._db.runtime_stats_summary(device_key) if device_key else None
        runtime_insights = self._db.runtime_device_insights(device_key) if device_key else None
        device_playlists = self._db.get_device_playlists(device_key) if device_key else []
        rockbox_state = detect_rockbox_database_state(device, device_row)
        data = build_summary_data(
            device,
            device_row,
            self._sync_engine.get_device_tracks(),
            runtime_summary,
            rockbox_state,
            self._device_state,
            len(device_playlists),
            runtime_insights,
            self._cached_device_storage(device, device_key, device_row),
        )
        self._device_summary.set_options(
            self._device_config_value("auto_sync_on_connect", False),
            self._device_config_value("resync_metadata_changes", True),
            False,
            self._device_config_value("auto_rebuild_rockbox_database_after_sync", False),
            self._device_config_value("verify_device_in_background", False),
        )
        self._device_summary.update_summary(data)

    def _update_sync_status(self):
        if self._device_state == "Updating Database":
            self._status_bar.set_right_text(self._rockbox_database_progress_text())
            return
        if self._device_state == "Sync Complete (DB stale)":
            self._status_bar.set_right_text("Rockbox database out of date")
            return
        if self._device_verification_running:
            self._status_bar.set_right_text("Verifying device in background...")
            return
        if not self._device_detector.is_connected:
            if self._sync_engine.current_device_key:
                self._status_bar.set_right_text("Using cached device inventory")
            else:
                self._status_bar.set_right_text("")
            return
        if not self._sync_engine.device_inventory_has_local_links():
            suffix = ""
            if self._last_sync_time:
                suffix = f" · Last sync {self._last_sync_time.strftime('%H:%M')}"
            self._status_bar.set_right_text(f"Using cached device inventory{suffix}")
            return
        counts = self._sync_engine.get_sync_status_counts()
        suffix = ""
        if self._last_sync_time:
            suffix = f" · Last sync {self._last_sync_time.strftime('%H:%M')}"
        total_operations = counts["missing"] + counts["resync"]
        if total_operations == 0:
            self._status_bar.set_right_text(f"Up to date{suffix}")
        else:
            self._status_bar.set_right_text(
                f"Sync required ({counts['missing']} missing, {counts['resync']} updates){suffix}"
            )

    def _rockbox_database_progress_text(self):
        elapsed = ""
        if self._rockbox_db_update_started_at is not None:
            elapsed_seconds = max(0, int(time.monotonic() - self._rockbox_db_update_started_at))
            minutes, seconds = divmod(elapsed_seconds, 60)
            hours, minutes = divmod(minutes, 60)
            if hours:
                elapsed = f"{hours:d}:{minutes:02d}:{seconds:02d}"
            else:
                elapsed = f"{minutes:02d}:{seconds:02d}"
        dots = "." * ((self._rockbox_db_feedback_tick % 3) + 1)
        if elapsed:
            return f"Rockbox database updating{dots} {elapsed}"
        return f"Rockbox database updating{dots}"

    def _on_browser_filter(self, genre, artist, album):
        self._current_filter_genre = genre
        self._current_filter_artist = artist
        self._current_filter_album = album
        self._refresh_view()

    def _toggle_column_browser(self, visible):
        self._config.show_column_browser = visible
        self._update_content_mode()

    # ═══════════════════════════════════════════════════════════════
    # Track interactions
    # ═══════════════════════════════════════════════════════════════

    def _ensure_video_player_window(self):
        if self._video_player_window is None:
            self._video_player_window = VideoPlayerWindow(self)
        return self._video_player_window

    def _on_track_double_click(self, track):
        track = normalize_track_for_ui(track)
        if not track.get("file_path"):
            self._status_bar.set_left_text("Only local library files can be played")
            return
        if track.get("media_type") == "video":
            player = self._ensure_video_player_window()
            self._playback.set_video_output(player.video_output)
            player.set_track(track)
            player.show()
            player.raise_()
            player.activateWindow()
        queue = self._playback_queue_for_context()
        if not self._playback.play_track(track, queue, self._current_view):
            return
        art_path = self._artwork.get_artwork_path(track, "thumb")
        self._toolbar.set_selected_track(track, art_path)
        title = track.get("title") or "Untitled"
        if track.get("media_type") == "video":
            self._status_bar.set_left_text(f"Playing video: {title}")
        else:
            self._status_bar.set_left_text(f"Playing: {title}")

    def _playback_queue_for_context(self):
        sender = self.sender()
        if sender in (self._artist_view, self._genre_view, self._album_view, self._video_view):
            return sender.current_tracks()
        return self._tracks_for_current_view(include_search=True)

    def _on_playback_track_changed(self, track):
        track = normalize_track_for_ui(track)
        if not track:
            self._current_playing_track_id = None
            self._toolbar.set_selected_track()
            self._mark_current_playing_track(None)
            return
        self._current_playing_track_id = track.get("id")
        art_path = self._artwork.get_artwork_path(track, "thumb")
        self._toolbar.set_selected_track(track, art_path)
        if track.get("media_type") == "video":
            player = self._ensure_video_player_window()
            player.set_track(track)
        else:
            self._playback.set_video_output(None)
        self._mark_current_playing_track(self._current_playing_track_id)

    def _on_playback_state_changed(self, state):
        self._toolbar.set_playback_state(state)

    def _on_playback_position_changed(self, position_ms, duration_ms):
        self._toolbar.set_playback_position(position_ms, duration_ms)

    def _on_seek_requested(self, slider_value):
        duration = self._playback.duration
        if duration <= 0:
            return
        self._playback.seek(int(duration * slider_value / 1000))

    def _on_playback_error(self, message):
        self._status_bar.set_left_text(f"Playback error: {message}")
        self._toolbar.set_playback_state("stopped")

    def _preview_store_track(self, track, queue_tracks):
        track = normalize_track_for_ui(track)
        queue = normalize_tracks_for_ui(queue_tracks or [track])
        if not track.get("preview_url") and not track.get("stream_url"):
            self._status_bar.set_left_text("No preview is available for this store track")
            return
        self._playback.set_video_output(None)
        if not self._playback.play_track(track, queue, "store_preview"):
            return
        cover_path = track.get("cover_path") or ""
        self._toolbar.set_selected_track(track, cover_path if os.path.isfile(cover_path) else "")
        title = track.get("title") or "store preview"
        self._status_bar.set_left_text(f"Previewing: {title}")

    def _mark_current_playing_track(self, track_id):
        self._track_model.set_current_track_id(track_id)
        self._video_view.set_current_track_id(track_id)
        self._artist_view.set_current_track_id(track_id)
        self._album_view.set_current_track_id(track_id)
        self._genre_view.set_current_track_id(track_id)

    def _on_track_selection_changed(self, tracks):
        if tracks:
            track = normalize_track_for_ui(tracks[0])
            art_path = self._artwork.get_artwork_path(track, "thumb")
            if self._playback.state == STATE_STOPPED:
                self._toolbar.set_selected_track(track, art_path)
            selected = summarize_tracks(tracks)
            visible = summarize_tracks(self._tracks_for_current_view())
            self._status_bar.update_selection_info(
                selected["count"], visible["count"], visible["duration"], visible["size"],
                item_label=self._current_status_item_label(),
            )
        else:
            if self._playback.state == STATE_STOPPED:
                self._toolbar.set_selected_track()
            self._update_current_view_status(self._tracks_for_current_view())

    def _show_track_info(self):
        tracks = self._active_track_table().get_selected_tracks()
        if tracks:
            self._show_track_info_for(tracks[0])

    def _show_track_info_for(self, track):
        d = normalize_track_for_ui(track)
        art_path = self._artwork.get_artwork_path(d, "display")
        editor = MetadataEditor(d, art_path, self)
        editor.metadata_saved.connect(self._on_metadata_saved)
        editor.exec()

    @staticmethod
    def _merge_track_row_from_file(existing_row, scanned_track):
        merged = dict(existing_row)
        scanned = dict(scanned_track or {})
        for field in (
            "media_type",
            "video_kind",
            "file_size",
            "last_modified",
            "title",
            "artist",
            "album",
            "album_artist",
            "show_title",
            "genre",
            "year",
            "season_number",
            "episode_number",
            "track_number",
            "track_total",
            "disc_number",
            "disc_total",
            "duration",
            "bitrate",
            "sample_rate",
            "channels",
            "codec",
            "composer",
            "comment",
            "compilation",
            "has_embedded_artwork",
            "metadata_hash",
            "artwork_hash",
        ):
            if field in scanned:
                merged[field] = scanned.get(field)
        merged["file_path"] = existing_row.get("file_path", "")
        merged["file_hash"] = compute_file_hash(merged["file_path"])
        merged.pop("id", None)
        return merged

    def _rewrite_track_row_from_file(self, existing_row, file_updates):
        write_track_metadata_to_file(existing_row.get("file_path", ""), file_updates)
        scanned_track, _scan_info = read_metadata_details(existing_row.get("file_path", ""))
        merged = self._merge_track_row_from_file(existing_row, scanned_track.to_dict())
        self._db.upsert_track(merged)
        refreshed = self._db.get_track_by_path(existing_row.get("file_path", ""))
        return dict(refreshed) if refreshed is not None else dict(existing_row)

    def _on_metadata_saved(self, track_id, updates):
        if updates:
            row = self._db.get_track_by_id(track_id)
            if row is None:
                return

            existing = dict(row)
            file_updates = {k: v for k, v in updates.items() if k in FILE_TAG_METADATA_FIELDS}
            library_updates = {k: v for k, v in updates.items() if k not in FILE_TAG_METADATA_FIELDS}

            try:
                if file_updates:
                    existing = self._rewrite_track_row_from_file(existing, file_updates)
                if library_updates:
                    self._db.update_track_metadata(track_id, library_updates)
                self._db.commit()
            except MetadataWriteError as exc:
                QMessageBox.warning(
                    self,
                    "Could Not Save Tags",
                    f"Could not write metadata to the media file.\n\n{exc}",
                )
                self._status_bar.set_left_text("Metadata write failed")
                return

            self._refresh_view()
            self._status_bar.set_left_text("Metadata saved")
            logger.info("Updated metadata for track %d: %s", track_id, list(updates.keys()))

    def _write_library_metadata_back_to_files(self):
        reply = QMessageBox.question(
            self,
            "Write Library Metadata Back to Files",
            "Push RockPod's current library metadata back into the source media files for the whole library?\n\n"
            "This updates supported file formats in place and refreshes RockPod from what was actually saved.",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.Yes,
        )
        if reply != QMessageBox.Yes:
            return

        rows = [dict(row) for row in self._db.get_all_tracks(media_type=None)]
        if not rows:
            QMessageBox.information(self, "Write Library Metadata Back to Files", "No tracks are in the library.")
            return

        progress = QProgressDialog(
            "Writing library metadata back to media files...",
            "Cancel",
            0,
            len(rows),
            self,
        )
        progress.setWindowTitle("Write Library Metadata Back to Files")
        progress.setMinimumDuration(0)
        progress.setWindowModality(Qt.WindowModal)
        progress.setAutoClose(False)
        progress.setAutoReset(False)
        progress.show()

        updated = 0
        unsupported = 0
        failed = []
        cancelled = False

        try:
            for index, row in enumerate(rows, start=1):
                file_path = row.get("file_path", "")
                progress.setValue(index - 1)
                progress.setLabelText(
                    f"Writing metadata for {os.path.basename(file_path) or file_path or 'track'}"
                )
                QApplication.processEvents()
                if progress.wasCanceled():
                    cancelled = True
                    break

                try:
                    self._rewrite_track_row_from_file(row, row)
                    updated += 1
                except MetadataWriteError as exc:
                    message = str(exc)
                    if "not supported yet" in message:
                        unsupported += 1
                    else:
                        failed.append((file_path, message))
            self._db.commit()
        finally:
            progress.setValue(len(rows) if not cancelled else min(progress.value(), len(rows)))
            progress.close()
            progress.deleteLater()

        self._refresh_view()

        lines = [f"Updated files: {updated}"]
        if unsupported:
            lines.append(f"Unsupported formats: {unsupported}")
        if failed:
            lines.append(f"Failed writes: {len(failed)}")
        if cancelled:
            lines.append("Operation cancelled before all tracks were processed.")
        if failed:
            sample = failed[:8]
            details = "\n".join(f"{path}: {message}" for path, message in sample)
            if len(failed) > len(sample):
                details += f"\n...and {len(failed) - len(sample)} more"
            lines.append("")
            lines.append(details)

        self._status_bar.set_left_text(
            "Library metadata backfill cancelled" if cancelled else "Library metadata backfill finished"
        )
        QMessageBox.information(self, "Write Library Metadata Back to Files", "\n".join(lines))

    @staticmethod
    def _write_lyrics_sidecar(file_path, lyrics_text):
        sidecar = os.path.splitext(str(file_path or ""))[0] + ".lrc"
        os.makedirs(os.path.dirname(sidecar) or ".", exist_ok=True)
        with open(sidecar, "w", encoding="utf-8") as handle:
            handle.write(str(lyrics_text or "").strip())
        return sidecar

    def _mirror_lyrics_sidecar_to_connected_device(self, row, local_sidecar):
        device = self._device_detector.current_device
        device_rel = str(row.get("device_path") or "").strip()
        mount_path = str(getattr(device, "mount_path", "") or "").strip() if device else ""
        if not mount_path or not device_rel:
            return False
        device_sidecar = os.path.splitext(os.path.join(mount_path, device_rel))[0] + ".lrc"
        os.makedirs(os.path.dirname(device_sidecar), exist_ok=True)
        shutil.copyfile(local_sidecar, device_sidecar)
        return True

    def _fetch_library_timed_lyrics(self):
        reply = QMessageBox.question(
            self,
            "Fetch Timed Lyrics for Library",
            "Fetch timed lyrics for every audio track in the library and save them as .lrc sidecars beside the source files?\n\n"
            "Per-word lyrics are preferred. If per-word timing is unavailable, line-timed lyrics are saved instead. Plain unsynced lyrics are not saved.",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.Yes,
        )
        if reply != QMessageBox.Yes:
            return

        rows = [dict(row) for row in self._db.get_all_tracks(media_type="audio")]
        if not rows:
            QMessageBox.information(self, "Fetch Timed Lyrics for Library", "No audio tracks are in the library.")
            return

        lookup = self._album_metadata_fetcher._lookup_client
        progress = QProgressDialog(
            "Fetching timed lyrics...",
            "Cancel",
            0,
            len(rows),
            self,
        )
        progress.setWindowTitle("Fetch Timed Lyrics for Library")
        progress.setMinimumDuration(0)
        progress.setWindowModality(Qt.WindowModal)
        progress.setAutoClose(False)
        progress.setAutoReset(False)
        progress.show()

        saved = 0
        per_word_saved = 0
        line_timed_saved = 0
        mirrored = 0
        plain_only = 0
        missing_metadata = 0
        not_found = 0
        failed = []
        cancelled = False

        try:
            for index, row in enumerate(rows, start=1):
                file_path = row.get("file_path", "")
                title = str(row.get("title") or "").strip()
                artist = str(row.get("artist") or row.get("album_artist") or "").strip()
                progress.setValue(index - 1)
                progress.setLabelText(
                    f"Fetching lyrics for {os.path.basename(file_path) or title or 'track'}"
                )
                QApplication.processEvents()
                if progress.wasCanceled():
                    cancelled = True
                    break

                if not file_path or not title or not artist:
                    missing_metadata += 1
                    continue

                try:
                    detail = lookup.fetch_track_lyrics_detail(artist, title)
                    lyrics_text = str(detail.get("lyrics") or "").strip()
                    if not lyrics_text:
                        not_found += 1
                        continue
                    if not detail.get("timed"):
                        plain_only += 1
                        continue
                    sidecar = self._write_lyrics_sidecar(file_path, lyrics_text)
                    saved += 1
                    if detail.get("per_word"):
                        per_word_saved += 1
                    else:
                        line_timed_saved += 1
                    if row.get("synced_to_device") and row.get("device_path"):
                        if self._mirror_lyrics_sidecar_to_connected_device(row, sidecar):
                            mirrored += 1
                except Exception as exc:
                    failed.append((file_path, str(exc)))
        finally:
            progress.setValue(len(rows) if not cancelled else min(progress.value(), len(rows)))
            progress.close()
            progress.deleteLater()

        lines = [f"Timed lyrics saved: {saved}"]
        if per_word_saved:
            lines.append(f"Per-word timed: {per_word_saved}")
        if line_timed_saved:
            lines.append(f"Line-timed fallback: {line_timed_saved}")
        if mirrored:
            lines.append(f"Mirrored to connected iPod: {mirrored}")
        if plain_only:
            lines.append(f"Plain lyrics only (not saved): {plain_only}")
        if not_found:
            lines.append(f"No lyrics found: {not_found}")
        if missing_metadata:
            lines.append(f"Missing artist/title metadata: {missing_metadata}")
        if failed:
            lines.append(f"Failed fetches: {len(failed)}")
        if cancelled:
            lines.append("Operation cancelled before all tracks were processed.")
        if failed:
            sample = failed[:8]
            details = "\n".join(f"{path}: {message}" for path, message in sample)
            if len(failed) > len(sample):
                details += f"\n...and {len(failed) - len(sample)} more"
            lines.append("")
            lines.append(details)

        self._status_bar.set_left_text(
            "Timed lyrics fetch cancelled" if cancelled else "Timed lyrics fetch finished"
        )
        QMessageBox.information(self, "Fetch Timed Lyrics for Library", "\n".join(lines))

    def _select_all(self):
        self._active_track_table().selectAll()

    def _on_table_context_menu(self, pos):
        table = self.sender() if isinstance(self.sender(), TrackTable) else self._track_table
        index = table.indexAt(pos)
        if index.isValid() and table.selectionModel():
            selected_rows = {item.row() for item in table.selectionModel().selectedRows()}
            if index.row() not in selected_rows:
                table.selectionModel().select(
                    index,
                    QItemSelectionModel.ClearAndSelect | QItemSelectionModel.Rows,
                )
                table.setCurrentIndex(index)
        self._show_track_context_menu(table.viewport().mapToGlobal(pos))

    def _on_video_context_menu(self, target, global_pos):
        self._show_track_context_menu(global_pos, context_target=target)

    def _show_track_context_menu(self, global_pos, context_target=None):
        menu = QMenu(self)
        menu.addAction("Get Info...", self._show_track_info)
        menu.addSeparator()

        if self._current_view == "library_videos":
            video_target = self._video_artwork_target(context_target)
            if video_target:
                label = self._video_fetch_label(video_target)
                menu.addAction(
                    label,
                    lambda target=video_target: self._fetch_artwork_for_album(target, force=True),
                )
                menu.addAction(
                    "Inspect Poster",
                    lambda target=video_target: self._show_artwork_debug_for(target),
                )
                menu.addSeparator()

        # Add to playlist submenu
        playlists = self._db.get_all_playlists()
        if playlists:
            pl_menu = menu.addMenu("Add to Playlist")
            for pl in playlists:
                pid = pl["id"]
                pname = pl["name"]
                pl_menu.addAction(pname, lambda pid=pid: self._add_selection_to_playlist(pid))
            menu.addSeparator()

        menu.addAction("New Playlist from Selection", self._new_playlist_from_selection)
        menu.addAction("Sync", self._sync_selected)

        if self._current_view.startswith("playlist_"):
            menu.addSeparator()
            move_up = menu.addAction("Move Up", lambda: self._move_selection_in_playlist(-1))
            move_down = menu.addAction("Move Down", lambda: self._move_selection_in_playlist(1))
            can_reorder = bool(
                self._current_manual_playlist_id()
                and not self._toolbar.search_text.strip()
                and self._active_track_table().get_selected_track_ids()
            )
            move_up.setEnabled(can_reorder)
            move_down.setEnabled(can_reorder)
            menu.addAction("Remove from Playlist", self._remove_selection_from_playlist)

        if self._current_view == "device_music":
            menu.addSeparator()
            menu.addAction("Delete from Device", self._delete_selected_from_device)

        menu.exec(global_pos)

    def _video_artwork_target(self, context_target=None):
        target = context_target or {}
        if hasattr(target, "get") and target.get("entry_kind") == "show":
            tracks = [normalize_track_for_ui(track) for track in (target.get("tracks") or [])]
            label = str(target.get("label") or "")
            if not tracks:
                return {}
            return {
                "group_key": str(target.get("key") or f"show:{label.casefold()}"),
                "album": label or tracks[0].get("show_title") or "Unknown Show",
                "artist": "",
                "tracks": tracks,
                "media_type": "video",
                "video_kind": "show",
                "video_scope": "show",
            }

        if hasattr(target, "get") and target.get("tracks"):
            tracks = [normalize_track_for_ui(track) for track in (target.get("tracks") or [])]
        else:
            tracks = [normalize_track_for_ui(track) for track in self._video_view.get_selected_tracks()]

        if not tracks:
            return {}

        first = tracks[0]
        video_kind = str(first.get("video_kind") or "")
        if video_kind == "show" and first.get("show_title"):
            show_title = str(first.get("show_title") or "").strip() or "Unknown Show"
            show_tracks = [
                track for track in normalize_tracks_for_ui(self._video_view.current_tracks())
                if str(track.get("show_title") or "").strip() == show_title
            ] or tracks
            return {
                "group_key": f"show:{show_title.casefold()}",
                "album": show_title,
                "artist": "",
                "tracks": show_tracks,
                "media_type": "video",
                "video_kind": "show",
                "video_scope": "show",
            }

        if video_kind not in {"movie", "home_video"}:
            return {}

        title = str(first.get("title") or first.get("album") or "Untitled Video").strip()
        return {
            "group_key": str(first.get("video_group_key") or first.get("file_path") or title),
            "album": title,
            "artist": str(first.get("artist") or first.get("album_artist") or "").strip(),
            "tracks": tracks,
            "media_type": "video",
            "video_kind": video_kind,
            "video_scope": video_kind,
        }

    @staticmethod
    def _video_fetch_label(target):
        video_kind = str(target.get("video_kind") or "")
        if video_kind == "show":
            return "Fetch Poster for This Show"
        if video_kind == "movie":
            return "Fetch Poster for This Movie"
        return "Fetch Poster for This Video"

    def _on_album_context_menu(self, album, global_pos):
        if not album:
            return
        menu = QMenu(self)
        menu.addAction(
            "Fetch Album Metadata...",
            lambda album=album: self._fetch_album_metadata_for(album),
        )
        menu.addSeparator()
        menu.addAction(
            "Get Info...",
            lambda album=album: self._show_album_info_for(album),
        )
        menu.addSeparator()
        menu.addAction(
            "Fetch Artwork for This Album",
            lambda album=album: self._fetch_artwork_for_album(album, force=True),
        )
        menu.addAction(
            "Inspect Artwork",
            lambda album=album: self._show_artwork_debug_for(album),
        )
        menu.exec(global_pos)

    def _show_album_info_for(self, album):
        if not album:
            return
        art_path = self._artwork.get_artwork_for_album(album, "display")
        dialog = AlbumInfoDialog(album, art_path, self)
        dialog.exec()

    def _fetch_album_metadata_for(self, album):
        if not album:
            return
        album_title = str(album.get("album") or "").strip() or "Unknown Album"
        artist_name = str(album.get("artist") or "").strip() or "Unknown Artist"
        album_key = str(album.get("group_key") or album.get("key") or f"{artist_name}\0{album_title}")
        if not self._album_metadata_fetcher.start(album_key, album_title, artist_name):
            self._status_bar.set_left_text("Album metadata fetch already in progress")
            return
        self._pending_album_metadata[album_key] = dict(album)
        self._show_album_metadata_progress(album_title, artist_name)
        self._status_bar.set_left_text(f"Fetching metadata for {artist_name} - {album_title}")

    def _show_album_metadata_progress(self, album_title, artist_name):
        if self._album_metadata_progress is not None:
            try:
                self._album_metadata_progress.close()
                self._album_metadata_progress.deleteLater()
            except RuntimeError:
                pass
        dialog = QProgressDialog(
            f"Fetching online metadata and lyrics for {artist_name} - {album_title}",
            "",
            0,
            0,
            self,
        )
        dialog.setWindowTitle("Fetch Album Metadata")
        dialog.setCancelButton(None)
        dialog.setMinimumDuration(0)
        dialog.setWindowModality(Qt.WindowModal)
        dialog.setAutoClose(False)
        dialog.setAutoReset(False)
        dialog.show()
        self._album_metadata_progress = dialog

    def _dismiss_album_metadata_progress(self):
        if self._album_metadata_progress is None:
            return
        try:
            self._album_metadata_progress.close()
            self._album_metadata_progress.deleteLater()
        except RuntimeError:
            pass
        self._album_metadata_progress = None

    def _on_album_metadata_fetched(self, album_key, metadata):
        album = self._pending_album_metadata.pop(album_key, {})
        self._dismiss_album_metadata_progress()
        art_path = self._artwork.get_artwork_for_album(album, "display") if album else ""
        dialog = AlbumMetadataDialog(metadata, art_path, self)
        dialog.exec()
        album_title = metadata.get("album") or "Unknown Album"
        artist_name = metadata.get("artist") or "Unknown Artist"
        self._status_bar.set_left_text(f"Fetched metadata for {artist_name} - {album_title}")

    def _on_album_metadata_fetch_error(self, album_key, message):
        album = self._pending_album_metadata.pop(album_key, {})
        self._dismiss_album_metadata_progress()
        album_title = album.get("album") or "Unknown Album"
        artist_name = album.get("artist") or "Unknown Artist"
        self._status_bar.set_left_text(f"Album metadata fetch failed for {artist_name} - {album_title}")
        QMessageBox.warning(
            self,
            "Fetch Album Metadata",
            f"Could not fetch metadata for {artist_name} - {album_title}.\n\n{message}",
        )

    def _fetch_artwork_for_album(self, album, force=True):
        album_title = (album.get("album") or "Unknown Album").strip()
        artist_name = (album.get("artist") or "Unknown Artist").strip()
        is_video = str(album.get("media_type") or "") == "video"
        queued, reason = self._artwork.start_online_lookup_now(
            album,
            force=force,
            with_reason=True,
        )
        if queued:
            if reason == "started":
                if is_video:
                    self._status_bar.set_left_text(f"Fetching poster for {album_title}")
                else:
                    self._status_bar.set_left_text(
                        f"Fetching artwork for {artist_name} - {album_title}"
                    )
            elif reason == "already_running":
                if is_video:
                    self._status_bar.set_left_text(f"Poster fetch already in progress for {album_title}")
                else:
                    self._status_bar.set_left_text(
                        f"Artwork fetch already in progress for {artist_name} - {album_title}"
                    )
            return

        reason_text = reason.replace("_", " ") if reason else "not queued"
        if is_video:
            self._status_bar.set_left_text(
                f"Poster fetch not started for {album_title}: {reason_text}"
            )
        else:
            self._status_bar.set_left_text(
                f"Artwork fetch not started for {artist_name} - {album_title}: {reason_text}"
            )

    def _choose_android_source(self):
        configured = str(self._config.get("android_source_path", "") or "").strip()
        if configured and os.path.isdir(configured):
            return configured

        candidates = discover_android_sources(configured)
        if len(candidates) == 1:
            return candidates[0]
        if len(candidates) > 1:
            selected, ok = QInputDialog.getItem(
                self,
                "Select Android Phone",
                "Mounted Android storage:",
                candidates,
                0,
                False,
            )
            if ok and selected:
                return selected
            return ""

        return QFileDialog.getExistingDirectory(
            self,
            "Select Android Storage Root",
            configured or os.path.expanduser("~"),
        )

    def _show_android_import_progress(self, source_root):
        self._dismiss_android_import_progress()
        dialog = QProgressDialog(
            f"Scanning and converting Android media from {source_root}",
            "Cancel",
            0,
            0,
            self,
        )
        dialog.setWindowTitle("Import Android Media")
        dialog.setMinimumDuration(0)
        dialog.setWindowModality(Qt.WindowModal)
        dialog.setAutoClose(False)
        dialog.setAutoReset(False)
        dialog.canceled.connect(self._android_importer.cancel_import)
        dialog.show()
        self._android_import_progress = dialog

    def _dismiss_android_import_progress(self):
        if self._android_import_progress is None:
            return
        try:
            self._android_import_progress.close()
            self._android_import_progress.deleteLater()
        except RuntimeError:
            pass
        self._android_import_progress = None

    def _import_android_media(self):
        if not self._device_detector.is_connected:
            QMessageBox.warning(self, "Import Android Media", "Connect an iPod first.")
            return
        if self._android_importer.is_running:
            self._status_bar.set_left_text("Android import already running")
            return

        source_root = self._choose_android_source()
        if not source_root:
            return

        include_photos = bool(self._config.get("android_import_include_photos", True))
        include_videos = bool(self._config.get("android_import_include_videos", True))
        for_tiktok_plugin = bool(self._config.get("android_import_for_tiktok_plugin", False))
        if not include_photos and not include_videos:
            QMessageBox.warning(
                self,
                "Import Android Media",
                "Enable photo or video import in Preferences > Device > Android Import.",
            )
            return

        device = self._device_detector.current_device
        if not device:
            return

        device_dir = (
            os.path.join("Videos", "iPodTikTok")
            if for_tiktok_plugin
            else self._config.get("android_import_device_dir", "Videos/Android Phone")
        )
        photo_duration = float(self._config.get("android_photo_duration_seconds", 8.0) or 8.0)
        if source_root != self._config.get("android_source_path", ""):
            self._config.set("android_source_path", source_root)
            self._config.save()

        started = self._android_importer.start_import(
            source_root,
            device.mount_path,
            device_dir,
            include_photos=include_photos,
            include_videos=include_videos,
            photo_duration_seconds=photo_duration,
            for_tiktok_plugin=for_tiktok_plugin,
        )
        if not started:
            self._status_bar.set_left_text("Android import already running")
            return

        self._show_android_import_progress(source_root)
        target_label = " for iPodTikTok" if for_tiktok_plugin else ""
        self._status_bar.set_left_text(
            f"Importing Android media{target_label} from {os.path.basename(source_root) or source_root}"
        )

    def _on_android_import_progress(self, current, total, label):
        if self._android_import_progress is None:
            return
        self._android_import_progress.setRange(0, max(total, 1))
        self._android_import_progress.setValue(min(current, max(total, 1)))
        self._android_import_progress.setLabelText(label)

    def _on_android_import_finished(self, report):
        self._dismiss_android_import_progress()
        device = self._device_detector.current_device
        if device:
            device.refresh_space()
            self._refresh_device_storage_breakdown(device)
            self._update_device_summary()

        summary = summarize_android_import(report)
        if summary["empty"]:
            self._status_bar.set_left_text(summary["status_text"])
            QMessageBox.information(
                self,
                "Import Android Media",
                summary["dialog_text"],
            )
            return

        if summary["status_text"]:
            self._status_bar.set_left_text(summary["status_text"])
        if summary["warning"]:
            QMessageBox.warning(self, "Import Android Media", summary["dialog_text"])
        else:
            QMessageBox.information(self, "Import Android Media", summary["dialog_text"])

    def _on_android_import_error(self, message):
        self._dismiss_android_import_progress()
        self._status_bar.set_left_text(f"Android import failed: {message}")
        QMessageBox.warning(self, "Import Android Media", message)

    def _on_android_import_cancelled(self):
        self._status_bar.set_left_text("Cancelling Android import...")

    # ═══════════════════════════════════════════════════════════════
    # Playlists
    # ═══════════════════════════════════════════════════════════════

    def _new_playlist(self):
        default = self._next_playlist_name()
        name, ok = QInputDialog.getText(
            self, "New Playlist", "Playlist name:", text=default
        )
        if ok and name.strip():
            pid = self._db.create_playlist(name.strip())
            self._db.commit()
            self._refresh_playlists()
            self._export_shared_music_playlists(silent=True)
            self._sidebar.select_item(f"playlist_{pid}")
            self._on_sidebar_selection("playlist", f"playlist_{pid}")
            logger.info("Created playlist: %s (id=%d)", name.strip(), pid)

    def _add_selection_to_playlist(self, playlist_id):
        track_ids = self._active_track_table().get_selected_track_ids()
        self._add_track_ids_to_playlist(playlist_id, sorted(track_ids))

    def _add_track_ids_to_playlist(self, playlist_id, track_ids):
        if not track_ids:
            return
        added = self._db.add_tracks_to_playlist(playlist_id, track_ids)
        self._db.commit()
        self._refresh_playlists()
        if added:
            self._export_shared_music_playlists(silent=True)

    def _refresh_missing_artwork(self):
        albums = group_tracks_by_album(self._db.get_all_tracks())
        result = self._artwork.refresh_missing_artwork(albums, force=False)
        queued = result.get("queued", 0)
        skipped = result.get("skipped", 0)
        reason = result.get("reason", "")
        skip_reasons = result.get("skip_reasons", {})
        if reason == "disabled":
            self._status_bar.set_left_text("Online artwork lookup is disabled")
        elif reason == "background_disabled":
            self._status_bar.set_left_text("Background artwork lookup is disabled")
        elif queued:
            self._status_bar.set_left_text(f"Fetching artwork... ({queued} albums queued)")
        else:
            if skip_reasons:
                top_reason = max(skip_reasons.items(), key=lambda item: item[1])[0].replace("_", " ")
                self._status_bar.set_left_text(f"No artwork lookups queued ({skipped} skipped: {top_reason})")
            else:
                self._status_bar.set_left_text(f"No artwork lookups queued ({skipped} skipped)")

    def _video_artwork_groups(self):
        tracks = normalize_tracks_for_ui(
            self._db.get_tracks_by_media_type("video", order_by="artist, album, disc_number, track_number, title")
        )
        grouped = build_video_browser_groups(tracks)
        targets = []
        for group_key, group in sorted(grouped["show"].items(), key=lambda item: item[1]["label"].casefold()):
            targets.append(
                {
                    "group_key": group_key,
                    "album": group["label"],
                    "artist": "",
                    "tracks": group["tracks"],
                    "media_type": "video",
                    "video_kind": "show",
                    "video_scope": "show",
                }
            )
        for track in grouped["movie"]:
            track = normalize_track_for_ui(track)
            targets.append(
                {
                    "group_key": str(track.get("video_group_key") or track.get("file_path") or track.get("title") or "movie"),
                    "album": str(track.get("title") or track.get("album") or "Untitled Movie"),
                    "artist": str(track.get("artist") or track.get("album_artist") or ""),
                    "tracks": [track],
                    "media_type": "video",
                    "video_kind": "movie",
                    "video_scope": "movie",
                }
            )
        return targets

    def _regenerate_ipod_artwork(self):
        if not self._device_detector.is_connected:
            return
        plan = self._sync_engine.build_sync_plan()
        plan.to_copy = []
        plan.to_resync = []
        plan.to_delete = []
        plan.up_to_date = []
        plan.total_bytes = 0
        if not plan.artwork_to_copy:
            self._status_bar.set_left_text("iPod artwork is up to date")
            return
        self._status_bar.set_left_text(plan.summary())
        dialog = SyncDialog(plan, self)
        dialog.sync_confirmed.connect(lambda: self._execute_sync(plan, dialog))
        dialog.sync_cancelled.connect(lambda: self._sync_engine.cancel_sync())
        dialog.exec()

    def _on_album_artwork_updated(self, album_key, path):
        if self._current_view in {"library_albums", "library_videos"}:
            self._refresh_view()
        selected = self._active_track_table().get_selected_tracks()
        if selected:
            track = selected[0]
            art_path = self._artwork.get_artwork_path(track, "thumb")
            self._toolbar.set_selected_track(track, art_path)
        self._status_bar.set_left_text("Artwork updated")

    def _on_album_artwork_failed(self, album_key, reason):
        logger.info("Artwork lookup failed for %s: %s", album_key, reason)
        self._status_bar.set_left_text(f"Artwork lookup failed: {reason}")

    def _on_artwork_lookup_status(self, message):
        if message:
            self._status_bar.set_left_text(message)

    def _show_selected_artwork_debug(self):
        selected = self._active_track_table().get_selected_tracks()
        target = None
        if selected:
            target = selected[0]
        elif self._current_view == "library_albums":
            tracks = self._album_view.current_tracks()
            if tracks:
                target = tracks
        elif self._current_view in ("library_artists", "library_genres"):
            tracks = self._tracks_for_current_view(include_search=True)
            if tracks:
                target = [tracks[0]]
        if target is None:
            self._status_bar.set_left_text("No track or album selected")
            return

        self._show_artwork_debug_for(target)

    def _show_artwork_debug_for(self, target):
        if target is None:
            self._status_bar.set_left_text("No track or album selected")
            return

        info = self._artwork.inspect_artwork(target)
        lines = [
            f"Album: {info.get('album', '')}",
            f"Artist: {info.get('artist', '')}",
            f"Source type: {info.get('desktop_source_type', '') or 'placeholder'}",
            f"Desktop source: {info.get('desktop_source_art_path', '') or '(none)'}",
            f"Source resolution: {self._format_resolution(info.get('desktop_source_resolution'))}",
            f"Thumb cache: {info.get('desktop_thumb_path', '') or '(none)'}",
            f"Thumb resolution: {self._format_resolution(info.get('desktop_thumb_resolution'))}",
            f"Display cache: {info.get('desktop_display_path', '') or '(none)'}",
            f"Display resolution: {self._format_resolution(info.get('desktop_display_resolution'))}",
            f"Display from hi-res source: {'Yes' if info.get('display_is_hi_res_thumb') else 'No'}",
            f"Device cover: {info.get('device_cover_export_path', '') or '(none)'}",
            f"Device cover resolution: {self._format_resolution(info.get('device_cover_resolution'))}",
            f"Device cover derived from higher-res source: {'Yes' if info.get('device_cover_from_high_res') else 'No'}",
            f"Online attempted: {'Yes' if info.get('online_lookup_attempted') else 'No'}",
            f"Online query: {info.get('online_query', '') or '(none)'}",
            f"Online match found: {'Yes' if info.get('online_match_found') else 'No'}",
            f"Online selected URL: {info.get('online_selected_url', '') or '(none)'}",
            f"Online selected size: {info.get('online_selected_size', '') or '(none)'}",
            f"Online download succeeded: {'Yes' if info.get('online_download_succeeded') else 'No'}",
            f"Online cached path: {info.get('online_cached_path', '') or '(none)'}",
            f"Online status: {info.get('online_status', '') or '(none)'}",
            f"Online error: {info.get('online_error', '') or '(none)'}",
        ]
        QMessageBox.information(self, "Artwork Diagnostics", "\n".join(lines))

    @staticmethod
    def _format_resolution(resolution):
        if not resolution:
            return "(unknown)"
        return f"{resolution[0]}x{resolution[1]}"

    def _new_playlist_from_selection(self):
        track_ids = sorted(self._active_track_table().get_selected_track_ids())
        if not track_ids:
            return
        pid = self._db.create_playlist(self._next_playlist_name())
        self._db.add_tracks_to_playlist(pid, track_ids)
        self._db.commit()
        self._refresh_playlists()
        self._export_shared_music_playlists(silent=True)
        self._sidebar.select_item(f"playlist_{pid}")
        self._on_sidebar_selection("playlist", f"playlist_{pid}")

    def _remove_selection_from_playlist(self):
        if not self._current_view.startswith("playlist_"):
            return
        try:
            playlist_id = int(self._current_view.replace("playlist_", ""))
        except ValueError:
            return
        track_ids = self._active_track_table().get_selected_track_ids()
        for tid in track_ids:
            self._db.remove_track_from_playlist(playlist_id, tid)
        self._playback.remove_tracks_from_queue(track_ids)
        self._db.commit()
        self._refresh_playlists()
        self._export_shared_music_playlists(silent=True)
        self._refresh_view()
        self._status_bar.set_left_text(f"Removed {len(track_ids)} track(s) from playlist")

    @staticmethod
    def _playlist_order_after_selection_move(current_ids, selected_ids, direction):
        ordered = list(current_ids or [])
        selected = set(selected_ids or [])
        if not ordered or not selected:
            return ordered
        if direction < 0:
            for index in range(1, len(ordered)):
                if ordered[index] in selected and ordered[index - 1] not in selected:
                    ordered[index - 1], ordered[index] = ordered[index], ordered[index - 1]
        elif direction > 0:
            for index in range(len(ordered) - 2, -1, -1):
                if ordered[index] in selected and ordered[index + 1] not in selected:
                    ordered[index + 1], ordered[index] = ordered[index], ordered[index + 1]
        return ordered

    def _move_selection_in_playlist(self, direction):
        playlist_id = self._current_manual_playlist_id()
        if not playlist_id:
            self._status_bar.set_left_text("Only regular playlists can be reordered")
            return
        if self._toolbar.search_text.strip():
            self._status_bar.set_left_text("Clear search before reordering a playlist")
            return

        selected_ids = self._active_track_table().get_selected_track_ids()
        if not selected_ids:
            return

        current_ids = [
            normalize_track_for_ui(track).get("id")
            for track in self._db.get_playlist_tracks(playlist_id)
        ]
        current_ids = [track_id for track_id in current_ids if track_id]
        new_order = self._playlist_order_after_selection_move(current_ids, selected_ids, direction)
        if new_order == current_ids:
            return

        updated = self._db.set_playlist_track_order(playlist_id, new_order)
        if not updated:
            return
        self._db.commit()
        self._refresh_playlists()
        self._export_shared_music_playlists(silent=True)
        self._refresh_view()
        self._track_table.select_track_ids(selected_ids)
        label = "Moved up" if direction < 0 else "Moved down"
        self._status_bar.set_left_text(f"{label} {len(selected_ids)} playlist track(s)")

    def _rename_playlist(self, playlist_id):
        playlist = self._db.get_playlist(playlist_id)
        if not playlist:
            return
        name, ok = QInputDialog.getText(
            self, "Rename Playlist", "Playlist name:", text=playlist["name"]
        )
        if ok and name.strip():
            self._db.rename_playlist(playlist_id, name.strip())
            self._db.commit()
            self._refresh_playlists()
            self._export_shared_music_playlists(silent=True)
            self._toolbar.set_title(name.strip())

    def _delete_playlist(self, playlist_id):
        playlist = self._db.get_playlist(playlist_id)
        if not playlist:
            return
        reply = QMessageBox.question(
            self,
            "Delete Playlist",
            f"Delete playlist \"{playlist['name']}\"?\n\nTracks will remain in your library.",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return
        self._db.delete_playlist(playlist_id)
        self._db.commit()
        self._refresh_playlists()
        self._export_shared_music_playlists(silent=True)
        self._current_view = "library_music"
        self._sidebar.select_item("library_music")
        self._toolbar.set_title("Music")
        self._refresh_view()

    def _next_playlist_name(self):
        existing = {pl["name"] for pl in self._db.get_all_playlists()}
        base = "Untitled Playlist"
        if base not in existing:
            return base
        i = 2
        while f"{base} {i}" in existing:
            i += 1
        return f"{base} {i}"

    def _refresh_playlists(self):
        self._sidebar.clear_playlists()
        playlists = self._db.get_all_playlists()
        regular_ids = [pl["id"] for pl in playlists if not pl["is_smart"]]
        regular_counts = self._db.get_playlist_track_counts(regular_ids)
        for pl in playlists:
            if pl["is_smart"]:
                count = evaluate_playlist_count(
                    self._db,
                    pl,
                    device_id=self._sync_engine.current_device_key,
                )
            else:
                count = regular_counts.get(pl["id"], 0)
            self._sidebar.add_playlist(
                f"{pl['name']} ({count})" if count else pl["name"], pl["id"]
            )
        self._refresh_device_playlists()

    def _export_shared_music_playlists(self, silent=False):
        try:
            result = export_local_music_playlists(
                self._db,
                self._config.music_dir,
                include_smart=False,
                device_id=self._sync_engine.current_device_key,
                convert_for_apple_music=True,
                ffmpeg_path=self._config.get("ffmpeg_binary", ""),
            )
        except Exception as exc:
            logger.warning("Shared music playlist export failed: %s", exc)
            if not silent:
                self._status_bar.set_left_text("Shared music playlist export failed")
            return {"success": False, "exported": [], "removed": [], "failures": [str(exc)]}

        if not result.get("success"):
            logger.warning("Shared music playlist export failed: %s", result.get("failures", []))
            if not silent:
                self._status_bar.set_left_text("Shared music playlist export failed")
        elif not silent:
            count = len(result.get("exported") or [])
            self._status_bar.set_left_text(f"Exported {count} shared music playlist(s)")
        return result

    def _refresh_device_playlists(self):
        self._sidebar.clear_device_playlists()
        device_key = self._sync_engine.current_device_key
        if not device_key:
            return
        for playlist in self._db.get_device_playlists(device_key):
            self._sidebar.add_device_playlist(
                playlist["name"],
                playlist["id"],
                playlist["track_count"],
            )

    def _on_sidebar_context_menu(self, item_id, global_pos):
        menu = QMenu(self)
        if not item_id or item_id == "section":
            menu.addAction("New Playlist", self._new_playlist)
        elif item_id.startswith("playlist_"):
            playlist_id = int(item_id.replace("playlist_", ""))
            playlist = self._db.get_playlist(playlist_id)
            playlist_data = dict(playlist) if playlist is not None else {}
            show_action = menu.addAction(
                "Show in Rockbox Playlists",
                lambda checked=False, pid=playlist_id: self._set_playlist_rockbox_sync(pid, checked),
            )
            show_action.setCheckable(True)
            show_action.setChecked(bool(playlist_data.get("sync_to_rockbox", 1)))
            sync_playlists_action = menu.addAction("Sync Rockbox Playlists Now", self._sync_rockbox_playlists_now)
            sync_playlists_action.setEnabled(
                bool(
                    self._device_detector.current_device
                    and getattr(self._device_detector.current_device, "is_rockbox", False)
                    and self._device_config_value("sync_playlists_to_device", True)
                )
            )
            menu.addSeparator()
            menu.addAction("Rename Playlist", lambda: self._rename_playlist(playlist_id))
            menu.addAction("Delete Playlist", lambda: self._delete_playlist(playlist_id))
            menu.addSeparator()
            menu.addAction("New Playlist", self._new_playlist)
        elif item_id.startswith("library_") or item_id.startswith("device_"):
            menu.addAction("Refresh", self._refresh_library)
            menu.addAction("Sync", self._start_sync)
            if item_id.startswith("device_"):
                menu.addAction("Force Device Rescan", self._force_device_rescan)
                menu.addAction("Forget Device", self._forget_device)
            menu.addSeparator()
            menu.addAction("New Playlist", self._new_playlist)
        else:
            menu.addAction("New Playlist", self._new_playlist)
        menu.exec(global_pos)

    def _set_playlist_rockbox_sync(self, playlist_id, enabled):
        playlist = self._db.get_playlist(playlist_id)
        if not playlist:
            return
        self._db.set_playlist_sync_to_rockbox(playlist_id, enabled)
        self._db.commit()
        self._refresh_playlists()
        if (
            self._device_detector.current_device
            and getattr(self._device_detector.current_device, "is_rockbox", False)
            and self._device_config_value("sync_playlists_to_device", True)
        ):
            self._sync_rockbox_playlists_now()
            return
        state = "shown" if enabled else "hidden"
        self._status_bar.set_left_text(f"{playlist['name']} will be {state} in Rockbox Playlists")

    def _sync_rockbox_playlists_now(self):
        device = self._device_detector.current_device
        if not device or not getattr(device, "is_rockbox", False):
            self._status_bar.set_left_text("No Rockbox device is connected")
            return
        if self._sync_engine.is_syncing:
            return
        track_ids = self._sync_engine.rockbox_playlist_track_ids()
        if track_ids:
            plan = self._build_sync_plan_with_feedback(track_ids=track_ids)
            if plan is None:
                return
            if self._sync_plan_has_blocking_errors(plan):
                return
            if plan.total_operations:
                self._status_bar.set_left_text(plan.summary())
                dialog = SyncDialog(plan, self)
                dialog.sync_confirmed.connect(lambda: self._execute_sync(plan, dialog))
                dialog.sync_cancelled.connect(lambda: self._sync_engine.cancel_sync())
                dialog.exec()
                return
        result = self._sync_rockbox_playlists(device)
        if result.get("success"):
            count = len(result.get("exported") or [])
            self._status_bar.set_left_text(f"Rockbox playlists updated: {count} playlist(s)")
        else:
            self._status_bar.set_left_text("Rockbox playlist update failed")

    # ═══════════════════════════════════════════════════════════════
    # Device operations
    # ═══════════════════════════════════════════════════════════════

    def _on_device_connected(self, device):
        logger.info("Device connected: %s", device)
        self._device_state = "Connected"
        self._rockbox_db_stale = False
        self._runtime_refresh_pending = False
        self._rockbox_db_monitor.start()
        self._sidebar.set_device_name(device.name)
        self._rockbox_profiles.sync_with_device(device)
        self._refresh_browser_panel()
        self._toolbar.set_sync_enabled(True)
        self._toolbar.set_eject_visible(True)

        device_key = self._sync_engine.set_current_device(device)
        self._refresh_device_storage_breakdown(device)
        cached = len(self._sync_engine.get_device_tracks())
        self._device_verification_running = False
        self._status_bar.set_left_text(f"Using cached device inventory ({cached} tracks)")
        self._update_status_bar()
        self._refresh_device_playlists()
        if self._current_view in ("device_music", "device_not_on_ipod"):
            self._refresh_view()
        if self._current_view == "device_root":
            self._update_device_summary()
        self._refresh_current_rockbox_panel()

        # Only verify in the background if the user enabled it explicitly.
        if self._device_config_value("verify_device_in_background", False):
            QTimer.singleShot(50, self._scan_device)

        # Auto-sync if configured
        if self._device_config_value("auto_sync_on_connect", False):
            QTimer.singleShot(1000, self._start_sync)

    def _on_device_disconnected(self, path):
        logger.info("Device disconnected: %s", path)
        self._device_state = "Disconnected"
        self._rockbox_db_stale = False
        self._runtime_refresh_pending = False
        self._rockbox_db_monitor.stop()
        self._sidebar.set_device_disconnected()
        self._toolbar.set_sync_enabled(False)
        self._toolbar.set_eject_visible(False)
        self._refresh_browser_panel()
        self._device_verification_running = False
        if self._sync_engine.current_device_key:
            cached = len(self._sync_engine.get_device_tracks())
            self._status_bar.set_left_text(f"Using cached device inventory ({cached} tracks)")
        else:
            self._status_bar.set_left_text("")
        self._update_status_bar()
        self._refresh_view()
        self._refresh_device_playlists()
        if self._current_view == "device_root":
            self._update_device_summary()
        self._refresh_current_rockbox_panel()

    def _refresh_current_rockbox_panel(self):
        if self._current_view == "rockbox_themes":
            self._refresh_theme_hub()
        elif self._current_view == "rockbox_wallpapers":
            self._refresh_ipone_wallpapers()
        elif self._current_view == "rockbox_theme_designer":
            self._refresh_theme_designer()
        elif self._current_view == "rockbox_boot":
            self._refresh_boot_manager()
        elif self._current_view == "rockbox_plugins":
            self._refresh_plugin_manager()
        elif self._current_view == "rockbox_game_sync":
            self._refresh_game_manager()
        elif self._current_view == "rockbox_games":
            self._refresh_game_browser_panel()
        elif self._current_view == "rockbox_photos":
            self._refresh_photo_manager()
        elif self._current_view == "rockbox_linux":
            self._refresh_linux_manager()
        elif self._current_view == "rockbox_simulator":
            self._refresh_simulator_panel()

    def _on_device_space_update(self, device):
        device_key = self._sync_engine.current_device_key
        if device_key and device_key in self._device_storage_cache:
            cached = dict(self._device_storage_cache[device_key])
            cached["total"] = int(getattr(device, "total_space", 0) or cached.get("total") or 0)
            cached["free"] = int(getattr(device, "free_space", 0) or cached.get("free") or 0)
            cached["used"] = max(cached["total"] - cached["free"], sum(int(cached.get(key, 0) or 0) for key in ("music", "games_plugins", "themes_assets", "rockbox_system", "linux_system", "other")))
            self._device_storage_cache[device_key] = cached
        if self._current_view == "device_root":
            self._update_device_summary()

    def _update_storage_bar(self, device):
        return

    def _cached_device_storage(self, device, device_key, device_row=None):
        cached = self._device_storage_cache.get(device_key or "")
        if cached:
            return cached
        device_row = dict(device_row) if device_row is not None and hasattr(device_row, "keys") else (device_row or {})
        return {
            "total": int(getattr(device, "total_space", 0) or device_row.get("capacity_bytes") or 0),
            "used": int(getattr(device, "used_space", 0) or 0),
            "free": int(getattr(device, "free_space", 0) or device_row.get("free_bytes_last_seen") or 0),
            "music": 0,
            "games_plugins": 0,
            "themes_assets": 0,
            "rockbox_system": 0,
            "linux_system": 0,
            "other": 0,
            "scanned_at": "",
        }

    def _refresh_device_storage_breakdown(self, device=None):
        device = device or self._device_detector.current_device
        if not device:
            return
        device_key = self._sync_engine.current_device_key
        if not device_key:
            return
        self._device_storage_analyzer.start(
            device_key,
            getattr(device, "mount_path", ""),
            getattr(device, "total_space", 0),
            getattr(device, "free_space", 0),
        )

    def _on_device_storage_finished(self, device_key, result):
        self._device_storage_cache[device_key] = result
        if device_key == self._sync_engine.current_device_key and self._current_view == "device_root":
            self._update_device_summary()

    def _on_device_storage_error(self, device_key, message):
        logger.warning("Device storage analysis failed for %s: %s", device_key, message)

    def _scan_device(self):
        if not self._device_detector.is_connected:
            return
        if self._device_inventory.is_running:
            self._status_bar.set_left_text("Using cached device inventory")
            return
        self._status_bar.set_left_text("Using cached device inventory")
        self._device_verification_running = True
        self._update_sync_status()
        self._device_inventory.start(self._device_detector.current_device, force_full=False)

    def _force_device_rescan(self):
        if not self._device_detector.is_connected:
            return
        if self._device_inventory.is_running:
            self._status_bar.set_left_text("Device refresh already running...")
            return
        self._status_bar.set_left_text("Force rescanning device...")
        self._device_verification_running = True
        self._update_sync_status()
        self._device_inventory.start(self._device_detector.current_device, force_full=True)

    def _on_device_inventory_status(self, message):
        if message:
            self._status_bar.set_left_text(message)

    def _forget_device(self):
        if not self._device_detector.current_device:
            return
        device_key = self._sync_engine.set_current_device(self._device_detector.current_device)
        reply = QMessageBox.question(
            self,
            "Forget Device",
            "Forget cached inventory for this iPod?\n\n"
            "Music files on the device will not be deleted.",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return
        self._db.forget_device(device_key)
        self._db.commit()
        self._sync_engine.clear_device_index(forget_current=True)
        self._status_bar.set_left_text("Device forgotten")
        self._refresh_view()
        self._update_sync_status()

    def _on_device_inventory_finished(self, summary):
        self._device_verification_running = False
        if self._device_state not in ("Syncing", "Sync Complete (DB stale)", "Updating Database"):
            self._device_state = "Ready" if self._device_detector.is_connected else "Disconnected"
        device_key = summary.get("device_key", "")
        if device_key:
            self._sync_engine.load_cached_device_inventory(device_key)
        count = summary.get("scanned", 0)
        changes = summary.get("new", 0) + summary.get("changed", 0) + summary.get("missing", 0)
        if changes:
            self._status_bar.set_left_text(f"Device refreshed: {count} tracks, {changes} changes")
        else:
            self._status_bar.set_left_text(f"Device up to date: {count} tracks")
        self._refresh_device_storage_breakdown(self._device_detector.current_device)
        self._update_status_bar()
        self._update_sync_status()
        self._refresh_playlists()
        if self._current_view in ("device_music", "device_not_on_ipod"):
            self._refresh_view()
        if self._current_view == "device_root":
            self._update_device_summary()

    def _on_device_inventory_error(self, message):
        self._device_verification_running = False
        self._status_bar.set_left_text(f"Device refresh failed: {message}")
        self._update_sync_status()

    def _show_device_diff(self):
        device_key = self._sync_engine.current_device_key
        if not device_key:
            self._status_bar.set_left_text("No device inventory available")
            return
        report = build_reconciliation_report(
            self._db,
            device_key,
            self._device_config_value("duplicate_strictness", "metadata_and_hash"),
            self._device_config_value("duration_match_tolerance_seconds", 2.0),
            sample_limit=25,
        )
        sync_plan = self._sync_engine.build_sync_plan()
        box = QMessageBox(self)
        box.setWindowTitle("Device Diff")
        box.setIcon(QMessageBox.Information)
        box.setText(format_reconciliation_report(report, sync_plan=sync_plan))
        box.exec()

    def _remove_duplicate_device_tracks(self):
        if not self._device_detector.is_connected:
            QMessageBox.warning(self, "No Device", "No Rockbox device is connected.")
            return
        duplicates = self._sync_engine.find_duplicate_device_tracks()
        if not duplicates:
            QMessageBox.information(self, "Duplicate Cleanup", "No duplicate device tracks were found.")
            self._status_bar.set_left_text("No duplicate device tracks found")
            return

        duplicate_files = sum(len(group["remove"]) for group in duplicates)
        sample_lines = []
        for group in duplicates[:20]:
            kept = group["keep"][0]
            label = " - ".join(part for part in (kept.get("artist", ""), kept.get("album", ""), kept.get("title", "")) if part)
            sample_lines.append(f"{label or kept.get('device_path', '(unknown)')}: keep {group['expected_count']}, remove {len(group['remove'])}")
        detail = "\n".join(sample_lines)
        if len(duplicates) > 20:
            detail += f"\n... and {len(duplicates) - 20} more groups"

        reply = QMessageBox.question(
            self,
            "Remove Duplicate Device Tracks",
            f"Found {duplicate_files} extra device files across {len(duplicates)} duplicate groups.\n\n"
            "Remove the extra copies and keep one canonical copy per track?\n\n"
            f"{detail}",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return

        result = self._sync_engine.remove_duplicate_device_tracks()
        if result["success"]:
            QMessageBox.information(
                self,
                "Duplicate Cleanup",
                f"Removed {len(result['deleted'])} duplicate device files.",
            )
            self._status_bar.set_left_text(f"Removed {len(result['deleted'])} duplicate device files")
        else:
            QMessageBox.warning(
                self,
                "Duplicate Cleanup",
                f"Removed {len(result['deleted'])} duplicate device files.\n\n"
                + "\n".join(result["failures"][:10]),
            )
            self._status_bar.set_left_text("Duplicate cleanup completed with errors")
        self._refresh_view()
        self._update_status_bar()
        self._update_sync_status()
        self._refresh_playlists()
        if self._current_view == "device_root":
            self._update_device_summary()

    def _clear_rockbox_database_cache(self):
        device = self._device_detector.current_device
        if not device:
            QMessageBox.warning(self, "No Device", "No Rockbox device is connected.")
            return
        if not getattr(device, "is_rockbox", False):
            QMessageBox.warning(self, "Not Rockbox", "The connected device does not appear to be a Rockbox device.")
            return

        reply = QMessageBox.question(
            self,
            "Clear Rockbox Database Cache",
            "Delete Rockbox tagcache/database cache files from the device?\n\n"
            "This does not remove your music files. Rockbox will need to rebuild its database afterward.",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return

        result = clear_rockbox_database_cache(device)
        self._rockbox_db_stale = True
        self._runtime_refresh_pending = False
        self._device_state = "Sync Complete (DB stale)"
        self._update_sync_status()
        self._update_device_summary()

        if result["success"]:
            QMessageBox.information(
                self,
                "Rockbox Database Cache Cleared",
                f"Removed {len(result['removed'])} Rockbox database cache files.\n\n"
                "Rebuild the database on the iPod, or let Rockbox auto-update if enabled.",
            )
            self._status_bar.set_left_text("Rockbox database cache cleared")
        else:
            QMessageBox.warning(
                self,
                "Rockbox Database Cache",
                (
                    f"Removed {len(result['removed'])} Rockbox database cache files.\n\n"
                    + "\n".join(result["failures"][:10])
                ),
            )
            self._status_bar.set_left_text("Rockbox database cache clear completed with errors")

    def _show_missing_tag_report(self):
        report = build_missing_tag_report(
            self._db,
            codecs=("FLAC", "AIFF"),
            sample_limit=200,
        )
        summary = (
            "No missing FLAC/AIFF tags found."
            if report.get("tracks_with_missing_tags_count", 0) == 0
            else (
                f"Found {report.get('tracks_with_missing_tags_count', 0)} FLAC/AIFF tracks "
                "with missing non-artwork tags."
            )
        )
        box = QMessageBox(self)
        box.setWindowTitle("Missing FLAC/AIFF Tags")
        box.setIcon(QMessageBox.Information)
        box.setText(summary)
        box.setDetailedText(format_missing_tag_report(report))
        box.exec()

    def _start_sync(self):
        if self._sync_engine.is_syncing:
            return
        if not self._device_detector.is_connected:
            QMessageBox.warning(self, "No Device", "No Rockbox device is connected.")
            return

        plan = self._build_sync_plan_with_feedback()
        if plan is None:
            return
        if self._sync_plan_has_blocking_errors(plan):
            return
        if plan.total_operations == 0 and not plan.errors:
            self._status_bar.set_left_text("Up to date")
            return
        self._status_bar.set_left_text(plan.summary())

        dialog = SyncDialog(plan, self)
        dialog.sync_confirmed.connect(lambda: self._execute_sync(plan, dialog))
        dialog.sync_cancelled.connect(lambda: self._sync_engine.cancel_sync())
        dialog.exec()

    def _sync_weather_only(self):
        if self._sync_engine.is_syncing:
            return
        if not self._device_detector.is_connected:
            QMessageBox.warning(self, "No Device", "No Rockbox device is connected.")
            return

        plan = self._build_sync_plan_with_feedback(weather_only=True)
        if plan is None:
            return
        if self._sync_plan_has_blocking_errors(plan):
            return
        if plan.total_operations == 0:
            self._status_bar.set_left_text("Weather forecast is up to date")
            return
        self._status_bar.set_left_text(plan.summary())
        dialog = SyncDialog(plan, self)
        dialog.sync_confirmed.connect(lambda: self._execute_sync(plan, dialog))
        dialog.sync_cancelled.connect(lambda: self._sync_engine.cancel_sync())
        dialog.exec()

    def _sync_selected(self):
        track_ids = self._active_track_table().get_selected_track_ids()
        if not track_ids:
            return
        if not self._device_detector.is_connected:
            QMessageBox.warning(self, "No Device", "No Rockbox device is connected.")
            return
        media_type = "audio"
        if self._current_view == "library_videos":
            media_type = "video"

        plan = self._build_sync_plan_with_feedback(
            track_ids=track_ids, media_type=media_type
        )
        if plan is None:
            return
        if self._sync_plan_has_blocking_errors(plan):
            return
        if plan.total_operations == 0:
            self._status_bar.set_left_text("Selected tracks are already synced")
            return
        self._status_bar.set_left_text(plan.summary())

        dialog = SyncDialog(plan, self)
        dialog.sync_confirmed.connect(lambda: self._execute_sync(plan, dialog))
        dialog.sync_cancelled.connect(lambda: self._sync_engine.cancel_sync())
        dialog.exec()

    def _refresh_video_sync_panel(self):
        videos = self._video_sync_candidates(
            self._db.get_tracks_by_media_type(
                "video",
                order_by="artist, album, disc_number, track_number, title",
            )
        )
        self._video_sync_panel.set_videos(videos)
        if not videos:
            self._video_sync_panel.set_status("No videos in the library. Add movies from the Store or scan your video folders.")
        elif not self._device_detector.is_connected:
            self._video_sync_panel.set_status("Connect an iPod to preview or sync selected videos.")
        else:
            missing = sum(1 for video in videos if not video.get("synced_to_device"))
            self._video_sync_panel.set_status(
                f"{missing} video{'s' if missing != 1 else ''} not on iPod."
            )

    def _video_sync_candidates(self, rows):
        video_roots = [
            os.path.abspath(os.path.expanduser(path))
            for path in (getattr(self._config, "video_dirs", None) or [self._config.video_dir])
            if path
        ]
        youtube_roots = [os.path.join(root, "YouTube") for root in video_roots]
        candidates = []
        for row in normalize_tracks_for_ui(rows):
            item = dict(row)
            file_path = os.path.abspath(os.path.expanduser(str(item.get("file_path") or "")))
            if not file_path.lower().endswith(".mpg"):
                continue
            if not any(self._path_is_relative_to(file_path, root) for root in youtube_roots):
                continue
            title = str(item.get("title") or "").strip().casefold()
            if title in {"", "youtube", "mpeg video", "youtube mpeg video"}:
                item["title"] = os.path.splitext(os.path.basename(file_path))[0]
            item["video_sync_label"] = "Downloaded"
            item["video_sync_category"] = "Downloaded"
            candidates.append(item)
        return candidates

    @staticmethod
    def _path_is_relative_to(path, root):
        try:
            return os.path.commonpath([path, os.path.abspath(root)]) == os.path.abspath(root)
        except ValueError:
            return False

    def _preview_video_sync(self, track_ids):
        self._show_video_sync_plan(track_ids)

    def _sync_video_track_ids(self, track_ids):
        self._show_video_sync_plan(track_ids)

    def _force_repair_video_track_ids(self, track_ids):
        self._show_video_sync_plan(track_ids, force_full=True)

    def _video_rows_for_ids(self, track_ids):
        ids = {int(track_id) for track_id in (track_ids or []) if int(track_id or 0)}
        if not ids:
            return []
        return [
            dict(row)
            for row in self._db.get_tracks_by_ids(ids, media_type=None)
            if str(row.get("media_type") or "") == "video"
        ]

    def _remove_video_track_ids_from_device(self, track_ids):
        rows = self._video_rows_for_ids(track_ids)
        targets = [row for row in rows if row.get("synced_to_device") or row.get("device_path")]
        if not targets:
            self._video_sync_panel.set_status("Select synced videos to remove from the iPod.")
            return
        if not self._device_detector.is_connected:
            self._video_sync_panel.set_status("Connect an iPod to remove selected videos.")
            QMessageBox.warning(self, "No Device", "No Rockbox device is connected.")
            return

        reply = QMessageBox.question(
            self,
            "Remove from iPod",
            f"Remove {len(targets)} selected video{'s' if len(targets) != 1 else ''} from the iPod?\n\n"
            "The local video files stay in your RockPod library.",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return

        deleted = self._remove_video_rows_from_device(targets)
        self._refresh_video_sync_panel()
        self._refresh_view()
        self._status_bar.set_left_text(f"Removed {deleted} video{'s' if deleted != 1 else ''} from iPod")

    def _remove_video_rows_from_device(self, rows):
        device_tracks = [dict(row) for row in self._sync_engine.get_device_tracks()]
        by_local_id = {
            int(row.get("local_track_id") or 0): row
            for row in device_tracks
            if int(row.get("local_track_id") or 0)
        }
        by_path = {
            str(row.get("device_path") or ""): row
            for row in device_tracks
            if row.get("device_path")
        }

        deleted = 0
        for row in rows:
            local_id = int(row.get("id") or 0)
            device_path = str(row.get("device_path") or "")
            device_row = by_local_id.get(local_id) or by_path.get(device_path)
            if not device_row and device_path:
                device_row = {"device_path": device_path, "local_track_id": local_id}
            if not device_row or not device_row.get("device_path"):
                continue
            ok, msg = self._sync_engine.delete_device_track(device_row)
            if ok:
                deleted += 1
            else:
                logger.warning("Failed to remove video from device: %s", msg)
        return deleted

    def _delete_video_track_ids(self, track_ids):
        rows = self._video_rows_for_ids(track_ids)
        if not rows:
            self._video_sync_panel.set_status("Select videos to delete.")
            return

        reply = QMessageBox.question(
            self,
            "Delete Local Videos",
            f"Delete {len(rows)} selected local video file{'s' if len(rows) != 1 else ''}?\n\n"
            "This removes the files from your computer and RockPod library. "
            "If an iPod is connected, synced copies are removed from it too.",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return

        if self._device_detector.is_connected:
            self._remove_video_rows_from_device(rows)

        deleted = 0
        failed = []
        for row in rows:
            file_path = os.path.expanduser(str(row.get("file_path") or ""))
            if file_path and os.path.exists(file_path):
                try:
                    os.remove(file_path)
                except OSError as exc:
                    failed.append(f"{os.path.basename(file_path) or file_path}: {exc}")
                else:
                    for suffix in (".jpg", ".jpeg", ".png"):
                        sidecar = os.path.splitext(file_path)[0] + suffix
                        if os.path.exists(sidecar):
                            try:
                                os.remove(sidecar)
                            except OSError:
                                logger.warning("Failed to remove video sidecar %s", sidecar)
        with self._db.transaction():
            for row in rows:
                track_id = int(row.get("id") or 0)
                if track_id:
                    self._db.delete_track(track_id)
                    deleted += 1

        self._playback.remove_tracks_from_queue({int(row.get("id") or 0) for row in rows})
        self._refresh_video_sync_panel()
        self._refresh_view()
        if failed:
            QMessageBox.warning(
                self,
                "Delete Incomplete",
                f"Deleted {deleted} video{'s' if deleted != 1 else ''}.\n\n"
                + "\n".join(failed[:6]),
            )
        self._status_bar.set_left_text(f"Deleted {deleted} local video{'s' if deleted != 1 else ''}")

    def _show_video_sync_plan(self, track_ids, force_full=False):
        track_ids = set(track_ids or [])
        if not track_ids:
            self._video_sync_panel.set_status("Select one or more videos to sync.")
            return
        if not self._device_detector.is_connected:
            self._video_sync_panel.set_status("No Rockbox device is connected.")
            QMessageBox.warning(self, "No Device", "No Rockbox device is connected.")
            return

        plan = self._build_sync_plan_with_feedback(
            track_ids=track_ids, force_full=force_full, media_type="video"
        )
        if plan is None:
            return
        if self._sync_plan_has_blocking_errors(plan):
            self._video_sync_panel.set_status("Sync blocked: review the warning details.")
            return
        if plan.total_operations == 0:
            if force_full:
                self._video_sync_panel.set_status("Selected videos are already synced. Force repair will still re-sync.")
            else:
                self._video_sync_panel.set_status("Selected videos are already synced.")
            self._status_bar.set_left_text("Selected videos are already synced")
            return

        self._video_sync_panel.set_status(plan.summary())
        self._status_bar.set_left_text(plan.summary())
        dialog = SyncDialog(plan, self)
        dialog.sync_confirmed.connect(lambda: self._execute_sync(plan, dialog))
        dialog.sync_cancelled.connect(lambda: self._sync_engine.cancel_sync())
        dialog.exec()

    def _sync_plan_has_blocking_errors(self, plan):
        errors = [str(error) for error in getattr(plan, "errors", []) if str(error).strip()]
        if not errors:
            return False
        self._status_bar.set_left_text("Sync blocked")
        QMessageBox.warning(
            self,
            "Sync Blocked",
            "RockPod blocked this sync because the plan has warnings that could affect files on the iPod.\n\n"
            + "\n".join(errors[:8]),
        )
        return True

    def _build_sync_plan_with_feedback(
        self, track_ids=None, force_full=False, weather_only=False, media_type="audio"
    ):
        if weather_only:
            self._status_bar.set_left_text("Planning weather sync...")
            progress_text = "Checking weather data..."
            window_title = "Planning Weather Sync"
            label_text = "Fetching weather forecast..."
        else:
            self._status_bar.set_left_text("Planning sync...")
            progress_text = "Comparing your library with the iPod..."
            window_title = "Planning Sync"
            label_text = "Loading library tracks..."

        progress = QProgressDialog(progress_text, None, 0, 0, self)
        progress.setWindowTitle(window_title)
        progress.setWindowModality(Qt.WindowModal)
        progress.setMinimumDuration(0)
        progress.setAutoClose(False)
        progress.setAutoReset(False)
        progress.setCancelButton(None)
        progress.setValue(0)
        progress.setLabelText(label_text)

        planner = SyncPlanBuilder(self)
        loop = QEventLoop(self)
        result = {"plan": None, "error": ""}

        planner.status.connect(progress.setLabelText)
        planner.finished.connect(lambda plan: result.update({"plan": plan}))
        planner.error.connect(lambda message: result.update({"error": message}))
        planner.finished.connect(loop.quit)
        planner.error.connect(loop.quit)

        started = planner.start(
            self._config.db_path,
            self._config,
            self._device_detector.current_device,
            self._config.artwork_cache_dir,
            track_ids=track_ids,
            force_full=force_full,
            weather_only=weather_only,
            media_type=media_type,
        )
        if not started:
            progress.close()
            QMessageBox.warning(self, "Planning Busy", "Sync planning is already in progress.")
            return None

        progress.show()
        loop.exec()
        progress.close()

        if result["error"]:
            self._status_bar.set_left_text("Sync planning failed")
            QMessageBox.warning(self, "Sync Planning Failed", result["error"])
            return None
        return result["plan"]

    def _execute_sync(self, plan, dialog):
        self._sync_dialog = dialog
        self._active_sync_plan = plan
        self._device_state = "Syncing"
        self._toolbar.set_syncing(True)
        self._update_sync_status()
        self._update_device_summary()
        self._sync_engine.execute_sync(plan)

    def _on_sync_progress(self, current, total, desc):
        if self._sync_dialog:
            self._sync_dialog.update_progress(current, total, desc)
        self._status_bar.set_sync_progress(current, total, desc)

    def _on_sync_done(self, copied, failed, skipped):
        if self._sync_dialog:
            self._sync_dialog.show_results(copied, failed, skipped)
        self._sync_dialog = None
        self._toolbar.set_syncing(False)
        if failed == 0 and self._active_sync_plan:
            has_media_changes = self._sync_plan_has_media_changes(self._active_sync_plan)
            post_sync_db_update_seconds = 0.0
            post_sync_cache_cleanup_seconds = 0.0
            removed_count = 0

            if has_media_changes:
                t_cache = time.perf_counter()
                self._update_device_cache_from_plan(self._active_sync_plan)
                post_sync_db_update_seconds = time.perf_counter() - t_cache

                t_cache_cleanup = time.perf_counter()
                cache_cleanup = self._sync_engine.cleanup_local_sync_cache(
                    device_key=self._sync_engine.current_device_key,
                ) if self._sync_engine.current_device_key else {}
                post_sync_cache_cleanup_seconds = time.perf_counter() - t_cache_cleanup
                removed_count = len(cache_cleanup.get("removed", []) if isinstance(cache_cleanup, dict) else [])
                if isinstance(cache_cleanup, dict) and cache_cleanup.get("errors"):
                    logger.warning("Cache cleanup reported errors: %s", ", ".join(cache_cleanup["errors"]))

            self._post_sync_rockbox_integration()
            profile = dict(getattr(self._active_sync_plan, "execution_profile", {}) or {})
            profile["post_sync_db_update_seconds"] = post_sync_db_update_seconds
            profile["post_sync_cache_cleanup_seconds"] = post_sync_cache_cleanup_seconds
            self._active_sync_plan.execution_profile = profile
            logger.info(
                "Sync finalize timing: post_sync_db_update=%.3fs post_sync_cache_cleanup=%.3fs removed=%d total=%.3fs",
                post_sync_db_update_seconds,
                post_sync_cache_cleanup_seconds,
                removed_count,
                float(profile.get("total_seconds", 0.0) or 0.0),
            )
        else:
            self._device_state = "Connected" if self._device_detector.is_connected else "Disconnected"
        self._active_sync_plan = None
        self._last_sync_time = datetime.now()
        if self._device_config_value("verify_device_in_background", False):
            self._scan_device()
        self._update_status_bar()
        self._refresh_view()
        self._update_sync_status()
        self._update_device_summary()
        logger.info("Sync done: %d copied, %d failed, %d skipped", copied, failed, skipped)

    def _post_sync_rockbox_integration(self):
        if not self._active_sync_plan:
            self._status_bar.set_left_text("Sync complete")
            self._device_state = "Ready"
            self._rockbox_db_stale = False
            self._rockbox_db_update_started_at = None
            self._rockbox_db_feedback_tick = 0
            return

        device = self._device_detector.current_device
        if not device or not getattr(device, "is_rockbox", False):
            return
        playlist_result = None
        plan = self._active_sync_plan
        has_media_changes = self._sync_plan_has_media_changes(plan)
        if has_media_changes and self._device_config_value("sync_playlists_to_device", True):
            playlist_result = self._sync_rockbox_playlists(device)
        if not has_media_changes:
            self._device_state = "Ready"
            self._rockbox_db_stale = False
            self._rockbox_db_update_started_at = None
            self._rockbox_db_feedback_tick = 0
            if playlist_result and playlist_result["success"] and playlist_result["exported"]:
                self._status_bar.set_left_text(
                    f"Sync complete; {len(playlist_result['exported'])} playlist(s) updated"
                )
            elif getattr(plan, "generated_to_copy", None):
                self._status_bar.set_left_text("Weather synced")
            else:
                self._status_bar.set_left_text("Sync complete")
            return

        host_result = self._generate_rockbox_database_host_side(device)
        if host_result.get("success"):
            pictureflow_result = invalidate_pictureflow_cache(device)
            if not pictureflow_result.get("success"):
                logger.warning(
                    "PictureFlow cache invalidation failed: %s",
                    pictureflow_result.get("failures", []),
                )
            elif pictureflow_result.get("removed"):
                logger.info(
                    "Invalidated %d PictureFlow cache file(s)",
                    len(pictureflow_result.get("removed", [])),
                )
            self._rockbox_db_stale = False
            self._runtime_refresh_pending = False
            self._device_state = "Ready"
            self._rockbox_db_update_started_at = None
            self._rockbox_db_feedback_tick = 0
            track_count = host_result.get("track_count", 0)
            if playlist_result and playlist_result["success"] and playlist_result["exported"]:
                self._status_bar.set_left_text(
                    f"Sync complete; database ready with {track_count} tracks; "
                    f"{len(playlist_result['exported'])} playlist(s) updated"
                )
            else:
                self._status_bar.set_left_text(
                    f"Sync complete; Rockbox database ready with {track_count} tracks"
                )
            self._update_sync_status()
            self._refresh_device_storage_breakdown(device)
            self._update_device_summary()
            return

        self._rockbox_db_stale = True
        self._runtime_refresh_pending = True
        self._device_state = "Sync Complete (DB stale)"
        mode = self._device_config_value("rockbox_database_update_mode", "active")
        if mode in ("assisted", "active") or self._device_config_value("auto_rebuild_rockbox_database_after_sync", False):
            try:
                enable_rockbox_tagcache_autoupdate(device)
                if mode == "active":
                    self._device_state = "Updating Database"
                    self._rockbox_db_update_started_at = time.monotonic()
                    self._rockbox_db_feedback_tick = 0
                    self._status_bar.set_left_text("Rockbox database rebuilding on device...")
                else:
                    self._rockbox_db_update_started_at = None
                    self._rockbox_db_feedback_tick = 0
                    self._status_bar.set_left_text("Rockbox will update its database automatically")
            except OSError as exc:
                logger.warning("Could not enable Rockbox database auto-update: %s", exc)
                self._rockbox_db_update_started_at = None
                self._rockbox_db_feedback_tick = 0
                self._status_bar.set_left_text("Rockbox database needs refresh on device")
        else:
            self._rockbox_db_update_started_at = None
            self._rockbox_db_feedback_tick = 0
            self._status_bar.set_left_text("Rockbox database needs refresh on device")
        self._update_sync_status()
        self._refresh_device_storage_breakdown(device)
        self._update_device_summary()

    def _sync_plan_has_media_changes(self, plan):
        return bool(
            plan
            and (
                getattr(plan, "to_copy", None)
                or getattr(plan, "to_resync", None)
                or getattr(plan, "to_delete", None)
                or getattr(plan, "artwork_to_copy", None)
                or getattr(plan, "preflight_linked", None)
            )
        )

    def _generate_rockbox_database_host_side(self, device):
        try:
            result = write_rockbox_tagcache_from_device_inventory(
                self._db,
                device,
                self._sync_engine.current_device_key,
            )
            logger.info(
                "Generated Rockbox database on host: %d tracks",
                result.get("track_count", 0),
            )
            return result
        except (OSError, TagcacheError) as exc:
            logger.warning("Host Rockbox database generation failed: %s", exc)
        except Exception:
            logger.exception("Host Rockbox database generation failed")
        return {"success": False, "track_count": 0, "files": []}

    def _sync_rockbox_playlists(self, device):
        try:
            result = export_device_playlists(
                self._db,
                device,
                duplicate_strictness=self._device_config_value("duplicate_strictness", "metadata_and_hash"),
                duration_tolerance=self._device_config_value("duration_match_tolerance_seconds", 2.0),
            )
            self._db.commit()
            self._refresh_playlists()
            if result["failures"]:
                logger.warning(
                    "Rockbox playlist export completed with %d failure(s)",
                    len(result["failures"]),
                )
            elif result["exported"] or result["removed"]:
                logger.info(
                    "Rockbox playlists synced: %d exported, %d removed",
                    len(result["exported"]),
                    len(result["removed"]),
                )
            return result
        except Exception:
            logger.exception("Rockbox playlist export failed")
            return {"success": False, "exported": [], "removed": [], "failures": ["playlist export failed"]}

    def _poll_rockbox_database_state(self):
        if not self._device_detector.is_connected:
            return
        if self._device_state not in ("Connected", "Ready", "Sync Complete (DB stale)", "Updating Database"):
            return
        device_key = self._sync_engine.current_device_key
        device_row = self._db.get_device_by_key(device_key) if device_key else None
        rockbox_state = detect_rockbox_database_state(self._device_detector.current_device, device_row)
        if self._rockbox_db_stale:
            if rockbox_state.get("database_needs_refresh") or not rockbox_state.get("database_present"):
                if rockbox_state.get("tagcache_autoupdate"):
                    if self._rockbox_db_update_started_at is None:
                        self._rockbox_db_update_started_at = time.monotonic()
                    self._rockbox_db_feedback_tick += 1
                    self._device_state = "Updating Database"
            else:
                self._rockbox_db_stale = False
                self._device_state = "Ready"
                self._rockbox_db_update_started_at = None
                self._rockbox_db_feedback_tick = 0
                self._status_bar.set_left_text("Rockbox database ready")
                if self._runtime_refresh_pending:
                    imported = import_runtime_data_for_device(self._db, self._device_detector.current_device)
                    self._db.commit()
                    self._runtime_refresh_pending = False
                    if imported:
                        self._refresh_playlists()
                        self._refresh_view()
        elif self._device_state == "Connected" and rockbox_state.get("database_present") and not rockbox_state.get("database_needs_refresh"):
            self._rockbox_db_update_started_at = None
            self._rockbox_db_feedback_tick = 0
            self._device_state = "Ready"
        self._update_sync_status()
        if self._current_view == "device_root":
            self._update_device_summary()

    def _update_device_cache_from_plan(self, plan):
        self._sync_engine.apply_successful_sync_to_cache(plan, datetime.now().isoformat(timespec="seconds"))

    def _upsert_synced_device_row(self, device_key, row, rel_path):
        self._db.upsert_device_track(
            {
                "device_id": device_key,
                "device_path": rel_path,
                "local_track_id": row.get("id"),
                "title": row.get("title", ""),
                "artist": row.get("artist", ""),
                "album": row.get("album", ""),
                "album_artist": row.get("album_artist", ""),
                "genre": row.get("genre", ""),
                "year": row.get("year"),
                "track_number": row.get("track_number"),
                "disc_number": row.get("disc_number", 1),
                "duration": row.get("duration", 0.0),
                "bitrate": row.get("bitrate", 0),
                "codec": row.get("codec", ""),
                "file_size": row.get("file_size", 0),
                "metadata_hash": row.get("metadata_hash", ""),
                "file_hash": row.get("file_hash", ""),
                "present_on_device": 1,
                "last_synced_at": datetime.now().isoformat(timespec="seconds"),
            }
        )

    def _on_sync_cancelled(self):
        if self._sync_dialog:
            self._sync_dialog.show_cancelled()
        self._sync_dialog = None
        self._active_sync_plan = None
        self._toolbar.set_syncing(False)

    def _on_sync_error(self, msg):
        self._sync_dialog = None
        self._active_sync_plan = None
        self._toolbar.set_syncing(False)
        self._status_bar.set_left_text(f"Sync error: {msg}")
        logger.error("Sync error: %s", msg)

    def _delete_selected_from_device(self):
        tracks = self._active_track_table().get_selected_tracks()
        if not tracks:
            return

        count = len(tracks)
        reply = QMessageBox.question(
            self, "Delete from Device",
            f"Delete {count} track(s) from the iPod?\n\n"
            "This will only remove them from the device, not from your library.",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return

        deleted = 0
        for track in tracks:
            ok, msg = self._sync_engine.delete_device_track(track)
            if ok:
                deleted += 1
            else:
                logger.warning("Failed to delete device track: %s", msg)

        self._scan_device()
        self._refresh_view()
        self._status_bar.set_left_text(f"Deleted {deleted} track(s) from device")

    def _eject_device(self):
        if not self._device_detector.is_connected:
            return
        reply = QMessageBox.question(
            self, "Eject Device",
            "Safely eject the iPod?",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.Yes,
        )
        if reply != QMessageBox.Yes:
            return
        ok, msg = self._device_detector.eject_device()
        if ok:
            self._status_bar.set_left_text("Device ejected safely")
        else:
            QMessageBox.warning(self, "Eject Failed", msg)

    def _create_mock_device(self):
        """Create a mock Rockbox device for testing."""
        default_path = os.path.join(os.path.expanduser("~"), ".rockpod", "mock_device")
        path, ok = QInputDialog.getText(
            self, "Create Mock Device",
            "Path for mock device folder:",
            text=default_path,
        )
        if ok and path.strip():
            create_mock_device(path.strip())
            self._config.mock_device_enabled = True
            self._config.mock_device_path = path.strip()
            self._config.save()
            self._status_bar.set_left_text(f"Mock device created at {path.strip()}")

    # ═══════════════════════════════════════════════════════════════
    # Store
    # ═══════════════════════════════════════════════════════════════

    def _refresh_browser_panel(self):
        download_dir = self._config.music_dir
        self._browser_panel.set_auto_accept_cookies(self._config.get("store_auto_accept_cookies", True))
        self._browser_panel.set_store_preferred_format(self._config.get("streamrip_preferred_format", "flac"))
        self._browser_panel.set_store_context(
            self._config.get("browser_home_url", "https://listen.tidal.com/"),
            download_dir,
        )
        self._start_store_homepage()

    def _refresh_music_sharing_panel(self):
        self._music_sharing_panel.set_connection_settings(self._music_sharing.settings())
        inbox, outbox = self._music_sharing.history()
        self._music_sharing_panel.set_share_items(inbox, outbox)

    def _save_music_sharing_settings(self, settings):
        try:
            self._music_sharing.set_settings(
                relay_url=(settings or {}).get("relay_url"),
                pair_code=(settings or {}).get("pair_code"),
                display_name=(settings or {}).get("display_name"),
            )
        except MusicSharingError as exc:
            self._music_sharing_panel.set_share_status(str(exc), running=False)
            self._status_bar.set_left_text("Music sharing settings not saved")
            return
        self._refresh_music_sharing_panel()
        self._music_sharing_panel.set_share_status("Music sharing settings saved.", running=False)
        self._status_bar.set_left_text("Music sharing settings saved")

    def _refresh_music_shares_from_relay(self):
        self._music_sharing_panel.set_share_status("Refreshing shared music...", running=True)
        try:
            added = self._music_sharing.fetch()
        except MusicSharingError as exc:
            self._music_sharing_panel.set_share_status(str(exc), running=False)
            self._status_bar.set_left_text("Music sharing refresh failed")
            return
        self._refresh_music_sharing_panel()
        if added:
            label = f"Received {added} shared music item{'s' if added != 1 else ''}."
        else:
            label = "No new shared music."
        self._music_sharing_panel.set_share_status(label, running=False)
        self._status_bar.set_left_text(label)

    def _send_music_share(self, item):
        self._music_sharing_panel.set_share_status("Sending shared music...", running=True)
        try:
            self._music_sharing.send(item)
        except MusicSharingError as exc:
            self._music_sharing_panel.set_share_status(str(exc), running=False)
            self._status_bar.set_left_text("Music sharing send failed")
            return
        self._refresh_music_sharing_panel()
        self._music_sharing_panel.clear_share_composer()
        title = str((item or {}).get("title") or "shared item")
        label = f"Shared {title}."
        self._music_sharing_panel.set_share_status(label, running=False)
        self._status_bar.set_left_text(label)

    def _start_music_share_import(self, result, output_format):
        self._start_store_result_import(result, output_format, status_panel=self._music_sharing_panel)

    def _refresh_game_browser_panel(self):
        profile = self._rockbox_profiles.current_profile()
        download_dir = ""
        if profile:
            download_dir = profile.get("games_library_path", "")
        if not download_dir:
            download_dir = self._config.get("games_library_path", "")
        if not download_dir:
            download_dir = os.path.join(os.path.expanduser("~"), "Documents", "Gameboy")
        self._game_browser_panel.set_auto_accept_cookies(self._config.get("store_auto_accept_cookies", True))
        self._game_browser_panel.set_store_context(
            self._config.get("browser_home_url", "https://www.rockbox.org/"),
            download_dir,
        )

    def _refresh_movie_store_panel(self):
        if self._movie_browse_loaded:
            return
        self._movie_browse_loaded = True
        query = "public domain full movies"
        self._movie_store_panel._movie_browse_edit.setText(query)
        self._start_movie_browse(query)

    def _open_browser_external(self, url):
        if not url:
            return
        start_detached_command(["xdg-open", url])

    def _create_child_process(self, request, working_directory, output_handler, finished_handler, error_handler):
        return create_child_process(
            self,
            request,
            working_directory,
            output_handler,
            finished_handler,
            error_handler,
        )

    def _start_movie_browse(self, query):
        if self._movie_browse_process is not None:
            return
        try:
            request = self._youtube_movie_importer.prepare_browse(query, limit=18)
        except YoutubeMovieImportError as exc:
            self._movie_store_panel.set_movie_browse_status(str(exc), running=False)
            return

        process = self._create_child_process(
            request,
            self._youtube_movie_importer.state_dir,
            self._on_movie_browse_output,
            self._on_movie_browse_finished,
            self._on_movie_browse_error,
        )
        self._movie_browse_process = process
        self._movie_browse_output_path = request.output_path
        self._movie_browse_output = []
        self._movie_browse_query = str(query or "").strip()
        self._movie_store_panel.set_movie_browse_status(f"Browsing YouTube for {self._movie_browse_query}...", running=True)
        self._status_bar.set_left_text("Browsing YouTube movies")
        process.start(request.command[0], request.command[1:])

    def _on_movie_browse_output(self):
        process = self._movie_browse_process
        if process is None:
            return
        text = compact_process_text(read_process_text(process))
        if text:
            self._movie_browse_output.append(text)

    def _on_movie_browse_finished(self, exit_code, exit_status):
        process = self._movie_browse_process
        self._movie_browse_process = None
        if process is not None:
            text = compact_process_text(read_process_text(process))
            if text:
                self._movie_browse_output.append(text)

        output_path = self._movie_browse_output_path
        self._movie_browse_output_path = ""
        query = self._movie_browse_query
        self._movie_browse_query = ""
        if exit_code != 0:
            detail = " ".join(self._movie_browse_output).strip()
            self._movie_browse_output = []
            self._movie_store_panel.set_movie_browse_status(
                f"Movie browse failed with exit code {exit_code}: {detail[-500:]}",
                running=False,
            )
            self._status_bar.set_left_text("Movie browse failed")
            return

        try:
            with open(output_path, "r") as handle:
                results = json.load(handle)
        except (OSError, json.JSONDecodeError) as exc:
            self._movie_browse_output = []
            self._movie_store_panel.set_movie_browse_status(f"Movie browse result could not be read: {exc}", running=False)
            self._status_bar.set_left_text("Movie browse failed")
            return

        self._movie_browse_output = []
        count = len(results)
        header = f"Browse: {query}" if query else "Featured Movies"
        self._movie_store_panel.set_movie_results(results, header)
        label = f"Found {count} movie result{'s' if count != 1 else ''}."
        self._movie_store_panel.set_movie_browse_status(label, running=False)
        self._status_bar.set_left_text(label)

    def _on_movie_browse_error(self, error):
        self._movie_browse_process = None
        self._movie_browse_output_path = ""
        self._movie_browse_output = []
        self._movie_browse_query = ""
        self._movie_store_panel.set_movie_browse_status(f"Movie browse could not start: {error}", running=False)
        self._status_bar.set_left_text("Movie browse failed")

    def _start_movie_import(self, url):
        if self._movie_import_process is not None:
            return
        movie_result = self._movie_store_panel.movie_result_for_url(url)
        duplicate_message = self._movie_import_duplicate_message(movie_result)
        if duplicate_message:
            self._movie_store_panel.set_movie_import_status(duplicate_message, running=False)
            self._status_bar.set_left_text(duplicate_message)
            QMessageBox.warning(self, "Movie Import", duplicate_message)
            return
        try:
            request = self._youtube_movie_importer.prepare_import(url)
        except YoutubeMovieImportError as exc:
            self._movie_store_panel.set_movie_import_status(str(exc), running=False)
            QMessageBox.warning(self, "Movie Import", str(exc))
            return

        process = self._create_child_process(
            request,
            request.output_dir,
            self._on_movie_import_output,
            self._on_movie_import_finished,
            self._on_movie_import_error,
        )

        self._movie_import_process = process
        self._movie_import_output = []
        self._movie_import_log_path = request.log_path
        self._movie_import_result = dict(movie_result or {})
        self._append_movie_import_log("Process started.\n")
        self._movie_store_panel.begin_movie_import(url, request.output_dir)
        self._movie_store_panel.set_movie_import_status(
            f"Downloading and converting movie... Log: {request.log_path}",
            running=True,
        )
        self._status_bar.set_left_text("Importing YouTube movie")
        process.start(request.command[0], request.command[1:])

    def _movie_import_duplicate_message(self, movie_result):
        result = dict(movie_result or {})
        if not result.get("title"):
            return ""
        try:
            rows = self._db.get_tracks_by_media_type("video")
        except Exception:
            logger.exception("Could not check existing movie imports")
            rows = []
        duplicate = existing_movie_duplicate(rows, result, self._youtube_movie_importer.output_dir)
        if not duplicate:
            return ""
        title = str(result.get("title") or "movie")
        existing = str(duplicate.get("title") or os.path.basename(str(duplicate.get("file_path") or "")) or "existing movie")
        return f"Movie already appears to be in your library: {title} ({existing})."

    def _append_movie_import_log(self, text):
        if not self._movie_import_log_path or not text:
            return
        try:
            with open(self._movie_import_log_path, "a") as handle:
                handle.write(text)
                if not text.endswith("\n"):
                    handle.write("\n")
        except OSError:
            pass

    def _on_movie_import_output(self):
        process = self._movie_import_process
        if process is None:
            return
        raw = read_process_text(process)
        self._append_movie_import_log(raw)
        lines = process_output_lines(raw)
        if lines:
            self._movie_import_output.extend(lines)
            progress = parse_movie_import_progress(lines)
            self._movie_store_panel.update_movie_import(progress["phase"], progress["progress"])
            self._movie_store_panel.set_movie_import_status(
                f"{progress['detail']}\nLog: {self._movie_import_log_path}",
                running=True,
            )

    def _on_movie_import_finished(self, exit_code, exit_status):
        process = self._movie_import_process
        self._movie_import_process = None
        if process is not None:
            raw = read_process_text(process)
            self._append_movie_import_log(raw)
            self._movie_import_output.extend(process_output_lines(raw))
        log_path = self._movie_import_log_path
        self._append_movie_import_log(f"\nProcess finished with exit code {exit_code}.\n")

        output_path = ""
        for line in self._movie_import_output:
            if line.startswith("ROCKPOD_MOVIE_OUTPUT="):
                output_path = line.split("=", 1)[1].strip()
        self._movie_import_output = []
        if exit_code != 0 or not output_path:
            message = f"YouTube movie import failed with exit code {exit_code}.\nLog: {log_path}"
            self._movie_store_panel.finish_movie_import(success=False)
            self._movie_store_panel.set_movie_import_status(message, running=False)
            self._status_bar.set_left_text("Movie import failed")
            self._movie_import_log_path = ""
            self._movie_import_result = {}
            return

        poster_source = str(
            self._movie_import_result.get("thumbnail_path")
            or self._movie_import_result.get("thumbnail")
            or ""
        )
        poster_path = persist_movie_import_poster(output_path, poster_source)
        if poster_path:
            self._append_movie_import_log(f"Poster sidecar: {poster_path}\n")
        self._movie_import_result = {}
        self._movie_import_log_path = ""
        poster_note = " Poster saved." if poster_path else ""
        label = f"Imported movie: {os.path.basename(output_path)}.{poster_note} Refreshing library.\nLog: {log_path}"
        self._movie_store_panel.finish_movie_import(output_path, success=True)
        self._movie_store_panel.set_movie_import_status(label, running=False)
        self._status_bar.set_left_text(label)
        self._start_scan(force_full=False)

    def _on_movie_import_error(self, error):
        self._movie_import_process = None
        log_path = self._movie_import_log_path
        self._append_movie_import_log(f"Process error: {error}\n")
        self._movie_import_log_path = ""
        self._movie_import_output = []
        self._movie_import_result = {}
        self._movie_store_panel.finish_movie_import(success=False)
        self._movie_store_panel.set_movie_import_status(
            f"YouTube movie import could not start: {error}\nLog: {log_path}",
            running=False,
        )
        self._status_bar.set_left_text("Movie import failed")

    def _start_store_search(self, query, source):
        if self._store_search_process is not None:
            return
        try:
            request = self._streamrip_importer.prepare_album_search(query, source, limit=24)
        except StreamripImportError as exc:
            self._browser_panel.set_store_search_status(str(exc), running=False)
            return

        process = self._create_child_process(
            request,
            self._streamrip_importer.streamrip_state_dir,
            self._on_store_search_output,
            self._on_store_search_finished,
            self._on_store_search_error,
        )

        self._store_search_process = process
        self._store_search_output_path = request.output_path
        self._store_search_output = []
        self._store_search_mode = "search"
        self._browser_panel.set_store_search_status(f"Searching {source.capitalize()}...", running=True)
        self._status_bar.set_left_text(f"Searching {source.capitalize()} store")
        process.start(request.command[0], request.command[1:])

    def _on_store_home_tab_requested(self, tab_key):
        self._start_store_homepage(force=True, tab_key=tab_key)

    def _start_store_homepage(self, force=False, tab_key=None):
        tab = str(tab_key or getattr(self._browser_panel, "_store_home_tab", "featured") or "featured").strip().lower()
        self._browser_panel.set_store_home_tab(tab, emit=False)
        if tab in self._store_home_loaded_tabs and not force:
            return
        if self._store_search_process is not None:
            return
        try:
            request = self._streamrip_importer.prepare_store_homepage(limit=24, home_tab=tab)
        except StreamripImportError as exc:
            self._browser_panel.set_store_search_status(str(exc), running=False)
            return

        process = self._create_child_process(
            request,
            self._streamrip_importer.streamrip_state_dir,
            self._on_store_search_output,
            self._on_store_search_finished,
            self._on_store_search_error,
        )

        self._store_search_process = process
        self._store_search_output_path = request.output_path
        self._store_search_output = []
        self._store_search_mode = "homepage"
        self._store_search_home_tab = tab
        label = self._store_home_tab_label(tab)
        self._browser_panel.set_store_search_status(f"Loading {label} from TIDAL...", running=True)
        self._status_bar.set_left_text(f"Loading TIDAL Store {label}")
        process.start(request.command[0], request.command[1:])

    @staticmethod
    def _store_home_tab_label(tab_key):
        labels = {
            "featured": "Featured",
            "new_releases": "New Releases",
            "top_albums": "Top Albums",
            "just_added": "Just Added",
            "alternative": "Alternative",
            "rock": "Rock",
            "hip_hop": "Hip-Hop/Rap",
        }
        return labels.get(str(tab_key or "featured"), "Featured")

    def _on_store_search_output(self):
        process = self._store_search_process
        if process is None:
            return
        text = compact_process_text(read_process_text(process))
        if text:
            self._store_search_output.append(text)

    def _on_store_search_finished(self, exit_code, exit_status):
        process = self._store_search_process
        self._store_search_process = None
        if process is not None:
            text = compact_process_text(read_process_text(process))
            if text:
                self._store_search_output.append(text)

        output_path = self._store_search_output_path
        self._store_search_output_path = ""
        mode = self._store_search_mode
        self._store_search_mode = ""
        home_tab = self._store_search_home_tab
        self._store_search_home_tab = "featured"
        if exit_code != 0:
            detail = " ".join(self._store_search_output).strip()
            self._store_search_output = []
            self._browser_panel.set_store_search_status(
                f"Store search failed with exit code {exit_code}: {detail[-500:]}",
                running=False,
            )
            self._status_bar.set_left_text("Store search failed")
            return

        try:
            with open(output_path, "r") as handle:
                results = json.load(handle)
        except (OSError, json.JSONDecodeError) as exc:
            self._store_search_output = []
            self._browser_panel.set_store_search_status(f"Store search result could not be read: {exc}", running=False)
            self._status_bar.set_left_text("Store search failed")
            return

        self._store_search_output = []
        results = self._mark_store_results_owned(results)
        if mode == "homepage":
            self._store_home_loaded_tabs.add(home_tab)
            self._browser_panel.set_store_home_results(results, home_tab)
        else:
            self._browser_panel.set_store_results(results, "Search Results")
        count = len(results)
        if mode == "homepage":
            tab_label = self._store_home_tab_label(home_tab)
            label = f"Loaded {count} {tab_label} TIDAL album{'s' if count != 1 else ''}."
        else:
            label = f"Found {count} album{'s' if count != 1 else ''}."
        self._browser_panel.set_store_search_status(label, running=False)
        self._status_bar.set_left_text(label)

    def _on_store_search_error(self, error):
        self._store_search_process = None
        self._store_search_output_path = ""
        self._store_search_output = []
        self._store_search_mode = ""
        self._store_search_home_tab = "featured"
        self._browser_panel.set_store_search_status(f"Store search could not start: {error}", running=False)
        self._status_bar.set_left_text("Store search failed")

    def _start_store_album_detail(self, result):
        if self._store_detail_process is not None:
            return
        item = dict(result or {})
        source = str(item.get("source") or "tidal").strip().lower()
        album_id = str(item.get("id") or "").strip()
        try:
            request = self._streamrip_importer.prepare_album_detail(source, album_id)
        except StreamripImportError as exc:
            self._browser_panel.set_store_search_status(str(exc), running=False)
            return

        process = self._create_child_process(
            request,
            self._streamrip_importer.streamrip_state_dir,
            self._on_store_album_detail_output,
            self._on_store_album_detail_finished,
            self._on_store_album_detail_error,
        )

        self._store_detail_process = process
        self._store_detail_output_path = request.output_path
        self._store_detail_output = []
        self._browser_panel.set_store_search_status("Loading track list...", running=False)
        self._status_bar.set_left_text("Loading store album details")
        process.start(request.command[0], request.command[1:])

    def _on_store_album_detail_output(self):
        process = self._store_detail_process
        if process is None:
            return
        text = compact_process_text(read_process_text(process))
        if text:
            self._store_detail_output.append(text)

    def _on_store_album_detail_finished(self, exit_code, exit_status):
        process = self._store_detail_process
        self._store_detail_process = None
        if process is not None:
            text = compact_process_text(read_process_text(process))
            if text:
                self._store_detail_output.append(text)

        output_path = self._store_detail_output_path
        self._store_detail_output_path = ""
        if exit_code != 0:
            detail = " ".join(self._store_detail_output).strip()
            self._store_detail_output = []
            self._browser_panel.set_store_search_status(
                f"Album details failed with exit code {exit_code}: {detail[-500:]}",
                running=False,
            )
            self._status_bar.set_left_text("Album details failed")
            return

        try:
            with open(output_path, "r") as handle:
                result = json.load(handle)
        except (OSError, json.JSONDecodeError) as exc:
            self._store_detail_output = []
            self._browser_panel.set_store_search_status(f"Album details could not be read: {exc}", running=False)
            self._status_bar.set_left_text("Album details failed")
            return

        self._store_detail_output = []
        result = self._mark_store_results_owned([result])[0]
        self._browser_panel.update_store_result_details(result)
        count = len(result.get("track_items") or [])
        self._browser_panel.set_store_search_status(
            f"Loaded {count} track{'s' if count != 1 else ''}.",
            running=False,
        )
        self._status_bar.set_left_text("Loaded store album details")

    def _on_store_album_detail_error(self, error):
        self._store_detail_process = None
        self._store_detail_output_path = ""
        self._store_detail_output = []
        self._browser_panel.set_store_search_status(f"Album details could not start: {error}", running=False)
        self._status_bar.set_left_text("Album details failed")

    def _mark_store_results_owned(self, results):
        marked = []
        for result in results or []:
            item = dict(result or {})
            url_info = streamrip_url_info(item.get("url"))
            if self._find_store_result_in_library(item, url_info):
                item["owned"] = True
                item["in_library"] = True
            track_items = []
            for track in item.get("track_items") or []:
                track_item = dict(track or {})
                track_url_info = streamrip_url_info(track_item.get("url"))
                if self._find_store_result_in_library(track_item, track_url_info):
                    track_item["owned"] = True
                    track_item["in_library"] = True
                track_items.append(track_item)
            if track_items:
                item["track_items"] = track_items
            marked.append(item)
        return marked

    def _active_store_import_panel(self):
        return self._store_import_panel or self._browser_panel

    def _start_store_result_import(self, result, output_format, status_panel=None):
        result = dict(result or {})
        url = str(result.get("url") or "").strip()
        if self._store_import_process is not None:
            panel = status_panel or self._active_store_import_panel()
            panel.set_store_import_status("Another music import is already running.", running=True)
            return
        self._store_import_panel = status_panel or self._browser_panel
        if not url:
            self._active_store_import_panel().set_store_import_status("This store item does not have an import URL.", running=False)
            self._status_bar.set_left_text("Store item cannot be imported")
            self._store_import_panel = None
            return
        self._start_store_import(url, output_format, result)

    def _start_store_import(self, url, output_format, store_result=None):
        if self._store_import_process is not None:
            self._active_store_import_panel().set_store_import_status("Another music import is already running.", running=True)
            return
        if self._store_import_panel is None:
            self._store_import_panel = self._browser_panel
        panel = self._active_store_import_panel()
        self._store_import_context = None
        duplicate_message = self._store_import_duplicate_message(url, store_result)
        if duplicate_message:
            panel.set_store_import_status(duplicate_message, running=False)
            self._status_bar.set_left_text(duplicate_message)
            self._store_import_panel = None
            return
        try:
            request = self._streamrip_importer.prepare_import(url, output_format)
        except StreamripImportError as exc:
            panel.set_store_import_status(str(exc), running=False)
            QMessageBox.warning(self, "Store Import", str(exc))
            self._store_import_panel = None
            return

        process = self._create_child_process(
            request,
            request.output_dir,
            self._on_store_import_output,
            self._on_store_import_finished,
            self._on_store_import_error,
        )

        self._store_import_process = process
        self._store_import_started_at = request.started_at
        self._store_import_output_dir = request.output_dir
        self._store_import_output = []
        self._store_import_log_path = request.log_path
        self._store_import_context = {
            "url": url,
            "url_info": streamrip_url_info(url),
            "store_result": dict(store_result or {}),
        }
        self._append_store_import_log("Process started.\n")
        panel.begin_store_import_download(url, request.output_dir)
        panel.set_store_import_status(
            f"Importing with streamrip... Log: {request.log_path}",
            running=True,
        )
        self._status_bar.set_left_text("Importing music from store")
        self._store_import_poll_timer.start()
        process.start(request.command[0], request.command[1:])

    @staticmethod
    def _store_match_text(value):
        return " ".join(str(value or "").casefold().split())

    def _store_import_duplicate_message(self, url, store_result=None):
        info = streamrip_url_info(url)
        existing_file = ""
        if info.get("source") and info.get("media_type") and info.get("id"):
            existing_file = find_existing_streamrip_source_file(
                self._config.music_dir,
                info["source"],
                info["media_type"],
                info["id"],
            )
        if existing_file:
            label = self._store_result_label(store_result, info)
            filename = os.path.basename(existing_file)
            return f"Already in library: {label}. Skipping download. Matched {filename}."

        matched = self._find_store_result_in_library(store_result, info)
        if matched:
            label = self._store_result_label(store_result, info, matched)
            return f"Already in library: {label}. Skipping download."
        return ""

    def _store_result_label(self, store_result=None, url_info=None, matched_track=None):
        result = dict(store_result or {})
        matched = dict(matched_track or {})
        title = result.get("title") or matched.get("album") or matched.get("title")
        artist = result.get("artist") or matched.get("album_artist") or matched.get("artist")
        media_type = result.get("media_type") or (url_info or {}).get("media_type") or "item"
        if title and artist:
            return f"{title} by {artist}"
        if title:
            return str(title)
        item_id = (url_info or {}).get("id")
        source = (url_info or {}).get("source")
        if source and item_id:
            return f"{source.capitalize()} {media_type} {item_id}"
        return "store item"

    def _find_store_result_in_library(self, store_result=None, url_info=None):
        result = dict(store_result or {})
        media_type = str(result.get("media_type") or (url_info or {}).get("media_type") or "").lower()
        title = self._store_match_text(result.get("title"))
        artist = self._store_match_text(result.get("artist"))
        if not title:
            return None

        try:
            tracks = [dict(row) for row in self._db.get_all_tracks(media_type="audio")]
        except Exception:
            logger.exception("Could not check library for existing store import")
            return None

        if media_type == "track":
            for row in tracks:
                if self._store_match_text(row.get("title")) != title:
                    continue
                row_artist = self._store_match_text(row.get("artist") or row.get("album_artist"))
                if not artist or row_artist == artist:
                    return row
            return None

        for row in tracks:
            if self._store_match_text(row.get("album")) != title:
                continue
            row_artists = {
                self._store_match_text(row.get("album_artist")),
                self._store_match_text(row.get("artist")),
            }
            if not artist or artist in row_artists:
                return row
        return None

    def _append_store_import_log(self, text):
        if not self._store_import_log_path or not text:
            return
        try:
            with open(self._store_import_log_path, "a") as handle:
                handle.write(text)
                if not text.endswith("\n"):
                    handle.write("\n")
        except OSError:
            pass

    def _on_store_import_output(self):
        process = self._store_import_process
        if process is None:
            return
        raw = read_process_text(process)
        self._append_store_import_log(raw)
        text = compact_process_text(raw)
        if text:
            self._store_import_output.append(text)
            suffix = f"\nLog: {self._store_import_log_path}" if self._store_import_log_path else ""
            self._active_store_import_panel().set_store_import_status(f"{text[-500:]}{suffix}", running=True)
        self._poll_store_import_downloads()

    def _poll_store_import_downloads(self):
        if not self._store_import_output_dir or not self._store_import_started_at:
            return
        imported = discover_imported_audio_files(self._store_import_output_dir, self._store_import_started_at)
        if imported:
            self._active_store_import_panel().update_store_import_downloads(
                imported,
                running=self._store_import_process is not None,
            )

    def _on_store_import_finished(self, exit_code, exit_status):
        self._store_import_poll_timer.stop()
        process = self._store_import_process
        self._store_import_process = None
        if process is not None:
            raw = read_process_text(process)
            self._append_store_import_log(raw)
            text = compact_process_text(raw)
            if text:
                self._store_import_output.append(text)
        imported = discover_imported_audio_files(self._store_import_output_dir, self._store_import_started_at)
        cover_updates = ensure_rockbox_cover_files(imported, self._store_import_output_dir)
        import_context = self._store_import_context
        self._store_import_context = None
        self._append_store_import_log(
            f"\nProcess finished with exit code {exit_code}.\n"
            f"Detected imported files: {len(imported)}\n"
            f"Rockbox cover files added: {len(cover_updates)}\n"
        )
        log_path = self._store_import_log_path
        panel = self._active_store_import_panel()
        panel.finish_store_import_downloads(imported, success=(exit_code == 0))
        self._store_import_started_at = 0.0
        self._store_import_output_dir = ""
        self._store_import_log_path = ""
        if exit_code != 0:
            message = store_import_failure_message(exit_code, self._store_import_output, log_path)
            self._store_import_output = []
            panel.set_store_import_status(message, running=False)
            self._store_import_panel = None
            return
        count = len(imported)
        if count == 0:
            label = store_import_empty_message(self._store_import_output, log_path)
            self._store_import_output = []
            panel.set_store_import_status(label, running=False)
            self._status_bar.set_left_text("No new store imports detected")
            self._store_import_panel = None
            return
        if self._is_spotify_playlist_import(import_context):
            self._pending_store_playlist_import = {
                "context": import_context,
                "imported_files": list(imported),
            }
        self._store_import_output = []
        label = store_import_success_message(count, len(cover_updates), log_path)
        panel.set_store_import_status(label, running=False)
        self._status_bar.set_left_text(label)
        self._store_import_panel = None
        self._start_scan(force_full=False)

    def _on_store_import_error(self, error):
        self._store_import_poll_timer.stop()
        imported = discover_imported_audio_files(self._store_import_output_dir, self._store_import_started_at)
        self._store_import_process = None
        self._store_import_output = []
        self._store_import_context = None
        log_path = self._store_import_log_path
        self._append_store_import_log(f"Process error: {error}\n")
        panel = self._active_store_import_panel()
        panel.finish_store_import_downloads(imported, success=False)
        self._store_import_started_at = 0.0
        self._store_import_output_dir = ""
        self._store_import_log_path = ""
        panel.set_store_import_status(
            f"streamrip could not start: {error}\nLog: {log_path}",
            running=False,
        )
        self._store_import_panel = None

    @staticmethod
    def _is_spotify_playlist_import(context):
        if not context:
            return False
        info = dict(context.get("url_info") or streamrip_url_info(context.get("url")))
        return info.get("source") == "spotify" and info.get("media_type") == "playlist" and bool(info.get("id"))

    def _finalize_pending_store_playlist_import(self):
        pending = self._pending_store_playlist_import
        if not pending:
            return
        self._pending_store_playlist_import = None

        context = dict(pending.get("context") or {})
        imported_files = list(pending.get("imported_files") or [])
        track_ids = []
        seen = set()
        for path in imported_files:
            row = self._db.get_track_by_path(path)
            if not row or row["media_type"] != "audio":
                continue
            track_id = row["id"]
            if track_id in seen:
                continue
            seen.add(track_id)
            track_ids.append(track_id)

        if not track_ids:
            self._status_bar.set_left_text("Spotify playlist imported, but no scanned audio tracks matched it")
            return

        name = self._unique_playlist_name(self._store_import_playlist_name(context, imported_files))
        playlist_id = self._db.create_playlist(name)
        added = self._db.add_tracks_to_playlist(playlist_id, track_ids)
        self._db.commit()
        self._refresh_playlists()
        self._export_shared_music_playlists(silent=True)
        self._status_bar.set_left_text(
            f"Created playlist {name} with {added} track{'s' if added != 1 else ''}; it will sync as a playlist"
        )

    def _store_import_playlist_name(self, context, imported_files):
        result = dict((context or {}).get("store_result") or {})
        for key in ("title", "name"):
            value = str(result.get(key) or "").strip()
            if value:
                return value

        folders = [os.path.dirname(path) for path in imported_files if path]
        if folders:
            try:
                common = os.path.commonpath(folders)
            except ValueError:
                common = folders[0]
            music_root = os.path.abspath(os.path.expanduser(str(self._config.music_dir or "")))
            if os.path.abspath(common) != music_root:
                basename = os.path.basename(common.rstrip(os.sep)).strip()
                if basename:
                    return basename

        info = dict((context or {}).get("url_info") or {})
        item_id = str(info.get("id") or "").strip()
        if item_id:
            return f"Spotify Playlist {item_id}"
        return "Spotify Playlist"

    def _unique_playlist_name(self, name):
        base = str(name or "Playlist").strip() or "Playlist"
        existing = {row["name"] for row in self._db.get_all_playlists()}
        if base not in existing:
            return base
        index = 2
        while f"{base} {index}" in existing:
            index += 1
        return f"{base} {index}"

    # ═══════════════════════════════════════════════════════════════
    # Preferences
    # ═══════════════════════════════════════════════════════════════

    def _show_preferences(self):
        dialog = PreferencesDialog(self._config, self)
        dialog.settings_saved.connect(self._on_settings_saved)
        dialog.exec()

    def _on_settings_saved(self, changes):
        theme_change_requested = "theme_mode" in changes
        for key, value in changes.items():
            self._config.set(key, value)
        self._config.save()
        logger.info("Settings updated: %s", list(changes.keys()))

        # React to specific changes
        if "music_dir" in changes or "video_dir" in changes or "video_dirs" in changes:
            self._start_scan()
        if "show_column_browser" in changes:
            self._column_browser.setVisible(changes["show_column_browser"])
        if "device_mount_path" in changes:
            profile = self._rockbox_profiles.current_profile()
            if profile:
                profile["device_mount_path"] = changes["device_mount_path"]
                self._rockbox_profiles.save_profile(profile)
            self._refresh_theme_hub()
            self._refresh_ipone_wallpapers()
            self._refresh_boot_manager()
            self._refresh_plugin_manager()
            self._refresh_game_manager()
            self._refresh_simulator_panel()
        if "games_device_target_dir" in changes or "games_simulator_target_dir" in changes:
            profile = self._rockbox_profiles.current_profile()
            if profile:
                if "games_device_target_dir" in changes:
                    profile["games_device_target_dir"] = changes["games_device_target_dir"]
                if "games_simulator_target_dir" in changes:
                    profile["games_simulator_target_dir"] = changes["games_simulator_target_dir"]
                self._rockbox_profiles.save_profile(profile)
            self._refresh_game_manager()
        if "games_library_path" in changes:
            profile = self._rockbox_profiles.current_profile()
            if profile:
                profile["games_library_path"] = changes["games_library_path"]
                self._rockbox_profiles.save_profile(profile)
            self._refresh_game_manager()
            self._refresh_game_browser_panel()
        if theme_change_requested:
            self._theme_assets = ThemeAssetManager(self._config)
            status = self._theme_assets.theme_status()
            if status["requested"] == "personal" and status["active"] != "personal":
                QMessageBox.information(
                    self,
                    "Personal Theme Unavailable",
                    "Personal iTunes Theme was selected, but assets/theme_itunes_personal/theme.json "
                    "is missing or invalid. RockPod will use the default theme until the personal pack is ready.",
                )
            self._apply_theme_assets()
        if "browser_home_url" in changes:
            self._refresh_browser_panel()
        self._artwork.set_config(self._config)
        self._refresh_playlists()

    def _apply_theme_assets(self):
        icon_path = self._theme_assets.asset_path("branding_app_icon")
        if icon_path:
            icon = QIcon(icon_path)
            self.setWindowIcon(icon)
            app = QApplication.instance()
            if app is not None:
                app.setWindowIcon(icon)
        if hasattr(self, "_toolbar"):
            self._toolbar.apply_theme_assets(self._theme_assets)
        if hasattr(self, "_sidebar"):
            self._sidebar.apply_theme_assets(self._theme_assets)
        if hasattr(self, "_device_summary"):
            self._device_summary.apply_theme_assets(self._theme_assets)
        if hasattr(self, "_album_view"):
            self._album_view.apply_theme_assets(self._theme_assets)
        if hasattr(self, "_video_view"):
            self._video_view.apply_theme_assets(self._theme_assets)

    # ═══════════════════════════════════════════════════════════════
    # Rockbox themes
    # ═══════════════════════════════════════════════════════════════

    def _refresh_ipone_wallpapers(self):
        profiles = self._rockbox_profiles.profiles()
        selected_id = self._rockbox_profiles.selected_profile_id()
        self._ipone_wallpapers.set_profiles(profiles, selected_id)
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._ipone_wallpapers.set_themes([], "")
            self._ipone_wallpapers.set_candidates([], [], [])
            return
        themes = self._rockbox_themes.list_themes(
            profile["source_repo_path"],
            profile["screen_resolution"],
            profile.get("target_device_model", ""),
            profile.get("device_mount_path", ""),
        )
        selected_theme = profile.get("selected_theme") or (themes[0]["id"] if themes else "")
        if themes and not any(item["id"] == selected_theme for item in themes):
            selected_theme = themes[0]["id"]
            profile["selected_theme"] = selected_theme
            self._rockbox_profiles.save_profile(profile)
        self._ipone_wallpapers.set_themes(themes, selected_theme)
        wallpaper_profile = dict(profile)
        if selected_theme:
            wallpaper_profile["selected_theme"] = selected_theme
        candidates = self._ipone_wallpapers_service.list_candidates(
            wallpaper_profile["source_repo_path"],
            wallpaper_profile,
            include_hidden=self._show_hidden_wallpapers,
        )
        self._ipone_wallpapers.set_candidates(
            candidates.get("lock", []),
            candidates.get("charge", []),
            candidates.get("pictureflow", []),
        )
        self._ipone_wallpapers.set_lockscreen_customization(profile.get("lockscreen_customization", {}))

    def _on_ipone_wallpaper_profile_selected(self, profile_id):
        self._rockbox_profiles.set_selected_profile(profile_id)
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_theme_designer()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_simulator_panel()

    def _on_ipone_wallpaper_theme_selected(self, theme_id):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        profile["selected_theme"] = theme_id
        self._rockbox_profiles.save_profile(profile)
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_theme_designer()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_simulator_panel()

    def _apply_ipone_wallpapers(self, selection):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._status_bar.set_left_text("No Rockbox profile selected")
            return
        lock_source = str(selection.get("lock_source") or "").strip()
        charge_source = str(selection.get("charge_source") or "").strip()
        pictureflow_source = str(selection.get("pictureflow_source") or "").strip()
        lockscreen_customization = selection.get("lockscreen_customization") or {}
        theme_id = str(selection.get("theme_id") or "").strip()
        if theme_id and theme_id != profile.get("selected_theme"):
            profile["selected_theme"] = theme_id
            self._rockbox_profiles.save_profile(profile)
        if lockscreen_customization:
            profile["lockscreen_customization"] = lockscreen_customization
            self._rockbox_profiles.save_profile(profile)
        try:
            bundle = self._ipone_wallpapers_service.build_apply_bundle(
                profile,
                lock_source=lock_source,
                charge_source=charge_source,
                pictureflow_source=pictureflow_source,
                lockscreen_customization=lockscreen_customization,
            )
            diff = self._rockbox_deploy.build_diff(profile, bundle)
        except ValueError as exc:
            QMessageBox.warning(self, "Apply Wallpapers", str(exc))
            return

        summary = diff["summary"]
        lines = [
            f"Apply wallpapers to {profile['name']} ({profile.get('selected_theme', 'theme')})?",
            "",
            f"Add {summary['add']} files",
            f"Overwrite {summary['overwrite']} files",
            f"Unchanged {summary['unchanged']} files",
        ]
        if lock_source:
            lines.extend(["", f"Lockscreen: {os.path.basename(lock_source)}"])
        if charge_source:
            lines.extend(["", f"Charge: {os.path.basename(charge_source)}"])
        if pictureflow_source:
            lines.extend(["", f"PictureFlow Init: {os.path.basename(pictureflow_source)}"])
        if lockscreen_customization:
            clock = lockscreen_customization.get("clock", {})
            lines.extend(["", f"Lock Screen Clock: {clock.get('position', 'center')} / {clock.get('style', 'solid')}"])
        prompt = QMessageBox(self)
        prompt.setWindowTitle("Apply Wallpapers")
        prompt.setText("\n".join(lines))
        detail_lines = []
        for item in diff["items"]:
            detail_lines.append(f"{item['status'].upper()}: {item['destination_rel']}")
        if detail_lines:
            prompt.setDetailedText("\n".join(detail_lines))
        prompt.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        prompt.setDefaultButton(QMessageBox.Yes)
        prompt.exec()
        if prompt.clickedButton() != prompt.button(QMessageBox.Yes):
            return

        result = self._rockbox_deploy.apply_diff(profile, diff)
        message = f"Copied {result['copied_count']} files\nBackup: {result['backup_dir']}"
        if result["failures"]:
            message += "\n\nFailures:\n" + "\n".join(result["failures"][:8])
        elif str(profile.get("screen_resolution") or "").strip() == "176x132":
            firmware_result = self._rebuild_and_deploy_runtime_firmware(profile)
            if firmware_result["success"]:
                message += "\n\nRebuilt and deployed rockbox.ipod for nano2g runtime changes."
            else:
                message += f"\n\nFirmware rebuild/deploy failed:\n{firmware_result['message']}"
        QMessageBox.information(self, "Wallpaper Apply Result", message)
        self._status_bar.set_left_text(
            "Wallpapers applied" if result["success"] else "Wallpaper apply completed with errors"
        )
        self._refresh_ipone_wallpapers()

    def _rebuild_and_deploy_runtime_firmware(self, profile):
        build = self._rockbox_boot.rebuild_firmware(profile)
        if not build["success"]:
            details = "\n".join(part for part in (build.get("stderr", ""), build.get("stdout", "")) if part.strip())
            return {
                "success": False,
                "message": details[:4000] if details else build["message"],
            }
        try:
            bundle = self._rockbox_boot.build_firmware_bundle(build["artifact_path"])
            deploy_profile = self._rockbox_boot.deploy_profile(profile, "device")
            diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
            result = self._rockbox_deploy.apply_diff(deploy_profile, diff)
        except ValueError as exc:
            return {"success": False, "message": str(exc)}
        if not result["success"]:
            return {"success": False, "message": "\n".join(result["failures"][:8])}
        return {"success": True, "message": "", "copied_count": result["copied_count"]}

    def _import_ipone_wallpaper(self, kind, source_path):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._status_bar.set_left_text("No Rockbox profile selected")
            return
        try:
            item = self._ipone_wallpapers_service.import_candidate(
                profile["source_repo_path"],
                profile,
                kind,
                source_path,
            )
        except ValueError as exc:
            QMessageBox.warning(self, "Add Wallpaper", str(exc))
            return
        self._refresh_ipone_wallpapers()
        label = item.get("label") or os.path.basename(item.get("source_path", source_path))
        kind_label = {
            "lock": "lockscreen",
            "charge": "charge",
            "pictureflow": "PictureFlow init",
        }.get(kind, kind)
        self._status_bar.set_left_text(f"Added {kind_label} wallpaper: {label}")

    def _remove_ipone_wallpaper(self, kind, candidate):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._status_bar.set_left_text("No Rockbox profile selected")
            return
        source_path = str(candidate.get("source_path") or "").strip()
        if not source_path:
            return
        if not candidate.get("removable"):
            QMessageBox.information(self, "Remove Wallpaper", "Built-in theme wallpapers cannot be removed.")
            return
        reply = QMessageBox.question(
            self,
            "Remove Wallpaper",
            f"Remove {candidate.get('label') or os.path.basename(source_path)}?",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return
        try:
            removed = self._ipone_wallpapers_service.remove_candidate(
                profile["source_repo_path"],
                source_path,
            )
        except ValueError as exc:
            QMessageBox.warning(self, "Remove Wallpaper", str(exc))
            return
        self._refresh_ipone_wallpapers()
        if removed:
            kind_label = "lockscreen" if kind == "lock" else "charge"
            self._status_bar.set_left_text(f"Removed {kind_label} wallpaper")

    def _hide_ipone_wallpaper(self, kind, candidate):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._status_bar.set_left_text("No Rockbox profile selected")
            return
        source_path = str(candidate.get("source_path") or "").strip()
        if not source_path:
            return
        hidden = not bool(candidate.get("hidden"))
        try:
            changed = self._ipone_wallpapers_service.set_hidden(
                profile["source_repo_path"],
                source_path,
                hidden,
            )
        except ValueError as exc:
            QMessageBox.warning(self, "Hide Wallpaper", str(exc))
            return
        self._refresh_ipone_wallpapers()
        if changed:
            kind_label = "lockscreen" if kind == "lock" else "charge"
            action = "Hidden" if hidden else "Unhidden"
            self._status_bar.set_left_text(f"{action} {kind_label} wallpaper")

    def _toggle_hidden_wallpapers(self, checked):
        checked = bool(checked)
        if checked:
            if not self._ensure_hidden_wallpaper_password():
                self._show_hidden_wallpapers_action.blockSignals(True)
                self._show_hidden_wallpapers_action.setChecked(False)
                self._show_hidden_wallpapers_action.blockSignals(False)
                return
        self._show_hidden_wallpapers = checked
        self._refresh_ipone_wallpapers()
        self._refresh_photo_manager()

    def _ensure_hidden_wallpaper_password(self):
        stored_hash = str(self._config.get("hidden_wallpapers_password_hash", "") or "")
        if not stored_hash:
            return self._set_hidden_wallpaper_password()
        password, ok = QInputDialog.getText(
            self,
            "Show Hidden Wallpapers/Photos",
            "Password:",
            QLineEdit.Password,
        )
        if not ok:
            return False
        if self._verify_hidden_wallpaper_password(password):
            return True
        QMessageBox.warning(self, "Show Hidden Wallpapers/Photos", "Incorrect password.")
        return False

    def _set_hidden_wallpaper_password(self):
        password, ok = QInputDialog.getText(
            self,
            "Set Hidden Wallpapers/Photos Password",
            "Create password:",
            QLineEdit.Password,
        )
        if not ok:
            return False
        password = str(password or "")
        if not password:
            QMessageBox.warning(self, "Set Hidden Wallpapers/Photos Password", "Password cannot be empty.")
            return False
        confirm, ok = QInputDialog.getText(
            self,
            "Set Hidden Wallpapers/Photos Password",
            "Confirm password:",
            QLineEdit.Password,
        )
        if not ok:
            return False
        if password != str(confirm or ""):
            QMessageBox.warning(self, "Set Hidden Wallpapers/Photos Password", "Passwords do not match.")
            return False
        salt = secrets.token_bytes(16)
        digest = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, 120_000)
        self._config.set("hidden_wallpapers_password_salt", base64.b64encode(salt).decode("ascii"))
        self._config.set("hidden_wallpapers_password_hash", base64.b64encode(digest).decode("ascii"))
        self._config.save()
        return True

    def _verify_hidden_wallpaper_password(self, password):
        try:
            salt = base64.b64decode(str(self._config.get("hidden_wallpapers_password_salt", "") or ""))
            expected = base64.b64decode(str(self._config.get("hidden_wallpapers_password_hash", "") or ""))
        except (ValueError, TypeError):
            return False
        digest = hashlib.pbkdf2_hmac("sha256", str(password or "").encode("utf-8"), salt, 120_000)
        return secrets.compare_digest(digest, expected)

    def _refresh_theme_hub(self):
        profiles = self._rockbox_profiles.profiles()
        selected_id = self._rockbox_profiles.selected_profile_id()
        self._theme_hub.set_profiles(profiles, selected_id)
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._current_theme_diff = None
            self._theme_hub.set_diff_summary(None)
            return
        themes = self._rockbox_themes.list_themes(
            profile["source_repo_path"],
            profile["screen_resolution"],
            profile.get("target_device_model", ""),
            profile.get("device_mount_path", ""),
        )
        if not themes:
            self._current_theme_diff = None
            self._theme_hub.set_themes([], "")
            self._theme_hub.set_diff_summary(None)
            return
        selected_theme = profile.get("selected_theme") or themes[0]["id"]
        if not any(item["id"] == selected_theme for item in themes):
            selected_theme = themes[0]["id"]
            profile["selected_theme"] = selected_theme
            self._rockbox_profiles.save_profile(profile)
        self._theme_hub.set_themes(themes, selected_theme)
        self._set_theme_details(profile, selected_theme)

    def _set_theme_details(self, profile, theme_id):
        if theme_id in THEME_DEFINITIONS:
            details = self._rockbox_themes.inspect_theme(theme_id, profile["source_repo_path"])
        else:
            details = self._rockbox_themes.inspect_device_theme(theme_id, profile.get("device_mount_path", ""))
        self._theme_hub.set_theme_details(details)
        try:
            diff = self._rockbox_deploy.build_diff(profile, details)
        except ValueError:
            diff = None
        self._current_theme_diff = diff
        self._theme_hub.set_diff_summary(diff)

    def _on_theme_profile_selected(self, profile_id):
        self._rockbox_profiles.set_selected_profile(profile_id)
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_theme_designer()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_simulator_panel()

    def _on_theme_profile_saved(self, profile_data):
        profile = self._rockbox_profiles.save_profile(profile_data)
        self._status_bar.set_left_text(f"Saved Rockbox profile: {profile['name']}")
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_theme_designer()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_simulator_panel()

    def _on_theme_selected(self, theme_id):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        profile["selected_theme"] = theme_id
        self._rockbox_profiles.save_profile(profile)
        self._set_theme_details(profile, theme_id)
        self._refresh_ipone_wallpapers()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_simulator_panel()

    def _use_connected_device_for_profile(self):
        if not self._device_detector.current_device:
            self._status_bar.set_left_text("No connected Rockbox device")
            return
        profile = self._rockbox_profiles.sync_with_device(self._device_detector.current_device)
        self._status_bar.set_left_text(f"Updated profile mount path: {profile['device_mount_path']}")
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_theme_designer()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_simulator_panel()

    def _deploy_selected_theme(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile or not self._current_theme_diff:
            self._status_bar.set_left_text("No Rockbox theme diff available")
            return
        summary = self._current_theme_diff["summary"]
        detail = (
            f"Add {summary['add']} files\n"
            f"Overwrite {summary['overwrite']} files\n"
            f"Unchanged {summary['unchanged']} files"
        )
        dry_run = QMessageBox(self)
        dry_run.setWindowTitle("Apply Rockbox Theme")
        dry_run.setText(
            f"Deploy {profile['selected_theme']} to {profile['device_mount_path']}?\n\n{detail}"
        )
        lines = []
        for state in ("add", "overwrite", "unchanged"):
            matching = [item["destination_rel"] for item in self._current_theme_diff["items"] if item["status"] == state]
            if matching:
                lines.append(state.upper())
                lines.extend(f"- {path}" for path in matching[:40])
                if len(matching) > 40:
                    lines.append(f"... and {len(matching) - 40} more")
                lines.append("")
        if lines:
            dry_run.setDetailedText("\n".join(lines).strip())
        dry_run.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        dry_run.setDefaultButton(QMessageBox.Yes)
        dry_run.exec()
        if dry_run.clickedButton() != dry_run.button(QMessageBox.Yes):
            return
        result = self._rockbox_deploy.apply_diff(profile, self._current_theme_diff)
        message = f"Copied {result['copied_count']} files\nBackup: {result['backup_dir']}"
        if result["failures"]:
            message += "\n\nFailures:\n" + "\n".join(result["failures"][:8])
        dialog = QMessageBox(self)
        dialog.setWindowTitle("Theme Deploy Result")
        dialog.setText(message)
        rollback_button = None
        if result["rollback_available"]:
            rollback_button = dialog.addButton("Rollback Now", QMessageBox.ActionRole)
        dialog.addButton(QMessageBox.Ok)
        dialog.exec()
        if rollback_button is not None and dialog.clickedButton() == rollback_button:
            self._restore_theme_backup()
        self._status_bar.set_left_text("Theme deployed" if result["success"] else "Theme deploy completed with errors")
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_plugin_manager()
        self._refresh_game_manager()

    def _delete_selected_theme_from_device(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._status_bar.set_left_text("No Rockbox profile selected")
            return
        theme_id = str(profile.get("selected_theme") or "").strip()
        if not theme_id:
            self._status_bar.set_left_text("No Rockbox theme selected")
            return
        try:
            bundle = self._rockbox_themes.remove_bundle_for_theme(
                theme_id,
                profile["source_repo_path"],
                profile.get("device_mount_path", ""),
            )
            diff = self._rockbox_deploy.build_diff(profile, bundle)
        except ValueError as exc:
            QMessageBox.warning(self, "Delete Theme Failed", str(exc))
            return

        remove_items = [item for item in diff["items"] if item["status"] == "remove"]
        if not remove_items:
            QMessageBox.information(self, "Delete Theme", f"No {theme_id} files were found on this device.")
            self._refresh_theme_hub()
            return

        confirm = QMessageBox(self)
        confirm.setWindowTitle("Delete Theme From Device")
        confirm.setText(
            f"Delete {len(remove_items)} {theme_id} theme file(s) from:\n"
            f"{profile['device_mount_path']}?\n\n"
            "A backup is created first, so Restore Previous can put these files back."
        )
        lines = [item["destination_rel"] for item in remove_items]
        if lines:
            detail = "\n".join(f"- {path}" for path in lines[:80])
            if len(lines) > 80:
                detail += f"\n... and {len(lines) - 80} more"
            confirm.setDetailedText(detail)
        confirm.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        confirm.setDefaultButton(QMessageBox.No)
        confirm.exec()
        if confirm.clickedButton() != confirm.button(QMessageBox.Yes):
            return

        result = self._rockbox_deploy.apply_diff(profile, diff)
        message = f"Removed {result['copied_count']} files\nBackup: {result['backup_dir']}"
        if result["failures"]:
            message += "\n\nFailures:\n" + "\n".join(result["failures"][:8])
        dialog = QMessageBox(self)
        dialog.setWindowTitle("Theme Delete Result")
        dialog.setText(message)
        rollback_button = None
        if result["rollback_available"]:
            rollback_button = dialog.addButton("Restore Now", QMessageBox.ActionRole)
        dialog.addButton(QMessageBox.Ok)
        dialog.exec()
        if rollback_button is not None and dialog.clickedButton() == rollback_button:
            self._restore_theme_backup()
        self._status_bar.set_left_text("Theme files deleted" if result["success"] else "Theme delete completed with errors")
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_plugin_manager()
        self._refresh_game_manager()

    def _reset_device_to_default(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._status_bar.set_left_text("No Rockbox profile selected")
            return
        try:
            bundle = self._rockbox_themes.build_reset_to_default_bundle(
                profile["source_repo_path"],
                profile.get("device_mount_path", ""),
            )
            diff = self._rockbox_deploy.build_diff(profile, bundle)
        except ValueError as exc:
            QMessageBox.warning(self, "Reset To Default Failed", str(exc))
            return

        remove_items = [item for item in diff["items"] if item["status"] == "remove"]
        if not remove_items:
            QMessageBox.information(self, "Reset To Default", "Device already has only default theme files.")
            self._refresh_theme_hub()
            return

        confirm = QMessageBox(self)
        confirm.setWindowTitle("Reset To Default")
        confirm.setText(
            f"Remove {len(remove_items)} custom theme file(s) from:\n"
            f"{profile['device_mount_path']}?\n\n"
            "A backup is created first, so Restore Previous can put these files back."
        )
        lines = [item["destination_rel"] for item in remove_items]
        if lines:
            detail = "\n".join(f"- {path}" for path in lines[:80])
            if len(lines) > 80:
                detail += f"\n... and {len(lines) - 80} more"
            confirm.setDetailedText(detail)
        confirm.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        confirm.setDefaultButton(QMessageBox.No)
        confirm.exec()
        if confirm.clickedButton() != confirm.button(QMessageBox.Yes):
            return

        result = self._rockbox_deploy.apply_diff(profile, diff)
        message = f"Removed {result['copied_count']} files\nBackup: {result['backup_dir']}"
        if result["failures"]:
            message += "\n\nFailures:\n" + "\n".join(result["failures"][:8])
        dialog = QMessageBox(self)
        dialog.setWindowTitle("Reset To Default Result")
        dialog.setText(message)
        rollback_button = None
        if result["rollback_available"]:
            rollback_button = dialog.addButton("Restore Now", QMessageBox.ActionRole)
        dialog.addButton(QMessageBox.Ok)
        dialog.exec()
        if rollback_button is not None and dialog.clickedButton() == rollback_button:
            self._restore_theme_backup()
        self._status_bar.set_left_text(
            "Device reset to default theme files"
            if result["success"]
            else "Reset to default completed with errors"
        )
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_plugin_manager()
        self._refresh_game_manager()

    def _restore_theme_backup(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        result = self._rockbox_deploy.restore_latest_backup(profile)
        if result["success"]:
            self._status_bar.set_left_text(f"Restored {result['restored_count']} files from backup")
            QMessageBox.information(
                self,
                "Theme Restored",
                f"Restored {result['restored_count']} files from:\n{result['backup_dir']}",
            )
        else:
            QMessageBox.warning(self, "Restore Failed", "\n".join(result["failures"]))
            self._status_bar.set_left_text("Theme restore failed")
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_simulator_panel()

    def _refresh_theme_designer(self):
        profiles = self._rockbox_profiles.profiles()
        selected_id = self._rockbox_profiles.selected_profile_id()
        self._theme_designer.set_profiles(profiles, selected_id)
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._current_designer_variant = None
            self._current_designer_draft = None
            self._current_designer_preview_path = ""
            return
        fonts = self._theme_designer_service.fonts_for_profile(profile["source_repo_path"])
        self._theme_designer.set_fonts(fonts)
        variants = self._theme_designer_service.list_variants(
            profile["source_repo_path"],
            profile["screen_resolution"],
        )
        draft = None
        if self._current_designer_draft:
            draft_profile_id = str(self._current_designer_draft.get("_profile_id") or "").strip()
            draft_resolution = str(self._current_designer_draft.get("screen_resolution") or "").strip()
            if draft_profile_id == profile["id"] and draft_resolution == profile["screen_resolution"]:
                draft = dict(self._current_designer_draft)
        selected_variant_id = (
            draft["id"]
            if draft and draft.get("id")
            else self._current_designer_variant["id"]
            if self._current_designer_variant and self._current_designer_variant.get("screen_resolution") == profile["screen_resolution"]
            else ""
        )
        self._theme_designer.set_variants(variants, selected_variant_id)
        if draft is not None:
            variant = dict(draft)
            variant.pop("_profile_id", None)
        elif selected_variant_id:
            variant = self._theme_designer_service.load_variant(profile["source_repo_path"], selected_variant_id)
        else:
            variant = self._theme_designer_service.new_variant(profile["source_repo_path"], profile)
        self._current_designer_variant = variant
        preview = self._theme_designer_service.build_preview_state(profile["source_repo_path"], profile, variant)
        if self._current_designer_preview_path and os.path.isfile(self._current_designer_preview_path):
            preview["simulator_preview_path"] = self._current_designer_preview_path
        self._theme_designer.load_variant(variant, preview)
        self._refresh_theme_designer_simulator_preview(profile)

    def _on_theme_designer_profile_selected(self, profile_id):
        self._rockbox_profiles.set_selected_profile(profile_id)
        self._current_designer_variant = None
        self._current_designer_draft = None
        self._current_designer_preview_path = ""
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_theme_designer()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_simulator_panel()

    def _on_theme_designer_variant_selected(self, variant_id):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        self._current_designer_preview_path = ""
        if variant_id:
            variant = self._theme_designer_service.load_variant(profile["source_repo_path"], variant_id)
        else:
            variant = self._theme_designer_service.new_variant(profile["source_repo_path"], profile)
        self._current_designer_draft = dict(variant)
        self._current_designer_draft["_profile_id"] = profile["id"]
        self._current_designer_variant = variant
        preview = self._theme_designer_service.build_preview_state(profile["source_repo_path"], profile, variant)
        self._theme_designer.load_variant(variant, preview)
        self._refresh_theme_designer_simulator_preview(profile)

    def _preferred_theme_designer_simulator_target(self, profile):
        target_id = profile.get("simulator_target") or ""
        target = self._simulator_target_by_id(target_id) if target_id else None
        if target:
            return target

        preferred_ids = []
        if profile.get("screen_resolution") == "320x240":
            preferred_ids.extend(["build-sim-video-5g", "build-sim-ipod6g", "build-sim"])
        elif profile.get("screen_resolution") == "176x132":
            preferred_ids.extend(["build-sim-nano2g"])

        for preferred_id in preferred_ids:
            target = self._simulator_target_by_id(preferred_id)
            if target:
                return target

        if self._simulator_targets:
            matches = [item for item in self._simulator_targets if item["screen_resolution"] == profile["screen_resolution"]]
            return matches[0] if matches else self._simulator_targets[0]
        return None

    def _on_theme_designer_preview_changed(self, variant_data, mode):
        self._pending_designer_preview_variant = dict(variant_data or {})
        self._pending_designer_preview_mode = str(mode or "")
        profile = self._rockbox_profiles.current_profile()
        if profile and variant_data:
            self._current_designer_draft = dict(variant_data)
            self._current_designer_draft["_profile_id"] = profile["id"]
        self._designer_preview_timer.stop()

    def _refresh_theme_designer_preview_now(self, variant_data):
        self._pending_designer_preview_variant = dict(variant_data or {})
        self._pending_designer_preview_mode = "simulator"
        profile = self._rockbox_profiles.current_profile()
        if profile and variant_data:
            self._current_designer_draft = dict(variant_data)
            self._current_designer_draft["_profile_id"] = profile["id"]
        self._apply_theme_designer_preview_to_simulator()

    def _refresh_theme_designer_simulator_preview(self, profile=None):
        profile = profile or self._rockbox_profiles.current_profile()
        if not profile:
            self._theme_designer.set_simulator_target("", "")
            self._theme_designer.set_simulator_preview("")
            return
        target = self._preferred_theme_designer_simulator_target(profile)
        if not target:
            target = self._rockbox_simulator.fallback_ipodvideo_target(profile)
        preview_target = self._rockbox_simulator.theme_designer_preview_target(profile, target) if target else None
        self._theme_designer.set_simulator_target(
            (preview_target or {}).get("binary_path", ""),
            (preview_target or {}).get("simdisk_path", ""),
        )
        shot = ""
        if self._current_designer_preview_path and os.path.isfile(self._current_designer_preview_path):
            shot = self._current_designer_preview_path
        self._theme_designer.set_simulator_preview(shot)

    def _ensure_theme_designer_default_preview(self, profile, variant):
        if not profile or not variant:
            return
        if self._current_designer_preview_path and os.path.isfile(self._current_designer_preview_path):
            return
        preview_variant = dict(variant)
        preview_variant["preview_screen"] = "sbs"
        self._theme_designer.set_preview_screen("sbs")
        self._pending_designer_preview_variant = preview_variant
        self._pending_designer_preview_mode = "simulator"
        self._apply_theme_designer_preview_to_simulator()

    def _apply_theme_designer_preview_to_simulator(self):
        if self._pending_designer_preview_mode != "simulator":
            return
        if self._designer_preview_running:
            self._designer_preview_rerun_requested = True
            return
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        self._designer_preview_running = True
        self._designer_preview_rerun_requested = False
        self._theme_designer.set_preview_loading(True)
        target = self._preferred_theme_designer_simulator_target(profile)
        if not target:
            target = self._rockbox_simulator.fallback_ipodvideo_target(profile)
        if not target:
            self._designer_preview_running = False
            self._theme_designer.set_preview_loading(False)
            return
        preview_variant = dict(
            self._pending_designer_preview_variant
            if self._pending_designer_preview_variant is not None
            else (self._current_designer_variant or {})
        )
        preview_screen = str(preview_variant.get("preview_screen") or "sbs").strip().lower()
        if preview_screen not in {"sbs", "wps", "lockscreen"}:
            preview_screen = "sbs"
        shot = ""
        try:
            bundle = self._theme_designer_service.build_preview_bundle(
                profile["source_repo_path"],
                profile,
                preview_variant,
            )
        except Exception:
            logger.exception("iPone designer preview bundle generation failed")
            self._designer_preview_running = False
            self._theme_designer.set_preview_loading(False)
            return

        try:
            self._theme_designer.shutdown_simulator_preview()
            preview_target = self._rockbox_simulator.theme_designer_preview_target(profile, target, reset=False)
            preview_profile = self._rockbox_simulator.simulator_profile(profile, preview_target)
            diff = self._rockbox_deploy.build_diff(preview_profile, bundle)
            self._rockbox_deploy.apply_diff(preview_profile, diff)
            self._rockbox_simulator.activate_theme_preview(
                preview_target,
                bundle["id"],
                preview_screen=preview_screen,
            )
            shot = self._rockbox_simulator.capture_theme_preview(
                preview_target,
                preview_screen=preview_screen,
            )
        except Exception:
            logger.exception("iPone designer preview deployment failed")
            shot = ""
        finally:
            self._designer_preview_running = False
            self._designer_preview_rerun_requested = False
            if shot:
                self._current_designer_preview_path = shot
                self._theme_designer.set_simulator_preview(shot)
            else:
                self._theme_designer.set_preview_loading(False)

    def _save_theme_designer_variant(self, variant_data):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        merged = dict(self._current_designer_variant or {})
        merged.update(variant_data)
        saved = self._theme_designer_service.save_variant(profile["source_repo_path"], merged)
        self._current_designer_variant = saved
        self._current_designer_draft = dict(saved)
        self._current_designer_draft["_profile_id"] = profile["id"]
        self._status_bar.set_left_text(f"Saved theme variant: {saved['name']}")
        self._refresh_theme_designer()

    def _rename_theme_designer_variant(self, variant_id, new_name):
        profile = self._rockbox_profiles.current_profile()
        if not profile or not variant_id:
            return
        renamed = self._theme_designer_service.rename_variant(profile["source_repo_path"], variant_id, new_name)
        self._current_designer_variant = renamed
        self._current_designer_draft = dict(renamed)
        self._current_designer_draft["_profile_id"] = profile["id"]
        self._status_bar.set_left_text(f"Renamed theme variant: {renamed['name']}")
        self._refresh_theme_designer()

    def _duplicate_theme_designer_variant(self, variant_id):
        profile = self._rockbox_profiles.current_profile()
        if not profile or not variant_id:
            return
        duplicated = self._theme_designer_service.duplicate_variant(profile["source_repo_path"], variant_id)
        self._current_designer_variant = duplicated
        self._status_bar.set_left_text(f"Duplicated theme variant: {duplicated['name']}")
        self._refresh_theme_designer()

    def _delete_theme_designer_variant(self, variant_id):
        profile = self._rockbox_profiles.current_profile()
        if not profile or not variant_id:
            return
        try:
            variant = self._theme_designer_service.load_variant(profile["source_repo_path"], variant_id)
        except FileNotFoundError:
            self._status_bar.set_left_text("Saved iPone variant was not found")
            self._refresh_theme_designer()
            return

        confirm = QMessageBox(self)
        confirm.setWindowTitle("Delete iPone Design")
        confirm.setText(
            f"Delete '{variant['name']}'?\n\n"
            "This removes the saved RockPod design. If matching generated theme files are on the connected iPod, "
            "RockPod will remove those too after making a backup."
        )
        confirm.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        confirm.setDefaultButton(QMessageBox.No)
        confirm.exec()
        if confirm.clickedButton() != confirm.button(QMessageBox.Yes):
            return

        removed_device_files = 0
        failures = []
        try:
            bundle = self._theme_designer_service.build_remove_bundle(profile["source_repo_path"], profile, variant)
            diff = self._rockbox_deploy.build_diff(profile, bundle)
            if diff["summary"].get("remove", 0):
                result = self._rockbox_deploy.apply_diff(profile, diff)
                removed_device_files = result.get("copied_count", 0)
                failures = result.get("failures", [])
        except ValueError as exc:
            failures = [str(exc)]

        local_removed = self._theme_designer_service.delete_variant(profile["source_repo_path"], variant_id)
        self._current_designer_variant = None
        self._current_designer_draft = None
        self._current_designer_preview_path = ""
        message = f"Deleted saved design: {variant['name']}" if local_removed else f"Saved design already deleted: {variant['name']}"
        if removed_device_files:
            message += f"\nRemoved {removed_device_files} generated file(s) from device."
        if failures:
            message += "\n\nDevice cleanup issues:\n" + "\n".join(failures[:8])
            QMessageBox.warning(self, "Delete iPone Design", message)
        else:
            QMessageBox.information(self, "Delete iPone Design", message)
        self._status_bar.set_left_text(f"Deleted iPone design: {variant['name']}")
        self._refresh_theme_designer()
        self._refresh_theme_hub()

    def _deploy_theme_designer_variant_to_device(self, variant_data):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        bundle = self._saved_theme_bundle_for_profile(profile, variant_data)
        if not bundle:
            return
        original_variant_data = variant_data
        confirmed_variant_data = self._confirm_theme_designer_readability(profile, variant_data, bundle)
        if confirmed_variant_data is None:
            return
        if confirmed_variant_data is not original_variant_data:
            bundle = self._saved_theme_bundle_for_profile(profile, confirmed_variant_data)
            if not bundle:
                return
        try:
            runtime_assets = self._build_theme_designer_runtime_config_asset(
                profile, bundle.get("variant"), bundle
            )
        except Exception as exc:
            logger.exception("Failed to build iPone designer runtime config")
            runtime_assets = []
            QMessageBox.warning(
                self,
                "iPone Designer Runtime Config",
                f"Could not prepare device config update:\n{exc}"
            )
        if runtime_assets:
            bundle["assets"].extend(runtime_assets if isinstance(runtime_assets, list) else [runtime_assets])
        self._deploy_theme_bundle(profile, bundle, "device")

    def _deploy_theme_designer_variant_to_simulator(self, variant_data):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        bundle = self._saved_theme_bundle_for_profile(profile, variant_data)
        if not bundle:
            return
        original_variant_data = variant_data
        confirmed_variant_data = self._confirm_theme_designer_readability(profile, variant_data, bundle)
        if confirmed_variant_data is None:
            return
        if confirmed_variant_data is not original_variant_data:
            bundle = self._saved_theme_bundle_for_profile(profile, confirmed_variant_data)
            if not bundle:
                return
        target_id = profile.get("simulator_target") or ""
        target = self._simulator_target_by_id(target_id) if target_id else None
        if not target and self._simulator_targets:
            matches = [item for item in self._simulator_targets if item["screen_resolution"] == profile["screen_resolution"]]
            target = matches[0] if matches else self._simulator_targets[0]
        if not target:
            QMessageBox.information(self, "iPone Designer", "No simulator target is available for this profile.")
            return
        sim_profile = self._rockbox_simulator.simulator_profile(profile, target)
        self._deploy_theme_bundle(sim_profile, bundle, "simulator")

    def _saved_theme_bundle_for_profile(self, profile, variant_data):
        merged = dict(self._current_designer_variant or {})
        merged.update(variant_data)
        try:
            saved = self._theme_designer_service.save_variant(profile["source_repo_path"], merged)
            bundle = self._theme_designer_service.build_bundle(profile["source_repo_path"], profile, saved)
        except ValueError as exc:
            QMessageBox.warning(self, "iPone Designer", str(exc))
            return None
        self._current_designer_variant = saved
        self._refresh_theme_designer()
        return bundle

    def _confirm_theme_designer_readability(self, profile, variant_data, bundle=None):
        merged = dict(self._current_designer_variant or {})
        merged.update(variant_data or {})
        budget_issues = []
        if bundle:
            budget_issues = [
                issue for issue in self._theme_designer_service.render_budget_recommendations_for_bundle(
                    profile["source_repo_path"],
                    profile,
                    bundle,
                )
                if issue.get("level") in {"critical", "warn"}
            ]
        if budget_issues:
            lines = []
            for issue in budget_issues[:6]:
                lines.append(
                    f"{issue['skin']}: {issue['label']} - {issue['detail']} "
                    f"{issue['recommendation']}"
                )
            if len(budget_issues) > 6:
                lines.append(f"+{len(budget_issues) - 6} more")

            dialog = QMessageBox(self)
            dialog.setWindowTitle("Render Budget Check")
            dialog.setIcon(QMessageBox.Warning)
            dialog.setText("Some generated iPone skin patterns may slow Rockbox redraws.")
            dialog.setInformativeText("\n".join(lines))
            dialog.addButton("Install Anyway", QMessageBox.DestructiveRole)
            cancel_button = dialog.addButton(QMessageBox.Cancel)
            dialog.exec()
            if dialog.clickedButton() is cancel_button:
                return None

        if bundle:
            issues = self._theme_designer_service.readability_recommendations_for_bundle(
                profile["source_repo_path"],
                profile,
                bundle,
            )
        else:
            issues = self._theme_designer_service.readability_recommendations(
                profile["source_repo_path"],
                profile,
                merged,
            )
        if not issues:
            return variant_data

        lines = []
        for issue in issues[:6]:
            lines.append(
                f"{issue['label']}: #{issue['current']} on {issue['background']} "
                f"is {issue['ratio']:.1f}:1; recommended #{issue['recommended']} "
                f"({issue['recommended_ratio']:.1f}:1)."
            )
        if len(issues) > 6:
            lines.append(f"+{len(issues) - 6} more")

        dialog = QMessageBox(self)
        dialog.setWindowTitle("Readability Check")
        dialog.setIcon(QMessageBox.Warning)
        dialog.setText("Some generated iPone skin text may be hard to read.")
        dialog.setInformativeText("\n".join(lines))
        apply_button = dialog.addButton("Use Recommended Colors", QMessageBox.AcceptRole)
        dialog.addButton("Install Anyway", QMessageBox.DestructiveRole)
        cancel_button = dialog.addButton(QMessageBox.Cancel)
        dialog.exec()
        clicked = dialog.clickedButton()
        if clicked is cancel_button:
            return None
        if clicked is not apply_button:
            return variant_data

        updated = dict(variant_data or {})
        colors = dict(updated.get("colors") or merged.get("colors") or {})
        clock = dict(updated.get("lockscreen_clock") or merged.get("lockscreen_clock") or {})
        for issue in issues:
            target = issue.get("target")
            recommended = issue.get("recommended")
            if target == "lockscreen_clock.color":
                clock["color"] = recommended
            elif target and target.startswith("colors."):
                colors[target.split(".", 1)[1]] = recommended
        if colors:
            updated["colors"] = colors
        if clock:
            updated["lockscreen_clock"] = clock
        return updated

    def _build_theme_designer_runtime_config_asset(self, profile, variant, bundle=None):
        variant_id = str((variant or {}).get("id") or "").strip()
        if not variant_id:
            return []

        mount_root = os.path.abspath(str(profile.get("device_mount_path") or "").strip())
        if not mount_root:
            return []

        runtime_dir = os.path.join(
            os.path.abspath(profile.get("source_repo_path") or ""),
            "rockpod",
            ".theme_designer",
            "runtime_configs",
        )
        os.makedirs(runtime_dir, exist_ok=True)
        runtime_cfg = os.path.join(runtime_dir, f"{variant_id}.cfg")
        live_cfg = os.path.join(mount_root, ".rockbox", "config.cfg")
        if os.path.isfile(live_cfg):
            shutil.copy2(live_cfg, runtime_cfg)
        else:
            atomic_write_text(runtime_cfg, "# Generated by RockPod\n")

        theme_settings = {}
        cfg_asset = next(
            (item for item in (bundle or {}).get("assets", []) if item.get("kind") == "cfg"),
            None,
        )
        cfg_source = str((cfg_asset or {}).get("source_abs") or "").strip()
        if cfg_source and os.path.isfile(cfg_source):
            theme_settings = self._rockbox_simulator._read_cfg_settings(cfg_source)

        right_pane_mode = self._theme_designer_service._normalize_right_pane_mode(
            (variant or {}).get("right_pane_mode"),
            (variant or {}).get("base_right_pane_mode", "miniplayer"),
        )

        variant_sbs_name = self._theme_designer_service._sbs_skin_name(variant or {})
        variant_theme_path = theme_settings.get("theme") or f"/.rockbox/themes/{variant_id}.cfg"
        variant_wps_path = theme_settings.get("wps") or f"/.rockbox/wps/{variant_id}.wps"
        variant_sbs_path = theme_settings.get("sbs") or f"/.rockbox/wps/{variant_sbs_name}.sbs"
        variant_fms_path = theme_settings.get("fms") or f"/.rockbox/wps/{variant_id}.fms"
        variant_backdrop_path = f"/.rockbox/backdrops/{variant_id}_bd.bmp"
        variant_iconset_path = f"/.rockbox/icons/{variant_id}.bmp"
        font_rel = str((variant or {}).get("font_rel", "") or "").strip()
        font_ref = f"/.rockbox/{font_rel.lstrip('/')}" if font_rel else ""

        overrides = dict(theme_settings)
        overrides.update(
            {
                "theme": variant_theme_path,
                "wps": variant_wps_path,
                "sbs": variant_sbs_path,
                "fms": variant_fms_path,
                "backdrop": theme_settings.get("backdrop") or variant_backdrop_path,
                "iconset": theme_settings.get("iconset") or variant_iconset_path,
                "viewers iconset": "-",
                "statusbar": "off",
                "ui viewport": "-",
                "ipone right pane": right_pane_mode,
                **({"font": font_ref} if font_ref else {}),
            },
        )
        self._rockbox_simulator._merge_cfg_settings(runtime_cfg, overrides)

        file_size = os.path.getsize(runtime_cfg)
        assets = []
        for destination_rel in (".rockbox/config.cfg", "config.cfg"):
            assets.append(
                {
                    "kind": "runtime_config",
                    "source_rel": f"theme_designer/runtime_configs/{variant_id}.cfg",
                    "source_abs": runtime_cfg,
                    "destination_rel": destination_rel,
                    "exists": True,
                    "size": file_size,
                    "preview_path": runtime_cfg,
                    "action": "copy",
                    "preserve_metadata": True,
                }
            )

        if len(assets) == 1:
            return assets[0]
        return assets

    def _deploy_theme_bundle(self, deploy_profile, bundle, target_label):
        try:
            diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
        except ValueError as exc:
            QMessageBox.warning(self, "iPone Designer", str(exc))
            return
        summary = diff["summary"]
        prompt = QMessageBox(self)
        prompt.setWindowTitle("Deploy Theme Variant")
        prompt.setText(
            f"Deploy {bundle['name']} to {target_label}?\n\n"
            f"Add {summary['add']} files\n"
            f"Overwrite {summary['overwrite']} files\n"
            f"Unchanged {summary['unchanged']} files"
        )
        prompt.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        prompt.setDefaultButton(QMessageBox.Yes)
        prompt.exec()
        if prompt.clickedButton() != prompt.button(QMessageBox.Yes):
            return
        progress = QProgressDialog(
            f"Installing {bundle['name']} to {target_label}...",
            None,
            0,
            3,
            self,
        )
        progress.setWindowTitle("Installing iPone Design")
        progress.setWindowModality(Qt.ApplicationModal)
        progress.setCancelButton(None)
        progress.setMinimumDuration(0)
        progress.setValue(0)
        QApplication.processEvents()
        progress.setLabelText("Copying theme files...")
        progress.setValue(1)
        QApplication.processEvents()
        result = self._rockbox_deploy.apply_diff(deploy_profile, diff)
        progress.setLabelText("Refreshing RockPod views...")
        progress.setValue(2)
        QApplication.processEvents()
        message = f"Copied {result['copied_count']} files\nBackup: {result['backup_dir']}"
        if result["failures"]:
            message += "\n\nFailures:\n" + "\n".join(result["failures"][:8])
        progress.setValue(3)
        progress.close()
        QMessageBox.information(self, "iPone Designer Deploy", message)
        self._status_bar.set_left_text(
            "Theme variant deployed" if result["success"] else "Theme variant deploy completed with errors"
        )
        self._refresh_theme_hub()
        self._refresh_ipone_wallpapers()
        self._refresh_theme_designer()
        self._refresh_simulator_panel()

    # ═══════════════════════════════════════════════════════════════
    # Rockbox boot / branding
    # ═══════════════════════════════════════════════════════════════

    def _refresh_boot_manager(self):
        profiles = self._rockbox_profiles.profiles()
        selected_id = self._rockbox_profiles.selected_profile_id()
        self._boot_manager.set_profiles(profiles, selected_id)
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._current_boot_diff = None
            self._boot_manager.set_details("", "", "", "", "", None)
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target")) if self._simulator_targets else None
        self._boot_manager.set_targets(
            self._current_boot_target_mode,
            bool(profile.get("device_mount_path")),
            bool(sim_target or profile.get("simulator_simdisk_path")),
        )
        self._set_boot_details(profile, self._boot_manager.current_target_mode())

    def _set_boot_details(self, profile, target_mode):
        self._current_boot_target_mode = target_mode or "device"
        spec = self._rockbox_boot.profile_spec(profile)
        image_path = self._rockbox_boot.default_image_for_profile(profile)
        validation = self._rockbox_boot.validate_image(image_path, profile)
        preview_path = ""
        if validation["valid"]:
            preview = self._rockbox_boot.generate_preview(
                image_path,
                profile,
                os.path.join(self._config.cache_dir, "boot_previews"),
            )
            preview_path = preview.get("preview_path", "")
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        destination_rel = "rockbox.ipod"
        diff = None
        if validation["valid"]:
            try:
                if self._current_boot_target_mode == "device":
                    bundle = self._rockbox_boot.build_source_bundle(
                        profile,
                        image_path,
                        os.path.join(self._config.cache_dir, "boot_staging"),
                    )
                    diff = self._rockbox_deploy.build_diff(self._rockbox_boot.source_profile(profile), bundle)
                    destination_rel = "repo boot assets -> make fullinstall"
                else:
                    deploy_profile = self._rockbox_boot.deploy_profile(profile, self._current_boot_target_mode, sim_target)
                    destination_rel = f".rockbox/rockpod/boot/branding/{profile['screen_resolution']}/boot-logo.bmp"
                    bundle = self._rockbox_boot.build_bundle(
                        profile,
                        image_path,
                        os.path.join(self._config.cache_dir, "boot_staging"),
                    )
                    diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
            except (ValueError, OSError):
                diff = None
        status_message = validation["message"]
        self._current_boot_diff = diff
        self._boot_manager.set_details(
            image_path,
            f"{spec['width']}x{spec['height']}",
            destination_rel,
            status_message,
            preview_path,
            diff["summary"] if diff else None,
        )

    def _on_boot_profile_selected(self, profile_id):
        self._rockbox_profiles.set_selected_profile(profile_id)
        self._refresh_theme_hub()
        self._refresh_boot_manager()
        self._refresh_simulator_panel()

    def _on_boot_target_mode_selected(self, target_mode):
        profile = self._rockbox_profiles.current_profile()
        if profile:
            self._set_boot_details(profile, target_mode)

    def _choose_boot_image(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        path, _filter = QFileDialog.getOpenFileName(
            self,
            "Choose Boot Image",
            profile.get("source_repo_path", self._repo_root),
            "Images (*.bmp *.png *.jpg *.jpeg);;All Files (*)",
        )
        if not path:
            return
        profile["boot_image_path"] = path
        self._rockbox_profiles.save_profile(profile)
        self._refresh_boot_manager()

    def _dry_run_boot_deploy(self):
        if not self._current_boot_diff:
            self._status_bar.set_left_text("No boot branding diff available")
            return
        summary = self._current_boot_diff["summary"]
        QMessageBox.information(
            self,
            "Boot Branding Dry Run",
            "\n".join(
                [
                    f"Add: {summary['add']}",
                    f"Overwrite: {summary['overwrite']}",
                    f"Unchanged: {summary['unchanged']}",
                    f"Missing: {summary['missing_source']}",
                ]
            ),
        )

    def _show_boot_progress(self, title, label, total=4):
        return BootProgressController(self, self._status_bar).show(title, label, total=total)

    def _update_boot_progress(self, progress, current, total, label, busy=False):
        BootProgressController(self, self._status_bar).update(progress, current, total, label, busy=busy)

    def _close_boot_progress(self, progress):
        BootProgressController(self, self._status_bar).close(progress)

    def _full_install_runtime_firmware(self, profile, progress=None):
        def on_progress(current, total, label):
            self._update_boot_progress(progress, current, total, label, busy=False)

        result = self._rockbox_boot.full_install_firmware(
            profile,
            progress_callback=on_progress,
        )
        if not result["success"]:
            details = "\n".join(
                part
                for part in (
                    result.get("stderr", ""),
                    result.get("stdout", ""),
                    result.get("message", ""),
                )
                if str(part).strip()
            )
            result["message"] = details[:4000] if details else result.get("message", "Rockbox full install failed")
            return result
        self._update_boot_progress(progress, 4, 4, "Full install complete")
        return result

    def _apply_boot_deploy(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile or not self._current_boot_diff:
            self._status_bar.set_left_text("No boot branding deploy available")
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        if self._current_boot_target_mode == "device":
            progress = self._show_boot_progress(
                "Apply Boot Branding",
                "Updating local Rockbox boot image sources...",
            )
            try:
                self._update_boot_progress(progress, 1, 4, "Updating local Rockbox boot image sources...")
                source_result = self._rockbox_deploy.apply_diff(
                    self._rockbox_boot.source_profile(profile),
                    self._current_boot_diff,
                )
                if not source_result["success"]:
                    self._status_bar.set_left_text("Boot branding source update failed")
                    QMessageBox.warning(self, "Boot Branding Failed", "\n".join(source_result["failures"]))
                    self._refresh_boot_manager()
                    return
                self._update_boot_progress(progress, 2, 4, "Invalidating generated boot image outputs...")
                self._rockbox_boot.invalidate_firmware_boot_assets(profile)
                firmware_result = self._full_install_runtime_firmware(profile, progress)
                if not firmware_result["success"]:
                    self._status_bar.set_left_text("Boot branding full install failed")
                    QMessageBox.warning(self, "Boot Branding Failed", firmware_result["message"])
                    self._refresh_boot_manager()
                    return
            finally:
                self._close_boot_progress(progress)

            source_note = ""
            if source_result["copied_count"] == 0:
                source_note = "No boot source files changed; the rendered boot assets already matched.\n"

            self._status_bar.set_left_text("Boot branding rebuilt and full installed")
            QMessageBox.information(
                self,
                "Boot Branding Updated",
                (
                    f"{source_note}"
                    f"Updated {source_result['copied_count']} boot source files\n"
                    f"Built {os.path.basename(firmware_result.get('artifact_path') or 'rockbox.ipod')}\n"
                    f"Ran fullinstall to {profile['device_mount_path']}"
                ),
            )
            self._refresh_boot_manager()
            return
        deploy_profile = self._rockbox_boot.deploy_profile(profile, self._current_boot_target_mode, sim_target)
        result = self._rockbox_deploy.apply_diff(deploy_profile, self._current_boot_diff)
        if result["success"]:
            self._status_bar.set_left_text(f"Boot branding updated: {result['copied_count']} files")
            QMessageBox.information(
                self,
                "Boot Branding Updated",
                f"Copied {result['copied_count']} files\nBackup: {result['backup_dir']}",
            )
        else:
            self._status_bar.set_left_text("Boot branding deploy failed")
            QMessageBox.warning(self, "Boot Branding Failed", "\n".join(result["failures"]))
        self._refresh_boot_manager()

    def _restore_boot_backup(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        if self._current_boot_target_mode == "device":
            result = self._rockbox_deploy.restore_latest_backup(self._rockbox_boot.source_profile(profile))
            if result["success"]:
                progress = self._show_boot_progress(
                    "Restore Boot Branding",
                    "Invalidating generated boot image outputs...",
                    total=3,
                )
                try:
                    self._update_boot_progress(progress, 1, 3, "Invalidating generated boot image outputs...")
                    self._rockbox_boot.invalidate_firmware_boot_assets(profile)
                    firmware_result = self._full_install_runtime_firmware(profile, progress)
                    if not firmware_result["success"]:
                        self._status_bar.set_left_text("Boot branding restore full install failed")
                        QMessageBox.warning(self, "Restore Failed", firmware_result["message"])
                        self._refresh_boot_manager()
                        return
                finally:
                    self._close_boot_progress(progress)
                self._status_bar.set_left_text("Restored boot branding and full installed Rockbox")
                QMessageBox.information(
                    self,
                    "Boot Branding Restored",
                    f"Restored {result['restored_count']} source files and ran fullinstall",
                )
            else:
                self._status_bar.set_left_text("Boot branding restore failed")
                QMessageBox.warning(self, "Restore Failed", "\n".join(result["failures"]))
            self._refresh_boot_manager()
            return
        deploy_profile = self._rockbox_boot.deploy_profile(profile, self._current_boot_target_mode, sim_target)
        result = self._rockbox_deploy.restore_latest_backup(deploy_profile)
        if result["success"]:
            self._status_bar.set_left_text(f"Restored {result['restored_count']} boot branding files")
            QMessageBox.information(
                self,
                "Boot Branding Restored",
                f"Restored {result['restored_count']} files from:\n{result['backup_dir']}",
            )
        else:
            self._status_bar.set_left_text("Boot branding restore failed")
            QMessageBox.warning(self, "Restore Failed", "\n".join(result["failures"]))
        self._refresh_boot_manager()

    # ═══════════════════════════════════════════════════════════════
    # Rockbox photos
    # ═══════════════════════════════════════════════════════════════

    def _refresh_photo_manager(self):
        if not self._simulator_targets:
            self._simulator_targets = self._rockbox_simulator.discover_targets(self._repo_root)
        profiles = self._rockbox_profiles.profiles()
        selected_id = self._rockbox_profiles.selected_profile_id()
        self._photo_manager.set_profiles(profiles, selected_id)
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._current_photo_diff = None
            self._photo_manager.set_photos([])
            self._photo_manager.set_selection_details([])
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target")) if self._simulator_targets else None
        self._photo_manager.set_targets(
            self._current_photo_target_mode,
            bool(profile.get("device_mount_path")),
            bool(sim_target or profile.get("simulator_simdisk_path")),
        )
        target_mode = self._photo_manager.current_target_mode()
        target_root = self._rockbox_photos.target_root(profile, target_mode, sim_target)
        photos = self._rockbox_photos.list_photos(
            profile,
            sim_target,
            include_hidden=self._show_hidden_wallpapers,
        )
        selected_ids = [photo["id"] for photo in self._photo_manager.selected_photos()]
        self._photo_manager.set_library_state(
            profile.get("photos_library_path", ""),
            target_root,
            f"{len(photos)} photos indexed",
        )
        self._photo_manager.set_photos(photos, selected_ids)
        self._on_photo_selection_changed()

    def _on_photo_profile_selected(self, profile_id):
        self._rockbox_profiles.set_selected_profile(profile_id)
        self._refresh_theme_hub()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_photo_manager()
        self._refresh_simulator_panel()
        self._refresh_browser_panel()

    def _on_photo_target_mode_selected(self, target_mode):
        self._current_photo_target_mode = target_mode or "device"
        self._refresh_photo_manager()

    def _choose_photo_library(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        path = QFileDialog.getExistingDirectory(
            self,
            "Choose Photo Folder",
            profile.get("photos_library_path") or self._config.get("photos_library_path", ""),
        )
        if not path:
            return
        profile["photos_library_path"] = path
        self._rockbox_profiles.save_profile(profile)
        self._refresh_photo_manager()

    def _on_photo_selection_changed(self):
        profile = self._rockbox_profiles.current_profile()
        photos = self._photo_manager.selected_photos()
        if not profile or not photos:
            self._current_photo_diff = None
            self._photo_manager.set_selection_details([], None)
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        deploy_profile = self._rockbox_photos.deploy_profile(profile, self._current_photo_target_mode, sim_target)
        bundle = self._rockbox_photos.build_sync_bundle(profile, photos, self._current_photo_target_mode)
        try:
            diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
        except ValueError:
            diff = None
        self._current_photo_diff = diff
        self._photo_manager.set_selection_details(photos, diff["summary"] if diff else None)

    def _dry_run_photo_sync(self):
        if not self._current_photo_diff:
            self._status_bar.set_left_text("No photo sync diff available")
            return
        summary = self._current_photo_diff["summary"]
        QMessageBox.information(
            self,
            "Photo Sync Dry Run",
            "\n".join(
                [
                    f"Add: {summary['add']}",
                    f"Overwrite: {summary['overwrite']}",
                    f"Remove: {summary.get('remove', 0)}",
                    f"Unchanged: {summary['unchanged']}",
                    f"Missing: {summary['missing_source']}",
                ]
            ),
        )

    def _sync_selected_photos(self):
        profile = self._rockbox_profiles.current_profile()
        photos = self._photo_manager.selected_photos()
        if not profile or not photos or not self._current_photo_diff:
            self._status_bar.set_left_text("No selected photos to sync")
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        deploy_profile = self._rockbox_photos.deploy_profile(profile, self._current_photo_target_mode, sim_target)
        result = self._rockbox_deploy.apply_diff(deploy_profile, self._current_photo_diff)
        if result["success"]:
            self._status_bar.set_left_text(f"Photos synced: {result['copied_count']} files")
            QMessageBox.information(
                self,
                "Photos Synced",
                f"Copied {result['copied_count']} files\nBackup: {result['backup_dir']}",
            )
        else:
            self._status_bar.set_left_text("Photo sync failed")
            QMessageBox.warning(self, "Photo Sync Failed", "\n".join(result["failures"]))
        self._refresh_photo_manager()

    def _hide_selected_photos(self):
        profile = self._rockbox_profiles.current_profile()
        photos = self._photo_manager.selected_photos()
        if not profile or not photos:
            self._status_bar.set_left_text("No selected photos to hide")
            return
        hidden = not all(photo.get("hidden") for photo in photos)
        changed = self._rockbox_photos.set_hidden(profile, photos, hidden)
        self._refresh_photo_manager()
        if changed:
            action = "Hidden" if hidden else "Unhidden"
            self._status_bar.set_left_text(f"{action} {changed} photo(s)")

    def _remove_selected_photos(self):
        profile = self._rockbox_profiles.current_profile()
        photos = self._photo_manager.selected_photos()
        if not profile or not photos:
            self._status_bar.set_left_text("No selected photos to remove")
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        deploy_profile = self._rockbox_photos.deploy_profile(profile, self._current_photo_target_mode, sim_target)
        bundle = self._rockbox_photos.build_remove_bundle(profile, photos, self._current_photo_target_mode)
        diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
        result = self._rockbox_deploy.apply_diff(deploy_profile, diff)
        if result["success"]:
            self._status_bar.set_left_text(f"Photos removed: {result['copied_count']} files")
            QMessageBox.information(
                self,
                "Photos Removed",
                f"Removed {result['copied_count']} photo file(s)\nBackup: {result['backup_dir']}",
            )
        else:
            self._status_bar.set_left_text("Photo remove failed")
            QMessageBox.warning(self, "Photo Remove Failed", "\n".join(result["failures"]))
        self._refresh_photo_manager()

    # ═══════════════════════════════════════════════════════════════
    # Rockboy games
    # ═══════════════════════════════════════════════════════════════

    def _refresh_game_manager(self):
        if not self._simulator_targets:
            self._simulator_targets = self._rockbox_simulator.discover_targets(self._repo_root)
        profiles = self._rockbox_profiles.profiles()
        selected_id = self._rockbox_profiles.selected_profile_id()
        self._game_manager.set_profiles(profiles, selected_id)
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._current_game_diff = None
            self._game_manager.set_games([])
            self._game_manager.set_selection_details([])
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target")) if self._simulator_targets else None
        self._game_manager.set_targets(
            self._current_game_target_mode,
            bool(profile.get("device_mount_path")),
            bool(sim_target or profile.get("simulator_simdisk_path")),
        )
        target_root = self._rockbox_games.target_root(profile, self._game_manager.current_target_mode(), sim_target)
        games = self._rockbox_games.list_games(profile, sim_target, config=self._config)
        latest_backup = self._rockbox_games.latest_save_backup_dir(profile, self._game_manager.current_target_mode())
        latest_backup_text = os.path.basename(latest_backup) if latest_backup else "None"
        self._game_manager.set_library_state(
            profile.get("games_library_path", ""),
            target_root,
            f"{len(games)} ROMs indexed",
            self._rockbox_games.settings_guidance(profile),
            latest_backup_text,
        )
        selected_ids = [game["id"] for game in self._game_manager.selected_games()]
        self._game_manager.set_games(games, selected_ids)
        self._on_game_selection_changed()

    def _on_game_profile_selected(self, profile_id):
        self._rockbox_profiles.set_selected_profile(profile_id)
        self._refresh_theme_hub()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_simulator_panel()
        self._refresh_game_browser_panel()

    def _on_game_target_mode_selected(self, target_mode):
        self._current_game_target_mode = target_mode or "device"
        self._refresh_game_manager()

    def _choose_game_library(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        path = QFileDialog.getExistingDirectory(
            self,
            "Choose Rockboy ROM Folder",
            profile.get("games_library_path") or self._config.get("games_library_path", ""),
        )
        if not path:
            return
        profile["games_library_path"] = path
        self._rockbox_profiles.save_profile(profile)
        self._refresh_game_manager()
        self._refresh_game_browser_panel()

    def _on_game_selection_changed(self):
        profile = self._rockbox_profiles.current_profile()
        games = self._game_manager.selected_games()
        if not profile or not games:
            self._current_game_diff = None
            self._game_manager.set_selection_details([], None)
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        deploy_profile = self._rockbox_games.deploy_profile(profile, self._current_game_target_mode, sim_target)
        bundle = self._rockbox_games.build_sync_bundle(profile, games, self._current_game_target_mode, sim_target)
        try:
            diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
        except ValueError:
            diff = None
        self._current_game_diff = diff
        self._game_manager.set_selection_details(games, diff["summary"] if diff else None)

    def _dry_run_game_sync(self):
        if not self._current_game_diff:
            self._status_bar.set_left_text("No game sync diff available")
            return
        summary = self._current_game_diff["summary"]
        QMessageBox.information(
            self,
            "Game Sync Dry Run",
            "\n".join(
                [
                    f"Add: {summary['add']}",
                    f"Overwrite: {summary['overwrite']}",
                    f"Remove: {summary.get('remove', 0)}",
                    f"Unchanged: {summary['unchanged']}",
                    f"Missing: {summary['missing_source']}",
                ]
            ),
        )

    def _sync_selected_games(self):
        profile = self._rockbox_profiles.current_profile()
        games = self._game_manager.selected_games()
        if not profile or not games or not self._current_game_diff:
            self._status_bar.set_left_text("No selected games to sync")
            return
        warnings = [game for game in games if game.get("performance", {}).get("level") in {"warn", "critical"}]
        if warnings:
            warning_lines = [f"{game['filename']}: {game['performance']['notes'][0]}" for game in warnings if game["performance"]["notes"]]
            QMessageBox.information(
                self,
                "Rockboy Performance Warning",
                "\n".join(warning_lines) or "Some selected ROMs are large for the target hardware.",
            )
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        self._rockbox_games.backup_saves(profile, games, self._current_game_target_mode, sim_target)
        deploy_profile = self._rockbox_games.deploy_profile(profile, self._current_game_target_mode, sim_target)
        result = self._rockbox_deploy.apply_diff(deploy_profile, self._current_game_diff)
        if result["success"]:
            self._status_bar.set_left_text(f"Games synced: {result['copied_count']} files")
            QMessageBox.information(
                self,
                "Games Synced",
                f"Copied {result['copied_count']} files\nBackup: {result['backup_dir']}",
            )
        else:
            self._status_bar.set_left_text("Game sync failed")
            QMessageBox.warning(self, "Game Sync Failed", "\n".join(result["failures"]))
        self._refresh_game_manager()

    def _remove_selected_games(self):
        profile = self._rockbox_profiles.current_profile()
        games = self._game_manager.selected_games()
        if not profile or not games:
            self._status_bar.set_left_text("No selected games to remove")
            return
        if any(game["device_save_exists"] or game["simulator_save_exists"] for game in games):
            QMessageBox.information(
                self,
                "Save Files Preserved",
                "Remove Selected will only remove ROM files. Existing .sav files are left in place.",
            )
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        self._rockbox_games.backup_saves(profile, games, self._current_game_target_mode, sim_target)
        deploy_profile = self._rockbox_games.deploy_profile(profile, self._current_game_target_mode, sim_target)
        bundle = self._rockbox_games.build_remove_bundle(profile, games, self._current_game_target_mode, sim_target)
        diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
        result = self._rockbox_deploy.apply_diff(deploy_profile, diff)
        if result["success"]:
            self._status_bar.set_left_text(f"Games removed: {result['copied_count']} files")
            QMessageBox.information(
                self,
                "Games Removed",
                f"Removed {result['copied_count']} ROM files\nBackup: {result['backup_dir']}",
            )
        else:
            self._status_bar.set_left_text("Game remove failed")
            QMessageBox.warning(self, "Game Remove Failed", "\n".join(result["failures"]))
        self._refresh_game_manager()

    def _backup_game_saves(self):
        profile = self._rockbox_profiles.current_profile()
        games = self._game_manager.selected_games()
        if not profile or not games:
            self._status_bar.set_left_text("No selected games")
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        result = self._rockbox_games.backup_saves(profile, games, self._current_game_target_mode, sim_target)
        self._status_bar.set_left_text(result["message"])
        QMessageBox.information(
            self,
            "Save Backup",
            f"{result['message']}\nBackup: {result['backup_dir']}",
        )
        self._refresh_game_manager()

    def _restore_game_saves(self):
        profile = self._rockbox_profiles.current_profile()
        games = self._game_manager.selected_games()
        if not profile or not games:
            self._status_bar.set_left_text("No selected games")
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        result = self._rockbox_games.restore_saves(profile, games, self._current_game_target_mode, sim_target)
        self._status_bar.set_left_text(result["message"])
        QMessageBox.information(self, "Save Restore", f"{result['message']}\nBackup: {result.get('backup_dir', '')}")
        self._refresh_game_manager()

    def _export_game_saves(self):
        profile = self._rockbox_profiles.current_profile()
        games = self._game_manager.selected_games()
        if not profile or not games:
            self._status_bar.set_left_text("No selected games")
            return
        default_name = f"{profile['id']}-rockboy-saves.zip"
        archive_path, _ = QFileDialog.getSaveFileName(
            self,
            "Export Rockboy Saves",
            os.path.join(profile.get("source_repo_path", self._repo_root), default_name),
            "ZIP Archives (*.zip)",
        )
        if not archive_path:
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        result = self._rockbox_games.export_save_bundle(profile, games, self._current_game_target_mode, sim_target, archive_path)
        self._status_bar.set_left_text(result["message"])
        QMessageBox.information(self, "Save Export", f"{result['message']}\nBundle: {result['archive_path']}")
        self._refresh_game_manager()

    def _import_game_saves(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._status_bar.set_left_text("No active profile")
            return
        archive_path, _ = QFileDialog.getOpenFileName(
            self,
            "Import Rockboy Save Bundle",
            profile.get("source_repo_path", self._repo_root),
            "ZIP Archives (*.zip)",
        )
        if not archive_path:
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        result = self._rockbox_games.import_save_bundle(profile, self._current_game_target_mode, sim_target, archive_path)
        self._status_bar.set_left_text(result["message"])
        QMessageBox.information(self, "Save Import", result["message"])
        self._refresh_game_manager()

    def _fetch_selected_game_cover(self):
        profile = self._rockbox_profiles.current_profile()
        games = self._game_manager.selected_games()
        if not profile or not games:
            self._status_bar.set_left_text("No selected game")
            return
        if len(games) > 1:
            self._status_bar.set_left_text("Select one game to fetch cover art")
            return
        result = self._rockbox_games.fetch_cover_for_game(games[0], self._config)
        self._status_bar.set_left_text(result["message"])
        if result["success"]:
            self._refresh_game_manager()

    def _fetch_selected_game_metadata(self):
        profile = self._rockbox_profiles.current_profile()
        games = self._game_manager.selected_games()
        if not profile or not games:
            self._status_bar.set_left_text("No selected game")
            return
        if len(games) > 1:
            self._status_bar.set_left_text("Select one game to fetch metadata")
            return
        result = self._rockbox_games.fetch_metadata_for_game(profile, games[0])
        self._status_bar.set_left_text(result["message"])
        if result["success"]:
            sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
            deploy_profile = self._rockbox_games.deploy_profile(profile, self._current_game_target_mode, sim_target)
            try:
                index_bundle = self._rockbox_games.build_index_bundle(
                    profile,
                    self._current_game_target_mode,
                    sim_target,
                    games,
                    include_missing=False,
                )
                if index_bundle["assets"]:
                    diff = self._rockbox_deploy.build_diff(deploy_profile, index_bundle)
                    if diff["summary"]["add"] or diff["summary"]["overwrite"]:
                        self._rockbox_deploy.apply_diff(deploy_profile, diff)
            except ValueError:
                pass
            self._refresh_game_manager()

    def _optimize_selected_game_cover(self):
        games = self._game_manager.selected_games()
        if len(games) != 1:
            self._status_bar.set_left_text("Select one game to optimize cover art")
            return
        result = self._rockbox_games.optimize_cover_asset(games[0])
        self._status_bar.set_left_text(result["message"])
        if result["success"]:
            self._refresh_game_manager()

    def _launch_selected_game_in_simulator(self):
        profile = self._rockbox_profiles.current_profile()
        games = self._game_manager.selected_games()
        if not profile or len(games) != 1:
            self._status_bar.set_left_text("Select one game to launch in simulator")
            return
        target = self._simulator_target_by_id(profile.get("simulator_target"))
        if not target:
            self._status_bar.set_left_text("No bound simulator target")
            return
        sim_target = target
        deploy_profile = self._rockbox_games.deploy_profile(profile, "simulator", sim_target)
        bundle = self._rockbox_games.build_sync_bundle(profile, [games[0]], "simulator", sim_target)
        diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
        if diff["summary"]["add"] or diff["summary"]["overwrite"]:
            self._rockbox_games.backup_saves(profile, [games[0]], "simulator", sim_target)
            self._rockbox_deploy.apply_diff(deploy_profile, diff)
        rom_path = os.path.join(self._rockbox_games.rom_target_root(profile, "simulator", sim_target), games[0]["filename"])
        launched = self._rockbox_simulator.launch_with_rom(sim_target, rom_path)
        if launched.get("autoload_supported"):
            self._status_bar.set_left_text(f"Launched {games[0]['filename']} in simulator")
        else:
            self._status_bar.set_left_text("Simulator launched; direct ROM autoload is not supported by this build")

    # ═══════════════════════════════════════════════════════════════
    # Rockbox plugins
    # ═══════════════════════════════════════════════════════════════

    def _refresh_plugin_manager(self):
        if not self._simulator_targets:
            self._simulator_targets = self._rockbox_simulator.discover_targets(self._repo_root)
        profiles = self._rockbox_profiles.profiles()
        selected_id = self._rockbox_profiles.selected_profile_id()
        self._plugin_manager.set_profiles(profiles, selected_id)
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            self._current_plugin_diff = None
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target")) if self._simulator_targets else None
        self._plugin_manager.set_targets(
            self._current_plugin_target_mode,
            bool(profile.get("device_mount_path")),
            bool(sim_target or profile.get("simulator_simdisk_path")),
        )
        plugins = self._rockbox_plugins.list_plugins(
            self._repo_root,
            profile,
            sim_target,
            self._plugin_manager.current_target_mode(),
        )
        if not plugins:
            self._current_plugin_diff = None
            self._current_plugin_id = ""
            self._plugin_manager.set_plugins([], "")
            return
        selected_plugin_id = self._current_plugin_id or plugins[0]["id"]
        if not any(item["id"] == selected_plugin_id for item in plugins):
            selected_plugin_id = plugins[0]["id"]
        self._plugin_manager.set_plugins(plugins, selected_plugin_id)
        self._set_plugin_details(profile, selected_plugin_id, self._plugin_manager.current_target_mode())

    def _set_plugin_details(self, profile, plugin_id, target_mode):
        self._current_plugin_target_mode = target_mode or "device"
        self._current_plugin_id = plugin_id
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        details = self._rockbox_plugins.plugin_details(
            self._repo_root,
            plugin_id,
            profile,
            sim_target,
            self._current_plugin_target_mode,
        )
        diff = None
        if details["binary_exists"]:
            deploy_profile = self._rockbox_plugins.deploy_profile(profile, self._current_plugin_target_mode, sim_target)
            bundle = self._rockbox_plugins.build_deploy_bundle(details)
            try:
                diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
            except ValueError:
                diff = None
        self._current_plugin_diff = diff
        self._plugin_manager.set_plugin_details(details, diff["summary"] if diff else None)

    def _on_plugin_profile_selected(self, profile_id):
        self._rockbox_profiles.set_selected_profile(profile_id)
        self._refresh_theme_hub()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_simulator_panel()

    def _on_plugin_target_mode_selected(self, target_mode):
        profile = self._rockbox_profiles.current_profile()
        if profile and self._current_plugin_id:
            self._set_plugin_details(profile, self._current_plugin_id, target_mode)
        else:
            self._refresh_plugin_manager()

    def _on_plugin_selected(self, plugin_id):
        profile = self._rockbox_profiles.current_profile()
        if profile:
            self._set_plugin_details(profile, plugin_id, self._plugin_manager.current_target_mode())

    def _dry_run_plugin_deploy(self):
        if not self._current_plugin_diff:
            self._status_bar.set_left_text("No plugin deploy available")
            return
        summary = self._current_plugin_diff["summary"]
        QMessageBox.information(
            self,
            "Plugin Dry Run",
            "\n".join(
                [
                    f"Add: {summary['add']}",
                    f"Overwrite: {summary['overwrite']}",
                    f"Remove: {summary.get('remove', 0)}",
                    f"Unchanged: {summary['unchanged']}",
                    f"Missing: {summary['missing_source']}",
                ]
            ),
        )

    def _apply_plugin_deploy(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile or not self._current_plugin_id or not self._current_plugin_diff:
            self._status_bar.set_left_text("No plugin deploy available")
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        deploy_profile = self._rockbox_plugins.deploy_profile(profile, self._current_plugin_target_mode, sim_target)
        result = self._rockbox_deploy.apply_diff(deploy_profile, self._current_plugin_diff)
        if result["success"]:
            self._status_bar.set_left_text(f"Plugin deployed: {result['copied_count']} files")
            QMessageBox.information(
                self,
                "Plugin Deployed",
                f"Copied {result['copied_count']} files\nBackup: {result['backup_dir']}",
            )
        else:
            self._status_bar.set_left_text("Plugin deploy failed")
            QMessageBox.warning(self, "Plugin Deploy Failed", "\n".join(result["failures"]))
        self._refresh_plugin_manager()

    def _remove_plugin(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile or not self._current_plugin_id:
            self._status_bar.set_left_text("No plugin selected")
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
        details = self._rockbox_plugins.plugin_details(
            self._repo_root,
            self._current_plugin_id,
            profile,
            sim_target,
            self._current_plugin_target_mode,
        )
        deploy_profile = self._rockbox_plugins.deploy_profile(profile, self._current_plugin_target_mode, sim_target)
        diff = self._rockbox_deploy.build_diff(deploy_profile, self._rockbox_plugins.build_remove_bundle(details))
        result = self._rockbox_deploy.apply_diff(deploy_profile, diff)
        if result["success"]:
            self._status_bar.set_left_text(f"Plugin removed: {result['copied_count']} files")
            QMessageBox.information(
                self,
                "Plugin Removed",
                f"Removed {result['copied_count']} files\nBackup: {result['backup_dir']}",
            )
        else:
            self._status_bar.set_left_text("Plugin remove failed")
            QMessageBox.warning(self, "Plugin Remove Failed", "\n".join(result["failures"]))
        self._refresh_plugin_manager()

    # ═══════════════════════════════════════════════════════════════
    # RockPod Linux
    # ═══════════════════════════════════════════════════════════════

    def _linux_cache_dir(self):
        if self._linux_cache_dir_override:
            return self._linux_cache_dir_override
        cache_dir = self._config.get("cache_dir", "")
        if not cache_dir:
            cache_dir = os.path.join(self._repo_root, "rockpod", ".cache")
        return os.path.join(cache_dir, "linux")

    def _linux_cache_dir_candidates(self):
        cache_dirs = []
        seen = set()

        def add(path):
            if not path:
                return
            resolved = os.path.abspath(os.path.expanduser(path))
            if resolved in seen:
                return
            seen.add(resolved)
            cache_dirs.append(resolved)

        if self._linux_cache_dir_override:
            add(self._linux_cache_dir_override)
        config_cache = self._config.get("cache_dir", "")
        if not config_cache:
            config_cache = os.path.join(self._repo_root, "rockpod", ".cache")
        add(os.path.join(config_cache, "linux"))
        add(os.path.join(os.path.expanduser("~/.rockpod"), "cache", "linux"))
        return cache_dirs

    def _find_verified_linux_cache_dir(self):
        cache_dirs = self._linux_cache_dir_candidates()
        for cache_dir in cache_dirs:
            iso_path = os.path.join(cache_dir, DEBIAN_LIVE_XFCE_ISO)
            try:
                if self._linux_payload.verify_iso(iso_path, require_cached=True).get("success"):
                    return cache_dir
            except Exception:
                continue
        return None

    def _linux_iso_path(self):
        return os.path.join(self._linux_cache_dir(), DEBIAN_LIVE_XFCE_ISO)

    def _linux_extracted_path(self):
        return os.path.join(
            self._linux_cache_dir(),
            DEBIAN_LIVE_XFCE_ISO.replace(".iso", "-vm-stage"),
        )

    def _refresh_linux_manager(self):
        device = self._device_detector.current_device
        iso_status = self._linux_payload.verify_iso(self._linux_iso_path(), require_cached=True)
        extracted = self._linux_extracted_path()
        stage_ready = (
            os.path.isfile(os.path.join(extracted, "manifest.json"))
            and os.path.isfile(os.path.join(extracted, "start-linux-linux.sh"))
        )
        payload_bytes = 0
        if iso_status["success"]:
            try:
                payload_bytes = os.path.getsize(self._linux_iso_path())
            except OSError:
                payload_bytes = 0

        install_text = "Install: connect a Rockbox iPod"
        installed = False
        vm_ready = False
        if device and getattr(device, "mount_path", ""):
            try:
                installed_status = self._linux_payload.installed_status(device.mount_path)
                installed = bool(installed_status.get("installed"))
                vm_ready = bool(installed_status.get("vm_ready"))
                if installed:
                    size_mb = int(installed_status.get("size_bytes") or 0) // (1024 * 1024)
                    alloc = int(installed_status.get("allocation_gb") or 0)
                    if vm_ready:
                        install_text = f"Install: ready ({size_mb} MB, cap {alloc} GB)"
                    else:
                        install_text = f"Install: VM copied; Debian install pending ({size_mb} MB, cap {alloc} GB)"
                    payload_bytes = int(installed_status.get("size_bytes") or payload_bytes)
                else:
                    install_text = "Install: not installed"
            except ValueError:
                install_text = "Install: invalid device root"

        self._linux_manager.set_status(
            {
                "device": {
                    "name": getattr(device, "name", ""),
                    "mount_path": getattr(device, "mount_path", ""),
                } if device else {},
                "iso": "Debian ISO: verified" if iso_status["success"] else "Debian ISO: missing or invalid",
                "stage": "Payload: staged (cache)" if stage_ready else "Payload: not staged",
                "install": install_text,
                "free_bytes": int(getattr(device, "free_space", 0) or 0) if device else 0,
                "payload_bytes": payload_bytes,
                "download_enabled": True,
                "stage_enabled": iso_status["success"],
                "install_enabled": bool(device and (not installed or not vm_ready)),
                "provision_enabled": bool(device and installed),
                "start_enabled": bool(device and installed and vm_ready),
                "uninstall_enabled": bool(device and installed),
            }
        )

    def _download_linux_iso(self):
        progress = QProgressDialog("Downloading Debian Live Xfce...", None, 0, 100, self)
        progress.setWindowTitle("RockPod Linux")
        progress.setWindowModality(Qt.WindowModal)
        progress.setCancelButton(None)
        progress.setMinimumDuration(0)
        progress.show()
        progress.raise_()
        progress.activateWindow()
        QApplication.processEvents()

        verified_cache_dir = self._find_verified_linux_cache_dir()
        if verified_cache_dir:
            self._linux_cache_dir_override = verified_cache_dir
            progress.setLabelText(
                f"Using verified Debian ISO from existing cache:\n{verified_cache_dir}"
            )
            QApplication.processEvents()
            progress.close()
            self._status_bar.set_left_text("Debian ISO already present and verified")
            return self._linux_payload.verify_iso(self._linux_iso_path())

        def on_progress(copied, total):
            if total:
                progress.setValue(min(100, int((copied / float(total)) * 100)))
            progress.setLabelText(f"Downloading Debian Live Xfce... {copied // (1024 * 1024)} MB")
            QApplication.processEvents()

        self._linux_cache_dir_override = None
        cache_dirs = self._linux_cache_dir_candidates()
        if not cache_dirs:
            cache_dirs = [self._linux_cache_dir()]

        last_error = None
        result = None
        try:
            for index, cache_dir in enumerate(cache_dirs):
                self._linux_cache_dir_override = cache_dir
                progress.setLabelText(
                    f"Downloading Debian Live Xfce...\nCache: {cache_dir}"
                )
                QApplication.processEvents()
                if index > 0:
                    progress.setLabelText(f"Retrying Debian ISO download to alternate cache: {cache_dir}")
                    QApplication.processEvents()
                try:
                    result = self._linux_payload.download_iso(cache_dir, on_progress)
                    break
                except OSError as exc:
                    last_error = exc
                    if exc.errno == errno.ENOSPC and index + 1 < len(cache_dirs):
                        continue
                    raise
            if result is None:
                if last_error is not None:
                    raise last_error
                raise RuntimeError("Unable to write Debian ISO to cache.")
        except Exception as exc:
            progress.close()
            if isinstance(exc, OSError) and exc.errno == errno.ENOSPC:
                candidates = "\n".join(
                    [f"- {os.path.abspath(os.path.expanduser(c))}" for c in cache_dirs]
                )
                QMessageBox.warning(
                    self,
                    "RockPod Linux",
                    "Host cache does not have enough space to download the ISO.\n"
                    f"Error: {exc}\n\n"
                    "Current cache candidate list:\n"
                    f"{candidates}",
                )
            else:
                QMessageBox.warning(self, "RockPod Linux", f"Download failed: {exc}")
            self._refresh_linux_manager()
            return
        progress.close()
        if result.get("success"):
            self._status_bar.set_left_text("Debian Live ISO is ready")
        else:
            QMessageBox.warning(self, "RockPod Linux", result.get("message", "Download verification failed"))
        self._refresh_linux_manager()

    def _stage_linux_payload(self):
        verification = self._linux_payload.verify_iso(self._linux_iso_path())
        if not verification["success"]:
            QMessageBox.warning(self, "RockPod Linux", "Download and verify the Debian ISO first.")
            self._refresh_linux_manager()
            return
        credentials = self._linux_manager.prompt_vm_user()
        if credentials is None:
            return
        progress = QProgressDialog("Staging Debian payload locally...", None, 0, 0, self)
        progress.setWindowTitle("RockPod Linux")
        progress.setWindowModality(Qt.WindowModal)
        progress.setCancelButton(None)
        progress.setMinimumDuration(0)
        progress.setAutoClose(False)
        progress.setAutoReset(False)
        progress.setLabelText("Preparing and extracting VM payload to cache (host only)...")
        progress.show()
        progress.raise_()
        progress.activateWindow()
        QApplication.processEvents()
        try:
            if os.path.isdir(self._linux_extracted_path()):
                shutil.rmtree(self._linux_extracted_path())
            bundle = self._linux_payload.build_vm_bundle_from_iso(
                self._linux_iso_path(),
                self._linux_extracted_path(),
                self._linux_manager.allocation_gb(),
                credentials,
            )
        except Exception as exc:
            progress.close()
            QMessageBox.warning(self, "RockPod Linux", f"Staging failed: {exc}")
            self._refresh_linux_manager()
            return
        progress.close()
        self._status_bar.set_left_text(
            f"Stage completed in cache; install to iPod via "
            f"'{self._linux_manager._install_btn.text()}' if mounted."
        )
        self._refresh_linux_manager()

    def _install_linux_payload(self, allocation_gb):
        install_btn = self._linux_manager._install_btn
        original_install_text = install_btn.text()
        install_btn.setEnabled(False)
        install_btn.setText("Installing Linux VM...")

        def _safe_progress_units(total_bytes, unit=1024 * 1024, max_range=2_147_483_647):
            if unit <= 0:
                unit = 1
            if total_bytes is None:
                return 1
            try:
                total = int(total_bytes)
            except Exception:
                return 1
            if total <= 0:
                return 1
            return max(min(max(total // unit, 1), max_range), 1)

        self._status_bar.set_left_text("Linux install: validating environment...")
        QApplication.processEvents()
        try:
            self._status_bar.set_left_text("Linux install: locating a valid cached Debian ISO...")
            QApplication.processEvents()

            device = self._device_detector.current_device
            if not device:
                QMessageBox.warning(self, "RockPod Linux", "Connect a Rockbox iPod first.")
                return

            try:
                verified_cache_dir = self._find_verified_linux_cache_dir()
                if verified_cache_dir:
                    self._linux_cache_dir_override = verified_cache_dir
                else:
                    self._linux_cache_dir_override = None
                self._status_bar.set_left_text("Linux install: validating cached Debian ISO checksum...")
                QApplication.processEvents()

                def verify_iso_with_progress():
                    verify_progress = QProgressDialog(
                        "Validating Debian ISO checksum...",
                        None,
                        0,
                        0,
                        self,
                    )
                    verify_progress.setWindowTitle("RockPod Linux")
                    verify_progress.setWindowModality(Qt.WindowModal)
                    verify_progress.setCancelButton(None)
                    verify_progress.setMinimumDuration(0)
                    verify_progress.setAutoClose(False)
                    verify_progress.setAutoReset(False)
                    verify_progress.setValue(0)
                    verify_progress.setLabelText("Validating Debian ISO checksum from cache...")
                    verify_progress.show()
                    verify_progress.raise_()
                    verify_progress.activateWindow()
                    QApplication.processEvents()

                    def on_verify_progress(current, total):
                        maximum = _safe_progress_units(total, unit=1024 * 1024)
                        if total:
                            displayed_current = int(current or 0) // (1024 * 1024)
                            verify_progress.setLabelText(
                                f"Validating Debian ISO checksum: "
                                f"{displayed_current} MB / "
                                f"{maximum} MB"
                            )
                        else:
                            verify_progress.setLabelText(
                                "Validating Debian ISO checksum..."
                                f" {current} bytes"
                            )
                        if verify_progress.maximum() != maximum:
                            verify_progress.setRange(0, maximum)
                        verify_progress.setValue(min(int(displayed_current) if total else 0, maximum))
                        self._status_bar.set_left_text(verify_progress.labelText())
                        QApplication.processEvents()

                    try:
                        verification = self._linux_payload.verify_iso(
                            self._linux_iso_path(),
                            progress_callback=on_verify_progress,
                        )
                    finally:
                        verify_progress.close()
                    return verification

                verification = verify_iso_with_progress()
                if not verification["success"]:
                    self._status_bar.set_left_text("Linux install: downloading Debian ISO...")
                    QApplication.processEvents()
                    self._download_linux_iso()
                    verification = verify_iso_with_progress()
                    if not verification["success"]:
                        raise RuntimeError("Debian ISO is missing or invalid.")
            except Exception as exc:
                details = str(exc).strip()
                if not details:
                    details = exc.__class__.__name__
                self._status_bar.set_left_text("Linux install blocked before credential check.")
                QMessageBox.warning(self, "RockPod Linux", f"Install blocked before credential check: {details}")
                self._refresh_linux_manager()
                return

            credentials = self._linux_manager.prompt_vm_user()
            if credentials is None:
                self._status_bar.set_left_text("Linux install canceled by user.")
                return

            stage_progress = QProgressDialog(
                "Preparing Debian VM payload locally...",
                None,
                0,
                0,
                self,
            )
            stage_progress.setWindowTitle("RockPod Linux")
            stage_progress.setWindowModality(Qt.WindowModal)
            stage_progress.setCancelButton(None)
            stage_progress.setMinimumDuration(0)
            stage_progress.setAutoClose(False)
            stage_progress.setAutoReset(False)
            stage_progress.setLabelText("Building bootable VM payload in cache...")
            stage_progress.show()
            stage_progress.raise_()
            stage_progress.activateWindow()
            QApplication.processEvents()
            try:
                if os.path.isdir(self._linux_extracted_path()):
                    shutil.rmtree(self._linux_extracted_path())
                bundle = self._linux_payload.build_vm_bundle_from_iso(
                    self._linux_iso_path(),
                    self._linux_extracted_path(),
                    allocation_gb,
                    credentials,
                    verify_iso=False,
                )
                allocation = self._linux_payload.validate_allocation(bundle, allocation_gb)
            except Exception as exc:
                details = str(exc).strip()
                if not details:
                    details = exc.__class__.__name__
                stage_progress.close()
                self._status_bar.set_left_text("Linux install blocked before copy.")
                QMessageBox.warning(self, "RockPod Linux", f"Install blocked before copy: {details}")
                self._refresh_linux_manager()
                return
            stage_progress.close()

            if not allocation["valid"]:
                QMessageBox.warning(self, "RockPod Linux", allocation["message"])
                self._status_bar.set_left_text("Linux install blocked by allocation limits.")
                return

            message = (
                f"Install Debian Live Xfce to:\n{device.mount_path}\n\n"
                f"Target: {os.path.join(device.mount_path, 'Linux', 'RockPodVM')}\n\n"
                f"Linux allocation: {allocation['allocation_gb']} GB maximum\n"
                f"Payload size: {allocation['payload_bytes'] // (1024 * 1024)} MB\n\n"
                "RockPod will copy the VM bundle, then launch the unattended Debian "
                "installer script from the iPod. It does not modify the iPod "
                "bootloader, stock boot, Rockbox boot, partitions, or boot sector."
            )
            if QMessageBox.question(
                self,
                "Install RockPod Linux",
                message,
                QMessageBox.Yes | QMessageBox.No,
                QMessageBox.Yes,
            ) != QMessageBox.Yes:
                self._status_bar.set_left_text("Linux install confirmation denied.")
                return

            progress = QProgressDialog(
                "Copying Linux payload to iPod...",
                None,
                0,
                _safe_progress_units(bundle.get("total_size"), unit=1024 * 1024),
                self,
            )
            cancel_state = {"cancelled": False}
            progress.setWindowTitle("RockPod Linux")
            progress.setWindowModality(Qt.WindowModal)
            progress.setCancelButtonText("Cancel")
            progress.setMinimumDuration(0)
            progress.setAutoClose(False)
            progress.setAutoReset(False)
            progress.show()
            progress.raise_()
            progress.activateWindow()
            QApplication.processEvents()

            def on_progress(current, total, label):
                if progress.wasCanceled():
                    cancel_state["cancelled"] = True
                    progress.setLabelText("Transfer cancel requested...")
                    QApplication.processEvents()
                    return
                current_value = (int(current or 0) // (1024 * 1024))
                display_max = _safe_progress_units(total, unit=1024 * 1024)
                if progress.maximum() != display_max:
                    progress.setRange(0, display_max)
                progress.setValue(min(current_value, display_max))
                if label:
                    progress.setLabelText(
                        f"{label} ({current_value} MB / {display_max} MB)"
                    )
                self._status_bar.set_left_text(progress.labelText())
                QApplication.processEvents()

            self._status_bar.set_left_text("Linux install: copying payload files...")
            try:
                profile = {
                    "id": self._sync_engine.current_device_key or "ipod",
                    "name": getattr(device, "name", "iPod"),
                    "device_mount_path": device.mount_path,
                    "source_repo_path": self._repo_root,
                    "backup_location": os.path.join(
                        self._repo_root,
                        "rockpod",
                        ".backups",
                        "linux",
                        self._sync_engine.current_device_key or "ipod",
                    ),
                }
                result = self._linux_payload.install_bundle(
                    profile,
                    bundle,
                    allocation_gb=allocation_gb,
                    progress_callback=on_progress,
                    cancel_state=cancel_state,
                )
            except Exception as exc:
                progress.close()
                self._status_bar.set_left_text("Linux install failed during transfer.")
                QMessageBox.warning(self, "RockPod Linux", f"Install failed: {exc}")
                self._refresh_linux_manager()
                return

            progress.close()
            copied_count = int(result.get("copied_count") or 0)
            copied_bytes = int(result.get("copied_bytes") or 0)
            if result.get("success"):
                self._status_bar.set_left_text(
                    f"Linux installed: {copied_count} files ({copied_bytes // (1024 * 1024)} MB)"
                )
                warnings = list(result.get("warnings") or [])
                if warnings:
                    self._status_bar.set_left_text(
                        f"Linux installed with {len(warnings)} warning(s): "
                        f"first: {warnings[0]}"
                    )
                self._refresh_device_storage_breakdown(device)
                autoinstall = os.path.join(
                    device.mount_path,
                    "Linux",
                    "RockPodVM",
                    "autoinstall-linux-linux.command",
                )
                try:
                    self._launch_linux_autoinstall_preview(autoinstall)
                    self._status_bar.set_left_text("Linux VM copied; Debian autoinstall running")
                except Exception as exc:
                    QMessageBox.warning(self, "RockPod Linux", f"Installed, but autoinstall could not start: {exc}")
            else:
                failures = list(result.get("failures") or [])
                warnings = list(result.get("warnings") or [])
                required_present = all(
                    os.path.isfile(
                        os.path.join(
                            device.mount_path,
                            "Linux",
                            "RockPodVM",
                            filename,
                        )
                    )
                    for filename in (
                        DEBIAN_LIVE_XFCE_ISO,
                        "start-linux-linux.command",
                        "autoinstall-linux-linux.command",
                        "manifest.json",
                    )
                )
                details = [f"Copied {copied_count} files ({copied_bytes // (1024 * 1024)} MB)."]
                if not copied_bytes and not failures:
                    details.append("No payload bytes were written to the device.")
                if failures:
                    details.extend(failures[:6])
                    if len(failures) > 6:
                        details.append(f"... and {len(failures) - 6} more issues.")
                was_cancelled = bool(result.get("cancelled") or cancel_state.get("cancelled"))
                if was_cancelled:
                    should_remove = self._confirm_remove_linux_partial_payload(
                        os.path.join(device.mount_path, "Linux", "RockPodVM"),
                        copied_count,
                        copied_bytes,
                    )
                    if should_remove:
                        cleanup = self._linux_payload.cleanup_vm_root(device.mount_path)
                        if cleanup.get("success"):
                            self._status_bar.set_left_text("Install canceled; partial Linux files removed")
                        else:
                            details.append("Cleanup issues:")
                            details.extend((cleanup.get("failures") or [])[:6])
                            if len(cleanup.get("failures", [])) > 6:
                                details.append(f"... and {len(cleanup.get('failures', [])) - 6} more issues.")
                            QMessageBox.warning(self, "RockPod Linux", "\n".join(details))
                    else:
                        QMessageBox.warning(self, "RockPod Linux", "\n".join(details))
                        self._status_bar.set_left_text("Install canceled. Partial payload kept on device.")
                elif required_present and warnings and not failures:
                    autoinstall = os.path.join(
                        device.mount_path,
                        "Linux",
                        "RockPodVM",
                        "autoinstall-linux-linux.command",
                    )
                    warning_text = "\n".join(warnings[:6])
                    if len(warnings) > 6:
                        warning_text += f"\n... and {len(warnings) - 6} more warnings."
                    QMessageBox.warning(
                        self,
                        "RockPod Linux",
                        f"{warning_text}\n\nContinuing with Debian autoinstall despite non-fatal warnings."
                        if warning_text
                        else "Starting Debian autoinstall despite non-fatal warnings.",
                    )
                    try:
                        self._launch_linux_autoinstall_preview(autoinstall)
                        self._status_bar.set_left_text("Linux VM copied with warnings; Debian autoinstall running")
                    except Exception as exc:
                        QMessageBox.warning(self, "RockPod Linux", f"Installed, but autoinstall could not start: {exc}")
                else:
                    QMessageBox.warning(self, "RockPod Linux", "\n".join(details))
            self._refresh_linux_manager()
        finally:
            install_btn.setEnabled(True)
            install_btn.setText(original_install_text)

    def _confirm_remove_linux_partial_payload(self, vm_root, copied_count, copied_bytes):
        return (
            QMessageBox.question(
                self,
                "RockPod Linux",
                f"Install was canceled during copy.\n\n"
                f"Copied: {copied_count} files ({copied_bytes // (1024 * 1024)} MB)\n"
                f"Location: {vm_root}\n\n"
                "Remove partial files so you can retry?",
            )
            == QMessageBox.Yes
        )

    def _launch_linux_autoinstall_preview(self, autoinstall):
        if not os.path.isfile(autoinstall):
            raise FileNotFoundError(autoinstall)

        if self._linux_install_process is not None and self._linux_install_process.state() != QProcess.NotRunning:
            if self._linux_install_dialog is not None:
                self._linux_install_dialog.show()
                self._linux_install_dialog.raise_()
                self._linux_install_dialog.activateWindow()
            raise RuntimeError("A Debian autoinstall VM is already running.")

        bash_exec = shutil.which("bash") or "/bin/bash"
        if not os.path.isfile(bash_exec):
            raise RuntimeError("bash was not found in PATH and /bin/bash is unavailable.")

        dialog = LinuxInstallProgressDialog(self)
        dialog.set_status("Launching Debian autoinstall VM...")
        dialog.show()
        dialog.raise_()
        dialog.activateWindow()
        self._linux_install_dialog = dialog

        process = None
        startup_launch = True
        launch_time = None
        finished = {"done": False}

        def set_active_process(candidate):
            nonlocal process
            process = candidate
            self._linux_install_process = process
            process.setProcessChannelMode(QProcess.MergedChannels)
            process.setWorkingDirectory(os.path.dirname(autoinstall))

        def finalize(message, success):
            nonlocal process
            if finished["done"]:
                return
            finished["done"] = True
            read_output()
            dialog.mark_finished(message, success=success)
            self._linux_install_process = None

            if process is not None:
                try:
                    process.deleteLater()
                except Exception:
                    pass
                process = None

            if self._device_detector.current_device is not None:
                self._refresh_device_storage_breakdown(self._device_detector.current_device)
            self._refresh_linux_manager()

        def read_output():
            proc = process
            if proc is None:
                return
            try:
                text = bytes(proc.readAllStandardOutput()).decode("utf-8", "replace")
            except Exception:
                return
            if text:
                dialog.append_output(text)
                dialog.set_status("Debian installer is running...")

        def process_is_running(candidate=None):
            proc = process if candidate is None else candidate
            if proc is None:
                return False
            try:
                return proc.state() != QProcess.NotRunning
            except Exception:
                return False

        def stop_install():
            if process is None or process.state() == QProcess.NotRunning:
                return
            dialog.append_output("\nStopping Debian autoinstall VM...\n")
            dialog.set_status("Stopping Debian autoinstall VM...")
            pid = int(process.processId())
            if pid > 0:
                try:
                    os.killpg(pid, signal.SIGTERM)
                except OSError:
                    process.terminate()
            else:
                process.terminate()

            def force_stop():
                if process is None or process.state() == QProcess.NotRunning:
                    return
                pid = int(process.processId())
                if pid > 0:
                    try:
                        os.killpg(pid, signal.SIGKILL)
                    except OSError:
                        process.kill()
                else:
                    process.kill()

            QTimer.singleShot(5000, force_stop)

        def on_error(_error):
            if process is None:
                return
            if not process_is_running():
                return
            if startup_launch:
                return
            message = process.errorString() or "Could not start Debian autoinstall."
            dialog.append_output(f"\nRockPod Linux install process error: {message}\n")
            finalize("Debian autoinstall could not start.", success=False)
            self._status_bar.set_left_text("Debian autoinstall could not start")

        def on_finished(exit_code, exit_status):
            if process is None:
                return
            if startup_launch:
                return
            if finished["done"]:
                return
            elapsed_ms = int((time.monotonic() - (launch_time or time.monotonic())) * 1000)
            success = exit_status == QProcess.NormalExit and int(exit_code) == 0
            if success:
                if elapsed_ms < 1000:
                    message = (
                        f"Debian autoinstall exited immediately ({elapsed_ms} ms). "
                        "This usually means the installer did not launch correctly."
                    )
                    dialog.append_output(f"\n{message}\n")
                    finalize("Debian autoinstall did not start.", success=False)
                    self._status_bar.set_left_text("Debian autoinstall did not start")
                    return
                dialog.append_output("\nDebian autoinstall finished. Start VM is now available.\n")
                finalize("Debian autoinstall finished. Use Start VM.", success=True)
                self._status_bar.set_left_text("Debian autoinstall finished")
            else:
                dialog.append_output(f"\nDebian autoinstall exited with code {int(exit_code)}.\n")
                finalize("Debian autoinstall stopped or failed.", success=False)
                self._status_bar.set_left_text("Debian autoinstall stopped or failed")

        def run_process(program, arguments, command_text):
            nonlocal process, startup_launch, launch_time
            startup_launch = True
            launch_time = time.monotonic()
            candidate = QProcess(self)
            set_active_process(candidate)
            candidate.readyReadStandardOutput.connect(read_output)
            candidate.readyReadStandardError.connect(read_output)
            candidate.errorOccurred.connect(on_error)
            candidate.finished.connect(on_finished)
            dialog.append_output(f"$ {command_text}\n")
            candidate.start(program, arguments)
            if candidate.waitForStarted(3000):
                startup_launch = False
                process = candidate
                return True
            error = candidate.errorString() or "start failed"
            try:
                candidate.deleteLater()
            except Exception:
                pass
            candidate.close()
            return error

        dialog.stop_requested.connect(stop_install)

        quoted_workdir = shlex.quote(os.path.dirname(autoinstall))
        script_name = shlex.quote(os.path.basename(autoinstall))
        quoted_bash = shlex.quote(bash_exec)
        start_attempts = []
        start_attempts.append((bash_exec, [autoinstall], f"{quoted_bash} {shlex.quote(autoinstall)}"))
        start_attempts.append((bash_exec, ["-lc", f"cd {quoted_workdir} && ./{script_name}"], f"{quoted_bash} -lc 'cd {quoted_workdir} && ./{script_name}'"))

        start_error = None
        started = False
        for program, args, label in start_attempts:
            result = run_process(program, args, label)
            if result is True:
                started = True
                break
            start_error = result

        if not started:
            message = start_error or "Could not start Debian autoinstall."
            dialog.append_output(f"\nRockPod Linux install process error: {message}\n")
            dialog.mark_finished("Debian autoinstall could not start.", success=False)
            self._linux_install_process = None
            raise RuntimeError(message)

        def confirm_running():
            if process is None or finished["done"]:
                return
            if not process_is_running():
                read_output()
                try:
                    code = int(process.exitCode())
                except Exception:
                    code = 1
                if code != 0:
                    message = f"Debian autoinstall exited early with code {code}."
                    dialog.append_output(f"\nRockPod Linux install process error: {message}\n")
                    finalize("Debian autoinstall could not start.", success=False)

        # Give immediate feedback if the autoinstall exits before the user can read output.
        QTimer.singleShot(800, confirm_running)
        dialog.set_status("Debian installer is running...")

    def _start_linux_vm(self):
        device = self._device_detector.current_device
        if not device:
            QMessageBox.warning(self, "RockPod Linux", "Connect the iPod first.")
            return
        launcher = os.path.join(
            device.mount_path,
            "Linux",
            "RockPodVM",
            "start-linux-linux.command",
        )
        if not os.path.isfile(launcher):
            QMessageBox.warning(self, "RockPod Linux", "The VM start command was not found on this iPod.")
            self._refresh_linux_manager()
            return
        try:
            start_detached_command(["bash", launcher])
        except Exception as exc:
            QMessageBox.warning(self, "RockPod Linux", f"Start failed: {exc}")
            return
        self._status_bar.set_left_text("RockPod Linux VM launched")

    def _provision_linux_vm(self):
        device = self._device_detector.current_device
        if not device:
            QMessageBox.warning(self, "RockPod Linux", "Connect the iPod first.")
            return
        try:
            status = self._linux_payload.installed_status(device.mount_path)
        except ValueError as exc:
            QMessageBox.warning(self, "RockPod Linux", str(exc))
            return
        if not status.get("installed"):
            QMessageBox.warning(self, "RockPod Linux", "Install the RockPod Linux VM first.")
            return
        message = (
            "Install RockPod apps and desktop theme into the VM on this iPod?\n\n"
            "The VM must be shut down. RockPod will launch a host-side provisioner "
            "that may ask for administrator permission to attach and mount the VMDK."
        )
        if QMessageBox.question(self, "Install Apps/Theme", message) != QMessageBox.Yes:
            return
        try:
            self._linux_payload.refresh_vm_provision_files(device.mount_path)
            provisioner = os.path.join(
                device.mount_path,
                "Linux",
                "RockPodVM",
                "provision-rockpod-linux-linux.command",
            )
            start_detached_command(["bash", provisioner])
        except Exception as exc:
            QMessageBox.warning(self, "RockPod Linux", f"Provisioning could not start: {exc}")
            return
        self._status_bar.set_left_text("RockPod Linux apps/theme provisioner launched")

    def _uninstall_linux_payload(self):
        device = self._device_detector.current_device
        if not device:
            QMessageBox.warning(self, "RockPod Linux", "Connect the iPod first.")
            return
        try:
            status = self._linux_payload.installed_status(device.mount_path)
        except ValueError as exc:
            QMessageBox.warning(self, "RockPod Linux", str(exc))
            return
        size_mb = int(status.get("size_bytes") or 0) // (1024 * 1024)
        if QMessageBox.question(
            self,
            "Uninstall RockPod Linux",
            f"Remove RockPod Linux VM files from this iPod?\n\n"
            f"Folder: {status.get('vm_root', '')}\n"
            f"Current size: {size_mb} MB\n\n"
            "Only manifest-owned RockPod VM files are removed.",
        ) != QMessageBox.Yes:
            return
        try:
            result = self._linux_payload.uninstall_vm(device.mount_path)
        except Exception as exc:
            QMessageBox.warning(self, "RockPod Linux", f"Uninstall failed: {exc}")
            self._refresh_linux_manager()
            return
        if result.get("success"):
            self._status_bar.set_left_text(f"RockPod Linux removed: {result.get('removed_count', 0)} files")
            self._refresh_device_storage_breakdown(device)
        else:
            QMessageBox.warning(self, "RockPod Linux", "\n".join(result.get("failures", [])) or "Uninstall failed")
        self._refresh_linux_manager()

    # ═══════════════════════════════════════════════════════════════
    # Rockbox simulator
    # ═══════════════════════════════════════════════════════════════

    def _simulator_target_by_id(self, target_id):
        for target in self._simulator_targets:
            if target["id"] == target_id:
                return target
        return None

    def _refresh_simulator_panel(self):
        profiles = self._rockbox_profiles.profiles()
        selected_profile_id = self._rockbox_profiles.selected_profile_id()
        self._simulator_panel.set_profiles(profiles, selected_profile_id)
        self._simulator_targets = self._rockbox_simulator.discover_targets(self._repo_root)
        profile = self._rockbox_profiles.current_profile()
        selected_sim = ""
        if profile:
            selected_sim = profile.get("simulator_target") or ""
            if not selected_sim and self._simulator_targets:
                matches = [target for target in self._simulator_targets if target["screen_resolution"] == profile["screen_resolution"]]
                selected_sim = matches[0]["id"] if matches else self._simulator_targets[0]["id"]
        self._simulator_panel.set_simulators(self._simulator_targets, selected_sim)
        if profile and selected_sim:
            self._set_simulator_details(profile, selected_sim)
        else:
            self._current_simulator_diff = None

    def _set_simulator_details(self, profile, target_id):
        target = self._simulator_target_by_id(target_id)
        if not profile or not target:
            self._current_simulator_diff = None
            self._simulator_panel.set_details({}, profile or {}, "", None)
            return
        screenshot_dir = self._rockbox_simulator.screenshot_dir(profile, target)
        sim_profile = self._rockbox_simulator.simulator_profile(profile, target)
        details = self._rockbox_themes.inspect_theme(profile["selected_theme"], profile["source_repo_path"])
        try:
            diff = self._rockbox_deploy.build_diff(sim_profile, details)
        except ValueError:
            diff = None
        self._current_simulator_diff = diff
        self._simulator_panel.set_details(target, profile, screenshot_dir, diff["summary"] if diff else None)

    def _on_simulator_profile_selected(self, profile_id):
        self._rockbox_profiles.set_selected_profile(profile_id)
        self._refresh_theme_hub()
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_simulator_panel()

    def _on_simulator_target_selected(self, target_id):
        profile = self._rockbox_profiles.current_profile()
        if profile:
            self._set_simulator_details(profile, target_id)

    def _bind_simulator_to_profile(self):
        profile = self._rockbox_profiles.current_profile()
        target = self._simulator_target_by_id(self._simulator_panel.current_simulator_id())
        if not profile or not target:
            self._status_bar.set_left_text("No simulator target selected")
            return
        updated = self._rockbox_simulator.bind_profile(profile, target)
        self._rockbox_profiles.save_profile(updated)
        self._status_bar.set_left_text(f"Bound simulator {target['id']} to profile {updated['name']}")
        self._refresh_boot_manager()
        self._refresh_plugin_manager()
        self._refresh_game_manager()
        self._refresh_simulator_panel()

    def _dry_run_to_simulator(self):
        if not self._current_simulator_diff:
            self._status_bar.set_left_text("No simulator diff available")
            return
        summary = self._current_simulator_diff["summary"]
        QMessageBox.information(
            self,
            "Simulator Dry Run",
            "\n".join(
                [
                    f"Add: {summary['add']}",
                    f"Overwrite: {summary['overwrite']}",
                    f"Unchanged: {summary['unchanged']}",
                    f"Missing: {summary['missing_source']}",
                ]
            ),
        )

    def _apply_to_simulator(self):
        profile = self._rockbox_profiles.current_profile()
        target = self._simulator_target_by_id(self._simulator_panel.current_simulator_id())
        if not profile or not target or not self._current_simulator_diff:
            self._status_bar.set_left_text("No simulator deploy available")
            return
        sim_profile = self._rockbox_simulator.simulator_profile(profile, target)
        result = self._rockbox_deploy.apply_diff(sim_profile, self._current_simulator_diff)
        if result["success"]:
            self._status_bar.set_left_text(f"Simulator updated: {result['copied_count']} files")
            QMessageBox.information(
                self,
                "Simulator Updated",
                f"Copied {result['copied_count']} files to {target['simdisk_path']}\nBackup: {result['backup_dir']}",
            )
        else:
            QMessageBox.warning(self, "Simulator Deploy Failed", "\n".join(result["failures"]))
            self._status_bar.set_left_text("Simulator deploy failed")
        self._refresh_simulator_panel()

    def _launch_simulator(self):
        target = self._simulator_target_by_id(self._simulator_panel.current_simulator_id())
        if not target:
            self._status_bar.set_left_text("No simulator target selected")
            return
        launched = self._rockbox_simulator.launch(target)
        self._status_bar.set_left_text(f"Launched {target['id']} (pid {launched['pid']})")

    def _capture_simulator_screenshot(self):
        profile = self._rockbox_profiles.current_profile()
        target = self._simulator_target_by_id(self._simulator_panel.current_simulator_id())
        if not profile or not target:
            self._status_bar.set_left_text("No simulator target selected")
            return
        result = self._rockbox_simulator.capture_screenshot(profile, target)
        if result["success"]:
            self._status_bar.set_left_text(f"Screenshot saved: {result['captured_path']}")
            QMessageBox.information(self, "Screenshot Captured", result["captured_path"])
            self._refresh_theme_designer_simulator_preview(profile)
        else:
            self._status_bar.set_left_text(result["message"])
            QMessageBox.warning(self, "Capture Failed", result["message"])

    def _open_simshots_folder(self):
        profile = self._rockbox_profiles.current_profile()
        target = self._simulator_target_by_id(self._simulator_panel.current_simulator_id())
        if not profile:
            return
        path = self._rockbox_simulator.ensure_screenshot_dir(profile, target)
        start_detached_command(["xdg-open", path])

    # ═══════════════════════════════════════════════════════════════
    # Window lifecycle
    # ═══════════════════════════════════════════════════════════════

    def closeEvent(self, event):
        """Save state on close."""
        self._device_detector.stop_polling()
        self._scanner.shutdown()
        self._android_importer.shutdown()
        self._device_inventory.shutdown()
        self._device_storage_analyzer.shutdown()
        self._album_metadata_fetcher.shutdown()
        self._dismiss_album_metadata_progress()
        self._dismiss_android_import_progress()
        self._sync_engine.shutdown()
        self._artwork.shutdown()

        # Save geometry
        self._config.window_geometry = self.saveGeometry().toBase64().data().decode()
        self._config.sidebar_width = self._splitter.sizes()[0]
        try:
            self._config.save()
        except OSError as exc:
            logger.warning("Could not save config on close: %s", exc)

        self._db.close()
        event.accept()
