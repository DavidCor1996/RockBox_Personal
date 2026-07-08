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
 * Experimental clean-video viewer entry point for iPod 6G+ video work.
 *
 * It handles two formats:
 * - .rvp: preconverted raw YUV420 video plus signed 16-bit stereo PCM sidecar.
 * - .h264: Annex-B scan/profile path used while the OpenH264 decoder is being
 *   ported behind this entry point.
 *
 ****************************************************************************/

#include "plugin.h"
#include "lib/helper.h"

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
#define RAW_OSD_BAR_HEIGHT 4
#define RAW_OSD_MARGIN 10
#define RAW_VOLUME_CARD_W 180
#define RAW_VOLUME_CARD_H 45
#define RAW_VOLUME_ICON_W 24
#define RAW_VOLUME_ICON_H 21
#define RAW_VOLUME_ICON_FRAMES 4
#define RAW_VOLUME_SLIDER_W 117
#define RAW_VOLUME_SLIDER_H 5
#define RAW_VOLUME_SLIDER_END_W 3

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

struct raw_osd_state {
    bool visible;
    bool volume_visible;
    bool paused;
    bool needs_redraw;
    bool hold_logged;
    long hide_tick;
    long volume_hide_tick;
    long current_frame;
    long total_frames;
    int fps;
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
static unsigned char raw_mixbuf[RAW_AUDIO_CHUNK] __attribute__((aligned(4)));

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
static struct raw_volume_assets raw_volume_assets;
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
    int cur_w = 0;
    int cur_h = 0;
    int dur_w = 0;
    int text_y;
    int bar_x;
    int bar_y;
    int bar_w;
    int fill_w;
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
    rb->lcd_getstringsize(current, &cur_w, &cur_h);
    rb->lcd_getstringsize(duration, &dur_w, NULL);

    text_y = LCD_HEIGHT - cur_h - 3;
    bar_x = RAW_OSD_MARGIN + cur_w + 8;
    bar_w = LCD_WIDTH - bar_x - dur_w - RAW_OSD_MARGIN - 8;
    if (bar_w < 12)
        bar_w = 12;
    bar_y = text_y + (cur_h - RAW_OSD_BAR_HEIGHT) / 2;
    fill_w = (int)((current_frame * bar_w) / total_frames);
    if (fill_w < 0)
        fill_w = 0;
    if (fill_w > bar_w)
        fill_w = bar_w;

    raw_osd_draw_text_shadow(RAW_OSD_MARGIN, text_y, current);
    raw_osd_draw_text_shadow(LCD_WIDTH - RAW_OSD_MARGIN - dur_w, text_y,
                             duration);

    rb->lcd_set_foreground(LCD_DARKGRAY);
    rb->lcd_fillrect(bar_x, bar_y, bar_w, RAW_OSD_BAR_HEIGHT);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_fillrect(bar_x, bar_y, fill_w, RAW_OSD_BAR_HEIGHT);
    raw_osd_update_rect(0, text_y - 2, LCD_WIDTH, cur_h + 5);

    if (raw_osd.paused)
    {
        int w = 0;
        int h = 0;
        rb->lcd_getstringsize("PAUSED", &w, &h);
        raw_osd_draw_text_shadow((LCD_WIDTH - w) / 2,
                                 (LCD_HEIGHT - h) / 2, "PAUSED");
        raw_osd_update_rect((LCD_WIDTH - w) / 2 - 1,
                            (LCD_HEIGHT - h) / 2 - 1, w + 3, h + 3);
    }
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
    {
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_fillrect(x, y, RAW_VOLUME_CARD_W, RAW_VOLUME_CARD_H);
        rb->lcd_set_foreground(LCD_DARKGRAY);
        rb->lcd_fillrect(slider_x, slider_y, RAW_VOLUME_SLIDER_W,
                         RAW_VOLUME_SLIDER_H);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_fillrect(slider_x, slider_y, fill_w, RAW_VOLUME_SLIDER_H);
        raw_osd_update_rect(x, y, RAW_VOLUME_CARD_W, RAW_VOLUME_CARD_H);
        return;
    }

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

