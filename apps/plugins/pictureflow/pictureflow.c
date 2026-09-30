/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id$
*
* Copyright (C) 2007 Jonas Hurrelmann (j@outpo.st)
* Copyright (C) 2007 Nicolas Pennequin
* Copyright (C) 2007 Ariya Hidayat (ariya@kde.org) (original Qt Version)
*
* Original code: http://code.google.com/p/pictureflow/
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

#include "plugin.h"
#include "albumart.h"
#include "lib/ipodjs_retailos.h"
#include "lib/read_image.h"
#include "lib/pluginlib_actions.h"
#include "lib/pluginlib_exit.h"
#include "lib/helper.h"
#include "lib/configfile.h"
#include "lib/grey.h"
#include "lib/mylcd.h"
#include "lib/feature_wrappers.h"
#include "lib/simple_viewer.h"

/******************************* Globals ***********************************/
static fb_data *lcd_fb;

/*
 *  Targets which use plugin_get_audio_buffer() can't have playback from
 * within pictureflow itself, as the whole core audio buffer is occupied */
#define PF_PLAYBACK_CAPABLE (PLUGIN_BUFFER_SIZE > 0x10000)

#if PF_PLAYBACK_CAPABLE
#include "lib/playback_control.h"
#include "lib/mul_id3.h"
#endif

#define PF_PREV ACTION_STD_PREV
#define PF_PREV_REPEAT ACTION_STD_PREVREPEAT
#define PF_NEXT ACTION_STD_NEXT
#define PF_NEXT_REPEAT ACTION_STD_NEXTREPEAT
#define PF_SELECT ACTION_STD_OK
#define PF_CONTEXT ACTION_STD_CONTEXT
#define PF_BACK ACTION_STD_CANCEL
#define PF_MENU ACTION_STD_MENU
#define PF_WPS ACTION_TREE_WPS
#define PF_JMP ACTION_LISTTREE_PGDOWN
#define PF_JMP_PREV ACTION_LISTTREE_PGUP

#define PF_QUIT (LAST_ACTION_PLACEHOLDER + 1)
#define PF_TRACKLIST (LAST_ACTION_PLACEHOLDER + 2)
#define PF_SORTING_NEXT (LAST_ACTION_PLACEHOLDER + 3)
#define PF_SORTING_PREV (LAST_ACTION_PLACEHOLDER + 4)

#if defined(HAVE_SCROLLWHEEL) || CONFIG_KEYPAD == IRIVER_H10_PAD || \
    CONFIG_KEYPAD == MPIO_HD300_PAD
#if (CONFIG_KEYPAD != IPOD_1G2G_PAD) \
    && (CONFIG_KEYPAD != IPOD_3G_PAD) \
    && (CONFIG_KEYPAD != IPOD_4G_PAD) \
    && (CONFIG_KEYPAD != FIIO_M3K_PAD)
#define USE_CORE_PREVNEXT
#endif
#endif

#ifndef USE_CORE_PREVNEXT
    /* scrollwheel targets use the wheel, just as they do in lists,
     * so there's no need for a special context,
     * others use left/right here too (as oppsed to up/down in lists) */
const struct button_mapping pf_context_album_scroll[] =
{
#ifdef HAVE_TOUCHSCREEN
    {PF_PREV,         BUTTON_MIDLEFT,                 BUTTON_NONE},
    {PF_PREV_REPEAT,  BUTTON_MIDLEFT|BUTTON_REPEAT,   BUTTON_NONE},
    {PF_NEXT,         BUTTON_MIDRIGHT,                BUTTON_NONE},
    {PF_NEXT_REPEAT,  BUTTON_MIDRIGHT|BUTTON_REPEAT,  BUTTON_NONE},
#endif
#if (CONFIG_KEYPAD == IAUDIO_M3_PAD || CONFIG_KEYPAD == MROBE500_PAD)
    {PF_PREV,         BUTTON_RC_REW,              BUTTON_NONE},
    {PF_PREV_REPEAT,  BUTTON_RC_REW|BUTTON_REPEAT,BUTTON_NONE},
    {PF_NEXT,         BUTTON_RC_FF,               BUTTON_NONE},
    {PF_NEXT_REPEAT,  BUTTON_RC_FF|BUTTON_REPEAT, BUTTON_NONE},
#elif (CONFIG_KEYPAD == IPOD_1G2G_PAD) \
    || (CONFIG_KEYPAD == IPOD_3G_PAD) \
    || (CONFIG_KEYPAD == IPOD_4G_PAD) \
    || (CONFIG_KEYPAD == FIIO_M3K_PAD)
    {PF_JMP_PREV,     BUTTON_LEFT,                BUTTON_NONE},
    {PF_JMP_PREV,     BUTTON_LEFT|BUTTON_REPEAT,  BUTTON_NONE},
    {PF_JMP,          BUTTON_RIGHT,               BUTTON_NONE},
    {PF_JMP,          BUTTON_RIGHT|BUTTON_REPEAT, BUTTON_NONE},
    {ACTION_NONE,     BUTTON_LEFT|BUTTON_REL,     BUTTON_LEFT},
    {ACTION_NONE,     BUTTON_RIGHT|BUTTON_REL,    BUTTON_RIGHT},
    {ACTION_NONE,     BUTTON_LEFT|BUTTON_REPEAT,  BUTTON_LEFT},
    {ACTION_NONE,     BUTTON_RIGHT|BUTTON_REPEAT, BUTTON_RIGHT},
#elif defined(BUTTON_LEFT) && defined(BUTTON_RIGHT)
    {PF_PREV,         BUTTON_LEFT,                BUTTON_NONE},
    {PF_PREV_REPEAT,  BUTTON_LEFT|BUTTON_REPEAT,  BUTTON_NONE},
    {PF_NEXT,         BUTTON_RIGHT,               BUTTON_NONE},
    {PF_NEXT_REPEAT,  BUTTON_RIGHT|BUTTON_REPEAT, BUTTON_NONE},
    {ACTION_NONE,     BUTTON_LEFT|BUTTON_REL,     BUTTON_LEFT},
    {ACTION_NONE,     BUTTON_RIGHT|BUTTON_REL,    BUTTON_RIGHT},
    {ACTION_NONE,     BUTTON_LEFT|BUTTON_REPEAT,  BUTTON_LEFT},
    {ACTION_NONE,     BUTTON_RIGHT|BUTTON_REPEAT, BUTTON_RIGHT},
#else
#warning "LEFT/RIGHT not defined!"
#endif
    LAST_ITEM_IN_LIST__NEXTLIST(CONTEXT_PLUGIN|1)
};
#endif /* !USE_CORE_PREVNEXT */

const struct button_mapping pf_context_buttons[] =
{
#ifdef HAVE_TOUCHSCREEN
    {PF_SELECT,       BUTTON_CENTER,              BUTTON_NONE},
    {PF_BACK,         BUTTON_BOTTOMRIGHT,         BUTTON_NONE},
#endif
#if CONFIG_KEYPAD == PHILIPS_HDD1630_PAD || \
    CONFIG_KEYPAD == GIGABEAT_PAD || CONFIG_KEYPAD == GIGABEAT_S_PAD || \
    CONFIG_KEYPAD == MROBE100_PAD || CONFIG_KEYPAD == MROBE500_PAD || \
    CONFIG_KEYPAD == PHILIPS_SA9200_PAD || CONFIG_KEYPAD == SANSA_CLIP_PAD || \
    CONFIG_KEYPAD == SANSA_FUZEPLUS_PAD || CONFIG_KEYPAD == CREATIVE_ZENXFI3_PAD || \
    CONFIG_KEYPAD == XDUOO_X3_PAD
    {PF_QUIT,         BUTTON_POWER,               BUTTON_NONE},
#if CONFIG_KEYPAD == SANSA_FUZEPLUS_PAD
    {PF_MENU,         BUTTON_SELECT|BUTTON_REPEAT,  BUTTON_SELECT},
#endif
#elif CONFIG_KEYPAD == SANSA_FUZE_PAD
    {PF_QUIT,         BUTTON_HOME|BUTTON_REPEAT,  BUTTON_NONE},
    {PF_TRACKLIST,    BUTTON_RIGHT,  BUTTON_NONE},
/* These all use short press of BUTTON_POWER for menu, map long POWER to quit
*/
#elif CONFIG_KEYPAD == SANSA_C200_PAD || CONFIG_KEYPAD == SANSA_M200_PAD || \
    CONFIG_KEYPAD == IRIVER_H10_PAD || CONFIG_KEYPAD == COWON_D2_PAD
    {PF_QUIT,         BUTTON_POWER|BUTTON_REPEAT, BUTTON_POWER},
#if CONFIG_KEYPAD == COWON_D2_PAD
    {PF_BACK,         BUTTON_POWER|BUTTON_REL,    BUTTON_POWER},
    {ACTION_NONE,     BUTTON_POWER,               BUTTON_NONE},
#endif
#elif CONFIG_KEYPAD == SANSA_E200_PAD
    {PF_QUIT,         BUTTON_POWER,               BUTTON_NONE},
#elif (CONFIG_KEYPAD == IPOD_1G2G_PAD) \
    || (CONFIG_KEYPAD == IPOD_3G_PAD) \
    || (CONFIG_KEYPAD == IPOD_4G_PAD)
    {PF_MENU,         BUTTON_MENU|BUTTON_REPEAT,  BUTTON_MENU},
    {PF_QUIT,         BUTTON_MENU|BUTTON_REL,     BUTTON_MENU},
    {PF_SORTING_NEXT, BUTTON_SELECT|BUTTON_MENU,  BUTTON_NONE},
    {PF_SORTING_PREV, BUTTON_SELECT|BUTTON_PLAY,  BUTTON_NONE},
#elif CONFIG_KEYPAD == MPIO_HD300_PAD
    {PF_QUIT,         BUTTON_MENU|BUTTON_REPEAT,  BUTTON_MENU},
#elif CONFIG_KEYPAD == IAUDIO_M3_PAD
    {PF_QUIT,         BUTTON_RC_REC,              BUTTON_NONE},
#elif CONFIG_KEYPAD == IRIVER_H100_PAD || CONFIG_KEYPAD == IRIVER_H300_PAD
    {PF_QUIT,         BUTTON_OFF,                 BUTTON_NONE},
#elif CONFIG_KEYPAD == PBELL_VIBE500_PAD
    {PF_QUIT,         BUTTON_REC,                 BUTTON_NONE},
#elif CONFIG_KEYPAD == SAMSUNG_YH820_PAD || CONFIG_KEYPAD == SAMSUNG_YH92X_PAD
    {PF_QUIT,         BUTTON_REW|BUTTON_REPEAT,   BUTTON_REW},
    {PF_MENU,         BUTTON_REW|BUTTON_REL,      BUTTON_REW},
    {PF_SELECT,       BUTTON_PLAY|BUTTON_REL,     BUTTON_PLAY},
    {PF_CONTEXT,      BUTTON_FFWD|BUTTON_REPEAT,  BUTTON_FFWD},
    {PF_TRACKLIST,    BUTTON_FFWD|BUTTON_REL,     BUTTON_FFWD},
    {PF_WPS,          BUTTON_PLAY|BUTTON_REPEAT,  BUTTON_PLAY},
#elif CONFIG_KEYPAD == FIIO_M3K_PAD
    {PF_JMP_PREV,     BUTTON_LEFT,                BUTTON_NONE},
    {PF_JMP_PREV,     BUTTON_LEFT|BUTTON_REPEAT,  BUTTON_NONE},
    {PF_JMP,          BUTTON_RIGHT,               BUTTON_NONE},
    {PF_JMP,          BUTTON_RIGHT|BUTTON_REPEAT, BUTTON_NONE},
    {PF_MENU,         BUTTON_POWER|BUTTON_REL,    BUTTON_POWER},
    {PF_SORTING_NEXT, BUTTON_VOL_UP,              BUTTON_NONE},
    {PF_SORTING_PREV, BUTTON_VOL_DOWN,            BUTTON_NONE},
    {PF_QUIT,         BUTTON_POWER|BUTTON_REPEAT, BUTTON_POWER},
    {PF_CONTEXT,      BUTTON_MENU|BUTTON_REL,     BUTTON_MENU},
    {PF_TRACKLIST,    BUTTON_MENU|BUTTON_REPEAT,  BUTTON_MENU},
#endif
#if CONFIG_KEYPAD == IAUDIO_M3_PAD
    LAST_ITEM_IN_LIST__NEXTLIST(CONTEXT_STD|CONTEXT_REMOTE)
#else
    LAST_ITEM_IN_LIST__NEXTLIST(CONTEXT_TREE)
#endif
};
const struct button_mapping *pf_contexts[] =
{
#ifndef USE_CORE_PREVNEXT
    pf_context_album_scroll,
#endif
    pf_context_buttons
};

#if LCD_DEPTH < 8
#if LCD_DEPTH > 1
#define N_BRIGHT(y) LCD_BRIGHTNESS(y)
#else   /* LCD_DEPTH <= 1 */
#define N_BRIGHT(y) ((y > 127) ? 0 : 1)
#ifdef HAVE_NEGATIVE_LCD  /* m:robe 100, Clip */
#define PICTUREFLOW_DRMODE DRMODE_SOLID
#else
#define PICTUREFLOW_DRMODE (DRMODE_SOLID|DRMODE_INVERSEVID)
#endif
#endif  /* LCD_DEPTH <= 1 */
#define USEGSLIB
GREY_INFO_STRUCT
#define LCD_BUF _grey_info.buffer
#define G_PIX(r,g,b) \
    (77 * (unsigned)(r) + 150 * (unsigned)(g) + 29 * (unsigned)(b)) / 256
#define N_PIX(r,g,b) N_BRIGHT(G_PIX(r,g,b))
#define G_BRIGHT(y) (y)
#define BUFFER_WIDTH _grey_info.width
#define BUFFER_HEIGHT _grey_info.height
typedef unsigned char pix_t;
#else   /* LCD_DEPTH >= 8 */
#define LCD_BUF lcd_fb
#define G_PIX LCD_RGBPACK
#define N_PIX LCD_RGBPACK
#define G_BRIGHT(y) LCD_RGBPACK(y,y,y)
#define N_BRIGHT(y) LCD_RGBPACK(y,y,y)
#define BUFFER_WIDTH LCD_WIDTH
#define BUFFER_HEIGHT LCD_HEIGHT
typedef fb_data pix_t;
#endif  /* LCD_DEPTH >= 8 */

#ifdef HAVE_LCD_COLOR
static pix_t pf_bg_color;     /* theme background, replaces G_BRIGHT(0) */
static pix_t pf_fg_color;     /* theme foreground, replaces G_BRIGHT(255) */
static pix_t pf_lss_color;    /* selector start color for gradient */
static pix_t pf_lse_color;    /* selector end color for gradient */
static pix_t pf_lst_color;    /* selector text color */

/* Pre-extracted RGB565 channel masks for fade_color() performance */
static unsigned int pf_bg_rb;  /* pf_bg_color & 0xf81f */
static unsigned int pf_bg_g;   /* pf_bg_color & 0x7e0 */

static bool pf_ipod_engine_enabled(void)
{
#if defined(IPOD_VIDEO) || defined(IPOD_6G)
    return rb->global_settings->ui_engine == UI_ENGINE_IPODJS;
#else
    return false;
#endif
}

static void pf_apply_ipod_engine_colors(void)
{
    if (rb->global_settings->ui_engine_dark_mode)
    {
        pf_bg_color = (pix_t)LCD_RGBPACK(18, 20, 24);
        pf_fg_color = (pix_t)LCD_RGBPACK(239, 242, 246);
        pf_lss_color = (pix_t)LCD_RGBPACK(73, 81, 94);
        pf_lse_color = (pix_t)LCD_RGBPACK(24, 29, 38);
        pf_lst_color = (pix_t)LCD_RGBPACK(255, 255, 255);
    }
    else
    {
        pf_bg_color = (pix_t)LCD_RGBPACK(255, 255, 255);
        pf_fg_color = (pix_t)LCD_RGBPACK(0, 0, 0);
        pf_lss_color = (pix_t)LCD_RGBPACK(60, 184, 255);
        pf_lse_color = (pix_t)LCD_RGBPACK(52, 122, 181);
        pf_lst_color = (pix_t)LCD_RGBPACK(255, 255, 255);
    }
}

#ifdef HAVE_ALBUMART
static void pf_update_dynamic_colors(void)
{
    if (pf_ipod_engine_enabled())
    {
        pf_apply_ipod_engine_colors();
    }
    else
    {
        pf_bg_color =
            (pix_t)rb->dynamic_colors_resolve(rb->global_settings->bg_color);
        pf_fg_color =
            (pix_t)rb->dynamic_colors_resolve(rb->global_settings->fg_color);
        pf_lss_color =
            (pix_t)rb->dynamic_colors_resolve(rb->global_settings->lss_color);
        pf_lse_color =
            (pix_t)rb->dynamic_colors_resolve(rb->global_settings->lse_color);
        pf_lst_color =
            (pix_t)rb->dynamic_colors_resolve(rb->global_settings->lst_color);
    }
    pf_bg_rb = pf_bg_color & 0xf81f;
    pf_bg_g  = pf_bg_color & 0x7e0;
}
#endif

/* Interpolate between pf_bg_color (brightness=0) and pf_fg_color (brightness=255) */
static inline pix_t pf_color_mix(int brightness)
{
    int bg_r = RGB_UNPACK_RED(pf_bg_color);
    int bg_g = RGB_UNPACK_GREEN(pf_bg_color);
    int bg_b = RGB_UNPACK_BLUE(pf_bg_color);
    int fg_r = RGB_UNPACK_RED(pf_fg_color);
    int fg_g = RGB_UNPACK_GREEN(pf_fg_color);
    int fg_b = RGB_UNPACK_BLUE(pf_fg_color);
    return LCD_RGBPACK(
        bg_r + (fg_r - bg_r) * brightness / 255,
        bg_g + (fg_g - bg_g) * brightness / 255,
        bg_b + (fg_b - bg_b) * brightness / 255
    );
}
#endif

/* for fixed-point arithmetic, we need minimum 32-bit long
   long long (64-bit) might be useful for multiplication and division */
#define PFreal long
#define PFREAL_SHIFT 10
#define PFREAL_FACTOR (1 << PFREAL_SHIFT)
#define PFREAL_ONE (1 << PFREAL_SHIFT)
#define PFREAL_HALF (PFREAL_ONE >> 1)

#define IANGLE_MAX 1024
#define IANGLE_MASK 1023

#define REFLECT_TOP (LCD_HEIGHT * 2 / 3)
#define REFLECT_HEIGHT (LCD_HEIGHT - REFLECT_TOP)
#define DISPLAY_HEIGHT REFLECT_TOP
#define DISPLAY_WIDTH MAX((LCD_HEIGHT * LCD_PIXEL_ASPECT_HEIGHT / \
    LCD_PIXEL_ASPECT_WIDTH / 2), (LCD_WIDTH * 2 / 5))
#define REFLECT_SC ((0x10000U * 3 + (REFLECT_HEIGHT * 5 - 1)) / \
    (REFLECT_HEIGHT * 5))
#define DISPLAY_OFFS ((LCD_HEIGHT / 2) - REFLECT_HEIGHT)
#define CAM_DIST MAX(MIN(LCD_HEIGHT,LCD_WIDTH),120)
#define CAM_DIST_R (CAM_DIST << PFREAL_SHIFT)
#define DISPLAY_LEFT_R (PFREAL_HALF - LCD_WIDTH * PFREAL_HALF)
#define MAXSLIDE_LEFT_R (PFREAL_HALF - DISPLAY_WIDTH * PFREAL_HALF)

#define SLIDE_CACHE_SIZE 200

#define MAX_SLIDES_COUNT 10

#if PF_PLAYBACK_CAPABLE
/* Keep track metadata outside buflib.  The old implementation borrowed the
 * front of the live pool with buflib_buffer_out(), which moves allocations
 * without callbacks and behaves differently in the simulator. */
#define TRACK_BUFFER_MIN (16u * 1024u)
#define TRACK_BUFFER_MAX (256u * 1024u)
#endif

#define THREAD_STACK_SIZE DEFAULT_STACK_SIZE + 0x200
#define CACHE_PREFIX PLUGIN_DEMOS_DATA_DIR "/pictureflow"
#define ALBUM_INDEX CACHE_PREFIX "/pictureflow_album.idx"
#define ALBUM_INDEX_TMP ALBUM_INDEX ".tmp"

#define EV_EXIT 9999
#define EV_WAKEUP 1337

#define EMPTY_SLIDE CACHE_PREFIX "/emptyslide.pfraw"
#define EMPTY_SLIDE_BMP PLUGIN_DEMOS_DIR "/pictureflow_emptyslide.bmp"
#define SPLASH_BMP PLUGIN_DEMOS_DIR "/pictureflow_splash.bmp"
#define SPLASH_BG_BMP PLUGIN_DEMOS_DIR "/pictureflow_loading_bg.bmp"

/* Ordered Bayer dithering for 24-bit to RGB565 conversion */
#ifdef HAVE_LCD_COLOR
static const unsigned char pf_dither_table[16] =
    {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 };
#define PF_DITHERY(y) (pf_dither_table[(y) & 15] & 0xAA)
#define PF_DITHERX(x) (pf_dither_table[(x) & 15])
#define PF_DITHERXDY(x,dy) (PF_DITHERX(x) ^ dy)
#endif

/* some magic numbers for cache_version. */
#define CACHE_REBUILD   0

/* Error return values */
#define SUCCESS              0
#define ERROR_NO_ALBUMS     -1
#define ERROR_BUFFER_FULL   -2
#define ERROR_NO_ARTISTS    -3
#define ERROR_USER_ABORT    -4
#define ERROR_DATABASE      -5

/* current version for cover cache */
#define CACHE_VERSION 6
#define CONFIG_VERSION 1
#define CONFIG_FILE "pictureflow.cfg"
#define INDEX_HDR "PFID"

/** structs we use */
struct pf_config_t
{
     /* config values */
     int slide_spacing;
     int center_margin;
     int slide_tuck;

     int num_slides;
     int zoom;

     int auto_wps;
     int last_album;


     int cache_version;

     int show_album_name;
     int sort_albums_by;
     int year_sort_order;
     bool show_year;
     bool parallel_slides;

     bool resize;
     bool show_fps;
     bool show_statusbar;

     bool update_albumart;

     int scroll_speed;
     int transition_speed;

     bool text_crossfade;
     bool reduced_motion;
     char last_album_name[MAX_PATH];
     char last_album_artist[MAX_PATH];
};

struct pf_index_t {
    uint32_t            header; /*INDEX_HDR*/
    uint16_t            artist_ct;
    uint16_t            album_ct;

    char               *artist_names;
    struct artist_data *artist_index;
    size_t              artist_len;

    unsigned int        album_untagged_idx;
    char               *album_names;
    struct album_data  *album_index;
    size_t              album_len;
    long                album_untagged_seek;

    void * buf;
    size_t buf_sz;
};

struct pf_track_t {
    int    count;
    int    cur_idx;
    int    sel;
    int    sel_pulse;
    int    last_sel;
    int    list_start;
    int    list_visible;
    int    list_y;
    int    list_h;
    size_t buf_sz;
    size_t used;
    struct track_data *index;
    char  *names;
};

struct albumart_t {
    struct bitmap input_bmp;
    char pfraw_file[MAX_PATH];
    char file[MAX_PATH];
    int idx;
    int slides;
    int inspected;
    void * buf;
    size_t buf_sz;
};

struct slide_data {
    int slide_index;
    int angle;
    PFreal cx;
    PFreal cy;
    PFreal distance;
};

struct slide_cache {
    int index;      /* index of the cached slide */
    int hid;        /* handle ID of the cached slide */
    short next; /* "next" slide, with LRU last */
    short prev; /* "previous" slide */
};

struct album_data {
    int name_idx;    /* offset to the album name */
    int artist_idx;  /* offset to the artist name */
    int year;        /* album year */
    long artist_seek; /* artist taglist position */
    long seek;        /* album taglist position */
};

struct artist_data {
    int name_idx; /* offset to the artist name */
    long seek;    /* artist taglist position */
};

struct track_data {
    uint32_t sort;
    uint32_t duration;
    int name_idx;       /* offset to the track name */
    long seek;
#if PF_PLAYBACK_CAPABLE
    /* offset to the filename in the string, needed for playlist generation */
    int filename_idx;
#endif
};

struct rect {
    int left;
    int right;
    int top;
    int bottom;
};

struct load_slide_event_data {
    int slide_index;
    int cache_index;
};

struct pf_slide_cache
{
    struct slide_cache cache[SLIDE_CACHE_SIZE];
    int free;
    int used;
    int left_idx;
    int right_idx;
    int center_idx;
};

enum pf_scroll_line_type {
    PF_SCROLL_TRACK = 0,
    PF_SCROLL_ALBUM,
    PF_SCROLL_ARTIST,
    PF_MAX_SCROLL_LINES
};

struct pf_scroll_line_info {
    long ticks;         /* number of ticks between each move */
    long delay;         /* number of ticks to delay starting scrolling */
    int step;           /* pixels to move */
    long next_scroll;   /* tick of the next move */
};

struct pf_scroll_line {
    int width;          /* width of the string */
    int offset;         /* x coordinate of the string */
    int step;           /* 0 if scroll is disabled. otherwise, pixels to move */
    long start_tick;    /* tick when to start scrolling */
};

struct pfraw_header {
    uint32_t magic;
    int32_t width;          /* bmap width in pixels */
    int32_t height;         /* bmap height in pixels */
    uint32_t data_size;
};

#define PFRAW_MAGIC 0x57524650 /* "PFRW" on little-endian targets */
#define PFRAW_MISSING_MAGIC 0x4d524650 /* Explicitly verified no source art. */

enum show_album_name_values {
    ALBUM_NAME_HIDE = 0,
    ALBUM_NAME_BOTTOM,
    ALBUM_NAME_TOP,
    ALBUM_AND_ARTIST_TOP,
    ALBUM_AND_ARTIST_BOTTOM
};
static char* show_album_name_conf[] =
{
    "hide",
    "bottom",
    "top",
    "both top",
    "both bottom",
};

enum sort_albums_by_values {
    SORT_BY_ARTIST_AND_NAME = 0,
    SORT_BY_ARTIST_AND_YEAR,
    SORT_BY_YEAR,
    SORT_BY_NAME,

    SORT_VALUES_SIZE
};
static char* sort_albums_by_conf[] =
{
    "artist + name",
    "artist + year",
    "year",
    "name"
};
enum year_sort_order_values {
    ASCENDING = 0,
    DESCENDING
};
static char* year_sort_order_conf[] =
{
    "ascending",
    "descending"
};

#define MAX_SPACING 40
#define MAX_MARGIN 80

static struct albumart_t aa_cache;
static struct pf_config_t pf_cfg;

static struct configdata config[] =
{
    { TYPE_INT, 0, MAX_SPACING, { .int_p = &pf_cfg.slide_spacing }, "slide spacing",
      NULL },
    { TYPE_INT, 0, MAX_MARGIN, { .int_p = &pf_cfg.center_margin }, "center margin",
      NULL },
    { TYPE_INT, 0, DISPLAY_WIDTH / 2, { .int_p = &pf_cfg.slide_tuck }, "slide tuck",
      NULL },
    { TYPE_INT, 0, MAX_SLIDES_COUNT, { .int_p = &pf_cfg.num_slides }, "slides count",
      NULL },
    { TYPE_INT, 0, 300, { .int_p = &pf_cfg.zoom }, "zoom", NULL },
    { TYPE_BOOL, 0, 1, { .bool_p = &pf_cfg.show_fps }, "show fps", NULL },
    { TYPE_BOOL, 0, 1, { .bool_p = &pf_cfg.resize }, "resize", NULL },
    { TYPE_INT, 0, 100, { .int_p = &pf_cfg.cache_version }, "cache version", NULL },
    { TYPE_ENUM, 0, 5, { .int_p = &pf_cfg.show_album_name }, "show album name",
      show_album_name_conf },
    { TYPE_INT, 0, 2, { .int_p = &pf_cfg.auto_wps }, "auto wps", NULL },
    { TYPE_INT, 0, 999999, { .int_p = &pf_cfg.last_album }, "last album", NULL },
    { TYPE_INT, 0, 999999, { .int_p = &aa_cache.idx }, "art cache pos", NULL },
    { TYPE_INT, 0, 999999, { .int_p = &aa_cache.inspected }, "art cache inspected", NULL },
    { TYPE_ENUM, 0, 4, { .int_p = &pf_cfg.sort_albums_by }, "sort albums by",
      sort_albums_by_conf },
    { TYPE_ENUM, 0, 2, { .int_p = &pf_cfg.year_sort_order }, "year order",
      year_sort_order_conf },
    { TYPE_BOOL, 0, 1, { .bool_p = &pf_cfg.show_year }, "show year", NULL },
    { TYPE_BOOL, 0, 1, { .bool_p = &pf_cfg.parallel_slides }, "parallel slides",
      NULL },
    { TYPE_BOOL, 0, 1, { .bool_p = &pf_cfg.show_statusbar }, "show statusbar", NULL },
    { TYPE_BOOL, 0, 1, { .bool_p = &pf_cfg.update_albumart }, "update albumart", NULL },
    { TYPE_INT, 100, 400, { .int_p = &pf_cfg.scroll_speed }, "scroll speed", NULL },
    { TYPE_INT, 100, 400, { .int_p = &pf_cfg.transition_speed }, "transition speed",
      NULL },
    { TYPE_BOOL, 0, 1, { .bool_p = &pf_cfg.text_crossfade }, "text crossfade", NULL },
    { TYPE_BOOL, 0, 1, { .bool_p = &pf_cfg.reduced_motion }, "reduced motion", NULL },
    { TYPE_STRING, 0, MAX_PATH, { .string = pf_cfg.last_album_name },
      "last album name", NULL },
    { TYPE_STRING, 0, MAX_PATH, { .string = pf_cfg.last_album_artist },
      "last album artist", NULL }
};

#define CONFIG_NUM_ITEMS (sizeof(config) / sizeof(struct configdata))

/** below we allocate the memory we want to use **/

