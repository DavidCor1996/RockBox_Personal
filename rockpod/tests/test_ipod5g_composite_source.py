from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def _read(relative):
    return (REPO_ROOT / relative).read_text(encoding="utf-8", errors="replace")


def test_ipod5g_registers_persistent_composite_policy():
    settings_h = _read("apps/settings.h")
    settings_list = _read("apps/settings_list.c")
    settings = _read("apps/settings.c")

    assert "defined(IPOD_6G) || defined(IPOD_VIDEO)" in settings_h
    assert "IPOD_COMPOSITE_VIDEO_AUTO" in settings_list
    assert "settings_apply_ipod_videoout" in settings_list
    assert "settings_apply_ipod_videoout(global_settings.composite_video_output)" in settings


def test_ipod5g_player_maps_lcd_auto_and_tv_at_launch():
    player = _read("apps/video_playback_5g.c")
    detector = player.split(
        "static uint8_t video5_detect_output_display(void)", 1
    )[1].split("static bool video5_set_region", 1)[0]

    assert "IPOD_COMPOSITE_VIDEO_OFF" in detector
    assert "return VIDEO5_DISPLAY_LCD;" in detector
    assert "IPOD_COMPOSITE_VIDEO_ON" in detector
    assert "return VIDEO5_DISPLAY_TV;" in detector
    assert "GPIOA_INPUT_VAL & 0x10" in detector


def test_ipod5g_ipodjs_quick_settings_exposes_composite_output():
    root_menu = _read("apps/root_menu.c")

    assert "(defined(IPOD_6G) || defined(IPOD_VIDEO))" in root_menu
    assert 'return "Composite Out";' in root_menu
    assert 'static const char * const states[] = {"LCD", "Auto", "TV"};' in root_menu
    assert "settings_apply_ipod_videoout(value);" in root_menu
