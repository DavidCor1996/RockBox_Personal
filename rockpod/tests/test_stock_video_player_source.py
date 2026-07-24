from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
PLAYER = REPO_ROOT / "apps" / "plugins" / "openh264_player.c"


def _source() -> str:
    return PLAYER.read_text(encoding="utf-8")


def test_video_player_uses_verified_apple_osd_assets():
    source = _source()
    for asset in (
        "status-header.apple.320x24x24.bmp",
        "status-playback.apple.20x32x24.bmp",
        "status-battery.apple.26x65x24.bmp",
        "progress-frame.apple.200x22x32.bmp",
        "progress-fill.apple.200x22x32.bmp",
        "progress-fill-cap.apple.16x16x24.bmp",
    ):
        assert asset in source

    assert "raw_osd_draw_pause_icon" not in source
    assert "raw_osd_draw_battery" not in source
    assert "raw_osd_gradient" not in source


def test_video_player_maps_stock_pause_rewind_and_scrubber_controls():
    source = _source()
    for action in (
        "ACTION_WPS_PLAY",
        "ACTION_WPS_SEEKBACK",
        "ACTION_WPS_SEEKFWD",
        "ACTION_WPS_STOPSEEK",
        "ACTION_WPS_SKIPPREV",
        "ACTION_WPS_BROWSE",
    ):
        assert action in source

    assert "RAW_INPUT_TOGGLE_SCRUBBER" in source
    assert "raw_osd.scrubbing" in source
    assert "segment_move_out" in source
    assert "raw_audio_start_at_frame" in source


def test_pause_and_seek_do_not_block_on_next_segment_prefetch():
    source = _source()
    state_machine = source[source.index("while (frame_index < frames)") :]

    assert "raw_prefetch_close(&prefetch);" in state_machine
    assert "wait_for_resume" not in source
