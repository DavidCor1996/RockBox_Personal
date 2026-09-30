/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 by The Rockbox Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include "config.h"
#include "system.h"
#include "font.h"
#include "notification_manager.h"
#include "lcd.h"
#include "bmp.h"
#include "file.h"
#include "settings.h"
#include "action.h"
#include "audio.h"
#include "appevents.h"
#include "backlight.h"
#include "button.h"
#include "misc.h"
#include "power.h"
#include "powermgmt.h"
#include "pcm_mixer.h"
#include "rbpaths.h"
#include "string-extra.h"
#include "timefuncs.h"
#include "rbunicode.h"
#include "usb.h"
#include "ipodjs_trace.h"
#include "ipodjs_retailos.h"
#include "ipodjs_ui.h"
#include "viewport.h"
#include "scroll_engine.h"

/* Bitmap payloads are handed to the BMP decoder as bm->data, which casts them
 * to fb_data * and writes them with halfword stores.  A plain char array only
 * guarantees one-byte alignment, and an unaligned strh data-aborts on the
 * iPod Video's core -- so force the base of every payload. */
#define IPODJS_BM_ALIGN __attribute__((aligned(4)))

#ifdef HAVE_IPODJS_UI

#define IPODJS_UI_HEADER_TOP       LCD_RGBPACK(252, 253, 253)
#define IPODJS_UI_HEADER_MID       LCD_RGBPACK(216, 219, 223)
#define IPODJS_UI_HEADER_BOTTOM    LCD_RGBPACK(174, 178, 183)
#define IPODJS_UI_SCREEN_BG        LCD_RGBPACK(255, 255, 255)
#define IPODJS_UI_TEXT             LCD_RGBPACK(0, 0, 0)
/* Sampled from Apple's RetailOS MainMenu_Template render at native LCD
 * resolution. The first highlighted pixel is part of the selection gloss;
 * these stops describe the blue body of the selected row. */
#define IPODJS_UI_ACTIVE_BOTTOM    LCD_RGBPACK(0, 92, 192)
#define IPODJS_UI_GRAPHITE         LCD_RGBPACK(84, 90, 100)
#define IPODJS_UI_U2_RED           LCD_RGBPACK(182, 24, 35)
#define IPODJS_UI_TEAL             LCD_RGBPACK(0, 128, 132)
#define IPODJS_UI_GREEN            LCD_RGBPACK(55, 142, 64)
#define IPODJS_UI_GOLD             LCD_RGBPACK(184, 135, 38)
#define IPODJS_UI_ORANGE           LCD_RGBPACK(208, 104, 32)
#define IPODJS_UI_PURPLE           LCD_RGBPACK(113, 82, 170)
#define IPODJS_UI_PINK             LCD_RGBPACK(195, 72, 128)
#define IPODJS_UI_MUTED_TEXT       LCD_RGBPACK(99, 101, 103)
#define IPODJS_UI_ASSET_DIR        ROCKBOX_DIR "/ipodjs"
#define IPODJS_UI_APPLE_ASSET_DIR  IPODJS_UI_ASSET_DIR "/apple"
#define IPODJS_UI_LABEL_CACHE_SIZE 32
#define IPODJS_UI_TEXT_SIZE        128
#define IPODJS_CHARGE_BODY_X       92
#define IPODJS_CHARGE_BODY_Y       75
#define IPODJS_CHARGE_BODY_W       145
#define IPODJS_CHARGE_BODY_H       76
#define IPODJS_CHARGE_WELL_X       96
#define IPODJS_CHARGE_WELL_Y       81
#define IPODJS_CHARGE_WELL_W       136
#define IPODJS_CHARGE_WELL_H       64
#define IPODJS_CHARGE_DAMAGE_X     84
#define IPODJS_CHARGE_DAMAGE_Y     70
#define IPODJS_CHARGE_DAMAGE_W     165
#define IPODJS_CHARGE_DAMAGE_H     130
#define IPODJS_CHARGE_FPS          20
#define IPODJS_RETAIL_CHARGE_X      63
#define IPODJS_RETAIL_CHARGE_Y      53
#define IPODJS_RETAIL_CHARGE_MIDDLE_W 123
#define IPODJS_RETAIL_CHARGE_W \
    (38 + IPODJS_RETAIL_CHARGE_MIDDLE_W + 45)
#define IPODJS_RETAIL_BATTERY_W    26
#define IPODJS_RETAIL_BATTERY_H    13
#define IPODJS_RETAIL_BATTERY_FRAMES 25
#define IPODJS_RETAIL_BATTERY_ATLAS_H \
    (IPODJS_RETAIL_BATTERY_H * IPODJS_RETAIL_BATTERY_FRAMES)
#define IPODJS_RETAIL_WPS_EQUALIZER_W 130
#define IPODJS_RETAIL_WPS_EQUALIZER_H 79
#define IPODJS_RETAIL_WPS_EQUALIZER_FRAMES 22
#define IPODJS_RETAIL_WPS_EQUALIZER_FRAME_BYTES 6004
#define IPODJS_RETAIL_WPS_EQUALIZER_FPS 10
#define IPODJS_RETAIL_WPS_PAUSED_W 72
#define IPODJS_RETAIL_WPS_PAUSED_H 71
#define IPODJS_RETAIL_IDLE_DIGIT_W 39
#define IPODJS_RETAIL_IDLE_DIGIT_H 76
#define IPODJS_RETAIL_IDLE_COLON_W 15
#define IPODJS_RETAIL_IDLE_BATTERY_W 72
#define IPODJS_RETAIL_IDLE_BATTERY_H 40
#define IPODJS_RETAIL_IDLE_BATTERY_FRAMES 8
#define IPODJS_RETAIL_IDLE_TIME_Y 54
#define IPODJS_RETAIL_IDLE_ICON_CX 106
#define IPODJS_RETAIL_IDLE_ICON_CY 170
#define IPODJS_RETAIL_IDLE_BATTERY_X 159
#define IPODJS_RETAIL_IDLE_BATTERY_Y 150
#define IPODJS_RETAIL_QUICK_SCROLL_W 74
#define IPODJS_RETAIL_QUICK_SCROLL_H 70
#define IPODJS_RETAIL_OPTIONBAR_STYLE_COUNT 3
#define IPODJS_RETAIL_OPTIONBAR_LAYER_COUNT 2
#define IPODJS_RETAIL_OPTIONBAR_PART_COUNT 3
#define IPODJS_RETAIL_OPTIONBAR_MAX_W 8
#define IPODJS_RETAIL_OPTIONBAR_MAX_H 29
#define IPODJS_RETAIL_CONTROL_ICON_COUNT 6
#define IPODJS_RETAIL_CONTROL_ICON_MAX_W 19
#define IPODJS_RETAIL_CONTROL_ICON_MAX_H 17
#define IPODJS_RETAIL_PROGRESS_LAYER_COUNT 2
#define IPODJS_RETAIL_PROGRESS_PART_COUNT 3
#define IPODJS_RETAIL_PROGRESS_MAX_W 12
#define IPODJS_RETAIL_PROGRESS_H 7
#define IPODJS_RETAIL_INPUT_PART_COUNT 3
#define IPODJS_RETAIL_INPUT_PART_W 10
#define IPODJS_RETAIL_INPUT_H 26
#define IPODJS_RETAIL_SELECTION_W 5
#define IPODJS_RETAIL_SELECTION_H 22
#define IPODJS_STOCK_BLUETOOTH_W   9
#define IPODJS_STOCK_BLUETOOTH_H   14
#define IPODJS_STOCK_WIFI_W         18
#define IPODJS_STOCK_WIFI_H         13
#define IPODJS_AIRPODS_W           124
#define IPODJS_AIRPODS_H           109
#define IPODJS_AIRPODS_ANIMATION_FPS 20

struct ipodjs_ui_label_cache_entry {
    bool valid;
    unsigned long stamp;
    int font;
    int width;
    int fit_width;
    int fit_pixels;
    char text[IPODJS_UI_TEXT_SIZE];
    char fit[IPODJS_UI_TEXT_SIZE];
};

static struct ipodjs_ui_label_cache_entry
    ipodjs_ui_label_cache[IPODJS_UI_LABEL_CACHE_SIZE];
static unsigned long ipodjs_ui_label_cache_stamp;
static bool ipodjs_ui_charging_active;
static bool ipodjs_ui_charging_seen;
static bool ipodjs_ui_fast_scroll_visible;
static long ipodjs_ui_fast_scroll_deadline;
static char ipodjs_ui_fast_scroll_label[8];
static int ipodjs_ui_transition_direction;
static long ipodjs_ui_transition_deadline;
static bool ipodjs_ui_preview_fade_active;
static int ipodjs_ui_preview_fade_x;
static int ipodjs_ui_preview_fade_y;
static int ipodjs_ui_preview_fade_w;
static int ipodjs_ui_preview_fade_h;

/*
 * Stock-style animation is decorative UI and must never ask buflib to move
 * or shrink playback memory.  The 6G has a fixed, target-scoped pair of
 * RGB565 frame workspaces instead.  This costs exactly two framebuffers in
 * BSS (307,200 bytes at 320x240) and has no runtime allocation path.
 */
#if defined(IPOD_6G) && LCD_DEPTH >= 16 && \
    LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
static fb_data ipodjs_ui_animation_frames[2]
    [FRAMEBUFFER_SIZE / sizeof(fb_data)];
#define ipodjs_ui_animation_old ipodjs_ui_animation_frames[0]
#define ipodjs_ui_animation_new ipodjs_ui_animation_frames[1]
#define IPODJS_UI_HAS_ANIMATION_WORKSPACE
#endif

#define IPODJS_UI_ANIMATION_SAMPLES 8
#define IPODJS_UI_ANIMATION_TICKS MAX(1, HZ / 4)

static void ipodjs_ui_animation_wait(long start_tick, int frame)
{
    long target = start_tick +
        (IPODJS_UI_ANIMATION_TICKS * frame) /
        (IPODJS_UI_ANIMATION_SAMPLES - 1);
    long delay = target - current_tick;

    if (delay > 0)
        sleep(delay);
}

/* Position follows elapsed time, not the number of LCD updates delivered.
 * Endpoint samples remain exact even when a frame misses its deadline.
 * This is the port's bounded easing, not a recovered Apple timing table. */
static int ipodjs_ui_animation_position(long start_tick, int frame, int extent)
{
    if (frame == 0)
        return 0;
    if (frame == IPODJS_UI_ANIMATION_SAMPLES - 1)
        return extent;
    long elapsed = current_tick - start_tick;
    int t = MIN(MAX(elapsed, 0), IPODJS_UI_ANIMATION_TICKS);
    int duration = IPODJS_UI_ANIMATION_TICKS;
    return extent * t * t * (3 * duration - 2 * t) /
        (duration * duration * duration);
}

void ipodjs_ui_transition_cancel(void)
{
    ipodjs_ui_transition_direction = 0;
    ipodjs_ui_transition_deadline = 0;
    ipodjs_ui_preview_fade_active = false;
}

#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
#define IPODJS_NETFLIX_DIR ROCKBOX_DIR "/ipodjs/netflix/launch"
#define IPODJS_NETFLIX_PACK IPODJS_NETFLIX_DIR "/intro-320x180.rgb565"
#define IPODJS_NETFLIX_WIDTH 320
#define IPODJS_NETFLIX_HEIGHT 180
#define IPODJS_NETFLIX_FRAME_BYTES (320 * 180 * 2)
#define IPODJS_NETFLIX_FRAMES 40
#define IPODJS_NETFLIX_SOUND \
    IPODJS_NETFLIX_DIR "/intro-20000-mono.mulaw"
#define IPODJS_NETFLIX_OUTPUT_WIDTH LCD_WIDTH
#define IPODJS_NETFLIX_OUTPUT_HEIGHT LCD_HEIGHT
#define IPODJS_NETFLIX_SOURCE_RATE 20000
#define IPODJS_NETFLIX_PCM_CHUNK_FRAMES 512
#define IPODJS_NETFLIX_INDEX_BYTES \
    (IPODJS_NETFLIX_OUTPUT_WIDTH * IPODJS_NETFLIX_OUTPUT_HEIGHT / 2)

static bool ipodjs_netflix_read_file(const char *path, unsigned char *buffer,
                                     size_t capacity, size_t *size)
{
    int fd = open(path, O_RDONLY);
    off_t length;
    size_t done = 0;

    if (fd < 0)
        return false;
    length = filesize(fd);
    if (length <= 0 || (off_t)(size_t)length != length ||
        (size_t)length > capacity)
    {
        close(fd);
        return false;
    }
    while (done < (size_t)length)
    {
        ssize_t count = read(fd, buffer + done, (size_t)length - done);

        if (count <= 0)
        {
            close(fd);
            return false;
        }
        done += (size_t)count;
    }
    close(fd);
    *size = done;
    return true;
}

/* Service one independent native-resolution frame in the existing 153600-byte
 * transition workspace. No playback allocation and no I/O in the renderer.
 * Independent frames allow late frames to be skipped without delta replay. */
static bool ipodjs_netflix_load_frame(int fd, int frame, fb_data *pixels)
{
    unsigned char *bytes = (unsigned char *)pixels;
    size_t done = 0;
    off_t offset = (off_t)frame * IPODJS_NETFLIX_FRAME_BYTES;

    if (lseek(fd, offset, SEEK_SET) != offset)
        return false;
    while (done < IPODJS_NETFLIX_FRAME_BYTES)
    {
        ssize_t count = read(fd, bytes + done,
                            IPODJS_NETFLIX_FRAME_BYTES - done);
        if (count <= 0)
            return false;
        done += count;
        yield();
    }
    for (size_t i = 0; i < IPODJS_NETFLIX_WIDTH * IPODJS_NETFLIX_HEIGHT; ++i)
        pixels[i] = (fb_data)(bytes[2 * i] | (bytes[2 * i + 1] << 8));
    return true;
}

static void ipodjs_netflix_render(const fb_data *pixels)
{
    /* Native 320x180, centered on the 320x240 LCD: preserve the entire
     * 16:9 image without enlarging pixels, cropping or stretching. */
    lcd_bitmap(pixels, (LCD_WIDTH - IPODJS_NETFLIX_WIDTH) / 2,
               (LCD_HEIGHT - IPODJS_NETFLIX_HEIGHT) / 2,
               IPODJS_NETFLIX_WIDTH, IPODJS_NETFLIX_HEIGHT);
    screens[SCREEN_MAIN].update();
}

#ifndef HAVE_HARDWARE_BEEP
static const unsigned char *ipodjs_netflix_audio;
static size_t ipodjs_netflix_audio_size;
static size_t ipodjs_netflix_audio_position;
static unsigned int ipodjs_netflix_audio_phase;
static unsigned int ipodjs_netflix_output_rate;
static int16_t *ipodjs_netflix_pcm;
static size_t ipodjs_netflix_pcm_frames;

static int16_t ipodjs_netflix_mulaw_decode(unsigned char value)
{
    int sample;
    int exponent;

    value = ~value;
    exponent = (value >> 4) & 7;
    sample = (((value & 15) << 3) + 0x84) << exponent;
    sample -= 0x84;
    return (value & 0x80) ? -sample : sample;
}

static void ipodjs_netflix_pcm_more(const void **start, size_t *size)
{
    size_t frames = 0;

    if (!ipodjs_netflix_pcm || ipodjs_netflix_pcm_frames == 0 ||
        !ipodjs_netflix_audio || ipodjs_netflix_output_rate == 0)
    {
        *start = NULL;
        *size = 0;
        return;
    }
    while (frames < ipodjs_netflix_pcm_frames &&
           ipodjs_netflix_audio_position < ipodjs_netflix_audio_size)
    {
        int16_t sample = ipodjs_netflix_mulaw_decode(
            ipodjs_netflix_audio[ipodjs_netflix_audio_position]);
        ipodjs_netflix_pcm[frames * 2] = sample;
        ipodjs_netflix_pcm[frames * 2 + 1] = sample;
        ++frames;
        ipodjs_netflix_audio_phase += IPODJS_NETFLIX_SOURCE_RATE;
        while (ipodjs_netflix_audio_phase >= ipodjs_netflix_output_rate)
        {
            ipodjs_netflix_audio_phase -= ipodjs_netflix_output_rate;
            ++ipodjs_netflix_audio_position;
        }
    }
    *start = ipodjs_netflix_pcm;
    *size = frames * 2 * sizeof(int16_t);
}

static void ipodjs_netflix_beep_detach(void)
{
    mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
    mixer_channel_set_buffer_hook(PCM_MIXER_CHAN_BEEP, NULL);
    {
        long deadline = current_tick + HZ / 4;

        while (mixer_channel_status(PCM_MIXER_CHAN_BEEP) !=
               CHANNEL_STOPPED && TIME_BEFORE(current_tick, deadline))
            sleep(1);
    }
    ipodjs_netflix_audio = NULL;
    ipodjs_netflix_audio_size = 0;
    ipodjs_netflix_audio_position = 0;
    ipodjs_netflix_audio_phase = 0;
    ipodjs_netflix_output_rate = 0;
    ipodjs_netflix_pcm = NULL;
    ipodjs_netflix_pcm_frames = 0;
}
#endif
#endif

bool ipodjs_ui_netflix_launch(void)
{
#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    struct screen *display = &screens[SCREEN_MAIN];
    fb_data *pixels = ipodjs_ui_animation_old;
    int frame_fd;
    unsigned char *sound = (unsigned char *)ipodjs_ui_animation_new;
    size_t sound_size = 0;
    size_t index_offset;
    const int frame_count = IPODJS_NETFLIX_FRAMES;
    const int frame_ms = 100;
    bool usb = false;
    long started;

#ifndef HAVE_HARDWARE_BEEP
    /* The workspace is reused for every launch. Detach the previous beep
     * callback before either half of it is overwritten by new assets. */
    ipodjs_netflix_beep_detach();
#endif
    ipodjs_ui_transition_cancel();
    frame_fd = open(IPODJS_NETFLIX_PACK, O_RDONLY);
    if (frame_fd < 0)
        return false;
    if (filesize(frame_fd) !=
        (off_t)IPODJS_NETFLIX_FRAMES * IPODJS_NETFLIX_FRAME_BYTES ||
        !ipodjs_netflix_load_frame(frame_fd, 0, pixels))
    {
        close(frame_fd);
        return false;
    }
#ifndef HAVE_HARDWARE_BEEP
    if (ipodjs_netflix_read_file(
            IPODJS_NETFLIX_SOUND, sound,
            sizeof(ipodjs_ui_animation_new) -
                IPODJS_NETFLIX_INDEX_BYTES - 4096,
                                  &sound_size))
    {
        ipodjs_netflix_audio = sound;
        ipodjs_netflix_audio_size = sound_size;
        ipodjs_netflix_audio_position = 0;
        ipodjs_netflix_audio_phase = 0;
        ipodjs_netflix_output_rate = mixer_get_frequency();
    }
#endif
    index_offset = (sound_size + 3) & ~(size_t)3;
#ifndef HAVE_HARDWARE_BEEP
    ipodjs_netflix_pcm =
        (int16_t *)(sound + index_offset + IPODJS_NETFLIX_INDEX_BYTES);
    ipodjs_netflix_pcm_frames =
        MIN((sizeof(ipodjs_ui_animation_new) -
             (index_offset + IPODJS_NETFLIX_INDEX_BYTES)) /
                (2 * sizeof(int16_t)),
            (size_t)IPODJS_NETFLIX_PCM_CHUNK_FRAMES);
#endif

    button_clear_queue();
    display->set_viewport(NULL);
    display->set_background(LCD_BLACK);
    display->clear_display();
    ipodjs_netflix_render(pixels);

#ifndef HAVE_HARDWARE_BEEP
    if (sound_size > 0 && ipodjs_netflix_output_rate > 0 &&
        ipodjs_netflix_pcm_frames > 0)
    {
        mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
        mixer_channel_set_amplitude(PCM_MIXER_CHAN_BEEP, MIX_AMP_UNITY);
        mixer_channel_play_data(PCM_MIXER_CHAN_BEEP,
                                ipodjs_netflix_pcm_more, NULL, 0);
    }
#endif
    started = current_tick;
    for (int frame = 1; frame < frame_count; ++frame)
    {
        long target = started + frame * frame_ms * HZ / 1000;
        long following_target =
            started + (frame + 1) * frame_ms * HZ / 1000;

        while (TIME_BEFORE(current_tick, target))
        {
            int button = button_get_w_tmo(0);

            if (button != BUTTON_NONE && !(button & BUTTON_REL))
            {
                usb = default_event_handler(button) == SYS_USB_CONNECTED;
                goto stop;
            }
            sleep(1);
        }
        {
            int button = button_get_w_tmo(0);

            if (button != BUTTON_NONE && !(button & BUTTON_REL))
            {
                usb = default_event_handler(button) == SYS_USB_CONNECTED;
                goto stop;
            }
        }
        if (frame + 1 == frame_count ||
            TIME_BEFORE(current_tick, following_target))
        {
            if (!ipodjs_netflix_load_frame(frame_fd, frame, pixels))
                goto stop;
            ipodjs_netflix_render(pixels);
        }
        else
            yield();
    }
#ifndef HAVE_HARDWARE_BEEP
    {
        long deadline = started + HZ * 4;

        while (TIME_BEFORE(current_tick, deadline) &&
               mixer_channel_status(PCM_MIXER_CHAN_BEEP) != CHANNEL_STOPPED)
        {
            int button = button_get_w_tmo(0);

            if (button != BUTTON_NONE && !(button & BUTTON_REL))
            {
                usb = default_event_handler(button) == SYS_USB_CONNECTED;
                break;
            }
            sleep(1);
        }
    }
#endif

stop:
    close(frame_fd);
#ifndef HAVE_HARDWARE_BEEP
    ipodjs_netflix_beep_detach();
#endif
    button_clear_queue();
    return usb;
#else
    return false;
#endif
}

static void ipodjs_ui_transition_begin_mode(int direction, bool vertical)
{
#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    ipodjs_ui_transition_cancel();
    if (!ipodjs_ui_enabled(SCREEN_MAIN) || direction == 0 || button_hold())
        return;

    ipodjs_ui_stop_menu_text_scroll();

    screens[SCREEN_MAIN].set_viewport(NULL);
    memcpy(ipodjs_ui_animation_old, FBADDR(0, 0), FRAMEBUFFER_SIZE);
    ipodjs_ui_transition_direction = direction < 0 ? -1 : 1;
    if (vertical)
        ipodjs_ui_transition_direction *= 2;
    ipodjs_ui_transition_deadline = current_tick + HZ;
#else
    (void)direction;
    (void)vertical;
#endif
}

void ipodjs_ui_transition_begin(int direction)
{
    ipodjs_ui_transition_begin_mode(direction, false);
}

void ipodjs_ui_transition_begin_vertical(int direction)
{
    ipodjs_ui_transition_begin_mode(direction, true);
}

