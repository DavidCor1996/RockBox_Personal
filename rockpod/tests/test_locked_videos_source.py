from pathlib import Path


ROOT_MENU = Path(__file__).parents[2] / "apps" / "root_menu.c"
PHOTOS = Path(__file__).parents[2] / "apps" / "plugins" / "photos.c"


def test_locked_videos_are_pin_gated_and_excluded_from_public_previews():
    source = ROOT_MENU.read_text(encoding="utf-8")

    assert '"virtual:locked", "Locked Videos"' in source
    assert "videos_unlock_locked_category" in source
    assert "videos_prompt_pin_wheel" in source
    assert '"Enter 4-digit code"' in source
    assert "VIDEO_LIST_LOCK_PIN" in source
    assert "video_manifest_entry_locked(fields12)" in source
    assert "only explicitly public preview entries" in source


def test_locked_video_pin_uses_photo_assets_and_fullscreen_surface():
    root_source = ROOT_MENU.read_text(encoding="utf-8")
    photos_source = PHOTOS.read_text(encoding="utf-8")
    shared_assets = (
        "fast-scroll-blank.apple.95x82x32.bmp",
        "search-field.apple.97x32x24.bmp",
        "search-selected.apple.97x32x24.bmp",
    )

    for asset in shared_assets:
        assert asset in photos_source
        assert asset in root_source
    assert "lcd_set_viewport(NULL);" in root_source
    assert "lcd_set_backdrop(NULL);" in root_source
    assert "lcd_clear_display();" in root_source
    assert "case ACTION_STD_MENU:" in root_source


def test_photos_and_videos_share_the_first_privacy_pin():
    source = PHOTOS.read_text(encoding="utf-8")

    assert 'ROCKBOX_DIR "/videolist/locked.pin"' in source
    assert "photos_get_or_create_shared_pin" in source
    assert "photos_write_shared_pin" in source


def test_settings_menu_can_use_the_shared_video_pin_gate():
    source = ROOT_MENU.read_text(encoding="utf-8")
    settings_h = (ROOT_MENU.parent / "settings.h").read_text(encoding="utf-8")
    settings_list = (ROOT_MENU.parent / "settings_list.c").read_text(encoding="utf-8")
    theme_menu = (ROOT_MENU.parent / "menus" / "theme_menu.c").read_text(encoding="utf-8")
    launcher = source.split(
        "static int root_menu_video_launch_menu_item", 1
    )[1].split("struct root_menu_video_extras_item", 1)[0]

    assert "videos_unlock_settings_menu" in source
    assert 'videos_unlock_with_shared_pin("Unlock Settings"' in source
    assert "global_settings.ui_engine_lock_settings" in source
    assert "videos_settings_lock_active()" in launcher
    assert launcher.index("videos_unlock_settings_menu()") < launcher.index(
        "root_menu_video_settings_menu()"
    )
    assert "bool ui_engine_lock_settings" in settings_h
    assert '"ui engine lock settings"' in settings_list
    assert '"Lock Settings Menu"' in theme_menu


def test_netflix_manifest_keeps_show_and_season_art_separate():
    source = ROOT_MENU.read_text(encoding="utf-8")

    assert "video_parse_manifest_line_v5" in source
    assert "char *fields[22]" in source
    assert "fields[season ? 21 : 20]" in source
    assert "video_manifest_hierarchy_art_id" in source
    assert 'line, false, fields[0], hierarchy_art_id' in source
    assert 'line, true, fields[0], hierarchy_art_id' in source
