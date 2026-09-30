"""Guard the Classic guide's light menu bar and protected Hold appearance."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    match = re.search(r"\b" + re.escape(name) + r"\([^;{}]*\)\s*\{(.*?)\n}",
                      source, re.S)
    assert match, name
    return match.group(1)


def test_light_header_is_the_official_20px_asset_not_the_dark_bar():
    source = (ROOT / "apps/gui/ipodjs_ui.c").read_text()
    registry = (ROOT / "tools/ipod_classic_resource_extract.py").read_text()
    assert '"statusbar-white-background": 8,' in registry
    assert '"statusbar-black-background": 10,' in registry
    assert '"statusbar-white-background", cache->light_background_data' in source
    assert '"statusbar-black-background", cache->hold_background_data' in source
    draw = function(source, "ipodjs_ui_draw_header_background")
    assert "button_hold() ? &ipodjs_ui_retail_status.hold_background" in draw
    assert "&ipodjs_ui_retail_status.light_background" in draw
    assert "MIN(header->height, IPODJS_UI_HEADER_HEIGHT)" in draw
    title = function(source, "ipodjs_ui_header_text")
    assert "ipodjs_ui_dark() || button_hold()" in title
    assert "LCD_RGBPACK(255, 255, 255) : LCD_RGBPACK(0, 0, 0)" in title


def test_music_reuses_the_same_header_without_a_second_cache():
    source = (ROOT / "apps/gui/ipodjs_ui.c").read_text()
    cache = source.split("struct ipodjs_ui_retail_wps_cache\n", 1)[1]
    cache = cache.split("\n};", 1)[0]
    assert "header_data" not in cache
    draw = function(source, "ipodjs_ui_draw_retailos_music_header")
    assert "&status->light_background" in draw
    assert "ipodjs_ui_draw_playback_indicator" in draw


def test_light_playback_glyph_changes_do_not_change_hold_selection():
    source = (ROOT / "apps/gui/ipodjs_ui.c").read_text()
    draw = function(source, "ipodjs_ui_draw_playback_indicator")
    assert "if (button_hold())" in draw
    assert "&ipodjs_ui_retail_status.white_pause" in draw
    assert "&ipodjs_ui_retail_status.black_pause" in draw
    assert "ipodjs_ui_draw_retailos_status_glyph" in draw
    assert "ipodjs_retailos_blit_color" not in draw
    glyph = function(source, "ipodjs_ui_draw_retailos_status_glyph")
    assert "UI_ENGINE_ACCENT_BLUE" in glyph
    assert "ipodjs_retailos_blit(display, image, x, y)" in glyph
    assert "ipodjs_retailos_blit_tint_part" in glyph


def test_status_draw_paths_only_paint_cached_pixels():
    source = (ROOT / "apps/gui/ipodjs_ui.c").read_text()
    for name in ("ipodjs_ui_draw_header_background",
                 "ipodjs_ui_draw_retailos_music_header",
                 "ipodjs_ui_draw_playback_indicator",
                 "ipodjs_ui_draw_retailos_status_glyph",
                 "ipodjs_ui_draw_retailos_battery",
                 "ipodjs_ui_draw_retail_charge_frame"):
        draw = function(source, name)
        for forbidden in ("load_named", "load_resource", "core_alloc(",
                          "read_bmp", "audio_stop(", "font_load("):
            assert forbidden not in draw


def test_power_glyph_is_composited_over_a_complete_battery():
    source = (ROOT / "apps/gui/ipodjs_ui.c").read_text()
    draw = function(source, "ipodjs_ui_draw_retailos_battery")
    assert "int frame = level * 22 / 100" in draw
    assert "if (!connected || !button_hold())" in draw
    assert draw.count("ipodjs_retailos_blit_part") == 2
    assert draw.index("ipodjs_retailos_blit_part") < draw.index(
        "frame = !charging_state()")
    for caller in ("ipodjs_ui_draw_header_battery",
                   "ipodjs_ui_draw_retailos_music_header"):
        assert "ipodjs_ui_draw_retailos_battery" in function(source, caller)


def test_large_charge_masks_center_in_casing_not_sprite_padding():
    source = (ROOT / "apps/gui/ipodjs_ui.c").read_text()
    draw = function(source, "ipodjs_ui_draw_retail_charge_frame")
    assert "const int body_x = origin_x + 22" in draw
    assert "const int body_y = origin_y + 22" in draw
    assert "#define IPODJS_RETAIL_CHARGE_MIDDLE_W 123" in source
    assert "const int body_w = IPODJS_RETAIL_CHARGE_W - 22 - 30" in draw
    assert "const int body_h = 75" in draw
    for width, height in ((47, 29), (23, 57)):
        assert f"body_x + (body_w - {width}) / 2" in draw
        assert f"body_y + (body_h - {height}) / 2" in draw
    assert draw.count("FB_RGBPACK(0, 0, 0)") == 2


def test_home_extras_clock_is_the_cached_apple_resource_renderer():
    source = (ROOT / "apps/root_menu.c").read_text()
    draw = source.split(
        "static void root_menu_video_draw_stock_clock_preview(int x, int y, int w,",
        2)[2].split("\n}", 1)[0]
    assert "ipodjs_utilities_draw_classic_preview" in draw
    for forbidden in ("fill_circle", "clock_hand", "lcd_drawline",
                      "file_exists", "read_bmp", "prepare_classic_preview"):
        assert forbidden not in draw
    startup = function(source, "root_menu_ipodjs_prepare_wps_fonts")
    assert startup.index("if (audio_status())") < startup.index(
        "ipodjs_utilities_prepare_classic_preview")
