/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Live TV: a DIRECTV style program guide driven by the wall clock.
 *
 * The schedule is generated on the PC and is a seven day rotation keyed on
 * the weekday, so the same instant always resolves to the same programme on
 * the iPod and in rockpod. Nothing is decided at playback time.
 *
 * See docs/livetv-directv-guide-spec.md
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
#ifndef MPEGPLAYER_LIVETV_H
#define MPEGPLAYER_LIVETV_H

#include "plugin.h"

#define LIVETV_PARAM_PREFIX     "-livetv:"
#define LIVETV_PARAM_PREFIX_LEN (sizeof(LIVETV_PARAM_PREFIX) - 1)
#define LIVETV_DM_PARAM_PREFIX     "-livetvdm:"
#define LIVETV_DM_PARAM_PREFIX_LEN (sizeof(LIVETV_DM_PARAM_PREFIX) - 1)

#define LIVETV_DEFAULT_ROOT     "/Videos/LiveTV"
#define LIVETV_CHANNELS_FILE    "channels.tsv"
#define LIVETV_GUIDE_FILE       "guide.tsv"
#define LIVETV_STATE_FILE       ".livetv_state"
#define LIVETV_BRAND_LOGO       "logos/directv.bmp"

/* Static model caps. Sized in the spec; nothing is heap allocated and no
 * core_alloc() memory is touched. */
#define LIVETV_MAX_CHANNELS     24
#define LIVETV_MAX_SLOTS        2048
#define LIVETV_TEXT_POOL        36864
#define LIVETV_PATH_POOL        24576

#define LIVETV_CALLSIGN_LEN     10
#define LIVETV_NAME_LEN         32
#define LIVETV_CATEGORY_LEN     16

#define LIVETV_KIND_SHOW        'S'
#define LIVETV_KIND_AD          'A'

/* The Weather channel is recognised by category, not by a new slot kind or
 * a fixed channel number: it is an ordinary channel whose shows are real
 * MPEG files (a 64-second panel/presenter/video clock carrying the user's
 * own looped music), scheduled and played exactly like any other channel. See
 * docs/livetv-weather-channel-spec.md section 2 for why a new slot kind
 * was rejected. */
#define LIVETV_WEATHER_CATEGORY "Weather"

#define LIVETV_DAY_SECONDS      86400
#define LIVETV_SLOT_SECONDS     1800    /* guide column = half an hour */

/* Picture in guide window and its white frame, matching the DIRECTV layout.
 * The window is flush with the right edge of the screen so the guide never
 * has to repaint a sliver beside it. Both coordinates are even because
 * vo_setup() aligns the video rectangle to even pixels. */
#define LIVETV_PIG_W            82
#define LIVETV_PIG_H            62
#define LIVETV_PIG_LOCAL_X      \
    (mpegplayer_livetv_desktop ? LIVETV_DM_WIN_W - LIVETV_PIG_W : 238)
#define LIVETV_PIG_LOCAL_Y      4
#define LIVETV_PIG_X            \
    (mpegplayer_livetv_desktop ? \
        LIVETV_DM_WIN_X + LIVETV_PIG_LOCAL_X : LIVETV_PIG_LOCAL_X)
#define LIVETV_PIG_Y            \
    (mpegplayer_livetv_desktop ? \
        LIVETV_DM_WIN_Y + LIVETV_DM_TITLE_H + LIVETV_PIG_LOCAL_Y : \
        LIVETV_PIG_LOCAL_Y)
#define LIVETV_PIG_BOX_X        (LIVETV_PIG_LOCAL_X - 1)
#define LIVETV_PIG_BOX_Y        (LIVETV_PIG_LOCAL_Y - 1)
#define LIVETV_PIG_BOX_W        (LIVETV_PIG_W + 1)
#define LIVETV_PIG_BOX_H        (LIVETV_PIG_H + 2)

/* Desktop Mode serialises its already-painted framebuffer before replacing
 * itself with mpegplayer. The player preserves that underlay in plugin-owned
 * memory so decoder teardown cannot erase the Dock during a channel change.
 * Only the active Aqua title strip is loaded here; video and DIRECTV chrome
 * are painted in the window body by the same plugin that owns decoding. */
#if LCD_WIDTH >= 1920
#define LIVETV_DM_WIN_W          897
#define LIVETV_DM_WIN_H          671
#define LIVETV_DM_WIN_X          ((LCD_WIDTH - LIVETV_DM_WIN_W) / 2)
#define LIVETV_DM_WIN_Y          170
#define LIVETV_DM_TITLE_PATH \
    PLUGIN_APPS_DATA_DIR \
        "/desktop_mode_snow_leopard/1920x1080/chrome/title-bar.897x24x16.bmp"