    if (raw_osd.visible && !raw_osd.paused && TIME_AFTER(now, raw_osd.hide_tick))
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
    if (!raw_osd.paused)
        return;

    if (!force && !raw_osd.needs_redraw && raw_osd.paused)
        return;

    old_fg = rb->lcd_get_foreground();
    if (raw_osd.visible)
        raw_osd_draw_progress();
    if (raw_osd.volume_visible)
        raw_osd_draw_volume();
    rb->lcd_set_foreground(old_fg);
    raw_osd.needs_redraw = false;
}

static void pcm_more(const void **start, size_t *size)
{
    size_t chunk = MIN(pcm_remaining, (size_t)RAW_AUDIO_CHUNK);
    const int16_t *src;
    int16_t *dst;
    size_t samples;

    chunk &= ~(size_t)3;
    if (chunk == 0)
    {
        *start = NULL;
        *size = 0;
        return;
    }

    src = (const int16_t *)pcm_cursor;
    dst = (int16_t *)raw_mixbuf;
    samples = chunk / sizeof(int16_t);
    for (size_t i = 0; i < samples; i++)
    {
        dst[i] = src[i];
    }

    *start = raw_mixbuf;
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
                                  struct raw_render_area *area)
{
    int width = config->width;
    int height = config->height;
    int dst_w;
    int dst_h;

    if (area == NULL || config == NULL)
        return;

    if (width == RAW_DEFAULT_WIDTH && height == RAW_DEFAULT_HEIGHT)
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

    if (config->fit == RAW_FIT_CONTAIN)
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
            rb->adjust_volume(-1);
            raw_osd_show_volume();
            return RAW_INPUT_VOLUME_CHANGED;

        case ACTION_WPS_VOLUP:
            rb->adjust_volume(1);
            raw_osd_show_volume();
            return RAW_INPUT_VOLUME_CHANGED;

        case ACTION_WPS_PLAY:
            return RAW_INPUT_TOGGLE_PAUSE;

        case ACTION_STD_OK:
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

static enum raw_input_command wait_for_resume(void)
{
    enum raw_input_command command;

    raw_audio_stop();
    rb->button_clear_queue();
    raw_osd_set_paused(true);
    raw_osd_draw_if_needed(true);

    while (true)
    {
        command = handle_playback_input();
        if (command == RAW_INPUT_EXIT ||
            command == RAW_INPUT_TOGGLE_PAUSE)
        {
            raw_osd_set_paused(false);
            raw_osd_draw_if_needed(true);
            return command;
        }
        if (command == RAW_INPUT_SHOW_OSD)
        {
            raw_osd.visible = true;
            raw_osd.needs_redraw = true;
            raw_osd_draw_if_needed(true);
        }
        else if (command == RAW_INPUT_VOLUME_CHANGED)
        {
            raw_osd.needs_redraw = true;
            raw_osd_draw_if_needed(true);
        }
        rb->sleep(1);
    }
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
                            long total_video_frames)
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
    size_t src_luma = (size_t)src_width * src_height;
    size_t src_chroma = src_luma / 4;
    size_t dst_luma = (size_t)dst_width * dst_height;
    size_t dst_chroma = dst_luma / 4;
    bool scale = false;
    unsigned char *draw_buf = NULL;
    long frame_index = 0;
    bool have_frame = false;
    bool user_exit = false;
    bool normal_end = false;
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
    src_luma = (size_t)src_width * src_height;
    src_chroma = src_luma / 4;
    dst_luma = (size_t)dst_width * dst_height;
    dst_chroma = dst_luma / 4;
    if (dst_width != src_width || dst_height != src_height)
        render_scale = true;
    if (scale)
        render_scale = true;
    if (render_scale && scaled_frame_buf == NULL)
        goto out;
    if ((size_t)frame_size != src_luma * 3 / 2)
        render_scale = true;
    if (render_scale && scaled_frame_buf == NULL)
        goto out;
    draw_buf = frame_buf;