static pix_t *buffer; /* for now it always points to the lcd framebuffer */
static uint8_t reflect_table[REFLECT_HEIGHT];
static int pf_height;           /* viewport height (LCD_HEIGHT minus status bar) */
static int pf_half_height;      /* pf_height / 2 */
static int pf_lower_half;       /* pf_height - pf_half_height, rows below center */
static int pf_reflect_height;   /* pf_height - DISPLAY_HEIGHT */
static int pf_display_offs;     /* pf_half_height - pf_reflect_height */
static int pf_vp_y;             /* viewport y-offset (status bar height) */
static struct viewport pf_vp;   /* PF rendering viewport (below status bar) */
static struct frame_buffer_t pf_framebuffer; /* bypass backdrop in clear_viewport */
static struct slide_data center_slide;
static struct slide_data left_slides[MAX_SLIDES_COUNT];
static struct slide_data right_slides[MAX_SLIDES_COUNT];
static int slide_frame;
static int step;
static int target;
static int fade;
static int center_index = 0; /* index of the slide that is in the center */
static int itilt;
static PFreal offsetX;
static PFreal auto_slide_spacing;
static PFreal offsetY;
static int number_of_slides;
static bool show_tracks_while_browsing = false;

static struct pf_slide_cache pf_sldcache;

/* use long for aligning */
unsigned long thread_stack[THREAD_STACK_SIZE / sizeof(long)];
/* queue (as array) for scheduling load_surface */

static int empty_slide_hid;

unsigned int thread_id;
struct event_queue thread_q;

static struct tagcache_search tcs;
/* Main-thread-only incremental track preparation. It must not share the art
 * search object: idle artwork discovery can run between track batches. */
static struct tagcache_search track_search;
static int track_prepare_album = -1;
static int track_prepare_offset;
static long track_prepare_deadline;
static long track_prepare_retry;
static long navigation_settle_tick;
static long album_open_tick;
static long album_error_until;
static bool track_cache_dirty;
static uint32_t track_cache_generation;

static struct buflib_context buf_ctx;

static struct pf_index_t pf_idx;

static struct pf_track_t pf_tracks;

static struct mp3entry id3;

void reset_track_list(void);
static inline void free_borrowed_tracks(void);
static unsigned int mfnv(char *str);

static bool thread_is_running;
static bool wants_to_quit;

/* Serialize the shared tagcache search object and slide buflib between the
 * main thread and the background cover loader. */
static struct mutex buf_ctx_mutex;
static struct mutex artwork_io_mutex;
static bool scene_drawn;
static bool albumart_source_missing;
static int artwork_repair_album = -1;
static long artwork_repair_retry;

static int cover_animation_keyframe;
static int extra_fade;
static long scroll_animation_tick;
static long cover_animation_tick;
#if defined(HAVE_LCD_COLOR) && LCD_DEPTH == 16 && \
    LCD_STRIDEFORMAT == HORIZONTAL_STRIDE && LCD_WIDTH == 320 && LCD_HEIGHT == 240
#define PF_RETAIL_FLIP
#define PF_FACE_WIDTH 256
#define PF_FACE_STRIP 16
#define PF_PANEL_TOP 8
#define PF_PANEL_HEIGHT 212
#define PF_PANEL_HEADER 48
#define PF_TRACK_ROW 24
static fb_data face_strip[PF_FACE_WIDTH * PF_FACE_STRIP];
static int retail_flip_position;
static int retail_flip_remainder;
static int retail_close_clock;
static bool retail_flip_active;
/* Cached antialiased text coverage, not RGB frame copies. Glyph file reads
 * happen only in service_retail_text(), never in the flip compositor. */
static unsigned char retail_text[PF_FACE_WIDTH * 212 / 2];
static unsigned char retail_labels[320 * 40 / 2];
static int retail_text_album = -1, retail_text_start = -1;
static int retail_label_album = -1;
static int retail_body_font, retail_title_font, retail_detail_font;
#define PF_MARQUEE_WIDTH 2048
static unsigned char retail_marquee[PF_MARQUEE_WIDTH * PF_TRACK_ROW / 2];
static int retail_marquee_selected = -1, retail_marquee_album = -1;
static int retail_marquee_width, retail_marquee_limit;
static long retail_marquee_tick;
static void service_retail_text(void);
static void draw_retail_labels(void);
static void render_retail_tracks(void);
static uint16_t retail_header[320 * 20];
static unsigned char retail_battery_data[26 * 13 * 3];
static unsigned char retail_transport_data[2][14 * 16 * 3];
static struct ipodjs_retailos_image retail_battery;
static struct ipodjs_retailos_image retail_transport[2];
static bool retail_header_valid;
static bool retail_transport_valid;
static int retail_battery_frame = -1;

static void service_retail_battery(void)
{
    int level = rb->battery_level();
    if (level < 0)
        return; /* Unknown is not an empty battery. Keep the last valid icon. */
    int frame = level < 0 ? 0 : MIN(22, level * 22 / 100);
#if CONFIG_CHARGING
    if (rb->charger_inserted())
        frame = rb->charging_state() ? 24 : 23;
#endif
    if (frame != retail_battery_frame &&
        ipodjs_retailos_load_resource_rga(33 + frame, retail_battery_data,
            sizeof(retail_battery_data), 26, 13, &retail_battery))
        retail_battery_frame = frame;
}

static void draw_retail_header(void)
{
    struct screen *display = rb->screens[SCREEN_MAIN];
    display->set_viewport(NULL);
    display->set_drawmode(DRMODE_FG);
    if (retail_header_valid)
        ipodjs_retailos_blit_opaque(display, retail_header, 320, 0, 0, 320, 20);
    else
    {
        display->set_foreground(LCD_WHITE);
        display->fillrect(0, 0, 320, 20);
    }
    if (retail_battery_frame >= 0)
        ipodjs_retailos_blit(display, &retail_battery, 289, 3);
    int audio = rb->audio_status();
    if (retail_transport_valid && (audio & AUDIO_STATUS_PLAY))
        ipodjs_retailos_blit(display,
            &retail_transport[(audio & AUDIO_STATUS_PAUSE) ? 1 : 0], 267, 2);
    display->set_viewport(&pf_vp);
}
#endif

static struct pf_scroll_line_info scroll_line_info;
static struct pf_scroll_line scroll_lines[PF_MAX_SCROLL_LINES];

enum ePFS{ePFS_ARTIST = 0, ePFS_ALBUM};
/*
    Proposals for transitions:

    pf_idle -> pf_scrolling : NEXT_ALBUM/PREV_ALBUM pressed
            -> pf_cover_in -> pf_show_tracks : SELECT_ALBUM clicked

    pf_scrolling -> pf_idle : NEXT_ALBUM/PREV_ALBUM released

    pf_show_tracks -> pf_cover_out -> pf_idle : SELECT_ALBUM pressed

    TODO:
    pf_show_tracks -> pf_cover_out -> pf_idle : MENU_PRESSED pressed
    pf_show_tracks -> play_track() -> exit() : SELECT_ALBUM pressed

    pf_idle, pf_scrolling -> show_menu(): MENU_PRESSED
*/
enum pf_states {
    pf_idle = 0,
    pf_scrolling,
    pf_cover_in,
    pf_show_tracks,
    pf_cover_out,
    pf_open_pending
};

static int pf_state;

#ifdef SIMULATOR
struct pf_trace_sample
{
    long tick;
    int state, album, position, audio, playlist, shown;
    unsigned long elapsed;
};
/* Simulator-only: enough complete frames for the 100-cycle playback gate. */
static struct pf_trace_sample pf_trace_samples[4096];
static unsigned pf_trace_count;
static bool pf_trace_enabled;
static unsigned pf_track_cache_hits;
static unsigned pf_scene_waits;
static void trace_frame(void)
{
    if (!pf_trace_enabled)
        return;
    struct pf_trace_sample *sample =
        &pf_trace_samples[pf_trace_count++ % ARRAYLEN(pf_trace_samples)];
    sample->tick = *rb->current_tick;
    sample->state = pf_state;
    sample->album = center_index;
    sample->position = 0;
#ifdef PF_RETAIL_FLIP
    sample->position = retail_flip_position;
#endif
    sample->audio = rb->audio_status();
    sample->playlist = rb->playlist_amount();
    sample->shown = scene_drawn;
    struct mp3entry *track = rb->audio_current_track();
    sample->elapsed = track ? track->elapsed : 0;
}

static void flush_trace(void)
{
    if (!pf_trace_enabled)
        return;
    int fd = rb->open(ROCKBOX_DIR "/pictureflow-trace.tsv",
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "# cache_hits=%u scene_waits=%u samples=%u\n",
                 pf_track_cache_hits, pf_scene_waits, pf_trace_count);
    rb->fdprintf(fd, "tick\tstate\talbum\tposition\taudio\tplaylist\tshown\telapsed\n");
    unsigned start = pf_trace_count > ARRAYLEN(pf_trace_samples) ?
                     pf_trace_count - ARRAYLEN(pf_trace_samples) : 0;
    for (unsigned i = start; i < pf_trace_count; i++)
    {
        struct pf_trace_sample *s =
            &pf_trace_samples[i % ARRAYLEN(pf_trace_samples)];
        rb->fdprintf(fd, "%ld\t%d\t%d\t%d\t%d\t%d\t%d\t%lu\n",
            s->tick, s->state, s->album, s->position, s->audio, s->playlist,
            s->shown, s->elapsed);
    }
    rb->close(fd);
}
#endif

#if PF_PLAYBACK_CAPABLE
static bool insert_whole_album;
static bool old_shuffle = false;
static int old_playlist = -1;
#endif

/** code */
static bool free_slide_prio(int prio);
bool load_new_slide(void);
int load_surface(int);
static void draw_progressbar(int step, int count, char *msg);
static void draw_splashscreen(unsigned char * buf_tmp, size_t buf_tmp_size);
static void free_all_slide_prio(int prio);

static inline void buf_ctx_lock(void)
{
    rb->mutex_lock(&buf_ctx_mutex);
}

static inline void buf_ctx_unlock(void)
{
    rb->mutex_unlock(&buf_ctx_mutex);
}

static inline bool storage_mode_is_ssd(void)
{
#ifdef HAVE_DISK_STORAGE
    return rb->global_settings->storage_mode == 2;
#else
    return false;
#endif
}

static char database_error[80] = "Database unavailable";

static bool check_database(void)
{
    const long deadline = *rb->current_tick + 10 * HZ;
    long next_revalidate = *rb->current_tick;
    bool warned = false;
    int revalidate_tries = 0;

    while (true)
    {
        struct tagcache_stat *stat = rb->tagcache_get_stat();

        if (stat->initialized && stat->ready)
            return true;

        /* A failed background header open can leave ready false even though
         * the complete on-disk database is still valid. Revalidate only while
         * tagcache is otherwise idle; tagcache_commit_finalize() serializes
         * this header pass against database writers. */
        if (stat->initialized && !stat->ready &&
            stat->commit_step == 0 &&
            (stat->scan_status == TAGCACHE_SCAN_IDLE ||
             stat->scan_status == TAGCACHE_SCAN_UP_TO_DATE) &&
            revalidate_tries < 3 &&
            !TIME_BEFORE(*rb->current_tick, next_revalidate))
        {
            revalidate_tries++;
            rb->tagcache_commit_finalize();
            next_revalidate = *rb->current_tick + HZ;
            stat = rb->tagcache_get_stat();
            if (stat->initialized && stat->ready)
                return true;
        }

        if (!warned &&
            !TIME_BEFORE(*rb->current_tick, deadline - 9 * HZ))
        {
            warned = true;
            rb->splash(0, ID2P(LANG_TAGCACHE_BUSY));
        }

        if (!TIME_BEFORE(*rb->current_tick, deadline))
        {
            rb->snprintf(database_error, sizeof(database_error),
                         "Database unavailable\nstate:%d step:%d valid:%d",
                         stat->scan_status, stat->commit_step,
                         stat->readyvalid ? 1 : 0);
            return false;
        }

        rb->sleep(HZ/10);
        rb->yield();
    }
}

/* Album changes can coincide with a background database update or an audio
 * refill. Do not turn one failed open into an empty album; give the core a
 * short opportunity to finish and use the same safe idle-state recovery. */
static bool start_tagcache_search(struct tagcache_search *tcs, int tag)
{
    const long deadline = *rb->current_tick + 2 * HZ;
    bool revalidated = false;

    do
    {
        if (rb->tagcache_search(tcs, tag))
            return true;

        struct tagcache_stat *stat = rb->tagcache_get_stat();
        if (!revalidated && stat->initialized && !stat->ready &&
            stat->commit_step == 0 &&
            (stat->scan_status == TAGCACHE_SCAN_IDLE ||
             stat->scan_status == TAGCACHE_SCAN_UP_TO_DATE))
        {
            revalidated = true;
            rb->tagcache_commit_finalize();
        }

        rb->sleep(HZ/10);
        rb->yield();
    }
    while (TIME_BEFORE(*rb->current_tick, deadline));

    return false;
}

static bool progress_cancel(int step, int count, char *msg)
{
    const struct text_message prompt = {
            (const char*[]) {"Quit?", "Progress will be lost"}, 2};

    int action = rb->get_action(CONTEXT_STD,TIMEOUT_NOBLOCK);
    if (action == ACTION_STD_CANCEL || action == ACTION_STD_MENU)
    {
        if (rb->gui_syncyesno_run(&prompt, NULL, NULL) == YESNO_YES)
            return true;
        rb->lcd_clear_display();
    }
    else
        msg = NULL;

    if (count)
        draw_progressbar(step, count, msg);

    return false;
}

static void config_save(int cache_version, bool update_albumart)
{
    pf_cfg.cache_version = cache_version;
    pf_cfg.update_albumart = update_albumart;
    configfile_save(CONFIG_FILE, config, CONFIG_NUM_ITEMS, CONFIG_VERSION);
}

static void config_set_defaults(struct pf_config_t *cfg)
{
     cfg->slide_spacing = DISPLAY_WIDTH / 4;  /* kept for config compat */
     cfg->center_margin = 0;
     cfg->slide_tuck = DISPLAY_WIDTH / 4;
     cfg->num_slides = 4;   /* 3 visible + 1 animation buffer per side */
     cfg->zoom = 100;
     cfg->show_fps = false;
     cfg->auto_wps = 0;
     cfg->last_album = 0;
     cfg->last_album_name[0] = '\0';
     cfg->last_album_artist[0] = '\0';

     cfg->resize = true;
     cfg->cache_version = CACHE_REBUILD;
     cfg->show_album_name = (LCD_HEIGHT > 100)
        ? ALBUM_AND_ARTIST_BOTTOM : ALBUM_NAME_BOTTOM;
     cfg->sort_albums_by = SORT_BY_ARTIST_AND_NAME;
     cfg->year_sort_order = ASCENDING;
     cfg->show_year = false;
     cfg->parallel_slides = true;
     cfg->show_statusbar = false;
     cfg->update_albumart = false;
     cfg->scroll_speed = 300;
     cfg->transition_speed = 300;
     cfg->reduced_motion = false;
     cfg->text_crossfade = true;
}

static inline PFreal fmul(PFreal a, PFreal b)
{
    return (a*b) >> PFREAL_SHIFT;
}

/**
 * This version preshifts each operand, which is useful when we know how many
 * of the least significant bits will be empty, or are worried about overflow
 * in a particular calculation
 */
static inline PFreal fmuln(PFreal a, PFreal b, int ps1, int ps2)
{
    return ((a >> ps1) * (b >> ps2)) >> (PFREAL_SHIFT - ps1 - ps2);
}

/* ARMv5+ has a clz instruction equivalent to our function.
 */
#if (defined(CPU_ARM) && (ARM_ARCH > 4))
static inline int clz(uint32_t v)
{
    return __builtin_clz(v);
}

/* Otherwise, use our clz, which can be inlined */
#elif defined(CPU_COLDFIRE)
/* This clz is based on the log2(n) implementation at
 * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog
 * A clz benchmark plugin showed this to be about 14% faster on coldfire
 * than the LUT-based version.
 */
static inline int clz(uint32_t v)
{
    int r = 32;
    if (v >= 0x10000)
    {
        v >>= 16;
        r -= 16;
    }
    if (v & 0xff00)
    {
        v >>= 8;
        r -= 8;
    }
    if (v & 0xf0)
    {
        v >>= 4;
        r -= 4;
    }
    if (v & 0xc)
    {
        v >>= 2;
        r -= 2;
    }
    if (v & 2)
    {
        v >>= 1;
        r -= 1;
    }
    r -= v;
    return r;
}
#else
static const char clz_lut[16] = { 4, 3, 2, 2, 1, 1, 1, 1,
                                  0, 0, 0, 0, 0, 0, 0, 0 };
/* This clz is based on the log2(n) implementation at
 * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogLookup
 * It is not any faster than the one above, but trades 16B in the lookup table
 * for a savings of 12B per each inlined call.
 */
static inline int clz(uint32_t v)
{
    int r = 28;
    if (v >= 0x10000)
    {
        v >>= 16;
        r -= 16;
    }
    if (v & 0xff00)
    {
        v >>= 8;
        r -= 8;
    }
    if (v & 0xf0)
    {
        v >>= 4;
        r -= 4;
    }
    return r + clz_lut[v];
}
#endif

/* Return the maximum possible left shift for a signed int32, without
 * overflow
 */
static inline int allowed_shift(int32_t val)
{
    uint32_t uval = val ^ (val >> 31);
    return clz(uval) - 1;
}

/* Calculate num/den, with the result shifted left by PFREAL_SHIFT, by shifting
 * num and den before dividing.
 */
static inline PFreal fdiv(PFreal num, PFreal den)
{
    int shift = allowed_shift(num);
    shift = MIN(PFREAL_SHIFT, shift);
    num <<= shift;
    den >>= PFREAL_SHIFT - shift;
    return num / den;
}

#define fmin(a,b) (((a) < (b)) ? (a) : (b))
#define fmax(a,b) (((a) > (b)) ? (a) : (b))
#define fabs(a) (a < 0 ? -a : a)
#define fbound(min,val,max) (fmax((min),fmin((max),(val))))

#define MULUQ(a, b) ((a) * (b))

#if 0
#define fmul(a,b) ( ((a)*(b)) >> PFREAL_SHIFT )
#define fdiv(n,m) ( ((n)<< PFREAL_SHIFT ) / m )

#define fconv(a, q1, q2) (((q2)>(q1)) ? (a)<<((q2)-(q1)) : (a)>>((q1)-(q2)))
#define tofloat(a, q) ( (float)(a) / (float)(1<<(q)) )

static inline PFreal fmul(PFreal a, PFreal b)
{
    return (a*b) >> PFREAL_SHIFT;
}

static inline PFreal fdiv(PFreal n, PFreal m)
{
    return (n<<(PFREAL_SHIFT))/m;
}
#endif

/* warning: regenerate the table if IANGLE_MAX and PFREAL_SHIFT are changed! */
static const short sin_tab[] = {
        0,   100,   200,   297,   392,   483,   569,   650,
      724,   792,   851,   903,   946,   980,  1004,  1019,
     1024,  1019,  1004,   980,   946,   903,   851,   792,
      724,   650,   569,   483,   392,   297,   200,   100,
        0,  -100,  -200,  -297,  -392,  -483,  -569,  -650,
     -724,  -792,  -851,  -903,  -946,  -980, -1004, -1019,
    -1024, -1019, -1004,  -980,  -946,  -903,  -851,  -792,
     -724,  -650,  -569,  -483,  -392,  -297,  -200,  -100,
        0
};

static inline PFreal fsin(int iangle)
{
    iangle &= IANGLE_MASK;

    int i = (iangle >> 4);
    PFreal p = sin_tab[i];
    PFreal q = sin_tab[(i+1)];
    PFreal g = (q - p);
    return p + g * (iangle-i*16)/16;
}

static inline PFreal fcos(int iangle)
{
    return fsin(iangle + (IANGLE_MAX >> 2));
}

/* scales the 8bit subpixel value to native lcd format, indicated by bits */
static inline unsigned scale_subpixel_lcd(unsigned val, unsigned bits)
{
    (void) bits;
#if LCD_PIXELFORMAT != RGB888
    val = val * ((1 << bits) - 1);
    val = ((val >> 8) + val + 128) >> 8;
#endif
    return val;
}

static void output_row_8_transposed(uint32_t row, void * row_in,
                                       struct scaler_context *ctx)
{
    pix_t *dest = (pix_t*)ctx->bm->data + row;
    pix_t *end = dest + ctx->bm->height * ctx->bm->width;
#ifdef USEGSLIB
    uint8_t *qp = (uint8_t*)row_in;
    for (; dest < end; dest += ctx->bm->height)
        *dest = *qp++;
#else
    struct uint8_rgb *qp = (struct uint8_rgb*)row_in;
    unsigned r, g, b;
    int col = 0;
    uint8_t dy = PF_DITHERY(row);
    for (; dest < end; dest += ctx->bm->height, col++)
    {
        int delta = ctx->dither ? PF_DITHERXDY(col, dy) : 127;
        r = (31 * qp->red + (qp->red >> 3) + delta) >> 8;
        g = (63 * qp->green + (qp->green >> 2) + delta) >> 8;
        b = (31 * qp->blue + (qp->blue >> 3) + delta) >> 8;
        qp++;
        *dest = FB_RGBPACK_LCD(r, g, b);
    }
#endif
}

/* read_image_file() is called without FORMAT_TRANSPARENT so
 * it's safe to ignore alpha channel in the next two functions */
static void output_row_32_transposed(uint32_t row, void * row_in,
                                       struct scaler_context *ctx)
{
    pix_t *dest = (pix_t*)ctx->bm->data + row;
    pix_t *end = dest + ctx->bm->height * ctx->bm->width;
#ifdef USEGSLIB
    uint32_t *qp = (uint32_t*)row_in;
    for (; dest < end; dest += ctx->bm->height)
        *dest = SC_OUT(*qp++, ctx);
#else
    struct uint32_argb *qp = (struct uint32_argb*)row_in;
    int r, g, b;
    int col = 0;
    uint8_t dy = PF_DITHERY(row);
    for (; dest < end; dest += ctx->bm->height, col++)
    {
        int delta = ctx->dither ? PF_DITHERXDY(col, dy) : 127;
        r = SC_OUT(qp->r, ctx);
        g = SC_OUT(qp->g, ctx);
        b = SC_OUT(qp->b, ctx);
        qp++;
        r = (31 * r + (r >> 3) + delta) >> 8;
        g = (63 * g + (g >> 2) + delta) >> 8;
        b = (31 * b + (b >> 3) + delta) >> 8;
        *dest = FB_RGBPACK_LCD(r, g, b);
    }
#endif
}

#ifdef HAVE_LCD_COLOR
static void output_row_32_transposed_fromyuv(uint32_t row, void * row_in,
                                       struct scaler_context *ctx)
{
    pix_t *dest = (pix_t*)ctx->bm->data + row;
    pix_t *end = dest + ctx->bm->height * ctx->bm->width;
    struct uint32_argb *qp = (struct uint32_argb*)row_in;
    int col = 0;
    uint8_t dy = PF_DITHERY(row);
    for (; dest < end; dest += ctx->bm->height, col++)
    {
        unsigned r, g, b, y, u, v;
        int delta = ctx->dither ? PF_DITHERXDY(col, dy) : 127;
        y = SC_OUT(qp->b, ctx);
        u = SC_OUT(qp->g, ctx);
        v = SC_OUT(qp->r, ctx);
        qp++;
        yuv_to_rgb(y, u, v, &r, &g, &b);
        r = (31 * r + (r >> 3) + delta) >> 8;
        g = (63 * g + (g >> 2) + delta) >> 8;
        b = (31 * b + (b >> 3) + delta) >> 8;
        *dest = FB_RGBPACK_LCD(r, g, b);
    }
}
#endif

static unsigned int get_size(struct bitmap *bm)
{
    return bm->width * bm->height * sizeof(pix_t);
}

const struct custom_format format_transposed = {
    .output_row_8 = output_row_8_transposed,
#ifdef HAVE_LCD_COLOR
    .output_row_32 = {
        output_row_32_transposed,
        output_row_32_transposed_fromyuv
    },
#else
    .output_row_32 = output_row_32_transposed,
#endif
    .get_size = get_size
};

static const struct button_mapping* get_context_map(int context)
{
    context &= ~CONTEXT_LOCKED;
    return pf_contexts[context & ~CONTEXT_PLUGIN];
}

/* scrolling */
static void init_scroll_lines(void)
{
    int i;
    static const char scroll_tick_table[16] = {
     /* Hz values:
        1, 1.25, 1.55, 2, 2.5, 3.12, 4, 5, 6.25, 8.33, 10, 12.5, 16.7, 20, 25, 33 */
        100, 80, 64, 50, 40, 32, 25, 20, 16, 12, 10, 8, 6, 5, 4, 3
    };

    scroll_line_info.ticks = scroll_tick_table[rb->global_settings->scroll_speed];
    scroll_line_info.step = rb->global_settings->scroll_step;
    scroll_line_info.delay = rb->global_settings->scroll_delay / (HZ / 10);
    scroll_line_info.next_scroll = *rb->current_tick;
    for (i = 0; i < PF_MAX_SCROLL_LINES; i++)
        scroll_lines[i].step = 0;
}

static void set_scroll_line(const char *str, enum pf_scroll_line_type type)
{
    struct pf_scroll_line *s = &scroll_lines[type];
    s->width = mylcd_getstringsize(str, NULL, NULL);
    s->step = 0;
    s->offset = 0;
    s->start_tick = *rb->current_tick + scroll_line_info.delay;
    if (LCD_WIDTH - s->width < 0)
        s->step = scroll_line_info.step;
    else
        s->offset = (LCD_WIDTH - s->width) / 2;
}

static int get_scroll_line_offset(enum pf_scroll_line_type type)
{
    return scroll_lines[type].offset;
}

static void update_scroll_lines(void)
{
    int i;

    if (TIME_BEFORE(*rb->current_tick, scroll_line_info.next_scroll))
        return;

    scroll_line_info.next_scroll = *rb->current_tick + scroll_line_info.ticks;

    for (i = 0; i < PF_MAX_SCROLL_LINES; i++)
    {
        struct pf_scroll_line *s = &scroll_lines[i];
        if (s->step && TIME_BEFORE(s->start_tick, *rb->current_tick))
        {
            s->offset -= s->step;

            if (s->offset >= 0) {
                /* at beginning of line */
                s->offset = 0;
                s->step = scroll_line_info.step;
                s->start_tick = *rb->current_tick + scroll_line_info.delay * 2;
            }
            if (s->offset <= LCD_WIDTH - s->width) {
                /* at end of line */
                s->offset = LCD_WIDTH - s->width;
                s->step = -scroll_line_info.step;
                s->start_tick = *rb->current_tick + scroll_line_info.delay * 2;
            }
        }
    }
}

/* Create the lookup table with the scaling values for the reflections */
static void init_reflect_table(void)
{
    int i;
    for (i = 0; i < pf_reflect_height; i++)
    {
        reflect_table[i] =
            (768 * (pf_reflect_height - i) + (5 * pf_reflect_height / 2)) /
            (5 * pf_reflect_height);
#ifdef PF_RETAIL_FLIP
        if (pf_ipod_engine_enabled())
        {
            int remaining = MAX(0, 48 - i);
            reflect_table[i] = 96 * remaining * remaining / (48 * 48);
        }
#endif
    }
}


static int compare_albums (const void *a_v, const void *b_v)
{
    uint32_t artist_a = ((struct album_data *)a_v)->artist_idx;
    uint32_t artist_b = ((struct album_data *)b_v)->artist_idx;

    uint32_t album_a = ((struct album_data *)a_v)->name_idx;
    uint32_t album_b = ((struct album_data *)b_v)->name_idx;

    int year_a = ((struct album_data *)a_v)->year;
    int year_b = ((struct album_data *)b_v)->year;

    switch (pf_cfg.sort_albums_by)
    {
        case SORT_BY_ARTIST_AND_NAME:
            if (artist_a - artist_b == 0)
                return (int)(album_a - album_b);
            break;
        case SORT_BY_ARTIST_AND_YEAR:
            if (artist_a - artist_b == 0)
            {
                if (pf_cfg.year_sort_order == ASCENDING)
                    return year_a - year_b;
                else
                    return year_b - year_a;
            }
            break;
        case SORT_BY_YEAR:
            if (year_a - year_b != 0)
            {
                if (pf_cfg.year_sort_order == ASCENDING)
                    return year_a - year_b;
                else
                    return year_b - year_a;
            }
            break;
        case SORT_BY_NAME:
            if (album_a - album_b != 0)
                return (int)(album_a - album_b);
            break;
    }

    return (int)(artist_a - artist_b);
}

static int compare_album_artists (const void *a_v, const void *b_v)
{
    uint32_t a = ((struct album_data *)a_v)->artist_idx;
    uint32_t b = ((struct album_data *)b_v)->artist_idx;
    return (int)(a - b);
}

static void write_album_index(int idx, int name_idx,
                              long album_seek, int artist_idx, long artist_seek)
{
    pf_idx.album_index[idx].name_idx = name_idx;
    pf_idx.album_index[idx].seek = album_seek;
    pf_idx.album_index[idx].artist_idx = artist_idx;
    pf_idx.album_index[idx].artist_seek = artist_seek;
    pf_idx.album_index[idx].year = 0;
}

static inline void write_album_entry(struct tagcache_search *tcs,
                                     int name_idx, unsigned int len)
{
    write_album_index(-pf_idx.album_ct, name_idx, tcs->result_seek, 0, -1);
    pf_idx.album_len += len;
    pf_idx.album_ct++;

    if (pf_idx.album_untagged_seek == -1 && rb->strcmp(UNTAGGED, tcs->result) == 0)
    {
        pf_idx.album_untagged_idx = name_idx;
        pf_idx.album_untagged_seek = tcs->result_seek;
    }
}

static void write_artist_entry(struct tagcache_search *tcs,
                               int name_idx, unsigned int len)
{
    pf_idx.artist_index[-pf_idx.artist_ct].name_idx = name_idx;
    pf_idx.artist_index[-pf_idx.artist_ct].seek = tcs->result_seek;
    pf_idx.artist_len += len;
    pf_idx.artist_ct++;
}