#else
#define LIVETV_DM_WIN_W          304
#define LIVETV_DM_WIN_H          174
#define LIVETV_DM_WIN_X          ((LCD_WIDTH - LIVETV_DM_WIN_W) / 2)
#define LIVETV_DM_WIN_Y          24
#define LIVETV_DM_TITLE_PATH \
    PLUGIN_APPS_DATA_DIR \
        "/desktop_mode_snow_leopard/320x240/chrome/title-bar.304x24x16.bmp"
#endif
#define LIVETV_DM_TITLE_H        24
#define LIVETV_DM_BODY_H         (LIVETV_DM_WIN_H - LIVETV_DM_TITLE_H)
#define LIVETV_DM_VIDEO_X        (LIVETV_DM_WIN_X + 2)
#if LCD_WIDTH >= 1920
/* At 1080p the tuned channel owns the entire window content area. The
 * decoder still preserves aspect ratio inside these bounds. */
#define LIVETV_DM_VIDEO_W        ((LIVETV_DM_WIN_W - 4) & ~1)
#define LIVETV_DM_VIDEO_H        ((LIVETV_DM_BODY_H - 4) & ~1)
#define LIVETV_DM_VIDEO_Y        (LIVETV_DM_WIN_Y + LIVETV_DM_TITLE_H + 2)
#else
#define LIVETV_DM_VIDEO_W        (((LIVETV_DM_WIN_W * 2 / 3) - 6) & ~1)
#define LIVETV_DM_VIDEO_H_4_3    ((LIVETV_DM_VIDEO_W * 3 / 4) & ~1)
#define LIVETV_DM_VIDEO_H        \
    MIN(LIVETV_DM_VIDEO_H_4_3, (LIVETV_DM_BODY_H - 4) & ~1)
#define LIVETV_DM_VIDEO_Y        \
    (LIVETV_DM_WIN_Y + LIVETV_DM_TITLE_H + 2 + \
     (LIVETV_DM_BODY_H - 4 - LIVETV_DM_VIDEO_H) / 2)
#endif
#define LIVETV_DM_VIDEO_BOX_X    (LIVETV_DM_VIDEO_X - 1)
#define LIVETV_DM_VIDEO_BOX_Y    (LIVETV_DM_VIDEO_Y - 1)
#define LIVETV_DM_VIDEO_BOX_W    (LIVETV_DM_VIDEO_W + 2)
#define LIVETV_DM_VIDEO_BOX_H    (LIVETV_DM_VIDEO_H + 2)
#define LIVETV_DM_SIDEBAR_X      \
    (LIVETV_DM_VIDEO_X + LIVETV_DM_VIDEO_W + 4)
#define LIVETV_DM_SIDEBAR_W      \
    (LIVETV_DM_WIN_X + LIVETV_DM_WIN_W - 2 - LIVETV_DM_SIDEBAR_X)

/* One file the player opens. A guide lists programmes, not the commercials
 * inside them, so every slot also carries the enclosing programme block;
 * commercials inherit the block of the show they interrupt. */
struct livetv_slot
{
    uint32_t start;       /* seconds after local midnight */
    uint32_t block_start; /* start of the enclosing programme */
    uint16_t block_dur;   /* length of the enclosing programme */
    uint16_t dur;         /* seconds */
    uint16_t chan;        /* index into the channel table */
    uint16_t title_off;   /* offset into the text pool */
    uint16_t rating_off;  /* offset into the text pool */
    uint16_t desc_off;    /* offset into the text pool */
    uint16_t path_off;    /* offset into the path pool */
    uint8_t  day;         /* 0 = Sunday, matches struct tm tm_wday */
    uint8_t  kind;        /* LIVETV_KIND_SHOW or LIVETV_KIND_AD */
};

struct livetv_channel
{
    int  number;
    char callsign[LIVETV_CALLSIGN_LEN];
    char name[LIVETV_NAME_LEN];
    char category[LIVETV_CATEGORY_LEN];
    char logo[64];
    int  first_slot;
    int  slot_count;
    bool favourite;
};

/* Model -------------------------------------------------------------- */

bool livetv_load(const char *root);
bool livetv_is_active(void);

int  livetv_channel_count(void);
const struct livetv_channel *livetv_channel(int index);

int  livetv_current_channel(void);
void livetv_set_current_channel(int index);
/* Move "delta" channels, wrapping at both ends and skipping channels
 * with nothing on the air. False when no other channel is playable. */
bool livetv_step_channel(int delta);

const char *livetv_slot_title(const struct livetv_slot *slot);
const char *livetv_slot_rating(const struct livetv_slot *slot);
const char *livetv_slot_desc(const struct livetv_slot *slot);
bool livetv_slot_path(const struct livetv_slot *slot, char *buf, size_t size);

/* What is airing on a channel "delta" seconds from now? "offset" receives
 * how far into the programme that instant falls. */
