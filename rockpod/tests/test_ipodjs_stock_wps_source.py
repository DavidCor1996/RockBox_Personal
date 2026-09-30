"""Static checks for the stock-engine iPodJS while-playing skin."""

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(rel_path):
    return (REPO_ROOT / rel_path).read_text(encoding="utf-8", errors="replace")


def test_ipod6g_ipodjs_uses_stock_wps_with_verified_apple_chrome():
    engine = _read("apps/gui/skin_engine/skin_engine.c")
    wps = _read("wps/ipodjs-classic.wps")
    wpslist = _read("wps/WPSLIST")

    assert 'setting = "ipodjs-classic";' in engine
    assert "global_settings.ui_engine == UI_ENGINE_IPODJS" in engine
    assert "%Cl(0,0,128,128,c,c,1)" in wps
    assert "%Vl(art,15,34,128,128,-)" in wps
    assert ".apple." not in wps
    assert "%Vs(none)" in wps
    assert "%dr(" not in wps
    assert "%Cd(" not in wps
    assert "Name: ipodjs-classic" in wpslist
    assert "wps.320x240x(16|24|32): ipodjs-classic.wps" in wpslist
    assert "font.320x240x(16|24|32): 14-Adobe-Helvetica-Bold.fnt" in wpslist

    # Fixed stock chrome must not be remapped to an album-art palette.
    assert "%St(ui engine accent)" not in wps


def test_ipodjs_wps_scrolling_metadata_clears_each_previous_frame():
    source = _read("apps/root_menu.c")
    draw = source.split(
        "static void root_menu_video_draw_wps_title_offset(int offset)", 1
    )[1].split("static bool root_menu_video_update_wps_title_scroll", 1)[0]
    assert "unsigned canvas = IPODJS_WPS_WHITE;" in draw
    assert draw.index("display->fillrect(") < draw.index("display->putsxy(")
    assert "display->set_viewport(NULL);" in draw
    for forbidden in ("font_load(", "core_alloc(", "open(", "audio_stop("):
        assert forbidden not in draw


def test_ipodjs_wps_scroll_splits_large_steps_into_bounded_frames():
    engine = _read("firmware/scroll_engine.c")
    worker = _read("firmware/drivers/lcd-scroll.c")

    assert "lcd_scroll_tick_interval()" in engine
    assert "IPODJS_SCROLL_FRAME_TICKS MAX(1, HZ / 30)" in worker
    assert "get_current_activity() == ACTIVITY_WPS" in worker
    assert "s->smooth_remainder + elapsed * si->step" in worker
    assert "s->offset += step;" in worker


def test_iponecustom_packages_the_ipodjs_stock_font():
    wpslist = _read("wps/WPSLIST")
    theme_cfg = _read("themes/iPoneCustom.cfg")
    iponecustom = wpslist.split("Name: iPoneCustom", 1)[1].split("</theme>", 1)[0]

    assert "font.320x240x(16|24|32): 14-Adobe-Helvetica-Bold.fnt" in iponecustom
    assert "font: /.rockbox/fonts/14-Adobe-Helvetica-Bold.fnt" in theme_cfg
    assert "ui engine: ipodjs" in theme_cfg


def test_ipodjs_extras_keeps_applications_submenu_and_files():
    root_menu = _read("apps/root_menu.c")
    extras = root_menu.split(
        "root_menu_video_extras_items[] = {", 1
    )[1].split("};", 1)[0]

    assert '{ "Applications", IPODJS_EXTRAS_APPLICATIONS }' in extras
    assert '{ "Files", GO_TO_FILEBROWSER }' in extras
    assert "ret = root_menu_video_applications_menu();" in root_menu
    assert "root_menu_video_item_is_extras(item)" in root_menu


def test_rockpod_themes_use_rockbox_menu_engine_with_ipodjs_extras_layout():
    root_menu = _read("apps/root_menu.c")
    rockbox_extras = root_menu.split(
        'MAKE_MENU(applications_menu, "Extras"', 1
    )[1].split(");", 1)[0]

    assert "&rockpod_applications_menu" in rockbox_extras
    assert "&file_browser" in rockbox_extras
    assert "&gameboy_context_menu" in rockbox_extras
    assert "&system_menu_" in rockbox_extras


def test_album_rows_use_representative_albumartist_before_filtered_search():
    tagtree = _read("apps/tagtree.c")
    fast_path = tagtree.index(
        "tagcache_retrieve(&tcs, tcs.idx_id, tag_albumartist,"
    )
    artist_fallback = tagtree.index(
        "tagcache_retrieve(&tcs, tcs.idx_id, tag_artist,", fast_path
    )
    assert fast_path < artist_fallback
    assert "opening nested filtered searches for every row" in tagtree