bool ipodjs_ui_transition_present(struct screen *display)
{
#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    int direction;
    int frame;
    long start_tick;
    bool vertical;

    if (display->screen_type != SCREEN_MAIN ||
        ipodjs_ui_transition_direction == 0)
        return false;
    if (TIME_AFTER(current_tick, ipodjs_ui_transition_deadline))
    {
        ipodjs_ui_transition_cancel();
        return false;
    }
    direction = ipodjs_ui_transition_direction;
    vertical = direction < -1 || direction > 1;
    /* Leave queued actions intact for the owner. No queue-count heuristic,
     * navigation-history rewrite or discard of Select/Menu/system events. */
    if (!vertical && (!button_queue_empty() || button_hold()))
    {
        ipodjs_ui_transition_cancel();
        return false;
    }
    display->set_viewport(NULL);
    memcpy(ipodjs_ui_animation_new, FBADDR(0, 0), FRAMEBUFFER_SIZE);
    start_tick = current_tick;

    for (frame = 0; frame < IPODJS_UI_ANIMATION_SAMPLES; frame++)
    {
        int progress = frame * frame *
            (3 * (IPODJS_UI_ANIMATION_SAMPLES - 1) - 2 * frame);
        int divisor = (IPODJS_UI_ANIMATION_SAMPLES - 1) *
            (IPODJS_UI_ANIMATION_SAMPLES - 1) *
            (IPODJS_UI_ANIMATION_SAMPLES - 1);
        int reveal = (vertical ? LCD_HEIGHT : LCD_WIDTH) *
                     progress / divisor;

        ipodjs_ui_animation_wait(start_tick, frame);
        if (!vertical)
            reveal = ipodjs_ui_animation_position(start_tick, frame, LCD_WIDTH);
        if (!vertical && reveal == LCD_WIDTH)
            frame = IPODJS_UI_ANIMATION_SAMPLES - 1;

        if (vertical && direction > 0)
        {
            memcpy(FBADDR(0, 0), ipodjs_ui_animation_old,
                   FRAMEBUFFER_SIZE);
            if (reveal > 0)
                memcpy(FBADDR(0, 0),
                       ipodjs_ui_animation_new +
                           (LCD_HEIGHT - reveal) * LCD_WIDTH,
                       reveal * LCD_WIDTH * sizeof(fb_data));
        }
        else if (vertical)
        {
            memcpy(FBADDR(0, 0), ipodjs_ui_animation_new,
                   FRAMEBUFFER_SIZE);
            if (LCD_HEIGHT - reveal > 0)
                memcpy(FBADDR(0, 0),
                       ipodjs_ui_animation_old + reveal * LCD_WIDTH,
                       (LCD_HEIGHT - reveal) * LCD_WIDTH *
                           sizeof(fb_data));
        }
        else if (direction > 0)
        {
            if (LCD_WIDTH - reveal > 0)
                display->bitmap_part(ipodjs_ui_animation_old, reveal, 0,
                                     LCD_WIDTH,
                                     0, 0, LCD_WIDTH - reveal, LCD_HEIGHT);
            if (reveal > 0)
                display->bitmap_part(ipodjs_ui_animation_new, 0, 0,
                                     LCD_WIDTH,
                                     LCD_WIDTH - reveal, 0,
                                     reveal, LCD_HEIGHT);
        }
        else
        {
            if (reveal > 0)
                display->bitmap_part(ipodjs_ui_animation_new,
                                     LCD_WIDTH - reveal, 0,
                                     LCD_WIDTH, 0, 0,
                                     reveal, LCD_HEIGHT);
            if (LCD_WIDTH - reveal > 0)
                display->bitmap_part(ipodjs_ui_animation_old, 0, 0,
                                     LCD_WIDTH,
                                     reveal, 0, LCD_WIDTH - reveal,
                                     LCD_HEIGHT);
        }
        display->update();
        ipodjs_trace_screen("Transition",
                            vertical ? (direction > 0 ? "down" : "up") :
                            (direction > 0 ? "forward" : "back"),
                            reveal, frame, IPODJS_UI_ANIMATION_SAMPLES,
                            0, 0, LCD_WIDTH, LCD_HEIGHT);
    }

    memcpy(FBADDR(0, 0), ipodjs_ui_animation_new, FRAMEBUFFER_SIZE);
    display->update();
    ipodjs_ui_transition_cancel();
    return true;
#else
    (void)display;
    return false;
#endif
}

bool ipodjs_ui_preview_fade_begin(struct screen *display,
                                  int x, int y, int width, int height)
{
#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    if (!display || display->screen_type != SCREEN_MAIN || button_hold() ||
        ipodjs_ui_transition_direction != 0 || width <= 0 || height <= 0 ||
        x < 0 || y < 0 || x + width > LCD_WIDTH || y + height > LCD_HEIGHT)
        return false;

    display->set_viewport(NULL);
    for (int row = y; row < y + height; row++)
    {
        memcpy(ipodjs_ui_animation_old + row * LCD_WIDTH + x,
               FBADDR(x, row), width * sizeof(fb_data));
    }
    ipodjs_ui_preview_fade_x = x;
    ipodjs_ui_preview_fade_y = y;
    ipodjs_ui_preview_fade_w = width;
    ipodjs_ui_preview_fade_h = height;
    ipodjs_ui_preview_fade_active = true;
    return true;
#else
    (void)display;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return false;
#endif
}

bool ipodjs_ui_preview_fade_present(struct screen *display)
{
#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    int x, y, width, height;
    long start_tick;

    if (!display || display->screen_type != SCREEN_MAIN ||
        !ipodjs_ui_preview_fade_active)
        return false;
    if (!button_queue_empty() || button_hold())
    {
        ipodjs_ui_preview_fade_active = false;
        return false;
    }

    x = ipodjs_ui_preview_fade_x;
    y = ipodjs_ui_preview_fade_y;
    width = ipodjs_ui_preview_fade_w;
    height = ipodjs_ui_preview_fade_h;
    display->set_viewport(NULL);
    for (int row = y; row < y + height; row++)
    {
        memcpy(ipodjs_ui_animation_new + row * LCD_WIDTH + x,
               FBADDR(x, row), width * sizeof(fb_data));
    }
    start_tick = current_tick;

    for (int frame = 0; frame < IPODJS_UI_ANIMATION_SAMPLES; frame++)
    {
        ipodjs_ui_animation_wait(start_tick, frame);
        int alpha = ipodjs_ui_animation_position(start_tick, frame, 256);
        if (alpha == 256)
            frame = IPODJS_UI_ANIMATION_SAMPLES - 1;
        for (int row = y; row < y + height; row++)
        {
            fb_data *dst = FBADDR(x, row);
            fb_data *old = ipodjs_ui_animation_old + row * LCD_WIDTH + x;
            fb_data *new = ipodjs_ui_animation_new + row * LCD_WIDTH + x;

            for (int col = 0; col < width; col++)
            {
                unsigned r = (FB_UNPACK_RED(old[col]) * (256 - alpha) +
                              FB_UNPACK_RED(new[col]) * alpha) >> 8;
                unsigned g = (FB_UNPACK_GREEN(old[col]) * (256 - alpha) +
                              FB_UNPACK_GREEN(new[col]) * alpha) >> 8;
                unsigned b = (FB_UNPACK_BLUE(old[col]) * (256 - alpha) +
                              FB_UNPACK_BLUE(new[col]) * alpha) >> 8;

                dst[col] = FB_RGBPACK(r, g, b);
            }
        }
        display->update_rect(x, y, width, height);
        ipodjs_trace_screen("Preview Fade", "crossfade", alpha, frame,
                            IPODJS_UI_ANIMATION_SAMPLES,
                            x, y, width, height);
    }

    for (int row = y; row < y + height; row++)
    {
        memcpy(FBADDR(x, row),
               ipodjs_ui_animation_new + row * LCD_WIDTH + x,
               width * sizeof(fb_data));
    }
    display->update_rect(x, y, width, height);
    ipodjs_ui_preview_fade_active = false;
    return true;
#else
    (void)display;
    return false;
#endif
}

struct ipodjs_ui_stock_status_cache {
    struct bitmap bluetooth;
    unsigned char bluetooth_data[
        BM_SIZE(IPODJS_STOCK_BLUETOOTH_W, IPODJS_STOCK_BLUETOOTH_H,
                FORMAT_NATIVE, false) +
        ALIGN_UP(IPODJS_STOCK_BLUETOOTH_W, 2) *
            IPODJS_STOCK_BLUETOOTH_H / 2] IPODJS_BM_ALIGN;
    struct bitmap wifi;
    unsigned char wifi_data[
        BM_SIZE(IPODJS_STOCK_WIFI_W, IPODJS_STOCK_WIFI_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap airpods;
    unsigned char airpods_data[
        BM_SIZE(IPODJS_AIRPODS_W, IPODJS_AIRPODS_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    bool bluetooth_tried, bluetooth_valid;
    bool wifi_tried, wifi_valid;
    bool airpods_tried, airpods_valid;
};

static struct ipodjs_ui_stock_status_cache ipodjs_ui_stock_status;

/*
 * This is the complete status-bar subset from iPod35 RetailOS 2.0.4.  It is
 * prepared once at a native-screen entry point and draw functions below only
 * blend cached pixels.  Both 25-frame battery atlases are retained intact:
 * levels 0..22, plug, then charge.  Its pixel payload is 125,142 bytes and
 * never borrows playback/core memory.
 */
struct ipodjs_ui_retail_status_cache
{
    struct ipodjs_retailos_image light_background;
    struct ipodjs_retailos_image hold_background;
    struct ipodjs_retailos_image black_background;
    struct ipodjs_retailos_image status_play;
    struct ipodjs_retailos_image white_battery;
    struct ipodjs_retailos_image black_battery;
    struct ipodjs_retailos_image white_lock;
    struct ipodjs_retailos_image black_lock;
    struct ipodjs_retailos_image white_play;
    struct ipodjs_retailos_image white_pause;
    struct ipodjs_retailos_image black_play;
    struct ipodjs_retailos_image black_pause;
    struct ipodjs_retailos_image white_repeat;
    struct ipodjs_retailos_image white_repeat_once;
    struct ipodjs_retailos_image black_repeat;
    struct ipodjs_retailos_image black_repeat_once;
    struct ipodjs_retailos_image white_shuffle;
    struct ipodjs_retailos_image black_shuffle;
    unsigned char light_background_data[
        IPODJS_RETAILOS_RGA_BYTES(320, 20)];
    unsigned char hold_background_data[
        IPODJS_RETAILOS_RGA_BYTES(320, 24)];
    unsigned char black_background_data[
        IPODJS_RETAILOS_RGA_BYTES(320, 24)];
    unsigned char status_play_data[IPODJS_RETAILOS_RGA_BYTES(14, 16)];
    unsigned char white_battery_data[
        IPODJS_RETAILOS_RGA_BYTES(26, 13 * 25)];
    unsigned char black_battery_data[
        IPODJS_RETAILOS_RGA_BYTES(26, 13 * 25)];
    unsigned char white_lock_data[IPODJS_RETAILOS_RGA_BYTES(11, 16)];
    unsigned char black_lock_data[IPODJS_RETAILOS_RGA_BYTES(13, 16)];
    unsigned char white_play_data[IPODJS_RETAILOS_RGA_BYTES(14, 16)];
    unsigned char white_pause_data[IPODJS_RETAILOS_RGA_BYTES(14, 16)];
    unsigned char black_play_data[IPODJS_RETAILOS_RGA_BYTES(14, 16)];
    unsigned char black_pause_data[IPODJS_RETAILOS_RGA_BYTES(14, 16)];
    unsigned char white_repeat_data[IPODJS_RETAILOS_RGA_BYTES(20, 14)];
    unsigned char white_repeat_once_data[
        IPODJS_RETAILOS_RGA_BYTES(24, 16)];
    unsigned char black_repeat_data[IPODJS_RETAILOS_RGA_BYTES(19, 15)];
    unsigned char black_repeat_once_data[
        IPODJS_RETAILOS_RGA_BYTES(19, 15)];
    unsigned char white_shuffle_data[IPODJS_RETAILOS_RGA_BYTES(18, 16)];
    unsigned char black_shuffle_data[IPODJS_RETAILOS_RGA_BYTES(18, 14)];
    bool tried;
    bool valid;
};

static struct ipodjs_ui_retail_status_cache ipodjs_ui_retail_status;

/* The official charging archive supplies compositing components rather than
 * a hidden set of full-screen frames.  Retain every component, including the
 * distinct moving-fill cap and critical-battery surface, in fixed BSS. */
struct ipodjs_ui_retail_charge_cache
{
    struct ipodjs_retailos_image empty_left;
    struct ipodjs_retailos_image empty_right;
    struct ipodjs_retailos_image empty_middle;
    struct ipodjs_retailos_image green_left;
    struct ipodjs_retailos_image green_right;
    struct ipodjs_retailos_image green_middle;
    struct ipodjs_retailos_image green_middle_cap;
    struct ipodjs_retailos_image critical;
    struct ipodjs_retailos_frame_pack bolt;
    struct ipodjs_retailos_frame_pack plug;
    unsigned char empty_left_data[IPODJS_RETAILOS_RGA_BYTES(38, 142)];
    unsigned char empty_right_data[IPODJS_RETAILOS_RGA_BYTES(45, 142)];
    unsigned char empty_middle_data[IPODJS_RETAILOS_RGA_BYTES(8, 142)];
    unsigned char green_left_data[IPODJS_RETAILOS_RGA_BYTES(38, 142)];
    unsigned char green_right_data[IPODJS_RETAILOS_RGA_BYTES(45, 142)];
    unsigned char green_middle_data[IPODJS_RETAILOS_RGA_BYTES(8, 142)];
    unsigned char green_middle_cap_data[
        IPODJS_RETAILOS_RGA_BYTES(8, 142)];
    unsigned char critical_data[IPODJS_RETAILOS_RGA_BYTES(205, 163)];
    unsigned char bolt_data[24 * 57];
    unsigned char plug_data[36 * 29];
    bool tried;
    bool valid;
};

static struct ipodjs_ui_retail_charge_cache ipodjs_ui_retail_charge;

/* Disk Mode owns this buffer only while the USB screen is active.  The pack
 * is the complete 18-frame RetailOS SyncArrow1..35 (odd-numbered) sequence;
 * draw-time code only reads the cached source masks. */
struct ipodjs_ui_retail_usb_cache
{
    struct ipodjs_retailos_frame_pack sync_arrows;
    struct ipodjs_retailos_image badge;
    struct ipodjs_retailos_image disconnect;
    bool disconnect_valid;
    bool ejected;
    unsigned char sync_arrow_data[18 * 3648];
    long started;
    int last_frame;
    bool tried;
    bool valid;
    bool presented;
};

static struct ipodjs_ui_retail_usb_cache ipodjs_ui_retail_usb;

/* Retain the archived equalizer resources, but WPS no longer exposes them.
 * WPS uses the original music-note cover until playback art is ready.
 * Preparation is separate from all cached draw and animation callbacks. */
struct ipodjs_ui_retail_wps_cache
{
    /* White music strips add 3,195 fixed pixel bytes. Header is shared. */
    struct ipodjs_retailos_image progress[2];
    struct ipodjs_retailos_image star;
    struct ipodjs_retailos_image rating_star, rating_dot;
    unsigned char music_progress_data[2][
        IPODJS_RETAILOS_RGA_BYTES(16, 28)];
    unsigned char star_data[IPODJS_RETAILOS_RGA_BYTES(13, 13)];
    unsigned char rating_star_data[IPODJS_RETAILOS_RGA_BYTES(22, 26)];
    unsigned char rating_dot_data[IPODJS_RETAILOS_RGA_BYTES(22, 26)];
    bool chrome_valid;
    struct bitmap cover;
    fb_data cover_data[128 * 128] IPODJS_BM_ALIGN;
    struct ipodjs_retailos_image scrub, shuffle;
    unsigned char scrub_data[IPODJS_RETAILOS_RGA_BYTES(13, 28)];
    unsigned char shuffle_data[IPODJS_RETAILOS_RGA_BYTES(25, 19)];
    bool cover_valid, scrub_valid, shuffle_valid;
    struct ipodjs_retailos_frame_pack equalizer;
    struct ipodjs_retailos_image paused;
    unsigned char equalizer_data[
        IPODJS_RETAIL_WPS_EQUALIZER_FRAMES *
        IPODJS_RETAIL_WPS_EQUALIZER_FRAME_BYTES];
    unsigned char paused_data[
        IPODJS_RETAILOS_RGA_BYTES(IPODJS_RETAIL_WPS_PAUSED_W,
                                  IPODJS_RETAIL_WPS_PAUSED_H)];
    long started;
    bool tried;
    bool equalizer_valid;
    bool paused_valid;
};

static struct ipodjs_ui_retail_wps_cache ipodjs_ui_retail_wps;

/* The low-power Now Playing screen is assembled from the exact RetailOS
 * component images: ten proportional digits, colon, all eight battery
 * states, and the lock/play/radio state marks.  These fixed buffers are
 * intentionally independent of the audio and album-art buffers. */
struct ipodjs_ui_retail_idle_cache
{
    struct ipodjs_retailos_image digits[10];
    struct ipodjs_retailos_image colon;
    struct ipodjs_retailos_image battery;
    struct ipodjs_retailos_image lock;
    struct ipodjs_retailos_image play;
    struct ipodjs_retailos_image radio;
    unsigned char digit_data[10][
        IPODJS_RETAILOS_RGA_BYTES(IPODJS_RETAIL_IDLE_DIGIT_W,
                                  IPODJS_RETAIL_IDLE_DIGIT_H)];
    unsigned char colon_data[
        IPODJS_RETAILOS_RGA_BYTES(IPODJS_RETAIL_IDLE_COLON_W,
                                  IPODJS_RETAIL_IDLE_DIGIT_H)];
    unsigned char battery_data[
        IPODJS_RETAILOS_RGA_BYTES(IPODJS_RETAIL_IDLE_BATTERY_W,
                                  IPODJS_RETAIL_IDLE_BATTERY_H *
                                  IPODJS_RETAIL_IDLE_BATTERY_FRAMES)];
    unsigned char lock_data[IPODJS_RETAILOS_RGA_BYTES(25, 38)];
    unsigned char play_data[IPODJS_RETAILOS_RGA_BYTES(41, 43)];
    unsigned char radio_data[IPODJS_RETAILOS_RGA_BYTES(67, 40)];
    int last_hour;
    int last_minute;
    int last_battery_frame;
    int last_icon;
    bool tried;
    bool valid;
    bool presented;
};

static struct ipodjs_ui_retail_idle_cache ipodjs_ui_retail_idle;

/* Exact RetailOS system controls used by the existing iPodJS interaction
 * paths.  The three option-bar families, WPS progress pieces, two quick-
 * scroll plates, input-field pieces, and six endpoint icons are all loaded
 * at screen entry.  The deliberately padded slots keep the storage fixed and
 * make every subsequent draw a cached-pixel operation. */
struct ipodjs_ui_retail_controls_cache
{
    struct ipodjs_retailos_image submenu;
    unsigned char submenu_data[IPODJS_RETAILOS_RGA_BYTES(10, 15)];
    bool submenu_valid;
    struct ipodjs_retailos_image optionbar[
        IPODJS_RETAIL_OPTIONBAR_STYLE_COUNT]
        [IPODJS_RETAIL_OPTIONBAR_LAYER_COUNT]
        [IPODJS_RETAIL_OPTIONBAR_PART_COUNT];
    unsigned char optionbar_data[
        IPODJS_RETAIL_OPTIONBAR_STYLE_COUNT]
        [IPODJS_RETAIL_OPTIONBAR_LAYER_COUNT]
        [IPODJS_RETAIL_OPTIONBAR_PART_COUNT]
        [IPODJS_RETAILOS_RGA_BYTES(IPODJS_RETAIL_OPTIONBAR_MAX_W,
                                    IPODJS_RETAIL_OPTIONBAR_MAX_H)];
    struct ipodjs_retailos_image icons[IPODJS_RETAIL_CONTROL_ICON_COUNT];
    unsigned char icon_data[IPODJS_RETAIL_CONTROL_ICON_COUNT]
        [IPODJS_RETAILOS_RGA_BYTES(IPODJS_RETAIL_CONTROL_ICON_MAX_W,
                                    IPODJS_RETAIL_CONTROL_ICON_MAX_H)];
    struct ipodjs_retailos_image progress[
        IPODJS_RETAIL_PROGRESS_LAYER_COUNT]
        [IPODJS_RETAIL_PROGRESS_PART_COUNT];
    unsigned char progress_data[
        IPODJS_RETAIL_PROGRESS_LAYER_COUNT]
        [IPODJS_RETAIL_PROGRESS_PART_COUNT]
        [IPODJS_RETAILOS_RGA_BYTES(IPODJS_RETAIL_PROGRESS_MAX_W,
                                    IPODJS_RETAIL_PROGRESS_H)];
    struct ipodjs_retailos_image quick_scroll[2];
    unsigned char quick_scroll_data[2]
        [IPODJS_RETAILOS_RGA_BYTES(IPODJS_RETAIL_QUICK_SCROLL_W,
                                    IPODJS_RETAIL_QUICK_SCROLL_H)];
    struct ipodjs_retailos_image input[IPODJS_RETAIL_INPUT_PART_COUNT];
    unsigned char input_data[IPODJS_RETAIL_INPUT_PART_COUNT]
        [IPODJS_RETAILOS_RGA_BYTES(IPODJS_RETAIL_INPUT_PART_W,
                                    IPODJS_RETAIL_INPUT_H)];
    bool tried;
    bool optionbar_valid;
    bool icons_valid;
    bool progress_valid;
    bool quick_scroll_valid;
    bool input_valid;
};

static struct ipodjs_ui_retail_controls_cache ipodjs_ui_retail_controls;

/* One opaque source background shared by full About and split previews.
 * RGB565 drops the redundant alpha plane; no compositor copy is added. */
static struct {
    bool tried, controls_valid, preview_valid;
    struct ipodjs_retailos_image scrollbar[3], check[2];
    struct ipodjs_retailos_image selection[3];
    struct ipodjs_retailos_image panel;
    struct ipodjs_retailos_frame_pack main_menu, logo;
    struct ipodjs_retailos_image capacity[2][3];
    unsigned char scrollbar_data[3][IPODJS_RETAILOS_RGA_BYTES(6, 10)];
    unsigned char check_data[2][IPODJS_RETAILOS_RGA_BYTES(14, 12)];
    unsigned char selection_data[2][
        IPODJS_RETAILOS_RGA_BYTES(IPODJS_RETAIL_SELECTION_W,
                                  IPODJS_RETAIL_SELECTION_H)];
    unsigned char selection_fill_data[
        IPODJS_RETAILOS_RGA_BYTES(1, IPODJS_RETAIL_SELECTION_H)];
    bool selection_valid;
    uint16_t background_data[320 * 240];
    unsigned char panel_data[IPODJS_RETAILOS_RGA_BYTES(78, 121)];
    unsigned char main_menu_data[52 * 121];
    unsigned char logo_data[120 * 119];
    unsigned char capacity_data[2][3][IPODJS_RETAILOS_RGA_BYTES(9, 13)];
} ipodjs_ui_retail_menu;

const uint16_t *ipodjs_ui_tv_background(void)
{
    return ipodjs_ui_retail_menu.preview_valid ?
        ipodjs_ui_retail_menu.background_data : NULL;
}

const struct ipodjs_retailos_image *ipodjs_ui_tv_asset(enum ipodjs_tv_asset asset)
{
    const struct ipodjs_ui_retail_controls_cache *c = &ipodjs_ui_retail_controls;
    const struct ipodjs_ui_retail_status_cache *s = &ipodjs_ui_retail_status;
    switch (asset)
    {
        case IPODJS_TV_SELECTION:
            return c->optionbar_valid ? c->optionbar[0][1] : NULL;
        case IPODJS_TV_PROGRESS:
            return c->progress_valid ? c->progress[0] : NULL;
        case IPODJS_TV_PROGRESS_FILL:
            return c->progress_valid ? c->progress[1] : NULL;
        default: break;
    }
    if (!s->valid) return NULL;
    switch (asset)
    {
        case IPODJS_TV_HEADER: return &s->light_background;
        case IPODJS_TV_PLAY: return &s->black_play;
        case IPODJS_TV_PAUSE: return &s->black_pause;
        case IPODJS_TV_SHUFFLE: return &s->white_shuffle;
        case IPODJS_TV_REPEAT: return &s->white_repeat;
        case IPODJS_TV_REPEAT_ONE: return &s->white_repeat_once;
        default: return NULL;
    }
}

bool ipodjs_ui_prepare_retailos_menu(void)
{
    static const uint8_t heights[3] = { 10, 8, 9 };
    static const uint8_t widths[2][3] = { { 9, 1, 9 }, { 7, 1, 7 } };
    bool valid = true;
    bool selection_valid = true;

    if (ipodjs_ui_retail_menu.tried)
        return ipodjs_ui_retail_menu.preview_valid;
    /* No late font/allocation or artwork work from music/navigation. */
    if (audio_status())
        return false;
    ipodjs_ui_retail_menu.tried = true;
    selection_valid &= ipodjs_retailos_load_resource_rga(89,
        ipodjs_ui_retail_menu.selection_data[0],
        sizeof(ipodjs_ui_retail_menu.selection_data[0]),
        IPODJS_RETAIL_SELECTION_W, IPODJS_RETAIL_SELECTION_H,
        &ipodjs_ui_retail_menu.selection[0]);
    selection_valid &= ipodjs_retailos_load_resource_rga(91,
        ipodjs_ui_retail_menu.selection_fill_data,
        sizeof(ipodjs_ui_retail_menu.selection_fill_data),
        1, IPODJS_RETAIL_SELECTION_H,
        &ipodjs_ui_retail_menu.selection[1]);
    selection_valid &= ipodjs_retailos_load_resource_rga(90,
        ipodjs_ui_retail_menu.selection_data[1],
        sizeof(ipodjs_ui_retail_menu.selection_data[1]),
        IPODJS_RETAIL_SELECTION_W, IPODJS_RETAIL_SELECTION_H,
        &ipodjs_ui_retail_menu.selection[2]);
    ipodjs_ui_retail_menu.selection_valid = selection_valid;
    for (int part = 0; part < 3; part++)
        valid &= ipodjs_retailos_load_resource_rga(part,
            ipodjs_ui_retail_menu.scrollbar_data[part],
            sizeof(ipodjs_ui_retail_menu.scrollbar_data[part]),
            6, heights[part], &ipodjs_ui_retail_menu.scrollbar[part]);
    for (int state = 0; state < 2; state++)
        valid &= ipodjs_retailos_load_resource_rga(448 + state,
            ipodjs_ui_retail_menu.check_data[state],
            sizeof(ipodjs_ui_retail_menu.check_data[state]),
            14, 12, &ipodjs_ui_retail_menu.check[state]);
    ipodjs_ui_retail_menu.controls_valid = valid;
    valid = ipodjs_retailos_load_opaque(11,
        ipodjs_ui_retail_menu.background_data,
        ARRAYLEN(ipodjs_ui_retail_menu.background_data), 320, 240);
    valid &= ipodjs_retailos_load_resource_rga(429,
        ipodjs_ui_retail_menu.panel_data,
        sizeof(ipodjs_ui_retail_menu.panel_data),
        78, 121, &ipodjs_ui_retail_menu.panel);
    valid &= ipodjs_retailos_load_named_raw("settings-main-menu",
        ipodjs_ui_retail_menu.main_menu_data,
        sizeof(ipodjs_ui_retail_menu.main_menu_data),
        78, 121, 52, 0x0004, 52 * 121, 0x0dad0bc0,
        &ipodjs_ui_retail_menu.main_menu);
    valid &= ipodjs_retailos_load_named_raw("settings-apple-logo",
        ipodjs_ui_retail_menu.logo_data,
        sizeof(ipodjs_ui_retail_menu.logo_data),
        119, 119, 120, 0x0008, 120 * 119, 0x0dad0bb8,
        &ipodjs_ui_retail_menu.logo);
    for (int layer = 0; layer < 2; layer++)
        for (int part = 0; part < 3; part++)
            valid &= ipodjs_retailos_load_resource_rga(401 + layer * 3 + part,
                ipodjs_ui_retail_menu.capacity_data[layer][part],
                sizeof(ipodjs_ui_retail_menu.capacity_data[layer][part]),
                widths[layer][part], 13,
                &ipodjs_ui_retail_menu.capacity[layer][part]);
    ipodjs_ui_retail_menu.preview_valid = valid;
    return valid;
}

bool ipodjs_ui_draw_retailos_background(struct screen *display)
{
    return ipodjs_ui_draw_retailos_background_rect(display, 0, 0, 320, 240);
}

bool ipodjs_ui_draw_retailos_background_rect(struct screen *display,
    int x, int y, int width, int height)
{
    if (!ipodjs_ui_retail_menu.preview_valid)
        return false;
    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x + width > 320 || y + height > 240)
        return false;
    ipodjs_retailos_blit_opaque(display,
        ipodjs_ui_retail_menu.background_data + y * 320 + x,
        320, x, y, width, height);
    return true;
}

void ipodjs_ui_draw_retailos_check(struct screen *display, int x, int y,
                                  bool selected)
{
    if (ipodjs_ui_retail_menu.controls_valid)
        ipodjs_retailos_blit(display,
            &ipodjs_ui_retail_menu.check[selected ? 1 : 0], x, y);
    else
    {
        /* Preserve the enabled state when the private asset pack is absent. */
        display->setfont(ipodjs_ui_retailos_font(false));
        display->putsxy(x, y, "On");
        display->setfont(ipodjs_ui_retailos_menu_font());
    }
}

void ipodjs_ui_draw_retailos_scrollbar(struct screen *display, int right,
    int y, int height, int first, int visible, int count)
{
    if (!ipodjs_ui_retail_menu.controls_valid || count <= visible ||
        visible <= 0 || height < 19)
        return;
    int size = MAX(19, height * visible / count);
    int top = y + (height - size) * MAX(0, MIN(first, count - visible)) /
        (count - visible);
    int x = right - 6;
    ipodjs_retailos_blit(display, &ipodjs_ui_retail_menu.scrollbar[0], x, top);
    for (int row = 10; row < size - 9; row += 8)
        ipodjs_retailos_blit_part(display,
            &ipodjs_ui_retail_menu.scrollbar[1], 0, 0, x, top + row,
            6, MIN(8, size - 9 - row));
    ipodjs_retailos_blit(display, &ipodjs_ui_retail_menu.scrollbar[2],
        x, top + size - 9);
}

static struct bitmap *ipodjs_ui_load_stock_status(const char *path,
                                                   struct bitmap *bm,
                                                   unsigned char *data,
                                                   size_t data_size,
                                                   int width, int height,
                                                   bool *tried, bool *valid)
{
    int rc;

    if (*valid)
        return bm;
    if (*tried)
        return NULL;
    *tried = true;
    if (!file_exists(path))
        return NULL;

    memset(bm, 0, sizeof(*bm));
    bm->width = width;
    bm->height = height;
    bm->format = FORMAT_NATIVE;
    bm->data = data;
    rc = read_bmp_file(path, bm, data_size,
                       FORMAT_NATIVE | FORMAT_DITHER | FORMAT_TRANSPARENT,
                       NULL);
    if (rc < 0 || bm->width != width || bm->height != height)
        return NULL;
    *valid = true;
    return bm;
}

bool ipodjs_ui_prepare_retailos_status(void)
{
    struct ipodjs_ui_retail_status_cache *cache =
        &ipodjs_ui_retail_status;
    bool valid = true;

    if (cache->valid)
        return true;
    if (cache->tried)
        return false;
    cache->tried = true;

    /* Apple's Classic guide, "Using iPod classic menus", shows resource 8:
     * a light silver 20px bar with black text. Resource 10 is the dark bar,
     * used by the preserved Hold presentation and RetailOS USB screen. */
    valid &= ipodjs_retailos_load_named_rga(
        "statusbar-white-background", cache->light_background_data,
        sizeof(cache->light_background_data), 320, 20,
        &cache->light_background);
    valid &= ipodjs_retailos_load_named_rga(
        "statusbar-black-background", cache->hold_background_data,
        sizeof(cache->hold_background_data), 320, 24,
        &cache->hold_background);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-statusbar", cache->black_background_data,
        sizeof(cache->black_background_data), 320, 24,
        &cache->black_background);
    valid &= ipodjs_retailos_load_named_rga(
        "statusbar-white-play-status", cache->status_play_data,
        sizeof(cache->status_play_data), 14, 16, &cache->status_play);
    valid &= ipodjs_retailos_load_animation_rga(
        IPODJS_RETAILOS_STATUSBAR_WHITE_BATTERY,
        cache->white_battery_data, sizeof(cache->white_battery_data),
        &cache->white_battery);
    valid &= ipodjs_retailos_load_animation_rga(
        IPODJS_RETAILOS_STATUSBAR_BLACK_BATTERY,
        cache->black_battery_data, sizeof(cache->black_battery_data),
        &cache->black_battery);
    valid &= ipodjs_retailos_load_named_rga(
        "statusbar-black-lock", cache->white_lock_data,
        sizeof(cache->white_lock_data), 11, 16, &cache->white_lock);
    valid &= ipodjs_retailos_load_named_rga(
        "statusbar-white-lock", cache->black_lock_data,
        sizeof(cache->black_lock_data), 13, 16, &cache->black_lock);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-black-play", cache->white_play_data,
        sizeof(cache->white_play_data), 14, 16, &cache->white_play);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-black-pause", cache->white_pause_data,
        sizeof(cache->white_pause_data), 14, 16, &cache->white_pause);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-white-play", cache->black_play_data,
        sizeof(cache->black_play_data), 14, 16, &cache->black_play);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-white-pause", cache->black_pause_data,
        sizeof(cache->black_pause_data), 14, 16, &cache->black_pause);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-white-repeat", cache->white_repeat_data,
        sizeof(cache->white_repeat_data), 20, 14,
        &cache->white_repeat);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-white-repeat-once", cache->white_repeat_once_data,
        sizeof(cache->white_repeat_once_data), 24, 16,
        &cache->white_repeat_once);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-black-repeat", cache->black_repeat_data,
        sizeof(cache->black_repeat_data), 19, 15,
        &cache->black_repeat);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-black-repeat-once", cache->black_repeat_once_data,
        sizeof(cache->black_repeat_once_data), 19, 15,
        &cache->black_repeat_once);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-white-shuffle", cache->white_shuffle_data,
        sizeof(cache->white_shuffle_data), 18, 16,
        &cache->white_shuffle);
    valid &= ipodjs_retailos_load_named_rga(
        "now-playing-black-shuffle", cache->black_shuffle_data,
        sizeof(cache->black_shuffle_data), 18, 14,
        &cache->black_shuffle);

    cache->valid = valid;
    return valid;
}

