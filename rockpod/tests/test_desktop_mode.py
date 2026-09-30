"""Desktop Mode host parity and device-selection tests."""

import json
from pathlib import Path
from types import SimpleNamespace

import services.desktop_mode as desktop


def test_desktop_mode_accepts_standard_ipod_dock_remote_buttons():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")

    assert "static long dm_remote_button(long button)" in source
    assert "case BUTTON_RC_LEFT:" in source
    assert "case BUTTON_RC_RIGHT:" in source
    assert "case BUTTON_RC_PLAY:" in source
    assert "case BUTTON_RC_SELECT:" in source
    assert "case BUTTON_RC_MENU:" in source
    assert "case BUTTON_RC_STOP:" in source
    assert "if (button & BUTTON_RC_UP)" in source
    assert "if (button & BUTTON_RC_DOWN)" in source
    assert "button = dm_remote_button(button);" in source


def _valid_pack(path):
    return {
        "valid": True,
        "complete": True,
        "root": str(path),
        "errors": [],
        "missing": [],
        "checked": ["desktop.aurora", "cursor.arrow"],
        "manifest": {
            "pack_id": "snow-leopard-10.6-ipod-320x240",
            "product_version": "10.6.8",
        },
    }


def test_choose_simulator_target_matches_connected_ipod_generation():
    targets = [
        {"id": "build-sim-video-5g", "screen_resolution": "320x240"},
        {"id": "build-sim-ipod6g", "screen_resolution": "320x240"},
        {"id": "build-sim-nano2g", "screen_resolution": "176x132"},
    ]

    classic = SimpleNamespace(rockbox_target="ipod6g")
    video = SimpleNamespace(rockbox_target="ipodvideo")

    assert desktop.DesktopModeService.choose_simulator_target(targets, classic)["id"] == "build-sim-ipod6g"
    assert desktop.DesktopModeService.choose_simulator_target(targets, video)["id"] == "build-sim-video-5g"


def test_choose_simulator_target_prefers_real_1080p_host_panel():
    targets = [
        {"id": "build-sim-ipod6g", "screen_resolution": "320x240"},
        {"id": "build-sim-desktop1080", "screen_resolution": "1920x1080"},
    ]

    chosen = desktop.DesktopModeService.choose_simulator_target(targets)

    assert chosen["id"] == "build-sim-desktop1080"


def test_ipod_engine_lists_desktop_mode_under_extras_applications():
    source = (
        Path(desktop.__file__).resolve().parents[2] / "apps/root_menu.c"
    ).read_text(encoding="utf-8")

    assert "&desktop_mode_item," in source
    assert '{ "Desktop Mode", launch_desktop_mode },' in source


def test_desktop_simulator_session_is_explicit_but_hardware_stays_dock_gated():
    root = Path(desktop.__file__).resolve().parents[2]
    menu = (root / "apps/root_menu.c").read_text(encoding="utf-8")
    plugin = (root / "apps/plugins/desktop_mode.c").read_text(
        encoding="utf-8"
    )

    assert '#define DESKTOP_MODE_SIMULATOR_TOKEN "simulator-desktop"' in menu
    assert '#define DM_SIMULATOR_LAUNCH_TOKEN "simulator-desktop"' in plugin
    assert "dm_simulator_session = parameter != NULL" in plugin
    assert "return dm_simulator_session;" in plugin
    assert "return ipod6g_videoout_active();" in menu
    assert "return dm_video_out_connected;" in plugin


def test_applications_menu_services_static_preview_after_input_settles():
    source = (
        Path(desktop.__file__).resolve().parents[2] / "apps/root_menu.c"
    ).read_text(encoding="utf-8")
    start = source.index("static int root_menu_video_applications_menu(void)")
    end = source.index(
        "struct root_menu_video_games_item",
        start,
    )
    applications = source[start:end]

    assert "IPODJS_ROOT_PREVIEW_SETTLE_DELAY" in applications
    assert "TIME_AFTER(current_tick, preview_io_settle_tick)" in applications
    assert "button_queue_count() == 0" in applications
    assert "root_menu_video_menu_preview_service(title)" in applications


