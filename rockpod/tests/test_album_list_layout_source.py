"""Static checks for the Classic-style database album list layout."""

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(rel_path):
    return (REPO_ROOT / rel_path).read_text(encoding="utf-8", errors="replace")


def test_album_list_setup_uses_stock_sized_fullscreen_cover_rows():
    albumlist = _read("apps/gui/albumlist_art.c")

    assert "#define ALBUMLIST_THUMB_SIZE 40" in albumlist
    assert "#define ALBUMLIST_COMPACT_THUMB_SIZE 24" in albumlist
    assert "#define ALBUMLIST_ROW_HEIGHT 44" in albumlist
    assert "global_settings.album_list_layout" in albumlist
    compact_check = "global_settings.album_list_layout == ALBUM_LIST_LAYOUT_COMPACT"
    assert compact_check in albumlist
    setup_body = albumlist.split("void albumlist_setup_list", 1)[1]
    assert setup_body.index(compact_check) < setup_body.index(
        "gui_synclist_set_fullscreen_albumlist(list, true);"
    )
    assert "gui_synclist_set_fullscreen_albumlist(list, true);" in albumlist
    assert "list->callback_get_item_icon = NULL;" in albumlist
    assert "list->show_icons = false;" in albumlist
    assert "list->callback_draw_item = albumlist_art_draw_item;" in albumlist
    assert "lookup_thumb_path(album, artist, path, sizeof(path))" in albumlist
    assert "load_thumb_bitmap(path, thumb_size)" in albumlist
    assert "#define ALBUMLIST_SCALED_BYTES(width, height)" in albumlist
    assert "BM_SCALED_SIZE(width, height, FORMAT_NATIVE, false)" in albumlist
    assert "ALBUMLIST_SCALE_SCRATCH_EXTRA(width)" in albumlist
    assert "ALBUMLIST_SCALED_BYTES(ALBUMLIST_THUMB_SIZE" in albumlist
    assert "ALBUMLIST_SCALED_BYTES(ALBUMLIST_SLIDESHOW_SIZE" in albumlist
    assert "FORMAT_NATIVE | FORMAT_RESIZE |" in albumlist
    assert "albumlist_art_draw_item_compact" in albumlist
    assert "list->callback_draw_item = albumlist_art_draw_item_compact;" in albumlist
    assert "albumlist_draw_item_with_art(list_info, ALBUMLIST_THUMB_SIZE" in albumlist
    assert "albumlist_draw_item_with_art(list_info, ALBUMLIST_COMPACT_THUMB_SIZE" in albumlist
    assert "text_info.item_indent += bm->width + text_pad;" in albumlist
    assert "albumlist_get_album_row" in albumlist
    assert "return album && album[0] != '\\0';" in albumlist
    assert "tc->selected_item = first_album_row;" in albumlist
    assert "list->fullscreen_albumlist_first_item = MAX(0, first_album_row);" in albumlist
    assert "gui_synclist_select_item(list, first_album_row);" in albumlist
    assert "list->start_item[i] = first_album_row;" in albumlist


def test_album_list_layout_resets_when_leaving_album_rows():
    albumlist = _read("apps/gui/albumlist_art.c")
    tree = _read("apps/tree.c")

    assert "list->show_icons = global_settings.show_icons;" in albumlist
    assert "gui_synclist_set_fullscreen_albumlist(list, false);" in albumlist
    assert "albumlist_has_album_rows(list)" in albumlist
    assert "albumlist_setup_list(list);" in tree
    assert "if (id3db)" not in tree.split("albumlist_setup_list(list);", 1)[0][-80:]


def test_album_list_layout_setting_is_theme_setting():
    settings_h = _read("apps/settings.h")
    settings_list = _read("apps/settings_list.c")
    theme_menu = _read("apps/menus/theme_menu.c")

    assert "enum album_list_layout" in settings_h
    assert "ALBUM_LIST_LAYOUT_FULL = 0" in settings_h
    assert "ALBUM_LIST_LAYOUT_COMPACT = 1" in settings_h
    assert "int album_list_layout;" in settings_h
    assert "CHOICE_SETTING(F_THEMESETTING, album_list_layout" in settings_list
    assert '"album list layout", "full,compact"' in settings_list
    assert "ALBUM_LIST_LAYOUT_FULL" in settings_list
    assert "LANG_ALBUM_LIST_LAYOUT" in settings_list
    assert "ipone_right_pane, LANG_IPONE_RIGHT_PANE" in settings_list
    assert "LANG_IPONE_RIGHT_PANE" in _read("apps/lang/english.lang")
    assert "MENUITEM_SETTING(album_list_layout" in theme_menu
    assert "&album_list_layout" in theme_menu


