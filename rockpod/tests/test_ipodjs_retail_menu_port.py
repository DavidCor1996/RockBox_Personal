"""Reference geometry, provenance, and scope guards for Apple-themed menus."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct

import pytest

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text()


def body(source, name):
    return source.split(name + "(", 1)[1].split("\n}", 1)[0]


def test_status_font_is_separate_and_titles_are_left_aligned():
    root = read("apps/root_menu.c")
    header = body(root, "root_menu_video_draw_status_title_width")
    assert "ipodjs_ui_retailos_font(false)" in header
    assert "DRMODE_FG" in header
    assert "bool preserve = button_hold();" in header
    assert "preserve ? 5 + root_menu_video_text_y_offset() : 4" in header
    bitmap = body(read("apps/gui/bitmap/list.c"), "list_ipodjs_header")
    assert "display, 6, 4, fit_width, title, false" in bitmap
    utilities = body(read("apps/gui/ipodjs_utilities.c"), "ipodjs_draw_header")
    assert "ipodjs_ui_retailos_font(false)" in utilities
    assert "const bool preserve = button_hold()" in utilities
    assert "DRMODE_FG" in utilities


def test_menu_gap_is_removed_without_moving_the_application_grid():
    assert "#define IPODJS_UI_RETAIL_MENU_HEADER_HEIGHT 20" in read(
        "apps/gui/ipodjs_ui.h")
    root = read("apps/root_menu.c")
    grid = body(root, "root_menu_video_draw_applications_ios3")
    assert "{ 29, 95, 161 }" in grid
    assert "root_menu_video_wps_font(false)" in grid
    assert "root_menu_video_retail_wps_font" not in grid
    assert 'root_menu_video_draw_status_title("Applications")' in grid
    assert "LCD_HEIGHT - IPODJS_UI_HEADER_HEIGHT" in grid
    assert "IPODJS_UI_RETAIL_MENU_HEADER_HEIGHT" in read("apps/gui/bitmap/list.c")


def test_wps_reference_uses_resolved_coordinates_and_aligned_time():
    root = read("apps/root_menu.c")
    frame = body(root, "ipodjs_video_draw_wps_full")
    for coordinate in ("title_y = 59", "artist_y = 80", "album_y = 98",
                       "rating_y = 115", "sequence_y = 134"):
        assert coordinate in frame
    assert "LCD_RGBPACK(61, 61, 61)" in frame
    progress = body(root, "ipodjs_video_draw_wps_progress")
    assert "progress_x - 10 - elapsed_width" in progress
    assert "remaining, false" in progress
    assert "progress_x = 58" in progress
    assert "progress_y = 207" in progress


def test_official_font_preparation_is_bounded_and_precedes_playback():
    ui = read("apps/gui/ipodjs_ui.c")
    prepare = body(ui, "ipodjs_ui_prepare_retailos_fonts")
    assert prepare.index("if (audio_status())") < prepare.index("font_load_ex")
    assert "font_load_ex(paths[face], 0, 96)" in prepare
    for name in ("ipodjs_ui_retailos_font", "ipodjs_ui_retailos_menu_font"):
        getter = body(ui, name)
        for forbidden in ("font_load", "file_exists", "core_alloc", "open("):
            assert forbidden not in getter
    assert "FT_Select_Charmap(face, FT_ENCODING_UNICODE)" in read("tools/convttf.c")


def test_menu_controls_use_native_source_sizes_and_cached_masks():
    ui = read("apps/gui/ipodjs_ui.c")
    prepare = body(ui, "ipodjs_ui_prepare_retailos_menu")
    assert prepare.index("if (audio_status())") < prepare.index(
        "ipodjs_retailos_load_resource_rga")
    for source in ('"settings-main-menu"', '"settings-apple-logo"',
                   "14, 12", "78, 121", "119, 119, 120"):
        assert source in prepare
    draw = body(ui, "ipodjs_ui_draw_retailos_settings_preview")
    assert "ipodjs_retailos_blit_mask" in draw
    assert "202, 81" in draw
    assert "179, 173, 130" in draw
    assert "ipodjs_ui_retailos_font(!main_menu)" in draw
    for name in ("ipodjs_ui_draw_retailos_settings_preview",
                 "ipodjs_ui_draw_retailos_scrollbar",
                 "ipodjs_ui_draw_retailos_check"):
        draw = body(ui, name)
        for forbidden in ("open(", "read_bmp", "font_load", "core_alloc",
                          "tagcache_", "audio_stop", "load_resource"):
            assert forbidden not in draw


def test_full_and_dirty_menu_paints_restore_system_font():
    bitmap = read("apps/gui/bitmap/list.c")
    draw = body(bitmap, "list_ipodjs_draw")
    after_header = draw.split("list_ipodjs_header(display,", 1)[1]
    assert after_header.index("display->setfont(list_ipodjs_font())") < (
        after_header.index("list_ipodjs_draw_row"))
    header = body(bitmap, "list_ipodjs_header")
    assert "display, title_width" in header
    assert "title_width - 31" in header
    assert "hline(8, list_w - 9" not in bitmap
    root = read("apps/root_menu.c")
    header = body(root, "root_menu_video_draw_status_title_width")
    pane = header.split("if (pane_mode)", 1)[1].split("else\n", 1)[0]
    assert 'title ? title : "iPod"' in pane


def test_menu_crop_validates_geometry_and_closes_its_descriptor():
    crop = body(read("apps/gui/ipodjs_retailos.c"),
                "ipodjs_retailos_load_resource_crop")
    for guard in ("x + width > source_width", "y + height > source_height",
                  "filesize(fd)", 'memcmp(header, "RGA1", 4)',
                  "lseek(fd, offset, SEEK_SET)", "close(fd)"):
        assert guard in crop


def test_quick_scroll_font_is_preloaded_not_opened_in_draw():
    ui = read("apps/gui/ipodjs_ui.c")
    for name in ("ipodjs_ui_fast_scroll_available", "ipodjs_ui_fast_scroll_font"):
        draw = body(ui, name)
        assert "font_load" not in draw
        assert "file_exists" not in draw


def test_all_three_existing_pin_menus_use_original_classic_parts():
    for file, prefix in (("photos", "photos"), ("comics", "comic"),
                         ("magazines", "mag")):
        source = read(f"apps/plugins/{file}.c")
        assert "ipodjs_retailos_prepare_pin" in source
        assert "fast-scroll-blank.apple" not in source
        draw = body(source, prefix + "_draw_pin_surface")
        for forbidden in ("read_bmp", "open(", "plugin_get_audio_buffer",
                          "audio_stop", "font_load"):
            assert forbidden not in draw
    cache = read("apps/plugins/lib/ipodjs_retailos_controls.c")
    for asset in ("system-quick-scroll", "system-input-field-left",
                  "optionbar-white-thumb-left"):
        assert asset in cache


def test_private_font_outputs_match_their_provenance_and_unicode_mapping():
    directory = ROOT / "assets/ipodjs/apple/retailos-fonts"
    if not (directory / "provenance.json").exists():
        pytest.skip("private Apple font extraction not installed")
    ledger = json.loads((directory / "provenance.json").read_text())
    assert ledger["glyph_metrics"] == "source advances and left bearings"
    for item in ledger["fonts"]:
        data = (directory / item["file"]).read_bytes()
        assert hashlib.sha256(data).hexdigest() == item["sha256"]
        assert data[:4] == b"RB12"
        assert struct.unpack_from("<I", data, 12)[0] == 32
        assert struct.unpack_from("<I", data, 20)[0] >= 64000


def test_apple_font_conversion_preserves_metrics_without_changing_legacy_fonts():
    converter = read("tools/convttf.c")
    assert "int             preserve_metrics = 0" in converter
    width = body(converter, "glyph_width")
    assert "if (preserve_metrics)" in width
    assert "face->glyph->advance.x" in width
    assert "col_off = slot->bitmap_left" in converter
    assert '"-A", "-s", "32"' in read("tools/prepare_ipod_classic_fonts.py")


def test_utility_rows_share_menu_fonts_and_large_clock_hands_are_centered():
    utilities = read("apps/gui/ipodjs_utilities.c")
    assert "ipodjs_ui_retailos_menu_font()" in body(utilities, "ipodjs_draw_base")
    preview = body(utilities, "ipodjs_utilities_draw_classic_preview")
    assert "clock_x + 29, clock_y + 29" in preview
    assert "clock_y + 15" not in preview


@pytest.mark.parametrize("damage", ["missing", "corrupt"])
def test_font_packaging_rejects_incomplete_outputs(tmp_path, damage):
    directory = ROOT / "assets/ipodjs/apple/retailos-fonts"
    if not (directory / "provenance.json").exists():
        pytest.skip("private Apple font extraction not installed")
    spec = importlib.util.spec_from_file_location(
        "classic_fonts", ROOT / "tools/prepare_ipod_classic_fonts.py")
    fonts = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fonts)
    fixture = tmp_path / "fonts"
    shutil.copytree(directory, fixture)
    target = fixture / "15-Helvetica-Bold-RetailOS-Apple.fnt"
    if damage == "missing":
        target.unlink()
    else:
        data = bytearray(target.read_bytes())
        data[-1] ^= 1
        target.write_bytes(data)
    with pytest.raises((ValueError, FileNotFoundError)):
        fonts.verify(fixture)
