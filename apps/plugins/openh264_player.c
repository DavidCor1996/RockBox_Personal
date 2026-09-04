/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    <| \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockpod Contributors
 *
 * Video viewer entry point for iPod 6G/7G hardware H.264 and legacy RVP.
 *
 * It handles two formats:
 * - .rvp: preconverted raw YUV420 video plus signed 16-bit stereo PCM sidecar.
 * - .m4v/.mp4/.mov: measured Apple-iPod H.264/AAC contract decoded by VPU-B.
 * - .h264: Annex-B scan/profile diagnostic path.
 *
 ****************************************************************************/

#include "plugin.h"
#include "lib/helper.h"
#include "netflix_intro.h"
#include "settings.h"

#define PROFILE_LOG ROCKBOX_DIR "/openh264/openh264_profile.log"
#define PROFILE_DIR ROCKBOX_DIR "/openh264"
#define SCAN_BUFSIZE (16 * 1024)
#define RAW_DEFAULT_WIDTH 320
#define RAW_DEFAULT_HEIGHT 240
#define RAW_DEFAULT_FPS 20
#define RAW_DEFAULT_SAMPLE_RATE 44100
#define RAW_DEFAULT_CHANNELS 2
#define RAW_AUDIO_CHUNK (32 * 1024)
#define RAW_PREFETCH_CHUNK (64 * 1024)
#define RAW_MAX_SEGMENTS 256
#define RVP_MARKER_BUFSIZE 32768
#define RAW_PCM_CHANNEL PCM_MIXER_CHAN_PLAYBACK
#define RAW_OSD_SHOW_TICKS (HZ * 2)
#define RAW_VOLUME_SHOW_TICKS ((HZ * 6) / 5)
#define RAW_OSD_HEADER_HEIGHT 24
#define RAW_OSD_TITLE_SIZE 96
#define RAW_STOCK_ASSET_DIR ROCKBOX_DIR "/ipodjs/apple"
#define RAW_RESUME_MAGIC 0x52565031u
#define RAW_RESUME_FILE \
    PLUGIN_APPS_DATA_DIR "/openh264-resume-%08lx.dat"
#define RAW_STOCK_PLAYBACK_W 20
#define RAW_STOCK_PLAYBACK_FRAME_H 16
#define RAW_STOCK_BATTERY_W 26
#define RAW_STOCK_BATTERY_FRAME_H 13
#define RAW_STOCK_BATTERY_FRAMES 5
#define RAW_STOCK_PROGRESS_W 200
#define RAW_STOCK_PROGRESS_H 22
#define RAW_STOCK_PROGRESS_CAP_W 16
#define RAW_STOCK_PROGRESS_CAP_H 16
#define RAW_VOLUME_CARD_W 180
#define RAW_VOLUME_CARD_H 45
#define RAW_VOLUME_ICON_W 24
#define RAW_VOLUME_ICON_H 21
#define RAW_VOLUME_ICON_FRAMES 4
#define RAW_VOLUME_SLIDER_W 117
#define RAW_VOLUME_SLIDER_H 5
#define RAW_VOLUME_SLIDER_END_W 3
#define RAW_NETFLIX_OVERLAY_H 64
#define RAW_NETFLIX_RED LCD_RGBPACK(180, 19, 29)
#define RAW_NETFLIX_BG LCD_RGBPACK(20, 20, 20)
#define RAW_NETFLIX_TRACK LCD_RGBPACK(72, 72, 72)
#define RAW_NETFLIX_INDEX ROCKBOX_DIR "/videolist/index.tsv"
#define RAW_NETFLIX_MARKER_FIELDS 27
#define H264_TIKTOK_PREFIX "-ipodtiktok:"
#define H264_TIKTOK_PREFIX_LEN (sizeof(H264_TIKTOK_PREFIX) - 1)
#define H264_TIKTOK_STATE \
    PLUGIN_APPS_DATA_DIR "/.ipodtiktok_h264_state.dat"

enum raw_fit_mode {
    RAW_FIT_UNKNOWN = 0,
    RAW_FIT_CONTAIN,
};

struct raw_video_config {
    int width;
    int height;
    int fps;
    int sample_rate;
    int channels;
    enum raw_fit_mode fit;
};

struct raw_render_area {
    int src_width;
    int src_height;
    int dst_x;
    int dst_y;
    int dst_width;
    int dst_height;
    bool scale;
};

struct raw_resume_record {
    uint32_t magic;
    uint32_t path_crc;
    int32_t frame;
    int32_t total_frames;
};

struct raw_osd_state {
    bool visible;
    bool volume_visible;
    bool paused;
    bool seeking;
    bool scrubbing;
    bool needs_redraw;
    bool hold_logged;
    long hide_tick;
    long volume_hide_tick;
    long current_frame;
    long total_frames;
    int fps;
};

enum raw_netflix_skip_kind {
    RAW_NETFLIX_SKIP_NONE = 0,
    RAW_NETFLIX_SKIP_INTRO,
    RAW_NETFLIX_SKIP_CREDITS,
};

struct raw_stock_assets {
    bool tried;
    bool loaded;
    struct bitmap header;
    struct bitmap playback;
    struct bitmap battery;
    struct bitmap progress_frame;
    struct bitmap progress_fill;
    struct bitmap progress_fill_cap;
};

struct raw_volume_assets {
    bool tried;
    bool loaded;
    struct bitmap backdrop;
    struct bitmap icons;
    struct bitmap slider_backdrop;
    struct bitmap slider_fill;
    struct bitmap slider_end;
};

static const unsigned char *pcm_cursor;
static size_t pcm_remaining;
static size_t raw_audio_bytes_per_frame;

struct raw_segment {
    char yuv[MAX_PATH];
    char pcm[MAX_PATH];
};

struct raw_prefetch {
    int fd;
    unsigned char *buf;
    off_t size;
    size_t done;
    bool active;
    bool complete;
    bool failed;
};

static struct raw_segment raw_segments[RAW_MAX_SEGMENTS];
static off_t raw_segment_pcm_sizes[RAW_MAX_SEGMENTS];
static long raw_segment_frame_counts[RAW_MAX_SEGMENTS];
static struct raw_osd_state raw_osd;
static struct raw_stock_assets raw_stock_assets;
static struct raw_volume_assets raw_volume_assets;
static fb_data raw_stock_header_data[LCD_WIDTH * RAW_OSD_HEADER_HEIGHT];
static fb_data raw_stock_playback_data[RAW_STOCK_PLAYBACK_W *
                                       RAW_STOCK_PLAYBACK_FRAME_H * 2];
static fb_data raw_stock_battery_data[RAW_STOCK_BATTERY_W *
                                      RAW_STOCK_BATTERY_FRAME_H *
                                      RAW_STOCK_BATTERY_FRAMES];
static fb_data raw_stock_progress_frame_data[RAW_STOCK_PROGRESS_W *
                                             RAW_STOCK_PROGRESS_H +
                                             (RAW_STOCK_PROGRESS_W *
                                              RAW_STOCK_PROGRESS_H) / 4];
static fb_data raw_stock_progress_fill_data[RAW_STOCK_PROGRESS_W *
                                            RAW_STOCK_PROGRESS_H +
                                            (RAW_STOCK_PROGRESS_W *
                                             RAW_STOCK_PROGRESS_H) / 4];
static fb_data raw_stock_progress_fill_cap_data[
    RAW_STOCK_PROGRESS_CAP_W * RAW_STOCK_PROGRESS_CAP_H];
static fb_data raw_volume_backdrop_data[RAW_VOLUME_CARD_W * RAW_VOLUME_CARD_H];
static fb_data raw_volume_icons_data[RAW_VOLUME_ICON_W *
                                     RAW_VOLUME_ICON_H *
                                     RAW_VOLUME_ICON_FRAMES];
static fb_data raw_volume_slider_backdrop_data[RAW_VOLUME_SLIDER_W *
                                               RAW_VOLUME_SLIDER_H];
static fb_data raw_volume_slider_fill_data[RAW_VOLUME_SLIDER_W *
                                           RAW_VOLUME_SLIDER_H];
static fb_data raw_volume_slider_end_data[RAW_VOLUME_SLIDER_END_W *
                                          RAW_VOLUME_SLIDER_H];
static char rvp_marker_buf[RVP_MARKER_BUFSIZE + 1];
static char rvp_seg_video[RAW_MAX_SEGMENTS][MAX_PATH];
static char rvp_seg_audio[RAW_MAX_SEGMENTS][MAX_PATH];
static unsigned char *raw_pool;
static size_t raw_pool_size;
static bool raw_pool_acquired;
static bool raw_audio_output_active;
static int raw_saved_audio_status;
static unsigned long raw_saved_elapsed;
static unsigned long raw_saved_offset;
static unsigned int raw_saved_mixer_freq;
static const char *raw_current_path;
static bool raw_netflix_launch;
static enum raw_netflix_skip_kind raw_netflix_skip_active;
static long raw_netflix_intro_start_frame;
static long raw_netflix_intro_end_frame;
static long raw_netflix_credits_start_frame;

#if defined(IPOD_6G) && !defined(SIMULATOR)
static void raw_video_restore_composite_output(void)
{
    const struct settings_list *setting;
    const struct choice_setting *choice;
    int mode;

    if (!rb || !rb->global_settings)
        return;

    setting = rb->find_setting(&rb->global_settings->composite_video_output);
    if (!setting)
        return;
    if ((setting->flags & F_CHOICE_SETTING) == 0)
        return;
    choice = setting->choice_setting;
    if (!choice || !choice->option_callback)
        return;

    mode = rb->global_settings->composite_video_output;
    if (mode < 0 || mode > 2)
        mode = 0;

    /*
     * Toggle composite video output reset path that mirrors Settings →
     * Composite Video Output off/on behavior (this fixes the post-exit white
     * frame seen on 6G when exiting MP4/M4V/MOV playback).
     */
    choice->option_callback(0);
    choice->option_callback(mode);
}
#endif

static void raw_audio_stop(void);

static void raw_wait_for_media_idle(void)
{
    enum channel_status status;
    long deadline = *rb->current_tick + HZ / 2;

    while (*rb->current_tick < deadline)
    {
        status = rb->mixer_channel_status(RAW_PCM_CHANNEL);
        if (status == CHANNEL_STOPPED && !rb->pcm_is_playing())
            return;
        rb->sleep(1);
    }
}