bool ipodjs_ui_prepare_retailos_controls(void)
{
    static const char * const optionbar_names
        [IPODJS_RETAIL_OPTIONBAR_STYLE_COUNT]
        [IPODJS_RETAIL_OPTIONBAR_LAYER_COUNT]
        [IPODJS_RETAIL_OPTIONBAR_PART_COUNT] =
    {
        {
            { "optionbar-white-well-left",
              "optionbar-white-well-center",
              "optionbar-white-well-right" },
            { "optionbar-white-thumb-left",
              "optionbar-white-thumb-center",
              "optionbar-white-thumb-right" },
        },
        {
            { "optionbar-black-well-left",
              "optionbar-black-well-center",
              "optionbar-black-well-right" },
            { "optionbar-black-thumb-left",
              "optionbar-black-thumb-center",
              "optionbar-black-thumb-right" },
        },
        {
            { "optionbar-now-playing-well-left",
              "optionbar-now-playing-well-center",
              "optionbar-now-playing-well-right" },
            { "optionbar-now-playing-thumb-left",
              "optionbar-now-playing-thumb-center",
              "optionbar-now-playing-thumb-right" },
        },
    };
    static const uint8_t optionbar_widths
        [IPODJS_RETAIL_OPTIONBAR_STYLE_COUNT]
        [IPODJS_RETAIL_OPTIONBAR_PART_COUNT] =
    {
        { 8, 1, 8 },
        { 6, 1, 6 },
        { 8, 1, 8 },
    };
    static const uint8_t optionbar_heights
        [IPODJS_RETAIL_OPTIONBAR_STYLE_COUNT] = { 29, 26, 27 };
    static const char * const icon_names[IPODJS_RETAIL_CONTROL_ICON_COUNT] =
    {
        "system-overlay-brightness-less",
        "system-overlay-brightness-more",
        "system-overlay-volume-left",
        "system-overlay-volume-right",
        "now-playing-white-volume-low",
        "now-playing-white-volume-high",
    };
    static const uint8_t icon_widths[IPODJS_RETAIL_CONTROL_ICON_COUNT] =
        { 12, 17, 15, 19, 9, 19 };
    static const uint8_t icon_heights[IPODJS_RETAIL_CONTROL_ICON_COUNT] =
        { 13, 17, 13, 17, 17, 17 };
    static const char * const progress_names
        [IPODJS_RETAIL_PROGRESS_LAYER_COUNT]
        [IPODJS_RETAIL_PROGRESS_PART_COUNT] =
    {
        { "now-playing-progressbar-left",
          "now-playing-progressbar-growth",
          "now-playing-progressbar-right" },
        { "now-playing-progressfill-left",
          "now-playing-progressfill-growth",
          "now-playing-progressfill-right" },
    };
    static const uint8_t progress_widths
        [IPODJS_RETAIL_PROGRESS_LAYER_COUNT]
        [IPODJS_RETAIL_PROGRESS_PART_COUNT] =
    {
        { 12, 5, 12 },
        { 3, 2, 4 },
    };
    static const char * const quick_scroll_names[2] =
        { "system-quick-scroll", "system-quick-scroll-123" };
    static const char * const input_names[IPODJS_RETAIL_INPUT_PART_COUNT] =
        { "system-input-field-left", "system-input-field-middle",
          "system-input-field-right" };
    struct ipodjs_ui_retail_controls_cache *cache =
        &ipodjs_ui_retail_controls;
    bool valid;
    int style;
    int layer;
    int part;
    int icon;

    if (cache->tried)
        return cache->optionbar_valid && cache->icons_valid &&
               cache->progress_valid && cache->quick_scroll_valid &&
               cache->input_valid;
    cache->tried = true;

    cache->submenu_valid = ipodjs_retailos_load_named_rga(
        "system-submenu", cache->submenu_data,
        sizeof(cache->submenu_data), 10, 15, &cache->submenu);
    valid = true;
    for (style = 0; style < IPODJS_RETAIL_OPTIONBAR_STYLE_COUNT; style++)
    {
        for (layer = 0; layer < IPODJS_RETAIL_OPTIONBAR_LAYER_COUNT;
             layer++)
        {
            for (part = 0; part < IPODJS_RETAIL_OPTIONBAR_PART_COUNT;
                 part++)
            {
                valid &= ipodjs_retailos_load_named_rga(
                    optionbar_names[style][layer][part],
                    cache->optionbar_data[style][layer][part],
                    sizeof(cache->optionbar_data[style][layer][part]),
                    optionbar_widths[style][part],
                    optionbar_heights[style],
                    &cache->optionbar[style][layer][part]);
            }
        }
    }
    cache->optionbar_valid = valid;

    valid = true;
    for (icon = 0; icon < IPODJS_RETAIL_CONTROL_ICON_COUNT; icon++)
    {
        valid &= ipodjs_retailos_load_named_rga(
            icon_names[icon], cache->icon_data[icon],
            sizeof(cache->icon_data[icon]), icon_widths[icon],
            icon_heights[icon], &cache->icons[icon]);
    }
    cache->icons_valid = valid;

    valid = true;
    for (layer = 0; layer < IPODJS_RETAIL_PROGRESS_LAYER_COUNT; layer++)
    {
        for (part = 0; part < IPODJS_RETAIL_PROGRESS_PART_COUNT; part++)
        {
            valid &= ipodjs_retailos_load_named_rga(
                progress_names[layer][part],
                cache->progress_data[layer][part],
                sizeof(cache->progress_data[layer][part]),
                progress_widths[layer][part], IPODJS_RETAIL_PROGRESS_H,
                &cache->progress[layer][part]);
        }
    }
    cache->progress_valid = valid;

    valid = true;
    for (part = 0; part < 2; part++)
    {
        valid &= ipodjs_retailos_load_named_rga(
            quick_scroll_names[part], cache->quick_scroll_data[part],
            sizeof(cache->quick_scroll_data[part]),
            IPODJS_RETAIL_QUICK_SCROLL_W,
            IPODJS_RETAIL_QUICK_SCROLL_H,
            &cache->quick_scroll[part]);
    }
    cache->quick_scroll_valid = valid;

    valid = true;
    for (part = 0; part < IPODJS_RETAIL_INPUT_PART_COUNT; part++)
    {
        valid &= ipodjs_retailos_load_named_rga(
            input_names[part], cache->input_data[part],
            sizeof(cache->input_data[part]), IPODJS_RETAIL_INPUT_PART_W,
            IPODJS_RETAIL_INPUT_H, &cache->input[part]);
    }
    cache->input_valid = valid;

    return cache->optionbar_valid && cache->icons_valid &&
           cache->progress_valid && cache->quick_scroll_valid &&
           cache->input_valid;
}

static bool ipodjs_ui_draw_retailos_horizontal_parts(
    struct screen *display, const struct ipodjs_retailos_image parts[3],
    int x, int y, int width)
{
    int center_width;
    int drawn;

    if (!display || !parts[0].pixels || !parts[1].pixels ||
        !parts[2].pixels || parts[0].height != parts[1].height ||
        parts[0].height != parts[2].height || parts[1].width == 0 ||
        width < parts[0].width + parts[2].width)
        return false;

    ipodjs_retailos_blit(display, &parts[0], x, y);
    center_width = width - parts[0].width - parts[2].width;
    drawn = 0;
    while (drawn < center_width)
    {
        int part_width = MIN((int)parts[1].width, center_width - drawn);

        ipodjs_retailos_blit_part(
            display, &parts[1], 0, 0,
            x + parts[0].width + drawn, y,
            part_width, parts[1].height);
        drawn += part_width;
    }
    ipodjs_retailos_blit(display, &parts[2],
                         x + width - parts[2].width, y);
    return true;
}

static bool ipodjs_ui_draw_retailos_horizontal_parts_color(
    struct screen *display, const struct ipodjs_retailos_image parts[3],
    int x, int y, int width, fb_data color)
{
    int center_width;
    int drawn = 0;

    if (!display || !parts[0].pixels || !parts[1].pixels ||
        !parts[2].pixels || parts[0].height != parts[1].height ||
        parts[0].height != parts[2].height || parts[1].width == 0 ||
        width < parts[0].width + parts[2].width)
        return false;

    ipodjs_retailos_blit_color(display, &parts[0], x, y, color);
    center_width = width - parts[0].width - parts[2].width;
    while (drawn < center_width)
    {
        int part_width = MIN((int)parts[1].width, center_width - drawn);

        ipodjs_retailos_blit_color_part(display, &parts[1], 0, 0,
            x + parts[0].width + drawn, y, part_width, parts[1].height,
            color);
        drawn += part_width;
    }
    ipodjs_retailos_blit_color(display, &parts[2],
        x + width - parts[2].width, y, color);
    return true;
}

bool ipodjs_ui_draw_retailos_settings_preview(struct screen *display,
    int x, int y, int width, int height, bool main_menu,
    const char *title, const char *detail, int used_percent)
{
    if (!display || !ipodjs_ui_retail_menu.preview_valid ||
        x != 160 || width != 160 || y < 0 || y + height > 240)
        return false;
    ipodjs_retailos_blit_opaque(display,
        ipodjs_ui_retail_menu.background_data + y * 320 + x,
        320, x, y, width, height);
    if (main_menu)
    {
        /* SettingsInfo_Template and MainMenu layout: native 78x121 layers,
         * including the source reflection, at absolute LCD (202,81). */
        ipodjs_retailos_blit(display, &ipodjs_ui_retail_menu.panel, 202, 81);
        ipodjs_retailos_blit_mask(display,
            &ipodjs_ui_retail_menu.main_menu, 0, 202, 81,
            FB_RGBPACK(238, 238, 238));
    }
    else
    {
        ipodjs_retailos_blit_mask(display, &ipodjs_ui_retail_menu.logo,
            0, 180, 45, FB_RGBPACK(255, 255, 255));
        /* SettingsInfo_About: the simple capacity view is 130x13, not
         * the 30-pixel detailed About capacity strip. */
        ipodjs_ui_draw_retailos_horizontal_parts(display,
            ipodjs_ui_retail_menu.capacity[0], 179, 173, 130);
        int filled = 130 * MAX(0, MIN(used_percent, 100)) / 100;
        if (filled >= 14)
            ipodjs_ui_draw_retailos_horizontal_parts(display,
                ipodjs_ui_retail_menu.capacity[1], 179, 173, filled);
        else if (filled > 0)
        {
            int left = (filled + 1) / 2;
            int right = filled - left;
            ipodjs_retailos_blit_part(display,
                &ipodjs_ui_retail_menu.capacity[1][0], 0, 0,
                179, 173, left, 13);
            if (right)
                ipodjs_retailos_blit_part(display,
                    &ipodjs_ui_retail_menu.capacity[1][2], 7 - right, 0,
                    179 + left, 173, right, 13);
        }
    }
    int old_mode = lcd_get_drawmode();
    display->set_drawmode(DRMODE_FG);
    display->set_foreground(LCD_RGBPACK(255, 255, 255));
    display->setfont(ipodjs_ui_retailos_font(true));
    ipodjs_ui_puts_fit(display, 170, 20, 140, title, true);
    /* About's free-space label is System_Font, unlike the small Main Menu
     * explanatory text. SettingsInfo_About_Template, word offset 516. */
    display->setfont(ipodjs_ui_retailos_font(!main_menu));
    ipodjs_ui_puts_fit(display, 165, main_menu ? 207 : 199,
        150, detail, true);
    display->set_drawmode(old_mode);
    return true;
}

bool ipodjs_ui_draw_retailos_optionbar(
    struct screen *display, int x, int y, int width, int percent,
    enum ipodjs_ui_retailos_optionbar_style style)
{
    struct ipodjs_ui_retail_controls_cache *cache =
        &ipodjs_ui_retail_controls;
    const struct ipodjs_retailos_image *well;
    const struct ipodjs_retailos_image *thumb;
    int filled;

    if (!display || display != &screens[SCREEN_MAIN] ||
        !cache->optionbar_valid ||
        style < 0 || style >= IPODJS_RETAIL_OPTIONBAR_STYLE_COUNT)
        return false;

    well = cache->optionbar[style][0];
    thumb = cache->optionbar[style][1];
    if (!ipodjs_ui_draw_retailos_horizontal_parts(
            display, well, x, y, width))
        return false;

    percent = MAX(0, MIN(100, percent));
    if (percent == 0)
        return true;
    filled = width * percent / 100;
    filled = MAX((int)thumb[0].width + thumb[2].width, filled);
    filled = MIN(width, filled);
    return ipodjs_ui_draw_retailos_horizontal_parts_color(
        display, thumb, x, y, filled, ipodjs_ui_accent());
}

