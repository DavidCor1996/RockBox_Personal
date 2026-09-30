#!/usr/bin/env python3
"""Execute ported C decision paths with stubbed external subsystems.

Set ROCKBOX_TEST_SOURCE to a saved pre-port source directory for negative
controls. These tests exercise extracted production code, not reimplemented
copies of its conditions. Device playback remains a separate acceptance test.
"""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(os.environ.get("ROCKBOX_TEST_SOURCE",
                          Path(__file__).resolve().parents[2]))


def source(path):
    return (ROOT / path).read_text()


def between(text, start, end):
    return text.split(start, 1)[1].split(end, 1)[0]


class UpstreamLowRiskTests(unittest.TestCase):
    def run_c(self, code):
        with tempfile.TemporaryDirectory(prefix="rb-lowrisk-") as directory:
            path = Path(directory)
            unit = path / "check.c"
            unit.write_text("#include <assert.h>\n#include <stdbool.h>\n"
                            "#include <stdint.h>\n#include <stddef.h>\n"
                            "#include <stdio.h>\n" + code)
            compiled = subprocess.run(["cc", "-std=gnu99", "-O1", "-g",
                            "-fsanitize=undefined",
                            "-fno-sanitize-recover=all", str(unit),
                            "-o", str(path / "check")],
                           capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(path / "check")],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_flac_seek_fallback(self):
        text = source("lib/rbcodec/codecs/flac.c")
        fragment = between(text, "    codec_set_replaygain(ci->id3);",
                           "    ci->set_elapsed(elapsedtime);")
        self.run_c(r"""
struct metadata { unsigned frequency; } metadata = { 48000 };
struct api { struct metadata *id3; } api = { &metadata }, *ci = &api;
struct flac { unsigned samplenumber, blocksize; };
static int offsets, times;
static unsigned time_sample;
static bool offset_ok, time_ok;
static bool flac_seek_offset(struct flac *f, unsigned offset)
{ (void)f; (void)offset; ++offsets; return offset_ok; }
static bool flac_seek(struct flac *f, unsigned sample)
{ (void)f; ++times; time_sample = sample; return time_ok; }
static unsigned run(unsigned samplesdone, unsigned elapsedtime)
{
    struct flac fc = { 48000, 48000 };
    offsets = times = 0;
""" + fragment + r"""
    return elapsedtime;
}
int main(void)
{
    time_ok = true;
    assert(run(0, 0) == 0 && offsets == 0 && times == 1);
    assert(run(123, 1500) == 1500 && offsets == 1 && times == 1);
    assert(time_sample == 72000);
    offset_ok = true;
    assert(run(123, 1500) == 2000 && offsets == 1 && times == 0);
    offset_ok = time_ok = false;
    assert(run(123, 1500) == 0 && times == 1);
    return 0;
}
""")

    def test_plugin_return_dispatchers(self):
        text = source("apps/root_menu.c")
        for name in ("load_plugin_screen", "load_plugin_path_screen"):
            with self.subTest(dispatcher=name):
                function = text.rsplit("static int " + name + "(", 1)[1]
                fragment = "if (ret == PLUGIN_USB_CONNECTED" + function.split(
                    "if (ret == PLUGIN_USB_CONNECTED", 1)[1]
                # The next branch differs between the two dispatchers.
                fragment = fragment.split("        else", 2)
                decision = fragment[0] + "        else" + fragment[1]
                self.run_c(r"""
enum { PLUGIN_USB_CONNECTED, PLUGIN_ERROR, PLUGIN_GOTO_WPS,
       GO_TO_ROOT, GO_TO_WPS, GO_TO_BROWSEPLUGINS };
static int dispatch(int ret, int old_global)
{
    int ret_val = -1;
""" + decision + r"""
    return ret_val;
}
int main(void)
{
    assert(dispatch(PLUGIN_GOTO_WPS, GO_TO_WPS) == GO_TO_BROWSEPLUGINS);
    assert(dispatch(PLUGIN_GOTO_WPS, GO_TO_ROOT) == GO_TO_WPS);
    assert(dispatch(PLUGIN_ERROR, GO_TO_WPS) == GO_TO_ROOT);
    assert(dispatch(PLUGIN_USB_CONNECTED, GO_TO_WPS) == GO_TO_ROOT);
    return 0;
}
""")

    def test_paused_track_clears_pcm_after_decoder_halt(self):
        text = source("apps/playback.c").split(
            "static void audio_start_playback(", 1)[1]
        fragment = "halt_decoding_track(true);" + between(
            text, "halt_decoding_track(true);",
            "        /* Set after track finish event")
        self.run_c(r"""
enum { PLAY_PLAYING, PLAY_PAUSED, AUDIO_START_RESTART = 1,
       TEF_NONE, PLAYING_ID3, TRACK_LIST_CLEAR_ALL, TRACK_CHANGE_MANUAL };
static bool halted;
static int stops, changes, track_event_flags;
static bool ff_rw_mode;
struct metadata { unsigned elapsed, offset; bool skip_resume_adjustments; };
static struct metadata track = { 4000, 1234, true };
static void halt_decoding_track(bool wait) { assert(wait); halted = true; }
static void pcmbuf_play_stop(void) { assert(halted); ++stops; }
static struct metadata *id3_get(int n) { (void)n; return &track; }
static void track_list_clear(int n) { (void)n; }
static void pcmbuf_update_frequency(void) { }
static void audio_playlist_track_finish(void) { }
static void pcmbuf_start_track_change(int n) { (void)n; ++changes; }
static void wipe_track_metadata(bool wipe) { (void)wipe; }
static void run(int old_status, unsigned flags)
{
    struct metadata resume = { 0 };
    bool skip_resume_adjustments = false;
    halted = false; stops = changes = 0;
""" + fragment + r"""
    if (flags & AUDIO_START_RESTART)
        assert(resume.elapsed == 4000 && resume.offset == 1234 &&
               skip_resume_adjustments);
}
int main(void)
{
    run(PLAY_PAUSED, 0); assert(stops == 1 && changes == 1);
    run(PLAY_PLAYING, 0); assert(stops == 0 && changes == 1);
    run(PLAY_PLAYING, AUDIO_START_RESTART); assert(stops == 1 && changes == 0);
    run(PLAY_PAUSED, AUDIO_START_RESTART); assert(stops == 1 && changes == 0);
    return 0;
}
""")

    def test_empty_playlist_percentage(self):
        text = source("apps/gui/skin_engine/skin_tokens.c")
        fragment = between(text, "case SKIN_TOKEN_PLAYLIST_PERCENT:\n        {",
                           "            if (intval")
        self.run_c(r"""
static int amount, position;
static int playlist_amount(void) { return amount; }
static int playlist_get_display_index(void) { return position; }
static const char *percent(int offset)
{
    static char value[32];
""" + fragment + r"""
    snprintf(value, sizeof(value), "%d", percentage);
    return value;
}
int main(void)
{
    amount = 0; position = 1; assert(percent(0) == NULL);
    amount = -1; assert(percent(0) == NULL);
    amount = 4; position = 2;
    assert(percent(0)[0] == '5' && percent(0)[1] == '0');
    assert(percent(1)[0] == '7' && percent(1)[1] == '5');
    return 0;
}
""")

    def test_fft_waits_until_frame_due(self):
        text = source("apps/plugins/fft/fft.c")
        fragment = "long delay = fft_draw();" + between(
            text, "long delay = fft_draw();", "        switch (button)")
        self.run_c(r"""
#define TIMEOUT_NOBLOCK 0
#define ARRAYLEN(a) (sizeof(a) / sizeof((a)[0]))
static int plugin_contexts[2];
static long next_delay, waited;
static int yields;
static long fft_draw(void) { return next_delay; }
static void do_yield(void) { ++yields; }
static struct api { void (*yield)(void); } api = { do_yield }, *rb = &api;
static int pluginlib_getaction(long delay, int *contexts, size_t count)
{ assert(contexts == plugin_contexts && count == 2); waited = delay; return 0; }
static void run(void)
{
""" + fragment + r"""
    assert(button == 0);
}
int main(void)
{
    next_delay = 5; run(); assert(waited == 5 && yields == 0);
    next_delay = 0; run(); assert(waited == 0 && yields == 1);
    next_delay = -3; run(); assert(waited == 0 && yields == 2);
    return 0;
}
""")

    def test_empty_playlist_progressbar(self):
        text = source("apps/gui/skin_engine/skin_display.c").split(
            "void draw_progressbar(", 1)[1]
        fragment = "unsigned long " + between(
            text, "unsigned long ", "    if (!pb->horizontal)")
        self.run_c(r"""
#define CONFIG_TUNER 0
#define HORIZONTAL 0
#define MAX_PEAK 32768
enum { SOUND_VOLUME, SKIN_TOKEN_VOLUMEBAR, SKIN_TOKEN_BATTERY_PERCENTBAR,
       SKIN_TOKEN_PEAKMETER_LEFTBAR, SKIN_TOKEN_PEAKMETER_RIGHTBAR,
       SKIN_TOKEN_PLAYLIST_PERCENTBAR, SKIN_TOKEN_LIST_SCROLLBAR,
       SKIN_TOKEN_SETTINGBAR };
struct font { int height; } font = { 12 };
struct vp { int font; } viewport;
struct progressbar { int type, setting, setting_offset; } bar;
struct track { unsigned long length, elapsed; } track;
struct state { int ff_rewind_count; } playback_state;
struct status { int volume; } global_status;
static int amount, position;
static int playlist_amount(void) { return amount; }
static int playlist_get_display_index(void) { return position; }
static struct font *font_get(int n) { (void)n; return &font; }
static int sound_min(int n) { (void)n; return -60; }
static int sound_max(int n) { (void)n; return 0; }
static int battery_level(void) { return 50; }
static void peak_meter_current_vals(int *l, int *r) { *l = *r = 0; }
static int peak_meter_scale_value(int n, int max) { (void)max; return n; }
static void skinlist_get_scrollbar(int *val, int *min, int *max)
{ *val = *min = *max = 0; }
static void get_setting_info_for_bar(int s, int o, int *count, int *val)
{ (void)s; (void)o; *count = 1; *val = 0; }
static void run(int type, unsigned expected_length, unsigned expected_end)
{
    struct vp *vp = &viewport;
    struct progressbar *pb = &bar;
    struct track *id3 = &track;
    struct state *state = &playback_state;
    int height = 10, y = 0, line = 0;
    bar.type = type;
""" + fragment + r"""
    assert(length == expected_length && end == expected_end);
}
int main(void)
{
    amount = 0; position = 1;
    run(SKIN_TOKEN_PLAYLIST_PERCENTBAR, 1, 0);
    amount = 4; position = 2;
    run(SKIN_TOKEN_PLAYLIST_PERCENTBAR, 4, 2);
    run(SKIN_TOKEN_LIST_SCROLLBAR, 1, 0);
    run(SKIN_TOKEN_SETTINGBAR, 1, 0);
    return 0;
}
""")


if __name__ == "__main__":
    unittest.main()