/* adds tagcache_search results into artist/album index */
static int get_tcs_search_res(int type, struct tagcache_search *tcs,
                              void **buf, size_t *bufsz)
{
    char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    int ret = SUCCESS;
    unsigned int l, name_idx = 0;
    void (*writefn)(struct tagcache_search *, int, unsigned int);
    int data_size;
    if (type == ePFS_ARTIST)
    {
        writefn = &write_artist_entry;
        data_size = sizeof(struct artist_data);
    }
    else
    {
        writefn = &write_album_entry;
        data_size = sizeof(struct album_data);
    }

    while (rb->tagcache_get_next(tcs, tcs_buf, tcs_bufsz))
    {
        if (progress_cancel(0, 0, NULL))
        {
            ret = ERROR_USER_ABORT;
            break;
        }

        *bufsz -= data_size;

        l = tcs->result_len;

        if ( l > *bufsz )
        {
            /* not enough memory */
            ret = ERROR_BUFFER_FULL;
            break;
        }

        rb->strcpy(*buf, tcs->result);

        *bufsz -= l;
        *buf = l + (char *)*buf;

        writefn(tcs, name_idx, l);

        name_idx += l;
    }
    rb->tagcache_search_finish(tcs);
    return ret;
}

#define STR_STEP_INDEXING_UNTAGGED "Preparing library"
#define STR_STEP_ASSIGNING_ALBUMS "Loading albums"
#define STR_STEP_ASSIGNING_ALBUM_YEAR "Sorting releases"
#define STR_STEP_REMOVING_DUPLICATES "Tidying albums"
#define STR_STEP_PREPARING_ARTWORK "Preparing artwork"

/*adds <untagged> albums/artist to existing album index */
static int create_album_untagged(struct tagcache_search *tcs,
                                 void **buf, size_t *bufsz)
{
    static char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    int ret = SUCCESS;
    int album_count = pf_idx.album_ct; /* store existing count */
    int total_count = pf_idx.album_ct + pf_idx.artist_ct * 2;
    long seek;
    int last, final, retry;
    int i, j;
    draw_splashscreen(*buf, *bufsz);
    draw_progressbar(0, total_count, STR_STEP_INDEXING_UNTAGGED);

    /* search tagcache for all <untagged> albums & save the albumartist seek pos */
    if (start_tagcache_search(tcs, tag_albumartist))
    {
        rb->tagcache_search_add_filter(tcs, tag_album, pf_idx.album_untagged_seek);

        while (rb->tagcache_get_next(tcs, tcs_buf, tcs_bufsz))
        {
            if (progress_cancel(pf_idx.album_ct, total_count, STR_STEP_INDEXING_UNTAGGED))
            {
                rb->tagcache_search_finish(tcs);
                return ERROR_USER_ABORT;
            }

            if (tcs->result_seek ==
                pf_idx.album_index[-(pf_idx.album_ct - 1)].artist_seek)
                continue;

            if (sizeof(struct album_data) > *bufsz)
            {
                /* not enough memory */
                ret = ERROR_BUFFER_FULL;
                break;
            }

            *bufsz -= sizeof(struct album_data);
            write_album_index(-pf_idx.album_ct, pf_idx.album_untagged_idx,
                               pf_idx.album_untagged_seek, -1, tcs->result_seek);

            pf_idx.album_ct++;
        }
        rb->tagcache_search_finish(tcs);

        if (ret == SUCCESS) {
            draw_splashscreen(*buf, *bufsz);
            draw_progressbar(0, pf_idx.album_ct, STR_STEP_INDEXING_UNTAGGED);

            last = 0;
            final = pf_idx.artist_ct;
            retry = 0;

            /* map the artist_seek position to the artist name index */
            for (j = album_count; j < pf_idx.album_ct; j++)
            {
                if (progress_cancel(j, pf_idx.album_ct, STR_STEP_INDEXING_UNTAGGED))
                    return ERROR_USER_ABORT;

                seek = pf_idx.album_index[-j].artist_seek;

    retry_artist_lookup:
                retry++;
                for (i = last; i < final; i++)
                {
                    if (seek == pf_idx.artist_index[i].seek)
                    {
                        int idx = pf_idx.artist_index[i].name_idx;
                        pf_idx.album_index[-j].artist_idx = idx;
                        last = i; /* last match, start here next loop */
                        final = pf_idx.artist_ct;
                        retry = 0;
                        break;
                    }
                }
                if (retry > 0 && retry < 2)
                {
                    /* no match start back at beginning */
                    final = last;
                    last = 0;
                    goto retry_artist_lookup;
                }
            }
        }
    }
    else
        return ERROR_DATABASE;

    return ret;
}

/* Create an index of all artists from the database */
static int build_artist_index(struct tagcache_search *tcs,
                                 void **buf, size_t *bufsz)
{
    int i, res = SUCCESS;
    struct artist_data* tmp_artist;

    /* artist index starts at end of buf it will be rearranged when finalized */
    pf_idx.artist_index = ((struct artist_data *)(*bufsz + (char *) *buf)) - 1;
    pf_idx.artist_ct = 0;
    pf_idx.artist_len = 0;
    /* artist names starts at beginning of buf */
    pf_idx.artist_names = *buf;

    if (!start_tagcache_search(tcs, tag_albumartist))
        return ERROR_DATABASE;
    res = get_tcs_search_res(ePFS_ARTIST, tcs, &(*buf), bufsz);
    if (res < SUCCESS)
        return res;

    /* finalize the artist index */
    ALIGN_BUFFER(*buf, *bufsz, alignof(struct artist_data));
    tmp_artist = (struct artist_data*)*buf;
    for (i = pf_idx.artist_ct - 1; i >= 0; i--)
        tmp_artist[i] = pf_idx.artist_index[-i];

    pf_idx.artist_index = tmp_artist;
    /* move buf ptr to end of artist_index */
    *buf = pf_idx.artist_index + pf_idx.artist_ct;

    if (res == SUCCESS)
    {
        if (pf_idx.artist_ct > 0)
            res = pf_idx.artist_ct;
        else
            res = ERROR_NO_ALBUMS;
    }

    return res;
}

static int assign_album_year(void)
{
    char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    draw_progressbar(0, pf_idx.album_ct, STR_STEP_ASSIGNING_ALBUM_YEAR);
    for (int album_idx = 0; album_idx < pf_idx.album_ct; album_idx++)
    {
        /* Prevent idle poweroff */
        rb->reset_poweroff_timer();

        if (progress_cancel(album_idx, pf_idx.album_ct, STR_STEP_ASSIGNING_ALBUM_YEAR))
            return ERROR_USER_ABORT;

        int album_year = 0;

        if (start_tagcache_search(&tcs, tag_year))
        {
            rb->tagcache_search_add_filter(&tcs, tag_album,
                                       pf_idx.album_index[album_idx].seek);

            if (pf_idx.album_index[album_idx].artist_idx >= 0)
                rb->tagcache_search_add_filter(&tcs, tag_albumartist,
                    pf_idx.album_index[album_idx].artist_seek);

            while (rb->tagcache_get_next(&tcs, tcs_buf, tcs_bufsz)) {
                int track_year = rb->tagcache_get_numeric(&tcs, tag_year);
                if (track_year > album_year)
                    album_year = track_year;
            }
        }
        else
            return ERROR_DATABASE;
        rb->tagcache_search_finish(&tcs);

        pf_idx.album_index[album_idx].year = album_year;
    }
    return SUCCESS;
}

/**
  Create an index of all artists and albums from the database.
  Also store the artists and album names so we can access them later.
 */
static int create_album_index(void)
{
    static char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    void *buf = pf_idx.buf;
    size_t buf_size = pf_idx.buf_sz;

    struct album_data* tmp_album;

    int i, j, last, final, retry, res;

    draw_splashscreen(buf, buf_size);
    ALIGN_BUFFER(buf, buf_size, sizeof(long));

    /* Artists */
    res = build_artist_index(&tcs, &buf, &buf_size);
    if (res < SUCCESS)
        return res;

    /* Albums */
    pf_idx.album_ct = 0;
    pf_idx.album_len =0;
    pf_idx.album_untagged_idx = -1;
    pf_idx.album_untagged_seek = -1;

    /* album_index starts at end of buf it will be rearranged when finalized */
    pf_idx.album_index = ((struct album_data *)(buf_size + (char *)buf)) - 1;
    /* album_names starts at the beginning of buf */
    pf_idx.album_names = buf;

    if (!start_tagcache_search(&tcs, tag_album))
        return ERROR_DATABASE;
    res = get_tcs_search_res(ePFS_ALBUM, &tcs, &buf, &buf_size);
    if (res < SUCCESS)
        return res;

    /* Build artist list for untagged albums */
    res = create_album_untagged(&tcs, &buf, &buf_size);

    if (res < SUCCESS)
        return res;

    /* finalize the album index */
    ALIGN_BUFFER(buf, buf_size, alignof(struct album_data));
    tmp_album = (struct album_data*)buf;
    for (i = pf_idx.album_ct - 1; i >= 0; i--)
        tmp_album[i] = pf_idx.album_index[-i];

    pf_idx.album_index = tmp_album;
    /* move buf ptr to end of album_index */
    buf = pf_idx.album_index + pf_idx.album_ct;

    /* Assign indices */
    draw_splashscreen(buf, buf_size);
    draw_progressbar(0, pf_idx.album_ct, STR_STEP_ASSIGNING_ALBUMS);
    for (j = 0; j < pf_idx.album_ct; j++)
    {
        /* Prevent idle poweroff */
        rb->reset_poweroff_timer();

        if (progress_cancel(j, pf_idx.album_ct, STR_STEP_ASSIGNING_ALBUMS))
            return ERROR_USER_ABORT;

        if (pf_idx.album_index[j].artist_seek >= 0) { continue; }

        if (!start_tagcache_search(&tcs, tag_albumartist))
            return ERROR_DATABASE;
        rb->tagcache_search_add_filter(&tcs, tag_album, pf_idx.album_index[j].seek);

        last = 0;
        final = pf_idx.artist_ct;
        retry = 0;
        if (rb->tagcache_get_next(&tcs, tcs_buf, tcs_bufsz))
        {

retry_artist_lookup:
            retry++;
            for (i = last; i < final; i++)
            {
                if (tcs.result_seek == pf_idx.artist_index[i].seek)
                {
                    int idx = pf_idx.artist_index[i].name_idx;
                    pf_idx.album_index[j].artist_idx = idx;
                    pf_idx.album_index[j].artist_seek = tcs.result_seek;
                    last = i; /* last match, start here next loop */
                    final = pf_idx.artist_ct;
                    retry = 0;
                    break;
                }
            }
            if (retry > 0 && retry < 2)
            {
                /* no match start back at beginning */
                final = last;
                last = 0;
                goto retry_artist_lookup;
            }
        }
        rb->tagcache_search_finish(&tcs);
    }

    draw_splashscreen(buf, buf_size);

    res = assign_album_year();

    if (res < SUCCESS)
        return res;

    /* sort list order to find duplicates */
    rb->qsort(pf_idx.album_index, pf_idx.album_ct,
              sizeof(struct album_data), compare_album_artists);

    draw_splashscreen(buf, buf_size);
    draw_progressbar(0, pf_idx.album_ct, STR_STEP_REMOVING_DUPLICATES);
    /* mark duplicate albums for deletion */
    for (i = 0; i < pf_idx.album_ct - 1; i++) /* -1 don't check last entry */
    {
        /* Prevent idle poweroff */
        rb->reset_poweroff_timer();

        if (progress_cancel(i, pf_idx.album_ct, STR_STEP_REMOVING_DUPLICATES))
            return ERROR_USER_ABORT;

        int idxi = pf_idx.album_index[i].artist_idx;
        int seeki = pf_idx.album_index[i].seek;

        for (j = i + 1; j < pf_idx.album_ct; j++)
        {
            if (idxi > 0 &&
            idxi == pf_idx.album_index[j].artist_idx &&
            seeki == pf_idx.album_index[j].seek)
            {
                pf_idx.album_index[j].artist_idx = -1;
            }
            else
            {
                i = j - 1;
                break;
            }
        }
    }

    /* now fix the album list order */
    rb->qsort(pf_idx.album_index, pf_idx.album_ct,
              sizeof(struct album_data), compare_album_artists);

    /* remove any extra untagged albums
     * extra space is orphaned till restart */
    pf_idx.album_index += pf_idx.album_untagged_idx + 1;
    pf_idx.album_ct -= pf_idx.album_untagged_idx + 1;

    pf_idx.buf = buf;
    pf_idx.buf_sz = buf_size;
    pf_idx.artist_index = 0;

    rb->qsort(pf_idx.album_index, pf_idx.album_ct,
                          sizeof(struct album_data), compare_albums);

    return (pf_idx.album_ct > 0) ? 0 : ERROR_NO_ALBUMS;
}

/*Saves the album index into a binary file to be recovered the
 next time PictureFlow is launched*/

static int save_album_index(void){
    int fd = rb->open(ALBUM_INDEX_TMP, O_WRONLY|O_CREAT|O_TRUNC, 0666);

    struct pf_index_t data;
    memcpy(&data, &pf_idx, sizeof(struct pf_index_t));

    if(fd >= 0)
    {
        bool ok = true;
        rb->memcpy(&data.header, INDEX_HDR, sizeof(pf_idx.header));

        ok = rb->write(fd, &data, sizeof(struct pf_index_t)) ==
             (ssize_t)sizeof(struct pf_index_t);
        if (ok)
            ok = rb->write(fd, data.artist_names, data.artist_len) ==
                 (ssize_t)data.artist_len;
        if (ok)
            ok = rb->write(fd, data.album_names, data.album_len) ==
                 (ssize_t)data.album_len;
        size_t album_idx_sz = data.album_ct * sizeof(struct album_data);
        if (ok)
            ok = rb->write(fd, data.album_index, album_idx_sz) ==
                 (ssize_t)album_idx_sz;
        if (rb->close(fd) < 0)
            ok = false;

        if (ok)
        {
            /* FAT rename cannot replace an existing file. Removing the old
             * file only after the complete replacement is closed means a
             * reset can leave a missing index, but never a partial one. */
            rb->remove(ALBUM_INDEX);
            if (rb->rename(ALBUM_INDEX_TMP, ALBUM_INDEX) == 0)
                return 0;
        }
    }

    rb->remove(ALBUM_INDEX_TMP);
    return -1;
}

/* reads data from save file to buffer */
static inline int read2buf(int fildes, void *buf, size_t nbyte){
    int read;
    read = rb->read(fildes, buf, nbyte);
    if (read < (int)nbyte)
        return 0;

    return read;
}

/*Loads the album_index information stored in the hard drive*/
static int load_album_index(void){

    int i, fr = rb->open(ALBUM_INDEX, O_RDONLY);
    struct pf_index_t data;

    void *bufstart = pf_idx.buf;
    size_t bufstart_sz = pf_idx.buf_sz;

    void* buf = pf_idx.buf;
    size_t buf_size = pf_idx.buf_sz;

    size_t album_idx_sz;
    int album_idx, artist_idx;

    if (fr >= 0){
        const off_t stored_size = rb->filesize(fr);
        if (stored_size > (off_t)sizeof(data))
        {
            if (rb->read(fr, &data, sizeof(data)) == sizeof(data) &&
                rb->memcmp(&(data.header), INDEX_HDR, sizeof(data.header)) == 0)
            {
                if (data.album_ct == 0 || data.artist_len == 0 ||
                    data.album_len == 0 ||
                    data.artist_len > bufstart_sz ||
                    data.album_len > bufstart_sz - data.artist_len)
                    goto failure;

                album_idx_sz = data.album_ct * sizeof(struct album_data);
                size_t expected_size = sizeof(data) + data.artist_len;
                if (data.album_len > SIZE_MAX - expected_size)
                    goto failure;
                expected_size += data.album_len;
                if (album_idx_sz > SIZE_MAX - expected_size)
                    goto failure;
                expected_size += album_idx_sz;

                if (stored_size != (off_t)expected_size)
                    goto failure;

                //rb->lseek(fr, sizeof(data) + 1, SEEK_SET);
                /* artist names */
                if (read2buf(fr, buf, data.artist_len) == 0)
                    goto failure;

                data.artist_names = buf;
                buf = (char *)buf + data.artist_len;
                buf_size -= data.artist_len;

                /* album names */
                if (read2buf(fr, buf, data.album_len) == 0)
                    goto failure;

                data.album_names = buf;
                buf = (char *)buf + data.album_len;
                buf_size -= data.album_len;

                /* index of album names */
                ALIGN_BUFFER(buf, buf_size, alignof(struct album_data));
                if (album_idx_sz > buf_size)
                    goto failure;
                if (read2buf(fr, buf, album_idx_sz) == 0)
                    goto failure;

                data.album_index = buf;
                buf = (char *)buf + album_idx_sz;
                buf_size -= album_idx_sz;

                rb->close(fr);
                fr = -1;

                /* sanity check loaded data */
                for (i = 0; i < data.album_ct; i++)
                {
                    album_idx = data.album_index[i].name_idx;
                    artist_idx = data.album_index[i].artist_idx;
                    if (album_idx < 0 || album_idx >= (int)data.album_len ||
                        artist_idx < 0 || artist_idx >= (int)data.artist_len ||
                        rb->memchr(data.album_names + album_idx, '\0',
                            data.album_len - album_idx) == NULL ||
                        rb->memchr(data.artist_names + artist_idx, '\0',
                            data.artist_len - artist_idx) == NULL)
                    {
                        goto failure;
                    }
                }

                memcpy(&pf_idx, &data, sizeof(struct pf_index_t));
                pf_idx.buf = buf;
                pf_idx.buf_sz = buf_size;

                rb->qsort(pf_idx.album_index, pf_idx.album_ct,
                          sizeof(struct album_data), compare_albums);

                return 0;
            }
        }
    }

failure:
    rb->splash(HZ/2, "Failed to load index");
    if (fr >= 0)
        rb->close(fr);

    pf_idx.buf = bufstart;
    pf_idx.buf_sz = bufstart_sz;
    pf_idx.artist_ct = 0;
    pf_idx.album_ct = 0;
    return -1;

}

/**
 Return a pointer to the album name of the given slide_index
 */
static char* get_album_name(const int slide_index)
{
    char *name = pf_idx.album_names + pf_idx.album_index[slide_index].name_idx;
    return name;
}

/**
 Return a pointer to the album name of the given slide_index
 */
static char* get_album_name_idx(const int slide_index, int *idx)
{
    *idx = pf_idx.album_index[slide_index].name_idx;
    char *name = pf_idx.album_names + pf_idx.album_index[slide_index].name_idx;
    return name;
}

/**
 Return a pointer to the album artist of the given slide_index
 */
static char* get_album_artist(const int slide_index)
{
    if (slide_index < pf_idx.album_ct && slide_index >= 0){
        int idx = pf_idx.album_index[slide_index].artist_idx;
        if (idx >= 0 && idx < (int) pf_idx.artist_len) {
            char *name = pf_idx.artist_names + idx;
            return name;
        }
    }
    return "?";
}


static char* get_slide_name(const int slide_index, bool artist)
{
    if (artist)
        return get_album_artist(slide_index);

    return get_album_name(slide_index);
}

/**
 Return a pointer to the track name of the active album
 create_track_index has to be called first.
 */
static char* get_track_name(const int track_index)
{
    if (track_index >= 0 && track_index < pf_tracks.count )
        return pf_tracks.names + pf_tracks.index[track_index].name_idx;
    return 0;
}
#if PF_PLAYBACK_CAPABLE
static char* get_track_filename(const int track_index)
{
    if ( track_index < pf_tracks.count )
        return pf_tracks.names + pf_tracks.index[track_index].filename_idx;
    return 0;
}
#endif



static int jmp_idx_prev(void)
{
    if (aa_cache.inspected < pf_idx.album_ct)
    {
#ifdef USEGSLIB
        grey_show(false);
        rb->lcd_clear_display();
        rb->lcd_update();
#endif
        rb->splash(HZ*2, rb->str(LANG_WAIT_FOR_CACHE));
#ifdef USEGSLIB
        grey_show(true);
#endif
        return center_index;
    }

    if (pf_cfg.sort_albums_by == SORT_BY_YEAR)
    {
        int current_year = pf_idx.album_index[center_index].year;

        for (int i = center_index - 1; i > 0; i-- )
        {
            if(pf_idx.album_index[i].year != current_year)
                current_year = pf_idx.album_index[i].year;
            while (i > 0)
            {
                if (pf_idx.album_index[i-1].year != current_year)
                    break;
                i--;
            }
            return i;
        }
    }
    else
    {
        bool by_artist = pf_cfg.sort_albums_by != SORT_BY_NAME;
        char *current_selection = get_slide_name(center_index, by_artist);

        for (int i = center_index - 1; i > 0; i-- )
        {
            if(rb->strncmp(get_slide_name(i, by_artist), current_selection, 1))
                current_selection = get_slide_name(i, by_artist);
            while (i > 0)
            {
                if (rb->strncmp(get_slide_name(i-1, by_artist), current_selection, 1))
                    break;
                i--;
            }
            return i;
        }
    }

    return 0;
}

static int jmp_idx_next(void)
{
    if (aa_cache.inspected < pf_idx.album_ct)
    {
#ifdef USEGSLIB
        grey_show(false);
        rb->lcd_clear_display();
        rb->lcd_update();
#endif
        rb->splash(HZ*2, rb->str(LANG_WAIT_FOR_CACHE));
#ifdef USEGSLIB
        grey_show(true);
#endif
        return center_index;
    }

    if (pf_cfg.sort_albums_by == SORT_BY_YEAR)
    {
        int current_year = pf_idx.album_index[center_index].year;
        for (int i = center_index + 1; i < pf_idx.album_ct; i++ )
            if(pf_idx.album_index[i].year != current_year)
                return i;
    }
    else
    {
        bool by_artist = pf_cfg.sort_albums_by != SORT_BY_NAME;
        char *current_selection = get_slide_name(center_index, by_artist);
        for (int i = center_index + 1; i < pf_idx.album_ct; i++ )
            if(rb->strncmp(get_slide_name(i, by_artist), current_selection, 1))
                return i;
    }
    return pf_idx.album_ct - 1;
}

static int id3_get_index(struct mp3entry *id3)
{
    char* current_artist = UNTAGGED;
    char* current_album  = UNTAGGED;

    if(id3)
    {
        /* we could be looking for the artist in either field */
        if(id3->albumartist)
            current_artist = id3->albumartist;
        else if(id3->artist)
            current_artist = id3->artist;

        if (id3->album && rb->strlen(id3->album) > 0)
            current_album = id3->album;

        //rb->splashf(1000, "%s, %s", current_album, current_artist);

        int i;
        int album_idx, artist_idx;

        for (i = 0; i < pf_idx.album_ct; i++ )
        {
            album_idx = pf_idx.album_index[i].name_idx;
            artist_idx = pf_idx.album_index[i].artist_idx;

            if(!rb->strcmp(pf_idx.album_names + album_idx, current_album) &&
                !rb->strcasecmp(pf_idx.artist_names + artist_idx, current_artist))
                return i;
        }

    }
    rb->splash(HZ/2, "Album Not Found!");
    return pf_cfg.last_album;
}

/**
  Compare two unsigned ints passed via pointers.
 */
static int compare_tracks (const void *a_v, const void *b_v)
{
    uint32_t a = ((struct track_data *)a_v)->sort;
    uint32_t b = ((struct track_data *)b_v)->sort;
    return (int)(a - b);
}



static bool track_buffer_avail(size_t needed)
{
    return pf_tracks.used <= pf_tracks.buf_sz &&
           needed <= pf_tracks.buf_sz - pf_tracks.used;
}

#if PF_PLAYBACK_CAPABLE
/* Pointer-free, little-endian per-album records. Database seeks are never
 * restored from disk. The master header includes the database commit ID. */
#define PF_TRACK_MAGIC 0x31544650u
static uint32_t track_hash(uint32_t hash, const void *data, size_t length)
{
    const unsigned char *bytes = data;
    while (length--)
        hash = (hash ^ *bytes++) * 16777619u;
    return hash;
}

static uint32_t database_generation(void)
{
    char path[MAX_PATH];
    unsigned char header[24];
    struct tagcache_stat *stat = rb->tagcache_get_stat();
    if (!stat->ready || stat->scan_status == TAGCACHE_SCAN_COMMITTING)
        return 0;
    rb->snprintf(path, sizeof(path), "%s/database_idx.tcd", stat->db_path);
    int fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    bool valid = rb->read(fd, header, sizeof(header)) == sizeof(header);
    rb->close(fd);
    return valid ? track_hash(2166136261u, header, sizeof(header)) : 0;
}

static void track_cache_path(char *path, size_t size, int album)
{
    rb->snprintf(path, size, CACHE_PREFIX "/%08x-%08x.pft",
                 mfnv(get_album_name(album)), mfnv(get_album_artist(album)));
}

static void track_cache_record(uint32_t *record, struct track_data *track)
{
    record[0] = htole32(track->sort);
    record[1] = htole32(track->duration);
    record[2] = htole32(track->name_idx);
    record[3] = htole32(track->filename_idx);
}

static bool load_track_cache(int album, uint32_t generation)
{
    char path[MAX_PATH];
    char key[MAX_PATH];
    uint32_t header[8], record[4];
    track_cache_path(path, sizeof(path), album);
    int fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    bool valid = false;
    if (rb->read(fd, header, sizeof(header)) != sizeof(header))
        goto done;
    for (unsigned i = 0; i < ARRAYLEN(header); i++)
        header[i] = letoh32(header[i]);
    size_t count = header[2], names = header[3];
    if (header[0] != PF_TRACK_MAGIC || header[1] != 1 || !generation ||
        header[4] != generation || !count || !names ||
        names > pf_tracks.buf_sz ||
        count > (pf_tracks.buf_sz - names) / sizeof(struct track_data) ||
        !header[6] || header[6] > sizeof(key) ||
        !header[7] || header[7] > sizeof(key))
        goto done;
    if (rb->filesize(fd) != (off_t)(sizeof(header) + header[6] + header[7] +
                                   names + count * sizeof(record)))
        goto done;
    for (int i = 0; i < 2; i++)
    {
        size_t length = header[6 + i];
        if (rb->read(fd, key, length) != (ssize_t)length || key[length-1] ||
            rb->strlen(key) + 1 != length ||
            rb->strcmp(key, i ? get_album_artist(album) : get_album_name(album)))
            goto done;
    }
    if (rb->read(fd, pf_tracks.names, names) != (ssize_t)names)
        goto done;
    uint32_t hash = track_hash(2166136261u, pf_tracks.names, names);
    pf_tracks.index = (struct track_data *)(pf_tracks.names + pf_tracks.buf_sz -
                                           count * sizeof(struct track_data));
    for (size_t i = 0; i < count; i++)
    {
        if (rb->read(fd, record, sizeof(record)) != sizeof(record))
            goto done;
        hash = track_hash(hash, record, sizeof(record));
        struct track_data *track = &pf_tracks.index[i];
        track->sort = letoh32(record[0]);
        track->duration = letoh32(record[1]);
        unsigned title = letoh32(record[2]);
        unsigned filename = letoh32(record[3]);
        if (title >= names || filename >= names ||
            !rb->memchr(pf_tracks.names + title, 0, names - title) ||
            !rb->memchr(pf_tracks.names + filename, 0, names - filename))
            goto done;
        track->name_idx = title;
        track->filename_idx = filename;
        track->seek = -1;
    }
    if (hash != header[5] || database_generation() != generation)
        goto done;
    pf_tracks.used = names + count * sizeof(struct track_data);
    pf_tracks.count = count;
    pf_tracks.cur_idx = album;
    reset_track_list();
    valid = true;
#ifdef SIMULATOR
    pf_track_cache_hits++;
#endif
done:
    rb->close(fd);
    return valid;
}

static void save_track_cache(void)
{
    int album = pf_tracks.cur_idx;
    if (!track_cache_dirty || album < 0)
        return;
    track_cache_dirty = false;
    if (!track_cache_generation ||
        database_generation() != track_cache_generation)
        return;
    const char *name = get_album_name(album);
    const char *artist = get_album_artist(album);
    size_t name_len = rb->strlen(name) + 1, artist_len = rb->strlen(artist) + 1;
    if (name_len > MAX_PATH || artist_len > MAX_PATH)
        return;
    size_t names = pf_tracks.used - pf_tracks.count * sizeof(struct track_data);
    uint32_t hash = track_hash(2166136261u, pf_tracks.names, names);
    uint32_t record[4];
    for (int i = 0; i < pf_tracks.count; i++)
    {
        track_cache_record(record, &pf_tracks.index[i]);
        hash = track_hash(hash, record, sizeof(record));
    }
    uint32_t header[8] = {PF_TRACK_MAGIC, 1, pf_tracks.count, names,
                         track_cache_generation, hash, name_len, artist_len};
    for (unsigned i = 0; i < ARRAYLEN(header); i++)
        header[i] = htole32(header[i]);
    char path[MAX_PATH], temporary[MAX_PATH];
    track_cache_path(path, sizeof(path), album);
    rb->snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    int fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    bool ok = rb->write(fd, header, sizeof(header)) == sizeof(header) &&
              rb->write(fd, name, name_len) == (ssize_t)name_len &&
              rb->write(fd, artist, artist_len) == (ssize_t)artist_len &&
              rb->write(fd, pf_tracks.names, names) == (ssize_t)names;
    for (int i = 0; ok && i < pf_tracks.count; i++)
    {
        track_cache_record(record, &pf_tracks.index[i]);
        ok = rb->write(fd, record, sizeof(record)) == sizeof(record);
    }
    ok = rb->close(fd) == 0 && ok;
    if (!ok || database_generation() != track_cache_generation ||
        rb->rename(temporary, path) < 0)
        rb->remove(temporary);
}
#endif


static int pf_tcs_retrieve_track_title(int string_index)
{
    char file_name[MAX_PATH];
    char *track_title = NULL;
    int str_len;

    if (rb->strcmp(UNTAGGED, track_search.result) == 0)
    {
        /* show filename instead of <untaggged> */
        if (!rb->tagcache_retrieve(&track_search, track_search.idx_id,
                                tag_virt_basename,
                                file_name, MAX_PATH))
            return 0;
        track_title = file_name;
    }

    if (!track_title)
        track_title = track_search.result;

    int max_len = rb->strlen(track_title) + 10;
    if (!track_buffer_avail(max_len))
        return 0;

    /* iPod Classic 6G custom: title only, no disc/track number prefix */
    str_len = rb->snprintf(pf_tracks.names + string_index, max_len,
        "%s", track_title);
    return str_len;
}