def test_desktop_mode_opens_real_device_music_and_videos():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")

    assert "candidate.idxid = search.idx_id;" in source
    assert "candidate.seek = search.result_seek;" in source
    assert "rb->tagcache_retrieve(&search, idxid, tag_filename" in source
    assert "rb->file_exists(path)" in source
    assert "dm_itunes_count == page_size" in source
    assert "for (i = 0; i < dm_itunes_count; i++)" in source
    assert "rb->playlist_start(index, 0, 0);" in source
    assert "dm_itunes_source == DM_ITUNES_VIDEOS" in source
    assert '!rb->strncmp(line, "video_id\\t", 9)' in source
    assert "return dm_open_selected_file(state);" in source
    assert "(attribute & FILE_ATTR_MASK) == FILE_ATTR_AUDIO" in source
    assert "return dm_play_selected_track(state);" in source
    assert "return dm_open_itunes_selection(state);" in source
    assert "plugin_get_audio_buffer" not in source
    assert "audio_stop(" not in source


def test_ipod_itunes_starts_windowed_and_fullscreen_stays_in_desktop_mode():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")

    assert "#define DM_ITUNES_H (dm_fullscreen ? LCD_HEIGHT - DM_MENUBAR_H :" in source
    assert "#define DM_ITUNES_ASSET_H 174" in source
    assert "static void dm_draw_itunes_chrome(void)" in source
    assert "#define DM_ITUNES_ROWS ((DM_ITUNES_H - DM_ITUNES_BODY_LOCAL_Y -" in source
    assert "state->app != DM_APP_DASHBOARD" in source
    assert "state->fullscreen = !state->fullscreen;" in source


def test_native_itunes_uses_captured_geometry_and_non_overlapping_rows():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")
    draw = source.index("static void dm_draw_itunes(void)")
    native_start = source.index("#if LCD_WIDTH < 1920", draw)
    native = source[native_start : source.index("#else", native_start)]

    assert "#define DM_ITUNES_BODY_LOCAL_Y 72" in source
    assert "#define DM_ITUNES_BOTTOM_H 24" in source
    assert "#define DM_ITUNES_ROW_H (LCD_WIDTH >= 640 ? 20 : 15)" in source
    assert "#define DM_ITUNES_NOW_PANEL_H 34" in source
    assert "#define DM_ITUNES_ART_SIZE (LCD_WIDTH == 640 ? 40 : 15)" in source
    assert "#define DM_ITUNES_SCROLLER_W 15" in source
    assert "MAX(DM_ITUNES_SCROLLER_MIN_THUMB_H" in native
    assert "DM_ITUNES_ART_SIZE * sizeof(uint32_t) * 4 * 3 + 3" in source
    assert "MAX(54, DM_ITUNES_W - 180), DM_ITUNES_NOW_PANEL_H" in native
    assert "DM_ITUNES_NAME_X" in native
    assert "DM_ITUNES_ARTIST_X" in native
    assert "y + 10" not in native
    assert "title_ink, row->title" in native
    assert "dm_blit_asset_region(DM_ASSET_ITUNES_WINDOW" in source
    assert "DM_ITUNES_Y + 22, 150, 22, 1, 42" in source
    assert "DM_ITUNES_Y + 56" in source
    assert "DM_ITUNES_Y + 72" in source


def test_dashboard_uses_translucent_aurora_and_flip_card_typography():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")
    dashboard = source[
        source.index("static void dm_draw_dashboard") :
        source.index("static void dm_draw_preferences")
    ]

    assert "dm_dim_rect" in dashboard
    assert "weekdays[weekday_index]" in dashboard
    assert "dm_draw_text_scaled_centered_in_box" in dashboard
    assert "weather_x + 4" in dashboard
    assert "DM_DASH_WEATHER_ICON_SIZE 32" in source
    assert "char location[16]" in dashboard
    assert "weather_x + 46, top_y + 14" in dashboard


