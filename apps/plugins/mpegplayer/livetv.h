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

#define LIVETV_DAY_SECONDS      86400
#define LIVETV_SLOT_SECONDS     1800    /* guide column = half an hour */

/* Picture in guide window and its white frame, matching the DIRECTV layout.
 * The window is flush with the right edge of the screen so the guide never
 * has to repaint a sliver beside it. Both coordinates are even because
 * vo_setup() aligns the video rectangle to even pixels. */
#define LIVETV_PIG_X            238
#define LIVETV_PIG_Y            4
#define LIVETV_PIG_W            (LCD_WIDTH - LIVETV_PIG_X)
#define LIVETV_PIG_H            62
#define LIVETV_PIG_BOX_X        (LIVETV_PIG_X - 1)
#define LIVETV_PIG_BOX_Y        (LIVETV_PIG_Y - 1)
#define LIVETV_PIG_BOX_W        (LCD_WIDTH - LIVETV_PIG_BOX_X)
#define LIVETV_PIG_BOX_H        (LIVETV_PIG_H + 2)

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

/* Video output -------------------------------------------------------- */

extern bool mpegplayer_livetv_launch;
extern bool mpegplayer_livetv_pig;

#endif /* MPEGPLAYER_LIVETV_H */