def test_ipone_right_pane_slideshow_pan_is_smooth_and_refreshed():
    albumlist = _read("apps/gui/albumlist_art.c")
    sbs = _read("apps/gui/statusbar-skinned.c")
    ipone_sbs = _read("wps/iPone.sbs")
    ipone7g_sbs = _read("wps/iPone7G.sbs")

    assert "#define ALBUMLIST_BITMAP_CACHE 24" in albumlist
    assert "#define ALBUMLIST_MANIFEST_CACHE_MAX 384" in albumlist
    assert "#define ALBUMLIST_SLIDESHOW_SIZE 384" in albumlist
    assert "#define ALBUMLIST_SLIDESHOW_PAN_SCALE 1024" in albumlist
    assert "#define ALBUMLIST_SLIDESHOW_PAN_DURATION (HZ * 6)" in albumlist
    assert "#define ALBUMLIST_SLIDESHOW_HOLD_DURATION 0" in albumlist
    assert "static int albumlist_random_manifest_index" in albumlist
    assert "albumlist_prepare_random_order(entry_count);" in albumlist
    assert "albumlist_manifest_count()" in albumlist
    assert "albumlist_manifest_at(wanted)" in albumlist
    assert "slideshow_order_base" in albumlist
    assert "slideshow_order_step" in albumlist
    assert "bool has_slide_field = field_count >= 7" in albumlist
    assert "albumlist_manifest_path_join(fields[slide_field], entry->slide_path" in albumlist
    assert "static bool slideshow_paused;" in albumlist
    assert "static long slideshow_paused_at;" in albumlist
    assert "static long slideshow_paused_total;" in albumlist
    assert "void albumlist_slideshow_set_paused(bool paused)" in albumlist
    assert "long slideshow_tick = albumlist_slideshow_tick();" in albumlist
    assert "return tick - slideshow_paused_total;" in albumlist
    assert "albumlist_gcd(slideshow_order_step, entry_count)" in albumlist
    assert "struct albumlist_slideshow_slot slideshow_slots[2]" in albumlist
    assert "#define ALBUMLIST_SLIDESHOW_FAILURE_CACHE 8" in albumlist
    assert "albumlist_slideshow_recent_failure(index, path)" in albumlist
    assert "albumlist_slideshow_record_failure(index, path);" in albumlist
    assert "albumlist_choose_slideshow_victim(protected_index)" in albumlist
    assert "albumlist_prefetch_slideshow_slot(next_wanted, wanted);" in albumlist
    assert "albumlist_slideshow_is_prerendered(path)" in albumlist
    assert "format |= FORMAT_RESIZE | FORMAT_KEEP_ASPECT | FORMAT_DITHER;" in albumlist
    assert "long pan_pos = pan_phase * ALBUMLIST_SLIDESHOW_PAN_SCALE /" in albumlist
    assert "int pan_mode = (int)(cycle % 6);" in albumlist
    assert "case 2: /* top to bottom */" in albumlist
    assert "case 3: /* bottom to top */" in albumlist
    assert "case 4: /* upper-left to lower-right */" in albumlist
    assert "default: /* upper-right to lower-left */" in albumlist
    assert "pan_range_x * pan_pos / ALBUMLIST_SLIDESHOW_PAN_SCALE" in albumlist
    assert "pan_range_y * pan_pos / ALBUMLIST_SLIDESHOW_PAN_SCALE" in albumlist
    assert "albumlist_draw_menu_shadow" not in albumlist
    assert "albumlist_draw_slideshow_shadow(display, x, y, width, height);" in albumlist
    assert "fb_data *pixel = FBADDR(x + xx, y + yy);" in albumlist
    assert "*pixel = FB_RGBPACK(r, g, b);" in albumlist
    assert "#define IPONE_RIGHT_PANE_SLIDESHOW_UPDATE_DELAY MAX(1, HZ / 30)" in sbs
    assert "sb_ipone_right_pane_slideshow_eligible" in sbs
    assert "sb_ipone_right_pane_slideshow_paused_by_hold" in sbs
    assert "sb_ipone_right_pane_slideshow_active" in sbs
    assert 'strstr(sbs_file, "Forest") != NULL' in sbs
    assert "albumlist_slideshow_set_paused(" in sbs
    assert "sb_ipone_right_pane_track_active_state(screen, &force);" in sbs
    assert "sb_ipone_update_right_pane_slideshow(screen, false);" in sbs
    assert "MIN(next_delay, IPONE_RIGHT_PANE_SLIDESHOW_UPDATE_DELAY)" not in sbs
    assert "%xl(SbsBgFullArt,iPone_bd_fullart.bmp)" in ipone_sbs
    assert "%?if(%St(ipone right pane), =, full art)<%xd(SbsBgFullArt)|%xd(SbsBg)>" in ipone_sbs
    assert "%xl(SbsIpodLabel,SbsIpodLabel.bmp)" in ipone7g_sbs
    assert "%V(8,3,58,14,-)\n%?if(%cs, =, 21)<|%?mh<|%xd(SbsIpodLabel)>>" in ipone7g_sbs
    assert "%aliPod" not in ipone7g_sbs
    assert "%Vd(SbsIpodTitle)" not in ipone7g_sbs