def test_dashboard_dock_dispatch_and_launchpad_geometry_stay_in_source_window():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")

    dock = source[
        source.index("static const enum dm_app dm_dock_apps") :
        source.index("static const char * const dm_dock_labels")
    ]
    assert "DM_APP_DASHBOARD" in dock
    assert '"Dashboard"' in source
    assert "dm_launchpad_item_rect" in source
    assert "dm_compose_scaled" in source
    assert "int label_width = MIN(item.width - 4, dm_text_width(small, name));" in source
    assert "label_x = x + icon_size / 2 - label_width / 2;" in source
    assert "dm_draw_text_in_box(small, label_x" in source
    assert "int content_x = window.x + 4;" in source
    assert "int body_y = window.y + DM_WIN_TITLE_H + DM_WIN_TOOLBAR_H;" in source
    assert "MIN(DM_DOCK_ICON, size)" in source
    assert "#define DM_NORMAL_WIN_BASE_X 8" in source
    assert "#define DM_NORMAL_WIN_BASE_Y 24" in source
    assert "MIN(DM_DOCK_ICON_Y - window.height, window.y + dy)" in source
    assert "state->app != DM_APP_DASHBOARD" in source
    assert "dm_dim_rect" in source


def test_desktop_has_finder_launchpad_menu_compact_icons_and_reflective_dock():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")

    assert "state->app == DM_APP_DESKTOP ? DM_APP_FINDER : state->app" in source
    menu_bar = source[
        source.index("static void dm_draw_menu_bar") :
        source.index("static void dm_draw_desktop_icons")
    ]
    assert "state->app != DM_APP_DASHBOARD" not in menu_bar
    assert "#define DM_DESKTOP_ICON 20" in source
    assert "DM_ACTION_DESKTOP_LAUNCHPAD" in source
    assert "dm_draw_launchpad_shortcut" in source
    assert "dm_compose_scaled(DM_ASSET_ICON_DISK" in source
    assert "#define DM_DESKTOP_ITEM_W 50" in source
    assert "#define DM_DOCK_REFLECTION_MAX_ALPHA 112" in source
    assert "static void dm_compose_dock_reflection" in source
    assert "dm_compose_dock_reflection(asset, x, y);" in source


def test_dashboard_widgets_use_cached_coverage_instead_of_opaque_bmps():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")
    dashboard = source[source.index("static void dm_draw_dashboard") :]

    for filename in (
        "world-clock.74x74x16.rga",
        "ical.104x51x16.rga",
        "weather.104x59x16.rga",
        "stickies.96x88x16.rga",
        "itunes.196x82x16.rga",
    ):
        assert filename in source
    for asset in (
        "DM_ASSET_DASH_CLOCK",
        "DM_ASSET_DASH_ICAL",
        "DM_ASSET_DASH_WEATHER",
        "DM_ASSET_DASH_STICKIES",
        "DM_ASSET_DASH_ITUNES",
    ):
        assert f"dm_compose({asset}," in dashboard
        assert f"dm_blit({asset}," not in dashboard

    assert '"iPod TV Out"' not in dashboard
    assert "dm_draw_text(small, weather_x" not in dashboard
    assert "dm_dashboard_battery" not in source
    assert "DM_ASSET_DASH_CLOCK = DM_ASSET_BMP_COUNT" in source


def test_desktop_mode_uses_real_lucida_metrics_and_metric_box_placement():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")

    assert "metrics[3] != '2'" in source
    assert "font->origin + column" in source
    assert "static int dm_font_top_in_box" in source
    assert "dm_draw_text_centered_in_box" in source
    assert "dm_draw_text_right_in_box" in source
    assert "DM_MENUBAR_H" in source
    assert "DM_WIN_TITLE_H" in source


