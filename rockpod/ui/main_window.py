"""Main application window — ties together all UI components in an iTunes 7 layout."""

import logging
import os
import time
from datetime import datetime

from PySide6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QSplitter,
    QApplication, QMessageBox, QInputDialog, QMenu, QStackedWidget, QFileDialog,
    QProgressDialog,
)
from PySide6.QtCore import Qt, QTimer, Slot
from PySide6.QtGui import QAction, QIcon

from app.config import Config
from app.database import Database
from services.library_scanner import LibraryScanner
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
from services.playback import PlaybackService, STATE_STOPPED
from services.sync_engine import SyncEngine
from services.theme_assets import ThemeAssetManager
from services.video_thumbnails import VideoThumbnailService
from services.smart_playlists import ensure_default_smart_playlists, evaluate_playlist
from services.smart_playlists import evaluate_playlist_count
from services.rockbox_device import (
    clear_rockbox_database_cache,
    detect_rockbox_database_state,
    enable_rockbox_tagcache_autoupdate,
)
from services.rockbox_deploy import RockboxDeployService
from services.rockbox_boot import RockboxBootService
from services.rockbox_games import RockboxGameService
from services.ipone_wallpapers import IPoneWallpaperService
from services.rockbox_profiles import RockboxProfileStore
from services.rockbox_plugins import RockboxPluginService
from services.rockbox_runtime import import_runtime_data_for_device
from services.rockbox_playlists import export_device_playlists
from services.rockbox_simulator import RockboxSimulatorService
from services.rockbox_themes import RockboxThemeService
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
from ui.ipone_wallpaper_manager import IPoneWallpaperManagerWidget
from ui.web_browser import BrowserPanel
from ui.boot_manager import BootManagerWidget
from ui.theme_hub import ThemeHubWidget
from ui.theme_designer import ThemeDesignerWidget
from ui.video_library import VideoGridView, build_video_browser_groups
from ui.video_player import VideoPlayerWindow
from ui.dialogs.metadata_editor import MetadataEditor
from ui.dialogs.album_info import AlbumInfoDialog
from ui.dialogs.album_metadata import AlbumMetadataDialog
from ui.dialogs.device_settings import DeviceSettingsDialog
from ui.dialogs.preferences import PreferencesDialog
from ui.dialogs.sync_dialog import SyncDialog

logger = logging.getLogger(__name__)


