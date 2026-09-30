"""Settings port scope, bounded-cache and honest About inventory contracts."""
import importlib.util
import re
from pathlib import Path
import pytest

ROOT = Path(__file__).resolve().parents[2]


def source(path):
    return (ROOT / path).read_text()


def body(text, name):
    return re.split(r"\b" + re.escape(name) + r"\(", text, maxsplit=1)[1].split("\n}", 1)[0]


def test_settings_assets_are_cached_without_playback_memory_ownership():
    text = source("apps/gui/ipodjs_settings.c")
    prepare = body(text, "ipodjs_settings_prepare")
    assert prepare.index("audio_status()") < prepare.index("load_resource_rga")
    for forbidden in ("core_alloc", "plugin_get_audio_buffer", "audio_stop(",
                      "audio_play(", "mixer_channel", "malloc("):
        assert forbidden not in text
    for name in ("ipodjs_about_draw", "ipodjs_settings_draw_message",
                 "ipodjs_settings_draw_confirmation", "span", "text"):
        draw = body(text, name)
        for forbidden in ("open(", "close(", "tagcache_", "load_resource",
                          "font_load", "read_bmp"):
            assert forbidden not in draw


def test_settings_keeps_options_and_about_lives_in_extras():
    text = source("apps/root_menu.c")
    items = text.split("root_menu_video_settings_items[] = {", 1)[1].split("};", 1)[0]
    labels = ("Main Menu", "Sound", "Playback", "General", "Themes",
              "Recording", "System", "Manage Settings", "OnlyFans Visibility", "All Settings")
    positions = [items.index('"' + label + '"') for label in labels]
    assert positions == sorted(positions)
    assert '"About"' not in items
    extras = text.split("root_menu_video_extras_items[] = {", 1)[1].split("};", 1)[0]
    assert '{ "About", IPODJS_EXTRAS_ABOUT }' in extras


def test_about_uses_source_coordinates_and_does_not_invent_identity():
    text = source("apps/gui/ipodjs_settings.c")
    for position in ("0, 61", "128, 40", "14 + i * 106", "192, 153",
                     "137 + i * 13, 220"):
        assert position in text
    snapshot = body(text, "ipodjs_about_snapshot")
    for evidence in ('"Unavailable"', "MODEL_NAME", "rbversion", "volume_size",
                     "stat->total_entries", "row < 32", "sum <= size - free"):
        assert evidence in snapshot
    assert "sscanf" not in text  # unavailable in the native core link
    assert "ULONG_MAX" in body(text, "decimal_value")
    assert "Capacity breakdown unavailable" in text


def test_dialogs_preserve_long_questions_and_exclude_hold_and_plugins():
    text = source("apps/gui/ipodjs_settings.c")
    available = body(text, "ipodjs_settings_dialog_available")
    assert "!button_hold()" in available
    assert "ACTIVITY_PLUGIN" in available
    question = body(text, "ipodjs_settings_draw_confirmation")
    assert question.index("width > 296") < question.index("clear_display")
    assert "LANG_CONFIRM_WITH_BUTTON" not in question
    assert "LANG_CANCEL_WITH_ANY" not in question
    assert "default_button == 1 ? 152 : 92, 192" in question
    assert "default_button == 1 ? 73 : 57" in question
    # Original 5px right caps land at x=144 (Yes) and x=220 (No).
    assert 92 + 57 - 5 == 144
    assert 152 + 73 - 5 == 220
    yesno = source("apps/gui/yesno.c")
    assert "ipodjs_settings_draw_message" in yesno


def test_apple_dialog_confirms_selected_button_and_preserves_fallback():
    text = source("apps/gui/yesno.c")
    assert "yn[i].selected_button = tmo_default_res == YESNO_YES ? 0 : 1" in text
    assert "if (yn[SCREEN_MAIN].retail_confirmation)" in text
    assert "result = yn[SCREEN_MAIN].selected_button ?" in text
    assert "YESNO_NO : YESNO_YES" in text
    for action in ("ACTION_STD_PREV", "ACTION_STD_PREVREPEAT",
                   "ACTION_STD_NEXT", "ACTION_STD_NEXTREPEAT"):
        assert f"action == {action}" in text
    assert "result = tmo_default_res" in text
    assert "result = YESNO_USB" in text
    assert "result = YESNO_YES;" in text  # Non-Apple fallback is unchanged.