def test_native_dock_has_eight_fixed_apps_and_optional_apps_stay_in_launchpad():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")
    dock = source[
        source.index("static const enum dm_app dm_dock_apps") :
        source.index("static const char * const dm_dock_labels")
    ]
    expected = (
        "DM_APP_FINDER",
        "DM_APP_ITUNES",
        "DM_APP_PREVIEW",
        "DM_APP_TEXTEDIT",
        "DM_APP_CALCULATOR",
        "DM_APP_DASHBOARD",
        "DM_APP_PREFERENCES",
        "DM_APP_TRASH",
    )

    assert "#define DM_DOCK_SLOTS 8" in source
    assert dock.count("DM_APP_") == len(expected)
    assert [dock.index(name) for name in expected] == sorted(
        dock.index(name) for name in expected
    )
    assert "DM_APP_STEAM" not in dock
    assert "DM_APP_NETFLIX" not in dock
    assert "DM_APP_SITEKICK" not in dock


def test_dashboard_and_launchpad_are_separate_apps():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")
    dock = source[
        source.index("static const enum dm_app dm_dock_apps") :
        source.index("static const char * const dm_dock_labels")
    ]

    assert "DM_APP_DASHBOARD" in dock
    assert "DM_APP_LAUNCHPAD" not in dock
    assert "return dm_activate_app(state, DM_APP_LAUNCHPAD);" in source
    assert "state->app == DM_APP_LAUNCHPAD" in source


def test_desktop_controls_are_bounded_and_hover_does_not_select_files():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")
    hover = source[
        source.rindex("static void dm_update_hover") :
        source.index("static bool dm_focus_move")
    ]
    click = source[
        source.index("static NO_INLINE int dm_click(struct dm_state") :
        source.index("static void dm_secondary_click")
    ]

    assert "#define DM_CONTROL_LIMIT 64" in source
    assert "struct dm_control_registry" in source
    assert "DM_ERR_CONTROL_OVERFLOW" in source
    assert "dm_file_selected =" not in hover
    assert "dm_control_at(state" in click
    assert "DM_ACTION_FILE_ROW" in click


def test_desktop_pointer_frames_use_clipped_composition_and_partial_updates():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")
    present = source[
        source.index("static void dm_present") :
        source.index("static bool dm_write_all")
    ]

    assert "dm_prepare_damage(state, full)" in source
    assert "dm_paint_clip" in source
    sink = source[source.index("static bool dm_lcd_present"):
                  source.index("static void dm_present")]
    assert "desktop_surface_present" in present
    assert "rb->lcd_bitmap_part" in sink
    assert "rb->lcd_update_rect" in sink
    assert "dirty.width * dirty.height" in source
    assert "DM_ACTION_DOCK_APP" in source


def test_desktop_has_one_cleanup_path_and_runtime_diagnostics():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")

    assert source.count("cleanup:") == 1
    assert "DM_OVERLAY_DIAGNOSTICS" in source
    assert '"Desktop Diagnostics"' in source
    assert "state->controls->high_water" in source
    assert "state->worst_frame_ticks" in source
    assert "state->partial_update_count" in source
    assert "rb->lcd_set_viewport(NULL)" in source
    assert "rb->lcd_set_foreground(saved_foreground)" in source
    assert "rb->lcd_set_background(saved_background)" in source
    assert "rb->lcd_set_drawmode(saved_drawmode)" in source


def test_simulator_autostart_follows_desktop_player_handoffs():
    source = (
        Path(desktop.__file__).resolve().parents[2] / "apps/main.c"
    ).read_text(encoding="utf-8")
    start = source.index('const char *sim_plugin = getenv("ROCKBOX_SIM_PLUGIN")')
    end = source.index("#endif", start)
    autostart = source[start:end]

    assert "open_plugin_get_entry()" in autostart
    assert "sim_plugin_rc == PLUGIN_GOTO_PLUGIN" in autostart
    assert "plugin_load(next_plugin, next_param)" in autostart
    assert "sim_plugin_loops = 100" in autostart


