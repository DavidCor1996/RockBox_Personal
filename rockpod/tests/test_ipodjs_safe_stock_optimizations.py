from __future__ import annotations

from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def _text(relative: str) -> str:
    return (REPO / relative).read_text(encoding="utf-8")


def _section(text: str, start: str, end: str) -> str:
    return text.split(start, 1)[1].split(end, 1)[0]


def test_wps_renderer_is_damage_driven_and_playback_read_only():
    source = _text("apps/root_menu.c")
    renderer = _section(
        source,
        "void root_menu_ipodjs_draw_wps_frame(void)",
        "static void ipodjs_video_draw_wps_empty_state",
    )

    assert "root_menu_video_wps_capture_state" in renderer
    assert "lcd_update_rect" in renderer
    assert "root_menu_video_wps_force_full" in renderer
    for forbidden in (
        "audio_stop(",
        "audio_pause(",
        "audio_resume(",
        "playlist_start(",
        "playlist_resume(",
        "pcmbuf_",
        "pcm_mixer_",
    ):
        assert forbidden not in renderer


def test_standard_lists_use_incremental_damage_with_safe_fallbacks():
    source = _text("apps/gui/bitmap/list.c")

    assert "struct list_ipodjs_render_cache" in source
    assert "display->update_rect" in source
    assert "!main_menu_config && !list->callback_draw_item" in source
    assert "display->update();" in source


def test_fast_scroll_requires_verified_private_apple_assets():
    list_source = _text("apps/gui/list.c")
    ui_source = _text("apps/gui/ipodjs_ui.c")
    root_source = _text("apps/root_menu.c")
    prep_source = _text("tools/prepare_ipodjs_apple_assets.py")
    buildzip = _text("tools/buildzip.pl")

    assert "IPODJS_FAST_SCROLL_ACCEL_TRIGGER 2" in list_source
    assert (
        "ipodjs_ui_fast_scroll_active() ||"
        in list_source
    )
    assert "action != ACTION_STD_PREV &&" in list_source
    assert "action != ACTION_STD_NEXT &&" in list_source
    assert "struct list_ipodjs_fast_scroll_index" in list_source
    assert "list_ipodjs_fast_scroll_step" in list_source
    assert "IPODJS_FAST_SCROLL_MIN_ITEMS 12" in list_source
    assert "IPODJS_FAST_SCROLL_MIN_ALBUM_ARTIST_ITEMS 3" in list_source
    assert "list_ipodjs_fast_scroll_is_album_artist_title" in list_source
    assert '"Artists by First Letter"' in list_source
    assert '"Album Artists"' in list_source
    assert '!strcmp(title, "Title")' in list_source
    assert "str(LANG_ID3_ALBUMARTIST)" in list_source
    assert "str(LANG_ID3_ALBUMARTISTS)" in list_source
    assert "str(LANG_SHOW_ALBUM_ARTISTS_BY_FIRST_LETTER)" in list_source
    assert '"/23-Helvetica-Apple.fnt"' in ui_source
    assert "ipodjs_ui_fast_scroll_available()" in list_source
    assert "fast-scroll-blank.apple.95x82x32.bmp" in ui_source
    assert "fast-scroll-123.apple.95x82x32.bmp" in ui_source
    assert "ipodjs_ui_fast_scroll_deadline = current_tick + HZ" in ui_source
    assert "FG|IMG blends Apple's translucent overlay" in ui_source
    assert '"Album Artists", -IPODJS_MUSIC_NATIVE_ALBUM_ARTISTS' in root_source
    assert "tag_albumartist" in root_source
    assert "root_menu_video_db_fast_scroll_build" in root_source
    assert "root_menu_video_db_fast_scroll_move" in root_source
    assert "IPODJS_DB_FAST_SCROLL_TRIGGER 2" in root_source
    assert "No artwork is traced, redrawn, interpolated, or resampled" in prep_source
    assert "IPSW_SHA256" in prep_source
    assert "GUIDE_SHA256" in prep_source
    assert '"-s", "65", "-l", "90"' in prep_source
    assert 'ROOT / "tools/convttf", "-B"' in prep_source
    assert 'tree_copy("$ROOT/assets/ipodjs/apple"' in buildzip

    fixture_source = _text("tools/ipodjs_fast_scroll_fixture.py")
    regression_source = _text("tools/ipodjs_fast_scroll_sim_regression.sh")
    assert 'ALBUM_ARTIST_NAMES = (NAMES[0], "Bírch", *NAMES[2:8])' in fixture_source
    assert "assert_short_album_artist_fixture" in regression_source


