"""Opt-in input diagnosis must not add I/O to WPS paint callbacks."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_observations_are_bounded_and_file_io_is_exit_only():
    source = (ROOT / "apps/root_menu.c").read_text()
    cached = source.split("static void ipodjs_wps_input_record(", 1)[1]
    cached = cached.split("static void ipodjs_wps_input_save(", 1)[0]
    for forbidden in ("open(", "close(", "fdprintf(", "core_alloc(",
                      "audio_stop(", "tagcache_", "sleep("):
        assert forbidden not in cached
    assert "ipodjs_wps_input_records[64]" in source
    assert source.count("ipodjs_wps_input_save();") == 1
    leave = source.split("void root_menu_ipodjs_leave_wps_frame(void)", 1)[1]
    leave = leave.split("static int root_menu_video_row_height", 1)[0]
    assert "ipodjs_wps_input_save();" in leave
    assert 'ROCKBOX_DIR "/ipodjs-wps-input.enable"' in source