static void append_raw_audio_state(const char *stage)
{
    int fd = rb->open(PROFILE_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
    {
        rb->mkdir(PROFILE_DIR);
        fd = rb->open(PROFILE_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    }

    if (fd < 0)
        return;

    rb->fdprintf(fd,
                 "mode=raw_audio_state stage=%s clip=\"%s\" "
                 "audio_status=%d pcm_playing=%d mixer_freq=%u "
                 "mix_status=%d mix_waiting=%lu saved_status=%d "
                 "saved_elapsed=%lu saved_offset=%lu pool=%p pool_size=%lu\n",
                 stage ? stage : "", raw_current_path ? raw_current_path : "",
                 rb->audio_status(), rb->pcm_is_playing() ? 1 : 0,
                 rb->mixer_get_frequency(),
                 rb->mixer_channel_status(RAW_PCM_CHANNEL),
                 (unsigned long)rb->mixer_channel_get_bytes_waiting(RAW_PCM_CHANNEL),
                 raw_saved_audio_status, raw_saved_elapsed, raw_saved_offset,
                 raw_pool, (unsigned long)raw_pool_size);
    rb->close(fd);
}

static void raw_capture_playback_state(void)
{
    struct mp3entry *id3 = rb->audio_current_track();
    int file_pos;

    raw_saved_audio_status = rb->audio_status();
    raw_saved_elapsed = id3 != NULL ? id3->elapsed : 0;
    file_pos = rb->audio_get_file_pos();
    raw_saved_offset = file_pos > 0 ? (unsigned long)file_pos : 0;
    append_raw_audio_state("prepare-before-stop");

    raw_saved_mixer_freq = rb->mixer_get_frequency();
}

static void raw_prepare_output(void)
{
    backlight_ignore_timeout();
    rb->backlight_on();
    raw_audio_stop();
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
    rb->pcmbuf_fade(false, true);
    raw_audio_output_active = true;
    append_raw_audio_state("prepare-after-stop");
}

enum playback_action {
    PLAYBACK_NONE = 0,
    PLAYBACK_EXIT,
    PLAYBACK_TOGGLE_PAUSE,
};

enum raw_input_command {
    RAW_INPUT_NONE = 0,
    RAW_INPUT_EXIT,
    RAW_INPUT_TOGGLE_PAUSE,
    RAW_INPUT_VOLUME_CHANGED,
    RAW_INPUT_SHOW_OSD,
    RAW_INPUT_TOGGLE_SCRUBBER,
    RAW_INPUT_SEEK_BACK,
    RAW_INPUT_SEEK_FORWARD,
    RAW_INPUT_COMMIT_SEEK,
    RAW_INPUT_RESTART_VIDEO,
    RAW_INPUT_SKIP_INTRO,
    RAW_INPUT_SKIP_CREDITS,
};

static void append_raw_control_state(const char *stage)
{
    int fd = rb->open(PROFILE_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
    {
        rb->mkdir(PROFILE_DIR);
        fd = rb->open(PROFILE_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    }

    if (fd < 0)
        return;

    rb->fdprintf(fd,
                 "mode=raw_controls stage=%s clip=\"%s\" frame=%ld "
                 "total=%ld paused=%d volume=%d hold=%d\n",
                 stage ? stage : "", raw_current_path ? raw_current_path : "",
                 raw_osd.current_frame, raw_osd.total_frames,
                 raw_osd.paused ? 1 : 0,
                 rb->global_status != NULL ? rb->global_status->volume : 0,
#ifdef HAS_BUTTON_HOLD
                 rb->button_hold() ? 1 : 0
#else
                 0
#endif
                 );
    rb->close(fd);
}

static bool raw_input_hold_active(void)
{
#ifdef HAS_BUTTON_HOLD
    return rb->button_hold();
#else
    return false;
#endif
}

static void raw_osd_show(void)
{
    raw_osd.visible = true;
    raw_osd.needs_redraw = true;
    raw_osd.hide_tick = *rb->current_tick + RAW_OSD_SHOW_TICKS;
    append_raw_control_state("show_osd");
}

static void raw_osd_show_volume(void)
{
    raw_osd.volume_visible = true;
    raw_osd.needs_redraw = true;
    raw_osd.volume_hide_tick = *rb->current_tick + RAW_VOLUME_SHOW_TICKS;
    append_raw_control_state("volume");
}

static void raw_osd_set_paused(bool paused)
{
    raw_osd.paused = paused;
    raw_osd.visible = true;
    raw_osd.needs_redraw = true;
    if (paused)
        raw_osd.hide_tick = *rb->current_tick + HZ * 3600;
    else
        raw_osd.hide_tick = *rb->current_tick + RAW_OSD_SHOW_TICKS;
    append_raw_control_state(paused ? "pause" : "resume");
}

static void raw_osd_set_seeking(bool seeking)
{
    raw_osd.seeking = seeking;
    raw_osd.visible = seeking || raw_osd.paused || raw_osd.scrubbing;
    raw_osd.needs_redraw = true;
    if (raw_osd.visible)
        raw_osd.hide_tick = *rb->current_tick + RAW_OSD_SHOW_TICKS;
    append_raw_control_state(seeking ? "seek" : "seek_commit");
}

static void raw_osd_reset(long total_frames, int fps)
{
    rb->memset(&raw_osd, 0, sizeof(raw_osd));
    raw_osd.total_frames = total_frames;
    raw_osd.fps = fps > 0 ? fps : RAW_DEFAULT_FPS;
    append_raw_control_state("start");
}

static void raw_osd_set_position(long frame)
{
    if (frame < 0)
        frame = 0;
    raw_osd.current_frame = frame;
    if (raw_osd.paused || raw_osd.seeking || raw_osd.scrubbing)
        raw_osd.needs_redraw = true;
}

static void raw_netflix_update_skip(void)
{
    enum raw_netflix_skip_kind active = RAW_NETFLIX_SKIP_NONE;
    long frame = raw_osd.current_frame;

    if (raw_netflix_launch &&
        raw_netflix_intro_end_frame > raw_netflix_intro_start_frame &&
        frame >= raw_netflix_intro_start_frame &&
        frame < raw_netflix_intro_end_frame)
        active = RAW_NETFLIX_SKIP_INTRO;
    else if (raw_netflix_launch && raw_netflix_credits_start_frame > 0 &&
             frame >= raw_netflix_credits_start_frame)
        active = RAW_NETFLIX_SKIP_CREDITS;
    raw_netflix_skip_active = active;
}

static void raw_osd_format_time(long frames, char *buf, size_t size)
{
    long seconds;
    long hours;
    long minutes;

    if (buf == NULL || size == 0)
        return;

    if (frames < 0)
        frames = 0;
    seconds = raw_osd.fps > 0 ? frames / raw_osd.fps : 0;
    hours = seconds / 3600;
    seconds %= 3600;
    minutes = seconds / 60;
    seconds %= 60;

    if (hours > 0)
        rb->snprintf(buf, size, "%ld:%02ld:%02ld", hours, minutes, seconds);
    else
        rb->snprintf(buf, size, "%ld:%02ld", minutes, seconds);
}

static void raw_osd_draw_text_shadow(int x, int y, const char *text)
{
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy(x + 1, y + 1, text);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(x, y, text);
}

static bool raw_stock_load_bitmap(const char *name, struct bitmap *bm,
                                  fb_data *pixels, size_t bytes,
                                  int width, int height, bool transparent)
{
    char path[MAX_PATH];
    int format = FORMAT_NATIVE;
    int rc;

    rb->snprintf(path, sizeof(path), RAW_STOCK_ASSET_DIR "/%s", name);
    rb->memset(bm, 0, sizeof(*bm));
    bm->data = (char *)pixels;
    if (transparent)
        format |= FORMAT_TRANSPARENT;
    rc = rb->read_bmp_file(path, bm, (int)bytes, format, NULL);
    return rc > 0 && bm->width == width && bm->height == height;
}

static bool raw_stock_assets_loaded(void)
{
    if (raw_stock_assets.tried)
        return raw_stock_assets.loaded;

    raw_stock_assets.tried = true;
    raw_stock_assets.loaded =
        raw_stock_load_bitmap("status-header.apple.320x24x24.bmp",
                              &raw_stock_assets.header,
                              raw_stock_header_data,
                              sizeof(raw_stock_header_data),
                              LCD_WIDTH, RAW_OSD_HEADER_HEIGHT, false) &&
        raw_stock_load_bitmap("status-playback.apple.20x32x24.bmp",
                              &raw_stock_assets.playback,
                              raw_stock_playback_data,
                              sizeof(raw_stock_playback_data),
                              RAW_STOCK_PLAYBACK_W,
                              RAW_STOCK_PLAYBACK_FRAME_H * 2, true) &&
        raw_stock_load_bitmap("status-battery.apple.26x65x24.bmp",
                              &raw_stock_assets.battery,
                              raw_stock_battery_data,
                              sizeof(raw_stock_battery_data),
                              RAW_STOCK_BATTERY_W,
                              RAW_STOCK_BATTERY_FRAME_H *
                                  RAW_STOCK_BATTERY_FRAMES, true) &&
        raw_stock_load_bitmap("progress-frame.apple.200x22x32.bmp",
                              &raw_stock_assets.progress_frame,
                              raw_stock_progress_frame_data,
                              sizeof(raw_stock_progress_frame_data),
                              RAW_STOCK_PROGRESS_W,
                              RAW_STOCK_PROGRESS_H, true) &&
        raw_stock_load_bitmap("progress-fill.apple.200x22x32.bmp",
                              &raw_stock_assets.progress_fill,
                              raw_stock_progress_fill_data,
                              sizeof(raw_stock_progress_fill_data),
                              RAW_STOCK_PROGRESS_W,
                              RAW_STOCK_PROGRESS_H, true) &&
        raw_stock_load_bitmap("progress-fill-cap.apple.16x16x24.bmp",
                              &raw_stock_assets.progress_fill_cap,
                              raw_stock_progress_fill_cap_data,
                              sizeof(raw_stock_progress_fill_cap_data),
                              RAW_STOCK_PROGRESS_CAP_W,
                              RAW_STOCK_PROGRESS_CAP_H, true);
    append_raw_control_state(raw_stock_assets.loaded ?
                             "stock_assets_loaded" :
                             "stock_assets_missing");
    return raw_stock_assets.loaded;
}

static void raw_osd_title(char *title, size_t size, int max_width)
{
    const char *base = raw_current_path;
    char *dot;
    int width;
    int chars;

    if (base == NULL)
        base = "Video";
    else if (rb->strrchr(base, '/') != NULL)
        base = rb->strrchr(base, '/') + 1;

    rb->strlcpy(title, base, size);
    dot = rb->strrchr(title, '.');
    if (dot != NULL)
        *dot = '\0';
    rb->lcd_getstringsize(title, &width, NULL);
    if (width <= max_width)
        return;

    chars = rb->utf8length((const unsigned char *)title);
    while (chars > 1)
    {
        int bytes = rb->utf8seek((const unsigned char *)title, --chars);
        if (bytes < 0 || (size_t)bytes + 4 > size)
            continue;
        rb->memcpy(title + bytes, "...", 4);
        rb->lcd_getstringsize(title, &width, NULL);
        if (width <= max_width)
            return;
    }
}

static int raw_osd_battery_frame(void)
{
    int level = rb->battery_level();

    if (level <= 20)
        return 0;
    if (level < 80)
        return 1;
    return 2;
}

static void raw_osd_update_rect(int x, int y, int width, int height)
{
    if (x < 0)
    {
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        height += y;
        y = 0;
    }
    if (x + width > LCD_WIDTH)
        width = LCD_WIDTH - x;
    if (y + height > LCD_HEIGHT)
        height = LCD_HEIGHT - y;
    if (width > 0 && height > 0)
        rb->lcd_update_rect(x, y, width, height);
}

static void raw_osd_draw_progress(void)
{
    char current[24];
    char duration[24];
    int dur_w = 0;
    int fill_w;
    int progress_x = (LCD_WIDTH - RAW_STOCK_PROGRESS_W) / 2;
    int progress_y = LCD_HEIGHT - RAW_STOCK_PROGRESS_H - 20;
    int text_y = progress_y + 3;
    char title[RAW_OSD_TITLE_SIZE];
    int title_w;
    int title_h;
    long current_frame = raw_osd.current_frame;
    long total_frames = raw_osd.total_frames;
    int battery_frame;

    if (!raw_stock_assets_loaded())
        return;

    if (total_frames <= 0)
        total_frames = 1;
    if (current_frame < 0)
        current_frame = 0;
    if (current_frame > total_frames)
        current_frame = total_frames;

    raw_osd_format_time(current_frame, current, sizeof(current));
    raw_osd_format_time(total_frames, duration, sizeof(duration));
    rb->lcd_getstringsize(duration, &dur_w, NULL);

    rb->lcd_bitmap((const fb_data *)raw_stock_assets.header.data,
                   0, 0, LCD_WIDTH, RAW_OSD_HEADER_HEIGHT);

    raw_osd_title(title, sizeof(title), LCD_WIDTH - 82);
    rb->lcd_getstringsize(title, &title_w, &title_h);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy((LCD_WIDTH - title_w) / 2,
                   (RAW_OSD_HEADER_HEIGHT - title_h) / 2, title);
    rb->lcd_bitmap_transparent_part(
        (const fb_data *)raw_stock_assets.playback.data,
        0, raw_osd.paused ? RAW_STOCK_PLAYBACK_FRAME_H : 0,
        RAW_STOCK_PLAYBACK_W, 270, 4,
        RAW_STOCK_PLAYBACK_W, RAW_STOCK_PLAYBACK_FRAME_H);
    battery_frame = raw_osd_battery_frame();
    rb->lcd_bitmap_transparent_part(
        (const fb_data *)raw_stock_assets.battery.data,
        0, battery_frame * RAW_STOCK_BATTERY_FRAME_H,
        RAW_STOCK_BATTERY_W, 289, 5,
        RAW_STOCK_BATTERY_W, RAW_STOCK_BATTERY_FRAME_H);

    fill_w = (int)(((long long)current_frame *
                    (RAW_STOCK_PROGRESS_W - 6)) / total_frames);
    if (fill_w < 0)
        fill_w = 0;
    if (fill_w > RAW_STOCK_PROGRESS_W - 6)
        fill_w = RAW_STOCK_PROGRESS_W - 6;

    rb->lcd_bitmap_transparent(
        (const fb_data *)raw_stock_assets.progress_frame.data,
        progress_x, progress_y,
        RAW_STOCK_PROGRESS_W, RAW_STOCK_PROGRESS_H);
    if (fill_w > 0)
    {
        int draw_w = current_frame >= total_frames ?
                     RAW_STOCK_PROGRESS_W : fill_w + 3;

        rb->lcd_bitmap_transparent_part(
            (const fb_data *)raw_stock_assets.progress_fill.data,
            0, 0, RAW_STOCK_PROGRESS_W, progress_x, progress_y,
            draw_w, RAW_STOCK_PROGRESS_H);
        if (draw_w >= RAW_STOCK_PROGRESS_CAP_W)
            rb->lcd_bitmap_transparent(
                (const fb_data *)raw_stock_assets.progress_fill_cap.data,
                progress_x + draw_w - RAW_STOCK_PROGRESS_CAP_W,
                progress_y + 3,
                RAW_STOCK_PROGRESS_CAP_W,
                RAW_STOCK_PROGRESS_CAP_H);
    }
    raw_osd_draw_text_shadow(8, text_y, current);
    raw_osd_draw_text_shadow(LCD_WIDTH - 8 - dur_w, text_y, duration);

    raw_osd_update_rect(0, 0, LCD_WIDTH, RAW_OSD_HEADER_HEIGHT);
    raw_osd_update_rect(0, progress_y, LCD_WIDTH,
                        LCD_HEIGHT - progress_y);
}

static void raw_osd_draw_netflix(void)
{
    char current[24];
    char duration[24];
    char title[RAW_OSD_TITLE_SIZE];
    const char *status =
                         raw_netflix_skip_active == RAW_NETFLIX_SKIP_INTRO ?
                         "SKIP INTRO" :
                         raw_netflix_skip_active == RAW_NETFLIX_SKIP_CREDITS ?
                         "SKIP CREDITS" :
                         raw_osd.paused ? "PAUSED" :
                         raw_osd.seeking ? "SEEKING" : "PLAYING";
    int duration_w;
    int status_w;
    int title_w;
    int title_h;
    int bar_x = 8;
    int bar_y = LCD_HEIGHT - 12;
    int bar_w = LCD_WIDTH - 16;
    int fill_w;
    int y = LCD_HEIGHT - RAW_NETFLIX_OVERLAY_H;
    long current_frame = raw_osd.current_frame;
    long total_frames = raw_osd.total_frames;

    if (total_frames <= 0)
        total_frames = 1;
    if (current_frame < 0)
        current_frame = 0;
    if (current_frame > total_frames)
        current_frame = total_frames;

    raw_osd_format_time(current_frame, current, sizeof(current));
    raw_osd_format_time(total_frames, duration, sizeof(duration));
    raw_osd_title(title, sizeof(title), LCD_WIDTH - 98);

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(RAW_NETFLIX_BG);
    rb->lcd_set_foreground(RAW_NETFLIX_BG);
    rb->lcd_fillrect(0, y, LCD_WIDTH, RAW_NETFLIX_OVERLAY_H);
    rb->lcd_set_foreground(RAW_NETFLIX_RED);
    rb->lcd_fillrect(0, y, LCD_WIDTH, 3);

    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_foreground(RAW_NETFLIX_RED);
    rb->lcd_putsxy(7, y + 7, "NETFLIX");
    rb->lcd_getstringsize(title, &title_w, &title_h);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(LCD_WIDTH - 7 - title_w, y + 7, title);

    rb->lcd_getstringsize(status, &status_w, NULL);
    rb->lcd_getstringsize(duration, &duration_w, NULL);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(8, y + 27, current);
    rb->lcd_set_foreground(RAW_NETFLIX_RED);
    rb->lcd_putsxy((LCD_WIDTH - status_w) / 2, y + 27, status);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(LCD_WIDTH - 8 - duration_w, y + 27, duration);

    rb->lcd_set_foreground(RAW_NETFLIX_TRACK);
    rb->lcd_fillrect(bar_x, bar_y, bar_w, 5);
    fill_w = (int)(((long long)current_frame * bar_w) / total_frames);
    if (fill_w > 0)
    {
        rb->lcd_set_foreground(RAW_NETFLIX_RED);
        rb->lcd_fillrect(bar_x, bar_y, fill_w, 5);
    }

    raw_osd_update_rect(0, y, LCD_WIDTH, RAW_NETFLIX_OVERLAY_H);
}

static bool raw_volume_load_bitmap(const char *path, struct bitmap *bm,
                                   fb_data *pixels, size_t bytes,
                                   int width, int height)
{
    int rc;

    rb->memset(bm, 0, sizeof(*bm));
    bm->data = (char *)pixels;
    rc = rb->read_bmp_file(path, bm, (int)bytes, FORMAT_NATIVE, NULL);
    return rc > 0 && bm->width == width && bm->height == height;
}

static void raw_volume_wps_asset_path(char *path, size_t size,
                                      const char *asset,
                                      bool fallback)
{
    const char *wps = NULL;
    const char *base;
    const char *dot;
    char theme[MAX_FILENAME + 1];
    size_t len;

    if (fallback || rb->global_settings == NULL ||
        rb->global_settings->wps_file[0] == '\0')
    {
        rb->snprintf(path, size, ROCKBOX_DIR "/wps/iPone/%s", asset);
        return;
    }

    wps = (const char *)rb->global_settings->wps_file;
    base = rb->strrchr(wps, '/');
    base = base != NULL ? base + 1 : wps;
    dot = rb->strrchr(base, '.');
    len = dot != NULL ? (size_t)(dot - base) : rb->strlen(base);
    if (len == 0 || len >= sizeof(theme))
    {
        rb->snprintf(path, size, ROCKBOX_DIR "/wps/iPone/%s", asset);
        return;
    }

    rb->memcpy(theme, base, len);
    theme[len] = '\0';
    rb->snprintf(path, size, ROCKBOX_DIR "/wps/%s/%s", theme, asset);
}

static bool raw_volume_try_load_set(bool fallback)
{
    char path[MAX_PATH];

    raw_volume_wps_asset_path(path, sizeof(path), "VolumeBackdrop.bmp",
                              fallback);
    if (!raw_volume_load_bitmap(path, &raw_volume_assets.backdrop,
                                raw_volume_backdrop_data,
                                sizeof(raw_volume_backdrop_data),
                                RAW_VOLUME_CARD_W, RAW_VOLUME_CARD_H))
        return false;

    raw_volume_wps_asset_path(path, sizeof(path), "VolumePromptIcons.bmp",
                              fallback);
    if (!raw_volume_load_bitmap(path, &raw_volume_assets.icons,
                                raw_volume_icons_data,
                                sizeof(raw_volume_icons_data),
                                RAW_VOLUME_ICON_W,
                                RAW_VOLUME_ICON_H * RAW_VOLUME_ICON_FRAMES))
        return false;

    raw_volume_wps_asset_path(path, sizeof(path),
                              "VolumeSliderBackdropPurple.bmp", fallback);
    if (!raw_volume_load_bitmap(path, &raw_volume_assets.slider_backdrop,
                                raw_volume_slider_backdrop_data,
                                sizeof(raw_volume_slider_backdrop_data),
                                RAW_VOLUME_SLIDER_W, RAW_VOLUME_SLIDER_H))
        return false;

    raw_volume_wps_asset_path(path, sizeof(path), "VolumeSliderPurple.bmp",
                              fallback);
    if (!raw_volume_load_bitmap(path, &raw_volume_assets.slider_fill,
                                raw_volume_slider_fill_data,
                                sizeof(raw_volume_slider_fill_data),
                                RAW_VOLUME_SLIDER_W, RAW_VOLUME_SLIDER_H))
        return false;

    raw_volume_wps_asset_path(path, sizeof(path), "VolumeSliderEndPurple.bmp",
                              fallback);
    return raw_volume_load_bitmap(path, &raw_volume_assets.slider_end,
                                  raw_volume_slider_end_data,
                                  sizeof(raw_volume_slider_end_data),
                                  RAW_VOLUME_SLIDER_END_W,
                                  RAW_VOLUME_SLIDER_H);
}

static bool raw_volume_assets_loaded(void)
{
    if (!raw_volume_assets.tried)
    {
        raw_volume_assets.tried = true;
        raw_volume_assets.loaded = raw_volume_try_load_set(false) ||
                                   raw_volume_try_load_set(true);
        append_raw_control_state(raw_volume_assets.loaded ?
                                 "volume_assets_loaded" :
                                 "volume_assets_missing");
    }

    return raw_volume_assets.loaded;
}

static int raw_volume_overlay_x(void)
{
    int x = (LCD_WIDTH - RAW_VOLUME_CARD_W) / 2;
    return x < 0 ? 0 : x;
}

static int raw_volume_overlay_y(void)
{
    int y = (LCD_HEIGHT * 11) / 24;
    if (y + RAW_VOLUME_CARD_H > LCD_HEIGHT)
        y = LCD_HEIGHT - RAW_VOLUME_CARD_H;
    return y < 0 ? 0 : y;
}

static int raw_volume_percent(void)
{
    int volume = rb->global_status != NULL ? rb->global_status->volume : 0;
    int min_volume = rb->sound_min(SOUND_VOLUME);
    int max_volume = rb->sound_max(SOUND_VOLUME);

    if (volume <= min_volume)
        return 0;
    if (volume >= max_volume)
        return 100;
    return ((volume - min_volume) * 100) / (max_volume - min_volume);
}

static void raw_osd_draw_volume(void)
{
    int x = raw_volume_overlay_x();
    int y = raw_volume_overlay_y();
    int percent = raw_volume_percent();
    int fill_w = (percent * RAW_VOLUME_SLIDER_W) / 100;
    int icon_frame;
    int icon_x = x + 14;
    int icon_y = y + 12;
    int slider_x = x + 46;
    int slider_y = y + 20;
    int volume = rb->global_status != NULL ? rb->global_status->volume : 0;

    if (!raw_volume_assets_loaded())
        return;

    if (volume <= rb->sound_min(SOUND_VOLUME))
        icon_frame = 0;
    else if (volume <= -60)
        icon_frame = 1;
    else if (volume <= -30)
        icon_frame = 2;
    else
        icon_frame = 3;

    rb->lcd_bitmap((const fb_data *)raw_volume_assets.backdrop.data,
                   x, y, RAW_VOLUME_CARD_W, RAW_VOLUME_CARD_H);
    rb->lcd_bitmap_transparent_part(
        (const fb_data *)raw_volume_assets.icons.data,
        0, icon_frame * RAW_VOLUME_ICON_H, RAW_VOLUME_ICON_W,
        icon_x, icon_y, RAW_VOLUME_ICON_W, RAW_VOLUME_ICON_H);
    rb->lcd_bitmap((const fb_data *)raw_volume_assets.slider_backdrop.data,
                   slider_x, slider_y, RAW_VOLUME_SLIDER_W,
                   RAW_VOLUME_SLIDER_H);
    if (fill_w > 0)
    {
        rb->lcd_bitmap_part((const fb_data *)raw_volume_assets.slider_fill.data,
                            0, 0, RAW_VOLUME_SLIDER_W, slider_x, slider_y,
                            fill_w, RAW_VOLUME_SLIDER_H);
        rb->lcd_bitmap_transparent(
            (const fb_data *)raw_volume_assets.slider_end.data,
            slider_x + fill_w - RAW_VOLUME_SLIDER_END_W, slider_y,
            RAW_VOLUME_SLIDER_END_W, RAW_VOLUME_SLIDER_H);
    }
    raw_osd_update_rect(x, y, RAW_VOLUME_CARD_W, RAW_VOLUME_CARD_H);
}

static void raw_osd_draw_if_needed(bool force)
{
    long now = *rb->current_tick;
    unsigned old_fg;
    int old_drawmode;

    if (raw_osd.visible && !raw_osd.paused && !raw_osd.seeking &&
        !raw_osd.scrubbing && TIME_AFTER(now, raw_osd.hide_tick))
    {
        raw_osd.visible = false;
        raw_osd.needs_redraw = false;
    }
    if (raw_osd.volume_visible && TIME_AFTER(now, raw_osd.volume_hide_tick))
    {
        raw_osd.volume_visible = false;
        raw_osd.needs_redraw = false;
    }

    if (!raw_osd.visible && !raw_osd.volume_visible)
        return;

    /*
     * iPod Video/Classic targets blit YUV directly to LCD DMA. A normal LCD
     * framebuffer update on top of moving video creates a two-present flicker.
     * Keep playback on the direct path; draw the full controls only while
     * paused, where there is no active video blit underneath.
     */
    if (!raw_osd.paused && !raw_osd.seeking && !raw_osd.scrubbing)
        return;

    if (!force && !raw_osd.needs_redraw && raw_osd.paused)
        return;

    old_fg = rb->lcd_get_foreground();
    old_drawmode = rb->lcd_get_drawmode();
    rb->lcd_set_drawmode(DRMODE_FG);
    if (raw_osd.visible)
    {
        if (raw_netflix_launch)
            raw_osd_draw_netflix();
        else
            raw_osd_draw_progress();
    }
    if (raw_osd.volume_visible && !raw_netflix_launch)
        raw_osd_draw_volume();
    rb->lcd_set_drawmode(old_drawmode);
    rb->lcd_set_foreground(old_fg);
    raw_osd.needs_redraw = false;
}

static void pcm_more(const void **start, size_t *size)
{
    size_t chunk = MIN(pcm_remaining, (size_t)RAW_AUDIO_CHUNK);

    chunk &= ~(size_t)3;
    if (chunk == 0)
    {
        *start = NULL;
        *size = 0;
        return;
    }

    *start = pcm_cursor;
    *size = chunk;
    pcm_cursor += chunk;
    pcm_remaining -= chunk;
}

static void raw_audio_stop(void)
{
    rb->mixer_channel_stop(PCM_MIXER_CHAN_VOICE);
    rb->mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
    rb->pcm_play_lock();
    rb->mixer_channel_stop(RAW_PCM_CHANNEL);
    rb->pcm_play_stop();
    rb->pcm_play_unlock();
    raw_wait_for_media_idle();
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
}

static void raw_audio_shutdown(void)
{
    raw_audio_stop();
    if (raw_audio_output_active)
    {
        rb->pcmbuf_fade(false, false);
        raw_audio_output_active = false;
    }
    if (raw_saved_mixer_freq != 0)
        rb->mixer_set_frequency(raw_saved_mixer_freq);
    if (raw_pool_acquired)
    {
        rb->plugin_release_audio_buffer();
        raw_pool = NULL;
        raw_pool_size = 0;
        raw_pool_acquired = false;
    }
    append_raw_audio_state("shutdown-after-restore");
    backlight_use_settings();
    rb->button_clear_queue();
}

static bool raw_audio_start_at_frame(unsigned char *audio_buf, off_t audio_size,
                                     long frame_index, size_t bytes_per_frame)
{
    size_t audio_offset;
    size_t available;

    if (audio_size <= 0)
        return false;
    if (bytes_per_frame == 0)
        return false;

    if ((long)frame_index < 0)
        return false;

    audio_offset = (size_t)frame_index * bytes_per_frame;
    if (audio_offset / bytes_per_frame != (size_t)frame_index)
        return false;
    if (audio_offset >= (size_t)audio_size)
        return false;

    available = (size_t)audio_size - audio_offset;
    pcm_cursor = audio_buf + audio_offset;
    pcm_remaining = available;
    rb->pcm_play_lock();
    rb->mixer_channel_stop(PCM_MIXER_CHAN_VOICE);
    rb->mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
    rb->mixer_channel_stop(RAW_PCM_CHANNEL);
    rb->pcm_play_unlock();
    rb->pcm_play_stop();

    raw_wait_for_media_idle();

    rb->mixer_set_frequency(rb->hw_freq_sampr[HW_FREQ_44]);
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
#if defined(HAVE_CS42L55) && !defined(SIMULATOR)
    rb->audiohw_idle_powerup();
#endif
    rb->mixer_channel_set_amplitude(RAW_PCM_CHANNEL, MIX_AMP_UNITY);
    rb->mixer_channel_play_data(RAW_PCM_CHANNEL, pcm_more, NULL, 0);
    append_raw_audio_state("start-at-frame");
    return true;
}

static size_t raw_audio_frame_size_for_segment(off_t audio_size, long frames,
                                               size_t fallback)
{
    size_t frame_bytes;

    if (fallback > 0)
        fallback &= ~(size_t)3;

    if (audio_size <= 0 || frames <= 0)
        return fallback;

    frame_bytes = (size_t)(audio_size / frames);
    frame_bytes &= ~(size_t)3;

    return frame_bytes > 0 ? frame_bytes : fallback;
}

static bool has_ext(const char *path, const char *ext)
{
    const char *dot = rb->strrchr(path, '.');
    return dot != NULL && !rb->strcasecmp(dot + 1, ext);
}

static bool replace_ext(const char *path, const char *ext,
                        char *out, size_t out_size)
{
    const char *dot = rb->strrchr(path, '.');
    size_t base_len;

    if (dot == NULL)
        return false;

    base_len = dot - path;
    if (base_len + 1 + rb->strlen(ext) + 1 > out_size)
        return false;

    rb->memcpy(out, path, base_len);
    out[base_len] = '.';
    rb->strcpy(out + base_len + 1, ext);
    return true;
}

static void raw_resume_filename(const char *path, char *filename,
                                size_t filename_size, uint32_t *crc_out)
{
    uint32_t crc = rb->crc_32(path, rb->strlen(path), 0xffffffff);

    rb->snprintf(filename, filename_size, RAW_RESUME_FILE,
                 (unsigned long)crc);
    if (crc_out != NULL)
        *crc_out = crc;
}

static long raw_resume_load(const char *path, long total_frames, int fps)
{
    struct raw_resume_record record;
    char filename[MAX_PATH];
    uint32_t crc;
    int fd;

    (void)fps;
    raw_resume_filename(path, filename, sizeof(filename), &crc);
    fd = rb->open(filename, O_RDONLY);
    if (fd < 0)
        return 0;

    if (rb->read(fd, &record, sizeof(record)) != sizeof(record))
    {
        rb->close(fd);
        return 0;
    }
    rb->close(fd);

    if (record.magic != RAW_RESUME_MAGIC || record.path_crc != crc ||
        record.total_frames != total_frames || record.frame <= 0 ||
        record.frame >= (total_frames * 95) / 100)
        return 0;

    return record.frame;
}

static void raw_resume_save(const char *path, long frame, long total_frames,
                            int fps)
{
    struct raw_resume_record record;
    char filename[MAX_PATH];
    uint32_t crc;
    int fd;

    (void)fps;
    raw_resume_filename(path, filename, sizeof(filename), &crc);
    if (frame <= 0 || frame >= (total_frames * 95) / 100)
    {
        rb->remove(filename);
        return;
    }

    rb->mkdir(PLUGIN_APPS_DATA_DIR);
    record.magic = RAW_RESUME_MAGIC;
    record.path_crc = crc;
    record.frame = frame;
    record.total_frames = total_frames;
    fd = rb->open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;

    (void)rb->write(fd, &record, sizeof(record));
    rb->close(fd);
}

static void raw_resume_clear(const char *path)
{
    char filename[MAX_PATH];

    raw_resume_filename(path, filename, sizeof(filename), NULL);
    rb->remove(filename);
}

static bool raw_netflix_split_marker_row(
    char *line, char *fields[RAW_NETFLIX_MARKER_FIELDS])
{
    int i;

    for (i = 0; i < RAW_NETFLIX_MARKER_FIELDS; i++)
    {
        char *tab;

        fields[i] = line;
        tab = rb->strchr(line, '\t');
        if (tab == NULL)
            return i == RAW_NETFLIX_MARKER_FIELDS - 1;
        *tab = '\0';
        line = tab + 1;
    }
    return true;
}

static void raw_netflix_load_markers(const char *path, int fps,
                                     long total_frames)
{
    char line[1024];
    const char *device_path = path != NULL && path[0] == '/' ? path + 1 : path;
    int fd;

    raw_netflix_skip_active = RAW_NETFLIX_SKIP_NONE;
    raw_netflix_intro_start_frame = 0;
    raw_netflix_intro_end_frame = 0;
    raw_netflix_credits_start_frame = 0;
    if (!raw_netflix_launch || device_path == NULL || fps <= 0)
        return;

    fd = rb->open(RAW_NETFLIX_INDEX, O_RDONLY);
    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[RAW_NETFLIX_MARKER_FIELDS];

        if (line[0] == '#' || !raw_netflix_split_marker_row(line, fields) ||
            rb->strcmp(fields[6], device_path))
            continue;
        raw_netflix_intro_start_frame = (long)rb->atoi(fields[23]) * fps;
        raw_netflix_intro_end_frame = (long)rb->atoi(fields[24]) * fps;
        raw_netflix_credits_start_frame =
            (long)rb->atoi(fields[25]) * fps;
        if (raw_netflix_credits_start_frame <= 0)
        {
            long credits_duration_frame =
                (long)rb->atoi(fields[26]) * fps;
            if (credits_duration_frame > 0 &&
                total_frames > credits_duration_frame)
                raw_netflix_credits_start_frame =
                    total_frames - credits_duration_frame;
        }
        if (raw_netflix_intro_end_frame <=
            raw_netflix_intro_start_frame)
        {
            raw_netflix_intro_start_frame = 0;
            raw_netflix_intro_end_frame = 0;
        }
        if (raw_netflix_credits_start_frame >= total_frames)
            raw_netflix_credits_start_frame = 0;
        break;
    }
    rb->close(fd);
}

/* Netflix "watched" record.
 *
 * A finished title leaves no resume record behind - raw_resume_save() removes
 * it past 95% and raw_resume_clear() removes it at end of stream - so
 * completion cannot be recovered from resume data and is recorded separately.
 * This shares one file with mpegplayer so the catalog has a single source.
 * Only a small text file is opened, appended to and closed on the exit path;
 * no PCM, mixer or buffer API is involved. */
#define RAW_NETFLIX_WATCHED_FILE \
    ROCKBOX_DIR "/videolist/netflix-watched.tsv"
#define RAW_NETFLIX_WATCHED_MAX 512

static bool raw_netflix_already_watched(const char *path)
{
    char line[MAX_PATH];
    int fd = rb->open(RAW_NETFLIX_WATCHED_FILE, O_RDONLY);
    bool found = false;
    int count = 0;

    if (fd < 0)
        return false;

    while (!found && count < RAW_NETFLIX_WATCHED_MAX &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        count++;
        if (!rb->strcmp(line, path))
            found = true;
    }

    rb->close(fd);
    return found;
}

static void raw_netflix_mark_watched(const char *path)
{
    int fd;

    if (path == NULL || path[0] != '/')
        return;
    if (raw_netflix_already_watched(path))
        return;

    fd = rb->open(RAW_NETFLIX_WATCHED_FILE,
                  O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;

    rb->fdprintf(fd, "%s\n", path);
    rb->close(fd);
}

static bool raw_path_dirname(const char *path, char *out, size_t out_size)
{
    const char *slash = rb->strrchr(path, '/');
    size_t len;

    if (slash == NULL)
    {
        if (out_size < 2)
            return false;
        rb->strcpy(out, ".");
        return true;
    }

    len = slash - path;
    if (len == 0)
        len = 1;
    if (len + 1 > out_size)
        return false;

    rb->memcpy(out, path, len);
    out[len] = '\0';
    return true;
}

static bool join_segment_path(const char *marker_path, const char *name,
                              char *out, size_t out_size)
{
    char dir[MAX_PATH];
    size_t dir_len;
    size_t name_len;

    if (name == NULL || name[0] == '\0')
        return false;

    if (name[0] == '/')
    {
        if (rb->strlen(name) + 1 > out_size)
            return false;
        rb->strcpy(out, name);
        return true;
    }

    if (!raw_path_dirname(marker_path, dir, sizeof(dir)))
        return false;

    dir_len = rb->strlen(dir);
    name_len = rb->strlen(name);
    if (dir_len + 1 + name_len + 1 > out_size)
        return false;

    rb->strcpy(out, dir);
    if (dir_len > 1 || dir[0] != '/')
        rb->strcat(out, "/");
    rb->strcat(out, name);
    return true;
}

static char *trim_marker_line(char *line)
{
    char *end;

    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n')
        line++;

    end = line + rb->strlen(line);
    while (end > line &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n'))
    {
        end--;
        *end = '\0';
    }

    return line;
}

static bool marker_key_value(char *line, char **key, char **value)
{
    char *eq = rb->strchr(line, '=');

    if (eq == NULL)
        return false;

    *eq = '\0';
    *key = trim_marker_line(line);
    *value = trim_marker_line(eq + 1);
    return (*key)[0] != '\0' && (*value)[0] != '\0';
}

static void raw_video_config_defaults(struct raw_video_config *config)
{
    config->width = RAW_DEFAULT_WIDTH;
    config->height = RAW_DEFAULT_HEIGHT;
    config->fps = RAW_DEFAULT_FPS;
    config->sample_rate = RAW_DEFAULT_SAMPLE_RATE;
    config->channels = RAW_DEFAULT_CHANNELS;
    config->fit = RAW_FIT_CONTAIN;
}

static bool parse_positive_int(const char *value, int *out)
{
    long parsed = 0;
    long val;

    if (value == NULL || out == NULL || *value == '\0')
        return false;

    while (*value == ' ' || *value == '\t')
        value++;

    if (*value == '\0')
        return false;

    if (*value == '-')
        return false;

    for (const char *cursor = value; *cursor != '\0'; cursor++)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;

        val = parsed * 10 + (*cursor - '0');
        if (val > 1000000)
            return false;
        parsed = val;
    }

    *out = (int)parsed;
    return true;
}

static enum raw_fit_mode parse_fit_mode(const char *value)
{
    if (value == NULL)
        return RAW_FIT_CONTAIN;

    if (!rb->strcasecmp(value, "contain"))
        return RAW_FIT_CONTAIN;

    return RAW_FIT_CONTAIN;
}

static void normalize_raw_video_dimensions(int *width, int *height)
{
    if (width == NULL || height == NULL)
        return;

    if (*width <= 0 || *height <= 0)
    {
        *width = RAW_DEFAULT_WIDTH;
        *height = RAW_DEFAULT_HEIGHT;
        return;
    }

    if ((*width & 1) != 0)
        *width -= 1;
    if ((*height & 1) != 0)
        *height -= 1;

    if (*width < 2 || *height < 2)
    {
        *width = RAW_DEFAULT_WIDTH;
        *height = RAW_DEFAULT_HEIGHT;
    }
}

static void compute_raw_render_area(const struct raw_video_config *config,
                                    struct raw_render_area *area,
                                    bool force_contain)
{
    int width = config->width;
    int height = config->height;
    int dst_w;
    int dst_h;

    if (area == NULL || config == NULL)
        return;

    if (!force_contain &&
        width == RAW_DEFAULT_WIDTH && height == RAW_DEFAULT_HEIGHT)
    {
        area->src_width = width;
        area->src_height = height;
        area->dst_x = 0;
        area->dst_y = 0;
        area->dst_width = LCD_WIDTH;
        area->dst_height = LCD_HEIGHT;
        area->scale = (LCD_WIDTH != width || LCD_HEIGHT != height);
        return;
    }

    area->src_width = width;
    area->src_height = height;
    area->dst_x = 0;
    area->dst_y = 0;
    area->dst_width = width;
    area->dst_height = height;
    area->scale = false;

    dst_w = width;
    dst_h = height;
    if (dst_w <= 0 || dst_h <= 0)
    {
        area->dst_width = RAW_DEFAULT_WIDTH;
        area->dst_height = RAW_DEFAULT_HEIGHT;
        area->scale = false;
        return;
    }

    if (force_contain || config->fit == RAW_FIT_CONTAIN)
    {
        dst_w = (int)(((long long)LCD_WIDTH * height) / width);
        if (dst_w < 1)
            dst_w = 1;
        if (dst_w <= LCD_HEIGHT)
        {
            dst_w = LCD_WIDTH;
            dst_h = (int)(((long long)LCD_WIDTH * height + width - 1) / width);
            if (dst_h > LCD_HEIGHT)
            {
                dst_h = LCD_HEIGHT;
                dst_w = (int)(((long long)LCD_HEIGHT * width + height - 1) / height);
            }
        }
        else
        {
            dst_h = LCD_HEIGHT;
            dst_w = (int)(((long long)LCD_HEIGHT * width + height - 1) / height);
        }
    }

    if (dst_w > LCD_WIDTH)
        dst_w = LCD_WIDTH;
    if (dst_h > LCD_HEIGHT)
        dst_h = LCD_HEIGHT;
    if (dst_w < 2)
        dst_w = 2;
    if (dst_h < 2)
        dst_h = 2;

    dst_w &= ~1;
    dst_h &= ~1;
    if (dst_w < 2)
        dst_w = 2;
    if (dst_h < 2)
        dst_h = 2;

    area->scale = (dst_w != width || dst_h != height);
    area->dst_width = dst_w;
    area->dst_height = dst_h;
    area->dst_x = (LCD_WIDTH - dst_w) / 2;
    area->dst_y = (LCD_HEIGHT - dst_h) / 2;
}

static bool stretch_plane_raw420(const uint8_t *src, uint8_t *dst, int src_stride,
                                int src_w, int src_h, int dst_w, int dst_h,
                                uint8_t *tmp_buf)
{
    uint8_t *dst_end = dst + dst_w * dst_h;
    int src_w2 = src_w * 2;
    int dst_w2 = dst_w * 2;
    int src_h2 = src_h * 2;
    int dst_h2 = dst_h * 2;
    int qw = src_w2 / dst_w2;
    int rw = src_w2 - qw * dst_w2;
    int qh = src_h2 / dst_h2;
    int rh = src_h2 - qh * dst_h2;
    int dw = dst_w;
    int dh = dst_h;

    (void)tmp_buf;

    while (1)
    {
        const uint8_t *s = src;
        uint8_t *dst_line_end = dst + dst_w;

        while (1)
        {
            *dst++ = *s;
            if (dst >= dst_line_end)
            {
                dw = dst_w;
                break;
            }

            s += qw;
            dw += rw;
            if (dw >= dst_w2)
            {
                dw -= dst_w2;
                s++;
            }
        }

        if (dst >= dst_end)
            break;

        src += qh * src_stride;
        dh += rh;
        if (dh >= dst_h2)
        {
            dh -= dst_h2;
            src += src_stride;
        }
    }

    return true;
}

static bool scale_yuv420_nearest(const unsigned char *source,
                                unsigned char *scaled,
                                int src_width, int src_height,
                                int dst_width, int dst_height)
{
    const uint8_t *src = source;
    uint8_t *dst = scaled;
    size_t source_luma = (size_t)src_width * src_height;
    size_t source_chroma = (source_luma / 4);
    size_t dest_luma = (size_t)dst_width * dst_height;

    if (src == NULL || dst == NULL || src_width <= 0 || src_height <= 0 ||
        dst_width <= 0 || dst_height <= 0 || (src_width & 1) || (src_height & 1))
    {
        return false;
    }

    if (dst_width == src_width && dst_height == src_height)
    {
        rb->memcpy(scaled, source, source_luma + 2u * source_chroma);
        return true;
    }

    /* Compact RVP is exactly half the LCD dimensions. Keep its 20 fps path
     * cheap by expanding each sample directly instead of using the general
     * rational scaler. */
    if (dst_width == src_width * 2 && dst_height == src_height * 2)
    {
        const uint8_t *src_planes[3] = {
            src,
            src + source_luma,
            src + source_luma + source_chroma,
        };
        uint8_t *dst_planes[3] = {
            dst,
            dst + dest_luma,
            dst + dest_luma + (dest_luma >> 2),
        };

        for (int plane = 0; plane < 3; plane++)
        {
            int plane_width = plane == 0 ? src_width : src_width / 2;
            int plane_height = plane == 0 ? src_height : src_height / 2;

            for (int y = 0; y < plane_height; y++)
            {
                const uint8_t *src_row =
                    src_planes[plane] + y * plane_width;
                uint8_t *dst_row0 =
                    dst_planes[plane] + (y * 2) * (plane_width * 2);
                uint8_t *dst_row1 = dst_row0 + plane_width * 2;

                for (int x = 0; x < plane_width; x++)
                {
                    uint8_t value = src_row[x];
                    int dx = x * 2;

                    dst_row0[dx] = value;
                    dst_row0[dx + 1] = value;
                    dst_row1[dx] = value;
                    dst_row1[dx + 1] = value;
                }
            }
        }
        return true;
    }

    stretch_plane_raw420(src, dst, src_width, src_width, src_height, dst_width, dst_height, NULL);
    stretch_plane_raw420(src + source_luma,
                        dst + dest_luma,
                        src_width / 2, src_width / 2, src_height / 2,
                        dst_width / 2, dst_height / 2, NULL);
    stretch_plane_raw420(src + source_luma + source_chroma,
                        dst + dest_luma + (dest_luma >> 2),
                        src_width / 2, src_width / 2, src_height / 2,
                        dst_width / 2, dst_height / 2, NULL);

    return true;
}

static int parse_rvp_segments(const char *path, struct raw_segment *segments,
                              int max_segments, struct raw_video_config *config)
{
    int fd;
    ssize_t bytes;
    char *line;
    char *next;
    char single_video[MAX_PATH] = "";
    char single_audio[MAX_PATH] = "";
    int highest_segment = 0;
    raw_video_config_defaults(config);

    for (int i = 0; i < RAW_MAX_SEGMENTS; i++)
    {
        rvp_seg_video[i][0] = '\0';
        rvp_seg_audio[i][0] = '\0';
    }

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    bytes = rb->read(fd, rvp_marker_buf, RVP_MARKER_BUFSIZE);
    rb->close(fd);
    if (bytes <= 0)
        return 0;
    rvp_marker_buf[bytes] = '\0';

    line = rvp_marker_buf;
    while (line != NULL && *line != '\0')
    {
        char *key;
        char *value;

        next = rb->strchr(line, '\n');
        if (next != NULL)
        {
            *next = '\0';
            next++;
        }

        line = trim_marker_line(line);
        if (marker_key_value(line, &key, &value))
        {
            if (!rb->strcasecmp(key, "width"))
            {
                int width;
                if (parse_positive_int(value, &width))
                    config->width = width;
            }
            else if (!rb->strcasecmp(key, "height"))
            {
                int height;
                if (parse_positive_int(value, &height))
                    config->height = height;
            }
            else if (!rb->strcasecmp(key, "fps"))
            {
                int fps;
                if (parse_positive_int(value, &fps))
                    config->fps = fps;
            }
            else if (!rb->strcasecmp(key, "sample_rate"))
            {
                int sample_rate;
                if (parse_positive_int(value, &sample_rate))
                    config->sample_rate = sample_rate;
            }
            else if (!rb->strcasecmp(key, "channels"))
            {
                int channels;
                if (parse_positive_int(value, &channels))
                    config->channels = channels;
            }
            else if (!rb->strcasecmp(key, "fit"))
            {
                config->fit = parse_fit_mode(value);
            }
            if (!rb->strcasecmp(key, "video"))
                rb->strlcpy(single_video, value, sizeof(single_video));
            else if (!rb->strcasecmp(key, "audio"))
                rb->strlcpy(single_audio, value, sizeof(single_audio));
            else if (!rb->strncasecmp(key, "segment", 7))
            {
                char *suffix = key + 7;
                int index = 0;

                while (*suffix >= '0' && *suffix <= '9')
                {
                    index = index * 10 + (*suffix - '0');
                    suffix++;
                }

                if (index > 0 && index <= max_segments && *suffix == '_')
                {
                    int slot = index - 1;
                    if (!rb->strcasecmp(suffix + 1, "video"))
                        rb->strlcpy(rvp_seg_video[slot], value,
                                    sizeof(rvp_seg_video[slot]));
                    else if (!rb->strcasecmp(suffix + 1, "audio"))
                        rb->strlcpy(rvp_seg_audio[slot], value,
                                    sizeof(rvp_seg_audio[slot]));
                    if (index > highest_segment)
                        highest_segment = index;
                }
            }
        }

        line = next;
    }

    normalize_raw_video_dimensions(&config->width, &config->height);
    if (config->fps <= 0)
        config->fps = RAW_DEFAULT_FPS;
    if (config->sample_rate <= 0)
        config->sample_rate = RAW_DEFAULT_SAMPLE_RATE;
    if (config->channels <= 0)
        config->channels = RAW_DEFAULT_CHANNELS;

    if (highest_segment > 0)
    {
        int count = 0;
        for (int i = 0; i < highest_segment && count < max_segments; i++)
        {
            if (rvp_seg_video[i][0] == '\0' || rvp_seg_audio[i][0] == '\0')
                return 0;
            if (!join_segment_path(path, rvp_seg_video[i], segments[count].yuv,
                                   sizeof(segments[count].yuv)) ||
                !join_segment_path(path, rvp_seg_audio[i], segments[count].pcm,
                                   sizeof(segments[count].pcm)))
                return 0;
            count++;
        }
        return count;
    }

    if (single_video[0] != '\0' && single_audio[0] != '\0')
    {
        if (!join_segment_path(path, single_video, segments[0].yuv,
                               sizeof(segments[0].yuv)) ||
            !join_segment_path(path, single_audio, segments[0].pcm,
                               sizeof(segments[0].pcm)))
            return 0;
        return 1;
    }

    return 0;
}

static enum raw_input_command handle_playback_input(void)
{
    int action = rb->get_action(CONTEXT_WPS, TIMEOUT_NOBLOCK);

    if (raw_input_hold_active())
    {
        rb->button_clear_queue();
        if (!raw_osd.hold_logged)
        {
            append_raw_control_state("hold");
            raw_osd.hold_logged = true;
        }
        return RAW_INPUT_NONE;
    }
    raw_osd.hold_logged = false;

    if (action == ACTION_NONE)
        return RAW_INPUT_NONE;

    switch (action)
    {
        case ACTION_WPS_VOLDOWN:
            if (raw_osd.scrubbing)
                return RAW_INPUT_SEEK_BACK;
            rb->adjust_volume(-1);
            raw_osd_show_volume();
            return RAW_INPUT_VOLUME_CHANGED;

        case ACTION_WPS_VOLUP:
            if (raw_osd.scrubbing)
                return RAW_INPUT_SEEK_FORWARD;
            rb->adjust_volume(1);
            raw_osd_show_volume();
            return RAW_INPUT_VOLUME_CHANGED;

        case ACTION_WPS_PLAY:
            return RAW_INPUT_TOGGLE_PAUSE;

        case ACTION_WPS_BROWSE:
        case ACTION_STD_OK:
            if (raw_netflix_skip_active == RAW_NETFLIX_SKIP_INTRO)
                return RAW_INPUT_SKIP_INTRO;
            if (raw_netflix_skip_active == RAW_NETFLIX_SKIP_CREDITS)
                return RAW_INPUT_SKIP_CREDITS;
            return RAW_INPUT_TOGGLE_SCRUBBER;

        case ACTION_WPS_SEEKBACK:
            return RAW_INPUT_SEEK_BACK;

        case ACTION_WPS_SEEKFWD:
            return RAW_INPUT_SEEK_FORWARD;

        case ACTION_WPS_STOPSEEK:
            return RAW_INPUT_COMMIT_SEEK;

        case ACTION_WPS_SKIPPREV:
            return RAW_INPUT_RESTART_VIDEO;

        case ACTION_WPS_SKIPNEXT:
            raw_osd_show();
            return RAW_INPUT_SHOW_OSD;

        case ACTION_STD_CANCEL:
        case ACTION_WPS_MENU:
        case ACTION_WPS_STOP:
            return RAW_INPUT_EXIT;

        default:
            break;
    }

    if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
        return RAW_INPUT_EXIT;

    raw_osd_show();
    return RAW_INPUT_SHOW_OSD;
}

static bool is_start_code(const unsigned char *p, int len, int *code_len)
{
    if (len >= 4 && p[0] == 0x00 && p[1] == 0x00 &&
        p[2] == 0x00 && p[3] == 0x01)
    {
        *code_len = 4;
        return true;
    }

    if (len >= 3 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01)
    {
        *code_len = 3;
        return true;
    }

    return false;
}

static void append_profile(const char *path, long bytes, long nal_count,
                           long idr_count, long sps_count, long pps_count,
                           long scan_ticks, int error)
{
    int fd = rb->open(PROFILE_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
    {
        rb->mkdir(PROFILE_DIR);
        fd = rb->open(PROFILE_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    }

    if (fd < 0)
        return;

    rb->fdprintf(fd,
                 "mode=scan clip=\"%s\" bytes=%ld nal_count=%ld "
                 "idr_count=%ld sps_count=%ld pps_count=%ld "
                 "scan_ticks=%ld error=%d decoder=not_vendored\n",
                 path ? path : "", bytes, nal_count, idr_count, sps_count,
                 pps_count, scan_ticks, error);
    rb->close(fd);
}

static void append_raw_profile(const char *path, long frames, long video_bytes,
                               long audio_bytes, long late_frames,
                               long ticks, int error,
                               const struct raw_video_config *config)
{
    int fd = rb->open(PROFILE_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
    {
        rb->mkdir(PROFILE_DIR);
        fd = rb->open(PROFILE_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    }

    if (fd < 0)
        return;

    int width = config != NULL ? config->width : RAW_DEFAULT_WIDTH;
    int height = config != NULL ? config->height : RAW_DEFAULT_HEIGHT;
    int fps = config != NULL ? config->fps : RAW_DEFAULT_FPS;
    int sample_rate = config != NULL ? config->sample_rate : RAW_DEFAULT_SAMPLE_RATE;
    int channels = config != NULL ? config->channels : RAW_DEFAULT_CHANNELS;
    if (width <= 0 || height <= 0)
    {
        width = RAW_DEFAULT_WIDTH;
        height = RAW_DEFAULT_HEIGHT;
    }
    if (fps <= 0)
        fps = RAW_DEFAULT_FPS;
    if (sample_rate <= 0)
        sample_rate = RAW_DEFAULT_SAMPLE_RATE;
    if (channels <= 0)
        channels = RAW_DEFAULT_CHANNELS;

    rb->fdprintf(fd,
                 "mode=raw_rvp clip=\"%s\" width=%d height=%d fps=%d "
                 "sample_rate=%d channels=%d frames=%ld video_bytes=%ld "
                 "audio_bytes=%ld late_frames=%ld play_ticks=%ld error=%d\n",
                 path ? path : "", width, height, fps,
                 sample_rate, channels, frames, video_bytes,
                 audio_bytes, late_frames, ticks, error);
    rb->close(fd);
}

static int scan_annexb(const char *path, long *bytes_out, long *nal_out,
                       long *idr_out, long *sps_out, long *pps_out,
                       long *ticks_out)
{
    int fd;
    unsigned char *buf;
    size_t buf_len;
    ssize_t read_len;
    unsigned char tail[4] = { 0, 0, 0, 0 };
    int tail_len = 0;
    long bytes = 0;
    long nal_count = 0;
    long idr_count = 0;
    long sps_count = 0;
    long pps_count = 0;
    long start_tick;
    int rc = 0;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return fd;

    buf = rb->plugin_get_buffer(&buf_len);
    if (buf_len > SCAN_BUFSIZE)
        buf_len = SCAN_BUFSIZE;

    if (buf == NULL || buf_len < 1024)
    {
        rb->close(fd);
        return -1;
    }

    start_tick = *rb->current_tick;

    while ((read_len = rb->read(fd, buf + tail_len, buf_len - tail_len)) > 0)
    {
        int len = read_len + tail_len;
        int i = 0;

        bytes += read_len;

        while (i + 4 < len)
        {
            int code_len = 0;
            if (is_start_code(buf + i, len - i, &code_len))
            {
                int nal_pos = i + code_len;
                if (nal_pos < len)
                {
                    int nal_type = buf[nal_pos] & 0x1f;
                    nal_count++;
                    if (nal_type == 5)
                        idr_count++;
                    else if (nal_type == 7)
                        sps_count++;
                    else if (nal_type == 8)
                        pps_count++;
                }
                i = nal_pos + 1;
            }
            else
            {
                i++;
            }
        }

        tail_len = MIN(4, len);
        rb->memcpy(tail, buf + len - tail_len, tail_len);
        rb->memcpy(buf, tail, tail_len);
    }

    if (read_len < 0)
        rc = -2;

    rb->close(fd);

    *bytes_out = bytes;
    *nal_out = nal_count;
    *idr_out = idr_count;
    *sps_out = sps_count;
    *pps_out = pps_count;
    *ticks_out = *rb->current_tick - start_tick;

    if (rc == 0 && (nal_count == 0 || sps_count == 0 || pps_count == 0))
        rc = -3;

    return rc;
}

static int load_file(int fd, unsigned char *buf, size_t bytes)
{
    size_t done = 0;

    while (done < bytes)
    {
        ssize_t got = rb->read(fd, buf + done, bytes - done);
        if (got <= 0)
            return -1;
        done += got;
    }

    return 0;
}

static off_t raw_file_size(const char *path)
{
    int fd = rb->open(path, O_RDONLY);
    off_t size;

    if (fd < 0)
        return -1;
    size = rb->filesize(fd);
    rb->close(fd);
    return size;
}

static long raw_count_video_frames(const char *path, size_t frame_size)
{
    off_t size;

    if (frame_size == 0)
        return -1;

    size = raw_file_size(path);
    if (size <= 0 || (size % (off_t)frame_size) != 0)
        return -1;

    return (long)(size / (off_t)frame_size);
}

static int load_path(const char *path, unsigned char *buf, size_t bytes)
{
    int fd = rb->open(path, O_RDONLY);
    int rc;

    if (fd < 0)
        return -1;
    rc = load_file(fd, buf, bytes);
    rb->close(fd);
    return rc;
}

static void raw_prefetch_close(struct raw_prefetch *pf)
{
    if (pf != NULL && pf->fd >= 0)
    {
        rb->close(pf->fd);
        pf->fd = -1;
    }
}

static void raw_prefetch_begin(struct raw_prefetch *pf, const char *path,
                               unsigned char *buf, off_t size)
{
    rb->memset(pf, 0, sizeof(*pf));
    pf->fd = -1;
    pf->buf = buf;
    pf->size = size;

    if (path == NULL || buf == NULL || size <= 0)
        return;

    pf->fd = rb->open(path, O_RDONLY);
    if (pf->fd < 0)
    {
        pf->failed = true;
        return;
    }
    pf->active = true;
}

static void raw_prefetch_step(struct raw_prefetch *pf)
{
    size_t wanted;
    ssize_t got;

    if (pf == NULL || !pf->active || pf->complete || pf->failed)
        return;
    if (pf->done >= (size_t)pf->size)
    {
        pf->complete = true;
        raw_prefetch_close(pf);
        return;
    }

    wanted = MIN((size_t)RAW_PREFETCH_CHUNK, (size_t)pf->size - pf->done);
    got = rb->read(pf->fd, pf->buf + pf->done, wanted);
    if (got <= 0)
    {
        pf->failed = true;
        raw_prefetch_close(pf);
        return;
    }
    pf->done += got;
    if (pf->done >= (size_t)pf->size)
    {
        pf->complete = true;
        raw_prefetch_close(pf);
    }
}

static bool raw_prefetch_finish(struct raw_prefetch *pf)
{
    while (pf != NULL && pf->active && !pf->complete && !pf->failed)
        raw_prefetch_step(pf);

    raw_prefetch_close(pf);
    return pf != NULL && pf->complete && !pf->failed;
}

static bool raw_size_add(size_t left, size_t right, size_t *result)
{
    if (result == NULL || right > SIZE_MAX - left)
        return false;
    *result = left + right;
    return true;
}

#if defined(HAVE_LCD_COLOR) && LCD_WIDTH == 320 && LCD_HEIGHT == 240
#define RAW_VIDEO_VOLUME_H 50
#define RAW_VIDEO_VOLUME_SEGMENTS 16
#define RAW_VIDEO_VOLUME_BAR_W 112
static unsigned char raw_volume_y[LCD_WIDTH * RAW_VIDEO_VOLUME_H];
static unsigned char raw_volume_u[(LCD_WIDTH / 2) *
                                  (RAW_VIDEO_VOLUME_H / 2)];
static unsigned char raw_volume_v[(LCD_WIDTH / 2) *
                                  (RAW_VIDEO_VOLUME_H / 2)];

static unsigned char raw_yuv_clamp(int value)
{
    return value < 0 ? 0 : value > 255 ? 255 : value;
}

static void raw_yuv_color(int red, int green, int blue,
                          unsigned char *y, unsigned char *u,
                          unsigned char *v)
{
    *y = raw_yuv_clamp(
        ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16);
    *u = raw_yuv_clamp(
        ((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128);
    *v = raw_yuv_clamp(
        ((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128);
}

static void raw_yuv_rect(unsigned char * const *planes,
                         int x, int y, int width, int height,
                         int red, int green, int blue)
{
    unsigned char py;
    unsigned char pu;
    unsigned char pv;
    int row;
    int col;

    raw_yuv_color(red, green, blue, &py, &pu, &pv);
    for (row = MAX(0, y); row < MIN(RAW_VIDEO_VOLUME_H, y + height); row++)
    {
        for (col = MAX(0, x); col < MIN(LCD_WIDTH, x + width); col++)
        {
            planes[0][row * LCD_WIDTH + col] = py;
            planes[1][(row / 2) * (LCD_WIDTH / 2) + col / 2] = pu;
            planes[2][(row / 2) * (LCD_WIDTH / 2) + col / 2] = pv;
        }
    }
}

static int raw_yuv_text_width(const char *text)
{
    struct font *font = rb->font_get(FONT_SYSFIXED);
    int width = 0;

    while (font != NULL && *text != '\0')
        width += rb->font_get_width(font, (unsigned char)*text++);
    return width;
}

static void raw_yuv_text_scaled(unsigned char * const *planes,
                                int x, int y, const char *text, int scale,
                                int red, int green, int blue)
{
    struct font *font = rb->font_get(FONT_SYSFIXED);

    if (font == NULL || font->depth != 0 || scale < 1)
        return;

    while (*text != '\0')
    {
        unsigned char ch = *text++;
        int glyph_w = rb->font_get_width(font, ch);
        const unsigned char *bits = rb->font_get_bits(font, ch);
        int col;

        for (col = 0; col < glyph_w; col++)
        {
            const unsigned char *src = bits + col;
            int row;

            for (row = 0; row < (int)font->height; row++)
            {
                if (src[(row >> 3) * glyph_w] & (1u << (row & 7)))
                    raw_yuv_rect(planes, x + col * scale, y + row * scale,
                                 scale, scale, red, green, blue);
            }
        }
        x += glyph_w * scale;
    }
}

static bool raw_blit_netflix_volume(unsigned char * const *planes,
                                    int stride, int x, int y,
                                    int width, int height)
{
    unsigned char *overlay[3] = {
        raw_volume_y, raw_volume_u, raw_volume_v
    };
    struct font *font;
    int top = LCD_HEIGHT - RAW_VIDEO_VOLUME_H;
    int label_w;
    int group_w;
    int group_x;
    int bar_x;
    int bar_y = (RAW_VIDEO_VOLUME_H - 14) / 2;
    int min_volume;
    int max_volume;
    int volume;
    int percent;
    int lit;
    int i;

    if (!raw_netflix_launch || !raw_osd.volume_visible ||
        x != 0 || y != 0 || width != LCD_WIDTH || height != LCD_HEIGHT)
        return false;

    rb->memcpy(overlay[0], planes[0] + top * stride,
               sizeof(raw_volume_y));
    rb->memcpy(overlay[1], planes[1] + (top / 2) * (stride / 2),
               sizeof(raw_volume_u));
    rb->memcpy(overlay[2], planes[2] + (top / 2) * (stride / 2),
               sizeof(raw_volume_v));

    font = rb->font_get(FONT_SYSFIXED);
    label_w = raw_yuv_text_width("VOLUME") * 2;
    group_w = label_w + 12 + RAW_VIDEO_VOLUME_BAR_W;
    group_x = (LCD_WIDTH - group_w) / 2;
    bar_x = group_x + label_w + 12;
    raw_yuv_text_scaled(overlay, group_x,
                        (RAW_VIDEO_VOLUME_H -
                         (font != NULL ? font->height * 2 : 16)) / 2,
                        "VOLUME", 2, 32, 255, 80);

    min_volume = rb->sound_min(SOUND_VOLUME);
    max_volume = rb->sound_max(SOUND_VOLUME);
    if (rb->global_settings != NULL &&
        rb->global_settings->volume_limit >= min_volume &&
        rb->global_settings->volume_limit < max_volume)
        max_volume = rb->global_settings->volume_limit;
    volume = rb->global_status != NULL ?
             rb->global_status->volume : min_volume;
    percent = volume <= min_volume ? 0 :
              volume >= max_volume ? 100 :
              ((volume - min_volume) * 100) /
              (max_volume - min_volume);
    lit = percent >= 100 ? RAW_VIDEO_VOLUME_SEGMENTS :
          (percent * (RAW_VIDEO_VOLUME_SEGMENTS - 1) + 99) / 100;
    for (i = 0; i < RAW_VIDEO_VOLUME_SEGMENTS; i++)
    {
        int segment_x =
            bar_x + (i * RAW_VIDEO_VOLUME_BAR_W) /
                    RAW_VIDEO_VOLUME_SEGMENTS;

        raw_yuv_rect(overlay, segment_x, bar_y, 5, 14,
                     i < lit ? 32 : 12,
                     i < lit ? 255 : 72,
                     i < lit ? 80 : 28);
    }

    rb->lcd_blit_yuv(planes, 0, 0, stride, 0, 0, LCD_WIDTH, top);
    rb->lcd_blit_yuv(overlay, 0, 0, LCD_WIDTH, 0, top,
                     LCD_WIDTH, RAW_VIDEO_VOLUME_H);
    rb->lcd_blit_yuv(planes, 0, top + RAW_VIDEO_VOLUME_H, stride,
                     0, top + RAW_VIDEO_VOLUME_H, LCD_WIDTH,
                     LCD_HEIGHT - top - RAW_VIDEO_VOLUME_H);
    return true;
}

static bool raw_blit_netflix_skip(unsigned char * const *planes,
                                  int stride, int x, int y,
                                  int width, int height)
{
    unsigned char *overlay[3] = {
        raw_volume_y, raw_volume_u, raw_volume_v
    };
    const char *label;
    int top = LCD_HEIGHT - RAW_VIDEO_VOLUME_H;
    int text_w;
    int button_w;
    int button_h = 28;
    int button_x;
    int button_y = (RAW_VIDEO_VOLUME_H - button_h) / 2;

    raw_netflix_update_skip();
    if (!raw_netflix_launch ||
        raw_netflix_skip_active == RAW_NETFLIX_SKIP_NONE ||
        x != 0 || y != 0 || width != LCD_WIDTH || height != LCD_HEIGHT)
        return false;

    label = raw_netflix_skip_active == RAW_NETFLIX_SKIP_INTRO ?
            "SKIP INTRO" : "SKIP CREDITS";
    text_w = raw_yuv_text_width(label);
    button_w = text_w + 24;
    button_x = LCD_WIDTH - button_w - 10;
    rb->memcpy(overlay[0], planes[0] + top * stride,
               sizeof(raw_volume_y));
    rb->memcpy(overlay[1], planes[1] + (top / 2) * (stride / 2),
               sizeof(raw_volume_u));
    rb->memcpy(overlay[2], planes[2] + (top / 2) * (stride / 2),
               sizeof(raw_volume_v));

    raw_yuv_rect(overlay, button_x, button_y, button_w, button_h,
                 20, 20, 20);
    raw_yuv_rect(overlay, button_x, button_y, button_w, 2,
                 255, 255, 255);
    raw_yuv_rect(overlay, button_x, button_y + button_h - 2,
                 button_w, 2, 255, 255, 255);
    raw_yuv_rect(overlay, button_x, button_y, 2, button_h,
                 255, 255, 255);
    raw_yuv_rect(overlay, button_x + button_w - 2, button_y,
                 2, button_h, 255, 255, 255);
    raw_yuv_text_scaled(overlay,
                        button_x + (button_w - text_w) / 2,
                        button_y + 9, label, 1, 255, 255, 255);

    rb->lcd_blit_yuv(planes, 0, 0, stride, 0, 0, LCD_WIDTH, top);
    rb->lcd_blit_yuv(overlay, 0, 0, LCD_WIDTH, 0, top,
                     LCD_WIDTH, RAW_VIDEO_VOLUME_H);
    return true;
}
#else
#define raw_blit_netflix_volume(planes, stride, x, y, width, height) false
#define raw_blit_netflix_skip(planes, stride, x, y, width, height) false
#endif

static bool raw_render_frame(unsigned char *frame_buf,
                             unsigned char *scaled_frame_buf,
                             int src_width, int src_height,
                             int dst_width, int dst_height,
                             int x, int y, bool render_scale)
{
    unsigned char *planes[3];
    size_t src_luma = (size_t)src_width * src_height;
    size_t src_chroma = src_luma / 4;
    size_t dst_luma = (size_t)dst_width * dst_height;
    size_t dst_chroma = dst_luma / 4;

    if (render_scale)
    {
        if (scaled_frame_buf == NULL ||
            !scale_yuv420_nearest(frame_buf, scaled_frame_buf,
                                  src_width, src_height,
                                  dst_width, dst_height))
            return false;
        planes[0] = scaled_frame_buf;
        planes[1] = planes[0] + dst_luma;
        planes[2] = planes[1] + dst_chroma;
        if (!raw_blit_netflix_volume(planes, dst_width, x, y,
                                     dst_width, dst_height) &&
            !raw_blit_netflix_skip(planes, dst_width, x, y,
                                   dst_width, dst_height))
            rb->lcd_blit_yuv(planes, 0, 0, dst_width, x, y,
                             dst_width, dst_height);
    }
    else
    {
        planes[0] = frame_buf;
        planes[1] = frame_buf + src_luma;
        planes[2] = planes[1] + src_chroma;
        if (!raw_blit_netflix_volume(planes, src_width, x, y,
                                     dst_width, dst_height) &&
            !raw_blit_netflix_skip(planes, src_width, x, y,
                                   dst_width, dst_height))
            rb->lcd_blit_yuv(planes, 0, 0, src_width, x, y,
                             dst_width, dst_height);
    }
    return true;
}

static int play_raw_segment(const char *display_path, const char *yuv_path,
                            const char *pcm_path, unsigned char *audio_buf,
                            off_t known_audio_size, bool audio_ready,
                            unsigned char *frame_buf,
                            unsigned char *scaled_frame_buf,
                            const char *next_pcm_path,
                            unsigned char *next_audio_buf,
                            off_t next_audio_size, bool *next_ready_out,
                            bool final_segment, long *frames_out,
                            long *late_out, long *ticks_out, bool *exit_out,
                            const struct raw_video_config *config,
                            const struct raw_render_area *render_area,
                            size_t frame_size, size_t audio_frame_size,
                            long segment_frame_offset,
                            long initial_frame,
                            bool initial_paused, bool initial_seeking,
                            long *position_out, int *segment_move_out,
                            bool *paused_out,
                            bool *seeking_out)
{
    int vfd = -1;
    int afd = -1;
    off_t video_size = 0;
    off_t audio_size = 0;
    long frames;
    long late_frames = 0;
    long start_tick = *rb->current_tick;
    long fps = RAW_DEFAULT_FPS;
    int x = 0;
    int y = 0;
    int src_width = RAW_DEFAULT_WIDTH;
    int src_height = RAW_DEFAULT_HEIGHT;
    int dst_width = src_width;
    int dst_height = src_height;
    bool scale = false;
    long frame_index = initial_frame;
    bool have_frame = false;
    bool user_exit = false;
    bool normal_end = false;
    bool paused = initial_paused;
    bool seek_active = initial_seeking;
    bool force_render = true;
    long seek_started_tick = *rb->current_tick;
    struct raw_prefetch prefetch;
    bool render_scale = false;
    size_t segment_audio_frame_size = 0;
    int rc = PLUGIN_ERROR;

#if !defined(HAVE_CS42L55) || defined(SIMULATOR)
    (void)final_segment;
#endif

    if (config != NULL)
    {
        fps = config->fps;
        src_width = config->width;
        src_height = config->height;
    }
    if (render_area != NULL)
    {
        x = render_area->dst_x;
        y = render_area->dst_y;
        dst_width = render_area->dst_width;
        dst_height = render_area->dst_height;
        scale = render_area->scale;
    }
    if (dst_width <= 0 || dst_height <= 0)
    {
        dst_width = src_width;
        dst_height = src_height;
        x = (LCD_WIDTH - dst_width) / 2;
        y = (LCD_HEIGHT - dst_height) / 2;
        scale = false;
    }
    if (fps <= 0)
        fps = RAW_DEFAULT_FPS;
    if (frame_size == 0)
        frame_size = (size_t)src_width * src_height * 3 / 2;
    if (frame_size == 0)
        frame_size = (size_t)RAW_DEFAULT_WIDTH * RAW_DEFAULT_HEIGHT * 3 / 2;
    if (dst_width != src_width || dst_height != src_height)
        render_scale = true;
    if (scale)
        render_scale = true;
    if (render_scale && scaled_frame_buf == NULL)
        goto out;
    if ((size_t)frame_size !=
        (size_t)src_width * src_height * 3 / 2)
        render_scale = true;
    if (render_scale && scaled_frame_buf == NULL)
        goto out;
    rb->memset(&prefetch, 0, sizeof(prefetch));
    prefetch.fd = -1;
    if (next_ready_out != NULL)
        *next_ready_out = false;
    if (segment_move_out != NULL)
        *segment_move_out = 0;

    vfd = rb->open(yuv_path, O_RDONLY);
    if (vfd < 0)
        goto out;
    if (!audio_ready)
    {
        afd = rb->open(pcm_path, O_RDONLY);
        if (afd < 0)
            goto out;
    }

    video_size = rb->filesize(vfd);
    audio_size = audio_ready ? known_audio_size : rb->filesize(afd);
    if (video_size <= 0 || audio_size <= 0 ||
        (video_size % (off_t)frame_size) != 0)
        goto out;

    frames = video_size / (off_t)frame_size;
    if (frame_index < 0)
        frame_index = 0;
    if (frame_index >= frames)
        frame_index = frames - 1;
    segment_audio_frame_size = raw_audio_frame_size_for_segment(
        audio_size, frames, audio_frame_size);
    if (segment_audio_frame_size == 0)
        goto out;

    if (audio_buf == NULL)
        audio_buf = raw_pool;
    if (frame_buf == NULL)
        frame_buf = (unsigned char *)(((uintptr_t)(audio_buf + audio_size + 3)) & ~(uintptr_t)3);
    if (raw_pool == NULL || audio_buf == NULL || frame_buf == NULL)
        goto out;

    if (!audio_ready && load_file(afd, audio_buf, audio_size) < 0)
        goto out;
    if (rb->lseek(vfd, (off_t)frame_index * (off_t)frame_size,
                  SEEK_SET) < 0)
        goto out;
    if (load_file(vfd, frame_buf, frame_size) < 0)
        goto out;
    have_frame = true;
    if (afd >= 0)
    {
        rb->close(afd);
        afd = -1;
    }
    raw_prefetch_begin(&prefetch, next_pcm_path, next_audio_buf,
                       next_audio_size);

    raw_audio_stop();
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
    if (!paused && !seek_active &&
        !raw_audio_start_at_frame(audio_buf, audio_size, frame_index,
                                  segment_audio_frame_size))
        goto out;
    rb->lcd_clear_display();
    if (!initial_seeking)
        rb->button_clear_queue();
    raw_osd.paused = paused;
    raw_osd.scrubbing = seek_active && raw_osd.scrubbing;
    raw_osd_set_seeking(seek_active);
    if (paused)
        raw_osd_set_paused(true);

    start_tick = *rb->current_tick - (frame_index * HZ) / fps;
    while (frame_index < frames)
    {
        long target = start_tick + (frame_index * HZ) / fps;
        long elapsed;
        long audio_frame;
        enum raw_input_command command;

        command = handle_playback_input();
        if (command == RAW_INPUT_EXIT)
        {
            user_exit = true;
            goto stopped;
        }
        if (command == RAW_INPUT_VOLUME_CHANGED)
            force_render = true;

        if (command == RAW_INPUT_TOGGLE_PAUSE)
        {
            if (!paused)
            {
                raw_prefetch_close(&prefetch);
                raw_audio_stop();
                paused = true;
                seek_active = false;
                raw_osd.scrubbing = false;
                raw_osd_set_seeking(false);
                raw_osd_set_paused(true);
                force_render = true;
            }
            else
            {
                paused = false;
                raw_osd.scrubbing = false;
                raw_osd_set_paused(false);
                raw_osd_set_seeking(false);
                start_tick = *rb->current_tick - (frame_index * HZ) / fps;
                if (!raw_audio_start_at_frame(audio_buf, audio_size,
                                              frame_index,
                                              segment_audio_frame_size))
                    goto stopped;
            }
        }
        else if (command == RAW_INPUT_TOGGLE_SCRUBBER)
        {
            if (!raw_osd.scrubbing)
            {
                raw_prefetch_close(&prefetch);
                raw_audio_stop();
                raw_osd.scrubbing = true;
                seek_active = true;
                seek_started_tick = *rb->current_tick;
                raw_osd_set_seeking(true);
                force_render = true;
            }
            else
            {
                raw_osd.scrubbing = false;
                seek_active = false;
                raw_osd_set_seeking(false);
                if (!paused)
                {
                    start_tick = *rb->current_tick -
                                 (frame_index * HZ) / fps;
                    if (!raw_audio_start_at_frame(audio_buf, audio_size,
                                                  frame_index,
                                                  segment_audio_frame_size))
                        goto stopped;
                }
            }
        }
        else if (command == RAW_INPUT_SEEK_BACK ||
                 command == RAW_INPUT_SEEK_FORWARD)
        {
            long held_ticks;
            long step;
            long candidate;

            if (!seek_active)
            {
                raw_prefetch_close(&prefetch);
                raw_audio_stop();
                seek_active = true;
                seek_started_tick = *rb->current_tick;
                raw_osd_set_seeking(true);
            }
            held_ticks = *rb->current_tick - seek_started_tick;
            step = MAX(1, fps / 2);
            if (held_ticks > HZ * 3)
                step = fps * 5;
            else if (held_ticks > HZ)
                step = fps * 2;
            candidate = frame_index +
                        (command == RAW_INPUT_SEEK_BACK ? -step : step);
            if (candidate < 0)
            {
                if (segment_frame_offset > 0 && segment_move_out != NULL)
                {
                    *segment_move_out = -1;
                    goto stopped;
                }
                candidate = 0;
            }
            if (candidate >= frames)
            {
                if (!final_segment && segment_move_out != NULL)
                {
                    *segment_move_out = 1;
                    goto stopped;
                }
                candidate = frames - 1;
            }
            frame_index = candidate;
            if (rb->lseek(vfd, (off_t)frame_index * (off_t)frame_size,
                          SEEK_SET) < 0 ||
                load_file(vfd, frame_buf, frame_size) < 0)
                goto out;
            have_frame = true;
            force_render = true;
        }
        else if (command == RAW_INPUT_COMMIT_SEEK && seek_active &&
                 !raw_osd.scrubbing)
        {
            seek_active = false;
            raw_osd_set_seeking(false);
            if (!paused)
            {
                start_tick = *rb->current_tick -
                             (frame_index * HZ) / fps;
                if (!raw_audio_start_at_frame(audio_buf, audio_size,
                                              frame_index,
                                              segment_audio_frame_size))
                    goto stopped;
            }
        }
        else if (command == RAW_INPUT_RESTART_VIDEO)
        {
            raw_audio_stop();
            if (segment_frame_offset > 0 && segment_move_out != NULL)
            {
                *segment_move_out = -2;
                seek_active = false;
                raw_osd.scrubbing = false;
                goto stopped;
            }
            frame_index = 0;
            if (rb->lseek(vfd, 0, SEEK_SET) < 0 ||
                load_file(vfd, frame_buf, frame_size) < 0)
                goto out;
            have_frame = true;
            seek_active = false;
            raw_osd.scrubbing = false;
            raw_osd_set_seeking(false);
            if (!paused)
            {
                start_tick = *rb->current_tick;
                if (!raw_audio_start_at_frame(audio_buf, audio_size, 0,
                                              segment_audio_frame_size))
                    goto stopped;
            }
            force_render = true;
        }
        else if (command == RAW_INPUT_SKIP_INTRO)
        {
            long candidate = raw_netflix_intro_end_frame -
                             segment_frame_offset;

            raw_audio_stop();
            raw_netflix_skip_active = RAW_NETFLIX_SKIP_NONE;
            if (candidate >= frames)
            {
                if (segment_move_out != NULL)
                    *segment_move_out = 2;
                goto stopped;
            }
            frame_index = MAX(0, candidate);
            if (rb->lseek(vfd, (off_t)frame_index * (off_t)frame_size,
                          SEEK_SET) < 0 ||
                load_file(vfd, frame_buf, frame_size) < 0)
                goto out;
            have_frame = true;
            start_tick = *rb->current_tick - (frame_index * HZ) / fps;
            if (!paused &&
                !raw_audio_start_at_frame(audio_buf, audio_size, frame_index,
                                          segment_audio_frame_size))
                goto stopped;
            force_render = true;
        }
        else if (command == RAW_INPUT_SKIP_CREDITS)
        {
            raw_audio_stop();
            raw_netflix_skip_active = RAW_NETFLIX_SKIP_NONE;
            if (segment_move_out != NULL)
                *segment_move_out = -3;
            goto stopped;
        }

        if (paused || seek_active)
        {
            if (raw_osd.volume_visible &&
                TIME_AFTER(*rb->current_tick, raw_osd.volume_hide_tick))
            {
                raw_osd.volume_visible = false;
                force_render = true;
            }
            if (force_render && have_frame)
            {
                if (!raw_render_frame(frame_buf, scaled_frame_buf,
                                      src_width, src_height,
                                      dst_width, dst_height, x, y,
                                      render_scale))
                    goto out;
                raw_osd_set_position(segment_frame_offset + frame_index);
                raw_osd_draw_if_needed(true);
                force_render = false;
            }
            rb->sleep(1);
            raw_prefetch_step(&prefetch);
            continue;
        }

        target = start_tick + (frame_index * HZ) / fps;
        if (*rb->current_tick < target)
        {
            rb->sleep(1);
            raw_prefetch_step(&prefetch);
            continue;
        }

        elapsed = *rb->current_tick - start_tick;
        audio_frame = (elapsed * fps) / HZ;
        if (audio_frame > frame_index + 1)
        {
            long skipped = audio_frame - frame_index;
            if (audio_frame >= frames)
                goto stopped;
            if (rb->lseek(vfd, (off_t)audio_frame * (off_t)frame_size, SEEK_SET) < 0)
                goto out;
            frame_index = audio_frame;
            target = start_tick + (frame_index * HZ) / fps;
            late_frames += skipped;
            have_frame = false;
        }

        if (!have_frame && load_file(vfd, frame_buf, frame_size) < 0)
            goto out;

        if (*rb->current_tick > target + 1)
            late_frames++;

        if (!raw_render_frame(frame_buf, scaled_frame_buf,
                              src_width, src_height,
                              dst_width, dst_height, x, y,
                              render_scale))
            goto out;
        raw_osd_set_position(segment_frame_offset + frame_index);
        raw_osd_draw_if_needed(false);
        raw_prefetch_step(&prefetch);

        frame_index++;
        have_frame = false;
        if (frame_index < frames && load_file(vfd, frame_buf, frame_size) < 0)
            goto out;
        have_frame = true;
    }

stopped:
    normal_end = !user_exit && frame_index >= frames &&
                 (segment_move_out == NULL || *segment_move_out == 0);
    if (normal_end && next_ready_out != NULL)
        *next_ready_out = raw_prefetch_finish(&prefetch);
    rc = PLUGIN_OK;

out:
    if (paused_out != NULL)
        *paused_out = paused;
    if (seeking_out != NULL)
        *seeking_out = seek_active;
    raw_prefetch_close(&prefetch);
    raw_audio_stop();
#if defined(HAVE_CS42L55) && !defined(SIMULATOR)
    if (final_segment || user_exit || rc != PLUGIN_OK)
        rb->audiohw_idle_powerdown();
#endif
    if (vfd >= 0)
        rb->close(vfd);
    if (afd >= 0)
        rb->close(afd);

    if (frames_out != NULL)
        *frames_out = (vfd >= 0 && video_size > 0) ?
            video_size / (off_t)frame_size : 0;
    if (late_out != NULL)
        *late_out = late_frames;
    if (ticks_out != NULL)
        *ticks_out = *rb->current_tick - start_tick;
    if (exit_out != NULL)
        *exit_out = user_exit;
    if (position_out != NULL)
        *position_out = segment_frame_offset + frame_index;

    {
        long final_frames = (vfd >= 0 && video_size > 0) ?
                           (long)(video_size / (off_t)frame_size) : -1;
        long final_video_bytes = (vfd >= 0) ? video_size : 0;
        long final_audio_bytes = audio_size > 0 ? (long)audio_size : 0;
        long final_ticks = *rb->current_tick - start_tick;
        int final_error = (rc == PLUGIN_OK) ? 0 : -1;

        append_raw_profile(display_path, final_frames, final_video_bytes,
                          final_audio_bytes, late_frames, final_ticks,
                          final_error, config);
    }

    return rc;
}

static int play_raw_rvp(const char *path, bool netflix_launch,
                        bool netflix_restart)
{
    struct raw_video_config raw_config;
    struct raw_render_area render_area;
    int segment_count;
    char yuv_path[MAX_PATH];
    char pcm_path[MAX_PATH];
    size_t frame_size;
    size_t scaled_frame_size;
    off_t max_pcm_size = 0;
    size_t pcm_slot_size = 0;
    size_t frame_storage_size = 0;
    size_t single_slot_need = 0;
    size_t double_slot_need = 0;
    unsigned char *audio_slots[2] = { NULL, NULL };
    unsigned char *frame_buf = NULL;
    unsigned char *scaled_frame_buf = NULL;
    bool audio_ready[2] = { false, false };
    bool double_buffer_audio = false;
    long total_video_frames = 0;
    long total_frames = 0;
    long total_late = 0;
    long total_ticks = 0;
    long resume_frame = 0;
    long last_position = 0;
    bool exited_by_user = false;
    int rc = PLUGIN_ERROR;

    raw_current_path = path;
    raw_netflix_launch = netflix_launch;
    raw_video_config_defaults(&raw_config);
    raw_capture_playback_state();
    raw_pool = rb->plugin_get_audio_buffer(&raw_pool_size);
    raw_pool_acquired = raw_pool != NULL;
    if (raw_pool == NULL)
    {
        raw_audio_shutdown();
        return PLUGIN_ERROR;
    }
    raw_prepare_output();

    append_raw_profile(path, -1, 0, 0, 0, 0, -20, NULL);
    segment_count = parse_rvp_segments(path, raw_segments, RAW_MAX_SEGMENTS,
                                      &raw_config);
    append_raw_profile(path, -2, segment_count, 0, 0, 0, -21, &raw_config);

    if (segment_count <= 0)
    {
        raw_video_config_defaults(&raw_config);
        if (!replace_ext(path, "yuv", yuv_path, sizeof(yuv_path)) ||
            !replace_ext(path, "pcm", pcm_path, sizeof(pcm_path)))
        {
            raw_audio_shutdown();
            return PLUGIN_ERROR;
        }
        rb->strlcpy(raw_segments[0].yuv, yuv_path,
                    sizeof(raw_segments[0].yuv));
        rb->strlcpy(raw_segments[0].pcm, pcm_path,
                    sizeof(raw_segments[0].pcm));
        segment_count = 1;
    }
    normalize_raw_video_dimensions(&raw_config.width, &raw_config.height);
    if (raw_config.fps <= 0)
        raw_config.fps = RAW_DEFAULT_FPS;
    if (raw_config.sample_rate <= 0)
        raw_config.sample_rate = RAW_DEFAULT_SAMPLE_RATE;
    if (raw_config.channels <= 0)
        raw_config.channels = RAW_DEFAULT_CHANNELS;

    frame_size = (size_t)raw_config.width * raw_config.height * 3 / 2;
    if (frame_size == 0)
    {
        raw_audio_shutdown();
        return PLUGIN_ERROR;
    }
    compute_raw_render_area(
        &raw_config, &render_area,
#if LCD_WIDTH >= 1920 && LCD_HEIGHT >= 1080
        netflix_launch
#else
        false
#endif
    );
    scaled_frame_size = (size_t)render_area.dst_width * render_area.dst_height * 3 / 2;
    if (scaled_frame_size == 0)
    {
        raw_audio_shutdown();
        return PLUGIN_ERROR;
    }
    if ((size_t)raw_config.fps > 0)
        raw_audio_bytes_per_frame = (size_t)raw_config.sample_rate * raw_config.channels * 2 / (size_t)raw_config.fps;
    else
        raw_audio_bytes_per_frame = (size_t)RAW_DEFAULT_SAMPLE_RATE * RAW_DEFAULT_CHANNELS * 2 / (size_t)RAW_DEFAULT_FPS;

    total_video_frames = 0;
    for (int i = 0; i < segment_count; i++)
    {
        raw_segment_frame_counts[i] = raw_count_video_frames(
            raw_segments[i].yuv, frame_size);
        if (raw_segment_frame_counts[i] <= 0)
        {
            append_raw_profile(path, -3, i, 0, 0, 0, -30, &raw_config);
            raw_audio_shutdown();
            return PLUGIN_ERROR;
        }
        total_video_frames += raw_segment_frame_counts[i];
    }
    raw_osd_reset(total_video_frames, raw_config.fps);
    raw_netflix_load_markers(path, raw_config.fps, total_video_frames);
    resume_frame = raw_resume_load(path, total_video_frames, raw_config.fps);
    if (netflix_restart)
        resume_frame = 0;
    if (netflix_launch && resume_frame == 0)
        netflix_intro_run(raw_pool, raw_pool_size);

    render_area.scale = (frame_size != scaled_frame_size) ||
                       render_area.dst_width != raw_config.width ||
                       render_area.dst_height != raw_config.height;

    if (render_area.scale && scaled_frame_size == 0)
    {
        render_area.scale = false;
    }

    for (int i = 0; i < segment_count; i++)
    {
        raw_segment_pcm_sizes[i] = raw_file_size(raw_segments[i].pcm);
        if (raw_segment_pcm_sizes[i] <= 0)
        {
            raw_audio_shutdown();
            return PLUGIN_ERROR;
        }
        if (raw_segment_pcm_sizes[i] > max_pcm_size)
            max_pcm_size = raw_segment_pcm_sizes[i];
    }

    if (max_pcm_size > 0 &&
        (off_t)(size_t)max_pcm_size == max_pcm_size)
        pcm_slot_size = (size_t)max_pcm_size;
    else
    {
        raw_audio_shutdown();
        return PLUGIN_ERROR;
    }

    if (!raw_size_add(frame_size,
                      render_area.scale ? scaled_frame_size : 0,
                      &frame_storage_size) ||
        !raw_size_add(frame_storage_size, 256, &frame_storage_size) ||
        !raw_size_add(pcm_slot_size, frame_storage_size,
                      &single_slot_need) ||
        !raw_size_add(pcm_slot_size, pcm_slot_size, &double_slot_need) ||
        !raw_size_add(double_slot_need, frame_storage_size,
                      &double_slot_need))
    {
        raw_audio_shutdown();
        return PLUGIN_ERROR;
    }

    if (raw_pool_size >= double_slot_need)
    {
        audio_slots[0] = raw_pool;
        audio_slots[1] = (unsigned char *)(((uintptr_t)
            (audio_slots[0] + pcm_slot_size + 3)) & ~(uintptr_t)3);
        frame_buf = (unsigned char *)(((uintptr_t)
            (audio_slots[1] + pcm_slot_size + 3)) & ~(uintptr_t)3);
        if (render_area.scale)
            scaled_frame_buf = (unsigned char *)(((uintptr_t)
                (frame_buf + frame_size + 3)) & ~(uintptr_t)3);
        double_buffer_audio = true;
        if (load_path(raw_segments[0].pcm, audio_slots[0],
                      (size_t)raw_segment_pcm_sizes[0]) < 0)
        {
            raw_audio_shutdown();
            return PLUGIN_ERROR;
        }
        audio_ready[0] = true;
    }
    else if (raw_pool_size < single_slot_need)
    {
        raw_audio_shutdown();
        return PLUGIN_ERROR;
    }
    if (max_pcm_size > 0 && !double_buffer_audio && frame_buf == NULL)
    {
        frame_buf = (unsigned char *)(((uintptr_t)
            (raw_pool + pcm_slot_size + 3)) & ~(uintptr_t)3);
        if (render_area.scale)
            scaled_frame_buf = (unsigned char *)(((uintptr_t)
                (frame_buf + frame_size + 3)) & ~(uintptr_t)3);
    }

    long segment_frame_offset = 0;
    long initial_frame = 0;
    bool paused = false;
    bool seeking = false;
    int i = 0;

    while (i < segment_count &&
           resume_frame >=
               segment_frame_offset + raw_segment_frame_counts[i])
    {
        segment_frame_offset += raw_segment_frame_counts[i];
        i++;
    }
    if (i < segment_count)
        initial_frame = resume_frame - segment_frame_offset;

    while (i < segment_count)
    {
        long frames = 0;
        long late = 0;
        long ticks = 0;
        bool user_exit = false;
        bool next_ready = false;
        int segment_move = 0;
        int slot = i & 1;
        int next_slot = (i + 1) & 1;

        rc = play_raw_segment(
            path, raw_segments[i].yuv, raw_segments[i].pcm,
            double_buffer_audio ? audio_slots[slot] : NULL,
            raw_segment_pcm_sizes[i],
            double_buffer_audio ? audio_ready[slot] : false,
            frame_buf,
            render_area.scale ? scaled_frame_buf : NULL,
            (double_buffer_audio && i + 1 < segment_count) ?
                raw_segments[i + 1].pcm : NULL,
            (double_buffer_audio && i + 1 < segment_count) ?
                audio_slots[next_slot] : NULL,
            (i + 1 < segment_count) ? raw_segment_pcm_sizes[i + 1] : 0,
            &next_ready, i + 1 >= segment_count,
            &frames, &late, &ticks, &user_exit, &raw_config, &render_area,
            frame_size, raw_audio_bytes_per_frame,
            segment_frame_offset, initial_frame,
            paused, seeking, &last_position, &segment_move,
            &paused, &seeking);
        if (double_buffer_audio)
        {
            audio_ready[slot] = false;
            if (i + 1 < segment_count)
                audio_ready[next_slot] = next_ready;
        }
        total_frames += frames;
        total_late += late;
        total_ticks += ticks;
        if (rc != PLUGIN_OK)
            break;
        if (user_exit)
        {
            exited_by_user = true;
            break;
        }
        if (segment_move == -2)
        {
            audio_ready[0] = false;
            audio_ready[1] = false;
            i = 0;
            segment_frame_offset = 0;
            initial_frame = 0;
            seeking = false;
            raw_osd.scrubbing = false;
            continue;
        }
        if (segment_move == -3)
        {
            last_position = total_video_frames;
            i = segment_count;
            break;
        }
        if (segment_move == 2)
        {
            long target = raw_netflix_intro_end_frame;

            audio_ready[0] = false;
            audio_ready[1] = false;
            i = 0;
            segment_frame_offset = 0;
            while (i < segment_count &&
                   target >= segment_frame_offset +
                             raw_segment_frame_counts[i])
            {
                segment_frame_offset += raw_segment_frame_counts[i];
                i++;
            }
            initial_frame = i < segment_count ?
                            target - segment_frame_offset : 0;
            seeking = false;
            raw_osd.scrubbing = false;
            continue;
        }
        if (segment_move < 0)
        {
            audio_ready[0] = false;
            audio_ready[1] = false;
            if (i > 0)
            {
                i--;
                segment_frame_offset -= raw_segment_frame_counts[i];
                initial_frame = raw_segment_frame_counts[i] - 1;
            }
            else
            {
                initial_frame = 0;
            }
            continue;
        }
        segment_frame_offset += raw_segment_frame_counts[i];
        i++;
        initial_frame = 0;
        if (segment_move > 0)
            continue;
        paused = false;
        seeking = false;
    }

    if (segment_count > 1)
        append_raw_profile(path, total_frames, 0, 0, total_late,
                           total_ticks, rc == PLUGIN_OK ? 0 : -1,
                           &raw_config);

    if (exited_by_user)
    {
        raw_resume_save(path, last_position, total_video_frames,
                        raw_config.fps);
        /* raw_resume_save() drops the record past 95%, treating the title as
         * finished. Record that here or the checkmark would be lost with it. */
        if (netflix_launch && total_video_frames > 0 &&
            last_position >= (total_video_frames * 95) / 100)
            raw_netflix_mark_watched(path);
    }
    else if (rc == PLUGIN_OK && i >= segment_count)
    {
        raw_resume_clear(path);
        if (netflix_launch)
            raw_netflix_mark_watched(path);
    }

    raw_audio_shutdown();
    return rc;
}

#if defined(IPOD_6G) || defined(IPOD_VIDEO)
static bool h264_tiktok_feed_item(const char *feed_path, int wanted,
                                  char *path, size_t path_size,
                                  int *count)
{
    char line[1024];
    int fd = rb->open(feed_path, O_RDONLY);
    int found = 0;

    if (path != NULL && path_size > 0)
        path[0] = '\0';
    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *first;
        char *video;
        char *end;

        if (!rb->strncmp(line, "id\t", 3) || line[0] == '#')
            continue;
        first = rb->strchr(line, '\t');
        if (first == NULL)
            continue;
        video = rb->strchr(first + 1, '\t');
        if (video == NULL)
            continue;
        video++;
        end = rb->strchr(video, '\t');
        if (end != NULL)
            *end = '\0';
        end = rb->strchr(video, '\r');
        if (end != NULL)
            *end = '\0';
        end = rb->strchr(video, '\n');
        if (end != NULL)
            *end = '\0';
        if (video[0] != '/' || !rb->file_exists(video))
            continue;
        if (found == wanted && path != NULL)
            rb->strlcpy(path, video, path_size);
        found++;
    }
    rb->close(fd);
    if (count != NULL)
        *count = found;
    return found > 0 && (wanted < 0 ||
                         (wanted < found && path != NULL && path[0]));
}

static int h264_tiktok_load_index(int count)
{
    char line[32];
    int fd = rb->open(H264_TIKTOK_STATE, O_RDONLY);
    int index = 0;

    if (fd >= 0)
    {
        if (rb->read_line(fd, line, sizeof(line)) > 0)
            index = rb->atoi(line);
        rb->close(fd);
    }
    return index >= 0 && index < count ? index : 0;
}

static void h264_tiktok_save_index(int index)
{
    int fd = rb->open(H264_TIKTOK_STATE,
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd >= 0)
    {
        rb->fdprintf(fd, "%d\n", index);
        rb->close(fd);
    }
}

static void h264_tiktok_profile(const char *video_path)
{
    char metadata[MAX_PATH];
    char creator[64] = "TikTok creator";
    char title[96] = "Profile";
    char line[256];
    char *dot;
    int fd;

    rb->strlcpy(metadata, video_path, sizeof(metadata));
    dot = rb->strrchr(metadata, '.');
    if (dot != NULL)
        rb->strlcpy(dot, ".ttm", sizeof(metadata) - (dot - metadata));
    fd = dot != NULL ? rb->open(metadata, O_RDONLY) : -1;
    if (fd >= 0)
    {
        while (rb->read_line(fd, line, sizeof(line)) > 0)
        {
            char *end = line;

            while (*end != '\0' && *end != '\r' && *end != '\n')
                end++;
            *end = '\0';
            if (!rb->strncmp(line, "creator=", 8))
                rb->strlcpy(creator, line + 8, sizeof(creator));
            else if (!rb->strncmp(line, "title=", 6))
                rb->strlcpy(title, line + 6, sizeof(title));
        }
        rb->close(fd);
    }
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();
    rb->lcd_puts(1, 1, "TikTok Profile");
    rb->lcd_puts_scroll(1, 4, creator);
    rb->lcd_puts_scroll(1, 7, title);
    rb->lcd_puts(1, 12, "MENU returns to video");
    rb->lcd_update();
    rb->button_get(true);
}

static enum plugin_status play_h264_tiktok_feed(const char *feed_path)
{
    static char video_path[MAX_PATH];
    static char launch[MAX_PATH + 16];
    int count = 0;
    int index;
    int rc = 0;

    if (!h264_tiktok_feed_item(feed_path, -1, NULL, 0, &count))
    {
        rb->splash(HZ * 2, "TikTok feed missing");
        return PLUGIN_ERROR;
    }
    index = h264_tiktok_load_index(count);
    raw_capture_playback_state();
    raw_pool = rb->plugin_get_audio_buffer(&raw_pool_size);
    raw_pool_acquired = raw_pool != NULL;
    if (!raw_pool_acquired)
        return PLUGIN_ERROR;
    raw_prepare_output();
    while (h264_tiktok_feed_item(feed_path, index, video_path,
                                 sizeof(video_path), &count))
    {
        rb->snprintf(launch, sizeof(launch), "tiktok-app:%s", video_path);
        rc = rb->video_h264_play(launch, raw_pool, raw_pool_size);
        if (rc < 0)
            break;
        if (rc == 1)
            break;
        if (rc == 4)
            h264_tiktok_profile(video_path);
        else if (rc == 2)
            index = (index + count - 1) % count;
        else
            index = (index + 1) % count;
        h264_tiktok_save_index(index);
    }
    raw_audio_shutdown();
    return rc < 0 ? PLUGIN_ERROR : PLUGIN_OK;
}

static int play_h264_mp4(const char *launch_parameter, const char *path,
                         bool netflix_launch)
{
    int rc;
    bool youtube_launch =
        !rb->strncmp(launch_parameter, "youtube-app:", 12) ||
        !rb->strncmp(launch_parameter, "youtube-live:", 13);
    bool youtube_live =
        !rb->strncmp(launch_parameter, "youtube-live:", 13);
    bool onlyfans_launch =
        !rb->strncmp(launch_parameter, "onlyfans-app:", 13);
    bool instagram_feed =
        !rb->strncmp(launch_parameter, "instagram-feed:", 15);
    bool instagram_launch = instagram_feed ||
        !rb->strncmp(launch_parameter, "instagram-app:", 14);
    bool reddit_launch =
        !rb->strncmp(launch_parameter, "reddit-app:", 11);
    bool spotify_launch =
        !rb->strncmp(launch_parameter, "spotify-wrapped:", 16);
    bool maps_launch =
        !rb->strncmp(launch_parameter, "-mapsdash:", 10);

    raw_capture_playback_state();
    raw_pool = rb->plugin_get_audio_buffer(&raw_pool_size);
    raw_pool_acquired = raw_pool != NULL;
    if (!raw_pool_acquired)
        return PLUGIN_ERROR;
    raw_prepare_output();
    do
    {
        rc = rb->video_h264_play(launch_parameter,
                                 raw_pool, raw_pool_size);
    }
    while (youtube_live && rc == 0);
    if (netflix_launch && rc == 0)
        raw_netflix_mark_watched(path);
    raw_audio_shutdown();
    if (rc < 0)
        return PLUGIN_ERROR;

#if defined(IPOD_6G) && !defined(SIMULATOR)
    raw_video_restore_composite_output();
#endif

    if (youtube_launch)
    {
        /* Match MPEG return path behavior on iPod 5G so LCD/composite gets
         * restored before jumping back into YouTube. */
#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_YUV)
        rb->lcd_set_mode(LCD_MODE_RGB565);
#endif
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_clear_display();
        rb->lcd_update();

        static char return_parameter[MAX_PATH + 8];

        rb->snprintf(return_parameter, sizeof(return_parameter), "return:%s",
                     path);
        return rb->plugin_open(PLUGIN_APPS_DIR "/youtube.rock",
                               return_parameter);
    }
    if (onlyfans_launch)
    {
        static char return_parameter[MAX_PATH + 8];

        rb->snprintf(return_parameter, sizeof(return_parameter), "return:%s",
                     path);
        return rb->plugin_open(PLUGIN_APPS_DIR "/onlyfans.rock",
                               return_parameter);
    }
    if (instagram_launch)
    {
        static char return_parameter[MAX_PATH + 24];
        const char *prefix = "return:";

        if (instagram_feed)
            prefix = rc == 3 ? "return-home-next:" :
                     rc == 2 ? "return-home-prev:" :
                     rc == 4 ? "return:" : "return-home:";
        rb->snprintf(return_parameter, sizeof(return_parameter), "%s%s",
                     prefix, path);
        return rb->plugin_open(PLUGIN_APPS_DIR "/instagram.rock",
                               return_parameter);
    }
    if (reddit_launch)
    {
        static char return_parameter[MAX_PATH + 8];

        rb->snprintf(return_parameter, sizeof(return_parameter), "return:%s",
                     path);
        return rb->plugin_open(PLUGIN_APPS_DIR "/reddit.rock",
                               return_parameter);
    }
    if (spotify_launch)
        return rb->plugin_open(PLUGIN_APPS_DIR "/spotify_wrapped.rock", NULL);
    if (maps_launch)
        return rb->plugin_open(PLUGIN_APPS_DIR "/nb_maps.rock", NULL);
    return PLUGIN_OK;
}
#endif

static const char *video_parameter_path(const char *parameter)
{
    static const char * const prefixes[] = {
        "youtube-app:", "youtube:", "reddit-app:", "onlyfans-app:",
        "instagram-app:", "instagram-feed:", "spotify-wrapped:",
        "tiktok-app:", "-mapsdash:",
    };
    size_t i;

    if (!rb->strncmp(parameter, "youtube-live:", 13))
    {
        const char *separator = rb->strchr(parameter + 13, ':');

        return separator != NULL ? separator + 1 : parameter;
    }

    for (i = 0; i < ARRAYLEN(prefixes); i++)
    {
        size_t length = rb->strlen(prefixes[i]);

        if (!rb->strncmp(parameter, prefixes[i], length))
            return parameter + length;
    }
    return parameter;
}

enum plugin_status plugin_start(const void *parameter)
{
    const char *path = parameter;
    const char *launch_parameter = parameter;
    bool netflix_launch;
    bool netflix_restart;
    long bytes = 0;
    long nal_count = 0;
    long idr_count = 0;
    long sps_count = 0;
    long pps_count = 0;
    long scan_ticks = 0;
    int rc;

    if (path == NULL || path[0] == '\0')
    {
        rb->splash(HZ * 2, "OpenH264: no file");
        return PLUGIN_ERROR;
    }

#if defined(IPOD_6G) || defined(IPOD_VIDEO)
    if (!rb->strncmp(path, H264_TIKTOK_PREFIX, H264_TIKTOK_PREFIX_LEN))
        return play_h264_tiktok_feed(path + H264_TIKTOK_PREFIX_LEN);
#endif

    netflix_restart =
        !rb->strncmp(path, NETFLIX_RESTART_PARAMETER_PREFIX,
                     NETFLIX_RESTART_PARAMETER_PREFIX_LEN);
    netflix_launch = netflix_restart ||
        !rb->strncmp(path, NETFLIX_PARAMETER_PREFIX,
                     NETFLIX_PARAMETER_PREFIX_LEN);
    if (netflix_restart)
        path += NETFLIX_RESTART_PARAMETER_PREFIX_LEN;
    else if (netflix_launch)
        path += NETFLIX_PARAMETER_PREFIX_LEN;

    path = video_parameter_path(path);

    if (has_ext(path, "rvp"))
        return play_raw_rvp(path, netflix_launch, netflix_restart);

#if defined(IPOD_6G) || defined(IPOD_VIDEO)
    if (has_ext(path, "mp4") || has_ext(path, "m4v") ||
        has_ext(path, "mov"))
        return play_h264_mp4(launch_parameter, path, netflix_launch);
#else
    if (has_ext(path, "mp4") || has_ext(path, "m4v") ||
        has_ext(path, "mov"))
    {
        rb->splash(HZ * 2, "Hardware H.264 requires video iPod");
        return PLUGIN_ERROR;
    }
#endif

    rc = scan_annexb(path, &bytes, &nal_count, &idr_count,
                     &sps_count, &pps_count, &scan_ticks);

    append_profile(path, bytes, nal_count, idr_count, sps_count,
                   pps_count, scan_ticks, rc);

    rb->lcd_clear_display();
    rb->lcd_puts(0, 0, "OpenH264 Player");
    rb->lcd_puts(0, 1, "Decoder pending");
    rb->lcd_putsf(0, 3, "NAL: %ld", nal_count);
    rb->lcd_putsf(0, 4, "SPS/PPS: %ld/%ld", sps_count, pps_count);
    rb->lcd_putsf(0, 5, "IDR: %ld", idr_count);
    rb->lcd_putsf(0, 6, "Bytes: %ld", bytes);
    rb->lcd_puts(0, 8, "Log: .rockbox/openh264");
    rb->lcd_puts(0, 10, "Press any key");
    rb->lcd_update();

    rb->button_get(true);

    return rc == 0 ? PLUGIN_OK : PLUGIN_ERROR;
}
