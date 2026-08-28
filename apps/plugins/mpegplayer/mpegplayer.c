/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * mpegplayer main entrypoint and UI implementation
 *
 * Copyright (c) 2007 Michael Sevakis
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

/****************************************************************************
 * NOTES:
 *
 * mpegplayer is structured as follows:
 *
 *                       +-->Video Thread-->Video Output-->LCD
 *                       |
 * UI-->Stream Manager-->+-->Audio Thread-->PCM buffer--Audio Device
 *         |       |     |                        |     (ref. clock)
 *         |       |     +-->Buffer Thread        |
 *    Stream Data  |             |          (clock intf./
 *     Requests    |         File Cache      drift adj.)
 *                 |          Disk I/O
 *         Stream services
 *          (timing, etc.)
 *
 * Thread list:
 *  1) The main thread - Handles user input, settings, basic playback control
 *     and USB connect.
 *
 *  2) Stream Manager thread - Handles playback state, events from streams
 *     such as when a stream is finished, stream commands, PCM state. The
 *     layer in which this thread run also handles arbitration of data
 *     requests between the streams and the disk buffer. The actual specific
 *     transport layer code may get moved out to support multiple container
 *     formats.
 *
 *  3) Buffer thread - Buffers data in the background, generates notifications
 *     to streams when their data has been buffered, and watches streams'
 *     progress to keep data available during playback. Handles synchronous
 *     random access requests when the file cache is missed.
 *
 *  4) Video thread (running on the COP for PortalPlayer targets) - Decodes
 *     the video stream and renders video frames to the LCD. Handles
 *     miscellaneous video tasks like frame and thumbnail printing.
 *
 *  5) Audio thread (running on the main CPU to maintain consistency with the
 *     audio FIQ hander on PP) - Decodes audio frames and places them into
 *     the PCM buffer for rendering by the audio device.
 *
 * Streams are neither aware of one another nor care about one another. All
 * streams shall have their own thread (unless it is _really_ efficient to
 * have a single thread handle a couple minor streams). All coordination of
 * the streams is done through the stream manager. The clocking is controlled
 * by and exposed by the stream manager to other streams and implemented at
 * the PCM level.
 *
 * Notes about MPEG files:
 *
 * MPEG System Clock is 27MHz - i.e. 27000000 ticks/second.
 *
 * FPS is represented in terms of a frame period - this is always an
 * integer number of 27MHz ticks.
 *
 * e.g. 29.97fps (30000/1001) NTSC video has an exact frame period of
 * 900900 27MHz ticks.
 *
 * In libmpeg2, info->sequence->frame_period contains the frame_period.
 *
 * Working with Rockbox's 100Hz tick, the common frame rates would need
 * to be as follows (1):
 *
 * FPS     | 27Mhz   | 100Hz          | 44.1KHz   | 48KHz
 * --------|-----------------------------------------------------------
 * 10*     | 2700000 | 10             | 4410      | 4800
 * 12*     | 2250000 |  8.3333        | 3675      | 4000
 * 15*     | 1800000 |  6.6667        | 2940      | 3200
 * 23.9760 | 1126125 |  4.170833333   | 1839.3375 | 2002
 * 24      | 1125000 |  4.166667      | 1837.5    | 2000
 * 25      | 1080000 |  4             | 1764      | 1920
 * 29.9700 |  900900 |  3.336667      | 1471,47   | 1601.6
 * 30      |  900000 |  3.333333      | 1470      | 1600
 *
 * *Unofficial framerates
 *
 * (1) But we don't really care since the audio clock is used anyway and has
 *     very fine resolution ;-)
 *****************************************************************************/
#include "plugin.h"
#include "mpegplayer.h"
#include "lib/helper.h"
#include "mpeg_settings.h"
#include "video_out.h"
#include "stream_thread.h"
#include "stream_mgr.h"
#include "livetv.h"
#include "../directv_boot.h"

#define MPEGPLAYER_NETFLIX_PREFIX "netflix:"
#define MPEGPLAYER_NETFLIX_PREFIX_LEN 8
#define MPEGPLAYER_MAPS_PREFIX "-mapsdash:"
#define MPEGPLAYER_MAPS_PREFIX_LEN (sizeof(MPEGPLAYER_MAPS_PREFIX) - 1)
#define MPEGPLAYER_NETFLIX_RESTART_PREFIX "netflix-restart:"
#define MPEGPLAYER_NETFLIX_RESTART_PREFIX_LEN 16
#define MPEGPLAYER_YOUTUBE_APP_PREFIX "youtube-app:"
#define MPEGPLAYER_YOUTUBE_APP_PREFIX_LEN 12
#define MPEGPLAYER_ONLYFANS_APP_PREFIX "onlyfans-app:"
#define MPEGPLAYER_ONLYFANS_APP_PREFIX_LEN 13
#define MPEGPLAYER_INSTAGRAM_APP_PREFIX "instagram-app:"
#define MPEGPLAYER_INSTAGRAM_APP_PREFIX_LEN 14
#define MPEGPLAYER_INSTAGRAM_FEED_PREFIX "instagram-feed:"
#define MPEGPLAYER_INSTAGRAM_FEED_PREFIX_LEN 15
#define MPEGPLAYER_REDDIT_APP_PREFIX "reddit-app:"
#define MPEGPLAYER_REDDIT_APP_PREFIX_LEN 11

bool mpegplayer_netflix_launch;
bool mpegplayer_youtube_launch;
bool mpegplayer_youtube_embedded;
static bool mpegplayer_youtube_app_launch;
static bool mpegplayer_onlyfans_app_launch;
static bool mpegplayer_instagram_app_launch;
static bool mpegplayer_instagram_feed_launch;
static bool mpegplayer_instagram_feed_expanded;
static int mpegplayer_instagram_return_direction;
static bool mpegplayer_instagram_return_profile;
static bool mpegplayer_reddit_app_launch;
bool mpegplayer_livetv_launch;
bool mpegplayer_livetv_pig;
bool mpegplayer_livetv_desktop;
bool mpegplayer_livetv_guide_active;
bool mpegplayer_livetv_pin_active;
bool mpegplayer_livetv_weather_hidden;
bool mpegplayer_livetv_weather_active;
static bool mpegplayer_maps_dashcam_launch;
static bool mpegplayer_livetv_weather_commercial;

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320) && (LCD_HEIGHT >= 240)
/* Desktop Mode and mpegplayer cannot remain loaded together. Preserve the
 * framebuffer handed to us by desktop_mode so stream reopen/OSD teardown can
 * restore the wallpaper and Dock without borrowing decoder or audio memory. */
#define LIVETV_DESKTOP_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_livetv_underlay.raw"
#define LIVETV_DESKTOP_UNDERLAY_MAGIC 0x44545631u /* "DTV1" */
static fb_data livetv_desktop_underlay[LCD_WIDTH * LCD_HEIGHT];
static bool livetv_desktop_underlay_valid;

static bool livetv_desktop_read_all(int fd, void *data, size_t size)
{
    unsigned char *cursor = data;

    while (size > 0)
    {
        ssize_t count = rb->read(fd, cursor, size);

        if (count <= 0)
            return false;
        cursor += count;
        size -= count;
    }
    return true;
}

static void livetv_desktop_capture_underlay(void)
{
    uint32_t header[3];
    struct viewport *vp =
        *(rb->screens[SCREEN_MAIN]->current_viewport);
    const fb_data *fb;
    int fd;
    int y;

    livetv_desktop_underlay_valid = false;

    fd = rb->open(LIVETV_DESKTOP_UNDERLAY_FILE, O_RDONLY);
    if (fd >= 0)
    {
        bool ok =
            livetv_desktop_read_all(fd, header, sizeof(header)) &&
            header[0] == LIVETV_DESKTOP_UNDERLAY_MAGIC &&
            header[1] == LCD_WIDTH && header[2] == LCD_HEIGHT &&
            livetv_desktop_read_all(
                fd, livetv_desktop_underlay,
                (size_t)LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data));

        rb->close(fd);
        if (ok)
        {
            livetv_desktop_underlay_valid = true;
            return;
        }
    }

    /* Keep direct launch useful even if no Desktop handoff file exists. */
    if (vp == NULL || vp->buffer == NULL || vp->buffer->fb_ptr == NULL)
        return;

    fb = vp->buffer->fb_ptr;
    for (y = 0; y < LCD_HEIGHT; y++)
    {
        rb->memcpy(livetv_desktop_underlay + y * LCD_WIDTH,
                   fb + y * vp->buffer->stride,
                   LCD_WIDTH * sizeof(fb_data));
    }
    livetv_desktop_underlay_valid = true;
}

static void livetv_desktop_restore_underlay(void)
{
    if (!livetv_desktop_underlay_valid)
        return;

    rb->lcd_bitmap(livetv_desktop_underlay, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rb->lcd_update();
}
#else
#define livetv_desktop_capture_underlay() do { } while (0)
#define livetv_desktop_restore_underlay() do { } while (0)
#endif

/* Join-in point for the programme Live TV is opening, in stream ticks. */
static uint32_t livetv_resume;

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320) && (LCD_HEIGHT >= 240)
#define YOUTUBE_LOGO_PATH \
    ROCKBOX_DIR "/offlineweb/assets/youtube-logo-2006.bmp"
#define YOUTUBE_PLAYER_PATH \
    ROCKBOX_DIR "/ipodjs/youtube/youtube-player-2007.bmp"
#define YOUTUBE_SEEK_KNOB_PATH \
    ROCKBOX_DIR "/ipodjs/youtube/youtube-player-seek-knob-2007.bmp"
#define YOUTUBE_VOLUME_KNOB_PATH \
    ROCKBOX_DIR "/ipodjs/youtube/youtube-player-volume-knob-2007.bmp"
#define INSTAGRAM_LIKES_PATH ROCKBOX_DIR "/instagram/likes.tsv"
#define INSTAGRAM_LIKES_TMP_PATH ROCKBOX_DIR "/instagram/likes.mpeg.tmp"
#define INSTAGRAM_LIBRARY_PATH ROCKBOX_DIR "/instagram/library.tsv"
static unsigned char youtube_logo_data[128 * 52 * sizeof(fb_data)];
static struct bitmap youtube_logo_bmp;
static unsigned char youtube_player_data[320 * 28 * sizeof(fb_data)];
static struct bitmap youtube_player_bmp;
static unsigned char youtube_seek_knob_data[16 * 19 * sizeof(fb_data)];
static struct bitmap youtube_seek_knob_bmp;
static unsigned char youtube_volume_knob_data[9 * 18 * sizeof(fb_data)];
static struct bitmap youtube_volume_knob_bmp;
static bool youtube_player_valid;
static bool youtube_seek_knob_valid;
static bool youtube_volume_knob_valid;
#include "pluginbitmaps/instagram_video_play.h"
#include "pluginbitmaps/instagram_heart.h"
#include "pluginbitmaps/instagram_heart_unliked.h"
static char instagram_group_id[32];
static bool instagram_liked;
/* Feed chrome is intentionally small and fixed.  It is populated once while
 * opening a clip, then painted by the YUV compositor without filesystem I/O. */
static char instagram_username[40];
static char instagram_caption[88];
static int instagram_likes;
static char youtube_title[64];
static char youtube_uploader[40];
static char youtube_added[24];
static char youtube_views[32];
static char youtube_duration[20];
#endif

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320) && (LCD_HEIGHT >= 240)
#include "pluginbitmaps/ipodtiktok_header.h"
#include "pluginbitmaps/ipodtiktok_scrim.h"
#include "pluginbitmaps/ipodtiktok_heart.h"
#include "pluginbitmaps/ipodtiktok_heart_outline.h"
#include "pluginbitmaps/ipodtiktok_check.h"
#include "pluginbitmaps/ipodtiktok_verified.h"
#define IPODTIKTOK_USE_BITMAP_ASSETS 1
#else
#define IPODTIKTOK_USE_BITMAP_ASSETS 0
#endif

#define MPLOG(...) DEBUGF("mpegplayer: " __VA_ARGS__)

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320) && (LCD_HEIGHT >= 240)
static void youtube_load_metadata(const char *videofile)
{
    char path[MAX_PATH];
    char line[128];
    char *dot;
    int fd;

    youtube_title[0] = '\0';
    youtube_uploader[0] = '\0';
    youtube_added[0] = '\0';
    youtube_views[0] = '\0';
    youtube_duration[0] = '\0';
    youtube_player_valid = false;
    youtube_seek_knob_valid = false;
    youtube_volume_knob_valid = false;
    if (mpegplayer_youtube_app_launch)
    {
        youtube_player_bmp.data = youtube_player_data;
        youtube_player_valid =
            rb->read_bmp_file(YOUTUBE_PLAYER_PATH, &youtube_player_bmp,
                              sizeof(youtube_player_data),
                              FORMAT_NATIVE, NULL) > 0 &&
            youtube_player_bmp.width == 320 &&
            youtube_player_bmp.height == 28;
        youtube_seek_knob_bmp.data = youtube_seek_knob_data;
        youtube_seek_knob_valid =
            rb->read_bmp_file(YOUTUBE_SEEK_KNOB_PATH,
                              &youtube_seek_knob_bmp,
                              sizeof(youtube_seek_knob_data),
                              FORMAT_NATIVE, NULL) > 0 &&
            youtube_seek_knob_bmp.width == 16 &&
            youtube_seek_knob_bmp.height == 19;
        youtube_volume_knob_bmp.data = youtube_volume_knob_data;
        youtube_volume_knob_valid =
            rb->read_bmp_file(YOUTUBE_VOLUME_KNOB_PATH,
                              &youtube_volume_knob_bmp,
                              sizeof(youtube_volume_knob_data),
                              FORMAT_NATIVE, NULL) > 0 &&
            youtube_volume_knob_bmp.width == 9 &&
            youtube_volume_knob_bmp.height == 18;
    }
    rb->strlcpy(path, videofile, sizeof(path));
    dot = rb->strrchr(path, '.');
    if (dot == NULL)
        return;
    rb->strlcpy(dot, ".ytm", sizeof(path) - (dot - path));
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        if (!rb->strncmp(line, "title=", 6))
            rb->strlcpy(youtube_title, line + 6, sizeof(youtube_title));
        else if (!rb->strncmp(line, "uploader=", 9))
            rb->strlcpy(youtube_uploader, line + 9,
                        sizeof(youtube_uploader));
        else if (!rb->strncmp(line, "added=", 6))
            rb->strlcpy(youtube_added, line + 6, sizeof(youtube_added));
        else if (!rb->strncmp(line, "views=", 6))
            rb->strlcpy(youtube_views, line + 6, sizeof(youtube_views));
        else if (!rb->strncmp(line, "duration=", 9))
            rb->strlcpy(youtube_duration, line + 9,
                        sizeof(youtube_duration));
    }
    rb->close(fd);
}

static void instagram_copy_metadata_value(char *target, size_t size,
                                          const char *value)
{
    char *end;

    rb->strlcpy(target, value, size);
    end = target;
    while (*end && *end != '\r' && *end != '\n')
        end++;
    *end = '\0';
}

static void instagram_load_metadata(const char *videofile)
{
    char path[MAX_PATH];
    char line[1024];
    char *dot;
    int fd;

    instagram_group_id[0] = '\0';
    instagram_liked = false;
    instagram_username[0] = '\0';
    instagram_caption[0] = '\0';
    instagram_likes = 0;
    rb->strlcpy(path, videofile, sizeof(path));
    dot = rb->strrchr(path, '.');
    if (dot != NULL)
    {
        rb->strlcpy(dot, ".igm", sizeof(path) - (dot - path));
        fd = rb->open(path, O_RDONLY);
        if (fd >= 0)
        {
            while (rb->read_line(fd, line, sizeof(line)) > 0)
            {
                if (!rb->strncmp(line, "group_id=", 9))
                    instagram_copy_metadata_value(
                        instagram_group_id, sizeof(instagram_group_id),
                        line + 9);
            }
            rb->close(fd);
        }
    }

    if (!instagram_group_id[0])
        goto load_feed_details;
    fd = rb->open(INSTAGRAM_LIKES_PATH, O_RDONLY);
    if (fd >= 0)
    {
        while (rb->read_line(fd, line, sizeof(line)) > 0)
        {
            instagram_copy_metadata_value(path, sizeof(path), line);
            if (!rb->strcmp(path, instagram_group_id))
            {
                instagram_liked = true;
                break;
            }
        }
        rb->close(fd);
    }

load_feed_details:
    /* Older syncs only wrote group_id into .igm.  Resolve the rest from the
     * canonical device library at launch, never while composing frames. */
    fd = rb->open(INSTAGRAM_LIBRARY_PATH, O_RDONLY);
    if (fd < 0)
        return;
    rb->read_line(fd, line, sizeof(line)); /* TSV header */
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *field[15];
        int count = 1;
        int index;

        field[0] = line;
        for (index = 1; index < (int)ARRAYLEN(field); index++)
        {
            char *tab = rb->strchr(field[index - 1], '\t');
            if (tab == NULL)
                break;
            *tab = '\0';
            field[index] = tab + 1;
            count++;
        }
        if (count < 15 ||
            (rb->strcmp(field[5], videofile) &&
             rb->strcmp(field[14], videofile)))
            continue;
        instagram_copy_metadata_value(instagram_username,
                                      sizeof(instagram_username), field[1]);
        instagram_copy_metadata_value(instagram_caption,
                                      sizeof(instagram_caption),
                                      field[4][0] ? field[4] : field[3]);
        instagram_likes = rb->atoi(field[8]);
        break;
    }
    rb->close(fd);
}

static bool instagram_toggle_like(void)
{
    char line[96];
    char clean[96];
    int source;
    int target;
    bool found = false;

    if (!instagram_group_id[0])
        return false;
    source = rb->open(INSTAGRAM_LIKES_PATH, O_RDONLY);
    target = rb->open(INSTAGRAM_LIKES_TMP_PATH,
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (target < 0)
    {
        if (source >= 0)
            rb->close(source);
        return false;
    }
    if (source >= 0)
    {
        while (rb->read_line(source, line, sizeof(line)) > 0)
        {
            instagram_copy_metadata_value(clean, sizeof(clean), line);
            if (!rb->strcmp(clean, instagram_group_id))
            {
                found = true;
                if (instagram_liked)
                    continue;
            }
            rb->fdprintf(target, "%s\n", clean);
        }
        rb->close(source);
    }
    if (!instagram_liked && !found)
        rb->fdprintf(target, "%s\n", instagram_group_id);
    rb->close(target);
    rb->remove(INSTAGRAM_LIKES_PATH);
    if (rb->rename(INSTAGRAM_LIKES_TMP_PATH, INSTAGRAM_LIKES_PATH) < 0)
    {
        rb->remove(INSTAGRAM_LIKES_TMP_PATH);
        return false;
    }
    instagram_liked = !instagram_liked;
    return true;
}

static void youtube_draw_embedded_chrome(void)
{
    int oldfg = rb->lcd_get_foreground();
    int oldbg = rb->lcd_get_background();

    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    youtube_logo_bmp.data = youtube_logo_data;
    if (rb->read_bmp_file(YOUTUBE_LOGO_PATH, &youtube_logo_bmp,
                          sizeof(youtube_logo_data),
                          FORMAT_NATIVE, NULL) > 0)
        rb->lcd_bitmap((const fb_data *)youtube_logo_bmp.data, 8, 0,
                       youtube_logo_bmp.width, youtube_logo_bmp.height);
    rb->lcd_set_foreground(LCD_RGBPACK(0x00, 0x33, 0xcc));
    rb->lcd_putsxy(220, 5, "Videos");
    rb->lcd_putsxy(220, 21, "Channels");
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_hline(0, LCD_WIDTH - 1, 43);
    rb->lcd_putsxy(5, 46, youtube_title[0] ?
                   youtube_title : "YouTube Video");
    rb->lcd_set_foreground(LCD_RGBPACK(0xe6, 0xf1, 0xfa));
    rb->lcd_fillrect(218, 62, LCD_WIDTH - 218, 158);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy(223, 67, "From:");
    rb->lcd_putsxy(223, 83, youtube_uploader);
    if (youtube_added[0])
    {
        rb->lcd_putsxy(223, 104, "Added:");
        rb->lcd_putsxy(223, 120, youtube_added);
    }
    rb->lcd_putsxy(223, 145, youtube_views);
    if (youtube_duration[0])
        rb->lcd_putsxy(223, 181, youtube_duration);
    rb->lcd_set_foreground(LCD_RGBPACK(0x00, 0x33, 0xcc));
    rb->lcd_fillrect(0, 220, LCD_WIDTH, 20);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy(6, 224, "Select Full Screen");
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_update();
    rb->lcd_set_foreground(oldfg);
    rb->lcd_set_background(oldbg);
}
#else
#define youtube_load_metadata(videofile)
#define youtube_draw_embedded_chrome()
#define instagram_load_metadata(videofile)
#define instagram_toggle_like() false
#endif


/* button definitions */
#if (CONFIG_KEYPAD == IRIVER_H100_PAD) || (CONFIG_KEYPAD == IRIVER_H300_PAD)
#define MPEG_MENU       BUTTON_MODE
#define MPEG_STOP       BUTTON_OFF
#define MPEG_PAUSE      BUTTON_ON
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif (CONFIG_KEYPAD == IPOD_4G_PAD) || (CONFIG_KEYPAD == IPOD_3G_PAD) || \
      (CONFIG_KEYPAD == IPOD_1G2G_PAD)
#define MPEG_MENU       BUTTON_MENU
#define MPEG_PAUSE      (BUTTON_PLAY | BUTTON_REL)
#define MPEG_STOP       (BUTTON_PLAY | BUTTON_REPEAT)
#define MPEG_VOLDOWN    BUTTON_SCROLL_BACK
#define MPEG_VOLUP      BUTTON_SCROLL_FWD
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT
#define MPEG_ZOOM       (BUTTON_SELECT | BUTTON_REL)
#ifdef BUTTON_RC_PLAY
/* 30-pin iAP accessories (including legacy video docks) arrive as the
 * target's BUTTON_RC_* values.  Keep them alongside, rather than replacing,
 * the clickwheel controls. */
#define MPEG_RC_MENU    BUTTON_RC_MENU
#define MPEG_RC_STOP    BUTTON_RC_STOP
#define MPEG_RC_PAUSE   (BUTTON_RC_PLAY | BUTTON_REL)
#define MPEG_RC_VOLDOWN BUTTON_RC_VOL_DOWN
#define MPEG_RC_VOLUP   BUTTON_RC_VOL_UP
#define MPEG_RC_DOWN    BUTTON_RC_DOWN
#define MPEG_RC_UP      BUTTON_RC_UP
#define MPEG_RC_RW      BUTTON_RC_LEFT
#define MPEG_RC_FF      BUTTON_RC_RIGHT
#define MPEG_RC_ZOOM    (BUTTON_RC_SELECT | BUTTON_REL)
#define MPEG_RC_GUIDE   BUTTON_RC_PLAY
#endif

#elif CONFIG_KEYPAD == IAUDIO_X5M5_PAD
#define MPEG_MENU       (BUTTON_REC | BUTTON_REL)
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == GIGABEAT_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_SELECT
#define MPEG_PAUSE2     BUTTON_A
#define MPEG_VOLDOWN    BUTTON_LEFT
#define MPEG_VOLUP      BUTTON_RIGHT
#define MPEG_VOLDOWN2   BUTTON_VOL_DOWN
#define MPEG_VOLUP2     BUTTON_VOL_UP
#define MPEG_RW         BUTTON_UP
#define MPEG_FF         BUTTON_DOWN

#define MPEG_RC_MENU    BUTTON_RC_DSP
#define MPEG_RC_STOP    (BUTTON_RC_PLAY | BUTTON_REPEAT)
#define MPEG_RC_PAUSE   (BUTTON_RC_PLAY | BUTTON_REL)
#define MPEG_RC_VOLDOWN BUTTON_RC_VOL_DOWN
#define MPEG_RC_VOLUP   BUTTON_RC_VOL_UP
#define MPEG_RC_RW      BUTTON_RC_REW
#define MPEG_RC_FF      BUTTON_RC_FF

#elif CONFIG_KEYPAD == GIGABEAT_S_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_SELECT
#define MPEG_PAUSE2     BUTTON_PLAY
#define MPEG_VOLDOWN    BUTTON_LEFT
#define MPEG_VOLUP      BUTTON_RIGHT
#define MPEG_VOLDOWN2   BUTTON_VOL_DOWN
#define MPEG_VOLUP2     BUTTON_VOL_UP
#define MPEG_RW         BUTTON_UP
#define MPEG_RW2        BUTTON_PREV
#define MPEG_FF         BUTTON_DOWN
#define MPEG_FF2        BUTTON_NEXT
#define MPEG_SHOW_OSD   BUTTON_BACK

#define MPEG_RC_MENU    BUTTON_RC_DSP
#define MPEG_RC_STOP    (BUTTON_RC_PLAY | BUTTON_REPEAT)
#define MPEG_RC_PAUSE   (BUTTON_RC_PLAY | BUTTON_REL)
#define MPEG_RC_VOLDOWN BUTTON_RC_VOL_DOWN
#define MPEG_RC_VOLUP   BUTTON_RC_VOL_UP
#define MPEG_RC_RW      BUTTON_RC_REW
#define MPEG_RC_FF      BUTTON_RC_FF

#elif CONFIG_KEYPAD == IRIVER_H10_PAD
#define MPEG_MENU       BUTTON_LEFT
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_VOLDOWN    BUTTON_SCROLL_DOWN
#define MPEG_VOLUP      BUTTON_SCROLL_UP
#define MPEG_RW         BUTTON_REW
#define MPEG_FF         BUTTON_FF

#elif CONFIG_KEYPAD == SANSA_E200_PAD
#define MPEG_MENU       BUTTON_SELECT
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_RIGHT
#define MPEG_VOLDOWN    BUTTON_SCROLL_BACK
#define MPEG_VOLUP      BUTTON_SCROLL_FWD
#define MPEG_RW         BUTTON_UP
#define MPEG_FF         BUTTON_DOWN

#elif CONFIG_KEYPAD == SANSA_FUZE_PAD
#define MPEG_MENU       BUTTON_SELECT
#define MPEG_STOP       (BUTTON_HOME|BUTTON_REPEAT)
#define MPEG_PAUSE      BUTTON_UP
#define MPEG_VOLDOWN    BUTTON_SCROLL_BACK
#define MPEG_VOLUP      BUTTON_SCROLL_FWD
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT


#elif CONFIG_KEYPAD == SANSA_C200_PAD || \
CONFIG_KEYPAD == SANSA_CLIP_PAD || \
CONFIG_KEYPAD == SANSA_M200_PAD
#define MPEG_MENU       BUTTON_SELECT
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_UP
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == MROBE500_PAD
#define MPEG_STOP       BUTTON_POWER

#define MPEG_RC_MENU    BUTTON_RC_HEART
#define MPEG_RC_STOP    BUTTON_RC_DOWN
#define MPEG_RC_PAUSE   BUTTON_RC_PLAY
#define MPEG_RC_VOLDOWN BUTTON_RC_VOL_DOWN
#define MPEG_RC_VOLUP   BUTTON_RC_VOL_UP
#define MPEG_RC_RW      BUTTON_RC_REW
#define MPEG_RC_FF      BUTTON_RC_FF

#elif CONFIG_KEYPAD == MROBE100_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == IAUDIO_M3_PAD
#define MPEG_MENU       BUTTON_RC_MENU
#define MPEG_STOP       BUTTON_RC_REC
#define MPEG_PAUSE      BUTTON_RC_PLAY
#define MPEG_VOLDOWN    BUTTON_RC_VOL_DOWN
#define MPEG_VOLUP      BUTTON_RC_VOL_UP
#define MPEG_RW         BUTTON_RC_REW
#define MPEG_FF         BUTTON_RC_FF

#elif CONFIG_KEYPAD == COWON_D2_PAD
#define MPEG_MENU       (BUTTON_MENU|BUTTON_REL)
//#define MPEG_STOP       BUTTON_POWER
#define MPEG_VOLDOWN    BUTTON_MINUS
#define MPEG_VOLUP      BUTTON_PLUS

#elif CONFIG_KEYPAD ==  CREATIVE_ZENXFI3_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       (BUTTON_PLAY|BUTTON_REPEAT)
#define MPEG_PAUSE      (BUTTON_PLAY|BUTTON_REL)
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_DOWN
#define MPEG_FF         BUTTON_UP

#elif CONFIG_KEYPAD == PHILIPS_HDD1630_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_SELECT
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == PHILIPS_HDD6330_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_PREV
#define MPEG_FF         BUTTON_NEXT

#elif CONFIG_KEYPAD == PHILIPS_SA9200_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_UP
#define MPEG_FF         BUTTON_DOWN

#elif CONFIG_KEYPAD == ONDAVX747_PAD
#define MPEG_MENU       (BUTTON_MENU|BUTTON_REL)
//#define MPEG_STOP       BUTTON_POWER
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP

#elif CONFIG_KEYPAD == ONDAVX777_PAD
#define MPEG_MENU       BUTTON_POWER

#elif (CONFIG_KEYPAD == SAMSUNG_YH820_PAD) || \
      (CONFIG_KEYPAD == SAMSUNG_YH92X_PAD)
#define MPEG_MENU       BUTTON_REW
#define MPEG_STOP       (BUTTON_PLAY | BUTTON_REPEAT)
#define MPEG_PAUSE      (BUTTON_PLAY | BUTTON_REL)
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT
#define MPEG_SHOW_OSD   BUTTON_FFWD

#elif CONFIG_KEYPAD == PBELL_VIBE500_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       BUTTON_REC
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_PREV
#define MPEG_FF         BUTTON_NEXT

#elif CONFIG_KEYPAD == MPIO_HD200_PAD
#define MPEG_MENU       BUTTON_FUNC
#define MPEG_PAUSE      (BUTTON_PLAY | BUTTON_REL)
#define MPEG_STOP       (BUTTON_PLAY | BUTTON_REPEAT)
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_REW
#define MPEG_FF         BUTTON_FF

#elif CONFIG_KEYPAD == MPIO_HD300_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_PAUSE      (BUTTON_PLAY | BUTTON_REL)
#define MPEG_STOP       (BUTTON_PLAY | BUTTON_REPEAT)
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_REW
#define MPEG_FF         BUTTON_FF

#elif CONFIG_KEYPAD == SANSA_FUZEPLUS_PAD
#define MPEG_MENU       BUTTON_POWER
#define MPEG_PAUSE      (BUTTON_PLAYPAUSE | BUTTON_REL)
#define MPEG_STOP       (BUTTON_PLAYPAUSE | BUTTON_REPEAT)
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == SANSA_CONNECT_PAD
#define MPEG_MENU       BUTTON_POWER
#define MPEG_PAUSE      (BUTTON_SELECT | BUTTON_REL)
#define MPEG_STOP       (BUTTON_SELECT | BUTTON_REPEAT)
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == SAMSUNG_YPR0_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_PAUSE      BUTTON_SELECT
#define MPEG_STOP       BUTTON_POWER
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == HM60X_PAD
#define MPEG_MENU       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_SELECT
#define MPEG_STOP       (BUTTON_SELECT | BUTTON_POWER)
#define MPEG_VOLDOWN    (BUTTON_POWER | BUTTON_DOWN)
#define MPEG_VOLUP      (BUTTON_POWER | BUTTON_UP)
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == HM801_PAD
#define MPEG_MENU       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_STOP       (BUTTON_POWER | BUTTON_PLAY)
#define MPEG_VOLDOWN    (BUTTON_POWER | BUTTON_DOWN)
#define MPEG_VOLUP      (BUTTON_POWER | BUTTON_UP)
#define MPEG_RW         BUTTON_PREV
#define MPEG_FF         BUTTON_NEXT

#elif CONFIG_KEYPAD == SONY_NWZ_PAD
#define MPEG_MENU       BUTTON_BACK
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_STOP       BUTTON_POWER
#define MPEG_VOLDOWN    BUTTON_UP
#define MPEG_VOLUP      BUTTON_DOWN
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == CREATIVE_ZEN_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_PAUSE      BUTTON_PLAYPAUSE
#define MPEG_STOP       BUTTON_BACK
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == DX50_PAD
#define MPEG_MENU       BUTTON_POWER
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_STOP       (BUTTON_PLAY|BUTTON_REPEAT)

#elif CONFIG_KEYPAD == CREATIVE_ZENXFI2_PAD
#define MPEG_MENU       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_MENU
#define MPEG_STOP       (BUTTON_MENU|BUTTON_REPEAT)

#elif CONFIG_KEYPAD == AGPTEK_ROCKER_PAD
#define MPEG_MENU       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_SELECT
#define MPEG_STOP       BUTTON_DOWN
#define MPEG_VOLDOWN    BUTTON_VOLDOWN
#define MPEG_VOLUP      BUTTON_VOLUP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == XDUOO_X3_PAD
#define MPEG_MENU       BUTTON_PLAY
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_HOME
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_PREV
#define MPEG_FF         BUTTON_NEXT

#elif CONFIG_KEYPAD == XDUOO_X3II_PAD || CONFIG_KEYPAD == XDUOO_X20_PAD
#define MPEG_MENU       BUTTON_PLAY
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_HOME
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_PREV
#define MPEG_FF         BUTTON_NEXT

#elif CONFIG_KEYPAD == FIIO_M3K_LINUX_PAD
#define MPEG_MENU       BUTTON_PLAY
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_HOME
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_PREV
#define MPEG_FF         BUTTON_NEXT

#elif CONFIG_KEYPAD == IHIFI_770_PAD || CONFIG_KEYPAD == IHIFI_800_PAD
#define MPEG_MENU       BUTTON_PLAY
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_HOME
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_PREV
#define MPEG_FF         BUTTON_NEXT

#elif CONFIG_KEYPAD == EROSQ_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_PREV
#define MPEG_FF         BUTTON_NEXT

#elif CONFIG_KEYPAD == FIIO_M3K_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       BUTTON_POWER
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_VOLDOWN    BUTTON_VOL_DOWN
#define MPEG_VOLUP      BUTTON_VOL_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == MA_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_STOP       BUTTON_BACK
#define MPEG_PAUSE      BUTTON_PLAY
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == SHANLING_Q1_PAD || CONFIG_KEYPAD == HIBY_R3PROII_PAD
/* use touchscreen */

#elif CONFIG_KEYPAD == RG_NANO_PAD
#define MPEG_MENU       BUTTON_START
#define MPEG_STOP       BUTTON_X
#define MPEG_PAUSE      BUTTON_A
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#elif CONFIG_KEYPAD == CTRU_PAD
#define MPEG_MENU       BUTTON_MENU
#define MPEG_PAUSE      BUTTON_SELECT
#define MPEG_STOP       BUTTON_POWER
#define MPEG_VOLDOWN    BUTTON_DOWN
#define MPEG_VOLUP      BUTTON_UP
#define MPEG_RW         BUTTON_LEFT
#define MPEG_FF         BUTTON_RIGHT

#else
#error No keymap defined!
#endif

#ifdef HAVE_TOUCHSCREEN
#ifndef MPEG_MENU
#define MPEG_MENU      (BUTTON_TOPRIGHT|BUTTON_REL)
#endif
#ifndef MPEG_STOP
#define MPEG_STOP       BUTTON_TOPLEFT
#endif
#ifndef MPEG_PAUSE
#define MPEG_PAUSE      BUTTON_CENTER
#endif
#ifndef MPEG_VOLDOWN
#define MPEG_VOLDOWN    BUTTON_BOTTOMMIDDLE
#endif
#ifndef MPEG_VOLUP
#define MPEG_VOLUP      BUTTON_TOPMIDDLE
#endif
#ifndef MPEG_RW
#define MPEG_RW         BUTTON_MIDLEFT
#endif
#ifndef MPEG_FF
#define MPEG_FF         BUTTON_MIDRIGHT
#endif
#endif

/* One thing we can do here for targets with remotes is having a display
 * always on the remote instead of always forcing a popup on the main display */

#define FF_REWIND_MAX_PERCENT 3 /* cap ff/rewind step size at max % of file */
                                /* 3% of 30min file == 54s step size */
#define MIN_FF_REWIND_STEP (TS_SECOND/2)
#define FF_REWIND_BUTTON_TIMEOUT (HZ/2)
#define OSD_MIN_UPDATE_INTERVAL (HZ/2)
#define FPS_UPDATE_INTERVAL (HZ) /* Get new FPS reading each second */

enum video_action
{
    VIDEO_STOP = 0,
    VIDEO_PREV,
    VIDEO_NEXT,
    VIDEO_REPEAT,
    VIDEO_ACTION_MANUAL = 0x8000, /* Flag that says user did it */
};

/* OSD status - same order as icon array */
enum osd_status_enum
{
    OSD_STATUS_STOPPED = 0,
    OSD_STATUS_PAUSED,
    OSD_STATUS_PLAYING,
    OSD_STATUS_FF,
    OSD_STATUS_RW,
    OSD_STATUS_COUNT,
    OSD_STATUS_MASK = 0x7
};

enum osd_bits
{
    OSD_REFRESH_DEFAULT    = 0x0000, /* Only refresh elements when due */
                                     /* Refresh the... */
    OSD_REFRESH_VOLUME     = 0x0001, /* ...volume display */
    OSD_REFRESH_TIME       = 0x0002, /* ...time display+progress */
    OSD_REFRESH_STATUS     = 0x0004, /* ...playback status icon */
    OSD_REFRESH_BACKGROUND = 0x0008, /* ...background (implies ALL) */
    OSD_REFRESH_VIDEO      = 0x0010, /* ...video image upon timeout */
    OSD_REFRESH_RESUME     = 0x0020, /* Resume playback upon timeout */
    OSD_NODRAW             = 0x8000, /* OR bitflag - don't draw anything */
    OSD_SHOW               = 0x4000, /* OR bitflag - show the OSD */
#ifdef HAVE_HEADPHONE_DETECTION
    OSD_HP_PAUSE           = 0x2000, /* OR bitflag - headphones caused pause */
#endif
    OSD_HIDE               = 0x0000, /* hide the OSD (aid readability) */
    OSD_REFRESH_ALL        = 0x000f, /* Only immediate graphical elements */
};

/* Status icons selected according to font height */
extern const unsigned char mpegplayer_status_icons_8x8x1[];
extern const unsigned char mpegplayer_status_icons_12x12x1[];
extern const unsigned char mpegplayer_status_icons_16x16x1[];

#if IPODTIKTOK_USE_BITMAP_ASSETS
#define FEED_HEADER_HEIGHT      BMPHEIGHT_ipodtiktok_header
#define FEED_SCRIM_HEIGHT       BMPHEIGHT_ipodtiktok_scrim
#define FEED_HEART_SIZE         BMPWIDTH_ipodtiktok_heart
#else
#define FEED_HEADER_HEIGHT      28
#define FEED_SCRIM_HEIGHT       80
#define FEED_HEART_SIZE         28
#endif

/* Main border areas that contain OSD elements */
#define OSD_BDR_L 4
#define OSD_BDR_T 4
#define OSD_BDR_R 4
#define OSD_BDR_B 4

#if defined(HAVE_LCD_COLOR) && LCD_WIDTH == 320 && LCD_HEIGHT == 240
#define MPEG_STOCK_CONTROLS 1
#define MPEG_STOCK_ASSET_DIR ROCKBOX_DIR "/ipodjs/apple"
#define MPEG_STOCK_HEADER_H 24
#define MPEG_STOCK_TITLE_SIZE 96
#define MPEG_STOCK_PLAYBACK_W 20
#define MPEG_STOCK_PLAYBACK_FRAME_H 16
#define MPEG_STOCK_BATTERY_W 26
#define MPEG_STOCK_BATTERY_FRAME_H 13
#define MPEG_STOCK_BATTERY_FRAMES 5
#define MPEG_STOCK_PROGRESS_W 200
#define MPEG_STOCK_PROGRESS_H 22
#define MPEG_STOCK_PROGRESS_CAP_W 16
#define MPEG_STOCK_PROGRESS_CAP_H 16
#define MPEG_VOLUME_CARD_W 180
#define MPEG_VOLUME_CARD_H 45
#define MPEG_VOLUME_ICON_W 24
#define MPEG_VOLUME_ICON_H 21
#define MPEG_VOLUME_ICON_FRAMES 4
#define MPEG_VOLUME_SLIDER_W 117
#define MPEG_VOLUME_SLIDER_H 5
#define MPEG_VOLUME_SLIDER_END_W 3
#define MPEG_NETFLIX_OVERLAY_H 64
#define MPEG_YOUTUBE_OVERLAY_H 28
#define MPEG_YOUTUBE_PROGRESS_LEFT 47
#define MPEG_YOUTUBE_PROGRESS_W 126
#define MPEG_YOUTUBE_PROGRESS_ERASE_X 35
#define MPEG_YOUTUBE_PROGRESS_ERASE_Y 4
#define MPEG_YOUTUBE_PROGRESS_ERASE_W 21
#define MPEG_YOUTUBE_PROGRESS_ERASE_H 21
#define MPEG_YOUTUBE_PROGRESS_KNOB_Y 5
#define MPEG_YOUTUBE_PROGRESS_KNOB_W 16
#define MPEG_YOUTUBE_PROGRESS_KNOB_H 19
#define MPEG_YOUTUBE_VOLUME_LEFT 230
#define MPEG_YOUTUBE_VOLUME_W 38
#define MPEG_YOUTUBE_VOLUME_ERASE_X 262
#define MPEG_YOUTUBE_VOLUME_ERASE_W 13
#define MPEG_YOUTUBE_VOLUME_KNOB_Y 5
#define MPEG_YOUTUBE_VOLUME_KNOB_W 9
#define MPEG_YOUTUBE_VOLUME_KNOB_H 18
#define MPEG_INSTAGRAM_VIDEO_LEFT 4
#define MPEG_INSTAGRAM_VIDEO_TOP 57
#define MPEG_INSTAGRAM_VIDEO_W 160
#define MPEG_INSTAGRAM_VIDEO_H 158
#define MPEG_INSTAGRAM_PLAY_MARGIN 6
#define MPEG_NETFLIX_SKIP_H 50
#else
#define MPEG_STOCK_CONTROLS 0
#endif

struct osd
{
    long hide_tick;
    long show_for;
    long print_tick;
    long print_delay;
    long resume_tick;
    long resume_delay;
    long next_auto_refresh;
    int x;
    int y;
    int width;
    int height;
    unsigned fgcolor;
    unsigned bgcolor;
    unsigned prog_fillcolor;
    struct vo_rect update_rect;
    struct vo_rect prog_rect;
    struct vo_rect time_rect;
    struct vo_rect dur_rect;
    struct vo_rect vol_rect;
    const unsigned char *icons;
    struct vo_rect stat_rect;
    int status;
    uint32_t curr_time;
    unsigned auto_refresh;
    unsigned flags;
    int font;
    /* iPone WPS slider assets */
    struct bitmap slider_bg_bmp;
    struct bitmap slider_knob_bmp;
    bool slider_bitmaps_loaded;
    bool use_wps_layout;
    bool stock_layout;
    bool netflix_layout;
    bool youtube_layout;
    bool instagram_layout;
};

struct fps
{
    /* FPS Display */
    struct vo_rect rect;    /* OSD coordinates */
    int pf_x;               /* Screen coordinates */
    int pf_y;
    int pf_width;
    int pf_height;
    long update_tick;       /* When to next update FPS reading */
    #define FPS_FORMAT  "%d.%02d"
    #define FPS_DIMSTR  "999.99" /* For establishing rect size */
    #define FPS_BUFSIZE sizeof("999.99")
};

static struct osd osd;
static struct fps fps NOCACHEBSS_ATTR; /* Accessed on other processor */
static char mpeg_osd_path[MAX_PATH];
static uint32_t netflix_overlay_duration;
enum netflix_skip_kind
{
    NETFLIX_SKIP_NONE = 0,
    NETFLIX_SKIP_INTRO,
    NETFLIX_SKIP_CREDITS,
};
static enum netflix_skip_kind netflix_skip_active;
static uint32_t netflix_intro_start;
static uint32_t netflix_intro_end;
static uint32_t netflix_credits_start;
static uint32_t netflix_credits_duration;
/* Set by every user-initiated stop, so the natural fall-through out of the
 * button loop can be recognised as end of stream. */
static bool mpeg_stop_requested;

#if MPEG_STOCK_CONTROLS
struct mpeg_stock_assets
{
    bool tried;
    bool loaded;
    struct bitmap header;
    struct bitmap playback;
    struct bitmap battery;
    struct bitmap progress_frame;
    struct bitmap progress_fill;
    struct bitmap progress_fill_cap;
};

struct mpeg_volume_assets
{
    bool tried;
    bool loaded;
    struct bitmap backdrop;
    struct bitmap icons;
    struct bitmap slider_backdrop;
    struct bitmap slider_fill;
    struct bitmap slider_end;
};

static struct mpeg_stock_assets mpeg_stock_assets;
static struct mpeg_volume_assets mpeg_volume_assets;
static fb_data mpeg_stock_header_data[LCD_WIDTH * MPEG_STOCK_HEADER_H];
static fb_data mpeg_stock_playback_data[MPEG_STOCK_PLAYBACK_W *
                                        MPEG_STOCK_PLAYBACK_FRAME_H * 2];
static fb_data mpeg_stock_battery_data[MPEG_STOCK_BATTERY_W *
                                       MPEG_STOCK_BATTERY_FRAME_H *
                                       MPEG_STOCK_BATTERY_FRAMES];
static fb_data mpeg_stock_progress_frame_data[
    MPEG_STOCK_PROGRESS_W * MPEG_STOCK_PROGRESS_H +
    (MPEG_STOCK_PROGRESS_W * MPEG_STOCK_PROGRESS_H) / 4];
static fb_data mpeg_stock_progress_fill_data[
    MPEG_STOCK_PROGRESS_W * MPEG_STOCK_PROGRESS_H +
    (MPEG_STOCK_PROGRESS_W * MPEG_STOCK_PROGRESS_H) / 4];
static fb_data mpeg_stock_progress_fill_cap_data[
    MPEG_STOCK_PROGRESS_CAP_W * MPEG_STOCK_PROGRESS_CAP_H];
static fb_data mpeg_volume_backdrop_data[
    MPEG_VOLUME_CARD_W * MPEG_VOLUME_CARD_H];
static fb_data mpeg_volume_icons_data[
    MPEG_VOLUME_ICON_W * MPEG_VOLUME_ICON_H * MPEG_VOLUME_ICON_FRAMES];
static fb_data mpeg_volume_slider_backdrop_data[
    MPEG_VOLUME_SLIDER_W * MPEG_VOLUME_SLIDER_H];
static fb_data mpeg_volume_slider_fill_data[
    MPEG_VOLUME_SLIDER_W * MPEG_VOLUME_SLIDER_H];
static fb_data mpeg_volume_slider_end_data[
    MPEG_VOLUME_SLIDER_END_W * MPEG_VOLUME_SLIDER_H];

/* The stock volume card owns its own visibility, exactly as the Live TV
 * overlays do. Routing it through osd_refresh() cannot work: a forced refresh
 * issued while the OSD is hidden is promoted to OSD_REFRESH_ALL, which paints
 * the header band and progress strip as well and then clears them again a few
 * seconds later. That promotion is the volume flash. */
#define MPEG_VOLUME_CARD_TIME (HZ * 2)
static long mpeg_volume_card_until;
#endif

#define IPODTIKTOK_PARAM_PREFIX "-ipodtiktok:"
#define IPODTIKTOK_FEED_PATH    PLUGIN_APPS_DATA_DIR "/.ipodtiktok_feed.tsv"
#define IPODTIKTOK_PROFILES_PATH ROCKBOX_DIR "/tiktok/profiles.tsv"
#define IPODTIKTOK_LAUNCH_MARKER \
    PLUGIN_APPS_DATA_DIR "/.ipodtiktok_launch.pending"
#define FEED_MAX_ITEMS          2048
#define FEED_ID_LEN             24
#define FEED_TITLE_LEN          64
#define FEED_CREATOR_LEN        32
#define FEED_DESCRIPTION_LEN    96
#define FEED_PATH_LEN           96
#define FEED_SKIP_COOLDOWN      MAX(1, HZ / 8)
#define FEED_SWIPE_WINDOW       (HZ / 4)
#define FEED_SWIPE_STEPS        2
#define FEED_SWIPE_ANIM_TIME    MAX(1, HZ / 5)
#define FEED_MENU_HOLD_TIME     (HZ * 3 / 4)
#define FEED_LIKE_ANIM_TIME     MAX(1, HZ / 2)
#define FEED_CONFIRM_TIME       HZ
#define FEED_VIDEO_LEFT         92
#define FEED_VIDEO_RIGHT        228
#define FEED_SIDE_GUTTER        6
#define FEED_VOLUME_TIME        (HZ * 2)
#define FEED_PROFILE_NAME_LEN   64
#define FEED_PROFILE_BIO_LEN    192
#define FEED_RECENT_MAX         16
#define FEED_RECENT_CREATORS_MAX 12
#define FEED_PROFILE_THUMB_SLOTS 3
#define FEED_PROFILE_THUMB_W    96
#define FEED_PROFILE_THUMB_H    72
#define FEED_PROFILE_AVATAR_SIZE 48
#define FEED_SAVED_PROFILE_MAX  64

struct feed_item
{
    char id[FEED_ID_LEN];
    char title[FEED_TITLE_LEN];
    char creator[FEED_CREATOR_LEN];
    char description[FEED_DESCRIPTION_LEN];
    char thumbnail[FEED_PATH_LEN];
    char path[FEED_PATH_LEN];
    int like_count;
    int comment_count;
    bool following;
    bool archived;
    bool liked;
    bool saved;
    bool watched;
    bool not_interested;
    unsigned char pin_order;
};

enum feed_section
{
    FEED_SECTION_FOR_YOU = 0,
    FEED_SECTION_FOLLOWING,
    FEED_SECTION_SAVED,
    FEED_SECTION_HISTORY,
    FEED_SECTION_COUNT,
};

struct feed_profile
{
    bool valid;
    char username[FEED_CREATOR_LEN];
    char display_name[FEED_PROFILE_NAME_LEN];
    char bio[FEED_PROFILE_BIO_LEN];
    char avatar[FEED_PATH_LEN];
    int followers;
    int following;
    int likes;
    int videos;
    bool verified;
};

struct feed_state
{
    bool active;
    char feed_path[MAX_PATH];
    char likes_path[MAX_PATH];
    char activity_path[MAX_PATH];
    char state_path[MAX_PATH];
    struct feed_item items[FEED_MAX_ITEMS];
    int count;
    int index;
    enum feed_section section;
    bool section_switch_pending;
    bool select_armed;
    long select_deadline;
    long skip_cooldown_until;
    long swipe_deadline;
    int swipe_direction;
    int swipe_steps;
    bool swipe_committed;
    int swipe_offset;
    int resistance_offset;
    int transition_direction;
    bool transition_enter_pending;
    bool transition_capture_pending;
    long like_anim_until;
    long confirm_until;
    char confirm_text[12];
    long volume_until;
    bool ui_visible;
    bool profile_visible;
    bool profile_saved_only;
    bool saved_profiles_visible;
    bool saved_profiles_resume_playback;
    bool profile_resume_playback;
    bool profile_selection_pending;
    bool profile_feed_active;
    bool action_visible;
    bool details_visible;
    bool action_resume_playback;
    int action_cursor;
    int profile_cursor;
    int profile_count;
    int saved_profile_cursor;
    int saved_profile_count;
    int saved_return_index;
    enum feed_section saved_return_section;
    struct feed_profile profile;
    bool likes_dirty;
    bool activity_dirty;
    bool state_dirty;
    uint32_t recommendation_seed;
    bool state_section_valid;
    char section_ids[FEED_SECTION_COUNT][FEED_ID_LEN];
    char recent_ids[FEED_RECENT_MAX][FEED_ID_LEN];
    int recent_count;
    char recent_creators[FEED_RECENT_CREATORS_MAX][FEED_CREATOR_LEN];
    int recent_creator_count;
    int prepared_from_index;
    enum feed_section prepared_section;
    bool prepared_profile_feed;
    int prepared_next_index;
    int prepared_prev_index;
    uint32_t prepared_next_seed;
};

static struct feed_state feed;
/* LCD_MODE_YUV is a plugin-session mode, not a per-clip mode. Reapplying it
 * while the previous card is deliberately being held can expose the target's
 * cleared YUV scanout as a green frame before the first new blit. */
static bool feed_yuv_mode_active;
static struct bitmap feed_profile_thumbs[FEED_PROFILE_THUMB_SLOTS];
static fb_data feed_profile_thumb_data[FEED_PROFILE_THUMB_SLOTS]
                                      [FEED_PROFILE_THUMB_W *
                                       FEED_PROFILE_THUMB_H];
static bool feed_profile_thumb_valid[FEED_PROFILE_THUMB_SLOTS];
static int feed_profile_thumb_start = -1;
/* Built once when a profile opens: 4 KiB replaces repeated O(feed.count)
 * scans while scrolling large archived accounts. */
static uint16_t feed_profile_items[FEED_MAX_ITEMS];
/* Saved owns a compact creator index, not another media or framebuffer
 * cache. Each entry points at one representative feed item. */
static uint16_t feed_saved_profile_items[FEED_SAVED_PROFILE_MAX];
static uint16_t feed_saved_profile_counts[FEED_SAVED_PROFILE_MAX];
static struct bitmap feed_profile_avatar;
static fb_data feed_profile_avatar_data[FEED_PROFILE_AVATAR_SIZE *
                                        FEED_PROFILE_AVATAR_SIZE];
static bool feed_profile_avatar_valid;
static bool feed_profile_avatar_pending;
static bool feed_profile_thumbs_pending;
static int feed_profile_thumb_load_slot;
static long feed_profile_asset_due;

static void feed_show_saved_profiles(void);
static void feed_hide_saved_profiles(bool resume);
static void feed_saved_profile_move(int delta);
static void feed_open_saved_profile(void);

static void osd_get_wps_slider_layout(uint32_t duration,
                                      int *time_w, int *bar_x,
                                      int *bar_w, int *dur_x)
{
    uint32_t seconds = duration / TS_SECOND;

    if (seconds >= 36000)
    {
        *time_w = 61;
        *bar_x = 68;
        *bar_w = 184;
        *dur_x = 257;
    }
    else if (seconds >= 3600)
    {
        *time_w = 53;
        *bar_x = 60;
        *bar_w = 200;
        *dur_x = 265;
    }
    else if (seconds >= 600)
    {
        *time_w = 40;
        *bar_x = 47;
        *bar_w = 226;
        *dur_x = 278;
    }
    else
    {
        *time_w = 32;
        *bar_x = 39;
        *bar_w = 242;
        *dur_x = 286;
    }
}

#ifdef LCD_PORTRAIT
static fb_data* get_framebuffer(void)
{
    struct viewport *vp_main = *(rb->screens[SCREEN_MAIN]->current_viewport);
    return vp_main->buffer->fb_ptr;
}
#endif

static void osd_show(unsigned show);
static void osd_refresh(int hint);
static void fps_update_post_frame_callback(void);
static int osd_stream_status(void);
static int osd_pause(void);
static void osd_resume(void);
static bool feed_item_in_section(int index, enum feed_section section);
#if !MPEG_STOCK_CONTROLS
static void feed_post_frame_callback(void);
#endif
#ifdef HAVE_LCD_COLOR
static void livetv_volume_show(void);
static void livetv_volume_hide(void);
#endif
#if MPEG_STOCK_CONTROLS
static void mpeg_volume_post_frame_callback(void);
static bool mpeg_volume_card_visible(void);
#endif

static void feed_reset(void)
{
    rb->memset(&feed, 0, sizeof(feed));
    feed.ui_visible = true;
    feed.prepared_from_index = -1;
    feed.prepared_next_index = -1;
    feed.prepared_prev_index = -1;
}

static char *feed_trim(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        text++;

    end = text + rb->strlen(text);
    while (end > text)
    {
        char ch = end[-1];

        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
            break;

        end--;
    }

    *end = '\0';
    return text;
}

static void feed_make_sibling_path(const char *base, const char *name,
                                   char *out, size_t out_size)
{
    char dir[MAX_PATH];
    char *slash;

    rb->strlcpy(dir, base, sizeof(dir));
    slash = rb->strrchr(dir, '/');
    if (slash != NULL)
        slash[1] = '\0';
    else
        dir[0] = '\0';

    rb->snprintf(out, out_size, "%s%s", dir, name);
}

static void feed_default_id_from_path(const char *path, int ordinal,
                                      char *out, size_t out_size)
{
    const char *name = rb->strrchr(path, '/');
    const char *ext;
    size_t i = 0;

    name = (name != NULL && name[1] != '\0') ? name + 1 : path;
    ext = rb->strrchr(name, '.');

    while (name[i] != '\0' && &name[i] != ext && i + 1 < out_size)
    {
        char ch = name[i];

        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9'))
        {
            out[i] = ch;
        }
        else
        {
            out[i] = '_';
        }

        i++;
    }

    out[i] = '\0';

    if (out[0] == '\0')
        rb->snprintf(out, out_size, "clip%d", ordinal + 1);
}

static void feed_default_title_from_path(const char *path, int ordinal,
                                         char *out, size_t out_size)
{
    const char *name = rb->strrchr(path, '/');
    const char *ext;
    size_t i = 0;
    bool prev_space = true;

    name = (name != NULL && name[1] != '\0') ? name + 1 : path;
    ext = rb->strrchr(name, '.');

    while (*name != '\0' && name != ext && i + 1 < out_size)
    {
        char ch = *name++;

        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9'))
        {
            out[i++] = ch;
            prev_space = false;
        }
        else if (!prev_space)
        {
            out[i++] = ' ';
            prev_space = true;
        }
    }

    if (i > 0 && out[i - 1] == ' ')
        i--;

    out[i] = '\0';

    if (out[0] == '\0')
        rb->snprintf(out, out_size, "Clip %d", ordinal + 1);
}

static void feed_load_likes(void)
{
    int fd;
    char line[128];
    int i;

    fd = rb->open(feed.likes_path, O_RDONLY);
    if (fd < 0)
        return;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *key = feed_trim(line);

        if (*key == '\0')
            continue;

        for (i = 0; i < feed.count; i++)
        {
            if (!rb->strcmp(feed.items[i].id, key) ||
                !rb->strcmp(feed.items[i].path, key))
            {
                feed.items[i].liked = true;
                break;
            }
        }
    }

    rb->close(fd);
}

static void feed_save_likes(void)
{
    int fd;
    int i;

    fd = rb->open(feed.likes_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;

    for (i = 0; i < feed.count; i++)
    {
        if (feed.items[i].liked)
            rb->fdprintf(fd, "%s\n", feed.items[i].id);
    }

    rb->close(fd);
}

static void feed_load_activity(void)
{
    int fd = rb->open(feed.activity_path, O_RDONLY);
    char line[128];

    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *key = feed_trim(line);
        char type;
        int i;

        if (key[0] == '\0' || key[1] != '\t')
            continue;
        type = key[0];
        key += 2;
        for (i = 0; i < feed.count; i++)
        {
            if (rb->strcmp(feed.items[i].id, key))
                continue;
            if (type == 'S')
                feed.items[i].saved = true;
            else if (type == 'H')
                feed.items[i].watched = true;
            else if (type == 'N')
                feed.items[i].not_interested = true;
            break;
        }
    }
    rb->close(fd);
}

static void feed_save_activity(void)
{
    int fd = rb->open(feed.activity_path,
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);
    int i;

    if (fd < 0)
        return;
    for (i = 0; i < feed.count; i++)
    {
        if (feed.items[i].saved)
            rb->fdprintf(fd, "S\t%s\n", feed.items[i].id);
        if (feed.items[i].watched)
            rb->fdprintf(fd, "H\t%s\n", feed.items[i].id);
        if (feed.items[i].not_interested)
            rb->fdprintf(fd, "N\t%s\n", feed.items[i].id);
    }
    rb->close(fd);
}

static int feed_find_item_by_id(const char *id)
{
    int i;

    if (id == NULL || *id == '\0')
        return -1;
    for (i = 0; i < feed.count; i++)
        if (!rb->strcmp(feed.items[i].id, id))
            return i;
    return -1;
}

static void feed_remember_section(enum feed_section section, int index)
{
    if (section >= FEED_SECTION_COUNT ||
        !feed_item_in_section(index, section))
        return;
    rb->strlcpy(feed.section_ids[section], feed.items[index].id,
                FEED_ID_LEN);
}

static int feed_restore_section(enum feed_section section)
{
    int index;
    int i;

    if (section >= FEED_SECTION_COUNT)
        return -1;
    index = feed_find_item_by_id(feed.section_ids[section]);
    if (feed_item_in_section(index, section))
        return index;
    for (i = 0; i < feed.count; i++)
        if (feed_item_in_section(i, section))
            return i;
    return -1;
}

static void feed_load_state(void)
{
    int fd;
    char line[128];
    int len;
    int index;

    fd = rb->open(feed.state_path, O_RDONLY);
    if (fd < 0)
        return;

    len = rb->read_line(fd, line, sizeof(line));
    if (len <= 0)
    {
        rb->close(fd);
        return;
    }

    index = rb->atoi(line);
    if (index >= 0 && index < feed.count)
        feed.index = index;

    len = rb->read_line(fd, line, sizeof(line));
    if (len > 0)
        feed.recommendation_seed = (uint32_t)rb->strtoul(line, NULL, 10);

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *value = feed_trim(line);

        if (!rb->strncmp(value, "S\t", 2))
        {
            int section = rb->atoi(value + 2);

            if (section >= 0 && section < FEED_SECTION_COUNT)
            {
                feed.section = (enum feed_section)section;
                feed.state_section_valid = true;
            }
        }
        else if (!rb->strncmp(value, "C\t", 2))
        {
            char *id = rb->strchr(value + 2, '\t');

            if (id != NULL)
            {
                int section;

                *id++ = '\0';
                section = rb->atoi(value + 2);
                if (section >= 0 && section < FEED_SECTION_COUNT)
                    rb->strlcpy(feed.section_ids[section], feed_trim(id),
                                FEED_ID_LEN);
            }
        }
        else if (!rb->strncmp(value, "R\t", 2))
        {
            if (feed.recent_count < FEED_RECENT_MAX && value[2] != '\0')
                rb->strlcpy(feed.recent_ids[feed.recent_count++], value + 2,
                            FEED_ID_LEN);
        }
        else if (!rb->strncmp(value, "K\t", 2))
        {
            if (feed.recent_creator_count < FEED_RECENT_CREATORS_MAX &&
                value[2] != '\0')
                rb->strlcpy(
                    feed.recent_creators[feed.recent_creator_count++],
                    value + 2, FEED_CREATOR_LEN);
        }
        /* Untagged lines are the original state format. Import them once and
         * write the tagged format on exit without discarding watch history. */
        else if (feed.recent_count < FEED_RECENT_MAX && *value != '\0')
        {
            rb->strlcpy(feed.recent_ids[feed.recent_count++], value,
                        FEED_ID_LEN);
        }
    }

    rb->close(fd);
}

static void feed_save_state(void)
{
    int fd;
    int i;

    if (!feed.profile_feed_active)
        feed_remember_section(feed.section, feed.index);

    fd = rb->open(feed.state_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;

    rb->fdprintf(fd, "%d\n%lu\n", feed.index,
                 (unsigned long)feed.recommendation_seed);
    rb->fdprintf(fd, "S\t%d\n", (int)feed.section);
    for (i = 0; i < FEED_SECTION_COUNT; i++)
        if (feed.section_ids[i][0] != '\0')
            rb->fdprintf(fd, "C\t%d\t%s\n", i, feed.section_ids[i]);
    for (i = 0; i < feed.recent_count; i++)
        rb->fdprintf(fd, "R\t%s\n", feed.recent_ids[i]);
    for (i = 0; i < feed.recent_creator_count; i++)
        rb->fdprintf(fd, "K\t%s\n", feed.recent_creators[i]);
    rb->close(fd);
}

static void feed_flush_pending(void)
{
    if (feed.likes_dirty)
    {
        feed_save_likes();
        feed.likes_dirty = false;
    }
    if (feed.activity_dirty)
    {
        feed_save_activity();
        feed.activity_dirty = false;
    }
    if (feed.state_dirty)
    {
        feed_save_state();
        feed.state_dirty = false;
    }
}

static uint32_t feed_hash_id(const char *text)
{
    uint32_t hash = 2166136261u;

    while (*text != '\0')
    {
        hash ^= (unsigned char)*text++;
        hash *= 16777619u;
    }
    return hash;
}

static bool feed_was_recent(const char *id)
{
    int i;

    for (i = 0; i < feed.recent_count; i++)
        if (!rb->strcmp(feed.recent_ids[i], id))
            return true;
    return false;
}

static bool feed_creator_was_recent(const char *creator)
{
    int i;

    if (creator == NULL || *creator == '\0')
        return false;
    for (i = 0; i < feed.recent_creator_count; i++)
        if (!rb->strcasecmp(feed.recent_creators[i], creator))
            return true;
    return false;
}

static void feed_mark_recent(int index)
{
    const char *id;
    const char *creator;
    int i;

    if (index < 0 || index >= feed.count)
        return;
    id = feed.items[index].id;
    for (i = 0; i < feed.recent_count; i++)
    {
        if (!rb->strcmp(feed.recent_ids[i], id))
        {
            for (; i + 1 < feed.recent_count; i++)
                rb->strlcpy(feed.recent_ids[i], feed.recent_ids[i + 1],
                            FEED_ID_LEN);
            feed.recent_count--;
            break;
        }
    }
    if (feed.recent_count == FEED_RECENT_MAX)
    {
        for (i = 0; i + 1 < feed.recent_count; i++)
            rb->strlcpy(feed.recent_ids[i], feed.recent_ids[i + 1],
                        FEED_ID_LEN);
        feed.recent_count--;
    }
    rb->strlcpy(feed.recent_ids[feed.recent_count++], id, FEED_ID_LEN);

    creator = feed.items[index].creator;
    if (*creator == '\0')
        return;
    for (i = 0; i < feed.recent_creator_count; i++)
    {
        if (!rb->strcasecmp(feed.recent_creators[i], creator))
        {
            for (; i + 1 < feed.recent_creator_count; i++)
                rb->strlcpy(feed.recent_creators[i],
                            feed.recent_creators[i + 1], FEED_CREATOR_LEN);
            feed.recent_creator_count--;
            break;
        }
    }
    if (feed.recent_creator_count == FEED_RECENT_CREATORS_MAX)
    {
        for (i = 0; i + 1 < feed.recent_creator_count; i++)
            rb->strlcpy(feed.recent_creators[i],
                        feed.recent_creators[i + 1], FEED_CREATOR_LEN);
        feed.recent_creator_count--;
    }
    rb->strlcpy(feed.recent_creators[feed.recent_creator_count++], creator,
                FEED_CREATOR_LEN);
}

static int feed_recommend_index_for_seed(uint32_t seed)
{
    const char *current_creator = feed.items[feed.index].creator;
    char liked_creators[FEED_RECENT_MAX][FEED_CREATOR_LEN];
    unsigned char liked_counts[FEED_RECENT_MAX] = { 0 };
    int liked_creator_count = 0;
    uint32_t best_score = 0;
    int best = -1;
    int pass;
    int i;

    /* Build a bounded preference table once. Full-profile archives can hold
     * thousands of clips, so rescanning the entire feed per candidate stalls
     * real iPod hardware before the first frame is shown. */
    for (i = 0; i < feed.count; i++)
    {
        int creator_index;

        if (!feed.items[i].liked || feed.items[i].creator[0] == '\0')
            continue;
        for (creator_index = 0; creator_index < liked_creator_count;
             creator_index++)
            if (!rb->strcasecmp(liked_creators[creator_index],
                                feed.items[i].creator))
                break;
        if (creator_index == liked_creator_count &&
            liked_creator_count < FEED_RECENT_MAX)
        {
            rb->strlcpy(liked_creators[liked_creator_count],
                        feed.items[i].creator, FEED_CREATOR_LEN);
            liked_counts[liked_creator_count] = 0;
            liked_creator_count++;
        }
        if (creator_index < liked_creator_count &&
            liked_counts[creator_index] < 3)
            liked_counts[creator_index]++;
    }

    for (pass = 0; pass < 4 && best < 0; pass++)
    {
        for (i = 0; i < feed.count; i++)
        {
            uint32_t score;
            int affinity = 0;
            int creator_index;

            if (i == feed.index || feed.items[i].not_interested)
                continue;
            if (pass < 2 && feed_was_recent(feed.items[i].id))
                continue;
            if (pass == 0 &&
                feed_creator_was_recent(feed.items[i].creator))
                continue;
            if (pass < 3 && current_creator[0] != '\0' &&
                !rb->strcasecmp(feed.items[i].creator, current_creator))
                continue;

            score = (feed_hash_id(feed.items[i].id) ^
                     seed) & 0x1fffffffu;
            for (creator_index = 0; creator_index < liked_creator_count;
                 creator_index++)
                if (!rb->strcasecmp(liked_creators[creator_index],
                                    feed.items[i].creator))
                {
                    affinity = liked_counts[creator_index];
                    break;
                }
            score += (uint32_t)affinity * 0x04000000u;
            /* Prefer unseen clips, while still allowing history to resurface
             * after every unseen candidate has been exhausted. */
            if (!feed.items[i].watched)
                score += 0x40000000u;
            if (best < 0 || score > best_score)
            {
                best = i;
                best_score = score;
            }
        }
    }
    return best;
}

static int feed_recommend_next_index(void)
{
    feed.recommendation_seed = feed.recommendation_seed * 1664525u +
                               1013904223u;
    return feed_recommend_index_for_seed(feed.recommendation_seed);
}

static int feed_predict_linear_index(int direction)
{
    int next = feed.index;
    int step = direction == VIDEO_PREV ? -1 : 1;
    int tries;

    if (feed.profile_feed_active && feed.profile_count > 0)
    {
        int ordinal;

        for (ordinal = 0; ordinal < feed.profile_count; ordinal++)
            if (feed_profile_items[ordinal] == feed.index)
                break;
        if (ordinal >= feed.profile_count)
            ordinal = 0;
        ordinal += step;
        if (ordinal < 0)
            return feed.index;
        else if (ordinal >= feed.profile_count)
            return feed.index;
        return feed_profile_items[ordinal];
    }

    for (tries = 0; tries < feed.count; tries++)
    {
        next += step;
        if (next < 0)
            return feed.index;
        else if (next >= feed.count)
            return feed.index;
        if (feed_item_in_section(next, feed.section))
            return next;
    }
    return feed.index;
}

static void feed_prepare_neighbors(void)
{
    feed.prepared_from_index = feed.index;
    feed.prepared_section = feed.section;
    feed.prepared_profile_feed = feed.profile_feed_active;
    feed.prepared_prev_index = feed_predict_linear_index(VIDEO_PREV);
    if (!feed.profile_feed_active &&
        feed.section == FEED_SECTION_FOR_YOU)
    {
        feed.prepared_next_seed = feed.recommendation_seed * 1664525u +
                                  1013904223u;
        feed.prepared_next_index =
            feed_recommend_index_for_seed(feed.prepared_next_seed);
    }
    else
    {
        feed.prepared_next_seed = feed.recommendation_seed;
        feed.prepared_next_index = feed_predict_linear_index(VIDEO_NEXT);
    }
}

static bool feed_prepared_neighbors_valid(void)
{
    return feed.prepared_from_index == feed.index &&
           feed.prepared_section == feed.section &&
           feed.prepared_profile_feed == feed.profile_feed_active;
}

static bool feed_prepared_boundary(int direction)
{
    int prepared;

    if (!feed_prepared_neighbors_valid())
        return false;
    prepared = direction == VIDEO_PREV ? feed.prepared_prev_index :
                                         feed.prepared_next_index;
    return prepared == feed.index;
}

static bool feed_add_item(const char *id, const char *title, const char *path,
                          const char *source_type, const char *creator,
                          const char *description, const char *thumbnail,
                          const char *like_count, const char *comment_count,
                          const char *pin_order)
{
    struct feed_item *item;

    if (feed.count >= FEED_MAX_ITEMS)
        return false;

    /* A valid feed item is always an absolute Rockbox path. This also makes
     * the parser robust to current and legacy TSV header variants. */
    if (path == NULL || path[0] != '/')
        return true;

    item = &feed.items[feed.count];

    if (id != NULL && *id != '\0')
        rb->strlcpy(item->id, id, sizeof(item->id));
    else
        feed_default_id_from_path(path, feed.count, item->id, sizeof(item->id));

    if (title != NULL && *title != '\0')
        rb->strlcpy(item->title, title, sizeof(item->title));
    else
        feed_default_title_from_path(path, feed.count, item->title,
                                     sizeof(item->title));

    rb->strlcpy(item->path, path, sizeof(item->path));
    rb->strlcpy(item->creator, creator ? creator : "",
                sizeof(item->creator));
    rb->strlcpy(item->description, description ? description : "",
                sizeof(item->description));
    rb->strlcpy(item->thumbnail, thumbnail ? thumbnail : "",
                sizeof(item->thumbnail));
    item->following = source_type && !rb->strcmp(source_type, "following");
    item->archived = source_type && !rb->strcmp(source_type, "archive");
    item->like_count = like_count ? rb->atoi(like_count) : 0;
    item->comment_count = comment_count ? rb->atoi(comment_count) : 0;
    item->pin_order = pin_order ? MIN(3, MAX(0, rb->atoi(pin_order))) : 0;
    feed.count++;
    return true;
}

static bool feed_parse_file(const char *path)
{
    int fd;
    char line[1024];
    int len;

    fd = rb->open(path, O_RDONLY);
    MPLOG("feed open path=%s fd=%d\n", path, fd);
    if (fd < 0)
        return false;

    while ((len = rb->read_line(fd, line, sizeof(line))) > 0)
    {
        char *fields[10] = { NULL };
        char *cursor;
        char *scan;
        int field_count = 1;

        cursor = feed_trim(line);
        if (*cursor == '\0' || *cursor == '#')
            continue;
        fields[0] = cursor;
        for (scan = cursor; *scan != '\0' &&
             field_count < (int)ARRAYLEN(fields); scan++)
        {
            if (*scan == '\t')
            {
                *scan = '\0';
                fields[field_count++] = scan + 1;
            }
        }
        if (field_count < 3)
            continue;

        for (int i = 0; i < field_count; i++)
            fields[i] = feed_trim(fields[i]);
        if (!rb->strcmp(fields[0], "id"))
            continue;

        if (!feed_add_item(fields[0], fields[1], fields[2],
                           field_count > 3 ? fields[3] : "manual",
                           field_count > 4 ? fields[4] : "",
                           field_count > 5 ? fields[5] : "",
                           field_count > 6 ? fields[6] : "",
                           field_count > 7 ? fields[7] : "0",
                           field_count > 8 ? fields[8] : "0",
                           field_count > 9 ? fields[9] : "0"))
            continue;
    }

    rb->close(fd);
    return feed.count > 0;
}

static bool feed_init_from_path(const char *path, char *videofile,
                                size_t videofile_size)
{
    feed_reset();
    feed.active = true;
    rb->strlcpy(feed.feed_path, path, sizeof(feed.feed_path));
    feed_make_sibling_path(path, ".ipodtiktok_likes.dat", feed.likes_path,
                           sizeof(feed.likes_path));
    feed_make_sibling_path(path, ".ipodtiktok_activity.dat",
                           feed.activity_path, sizeof(feed.activity_path));
    feed_make_sibling_path(path, ".ipodtiktok_state.dat", feed.state_path,
                           sizeof(feed.state_path));

    if (!feed_parse_file(path))
        return false;

    feed_load_likes();
    feed_load_activity();
    feed_load_state();

    if (feed.index < 0 || feed.index >= feed.count)
        feed.index = 0;

    if (!feed.state_section_valid)
        feed.section = feed.items[feed.index].following ?
                       FEED_SECTION_FOLLOWING : FEED_SECTION_FOR_YOU;
    else
    {
        int restored = feed_restore_section(feed.section);

        if (restored >= 0)
            feed.index = restored;
    }

    if (feed.recommendation_seed == 0)
        feed.recommendation_seed = (uint32_t)*rb->current_tick ^
                                   feed_hash_id(feed.items[feed.index].id);
    if (feed.section == FEED_SECTION_FOR_YOU && feed.count > 1)
    {
        int recommendation = feed_recommend_next_index();

        if (recommendation >= 0)
            feed.index = recommendation;
        feed_mark_recent(feed.index);
    }
    feed_remember_section(feed.section, feed.index);
    /* Also migrates the original index-only state format on the next flush. */
    feed.state_dirty = true;

    feed.skip_cooldown_until = 0;
    feed.swipe_deadline = 0;
    feed.swipe_direction = 0;
    feed.swipe_steps = 0;
    feed.swipe_offset = 0;
    feed.transition_direction = 0;
    feed.transition_enter_pending = false;
    feed.transition_capture_pending = false;
    feed.section_switch_pending = false;
    feed.select_armed = false;
    rb->strlcpy(videofile, feed.items[feed.index].path, videofile_size);
    return true;
}

static bool feed_get_next_file(int direction, char *videofile, size_t bufsize)
{
    int next = feed.index;
    int step = direction == VIDEO_PREV ? -1 : 1;
    int tries;

    if (!feed.active || feed.count <= 0)
        return false;

    /* Leaving a card is enough to place it in History. The bit is persisted
     * separately from the generated feed, so resyncing never erases it. */
    if (!feed.items[feed.index].watched)
    {
        feed.items[feed.index].watched = true;
        feed.activity_dirty = true;
    }

    if (feed.profile_selection_pending)
    {
        /* SELECT on a profile row has already chosen the exact item. */
        feed.profile_selection_pending = false;
        next = feed.index;
    }
    else if (feed.profile_feed_active && feed.profile_count > 0)
    {
        /* A video opened from a creator page owns the swipe queue until the
         * user explicitly changes tabs with Left. Keep the profile's synced
         * newest-first/pinned ordering instead of falling back into For You. */
        if (feed_prepared_neighbors_valid())
            next = direction == VIDEO_PREV ? feed.prepared_prev_index :
                                             feed.prepared_next_index;
        else
            next = feed_predict_linear_index(direction);
    }
    else if (feed.section_switch_pending)
    {
        feed.section_switch_pending = false;
        if (feed.section == FEED_SECTION_FOR_YOU)
        {
            int recommendation = feed_recommend_next_index();

            if (recommendation >= 0)
                next = recommendation;
        }
    }
    else if (feed.section == FEED_SECTION_FOR_YOU &&
             direction != VIDEO_PREV)
    {
        int recommendation;

        if (feed_prepared_neighbors_valid() &&
            feed.prepared_next_index >= 0)
        {
            recommendation = feed.prepared_next_index;
            feed.recommendation_seed = feed.prepared_next_seed;
        }
        else
            recommendation = feed_recommend_next_index();

        if (recommendation >= 0)
            next = recommendation;
    }
    else if (feed_prepared_neighbors_valid())
    {
        next = direction == VIDEO_PREV ? feed.prepared_prev_index :
                                         feed.prepared_next_index;
    }
    else
    {
        for (tries = 0; tries < feed.count; tries++)
        {
            next += step;
            if (next < 0)
                next = feed.count - 1;
            else if (next >= feed.count)
                next = 0;
            if (feed_item_in_section(next, feed.section))
                break;
        }
    }

    feed.index = next;
    feed.prepared_from_index = -1;
    if (!feed.profile_feed_active &&
        feed.section == FEED_SECTION_FOR_YOU && direction != VIDEO_PREV)
        feed_mark_recent(feed.index);
    if (!feed.profile_feed_active)
        feed_remember_section(feed.section, feed.index);
    feed.state_dirty = true;
    rb->strlcpy(videofile, feed.items[feed.index].path, bufsize);
    return true;
}

static bool feed_swipe_allowed(int direction, bool accelerated)
{
    long tick = *rb->current_tick;
    int required_steps = accelerated ? 1 : FEED_SWIPE_STEPS;

    if (feed.swipe_committed ||
        TIME_BEFORE(tick, feed.skip_cooldown_until))
        return false;

    if (feed.swipe_direction != direction ||
        !TIME_BEFORE(tick, feed.swipe_deadline))
    {
        feed.swipe_direction = direction;
        feed.swipe_steps = 0;
    }
    feed.swipe_deadline = tick + FEED_SWIPE_WINDOW;
    if (++feed.swipe_steps < required_steps)
        return false;

    feed.swipe_steps = 0;
    /* One physical wheel burst commits at most one card. Any queued repeat
     * tail is discarded by the cooldown after the incoming card settles. */
    feed.swipe_committed = true;
    feed.skip_cooldown_until = tick + FEED_SKIP_COOLDOWN;
    return true;
}

static void feed_animate_swipe(void)
{
    long start = *rb->current_tick;
    int last_offset = -1;

    /* video_out combines its fixed outgoing-card cache with the newly
     * decoded card in the existing YUV compositor. Position is elapsed-time
     * based so dropped frames shorten the push instead of slowing input.
     * Smoothstep starts and finishes at rest; the old ease-out curve jumped
     * immediately and made the click-wheel transition look like a flash. */
    while (true)
    {
        long elapsed = *rb->current_tick - start;
        int progress = MIN(256, (int)(elapsed * 256 /
                                      FEED_SWIPE_ANIM_TIME));
        int eased = progress * progress * (768 - 2 * progress) / 65536;
        int offset = LCD_HEIGHT * eased / 256;

        feed.swipe_offset = offset & ~1;
        if (feed.swipe_offset != last_offset)
        {
            stream_draw_frame(false);
            last_offset = feed.swipe_offset;
        }
        if (progress >= 256)
            break;
        rb->sleep(1);
    }
    feed.swipe_offset = LCD_HEIGHT;
}

static bool feed_release_primed_transition(void)
{
    long start = *rb->current_tick;
    long video_deadline = start + HZ / 2;
    long audio_grace = start + HZ / 6;
    bool video_ready = false;
    bool audio_ready = false;

    /* At offset zero video_out paints only the fixed outgoing cache. These
     * readiness probes can therefore decode and compose the incoming card
     * without exposing a blank/green frame or its UI prematurely. Audio uses
     * the normal playback mixer channel but its PCM clock remains held. */
    do
    {
        video_ready = stream_draw_frame(false) || video_ready;
        audio_ready = stream_primed_audio_ready();
        if (video_ready &&
            (audio_ready || !TIME_BEFORE(*rb->current_tick, audio_grace)))
            break;
        rb->sleep(1);
    }
    while (TIME_BEFORE(*rb->current_tick, video_deadline));

    stream_release_prime();
    return video_ready;
}

static void feed_animate_edge_resistance(int direction)
{
    long start = *rb->current_tick;
    const long duration = MAX(1, HZ / 8);

    while (true)
    {
        long elapsed = *rb->current_tick - start;
        int progress = MIN(256, (int)(elapsed * 256 / duration));
        int distance = progress > 128 ? progress - 128 : 128 - progress;
        int triangle = 128 - distance;

        feed.resistance_offset = direction == VIDEO_PREV ?
                                 -(triangle * 10 / 128) :
                                  (triangle * 10 / 128);
        feed.resistance_offset &= ~1;
        stream_draw_frame(false);
        if (progress >= 256)
            break;
        rb->sleep(1);
    }
    feed.resistance_offset = 0;
    stream_draw_frame(false);
}

static void feed_like_current(void)
{
    struct feed_item *item;

    if (!feed.active || feed.index < 0 || feed.index >= feed.count)
        return;

    item = &feed.items[feed.index];
    item->liked = !item->liked;
    feed.likes_dirty = true;
    feed.prepared_from_index = -1;

    feed.like_anim_until = item->liked ?
                           *rb->current_tick + FEED_LIKE_ANIM_TIME : 0;
    if (osd_stream_status() == STREAM_PAUSED)
        stream_draw_frame(false);
    feed_prepare_neighbors();
}

static bool feed_item_in_section(int index, enum feed_section section)
{
    const struct feed_item *item;

    if (index < 0 || index >= feed.count)
        return false;
    item = &feed.items[index];
    switch (section)
    {
    case FEED_SECTION_FOLLOWING:
        return item->following;
    case FEED_SECTION_SAVED:
        return item->saved;
    case FEED_SECTION_HISTORY:
        return item->watched;
    case FEED_SECTION_FOR_YOU:
    default:
        /* TikTok's For You pool can surface followed creators too. Keeping
         * them out made every newly followed account invisible here. */
        return !item->not_interested;
    }
}

static const char *feed_section_label(enum feed_section section)
{
    if (feed.profile_feed_active)
        return "PROFILE";
    switch (section)
    {
    case FEED_SECTION_FOLLOWING: return "FOLLOWING";
    case FEED_SECTION_SAVED: return "SAVED";
    case FEED_SECTION_HISTORY: return "HISTORY";
    case FEED_SECTION_FOR_YOU:
    default: return "FOR YOU";
    }
}

static bool feed_set_section(enum feed_section section)
{
    int i;

    if (!feed.active || feed.section == section)
        return false;
    i = feed_restore_section(section);
    if (i >= 0)
    {
        feed.section = section;
        feed.index = i;
        feed.section_switch_pending = true;
        feed.state_dirty = true;
        return true;
    }
    return false;
}

static int feed_section_count(void)
{
    int count = 0;
    int i;

    if (feed.profile_feed_active)
        return feed.profile_count;

    for (i = 0; i < feed.count; i++)
        if (feed_item_in_section(i, feed.section))
            count++;
    return count;
}

static int feed_section_position(void)
{
    int position = 0;
    int i;

    if (feed.profile_feed_active)
    {
        for (i = 0; i < feed.profile_count; i++)
            if (feed_profile_items[i] == feed.index)
                return i + 1;
        return 1;
    }

    for (i = 0; i <= feed.index && i < feed.count; i++)
        if (feed_item_in_section(i, feed.section))
            position++;
    return MAX(1, position);
}

static bool feed_cycle_section(int direction)
{
    int section = (int)feed.section;
    int tries;

    /* Left is the explicit exit from a creator-owned swipe queue. Preserve
     * the tab's own cursor rather than replacing it with the profile clip. */
    if (!feed.profile_feed_active)
        feed_remember_section(feed.section, feed.index);
    feed.profile_feed_active = false;

    for (tries = 0; tries < FEED_SECTION_COUNT; tries++)
    {
        section = (section + direction + FEED_SECTION_COUNT) %
                  FEED_SECTION_COUNT;
        if (feed_set_section((enum feed_section)section))
            return true;
    }
    return false;
}

static void feed_show_actions(void)
{
    if (!feed.active || feed.profile_visible || feed.details_visible)
        return;
    feed.action_resume_playback = osd_stream_status() == STREAM_PLAYING;
    if (feed.action_resume_playback)
        osd_pause();
    feed.action_visible = true;
    feed.action_cursor = 0;
    feed.select_armed = false;
    stream_draw_frame(false);
}

static void feed_hide_actions(bool resume)
{
    bool should_resume = resume && feed.action_resume_playback;

    feed.action_visible = false;
    feed.details_visible = false;
    feed.action_resume_playback = false;
    if (should_resume && osd_stream_status() == STREAM_PAUSED)
        osd_resume();
    stream_draw_frame(false);
}

static void feed_action_move(int delta)
{
    int next = feed.action_cursor + delta;

    if (!feed.action_visible)
        return;
    if (next < 0)
        next = 3;
    else if (next > 3)
        next = 0;
    feed.action_cursor = next;
    stream_draw_frame(false);
}

static void feed_format_label(char *buf, size_t buf_size,
                              const char *src, int max_width)
{
    char source[FEED_PROFILE_BIO_LEN];
    size_t in = 0;
    size_t out = 0;
    bool pending_space = false;
    int width;

    /* The YUV fixed-font renderer is byte based. Strip unsupported UTF-8
     * sequences instead of painting every byte as a garbage glyph. This is
     * deliberately in-place safe because profile usernames use one buffer. */
    rb->strlcpy(source, src ? src : "", sizeof(source));
    while (source[in] != '\0' && out + 1 < buf_size)
    {
        unsigned char ch = (unsigned char)source[in++];

        if (ch >= 0x80)
        {
            while (((unsigned char)source[in] & 0xc0) == 0x80)
                in++;
            pending_space = out > 0;
            continue;
        }
        if (ch <= 0x20 || ch == 0x7f)
        {
            pending_space = out > 0;
            continue;
        }
        if (pending_space && out + 1 < buf_size)
            buf[out++] = ' ';
        pending_space = false;
        buf[out++] = (char)ch;
    }
    while (out > 0 && buf[out - 1] == ' ')
        out--;
    buf[out] = '\0';

    if (buf[0] == '\0')
        rb->strlcpy(buf, "TikTok", buf_size);
    mylcd_getstringsize(buf, &width, NULL);
    if (width <= max_width)
        return;

    while (buf[0] != '\0')
    {
        size_t text_len = rb->strlen(buf);

        if (text_len > 3)
        {
            buf[text_len - 1] = '\0';
            rb->strlcpy(&buf[text_len - 4], "...", 4);
        }

        mylcd_getstringsize(buf, &width, NULL);
        if (width <= max_width || text_len <= 4)
            return;
    }
}

static bool feed_profile_creator_matches(int index)
{
    const char *creator;

    if (index < 0 || index >= feed.count)
        return false;
    creator = feed.items[index].creator;
    if (*creator == '@')
        creator++;
    return !rb->strcasecmp(creator, feed.profile.username);
}

static int feed_profile_item_index(int ordinal)
{
    if (ordinal < 0 || ordinal >= feed.profile_count)
        return -1;
    return feed_profile_items[ordinal];
}

static int feed_profile_thumbnail_start(void)
{
    int start = feed.profile_cursor > 0 ? feed.profile_cursor - 1 : 0;

    if (feed.profile_count > FEED_PROFILE_THUMB_SLOTS &&
        start > feed.profile_count - FEED_PROFILE_THUMB_SLOTS)
        start = feed.profile_count - FEED_PROFILE_THUMB_SLOTS;
    return start;
}

/* Prepare cache ownership during the input event, but never touch storage.
 * Adjacent moves retain two decoded cards and invalidate only the incoming
 * edge. One idle service call below loads at most one missing thumbnail. */
static void feed_profile_prepare_thumbnails(void)
{
    int start = feed_profile_thumbnail_start();
    int slot;

    if (start == feed_profile_thumb_start)
        return;
    if (start == feed_profile_thumb_start + 1)
    {
        for (slot = 0; slot < FEED_PROFILE_THUMB_SLOTS - 1; slot++)
        {
            rb->memcpy(feed_profile_thumb_data[slot],
                       feed_profile_thumb_data[slot + 1],
                       sizeof(feed_profile_thumb_data[slot]));
            feed_profile_thumb_valid[slot] =
                feed_profile_thumb_valid[slot + 1];
        }
        feed_profile_thumb_valid[FEED_PROFILE_THUMB_SLOTS - 1] = false;
    }
    else if (start + 1 == feed_profile_thumb_start)
    {
        for (slot = FEED_PROFILE_THUMB_SLOTS - 1; slot > 0; slot--)
        {
            rb->memcpy(feed_profile_thumb_data[slot],
                       feed_profile_thumb_data[slot - 1],
                       sizeof(feed_profile_thumb_data[slot]));
            feed_profile_thumb_valid[slot] =
                feed_profile_thumb_valid[slot - 1];
        }
        feed_profile_thumb_valid[0] = false;
    }
    else
    {
        for (slot = 0; slot < FEED_PROFILE_THUMB_SLOTS; slot++)
            feed_profile_thumb_valid[slot] = false;
    }

    feed_profile_thumb_start = start;
    feed_profile_thumb_load_slot = 0;
    feed_profile_thumbs_pending = true;
    feed_profile_asset_due = *rb->current_tick + MAX(1, HZ / 20);
}

static bool feed_profile_service_assets(void)
{
    int slot;

    if (!feed.profile_visible || rb->button_queue_count() != 0 ||
        TIME_BEFORE(*rb->current_tick, feed_profile_asset_due))
        return false;

    if (feed_profile_avatar_pending)
    {
        feed_profile_avatar_pending = false;
        if (feed.profile.avatar[0] == '/')
            feed_profile_avatar_valid =
                rb->read_bmp_file(feed.profile.avatar, &feed_profile_avatar,
                                  sizeof(feed_profile_avatar_data),
                                  FORMAT_NATIVE, NULL) > 0 &&
                feed_profile_avatar.width == FEED_PROFILE_AVATAR_SIZE &&
                feed_profile_avatar.height == FEED_PROFILE_AVATAR_SIZE;
        feed_profile_asset_due = *rb->current_tick + 1;
        return true;
    }

    if (!feed_profile_thumbs_pending)
        return false;
    for (slot = feed_profile_thumb_load_slot;
         slot < FEED_PROFILE_THUMB_SLOTS; slot++)
    {
        int index;
        struct bitmap *bitmap = &feed_profile_thumbs[slot];

        feed_profile_thumb_load_slot = slot + 1;
        if (feed_profile_thumb_valid[slot])
            continue;
        index = feed_profile_item_index(feed_profile_thumb_start + slot);
        if (index < 0 || feed.items[index].thumbnail[0] != '/')
            continue;
        rb->memset(bitmap, 0, sizeof(*bitmap));
        bitmap->data = (unsigned char *)feed_profile_thumb_data[slot];
        feed_profile_thumb_valid[slot] =
            rb->read_bmp_file(feed.items[index].thumbnail, bitmap,
            sizeof(feed_profile_thumb_data[slot]),
            FORMAT_NATIVE, NULL) > 0 &&
            bitmap->width == FEED_PROFILE_THUMB_W &&
            bitmap->height == FEED_PROFILE_THUMB_H;
        feed_profile_asset_due = *rb->current_tick + 1;
        return true;
    }
    feed_profile_thumbs_pending = false;
    return false;
}

static void feed_profile_move(int delta)
{
    int next;

    if (!feed.profile_visible || feed.profile_count <= 0)
        return;
    next = feed.profile_cursor + delta;
    if (next < 0)
        next = 0;
    else if (next >= feed.profile_count)
        next = feed.profile_count - 1;
    if (next != feed.profile_cursor)
    {
        feed.profile_cursor = next;
        feed_profile_prepare_thumbnails();
        stream_draw_frame(false);
    }
}

static void feed_format_count(char *buf, size_t buf_size, int value)
{
    unsigned int count = value > 0 ? (unsigned int)value : 0;

    if (count >= 1000000)
    {
        unsigned int decimal = (count % 1000000) / 100000;
        if (decimal != 0 && count < 10000000)
            rb->snprintf(buf, buf_size, "%u.%uM", count / 1000000, decimal);
        else
            rb->snprintf(buf, buf_size, "%uM", count / 1000000);
    }
    else if (count >= 1000)
    {
        unsigned int decimal = (count % 1000) / 100;
        if (decimal != 0 && count < 100000)
            rb->snprintf(buf, buf_size, "%u.%uK", count / 1000, decimal);
        else
            rb->snprintf(buf, buf_size, "%uK", count / 1000);
    }
    else
        rb->snprintf(buf, buf_size, "%u", count);
}

static void feed_show_current_profile(bool saved_only)
{
    const char *creator;
    int fd;
    char line[1024];

    if (!feed.active || feed.index < 0 || feed.index >= feed.count)
        return;

    feed.profile_saved_only = saved_only;

    rb->memset(&feed.profile, 0, sizeof(feed.profile));
    creator = feed.items[feed.index].creator;
    if (*creator == '@')
        creator++;
    rb->strlcpy(feed.profile.username, creator,
                sizeof(feed.profile.username));
    rb->strlcpy(feed.profile.display_name,
                feed.items[feed.index].creator[0] ?
                feed.items[feed.index].creator : feed.items[feed.index].title,
                sizeof(feed.profile.display_name));

    fd = rb->open(IPODTIKTOK_PROFILES_PATH, O_RDONLY);
    if (fd >= 0)
    {
        while (rb->read_line(fd, line, sizeof(line)) > 0)
        {
            char *fields[12] = { NULL };
            char *scan;
            int field_count = 1;
            int i;

            fields[0] = feed_trim(line);
            for (scan = fields[0]; *scan != '\0' &&
                 field_count < (int)ARRAYLEN(fields); scan++)
            {
                if (*scan == '\t')
                {
                    *scan = '\0';
                    fields[field_count++] = scan + 1;
                }
            }
            for (i = 0; i < field_count; i++)
                fields[i] = feed_trim(fields[i]);

            if (field_count < 9 || !rb->strcmp(fields[1], "username") ||
                rb->strcasecmp(fields[1], creator))
                continue;

            feed.profile.valid = true;
            rb->strlcpy(feed.profile.username, fields[1],
                        sizeof(feed.profile.username));
            rb->strlcpy(feed.profile.display_name, fields[2],
                        sizeof(feed.profile.display_name));
            rb->strlcpy(feed.profile.bio, fields[3],
                        sizeof(feed.profile.bio));
            rb->strlcpy(feed.profile.avatar, fields[4],
                        sizeof(feed.profile.avatar));
            feed.profile.followers = rb->atoi(fields[5]);
            feed.profile.following = rb->atoi(fields[6]);
            feed.profile.likes = rb->atoi(fields[7]);
            feed.profile.videos = rb->atoi(fields[8]);
            feed.profile.verified = field_count > 10 && rb->atoi(fields[10]);
            break;
        }
        rb->close(fd);
    }

    feed_profile_avatar_valid = false;
    rb->memset(&feed_profile_avatar, 0, sizeof(feed_profile_avatar));
    feed_profile_avatar.data = (unsigned char *)feed_profile_avatar_data;
    feed_profile_avatar_pending = feed.profile.avatar[0] == '/';

    feed.profile_count = 0;
    feed.profile_cursor = 0;
    feed_profile_thumb_start = -1;
    feed_profile_thumbs_pending = false;
    for (int i = 0; i < FEED_PROFILE_THUMB_SLOTS; i++)
        feed_profile_thumb_valid[i] = false;
    for (int i = 0; i < feed.count; i++)
    {
        if (!feed_profile_creator_matches(i))
            continue;
        if (feed.profile_saved_only && !feed.items[i].saved)
            continue;
        if (i == feed.index)
            feed.profile_cursor = feed.profile_count;
        feed_profile_items[feed.profile_count] = (uint16_t)i;
        feed.profile_count++;
    }
    feed_profile_prepare_thumbnails();

    feed.profile_resume_playback =
        osd_stream_status() == STREAM_PLAYING;
    if (feed.profile_resume_playback)
        osd_pause();
    feed.profile_visible = true;
    feed.select_armed = false;
    stream_draw_frame(false);
}

static void feed_hide_profile(void)
{
    bool resume = feed.profile_resume_playback;

    feed.profile_visible = false;
    feed_profile_avatar_pending = false;
    feed_profile_thumbs_pending = false;
    feed.profile_resume_playback = false;
    if (feed.profile_saved_only)
    {
        feed.profile_saved_only = false;
        feed.saved_profiles_visible = true;
        stream_draw_frame(false);
        return;
    }
    if (resume && osd_stream_status() == STREAM_PAUSED)
        osd_resume();
    stream_draw_frame(false);
}

static void feed_build_saved_profiles(void)
{
    int i;

    feed.saved_profile_count = 0;
    feed.saved_profile_cursor = 0;
    for (i = 0; i < feed.count; i++)
    {
        int profile;
        const char *creator;

        if (!feed.items[i].saved)
            continue;
        creator = feed.items[i].creator;
        for (profile = 0; profile < feed.saved_profile_count; profile++)
        {
            const char *known =
                feed.items[feed_saved_profile_items[profile]].creator;
            if (!rb->strcasecmp(creator, known))
                break;
        }
        if (profile < feed.saved_profile_count)
        {
            feed_saved_profile_counts[profile]++;
            continue;
        }
        if (feed.saved_profile_count >= FEED_SAVED_PROFILE_MAX)
            continue;
        feed_saved_profile_items[feed.saved_profile_count] = (uint16_t)i;
        feed_saved_profile_counts[feed.saved_profile_count] = 1;
        feed.saved_profile_count++;
    }
}

static void feed_show_saved_profiles(void)
{
    feed_build_saved_profiles();
    feed.saved_profiles_resume_playback =
        osd_stream_status() == STREAM_PLAYING;
    if (feed.saved_profiles_resume_playback)
        osd_pause();
    feed.saved_profiles_visible = true;
    feed.select_armed = false;
    stream_draw_frame(false);
}

static void feed_hide_saved_profiles(bool resume)
{
    bool should_resume = resume && feed.saved_profiles_resume_playback;

    feed.saved_profiles_visible = false;
    feed.saved_profiles_resume_playback = false;
    feed.section = feed.saved_return_section;
    feed.index = feed.saved_return_index;
    if (should_resume && osd_stream_status() == STREAM_PAUSED)
        osd_resume();
    stream_draw_frame(false);
}

static bool feed_leave_saved_profiles_for_section(int direction)
{
    bool should_resume = feed.saved_profiles_resume_playback;

    feed.saved_profiles_visible = false;
    feed.saved_profiles_resume_playback = false;
    feed.select_armed = false;
    /* The Saved picker is an overlay for the Saved tab. Left must continue
     * through the tab strip instead of restoring History and trapping the
     * user in a History <-> Saved loop. Menu remains the ordinary Back path
     * and still restores saved_return_section above. */
    if (feed_cycle_section(direction))
        return true;

    feed.section = feed.saved_return_section;
    feed.index = feed.saved_return_index;
    if (should_resume && osd_stream_status() == STREAM_PAUSED)
        osd_resume();
    stream_draw_frame(false);
    return false;
}

static void feed_saved_profile_move(int delta)
{
    int next;

    if (!feed.saved_profiles_visible || feed.saved_profile_count <= 0)
        return;
    next = feed.saved_profile_cursor + delta;
    next = MAX(0, MIN(feed.saved_profile_count - 1, next));
    if (next != feed.saved_profile_cursor)
    {
        feed.saved_profile_cursor = next;
        stream_draw_frame(false);
    }
}

static void feed_open_saved_profile(void)
{
    int target;

    if (!feed.saved_profiles_visible || feed.saved_profile_count <= 0)
        return;
    target = feed_saved_profile_items[feed.saved_profile_cursor];
    if (target < 0 || target >= feed.count)
        return;
    feed.index = target;
    feed.saved_profiles_visible = false;
    feed_show_current_profile(true);
}

#ifdef LCD_LANDSCAPE
    #define __X (x + osd.x)
    #define __Y (y + osd.y)
    #define __W width
    #define __H height
#else
    #define __X (LCD_WIDTH - (y + osd.y) - height)
    #define __Y (x + osd.x)
    #define __W height
    #define __H width
#endif

#ifdef HAVE_LCD_COLOR
/* Blend two colors in 0-100% (0-255) mix of c2 into c1 */
static unsigned draw_blendcolor(unsigned c1, unsigned c2, unsigned char amount)
{
    int r1 = RGB_UNPACK_RED(c1);
    int g1 = RGB_UNPACK_GREEN(c1);
    int b1 = RGB_UNPACK_BLUE(c1);

    int r2 = RGB_UNPACK_RED(c2);
    int g2 = RGB_UNPACK_GREEN(c2);
    int b2 = RGB_UNPACK_BLUE(c2);

    return LCD_RGBPACK(amount*(r2 - r1) / 255 + r1,
                       amount*(g2 - g1) / 255 + g1,
                       amount*(b2 - b1) / 255 + b1);
}
#endif

/* Drawing functions that operate rotated on LCD_PORTRAIT displays -
 * most are just wrappers of lcd_* functions with transforms applied.
 * The origin is the upper-left corner of the OSD area */
static void draw_update_rect(int x, int y, int width, int height)
{
    mylcd_update_rect(__X, __Y, __W, __H);
}

static void draw_clear_area(int x, int y, int width, int height)
{
    /* Fail-safe: Never clear full OSD/video area in WPS overlay mode */
    if (osd.use_wps_layout && x == 0 && y == 0 && width == osd.width && height == osd.height)
        return;
#ifdef HAVE_LCD_COLOR
    rb->screen_clear_area(rb->screens[SCREEN_MAIN], __X, __Y, __W, __H);
#else
    int oldmode = grey_get_drawmode();
    grey_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);
    grey_fillrect(__X, __Y, __W, __H);
    grey_set_drawmode(oldmode);
#endif
}

#if !IPODTIKTOK_USE_BITMAP_ASSETS
static void draw_clear_area_rect(const struct vo_rect *rc)
{
    draw_clear_area(rc->l, rc->t, rc->r - rc->l, rc->b - rc->t);
}
#endif

static void draw_fillrect(int x, int y, int width, int height)
{
    mylcd_fillrect(__X, __Y, __W, __H);
}

static void draw_hline(int x1, int x2, int y)
{
#ifdef LCD_LANDSCAPE
    mylcd_hline(x1 + osd.x, x2 + osd.x, y + osd.y);
#else
    y = LCD_WIDTH - (y + osd.y) - 1;
    mylcd_vline(y, x1 + osd.x, x2 + osd.x);
#endif
}

static void draw_vline(int x, int y1, int y2)
{
#ifdef LCD_LANDSCAPE
    mylcd_vline(x + osd.x, y1 + osd.y, y2 + osd.y);
#else
    y1 = LCD_WIDTH - (y1 + osd.y) - 1;
    y2 = LCD_WIDTH - (y2 + osd.y) - 1;
    mylcd_hline(y1, y2, x + osd.x);
#endif
}

static void draw_scrollbar_draw(int x, int y, int width, int height,
                                uint32_t min, uint32_t max, uint32_t val)
{
    /* Use iPone WPS slider bitmaps if loaded */
    if (osd.slider_bitmaps_loaded)
    {
        int bg_width = osd.slider_bg_bmp.width;
        int bg_height = osd.slider_bg_bmp.height;
        int knob_width = osd.slider_knob_bmp.width;
        int knob_height = osd.slider_knob_bmp.height;
        int draw_y = y + (height - bg_height) / 2;
        
        /* Calculate available width for slider movement (drawn width - knob width) */
        int draw_width = MIN(bg_width, width);
        int slider_range = draw_width - knob_width;
        if (slider_range < 0)
            slider_range = 0;
        
        /* Calculate knob position based on playback progress */
        int knob_pos = 0;
        if (max > min)
        {
            knob_pos = (int)muldiv_uint32(slider_range, val, max - min);
            if (knob_pos > slider_range)
                knob_pos = slider_range;
        }
        
#ifdef LCD_LANDSCAPE
        /* In WPS overlay mode, skip the track bitmap - it covers video.
         * Only draw the knob to show position without a background. */
        if (!osd.use_wps_layout)
        {
            rb->lcd_bmp_part(&osd.slider_bg_bmp, 0, 0,
                             x + osd.x, draw_y + osd.y,
                             draw_width, bg_height);
        }

        /* Draw the slider knob at the calculated position */
        rb->lcd_bmp_part(&osd.slider_knob_bmp, 0, 0,
                         x + osd.x + knob_pos,
                         draw_y + osd.y + (bg_height - knob_height) / 2,
                         knob_width, knob_height);
#else
        /* Portrait orientation - need to handle rotation */
        /* For now, fall back to generic drawing on portrait displays */
        /* Fall through to generic drawing */
#endif
        return;
    }
    
    /* Fall back to original generic drawing - skip all fills in WPS overlay mode */
    if (osd.use_wps_layout)
        return;
    {
        unsigned oldfg = mylcd_get_foreground();
        
        draw_hline(x + 1, x + width - 2, y);
        draw_hline(x + 1, x + width - 2, y + height - 1);
        draw_vline(x, y + 1, y + height - 2);
        draw_vline(x + width - 1, y + 1, y + height - 2);
        
        val = muldiv_uint32(width - 2, val, max - min);
        val = MIN(val, (uint32_t)(width - 2));
        
        draw_fillrect(x + 1, y + 1, val, height - 2);
        
        mylcd_set_foreground(osd.prog_fillcolor);
        
        draw_fillrect(x + 1 + val, y + 1, width - 2 - val, height - 2);
        
        mylcd_set_foreground(oldfg);
    }
}

static void draw_scrollbar_draw_rect(const struct vo_rect *rc, int min,
                                     int max, int val)
{
    draw_scrollbar_draw(rc->l, rc->t, rc->r - rc->l, rc->b - rc->t,
                        min, max, val);
}

static void draw_setfont(int font)
{
    osd.font = font;
    mylcd_setfont(font);
}

#ifdef LCD_PORTRAIT
/* Portrait displays need rotated text rendering */

/* Limited function that only renders in DRMODE_FG and uses absolute screen
 * coordinates */
static void draw_oriented_mono_bitmap_part(const unsigned char *src,
                                           int src_x, int src_y,
                                           int stride, int x, int y,
                                           int width, int height)
{
    const unsigned char *src_end;
    fb_data *dst, *dst_end;
    unsigned fg_pattern;

    if (x + width > SCREEN_WIDTH)
        width = SCREEN_WIDTH - x; /* Clip right */
    if (x < 0)
        width += x, x = 0; /* Clip left */
    if (width <= 0)
        return; /* nothing left to do */

    if (y + height > SCREEN_HEIGHT)
        height = SCREEN_HEIGHT - y; /* Clip bottom */
    if (y < 0)
        height += y, y = 0; /* Clip top */
    if (height <= 0)
        return; /* nothing left to do */

    fg_pattern =     rb->lcd_get_foreground();
    /*bg_pattern =*/ rb->lcd_get_background();

    src += stride * (src_y >> 3) + src_x; /* move starting point */
    src_y  &= 7;
    src_end = src + width;

    dst = get_framebuffer() + (LCD_WIDTH - y) + x*LCD_WIDTH;
    do
    {
        const unsigned char *src_col = src++;
        unsigned data = *src_col >> src_y;
        int numbits = 8 - src_y;

        fb_data *dst_col = dst;
        dst_end = dst_col - height;
        dst += LCD_WIDTH;

        do
        {
            dst_col--;

            if (data & 1)
                *dst_col = FB_SCALARPACK(fg_pattern);
#if 0
            else
                *dst_col = bg_pattern;
#endif
            data >>= 1;
            if (--numbits == 0) {
                src_col += stride;
                data = *src_col;
                numbits = 8;
            }
        }
        while (dst_col > dst_end);
    }
    while (src < src_end);
}


#define ALPHA_COLOR_FONT_DEPTH 2
#define ALPHA_COLOR_LOOKUP_SHIFT (1 << ALPHA_COLOR_FONT_DEPTH)
#define ALPHA_COLOR_LOOKUP_SIZE ((1 << ALPHA_COLOR_LOOKUP_SHIFT) - 1)
#define ALPHA_COLOR_PIXEL_PER_BYTE (8 >> ALPHA_COLOR_FONT_DEPTH)
#define ALPHA_COLOR_PIXEL_PER_WORD (32 >> ALPHA_COLOR_FONT_DEPTH)
#ifdef CPU_ARM
#define BLEND_INIT do {} while (0)
#define BLEND_FINISH do {} while(0)
#define BLEND_START(acc, color, alpha) \
    asm volatile("mul %0, %1, %2" : "=&r" (acc) : "r" (color), "r" (alpha))
#define BLEND_CONT(acc, color, alpha) \
    asm volatile("mla %0, %1, %2, %0" : "+&r" (acc) : "r" (color), "r" (alpha))
#define BLEND_OUT(acc) do {} while (0)
#elif defined(CPU_COLDFIRE)
#define ALPHA_BITMAP_READ_WORDS
#define BLEND_INIT \
    unsigned long _macsr = coldfire_get_macsr(); \
    coldfire_set_macsr(EMAC_UNSIGNED)
#define BLEND_FINISH \
    coldfire_set_macsr(_macsr)
#define BLEND_START(acc, color, alpha) \
    asm volatile("mac.l %0, %1, %%acc0" :: "%d" (color), "d" (alpha))
#define BLEND_CONT BLEND_START
#define BLEND_OUT(acc) asm volatile("movclr.l %%acc0, %0" : "=d" (acc))
#else
#define BLEND_INIT do {} while (0)
#define BLEND_FINISH do {} while(0)
#define BLEND_START(acc, color, alpha) ((acc) = (color) * (alpha))
#define BLEND_CONT(acc, color, alpha) ((acc) += (color) * (alpha))
#define BLEND_OUT(acc) do {} while (0)
#endif

/* Blend the given two colors */
static inline unsigned blend_two_colors(unsigned c1, unsigned c2, unsigned a)
{
#if LCD_DEPTH == 16
    a += a >> (ALPHA_COLOR_LOOKUP_SHIFT - 1);
#if (LCD_PIXELFORMAT == RGB565SWAPPED)
    c1 = swap16(c1);
    c2 = swap16(c2);
#endif
    unsigned c1l = (c1 | (c1 << 16)) & 0x07e0f81f;
    unsigned c2l = (c2 | (c2 << 16)) & 0x07e0f81f;
    unsigned p;
    BLEND_START(p, c1l, a);
    BLEND_CONT(p, c2l, ALPHA_COLOR_LOOKUP_SIZE + 1 - a);
    BLEND_OUT(p);
    p = (p >> ALPHA_COLOR_LOOKUP_SHIFT) & 0x07e0f81f;
    p |= (p >> 16);
#if (LCD_PIXELFORMAT == RGB565SWAPPED)
    return swap16(p);
#else
    return p;
#endif

#else /* LCD_DEPTH == 24 */
    unsigned s = c1;
    unsigned d = c2;
    unsigned s1 = s & 0xff00ff;
    unsigned d1 = d & 0xff00ff;
    a += a >> (ALPHA_COLOR_LOOKUP_SHIFT - 1);
    d1 = (d1 + ((s1 - d1) * a >> ALPHA_COLOR_LOOKUP_SHIFT)) & 0xff00ff;
    s &= 0xff00;
    d &= 0xff00;
    d = (d + ((s - d) * a >> ALPHA_COLOR_LOOKUP_SHIFT)) & 0xff00;

    return d1 | d;
#endif
}

static void draw_oriented_alpha_bitmap_part(const unsigned char *src,
                                            int src_x, int src_y,
                                            int stride, int x, int y,
                                            int width, int height)
{
    fb_data *dst, *dst_start;
    unsigned fg_pattern;

    if (x + width > SCREEN_WIDTH)
        width = SCREEN_WIDTH - x; /* Clip right */
    if (x < 0)
        width += x, x = 0; /* Clip left */
    if (width <= 0)
        return; /* nothing left to do */

    if (y + height > SCREEN_HEIGHT)
        height = SCREEN_HEIGHT - y; /* Clip bottom */
    if (y < 0)
        height += y, y = 0; /* Clip top */
    if (height <= 0)
        return; /* nothing left to do */

    /* initialize blending */
    BLEND_INIT;

    fg_pattern =    rb->lcd_get_foreground();
    /*bg_pattern=*/ rb->lcd_get_background();

    dst_start = get_framebuffer() + (LCD_WIDTH - y - 1) + x*LCD_WIDTH;
    int col, row = height;
    unsigned data, pixels;
    unsigned skip_end = (stride - width);
    unsigned skip_start = src_y * stride + src_x;

#ifdef ALPHA_BITMAP_READ_WORDS
    uint32_t *src_w = (uint32_t *)((uintptr_t)src & ~3);
    skip_start += ALPHA_COLOR_PIXEL_PER_BYTE * ((uintptr_t)src & 3);
    src_w += skip_start / ALPHA_COLOR_PIXEL_PER_WORD;
    data = letoh32(*src_w++);
#else
    src += skip_start / ALPHA_COLOR_PIXEL_PER_BYTE;
    data = *src;
#endif
    pixels = skip_start % ALPHA_COLOR_PIXEL_PER_WORD;
    data >>= pixels * ALPHA_COLOR_LOOKUP_SHIFT;
#ifdef ALPHA_BITMAP_READ_WORDS
    pixels = 8 - pixels;
#endif

    do
    {
        col = width;
        dst = dst_start--;
#ifdef ALPHA_BITMAP_READ_WORDS
#define UPDATE_SRC_ALPHA    do { \
            if (--pixels) \
                data >>= ALPHA_COLOR_LOOKUP_SHIFT; \
            else \
            { \
                data = letoh32(*src_w++); \
                pixels = ALPHA_COLOR_PIXEL_PER_WORD; \
            } \
        } while (0)
#elif ALPHA_COLOR_PIXEL_PER_BYTE == 2
#define UPDATE_SRC_ALPHA    do { \
            if (pixels ^= 1) \
                data >>= ALPHA_COLOR_LOOKUP_SHIFT; \
            else \
                data = *(++src); \
        } while (0)
#else
#define UPDATE_SRC_ALPHA    do { \
            if (pixels = (++pixels % ALPHA_COLOR_PIXEL_PER_BYTE)) \
                data >>= ALPHA_COLOR_LOOKUP_SHIFT; \
            else \
                data = *(++src); \
        } while (0)
#endif
        do
        {
            unsigned color = blend_two_colors(FB_UNPACK_SCALAR_LCD(*dst), fg_pattern,
                                    data & ALPHA_COLOR_LOOKUP_SIZE );
            *dst= FB_SCALARPACK(color);
            dst += LCD_WIDTH;
            UPDATE_SRC_ALPHA;
        }
        while (--col);
#ifdef ALPHA_BITMAP_READ_WORDS
        if (skip_end < pixels)
        {
            pixels -= skip_end;
            data >>= skip_end * ALPHA_COLOR_LOOKUP_SHIFT;
        } else {
            pixels = skip_end - pixels;
            src_w += pixels / ALPHA_COLOR_PIXEL_PER_WORD;
            pixels %= ALPHA_COLOR_PIXEL_PER_WORD;
            data = letoh32(*src_w++);
            data >>= pixels * ALPHA_COLOR_LOOKUP_SHIFT;
            pixels = 8 - pixels;
        }
#else
        if (skip_end)
        {
            pixels += skip_end;
            if (pixels >= ALPHA_COLOR_PIXEL_PER_BYTE)
            {
                src += pixels / ALPHA_COLOR_PIXEL_PER_BYTE;
                pixels %= ALPHA_COLOR_PIXEL_PER_BYTE;
                data = *src;
                data >>= pixels * ALPHA_COLOR_LOOKUP_SHIFT;
            } else
                data >>= skip_end * ALPHA_COLOR_LOOKUP_SHIFT;
        }
#endif
    } while (--row);
}

static void draw_putsxy_oriented(int x, int y, const char *str)
{
    ucschar_t ch;
    ucschar_t *ucs;
    int ofs = MIN(x, 0);
    struct font* pf = rb->font_get(osd.font);

    ucs = rb->bidi_l2v(str, 1);

    x += osd.x;
    y += osd.y;

    while ((ch = *ucs++) != 0 && x < SCREEN_WIDTH)
    {
        int width;
        const unsigned char *bits;

        /* get proportional width and glyph bits */
        width = rb->font_get_width(pf, ch);

        if (ofs > width) {
            ofs -= width;
            continue;
        }

        bits = rb->font_get_bits(pf, ch);

        if (pf->depth)
            draw_oriented_alpha_bitmap_part(bits, ofs, 0, width, x, y,
                                            width - ofs, pf->height);
        else
            draw_oriented_mono_bitmap_part(bits, ofs, 0, width, x, y,
                                           width - ofs, pf->height);

        x += width - ofs;
        ofs = 0;
    }
}
#else
static void draw_oriented_mono_bitmap_part(const unsigned char *src,
                                           int src_x, int src_y,
                                           int stride, int x, int y,
                                           int width, int height)
{
    int mode = mylcd_get_drawmode();
    mylcd_set_drawmode(DRMODE_FG);
    mylcd_mono_bitmap_part(src, src_x, src_y, stride, x, y, width, height);
    mylcd_set_drawmode(mode);
}

static void draw_putsxy_oriented(int x, int y, const char *str)
{
    int mode = mylcd_get_drawmode();
    mylcd_set_drawmode(DRMODE_FG);
    mylcd_putsxy(x + osd.x, y + osd.y, str);
    mylcd_set_drawmode(mode);
}
#endif /* LCD_PORTRAIT */

/** FPS Display **/

/* Post-frame callback (on video thread) - update the FPS rectangle from the
 * framebuffer */
static void fps_post_frame_callback(void)
{
    vo_lock();
    mylcd_update_rect(fps.pf_x, fps.pf_y,
                      fps.pf_width, fps.pf_height);
    vo_unlock();
}

#if !MPEG_STOCK_CONTROLS
static void feed_draw_side_ui(void)
{
    const struct feed_item *item;
    char text[FEED_DESCRIPTION_LEN];
    char time_text[20];
    int width;
    int progress;
    uint32_t duration;
    uint32_t time;
    int heart_x = FEED_VIDEO_RIGHT +
                  (SCREEN_WIDTH - FEED_VIDEO_RIGHT - FEED_HEART_SIZE) / 2;
    int heart_y = 70;
    bool show_volume;

    if (!feed.active || feed.index < 0 || feed.index >= feed.count)
        return;

    item = &feed.items[feed.index];
    duration = stream_get_duration();
    time = osd.curr_time;
    if (duration == INVALID_TIMESTAMP || duration == 0)
        duration = 1;
    if (time > duration)
        time = duration;
    progress = (int)muldiv_uint32(FEED_VIDEO_LEFT - 2 * FEED_SIDE_GUTTER,
                                  time, duration);
    show_volume = TIME_BEFORE(*rb->current_tick, feed.volume_until);

    mylcd_set_drawmode(DRMODE_SOLID);
    mylcd_set_background(LCD_BLACK);
    mylcd_set_foreground(LCD_BLACK);
    draw_fillrect(0, 0, FEED_VIDEO_LEFT, SCREEN_HEIGHT);
    draw_fillrect(FEED_VIDEO_RIGHT, 0,
                  SCREEN_WIDTH - FEED_VIDEO_RIGHT, SCREEN_HEIGHT);

    mylcd_set_foreground(LCD_RGBPACK(0x62, 0x62, 0x66));
    draw_vline(FEED_VIDEO_LEFT - 1, 0, SCREEN_HEIGHT - 1);
    draw_vline(FEED_VIDEO_RIGHT, 0, SCREEN_HEIGHT - 1);

#if IPODTIKTOK_USE_BITMAP_ASSETS
    rb->lcd_bitmap_part(ipodtiktok_header, 0, 0,
                        BMPWIDTH_ipodtiktok_header,
                        4, 4, FEED_VIDEO_LEFT - 8,
                        MIN(BMPHEIGHT_ipodtiktok_header, 36));
#else
    mylcd_set_foreground(LCD_WHITE);
    draw_putsxy_oriented(8, 10, "TikTok");
#endif

    mylcd_set_foreground(feed.section == FEED_SECTION_FOR_YOU ?
                         LCD_WHITE : LCD_RGBPACK(0x99, 0x99, 0x9d));
    draw_putsxy_oriented(FEED_SIDE_GUTTER, 48, "For You");
    mylcd_set_foreground(feed.section == FEED_SECTION_FOLLOWING ?
                         LCD_WHITE : LCD_RGBPACK(0x99, 0x99, 0x9d));
    draw_putsxy_oriented(FEED_SIDE_GUTTER, 62, "Following");
    mylcd_set_foreground(LCD_RGBPACK(0x2f, 0x8f, 0xe5));
    draw_fillrect(FEED_SIDE_GUTTER,
                  feed.section == FEED_SECTION_FOR_YOU ? 58 : 72,
                  44, 2);

    mylcd_set_foreground(LCD_WHITE);
    feed_format_label(text, sizeof(text),
                      item->creator[0] ? item->creator : item->title,
                      FEED_VIDEO_LEFT - 2 * FEED_SIDE_GUTTER);
    draw_putsxy_oriented(FEED_SIDE_GUTTER, 91, text);
    mylcd_set_foreground(LCD_RGBPACK(0xc8, 0xc8, 0xcc));
    feed_format_label(text, sizeof(text),
                      item->description[0] ? item->description : item->title,
                      FEED_VIDEO_LEFT - 2 * FEED_SIDE_GUTTER);
    draw_putsxy_oriented(FEED_SIDE_GUTTER, 108, text);

    if (show_volume)
    {
        int min_volume = rb->sound_min(SOUND_VOLUME);
        int max_volume = rb->sound_max(SOUND_VOLUME);
        int volume = rb->global_status->volume;
        int volume_width = 0;

        if (max_volume > min_volume)
            volume_width = (volume - min_volume) *
                           (FEED_VIDEO_LEFT - 2 * FEED_SIDE_GUTTER) /
                           (max_volume - min_volume);
        mylcd_set_foreground(LCD_WHITE);
        draw_putsxy_oriented(FEED_SIDE_GUTTER, 190, "Volume");
        mylcd_set_foreground(LCD_RGBPACK(0x55, 0x55, 0x59));
        draw_fillrect(FEED_SIDE_GUTTER, 208,
                      FEED_VIDEO_LEFT - 2 * FEED_SIDE_GUTTER, 5);
        mylcd_set_foreground(LCD_RGBPACK(0x2f, 0x8f, 0xe5));
        draw_fillrect(FEED_SIDE_GUTTER, 208, volume_width, 5);
    }
    else
    {
        rb->snprintf(time_text, sizeof(time_text), "%lu:%02lu",
                     (unsigned long)(time / TS_SECOND / 60),
                     (unsigned long)(time / TS_SECOND % 60));
        mylcd_set_foreground(LCD_WHITE);
        draw_putsxy_oriented(FEED_SIDE_GUTTER, 190, time_text);
        mylcd_set_foreground(LCD_RGBPACK(0x55, 0x55, 0x59));
        draw_fillrect(FEED_SIDE_GUTTER, 208,
                      FEED_VIDEO_LEFT - 2 * FEED_SIDE_GUTTER, 5);
        mylcd_set_foreground(LCD_RGBPACK(0x2f, 0x8f, 0xe5));
        draw_fillrect(FEED_SIDE_GUTTER, 208, progress, 5);
    }

#if IPODTIKTOK_USE_BITMAP_ASSETS
    if (item->liked || TIME_BEFORE(*rb->current_tick, feed.like_anim_until))
        rb->lcd_bitmap_transparent(ipodtiktok_heart, heart_x, heart_y,
                                   BMPWIDTH_ipodtiktok_heart,
                                   BMPHEIGHT_ipodtiktok_heart);
    else
        rb->lcd_bitmap_transparent(ipodtiktok_heart_outline, heart_x, heart_y,
                                   BMPWIDTH_ipodtiktok_heart_outline,
                                   BMPHEIGHT_ipodtiktok_heart_outline);
#endif
    rb->snprintf(text, sizeof(text), "%d",
                 item->like_count + (item->liked ? 1 : 0));
    mylcd_getstringsize(text, &width, NULL);
    mylcd_set_foreground(LCD_WHITE);
    draw_putsxy_oriented(heart_x + (FEED_HEART_SIZE - width) / 2,
                         heart_y + FEED_HEART_SIZE + 3, text);
    mylcd_set_foreground(LCD_RGBPACK(0xc8, 0xc8, 0xcc));
    draw_putsxy_oriented(FEED_VIDEO_RIGHT + 10, 130, "Comments");
    rb->snprintf(text, sizeof(text), "%d", item->comment_count);
    mylcd_getstringsize(text, &width, NULL);
    draw_putsxy_oriented(FEED_VIDEO_RIGHT +
                         (SCREEN_WIDTH - FEED_VIDEO_RIGHT - width) / 2,
                         146, text);
    rb->snprintf(text, sizeof(text), "%d of %d",
                 feed_section_position(), feed_section_count());
    mylcd_getstringsize(text, &width, NULL);
    mylcd_set_foreground(LCD_WHITE);
    draw_putsxy_oriented(FEED_VIDEO_RIGHT +
                         (SCREEN_WIDTH - FEED_VIDEO_RIGHT - width) / 2,
                         214, text);

    draw_update_rect(0, 0, FEED_VIDEO_LEFT, SCREEN_HEIGHT);
    draw_update_rect(FEED_VIDEO_RIGHT, 0,
                     SCREEN_WIDTH - FEED_VIDEO_RIGHT, SCREEN_HEIGHT);
}

static void feed_post_frame_callback(void)
{
    if (!feed.active)
        return;

    vo_lock();
    feed_draw_side_ui();
    vo_unlock();
}
#endif

/* Set up to have the callback only update the intersection of the video
 * rectangle and the FPS text rectangle - if they don't intersect, then
 * the callback is set to NULL */
static void fps_update_post_frame_callback(void)
{
    void (*cb)(void) = NULL;

    if (mpegplayer_maps_dashcam_launch) {
        /* Maps dashcams are intentionally video-only.  In particular, do
         * not let a saved FPS setting or the stock volume card claim the
         * single post-frame callback and paint over subsequent frames. */
    }
    else if (feed.active) {
        /* TikTok owns the YUV compositor. A second RGB post-frame redraw
         * races the video blit and makes the two side panels flash. */
    }
#if MPEG_STOCK_CONTROLS
    else if (mpeg_volume_card_visible()) {
        /* The card outranks the FPS readout while it is up; both cannot own
         * the single post-frame callback slot. */
        cb = mpeg_volume_post_frame_callback;
    }
#endif
    else if (settings.showfps) {
        struct vo_rect cliprect;

        if (stream_vo_get_clip(&cliprect)) {
            /* Oriented screen coordinates -> OSD coordinates */
            vo_rect_offset(&cliprect, -osd.x, -osd.y);

            if (vo_rect_intersect(&cliprect, &cliprect, &fps.rect)) {
                int x = cliprect.l;
                int y = cliprect.t;
                int width = cliprect.r - cliprect.l;
                int height = cliprect.b - cliprect.t;

                /* OSD coordinates -> framebuffer coordinates */
                fps.pf_x = __X;
                fps.pf_y = __Y;
                fps.pf_width = __W;
                fps.pf_height = __H;

                cb = fps_post_frame_callback;
            }
        }
    }

    stream_set_callback(VIDEO_SET_POST_FRAME_CALLBACK, cb);
}

/* Refresh the FPS display */
static void fps_refresh(void)
{
    char str[FPS_BUFSIZE];
    struct video_output_stats stats;
    int w, h, sw;
    long tick;

    tick = *rb->current_tick;

    if (TIME_BEFORE(tick, fps.update_tick))
        return;

    fps.update_tick = tick + FPS_UPDATE_INTERVAL;

    stream_video_stats(&stats);

    rb->snprintf(str, FPS_BUFSIZE, FPS_FORMAT,
                 stats.fps / 100, stats.fps % 100);

    w = fps.rect.r - fps.rect.l;
    h = fps.rect.b - fps.rect.t;

    draw_clear_area(fps.rect.l, fps.rect.t, w, h);
    mylcd_getstringsize(str, &sw, NULL);
    draw_putsxy_oriented(fps.rect.r - sw, fps.rect.t, str);

    vo_lock();
    draw_update_rect(fps.rect.l, fps.rect.t, w, h);
    vo_unlock();
}

/* Initialize the FPS display */
static void fps_init(void)
{
    fps.update_tick = *rb->current_tick;
    fps.rect.l = fps.rect.t = 0;
    mylcd_getstringsize(FPS_DIMSTR, &fps.rect.r, &fps.rect.b);
    vo_rect_offset(&fps.rect, -osd.x, -osd.y);
    fps_update_post_frame_callback();
}

/** OSD **/

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
/* So we can refresh the overlay */
static void osd_lcd_enable_hook(unsigned short id, void* param)
{
    (void)id;
    (void)param;
    rb->button_queue_post(LCD_ENABLE_EVENT_1, 0);
}
#endif

static void osdbacklight_hw_on_video_mode(bool video_on)
{
    if (video_on) {
        /* Turn off backlight timeout */
        backlight_ignore_timeout();

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
        rb->remove_event(LCD_EVENT_ACTIVATION, osd_lcd_enable_hook);
#endif
    } else {
#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
        rb->add_event(LCD_EVENT_ACTIVATION, osd_lcd_enable_hook);
#endif
        /* Revert to user's backlight settings */
        backlight_use_settings();
    }
}

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
static void osd_backlight_brightness_video_mode(bool video_on)
{
    if (settings.backlight_brightness < 0)
        return;

    mpeg_backlight_update_brightness(
        video_on ? settings.backlight_brightness : -1);
}
#else
#define osd_backlight_brightness_video_mode(video_on)
#endif /* HAVE_BACKLIGHT_BRIGHTNESS */

#if MPEG_STOCK_CONTROLS
static bool mpeg_stock_load_bitmap(const char *name, struct bitmap *bm,
                                   fb_data *pixels, size_t bytes,
                                   int width, int height, bool transparent)
{
    char path[MAX_PATH];
    int format = FORMAT_NATIVE;
    int rc;

    rb->snprintf(path, sizeof(path), MPEG_STOCK_ASSET_DIR "/%s", name);
    rb->memset(bm, 0, sizeof(*bm));
    bm->data = (char *)pixels;
    if (transparent)
        format |= FORMAT_TRANSPARENT;
    rc = rb->read_bmp_file(path, bm, (int)bytes, format, NULL);
    return rc > 0 && bm->width == width && bm->height == height;
}

static bool mpeg_stock_assets_loaded(void)
{
    if (mpeg_stock_assets.tried)
        return mpeg_stock_assets.loaded;

    mpeg_stock_assets.tried = true;
    mpeg_stock_assets.loaded =
        mpeg_stock_load_bitmap("status-header.apple.320x24x24.bmp",
                               &mpeg_stock_assets.header,
                               mpeg_stock_header_data,
                               sizeof(mpeg_stock_header_data),
                               LCD_WIDTH, MPEG_STOCK_HEADER_H, false) &&
        mpeg_stock_load_bitmap("status-playback.apple.20x32x24.bmp",
                               &mpeg_stock_assets.playback,
                               mpeg_stock_playback_data,
                               sizeof(mpeg_stock_playback_data),
                               MPEG_STOCK_PLAYBACK_W,
                               MPEG_STOCK_PLAYBACK_FRAME_H * 2, true) &&
        mpeg_stock_load_bitmap("status-battery.apple.26x65x24.bmp",
                               &mpeg_stock_assets.battery,
                               mpeg_stock_battery_data,
                               sizeof(mpeg_stock_battery_data),
                               MPEG_STOCK_BATTERY_W,
                               MPEG_STOCK_BATTERY_FRAME_H *
                                   MPEG_STOCK_BATTERY_FRAMES, true) &&
        mpeg_stock_load_bitmap("progress-frame.apple.200x22x32.bmp",
                               &mpeg_stock_assets.progress_frame,
                               mpeg_stock_progress_frame_data,
                               sizeof(mpeg_stock_progress_frame_data),
                               MPEG_STOCK_PROGRESS_W,
                               MPEG_STOCK_PROGRESS_H, true) &&
        mpeg_stock_load_bitmap("progress-fill.apple.200x22x32.bmp",
                               &mpeg_stock_assets.progress_fill,
                               mpeg_stock_progress_fill_data,
                               sizeof(mpeg_stock_progress_fill_data),
                               MPEG_STOCK_PROGRESS_W,
                               MPEG_STOCK_PROGRESS_H, true) &&
        mpeg_stock_load_bitmap("progress-fill-cap.apple.16x16x24.bmp",
                               &mpeg_stock_assets.progress_fill_cap,
                               mpeg_stock_progress_fill_cap_data,
                               sizeof(mpeg_stock_progress_fill_cap_data),
                               MPEG_STOCK_PROGRESS_CAP_W,
                               MPEG_STOCK_PROGRESS_CAP_H, true);
    return mpeg_stock_assets.loaded;
}

static bool mpeg_volume_load_bitmap(const char *path, struct bitmap *bm,
                                    fb_data *pixels, size_t bytes,
                                    int width, int height, bool transparent)
{
    int format = FORMAT_NATIVE;
    int rc;

    rb->memset(bm, 0, sizeof(*bm));
    bm->data = (char *)pixels;
    if (transparent)
        format |= FORMAT_TRANSPARENT;
    rc = rb->read_bmp_file(path, bm, (int)bytes, format, NULL);
    return rc > 0 && bm->width == width && bm->height == height;
}

static void mpeg_volume_wps_asset_path(char *path, size_t size,
                                       const char *asset, bool fallback)
{
    const char *wps;
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

static bool mpeg_volume_try_load_set(bool fallback)
{
    char path[MAX_PATH];

    mpeg_volume_wps_asset_path(path, sizeof(path), "VolumeBackdrop.bmp",
                               fallback);
    if (!mpeg_volume_load_bitmap(path, &mpeg_volume_assets.backdrop,
                                 mpeg_volume_backdrop_data,
                                 sizeof(mpeg_volume_backdrop_data),
                                 MPEG_VOLUME_CARD_W, MPEG_VOLUME_CARD_H,
                                 true))
        return false;

    mpeg_volume_wps_asset_path(path, sizeof(path), "VolumePromptIcons.bmp",
                               fallback);
    if (!mpeg_volume_load_bitmap(path, &mpeg_volume_assets.icons,
                                 mpeg_volume_icons_data,
                                 sizeof(mpeg_volume_icons_data),
                                 MPEG_VOLUME_ICON_W,
                                 MPEG_VOLUME_ICON_H *
                                     MPEG_VOLUME_ICON_FRAMES, true))
        return false;

    mpeg_volume_wps_asset_path(path, sizeof(path),
                               "VolumeSliderBackdropPurple.bmp", fallback);
    if (!mpeg_volume_load_bitmap(path, &mpeg_volume_assets.slider_backdrop,
                                 mpeg_volume_slider_backdrop_data,
                                 sizeof(mpeg_volume_slider_backdrop_data),
                                 MPEG_VOLUME_SLIDER_W, MPEG_VOLUME_SLIDER_H,
                                 false))
        return false;

    mpeg_volume_wps_asset_path(path, sizeof(path),
                               "VolumeSliderPurple.bmp", fallback);
    if (!mpeg_volume_load_bitmap(path, &mpeg_volume_assets.slider_fill,
                                 mpeg_volume_slider_fill_data,
                                 sizeof(mpeg_volume_slider_fill_data),
                                 MPEG_VOLUME_SLIDER_W, MPEG_VOLUME_SLIDER_H,
                                 false))
        return false;

    mpeg_volume_wps_asset_path(path, sizeof(path),
                               "VolumeSliderEndPurple.bmp", fallback);
    return mpeg_volume_load_bitmap(path, &mpeg_volume_assets.slider_end,
                                   mpeg_volume_slider_end_data,
                                   sizeof(mpeg_volume_slider_end_data),
                                   MPEG_VOLUME_SLIDER_END_W,
                                   MPEG_VOLUME_SLIDER_H, true);
}

static bool mpeg_volume_assets_loaded(void)
{
    if (!mpeg_volume_assets.tried)
    {
        mpeg_volume_assets.tried = true;
        mpeg_volume_assets.loaded = mpeg_volume_try_load_set(false) ||
                                    mpeg_volume_try_load_set(true);
    }

    return mpeg_volume_assets.loaded;
}

static void mpeg_stock_format_time(uint32_t timestamp,
                                   char *buf, size_t size)
{
    uint32_t seconds;
    uint32_t hours;
    uint32_t minutes;

    if (buf == NULL || size == 0)
        return;

    seconds = timestamp / TS_SECOND;
    hours = seconds / 3600;
    seconds %= 3600;
    minutes = seconds / 60;
    seconds %= 60;

    if (hours > 0)
        rb->snprintf(buf, size, "%lu:%02lu:%02lu",
                     (unsigned long)hours, (unsigned long)minutes,
                     (unsigned long)seconds);
    else
        rb->snprintf(buf, size, "%lu:%02lu",
                     (unsigned long)minutes, (unsigned long)seconds);
}

static void mpeg_stock_draw_text_shadow(int x, int y, const char *text)
{
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy(x + 1, y + 1, text);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(x, y, text);
}

static void mpeg_stock_title(char *title, size_t size, int max_width)
{
    const char *base = mpeg_osd_path;
    char *dot;
    int width;
    int chars;

    if (base[0] == '\0')
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

static int mpeg_stock_battery_frame(void)
{
    int level = rb->battery_level();

    if (level <= 20)
        return 0;
    if (level < 80)
        return 1;
    return 2;
}

static void mpeg_stock_draw_progress(void)
{
    char current[24];
    char duration_text[24];
    char title[MPEG_STOCK_TITLE_SIZE];
    int dur_w = 0;
    int title_w;
    int title_h;
    int fill_w;
    int battery_frame;
    int progress_x = (LCD_WIDTH - MPEG_STOCK_PROGRESS_W) / 2;
    int progress_y = LCD_HEIGHT - MPEG_STOCK_PROGRESS_H - 20;
    int text_y = progress_y + 3;
    uint32_t current_time = osd.curr_time;
    uint32_t total_time = stream_get_duration();

    if (!mpeg_stock_assets_loaded())
        return;

    if (total_time == INVALID_TIMESTAMP || total_time == 0)
        total_time = 1;
    if (current_time > total_time)
        current_time = total_time;

    mpeg_stock_format_time(current_time, current, sizeof(current));
    mpeg_stock_format_time(total_time, duration_text, sizeof(duration_text));
    rb->lcd_getstringsize(duration_text, &dur_w, NULL);

    rb->lcd_bitmap((const fb_data *)mpeg_stock_assets.header.data,
                   0, 0, LCD_WIDTH, MPEG_STOCK_HEADER_H);

    mpeg_stock_title(title, sizeof(title), LCD_WIDTH - 82);
    rb->lcd_getstringsize(title, &title_w, &title_h);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy((LCD_WIDTH - title_w) / 2,
                   (MPEG_STOCK_HEADER_H - title_h) / 2, title);
    rb->lcd_bitmap_transparent_part(
        (const fb_data *)mpeg_stock_assets.playback.data,
        0,
        osd.status == OSD_STATUS_PAUSED ?
            MPEG_STOCK_PLAYBACK_FRAME_H : 0,
        MPEG_STOCK_PLAYBACK_W, 270, 4,
        MPEG_STOCK_PLAYBACK_W, MPEG_STOCK_PLAYBACK_FRAME_H);
    battery_frame = mpeg_stock_battery_frame();
    rb->lcd_bitmap_transparent_part(
        (const fb_data *)mpeg_stock_assets.battery.data,
        0, battery_frame * MPEG_STOCK_BATTERY_FRAME_H,
        MPEG_STOCK_BATTERY_W, 289, 5,
        MPEG_STOCK_BATTERY_W, MPEG_STOCK_BATTERY_FRAME_H);

    fill_w = (int)(((uint64_t)current_time *
                    (MPEG_STOCK_PROGRESS_W - 6)) / total_time);
    if (fill_w < 0)
        fill_w = 0;
    if (fill_w > MPEG_STOCK_PROGRESS_W - 6)
        fill_w = MPEG_STOCK_PROGRESS_W - 6;

    rb->lcd_bitmap_transparent(
        (const fb_data *)mpeg_stock_assets.progress_frame.data,
        progress_x, progress_y,
        MPEG_STOCK_PROGRESS_W, MPEG_STOCK_PROGRESS_H);
    if (fill_w > 0)
    {
        int draw_w = current_time >= total_time ?
                     MPEG_STOCK_PROGRESS_W : fill_w + 3;

        rb->lcd_bitmap_transparent_part(
            (const fb_data *)mpeg_stock_assets.progress_fill.data,
            0, 0, MPEG_STOCK_PROGRESS_W, progress_x, progress_y,
            draw_w, MPEG_STOCK_PROGRESS_H);
        if (draw_w >= MPEG_STOCK_PROGRESS_CAP_W)
            rb->lcd_bitmap_transparent(
                (const fb_data *)mpeg_stock_assets.progress_fill_cap.data,
                progress_x + draw_w - MPEG_STOCK_PROGRESS_CAP_W,
                progress_y + 3,
                MPEG_STOCK_PROGRESS_CAP_W,
                MPEG_STOCK_PROGRESS_CAP_H);
    }
    mpeg_stock_draw_text_shadow(8, text_y, current);
    mpeg_stock_draw_text_shadow(
        LCD_WIDTH - 8 - dur_w, text_y, duration_text);

}

static void mpeg_volume_card_rect(int *x, int *y)
{
    int card_x = (LCD_WIDTH - MPEG_VOLUME_CARD_W) / 2;
    int card_y = (LCD_HEIGHT * 11) / 24;

    if (card_x < 0)
        card_x = 0;
    if (card_y + MPEG_VOLUME_CARD_H > LCD_HEIGHT)
        card_y = LCD_HEIGHT - MPEG_VOLUME_CARD_H;
    if (card_y < 0)
        card_y = 0;

    *x = card_x;
    *y = card_y;
}

static void mpeg_stock_draw_volume(void)
{
    int x;
    int y;
    int volume = rb->global_status != NULL ? rb->global_status->volume : 0;
    int min_volume = rb->sound_min(SOUND_VOLUME);
    int max_volume = rb->sound_max(SOUND_VOLUME);
    int percent;
    int fill_w;
    int icon_frame;
    int icon_x;
    int icon_y;
    int slider_x;
    int slider_y;

    if (!mpeg_volume_assets_loaded())
        return;

    mpeg_volume_card_rect(&x, &y);

    if (volume <= min_volume)
        percent = 0;
    else if (volume >= max_volume)
        percent = 100;
    else
        percent = ((volume - min_volume) * 100) /
                  (max_volume - min_volume);
    fill_w = (percent * MPEG_VOLUME_SLIDER_W) / 100;

    if (volume <= min_volume)
        icon_frame = 0;
    else if (volume <= -60)
        icon_frame = 1;
    else if (volume <= -30)
        icon_frame = 2;
    else
        icon_frame = 3;

    icon_x = x + 14;
    icon_y = y + 12;
    slider_x = x + 46;
    slider_y = y + 20;
    /* The card is keyed on magenta for its rounded corners; drawing it
     * opaque painted those corners over the video. */
    rb->lcd_bitmap_transparent(
        (const fb_data *)mpeg_volume_assets.backdrop.data,
        x, y, MPEG_VOLUME_CARD_W, MPEG_VOLUME_CARD_H);
    rb->lcd_bitmap_transparent_part(
        (const fb_data *)mpeg_volume_assets.icons.data,
        0, icon_frame * MPEG_VOLUME_ICON_H, MPEG_VOLUME_ICON_W,
        icon_x, icon_y, MPEG_VOLUME_ICON_W, MPEG_VOLUME_ICON_H);
    rb->lcd_bitmap(
        (const fb_data *)mpeg_volume_assets.slider_backdrop.data,
        slider_x, slider_y, MPEG_VOLUME_SLIDER_W,
        MPEG_VOLUME_SLIDER_H);
    if (fill_w > 0)
    {
        rb->lcd_bitmap_part(
            (const fb_data *)mpeg_volume_assets.slider_fill.data,
            0, 0, MPEG_VOLUME_SLIDER_W, slider_x, slider_y,
            fill_w, MPEG_VOLUME_SLIDER_H);
        rb->lcd_bitmap_transparent(
            (const fb_data *)mpeg_volume_assets.slider_end.data,
            slider_x + fill_w - MPEG_VOLUME_SLIDER_END_W, slider_y,
            MPEG_VOLUME_SLIDER_END_W, MPEG_VOLUME_SLIDER_H);
    }
}

static bool mpeg_volume_card_visible(void)
{
    return mpeg_volume_card_until != 0;
}

/* Repaint the card over each newly presented frame so it survives playing
 * video. Runs on the video thread, like the other post-frame callbacks. */
static void mpeg_volume_post_frame_callback(void)
{
    int x;
    int y;

    if (!mpeg_volume_card_visible())
        return;

    mpeg_volume_card_rect(&x, &y);

    vo_lock();
    mpeg_stock_draw_volume();
    draw_update_rect(x, y, MPEG_VOLUME_CARD_W, MPEG_VOLUME_CARD_H);
    vo_unlock();
}

/* Present the card and restart its timer, touching only its own rectangle. */
static void mpeg_volume_card_show(void)
{
    int x;
    int y;

    if (mpegplayer_maps_dashcam_launch || !mpeg_volume_assets_loaded())
        return;

    mpeg_volume_card_until = *rb->current_tick + MPEG_VOLUME_CARD_TIME;
    mpeg_volume_card_rect(&x, &y);

    mpeg_stock_draw_volume();

    vo_lock();
    draw_update_rect(x, y, MPEG_VOLUME_CARD_W, MPEG_VOLUME_CARD_H);
    vo_unlock();

    fps_update_post_frame_callback();
}

/* Drop the card. A playing stream would paint over it on its own, but a
 * paused one never would, so redraw the frame the way Live TV does. */
static void mpeg_volume_card_hide(void)
{
    if (!mpeg_volume_card_visible())
        return;

    mpeg_volume_card_until = 0;
    fps_update_post_frame_callback();
    stream_draw_frame(false);
}
#else
static bool mpeg_stock_assets_loaded(void)
{
    return false;
}
#endif

static void osd_text_init(void)
{
    struct hms hms;
    char buf[32];
    int phys;
    int spc_width;
    uint32_t duration;

    draw_setfont(FONT_UI);

    osd.use_wps_layout = false;
    osd.netflix_layout = mpegplayer_netflix_launch && !feed.active;
    osd.youtube_layout = mpegplayer_youtube_app_launch && !feed.active;
    osd.instagram_layout = mpegplayer_instagram_app_launch && !feed.active;
    osd.stock_layout = !osd.netflix_layout && !osd.youtube_layout &&
                       !osd.instagram_layout && !feed.active &&
                       mpeg_stock_assets_loaded();
    osd.x = 0;
    osd.width = SCREEN_WIDTH;

    vo_rect_clear(&osd.time_rect);
    vo_rect_clear(&osd.stat_rect);
    vo_rect_clear(&osd.prog_rect);
    vo_rect_clear(&osd.vol_rect);

#if MPEG_STOCK_CONTROLS
    if (osd.instagram_layout)
    {
        osd.use_wps_layout = true;
        osd.height = SCREEN_HEIGHT;
        osd.y = 0;
        draw_setfont(FONT_SYSFIXED);
        return;
    }

    if (osd.netflix_layout || osd.youtube_layout)
    {
        netflix_overlay_duration = stream_get_duration();
        osd.height = osd.youtube_layout ? MPEG_YOUTUBE_OVERLAY_H :
                     MPEG_NETFLIX_OVERLAY_H;
        osd.y = SCREEN_HEIGHT - osd.height;
        draw_setfont(FONT_SYSFIXED);
        return;
    }

    if (osd.stock_layout)
    {
        int volume_x = (SCREEN_WIDTH - MPEG_VOLUME_CARD_W) / 2;
        int volume_y = (SCREEN_HEIGHT * 11) / 24;

        osd.use_wps_layout = true;
        osd.height = SCREEN_HEIGHT;
        osd.y = 0;
        vo_rect_set_ext(&osd.vol_rect, volume_x, volume_y,
                        MPEG_VOLUME_CARD_W, MPEG_VOLUME_CARD_H);
        draw_setfont(FONT_SYSFIXED);
        return;
    }
#endif

    duration = stream_get_duration();
    ts_to_hms(duration, &hms);
    hms_format(buf, sizeof (buf), &hms);
    /* Calculate size for "duration / duration" format (worst case width) */
    {
        char temp[64];
        rb->snprintf(temp, sizeof(temp), "%s / %s", buf, buf);
        mylcd_getstringsize(temp, &osd.time_rect.r, &osd.time_rect.b);
    }

    /* Choose well-sized bitmap images relative to font height */
    if (osd.time_rect.b < 12) {
        osd.icons = mpegplayer_status_icons_8x8x1;
        osd.stat_rect.r = osd.stat_rect.b = 8;
    } else if (osd.time_rect.b < 16) {
        osd.icons = mpegplayer_status_icons_12x12x1;
        osd.stat_rect.r = osd.stat_rect.b = 12;
    } else {
        osd.icons = mpegplayer_status_icons_16x16x1;
        osd.stat_rect.r = osd.stat_rect.b = 16;
    }

    if (osd.stat_rect.b < osd.time_rect.b) {
        vo_rect_offset(&osd.stat_rect, 0,
                       (osd.time_rect.b - osd.stat_rect.b) / 2 + OSD_BDR_T);
        vo_rect_offset(&osd.time_rect, OSD_BDR_L, OSD_BDR_T);
    } else {
        vo_rect_offset(&osd.time_rect, OSD_BDR_L,
                       osd.stat_rect.b - osd.time_rect.b + OSD_BDR_T);
        vo_rect_offset(&osd.stat_rect, 0, OSD_BDR_T);
    }

    osd.dur_rect = osd.time_rect;

    phys = rb->sound_val2phys(SOUND_VOLUME, rb->sound_min(SOUND_VOLUME));
    rb->snprintf(buf, sizeof(buf), "%d%s", phys,
                 rb->sound_unit(SOUND_VOLUME));

    mylcd_getstringsize(" ", &spc_width, NULL);
    mylcd_getstringsize(buf, &osd.vol_rect.r, &osd.vol_rect.b);

    if (feed.active)
    {
        int bottom_top = SCREEN_HEIGHT - FEED_SCRIM_HEIGHT;
        int title_h = osd.time_rect.b;

        osd.use_wps_layout = true;
        vo_rect_set_ext(&osd.stat_rect, 0, 0, SCREEN_WIDTH, FEED_HEADER_HEIGHT);
        vo_rect_set_ext(&osd.dur_rect,
                        SCREEN_WIDTH - 52,
                        12,
                        40, title_h);
        vo_rect_set_ext(&osd.time_rect,
                        14,
                        bottom_top + 28,
                        SCREEN_WIDTH - 96,
                        title_h);
        vo_rect_set_ext(&osd.vol_rect,
                        SCREEN_WIDTH - FEED_HEART_SIZE - 14,
                        bottom_top + 8,
                        FEED_HEART_SIZE,
                        FEED_HEART_SIZE + title_h + 4);
        vo_rect_set_ext(&osd.prog_rect, 0, SCREEN_HEIGHT - 3, SCREEN_WIDTH, 3);

        osd.height = SCREEN_HEIGHT;
        osd.y = 0;
        draw_setfont(FONT_SYSFIXED);
        return;
    }

    if (osd.slider_bitmaps_loaded && osd.slider_bg_bmp.height == 12)
    {
        int time_w;
        int bar_x;
        int bar_w;
        int dur_x;

        /* Match iPone WPS slider layout */
        osd.use_wps_layout = true;

        osd_get_wps_slider_layout(duration, &time_w, &bar_x, &bar_w, &dur_x);

        vo_rect_set_ext(&osd.prog_rect, bar_x, 209,
                        bar_w,
                        osd.slider_bg_bmp.height);

        vo_rect_set_ext(&osd.time_rect, 2, 194,
                        time_w, osd.time_rect.b);

        vo_rect_set_ext(&osd.dur_rect,
                        dur_x,
                        194,
                        time_w, osd.time_rect.b);

        vo_rect_set_ext(&osd.stat_rect, 11, 3,
                        osd.stat_rect.r, osd.stat_rect.b);

        vo_rect_clear(&osd.vol_rect);

        osd.height = SCREEN_HEIGHT;
        osd.y = 0; /* WPS mode: coordinates are absolute, no offset needed */
    }
    else
    {
        osd.prog_rect.r = SCREEN_WIDTH - OSD_BDR_L - spc_width -
                               osd.vol_rect.r - OSD_BDR_R;
        osd.prog_rect.b = osd.stat_rect.b; /* Full icon height for better visibility */
        vo_rect_offset(&osd.prog_rect, osd.time_rect.l,
                       osd.time_rect.b);

        vo_rect_offset(&osd.stat_rect,
                       (osd.prog_rect.r + osd.prog_rect.l - osd.stat_rect.r) / 2,
                       0);

        vo_rect_offset(&osd.dur_rect,
                       osd.prog_rect.r - osd.dur_rect.r, 0);

        vo_rect_offset(&osd.vol_rect, osd.prog_rect.r + spc_width,
                       (osd.prog_rect.b + osd.prog_rect.t - osd.vol_rect.b) / 2);

        osd.height = OSD_BDR_T + MAX(osd.prog_rect.b, osd.vol_rect.b) -
                        MIN(osd.time_rect.t, osd.stat_rect.t) + OSD_BDR_B;

#ifdef HAVE_LCD_COLOR
        osd.height = ALIGN_UP(osd.height, 2);
#endif
        osd.y = SCREEN_HEIGHT - osd.height;
    }

    draw_setfont(FONT_SYSFIXED);
}

static void osd_init(void)
{
    osd.flags = 0;
    osd.show_for = feed.active ? HZ * 60 * 60 :
                   (mpegplayer_instagram_app_launch ? HZ * 2 : HZ * 4);
    osd.print_delay = 75*HZ/100;
    osd.resume_delay = HZ/2;
#ifdef HAVE_LCD_COLOR
    osd.bgcolor = LCD_RGBPACK(0x20, 0x20, 0x20);     /* Dark gray background */
    osd.fgcolor = LCD_RGBPACK(0x00, 0xcc, 0xff);     /* Bright cyan for progress */
    osd.prog_fillcolor = LCD_RGBPACK(0x40, 0x40, 0x40); /* Medium gray for unfilled */
#else
    osd.bgcolor = GREY_LIGHTGRAY;
    osd.fgcolor = GREY_BLACK;
    osd.prog_fillcolor = GREY_WHITE;
#endif
    osd.curr_time = 0;
    osd.status = OSD_STATUS_STOPPED;
    osd.auto_refresh = OSD_REFRESH_TIME;
    osd.next_auto_refresh = *rb->current_tick;
    osd.use_wps_layout = false;
    osd.stock_layout = false;
    osd.netflix_layout = false;
    osd.youtube_layout = false;
    osd.instagram_layout = false;
    
    /* The iPone slider layout makes the OSD update rectangle full-screen.
     * That is unsafe over YUV video: pause/seek/volume redraws can black out
     * most of the framebuffer. Keep normal MPEG playback on the bounded stock
     * OSD. Feed mode draws its own chrome independently. */
    osd.slider_bitmaps_loaded = false;
    
    osd_text_init();
    fps_init();
}

#ifdef HAVE_HEADPHONE_DETECTION
static void osd_set_hp_pause_flag(bool set)
{
    if (set)
        osd.flags |= OSD_HP_PAUSE;
    else
        osd.flags &= ~OSD_HP_PAUSE;
}
#else
#define osd_set_hp_pause_flag(set)
#endif /* HAVE_HEADPHONE_DETECTION */

static void osd_schedule_refresh(unsigned refresh)
{
    long tick = *rb->current_tick;

    if (refresh & OSD_REFRESH_VIDEO)
        osd.print_tick = tick + osd.print_delay;

    if (refresh & OSD_REFRESH_RESUME)
        osd.resume_tick = tick + osd.resume_delay;

    osd.auto_refresh |= refresh;
}

static void osd_cancel_refresh(unsigned refresh)
{
    osd.auto_refresh &= ~refresh;
}

/* Refresh the background area */
static void osd_refresh_background(void)
{
    char buf[32];
    struct hms hms;

    if (osd.instagram_layout)
    {
        vo_rect_set_ext(&osd.update_rect, 0, 0,
                        SCREEN_WIDTH, SCREEN_HEIGHT);
        return;
    }

    if (feed.active)
    {
#if IPODTIKTOK_USE_BITMAP_ASSETS
        rb->lcd_bitmap(ipodtiktok_header, 0, 0,
                       BMPWIDTH_ipodtiktok_header,
                       BMPHEIGHT_ipodtiktok_header);
        rb->lcd_bitmap(ipodtiktok_scrim, 0,
                       SCREEN_HEIGHT - BMPHEIGHT_ipodtiktok_scrim,
                       BMPWIDTH_ipodtiktok_scrim,
                       BMPHEIGHT_ipodtiktok_scrim);
#else
        mylcd_set_drawmode(DRMODE_SOLID);
        mylcd_set_foreground(LCD_RGBPACK(0x08, 0x08, 0x0d));
        draw_fillrect(0, 0, SCREEN_WIDTH, FEED_HEADER_HEIGHT);
        mylcd_set_foreground(LCD_RGBPACK(0x06, 0x06, 0x09));
        draw_fillrect(0, SCREEN_HEIGHT - FEED_SCRIM_HEIGHT,
                      SCREEN_WIDTH, FEED_SCRIM_HEIGHT);
#endif
        vo_rect_set_ext(&osd.update_rect, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        return;
    }

    if (osd.use_wps_layout)
    {
        /* WPS layout uses transparent background - don't clear anything */
        /* Just set up the drawing mode for text/graphics */
        mylcd_set_drawmode(DRMODE_SOLID);
        vo_rect_set_ext(&osd.update_rect, 0, 0, osd.width, osd.height);
        return;
    }

    unsigned bg = mylcd_get_background();
    mylcd_set_drawmode(DRMODE_SOLID | DRMODE_INVERSEVID);

#ifdef HAVE_LCD_COLOR
    /* Draw a "raised" area for our graphics */
    mylcd_set_background(draw_blendcolor(bg, MYLCD_WHITE, 192));
    draw_hline(0, osd.width, 0);

    mylcd_set_background(draw_blendcolor(bg, MYLCD_WHITE, 80));
    draw_hline(0, osd.width, 1);

    mylcd_set_background(draw_blendcolor(bg, MYLCD_BLACK, 48));
    draw_hline(0, osd.width, osd.height-2);

    mylcd_set_background(draw_blendcolor(bg, MYLCD_BLACK, 128));
    draw_hline(0, osd.width, osd.height-1);

    mylcd_set_background(bg);
    draw_clear_area(0, 2, osd.width, osd.height - 4);
#else
    /* Give contrast with the main background */
    mylcd_set_background(MYLCD_WHITE);
    draw_hline(0, osd.width, 0);

    mylcd_set_background(MYLCD_DARKGRAY);
    draw_hline(0, osd.width, osd.height-1);

    mylcd_set_background(bg);
    draw_clear_area(0, 1, osd.width, osd.height - 2);
#endif

    vo_rect_set_ext(&osd.update_rect, 0, 0, osd.width, osd.height);
    mylcd_set_drawmode(DRMODE_SOLID);

    if (stream_get_duration() != INVALID_TIMESTAMP) {
        /* Draw the movie duration */
        ts_to_hms(stream_get_duration(), &hms);
        hms_format(buf, sizeof (buf), &hms);
        draw_putsxy_oriented(osd.dur_rect.l, osd.dur_rect.t, buf);
    }
    /* else don't know the duration */
}

    /* Refresh the current time display + the progress bar */
    static void osd_refresh_time(void)
    {
        char buf[64];
        char time_str[32];
        char dur_str[32];
        struct hms hms;

    uint32_t duration = stream_get_duration();
    uint32_t time = osd.curr_time;

    if (osd.instagram_layout)
        return;

    if (duration == INVALID_TIMESTAMP || duration == 0)
        duration = 1;

    if (time > duration)
        time = duration;

    if (feed.active)
    {
        int counter_w = 0;
        char title_buf[FEED_TITLE_LEN + 8];

#if IPODTIKTOK_USE_BITMAP_ASSETS
        rb->lcd_bitmap_part(ipodtiktok_scrim,
                            osd.time_rect.l,
                            osd.time_rect.t - (SCREEN_HEIGHT - BMPHEIGHT_ipodtiktok_scrim),
                            BMPWIDTH_ipodtiktok_scrim,
                            osd.time_rect.l,
                            osd.time_rect.t,
                            osd.time_rect.r - osd.time_rect.l,
                            osd.time_rect.b - osd.time_rect.t + 2);
        rb->lcd_bitmap_part(ipodtiktok_header,
                            osd.dur_rect.l,
                            osd.dur_rect.t,
                            BMPWIDTH_ipodtiktok_header,
                            osd.dur_rect.l,
                            osd.dur_rect.t,
                            osd.dur_rect.r - osd.dur_rect.l,
                            osd.dur_rect.b - osd.dur_rect.t);
        rb->lcd_bitmap_part(ipodtiktok_scrim,
                            0,
                            BMPHEIGHT_ipodtiktok_scrim - (osd.prog_rect.b - osd.prog_rect.t),
                            BMPWIDTH_ipodtiktok_scrim,
                            0,
                            osd.prog_rect.t,
                            osd.prog_rect.r - osd.prog_rect.l,
                            osd.prog_rect.b - osd.prog_rect.t);
#else
        draw_clear_area_rect(&osd.time_rect);
        draw_clear_area_rect(&osd.dur_rect);
        draw_clear_area_rect(&osd.prog_rect);
#endif

        if (feed.index >= 0 && feed.index < feed.count)
        {
            rb->snprintf(buf, sizeof(buf), "%d/%d",
                         feed_section_position(), feed_section_count());
            mylcd_getstringsize(buf, &counter_w, NULL);
            mylcd_set_foreground(LCD_RGBPACK(0xff, 0xff, 0xff));
            draw_putsxy_oriented(osd.dur_rect.r - counter_w, osd.dur_rect.t, buf);

            feed_format_label(title_buf, sizeof(title_buf),
                              feed.items[feed.index].title,
                              osd.time_rect.r - osd.time_rect.l);
            mylcd_set_foreground(LCD_RGBPACK(0xff, 0xff, 0xff));
            draw_putsxy_oriented(osd.time_rect.l, osd.time_rect.t, title_buf);
            if (feed.items[feed.index].creator[0])
            {
                feed_format_label(title_buf, sizeof(title_buf),
                                  feed.items[feed.index].creator,
                                  osd.time_rect.r - osd.time_rect.l);
                draw_putsxy_oriented(osd.time_rect.l,
                                     osd.time_rect.t - 18, title_buf);
            }
            if (feed.items[feed.index].description[0])
            {
                feed_format_label(title_buf, sizeof(title_buf),
                                  feed.items[feed.index].description,
                                  osd.time_rect.r - osd.time_rect.l);
                mylcd_set_foreground(LCD_RGBPACK(0xd8, 0xd8, 0xdc));
                draw_putsxy_oriented(osd.time_rect.l,
                                     osd.time_rect.t + 18, title_buf);
            }
        }

        mylcd_set_foreground(LCD_RGBPACK(0x34, 0x34, 0x3d));
        draw_fillrect(osd.prog_rect.l, osd.prog_rect.t,
                      osd.prog_rect.r - osd.prog_rect.l,
                      osd.prog_rect.b - osd.prog_rect.t);
        mylcd_set_foreground(LCD_RGBPACK(0xff, 0x4d, 0x6d));
        draw_fillrect(osd.prog_rect.l, osd.prog_rect.t,
                      (duration > 0) ?
                      (int)muldiv_uint32(osd.prog_rect.r - osd.prog_rect.l,
                                         time, duration) : 0,
                      osd.prog_rect.b - osd.prog_rect.t);

        vo_rect_union(&osd.update_rect, &osd.update_rect, &osd.time_rect);
        vo_rect_union(&osd.update_rect, &osd.update_rect, &osd.dur_rect);
        vo_rect_union(&osd.update_rect, &osd.update_rect, &osd.prog_rect);
        return;
    }

    draw_scrollbar_draw_rect(&osd.prog_rect, 0, duration,
                             time);

    /* Format current time */
    ts_to_hms(time, &hms);
    hms_format(time_str, sizeof(time_str), &hms);
    
    /* Format total duration */
    ts_to_hms(duration, &hms);
    hms_format(dur_str, sizeof(dur_str), &hms);
    
    if (osd.use_wps_layout)
    {
        int time_w;
        int dur_w;
        int time_x;
        int dur_x;
        int time_y;
        int dur_y;
        
        /* iPone WPS colors */
        unsigned fg_color = 0xFFFFFF;      /* White text */
        unsigned shadow_color = 0x191523;  /* Dark shadow */
        unsigned glass_shadow = 0x15121b;  /* Glass shadow */
        unsigned glass_outer = 0x26222f;   /* Glass outer */
        unsigned glass_inner = 0x2d2936;   /* Glass inner */
        unsigned glass_highlight = 0x464056; /* Glass highlight */
        unsigned glass_lowlight = 0x18161f;  /* Glass lowlight */

        mylcd_getstringsize(time_str, &time_w, NULL);
        mylcd_getstringsize(dur_str, &dur_w, NULL);

        time_y = osd.time_rect.t + (osd.time_rect.b - osd.time_rect.t - 16) / 2;
        dur_y = osd.dur_rect.t + (osd.dur_rect.b - osd.dur_rect.t - 16) / 2;

        time_x = osd.time_rect.l + (osd.time_rect.r - osd.time_rect.l) - time_w;
        dur_x = osd.dur_rect.l;

        /* Draw text shadows first (NO glass card background in WPS overlay) */
        mylcd_set_foreground(shadow_color);
        draw_putsxy_oriented(time_x + 1, time_y + 1, time_str);
        draw_putsxy_oriented(dur_x + 1, dur_y + 1, dur_str);

        /* Draw main text */
        mylcd_set_foreground(fg_color);
        draw_putsxy_oriented(time_x, time_y, time_str);
        draw_putsxy_oriented(dur_x, dur_y, dur_str);

        vo_rect_union(&osd.update_rect, &osd.update_rect,
                      &osd.time_rect);
        vo_rect_union(&osd.update_rect, &osd.update_rect,
                      &osd.dur_rect);
    }
    else
    {
        /* Combine as "current / total" */
        rb->snprintf(buf, sizeof(buf), "%s / %s", time_str, dur_str);

        /* Do not clear time_rect in WPS mode! Just draw text. */
        draw_putsxy_oriented(osd.time_rect.l, osd.time_rect.t, buf);

        vo_rect_union(&osd.update_rect, &osd.update_rect,
                      &osd.time_rect);
    }

    vo_rect_union(&osd.update_rect, &osd.update_rect,
                  &osd.prog_rect);
}

/* Refresh the volume display area */
static void osd_refresh_volume(void)
{
    char buf[32];
    int width;

    if (osd.instagram_layout)
        return;

    if (feed.active)
    {
        int heart_x = osd.vol_rect.l;
        int heart_y = osd.vol_rect.t;

#if IPODTIKTOK_USE_BITMAP_ASSETS
        rb->lcd_bitmap_part(ipodtiktok_scrim,
                            heart_x,
                            MAX(0, heart_y - (SCREEN_HEIGHT - BMPHEIGHT_ipodtiktok_scrim)),
                            BMPWIDTH_ipodtiktok_scrim,
                            heart_x,
                            MAX(heart_y, SCREEN_HEIGHT - BMPHEIGHT_ipodtiktok_scrim),
                            FEED_HEART_SIZE,
                            MIN(FEED_HEART_SIZE + osd.time_rect.b + 4,
                                SCREEN_HEIGHT - MAX(heart_y, SCREEN_HEIGHT - BMPHEIGHT_ipodtiktok_scrim)));
        if (feed.index >= 0 && feed.index < feed.count &&
            feed.items[feed.index].liked)
            rb->lcd_bitmap(ipodtiktok_heart, heart_x, heart_y,
                           BMPWIDTH_ipodtiktok_heart,
                           BMPHEIGHT_ipodtiktok_heart);
        else
            rb->lcd_bitmap(ipodtiktok_heart_outline, heart_x, heart_y,
                           BMPWIDTH_ipodtiktok_heart_outline,
                           BMPHEIGHT_ipodtiktok_heart_outline);
#else
        draw_clear_area_rect(&osd.vol_rect);
#endif

        if (feed.index >= 0 && feed.index < feed.count)
        {
            rb->snprintf(buf, sizeof(buf), "%d",
                         feed.items[feed.index].like_count +
                         (feed.items[feed.index].liked ? 1 : 0));
            mylcd_getstringsize(buf, &width, NULL);
            mylcd_set_foreground(LCD_RGBPACK(0xff, 0xff, 0xff));
            draw_putsxy_oriented(heart_x + (FEED_HEART_SIZE - width) / 2,
                                 heart_y + FEED_HEART_SIZE + 4,
                                 buf);
        }

        vo_rect_union(&osd.update_rect, &osd.update_rect, &osd.vol_rect);
        return;
    }

    if (vo_rect_empty(&osd.vol_rect))
        return;

    int volume = rb->global_status->volume;
    rb->snprintf(buf, sizeof (buf), "%d%s",
                 rb->sound_val2phys(SOUND_VOLUME, volume),
                 rb->sound_unit(SOUND_VOLUME));
    mylcd_getstringsize(buf, &width, NULL);

    /* Right-justified */
    /* Do not clear vol_rect in WPS mode! Just draw text. */
    draw_putsxy_oriented(osd.vol_rect.r - width, osd.vol_rect.t, buf);

    vo_rect_union(&osd.update_rect, &osd.update_rect, &osd.vol_rect);
}

/* Refresh the status icon */
static void osd_refresh_status(void)
{
    int icon_size = osd.stat_rect.r - osd.stat_rect.l;

    if (osd.instagram_layout)
        return;

    if (feed.active)
    {
        const char *left = "Following";
        const char *right = "For You";
        int left_w;
        int right_w;
        int sep_w;
        int baseline_y = 12;
        int for_you_x;
        int following_x;
        int sep_x;

#if IPODTIKTOK_USE_BITMAP_ASSETS
        rb->lcd_bitmap_part(ipodtiktok_header, 0, 0,
                            BMPWIDTH_ipodtiktok_header,
                            0, 0,
                            BMPWIDTH_ipodtiktok_header,
                            BMPHEIGHT_ipodtiktok_header);
#endif

        mylcd_getstringsize(left, &left_w, NULL);
        mylcd_getstringsize(right, &right_w, NULL);
        mylcd_getstringsize("|", &sep_w, NULL);

        for_you_x = SCREEN_WIDTH / 2 - right_w / 2;
        sep_x = for_you_x - 12;
        following_x = sep_x - sep_w - 12 - left_w;

        mylcd_set_foreground(feed.section == FEED_SECTION_FOLLOWING ?
                             LCD_RGBPACK(0xff, 0xff, 0xff) :
                             LCD_RGBPACK(0xb0, 0xb5, 0xc0));
        draw_putsxy_oriented(following_x, baseline_y, left);
        mylcd_set_foreground(LCD_RGBPACK(0xb0, 0xb5, 0xc0));
        draw_putsxy_oriented(sep_x, baseline_y, "|");
        mylcd_set_foreground(feed.section == FEED_SECTION_FOR_YOU ?
                             LCD_RGBPACK(0xff, 0xff, 0xff) :
                             LCD_RGBPACK(0xb0, 0xb5, 0xc0));
        draw_putsxy_oriented(for_you_x, baseline_y, right);
        mylcd_set_foreground(LCD_RGBPACK(0xff, 0x4d, 0x6d));
        if (feed.section == FEED_SECTION_FOLLOWING)
            draw_fillrect(following_x + left_w / 2 - 18,
                          baseline_y + osd.time_rect.b + 1, 36, 2);
        else
            draw_fillrect(for_you_x + right_w / 2 - 18,
                          baseline_y + osd.time_rect.b + 1, 36, 2);

        vo_rect_union(&osd.update_rect, &osd.update_rect, &osd.stat_rect);
        return;
    }

    if (osd.use_wps_layout)
    {
        /* iPone WPS status icon with pill background */
        unsigned pill_color = 0x282434;  /* iPone PlayerStatePill color */
        
        /* No pill background behind status icon in WPS overlay mode */
        
        /* Draw the status icon */
#ifdef HAVE_LCD_COLOR
        unsigned oldfg = mylcd_get_foreground();
        mylcd_set_foreground(0xFFFFFF); /* White icon */
        draw_oriented_mono_bitmap_part(osd.icons,
                                       icon_size*osd.status,
                                       0,
                                       icon_size*OSD_STATUS_COUNT,
                                       osd.stat_rect.l + osd.x,
                                       osd.stat_rect.t + osd.y,
                                       icon_size, icon_size);
        mylcd_set_foreground(oldfg);
#else
        draw_oriented_mono_bitmap_part(osd.icons,
                                       icon_size*osd.status,
                                       0,
                                       icon_size*OSD_STATUS_COUNT,
                                       osd.stat_rect.l + osd.x,
                                       osd.stat_rect.t + osd.y,
                                       icon_size, icon_size);
#endif
    }
    else
    {
        /* Original generic OSD styling */
        /* Do not clear stat_rect in WPS mode! Only draw icon */

#ifdef HAVE_LCD_COLOR
        /* Draw status icon with a drop shadow */
        unsigned oldfg = mylcd_get_foreground();
        int i = 1;

        mylcd_set_foreground(draw_blendcolor(mylcd_get_background(),
                            MYLCD_BLACK, 96));

        while (1)
        {
            draw_oriented_mono_bitmap_part(osd.icons,
                                           icon_size*osd.status,
                                           0,
                                           icon_size*OSD_STATUS_COUNT,
                                           osd.stat_rect.l + osd.x + i,
                                           osd.stat_rect.t + osd.y + i,
                                           icon_size, icon_size);

            if (--i < 0)
                break;

            mylcd_set_foreground(oldfg);
        }
#else
        draw_oriented_mono_bitmap_part(osd.icons,
                                       icon_size*osd.status,
                                       0,
                                       icon_size*OSD_STATUS_COUNT,
                                       osd.stat_rect.l + osd.x,
                                       osd.stat_rect.t + osd.y,
                                       icon_size, icon_size);
#endif
    }

    vo_rect_union(&osd.update_rect, &osd.update_rect, &osd.stat_rect);
}

/* Update the current status which determines which icon is displayed */
static bool osd_update_status(void)
{
    int status;

    switch (stream_status())
    {
    default:
        status = OSD_STATUS_STOPPED;
        break;
    case STREAM_PAUSED:
        /* If paused with a pending resume, coerce it to OSD_STATUS_PLAYING */
        status = (osd.auto_refresh & OSD_REFRESH_RESUME) ?
            OSD_STATUS_PLAYING : OSD_STATUS_PAUSED;
        break;
    case STREAM_PLAYING:
        status = OSD_STATUS_PLAYING;
        break;
    }

    if (status != osd.status) {
        /* A refresh is needed */
        osd.status = status;
        return true;
    }

    return false;
}

/* Update the current time that will be displayed */
static void osd_update_time(void)
{
    uint32_t start;
    osd.curr_time = stream_get_seek_time(&start);
    osd.curr_time -= start;
}

/* Refresh various parts of the OSD - showing it if it is hidden */
static void osd_refresh(int hint)
{
    long tick;
    unsigned oldbg, oldfg;

    tick = *rb->current_tick;

    /* A dashcam opened by Maps is an immersive, edge-to-edge view.  Keep
     * timing/resume work in the normal button loop, but never promote a
     * forced status or volume refresh into visible player chrome. */
    if (mpegplayer_maps_dashcam_launch)
    {
        if (hint == OSD_REFRESH_DEFAULT)
        {
            if (osd.status == OSD_STATUS_PLAYING)
                rb->reset_poweroff_timer();

            if ((osd.auto_refresh & OSD_REFRESH_VIDEO) &&
                TIME_AFTER(tick, osd.print_tick))
            {
                osd.auto_refresh &= ~OSD_REFRESH_VIDEO;
                stream_draw_frame(false);
            }

            if ((osd.auto_refresh & OSD_REFRESH_RESUME) &&
                TIME_AFTER(tick, osd.resume_tick))
            {
                osd.auto_refresh &= ~(OSD_REFRESH_RESUME |
                                      OSD_REFRESH_VIDEO);
                stream_resume();
            }
        }
        return;
    }

    if (settings.showfps)
        fps_refresh();

    if (hint == OSD_REFRESH_DEFAULT) {
        /* The default which forces no updates */

        /* Make sure Rockbox doesn't turn off the player because of
           too little activity */
        if (osd.status == OSD_STATUS_PLAYING)
            rb->reset_poweroff_timer();

        /* Redraw the current or possibly extract a new video frame */
        if ((osd.auto_refresh & OSD_REFRESH_VIDEO) &&
            TIME_AFTER(tick, osd.print_tick)) {
            osd.auto_refresh &= ~OSD_REFRESH_VIDEO;
            stream_draw_frame(false);
        }

        /* Restart playback if the timout was reached */
        if ((osd.auto_refresh & OSD_REFRESH_RESUME) &&
            TIME_AFTER(tick, osd.resume_tick)) {
            osd.auto_refresh &= ~(OSD_REFRESH_RESUME | OSD_REFRESH_VIDEO);
            stream_resume();
        }

        /* If not visible, return */
        if (!(osd.flags & OSD_SHOW))
            return;

        if (TIME_AFTER(tick, osd.hide_tick)) {
            osd_show(OSD_HIDE);
            return;
        }
    } else {
        /* A forced update of some region */

        /* Show if currently invisible */
        if (!(osd.flags & OSD_SHOW)) {
            /* Avoid call back into this function - it will be drawn */
            osd_show(OSD_SHOW | OSD_NODRAW);
            hint = OSD_REFRESH_ALL;
        }

        /* Move back timeouts for frame print and hide */
        osd.print_tick = tick + osd.print_delay;
        osd.hide_tick = tick + osd.show_for;
    }

    if (TIME_AFTER(tick, osd.next_auto_refresh)) {
        /* Refresh whatever graphical elements are due automatically */
        osd.next_auto_refresh = tick + OSD_MIN_UPDATE_INTERVAL;

        if (osd.auto_refresh & OSD_REFRESH_STATUS) {
            if (osd_update_status())
                hint |= OSD_REFRESH_STATUS;
        }

        if (osd.auto_refresh & OSD_REFRESH_TIME) {
            osd_update_time();
            hint |= OSD_REFRESH_TIME;
        }
    }

    if (hint == 0)
        return; /* No drawing needed */

    /* Set basic drawing params that are used. Elements that perform variations
     * will restore them. */
    oldfg = mylcd_get_foreground();
    oldbg = mylcd_get_background();

    draw_setfont(FONT_UI);
    mylcd_set_foreground(osd.fgcolor);
    mylcd_set_background(osd.bgcolor);

    vo_rect_clear(&osd.update_rect);

    if (feed.active)
    {
        draw_setfont(FONT_SYSFIXED);
        mylcd_set_foreground(oldfg);
        mylcd_set_background(oldbg);
        /* Recompose the current picture and both gutters atomically in YUV.
         * Never paint TikTok's UI through the framebuffer callback. */
        stream_draw_frame(false);
        return;
    }

    if (osd.instagram_layout)
    {
        draw_setfont(FONT_SYSFIXED);
        mylcd_set_foreground(oldfg);
        mylcd_set_background(oldbg);
        /* The player chrome, live progress, and like state are composited
         * into the decoded frame. Never expose the RGB framebuffer beneath
         * it or let a generic full-screen refresh cover the video. */
        stream_draw_frame(false);
        return;
    }

#if MPEG_STOCK_CONTROLS
    if (osd.netflix_layout || osd.youtube_layout)
    {
        draw_setfont(FONT_SYSFIXED);
        mylcd_set_foreground(oldfg);
        mylcd_set_background(oldbg);
        stream_draw_frame(false);
        return;
    }

    if (osd.stock_layout && !feed.active)
    {
        bool progress_changed =
            hint & (OSD_REFRESH_BACKGROUND |
                    OSD_REFRESH_TIME |
                    OSD_REFRESH_STATUS);
        /* The volume card is not drawn from here. It owns its own timer and
         * rectangle in mpeg_volume_card_show()/_hide(), which keeps a volume
         * press from being promoted to OSD_REFRESH_ALL and flashing this
         * chrome on and off. */

        if (progress_changed)
            mpeg_stock_draw_progress();

        draw_setfont(FONT_SYSFIXED);
        mylcd_set_foreground(oldfg);
        mylcd_set_background(oldbg);
        vo_lock();
        /*
         * Header and progress are disjoint. Updating their bounding union
         * would present untouched black RGB framebuffer pixels over the YUV
         * frame and recreate the full-screen black overlay.
         */
        if (progress_changed)
        {
            draw_update_rect(0, 0, LCD_WIDTH, MPEG_STOCK_HEADER_H);
            draw_update_rect(
                0, LCD_HEIGHT - MPEG_STOCK_PROGRESS_H - 20,
                LCD_WIDTH, MPEG_STOCK_PROGRESS_H + 20);
        }
        vo_unlock();
        return;
    }
#endif

    if (hint & OSD_REFRESH_BACKGROUND) {
        osd_refresh_background();
        hint |= OSD_REFRESH_ALL; /* Requires a redraw of everything */
    }

    if (hint & OSD_REFRESH_TIME) {
        osd_refresh_time();
    }

    if (hint & OSD_REFRESH_VOLUME) {
        osd_refresh_volume();
    }

    if (hint & OSD_REFRESH_STATUS) {
        osd_refresh_status();
    }

    /* Update the dirty rectangle */
    draw_setfont(FONT_SYSFIXED);
    mylcd_set_foreground(oldfg);
    mylcd_set_background(oldbg);

    vo_lock();

    draw_update_rect(osd.update_rect.l,
                     osd.update_rect.t,
                     osd.update_rect.r - osd.update_rect.l,
                     osd.update_rect.b - osd.update_rect.t);

    vo_unlock();
}

/* Show/Hide the OSD */
static void osd_show(unsigned show)
{
    if (mpegplayer_maps_dashcam_launch)
    {
        osd.flags &= ~OSD_SHOW;
        stream_vo_set_clip(NULL);
        return;
    }

    /* Inline Instagram owns a permanent feed surface, not a timed playback
     * OSD. Ignore the generic two-second hide until Select explicitly marks
     * the stream expanded; that transition then follows the normal hide path
     * below and reveals full-screen video. */
    if (!(show & OSD_SHOW) && mpegplayer_instagram_feed_launch &&
        !mpegplayer_instagram_feed_expanded && !mpeg_stop_requested)
    {
        osd.flags |= OSD_SHOW;
        return;
    }

    if (((show ^ osd.flags) & OSD_SHOW) == 0)
    {
        if (show & OSD_SHOW) {
            osd.hide_tick = *rb->current_tick + osd.show_for;
        }
        return;
    }

    if (show & OSD_SHOW) {
        osd.flags |= OSD_SHOW;

        if (osd.status != OSD_STATUS_PLAYING) {
            /* Not playing - set brightness to mpegplayer setting */
            osd_backlight_brightness_video_mode(true);
        }

        if (feed.active) {
            /* The fixed portrait picture leaves black control gutters. */
            stream_vo_set_clip(NULL);
        } else if (mpegplayer_instagram_feed_launch &&
                   !mpegplayer_instagram_feed_expanded) {
            /* The feed preview is a clipped square, not a reduced
             * full-screen player. The YUV overlay owns the surrounding
             * Instagram chrome; Select restores the ordinary full screen. */
            struct vo_rect rc = {
                MPEG_INSTAGRAM_VIDEO_LEFT, MPEG_INSTAGRAM_VIDEO_TOP,
                MPEG_INSTAGRAM_VIDEO_LEFT + MPEG_INSTAGRAM_VIDEO_W,
                MPEG_INSTAGRAM_VIDEO_TOP + MPEG_INSTAGRAM_VIDEO_H
            };
            stream_vo_set_clip(&rc);
        } else if (osd.netflix_layout || osd.youtube_layout ||
                   osd.use_wps_layout) {
            /* YUV-composited and WPS overlays leave video full-screen. */
            stream_vo_set_clip(NULL);
        } else {
            /* Clip away the part of video that is covered by the OSD strip */
            struct vo_rect rc = { 0, 0, SCREEN_WIDTH, osd.y };
            stream_vo_set_clip(&rc);
        }

        if (!(show & OSD_NODRAW))
            osd_refresh(OSD_REFRESH_ALL);
    } else {
        /* Uncover clipped video area and redraw it */
        osd.flags &= ~OSD_SHOW;

        if (!osd.netflix_layout && !osd.youtube_layout &&
            !osd.use_wps_layout) {
            /* Only draw clear background in non-WPS mode */
            draw_clear_area(0, 0, osd.width, osd.height);
        }

        if (!(show & OSD_NODRAW)) {
            stream_vo_set_clip(NULL);
            if (!osd.netflix_layout && !osd.youtube_layout)
            {
                vo_lock();
                draw_update_rect(0, 0, osd.width, osd.height);
                vo_unlock();
            }
            stream_draw_frame(false);
        } else {
            stream_vo_set_clip(NULL);
        }

        if (osd.status != OSD_STATUS_PLAYING) {
            /* Not playing - restore backlight brightness */
            osd_backlight_brightness_video_mode(false);
        }
    }
}

/* Set the current status - update screen if specified */
static void osd_set_status(int status)
{
    bool draw = (status & OSD_NODRAW) == 0;

    status &= OSD_STATUS_MASK;

    if (osd.status != status) {

        osd.status = status;

        if (draw)
            osd_refresh(OSD_REFRESH_STATUS);
    }
}

/* Get the current status value */
static int osd_get_status(void)
{
    return osd.status & OSD_STATUS_MASK;
}

/* Handle Fast-forward/Rewind keys using WPS settings (and some nicked code ;)
 * Returns last button code
 */
static int osd_ff_rw(int btn, unsigned refresh, uint32_t *new_time)
{
    const long ff_rw_accel = (rb->global_settings->ff_rewind_accel + 3);
    uint32_t start;
    uint32_t time = stream_get_seek_time(&start);
    const uint32_t duration = stream_get_duration();
    unsigned int step =
        TS_SECOND * rb->global_settings->ff_rewind_min_step;
    unsigned int max_step = 0;
    uint32_t ff_rw_count = 0;
    unsigned status = osd.status;
    int new_btn;

    osd_cancel_refresh(OSD_REFRESH_VIDEO | OSD_REFRESH_RESUME |
                       OSD_REFRESH_TIME);

    /* Scale the initial step for long videos. A one-second step makes a
     * multi-hour stream appear frozen, especially during static intros. */
    step = MAX(step, duration / 200);
    step = MAX(step, MIN_FF_REWIND_STEP);

    time -= start; /* Absolute clock => stream-relative */

    switch (btn)
    {
    case MPEG_FF:
#ifdef MPEG_FF2
    case MPEG_FF2:
#endif
#ifdef MPEG_RC_FF
    case MPEG_RC_FF:
#endif
        osd_set_status(OSD_STATUS_FF);
        new_btn = btn | BUTTON_REPEAT; /* simplify code below */
        break;
    case MPEG_RW:
#ifdef MPEG_RW2
    case MPEG_RW2:
#endif
#ifdef MPEG_RC_RW
    case MPEG_RC_RW:
#endif
        osd_set_status(OSD_STATUS_RW);
        new_btn = btn | BUTTON_REPEAT; /* simplify code below */
        break;
    default:
        new_btn = BUTTON_NONE; /* Fail tests below but still do proper exit */
    }

    while (1)
    {
        stream_keep_disk_active();

        if (new_btn == (btn | BUTTON_REPEAT)) {
            if (osd.status == OSD_STATUS_FF) {
                /* fast forwarding, calc max step relative to end */
                max_step = muldiv_uint32(duration - (time + ff_rw_count),
                                         FF_REWIND_MAX_PERCENT, 100);
            } else {
                /* rewinding, calc max step relative to start */
                max_step = muldiv_uint32(time - ff_rw_count,
                                         FF_REWIND_MAX_PERCENT, 100);
            }

            max_step = MAX(max_step, MIN_FF_REWIND_STEP);

            if (step > max_step)
                step = max_step;

            ff_rw_count += step;

            /* smooth seeking by multiplying step by: 1 + (2 ^ -accel) */
            step += step >> ff_rw_accel;

            if (osd.status == OSD_STATUS_FF) {
                if (duration - time <= ff_rw_count)
                    ff_rw_count = duration - time;

                osd.curr_time = time + ff_rw_count;
            } else {
                if (time <= ff_rw_count)
                    ff_rw_count = time;

                osd.curr_time = time - ff_rw_count;
            }

            osd_refresh(OSD_REFRESH_TIME);

            new_btn = mpeg_button_get(FF_REWIND_BUTTON_TIMEOUT);
        }
        else {
            if (new_btn == (btn | BUTTON_REL) ||
                new_btn == BUTTON_NONE) {
                if (osd.status == OSD_STATUS_FF)
                    time += ff_rw_count;
                else if (osd.status == OSD_STATUS_RW)
                    time -= ff_rw_count;
            }

            *new_time = time;

            osd_schedule_refresh(refresh);
            osd_set_status(status);
            osd_schedule_refresh(OSD_REFRESH_TIME);

            return new_btn;
        }
    }
}

/* Return adjusted STREAM_* status */
static int osd_stream_status(void)
{
    int status = stream_status();

    /* Coerce to STREAM_PLAYING if paused with a pending resume */
    if (status == STREAM_PAUSED) {
        if (osd.auto_refresh & OSD_REFRESH_RESUME)
            status = STREAM_PLAYING;
    }

    return status;
}

/* Change the current audio volume by a specified amount */
static void osd_set_volume(int delta)
{
    int vol = rb->global_status->volume;
    int limit;

    vol += delta;

    if (delta < 0) {
        /* Volume down - clip to lower limit */
        limit = rb->sound_min(SOUND_VOLUME);
        if (vol < limit)
            vol = limit;
    } else {
        /* Volume up - clip to upper limit */
        limit = rb->sound_max(SOUND_VOLUME);
        if (vol > limit)
            vol = limit;
    }

    /* Sync the global settings */
    if (vol != rb->global_status->volume)
        rb->sound_set(SOUND_VOLUME, vol);

    if (mpegplayer_maps_dashcam_launch)
        return;

    if (feed.active)
    {
        feed.volume_until = *rb->current_tick + FEED_VOLUME_TIME;
        osd_refresh(OSD_REFRESH_VOLUME);
        return;
    }

#ifdef HAVE_LCD_COLOR
    if (mpegplayer_livetv_launch || mpegplayer_netflix_launch)
    {
        /* Desktop Mode owns a bounded video window whose clip must not be
         * replaced by the full-screen receiver OSD. */
        if (!mpegplayer_livetv_desktop)
            livetv_volume_show();
        return;
    }
#endif

#if MPEG_STOCK_CONTROLS
    if (osd.youtube_layout && !feed.active)
    {
        osd_show(OSD_SHOW);
        osd_refresh(OSD_REFRESH_VOLUME | OSD_REFRESH_TIME);
        return;
    }

    /*
     * The stock card presents itself. Routing it through osd_refresh() would
     * promote a volume-only refresh to OSD_REFRESH_ALL whenever the OSD is
     * hidden, dragging the header band and progress strip on screen and then
     * clearing them again - the flash. Its own timer and update rectangle
     * keep the card to its own pixels, so it can also be shown while the
     * stream is playing.
     */
    if (osd.stock_layout && !feed.active)
    {
        mpeg_volume_card_show();
        return;
    }
#endif

    /* Some full-screen layouts intentionally omit the volume overlay. */
    if (!vo_rect_empty(&osd.vol_rect))
        osd_refresh(OSD_REFRESH_VOLUME);
}

/* Begin playback at the specified time */
static int osd_play(uint32_t time)
{
    int retval;

    osd_set_hp_pause_flag(false);
    osd_cancel_refresh(OSD_REFRESH_VIDEO | OSD_REFRESH_RESUME);

    retval = stream_seek(time, SEEK_SET);

    if (retval >= STREAM_OK) {
        osdbacklight_hw_on_video_mode(true);
        osd_backlight_brightness_video_mode(true);
        stream_show_vo(true);

        retval = feed.active && feed.transition_enter_pending ?
                 stream_play_primed() : stream_play();

        if (retval >= STREAM_OK)
            osd_set_status(OSD_STATUS_PLAYING | OSD_NODRAW);
    }

    return retval;
}

/* Halt playback - pause engine and return logical state */
static int osd_halt(void)
{
    int status = stream_pause();

    /* Coerce to STREAM_PLAYING if paused with a pending resume */
    if (status == STREAM_PAUSED) {
        if (osd_get_status() == OSD_STATUS_PLAYING)
            status = STREAM_PLAYING;
    }

    /* Cancel some auto refreshes - caller will restart them if desired */
    osd_cancel_refresh(OSD_REFRESH_VIDEO | OSD_REFRESH_RESUME);

    /* No backlight fiddling here - callers does the right thing */

    return status;
}

/* Pause playback if playing */
static int osd_pause(void)
{
    unsigned refresh = osd.auto_refresh;
    int status = osd_halt();

    osd_set_hp_pause_flag(false);

    if (status == STREAM_PLAYING && (refresh & OSD_REFRESH_RESUME)) {
        /* Resume pending - change to a still video frame update */
        osd_schedule_refresh(OSD_REFRESH_VIDEO);
    }

    osd_set_status(OSD_STATUS_PAUSED);

    osdbacklight_hw_on_video_mode(false);
    /* Leave brightness alone and restore it when OSD is hidden */

    if (stream_can_seek() && rb->global_settings->pause_rewind) {
        stream_seek(-rb->global_settings->pause_rewind*TS_SECOND,
                    SEEK_CUR);
        osd_schedule_refresh(OSD_REFRESH_VIDEO);
        /* Update time display now */
        osd_update_time();
        osd_refresh(OSD_REFRESH_TIME);
    }

    return status;
}

/* Resume playback if halted or paused */
static void osd_resume(void)
{
    /* Cancel video and resume auto refresh - the resyc when starting
     * playback will perform those tasks */
    osd_set_hp_pause_flag(false);
    osdbacklight_hw_on_video_mode(true);
    osd_backlight_brightness_video_mode(true);
    osd_cancel_refresh(OSD_REFRESH_VIDEO | OSD_REFRESH_RESUME);
    osd_set_status(OSD_STATUS_PLAYING);
    stream_resume();
}

/* Stop playback - remember the resume point if not closed */
static void osd_stop(void)
{
    uint32_t resume_time;

    osd_set_hp_pause_flag(false);
    mpeg_stop_requested = true;
    osd_cancel_refresh(OSD_REFRESH_VIDEO | OSD_REFRESH_RESUME);
    osd_set_status(OSD_STATUS_STOPPED | OSD_NODRAW);
    osd_show(OSD_HIDE);

#ifdef HAVE_LCD_COLOR
    if (mpegplayer_livetv_launch)
        livetv_volume_hide();
#endif

#if MPEG_STOCK_CONTROLS
    /* Retire the card before the stream goes away; there will be no further
     * frame to paint it over and nothing left to redraw underneath it. */
    if (mpeg_volume_card_visible())
    {
        mpeg_volume_card_until = 0;
        fps_update_post_frame_callback();
    }
#endif

    stream_stop();

    resume_time = stream_get_resume_time();

    if (resume_time != INVALID_TIMESTAMP)
        settings.resume_time = resume_time;

    osdbacklight_hw_on_video_mode(false);
    osd_backlight_brightness_video_mode(false);
}

/* Perform a seek by button if seeking is possible for this stream.
 *
 * A delay will be inserted before restarting in case the user decides to
 * seek again soon after.
 *
 * Returns last button code
 */
static int osd_seek_btn(int btn)
{
    int status;
    unsigned refresh = 0;
    uint32_t time;

    if (!stream_can_seek())
        return true;

    /* Halt playback - not strictly necessary but nice when doing
     * buttons */
    status = osd_halt();

    if (status == STREAM_STOPPED)
        return true;

    osd_show(OSD_SHOW);

    /* Obtain a new playback point according to the buttons */
    if (status == STREAM_PLAYING)
        refresh = OSD_REFRESH_RESUME; /* delay resume if playing */
    else
        refresh = OSD_REFRESH_VIDEO;  /* refresh if paused */

    btn = osd_ff_rw(btn, refresh, &time);

    /* Tell engine to resume at that time */
    stream_seek(time, SEEK_SET);

    if (refresh == OSD_REFRESH_RESUME)
    {
        osd_cancel_refresh(OSD_REFRESH_RESUME | OSD_REFRESH_VIDEO);
        osd_resume();
    }

    return btn;
}

/* Perform a seek by time if seeking is possible for this stream
 *
 * If playing, the seeking is immediate, otherise a delay is added to showing
 * a still if paused in case the user does another seek soon after.
 *
 * If seeking isn't possible, a time of zero performs a skip to the
 * beginning.
 */
static void osd_seek_time(uint32_t time)
{
    int status;
    unsigned refresh = 0;

    if (!stream_can_seek() && time != 0)
        return;

    stream_wait_status();
    status = osd_stream_status();

    if (status == STREAM_STOPPED)
        return;

    if (status == STREAM_PLAYING)    /* merely preserve resume */
        refresh = osd.auto_refresh & OSD_REFRESH_RESUME;
    else
        refresh = OSD_REFRESH_VIDEO; /* refresh if paused */

    /* Cancel print or resume if pending */
    osd_cancel_refresh(OSD_REFRESH_VIDEO | OSD_REFRESH_RESUME);

    /* Tell engine to seek to the given time - no state change */
    stream_seek(time, SEEK_SET);

    osd_update_time();
    osd_refresh(OSD_REFRESH_TIME);
    osd_schedule_refresh(refresh);
}

/* Has this file one of the supported extensions? */
static bool is_videofile(const char* file)
{
    static const char * const extensions[] =
    {
        /* Should match apps/plugins/viewers.config */
        "mpg", "mpeg", "mpv", "m2v"
    };

    const char* ext = rb->strrchr(file, '.');
    int i;

    if (!ext)
        return false;

    for (i = ARRAYLEN(extensions) - 1; i >= 0; i--)
    {
        if (!rb->strcasecmp(ext + 1, extensions[i]))
            break;
    }

    return i >= 0;
}

/* deliver the next/previous video file in the current directory.
   returns false if there is none. */
static bool get_videofile(int direction, char* videofile, size_t bufsize)
{
    struct tree_context *tree = rb->tree_get_context();
    struct entry *dircache = rb->tree_get_entries(tree);
    int i, step, end, found = 0;
    char *videoname = rb->strrchr(videofile, '/') + 1;
    size_t rest = bufsize - (videoname - videofile) - 1;

    if (direction == VIDEO_NEXT) {
        i = 0;
        step = 1;
        end = tree->filesindir;
    } else {
        i = tree->filesindir-1;
        step = -1;
        end = -1;
    }
    for (; i != end; i += step)
    {
        const char* name = dircache[i].name;
        if (!rb->strcmp(name, videoname)) {
            found = 1;
            continue;
        }
        if (found && rb->strlen(name) <= rest &&
            !(dircache[i].attr & ATTR_DIRECTORY) && is_videofile(name))
        {
            rb->strcpy(videoname, name);
            return true;
        }
    }

    return false;
}

#ifdef HAVE_HEADPHONE_DETECTION
/* Handle SYS_PHONE_PLUGGED/UNPLUGGED */
static void osd_handle_phone_plug(bool inserted)
{
    if (rb->global_settings->unplug_mode == 0)
        return;

    /* Wait for any incomplete state transition to complete first */
    stream_wait_status();

    int status = osd_stream_status();

    if (inserted) {
        if (rb->global_settings->unplug_mode > 1) {
            if (status == STREAM_PAUSED &&
                (osd.flags & OSD_HP_PAUSE)) {
                osd_resume();
            }
        }
    } else {
        if (status == STREAM_PLAYING) {
            osd_pause();

            osd_set_hp_pause_flag(true);
        }
    }
}
#endif

#ifdef HAVE_LCD_COLOR
/* The guide is Live TV's home screen: the app opens on it and MENU returns
 * to it, with the channel still decoding into the corner window. */
static bool livetv_show_guide = true;
static bool livetv_banner_pending;
/* Consecutive channels whose programme would not open. Bounded by the
 * lineup size so a broken install cannot spin forever. */
static int livetv_open_failures;

/* Timed overlays over the full screen picture. The video is clipped away
 * from the overlay strip so the decoder does not paint over it. */
static long livetv_overlay_until;
#define LIVETV_VOLUME_H 50
#define LIVETV_VOLUME_TIME (HZ * 2)
#define LIVETV_VOLUME_GREEN LCD_RGBPACK(32, 255, 80)
#define LIVETV_VOLUME_DIM LCD_RGBPACK(12, 72, 28)
#define LIVETV_VOLUME_SEGMENTS 16
#define LIVETV_VOLUME_BAR_W 112
static long livetv_volume_until;

static unsigned char mpeg_yuv_clamp(int value)
{
    if (value < 0)
        return 0;
    if (value > 255)
        return 255;
    return value;
}

static void mpeg_yuv_color(int red, int green, int blue,
                           unsigned char *y, unsigned char *u,
                           unsigned char *v)
{
    *y = mpeg_yuv_clamp(
        ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16);
    *u = mpeg_yuv_clamp(
        ((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128);
    *v = mpeg_yuv_clamp(
        ((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128);
}

static void mpeg_yuv_pixel(uint8_t * const *planes, int width, int height,
                           int x, int y, int red, int green, int blue)
{
    unsigned char py;
    unsigned char pu;
    unsigned char pv;

    if (x < 0 || x >= width || y < 0 || y >= height)
        return;

    mpeg_yuv_color(red, green, blue, &py, &pu, &pv);
    planes[0][y * width + x] = py;
    planes[1][(y / 2) * (width / 2) + x / 2] = pu;
    planes[2][(y / 2) * (width / 2) + x / 2] = pv;
}

static void mpeg_yuv_rect(uint8_t * const *planes, int width, int height,
                          int x, int y, int rect_w, int rect_h,
                          int red, int green, int blue)
{
    int row;
    int col;

    for (row = MAX(0, y); row < MIN(height, y + rect_h); row++)
        for (col = MAX(0, x); col < MIN(width, x + rect_w); col++)
            mpeg_yuv_pixel(planes, width, height, col, row,
                           red, green, blue);
}

static int mpeg_yuv_text_width(const char *text)
{
    struct font *font = rb->font_get(FONT_SYSFIXED);
    int width = 0;

    while (*text != '\0')
        width += rb->font_get_width(font, (unsigned char)*text++);
    return width;
}

static void mpeg_yuv_text(uint8_t * const *planes, int width, int height,
                          int x, int y, const char *text,
                          int red, int green, int blue)
{
    struct font *font = rb->font_get(FONT_SYSFIXED);

    if (font == NULL || font->depth != 0)
        return;

    while (*text != '\0' && x < width)
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
                    mpeg_yuv_pixel(planes, width, height, x + col, y + row,
                                   red, green, blue);
            }
        }
        x += glyph_w;
    }
}

static void mpeg_yuv_text_scaled(uint8_t * const *planes,
                                 int width, int height,
                                 int x, int y, const char *text, int scale,
                                 int red, int green, int blue)
{
    struct font *font = rb->font_get(FONT_SYSFIXED);

    if (font == NULL || font->depth != 0 || scale < 1)
        return;

    while (*text != '\0' && x < width)
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
                    mpeg_yuv_rect(planes, width, height,
                                  x + col * scale, y + row * scale,
                                  scale, scale, red, green, blue);
            }
        }
        x += glyph_w * scale;
    }
}

static void mpeg_yuv_overlay_title(char *title, size_t size, int max_width)
{
    const char *base = mpeg_osd_path;
    char *dot;

    if (base[0] == '\0')
        base = "Video";
    else if (rb->strrchr(base, '/') != NULL)
        base = rb->strrchr(base, '/') + 1;

    rb->strlcpy(title, base, size);
    dot = rb->strrchr(title, '.');
    if (dot != NULL)
        *dot = '\0';

    while (title[0] != '\0' &&
           mpeg_yuv_text_width(title) > max_width)
    {
        size_t length = rb->strlen(title);

        if (length <= 3)
            break;
        title[length - 1] = '\0';
    }
}

#if MPEG_STOCK_CONTROLS
static void mpeg_yuv_youtube_sprite(uint8_t * const *planes,
                                    int width, int height,
                                    const struct bitmap *sprite,
                                    int dst_x, int dst_y)
{
    int row;
    int col;

    for (row = 0; row < sprite->height; row++)
        for (col = 0; col < sprite->width; col++)
        {
            fb_data pixel =
                ((const fb_data *)sprite->data)[row * sprite->width + col];
            int red = FB_UNPACK_RED(pixel);
            int green = FB_UNPACK_GREEN(pixel);
            int blue = FB_UNPACK_BLUE(pixel);

            if (red > 248 && green < 8 && blue > 248)
                continue;

            mpeg_yuv_pixel(planes, width, height,
                           dst_x + col, dst_y + row,
                           red, green, blue);
        }
}

static void mpeg_yuv_youtube_asset_column(uint8_t * const *planes,
                                          int width, int height,
                                          int src_x, int dst_x, int dst_y,
                                          int draw_width,
                                          int draw_height)
{
    int row;
    int col;

    for (row = 0; row < draw_height; row++)
    {
        fb_data pixel =
            ((const fb_data *)youtube_player_bmp.data)
            [(dst_y + row) * youtube_player_bmp.width + src_x];

        for (col = 0; col < draw_width; col++)
            mpeg_yuv_pixel(planes, width, height,
                           dst_x + col, dst_y + row,
                           FB_UNPACK_RED(pixel),
                           FB_UNPACK_GREEN(pixel),
                           FB_UNPACK_BLUE(pixel));
    }
}

static void mpeg_yuv_feed_bitmap(uint8_t * const *planes,
                                 int width, int height,
                                 const fb_data *pixels,
                                 int bitmap_width, int bitmap_height,
                                 int src_x, int src_y,
                                 int dst_x, int dst_y,
                                 int draw_width, int draw_height,
                                 bool black_is_transparent)
{
    int row;
    int col;

    for (row = 0; row < draw_height; row++)
        for (col = 0; col < draw_width; col++)
        {
            fb_data pixel;
            int red;
            int green;
            int blue;

            if (src_x + col >= bitmap_width ||
                src_y + row >= bitmap_height)
                continue;
            pixel = pixels[(src_y + row) * bitmap_width + src_x + col];
            red = FB_UNPACK_RED(pixel);
            green = FB_UNPACK_GREEN(pixel);
            blue = FB_UNPACK_BLUE(pixel);
            if (black_is_transparent &&
                ((red < 4 && green < 4 && blue < 4) ||
                 (red > 248 && green < 8 && blue > 248)))
                continue;
            mpeg_yuv_pixel(planes, width, height,
                           dst_x + col, dst_y + row, red, green, blue);
        }
}

static void mpeg_yuv_feed_bitmap_scaled_alpha(
    uint8_t * const *planes, int width, int height,
    const fb_data *pixels, int bitmap_width, int bitmap_height,
    int dst_x, int dst_y, int draw_width, int draw_height, int alpha)
{
    int row;
    int col;

    if (draw_width <= 0 || draw_height <= 0 || alpha <= 0)
        return;
    alpha = MIN(256, alpha);
    for (row = 0; row < draw_height; row++)
        for (col = 0; col < draw_width; col++)
        {
            int x = dst_x + col;
            int y = dst_y + row;
            int src_x = col * bitmap_width / draw_width;
            int src_y = row * bitmap_height / draw_height;
            fb_data pixel = pixels[src_y * bitmap_width + src_x];
            int red = FB_UNPACK_RED(pixel);
            int green = FB_UNPACK_GREEN(pixel);
            int blue = FB_UNPACK_BLUE(pixel);
            unsigned char py;
            unsigned char pu;
            unsigned char pv;
            int index;

            if (x < 0 || x >= width || y < 0 || y >= height ||
                (red < 4 && green < 4 && blue < 4))
                continue;
            mpeg_yuv_color(red, green, blue, &py, &pu, &pv);
            index = y * width + x;
            planes[0][index] =
                (planes[0][index] * (256 - alpha) + py * alpha) >> 8;
            if ((x & 1) == 0 && (y & 1) == 0)
            {
                index = (y / 2) * (width / 2) + x / 2;
                planes[1][index] =
                    (planes[1][index] * (256 - alpha) + pu * alpha) >> 8;
                planes[2][index] =
                    (planes[2][index] * (256 - alpha) + pv * alpha) >> 8;
            }
        }
}
#endif

int mpegplayer_yuv_overlay_height(void)
{
    if (feed.active)
        return LCD_HEIGHT;
    if (osd.instagram_layout && (osd.flags & OSD_SHOW))
        return LCD_HEIGHT;
    if ((mpegplayer_livetv_launch || mpegplayer_netflix_launch) &&
        !mpegplayer_livetv_desktop && livetv_volume_until != 0)
        return LIVETV_VOLUME_H;
    if (mpegplayer_livetv_launch && !mpegplayer_livetv_desktop &&
        mpegplayer_livetv_weather_commercial)
        return LIVETV_WEATHER_COMMERCIAL_OVERLAY_H;
#if MPEG_STOCK_CONTROLS
    if (osd.youtube_layout && (osd.flags & OSD_SHOW))
        return MPEG_YOUTUBE_OVERLAY_H;
    if (osd.netflix_layout && (osd.flags & OSD_SHOW))
        return MPEG_NETFLIX_OVERLAY_H;
    if (mpegplayer_netflix_launch &&
        netflix_skip_active != NETFLIX_SKIP_NONE)
        return MPEG_NETFLIX_SKIP_H;
#endif
    return 0;
}

int mpegplayer_yuv_overlay_y(void)
{
    if ((mpegplayer_livetv_launch || mpegplayer_netflix_launch) &&
        !mpegplayer_livetv_desktop && livetv_volume_until != 0)
        return LCD_HEIGHT - LIVETV_VOLUME_H;

    return LCD_HEIGHT - mpegplayer_yuv_overlay_height();
}

int mpegplayer_yuv_overlay_offset(void)
{
    if (feed.active && !feed.profile_visible)
        return feed.swipe_offset;
    return 0;
}

int mpegplayer_yuv_overlay_resistance_offset(void)
{
    return feed.active ? feed.resistance_offset : 0;
}

int mpegplayer_yuv_overlay_transition_direction(void)
{
    return feed.transition_direction == VIDEO_PREV ? -1 : 1;
}

bool mpegplayer_yuv_overlay_transition_active(void)
{
    return feed.active && feed.transition_enter_pending;
}

bool mpegplayer_yuv_overlay_capture_pending(void)
{
    return feed.active && feed.transition_capture_pending;
}

bool mpegplayer_instagram_inline_rect(struct vo_rect *rect)
{
    if (!mpegplayer_instagram_feed_launch ||
        mpegplayer_instagram_feed_expanded || rect == NULL)
        return false;
    vo_rect_set_ext(rect, MPEG_INSTAGRAM_VIDEO_LEFT,
                    MPEG_INSTAGRAM_VIDEO_TOP,
                    MPEG_INSTAGRAM_VIDEO_W, MPEG_INSTAGRAM_VIDEO_H);
    return true;
}

void mpegplayer_yuv_overlay_draw(uint8_t * const *planes,
                                 int width, int height)
{
    if (osd.instagram_layout)
    {
        char likes[24];
        int video_left = mpegplayer_instagram_feed_launch ?
                         MPEG_INSTAGRAM_VIDEO_LEFT : 0;
        int video_top = mpegplayer_instagram_feed_launch ?
                        MPEG_INSTAGRAM_VIDEO_TOP : 0;
        int video_width = mpegplayer_instagram_feed_launch ?
                          MPEG_INSTAGRAM_VIDEO_W : width;

        if (mpegplayer_instagram_feed_launch &&
            !mpegplayer_instagram_feed_expanded)
        {
            /* This is the same fixed 2010 feed card used by instagram.rock,
             * baked into each decoded frame like the Live TV receiver UI.
             * All values were read before playback started; this path is
             * strictly bounded YUV composition, with no framebuffer use,
             * disk access, or allocation. */
            /* Do not repaint the decoded card. The compositor runs after
             * the decoder, so the surrounding feed background is split into
             * four bands instead of clearing the full frame. */
            mpeg_yuv_rect(planes, width, height, 0, 0, width,
                          MPEG_INSTAGRAM_VIDEO_TOP, 247, 245, 239);
            mpeg_yuv_rect(planes, width, height, 0,
                          MPEG_INSTAGRAM_VIDEO_TOP + MPEG_INSTAGRAM_VIDEO_H,
                          width, 222 - (MPEG_INSTAGRAM_VIDEO_TOP +
                          MPEG_INSTAGRAM_VIDEO_H), 247, 245, 239);
            mpeg_yuv_rect(planes, width, height, 0,
                          MPEG_INSTAGRAM_VIDEO_TOP,
                          MPEG_INSTAGRAM_VIDEO_LEFT,
                          MPEG_INSTAGRAM_VIDEO_H, 247, 245, 239);
            mpeg_yuv_rect(planes, width, height,
                          MPEG_INSTAGRAM_VIDEO_LEFT + MPEG_INSTAGRAM_VIDEO_W,
                          MPEG_INSTAGRAM_VIDEO_TOP,
                          width - (MPEG_INSTAGRAM_VIDEO_LEFT +
                          MPEG_INSTAGRAM_VIDEO_W),
                          MPEG_INSTAGRAM_VIDEO_H, 247, 245, 239);
            mpeg_yuv_rect(planes, width, height, 0, 0, width, 28,
                          43, 79, 107);                /* IG_NAVY */
            mpeg_yuv_rect(planes, width, height, 0, 28, width, 28,
                          241, 250, 254);              /* IG_SELECTED */
            mpeg_yuv_rect(planes, width, height, 3, 56,
                          MPEG_INSTAGRAM_VIDEO_W + 2, 1,
                          213, 217, 220);               /* IG_BORDER */
            mpeg_yuv_rect(planes, width, height, 3, 56, 1,
                          MPEG_INSTAGRAM_VIDEO_H + 2,
                          213, 217, 220);
            mpeg_yuv_rect(planes, width, height,
                          MPEG_INSTAGRAM_VIDEO_LEFT + MPEG_INSTAGRAM_VIDEO_W,
                          56, 1, MPEG_INSTAGRAM_VIDEO_H + 2,
                          213, 217, 220);
            mpeg_yuv_rect(planes, width, height, 3,
                          MPEG_INSTAGRAM_VIDEO_TOP + MPEG_INSTAGRAM_VIDEO_H,
                          MPEG_INSTAGRAM_VIDEO_W + 2, 1,
                          213, 217, 220);
            mpeg_yuv_rect(planes, width, height, 0, 222, width, 18,
                          43, 79, 107);
            mpeg_yuv_text(planes, width, height, 33, 7, "Instagram",
                          255, 255, 255);
            mpeg_yuv_text(planes, width, height, 267, 34, "HOME",
                          118, 118, 118);
            mpeg_yuv_rect(planes, width, height, 4, 32, 18, 18,
                          231, 238, 243);
            mpeg_yuv_text(planes, width, height, 7, 37, "@",
                          63, 114, 150);
            mpeg_yuv_text(planes, width, height, 28, 35,
                          instagram_username[0] ? instagram_username :
                          "Instagram", 63, 114, 150);
            mpeg_yuv_feed_bitmap(planes, width, height,
                                 instagram_liked ? instagram_heart :
                                 instagram_heart_unliked,
                                 instagram_liked ? BMPWIDTH_instagram_heart :
                                 BMPWIDTH_instagram_heart_unliked,
                                 instagram_liked ? BMPHEIGHT_instagram_heart :
                                 BMPHEIGHT_instagram_heart_unliked,
                                 0, 0, 174, 60, 32, 32, true);
            rb->snprintf(likes, sizeof(likes), "%d likes",
                         instagram_likes + (instagram_liked ? 1 : 0));
            mpeg_yuv_text(planes, width, height, 210, 68, likes,
                          63, 114, 150);
            mpeg_yuv_text(planes, width, height, 172, 96, "VIDEO POST",
                          118, 118, 118);
            mpeg_yuv_text(planes, width, height, 172, 122,
                          instagram_username[0] ? instagram_username :
                          "Instagram", 63, 114, 150);
            mpeg_yuv_text(planes, width, height, 172, 142,
                          instagram_caption[0] ? instagram_caption :
                          "Video", 0, 0, 0);
            mpeg_yuv_text(planes, width, height, 172, 202,
                          "Select full screen", 118, 118, 118);
            mpeg_yuv_text(planes, width, height, 8, 226,
                          "Home       Favorites       Profile",
                          255, 255, 255);
        }
        /* Authentic play artwork is retained as the compact feed-video cue. */
        mpeg_yuv_feed_bitmap_scaled_alpha(
            planes, width, height,
            instagram_video_play,
            BMPWIDTH_instagram_video_play,
            BMPHEIGHT_instagram_video_play,
            video_left + video_width -
                BMPWIDTH_instagram_video_play - MPEG_INSTAGRAM_PLAY_MARGIN,
            video_top + MPEG_INSTAGRAM_PLAY_MARGIN,
            BMPWIDTH_instagram_video_play,
            BMPHEIGHT_instagram_video_play, 224);
        return;
    }

    if (feed.active && feed.index >= 0 && feed.index < feed.count)
    {
        const struct feed_item *item = &feed.items[feed.index];
        char text[FEED_DESCRIPTION_LEN];
        uint32_t duration = stream_get_duration();
        uint32_t time = osd.curr_time;
        int heart_x = FEED_VIDEO_RIGHT +
                      (width - FEED_VIDEO_RIGHT - FEED_HEART_SIZE) / 2;
        int heart_y = 56;
        int track_x = FEED_SIDE_GUTTER + 4;
        int track_w = FEED_VIDEO_LEFT - 2 * FEED_SIDE_GUTTER - 8;
        int text_width;
        int progress;

        if (feed.details_visible)
        {
            const char *caption = item->description[0] ?
                                  item->description : item->title;
            int offset = 0;
            int line;

            mpeg_yuv_rect(planes, width, height, 0, 0, width, height,
                          8, 8, 10);
#if IPODTIKTOK_USE_BITMAP_ASSETS
            mpeg_yuv_feed_bitmap(planes, width, height,
                                 ipodtiktok_header,
                                 BMPWIDTH_ipodtiktok_header,
                                 BMPHEIGHT_ipodtiktok_header,
                                 8, 11, 12, 10, 76, 18, false);
#endif
            mpeg_yuv_text(planes, width, height, 246, 12, "DETAILS",
                          145, 145, 152);
            mpeg_yuv_rect(planes, width, height, 8, 40, width - 16, 1,
                          48, 48, 52);
            feed_format_label(text, sizeof(text), item->creator, width - 24);
            mpeg_yuv_text(planes, width, height, 12, 54, text,
                          37, 244, 238);
            feed_format_label(text, sizeof(text), item->title, width - 24);
            mpeg_yuv_text(planes, width, height, 12, 76, text,
                          255, 255, 255);
            for (line = 0; line < 5 && caption[offset] != '\0'; line++)
            {
                int take = MIN(48, (int)rb->strlen(caption + offset));
                int end = take;

                if (caption[offset + take] != '\0')
                    while (end > 24 && caption[offset + end] != ' ')
                        end--;
                if (end <= 24)
                    end = take;
                rb->strlcpy(text, caption + offset,
                            MIN((size_t)end + 1, sizeof(text)));
                feed_format_label(text, sizeof(text), text, width - 24);
                mpeg_yuv_text(planes, width, height, 12, 104 + line * 18,
                              text, 205, 205, 210);
                offset += end;
                while (caption[offset] == ' ')
                    offset++;
            }
            mpeg_yuv_rect(planes, width, height, 8, 208, width - 16, 1,
                          48, 48, 52);
            mpeg_yuv_text(planes, width, height, 12, 218,
                          "MENU  Back", 145, 145, 152);
            return;
        }

        if (feed.saved_profiles_visible)
        {
            int top = feed.saved_profile_cursor > 3 ?
                      feed.saved_profile_cursor - 3 : 0;
            int row;

            mpeg_yuv_rect(planes, width, height, 0, 0, width, height,
                          8, 8, 10);
#if IPODTIKTOK_USE_BITMAP_ASSETS
            mpeg_yuv_feed_bitmap(planes, width, height,
                                 ipodtiktok_header,
                                 BMPWIDTH_ipodtiktok_header,
                                 BMPHEIGHT_ipodtiktok_header,
                                 8, 11, 12, 10, 76, 18, false);
#else
            mpeg_yuv_text(planes, width, height, 8, 10, "TikTok",
                          255, 255, 255);
#endif
            mpeg_yuv_text(planes, width, height, 262, 12, "SAVED",
                          37, 244, 238);
            mpeg_yuv_rect(planes, width, height, 8, 40, width - 16, 1,
                          48, 48, 52);
            mpeg_yuv_text(planes, width, height, 12, 49,
                          "Saved profiles", 255, 255, 255);
            mpeg_yuv_text(planes, width, height, 213, 49,
                          "SELECT TO OPEN", 145, 145, 152);

            for (row = 0; row < 4 && top + row < feed.saved_profile_count;
                 row++)
            {
                int ordinal = top + row;
                int item_index = feed_saved_profile_items[ordinal];
                int y = 71 + row * 38;
                bool selected = ordinal == feed.saved_profile_cursor;
                const char *creator = feed.items[item_index].creator;

                if (selected)
                    mpeg_yuv_rect(planes, width, height, 8, y - 4,
                                  width - 16, 34, 21, 70, 70);
                mpeg_yuv_rect(planes, width, height, 13, y, 24, 24,
                              selected ? 37 : 45,
                              selected ? 244 : 45,
                              selected ? 238 : 49);
                mpeg_yuv_text(planes, width, height, 20, y + 7, "@",
                              255, 255, 255);
                feed_format_label(text, sizeof(text), creator, 184);
                mpeg_yuv_text(planes, width, height, 48, y + 2, text,
                              255, 255, 255);
                rb->snprintf(text, sizeof(text), "%u saved",
                             (unsigned)feed_saved_profile_counts[ordinal]);
                mpeg_yuv_text(planes, width, height, 48, y + 17, text,
                              145, 145, 152);
                mpeg_yuv_text(planes, width, height, 294, y + 7, ">",
                              37, 244, 238);
            }
            if (feed.saved_profile_count == 0)
                mpeg_yuv_text(planes, width, height, 73, 118,
                              "No saved videos yet", 185, 185, 190);
            mpeg_yuv_rect(planes, width, height, 8, 218,
                          width - 16, 1, 48, 48, 52);
            mpeg_yuv_text(planes, width, height, 12, 224,
                          "Wheel  Browse    Select  Videos    Menu  Back",
                          145, 145, 152);
            return;
        }

        if (feed.profile_visible)
        {
            int start;
            int shown = 0;

            mpeg_yuv_rect(planes, width, height, 0, 0, width, height,
                          8, 8, 10);
#if IPODTIKTOK_USE_BITMAP_ASSETS
            mpeg_yuv_feed_bitmap(planes, width, height,
                                 ipodtiktok_header,
                                 BMPWIDTH_ipodtiktok_header,
                                 BMPHEIGHT_ipodtiktok_header,
                                 8, 11, 12, 10, 76, 18, false);
#else
            mpeg_yuv_text(planes, width, height, 8, 10, "TikTok",
                          255, 255, 255);
#endif
            mpeg_yuv_text(planes, width, height, 246, 12, "PROFILE",
                          145, 145, 152);
            mpeg_yuv_rect(planes, width, height, 8, 40, width - 16, 1,
                          48, 48, 52);

            if (feed_profile_avatar_valid)
            {
                mpeg_yuv_rect(planes, width, height, 258, 47, 54, 54,
                              37, 244, 238);
                mpeg_yuv_feed_bitmap(
                    planes, width, height,
                    (const fb_data *)feed_profile_avatar.data,
                    feed_profile_avatar.width, feed_profile_avatar.height,
                    0, 0, 261, 50, FEED_PROFILE_AVATAR_SIZE,
                    FEED_PROFILE_AVATAR_SIZE, false);
            }

            feed_format_label(text, sizeof(text),
                              feed.profile.display_name, 198);
            mpeg_yuv_text(planes, width, height, 12, 52, text,
                          255, 255, 255);
            if (feed.profile.verified)
            {
                int name_width = mpeg_yuv_text_width(text);
                mpeg_yuv_feed_bitmap(
                    planes, width, height, ipodtiktok_verified,
                    BMPWIDTH_ipodtiktok_verified,
                    BMPHEIGHT_ipodtiktok_verified,
                    0, 0, MIN(238, 16 + name_width), 51,
                    BMPWIDTH_ipodtiktok_verified,
                    BMPHEIGHT_ipodtiktok_verified, true);
            }
            rb->snprintf(text, sizeof(text), "@%s", feed.profile.username);
            feed_format_label(text, sizeof(text), text, 198);
            mpeg_yuv_text(planes, width, height, 12, 68, text,
                          37, 244, 238);

            feed_format_count(text, sizeof(text), feed.profile.followers);
            mpeg_yuv_text(planes, width, height, 12, 92, text,
                          255, 255, 255);
            mpeg_yuv_text(planes, width, height, 12, 106, "Followers",
                          145, 145, 152);
            feed_format_count(text, sizeof(text), feed.profile.following);
            mpeg_yuv_text(planes, width, height, 92, 92, text,
                          255, 255, 255);
            mpeg_yuv_text(planes, width, height, 92, 106, "Following",
                          145, 145, 152);
            feed_format_count(text, sizeof(text), feed.profile.likes);
            mpeg_yuv_text(planes, width, height, 180, 92, text,
                          255, 255, 255);
            mpeg_yuv_text(planes, width, height, 180, 106, "Likes",
                          145, 145, 152);
            feed_format_count(text, sizeof(text), feed.profile.videos);
            mpeg_yuv_text(planes, width, height, 250, 92, text,
                          255, 255, 255);
            mpeg_yuv_text(planes, width, height, 250, 106, "Videos",
                          145, 145, 152);

            feed_format_label(text, sizeof(text),
                              feed.profile.bio[0] ? feed.profile.bio :
                              item->description, width - 24);
            mpeg_yuv_text(planes, width, height, 12, 128, text,
                          205, 205, 210);
            mpeg_yuv_rect(planes, width, height, 8, 148, width - 16, 1,
                          48, 48, 52);
            mpeg_yuv_text(planes, width, height, 12, 153,
                          feed.profile_saved_only ? "SAVED" : "VIDEOS",
                          145, 145, 152);

            start = feed.profile_cursor > 0 ? feed.profile_cursor - 1 : 0;
            if (feed.profile_count > 3 && start > feed.profile_count - 3)
                start = feed.profile_count - 3;
            while (shown < 3 && start + shown < feed.profile_count)
            {
                int ordinal = start + shown;
                int item_index = feed_profile_item_index(ordinal);
                bool selected = ordinal == feed.profile_cursor;

                if (item_index < 0)
                    break;
                mpeg_yuv_rect(planes, width, height,
                              7 + shown * 104, 167, 98, 66,
                              selected ? 37 : 45,
                              selected ? 244 : 45,
                              selected ? 238 : 49);
                if (feed_profile_thumb_valid[shown])
                    mpeg_yuv_feed_bitmap(
                        planes, width, height,
                        (const fb_data *)feed_profile_thumbs[shown].data,
                        feed_profile_thumbs[shown].width,
                        feed_profile_thumbs[shown].height,
                        0, 4, 8 + shown * 104, 168, 96, 64, false);
                else
                {
                    mpeg_yuv_rect(planes, width, height,
                                  8 + shown * 104, 168, 96, 64,
                                  22, 22, 25);
                    feed_format_label(text, sizeof(text),
                                      feed.items[item_index].title, 84);
                        mpeg_yuv_text(planes, width, height,
                                  14 + shown * 104, 192, text,
                                  185, 185, 190);
                }
                if (feed.items[item_index].pin_order > 0)
                {
                    mpeg_yuv_rect(planes, width, height,
                                  12 + shown * 104, 172, 42, 12,
                                  254, 44, 85);
                    mpeg_yuv_text(planes, width, height,
                                  15 + shown * 104, 174, "PINNED",
                                  255, 255, 255);
                }
                shown++;
            }
            if (shown == 0)
                mpeg_yuv_text(planes, width, height, 20, 177,
                              "Current video", 185, 185, 190);
            return;
        }

        if (duration == INVALID_TIMESTAMP || duration == 0)
            duration = 1;
        if (time > duration)
            time = duration;
        progress = (int)muldiv_uint32(track_w, time, duration);

        mpeg_yuv_rect(planes, width, height, 0, 0,
                      FEED_VIDEO_LEFT, height, 8, 8, 10);
        mpeg_yuv_rect(planes, width, height, FEED_VIDEO_RIGHT, 0,
                      width - FEED_VIDEO_RIGHT, height, 8, 8, 10);
        mpeg_yuv_rect(planes, width, height, FEED_VIDEO_LEFT - 1, 0,
                      1, height, 38, 38, 42);
        mpeg_yuv_rect(planes, width, height, FEED_VIDEO_RIGHT, 0,
                      1, height, 38, 38, 42);

#if IPODTIKTOK_USE_BITMAP_ASSETS
        mpeg_yuv_feed_bitmap(planes, width, height,
                             ipodtiktok_header,
                             BMPWIDTH_ipodtiktok_header,
                             BMPHEIGHT_ipodtiktok_header,
                             8, 11, 8, 10, 76, 18, false);
#else
        mpeg_yuv_text(planes, width, height, 8, 10, "TikTok",
                      255, 255, 255);
#endif
        mpeg_yuv_rect(planes, width, height, FEED_SIDE_GUTTER, 42,
                      FEED_VIDEO_LEFT - 2 * FEED_SIDE_GUTTER, 1,
                      48, 48, 52);
        mpeg_yuv_text(planes, width, height, FEED_SIDE_GUTTER, 48,
                      "For You",
                      feed.section == FEED_SECTION_FOR_YOU ? 255 : 153,
                      feed.section == FEED_SECTION_FOR_YOU ? 255 : 153,
                      feed.section == FEED_SECTION_FOR_YOU ? 255 : 157);
        mpeg_yuv_text(planes, width, height, FEED_SIDE_GUTTER, 62,
                      "Following",
                      feed.section == FEED_SECTION_FOLLOWING ? 255 : 153,
                      feed.section == FEED_SECTION_FOLLOWING ? 255 : 153,
                      feed.section == FEED_SECTION_FOLLOWING ? 255 : 157);
        mpeg_yuv_text(planes, width, height, FEED_SIDE_GUTTER, 76,
                      "Saved",
                      feed.section == FEED_SECTION_SAVED ? 255 : 153,
                      feed.section == FEED_SECTION_SAVED ? 255 : 153,
                      feed.section == FEED_SECTION_SAVED ? 255 : 157);
        mpeg_yuv_text(planes, width, height, FEED_SIDE_GUTTER, 90,
                      "History",
                      feed.section == FEED_SECTION_HISTORY ? 255 : 153,
                      feed.section == FEED_SECTION_HISTORY ? 255 : 153,
                      feed.section == FEED_SECTION_HISTORY ? 255 : 157);
        mpeg_yuv_rect(planes, width, height, FEED_SIDE_GUTTER + 1,
                      59 + (int)feed.section * 14,
                      43, 1, 254, 44, 85);
        mpeg_yuv_rect(planes, width, height, FEED_SIDE_GUTTER,
                      58 + (int)feed.section * 14,
                      43, 1, 37, 244, 238);

        mpeg_yuv_text(planes, width, height, FEED_SIDE_GUTTER, 111,
                      "Now Playing", 125, 125, 132);
        feed_format_label(text, sizeof(text),
                          item->creator[0] ? item->creator : item->title,
                          FEED_VIDEO_LEFT - 2 * FEED_SIDE_GUTTER);
        mpeg_yuv_text(planes, width, height, FEED_SIDE_GUTTER, 125,
                      text, 255, 255, 255);
        feed_format_label(text, sizeof(text),
                          item->description[0] ?
                          item->description : item->title,
                          FEED_VIDEO_LEFT - 2 * FEED_SIDE_GUTTER);
        mpeg_yuv_text(planes, width, height, FEED_SIDE_GUTTER, 142,
                      text, 190, 190, 196);

        if (TIME_BEFORE(*rb->current_tick, feed.volume_until))
        {
            int min_volume = rb->sound_min(SOUND_VOLUME);
            int max_volume = rb->sound_max(SOUND_VOLUME);
            int volume_width = 0;
            int knob_x;

            if (rb->global_settings->volume_limit >= min_volume &&
                rb->global_settings->volume_limit < max_volume)
                max_volume = rb->global_settings->volume_limit;

            if (max_volume > min_volume)
                volume_width = (rb->global_status->volume - min_volume) *
                    track_w /
                    (max_volume - min_volume);
            volume_width = MIN(track_w, MAX(0, volume_width));
            knob_x = track_x + MIN(track_w - 5,
                                   MAX(0, volume_width - 2));
            rb->snprintf(text, sizeof(text), "Volume %d%%",
                         max_volume > min_volume ?
                         (rb->global_status->volume - min_volume) * 100 /
                         (max_volume - min_volume) : 0);
            mpeg_yuv_text(planes, width, height, FEED_SIDE_GUTTER, 188,
                          text, 255, 255, 255);
            mpeg_yuv_rect(planes, width, height, track_x, 208,
                          track_w, 3, 63, 63, 68);
            mpeg_yuv_rect(planes, width, height, track_x, 208,
                          volume_width, 3, 238, 238, 242);
            mpeg_yuv_rect(planes, width, height,
                          knob_x, 207,
                          5, 5, 37, 244, 238);
        }
        else
        {
            rb->snprintf(text, sizeof(text), "%lu:%02lu / %lu:%02lu",
                         (unsigned long)(time / TS_SECOND / 60),
                         (unsigned long)(time / TS_SECOND % 60),
                         (unsigned long)(duration / TS_SECOND / 60),
                         (unsigned long)(duration / TS_SECOND % 60));
            mpeg_yuv_text(planes, width, height, FEED_SIDE_GUTTER, 188,
                          text, 255, 255, 255);
            mpeg_yuv_rect(planes, width, height, track_x, 208,
                          track_w, 3, 63, 63, 68);
            mpeg_yuv_rect(planes, width, height, track_x, 208,
                          progress, 3, 238, 238, 242);
            mpeg_yuv_rect(planes, width, height,
                          track_x + MIN(track_w - 5,
                                        MAX(0, progress - 2)), 207,
                          5, 5, 254, 44, 85);
        }

        rb->strlcpy(text,
                    osd_stream_status() == STREAM_PAUSED ? "PAUSED" : "PLAYING",
                    sizeof(text));
        text_width = mpeg_yuv_text_width(text);
        mpeg_yuv_text(planes, width, height,
                      FEED_VIDEO_RIGHT +
                      (width - FEED_VIDEO_RIGHT - text_width) / 2,
                      16, text, 145, 145, 152);
        mpeg_yuv_rect(planes, width, height, FEED_VIDEO_RIGHT + 8, 41,
                      width - FEED_VIDEO_RIGHT - 16, 1, 48, 48, 52);

#if IPODTIKTOK_USE_BITMAP_ASSETS
        mpeg_yuv_feed_bitmap(
            planes, width, height,
            item->liked ? ipodtiktok_heart : ipodtiktok_heart_outline,
            FEED_HEART_SIZE, FEED_HEART_SIZE,
            0, 0, heart_x, heart_y,
            FEED_HEART_SIZE, FEED_HEART_SIZE, true);
        if (item->liked &&
            TIME_BEFORE(*rb->current_tick, feed.like_anim_until))
        {
            long remaining = feed.like_anim_until - *rb->current_tick;
            long elapsed = FEED_LIKE_ANIM_TIME - remaining;
            int peak = MAX(1, FEED_LIKE_ANIM_TIME / 3);
            int size;
            int alpha = (int)(remaining * 256 / FEED_LIKE_ANIM_TIME);

            if (elapsed < peak)
                size = 20 + (int)(elapsed * 22 / peak);
            else
                size = 42 - (int)((elapsed - peak) * 10 /
                                  MAX(1, FEED_LIKE_ANIM_TIME - peak));
            mpeg_yuv_feed_bitmap_scaled_alpha(
                planes, width, height, ipodtiktok_heart,
                FEED_HEART_SIZE, FEED_HEART_SIZE,
                heart_x + (FEED_HEART_SIZE - size) / 2,
                heart_y + (FEED_HEART_SIZE - size) / 2,
                size, size, alpha);
        }
#endif
        feed_format_count(text, sizeof(text),
                          item->like_count + (item->liked ? 1 : 0));
        text_width = mpeg_yuv_text_width(text);
        mpeg_yuv_text(planes, width, height,
                      heart_x + (FEED_HEART_SIZE - text_width) / 2,
                      heart_y + FEED_HEART_SIZE + 3,
                      text, 255, 255, 255);
        text_width = mpeg_yuv_text_width("Likes");
        mpeg_yuv_text(planes, width, height,
                      FEED_VIDEO_RIGHT +
                      (width - FEED_VIDEO_RIGHT - text_width) / 2,
                      104, "Likes", 145, 145, 152);
        mpeg_yuv_rect(planes, width, height, FEED_VIDEO_RIGHT + 8, 122,
                      width - FEED_VIDEO_RIGHT - 16, 1, 48, 48, 52);
        text_width = mpeg_yuv_text_width("Comments");
        mpeg_yuv_text(planes, width, height,
                      FEED_VIDEO_RIGHT +
                      (width - FEED_VIDEO_RIGHT - text_width) / 2,
                      132, "Comments", 190, 190, 196);
        feed_format_count(text, sizeof(text), item->comment_count);
        text_width = mpeg_yuv_text_width(text);
        mpeg_yuv_text(planes, width, height,
                      FEED_VIDEO_RIGHT +
                      (width - FEED_VIDEO_RIGHT - text_width) / 2,
                      148, text, 255, 255, 255);
        mpeg_yuv_rect(planes, width, height, FEED_VIDEO_RIGHT + 8, 176,
                      width - FEED_VIDEO_RIGHT - 16, 1, 48, 48, 52);
        rb->snprintf(text, sizeof(text), "%d / %d",
                     feed_section_position(), feed_section_count());
        text_width = mpeg_yuv_text_width(text);
        mpeg_yuv_text(planes, width, height,
                      FEED_VIDEO_RIGHT +
                      (width - FEED_VIDEO_RIGHT - text_width) / 2,
                      205, text, 255, 255, 255);
        rb->strlcpy(text, feed_section_label(feed.section), sizeof(text));
        text_width = mpeg_yuv_text_width(text);
        mpeg_yuv_text(planes, width, height,
                      FEED_VIDEO_RIGHT +
                      (width - FEED_VIDEO_RIGHT - text_width) / 2,
                      220, text, 37, 244, 238);

        if (TIME_BEFORE(*rb->current_tick, feed.confirm_until))
        {
            mpeg_yuv_rect(planes, width, height, 6, 160, 80, 24,
                          15, 45, 46);
#if IPODTIKTOK_USE_BITMAP_ASSETS
            mpeg_yuv_feed_bitmap(
                planes, width, height, ipodtiktok_check,
                BMPWIDTH_ipodtiktok_check, BMPHEIGHT_ipodtiktok_check,
                0, 0, 7, 160, 24, 24, true);
#endif
            mpeg_yuv_text(planes, width, height, 34, 168,
                          feed.confirm_text, 255, 255, 255);
        }

        if (feed.action_visible)
        {
            static const char * const actions[4] = {
                "Save", "Not interested", "Profile", "Details"
            };
            int i;

            mpeg_yuv_rect(planes, width, height, 0, 38,
                          FEED_VIDEO_LEFT, 136, 8, 8, 10);
            mpeg_yuv_rect(planes, width, height, FEED_VIDEO_RIGHT, 38,
                          width - FEED_VIDEO_RIGHT, 136, 8, 8, 10);
            for (i = 0; i < 4; i++)
            {
                int x = i < 2 ? FEED_SIDE_GUTTER : FEED_VIDEO_RIGHT + 7;
                int y = 51 + (i & 1) * 48;
                const char *label = actions[i];

                if (i == 0 && item->saved)
                    label = "Unsave";
                if (i == feed.action_cursor)
                    mpeg_yuv_rect(planes, width, height, x - 2, y - 5,
                                  i < 2 ? FEED_VIDEO_LEFT - 8 :
                                  width - FEED_VIDEO_RIGHT - 10,
                                  30, 37, 94, 92);
                feed_format_label(text, sizeof(text), label,
                                  i < 2 ? FEED_VIDEO_LEFT - 12 :
                                  width - FEED_VIDEO_RIGHT - 14);
                mpeg_yuv_text(planes, width, height, x, y, text,
                              255, 255, 255);
            }
        }
        return;
    }

    if ((mpegplayer_livetv_launch || mpegplayer_netflix_launch) &&
        livetv_volume_until != 0)
    {
        struct font *font = rb->font_get(FONT_SYSFIXED);
        int min_volume = rb->sound_min(SOUND_VOLUME);
        int max_volume = rb->sound_max(SOUND_VOLUME);
        int volume = rb->global_status != NULL ?
                     rb->global_status->volume : min_volume;
        int percent;
        int lit;
        int label_w = mpeg_yuv_text_width("VOLUME") * 2;
        int gap = 12;
        int bar_w = MIN(LIVETV_VOLUME_BAR_W,
                        width - label_w - gap - 16);
        int group_w = label_w + gap + bar_w;
        int group_x = (width - group_w) / 2;
        int bar_x = group_x + label_w + gap;
        int segment_w =
            MAX(3, bar_w / LIVETV_VOLUME_SEGMENTS - 2);
        int bar_y = (height - 14) / 2;
        int font_height = font != NULL ? font->height * 2 : 16;
        int i;

        if (rb->global_settings != NULL &&
            rb->global_settings->volume_limit >= min_volume &&
            rb->global_settings->volume_limit < max_volume)
            max_volume = rb->global_settings->volume_limit;

        if (volume <= min_volume)
            percent = 0;
        else if (volume >= max_volume)
            percent = 100;
        else
            percent = ((volume - min_volume) * 100) /
                      (max_volume - min_volume);
        /*
         * Reserve the final segment for the exact configured maximum.
         * A simple ceil(percent * segments) makes the meter look full while
         * the wheel still has several valid volume steps remaining.
         */
        lit = percent >= 100 ? LIVETV_VOLUME_SEGMENTS :
              (percent * (LIVETV_VOLUME_SEGMENTS - 1) + 99) / 100;

        mpeg_yuv_text_scaled(planes, width, height, group_x,
                             (height - font_height) / 2,
                             "VOLUME", 2, 32, 255, 80);
        for (i = 0; i < LIVETV_VOLUME_SEGMENTS; i++)
        {
            int x = bar_x + (i * bar_w) / LIVETV_VOLUME_SEGMENTS;

            if (i < lit)
                mpeg_yuv_rect(planes, width, height, x, bar_y,
                              segment_w, 14, 32, 255, 80);
            else
                mpeg_yuv_rect(planes, width, height, x, bar_y,
                              segment_w, 14, 12, 72, 28);
        }
        return;
    }

    if (mpegplayer_livetv_launch &&
        mpegplayer_livetv_weather_commercial)
    {
        char primary[64];
        char secondary[64];
        char tertiary[64];
        int copy_x = 68;

        livetv_weather_commercial_text(
            primary, sizeof(primary), secondary, sizeof(secondary),
            tertiary, sizeof(tertiary));
        mpeg_yuv_rect(planes, width, height, 0, 0, width, height,
                      3, 22, 55);
        mpeg_yuv_rect(planes, width, height, 0, 0, width, 3,
                      47, 190, 244);
        mpeg_yuv_rect(planes, width, height, 0, 3, 62, height - 3,
                      7, 66, 122);
        mpeg_yuv_rect(planes, width, height, 62, 3, 2, height - 3,
                      47, 147, 210);
        mpeg_yuv_text(planes, width, height, 6, 10, "WX 102",
                      255, 255, 255);
        mpeg_yuv_text(planes, width, height, 6, 27, "LOCAL",
                      105, 214, 255);
        mpeg_yuv_text(planes, width, height, 6, 44, "LIVE",
                      219, 235, 245);
        mpeg_yuv_text(planes, width, height, copy_x, 7, primary,
                      255, 255, 255);
        mpeg_yuv_text(planes, width, height, copy_x, 25, secondary,
                      166, 218, 247);
        mpeg_yuv_text(planes, width, height, copy_x, 43, tertiary,
                      219, 235, 245);
        return;
    }

#if MPEG_STOCK_CONTROLS
    if (osd.youtube_layout && (osd.flags & OSD_SHOW))
    {
        char current[24];
        uint32_t current_time = osd.curr_time;
        uint32_t total_time = netflix_overlay_duration;
        int min_volume = rb->sound_min(SOUND_VOLUME);
        int max_volume = rb->sound_max(SOUND_VOLUME);
        int volume = rb->global_status != NULL ?
                     rb->global_status->volume : min_volume;
        int progress_x;
        int volume_percent;
        int volume_x;
        int time_width;
        int row;
        int col;

        if (!youtube_player_valid)
            return;

        /* This is the archived 2006-2008 embedded-player strip itself,
         * proportionally rasterized to the iPod width. It is not a drawn
         * approximation; each source pixel is composited over decoded YUV. */
        for (row = 0; row < youtube_player_bmp.height; row++)
            for (col = 0; col < youtube_player_bmp.width; col++)
            {
                fb_data pixel =
                    ((const fb_data *)youtube_player_bmp.data)
                    [row * youtube_player_bmp.width + col];

                mpeg_yuv_pixel(planes, width, height, col, row,
                               FB_UNPACK_RED(pixel),
                               FB_UNPACK_GREEN(pixel),
                               FB_UNPACK_BLUE(pixel));
            }

        if (total_time == INVALID_TIMESTAMP || total_time == 0)
            total_time = 1;
        if (current_time > total_time)
            current_time = total_time;

        /* Move the real raster playhead from the archived strip to the live
         * decoder timestamp. The sampled source column restores the matching
         * chrome gradient beneath its old position. */
        mpeg_yuv_youtube_asset_column(
            planes, width, height, 56,
            MPEG_YOUTUBE_PROGRESS_ERASE_X,
            MPEG_YOUTUBE_PROGRESS_ERASE_Y,
            MPEG_YOUTUBE_PROGRESS_ERASE_W,
            MPEG_YOUTUBE_PROGRESS_ERASE_H);
        progress_x = MPEG_YOUTUBE_PROGRESS_LEFT -
                     MPEG_YOUTUBE_PROGRESS_KNOB_W / 2 +
                     (int)(((uint64_t)current_time *
                            MPEG_YOUTUBE_PROGRESS_W) / total_time);
        if (youtube_seek_knob_valid)
            mpeg_yuv_youtube_sprite(
                planes, width, height, &youtube_seek_knob_bmp,
                progress_x, MPEG_YOUTUBE_PROGRESS_KNOB_Y);

        /* Restore the timer's captured vertical gradient from an untouched
         * source column, then replace only its digits. */
        mpeg_stock_format_time(current_time, current, sizeof(current));
        time_width = mpeg_yuv_text_width(current);
        mpeg_yuv_youtube_asset_column(
            planes, width, height, 215, 184, 7, 32, 15);
        mpeg_yuv_text(planes, width, height,
                      184 + (32 - time_width) / 2, 10, current,
                      255, 255, 255);

        if (rb->global_settings != NULL &&
            rb->global_settings->volume_limit >= min_volume &&
            rb->global_settings->volume_limit < max_volume)
            max_volume = rb->global_settings->volume_limit;
        if (volume <= min_volume)
            volume_percent = 0;
        else if (volume >= max_volume)
            volume_percent = 100;
        else
            volume_percent = ((volume - min_volume) * 100) /
                             (max_volume - min_volume);

        /* Reposition the captured volume knob to the live Rockbox volume. */
        mpeg_yuv_youtube_asset_column(
            planes, width, height, 258,
            MPEG_YOUTUBE_VOLUME_ERASE_X,
            MPEG_YOUTUBE_VOLUME_KNOB_Y,
            MPEG_YOUTUBE_VOLUME_ERASE_W,
            MPEG_YOUTUBE_VOLUME_KNOB_H);
        volume_x = MPEG_YOUTUBE_VOLUME_LEFT -
                   MPEG_YOUTUBE_VOLUME_KNOB_W / 2 +
                   volume_percent * MPEG_YOUTUBE_VOLUME_W / 100;
        if (youtube_volume_knob_valid)
            mpeg_yuv_youtube_sprite(
                planes, width, height, &youtube_volume_knob_bmp,
                volume_x, MPEG_YOUTUBE_VOLUME_KNOB_Y);
        return;
    }

    if (mpegplayer_netflix_launch &&
        netflix_skip_active != NETFLIX_SKIP_NONE &&
        !(osd.flags & OSD_SHOW))
    {
        const char *label = netflix_skip_active == NETFLIX_SKIP_INTRO ?
                            "SKIP INTRO" : "SKIP CREDITS";
        int text_w = mpeg_yuv_text_width(label);
        int button_w = text_w + 24;
        int button_h = 28;
        int button_x = width - button_w - 10;
        int button_y = (height - button_h) / 2;

        /* Netflix's on-video action is a dark translucent-looking rectangle
         * with a fine white border and compact white type. The YUV compositor
         * owns only this bottom band, so the decoded picture remains visible
         * everywhere outside the button. */
        mpeg_yuv_rect(planes, width, height, button_x, button_y,
                      button_w, button_h, 20, 20, 20);
        mpeg_yuv_rect(planes, width, height, button_x, button_y,
                      button_w, 2, 255, 255, 255);
        mpeg_yuv_rect(planes, width, height, button_x,
                      button_y + button_h - 2, button_w, 2,
                      255, 255, 255);
        mpeg_yuv_rect(planes, width, height, button_x, button_y,
                      2, button_h, 255, 255, 255);
        mpeg_yuv_rect(planes, width, height,
                      button_x + button_w - 2, button_y,
                      2, button_h, 255, 255, 255);
        mpeg_yuv_text(planes, width, height,
                      button_x + (button_w - text_w) / 2,
                      button_y + 9, label, 255, 255, 255);
        return;
    }

    if (osd.netflix_layout && (osd.flags & OSD_SHOW))
    {
        char current[24];
        char duration_text[24];
        char title[MPEG_STOCK_TITLE_SIZE];
        const char *status;
        int duration_w;
        int status_w;
        int title_w;
        int bar_w = width - 16;
        int fill_w;
        uint32_t current_time = osd.curr_time;
        uint32_t total_time = netflix_overlay_duration;

        if (total_time == INVALID_TIMESTAMP || total_time == 0)
            total_time = 1;
        if (current_time > total_time)
            current_time = total_time;

        mpeg_stock_format_time(current_time, current, sizeof(current));
        mpeg_stock_format_time(total_time, duration_text,
                               sizeof(duration_text));
        mpeg_yuv_overlay_title(title, sizeof(title), width - 98);
        status = osd.status == OSD_STATUS_PAUSED ? "PAUSED" :
                 osd.status == OSD_STATUS_FF ? "FORWARD" :
                 osd.status == OSD_STATUS_RW ? "REWIND" : "PLAYING";

        mpeg_yuv_rect(planes, width, height, 0, 0, width, height,
                      20, 20, 20);
        mpeg_yuv_rect(planes, width, height, 0, 0, width, 3,
                      180, 19, 29);
        mpeg_yuv_text(planes, width, height, 7, 7, "NETFLIX",
                      180, 19, 29);
        title_w = mpeg_yuv_text_width(title);
        mpeg_yuv_text(planes, width, height, width - 7 - title_w, 7,
                      title, 255, 255, 255);

        status_w = mpeg_yuv_text_width(status);
        duration_w = mpeg_yuv_text_width(duration_text);
        mpeg_yuv_text(planes, width, height, 8, 27, current,
                      255, 255, 255);
        mpeg_yuv_text(planes, width, height,
                      (width - status_w) / 2, 27, status,
                      180, 19, 29);
        mpeg_yuv_text(planes, width, height,
                      width - 8 - duration_w, 27, duration_text,
                      255, 255, 255);

        mpeg_yuv_rect(planes, width, height, 8, height - 12,
                      bar_w, 5, 72, 72, 72);
        fill_w = (int)(((uint64_t)current_time * bar_w) / total_time);
        if (fill_w > 0)
            mpeg_yuv_rect(planes, width, height, 8, height - 12,
                          fill_w, 5, 180, 19, 29);
        return;
    }
#endif

}

static void livetv_volume_show(void)
{
    /* If receiver information is up, clear that clipped banner once before
     * handing the full picture back to the decoder. */
    if (livetv_overlay_until != 0)
    {
        livetv_overlay_until = 0;
        stream_vo_set_clip(NULL);
    }

    livetv_volume_until = *rb->current_tick + LIVETV_VOLUME_TIME;
    /* The video output copies the real decoded band, composites the green
     * phosphor pixels in YUV, and presents it in one blit. */
    stream_draw_frame(false);
}

static void livetv_volume_hide(void)
{
    if (livetv_volume_until == 0)
        return;

    livetv_volume_until = 0;
    stream_draw_frame(false);
    if (mpegplayer_livetv_weather_hidden)
        livetv_weather_draw();
    else if (mpegplayer_livetv_weather_commercial)
    {
        struct vo_rect rc = {
            0, 0, LCD_WIDTH,
            LCD_HEIGHT - LIVETV_WEATHER_COMMERCIAL_OVERLAY_H
        };
        stream_vo_set_clip(&rc);
        stream_draw_frame(false);
        livetv_weather_draw_commercial_overlay();
    }
}

static void livetv_overlay_show(bool mini)
{
    struct vo_rect rc;

    if (livetv_volume_until != 0)
        livetv_volume_until = 0;
    rc.l = 0;
    rc.t = 0;
    rc.r = LCD_WIDTH;
    rc.b = LCD_HEIGHT - (mini ? LIVETV_MINI_H : LIVETV_INFO_H);
    stream_vo_set_clip(&rc);

    if (mini)
        livetv_draw_mini_guide(livetv_current_channel());
    else
        livetv_draw_info_banner(livetv_current_channel());

    livetv_overlay_until = *rb->current_tick + HZ * 5;
}

static void livetv_overlay_hide(void)
{
    if (livetv_overlay_until == 0)
        return;

    livetv_overlay_until = 0;
    stream_vo_set_clip(NULL);
    stream_draw_frame(false);
    /* The info banner/mini guide strip overlaps the bottom of the Weather
     * channel's panels and ticker; stream_draw_frame() only repaints the
     * (hidden, on this channel) video, so the chrome underneath the strip
     * needs its own repaint once the strip is gone. */
    if (mpegplayer_livetv_weather_hidden)
        livetv_weather_draw();
    else if (mpegplayer_livetv_weather_commercial)
    {
        struct vo_rect rc = {
            0, 0, LCD_WIDTH,
            LCD_HEIGHT - LIVETV_WEATHER_COMMERCIAL_OVERLAY_H
        };
        stream_vo_set_clip(&rc);
        stream_draw_frame(false);
        livetv_weather_draw_commercial_overlay();
    }
}

/* Select between native forecast panels and the carrier's presenter/video
 * inserts without stopping, seeking, reopening, or taking ownership of
 * audio. Ad slots are intentionally never treated as forecast programming. */
static void livetv_update_weather_view(uint32_t stream_seconds, bool entering)
{
    static int logged_phase = -1;
    bool active = mpegplayer_livetv_launch &&
                  !mpegplayer_livetv_desktop &&
                  livetv_weather_program_active();
    bool commercial = mpegplayer_livetv_launch &&
                      !mpegplayer_livetv_desktop &&
                      livetv_weather_commercial_active();
    bool commercial_changed =
        commercial != mpegplayer_livetv_weather_commercial;
    bool hidden = active && !livetv_weather_wants_video(stream_seconds);
    bool changed = hidden != mpegplayer_livetv_weather_hidden;
    int phase = (stream_seconds % 64) / 8;

    if (entering || changed || phase != logged_phase)
    {
        MPLOG("weather view active=%d hidden=%d seconds=%lu phase=%d\n",
              active, hidden, (unsigned long)stream_seconds, phase);
        logged_phase = phase;
    }

    if (entering && (active || commercial))
        livetv_weather_enter(stream_seconds);

    mpegplayer_livetv_weather_active = active;
    mpegplayer_livetv_weather_commercial = commercial;
    if (changed)
        mpegplayer_livetv_weather_hidden = hidden;

    if (!entering && changed)
    {
        if (hidden)
            livetv_weather_draw();
        else if (active && changed)
            stream_draw_frame(false);
    }

    if (commercial && (entering || commercial_changed) &&
        livetv_overlay_until == 0 && livetv_volume_until == 0)
    {
        struct vo_rect rc = {
            0, 0, LCD_WIDTH,
            LCD_HEIGHT - LIVETV_WEATHER_COMMERCIAL_OVERLAY_H
        };
        stream_vo_set_clip(&rc);
        stream_draw_frame(false);
        livetv_weather_draw_commercial_overlay();
    }
    else if (entering && !active)
    {
        stream_vo_set_clip(NULL);
    }
}

/* Run the guide without disturbing playback. Returns a VIDEO_* action for
 * the caller to hand back, or -1 to carry on watching full screen. */
static int livetv_guide_session(void)
{
    struct viewport guide_vp;
    struct viewport *previous_vp = NULL;
    int result;

    livetv_overlay_hide();
    livetv_volume_hide();
    mpegplayer_livetv_pig = true;
    mpegplayer_livetv_guide_active = true;
    if (mpegplayer_livetv_desktop)
    {
        livetv_desktop_restore_underlay();
        livetv_desktop_draw_window();
    }
    /* Re-runs vo_setup(), which puts the video in the guide window. */
    stream_vo_set_display_mode(settings.display_mode);
    stream_vo_set_clip(NULL);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    if (mpegplayer_livetv_desktop)
    {
        /* Install the viewport after vo_setup: decoder setup can reassert the
         * default LCD viewport. The guide paints every content pixel itself,
         * so clearing here would only destroy the Desktop outside the window. */
        rb->memset(&guide_vp, 0, sizeof(guide_vp));
        guide_vp.x = LIVETV_DM_WIN_X;
        guide_vp.y = LIVETV_DM_WIN_Y + LIVETV_DM_TITLE_H;
        guide_vp.width = LIVETV_DM_WIN_W;
        guide_vp.height = LIVETV_DM_BODY_H;
        guide_vp.font = FONT_UI;
        guide_vp.drawmode = DRMODE_SOLID;
        guide_vp.fg_pattern = LCD_WHITE;
        guide_vp.bg_pattern = LCD_BLACK;
        previous_vp = rb->lcd_set_viewport(&guide_vp);
    }
    else
    {
        rb->lcd_clear_display();
        rb->lcd_update();
    }
    stream_draw_frame(false);

    result = livetv_guide_run();

    if (mpegplayer_livetv_desktop)
        rb->lcd_set_viewport(previous_vp);
    mpegplayer_livetv_guide_active = false;
    mpegplayer_livetv_pig = mpegplayer_livetv_desktop;
    /* mpegplayer_livetv_weather_hidden, untouched by the guide, still says
     * whether the channel being returned to is the Weather channel; this
     * re-run of vo_setup() hides its video again rather than going full
     * screen, exactly as it already restores mpegplayer_livetv_pig's
     * guide-window sizing for every other channel above. */
    stream_vo_set_display_mode(settings.display_mode);
    rb->lcd_set_background(LCD_BLACK);
    if (mpegplayer_livetv_desktop)
    {
        livetv_desktop_restore_underlay();
        livetv_desktop_draw_window();
    }
    else
    {
        rb->lcd_clear_display();
        rb->lcd_update();
    }

    switch (result)
    {
    case LIVETV_GUIDE_TUNE:
        /* Choosing a programme tunes it and leaves the guide, the way
         * SELECT behaves on the receiver. */
        livetv_show_guide = false;
        livetv_banner_pending = true;
        return VIDEO_NEXT;

    case LIVETV_GUIDE_EXIT:
        return VIDEO_STOP;

    default:
        livetv_show_guide = false;
        stream_draw_frame(false);
        if (mpegplayer_livetv_weather_hidden)
            livetv_weather_draw();
        else if (mpegplayer_livetv_weather_commercial)
        {
            struct vo_rect rc = {
                0, 0, LCD_WIDTH,
                LCD_HEIGHT - LIVETV_WEATHER_COMMERCIAL_OVERLAY_H
            };
            stream_vo_set_clip(&rc);
            stream_draw_frame(false);
            livetv_weather_draw_commercial_overlay();
        }
        return -1;
    }
}

/* Channel up and down from the click wheel while watching full screen. */
static int livetv_change_channel(int delta)
{
    /* livetv_step_channel() wraps at both ends and skips channels with
     * nothing on the air, so holding channel-up walks the lineup round
     * and round instead of stopping - or landing on a channel that
     * cannot produce a file to play. */
    if (!livetv_step_channel(delta))
        return -1;

    livetv_save_state();
    livetv_banner_pending = true;
    return VIDEO_NEXT;
}
#endif /* HAVE_LCD_COLOR */

/* Netflix "watched" record.
 *
 * stream_on_ev_complete() zeroes stream_mgr.resume_time on end of stream
 * ("Played to end - no resume"), so a finished title is indistinguishable
 * from a never-played one in mpegplayer.cfg. Completion therefore needs its
 * own record. This only opens, appends to and closes a small text file on the
 * exit path - no PCM, mixer, playlist or shared-buffer API is involved, so
 * docs/plugin-audio-lifecycle-steering.md is unaffected. */
#define MPEG_NETFLIX_WATCHED_FILE ROCKBOX_DIR "/videolist/netflix-watched.tsv"
#define MPEG_NETFLIX_WATCHED_MAX 512
#define MPEG_NETFLIX_INDEX ROCKBOX_DIR "/videolist/index.tsv"
#define MPEG_NETFLIX_MARKER_FIELDS 27

static bool mpeg_netflix_split_marker_row(char *line,
                                          char *fields[MPEG_NETFLIX_MARKER_FIELDS])
{
    int i;

    for (i = 0; i < MPEG_NETFLIX_MARKER_FIELDS; i++)
    {
        char *tab;

        fields[i] = line;
        tab = rb->strchr(line, '\t');
        if (tab == NULL)
            return i == MPEG_NETFLIX_MARKER_FIELDS - 1;
        *tab = '\0';
        line = tab + 1;
    }
    return true;
}

static void mpeg_netflix_load_markers(const char *path)
{
    char line[1024];
    const char *device_path = path != NULL && path[0] == '/' ? path + 1 : path;
    int fd;

    netflix_skip_active = NETFLIX_SKIP_NONE;
    netflix_intro_start = 0;
    netflix_intro_end = 0;
    netflix_credits_start = 0;
    netflix_credits_duration = 0;
    if (!mpegplayer_netflix_launch || device_path == NULL)
        return;

    fd = rb->open(MPEG_NETFLIX_INDEX, O_RDONLY);
    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[MPEG_NETFLIX_MARKER_FIELDS];

        if (line[0] == '#' || !mpeg_netflix_split_marker_row(line, fields) ||
            rb->strcmp(fields[6], device_path))
            continue;
        netflix_intro_start = (uint32_t)rb->atoi(fields[23]) * TS_SECOND;
        netflix_intro_end = (uint32_t)rb->atoi(fields[24]) * TS_SECOND;
        netflix_credits_start = (uint32_t)rb->atoi(fields[25]) * TS_SECOND;
        netflix_credits_duration =
            (uint32_t)rb->atoi(fields[26]) * TS_SECOND;
        if (netflix_intro_end <= netflix_intro_start)
        {
            netflix_intro_start = 0;
            netflix_intro_end = 0;
        }
        break;
    }
    rb->close(fd);
}

static void mpeg_netflix_update_skip(void)
{
    enum netflix_skip_kind active = NETFLIX_SKIP_NONE;
    uint32_t time;
    uint32_t credits_start = netflix_credits_start;

    if (!mpegplayer_netflix_launch || osd_stream_status() == STREAM_STOPPED)
    {
        netflix_skip_active = NETFLIX_SKIP_NONE;
        return;
    }

    time = stream_get_time();
    if (credits_start == 0 && netflix_credits_duration > 0)
    {
        uint32_t duration = stream_get_duration();
        if (duration != INVALID_TIMESTAMP &&
            duration > netflix_credits_duration)
            credits_start = duration - netflix_credits_duration;
    }
    if (netflix_intro_end > netflix_intro_start &&
        time >= netflix_intro_start && time < netflix_intro_end)
        active = NETFLIX_SKIP_INTRO;
    else if (credits_start > 0 && time >= credits_start)
        active = NETFLIX_SKIP_CREDITS;
    netflix_skip_active = active;
}

static bool mpeg_netflix_already_watched(const char *path)
{
    char line[MAX_PATH];
    int fd = rb->open(MPEG_NETFLIX_WATCHED_FILE, O_RDONLY);
    bool found = false;
    int count = 0;

    if (fd < 0)
        return false;

    while (!found && count < MPEG_NETFLIX_WATCHED_MAX &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        count++;
        if (!rb->strcmp(line, path))
            found = true;
    }

    rb->close(fd);
    return found;
}

static void mpeg_netflix_mark_watched(void)
{
    int fd;

    if (!mpegplayer_netflix_launch || feed.active ||
        mpeg_osd_path[0] != '/')
        return;
#ifdef HAVE_LCD_COLOR
    if (mpegplayer_livetv_launch)
        return;
#endif

    if (mpeg_netflix_already_watched(mpeg_osd_path))
        return;

    fd = rb->open(MPEG_NETFLIX_WATCHED_FILE,
                  O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;

    rb->fdprintf(fd, "%s\n", mpeg_osd_path);
    rb->close(fd);
}

static int button_loop(void)
{
    /* TikTok is repeat-one by design: reaching EOS keeps the same card on
     * screen. Only a completed, accepted wheel gesture sets NEXT or PREV. */
    int next_action = (feed.active || mpegplayer_instagram_feed_launch) ?
                      VIDEO_REPEAT :
                      (mpegplayer_livetv_launch ? VIDEO_NEXT :
                      ((settings.play_mode == 0) ? VIDEO_STOP : VIDEO_NEXT));
    bool feed_menu_pending = false;
    bool feed_menu_long_done = false;
    long feed_menu_deadline = 0;
    long youtube_zoom_ready = mpegplayer_youtube_launch ?
                              *rb->current_tick + HZ / 2 : 0;
    long livetv_desktop_tick = 0;

    if (feed.active)
        feed.swipe_committed = false;

    rb->lcd_setfont(FONT_SYSFIXED);
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
#endif
    if (mpegplayer_livetv_desktop && !livetv_show_guide)
    {
        /* stream_close()/osd_stop() may have cleared the LCD while changing
         * channels. Reassert the desktop before painting the new window. */
        livetv_desktop_restore_underlay();
    }
    /* Keep TikTok's last fully composed frame on-screen while the next clip
     * opens. Clearing the RGB framebuffer here appears as a green flash on
     * the physical iPod between wheel-scroll selections. */
    else if (!feed.active)
    {
        rb->lcd_clear_display();
        rb->lcd_update();
    }

    if (mpegplayer_youtube_launch && mpegplayer_youtube_embedded)
    {
        rb->button_clear_queue();
        youtube_draw_embedded_chrome();
    }

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_YUV)
    if (!feed.active || !feed_yuv_mode_active)
    {
        rb->lcd_set_mode(LCD_MODE_YUV);
        if (feed.active)
            feed_yuv_mode_active = true;
    }
#endif

    osd_init();

#ifdef HAVE_LCD_COLOR
    if (mpegplayer_livetv_launch)
    {
        /* The schedule says how long a programme runs, but the file backing
         * it may be shorter, and a join-in point past the end would stop the
         * stream dead. Wrap instead, so a short clip simply repeats for the
         * rest of its slot and the channel still looks live. */
        uint32_t duration = stream_get_duration();

        if (duration > TS_SECOND && settings.resume_time >= duration)
            settings.resume_time %= duration;
        if (duration > TS_SECOND &&
            settings.resume_time + TS_SECOND > duration)
            settings.resume_time = 0;
    }
#endif

    /* Start playback at the specified starting time */
    if (osd_play(settings.resume_time) < STREAM_OK) {
        if (feed.active)
        {
            feed.swipe_offset = 0;
            feed.transition_enter_pending = false;
            feed.transition_capture_pending = false;
        }
        rb->splash(HZ*2, "Playback failed");
        return VIDEO_STOP;
    }
    MPLOG("playback controls ready\n");

#ifdef HAVE_LCD_COLOR
    if (mpegplayer_livetv_launch && !mpegplayer_livetv_desktop)
    {
        /* Follow the audio-master stream timestamp, not a wall clock started
         * before osd_play(). Opening/seek latency otherwise advances the
         * Weather phase early and can hide a report while its audio plays. */
        livetv_update_weather_view(stream_get_time() / TS_SECOND, false);
        if (mpegplayer_livetv_weather_hidden)
            livetv_weather_draw();
        else if (mpegplayer_livetv_weather_active)
            stream_draw_frame(false);
    }

    if (mpegplayer_livetv_desktop)
    {
        livetv_desktop_draw_window();
        stream_draw_frame(false);
        livetv_desktop_tick = *rb->current_tick + HZ;
    }
#endif

    if (feed.active && feed.ui_visible)
        osd_show(OSD_SHOW);
    if (mpegplayer_instagram_app_launch)
        osd_show(OSD_SHOW);

    if (feed.active && feed.transition_enter_pending)
    {
        if (feed_release_primed_transition())
            feed_animate_swipe();
        feed.transition_enter_pending = false;
        feed.swipe_offset = 0;
        /* Ignore the tail of the same physical wheel gesture after the new
         * card settles; a fresh gesture remains responsive shortly after. */
        feed.skip_cooldown_until = *rb->current_tick + FEED_SKIP_COOLDOWN;
    }

    /* Candidate scoring and profile/section lookup happen once while the
     * current card is playing, never in the accepted wheel gesture path. */
    if (feed.active)
        feed_prepare_neighbors();

#ifdef HAVE_LCD_COLOR
    if (mpegplayer_livetv_launch)
    {
        if (livetv_show_guide)
        {
            int action = livetv_guide_session();

            if (action >= 0)
            {
                osd_stop();
                return action;
            }
        }
        else if (livetv_banner_pending)
        {
            /* Show which channel we just landed on, as a receiver does. */
            livetv_banner_pending = false;
            if (mpegplayer_livetv_desktop)
                livetv_desktop_draw_window();
            else
                livetv_overlay_show(false);
        }

        /* A plain channel up/down (MPEG_RW/MPEG_FF) reopens the stream
         * without ever going through the guide or setting
         * livetv_banner_pending, so on the Weather channel nothing above
         * would otherwise repaint the panel chrome - it would stay on
         * whatever osd_play()/stream_open() left on screen (typically
         * black) until the next 8-second rotation tick. Every path that
         * reaches here for the Weather channel needs the panel redrawn
         * once, not just the guide-return and initial-launch cases above. */
        if (mpegplayer_livetv_weather_hidden)
            livetv_weather_draw();
    }
#endif

    mpeg_stop_requested = false;

    /* Gently poll the video player for EOS and handle UI */
feed_repeat_playback:
    while (stream_status() != STREAM_STOPPED)
    {
        int button = mpeg_button_get(OSD_MIN_UPDATE_INTERVAL/2);
        if (button != BUTTON_NONE)
            MPLOG("button=0x%x stream=%d osd=%d\n",
                  button, stream_status(), osd_get_status());

#ifdef HAVE_LCD_COLOR
        /* Follow the audio-master playback timestamp on every iteration.
         * Using a separate wall clock drifts across open/seek latency and
         * can expose report speech before its decoded picture. */
        if (mpegplayer_livetv_launch &&
            mpegplayer_livetv_weather_active &&
            !mpegplayer_livetv_desktop &&
            livetv_overlay_until == 0 &&
            livetv_volume_until == 0)
        {
            uint32_t seconds = stream_get_time() / TS_SECOND;
            livetv_update_weather_view(seconds, false);
            if (mpegplayer_livetv_weather_hidden)
                livetv_weather_tick(seconds);
        }
#endif

        switch (button)
        {
        case BUTTON_NONE:
        {
            if (feed.active && feed.select_armed &&
                !TIME_BEFORE(*rb->current_tick, feed.select_deadline))
            {
                feed.select_armed = false;
                if (osd_stream_status() == STREAM_PLAYING)
                    osd_pause();
                else if (osd_stream_status() == STREAM_PAUSED)
                    osd_resume();
                stream_draw_frame(false);
            }
#if MPEG_STOCK_CONTROLS
            if (mpeg_volume_card_visible() &&
                !TIME_BEFORE(*rb->current_tick, mpeg_volume_card_until))
                mpeg_volume_card_hide();
#endif
#ifdef HAVE_LCD_COLOR
            if (livetv_volume_until != 0 &&
                !TIME_BEFORE(*rb->current_tick, livetv_volume_until))
                livetv_volume_hide();

            if (mpegplayer_livetv_launch)
            {
                if (mpegplayer_livetv_desktop)
                {
                    if (!TIME_BEFORE(*rb->current_tick,
                                     livetv_desktop_tick))
                    {
                        livetv_desktop_tick = *rb->current_tick + HZ;
                        livetv_desktop_draw_window();
                    }
                    continue;
                }
                if (livetv_overlay_until != 0 &&
                    !TIME_BEFORE(*rb->current_tick, livetv_overlay_until))
                    livetv_overlay_hide();
                continue;
            }
#endif
            mpeg_netflix_update_skip();
            if (feed.active && feed_menu_pending && !feed_menu_long_done &&
                !TIME_BEFORE(*rb->current_tick, feed_menu_deadline))
            {
                feed_menu_long_done = true;
                feed_show_actions();
            }
            if (feed.active && feed_profile_service_assets())
                stream_draw_frame(false);
            osd_refresh(OSD_REFRESH_DEFAULT);
            continue;
            } /* BUTTON_NONE: */

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
        case LCD_ENABLE_EVENT_1:
        {
            /* Draw the current frame if prepared already */
            stream_draw_frame(true);
            break;
            } /* LCD_ENABLE_EVENT_1: */
#endif

#ifdef MPEG_RC_GUIDE
        case MPEG_RC_GUIDE:
        {
#ifdef HAVE_LCD_COLOR
            if (mpegplayer_livetv_launch)
            {
                int action = livetv_guide_session();

                if (action >= 0)
                {
                    next_action = action;
                    osd_stop();
                }
                else if (livetv_banner_pending)
                {
                    livetv_banner_pending = false;
                    livetv_overlay_show(false);
                }
            }
#endif
            /* Ordinary video still handles Play on its release below. */
            break;
        }
#endif

        case MPEG_VOLUP:
        case MPEG_VOLUP|BUTTON_REPEAT:
#ifdef MPEG_VOLUP2
        case MPEG_VOLUP2:
        case MPEG_VOLUP2|BUTTON_REPEAT:
#endif
#ifdef MPEG_RC_VOLUP
        case MPEG_RC_VOLUP:
        case MPEG_RC_VOLUP|BUTTON_REPEAT:
#endif
#ifdef MPEG_RC_UP
        case MPEG_RC_UP:
        case MPEG_RC_UP|BUTTON_REPEAT:
#endif
        {
            if (mpegplayer_instagram_feed_launch &&
                !mpegplayer_instagram_feed_expanded)
            {
                mpegplayer_instagram_return_direction = 1;
                next_action = VIDEO_STOP;
                osd_stop();
                break;
            }
            if (feed.active)
            {
                if (feed.action_visible)
                {
                    feed_action_move(+1);
                    break;
                }
                if (feed.details_visible)
                    break;
                if (feed.saved_profiles_visible)
                {
                    feed_saved_profile_move(+1);
                    break;
                }
                if (feed.profile_visible)
                {
                    feed_profile_move(+1);
                    break;
                }
                if (feed_swipe_allowed(
                        VIDEO_NEXT, (button & BUTTON_REPEAT) != 0))
                {
                    if (feed_prepared_boundary(VIDEO_NEXT))
                    {
                        feed_animate_edge_resistance(VIDEO_NEXT);
                        break;
                    }
                    feed.transition_direction = VIDEO_NEXT;
                    feed.transition_enter_pending = true;
                    feed.transition_capture_pending = true;
                    stream_draw_frame(false);
                    feed.transition_capture_pending = false;
                    osd_stop();
                    next_action = VIDEO_NEXT | VIDEO_ACTION_MANUAL;
                }
                break;
            }

            osd_set_volume(+1);
            break;
            } /* MPEG_VOLUP*: */

        case MPEG_VOLDOWN:
        case MPEG_VOLDOWN|BUTTON_REPEAT:
#ifdef MPEG_VOLDOWN2
        case MPEG_VOLDOWN2:
        case MPEG_VOLDOWN2|BUTTON_REPEAT:
#endif
#ifdef MPEG_RC_VOLDOWN
        case MPEG_RC_VOLDOWN:
        case MPEG_RC_VOLDOWN|BUTTON_REPEAT:
#endif
#ifdef MPEG_RC_DOWN
        case MPEG_RC_DOWN:
        case MPEG_RC_DOWN|BUTTON_REPEAT:
#endif
        {
            if (mpegplayer_instagram_feed_launch &&
                !mpegplayer_instagram_feed_expanded)
            {
                mpegplayer_instagram_return_direction = -1;
                next_action = VIDEO_STOP;
                osd_stop();
                break;
            }
            if (feed.active)
            {
                if (feed.action_visible)
                {
                    feed_action_move(-1);
                    break;
                }
                if (feed.details_visible)
                    break;
                if (feed.saved_profiles_visible)
                {
                    feed_saved_profile_move(-1);
                    break;
                }
                if (feed.profile_visible)
                {
                    feed_profile_move(-1);
                    break;
                }
                if (feed_swipe_allowed(
                        VIDEO_PREV, (button & BUTTON_REPEAT) != 0))
                {
                    if (feed_prepared_boundary(VIDEO_PREV))
                    {
                        feed_animate_edge_resistance(VIDEO_PREV);
                        break;
                    }
                    feed.transition_direction = VIDEO_PREV;
                    feed.transition_enter_pending = true;
                    feed.transition_capture_pending = true;
                    stream_draw_frame(false);
                    feed.transition_capture_pending = false;
                    osd_stop();
                    next_action = VIDEO_PREV | VIDEO_ACTION_MANUAL;
                }
                break;
            }

            osd_set_volume(-1);
            break;
            } /* MPEG_VOLDOWN*: */

        case MPEG_MENU:
#if (MPEG_MENU & BUTTON_REL) == 0
        case MPEG_MENU | BUTTON_REPEAT:
        case MPEG_MENU | BUTTON_REL:
#endif
#ifdef MPEG_RC_MENU
        case MPEG_RC_MENU:
#endif
        {
#ifdef HAVE_LCD_COLOR
            if (mpegplayer_livetv_launch)
            {
                /* MENU returns to the guide with this channel still
                 * playing in the corner, as on a DIRECTV receiver. */
                int action;

                if (button != MPEG_MENU
#ifdef MPEG_RC_MENU
                    && button != MPEG_RC_MENU
#endif
                   )
                    break;

                action = livetv_guide_session();
                if (action >= 0)
                {
                    next_action = action;
                    osd_stop();
                }
                else if (livetv_banner_pending)
                {
                    livetv_banner_pending = false;
                    livetv_overlay_show(false);
                }
                break;
            }
#endif
            if (feed.active)
            {
                if (button == MPEG_MENU)
                {
                    if (feed.action_visible || feed.details_visible)
                    {
                        feed_hide_actions(true);
                        feed_menu_pending = true;
                        feed_menu_long_done = true;
                        break;
                    }
                    if (feed.saved_profiles_visible)
                    {
                        feed_hide_saved_profiles(true);
                        feed_menu_pending = true;
                        feed_menu_long_done = true;
                        break;
                    }
                    if (feed.profile_visible)
                    {
                        feed_hide_profile();
                        feed_menu_pending = true;
                        feed_menu_long_done = true;
                        break;
                    }
                    feed_menu_pending = true;
                    feed_menu_long_done = false;
                    feed_menu_deadline = *rb->current_tick + FEED_MENU_HOLD_TIME;
                }
#if (MPEG_MENU & BUTTON_REL) == 0
                else if (button == (MPEG_MENU | BUTTON_REPEAT))
                {
                    if (!feed_menu_pending)
                    {
                        feed_menu_pending = true;
                        feed_menu_long_done = false;
                        feed_menu_deadline = *rb->current_tick + FEED_MENU_HOLD_TIME;
                    }

                    /* BUTTON_REPEAT is itself the platform's certified hold
                     * threshold. Open immediately on the first repeat so a
                     * sparse clickwheel repeat stream cannot fall through as
                     * a short Menu press when released. */
                    if (!feed_menu_long_done)
                    {
                        feed_menu_long_done = true;
                        feed_show_actions();
                    }
                }
                else if (button == (MPEG_MENU | BUTTON_REL))
                {
                    if (feed_menu_pending && !feed_menu_long_done)
                    {
                        next_action = VIDEO_STOP;
                        osd_stop();
                    }

                    feed_menu_pending = false;
                    feed_menu_long_done = false;
                }
#endif
                break;
            }

            if (mpegplayer_instagram_app_launch)
            {
                next_action = VIDEO_STOP;
                osd_stop();
                break;
            }

            if (mpegplayer_youtube_app_launch)
            {
                next_action = VIDEO_STOP;
                osd_stop();
                break;
            }

#if (CONFIG_KEYPAD == IPOD_4G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_1G2G_PAD)
            next_action = VIDEO_STOP;
            osd_stop();
            break;
#endif

            int state = osd_halt(); /* save previous state */
            int result;

            /* Hide video output */
            osd_show(OSD_HIDE | OSD_NODRAW);
            stream_show_vo(false);
            osd_backlight_brightness_video_mode(false);

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_YUV)
            rb->lcd_set_mode(LCD_MODE_RGB565);
#endif

            result = mpeg_menu();

            next_action = (settings.play_mode == 0) ? VIDEO_STOP : VIDEO_NEXT;

            fps_update_post_frame_callback();

            /* The menu can change the font, so restore */
            rb->lcd_setfont(FONT_SYSFIXED);
#ifdef HAVE_LCD_COLOR
            rb->lcd_set_foreground(LCD_WHITE);
            rb->lcd_set_background(LCD_BLACK);
#endif
            rb->lcd_clear_display();
            rb->lcd_update();

            switch (result)
            {
            case MPEG_MENU_QUIT:
                next_action = VIDEO_STOP;
                osd_stop();
                break;

            default:
#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_YUV)
                rb->lcd_set_mode(LCD_MODE_YUV);
#endif
                /* If not stopped, show video again */
                if (state != STREAM_STOPPED) {
                    osd_show(OSD_SHOW);
                    stream_show_vo(true);
                }

                /* If stream was playing, restart it */
                if (state == STREAM_PLAYING) {
                    osd_resume();
                }
                break;
            }
            break;
            } /* MPEG_MENU: */

#ifdef MPEG_SHOW_OSD
        case MPEG_SHOW_OSD:
        case MPEG_SHOW_OSD | BUTTON_REPEAT:
            /* Show if not visible */
            osd_show(OSD_SHOW);
            /* Make sure it refreshes */
            osd_refresh(OSD_REFRESH_DEFAULT);
            break;
#endif

        case MPEG_STOP:
#ifdef MPEG_RC_STOP
        case MPEG_RC_STOP:
#endif
        case ACTION_STD_CANCEL:
        {
        cancel_playback:
            next_action = VIDEO_STOP;
            osd_stop();
            break;
            } /* MPEG_STOP: */

        case MPEG_PAUSE:
#ifdef MPEG_PAUSE2
        case MPEG_PAUSE2:
#endif
#ifdef MPEG_RC_PAUSE
        case MPEG_RC_PAUSE:
#endif
        {
#ifdef HAVE_LCD_COLOR
            if (mpegplayer_livetv_launch)
            {
                if (mpegplayer_livetv_desktop)
                {
                    livetv_desktop_draw_window();
                    stream_draw_frame(false);
                    break;
                }
                /* Live television does not pause. Physical PLAY brings up
                 * the one-line mini guide, as the BLUE key does. A dock
                 * remote's Play press enters the full guide above because
                 * Philips may expose only Play and Previous/Next. */
                if (livetv_overlay_until != 0)
                    livetv_overlay_hide();
                else
                    livetv_overlay_show(true);
                break;
            }
#endif
            int status = osd_stream_status();

            if (feed.active && (feed.action_visible || feed.details_visible ||
                                feed.profile_visible ||
                                feed.saved_profiles_visible))
                break;

            if (status == STREAM_PLAYING) {
                /* Playing => Paused */
                osd_pause();
            }
            else if (status == STREAM_PAUSED) {
                /* Paused => Playing */
                osd_resume();
            }

            break;
            } /* MPEG_PAUSE*: */

#ifdef MPEG_ZOOM
        case MPEG_ZOOM:
#ifdef MPEG_RC_ZOOM
        case MPEG_RC_ZOOM:
#endif
        {
#ifdef HAVE_LCD_COLOR
            if (mpegplayer_livetv_launch)
            {
                if (mpegplayer_livetv_desktop)
                {
                    livetv_desktop_draw_window();
                    stream_draw_frame(false);
                    break;
                }
                /* SELECT shows the channel information banner. */
                if (livetv_overlay_until != 0)
                    livetv_overlay_hide();
                else
                    livetv_overlay_show(false);
                break;
            }
#endif
            if (mpegplayer_instagram_feed_launch &&
                !mpegplayer_instagram_feed_expanded)
            {
                mpegplayer_instagram_feed_expanded = true;
                osd.instagram_layout = false;
                osd_show(OSD_HIDE | OSD_NODRAW);
                stream_vo_set_clip(NULL);
                settings.display_mode = MPEG_VIDEO_DISPLAY_FIT;
                stream_vo_set_display_mode(settings.display_mode);
                stream_draw_frame(false);
                break;
            }
            if (mpegplayer_instagram_app_launch)
            {
                if (instagram_toggle_like())
                    stream_draw_frame(false);
                break;
            }
            if (mpegplayer_netflix_launch &&
                netflix_skip_active != NETFLIX_SKIP_NONE)
            {
                if (netflix_skip_active == NETFLIX_SKIP_INTRO)
                {
                    netflix_skip_active = NETFLIX_SKIP_NONE;
                    osd_seek_time(netflix_intro_end);
                }
                else
                {
                    netflix_skip_active = NETFLIX_SKIP_NONE;
                    mpeg_netflix_mark_watched();
                    next_action = VIDEO_STOP;
                    osd_stop();
                }
                break;
            }
            if (mpegplayer_youtube_launch && mpegplayer_youtube_embedded)
            {
                if (TIME_BEFORE(*rb->current_tick, youtube_zoom_ready))
                    break;
                mpegplayer_youtube_embedded = false;
#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_YUV)
                rb->lcd_set_mode(LCD_MODE_RGB565);
#endif
                rb->lcd_set_background(LCD_BLACK);
                rb->lcd_clear_display();
                rb->lcd_update();
#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_YUV)
                rb->lcd_set_mode(LCD_MODE_YUV);
#endif
                settings.display_mode = MPEG_VIDEO_DISPLAY_FIT;
                stream_vo_set_display_mode(settings.display_mode);
                stream_vo_set_clip(NULL);
                stream_draw_frame(false);
                break;
            }
            if (feed.active)
            {
                if (feed.saved_profiles_visible)
                {
                    feed_open_saved_profile();
                    break;
                }
                if (feed.details_visible)
                {
                    feed_hide_actions(true);
                    break;
                }
                if (feed.action_visible)
                {
                    struct feed_item *item = &feed.items[feed.index];

                    switch (feed.action_cursor)
                    {
                    case 0:
                        item->saved = !item->saved;
                        feed.activity_dirty = true;
                        rb->strlcpy(feed.confirm_text,
                                    item->saved ? "SAVED" : "REMOVED",
                                    sizeof(feed.confirm_text));
                        feed.confirm_until =
                            *rb->current_tick + FEED_CONFIRM_TIME;
                        feed_hide_actions(true);
                        break;
                    case 1:
                        item->not_interested = true;
                        feed.activity_dirty = true;
                        feed_hide_actions(false);
                        feed.transition_direction = VIDEO_NEXT;
                        feed.transition_enter_pending = true;
                        feed.transition_capture_pending = true;
                        stream_draw_frame(false);
                        feed.transition_capture_pending = false;
                        osd_stop();
                        next_action = VIDEO_NEXT | VIDEO_ACTION_MANUAL;
                        break;
                    case 2:
                        feed_hide_actions(true);
                        feed_show_current_profile(false);
                        break;
                    case 3:
                        feed.action_visible = false;
                        feed.details_visible = true;
                        stream_draw_frame(false);
                        break;
                    }
                    break;
                }
                if (feed.profile_visible)
                {
                    int target = feed_profile_item_index(feed.profile_cursor);

                    if (target >= 0)
                    {
                        bool changed = target != feed.index;

                        feed.index = target;
                        feed.section = feed.items[target].following ?
                            FEED_SECTION_FOLLOWING : FEED_SECTION_FOR_YOU;
                        feed.profile_feed_active = true;
                        feed.profile_visible = false;
                        feed.select_armed = false;
                        feed.state_dirty = true;
                        if (changed)
                        {
                            feed.profile_selection_pending = true;
                            feed.profile_resume_playback = false;
                            osd_stop();
                            next_action = VIDEO_NEXT | VIDEO_ACTION_MANUAL;
                        }
                        else
                        {
                            /* The representative saved clip is already the
                             * paused playback item. Re-entering the Saved
                             * picker here made its first video impossible to
                             * open; close both overlays and resume it. */
                            feed.profile_visible = false;
                            feed.profile_saved_only = false;
                            feed.saved_profiles_visible = false;
                            feed.profile_resume_playback = false;
                            feed.saved_profiles_resume_playback = false;
                            if (osd_stream_status() == STREAM_PAUSED)
                                osd_resume();
                            stream_draw_frame(false);
                        }
                    }
                    else
                        feed_hide_profile();
                    break;
                }
                if (feed.select_armed &&
                    TIME_BEFORE(*rb->current_tick, feed.select_deadline))
                {
                    feed.select_armed = false;
                    feed_like_current();
                }
                else
                {
                    feed.select_armed = true;
                    feed.select_deadline = *rb->current_tick + HZ / 3;
                }
                break;
            }

            if (settings.display_mode == MPEG_VIDEO_DISPLAY_FILL)
                settings.display_mode = MPEG_VIDEO_DISPLAY_FIT;
            else
                settings.display_mode = MPEG_VIDEO_DISPLAY_FILL;

            stream_vo_set_display_mode(settings.display_mode);
            break;
            } /* MPEG_ZOOM: */
#endif

        case MPEG_RW:
#ifdef MPEG_RW2
        case MPEG_RW2:
#endif
#ifdef MPEG_RC_RW
        case MPEG_RC_RW:
#endif
        {
            int old_button = button;

#ifdef HAVE_LCD_COLOR
            if (mpegplayer_livetv_launch)
            {
                /* Live television cannot be rewound, so the left key steps
                 * down a channel instead. */
                int action = livetv_change_channel(-1);

                if (action >= 0)
                {
                    osd_stop();
                    next_action = action;
                }
                break;
            }
#endif

            if (feed.active)
            {
                enum feed_section old_section = feed.section;
                int old_index = feed.index;

                if (feed.saved_profiles_visible)
                {
                    if (feed_leave_saved_profiles_for_section(-1))
                    {
                        osd_stop();
                        next_action = VIDEO_PREV | VIDEO_ACTION_MANUAL;
                    }
                    break;
                }
                if (feed.profile_visible || feed.action_visible ||
                    feed.details_visible)
                    break;
                if (feed_cycle_section(-1))
                {
                    if (feed.section == FEED_SECTION_SAVED)
                    {
                        feed.saved_return_section = old_section;
                        feed.saved_return_index = old_index;
                        feed_show_saved_profiles();
                    }
                    else
                    {
                        osd_stop();
                        next_action = VIDEO_PREV | VIDEO_ACTION_MANUAL;
                    }
                }
                break;
            }

            /* If button has been released: skip to next/previous file */
            button = mpeg_button_get(OSD_MIN_UPDATE_INTERVAL);

            if ((old_button | BUTTON_REL) == button) {
                /* Check current playback position */
                osd_update_time();

                if (settings.play_mode == 0 || osd.curr_time >= 3*TS_SECOND) {
                    /* Start the current video from the beginning */
                    osd_seek_time(0*TS_SECOND);
                }
                else {
                    /* Release within 3 seconds of start: skip to previous
                     * file */
                    osd_stop();
                    next_action = VIDEO_PREV | VIDEO_ACTION_MANUAL;
                }
            }
            else if ((button & ~BUTTON_REPEAT) == old_button) {
                button = osd_seek_btn(old_button);
            }

            if (button == ACTION_STD_CANCEL)
                goto cancel_playback; /* jump to stop handling above */

            rb->default_event_handler(button);
            break;
            } /* MPEG_RW: */

        case MPEG_FF:
#ifdef MPEG_FF2
        case MPEG_FF2:
#endif
#ifdef MPEG_RC_FF
        case MPEG_RC_FF:
#endif
        {
            int old_button = button;

#ifdef HAVE_LCD_COLOR
            if (mpegplayer_livetv_launch)
            {
                /* Live television cannot be fast forwarded, so the right
                 * key steps up a channel instead. */
                int action = livetv_change_channel(1);

                if (action >= 0)
                {
                    osd_stop();
                    next_action = action;
                }
                break;
            }
#endif

            if (mpegplayer_instagram_feed_launch)
            {
                mpegplayer_instagram_return_profile = true;
                next_action = VIDEO_STOP;
                osd_stop();
                break;
            }

            if (feed.active)
            {
                if (feed.saved_profiles_visible)
                {
                    feed_open_saved_profile();
                    break;
                }
                if (!feed.profile_visible && !feed.action_visible &&
                    !feed.details_visible)
                    feed_show_current_profile(false);
                break;
            }

            if (settings.play_mode != 0)
                button = mpeg_button_get(OSD_MIN_UPDATE_INTERVAL);

            if ((old_button | BUTTON_REL) == button) {
                /* If button has been released: skip to next file */
                osd_stop();
                next_action = VIDEO_NEXT | VIDEO_ACTION_MANUAL;
            }
            else if ((button & ~BUTTON_REPEAT) == old_button) {
                button = osd_seek_btn(old_button);
            }

            if (button == ACTION_STD_CANCEL)
                goto cancel_playback; /* jump to stop handling above */

            rb->default_event_handler(button);
            break;
            } /* MPEG_FF: */

#ifdef HAVE_HEADPHONE_DETECTION
        case SYS_PHONE_PLUGGED:
        case SYS_PHONE_UNPLUGGED:
        {
            osd_handle_phone_plug(button == SYS_PHONE_PLUGGED);
            break;
            } /* SYS_PHONE_*: */
#endif

        default:
        {
            osd_refresh(OSD_REFRESH_DEFAULT);
            rb->default_event_handler(button);
            break;
            } /* default: */
        }

        rb->yield();
    } /* end while */

    /* Repeat the current TikTok parser in place. Closing and reopening the
     * stream hides the video output between clips; on iPod hardware that
     * exposes the YUV key colour as a green flash. The stopped stream already
     * reset resume_time to zero, so stream_play() performs a clean rewind
     * while the last decoded frame remains visible. */
    if ((feed.active || mpegplayer_instagram_feed_launch) &&
        next_action == VIDEO_REPEAT &&
        !mpeg_stop_requested && stream_play() != STREAM_ERROR &&
        stream_status() != STREAM_STOPPED)
    {
        goto feed_repeat_playback;
    }

    /* Reaching here without a requested stop means the stream ended on its
     * own. Every in-loop user stop goes through osd_stop() first. */
    if (!mpeg_stop_requested)
        mpeg_netflix_mark_watched();

    osd_stop();

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
    /* Be sure hook is removed before exiting since the stop will put it
     * back because of the backlight restore. */
    rb->remove_event(LCD_EVENT_ACTIVATION, osd_lcd_enable_hook);
#endif

    rb->lcd_setfont(FONT_UI);

    return next_action;
}

enum plugin_status plugin_start(const void* parameter)
{
    static char videofile[MAX_PATH];
    int status = PLUGIN_OK; /* assume success */
    bool quit = false;
    bool netflix_restart;
    bool ipodtiktok_launch = rb->file_exists(IPODTIKTOK_LAUNCH_MARKER);

    if (ipodtiktok_launch)
        rb->remove(IPODTIKTOK_LAUNCH_MARKER);

    MPLOG("plugin_start enter\n");
    MPLOG("plugin_start parameter=%s\n",
          parameter ? (const char *)parameter : "(null)");

    if (parameter == NULL) {
        /* No file = GTFO */
        MPLOG("plugin_start no file parameter\n");
        rb->splash(HZ*2, "No File");
        return PLUGIN_ERROR;
    }

    mpegplayer_livetv_desktop =
        !rb->strncmp((const char *)parameter, LIVETV_DM_PARAM_PREFIX,
                     LIVETV_DM_PARAM_PREFIX_LEN);
    mpegplayer_instagram_feed_launch =
        !rb->strncmp((const char *)parameter,
                     MPEGPLAYER_INSTAGRAM_FEED_PREFIX,
                     MPEGPLAYER_INSTAGRAM_FEED_PREFIX_LEN);
#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320) && (LCD_HEIGHT >= 240)
    livetv_desktop_underlay_valid = false;
#endif
    if (mpegplayer_livetv_desktop)
        livetv_desktop_capture_underlay();
    feed_reset();
    feed_yuv_mode_active = false;

#ifdef HAVE_LCD_COLOR
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
#endif

    /* Instagram already painted an authentic launch/home frame. Keep that
     * frame over parser startup and buffering until the first inline YUV
     * frame is ready, just as the Direct TV desktop preserves its underlay. */
    if (!mpegplayer_livetv_desktop && !mpegplayer_instagram_feed_launch)
    {
        rb->lcd_clear_display();
        rb->lcd_update();
    }

    netflix_restart =
        !rb->strncmp((const char *)parameter,
                     MPEGPLAYER_NETFLIX_RESTART_PREFIX,
                     MPEGPLAYER_NETFLIX_RESTART_PREFIX_LEN);
    mpegplayer_netflix_launch = netflix_restart ||
        !rb->strncmp((const char *)parameter,
                     MPEGPLAYER_NETFLIX_PREFIX,
                     MPEGPLAYER_NETFLIX_PREFIX_LEN);
    mpegplayer_youtube_app_launch =
        !rb->strncmp((const char *)parameter,
                     MPEGPLAYER_YOUTUBE_APP_PREFIX,
                     MPEGPLAYER_YOUTUBE_APP_PREFIX_LEN);
    mpegplayer_onlyfans_app_launch =
        !rb->strncmp((const char *)parameter,
                     MPEGPLAYER_ONLYFANS_APP_PREFIX,
                     MPEGPLAYER_ONLYFANS_APP_PREFIX_LEN);
    mpegplayer_instagram_app_launch = mpegplayer_instagram_feed_launch ||
        !rb->strncmp((const char *)parameter,
                     MPEGPLAYER_INSTAGRAM_APP_PREFIX,
                     MPEGPLAYER_INSTAGRAM_APP_PREFIX_LEN);
    mpegplayer_instagram_return_direction = 0;
    mpegplayer_instagram_return_profile = false;
    mpegplayer_instagram_feed_expanded = false;
    mpegplayer_reddit_app_launch =
        !rb->strncmp((const char *)parameter,
                     MPEGPLAYER_REDDIT_APP_PREFIX,
                     MPEGPLAYER_REDDIT_APP_PREFIX_LEN);
    mpegplayer_youtube_launch = mpegplayer_youtube_app_launch ||
        !rb->strncmp((const char *)parameter, "youtube:", 8);
    /* Offline Web retains its embedded watch-page presentation. The
     * standalone app has already shown a video detail page, so Watch Video
     * opens the full-screen player and its YouTube-specific OSD directly. */
    mpegplayer_youtube_embedded = mpegplayer_youtube_launch &&
                                  !mpegplayer_youtube_app_launch;
    mpegplayer_maps_dashcam_launch =
        !rb->strncmp((const char *)parameter, MPEGPLAYER_MAPS_PREFIX,
                     MPEGPLAYER_MAPS_PREFIX_LEN);

    if (ipodtiktok_launch)
    {
        if (!feed_init_from_path(IPODTIKTOK_FEED_PATH, videofile,
                                 sizeof(videofile)))
        {
            rb->splash(HZ * 2, "TikTok feed missing");
            return PLUGIN_ERROR;
        }
    }
    else if (mpegplayer_maps_dashcam_launch)
    {
        rb->strlcpy(videofile,
                    (const char *)parameter + MPEGPLAYER_MAPS_PREFIX_LEN,
                    sizeof(videofile));
    }
    else if (mpegplayer_onlyfans_app_launch)
    {
        rb->strlcpy(videofile, (const char *)parameter +
                    MPEGPLAYER_ONLYFANS_APP_PREFIX_LEN, sizeof(videofile));
    }
    else if (mpegplayer_instagram_feed_launch)
    {
        rb->strlcpy(videofile, (const char *)parameter +
                    MPEGPLAYER_INSTAGRAM_FEED_PREFIX_LEN, sizeof(videofile));
    }
    else if (mpegplayer_instagram_app_launch)
    {
        rb->strlcpy(videofile, (const char *)parameter +
                    MPEGPLAYER_INSTAGRAM_APP_PREFIX_LEN, sizeof(videofile));
    }
    else if (mpegplayer_reddit_app_launch)
    {
        rb->strlcpy(videofile, (const char *)parameter +
                    MPEGPLAYER_REDDIT_APP_PREFIX_LEN, sizeof(videofile));
    }
    else if (mpegplayer_youtube_launch)
    {
        rb->strlcpy(videofile, (const char *)parameter +
                    (mpegplayer_youtube_app_launch ?
                     MPEGPLAYER_YOUTUBE_APP_PREFIX_LEN : 8),
                    sizeof(videofile));
        youtube_load_metadata(videofile);
    }
    else if (netflix_restart)
    {
        rb->strlcpy(videofile,
                    (const char *)parameter +
                        MPEGPLAYER_NETFLIX_RESTART_PREFIX_LEN,
                    sizeof(videofile));
    }
    else if (mpegplayer_netflix_launch)
    {
        rb->strlcpy(videofile,
                    (const char *)parameter +
                        MPEGPLAYER_NETFLIX_PREFIX_LEN,
                    sizeof(videofile));
    }
#ifdef HAVE_LCD_COLOR
    else if (mpegplayer_livetv_desktop ||
             !rb->strncmp((const char *)parameter, LIVETV_PARAM_PREFIX,
                          LIVETV_PARAM_PREFIX_LEN))
    {
        const char *root = (const char *)parameter +
            (mpegplayer_livetv_desktop ? LIVETV_DM_PARAM_PREFIX_LEN :
                                         LIVETV_PARAM_PREFIX_LEN);

        if (!mpegplayer_livetv_desktop)
        {
            /* Ahead of livetv_load(), because parsing a full week of guide
             * lines is the longest black screen of the launch, not the
             * shortest. The ident leaves its last frame up, so the parse,
             * the tune and the first channel's buffering all happen under
             * the boot screen. The scratch memory is the same block
             * stream_init() asks for later - plugin_get_audio_buffer()
             * hands back the one allocation both times - and this sits
             * outside the play loop, so a channel change never replays it. */
            size_t boot_size = 0;
            void *boot_mem = rb->plugin_get_audio_buffer(&boot_size);

            directv_boot_run(boot_mem, boot_size);
        }

        if (!livetv_load(root))
        {
            rb->splash(HZ * 3, "Live TV schedule missing");
            return PLUGIN_ERROR;
        }

        if (!livetv_tune(livetv_current_channel(), videofile,
                         sizeof(videofile), &livetv_resume))
        {
            rb->splash(HZ * 3, "No Live TV programming");
            return PLUGIN_ERROR;
        }

        mpegplayer_livetv_launch = true;
        mpegplayer_livetv_pig = mpegplayer_livetv_desktop;
        mpegplayer_livetv_guide_active = false;
        livetv_show_guide = true;
        if (mpegplayer_livetv_desktop)
            livetv_desktop_prepare();
    }
#endif
    else if (!rb->strncmp((const char *)parameter, IPODTIKTOK_PARAM_PREFIX,
                     sizeof(IPODTIKTOK_PARAM_PREFIX) - 1))
    {
        const char *feed_path = (const char *)parameter +
                                sizeof(IPODTIKTOK_PARAM_PREFIX) - 1;

        if (!feed_init_from_path(feed_path, videofile, sizeof(videofile)))
        {
            rb->splash(HZ * 2, "iPodTikTok feed missing");
            return PLUGIN_ERROR;
        }
    }
    else
    {
        rb->strlcpy(videofile, (const char*) parameter, sizeof(videofile));
    }

    if (mpegplayer_instagram_app_launch)
        instagram_load_metadata(videofile);

    MPLOG("target file=%s\n", videofile);

    MPLOG("stream_init begin\n");
    bool play_netflix_intro = mpegplayer_netflix_launch &&
                              (netflix_restart ||
                               !mpeg_resume_available(videofile));
    if (stream_init(play_netflix_intro) < STREAM_OK) {
        /* Fatal because this should not fail */
        MPLOG("stream_init failed\n");
        DEBUGF("Could not initialize streams\n");
        status = PLUGIN_ERROR;
    } else {
        MPLOG("stream_init ok\n");
        int next_action = VIDEO_STOP;
        bool get_videofile_says = true;

        while (!quit)
        {
            init_settings(videofile);
            rb->strlcpy(mpeg_osd_path, videofile, sizeof(mpeg_osd_path));
            mpeg_netflix_load_markers(videofile);

#ifdef HAVE_LCD_COLOR
            if (mpegplayer_livetv_launch)
            {
                /* Live TV always joins the broadcast already in progress,
                 * so the resume point comes from the clock, not from a
                 * stored bookmark. */
                settings.display_mode = MPEG_VIDEO_DISPLAY_FIT;
                settings.play_mode = 0;
                settings.resume_options = MPEG_RESUME_ALWAYS;
                settings.resume_time = livetv_resume;
                stream_vo_set_display_mode(settings.display_mode);
                /* Every reopen here is a (possibly new) channel, whether
                 * from initial launch, the guide, or channel up/down, so
                 * this is the one place that needs to notice a tune onto
                 * or off of the Weather channel. */
                livetv_update_weather_view(settings.resume_time / TS_SECOND,
                                           true);
            }
            else
#endif
            if (feed.active)
            {
                settings.display_mode = MPEG_VIDEO_DISPLAY_FIT;
                settings.play_mode = 0;
                settings.resume_options = MPEG_RESUME_RESTART;
                settings.resume_time = 0;
                stream_vo_set_display_mode(settings.display_mode);
                feed.state_dirty = true;
            }
            else if (mpegplayer_instagram_feed_launch)
            {
                settings.display_mode = MPEG_VIDEO_DISPLAY_FIT;
                settings.play_mode = 0;
                settings.resume_options = MPEG_RESUME_RESTART;
                settings.resume_time = 0;
                stream_vo_set_display_mode(settings.display_mode);
            }
            else if (mpegplayer_youtube_launch)
            {
                settings.display_mode = MPEG_VIDEO_DISPLAY_FIT;
                settings.play_mode = 0;
                settings.resume_options = MPEG_RESUME_ALWAYS;
                stream_vo_set_display_mode(settings.display_mode);
            }
            else if (mpegplayer_maps_dashcam_launch)
            {
                /* Maps opens a complete drive, not a seek preview. Start at
                 * frame zero and use the full 320x240 display immediately. */
                settings.display_mode = MPEG_VIDEO_DISPLAY_FILL;
                settings.play_mode = 0;
                settings.resume_options = MPEG_RESUME_RESTART;
                settings.resume_time = 0;
                stream_vo_set_display_mode(settings.display_mode);
            }
            else if (netflix_restart)
            {
                settings.resume_options = MPEG_RESUME_RESTART;
                settings.resume_time = 0;
            }

            MPLOG("init_settings done\n");

            MPLOG("stream_open begin\n");
            int result = stream_open(videofile);
            bool manual_skip = false;
            MPLOG("stream_open result=%d\n", result);

            if (result >= STREAM_OK) {
#ifdef HAVE_LCD_COLOR
                /* Something played, so the lineup is not broken after
                 * all - let a later bad file get its own full retry. */
                livetv_open_failures = 0;
#endif
                if (feed.active || mpegplayer_instagram_feed_launch ||
                    mpegplayer_livetv_launch ||
                    mpegplayer_maps_dashcam_launch)
                {
                    result = MPEG_START_RESTART;
                }
                else if (mpegplayer_youtube_launch)
                {
                    /* Continue immediately from the saved per-file position.
                     * mpeg_start_menu() remains UI-free in RESUME_ALWAYS mode
                     * and applies the stock early/95%-complete thresholds. */
                    result = mpeg_start_menu(stream_get_duration());
                }
                else
                {
                    /* start menu */
                    rb->lcd_clear_display();
                    rb->lcd_update();
                    result = mpeg_start_menu(stream_get_duration());
                }

                next_action = VIDEO_STOP;
                if (result != MPEG_START_QUIT) {
                    /* Enter button loop and process UI */
                    MPLOG("button_loop begin\n");
                    next_action = button_loop();
                    MPLOG("button_loop end action=%d\n", next_action);
                    manual_skip = next_action & VIDEO_ACTION_MANUAL;
                    next_action &= ~VIDEO_ACTION_MANUAL;
                }

                MPLOG("stream_close begin\n");
                stream_close();
                MPLOG("stream_close done\n");

                /* The standalone Instagram feed reopens its app to resolve
                 * the adjacent mixed-media post. Preserve the final decoded
                 * frame until that app atomically draws the ready post;
                 * clearing here exposed a black/white intermediate frame on
                 * iPod LCD hardware. */
                if (!mpegplayer_livetv_desktop && !feed.active &&
                    !mpegplayer_instagram_feed_launch)
                {
                    rb->lcd_clear_display();
                    rb->lcd_update();
                }

                if (!feed.active)
                    save_settings();
            } else {
                /* Problem with file; display message about it - not
                 * considered a plugin error */
                long tick;
                const char *errstring;

                /* Repeat-one must not turn one damaged sync into an
                 * unbreakable reopen loop. Search forward for a playable
                 * card just as the ordinary playlist path does. */
                if (feed.active && next_action == VIDEO_REPEAT)
                    next_action = VIDEO_NEXT;

                DEBUGF("Could not open %s\n", videofile);
                MPLOG("stream_open failed result=%d\n", result);

#ifdef HAVE_LCD_COLOR
                if (mpegplayer_livetv_launch)
                {
                    /* One unplayable programme must not take Live TV
                     * down. Nothing here uses get_videofile(), and
                     * livetv_advance() re-resolves the same instant on
                     * the same channel, so retrying would reopen the
                     * very same file forever and the guide would stay
                     * unreachable. Move to the next channel instead,
                     * and once the whole lineup has been tried, hand
                     * control to the guide so the viewer can pick
                     * something themselves. */
                    livetv_open_failures++;

                    if (livetv_open_failures < livetv_channel_count() &&
                        livetv_change_channel(1) == VIDEO_NEXT &&
                        livetv_advance(videofile, sizeof(videofile),
                                       &livetv_resume))
                    {
                        rb->splash(HZ, "Channel unavailable");
                        continue;   /* reopen on the new channel */
                    }

                    /* Every channel failed: this is a broken install
                     * rather than one bad file, so say so and leave
                     * instead of spinning. */
                    livetv_open_failures = 0;
                    rb->splash(HZ * 2, "No playable channel");
                    next_action = VIDEO_STOP;
                    quit = true;
                    break;
                }
#endif
                switch (result)
                {
                case STREAM_UNSUPPORTED:
                    errstring = "Unsupported format";
                    break;
                default:
                    errstring = "Error opening file: %d";
                }

                tick = *rb->current_tick + HZ*2;

                rb->splashf(0, errstring, result);

                /* Be sure it doesn't get stuck in an unbreakable loop of bad
                 * files, just in case! Otherwise, keep searching in the
                 * chosen direction until a good one is found. */
                while (!quit && TIME_BEFORE(*rb->current_tick, tick))
                {
                    int button = mpeg_button_get(HZ*2);

                    switch (button)
                    {
                    case MPEG_STOP:
                    case ACTION_STD_CANCEL:
                        /* Abort the search and exit */
                        next_action = VIDEO_STOP;
                        quit = true;
                        break;

                    case BUTTON_NONE:
                        if (feed.active || settings.play_mode != 0) {
                            if (next_action == VIDEO_STOP) {
                                /* Default to next file */
                                next_action = VIDEO_NEXT;
                            }
                            else if (next_action == VIDEO_PREV &&
                                     !get_videofile_says) {
                                /* Was first file already; avoid endlessly
                                 * retrying it */
                                next_action = VIDEO_STOP;
                            }
                        }
                        break;

                    default:
                        rb->default_event_handler(button);
                    } /* switch */
                } /* while */
            }

            /* return value of button_loop says, what's next */
            switch (next_action)
            {
            case VIDEO_REPEAT:
                /* Keep videofile unchanged. The outer loop reopens it from
                 * zero; feed selection and recommendation state do not move. */
                break;

            case VIDEO_NEXT:
            {
#ifdef HAVE_LCD_COLOR
                if (mpegplayer_livetv_launch)
                {
                    /* Whatever the clock says is on the air is what plays
                     * next, whether we got here because a programme ended
                     * or because the viewer changed channel. */
                    get_videofile_says = livetv_advance(videofile,
                                                        sizeof(videofile),
                                                        &livetv_resume);
                    quit = !get_videofile_says;
                    break;
                }
#endif
                get_videofile_says = feed.active ?
                    feed_get_next_file(VIDEO_NEXT, videofile, sizeof(videofile)) :
                    get_videofile(VIDEO_NEXT, videofile, sizeof(videofile));
                /* quit after finished the last videofile */
                quit = !get_videofile_says;

                if (manual_skip)
                {
                    rb->system_sound_play(get_videofile_says ?
                                          SOUND_TRACK_SKIP : SOUND_TRACK_NO_MORE);
                }

                break;
                }
            case VIDEO_PREV:
            {
                get_videofile_says = feed.active ?
                    feed_get_next_file(VIDEO_PREV, videofile, sizeof(videofile)) :
                    get_videofile(VIDEO_PREV, videofile, sizeof(videofile));
                /* if there is no previous file, play the same videofile */

                if (manual_skip)
                {
                    rb->system_sound_play(get_videofile_says ?
                                          SOUND_TRACK_SKIP : SOUND_TRACK_NO_MORE);
                }

                break;
                }
            case VIDEO_STOP:
            {
                quit = true;
                break;
                }
            }
        } /* while */
    }

    if (feed.active)
        feed_flush_pending();

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_YUV)
    rb->lcd_set_mode(LCD_MODE_RGB565);
    feed_yuv_mode_active = false;
#endif

    stream_exit();
    MPLOG("stream_exit done\n");

    /* Actually handle delayed processing of system events of interest
     * that were captured in other button loops */
    mpeg_sysevent_handle();
    MPLOG("plugin_start return=%d\n", status);

    if (mpegplayer_youtube_app_launch && status != PLUGIN_USB_CONNECTED)
    {
        static char return_parameter[MAX_PATH + 8];

        rb->snprintf(return_parameter, sizeof(return_parameter), "return:%s",
                     videofile);
        return rb->plugin_open(PLUGIN_APPS_DIR "/youtube.rock",
                               return_parameter);
    }

    if (mpegplayer_onlyfans_app_launch && status != PLUGIN_USB_CONNECTED)
    {
        static char return_parameter[MAX_PATH + 8];

        rb->snprintf(return_parameter, sizeof(return_parameter), "return:%s",
                     videofile);
        return rb->plugin_open(PLUGIN_APPS_DIR "/onlyfans.rock",
                               return_parameter);
    }

    if (mpegplayer_instagram_app_launch && status != PLUGIN_USB_CONNECTED)
    {
        static char return_parameter[MAX_PATH + 24];
        const char *prefix = "return:";

        if (mpegplayer_instagram_feed_launch &&
            !mpegplayer_instagram_return_profile)
        {
            if (mpegplayer_instagram_return_direction > 0)
                prefix = "return-home-next:";
            else if (mpegplayer_instagram_return_direction < 0)
                prefix = "return-home-prev:";
            else
                prefix = "return-home:";
        }
        rb->snprintf(return_parameter, sizeof(return_parameter), "%s%s",
                     prefix, videofile);
        return rb->plugin_open(PLUGIN_APPS_DIR "/instagram.rock",
                               return_parameter);
    }

    if (mpegplayer_reddit_app_launch && status != PLUGIN_USB_CONNECTED)
    {
        static char return_parameter[MAX_PATH + 8];

        rb->snprintf(return_parameter, sizeof(return_parameter), "return:%s",
                     videofile);
        return rb->plugin_open(PLUGIN_APPS_DIR "/reddit.rock",
                               return_parameter);
    }

    return status;
}