bool ipodjs_ui_draw_retailos_adjustment(
    struct screen *display, int x, int y, int width, int percent,
    bool brightness, bool now_playing)
{
    struct ipodjs_ui_retail_controls_cache *cache =
        &ipodjs_ui_retail_controls;
    enum ipodjs_ui_retailos_optionbar_style style;
    const struct ipodjs_retailos_image *left;
    const struct ipodjs_retailos_image *right;
    int height;

    if (!cache->optionbar_valid || !cache->icons_valid)
        return false;
    if (now_playing)
    {
        style = IPODJS_UI_RETAILOS_OPTIONBAR_NOW_PLAYING;
        left = &cache->icons[4];
        right = &cache->icons[5];
    }
    else
    {
        style = global_settings.ui_engine_dark_mode ?
            IPODJS_UI_RETAILOS_OPTIONBAR_BLACK :
            IPODJS_UI_RETAILOS_OPTIONBAR_WHITE;
        left = &cache->icons[brightness ? 0 : 2];
        right = &cache->icons[brightness ? 1 : 3];
    }
    height = cache->optionbar[style][0][0].height;
    if (!ipodjs_ui_draw_retailos_optionbar(
            display, x, y, width, percent, style))
        return false;

    ipodjs_retailos_blit(display, left, x - 10 - left->width,
                         y + (height - left->height) / 2);
    ipodjs_retailos_blit(display, right, x + width + 10,
                         y + (height - right->height) / 2);
    return true;
}

bool ipodjs_ui_draw_retailos_control_icon(
    struct screen *display, int x, int y, bool brightness, bool high,
    bool now_playing)
{
    struct ipodjs_ui_retail_controls_cache *cache =
        &ipodjs_ui_retail_controls;
    int icon;

    if (!display || display != &screens[SCREEN_MAIN] ||
        !cache->icons_valid || (brightness && now_playing))
        return false;
    if (now_playing)
        icon = high ? 5 : 4;
    else if (brightness)
        icon = high ? 1 : 0;
    else
        icon = high ? 3 : 2;
    ipodjs_retailos_blit(display, &cache->icons[icon], x, y);
    return true;
}

bool ipodjs_ui_draw_retailos_progress(
    struct screen *display, int x, int y, int width, int height,
    int percent)
{
    struct ipodjs_ui_retail_controls_cache *cache =
        &ipodjs_ui_retail_controls;
    int track_y;
    int filled;

    if (!display || display != &screens[SCREEN_MAIN] ||
        !cache->progress_valid || height < IPODJS_RETAIL_PROGRESS_H)
        return false;

    track_y = y + (height - IPODJS_RETAIL_PROGRESS_H) / 2;
    if (!ipodjs_ui_draw_retailos_horizontal_parts(
            display, cache->progress[0], x, track_y, width))
        return false;

    percent = MAX(0, MIN(100, percent));
    if (percent == 0)
        return true;
    filled = width * percent / 100;
    filled = MAX((int)cache->progress[1][0].width +
                 cache->progress[1][2].width, filled);
    filled = MIN(width, filled);
    return ipodjs_ui_draw_retailos_horizontal_parts_color(
        display, cache->progress[1], x, track_y, filled,
        ipodjs_ui_accent());
}

static bool ipodjs_ui_prepare_retailos_charging(void)
{
    struct ipodjs_ui_retail_charge_cache *cache =
        &ipodjs_ui_retail_charge;
    bool valid = true;

    if (cache->valid)
        return true;
    if (cache->tried)
        return false;
    cache->tried = true;

    valid &= ipodjs_retailos_load_named_rga(
        "charging-empty-cap-left", cache->empty_left_data,
        sizeof(cache->empty_left_data), 38, 142, &cache->empty_left);
    valid &= ipodjs_retailos_load_named_rga(
        "charging-empty-cap-right", cache->empty_right_data,
        sizeof(cache->empty_right_data), 45, 142, &cache->empty_right);
    valid &= ipodjs_retailos_load_named_rga(
        "charging-empty-middle", cache->empty_middle_data,
        sizeof(cache->empty_middle_data), 8, 142, &cache->empty_middle);
    valid &= ipodjs_retailos_load_named_rga(
        "charging-green-cap-left", cache->green_left_data,
        sizeof(cache->green_left_data), 38, 142, &cache->green_left);
    valid &= ipodjs_retailos_load_named_rga(
        "charging-green-cap-right", cache->green_right_data,
        sizeof(cache->green_right_data), 45, 142, &cache->green_right);
    valid &= ipodjs_retailos_load_named_rga(
        "charging-green-middle", cache->green_middle_data,
        sizeof(cache->green_middle_data), 8, 142,
        &cache->green_middle);
    valid &= ipodjs_retailos_load_named_rga(
        "charging-green-middle-cap", cache->green_middle_cap_data,
        sizeof(cache->green_middle_cap_data), 8, 142,
        &cache->green_middle_cap);
    valid &= ipodjs_retailos_load_named_rga(
        "charging-critical", cache->critical_data,
        sizeof(cache->critical_data), 205, 163, &cache->critical);
    valid &= ipodjs_retailos_load_named_raw(
        "charging-charge", cache->bolt_data, sizeof(cache->bolt_data),
        23, 57, 24, 0x0004, sizeof(cache->bolt_data),
        0x0dad0df1, &cache->bolt);
    valid &= ipodjs_retailos_load_named_raw(
        "charging-plug", cache->plug_data, sizeof(cache->plug_data),
        47, 29, 36, 0x0004, sizeof(cache->plug_data),
        0x0dad0df2, &cache->plug);

    cache->valid = valid;
    return valid;
}

static bool ipodjs_ui_prepare_retailos_usb(void)
{
    struct ipodjs_ui_retail_usb_cache *cache = &ipodjs_ui_retail_usb;
    cache->valid = false;
    cache->presented = false;
    cache->ejected = false;
    cache->disconnect_valid = false;
#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    struct ipodjs_retailos_image background;
    unsigned char *raw = (unsigned char *)ipodjs_ui_animation_frames;

    /* USB owns the LCD until disconnect. Reuse the inactive transition pair:
     * load the opaque source RGA, then compact/expand it in place to native
     * pixels. No playback arena allocation or additional full-screen cache. */
    ipodjs_ui_transition_cancel();
    if (!ipodjs_retailos_load_resource_rga(
            11, raw, sizeof(ipodjs_ui_animation_frames),
            LCD_WIDTH, LCD_HEIGHT, &background))
        return false;
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++)
        if (raw[i * 3 + 2] != 255)
            return false;
    for (int step = 0; step < LCD_WIDTH * LCD_HEIGHT; step++)
    {
        int i = sizeof(fb_data) > 3 ?
            LCD_WIDTH * LCD_HEIGHT - 1 - step : step;
        unsigned pixel = raw[i * 3] | (raw[i * 3 + 1] << 8);
        ipodjs_ui_animation_old[i] = FB_RGBPACK(
            ((pixel >> 11) & 31) * 255 / 31,
            ((pixel >> 5) & 63) * 255 / 63,
            (pixel & 31) * 255 / 31);
    }
    /* Resource 392 is DiskModeImage_SyncIcon: the original gold badge.
     * Resource 562 is only the connector mask, not this shared background. */
    cache->valid = ipodjs_retailos_load_resource_rga(
        392, ipodjs_ui_animation_new, sizeof(ipodjs_ui_animation_new),
        112, 112, &cache->badge);
    /* Keep the original disconnect artwork beside the badge in the
     * existing transition workspace, before USB takes storage away. */
    size_t badge_bytes = IPODJS_RETAILOS_RGA_BYTES(112, 112);
    cache->disconnect_valid = ipodjs_retailos_load_resource_rga(
        563, (unsigned char *)ipodjs_ui_animation_new + badge_bytes,
        sizeof(ipodjs_ui_animation_new) - badge_bytes,
        112, 112, &cache->disconnect);
    cache->valid &= ipodjs_retailos_load_animation_raw(
            IPODJS_RETAILOS_DISK_MODE_SYNC_ARROWS,
            cache->sync_arrow_data, sizeof(cache->sync_arrow_data),
            &cache->sync_arrows);
    cache->valid &= ipodjs_ui_prepare_retailos_status();
#endif
    cache->started = current_tick;
    cache->last_frame = -1;
    return cache->valid;
}

bool ipodjs_ui_prepare_retailos_playback(void)
{
    static const char * const digit_names[10] =
    {
        "now-playing-idle-digit-0", "now-playing-idle-digit-1",
        "now-playing-idle-digit-2", "now-playing-idle-digit-3",
        "now-playing-idle-digit-4", "now-playing-idle-digit-5",
        "now-playing-idle-digit-6", "now-playing-idle-digit-7",
        "now-playing-idle-digit-8", "now-playing-idle-digit-9",
    };
    static const uint16_t digit_widths[10] =
        { 38, 27, 39, 38, 38, 38, 38, 38, 38, 38 };
    struct ipodjs_ui_retail_wps_cache *wps = &ipodjs_ui_retail_wps;
    struct ipodjs_ui_retail_idle_cache *idle = &ipodjs_ui_retail_idle;
    bool wps_valid;
    bool idle_valid;
    int digit;

    if (!wps->tried)
    {
        wps->tried = true;
        /* Original CoverFlow_Proxy_Image. Decode into fixed storage before
         * rendering; expand backwards for simulators with 32-bit pixels. */
        wps->cover_valid = ipodjs_retailos_load_opaque(6,
            (uint16_t *)wps->cover_data, 128 * 128, 128, 128);
#if LCD_DEPTH > 16
        if (wps->cover_valid)
            for (int i = 128 * 128 - 1; i >= 0; i--)
            {
                unsigned pixel = ((uint16_t *)wps->cover_data)[i];
                wps->cover_data[i] = FB_RGBPACK(
                    ((pixel >> 11) & 31) * 255 / 31,
                    ((pixel >> 5) & 63) * 255 / 63,
                    (pixel & 31) * 255 / 31);
            }
#endif
        wps->cover.width = wps->cover.height = 128;
        wps->cover.format = FORMAT_NATIVE;
        wps->cover.data = (unsigned char *)wps->cover_data;
        wps->scrub_valid = ipodjs_retailos_load_resource_rga(
            298, wps->scrub_data, sizeof(wps->scrub_data),
            13, 28, &wps->scrub);
        wps->shuffle_valid = ipodjs_retailos_load_resource_rga(
            293, wps->shuffle_data, sizeof(wps->shuffle_data),
            25, 19, &wps->shuffle);
        wps->chrome_valid = true;
        for (int layer = 0; layer < 2; layer++)
            wps->chrome_valid &= ipodjs_retailos_load_resource_rga(
                296 + layer, wps->music_progress_data[layer],
                sizeof(wps->music_progress_data[layer]),
                16, 28, &wps->progress[layer]);
        wps->chrome_valid &= ipodjs_retailos_load_resource_rga(
            300, wps->star_data, sizeof(wps->star_data),
            13, 13, &wps->star);
        wps->chrome_valid &= ipodjs_retailos_load_resource_rga(
            301, wps->rating_star_data, sizeof(wps->rating_star_data),
            22, 26, &wps->rating_star);
        wps->chrome_valid &= ipodjs_retailos_load_resource_rga(
            302, wps->rating_dot_data, sizeof(wps->rating_dot_data),
            22, 26, &wps->rating_dot);
        wps->equalizer_valid = ipodjs_retailos_load_animation_raw(
            IPODJS_RETAILOS_NOW_PLAYING_EQUALIZER,
            wps->equalizer_data, sizeof(wps->equalizer_data),
            &wps->equalizer);
        wps->paused_valid = ipodjs_retailos_load_named_rga(
            "now-playing-paused", wps->paused_data,
            sizeof(wps->paused_data), IPODJS_RETAIL_WPS_PAUSED_W,
            IPODJS_RETAIL_WPS_PAUSED_H, &wps->paused);
    }

    if (!idle->tried)
    {
        idle->tried = true;
        idle_valid = true;
        for (digit = 0; digit < 10; digit++)
        {
            idle_valid &= ipodjs_retailos_load_named_rga(
                digit_names[digit], idle->digit_data[digit],
                sizeof(idle->digit_data[digit]), digit_widths[digit],
                IPODJS_RETAIL_IDLE_DIGIT_H, &idle->digits[digit]);
        }
        idle_valid &= ipodjs_retailos_load_named_rga(
            "now-playing-idle-colon", idle->colon_data,
            sizeof(idle->colon_data), IPODJS_RETAIL_IDLE_COLON_W,
            IPODJS_RETAIL_IDLE_DIGIT_H, &idle->colon);
        idle_valid &= ipodjs_retailos_load_animation_rga(
            IPODJS_RETAILOS_NOW_PLAYING_IDLE_BATTERY,
            idle->battery_data, sizeof(idle->battery_data),
            &idle->battery);
        idle_valid &= ipodjs_retailos_load_named_rga(
            "now-playing-idle-lock", idle->lock_data,
            sizeof(idle->lock_data), 25, 38, &idle->lock);
        idle_valid &= ipodjs_retailos_load_named_rga(
            "now-playing-idle-play", idle->play_data,
            sizeof(idle->play_data), 41, 43, &idle->play);
        idle_valid &= ipodjs_retailos_load_named_rga(
            "now-playing-idle-radio", idle->radio_data,
            sizeof(idle->radio_data), 67, 40, &idle->radio);
        idle->valid = idle_valid;
    }

    wps->started = current_tick;
    idle->presented = false;
    idle->last_hour = -1;
    idle->last_minute = -1;
    idle->last_battery_frame = -1;
    idle->last_icon = -1;
    wps_valid = wps->equalizer_valid && wps->paused_valid &&
                wps->chrome_valid;
    return wps_valid && idle->valid;
}

static void ipodjs_ui_draw_retailos_battery(
    struct screen *display, const struct ipodjs_retailos_image *atlas,
    int x, int y)
{
    int level = MAX(0, MIN(100, battery_level()));
    int frame = level * 22 / 100;
    bool connected = charger_inserted();

    /* Frames 23/24 contain only the plug/bolt, not a battery casing.
     * Composite them over the complete level frame at the same origin.
     * Keep the explicitly protected Hold presentation unchanged. */
    if (!connected || !button_hold())
        ipodjs_retailos_blit_part(display, atlas,
            0, frame * IPODJS_RETAIL_BATTERY_H, x, y,
            IPODJS_RETAIL_BATTERY_W, IPODJS_RETAIL_BATTERY_H);
    if (connected)
    {
        frame = !charging_state() && level >= 99 ? 23 : 24;
        ipodjs_retailos_blit_part(display, atlas,
            0, frame * IPODJS_RETAIL_BATTERY_H, x, y,
            IPODJS_RETAIL_BATTERY_W, IPODJS_RETAIL_BATTERY_H);
    }
}

static void ipodjs_ui_draw_retailos_status_glyph(
    struct screen *display, const struct ipodjs_retailos_image *image,
    int x, int y)
{
    if (global_settings.ui_engine_accent == UI_ENGINE_ACCENT_BLUE)
        ipodjs_retailos_blit(display, image, x, y);
    else
        ipodjs_retailos_blit_tint_part(display, image,
            0, 0, x, y, image->width, image->height, ipodjs_ui_accent());
}

bool ipodjs_ui_draw_retailos_music_header(struct screen *display)
{
    struct ipodjs_ui_retail_status_cache *status = &ipodjs_ui_retail_status;
    int playback = audio_status();
    bool dark = global_settings.ui_engine_dark_mode;

    if (!display || !status->valid)
        return false;
    display->set_foreground(LCD_RGBPACK(255, 255, 255));
    display->fillrect(0, 0, LCD_WIDTH, IPODJS_UI_HEADER_HEIGHT);
    ipodjs_retailos_blit(display, dark ? &status->black_background :
                         &status->light_background, 0, 0);
    ipodjs_ui_draw_retailos_battery(display,
                                   dark ? &status->black_battery :
                                          &status->white_battery,
                                   LCD_WIDTH - 31, 3);
    if (playback & AUDIO_STATUS_PLAY)
        ipodjs_ui_draw_playback_indicator(display, LCD_WIDTH - 50, 2);
    return true;
}

void ipodjs_ui_draw_retailos_music_modes(struct screen *display,
                                         bool shuffle, int repeat)
{
    struct ipodjs_ui_retail_status_cache *status = &ipodjs_ui_retail_status;

    if (!display || !status->valid)
        return;
    if (shuffle)
        ipodjs_retailos_blit(display,
            global_settings.ui_engine_dark_mode ? &status->white_shuffle :
                                                  &status->black_shuffle,
            273, 25);
    if (repeat != REPEAT_OFF)
        ipodjs_retailos_blit(display, repeat == REPEAT_ONE ?
            (global_settings.ui_engine_dark_mode ?
                &status->white_repeat_once : &status->black_repeat_once) :
            (global_settings.ui_engine_dark_mode ? &status->white_repeat :
                                                   &status->black_repeat),
            296, 25);
}

bool ipodjs_ui_draw_retailos_music_progress(struct screen *display,
                                            int x, int y, int width,
                                            int percent)
{
    struct ipodjs_ui_retail_wps_cache *wps = &ipodjs_ui_retail_wps;
    int filled = width * MAX(0, MIN(100, percent)) / 100;

    if (!display || !wps->chrome_valid || width <= 0)
        return false;
    /* These source strips include their own reflection. Tile horizontally
     * without scaling, recolouring, or synthesizing highlight rows. */
    for (int dx = 0; dx < width;)
    {
        int layer = dx < filled ? 1 : 0;
        int extent = layer ? filled : width;
        int count = MIN(16, extent - dx);

        if (layer)
        {
            int source_x = dx % wps->progress[layer].width;
            int tile = MIN(count, wps->progress[layer].width - source_x);
            ipodjs_retailos_blit_color_part(display,
                &wps->progress[layer], source_x, 0, x + dx, y,
                tile, 28, ipodjs_ui_accent());
            count = tile;
        }
        else if (global_settings.ui_engine_dark_mode)
        {
            int source_x = dx % wps->progress[layer].width;
            int tile = MIN(count, wps->progress[layer].width - source_x);
            ipodjs_retailos_blit_color_part(display,
                &wps->progress[layer], source_x, 0, x + dx, y,
                tile, 28, LCD_RGBPACK(70, 78, 90));
            count = tile;
        }
        else
            ipodjs_retailos_blit_part(display, &wps->progress[layer],
                0, 0, x + dx, y, count, 28);
        dx += count;
    }
    return true;
}

void ipodjs_ui_draw_retailos_music_rating(struct screen *display,
                                          int x, int y, int stars)
{
    if (!display || !ipodjs_ui_retail_wps.chrome_valid)
        return;
    for (int star = 0; star < MIN(5, MAX(0, stars)); star++)
        ipodjs_retailos_blit(display, &ipodjs_ui_retail_wps.star,
                             x + star * 13, y);
}

void ipodjs_ui_draw_retailos_rating_editor(struct screen *display, int stars)
{
    if (!display || !ipodjs_ui_retail_wps.chrome_valid)
        return;
    stars = MIN(5, MAX(0, stars));
    display->set_foreground(ipodjs_ui_panel());
    display->fillrect(95, 200, 130, 26);
    for (int i = 0; i < 5; i++)
        ipodjs_retailos_blit(display, i < stars ?
            &ipodjs_ui_retail_wps.rating_star : &ipodjs_ui_retail_wps.rating_dot,
            97 + i * 26, 200);
}

struct bitmap *ipodjs_ui_retailos_music_cover(void)
{
    return ipodjs_ui_retail_wps.cover_valid ?
        &ipodjs_ui_retail_wps.cover : NULL;
}

void ipodjs_ui_draw_retailos_scrubber(struct screen *display, int percent)
{
    struct ipodjs_ui_retail_wps_cache *wps = &ipodjs_ui_retail_wps;
    if (!display || !wps->scrub_valid || !wps->chrome_valid)
        return;

    /* Seek mode uses the original empty rail and diamond, not a filled
     * progress bar underneath it. Position the diamond's centre on the
     * playhead, including the two endpoints; retain the source reflection. */
    ipodjs_ui_draw_retailos_music_progress(display, 58, 207, 204, 0);
    ipodjs_retailos_blit(display, &wps->scrub,
        58 + 203 * MAX(0, MIN(100, percent)) / 100 - wps->scrub.width / 2,
        207);
}

void ipodjs_ui_draw_retailos_shuffle_selector(struct screen *display,
                                              bool enabled)
{
    struct ipodjs_ui_retail_controls_cache *cache =
        &ipodjs_ui_retail_controls;
    if (!display || !cache->optionbar_valid)
        return;
    const struct ipodjs_retailos_image *well =
        cache->optionbar[IPODJS_UI_RETAILOS_OPTIONBAR_NOW_PLAYING][0];
    const struct ipodjs_retailos_image *thumb =
        cache->optionbar[IPODJS_UI_RETAILOS_OPTIONBAR_WHITE][1];
    ipodjs_ui_draw_retailos_horizontal_parts(display, well, 72, 200, 176);
    ipodjs_ui_draw_retailos_horizontal_parts(display, thumb,
        72 + (enabled ? 88 : 0), 200, 88);
    if (ipodjs_ui_retail_wps.shuffle_valid)
        ipodjs_retailos_blit(display, &ipodjs_ui_retail_wps.shuffle, 32, 201);
    display->setfont(ipodjs_ui_retailos_font(false));
    display->set_drawmode(DRMODE_FG);
    display->set_foreground(enabled ? LCD_RGBPACK(61, 61, 61) :
                                      LCD_RGBPACK(255, 255, 255));
    ipodjs_ui_puts_fit(display, 72, 205, 88, "Off", true);
    display->set_foreground(enabled ? LCD_RGBPACK(255, 255, 255) :
                                      LCD_RGBPACK(61, 61, 61));
    ipodjs_ui_puts_fit(display, 160, 205, 88, "Songs", true);
    display->set_drawmode(DRMODE_SOLID);
}

bool ipodjs_ui_retailos_now_playing_animation_available(void)
{
    return ipodjs_ui_retail_wps.equalizer_valid;
}

int ipodjs_ui_retailos_now_playing_frame(void)
{
    struct ipodjs_ui_retail_wps_cache *cache = &ipodjs_ui_retail_wps;
    unsigned long elapsed;
    unsigned long cycle_ticks;

    if (!cache->equalizer_valid || cache->equalizer.frame_count == 0)
        return -1;

    elapsed = (unsigned long)(current_tick - cache->started);
    cycle_ticks = (unsigned long)HZ * cache->equalizer.frame_count;
    elapsed %= cycle_ticks;
    return (elapsed * IPODJS_RETAIL_WPS_EQUALIZER_FPS / HZ) %
           cache->equalizer.frame_count;
}

bool ipodjs_ui_draw_retailos_now_playing_activity(
    struct screen *display, int x, int y, int size, bool paused, int frame)
{
    struct ipodjs_ui_retail_wps_cache *cache = &ipodjs_ui_retail_wps;

    if (!display || display != &screens[SCREEN_MAIN])
        return false;
    if (paused)
    {
        if (!cache->paused_valid)
            return false;
        ipodjs_retailos_blit(
            display, &cache->paused,
            x + (size - cache->paused.width) / 2,
            y + (size - cache->paused.height) / 2);
        return true;
    }
    if (!cache->equalizer_valid || frame < 0 ||
        frame >= cache->equalizer.frame_count)
        return false;

    ipodjs_retailos_blit_gray(
        display, &cache->equalizer, frame,
        x + (size - cache->equalizer.width) / 2,
        y + (size - cache->equalizer.height) / 2);
    return true;
}

enum ipodjs_ui_retail_idle_icon
{
    IPODJS_RETAIL_IDLE_PLAY = 0,
    IPODJS_RETAIL_IDLE_LOCK,
    IPODJS_RETAIL_IDLE_RADIO,
};

