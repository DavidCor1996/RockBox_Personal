from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
MPEGPLAYER_SOURCE = (
    REPO_ROOT / "apps" / "plugins" / "mpegplayer" / "mpegplayer.c"
)
MPEG_PARSER_SOURCE = (
    REPO_ROOT / "apps" / "plugins" / "mpegplayer" / "mpeg_parser.c"
)


def test_hold_seek_has_release_event_timeout():
    source = MPEGPLAYER_SOURCE.read_text(encoding="utf-8")
    start = source.index("static int osd_ff_rw(")
    end = source.index("static int osd_stream_status(", start)
    seek_loop = source[start:end]

    assert "mpeg_button_get(FF_REWIND_BUTTON_TIMEOUT)" in seek_loop
    assert "mpeg_button_get(TIMEOUT_BLOCK)" not in seek_loop
    assert "new_btn == BUTTON_NONE" in seek_loop
    assert "step = MAX(step, duration / 200);" in seek_loop
    assert "step = MAX(step, MIN_FF_REWIND_STEP);" in seek_loop
    button_start = source.index("static int osd_seek_btn(")
    button_end = source.index("static void osd_seek_time(", button_start)
    assert "osd_show(OSD_HIDE);" in source[button_start:button_end]


def test_volume_change_skips_empty_overlay_rectangle():
    source = MPEGPLAYER_SOURCE.read_text(encoding="utf-8")
    refresh_start = source.index("static void osd_refresh_volume(void)")
    refresh_end = source.index(
        "static void osd_refresh_status(void)", refresh_start
    )
    set_start = source.index("static void osd_set_volume(int delta)")
    set_end = source.index("static int osd_play(", set_start)

    assert "if (vo_rect_empty(&osd.vol_rect))" in source[
        refresh_start:refresh_end
    ]
    assert "if (!vo_rect_empty(&osd.vol_rect))" in source[set_start:set_end]


def test_pause_and_resume_leave_full_screen_video_uncovered():
    source = MPEGPLAYER_SOURCE.read_text(encoding="utf-8")
    pause_start = source.index("static int osd_pause(void)")
    pause_end = source.index("static void osd_resume(void)", pause_start)
    resume_end = source.index("static void osd_stop(void)", pause_end)
    pause = source[pause_start:pause_end]
    resume = source[pause_end:resume_end]

    assert "osd_set_status(OSD_STATUS_PAUSED | OSD_NODRAW);" in pause
    assert "osd_show(OSD_HIDE);" in pause
    assert "osd_set_status(OSD_STATUS_PLAYING | OSD_NODRAW);" in resume
    assert "stream_resume();" in resume
    assert "osd_show(OSD_HIDE);" in resume


def test_long_video_seek_scans_are_bounded():
    source = MPEG_PARSER_SOURCE.read_text(encoding="utf-8")

    assert "#define MPEG_SEEK_SCAN_LIMIT (1024 * 1024)" in source
    assert source.count("MPEG_SEEK_SCAN_LIMIT") >= 3
    assert "sk.len = MIN((sk.dir < 0) ? pos_new - pos_left" in source
    assert "sk.len = MIN(sk.pos, (off_t)MPEG_SEEK_SCAN_LIMIT);" in source


def test_ipod_menu_stops_video_instead_of_opening_settings():
    source = MPEGPLAYER_SOURCE.read_text(encoding="utf-8")
    start = source.index("        case MPEG_MENU:")
    end = source.index("#ifdef MPEG_SHOW_OSD", start)
    menu_case = source[start:end]

    stop = menu_case.index("next_action = VIDEO_STOP;")
    menu = menu_case.index("result = mpeg_menu();")
    assert stop < menu
    assert "osd_stop();" in menu_case[stop:menu]
