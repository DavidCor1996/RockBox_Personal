from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]


def _text(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


def test_steam_games_appearance_is_optional_and_intercepts_games_entries():
    settings_h = _text("apps/settings.h")
    settings_list = _text("apps/settings_list.c")
    theme_menu = _text("apps/menus/theme_menu.c")
    root_menu = _text("apps/root_menu.c")

    assert "UI_ENGINE_GAMES_CLASSIC" in settings_h
    assert "UI_ENGINE_GAMES_STEAM" in settings_h
    assert '"ui engine games appearance", "classic,steam"' in settings_list
    assert '"iPod Games Appearance"' in theme_menu
    dispatch = root_menu.index(
        "global_settings.ui_engine_games_appearance ==\n"
        "            UI_ENGINE_GAMES_STEAM"
    )
    direct_cover_flow = root_menu.index("if (item == &gameboy_browser)", dispatch)
    assert dispatch < direct_cover_flow


def test_steam_library_uses_exact_palette_bounded_cache_and_real_art_contract():
    root_menu = _text("apps/root_menu.c")
    buildzip = _text("tools/buildzip.pl")
    spec = _text("docs/ipodjs-steam-games-app-spec.md")

    for rgb in (
        "LCD_RGBPACK(23, 26, 33)",
        "LCD_RGBPACK(27, 40, 56)",
        "LCD_RGBPACK(42, 71, 94)",
        "LCD_RGBPACK(102, 192, 244)",
    ):
        assert rgb in root_menu
    assert "IPODJS_STEAM_COVER_CACHE 3" in root_menu
    assert "IPODJS_STEAM_MAX_CONSOLES 24" in root_menu
    assert "case ACTION_STD_CONTEXT:" in root_menu
    assert 'ipodjs_trace_screen("Steam Consoles"' in root_menu
    assert "BM_SCALED_SIZE(IPODJS_STEAM_COVER_W" in root_menu
    assert "core_alloc(" not in root_menu[root_menu.index("#define IPODJS_STEAM_MAX_GAMES"):root_menu.index("enum root_menu_video_qs_item")]
    assert "rockbox-manual-screenshot" in buildzip
    assert "existing-cover" in buildzip
    assert "generated-typographic-cover" not in buildzip
    assert "no synthetic game thumbnail" in spec


def test_steam_library_loads_installed_ipod_games_from_achievement_catalog():
    root_menu = _text("apps/root_menu.c")

    assert "ipodjs_steam_load_ipod_games();" in root_menu
    assert 'strcasecmp(fields[2], "iPod Games")' in root_menu
    assert 'PLUGIN_GAMES_DIR "/ipodgames.rock", fields[11], fields[4]' in \
        root_menu


def test_anarch_has_verified_native_artwork_for_steam_discovery():
    sources = _text("assets/game_covers/native/SOURCES.tsv")
    cover = ROOT / "assets/game_covers/native/anarch.bmp"

    assert "anarch\texisting-cover\tapps/plugins/anarch/upstream/" \
        "media/screenshot1.png" in sources
    with Image.open(cover) as artwork:
        assert artwork.size == (160, 120)


def test_cps1_has_a_dedicated_zip_platform_and_manifest():
    root_menu = _text("apps/root_menu.c")

    assert 'strstr(path, "/games/cps1/roms/")' in root_menu
    assert 'return "CPS1 Arcade";' in root_menu
    assert 'ROCKBOX_DIR "/rocks/games/cps1/games.tsv"' in root_menu


def test_official_steam_logo_has_native_header_background_without_white_box():
    readme = _text("assets/ipodjs/sources/steam/README.md")
    logo_path = ROOT / "assets/ipodjs/rockbox/steam/steam-logo-official.110x32x24.bmp"

    assert "store.akamai.steamstatic.com" in readme
    with Image.open(logo_path).convert("RGB") as logo:
        assert logo.size == (110, 32)
        assert logo.getpixel((0, 0)) == (16, 24, 33)
        assert logo.getpixel((109, 31)) == (16, 24, 33)
        assert max(channel for pixel in logo.getdata() for channel in pixel) < 255