#if PF_PLAYBACK_CAPABLE
static int pf_tcs_retrieve_file_name(int fn_idx)
{
    if (!track_buffer_avail(MAX_PATH))
        return 0;

    char *filename = pf_tracks.names + fn_idx;
    filename[0] = '\0';
    if (!rb->tagcache_retrieve(&track_search, track_search.idx_id, tag_filename,
                               filename, MAX_PATH))
        return 0;

    return rb->strlen(filename);
}
#endif

/**
  Create the track index of the given slide_index.
 */
/* Prepare at most four rows per service call. No searches or track-file reads
 * belong in the track-list renderer or the cover animation. */
static bool prepare_track_index(const int slide_index)
{
    char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    if ( slide_index == pf_tracks.cur_idx )
        return true;

    struct tagcache_stat *stat = rb->tagcache_get_stat();
    if (!stat->ready || stat->scan_status == TAGCACHE_SCAN_COMMITTING ||
        stat->commit_step != 0)
    {
        free_borrowed_tracks();
        return false;
    }

    if (track_prepare_album != slide_index)
    {
        free_borrowed_tracks();
        if (TIME_BEFORE(*rb->current_tick, track_prepare_retry))
            return false;
#if PF_PLAYBACK_CAPABLE
        track_cache_generation = database_generation();
        if (load_track_cache(slide_index, track_cache_generation))
            return true;
#endif
        /* Foreground readiness is bounded; do not run the synchronous
         * tagcache recovery loop for speculative metadata. */
        if (!rb->tagcache_search(&track_search, tag_title))
        {
            track_prepare_retry = *rb->current_tick + HZ;
            return false;
        }
        track_prepare_album = slide_index;
        track_prepare_offset = 0;
        track_prepare_deadline = *rb->current_tick + 2 * HZ;
        rb->tagcache_search_add_filter(&track_search, tag_album,
                                      pf_idx.album_index[slide_index].seek);
        if (pf_idx.album_index[slide_index].artist_idx >= 0)
            rb->tagcache_search_add_filter(&track_search, tag_albumartist,
                pf_idx.album_index[slide_index].artist_seek);
    }

    if (TIME_AFTER(*rb->current_tick, track_prepare_deadline))
        goto fail;

    int string_index = track_prepare_offset;
    for (int batch = 0; batch < 4; batch++)
    {
        if (!rb->tagcache_get_next(&track_search, tcs_buf, tcs_bufsz))
            goto complete;
        int disc_num = rb->tagcache_get_numeric(&track_search, tag_discnumber);
        int track_num = rb->tagcache_get_numeric(&track_search, tag_tracknumber);
        disc_num = disc_num > 0 ? disc_num : 0;
        track_num = track_num > 0 ? track_num : 0;
        int fn_idx = 1 + pf_tcs_retrieve_track_title(string_index);
        if (fn_idx <= 1)
            goto fail;
        pf_tracks.used += fn_idx;

#if PF_PLAYBACK_CAPABLE
        int fn_len = 1 + pf_tcs_retrieve_file_name(string_index + fn_idx);
        if (fn_len <= 1)
            goto fail;
        pf_tracks.used += fn_len;
#endif
        if (!track_buffer_avail(sizeof(struct track_data)))
            goto fail;

        pf_tracks.used += sizeof(struct track_data);
        unsigned int arr_sz = (pf_tracks.count + 1) * sizeof(struct track_data);
        /* Array descends from the upper end of the fixed track arena. */
        pf_tracks.index = (struct track_data*)(pf_tracks.names + pf_tracks.buf_sz
                                                               - arr_sz );
        pf_tracks.index->sort = (disc_num << 24) + (track_num << 14);
        pf_tracks.index->duration =
            rb->tagcache_get_numeric(&track_search, tag_length) / 1000;
        pf_tracks.index->sort += pf_tracks.count;
        pf_tracks.index->name_idx = string_index;
        pf_tracks.index->seek = track_search.result_seek;
#if PF_PLAYBACK_CAPABLE
        pf_tracks.index->filename_idx = fn_idx + string_index;
        string_index += (fn_idx + fn_len);
#else
        string_index += fn_idx;
#endif
        pf_tracks.count++;
    }
    track_prepare_offset = string_index;
    return false;

complete:
    rb->tagcache_search_finish(&track_search);
    track_prepare_album = -1;

    if (pf_tracks.count == 0)
        goto fail;

    /* now fix the track list order */
    rb->qsort(pf_tracks.index, pf_tracks.count,
              sizeof(struct track_data), compare_tracks);

    pf_tracks.cur_idx = slide_index;
    track_cache_dirty = true;
    reset_track_list();
    return true;
fail:
    free_borrowed_tracks();
    track_prepare_retry = *rb->current_tick + HZ;
    return false;
}

/* Explicit context/playback operations may synchronously finish preparation;
 * normal browsing and opening use one bounded batch per event-loop turn. */
static void create_track_index(const int slide_index)
{
    while (!prepare_track_index(slide_index) && track_prepare_album >= 0)
        rb->yield();
}

/**
  Clear the fixed track arena for the next album.
*/
static inline void free_borrowed_tracks(void)
{
    track_cache_dirty = false;
    if (track_prepare_album >= 0)
    {
        rb->tagcache_search_finish(&track_search);
        track_prepare_album = -1;
    }
    pf_tracks.count = 0;
    pf_tracks.used = 0;
    pf_tracks.cur_idx = -1;
}

/* Fills mp3entry with metadata retrieved from  RAM, if possible, or by reading from
 * the file directly.  Note that the tagcache only stores a subset of metadata and
 * will thus not return certain properties of the file, such as frequency, size, or
 * codec.
 */
bool retrieve_id3(struct mp3entry *id3, const char* file)
{
#if defined (HAVE_TAGCACHE) && defined(HAVE_TC_RAMCACHE) && defined(HAVE_DIRCACHE)
    if (rb->tagcache_fill_tags(id3, file))
    {
        rb->strlcpy(id3->path, file, sizeof(id3->path));
        return true;
    }
#endif

    return rb->get_metadata(id3, -1, file);
}

/**
  Determine filename of the album art for the given slide_index and
  store the result in buf.
  The algorithm looks for the first track of the given album uses
  find_albumart to find the filename.
 */
static bool get_albumart_for_index_from_db(const int slide_index, char *buf,
                                    int buflen)
{
    bool ret;
    char tcs_buf[TAGCACHE_BUFSZ];
    const long tcs_bufsz = sizeof(tcs_buf);
    albumart_source_missing = false;
    if (tcs.valid || !start_tagcache_search(&tcs, tag_filename))
        return false;

    /* find the first track of the album */
    rb->tagcache_search_add_filter(&tcs, tag_album,
                                   pf_idx.album_index[slide_index].seek);

    rb->tagcache_search_add_filter(&tcs, tag_albumartist,
                                   pf_idx.album_index[slide_index].artist_seek);

    ret = rb->tagcache_get_next(&tcs, tcs_buf, tcs_bufsz) &&
          retrieve_id3(&id3, tcs.result);
    if (ret)
    {
        ret = search_albumart_files(&id3, ":", buf, buflen);
        albumart_source_missing = !ret;
    }

    rb->tagcache_search_finish(&tcs);
    return ret;
}

static bool draw_splash_albumart(unsigned char *buf_tmp, size_t buf_tmp_size,
                                 int slide_index, int x, int y, int size)
{
    struct screen* display = rb->screens[SCREEN_MAIN];
    struct bitmap cover;
    char path[MAX_PATH];
    int ret;
    const unsigned int format = FORMAT_NATIVE | FORMAT_RESIZE |
                                FORMAT_KEEP_ASPECT | FORMAT_DITHER;

    if (slide_index < 0 || slide_index >= pf_idx.album_ct ||
        !pf_idx.album_index || size <= 0)
        return false;

    if (!get_albumart_for_index_from_db(slide_index, path, sizeof(path)))
        return false;

    cover.width = size;
    cover.height = size;
    cover.format = FORMAT_NATIVE;
    cover.data = buf_tmp;

    ret = read_image_file(path, &cover, buf_tmp_size, format, NULL);
    if (ret <= 0)
        return false;

    rb->lcd_set_foreground(LCD_RGBPACK(52, 55, 60));
    rb->lcd_fillrect(x + 3, y + 3, size, size);
    rb->lcd_set_foreground(LCD_RGBPACK(248, 248, 248));
    rb->lcd_fillrect(x - 2, y - 2, size + 4, size + 4);
    display->bitmap(cover.data, x, y, cover.width, cover.height);
    rb->lcd_set_foreground(LCD_RGBPACK(118, 122, 128));
    rb->lcd_drawrect(x - 1, y - 1, size + 2, size + 2);
    return true;
}

static void draw_splash_albumart_strip(unsigned char *buf_tmp,
                                       size_t buf_tmp_size)
{
#if LCD_WIDTH >= 300 && LCD_HEIGHT >= 220
    int drawn = 0;

    if (draw_splash_albumart(buf_tmp, buf_tmp_size, 1, 58, 79, 44))
        drawn++;
    if (draw_splash_albumart(buf_tmp, buf_tmp_size, 2, LCD_WIDTH - 102,
                             79, 44))
        drawn++;
    if (draw_splash_albumart(buf_tmp, buf_tmp_size, 0,
                             (LCD_WIDTH - 72) / 2, 53, 72))
        drawn++;

    if (drawn == 0)
        return;

    rb->lcd_set_foreground(LCD_RGBPACK(224, 228, 234));
    rb->lcd_hline(86, LCD_WIDTH - 87, 133);
    rb->lcd_set_foreground(LCD_RGBPACK(246, 247, 249));
    rb->lcd_hline(98, LCD_WIDTH - 99, 134);
#else
    (void)buf_tmp;
    (void)buf_tmp_size;
#endif
}

/**
  Draw the Cover Flow loading screen
 */
static void draw_splashscreen(unsigned char * buf_tmp, size_t buf_tmp_size)
{
    struct screen* display = rb->screens[SCREEN_MAIN];
    struct bitmap background;
    int bg_ret;
#if FB_DATA_SZ > 1
    ALIGN_BUFFER(buf_tmp, buf_tmp_size, sizeof(fb_data));
#endif
    struct bitmap logo = {
#if LCD_WIDTH < 200
        .width = 100,
        .height = 18,
#else
        .width = 193,
        .height = 34,
#endif
        .data = buf_tmp
    };
    background.width = LCD_WIDTH;
    background.height = LCD_HEIGHT;
    background.data = buf_tmp;
    background.format = FORMAT_NATIVE;
    bg_ret = rb->read_bmp_file(SPLASH_BG_BMP, &background, buf_tmp_size,
                               FORMAT_NATIVE, NULL);
#if LCD_DEPTH > 1
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_background(pf_bg_color);
    rb->lcd_set_foreground(pf_fg_color);
#else
    rb->lcd_set_background(N_BRIGHT(0));
    rb->lcd_set_foreground(N_BRIGHT(255));
#endif
#else
    rb->lcd_set_drawmode(PICTUREFLOW_DRMODE);
#endif
    rb->lcd_clear_display();

    if (bg_ret > 0)
        display->bitmap(background.data, 0, 0, background.width, background.height);

    draw_splash_albumart_strip(buf_tmp, buf_tmp_size);

    int ret = rb->read_bmp_file(SPLASH_BMP, &logo, buf_tmp_size,
                                FORMAT_NATIVE, NULL);

    if (ret > 0)
    {
#if LCD_DEPTH == 1  /* Mono LCDs need the logo inverted */
        rb->lcd_set_drawmode(PICTUREFLOW_DRMODE ^ DRMODE_INVERSEVID);
#endif
        display->bitmap(logo.data, (LCD_WIDTH - logo.width) / 2, 10,
            logo.width, logo.height);
#if LCD_DEPTH == 1  /* Mono LCDs need the logo inverted */
        rb->lcd_set_drawmode(PICTUREFLOW_DRMODE);
#endif
    }

    rb->lcd_update();
}


/**
  Draw a simple progress bar
 */
static void draw_progressbar(int step, int count, char *msg)
{
    static int txt_w, txt_h;
    const int bar_height = 10;
    const int w = LCD_WIDTH - 44;
    const int x = 22;
    static int y;
    if (msg != NULL)
    {
#if LCD_DEPTH > 1
#ifdef HAVE_LCD_COLOR
        rb->lcd_set_background(pf_bg_color);
        rb->lcd_set_foreground(pf_fg_color);
#else
        rb->lcd_set_background(N_BRIGHT(0));
        rb->lcd_set_foreground(N_BRIGHT(255));
#endif
#else
        rb->lcd_set_drawmode(PICTUREFLOW_DRMODE);
#endif
        rb->lcd_getstringsize(msg, &txt_w, &txt_h);

        y = (LCD_HEIGHT - txt_h)/2 + 18;

        rb->lcd_putsxy((LCD_WIDTH - txt_w)/2, y, msg);
        y += (txt_h + 9);
    }
#if LCD_DEPTH > 1
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(N_PIX(99, 84, 128));
#else
    rb->lcd_set_foreground(N_BRIGHT(100));
#endif
#endif
    rb->lcd_drawrect(x, y, w+2, bar_height);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(N_PIX(159, 119, 236));
#endif

    rb->lcd_fillrect(x+1, y+1, count > 0 ? step * w / count : 0, bar_height-2);
#if LCD_DEPTH > 1
    rb->lcd_set_foreground(N_PIX(232, 211, 255));
    rb->lcd_fillrect(x+2, y+2, count > 0 ? step * (w - 2) / count : 0, 1);
#endif
#if LCD_DEPTH > 1
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(pf_fg_color);
#else
    rb->lcd_set_foreground(N_BRIGHT(255));
#endif
#endif
    rb->lcd_update();
    rb->yield();
}

/* Calculate modified FNV hash of string
 * has good avalanche behaviour and uniform distribution
 * see http://home.comcast.net/~bretm/hash/ */
static unsigned int mfnv(char *str)
{
    const unsigned int p = 16777619;
    unsigned int hash = 0x811C9DC5; // 2166136261;

    if (!str)
        return 0;

    while(*str)
        hash = (hash ^ *str++) * p;
    hash += hash << 13;
    hash ^= hash >> 7;
    hash += hash << 3;
    hash ^= hash >> 17;
    hash += hash << 5;
    return hash;
}

/**
 Save the given bitmap as filename in the pfraw format
 */
static bool save_pfraw(char* filename, struct bitmap *bm)
{
    char tmpname[MAX_PATH];
    struct pfraw_header bmph;
    size_t data_size;

    if (bm->width <= 0 || bm->height <= 0 ||
        (size_t)bm->width > SIZE_MAX / (size_t)bm->height ||
        (size_t)bm->width * (size_t)bm->height >
            UINT32_MAX / sizeof(pix_t))
        return false;

    data_size = sizeof(pix_t) * (size_t)bm->width * (size_t)bm->height;
    bmph.magic = PFRAW_MAGIC;
    bmph.width = bm->width;
    bmph.height = bm->height;
    bmph.data_size = data_size;

    if (rb->snprintf(tmpname, sizeof(tmpname), "%s.tmp", filename) >=
        (int)sizeof(tmpname))
        return false;

    int fh = rb->open(tmpname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fh < 0)
        return false;

    bool ok = rb->write(fh, &bmph, sizeof(bmph)) == (ssize_t)sizeof(bmph) &&
              rb->write(fh, bm->data, data_size) == (ssize_t)data_size;
    if (rb->close(fh) < 0)
        ok = false;

    if (ok)
    {
        /* Publish a complete record without first deleting the old one. */
        if (rb->rename(tmpname, filename) == 0)
            return true;
    }

    rb->remove(tmpname);
    return false;
}

static bool incremental_albumart_cache(bool verbose)
{
    if (!aa_cache.buf)
        goto aa_failure;

    if (aa_cache.inspected >= pf_idx.album_ct)
        return false;

    /* Prevent idle poweroff */
    rb->reset_poweroff_timer();

    int idx, ret;
    unsigned int hash_artist, hash_album;
    unsigned int format = FORMAT_NATIVE | FORMAT_DITHER;

    if (pf_cfg.resize)
        format |= FORMAT_RESIZE|FORMAT_KEEP_ASPECT;

    idx = aa_cache.idx;
    if (idx >= pf_idx.album_ct || idx < 0) { idx = 0; } /* Rollover */


    aa_cache.idx++;
    aa_cache.inspected++;
    if (aa_cache.idx >= pf_idx.album_ct) { aa_cache.idx = 0; } /* Rollover */


    hash_artist = mfnv(get_album_artist(idx));
    hash_album = mfnv(get_album_name(idx));

    rb->snprintf(aa_cache.pfraw_file, sizeof(aa_cache.pfraw_file),
                 CACHE_PREFIX "/%x%x.pfraw", hash_album, hash_artist);

    if (pf_cfg.cache_version == CACHE_VERSION && pf_cfg.update_albumart &&
        rb->file_exists(aa_cache.pfraw_file)) {
        aa_cache.slides++;
        goto aa_success;
    }

    if (!get_albumart_for_index_from_db(idx, aa_cache.file, sizeof(aa_cache.file)))
    {
        if (albumart_source_missing)
        {
            /* An explicit negative cache record is distinct from a pending
             * read or a damaged/missing prepared file. */
            struct pfraw_header missing = {PFRAW_MISSING_MAGIC, 0, 0, 0};
            char temporary[MAX_PATH];
            rb->snprintf(temporary, sizeof(temporary), "%s.tmp",
                         aa_cache.pfraw_file);
            int fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd >= 0)
            {
                bool ok = rb->write(fd, &missing, sizeof(missing)) ==
                          (ssize_t)sizeof(missing);
                ok = rb->close(fd) == 0 && ok;
                if (ok && rb->rename(temporary, aa_cache.pfraw_file) == 0)
                {
                    aa_cache.slides++;
                    goto aa_success;
                }
                rb->remove(temporary);
            }
        }
        goto aa_failure;
    }


    aa_cache.input_bmp.data = aa_cache.buf;
    aa_cache.input_bmp.width = DISPLAY_WIDTH;
    aa_cache.input_bmp.height = DISPLAY_HEIGHT;

    ret = read_image_file(aa_cache.file, &aa_cache.input_bmp,
                          aa_cache.buf_sz, format, &format_transposed);
    if (ret <= 0) {
        if (verbose) {
            rb->splashf(HZ, "Album art is bad: %s", get_album_name(idx));
        }

        goto aa_failure;
    }
    if (!save_pfraw(aa_cache.pfraw_file, &aa_cache.input_bmp))
    {
        if (verbose) { rb->splash(HZ, "Could not write bmp"); }
        goto aa_failure;
    }
    aa_cache.slides++;

aa_failure:
    if (verbose)
    {
        if (aa_cache.inspected >= pf_idx.album_ct)
            configfile_save(CONFIG_FILE, config, CONFIG_NUM_ITEMS,
                            CONFIG_VERSION);
        return false;
    }

aa_success:
    if (aa_cache.inspected >= pf_idx.album_ct)
    {
        configfile_save(CONFIG_FILE, config, CONFIG_NUM_ITEMS,
                            CONFIG_VERSION);
        buf_ctx_lock();
        free_all_slide_prio(0);
        buf_ctx_unlock();
        if (pf_state == pf_idle)
            rb->queue_post(&thread_q, EV_WAKEUP, 0);
    }

    if(verbose)/* direct interaction with user */
        return true;

    return false;
}

/**
 Precomupte the album art images and store them in CACHE_PREFIX.
 Use the "?" bitmap if image is not found.
 */
static bool create_albumart_cache(void)
{
    draw_splashscreen(pf_idx.buf, pf_idx.buf_sz);
    draw_progressbar(0, pf_idx.album_ct, STR_STEP_PREPARING_ARTWORK);
    aa_cache.inspected = 0;
    for (int i=0; i < pf_idx.album_ct; i++)
    {
        incremental_albumart_cache(true);
        draw_progressbar(aa_cache.inspected, pf_idx.album_ct, NULL);
        if (rb->button_get(false) > BUTTON_NONE)
            return true;
    }
    if ( aa_cache.slides == 0 ) {
        /* Warn the user that we couldn't find any albumart */
        rb->splash(2*HZ, ID2P(LANG_NO_ALBUMART_FOUND));
        return false;
    }
    return true;
}

/**
  Create the "?" slide, that is shown while loading
  or when no cover was found.
 */
static int create_empty_slide(bool force)
{
    const unsigned int format = FORMAT_NATIVE|FORMAT_RESIZE|FORMAT_KEEP_ASPECT;

    if (!aa_cache.buf)
        return false;

    if ( force || ! rb->file_exists( EMPTY_SLIDE ) )  {
        aa_cache.input_bmp.width = DISPLAY_WIDTH;
        aa_cache.input_bmp.height = DISPLAY_HEIGHT;
#if LCD_DEPTH > 1
        aa_cache.input_bmp.format = FORMAT_NATIVE;
#endif
        aa_cache.input_bmp.data = (char*)aa_cache.buf;

        scaled_read_bmp_file(EMPTY_SLIDE_BMP, &aa_cache.input_bmp,
                             aa_cache.buf_sz, format, &format_transposed);

        if (!save_pfraw(EMPTY_SLIDE, &aa_cache.input_bmp))
            return false;
    }

    return true;
}

/**
 Thread used for loading and preparing bitmaps in the background
 */
static void thread(void)
{
    /* SSD mode: poll more frequently since disk access is cheap */
    long sleep_time = storage_mode_is_ssd() ? HZ : 5 * HZ;
    struct queue_event ev;
    while (1) {
        rb->queue_wait_w_tmo(&thread_q, &ev, sleep_time);
        switch (ev.id) {
            case EV_EXIT:
                return;
            case EV_WAKEUP:
                /* we just woke up */
                break;
        }

        if(ev.id != SYS_TIMEOUT) {
            while ( rb->queue_empty(&thread_q) ) {
                buf_ctx_lock();
                bool slide_loaded = load_new_slide();
                buf_ctx_unlock();
                if (!slide_loaded)
                    break;
                rb->yield();
            }
        }
    }
}


/**
 End the thread by posting the EV_EXIT event
 */
static void end_pf_thread(void)
{
    if ( thread_is_running ) {
        rb->queue_post(&thread_q, EV_EXIT, 0);
        rb->thread_wait(thread_id);
        /* remove the thread's queue from the broadcast list */
        rb->queue_delete(&thread_q);
        thread_is_running = false;
    }
}


/**
 Create the thread an setup the event queue
 */
static bool create_pf_thread(void)
{
    /* put the thread's queue in the bcast list */
    rb->queue_init(&thread_q, true);
    if ((thread_id = rb->create_thread(
                           thread,
                           thread_stack,
                           sizeof(thread_stack),
                            0,
                           "Picture load thread"
                               IF_PRIO(, PRIORITY_BUFFERING)
                               IF_COP(, CPU)
                                      )
        ) == 0) {
        rb->queue_delete(&thread_q);
        return false;
    }
    thread_is_running = true;
    rb->queue_post(&thread_q, EV_WAKEUP, 0);
    return true;
}

#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && PF_PLAYBACK_CAPABLE
static void __attribute__((noinline)) pf_hibernate_advertise(void)
{
    rb->button_queue_post(SYS_HIBERNATE_CAPABLE,
            (intptr_t)SYS_HIBERNATE_PLUGIN_PROTOCOL);
}

static void __attribute__((noinline)) pf_hibernate_activate(void)
{
    /* PictureFlow clears stale input once initialization is complete.  The
     * capability event must be posted after that clear or the plugin can
     * erase its own opt-in before the core has consumed it. */
    rb->button_clear_queue();
    pf_hibernate_advertise();
}

static bool __attribute__((noinline)) pf_hibernate_acknowledge(void)
{
    int event;

    if (track_prepare_album >= 0)
        free_borrowed_tracks();

    /* The core has paused before touching audio/storage hardware and is
     * waiting for the same kind of service acknowledgement used by
     * RetailOS. Stop the only PictureFlow worker so it cannot be inside
     * cover-cache I/O when the disk/controller is suspended. The plugin's
     * main thread is this caller and remains parked in button_get() until
     * the retained transaction has returned. */
    end_pf_thread();
    rb->button_queue_post(SYS_HIBERNATE_READY,
            (intptr_t)SYS_HIBERNATE_PLUGIN_PROTOCOL);

    do
    {
        event = rb->button_get(true);
    }
    while (event != SYS_HIBERNATE_COMPLETE);

    /* Hardware, scheduler, display, and core audio are live again. Start a
     * fresh cover-cache worker before accepting UI input. */
    return create_pf_thread();
}
#endif


static void initialize_slide_cache(void)
{
    int i= 0;
    for (i = 0; i < SLIDE_CACHE_SIZE; i++) {
        pf_sldcache.cache[i].hid = 0;
        pf_sldcache.cache[i].index = 0;
        pf_sldcache.cache[i].next = i + 1;
        pf_sldcache.cache[i].prev = i - 1;
    }
    pf_sldcache.cache[0].prev = i - 1;
    pf_sldcache.cache[i - 1].next = 0;

    pf_sldcache.free = 0;
    pf_sldcache.used = -1;
    pf_sldcache.left_idx = -1;
    pf_sldcache.right_idx = -1;
    pf_sldcache.center_idx = -1;
}


/*
 * The following functions implement the linked-list-in-array used to manage
 * the LRU cache of slides, and the list of free cache slots.
 */

#define _SEEK_RIGHT_WHILE(start, cond) \
({ \
    int ind_, next_ = (start); \
    int i_ = 0; \
    do { \
        ind_ = next_; \
        next_ = pf_sldcache.cache[ind_].next; \
        i_++; \
    } while (next_ != pf_sldcache.used && (cond) && i_ < SLIDE_CACHE_SIZE); \
    if (i_ >= SLIDE_CACHE_SIZE) \
    /* TODO: Not supposed to happen */ \
        ind_ = -1; \
    ind_; \
})

#define _SEEK_LEFT_WHILE(start, cond) \
({ \
    int ind_, next_ = (start); \
    int i_ = 0; \
    do { \
        ind_ = next_; \
        next_ = pf_sldcache.cache[ind_].prev; \
        i_++; \
    } while (ind_ != pf_sldcache.used && (cond) && i_ < SLIDE_CACHE_SIZE); \
    if (i_ >= SLIDE_CACHE_SIZE) \
    /* TODO: Not supposed to happen */ \
        ind_ = -1; \
    ind_; \
})

/**
 Pop the given item from the linked list starting at *head, returning the next
 item, or -1 if the list is now empty.
*/
static inline int lla_pop_item (int *head, int i)
{
    int prev = pf_sldcache.cache[i].prev;
    int next = pf_sldcache.cache[i].next;
    if (i == next)
    {
        *head = -1;
        return -1;
    }
    else if (i == *head)
        *head = next;
    pf_sldcache.cache[next].prev = prev;
    pf_sldcache.cache[prev].next = next;
    return next;
}


/**
 Pop the head item from the list starting at *head, returning the index of the
 item, or -1 if the list is already empty.
*/
static inline int lla_pop_head (int *head)
{
    int i = *head;
    if (i != -1)
        lla_pop_item(head, i);
    return i;
}

/**
 Insert the item at index i before the one at index p.
*/
static inline void lla_insert (int i, int p)
{
    int next = p;
    int prev = pf_sldcache.cache[next].prev;
    pf_sldcache.cache[next].prev = i;
    pf_sldcache.cache[prev].next = i;
    pf_sldcache.cache[i].next = next;
    pf_sldcache.cache[i].prev = prev;
}


/**
 Insert the item at index i at the end of the list starting at *head.
*/
static inline void lla_insert_tail (int *head, int i)
{
    if (*head == -1)
    {
        *head = i;
        pf_sldcache.cache[i].next = i;
        pf_sldcache.cache[i].prev = i;
    } else
        lla_insert(i, *head);
}

/**
 Insert the item at index i before the one at index p.
*/
static inline void lla_insert_after(int i, int p)
{
    p = pf_sldcache.cache[p].next;
    lla_insert(i, p);
}


/**
 Insert the item at index i before the one at index p in the list starting at
 *head
*/
static inline void lla_insert_before(int *head, int i, int p)
{
    lla_insert(i, p);
    if (*head == p)
        *head = i;
}


/**
 Free the used slide at index i, and its buffer, and move it to the free
 slides list.
*/
static inline void free_slide(int i)
{
    if (pf_sldcache.cache[i].hid != empty_slide_hid)
        rb->buflib_free(&buf_ctx, pf_sldcache.cache[i].hid);
    pf_sldcache.cache[i].index = -1;
    lla_pop_item(&pf_sldcache.used, i);
    lla_insert_tail(&pf_sldcache.free, i);
    if (pf_sldcache.used == -1)
    {
        pf_sldcache.right_idx = -1;
        pf_sldcache.left_idx = -1;
        pf_sldcache.center_idx = -1;
    }
}


/**
 Free one slide ranked above the given priority. If no such slide can be found,
 return false.
*/
static bool free_slide_prio(int prio)
{
    if (pf_sldcache.used == -1)
        return false;

    int i, prio_max;
    int l = pf_sldcache.used;
    int r = pf_sldcache.cache[pf_sldcache.used].prev;

    int prio_l = pf_sldcache.cache[l].index < center_index ?
           center_index - pf_sldcache.cache[l].index : 0;
    int prio_r = pf_sldcache.cache[r].index > center_index ?
           pf_sldcache.cache[r].index - center_index : 0;
    if (prio_l > prio_r)
    {
        i = l;
        prio_max = prio_l;
    } else {
        i = r;
        prio_max = prio_r;
    }
    /* The current scene and both adjacent transition margins are leases.
     * A cover cannot disappear merely because lookahead needs memory. */
    if (prio_max > prio && prio_max > pf_cfg.num_slides + 2)
    {
        if (i == pf_sldcache.left_idx)
            pf_sldcache.left_idx = pf_sldcache.cache[i].next;
        if (i == pf_sldcache.right_idx)
            pf_sldcache.right_idx = pf_sldcache.cache[i].prev;
        free_slide(i);
        return true;
    } else
        return false;
}


/**
 Free all slides ranked above the given priority.
*/
static void free_all_slide_prio(int prio)
{
    while (free_slide_prio(prio))
    {;;}
}