def test_fast_scroll_folds_supported_latin_metadata_and_rejects_other_scripts():
    ui_source = _text("apps/gui/ipodjs_ui.c")
    list_source = _text("apps/gui/list.c")
    root_source = _text("apps/root_menu.c")

    assert "int ipodjs_ui_fast_scroll_bucket(const char *text)" in ui_source
    assert "utf8decode(name, &ch)" in ui_source
    assert "ch >= 0x00c0 && ch <= 0x00c6" in ui_source
    assert "ch >= 0x0179 && ch <= 0x017e" in ui_source
    assert "return -1;" in ui_source
    assert "return ipodjs_ui_fast_scroll_bucket" in list_source
    assert "if (!list_ipodjs_fast_scroll_build(list))" in list_source
    assert "return ipodjs_ui_fast_scroll_bucket(label);" in root_source
    assert "if (bucket < 0)" in root_source


def test_simulator_can_inject_realistic_wheel_velocity_for_fast_scroll_gate():
    source = _text("firmware/target/hosted/sdl/button-sdl.c")

    assert 'getenv("ROCKPOD_SIM_WHEEL_VELOCITY")' in source
    assert 'getenv("ROCKPOD_SIM_WHEEL_VELOCITY_GATE")' in source
    assert "velocity <= 0xffffff" in source
    assert "1u << 31" in source


def test_simulator_can_hold_play_without_x11_auto_release():
    source = _text("firmware/target/hosted/sdl/button-sdl.c")

    assert 'getenv("ROCKPOD_SIM_PLAY_HOLD_GATE")' in source
    assert "return btn | BUTTON_PLAY;" in source


def test_simulator_hold_switch_gate_is_deterministic():
    source = _text("firmware/target/hosted/sdl/button-sdl.c")
    regression = _text("tools/ipodjs_navigation_sim_regression.sh")

    assert 'getenv("ROCKPOD_SIM_HOLD_GATE")' in source
    assert 'touch "${runtime_root}/hold.gate"' in regression
    assert 'rm -f "${runtime_root}/hold.gate"' in regression
    assert 'ROCKPOD_SIM_HOLD_GATE="${runtime_root}/hold.gate"' in regression


def test_stock_search_is_asset_gated_and_keeps_browsing_playback_safe():
    root_source = _text("apps/root_menu.c")
    tagtree_source = _text("apps/tagtree.c")
    prep_source = _text("tools/prepare_ipodjs_apple_assets.py")
    regression = _text("tools/ipodjs_search_sim_regression.sh")

    search_build = _section(
        root_source,
        "static void root_menu_video_search_add_tag",
        "static void root_menu_video_search_draw_input",
    )
    search_draw = _section(
        root_source,
        "static void root_menu_video_search_draw(",
        "static int root_menu_video_search_open_result",
    )
    for forbidden in (
        "audio_stop(",
        "audio_pause(",
        "audio_resume(",
        "playlist_start(",
        "pcmbuf_",
        "pcm_mixer_",
    ):
        assert forbidden not in search_build
        assert forbidden not in search_draw

    assert '!strcasecmp(P2STR((unsigned char *)dptr->name), "Search")' in tagtree_source
    assert "root_menu_ipodjs_search_available()" in tagtree_source
    assert "root_menu_ipodjs_search()" in tagtree_source
    assert "tagcache_search_add_clause" in search_build
    assert "source_constant" in search_build
    assert "root_menu_video_font()" in root_source
    assert "ipodjs_ui_search_surfaces_available()" in root_source
    assert "root_menu_video_search_round_fill" not in root_source
    assert "ipodjs_ui_draw_search_surface" in root_source
    assert "surface == IPODJS_UI_SEARCH_PANEL ? 16 : 6" in _text(
        "apps/gui/ipodjs_ui.c"
    )
    assert "int selected_w = MAX(glyph_w + 6, 16);" in root_source
    assert "entering ? character : selected" in root_source
    assert "wait_for_search_character" in regression
    assert "capture search-i-input" in regression
    assert "Search wheel did not expose every A-Z character" in regression

    for resource_id, asset in {
        22203: "search-field.apple.97x32x24.bmp",
        22206: "search-selected.apple.97x32x24.bmp",
    }.items():
        assert f"{resource_id}:" in prep_source
        assert asset in prep_source
        assert asset in _text("apps/gui/ipodjs_ui.c")

    expected = {
        30240: "search-song.apple.16x16x24.bmp",
        30242: "search-artist.apple.16x16x24.bmp",
        30244: "search-album.apple.16x16x24.bmp",
        30248: "search-playlist.apple.16x16x24.bmp",
    }
    for resource_id, asset in expected.items():
        assert f"{resource_id}:" in prep_source
        assert asset in prep_source
        assert asset in root_source
        assert asset in regression

    assert 'wait_for_record "${before}" wps "Now Playing"' in regression
    assert (
        "live X search did not return artist, album, song, and playlist"
        in regression
    )


