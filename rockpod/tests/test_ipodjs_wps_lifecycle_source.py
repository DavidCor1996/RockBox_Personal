"""Static guards for the iPodJS/Rockbox WPS lifecycle boundary."""

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(rel_path):
    return (REPO_ROOT / rel_path).read_text(
        encoding="utf-8", errors="replace"
    )


def test_ipodjs_wps_uses_the_rockbox_skin_and_action_loop():
    wps = _read("apps/gui/wps.c")
    loop = wps.split("long gui_wps_show(void)", 1)[1]

    assert "button = skin_wait_for_action(" in loop
    assert "root_menu_ipodjs_draw_wps_frame();" in loop
    assert "button = get_action(CONTEXT_WPS" not in loop


def test_ipod6g_decorative_art_never_remakes_the_audio_buffer():
    root_menu = _read("apps/root_menu.c")
    ensure = root_menu.split(
        "static void root_menu_video_ensure_aa_slot(void)", 1
    )[1].split("static struct bitmap *root_menu_video_buffered_art", 1)[0]
    ipod6g = ensure.split("#ifdef IPOD_6G", 1)[1].split("#else", 1)[0]

    assert "playback_claim_aa_slot" not in ipod6g
    assert "playback_update_aa_dims();" not in ipod6g
    assert "decorative UI must never restart playback" in ipod6g


def test_ipodjs_controls_do_not_reconstruct_wps_after_lyrics_menu():
    wps = _read("apps/gui/wps.c")
    lyrics = wps.split("case ACTION_WPS_CONTEXT:", 1)[1].split(
        "case ACTION_WPS_BROWSE:", 1
    )[0]

    assert "plugin_ret == PLUGIN_GOTO_ROOT" in lyrics
    assert "return GO_TO_ROOT;" in lyrics
    assert "gwps_enter_wps(false);" not in lyrics


def test_ipodjs_lyrics_short_select_is_inert_and_menu_is_the_only_exit():
    lyrics = _read("apps/plugins/lrcplayer.c")
    browse = lyrics.split("case ACTION_WPS_BROWSE:", 1)[1].split(
        "case ACTION_WPS_STOP:", 1
    )[0]
    menu = lyrics.split("case ACTION_WPS_MENU:", 1)[1].split(
        "default:", 1
    )[0]

    assert "if (!stock_ipod_ui)" in browse
    assert "ret = PLUGIN_OK;" in browse
    assert "ret = PLUGIN_GOTO_ROOT;" in menu


def test_ipod_wps_short_menu_does_not_depend_on_stale_action_history():
    keymap = _read("apps/keymaps/keymap-ipod.c")
    wps = keymap.split(
        "static const struct button_mapping button_context_wps[]", 1
    )[1].split("LAST_ITEM_IN_LIST", 1)[0]

    assert (
        "{ ACTION_WPS_MENU,          BUTTON_MENU,"
        "                    BUTTON_NONE }" in wps
    )
    assert "BUTTON_MENU|BUTTON_REL" not in wps
    assert "ACTION_WPS_QUICKSCREEN" not in wps

    wps_loop = _read("apps/gui/wps.c")
    menu = wps_loop.split("case ACTION_WPS_MENU:", 1)[1].split(
        "case ACTION_WPS_QUICKSCREEN:", 1
    )[0]
    assert "action_wait_for_release();" in menu


def test_ipodjs_wps_uses_verified_stock_apple_chrome():
    skin = _read("wps/ipodjs-classic.wps")

    ui = _read("apps/gui/ipodjs_ui.c")
    assert "ipodjs_retailos_load_resource_rga(" in ui
    assert "IPODJS_RETAILOS_NOW_PLAYING_EQUALIZER" in ui
    assert "IPODJS_RETAILOS_STATUSBAR_WHITE_BATTERY" in ui
    assert ".apple." not in skin

    art = skin.split(
        "%Vl(art,15,34,128,128,-)", 1
    )[1].split("# Stock metadata hierarchy", 1)[0]
    assert "%Cl(0,0,128,128,c,c,1)" in art
    assert "%Cl(15,34,128,128,c,c,1)" not in skin
    assert "%Cd(" not in skin
    assert "%Vs(none)" in skin
    assert "iPone" not in skin


def test_wps_font_lookup_never_allocates_or_loads_during_rendering():
    root = _read("apps/root_menu.c")
    getter = root.split("static int root_menu_video_wps_font(bool bold)", 1)[1]
    getter = getter.split("static void root_menu_video_prepare_wps_font", 1)[0]
    for forbidden in ("font_load(", "file_exists(", "core_alloc(", "open("):
        assert forbidden not in getter
    assert "root_menu_ipodjs_prepare_wps_fonts();" in _read("apps/main.c")
