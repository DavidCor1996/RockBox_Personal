#!/usr/bin/env python3
"""Run the native H.264 presentation loop against a deterministic clock.

Compile the actual loop with mocked decoder, PCM clock and LCD costs. This
checks deadline placement and recovery from expensive screen updates without
claiming to emulate the device's decoder or measure real hardware throughput.
"""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "apps/video_playback.c"


def main():
    source = SOURCE.read_text()
    declarations = source[source.index("enum video_style"):source.index("struct video_resume_record")]
    timing = source[source.index("static uint32_t video_pts_ms("):source.index("static uint32_t video_duration_ms(")]
    loop = source[source.index("    while (sample < demux.num_samples && action == VIDEO_INPUT_NONE)"):source.index("    if (action == VIDEO_INPUT_EXIT)\n        result = 1;")]
    harness = PRELUDE + declarations + timing + SETUP + loop + CHECKS
    with tempfile.TemporaryDirectory(prefix="h264-av-sync-") as temporary:
        path = pathlib.Path(temporary)
        (path / "gate.c").write_text(harness)
        subprocess.run(["cc", "-std=gnu99", "-O2", str(path / "gate.c"), "-o", str(path / "gate")], check=True)
        subprocess.run([str(path / "gate")], check=True)


PRELUDE = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#define HZ 100
#define SEEK_SET 0
#define VIDEO_READ_BUFFER 4096
#define TIME_BEFORE(a,b) ((long)((a)-(b)) < 0)
static uint32_t now_ms, decode_ms, render_ms, shown, decoded_count, worst_late;
static uint32_t period, stall_frame;
#define current_tick ((long)(now_ms / 10))
struct video_timing { uint32_t run, in_run; uint64_t ticks; };
struct mp4v_stts_entry { uint32_t sample_count, sample_delta; };
struct mp4v_demux_res {
    uint32_t timescale, num_stts, num_samples, width, height;
    int nalu_len_size, codecdata_len;
    void *codecdata;
    struct mp4v_stts_entry stts[1];
};
#define sleep(t) (now_ms += (t) * 10)
#define video_pcm_get_clock_ms() now_ms
#define video_audio_failed() false
#define video_audio_is_active() true
#define video_pcm_empty() false
#define video_audio_stop() ((void)0)
#define video_audio_seek(t) (now_ms = (t))
#define video_input(...) VIDEO_INPUT_NONE
#define video_instagram_toggle_like(...) ((void)0)
#define video_tiktok_toggle_like(...) ((void)0)
#define video_twitch_chat_update(...) ((void)0)
#define video_twitch_chat_animate(...) ((void)0)
#define vpu_h264_close(...) ((void)0)
#define vpu_h264_open(...) ((void *)1)
#define vpu_h264_configure(...) 0
#define vpu_h264_get_frame(...) ((void)0)
#define splash(...) assert(!"unexpected playback error")
#define lseek(fd, offset, whence) 0
#define read(fd, buf, size) (size)
#define file_read_at(fd, buf, size, offset) (size)
static int mp4v_get_sample_offset(void *d, uint32_t sample, uint32_t *off, uint32_t *size)
{ *off = sample; *size = 1; return 0; }
static int mock_decode(void)
{ decoded_count++; now_ms += decode_ms; if (decoded_count == stall_frame) now_ms += 250; return 1; }
#define vpu_h264_decode_sample(...) mock_decode()
static void mock_draw(uint32_t pts)
{
    assert(now_ms + 2 >= pts);
    uint32_t late = now_ms > pts ? now_ms - pts : 0;
    if (late > worst_late) worst_late = late;
    shown++;
    now_ms += render_ms;
}
#define video_draw_frame(y,cb,cr,w,h,s,b,l,p,pts,...) mock_draw(pts)
"""
SETUP = r"""
static uint32_t video_sample_for_ms(struct mp4v_demux_res *d, uint32_t ms,
                                    struct video_timing *t)
{ t->ticks = ms / period * period; return ms / period; }
static void run(uint32_t frame_ms, uint32_t lcd_ms, uint32_t stall)
{
    period = frame_ms; render_ms = lcd_ms; stall_frame = stall;
    now_ms = shown = decoded_count = worst_late = 0; decode_ms = 12;
    struct mp4v_demux_res demux = { .timescale=1000, .num_stts=1,
        .num_samples=1800, .stts={{1800, frame_ms}} };
    struct video_timing timing = {0};
    struct video_launch launch = { .style=VIDEO_STYLE_YOUTUBE };
    uint32_t sample=0, last_sample=0, duration_ms=1800*frame_ms;
    long start_tick=0, pause_started=0, overlay_until=0, last_present_tick=0;
    bool paused=false, cpu_boosted=true, have_audio=true, audio_master=true;
    bool frame_presented=false;
    enum video_input_action action=VIDEO_INPUT_NONE;
    void *decoder=(void *)1, *decoder_buffer=0, *read_buffer=0, *scale_buffer=0;
    size_t decoder_size=0;
    int video_fd=0;
"""
CHECKS = r"""
    assert(decoded_count == demux.num_samples);
    assert(shown > demux.num_samples / 2);
    assert(now_ms < duration_ms + 100);
    if (!stall) assert(worst_late < 100);
    if (lcd_ms + decode_ms < frame_ms && !stall)
        assert(shown == demux.num_samples);
    else
        assert(shown < demux.num_samples);
    printf("AV sync passed: frame=%ums LCD=%ums stall=%u decoded=%u displayed=%u end_lag=%dms\n",
           frame_ms, lcd_ms, stall, decoded_count, shown, (int)(now_ms-duration_ms));
    return;
cleanup:
    assert(!"playback failed");
}
int main(void)
{
    run(42, 8, 0);   /* Netflix-like 24 fps: preserve every frame. */
    run(33, 25, 0);  /* 30 fps with costly scaling/overlays: catch up. */
    run(33, 25, 200); /* Recover after a transient disk/decode stall. */
    return 0;
}
"""

if __name__ == "__main__":
    main()