def test_ipod6g_stock_music_history_and_search_bounds_are_lifecycle_safe():
    root_source = _text("apps/root_menu.c")
    wps_entry = _section(
        root_source,
        "static int wpsscrn(void* param)",
        "static int radio(void* param)",
    )
    search_add = _section(
        root_source,
        "static void root_menu_video_search_add_tag",
        "static void root_menu_video_search_build",
    )
    search_build = _section(
        root_source,
        "static void root_menu_video_search_build",
        "static void root_menu_video_search_draw_input",
    )

    assert "#ifdef IPOD_6G\n    return true;" in root_source
    assert "!root_menu_video_uses_stock_music() &&" in wps_entry
    assert "frame.coverflow ? GO_TO_PICTUREFLOW" in wps_entry
    assert "root_menu_video_search_total >" in search_add
    assert "IPODJS_SEARCH_MAX_RESULTS)\n            break;" in search_add
    assert "IPODJS_SEARCH_MAX_RESULTS &&" in search_build
    assert "root_menu_video_search_total >" in search_build
    regression = _text("tools/ipodjs_search_sim_regression.sh")
    assert 'stress_cycles="${IPODJS_SEARCH_STRESS_CYCLES:-8}"' in regression
    assert "stock iPod 6G music route used an iPodJS database history frame" in regression


def test_failed_tagcache_index_open_closes_partial_search_and_can_retry():
    tagcache = _text("apps/tagcache.c")
    regression = _text("tools/ipodjs_navigation_sim_regression.sh")
    search = _section(
        tagcache,
        "bool tagcache_search(struct tagcache_search *tcs, int tag)",
        "void tagcache_search_set_uniqbuf",
    )
    finish = _section(
        tagcache,
        "void tagcache_search_finish(struct tagcache_search *tcs)",
        "#if defined(HAVE_TC_RAMCACHE)",
    )

    assert "static void tagcache_search_close_files" in tagcache
    assert "tagcache_search_close_files(tcs);" in search
    assert "transient index failure" in search
    assert "tagcache_search_close_files(tcs);" in finish
    assert 'ROCKBOX_DIR "/tagcache-open-fail.gate"' in tagcache
    assert "IPODJS_NAVIGATION_TAGCACHE_FAULT" in regression
    assert "failed tagcache open leaked a descriptor" in regression


def test_photos_uses_stock_numeric_wheel_pin_and_ipodjs_theme_mode():
    photos = _text("apps/plugins/photos.c")
    regression = _text("tools/ipodjs_photos_sim_regression.sh")

    assert 'static const char digits[] = "0123456789";' in photos
    assert "photos_prompt_pin_wheel" in photos
    assert "photos_pin_ipod_ctx" in photos
    assert "fast-scroll-blank.apple.95x82x32.bmp" in photos
    assert "search-field.apple.97x32x24.bmp" in photos
    assert "search-selected.apple.97x32x24.bmp" in photos
    assert "photos_draw_pin_surface(&photos_pin_surfaces.panel, 16" in photos
    assert "ui_engine_dark_mode" in photos
    assert "photos_background()" in photos
    assert "photos_tile_background()" in photos
    assert "photos_tile_border(selected)" in photos
    assert "Locked.bmp|1234" in regression
    assert "IPODJS_PHOTOS_DARK" in regression
    assert "numeric PIN did not open the locked photo" in regression