def test_1080p_host_target_packages_itunes_media_players():
    root = Path(desktop.__file__).resolve().parents[2]
    sources = (root / "apps/plugins/SOURCES").read_text(encoding="utf-8")
    subdirs = (root / "apps/plugins/SUBDIRS").read_text(encoding="utf-8")
    host_sources = sources[
        sources.index("#ifdef DESKTOP_1080") : sources.index("#else")
    ]
    host_subdirs = subdirs[
        subdirs.index("#ifdef DESKTOP_1080") : subdirs.index("#else")
    ]

    assert "desktop_mode.c" in host_sources
    assert "sitekick.c" in host_sources
    assert "netflix_desktop.c" in host_sources
    assert "openh264_player.c" in host_sources
    assert "mpegplayer" in host_subdirs


def test_desktop_mode_launches_sitekick_with_an_exact_desktop_underlay():
    root = Path(desktop.__file__).resolve().parents[2]
    source = (root / "apps/plugins/desktop_mode.c").read_text(
        encoding="utf-8"
    )

    assert "DM_APP_SITEKICK" in source
    assert 'PLUGIN_APPS_DIR "/sitekick.rock"' in source
    assert '"-desktop"' in source
    assert "DM_SITEKICK_UNDERLAY_MAGIC" in source
    assert "desktop_mode_sitekick_underlay.raw" in source
    assert "sitekick/desktop/icon.64x64.rga" in source


def test_desktop_mode_launches_netflix_from_bounded_launchpad_with_underlay():
    root = Path(desktop.__file__).resolve().parents[2]
    shell = (root / "apps/plugins/desktop_mode.c").read_text(
        encoding="utf-8"
    )
    netflix = (root / "apps/plugins/netflix_desktop.c").read_text(
        encoding="utf-8"
    )

    assert "DM_APP_NETFLIX" in shell
    assert 'PLUGIN_APPS_DIR "/netflix_desktop.rock"' in shell
    assert "DM_NETFLIX_UNDERLAY_MAGIC" in shell
    assert "desktop_mode_netflix_underlay.raw" in shell
    assert "netflix/desktop/icon.64x64.rga" in shell
    assert "#define DM_DOCK_SLOTS 8" in shell
    assert "DM_APP_LAUNCHPAD" in shell
    launchpad = shell[
        shell.index("static const enum dm_app dm_launchpad_apps[]") :
        shell.index("static const char * const dm_apple_menu_labels")
    ]
    assert "DM_APP_NETFLIX" in launchpad

    assert 'NF_VIDEO_INDEX ROCKBOX_DIR "/videolist/index.tsv"' in netflix
    assert 'nf_contains_ci(path, "/livetv/")' in netflix
    assert 'nf_contains_ci(path, "/live/")' in netflix
    assert 'nf_contains_ci(path, "/youtube/")' in netflix
    assert 'nf_contains_ci(path, "/downloaded/")' in netflix
    assert '"netflix:%s"' in netflix
    assert "DRMODE_FG" in netflix
    assert 'ROCKBOX_DIR "/host-pointer"' in netflix
    assert "nf_handle_click" in netflix
    assert "plugin_get_audio_buffer" not in netflix
    assert "audio_stop(" not in netflix
    assert "core_alloc(" not in netflix

    mpeg_video = (
        root / "apps/plugins/mpegplayer/video_out_rockbox.c"
    ).read_text(encoding="utf-8")
    openh264 = (
        root / "apps/plugins/openh264_player.c"
    ).read_text(encoding="utf-8")
    assert "mpegplayer_netflix_launch && buf != NULL" in mpeg_video
    assert "NETFLIX DESKTOP: source=" in mpeg_video
    assert "force_contain" in openh264


