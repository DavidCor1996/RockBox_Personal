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