def test_plugin_and_utility_browsers_reuse_the_common_stock_list_path():
    plugin_menu = _text("apps/menus/plugin_menu.c")
    list_source = _text("apps/gui/bitmap/list.c")
    menu_source = _text("apps/menu.c")
    regression = _text("tools/ipodjs_navigation_sim_regression.sh")

    assert "rockbox_browse(&browse)" in plugin_menu
    assert "list_ipodjs_enabled(display, list)" in list_source
    assert "list_ipodjs_draw(display, list)" in list_source
    assert "ipodjs_ui_transition_begin(1)" in menu_source
    assert "ipodjs_ui_transition_begin(-1)" in menu_source
    assert 'IPODJS_NAVIGATION_UTILITIES:-0' in regression
    assert 'hold_cycle "00-extras"' in regression
    hold_overlay = _section(
        _text("apps/root_menu.c"),
        "static void ipodjs_video_draw_hold_overlay(void)",
        "bool root_menu_ipodjs_handle_lockscreen(void)",
    )
    assert "lcd_set_viewport(NULL);" in hold_overlay


def test_status_renderer_is_shared_and_theme_references_are_not_packaged():
    root_source = _text("apps/root_menu.c")
    list_source = _text("apps/gui/bitmap/list.c")
    ui_source = _text("apps/gui/ipodjs_ui.c")
    buildzip = _text("tools/buildzip.pl")

    assert "ipodjs_ui_draw_header_background" in root_source
    assert "ipodjs_ui_draw_header_battery" in root_source
    assert "ipodjs_ui_draw_header_background" in list_source
    assert "ipodjs_ui_draw_header_battery" in list_source
    assert "status-battery.apple.26x65x24.bmp" in ui_source
    assert "status-header.apple.320x24x24.bmp" in ui_source
    assert "status-playback.apple.20x32x24.bmp" in ui_source
    assert "status-hold.apple.12x15x24.bmp" in ui_source
    assert "status-repeat.apple.21x38x24.bmp" in ui_source
    assert "status-shuffle.apple.21x19x24.bmp" in ui_source
    assert "#define IPODJS_STOCK_HEADER_H      24" in ui_source
    assert "status-(?:battery|playing|hold|header|repeat|shuffle)-stock" in buildzip
    assert "alphabet-overlay-stock" in buildzip
    assert "24-iLike" in buildzip


def test_apple_status_assets_are_exact_verified_firmware_resources():
    prep_source = _text("tools/prepare_ipodjs_apple_assets.py")

    for resource_id in (24263, 24265, 24266, 24267, 24379, 25552, 25554):
        assert f"{resource_id}:" in prep_source
    assert "find_resource(images, 24379, 320, 24)" in prep_source
    assert "find_resource(images, 25552, 20, 16)" in prep_source
    assert "find_resource(images, 25554, 20, 16)" in prep_source
    assert "find_resource(images, 24263, 12, 15)" in prep_source
    assert "find_resource(images, 24265, 21, 19)" in prep_source
    assert "find_resource(images, 24266, 21, 19)" in prep_source
    assert "find_resource(images, 24267, 21, 19)" in prep_source


def test_music_preview_always_keeps_default_album_slideshow():
    source = _text("apps/root_menu.c")
    preview = _section(
        source,
        "static void root_menu_video_draw_preview_for_title",
        "static void root_menu_video_draw_clock_date",
    )

    assert "albumlist_draw_slideshow" in preview
    assert "root_menu_video_draw_stock_wps_art" not in preview
    assert "audio_current_track()" not in preview
    for forbidden in ("audio_stop(", "audio_pause(", "playlist_start("):
        assert forbidden not in preview