def test_apple_transitions_follow_elapsed_time_and_do_not_consume_input():
    text = source("apps/gui/ipodjs_ui.c")
    position = body(text, "ipodjs_ui_animation_position")
    assert "current_tick - start_tick" in position
    for name in ("ipodjs_ui_transition_present", "ipodjs_ui_preview_fade_present"):
        draw = body(text, name)
        assert "ipodjs_ui_animation_position" in draw
        assert "!button_queue_empty()" in draw
        for forbidden in ("button_get", "button_clear", "button_queue_count", "core_alloc"):
            assert forbidden not in draw


@pytest.fixture
def inventory_module():
    spec = importlib.util.spec_from_file_location("about_inventory",
        ROOT / "tools/ipodjs_about_inventory.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_inventory_counts_real_files_not_artwork_or_symlinks(tmp_path, inventory_module):
    for directory in (".rockbox", "Music", "Photos", "Videos"):
        (tmp_path / directory).mkdir()
    for filename, size in (("Music/a.FLAC", 2048), ("Music/cover.jpg", 9000),
                           ("Photos/photo.jpg", 1024), ("Videos/a.m4v", 4096),
                           (".rockbox/logo.mp4", 8192)):
        (tmp_path / filename).write_bytes(b"a" * size)
    (tmp_path / "Music/link.flac").symlink_to(tmp_path / "Music/a.FLAC")
    (tmp_path / "copy").symlink_to(tmp_path / "Music", target_is_directory=True)
    data = inventory_module.inventory(tmp_path)
    assert data.startswith("ipodjs-about-v2\ncount0\t1\ncount1\t1\ncount4\t1\n")
    allocated = lambda name: (tmp_path / name).stat().st_blocks // 2
    music = allocated("Music/a.FLAC") + allocated("Music/cover.jpg")
    apps = allocated("Videos/a.m4v") + allocated("Photos/photo.jpg")
    assert f"kib0\t{music}\n" in data and f"kib1\t{apps}\n" in data
    assert f"app\tNetflix\t{allocated('Videos/a.m4v')}\n" in data
    assert f"app\tPhotos\t{allocated('Photos/photo.jpg')}\n" in data
    assert "count2" not in data and "count5" not in data
    output = tmp_path / ".rockbox/ipodjs/about.tsv"
    inventory_module.write_inventory(tmp_path, output)
    assert output.read_text() == data


def test_inventory_failure_preserves_previous_snapshot(tmp_path, inventory_module, monkeypatch):
    (tmp_path / ".rockbox").mkdir()
    output = tmp_path / ".rockbox/about.tsv"
    output.write_text("previous")
    def fail(*args, **kwargs):
        raise OSError("unreadable directory")
    monkeypatch.setattr(inventory_module.os, "walk", fail)
    with pytest.raises(OSError):
        inventory_module.write_inventory(tmp_path, output)
    assert output.read_text() == "previous"


def test_deploy_prepares_about_snapshot_after_copying_package():
    text = source("tools/deploy_ipod6g_preserve_database.sh")
    assert text.index("rsync -rt") < text.index("ipodjs_about_inventory.py") < text.rindex("\nsync")


def test_about_and_grid_header_join_does_not_erase_four_body_rows():
    ui = source("apps/gui/ipodjs_ui.c")
    header = body(ui, "ipodjs_ui_draw_header_background")
    assert ("display->fillrect(0, 0, width,\n"
            "            MIN(header->height, IPODJS_UI_HEADER_HEIGHT))") in header
    grid = body(source("apps/root_menu.c"), "root_menu_video_draw_applications_ios3")
    assert "!global_settings.ui_engine_dark_mode && !button_hold()" in grid
    assert "IPODJS_UI_HEADER_HEIGHT - IPODJS_UI_RETAIL_MENU_HEADER_HEIGHT" in grid
    assert "{ 29, 95, 161 }" in grid


def test_about_overflow_scrolls_full_values_without_smaller_fonts_or_ellipsis():
    text = source("apps/gui/ipodjs_settings.c")
    draw = body(text, "text")
    assert "pixels > width && about_scroll.collecting" in draw
    assert "clipped_text" in draw
    scroll = body(text, "ipodjs_about_scroll")
    assert "ipodjs_ui_draw_retailos_background_rect" in scroll
    assert "button_hold() || !button_queue_empty()" in scroll
    for forbidden in ("open(", "audio_", "font_load", "malloc", "tagcache"):
        assert forbidden not in scroll


def test_indexed_video_is_counted_once_even_in_hidden_content(tmp_path, inventory_module):
    directory = tmp_path / ".rockbox/videolist"
    directory.mkdir(parents=True)
    (directory / "movie.mpg").write_bytes(b"x" * 3072)
    (directory / "index.tsv").write_text(
        "# source index\nvideo_id\tdevice_path\n"
        "1\t.rockbox/videolist/movie.mpg\n2\t.rockbox/videolist/movie.mpg\n")
    result = inventory_module.inventory(tmp_path)
    size = sum(path.stat().st_blocks // 2 for path in directory.iterdir())
    assert "count1\t1\n" in result and f"app\tNetflix\t{size}\n" in result


def test_media_index_cannot_escape_the_volume(tmp_path, inventory_module):
    directory = tmp_path / ".rockbox/videolist"
    directory.mkdir(parents=True)
    (directory / "index.tsv").write_text("device_path\n../../outside.mpg\n")
    with pytest.raises(ValueError, match="outside"):
        inventory_module.inventory(tmp_path)


def test_menu_overflow_uses_existing_scroller_and_cleans_up_on_exit():
    ui = source("apps/gui/ipodjs_ui.c")
    scroll = body(ui, "ipodjs_ui_menu_text_scroll")
    assert "pixels <= width" in scroll
    assert "lcd_getfont()" in scroll
    assert "putsxy_scroll_func" in scroll
    for forbidden in ("malloc", "font_load", "core_alloc", "button_get"):
        assert forbidden not in scroll
    lists = source("apps/gui/bitmap/list.c")
    assert "ipodjs_ui_stop_menu_text_scroll" in body(lists, "gui_synclist_scroll_stop")
    assert "ipodjs_ui_stop_menu_text_scroll" in body(ui, "ipodjs_ui_transition_begin_mode")


def test_onlyfans_and_its_external_media_are_other_not_app_rows(tmp_path, inventory_module):
    for directory in (".rockbox/onlyfans", ".rockbox/instagram", "YouTube", "Videos"):
        (tmp_path / directory).mkdir(parents=True)
    (tmp_path / "Videos/shared.mpg").write_bytes(b"x" * 8192)
    (tmp_path / "YouTube/clip.mpg").write_bytes(b"x" * 4096)
    (tmp_path / ".rockbox/instagram/feed.bmp").write_bytes(b"x" * 4096)
    (tmp_path / ".rockbox/onlyfans/library.tsv").write_text("media\n/Videos/shared.mpg\n")
    result = inventory_module.inventory(tmp_path)
    assert "OnlyFans" not in result
    assert "app\tNetflix" not in result
    assert "app\tYouTube\t4\n" in result
    assert "app\tInstagram\t4\n" in result
    assert "kib1\t8\n" in result


def test_app_binary_and_assets_are_counted_together(tmp_path, inventory_module):
    for directory in (".rockbox/rocks/apps", ".rockbox/ipodjs/youtube"):
        (tmp_path / directory).mkdir(parents=True)
    (tmp_path / ".rockbox/rocks/apps/youtube.rock").write_bytes(b"x" * 4096)
    (tmp_path / ".rockbox/ipodjs/youtube/logo.bmp").write_bytes(b"x" * 8192)
    assert "app\tYouTube\t12\n" in inventory_module.inventory(tmp_path)


def test_runtime_app_rows_are_bounded_and_onlyfans_is_rejected():
    text = source("apps/gui/ipodjs_settings.c")
    assert "ABOUT_MAX_APPS 256" in text
    assert "row < ABOUT_MAX_APPS + 16" in text
    assert 'strcmp(name, "OnlyFans")' in text
    assert "app_sum == kib[1]" in text
    assert '{"Music", "Apps", "Other"}' in text


def test_grid_overflow_keeps_the_existing_font_and_icon_positions():
    root = source("apps/root_menu.c")
    grid = body(root, "root_menu_video_draw_applications_ios3")
    assert "{ 29, 95, 161 }" in grid
    assert "root_menu_video_wps_font(false)" in grid
    assert "label_width > available" in grid
    scroll = body(root, "root_menu_video_grid_label_scroll")
    assert "button_hold() || !button_queue_empty()" in scroll
    for forbidden in ("read_bmp", "open(", "core_alloc", "font_load", "audio_"):
        assert forbidden not in scroll