def test_album_list_fullscreen_viewport_keeps_non_album_iclassic_pane():
    list_c = _read("apps/gui/list.c")
    list_h = _read("apps/gui/list.h")

    assert "bool force_fullscreen_albumlist;" in list_h
    assert "bool fullscreen_albumlist_theme_hidden;" in list_h
    assert "int fullscreen_albumlist_first_item;" in list_h
    assert "gui_synclist_set_fullscreen_albumlist" in list_h
    assert "list_apply_fullscreen_albumlist_viewport" in list_c
    assert "viewport_set_defaults(vp, screen);" in list_c
    assert "if (list->force_fullscreen_albumlist)" in list_c
    assert "gui_list->force_fullscreen_albumlist ||" in list_c
    assert "!skinlist_draw(&screens[i], gui_list)" in list_c
    assert "viewportmanager_theme_enable(i, false, NULL);" in list_c
    assert "viewportmanager_theme_undo(i, true);" in list_c
    assert "return list->title != NULL &&" in list_c
    assert "list_nb_lines(list, screen) > 2;" in list_c
    assert "list_get_title_height" in list_c
    assert "font_get(list->parent[screen]->font)->height" in list_c
    assert "item_number < gui_list->fullscreen_albumlist_first_item" in list_c
    assert "sb_skin_needs_fast_update(SCREEN_MAIN)" in list_c
    assert "int fast_timeout = MAX(1, HZ / 30);" in list_c
    assert "timeout == TIMEOUT_BLOCK || timeout > fast_timeout" in list_c
    assert "gui_list->start_item[screen] = min_start;" in list_c
    assert "list_is_ipodvideo_iclassic_theme()" in list_c
    assert "vp->width = 144;" in list_c

    bitmap_list = _read("apps/gui/bitmap/list.c")
    assert "!list->force_fullscreen_albumlist &&" in bitmap_list
    assert "list_get_title_height(list, screen)" in bitmap_list
    assert "(list_text_vp->height % linedes.height) != 0" in bitmap_list
    assert "end < list->nb_items" in bitmap_list


def test_album_list_draws_clipped_partial_bottom_row():
    bitmap_list = _read("apps/gui/bitmap/list.c")

    ipodjs_draw = bitmap_list.split(
        "static void list_ipodjs_draw(struct screen", 1
    )[1].split("void gui_list_draw_item_default", 1)[0]
    assert "int draw_rows = visible;" in ipodjs_draw
    assert "(list_h % row_h) != 0" in ipodjs_draw
    assert "draw_rows++;" in ipodjs_draw

    stock_draw = bitmap_list.split("void list_draw(struct screen", 1)[1]
    assert "list->force_fullscreen_albumlist" in stock_draw
    assert "(list_text_vp->height % linedes.height) != 0" in stock_draw
    assert "end++;" in stock_draw


def test_right_pane_video_is_not_gated_by_audio_playback():
    statusbar = _read("apps/gui/statusbar-skinned.c")
    eligibility = statusbar.split(
        "static bool sb_ipone_right_pane_video_eligible", 1
    )[1].split("static unsigned sb_ipone_video_read_le16", 1)[0]

    assert "audio_status()" not in eligibility
    assert "AUDIO_STATUS_PLAY" not in eligibility
    assert "AUDIO_STATUS_PAUSE" not in eligibility