def test_app_weather_calm_and_maps_use_real_cached_preview_assets():
    source = _text("apps/root_menu.c")

    assert '"weather-loop"' in source
    assert '"maps-globe"' in source
    assert '"/calm/calm-icon.64x64x24.bmp"' in source
    assert "root_menu_video_draw_weather_preview" in source
    assert "root_menu_video_draw_calm_preview" in source
    assert "root_menu_video_draw_maps_preview" in source
    assert "root_menu_video_custom_preview_animation_due" in source

    service = source.rsplit(
        "static bool root_menu_video_animated_preview_service", 1
    )[1].split(
        "static struct root_menu_video_menu_preview_slot *", 1
    )[0]
    assert "read_bmp_file" in service
    missing = service.split("if (!file_exists(path))", 1)[1].split(
        "if (root_menu_video_menu_preview_recent_failure", 1
    )[0]
    assert "root_menu_video_menu_preview_record_failure" not in missing
    assert "Sync Weather for radar" not in source
    for draw_name, next_name in (
        ("static void root_menu_video_draw_weather_preview",
         "static void root_menu_video_draw_calm_preview"),
        ("static void root_menu_video_draw_calm_preview",
         "static void root_menu_video_draw_maps_preview"),
        ("static void root_menu_video_draw_maps_preview",
         "static bool root_menu_video_custom_preview_animation_due"),
    ):
        draw = source.rsplit(draw_name, 1)[1].split(next_name, 1)[0]
        assert "read_bmp_file" not in draw
        assert "file_exists" not in draw
        assert "core_alloc" not in draw

    assert (
        REPO / "assets/ipodjs/rockbox/calm/calm-icon.64x64x24.bmp"
    ).is_file()
    globe = REPO / "assets/ipodjs/rockbox/previews/maps-globe"
    assert len(list(globe.glob("frame-*.bmp"))) == 12


def test_ipodjs_album_browser_keeps_stock_sized_cover_rows():
    source = _text("apps/gui/albumlist_art.c")
    setup = source.split("void albumlist_setup_list", 1)[1]

    ipodjs_guard = setup.index("global_settings.ui_engine == UI_ENGINE_IPODJS")
    fullscreen = setup.index("gui_synclist_set_fullscreen_albumlist(list, true)")
    assert ipodjs_guard < fullscreen
    ipodjs_body = setup[ipodjs_guard:fullscreen]
    assert "list->callback_draw_item = albumlist_art_draw_item;" in ipodjs_body
    assert "ALBUMLIST_ROW_HEIGHT" in ipodjs_body
    assert "tagtree_get_album_art_path" in source


def test_apple_progress_frame_blends_rounded_alpha_against_live_surface():
    source = _text("apps/root_menu.c")
    renderer = _section(
        source,
        "static bool root_menu_video_draw_apple_track",
        "static bool root_menu_video_draw_apple_slider",
    )

    assert "old_drawmode = lcd_get_drawmode();" in renderer
    assert "lcd_set_drawmode(DRMODE_FG);" in renderer
    assert "lcd_set_drawmode(old_drawmode);" in renderer


def test_quick_settings_is_full_width_and_traceable():
    source = _text("apps/root_menu.c")
    renderer = _section(
        source,
        "static void root_menu_video_draw_quick_settings",
        "enum root_menu_video_cache_item",
    )

    assert 'root_menu_video_draw_status_title("Quick Settings")' in renderer
    assert "lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH" in renderer
    assert 'ipodjs_trace_screen("Quick Settings", "full"' in renderer


def test_wps_modes_are_drawn_after_the_surface_and_use_bounded_damage():
    source = _text("apps/root_menu.c")
    full = _section(
        source,
        "static void ipodjs_video_draw_wps_full(void)",
        "struct root_menu_video_wps_render_state",
    )

    assert full.index("lcd_fillrect(0, IPODJS_HEADER_HEIGHT") < full.index(
        "root_menu_video_draw_wps_modes();"
    )
    assert "IPODJS_HEADER_HEIGHT + 20" in source
    assert "ipodjs_ui_draw_shuffle_indicator" in source
    assert "ipodjs_ui_draw_repeat_indicator" in source


def test_shutdown_crt_closes_over_the_live_frame_without_wps_redraw():
    source = _text("apps/gui/ipodjs_ui.c")
    animation = _section(
        source,
        "void ipodjs_ui_shutdown_animation(void)",
        "unsigned ipodjs_ui_rgb_blend",
    )

    assert "Close a pair of black shutters over the live iPodJS frame" in animation
    assert 'ipodjs_trace_screen("Shutdown CRT", "band"' in animation
    assert 'ipodjs_trace_screen("Shutdown CRT", "line"' in animation
    assert 'ipodjs_trace_screen("Shutdown CRT", "black"' in animation
    assert "root_menu_video_draw" not in animation


def test_apple_battery_mapping_is_bounded_to_five_documented_frames():
    ui_source = _text("apps/gui/ipodjs_ui.c")

    assert "frame = level >= 100 ? 4 : 3" in ui_source
    assert "frame = 0" in ui_source
    assert "frame = 1" in ui_source
    assert "frame = 2" in ui_source
    assert "frame * 13" in ui_source
    assert "26, 13" in ui_source
    assert "STOCK_BATTERY_LAST_FRAME" not in ui_source


