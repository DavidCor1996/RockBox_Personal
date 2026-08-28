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
#include "rbunicode.h"
#include "usb.h"
#include "ipodjs_trace.h"
#include "ipodjs_ui.h"

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
#define IPODJS_UI_ACTIVE_TOP       LCD_RGBPACK(107, 200, 254)
#define IPODJS_UI_ACTIVE_MID       LCD_RGBPACK(38, 146, 226)
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
#define IPODJS_STATUS_ICON_SIZE    12
#define IPODJS_APPLE_BATTERY_W     26
#define IPODJS_APPLE_BATTERY_H     65
#define IPODJS_FAST_SCROLL_W       95
#define IPODJS_FAST_SCROLL_H       82
#define IPODJS_FAST_SCROLL_ALPHA_BYTES \
    ((IPODJS_FAST_SCROLL_W + 1) / 2 * IPODJS_FAST_SCROLL_H)
#define IPODJS_STOCK_PLAYING_W     20
#define IPODJS_STOCK_PLAYING_H     32
#define IPODJS_STOCK_HOLD_W        12
#define IPODJS_STOCK_HOLD_H        15
#define IPODJS_STOCK_HEADER_W      320
#define IPODJS_STOCK_HEADER_H      24
#define IPODJS_STOCK_REPEAT_W      21
#define IPODJS_STOCK_REPEAT_H      38
#define IPODJS_STOCK_SHUFFLE_W     21
#define IPODJS_STOCK_SHUFFLE_H     19
#define IPODJS_STOCK_BLUETOOTH_W   12
#define IPODJS_STOCK_BLUETOOTH_H   19
#define IPODJS_STOCK_WIFI_W         18
#define IPODJS_STOCK_WIFI_H         13
#define IPODJS_AIRPODS_W           124
#define IPODJS_AIRPODS_H           109
#define IPODJS_AIRPODS_ANIMATION_FPS 20
#define IPODJS_SEARCH_SURFACE_W     97
#define IPODJS_SEARCH_SURFACE_H     32

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
static fb_data ipodjs_ui_animation_old[FRAMEBUFFER_SIZE / sizeof(fb_data)];
static fb_data ipodjs_ui_animation_new[FRAMEBUFFER_SIZE / sizeof(fb_data)];
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

void ipodjs_ui_transition_cancel(void)
{
    ipodjs_ui_transition_direction = 0;
    ipodjs_ui_transition_deadline = 0;
    ipodjs_ui_preview_fade_active = false;
}

#ifdef IPODJS_UI_HAS_ANIMATION_WORKSPACE
#define IPODJS_NETFLIX_DIR ROCKBOX_DIR "/ipodjs/netflix/launch"
#define IPODJS_NETFLIX_PACK IPODJS_NETFLIX_DIR "/intro-106x60.nfr"
#define IPODJS_NETFLIX_SOUND \
    IPODJS_NETFLIX_DIR "/intro-20000-mono.mulaw"
#define IPODJS_NETFLIX_OUTPUT_WIDTH LCD_WIDTH
#define IPODJS_NETFLIX_OUTPUT_HEIGHT LCD_HEIGHT
#define IPODJS_NETFLIX_SOURCE_RATE 20000
#define IPODJS_NETFLIX_PACK_HEADER 16
#define IPODJS_NETFLIX_PCM_CHUNK_FRAMES 512
#define IPODJS_NETFLIX_INDEX_BYTES \
    (IPODJS_NETFLIX_OUTPUT_WIDTH * IPODJS_NETFLIX_OUTPUT_HEIGHT / 2)

static uint16_t ipodjs_netflix_read_le16(const unsigned char *data)
{
    return data[0] | (data[1] << 8);
}