static const struct ipodjs_retailos_image *
ipodjs_ui_retail_idle_icon(struct ipodjs_ui_retail_idle_cache *cache,
                           int icon)
{
    if (icon == IPODJS_RETAIL_IDLE_LOCK)
        return &cache->lock;
    if (icon == IPODJS_RETAIL_IDLE_RADIO)
        return &cache->radio;
    return &cache->play;
}

bool ipodjs_ui_draw_retailos_playback_idle(struct screen *display)
{
    struct ipodjs_ui_retail_idle_cache *cache = &ipodjs_ui_retail_idle;
    const struct ipodjs_retailos_image *time_images[5];
    const struct ipodjs_retailos_image *icon_image;
    struct viewport *last_vp;
    struct tm *now;
    int status = audio_status();
    int level;
    int battery_frame;
    int icon;
    int hour;
    int minute;
    int count = 0;
    int width = 0;
    int x;
    int index;

    if (!global_settings.ui_engine_playback_screensaver ||
        !display || display != &screens[SCREEN_MAIN] || !cache->valid ||
        display->is_backlight_on(false) ||
        !(status & AUDIO_STATUS_PLAY) || (status & AUDIO_STATUS_PAUSE))
    {
        cache->presented = false;
        return false;
    }

    now = get_time();
    hour = now->tm_hour % 12;
    if (hour == 0)
        hour = 12;
    minute = now->tm_min;
    level = battery_level();
    if (level < 0)
        level = 0;
    battery_frame = (MIN(level, 100) *
                     (IPODJS_RETAIL_IDLE_BATTERY_FRAMES - 1) + 50) / 100;
    if (button_hold())
        icon = IPODJS_RETAIL_IDLE_LOCK;
    else if (get_current_activity() == ACTIVITY_FM)
        icon = IPODJS_RETAIL_IDLE_RADIO;
    else
        icon = IPODJS_RETAIL_IDLE_PLAY;

    if (cache->presented && cache->last_hour == hour &&
        cache->last_minute == minute &&
        cache->last_battery_frame == battery_frame &&
        cache->last_icon == icon)
        return true;

    if (hour >= 10)
        time_images[count++] = &cache->digits[hour / 10];
    time_images[count++] = &cache->digits[hour % 10];
    time_images[count++] = &cache->colon;
    time_images[count++] = &cache->digits[minute / 10];
    time_images[count++] = &cache->digits[minute % 10];
    for (index = 0; index < count; index++)
        width += time_images[index]->width;

    last_vp = display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(LCD_RGBPACK(255, 255, 255));
    display->set_foreground(LCD_RGBPACK(255, 255, 255));
    display->clear_display();

    x = (display->lcdwidth - width) / 2;
    for (index = 0; index < count; index++)
    {
        ipodjs_retailos_blit(display, time_images[index], x,
                             IPODJS_RETAIL_IDLE_TIME_Y);
        x += time_images[index]->width;
    }

    icon_image = ipodjs_ui_retail_idle_icon(cache, icon);
    ipodjs_retailos_blit(
        display, icon_image,
        IPODJS_RETAIL_IDLE_ICON_CX - icon_image->width / 2,
        IPODJS_RETAIL_IDLE_ICON_CY - icon_image->height / 2);
    ipodjs_retailos_blit_part(
        display, &cache->battery, 0,
        battery_frame * IPODJS_RETAIL_IDLE_BATTERY_H,
        IPODJS_RETAIL_IDLE_BATTERY_X, IPODJS_RETAIL_IDLE_BATTERY_Y,
        IPODJS_RETAIL_IDLE_BATTERY_W, IPODJS_RETAIL_IDLE_BATTERY_H);
    display->update();
    display->set_viewport(last_vp);

    cache->presented = true;
    cache->last_hour = hour;
    cache->last_minute = minute;
    cache->last_battery_frame = battery_frame;
    cache->last_icon = icon;
    return true;
}

void ipodjs_ui_retailos_playback_leave(void)
{
    ipodjs_ui_retail_idle.presented = false;
}

static struct bitmap *ipodjs_ui_stock_bluetooth(void)
{
    return ipodjs_ui_load_stock_status(
        IPODJS_UI_APPLE_ASSET_DIR
            "/status-bluetooth.apple.9x14x32.bmp",
        &ipodjs_ui_stock_status.bluetooth,
        ipodjs_ui_stock_status.bluetooth_data,
        sizeof(ipodjs_ui_stock_status.bluetooth_data),
        IPODJS_STOCK_BLUETOOTH_W, IPODJS_STOCK_BLUETOOTH_H,
        &ipodjs_ui_stock_status.bluetooth_tried,
        &ipodjs_ui_stock_status.bluetooth_valid);
}

static struct bitmap *ipodjs_ui_stock_wifi(void)
{
    return ipodjs_ui_load_stock_status(
        IPODJS_UI_APPLE_ASSET_DIR "/status-wifi.apple.18x13x24.bmp",
        &ipodjs_ui_stock_status.wifi,
        ipodjs_ui_stock_status.wifi_data,
        sizeof(ipodjs_ui_stock_status.wifi_data),
        IPODJS_STOCK_WIFI_W, IPODJS_STOCK_WIFI_H,
        &ipodjs_ui_stock_status.wifi_tried,
        &ipodjs_ui_stock_status.wifi_valid);
}

static struct bitmap *ipodjs_ui_airpods(void)
{
    return ipodjs_ui_load_stock_status(
        IPODJS_UI_APPLE_ASSET_DIR
            "/airpods-pro-connected.apple.124x109x24.bmp",
        &ipodjs_ui_stock_status.airpods,
        ipodjs_ui_stock_status.airpods_data,
        sizeof(ipodjs_ui_stock_status.airpods_data),
        IPODJS_AIRPODS_W, IPODJS_AIRPODS_H,
        &ipodjs_ui_stock_status.airpods_tried,
        &ipodjs_ui_stock_status.airpods_valid);
}

bool ipodjs_ui_search_surfaces_available(void)
{
    struct ipodjs_ui_retail_controls_cache *cache =
        &ipodjs_ui_retail_controls;

    if (!cache->tried)
        (void)ipodjs_ui_prepare_retailos_controls();
    return cache->quick_scroll_valid && cache->input_valid &&
           cache->optionbar_valid;
}

bool ipodjs_ui_prepare_search_surfaces(void)
{
    (void)ipodjs_ui_prepare_retailos_controls();
    return ipodjs_ui_search_surfaces_available();
}

static void ipodjs_ui_retailos_blit_tiled_part(
    struct screen *display, const struct ipodjs_retailos_image *image,
    int source_x, int source_y, int source_width, int source_height,
    int x, int y, int width, int height)
{
    int drawn_y = 0;

    while (drawn_y < height)
    {
        int part_height = MIN(source_height, height - drawn_y);
        int drawn_x = 0;

        while (drawn_x < width)
        {
            int part_width = MIN(source_width, width - drawn_x);

            ipodjs_retailos_blit_part(
                display, image, source_x, source_y,
                x + drawn_x, y + drawn_y, part_width, part_height);
            drawn_x += part_width;
        }
        drawn_y += part_height;
    }
}

static bool ipodjs_ui_draw_retailos_nine_slice(
    struct screen *display, const struct ipodjs_retailos_image *image,
    int x, int y, int width, int height, int border)
{
    int center_width;
    int center_height;

    if (!display || !image || !image->pixels || border <= 0 ||
        image->width <= border * 2 || image->height <= border * 2 ||
        width < border * 2 || height < border * 2)
        return false;

    center_width = width - border * 2;
    center_height = height - border * 2;
    ipodjs_retailos_blit_part(display, image, 0, 0,
                              x, y, border, border);
    ipodjs_retailos_blit_part(
        display, image, image->width - border, 0,
        x + width - border, y, border, border);
    ipodjs_retailos_blit_part(
        display, image, 0, image->height - border,
        x, y + height - border, border, border);
    ipodjs_retailos_blit_part(
        display, image, image->width - border, image->height - border,
        x + width - border, y + height - border, border, border);
    ipodjs_ui_retailos_blit_tiled_part(
        display, image, border, 0, image->width - border * 2, border,
        x + border, y, center_width, border);
    ipodjs_ui_retailos_blit_tiled_part(
        display, image, border, image->height - border,
        image->width - border * 2, border,
        x + border, y + height - border, center_width, border);
    ipodjs_ui_retailos_blit_tiled_part(
        display, image, 0, border, border, image->height - border * 2,
        x, y + border, border, center_height);
    ipodjs_ui_retailos_blit_tiled_part(
        display, image, image->width - border, border,
        border, image->height - border * 2,
        x + width - border, y + border, border, center_height);
    ipodjs_ui_retailos_blit_tiled_part(
        display, image, border, border,
        image->width - border * 2, image->height - border * 2,
        x + border, y + border, center_width, center_height);
    return true;
}

static bool ipodjs_ui_draw_retailos_horizontal_parts_cropped(
    struct screen *display, const struct ipodjs_retailos_image parts[3],
    int x, int y, int width, int height)
{
    int draw_height;
    int source_y;
    int target_y;
    int center_width;
    int drawn;

    if (!display || !parts[0].pixels || !parts[1].pixels ||
        !parts[2].pixels || parts[0].height != parts[1].height ||
        parts[0].height != parts[2].height || parts[1].width == 0 ||
        width < parts[0].width + parts[2].width || height <= 0)
        return false;

    draw_height = MIN(height, (int)parts[0].height);
    source_y = (parts[0].height - draw_height) / 2;
    target_y = y + (height - draw_height) / 2;
    ipodjs_retailos_blit_part(display, &parts[0], 0, source_y,
                              x, target_y, parts[0].width, draw_height);
    center_width = width - parts[0].width - parts[2].width;
    drawn = 0;
    while (drawn < center_width)
    {
        int part_width = MIN((int)parts[1].width, center_width - drawn);

        ipodjs_retailos_blit_part(
            display, &parts[1], 0, source_y,
            x + parts[0].width + drawn, target_y,
            part_width, draw_height);
        drawn += part_width;
    }
    ipodjs_retailos_blit_part(
        display, &parts[2], 0, source_y,
        x + width - parts[2].width, target_y,
        parts[2].width, draw_height);
    return true;
}

bool ipodjs_ui_draw_search_surface(struct screen *display,
                                   enum ipodjs_ui_search_surface surface,
                                   int x, int y, int width, int height)
{
    struct ipodjs_ui_retail_controls_cache *cache =
        &ipodjs_ui_retail_controls;

    if (!display || display != &screens[SCREEN_MAIN])
        return false;
    if (surface == IPODJS_UI_SEARCH_PANEL)
        return cache->quick_scroll_valid &&
            ipodjs_ui_draw_retailos_nine_slice(
                display, &cache->quick_scroll[0], x, y,
                width, height, 16);
    if (surface == IPODJS_UI_SEARCH_FIELD)
        return cache->input_valid &&
            ipodjs_ui_draw_retailos_horizontal_parts_cropped(
                display, cache->input, x, y, width, height);
    if (surface == IPODJS_UI_SEARCH_SELECTED)
        return cache->optionbar_valid &&
            ipodjs_ui_draw_retailos_horizontal_parts_cropped(
                display,
                cache->optionbar[IPODJS_UI_RETAILOS_OPTIONBAR_WHITE][1],
                x, y, width, height);
    return false;
}

bool ipodjs_ui_enabled(enum screen_type screen)
{
    return screen == SCREEN_MAIN &&
           global_settings.ui_engine == UI_ENGINE_IPODJS;
}

int ipodjs_ui_row_height(void)
{
    int row_h = global_settings.ui_engine_density == UI_ENGINE_DENSITY_COMPACT ?
        20 : 24;

    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_SMALL)
        row_h -= 2;
    else if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_LARGE)
        row_h += 4;

    return MAX(18, row_h);
}

int ipodjs_ui_font(void)
{
    static int small_font = -2;
    static int normal_font = -2;
    static int large_font = -2;
    int *fontp;
    const char *path;
    const char *fallback_path;

    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_SMALL)
    {
        fontp = &small_font;
        path = IPODJS_UI_ASSET_DIR "/14-Adobe-Helvetica-Bold.fnt";
        fallback_path = FONT_DIR "/14-Adobe-Helvetica-Bold.fnt";
    }
    else if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_LARGE)
    {
        fontp = &large_font;
        path = IPODJS_UI_ASSET_DIR "/18-Adobe-Helvetica-Bold.fnt";
        fallback_path = FONT_DIR "/18-Adobe-Helvetica-Bold.fnt";
    }
    else
    {
        fontp = &normal_font;
        path = IPODJS_UI_ASSET_DIR "/16-Adobe-Helvetica-Bold.fnt";
        fallback_path = FONT_DIR "/16-Adobe-Helvetica-Bold.fnt";
    }

    if (*fontp < 0)
    {
        if (file_exists(path))
        {
            int loaded = font_load(path);
            if (loaded >= 0)
            {
                font_lock(loaded, true);
                *fontp = loaded;
            }
        }

        if (*fontp < 0 && file_exists(fallback_path))
        {
            int loaded = font_load(fallback_path);
            if (loaded >= 0)
            {
                font_lock(loaded, true);
                *fontp = loaded;
            }
        }
    }

    return *fontp >= 0 ? *fontp : FONT_SYSFIXED;
}

static int ipodjs_ui_retail_fonts[5] = { -1, -1, -1, -1, -1 };
static int ipodjs_ui_fast_font = -1;

void ipodjs_ui_prepare_retailos_fonts(void)
{
    static const char * const paths[5] = {
        IPODJS_UI_APPLE_ASSET_DIR
            "/retailos-fonts/15-Helvetica-Bold-RetailOS-Apple.fnt",
        IPODJS_UI_APPLE_ASSET_DIR
            "/retailos-fonts/19-Helvetica-Bold-RetailOS-Apple.fnt",
        IPODJS_UI_APPLE_ASSET_DIR
            "/retailos-fonts/23-Helvetica-Bold-RetailOS-Apple.fnt",
        IPODJS_UI_APPLE_ASSET_DIR
            "/retailos-fonts/16-Helvetica-RetailOS-Apple.fnt",
        IPODJS_UI_APPLE_ASSET_DIR
            "/retailos-fonts/15-Helvetica-RetailOS-Apple.fnt",
    };

    if (audio_status())
        return;
    notification_manager_prepare_visuals();
    /* Preserve the legacy menu font for Hold and the application grid. */
    (void)ipodjs_ui_font();
    for (int face = 0; face < 5; face++)
    {
        if (ipodjs_ui_retail_fonts[face] >= 0)
            continue;
        int id = font_load_ex(paths[face], 0, 96);
        if (id >= 0)
        {
            font_lock(id, true);
            ipodjs_ui_retail_fonts[face] = id;
        }
    }
    /* Preserve Apple's embedded bitmap strike for the quick-scroll plate,
     * but prepare it here rather than allocating on the first fast scroll. */
    if (ipodjs_ui_fast_font < 0)
    {
        int id = font_load_ex(IPODJS_UI_APPLE_ASSET_DIR
            "/23-Helvetica-Apple.fnt", 0, 96);
        if (id >= 0)
        {
            font_lock(id, true);
            ipodjs_ui_fast_font = id;
        }
    }
}

int ipodjs_ui_retailos_font(bool title)
{
    int id = ipodjs_ui_retail_fonts[title ? 1 : 0];
    return id >= 0 ? id : FONT_UI;
}

int ipodjs_ui_tv_font(int size)
{
    int id = ipodjs_ui_retail_fonts[MAX(0, MIN(2, size))];
    return id >= 0 ? id : FONT_UI;
}

int ipodjs_ui_retailos_menu_font(void)
{
    int face = global_settings.ui_engine_font_scale == UI_ENGINE_FONT_SMALL ?
        0 : global_settings.ui_engine_font_scale == UI_ENGINE_FONT_LARGE ? 2 : 1;
    int id = ipodjs_ui_retail_fonts[face];
    return id >= 0 ? id : FONT_UI;
}

int ipodjs_ui_retailos_detail_font(void)
{
    int id = ipodjs_ui_retail_fonts[4];
    return id >= 0 ? id : FONT_UI;
}

int ipodjs_ui_text_y_offset(void)
{
    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_SMALL)
        return -1;
    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_LARGE)
        return 1;
    return 0;
}

unsigned ipodjs_ui_accent(void)
{
    switch (global_settings.ui_engine_accent)
    {
        case UI_ENGINE_ACCENT_GRAPHITE:
            return IPODJS_UI_GRAPHITE;
        case UI_ENGINE_ACCENT_U2:
            return IPODJS_UI_U2_RED;
        case UI_ENGINE_ACCENT_TEAL:
            return IPODJS_UI_TEAL;
        case UI_ENGINE_ACCENT_GREEN:
            return IPODJS_UI_GREEN;
        case UI_ENGINE_ACCENT_GOLD:
            return IPODJS_UI_GOLD;
        case UI_ENGINE_ACCENT_ORANGE:
            return IPODJS_UI_ORANGE;
        case UI_ENGINE_ACCENT_PURPLE:
            return IPODJS_UI_PURPLE;
        case UI_ENGINE_ACCENT_PINK:
            return IPODJS_UI_PINK;
        case UI_ENGINE_ACCENT_BLUE:
        default:
            return IPODJS_UI_ACTIVE_BOTTOM;
    }
}

static bool ipodjs_ui_dark(void)
{
    return global_settings.ui_engine_dark_mode;
}

unsigned ipodjs_ui_screen_bg(void)
{
    if (ipodjs_ui_dark())
    {
        if (global_settings.ui_engine_surface ==
            UI_ENGINE_SURFACE_TRANSPARENT)
            return LCD_RGBPACK(22, 24, 28);
        return LCD_RGBPACK(18, 20, 24);
    }

    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_SOFT)
        return LCD_RGBPACK(246, 247, 249);
    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_TRANSPARENT)
        return LCD_RGBPACK(239, 244, 249);
    return IPODJS_UI_SCREEN_BG;
}

unsigned ipodjs_ui_row_bg(void)
{
    if (ipodjs_ui_dark())
    {
        if (global_settings.ui_engine_surface ==
            UI_ENGINE_SURFACE_TRANSPARENT)
            return LCD_RGBPACK(28, 31, 36);
        if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_SOFT)
            return LCD_RGBPACK(31, 34, 40);
        return LCD_RGBPACK(24, 27, 32);
    }

    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_SOFT)
        return LCD_RGBPACK(250, 251, 252);
    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_TRANSPARENT)
        return LCD_RGBPACK(247, 249, 251);
    return IPODJS_UI_SCREEN_BG;
}

unsigned ipodjs_ui_text(void)
{
    return ipodjs_ui_dark() ? LCD_RGBPACK(239, 242, 246) :
                              IPODJS_UI_TEXT;
}

unsigned ipodjs_ui_muted_text(void)
{
    return ipodjs_ui_dark() ? LCD_RGBPACK(166, 173, 184) :
                              IPODJS_UI_MUTED_TEXT;
}

unsigned ipodjs_ui_header_text(void)
{
    if (ipodjs_ui_retail_status.valid)
        return ipodjs_ui_dark() || button_hold() ?
            LCD_RGBPACK(255, 255, 255) : LCD_RGBPACK(0, 0, 0);
    return ipodjs_ui_dark() ? LCD_RGBPACK(246, 248, 250) :
                              IPODJS_UI_TEXT;
}

unsigned ipodjs_ui_header_bg(void)
{
    if (ipodjs_ui_retail_status.valid)
        return ipodjs_ui_dark() || button_hold() ?
            LCD_RGBPACK(12, 12, 12) : LCD_RGBPACK(255, 255, 255);
    return ipodjs_ui_dark() ? LCD_RGBPACK(24, 29, 38) :
                              IPODJS_UI_HEADER_BOTTOM;
}

unsigned ipodjs_ui_panel(void)
{
    if (ipodjs_ui_dark())
    {
        if (global_settings.ui_engine_surface ==
            UI_ENGINE_SURFACE_TRANSPARENT)
            return LCD_RGBPACK(34, 38, 44);
        if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_SOFT)
            return LCD_RGBPACK(38, 42, 49);
        return LCD_RGBPACK(24, 27, 32);
    }
    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_TRANSPARENT)
        return LCD_RGBPACK(242, 246, 250);
    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_SOFT)
        return LCD_RGBPACK(236, 238, 241);
    return IPODJS_UI_SCREEN_BG;
}

void ipodjs_ui_prepare_native_frame(void)
{
    struct screen *display = &screens[SCREEN_MAIN];
    unsigned top;
    unsigned mid;
    unsigned bottom;

    if (!ipodjs_ui_enabled(SCREEN_MAIN))
        return;

    if (ipodjs_ui_dark())
    {
        top = LCD_RGBPACK(73, 81, 94);
        mid = LCD_RGBPACK(39, 45, 56);
        bottom = LCD_RGBPACK(18, 22, 30);
    }
    else
    {
        top = LCD_RGBPACK(247, 248, 249);
        mid = LCD_RGBPACK(199, 204, 211);
        bottom = LCD_RGBPACK(116, 126, 140);
    }

    display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(ipodjs_ui_screen_bg());
    display->clear_display();
    ipodjs_ui_glass_gradient(display, 0, 0, display->lcdwidth,
                             display->lcdheight, top, mid, bottom);
    display->update();
}

void ipodjs_ui_shutdown_animation(void)
{
    static const unsigned char band_heights[] = {
        216, 180, 132, 82, 38, 10,
    };
    static const unsigned short line_widths[] = {
        240, 144, 72, 28, 8,
    };
    struct screen *display = &screens[SCREEN_MAIN];
    const unsigned black = LCD_RGBPACK(0, 0, 0);
    const unsigned glow = LCD_RGBPACK(176, 205, 220);
    const unsigned white = LCD_RGBPACK(255, 255, 255);
    int center_y = display->lcdheight / 2;

    if (!ipodjs_ui_enabled(SCREEN_MAIN))
        return;

    display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(black);

    /* Close a pair of black shutters over the live iPodJS frame. */
    for (size_t i = 0; i < ARRAYLEN(band_heights); i++)
    {
        int band_h = MIN(display->lcdheight, band_heights[i]);
        int top = (display->lcdheight - band_h) / 2;
        int bottom = top + band_h;

        display->set_foreground(black);
        display->fillrect(0, 0, display->lcdwidth, top);
        display->fillrect(0, bottom, display->lcdwidth,
                          display->lcdheight - bottom);
        display->set_foreground(glow);
        display->hline(0, display->lcdwidth - 1, top);
        display->hline(0, display->lcdwidth - 1, bottom - 1);
        display->update();
        ipodjs_trace_screen("Shutdown CRT", "band", (int)i, 0,
                            ARRAYLEN(band_heights), 0, 0,
                            display->lcdwidth, display->lcdheight);
        sleep(MAX(1, HZ / 30));
    }

    /* Resolve the last strip into the bright line and dot of a CRT. */
    for (size_t i = 0; i < ARRAYLEN(line_widths); i++)
    {
        int width = MIN(display->lcdwidth, line_widths[i]);
        int left = (display->lcdwidth - width) / 2;

        display->set_foreground(black);
        display->fillrect(0, 0, display->lcdwidth, display->lcdheight);
        display->set_foreground(glow);
        display->hline(left, left + width - 1, center_y - 1);
        display->set_foreground(white);
        display->hline(left, left + width - 1, center_y);
        display->set_foreground(glow);
        display->hline(left, left + width - 1, center_y + 1);
        display->update();
        ipodjs_trace_screen("Shutdown CRT", "line", (int)i, 0,
                            ARRAYLEN(line_widths), 0, 0,
                            display->lcdwidth, display->lcdheight);
        sleep(MAX(1, HZ / 30));
    }

    display->set_foreground(white);
    display->fillrect(display->lcdwidth / 2 - 1, center_y - 1, 3, 3);
    display->update();
    ipodjs_trace_screen("Shutdown CRT", "dot", 0, 0, 1, 0, 0,
                        display->lcdwidth, display->lcdheight);
    sleep(MAX(1, HZ / 16));

    display->set_foreground(black);
    display->fillrect(0, 0, display->lcdwidth, display->lcdheight);
    display->update();
    ipodjs_trace_screen("Shutdown CRT", "black", 0, 0, 1, 0, 0,
                        display->lcdwidth, display->lcdheight);
}