def test_preview_cache_is_right_sized_for_the_320x240_target():
    source = _text("apps/root_menu.c")
    assert "#define IPODJS_PREVIEW_IMAGE_SIZE 240" in source


def test_pictureflow_return_uses_typed_navigation_origin():
    source = _text("apps/root_menu.c")
    assert "struct ipodjs_ui_origin_frame" in source
    assert "ipodjs_ui_origin_push" in source
    assert "ipodjs_ui_origin_pop" in source
    assert "IPODJS_UI_ORIGIN_PICTUREFLOW" in source
    assert "ipodjs_wps_from_pictureflow" not in source


def test_preview_paths_have_one_active_table_not_per_source_duplicates():
    source = _text("apps/root_menu.c")
    cache = _section(
        source,
        "struct root_menu_video_preview_source_cache",
        "struct root_menu_video_preview_failure",
    )
    assert "paths[" not in cache
    assert "root_menu_video_preview_paths[IPODJS_PREVIEW_MAX_ITEMS]" in source


def test_lockscreen_keeps_clock_weather_and_packages_large_font():
    source = _text("apps/root_menu.c")
    lock_overlay = _section(
        source,
        "static void ipodjs_video_draw_hold_overlay(void)",
        "bool root_menu_ipodjs_handle_lockscreen(void)",
    )
    buildzip = _text("tools/buildzip.pl")

    assert "root_menu_video_draw_clock_lock();" in lock_overlay
    assert "root_menu_video_draw_slanted_cached_art" not in lock_overlay
    assert "root_menu_video_draw_lock_weather(144)" in source
    assert 'copy("$ROOT/fonts/35-Adobe-Helvetica-Bold.fnt"' in buildzip


def test_apple_adjustment_assets_are_exact_verified_firmware_resources():
    prep_source = _text("tools/prepare_ipodjs_apple_assets.py")
    root_source = _text("apps/root_menu.c")

    expected = {
        24282: (11, 17),
        24283: (19, 18),
        24286: (20, 21),
        24287: (30, 31),
        24292: (248, 20),
        24293: (248, 20),
        24343: (316, 20),
        30239: (200, 22),
    }
    for resource_id, (width, height) in expected.items():
        assert f"{resource_id}:" in prep_source
        assert (
            f"find_resource(images, {resource_id}, {width}, {height})"
            in prep_source
        )

    for asset in (
        "volume-low.apple.11x17x24.bmp",
        "volume-high.apple.19x18x24.bmp",
        "brightness-low.apple.20x21x32.bmp",
        "brightness-high.apple.30x31x32.bmp",
        "slider-light.apple.248x20x32.bmp",
        "slider-dark.apple.248x20x32.bmp",
        "slider-fill.apple.316x20x24.bmp",
        "progress-frame.apple.200x22x32.bmp",
        "progress-fill.apple.200x22x32.bmp",
        "progress-fill-cap.apple.16x16x24.bmp",
    ):
        assert asset in prep_source
        assert asset in root_source

    # The 32-bit IPSW resources carry a packed 4-bit alpha plane.
    assert "BM_SIZE(248, 20, FORMAT_NATIVE, false) + 248 * 20 / 2" in root_source
    assert root_source.index("lcd_bmp(frame, frame_x, frame_y);") < root_source.index(
        "lcd_bmp_part(fill, 2, 2"
    )
    assert "lcd_bmp(fill_cap, frame_x + draw_w - cap_w, frame_y + 3);" in root_source


def test_stock_wps_uses_exact_progress_frame_and_scrolls_long_titles():
    source = _text("apps/root_menu.c")

    assert "IPODJS_APPLE_PROGRESS_FRAME" in source
    assert "root_menu_video_draw_apple_track(60, 198" in source
    assert "root_menu_video_draw_apple_track(progress_x, progress_y" in source
    assert "gui_scrollable_puts" not in source
    assert "root_menu_video_update_wps_title_scroll" in source
    assert "static const unsigned char scroll_ticks[18]" in source
    assert "global_settings.scroll_speed" in source
    assert "global_settings.scroll_step" in source
    assert "display->putsxy(-offset, 0" in source
    assert 'ipodjs_trace_screen("Now Playing", "title-scroll"' in source