static uint32_t ipodjs_netflix_read_le32(const unsigned char *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

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

static unsigned int ipodjs_netflix_index_get(const unsigned char *indices,
                                              size_t position)
{
    unsigned char packed = indices[position / 2];
    return position & 1 ? packed & 15 : packed >> 4;
}

static void ipodjs_netflix_index_set(unsigned char *indices, size_t position,
                                     unsigned int value)
{
    unsigned char *packed = &indices[position / 2];

    if (position & 1)
        *packed = (*packed & 0xf0) | value;
    else
        *packed = (*packed & 0x0f) | (value << 4);
}

static bool ipodjs_netflix_decode_indices(const unsigned char *pack,
                                           size_t pack_size, int frame,
                                           unsigned char *indices)
{
    size_t offsets_at;
    size_t position = 0;
    size_t cursor;
    size_t end;
    int palette_count;
    int frame_count;
    int width;
    int height;
    bool rgb565;

    if (pack_size < IPODJS_NETFLIX_PACK_HEADER ||
        (memcmp(pack, "NFX1", 4) && memcmp(pack, "NFR1", 4)))
        return false;
    rgb565 = !memcmp(pack, "NFR1", 4);
    width = ipodjs_netflix_read_le16(pack + 4);
    height = ipodjs_netflix_read_le16(pack + 6);
    frame_count = ipodjs_netflix_read_le16(pack + 8);
    palette_count = ipodjs_netflix_read_le16(pack + 12);
    if (frame < 0 || frame >= frame_count ||
        frame_count <= 0 || frame_count > 64 ||
        width <= 0 || width > IPODJS_NETFLIX_OUTPUT_WIDTH ||
        height <= 0 || height > IPODJS_NETFLIX_OUTPUT_HEIGHT ||
        (width * height) & 1 ||
        (rgb565 ? palette_count != 0 :
                  palette_count <= 0 || palette_count > 16))
        return false;
    offsets_at = IPODJS_NETFLIX_PACK_HEADER + palette_count * 2;
    if (offsets_at + (frame_count + 1) * 4 > pack_size)
        return false;
    cursor = ipodjs_netflix_read_le32(pack + offsets_at + frame * 4);
    end = ipodjs_netflix_read_le32(pack + offsets_at + (frame + 1) * 4);
    if (cursor >= end || end > pack_size)
        return false;

    while (cursor + 2 <= end &&
           position < (size_t)width * height)
    {
        unsigned int token = ipodjs_netflix_read_le16(pack + cursor);
        size_t count = token & 0x7fff;

        cursor += 2;
        if (count == 0 ||
            position + count > (size_t)width * height)
            return false;
        if (token & 0x8000)
        {
            position += count;
            continue;
        }
        if (rgb565)
        {
            if (cursor + count * 2 > end)
                return false;
            for (size_t pixel = 0; pixel < count; ++pixel)
                ((fb_data *)indices)[position + pixel] =
                    (fb_data)ipodjs_netflix_read_le16(
                        pack + cursor + pixel * 2);
            cursor += count * 2;
        }
        else
        {
            if (cursor + (count + 1) / 2 > end)
                return false;
            for (size_t pixel = 0; pixel < count; ++pixel)
            {
                unsigned char packed = pack[cursor + pixel / 2];
                unsigned int color = pixel & 1 ? packed & 15 : packed >> 4;

                if (color >= (unsigned int)palette_count)
                    return false;
                ipodjs_netflix_index_set(indices, position + pixel, color);
            }
            cursor += (count + 1) / 2;
        }
        position += count;
    }
    return position == (size_t)width * height &&
           cursor == end;
}

static void ipodjs_netflix_render(const unsigned char *pack,
                                  const unsigned char *indices)
{
    static unsigned short source_x_map[IPODJS_NETFLIX_OUTPUT_WIDTH];
    static int mapped_width;
    static int mapped_height;
    fb_data palette[16];
    fb_data *output = FBADDR(0, 0);
    int width = ipodjs_netflix_read_le16(pack + 4);
    int height = ipodjs_netflix_read_le16(pack + 6);
    int palette_count = ipodjs_netflix_read_le16(pack + 12);
    bool rgb565 = !memcmp(pack, "NFR1", 4);
    int crop_x = 0;
    int crop_y = 0;
    int crop_width = width;
    int crop_height = height;
    int previous_source_y = -1;

    /* Fill 320x240 without distorting the 16:9 source.  The source is wider
     * than the LCD, so retain its full height and crop equal amounts from the
     * left and right before nearest-neighbour scaling. */
    if (width * IPODJS_NETFLIX_OUTPUT_HEIGHT >
        height * IPODJS_NETFLIX_OUTPUT_WIDTH)
    {
        crop_width = height * IPODJS_NETFLIX_OUTPUT_WIDTH /
                     IPODJS_NETFLIX_OUTPUT_HEIGHT;
        crop_x = (width - crop_width) / 2;
    }
    else if (width * IPODJS_NETFLIX_OUTPUT_HEIGHT <
             height * IPODJS_NETFLIX_OUTPUT_WIDTH)
    {
        crop_height = width * IPODJS_NETFLIX_OUTPUT_HEIGHT /
                      IPODJS_NETFLIX_OUTPUT_WIDTH;
        crop_y = (height - crop_height) / 2;
    }

    if (mapped_width != width || mapped_height != height)
    {
        for (int x = 0; x < IPODJS_NETFLIX_OUTPUT_WIDTH; ++x)
            source_x_map[x] = crop_x + x * crop_width /
                IPODJS_NETFLIX_OUTPUT_WIDTH;
        mapped_width = width;
        mapped_height = height;
    }
    for (int index = 0; index < palette_count; ++index)
        palette[index] = (fb_data)ipodjs_netflix_read_le16(
            pack + IPODJS_NETFLIX_PACK_HEADER + index * 2);
    for (int y = 0; y < IPODJS_NETFLIX_OUTPUT_HEIGHT; ++y)
    {
        int source_y = crop_y + y * crop_height /
            IPODJS_NETFLIX_OUTPUT_HEIGHT;
        size_t source_row = (size_t)source_y * width;
        fb_data *destination =
            output + (size_t)y * IPODJS_NETFLIX_OUTPUT_WIDTH;

        if (rgb565 && source_y == previous_source_y)
        {
            memcpy(destination,
                   destination - IPODJS_NETFLIX_OUTPUT_WIDTH,
                   IPODJS_NETFLIX_OUTPUT_WIDTH * sizeof(fb_data));
            continue;
        }
        for (int x = 0; x < IPODJS_NETFLIX_OUTPUT_WIDTH; ++x)
        {
            if (rgb565)
            {
                const fb_data *pixels = (const fb_data *)indices;

                destination[x] =
                    pixels[source_row + source_x_map[x]];
            }
            else
            {
                size_t source = source_row + source_x_map[x];
                destination[x] =
                    palette[ipodjs_netflix_index_get(indices, source)];
            }
        }
        previous_source_y = source_y;
    }
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
    unsigned char *pack = (unsigned char *)ipodjs_ui_animation_old;
    unsigned char *sound = (unsigned char *)ipodjs_ui_animation_new;
    size_t pack_size;
    size_t sound_size = 0;
    size_t index_offset;
    unsigned char *current_indices;
    int frame_count;
    int frame_ms;
    bool usb = false;
    long started;

#ifndef HAVE_HARDWARE_BEEP
    /* The workspace is reused for every launch. Detach the previous beep
     * callback before either half of it is overwritten by new assets. */
    ipodjs_netflix_beep_detach();
#endif
    ipodjs_ui_transition_cancel();
    if (!ipodjs_netflix_read_file(IPODJS_NETFLIX_PACK, pack,
                                   sizeof(ipodjs_ui_animation_old),
                                   &pack_size) ||
        pack_size < IPODJS_NETFLIX_PACK_HEADER ||
        (memcmp(pack, "NFX1", 4) && memcmp(pack, "NFR1", 4)))
        return false;
    frame_count = ipodjs_netflix_read_le16(pack + 8);
    frame_ms = ipodjs_netflix_read_le16(pack + 10);
    if (frame_count <= 0 || frame_count > 64 ||
        frame_ms < 40 || frame_ms > 500)
        return false;
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
    current_indices = sound + index_offset;
#ifndef HAVE_HARDWARE_BEEP
    ipodjs_netflix_pcm =
        (int16_t *)(current_indices + IPODJS_NETFLIX_INDEX_BYTES);
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
    memset(current_indices, 0, IPODJS_NETFLIX_INDEX_BYTES);
    if (!ipodjs_netflix_decode_indices(
            pack, pack_size, 0, current_indices))
    {
#ifndef HAVE_HARDWARE_BEEP
        ipodjs_netflix_beep_detach();
#endif
        return false;
    }
    ipodjs_netflix_render(pack, current_indices);

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
        if (!ipodjs_netflix_decode_indices(
                pack, pack_size, frame, current_indices))
            goto stop;
        if (frame + 1 == frame_count ||
            TIME_BEFORE(current_tick, following_target))
            ipodjs_netflix_render(pack, current_indices);
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
        int progress = frame * frame *
            (3 * (IPODJS_UI_ANIMATION_SAMPLES - 1) - 2 * frame);
        int divisor = (IPODJS_UI_ANIMATION_SAMPLES - 1) *
            (IPODJS_UI_ANIMATION_SAMPLES - 1) *
            (IPODJS_UI_ANIMATION_SAMPLES - 1);
        int alpha = 256 * progress / divisor;

        ipodjs_ui_animation_wait(start_tick, frame);
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

struct ipodjs_ui_status_icon_cache {
    struct bitmap bm;
    unsigned char data[
        BM_SIZE(IPODJS_STATUS_ICON_SIZE, IPODJS_STATUS_ICON_SIZE,
                FORMAT_NATIVE, false)];
    bool tried;
    bool valid;
};

static struct ipodjs_ui_status_icon_cache
    ipodjs_ui_status_icons[2][2];

struct ipodjs_ui_stock_status_cache {
    struct bitmap battery;
    unsigned char battery_data[
        BM_SIZE(IPODJS_APPLE_BATTERY_W, IPODJS_APPLE_BATTERY_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap playing;
    unsigned char playing_data[
        BM_SIZE(IPODJS_STOCK_PLAYING_W, IPODJS_STOCK_PLAYING_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap hold;
    unsigned char hold_data[
        BM_SIZE(IPODJS_STOCK_HOLD_W, IPODJS_STOCK_HOLD_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap header;
    unsigned char header_data[
        BM_SIZE(IPODJS_STOCK_HEADER_W, IPODJS_STOCK_HEADER_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap repeat;
    unsigned char repeat_data[
        BM_SIZE(IPODJS_STOCK_REPEAT_W, IPODJS_STOCK_REPEAT_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap shuffle;
    unsigned char shuffle_data[
        BM_SIZE(IPODJS_STOCK_SHUFFLE_W, IPODJS_STOCK_SHUFFLE_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap bluetooth;
    unsigned char bluetooth_data[
        BM_SIZE(IPODJS_STOCK_BLUETOOTH_W, IPODJS_STOCK_BLUETOOTH_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap wifi;
    unsigned char wifi_data[
        BM_SIZE(IPODJS_STOCK_WIFI_W, IPODJS_STOCK_WIFI_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap airpods;
    unsigned char airpods_data[
        BM_SIZE(IPODJS_AIRPODS_W, IPODJS_AIRPODS_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    bool battery_tried, battery_valid;
    bool playing_tried, playing_valid;
    bool hold_tried, hold_valid;
    bool header_tried, header_valid;
    bool repeat_tried, repeat_valid;
    bool shuffle_tried, shuffle_valid;
    bool bluetooth_tried, bluetooth_valid;
    bool wifi_tried, wifi_valid;
    bool airpods_tried, airpods_valid;
};

static struct ipodjs_ui_stock_status_cache ipodjs_ui_stock_status;
static struct bitmap ipodjs_ui_fast_scroll_overlay;
static unsigned char ipodjs_ui_fast_scroll_overlay_data[
    BM_SIZE(IPODJS_FAST_SCROLL_W, IPODJS_FAST_SCROLL_H,
            FORMAT_NATIVE, false) + IPODJS_FAST_SCROLL_ALPHA_BYTES] IPODJS_BM_ALIGN;
static bool ipodjs_ui_fast_scroll_overlay_tried[2];
static bool ipodjs_ui_fast_scroll_overlay_valid;
static int ipodjs_ui_fast_scroll_overlay_kind = -1;

struct ipodjs_ui_search_surface_cache {
    struct bitmap field;
    unsigned char field_data[
        BM_SIZE(IPODJS_SEARCH_SURFACE_W, IPODJS_SEARCH_SURFACE_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap selected;
    unsigned char selected_data[
        BM_SIZE(IPODJS_SEARCH_SURFACE_W, IPODJS_SEARCH_SURFACE_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    bool field_tried, field_valid;
    bool selected_tried, selected_valid;
};

static struct ipodjs_ui_search_surface_cache ipodjs_ui_search_surfaces;

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

static struct bitmap *ipodjs_ui_apple_battery(void)
{
    return ipodjs_ui_load_stock_status(
        IPODJS_UI_APPLE_ASSET_DIR
            "/status-battery.apple.26x65x24.bmp",
        &ipodjs_ui_stock_status.battery,
        ipodjs_ui_stock_status.battery_data,
        sizeof(ipodjs_ui_stock_status.battery_data),
        IPODJS_APPLE_BATTERY_W, IPODJS_APPLE_BATTERY_H,
        &ipodjs_ui_stock_status.battery_tried,
        &ipodjs_ui_stock_status.battery_valid);
}

static struct bitmap *ipodjs_ui_stock_playing(void)
{
    return ipodjs_ui_load_stock_status(
        IPODJS_UI_APPLE_ASSET_DIR
            "/status-playback.apple.20x32x24.bmp",
        &ipodjs_ui_stock_status.playing,
        ipodjs_ui_stock_status.playing_data,
        sizeof(ipodjs_ui_stock_status.playing_data),
        IPODJS_STOCK_PLAYING_W, IPODJS_STOCK_PLAYING_H,
        &ipodjs_ui_stock_status.playing_tried,
        &ipodjs_ui_stock_status.playing_valid);
}

static struct bitmap *ipodjs_ui_stock_hold(void)
{
    return ipodjs_ui_load_stock_status(
        IPODJS_UI_APPLE_ASSET_DIR "/status-hold.apple.12x15x24.bmp",
        &ipodjs_ui_stock_status.hold, ipodjs_ui_stock_status.hold_data,
        sizeof(ipodjs_ui_stock_status.hold_data),
        IPODJS_STOCK_HOLD_W, IPODJS_STOCK_HOLD_H,
        &ipodjs_ui_stock_status.hold_tried,
        &ipodjs_ui_stock_status.hold_valid);
}

static struct bitmap *ipodjs_ui_stock_header(void)
{
    return ipodjs_ui_load_stock_status(
        IPODJS_UI_APPLE_ASSET_DIR "/status-header.apple.320x24x24.bmp",
        &ipodjs_ui_stock_status.header, ipodjs_ui_stock_status.header_data,
        sizeof(ipodjs_ui_stock_status.header_data),
        IPODJS_STOCK_HEADER_W, IPODJS_STOCK_HEADER_H,
        &ipodjs_ui_stock_status.header_tried,
        &ipodjs_ui_stock_status.header_valid);
}

static struct bitmap *ipodjs_ui_stock_repeat(void)
{
    return ipodjs_ui_load_stock_status(
        IPODJS_UI_APPLE_ASSET_DIR "/status-repeat.apple.21x38x24.bmp",
        &ipodjs_ui_stock_status.repeat, ipodjs_ui_stock_status.repeat_data,
        sizeof(ipodjs_ui_stock_status.repeat_data),
        IPODJS_STOCK_REPEAT_W, IPODJS_STOCK_REPEAT_H,
        &ipodjs_ui_stock_status.repeat_tried,
        &ipodjs_ui_stock_status.repeat_valid);
}

static struct bitmap *ipodjs_ui_stock_shuffle(void)
{
    return ipodjs_ui_load_stock_status(
        IPODJS_UI_APPLE_ASSET_DIR "/status-shuffle.apple.21x19x24.bmp",
        &ipodjs_ui_stock_status.shuffle, ipodjs_ui_stock_status.shuffle_data,
        sizeof(ipodjs_ui_stock_status.shuffle_data),
        IPODJS_STOCK_SHUFFLE_W, IPODJS_STOCK_SHUFFLE_H,
        &ipodjs_ui_stock_status.shuffle_tried,
        &ipodjs_ui_stock_status.shuffle_valid);
}

static struct bitmap *ipodjs_ui_stock_bluetooth(void)
{
    return ipodjs_ui_load_stock_status(
        IPODJS_UI_APPLE_ASSET_DIR
            "/status-bluetooth.apple.12x19x24.bmp",
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

static struct bitmap *ipodjs_ui_fast_scroll_overlay_asset(bool digits)
{
    int kind = digits ? 1 : 0;
    const char *path = digits ?
        IPODJS_UI_APPLE_ASSET_DIR
            "/fast-scroll-123.apple.95x82x32.bmp" :
        IPODJS_UI_APPLE_ASSET_DIR
            "/fast-scroll-blank.apple.95x82x32.bmp";
    int rc;

    if (ipodjs_ui_fast_scroll_overlay_valid &&
        ipodjs_ui_fast_scroll_overlay_kind == kind)
        return &ipodjs_ui_fast_scroll_overlay;
    if (ipodjs_ui_fast_scroll_overlay_tried[kind] && !file_exists(path))
        return NULL;

    ipodjs_ui_fast_scroll_overlay_tried[kind] = true;
    ipodjs_ui_fast_scroll_overlay_valid = false;
    if (!file_exists(path))
        return NULL;

    memset(&ipodjs_ui_fast_scroll_overlay, 0,
           sizeof(ipodjs_ui_fast_scroll_overlay));
    ipodjs_ui_fast_scroll_overlay.width = IPODJS_FAST_SCROLL_W;
    ipodjs_ui_fast_scroll_overlay.height = IPODJS_FAST_SCROLL_H;
    ipodjs_ui_fast_scroll_overlay.format = FORMAT_NATIVE;
    ipodjs_ui_fast_scroll_overlay.data =
        ipodjs_ui_fast_scroll_overlay_data;
    rc = read_bmp_file(path, &ipodjs_ui_fast_scroll_overlay,
                       sizeof(ipodjs_ui_fast_scroll_overlay_data),
                       FORMAT_NATIVE | FORMAT_DITHER | FORMAT_TRANSPARENT,
                       NULL);
    if (rc < 0 ||
        ipodjs_ui_fast_scroll_overlay.width != IPODJS_FAST_SCROLL_W ||
        ipodjs_ui_fast_scroll_overlay.height != IPODJS_FAST_SCROLL_H)
        return NULL;

    ipodjs_ui_fast_scroll_overlay_kind = kind;
    ipodjs_ui_fast_scroll_overlay_valid = true;
    return &ipodjs_ui_fast_scroll_overlay;
}

static struct bitmap *ipodjs_ui_search_surface_asset(
    enum ipodjs_ui_search_surface surface)
{
    if (surface == IPODJS_UI_SEARCH_PANEL)
        return ipodjs_ui_fast_scroll_overlay_asset(false);
    if (surface == IPODJS_UI_SEARCH_FIELD)
    {
        return ipodjs_ui_load_stock_status(
            IPODJS_UI_APPLE_ASSET_DIR
                "/search-field.apple.97x32x24.bmp",
            &ipodjs_ui_search_surfaces.field,
            ipodjs_ui_search_surfaces.field_data,
            sizeof(ipodjs_ui_search_surfaces.field_data),
            IPODJS_SEARCH_SURFACE_W, IPODJS_SEARCH_SURFACE_H,
            &ipodjs_ui_search_surfaces.field_tried,
            &ipodjs_ui_search_surfaces.field_valid);
    }
    if (surface == IPODJS_UI_SEARCH_SELECTED)
    {
        return ipodjs_ui_load_stock_status(
            IPODJS_UI_APPLE_ASSET_DIR
                "/search-selected.apple.97x32x24.bmp",
            &ipodjs_ui_search_surfaces.selected,
            ipodjs_ui_search_surfaces.selected_data,
            sizeof(ipodjs_ui_search_surfaces.selected_data),
            IPODJS_SEARCH_SURFACE_W, IPODJS_SEARCH_SURFACE_H,
            &ipodjs_ui_search_surfaces.selected_tried,
            &ipodjs_ui_search_surfaces.selected_valid);
    }
    return NULL;
}

bool ipodjs_ui_search_surfaces_available(void)
{
    return file_exists(IPODJS_UI_APPLE_ASSET_DIR
                       "/fast-scroll-blank.apple.95x82x32.bmp") &&
           file_exists(IPODJS_UI_APPLE_ASSET_DIR
                       "/search-field.apple.97x32x24.bmp") &&
           file_exists(IPODJS_UI_APPLE_ASSET_DIR
                       "/search-selected.apple.97x32x24.bmp");
}

bool ipodjs_ui_prepare_search_surfaces(void)
{
    return ipodjs_ui_search_surface_asset(IPODJS_UI_SEARCH_PANEL) != NULL &&
           ipodjs_ui_search_surface_asset(IPODJS_UI_SEARCH_FIELD) != NULL &&
           ipodjs_ui_search_surface_asset(IPODJS_UI_SEARCH_SELECTED) != NULL;
}

static struct bitmap *ipodjs_ui_search_surface_cached(
    enum ipodjs_ui_search_surface surface)
{
    if (surface == IPODJS_UI_SEARCH_PANEL)
    {
        return ipodjs_ui_fast_scroll_overlay_valid ?
            &ipodjs_ui_fast_scroll_overlay : NULL;
    }
    if (surface == IPODJS_UI_SEARCH_FIELD)
    {
        return ipodjs_ui_search_surfaces.field_valid ?
            &ipodjs_ui_search_surfaces.field : NULL;
    }
    if (surface == IPODJS_UI_SEARCH_SELECTED)
    {
        return ipodjs_ui_search_surfaces.selected_valid ?
            &ipodjs_ui_search_surfaces.selected : NULL;
    }
    return NULL;
}

static void ipodjs_ui_draw_tiled_part(struct screen *display,
                                      struct bitmap *bm,
                                      int src_x, int src_y,
                                      int src_w, int src_h,
                                      int x, int y, int width, int height)
{
    int drawn_y = 0;

    while (drawn_y < height)
    {
        int part_h = MIN(src_h, height - drawn_y);
        int part_src_y = src_y + (src_h - part_h) / 2;
        int drawn_x = 0;

        while (drawn_x < width)
        {
            int part_w = MIN(src_w, width - drawn_x);
            int part_src_x = src_x + (src_w - part_w) / 2;

            display->bmp_part(bm, part_src_x, part_src_y,
                              x + drawn_x, y + drawn_y,
                              part_w, part_h);
            drawn_x += part_w;
        }
        drawn_y += part_h;
    }
}

bool ipodjs_ui_draw_search_surface(struct screen *display,
                                   enum ipodjs_ui_search_surface surface,
                                   int x, int y, int width, int height)
{
    struct bitmap *bm = ipodjs_ui_search_surface_cached(surface);
    /* The Apple fast-scroll plate has a roughly 16 px corner radius.  Cutting
     * it at 8 px tiles part of each curve into the horizontal Search strip,
     * leaving visible shoulders at both ends. */
    int border = surface == IPODJS_UI_SEARCH_PANEL ? 16 : 6;
    int center_w;
    int center_h;

    if (!display || !bm || width < border * 2 || height < border * 2)
        return false;

    center_w = width - border * 2;
    center_h = height - border * 2;
    display->set_drawmode(DRMODE_FG);

    display->bmp_part(bm, 0, 0, x, y, border, border);
    display->bmp_part(bm, bm->width - border, 0,
                      x + width - border, y, border, border);
    display->bmp_part(bm, 0, bm->height - border,
                      x, y + height - border, border, border);
    display->bmp_part(bm, bm->width - border, bm->height - border,
                      x + width - border, y + height - border,
                      border, border);

    ipodjs_ui_draw_tiled_part(display, bm, border, 0,
                              bm->width - border * 2, border,
                              x + border, y, center_w, border);
    ipodjs_ui_draw_tiled_part(display, bm, border, bm->height - border,
                              bm->width - border * 2, border,
                              x + border, y + height - border,
                              center_w, border);
    ipodjs_ui_draw_tiled_part(display, bm, 0, border,
                              border, bm->height - border * 2,
                              x, y + border, border, center_h);
    ipodjs_ui_draw_tiled_part(display, bm, bm->width - border, border,
                              border, bm->height - border * 2,
                              x + width - border, y + border,
                              border, center_h);
    ipodjs_ui_draw_tiled_part(display, bm, border, border,
                              bm->width - border * 2,
                              bm->height - border * 2,
                              x + border, y + border,
                              center_w, center_h);
    display->set_drawmode(DRMODE_SOLID);
    return true;
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
    return ipodjs_ui_dark() ? LCD_RGBPACK(18, 20, 24) :
                              IPODJS_UI_SCREEN_BG;
}

unsigned ipodjs_ui_row_bg(void)
{
    return ipodjs_ui_dark() ? LCD_RGBPACK(24, 27, 32) :
                              IPODJS_UI_SCREEN_BG;
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
    return ipodjs_ui_dark() ? LCD_RGBPACK(246, 248, 250) :
                              IPODJS_UI_TEXT;
}

unsigned ipodjs_ui_header_bg(void)
{
    return ipodjs_ui_dark() ? LCD_RGBPACK(24, 29, 38) :
                              IPODJS_UI_HEADER_BOTTOM;
}

unsigned ipodjs_ui_panel(void)
{
    if (ipodjs_ui_dark())
        return LCD_RGBPACK(24, 27, 32);
    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_TRANSPARENT)
        return LCD_RGBPACK(248, 249, 250);
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
    unsigned top;
    unsigned bottom;

    if (global_settings.ui_engine_accent == UI_ENGINE_ACCENT_BLUE)
    {
        ipodjs_ui_glass_gradient(display, x, y, w, h,
                                 IPODJS_UI_ACTIVE_TOP,
                                 IPODJS_UI_ACTIVE_MID,
                                 IPODJS_UI_ACTIVE_BOTTOM);
        if (midp)
            *midp = IPODJS_UI_ACTIVE_MID;
        return;
    }

    top = ipodjs_ui_rgb_blend(FB_UNPACK_RED(accent),
                              FB_UNPACK_GREEN(accent),
                              FB_UNPACK_BLUE(accent),
                              255, 255, 255, 112);
    bottom = ipodjs_ui_rgb_blend(FB_UNPACK_RED(accent),
                                 FB_UNPACK_GREEN(accent),
                                 FB_UNPACK_BLUE(accent),
                                 0, 0, 0, 70);
    ipodjs_ui_glass_gradient(display, x, y, w, h, top, accent, bottom);
    if (midp)
        *midp = accent;
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

    display->set_foreground(color);
    display->fillrect(x, y + 1, 2, 1);
    display->fillrect(x + 2, y + 2, 2, 1);
    display->fillrect(x + 4, y + 3, 2, 1);
    display->fillrect(x + 2, y + 4, 2, 1);
    display->fillrect(x, y + 5, 2, 1);
}

static struct bitmap *ipodjs_ui_status_icon(bool paused)
{
    bool dark = global_settings.ui_engine_dark_mode;
    struct ipodjs_ui_status_icon_cache *cache =
        &ipodjs_ui_status_icons[dark ? 1 : 0][paused ? 1 : 0];
    const char *path;
    int rc;

    if (cache->valid)
        return &cache->bm;
    if (cache->tried)
        return NULL;

    if (paused)
        path = dark ? IPODJS_UI_ASSET_DIR "/pause.12x12x24-dark.bmp" :
                      IPODJS_UI_ASSET_DIR "/pause.12x12x24.bmp";
    else
        path = dark ? IPODJS_UI_ASSET_DIR "/play.12x12x24-dark.bmp" :
                      IPODJS_UI_ASSET_DIR "/play.12x12x24.bmp";

    cache->tried = true;
    if (!file_exists(path))
        return NULL;

    memset(&cache->bm, 0, sizeof(cache->bm));
    cache->bm.width = IPODJS_STATUS_ICON_SIZE;
    cache->bm.height = IPODJS_STATUS_ICON_SIZE;
    cache->bm.format = FORMAT_NATIVE;
    cache->bm.data = cache->data;
    rc = read_bmp_file(path, &cache->bm, sizeof(cache->data),
                       FORMAT_NATIVE | FORMAT_DITHER | FORMAT_TRANSPARENT,
                       NULL);
    if (rc < 0)
        return NULL;

    cache->valid = true;
    return &cache->bm;
}

void ipodjs_ui_draw_header_background(struct screen *display, int width)
{
    struct bitmap *header;

    if (!display)
        return;
    width = MAX(0, MIN(width, display->lcdwidth));
    header = global_settings.ui_engine_dark_mode ? NULL :
             ipodjs_ui_stock_header();
    if (header)
    {
        display->bmp_part(header, 0, 0, 0, 0, width,
                          IPODJS_UI_HEADER_HEIGHT);
        return;
    }

    ipodjs_ui_glass_gradient(display, 0, 0, width,
        IPODJS_UI_HEADER_HEIGHT,
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
    struct bitmap *bm;
    unsigned color;

    if (!display)
        return;

    status = audio_status();
    if (!(status & AUDIO_STATUS_PLAY))
        return;

    paused = (status & AUDIO_STATUS_PAUSE) != 0;
    if (!global_settings.ui_engine_dark_mode)
    {
        bm = ipodjs_ui_stock_playing();
        if (bm)
        {
            display->bmp_part(bm, 0, paused ? 16 : 0, x, y, 20, 16);
            return;
        }
    }
    bm = ipodjs_ui_status_icon(paused);
    if (bm)
    {
        display->bmp(bm, x, y);
        return;
    }

    color = global_settings.ui_engine_dark_mode ?
        LCD_RGBPACK(116, 191, 234) : LCD_RGBPACK(43, 153, 213);
    display->set_foreground(color);
    if (paused)
    {
        display->fillrect(x + 2, y + 2, 2, 8);
        display->fillrect(x + 7, y + 2, 2, 8);
    }
    else
    {
        display->vline(x + 3, y + 1, y + 10);
        display->vline(x + 4, y + 2, y + 9);
        display->vline(x + 5, y + 3, y + 8);
        display->vline(x + 6, y + 4, y + 7);
        display->vline(x + 7, y + 5, y + 6);
    }
}

void ipodjs_ui_draw_hold_indicator(struct screen *display, int x, int y)
{
    struct bitmap *bm;

    if (!display || !button_hold())
        return;
    bm = ipodjs_ui_stock_hold();
    if (bm)
        display->bmp(bm, x, y);
}

void ipodjs_ui_draw_repeat_indicator(struct screen *display, int x, int y,
                                     int repeat_mode)
{
    struct bitmap *bm;
    int frame = 0;

    if (!display || repeat_mode == REPEAT_OFF)
        return;
    bm = ipodjs_ui_stock_repeat();
    if (!bm)
        return;
    if (repeat_mode == REPEAT_ONE)
        frame = 1;
    display->bmp_part(bm, 0, frame * 19, x, y, 21, 19);
}

void ipodjs_ui_draw_shuffle_indicator(struct screen *display, int x, int y)
{
    struct bitmap *bm;

    if (!display)
        return;
    bm = ipodjs_ui_stock_shuffle();
    if (bm)
        display->bmp(bm, x, y);
}

void ipodjs_ui_prepare_bluetooth_indicator(void)
{
    /* Screen-entry service point: the draw path below remains I/O-free. */
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

    display->bmp(&ipodjs_ui_stock_status.bluetooth, x, y);
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
        unsigned color = distance == 0 ? LCD_RGBPACK(0, 122, 255) :
            distance <= 2 ? LCD_RGBPACK(105, 179, 255) :
            LCD_RGBPACK(205, 226, 248);

        display->set_foreground(color);
        display->fillrect(x + points[index][0] - 1,
                          y + points[index][1] - 1, 3, 3);
    }
}

static void ipodjs_ui_draw_connection_check(struct screen *display,
                                             int x, int y)
{
    display->set_foreground(LCD_RGBPACK(0, 122, 255));
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
            LCD_RGBPACK(0, 122, 255) : LCD_RGBPACK(105, 107, 112));
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

    struct bitmap *stock;

    if (!display)
        return;

    level = MAX(0, MIN(100, battery_level()));
    stock = global_settings.ui_engine_dark_mode ? NULL :
            ipodjs_ui_apple_battery();
    if (stock)
    {
        int frame;

        if (charger_inserted())
            frame = level >= 100 ? 4 : 3;
        else if (level <= 20)
            frame = 0;
        else if (level < 80)
            frame = 1;
        else
            frame = 2;
        /* These five 26x13 states are the lossless images embedded in
         * Apple's iPod classic 120GB User Guide.  Sparse documented states
         * are preferable to inventing intermediate pixels. */
        display->bmp_part(stock, 0, frame * 13, x, y, 26, 13);
        return;
    }
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
    static int fast_font = -2;
    const char *path =
        IPODJS_UI_APPLE_ASSET_DIR "/23-Helvetica-Apple.fnt";

    if (fast_font < 0 && file_exists(path))
        fast_font = font_load(path);
    if (fast_font >= 0)
        font_lock(fast_font, true);

    return fast_font >= 0 ? fast_font : ipodjs_ui_font();
}

bool ipodjs_ui_fast_scroll_available(void)
{
    /* Never expose a text-only or recreated approximation.  Both frames are
     * direct paMB extractions from verified Apple firmware and the font is
     * converted from that firmware's Helvetica_23.ttf. */
    return file_exists(IPODJS_UI_APPLE_ASSET_DIR
                       "/fast-scroll-blank.apple.95x82x32.bmp") &&
           file_exists(IPODJS_UI_APPLE_ASSET_DIR
                       "/fast-scroll-123.apple.95x82x32.bmp") &&
           file_exists(IPODJS_UI_APPLE_ASSET_DIR
                       "/23-Helvetica-Apple.fnt");
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
        TIME_AFTER(current_tick, ipodjs_ui_fast_scroll_deadline))
    {
        ipodjs_trace_fast_scroll(ipodjs_ui_fast_scroll_label, "expired");
        ipodjs_ui_fast_scroll_visible = false;
        return true;
    }
    return false;
}

void ipodjs_ui_draw_fast_scroll(struct screen *display)
{
    struct bitmap *overlay;
    int font;
    int width;
    int height;

    if (!display || !ipodjs_ui_fast_scroll_visible)
        return;

    overlay = ipodjs_ui_fast_scroll_overlay_asset(
        ipodjs_ui_fast_scroll_label[0] == '#');
    if (!overlay)
        return;
    /* Alpha bitmaps in DRMODE_SOLID blend against the viewport's background
     * pattern, which list row callbacks may leave set to arbitrary colors.
     * FG|IMG blends Apple's translucent overlay with the pixels already on
     * screen, matching the stock compositing behavior. */
    display->set_drawmode(DRMODE_FG);
    display->bmp(overlay, (display->lcdwidth - overlay->width) / 2,
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
    static int charge_font = -2;
    const char *path = IPODJS_UI_ASSET_DIR "/14-Adobe-Helvetica-Bold.fnt";
    const char *fallback = FONT_DIR "/14-Adobe-Helvetica-Bold.fnt";

    if (charge_font < 0 && file_exists(path))
        charge_font = font_load(path);
    if (charge_font < 0 && file_exists(fallback))
        charge_font = font_load(fallback);
    if (charge_font >= 0)
        font_lock(charge_font, true);

    return charge_font >= 0 ? charge_font : FONT_SYSFIXED;
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

static void ipodjs_ui_draw_usb_battery(struct screen *display, int x, int y)
{
    int level = MAX(0, MIN(100, battery_level()));
    int fill = 16 * level / 100;

    display->set_foreground(LCD_RGBPACK(218, 222, 226));
    display->drawrect(x, y, 21, 9);
    display->fillrect(x + 21, y + 3, 2, 4);
    display->set_foreground(LCD_RGBPACK(38, 43, 48));
    display->fillrect(x + 2, y + 2, 16, 5);
    if (fill > 0)
    {
        display->set_foreground(level <= 15 ?
            LCD_RGBPACK(220, 54, 48) : LCD_RGBPACK(122, 190, 76));
        display->fillrect(x + 2, y + 2, fill, 5);
    }
}

static void ipodjs_ui_draw_usb_lock(struct screen *display, int x, int y)
{
    unsigned color = LCD_RGBPACK(222, 225, 229);

    display->set_foreground(color);
    display->drawrect(x + 2, y, 7, 7);
    display->fillrect(x, y + 5, 11, 8);
    display->set_foreground(LCD_RGBPACK(42, 47, 52));
    display->fillrect(x + 5, y + 8, 1, 3);
}

static void ipodjs_ui_draw_thick_segment(struct screen *display,
                                         int x1, int y1, int x2, int y2,
                                         unsigned color)
{
    display->set_foreground(color);
    for (int offset = -3; offset <= 3; offset++)
    {
        display->drawline(x1 + offset, y1, x2 + offset, y2);
        display->drawline(x1, y1 + offset, x2, y2 + offset);
    }
}

static void ipodjs_ui_draw_usb_sync_mark(struct screen *display, int cx,
                                         int cy, bool dark)
{
    static const struct ipodjs_ui_point upper_arrow[] = {
        { 13, -10 }, { 27, -8 }, { 20, 5 }
    };
    static const struct ipodjs_ui_point lower_arrow[] = {
        { -13, 10 }, { -27, 8 }, { -20, -5 }
    };
    unsigned mark = dark ? LCD_RGBPACK(22, 25, 29) :
                           LCD_RGBPACK(34, 37, 40);

    ipodjs_ui_draw_thick_segment(display, cx - 20, cy - 5,
                                 cx - 17, cy - 14, mark);
    ipodjs_ui_draw_thick_segment(display, cx - 17, cy - 14,
                                 cx - 9, cy - 20, mark);
    ipodjs_ui_draw_thick_segment(display, cx - 9, cy - 20,
                                 cx + 3, cy - 22, mark);
    ipodjs_ui_draw_thick_segment(display, cx + 3, cy - 22,
                                 cx + 15, cy - 17, mark);
    ipodjs_ui_draw_thick_segment(display, cx + 15, cy - 17,
                                 cx + 20, cy - 8, mark);
    ipodjs_ui_fill_polygon(display, upper_arrow,
                           ARRAYLEN(upper_arrow), cx, cy, mark);

    ipodjs_ui_draw_thick_segment(display, cx + 20, cy + 5,
                                 cx + 17, cy + 14, mark);
    ipodjs_ui_draw_thick_segment(display, cx + 17, cy + 14,
                                 cx + 9, cy + 20, mark);
    ipodjs_ui_draw_thick_segment(display, cx + 9, cy + 20,
                                 cx - 3, cy + 22, mark);
    ipodjs_ui_draw_thick_segment(display, cx - 3, cy + 22,
                                 cx - 15, cy + 17, mark);
    ipodjs_ui_draw_thick_segment(display, cx - 15, cy + 17,
                                 cx - 20, cy + 8, mark);
    ipodjs_ui_fill_polygon(display, lower_arrow,
                           ARRAYLEN(lower_arrow), cx, cy, mark);
}

void ipodjs_ui_usb_prepare(void)
{
    int normal = ipodjs_ui_font();
    int bold = ipodjs_ui_charge_font();

    font_getstringsize("iPod", NULL, NULL, normal);
    font_getstringsize("Connected", NULL, NULL, bold);
    font_getstringsize("Eject Before Disconnecting", NULL, NULL, normal);
}

void ipodjs_ui_draw_usb_connected(struct screen *display)
{
    struct viewport *last_vp;
    bool dark = global_settings.ui_engine_dark_mode;
    int cx = display->lcdwidth / 2;
    int icon_cy = 101;
    int normal = ipodjs_ui_font();
    int bold = ipodjs_ui_charge_font();

    last_vp = display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(dark ? LCD_RGBPACK(3, 12, 21) :
                                   LCD_RGBPACK(4, 43, 64));
    display->clear_display();
    ipodjs_ui_glass_gradient(display, 0, 23, display->lcdwidth,
                             display->lcdheight - 23,
                             dark ? LCD_RGBPACK(18, 91, 128) :
                                    LCD_RGBPACK(29, 137, 181),
                             dark ? LCD_RGBPACK(5, 49, 76) :
                                    LCD_RGBPACK(5, 91, 132),
                             dark ? LCD_RGBPACK(1, 13, 24) :
                                    LCD_RGBPACK(1, 29, 48));

    ipodjs_ui_glass_gradient(display, 0, 0, display->lcdwidth, 23,
                             LCD_RGBPACK(70, 70, 70),
                             LCD_RGBPACK(32, 36, 40),
                             LCD_RGBPACK(8, 14, 19));
    display->set_foreground(LCD_RGBPACK(106, 116, 124));
    display->hline(0, display->lcdwidth - 1, 22);
    display->setfont(normal);
    display->set_foreground(LCD_RGBPACK(234, 237, 240));
    ipodjs_ui_puts_fit(display, 90, 3, display->lcdwidth - 180,
                       "iPod", true);
    ipodjs_ui_draw_usb_battery(display, display->lcdwidth - 32, 7);
    if (button_hold())
        ipodjs_ui_draw_usb_lock(display, 8, 5);

    ipodjs_ui_charge_rounded_gradient(display, cx - 43, icon_cy - 42,
                                      88, 88, 44,
                                      LCD_RGBPACK(10, 27, 36),
                                      LCD_RGBPACK(3, 14, 21),
                                      LCD_RGBPACK(1, 8, 13),
                                      cx - 43, 88);
    ipodjs_ui_charge_rounded_gradient(display, cx - 44, icon_cy - 44,
                                      88, 88, 44,
                                      LCD_RGBPACK(254, 223, 125),
                                      LCD_RGBPACK(222, 165, 55),
                                      LCD_RGBPACK(160, 100, 18),
                                      cx - 44, 88);
    ipodjs_ui_draw_usb_sync_mark(display, cx, icon_cy, dark);

    display->setfont(bold);
    display->set_foreground(LCD_RGBPACK(246, 248, 250));
    ipodjs_ui_puts_fit(display, 24, 167, display->lcdwidth - 48,
                       "Connected", true);
    display->setfont(normal);
    display->set_foreground(LCD_RGBPACK(224, 233, 239));
    ipodjs_ui_puts_fit(display, 20, 190, display->lcdwidth - 40,
                       "Eject Before Disconnecting", true);

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