unsigned ipodjs_ui_rgb_blend(int br, int bg, int bb,
                             int fr, int fg, int fb,
                             int alpha)
{
    alpha = MAX(0, MIN(alpha, 255));
    return LCD_RGBPACK((br * (255 - alpha) + fr * alpha) / 255,
                       (bg * (255 - alpha) + fg * alpha) / 255,
                       (bb * (255 - alpha) + fb * alpha) / 255);
}

void ipodjs_ui_gradient(struct screen *display, int x, int y, int w, int h,
                        unsigned top, unsigned bottom)
{
    if (!display || h <= 0 || w <= 0)
        return;

#ifdef HAVE_LCD_COLOR
    if (display->screen_type == SCREEN_MAIN)
    {
        lcd_gradient_fillrect(x, y, w, h, top, bottom);
        return;
    }
#endif

    int tr = RGB_UNPACK_RED(top);
    int tg = RGB_UNPACK_GREEN(top);
    int tb = RGB_UNPACK_BLUE(top);
    int br = RGB_UNPACK_RED(bottom);
    int bg = RGB_UNPACK_GREEN(bottom);
    int bb = RGB_UNPACK_BLUE(bottom);
    int denom = MAX(1, h - 1);

    for (int row = 0; row < h; row++)
    {
        int r = (tr * (denom - row) + br * row) / denom;
        int g = (tg * (denom - row) + bg * row) / denom;
        int b = (tb * (denom - row) + bb * row) / denom;
        display->set_foreground(LCD_RGBPACK(r, g, b));
        display->hline(x, x + w - 1, y + row);
    }
}

void ipodjs_ui_glass_gradient(struct screen *display, int x, int y,
                              int w, int h, unsigned top,
                              unsigned mid, unsigned bottom)
{
    int upper;

    if (h <= 1)
    {
        ipodjs_ui_gradient(display, x, y, w, h, top, bottom);
        return;
    }

    upper = MAX(1, (h * 45) / 100);
    ipodjs_ui_gradient(display, x, y, w, upper, top, mid);
    ipodjs_ui_gradient(display, x, y + upper, w, h - upper, mid, bottom);
}

void ipodjs_ui_selection_gradient(struct screen *display, int x, int y,
                                  int w, int h, unsigned *midp)
{
    unsigned accent = ipodjs_ui_accent();
    const struct ipodjs_retailos_image *fill;
    bool recolor = global_settings.ui_engine_accent != UI_ENGINE_ACCENT_BLUE;

    if (!display || !ipodjs_ui_retail_menu.selection_valid ||
        w <= 0 || h <= 0)
        return;

    fill = &ipodjs_ui_retail_menu.selection[1];
    if (!fill->pixels || fill->width != 1 || fill->height == 0)
        return;

    /* List rows are rectangular in RetailOS. Use the genuine one-pixel
     * center gradient from Apple's selector asset across the row; the cap
     * pieces belong to pill controls and round the corners incorrectly here.
     * Scale the complete strip for taller artwork rows instead of clipping
     * the selection to the stock text-row height. */
    int step = h == fill->height ? h : 1;

    for (int row = 0; row < h; row += step)
    {
        int source_y = h > 1 ? row * (fill->height - 1) / (h - 1) : 0;

        for (int offset = 0; offset < w; offset++)
        {
            if (recolor)
                ipodjs_retailos_blit_tint_part(display, fill, 0, source_y,
                    x + offset, y + row, 1, step, accent);
            else
                ipodjs_retailos_blit_part(display, fill, 0, source_y,
                    x + offset, y + row, 1, step);
        }
    }
    if (midp)
        *midp = accent;
}

/* One selected menu label, using Rockbox's existing scrolling scheduler.
 * Static viewport lifetime and explicit owner teardown prevent stale labels
 * from painting over a different screen. No reduced face size or ellipsis. */
static struct viewport ipodjs_ui_menu_scroll_vp;
static int ipodjs_ui_menu_scroll_row_y, ipodjs_ui_menu_scroll_row_h;

void ipodjs_ui_stop_menu_text_scroll(void)
{
    screens[SCREEN_MAIN].scroll_stop_viewport(&ipodjs_ui_menu_scroll_vp);
}

static void ipodjs_ui_menu_scroll_draw(struct scrollinfo *scroll)
{
    if (!scroll->line || button_hold()) return;
    struct screen *display = &screens[SCREEN_MAIN];
    ipodjs_ui_selection_gradient(display, 0, ipodjs_ui_menu_scroll_row_y,
        ipodjs_ui_menu_scroll_vp.width, ipodjs_ui_menu_scroll_row_h, NULL);
    display->set_drawmode(DRMODE_FG);
    display->set_foreground(LCD_RGBPACK(255,255,255));
    display->putsxy(-scroll->offset, 0, scroll->line);
}

void ipodjs_ui_menu_text_scroll(struct screen *display, int x, int y,
    int width, int row_y, int row_height, const char *text)
{
    ipodjs_ui_stop_menu_text_scroll();
    if (!display || display->screen_type != SCREEN_MAIN || button_hold() ||
        !ipodjs_ui_enabled(SCREEN_MAIN) || width <= 0)
        return;
    int pixels, height;
    display->getstringsize(text, &pixels, &height);
    if (pixels <= width) return;
    viewport_set_defaults(&ipodjs_ui_menu_scroll_vp, SCREEN_MAIN);
    ipodjs_ui_menu_scroll_vp.font = lcd_getfont();
    ipodjs_ui_menu_scroll_vp.x = x; ipodjs_ui_menu_scroll_vp.y = y;
    ipodjs_ui_menu_scroll_vp.width = width;
    ipodjs_ui_menu_scroll_vp.height = height;
    ipodjs_ui_menu_scroll_row_y = row_y - y;
    ipodjs_ui_menu_scroll_row_h = row_height;
    struct viewport *saved = display->set_viewport(&ipodjs_ui_menu_scroll_vp);
    ipodjs_ui_selection_gradient(display, 0, row_y - y, width, row_height, NULL);
    display->set_drawmode(DRMODE_FG);
    display->set_foreground(LCD_RGBPACK(255,255,255));
    display->putsxy_scroll_func(0, 0, text, ipodjs_ui_menu_scroll_draw, NULL, 0);
    display->set_viewport(saved);
}

static struct ipodjs_ui_label_cache_entry *ipodjs_ui_label_cache_find(
    const char *text, int font)
{
    int i;

    if (!text)
        return NULL;

    for (i = 0; i < IPODJS_UI_LABEL_CACHE_SIZE; i++)
    {
        if (!ipodjs_ui_label_cache[i].valid)
            continue;
        if (ipodjs_ui_label_cache[i].font != font)
            continue;
        if (!strcmp(ipodjs_ui_label_cache[i].text, text))
            return &ipodjs_ui_label_cache[i];
    }

    return NULL;
}

static struct ipodjs_ui_label_cache_entry *ipodjs_ui_label_cache_get(
    struct screen *display, const char *text)
{
    struct ipodjs_ui_label_cache_entry *entry;
    int font;
    int i;

    if (!display || !text || !text[0] ||
        strlen(text) >= IPODJS_UI_TEXT_SIZE)
        return NULL;

    font = lcd_getfont();
    if (display->screen_type != SCREEN_MAIN)
        font = ipodjs_ui_font();

    entry = ipodjs_ui_label_cache_find(text, font);
    if (entry)
    {
        entry->stamp = ++ipodjs_ui_label_cache_stamp;
        return entry;
    }

    {
        int victim = -1;
        unsigned long oldest = ULONG_MAX;

        for (i = 0; i < IPODJS_UI_LABEL_CACHE_SIZE; i++)
        {
            if (!ipodjs_ui_label_cache[i].valid)
            {
                victim = i;
                break;
            }

            if (ipodjs_ui_label_cache[i].stamp < oldest)
            {
                oldest = ipodjs_ui_label_cache[i].stamp;
                victim = i;
            }
        }

        entry = &ipodjs_ui_label_cache[victim];
        entry->valid = true;
        entry->stamp = ++ipodjs_ui_label_cache_stamp;
        entry->font = font;
        strmemccpy(entry->text, text, sizeof(entry->text));
        display->getstringsize((const unsigned char *)entry->text,
                               &entry->width, NULL);
        entry->fit_width = -1;
        entry->fit_pixels = -1;
        entry->fit[0] = '\0';
    }

    return entry;
}

static int ipodjs_ui_utf8_prefix_bytes(const char *text, int chars,
                                       size_t max_bytes)
{
    int bytes;

    if (!text || chars <= 0 || max_bytes == 0)
        return 0;

    bytes = utf8seek((const unsigned char *)text, chars);
    while (bytes > (int)max_bytes && chars > 0)
        bytes = utf8seek((const unsigned char *)text, --chars);
    return MAX(0, bytes);
}

static void ipodjs_ui_fit_text(struct screen *display, const char *text,
                               int width, char *buf, size_t buf_size,
                               int *pixel_width)
{
    static const char ellipsis[] = "...";
    int chars = utf8length((const unsigned char *)text);
    int low = 0;
    int high = chars;
    int best = 0;
    int bytes;
    int w = 0;
    int ellipsis_w = 0;
    bool clipped_by_buffer;

    bytes = ipodjs_ui_utf8_prefix_bytes(text, chars, buf_size - 1);
    memcpy(buf, text, bytes);
    buf[bytes] = '\0';
    clipped_by_buffer = text[bytes] != '\0';
    display->getstringsize((const unsigned char *)buf, &w, NULL);
    if (!clipped_by_buffer && w <= width)
    {
        *pixel_width = w;
        return;
    }

    display->getstringsize((const unsigned char *)ellipsis,
                           &ellipsis_w, NULL);
    width = MAX(0, width - ellipsis_w);
    while (low <= high)
    {
        int mid = low + (high - low) / 2;
        int candidate_w;

        bytes = ipodjs_ui_utf8_prefix_bytes(text, mid, buf_size - 4);
        memcpy(buf, text, bytes);
        buf[bytes] = '\0';
        display->getstringsize((const unsigned char *)buf,
                               &candidate_w, NULL);
        if (candidate_w <= width)
        {
            best = mid;
            low = mid + 1;
        }
        else
            high = mid - 1;
    }

    bytes = ipodjs_ui_utf8_prefix_bytes(text, best, buf_size - 4);
    memcpy(buf, text, bytes);
    memcpy(buf + bytes, ellipsis, sizeof(ellipsis));
    display->getstringsize((const unsigned char *)buf, &w, NULL);
    *pixel_width = w;
}

void ipodjs_ui_puts_fit(struct screen *display, int x, int y, int width,
                        const char *text, bool center)
{
    char buf[IPODJS_UI_TEXT_SIZE];
    int w = 0;
    struct ipodjs_ui_label_cache_entry *entry;

    if (!display || !text || !text[0] || width <= 0)
        return;

    entry = ipodjs_ui_label_cache_get(display, text);
    if (!entry)
        ipodjs_ui_fit_text(display, text, width, buf, sizeof(buf), &w);
    else if (entry->fit_width == width)
    {
        strmemccpy(buf, entry->fit, sizeof(buf));
        w = entry->fit_pixels;
    }
    else
    {
        ipodjs_ui_fit_text(display, entry->text, width, buf, sizeof(buf), &w);

        entry->fit_width = width;
        entry->fit_pixels = w;
        strmemccpy(entry->fit, buf, sizeof(entry->fit));
    }

    if (center && w < width)
        x += (width - w) / 2;

    display->set_drawmode(DRMODE_FG);
    display->putsxy(x, y, (const unsigned char *)buf);
    display->set_drawmode(DRMODE_SOLID);
}

void ipodjs_ui_draw_arrow(struct screen *display, int x, int y,
                          unsigned color)
{
    if (!display)
        return;

    if (ipodjs_ui_retail_controls.submenu_valid && !button_hold() &&
        color == LCD_RGBPACK(255, 255, 255))
    {
        ipodjs_retailos_blit(display, &ipodjs_ui_retail_controls.submenu,
                             x, y - 4);
        return;
    }
    display->set_foreground(color);
    display->fillrect(x, y + 1, 2, 1);
    display->fillrect(x + 2, y + 2, 2, 1);
    display->fillrect(x + 4, y + 3, 2, 1);
    display->fillrect(x + 2, y + 4, 2, 1);
    display->fillrect(x, y + 5, 2, 1);
}

void ipodjs_ui_draw_header_background(struct screen *display, int width)
{
    const struct ipodjs_retailos_image *header;

    if (!display)
        return;
    width = MAX(0, MIN(width, display->lcdwidth));
    header = global_settings.ui_engine_dark_mode ?
        &ipodjs_ui_retail_status.black_background :
        button_hold() ? &ipodjs_ui_retail_status.hold_background :
                        &ipodjs_ui_retail_status.light_background;
    if (ipodjs_ui_retail_status.valid)
    {
        display->set_foreground(global_settings.ui_engine_dark_mode ?
            LCD_RGBPACK(0, 0, 0) : LCD_RGBPACK(255, 255, 255));
        /* Light RetailOS headers are 20px; the Hold/dark headers are 24px.
         * Clearing 24 rows behind a 20px asset erases the body's first four
         * rows on full-screen About and Applications. */
        display->fillrect(0, 0, width,
            MIN(header->height, IPODJS_UI_HEADER_HEIGHT));
        ipodjs_retailos_blit_part(display, header, 0, 0, 0, 0,
            width, MIN(header->height, IPODJS_UI_HEADER_HEIGHT));
        return;
    }

    ipodjs_ui_glass_gradient(display, 0, 0, width,
        global_settings.ui_engine_dark_mode || button_hold() ?
            IPODJS_UI_HEADER_HEIGHT : IPODJS_UI_RETAIL_MENU_HEADER_HEIGHT,
        global_settings.ui_engine_dark_mode ? LCD_RGBPACK(92, 98, 108) :
                                              IPODJS_UI_HEADER_TOP,
        global_settings.ui_engine_dark_mode ? LCD_RGBPACK(50, 56, 66) :
                                              IPODJS_UI_HEADER_MID,
        ipodjs_ui_header_bg());
}

void ipodjs_ui_draw_playback_indicator(struct screen *display, int x, int y)
{
    int status;
    bool paused;
    const struct ipodjs_retailos_image *image;

    if (!display)
        return;

    status = audio_status();
    if (!(status & AUDIO_STATUS_PLAY))
        return;

    paused = (status & AUDIO_STATUS_PAUSE) != 0;
    if (!ipodjs_ui_retail_status.valid)
        return;

    if (!paused)
    {
        ipodjs_ui_draw_retailos_status_glyph(display,
            &ipodjs_ui_retail_status.status_play, x, y);
        return;
    }

    /* The light-bar retail pause sprite contains Apple's shaded blue ink.
     * Keep those pixels for the default theme and tint their luminance for
     * custom accents, just like the retail play sprite. The white Hold
     * sprite is a separate translucent asset, not a recoloring mask. */
    if (button_hold())
        image = &ipodjs_ui_retail_status.white_pause;
    else
        image = &ipodjs_ui_retail_status.black_pause;
    if (button_hold())
        ipodjs_retailos_blit(display, image, x, y);
    else
        ipodjs_ui_draw_retailos_status_glyph(display, image, x, y);
}

void ipodjs_ui_draw_hold_indicator(struct screen *display, int x, int y)
{
    const struct ipodjs_retailos_image *image;

    if (!display || !button_hold())
        return;
    if (!ipodjs_ui_retail_status.valid)
        return;
    image = global_settings.ui_engine_dark_mode ?
        &ipodjs_ui_retail_status.black_lock :
        &ipodjs_ui_retail_status.white_lock;
    ipodjs_retailos_blit(display, image, x, y);
}

void ipodjs_ui_draw_repeat_indicator(struct screen *display, int x, int y,
                                     int repeat_mode)
{
    const struct ipodjs_retailos_image *image;

    if (!display || repeat_mode == REPEAT_OFF)
        return;
    if (!ipodjs_ui_retail_status.valid)
        return;
    if (global_settings.ui_engine_dark_mode || !button_hold())
        image = repeat_mode == REPEAT_ONE ?
            &ipodjs_ui_retail_status.black_repeat_once :
            &ipodjs_ui_retail_status.black_repeat;
    else
        image = repeat_mode == REPEAT_ONE ?
            &ipodjs_ui_retail_status.white_repeat_once :
            &ipodjs_ui_retail_status.white_repeat;
    ipodjs_retailos_blit(display, image, x, y);
}

void ipodjs_ui_draw_shuffle_indicator(struct screen *display, int x, int y)
{
    const struct ipodjs_retailos_image *image;

    if (!display)
        return;
    if (!ipodjs_ui_retail_status.valid)
        return;
    image = global_settings.ui_engine_dark_mode || !button_hold() ?
        &ipodjs_ui_retail_status.black_shuffle :
        &ipodjs_ui_retail_status.white_shuffle;
    ipodjs_retailos_blit(display, image, x, y);
}

void ipodjs_ui_prepare_bluetooth_indicator(void)
{
    /* Screen-entry service point: the draw path below remains I/O-free. */
    ipodjs_ui_prepare_retailos_status();
    ipodjs_ui_prepare_retailos_controls();
    ipodjs_ui_stock_bluetooth();
    ipodjs_ui_stock_wifi();
    ipodjs_ui_airpods();
}

void ipodjs_ui_draw_wifi_indicator(struct screen *display, int x, int y)
{
    if (!display || !ipodjs_ui_stock_status.wifi_valid)
        return;

    display->transparent_bitmap(ipodjs_ui_stock_status.wifi.data, x, y,
                                ipodjs_ui_stock_status.wifi.width,
                                ipodjs_ui_stock_status.wifi.height);
}

void ipodjs_ui_draw_bluetooth_indicator(struct screen *display, int x, int y)
{
    if (!display || !ipodjs_ui_stock_status.bluetooth_valid)
        return;

    /* Blend alpha over the painted header, not the solid background color. */
    int old_mode = (*display->current_viewport)->drawmode;
    display->set_drawmode(DRMODE_FG);
    display->bmp(&ipodjs_ui_stock_status.bluetooth, x, y);
    display->set_drawmode(old_mode);
}

static void ipodjs_ui_fill_connection_sheet(struct screen *display,
                                            int x, int y, int w, int h,
                                            unsigned color)
{
    static const unsigned char inset[] = { 6, 4, 2, 1, 1, 0 };

    display->set_foreground(color);
    display->fillrect(x, y + 6, w, h - 12);
    display->fillrect(x + 6, y, w - 12, h);
    for (int row = 0; row < 6; row++)
    {
        int edge = inset[row];

        display->hline(x + edge, x + w - edge - 1, y + row);
        display->hline(x + edge, x + w - edge - 1,
                       y + h - row - 1);
    }
}

static void ipodjs_ui_draw_connection_spinner(struct screen *display,
                                               int x, int y, int phase)
{
    static const signed char points[8][2] = {
        { 0, -7 }, { 5, -5 }, { 7, 0 }, { 5, 5 },
        { 0, 7 }, { -5, 5 }, { -7, 0 }, { -5, -5 },
    };

    for (int index = 0; index < 8; index++)
    {
        int distance = (index - phase + 8) & 7;
        unsigned accent = ipodjs_ui_accent();
        unsigned color = ipodjs_ui_rgb_blend(250, 250, 252,
            RGB_UNPACK_RED(accent), RGB_UNPACK_GREEN(accent),
            RGB_UNPACK_BLUE(accent),
            distance == 0 ? 255 : distance <= 2 ? 150 : 50);

        display->set_foreground(color);
        display->fillrect(x + points[index][0] - 1,
                          y + points[index][1] - 1, 3, 3);
    }
}

static void ipodjs_ui_draw_connection_check(struct screen *display,
                                             int x, int y)
{
    display->set_foreground(ipodjs_ui_accent());
    display->fillrect(x - 9, y - 9, 19, 19);
    display->set_foreground(LCD_RGBPACK(255, 255, 255));
    display->hline(x - 5, x - 2, y);
    display->hline(x - 3, x, y + 2);
    display->hline(x, x + 6, y - 4);
    display->drawpixel(x - 4, y + 1);
    display->drawpixel(x - 1, y + 1);
    display->drawpixel(x + 1, y - 1);
    display->drawpixel(x + 2, y - 2);
}

static int ipodjs_ui_connection_smoothstep(long elapsed, long duration)
{
    const int scale = 256;
    int progress;

    if (elapsed <= 0)
        return 0;
    if (elapsed >= duration)
        return scale;
    progress = elapsed * scale / duration;
    return progress * progress * (3 * scale - 2 * progress) /
           (scale * scale);
}