def test_hierarchical_transitions_are_bounded_and_fail_safe():
    ui_source = _text("apps/gui/ipodjs_ui.c")
    list_source = _text("apps/gui/bitmap/list.c")
    menu_source = _text("apps/menu.c")

    assert "ipodjs_ui_transition_deadline = current_tick + HZ" in ui_source
    assert "new_handle = core_alloc(FRAMEBUFFER_SIZE)" in ui_source
    assert "if (new_handle <= 0)" in ui_source
    assert "ipodjs_ui_transition_cancel();" in ui_source
    assert "(audio_status() & AUDIO_STATUS_PLAY)" in ui_source
    assert "use a direct screen handoff whenever an" in ui_source
    assert "enum { TRANSITION_FRAMES = 9 }" in ui_source
    assert 'ipodjs_trace_screen("Transition"' in ui_source
    assert "ipodjs_ui_transition_present(display)" in list_source
    assert "ipodjs_ui_transition_begin(1)" in menu_source
    assert "ipodjs_ui_transition_begin(-1)" in menu_source
    for forbidden in ("audio_stop(", "audio_pause(", "pcmbuf_", "pcm_mixer_"):
        assert forbidden not in _section(
            ui_source,
            "void ipodjs_ui_transition_cancel(void)",
            "bool ipodjs_ui_charging_animation_active",
        )


def test_ipodjs_database_failure_uses_marker_aware_recovery():
    tagcache_source = _text("apps/tagcache.c")
    tagtree_source = _text("apps/tagtree.c")

    recovery = _section(
        tagcache_source,
        "bool tagcache_revalidate(void)",
        "#ifdef HAVE_TC_RAMCACHE",
    )
    assert "valid = check_all_headers();" in recovery
    assert "tc_stat.ready = true;" in recovery
    for forbidden in ("remove_files(", "tagcache_build(", "commit("):
        assert forbidden not in recovery

    transaction_recovery = _section(
        tagcache_source,
        "bool tagcache_recover(void)",
        "#ifdef HAVE_TC_RAMCACHE",
    )
    assert "return tagcache_revalidate();" in transaction_recovery
    assert "db_file_exists(TAGCACHE_FILE_COMMIT)" in transaction_recovery
    assert "db_master_state() == 1" in transaction_recovery
    assert "tagcache_backup_valid()" in transaction_recovery
    assert "queue_post(&tagcache_queue, Q_RECOVER, 0);" in transaction_recovery
    assert "db_file_exists(TAGCACHE_FILE_HOSTCOMMIT)" in transaction_recovery
    assert "Q_REBUILD" not in transaction_recovery

    assert 'TAGCACHE_BACKUP_DIRECTORY "tagcache_backup"' in tagcache_source
    assert "static bool tagcache_restore_backup(void)" in tagcache_source
    assert "unsigned char buffer[512];" in tagcache_source
    assert "case Q_RECOVER:" in tagcache_source

    deploy_source = _text("tools/deploy_ipod6g_preserve_database.sh")
    assert "install_recovery_snapshot" in deploy_source
    assert '.rockbox/tagcache_backup' in deploy_source

    assert 'splash(0, "Loading Music...")' in tagtree_source
    assert "tagcache_recover() && tagcache_search(tcs, tag)" in tagtree_source
    assert "tagtree_search_with_recovery(&tcs, tag)" in tagtree_source


def test_pictureflow_animation_is_tick_normalized():
    source = _text("apps/plugins/pictureflow/pictureflow.c")

    assert "static long scroll_animation_tick;" in source
    assert "static long cover_animation_tick;" in source
    assert "elapsed = now - scroll_animation_tick;" in source
    assert "elapsed = MIN(elapsed, MAX(1, HZ / 10));" in source
    assert "slide_frame += speed * elapsed * step;" in source
    assert "if (now == cover_animation_tick)" in source


def test_menu_from_apple_slider_returns_to_quick_settings_first():
    source = _text("apps/root_menu.c")
    quick_settings = _section(
        source,
        "static int root_menu_video_quick_settings(void)\n{",
        "struct root_menu_video_settings_item",
    )

    menu_case = quick_settings.split("case ACTION_STD_MENU:", 1)[1]
    assert "if (adjusting)" in menu_case
    assert "adjusting = false;" in menu_case
    assert menu_case.index("adjusting = false;") < menu_case.index(
        "return GO_TO_ROOT;"
    )