def test_right_pane_video_accepts_rockbox_extensionless_sbs_setting():
    statusbar = _read("apps/gui/statusbar-skinned.c")
    resolver = statusbar.split(
        "static bool sb_ipone_video_path", 1
    )[1].split("static bool sb_ipone_video_load_frame", 1)[0]

    assert 'if (ext && !strcmp(ext, ".sbs"))' in resolver
    assert 'if (!ext || strcmp(ext, ".sbs"))' not in resolver
    assert '"/.rockbox/wps/%s/RightPaneVideo.rbvp"' in resolver


def test_right_pane_video_is_a_distinct_third_setting():
    settings = _read("apps/settings_list.c")
    statusbar = _read("apps/gui/statusbar-skinned.c")

    assert '"miniplayer,full art,video", NULL, 3' in settings
    assert '"Miniplayer", "Full Art", "Video"' in settings
    assert "global_settings.ipone_right_pane != 2" in statusbar
    assert "global_settings.ipone_right_pane == 2" in statusbar


def test_ipodjs_fullscreen_album_lists_keep_ipodjs_palette():
    bitmap_list = _read("apps/gui/bitmap/list.c")
    root_menu = _read("apps/root_menu.c")

    enabled_body = bitmap_list.split(
        "static bool list_ipodjs_enabled", 1
    )[1].split("static int list_ipodjs_font", 1)[0]
    assert "global_settings.ui_engine == UI_ENGINE_IPODJS" in enabled_body
    assert "force_fullscreen_albumlist" not in enabled_body
    assert "if (list_ipodjs_enabled(display))" in bitmap_list
    assert "ipodjs_ui_header_text()" in bitmap_list
    assert "ipodjs_ui_header_bg()" in bitmap_list
    assert "ipodjs_ui_glass_gradient" in bitmap_list
    assert "global_settings.ui_engine_dark_mode" in bitmap_list

    handoff_body = root_menu.split(
        "static int root_menu_video_finish_native_screen", 1
    )[1].split("static int root_menu_video_row_height", 1)[0]
    assert "root_menu_video_uses_stock_music()" in handoff_body
    assert "ret == GO_TO_DBBROWSER" in handoff_body


def test_ipodjs_photos_slideshow_uses_full_quality_previews():
    root_menu = _read("apps/root_menu.c")
    photo_loader = root_menu.split(
        "static int root_menu_video_photo_load_locks", 1
    )[1].split("static void root_menu_video_preview_load_game_paths", 1)[0]
    photo_timing = root_menu.split(
        "static long root_menu_video_source_slideshow_period", 1
    )[1].split("static bool root_menu_video_preview_service", 1)[0]
    decode_gate = root_menu.split(
        "static bool root_menu_video_preview_decode_permitted", 1
    )[1].split("static struct root_menu_video_preview_slot *", 1)[0]
    preview_service = root_menu.split(
        "static bool root_menu_video_preview_service", 1
    )[1].split("static bool root_menu_video_draw_preview_cover", 1)[0]

    assert '.photo_previews"' in photo_loader
    assert ".photo_thumbs" not in photo_loader
    assert 'root_menu_video_preview_path_has_previews("/Photos")' in root_menu
    assert "root_menu_video_draw_source_slideshow_cached(source," in root_menu
    assert "#define IPODJS_PHOTO_INDEX_MAX 1024" in root_menu
    assert "#define IPODJS_PREVIEW_IMAGE_WIDTH 320" in root_menu
    assert "#define IPODJS_PREVIEW_IMAGE_HEIGHT 240" in root_menu
    assert "slot->bm.width = IPODJS_PREVIEW_IMAGE_WIDTH;" in root_menu
    assert "slot->bm.height = IPODJS_PREVIEW_IMAGE_HEIGHT;" in root_menu
    assert "slideshow_order_index(&root_menu_video_photo_order" in photo_timing
    assert "if (source == IPODJS_PREVIEW_PHOTOS)" in photo_timing
    assert "return HZ * 6;" in photo_timing
    assert "root_menu_video_source_slideshow_period(source) / 2" in photo_timing
    assert "AUDIO_STATUS_PAUSE" not in decode_gate
    assert "AUDIO_STATUS_PAUSE" not in preview_service


