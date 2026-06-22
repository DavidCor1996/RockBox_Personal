"""Static checks for the Classic-style database album list layout."""

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(rel_path):
    return (REPO_ROOT / rel_path).read_text(encoding="utf-8", errors="replace")


def test_album_list_setup_uses_stock_sized_fullscreen_cover_rows():
    albumlist = _read("apps/gui/albumlist_art.c")

    assert "#define ALBUMLIST_THUMB_SIZE 40" in albumlist
    assert "#define ALBUMLIST_ROW_HEIGHT 44" in albumlist
    assert "gui_synclist_set_fullscreen_albumlist(list, true);" in albumlist
    assert "list->callback_get_item_icon = NULL;" in albumlist
    assert "list->show_icons = false;" in albumlist
    assert "list->callback_draw_item = albumlist_art_draw_item;" in albumlist
    assert "lookup_thumb_path(album, artist, path, sizeof(path))" in albumlist
    assert "load_thumb_bitmap(path)" in albumlist
    assert "text_info.item_indent += ALBUMLIST_THUMB_SIZE + ALBUMLIST_TEXT_PAD;" in albumlist
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
    assert "list->title != NULL && list_nb_lines(list, screen) > 2" in list_c
    assert "list_get_title_height" in list_c
    assert "font_get(list->parent[screen]->font)->height" in list_c
    assert "item_number < gui_list->fullscreen_albumlist_first_item" in list_c
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