def test_prepare_device_parity_is_isolated_and_copies_exact_config(tmp_dir, monkeypatch):
    repo = Path(tmp_dir) / "repo"
    base = repo / "build-sim-desktop1080/simdisk"
    build = repo / "build-sim-desktop1080"
    binary = build / "rockboxui"
    plugin = build / "apps/plugins/desktop_mode.rock"
    sitekick_plugin = build / "apps/plugins/sitekick.rock"
    netflix_plugin = build / "apps/plugins/netflix_desktop.rock"
    binary.parent.mkdir(parents=True)
    binary.write_bytes(b"sim")
    plugin.parent.mkdir(parents=True)
    plugin.write_bytes(b"desktop-plugin")
    sitekick_plugin.write_bytes(b"sitekick-plugin")
    netflix_plugin.write_bytes(b"netflix-plugin")
    xp = base / ".rockbox/rocks.data/desktop_mode_xp/bliss.bmp"
    xp.parent.mkdir(parents=True)
    xp.write_bytes(b"old-xp")
    demo_music = base / "Music/Demo Artist/demo.mp3"
    demo_music.parent.mkdir(parents=True)
    demo_music.write_bytes(b"simulator-demo")
    demo_video_index = base / ".rockbox/videolist/index.tsv"
    demo_video_index.parent.mkdir(parents=True)
    demo_video_index.write_text("hardcoded simulator video\n", encoding="utf-8")
    (base / ".rockbox/database_0.tcd").write_bytes(b"simulator-database")
    (base / ".rockbox/.playlist_control").write_bytes(b"stale-playlist")
    (base / ".rockbox/.resume.cfg").write_bytes(b"stale-resume")

    device_root = Path(tmp_dir) / "ipod"
    pack = device_root / desktop.DEVICE_PACK_RELATIVE
    pack.mkdir(parents=True)
    (pack / "private-asset.bmp").write_bytes(b"apple-pixels")
    device_music = device_root / "Music/Real Artist/real-song.mp3"
    device_music.parent.mkdir(parents=True)
    device_music.write_bytes(b"ipod-music")
    device_video = device_root / "Videos/real-video.rvp"
    device_video.parent.mkdir(parents=True)
    device_video.write_bytes(b"ipod-video")
    device_video_index = device_root / ".rockbox/videolist/index.tsv"
    device_video_index.parent.mkdir(parents=True)
    device_video_index.write_text("real iPod video\n", encoding="utf-8")
    (device_root / ".rockbox/database_0.tcd").write_bytes(b"ipod-database")
    device_sitekick = device_root / desktop.SITEKICK_DATA_RELATIVE
    (device_sitekick / "state").mkdir(parents=True)
    (device_sitekick / "state/save.v1.dat").write_bytes(b"ipod-sitekick-save")
    device_netflix = device_root / desktop.NETFLIX_DATA_RELATIVE
    device_netflix.mkdir(parents=True)
    (device_netflix / "netflix-logo.bmp").write_bytes(b"netflix-assets")
    config = device_root / desktop.DESKTOP_CONFIG_RELATIVE
    config.parent.mkdir(parents=True, exist_ok=True)
    config.write_text("snow pointer speed: 7\n", encoding="utf-8")
    device = SimpleNamespace(
        mount_path=str(device_root),
        stable_device_key="rockbox:test-device",
        rockbox_target="ipod6g",
    )
    target = {
        "id": "build-sim-desktop1080",
        "screen_resolution": "1920x1080",
        "binary_path": str(binary),
        "simdisk_path": str(base),
        "build_dir": str(build),
    }
    monkeypatch.setattr(desktop, "validate_pack", lambda path: _valid_pack(path))

    service = desktop.DesktopModeService(repo)
    preview = service.prepare_parity_preview(
        target,
        device=device,
        use_device_pack=True,
    )

    isolated = Path(preview["simdisk_path"])
    assert isolated != base
    assert (
        isolated / desktop.DEVICE_PACK_RELATIVE / "private-asset.bmp"
    ).read_bytes() == b"apple-pixels"
    assert (
        isolated / desktop.SIMULATOR_CONFIG_RELATIVE
    ).read_text(encoding="utf-8") == "snow pointer speed: 7\n"
    assert (
        isolated / desktop.DESKTOP_PLUGIN_RELATIVE
    ).read_bytes() == b"desktop-plugin"
    assert (
        isolated / desktop.SITEKICK_PLUGIN_RELATIVE
    ).read_bytes() == b"sitekick-plugin"
    assert (
        isolated / desktop.NETFLIX_PLUGIN_RELATIVE
    ).read_bytes() == b"netflix-plugin"
    assert (
        isolated
        / desktop.SITEKICK_DATA_RELATIVE
        / "state/save.v1.dat"
    ).read_bytes() == b"ipod-sitekick-save"
    assert (
        isolated
        / desktop.NETFLIX_DATA_RELATIVE
        / "netflix-logo.bmp"
    ).read_bytes() == b"netflix-assets"
    assert (isolated / "Music").is_symlink()
    assert (
        isolated / "Music/Real Artist/real-song.mp3"
    ).read_bytes() == b"ipod-music"
    assert not (isolated / "Music/Demo Artist/demo.mp3").exists()
    assert (isolated / "Videos").is_symlink()
    assert (
        isolated / "Videos/real-video.rvp"
    ).read_bytes() == b"ipod-video"
    assert (
        isolated / ".rockbox/videolist/index.tsv"
    ).read_text(encoding="utf-8") == "real iPod video\n"
    assert (
        isolated / ".rockbox/database_0.tcd"
    ).read_bytes() == b"ipod-database"
    assert not (isolated / ".rockbox/.playlist_control").exists()
    assert not (isolated / ".rockbox/.resume.cfg").exists()
    assert not (isolated / ".rockbox/rocks.data/desktop_mode_xp").exists()
    assert xp.read_bytes() == b"old-xp"
    assert config.read_text(encoding="utf-8") == "snow pointer speed: 7\n"
    parity = json.loads(
        (Path(preview["preview_root"]) / "parity-manifest.json").read_text(
            encoding="utf-8"
        )
    )
    assert parity["source_kind"] == "connected-device"
    assert parity["config_copied"] is True
    assert parity["media_source_root"] == str(device_root.resolve())
    assert parity["linked_device_entries"] == ["Music", "Videos"]
    assert parity["tagcache_files_copied"] == ["database_0.tcd"]
    assert parity["playback_state_removed"] == [
        ".playlist_control",
        ".resume.cfg",
    ]
    assert parity["videolist_copied"] is True
    assert parity["sitekick_copied"] is True
    assert parity["netflix_assets_copied"] is True
    assert parity["isolated"] is True