class MainWindow(QMainWindow):
    """RockPod main window — iTunes 7-era layout and behavior."""

    def __init__(self, config=None):
        super().__init__()

        # Core services
        self._config = config or Config()
        self._config.ensure_dirs()
        self._db_was_missing = not os.path.exists(self._config.db_path)
        self._db = Database(self._config.db_path)
        self._artwork = ArtworkManager(self._config.artwork_cache_dir, self._config, self)
        self._video_thumbnails = VideoThumbnailService(
            self._config.artwork_cache_dir,
            self._config,
            self._artwork,
        )
        self._android_importer = AndroidMediaImporter(self)
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
        self._ipone_wallpapers_service = IPoneWallpaperService()
        self._rockbox_plugins = RockboxPluginService()
        self._rockbox_simulator = RockboxSimulatorService()
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

        created = ensure_default_smart_playlists(self._db)
        if created:
            self._db.commit()

        self._init_window()
        self._init_ui()
        self._init_menu()
        self._connect_signals()
        self._load_cached_library()
        self._refresh_playlists()
        self._refresh_browser_panel()
        self._refresh_current_rockbox_panel()

        # Let the window paint before probing the device or refreshing
        # off-screen Rockbox panels. Those paths can touch slow removable
        # storage and make startup feel hung if they run synchronously here.
        QTimer.singleShot(0, self._device_detector.start_polling)
        QTimer.singleShot(150, self._refresh_nonvisible_rockbox_panels)

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
        self._video_player_window = None
        self._device_summary = DeviceSummaryWidget()
        self._theme_hub = ThemeHubWidget()
        self._ipone_wallpapers = IPoneWallpaperManagerWidget()
        self._theme_designer = ThemeDesignerWidget()
        self._boot_manager = BootManagerWidget()
        self._plugin_manager = PluginManagerWidget()
        self._game_manager = GameManagerWidget()
        self._browser_panel = BrowserPanel()
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
        self._content_stack.addWidget(self._artist_view)
        self._content_stack.addWidget(self._album_view)
        self._content_stack.addWidget(self._genre_view)
        self._content_stack.addWidget(self._device_summary)
        self._content_stack.addWidget(self._theme_hub)
        self._content_stack.addWidget(self._ipone_wallpapers)
        self._content_stack.addWidget(self._theme_designer)
        self._content_stack.addWidget(self._boot_manager)
        self._content_stack.addWidget(self._plugin_manager)
        self._content_stack.addWidget(self._game_manager)
        self._content_stack.addWidget(self._browser_panel)
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
        view_menu.addAction("Inspect Selected Artwork", self._show_selected_artwork_debug)

        # Device menu
        device_menu = menubar.addMenu("Device")
        device_menu.addAction("Sync to iPod", self._start_sync, "Ctrl+S")
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
        self._video_view.track_double_clicked.connect(self._on_track_double_click)
        self._video_view.selection_changed.connect(self._on_track_selection_changed)
        self._video_view.context_requested.connect(self._on_video_context_menu)

        # Library browser views
        self._artist_view.track_double_clicked.connect(self._on_track_double_click)
        self._artist_view.selection_changed.connect(self._on_track_selection_changed)
        self._album_view.track_double_clicked.connect(self._on_track_double_click)
        self._album_view.selection_changed.connect(self._on_track_selection_changed)
        self._album_view.album_context_requested.connect(self._on_album_context_menu)
        self._genre_view.track_double_clicked.connect(self._on_track_double_click)
        self._genre_view.selection_changed.connect(self._on_track_selection_changed)
        self._theme_hub.profile_selected.connect(self._on_theme_profile_selected)
        self._theme_hub.profile_saved.connect(self._on_theme_profile_saved)
        self._theme_hub.theme_selected.connect(self._on_theme_selected)
        self._theme_hub.deploy_requested.connect(self._deploy_selected_theme)
        self._theme_hub.restore_requested.connect(self._restore_theme_backup)
        self._theme_hub.use_connected_device_requested.connect(self._use_connected_device_for_profile)
        self._ipone_wallpapers.profile_selected.connect(self._on_ipone_wallpaper_profile_selected)
        self._ipone_wallpapers.apply_requested.connect(self._apply_ipone_wallpapers)
        self._ipone_wallpapers.import_requested.connect(self._import_ipone_wallpaper)
        self._ipone_wallpapers.remove_requested.connect(self._remove_ipone_wallpaper)
        self._theme_designer.profile_selected.connect(self._on_theme_designer_profile_selected)
        self._theme_designer.variant_selected.connect(self._on_theme_designer_variant_selected)
        self._theme_designer.preview_changed.connect(self._on_theme_designer_preview_changed)
        self._theme_designer.simulator_refresh_requested.connect(self._refresh_theme_designer_preview_now)
        self._theme_designer.save_requested.connect(self._save_theme_designer_variant)
        self._theme_designer.rename_requested.connect(self._rename_theme_designer_variant)
        self._theme_designer.duplicate_requested.connect(self._duplicate_theme_designer_variant)
        self._theme_designer.deploy_device_requested.connect(self._deploy_theme_designer_variant_to_device)
        self._theme_designer.deploy_simulator_requested.connect(self._deploy_theme_designer_variant_to_simulator)
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
        self._browser_panel.open_external_requested.connect(self._open_browser_external)
        self._simulator_panel.profile_selected.connect(self._on_simulator_profile_selected)
        self._simulator_panel.simulator_selected.connect(self._on_simulator_target_selected)
        self._simulator_panel.bind_requested.connect(self._bind_simulator_to_profile)
        self._simulator_panel.dry_run_requested.connect(self._dry_run_to_simulator)
        self._simulator_panel.apply_requested.connect(self._apply_to_simulator)
        self._simulator_panel.launch_requested.connect(self._launch_simulator)
        self._simulator_panel.capture_requested.connect(self._capture_simulator_screenshot)
        self._simulator_panel.open_screenshots_requested.connect(self._open_simshots_folder)

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

    def _on_scan_error(self, msg):
        self._toolbar.set_scanning(False)
        self._status_bar.stop_scan_activity()
        self._scan_ui_refresh_timer.stop()
        self._scan_ui_refresh_pending = False
        self._last_scan_report = self._scanner.last_report
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
        if self._current_view == "library_videos":
            return "video"
        return "audio"

    def _current_status_item_label(self):
        if self._current_view == "library_videos":
            return "videos"
        return "songs"

    def _active_track_table(self):
        if self._current_view == "library_videos":
            return self._video_view
        return self._track_table

    def _active_track_selection_ids(self):
        return self._active_track_table().get_selected_track_ids()

    def _refresh_view(self):
        """Reload the track table based on current sidebar selection and filters."""
        selected_ids = self._active_track_selection_ids()
        tracks = self._tracks_for_current_view(include_search=True)
        if self._current_view == "library_videos":
            self._video_view.set_tracks(tracks)
            self._video_view.set_current_track_id(self._current_playing_track_id)
            self._video_view.select_track_ids(selected_ids)
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
            elif not self._sync_engine.device_inventory_has_local_links():
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
        elif self._current_view == "rockbox_boot":
            tracks = []
        elif self._current_view == "rockbox_plugins":
            tracks = []
        elif self._current_view == "rockbox_games":
            tracks = []
        elif self._current_view == "rockbox_browser":
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
            "library_artists": "Artists",
            "library_albums": "Albums",
            "library_genres": "Genres",
            "rockbox_themes": "Themes",
            "rockbox_wallpapers": "Wallpapers",
            "rockbox_theme_designer": "Theme Designer",
            "rockbox_boot": "Boot / Branding",
            "rockbox_plugins": "Plugins",
            "rockbox_games": "Games",
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
        elif self._current_view == "rockbox_boot":
            self._content_stack.setCurrentWidget(self._boot_manager)
            self._refresh_boot_manager()
        elif self._current_view == "rockbox_plugins":
            self._content_stack.setCurrentWidget(self._plugin_manager)
            self._refresh_plugin_manager()
        elif self._current_view == "rockbox_games":
            self._content_stack.setCurrentWidget(self._game_manager)
            self._refresh_game_manager()
        elif self._current_view == "rockbox_browser":
            self._content_stack.setCurrentWidget(self._browser_panel)
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

    def _on_metadata_saved(self, track_id, updates):
        if updates:
            self._db.update_track_metadata(track_id, updates)
            self._db.commit()
            self._refresh_view()
            logger.info("Updated metadata for track %d: %s", track_id, list(updates.keys()))

    def _select_all(self):
        self._active_track_table().selectAll()

    def _on_table_context_menu(self, pos):
        table = self.sender() if isinstance(self.sender(), TrackTable) else self._track_table
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

        scanned = int(report.get("scanned", 0) or 0)
        imported = int(report.get("imported", 0) or 0)
        imported_photos = int(report.get("imported_photos", 0) or 0)
        imported_videos = int(report.get("imported_videos", 0) or 0)
        skipped = int(report.get("skipped_existing", 0) or 0)
        failures = list(report.get("failures") or [])
        cancelled = bool(report.get("cancelled"))
        device_subdir = report.get("device_subdir", "")
        for_tiktok_plugin = bool(report.get("for_tiktok_plugin"))
        feed_entries = int(report.get("feed_entries", 0) or 0)

        if scanned == 0 and not failures:
            self._status_bar.set_left_text("No Android media found")
            QMessageBox.information(
                self,
                "Import Android Media",
                "No photos or videos were found under the selected Android storage root.",
            )
            return

        lines = [
            f"Scanned {scanned} Android media items.",
            f"Imported {imported} files to /{device_subdir}.",
        ]
        if imported:
            lines.append(f"Imported breakdown: {imported_photos} photos, {imported_videos} videos.")
        if skipped:
            lines.append(f"Skipped {skipped} items already present on the iPod.")
        if for_tiktok_plugin:
            lines.append(f"Updated iPodTikTok feed with {feed_entries} clips.")
        if cancelled:
            lines.append("Import was cancelled before all items finished.")
        if failures:
            lines.append("")
            lines.append("Failures:")
            for item in failures[:8]:
                lines.append(
                    f"{os.path.basename(item.get('path', ''))}: "
                    f"{item.get('error', 'conversion failed')}"
                )
            if len(failures) > 8:
                lines.append(f"...and {len(failures) - 8} more.")

        if imported and not cancelled:
            if for_tiktok_plugin:
                self._status_bar.set_left_text(
                    f"Imported iPodTikTok clips: {imported} files, feed now has {feed_entries} clips"
                )
            else:
                self._status_bar.set_left_text(
                    f"Imported Android media: {imported} files ({imported_photos} photos, {imported_videos} videos)"
                )
        elif cancelled:
            self._status_bar.set_left_text("Android import cancelled")

        if failures:
            QMessageBox.warning(self, "Import Android Media", "\n".join(lines))
        else:
            QMessageBox.information(self, "Import Android Media", "\n".join(lines))

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
        self._refresh_view()
        self._status_bar.set_left_text(f"Removed {len(track_ids)} track(s) from playlist")

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
        self._update_sync_status()
        self._refresh_playlists()
        if self._current_view in ("device_music", "device_not_on_ipod"):
            self._refresh_view()
        if self._current_view == "device_root":
            self._update_device_summary()
        self._refresh_current_rockbox_panel()
        QTimer.singleShot(0, self._refresh_nonvisible_rockbox_panels)

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
        self._refresh_playlists()
        if self._current_view == "device_root":
            self._update_device_summary()
        self._refresh_current_rockbox_panel()
        QTimer.singleShot(0, self._refresh_nonvisible_rockbox_panels)

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
        elif self._current_view == "rockbox_games":
            self._refresh_game_manager()
        elif self._current_view == "rockbox_simulator":
            self._refresh_simulator_panel()

    def _refresh_nonvisible_rockbox_panels(self):
        refreshers = [
            ("rockbox_themes", self._refresh_theme_hub),
            ("rockbox_wallpapers", self._refresh_ipone_wallpapers),
            ("rockbox_theme_designer", self._refresh_theme_designer),
            ("rockbox_boot", self._refresh_boot_manager),
            ("rockbox_plugins", self._refresh_plugin_manager),
            ("rockbox_games", self._refresh_game_manager),
            ("rockbox_simulator", self._refresh_simulator_panel),
        ]
        for view_id, refresher in refreshers:
            if self._current_view == view_id:
                continue
            refresher()

    def _on_device_space_update(self, device):
        device_key = self._sync_engine.current_device_key
        if device_key and device_key in self._device_storage_cache:
            cached = dict(self._device_storage_cache[device_key])
            cached["total"] = int(getattr(device, "total_space", 0) or cached.get("total") or 0)
            cached["free"] = int(getattr(device, "free_space", 0) or cached.get("free") or 0)
            cached["used"] = max(cached["total"] - cached["free"], sum(int(cached.get(key, 0) or 0) for key in ("music", "games_plugins", "themes_assets", "rockbox_system", "other")))
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
        box = QMessageBox(self)
        box.setWindowTitle("Device Diff")
        box.setIcon(QMessageBox.Information)
        box.setText(format_reconciliation_report(report))
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

        self._status_bar.set_left_text("Planning sync...")
        plan = self._sync_engine.build_sync_plan()
        if plan.total_operations == 0 and not plan.errors:
            self._status_bar.set_left_text("Up to date")
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

        self._status_bar.set_left_text("Planning sync...")
        plan = self._sync_engine.build_sync_plan(track_ids=track_ids)
        if plan.total_operations == 0:
            self._status_bar.set_left_text("Selected tracks are already synced")
            return
        self._status_bar.set_left_text(plan.summary())

        dialog = SyncDialog(plan, self)
        dialog.sync_confirmed.connect(lambda: self._execute_sync(plan, dialog))
        dialog.sync_cancelled.connect(lambda: self._sync_engine.cancel_sync())
        dialog.exec()

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
            t_cache = time.perf_counter()
            self._update_device_cache_from_plan(self._active_sync_plan)
            post_sync_db_update_seconds = time.perf_counter() - t_cache
            self._post_sync_rockbox_integration()
            profile = dict(getattr(self._active_sync_plan, "execution_profile", {}) or {})
            profile["post_sync_db_update_seconds"] = post_sync_db_update_seconds
            self._active_sync_plan.execution_profile = profile
            logger.info(
                "Sync finalize timing: post_sync_db_update=%.3fs total=%.3fs",
                post_sync_db_update_seconds,
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
        device = self._device_detector.current_device
        if not device or not getattr(device, "is_rockbox", False):
            return
        playlist_result = None
        if self._device_config_value("sync_playlists_to_device", True):
            playlist_result = self._sync_rockbox_playlists(device)
        had_track_changes = bool(
            self._active_sync_plan and (
                self._active_sync_plan.to_copy
                or self._active_sync_plan.to_resync
                or self._active_sync_plan.to_delete
            )
        )
        if not had_track_changes:
            self._device_state = "Ready"
            self._rockbox_db_stale = False
            self._rockbox_db_update_started_at = None
            self._rockbox_db_feedback_tick = 0
            if playlist_result and playlist_result["success"] and playlist_result["exported"]:
                self._status_bar.set_left_text(
                    f"Sync complete; {len(playlist_result['exported'])} playlist(s) updated"
                )
            else:
                self._status_bar.set_left_text("Sync complete")
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

    def _sync_rockbox_playlists(self, device):
        try:
            result = export_device_playlists(self._db, device)
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
        profile = self._rockbox_profiles.current_profile()
        download_dir = ""
        if profile:
            download_dir = profile.get("games_library_path", "")
            if not download_dir:
                download_dir = self._config.get("games_library_path", "")
            if not download_dir:
                download_dir = os.path.join(os.path.expanduser("~"), "Documents", "Gameboy")
        self._browser_panel.set_auto_accept_cookies(self._config.get("store_auto_accept_cookies", True))
        self._browser_panel.set_store_context(
            self._config.get("browser_home_url", "https://www.rockbox.org/"),
            download_dir,
        )

    def _open_browser_external(self, url):
        if not url:
            return
        import subprocess
        subprocess.Popen(["xdg-open", url], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)

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
            self._refresh_browser_panel()
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
            self._ipone_wallpapers.set_candidates([], [])
            return
        candidates = self._ipone_wallpapers_service.list_candidates(
            profile["source_repo_path"],
            profile,
        )
        self._ipone_wallpapers.set_candidates(
            candidates.get("lock", []),
            candidates.get("charge", []),
        )

    def _on_ipone_wallpaper_profile_selected(self, profile_id):
        self._rockbox_profiles.set_selected_profile(profile_id)
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
        if not lock_source and not charge_source:
            self._status_bar.set_left_text("No wallpaper selected")
            return
        try:
            bundle = self._ipone_wallpapers_service.build_apply_bundle(
                profile,
                lock_source=lock_source,
                charge_source=charge_source,
            )
            diff = self._rockbox_deploy.build_diff(profile, bundle)
        except ValueError as exc:
            QMessageBox.warning(self, "Apply Wallpapers", str(exc))
            return

        summary = diff["summary"]
        lines = [
            f"Apply wallpapers to {profile['name']}?",
            "",
            f"Add {summary['add']} files",
            f"Overwrite {summary['overwrite']} files",
            f"Unchanged {summary['unchanged']} files",
        ]
        if lock_source:
            lines.extend(["", f"Lockscreen: {os.path.basename(lock_source)}"])
        if charge_source:
            lines.extend(["", f"Charge: {os.path.basename(charge_source)}"])
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
        QMessageBox.information(self, "Wallpaper Apply Result", message)
        self._status_bar.set_left_text(
            "Wallpapers applied" if result["success"] else "Wallpaper apply completed with errors"
        )
        self._refresh_ipone_wallpapers()

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
        kind_label = "lockscreen" if kind == "lock" else "charge"
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
            QMessageBox.information(self, "Remove Wallpaper", "Built-in iPone wallpapers cannot be removed.")
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
        )
        if not themes:
            self._current_theme_diff = None
            self._theme_hub.set_themes([], "")
            self._theme_hub.set_diff_summary(None)
            return
        selected_theme = profile.get("selected_theme") or themes[0]["id"]
        if not any(item["id"] == selected_theme for item in themes):
            selected_theme = themes[0]["id"]
        self._theme_hub.set_themes(themes, selected_theme)
        self._set_theme_details(profile, selected_theme)

    def _set_theme_details(self, profile, theme_id):
        details = self._rockbox_themes.inspect_theme(theme_id, profile["source_repo_path"])
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
        self._ensure_theme_designer_default_preview(profile, variant)

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
        self._ensure_theme_designer_default_preview(profile, variant)

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
        if not target and profile.get("screen_resolution") == "320x240":
            build_dir = os.path.join(profile["source_repo_path"], "build-sim-video-5g")
            binary = os.path.join(build_dir, "rockboxui")
            simdisk = os.path.join(build_dir, "simdisk")
            if os.path.isfile(binary) and os.path.isdir(simdisk):
                target = {
                    "id": "build-sim-video-5g",
                    "build_dir": build_dir,
                    "binary_path": binary,
                    "simdisk_path": simdisk,
                    "screen_resolution": "320x240",
                }
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
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        target = self._preferred_theme_designer_simulator_target(profile)
        if not target and profile.get("screen_resolution") == "320x240":
            build_dir = os.path.join(profile["source_repo_path"], "build-sim-video-5g")
            binary = os.path.join(build_dir, "rockboxui")
            simdisk = os.path.join(build_dir, "simdisk")
            if os.path.isfile(binary) and os.path.isdir(simdisk):
                target = {
                    "id": "build-sim-video-5g",
                    "name": "build-sim-video-5g",
                    "build_dir": build_dir,
                    "binary_path": binary,
                    "simdisk_path": simdisk,
                    "screen_resolution": "320x240",
                    "device_model": "iPod Classic / Video",
                }
        if not target:
            return
        try:
            bundle = self._theme_designer_service.build_preview_bundle(
                profile["source_repo_path"],
                profile,
                self._pending_designer_preview_variant or (self._current_designer_variant or {}),
            )
        except Exception:
            logger.exception("Theme designer preview bundle generation failed")
            return

        self._theme_designer.shutdown_simulator_preview()
        preview_target = self._rockbox_simulator.theme_designer_preview_target(profile, target, reset=True)
        preview_profile = self._rockbox_simulator.simulator_profile(profile, preview_target)
        preview_screen = str(
            (self._pending_designer_preview_variant or {}).get("preview_screen")
            or "wps"
        ).strip().lower() or "wps"
        try:
            diff = self._rockbox_deploy.build_diff(preview_profile, bundle)
            self._rockbox_deploy.apply_diff(preview_profile, diff)
            self._rockbox_simulator.activate_theme_preview(
                preview_target,
                bundle["id"],
                preview_screen=preview_screen,
            )
        except Exception:
            logger.exception("Theme designer preview deployment failed")
            return
        shot = self._rockbox_simulator.capture_theme_preview(
            preview_target,
            preview_screen=preview_screen,
        )
        if shot:
            self._current_designer_preview_path = shot
            self._theme_designer.set_simulator_preview(shot)

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

    def _deploy_theme_designer_variant_to_device(self, variant_data):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        bundle = self._saved_theme_bundle_for_profile(profile, variant_data)
        if bundle:
            self._deploy_theme_bundle(profile, bundle, "device")

    def _deploy_theme_designer_variant_to_simulator(self, variant_data):
        profile = self._rockbox_profiles.current_profile()
        if not profile:
            return
        bundle = self._saved_theme_bundle_for_profile(profile, variant_data)
        if not bundle:
            return
        target_id = profile.get("simulator_target") or ""
        target = self._simulator_target_by_id(target_id) if target_id else None
        if not target and self._simulator_targets:
            matches = [item for item in self._simulator_targets if item["screen_resolution"] == profile["screen_resolution"]]
            target = matches[0] if matches else self._simulator_targets[0]
        if not target:
            QMessageBox.information(self, "Theme Designer", "No simulator target is available for this profile.")
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
            QMessageBox.warning(self, "Theme Designer", str(exc))
            return None
        self._current_designer_variant = saved
        self._refresh_theme_designer()
        return bundle

    def _deploy_theme_bundle(self, deploy_profile, bundle, target_label):
        try:
            diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
        except ValueError as exc:
            QMessageBox.warning(self, "Theme Designer", str(exc))
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
        result = self._rockbox_deploy.apply_diff(deploy_profile, diff)
        message = f"Copied {result['copied_count']} files\nBackup: {result['backup_dir']}"
        if result["failures"]:
            message += "\n\nFailures:\n" + "\n".join(result["failures"][:8])
        QMessageBox.information(self, "Theme Designer Deploy", message)
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
        deploy_profile = self._rockbox_boot.deploy_profile(profile, self._current_boot_target_mode, sim_target)
        destination_rel = f".rockbox/rockpod/boot/branding/{profile['screen_resolution']}/boot-logo.bmp"
        diff = None
        if validation["valid"]:
            try:
                bundle = self._rockbox_boot.build_bundle(
                    profile,
                    image_path,
                    os.path.join(self._config.cache_dir, "boot_staging"),
                )
                diff = self._rockbox_deploy.build_diff(deploy_profile, bundle)
            except (ValueError, OSError):
                diff = None
        self._current_boot_diff = diff
        self._boot_manager.set_details(
            image_path,
            f"{spec['width']}x{spec['height']}",
            destination_rel,
            validation["message"],
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

    def _apply_boot_deploy(self):
        profile = self._rockbox_profiles.current_profile()
        if not profile or not self._current_boot_diff:
            self._status_bar.set_left_text("No boot branding deploy available")
            return
        sim_target = self._simulator_target_by_id(profile.get("simulator_target"))
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
        self._refresh_browser_panel()

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
        self._refresh_browser_panel()

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
        import subprocess
        subprocess.Popen(["xdg-open", path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)

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