void ipodjs_ui_airpods_connected_animation(void)
{
    struct screen *display = &screens[SCREEN_MAIN];
    const long enter_ticks = MAX(1, 2 * HZ / 5);
    const long hold_ticks = MAX(1, 3 * HZ / 2);
    const long exit_ticks = MAX(1, 7 * HZ / 10);
    const long connected_ticks = MAX(1, 2 * HZ / 5);
    const int resting_y = 8;
    const int sheet_x = 8;
    const int sheet_w = LCD_WIDTH - 16;
    const int sheet_h = LCD_HEIGHT - 16;
    const int image_x = (LCD_WIDTH - IPODJS_AIRPODS_W) / 2;
    bool restore_home = true;
    long started;

    if (!ipodjs_ui_stock_status.airpods_valid)
        return;

    backlight_on();
    display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    /* Reuse the fixed transition pair while this synchronous animation owns
     * the display.  Keep the original Home frame for a clean handoff and a
     * dimmed copy for restoring pixels exposed by the moving sheet. */
    ipodjs_ui_transition_cancel();
    memcpy(ipodjs_ui_animation_old, FBADDR(0, 0), FRAMEBUFFER_SIZE);
    for (size_t pixel = 0;
         pixel < FRAMEBUFFER_SIZE / sizeof(fb_data); pixel++)
    {
        fb_data source = ipodjs_ui_animation_old[pixel];
        unsigned red = FB_UNPACK_RED(source) * 5 / 8;
        unsigned green = FB_UNPACK_GREEN(source) * 5 / 8;
        unsigned blue = FB_UNPACK_BLUE(source) * 5 / 8;

        ipodjs_ui_animation_new[pixel] =
            FB_RGBPACK(red, green, blue);
    }
#endif
    started = current_tick;

    while (true)
    {
        long elapsed = current_tick - started;
        long exit_elapsed = elapsed - enter_ticks - hold_ticks;
        int sheet_y;
        int action;
        bool connected = elapsed >= enter_ticks + connected_ticks;
        bool finished = exit_elapsed >= exit_ticks;

        if (elapsed < enter_ticks)
        {
            sheet_y = LCD_HEIGHT -
                (LCD_HEIGHT - resting_y) * elapsed / enter_ticks;
        }
        else if (exit_elapsed <= 0)
            sheet_y = resting_y;
        else
        {
            int eased = ipodjs_ui_connection_smoothstep(
                exit_elapsed, exit_ticks);

            sheet_y = resting_y +
                (LCD_HEIGHT - resting_y) * eased / 256;
        }

#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
        memcpy(FBADDR(0, 0), ipodjs_ui_animation_new, FRAMEBUFFER_SIZE);
#else
        display->set_background(LCD_RGBPACK(30, 33, 38));
        display->clear_display();
#endif
        ipodjs_ui_fill_connection_sheet(display, sheet_x, sheet_y,
                                        sheet_w, sheet_h,
                                        LCD_RGBPACK(250, 250, 252));

        display->set_foreground(LCD_RGBPACK(196, 198, 203));
        display->fillrect((LCD_WIDTH - 34) / 2, sheet_y + 5, 34, 3);
        display->bmp(&ipodjs_ui_stock_status.airpods,
                     image_x, sheet_y + 14);

        display->setfont(FONT_UI);
        display->set_background(LCD_RGBPACK(250, 250, 252));
        display->set_foreground(LCD_RGBPACK(18, 18, 20));
        ipodjs_ui_puts_fit(display, 24, sheet_y + 127,
                          LCD_WIDTH - 48, "AirPods Pro", true);
        display->set_foreground(connected ?
            ipodjs_ui_accent() : LCD_RGBPACK(105, 107, 112));
        ipodjs_ui_puts_fit(display, 24, sheet_y + 151,
                          LCD_WIDTH - 48,
                          connected ? "Connected" : "Connecting...", true);

        if (connected)
            ipodjs_ui_draw_connection_check(display, LCD_WIDTH / 2,
                                            sheet_y + 190);
        else
            ipodjs_ui_draw_connection_spinner(display, LCD_WIDTH / 2,
                                              sheet_y + 190,
                                              (elapsed *
                                               IPODJS_AIRPODS_ANIMATION_FPS /
                                               HZ) & 7);

        display->update();
        if (finished)
            break;
        action = get_action(CONTEXT_STD | ALLOW_SOFTLOCK,
                            MAX(1, HZ / IPODJS_AIRPODS_ANIMATION_FPS));
        if (action != ACTION_NONE)
        {
            if (IS_SYSEVENT(action))
            {
                default_event_handler(action);
                restore_home = false;
            }
            break;
        }
    }

#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    if (restore_home)
    {
        memcpy(FBADDR(0, 0), ipodjs_ui_animation_old, FRAMEBUFFER_SIZE);
        display->update();
    }
#else
    (void)restore_home;
#endif
}

void ipodjs_ui_draw_header_battery(struct screen *display, int x, int y)
{
    const int inner_w = 22;
    const int inner_h = 10;
    int level;
    int draw_level;
    int fill_w;
    bool charging;
    unsigned fill;
    unsigned shine;
    unsigned shade;

    const struct ipodjs_retailos_image *atlas;

    if (!display)
        return;

    level = MAX(0, MIN(100, battery_level()));
    if (ipodjs_ui_retail_status.valid)
    {
        atlas = global_settings.ui_engine_dark_mode ?
            &ipodjs_ui_retail_status.black_battery :
            &ipodjs_ui_retail_status.white_battery;
        ipodjs_ui_draw_retailos_battery(display, atlas, x, y);
        return;
    }

    /* Unbranded Rockbox fallback.  It is never presented as Apple artwork;
     * personal Classic packages use the complete official 25-state atlas. */
    draw_level = MAX(15, level);
    fill_w = inner_w * draw_level / 100;
    charging = charger_inserted();
    fill = (level > 20 || charging) ?
        LCD_RGBPACK(165, 224, 127) : LCD_RGBPACK(209, 127, 107);
    shine = ipodjs_ui_rgb_blend(
        RGB_UNPACK_RED(fill), RGB_UNPACK_GREEN(fill),
        RGB_UNPACK_BLUE(fill), 255, 255, 255, 120);
    shade = ipodjs_ui_rgb_blend(
        RGB_UNPACK_RED(fill), RGB_UNPACK_GREEN(fill),
        RGB_UNPACK_BLUE(fill), 0, 0, 0, 82);

    display->set_foreground(LCD_RGBPACK(84, 88, 91));
    display->fillrect(x + 1, y + 1, inner_w, inner_h);
    ipodjs_ui_gradient(display, x + 1, y + 1, inner_w, inner_h,
                       LCD_RGBPACK(84, 88, 91),
                       LCD_RGBPACK(126, 130, 133));
    display->set_foreground(fill);
    display->fillrect(x + 1, y + 1, fill_w, inner_h);
    if (fill_w > 0)
    {
        display->set_foreground(shine);
        display->hline(x + 1, x + fill_w, y + 2);
        display->hline(x + 1, x + fill_w, y + 3);
        display->set_foreground(shade);
        display->hline(x + 1, x + fill_w, y + 8);
        display->hline(x + 1, x + fill_w, y + 9);
        display->hline(x + 1, x + fill_w, y + 10);
    }

    display->set_foreground(LCD_RGBPACK(98, 98, 98));
    display->drawrect(x, y, 24, 12);
    display->set_foreground(LCD_RGBPACK(196, 196, 196));
    display->fillrect(x + 24, y + 3, 3, 6);
}

static int ipodjs_ui_fast_scroll_font(void)
{
    return ipodjs_ui_fast_font >= 0 ? ipodjs_ui_fast_font : FONT_UI;
}

bool ipodjs_ui_fast_scroll_available(void)
{
    struct ipodjs_ui_retail_controls_cache *cache =
        &ipodjs_ui_retail_controls;

    /* Never expose a text-only or recreated approximation.  Both plates are
     * the exact 74x70 iPod35 RetailOS resources.  The font remains Apple's
     * direct embedded-bitmap strike and is not rasterized from an outline. */
    return cache->quick_scroll_valid && ipodjs_ui_fast_font >= 0;
}

static int ipodjs_ui_latin_bucket(ucschar_t ch)
{
    if (ch >= 'a' && ch <= 'z')
        return ch - 'a' + 1;
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 1;

    /* Unicode Latin-1 and Latin Extended-A collation groups.  The stock
     * overlay remains the verified Apple A-Z atlas; no substitute glyph is
     * synthesized for scripts the atlas does not contain. */
    if ((ch >= 0x00c0 && ch <= 0x00c6) ||
        (ch >= 0x00e0 && ch <= 0x00e6) ||
        (ch >= 0x0100 && ch <= 0x0105)) return 1;  /* A */
    if (ch == 0x00c7 || ch == 0x00e7 ||
        (ch >= 0x0106 && ch <= 0x010d)) return 3;  /* C */
    if ((ch >= 0x010e && ch <= 0x0111)) return 4;  /* D */
    if ((ch >= 0x00c8 && ch <= 0x00cb) ||
        (ch >= 0x00e8 && ch <= 0x00eb) ||
        (ch >= 0x0112 && ch <= 0x011b)) return 5;  /* E */
    if (ch >= 0x011c && ch <= 0x0123) return 7;    /* G */
    if (ch >= 0x0124 && ch <= 0x0127) return 8;    /* H */
    if ((ch >= 0x00cc && ch <= 0x00cf) ||
        (ch >= 0x00ec && ch <= 0x00ef) ||
        (ch >= 0x0128 && ch <= 0x0133)) return 9;  /* I */
    if (ch == 0x0134 || ch == 0x0135) return 10;   /* J */
    if (ch >= 0x0136 && ch <= 0x0138) return 11;   /* K */
    if (ch >= 0x0139 && ch <= 0x0142) return 12;   /* L */
    if (ch == 0x00d1 || ch == 0x00f1 ||
        (ch >= 0x0143 && ch <= 0x014b)) return 14; /* N */
    if ((ch >= 0x00d2 && ch <= 0x00d6) || ch == 0x00d8 ||
        (ch >= 0x00f2 && ch <= 0x00f6) || ch == 0x00f8 ||
        (ch >= 0x014c && ch <= 0x0153)) return 15; /* O */
    if (ch >= 0x0154 && ch <= 0x0159) return 18;   /* R */
    if (ch >= 0x015a && ch <= 0x0161) return 19;   /* S */
    if (ch >= 0x0162 && ch <= 0x0167) return 20;   /* T */
    if ((ch >= 0x00d9 && ch <= 0x00dc) ||
        (ch >= 0x00f9 && ch <= 0x00fc) ||
        (ch >= 0x0168 && ch <= 0x0173)) return 21; /* U */
    if (ch >= 0x0174 && ch <= 0x0175) return 23;   /* W */
    if (ch == 0x00dd || ch == 0x00fd || ch == 0x00ff ||
        (ch >= 0x0176 && ch <= 0x0178)) return 25; /* Y */
    if (ch >= 0x0179 && ch <= 0x017e) return 26;   /* Z */
    return -1;
}

int ipodjs_ui_fast_scroll_bucket(const char *text)
{
    const unsigned char *name = (const unsigned char *)text;
    ucschar_t ch = 0;

    if (!name)
        return 0;
    while (*name == '\t' || *name == ' ')
        name++;
    if (!*name)
        return 0;
    if (*name < 0x80)
    {
        int bucket = ipodjs_ui_latin_bucket(*name);
        return bucket >= 0 ? bucket : 0;
    }
    utf8decode(name, &ch);
    return ipodjs_ui_latin_bucket(ch);
}

void ipodjs_ui_fast_scroll_show(const char *label)
{
    if (!ipodjs_ui_fast_scroll_available())
    {
        ipodjs_ui_fast_scroll_visible = false;
        return;
    }
    strmemccpy(ipodjs_ui_fast_scroll_label, label ? label : "#",
               sizeof(ipodjs_ui_fast_scroll_label));
    ipodjs_ui_fast_scroll_visible = true;
    ipodjs_ui_fast_scroll_deadline = current_tick + HZ;
    ipodjs_trace_fast_scroll(ipodjs_ui_fast_scroll_label, "show");
}

void ipodjs_ui_fast_scroll_clear(void)
{
    if (ipodjs_ui_fast_scroll_visible)
        ipodjs_trace_fast_scroll(ipodjs_ui_fast_scroll_label, "clear");
    ipodjs_ui_fast_scroll_visible = false;
}

bool ipodjs_ui_fast_scroll_active(void)
{
    return ipodjs_ui_fast_scroll_visible;
}

bool ipodjs_ui_fast_scroll_take_expired(void)
{
    if (ipodjs_ui_fast_scroll_visible &&
        (TIME_AFTER(current_tick, ipodjs_ui_fast_scroll_deadline)
#ifdef HAVE_WHEEL_POSITION
         /* Drain already queued wheel detents before observing finger lift. */
         || (button_queue_empty() && wheel_status() < 0)
#endif
        ))
    {
        ipodjs_trace_fast_scroll(ipodjs_ui_fast_scroll_label, "expired");
        ipodjs_ui_fast_scroll_visible = false;
        return true;
    }
    return false;
}

void ipodjs_ui_draw_fast_scroll(struct screen *display)
{
    struct ipodjs_ui_retail_controls_cache *cache =
        &ipodjs_ui_retail_controls;
    const struct ipodjs_retailos_image *overlay;
    int font;
    int width;
    int height;

    if (!display || !ipodjs_ui_fast_scroll_visible)
        return;

    if (!cache->quick_scroll_valid)
        return;
    overlay = &cache->quick_scroll[
        ipodjs_ui_fast_scroll_label[0] == '#' ? 1 : 0];
    ipodjs_retailos_blit(
        display, overlay, (display->lcdwidth - overlay->width) / 2,
        IPODJS_UI_HEADER_HEIGHT +
        (display->lcdheight - IPODJS_UI_HEADER_HEIGHT -
         overlay->height) / 2);

    if (ipodjs_ui_fast_scroll_label[0] == '#')
    {
        display->set_drawmode(DRMODE_SOLID);
        return;
    }

    font = ipodjs_ui_fast_scroll_font();
    display->setfont(font);
    display->getstringsize((const unsigned char *)ipodjs_ui_fast_scroll_label,
                           &width, &height);
    display->set_foreground(LCD_RGBPACK(255, 255, 255));
    display->set_drawmode(DRMODE_FG);
    display->putsxy((display->lcdwidth - width) / 2,
                    IPODJS_UI_HEADER_HEIGHT +
                    (display->lcdheight - IPODJS_UI_HEADER_HEIGHT - height) / 2,
                    (const unsigned char *)ipodjs_ui_fast_scroll_label);
    display->set_drawmode(DRMODE_SOLID);
}

struct ipodjs_ui_point {
    int x;
    int y;
};

static int ipodjs_ui_charge_font(void)
{
    return ipodjs_ui_retailos_font(false);
}

static void ipodjs_ui_fill_polygon(struct screen *display,
                                   const struct ipodjs_ui_point *points,
                                   int count, int offset_x, int offset_y,
                                   unsigned color)
{
    int intersections[8];
    int min_y = points[0].y;
    int max_y = points[0].y;
    int y;
    int i;

    for (i = 1; i < count; i++)
    {
        min_y = MIN(min_y, points[i].y);
        max_y = MAX(max_y, points[i].y);
    }

    display->set_foreground(color);
    for (y = min_y; y <= max_y; y++)
    {
        int found = 0;
        int edge;

        for (edge = 0; edge < count; edge++)
        {
            const struct ipodjs_ui_point *a = &points[edge];
            const struct ipodjs_ui_point *b = &points[(edge + 1) % count];
            int low_y = MIN(a->y, b->y);
            int high_y = MAX(a->y, b->y);

            if (a->y == b->y || y < low_y || y >= high_y)
                continue;
            if (found < (int)ARRAYLEN(intersections))
            {
                intersections[found++] = a->x +
                    (y - a->y) * (b->x - a->x) / (b->y - a->y);
            }
        }

        for (i = 1; i < found; i++)
        {
            int value = intersections[i];
            int j = i - 1;

            while (j >= 0 && intersections[j] > value)
            {
                intersections[j + 1] = intersections[j];
                j--;
            }
            intersections[j + 1] = value;
        }

        for (i = 0; i + 1 < found; i += 2)
        {
            display->hline(offset_x + intersections[i],
                           offset_x + intersections[i + 1],
                           offset_y + y);
        }
    }
}

static unsigned ipodjs_ui_charge_mix(unsigned from, unsigned to,
                                     int amount, int total)
{
    int alpha = total > 0 ? (amount * 255) / total : 255;

    return ipodjs_ui_rgb_blend(RGB_UNPACK_RED(from),
                               RGB_UNPACK_GREEN(from),
                               RGB_UNPACK_BLUE(from),
                               RGB_UNPACK_RED(to),
                               RGB_UNPACK_GREEN(to),
                               RGB_UNPACK_BLUE(to), alpha);
}

static int ipodjs_ui_charge_round_inset(int row, int height, int radius)
{
    int edge_row;
    int dy;
    int dx = 0;

    radius = MIN(radius, height / 2);
    if (radius <= 0 || (row >= radius && row < height - radius))
        return 0;

    edge_row = row < radius ? row : height - 1 - row;
    dy = radius - 1 - edge_row;
    while ((dx + 1) * (dx + 1) + dy * dy <= radius * radius)
        dx++;

    return MAX(0, radius - 1 - dx);
}

static unsigned ipodjs_ui_charge_gradient_color(unsigned top, unsigned middle,
                                                 unsigned bottom, int row,
                                                 int height)
{
    int split = MAX(1, (height * 42) / 100);

    if (row < split)
        return ipodjs_ui_charge_mix(top, middle, row, MAX(1, split - 1));
    return ipodjs_ui_charge_mix(middle, bottom, row - split,
                                MAX(1, height - split - 1));
}

static void ipodjs_ui_charge_rounded_gradient(struct screen *display,
                                              int x, int y, int width,
                                              int height, int radius,
                                              unsigned top, unsigned middle,
                                              unsigned bottom, int clip_x,
                                              int clip_width)
{
    int clip_right = clip_x + clip_width - 1;
    int row;

    if (width <= 0 || height <= 0 || clip_width <= 0)
        return;

    for (row = 0; row < height; row++)
    {
        int inset = ipodjs_ui_charge_round_inset(row, height, radius);
        int left = MAX(x + inset, clip_x);
        int right = MIN(x + width - 1 - inset, clip_right);

        if (left > right)
            continue;
        display->set_foreground(ipodjs_ui_charge_gradient_color(
            top, middle, bottom, row, height));
        display->hline(left, right, y + row);
    }
}

static int ipodjs_ui_usb_small_font(void)
{
    int id = ipodjs_ui_retail_fonts[3];
    return id >= 0 ? id : FONT_UI;
}

void ipodjs_ui_usb_prepare(void)
{
    ipodjs_ui_prepare_retailos_fonts();
    int normal = ipodjs_ui_usb_small_font();
    int bold = ipodjs_ui_retailos_font(true);

    ipodjs_ui_prepare_retailos_usb();
    /* Warm every fixed label before USB closes font descriptors. */
    font_getstringsize("iPod", NULL, NULL, ipodjs_ui_retailos_font(false));
    font_getstringsize("USB", NULL, NULL, ipodjs_ui_retailos_font(false));
    font_getstringsize("Connected", NULL, NULL, bold);
    font_getstringsize("OK to disconnect.", NULL, NULL, bold);
    font_getstringsize("Eject before disconnecting.", NULL, NULL, normal);
}

void ipodjs_ui_usb_set_ejected(bool ejected)
{
    if (ipodjs_ui_retail_usb.ejected != ejected)
    {
        ipodjs_ui_retail_usb.ejected = ejected;
        ipodjs_ui_retail_usb.presented = false;
    }
}

bool ipodjs_ui_usb_animation_active(void)
{
    return ipodjs_ui_retail_usb.valid && !ipodjs_ui_retail_usb.ejected;
}

static int ipodjs_ui_usb_sync_frame(void)
{
    struct ipodjs_ui_retail_usb_cache *cache = &ipodjs_ui_retail_usb;
    unsigned long elapsed = (unsigned long)(current_tick - cache->started);
    unsigned long cycle_ticks =
        (unsigned long)HZ * cache->sync_arrows.frame_count;

    elapsed %= cycle_ticks;
    return (elapsed * 12u / HZ) % cache->sync_arrows.frame_count;
}

static void ipodjs_ui_draw_usb_sync_asset(struct screen *display)
{
    struct ipodjs_ui_retail_usb_cache *cache = &ipodjs_ui_retail_usb;
    int frame = ipodjs_ui_usb_sync_frame();

    /* DiskMode_Arrows_Color is 0x00000000 in the source RLOC table.
     * All 18 source masks are used at native size; never rotate a substitute. */
    ipodjs_retailos_blit_mask(display, &cache->sync_arrows, frame,
                              122, 62, FB_RGBPACK(0, 0, 0));
    cache->last_frame = frame;
    ipodjs_trace_screen("USB Connected", "source-frame", frame, 0,
                        cache->sync_arrows.frame_count, 122, 62, 76, 76);
}

void ipodjs_ui_draw_usb_connected(struct screen *display)
{
    struct viewport *last_vp;
    struct ipodjs_ui_retail_usb_cache *cache = &ipodjs_ui_retail_usb;
    int normal = ipodjs_ui_usb_small_font();
    int bold = ipodjs_ui_retailos_font(true);

    last_vp = display->set_viewport(NULL);
#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    if (cache->valid && cache->presented)
    {
        if (!cache->ejected &&
            ipodjs_ui_usb_sync_frame() != cache->last_frame)
        {
            /* The USB-exclusive transition buffer contains the completed
             * static screen, including the real badge and its background.
             * Restore only the mask rectangle: no I/O or decode after ACK. */
            display->bitmap_part(ipodjs_ui_animation_old,
                                 122, 62, LCD_WIDTH, 122, 62, 76, 76);
            ipodjs_ui_draw_usb_sync_asset(display);
            display->update_rect(122, 62, 76, 76);
        }
        display->set_viewport(last_vp);
        return;
    }
#endif

    display->set_drawmode(DRMODE_SOLID);
    display->set_background(LCD_RGBPACK(0, 0, 0));
    display->clear_display();
#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    if (cache->valid)
    {
        display->bitmap(ipodjs_ui_animation_old, 0, 0,
                        LCD_WIDTH, LCD_HEIGHT);
        ipodjs_retailos_blit(display,
                            &ipodjs_ui_retail_status.hold_background, 0, 0);
        ipodjs_ui_draw_retailos_battery(
            display, &ipodjs_ui_retail_status.black_battery,
            display->lcdwidth - 31, 4);
        if (button_hold())
            ipodjs_retailos_blit(display,
                                &ipodjs_ui_retail_status.white_lock, 8, 4);
        ipodjs_retailos_blit(display, &cache->badge, 104, 46);
        /* Cache chrome without labels or either foreground icon, so eject
         * can replace the Connected screen without disk access. */
        memcpy(ipodjs_ui_animation_old, FBADDR(0, 0), FRAMEBUFFER_SIZE);
        if (cache->ejected && cache->disconnect_valid)
            ipodjs_retailos_blit(display, &cache->disconnect, 104, 46);
    }
#endif
    /* An incomplete private archive gets a plain unbranded text screen,
     * never the previous procedural Apple-looking substitute. */
    display->set_drawmode(DRMODE_FG);
    display->set_foreground(LCD_RGBPACK(255, 255, 255));
    display->setfont(ipodjs_ui_retailos_font(false));
    ipodjs_ui_puts_fit(display, 40, 4, display->lcdwidth - 80,
                       cache->valid ? "iPod" : "USB", true);
    display->setfont(bold);
    ipodjs_ui_puts_fit(display, 10, 174, display->lcdwidth - 20,
                       cache->ejected ? "OK to disconnect." : "Connected",
                       true);
    display->setfont(normal);
    if (!cache->ejected)
        ipodjs_ui_puts_fit(display, 10, 191, display->lcdwidth - 20,
                           "Eject before disconnecting.", true);
    display->set_drawmode(DRMODE_SOLID);

#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
    if (cache->valid)
    {
        if (!cache->ejected)
            ipodjs_ui_draw_usb_sync_asset(display);
        cache->presented = true;
    }
#endif
    display->update();
    display->set_viewport(last_vp);
}

static void ipodjs_ui_draw_charge_bolt(struct screen *display, bool dark)
{
    static const struct ipodjs_ui_point bolt[] = {
        { 15, 0 }, { 3, 25 }, { 12, 24 }, { 7, 48 },
        { 28, 17 }, { 18, 18 }, { 23, 0 }
    };
    unsigned edge = dark ? LCD_RGBPACK(20, 23, 28) :
                           LCD_RGBPACK(38, 41, 45);
    unsigned face = dark ? LCD_RGBPACK(39, 43, 49) :
                           LCD_RGBPACK(52, 55, 59);

    ipodjs_ui_fill_polygon(display, bolt, ARRAYLEN(bolt),
                           148, 88, edge);
    ipodjs_ui_fill_polygon(display, bolt, ARRAYLEN(bolt),
                           149, 89, face);
}

static void ipodjs_ui_draw_charge_plug(struct screen *display, bool dark)
{
    unsigned top = dark ? LCD_RGBPACK(55, 61, 68) :
                          LCD_RGBPACK(65, 69, 73);
    unsigned color = dark ? LCD_RGBPACK(26, 30, 35) :
                            LCD_RGBPACK(38, 42, 45);

    /* Stock Classic silhouette: cable at left and two right-facing pins. */
    display->set_foreground(color);
    display->fillrect(138, 110, 17, 7);
    ipodjs_ui_charge_rounded_gradient(display, 151, 99, 27, 28, 5,
                                      top, color, color, 151, 27);
    display->set_foreground(color);
    display->fillrect(176, 102, 10, 6);
    display->fillrect(176, 118, 10, 6);
}