/**
 Read the pfraw image given as filename and return the hid of the buffer
 */
static int read_pfraw(char* filename, int prio, bool background)
{
    struct pfraw_header bmph;
    int result = -2;
    if (background)
        buf_ctx_unlock();
    rb->mutex_lock(&artwork_io_mutex);
    int fh = rb->open(filename, O_RDONLY);
    if (fh < 0) {
        /* pf_cfg.cache_version = CACHE_UPDATE; -- don't invalidate on missing pfraw */
        goto done;
    }

    off_t file_size = rb->filesize(fh);
    if (rb->read(fh, &bmph, sizeof(bmph)) != (ssize_t)sizeof(bmph))
        goto corrupt;
    if (bmph.magic == PFRAW_MISSING_MAGIC && bmph.width == 0 &&
        bmph.height == 0 && bmph.data_size == 0 && file_size == sizeof(bmph))
    {
        rb->close(fh);
        result = empty_slide_hid;
        goto done;
    }
    if (bmph.magic != PFRAW_MAGIC || bmph.width <= 0 || bmph.height <= 0 ||
        (size_t)bmph.width > SIZE_MAX / (size_t)bmph.height)
        goto corrupt;

    size_t pixel_count = (size_t)bmph.width * (size_t)bmph.height;
    if (pixel_count > UINT32_MAX / sizeof(pix_t))
        goto corrupt;

    size_t data_size = sizeof(pix_t) * pixel_count;
    if (bmph.data_size != data_size || data_size > aa_cache.buf_sz ||
        file_size != (off_t)(sizeof(bmph) + data_size) ||
        data_size > SIZE_MAX - sizeof(struct dim))
        goto corrupt;

    size_t size = sizeof(struct dim) + data_size;

    /* Decode workspace is idle while this mutex is held. Disk reads never
     * hold the slide/render mutex, and incomplete pixels are not published. */
    if (rb->read(fh, aa_cache.buf, data_size) != (ssize_t)data_size)
        goto corrupt;
    rb->close(fh);
    fh = -1;
    result = -1; /* Allocation pressure is not an artwork repair request. */
    if (background)
        buf_ctx_lock();

    int hid;
    do {
        hid = rb->buflib_alloc(&buf_ctx, size);
    } while (hid < 0 && free_slide_prio(prio));

    if (hid < 0) {
        if (background)
            buf_ctx_unlock();
        goto done;
    }

    struct dim *bm = rb->buflib_get_data(&buf_ctx, hid);

    bm->width = bmph.width;
    bm->height = bmph.height;
    pix_t *data = (pix_t*)(sizeof(struct dim) + (char *)bm);

    rb->memcpy(data, aa_cache.buf, data_size);
    result = hid;
    if (background)
        buf_ctx_unlock();
    goto done;

corrupt:
    rb->close(fh);
    /* Keep damaged records available for diagnosis/targeted repair. */
done:
    rb->mutex_unlock(&artwork_io_mutex);
    if (background)
        buf_ctx_lock();
    return result;
}


/**
  Load the surface for the given slide_index into the cache at cache_index.
 */
static inline bool load_and_prepare_surface(const int slide_index,
                                            const int cache_index,
                                            const int prio)
{
    char pfraw_file[MAX_PATH];
    unsigned int hash_artist = mfnv(get_album_artist(slide_index));
    unsigned int hash_album = mfnv(get_album_name(slide_index));

    rb->snprintf(pfraw_file, sizeof(pfraw_file), CACHE_PREFIX "/%x%x.pfraw",
                 hash_album, hash_artist);

    if (cache_index < 0 || cache_index >= SLIDE_CACHE_SIZE)
        return false;
    int hid = read_pfraw(pfraw_file, prio, true);
    if (hid < 0)
    {
        if (hid == -2)
            artwork_repair_album = slide_index;
        return false;
    }

    pf_sldcache.cache[cache_index].hid = hid;

    if ( cache_index < SLIDE_CACHE_SIZE ) {
        pf_sldcache.cache[cache_index].index = slide_index;
    }

    return true;
}


/**
 Load the "next" slide that we can load, freeing old slides if needed, provided
 that they are further from center_index than the current slide
*/
bool load_new_slide(void)
{
    if (wants_to_quit)
        return false;

    int i = -1;

    if (pf_sldcache.center_idx != -1)
    {
        int next, prev;
        if (pf_sldcache.cache[pf_sldcache.center_idx].index != center_index)
        {
            if (pf_sldcache.cache[pf_sldcache.center_idx].index < center_index)
            {
                pf_sldcache.center_idx = _SEEK_RIGHT_WHILE(pf_sldcache.center_idx,
                                pf_sldcache.cache[next_].index <= center_index);
                if (pf_sldcache.center_idx == -1)
                    goto fatal_fail;

                prev = pf_sldcache.center_idx;
                next = pf_sldcache.cache[pf_sldcache.center_idx].next;
            }
            else
            {
                pf_sldcache.center_idx = _SEEK_LEFT_WHILE(pf_sldcache.center_idx,
                                pf_sldcache.cache[next_].index >= center_index);
                if (pf_sldcache.center_idx == -1)
                    goto fatal_fail;

                next = pf_sldcache.center_idx;
                prev = pf_sldcache.cache[pf_sldcache.center_idx].prev;
            }
            if (pf_sldcache.cache[pf_sldcache.center_idx].index != center_index)
            {
                if (pf_sldcache.free == -1)
                    free_slide_prio(0);

                i = lla_pop_head(&pf_sldcache.free);
                if (!load_and_prepare_surface(center_index, i, 0))
                    goto fail_and_refree;

                if (pf_sldcache.cache[next].index == -1)
                {
                    if (pf_sldcache.cache[prev].index == -1)
                        goto insert_first_slide;
                    else
                        next = pf_sldcache.cache[prev].next;
                }
                lla_insert(i, next);
                if (pf_sldcache.cache[i].index < pf_sldcache.cache[pf_sldcache.used].index)
                    pf_sldcache.used = i;

                pf_sldcache.center_idx = i;
                pf_sldcache.left_idx = i;
                pf_sldcache.right_idx = i;
                return true;
            }
        }
        int left, center, right;
        left = pf_sldcache.cache[pf_sldcache.left_idx].index;
        center = pf_sldcache.cache[pf_sldcache.center_idx].index;
        right = pf_sldcache.cache[pf_sldcache.right_idx].index;

        if (left > center)
            pf_sldcache.left_idx = pf_sldcache.center_idx;
        if (right < center)
            pf_sldcache.right_idx = pf_sldcache.center_idx;

        pf_sldcache.left_idx = _SEEK_LEFT_WHILE(pf_sldcache.left_idx,
            pf_sldcache.cache[ind_].index - 1 == pf_sldcache.cache[next_].index);

        pf_sldcache.right_idx = _SEEK_RIGHT_WHILE(pf_sldcache.right_idx,
            pf_sldcache.cache[ind_].index + 1 == pf_sldcache.cache[next_].index);
        if (pf_sldcache.right_idx == -1 || pf_sldcache.left_idx == -1)
            goto fatal_fail;


        /* update indices */
        left = pf_sldcache.cache[pf_sldcache.left_idx].index;
        center = pf_sldcache.cache[pf_sldcache.center_idx].index;
        right = pf_sldcache.cache[pf_sldcache.right_idx].index;

        int prio_l = center - left + 1;
        int prio_r = right - center + 1;
        if ((prio_l < prio_r
             || right >= number_of_slides - 1) && left > 0)
        {
            if (pf_sldcache.free == -1 && !free_slide_prio(prio_l))
            {
                return false;
            }

            i = lla_pop_head(&pf_sldcache.free);
            if (load_and_prepare_surface(left - 1, i, prio_l))
            {
                lla_insert_before(&pf_sldcache.used, i, pf_sldcache.left_idx);
                pf_sldcache.left_idx = i;
                return true;
            }
        } else if(right < number_of_slides - 1)
        {
            if (pf_sldcache.free == -1 && !free_slide_prio(prio_r))
            {
                return false;
            }

            i = lla_pop_head(&pf_sldcache.free);
            if (load_and_prepare_surface(right + 1, i, prio_r))
            {
                lla_insert_after(i, pf_sldcache.right_idx);
                pf_sldcache.right_idx = i;
                return true;
            }
        }
    } else {
        i = lla_pop_head(&pf_sldcache.free);
        if (load_and_prepare_surface(center_index, i, 0))
        {
insert_first_slide:
            pf_sldcache.cache[i].next = i;
            pf_sldcache.cache[i].prev = i;
            pf_sldcache.center_idx = i;
            pf_sldcache.left_idx = i;
            pf_sldcache.right_idx = i;
            pf_sldcache.used = i;
            return true;
        }
    }
fail_and_refree:
    if (i != -1)
    {
        lla_insert_tail(&pf_sldcache.free, i);
    }
    return false;
fatal_fail:
    /* A complete cache reset must release even the pinned scene. Otherwise
     * initialize_slide_cache() loses its handles and leaks the slide pool. */
    while (pf_sldcache.used != -1)
        free_slide(pf_sldcache.used);
    initialize_slide_cache();
    return false;
}


/**
  Get a slide from the buffer
 */
static inline struct dim *get_slide(const int hid)
{
    if (!hid)
        return NULL;

    struct dim *bmp;

    bmp = rb->buflib_get_data(&buf_ctx, hid);

    return bmp;
}


/**
 Return the requested surface
*/
static inline struct dim *surface(const int slide_index)
{
    if (slide_index < 0)
        return 0;
    if (slide_index >= number_of_slides)
        return 0;
    int i;
    if ((i = pf_sldcache.used ) != -1)
    {
        int j = 0;
        do {
            if (pf_sldcache.cache[i].index == slide_index) {
                return get_slide(pf_sldcache.cache[i].hid);
            }
            i = pf_sldcache.cache[i].next;
            j++;
        } while (i != pf_sldcache.used && j < SLIDE_CACHE_SIZE);
    }
    return NULL;
}

/**
 adjust slides so that they are in "steady state" position
 */
static void reset_slides(void)
{
    center_slide.angle = 0;
    center_slide.cx = 0;
    center_slide.cy = 0;
    center_slide.distance = 0;
    center_slide.slide_index = center_index;

    int i;
    for (i = 0; i < pf_cfg.num_slides; i++) {
        struct slide_data *si = &left_slides[i];
        si->angle = itilt;
        si->cx = -(offsetX + auto_slide_spacing * i);
        si->cy = offsetY;
        si->slide_index = center_index - 1 - i;
        si->distance = 0;
    }

    for (i = 0; i < pf_cfg.num_slides; i++) {
        struct slide_data *si = &right_slides[i];
        si->angle = -itilt;
        si->cx = offsetX + auto_slide_spacing * i;
        si->cy = offsetY;
        si->slide_index = center_index + 1 + i;
        si->distance = 0;
    }
}


/**
 Updates look-up table and other stuff necessary for the rendering.
 Call this when the viewport size or slide dimension is changed.
 *
 * To calculate the offset that will provide the proper margin, we use the same
 * projection used to render the slides. The solution for xc, the slide center,
 * is:
 *                         xp * (zo + xs * sin(r))
 * xc = xp - xs * cos(r) + ───────────────────────
 *                                    z
 * TODO: support moving the side slides toward or away from the camera
 */
static void recalc_offsets(void)
{
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
    {
        /* Classic side stacks share a steeper canonical yaw, rather than
         * converging toward a common camera vanishing point. Anchor the
         * nearest outer edge at x=55/264 and repeat at 28-pixel intervals. */
        itilt = 70 * IANGLE_MAX / 360;
        PFreal half = -MAXSLIDE_LEFT_R;
        PFreal depth = fmul(half, fsin(itilt));
        PFreal edge = fmul(half, fcos(itilt));
        PFreal shift = 104 * PFREAL_ONE + PFREAL_HALF - edge;
        offsetX = shift + fmul(shift, depth) / CAM_DIST;
        auto_slide_spacing = 28 * PFREAL_ONE + 28 * depth / CAM_DIST;
        offsetY = 0;
        return;
    }
#endif
    PFreal xs = PFREAL_HALF - DISPLAY_WIDTH * PFREAL_HALF;
    PFreal zo;
    PFreal xp = (DISPLAY_WIDTH * PFREAL_HALF - PFREAL_HALF +
                pf_cfg.center_margin * PFREAL_ONE) * pf_cfg.zoom / 100
                - pf_cfg.slide_tuck * PFREAL_ONE;
    PFreal cosr, sinr;

    itilt = (pf_cfg.parallel_slides ? 55 : 70) * IANGLE_MAX / 360;
    cosr = fcos(-itilt);
    sinr = fsin(-itilt);
    zo = CAM_DIST_R * 100 / pf_cfg.zoom - CAM_DIST_R +
        fmuln(MAXSLIDE_LEFT_R, sinr, PFREAL_SHIFT - 2, 0);
    offsetX = xp - fmul(xs, cosr) + fmuln(xp,
        zo + fmuln(xs, sinr, PFREAL_SHIFT - 2, 0), PFREAL_SHIFT - 2, 0)
        / CAM_DIST;
    offsetY = DISPLAY_WIDTH / 2 * (fsin(itilt) + PFREAL_ONE / 2);

    /* auto-compute side slide spacing for 3 visible slides per side.
     * We distribute 3 visible slides across 2 intervals from offsetX
     * to cx_last (the world-space cx where the last slide's far edge
     * reaches the screen border).
     */
    {
        PFreal cx_last;
        if (pf_cfg.parallel_slides) {
            /* In parallel mode, each slide is rendered at cx=0 then
             * shifted by screen_cx = CAM_DIST*cx/(CAM_DIST_R+zo).
             * Find the canonical far-edge position (right bitmap edge
             * of a right slide rendered at cx=0), then invert the
             * shift to find the cx that reaches the screen border. */
            PFreal edge_r = fdiv(CAM_DIST * fmul(-xs, cosr),
                CAM_DIST_R + zo + fmul(-xs, sinr));
            PFreal target = -DISPLAY_LEFT_R - edge_r;
            /* Invert: cx = target * (CAM_DIST_R+zo) / CAM_DIST_R
             *            = target + target * zo / CAM_DIST_R */
            cx_last = target
                + fmuln(target, zo, PFREAL_SHIFT - 2, 0) / CAM_DIST;
        } else {
            cx_last = (-DISPLAY_LEFT_R) * 100 / pf_cfg.zoom
                             + fmul(xs, cosr);
        }
        PFreal span = cx_last - offsetX;
        if (span < PFREAL_ONE)
            span = PFREAL_ONE;
        auto_slide_spacing = span / 2;
    }
}

/**
   Fade the given color toward the theme background color.
   a = 256 means fully opaque (original color), a = 0 means fully bg.
 */
static inline pix_t fade_color(pix_t c, unsigned a)
{
#if (LCD_PIXELFORMAT == RGB565SWAPPED)
    unsigned int result;
    c = swap16(c);
    a = (a + 2) & 0x1fc;
    unsigned int inv_a = 0x100 - a;
    result = (((c & 0xf81f) * a + pf_bg_rb * inv_a)) & 0xf81f00;
    result |= (((c & 0x7e0) * a + pf_bg_g * inv_a)) & 0x7e000;
    result >>= 8;
    return swap16(result);

#elif LCD_PIXELFORMAT == RGB565
    unsigned int result;
    a = (a + 2) & 0x1fc;
    unsigned int inv_a = 0x100 - a;
    result = (((c & 0xf81f) * a + pf_bg_rb * inv_a)) & 0xf81f00;
    result |= (((c & 0x7e0) * a + pf_bg_g * inv_a)) & 0x7e000;
    result >>= 8;
    return result;

#elif (LCD_PIXELFORMAT == RGB888 || LCD_PIXELFORMAT == XRGB8888)
    unsigned int pixel = FB_UNPACK_SCALAR_LCD(c);
    unsigned int bg = FB_UNPACK_SCALAR_LCD(pf_bg_color);
    unsigned int result;
    a = (a + 2) & 0x1fc;
    unsigned int inv_a = 0x100 - a;
    result  = (((pixel & 0xff00ff) * a + (bg & 0xff00ff) * inv_a)) & 0xff00ff00;
    result |= (((pixel & 0x00ff00) * a + (bg & 0x00ff00) * inv_a)) & 0x00ff0000;
    result >>= 8;
    return FB_SCALARPACK(result);

#else
    unsigned val = c;
    return MULUQ(val, a) >> 8;
#endif
}

/**
 * Render a single slide
 * Where xc is the slide's horizontal offset from center, xs is the horizontal
 * on the slide from its center, zo is the slide's depth offset from the plane
 * of the display, r is the angle at which the slide is tilted, and xp is the
 * point on the display corresponding to xs on the slide, the projection
 * formulas are:
 *
 *      z * (xc + xs * cos(r))
 * xp = ──────────────────────
 *       z + zo + xs * sin(r)
 *
 *      z * (xc - xp) - xp * zo
 * xs = ────────────────────────
 *      xp * sin(r) - z * cos(r)
 *
 * We use the xp projection once, to find the left edge of the slide on the
 * display. From there, we use the xs reverse projection to find the horizontal
 * offset from the slide center of each column on the screen, until we reach
 * the right edge of the slide, or the screen. The reverse projection can be
 * optimized by saving the numerator and denominator of the fraction, which can
 * then be incremented by (z + zo) and sin(r) respectively.
 */

/* Clear only the PictureFlow viewport area (not the status bar).
 * Uses screen->clear_viewport() which fills with bg_pattern,
 * unlike lcd_fillrect(DRMODE_SOLID) which fills with fg_pattern. */
static void pf_clear_display(void)
{
#if CONFIG_KEYPAD == IPOD_3G_PAD
    rb->lcd_set_viewport(NULL);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_set_viewport(&pf_vp);
#else
    rb->screens[SCREEN_MAIN]->clear_viewport();
#endif
}

static void render_slide(struct slide_data *slide, const int alpha)
{
    struct dim *bmp = surface(slide->slide_index);
    if (!bmp) {
        return;
    }
    if (slide->angle > 255 || slide->angle < -255)
        return;
    pix_t *src = (pix_t*)(sizeof(struct dim) + (char *)bmp);

    const int sw = bmp->width;
    const int sh = bmp->height;
    const PFreal slide_left = -sw * PFREAL_HALF + PFREAL_HALF;
    const int w = LCD_WIDTH;

    uint8_t reftab[REFLECT_HEIGHT]; /* on stack, which is in IRAM on several targets */

    if (alpha == 256) { /* opaque -> copy table */
        rb->memcpy(reftab, reflect_table, pf_reflect_height);
    } else {            /* precalculate faded table */
        int i, lalpha;
        for (i = 0; i < pf_reflect_height; i++) {
            lalpha = reflect_table[i];
            reftab[i] = (MULUQ(lalpha, alpha) + 129) >> 8;
        }
    }

    PFreal cosr = fcos(slide->angle);
    PFreal sinr = fsin(slide->angle);
    PFreal zo = PFREAL_ONE * slide->distance + CAM_DIST_R * 100 / pf_cfg.zoom
        - CAM_DIST_R - fmuln(MAXSLIDE_LEFT_R, fabs(sinr), PFREAL_SHIFT - 2, 0);

    /* For parallel rendering, project the slide as if cx=0 (canonical tilt),
     * then shift horizontally to the true screen position. */
    bool parallel = pf_cfg.parallel_slides;
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
        parallel = true;
#endif
    PFreal screen_cx = (parallel && slide->angle != 0)
        ? fdiv(CAM_DIST * slide->cx, CAM_DIST_R + zo) : 0;
    PFreal render_cx = (screen_cx != 0) ? 0 : slide->cx;

    PFreal xs = slide_left, xsnum, xsnumi, xsden, xsdeni;
    PFreal xp = fdiv(CAM_DIST * (render_cx + fmul(xs, cosr)),
        (CAM_DIST_R + zo + fmul(xs,sinr)));

    /* Since we're finding the screen position of the left edge of the slide,
     * we round up.
     */
    xp += screen_cx;
    int xi = (fmax(DISPLAY_LEFT_R, xp) - DISPLAY_LEFT_R + PFREAL_ONE - 1)
        >> PFREAL_SHIFT;
    xp = DISPLAY_LEFT_R + xi * PFREAL_ONE;
    if (xi >= w) {
        return;
    }
    PFreal xp_local = xp - screen_cx;
    xsnum = CAM_DIST * (render_cx - xp_local)
        - fmuln(xp_local, zo, PFREAL_SHIFT - 2, 0);
    xsden = fmuln(xp_local, sinr, PFREAL_SHIFT - 2, 0) - CAM_DIST * cosr;
    xs = fdiv(xsnum, xsden);

    xsnumi = -CAM_DIST_R - zo;
    xsdeni = sinr;
    int x;
    int dy = PFREAL_ONE;
    const int half_height = pf_half_height;
    const int lower_half = pf_lower_half;
    const int display_offs = pf_display_offs;
    const int reflect_height = pf_reflect_height;
    const bool perspective = (zo != 0 || slide->angle != 0);
    const int p_start_upper = (sh - 1 - display_offs) * PFREAL_ONE;
    const int p_start_lower = (sh - display_offs) * PFREAL_ONE;
    const int plim2_max = MIN(sh + reflect_height, sh * 2) * PFREAL_ONE;
    for (x = xi; x < w; x++) {
        /* Forward/inverse fixed-point rounding can put the first sample a
         * fraction left of the bitmap. Do not unsigned-wrap that to a huge
         * column and discard the entire face (notably at Classic's yaw). */
        int column = MAX(0, xs - slide_left) >> PFREAL_SHIFT;
        if (column >= sw)
            break;
        if (perspective) {
            dy = (CAM_DIST_R + zo + fmul(xs, sinr)) / CAM_DIST;
        }

        const pix_t *ptr = &src[column * sh];

#if LCD_STRIDEFORMAT == VERTICAL_STRIDE
#define PIXELSTEP_Y   1
#define LCDADDR(x, y) (&buffer[BUFFER_HEIGHT*(x) + (y)])
#else
#define PIXELSTEP_Y   BUFFER_WIDTH
#define LCDADDR(x, y) (&buffer[(y)*BUFFER_WIDTH + (x)])
#endif

        int p = p_start_upper;
        int plim = MAX(0, p - (half_height-1) * dy);
        pix_t *pixel = LCDADDR(x, half_height-1 );

        if (alpha == 256) {
            while (p >= plim) {
                *pixel = ptr[((unsigned)p) >> PFREAL_SHIFT];
                p -= dy;
                pixel -= PIXELSTEP_Y;
            }
        } else {
            while (p >= plim) {
                *pixel = fade_color(ptr[((unsigned)p) >> PFREAL_SHIFT], alpha);
                p -= dy;
                pixel -= PIXELSTEP_Y;
            }
        }
        p = p_start_lower;
        plim = MIN(sh * PFREAL_ONE, p + lower_half * dy);
        int plim2 = MIN(plim2_max, p + lower_half * dy);
        pixel = LCDADDR(x, half_height );

        if (alpha == 256) {
            while (p < plim) {
                *pixel = ptr[((unsigned)p) >> PFREAL_SHIFT];
                p += dy;
                pixel += PIXELSTEP_Y;
            }
        } else {
            while (p < plim) {
                *pixel = fade_color(ptr[((unsigned)p) >> PFREAL_SHIFT], alpha);
                p += dy;
                pixel += PIXELSTEP_Y;
            }
        }
        while (p < plim2) {
            int ty = (((unsigned)p) >> PFREAL_SHIFT) - sh;
            int lalpha = reftab[ty];
            *pixel = fade_color(ptr[sh - 1 - ty], lalpha);
            p += dy;
            pixel += PIXELSTEP_Y;
        }

        if (perspective)
        {
            xsnum += xsnumi;
            xsden += xsdeni;
            xs = fdiv(xsnum, xsden);
        } else
            xs += PFREAL_ONE;

    }
    /* let the music play... */
    rb->yield();
    return;
}

/**
  Jump to the given slide_index
 */
static inline void set_current_slide(const int slide_index)
{
    int old_center_index = center_index;
    step = 0;
    center_index = fbound(0, slide_index, number_of_slides - 1);
    if (old_center_index != center_index)
    {
        if (track_prepare_album >= 0)
            free_borrowed_tracks();
        rb->queue_remove_from_head(&thread_q, EV_WAKEUP);
        rb->queue_post(&thread_q, EV_WAKEUP, 0);
    }
    target = center_index;
    slide_frame = center_index << 16;
    reset_slides();
}


static void skip_animation_to_idle_state(void);
static void return_to_idle_state(void)
{
    if (track_prepare_album >= 0)
        free_borrowed_tracks();
    if (pf_state == pf_show_tracks)
        free_borrowed_tracks();
    if (pf_state == pf_show_tracks ||
        pf_state == pf_cover_in ||
        pf_state == pf_cover_out || pf_state == pf_open_pending)
        skip_animation_to_idle_state();
    else if (pf_state == pf_scrolling)
        set_current_slide(target);

    pf_state = pf_idle;
}


static void set_initial_slide(const char* selected_file)
{
    if (pf_cfg.last_album_name[0] && pf_cfg.last_album_artist[0])
    {
        for (int i = 0; i < number_of_slides; i++)
        {
            if (!rb->strcmp(pf_cfg.last_album_name, get_album_name(i)) &&
                !rb->strcmp(pf_cfg.last_album_artist, get_album_artist(i)))
            {
                pf_cfg.last_album = i;
                break;
            }
        }
    }
    if (selected_file)
        set_current_slide(retrieve_id3(&id3, selected_file) ?
                            id3_get_index(&id3) :
                            pf_cfg.last_album);
    else
        set_current_slide(rb->audio_status() ?
                            id3_get_index(rb->audio_current_track()) :
                            pf_cfg.last_album);

}

static void reselect(unsigned int hash_album, unsigned int hash_artist)
{
    int i, album_idx, artist_idx;
    for (i = 0; i < pf_idx.album_ct; i++ )
    {
        album_idx = pf_idx.album_index[i].name_idx;
        artist_idx = pf_idx.album_index[i].artist_idx;

        if(hash_album == mfnv(pf_idx.album_names + album_idx) &&
           hash_artist == mfnv(pf_idx.artist_names + artist_idx))
        {
            set_current_slide(i);
            pf_cfg.last_album = i;
            return;
        }
    }
    set_initial_slide(NULL);
}

static bool sort_albums(int new_sorting, bool from_settings)
{
    unsigned int hash_album, hash_artist;
    static const char* sort_options[] = {
        ID2P(LANG_ARTIST_PLUS_NAME),
        ID2P(LANG_ARTIST_PLUS_YEAR),
        ID2P(LANG_ID3_YEAR),
        ID2P(LANG_NAME)
    };

    /* Only change sorting once artwork has been inspected */
    if (aa_cache.inspected < pf_idx.album_ct)
    {
#ifdef USEGSLIB
        if (!from_settings)
            grey_show(false);
#endif
        rb->splash(HZ*2, rb->str(LANG_WAIT_FOR_CACHE));
#ifdef USEGSLIB
        if (!from_settings)
            grey_show(true);
#endif
        return false;
    }

    return_to_idle_state();

    pf_cfg.sort_albums_by = new_sorting;
    if (!from_settings)
    {
#ifdef USEGSLIB
        grey_show(false);
#if LCD_DEPTH > 1
        rb->lcd_set_background(N_BRIGHT(0));
        rb->lcd_set_foreground(N_BRIGHT(255));
#endif
        rb->lcd_clear_display();
        rb->lcd_update();
#endif
        rb->splash(HZ, sort_options[pf_cfg.sort_albums_by]);
    }

    hash_album = mfnv(get_album_name(center_index));
    hash_artist = mfnv(get_album_artist(center_index));

    end_pf_thread(); /* stop loading of covers  */
    artwork_repair_album = -1;

    rb->qsort(pf_idx.album_index, pf_idx.album_ct,
                  sizeof(struct album_data), compare_albums);

    /* Empty cache and restart cover loading thread */
    rb->buflib_init(&buf_ctx, (void *)pf_idx.buf, pf_idx.buf_sz);
    empty_slide_hid = read_pfraw(EMPTY_SLIDE, 0, false);
    initialize_slide_cache();
    create_pf_thread();

    reselect(hash_album, hash_artist); /* splash if not found */

#ifdef USEGSLIB
    if (!from_settings)
        grey_show(true);
#endif
    return true;
}

/**
  Start the animation for changing slides
 */
static void start_animation(void)
{
    step = (target < center_slide.slide_index) ? -1 : 1;
    scroll_animation_tick = *rb->current_tick;
    pf_state = pf_scrolling;
}

static void update_scroll_animation(void);

/**
  Go to the previous slide
 */
static void show_previous_slide(void)
{
    if (step == 0) {
        if (center_index > 0) {
            target = center_index - 1;
            start_animation();
        }
    } else if ( step > 0 ) {
        target = center_index;
        step = (target <= center_slide.slide_index) ? -1 : 1;
        if (step < 0)
            update_scroll_animation();
    } else {
        target = fmax(0, center_index - 2);
    }
}


/**
  Go to the next slide
 */
static void show_next_slide(void)
{
    if (step == 0) {
        if (center_index < number_of_slides - 1) {
            target = center_index + 1;
            start_animation();
        }
    } else if ( step < 0 ) {
        target = center_index;
        step = (target < center_slide.slide_index) ? -1 : 1;
        if (step > 0)
            update_scroll_animation();
    } else {
        target = fmin(center_index + 2, number_of_slides - 1);
    }
}