def test_launch_parity_uses_direct_plugin_autostart(tmp_dir, monkeypatch):
    root = Path(tmp_dir)
    binary = root / "rockboxui"
    simdisk = root / "preview/simdisk"
    build = root / "build"
    binary.write_bytes(b"sim")
    simdisk.mkdir(parents=True)
    build.mkdir()
    captured = {}

    class Process:
        pid = 4242
        terminated = False

        def poll(self):
            return 0 if self.terminated else None

        def terminate(self):
            self.terminated = True

        def wait(self, timeout=None):
            return 0

        def kill(self):
            self.terminated = True

    def fake_popen(command, **kwargs):
        captured["command"] = command
        captured["kwargs"] = kwargs
        return Process()

    monkeypatch.setattr(desktop.subprocess, "Popen", fake_popen)
    service = desktop.DesktopModeService(root)
    result = service.launch_parity_preview(
        {
            "preview_root": str(root / "preview"),
            "simdisk_path": str(simdisk),
            "binary_path": str(binary),
            "build_dir": str(build),
            "target_id": "build-sim-ipod6g",
            "manifest": {"source_kind": "connected-device"},
        },
        screen_size=(1920, 1080),
    )

    assert result["pid"] == 4242
    assert captured["kwargs"]["env"]["ROCKBOX_SIM_PLUGIN"] == "/.rockbox/rocks/apps/desktop_mode.rock"
    assert captured["kwargs"]["env"]["ROCKBOX_SIM_PLUGIN_PARAM"] == "simulator-desktop"
    assert captured["kwargs"]["env"]["ROCKBOX_SIM_PLUGIN_EXIT"] == "1"
    assert captured["kwargs"]["env"]["RBROOT"] == str(root / "preview")
    assert "--root" in captured["command"]
    # The parity session fills the host display, and the host's own mouse
    # drives the pointer instead of the simulated click wheel.
    assert "--fullscreen" in captured["command"]
    assert "--zoom" not in captured["command"]
    assert result["fullscreen"] is True
    assert captured["kwargs"]["env"]["ROCKPOD_SIM_HOST_POINTER"].endswith(
        "host-pointer"
    )
    # A windowed session instead scales by a whole number: 1920x1080 fits four
    # whole 320x240 panels, so no Apple pixel is resampled.
    service.launch_parity_preview(
        {
            "preview_root": str(root / "preview"),
            "simdisk_path": str(simdisk),
            "binary_path": str(binary),
            "build_dir": str(build),
            "target_id": "build-sim-ipod6g",
            "manifest": {"source_kind": "connected-device"},
        },
        screen_size=(1920, 1080),
        fullscreen=False,
    )
    assert captured["command"][captured["command"].index("--zoom") + 1] == "4"
    assert service.stop_host_previews() == 2