static void ipodjs_ui_draw_charge_background(struct screen *display,
                                             int x, int y, int width,
                                             int height, bool dark)
{
    unsigned base_top = dark ? LCD_RGBPACK(66, 75, 89) :
                               LCD_RGBPACK(132, 149, 162);
    unsigned base_middle = dark ? LCD_RGBPACK(39, 46, 57) :
                                  LCD_RGBPACK(101, 113, 125);
    unsigned base_bottom = dark ? LCD_RGBPACK(17, 22, 30) :
                                  LCD_RGBPACK(70, 79, 86);
    int upper = MAX(1, (display->lcdheight * 45) / 100);
    int lower = MAX(1, display->lcdheight - upper);
    int end_y = y + height;
    int end_x = x + width;
    int band_x;

    for (band_x = x; band_x < end_x; band_x += 4)
    {
        int band_width = MIN(4, end_x - band_x);
        int center_x = band_x + band_width / 2;
        int distance = abs(center_x - display->lcdwidth / 2);
        int focus = MAX(0, 160 - distance);
        int focus2 = (focus * focus) / 160;
        int top_glow = dark ? (focus2 * 11) / 160 :
                              (focus2 * 25) / 160;
        int middle_glow = dark ? (focus2 * 7) / 160 :
                                 (focus2 * 14) / 160;
        int bottom_glow = dark ? (focus2 * 3) / 160 :
                                 (focus2 * 7) / 160;
        unsigned top = LCD_RGBPACK(
            MIN(255, RGB_UNPACK_RED(base_top) + top_glow),
            MIN(255, RGB_UNPACK_GREEN(base_top) + top_glow),
            MIN(255, RGB_UNPACK_BLUE(base_top) + top_glow));
        unsigned middle = LCD_RGBPACK(
            MIN(255, RGB_UNPACK_RED(base_middle) + middle_glow),
            MIN(255, RGB_UNPACK_GREEN(base_middle) + middle_glow),
            MIN(255, RGB_UNPACK_BLUE(base_middle) + middle_glow));
        unsigned bottom = LCD_RGBPACK(
            MIN(255, RGB_UNPACK_RED(base_bottom) + bottom_glow),
            MIN(255, RGB_UNPACK_GREEN(base_bottom) + bottom_glow),
            MIN(255, RGB_UNPACK_BLUE(base_bottom) + bottom_glow));

        if (y < upper)
        {
            int part_end = MIN(end_y, upper);

            display->gradient_fillrect_part(band_x, y, band_width,
                                            part_end - y, top, middle,
                                            upper, y);
        }
        if (end_y > upper)
        {
            int part_y = MAX(y, upper);

            display->gradient_fillrect_part(band_x, part_y, band_width,
                                            end_y - part_y, middle, bottom,
                                            lower, part_y - upper);
        }
    }
}

static unsigned ipodjs_ui_charge_background_color(int x, int y, int width,
                                                  int height,
                                                  bool dark)
{
    unsigned base_top = dark ? LCD_RGBPACK(66, 75, 89) :
                               LCD_RGBPACK(132, 149, 162);
    unsigned base_middle = dark ? LCD_RGBPACK(39, 46, 57) :
                                  LCD_RGBPACK(101, 113, 125);
    unsigned base_bottom = dark ? LCD_RGBPACK(17, 22, 30) :
                                  LCD_RGBPACK(70, 79, 86);
    int distance = abs(x - width / 2);
    int focus = MAX(0, width / 2 - distance);
    int focus2 = width > 0 ? (focus * focus) / MAX(1, width / 2) : 0;
    int top_glow = dark ? (focus2 * 11) / MAX(1, width / 2) :
                          (focus2 * 25) / MAX(1, width / 2);
    int middle_glow = dark ? (focus2 * 7) / MAX(1, width / 2) :
                             (focus2 * 14) / MAX(1, width / 2);
    int bottom_glow = dark ? (focus2 * 3) / MAX(1, width / 2) :
                             (focus2 * 7) / MAX(1, width / 2);
    unsigned top = LCD_RGBPACK(
        MIN(255, RGB_UNPACK_RED(base_top) + top_glow),
        MIN(255, RGB_UNPACK_GREEN(base_top) + top_glow),
        MIN(255, RGB_UNPACK_BLUE(base_top) + top_glow));
    unsigned middle = LCD_RGBPACK(
        MIN(255, RGB_UNPACK_RED(base_middle) + middle_glow),
        MIN(255, RGB_UNPACK_GREEN(base_middle) + middle_glow),
        MIN(255, RGB_UNPACK_BLUE(base_middle) + middle_glow));
    unsigned bottom = LCD_RGBPACK(
        MIN(255, RGB_UNPACK_RED(base_bottom) + bottom_glow),
        MIN(255, RGB_UNPACK_GREEN(base_bottom) + bottom_glow),
        MIN(255, RGB_UNPACK_BLUE(base_bottom) + bottom_glow));
    int split = MAX(1, (height * 45) / 100);

    if (y < split)
        return ipodjs_ui_charge_mix(top, middle, y, MAX(1, split - 1));
    return ipodjs_ui_charge_mix(middle, bottom, y - split,
                                MAX(1, height - split - 1));
}

static void ipodjs_ui_draw_charge_reflection(struct screen *display,
                                             int fill_width, bool dark)
{
    const unsigned shell = dark ? LCD_RGBPACK(91, 98, 108) :
                                  LCD_RGBPACK(86, 92, 98);
    const unsigned graphite = dark ? LCD_RGBPACK(57, 63, 72) :
                                     LCD_RGBPACK(62, 67, 71);
    const unsigned green = dark ? LCD_RGBPACK(66, 151, 49) :
                                  LCD_RGBPACK(69, 158, 47);
    const int reflection_y = IPODJS_CHARGE_BODY_Y + IPODJS_CHARGE_BODY_H + 1;
    const int reflection_h = 43;
    int row;

    for (row = 0; row < reflection_h; row++)
    {
        int source_row = IPODJS_CHARGE_BODY_H - 1 - row;
        int inset = ipodjs_ui_charge_round_inset(source_row,
                         IPODJS_CHARGE_BODY_H, 8);
        int left = IPODJS_CHARGE_BODY_X + inset;
        int right = IPODJS_CHARGE_BODY_X + IPODJS_CHARGE_BODY_W - 1 - inset;
        int y = reflection_y + row;
        int alpha = (86 * (reflection_h - row)) / reflection_h;
        unsigned background = ipodjs_ui_charge_background_color(
                                  display->lcdwidth / 2, y,
                                  display->lcdwidth,
                                  display->lcdheight, dark);
        unsigned body = row < 4 ? shell : graphite;

        body = ipodjs_ui_charge_mix(background, body, alpha, 255);
        display->set_foreground(body);
        display->hline(left, right, y);

        if (fill_width > 0)
        {
            int fill_left = MAX(left, IPODJS_CHARGE_WELL_X);
            int fill_right = MIN(right, IPODJS_CHARGE_WELL_X +
                                 fill_width - 1);

            if (fill_left <= fill_right)
            {
                unsigned reflected_green = ipodjs_ui_charge_mix(
                    background, green, alpha, 255);
                display->set_foreground(reflected_green);
                display->hline(fill_left, fill_right, y);
            }
        }
    }
}

static void ipodjs_ui_draw_retail_charge_frame(struct screen *display,
                                                bool full, int fill_width,
                                                bool first)
{
    struct ipodjs_ui_retail_charge_cache *cache =
        &ipodjs_ui_retail_charge;
    struct viewport *last_vp = display->set_viewport(NULL);
    /* Native cap positions measured in Apple's Classic guide, page 12.
     * Charging and Charged references have distinct visual origins. */
    const int origin_x = full ? 60 : IPODJS_RETAIL_CHARGE_X;
    const int origin_y = full ? 63 : IPODJS_RETAIL_CHARGE_Y;
    const int middle_x = origin_x + 38;
    const int right_x = middle_x + IPODJS_RETAIL_CHARGE_MIDDLE_W;
    /* Source casing: x=22 through right-cap x=14, y=22..96.
     * Exclude transparent padding, the terminal and the reflection when
     * centering the native masks; the 142px sprite is not the body height. */
    const int body_x = origin_x + 22;
    const int body_y = origin_y + 22;
    const int body_w = IPODJS_RETAIL_CHARGE_W - 22 - 30;
    const int body_h = 75;
    bool dark = global_settings.ui_engine_dark_mode;
    int middle_width;
    int index;

    fill_width = MAX(0, MIN(fill_width, IPODJS_CHARGE_WELL_W));
    middle_width = (fill_width * IPODJS_RETAIL_CHARGE_MIDDLE_W +
                    IPODJS_CHARGE_WELL_W - 1) /
                   IPODJS_CHARGE_WELL_W;
    if (full)
        middle_width = IPODJS_RETAIL_CHARGE_MIDDLE_W;

    display->set_drawmode(DRMODE_SOLID);
    if (first)
        ipodjs_ui_draw_charge_background(display, 0, 0,
                                          display->lcdwidth,
                                          display->lcdheight, dark);
    else
        ipodjs_ui_draw_charge_background(display, 50, 48, 220, 174, dark);

    ipodjs_ui_glass_gradient(display, 0, 0, display->lcdwidth, 23,
                             LCD_RGBPACK(68, 68, 68),
                             LCD_RGBPACK(31, 35, 38),
                             LCD_RGBPACK(9, 16, 21));
    display->set_foreground(LCD_RGBPACK(105, 116, 125));
    display->hline(0, display->lcdwidth - 1, 22);
    display->setfont(ipodjs_ui_charge_font());
    display->set_foreground(LCD_RGBPACK(244, 246, 248));
    ipodjs_ui_puts_fit(display, 10, 3, display->lcdwidth - 20,
                       full ? "Charged" : "Charging", true);

    if (!full && battery_level() >= 0 && battery_level() <= 2)
    {
        ipodjs_retailos_blit(display, &cache->critical,
                             (LCD_WIDTH - 205) / 2, 48);
    }
    else
    {
        ipodjs_retailos_blit(display, &cache->empty_left,
                             origin_x, origin_y);
        for (index = 0; index < IPODJS_RETAIL_CHARGE_MIDDLE_W; index += 8)
            ipodjs_retailos_blit_part(display, &cache->empty_middle, 0, 0,
                middle_x + index, origin_y,
                MIN(8, IPODJS_RETAIL_CHARGE_MIDDLE_W - index), 142);
        ipodjs_retailos_blit(display, &cache->empty_right, right_x,
                             origin_y);

        if (middle_width > 0)
        {
            ipodjs_retailos_blit(display, &cache->green_left,
                                 origin_x, origin_y);
            for (index = 0; index < middle_width; index += 8)
                ipodjs_retailos_blit_part(display, &cache->green_middle, 0, 0,
                    middle_x + index, origin_y,
                    MIN(8, middle_width - index), 142);
            if (middle_width == IPODJS_RETAIL_CHARGE_MIDDLE_W)
                ipodjs_retailos_blit(display, &cache->green_right,
                                     right_x, origin_y);
            else
                ipodjs_retailos_blit_part(display, &cache->green_middle_cap,
                    0, 0, middle_x + middle_width, origin_y,
                    MIN(8, IPODJS_RETAIL_CHARGE_MIDDLE_W - middle_width), 142);
        }

        if (full)
            ipodjs_retailos_blit_mask(
                display, &cache->plug, 0, body_x + (body_w - 47) / 2,
                body_y + (body_h - 29) / 2,
                FB_RGBPACK(0, 0, 0));
        else
            ipodjs_retailos_blit_mask(
                display, &cache->bolt, 0, body_x + (body_w - 23) / 2,
                body_y + (body_h - 57) / 2,
                FB_RGBPACK(0, 0, 0));
    }

    if (first)
        display->update();
    else
    {
        display->update_rect(0, 0, display->lcdwidth, 23);
        display->update_rect(50, 48, 220, 174);
    }
    display->set_viewport(last_vp);
}

static void ipodjs_ui_draw_charge_frame(struct screen *display, bool full,
                                        int fill_width, bool first)
{
    bool dark = global_settings.ui_engine_dark_mode;
    unsigned title = LCD_RGBPACK(244, 246, 248);
    unsigned outer_top = dark ? LCD_RGBPACK(238, 241, 244) :
                                LCD_RGBPACK(248, 249, 250);
    unsigned outer_mid = dark ? LCD_RGBPACK(132, 139, 148) :
                                LCD_RGBPACK(160, 165, 171);
    unsigned outer_bottom = dark ? LCD_RGBPACK(39, 44, 51) :
                                   LCD_RGBPACK(39, 43, 48);
    unsigned clear_top = dark ? LCD_RGBPACK(166, 172, 181) :
                                LCD_RGBPACK(181, 181, 186);
    unsigned clear_mid = dark ? LCD_RGBPACK(66, 72, 81) :
                                LCD_RGBPACK(69, 73, 77);
    unsigned clear_bottom = dark ? LCD_RGBPACK(73, 79, 87) :
                                   LCD_RGBPACK(93, 95, 97);
    unsigned green_top = dark ? LCD_RGBPACK(174, 239, 146) :
                                LCD_RGBPACK(181, 242, 158);
    unsigned green_mid = dark ? LCD_RGBPACK(62, 160, 47) :
                                LCD_RGBPACK(62, 155, 45);
    unsigned green_bottom = dark ? LCD_RGBPACK(60, 105, 57) :
                                   LCD_RGBPACK(74, 112, 66);
    unsigned shadow = dark ? LCD_RGBPACK(10, 12, 16) :
                             LCD_RGBPACK(73, 79, 87);
    unsigned rim = dark ? LCD_RGBPACK(208, 214, 221) :
                          LCD_RGBPACK(234, 236, 239);
    struct viewport *last_vp;
    int font;

    if (ipodjs_ui_retail_charge.valid)
    {
        ipodjs_ui_draw_retail_charge_frame(display, full, fill_width, first);
        return;
    }

    fill_width = MAX(0, MIN(fill_width, IPODJS_CHARGE_WELL_W));
    if (full)
        fill_width = IPODJS_CHARGE_WELL_W;

    last_vp = display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(dark ? LCD_RGBPACK(52, 59, 71) :
                                   LCD_RGBPACK(199, 204, 211));
    if (first)
    {
        ipodjs_ui_draw_charge_background(display, 0, 0,
                                         display->lcdwidth,
                                         display->lcdheight, dark);
    }

    ipodjs_ui_glass_gradient(display, 0, 0, display->lcdwidth, 23,
                             LCD_RGBPACK(68, 68, 68),
                             LCD_RGBPACK(31, 35, 38),
                             LCD_RGBPACK(9, 16, 21));
    display->set_foreground(LCD_RGBPACK(105, 116, 125));
    display->hline(0, display->lcdwidth - 1, 22);

    font = ipodjs_ui_charge_font();
    display->setfont(font);
    display->set_foreground(title);
    ipodjs_ui_puts_fit(display, 10, 3, display->lcdwidth - 20,
                       full ? "Charged" : "Charging", true);

    ipodjs_ui_draw_charge_background(display, IPODJS_CHARGE_DAMAGE_X,
                                     IPODJS_CHARGE_DAMAGE_Y,
                                     IPODJS_CHARGE_DAMAGE_W,
                                     IPODJS_CHARGE_DAMAGE_H, dark);

    ipodjs_ui_draw_charge_reflection(display, fill_width, dark);

    /* Recessed left cap and the small cylindrical positive terminal. */
    ipodjs_ui_charge_rounded_gradient(display, 86, 82, 11, 62, 5,
                                      outer_top, outer_mid, shadow, 86, 11);
    ipodjs_ui_charge_rounded_gradient(display, 231, 81, 9, 66, 4,
                                      outer_top, outer_mid, outer_bottom,
                                      231, 9);
    ipodjs_ui_charge_rounded_gradient(display, 238, 94, 8, 40, 4,
                                      rim, outer_mid, outer_bottom, 238, 8);

    display->set_foreground(shadow);
    display->hline(94, 238, 152);
    ipodjs_ui_charge_rounded_gradient(display, IPODJS_CHARGE_BODY_X,
                                      IPODJS_CHARGE_BODY_Y,
                                      IPODJS_CHARGE_BODY_W,
                                      IPODJS_CHARGE_BODY_H, 8,
                                      outer_top, outer_mid, outer_bottom,
                                      IPODJS_CHARGE_BODY_X,
                                      IPODJS_CHARGE_BODY_W);
    ipodjs_ui_charge_rounded_gradient(display, IPODJS_CHARGE_WELL_X,
                                      IPODJS_CHARGE_WELL_Y,
                                      IPODJS_CHARGE_WELL_W,
                                      IPODJS_CHARGE_WELL_H, 4,
                                      clear_top, clear_mid, clear_bottom,
                                      IPODJS_CHARGE_WELL_X,
                                      IPODJS_CHARGE_WELL_W);

    if (fill_width > 0)
    {
        ipodjs_ui_charge_rounded_gradient(display, IPODJS_CHARGE_WELL_X,
                                          IPODJS_CHARGE_WELL_Y,
                                          IPODJS_CHARGE_WELL_W,
                                          IPODJS_CHARGE_WELL_H, 4,
                                          green_top, green_mid, green_bottom,
                                          IPODJS_CHARGE_WELL_X, fill_width);
        if (fill_width < IPODJS_CHARGE_WELL_W)
        {
            int fade_width = MIN(5, fill_width);
            int fade;

            /* Stock boundary is a short glass blend, not a bright divider. */
            for (fade = 0; fade < fade_width; fade++)
            {
                int amount = fade + 1;
                int total = fade_width + 1;
                int edge_x = IPODJS_CHARGE_WELL_X + fill_width -
                             fade_width + fade;

                ipodjs_ui_charge_rounded_gradient(display,
                    IPODJS_CHARGE_WELL_X, IPODJS_CHARGE_WELL_Y,
                    IPODJS_CHARGE_WELL_W, IPODJS_CHARGE_WELL_H, 4,
                    ipodjs_ui_charge_mix(green_top, clear_top,
                                         amount, total),
                    ipodjs_ui_charge_mix(green_mid, clear_mid,
                                         amount, total),
                    ipodjs_ui_charge_mix(green_bottom, clear_bottom,
                                         amount, total),
                    edge_x, 1);
            }
        }
    }

    display->set_foreground(rim);
    display->hline(97, 231, IPODJS_CHARGE_WELL_Y);
    display->set_foreground(outer_bottom);
    display->hline(97, 231, IPODJS_CHARGE_WELL_Y +
                             IPODJS_CHARGE_WELL_H - 1);

    if (full)
        ipodjs_ui_draw_charge_plug(display, dark);
    else
        ipodjs_ui_draw_charge_bolt(display, dark);

    if (first)
        display->update();
    else
    {
        display->update_rect(0, 0, display->lcdwidth, 23);
        display->update_rect(IPODJS_CHARGE_DAMAGE_X,
                             IPODJS_CHARGE_DAMAGE_Y,
                             IPODJS_CHARGE_DAMAGE_W,
                             IPODJS_CHARGE_DAMAGE_H);
    }
    display->set_viewport(last_vp);
}

static bool ipodjs_ui_charging_dismiss_action(int action)
{
    if (action == ACTION_NONE || IS_SYSEVENT(action) || button_hold())
        return false;

    return action != ACTION_STD_PREV &&
           action != ACTION_STD_PREVREPEAT &&
           action != ACTION_STD_NEXT &&
           action != ACTION_STD_NEXTREPEAT;
}

static bool ipodjs_ui_charging_handle_event(int action)
{
    if (!IS_SYSEVENT(action))
        return false;

    if (action != SYS_CHARGER_CONNECTED)
        default_event_handler(action);
    return action == SYS_USB_CONNECTED ||
           action == SYS_CHARGER_DISCONNECTED;
}

void ipodjs_ui_charging_screen(bool classify_usb)
{
    struct screen *display = &screens[SCREEN_MAIN];
    long classify_until = current_tick + (HZ * 3) / 4;
    long animation_start;
    long next_sample = current_tick;
    int full_samples = 0;
    int last_fill = -1;
    bool last_full = false;
    bool first = true;

    if (ipodjs_ui_charging_active || ipodjs_ui_charging_seen ||
        !ipodjs_ui_enabled(SCREEN_MAIN))
        return;

    ipodjs_ui_charging_active = true;
    ipodjs_ui_charging_seen = true;
    backlight_on();

    while (classify_usb && charger_inserted() &&
           TIME_BEFORE(current_tick, classify_until))
    {
        int action = get_action(CONTEXT_STD | ALLOW_SOFTLOCK, HZ / 10);

        if (ipodjs_ui_charging_handle_event(action) ||
            ipodjs_ui_charging_dismiss_action(action))
            goto charging_done;
    }

    /* Bounded screen-entry service point.  No asset or font I/O occurs in
     * ipodjs_ui_draw_charge_frame() or on an animation tick. */
    ipodjs_ui_prepare_retailos_charging();
    (void)ipodjs_ui_charge_font();
    animation_start = current_tick;

    while (charger_inserted())
    {
        long now = current_tick;
        int fill_width;
        bool full;
        int action;

        if (!TIME_BEFORE(now, next_sample))
        {
            int level = battery_level();

            if (!charging_state() && level >= 99)
                full_samples++;
            else
                full_samples = 0;
            next_sample = now + HZ;
        }

        full = full_samples >= 2;
        if (full)
            fill_width = IPODJS_CHARGE_WELL_W;
        else
        {
            const long fill_ticks = HZ * 4;
            const long cycle_ticks = (HZ * 19) / 4;
            long elapsed = (now - animation_start) % MAX(1, cycle_ticks);

            /* A continuous stock-style sweep, followed by a short full hold. */
            fill_width = elapsed >= fill_ticks ? IPODJS_CHARGE_WELL_W :
                (int)((elapsed * IPODJS_CHARGE_WELL_W) /
                      MAX(1, fill_ticks));
        }
        if (display->is_backlight_on(false) &&
            (first || fill_width != last_fill || full != last_full))
        {
            ipodjs_ui_draw_charge_frame(display, full, fill_width, first);
            first = false;
            last_fill = fill_width;
            last_full = full;
        }

        action = get_action(CONTEXT_STD | ALLOW_SOFTLOCK,
                            MAX(1, HZ / IPODJS_CHARGE_FPS));
        if (ipodjs_ui_charging_handle_event(action) ||
            ipodjs_ui_charging_dismiss_action(action))
            break;
    }

charging_done:
    ipodjs_ui_charging_active = false;
    send_event(GUI_EVENT_ACTIONUPDATE, (void *)1);
}

void ipodjs_ui_charging_disconnected(void)
{
    ipodjs_ui_charging_seen = false;
}

bool ipodjs_ui_handle_system_event(int action, bool *redraw)
{
    unsigned int power;
    bool usb_power;
    bool power_only;

    if (!IS_SYSEVENT(action))
        return false;

    default_event_handler(action);
    if (action == SYS_CHARGER_CONNECTED && charger_inserted())
    {
        power = power_input_status();
        usb_power = (power & POWER_INPUT_USB) != 0;
        power_only = !usb_power;
#ifdef HAVE_USB_POWER
        if (usb_power)
            power_only = usb_powered_only();
#endif
        if (power_only)
            ipodjs_ui_charging_screen(usb_power);
    }
    if (redraw)
        *redraw = true;
    return true;
}

void ipodjs_ui_label_cache_reset(void)
{
    memset(ipodjs_ui_label_cache, 0, sizeof(ipodjs_ui_label_cache));
    ipodjs_ui_label_cache_stamp = 0;
}

#endif