/**
  Render the slides. Updates only the offscreen buffer.
*/
static void render_all_slides(void)
{
    buf_ctx_lock();
    scene_drawn = true;
    /* Do not clear the last complete frame until every potentially visible
     * face is resident. This also gates distant jumps and fast reversals. */
    for (int i = MAX(0, center_index - pf_cfg.num_slides);
         i <= MIN(number_of_slides - 1, center_index + pf_cfg.num_slides); i++)
    {
        if (!surface(i))
        {
            scene_drawn = false;
#ifdef SIMULATOR
            pf_scene_waits++;
#endif
            buf_ctx_unlock();
            return;
        }
    }
#ifdef HAVE_LCD_COLOR
    mylcd_set_background(pf_bg_color);
#else
    mylcd_set_background(G_BRIGHT(0));
#endif
    /* TODO: Optimizes this by e.g. invalidating rects */
    pf_clear_display();

    int nleft = pf_cfg.num_slides;
    int nright = pf_cfg.num_slides;

    int alpha;
    int index;
    if (step == 0) {
        /* no animation, boring plain rendering */
        for (index = nleft - 2; index >= 0; index--) {
            alpha = (index < nleft - 2) ? 256 : 128;
            alpha -= extra_fade;
            if (alpha > 0 )
                render_slide(&left_slides[index], alpha);
        }
        for (index = nright - 2; index >= 0; index--) {
            alpha = (index < nright - 2) ? 256 : 128;
            alpha -= extra_fade;
            if (alpha > 0 )
                render_slide(&right_slides[index], alpha);
        }
    } else {
        /* the first and last slide must fade in/fade out */

        /* Check if the transitioning slide will be re-rendered later for
         * z-order correction.  If so, skip its first render to avoid
         * drawing an entire slide that gets immediately overwritten. */
        bool skip_right_0 = false, skip_left_0 = false;
        if (step > 0) {
            PFreal cd = (center_slide.cx >= 0) ? center_slide.cx
                                                : -center_slide.cx;
            PFreal td = (right_slides[0].cx >= 0) ? right_slides[0].cx
                                                    : -right_slides[0].cx;
            skip_right_0 = (td < cd);
        } else if (step < 0) {
            PFreal cd = (center_slide.cx >= 0) ? center_slide.cx
                                                : -center_slide.cx;
            PFreal td = (left_slides[0].cx >= 0) ? left_slides[0].cx
                                                   : -left_slides[0].cx;
            skip_left_0 = (td < cd);
        }

        /* if step<0 and nleft==1, left_slides[0] is fading in  */
        alpha = ((step > 0) ? 0 : ((nleft == 1) ? 256 : 128)) - fade / 2;
        for (index = nleft - 1; index >= 0; index--) {
            if (index == 0 && skip_left_0) {
                alpha += 128;
                if (alpha > 256) alpha = 256;
                continue;
            }
            if (alpha > 0)
                render_slide(&left_slides[index], alpha);
            alpha += 128;
            if (alpha > 256) alpha = 256;
        }
        /* if step>0 and nright==1, right_slides[0] is fading in  */
        alpha = ((step > 0) ? ((nright == 1) ? 128 : 0) : -64) + fade / 2;
        for (index = nright - 1; index >= 0; index--) {
            if (index == 0 && skip_right_0) {
                alpha += 128;
                if (alpha > 256) alpha = 256;
                continue;
            }
            if (alpha > 0)
                render_slide(&right_slides[index], alpha);
            alpha += 128;
            if (alpha > 256) alpha = 256;
        }
    }
    alpha = 256;
    if (step != 0 && pf_cfg.num_slides <= 2) /* fading out center slide */
        alpha = (step > 0) ? 256 - fade / 2 : 128 + fade / 2;
#ifdef PF_RETAIL_FLIP
    if (!pf_ipod_engine_enabled() || retail_flip_position < 256)
#endif
        render_slide(&center_slide, alpha);
    /* During animation, re-render the transitioning slide on top once
     * it is closer to screen center than the outgoing center slide.
     * This gives a smooth z-order transition instead of a sudden flip. */
    if (step > 0) {
        PFreal cd = (center_slide.cx >= 0) ? center_slide.cx : -center_slide.cx;
        PFreal td = (right_slides[0].cx >= 0) ? right_slides[0].cx
                                               : -right_slides[0].cx;
        if (td < cd)
            render_slide(&right_slides[0], 256);
    } else if (step < 0) {
        PFreal cd = (center_slide.cx >= 0) ? center_slide.cx : -center_slide.cx;
        PFreal td = (left_slides[0].cx >= 0) ? left_slides[0].cx
                                              : -left_slides[0].cx;
        if (td < cd)
            render_slide(&left_slides[0], 256);
    }
    buf_ctx_unlock();
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled() && pf_state == pf_cover_out)
        draw_retail_labels();
#endif
}


/**
  Updates the animation effect. Call this periodically from a timer.
*/
static void update_scroll_animation(void)
{
    long now;
    long elapsed;

    if (step == 0)
        return;

    now = *rb->current_tick;
    elapsed = now - scroll_animation_tick;
    if (elapsed <= 0)
        return;
    scroll_animation_tick = now;
    elapsed = MIN(elapsed, MAX(1, HZ / 10));

    int speed = 16384;
    int i;

    /* deaccelerate when approaching the target */
    if (true) {
        const int max = 2 * 65536;

        int fi = slide_frame;
        fi -= (target << 16);
        if (fi < 0)
            fi = -fi;
        fi = fmin(fi, max);

        int ia = IANGLE_MAX * (fi - max / 2) / (max * 2);
        int accel = 16384 * (PFREAL_ONE + fsin(ia)) / PFREAL_ONE;
        speed = (512 * pf_cfg.transition_speed / 100
              + accel * pf_cfg.scroll_speed / 100) / 10;
    }

    slide_frame += speed * elapsed * step;

    int index = slide_frame >> 16;
    int pos = slide_frame & 0xffff;
    int neg = 65536 - pos;
    int tick = (step < 0) ? neg : pos;
    PFreal ftick = (tick * PFREAL_ONE) >> 16;

    /* the leftmost and rightmost slide must fade away */
    fade = pos / 256;

    if (step < 0)
        index++;
    if (center_index != index) {
        center_index = index;
        rb->queue_post(&thread_q, EV_WAKEUP, 0);
        slide_frame = index << 16;
        /* Recalculate pos/tick/ftick/fade for the snapped slide_frame.
         * Without this, stale pre-snap values cause a one-frame alpha
         * discontinuity (flash) at boundary crossings. */
        pos = (step < 0) ? 65535 : 0;
        neg = 65536 - pos;
        tick = (step < 0) ? neg : pos;
        ftick = (tick * PFREAL_ONE) >> 16;
        fade = pos / 256;
        center_slide.slide_index = center_index;
        for (i = 0; i < pf_cfg.num_slides; i++)
            left_slides[i].slide_index = center_index - 1 - i;
        for (i = 0; i < pf_cfg.num_slides; i++)
            right_slides[i].slide_index = center_index + 1 + i;
    }

    center_slide.angle = (step * tick * itilt) >> 16;
    center_slide.cx = -step * fmul(offsetX, ftick);
    center_slide.cy = fmul(offsetY, ftick);

    if (center_index == target) {
        reset_slides();
        pf_state = pf_idle;
        slide_frame = center_index << 16;
        step = 0;
        fade = 256;
        return;
    }

    for (i = 0; i < pf_cfg.num_slides; i++) {
        struct slide_data *si = &left_slides[i];
        si->angle = itilt;
        si->cx =
            -(offsetX + auto_slide_spacing * i + step
                        * fmul(auto_slide_spacing, ftick));
        si->cy = offsetY;
    }

    for (i = 0; i < pf_cfg.num_slides; i++) {
        struct slide_data *si = &right_slides[i];
        si->angle = -itilt;
        si->cx =
            offsetX + auto_slide_spacing * i - step
                      * fmul(auto_slide_spacing, ftick);
        si->cy = offsetY;
    }

    if (step > 0) {
        PFreal ftick = (neg * PFREAL_ONE) >> 16;
        right_slides[0].angle = -(neg * itilt) >> 16;
        right_slides[0].cx = fmul(offsetX, ftick);
        right_slides[0].cy = fmul(offsetY, ftick);
    } else {
        PFreal ftick = (pos * PFREAL_ONE) >> 16;
        left_slides[0].angle = (pos * itilt) >> 16;
        left_slides[0].cx = -fmul(offsetX, ftick);
        left_slides[0].cy = fmul(offsetY, ftick);
    }

    /* must change direction ? */
    if (target < index)
        if (step > 0)
            step = -1;
    if (target > index)
        if (step < 0)
            step = 1;
}


/**
  Cleanup the plugin
*/
static void cleanup(void)
{
    wants_to_quit = true;
    free_borrowed_tracks();

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(false);
#endif
    end_pf_thread();
#ifdef SIMULATOR
    flush_trace();
#endif

    /* Turn on backlight timeout (revert to settings) */
    backlight_use_settings();

#ifdef USEGSLIB
    grey_release();
#endif
}

static void skip_animation_to_show_tracks(void);
static void adjust_album_display_for_setting(int old_val, int new_val)
{
    if (old_val == new_val)
        return;

    reset_track_list();
    recalc_offsets();
    reset_slides();

    if (pf_state == pf_show_tracks)
        skip_animation_to_show_tracks();
}

static int display_settings_menu(void)
{
    int selection = 0;
    int old_val;

    MENUITEM_STRINGLIST(display_menu, ID2P(LANG_DISPLAY), NULL,
                        ID2P(LANG_DISPLAY_FPS),
                        ID2P(LANG_CENTRE_MARGIN),
                        ID2P(LANG_NUMBER_OF_SLIDES),
                        ID2P(LANG_ZOOM),
                        ID2P(LANG_SPACING),
                        ID2P(LANG_RESIZE_COVERS),
                        "Scroll Speed %",
                        "Transition Speed %",
                        "Text Crossfade",
                        "Reduced Motion");

    do {
        selection=rb->do_menu(&display_menu, &selection, NULL, false);
        switch(selection) {
            case 0:
                old_val = pf_cfg.show_fps;
                rb->set_bool(rb->str(LANG_DISPLAY_FPS), &pf_cfg.show_fps);
                if (old_val != pf_cfg.show_fps)
                    reset_track_list();
                break;
            case 1:
                old_val = pf_cfg.center_margin;
                rb->set_int(rb->str(LANG_CENTRE_MARGIN), "", 1,
                            &pf_cfg.center_margin,
                            NULL, 1, 0, 80, NULL );
                adjust_album_display_for_setting(old_val, pf_cfg.center_margin);
                break;
            case 2:
                old_val = pf_cfg.slide_tuck;
                rb->set_int(rb->str(LANG_NUMBER_OF_SLIDES), "", 1,
                        &pf_cfg.slide_tuck, NULL, 1, 0,
                        DISPLAY_WIDTH / 2, NULL );
                adjust_album_display_for_setting(old_val, pf_cfg.slide_tuck);
                break;
            case 3:
                old_val = pf_cfg.zoom;
                rb->set_int(rb->str(LANG_ZOOM), "", 1, &pf_cfg.zoom,
                            NULL, 1, 10, 300, NULL );
                adjust_album_display_for_setting(old_val, pf_cfg.zoom);
                break;
            case 4:
                old_val = pf_cfg.parallel_slides;
                rb->set_bool(rb->str(LANG_SPACING), &pf_cfg.parallel_slides);
                adjust_album_display_for_setting(old_val,
                                                 pf_cfg.parallel_slides);
                break;
            case 5:
                old_val = pf_cfg.resize;
                rb->set_bool(rb->str(LANG_RESIZE_COVERS), &pf_cfg.resize);
                if (old_val == pf_cfg.resize) /* changed? */
                    break;
                else if (!rb->yesno_pop_confirm(ID2P(LANG_RESIZE_COVERS)))
                {
                    pf_cfg.resize = old_val;
                    break;
                }

                pf_cfg.update_albumart = false;
                pf_cfg.cache_version = CACHE_REBUILD;
                rb->remove(EMPTY_SLIDE);
                configfile_save(CONFIG_FILE, config,
                                CONFIG_NUM_ITEMS, CONFIG_VERSION);
                return -3; /* re-init */
            case 6:
                rb->set_int("Scroll Speed %", "", 1,
                            &pf_cfg.scroll_speed,
                            NULL, 25, 100, 400, NULL );
                break;
            case 7:
                rb->set_int("Transition Speed %", "", 1,
                            &pf_cfg.transition_speed,
                            NULL, 25, 100, 400, NULL );
                break;
            case 8:
                rb->set_bool("Text Crossfade", &pf_cfg.text_crossfade);
                break;
            case 9:
                rb->set_bool("Reduced Motion", &pf_cfg.reduced_motion);
                break;
            case MENU_ATTACHED_USB:
                return PLUGIN_USB_CONNECTED;
        }
    } while (selection >= 0);

    return 0;
}

/**
  Shows the settings menu
 */
static int settings_menu(void)
{
    int selection = 0;
    int old_val, result;

    MENUITEM_STRINGLIST(settings_menu, "PictureFlow Settings", NULL,
                        ID2P(LANG_SHOW_ALBUM_TITLE),
                        ID2P(LANG_SHOW_YEAR_IN_ALBUM_TITLE),
                        ID2P(LANG_YEAR_SORT_ORDER),
                        ID2P(LANG_WPS_INTEGRATION),
                        ID2P(LANG_DISPLAY));

    static const struct opt_items album_name_options[] = {
        { STR(LANG_HIDE_ALBUM_TITLE_NEW) },
        { STR(LANG_SHOW_AT_THE_BOTTOM_NEW) },
        { STR(LANG_SHOW_AT_THE_TOP_NEW) },
        { STR(LANG_SHOW_ALL_AT_THE_TOP) },
        { STR(LANG_SHOW_ALL_AT_THE_BOTTOM) },
    };
    static const struct opt_items year_sort_order_options[] = {
        { STR(LANG_ASCENDING) },
        { STR(LANG_DESCENDING) }
    };
    static const struct opt_items wps_options[] = {
        { STR(LANG_OFF) },
        { STR(LANG_DIRECT) },
        { STR(LANG_VIA_TRACK_LIST) }
    };

    do {
        selection=rb->do_menu(&settings_menu,&selection, NULL, false);
        switch(selection) {
            case 0:
                old_val = pf_cfg.show_album_name;
                rb->set_option(rb->str(LANG_SHOW_ALBUM_TITLE),
                      &pf_cfg.show_album_name, RB_INT, album_name_options, 5, NULL);
                adjust_album_display_for_setting(old_val, pf_cfg.show_album_name);
                break;
            case 1:
                rb->set_bool(rb->str(LANG_SHOW_YEAR_IN_ALBUM_TITLE), &pf_cfg.show_year);
                break;
            case 2:
                old_val = pf_cfg.year_sort_order;
                rb->set_option(rb->str(LANG_YEAR_SORT_ORDER),
                      &pf_cfg.year_sort_order, RB_INT, year_sort_order_options, 2, NULL);
                if (old_val != pf_cfg.year_sort_order &&
                    !sort_albums(pf_cfg.sort_albums_by, true))
                    pf_cfg.year_sort_order = old_val;
                break;
            case 3:
                rb->set_option(rb->str(LANG_WPS_INTEGRATION),
                               &pf_cfg.auto_wps, RB_INT, wps_options, 3, NULL);
                break;
            case 4:
                if ((result = display_settings_menu()))
                    return result;
                break;
            case MENU_ATTACHED_USB:
                return PLUGIN_USB_CONNECTED;
        }
    } while ( selection >= 0 );

    return 0;
}

/**
  Show the main menu
 */
enum {
    PF_SORT_ALBUMS_BY,
    PF_SHOW_TRACKS_WHILE_BROWSING,
    PF_GOTO_LAST_ALBUM,
    PF_GOTO_WPS,
#if PF_PLAYBACK_CAPABLE
    PF_MENU_PLAYBACK_CONTROL,
#endif
    PF_REBUILD_CACHE,
    PF_UPDATE_CACHE,
    PF_MENU_SETTINGS,
    PF_MENU_QUIT,
};

static int main_menu(void)
{
    int selection = 0;
    int result, curr_album, old_val;

#if LCD_DEPTH > 1
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(pf_fg_color);
#else
    rb->lcd_set_foreground(N_BRIGHT(255));
#endif
#endif

    MENUITEM_STRINGLIST(main_menu, "PictureFlow", NULL,
                        ID2P(LANG_SORT_ALBUMS_BY),
                        ID2P(LANG_SHOW_TRACKS_WHILE_BROWSING),
                        ID2P(LANG_GOTO_LAST_ALBUM),
                        ID2P(LANG_GOTO_WPS),
#if PF_PLAYBACK_CAPABLE
                        ID2P(LANG_PLAYBACK_CONTROL),
#endif
                        ID2P(LANG_REBUILD_CACHE),
                        ID2P(LANG_UPDATE_CACHE),
                        ID2P(LANG_SETTINGS),
                        ID2P(LANG_MENU_QUIT));

    static const struct opt_items sort_options[] = {
        { STR(LANG_ARTIST_PLUS_NAME) },
        { STR(LANG_ARTIST_PLUS_YEAR) },
        { STR(LANG_ID3_YEAR) },
        { STR(LANG_NAME) }};

    while (1)  {
        switch (rb->do_menu(&main_menu,&selection, NULL, false)) {
            case PF_SORT_ALBUMS_BY:
                old_val = pf_cfg.sort_albums_by;
                rb->set_option(rb->str(LANG_SORT_ALBUMS_BY),
                      &pf_cfg.sort_albums_by, RB_INT, sort_options, 4, NULL);
                if (old_val != pf_cfg.sort_albums_by &&
                    !sort_albums(pf_cfg.sort_albums_by, true))
                    pf_cfg.sort_albums_by = old_val;
                if (old_val == pf_cfg.sort_albums_by)
                    break;
                return 0;
            case PF_SHOW_TRACKS_WHILE_BROWSING:
                if (pf_state != pf_show_tracks)
                {
                    if (pf_state == pf_scrolling)
                        set_current_slide(target);

                    skip_animation_to_show_tracks();
                }
                show_tracks_while_browsing = true;
                return 0;
            case PF_GOTO_LAST_ALBUM:
                if (pf_state == pf_scrolling)
                    curr_album = target;
                else
                    curr_album = center_index;

                if (pf_state == pf_show_tracks)
                    free_borrowed_tracks();
                if (pf_state == pf_show_tracks ||
                    pf_state == pf_cover_in ||
                    pf_state == pf_cover_out)
                    skip_animation_to_idle_state();

                set_current_slide(pf_cfg.last_album);
                pf_cfg.last_album = curr_album;

                pf_state = pf_idle;
                return 0;
            case PF_GOTO_WPS: /* WPS */
                return -2;
#if PF_PLAYBACK_CAPABLE
            case PF_MENU_PLAYBACK_CONTROL: /* Playback Control */
                playback_control(NULL);
                break;
#endif
            case PF_REBUILD_CACHE:
                if (!rb->yesno_pop_confirm(ID2P(LANG_REBUILD_CACHE)))
                    break;
                pf_cfg.update_albumart = false;
                pf_cfg.cache_version = CACHE_REBUILD;
                rb->remove(EMPTY_SLIDE);
                configfile_save(CONFIG_FILE, config,
                                CONFIG_NUM_ITEMS, CONFIG_VERSION);
                return -3; /* re-init */
            case PF_UPDATE_CACHE:
                if (!rb->yesno_pop_confirm(ID2P(LANG_UPDATE_CACHE)))
                    break;
                pf_cfg.update_albumart = true;
                pf_cfg.cache_version = CACHE_REBUILD;
                rb->remove(EMPTY_SLIDE);
                configfile_save(CONFIG_FILE, config,
                                CONFIG_NUM_ITEMS, CONFIG_VERSION);
                return -3; /* re-init */
            case PF_MENU_SETTINGS:
                result = settings_menu();
                if (result != 0)
                    return result;
                break;
            case PF_MENU_QUIT:
                return -1;

            case MENU_ATTACHED_USB:
                return PLUGIN_USB_CONNECTED;

            default:
                return 0;
        }
    }
}

/* iPod Classic 6G custom: faster cover animation (~2x)
   Same total movement, half the frames */
#define ZOOMIN_FRAME_COUNT  10
#define ZOOMIN_FRAME_DIST   -10
#define ZOOMIN_FRAME_ANGLE  2
#define ZOOMIN_FRAME_FADE   25

#define ROTATE_FRAME_COUNT  8
#define ROTATE_FRAME_ANGLE  30

#define KEYFRAME_COUNT ZOOMIN_FRAME_COUNT + ROTATE_FRAME_COUNT
static void skip_animation_to_idle_state(void);

/**
   Animation step for zooming into the current cover
 */
static void update_cover_in_animation(void)
{
    long now = *rb->current_tick;
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
    {
        if (!retail_flip_active)
        {
            cover_animation_tick = now;
            retail_flip_remainder = 0;
        }
        retail_flip_active = true;
        long elapsed = MAX(0, now - cover_animation_tick);
        cover_animation_tick = now;
        long scaled = elapsed * 1024 + retail_flip_remainder;
        int duration = MAX(1, HZ * 3 / 10);
        retail_flip_remainder = scaled % duration;
        retail_flip_position = MIN(1024, retail_flip_position +
                                  scaled / duration);
        extra_fade = 0;
        center_slide.distance = 0;
        center_slide.angle = -MIN(256, retail_flip_position);
        if (retail_flip_position == 1024)
        {
            retail_flip_active = false;
            pf_state = pf_show_tracks;
            retail_marquee_tick = now;
        }
        return;
    }
#endif

    if (now == cover_animation_tick)
        return;
    cover_animation_tick = now;
    cover_animation_keyframe++;

    if(cover_animation_keyframe <= ZOOMIN_FRAME_COUNT)
    {
        center_slide.distance += ZOOMIN_FRAME_DIST;
        center_slide.angle +=    ZOOMIN_FRAME_ANGLE;
        extra_fade +=            ZOOMIN_FRAME_FADE;
    }
    else if(cover_animation_keyframe <= KEYFRAME_COUNT)
        center_slide.angle += ROTATE_FRAME_ANGLE;
    else
    {
        cover_animation_keyframe = 0;
        pf_state = pf_show_tracks;
    }
}

/**
   Animation step for zooming out the current cover
 */
static void update_cover_out_animation(void)
{
    if (pf_cfg.reduced_motion)
    {
        skip_animation_to_idle_state();
        return;
    }
    long now = *rb->current_tick;
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
    {
        if (!retail_flip_active)
        {
            cover_animation_tick = now;
            retail_flip_remainder = 0;
            retail_close_clock = retail_flip_position >= 256 ?
                768 + (retail_flip_position - 256) / 3 :
                retail_flip_position * 3;
        }
        retail_flip_active = true;
        long elapsed = MAX(0, now - cover_animation_tick);
        cover_animation_tick = now;
        long scaled = elapsed * 1024 + retail_flip_remainder;
        int duration = MAX(1, HZ * 3 / 10);
        retail_flip_remainder = scaled % duration;
        /* The source closing sequence turns the back face away in the
         * first quarter, then settles the cover over the remaining time. */
        retail_close_clock = MAX(0, retail_close_clock - scaled / duration);
        retail_flip_position = retail_close_clock >= 768 ?
            256 + (retail_close_clock - 768) * 3 : retail_close_clock / 3;
        extra_fade = 0;
        center_slide.distance = 0;
        center_slide.angle = -MIN(256, retail_flip_position);
        if (retail_flip_position == 0)
        {
            retail_flip_active = false;
            pf_state = pf_idle;
        }
        return;
    }
#endif

    if (now == cover_animation_tick)
        return;
    cover_animation_tick = now;
    cover_animation_keyframe++;

    if(cover_animation_keyframe <= ROTATE_FRAME_COUNT)
        center_slide.angle -= ROTATE_FRAME_ANGLE;
    else if(cover_animation_keyframe <= KEYFRAME_COUNT)
    {
        center_slide.distance -= ZOOMIN_FRAME_DIST;
        center_slide.angle -=    ZOOMIN_FRAME_ANGLE;
        extra_fade -=            ZOOMIN_FRAME_FADE;
    }
    else
    {
        cover_animation_keyframe = 0;
        pf_state = pf_idle;
    }
}

/**
   Immediately show tracks and skip any animation frames
*/
static void skip_animation_to_show_tracks(void)
{
    pf_state = pf_show_tracks;
    cover_animation_keyframe = 0;

    extra_fade =            ZOOMIN_FRAME_COUNT * ZOOMIN_FRAME_FADE;
    center_slide.distance = ZOOMIN_FRAME_COUNT * ZOOMIN_FRAME_DIST;
    center_slide.angle   = (ZOOMIN_FRAME_COUNT * ZOOMIN_FRAME_ANGLE) +
                           (ROTATE_FRAME_COUNT * ROTATE_FRAME_ANGLE);
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
    {
        retail_flip_position = 1024;
        retail_flip_active = false;
        extra_fade = 0;
        center_slide.distance = 0;
        center_slide.angle = 256;
    }
#endif
}

/**
   Immediately transition to idle state and skip any animation frames
*/
static void skip_animation_to_idle_state(void)
{
    pf_state = pf_idle;
    cover_animation_keyframe = 0;
    extra_fade = 0;
#ifdef PF_RETAIL_FLIP
    retail_flip_position = 0;
    retail_flip_active = false;
#endif
    set_current_slide(center_index);
}

/**
  Change direction during cover in/out animation
*/
static void reverse_animation(void)
{
    pf_state = pf_state == pf_cover_out ? pf_cover_in : pf_cover_out;
    cover_animation_keyframe = KEYFRAME_COUNT - cover_animation_keyframe;
    cover_animation_tick = *rb->current_tick;
#ifdef PF_RETAIL_FLIP
    retail_flip_active = true;
    retail_close_clock = retail_flip_position >= 256 ?
        768 + (retail_flip_position - 256) / 3 : retail_flip_position * 3;
    retail_flip_remainder = 0;
#endif
}

/**
   Draw the selection highlight bar at y with height h
 */
static inline void draw_gradient(int y, int h)
{
#ifdef HAVE_LCD_COLOR
    mylcd_set_foreground(pf_lss_color);
    mylcd_fillrect(0, y, LCD_WIDTH, h);
#else
    int r, inc, c;
    inc = (100 << 8) / h;
    c = 0;
    for (r = 0; r < h; r++) {
        int bright = (r > h/2) ? (h - r) : r;
        bright = bright * 255 / (h/2);
        mylcd_set_foreground(G_BRIGHT(160 * bright / 255));
        mylcd_hline(0, LCD_WIDTH, r + y);
    }
#endif
}


static void track_list_yh(int char_height)
{
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
    {
        pf_tracks.list_y = PF_PANEL_TOP + PF_PANEL_HEADER;
        pf_tracks.list_h = pf_height - pf_tracks.list_y;
        return;
    }
#endif
    bool needs_space = pf_cfg.show_fps || aa_cache.inspected < pf_idx.album_ct;

    switch (pf_cfg.show_album_name)
    {
        case ALBUM_NAME_HIDE:
            pf_tracks.list_y = (needs_space ? char_height : 0);
            pf_tracks.list_h = pf_height - pf_tracks.list_y;
            break;
        case ALBUM_NAME_BOTTOM:
            pf_tracks.list_y = (needs_space ? char_height : 0);
            pf_tracks.list_h = pf_height - pf_tracks.list_y - (char_height * 3);
            break;
        case ALBUM_AND_ARTIST_TOP:
            pf_tracks.list_y = char_height * 3;
            pf_tracks.list_h = pf_height - pf_tracks.list_y -
                           (needs_space ? char_height : 0);
            break;
        case ALBUM_AND_ARTIST_BOTTOM:
            pf_tracks.list_y = (needs_space ? char_height : 0);
            pf_tracks.list_h = pf_height - pf_tracks.list_y - (char_height * 3);
            break;
        case ALBUM_NAME_TOP:
        default:
            pf_tracks.list_y = char_height * 3;
            pf_tracks.list_h = pf_height - pf_tracks.list_y -
                           (needs_space ? char_height : 0);
            break;
    }
}

/**
    Reset the track list after a album change
 */
void reset_track_list(void)
{
#ifdef PF_RETAIL_FLIP
    retail_text_album = -1;
    retail_marquee_album = -1;
#endif
    int char_height = rb->screens[SCREEN_MAIN]->getcharheight();
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
        char_height = PF_TRACK_ROW;
#endif
    int total_height;
    track_list_yh(char_height);
    pf_tracks.list_visible =
                         fmin( pf_tracks.list_h/char_height , pf_tracks.count );
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
        pf_tracks.list_visible = MIN(pf_tracks.count,
            (pf_tracks.list_h + PF_TRACK_ROW - 1) / PF_TRACK_ROW);
#endif

    pf_tracks.list_start = 0;
    pf_tracks.sel = 0;
    pf_tracks.last_sel = -1;

    /* let the tracklist start more centered
     * if the screen isn't filled with tracks */
    total_height = pf_tracks.count*char_height;
    if (total_height < pf_tracks.list_h
#ifdef PF_RETAIL_FLIP
        && !pf_ipod_engine_enabled()
#endif
       )
    {
        pf_tracks.list_y += (pf_tracks.list_h - total_height) / 2;
        pf_tracks.list_h = total_height;
    }
}

static void draw_album_text(void);
#ifdef PF_RETAIL_FLIP
/* Match an already-loaded font against its on-disk metrics. No font_load():
 * entering Cover Flow during playback must not allocate a core glyph cache. */
static int retail_loaded_font(const char *name)
{
    char path[MAX_PATH];
    uint32_t header[9];
    rb->snprintf(path, sizeof(path), ROCKBOX_DIR
                 "/ipodjs/apple/retailos-fonts/%s", name);
    int fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return rb->screens[SCREEN_MAIN]->getuifont();
    bool valid = rb->read(fd, header, sizeof(header)) == sizeof(header);
    rb->close(fd);
    if (valid)
    {
        for (int i = 1; i < MAXFONTS; i++)
        {
            struct font *font = rb->font_get(i);
            if (font->height == (letoh32(header[1]) >> 16) &&
                font->maxwidth == (int)(letoh32(header[1]) & 65535) &&
                font->size == (int)letoh32(header[5]) &&
                font->bits_size == (int32_t)letoh32(header[6]))
                return i;
        }
    }
    return rb->screens[SCREEN_MAIN]->getuifont();
}

static pix_t retail_blend(pix_t background, pix_t foreground, unsigned alpha)
{
    unsigned inverse = 15 - alpha;
    return LCD_RGBPACK(
        (RGB_UNPACK_RED(background) * inverse +
         RGB_UNPACK_RED(foreground) * alpha) / 15,
        (RGB_UNPACK_GREEN(background) * inverse +
         RGB_UNPACK_GREEN(foreground) * alpha) / 15,
        (RGB_UNPACK_BLUE(background) * inverse +
         RGB_UNPACK_BLUE(foreground) * alpha) / 15);
}