    rb->memset(&prefetch, 0, sizeof(prefetch));
    prefetch.fd = -1;
    if (next_ready_out != NULL)
        *next_ready_out = false;

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
    if (!raw_audio_start_at_frame(audio_buf, audio_size, 0,
                                  segment_audio_frame_size))
        goto out;
    rb->lcd_clear_display();
    rb->button_clear_queue();
    if (segment_frame_offset == 0)
        raw_osd_reset(total_video_frames > 0 ? total_video_frames : frames,
                      fps);

    start_tick = *rb->current_tick;
    while (frame_index < frames)
    {
        long target = start_tick + (frame_index * HZ) / fps;
        unsigned char *planes[3];
        long elapsed;
        long audio_frame;
        enum raw_input_command command;

        command = handle_playback_input();
        if (command == RAW_INPUT_EXIT)
        {
            user_exit = true;
            goto stopped;
        }
        if (command == RAW_INPUT_TOGGLE_PAUSE)
        {
            raw_prefetch_finish(&prefetch);
            command = wait_for_resume();
            if (command == RAW_INPUT_EXIT)
            {
                user_exit = true;
                goto stopped;
            }
            start_tick = *rb->current_tick - (frame_index * HZ) / fps;
            if (!raw_audio_start_at_frame(audio_buf, audio_size, frame_index,
                                          segment_audio_frame_size))
                goto stopped;
            target = start_tick + (frame_index * HZ) / fps;
        }

        while (*rb->current_tick < target)
        {
            command = handle_playback_input();
            if (command == RAW_INPUT_EXIT)
            {
                user_exit = true;
                goto stopped;
            }
            if (command == RAW_INPUT_TOGGLE_PAUSE)
            {
                command = wait_for_resume();
                if (command == RAW_INPUT_EXIT)
                {
                    user_exit = true;
                    goto stopped;
                }
                start_tick = *rb->current_tick - (frame_index * HZ) / fps;
                if (!raw_audio_start_at_frame(audio_buf, audio_size, frame_index,
                                              segment_audio_frame_size))
                    goto stopped;
                target = start_tick + (frame_index * HZ) / fps;
            }
            rb->sleep(1);
            raw_prefetch_step(&prefetch);
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

        if (render_scale)
        {
            draw_buf = scaled_frame_buf;
            if (!scale_yuv420_nearest(
                    frame_buf, draw_buf, src_width, src_height,
                    dst_width, dst_height))
            {
                goto out;
            }
            planes[0] = draw_buf;
            planes[1] = planes[0] + dst_luma;
            planes[2] = planes[1] + dst_chroma;
            rb->lcd_blit_yuv(planes, 0, 0, dst_width, x, y,
                             dst_width, dst_height);
        }
        else
        {
            planes[0] = frame_buf;
            planes[1] = frame_buf + src_luma;
            planes[2] = planes[1] + src_chroma;
            rb->lcd_blit_yuv(planes, 0, 0, src_width, x, y, dst_width,
                             dst_height);
        }
        raw_osd_set_position(segment_frame_offset + frame_index);
        raw_osd_draw_if_needed(false);
        raw_prefetch_step(&prefetch);

        command = handle_playback_input();
        if (command == RAW_INPUT_EXIT)
        {
            user_exit = true;
            goto stopped;
        }
        if (command == RAW_INPUT_TOGGLE_PAUSE)
        {
            raw_prefetch_finish(&prefetch);
            command = wait_for_resume();
            if (command == RAW_INPUT_EXIT)
            {
                user_exit = true;
                goto stopped;
            }
            start_tick = *rb->current_tick - (frame_index * HZ) / fps;
            if (!raw_audio_start_at_frame(audio_buf, audio_size, frame_index,
                                          segment_audio_frame_size))
                goto stopped;
        }

        frame_index++;
        have_frame = false;
        if (frame_index < frames && load_file(vfd, frame_buf, frame_size) < 0)
            goto out;
        have_frame = true;
    }

stopped:
    normal_end = !user_exit && frame_index >= frames;
    if (normal_end && next_ready_out != NULL)
        *next_ready_out = raw_prefetch_finish(&prefetch);
    rc = PLUGIN_OK;

out:
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

static int play_raw_rvp(const char *path)
{
    struct raw_video_config raw_config;
    struct raw_render_area render_area;
    int segment_count;
    char yuv_path[MAX_PATH];
    char pcm_path[MAX_PATH];
    size_t frame_size;
    size_t scaled_frame_size;
    off_t max_pcm_size = 0;
    unsigned char *audio_slots[2] = { NULL, NULL };
    unsigned char *frame_buf = NULL;
    unsigned char *scaled_frame_buf = NULL;
    bool audio_ready[2] = { false, false };
    bool double_buffer_audio = false;
    long total_video_frames = 0;
    long total_frames = 0;
    long total_late = 0;
    long total_ticks = 0;
    int rc = PLUGIN_ERROR;

    raw_current_path = path;
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
    compute_raw_render_area(&raw_config, &render_area);
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
        raw_pool_size >= (size_t)(max_pcm_size * 2 +
                                frame_size +
                                (render_area.scale ? scaled_frame_size : 0) +
                                256))
    {
        audio_slots[0] = raw_pool;
        audio_slots[1] = (unsigned char *)(((uintptr_t)(audio_slots[0] + max_pcm_size + 3)) & ~(uintptr_t)3);
        frame_buf = (unsigned char *)(((uintptr_t)(audio_slots[1] + max_pcm_size + 3)) & ~(uintptr_t)3);
        if (render_area.scale)
            scaled_frame_buf = (unsigned char *)(((uintptr_t)(frame_buf + frame_size + 3)) & ~(uintptr_t)3);
        double_buffer_audio = true;
        if (load_path(raw_segments[0].pcm, audio_slots[0],
                      (size_t)raw_segment_pcm_sizes[0]) < 0)
        {
            raw_audio_shutdown();
            return PLUGIN_ERROR;
        }
        audio_ready[0] = true;
    }
    else if (max_pcm_size > 0 &&
             raw_pool_size < (size_t)(max_pcm_size + frame_size +
                                    (render_area.scale ? scaled_frame_size : 0) + 64))
    {
        raw_audio_shutdown();
        return PLUGIN_ERROR;
    }
    if (max_pcm_size > 0 && !double_buffer_audio && frame_buf == NULL)
    {
        frame_buf = (unsigned char *)(((uintptr_t)(raw_pool + max_pcm_size + 3)) & ~(uintptr_t)3);
        if (render_area.scale)
            scaled_frame_buf = (unsigned char *)(((uintptr_t)(frame_buf + frame_size + 3)) & ~(uintptr_t)3);
    }

    for (int i = 0; i < segment_count; i++)
    {
        long segment_frame_offset = 0;
        long frames = 0;
        long late = 0;
        long ticks = 0;
        bool user_exit = false;
        bool next_ready = false;
        int slot = i & 1;
        int next_slot = (i + 1) & 1;

        for (int j = 0; j < i; j++)
            segment_frame_offset += raw_segment_frame_counts[j];

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
            segment_frame_offset, total_video_frames);
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
            break;
    }

    if (segment_count > 1)
        append_raw_profile(path, total_frames, 0, 0, total_late,
                           total_ticks, rc == PLUGIN_OK ? 0 : -1,
                           &raw_config);

    raw_audio_shutdown();
    return rc;
}

enum plugin_status plugin_start(const void *parameter)
{
    const char *path = parameter;
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

    if (has_ext(path, "rvp"))
        return play_raw_rvp(path);

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