def test_biggest_wins_keep_dock_and_window_lifecycle_consistent():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")

    dock_start = source.index(
        "if (!state->fullscreen && state->app != DM_APP_LAUNCHPAD"
    )
    dock_registration = source[dock_start : dock_start + 420]
    assert "state->app != DM_APP_DASHBOARD" in dock_registration
    assert "state->app != DM_APP_ITUNES" not in dock_registration

    assert "#define DM_ITUNES_SCROLLER_MIN_THUMB_H" in source
    assert source.count("DM_ITUNES_SCROLLER_MIN_THUMB_H") >= 3

    menu_start = source.index("static void dm_menu_action")
    menu = source[menu_start : source.index("static bool dm_button", menu_start)]
    assert "if (dm_app_has_window(state->app))" in menu
    assert "dm_start_animation(state, state->app, true);" in menu


def test_next_visual_pass_has_album_tiles_focus_restore_and_tv_safe_margin():
    source = (
        Path(desktop.__file__).resolve().parents[2]
        / "apps/plugins/desktop_mode.c"
    ).read_text(encoding="utf-8")

    assert "#define DM_ITUNES_ALBUM_COLUMNS 2" in source
    assert "#define DM_ITUNES_ALBUM_ROWS (LCD_WIDTH == 640 ? 6 : 2)" in source
    assert "DM_ITUNES_ALBUM_TILES" in source
    assert "dm_itunes_album_item_rect" in source
    assert '"Albums"' in source
    assert '"Unknown Artist"' in source

    assert "static void dm_close_active_window" in source
    close_start = source.index("static void dm_close_active_window")
    close = source[close_start : source.index("static enum dm_asset_id", close_start)]
    assert "state->window_stack[state->window_stack_count - 1]" in close
    assert "dm_stack_raise(state, state->app);" in close

    assert "#define DM_SOURCE_SAFE_MARGIN 8" in source
    assert "DM_SOURCE_SAFE_MARGIN - window.width" in source