static void draw_retail_labels(void)
{
    if (retail_label_album != center_index)
        return;
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 320; x++)
        {
            unsigned pos = y * 320 + x;
            unsigned a = (retail_labels[pos / 2] >> ((pos & 1) * 4)) & 15;
            if (a)
            {
                unsigned dest = (174 + y) * BUFFER_WIDTH + x;
                buffer[dest] = retail_blend(buffer[dest], pf_fg_color, a);
            }
        }
}

/* Service-only rasterization. The packed mask is independent of the core
 * glyph cache, so subsequent eviction cannot cause an animation-time read. */
static void retail_mask_text(unsigned char *mask, int width, int height,
                             int x, int y, int right, int font_id,
                             const char *text)
{
    int last_base_width = 0;
    const ucschar_t *cursor = rb->bidi_l2v((const unsigned char *)text, 1);
    for (; *cursor && x < right; cursor++)
    {
        ucschar_t code = *cursor;
        struct font *font = rb->font_get(font_id);
        int glyph_width = rb->font_get_width(font, code);
        bool rtl;
        bool mark = rb->is_diacritic(code, &rtl);
        int base_width = glyph_width;
        if (rtl && mark)
        {
            const ucschar_t *next = cursor + 1;
            bool ignored;
            while (*next && rb->is_diacritic(*next, &ignored))
                next++;
            base_width = *next ? rb->font_get_width(font, *next) : 0;
        }
        else if (!rtl)
        {
            if (!mark)
                last_base_width = glyph_width;
            base_width = last_base_width;
        }
        int glyph_x = x + (mark ? (base_width - glyph_width) / 2 : 0);
        const unsigned char *bits = rb->font_get_bits(font, code);
        for (unsigned gy = 0; gy < font->height && y + (int)gy < height; gy++)
        {
            if (y + (int)gy < 0)
                continue;
            for (int gx = 0; gx < glyph_width && glyph_x + gx < right; gx++)
            {
                if (glyph_x + gx < 0 || glyph_x + gx >= width)
                    continue;
                unsigned a;
                if (font->depth)
                {
                    unsigned source = gy * glyph_width + gx;
                    a = 15 - ((bits[source / 2] >> ((source & 1) * 4)) & 15);
                }
                else
                    a = (bits[(gy / 8) * glyph_width + gx] &
                         (1 << (gy & 7))) ? 15 : 0;
                unsigned dest = (y + gy) * width + glyph_x + gx;
                unsigned shift = (dest & 1) * 4;
                unsigned old = (mask[dest / 2] >> shift) & 15;
                mask[dest / 2] = (mask[dest / 2] & ~(15 << shift)) |
                                     (MAX(a, old) << shift);
            }
        }
        bool next_rtl;
        bool next_mark = rb->is_diacritic(cursor[1], &next_rtl);
        if ((rtl && !mark) || (!rtl && (!next_mark || next_rtl)))
            x += base_width;
    }
}

static int retail_text_width(const char *text, int font)
{
    return rb->font_getstringsize((const unsigned char *)text, NULL, NULL, font);
}

static void retail_mask_clipped(unsigned char *mask, int width, int height,
                                int x, int y, int right, int font,
                                const char *text)
{
    if (retail_text_width(text, font) <= right - x)
    {
        retail_mask_text(mask, width, height, x, y, right, font, text);
        return;
    }
    char clipped[MAX_PATH + 4];
    const unsigned char *start = (const unsigned char *)text, *cursor = start;
    int used = 0, pixels = 0;
    int room = right - x - retail_text_width("…", font);
    while (*cursor)
    {
        ucschar_t code;
        const unsigned char *next = rb->utf8decode(cursor, &code);
        int glyph_width = rb->font_get_width(rb->font_get(font), code);
        if (pixels + glyph_width > room || next - start >= MAX_PATH)
            break;
        used = next - start;
        pixels += glyph_width;
        cursor = next;
    }
    rb->memcpy(clipped, text, used);
    rb->strcpy(clipped + used, "…");
    retail_mask_text(mask, width, height, x, y, right, font, clipped);
}

static int retail_marquee_offset(void)
{
    int span = retail_marquee_width - retail_marquee_limit;
    long elapsed = *rb->current_tick - retail_marquee_tick;
    if (span <= 0 || elapsed <= HZ || pf_state != pf_show_tracks)
        return 0;
    long travel = MAX(1, span * HZ / 30);
    long phase = (elapsed - HZ) % (2 * travel + 2 * HZ);
    if (phase < travel)
        return phase * span / travel;
    if (phase < travel + HZ)
        return span;
    if (phase < 2 * travel + HZ)
        return span - (phase - travel - HZ) * span / travel;
    return 0;
}

static void prepare_retail_header(void)
{
    retail_body_font = retail_loaded_font("19-Helvetica-Bold-RetailOS-Apple.fnt");
    retail_title_font = retail_loaded_font("19-Helvetica-Bold-RetailOS-Apple.fnt");
    retail_detail_font = retail_loaded_font("15-Helvetica-RetailOS-Apple.fnt");
    rb->memset(retail_labels, 0, sizeof(retail_labels));
    retail_mask_text(retail_labels, 320, 20, 4, 1, 260,
                     retail_loaded_font("15-Helvetica-Bold-RetailOS-Apple.fnt"),
                     "Cover Flow");
    for (int i = 0; i < 320 * 20; i++)
    {
        unsigned alpha = (retail_labels[i / 2] >> ((i & 1) * 4)) & 15;
        if (!retail_header_valid)
            retail_header[i] = LCD_WHITE;
        if (alpha)
            retail_header[i] = retail_blend(retail_header[i], LCD_BLACK, alpha);
    }
    retail_header_valid = true;
    rb->memset(retail_labels, 0, sizeof(retail_labels));
}

static void service_retail_text(void)
{
    if (retail_label_album != center_index)
    {
        const char *album = get_album_name(center_index);
        const char *artist = get_album_artist(center_index);
        rb->memset(retail_labels, 0, sizeof(retail_labels));
        retail_mask_clipped(retail_labels, 320, 40,
            MAX(4, (320 - retail_text_width(album, retail_title_font)) / 2),
            0, 316, retail_title_font, album);
        retail_mask_clipped(retail_labels, 320, 40,
            MAX(4, (320 - retail_text_width(artist, retail_title_font)) / 2),
            20, 316, retail_title_font, artist);
        retail_label_album = center_index;
    }
    if (pf_tracks.cur_idx != center_index)
        return;
    if (retail_marquee_selected != pf_tracks.sel ||
        retail_marquee_album != center_index)
    {
        const char *title = get_track_name(pf_tracks.sel);
        char duration[16];
        unsigned seconds = pf_tracks.index[pf_tracks.sel].duration;
        rb->snprintf(duration, sizeof(duration), "%u:%02u", seconds / 60,
                     seconds % 60);
        retail_marquee_limit = 236 - retail_text_width(duration, retail_body_font);
        retail_marquee_width = MIN(PF_MARQUEE_WIDTH,
                                  retail_text_width(title, retail_body_font));
        rb->memset(retail_marquee, 0, sizeof(retail_marquee));
        retail_mask_clipped(retail_marquee, PF_MARQUEE_WIDTH, PF_TRACK_ROW, 0, 0,
                            PF_MARQUEE_WIDTH, retail_body_font, title);
        retail_marquee_selected = pf_tracks.sel;
        retail_marquee_album = center_index;
        retail_marquee_tick = *rb->current_tick;
    }
    if (retail_text_album == center_index &&
        retail_text_start == pf_tracks.list_start)
        return;
    rb->memset(retail_text, 0, sizeof(retail_text));
    retail_mask_clipped(retail_text, PF_FACE_WIDTH, 212, 8, 8, 248,
                     retail_title_font, get_album_name(center_index));
    retail_mask_clipped(retail_text, PF_FACE_WIDTH, 212, 8, 29, 248,
                     retail_detail_font, get_album_artist(center_index));
    for (int row = 0; row < pf_tracks.list_visible; row++)
    {
        int track = pf_tracks.list_start + row;
        char duration[16];
        unsigned seconds = pf_tracks.index[track].duration;
        rb->snprintf(duration, sizeof(duration), "%u:%02u", seconds / 60,
                     seconds % 60);
        int time_x = 250 - retail_text_width(duration, retail_body_font);
        retail_mask_clipped(retail_text, PF_FACE_WIDTH, 212, 8,
                         PF_PANEL_HEADER + 3 + row * PF_TRACK_ROW,
                         time_x - 6, retail_body_font, get_track_name(track));
        retail_mask_text(retail_text, PF_FACE_WIDTH, 212, time_x,
                         PF_PANEL_HEADER + 3 + row * PF_TRACK_ROW,
                         250, retail_body_font, duration);
    }
    retail_text_album = center_index;
    retail_text_start = pf_tracks.list_start;
}
#endif

static void request_album_open(void)
{
    album_open_tick = *rb->current_tick;
    cover_animation_keyframe = 0;
    cover_animation_tick = album_open_tick;
    pf_state = pf_open_pending;
#ifdef PF_RETAIL_FLIP
    retail_flip_position = 0;
    retail_flip_active = false;
#endif
}

#ifdef PF_RETAIL_FLIP
static void *face_strip_address(int x, int y)
{
    return &face_strip[y * PF_FACE_WIDTH + x];
}

/* Render the back face in 16-row strips. This keeps the perspective flip
 * below 8 KiB without allocating a second framebuffer or decoding artwork. */
static void render_retail_tracks(void)
{
    int progress = retail_flip_position - 256;
    if (progress <= 0 || pf_tracks.cur_idx != center_slide.slide_index)
        return;
    int height = PF_PANEL_HEIGHT;
    const int row_height = PF_TRACK_ROW;
    int marquee_offset = retail_marquee_offset();
    /* Perspective face grows from edge-on to its final 256-pixel panel. */
    int angle = (768 - progress) * 256 / 768;
    int cosine = fcos(angle), sine = fsin(angle);
    int radius = PF_FACE_WIDTH / 2;
    int left = -radius * CAM_DIST * cosine /
               (CAM_DIST * PFREAL_ONE - radius * sine);
    int right = radius * CAM_DIST * cosine /
                (CAM_DIST * PFREAL_ONE + radius * sine);
    int first_x = MAX(0, LCD_WIDTH / 2 + left);
    int last_x = MIN(LCD_WIDTH, LCD_WIDTH / 2 + right);
    struct viewport vp = pf_vp;
    struct frame_buffer_t fb = {
        .fb_ptr = face_strip,
        .get_address_fn = face_strip_address,
        .stride = PF_FACE_WIDTH,
        .elems = PF_FACE_WIDTH * PF_FACE_STRIP,
    };
    vp.x = 0;
    vp.y = 0;
    vp.width = PF_FACE_WIDTH;
    vp.height = PF_FACE_STRIP;
    rb->viewport_set_buffer(&vp, &fb, SCREEN_MAIN);
    for (int strip_y = 0; strip_y < height; strip_y += PF_FACE_STRIP)
    {
        rb->lcd_set_viewport(&vp);
        rb->lcd_set_drawmode(DRMODE_SOLID);
        rb->lcd_set_background(pf_bg_color);
        rb->lcd_set_foreground(pf_bg_color);
        rb->lcd_fillrect(0, 0, PF_FACE_WIDTH, PF_FACE_STRIP);
        /* Both contemporary Classic references show a shaded blue heading.
         * Do not substitute the unrelated neutral-gray resource 393. */
        for (int y = 0; y < PF_FACE_STRIP && strip_y + y < PF_PANEL_HEADER; y++)
        {
            int shade = (strip_y + y) * 255 / (PF_PANEL_HEADER - 1);
            rb->lcd_set_foreground(LCD_RGBPACK(100 - shade * 21 / 255,
                151 - shade * 22 / 255, 211 - shade * 21 / 255));
            rb->lcd_hline(0, PF_FACE_WIDTH - 1, y);
        }
        rb->lcd_set_drawmode(DRMODE_FG);
        for (int row = 0; row < pf_tracks.list_visible; row++)
        {
            int y = PF_PANEL_HEADER + row * row_height;
            if (y + row_height <= strip_y || y >= strip_y + PF_FACE_STRIP)
                continue;
            int track = retail_text_start + row;
            if (track == pf_tracks.sel)
            {
                /* Classic's selection is a shaded blue bar, not the flat
                 * theme selection used by the ordinary PictureFlow list. */
                for (int line = MAX(0, strip_y - y);
                     line < MIN(row_height, strip_y + PF_FACE_STRIP - y);
                     line++)
                {
                    int shade = line * 255 / MAX(1, row_height - 1);
                    rb->lcd_set_foreground(LCD_RGBPACK(
                        80 - shade * 60 / 255,
                        180 - shade * 45 / 255,
                        245 - shade * 25 / 255));
                    rb->lcd_hline(1, PF_FACE_WIDTH - 2, y + line - strip_y);
                }
            }
        }
        for (int sy = 0; sy < PF_FACE_STRIP && strip_y + sy < height; sy++)
        {
            int y = strip_y + sy;
            bool white = y < PF_PANEL_HEADER ||
                retail_text_start + (y - PF_PANEL_HEADER) / row_height == pf_tracks.sel;
            pix_t foreground = white ? LCD_WHITE : pf_fg_color;
            for (int x = 1; x < PF_FACE_WIDTH - 1; x++)
            {
                unsigned pos = y * PF_FACE_WIDTH + x;
                unsigned a = (retail_text[pos / 2] >> ((pos & 1) * 4)) & 15;
                int text_y = y - PF_PANEL_HEADER - 3 -
                    (pf_tracks.sel - retail_text_start) * row_height;
                if (retail_marquee_album == center_index &&
                    retail_marquee_selected == pf_tracks.sel &&
                    text_y >= 0 && text_y < PF_TRACK_ROW &&
                    x >= 8 && x < 8 + retail_marquee_limit)
                {
                    unsigned source = text_y * PF_MARQUEE_WIDTH +
                                      x - 8 + marquee_offset;
                    a = (retail_marquee[source / 2] >> ((source & 1) * 4)) & 15;
                }
                if (a)
                {
                    unsigned dest = sy * PF_FACE_WIDTH + x;
                    face_strip[dest] = retail_blend(face_strip[dest], foreground, a);
                }
            }
        }
        rb->lcd_set_foreground(LCD_RGBPACK(158, 171, 184));
        rb->lcd_vline(0, 0, PF_FACE_STRIP - 1);
        rb->lcd_vline(PF_FACE_WIDTH - 1, 0, PF_FACE_STRIP - 1);
        rb->lcd_set_viewport(&pf_vp);
        for (int x = first_x; x < last_x; x++)
        {
            /* Inverse projective mapping, not a centered width squash.
             * The near edge expands left/up while the far edge recedes;
             * glyph widths and vertical scale share the same depth. */
            int projected = x - LCD_WIDTH / 2;
            int denominator = CAM_DIST * cosine - projected * sine;
            if (denominator <= 0)
                continue;
            int source = CAM_DIST * projected * PFREAL_ONE / denominator;
            int sx = source + radius;
            if (sx < 0 || sx >= PF_FACE_WIDTH)
                continue;
            int depth = CAM_DIST * PFREAL_ONE + source * sine;
            int column_height = height * CAM_DIST * PFREAL_ONE / depth;
            int top = PF_PANEL_TOP + (height - column_height) / 2;
            int first_y = MAX(0, top +
                             (strip_y * column_height + height - 1) / height);
            int last_y = MIN(pf_height, top +
                (MIN(height, strip_y + PF_FACE_STRIP) * column_height +
                 height - 1) / height);
            for (int y = first_y; y < last_y; y++)
            {
                int sy = (y - top) * height / column_height - strip_y;
                if (sy >= 0 && sy < PF_FACE_STRIP)
                    buffer[y * BUFFER_WIDTH + x] =
                        face_strip[sy * PF_FACE_WIDTH + sx];
            }
        }
    }
    rb->lcd_set_viewport(&pf_vp);
    mylcd_set_drawmode(DRMODE_FG);
}
#endif

/**
  Display the list of tracks
 */
static bool show_track_list(void)
{
    if (center_slide.slide_index != pf_tracks.cur_idx || pf_tracks.count == 0)
        return false;
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
    {
        render_all_slides();
        if (scene_drawn)
            render_retail_tracks();
        return true;
    }
#endif
    pf_clear_display();
    int titletxt_w, titletxt_x, color, titletxt_h;
    titletxt_h = rb->screens[SCREEN_MAIN]->getcharheight();

    int titletxt_y = pf_tracks.list_y;
    int track_i;
    int fade;

    track_i = pf_tracks.list_start;
    for (; track_i < pf_tracks.list_visible + pf_tracks.list_start; track_i++)
    {
        char *trackname = get_track_name(track_i);
        if (track_i == pf_tracks.sel && !show_tracks_while_browsing) {
            if (pf_tracks.sel != pf_tracks.last_sel) {
                set_scroll_line(trackname, PF_SCROLL_TRACK);
                pf_tracks.last_sel = pf_tracks.sel;
            }
            draw_gradient(titletxt_y, titletxt_h);
            titletxt_x = get_scroll_line_offset(PF_SCROLL_TRACK);
#ifdef HAVE_LCD_COLOR
            mylcd_set_foreground(pf_lst_color);
#else
            mylcd_set_foreground(G_BRIGHT(255));
#endif
        }
        else {
            titletxt_w = mylcd_getstringsize(trackname, NULL, NULL);
            titletxt_x = (LCD_WIDTH-titletxt_w)/2;
            fade = (abs(pf_tracks.sel - track_i) * 200 / pf_tracks.count);
            color = 250 - fade;
#ifdef HAVE_LCD_COLOR
            mylcd_set_foreground(pf_color_mix(color));
#else
            mylcd_set_foreground(G_BRIGHT(color));
#endif
        }
        mylcd_putsxy(titletxt_x,titletxt_y,trackname);
        titletxt_y += titletxt_h;
    }

    return true;
}

static void select_next_track(void)
{
    if ( pf_tracks.sel < pf_tracks.count - 1 ) {
        pf_tracks.sel++;
        if (pf_tracks.sel==(pf_tracks.list_visible+pf_tracks.list_start))
            pf_tracks.list_start++;
    } else if (rb->global_settings->list_wraparound) {
        /* Rollover */
        pf_tracks.sel = 0;
        pf_tracks.list_start = 0;
    }
}

static void select_prev_track(void)
{
    if (pf_tracks.sel > 0 ) {
        if (pf_tracks.sel==pf_tracks.list_start) pf_tracks.list_start--;
        pf_tracks.sel--;
    } else if (rb->global_settings->list_wraparound) {
        /* Rolllover */
        pf_tracks.sel = pf_tracks.count - 1;
        pf_tracks.list_start = pf_tracks.count - pf_tracks.list_visible;
    }
}

static void select_next_album(void)
{
    if (center_index < number_of_slides - 1) {
        free_borrowed_tracks();
        target = center_index + 1;
        set_current_slide(target);
        request_album_open();
    }
}

static void select_prev_album(void)
{
    if (center_index > 0) {
        free_borrowed_tracks();
        target = center_index - 1;
        set_current_slide(target);
        request_album_open();
    }
}

#if PF_PLAYBACK_CAPABLE
static int show_id3_info(const char *selected_file)
{
    int i;
    unsigned long last_tick;
    const char *file_name;
    bool is_multiple_tracks = insert_whole_album && pf_tracks.count > 1;

    last_tick = *(rb->current_tick) + HZ/2;
    rb->splash_progress_set_delay(HZ / 2); /* wait 1/2 sec before progress */
    i = 0;
    do {
        file_name = i == 0 ? selected_file : get_track_filename(i);
        if (!rb->get_metadata(&id3, -1, file_name))
            return 0;

        if (is_multiple_tracks)
        {
            rb->splash_progress(i, pf_tracks.count,
                                "%s (%s)", rb->str(LANG_WAIT), rb->str(LANG_OFF_ABORT));
            if (TIME_AFTER(*(rb->current_tick), last_tick + HZ/4))
            {
                if (rb->action_userabort(TIMEOUT_NOBLOCK))
                    return 0;
                last_tick = *(rb->current_tick);
            }

            collect_id3(&id3, i == 0);
            rb->yield();
        }
    } while (++i < pf_tracks.count && is_multiple_tracks);

    if (is_multiple_tracks)
        finalize_id3(&id3);

    return rb->browse_id3(&id3, 0, 0, NULL, i, &view_text) ? PLUGIN_USB_CONNECTED : 0;
}


static bool pf_current_playlist_insert(int position, bool queue, bool create_new)
{
    if (position == PLAYLIST_REPLACE)
    {
        if ((!create_new && rb->playlist_remove_all_tracks(NULL) == 0) ||
             (create_new && rb->playlist_create(NULL, NULL) == 0))
            position = PLAYLIST_INSERT_LAST;
        else
            return false;
    }

    if (!insert_whole_album)
        rb->playlist_insert_track(NULL, get_track_filename(pf_tracks.sel),
                                        position, queue, false);
    else
    {
        int i = 0;
        do {
            rb->yield();
            if (rb->playlist_insert_track(NULL, get_track_filename(i),
                    position, queue, false) < 0)
                break;
            if (position == PLAYLIST_INSERT_FIRST)
                position = PLAYLIST_INSERT;
        } while(++i < pf_tracks.count);
    }
    rb->playlist_sync(NULL);
    old_playlist = create_new ? center_slide.slide_index : -1;
    return true;
}


static int pf_add_to_playlist(const char* playlist, bool new_playlist)
{
    int fd;
    int result = 0;

    if (new_playlist)
        fd = rb->open_utf8(playlist, O_CREAT|O_WRONLY|O_TRUNC);
    else
        fd = rb->open(playlist, O_CREAT|O_WRONLY|O_APPEND, 0666);

    if(fd < 0)
        return -1;

    rb->reload_directory();

    if (!insert_whole_album)
    {
        if (rb->fdprintf(fd, "%s\n", get_track_filename(pf_tracks.sel)) <= 0)
            result = -1;
    }
    else
    {
        int i = 0;
        do {
            if (rb->fdprintf(fd, "%s\n", get_track_filename(i)) <= 0)
            {
                result = -1;
                break;
            }
            rb->yield();
        } while(++i < pf_tracks.count);
    }
    rb->close(fd);
    return result;
}


static bool track_list_ready(void)
{
    if (pf_state != pf_show_tracks)
    {
        if (!storage_mode_is_ssd()
#ifdef HAVE_TC_RAMCACHE
            && !rb->tagcache_is_in_ram()
#endif
        )
            rb->splash(0, ID2P(LANG_WAIT));
        create_track_index(center_slide.slide_index);
        if (pf_tracks.count == 0)
        {
            free_borrowed_tracks();
            return false;
        }
        reset_track_list();
    }
    return true;
}


static bool context_menu_ready(void)
{
#ifdef USEGSLIB
    grey_show(false);
    rb->lcd_clear_display();
    rb->lcd_update();
#endif
    if (!track_list_ready())
    {
#ifdef USEGSLIB
        grey_show(true);
#endif
        return false;
    }
#if LCD_DEPTH > 1
#ifdef USEGSLIB
    rb->lcd_set_foreground(N_BRIGHT(0));
    rb->lcd_set_background(N_BRIGHT(255));
#elif defined (HAVE_LCD_COLOR)
    rb->lcd_set_background(rb->global_settings->bg_color);
    rb->lcd_set_foreground(rb->global_settings->fg_color);
#endif
#endif
    insert_whole_album = (pf_state != pf_show_tracks) || show_tracks_while_browsing;
    FOR_NB_SCREENS(i)
        rb->viewportmanager_theme_enable(i, true, NULL);

    return true;
}

static void context_menu_cleanup(void)
{
    FOR_NB_SCREENS(i)
        rb->viewportmanager_theme_undo(i, false);
    rb->sb_set_persistent_title("Cover Flow", Icon_NOICON, SCREEN_MAIN);
    rb->lcd_set_viewport(&pf_vp);
    if (pf_state != pf_show_tracks)
        free_borrowed_tracks();
#ifdef USEGSLIB
    grey_show(true);
#elif LCD_DEPTH > 1 && defined(HAVE_LCD_COLOR)
    rb->lcd_set_background(pf_bg_color);
    rb->lcd_set_foreground(pf_fg_color);
#endif
    mylcd_set_drawmode(DRMODE_FG);
}


static int context_menu(void)
{
    char album_name[MAX_PATH];
    char *file_name = get_track_filename(show_tracks_while_browsing ? 0 : pf_tracks.sel);
    int attr = FILE_ATTR_AUDIO;

    enum {
        PF_CURRENT_PLAYLIST = 0,
        PF_CATALOG,
        PF_ID3_INFO
    };
    MENUITEM_STRINGLIST(context_menu, ID2P(LANG_ONPLAY_MENU_TITLE), NULL,
                        ID2P(LANG_PLAYING_NEXT),
                        ID2P(LANG_ADD_TO_PL),
                        ID2P(LANG_MENU_SHOW_ID3_INFO));

    while (1)  {
        switch (rb->do_menu(&context_menu,
                            NULL, NULL, false)) {

            case PF_CURRENT_PLAYLIST:
                if (insert_whole_album && pf_tracks.count > 1)
                {
                    attr = ATTR_DIRECTORY;
                    file_name = NULL;
                }
                rb->onplay_show_playlist_menu(file_name, attr, &pf_current_playlist_insert);
                return 0;
            case PF_CATALOG:
                if (insert_whole_album)
                {
                    /* add a leading slash so that catalog_add_to_a_playlist
                       later prefills the name when creating a new playlist */
                    rb->snprintf(album_name, MAX_PATH, "/%s", get_album_name(center_index));
                    rb->fix_path_part(album_name, 1, sizeof(album_name) - 2);
                    file_name = album_name;
                    attr = ATTR_DIRECTORY;
                }

                rb->onplay_show_playlist_cat_menu(file_name, attr, &pf_add_to_playlist);
                return 0;
            case PF_ID3_INFO:
                return show_id3_info(file_name);
            case MENU_ATTACHED_USB:
                return PLUGIN_USB_CONNECTED;
            default:
                return 0;

        }
    }
}



/*
 * Puts selected album's tracks into a newly created playlist and starts playing
 */
static bool start_playback(bool return_to_WPS)
{
#ifdef USEGSLIB
    grey_show(false);
#if LCD_DEPTH > 1
    rb->lcd_set_background(N_BRIGHT(0));
    rb->lcd_set_foreground(N_BRIGHT(255));
#endif
    rb->lcd_clear_display();
    rb->lcd_update();
#endif

    if (!rb->warn_on_pl_erase() || !track_list_ready())
    {
#ifdef USEGSLIB
        grey_show(true);
#endif
        return false;
    }

    insert_whole_album = true;
    int start_index = pf_tracks.sel;
    bool shuffle = rb->global_settings->playlist_shuffle;
    /* can't reuse playlist if it may be out of sync with our track list */
    if (shuffle || center_slide.slide_index != old_playlist
                || (old_shuffle != shuffle))
    {
        if (!pf_current_playlist_insert(PLAYLIST_REPLACE, false, true))
        {
#ifdef USEGSLIB
            grey_show(true);
#endif
            return false;
        }
        if (shuffle)
            start_index = rb->playlist_shuffle(*rb->current_tick, pf_tracks.sel);
    }
    rb->playlist_start(start_index, 0, 0);
    old_shuffle = shuffle;
    
    /* Handle return_to_WPS for all display types */
    if (return_to_WPS)
    {
        /* Return to WPS after starting playback */
        return true;
    }
    
#ifdef USEGSLIB
    grey_show(true);
#endif
    return true;
}
#endif /* PF_PLAYBACK_CAPABLE */

/**
   Draw the current album name
 */
static void draw_album_text(void)
{
    char album_and_year[MAX_PATH];
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
    {
        if (retail_flip_position < 256 && pf_state != pf_cover_out)
            draw_retail_labels();
        return;
    }
#endif

    if (pf_cfg.show_album_name == ALBUM_NAME_HIDE)
        return;

    static int prev_albumtxt_index = -1;
    static bool prev_show_year = false;
    int albumtxt_index;
    int char_height;
    int albumtxt_x, albumtxt_y, artisttxt_x;
    int album_idx = 0;

    char *albumtxt;
    char *artisttxt;
    int c;
    /* Draw album text */
    if ( pf_state == pf_scrolling ) {
        c = fade;
        if (step < 0) c = 255-c;
        if (c > 128 ) { /* half way to next slide .. still not perfect! */
            albumtxt_index = center_index+step;
            if (pf_cfg.text_crossfade)
                c = (c-128)*2;
            else
                c = 255;
        }
        else {
            albumtxt_index = center_index;
            if (pf_cfg.text_crossfade)
                c = (128-c)*2;
            else
                c = 255;
        }
    }
    else {
        albumtxt_index = center_index;
        c= 255;
    }
    albumtxt = get_album_name_idx(albumtxt_index, &album_idx);

    if (pf_cfg.show_year && pf_idx.album_index[albumtxt_index].year > 0)
    {
        rb->snprintf(album_and_year, sizeof(album_and_year), "%s – %d",
             albumtxt, pf_idx.album_index[albumtxt_index].year);
    } else
        rb->snprintf(album_and_year, sizeof(album_and_year), "%s", albumtxt);

#ifdef HAVE_LCD_COLOR
    mylcd_set_foreground(pf_color_mix(c));
#else
    mylcd_set_foreground(G_BRIGHT(c));
#endif
    bool album_changed = (albumtxt_index != prev_albumtxt_index
                         || pf_cfg.show_year != prev_show_year);
    if (album_changed) {
        set_scroll_line(album_and_year, PF_SCROLL_ALBUM);
        prev_albumtxt_index = albumtxt_index;
        prev_show_year = pf_cfg.show_year;
    }

    char_height = rb->screens[SCREEN_MAIN]->getcharheight();
    switch(pf_cfg.show_album_name){
        case ALBUM_AND_ARTIST_TOP:
            albumtxt_y = 0;
            break;
        case ALBUM_NAME_BOTTOM:
        case ALBUM_AND_ARTIST_BOTTOM:
            albumtxt_y = pf_cfg.show_statusbar
                ? (pf_height - (char_height * 9 / 4))
                : (pf_height - (char_height * 5 / 2));
            break;
        case ALBUM_NAME_TOP:
        default:
            albumtxt_y = char_height / 2;
            break;
    }

    albumtxt_x = get_scroll_line_offset(PF_SCROLL_ALBUM);


    if ((pf_cfg.show_album_name == ALBUM_AND_ARTIST_TOP)
        || (pf_cfg.show_album_name == ALBUM_AND_ARTIST_BOTTOM)){

        /* iPod Classic 6G custom: compare seek (unique per album) instead
           of name_idx (byte offset that can collide after sort/dedup) */
        if (pf_idx.album_index[albumtxt_index].seek != pf_idx.album_untagged_seek)
            mylcd_putsxy(albumtxt_x, albumtxt_y, album_and_year);

        artisttxt = get_album_artist(albumtxt_index);
        if (album_changed)
            set_scroll_line(artisttxt, PF_SCROLL_ARTIST);
        artisttxt_x = get_scroll_line_offset(PF_SCROLL_ARTIST);
        int y_offset = char_height * 3 / 4;
        mylcd_putsxy(artisttxt_x, albumtxt_y + y_offset, artisttxt);
    } else {
        mylcd_putsxy(albumtxt_x, albumtxt_y, album_and_year);
    }
}