def test_ipodjs_photo_and_game_slideshows_cover_the_full_right_pane():
    root_menu = _read("apps/root_menu.c")
    cover_draw = root_menu.split(
        "static bool root_menu_video_draw_preview_cover", 1
    )[1].split("static bool root_menu_video_draw_source_slideshow_cached", 1)[0]
    slideshow_draw = root_menu.split(
        "static bool root_menu_video_draw_source_slideshow_cached", 1
    )[1].split("static void root_menu_video_draw_clock_date", 1)[0]

    assert "bm->width * h > bm->height * w" in cover_draw
    assert "crop_w = MAX(1, bm->height * w / h);" in cover_draw
    assert "crop_h = MAX(1, bm->width * h / w);" in cover_draw
    assert "dst[dst_x] = src[sample_x];" in cover_draw
    assert "core_alloc" not in cover_draw
    assert "root_menu_video_draw_preview_cover(&slot->bm" in slideshow_draw
    assert "lcd_fillrect" not in slideshow_draw


def test_ipodjs_photos_slideshow_excludes_locked_photos_and_folders():
    root_menu = _read("apps/root_menu.c")
    lock_filter = root_menu.split(
        "static bool root_menu_video_photo_preview_is_locked", 1
    )[1].split("static void root_menu_video_preview_load_photo_paths", 1)[0]
    photo_launcher = root_menu.split(
        "static int launch_photos_plugin", 1
    )[1].split("MENUITEM_FUNCTION(photos_item", 1)[0]

    lock_boundary = root_menu.split(
        "static bool root_menu_video_photo_relative_locked", 1
    )[1].split("static bool root_menu_video_photo_preview_is_locked", 1)[0]
    index_loader = root_menu.split(
        "static int root_menu_video_photo_load_locks", 1
    )[1].split("static void root_menu_video_preview_load_photo_paths", 1)[0]

    assert 'PLUGIN_APPS_DATA_DIR "/photos.locks"' in root_menu
    assert "relative_len == lock_len || relative[lock_len] == '/'" in lock_boundary
    assert (
        "root_menu_video_preview_filter_locked_photos(root_menu_video_photo_root);"
        in root_menu
    )
    assert "root_menu_video_preview_path_count = 0;" in lock_filter
    # The index path must apply locks while building the offset table and
    # fail closed when the lock database cannot be read.
    assert "root_menu_video_photo_relative_locked(line, locks[i])" in index_loader
    assert "lock_count < 0" in index_loader
    assert "IPODJS_PREVIEW_PHOTOS" in photo_launcher
    assert "file_exists(photo_path)" in root_menu


def test_ipodjs_photos_fallback_maps_nested_previews_from_preview_root():
    root_menu = _read("apps/root_menu.c")
    preview_scan = root_menu.split(
        "static void root_menu_video_preview_scan_photo_previews", 1
    )[1].split(
        "static bool root_menu_video_photo_relative_locked", 1
    )[0]

    assert "const char *preview_root" in preview_scan
    assert "const size_t prefix_len = strlen(preview_root);" in preview_scan
    assert "strncmp(child, preview_root, prefix_len)" in preview_scan
    assert (
        "root_menu_video_preview_scan_photo_previews(child,\n"
        "                                                       preview_root,"
        in preview_scan
    )


def test_photos_plugin_keeps_full_quality_previews_with_photo_operations():
    photos = _read("apps/plugins/photos.c")

    assert '#define PHOTOS_PREVIEW_DIR_SUFFIX ".photo_previews"' in photos
    assert "static void photos_move_sidecars" in photos
    assert "photos_move_sidecars(entry->path, newpath, entry->is_dir);" in photos
    assert "photos_move_sidecars(entry->path, newpath, false);" in photos
    assert "static void photos_delete_sidecars_for" in photos
    assert "photos_delete_sidecars_for(delete_path, is_dir);" in photos
    # Rename/delete operations must keep the hover-pane preview index
    # consistent so it never references a moved or deleted photo.
    assert "photos_update_preview_index(old_rel, new_rel, is_dir);" in photos
    assert "photos_update_preview_index(relpath, NULL, is_dir);" in photos


def test_album_list_change_does_not_edit_ipone_colors():
    cfg = _read("build-sim-video-5g/simdisk/.rockbox/config.cfg")

    for line in (
        "foreground color: f7f7ff",
        "background color: 100c10",
        "line selector start color: 292031",
        "line selector end color: 9c79e7",
        "line selector text color: fffbff",
        "list separator color: 181421",
    ):
        assert line in cfg


def test_album_list_export_uses_stock_sized_cover_thumbs():
    artwork_manager = _read("rockpod/services/artwork_manager.py")

    assert "_ALBUM_LIST_THUMB_SIZE = (40, 40)" in artwork_manager