const struct livetv_slot *livetv_slot_at(int chan, long delta,
                                         uint32_t *offset);
/* The programme following the one airing "delta" seconds from now. */
const struct livetv_slot *livetv_slot_next(int chan, long delta);

/* Seconds after local midnight, and the weekday, from the RTC. */
uint32_t livetv_now_secs(void);
int livetv_now_day(void);

/* Resolve the file a channel should be playing right now. "resume" receives
 * the join-in point in stream ticks. */
bool livetv_tune(int chan, char *videofile, size_t size, uint32_t *resume);
/* Resolve what should play once the current programme ends. */
bool livetv_advance(char *videofile, size_t size, uint32_t *resume);

void livetv_save_state(void);

/* Guide UI ----------------------------------------------------------- */

void livetv_guide_enter(void);
void livetv_guide_draw(void);
/* Repaint only the changed grid rows plus the information banner. */
void livetv_guide_draw_rows(void);
/* Navigate. dchan moves between channels, dtime between programmes. */
bool livetv_guide_move(int dchan, int dtime);
/* Jump the visible window by whole hours (the colour keys). */
void livetv_guide_jump_hours(int hours);
/* Cycle All Channels / Favourites / a category. */
void livetv_guide_cycle_filter(void);
const char *livetv_guide_filter_name(void);
/* Snap the selection back to what is airing now. */
void livetv_guide_reset_to_now(void);
int  livetv_guide_selected_channel(void);
bool livetv_guide_selection_is_live(void);

/* Guide Options modal, drawn by livetv.c and driven by the caller. */
#define LIVETV_OPTION_COUNT 4
void livetv_options_draw(int selected);
const char *livetv_options_label(int index);
/* Returns true when the guide needs a full repaint afterwards. */
bool livetv_options_activate(int index);

/* What the guide session asked the player to do next */
enum livetv_guide_result
{
    LIVETV_GUIDE_WATCH = 0, /* go full screen on the channel already open */
    LIVETV_GUIDE_TUNE,      /* the channel changed, reopen the stream */
    LIVETV_GUIDE_EXIT,      /* leave Live TV */
};

/* Run the guide modally while the current channel keeps decoding into the
 * picture in guide window. */
int livetv_guide_run(void);

/* Full screen overlays ------------------------------------------------ */

/* Tall enough for three lines of the UI font plus padding, and for the two
 * row mini guide. The video is clipped away from these strips, so they must
 * not be smaller than what gets drawn into them. */
#define LIVETV_INFO_H  60
#define LIVETV_MINI_H  44

void livetv_draw_info_banner(int chan);
void livetv_draw_mini_guide(int chan);
void livetv_clear_overlay(void);
/* Cache the small Aqua title strip before stream_init() takes playback
 * memory and starts decoder callbacks. */
bool livetv_desktop_prepare(void);
/* Draw only the Aqua title/body pixels around the windowed decoded video. */
void livetv_desktop_draw_window(void);

/* Weather channel ------------------------------------------------------ */

/* True when "chan" is the Weather channel. */
bool livetv_channel_is_weather(int chan);
/* True only while the current Weather slot is forecast programming. Ads
 * remain ordinary full-screen video even though they share the channel. */
bool livetv_weather_program_active(void);
/* Seconds into the current forecast carrier. Anchored to the schedule's
 * tune-in resume point, then advanced with the playback/UI monotonic clock. */
uint32_t livetv_weather_program_seconds(void);
/* The carrier's 64-second broadcast clock exposes presenter and weather
 * inserts at fixed phases. */
bool livetv_weather_wants_video(uint32_t stream_seconds);

/* Load (or reload) the forecast the Weather channel shows, from the same
 * /.rockbox/rockpod/weather/forecast.tsv the standalone weather.rock reads.
 * Call once when tuning into the channel, not per frame or per panel. */
void livetv_weather_enter(uint32_t stream_seconds);
/* Synchronise the selected panel and its lightweight animation to the
 * carrier clock. Safe to call on every idle tick. */
void livetv_weather_tick(uint32_t stream_seconds);
/* Repaint whichever panel is currently selected, e.g. after the video
 * rectangle was reasserted by an overlay or the guide returning. */
void livetv_weather_draw(void);

/* Video output -------------------------------------------------------- */

extern bool mpegplayer_livetv_launch;
extern bool mpegplayer_livetv_pig;
extern bool mpegplayer_livetv_desktop;
extern bool mpegplayer_livetv_guide_active;
/* True during native forecast-panel phases. The video output keeps decoding
 * on its normal full-screen rectangle but skips framebuffer blits until the
 * next presenter/video phase. Guide picture-in-guide remains visible. */
extern bool mpegplayer_livetv_weather_hidden;

#endif /* MPEGPLAYER_LIVETV_H */