/**
  Display an error message and wait for input.
*/
static void error_wait(const char *message)
{
    rb->splashf(0, "%s. Press any button to continue.", message);
    while (rb->get_action(CONTEXT_STD, 1) == ACTION_NONE)
        rb->yield();
    rb->sleep(2 * HZ);
}

static bool init(void)
{
    int ret = SUCCESS;
    void * buf;
    size_t buf_size;

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(true); /* revert in cleanup */
#endif

    wants_to_quit = false;
#ifdef SIMULATOR
    pf_trace_enabled = rb->file_exists(ROCKBOX_DIR "/pictureflow-trace.enable");
    pf_trace_count = 0;
    pf_track_cache_hits = 0;
    pf_scene_waits = 0;
#endif

    /* must appear before config load */
    rb->memset(&aa_cache, 0, sizeof(struct albumart_t));

    config_set_defaults(&pf_cfg); /* must appear before configfile_save */
    configfile_load(CONFIG_FILE, config, CONFIG_NUM_ITEMS, CONFIG_VERSION);

    /* Classic Cover Flow retains its title/status strip above both faces. */
#ifdef HAVE_LCD_COLOR
    if (pf_ipod_engine_enabled())
        pf_cfg.show_statusbar = true;
#endif

    /* Compute viewport and rendering layout based on show_statusbar setting */
    if (pf_cfg.show_statusbar)
    {
        FOR_NB_SCREENS(i)
            rb->viewportmanager_theme_enable(i, true, NULL);
        rb->viewport_set_defaults(&pf_vp, SCREEN_MAIN);
        FOR_NB_SCREENS(i)
            rb->viewportmanager_theme_undo(i, false);
    }
    else
    {
        rb->viewport_set_fullscreen(&pf_vp, SCREEN_MAIN);
    }
    pf_vp.buffer = &pf_framebuffer;
    pf_vp.x = 0;
    pf_vp.width = LCD_WIDTH;
#ifdef PF_RETAIL_FLIP
    if (pf_ipod_engine_enabled())
    {
        pf_vp.y = 20;
        pf_vp.height = LCD_HEIGHT - 20;
        retail_header_valid = ipodjs_retailos_load_opaque(8, retail_header,
            ARRAYLEN(retail_header), 320, 20);
        retail_transport_valid = ipodjs_retailos_load_resource_rga(277,
            retail_transport_data[0], sizeof(retail_transport_data[0]),
            14, 16, &retail_transport[0]);
        retail_transport_valid &= ipodjs_retailos_load_resource_rga(276,
            retail_transport_data[1], sizeof(retail_transport_data[1]),
            14, 16, &retail_transport[1]);
        service_retail_battery();
        prepare_retail_header();
    }
#endif

    pf_vp_y = pf_cfg.show_statusbar ? pf_vp.y : 0;
    pf_height = pf_cfg.show_statusbar ? pf_vp.height : LCD_HEIGHT;
    pf_half_height = LCD_HEIGHT / 2 - pf_vp_y + (pf_cfg.show_statusbar ? 8 : 0);
    pf_lower_half = pf_height - pf_half_height;
    pf_reflect_height = REFLECT_HEIGHT;
    pf_display_offs = DISPLAY_OFFS;

#ifdef HAVE_LCD_COLOR
#ifdef HAVE_ALBUMART
    pf_update_dynamic_colors();
#else
    if (pf_ipod_engine_enabled())
    {
        pf_apply_ipod_engine_colors();
    }
    else
    {
        pf_bg_color = (pix_t)rb->global_settings->bg_color;
        pf_fg_color = (pix_t)rb->global_settings->fg_color;
        pf_lss_color = (pix_t)rb->global_settings->lss_color;
        pf_lse_color = (pix_t)rb->global_settings->lse_color;
        pf_lst_color = (pix_t)rb->global_settings->lst_color;
    }
#endif
    pf_bg_rb = pf_bg_color & 0xf81f;
    pf_bg_g  = pf_bg_color & 0x7e0;
#endif


#if PF_PLAYBACK_CAPABLE
    buf = rb->plugin_get_buffer(&buf_size);
#else
    buf = rb->plugin_get_audio_buffer(&buf_size);
#ifndef SIMULATOR
    if ((uintptr_t)buf < (uintptr_t)plugin_start_addr)
    {
        uint32_t tmp_size = (uintptr_t)plugin_start_addr - (uintptr_t)buf;
        buf_size = MIN(buf_size, tmp_size);
    }
#endif
#endif

#ifdef USEGSLIB
    long grey_buf_used;
    if (!grey_init(buf, buf_size, GREY_BUFFERED|GREY_ON_COP,
                   LCD_WIDTH, LCD_HEIGHT, &grey_buf_used))
    {
        error_wait("Greylib init failed!");
        return false;
    }
    grey_setfont(FONT_UI);
    buf_size -= grey_buf_used;
    buf = (void*)(grey_buf_used + (char*)buf);
#endif

    /* store buffer pointers and sizes */
    pf_idx.buf = buf;
    pf_idx.buf_sz = buf_size;

    rb->lcd_setfont(FONT_UI);

    if (!rb->dir_exists(CACHE_PREFIX))
    {
        if (rb->mkdir( CACHE_PREFIX ) < 0)
        {
            error_wait("Could not create directory " CACHE_PREFIX);
            return false;
        }
    }

    rb->mutex_init(&buf_ctx_mutex);
    rb->mutex_init(&artwork_io_mutex);

    init_scroll_lines();
    init_reflect_table();

    /*Scan will trigger when no file is found or the option was activated*/
    if ((pf_cfg.cache_version != CACHE_VERSION)|| (load_album_index() < 0))
    {
        ret = create_album_index();

        if (ret == 0)
        {
            pf_cfg.cache_version = CACHE_REBUILD;
            if (save_album_index() < 0)
                rb->splash(HZ, "Could not write index");
        }
    }

    if (ret == ERROR_BUFFER_FULL)
    {
        error_wait("Not enough memory for album names");
        return false;
    }
    else if (ret == ERROR_NO_ALBUMS)
    {
        error_wait("No albums found. Please enable database");
        return false;
    }
    else if (ret == ERROR_DATABASE)
    {
        error_wait(database_error);
        return false;
    }
    else if (ret == ERROR_USER_ABORT)
        return false;

    number_of_slides = pf_idx.album_ct;

    size_t aa_min = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(pix_t);
    size_t aa_bufsz = ALIGN_DOWN(MAX(aa_min * 3, pf_idx.buf_sz / 8),
                                 sizeof(long));
    if (aa_bufsz < aa_min)
    {
        error_wait("Not enough memory for album art cache");
        return false;
    }

    ALIGN_BUFFER(pf_idx.buf, pf_idx.buf_sz, sizeof(long));
    aa_cache.buf = (char*) pf_idx.buf;
    aa_cache.buf_sz = aa_bufsz;

    pf_idx.buf += aa_bufsz;
    pf_idx.buf_sz -= aa_bufsz;

#if PF_PLAYBACK_CAPABLE
    /* Reserve a stable arena before initializing buflib. This removes the
     * hardware-only pool relocation that used to occur for every album. */
    ALIGN_BUFFER(pf_idx.buf, pf_idx.buf_sz, alignof(struct track_data));
    size_t track_bufsz = MAX((size_t)TRACK_BUFFER_MIN, pf_idx.buf_sz / 8);
    track_bufsz = MIN((size_t)TRACK_BUFFER_MAX, track_bufsz);
    track_bufsz = MIN(track_bufsz, pf_idx.buf_sz / 2);
    track_bufsz = ALIGN_DOWN(track_bufsz, alignof(struct track_data));
    if (track_bufsz < TRACK_BUFFER_MIN)
    {
        error_wait("Not enough memory for track list");
        return false;
    }

    pf_tracks.names = (char *)pf_idx.buf;
    pf_tracks.buf_sz = track_bufsz;
    pf_idx.buf = pf_tracks.names + track_bufsz;
    pf_idx.buf_sz -= track_bufsz;
#else
    pf_tracks.names = NULL;
    pf_tracks.buf_sz = 0;
#endif

    rb->buflib_init(&buf_ctx, (void *)pf_idx.buf, pf_idx.buf_sz);
    initialize_slide_cache();

    if (!create_empty_slide(pf_cfg.cache_version != CACHE_VERSION))
    {
        config_save(CACHE_REBUILD, false);
        error_wait("Could not load the empty slide");
        return false;
    }

    if ((pf_cfg.cache_version != CACHE_VERSION) && !create_albumart_cache())
    {
        config_save(CACHE_REBUILD, false);
        error_wait("Could not create album art cache");
    }

    if (pf_cfg.cache_version != CACHE_VERSION)
        config_save(CACHE_VERSION, pf_cfg.update_albumart);

    if ((empty_slide_hid = read_pfraw(EMPTY_SLIDE, 0, false)) < 0)
    {
        /* read_pfraw removes malformed entries. Recreate the built-in
         * fallback immediately so one bad cache file cannot make the whole
         * plugin unavailable until a second launch. */
        if (!create_empty_slide(true) ||
            (empty_slide_hid = read_pfraw(EMPTY_SLIDE, 0, false)) < 0)
        {
            error_wait("Unable to load empty slide image");
            return false;
        }
    }

    if (!create_pf_thread())
    {
        error_wait("Cannot create thread!");
        return false;
    }

    buffer = LCD_BUF;
    buffer += pf_vp_y * BUFFER_WIDTH; /* offset below status bar */

    pf_state = pf_idle;

    pf_tracks.cur_idx = -1;
    pf_tracks.used = 0;
    pf_tracks.count = 0;

    extra_fade = 0;
    slide_frame = 0;
    step = 0;
    target = 0;
    fade = 256;

    recalc_offsets();
    reset_slides();

#ifdef USEGSLIB
    grey_set_drawmode(DRMODE_FG);
#endif
    rb->lcd_set_drawmode(DRMODE_FG);

    return true;
}

static bool reinit(void)
{
    return_to_idle_state();

    unsigned int hash_album = mfnv(get_album_name(center_index));
    unsigned int hash_artist = mfnv(get_album_artist(center_index));

    cleanup();
    if (init())
    {
        reselect(hash_album, hash_artist); /* splash if not found */
#ifdef USEGSLIB
        grey_show(true);
#endif
        return true;
    }
    return false;
}

static bool prompt_reinit(void)
{
    const struct text_message prompt = {
      (const char*[]) {ID2P(LANG_TAGCACHE_BUSY), ID2P(LANG_TAGCACHE_UPDATE)}, 2};
#ifdef USEGSLIB
    grey_show(false);
#endif
    if (rb->gui_syncyesno_run(&prompt, NULL, NULL) == YESNO_YES)
    {
        pf_cfg.update_albumart = true;
        pf_cfg.cache_version = CACHE_REBUILD;
        rb->remove(EMPTY_SLIDE);
        configfile_save(CONFIG_FILE, config, CONFIG_NUM_ITEMS, CONFIG_VERSION);
        if (!reinit())
            return false;
    }
    else
    {
#ifdef USEGSLIB
        grey_show(true);
#endif
        mylcd_set_drawmode(DRMODE_FG);
    }
    return true;
}

/**
  Main function that also contain the main plasma
  algorithm.
 */
static int pictureflow_main(void)
{
    int ret;
    char fpstxt[10];
    int button;
    int frames = 0;
    long last_update = *rb->current_tick;
    long current_update;
    long update_interval = 100;
    int fps = 0;
    int fpstxt_y;
    bool instant_update;

    while (true) {
        /* Get input first. The SBS renders during get_custom_action() and
         * writes into the framebuffer (including decorative viewports that
         * overlap PF's area). Inhibit the SBS's lcd_update() so it doesn't
         * push that content to the display — PF's own lcd_update() after
         * rendering will push both the SBS status bar and PF content
         * atomically, avoiding one-frame flicker of theme artifacts. */
        instant_update = (pf_state == pf_scrolling ||
                          pf_state == pf_cover_in ||
                          pf_state == pf_cover_out ||
                          pf_state == pf_open_pending);

        if (pf_cfg.show_statusbar)
            rb->skin_render_inhibit_flush(true);

        button = rb->get_custom_action(CONTEXT_PLUGIN
#ifndef USE_CORE_PREVNEXT
            |(pf_state == pf_show_tracks ? 1 : 0)
#endif
            ,instant_update ? MAX(1, HZ/30) : HZ/16,
            get_context_map);

        if (pf_cfg.show_statusbar)
            rb->skin_render_inhibit_flush(false);

        /* SBS rendering in get_custom_action resets the viewport to default.
         * Restore ours so LCD API calls use the correct coordinates. */
        rb->lcd_set_viewport(&pf_vp);

        current_update = *rb->current_tick;
        frames++;

        if (button != ACTION_NONE)
        {
            navigation_settle_tick = current_update;
            if (track_prepare_album >= 0 && pf_state != pf_open_pending)
                free_borrowed_tracks();
        }

#if defined(HAVE_LCD_COLOR) && defined(HAVE_ALBUMART)
        pf_update_dynamic_colors();
#endif
        update_scroll_lines();

        /* Handle states */
        scene_drawn = true;
        switch ( pf_state ) {
            case pf_scrolling:
                update_scroll_animation();
                render_all_slides();
                break;
            case pf_cover_in:
                update_cover_in_animation();
                render_all_slides();
#ifdef PF_RETAIL_FLIP
                if (scene_drawn && pf_ipod_engine_enabled())
                    render_retail_tracks();
#endif
                break;
            case pf_cover_out:
                show_tracks_while_browsing = false;
                update_cover_out_animation();
                render_all_slides();
#ifdef PF_RETAIL_FLIP
                if (scene_drawn && pf_ipod_engine_enabled())
                    render_retail_tracks();
#endif
                break;
            case pf_show_tracks:
                if (!show_track_list())
                {
                    skip_animation_to_idle_state();
                    request_album_open();
                    render_all_slides();
                }
                break;
            case pf_open_pending:
                render_all_slides();
                break;
            case pf_idle:
                show_tracks_while_browsing = false;
                render_all_slides();
                break;
        }

        /* Calculate FPS */
        if (current_update - last_update > update_interval) {
            fps = frames * HZ / (current_update - last_update);
            last_update = current_update;
            frames = 0;
        }
        /* Draw FPS only when explicitly enabled. Cache progress continues in
         * the background without overlaying a percentage on the cover view. */
        if (pf_cfg.show_fps)
        {
#ifdef USEGSLIB
            mylcd_set_foreground(G_BRIGHT(255));
#else
            mylcd_set_foreground(G_PIX(255,0,0));
#endif
            rb->snprintf(fpstxt, sizeof(fpstxt), "FPS: %d", fps);

            if (pf_cfg.show_album_name == ALBUM_NAME_TOP ||
                pf_cfg.show_album_name == ALBUM_AND_ARTIST_TOP)
                fpstxt_y = pf_height -
                           rb->screens[SCREEN_MAIN]->getcharheight();
            else
                fpstxt_y = 0;
            mylcd_putsxy(0, fpstxt_y, fpstxt);
        }
        if (scene_drawn)
            draw_album_text();
        if (scene_drawn &&
            ((pf_state == pf_open_pending &&
              TIME_AFTER(*rb->current_tick, album_open_tick + HZ * 15 / 100)) ||
             TIME_BEFORE(*rb->current_tick, album_error_until)))
        {
            mylcd_set_foreground(pf_fg_color);
            mylcd_putsxy(4, pf_height - rb->screens[SCREEN_MAIN]->getcharheight(),
                pf_state == pf_open_pending ? "Opening album..." :
                                             "Album unavailable");
        }


        /* Copy offscreen buffer to LCD and give time to other threads */
#ifdef PF_RETAIL_FLIP
        if (scene_drawn && pf_ipod_engine_enabled())
            draw_retail_header();
#endif
        if (scene_drawn)
            mylcd_update();
#ifdef SIMULATOR
        trace_frame();
#endif
        rb->yield();

        /* Optional work runs after a complete frame, never inside a draw or
         * animation. Leave newly queued input for the ordinary action loop. */
        if (button == ACTION_NONE && rb->button_queue_count() == 0
#ifdef HAS_BUTTON_HOLD
            && !rb->button_hold()
#endif
            )
        {
#ifdef PF_RETAIL_FLIP
            if (pf_ipod_engine_enabled() &&
                (pf_state == pf_open_pending ||
                 ((pf_state == pf_idle || pf_state == pf_show_tracks) &&
                  TIME_AFTER(*rb->current_tick, navigation_settle_tick +
                             MAX(1, HZ * 8 / 100)))))
                service_retail_text();
#endif
            int repair;
            buf_ctx_lock();
            repair = artwork_repair_album;
            buf_ctx_unlock();
            if ((pf_state == pf_idle || pf_state == pf_open_pending ||
                 pf_state == pf_scrolling) && repair >= 0 &&
                TIME_AFTER(*rb->current_tick, artwork_repair_retry))
            {
                rb->mutex_lock(&artwork_io_mutex);
                int saved_index = aa_cache.idx;
                int saved_inspected = aa_cache.inspected;
                aa_cache.idx = repair;
                aa_cache.inspected = 0;
                bool saved_update = pf_cfg.update_albumart;
                pf_cfg.update_albumart = false;
                incremental_albumart_cache(false);
                pf_cfg.update_albumart = saved_update;
                aa_cache.idx = saved_index;
                aa_cache.inspected = saved_inspected;
                rb->mutex_unlock(&artwork_io_mutex);
                buf_ctx_lock();
                if (artwork_repair_album == repair)
                    artwork_repair_album = -1;
                buf_ctx_unlock();
                artwork_repair_retry = *rb->current_tick + MAX(1, HZ / 10);
                rb->queue_post(&thread_q, EV_WAKEUP, 0);
            }
            if (pf_state == pf_open_pending)
            {
                if (TIME_AFTER(*rb->current_tick, album_open_tick + 2 * HZ))
                {
                    free_borrowed_tracks();
                    skip_animation_to_idle_state();
                    track_prepare_retry = *rb->current_tick + HZ;
                    album_error_until = *rb->current_tick + 2 * HZ;
                }
                else if (scene_drawn &&
                         prepare_track_index(center_slide.slide_index))
                {
#ifdef PF_RETAIL_FLIP
                    if (pf_ipod_engine_enabled())
                        service_retail_text();
#endif
                    cover_animation_tick = *rb->current_tick;
                    pf_state = pf_cover_in;
                    if (pf_cfg.reduced_motion)
                        skip_animation_to_show_tracks();
                }
            }
            else if (pf_state == pf_idle &&
                     TIME_AFTER(*rb->current_tick,
                                navigation_settle_tick + MAX(1, HZ * 8 / 100)))
            {
                if (pf_tracks.cur_idx != center_slide.slide_index)
                    prepare_track_index(center_slide.slide_index);
#if PF_PLAYBACK_CAPABLE
                else if (track_cache_dirty)
                    save_track_cache();
#endif
                else if (aa_cache.inspected < pf_idx.album_ct)
                {
                    rb->mutex_lock(&artwork_io_mutex);
                    incremental_albumart_cache(false);
                    rb->mutex_unlock(&artwork_io_mutex);
                }
#ifdef PF_RETAIL_FLIP
                else if (pf_ipod_engine_enabled())
                    service_retail_battery();
#endif
            }
        }

        switch (button) {
        case PF_QUIT:
            if (pf_state == pf_open_pending)
            {
                free_borrowed_tracks();
                skip_animation_to_idle_state();
                break;
            }
            if (pf_state == pf_cover_in)
            {
                reverse_animation();
                break;
            }
            if (pf_state == pf_cover_out)
            {
                skip_animation_to_idle_state();
                break;
            }
            if (pf_state == pf_show_tracks)
            {
                pf_state = pf_cover_out;
                break;
            }
            return PLUGIN_OK;
        case PF_WPS:
            return PLUGIN_GOTO_WPS;
        case PF_BACK:
            if (pf_state == pf_open_pending)
            {
                free_borrowed_tracks();
                skip_animation_to_idle_state();
                break;
            }
            if (show_tracks_while_browsing)
                show_tracks_while_browsing = false;
            else if (pf_state == pf_show_tracks)
            {
                pf_state = pf_cover_out;
            }
            else if (pf_state == pf_cover_in)
                reverse_animation();
            else if (pf_state == pf_cover_out)
                skip_animation_to_idle_state();
            else if (pf_state == pf_idle || pf_state == pf_scrolling)
                return PLUGIN_GOTO_WPS;
            break;
        case PF_MENU:
            if (pf_state == pf_open_pending)
            {
                free_borrowed_tracks();
                skip_animation_to_idle_state();
                break;
            }
            if (pf_state == pf_show_tracks)
            {
                pf_state = pf_cover_out;
                break;
            }
#ifdef USEGSLIB
            grey_show(false);
#endif
            FOR_NB_SCREENS(i)
                rb->viewportmanager_theme_enable(i, true, NULL);
            ret = main_menu();
            FOR_NB_SCREENS(i)
                rb->viewportmanager_theme_undo(i, false);
            rb->sb_set_persistent_title("Cover Flow", Icon_NOICON, SCREEN_MAIN);
            rb->lcd_set_viewport(&pf_vp);

            if (ret == -3)
            {
                /* Pop the main loop's theme push before reinit */
                FOR_NB_SCREENS(i)
                    rb->viewportmanager_theme_undo(i, false);
                if (!reinit())
                    return PLUGIN_OK;
                /* Push with new setting (reinit loaded new config) */
                FOR_NB_SCREENS(i)
                    rb->viewportmanager_theme_enable(i,
                        false, NULL);
                rb->sb_set_persistent_title("Cover Flow", Icon_NOICON, SCREEN_MAIN);
                rb->lcd_set_viewport(&pf_vp);
#ifdef HAVE_LCD_COLOR
                mylcd_set_background(pf_bg_color);
                mylcd_set_foreground(pf_fg_color);
#endif
                mylcd_set_drawmode(DRMODE_FG);
            }
            else if (ret == -2)
                return PLUGIN_GOTO_WPS;
            else if (ret == -1)
                return PLUGIN_OK;
            else if (ret != 0 )
                return ret;
            else
            {
#ifdef USEGSLIB
                grey_show(true);
#endif
                mylcd_set_drawmode(DRMODE_FG);
            }
            break;

        case PF_NEXT:
        case PF_NEXT_REPEAT:
            if ( pf_state == pf_show_tracks )
            {
                if (show_tracks_while_browsing)
                    select_next_album();
                else
                    select_next_track();
            }
            else if (pf_state == pf_cover_in)
                skip_animation_to_show_tracks();
            else if (pf_state == pf_cover_out)
                skip_animation_to_idle_state();

            if ( pf_state == pf_idle || pf_state == pf_scrolling )
                show_next_slide();
            break;

        case PF_PREV:
        case PF_PREV_REPEAT:
            if ( pf_state == pf_show_tracks )
            {
                if (show_tracks_while_browsing)
                    select_prev_album();
                else
                    select_prev_track();
            }
            else if (pf_state == pf_cover_in)
                skip_animation_to_show_tracks();
            else if (pf_state == pf_cover_out)
                skip_animation_to_idle_state();

            if ( pf_state == pf_idle || pf_state == pf_scrolling )
                show_previous_slide();
            break;
        case PF_SORTING_NEXT:
            sort_albums((pf_cfg.sort_albums_by + 1) % SORT_VALUES_SIZE, false);
            break;
        case PF_SORTING_PREV:
            sort_albums((pf_cfg.sort_albums_by + (SORT_VALUES_SIZE - 1)) % SORT_VALUES_SIZE, false);
            break;
        case PF_JMP:
            if (pf_state == pf_idle || pf_state == pf_scrolling)
            {
                int new_idx = jmp_idx_next();
                if (new_idx != center_index)
                {
                    pf_state = pf_idle;
                    set_current_slide(new_idx);
                }
            }
            else if ( pf_state == pf_show_tracks )
                select_next_album();
            break;
        case PF_JMP_PREV:
            if (pf_state == pf_idle || pf_state == pf_scrolling)
            {
                int new_idx = jmp_idx_prev();
                if (new_idx != center_index)
                {
                    pf_state = pf_idle;
                    set_current_slide(new_idx);
                }
            }
            else if ( pf_state == pf_show_tracks )
                select_prev_album();
            else if (pf_state == pf_cover_in)
                reverse_animation();
            else if (pf_state == pf_cover_out)
                skip_animation_to_idle_state();
            break;
#if PF_PLAYBACK_CAPABLE
        case PF_CONTEXT:
            if (pf_state == pf_idle || pf_state == pf_scrolling ||
                pf_state == pf_show_tracks || pf_state == pf_cover_out)
            {
                if ( pf_state == pf_scrolling)
                {
                    set_current_slide(target);
                    pf_state = pf_idle;
                }
                else if (pf_state == pf_cover_out)
                    skip_animation_to_idle_state();

                if (context_menu_ready())
                {
                    ret = context_menu();
                    context_menu_cleanup();
                    if ( ret != 0 ) return ret;
                }
                else if (!prompt_reinit())
                    return PLUGIN_OK;
            }
            break;
#endif
        case PF_TRACKLIST:
        case PF_SELECT:
            if (pf_state == pf_idle || pf_state == pf_scrolling)
            {
                if (pf_state == pf_scrolling)
                    set_current_slide(target);
                request_album_open();
            }
            else if (pf_state == pf_cover_out)
                reverse_animation();
            else if (pf_state == pf_cover_in)
                break; /* A second Center during the flip must not play. */
            else if (pf_state == pf_show_tracks)
            {
                if (show_tracks_while_browsing)
                    show_tracks_while_browsing = false;
#if PF_PLAYBACK_CAPABLE
                if (start_playback(true))
                    return PLUGIN_GOTO_WPS;
#endif
            }
            break;
#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && PF_PLAYBACK_CAPABLE
        case SYS_HIBERNATE_PREPARE:
            if (!pf_hibernate_acknowledge())
                return PLUGIN_ERROR;
            break;
#endif
        default:
            exit_on_usb(button);
            break;
        }
    }
}

/*************************** Plugin entry point ****************************/

enum plugin_status plugin_start(const void *parameter)
{
    struct viewport *vp_main = rb->lcd_set_viewport(NULL);
    lcd_fb = vp_main->buffer->fb_ptr;

    /* Create a framebuffer wrapper identical to the default but at a different
     * address. lcd_clear_viewport() checks (vp->buffer == &lcd_framebuffer_default)
     * by pointer — with our own struct, it fills with bg_pattern instead of
     * copying from the theme backdrop buffer. */
    pf_framebuffer = *vp_main->buffer;

    int ret;
    const char *file = parameter;
    bool file_id3 = (parameter && (((char *) parameter)[0] == '/'));

    if (!check_database())
    {
        error_wait(database_error);
        return PLUGIN_OK;
    }
    atexit(cleanup);

    if (init())
    {
#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && PF_PLAYBACK_CAPABLE
        /* Versioned opt-in: unmodified plugins never receive the retained
         * prepare event and retain the normal cold-shutdown fallback. */
#endif
        /* Push theme state for main loop */
        FOR_NB_SCREENS(i)
            rb->viewportmanager_theme_enable(i, false, NULL);
        rb->sb_set_persistent_title("Cover Flow", Icon_NOICON, SCREEN_MAIN);
        rb->lcd_set_viewport(&pf_vp);
#if CONFIG_KEYPAD == IPOD_3G_PAD
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_set_foreground(LCD_WHITE);
#endif
#ifdef HAVE_LCD_COLOR
        mylcd_set_background(pf_bg_color);
        mylcd_set_foreground(pf_fg_color);
#endif
        mylcd_set_drawmode(DRMODE_FG);

        set_initial_slide(file_id3 ? file : NULL); /* may call splash */
#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && PF_PLAYBACK_CAPABLE
        pf_hibernate_activate();
#else
        rb->button_clear_queue();
#endif
#ifdef USEGSLIB
        grey_show(true);
#endif
        ret = pictureflow_main();

        rb->lcd_set_viewport(NULL);
        FOR_NB_SCREENS(i)
            rb->viewportmanager_theme_undo(i, false);
    }
    else
        ret = PLUGIN_OK;

    if ( ret == PLUGIN_OK || ret == PLUGIN_GOTO_WPS)
    {
        if (pf_state == pf_scrolling)
            pf_cfg.last_album = target;
        else
            pf_cfg.last_album = center_index;

        if (pf_cfg.last_album >= 0 && pf_cfg.last_album < number_of_slides)
        {
            const char *name = get_album_name(pf_cfg.last_album);
            const char *artist = get_album_artist(pf_cfg.last_album);
            if (rb->strlen(name) < MAX_PATH && rb->strlen(artist) < MAX_PATH)
            {
                rb->strlcpy(pf_cfg.last_album_name, name, MAX_PATH);
                rb->strlcpy(pf_cfg.last_album_artist, artist, MAX_PATH);
            }
            else
            {
                pf_cfg.last_album_name[0] = '\0';
                pf_cfg.last_album_artist[0] = '\0';
            }
        }

        if (configfile_save(CONFIG_FILE, config, CONFIG_NUM_ITEMS,
                            CONFIG_VERSION))
        {
#ifdef USEGSLIB
            grey_show(false);
#endif
            rb->splash(HZ, ID2P(LANG_ERROR_WRITING_CONFIG));
            ret = PLUGIN_OK;
        }
    }
    return ret;
}
