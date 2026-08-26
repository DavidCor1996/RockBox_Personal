/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2007 Jonathan Gordon
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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include "string-extra.h"
#include "config.h"
#include "appevents.h"
#include "menu.h"
#include "root_menu.h"
#include "lang.h"
#include "settings.h"
#include "screens.h"
#include "kernel.h"
#include "debug.h"
#include "misc.h"
#include "open_plugin.h"
#include "rolo.h"
#include "powermgmt.h"
#include "power.h"
#include "button.h"
#include "backlight.h"
#include "sound.h"
#include "talk.h"
#include "audio.h"
#include "storage.h"
#include "shortcuts.h"
#include "version.h"
#ifdef IPOD_ACCESSORY_PROTOCOL
#include "iap.h"
#endif
#if defined(IPOD_6G) && !defined(SIMULATOR)
#include "videoout-6g.h"
#endif

#ifdef HAVE_HOTSWAP
#include "mv.h"
#endif
/* gui api */
#include "list.h"
#include "slideshow_order.h"
#include "splash.h"
#include "action.h"
#include "yesno.h"
#include "viewport.h"
#include "core_alloc.h"
#include "rbpaths.h"
#include "bmp.h"
#ifdef HAVE_JPEG
#include "recorder/jpeg_load.h"
#endif

#include "tree.h"
#if CONFIG_TUNER
#include "radio.h"
#endif
#ifdef HAVE_RECORDING
#include "recording.h"
#endif
#include "wps.h"
#include "skin_engine/skin_engine.h"
#include "skin_engine/wps_internals.h"
#include "gui/ipodjs_ui.h"
#include "gui/ipodjs_trace.h"
#ifdef HAVE_IPODJS_UI
#include "gui/qrcode/qrcodegen.h"
#endif
#include "bookmark.h"
#include "playlist.h"
#include "playlist_viewer.h"
#include "playlist_catalog.h"
#include "playback.h"
#include "buffering.h"
#include "menus/exported_menus.h"
#ifdef HAVE_RTC_ALARM
#include "rtc.h"
#endif
#ifdef HAVE_TAGCACHE
#include "tagcache.h"
#endif
#ifdef HAVE_ALBUMART
#include "albumart.h"
#ifdef HAVE_JPEG
#endif
#endif
#include "language.h"
#include "plugin.h"
#include "rockachievements_telemetry.h"
#ifdef HAVE_IPODJS_UI
#include "notification_manager.h"
#include "usb_internet.h"
#if defined(USB_ENABLE_IPHETH_HOST) && !defined(SIMULATOR)
#include "usb_iphone_network.h"
#include "usb_iphone_preflight.h"
#include "usb_iphone_tether.h"
#endif
#endif
#include "filetypes.h"
#include "file.h"
#include "keyboard.h"
#include "disk.h"
#include "mv.h"
#include "dir.h"
#include "crc32.h"
#include "general.h"
#if defined(IPOD_NANO2G) || defined(IPOD_VIDEO) || defined(IPOD_6G) || \
    defined(IPOD_NANO3G)
#include "lcd.h"
#include "font.h"
#include "timefuncs.h"
#endif
#ifdef HAVE_IPODJS_UI
#include "gui/albumlist_art.h"

/* Bitmap payloads are handed to the BMP decoder as bm->data, which casts them
 * to fb_data * and writes them with halfword stores.  A plain char array only
 * guarantees one-byte alignment, and an unaligned strh data-aborts on the
 * iPod Video's core -- so force the base of every payload. */
#define IPODJS_BM_ALIGN __attribute__((aligned(4)))
static bool root_menu_video_enabled(void);
static bool root_menu_video_uses_stock_music(void);
static void root_menu_video_enter_native_screen(void);
static int root_menu_video_finish_native_screen(int ret);
static bool root_menu_video_handle_tree_stop(int action, bool *redraw);
static void root_menu_video_prepare_netflix_logo(void);
static void root_menu_video_prepare_netflix_browser_logo(void);
static struct bitmap *root_menu_video_netflix_logo_small_cached(void);
static void root_menu_video_prepare_netflix_watched_badge(void);
static struct bitmap *root_menu_video_netflix_watched_cached(void);
static int ipodjs_video_wps(void);
static bool ipodjs_video_wps_empty(const char *title, const char *message,
                                   bool allow_replay);
#ifdef HAVE_TAGCACHE
static int root_menu_video_qrcode_screen(void);
#endif
#endif
#if defined(IPOD_NANO2G)

#define ROCKPOD_NANO2G_BOOTLOADER_STAGE_MARKER ROCKBOX_DIR "/rockpod/boot/nano2g-encrypt-pending"
#define ROCKPOD_NANO2G_BOOTLOADER_STAGE_INPUT ROCKBOX_DIR "/b.ipod"
#define ROCKPOD_NANO2G_BOOTLOADER_STAGE_OUTPUT ROCKBOX_DIR "/b.ipodx"
#endif

struct root_items {
    int (*function)(void* param);
    void* param;
    const struct menu_item_ex *context_menu;
};
static int next_screen = GO_TO_ROOT; /* holding info about the upcoming screen
                                        * which is the current screen for the
                                        * rest of the code after load_screen
                                        * is called */
static int last_screen = GO_TO_ROOT; /* unfortunatly needed so we can resume
                                        or goto current track based on previous
                                        screen */

#if defined(HAVE_TAGCACHE) && defined(HAVE_IPODJS_UI)
/* Plugin dispatch and the WPS both participate in root-menu history. Keep
 * the PictureFlow origin explicit so an iPodJS WPS Menu press cannot be
 * redirected by an intermediate history update. */
enum ipodjs_ui_origin {
    IPODJS_UI_ORIGIN_NONE = 0,
    IPODJS_UI_ORIGIN_DATABASE,
    IPODJS_UI_ORIGIN_FILES,
    IPODJS_UI_ORIGIN_PICTUREFLOW,
};

struct ipodjs_ui_origin_frame {
    int screen;
    enum ipodjs_ui_origin origin;
    int selected_row;
    int first_visible_row;
    int database_filter;
    int database_seek;
    bool coverflow;
};

#define IPODJS_UI_ORIGIN_DEPTH 4
static struct ipodjs_ui_origin_frame
    ipodjs_ui_origin_stack[IPODJS_UI_ORIGIN_DEPTH];
static int ipodjs_ui_origin_depth;

static void ipodjs_ui_origin_clear(void)
{
    ipodjs_ui_origin_depth = 0;
}

static void ipodjs_ui_origin_push(int screen, enum ipodjs_ui_origin origin)
{
    struct ipodjs_ui_origin_frame *frame;

    if (ipodjs_ui_origin_depth > 0 &&
        ipodjs_ui_origin_stack[ipodjs_ui_origin_depth - 1].origin == origin)
        return;
    if (ipodjs_ui_origin_depth == IPODJS_UI_ORIGIN_DEPTH)
    {
        memmove(&ipodjs_ui_origin_stack[0], &ipodjs_ui_origin_stack[1],
                sizeof(ipodjs_ui_origin_stack[0]) *
                (IPODJS_UI_ORIGIN_DEPTH - 1));
        ipodjs_ui_origin_depth--;
    }
    frame = &ipodjs_ui_origin_stack[ipodjs_ui_origin_depth++];
    frame->screen = screen;
    frame->origin = origin;
    /* Browser owners retain their live selection/window.  These sentinel
     * fields make that ownership explicit without duplicating tagcache or
     * playlist state in the iPodJS stack. */
    frame->selected_row = -1;
    frame->first_visible_row = -1;
    frame->database_filter = -1;
    frame->database_seek = -1;
    frame->coverflow = origin == IPODJS_UI_ORIGIN_PICTUREFLOW;
    ipodjs_trace_origin("push", screen, origin, ipodjs_ui_origin_depth);
}

static bool ipodjs_ui_origin_pop(struct ipodjs_ui_origin_frame *frame)
{
    if (ipodjs_ui_origin_depth <= 0)
        return false;
    *frame = ipodjs_ui_origin_stack[--ipodjs_ui_origin_depth];
    ipodjs_trace_origin("pop", frame->screen, frame->origin,
                        ipodjs_ui_origin_depth);
    return true;
}
#endif

static int previous_music = GO_TO_WPS; /* Toggles behavior of the return-to
                                        * playback-button depending
                                        * on FM radio */

#if (CONFIG_TUNER)
static void rootmenu_start_playback_callback(unsigned short id, void *param)
{
    (void) id; (void) param;
    /* Cancel FM radio selection as previous music. For cases where we start
       playback without going to the WPS, such as playlist insert or
       playlist catalog. */
    previous_music = GO_TO_WPS;
}
#endif

static char current_track_path[MAX_PATH];
static void rootmenu_track_changed_callback(unsigned short id, void* param)
{
    (void)id;
    struct mp3entry *id3 = ((struct track_event *)param)->id3;
    strmemccpy(current_track_path, id3->path, MAX_PATH);
}

static bool browser_enter_native_screen(void)
{
#ifdef HAVE_IPODJS_UI
    if (root_menu_video_enabled())
    {
        root_menu_video_enter_native_screen();
        return true;
    }
#endif
    return false;
}

static int browser_finish_native_screen(bool active, int ret)
{
#ifdef HAVE_IPODJS_UI
    if (active)
        return root_menu_video_finish_native_screen(ret);
#else
    (void)active;
#endif
    return ret;
}

static int browser(void* param)
{
    int ret_val;
    bool ipodjs_native = browser_enter_native_screen();
#ifdef HAVE_TAGCACHE
    struct tree_context* tc = tree_get_context();
#endif
    int filter = SHOW_SUPPORTED;
    char folder[MAX_PATH] = "/";
    /* stuff needed to remember position in file browser */
    static char last_folder[MAX_PATH] = "/";
    /* and stuff for the database browser */
#ifdef HAVE_TAGCACHE
    static int last_db_dirlevel = 0, last_db_selection = 0, last_ft_dirlevel = 0;
#endif

    switch ((intptr_t)param)
    {
        case GO_TO_FILEBROWSER:
            filter = global_settings.dirfilter;
            if (global_settings.browse_current &&
                    last_screen == GO_TO_WPS &&
                    current_track_path[0])
            {
                strcpy(folder, current_track_path);
            }
            else if (!strcmp(last_folder, "/"))
            {
                strcpy(folder, global_settings.start_directory);
            }
            else
            {
#ifdef HAVE_HOTSWAP
                bool in_hotswap = false;
                /* handle entering an ejected drive */
                int i;
                for (i = 0; i < NUM_VOLUMES; i++)
                {
                    char vol_string[VOL_MAX_LEN + 1];
                    if (!volume_removable(i))
                        continue;
                    get_volume_name(i, vol_string);
                    /* test whether we would browse the external card */
                    if (!volume_present(i) &&
                            (strstr(last_folder, vol_string)
#ifdef HAVE_HOTSWAP_STORAGE_AS_MAIN
                                                                || (i == 0)
#endif
                                                                ))
                    {   /* leave folder as "/" to avoid crash when trying
                         * to access an ejected drive */
                        strcpy(folder, "/");
                        in_hotswap = true;
                        break;
                    }
                }
                if (!in_hotswap)
#endif /*HAVE_HOTSWAP*/
                    strcpy(folder, last_folder);
            }
            push_current_activity(ACTIVITY_FILEBROWSER);
        break;
#ifdef HAVE_TAGCACHE
        case GO_TO_DBBROWSER:
            if (!tagcache_is_usable())
            {
                long next_recovery = 0;

                /* Now display progress until it's ready or the user exits */
                while(!tagcache_is_usable())
                {
                    struct tagcache_stat *stat = tagcache_get_stat();

                    /* Allow user to exit */
                    if (action_userabort(HZ/2))
                        break;

                    /* Maybe just needs to reboot due to delayed commit */
                    if (stat->commit_delayed)
                    {
                        splash(HZ*2, ID2P(LANG_PLEASE_REBOOT));
                        break;
                    }

                    /* Check if ready status is known */
                    if (!stat->readyvalid)
                    {
                        splash(0, ID2P(LANG_TAGCACHE_BUSY));
                        continue;
                    }

                    /* Revalidate a previously deployed database without
                     * deleting it. A transient playback/storage failure must
                     * never turn entering Music into an automatic rebuild. */
                    if (!stat->ready && stat->processed_entries == 0 &&
                        stat->commit_step == 0 &&
                        TIME_AFTER(current_tick, next_recovery))
                    {
                        next_recovery = current_tick + HZ * 3;
                        tagcache_recover();
                    }

                    /* Display building progress */
                    static long talked_tick = 0;
                    if(global_settings.talk_menu &&
                       (talked_tick == 0
                        || TIME_AFTER(current_tick, talked_tick+7*HZ)))
                    {
                        talked_tick = current_tick;
                        if (stat->commit_step > 0)
                        {
                            talk_id(LANG_TAGCACHE_INIT, false);
                            talk_number(stat->commit_step, true);
                            talk_id(VOICE_OF, true);
                            talk_number(tagcache_get_max_commit_step(), true);
                        } else if(stat->processed_entries)
                        {
                            talk_number(stat->processed_entries, false);
                            talk_id(LANG_BUILDING_DATABASE, true);
                        }
                    }
                    if (stat->commit_step > 0)
                    {
                        int max_step = tagcache_get_max_commit_step();
                        const char *stage = tagcache_commit_stage_name(stat);
                        /* (prevent redundant voicing by splash_progress */
                        bool tmp = global_settings.talk_menu;
                        global_settings.talk_menu = false;

                        if (lang_is_rtl())
                        {
                            if (stage)
                                splash_progress(stat->commit_step, max_step,
                                                "[%d/%d] %s: %s",
                                                stat->commit_step, max_step,
                                                stage, str(LANG_TAGCACHE_INIT));
                            else
                                splash_progress(stat->commit_step, max_step,
                                                "[%d/%d] %s",
                                                stat->commit_step, max_step,
                                                str(LANG_TAGCACHE_INIT));
                        }
                        else
                        {
                            if (stage)
                                splash_progress(stat->commit_step, max_step,
                                                "%s: %s [%d/%d]",
                                                str(LANG_TAGCACHE_INIT), stage,
                                                stat->commit_step, max_step);
                            else
                                splash_progress(stat->commit_step, max_step,
                                                "%s [%d/%d]",
                                                str(LANG_TAGCACHE_INIT),
                                                stat->commit_step, max_step);
                        }
                        global_settings.talk_menu = tmp;
                    }
                    else if (stat->progress >= 0)
                    {
                        const char *progress_text = str(LANG_BUILDING_DATABASE);
                        if (stat->scan_status == TAGCACHE_SCAN_UPDATING)
                            progress_text = "Updating database... %d found";
                        splash_progress(stat->progress, 100,
                                        progress_text,
                                        stat->processed_entries);
                    }
                    else
                    {
                        const char *progress_text = str(LANG_BUILDING_DATABASE);
                        if (stat->scan_status == TAGCACHE_SCAN_UPDATING)
                            progress_text = "Updating database... %d found";
                        splashf(0, progress_text,
                                   stat->processed_entries); /* (voiced above) */
                    }
                }
            }
            if (!tagcache_is_usable())
                return browser_finish_native_screen(ipodjs_native,
                                                    GO_TO_PREVIOUS);
            filter = SHOW_ID3DB;
            last_ft_dirlevel = tc->dirlevel;
            tc->dirlevel = last_db_dirlevel;
            tc->selected_item = last_db_selection;
            push_current_activity(ACTIVITY_DATABASEBROWSER);
        break;
#endif /*HAVE_TAGCACHE*/
    }

    struct browse_context browse = {
        .dirfilter = filter,
        .icon = Icon_NOICON,
        .root = folder,
    };

    ret_val = rockbox_browse(&browse);

    if (ret_val == GO_TO_WPS
        || ret_val == GO_TO_PREVIOUS_MUSIC
        || ret_val == GO_TO_PLUGIN)
        pop_current_activity_without_refresh();
    else
        pop_current_activity();

    switch ((intptr_t)param)
    {
        case GO_TO_FILEBROWSER:
            if (!get_current_file(last_folder, MAX_PATH) ||
                (!strchr(&last_folder[1], '/') &&
                 global_settings.start_directory[1] != '\0'))
            {
                last_folder[0] = '/';
                last_folder[1] = '\0';
            }
        break;
#ifdef HAVE_TAGCACHE
        case GO_TO_DBBROWSER:
            last_db_dirlevel = tc->dirlevel;
            last_db_selection = tc->selected_item;
            tc->dirlevel = last_ft_dirlevel;
        break;
#endif
    }
    return browser_finish_native_screen(ipodjs_native, ret_val);
}

#define VIDEO_BROWSER_MAX_FILES 192
#define VIDEO_BROWSER_MAX_DEPTH 6
#define VIDEO_BROWSER_TITLE_MAX 64
#define VIDEO_LIST_INDEX ROCKBOX_DIR "/videolist/index.tsv"
#define VIDEO_LIST_ROOT ROCKBOX_DIR "/videolist"
#define VIDEO_LIST_LOCK_PIN VIDEO_LIST_ROOT "/locked.pin"
#define VIDEO_LIST_NETFLIX_LAST VIDEO_LIST_ROOT "/netflix-last.path"
#define VIDEO_LIST_NETFLIX_WATCHED VIDEO_LIST_ROOT "/netflix-watched.tsv"
/* Watched paths are held as CRCs, not strings: 256 CRCs cost 1 KiB where the
 * paths themselves would cost well over 100 KiB. A collision would show one
 * spurious checkmark, which is the cheapest possible failure here. */
#define VIDEO_LIST_WATCHED_MAX 256
#define VIDEO_LIST_MPEG_RESUME \
    ROCKBOX_DIR "/rocks/apps/mpegplayer.cfg"
#define VIDEO_LIST_RVP_RESUME \
    ROCKBOX_DIR "/rocks/apps/openh264-resume-%08lx.dat"
#define VIDEO_LIST_RVP_RESUME_MAGIC 0x52565031u
#define VIDEO_LIST_MPEG_TS_SECOND 45000
#define VIDEO_LIST_THUMB_SIZE 32
#define VIDEO_LIST_NETFLIX_POSTER_W 28
#define VIDEO_LIST_NETFLIX_POSTER_H 42
#define VIDEO_LIST_NETFLIX_LANDING_W 72
#define VIDEO_LIST_NETFLIX_LANDING_H 108
#define VIDEO_LIST_NETFLIX_DETAIL_W 96
#define VIDEO_LIST_NETFLIX_DETAIL_H 144
#define VIDEO_LIST_NETFLIX_LANDING_CACHE 3
#define VIDEO_LIST_NETFLIX_WATCHED_SIZE 16
/* Netflix brand red and a single derived gradient/shade family, reused
 * everywhere the Netflix appearance draws a red accent. #B4131D is the
 * dominant red measured across the shipped period wordmark
 * netflix-logo-2001.150x70x24.bmp, so the top bar and the iPodJS home right
 * pane match the logo drawn on them. The modern ribbon red #E50914 belongs to
 * a later mark and is deliberately not used here. */
#define VIDEO_LIST_NETFLIX_RED       LCD_RGBPACK(180, 19, 29)
#define VIDEO_LIST_NETFLIX_RED_DARK  LCD_RGBPACK(112, 12, 18)
#define VIDEO_LIST_ART_ID_LEN 25
#define VIDEO_LIST_LOCK_ART_ID "__locked__"
#define VIDEO_LIST_MOVIES_ART_ID "__netflix_movies__"
#define VIDEO_LIST_SHOWS_ART_ID "__netflix_shows__"
#define VIDEO_LIST_MUSIC_ART_ID "__netflix_music__"
#define VIDEO_LIST_CONCERTS_ART_ID "__netflix_concerts__"
#define VIDEO_LIST_HOME_ART_ID "__netflix_home__"
#define VIDEO_LIST_MANIFEST_LINE_MAX 1024
#define VIDEO_LIST_BITMAP_MAX_H VIDEO_LIST_NETFLIX_POSTER_H
#define VIDEO_LIST_TEXT_PAD 6
#define VIDEO_LIST_LOOKUP_CACHE 16
#define VIDEO_LIST_BITMAP_CACHE 8
#define VIDEO_LIST_PATH_LEN MAX_PATH
#ifdef HAVE_IPODJS_UI
#define VIDEO_PIN_PANEL_ASSET \
    ROCKBOX_DIR "/ipodjs/apple/fast-scroll-blank.apple.95x82x32.bmp"
#define VIDEO_PIN_FIELD_ASSET \
    ROCKBOX_DIR "/ipodjs/apple/search-field.apple.97x32x24.bmp"
#define VIDEO_PIN_SELECTED_ASSET \
    ROCKBOX_DIR "/ipodjs/apple/search-selected.apple.97x32x24.bmp"
#define VIDEO_PIN_PANEL_W 95
#define VIDEO_PIN_PANEL_H 82
#define VIDEO_PIN_SURFACE_W 97
#define VIDEO_PIN_SURFACE_H 32
#define VIDEO_PIN_PANEL_ALPHA_BYTES \
    (((VIDEO_PIN_PANEL_W + 1) / 2) * VIDEO_PIN_PANEL_H)
#endif

struct video_entry
{
    char path[MAX_PATH];
    char title[VIDEO_BROWSER_TITLE_MAX];
    char art_id[VIDEO_LIST_ART_ID_LEN];
    char format[8];  /* File extension for display (e.g., "MPEG", "MP4") */
    bool is_directory;
    bool playable;
    off_t filesize;  /* File size in bytes */
    time_t mtime;    /* Modification time for sorting */
    int width;       /* Video width (0 = unknown) */
    int height;      /* Video height (0 = unknown) */
    unsigned duration_sec;  /* Duration in seconds (0 = unknown) */
    int year;
    bool is_resume;
    unsigned resume_percent;
    bool watched;    /* Played to the end at least once */
};

struct video_rvp_resume_record
{
    uint32_t magic;
    uint32_t path_crc;
    int32_t frame;
    int32_t total_frames;
};

enum video_sort_order
{
    VIDEO_SORT_NAME_AZ,     /* Name A-Z */
    VIDEO_SORT_NAME_ZA,     /* Name Z-A */
    VIDEO_SORT_DATE_NEW,    /* Newest first */
    VIDEO_SORT_DATE_OLD,    /* Oldest first */
    VIDEO_SORT_SIZE_LARGE,  /* Largest first */
    VIDEO_SORT_SIZE_SMALL,  /* Smallest first */
};

struct video_browser_state
{
    struct video_entry entries[VIDEO_BROWSER_MAX_FILES];
    char current_path[VIDEO_LIST_PATH_LEN];
    int depth;
    char parent_path_stack[VIDEO_BROWSER_MAX_DEPTH][VIDEO_LIST_PATH_LEN];
    int parent_selection_stack[VIDEO_BROWSER_MAX_DEPTH];
    int count;
    int selection;
    int next_screen;
    bool truncated;
    enum video_sort_order sort_order;
};

enum root_menu_video_preview_source {
    IPODJS_PREVIEW_NONE = 0,
    IPODJS_PREVIEW_MUSIC,
    IPODJS_PREVIEW_VIDEOS,
    IPODJS_PREVIEW_PHOTOS,
    IPODJS_PREVIEW_GAMES,
    IPODJS_PREVIEW_POKEMINI,
    IPODJS_PREVIEW_MAGAZINES,
    IPODJS_PREVIEW_COMICS,
    IPODJS_PREVIEW_AVATAR,
};

#ifdef HAVE_LCD_COLOR
struct video_thumb_lookup_slot {
    bool valid;
    bool found;
    bool is_directory;
    char video_path[VIDEO_LIST_PATH_LEN];
    char thumb_path[VIDEO_LIST_PATH_LEN];
};

struct video_thumb_bitmap_slot {
    bool valid;
    unsigned long last_used;
    char path[VIDEO_LIST_PATH_LEN];
    struct bitmap bm;
    unsigned char data[BM_SIZE(VIDEO_LIST_THUMB_SIZE, VIDEO_LIST_BITMAP_MAX_H,
                               FORMAT_NATIVE, false)];
};

struct video_netflix_landing_slot {
    bool valid;
    unsigned long last_used;
    char path[VIDEO_LIST_PATH_LEN];
    struct bitmap bm;
    unsigned char data[
        BM_SIZE(VIDEO_LIST_NETFLIX_LANDING_W,
                VIDEO_LIST_NETFLIX_LANDING_H,
                FORMAT_NATIVE, false)];
};

static struct video_thumb_lookup_slot video_thumb_lookup_cache[VIDEO_LIST_LOOKUP_CACHE];
static int video_thumb_lookup_victim;
static struct video_thumb_bitmap_slot video_thumb_bitmap_cache[VIDEO_LIST_BITMAP_CACHE];
static unsigned long video_thumb_bitmap_tick;
static struct video_netflix_landing_slot
    video_netflix_landing_cache[VIDEO_LIST_NETFLIX_LANDING_CACHE];
static unsigned long video_netflix_landing_tick;

#ifdef HAVE_IPODJS_UI
struct video_netflix_detail {
    char genre[48];
    char content_rating[16];
    char plot[192];
    int rating;
};

static struct bitmap video_netflix_detail_bm;
static unsigned char video_netflix_detail_data[
    BM_SIZE(VIDEO_LIST_NETFLIX_DETAIL_W, VIDEO_LIST_NETFLIX_DETAIL_H,
            FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
static bool video_netflix_detail_valid;
static char video_netflix_detail_art_id[VIDEO_LIST_ART_ID_LEN];
/* Description shown on the landing screen for the selected row, resolved at
 * the bounded service point so the draw only reads cached text. */
static char video_netflix_landing_plot[192];
#endif

#ifdef HAVE_IPODJS_UI
struct videos_pin_surfaces {
    struct bitmap panel;
    unsigned char panel_data[
        BM_SIZE(VIDEO_PIN_PANEL_W, VIDEO_PIN_PANEL_H,
                FORMAT_NATIVE, false) + VIDEO_PIN_PANEL_ALPHA_BYTES] IPODJS_BM_ALIGN;
    struct bitmap field;
    unsigned char field_data[
        BM_SIZE(VIDEO_PIN_SURFACE_W, VIDEO_PIN_SURFACE_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    struct bitmap selected;
    unsigned char selected_data[
        BM_SIZE(VIDEO_PIN_SURFACE_W, VIDEO_PIN_SURFACE_H,
                FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    bool tried;
    bool valid;
};

static struct videos_pin_surfaces videos_pin_surfaces;
#endif
#endif

static const char * const video_scan_roots[] = {
    "/Videos",
    "/Video",
    "/iPod_Control/Video",
    "/iPod_Control/Videos",
    "/iPod_Control/Movies",
};

#ifdef HAVE_LCD_COLOR
static int video_ascii_casecmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb)
            return ca - cb;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static void video_trim_line(char *line)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
        line[--len] = '\0';
}

static bool video_split_tsv(char *line, char *fields[], int field_count)
{
    int i;

    for (i = 0; i < field_count; i++)
    {
        fields[i] = line;
        char *tab = strchr(line, '\t');
        if (tab)
        {
            *tab = '\0';
            line = tab + 1;
        }
        else if (i != field_count - 1)
        {
            return false;
        }
    }
    return true;
}

static bool video_parse_manifest_line(const char *line, char *parsed,
                                      size_t parsed_size, char *fields[12])
{
    static char unlocked_field[] = "0";

    strmemccpy(parsed, line, parsed_size);
    if (video_split_tsv(parsed, fields, 12))
        return true;

    strmemccpy(parsed, line, parsed_size);
    if (!video_split_tsv(parsed, fields, 11))
        return false;

    fields[11] = unlocked_field;
    return true;
}

static bool video_parse_manifest_line_v4(const char *line, char *parsed,
                                         size_t parsed_size,
                                         char *fields[20])
{
    strmemccpy(parsed, line, parsed_size);
    return video_split_tsv(parsed, fields, 20);
}

static bool video_parse_manifest_line_v5(const char *line, char *parsed,
                                         size_t parsed_size,
                                         char *fields[22])
{
    strmemccpy(parsed, line, parsed_size);
    return video_split_tsv(parsed, fields, 22);
}

/* v6 appended show_plot. A device still holding a v5 manifest must keep
 * working, so fall back to the shorter split and report an empty synopsis
 * rather than losing the row's show and season art. */
static bool video_parse_manifest_line_v6(const char *line, char *parsed,
                                         size_t parsed_size,
                                         char *fields[23])
{
    static char empty_field[] = "";

    strmemccpy(parsed, line, parsed_size);
    if (video_split_tsv(parsed, fields, 23))
        return true;

    strmemccpy(parsed, line, parsed_size);
    if (!video_split_tsv(parsed, fields, 22))
        return false;

    fields[22] = empty_field;
    return true;
}

static void video_manifest_hierarchy_art_id(
    const char *line, bool season, const char *fallback,
    char *art_id, size_t art_id_size)
{
    char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
    char *fields[22];
    const char *selected = fallback;

    if (video_parse_manifest_line_v5(line, parsed, sizeof(parsed), fields))
    {
        const char *candidate = fields[season ? 21 : 20];
        if (candidate[0])
            selected = candidate;
    }
    strmemccpy(art_id, selected ? selected : "", art_id_size);
}

static bool video_manifest_entry_locked(char *fields[12])
{
    return fields[11][0] == '1';
}

static int video_manifest_year(const char *line)
{
    char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
    char *fields[20];

    if (!video_parse_manifest_line_v4(line, parsed, sizeof(parsed), fields))
        return 0;
    return atoi(fields[12]);
}

#ifdef HAVE_IPODJS_UI
static bool videos_load_pin_bitmap(const char *path, struct bitmap *bitmap,
                                   unsigned char *data, size_t data_size,
                                   int width, int height)
{
    int rc;

    if (!file_exists(path))
        return false;
    memset(bitmap, 0, sizeof(*bitmap));
    bitmap->width = width;
    bitmap->height = height;
    bitmap->format = FORMAT_NATIVE;
    bitmap->data = data;
    rc = read_bmp_file(path, bitmap, (int)data_size,
                       FORMAT_NATIVE | FORMAT_DITHER |
                       FORMAT_TRANSPARENT, NULL);
    return rc >= 0 && bitmap->width == width && bitmap->height == height;
}

static bool videos_load_pin_surfaces(void)
{
    if (videos_pin_surfaces.tried)
        return videos_pin_surfaces.valid;

    videos_pin_surfaces.tried = true;
    videos_pin_surfaces.valid =
        videos_load_pin_bitmap(
            VIDEO_PIN_PANEL_ASSET,
            &videos_pin_surfaces.panel, videos_pin_surfaces.panel_data,
            sizeof(videos_pin_surfaces.panel_data),
            VIDEO_PIN_PANEL_W, VIDEO_PIN_PANEL_H) &&
        videos_load_pin_bitmap(
            VIDEO_PIN_FIELD_ASSET,
            &videos_pin_surfaces.field, videos_pin_surfaces.field_data,
            sizeof(videos_pin_surfaces.field_data),
            VIDEO_PIN_SURFACE_W, VIDEO_PIN_SURFACE_H) &&
        videos_load_pin_bitmap(
            VIDEO_PIN_SELECTED_ASSET,
            &videos_pin_surfaces.selected,
            videos_pin_surfaces.selected_data,
            sizeof(videos_pin_surfaces.selected_data),
            VIDEO_PIN_SURFACE_W, VIDEO_PIN_SURFACE_H);
    return videos_pin_surfaces.valid;
}

static void videos_draw_tiled_pin_part(struct bitmap *bitmap,
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

            lcd_bmp_part(bitmap, part_src_x, part_src_y,
                         x + drawn_x, y + drawn_y, part_w, part_h);
            drawn_x += part_w;
        }
        drawn_y += part_h;
    }
}

static void videos_draw_pin_surface(struct bitmap *bitmap, int border,
                                    int x, int y, int width, int height)
{
    int center_w = width - border * 2;
    int center_h = height - border * 2;

    lcd_set_drawmode(DRMODE_FG);
    lcd_bmp_part(bitmap, 0, 0, x, y, border, border);
    lcd_bmp_part(bitmap, bitmap->width - border, 0,
                 x + width - border, y, border, border);
    lcd_bmp_part(bitmap, 0, bitmap->height - border,
                 x, y + height - border, border, border);
    lcd_bmp_part(bitmap, bitmap->width - border,
                 bitmap->height - border,
                 x + width - border, y + height - border,
                 border, border);
    videos_draw_tiled_pin_part(bitmap, border, 0,
        bitmap->width - border * 2, border,
        x + border, y, center_w, border);
    videos_draw_tiled_pin_part(bitmap, border, bitmap->height - border,
        bitmap->width - border * 2, border,
        x + border, y + height - border, center_w, border);
    videos_draw_tiled_pin_part(bitmap, 0, border, border,
        bitmap->height - border * 2,
        x, y + border, border, center_h);
    videos_draw_tiled_pin_part(bitmap, bitmap->width - border, border,
        border, bitmap->height - border * 2,
        x + width - border, y + border, border, center_h);
    videos_draw_tiled_pin_part(bitmap, border, border,
        bitmap->width - border * 2, bitmap->height - border * 2,
        x + border, y + border, center_w, center_h);
    lcd_set_drawmode(DRMODE_SOLID);
}

static void videos_draw_pin_prompt(const char *title, const char *pin,
                                   int digit)
{
    static const char digits[] = "0123456789";
    const int panel_x = 20;
    const int panel_y = LCD_HEIGHT - 69;
    const int panel_w = LCD_WIDTH - 42;
    const int panel_h = 50;
    const int field_x = 27;
    const int field_y = panel_y + 12;
    const int field_w = 68;
    const int field_h = 25;
    const int center_x = LCD_WIDTH / 2 - 9;
    char masked[5];
    int title_w;
    int text_h;
    int masked_w;
    int length = strlen(pin);
    int i;

    /* The Videos browser can leave its split-pane viewport and artwork
     * backdrop active.  PIN entry owns and repaints the full LCD. */
    lcd_set_viewport(NULL);
    lcd_set_backdrop(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(global_settings.ui_engine_dark_mode ?
                       LCD_RGBPACK(18, 20, 24) : LCD_WHITE);
    lcd_set_foreground(global_settings.ui_engine_dark_mode ?
                       LCD_RGBPACK(239, 242, 246) : LCD_BLACK);
    lcd_clear_display();
    lcd_setfont(FONT_UI);
    lcd_getstringsize(title, &title_w, &text_h);
    lcd_putsxy(MAX(4, (LCD_WIDTH - title_w) / 2), 38, title);
    lcd_putsxy(42, 78, "Scroll to choose, Select to enter");

    videos_draw_pin_surface(&videos_pin_surfaces.panel, 16,
                            panel_x, panel_y, panel_w, panel_h);
    videos_draw_pin_surface(&videos_pin_surfaces.field, 6,
                            field_x, field_y, field_w, field_h);

    for (i = 0; i < length && i < 4; i++)
        masked[i] = '*';
    masked[MIN(length, 4)] = '\0';
    lcd_getstringsize(masked, &masked_w, &text_h);
    lcd_set_foreground(LCD_RGBPACK(20, 24, 27));
    lcd_set_drawmode(DRMODE_FG);
    lcd_putsxy(MAX(field_x + 4, field_x + field_w - masked_w - 4),
               field_y + MAX(0, (field_h - text_h) / 2), masked);

    for (i = -5; i <= 6; i++)
    {
        int index = (digit + i + 20) % 10;
        int x = center_x + i * 19;
        char glyph[2] = {digits[index], '\0'};
        int glyph_w;
        int glyph_h;

        if (x < field_x + field_w + 9)
            continue;
        lcd_getstringsize(glyph, &glyph_w, &glyph_h);
        if (i == 0)
        {
            int selected_w = MAX(glyph_w + 6, 16);
            int selected_x = x - (selected_w - glyph_w) / 2;

            videos_draw_pin_surface(&videos_pin_surfaces.selected, 6,
                                    selected_x, panel_y + 10,
                                    selected_w, text_h + 3);
            lcd_set_foreground(LCD_WHITE);
        }
        else
            lcd_set_foreground(LCD_RGBPACK(239, 244, 246));
        lcd_set_drawmode(DRMODE_FG);
        lcd_putsxy(x, panel_y + 11, glyph);
    }
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_update();
}
#endif

/* Keep Locked Videos code entry identical to Photos: use the same Apple
 * surfaces, turn the wheel to choose, Select to enter, Left to erase, and
 * Menu to cancel.  Assets are loaded once before the cached draw loop. */
static int videos_prompt_pin_wheel(const char *title, char *pin,
                                   size_t pin_size)
{
#ifdef HAVE_IPODJS_UI
    struct viewport *old_viewport;
    fb_data *old_backdrop;
    int digit = 0;
    int result = -1;

    if (pin_size < 5 || global_settings.ui_engine != UI_ENGINE_IPODJS ||
        LCD_WIDTH < 220 || LCD_HEIGHT < 180 ||
        !videos_load_pin_surfaces())
        return -1;

    old_backdrop = lcd_get_backdrop();
    old_viewport = lcd_set_viewport(NULL);
    lcd_set_backdrop(NULL);
    pin[0] = '\0';
    while (result < 0)
    {
        int action;
        size_t length;

        videos_draw_pin_prompt(title, pin, digit);
        action = get_action(CONTEXT_STD, TIMEOUT_BLOCK);
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                digit = digit <= 0 ? 9 : digit - 1;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                digit = (digit + 1) % 10;
                break;
            case ACTION_STD_CANCEL:
                length = strlen(pin);
                if (length > 0)
                    pin[length - 1] = '\0';
                break;
            case ACTION_STD_OK:
                length = strlen(pin);
                if (length < 4)
                {
                    pin[length] = (char)('0' + digit);
                    pin[length + 1] = '\0';
                }
                if (length + 1 == 4)
                    result = 1;
                break;
            case ACTION_STD_MENU:
                result = 0;
                break;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                    result = 0;
                break;
        }
    }

    lcd_set_viewport(NULL);
    lcd_set_backdrop(old_backdrop);
    lcd_set_viewport(old_viewport);
    return result;
#else
    (void)title;
    (void)pin;
    (void)pin_size;
    return -1;
#endif
}

static bool videos_prompt_pin(const char *title, char *pin, size_t pin_size)
{
    int wheel_rc = videos_prompt_pin_wheel(title, pin, pin_size);
    int i;

    if (wheel_rc >= 0)
        return wheel_rc > 0;
    splash(HZ / 2, title);
    pin[0] = '\0';
    if (kbd_input(pin, pin_size, NULL) < 0 || strlen(pin) != 4)
        return false;
    for (i = 0; i < 4; i++)
        if (!isdigit((unsigned char)pin[i]))
            return false;
    return true;
}

/* Reads the shared privacy PIN.  Returns 0 when a valid four-digit PIN is
 * present, -1 when the file is missing or unreadable, and 1 when it holds
 * something the lock screens could never match. */
static int videos_read_shared_pin(char *pin, size_t pin_size)
{
    int fd;
    int i;

    if (!pin || pin_size < 5)
        return -1;
    pin[0] = '\0';
    fd = open(VIDEO_LIST_LOCK_PIN, O_RDONLY);
    if (fd < 0)
        return -1;
    if (read_line(fd, pin, (int)pin_size) <= 0)
    {
        close(fd);
        return -1;
    }
    close(fd);
    video_trim_line(pin);
    if (strlen(pin) != 4)
        return 1;
    for (i = 0; i < 4; i++)
        if (!isdigit((unsigned char)pin[i]))
            return 1;
    return 0;
}

static bool videos_unlock_with_shared_pin(const char *title,
                                          const char *unavailable,
                                          const char *invalid)
{
    char expected[16];
    char entered[16] = "";
    int rc = videos_read_shared_pin(expected, sizeof(expected));

    if (rc < 0)
    {
        splash(HZ * 2, unavailable);
        return false;
    }
    if (rc > 0)
    {
        splash(HZ * 2, invalid);
        return false;
    }

    if (!videos_prompt_pin(title, entered, sizeof(entered)))
        return false;
    if (strcmp(entered, expected) != 0)
    {
        splash(HZ * 2, "Wrong code");
        return false;
    }
    return true;
}

static bool videos_unlock_locked_category(void)
{
    return videos_unlock_with_shared_pin("Enter 4-digit code",
                                         "Locked Videos unavailable",
                                         "Locked Videos PIN invalid");
}

static bool videos_unlock_settings_menu(void)
{
    return videos_unlock_with_shared_pin("Unlock Settings",
                                         "Settings lock unavailable",
                                         "Settings lock PIN invalid");
}

/* "Lock Settings Menu" lives inside the very menu it guards, so a missing or
 * malformed PIN file must not lock the user out of the only place the switch
 * can be turned back off.  The lock only bites once RockPod has written a PIN
 * the unlock screen could actually match. */
static bool videos_settings_lock_active(void)
{
    char expected[16];

    return global_settings.ui_engine_lock_settings &&
           videos_read_shared_pin(expected, sizeof(expected)) == 0;
}

static bool video_manifest_path_matches(const char *entry_path,
                                        const char *manifest_path)
{
    const char *entry_rel = entry_path;

    if (!entry_path || !manifest_path || !manifest_path[0])
        return false;

    if (entry_rel[0] == '/')
        entry_rel++;

    if (video_ascii_casecmp(entry_rel, manifest_path) == 0)
        return true;

    return manifest_path[0] == '/' &&
           video_ascii_casecmp(entry_path, manifest_path) == 0;
}

static bool video_manifest_path_is_descendant(const char *directory_path,
                                              const char *manifest_path)
{
    const char *directory = directory_path;
    const char *candidate = manifest_path;
    size_t length;
    size_t i;

    if (!directory || !candidate || !directory[0] || !candidate[0])
        return false;

    while (*directory == '/')
        directory++;
    while (*candidate == '/')
        candidate++;

    length = strlen(directory);
    while (length > 0 && directory[length - 1] == '/')
        length--;
    if (length == 0)
        return false;

    for (i = 0; i < length; i++)
    {
        if (!candidate[i] ||
            tolower((unsigned char)directory[i]) !=
            tolower((unsigned char)candidate[i]))
            return false;
    }

    return candidate[length] == '/';
}

static bool video_find_manifest_thumb(const char *video_path,
                                      bool is_directory,
                                      char *thumb_path,
                                      size_t thumb_path_size)
{
    int fd = open(VIDEO_LIST_INDEX, O_RDONLY);
    if (fd < 0)
        return false;

    char line[VIDEO_LIST_MANIFEST_LINE_MAX];
    bool found = false;
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
        char *fields12[12];
        char *fields7[7];
        char *fields6[6];
        const char *thumb = "";
        const char *device_path = "";

        video_trim_line(line);
        if (line[0] == '#' || line[0] == '\0' ||
            strncmp(line, "video_id\t", 9) == 0)
            continue;

        if (video_parse_manifest_line(line, parsed, sizeof(parsed), fields12))
        {
            if (video_manifest_entry_locked(fields12) && is_directory)
                continue;
            thumb = fields12[1];
            device_path = fields12[6];
        }
        else
        {
            strmemccpy(parsed, line, sizeof(parsed));
            if (video_split_tsv(parsed, fields7, 7))
            {
                thumb = fields7[1];
                device_path = fields7[6];
            }
            else
            {
                strmemccpy(parsed, line, sizeof(parsed));
                if (!video_split_tsv(parsed, fields6, 6))
                    continue;
                thumb = fields6[1];
                device_path = fields6[5];
            }
        }

        if (!thumb[0])
            continue;

        if ((!is_directory &&
             video_manifest_path_matches(video_path, device_path)) ||
            (is_directory &&
             video_manifest_path_is_descendant(video_path, device_path)))
        {
            snprintf(thumb_path, thumb_path_size, "%s/%s",
                     VIDEO_LIST_ROOT, thumb);
            found = true;
            break;
        }
    }

    close(fd);
    return found;
}

static bool video_lookup_thumb_path(const char *video_path,
                                    bool is_directory,
                                    char *thumb_path,
                                    size_t thumb_path_size)
{
    int i;

    for (i = 0; i < VIDEO_LIST_LOOKUP_CACHE; i++)
    {
        struct video_thumb_lookup_slot *slot = &video_thumb_lookup_cache[i];
        if (!slot->valid)
            continue;
        if (slot->is_directory == is_directory &&
            video_ascii_casecmp(slot->video_path, video_path) == 0)
        {
            if (slot->found)
                strmemccpy(thumb_path, slot->thumb_path, thumb_path_size);
            return slot->found;
        }
    }

    struct video_thumb_lookup_slot *slot =
        &video_thumb_lookup_cache[video_thumb_lookup_victim];
    video_thumb_lookup_victim =
        (video_thumb_lookup_victim + 1) % VIDEO_LIST_LOOKUP_CACHE;
    slot->valid = true;
    slot->is_directory = is_directory;
    strmemccpy(slot->video_path, video_path, sizeof(slot->video_path));
    slot->found = video_find_manifest_thumb(video_path, is_directory,
                                            slot->thumb_path,
                                            sizeof(slot->thumb_path));
    if (slot->found)
        strmemccpy(thumb_path, slot->thumb_path, thumb_path_size);
    return slot->found;
}

static struct video_thumb_bitmap_slot *video_thumb_bitmap_cache_victim(void)
{
    int victim = 0;
    int i;
    unsigned long oldest = video_thumb_bitmap_cache[0].last_used;

    for (i = 0; i < VIDEO_LIST_BITMAP_CACHE; i++)
    {
        if (!video_thumb_bitmap_cache[i].valid)
            return &video_thumb_bitmap_cache[i];
        if (video_thumb_bitmap_cache[i].last_used < oldest)
        {
            oldest = video_thumb_bitmap_cache[i].last_used;
            victim = i;
        }
    }

    return &video_thumb_bitmap_cache[victim];
}

static struct bitmap *video_load_thumb_bitmap(const char *path,
                                               int width, int height)
{
    int i;

    for (i = 0; i < VIDEO_LIST_BITMAP_CACHE; i++)
    {
        struct video_thumb_bitmap_slot *slot = &video_thumb_bitmap_cache[i];
        if (slot->valid && strcmp(slot->path, path) == 0)
        {
            slot->last_used = ++video_thumb_bitmap_tick;
            return &slot->bm;
        }
    }

    struct video_thumb_bitmap_slot *slot = video_thumb_bitmap_cache_victim();
    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->bm.width = width;
    slot->bm.height = height;
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;

    int rc = read_bmp_file(path, &slot->bm, sizeof(slot->data),
                           FORMAT_NATIVE | FORMAT_DITHER, NULL);
    if (rc < 0)
    {
        slot->valid = false;
        return NULL;
    }

    slot->valid = true;
    slot->last_used = ++video_thumb_bitmap_tick;
    strmemccpy(slot->path, path, sizeof(slot->path));
    return &slot->bm;
}

static struct bitmap *video_find_thumb_bitmap(const char *path)
{
    int i;

    for (i = 0; i < VIDEO_LIST_BITMAP_CACHE; i++)
    {
        struct video_thumb_bitmap_slot *slot = &video_thumb_bitmap_cache[i];

        if (slot->valid && strcmp(slot->path, path) == 0)
        {
            slot->last_used = ++video_thumb_bitmap_tick;
            return &slot->bm;
        }
    }
    return NULL;
}

static struct video_netflix_landing_slot *
video_netflix_landing_cache_victim(void)
{
    int victim = 0;
    int i;
    unsigned long oldest = video_netflix_landing_cache[0].last_used;

    for (i = 0; i < VIDEO_LIST_NETFLIX_LANDING_CACHE; i++)
    {
        if (!video_netflix_landing_cache[i].valid)
            return &video_netflix_landing_cache[i];
        if (video_netflix_landing_cache[i].last_used < oldest)
        {
            oldest = video_netflix_landing_cache[i].last_used;
            victim = i;
        }
    }
    return &video_netflix_landing_cache[victim];
}

static struct bitmap *video_find_netflix_landing_bitmap(const char *path)
{
    int i;

    for (i = 0; i < VIDEO_LIST_NETFLIX_LANDING_CACHE; i++)
    {
        struct video_netflix_landing_slot *slot =
            &video_netflix_landing_cache[i];
        if (slot->valid && strcmp(slot->path, path) == 0)
        {
            slot->last_used = ++video_netflix_landing_tick;
            return &slot->bm;
        }
    }
    return NULL;
}

static struct bitmap *video_load_netflix_landing_bitmap(const char *path)
{
    struct video_netflix_landing_slot *slot;
    struct bitmap *cached = video_find_netflix_landing_bitmap(path);

    if (cached)
        return cached;

    slot = video_netflix_landing_cache_victim();
    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->bm.width = VIDEO_LIST_NETFLIX_LANDING_W;
    slot->bm.height = VIDEO_LIST_NETFLIX_LANDING_H;
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;
    if (read_bmp_file(path, &slot->bm, sizeof(slot->data),
                      FORMAT_NATIVE | FORMAT_DITHER, NULL) < 0)
    {
        slot->valid = false;
        return NULL;
    }

    slot->valid = true;
    slot->last_used = ++video_netflix_landing_tick;
    strmemccpy(slot->path, path, sizeof(slot->path));
    return &slot->bm;
}

static const char *video_netflix_category_asset(const char *art_id)
{
    if (!strcmp(art_id, VIDEO_LIST_MOVIES_ART_ID))
        return "movies";
    if (!strcmp(art_id, VIDEO_LIST_SHOWS_ART_ID))
        return "tv-shows";
    if (!strcmp(art_id, VIDEO_LIST_MUSIC_ART_ID))
        return "music-videos";
    if (!strcmp(art_id, VIDEO_LIST_CONCERTS_ART_ID))
        return "music-videos";
    if (!strcmp(art_id, VIDEO_LIST_HOME_ART_ID))
        return "home-videos";
    return NULL;
}

static bool video_entry_netflix_landing_path(const struct video_entry *entry,
                                             char *path, size_t path_size)
{
    const char *category;

    if (!entry || !entry->art_id[0])
        return false;
    category = video_netflix_category_asset(entry->art_id);
    if (category)
    {
        snprintf(path, path_size,
                 ROCKBOX_DIR
                 "/ipodjs/netflix/categories/%s.72x108x24.bmp",
                 category);
        return true;
    }
    if (!strcmp(entry->art_id, VIDEO_LIST_LOCK_ART_ID))
    {
        strmemccpy(path,
            ROCKBOX_DIR "/ipodjs/netflix/locked.72x108x24.bmp",
            path_size);
        return true;
    }
    snprintf(path, path_size, "%s/netflix-landing/%s.bmp",
             VIDEO_LIST_ROOT, entry->art_id);
    return true;
}

static void videos_cache_netflix_landing_window(
    struct video_browser_state *state, int selected)
{
    int offset;

    if (!state || state->count <= 0)
        return;
    selected = MAX(0, MIN(selected, state->count - 1));
    for (offset = -1; offset <= 1; offset++)
    {
        int index = (selected + offset + state->count) % state->count;
        char path[MAX_PATH];

        if (!video_entry_netflix_landing_path(&state->entries[index],
                                              path, sizeof(path)) ||
            video_find_netflix_landing_bitmap(path))
            continue;
        (void)video_load_netflix_landing_bitmap(path);
        if (button_queue_count() > 0)
            break;
    }
}

static bool videos_netflix_appearance(void)
{
#ifdef HAVE_IPODJS_UI
    return global_settings.ui_engine == UI_ENGINE_IPODJS &&
           global_settings.ui_engine_video_appearance ==
               UI_ENGINE_VIDEO_NETFLIX;
#else
    return false;
#endif
}

static bool video_entry_art_path(const struct video_entry *entry,
                                 bool detail, char *path,
                                 size_t path_size)
{
    const char *category;

    if (!entry || !entry->art_id[0] || !path || path_size == 0)
        return false;

    category = video_netflix_category_asset(entry->art_id);
    if (category)
    {
        snprintf(path, path_size,
                 ROCKBOX_DIR "/ipodjs/netflix/categories/%s.%s.bmp",
                 category, detail ? "96x144x24" : "72x108x24");
        return true;
    }
    if (!strcmp(entry->art_id, VIDEO_LIST_LOCK_ART_ID))
    {
        if (videos_netflix_appearance())
            strmemccpy(path, detail ?
                ROCKBOX_DIR "/ipodjs/netflix/locked.96x144x24.bmp" :
                ROCKBOX_DIR "/ipodjs/netflix/locked.28x42x24.bmp",
                path_size);
        else
            strmemccpy(path,
                ROCKBOX_DIR "/ipodjs/netflix/locked.32x32x24.bmp",
                path_size);
        return true;
    }

    if (videos_netflix_appearance())
    {
        snprintf(path, path_size, "%s/%s/%s.bmp", VIDEO_LIST_ROOT,
                 detail ? "netflix-detail" : "netflix", entry->art_id);
    }
    else
    {
        snprintf(path, path_size, "%s/thumbs/%s.bmp", VIDEO_LIST_ROOT,
                 entry->art_id);
    }
    return true;
}

static void videos_cache_art_window(struct video_browser_state *state,
                                    int selected)
{
    int first;
    int last;
    int i;

    if (!state || state->count <= 0)
        return;

    selected = MAX(0, MIN(selected, state->count - 1));
    first = MAX(0, selected - 3);
    last = MIN(state->count, first + VIDEO_LIST_BITMAP_CACHE);
    first = MAX(0, last - VIDEO_LIST_BITMAP_CACHE);

    for (i = first; i < last; i++)
    {
        char path[VIDEO_LIST_PATH_LEN];
        int width = videos_netflix_appearance() ?
                    VIDEO_LIST_NETFLIX_POSTER_W : VIDEO_LIST_THUMB_SIZE;
        int height = videos_netflix_appearance() ?
                     VIDEO_LIST_NETFLIX_POSTER_H : VIDEO_LIST_THUMB_SIZE;

        if (videos_netflix_appearance() &&
            video_netflix_category_asset(state->entries[i].art_id))
            continue;
        if (!video_entry_art_path(&state->entries[i], false,
                                  path, sizeof(path)) &&
            !video_lookup_thumb_path(state->entries[i].path,
                                     state->entries[i].is_directory,
                                     path, sizeof(path)))
            continue;
        if (video_find_thumb_bitmap(path))
            continue;
        (void)video_load_thumb_bitmap(path, width, height);
        if (button_queue_count() > 0)
            break;
    }
}
#endif

static bool is_supported_video_ext(const char *ext)
{
    return !strcasecmp(ext, "mpg") || !strcasecmp(ext, "mpeg")
        || !strcasecmp(ext, "mpv") || !strcasecmp(ext, "m2v")
        || !strcasecmp(ext, "h264")
        || !strcasecmp(ext, "rvp");
}

static const char *video_plugin_for_ext(const char *ext)
{
    if (ext && (!strcasecmp(ext, "h264") || !strcasecmp(ext, "rvp")))
        return "openh264_player";

    return "mpegplayer";
}

static bool video_netflix_load_last_path(char *path, size_t path_size)
{
    int fd = open(VIDEO_LIST_NETFLIX_LAST, O_RDONLY);

    if (fd < 0)
        return false;
    if (read_line(fd, path, path_size) <= 0)
    {
        close(fd);
        return false;
    }
    close(fd);
    video_trim_line(path);
    return path[0] == '/';
}

static void video_netflix_forget_last(void)
{
    remove(VIDEO_LIST_NETFLIX_LAST);
}

static void video_netflix_remember_entry(const struct video_entry *entry)
{
    int fd;

    if (!entry || !entry->playable || entry->is_directory)
        return;
    mkdir(VIDEO_LIST_ROOT);
    fd = open(VIDEO_LIST_NETFLIX_LAST,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    fdprintf(fd, "%s\n", entry->path);
    close(fd);
}

static int video_netflix_mpeg_resume_time(const char *path)
{
    char line[MAX_PATH + 32];
    int fd = open(VIDEO_LIST_MPEG_RESUME, O_RDONLY);

    if (fd < 0)
        return 0;
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *name;
        char *value;

        settings_parseline(line, &name, &value);
        if (!strcmp(name, path))
        {
            int resume_time = atoi(value);

            close(fd);
            return MAX(0, resume_time);
        }
    }
    close(fd);
    return 0;
}

static bool video_netflix_resume_progress(struct video_entry *entry)
{
    unsigned percent = 0;

    if (!entry || !entry->playable)
        return false;
    if (!strcasecmp(entry->format, "RVP"))
    {
        struct video_rvp_resume_record record;
        uint32_t crc = crc_32(entry->path, strlen(entry->path), 0xffffffff);
        char filename[MAX_PATH];
        int fd;

        snprintf(filename, sizeof(filename), VIDEO_LIST_RVP_RESUME,
                 (unsigned long)crc);
        fd = open(filename, O_RDONLY);
        if (fd < 0)
            return false;
        if (read(fd, &record, sizeof(record)) != sizeof(record))
        {
            close(fd);
            return false;
        }
        close(fd);
        if (record.magic != VIDEO_LIST_RVP_RESUME_MAGIC ||
            record.path_crc != crc || record.frame <= 0 ||
            record.total_frames <= 0 ||
            record.frame >= (record.total_frames * 95) / 100)
            return false;
        percent = (unsigned)
            ((uint64_t)record.frame * 100 / record.total_frames);
    }
    else
    {
        int resume_time = video_netflix_mpeg_resume_time(entry->path);

        if (resume_time <= 0)
            return false;
        if (entry->duration_sec > 0)
        {
            uint64_t resume_seconds =
                (uint64_t)resume_time / VIDEO_LIST_MPEG_TS_SECOND;

            percent = (unsigned)
                (resume_seconds * 100 / entry->duration_sec);
        }
        else
            percent = 20;
        if (percent >= 95)
            return false;
    }

    entry->resume_percent = MAX(1u, MIN(percent, 94u));
    return true;
}

#if defined(HAVE_LCD_COLOR) && defined(HAVE_IPODJS_UI)
static uint32_t video_netflix_watched_crc[VIDEO_LIST_WATCHED_MAX];
static int video_netflix_watched_count;

/* Read the completion record written by the players. mpegplayer clears its
 * resume point at end of stream, so "finished" cannot be recovered from
 * resume data and is tracked separately. */
static void video_netflix_load_watched(void)
{
    char line[MAX_PATH];
    int fd;

    video_netflix_watched_count = 0;
    fd = open(VIDEO_LIST_NETFLIX_WATCHED, O_RDONLY);
    if (fd < 0)
        return;

    while (video_netflix_watched_count < VIDEO_LIST_WATCHED_MAX &&
           read_line(fd, line, sizeof(line)) > 0)
    {
        video_trim_line(line);
        if (line[0] != '/')
            continue;
        video_netflix_watched_crc[video_netflix_watched_count++] =
            crc_32(line, strlen(line), 0xffffffff);
    }
    close(fd);
}

static bool video_netflix_path_watched(const char *path)
{
    uint32_t crc;
    int i;

    if (!path || path[0] != '/')
        return false;
    crc = crc_32(path, strlen(path), 0xffffffff);
    for (i = 0; i < video_netflix_watched_count; i++)
    {
        if (video_netflix_watched_crc[i] == crc)
            return true;
    }
    return false;
}

/* Stamp the loaded list once per scan. Draw callbacks then only read a bool,
 * never the storage. */
static void videos_netflix_apply_watched(struct video_browser_state *state)
{
    int i;

    if (!state)
        return;
    for (i = 0; i < state->count; i++)
    {
        struct video_entry *entry = &state->entries[i];

        /* Both players delete their resume record once a title is finished,
         * so completion is only ever known from the shared watched list. */
        entry->watched = !entry->is_directory && entry->playable &&
                         video_netflix_path_watched(entry->path);
    }
}
#endif

static int video_launch_entry_at(const struct video_entry *entry,
                                 bool from_beginning)
{
    const char *parameter = entry->path;
    int result;
#if defined(HAVE_LCD_COLOR) && defined(HAVE_IPODJS_UI)
    char netflix_parameter[MAX_PATH + 17];
    bool netflix = videos_netflix_appearance();

    if (netflix)
    {
        video_netflix_remember_entry(entry);
        snprintf(netflix_parameter, sizeof(netflix_parameter),
                 from_beginning ? "netflix-restart:%s" : "netflix:%s",
                 entry->path);
        parameter = netflix_parameter;
    }
#else
    (void)from_beginning;
#endif

    result = filetype_load_plugin(video_plugin_for_ext(entry->format),
                                  parameter);
#if defined(HAVE_LCD_COLOR) && defined(HAVE_IPODJS_UI)
    if (netflix)
    {
        struct video_entry checked = *entry;

        if (!video_netflix_resume_progress(&checked))
            video_netflix_forget_last();
    }
#endif
    return result;
}

static int video_launch_entry(const struct video_entry *entry)
{
    return video_launch_entry_at(entry, false);
}

static bool is_known_video_ext(const char *ext)
{
    return is_supported_video_ext(ext)
        || !strcasecmp(ext, "mp4")
        || !strcasecmp(ext, "m4v")
        || !strcasecmp(ext, "mov")
        || !strcasecmp(ext, "h264")
        || !strcasecmp(ext, "rvp");
}

static int video_entry_type_weight(bool is_directory);

/* Comparison functions for qsort */
static int video_compare_name_az(const void *a, const void *b)
{
    const struct video_entry *va = (const struct video_entry *)a;
    const struct video_entry *vb = (const struct video_entry *)b;

    if (va->is_directory != vb->is_directory)
        return video_entry_type_weight(va->is_directory) -
               video_entry_type_weight(vb->is_directory);

    return strcasecmp(va->title, vb->title);
}

static int video_compare_name_za(const void *a, const void *b)
{
    return -video_compare_name_az(a, b);
}

static int video_entry_type_weight(bool is_directory)
{
    return is_directory ? 0 : 1;
}

static int video_compare_date_new(const void *a, const void *b)
{
    const struct video_entry *va = (const struct video_entry *)a;
    const struct video_entry *vb = (const struct video_entry *)b;

    if (va->is_directory != vb->is_directory)
        return video_entry_type_weight(va->is_directory) -
               video_entry_type_weight(vb->is_directory);

    /* Newer files first (larger mtime first) */
    if (va->mtime > vb->mtime) return -1;
    if (va->mtime < vb->mtime) return 1;
    return 0;
}

static int video_compare_date_old(const void *a, const void *b)
{
    return -video_compare_date_new(a, b);
}

static int video_compare_size_large(const void *a, const void *b)
{
    const struct video_entry *va = (const struct video_entry *)a;
    const struct video_entry *vb = (const struct video_entry *)b;

    if (va->is_directory != vb->is_directory)
        return video_entry_type_weight(va->is_directory) -
               video_entry_type_weight(vb->is_directory);

    /* Larger files first */
    if (va->filesize > vb->filesize) return -1;
    if (va->filesize < vb->filesize) return 1;
    return 0;
}

static int video_compare_size_small(const void *a, const void *b)
{
    return -video_compare_size_large(a, b);
}

/* Sort videos by current sort order */
static void videos_sort(struct video_browser_state *state)
{
    int (*compare_func)(const void *, const void *);
    
    if (state->count <= 1)
        return;
    
    switch (state->sort_order)
    {
        case VIDEO_SORT_NAME_AZ:
            compare_func = video_compare_name_az;
            break;
        case VIDEO_SORT_NAME_ZA:
            compare_func = video_compare_name_za;
            break;
        case VIDEO_SORT_DATE_NEW:
            compare_func = video_compare_date_new;
            break;
        case VIDEO_SORT_DATE_OLD:
            compare_func = video_compare_date_old;
            break;
        case VIDEO_SORT_SIZE_LARGE:
            compare_func = video_compare_size_large;
            break;
        case VIDEO_SORT_SIZE_SMALL:
            compare_func = video_compare_size_small;
            break;
        default:
            compare_func = video_compare_name_az;
            break;
    }
    
    qsort(state->entries, state->count, sizeof(struct video_entry), compare_func);
}

static const char *video_get_ext(const char *name)
{
    const char *ext = strrchr(name, '.');

    if (!ext || !ext[1])
        return NULL;

    return ext + 1;
}

static bool videos_is_apple_style_name(const char *name)
{
    const char *p;

    if (strncasecmp(name, "M4V", 3))
        return false;

    p = name + 3;

    if (!*p)
        return false;

    while (*p)
    {
        if (!isdigit((unsigned char)*p))
            return false;

        p++;
    }

    return true;
}

static bool videos_is_apple_path(const char *path)
{
    return strstr(path, "/iPod_Control/") != NULL;
}

/* Remove common junk patterns from video titles */
static void videos_strip_junk(char *str)
{
    char *p, *start, *end;
    size_t len = strlen(str);
    
    /* Remove content in square brackets [2024], [1080p], [x264], etc. */
    while ((start = strchr(str, '[')) != NULL)
    {
        end = strchr(start, ']');
        if (end)
        {
            memmove(start, end + 1, strlen(end + 1) + 1);
        }
        else
            break;
    }
    
    /* Remove content in parentheses (2024), (BluRay), etc. */
    while ((start = strchr(str, '(')) != NULL)
    {
        end = strchr(start, ')');
        if (end)
        {
            memmove(start, end + 1, strlen(end + 1) + 1);
        }
        else
            break;
    }
    
    /* Remove common quality/codec markers that might remain */
    static const char * const junk_markers[] = {
        "1080p", "720p", "480p", "360p",
        "BluRay", "BRRip", "DVDRip", "WEBRip", "HDTV",
        "x264", "x265", "h264", "h265", "XviD", "DivX",
        "AAC", "AC3", "MP3",
        NULL
    };
    
    for (int i = 0; junk_markers[i]; i++)
    {
        /* Case-insensitive search */
        p = str;
        while (*p)
        {
            if (!strncasecmp(p, junk_markers[i], strlen(junk_markers[i])))
            {
                /* Check if it's a standalone word (not part of title) */
                bool is_start = (p == str || !isalnum((unsigned char)p[-1]));
                bool is_end = !isalnum((unsigned char)p[strlen(junk_markers[i])]);
                
                if (is_start && is_end)
                {
                    memmove(p, p + strlen(junk_markers[i]),
                            strlen(p + strlen(junk_markers[i])) + 1);
                    continue;
                }
            }
            p++;
        }
    }
    
    /* Trim leading/trailing spaces */
    p = str;
    while (isspace((unsigned char)*p))
        p++;
    if (p != str)
        memmove(str, p, strlen(p) + 1);
    
    len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1]))
        str[--len] = '\0';
}

static void videos_simplify_name(const char *name,
                                 char *title,
                                 size_t title_len)
{
    size_t n = 0;
    bool prev_space = true;
    char temp[VIDEO_BROWSER_TITLE_MAX];

    if (title_len == 0)
        return;

    /* First pass: convert separators to spaces, keep only alphanumeric + spaces */
    while (*name && n + 1 < title_len)
    {
        unsigned char c = (unsigned char)*name++;

        if (isalnum(c))
        {
            temp[n++] = c;
            prev_space = false;
            continue;
        }

        /* Convert common separators to spaces */
        if (c == ' ' || c == '_' || c == '-' || c == '.')
        {
            if (!prev_space && n + 1 < title_len)
            {
                temp[n++] = ' ';
                prev_space = true;
            }
        }
        /* Keep brackets and parens for junk detection */
        else if (c == '[' || c == ']' || c == '(' || c == ')')
        {
            if (n + 1 < title_len)
            {
                temp[n++] = c;
                prev_space = false;
            }
        }
    }

    /* Remove trailing space */
    if (n > 0 && temp[n - 1] == ' ')
        n--;

    temp[n] = '\0';

    /* Second pass: remove junk patterns */
    videos_strip_junk(temp);

    /* Final cleanup: normalize multiple spaces */
    n = 0;
    prev_space = true;
    for (const char *p = temp; *p && n + 1 < title_len; p++)
    {
        if (isspace((unsigned char)*p))
        {
            if (!prev_space)
            {
                title[n++] = ' ';
                prev_space = true;
            }
        }
        else
        {
            title[n++] = *p;
            prev_space = false;
        }
    }

    /* Remove trailing space */
    if (n > 0 && title[n - 1] == ' ')
        n--;

    title[n] = '\0';

    if (n == 0)
        strmemccpy(title, "Video", title_len);
}

static void videos_make_title(const char *path,
                              char *title,
                              size_t title_len)
{
    char base[VIDEO_BROWSER_TITLE_MAX];
    const char *name;
    const char *ext;

    name = strrchr(path, '/');
    name = (name && name[1]) ? name + 1 : path;

    strmemccpy(base, name, sizeof(base));

    ext = strrchr(base, '.');
    if (ext != NULL)
        *(char *)ext = '\0';

    if (videos_is_apple_path(path) && videos_is_apple_style_name(base))
    {
        snprintf(title, title_len, "Video %s", base + 3);
        return;
    }

    videos_simplify_name(base, title, title_len);
}

static void videos_make_dir_title(const char *path,
                                 char *title,
                                 size_t title_len)
{
    const char *name;

    if (!path || !title || !title_len)
        return;

    name = strrchr(path, '/');
    name = (name && name[1]) ? name + 1 : path;
    videos_simplify_name(name, title, title_len);
}

static bool videos_have_path(struct video_browser_state *state,
                             const char *path)
{
    int i;

    for (i = 0; i < state->count; i++)
    {
        if (!strcmp(state->entries[i].path, path))
            return true;
    }

    return false;
}

static void videos_add_file(struct video_browser_state *state,
                            const char *path,
                            bool is_directory,
                            const char *ext,
                            bool playable,
                            const struct dirinfo *info)
{
    struct video_entry *vid;
    
    if (state->count >= VIDEO_BROWSER_MAX_FILES)
    {
        state->truncated = true;
        return;
    }

    if (videos_have_path(state, path))
        return;

    vid = &state->entries[state->count];
    
    strmemccpy(vid->path, path, sizeof(vid->path));
    vid->art_id[0] = '\0';
    if (is_directory)
        videos_make_dir_title(path, vid->title, sizeof(vid->title));
    else
        videos_make_title(path, vid->title, sizeof(vid->title));
    
    /* Store format for display - normalize extension to uppercase */
    if (ext)
    {
        size_t i;
        for (i = 0; i < sizeof(vid->format) - 1 && ext[i]; i++)
            vid->format[i] = toupper((unsigned char)ext[i]);
        vid->format[i] = '\0';
    }
    else
    {
        vid->format[0] = '\0';
    }
    
    vid->is_directory = is_directory;
    vid->playable = is_directory ? false : playable;
    
    /* Store file metadata */
    if (info)
    {
        vid->filesize = info->size;
        vid->mtime = info->mtime;
    }
    else
    {
        vid->filesize = 0;
        vid->mtime = 0;
    }
    
    /* Duration and resolution are unknown at scan time */
    vid->width = 0;
    vid->height = 0;
    vid->duration_sec = 0;
    vid->year = 0;
    vid->is_resume = false;
    vid->resume_percent = 0;

    state->count++;
}

static bool videos_has_known_video(const char *dir, int depth)
{
    DIR *dp;
    struct dirent *entry;

    if (depth > VIDEO_BROWSER_MAX_DEPTH)
        return false;

    dp = opendir(dir);
    if (!dp)
        return false;

    while ((entry = readdir(dp)) != NULL)
    {
        struct dirinfo info;
        const char *ext;
        char fullpath[MAX_PATH];

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        info = dir_get_info(dp, entry);

        if (!strcmp(dir, "/"))
            snprintf(fullpath, sizeof(fullpath), "/%s", entry->d_name);
        else
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);

        if (info.attribute & ATTR_DIRECTORY)
        {
            if (videos_has_known_video(fullpath, depth + 1))
            {
                closedir(dp);
                return true;
            }
            continue;
        }

        ext = video_get_ext(entry->d_name);
        if (ext && is_known_video_ext(ext))
        {
            closedir(dp);
            return true;
        }
    }

    closedir(dp);
    return false;
}

static void videos_scan_single_dir(struct video_browser_state *state,
                                  const char *dir)
{
    DIR *dp;
    struct dirent *entry;

    if (state->truncated)
        return;

    dp = opendir(dir);
    if (!dp)
        return;

    while (!state->truncated && (entry = readdir(dp)) != NULL)
    {
        struct dirinfo info;
        const char *ext;
        char fullpath[MAX_PATH];

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        info = dir_get_info(dp, entry);

        if (!strcmp(dir, "/"))
            snprintf(fullpath, sizeof(fullpath), "/%s", entry->d_name);
        else
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);

        if (info.attribute & ATTR_DIRECTORY)
        {
            if (videos_has_known_video(fullpath, 1))
                videos_add_file(state, fullpath, true, NULL, false, &info);
            continue;
        }

        ext = video_get_ext(entry->d_name);
        if (!ext || !is_known_video_ext(ext))
            continue;

        videos_add_file(state, fullpath, false, ext, is_supported_video_ext(ext), &info);
    }

    closedir(dp);
}

static void videos_add_virtual_dir(struct video_browser_state *state,
                                   const char *path, const char *title)
{
    struct video_entry *vid;

    if (state->count >= VIDEO_BROWSER_MAX_FILES)
    {
        state->truncated = true;
        return;
    }

    vid = &state->entries[state->count];
    strmemccpy(vid->path, path, sizeof(vid->path));
    strmemccpy(vid->title, title, sizeof(vid->title));
    vid->art_id[0] = '\0';
    vid->format[0] = '\0';
    vid->is_directory = true;
    vid->playable = false;
    vid->filesize = 0;
    vid->mtime = 0;
    vid->width = 0;
    vid->height = 0;
    vid->duration_sec = 0;
    vid->year = 0;
    vid->is_resume = false;
    vid->resume_percent = 0;
    state->count++;
}

static void videos_init_virtual_video(struct video_entry *vid,
                                      const char *device_path,
                                      const char *title,
                                      unsigned duration_sec,
                                      const char *art_id,
                                      int year)
{
    const char *ext;

    snprintf(vid->path, sizeof(vid->path), "/%s", device_path);
    strmemccpy(vid->title, title, sizeof(vid->title));
    strmemccpy(vid->art_id, art_id ? art_id : "", sizeof(vid->art_id));

    ext = video_get_ext(device_path);
    if (ext)
    {
        size_t i;
        for (i = 0; i < sizeof(vid->format) - 1 && ext[i]; i++)
            vid->format[i] = toupper((unsigned char)ext[i]);
        vid->format[i] = '\0';
        vid->playable = is_supported_video_ext(ext);
    }
    else
    {
        vid->format[0] = '\0';
        vid->playable = false;
    }

    vid->is_directory = false;
    vid->filesize = 0;
    vid->mtime = 0;
    vid->width = 0;
    vid->height = 0;
    vid->duration_sec = duration_sec;
    vid->year = year;
    vid->is_resume = false;
    vid->resume_percent = 0;
}

static void videos_add_virtual_video(struct video_browser_state *state,
                                     const char *device_path,
                                     const char *title,
                                     unsigned duration_sec,
                                     const char *art_id,
                                     int year)
{
    if (state->count >= VIDEO_BROWSER_MAX_FILES)
    {
        state->truncated = true;
        return;
    }

    videos_init_virtual_video(&state->entries[state->count],
                              device_path, title, duration_sec,
                              art_id, year);
    state->count++;
}

static void videos_scan_virtual_dir(struct video_browser_state *state)
{
    state->count = 0;
    state->truncated = false;

    int fd = open(VIDEO_LIST_INDEX, O_RDONLY);
    if (fd < 0)
    {
        /* Fallback to local files scan if index doesn't exist */
        if (state->depth == 0 && state->current_path[0] == '\0')
        {
            int i;
            for (i = 0; i < (int)ARRAYLEN(video_scan_roots); i++)
                videos_scan_single_dir(state, video_scan_roots[i]);
        }
        else
        {
            videos_scan_single_dir(state, state->current_path);
        }
        return;
    }

    char line[VIDEO_LIST_MANIFEST_LINE_MAX];
    
    if (state->depth == 0)
    {
        struct video_entry resume_entry;
        char last_path[MAX_PATH];
        bool has_last_path =
            video_netflix_load_last_path(last_path, sizeof(last_path));
        bool found_last = false;

        /* Top Level Categories */
        videos_add_virtual_dir(state, "virtual:movies", "Movies");
        videos_add_virtual_dir(state, "virtual:shows", "TV Shows");
        videos_add_virtual_dir(state, "virtual:concerts", "Concerts");
        videos_add_virtual_dir(state, "virtual:music_videos", "Music Videos");
        videos_add_virtual_dir(state, "virtual:home_videos", "Home Videos");
        videos_add_virtual_dir(state, "virtual:locked", "Locked Videos");
        strmemccpy(state->entries[0].art_id, VIDEO_LIST_MOVIES_ART_ID,
                   sizeof(state->entries[0].art_id));
        strmemccpy(state->entries[1].art_id, VIDEO_LIST_SHOWS_ART_ID,
                   sizeof(state->entries[1].art_id));
        strmemccpy(state->entries[2].art_id, VIDEO_LIST_CONCERTS_ART_ID,
                   sizeof(state->entries[2].art_id));
        strmemccpy(state->entries[3].art_id, VIDEO_LIST_MUSIC_ART_ID,
                   sizeof(state->entries[3].art_id));
        strmemccpy(state->entries[4].art_id, VIDEO_LIST_HOME_ART_ID,
                   sizeof(state->entries[4].art_id));
        strmemccpy(state->entries[5].art_id, VIDEO_LIST_LOCK_ART_ID,
                   sizeof(state->entries[5].art_id));

        /* Resolve the last title while the manifest is already open. Artwork
         * decoding remains in the browser's bounded service point. */
        while (read_line(fd, line, sizeof(line)) > 0)
        {
            char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
            char *fields[12];
            const char *device_path;

            video_trim_line(line);
            if (line[0] == '#' || line[0] == '\0' ||
                strncmp(line, "video_id\t", 9) == 0 ||
                !video_parse_manifest_line(line, parsed,
                                           sizeof(parsed), fields))
                continue;

            if (!has_last_path || found_last ||
                video_manifest_entry_locked(fields))
                continue;
            device_path = last_path[0] == '/' ? last_path + 1 : last_path;
            if (strcmp(fields[6], device_path))
                continue;

            videos_init_virtual_video(
                &resume_entry, fields[6], fields[3],
                (unsigned)atoi(fields[10]), fields[0],
                video_manifest_year(line));
            found_last = resume_entry.playable;
        }

        if (found_last && video_netflix_resume_progress(&resume_entry))
        {
            memmove(&state->entries[1], &state->entries[0],
                    state->count * sizeof(state->entries[0]));
            resume_entry.is_resume = true;
            state->entries[0] = resume_entry;
            state->count++;
        }
        else if (has_last_path)
            video_netflix_forget_last();
    }
    else if (strcmp(state->current_path, "virtual:movies") == 0)
    {
        /* List all movies */
        while (state->count < VIDEO_BROWSER_MAX_FILES && read_line(fd, line, sizeof(line)) > 0)
        {
            char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
            char *fields[12];
            video_trim_line(line);
            if (line[0] == '#' || line[0] == '\0' || strncmp(line, "video_id\t", 9) == 0)
                continue;
                
            if (!video_parse_manifest_line(line, parsed, sizeof(parsed), fields) ||
                (video_manifest_entry_locked(fields) &&
                 strcmp(state->current_path, "virtual:locked") != 0))
                continue;
                
            const char *kind = fields[4];
            if (strcmp(kind, "movie") != 0)
                continue;
                
            videos_add_virtual_video(state, fields[6], fields[3],
                                     (unsigned)atoi(fields[10]), fields[0],
                                     video_manifest_year(line));
        }
    }
    else if (strcmp(state->current_path, "virtual:shows") == 0)
    {
        /* List all unique TV shows */
        while (state->count < VIDEO_BROWSER_MAX_FILES && read_line(fd, line, sizeof(line)) > 0)
        {
            char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
            char *fields[12];
            char dir_path[MAX_PATH];
            char hierarchy_art_id[VIDEO_LIST_ART_ID_LEN];
            video_trim_line(line);
            if (line[0] == '#' || line[0] == '\0' || strncmp(line, "video_id\t", 9) == 0)
                continue;
                
            if (!video_parse_manifest_line(line, parsed, sizeof(parsed), fields) ||
                (video_manifest_entry_locked(fields) &&
                 strcmp(state->current_path, "virtual:locked") != 0))
                continue;
                
            const char *kind = fields[4];
            if (strcmp(kind, "show") != 0)
                continue;
                
            const char *show_name = fields[7];
            if (!show_name[0])
                show_name = "Unknown Show";
                
            /* Check if show already added */
            bool exists = false;
            int j;
            for (j = 0; j < state->count; j++)
            {
                if (strcmp(state->entries[j].title, show_name) == 0)
                {
                    exists = true;
                    break;
                }
            }
            if (exists)
                continue;
                
            snprintf(dir_path, sizeof(dir_path), "virtual:show:%s", show_name);
            videos_add_virtual_dir(state, dir_path, show_name);
            video_manifest_hierarchy_art_id(
                line, false, fields[0], hierarchy_art_id,
                sizeof(hierarchy_art_id));
            strmemccpy(state->entries[state->count - 1].art_id,
                       hierarchy_art_id,
                       sizeof(state->entries[state->count - 1].art_id));
        }
    }
    else if (strncmp(state->current_path, "virtual:show:", 13) == 0)
    {
        /* List unique seasons for selected show */
        const char *target_show = state->current_path + 13;
        while (state->count < VIDEO_BROWSER_MAX_FILES && read_line(fd, line, sizeof(line)) > 0)
        {
            char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
            char *fields[12];
            char dir_path[MAX_PATH];
            char hierarchy_art_id[VIDEO_LIST_ART_ID_LEN];
            video_trim_line(line);
            if (line[0] == '#' || line[0] == '\0' || strncmp(line, "video_id\t", 9) == 0)
                continue;
                
            if (!video_parse_manifest_line(line, parsed, sizeof(parsed), fields) ||
                (video_manifest_entry_locked(fields) &&
                 strcmp(state->current_path, "virtual:locked") != 0))
                continue;
                
            if (strcmp(fields[4], "show") != 0)
                continue;
                
            const char *show_name = fields[7][0] ? fields[7] : "Unknown Show";
            if (strcmp(show_name, target_show) != 0)
                continue;
                
            const char *season_num_str = fields[8];
            char season_title[32];
            if (!season_num_str[0] || strcmp(season_num_str, "0") == 0)
                strmemccpy(season_title, "Specials", sizeof(season_title));
            else
                snprintf(season_title, sizeof(season_title), "Season %s", season_num_str);
                
            /* Check if season already added */
            bool exists = false;
            int j;
            for (j = 0; j < state->count; j++)
            {
                if (strcmp(state->entries[j].title, season_title) == 0)
                {
                    exists = true;
                    break;
                }
            }
            if (exists)
                continue;
                
            snprintf(dir_path, sizeof(dir_path), "virtual:season:%s:%s",
                     target_show, season_num_str[0] ? season_num_str : "0");
            videos_add_virtual_dir(state, dir_path, season_title);
            video_manifest_hierarchy_art_id(
                line, true, fields[0], hierarchy_art_id,
                sizeof(hierarchy_art_id));
            strmemccpy(state->entries[state->count - 1].art_id,
                       hierarchy_art_id,
                       sizeof(state->entries[state->count - 1].art_id));
        }
    }
    else if (strncmp(state->current_path, "virtual:season:", 15) == 0)
    {
        /* List episodes inside selected show and season */
        char show_buf[128];
        strmemccpy(show_buf, state->current_path + 15, sizeof(show_buf));
        char *colon = strchr(show_buf, ':');
        if (colon)
        {
            *colon = '\0';
            const char *target_show = show_buf;
            const char *target_season = colon + 1;
            
            while (state->count < VIDEO_BROWSER_MAX_FILES && read_line(fd, line, sizeof(line)) > 0)
            {
                char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
                char *fields[12];
                char episode_title[VIDEO_BROWSER_TITLE_MAX];
                char hierarchy_art_id[VIDEO_LIST_ART_ID_LEN];
                video_trim_line(line);
                if (line[0] == '#' || line[0] == '\0' || strncmp(line, "video_id\t", 9) == 0)
                    continue;
                    
                if (!video_parse_manifest_line(line, parsed, sizeof(parsed), fields) ||
                    (video_manifest_entry_locked(fields) &&
                     strcmp(state->current_path, "virtual:locked") != 0))
                    continue;
                    
                if (strcmp(fields[4], "show") != 0)
                    continue;
                    
                const char *show_name = fields[7][0] ? fields[7] : "Unknown Show";
                if (strcmp(show_name, target_show) != 0)
                    continue;
                    
                const char *season_num_str = fields[8][0] ? fields[8] : "0";
                if (strcmp(season_num_str, target_season) != 0)
                    continue;
                    
                const char *ep_num_str = fields[9];
                if (ep_num_str[0])
                {
                    int season_num = atoi(season_num_str);
                    int ep_num = atoi(ep_num_str);

                    if (season_num > 0)
                    {
                        snprintf(episode_title, sizeof(episode_title),
                                 "S%02dE%02d - %s", season_num, ep_num,
                                 fields[3]);
                    }
                    else
                    {
                        snprintf(episode_title, sizeof(episode_title),
                                 "E%02d - %s", ep_num, fields[3]);
                    }
                }
                else
                    strmemccpy(episode_title, fields[3], sizeof(episode_title));
                
                video_manifest_hierarchy_art_id(
                    line, true, fields[0], hierarchy_art_id,
                    sizeof(hierarchy_art_id));
                videos_add_virtual_video(state, fields[6], episode_title,
                                         (unsigned)atoi(fields[10]),
                                         hierarchy_art_id,
                                         video_manifest_year(line));
            }
        }
    }
    else if (strcmp(state->current_path, "virtual:concerts") == 0)
    {
        while (state->count < VIDEO_BROWSER_MAX_FILES &&
               read_line(fd, line, sizeof(line)) > 0)
        {
            char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
            char *fields[12];

            video_trim_line(line);
            if (line[0] == '#' || line[0] == '\0' ||
                strncmp(line, "video_id\t", 9) == 0)
                continue;
            if (!video_parse_manifest_line(line, parsed, sizeof(parsed),
                                           fields) ||
                (video_manifest_entry_locked(fields) &&
                 strcmp(state->current_path, "virtual:locked") != 0) ||
                strcmp(fields[4], "concert") != 0)
                continue;
            videos_add_virtual_video(state, fields[6], fields[3],
                                     (unsigned)atoi(fields[10]), fields[0],
                                     video_manifest_year(line));
        }
    }
    else if (strcmp(state->current_path, "virtual:music_videos") == 0)
    {
        /* List all music videos */
        while (state->count < VIDEO_BROWSER_MAX_FILES && read_line(fd, line, sizeof(line)) > 0)
        {
            char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
            char *fields[12];
            video_trim_line(line);
            if (line[0] == '#' || line[0] == '\0' || strncmp(line, "video_id\t", 9) == 0)
                continue;
                
            if (!video_parse_manifest_line(line, parsed, sizeof(parsed), fields) ||
                (video_manifest_entry_locked(fields) &&
                 strcmp(state->current_path, "virtual:locked") != 0))
                continue;
                
            const char *kind = fields[4];
            if (strcmp(kind, "music_video") != 0)
                continue;
                
            videos_add_virtual_video(state, fields[6], fields[3],
                                     (unsigned)atoi(fields[10]), fields[0],
                                     video_manifest_year(line));
        }
    }
    else if (strcmp(state->current_path, "virtual:home_videos") == 0)
    {
        /* List all home videos and unmatched */
        while (state->count < VIDEO_BROWSER_MAX_FILES && read_line(fd, line, sizeof(line)) > 0)
        {
            char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
            char *fields[12];
            video_trim_line(line);
            if (line[0] == '#' || line[0] == '\0' || strncmp(line, "video_id\t", 9) == 0)
                continue;
                
            if (!video_parse_manifest_line(line, parsed, sizeof(parsed), fields) ||
                (video_manifest_entry_locked(fields) &&
                 strcmp(state->current_path, "virtual:locked") != 0))
                continue;
                
            const char *kind = fields[4];
            if (strcmp(kind, "home_video") != 0 && strcmp(kind, "unknown") != 0 && strcmp(kind, "video") != 0)
                continue;
                
            videos_add_virtual_video(state, fields[6], fields[3],
                                     (unsigned)atoi(fields[10]), fields[0],
                                     video_manifest_year(line));
        }
    }
    else if (strcmp(state->current_path, "virtual:locked") == 0)
    {
        while (state->count < VIDEO_BROWSER_MAX_FILES &&
               read_line(fd, line, sizeof(line)) > 0)
        {
            char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
            char *fields[12];
            video_trim_line(line);
            if (line[0] == '#' || line[0] == '\0' ||
                strncmp(line, "video_id\t", 9) == 0)
                continue;

            if (!video_parse_manifest_line(line, parsed, sizeof(parsed), fields) ||
                !video_manifest_entry_locked(fields))
                continue;

            videos_add_virtual_video(state, fields[6], fields[3],
                                     (unsigned)atoi(fields[10]),
                                     VIDEO_LIST_LOCK_ART_ID,
                                     video_manifest_year(line));
        }
    }

    close(fd);

    if (state->count > 1 &&
        !(state->depth == 0 && state->current_path[0] == '\0'))
        videos_sort(state);
}

static void videos_scan_dir(struct video_browser_state *state)
{
    videos_scan_virtual_dir(state);
}

static const char *videos_get_name(int selected_item,
                                   void *data,
                                   char *buffer,
                                   size_t buffer_len)
{
    struct video_browser_state *state = data;

    if (selected_item < 0 || selected_item >= state->count)
        return "";

    if (state->entries[selected_item].is_directory)
        return state->entries[selected_item].title;

    /* Show format indicator for all videos */
    if (state->entries[selected_item].playable)
    {
        if (state->entries[selected_item].format[0])
        {
            snprintf(buffer, buffer_len, "%s - %s",
                     state->entries[selected_item].title,
                     state->entries[selected_item].format);
            return buffer;
        }
        return state->entries[selected_item].title;
    }

    /* Unsupported files get more helpful messaging */
    if (state->entries[selected_item].format[0])
    {
        snprintf(buffer, buffer_len, "%s - %s (not supported)",
                 state->entries[selected_item].title,
                 state->entries[selected_item].format);
    }
    else
    {
        snprintf(buffer, buffer_len, "%s (not supported)",
                 state->entries[selected_item].title);
    }
    return buffer;
}

static enum themable_icons videos_get_icon(int selected_item, void *data)
{
    struct video_browser_state *state = data;

    if (selected_item < 0 || selected_item >= state->count)
        return Icon_NOICON;

    if (state->entries[selected_item].is_directory)
        return Icon_Folder;

    if (state->entries[selected_item].playable)
        return Icon_file_view_menu;

    return Icon_Questionmark;
}

#ifdef HAVE_LCD_COLOR
static void videos_draw_item(struct list_putlineinfo_t *list_info)
{
    if (!list_info || list_info->is_title)
    {
        gui_list_draw_item_default(list_info);
        return;
    }

    struct video_browser_state *state =
        (struct video_browser_state *)list_info->list->data;
    if (!state || list_info->line < 0 || list_info->line >= state->count)
    {
        gui_list_draw_item_default(list_info);
        return;
    }

    struct video_entry *entry = &state->entries[list_info->line];
    char thumb_path[VIDEO_LIST_PATH_LEN];
    struct bitmap *bm = NULL;
    if (video_entry_art_path(entry, false, thumb_path, sizeof(thumb_path)))
        bm = video_find_thumb_bitmap(thumb_path);

#ifdef HAVE_IPODJS_UI
    if (global_settings.ui_engine == UI_ENGINE_IPODJS)
    {
        int row_height = list_info->linedes->height;
        int font_height = font_get(ipodjs_ui_font())->height;
        int text_x = 8;
        int text_y = list_info->y +
            MAX(0, (row_height - font_height) / 2);

        if (videos_netflix_appearance())
        {
            struct screen *display = list_info->display;
            char metadata[64];
            int title_y;

            display->set_drawmode(DRMODE_SOLID);
            if (list_info->is_selected)
            {
                ipodjs_ui_gradient(display, 0, list_info->y,
                                   display->lcdwidth, row_height,
                                   VIDEO_LIST_NETFLIX_RED,
                                   VIDEO_LIST_NETFLIX_RED_DARK);
                display->set_foreground(LCD_WHITE);
                display->set_background(LCD_RGBPACK(185, 8, 17));
            }
            else
            {
                display->set_foreground(LCD_RGBPACK(20, 20, 20));
                display->fillrect(0, list_info->y,
                                  display->lcdwidth, row_height);
                if (list_info->y > IPODJS_UI_HEADER_HEIGHT)
                {
                    display->set_foreground(LCD_RGBPACK(45, 45, 45));
                    display->hline(7, display->lcdwidth - 8,
                                   list_info->y);
                }
                display->set_foreground(LCD_RGBPACK(233, 233, 233));
                display->set_background(LCD_RGBPACK(20, 20, 20));
            }

            if (bm)
            {
                int image_y = list_info->y +
                    MAX(0, (row_height - bm->height) / 2);
                display->bmp_part(bm, 0, 0, 7, image_y,
                                  bm->width, bm->height);
                text_x = 7 + bm->width + VIDEO_LIST_TEXT_PAD;
            }

            if (entry->is_directory)
            {
                ipodjs_ui_puts_fit(display, text_x, text_y,
                                   display->lcdwidth - text_x - 20,
                                   entry->title, false);
                return;
            }

            title_y = list_info->y + 5;
            ipodjs_ui_puts_fit(display, text_x, title_y,
                               display->lcdwidth - text_x - 20,
                               entry->title, false);
            if (entry->year > 0 && entry->duration_sec > 0)
                snprintf(metadata, sizeof(metadata), "%d  %u min  %s",
                         entry->year, (entry->duration_sec + 30) / 60,
                         entry->format);
            else if (entry->year > 0)
                snprintf(metadata, sizeof(metadata), "%d  %s",
                         entry->year, entry->format);
            else if (entry->duration_sec > 0)
                snprintf(metadata, sizeof(metadata), "%u min  %s",
                         (entry->duration_sec + 30) / 60, entry->format);
            else
                snprintf(metadata, sizeof(metadata), "%s", entry->format);
            display->setfont(FONT_SYSFIXED);
            ipodjs_ui_puts_fit(display, text_x,
                               list_info->y + row_height - 15,
                               display->lcdwidth - text_x - 20,
                               metadata, false);
            display->setfont(ipodjs_ui_font());
            return;
        }

        if (bm)
        {
            int image_y = list_info->y +
                MAX(0, (row_height - bm->height) / 2);
            list_info->display->bmp_part(bm, 0, 0, 8, image_y,
                                         bm->width, bm->height);
            text_x += bm->width + VIDEO_LIST_TEXT_PAD;
        }

        ipodjs_ui_puts_fit(list_info->display, text_x, text_y,
                           list_info->display->lcdwidth - text_x - 20,
                           list_info->dsp_text, false);
        return;
    }
#endif

    if (!bm)
    {
        gui_list_draw_item_default(list_info);
        return;
    }

    struct list_putlineinfo_t text_info = *list_info;
    text_info.item_indent += VIDEO_LIST_THUMB_SIZE + VIDEO_LIST_TEXT_PAD;
    text_info.icon = Icon_NOICON;
    text_info.have_icons = false;
    gui_list_draw_item_default(&text_info);

    int x = list_info->item_indent + 1;
    int y = list_info->y + MAX(0, (list_info->linedes->height - bm->height) / 2);
    list_info->display->bmp_part(bm, 0, 0, x, y, bm->width, bm->height);
}

static void videos_setup_art_list(struct gui_synclist *list)
{
    if (!list)
        return;

    gui_synclist_set_fullscreen_albumlist(list,
#ifdef HAVE_IPODJS_UI
        global_settings.ui_engine != UI_ENGINE_IPODJS);
#else
        true);
#endif

    list->callback_draw_item = videos_draw_item;
    FOR_NB_SCREENS(i)
    {
        int min_height = videos_netflix_appearance() ?
                         VIDEO_LIST_NETFLIX_POSTER_H + 2 :
                         VIDEO_LIST_THUMB_SIZE + 2;
        if (list->line_height[i] < min_height)
            list->line_height[i] = min_height;
    }
}
#endif

/* Current video for preview screen - used by context menu */
static struct video_entry *current_preview_entry;
static struct video_browser_state *current_video_browser_state;
static bool videos_browser_screen_active;
static void root_menu_video_preview_invalidate_source_cache(
    enum root_menu_video_preview_source source);
#ifndef HAVE_IPODJS_UI
static void root_menu_video_preview_invalidate_source_cache(
    enum root_menu_video_preview_source source)
{
    (void)source;
}
#endif

bool root_menu_videos_browser_active(void)
{
    return videos_browser_screen_active;
}

#if defined(HAVE_LCD_COLOR) && defined(HAVE_IPODJS_UI)
static void video_load_netflix_detail(const struct video_entry *entry,
                                      struct video_netflix_detail *detail)
{
    char line[VIDEO_LIST_MANIFEST_LINE_MAX];
    char poster_path[MAX_PATH];
    int fd;

    memset(detail, 0, sizeof(*detail));
    video_netflix_detail_valid = false;
    video_netflix_detail_art_id[0] = '\0';
    poster_path[0] = '\0';

    fd = open(VIDEO_LIST_INDEX, O_RDONLY);
    if (fd >= 0)
    {
        while (read_line(fd, line, sizeof(line)) > 0)
        {
            char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
            char *fields[23];
            const char *device_path;

            video_trim_line(line);
            if (line[0] == '#' || line[0] == '\0' ||
                strncmp(line, "video_id\t", 9) == 0 ||
                !video_parse_manifest_line_v6(line, parsed,
                                              sizeof(parsed), fields))
                continue;

            device_path = entry->path[0] == '/' ? entry->path + 1 :
                                                  entry->path;
            if (strcmp(fields[6], device_path) != 0)
                continue;

            strmemccpy(detail->genre, fields[13], sizeof(detail->genre));
            detail->rating = atoi(fields[14]);
            strmemccpy(detail->plot,
                       fields[16][0] ? fields[16] : fields[15],
                       sizeof(detail->plot));
            strmemccpy(detail->content_rating, fields[17],
                       sizeof(detail->content_rating));
            /* A row with no plot of its own still describes its series, so
             * every title ends up with something to read. */
            if (!detail->plot[0])
                strmemccpy(detail->plot, fields[22], sizeof(detail->plot));
            if (fields[19][0] &&
                strcmp(entry->art_id, VIDEO_LIST_LOCK_ART_ID))
                snprintf(poster_path, sizeof(poster_path), "%s/%s",
                         VIDEO_LIST_ROOT, fields[19]);
            break;
        }
        close(fd);
    }

    if (!poster_path[0])
        (void)video_entry_art_path(entry, true, poster_path,
                                   sizeof(poster_path));
    if (!poster_path[0] || !file_exists(poster_path))
        return;

    memset(&video_netflix_detail_bm, 0, sizeof(video_netflix_detail_bm));
    video_netflix_detail_bm.width = VIDEO_LIST_NETFLIX_DETAIL_W;
    video_netflix_detail_bm.height = VIDEO_LIST_NETFLIX_DETAIL_H;
    video_netflix_detail_bm.format = FORMAT_NATIVE;
    video_netflix_detail_bm.data = video_netflix_detail_data;
    video_netflix_detail_valid =
        read_bmp_file(poster_path, &video_netflix_detail_bm,
                      sizeof(video_netflix_detail_data),
                      FORMAT_NATIVE | FORMAT_DITHER, NULL) >= 0;
    if (video_netflix_detail_valid)
        strmemccpy(video_netflix_detail_art_id, entry->art_id,
                   sizeof(video_netflix_detail_art_id));
}

static void video_draw_netflix_wrapped(struct screen *display,
                                       const char *text, int x, int y,
                                       int width, int max_lines)
{
    const char *cursor = text ? text : "";
    int font_width = MAX(1, font_get(FONT_SYSFIXED)->maxwidth);
    int line_height = font_get(FONT_SYSFIXED)->height;
    int max_chars = MAX(1, MIN(63, width / font_width));
    int line_index;

    for (line_index = 0; line_index < max_lines && *cursor; line_index++)
    {
        char line[64];
        int length = MIN((int)strlen(cursor), max_chars);
        int cut = length;

        if (cursor[length] != '\0')
        {
            while (cut > 0 && cursor[cut] != ' ' &&
                   cursor[cut] != '\n')
                cut--;
            if (cut == 0)
                cut = length;
        }
        memcpy(line, cursor, cut);
        line[cut] = '\0';
        display->putsxy(x, y + line_index * line_height, line);
        cursor += cut;
        while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r')
            cursor++;
    }
}

static void video_draw_netflix_detail(const struct video_entry *entry,
                                      const struct video_netflix_detail *detail,
                                      int selected_action)
{
    struct screen *display = &screens[SCREEN_MAIN];
    const int poster_x = 8;
    const int poster_y = 48;
    const int text_x = poster_x + VIDEO_LIST_NETFLIX_DETAIL_W + 10;
    const int text_w = display->lcdwidth - text_x - 8;
    struct bitmap *logo = root_menu_video_netflix_logo_small_cached();
    char metadata[80];

    display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(LCD_RGBPACK(13, 13, 13));
    display->set_foreground(LCD_RGBPACK(13, 13, 13));
    display->clear_display();

    display->set_foreground(VIDEO_LIST_NETFLIX_RED);
    display->fillrect(0, 0, display->lcdwidth, 42);
    if (logo)
        display->bmp_part(logo, 0, 0, 7, 0, logo->width, logo->height);
    display->setfont(FONT_SYSFIXED);
    display->set_foreground(LCD_WHITE);
    display->set_background(VIDEO_LIST_NETFLIX_RED);
    ipodjs_ui_puts_fit(display, 112, 15, display->lcdwidth - 120,
                       "TITLE DETAILS", true);

    if (video_netflix_detail_valid)
        display->bmp_part(&video_netflix_detail_bm, 0, 0,
                          poster_x, poster_y,
                          video_netflix_detail_bm.width,
                          video_netflix_detail_bm.height);
    else
    {
        display->set_foreground(LCD_RGBPACK(28, 26, 25));
        display->fillrect(poster_x, poster_y,
                          VIDEO_LIST_NETFLIX_DETAIL_W,
                          VIDEO_LIST_NETFLIX_DETAIL_H);
        display->set_foreground(LCD_RGBPACK(208, 205, 199));
        display->set_background(LCD_RGBPACK(28, 26, 25));
        ipodjs_ui_puts_fit(display, poster_x + 5, poster_y + 64,
                           VIDEO_LIST_NETFLIX_DETAIL_W - 10,
                           "No Cover", true);
    }

    if (entry->is_resume)
    {
        int progress_width =
            (VIDEO_LIST_NETFLIX_DETAIL_W * entry->resume_percent) / 100;

        display->set_foreground(LCD_RGBPACK(22, 22, 22));
        display->fillrect(poster_x,
                          poster_y + VIDEO_LIST_NETFLIX_DETAIL_H - 5,
                          VIDEO_LIST_NETFLIX_DETAIL_W, 5);
        display->set_foreground(VIDEO_LIST_NETFLIX_RED);
        display->fillrect(poster_x,
                          poster_y + VIDEO_LIST_NETFLIX_DETAIL_H - 5,
                          MAX(2, progress_width), 5);
    }

    display->setfont(ipodjs_ui_font());
    display->set_foreground(LCD_WHITE);
    display->set_background(LCD_RGBPACK(13, 13, 13));
    ipodjs_ui_puts_fit(display, text_x, 48, text_w,
                       entry->title, false);

    if (entry->year > 0 && entry->duration_sec > 0)
        snprintf(metadata, sizeof(metadata), "%d  |  %u min",
                 entry->year, (entry->duration_sec + 30) / 60);
    else if (entry->year > 0)
        snprintf(metadata, sizeof(metadata), "%d", entry->year);
    else if (entry->duration_sec > 0)
        snprintf(metadata, sizeof(metadata), "%u min",
                 (entry->duration_sec + 30) / 60);
    else
        metadata[0] = '\0';

    if (detail->rating > 0)
    {
        size_t used = strlen(metadata);
        snprintf(metadata + used, sizeof(metadata) - used,
                 "%s%d/5", used ? "  |  " : "", detail->rating);
    }

    display->setfont(FONT_SYSFIXED);
    display->set_foreground(LCD_RGBPACK(181, 181, 181));
    ipodjs_ui_puts_fit(display, text_x, 73, text_w, metadata, false);
    snprintf(metadata, sizeof(metadata), "%s%s%s",
             detail->content_rating[0] ? detail->content_rating :
                                         entry->format,
             detail->genre[0] ? "  |  " : "",
             detail->genre);
    ipodjs_ui_puts_fit(display, text_x, 90, text_w, metadata, false);

    display->set_foreground(LCD_RGBPACK(225, 225, 225));
    video_draw_netflix_wrapped(display, detail->plot,
                               text_x, 108, text_w, 4);

    if (entry->is_resume)
    {
        const int button_y[2] = { 163, 191 };
        const char * const button_text[2] = {
            "RESUME", "PLAY FROM BEGINNING"
        };
        int button;

        display->setfont(ipodjs_ui_font());
        for (button = 0; button < 2; button++)
        {
            if (selected_action == button)
            {
                display->set_foreground(LCD_WHITE);
                display->fillrect(text_x, button_y[button], text_w, 24);
                display->set_foreground(LCD_BLACK);
                display->set_background(LCD_WHITE);
            }
            else
            {
                display->set_foreground(LCD_RGBPACK(49, 49, 49));
                display->fillrect(text_x, button_y[button], text_w, 24);
                display->set_foreground(LCD_RGBPACK(106, 106, 106));
                display->drawrect(text_x, button_y[button], text_w, 24);
                display->set_foreground(LCD_WHITE);
                display->set_background(LCD_RGBPACK(49, 49, 49));
            }
            ipodjs_ui_puts_fit(display, text_x + 6,
                               button_y[button] + 4, text_w - 12,
                               button_text[button], true);
        }
    }
    else if (entry->playable)
    {
        display->set_foreground(LCD_WHITE);
        display->fillrect(text_x, 165, text_w, 27);
        display->setfont(ipodjs_ui_font());
        display->set_foreground(LCD_BLACK);
        display->set_background(LCD_WHITE);
        ipodjs_ui_puts_fit(display, text_x + 6, 170, text_w - 12,
                           "PLAY", true);
    }
    else
    {
        display->set_foreground(LCD_RGBPACK(49, 49, 49));
        display->fillrect(text_x, 165, text_w, 27);
        display->set_foreground(LCD_RGBPACK(106, 106, 106));
        display->drawrect(text_x, 165, text_w, 27);
        display->setfont(ipodjs_ui_font());
        display->set_foreground(LCD_RGBPACK(150, 150, 150));
        display->set_background(LCD_RGBPACK(49, 49, 49));
        ipodjs_ui_puts_fit(display, text_x + 6, 170, text_w - 12,
                           "UNSUPPORTED", true);
    }

    display->setfont(FONT_SYSFIXED);
    display->set_foreground(LCD_RGBPACK(140, 140, 140));
    display->set_background(LCD_RGBPACK(13, 13, 13));
    ipodjs_ui_puts_fit(display, 8, display->lcdheight - 16,
                       display->lcdwidth - 16,
                       entry->is_resume ?
                           "SCROLL  Choose     SELECT  Play" :
                           "MENU  Back       SELECT  Play", true);
    display->update();
}

static int video_netflix_detail_screen(struct video_entry *entry)
{
    struct video_netflix_detail detail;
    struct viewport vp;
    int selected_action = 0;
    bool repaint_pending = false;

    root_menu_video_prepare_netflix_browser_logo();
    video_load_netflix_detail(entry, &detail);
    viewportmanager_theme_enable(SCREEN_MAIN, false, &vp);
    video_draw_netflix_detail(entry, &detail, selected_action);
    /* One update does not reliably reach the panel, which would leave the
     * previous screen showing. Repaint once more on the first idle tick;
     * everything it paints is already cached. */
    repaint_pending = true;

    while (true)
    {
        int action = get_action(CONTEXT_STD, HZ / 4);

        if (action == ACTION_NONE)
        {
            if (repaint_pending)
            {
                repaint_pending = false;
                video_draw_netflix_detail(entry, &detail, selected_action);
            }
            continue;
        }
        repaint_pending = true;

        switch (action)
        {
            case ACTION_STD_OK:
                if (entry->playable)
                {
                    viewportmanager_theme_undo(SCREEN_MAIN, false);
                    video_launch_entry_at(entry,
                                          entry->is_resume &&
                                          selected_action == 1);
                    return 1;
                }
                break;
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (entry->is_resume)
                {
                    selected_action = selected_action == 0 ? 1 : 0;
                    video_draw_netflix_detail(entry, &detail,
                                              selected_action);
                }
                break;
            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                viewportmanager_theme_undo(SCREEN_MAIN, false);
                return 0;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    viewportmanager_theme_undo(SCREEN_MAIN, false);
                    return 0;
                }
                break;
        }
    }
}

static const char *videos_netflix_rail_title(
    const struct video_browser_state *state)
{
    if (!state || state->depth == 0 || !state->current_path[0])
        return "BROWSE";
    if (!strcmp(state->current_path, "virtual:movies"))
        return "MOVIES";
    if (!strcmp(state->current_path, "virtual:shows"))
        return "TV SHOWS";
    if (!strcmp(state->current_path, "virtual:concerts"))
        return "CONCERTS";
    if (!strcmp(state->current_path, "virtual:music_videos"))
        return "MUSIC VIDEOS";
    if (!strcmp(state->current_path, "virtual:home_videos"))
        return "HOME VIDEOS";
    if (!strcmp(state->current_path, "virtual:locked"))
        return "LOCKED";
    if (!strncmp(state->current_path, "virtual:season:", 15))
        return "EPISODES";
    if (!strncmp(state->current_path, "virtual:show:", 13))
        return "SEASONS";
    return "MY LIST";
}

static struct bitmap *video_netflix_landing_art(
    const struct video_entry *entry)
{
    char path[MAX_PATH];

    if (!video_entry_netflix_landing_path(entry, path, sizeof(path)))
        return NULL;
    return video_find_netflix_landing_bitmap(path);
}

static void video_draw_netflix_cover_placeholder(struct screen *display,
                                                  int x, int y,
                                                  int width, int height)
{
    display->set_foreground(LCD_RGBPACK(32, 32, 32));
    display->fillrect(x, y, width, height);
    display->setfont(ipodjs_ui_font());
    display->set_foreground(VIDEO_LIST_NETFLIX_RED);
    display->set_background(LCD_RGBPACK(32, 32, 32));
    ipodjs_ui_puts_fit(display, x + 4, y + height / 2 - 9,
                       width - 8, "NETFLIX", true);
}

/* Corner badge for a title that has been played to the end. Cached pixels
 * only; the asset is loaded when the browser is entered. */
static void video_draw_netflix_watched_badge(struct screen *display,
                                             const struct video_entry *entry,
                                             int x, int y, int width)
{
    struct bitmap *badge;

    if (!entry || !entry->watched)
        return;
    badge = root_menu_video_netflix_watched_cached();
    if (!badge)
        return;
    display->bmp_part(badge, 0, 0,
                      x + width - badge->width - 3, y + 3,
                      badge->width, badge->height);
}

/* Wide "continue watching" card. Both actions live on it, side by side, and
 * the wheel steps between them as two extra stops in the row carousel. */
static void video_draw_netflix_resume_card(struct screen *display,
                                           const struct video_entry *entry,
                                           int resume_action)
{
    const int card_x = 8;
    const int card_y = 50;
    const int card_w = display->lcdwidth - 16;
    const int card_h = 112;
    const int poster_x = card_x + 4;
    const int poster_y = card_y + 2;
    const int text_x = poster_x + VIDEO_LIST_NETFLIX_LANDING_W + 10;
    const int text_w = card_x + card_w - text_x - 6;
    const int bar_y = card_y + 74;
    const int pill_y = 174;
    const int pill_w = (card_w - 8) / 2;
    const int pill_h = 30;
    const char * const pill_text[2] = { "RESUME", "PLAY FROM BEGINNING" };
    struct bitmap *art;
    char metadata[64];
    int progress_width;
    int pill;

    display->set_foreground(LCD_RGBPACK(24, 24, 24));
    display->fillrect(card_x, card_y, card_w, card_h);

    art = video_netflix_landing_art(entry);
    if (video_netflix_detail_valid &&
        !strcmp(video_netflix_detail_art_id, entry->art_id))
        display->bmp_part(&video_netflix_detail_bm, 0, 0,
                          poster_x, poster_y,
                          VIDEO_LIST_NETFLIX_LANDING_W,
                          VIDEO_LIST_NETFLIX_LANDING_H);
    else if (art)
        display->bmp_part(art, 0, 0, poster_x, poster_y,
                          art->width, art->height);
    else
        video_draw_netflix_cover_placeholder(
            display, poster_x, poster_y,
            VIDEO_LIST_NETFLIX_LANDING_W,
            VIDEO_LIST_NETFLIX_LANDING_H);

    display->setfont(ipodjs_ui_font());
    display->set_foreground(LCD_WHITE);
    display->set_background(LCD_RGBPACK(24, 24, 24));
    ipodjs_ui_puts_fit(display, text_x, card_y + 8, text_w,
                       entry->title, false);

    progress_width = (text_w * entry->resume_percent) / 100;
    display->set_foreground(LCD_RGBPACK(58, 58, 58));
    display->fillrect(text_x, bar_y, text_w, 5);
    display->set_foreground(VIDEO_LIST_NETFLIX_RED);
    display->fillrect(text_x, bar_y, MAX(2, progress_width), 5);

    snprintf(metadata, sizeof(metadata), "CONTINUE FROM %u%%",
             entry->resume_percent);
    display->setfont(FONT_SYSFIXED);
    display->set_foreground(LCD_RGBPACK(178, 178, 178));
    ipodjs_ui_puts_fit(display, text_x, bar_y + 10, text_w,
                       metadata, false);

    display->setfont(ipodjs_ui_font());
    for (pill = 0; pill < 2; pill++)
    {
        int pill_x = card_x + pill * (pill_w + 8);

        if (resume_action == pill)
        {
            display->set_foreground(LCD_WHITE);
            display->fillrect(pill_x, pill_y, pill_w, pill_h);
            display->set_foreground(LCD_BLACK);
            display->set_background(LCD_WHITE);
        }
        else
        {
            display->set_foreground(LCD_RGBPACK(49, 49, 49));
            display->fillrect(pill_x, pill_y, pill_w, pill_h);
            display->set_foreground(LCD_RGBPACK(106, 106, 106));
            display->drawrect(pill_x, pill_y, pill_w, pill_h);
            display->set_foreground(LCD_WHITE);
            display->set_background(LCD_RGBPACK(49, 49, 49));
        }
        ipodjs_ui_puts_fit(display, pill_x + 5, pill_y + 6,
                           pill_w - 10, pill_text[pill], true);
    }

    display->setfont(FONT_SYSFIXED);
    display->set_foreground(LCD_RGBPACK(165, 165, 165));
    display->set_background(LCD_RGBPACK(11, 11, 11));
    ipodjs_ui_puts_fit(display, 8, 222, display->lcdwidth - 16,
                       "SCROLL  Choose     SELECT  Play", true);
}

static void video_draw_netflix_landing(
    const struct video_browser_state *state, int selected,
    int resume_action)
{
    struct screen *display = &screens[SCREEN_MAIN];
    struct bitmap *logo = root_menu_video_netflix_logo_small_cached();
    const int center_x = (display->lcdwidth -
                          VIDEO_LIST_NETFLIX_DETAIL_W) / 2;
    const int center_y = 50;
    const int side_y = 70;
    const int left_x = 8;
    const int right_x = display->lcdwidth -
                        VIDEO_LIST_NETFLIX_LANDING_W - 8;
    char metadata[64];

    display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(LCD_RGBPACK(11, 11, 11));
    display->set_foreground(LCD_RGBPACK(11, 11, 11));
    display->clear_display();

    display->set_foreground(VIDEO_LIST_NETFLIX_RED);
    display->fillrect(0, 0, display->lcdwidth, 42);
    if (logo)
        display->bmp_part(logo, 0, 0, 7, 0, logo->width, logo->height);
    display->setfont(FONT_SYSFIXED);
    display->set_foreground(LCD_WHITE);
    display->set_background(VIDEO_LIST_NETFLIX_RED);
    ipodjs_ui_puts_fit(display, 108, 15, display->lcdwidth - 116,
                       videos_netflix_rail_title(state), true);

    if (!state || state->count <= 0)
    {
        display->setfont(ipodjs_ui_font());
        display->set_foreground(LCD_RGBPACK(205, 205, 205));
        display->set_background(LCD_RGBPACK(11, 11, 11));
        ipodjs_ui_puts_fit(display, 24, 112,
                           display->lcdwidth - 48,
                           "No titles available", true);
        display->setfont(FONT_SYSFIXED);
        display->set_foreground(LCD_RGBPACK(130, 130, 130));
        ipodjs_ui_puts_fit(display, 8, display->lcdheight - 16,
                           display->lcdwidth - 16,
                           "MENU  Back", true);
        display->update();
        return;
    }

    selected = MAX(0, MIN(selected, state->count - 1));

    /* The resume title gets a wide card of its own with both actions on it.
     * It replaces the carousel because it occupies the neighbours' columns. */
    if (state->entries[selected].is_resume)
    {
        video_draw_netflix_resume_card(display, &state->entries[selected],
                                       resume_action);
        display->update();
        return;
    }

    if (state->count > 1)
    {
        int previous = (selected + state->count - 1) % state->count;
        struct bitmap *art =
            video_netflix_landing_art(&state->entries[previous]);
        if (art)
            display->bmp_part(art, 0, 0, left_x, side_y,
                              art->width, art->height);
        else
            video_draw_netflix_cover_placeholder(
                display, left_x, side_y,
                VIDEO_LIST_NETFLIX_LANDING_W,
                VIDEO_LIST_NETFLIX_LANDING_H);
        video_draw_netflix_watched_badge(
            display, &state->entries[previous], left_x,
            side_y, VIDEO_LIST_NETFLIX_LANDING_W);
    }

    if (state->count > 2)
    {
        int next = (selected + 1) % state->count;
        struct bitmap *art =
            video_netflix_landing_art(&state->entries[next]);
        if (art)
            display->bmp_part(art, 0, 0, right_x, side_y,
                              art->width, art->height);
        else
            video_draw_netflix_cover_placeholder(
                display, right_x, side_y,
                VIDEO_LIST_NETFLIX_LANDING_W,
                VIDEO_LIST_NETFLIX_LANDING_H);
        video_draw_netflix_watched_badge(
            display, &state->entries[next], right_x, side_y,
            VIDEO_LIST_NETFLIX_LANDING_W);
    }

    display->set_foreground(VIDEO_LIST_NETFLIX_RED);
    display->fillrect(center_x - 3, center_y - 3,
                      VIDEO_LIST_NETFLIX_DETAIL_W + 6,
                      VIDEO_LIST_NETFLIX_DETAIL_H + 6);
    if (video_netflix_detail_valid &&
        !strcmp(video_netflix_detail_art_id,
                state->entries[selected].art_id))
        display->bmp_part(&video_netflix_detail_bm, 0, 0,
                          center_x, center_y,
                          video_netflix_detail_bm.width,
                          video_netflix_detail_bm.height);
    else
    {
        struct bitmap *art =
            video_netflix_landing_art(&state->entries[selected]);

        video_draw_netflix_cover_placeholder(
            display, center_x, center_y,
            VIDEO_LIST_NETFLIX_DETAIL_W,
            VIDEO_LIST_NETFLIX_DETAIL_H);
        if (art)
            display->bmp_part(art, 0, 0,
                center_x + (VIDEO_LIST_NETFLIX_DETAIL_W - art->width) / 2,
                center_y + (VIDEO_LIST_NETFLIX_DETAIL_H - art->height) / 2,
                art->width, art->height);
    }

    video_draw_netflix_watched_badge(display, &state->entries[selected],
                                     center_x, center_y,
                                     VIDEO_LIST_NETFLIX_DETAIL_W);

    display->setfont(ipodjs_ui_font());
    display->set_foreground(LCD_WHITE);
    display->set_background(LCD_RGBPACK(11, 11, 11));
    ipodjs_ui_puts_fit(display, 8, 201,
                       display->lcdwidth - 16,
                       state->entries[selected].title, true);

    /* A show or season has no metadata row of its own, so its synopsis takes
     * this line. Select-hold opens the full text. */
    if (state->entries[selected].is_directory &&
        video_netflix_landing_plot[0])
        strmemccpy(metadata, video_netflix_landing_plot, sizeof(metadata));
    else if (state->entries[selected].is_directory)
        strmemccpy(metadata, "SELECT TO BROWSE", sizeof(metadata));
    else if (state->entries[selected].year > 0 &&
             state->entries[selected].duration_sec > 0)
        snprintf(metadata, sizeof(metadata), "%d  |  %u MIN  |  INFO",
                 state->entries[selected].year,
                 (state->entries[selected].duration_sec + 30) / 60);
    else if (state->entries[selected].year > 0)
        snprintf(metadata, sizeof(metadata), "%d  |  INFO",
                 state->entries[selected].year);
    else
        strmemccpy(metadata, "SELECT FOR INFO", sizeof(metadata));

    display->setfont(FONT_SYSFIXED);
    display->set_foreground(LCD_RGBPACK(165, 165, 165));
    ipodjs_ui_puts_fit(display, 8, 222,
                       display->lcdwidth - 16, metadata, true);
    display->update();
}

/* Resolve the series synopsis for a show or season row, which has no manifest
 * row of its own. Runs only from the bounded service point below.
 *
 * Deliberately not inlined. Its two manifest line buffers are over 2 KB, and
 * the caller goes on to call video_load_netflix_detail(), whose frame is
 * another 2.4 KB. Inlined, both are live at once against the 8 KB main stack
 * and the browser panics with a stack overflow on hardware - the simulator's
 * host stack is large enough to hide it. Kept separate, the frames are
 * sequential and the peak is one of them, not their sum. */
static void __attribute__((noinline))
video_netflix_load_show_plot(const struct video_entry *entry,
                             char *plot, size_t plot_size)
{
    char line[VIDEO_LIST_MANIFEST_LINE_MAX];
    char show_buf[128];
    const char *show_name;
    int fd;

    plot[0] = '\0';
    if (!strncmp(entry->path, "virtual:show:", 13))
        show_name = entry->path + 13;
    else if (!strncmp(entry->path, "virtual:season:", 15))
    {
        /* virtual:season:<show>:<season>. The season scanner splits on the
         * first colon too, so a show title containing one is already out of
         * scope here. */
        char *colon;

        strmemccpy(show_buf, entry->path + 15, sizeof(show_buf));
        colon = strchr(show_buf, ':');
        if (!colon)
            return;
        *colon = '\0';
        show_name = show_buf;
    }
    else
        return;
    if (!show_name[0])
        return;

    fd = open(VIDEO_LIST_INDEX, O_RDONLY);
    if (fd < 0)
        return;

    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char parsed[VIDEO_LIST_MANIFEST_LINE_MAX];
        char *fields[23];

        video_trim_line(line);
        if (line[0] == '#' || line[0] == '\0' ||
            strncmp(line, "video_id\t", 9) == 0 ||
            !video_parse_manifest_line_v6(line, parsed, sizeof(parsed),
                                          fields))
            continue;
        if (strcmp(fields[4], "show") || strcmp(fields[7], show_name))
            continue;

        /* Prefer the series blurb; fall back to an episode summary so a
         * show is never left without a description. */
        if (fields[22][0])
        {
            strmemccpy(plot, fields[22], plot_size);
            break;
        }
        if (!plot[0])
            strmemccpy(plot, fields[16][0] ? fields[16] : fields[15],
                       plot_size);
    }
    close(fd);
}

static void videos_prepare_netflix_selection(
    struct video_browser_state *state, int selected)
{
    struct video_netflix_detail detail;
    struct video_entry *entry;

    video_netflix_landing_plot[0] = '\0';
    if (!state || state->count <= 0)
    {
        video_netflix_detail_valid = false;
        return;
    }
    selected = MAX(0, MIN(selected, state->count - 1));
    entry = &state->entries[selected];
    videos_cache_netflix_landing_window(state, selected);

    if (entry->is_directory)
    {
        video_netflix_detail_valid = false;
        video_netflix_load_show_plot(entry, video_netflix_landing_plot,
                                     sizeof(video_netflix_landing_plot));
        return;
    }

    video_load_netflix_detail(entry, &detail);
    strmemccpy(video_netflix_landing_plot, detail.plot,
               sizeof(video_netflix_landing_plot));
}

/* Full synopsis for a show or season row. Everything it paints is already
 * cached, so entering it performs no storage access. */
static void video_netflix_show_info_screen(const struct video_entry *entry)
{
    struct screen *display = &screens[SCREEN_MAIN];
    struct bitmap *logo = root_menu_video_netflix_logo_small_cached();
    struct bitmap *art = video_netflix_landing_art(entry);
    const int text_x = 8 + VIDEO_LIST_NETFLIX_LANDING_W + 12;
    struct viewport vp;
    bool repainted;

    viewportmanager_theme_enable(SCREEN_MAIN, false, &vp);
    display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(LCD_RGBPACK(13, 13, 13));
    display->set_foreground(LCD_RGBPACK(13, 13, 13));
    display->clear_display();

    display->set_foreground(VIDEO_LIST_NETFLIX_RED);
    display->fillrect(0, 0, display->lcdwidth, 42);
    if (logo)
        display->bmp_part(logo, 0, 0, 7, 0, logo->width, logo->height);
    display->setfont(FONT_SYSFIXED);
    display->set_foreground(LCD_WHITE);
    display->set_background(VIDEO_LIST_NETFLIX_RED);
    ipodjs_ui_puts_fit(display, 112, 15, display->lcdwidth - 120,
                       "ABOUT", true);

    if (art)
        display->bmp_part(art, 0, 0, 8, 52, art->width, art->height);
    else
        video_draw_netflix_cover_placeholder(
            display, 8, 52, VIDEO_LIST_NETFLIX_LANDING_W,
            VIDEO_LIST_NETFLIX_LANDING_H);

    display->setfont(ipodjs_ui_font());
    display->set_foreground(LCD_WHITE);
    display->set_background(LCD_RGBPACK(13, 13, 13));
    ipodjs_ui_puts_fit(display, text_x, 52,
                       display->lcdwidth - text_x - 8, entry->title, false);

    display->setfont(FONT_SYSFIXED);
    display->set_foreground(LCD_RGBPACK(222, 222, 222));
    video_draw_netflix_wrapped(
        display,
        video_netflix_landing_plot[0] ? video_netflix_landing_plot :
                                        "No description available.",
        text_x, 78, display->lcdwidth - text_x - 8, 5);

    display->set_foreground(LCD_RGBPACK(140, 140, 140));
    ipodjs_ui_puts_fit(display, 8, display->lcdheight - 16,
                       display->lcdwidth - 16, "MENU  Back", true);
    display->update();

    /* As on the other Netflix screens, one update is not always enough to
     * reach the panel; repaint once from cached pixels on the first idle
     * tick so this screen cannot be left invisible. */
    repainted = false;
    while (true)
    {
        int action = get_action(CONTEXT_STD, HZ / 4);

        if (action == ACTION_NONE)
        {
            if (!repainted)
            {
                repainted = true;
                display->update();
            }
            continue;
        }
        if (action == ACTION_STD_CANCEL || action == ACTION_STD_MENU ||
            action == ACTION_STD_OK)
            break;
        if (default_event_handler(action) == SYS_USB_CONNECTED)
            break;
    }
    viewportmanager_theme_undo(SCREEN_MAIN, false);
}

static void videos_netflix_leave_level(struct video_browser_state *state)
{
    int parent_depth = state->depth - 1;

    state->depth = parent_depth;
    strmemccpy(state->current_path,
               state->parent_path_stack[parent_depth],
               sizeof(state->current_path));
    state->selection = state->parent_selection_stack[parent_depth];
    videos_scan_dir(state);
    if (state->count > 0)
        state->selection = MAX(0, MIN(state->selection,
                                     state->count - 1));
    else
        state->selection = 0;
    root_menu_video_preview_invalidate_source_cache(IPODJS_PREVIEW_VIDEOS);
}

static bool videos_netflix_enter_level(struct video_browser_state *state,
                                        int selected)
{
    int current_depth;

    if (!state || selected < 0 || selected >= state->count ||
        !state->entries[selected].is_directory)
        return false;
    if (!strcmp(state->entries[selected].path, "virtual:locked") &&
        !videos_unlock_locked_category())
        return false;
    if (state->depth >= VIDEO_BROWSER_MAX_DEPTH)
    {
        splash(HZ * 2, "Max folder depth reached");
        return false;
    }

    current_depth = state->depth;
    strmemccpy(state->parent_path_stack[current_depth],
               state->current_path,
               sizeof(state->parent_path_stack[current_depth]));
    state->parent_selection_stack[current_depth] = selected;
    strmemccpy(state->current_path, state->entries[selected].path,
               sizeof(state->current_path));
    state->depth = current_depth + 1;
    state->selection = 0;
    videos_scan_dir(state);
    root_menu_video_preview_invalidate_source_cache(IPODJS_PREVIEW_VIDEOS);
    return true;
}

static int videos_netflix_browser(struct video_browser_state *state)
{
    struct viewport vp;
    int result = GO_TO_PREVIOUS;
    bool redraw = true;
    bool selection_pending = false;
    long selection_settle_tick = 0;
    /* Which action the resume card has focused. The two pills are extra
     * stops in this same carousel, so the wheel reaches both and then
     * carries on to the next title without a second input mode. */
    int resume_action = 0;
    /* One update after an action does not reliably reach the panel on this
     * screen, which leaves a stale frame after a focus change or a level
     * change. Every action therefore schedules one extra repaint on the next
     * idle tick. It repaints cached pixels only and never re-enters the
     * artwork service point, so it costs no storage access. */
    int pending_repaints = 0;

    state->selection = state->count > 0 ?
        MAX(0, MIN(state->selection, state->count - 1)) : 0;
    root_menu_video_prepare_netflix_browser_logo();
    root_menu_video_prepare_netflix_watched_badge();
    video_netflix_load_watched();
    videos_netflix_apply_watched(state);
    videos_prepare_netflix_selection(state, state->selection);
    viewportmanager_theme_enable(SCREEN_MAIN, false, &vp);
    button_clear_queue();

    while (true)
    {
        int action;

        if (redraw)
        {
            video_draw_netflix_landing(state, state->selection,
                                       resume_action);
            redraw = false;
        }
        action = get_action(CONTEXT_TREE, HZ / 20);
        switch (action)
        {
            case ACTION_NONE:
                if (selection_pending &&
                    TIME_AFTER(current_tick, selection_settle_tick) &&
                    button_queue_count() == 0)
                {
                    videos_prepare_netflix_selection(state,
                                                     state->selection);
                    selection_pending = false;
                    pending_repaints = 1;
                    redraw = true;
                }
                else if (pending_repaints > 0 && button_queue_count() == 0)
                {
                    pending_repaints--;
                    redraw = true;
                }
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (state->count <= 0)
                    break;
                /* Step back through the resume card's actions first. This
                 * only moves focus, so it must not re-enter the artwork
                 * service point. */
                if (state->entries[state->selection].is_resume &&
                    resume_action > 0)
                {
                    resume_action--;
                    redraw = true;
                    break;
                }
                state->selection =
                    (state->selection + state->count - 1) % state->count;
                resume_action =
                    state->entries[state->selection].is_resume ? 1 : 0;
                selection_pending = true;
                selection_settle_tick = current_tick + MAX(1, HZ / 10);
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (state->count <= 0)
                    break;
                if (state->entries[state->selection].is_resume &&
                    resume_action < 1)
                {
                    resume_action++;
                    redraw = true;
                    break;
                }
                state->selection =
                    (state->selection + 1) % state->count;
                resume_action = 0;
                selection_pending = true;
                selection_settle_tick = current_tick + MAX(1, HZ / 10);
                redraw = true;
                break;

            case ACTION_STD_OK:
                if (state->count <= 0)
                    break;
                if (state->entries[state->selection].is_resume)
                {
                    /* Play the focused action straight from the card. */
                    viewportmanager_theme_undo(SCREEN_MAIN, false);
                    (void)video_launch_entry_at(
                        &state->entries[state->selection],
                        resume_action == 1);
                    viewportmanager_theme_enable(SCREEN_MAIN, false, &vp);
                    videos_scan_dir(state);
                    state->selection = state->count > 0 ?
                        MAX(0, MIN(state->selection, state->count - 1)) : 0;
                    video_netflix_load_watched();
                    videos_netflix_apply_watched(state);
                    resume_action = 0;
                    videos_prepare_netflix_selection(state,
                                                     state->selection);
                    selection_pending = false;
                    redraw = true;
                }
                else if (state->entries[state->selection].is_directory)
                {
                    if (videos_netflix_enter_level(state,
                                                   state->selection))
                    {
                        videos_netflix_apply_watched(state);
                        videos_prepare_netflix_selection(state,
                                                         state->selection);
                        selection_pending = false;
                        redraw = true;
                    }
                    else
                        redraw = true;
                }
                else
                {
                    viewportmanager_theme_undo(SCREEN_MAIN, false);
                    (void)video_netflix_detail_screen(
                        &state->entries[state->selection]);
                    viewportmanager_theme_enable(SCREEN_MAIN, false, &vp);
                    video_netflix_load_watched();
                    videos_netflix_apply_watched(state);
                    videos_prepare_netflix_selection(state,
                                                     state->selection);
                    selection_pending = false;
                    redraw = true;
                }
                break;

            case ACTION_STD_CONTEXT:
                if (state->count <= 0)
                    break;
                if (state->entries[state->selection].is_directory)
                {
                    /* Shows and seasons have no detail screen of their own,
                     * so this is where their full synopsis lives. */
                    viewportmanager_theme_undo(SCREEN_MAIN, false);
                    video_netflix_show_info_screen(
                        &state->entries[state->selection]);
                    viewportmanager_theme_enable(SCREEN_MAIN, false, &vp);
                    redraw = true;
                }
                else
                {
                    viewportmanager_theme_undo(SCREEN_MAIN, false);
                    (void)video_netflix_detail_screen(
                        &state->entries[state->selection]);
                    viewportmanager_theme_enable(SCREEN_MAIN, false, &vp);
                    video_netflix_load_watched();
                    videos_netflix_apply_watched(state);
                    videos_prepare_netflix_selection(state,
                                                     state->selection);
                    selection_pending = false;
                    redraw = true;
                }
                break;

            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                if (state->depth > 0)
                {
                    videos_netflix_leave_level(state);
                    videos_netflix_apply_watched(state);
                    resume_action = 0;
                    videos_prepare_netflix_selection(state,
                                                     state->selection);
                    selection_pending = false;
                    redraw = true;
                }
                else
                    goto done;
                break;

            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    result = GO_TO_ROOT;
                    goto done;
                }
                break;
        }

        /* Any action-driven repaint gets one follow-up pass; the idle branch
         * above consumes it. ACTION_NONE is excluded so this cannot re-arm
         * itself and repaint forever. */
        if (redraw && action != ACTION_NONE)
            pending_repaints = 1;
    }

done:
    viewportmanager_theme_undo(SCREEN_MAIN, false);
    return result;
}
#endif

/* Sort order change handler - returns ACTION_REDRAW to update display */
static int video_set_sort_order(enum video_sort_order order)
{
    if (!current_video_browser_state)
        return 0;
    
    current_video_browser_state->sort_order = order;
    videos_sort(current_video_browser_state);
    
    return ACTION_REDRAW;  /* Tell simplelist to refresh */
}

/* Sort menu callbacks */
static int video_sort_name_az(void)
{
    return video_set_sort_order(VIDEO_SORT_NAME_AZ);
}

static int video_sort_name_za(void)
{
    return video_set_sort_order(VIDEO_SORT_NAME_ZA);
}

static int video_sort_date_new(void)
{
    return video_set_sort_order(VIDEO_SORT_DATE_NEW);
}

static int video_sort_date_old(void)
{
    return video_set_sort_order(VIDEO_SORT_DATE_OLD);
}

static int video_sort_size_large(void)
{
    return video_set_sort_order(VIDEO_SORT_SIZE_LARGE);
}

static int video_sort_size_small(void)
{
    return video_set_sort_order(VIDEO_SORT_SIZE_SMALL);
}

/* Video preview screen - shows metadata and optional thumbnail */
static int video_preview_screen(void)
{
    struct video_entry *entry = current_preview_entry;
    struct viewport vp;
    int y = 0;
    char buf[128];
    
    if (!entry)
        return 0;

#if defined(HAVE_LCD_COLOR) && defined(HAVE_IPODJS_UI)
    if (videos_netflix_appearance())
        return video_netflix_detail_screen(entry);
#endif
    
    viewportmanager_theme_enable(SCREEN_MAIN, false, &vp);
    screens[SCREEN_MAIN].set_viewport(&vp);
    screens[SCREEN_MAIN].clear_display();
    
    /* Title */
    screens[SCREEN_MAIN].puts_scroll(0, y++, entry->title);
    y++;  /* blank line */
    
    /* Format and status */
    if (entry->playable)
    {
        snprintf(buf, sizeof(buf), "Format: %s (Playable)", entry->format);
    }
    else
    {
        snprintf(buf, sizeof(buf), "Format: %s (Not supported)", entry->format);
    }
    screens[SCREEN_MAIN].puts(0, y++, buf);
    
    /* File size */
    if (entry->filesize > 0)
    {
        if (entry->filesize >= 1024*1024)
            snprintf(buf, sizeof(buf), "Size: %ld MB", 
                     (long)(entry->filesize / (1024*1024)));
        else if (entry->filesize >= 1024)
            snprintf(buf, sizeof(buf), "Size: %ld KB", 
                     (long)(entry->filesize / 1024));
        else
            snprintf(buf, sizeof(buf), "Size: %ld bytes", (long)entry->filesize);
        screens[SCREEN_MAIN].puts(0, y++, buf);
    }
    
    /* Duration and resolution (if available) */
    if (entry->width > 0 && entry->height > 0)
    {
        snprintf(buf, sizeof(buf), "Resolution: %dx%d", entry->width, entry->height);
        screens[SCREEN_MAIN].puts(0, y++, buf);
    }
    
    if (entry->duration_sec > 0)
    {
        unsigned hours = entry->duration_sec / 3600;
        unsigned mins = (entry->duration_sec % 3600) / 60;
        unsigned secs = entry->duration_sec % 60;
        
        if (hours > 0)
            snprintf(buf, sizeof(buf), "Duration: %u:%02u:%02u", hours, mins, secs);
        else
            snprintf(buf, sizeof(buf), "Duration: %u:%02u", mins, secs);
        screens[SCREEN_MAIN].puts(0, y++, buf);
    }
    
    y++;  /* blank line */
    
    /* Path (truncated for display) */
    snprintf(buf, sizeof(buf), "Path: %s", entry->path);
    screens[SCREEN_MAIN].puts_scroll(0, y++, buf);
    
    y++;  /* blank line */
    
    /* Instructions */
    if (entry->playable)
    {
        screens[SCREEN_MAIN].puts(0, y++, "CENTER: Play");
        screens[SCREEN_MAIN].puts(0, y++, "MENU: Return");
    }
    else
    {
        screens[SCREEN_MAIN].puts(0, y++, "MENU: Return");
    }
    
    screens[SCREEN_MAIN].update();
    
    /* Wait for user input */
    while (true)
    {
        int button = get_action(CONTEXT_STD, HZ/4);
        
        switch (button)
        {
            case ACTION_STD_OK:
                /* Play video if supported */
                if (entry->playable)
                {
                    viewportmanager_theme_undo(SCREEN_MAIN, false);
                    video_launch_entry(entry);
                    return 1;  /* Return to browser */
                }
                break;
                
            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                /* Return to browser */
                viewportmanager_theme_undo(SCREEN_MAIN, false);
                return 0;
        }
    }
}

/* Menu items for video context menu */
MENUITEM_FUNCTION(video_preview_item, 0, "Show Preview",
                  video_preview_screen, NULL, Icon_NOICON);

/* Sort menu items */
MENUITEM_FUNCTION(video_sort_name_az_item, 0, "Name (A-Z)",
                  video_sort_name_az, NULL, Icon_NOICON);
MENUITEM_FUNCTION(video_sort_name_za_item, 0, "Name (Z-A)",
                  video_sort_name_za, NULL, Icon_NOICON);
MENUITEM_FUNCTION(video_sort_date_new_item, 0, "Date (Newest First)",
                  video_sort_date_new, NULL, Icon_NOICON);
MENUITEM_FUNCTION(video_sort_date_old_item, 0, "Date (Oldest First)",
                  video_sort_date_old, NULL, Icon_NOICON);
MENUITEM_FUNCTION(video_sort_size_large_item, 0, "Size (Largest First)",
                  video_sort_size_large, NULL, Icon_NOICON);
MENUITEM_FUNCTION(video_sort_size_small_item, 0, "Size (Smallest First)",
                  video_sort_size_small, NULL, Icon_NOICON);

MAKE_MENU(video_sort_menu, "Sort By", NULL, Icon_NOICON,
          &video_sort_name_az_item,
          &video_sort_name_za_item,
          &video_sort_date_new_item,
          &video_sort_date_old_item,
          &video_sort_size_large_item,
          &video_sort_size_small_item);

MAKE_MENU(video_context_menu_items, "Video Options", NULL, Icon_NOICON,
          &video_preview_item,
          &video_sort_menu);

/* Context menu for video entry */
static int video_context_menu(struct video_entry *entry)
{
    int result;
    
    current_preview_entry = entry;
    
    result = do_menu(&video_context_menu_items, NULL, NULL, false);
    
    current_preview_entry = NULL;
    
    return result;
}

static int videos_action_cb(int action, struct gui_synclist *lists)
{
    struct video_browser_state *state = lists->data;
    int selected = gui_synclist_get_sel_pos(lists);

#ifdef HAVE_LCD_COLOR
    videos_setup_art_list(lists);
    videos_cache_art_window(state, selected);
#endif

    /* Set global state pointer for sort menu access */
    current_video_browser_state = state;

    if (action == ACTION_STD_CANCEL)
    {
        state->selection = selected;
        if (state->depth > 0)
        {
            int parent_depth = state->depth - 1;

            state->depth = parent_depth;
            if (parent_depth >= 0)
                strmemccpy(state->current_path,
                           state->parent_path_stack[parent_depth],
                           sizeof(state->current_path));
            else
                state->current_path[0] = '\0';

            state->selection = state->parent_selection_stack[parent_depth];
            videos_scan_dir(state);
#ifdef HAVE_LCD_COLOR
            videos_cache_art_window(state, state->selection);
#endif
            root_menu_video_preview_invalidate_source_cache(
                IPODJS_PREVIEW_VIDEOS);

            gui_synclist_set_nb_items(lists, state->count);
            if (state->count > 0)
                gui_synclist_select_item(lists,
                                         MIN(state->selection, state->count - 1));
            else
                gui_synclist_select_item(lists, 0);
            current_video_browser_state = NULL;
            return ACTION_REDRAW;
        }
        current_video_browser_state = NULL;
        return action;
    }

    /* Handle context menu (long press on SELECT) */
    if (action == ACTION_STD_CONTEXT)
    {
        if (selected >= 0 && selected < state->count
            && !state->entries[selected].is_directory)
        {
            state->selection = selected;
            video_context_menu(&state->entries[selected]);
        }
        current_video_browser_state = NULL;
        return ACTION_REDRAW;
    }

    if (action != ACTION_STD_OK)
    {
        current_video_browser_state = NULL;
        return action;
    }

    if (selected < 0 || selected >= state->count)
    {
        current_video_browser_state = NULL;
        return ACTION_REDRAW;
    }

    state->selection = selected;
    if (state->entries[selected].is_directory)
    {
        if (strcmp(state->entries[selected].path, "virtual:locked") == 0 &&
            !videos_unlock_locked_category())
        {
            current_video_browser_state = NULL;
            return ACTION_REDRAW;
        }
        if (state->depth < VIDEO_BROWSER_MAX_DEPTH)
        {
            int current_depth = state->depth;

            strmemccpy(state->parent_path_stack[current_depth],
                       state->current_path,
                       sizeof(state->parent_path_stack[current_depth]));
            state->parent_selection_stack[current_depth] = state->selection;
            strmemccpy(state->current_path,
                       state->entries[selected].path,
                       sizeof(state->current_path));
            state->depth = current_depth + 1;
            state->selection = 0;

            videos_scan_dir(state);
#ifdef HAVE_LCD_COLOR
            videos_cache_art_window(state, state->selection);
#endif
            root_menu_video_preview_invalidate_source_cache(
                IPODJS_PREVIEW_VIDEOS);
            if (state->count == 0)
                state->selection = -1;
            else if (state->selection >= state->count)
                state->selection = 0;

            gui_synclist_set_nb_items(lists, state->count);
            if (state->selection < 0)
                gui_synclist_select_item(lists, 0);
            else
                gui_synclist_select_item(lists, state->selection);
            current_video_browser_state = NULL;
            return ACTION_REDRAW;
        }

        splash(HZ * 2, "Max folder depth reached");
        current_video_browser_state = NULL;
        return ACTION_REDRAW;
    }

#if defined(HAVE_LCD_COLOR) && defined(HAVE_IPODJS_UI)
    if (videos_netflix_appearance())
    {
        current_preview_entry = &state->entries[selected];
        video_preview_screen();
        current_preview_entry = NULL;
        current_video_browser_state = NULL;
        return ACTION_REDRAW;
    }
#endif

    if (!state->entries[selected].playable)
    {
        /* Show helpful message based on format */
        if (!strcasecmp(state->entries[selected].format, "mp4") ||
            !strcasecmp(state->entries[selected].format, "m4v"))
        {
            splash(HZ * 2, "MP4 not supported - Use MPEG-2 (.mpg)");
        }
        else if (!strcasecmp(state->entries[selected].format, "mov"))
        {
            splash(HZ * 2, "MOV not supported - Use MPEG-2 (.mpg)");
        }
        else
        {
            splash(HZ * 2, "Format not supported - Use MPEG-2 (.mpg)");
        }
        current_video_browser_state = NULL;
        return ACTION_REDRAW;
    }

    switch (video_launch_entry(&state->entries[selected]))
    {
        case PLUGIN_GOTO_WPS:
            state->next_screen = GO_TO_WPS;
            current_video_browser_state = NULL;
            return ACTION_STD_CANCEL;

        case PLUGIN_USB_CONNECTED:
        case PLUGIN_ERROR:
            state->next_screen = GO_TO_ROOT;
            current_video_browser_state = NULL;
            return ACTION_STD_CANCEL;

        default:
            current_video_browser_state = NULL;
            return ACTION_REDRAW;
    }
}

static int videos_scrn(void* param)
{
    static struct video_browser_state state;
    struct simplelist_info list;
    char *list_title = "Videos";
    bool usb;
    int ret = GO_TO_PREVIOUS;

    (void)param;

    state.count = 0;
    state.truncated = false;
    state.next_screen = GO_TO_PREVIOUS;
    state.sort_order = VIDEO_SORT_NAME_AZ;  /* Default sort by name A-Z */
    state.depth = 0;
    state.current_path[0] = '\0';

#if defined(HAVE_LCD_COLOR) && defined(HAVE_IPODJS_UI)
    if (videos_netflix_appearance() && ipodjs_ui_netflix_launch())
        return GO_TO_ROOT;
#endif
    videos_scan_dir(&state);
#ifdef HAVE_LCD_COLOR
    videos_cache_art_window(&state, 0);
#endif

    if (state.count == 0)
    {
        splash(HZ * 2, "No videos found");
        return GO_TO_PREVIOUS;
    }

    if (state.truncated)
        splashf(HZ, "Showing first %d videos", VIDEO_BROWSER_MAX_FILES);

#if defined(HAVE_LCD_COLOR) && defined(HAVE_IPODJS_UI)
    if (videos_netflix_appearance())
    {
        videos_browser_screen_active = true;
        push_current_activity(ACTIVITY_FILEBROWSER);
        ret = videos_netflix_browser(&state);
        pop_current_activity();
        videos_browser_screen_active = false;
        return ret;
    }
#endif
    simplelist_info_init(&list, list_title, state.count, &state);
    list.get_name = videos_get_name;
    list.get_icon = global_settings.show_icons ? videos_get_icon : NULL;
    list.action_callback = videos_action_cb;
    list.title_icon = Icon_file_view_menu;
#ifdef HAVE_IPODJS_UI
    list.hide_theme = global_settings.ui_engine == UI_ENGINE_IPODJS;
#else
    list.hide_theme = false;
#endif
    list.selection = MIN(state.selection, state.count - 1);

    videos_browser_screen_active = true;
    push_current_activity(ACTIVITY_FILEBROWSER);
    usb = simplelist_show_list(&list);
    pop_current_activity();
    videos_browser_screen_active = false;

    if (usb)
        return GO_TO_ROOT;

    if (list.selection >= 0)
        state.selection = list.selection;

    if (state.next_screen != GO_TO_PREVIOUS)
        ret = state.next_screen;

    return ret;
}

#ifdef HAVE_RECORDING
static int recscrn(void* param)
{
    (void)param;
    recording_screen(false);
    return GO_TO_ROOT;
}
#endif
static int wpsscrn(void* param)
{
    int ret_val = GO_TO_PREVIOUS;
    int audstatus = audio_status();
#ifdef HAVE_IPODJS_UI
    bool ipodjs_direct_return;
#endif
    (void)param;
    push_current_activity(ACTIVITY_WPS);

#if defined(HAVE_TAGCACHE) && defined(HAVE_IPODJS_UI)
    if (root_menu_video_enabled() &&
        ipodjs_ui_origin_depth == 0)
    {
        if (last_screen == GO_TO_DBBROWSER)
            ipodjs_ui_origin_push(GO_TO_DBBROWSER,
                                  IPODJS_UI_ORIGIN_DATABASE);
        else if (last_screen == GO_TO_FILEBROWSER)
            ipodjs_ui_origin_push(GO_TO_FILEBROWSER,
                                  IPODJS_UI_ORIGIN_FILES);
    }
#endif

#ifdef HAVE_PITCHCONTROL
    if (!audstatus)
    {
        sound_set_pitch(global_status.resume_pitch);
        dsp_set_timestretch(global_status.resume_speed);
    }
#endif

    if (audstatus)
    {
        talk_shutup();
#ifdef HAVE_IPODJS_UI
        if (root_menu_video_enabled() &&
            !root_menu_video_uses_stock_music())
            ret_val = ipodjs_video_wps();
        else
#endif
        ret_val = gui_wps_show();
    }
    else if (global_status.resume_index != -1)
    {
        DEBUGF("Resume index %d crc32 %lX offset %lX\n",
               global_status.resume_index,
               (unsigned long)global_status.resume_crc32,
               (unsigned long)global_status.resume_offset);
        if (playlist_resume() != -1)
        {
            playlist_resume_track(global_status.resume_index,
                global_status.resume_crc32,
                global_status.resume_elapsed,
                global_status.resume_offset);
#ifdef HAVE_IPODJS_UI
            if (root_menu_video_enabled() &&
                !root_menu_video_uses_stock_music())
                ret_val = ipodjs_video_wps();
            else
#endif
            ret_val = gui_wps_show();
        }
    }
    else if (!file_exists(PLAYLIST_CONTROL_FILE))
    {
#ifdef HAVE_IPODJS_UI
        if (root_menu_video_enabled() &&
            !root_menu_video_uses_stock_music())
        {
            ipodjs_video_wps_empty("No Music", "Nothing to resume", false);
            ret_val = GO_TO_ROOT;
        }
        else
#endif
        splash(HZ*2, ID2P(LANG_NOTHING_TO_RESUME));
    }
    else if (
#ifdef HAVE_IPODJS_UI
             (root_menu_video_enabled() &&
              !root_menu_video_uses_stock_music() ?
              ipodjs_video_wps_empty("Playlist Finished",
                                     "Select to replay", true) :
              yesno_pop(ID2P(LANG_REPLAY_FINISHED_PLAYLIST))) &&
#else
             yesno_pop(ID2P(LANG_REPLAY_FINISHED_PLAYLIST)) &&
#endif
             playlist_resume() != -1)
    {
        playlist_start(0, 0, 0);
#ifdef HAVE_IPODJS_UI
        if (root_menu_video_enabled() &&
            !root_menu_video_uses_stock_music())
            ret_val = ipodjs_video_wps();
        else
#endif
        ret_val = gui_wps_show();
    }

#if defined(HAVE_TAGCACHE) && defined(HAVE_IPODJS_UI)
    /* Pop one display-only origin on a normal short-Menu WPS exit. Browser
     * owners keep their live row/window state; PictureFlow needs an explicit
     * return because plugin dispatch can rewrite generic root history. */
    if (ret_val == GO_TO_ROOT || ret_val == GO_TO_PREVIOUS ||
        ret_val == GO_TO_PREVIOUS_BROWSER || ret_val == GO_TO_DBBROWSER ||
        ret_val == GO_TO_FILEBROWSER)
    {
        struct ipodjs_ui_origin_frame frame;

        if (ipodjs_ui_origin_pop(&frame))
            ret_val = frame.coverflow ? GO_TO_PICTUREFLOW : frame.screen;
    }
    /* WPS launched from the non-stock root menu should still return through
     * root history instead of falling into the default browser. */
    if (!root_menu_video_uses_stock_music() &&
        (ret_val == GO_TO_ROOT || ret_val == GO_TO_PREVIOUS_BROWSER) &&
        last_screen == GO_TO_ROOT &&
        audio_status())
    {
        ret_val = GO_TO_PREVIOUS;
    }
#endif

#ifdef HAVE_IPODJS_UI
    ipodjs_direct_return = root_menu_video_enabled() &&
        root_menu_video_uses_stock_music() &&
        (ret_val == GO_TO_ROOT || ret_val == GO_TO_PREVIOUS ||
         ret_val == GO_TO_PREVIOUS_BROWSER || ret_val == GO_TO_DBBROWSER ||
         ret_val == GO_TO_FILEBROWSER);
#endif

    if (
#ifdef HAVE_IPODJS_UI
        ipodjs_direct_return ||
#endif
        ret_val == GO_TO_PLAYLIST_VIEWER
        || ret_val == GO_TO_PLUGIN
        || ret_val == GO_TO_WPS
        || ret_val == GO_TO_PREVIOUS_MUSIC
        || ret_val == GO_TO_PREVIOUS_BROWSER
        || (ret_val == GO_TO_PREVIOUS
               && (last_screen == GO_TO_MAINMENU /* Settings */
                || last_screen == GO_TO_BROWSEPLUGINS
                || last_screen == GO_TO_SYSTEM_SCREEN
                || last_screen == GO_TO_PLAYLISTS_SCREEN)))
    {
        pop_current_activity_without_refresh();
    }
    else
        pop_current_activity();

    return ret_val;
}
#if CONFIG_TUNER
static int radio(void* param)
{
    (void)param;
    radio_screen();
    return GO_TO_ROOT;
}
#endif

static int miscscrn(void * param)
{
    const struct menu_item_ex *menu = (const struct menu_item_ex*)param;
    int result = do_menu(menu, NULL, NULL, false);
    switch (result)
    {
        case GO_TO_PLUGIN:
        case GO_TO_PLAYLIST_VIEWER:
        case GO_TO_WPS:
        case GO_TO_PREVIOUS_MUSIC:
            return result;
        default:
            return GO_TO_ROOT;
    }
}


static int playlist_view_catalog(void * param)
{
    (void)param;
    push_current_activity(ACTIVITY_PLAYLISTBROWSER);
    bool item_was_selected = catalog_view_playlists();

    if (item_was_selected)
    {
        pop_current_activity_without_refresh();
        return GO_TO_WPS;
    }
    pop_current_activity();
    return GO_TO_ROOT;
}

static int playlist_view(void * param)
{
    (void)param;
    int val;

    val = playlist_viewer();
    switch (val)
    {
        case PLAYLIST_VIEWER_MAINMENU:
        case PLAYLIST_VIEWER_USB:
            return GO_TO_ROOT;
        case PLAYLIST_VIEWER_OK:
            return GO_TO_PREVIOUS;
    }
    return GO_TO_PREVIOUS;
}

static int load_bmarks(void* param)
{
    (void)param;
    if(bookmark_mrb_load())
        return GO_TO_WPS;
    return GO_TO_PREVIOUS;
}

#ifdef HAVE_TAGCACHE
static int pictureflow_scrn(void* param)
{
    (void)param;
#ifdef HAVE_IPODJS_UI
    ipodjs_ui_origin_clear();
#endif
    int ret = filetype_load_plugin("pictureflow", NULL);
    switch (ret)
    {
        case PLUGIN_GOTO_WPS:
#ifdef HAVE_IPODJS_UI
            if (root_menu_video_enabled())
                ipodjs_ui_origin_push(GO_TO_PICTUREFLOW,
                                      IPODJS_UI_ORIGIN_PICTUREFLOW);
#endif
            return GO_TO_WPS;
        case PLUGIN_USB_CONNECTED:
        case PLUGIN_ERROR:
            return GO_TO_ROOT;
        default:
            return GO_TO_PREVIOUS;
    }
}
#endif

/* These are all static const'd from apps/menus/ *.c
   so little hack so we can use them */
extern struct menu_item_ex
        file_menu,
#ifdef HAVE_TAGCACHE
        tagcache_menu,
#endif
        main_menu_,
        manage_settings,
        plugin_menu,
        playlist_options,
        info_menu,
        system_menu;
static const struct root_items items[] = {
    [GO_TO_FILEBROWSER] =   { browser, (void*)GO_TO_FILEBROWSER, &file_menu},
#ifdef HAVE_TAGCACHE
    [GO_TO_DBBROWSER] =     { browser, (void*)GO_TO_DBBROWSER, &tagcache_menu },
#endif
    [GO_TO_VIDEOS] =        { videos_scrn, NULL, NULL },
    [GO_TO_WPS] =           { wpsscrn, NULL, &playback_settings },
    [GO_TO_MAINMENU] =      { miscscrn, (struct menu_item_ex*)&main_menu_,
                                                            &manage_settings },

#ifdef HAVE_RECORDING
    [GO_TO_RECSCREEN] =     {  recscrn, NULL, &recording_settings_menu },
#endif

#if CONFIG_TUNER
    [GO_TO_FM] =            { radio, NULL, &radio_settings_menu },
#endif

    [GO_TO_RECENTBMARKS] =  { load_bmarks, NULL, &bookmark_settings_menu },
    [GO_TO_BROWSEPLUGINS] = { miscscrn, &plugin_menu, NULL },
    [GO_TO_PLAYLISTS_SCREEN] = { playlist_view_catalog, NULL,
                                                        &playlist_options },
    [GO_TO_PLAYLIST_VIEWER] = { playlist_view, NULL, &playlist_options },
    [GO_TO_SYSTEM_SCREEN] = { miscscrn, &info_menu, &system_menu },
    [GO_TO_SHORTCUTMENU] = { do_shortcut_menu, NULL, NULL },
#ifdef HAVE_TAGCACHE
    [GO_TO_PICTUREFLOW] = { pictureflow_scrn, NULL, NULL },
#endif

};
//static const int nb_items = sizeof(items)/sizeof(*items);

static int item_callback(int action,
                         const struct menu_item_ex *this_item,
                         struct gui_synclist *this_list);
static void root_menu_wait_for_button_release(void);
static bool root_menu_request_reboot(void);
static void root_menu_open_power_menu(void);

MENUITEM_RETURNVALUE(shortcut_menu, ID2P(LANG_SHORTCUTS), GO_TO_SHORTCUTMENU,
                        NULL, Icon_Bookmark);

MENUITEM_RETURNVALUE(file_browser, ID2P(LANG_DIR_BROWSER), GO_TO_FILEBROWSER,
                        NULL, Icon_file_view_menu);
MENUITEM_RETURNVALUE(videos, "Videos", GO_TO_VIDEOS,
                        NULL, Icon_file_view_menu);
#ifdef HAVE_TAGCACHE
MENUITEM_RETURNVALUE(db_browser, "Music", GO_TO_DBBROWSER,
                        NULL, Icon_Audio);
MENUITEM_RETURNVALUE(pictureflow_item, "Cover Flow", GO_TO_PICTUREFLOW,
                        NULL, Icon_Rockbox);
#endif
MENUITEM_RETURNVALUE(rocks_browser, ID2P(LANG_PLUGINS), GO_TO_BROWSEPLUGINS,
                        NULL, Icon_Plugin);

static char *get_wps_item_name(int selected_item, void * data,
                               char *buffer, size_t buffer_len)
{
    (void)selected_item; (void)data; (void)buffer; (void)buffer_len;
    if (audio_status())
        return ID2P(LANG_NOW_PLAYING);
    return ID2P(LANG_RESUME_PLAYBACK);
}
MENUITEM_RETURNVALUE_DYNTEXT(wps_item, GO_TO_WPS, NULL, get_wps_item_name,
                                NULL, NULL, Icon_Playback_menu);
#ifdef HAVE_RECORDING
MENUITEM_RETURNVALUE(rec, ID2P(LANG_RECORDING), GO_TO_RECSCREEN,
                        NULL, Icon_Recording);
#endif
#if CONFIG_TUNER
MENUITEM_RETURNVALUE(fm, ID2P(LANG_FM_RADIO), GO_TO_FM,
                        item_callback, Icon_Radio_screen);
#endif
MENUITEM_RETURNVALUE(menu_, ID2P(LANG_SETTINGS), GO_TO_MAINMENU,
                        NULL, Icon_Submenu_Entered);
MENUITEM_RETURNVALUE(bookmarks, ID2P(LANG_BOOKMARK_MENU_RECENT_BOOKMARKS),
                        GO_TO_RECENTBMARKS,  item_callback,
                        Icon_Bookmark);
MENUITEM_RETURNVALUE(playlists, ID2P(LANG_PLAYLISTS), GO_TO_PLAYLISTS_SCREEN,
                     NULL, Icon_Playlist);
static int load_plugin_path_screen(const char *path, const char *param);

static int launch_photos_plugin(void *param)
{
    int ret;

    (void)param;
    ret = load_plugin_path_screen(PLUGIN_APPS_DIR "/photos.rock", NULL);
    root_menu_video_preview_invalidate_source_cache(IPODJS_PREVIEW_PHOTOS);
    return ret;
}

MENUITEM_FUNCTION(photos_item, MENU_FUNC_CHECK_RETVAL,
                  "Photos", launch_photos_plugin,
                  NULL, Icon_Folder);

static int launch_desktop_mode(void *param)
{
    (void)param;
    return load_plugin_path_screen(PLUGIN_APPS_DIR "/desktop_mode.rock", NULL);
}

MENUITEM_FUNCTION(desktop_mode_item, MENU_FUNC_CHECK_RETVAL,
                  "Desktop Mode", launch_desktop_mode,
                  NULL, Icon_Rockbox);

static int launch_offlineweb_plugin(void *param)
{
    (void)param;
    return load_plugin_path_screen(PLUGIN_APPS_DIR "/offlineweb.rock",
                                   "online");
}

static int internet_item_callback(int action,
                                  const struct menu_item_ex *this_item,
                                  struct gui_synclist *this_list)
{
    (void)this_item;
    (void)this_list;
    if (action == ACTION_REQUEST_MENUITEM && !usb_internet_connected())
        return ACTION_EXIT_MENUITEM;
    return action;
}

MENUITEM_FUNCTION(offlineweb_item, MENU_FUNC_CHECK_RETVAL,
                  "Internet", launch_offlineweb_plugin,
                  internet_item_callback, Icon_Rockbox);

static int launch_achievements_plugin(void *param)
{
    (void)param;
    return load_plugin_path_screen(
        PLUGIN_APPS_DIR "/achievements.rock", NULL);
}

MENUITEM_FUNCTION(achievements_item, MENU_FUNC_CHECK_RETVAL,
                  "Achievements", launch_achievements_plugin,
                  NULL, Icon_Plugin);

static int launch_sitekick_plugin(void *param)
{
    return load_plugin_path_screen(
        PLUGIN_APPS_DIR "/sitekick.rock", param);
}

MENUITEM_FUNCTION(sitekick_item, MENU_FUNC_CHECK_RETVAL,
                  "Sitekick", launch_sitekick_plugin,
                  NULL, Icon_Plugin);

static int launch_livetv_plugin(void *param)
{
    (void)param;
    return load_plugin_path_screen(
        PLUGIN_APPS_DIR "/livetv.rock", NULL);
}

MENUITEM_FUNCTION(livetv_item, MENU_FUNC_CHECK_RETVAL,
                  "DIRECTV", launch_livetv_plugin,
                  NULL, Icon_Plugin);

static int launch_weather_plugin(void *param)
{
    (void)param;
    char path[MAX_PATH];
    static const char *paths[] = {
        PLUGIN_APPS_DIR "/weather.rock",
        PLUGIN_DIR "/weather.rock",
        PLUGIN_APPS_DIR "/weather/weather.rock",
        PLUGIN_DIR "/apps/weather.rock",
        PLUGIN_DIR "/apps/weather/weather.rock",
        ROCKBOX_DIR "/rocks/apps/weather.rock",
        ROCKBOX_DIR "/rocks/weather.rock",
        ROCKBOX_DIR "/rocks/apps/weather/weather.rock",
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
    {
        strcpy(path, paths[i]);
        if (file_exists(path))
            return load_plugin_path_screen(path, NULL);
    }

    splash(HZ, "Weather plugin missing\nInstall to .rockbox/rocks/apps/weather.rock");
    return GO_TO_ROOT;
}

static int launch_pokedex_plugin(void *param)
{
    (void)param;
    char path[MAX_PATH];
    static const char *paths[] = {
        PLUGIN_APPS_DIR "/pokedex.rock",
        PLUGIN_DIR "/pokedex.rock",
        ROCKBOX_DIR "/rocks/apps/pokedex.rock",
        ROCKBOX_DIR "/rocks/pokedex.rock",
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
    {
        strcpy(path, paths[i]);
        if (file_exists(path))
            return load_plugin_path_screen(path, NULL);
    }

    splash(HZ, "Pokedex plugin missing\nInstall to .rockbox/rocks/apps/pokedex.rock");
    return GO_TO_ROOT;
}

#if defined(HAVE_IPODJS_UI) && defined(HAVE_TAGCACHE)
static int launch_qrcode_tool(void *param)
{
    (void)param;
    return root_menu_video_qrcode_screen();
}

MENUITEM_FUNCTION(qrcode_item, MENU_FUNC_CHECK_RETVAL,
                  "QR Codes", launch_qrcode_tool,
                  NULL, Icon_Plugin);
#endif

static int launch_calm_plugin(void *param)
{
    (void)param;
    return load_plugin_path_screen(PLUGIN_APPS_DIR "/calm.rock", NULL);
}

MENUITEM_FUNCTION(calm_item, MENU_FUNC_CHECK_RETVAL,
                  "Calm", launch_calm_plugin,
                  NULL, Icon_Plugin);

static int launch_tamagotchi_plugin(void *param)
{
    (void)param;
    char path[MAX_PATH];
    static const char *paths[] = {
        PLUGIN_APPS_DIR "/tamagotchi.rock",
        PLUGIN_DIR "/apps/tamagotchi.rock",
        ROCKBOX_DIR "/rocks/apps/tamagotchi.rock",
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
    {
        strcpy(path, paths[i]);
        if (file_exists(path))
            return load_plugin_path_screen(path, NULL);
    }

    splash(HZ, "Tamagotchi plugin missing\nInstall to .rockbox/rocks/apps/tamagotchi.rock");
    return GO_TO_ROOT;
}

MENUITEM_FUNCTION(tamagotchi_item, MENU_FUNC_CHECK_RETVAL,
                  "Tamagotchi", launch_tamagotchi_plugin,
                  NULL, Icon_Plugin);

static int launch_clock_plugin(void *param)
{
    (void)param;
    char path[MAX_PATH];
    static const char *paths[] = {
        PLUGIN_APPS_DIR "/clock.rock",
        PLUGIN_DIR "/apps/clock.rock",
        ROCKBOX_DIR "/rocks/apps/clock.rock",
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
    {
        strcpy(path, paths[i]);
        if (file_exists(path))
            return load_plugin_path_screen(path, NULL);
    }

    splash(HZ, "Clock plugin missing\nInstall to .rockbox/rocks/apps/clock.rock");
    return GO_TO_ROOT;
}

static int launch_stopwatch_plugin(void *param)
{
    (void)param;
    char path[MAX_PATH];
    static const char *paths[] = {
        PLUGIN_APPS_DIR "/stopwatch.rock",
        PLUGIN_DIR "/apps/stopwatch.rock",
        ROCKBOX_DIR "/rocks/apps/stopwatch.rock",
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
    {
        strcpy(path, paths[i]);
        if (file_exists(path))
            return load_plugin_path_screen(path, NULL);
    }

    splash(HZ, "Stopwatch plugin missing\nInstall to .rockbox/rocks/apps/stopwatch.rock");
    return GO_TO_ROOT;
}

static int launch_alarmclock_plugin(void *param)
{
    (void)param;
    char path[MAX_PATH];
    static const char *paths[] = {
        PLUGIN_APPS_DIR "/alarmclock.rock",
        PLUGIN_DIR "/apps/alarmclock.rock",
        ROCKBOX_DIR "/rocks/apps/alarmclock.rock",
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
    {
        strcpy(path, paths[i]);
        if (file_exists(path))
            return load_plugin_path_screen(path, NULL);
    }

    splash(HZ, "Alarm Clock plugin missing\nInstall to .rockbox/rocks/apps/alarmclock.rock");
    return GO_TO_ROOT;
}

MENUITEM_FUNCTION(clock_item, MENU_FUNC_CHECK_RETVAL,
                  "Clock", launch_clock_plugin,
                  NULL, Icon_Plugin);

static int launch_lrcplayer_plugin(void *param)
{
    (void)param;
    return load_plugin_path_screen(PLUGIN_APPS_DIR "/lrcplayer.rock", NULL);
}

static int launch_maps_plugin(void *param)
{
    (void)param;
    char path[MAX_PATH];

    snprintf(path, sizeof(path), PLUGIN_APPS_DIR "/nb_maps.rock");
    if (!file_exists(path))
    {
        snprintf(path, sizeof(path), PLUGIN_DIR "/nb_maps.rock");
        if (!file_exists(path))
        {
            splash(HZ, "Maps not installed");
            return GO_TO_ROOT;
        }
    }
    return load_plugin_path_screen(path, NULL);
}

static int launch_pocketsky_plugin(void *param)
{
    (void)param;
    char path[MAX_PATH];
    static const char * const paths[] = {
        PLUGIN_APPS_DIR "/pocketsky.rock",
        PLUGIN_DIR "/apps/pocketsky.rock",
        ROCKBOX_DIR "/rocks/apps/pocketsky.rock",
    };

    for (size_t i = 0; i < ARRAYLEN(paths); i++)
    {
        strmemccpy(path, paths[i], sizeof(path));
        if (file_exists(path))
            return load_plugin_path_screen(path, NULL);
    }

    splash(HZ, "Pocket Sky plugin missing\nInstall to .rockbox/rocks/apps/pocketsky.rock");
    return GO_TO_ROOT;
}

static int launch_magazines_plugin(void *param)
{
    int result;

    (void)param;
    if (!file_exists(PLUGIN_APPS_DIR "/magazines.rock"))
    {
        splash(HZ * 2, "Magazines plugin missing");
        return GO_TO_ROOT;
    }
    result = load_plugin_path_screen(PLUGIN_APPS_DIR "/magazines.rock", NULL);
    root_menu_video_preview_invalidate_source_cache(
        IPODJS_PREVIEW_MAGAZINES);
    return result;
}

static int launch_comics_plugin(void *param)
{
    int result;

    (void)param;
    if (!file_exists(PLUGIN_APPS_DIR "/comics.rock"))
    {
        splash(HZ * 2, "Comics plugin missing");
        return GO_TO_ROOT;
    }
    result = load_plugin_path_screen(PLUGIN_APPS_DIR "/comics.rock", NULL);
    root_menu_video_preview_invalidate_source_cache(
        IPODJS_PREVIEW_COMICS);
    return result;
}

static int launch_pokemini(void *param);

    MENUITEM_FUNCTION(weather_item, MENU_FUNC_CHECK_RETVAL,
                  "Weather", launch_weather_plugin,
                  NULL, Icon_Plugin);
    MENUITEM_FUNCTION(maps_item, MENU_FUNC_CHECK_RETVAL,
                  "Maps", launch_maps_plugin,
                  NULL, Icon_Folder);
    MENUITEM_FUNCTION(pocketsky_item, MENU_FUNC_CHECK_RETVAL,
                  "Pocket Sky", launch_pocketsky_plugin,
                  NULL, Icon_Plugin);
    MENUITEM_FUNCTION(magazines_item, MENU_FUNC_CHECK_RETVAL,
                  "Magazines", launch_magazines_plugin,
                  NULL, Icon_Folder);
    MENUITEM_FUNCTION(comics_item, MENU_FUNC_CHECK_RETVAL,
                  "Comics", launch_comics_plugin,
                  NULL, Icon_Comics);
    MENUITEM_FUNCTION(pokedex_item, MENU_FUNC_CHECK_RETVAL,
                  "Pokedex", launch_pokedex_plugin,
                  NULL, Icon_Plugin);
    MAKE_MENU(applications_menu, "Extras", NULL, Icon_Plugin,
          &clock_item, &desktop_mode_item,
          &achievements_item, &livetv_item, &sitekick_item, &tamagotchi_item,
          &maps_item, &weather_item, &calm_item,
          &offlineweb_item, &magazines_item, &comics_item, &pocketsky_item,
#if defined(HAVE_IPODJS_UI) && defined(HAVE_TAGCACHE)
          &qrcode_item,
#endif
          &pokedex_item);

static const struct browse_folder_info gameboy_folder = {"/gameboy/", SHOW_ALL};
static const struct browse_folder_info pokemini_folder = {"/PokeMini/", SHOW_ALL};
#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 220)
#define INFONES_SAVE_DIR ROCKBOX_DIR "/infones"
#define INFONES_OPTIONS_PATH INFONES_SAVE_DIR "/options.cfg"

struct infones_root_options
{
    bool sound;
    bool autosave;
    int audio_quality;
};

static void infones_options_defaults(struct infones_root_options *options)
{
    options->sound = true;
    options->autosave = true;
    options->audio_quality = 1;
}

static void infones_options_parse_line(struct infones_root_options *options,
                                       const char *line)
{
    if (strncmp(line, "sound=", 6) == 0)
        options->sound = line[6] != '0';
    else if (strncmp(line, "autosave=", 9) == 0)
        options->autosave = line[9] != '0';
    else if (strncmp(line, "audio_quality=", 14) == 0)
        options->audio_quality = line[14] == '0' ? 0 : 1;
}

static void infones_options_load(struct infones_root_options *options)
{
    char buf[128];
    char line[32];
    int fd;
    ssize_t got;
    int i;
    int j = 0;

    infones_options_defaults(options);

    fd = open(INFONES_OPTIONS_PATH, O_RDONLY);
    if (fd < 0)
        return;

    got = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (got <= 0)
        return;

    buf[got] = '\0';
    for (i = 0; i <= got; i++)
    {
        char c = buf[i];

        if (c == '\n' || c == '\r' || c == '\0')
        {
            if (j > 0)
            {
                line[j] = '\0';
                infones_options_parse_line(options, line);
                j = 0;
            }
        }
        else if (j < (int)sizeof(line) - 1)
        {
            line[j++] = c;
        }
    }
}

static bool infones_options_save(const struct infones_root_options *options)
{
    int fd;

    mkdir(INFONES_SAVE_DIR);
    fd = open(INFONES_OPTIONS_PATH, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    fdprintf(fd, "sound=%d\nautosave=%d\naudio_quality=%d\n",
             options->sound ? 1 : 0, options->autosave ? 1 : 0,
             options->audio_quality ? 1 : 0);
    close(fd);
    return true;
}

static int infones_toggle_sound(void* param)
{
    struct infones_root_options options;

    (void)param;
    infones_options_load(&options);
    options.sound = !options.sound;
    if (infones_options_save(&options))
        splash(HZ, options.sound ? "NES sound on" : "NES sound off");
    else
        splash(HZ, "NES options failed");

    return 0;
}

static int infones_toggle_autosave(void* param)
{
    struct infones_root_options options;

    (void)param;
    infones_options_load(&options);
    options.autosave = !options.autosave;
    if (infones_options_save(&options))
        splash(HZ, options.autosave ? "NES autosave on" : "NES autosave off");
    else
        splash(HZ, "NES options failed");

    return 0;
}

static int infones_toggle_audio_quality(void* param)
{
    struct infones_root_options options;

    (void)param;
    infones_options_load(&options);
    options.audio_quality = options.audio_quality ? 0 : 1;
    if (infones_options_save(&options))
        splash(HZ, options.audio_quality ? "NES audio 22k" : "NES audio 11k");
    else
        splash(HZ, "NES options failed");

    return 0;
}

static bool infones_is_sav_name(const char *name)
{
    size_t len = strlen(name);

    return len > 4 &&
           tolower((unsigned char)name[len - 4]) == '.' &&
           tolower((unsigned char)name[len - 3]) == 's' &&
           tolower((unsigned char)name[len - 2]) == 'a' &&
           tolower((unsigned char)name[len - 1]) == 'v';
}

static int infones_clear_saves(void* param)
{
    DIR *dir;
    struct dirent *entry;
    int cleared = 0;

    (void)param;
    if (!yesno_pop("Clear NES saves?"))
        return 0;

    dir = opendir(INFONES_SAVE_DIR);
    if (!dir)
    {
        splash(HZ, "No NES saves");
        return 0;
    }

    while ((entry = readdir(dir)))
    {
        char path[MAX_PATH];

        if (!infones_is_sav_name(entry->d_name))
            continue;

        if (snprintf(path, sizeof(path), INFONES_SAVE_DIR "/%s",
                     entry->d_name) >= (int)sizeof(path))
            continue;

        if (remove(path) == 0)
            cleared++;
    }
    closedir(dir);

    splashf(HZ, "Cleared %d NES save%s", cleared, cleared == 1 ? "" : "s");
    return 0;
}

static int launch_gameboy_browser(void* param)
{
    (void)param;
    return load_plugin_path_screen(PLUGIN_GAMES_DIR "/rockboy_launcher.rock", NULL);
}

static int browse_gameboy_roms(void* param)
{
    (void)param;
    return browse_folder((void *)&gameboy_folder);
}

static int launch_podemon_go(void* param)
{
    (void)param;
    return load_plugin_path_screen(PLUGIN_GAMES_DIR "/pocketcatch.rock", NULL);
}

static int launch_pokemini(void* param)
{
    (void)param;
    if (file_exists(PLUGIN_GAMES_DIR "/pokemini_launcher.rock"))
        return load_plugin_path_screen(PLUGIN_GAMES_DIR "/pokemini_launcher.rock", NULL);

    return browse_folder((void *)&pokemini_folder);
}

static int launch_maker_lite(void* param)
{
    (void)param;
    if (!file_exists(PLUGIN_GAMES_DIR "/maker_lite.rock"))
    {
        splash(HZ, "Maker Lite not installed");
        return GO_TO_ROOT;
    }

    return load_plugin_path_screen(PLUGIN_GAMES_DIR "/maker_lite.rock", NULL);
}

static int launch_stickrpg(void* param)
{
    (void)param;
    if (!file_exists(VIEWERS_DIR "/flashplayer.rock"))
    {
        splash(HZ, "Flash Player not installed");
        return GO_TO_ROOT;
    }

    if (!file_exists(ROCKBOX_DIR "/flash/stickrpg/stickrpg.swf"))
    {
        splash(HZ, "Stick RPG SWF missing");
        return GO_TO_ROOT;
    }

    return load_plugin_path_screen(VIEWERS_DIR "/flashplayer.rock",
                                   ROCKBOX_DIR "/flash/stickrpg/stickrpg.swf");
}

static int launch_club_penguin(void* param)
{
    (void)param;
    if (!file_exists(PLUGIN_GAMES_DIR "/clubpenguin.rock"))
    {
        splash(HZ, "Club Penguin not installed");
        return GO_TO_ROOT;
    }

    return load_plugin_path_screen(PLUGIN_GAMES_DIR "/clubpenguin.rock", NULL);
}

static int launch_wwe_backstage(void* param)
{
    (void)param;
    if (!file_exists(PLUGIN_GAMES_DIR "/wwe_backstage.rock"))
    {
        splash(HZ, "WWE Backstage not installed");
        return GO_TO_ROOT;
    }

    if (!file_exists(ROCKBOX_DIR "/rocks/games/wwe_backstage/wwe-backstage.twv"))
    {
        splash(HZ, "WWE Backstage data missing");
        return GO_TO_ROOT;
    }

    return load_plugin_path_screen(PLUGIN_GAMES_DIR "/wwe_backstage.rock",
        ROCKBOX_DIR "/rocks/games/wwe_backstage/wwe-backstage.twv");
}

static int launch_runescape_classic(void* param)
{
    (void)param;
    if (!file_exists(PLUGIN_GAMES_DIR "/runescape_classic.rock"))
    {
        splash(HZ, "RuneScape Classic not installed");
        return GO_TO_ROOT;
    }

    return load_plugin_path_screen(PLUGIN_GAMES_DIR "/runescape_classic.rock",
                                   NULL);
}

static int launch_smsgg(void* param)
{
    (void)param;
    if (!file_exists(PLUGIN_GAMES_DIR "/smsgg.rock"))
    {
        splash(HZ, "SMSGG not installed");
        return GO_TO_ROOT;
    }

    return load_plugin_path_screen(PLUGIN_GAMES_DIR "/smsgg.rock", NULL);
}

static int browse_pokemini_roms(void* param)
{
    (void)param;
    return browse_folder((void *)&pokemini_folder);
}

MENUITEM_FUNCTION(gameboy_coverflow_item, MENU_FUNC_CHECK_RETVAL,
                  "Game Cover Flow", launch_gameboy_browser,
                  NULL, Icon_NOICON);
MENUITEM_FUNCTION(gameboy_files_item, MENU_FUNC_CHECK_RETVAL,
                  "Browse ROM Files", browse_gameboy_roms,
                  NULL, Icon_NOICON);
MENUITEM_FUNCTION(club_penguin_item, MENU_FUNC_CHECK_RETVAL,
                  "Club Penguin", launch_club_penguin,
                  NULL, Icon_Plugin);
MENUITEM_FUNCTION(stickrpg_item, MENU_FUNC_CHECK_RETVAL,
                  "Stick RPG", launch_stickrpg,
                  NULL, Icon_Plugin);
MENUITEM_FUNCTION(maker_lite_item, MENU_FUNC_CHECK_RETVAL,
                  "Maker Lite", launch_maker_lite,
                  NULL, Icon_Plugin);
MENUITEM_FUNCTION(wwe_backstage_item, MENU_FUNC_CHECK_RETVAL,
                  "WWE Backstage", launch_wwe_backstage,
                  NULL, Icon_Plugin);
MENUITEM_FUNCTION(runescape_classic_item, MENU_FUNC_CHECK_RETVAL,
                  "RuneScape Classic", launch_runescape_classic,
                  NULL, Icon_Plugin);
MENUITEM_FUNCTION(smsgg_item, MENU_FUNC_CHECK_RETVAL,
                  "Sega Master System / Game Gear", launch_smsgg,
                  NULL, Icon_Plugin);
MENUITEM_FUNCTION(infones_sound_item, MENU_FUNC_CHECK_RETVAL,
                  "Toggle NES Sound", infones_toggle_sound,
                  NULL, Icon_NOICON);
MENUITEM_FUNCTION(infones_autosave_item, MENU_FUNC_CHECK_RETVAL,
                  "Toggle NES Autosave", infones_toggle_autosave,
                  NULL, Icon_NOICON);
MENUITEM_FUNCTION(infones_audio_quality_item, MENU_FUNC_CHECK_RETVAL,
                  "Toggle NES Audio Quality", infones_toggle_audio_quality,
                  NULL, Icon_NOICON);
MENUITEM_FUNCTION(infones_clear_saves_item, MENU_FUNC_CHECK_RETVAL,
                  "Clear NES Saves", infones_clear_saves,
                  NULL, Icon_NOICON);
MAKE_MENU(gameboy_context_menu, "Games", NULL, Icon_NOICON,
          &club_penguin_item, &runescape_classic_item, &wwe_backstage_item,
          &stickrpg_item, &maker_lite_item, &smsgg_item,
          &gameboy_coverflow_item,
          &gameboy_files_item,
          &infones_sound_item, &infones_autosave_item,
          &infones_audio_quality_item,
          &infones_clear_saves_item);

MENUITEM_FUNCTION(gameboy_browser, MENU_FUNC_CHECK_RETVAL,
                  ID2P(LANG_PLUGIN_GAMES), launch_gameboy_browser,
                  NULL, Icon_Folder);
MENUITEM_FUNCTION(podemon_go_item, MENU_FUNC_CHECK_RETVAL,
                  "Podemon Go", launch_podemon_go,
                  NULL, Icon_Plugin);
MENUITEM_FUNCTION(pokemini_item, MENU_FUNC_CHECK_RETVAL,
                  "PokeMini", launch_pokemini,
                  NULL, Icon_Plugin);
MENUITEM_FUNCTION(pokemini_files_item, MENU_FUNC_CHECK_RETVAL,
                  "Browse PokeMini Files", browse_pokemini_roms,
                  NULL, Icon_NOICON);
MAKE_MENU(pokemini_context_menu, "PokeMini", NULL, Icon_NOICON,
          &pokemini_files_item);
#else
MENUITEM_FUNCTION_W_PARAM(gameboy_browser, MENU_FUNC_CHECK_RETVAL,
                          ID2P(LANG_PLUGIN_GAMES), browse_folder,
                          (void *)&gameboy_folder, NULL, Icon_Folder);
MENUITEM_RETURNVALUE(podemon_go_item, "Podemon Go", GO_TO_ROOT,
                     NULL, Icon_Plugin);
MENUITEM_FUNCTION_W_PARAM(pokemini_item, MENU_FUNC_CHECK_RETVAL,
                          "PokeMini", browse_folder,
                          (void *)&pokemini_folder, NULL, Icon_Plugin);
#endif
MENUITEM_RETURNVALUE(system_menu_, ID2P(LANG_SYSTEM), GO_TO_SYSTEM_SCREEN,
                     NULL, Icon_System_menu);

struct menu_item_ex root_menu_;
static struct menu_callback_with_desc root_menu_desc = {
        item_callback, ID2P(LANG_ROCKBOX_TITLE), Icon_Rockbox };

static struct menu_table menu_table[] = {
#ifdef HAVE_TAGCACHE
    { "pictureflow", &pictureflow_item },
    { "database", &db_browser },
#endif
    { "videos", &videos },
    { "directv", &livetv_item },
    { "applications", &applications_menu },
    { "internet", &offlineweb_item },
    { "photos", &photos_item },
    { "desktop", &desktop_mode_item },
    { "games", &gameboy_browser },
    { "podemon_go", &podemon_go_item },
    { "pokemini", &pokemini_item },
    { "files", &file_browser },
    { "wps", &wps_item },
    { "playlists", &playlists },
    { "plugins", &rocks_browser },
    { "shortcuts", &shortcut_menu },
    { "settings", &menu_ },
    { "system_menu", &system_menu_ },
};
#define MAX_MENU_ITEMS (sizeof(menu_table) / sizeof(struct menu_table))
static struct menu_item_ex *root_menu__[MAX_MENU_ITEMS];

/* Keep the uncustomized iPodJS home screen and Main Menu Configuration's
 * "Load Default" action on the same stock Apple-style item set. */
#ifdef HAVE_IPODJS_UI
static const struct menu_item_ex * const root_menu_ipodjs_default_items[] = {
#ifdef HAVE_TAGCACHE
    &pictureflow_item,
    &db_browser,
#endif
    &videos,
    &livetv_item,
    &photos_item,
    &offlineweb_item,
    &applications_menu,
    &menu_,
    &wps_item,
};
#endif

enum root_power_menu_choice
{
    ROOT_POWER_MENU_SHUTDOWN = 0,
    ROOT_POWER_MENU_REBOOT,
    ROOT_POWER_MENU_CANCEL,
};

MENUITEM_RETURNVALUE(root_power_shutdown_item, "Shut Down",
                     ROOT_POWER_MENU_SHUTDOWN, NULL, Icon_NOICON);
MENUITEM_RETURNVALUE(root_power_reboot_item, "Reboot",
                     ROOT_POWER_MENU_REBOOT, NULL, Icon_NOICON);
MAKE_MENU(root_power_menu, "Power", NULL, Icon_NOICON,
          &root_power_shutdown_item, &root_power_reboot_item);

struct menu_table *root_menu_get_options(int *nb_options)
{
    *nb_options = MAX_MENU_ITEMS;

    return menu_table;
}

void root_menu_load_from_cfg(void* setting, char *value)
{
    char *next = value, *start, *end;
    unsigned int menu_item_count = 0, i;

    if (*value == '-')
    {
        root_menu_set_default(setting, NULL);
        return;
    }
    root_menu_.flags = MENU_HAS_DESC | MT_MENU;
    root_menu_.submenus = (const struct menu_item_ex **)&root_menu__;
    root_menu_.callback_and_desc = &root_menu_desc;

    while (next && menu_item_count < MAX_MENU_ITEMS)
    {
        start = next;
        next = strchr(next, ',');
        if (next)
        {
            *next = '\0';
            next++;
        }
        start = skip_whitespace(start);
        if ((end = strchr(start, ' ')))
            *end = '\0';
        for (i=0; i<MAX_MENU_ITEMS; i++)
        {
            if (*start && !strcmp(start, menu_table[i].string))
            {
                root_menu__[menu_item_count++] = (struct menu_item_ex *)menu_table[i].item;
                break;
            }
        }
    }

    /* An empty root cannot be navigated. Preserve one recovery route, but
     * otherwise honor the configured visibility exactly. */
    if (menu_item_count == 0)
        root_menu__[menu_item_count++] = (struct menu_item_ex *)&menu_;
    root_menu_.flags |= MENU_ITEM_COUNT(menu_item_count);
    *(bool*)setting = true;
}

char* root_menu_write_to_cfg(void* setting, char*buf, int buf_len)
{
    (void)setting;
    unsigned i, j;

    if (buf == NULL || buf_len <= 0)
        return buf;

    for (i = 0; i < MENU_GET_COUNT(root_menu_.flags); i++)
    {
        for (j=0; j<MAX_MENU_ITEMS; j++)
        {
            if (menu_table[j].item == root_menu__[i])
            {
                int written = snprintf(buf, buf_len, "%s, ",
                                       menu_table[j].string);

                if (written < 0)
                    return buf;
                if (written >= buf_len)
                {
                    buf += buf_len - 1;
                    return buf;
                }
                buf_len -= written;
                buf += written;
                break;
            }
        }
    }
    return buf;
}

void root_menu_set_default(void* setting, void* defaultval)
{
    unsigned i;
    unsigned count = 0;
    (void)defaultval;

    root_menu_.flags = MENU_HAS_DESC | MT_MENU;
    root_menu_.submenus = (const struct menu_item_ex **)&root_menu__;
    root_menu_.callback_and_desc = &root_menu_desc;

#ifdef HAVE_IPODJS_UI
    if (global_settings.ui_engine == UI_ENGINE_IPODJS)
    {
        for (i = 0; i < ARRAYLEN(root_menu_ipodjs_default_items); i++)
            root_menu__[count++] = (struct menu_item_ex *)
                root_menu_ipodjs_default_items[i];
    }
    else
#endif
    {
        for (i=0; i<MAX_MENU_ITEMS; i++)
        {
            if (menu_table[i].item == &podemon_go_item ||
                menu_table[i].item == &pokemini_item ||
                menu_table[i].item == &applications_menu ||
                menu_table[i].item == &desktop_mode_item)
                continue;

            root_menu__[count++] = (struct menu_item_ex *)menu_table[i].item;
        }
    }
    root_menu_.flags |= MENU_ITEM_COUNT(count);
    *(bool*)setting = false;
}

bool root_menu_is_changed(void* setting, void* defaultval)
{
    (void)defaultval;
    return *(bool*)setting;
}

static int item_callback(int action,
                         const struct menu_item_ex *this_item,
                         struct gui_synclist *this_list)
{
    (void)this_list;
    switch (action)
    {
        case ACTION_TREE_STOP:
            return ACTION_REDRAW;
        case ACTION_TREE_POWER_MENU:
            if (this_item == &root_menu_)
            {
                root_menu_open_power_menu();
                return ACTION_REDRAW;
            }
            return ACTION_NONE;
        case ACTION_REQUEST_MENUITEM:
#if CONFIG_TUNER
            if (this_item == &fm)
            {
                if (radio_hardware_present() == 0)
                    return ACTION_EXIT_MENUITEM;
            }
            else
#endif
                if (this_item == &bookmarks)
            {
                if (global_settings.usemrb == 0)
                    return ACTION_EXIT_MENUITEM;
            }
        break;
    }
    return action;
}

static void root_menu_wait_for_button_release(void)
{
    long timeout = current_tick + HZ / 2;

    while ((button_status() & (BUTTON_MENU | BUTTON_SELECT)) &&
           TIME_BEFORE(current_tick, timeout))
        sleep(HZ/50);
}

static bool root_menu_request_reboot(void)
{
#if CONFIG_CHARGING && !defined(HAVE_POWEROFF_WHILE_CHARGING)
    if (charger_inserted())
    {
        charging_splash();
        return false;
    }
#endif

    sys_reboot();
    return true;
}

static void root_menu_open_power_menu(void)
{
    int selection = 0;
    int result;

    root_menu_wait_for_button_release();
    button_clear_queue();

    result = do_menu(&root_power_menu, &selection, NULL, false);

    switch (result)
    {
    case ROOT_POWER_MENU_SHUTDOWN:
#if CONFIG_CHARGING && !defined(HAVE_POWEROFF_WHILE_CHARGING)
        if (charger_inserted())
            charging_splash();
        else
#endif
            sys_poweroff();
        break;

    case ROOT_POWER_MENU_REBOOT:
        root_menu_request_reboot();
        break;

    default:
        break;
    }
}

#if defined(IPOD_NANO2G)
#define NANO2G_DASH_BG          LCD_RGBPACK(13, 16, 20)
#define NANO2G_DASH_PANEL       LCD_RGBPACK(30, 22, 42)
#define NANO2G_DASH_PANEL_2     LCD_RGBPACK(48, 34, 68)
#define NANO2G_DASH_ACCENT      LCD_RGBPACK(157, 122, 230)
#define NANO2G_DASH_ACCENT_2    LCD_RGBPACK(232, 210, 255)
#define NANO2G_DASH_TEXT        LCD_RGBPACK(252, 249, 255)
#define NANO2G_DASH_DIM         LCD_RGBPACK(204, 195, 228)
#define NANO2G_DASH_DANGER      LCD_RGBPACK(255, 118, 150)

static unsigned char root_menu_nano2g_bmp_buf[
    BM_SIZE(LCD_WIDTH, LCD_HEIGHT, FORMAT_NATIVE, false)];
static struct bitmap root_menu_nano2g_bmp;
static char root_menu_nano2g_bmp_path[MAX_PATH];
static time_t root_menu_nano2g_bmp_mtime;

static time_t root_menu_nano2g_path_mtime(const char *path)
{
    char dirpath[MAX_PATH];
    char *name;
    DIR *dir;
    struct dirent *entry;
    time_t mtime = 0;

    strmemccpy(dirpath, path, sizeof(dirpath));
    name = strrchr(dirpath, '/');
    if (!name || name == dirpath)
        return 0;

    *name++ = '\0';
    dir = opendir(dirpath);
    if (!dir)
        return 0;

    while ((entry = readdir(dir)))
    {
        if (!strcmp(entry->d_name, name))
        {
            mtime = dir_get_info(dir, entry).mtime;
            break;
        }
    }

    closedir(dir);
    return mtime;
}

static bool root_menu_nano2g_draw_bitmap(const char *name)
{
    char path[MAX_PATH];
    time_t mtime;

    snprintf(path, sizeof(path), WPS_DIR "/iPone_nano2g/%s", name);
    mtime = root_menu_nano2g_path_mtime(path);

    if (strcmp(root_menu_nano2g_bmp_path, path) || root_menu_nano2g_bmp_mtime != mtime)
    {
        root_menu_nano2g_bmp.data = root_menu_nano2g_bmp_buf;
        root_menu_nano2g_bmp.width = LCD_WIDTH;
        root_menu_nano2g_bmp.height = LCD_HEIGHT;
        if (read_bmp_file(path, &root_menu_nano2g_bmp,
                          sizeof(root_menu_nano2g_bmp_buf),
                          FORMAT_NATIVE | FORMAT_DITHER, NULL) < 0)
        {
            root_menu_nano2g_bmp_path[0] = '\0';
            root_menu_nano2g_bmp_mtime = 0;
            return false;
        }

        strmemccpy(root_menu_nano2g_bmp_path, path,
                   sizeof(root_menu_nano2g_bmp_path));
        root_menu_nano2g_bmp_mtime = mtime;
    }

    lcd_bmp(&root_menu_nano2g_bmp, 0, 0);
    return true;
}

static int root_menu_nano2g_count(void)
{
    return MENU_GET_COUNT(root_menu_.flags);
}

static const char *root_menu_nano2g_label(const struct menu_item_ex *item)
{
    if ((item->flags & MENU_TYPE_MASK) == MT_RETURN_VALUE)
    {
        switch (item->value)
        {
#ifdef HAVE_TAGCACHE
            case GO_TO_DBBROWSER:
                return "Music";
            case GO_TO_PICTUREFLOW:
                return "Covers";
#endif
            case GO_TO_FILEBROWSER:
                return "Files";
            case GO_TO_VIDEOS:
                return "Videos";
            case GO_TO_WPS:
                return audio_status() ? "Playing" : "Resume";
            case GO_TO_MAINMENU:
                return "Setup";
            case GO_TO_BROWSEPLUGINS:
                return "Apps";
            case GO_TO_PLAYLISTS_SCREEN:
                return "Lists";
            case GO_TO_SYSTEM_SCREEN:
                return "Info";
            case GO_TO_SHORTCUTMENU:
                return "Pins";
#if CONFIG_TUNER
            case GO_TO_FM:
                return "Radio";
#endif
        }
    }

    if (item == &photos_item)
        return "Photos";
    if (item == &applications_menu)
        return "Apps";
    if (item == &livetv_item)
        return "DIRECTV";

    return "Menu";
}

static const char *root_menu_nano2g_glyph(const struct menu_item_ex *item)
{
    if ((item->flags & MENU_TYPE_MASK) == MT_RETURN_VALUE)
    {
        switch (item->value)
        {
#ifdef HAVE_TAGCACHE
            case GO_TO_DBBROWSER:
                return "M";
            case GO_TO_PICTUREFLOW:
                return "C";
#endif
            case GO_TO_FILEBROWSER:
                return "F";
            case GO_TO_VIDEOS:
                return "V";
            case GO_TO_WPS:
                return ">";
            case GO_TO_MAINMENU:
                return "*";
            case GO_TO_BROWSEPLUGINS:
                return "A";
            case GO_TO_PLAYLISTS_SCREEN:
                return "L";
            case GO_TO_SYSTEM_SCREEN:
                return "i";
            case GO_TO_SHORTCUTMENU:
                return "P";
#if CONFIG_TUNER
            case GO_TO_FM:
                return "R";
#endif
        }
    }

    if (item == &photos_item)
        return "P";
    if (item == &applications_menu)
        return "A";

    return ".";
}

static void root_menu_nano2g_puts_fit(int x, int y, int width,
                                      const char *text, bool center)
{
    char buf[64];
    int w, h, len;

    if (!text || !text[0])
        return;

    strmemccpy(buf, text, sizeof(buf));
    len = strlen(buf);
    lcd_getstringsize((const unsigned char *)buf, &w, &h);
    while (len > 1 && w > width)
    {
        buf[--len] = '\0';
        lcd_getstringsize((const unsigned char *)buf, &w, &h);
    }

    if (center && w < width)
        x += (width - w) / 2;

    lcd_set_drawmode(DRMODE_FG);
    lcd_putsxy(x, y, (const unsigned char *)buf);
    lcd_set_drawmode(DRMODE_SOLID);
}

static void root_menu_nano2g_fill_roundish(int x, int y, int w, int h,
                                           unsigned color)
{
    lcd_set_foreground(color);
    lcd_fillrect(x + 1, y, w - 2, h);
    lcd_fillrect(x, y + 1, w, h - 2);
}

static void root_menu_nano2g_draw_meter(int x, int y, int w, int h,
                                        int percent, unsigned fill)
{
    int inner = (w - 2) * MAX(0, MIN(100, percent)) / 100;

    lcd_set_foreground(NANO2G_DASH_DIM);
    lcd_drawrect(x, y, w, h);
    lcd_set_foreground(fill);
    lcd_fillrect(x + 1, y + 1, inner, h - 2);
}

static void root_menu_nano2g_time12(char *buf, size_t buf_size,
                                    const struct tm *tm, bool full_suffix)
{
    int hour;

    if (!tm || !valid_time(tm))
    {
        snprintf(buf, buf_size, "--:--");
        return;
    }

    hour = tm->tm_hour % 12;
    if (hour == 0)
        hour = 12;

    if (full_suffix)
        snprintf(buf, buf_size, "%d:%02d %s", hour, tm->tm_min,
                 tm->tm_hour < 12 ? "AM" : "PM");
    else
        snprintf(buf, buf_size, "%d:%02d%c", hour, tm->tm_min,
                 tm->tm_hour < 12 ? 'a' : 'p');
}

static const char *root_menu_nano2g_now_title(void)
{
    struct mp3entry *id3 = audio_current_track();
    char *name;

    if (id3)
    {
        if (id3->title && id3->title[0])
            return id3->title;

        name = strrchr(id3->path, '/');
        return (name && name[1]) ? name + 1 : id3->path;
    }

    return "Ready";
}

static const char *root_menu_nano2g_now_subtitle(void)
{
    struct mp3entry *id3 = audio_current_track();

    if (id3 && id3->artist && id3->artist[0])
        return id3->artist;

    if (audio_status() & AUDIO_STATUS_PAUSE)
        return "Paused";
    if (audio_status() & AUDIO_STATUS_PLAY)
        return "Playing";

    return "Select Music or Files";
}

static bool root_menu_nano2g_has_now_playing(void)
{
    int status = audio_status();
    return (status & AUDIO_STATUS_PLAY) && !(status & AUDIO_STATUS_PAUSE);
}

static void root_menu_nano2g_draw_status(void)
{
    char buf[24];
    struct tm *tm = get_time();
    int batt = battery_level();

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(NANO2G_DASH_TEXT);
    root_menu_nano2g_puts_fit(6, 4, 62, "iPod", false);

    lcd_set_foreground(NANO2G_DASH_DIM);
    root_menu_nano2g_time12(buf, sizeof(buf), tm, false);
    root_menu_nano2g_puts_fit(69, 4, 38, buf, true);

    snprintf(buf, sizeof(buf), "%s%d%%", charger_inserted() ? "+" : "", batt);
    lcd_set_foreground(charger_inserted() ? NANO2G_DASH_ACCENT :
                                      (batt <= 15 ? NANO2G_DASH_DANGER :
                                                    NANO2G_DASH_TEXT));
    root_menu_nano2g_puts_fit(122, 4, 28, buf, false);
    root_menu_nano2g_draw_meter(150, 6, 20, 6, batt,
                                batt <= 15 ? NANO2G_DASH_DANGER :
                                             NANO2G_DASH_ACCENT);
}

static void root_menu_nano2g_draw_now_playing(bool selected)
{
    int elapsed = 0;
    int total = 100;
    int percent = 0;
    struct mp3entry *id3 = audio_current_track();

    root_menu_nano2g_fill_roundish(7, 18, 162, 28,
        selected ? LCD_RGBPACK(74, 48, 112) : NANO2G_DASH_PANEL);
    lcd_set_foreground(selected ? NANO2G_DASH_ACCENT_2 : NANO2G_DASH_ACCENT);
    lcd_fillrect(8, 19, 3, 26);

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(NANO2G_DASH_TEXT);
    root_menu_nano2g_puts_fit(15, 21, selected ? 94 : 106,
                              root_menu_nano2g_now_title(), false);
    lcd_set_foreground(NANO2G_DASH_DIM);
    root_menu_nano2g_puts_fit(15, 33, selected ? 94 : 106,
                              selected ? "Sel WPS  Play" :
                                         root_menu_nano2g_now_subtitle(),
                              false);

    lcd_set_foreground((audio_status() & AUDIO_STATUS_PAUSE) ?
                       NANO2G_DASH_ACCENT_2 : NANO2G_DASH_ACCENT);
    root_menu_nano2g_puts_fit(130, 25, 30,
                              (audio_status() & AUDIO_STATUS_PLAY) ?
                              ((audio_status() & AUDIO_STATUS_PAUSE) ? "PAUSE" : "PLAY") :
                              "IDLE", true);

    if (id3 && id3->length > 0)
    {
        elapsed = id3->elapsed;
        total = id3->length;
        percent = elapsed * 100 / total;
    }
    root_menu_nano2g_draw_meter(15, 43, 144, 3, percent, NANO2G_DASH_ACCENT);

    if (selected)
    {
        lcd_set_foreground(NANO2G_DASH_ACCENT_2);
        lcd_drawrect(7, 18, 162, 28);
    }
}

static void root_menu_nano2g_draw_list_item(int item_index, int slot,
                                            int selected, bool show_widget)
{
    int x = 7;
    int y = (show_widget ? 52 : 24) + slot * 19;
    int width = 162;
    int height = 16;
    bool is_selected = item_index == selected;
    const struct menu_item_ex *item = root_menu__[item_index];

    root_menu_nano2g_fill_roundish(x, y, width, height,
        is_selected ? LCD_RGBPACK(86, 56, 126) : NANO2G_DASH_PANEL);

    lcd_set_foreground(is_selected ? NANO2G_DASH_ACCENT_2 : NANO2G_DASH_ACCENT);
    lcd_fillrect(x + 4, y + 4, 11, 11);
    lcd_set_foreground(is_selected ? LCD_RGBPACK(86, 56, 126) : NANO2G_DASH_PANEL);
    root_menu_nano2g_puts_fit(x + 6, y + 5, 7,
                              root_menu_nano2g_glyph(item), true);

    lcd_set_foreground(is_selected ? NANO2G_DASH_TEXT : NANO2G_DASH_TEXT);
    root_menu_nano2g_puts_fit(x + 22, y + 5, 122,
                              root_menu_nano2g_label(item), false);

    lcd_set_foreground(is_selected ? NANO2G_DASH_ACCENT_2 : NANO2G_DASH_DIM);
    root_menu_nano2g_puts_fit(x + 146, y + 5, 12, ">", true);

    if (is_selected)
    {
        lcd_set_foreground(NANO2G_DASH_ACCENT_2);
        lcd_drawrect(x, y, width, height);
    }
}

static void root_menu_nano2g_draw_dashboard(int selected)
{
    bool show_widget = root_menu_nano2g_has_now_playing();
    int count = root_menu_nano2g_count();
    int page_size = show_widget ? 4 : 5;
    int page = selected < 0 ? 0 : selected / page_size;
    int start = page * page_size;
    int pages = (count + page_size - 1) / page_size;
    int i;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(NANO2G_DASH_BG);
    lcd_clear_display();

    lcd_set_foreground(LCD_RGBPACK(16, 11, 24));
    lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    lcd_set_foreground(LCD_RGBPACK(41, 24, 66));
    lcd_fillrect(0, 0, LCD_WIDTH, 34);
    lcd_set_foreground(LCD_RGBPACK(30, 16, 50));
    lcd_fillrect(0, 102, LCD_WIDTH, 30);
    lcd_set_foreground(LCD_RGBPACK(37, 22, 56));
    lcd_fillrect(0, 0, LCD_WIDTH, 16);

    root_menu_nano2g_draw_status();
    if (show_widget)
        root_menu_nano2g_draw_now_playing(selected < 0);

    for (i = 0; i < page_size && start + i < count; i++)
        root_menu_nano2g_draw_list_item(start + i, i, selected, show_widget);

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(NANO2G_DASH_DIM);
    root_menu_nano2g_puts_fit(8, 121, 65, "Wheel", false);
    root_menu_nano2g_puts_fit(103, 121, 65, "Select", false);
    for (i = 0; i < pages; i++)
    {
        lcd_set_foreground(i == page ? NANO2G_DASH_ACCENT_2 : NANO2G_DASH_DIM);
        lcd_fillrect(82 + i * 6, 124, 3, 3);
    }

    lcd_update();
}

static void root_menu_nano2g_draw_lockscreen(void)
{
    char buf[32];
    int batt = battery_level();

    backlight_on();
    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(LCD_RGBPACK(12, 8, 18));
    lcd_clear_display();

    if (!root_menu_nano2g_draw_bitmap(charger_inserted() ?
                                      "ChargeWallpaper.bmp" :
                                      "Wallpaper.bmp"))
    {
        lcd_set_foreground(LCD_RGBPACK(16, 11, 24));
        lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
        lcd_set_foreground(LCD_RGBPACK(55, 30, 90));
        lcd_fillrect(0, 0, LCD_WIDTH, 55);
    }

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(NANO2G_DASH_ACCENT_2);
    root_menu_nano2g_puts_fit(8, 7, 48, "HOLD", false);
    lcd_set_foreground(charger_inserted() ? NANO2G_DASH_ACCENT : NANO2G_DASH_TEXT);
    snprintf(buf, sizeof(buf), "%s%d%%", charger_inserted() ? "CHG " : "", batt);
    root_menu_nano2g_puts_fit(121, 7, 48, buf, false);

    lcd_update();
}

static int root_menu_nano2g_dashboard(int *selectedp)
{
    int selected = *selectedp;
    int count = root_menu_nano2g_count();
    bool redraw = true;

    selected = MAX(-1, MIN(selected, count - 1));

    viewportmanager_theme_enable(SCREEN_MAIN, false, NULL);
    button_clear_queue();

    while (true)
    {
        int action;

        if (button_hold())
        {
            root_menu_nano2g_draw_lockscreen();
            while (button_hold())
            {
                sleep(HZ/4);
                root_menu_nano2g_draw_lockscreen();
            }
            button_clear_queue();
            redraw = true;
        }

        if (redraw)
        {
            bool show_widget = root_menu_nano2g_has_now_playing();
            if (!show_widget && selected < 0)
                selected = 0;
            root_menu_nano2g_draw_dashboard(selected);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, HZ/5);
        switch (action)
        {
            case ACTION_NONE:
#ifdef HAVE_TAGCACHE
                if (selected >= 0 && selected < count &&
                    root_menu__[selected] == &pictureflow_item)
                    redraw = true;
#endif
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
            {
                bool show_widget = root_menu_nano2g_has_now_playing();
                if (show_widget)
                {
                    if (selected < 0)
                        selected = count - 1;
                    else if (selected == 0)
                        selected = -1;
                    else
                        selected--;
                }
                else
                {
                    if (selected <= 0)
                        selected = count - 1;
                    else
                        selected--;
                }
                redraw = true;
                break;
            }

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
            {
                bool show_widget = root_menu_nano2g_has_now_playing();
                if (show_widget)
                {
                    if (selected < 0)
                        selected = 0;
                    else if (selected >= count - 1)
                        selected = -1;
                    else
                        selected++;
                }
                else
                {
                    if (selected < 0 || selected >= count - 1)
                        selected = 0;
                    else
                        selected++;
                }
                redraw = true;
                break;
            }

            case ACTION_STD_OK:
                *selectedp = selected;
                viewportmanager_theme_undo(SCREEN_MAIN, false);
                if (selected < 0 && root_menu_nano2g_has_now_playing())
                    return GO_TO_WPS;
                if ((root_menu__[selected]->flags & MENU_TYPE_MASK) == MT_RETURN_VALUE)
                    return root_menu__[selected]->value;
                return GO_TO_ROOT;

            case ACTION_STD_CONTEXT:
                *selectedp = selected;
                if (selected < 0)
                {
                    if (audio_status() & AUDIO_STATUS_PLAY)
                    {
                        if (audio_status() & AUDIO_STATUS_PAUSE)
                            audio_resume();
                        else
                            audio_pause();
                        redraw = true;
                        break;
                    }
                    viewportmanager_theme_undo(SCREEN_MAIN, false);
                    return GO_TO_WPS;
                }
                viewportmanager_theme_undo(SCREEN_MAIN, false);
                return GO_TO_ROOTITEM_CONTEXT;

            case ACTION_TREE_WPS:
                *selectedp = selected;
                if (selected < 0 && root_menu_nano2g_has_now_playing())
                {
                    if (audio_status() & AUDIO_STATUS_PAUSE)
                        audio_resume();
                    else
                        audio_pause();
                    redraw = true;
                    break;
                }
                viewportmanager_theme_undo(SCREEN_MAIN, false);
                return GO_TO_WPS;

            case ACTION_TREE_POWER_MENU:
                root_menu_open_power_menu();
                redraw = true;
                button_clear_queue();
                break;

            case ACTION_STD_MENU:
                *selectedp = selected;
                viewportmanager_theme_undo(SCREEN_MAIN, false);
                return GO_TO_MAINMENU;

            case ACTION_STD_CANCEL:
                if (selected < 0 && (audio_status() & AUDIO_STATUS_PLAY))
                {
                    audio_prev();
                    redraw = true;
                    break;
                }
                *selectedp = selected;
                viewportmanager_theme_undo(SCREEN_MAIN, false);
                return GO_TO_PREVIOUS;
        }
    }
}
#endif /* IPOD_NANO2G */

#ifdef HAVE_IPODJS_UI
#define IPODJS_HEADER_TOP       LCD_RGBPACK(252, 253, 253)
#define IPODJS_HEADER_MID       LCD_RGBPACK(216, 219, 223)
#define IPODJS_HEADER_BOTTOM    LCD_RGBPACK(174, 178, 183)
#define IPODJS_HEADER_BORDER    LCD_RGBPACK(126, 134, 143)
#define IPODJS_PREVIEW_TEXT     LCD_RGBPACK(255, 255, 255)
#define IPODJS_PREVIEW_TOP      LCD_RGBPACK(185, 192, 202)
#define IPODJS_PREVIEW_MID      LCD_RGBPACK(130, 138, 151)
#define IPODJS_PREVIEW_BOTTOM   LCD_RGBPACK(94, 102, 116)
#define IPODJS_LOCK_TOP         LCD_RGBPACK(247, 248, 249)
#define IPODJS_LOCK_MID         LCD_RGBPACK(199, 204, 211)
#define IPODJS_LOCK_BOTTOM      LCD_RGBPACK(116, 126, 140)
#define IPODJS_ACTIVE_TOP       LCD_RGBPACK(107, 200, 254)
#define IPODJS_ACTIVE_MID       LCD_RGBPACK(38, 146, 226)
#define IPODJS_SPLIT            LCD_RGBPACK(210, 210, 210)
#define IPODJS_BATTERY_BORDER   LCD_RGBPACK(98, 98, 98)
#define IPODJS_BATTERY_BG       LCD_RGBPACK(84, 88, 91)
#define IPODJS_BATTERY_HEALTHY  LCD_RGBPACK(165, 224, 127)
#define IPODJS_BATTERY_WARNING  LCD_RGBPACK(209, 127, 107)
#define IPODJS_BATTERY_CAP      LCD_RGBPACK(196, 196, 196)
#define IPODJS_LIST_WIDTH       (LCD_WIDTH / 2)
#define IPODJS_PREVIEW_X        IPODJS_LIST_WIDTH
#define IPODJS_SPLIT_X          (IPODJS_LIST_WIDTH - 1)
#define IPODJS_HEADER_HEIGHT    24
#define IPODJS_MENU_BOTTOM_INSET 18
#define IPODJS_DB_MAX_ROWS      1024
#define IPODJS_DB_WINDOW_ROWS   96
#define IPODJS_DB_WINDOW_MARGIN 16
#define IPODJS_DB_MAX_DEPTH     3
#define IPODJS_DB_LABEL_LEN     64
#define IPODJS_WPS_ART_MAX      128
#define IPODJS_DB_ART_MAX       40
#define IPODJS_DB_ART_STOCK_SIZE 40
#define IPODJS_DB_ART_CACHE     8
#define IPODJS_DB_ALBUM_ROW_H   48
#define IPODJS_DB_ART_WORK_EXTRA (44 * 1024)
#define IPODJS_STOCK_ART_SIZE   128
#define IPODJS_PREVIEW_IMAGE_WIDTH 320
#define IPODJS_PREVIEW_IMAGE_HEIGHT 240
#define IPODJS_PREVIEW_IMAGE_CACHE 2
/* A second static pane avoids re-decoding recent root-menu previews on 6G. */
#if defined(IPOD_6G)
#define IPODJS_MENU_PREVIEW_CACHE 2
#else
#define IPODJS_MENU_PREVIEW_CACHE 1
#endif
#define IPODJS_PREVIEW_MAX_ITEMS 64
#define IPODJS_SLIDESHOW_DELAY  MAX(1, HZ / 12)
#define IPODJS_ROOT_PREVIEW_SETTLE_DELAY MAX(1, HZ / 2)
#define IPODJS_SLIDESHOW_AUDIO_DELAY MAX(1, HZ / 10)
#define IPODJS_PREVIEW_DECODE_INTERVAL (HZ / 6)
#define IPODJS_PREVIEW_DECODE_BURST 2
#define IPODJS_WEATHER_PREVIEW_FRAMES 4
#define IPODJS_MAPS_PREVIEW_FRAMES 12
#define IPODJS_DB_ALBUM_CACHE_TTL (HZ * 120)
#define IPODJS_DB_PLAYPAUSE_DEBOUNCE MAX(1, HZ / 5)
#define IPODJS_ASSET_DIR        ROCKBOX_DIR "/ipodjs"
#define IPODJS_APPLE_ASSET_DIR  ROCKBOX_DIR "/ipodjs/apple"
#define IPODJS_PHOTOS_LOCKS     PLUGIN_APPS_DATA_DIR "/photos.locks"
#define IPODJS_DESKTOP_PREVIEW  \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_snow_leopard/ipodjs/" \
    "menu-preview.174x220x16.bmp"
#define IPODJS_LIVETV_PREVIEW   \
    IPODJS_ASSET_DIR "/previews/livetv.174x240x24.bmp"

#if defined(LOGF_ENABLE) && defined(SIMULATOR)
#define IPODJS_LOGF(...) logf(__VA_ARGS__)
#else
#define IPODJS_LOGF(...) do { } while (0)
#endif

static int root_menu_video_theme_depth;
static bool root_menu_video_native_handoff;
static long root_menu_video_last_playpause_tick;
static bool root_menu_video_tree_stop_pending;
static bool root_menu_video_wps_force_full = true;
static long root_menu_video_hold_storm_until;
static long root_menu_video_hold_storm_window_start;
static int root_menu_video_hold_storm_toggles;
static bool root_menu_video_hold_static_frame_valid;

bool root_menu_ipodjs_native_screen_active(void)
{
    return root_menu_video_theme_depth > 0 ||
           root_menu_video_native_handoff;
}

static bool root_menu_video_hold_storm_active(void)
{
    return TIME_BEFORE(current_tick, root_menu_video_hold_storm_until);
}

static int root_menu_video_count(void)
{
    const struct menu_item_ex * const *items;
    int source_count;
    int count = 0;

    if (global_settings.root_menu_customized)
    {
        items = (const struct menu_item_ex * const *)root_menu__;
        source_count = MENU_GET_COUNT(root_menu_.flags);
    }
    else
    {
        items = root_menu_ipodjs_default_items;
        source_count = ARRAYLEN(root_menu_ipodjs_default_items);
    }
    for (int i = 0; i < source_count; i++)
        if (items[i] != &offlineweb_item || usb_internet_connected())
            count++;
    return count;
}

static const struct menu_item_ex *root_menu_video_item(int index)
{
    const struct menu_item_ex * const *items;
    int source_count;
    int visible = 0;

    if (index < 0)
        return NULL;
    if (global_settings.root_menu_customized)
    {
        items = (const struct menu_item_ex * const *)root_menu__;
        source_count = MENU_GET_COUNT(root_menu_.flags);
    }
    else
    {
        items = root_menu_ipodjs_default_items;
        source_count = ARRAYLEN(root_menu_ipodjs_default_items);
    }
    for (int i = 0; i < source_count; i++)
    {
        if (items[i] == &offlineweb_item && !usb_internet_connected())
            continue;
        if (visible++ == index)
            return items[i];
    }
    return NULL;
}

static bool root_menu_video_has_current_track(void)
{
    struct mp3entry *id3 = audio_current_track();

    return id3 && id3->path[0] != '\0';
}

static bool root_menu_video_try_resume_playback(void)
{
    int resume_ret;

    if (root_menu_video_has_current_track())
    {
        playlist_start(0, 0, 0);
        if (audio_status() & AUDIO_STATUS_PLAY)
        {
            IPODJS_LOGF("ipodjs: resumed current track");
            return true;
        }
    }

    resume_ret = playlist_resume();
    if (resume_ret != -1)
    {
        if (global_status.resume_index >= 0)
        {
            playlist_resume_track(global_status.resume_index,
                                  global_status.resume_crc32,
                                  global_status.resume_elapsed,
                                  global_status.resume_offset);
        }
        else
            playlist_start(0, 0, 0);

        if (audio_status() & AUDIO_STATUS_PLAY)
        {
            IPODJS_LOGF("ipodjs: resumed playlist index=%d", global_status.resume_index);
            return true;
        }
    }

    IPODJS_LOGF("ipodjs: no resumable playback state");
    return false;
}

static bool root_menu_video_enabled(void)
{
    return global_settings.ui_engine == UI_ENGINE_IPODJS;
}

/* Keep the iPodJS dashboard on Classic hardware, but hand music browsing and
 * playback to Rockbox's normal tagtree/WPS lifecycle.  This deliberately
 * avoids maintaining a second database/playlist/WPS path on the 6G. */
static bool root_menu_video_uses_stock_music(void)
{
#ifdef IPOD_6G
    return true;
#else
    return false;
#endif
}

static void root_menu_video_enter_native_screen(void)
{
    if (root_menu_video_theme_depth++ == 0)
    {
        bool direct_handoff = root_menu_video_native_handoff;

        ipodjs_ui_prepare_bluetooth_indicator();

        if (root_menu_video_native_handoff)
            root_menu_video_native_handoff = false;
        else
            viewportmanager_theme_enable(SCREEN_MAIN, false, NULL);

        /* Keep the old frame until the destination draws its first frame. */
        if (!direct_handoff)
            ipodjs_ui_prepare_native_frame();
    }
}

static int root_menu_video_finish_native_screen(int ret)
{
    if (root_menu_video_theme_depth > 0 &&
        --root_menu_video_theme_depth == 0)
    {
        if (root_menu_video_enabled() &&
            (ret == GO_TO_ROOT || ret == GO_TO_WPS ||
             (root_menu_video_uses_stock_music() &&
              ret == GO_TO_DBBROWSER)))
            root_menu_video_native_handoff = true;
        else
        {
            root_menu_video_native_handoff = false;
            viewportmanager_theme_undo(SCREEN_MAIN, false);
        }
    }

    return ret;
}

void root_menu_ipodjs_enter_wps_frame(void)
{
    if (root_menu_video_enabled())
    {
        root_menu_video_wps_force_full = true;
        root_menu_video_enter_native_screen();
    }
}

void root_menu_ipodjs_leave_wps_frame(void)
{
    if (root_menu_video_enabled())
    {
        root_menu_video_wps_force_full = true;
        root_menu_video_finish_native_screen(GO_TO_ROOT);
    }
}

static int root_menu_video_row_height(void)
{
    return ipodjs_ui_row_height();
}

static int root_menu_video_visible_rows(int row_h)
{
    int h = LCD_HEIGHT - IPODJS_HEADER_HEIGHT;

    return MAX(1, h / MAX(1, row_h));
}

static unsigned root_menu_video_accent(void)
{
    return ipodjs_ui_accent();
}

static const char *root_menu_video_accent_name(void)
{
    switch (global_settings.ui_engine_accent)
    {
        case UI_ENGINE_ACCENT_GRAPHITE:
            return "Graphite";
        case UI_ENGINE_ACCENT_U2:
            return "U2 Red";
        case UI_ENGINE_ACCENT_TEAL:
            return "Teal";
        case UI_ENGINE_ACCENT_GREEN:
            return "Green";
        case UI_ENGINE_ACCENT_GOLD:
            return "Gold";
        case UI_ENGINE_ACCENT_ORANGE:
            return "Orange";
        case UI_ENGINE_ACCENT_PURPLE:
            return "Purple";
        case UI_ENGINE_ACCENT_PINK:
            return "Pink";
        case UI_ENGINE_ACCENT_BLUE:
        default:
            return "Blue";
    }
}

static bool root_menu_video_dark(void)
{
    return global_settings.ui_engine_dark_mode;
}

static unsigned root_menu_video_screen_bg(void)
{
    return ipodjs_ui_screen_bg();
}

static unsigned root_menu_video_row_bg(void)
{
    return ipodjs_ui_row_bg();
}

static unsigned root_menu_video_text(void)
{
    return ipodjs_ui_text();
}

static unsigned root_menu_video_muted_text(void)
{
    return ipodjs_ui_muted_text();
}

static unsigned root_menu_video_header_text(void)
{
    return ipodjs_ui_header_text();
}

static unsigned root_menu_video_header_bg(void)
{
    return ipodjs_ui_header_bg();
}

static int root_menu_video_font(void)
{
    return ipodjs_ui_font();
}

static int root_menu_video_wps_font(bool bold)
{
    static int regular_font = -2;
    static int bold_font = -2;
    int *fontp = bold ? &bold_font : &regular_font;
    const char *name = bold ? "14-Adobe-Helvetica-Bold.fnt" :
                              "12-Adobe-Helvetica.fnt";
    char path[MAX_PATH];
    int loaded;

    if (*fontp >= 0)
        return *fontp;

    snprintf(path, sizeof(path), "%s/%s", IPODJS_ASSET_DIR, name);
    loaded = file_exists(path) ? font_load(path) : -1;
    if (loaded < 0)
    {
        snprintf(path, sizeof(path), "%s/%s", FONT_DIR, name);
        loaded = file_exists(path) ? font_load(path) : -1;
    }

    if (loaded >= 0)
    {
        font_lock(loaded, true);
        *fontp = loaded;
    }

    return *fontp >= 0 ? *fontp : root_menu_video_font();
}

static int root_menu_video_text_y_offset(void)
{
    return ipodjs_ui_text_y_offset();
}

static int root_menu_video_lock_font(void)
{
    static int lock_font = -2;
    const char *path = FONT_DIR "/35-Adobe-Helvetica-Bold.fnt";

    if (lock_font < 0 && file_exists(path))
    {
        int loaded = font_load(path);
        if (loaded >= 0)
        {
            font_lock(loaded, true);
            lock_font = loaded;
        }
    }

    return lock_font >= 0 ? lock_font : root_menu_video_font();
}

static unsigned root_menu_video_panel(void)
{
    return ipodjs_ui_panel();
}

static const char *root_menu_video_label(const struct menu_item_ex *item)
{
    if (!item)
        return "Menu";

    if ((item->flags & MENU_TYPE_MASK) == MT_RETURN_VALUE)
    {
        switch (item->value)
        {
#ifdef HAVE_TAGCACHE
            case GO_TO_DBBROWSER:
                return "Music";
            case GO_TO_PICTUREFLOW:
                return "Cover Flow";
#endif
            case GO_TO_FILEBROWSER:
                return "Files";
            case GO_TO_VIDEOS:
                return global_settings.ui_engine_video_appearance ==
                       UI_ENGINE_VIDEO_NETFLIX ? "Netflix" : "Videos";
            case GO_TO_WPS:
                return audio_status() ? "Now Playing" : "Now Playing";
            case GO_TO_MAINMENU:
                return "Settings";
            case GO_TO_BROWSEPLUGINS:
                return "Plugins";
            case GO_TO_PLAYLISTS_SCREEN:
                return "Playlists";
            case GO_TO_SYSTEM_SCREEN:
                return "System";
            case GO_TO_SHORTCUTMENU:
                return "Shortcuts";
#if CONFIG_TUNER
            case GO_TO_FM:
                return "Radio";
#endif
        }
    }

    if (item == &photos_item)
        return "Photos";
    if (item == &applications_menu)
        return "Extras";
    if (item == &offlineweb_item)
        return "Internet";
    if (item == &desktop_mode_item)
        return "Desktop Mode";
    if (item == &livetv_item)
        return "DIRECTV";
    if (item == &podemon_go_item)
        return "Podemon Go";
#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 220)
    if (item == &gameboy_browser)
        return "Games";
    if (item == &pokemini_item)
        return "PokeMini";
#endif

    return "Menu";
}

static const char *root_menu_video_preview(const struct menu_item_ex *item)
{
    const char *label = root_menu_video_label(item);

    if (!strcmp(label, "Music"))
        return "Music";
    if (!strcmp(label, "Cover Flow"))
        return "Cover Flow";
    if (!strcmp(label, "Now Playing"))
        return "Now Playing";
    if (!strcmp(label, "Settings"))
        return "Settings";
    if (!strcmp(label, "Extras"))
        return "Extras";
    return label;
}

static bool root_menu_video_item_is_settings(const struct menu_item_ex *item)
{
    return item && (item->flags & MENU_TYPE_MASK) == MT_RETURN_VALUE &&
           item->value == GO_TO_MAINMENU;
}

static bool root_menu_video_item_is_extras(const struct menu_item_ex *item)
{
    return item == &applications_menu;
}

static bool root_menu_video_item_is_games(const struct menu_item_ex *item)
{
#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 220)
    return item == &gameboy_browser || item == &pokemini_item;
#else
    (void)item;
    return false;
#endif
}

static bool root_menu_video_item_is_videos(const struct menu_item_ex *item)
{
    return item && (item->flags & MENU_TYPE_MASK) == MT_RETURN_VALUE &&
           item->value == GO_TO_VIDEOS;
}

static unsigned root_menu_video_rgb_blend(int br, int bg, int bb,
                                          int fr, int fg, int fb,
                                          int alpha)
{
    return ipodjs_ui_rgb_blend(br, bg, bb, fr, fg, fb, alpha);
}

static void root_menu_video_gradient(int x, int y, int w, int h,
                                     unsigned top, unsigned bottom)
{
    ipodjs_ui_gradient(&screens[SCREEN_MAIN], x, y, w, h, top, bottom);
}

static void root_menu_video_glass_gradient(int x, int y, int w, int h,
                                           unsigned top, unsigned mid,
                                           unsigned bottom)
{
    ipodjs_ui_glass_gradient(&screens[SCREEN_MAIN], x, y, w, h,
                             top, mid, bottom);
}

static void root_menu_video_preview_gradient(int x, int y, int w, int h)
{
    if (root_menu_video_dark())
    {
        root_menu_video_glass_gradient(x, y, w, h,
                                       LCD_RGBPACK(73, 81, 94),
                                       LCD_RGBPACK(39, 45, 56),
                                       LCD_RGBPACK(18, 22, 30));
        lcd_set_foreground(LCD_RGBPACK(96, 104, 118));
    }
    else
    {
        root_menu_video_glass_gradient(x, y, w, h, IPODJS_PREVIEW_TOP,
                                       IPODJS_PREVIEW_MID,
                                       IPODJS_PREVIEW_BOTTOM);
        lcd_set_foreground(root_menu_video_rgb_blend(255, 255, 255,
                                                     0, 0, 0, 34));
    }
    lcd_hline(x, x + w - 1, y);
}

static void root_menu_video_lock_gradient(int x, int y, int w, int h)
{
    root_menu_video_glass_gradient(x, y, w, h, IPODJS_LOCK_TOP,
                                   IPODJS_LOCK_MID,
                                   IPODJS_LOCK_BOTTOM);
    lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
    lcd_hline(x, x + w - 1, y);
}

static void root_menu_video_selection_gradient(int x, int y, int w, int h)
{
    ipodjs_ui_selection_gradient(&screens[SCREEN_MAIN], x, y, w, h, NULL);
}

static void root_menu_video_puts_fit(int x, int y, int width,
                                     const char *text, bool center);
static struct bitmap *root_menu_video_load_ui_bmp(const char *path,
                                                  struct bitmap *bm,
                                                  unsigned char *data,
                                                  size_t data_size,
                                                  int width, int height,
                                                  bool *tried, bool *valid);

static char root_menu_video_storage_cached[32];
static int root_menu_video_storage_cached_used_pct;
static long root_menu_video_storage_cached_tick;
static bool root_menu_video_storage_cached_valid;

static void root_menu_video_storage_info(char *buf, size_t buf_size,
                                         int *used_pctp, bool allow_refresh)
{
    sector_t size = 0;
    sector_t free = 0;
    unsigned long free_tenths_gb;
    unsigned long long total_kib;
    unsigned long long used_kib;

    if (root_menu_video_storage_cached_valid &&
        (!allow_refresh ||
         TIME_BEFORE(current_tick,
                     root_menu_video_storage_cached_tick + HZ * 300)))
    {
        strmemccpy(buf, root_menu_video_storage_cached, buf_size);
        if (used_pctp)
            *used_pctp = root_menu_video_storage_cached_used_pct;
        return;
    }

    volume_size(IF_MV(0,) &size, &free);
    if (size == 0)
    {
        strmemccpy(buf, "Storage loading", buf_size);
        if (used_pctp)
            *used_pctp = 0;
        return;
    }

    total_kib = (unsigned long long)size;
    used_kib = free < size ? (unsigned long long)(size - free) : 0;
    root_menu_video_storage_cached_used_pct =
        MIN(100, (int)((used_kib * 100) / total_kib));
    /* The stock Classic labels this value as GB (not Rockbox's GiB) and
     * keeps a single decimal place in the split-menu Settings preview. */
    free_tenths_gb = (unsigned long)
        ((((unsigned long long)free * 10) + (512ULL * 1024)) /
         (1024ULL * 1024));
    snprintf(root_menu_video_storage_cached,
             sizeof(root_menu_video_storage_cached), "%lu.%lu GB Free",
             free_tenths_gb / 10, free_tenths_gb % 10);
    root_menu_video_storage_cached_tick = current_tick;
    root_menu_video_storage_cached_valid = true;
    strmemccpy(buf, root_menu_video_storage_cached, buf_size);
    if (used_pctp)
        *used_pctp = root_menu_video_storage_cached_used_pct;
}

static struct bitmap root_menu_video_apple_logo_bm;
static unsigned char root_menu_video_apple_logo_data[
    BM_SIZE(48, 58, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
static bool root_menu_video_apple_logo_tried;
static bool root_menu_video_apple_logo_valid;

static struct bitmap root_menu_video_netflix_logo_bm;
static unsigned char root_menu_video_netflix_logo_data[
    BM_SIZE(150, 70, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
static bool root_menu_video_netflix_logo_tried;
static bool root_menu_video_netflix_logo_valid;
static struct bitmap root_menu_video_netflix_watched_bm;
static unsigned char root_menu_video_netflix_watched_data[
    BM_SIZE(VIDEO_LIST_NETFLIX_WATCHED_SIZE,
            VIDEO_LIST_NETFLIX_WATCHED_SIZE, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
static bool root_menu_video_netflix_watched_tried;
static bool root_menu_video_netflix_watched_valid;
static struct bitmap root_menu_video_netflix_logo_small_bm;
static unsigned char root_menu_video_netflix_logo_small_data[
    BM_SIZE(91, 42, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
static bool root_menu_video_netflix_logo_small_tried;
static bool root_menu_video_netflix_logo_small_valid;
static struct bitmap root_menu_video_steam_logo_bm;
static unsigned char root_menu_video_steam_logo_data[
    BM_SIZE(110, 32, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
static bool root_menu_video_steam_logo_tried;
static bool root_menu_video_steam_logo_valid;

static void root_menu_video_prepare_netflix_logo(void)
{
    if (global_settings.ui_engine_video_appearance !=
        UI_ENGINE_VIDEO_NETFLIX)
        return;

    (void)root_menu_video_load_ui_bmp(
        ROCKBOX_DIR "/ipodjs/netflix/netflix-logo-2001.150x70x24.bmp",
        &root_menu_video_netflix_logo_bm,
        root_menu_video_netflix_logo_data,
        sizeof(root_menu_video_netflix_logo_data), 150, 70,
        &root_menu_video_netflix_logo_tried,
        &root_menu_video_netflix_logo_valid);
}

static void root_menu_video_prepare_netflix_browser_logo(void)
{
    if (global_settings.ui_engine_video_appearance !=
        UI_ENGINE_VIDEO_NETFLIX)
        return;

    (void)root_menu_video_load_ui_bmp(
        ROCKBOX_DIR "/ipodjs/netflix/netflix-logo-2001.91x42x24.bmp",
        &root_menu_video_netflix_logo_small_bm,
        root_menu_video_netflix_logo_small_data,
        sizeof(root_menu_video_netflix_logo_small_data), 91, 42,
        &root_menu_video_netflix_logo_small_tried,
        &root_menu_video_netflix_logo_small_valid);
}

static struct bitmap *root_menu_video_netflix_logo_cached(void)
{
    return root_menu_video_netflix_logo_valid ?
           &root_menu_video_netflix_logo_bm : NULL;
}

static struct bitmap *root_menu_video_netflix_logo_small_cached(void)
{
    return root_menu_video_netflix_logo_small_valid ?
           &root_menu_video_netflix_logo_small_bm : NULL;
}

static void root_menu_video_prepare_netflix_watched_badge(void)
{
    if (global_settings.ui_engine_video_appearance !=
        UI_ENGINE_VIDEO_NETFLIX)
        return;

    (void)root_menu_video_load_ui_bmp(
        ROCKBOX_DIR "/ipodjs/netflix/watched.16x16x24.bmp",
        &root_menu_video_netflix_watched_bm,
        root_menu_video_netflix_watched_data,
        sizeof(root_menu_video_netflix_watched_data),
        VIDEO_LIST_NETFLIX_WATCHED_SIZE,
        VIDEO_LIST_NETFLIX_WATCHED_SIZE,
        &root_menu_video_netflix_watched_tried,
        &root_menu_video_netflix_watched_valid);
}

static struct bitmap *root_menu_video_netflix_watched_cached(void)
{
    return root_menu_video_netflix_watched_valid ?
           &root_menu_video_netflix_watched_bm : NULL;
}

static void root_menu_video_prepare_steam_logo(void)
{
    if (global_settings.ui_engine_games_appearance !=
        UI_ENGINE_GAMES_STEAM)
        return;

    (void)root_menu_video_load_ui_bmp(
        ROCKBOX_DIR "/ipodjs/steam/steam-logo-official.110x32x24.bmp",
        &root_menu_video_steam_logo_bm,
        root_menu_video_steam_logo_data,
        sizeof(root_menu_video_steam_logo_data), 110, 32,
        &root_menu_video_steam_logo_tried,
        &root_menu_video_steam_logo_valid);
}

static struct bitmap *root_menu_video_steam_logo_cached(void)
{
    return root_menu_video_steam_logo_valid ?
           &root_menu_video_steam_logo_bm : NULL;
}

static struct bitmap *root_menu_video_apple_logo_asset(void)
{
    return root_menu_video_load_ui_bmp(
        IPODJS_ASSET_DIR "/apple-logo-white.48x58x24.bmp",
        &root_menu_video_apple_logo_bm,
        root_menu_video_apple_logo_data,
        sizeof(root_menu_video_apple_logo_data), 48, 58,
        &root_menu_video_apple_logo_tried,
        &root_menu_video_apple_logo_valid);
}

static void root_menu_video_draw_apple_mark(int cx, int y)
{
    struct bitmap *bm = root_menu_video_apple_logo_asset();

    if (bm)
    {
        lcd_bmp(bm, cx - bm->width / 2, y);
        return;
    }
}

static void root_menu_video_draw_storage_bar(int x, int y, int w, int used_pct)
{
    int h = 11;
    int fill_w;

    /* Stock Classic: a dark inset capsule is the unused-space cutout and a
     * white capsule grows from the left to show used capacity. */
    lcd_set_foreground(LCD_RGBPACK(69, 78, 92));
    lcd_fillrect(x + 3, y + 1, w - 6, h);
    lcd_fillrect(x + 1, y + 3, w - 2, h - 4);
    lcd_fillrect(x, y + 5, w, h - 8);

    lcd_set_foreground(LCD_RGBPACK(105, 116, 132));
    lcd_fillrect(x + 3, y + 2, w - 6, h - 3);
    lcd_fillrect(x + 1, y + 4, w - 2, h - 7);

    fill_w = (w - 2) * MAX(0, MIN(used_pct, 100)) / 100;
    if (fill_w > 0)
    {
        /* Apple leaves even a nearly-empty device with a small white pip. */
        fill_w = MAX(h - 2, MIN(fill_w, w - 2));
        lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
        lcd_fillrect(x + 4, y + 1, MAX(1, fill_w - 6), h - 2);
        lcd_fillrect(x + 2, y + 3, MAX(1, fill_w - 2), h - 6);
        lcd_fillrect(x + 1, y + 4, fill_w, h - 8);
    }
}

static void root_menu_video_draw_settings_preview(int x, int y, int w, int h,
                                                  bool refresh_storage)
{
    char storage[32];
    int used_pct;
    int bar_w = w - 44;
    int bar_x = x + (w - bar_w) / 2;

    root_menu_video_preview_gradient(x, y, w, h);
    root_menu_video_puts_fit(x + 12, y + 18, w - 24, "iPod classic", true);
    root_menu_video_draw_apple_mark(x + w / 2, y + 62);
    lcd_set_foreground(IPODJS_PREVIEW_TEXT);
    lcd_set_background(root_menu_video_dark() ?
                       LCD_RGBPACK(18, 22, 30) : IPODJS_PREVIEW_BOTTOM);
    root_menu_video_storage_info(storage, sizeof(storage), &used_pct,
                                 refresh_storage);
    root_menu_video_draw_storage_bar(bar_x, y + 172, bar_w, used_pct);
    lcd_set_foreground(LCD_RGBPACK(222, 226, 232));
    root_menu_video_puts_fit(x + 14, y + 197, w - 28, storage, true);
    (void)h;
}

static void root_menu_video_puts_fit(int x, int y, int width,
                                     const char *text, bool center);
static int root_menu_video_quick_settings(void);
static int root_menu_video_settings_menu(void);
static int root_menu_video_extras_menu(void);
static int root_menu_video_applications_menu(void);
static int root_menu_video_games_menu(void);
static int root_menu_video_clock_screen(void);

#ifdef HAVE_ALBUMART
#ifndef IPOD_6G
static int root_menu_video_aa_slot = -1;
#endif

static struct bitmap root_menu_video_default_art_bm;
static unsigned char root_menu_video_default_art_data[
    BM_SCALED_SIZE(IPODJS_WPS_ART_MAX, IPODJS_WPS_ART_MAX,
                   FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
static bool root_menu_video_default_art_tried;
static bool root_menu_video_default_art_valid;

static struct bitmap root_menu_video_gloss_blue_bm;
static unsigned char root_menu_video_gloss_blue_data[
    BM_SIZE(64, 9, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
static bool root_menu_video_gloss_blue_tried;
static bool root_menu_video_gloss_blue_valid;
#endif

struct root_menu_video_apple_slider_assets {
    struct bitmap volume_low;
    struct bitmap volume_high;
    struct bitmap brightness_low;
    struct bitmap brightness_high;
    struct bitmap frame_light;
    struct bitmap frame_dark;
    struct bitmap fill;
    struct bitmap progress_frame;
    struct bitmap progress_fill;
    struct bitmap progress_fill_cap;
    unsigned char volume_low_data[
        BM_SIZE(11, 17, FORMAT_NATIVE, false) + 11 * 17 / 2 + 1] IPODJS_BM_ALIGN;
    unsigned char volume_high_data[
        BM_SIZE(19, 18, FORMAT_NATIVE, false) + 19 * 18 / 2 + 1] IPODJS_BM_ALIGN;
    unsigned char brightness_low_data[
        BM_SIZE(20, 21, FORMAT_NATIVE, false) + 20 * 21 / 2] IPODJS_BM_ALIGN;
    unsigned char brightness_high_data[
        BM_SIZE(30, 31, FORMAT_NATIVE, false) + 30 * 31 / 2] IPODJS_BM_ALIGN;
    unsigned char frame_light_data[
        BM_SIZE(248, 20, FORMAT_NATIVE, false) + 248 * 20 / 2] IPODJS_BM_ALIGN;
    unsigned char frame_dark_data[
        BM_SIZE(248, 20, FORMAT_NATIVE, false) + 248 * 20 / 2] IPODJS_BM_ALIGN;
    unsigned char fill_data[
        BM_SIZE(316, 20, FORMAT_NATIVE, false) + 316 * 20 / 2] IPODJS_BM_ALIGN;
    unsigned char progress_frame_data[
        BM_SIZE(200, 22, FORMAT_NATIVE, false) + 200 * 22 / 2] IPODJS_BM_ALIGN;
    unsigned char progress_fill_data[
        BM_SIZE(200, 22, FORMAT_NATIVE, false) + 200 * 22 / 2] IPODJS_BM_ALIGN;
    unsigned char progress_fill_cap_data[
        BM_SIZE(16, 16, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    bool tried[10];
    bool valid[10];
};

static struct root_menu_video_apple_slider_assets
    root_menu_video_apple_sliders;
static int root_menu_video_asset_dark_state = -1;

static void root_menu_video_invalidate_ui_asset_cache(void)
{
    root_menu_video_apple_logo_tried = false;
    root_menu_video_apple_logo_valid = false;
    root_menu_video_netflix_logo_tried = false;
    root_menu_video_netflix_logo_valid = false;
    root_menu_video_netflix_logo_small_tried = false;
    root_menu_video_netflix_logo_small_valid = false;
    root_menu_video_steam_logo_tried = false;
    root_menu_video_steam_logo_valid = false;
    memset(root_menu_video_apple_sliders.tried, 0,
           sizeof(root_menu_video_apple_sliders.tried));
    memset(root_menu_video_apple_sliders.valid, 0,
           sizeof(root_menu_video_apple_sliders.valid));
#ifdef HAVE_ALBUMART
    root_menu_video_default_art_tried = false;
    root_menu_video_default_art_valid = false;
    root_menu_video_gloss_blue_tried = false;
    root_menu_video_gloss_blue_valid = false;
#endif
}

static void root_menu_video_sync_asset_mode(void)
{
    int dark = root_menu_video_dark() ? 1 : 0;

    if (root_menu_video_asset_dark_state == dark)
        return;

    root_menu_video_asset_dark_state = dark;
    root_menu_video_invalidate_ui_asset_cache();
}

static bool root_menu_video_asset_suffix_path(const char *path,
                                              const char *suffix,
                                              char *buf,
                                              size_t buf_size)
{
    const char *dot;
    size_t base_len;
    size_t suffix_len;
    size_t ext_len;

    if (!path || !suffix || !buf || buf_size == 0)
        return false;

    dot = strrchr(path, '.');
    if (!dot)
        return false;

    base_len = dot - path;
    suffix_len = strlen(suffix);
    ext_len = strlen(dot);
    if (base_len + suffix_len + ext_len + 1 > buf_size)
        return false;

    memcpy(buf, path, base_len);
    snprintf(buf + base_len, buf_size - base_len, "%s%s", suffix, dot);
    return true;
}

static bool root_menu_video_apple_asset_path(const char *path, bool dark,
                                             char *buf, size_t buf_size)
{
    char apple_path[MAX_PATH];
    const char *rel;
    const size_t prefix_len = sizeof(IPODJS_ASSET_DIR) - 1;

    if (!path || !buf || buf_size == 0)
        return false;

    if (strncmp(path, IPODJS_ASSET_DIR, prefix_len) || path[prefix_len] != '/')
        return false;

    rel = path + prefix_len + 1;
    if (snprintf(apple_path, sizeof(apple_path), IPODJS_APPLE_ASSET_DIR
                 "/%s", rel) >= (int)sizeof(apple_path))
        return false;

    if (dark)
    {
        if (!root_menu_video_asset_suffix_path(apple_path, "-dark", buf,
                                               buf_size))
            return false;
    }
    else if (snprintf(buf, buf_size, "%s", apple_path) >= (int)buf_size)
        return false;

    if (file_exists(buf))
        return true;

    return false;
}

static const char *root_menu_video_asset_path(const char *path,
                                              char *buf,
                                              size_t buf_size)
{
    if (!path || !buf || buf_size == 0)
        return path;

    if (root_menu_video_dark() &&
        root_menu_video_apple_asset_path(path, true, buf, buf_size))
        return buf;

    if (root_menu_video_apple_asset_path(path, false, buf, buf_size))
        return buf;

    if (root_menu_video_dark() &&
        root_menu_video_asset_suffix_path(path, "-dark", buf, buf_size) &&
        file_exists(buf))
        return buf;

    return path;
}

static struct bitmap *root_menu_video_load_ui_bmp(const char *path,
                                                  struct bitmap *bm,
                                                  unsigned char *data,
                                                  size_t data_size,
                                                  int width, int height,
                                                  bool *tried, bool *valid)
{
    int rc;
    char themed_path[MAX_PATH];
    const char *load_path;

    root_menu_video_sync_asset_mode();
    if (*valid)
        return bm;
    if (*tried)
        return NULL;

    load_path = root_menu_video_asset_path(path, themed_path,
                                           sizeof(themed_path));
    *tried = true;
    if (!file_exists(load_path))
        return NULL;

    memset(bm, 0, sizeof(*bm));
    bm->width = width;
    bm->height = height;
    bm->format = FORMAT_NATIVE;
    bm->data = data;

    rc = read_bmp_file(load_path, bm, data_size,
                       FORMAT_NATIVE | FORMAT_DITHER |
                       FORMAT_TRANSPARENT, NULL);
    if (rc < 0)
        return NULL;

    *valid = true;
    return bm;
}

enum root_menu_video_apple_slider_asset {
    IPODJS_APPLE_VOLUME_LOW = 0,
    IPODJS_APPLE_VOLUME_HIGH,
    IPODJS_APPLE_BRIGHTNESS_LOW,
    IPODJS_APPLE_BRIGHTNESS_HIGH,
    IPODJS_APPLE_SLIDER_LIGHT,
    IPODJS_APPLE_SLIDER_DARK,
    IPODJS_APPLE_SLIDER_FILL,
    IPODJS_APPLE_PROGRESS_FRAME,
    IPODJS_APPLE_PROGRESS_FILL,
    IPODJS_APPLE_PROGRESS_FILL_CAP,
};

static struct bitmap *root_menu_video_apple_slider_asset(int asset)
{
    struct root_menu_video_apple_slider_assets *a =
        &root_menu_video_apple_sliders;
    const char *path;
    struct bitmap *bm;
    unsigned char *data;
    size_t data_size;
    int width;
    int height;

    switch (asset)
    {
        case IPODJS_APPLE_VOLUME_LOW:
            path = IPODJS_APPLE_ASSET_DIR
                "/volume-low.apple.11x17x24.bmp";
            bm = &a->volume_low;
            data = a->volume_low_data;
            data_size = sizeof(a->volume_low_data);
            width = 11;
            height = 17;
            break;
        case IPODJS_APPLE_VOLUME_HIGH:
            path = IPODJS_APPLE_ASSET_DIR
                "/volume-high.apple.19x18x24.bmp";
            bm = &a->volume_high;
            data = a->volume_high_data;
            data_size = sizeof(a->volume_high_data);
            width = 19;
            height = 18;
            break;
        case IPODJS_APPLE_BRIGHTNESS_LOW:
            path = IPODJS_APPLE_ASSET_DIR
                "/brightness-low.apple.20x21x32.bmp";
            bm = &a->brightness_low;
            data = a->brightness_low_data;
            data_size = sizeof(a->brightness_low_data);
            width = 20;
            height = 21;
            break;
        case IPODJS_APPLE_BRIGHTNESS_HIGH:
            path = IPODJS_APPLE_ASSET_DIR
                "/brightness-high.apple.30x31x32.bmp";
            bm = &a->brightness_high;
            data = a->brightness_high_data;
            data_size = sizeof(a->brightness_high_data);
            width = 30;
            height = 31;
            break;
        case IPODJS_APPLE_SLIDER_LIGHT:
            path = IPODJS_APPLE_ASSET_DIR
                "/slider-light.apple.248x20x32.bmp";
            bm = &a->frame_light;
            data = a->frame_light_data;
            data_size = sizeof(a->frame_light_data);
            width = 248;
            height = 20;
            break;
        case IPODJS_APPLE_SLIDER_DARK:
            path = IPODJS_APPLE_ASSET_DIR
                "/slider-dark.apple.248x20x32.bmp";
            bm = &a->frame_dark;
            data = a->frame_dark_data;
            data_size = sizeof(a->frame_dark_data);
            width = 248;
            height = 20;
            break;
        case IPODJS_APPLE_SLIDER_FILL:
            path = IPODJS_APPLE_ASSET_DIR
                "/slider-fill.apple.316x20x24.bmp";
            bm = &a->fill;
            data = a->fill_data;
            data_size = sizeof(a->fill_data);
            width = 316;
            height = 20;
            break;
        case IPODJS_APPLE_PROGRESS_FRAME:
            path = IPODJS_APPLE_ASSET_DIR
                "/progress-frame.apple.200x22x32.bmp";
            bm = &a->progress_frame;
            data = a->progress_frame_data;
            data_size = sizeof(a->progress_frame_data);
            width = 200;
            height = 22;
            break;
        case IPODJS_APPLE_PROGRESS_FILL:
            path = IPODJS_APPLE_ASSET_DIR
                "/progress-fill.apple.200x22x32.bmp";
            bm = &a->progress_fill;
            data = a->progress_fill_data;
            data_size = sizeof(a->progress_fill_data);
            width = 200;
            height = 22;
            break;
        case IPODJS_APPLE_PROGRESS_FILL_CAP:
            path = IPODJS_APPLE_ASSET_DIR
                "/progress-fill-cap.apple.16x16x24.bmp";
            bm = &a->progress_fill_cap;
            data = a->progress_fill_cap_data;
            data_size = sizeof(a->progress_fill_cap_data);
            width = 16;
            height = 16;
            break;
        default:
            return NULL;
    }

    return root_menu_video_load_ui_bmp(
        path, bm, data, data_size, width, height,
        &a->tried[asset], &a->valid[asset]);
}

static void root_menu_video_draw_play_icon(int x, int y, bool paused)
{
    (void)paused;
    ipodjs_ui_draw_playback_indicator(&screens[SCREEN_MAIN], x, y);
}

static void root_menu_video_draw_battery(int x, int y, int percent,
                                         bool charging)
{
    (void)percent;
    (void)charging;
    ipodjs_ui_draw_header_battery(&screens[SCREEN_MAIN], x, y);
}

#ifdef HAVE_ALBUMART
static bool root_menu_video_path_is_bmp(const char *path)
{
    size_t len = path ? strlen(path) : 0;

    if (len < 4)
        return false;

    path += len - 4;
    return path[0] == '.' &&
           (path[1] == 'b' || path[1] == 'B') &&
           (path[2] == 'm' || path[2] == 'M') &&
           (path[3] == 'p' || path[3] == 'P');
}

static void root_menu_video_ensure_aa_slot(void)
{
#ifdef IPOD_6G
    /* The stock iPodJS music path uses the normal WPS skin engine, which
     * already owns and sizes its album-art slots.  Claiming one here and
     * calling playback_update_aa_dims() after playback starts synchronously
     * tears down and remakes the audio buffer.  Reuse Rockbox's existing WPS
     * handle instead; decorative UI must never restart playback. */
    return;
#else
    if (root_menu_video_aa_slot < 0)
    {
        struct dim dim = { IPODJS_STOCK_ART_SIZE, IPODJS_STOCK_ART_SIZE };
        root_menu_video_aa_slot = playback_claim_aa_slot(&dim);
        if (root_menu_video_aa_slot >= 0)
            playback_update_aa_dims();
    }
#endif
}

static struct bitmap *root_menu_video_buffered_art(void)
{
    int handle;
    struct bitmap *bm = NULL;

#ifdef IPOD_6G
    /* The configured stock WPS claims the 128px player art before playback.
     * Do not create a second owner from the dashboard or WPS draw paths. */
    for (int slot = 0; slot < WPS_MAX_ALBUMART; slot++)
    {
        handle = playback_current_aa_hid(slot);
        if (handle < 0)
            continue;

        if (bufgetdata(handle, 0, (void *)&bm) > 0 && bm &&
            bm->width == IPODJS_STOCK_ART_SIZE &&
            bm->height == IPODJS_STOCK_ART_SIZE)
            return bm;
    }

    return NULL;
#else
    root_menu_video_ensure_aa_slot();
    if (root_menu_video_aa_slot < 0)
        return NULL;

    handle = playback_current_aa_hid(root_menu_video_aa_slot);
    if (handle < 0)
        return NULL;

    if (bufgetdata(handle, 0, (void *)&bm) <= 0)
        return NULL;

    return bm;
#endif
}

static struct bitmap *root_menu_video_load_cached_bmp(const char *path,
                                                      struct bitmap *bm,
                                                      unsigned char *data,
                                                      size_t data_size,
                                                      int width, int height,
                                                      bool *tried,
                                                      bool *valid)
{
    int rc;
    char themed_path[MAX_PATH];
    const char *load_path;

    root_menu_video_sync_asset_mode();
    if (*valid)
        return bm;
    if (*tried)
        return NULL;

    load_path = root_menu_video_asset_path(path, themed_path,
                                           sizeof(themed_path));
    *tried = true;
    if (!file_exists(load_path))
        return NULL;

    memset(bm, 0, sizeof(*bm));
    bm->width = width;
    bm->height = height;
    bm->format = FORMAT_NATIVE;
    bm->data = data;
    rc = read_bmp_file(load_path, bm, data_size,
                       FORMAT_NATIVE | FORMAT_DITHER, NULL);
    if (rc < 0)
        rc = read_bmp_file(load_path, bm, data_size,
                           FORMAT_NATIVE | FORMAT_RESIZE |
                           FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);
    if (rc < 0)
        return NULL;

    *valid = true;
    return bm;
}

static struct bitmap *root_menu_video_default_art(void)
{
    return root_menu_video_load_cached_bmp(
        IPODJS_ASSET_DIR "/default_album_artwork.128x128x24.bmp",
        &root_menu_video_default_art_bm,
        root_menu_video_default_art_data,
        sizeof(root_menu_video_default_art_data),
        IPODJS_WPS_ART_MAX, IPODJS_WPS_ART_MAX,
        &root_menu_video_default_art_tried,
        &root_menu_video_default_art_valid);
}

static struct bitmap *root_menu_video_gloss_blue(void)
{
    return root_menu_video_load_cached_bmp(
        IPODJS_ASSET_DIR "/gloss-blue.64x9x24.bmp",
        &root_menu_video_gloss_blue_bm,
        root_menu_video_gloss_blue_data,
        sizeof(root_menu_video_gloss_blue_data),
        64, 9,
        &root_menu_video_gloss_blue_tried,
        &root_menu_video_gloss_blue_valid);
}

static bool root_menu_video_try_cover_path(char *path, size_t path_size,
                                           const char *fmt,
                                           const char *dir,
                                           const char *name,
                                           int size)
{
    if (name)
        snprintf(path, path_size, fmt, dir, name, size, size);
    else
        snprintf(path, path_size, fmt, dir, size, size);
    return file_exists(path);
}

static bool root_menu_video_try_cover_formats(char *path, size_t path_size,
                                              const char *dir, int size)
{
    static const char * const formats[] = {
        "%scover.%dx%d.bmp",
        "%sCover.%dx%d.bmp",
        "%sfolder.%dx%d.bmp",
        "%sFolder.%dx%d.bmp",
        "%scover.bmp",
        "%sCover.bmp",
        "%scover.138x138.bmp",
        "%sCover.138x138.bmp",
        "%scover.51x51.bmp",
        "%sCover.51x51.bmp",
        "%scover.jpg",
        "%sCover.jpg",
        "%scover.jpeg",
        "%sCover.jpeg",
        "%sfolder.jpg",
        "%sFolder.jpg",
        "%sfolder.jpeg",
        "%sFolder.jpeg",
        "%sfolder.bmp",
        "%sFolder.bmp",
        "%salbumart.jpg",
        "%sAlbumArt.jpg",
        "%salbumart.jpeg",
        "%sAlbumArt.jpeg",
        "%salbumart.bmp",
        "%sAlbumArt.bmp",
    };
    size_t i;

    for (i = 0; i < ARRAYLEN(formats); i++)
    {
        if (root_menu_video_try_cover_path(path, path_size, formats[i],
                                           dir, NULL, size))
            return true;
    }

    return false;
}

static bool root_menu_video_find_local_cover(const struct mp3entry *id3,
                                             int size, char *path,
                                             size_t path_size)
{
    char dir[MAX_PATH];
    char album[IPODJS_DB_LABEL_LEN];
    char *slash;
    int dir_len;
    int album_len = 0;

    if (!id3 || !id3->path[0])
        return false;

    strmemccpy(dir, id3->path, sizeof(dir));
    slash = strrchr(dir, '/');
    if (!slash)
        return false;

    slash[1] = '\0';
    dir_len = strlen(dir);

    if (root_menu_video_try_cover_formats(path, path_size, dir, size))
        return true;

    if (id3->album && id3->album[0])
    {
        strmemccpy(album, id3->album, sizeof(album));
        album_len = strlen(album);
        snprintf(path, path_size, "%s%s.%dx%d.bmp", dir, album, size, size);
        fix_path_part(path, dir_len, album_len);
        if (file_exists(path))
            return true;

        snprintf(path, path_size, "%s%s.bmp", dir, album);
        fix_path_part(path, dir_len, album_len);
        if (file_exists(path))
            return true;

        snprintf(path, path_size, "%s%s.jpg", dir, album);
        fix_path_part(path, dir_len, album_len);
        if (file_exists(path))
            return true;

        snprintf(path, path_size, "%s%s.jpeg", dir, album);
        fix_path_part(path, dir_len, album_len);
        if (file_exists(path))
            return true;
    }

    return false;
}

#ifdef HAVE_TAGCACHE
struct root_menu_video_db_art_slot {
    bool valid;
    bool miss;
    int album_seek;
    int filter_tag;
    int filter_seek;
    int size;
    struct bitmap bm;
    unsigned char data[BM_SIZE(IPODJS_DB_ART_MAX, IPODJS_DB_ART_MAX,
                               FORMAT_NATIVE, false)];
};

static struct root_menu_video_db_art_slot
    root_menu_video_db_art_cache[IPODJS_DB_ART_CACHE];
static unsigned char root_menu_video_db_art_decode_data[
    BM_SIZE(IPODJS_DB_ART_MAX, IPODJS_DB_ART_MAX, FORMAT_NATIVE, false) +
    IPODJS_DB_ART_WORK_EXTRA] IPODJS_BM_ALIGN;
static int root_menu_video_db_art_next;

static bool root_menu_video_db_album_track_path(int album_seek,
                                                int filter_tag,
                                                int filter_seek,
                                                char *path,
                                                size_t path_size)
{
    struct tagcache_search tcs;
    char buf[TAGCACHE_BUFSZ];
    bool found;

    if (!path || path_size == 0 || !tagcache_search(&tcs, tag_filename))
        return false;

    tagcache_search_add_filter(&tcs, tag_album, album_seek);
    if (filter_tag >= 0)
        tagcache_search_add_filter(&tcs, filter_tag, filter_seek);

    found = tagcache_get_next(&tcs, buf, sizeof(buf));
    if (found)
        strmemccpy(path, buf, path_size);

    tagcache_search_finish(&tcs);
    return found;
}

static bool root_menu_video_bitmap_has_pixels(const struct bitmap *bm)
{
    const unsigned char *data;
    size_t bytes;

    if (!bm || !bm->data || bm->width <= 0 || bm->height <= 0)
        return false;

    data = bm->data;
    bytes = (size_t)bm->width * (size_t)bm->height * FB_DATA_SZ;
    for (size_t i = 0; i < bytes; i++)
    {
        if (data[i] != 0)
            return true;
    }

    return false;
}

static bool root_menu_video_db_album_id3(int album_seek, int filter_tag,
                                         int filter_seek,
                                         struct mp3entry *id3)
{
    char path[MAX_PATH];
    char normalized_path[MAX_PATH];
    const char *lookup_path = path;

    if (!root_menu_video_db_album_track_path(album_seek, filter_tag,
                                             filter_seek, path,
                                             sizeof(path)))
        return false;

    if (path[0] != '/')
    {
        snprintf(normalized_path, sizeof(normalized_path), "/%s", path);
        lookup_path = normalized_path;
    }

    memset(id3, 0, sizeof(*id3));
    strmemccpy(id3->path, lookup_path, sizeof(id3->path));
    return true;
}

static struct bitmap *root_menu_video_db_album_art(int album_seek,
                                                   int filter_tag,
                                                   int filter_seek,
                                                   int size)
{
    struct root_menu_video_db_art_slot *slot;
    struct mp3entry id3;
    struct bitmap decoded;
    char path[MAX_PATH];
    size_t decoded_size;
    int i;
    int rc;

    size = MAX(10, MIN(size, IPODJS_DB_ART_MAX));
    for (i = 0; i < IPODJS_DB_ART_CACHE; i++)
    {
        slot = &root_menu_video_db_art_cache[i];
        if (slot->album_seek == album_seek &&
            slot->filter_tag == filter_tag &&
            slot->filter_seek == filter_seek &&
            slot->size == size)
            return slot->valid ? &slot->bm : NULL;
    }

    slot = &root_menu_video_db_art_cache[root_menu_video_db_art_next++ %
                                         IPODJS_DB_ART_CACHE];
    memset(slot, 0, sizeof(*slot));
    slot->album_seek = album_seek;
    slot->filter_tag = filter_tag;
    slot->filter_seek = filter_seek;
    slot->size = size;
    slot->miss = true;

    if (!root_menu_video_db_album_id3(album_seek, filter_tag, filter_seek,
                                      &id3))
        return NULL;

    if (!root_menu_video_find_local_cover(&id3, size, path, sizeof(path)))
        return NULL;

    /*
     * Album-list rows should use the pre-rendered albumlist BMP cache.  The
     * on-device JPEG fallback is too expensive for list scrolling and can
     * produce black decoded buffers on some cover.jpg files.  Keep this
     * fallback limited to already-converted BMP art.
     */
    if (!root_menu_video_path_is_bmp(path))
        return NULL;

    memset(&decoded, 0, sizeof(decoded));
    decoded.width = size;
    decoded.height = size;
    decoded.format = FORMAT_NATIVE;
    decoded.data = root_menu_video_db_art_decode_data;

    rc = read_bmp_file(path, &decoded,
                       sizeof(root_menu_video_db_art_decode_data),
                       FORMAT_NATIVE | FORMAT_RESIZE |
                       FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);

    if (rc < 0 || !root_menu_video_bitmap_has_pixels(&decoded))
        return NULL;

    decoded_size = BM_SIZE(decoded.width, decoded.height,
                           FORMAT_NATIVE, false);
    if (decoded_size > sizeof(slot->data))
        return NULL;

    memcpy(slot->data, decoded.data, decoded_size);
    slot->bm = decoded;
    slot->bm.data = slot->data;

    slot->valid = true;
    slot->miss = false;
    return &slot->bm;
}

static void root_menu_video_draw_album_thumb(const char *album,
                                             const char *artist,
                                             int album_seek, int filter_tag,
                                             int filter_seek, int x, int y,
                                             int size, bool active)
{
    struct bitmap *bm = albumlist_art_get_thumb(album, artist, size);

    if (!bm && artist && artist[0])
        bm = albumlist_art_get_thumb(album, "", size);

    if (!bm)
        bm = root_menu_video_db_album_art(album_seek, filter_tag,
                                          filter_seek, size);

    lcd_set_foreground(active ? LCD_RGBPACK(137, 203, 246) :
                                LCD_RGBPACK(239, 240, 242));
    lcd_fillrect(x - 1, y - 1, size + 2, size + 2);

    if (bm && !root_menu_video_bitmap_has_pixels(bm))
        bm = NULL;

    if (bm)
    {
        lcd_bmp_part(bm, 0, 0, x, y, MIN(size, bm->width),
                     MIN(size, bm->height));
    }
    else
    {
        root_menu_video_gradient(x, y, size, size,
                                 LCD_RGBPACK(242, 243, 245),
                                 LCD_RGBPACK(213, 217, 222));
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(active ? LCD_RGBPACK(245, 250, 255) :
                                    LCD_RGBPACK(150, 156, 164));
        lcd_set_background(LCD_RGBPACK(226, 229, 234));
        root_menu_video_puts_fit(x + 3, y + size / 2 - 6, size - 6,
                                 "Art", true);
    }

    lcd_set_foreground(active ? LCD_RGBPACK(235, 246, 255) :
                                LCD_RGBPACK(176, 180, 186));
    lcd_drawrect(x - 1, y - 1, size + 2, size + 2);
}
#endif /* HAVE_TAGCACHE */
#endif

static struct bitmap *root_menu_video_stock_wps_art(struct mp3entry *id3)
{
#ifdef HAVE_ALBUMART
    struct bitmap *bm = root_menu_video_buffered_art();

    if (!bm)
        bm = root_menu_video_default_art();

    (void)id3;
    return bm;
#else
    (void)id3;
    return NULL;
#endif
}

static void root_menu_video_fade_reflection(int x, int y, int w, int h,
                                            unsigned background)
{
    int yy;

    for (yy = 0; yy < h; yy++)
    {
        int alpha = 192 + yy * 64 / MAX(1, h - 1);
        int xx;

        for (xx = 0; xx < w; xx++)
        {
            fb_data *pixel = FBADDR(x + xx, y + yy);
            unsigned r = (FB_UNPACK_RED(*pixel) * (256 - alpha) +
                          FB_UNPACK_RED(background) * alpha) >> 8;
            unsigned g = (FB_UNPACK_GREEN(*pixel) * (256 - alpha) +
                          FB_UNPACK_GREEN(background) * alpha) >> 8;
            unsigned b = (FB_UNPACK_BLUE(*pixel) * (256 - alpha) +
                          FB_UNPACK_BLUE(background) * alpha) >> 8;

            *pixel = FB_RGBPACK(r, g, b);
        }
    }
}

static bool root_menu_video_draw_stock_wps_art(int x, int y,
                                               struct mp3entry *id3)
{
    struct bitmap *bm = root_menu_video_stock_wps_art(id3);
    int size = IPODJS_STOCK_ART_SIZE;
    int draw_w;
    int draw_h;
    int reflection_h = 30;
    int row;

    if (!bm || !bm->data || bm->width <= 0 || bm->height <= 0)
        return false;

    draw_w = MIN(size, bm->width);
    draw_h = MIN(size, bm->height);
    /* See draw_album_art() in the skin engine: buffered album art is an
     * opaque native bitmap, not a transparent themed asset. */
    lcd_bitmap_part((fb_data *)bm->data, 0, 0,
                    STRIDE(SCREEN_MAIN, bm->width, bm->height),
                    x, y, draw_w, draw_h);

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(70, 75, 84) : LCD_RGBPACK(176, 176, 176));
    lcd_drawrect(x - 1, y - 1, draw_w + 2, draw_h + 2);

    reflection_h = MIN(reflection_h, draw_h);
    for (row = 0; row < reflection_h; row++)
    {
        lcd_bitmap_part((fb_data *)bm->data, 0, draw_h - 1 - row,
                        STRIDE(SCREEN_MAIN, bm->width, bm->height),
                        x, y + draw_h + 2 + row, draw_w, 1);
    }
    root_menu_video_fade_reflection(x, y + draw_h + 2, draw_w,
                                    reflection_h,
                                    root_menu_video_panel());
    return true;
}

static bool root_menu_video_paused_locked(void)
{
    int status = audio_status();

    return button_hold() &&
           (status & AUDIO_STATUS_PLAY) &&
           (status & AUDIO_STATUS_PAUSE);
}

static bool root_menu_video_paused_playback(void)
{
    int status = audio_status();

    return (status & AUDIO_STATUS_PLAY) && (status & AUDIO_STATUS_PAUSE);
}

static void root_menu_video_puts_fit(int x, int y, int width,
                                     const char *text, bool center)
{
    ipodjs_ui_puts_fit(&screens[SCREEN_MAIN], x, y, width, text, center);
}

static struct viewport root_menu_video_wps_title_vp;
static struct {
    bool active;
    int width;
    int height;
    int text_width;
    int gap_width;
    int last_offset;
    long start_tick;
    char text[MAX_PATH];
} root_menu_video_wps_title_scroll;

static void root_menu_video_stop_wps_title_scroll(void)
{
    root_menu_video_wps_title_scroll.active = false;
}

static void root_menu_video_draw_wps_title_offset(int offset)
{
    struct screen *display = &screens[SCREEN_MAIN];
    unsigned canvas = root_menu_video_screen_bg();
    unsigned text = root_menu_video_text();
    int repeat_x = root_menu_video_wps_title_scroll.text_width +
                   root_menu_video_wps_title_scroll.gap_width - offset;

    if (!root_menu_video_wps_title_scroll.active)
        return;

    display->set_viewport(&root_menu_video_wps_title_vp);
    display->set_drawmode(DRMODE_SOLID);
    display->set_foreground(canvas);
    display->fillrect(0, 0, root_menu_video_wps_title_scroll.width,
                      root_menu_video_wps_title_scroll.height);
    display->set_drawmode(DRMODE_FG);
    display->set_foreground(text);
    display->putsxy(-offset, 0, root_menu_video_wps_title_scroll.text);
    display->putsxy(repeat_x, 0, root_menu_video_wps_title_scroll.text);
    display->set_viewport(NULL);
}

static bool root_menu_video_update_wps_title_scroll(void)
{
    static const unsigned char scroll_ticks[18] = {
        100, 80, 64, 50, 40, 32, 25, 20, 16,
        12, 10, 8, 6, 5, 4, 3, 2, 1
    };
    long elapsed;
    int speed;
    int step;
    int cycle;
    int offset;

    if (!root_menu_video_wps_title_scroll.active ||
        TIME_BEFORE(current_tick, root_menu_video_wps_title_scroll.start_tick))
        return false;

    elapsed = current_tick - root_menu_video_wps_title_scroll.start_tick;
    speed = MAX(0, MIN(17, global_settings.scroll_speed));
    step = MAX(1, global_settings.scroll_step);
    cycle = root_menu_video_wps_title_scroll.text_width +
            root_menu_video_wps_title_scroll.gap_width;
    offset = ((elapsed / scroll_ticks[speed]) * step) / MAX(1, cycle);
    offset = ((elapsed / scroll_ticks[speed]) * step) - offset * cycle;
    if (offset == root_menu_video_wps_title_scroll.last_offset)
        return false;

    root_menu_video_wps_title_scroll.last_offset = offset;
    root_menu_video_draw_wps_title_offset(offset);
    lcd_update_rect(root_menu_video_wps_title_vp.x,
                    root_menu_video_wps_title_vp.y,
                    root_menu_video_wps_title_vp.width,
                    root_menu_video_wps_title_vp.height);
    return true;
}

static void root_menu_video_draw_wps_title(int x, int y, int width,
                                           const char *title, int font,
                                           unsigned text, unsigned panel,
                                           bool center)
{
    int text_width;
    int text_height;

    root_menu_video_stop_wps_title_scroll();
    font_getstringsize(title, &text_width, &text_height, font);
    if (text_width <= width)
    {
        lcd_setfont(font);
        lcd_set_foreground(text);
        lcd_set_background(panel);
        root_menu_video_puts_fit(x, y, width, title, center);
        return;
    }

    viewport_set_fullscreen(&root_menu_video_wps_title_vp, SCREEN_MAIN);
    root_menu_video_wps_title_vp.x = x;
    root_menu_video_wps_title_vp.y = y;
    root_menu_video_wps_title_vp.width = width;
    root_menu_video_wps_title_vp.height = text_height;
    root_menu_video_wps_title_vp.font = font;
    root_menu_video_wps_title_vp.fg_pattern = text;
    root_menu_video_wps_title_vp.bg_pattern = root_menu_video_screen_bg();
    root_menu_video_wps_title_scroll.active = true;
    root_menu_video_wps_title_scroll.width = width;
    root_menu_video_wps_title_scroll.height = text_height;
    root_menu_video_wps_title_scroll.text_width = text_width;
    font_getstringsize("   ", &root_menu_video_wps_title_scroll.gap_width,
                       NULL, font);
    root_menu_video_wps_title_scroll.last_offset = 0;
    root_menu_video_wps_title_scroll.start_tick = current_tick +
        global_settings.scroll_delay / MAX(1, 1000 / HZ);
    strmemccpy(root_menu_video_wps_title_scroll.text, title,
               sizeof(root_menu_video_wps_title_scroll.text));
    root_menu_video_draw_wps_title_offset(0);
    ipodjs_trace_screen("Now Playing", "title-scroll", text_width,
                        0, 1, x, y, width, text_height);
}

static void root_menu_video_duration(char *buf, size_t buf_size,
                                     unsigned long ms)
{
    unsigned long seconds = ms / 1000;

    snprintf(buf, buf_size, "%lu:%02lu", seconds / 60, seconds % 60);
}

static void root_menu_video_remaining_duration(char *buf, size_t buf_size,
                                               struct mp3entry *id3)
{
    char duration[16];
    unsigned long remaining = 0;

    if (id3 && id3->length > id3->elapsed)
        remaining = id3->length - id3->elapsed;

    root_menu_video_duration(duration, sizeof(duration), remaining);
    snprintf(buf, buf_size, "-%s", duration);
}

static const char *root_menu_video_now_title(void)
{
    struct mp3entry *id3 = audio_current_track();
    char *name;

    if (id3)
    {
        if (id3->title && id3->title[0])
            return id3->title;

        name = strrchr(id3->path, '/');
        return (name && name[1]) ? name + 1 : id3->path;
    }

    return "Now Playing";
}

static int root_menu_video_elapsed_percent(void)
{
    struct mp3entry *id3 = audio_current_track();

    if (id3 && id3->length > 0)
        return MIN(100, (int)(id3->elapsed * 100 / id3->length));

    return 0;
}

#ifdef HAVE_ALBUMART
static void root_menu_video_bmp_tile(struct bitmap *bm, int x, int y,
                                     int w, int h)
{
    int dx;
    int dy;

    if (!bm || w <= 0 || h <= 0)
        return;

    for (dx = 0; dx < w; dx += bm->width)
    {
        int draw_w = MIN(bm->width, w - dx);
        for (dy = 0; dy < h; dy += bm->height)
        {
            int draw_h = MIN(bm->height, h - dy);
            lcd_bmp_part(bm, 0, 0, x + dx, y + dy, draw_w, draw_h);
        }
    }
}
#endif

static void root_menu_video_draw_meter(int x, int y, int w, int h,
                                       int percent, unsigned fill)
{
    int inner_w = MAX(0, w - 2);
    int inner = inner_w * MAX(0, MIN(100, percent)) / 100;
    bool dark = root_menu_video_dark();
    unsigned border = dark ? LCD_RGBPACK(28, 32, 39) :
        root_menu_video_rgb_blend(255, 255, 255, 0, 0, 0, 45);

    lcd_set_foreground(dark ? LCD_RGBPACK(41, 46, 55) :
                       LCD_RGBPACK(238, 238, 238));
    lcd_fillrect(x, y, w, h);
    if (dark)
        root_menu_video_glass_gradient(x, y, w, h,
                                       LCD_RGBPACK(73, 80, 93),
                                       LCD_RGBPACK(50, 56, 68),
                                       LCD_RGBPACK(30, 35, 43));
    else
        root_menu_video_glass_gradient(x, y, w, h,
                                       LCD_RGBPACK(255, 255, 255),
                                       LCD_RGBPACK(224, 228, 234),
                                       LCD_RGBPACK(196, 202, 210));
    if (inner > 0)
    {
#ifdef HAVE_ALBUMART
        struct bitmap *gloss_blue = root_menu_video_gloss_blue();
        if (gloss_blue)
            root_menu_video_bmp_tile(gloss_blue, x + 1, y + 1, inner,
                                     h - 2);
        else
#endif
        root_menu_video_glass_gradient(x + 1, y + 1, inner, h - 2,
                                       IPODJS_ACTIVE_TOP,
                                       IPODJS_ACTIVE_MID,
                                       fill);
    }
    lcd_set_foreground(border);
    lcd_drawrect(x, y, w, h);
}

static int root_menu_video_volume_percent(void)
{
    int minvol = sound_min(SOUND_VOLUME);
    int maxvol = sound_max(SOUND_VOLUME);
    int percent;

    if (global_status.volume <= minvol)
        return 0;
    if (global_status.volume >= maxvol)
        return 100;

    percent = to_normalized_volume(global_status.volume, minvol, maxvol,
                                   100);
    return MAX(0, MIN(100, percent));
}

static bool root_menu_video_volume_overlay_active(void)
{
    return global_status.last_volume_change &&
           TIME_BEFORE(current_tick, global_status.last_volume_change + HZ * 2);
}

static void root_menu_video_draw_stock_meter(int x, int y, int w, int h,
                                             int percent, unsigned fill)
{
    bool dark = root_menu_video_dark();
    int inner_w = MAX(0, w - 4);
    int filled = inner_w * MAX(0, MIN(100, percent)) / 100;
    unsigned border = dark ? LCD_RGBPACK(78, 84, 94) :
                             LCD_RGBPACK(174, 174, 174);
    unsigned track_top = dark ? LCD_RGBPACK(32, 36, 43) :
                                LCD_RGBPACK(224, 224, 224);
    unsigned track_bottom = dark ? LCD_RGBPACK(55, 61, 70) :
                                   LCD_RGBPACK(252, 252, 252);

    lcd_set_foreground(border);
    lcd_fillrect(x + 2, y, w - 4, h);
    lcd_fillrect(x + 1, y + 1, w - 2, h - 2);
    lcd_fillrect(x, y + 2, w, h - 4);

    root_menu_video_gradient(x + 2, y + 2, w - 4, h - 4,
                             track_top, track_bottom);
    if (filled > 0)
    {
        int fill_w = MIN(inner_w, MAX(2, filled));
        unsigned fill_top = root_menu_video_rgb_blend(
            FB_UNPACK_RED(fill), FB_UNPACK_GREEN(fill),
            FB_UNPACK_BLUE(fill), 255, 255, 255, 104);

        root_menu_video_gradient(x + 2, y + 2, fill_w, h - 4,
                                 fill_top, fill);
    }
}

static bool root_menu_video_draw_apple_track(int frame_x, int frame_y,
                                             int percent, int frame_asset)
{
    struct bitmap *frame = root_menu_video_apple_slider_asset(frame_asset);
    bool progress = frame_asset == IPODJS_APPLE_PROGRESS_FRAME;
    struct bitmap *fill = root_menu_video_apple_slider_asset(progress ?
        IPODJS_APPLE_PROGRESS_FILL : IPODJS_APPLE_SLIDER_FILL);
    struct bitmap *fill_cap = progress ?
        root_menu_video_apple_slider_asset(
            IPODJS_APPLE_PROGRESS_FILL_CAP) : NULL;
    int old_drawmode;
    int filled;

    if (!frame || !fill || (progress && !fill_cap))
        return false;

    /* Apple's frame and fill carry antialiased alpha at both rounded ends.
     * Blend against the pixels already in the WPS/slider surface; SOLID mode
     * blends transparent pixels against the viewport background color and
     * can leave a square dark box around the left cap. */
    old_drawmode = lcd_get_drawmode();
    lcd_set_drawmode(DRMODE_FG);
    percent = MAX(0, MIN(100, percent));
    filled = (frame->width - 6) * percent / 100;
    lcd_bmp(frame, frame_x, frame_y);
    if (filled > 0)
    {
        if (progress)
        {
            int draw_w = percent == 100 ? frame->width : filled + 3;
            int cap_w = fill_cap->width;

            lcd_bmp_part(fill, 0, 0, frame_x, frame_y,
                         draw_w, frame->height);
            if (draw_w >= cap_w)
            {
                lcd_bmp(fill_cap, frame_x + draw_w - cap_w, frame_y + 3);
            }
        }
        else
        {
            lcd_bmp_part(fill, 2, 2, frame_x + 3, frame_y + 3,
                         filled, frame->height - 6);
        }

        /* The fill is clipped from Apple's continuous blue strip. Reapply
         * the exact Apple frame edge pixels afterward so the strip cannot
         * square off or overwrite the antialiased rounded ends. */
        lcd_bmp_part(frame, 0, 0, frame_x, frame_y,
                     frame->width, 3);
        lcd_bmp_part(frame, 0, frame->height - 3,
                     frame_x, frame_y + frame->height - 3,
                     frame->width, 3);
        lcd_bmp_part(frame, 0, 3, frame_x, frame_y + 3,
                     4, frame->height - 6);
        lcd_bmp_part(frame, frame->width - 4, 3,
                     frame_x + frame->width - 4, frame_y + 3,
                     4, frame->height - 6);
    }
    lcd_set_drawmode(old_drawmode);
    return true;
}

static bool root_menu_video_draw_apple_slider(int frame_x, int frame_y,
                                              int percent, bool brightness)
{
    struct bitmap *low = root_menu_video_apple_slider_asset(
        brightness ? IPODJS_APPLE_BRIGHTNESS_LOW :
                     IPODJS_APPLE_VOLUME_LOW);
    struct bitmap *high = root_menu_video_apple_slider_asset(
        brightness ? IPODJS_APPLE_BRIGHTNESS_HIGH :
                     IPODJS_APPLE_VOLUME_HIGH);
    int frame_asset = root_menu_video_dark() ? IPODJS_APPLE_SLIDER_DARK :
                                               IPODJS_APPLE_SLIDER_LIGHT;
    struct bitmap *frame = root_menu_video_apple_slider_asset(frame_asset);

    if (!low || !high || !frame ||
        !root_menu_video_draw_apple_track(frame_x, frame_y, percent,
                                          frame_asset))
        return false;

    lcd_bmp(low, frame_x - 10 - low->width,
            frame_y + (frame->height - low->height) / 2);
    lcd_bmp(high, frame_x + frame->width + 10,
            frame_y + (frame->height - high->height) / 2);
    return true;
}

static void root_menu_video_draw_volume_overlay(void)
{
    int percent;

    if (!root_menu_video_volume_overlay_active())
        return;

    percent = root_menu_video_volume_percent();
    lcd_set_foreground(root_menu_video_panel());
    lcd_fillrect(0, 196, LCD_WIDTH, 28);
    if (!root_menu_video_draw_apple_track(60, 198, percent,
                                          IPODJS_APPLE_PROGRESS_FRAME))
    {
        root_menu_video_draw_stock_meter(58, 201, 204, 13, percent,
                                         root_menu_video_accent());
    }
    else
    {
        struct bitmap *low = root_menu_video_apple_slider_asset(
            IPODJS_APPLE_VOLUME_LOW);
        struct bitmap *high = root_menu_video_apple_slider_asset(
            IPODJS_APPLE_VOLUME_HIGH);

        if (low)
            lcd_bmp(low, 34, 200);
        if (high)
            lcd_bmp(high, 268, 200);
    }
}

struct root_menu_video_preview_slot {
    bool valid;
    enum root_menu_video_preview_source source;
    int index;
    unsigned gen;
    char path[MAX_PATH];
    struct bitmap bm;
    unsigned char data[BM_SIZE(IPODJS_PREVIEW_IMAGE_WIDTH,
                               IPODJS_PREVIEW_IMAGE_HEIGHT,
                               FORMAT_NATIVE, false)];
};

struct root_menu_video_preview_source_cache {
    bool loaded;
    long next_reload;
    int path_count;
};

struct root_menu_video_preview_failure {
    bool valid;
    enum root_menu_video_preview_source source;
    int index;
    long retry_after;
    char path[MAX_PATH];
};

static enum root_menu_video_preview_source root_menu_video_preview_loaded;
static int root_menu_video_preview_path_count;
static char root_menu_video_preview_paths[IPODJS_PREVIEW_MAX_ITEMS][MAX_PATH];
static int root_menu_video_preview_victim;

/* Photos previews come from a RockPod-written index of every synced photo
 * rather than a device crawl.  Only line offsets are stored; the path for a
 * slide is re-read from the index inside the idle service, never a draw. */
#define IPODJS_PHOTO_INDEX_MAX 1024
#define IPODJS_PHOTO_LOCKS_BUF 4096
static char root_menu_video_photo_preview_root[MAX_PATH];
static char root_menu_video_photo_root[MAX_PATH];
static uint32_t root_menu_video_photo_index_offsets[IPODJS_PHOTO_INDEX_MAX];
static int root_menu_video_photo_index_count;
static bool root_menu_video_photo_index_active;
static unsigned root_menu_video_photo_index_generation;
static struct slideshow_order root_menu_video_photo_order;
static long root_menu_video_preview_decode_window_tick;
static int root_menu_video_preview_decode_budget;
static enum root_menu_video_preview_source
    root_menu_video_preview_last_drawn_source;
static int root_menu_video_preview_last_drawn_index = -1;
static struct root_menu_video_preview_source_cache
    root_menu_video_preview_source_caches[IPODJS_PREVIEW_AVATAR + 1];
static struct root_menu_video_preview_failure
    root_menu_video_preview_failures[8];
struct root_menu_video_menu_preview_slot {
    bool valid;
    unsigned long stamp;
    int dark;
    char path[MAX_PATH];
    struct bitmap bm;
    unsigned char data[BM_SIZE(174, LCD_HEIGHT, FORMAT_NATIVE, false)];
};

static unsigned long root_menu_video_menu_preview_stamp;

struct root_menu_video_menu_preview_failure {
    bool valid;
    int dark;
    long retry_after;
    char path[MAX_PATH];
};

static struct root_menu_video_menu_preview_failure
    root_menu_video_menu_preview_failures[8];

union root_menu_video_preview_storage {
    struct root_menu_video_preview_slot
        slideshow[IPODJS_PREVIEW_IMAGE_CACHE];
    struct root_menu_video_menu_preview_slot
        menu[IPODJS_MENU_PREVIEW_CACHE];
};

enum root_menu_video_preview_cache_mode {
    IPODJS_PREVIEW_CACHE_NONE = 0,
    IPODJS_PREVIEW_CACHE_SLIDESHOW,
    IPODJS_PREVIEW_CACHE_MENU,
};

static union root_menu_video_preview_storage root_menu_video_preview_storage;
static enum root_menu_video_preview_cache_mode
    root_menu_video_preview_cache_mode;

#define root_menu_video_preview_slots \
    root_menu_video_preview_storage.slideshow
#define root_menu_video_menu_preview_slots \
    root_menu_video_preview_storage.menu

static void root_menu_video_preview_use_cache(
    enum root_menu_video_preview_cache_mode mode)
{
    if (root_menu_video_preview_cache_mode == mode)
        return;

    root_menu_video_preview_cache_mode = mode;
    if (mode == IPODJS_PREVIEW_CACHE_SLIDESHOW)
    {
        for (int i = 0; i < IPODJS_PREVIEW_IMAGE_CACHE; i++)
            root_menu_video_preview_slots[i].valid = false;
        root_menu_video_preview_victim = 0;
    }
    else if (mode == IPODJS_PREVIEW_CACHE_MENU)
    {
        for (int i = 0; i < IPODJS_MENU_PREVIEW_CACHE; i++)
            root_menu_video_menu_preview_slots[i].valid = false;
        root_menu_video_menu_preview_stamp = 0;
    }
}

static const char *root_menu_video_preview_asset_name(const char *title)
{
    if (!title || !title[0])
        return "menu";
    if (!strcmp(title, "Cover Flow"))
        return "cover-flow";
    if (!strcmp(title, "Now Playing"))
        return "now-playing";
    if (!strcmp(title, "PokeMini"))
        return "pokemini";
    if (!strcmp(title, "Browse PokeMini ROMs"))
        return "pokemini";
    if (!strcmp(title, "Stick RPG"))
        return "stickrpg";
    if (!strcmp(title, "RuneScape Classic") ||
        !strcmp(title, "Club Penguin"))
        return "games";
    if (!strcmp(title, "Clock"))
        return "clock";
    if (!strcmp(title, "Desktop Mode"))
        return "desktop-mode";
    if (!strcmp(title, "DIRECTV"))
        return "livetv";
    if (!strcmp(title, "Applications"))
        return "applications";
    if (!strcmp(title, "Game Cover Flow"))
        return "games";
    if (!strcmp(title, "Browse ROM Files"))
        return "games";
    if (!strcmp(title, "Sega Master System / Game Gear"))
        return "games";
    if (!strncmp(title, "Toggle NES", 10))
        return "games";
    if (!strcmp(title, "Clear NES Saves"))
        return "games";
    if (!strcmp(title, "Music"))
        return "music";
    if (!strcmp(title, "Videos") || !strcmp(title, "Netflix"))
        return "videos";
    if (!strcmp(title, "Photos"))
        return "photos";
    if (!strcmp(title, "Games"))
        return "games";
    if (!strcmp(title, "Files"))
        return "files";
    if (!strcmp(title, "Playlists"))
        return "playlists";
    if (!strcmp(title, "Plugins"))
        return "plugins";
    if (!strcmp(title, "Settings"))
        return "settings";
    if (!strcmp(title, "System"))
        return "system";
    if (!strcmp(title, "Shortcuts"))
        return "shortcuts";
    if (!strcmp(title, "Radio"))
        return "radio";
    if (!strcmp(title, "Extras"))
        return "extras";
    if (!strcmp(title, "Weather"))
        return "weather";
    if (!strcmp(title, "Maps"))
        return "maps";
    if (!strcmp(title, "Database"))
        return "database";
    return "menu";
}

static enum root_menu_video_preview_source
root_menu_video_preview_source_for_title(const char *title)
{
    if (!title || !title[0])
        return IPODJS_PREVIEW_NONE;
    if (!strcmp(title, "PokeMini") ||
        !strcmp(title, "Browse PokeMini ROMs"))
        return IPODJS_PREVIEW_POKEMINI;
    if (!strcmp(title, "Games") ||
        !strcmp(title, "RuneScape Classic") ||
        !strcmp(title, "Stick RPG") ||
        !strcmp(title, "Club Penguin") ||
        !strcmp(title, "Sega Master System / Game Gear") ||
        !strcmp(title, "Game Cover Flow") ||
        !strcmp(title, "Browse ROM Files"))
        return IPODJS_PREVIEW_GAMES;
    if (!strcmp(title, "Videos") || !strcmp(title, "Netflix"))
        return IPODJS_PREVIEW_VIDEOS;
    if (!strcmp(title, "Photos"))
        return IPODJS_PREVIEW_PHOTOS;
    if (!strcmp(title, "Magazines"))
        return IPODJS_PREVIEW_MAGAZINES;
    if (!strcmp(title, "Comics"))
        return IPODJS_PREVIEW_COMICS;
    if (!strcmp(title, "Music") || !strcmp(title, "Cover Flow") ||
        !strcmp(title, "Now Playing"))
        return IPODJS_PREVIEW_MUSIC;
    if (!strcmp(title, "Achievements"))
        return IPODJS_PREVIEW_AVATAR;
    if (!strcmp(title, "Extras") &&
        global_settings.ui_engine_extras_pane == UI_ENGINE_EXTRAS_AVATAR)
        return IPODJS_PREVIEW_AVATAR;
    return IPODJS_PREVIEW_NONE;
}

static bool root_menu_video_preview_asset_top_aligned(const char *title)
{
    static const char * const top_aligned[] = {
        "Clock", "Applications", "PokeMini", "Files", "Playlists",
        "Plugins", "Shortcuts", "System", "Extras", "DIRECTV",
    };

    if (!title)
        return false;

    for (int i = 0; i < (int)ARRAYLEN(top_aligned); i++)
    {
        if (!strcmp(title, top_aligned[i]))
            return true;
    }

    return false;
}

static const char *root_menu_video_menu_preview_path(const char *title)
{
    if (title && !strcmp(title, "Desktop Mode"))
        return IPODJS_DESKTOP_PREVIEW;
    if (title && !strcmp(title, "DIRECTV"))
        return IPODJS_LIVETV_PREVIEW;
    if (title && !strcmp(title, "Calm"))
        return IPODJS_ASSET_DIR "/calm/calm-icon.64x64x24.bmp";
    return NULL;
}

static bool root_menu_video_preview_asset_is_verified(const char *title)
{
    /*
     * The bundled menu-preview bitmaps are illustrative placeholders rather
     * than captures from the stock firmware. Desktop Mode comes from the
     * checksum-verified, user-owned Snow Leopard pack; Live TV uses its
     * commissioned photographic broadcast pane.
     */
    return root_menu_video_menu_preview_path(title) != NULL;
}

static bool root_menu_video_menu_preview_recent_failure(const char *path,
                                                        int dark)
{
    if (!path || !path[0])
        return false;

    for (int i = 0; i < (int)ARRAYLEN(root_menu_video_menu_preview_failures); i++)
    {
        struct root_menu_video_menu_preview_failure *failure =
            &root_menu_video_menu_preview_failures[i];

        if (!failure->valid || failure->dark != dark ||
            strcmp(failure->path, path))
            continue;

        if (!TIME_AFTER(current_tick, failure->retry_after))
            return true;

        failure->valid = false;
        return false;
    }

    return false;
}

static void root_menu_video_menu_preview_record_failure(const char *path,
                                                        int dark)
{
    int victim = 0;
    long oldest = root_menu_video_menu_preview_failures[0].retry_after;

    if (!path || !path[0])
        return;

    for (int i = 0; i < (int)ARRAYLEN(root_menu_video_menu_preview_failures); i++)
    {
        struct root_menu_video_menu_preview_failure *failure =
            &root_menu_video_menu_preview_failures[i];

        if (!failure->valid ||
            (failure->dark == dark && !strcmp(failure->path, path)))
        {
            victim = i;
            break;
        }

        if (TIME_BEFORE(failure->retry_after, oldest))
        {
            oldest = failure->retry_after;
            victim = i;
        }
    }

    struct root_menu_video_menu_preview_failure *failure =
        &root_menu_video_menu_preview_failures[victim];
    failure->valid = true;
    failure->dark = dark;
    failure->retry_after = current_tick + HZ * 60;
    strmemccpy(failure->path, path, sizeof(failure->path));
}

static void root_menu_video_menu_preview_clear_failure(const char *path,
                                                       int dark)
{
    if (!path || !path[0])
        return;

    for (int i = 0; i < (int)ARRAYLEN(root_menu_video_menu_preview_failures); i++)
    {
        struct root_menu_video_menu_preview_failure *failure =
            &root_menu_video_menu_preview_failures[i];

        if (failure->valid && failure->dark == dark &&
            !strcmp(failure->path, path))
        {
            failure->valid = false;
            return;
        }
    }
}

static struct root_menu_video_menu_preview_slot *
root_menu_video_menu_preview_find(const char *path, int dark)
{
    root_menu_video_preview_use_cache(IPODJS_PREVIEW_CACHE_MENU);

    if (!path || !path[0])
        return NULL;

    for (int i = 0; i < (int)ARRAYLEN(root_menu_video_menu_preview_slots); i++)
    {
        struct root_menu_video_menu_preview_slot *slot =
            &root_menu_video_menu_preview_slots[i];

        if (slot->valid && slot->dark == dark && !strcmp(slot->path, path))
            return slot;
    }

    return NULL;
}

static struct root_menu_video_menu_preview_slot *
root_menu_video_menu_preview_victim(void)
{
    int victim = 0;
    unsigned long oldest = root_menu_video_menu_preview_slots[0].stamp;

    for (int i = 0; i < (int)ARRAYLEN(root_menu_video_menu_preview_slots); i++)
    {
        struct root_menu_video_menu_preview_slot *slot =
            &root_menu_video_menu_preview_slots[i];

        if (!slot->valid)
            return slot;

        if (slot->stamp < oldest)
        {
            oldest = slot->stamp;
            victim = i;
        }
    }

    return &root_menu_video_menu_preview_slots[victim];
}

static void root_menu_video_draw_menu_preview_slot(
    struct root_menu_video_menu_preview_slot *slot,
    int x, int y, int w, int h, bool top_aligned)
{
    slot->stamp = ++root_menu_video_menu_preview_stamp;
    lcd_bmp_part(&slot->bm, 0, 0, x,
                 top_aligned ? y : y + MAX(0, h - slot->bm.height),
                 MIN(w, slot->bm.width), MIN(h, slot->bm.height));
}

static bool root_menu_video_draw_menu_preview_asset_cached(
    const char *title, int x, int y, int w, int h)
{
    const char *load_path;
    int dark = root_menu_video_dark() ? 1 : 0;
    struct root_menu_video_menu_preview_slot *slot;

    if (!root_menu_video_preview_asset_is_verified(title))
        return false;
    load_path = root_menu_video_menu_preview_path(title);
    slot = root_menu_video_menu_preview_find(load_path, dark);
    if (!slot)
        return false;

    root_menu_video_draw_menu_preview_slot(slot, x, y, w, h, false);
    return true;
}

static bool root_menu_video_animated_preview_service(const char *title);
static bool root_menu_video_custom_preview_animation_due(
    const char *title, long *next_tick);
static void root_menu_video_draw_weather_preview(int x, int y, int w, int h);
static void root_menu_video_draw_calm_preview(int x, int y, int w, int h);
static void root_menu_video_draw_maps_preview(int x, int y, int w, int h);

/* Idle-service only: all file checks and decoding stay out of draw paths. */
static bool root_menu_video_menu_preview_service(const char *title)
{
    const char *load_path;
    int dark = root_menu_video_dark() ? 1 : 0;
    struct root_menu_video_menu_preview_slot *slot;
    int rc;

    if (root_menu_video_animated_preview_service(title))
        return true;

    if (!root_menu_video_preview_asset_is_verified(title))
        return false;
    load_path = root_menu_video_menu_preview_path(title);
    slot = root_menu_video_menu_preview_find(load_path, dark);
    if (slot)
        return false;
    if (root_menu_video_menu_preview_recent_failure(load_path, dark))
        return false;
    if (!file_exists(load_path))
    {
        root_menu_video_menu_preview_record_failure(load_path, dark);
        return false;
    }

    slot = root_menu_video_menu_preview_victim();
    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->valid = false;
    slot->path[0] = '\0';
    slot->dark = dark;
    slot->bm.width = title && !strcmp(title, "Calm") ? 64 : 174;
    slot->bm.height = title && !strcmp(title, "Calm") ? 64 :
        (root_menu_video_preview_asset_top_aligned(title) ?
         LCD_HEIGHT : 220);
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;
    rc = read_bmp_file(load_path, &slot->bm, sizeof(slot->data),
                       FORMAT_NATIVE | FORMAT_TRANSPARENT, NULL);
    if (rc < 0)
    {
        root_menu_video_menu_preview_record_failure(load_path, dark);
        return false;
    }

    strmemccpy(slot->path, load_path, sizeof(slot->path));
    slot->valid = true;
    root_menu_video_menu_preview_clear_failure(load_path, dark);
    return true;
}

static enum root_menu_video_preview_source
root_menu_video_preview_source_for_item(const struct menu_item_ex *item)
{
    const char *title = root_menu_video_preview(item);

    if (root_menu_video_item_is_settings(item))
        return IPODJS_PREVIEW_NONE;
    if (root_menu_video_item_is_games(item))
        return IPODJS_PREVIEW_GAMES;
    if (root_menu_video_item_is_videos(item))
        return IPODJS_PREVIEW_VIDEOS;
    return root_menu_video_preview_source_for_title(title);
}

static bool root_menu_video_preview_uses_slideshow(
    enum root_menu_video_preview_source source)
{
    return source != IPODJS_PREVIEW_NONE;
}

static bool root_menu_video_preview_recent_failure(
    enum root_menu_video_preview_source source, int index, const char *path)
{
    for (int i = 0; i < (int)ARRAYLEN(root_menu_video_preview_failures); i++)
    {
        struct root_menu_video_preview_failure *failure =
            &root_menu_video_preview_failures[i];

        if (!failure->valid ||
            failure->source != source ||
            failure->index != index ||
            strcmp(failure->path, path) != 0)
        {
            continue;
        }

        if (!TIME_AFTER(current_tick, failure->retry_after))
            return true;

        failure->valid = false;
        return false;
    }

    return false;
}

static void root_menu_video_preview_record_failure(
    enum root_menu_video_preview_source source, int index, const char *path)
{
    int victim = 0;
    long oldest = root_menu_video_preview_failures[0].retry_after;

    for (int i = 0; i < (int)ARRAYLEN(root_menu_video_preview_failures); i++)
    {
        struct root_menu_video_preview_failure *failure =
            &root_menu_video_preview_failures[i];

        if (!failure->valid ||
            (failure->source == source &&
             failure->index == index &&
             strcmp(failure->path, path) == 0))
        {
            victim = i;
            break;
        }

        if (TIME_BEFORE(failure->retry_after, oldest))
        {
            oldest = failure->retry_after;
            victim = i;
        }
    }

    struct root_menu_video_preview_failure *failure =
        &root_menu_video_preview_failures[victim];
    failure->valid = true;
    failure->source = source;
    failure->index = index;
    failure->retry_after = current_tick + HZ * 15;
    strmemccpy(failure->path, path, sizeof(failure->path));
}

static void root_menu_video_preview_clear_failure(
    enum root_menu_video_preview_source source, int index, const char *path)
{
    for (int i = 0; i < (int)ARRAYLEN(root_menu_video_preview_failures); i++)
    {
        struct root_menu_video_preview_failure *failure =
            &root_menu_video_preview_failures[i];

        if (failure->valid &&
            failure->source == source &&
            failure->index == index &&
            strcmp(failure->path, path) == 0)
        {
            failure->valid = false;
            return;
        }
    }
}

static DIR *root_menu_video_preview_opendir(
    enum root_menu_video_preview_source source, const char *path)
{
    DIR *dir;

    if (!path || !path[0])
        return NULL;

    if (root_menu_video_preview_recent_failure(source, -1, path))
        return NULL;

    dir = opendir(path);
    if (!dir)
    {
        root_menu_video_preview_record_failure(source, -1, path);
        return NULL;
    }

    root_menu_video_preview_clear_failure(source, -1, path);
    return dir;
}

static bool root_menu_video_preview_decode_permitted(void)
{
    if (button_hold())
        return false;

    if (root_menu_video_hold_storm_active())
        return false;

    if (TIME_AFTER(current_tick,
                   root_menu_video_preview_decode_window_tick +
                   IPODJS_PREVIEW_DECODE_INTERVAL) ||
        root_menu_video_preview_decode_window_tick == 0)
    {
        root_menu_video_preview_decode_window_tick = current_tick;
        root_menu_video_preview_decode_budget = IPODJS_PREVIEW_DECODE_BURST;
    }

    if (root_menu_video_preview_decode_budget <= 0)
        return false;

    root_menu_video_preview_decode_budget--;
    return true;
}

static void root_menu_video_preview_invalidate_source_cache(
    enum root_menu_video_preview_source source)
{
    if (source <= IPODJS_PREVIEW_NONE ||
        source > IPODJS_PREVIEW_AVATAR)
    {
        return;
    }

    root_menu_video_preview_source_caches[source].loaded = false;
    root_menu_video_preview_source_caches[source].next_reload = 0;
    if (root_menu_video_preview_loaded == source)
        root_menu_video_preview_loaded = IPODJS_PREVIEW_NONE;
}

static void root_menu_video_preview_reset_slots(void)
{
    root_menu_video_preview_use_cache(IPODJS_PREVIEW_CACHE_SLIDESHOW);
    for (int i = 0; i < IPODJS_PREVIEW_IMAGE_CACHE; i++)
        root_menu_video_preview_slots[i].valid = false;
    root_menu_video_preview_victim = 0;
}

static void root_menu_video_preview_add_path(const char *path)
{
    if (!path || !path[0] ||
        root_menu_video_preview_path_count >= IPODJS_PREVIEW_MAX_ITEMS)
        return;

    for (int i = 0; i < root_menu_video_preview_path_count; i++)
    {
        if (!strcmp(root_menu_video_preview_paths[i], path))
            return;
    }

    strmemccpy(root_menu_video_preview_paths[root_menu_video_preview_path_count],
               path, sizeof(root_menu_video_preview_paths[0]));
    root_menu_video_preview_path_count++;
}

static void root_menu_video_preview_load_video_paths(void)
{
    int fd = open(VIDEO_LIST_INDEX, O_RDONLY);
    bool manifest_available = fd >= 0;
    char line[512];
    DIR *dir;
    struct dirent *entry;

    while (fd >= 0 &&
           root_menu_video_preview_path_count < IPODJS_PREVIEW_MAX_ITEMS &&
           read_line(fd, line, sizeof(line)) > 0)
    {
        char parsed[512];
        char *fields12[12];
        char *fields7[7];
        char *fields6[6];
        const char *art_path = "";

        video_trim_line(line);
        if (line[0] == '#' || line[0] == '\0' ||
            !strncmp(line, "video_id\t", 9))
            continue;

        if (video_parse_manifest_line(line, parsed, sizeof(parsed), fields12))
        {
            if (video_manifest_entry_locked(fields12))
                continue;
            art_path = fields12[2][0] ? fields12[2] : fields12[1];
        }
        else
        {
            strmemccpy(parsed, line, sizeof(parsed));
            if (video_split_tsv(parsed, fields7, 7))
                art_path = fields7[2][0] ? fields7[2] : fields7[1];
            else
            {
                strmemccpy(parsed, line, sizeof(parsed));
                if (!video_split_tsv(parsed, fields6, 6))
                    continue;
                art_path = fields6[1];
            }
        }

        if (!art_path[0])
            continue;

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", VIDEO_LIST_ROOT, art_path);
        if (file_exists(path))
            root_menu_video_preview_add_path(path);
    }

    if (fd >= 0)
        close(fd);

    /* With a manifest present, only explicitly public preview entries are
     * eligible.  Scanning the cache directory would reveal locked artwork. */
    if (manifest_available)
        return;

    bool scanning_previews = true;
    dir = root_menu_video_preview_opendir(IPODJS_PREVIEW_VIDEOS,
                                          VIDEO_LIST_ROOT "/previews");
    if (!dir)
    {
        scanning_previews = false;
        dir = root_menu_video_preview_opendir(IPODJS_PREVIEW_VIDEOS,
                                              VIDEO_LIST_ROOT "/thumbs");
    }
    if (!dir)
        return;

    while (root_menu_video_preview_path_count < IPODJS_PREVIEW_MAX_ITEMS &&
           (entry = readdir(dir)) != NULL)
    {
        const char *ext;
        char path[MAX_PATH];

        ext = strrchr(entry->d_name, '.');
        if (!ext || strcasecmp(ext, ".bmp"))
            continue;

        snprintf(path, sizeof(path), "%s/%s/%s", VIDEO_LIST_ROOT,
                 scanning_previews ? "previews" : "thumbs",
                 entry->d_name);
        if (file_exists(path))
            root_menu_video_preview_add_path(path);
    }

    closedir(dir);
}

static bool root_menu_video_preview_pane_variant(const char *path,
                                                 char *out,
                                                 size_t out_size)
{
    const char *ext;
    size_t base_len;

    if (!path || !path[0] || !out || out_size == 0)
        return false;

    ext = strrchr(path, '.');
    if (!ext)
        return false;

    base_len = ext - path;
    if (base_len + sizeof(".pane.bmp") > out_size)
        return false;

    memcpy(out, path, base_len);
    snprintf(out + base_len, out_size - base_len, ".pane.bmp");
    return file_exists(out);
}

static bool root_menu_video_preview_path_has_previews(const char *path)
{
    char preview_root[MAX_PATH];

    snprintf(preview_root, sizeof(preview_root), "%s/.photo_previews", path);
    return file_exists(preview_root);
}

static int root_menu_video_preview_count_photo_previews(const char *path, int depth);

static void root_menu_video_preview_scan_better_root(const char *base, int depth,
                                                     bool *found_root,
                                                     char *best_root,
                                                     int *best_count)
{
    DIR *dir;
    struct dirent *entry;
    struct dirinfo info;
    bool is_root = (base[0] == '/' && base[1] == '\0');
    char candidate[MAX_PATH];
    char preview_root[MAX_PATH];
    int count;

    /* Fallback discovery only: stay near the volume root.  A deep recursive
     * crawl spins storage for seconds at hover time on hardware. */
    if (!base || !base[0] || depth > 2)
        return;

    dir = opendir(base);
    if (!dir)
        return;

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (is_root)
            snprintf(candidate, sizeof(candidate), "/%s", entry->d_name);
        else
            snprintf(candidate, sizeof(candidate), "%s/%s", base, entry->d_name);

        info = dir_get_info(dir, entry);
        if (!(info.attribute & ATTR_DIRECTORY))
            continue;

        if (root_menu_video_preview_path_has_previews(candidate))
        {
            snprintf(preview_root, sizeof(preview_root), "%s/.photo_previews",
                     candidate);
            count = root_menu_video_preview_count_photo_previews(preview_root, 0);
            if (!*found_root || count > *best_count)
            {
                snprintf(best_root, MAX_PATH, "%s", candidate);
                *best_count = count;
                *found_root = true;
            }
        }

        root_menu_video_preview_scan_better_root(candidate, depth + 1, found_root,
                                                best_root, best_count);
    }

    closedir(dir);
}

static int root_menu_video_preview_count_photo_previews(const char *path, int depth)
{
    DIR *dir;
    struct dirent *entry;
    struct dirinfo info;
    int count = 0;
    char child[MAX_PATH];
    const char *ext;

    if (!path || !path[0] || depth > 6)
        return 0;

    dir = opendir(path);
    if (!dir)
        return 0;

    while ((entry = readdir(dir)) != NULL)
    {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        info = dir_get_info(dir, entry);
        if (info.attribute & ATTR_DIRECTORY)
        {
            count += root_menu_video_preview_count_photo_previews(child, depth + 1);
            continue;
        }

        ext = strrchr(entry->d_name, '.');
        if (ext && !strcasecmp(ext, ".bmp"))
            count++;
    }

    closedir(dir);
    return count;
}

static bool root_menu_video_preview_resolve_photo_root(char *path, size_t path_size)
{
    bool found_best_root = false;
    int best_count = 0;
    char best_root[MAX_PATH];

    /* The RockPod layout keeps previews under /Photos.  When that exists,
     * use it directly instead of surveying the device for a better root. */
    if (root_menu_video_preview_path_has_previews("/Photos"))
    {
        snprintf(path, path_size, "/Photos");
        return true;
    }

    root_menu_video_preview_scan_better_root("/", 1, &found_best_root, best_root,
                                            &best_count);

    if (found_best_root)
    {
        snprintf(path, path_size, "%s", best_root);
        return true;
    }

    return false;
}

static void root_menu_video_preview_scan_photo_previews(const char *path,
                                                        const char *preview_root,
                                                        const char *photo_root,
                                                        int depth)
{
    DIR *dir;
    struct dirent *entry;

    if (depth > 6 ||
        root_menu_video_preview_path_count >= IPODJS_PREVIEW_MAX_ITEMS)
        return;

    dir = root_menu_video_preview_opendir(IPODJS_PREVIEW_PHOTOS, path);
    if (!dir)
        return;

    while (root_menu_video_preview_path_count < IPODJS_PREVIEW_MAX_ITEMS &&
           (entry = readdir(dir)) != NULL)
    {
        struct dirinfo info;
        const char *ext;
        char child[MAX_PATH];

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        info = dir_get_info(dir, entry);
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (info.attribute & ATTR_DIRECTORY)
        {
            root_menu_video_preview_scan_photo_previews(child,
                                                       preview_root,
                                                       photo_root,
                                                       depth + 1);
            continue;
        }

        ext = strrchr(entry->d_name, '.');
        if (ext && !strcasecmp(ext, ".bmp"))
        {
            const size_t prefix_len = strlen(preview_root);
            const char *relative = child + prefix_len + 1;
            size_t relative_len;
            char photo_path[MAX_PATH];

            if (strncmp(child, preview_root, prefix_len) ||
                child[prefix_len] != '/')
                continue;

            relative_len = strlen(relative);
            if (relative_len > 4 &&
                snprintf(photo_path, sizeof(photo_path), "%s/%.*s",
                         photo_root, (int)relative_len - 4, relative) <
                    (int)sizeof(photo_path) &&
                file_exists(photo_path))
            {
                root_menu_video_preview_add_path(child);
            }
        }
    }

    closedir(dir);
}

/* relative: preview path relative to .photo_previews/, including ".bmp". */
static bool root_menu_video_photo_relative_locked(const char *relative,
                                                  const char *lock_path)
{
    size_t relative_len;
    size_t lock_len;

    if (!relative || !lock_path || !lock_path[0])
        return false;

    relative_len = strlen(relative);
    if (relative_len <= 4 || strcasecmp(relative + relative_len - 4, ".bmp"))
        return false;

    relative_len -= 4;
    lock_len = strlen(lock_path);
    if (relative_len < lock_len || strncmp(relative, lock_path, lock_len))
        return false;

    return relative_len == lock_len || relative[lock_len] == '/';
}

static bool root_menu_video_photo_preview_is_locked(const char *path,
                                                    const char *lock_path,
                                                    const char *photo_root)
{
    char prefix[MAX_PATH];

    if (!photo_root || !photo_root[0])
        return false;

    snprintf(prefix, sizeof(prefix), "%s/.photo_previews/", photo_root);
    if (!path || strncmp(path, prefix, strlen(prefix)))
        return false;

    return root_menu_video_photo_relative_locked(path + strlen(prefix),
                                                 lock_path);
}

static void root_menu_video_preview_filter_locked_photos(const char *photo_root)
{
    char line[MAX_PATH + 16];
    int fd;

    if (!file_exists(IPODJS_PHOTOS_LOCKS))
        return;

    fd = open(IPODJS_PHOTOS_LOCKS, O_RDONLY);
    if (fd < 0)
    {
        /* A lock database that cannot be read must not leak private images. */
        root_menu_video_preview_path_count = 0;
        return;
    }

    while (root_menu_video_preview_path_count > 0 &&
           read_line(fd, line, sizeof(line)) > 0)
    {
        char *separator;
        int kept = 0;

        video_trim_line(line);
        separator = strrchr(line, '|');
        if (!separator || separator == line)
            continue;
        *separator = '\0';

        for (int i = 0; i < root_menu_video_preview_path_count; i++)
        {
            if (root_menu_video_photo_preview_is_locked(
                    root_menu_video_preview_paths[i], line, photo_root))
                continue;

            if (kept != i)
            {
                strmemccpy(root_menu_video_preview_paths[kept],
                           root_menu_video_preview_paths[i],
                           sizeof(root_menu_video_preview_paths[kept]));
            }
            kept++;
        }
        root_menu_video_preview_path_count = kept;
    }

    close(fd);
}

/* Load the photos.locks lock paths into a bounded buffer.  Returns the
 * entry count, 0 when no locks apply, or -1 when the lock database exists
 * but cannot be trusted (unreadable or oversized): callers must fail
 * closed and show nothing rather than leak private images. */
static int root_menu_video_photo_load_locks(char *buf, size_t buf_size,
                                            const char **locks,
                                            int max_locks)
{
    int fd;
    int count = 0;
    size_t used = 0;
    char line[MAX_PATH + 16];

    if (!file_exists(IPODJS_PHOTOS_LOCKS))
        return 0;

    fd = open(IPODJS_PHOTOS_LOCKS, O_RDONLY);
    if (fd < 0)
        return -1;

    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *separator;
        size_t len;

        video_trim_line(line);
        separator = strrchr(line, '|');
        if (!separator || separator == line)
            continue;
        *separator = '\0';

        len = strlen(line) + 1;
        if (count >= max_locks || used + len > buf_size)
        {
            close(fd);
            return -1;
        }

        memcpy(buf + used, line, len);
        locks[count++] = buf + used;
        used += len;
    }

    close(fd);
    return count;
}

/* Build the offset table from the RockPod-written preview index.  Returns
 * true when an index file exists (even if every entry was filtered), so
 * the caller skips the legacy directory scan. */
static bool root_menu_video_preview_load_photo_index(void)
{
    static char locks_buf[IPODJS_PHOTO_LOCKS_BUF];
    static const char *locks[128];
    char index_path[MAX_PATH];
    char line[MAX_PATH + 32];
    int lock_count;
    int fd;

    snprintf(index_path, sizeof(index_path), "%s/index.tsv",
             root_menu_video_photo_preview_root);
    fd = open(index_path, O_RDONLY);
    if (fd < 0)
        return false;

    root_menu_video_photo_index_active = true;

    lock_count = root_menu_video_photo_load_locks(locks_buf,
                                                  sizeof(locks_buf),
                                                  locks, ARRAYLEN(locks));
    if (lock_count < 0)
    {
        close(fd);
        return true;
    }

    while (root_menu_video_photo_index_count < IPODJS_PHOTO_INDEX_MAX)
    {
        off_t offset = lseek(fd, 0, SEEK_CUR);
        char *tab;
        size_t len;
        bool locked = false;

        if (offset < 0 || read_line(fd, line, sizeof(line)) <= 0)
            break;

        video_trim_line(line);
        if (line[0] == '#' || line[0] == '\0' ||
            !strncmp(line, "relpath\t", 8))
            continue;

        tab = strchr(line, '\t');
        if (tab)
            *tab = '\0';

        len = strlen(line);
        if (len <= 4 || strcasecmp(line + len - 4, ".bmp"))
            continue;

        for (int i = 0; i < lock_count && !locked; i++)
            locked = root_menu_video_photo_relative_locked(line, locks[i]);
        if (locked)
            continue;

        root_menu_video_photo_index_offsets[
            root_menu_video_photo_index_count++] = (uint32_t)offset;
    }

    close(fd);
    return true;
}

/* Re-read one preview path from the index.  Idle-service only: this opens
 * and reads a file and must never be reachable from a draw callback. */
static bool root_menu_video_photo_index_entry(int index, char *path,
                                              size_t path_size)
{
    char index_path[MAX_PATH];
    char line[MAX_PATH + 32];
    char *tab;
    int fd;
    bool ok;

    if (index < 0 || index >= root_menu_video_photo_index_count)
        return false;

    snprintf(index_path, sizeof(index_path), "%s/index.tsv",
             root_menu_video_photo_preview_root);
    fd = open(index_path, O_RDONLY);
    if (fd < 0)
        return false;

    ok = lseek(fd, (off_t)root_menu_video_photo_index_offsets[index],
               SEEK_SET) >= 0 &&
         read_line(fd, line, sizeof(line)) > 0;
    close(fd);
    if (!ok)
        return false;

    video_trim_line(line);
    tab = strchr(line, '\t');
    if (tab)
        *tab = '\0';
    if (!line[0])
        return false;

    return snprintf(path, path_size, "%s/%s",
                    root_menu_video_photo_preview_root, line) <
           (int)path_size;
}

/* Resolve the preview slide for an index entry, skipping previews whose
 * source photo was deleted since the index was written. */
static bool root_menu_video_photo_slide_path(int index, char *path,
                                             size_t path_size)
{
    char photo_path[MAX_PATH];
    const char *relative;
    size_t prefix_len;
    size_t len;

    if (!root_menu_video_photo_index_entry(index, path, path_size))
        return false;

    prefix_len = strlen(root_menu_video_photo_preview_root);
    if (strlen(path) <= prefix_len + 1)
        return false;

    relative = path + prefix_len + 1;
    len = strlen(relative);
    if (len <= 4)
        return false;

    if (snprintf(photo_path, sizeof(photo_path), "%s/%.*s",
                 root_menu_video_photo_root, (int)(len - 4), relative) >=
        (int)sizeof(photo_path))
        return false;

    return file_exists(photo_path);
}

static void root_menu_video_preview_load_photo_paths(void)
{
    root_menu_video_photo_index_active = false;
    root_menu_video_photo_index_count = 0;
    root_menu_video_photo_index_generation++;
    slideshow_order_reset(&root_menu_video_photo_order);

    if (!root_menu_video_preview_resolve_photo_root(
            root_menu_video_photo_root,
            sizeof(root_menu_video_photo_root)))
        return;

    snprintf(root_menu_video_photo_preview_root,
             sizeof(root_menu_video_photo_preview_root),
             "%s/.photo_previews", root_menu_video_photo_root);

    if (root_menu_video_preview_load_photo_index())
    {
        root_menu_video_preview_path_count =
            root_menu_video_photo_index_count;
        return;
    }

    /* Legacy layout without an index: scan only the preview tree. */
    root_menu_video_preview_scan_photo_previews(
        root_menu_video_photo_preview_root,
        root_menu_video_photo_preview_root,
        root_menu_video_photo_root, 0);
    root_menu_video_preview_filter_locked_photos(root_menu_video_photo_root);
}

static void root_menu_video_preview_load_game_paths(void)
{
    static const char * const indexes[] = {
        ROCKBOX_DIR "/rocks/games/rockboy_launcher/games.tsv",
        ROCKBOX_DIR "/rocks/games/pokemini_launcher/games.tsv",
    };
    static const char * const cover_dirs[] = {
        ROCKBOX_DIR "/rocks/games/rockboy_launcher/covers",
        ROCKBOX_DIR "/rocks/games/clubpenguin/covers",
        ROCKBOX_DIR "/rocks/games/runescape_classic/covers",
        ROCKBOX_DIR "/rocks/games/wwe_backstage/covers",
        ROCKBOX_DIR "/rocks/games/smsgg/covers",
        ROCKBOX_DIR "/rocks/games/pokemini_launcher/covers",
        IPODJS_ASSET_DIR "/clubpenguin/covers",
        IPODJS_ASSET_DIR "/runescape_classic/covers",
        IPODJS_ASSET_DIR "/stickrpg/covers",
        IPODJS_ASSET_DIR "/pokemini/covers",
    };

    for (int i = 0; i < (int)ARRAYLEN(indexes); i++)
    {
        int fd = open(indexes[i], O_RDONLY);
        char line[512];

        if (fd < 0)
            continue;

        while (root_menu_video_preview_path_count < IPODJS_PREVIEW_MAX_ITEMS &&
               read_line(fd, line, sizeof(line)) > 0)
        {
            char *fields[10];

            video_trim_line(line);
            if (line[0] == '#' || line[0] == '\0')
                continue;

            if (!video_split_tsv(line, fields, 10) || !fields[2][0])
                continue;

            char pane_path[MAX_PATH];
            if (root_menu_video_preview_pane_variant(fields[2], pane_path,
                                                     sizeof(pane_path)))
                root_menu_video_preview_add_path(pane_path);
            else if (file_exists(fields[2]))
                root_menu_video_preview_add_path(fields[2]);
        }

        close(fd);
    }

    for (int i = 0; i < (int)ARRAYLEN(cover_dirs); i++)
    {
        DIR *dir = root_menu_video_preview_opendir(IPODJS_PREVIEW_GAMES,
                                                   cover_dirs[i]);
        struct dirent *entry;

        if (!dir)
            continue;

        while (root_menu_video_preview_path_count < IPODJS_PREVIEW_MAX_ITEMS &&
               (entry = readdir(dir)) != NULL)
        {
            const char *ext = strrchr(entry->d_name, '.');
            char path[MAX_PATH];
            char pane_path[MAX_PATH];

            if (!ext || strcasecmp(ext, ".bmp") ||
                strstr(entry->d_name, ".pane.bmp"))
                continue;

            snprintf(path, sizeof(path), "%s/%s", cover_dirs[i],
                     entry->d_name);
            if (root_menu_video_preview_pane_variant(path, pane_path,
                                                     sizeof(pane_path)))
                root_menu_video_preview_add_path(pane_path);
            else if (file_exists(path))
                root_menu_video_preview_add_path(path);
        }

        closedir(dir);
    }
}

static bool root_menu_video_preview_safe_magazine_name(const char *name)
{
    const unsigned char *cursor = (const unsigned char *)name;

    if (!name || !name[0] || !strcmp(name, ".") || !strcmp(name, ".."))
        return false;
    while (*cursor)
    {
        if (*cursor == '/' || *cursor == '\\' || *cursor < 32)
            return false;
        cursor++;
    }
    return true;
}

static bool root_menu_video_preview_magazine_unlocked(
    const char *directory)
{
    char path[MAX_PATH];
    char line[256];
    int fd;

    snprintf(path, sizeof(path), "/Magazines/%s/issue.mgi", directory);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *cursor = line;

        video_trim_line(cursor);
        while (*cursor == ' ' || *cursor == '\t')
            cursor++;
        if (!strncmp(cursor, "locked=", 7) && atoi(cursor + 7) != 0)
        {
            close(fd);
            return false;
        }
    }
    close(fd);
    return true;
}

static void root_menu_video_preview_load_magazine_paths(void)
{
    char directory[256];
    char cover[MAX_PATH];
    int fd = open("/Magazines/catalog.mgi", O_RDONLY);

    while (fd >= 0 &&
           root_menu_video_preview_path_count < IPODJS_PREVIEW_MAX_ITEMS &&
           read_line(fd, directory, sizeof(directory)) > 0)
    {
        video_trim_line(directory);
        if (!directory[0] || directory[0] == '#' ||
            !root_menu_video_preview_safe_magazine_name(directory) ||
            !root_menu_video_preview_magazine_unlocked(directory))
            continue;
        snprintf(cover, sizeof(cover), "/Magazines/%s/cover-pane.jpg",
                 directory);
        if (!file_exists(cover))
            snprintf(cover, sizeof(cover), "/Magazines/%s/cover.jpg",
                     directory);
        if (file_exists(cover))
            root_menu_video_preview_add_path(cover);
    }
    if (fd >= 0)
        close(fd);
}

static bool root_menu_video_preview_comics_unlocked(
    const char *directory)
{
    char path[MAX_PATH];
    char line[256];
    int fd;

    snprintf(path, sizeof(path), "/Comics/%s/issue.mgi", directory);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *cursor = line;

        video_trim_line(cursor);
        while (*cursor == ' ' || *cursor == '\t')
            cursor++;
        if (!strncmp(cursor, "locked=", 7) && atoi(cursor + 7) != 0)
        {
            close(fd);
            return false;
        }
    }
    close(fd);
    return true;
}

static void root_menu_video_preview_load_comics_paths(void)
{
    char directory[256];
    char cover[MAX_PATH];
    int fd = open("/Comics/catalog.mgi", O_RDONLY);

    while (fd >= 0 &&
           root_menu_video_preview_path_count < IPODJS_PREVIEW_MAX_ITEMS &&
           read_line(fd, directory, sizeof(directory)) > 0)
    {
        video_trim_line(directory);
        if (!directory[0] || directory[0] == '#' ||
            !root_menu_video_preview_safe_magazine_name(directory) ||
            !root_menu_video_preview_comics_unlocked(directory))
            continue;
        snprintf(cover, sizeof(cover), "/Comics/%s/cover-pane.jpg",
                 directory);
        if (!file_exists(cover))
            snprintf(cover, sizeof(cover), "/Comics/%s/cover.jpg",
                     directory);
        if (file_exists(cover))
            root_menu_video_preview_add_path(cover);
    }
    if (fd >= 0)
        close(fd);
}

static void root_menu_video_preview_load_pokemini_paths(void)
{
    static const char * const cover_dirs[] = {
        ROCKBOX_DIR "/rocks/games/pokemini_launcher/covers",
        IPODJS_ASSET_DIR "/pokemini/covers",
    };

    for (int i = 0; i < (int)ARRAYLEN(cover_dirs); i++)
    {
        DIR *dir = root_menu_video_preview_opendir(IPODJS_PREVIEW_POKEMINI,
                                                   cover_dirs[i]);
        struct dirent *entry;

        if (!dir)
            continue;

        while (root_menu_video_preview_path_count < IPODJS_PREVIEW_MAX_ITEMS &&
               (entry = readdir(dir)) != NULL)
        {
            const char *ext = strrchr(entry->d_name, '.');
            char path[MAX_PATH];
            char pane_path[MAX_PATH];

            if (!ext || strcasecmp(ext, ".bmp") ||
                strstr(entry->d_name, ".pane.bmp"))
                continue;

            snprintf(path, sizeof(path), "%s/%s", cover_dirs[i],
                     entry->d_name);
            if (root_menu_video_preview_pane_variant(path, pane_path,
                                                     sizeof(pane_path)))
                root_menu_video_preview_add_path(pane_path);
            else if (file_exists(path))
                root_menu_video_preview_add_path(path);
        }

        closedir(dir);
    }
}

/* Menu preview frames live beside the avatar clips the Achievements plugin
 * uses, so the pane always shows the avatar RockPod last synced.  Only the
 * frame list is read here; decoding stays on the throttled slideshow path. */
#define IPODJS_AVATAR_PREVIEW_ROOT ROCKBOX_DIR "/achievements/avatar"

/* Generation the pane's frame list was built from, so a freshly synced
 * avatar replaces a stale one without needing a reboot. */
static char root_menu_video_avatar_generation[40];
static long root_menu_video_avatar_check_tick;

static bool root_menu_video_avatar_read_generation(char *buffer, size_t size)
{
    int fd = open(IPODJS_AVATAR_PREVIEW_ROOT "/current", O_RDONLY);
    bool loaded = false;

    if (fd < 0)
        return false;
    if (read_line(fd, buffer, (int)size) > 0 && buffer[0])
        loaded = true;
    close(fd);
    return loaded;
}

/* Called only from the idle service point, and at most once a second. */
static void root_menu_video_avatar_refresh_generation(void)
{
    char generation[40];

    if (!TIME_AFTER(current_tick, root_menu_video_avatar_check_tick))
        return;
    root_menu_video_avatar_check_tick = current_tick + HZ;
    if (!root_menu_video_avatar_read_generation(generation,
                                                sizeof(generation)))
        return;
    if (!strcmp(generation, root_menu_video_avatar_generation))
        return;
    strmemccpy(root_menu_video_avatar_generation, generation,
               sizeof(root_menu_video_avatar_generation));
    root_menu_video_preview_invalidate_source_cache(IPODJS_PREVIEW_AVATAR);
    if (root_menu_video_preview_loaded == IPODJS_PREVIEW_AVATAR)
        root_menu_video_preview_loaded = IPODJS_PREVIEW_NONE;
}

static void root_menu_video_preview_load_avatar_paths(void)
{
    char generation[40];
    char directory[MAX_PATH];
    char frame[MAX_PATH];
    int fd;
    int index;

    if (!root_menu_video_avatar_read_generation(generation,
                                                sizeof(generation)))
        return;
    strmemccpy(root_menu_video_avatar_generation, generation,
               sizeof(root_menu_video_avatar_generation));

    if (snprintf(directory, sizeof(directory), "%s/generations/%s/menu",
                 IPODJS_AVATAR_PREVIEW_ROOT, generation) >=
            (int)sizeof(directory))
        return;

    /* Frames are numbered by RockPod, so they are gathered in order rather
     * than in whatever order the directory happens to return. */
    for (index = 0; index < IPODJS_PREVIEW_MAX_ITEMS; index++)
    {
        if (snprintf(frame, sizeof(frame), "%s/frame-%02d.bmp",
                     directory, index) >= (int)sizeof(frame))
            break;
        if (!file_exists(frame))
            break;
        root_menu_video_preview_add_path(frame);
    }
}

static void root_menu_video_preview_ensure_paths(
    enum root_menu_video_preview_source source)
{
    struct root_menu_video_preview_source_cache *cache;

    if (source <= IPODJS_PREVIEW_NONE ||
        source > IPODJS_PREVIEW_AVATAR)
    {
        root_menu_video_preview_loaded = source;
        root_menu_video_preview_path_count = 0;
        root_menu_video_preview_reset_slots();
        return;
    }

    if (root_menu_video_preview_loaded == source)
        return;

    cache = &root_menu_video_preview_source_caches[source];
    root_menu_video_preview_loaded = source;
    root_menu_video_preview_reset_slots();
    root_menu_video_preview_decode_budget = IPODJS_PREVIEW_DECODE_BURST;
    root_menu_video_preview_decode_window_tick = current_tick;

    /* Keep one active path table.  Source caches retain only scan metadata;
     * duplicating 64 MAX_PATH strings for every source wasted static RAM. */
    root_menu_video_preview_path_count = 0;

    if (source == IPODJS_PREVIEW_VIDEOS)
        root_menu_video_preview_load_video_paths();
    else if (source == IPODJS_PREVIEW_PHOTOS)
        root_menu_video_preview_load_photo_paths();
    else if (source == IPODJS_PREVIEW_GAMES)
        root_menu_video_preview_load_game_paths();
    else if (source == IPODJS_PREVIEW_POKEMINI)
        root_menu_video_preview_load_pokemini_paths();
    else if (source == IPODJS_PREVIEW_MAGAZINES)
        root_menu_video_preview_load_magazine_paths();
    else if (source == IPODJS_PREVIEW_COMICS)
        root_menu_video_preview_load_comics_paths();
    else if (source == IPODJS_PREVIEW_AVATAR)
        root_menu_video_preview_load_avatar_paths();

    cache->path_count = root_menu_video_preview_path_count;
    cache->loaded = true;
    cache->next_reload = current_tick + HZ * 180;
}

static struct root_menu_video_preview_slot *
root_menu_video_preview_slot_victim(void)
{
    for (int i = 0; i < IPODJS_PREVIEW_IMAGE_CACHE; i++)
    {
        if (!root_menu_video_preview_slots[i].valid)
            return &root_menu_video_preview_slots[i];
    }

    struct root_menu_video_preview_slot *slot =
        &root_menu_video_preview_slots[root_menu_video_preview_victim];
    root_menu_video_preview_victim =
        (root_menu_video_preview_victim + 1) % IPODJS_PREVIEW_IMAGE_CACHE;
    return slot;
}

static bool root_menu_video_preview_photo_index_mode(
    enum root_menu_video_preview_source source)
{
    return source == IPODJS_PREVIEW_PHOTOS &&
           root_menu_video_photo_index_active;
}

static struct root_menu_video_preview_slot *
root_menu_video_preview_find_slot(enum root_menu_video_preview_source source,
                                  int index)
{
    bool index_mode = root_menu_video_preview_photo_index_mode(source);
    const char *path = NULL;

    root_menu_video_preview_use_cache(IPODJS_PREVIEW_CACHE_SLIDESHOW);

    if (index < 0 || index >= root_menu_video_preview_path_count)
        return NULL;

    /* Index-backed photos cannot compare paths here: resolving one needs
     * file I/O and this runs from draw callbacks.  The generation stamp
     * invalidates slots across index reloads instead. */
    if (!index_mode)
        path = root_menu_video_preview_paths[index];

    for (int i = 0; i < IPODJS_PREVIEW_IMAGE_CACHE; i++)
    {
        struct root_menu_video_preview_slot *slot =
            &root_menu_video_preview_slots[i];

        if (!slot->valid || slot->source != source || slot->index != index)
            continue;

        if (index_mode)
        {
            if (slot->gen == root_menu_video_photo_index_generation)
                return slot;
        }
        else if (!strcmp(slot->path, path))
            return slot;
    }

    return NULL;
}

static struct root_menu_video_preview_slot *
root_menu_video_preview_get_slot(enum root_menu_video_preview_source source,
                                 int index)
{
    char resolved[MAX_PATH];
    const char *path;
    struct root_menu_video_preview_slot *slot;

    slot = root_menu_video_preview_find_slot(source, index);
    if (slot)
        return slot;

    if (index < 0 || index >= root_menu_video_preview_path_count)
        return NULL;

    if (root_menu_video_preview_photo_index_mode(source))
    {
        if (root_menu_video_preview_recent_failure(source, index, ""))
            return NULL;

        if (!root_menu_video_photo_slide_path(index, resolved,
                                              sizeof(resolved)))
        {
            root_menu_video_preview_record_failure(source, index, "");
            return NULL;
        }
        path = resolved;
    }
    else
        path = root_menu_video_preview_paths[index];

    if (root_menu_video_preview_recent_failure(source, index, path))
        return NULL;

    if (!root_menu_video_preview_decode_permitted())
        return NULL;

    slot = root_menu_video_preview_slot_victim();
    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->bm.width = IPODJS_PREVIEW_IMAGE_WIDTH;
    slot->bm.height = IPODJS_PREVIEW_IMAGE_HEIGHT;
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;

    int rc;

#ifdef HAVE_JPEG
    if (source == IPODJS_PREVIEW_MAGAZINES || source == IPODJS_PREVIEW_COMICS)
        rc = read_jpeg_file(path, &slot->bm, sizeof(slot->data),
                            FORMAT_NATIVE | FORMAT_RESIZE |
                            FORMAT_KEEP_ASPECT, NULL);
    else
#endif
        rc = read_bmp_file(path, &slot->bm, sizeof(slot->data),
                           FORMAT_NATIVE | FORMAT_RESIZE |
                           FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);
    if (rc < 0)
    {
        slot->valid = false;
        root_menu_video_preview_record_failure(source, index, path);
        return NULL;
    }

    slot->valid = true;
    slot->source = source;
    slot->index = index;
    slot->gen = root_menu_video_photo_index_generation;
    strmemccpy(slot->path, path, sizeof(slot->path));
    root_menu_video_preview_clear_failure(source, index, path);
    return slot;
}

/* Photos match the Music pane: one slow pan per slide, random non-repeating
 * order, next slide prefetched from half-phase.  The other sources keep their
 * original three-second sequential cadence. */
static long root_menu_video_source_slideshow_period(
    enum root_menu_video_preview_source source)
{
    if (source == IPODJS_PREVIEW_AVATAR)
        return MAX(1, HZ / 6);
    if (source == IPODJS_PREVIEW_PHOTOS)
        return HZ * 6;
    return HZ * 3;
}

static long root_menu_video_source_slideshow_pan_duration(
    enum root_menu_video_preview_source source)
{
    if (source == IPODJS_PREVIEW_PHOTOS)
        return HZ * 6;
    return HZ * 2;
}

/* The avatar pane plays pre-rendered emote frames.  Panning would move the
 * stage underneath the character and make the emote look unstable. */
static bool root_menu_video_preview_pans(
    enum root_menu_video_preview_source source)
{
    return source != IPODJS_PREVIEW_AVATAR;
}

static long root_menu_video_source_slideshow_prefetch_phase(
    enum root_menu_video_preview_source source)
{
    if (source == IPODJS_PREVIEW_AVATAR)
        return MAX(1, root_menu_video_source_slideshow_period(source) / 2);
    if (source == IPODJS_PREVIEW_PHOTOS)
        return root_menu_video_source_slideshow_period(source) / 2;
    return HZ;
}

static int root_menu_video_source_slideshow_wanted(
    enum root_menu_video_preview_source source, long cycle)
{
    if (root_menu_video_preview_path_count <= 0)
        return 0;

    if (source == IPODJS_PREVIEW_PHOTOS)
        return slideshow_order_index(&root_menu_video_photo_order, cycle,
                                     root_menu_video_preview_path_count);

    return (int)(cycle % root_menu_video_preview_path_count);
}

static bool root_menu_video_preview_service(
    enum root_menu_video_preview_source source)
{
    long period;
    long cycle;
    long phase;
    int wanted;
    int next_wanted;

    /* Scanning and bitmap decode are idle services, never paint work. */
    if (!button_queue_empty() || button_hold() ||
        root_menu_video_hold_storm_active())
        return false;

    if (source == IPODJS_PREVIEW_AVATAR)
        root_menu_video_avatar_refresh_generation();

    root_menu_video_preview_ensure_paths(source);
    if (root_menu_video_preview_path_count <= 0)
        return false;

    period = root_menu_video_source_slideshow_period(source);
    cycle = current_tick / period;
    phase = current_tick % period;
    wanted = root_menu_video_source_slideshow_wanted(source, cycle);
    next_wanted = root_menu_video_source_slideshow_wanted(source, cycle + 1);

    if (!root_menu_video_preview_find_slot(source, wanted))
        return root_menu_video_preview_get_slot(source, wanted) != NULL;

    if (phase >= root_menu_video_source_slideshow_prefetch_phase(source) &&
        !root_menu_video_preview_find_slot(source, next_wanted))
        (void)root_menu_video_preview_get_slot(source, next_wanted);

    return false;
}

static bool root_menu_video_source_slideshow_frame_changed(
    enum root_menu_video_preview_source source)
{
    int wanted;
    struct root_menu_video_preview_slot *slot;

    if (root_menu_video_preview_loaded != source ||
        root_menu_video_preview_path_count <= 0)
        return false;

    wanted = root_menu_video_source_slideshow_wanted(
        source,
        current_tick / root_menu_video_source_slideshow_period(source));
    slot = root_menu_video_preview_find_slot(source, wanted);
    return slot &&
        (root_menu_video_preview_last_drawn_source != source ||
         root_menu_video_preview_last_drawn_index != slot->index);
}

static bool root_menu_video_draw_preview_cover(
    const struct bitmap *bm, int x, int y, int w, int h,
    long pan_pos, long pan_scale)
{
    int crop_w;
    int crop_h;
    int pan_range_x;
    int pan_range_y;
    int src_x;
    int src_y;
    unsigned long x_step;
    unsigned long y_step;
    unsigned long y_pos;
    const fb_data *pixels;

    if (!bm || !bm->data || bm->width <= 0 || bm->height <= 0 ||
        w <= 0 || h <= 0 || pan_scale <= 0)
        return false;

    /* Crop to the pane aspect before scaling so every cached image covers
     * the full right pane. This also lets low-resolution photo previews and
     * portrait game covers zoom without exposing the grey pane beneath. */
    if (bm->width * h > bm->height * w)
    {
        crop_h = bm->height;
        crop_w = MAX(1, bm->height * w / h);
    }
    else
    {
        crop_w = bm->width;
        crop_h = MAX(1, bm->width * h / w);
    }

    pan_range_x = bm->width - crop_w;
    pan_range_y = bm->height - crop_h;
    src_x = pan_range_x * pan_pos / pan_scale;
    src_y = pan_range_y * pan_pos / pan_scale;
    x_step = ((unsigned long)crop_w << 16) / w;
    y_step = ((unsigned long)crop_h << 16) / h;
    y_pos = ((unsigned long)src_y << 16) + y_step / 2;
    pixels = (const fb_data *)bm->data;

    for (int dst_y = 0; dst_y < h; dst_y++)
    {
        int sample_y = MIN(bm->height - 1, (int)(y_pos >> 16));
        const fb_data *src = pixels + sample_y * bm->width;
        fb_data *dst = FBADDR(x, y + dst_y);
        unsigned long x_pos = ((unsigned long)src_x << 16) + x_step / 2;

        for (int dst_x = 0; dst_x < w; dst_x++)
        {
            int sample_x = MIN(bm->width - 1, (int)(x_pos >> 16));
            dst[dst_x] = src[sample_x];
            x_pos += x_step;
        }
        y_pos += y_step;
    }

    return true;
}

static bool root_menu_video_draw_source_slideshow_cached(
    enum root_menu_video_preview_source source, int x, int y, int w, int h)
{
    if (button_hold())
        return false;

    if (root_menu_video_hold_storm_active())
        return false;

    if (root_menu_video_preview_loaded != source ||
        root_menu_video_preview_path_count <= 0)
        return false;

    long tick = current_tick;
    long period = root_menu_video_source_slideshow_period(source);
    long pan_duration = root_menu_video_source_slideshow_pan_duration(source);
    long cycle = tick / period;
    long phase = tick % period;
    int wanted = root_menu_video_source_slideshow_wanted(source, cycle);
    long pan_phase = MIN(phase, pan_duration);
    long pan_scale = 1000;
    long pan_pos = pan_phase * pan_scale / pan_duration;
    struct root_menu_video_preview_slot *slot =
        root_menu_video_preview_find_slot(source, wanted);

    if (!slot && root_menu_video_preview_last_drawn_source == source &&
        root_menu_video_preview_last_drawn_index >= 0)
        slot = root_menu_video_preview_find_slot(
            source, root_menu_video_preview_last_drawn_index);
    if (!slot)
        return false;

    if (cycle & 1)
        pan_pos = pan_scale - pan_pos;

    if (!root_menu_video_preview_pans(source))
        pan_pos = pan_scale / 2;

    if (!root_menu_video_draw_preview_cover(&slot->bm, x, y, w, h,
                                             pan_pos, pan_scale))
        return false;

    root_menu_video_preview_last_drawn_source = source;
    root_menu_video_preview_last_drawn_index = slot->index;

    return true;
}

static void root_menu_video_draw_clock_date(int x, int y, int w, int h,
                                            bool show_time);
static void root_menu_video_draw_stock_clock_preview(int x, int y, int w,
                                                     int h);
static bool root_menu_video_prepare_sitekick_preview(void);
static bool root_menu_video_draw_sitekick_preview(int x, int y, int w,
                                                  int h);
static bool root_menu_video_sitekick_animation_due(const char *title,
                                                   long *next_tick);

static void root_menu_video_draw_stock_preview_pane(const char *title,
                                                    int x, int y, int w,
                                                    int h)
{
    if (title && (!strcmp(title, "Clock") ||
        (!strcmp(title, "Extras") &&
         global_settings.ui_engine_extras_pane == UI_ENGINE_EXTRAS_CLOCK)))
    {
        root_menu_video_draw_stock_clock_preview(x, y, w, h);
        return;
    }

    root_menu_video_preview_gradient(x, y, w, h);
}

static bool root_menu_video_should_animate(
    enum root_menu_video_preview_source source, long *next_tick)
{
    int status = audio_status();
    long delay = (status & AUDIO_STATUS_PLAY) ?
                 IPODJS_SLIDESHOW_AUDIO_DELAY : IPODJS_SLIDESHOW_DELAY;

    if (button_hold() || (status & AUDIO_STATUS_PAUSE))
        return false;

    if (!root_menu_video_preview_uses_slideshow(source))
        return false;

    if (root_menu_video_hold_storm_active())
        return false;

    if (!TIME_AFTER(current_tick, *next_tick))
        return false;

    *next_tick = current_tick + delay;
    return true;
}

static bool root_menu_video_sitekick_title(const char *title)
{
    return title &&
        (!strcmp(title, "Sitekick") ||
         (!strcmp(title, "Extras") &&
          global_settings.ui_engine_extras_pane ==
              UI_ENGINE_EXTRAS_SITEKICK));
}

static bool root_menu_video_sitekick_animation_due(const char *title,
                                                   long *next_tick)
{
    if (!root_menu_video_sitekick_title(title) || button_hold() ||
        root_menu_video_hold_storm_active() ||
        !TIME_AFTER(current_tick, *next_tick))
        return false;

    *next_tick = current_tick + MAX(1, HZ / 12);
    return true;
}

static void root_menu_video_draw_preview_for_title(const char *title,
                                                   int x, int y, int w, int h)
{
    enum root_menu_video_preview_source source =
        root_menu_video_preview_source_for_title(title);
    bool drew = false;

    root_menu_video_preview_gradient(x, y, w, h);
    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(IPODJS_PREVIEW_TEXT);
    lcd_set_background(IPODJS_PREVIEW_BOTTOM);

    if (title && (!strcmp(title, "Applications") ||
                  !strcmp(title, "Weather")))
    {
        root_menu_video_draw_weather_preview(x, y, w, h);
        return;
    }

    if (title && !strcmp(title, "Calm"))
    {
        root_menu_video_draw_calm_preview(x, y, w, h);
        return;
    }

    if (title && !strcmp(title, "Maps"))
    {
        root_menu_video_draw_maps_preview(x, y, w, h);
        return;
    }

    if (root_menu_video_sitekick_title(title) &&
        root_menu_video_draw_sitekick_preview(x, y, w, h))
        return;

    if (title && !strcmp(title, "Netflix"))
    {
        struct bitmap *logo = root_menu_video_netflix_logo_cached();

        lcd_set_foreground(VIDEO_LIST_NETFLIX_RED);
        lcd_fillrect(x, y, w, h);
        if (logo)
            lcd_bmp(logo, x + (w - logo->width) / 2,
                    y + (h - logo->height) / 2);
        return;
    }

    if (button_hold())
        source = IPODJS_PREVIEW_NONE;

    if (source == IPODJS_PREVIEW_MUSIC)
    {
        /* The Home pane previews the selected menu, not playback state.
         * Keep Music on its album-cover pan even while a track is playing;
         * the status-bar play glyph is the persistent playback indicator. */
        /* Navigation redraws must never open or decode artwork. The working
         * iPone theme pane services its slideshow from the status-bar update
         * loop; iPodJS follows the same split and only paints cached pixels
         * here. */
        drew = albumlist_draw_slideshow_cached(&screens[SCREEN_MAIN],
                                                x, y, w, h);
    }
    else if (source == IPODJS_PREVIEW_VIDEOS ||
             source == IPODJS_PREVIEW_PHOTOS ||
             source == IPODJS_PREVIEW_GAMES ||
             source == IPODJS_PREVIEW_POKEMINI ||
             source == IPODJS_PREVIEW_MAGAZINES ||
             source == IPODJS_PREVIEW_COMICS ||
             source == IPODJS_PREVIEW_AVATAR)
        drew = root_menu_video_draw_source_slideshow_cached(source,
                                                            x, y, w, h);

    if (!drew && root_menu_video_preview_asset_is_verified(title))
        drew = root_menu_video_draw_menu_preview_asset_cached(title,
                                                              x, y, w, h);

    if (drew && title && !strcmp(title, "Clock"))
        root_menu_video_draw_clock_date(x, y, w, h, false);

    if (!drew)
    {
        root_menu_video_draw_stock_preview_pane(title, x, y, w, h);
    }
}

static void root_menu_video_draw_clock_date(int x, int y, int w, int h,
                                            bool show_time)
{
    static const char * const months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    struct tm *tm = get_time();
    char buf[32];
    int text_y = y + h - (show_time ? 48 : 31);

    if (!tm || tm->tm_mon < 0 || tm->tm_mon > 11)
        return;

    lcd_setfont(root_menu_video_font());
    lcd_set_drawmode(DRMODE_FG);
    lcd_set_foreground(LCD_RGBPACK(58, 68, 80));
    snprintf(buf, sizeof(buf), "%s %d %d", months[tm->tm_mon],
             tm->tm_mday, tm->tm_year + 1900);
    root_menu_video_puts_fit(x + 16, text_y + 1, w - 32, buf, true);
    lcd_set_foreground(LCD_RGBPACK(245, 247, 250));
    root_menu_video_puts_fit(x + 16, text_y, w - 32, buf, true);

    if (show_time)
    {
        int display_hour = tm->tm_hour;

        if (global_settings.timeformat)
        {
            display_hour %= 12;
            if (display_hour == 0)
                display_hour = 12;
        }

        snprintf(buf, sizeof(buf), "%02d:%02d", display_hour, tm->tm_min);
        lcd_set_foreground(LCD_RGBPACK(45, 53, 64));
        root_menu_video_puts_fit(x + 16, text_y + 20, w - 32, buf, true);
    }
    lcd_set_drawmode(DRMODE_SOLID);
}

static void root_menu_video_fill_circle(int cx, int cy, int radius,
                                        unsigned color)
{
    int x = 0;
    int y = radius;
    int decision = 3 - 2 * radius;

    lcd_set_foreground(color);
    while (y >= x)
    {
        lcd_hline(cx - x, cx + x, cy - y);
        lcd_hline(cx - x, cx + x, cy + y);
        lcd_hline(cx - y, cx + y, cy - x);
        lcd_hline(cx - y, cx + y, cy + x);

        if (decision > 0)
        {
            y--;
            decision += 4 * (x - y) + 10;
        }
        else
            decision += 4 * x + 6;
        x++;
    }
}

static bool root_menu_video_animated_preview_info(
    const char *title, const char **directory, int *frame_count,
    long *frame_period)
{
    if (title && (!strcmp(title, "Applications") ||
                  !strcmp(title, "Weather")))
    {
        *directory = "weather-loop";
        *frame_count = IPODJS_WEATHER_PREVIEW_FRAMES;
        *frame_period = HZ;
        return true;
    }
    if (title && !strcmp(title, "Maps"))
    {
        *directory = "maps-globe";
        *frame_count = IPODJS_MAPS_PREVIEW_FRAMES;
        *frame_period = MAX(1, HZ / 2);
        return true;
    }
    return false;
}

static bool root_menu_video_animated_preview_path(
    const char *title, int frame, char *path, size_t path_size)
{
    const char *directory;
    int frame_count;
    long frame_period;

    if (!root_menu_video_animated_preview_info(
            title, &directory, &frame_count, &frame_period))
        return false;

    (void)frame_period;
    frame %= frame_count;
    if (frame < 0)
        frame += frame_count;
    return snprintf(path, path_size,
                    IPODJS_ASSET_DIR "/previews/%s/frame-%02d.bmp",
                    directory, frame) < (int)path_size;
}

static int root_menu_video_animated_preview_frame(const char *title)
{
    const char *directory;
    int frame_count;
    long frame_period;

    if (!root_menu_video_animated_preview_info(
            title, &directory, &frame_count, &frame_period))
        return 0;

    (void)directory;
    return (current_tick / frame_period) % frame_count;
}

/* Load at most one fixed-size frame from an idle service point. The draw
 * functions below only look up and paint these two existing menu-cache slots;
 * they never touch storage or allocate playback memory. */
static bool root_menu_video_animated_preview_service(const char *title)
{
    char path[MAX_PATH];
    int dark = root_menu_video_dark() ? 1 : 0;
    int current;

    if (!root_menu_video_animated_preview_path(title, 0,
                                               path, sizeof(path)))
        return false;

    current = root_menu_video_animated_preview_frame(title);
    for (int offset = 0; offset < 2; offset++)
    {
        struct root_menu_video_menu_preview_slot *slot;
        int wanted = current + offset;
        int rc;

        if (!root_menu_video_animated_preview_path(
                title, wanted, path, sizeof(path)))
            return false;
        slot = root_menu_video_menu_preview_find(path, dark);
        if (slot)
            continue;

        /* Weather Sync may install these frames while Rockbox is running.
         * Do not negative-cache an absent live asset: that made a completed
         * sync continue to look missing for up to a minute.  Decode failures
         * are still throttled below so a corrupt BMP cannot cause an I/O
         * retry loop. */
        if (!file_exists(path))
            continue;
        if (root_menu_video_menu_preview_recent_failure(path, dark))
            continue;

        slot = root_menu_video_menu_preview_victim();
        memset(&slot->bm, 0, sizeof(slot->bm));
        slot->valid = false;
        slot->path[0] = '\0';
        slot->dark = dark;
        slot->bm.width = 174;
        slot->bm.height = LCD_HEIGHT;
        slot->bm.format = FORMAT_NATIVE;
        slot->bm.data = slot->data;
        rc = read_bmp_file(path, &slot->bm, sizeof(slot->data),
                           FORMAT_NATIVE, NULL);
        if (rc < 0)
        {
            root_menu_video_menu_preview_record_failure(path, dark);
            return false;
        }
        strmemccpy(slot->path, path, sizeof(slot->path));
        slot->valid = true;
        root_menu_video_menu_preview_clear_failure(path, dark);
        return true;
    }

    return false;
}

static struct root_menu_video_menu_preview_slot *
root_menu_video_animated_preview_cached(const char *title)
{
    char path[MAX_PATH];
    int dark = root_menu_video_dark() ? 1 : 0;
    int current = root_menu_video_animated_preview_frame(title);
    struct root_menu_video_menu_preview_slot *slot;

    if (!root_menu_video_animated_preview_path(
            title, current, path, sizeof(path)))
        return NULL;
    slot = root_menu_video_menu_preview_find(path, dark);
    if (slot)
        return slot;

    /* A frame boundary can arrive before the idle service has decoded its
     * prefetched successor. Hold the immediately previous photographic frame
     * instead of flashing a placeholder. */
    if (!root_menu_video_animated_preview_path(
            title, current - 1, path, sizeof(path)))
        return NULL;
    return root_menu_video_menu_preview_find(path, dark);
}

static void root_menu_video_draw_weather_preview(int x, int y, int w, int h)
{
    struct root_menu_video_menu_preview_slot *slot =
        root_menu_video_animated_preview_cached("Weather");

    if (slot)
    {
        root_menu_video_draw_menu_preview_slot(slot, x, y, w, h, true);
        return;
    }

    root_menu_video_gradient(x, y, w, h,
                             LCD_RGBPACK(20, 67, 104),
                             LCD_RGBPACK(5, 23, 42));
    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(LCD_RGBPACK(233, 245, 255));
    lcd_set_background(LCD_RGBPACK(5, 23, 42));
    root_menu_video_puts_fit(x + 12, y + 88, w - 24,
                             "LOCAL FORECAST", true);
    lcd_set_foreground(LCD_RGBPACK(116, 190, 233));
    root_menu_video_puts_fit(x + 12, y + 111, w - 24,
                             "Loading synced forecast", true);
}

static void root_menu_video_draw_calm_preview(int x, int y, int w, int h)
{
    const char *path = IPODJS_ASSET_DIR
        "/calm/calm-icon.64x64x24.bmp";
    struct root_menu_video_menu_preview_slot *logo =
        root_menu_video_menu_preview_find(
            path, root_menu_video_dark() ? 1 : 0);

    root_menu_video_glass_gradient(x, y, w, h,
                                   LCD_RGBPACK(71, 202, 235),
                                   LCD_RGBPACK(54, 153, 232),
                                   LCD_RGBPACK(37, 91, 192));
    if (logo)
        lcd_bmp(&logo->bm, x + (w - logo->bm.width) / 2, y + 58);

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
    lcd_set_background(LCD_RGBPACK(37, 91, 192));
    root_menu_video_puts_fit(x + 14, y + 139, w - 28,
                             "Sleep  Relax  Focus", true);
    lcd_set_foreground(LCD_RGBPACK(211, 239, 255));
    root_menu_video_puts_fit(x + 14, y + 165, w - 28,
                             "Your calm library", true);
}

static void root_menu_video_draw_maps_preview(int x, int y, int w, int h)
{
    struct root_menu_video_menu_preview_slot *slot =
        root_menu_video_animated_preview_cached("Maps");

    if (slot)
    {
        root_menu_video_draw_menu_preview_slot(slot, x, y, w, h, true);
        return;
    }

    lcd_set_foreground(LCD_RGBPACK(0, 0, 0));
    lcd_fillrect(x, y, w, h);
    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(LCD_RGBPACK(173, 211, 238));
    lcd_set_background(LCD_RGBPACK(0, 0, 0));
    root_menu_video_puts_fit(x + 12, y + 104, w - 24,
                             "LOADING EARTH", true);
}

static bool root_menu_video_custom_preview_animation_due(
    const char *title, long *next_tick)
{
    const char *directory;
    int frame_count;
    long frame_period;

    if (!root_menu_video_animated_preview_info(
            title, &directory, &frame_count, &frame_period) ||
        button_hold() || root_menu_video_hold_storm_active() ||
        !TIME_AFTER(current_tick, *next_tick))
        return false;

    (void)directory;
    (void)frame_count;
    *next_tick = current_tick + frame_period;
    return true;
}

static void root_menu_video_clock_hand(int cx, int cy, int position,
                                       int length, unsigned color,
                                       bool thick)
{
    static const short sine[60] = {
           0,  105,  208,  309,  407,  500,  588,  669,  743,  809,
         866,  914,  951,  978,  995, 1000,  995,  978,  951,  914,
         866,  809,  743,  669,  588,  500,  407,  309,  208,  105,
           0, -105, -208, -309, -407, -500, -588, -669, -743, -809,
        -866, -914, -951, -978, -995,-1000, -995, -978, -951, -914,
        -866, -809, -743, -669, -588, -500, -407, -309, -208, -105,
    };
    int end_x;
    int end_y;

    position %= 60;
    if (position < 0)
        position += 60;
    end_x = cx + sine[position] * length / 1000;
    end_y = cy - sine[(position + 15) % 60] * length / 1000;

    lcd_set_foreground(color);
    lcd_drawline(cx, cy, end_x, end_y);
    if (thick)
    {
        lcd_drawline(cx - 1, cy, end_x - 1, end_y);
        lcd_drawline(cx + 1, cy, end_x + 1, end_y);
    }
}

/* Blit the stock Extras/Clock pane background through the menu-preview
 * slot cache.  Fails closed to the gradient fallback when the asset is
 * missing or unreadable. */
static bool root_menu_video_draw_clock_pane_background(int x, int y, int w,
                                                       int h)
{
    char themed_path[MAX_PATH];
    const char *load_path;
    struct root_menu_video_menu_preview_slot *slot;
    int dark = root_menu_video_dark() ? 1 : 0;
    int rc;

    load_path = root_menu_video_asset_path(
        IPODJS_ASSET_DIR "/previews/clock-pane-stock.174x240x24.bmp",
        themed_path, sizeof(themed_path));
    slot = root_menu_video_menu_preview_find(load_path, dark);
    if (!slot)
    {
        if (root_menu_video_menu_preview_recent_failure(load_path, dark))
            return false;
        if (!file_exists(load_path))
        {
            root_menu_video_menu_preview_record_failure(load_path, dark);
            return false;
        }

        slot = root_menu_video_menu_preview_victim();
        memset(&slot->bm, 0, sizeof(slot->bm));
        slot->valid = false;
        slot->path[0] = '\0';
        slot->dark = dark;
        slot->bm.width = 174;
        slot->bm.height = LCD_HEIGHT;
        slot->bm.format = FORMAT_NATIVE;
        slot->bm.data = slot->data;
        rc = read_bmp_file(load_path, &slot->bm, sizeof(slot->data),
                           FORMAT_NATIVE, NULL);
        if (rc < 0)
        {
            root_menu_video_menu_preview_record_failure(load_path, dark);
            return false;
        }

        strmemccpy(slot->path, load_path, sizeof(slot->path));
        slot->valid = true;
        root_menu_video_menu_preview_clear_failure(load_path, dark);
    }

    root_menu_video_draw_menu_preview_slot(slot, x, y, w, h, true);
    return true;
}

static bool root_menu_video_prepare_sitekick_component(const char *load_path,
                                                       int width, int height)
{
    struct root_menu_video_menu_preview_slot *slot;
    int dark = root_menu_video_dark() ? 1 : 0;
    int rc;

    slot = root_menu_video_menu_preview_find(load_path, dark);
    if (slot)
        return true;
    if (root_menu_video_menu_preview_recent_failure(load_path, dark))
        return false;
    if (!file_exists(load_path))
    {
        root_menu_video_menu_preview_record_failure(load_path, dark);
        return false;
    }

    slot = root_menu_video_menu_preview_victim();
    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->valid = false;
    slot->path[0] = '\0';
    slot->dark = dark;
    slot->bm.width = width;
    slot->bm.height = height;
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;
    rc = read_bmp_file(load_path, &slot->bm, sizeof(slot->data),
                       FORMAT_NATIVE, NULL);
    if (rc < 0)
    {
        root_menu_video_menu_preview_record_failure(load_path, dark);
        return false;
    }
    strmemccpy(slot->path, load_path, sizeof(slot->path));
    slot->valid = true;
    root_menu_video_menu_preview_clear_failure(load_path, dark);
    return true;
}

static void root_menu_video_invalidate_sitekick_preview(void)
{
    static const char prefix[] = ROCKBOX_DIR "/sitekick/preview/";

    root_menu_video_preview_use_cache(IPODJS_PREVIEW_CACHE_MENU);
    for (int i = 0;
         i < (int)ARRAYLEN(root_menu_video_menu_preview_slots); i++)
    {
        struct root_menu_video_menu_preview_slot *slot =
            &root_menu_video_menu_preview_slots[i];

        if (slot->valid && !strncmp(slot->path, prefix, sizeof(prefix) - 1))
            slot->valid = false;
    }
    for (int i = 0;
         i < (int)ARRAYLEN(root_menu_video_menu_preview_failures); i++)
    {
        struct root_menu_video_menu_preview_failure *failure =
            &root_menu_video_menu_preview_failures[i];

        if (failure->valid &&
            !strncmp(failure->path, prefix, sizeof(prefix) - 1))
            failure->valid = false;
    }
}

static bool root_menu_video_prepare_sitekick_preview(void)
{
    bool background;
    bool character;

    /* RockPod atomically replaces these files during USB sync.  A native
     * screen entry is the safe point to decode the new loadout; drawing the
     * floating animation itself remains disk-free and cache-only. */
    root_menu_video_invalidate_sitekick_preview();
    background = root_menu_video_prepare_sitekick_component(
        ROCKBOX_DIR "/sitekick/preview/pane-background.bmp", 174, LCD_HEIGHT);
    character = root_menu_video_prepare_sitekick_component(
        ROCKBOX_DIR "/sitekick/preview/current-float.bmp", 154, 139);

    if (background && character)
        return true;

    /* Packages from before the floating-pane split still get a static
     * current Sitekick instead of an empty right pane. */
    return root_menu_video_prepare_sitekick_component(
        ROCKBOX_DIR "/sitekick/preview/current.bmp", 174, LCD_HEIGHT);
}

static bool root_menu_video_draw_sitekick_preview(int x, int y, int w, int h)
{
    static const signed char float_y[16] = {
         0, -1, -2, -3, -3, -2, -1,  0,
         1,  2,  3,  3,  2,  1,  0, -1,
    };
    int dark = root_menu_video_dark() ? 1 : 0;
    struct root_menu_video_menu_preview_slot *background =
        root_menu_video_menu_preview_find(
            ROCKBOX_DIR "/sitekick/preview/pane-background.bmp", dark);
    struct root_menu_video_menu_preview_slot *character =
        root_menu_video_menu_preview_find(
            ROCKBOX_DIR "/sitekick/preview/current-float.bmp", dark);

    if (background && character)
    {
        int phase = (current_tick / MAX(1, HZ / 12)) %
                    (int)ARRAYLEN(float_y);

        root_menu_video_draw_menu_preview_slot(background, x, y, w, h, true);
        root_menu_video_draw_menu_preview_slot(
            character, x + 10, y + 40 + float_y[phase],
            MAX(0, w - 10), MAX(0, h - 37), true);
        return true;
    }
    else
    {
        struct root_menu_video_menu_preview_slot *fallback =
            root_menu_video_menu_preview_find(
                ROCKBOX_DIR "/sitekick/preview/current.bmp", dark);
        if (!fallback)
            return false;
        root_menu_video_draw_menu_preview_slot(fallback, x, y, w, h, true);
        return true;
    }
}

static void root_menu_video_draw_stock_clock_preview(int x, int y, int w,
                                                     int h)
{
    static const short tick_x[12] = {
          0,  25,  43,  50,  43,  25,   0, -25, -43, -50, -43, -25
    };
    static const short tick_y[12] = {
        -50, -43, -25,   0,  25,  43,  50,  43,  25,   0, -25, -43
    };
    struct tm *tm = get_time();
    int cx = x + w / 2;
    int cy = y + 79;
    int radius = MIN(55, w / 2 - 10);
    bool stock_bg = root_menu_video_draw_clock_pane_background(x, y, w, h);
    unsigned face = root_menu_video_dark() ?
        LCD_RGBPACK(42, 46, 54) : LCD_RGBPACK(248, 249, 250);
    unsigned tick = root_menu_video_dark() ?
        LCD_RGBPACK(232, 234, 238) : LCD_RGBPACK(38, 42, 48);
    unsigned second = LCD_RGBPACK(218, 40, 46);

    if (stock_bg)
    {
        /* The stock pane keeps a light clock face on its dark background
         * in both themes. */
        face = LCD_RGBPACK(248, 249, 250);
        tick = LCD_RGBPACK(38, 42, 48);
    }
    else
        root_menu_video_preview_gradient(x, y, w, h);

    root_menu_video_fill_circle(cx + 2, cy + 3, radius + 3,
        (stock_bg || root_menu_video_dark()) ? LCD_RGBPACK(12, 14, 18) :
                                 LCD_RGBPACK(72, 78, 88));
    root_menu_video_fill_circle(cx, cy, radius + 3,
                                 LCD_RGBPACK(176, 183, 193));
    root_menu_video_fill_circle(cx, cy, radius + 1,
                                 LCD_RGBPACK(224, 228, 234));
    root_menu_video_fill_circle(cx, cy, radius - 3, face);

    for (int i = 0; i < 12; i++)
    {
        int outer_x = cx + tick_x[i] * (radius - 7) / 50;
        int outer_y = cy + tick_y[i] * (radius - 7) / 50;
        int inner_x = cx + tick_x[i] * (radius - 14) / 50;
        int inner_y = cy + tick_y[i] * (radius - 14) / 50;

        lcd_set_foreground(tick);
        lcd_drawline(inner_x, inner_y, outer_x, outer_y);
    }

    if (tm)
    {
        int hour_position = (tm->tm_hour % 12) * 5 + tm->tm_min / 12;
        root_menu_video_clock_hand(cx, cy, hour_position,
                                   radius - 25, tick, true);
        root_menu_video_clock_hand(cx, cy, tm->tm_min,
                                   radius - 15, tick, true);
        root_menu_video_clock_hand(cx, cy, tm->tm_sec,
                                   radius - 10, second, false);
    }

    root_menu_video_fill_circle(cx, cy, 4, tick);
    root_menu_video_fill_circle(cx, cy, 2, LCD_RGBPACK(104, 174, 222));
    root_menu_video_draw_clock_date(x, y, w, h, false);
}

static bool root_menu_video_hold_update(bool *held, bool *redraw)
{
    bool now = button_hold();

    if (now != *held)
    {
        root_menu_video_hold_static_frame_valid = false;
        albumlist_slideshow_set_paused(now);
        IPODJS_LOGF("ipodjs: hold=%d paused=%d status=%d",
                    now ? 1 : 0,
                    (audio_status() & AUDIO_STATUS_PAUSE) ? 1 : 0,
                    audio_status());
        if (!root_menu_video_hold_storm_window_start ||
            TIME_AFTER(current_tick,
                       root_menu_video_hold_storm_window_start + HZ))
        {
            root_menu_video_hold_storm_window_start = current_tick;
            root_menu_video_hold_storm_toggles = 1;
        }
        else
        {
            root_menu_video_hold_storm_toggles++;
            if (root_menu_video_hold_storm_toggles >= 4)
            {
                root_menu_video_hold_storm_until = current_tick + HZ * 2;
                root_menu_video_hold_storm_window_start = 0;
                root_menu_video_hold_storm_toggles = 0;
            }
        }

        *held = now;
        *redraw = true;
        button_clear_queue();
    }

    return now;
}

static bool root_menu_video_status_bluetooth_valid;
static bool root_menu_video_status_bluetooth_drawn;

#ifdef IPOD_ACCESSORY_PROTOCOL
static bool root_menu_video_bluetooth_ready(void)
{
    return iap_kokkia_present();
}
#endif

static void root_menu_video_draw_status_title_width(const char *title,
                                                     int width,
                                                     bool pane_mode)
{
    int batt = battery_level();
    int batt_x = width - 31;
    int icon_x = batt_x - 19;
    int bluetooth_x = icon_x - 16;
    bool bluetooth = false;
    bool internet = usb_internet_connected();

#ifdef IPOD_ACCESSORY_PROTOCOL
    bluetooth = !internet && iap_kokkia_present();
#endif
    root_menu_video_status_bluetooth_drawn = bluetooth;
    root_menu_video_status_bluetooth_valid = true;

    ipodjs_ui_draw_header_background(&screens[SCREEN_MAIN], width);
    if (root_menu_video_dark())
    {
        lcd_set_foreground(LCD_RGBPACK(11, 14, 19));
        lcd_hline(0, width - 1, IPODJS_HEADER_HEIGHT - 1);
    }

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_header_text());
    lcd_set_background(root_menu_video_header_bg());
    if (pane_mode)
    {
        root_menu_video_puts_fit(6, 5 + root_menu_video_text_y_offset(),
                                 (bluetooth || internet) ? bluetooth_x - 12 :
                                 icon_x - 10,
                                 "iPod", false);

        if (button_hold())
            ipodjs_ui_draw_hold_indicator(&screens[SCREEN_MAIN], icon_x, 4);
        else if (audio_status() & AUDIO_STATUS_PLAY)
            root_menu_video_draw_play_icon(icon_x, 4,
                audio_status() & AUDIO_STATUS_PAUSE);
    }
    else
    {
        root_menu_video_puts_fit(6, 5 + root_menu_video_text_y_offset(),
                                 (bluetooth || internet) ? bluetooth_x - 12 :
                                 width - 52,
                                 title ? title : "iPod",
                                 false);

        if (button_hold())
            ipodjs_ui_draw_hold_indicator(&screens[SCREEN_MAIN], icon_x, 4);
        else if (audio_status() & AUDIO_STATUS_PLAY)
        {
            root_menu_video_draw_play_icon(icon_x, 4,
                audio_status() & AUDIO_STATUS_PAUSE);
        }
    }

    if (bluetooth)
        ipodjs_ui_draw_bluetooth_indicator(&screens[SCREEN_MAIN],
                                           bluetooth_x, 2);
    else if (internet)
        ipodjs_ui_draw_wifi_indicator(&screens[SCREEN_MAIN],
                                      bluetooth_x - 5, 3);
    root_menu_video_draw_battery(batt_x, 5, batt, charger_inserted());
}

static void root_menu_video_draw_status_title(const char *title)
{
    root_menu_video_draw_status_title_width(title, LCD_WIDTH, false);
}

static void root_menu_video_draw_wps_status_title(const char *title)
{
    int batt_x = LCD_WIDTH - 31;
    int icon_x = batt_x - 19;
    int bluetooth_x = icon_x - 16;
    bool bluetooth = false;
    bool internet = usb_internet_connected();

#ifdef IPOD_ACCESSORY_PROTOCOL
    bluetooth = !internet && iap_kokkia_present();
#endif
    root_menu_video_status_bluetooth_drawn = bluetooth;
    root_menu_video_status_bluetooth_valid = true;

    ipodjs_ui_draw_header_background(&screens[SCREEN_MAIN], LCD_WIDTH);
    if (root_menu_video_dark())
    {
        lcd_set_foreground(LCD_RGBPACK(11, 14, 19));
        lcd_hline(0, LCD_WIDTH - 1, IPODJS_HEADER_HEIGHT - 1);
    }

    lcd_setfont(root_menu_video_wps_font(true));
    lcd_set_foreground(root_menu_video_header_text());
    lcd_set_background(root_menu_video_header_bg());
    root_menu_video_puts_fit(6, 5, LCD_WIDTH - 52,
                             title ? title : "Now Playing", false);

    if (button_hold())
        ipodjs_ui_draw_hold_indicator(&screens[SCREEN_MAIN], icon_x, 4);
    else if (audio_status() & AUDIO_STATUS_PLAY)
        ipodjs_ui_draw_playback_indicator(&screens[SCREEN_MAIN], icon_x, 4);

    if (bluetooth)
        ipodjs_ui_draw_bluetooth_indicator(&screens[SCREEN_MAIN],
                                           bluetooth_x, 2);
    else if (internet)
        ipodjs_ui_draw_wifi_indicator(&screens[SCREEN_MAIN],
                                      bluetooth_x - 5, 3);
    root_menu_video_draw_battery(batt_x, 5, battery_level(),
                                 charger_inserted());
}

static void root_menu_video_draw_wps_modes(void)
{
    const int y = IPODJS_HEADER_HEIGHT;
    unsigned panel = root_menu_video_panel();
    unsigned split = root_menu_video_dark() ?
        LCD_RGBPACK(54, 60, 70) : IPODJS_SPLIT;

    /* These Apple indicators sit immediately below the status bar.  Draw
     * them after the WPS surface so the body fill cannot erase them. */
    lcd_set_foreground(panel);
    lcd_fillrect(258, y, LCD_WIDTH - 258, 20);
    lcd_set_foreground(split);
    lcd_hline(258, LCD_WIDTH - 1, y);
    if (global_settings.playlist_shuffle)
        ipodjs_ui_draw_shuffle_indicator(&screens[SCREEN_MAIN], 264, y);
    if (global_settings.repeat_mode != REPEAT_OFF)
        ipodjs_ui_draw_repeat_indicator(&screens[SCREEN_MAIN], 290, y,
                                        global_settings.repeat_mode);
}

static void root_menu_video_draw_status(void)
{
    root_menu_video_draw_status_title_width("iPod", IPODJS_LIST_WIDTH,
                                            true);
}

static void root_menu_video_draw_arrow(int x, int y)
{
    ipodjs_ui_draw_arrow(&screens[SCREEN_MAIN], x, y, IPODJS_PREVIEW_TEXT);
}

static void root_menu_video_draw_preview(int selected)
{
    int count = root_menu_video_count();
    const struct menu_item_ex *item = NULL;
    const char *title = "iPod";
    int x = IPODJS_PREVIEW_X;
    int w = LCD_WIDTH - x;
    int y = 0;
    int h = LCD_HEIGHT - y;

    if (selected >= 0 && selected < count)
    {
        item = root_menu_video_item(selected);
        title = root_menu_video_preview(item);
    }

    if (root_menu_video_item_is_settings(item))
    {
        root_menu_video_preview_gradient(x, y, w, h);
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(IPODJS_PREVIEW_TEXT);
        lcd_set_background(IPODJS_PREVIEW_BOTTOM);
        root_menu_video_draw_settings_preview(x, y, w, h, false);
    }
    else
    {
        root_menu_video_draw_preview_for_title(title, x, y, w, h);
    }
}

struct root_menu_video_weather {
    bool available;
    bool hourly;
    bool live;
    bool night;
    long checked_tick;
    char icon[24];
    char condition[32];
    char location[32];
    char temp[16];
};

static struct root_menu_video_weather root_menu_video_weather_cache;
static struct bitmap root_menu_video_weather_icon_bm;
static unsigned char root_menu_video_weather_icon_data[
    BM_SIZE(40, 40, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
static char root_menu_video_weather_icon_path[MAX_PATH];
static bool root_menu_video_weather_icon_valid;
static char root_menu_video_weather_icon_miss[32];
static bool root_menu_video_weather_icon_miss_night;
static long root_menu_video_weather_icon_retry;

static int root_menu_video_weather_stamp_key(const char *stamp)
{
    char buf[5];
    int year;
    int month;
    int day;
    int hour;

    if (!stamp || strlen(stamp) < 13)
        return -1;

    memcpy(buf, stamp, 4);
    buf[4] = '\0';
    year = atoi(buf);
    memcpy(buf, stamp + 5, 2);
    buf[2] = '\0';
    month = atoi(buf);
    memcpy(buf, stamp + 8, 2);
    buf[2] = '\0';
    day = atoi(buf);
    memcpy(buf, stamp + 11, 2);
    buf[2] = '\0';
    hour = atoi(buf);

    if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23)
        return -1;

    return (((year * 100) + month) * 100 + day) * 100 + hour;
}

static int root_menu_video_weather_now_key(void)
{
    struct tm *tm = get_time();

    if (!tm || !valid_time(tm))
        return -1;

    return ((((tm->tm_year + 1900) * 100 + tm->tm_mon + 1) * 100 +
             tm->tm_mday) * 100 + tm->tm_hour);
}

static void root_menu_video_copy_tsv_field(char **cursor, char *dst,
                                           size_t dst_size)
{
    char *start = *cursor;
    char *end;
    size_t len;

    if (!start || !dst || dst_size == 0)
        return;

    end = start;
    while (*end && *end != '\t' && *end != '\n' && *end != '\r')
        end++;

    len = MIN((size_t)(end - start), dst_size - 1);
    memcpy(dst, start, len);
    dst[len] = '\0';
    *cursor = *end == '\t' ? end + 1 : end;
}

static const char *root_menu_video_weather_icon_name(const char *icon,
                                                     bool night)
{
    if (icon && strstr(icon, "clear"))
        return night ? "clear_night" : "clear_day";
    if (icon && strstr(icon, "partly_cloudy"))
        return "partly_cloudy";
    if (icon && strstr(icon, "cloudy"))
        return "cloudy";
    if (icon && strstr(icon, "drizzle"))
        return "drizzle";
    if (icon && strstr(icon, "rain"))
        return "rain";
    if (icon && strstr(icon, "snow"))
        return "snow";
    if (icon && strstr(icon, "fog"))
        return "fog";
    if (icon && strstr(icon, "thunder"))
        return "thunderstorm";
    return "unknown";
}

static struct bitmap *root_menu_video_weather_icon_asset(const char *icon,
                                                         bool night)
{
    char path[MAX_PATH];
    char fallback_path[MAX_PATH];
    const char *name = root_menu_video_weather_icon_name(icon, night);
    int rc;

    if (root_menu_video_weather_icon_miss[0] &&
        root_menu_video_weather_icon_miss_night == night &&
        !strcmp(root_menu_video_weather_icon_miss, name) &&
        !TIME_AFTER(current_tick, root_menu_video_weather_icon_retry))
    {
        return NULL;
    }

    snprintf(path, sizeof(path), ROCKBOX_DIR
             "/rockpod/weather/apple-icons/%s.40x40x24.bmp", name);
    if (!file_exists(path))
    {
        snprintf(fallback_path, sizeof(fallback_path), ROCKBOX_DIR
                 "/rockpod/weather/icons/%s.40x40x24.bmp", name);
        if (file_exists(fallback_path))
            strmemccpy(path, fallback_path, sizeof(path));
        else
        {
            strmemccpy(root_menu_video_weather_icon_miss, name,
                       sizeof(root_menu_video_weather_icon_miss));
            root_menu_video_weather_icon_miss_night = night;
            root_menu_video_weather_icon_retry = current_tick + HZ * 600;
            return NULL;
        }
    }

    if (root_menu_video_weather_icon_valid &&
        !strcmp(path, root_menu_video_weather_icon_path))
        return &root_menu_video_weather_icon_bm;

    root_menu_video_weather_icon_valid = false;
    root_menu_video_weather_icon_path[0] = '\0';

    memset(&root_menu_video_weather_icon_bm, 0,
           sizeof(root_menu_video_weather_icon_bm));
    root_menu_video_weather_icon_bm.width = 40;
    root_menu_video_weather_icon_bm.height = 40;
    root_menu_video_weather_icon_bm.format = FORMAT_NATIVE;
    root_menu_video_weather_icon_bm.data = root_menu_video_weather_icon_data;
    rc = read_bmp_file(path, &root_menu_video_weather_icon_bm,
                       sizeof(root_menu_video_weather_icon_data),
                       FORMAT_NATIVE | FORMAT_TRANSPARENT, NULL);
    if (rc < 0)
    {
        strmemccpy(root_menu_video_weather_icon_miss, name,
                   sizeof(root_menu_video_weather_icon_miss));
        root_menu_video_weather_icon_miss_night = night;
        root_menu_video_weather_icon_retry = current_tick + HZ * 600;
        return NULL;
    }

    fb_data *pixels = (fb_data *)root_menu_video_weather_icon_bm.data;
    int count = root_menu_video_weather_icon_bm.width *
                root_menu_video_weather_icon_bm.height;
    for (int i = 0; i < count; i++)
    {
        unsigned px = pixels[i];
        int r = FB_UNPACK_RED(px);
        int g = FB_UNPACK_GREEN(px);
        int b = FB_UNPACK_BLUE(px);

        if (r >= 170 && b >= 190 && g + 18 < r && g + 18 < b)
            pixels[i] = TRANSPARENT_COLOR;
    }

    strmemccpy(root_menu_video_weather_icon_path, path,
               sizeof(root_menu_video_weather_icon_path));
    root_menu_video_weather_icon_valid = true;
    root_menu_video_weather_icon_miss[0] = '\0';
    return &root_menu_video_weather_icon_bm;
}

static bool root_menu_video_load_weather(void)
{
    char line[512];
    char units[12] = "metric";
    char min_temp[8] = "";
    char max_temp[8] = "";
    char skip[64];
    char *next;
    int best_past_key = -1;
    int best_future_key = -1;
    int now_key;
    int fd;

    if (root_menu_video_weather_cache.checked_tick &&
        TIME_BEFORE(current_tick,
                    root_menu_video_weather_cache.checked_tick + HZ * 60))
        return root_menu_video_weather_cache.available &&
               (!usb_internet_connected() ||
                root_menu_video_weather_cache.live);

    root_menu_video_weather_cache.checked_tick = current_tick;
    root_menu_video_weather_cache.available = false;
    root_menu_video_weather_cache.hourly = false;
    root_menu_video_weather_cache.live = false;
    root_menu_video_weather_cache.night = false;
    root_menu_video_weather_cache.icon[0] = '\0';
    root_menu_video_weather_cache.condition[0] = '\0';
    root_menu_video_weather_cache.location[0] = '\0';
    root_menu_video_weather_cache.temp[0] = '\0';

    fd = open(ROCKBOX_DIR "/rockpod/weather/forecast.tsv", O_RDONLY);
    if (fd < 0)
        return false;

    if (read_line(fd, line, sizeof(line)) <= 0)
    {
        close(fd);
        return false;
    }

    next = line;
    root_menu_video_copy_tsv_field(&next, skip, sizeof(skip));
    if (strcmp(skip, "rockpod_weather_v1"))
    {
        close(fd);
        return false;
    }
    root_menu_video_copy_tsv_field(&next,
        root_menu_video_weather_cache.location,
        sizeof(root_menu_video_weather_cache.location));
    root_menu_video_copy_tsv_field(&next, skip, sizeof(skip));
    root_menu_video_copy_tsv_field(&next, skip, sizeof(skip));
    root_menu_video_copy_tsv_field(&next, skip, sizeof(skip));
    root_menu_video_copy_tsv_field(&next, skip, sizeof(skip));
    root_menu_video_copy_tsv_field(&next, skip, sizeof(skip));
    root_menu_video_copy_tsv_field(&next, units, sizeof(units));

    now_key = root_menu_video_weather_now_key();
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char first[24];

        next = line;
        root_menu_video_copy_tsv_field(&next, first, sizeof(first));
        if (!first[0])
            continue;

        if (!strcmp(first, "current") || !strcmp(first, "hourly"))
        {
            char stamp[24];
            char icon[24];
            char condition[32];
            char temp[8];
            char precip[8];
            char wind[8];
            char wind_dir[8];
            char is_day[4];
            int key;
            bool use = false;

            root_menu_video_copy_tsv_field(&next, stamp, sizeof(stamp));
            root_menu_video_copy_tsv_field(&next, icon, sizeof(icon));
            root_menu_video_copy_tsv_field(&next, condition,
                                           sizeof(condition));
            root_menu_video_copy_tsv_field(&next, temp, sizeof(temp));
            root_menu_video_copy_tsv_field(&next, precip, sizeof(precip));
            root_menu_video_copy_tsv_field(&next, wind, sizeof(wind));
            root_menu_video_copy_tsv_field(&next, wind_dir,
                                           sizeof(wind_dir));
            root_menu_video_copy_tsv_field(&next, is_day,
                                           sizeof(is_day));

            if (!temp[0] || !condition[0])
                continue;

            if (!strcmp(first, "current"))
            {
                strmemccpy(root_menu_video_weather_cache.icon, icon,
                           sizeof(root_menu_video_weather_cache.icon));
                strmemccpy(root_menu_video_weather_cache.condition,
                           condition,
                           sizeof(root_menu_video_weather_cache.condition));
                snprintf(root_menu_video_weather_cache.temp,
                         sizeof(root_menu_video_weather_cache.temp), "%s %c",
                         temp, units[0] == 'i' ? 'F' : 'C');
                root_menu_video_weather_cache.available = true;
                root_menu_video_weather_cache.hourly = true;
                root_menu_video_weather_cache.live = true;
                root_menu_video_weather_cache.night =
                    is_day[0] && atoi(is_day) == 0;
                continue;
            }

            if (root_menu_video_weather_cache.live)
                continue;

            key = root_menu_video_weather_stamp_key(stamp);
            if (now_key < 0)
                use = !root_menu_video_weather_cache.hourly;
            else if (key <= now_key && key > best_past_key)
            {
                best_past_key = key;
                use = true;
            }
            else if (best_past_key < 0 && key > now_key &&
                     (best_future_key < 0 || key < best_future_key))
            {
                best_future_key = key;
                use = true;
            }

            if (use)
            {
                strmemccpy(root_menu_video_weather_cache.icon, icon,
                           sizeof(root_menu_video_weather_cache.icon));
                strmemccpy(root_menu_video_weather_cache.condition,
                           condition,
                           sizeof(root_menu_video_weather_cache.condition));
                snprintf(root_menu_video_weather_cache.temp,
                         sizeof(root_menu_video_weather_cache.temp), "%s %c",
                         temp, units[0] == 'i' ? 'F' : 'C');
                root_menu_video_weather_cache.hourly = true;
                root_menu_video_weather_cache.night =
                    is_day[0] && atoi(is_day) == 0;
            }
            continue;
        }

        if (!root_menu_video_weather_cache.available)
        {
            strmemccpy(skip, first, sizeof(skip));
            root_menu_video_copy_tsv_field(&next,
                root_menu_video_weather_cache.icon,
                sizeof(root_menu_video_weather_cache.icon));
            root_menu_video_copy_tsv_field(&next,
                root_menu_video_weather_cache.condition,
                sizeof(root_menu_video_weather_cache.condition));
            root_menu_video_copy_tsv_field(&next, min_temp,
                                           sizeof(min_temp));
            root_menu_video_copy_tsv_field(&next, max_temp,
                                           sizeof(max_temp));
            snprintf(root_menu_video_weather_cache.temp,
                     sizeof(root_menu_video_weather_cache.temp), "%s-%s %c",
                     min_temp, max_temp, units[0] == 'i' ? 'F' : 'C');
            root_menu_video_weather_cache.night = false;
            root_menu_video_weather_cache.available =
                root_menu_video_weather_cache.condition[0] != '\0';
        }
    }

    close(fd);
    root_menu_video_weather_cache.available =
        root_menu_video_weather_cache.condition[0] != '\0';
    if (usb_internet_connected() && !root_menu_video_weather_cache.live)
        root_menu_video_weather_cache.available = false;
    return root_menu_video_weather_cache.available;
}

static bool root_menu_video_draw_weather_icon(int x, int y, const char *icon,
                                              bool night)
{
    struct bitmap *asset = root_menu_video_weather_icon_asset(icon, night);

    if (asset)
    {
        lcd_bitmap_transparent((fb_data *)asset->data, x, y,
                               asset->width, asset->height);
        return true;
    }

    return false;
}

static bool root_menu_video_draw_lock_weather(int y)
{
    if (!root_menu_video_load_weather())
        return false;

    if (root_menu_video_draw_weather_icon(LCD_WIDTH / 2 - 20, y,
            root_menu_video_weather_cache.icon,
            root_menu_video_weather_cache.night))
        y += 42;

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(LCD_RGBPACK(42, 48, 56));
    lcd_set_background(IPODJS_LOCK_BOTTOM);
    root_menu_video_puts_fit(30, y, LCD_WIDTH - 60,
                             root_menu_video_weather_cache.temp, true);
    lcd_set_foreground(LCD_RGBPACK(68, 76, 88));
    root_menu_video_puts_fit(30, y + 14, LCD_WIDTH - 60,
                             root_menu_video_weather_cache.condition, true);
    return true;
}

static void root_menu_video_draw_clock_lock(void)
{
    static const char * const wdays[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    static const char * const months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    struct tm *tm = get_time();
    char clock[12];
    char date[48];
    char battery[24];
    int hour;

    root_menu_video_lock_gradient(0, 0, LCD_WIDTH, LCD_HEIGHT);
    root_menu_video_draw_status_title("HOLD");

    if (!tm)
        goto trace;

    hour = tm->tm_hour % 12;
    if (hour == 0)
        hour = 12;
    snprintf(clock, sizeof(clock), "%d:%02d %s", hour, tm->tm_min,
             tm->tm_hour >= 12 ? "PM" : "AM");
    lcd_setfont(root_menu_video_lock_font());
    lcd_set_foreground(LCD_RGBPACK(38, 43, 50));
    lcd_set_background(IPODJS_LOCK_MID);
    root_menu_video_puts_fit(30, 58, LCD_WIDTH - 60, clock, true);

    snprintf(date, sizeof(date), "%s, %s %d",
             wdays[MAX(0, MIN(tm->tm_wday, 6))],
             months[MAX(0, MIN(tm->tm_mon, 11))],
             tm->tm_mday);
    snprintf(battery, sizeof(battery), "Battery %d%%", battery_level());

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(LCD_RGBPACK(48, 55, 64));
    lcd_set_background(IPODJS_LOCK_BOTTOM);
    root_menu_video_puts_fit(30, 122, LCD_WIDTH - 60, date, true);
    if (root_menu_video_draw_lock_weather(144))
        root_menu_video_puts_fit(30, 220, LCD_WIDTH - 60, battery, true);
    else
    {
        root_menu_video_puts_fit(30, 174, LCD_WIDTH - 60, battery, true);
        lcd_set_foreground(LCD_RGBPACK(80, 88, 100));
        root_menu_video_puts_fit(30, 214, LCD_WIDTH - 60, "Hold", true);
    }

trace:
    ipodjs_trace_screen("Lockscreen", "full", 0, 0, 0,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

struct root_menu_video_list_draw_state {
    int selected;
    int top;
    int row_h;
    int visible;
    bool valid;
};

static int root_menu_video_list_top(int selected, int visible)
{
    if (selected >= visible)
        return selected - visible + 1;

    return 0;
}

static void root_menu_video_draw_list_row(int index, int screen_row,
                                          int row_h, bool active)
{
    int y = IPODJS_HEADER_HEIGHT + screen_row * row_h;
    const char *label = root_menu_video_label(root_menu_video_item(index));

    if (active)
    {
        unsigned accent = root_menu_video_accent();
        root_menu_video_selection_gradient(0, y, IPODJS_LIST_WIDTH, row_h);
        lcd_set_foreground(IPODJS_PREVIEW_TEXT);
        lcd_set_background(accent);
    }
    else
    {
        lcd_set_foreground(root_menu_video_row_bg());
        lcd_fillrect(0, y, IPODJS_LIST_WIDTH, row_h);
        lcd_set_foreground(root_menu_video_text());
        lcd_set_background(root_menu_video_row_bg());
    }

    root_menu_video_puts_fit(6, y + 4, IPODJS_LIST_WIDTH - 24,
                             label, false);
    if (active)
        root_menu_video_draw_arrow(IPODJS_LIST_WIDTH - 13,
                                   y + (row_h - 6) / 2);
}

static void root_menu_video_draw_list(int selected,
                                      struct root_menu_video_list_draw_state
                                      *state)
{
    int count = root_menu_video_count();
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int top = root_menu_video_list_top(selected, visible);
    int i;

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_row_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, IPODJS_LIST_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    for (i = 0; i < visible && top + i < count; i++)
    {
        int index = top + i;
        root_menu_video_draw_list_row(index, i, row_h, index == selected);
    }

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(58, 64, 74) : IPODJS_SPLIT);
    lcd_vline(IPODJS_SPLIT_X, 0, LCD_HEIGHT - 1);

    if (state)
    {
        state->selected = selected;
        state->top = top;
        state->row_h = row_h;
        state->visible = visible;
        state->valid = true;
    }
}

static void root_menu_video_draw_native_row(const char *label, int screen_row,
                                            int row_h, int width, bool active)
{
    int y = IPODJS_HEADER_HEIGHT + screen_row * row_h;

    if (active)
    {
        unsigned accent = root_menu_video_accent();
        root_menu_video_selection_gradient(0, y, width, row_h);
        lcd_set_foreground(IPODJS_PREVIEW_TEXT);
        lcd_set_background(accent);
    }
    else
    {
        lcd_set_foreground(root_menu_video_row_bg());
        lcd_fillrect(0, y, width, row_h);
        lcd_set_foreground(root_menu_video_text());
        lcd_set_background(root_menu_video_row_bg());
    }

    root_menu_video_puts_fit(6, y + 4, width - 24,
                             label ? label : "", false);
    if (active)
        root_menu_video_draw_arrow(width - 13,
                                   y + (row_h - 6) / 2);
}

static bool root_menu_video_draw_native_pane_delta(int previous_selected,
                                                   int selected, int count,
                                                   const char *previous_label,
                                                   const char *selected_label,
                                                   bool settings_preview)
{
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int previous_top = root_menu_video_list_top(previous_selected, visible);
    int selected_top = root_menu_video_list_top(selected, visible);
    int previous_row;
    int selected_row;
    int preview_x = IPODJS_PREVIEW_X;

    (void)count;
    if (previous_top != selected_top)
        return false;

    previous_row = previous_selected - selected_top;
    selected_row = selected - selected_top;
    lcd_setfont(root_menu_video_font());
    root_menu_video_draw_native_row(previous_label, previous_row, row_h,
                                    IPODJS_LIST_WIDTH, false);
    root_menu_video_draw_native_row(selected_label, selected_row, row_h,
                                    IPODJS_LIST_WIDTH, true);

    if (settings_preview)
        root_menu_video_draw_settings_preview(preview_x, 0,
            LCD_WIDTH - preview_x, LCD_HEIGHT, false);
    else
        root_menu_video_draw_preview_for_title(selected_label, preview_x, 0,
            LCD_WIDTH - preview_x, LCD_HEIGHT);

    lcd_update_rect(0, IPODJS_HEADER_HEIGHT + MIN(previous_row, selected_row) *
                    row_h, IPODJS_LIST_WIDTH,
                    (abs(previous_row - selected_row) + 1) * row_h);
    lcd_update_rect(preview_x, 0, LCD_WIDTH - preview_x, LCD_HEIGHT);
    return true;
}

static void ipodjs_video_draw_hold_overlay(void)
{
    if (!button_hold())
        return;

    /* Preview and scrolling helpers may leave a pane-sized viewport active.
     * Hold is a full-screen system presentation on every submenu. */
    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    if (global_settings.ui_engine_hold_effect == UI_ENGINE_HOLD_LOCKSCREEN)
    {
        /* Keep one lock presentation everywhere.  The stock-like clock and
         * weather view must not disappear merely because music is playing. */
        root_menu_video_draw_clock_lock();
        return;
    }
    else
    {
        unsigned overlay = root_menu_video_dark() ?
            LCD_RGBPACK(26, 29, 34) : LCD_RGBPACK(54, 58, 66);
        int y = LCD_HEIGHT - 48;

        root_menu_video_draw_status_title("HOLD");
        lcd_set_foreground(overlay);
        lcd_fillrect(0, y, LCD_WIDTH, 48);
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(IPODJS_PREVIEW_TEXT);
        lcd_set_background(overlay);
        root_menu_video_puts_fit(18, y + 7, LCD_WIDTH - 36, "Hold", true);
        lcd_set_foreground(LCD_RGBPACK(190, 194, 200));
        root_menu_video_puts_fit(18, y + 27, LCD_WIDTH - 36,
                                 "Controls Locked", true);
    }
}

bool root_menu_ipodjs_handle_lockscreen(void)
{
    long next_refresh = 0;

    if (!root_menu_video_enabled() || !button_hold())
        return false;

    root_menu_video_hold_static_frame_valid = false;
    albumlist_slideshow_set_paused(true);
    button_clear_queue();

    while (button_hold())
    {
        int action;

        if (!next_refresh || TIME_AFTER(current_tick, next_refresh))
        {
            ipodjs_video_draw_hold_overlay();
            lcd_update();
            next_refresh = current_tick + HZ;
        }

        action = get_action(CONTEXT_STD|ALLOW_SOFTLOCK, HZ/5);
        if (IS_SYSEVENT(action))
            default_event_handler(action);
    }

    albumlist_slideshow_set_paused(false);
    root_menu_video_hold_static_frame_valid = false;
    button_clear_queue();
    return true;
}

static void ipodjs_video_draw_wps_progress(struct mp3entry *id3,
                                           unsigned accent)
{
    char elapsed[16];
    char remaining[16];
    int progress_x = 60;
    int progress_w = 200;
    int progress_y = 198;
    unsigned panel = root_menu_video_panel();
    unsigned time_color = root_menu_video_dark() ?
        LCD_RGBPACK(225, 228, 233) : LCD_RGBPACK(28, 28, 28);

    root_menu_video_duration(elapsed, sizeof(elapsed),
                             id3 ? id3->elapsed : 0);
    root_menu_video_remaining_duration(remaining, sizeof(remaining), id3);
    if (!root_menu_video_draw_apple_track(progress_x, progress_y,
                                          root_menu_video_elapsed_percent(),
                                          IPODJS_APPLE_PROGRESS_FRAME))
    {
        root_menu_video_draw_stock_meter(progress_x, progress_y + 3,
                                         progress_w, 13,
                                         root_menu_video_elapsed_percent(),
                                         accent);
    }

    lcd_setfont(root_menu_video_wps_font(false));
    lcd_set_foreground(time_color);
    lcd_set_background(panel);
    root_menu_video_puts_fit(10, progress_y + 4,
                             progress_x - 16,
                             elapsed, false);
    root_menu_video_puts_fit(progress_x + progress_w + 6, progress_y + 4,
                             LCD_WIDTH - progress_x - progress_w - 10,
                             remaining, true);

}

static void root_menu_video_draw_wps_star(int x, int y, unsigned color)
{
    lcd_set_foreground(color);
    lcd_hline(x + 4, x + 4, y);
    lcd_hline(x + 3, x + 5, y + 1);
    lcd_hline(x, x + 8, y + 2);
    lcd_hline(x + 1, x + 7, y + 3);
    lcd_hline(x + 2, x + 6, y + 4);
    lcd_hline(x + 1, x + 7, y + 5);
    lcd_hline(x + 1, x + 3, y + 6);
    lcd_hline(x + 5, x + 7, y + 6);
}

static int root_menu_video_draw_wps_rating(struct mp3entry *id3,
                                           int x, int y,
                                           unsigned color)
{
    int stars = 0;
    int i;

#ifdef HAVE_TAGCACHE
    if (id3)
        stars = MIN(5, MAX(0, (id3->rating + 1) / 2));
#else
    (void)id3;
#endif
    for (i = 0; i < stars; i++)
        root_menu_video_draw_wps_star(x + i * 12, y, color);

    return stars;
}

static void ipodjs_video_draw_wps_full(void)
{
    struct mp3entry *id3 = audio_current_track();
    unsigned accent = root_menu_video_accent();
    unsigned panel = root_menu_video_panel();
    unsigned text = root_menu_video_text();
    unsigned split = root_menu_video_dark() ?
        LCD_RGBPACK(54, 60, 70) : IPODJS_SPLIT;
    int art_x = 15;
    int art_y = 34;
    int info_x = 157;
    int info_w = LCD_WIDTH - info_x - 10;
    int playlist_index = playlist_get_display_index();
    int playlist_count = playlist_amount();
    int title_font = root_menu_video_wps_font(true);
    int detail_font = root_menu_video_wps_font(false);
    int title_height = font_get(title_font)->height;
    int detail_height = font_get(detail_font)->height;
    int title_y;
    int artist_y;
    int album_y;
    int sequence_y;
    int rating_y;
    int rating_stars;
    char sequence[24];
    bool volume_active = root_menu_video_volume_overlay_active();
    const char *artist = id3 && id3->artist ? id3->artist : "";
    const char *album = id3 && id3->album ? id3->album : "";

    root_menu_video_stop_wps_title_scroll();
    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_wps_status_title("Now Playing");

    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_TRANSPARENT)
    {
        if (root_menu_video_dark())
            root_menu_video_gradient(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                LCD_HEIGHT - IPODJS_HEADER_HEIGHT,
                LCD_RGBPACK(32, 37, 46), LCD_RGBPACK(17, 21, 28));
        else
            root_menu_video_gradient(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                LCD_HEIGHT - IPODJS_HEADER_HEIGHT,
                root_menu_video_rgb_blend(255, 255, 255, 248, 249, 250, 190),
                root_menu_video_rgb_blend(255, 255, 255, 225, 229, 235, 160));
    }
    else
    {
        lcd_set_foreground(panel);
        lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                     LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
    }

    lcd_set_foreground(split);
    lcd_hline(0, LCD_WIDTH - 1, IPODJS_HEADER_HEIGHT);
    root_menu_video_draw_wps_modes();

    bool art_drawn = root_menu_video_draw_stock_wps_art(art_x, art_y, id3);
    if (!art_drawn)
    {
        info_x = 24;
        info_w = LCD_WIDTH - 48;
    }

    title_y = art_drawn ? 58 : 66;
    artist_y = title_y + title_height + 4;
    album_y = artist_y + detail_height + 3;
    rating_y = album_y + detail_height + 7;

    lcd_set_foreground(text);
    lcd_set_background(panel);
    lcd_setfont(title_font);
    root_menu_video_draw_wps_title(info_x, title_y, info_w,
                                   root_menu_video_now_title(), title_font,
                                   text, panel, !art_drawn);
    lcd_set_foreground(text);
    lcd_setfont(detail_font);
    root_menu_video_puts_fit(info_x, artist_y, info_w,
                             artist, !art_drawn);
    root_menu_video_puts_fit(info_x, album_y, info_w,
                             album, !art_drawn);

    rating_stars = root_menu_video_draw_wps_rating(id3,
        art_drawn ? info_x : (LCD_WIDTH - 57) / 2,
        rating_y, text);
    sequence_y = rating_stars > 0 ? rating_y + 12 : rating_y;

    if (playlist_index > 0 && playlist_count > 0)
    {
        snprintf(sequence, sizeof(sequence), "%d of %d",
                 playlist_index, playlist_count);
        lcd_set_foreground(text);
        lcd_set_background(panel);
        lcd_setfont(detail_font);
        root_menu_video_puts_fit(info_x, sequence_y, info_w, sequence,
                                 !art_drawn);
    }

    if (!volume_active)
        ipodjs_video_draw_wps_progress(id3, accent);

    ipodjs_video_draw_hold_overlay();
    root_menu_video_draw_volume_overlay();
    lcd_update();
}

struct root_menu_video_wps_render_state {
    bool valid;
    bool hold;
    bool charging;
    bool volume_overlay;
    bool shuffle;
    int status;
    int battery;
    int repeat;
    int rating;
    int playlist_index;
    int playlist_count;
    int volume_percent;
    int accent;
    int dark;
    int surface;
    int font_scale;
    unsigned long elapsed_second;
    unsigned long duration_second;
    const void *art_data;
    int art_width;
    int art_height;
    char path[MAX_PATH];
};

static struct root_menu_video_wps_render_state
    root_menu_video_wps_render_state;

static void root_menu_video_wps_capture_state(
    struct root_menu_video_wps_render_state *state)
{
    struct mp3entry *id3 = audio_current_track();
    struct bitmap *art = root_menu_video_stock_wps_art(id3);

    memset(state, 0, sizeof(*state));
    state->valid = true;
    state->hold = button_hold();
    state->charging = charger_inserted();
    state->volume_overlay = root_menu_video_volume_overlay_active();
    state->shuffle = global_settings.playlist_shuffle;
    state->status = audio_status();
    state->battery = battery_level();
    state->repeat = global_settings.repeat_mode;
    state->rating = id3 ? id3->rating : 0;
    state->playlist_index = playlist_get_display_index();
    state->playlist_count = playlist_amount();
    state->volume_percent = root_menu_video_volume_percent();
    state->accent = global_settings.ui_engine_accent;
    state->dark = global_settings.ui_engine_dark_mode;
    state->surface = global_settings.ui_engine_surface;
    state->font_scale = global_settings.ui_engine_font_scale;
    state->elapsed_second = id3 ? id3->elapsed / 1000 : 0;
    state->duration_second = id3 ? id3->length / 1000 : 0;
    if (art)
    {
        state->art_data = art->data;
        state->art_width = art->width;
        state->art_height = art->height;
    }
    if (id3)
        strmemccpy(state->path, id3->path, sizeof(state->path));
}

static bool root_menu_video_wps_full_changed(
    const struct root_menu_video_wps_render_state *old,
    const struct root_menu_video_wps_render_state *now)
{
    return !old->valid || old->hold != now->hold ||
           old->playlist_index != now->playlist_index ||
           old->playlist_count != now->playlist_count ||
           old->rating != now->rating || old->accent != now->accent ||
           old->dark != now->dark || old->surface != now->surface ||
           old->font_scale != now->font_scale ||
           old->duration_second != now->duration_second ||
           old->art_data != now->art_data ||
           old->art_width != now->art_width ||
           old->art_height != now->art_height ||
           strcmp(old->path, now->path);
}

static void root_menu_video_draw_wps_bottom(struct mp3entry *id3,
                                            bool volume_overlay)
{
    lcd_set_foreground(root_menu_video_panel());
    lcd_fillrect(0, 196, LCD_WIDTH, 28);
    if (volume_overlay)
        root_menu_video_draw_volume_overlay();
    else
        ipodjs_video_draw_wps_progress(id3, root_menu_video_accent());
    lcd_update_rect(0, 196, LCD_WIDTH, 28);
}

void root_menu_ipodjs_draw_wps_frame(void)
{
    struct root_menu_video_wps_render_state now;
    struct mp3entry *id3;
    bool header_changed;
    bool bottom_changed;

    if (global_settings.ui_engine != UI_ENGINE_IPODJS)
        return;

#if defined(HAVE_LCD_ENABLE) || defined(HAVE_LCD_SLEEP)
    if (!lcd_active())
    {
        root_menu_video_wps_render_state.valid = false;
        return;
    }
#endif

    root_menu_video_ensure_aa_slot();
    root_menu_video_apple_slider_asset(IPODJS_APPLE_VOLUME_LOW);
    root_menu_video_apple_slider_asset(IPODJS_APPLE_VOLUME_HIGH);
    root_menu_video_apple_slider_asset(IPODJS_APPLE_SLIDER_LIGHT);
    root_menu_video_apple_slider_asset(IPODJS_APPLE_SLIDER_DARK);
    root_menu_video_apple_slider_asset(IPODJS_APPLE_SLIDER_FILL);
    root_menu_video_apple_slider_asset(IPODJS_APPLE_PROGRESS_FRAME);
    root_menu_video_wps_capture_state(&now);

    if (root_menu_video_wps_force_full ||
        root_menu_video_wps_full_changed(&root_menu_video_wps_render_state,
                                         &now))
    {
        ipodjs_video_draw_wps_full();
        ipodjs_trace_wps("full", 0, 0, LCD_WIDTH, LCD_HEIGHT,
                         now.art_data, now.art_width, now.art_height);
        root_menu_video_wps_force_full = false;
        root_menu_video_wps_render_state = now;
        return;
    }

    (void)root_menu_video_update_wps_title_scroll();

    header_changed = root_menu_video_wps_render_state.status != now.status ||
        root_menu_video_wps_render_state.battery != now.battery ||
        root_menu_video_wps_render_state.charging != now.charging ||
        root_menu_video_wps_render_state.shuffle != now.shuffle ||
        root_menu_video_wps_render_state.repeat != now.repeat;
    bottom_changed =
        root_menu_video_wps_render_state.elapsed_second != now.elapsed_second ||
        root_menu_video_wps_render_state.volume_overlay != now.volume_overlay ||
        (now.volume_overlay &&
         root_menu_video_wps_render_state.volume_percent !=
             now.volume_percent);

    /* The transparent profile's panel is a full-height gradient.  A flat
     * rectangle would leave a visible seam, so retain the conservative full
     * redraw for that optional profile.  Stock Fidelity takes the fast path. */
    if (bottom_changed &&
        now.surface == UI_ENGINE_SURFACE_TRANSPARENT)
    {
        ipodjs_video_draw_wps_full();
        ipodjs_trace_wps("full-transparent", 0, 0, LCD_WIDTH, LCD_HEIGHT,
                         now.art_data, now.art_width, now.art_height);
        root_menu_video_wps_render_state = now;
        return;
    }

    id3 = audio_current_track();
    if (header_changed)
    {
        root_menu_video_draw_wps_status_title("Now Playing");
        root_menu_video_draw_wps_modes();
        lcd_update_rect(0, 0, LCD_WIDTH, IPODJS_HEADER_HEIGHT + 20);
        ipodjs_trace_wps("header", 0, 0, LCD_WIDTH,
                         IPODJS_HEADER_HEIGHT + 20, now.art_data,
                         now.art_width, now.art_height);
    }
    if (bottom_changed)
    {
        root_menu_video_draw_wps_bottom(id3, now.volume_overlay);
        ipodjs_trace_wps("bottom", 0, 196, LCD_WIDTH, 28,
                         now.art_data, now.art_width, now.art_height);
    }

    root_menu_video_wps_render_state = now;
}

static void ipodjs_video_draw_wps_empty_state(const char *title,
                                              const char *message,
                                              bool allow_replay)
{
    unsigned panel = root_menu_video_panel();
    unsigned text = root_menu_video_text();
    unsigned muted = root_menu_video_muted_text();

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_wps_status_title("Now Playing");

    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_TRANSPARENT)
    {
        if (root_menu_video_dark())
            root_menu_video_gradient(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                LCD_HEIGHT - IPODJS_HEADER_HEIGHT,
                LCD_RGBPACK(32, 37, 46), LCD_RGBPACK(17, 21, 28));
        else
            root_menu_video_gradient(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                LCD_HEIGHT - IPODJS_HEADER_HEIGHT,
                LCD_RGBPACK(248, 249, 251), LCD_RGBPACK(224, 229, 236));
    }
    else
    {
        lcd_set_foreground(panel);
        lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                     LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
    }

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(54, 60, 70) : IPODJS_SPLIT);
    lcd_hline(0, LCD_WIDTH - 1, IPODJS_HEADER_HEIGHT);
    root_menu_video_draw_wps_modes();

    lcd_setfont(root_menu_video_wps_font(true));
    lcd_set_foreground(text);
    lcd_set_background(panel);
    root_menu_video_puts_fit(30, 92, LCD_WIDTH - 60,
                             title ? title : "No Music", true);
    lcd_setfont(root_menu_video_wps_font(false));
    lcd_set_foreground(muted);
    root_menu_video_puts_fit(30, 120, LCD_WIDTH - 60,
                             message ? message : "Nothing playing", true);
    if (allow_replay)
        root_menu_video_puts_fit(30, 146, LCD_WIDTH - 60,
                                 "Select replays", true);
    else
        root_menu_video_puts_fit(30, 146, LCD_WIDTH - 60,
                                 "Menu returns", true);

    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

static bool ipodjs_video_wps_empty(const char *title, const char *message,
                                   bool allow_replay)
{
    bool redraw = true;
    bool held = button_hold();
    long next_hold_refresh = 0;

    root_menu_video_enter_native_screen();
    root_menu_wait_for_button_release();
    button_clear_queue();

    while (true)
    {
        int action;

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            ipodjs_video_draw_wps_empty_state(title, message, allow_replay);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE|ALLOW_SOFTLOCK, HZ/5);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(allow_replay);

            case ACTION_TREE_WPS:
            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(false);
        }
    }
}

static bool root_menu_video_handle_play_pause(bool allow_enter_wps,
                                              bool *started_playback)
{
    int status = audio_status();
    long now = current_tick;

    if (started_playback)
        *started_playback = false;

    if (TIME_BEFORE(now, root_menu_video_last_playpause_tick +
                    IPODJS_DB_PLAYPAUSE_DEBOUNCE))
    {
        IPODJS_LOGF("ipodjs: play/pause debounced status=%d", status);
        return false;
    }

    root_menu_video_last_playpause_tick = now;
    IPODJS_LOGF("ipodjs: play/pause status=%d has_track=%d", status,
                root_menu_video_has_current_track() ? 1 : 0);

    if (status & AUDIO_STATUS_PLAY)
    {
        wps_do_playpause(false);
        IPODJS_LOGF("ipodjs: toggled active playback");
        return false;
    }

    if (!root_menu_video_try_resume_playback())
    {
        button_clear_queue();
        return false;
    }

    if (started_playback)
        *started_playback = true;

    button_clear_queue();
    return allow_enter_wps;
}

static bool root_menu_video_handle_tree_stop(int action, bool *redraw)
{
#ifdef IPOD_ACCESSORY_PROTOCOL
    if (action == ACTION_NONE && !button_hold() &&
        iap_take_kokkia_connection_event())
    {
        /* Connection animations belong to the Home dashboard only.  Consume
         * the edge here so it cannot appear late after leaving another
         * native screen. */
        if (redraw)
            *redraw = true;
        return true;
    }

    if (root_menu_video_status_bluetooth_valid &&
        root_menu_video_bluetooth_ready() !=
            root_menu_video_status_bluetooth_drawn &&
        redraw)
        *redraw = true;
#endif

    if (root_menu_video_tree_stop_pending)
    {
        if (action == ACTION_TREE_STOP ||
            (button_status() & BUTTON_PLAY) != 0)
            return true;

        root_menu_video_tree_stop_pending = false;
        if (action == ACTION_TREE_WPS)
            return true;
    }

    if (action != ACTION_TREE_STOP)
        return false;

    root_menu_video_tree_stop_pending = true;

    if (audio_status())
    {
        audio_stop();
        if (redraw)
            *redraw = true;
    }
    return true;
}

static void ipodjs_video_play_pause(void)
{
    IPODJS_LOGF("ipodjs: wps playpause before status=%d", audio_status());
    (void)root_menu_video_handle_play_pause(false, NULL);
    IPODJS_LOGF("ipodjs: wps playpause after status=%d", audio_status());
}

static int ipodjs_video_wps_finish(int ret)
{
    root_menu_video_stop_wps_title_scroll();
    root_menu_video_wps_force_full = true;
    wps_state_deinit();
    return root_menu_video_finish_native_screen(ret);
}

static int ipodjs_video_wps(void)
{
    bool redraw = true;
    bool held = button_hold();
    bool volume_overlay_was_active = false;
    bool deferred_status_save = false;
    long next_hold_refresh = 0;
    long last_elapsed_sec = -1;
    int last_battery = -1;
    bool last_charging = false;
    bool last_paused = false;
    bool last_bluetooth = false;
    bool last_internet = false;
    char last_track_path[MAX_PATH] = "";

    root_menu_video_enter_native_screen();
    root_menu_video_wps_force_full = true;
    wps_state_init();
    root_menu_video_ensure_aa_slot();
    root_menu_video_apple_slider_asset(IPODJS_APPLE_VOLUME_LOW);
    root_menu_video_apple_slider_asset(IPODJS_APPLE_VOLUME_HIGH);
    root_menu_video_apple_slider_asset(IPODJS_APPLE_SLIDER_LIGHT);
    root_menu_video_apple_slider_asset(IPODJS_APPLE_SLIDER_DARK);
    root_menu_video_apple_slider_asset(IPODJS_APPLE_SLIDER_FILL);
    root_menu_video_apple_slider_asset(IPODJS_APPLE_PROGRESS_FRAME);
    root_menu_wait_for_button_release();
    button_clear_queue();

    while (true)
    {
        int action;
        int status = audio_status();
        int battery = battery_level();
        bool charging = charger_inserted();
        bool bluetooth = false;
        bool internet = usb_internet_connected();
#ifdef IPOD_ACCESSORY_PROTOCOL
        bluetooth = iap_kokkia_present();
#endif
        bool status_redraw = battery != last_battery ||
                             charging != last_charging ||
                             bluetooth != last_bluetooth ||
                             internet != last_internet;
        bool paused;

        if (!(status & AUDIO_STATUS_PLAY))
            break;

        paused = (status & AUDIO_STATUS_PAUSE) != 0;

        root_menu_video_hold_update(&held, &redraw);
        if (root_menu_video_volume_overlay_active() != volume_overlay_was_active)
            redraw = true;
        if (paused)
        {
            if (paused != last_paused)
            {
                last_paused = paused;
                redraw = true;
            }
        }
        else
        {
            struct mp3entry *id3 = audio_current_track();
            const char *path = id3 ? id3->path : "";
            long elapsed_sec = id3 ? (long)(id3->elapsed / 1000) : -1;

            if (strcmp(last_track_path, path))
            {
                strmemccpy(last_track_path, path, sizeof(last_track_path));
                last_elapsed_sec = -1;
                redraw = true;
            }
            if (elapsed_sec != last_elapsed_sec)
            {
                redraw = true;
                last_elapsed_sec = elapsed_sec;
            }
            if (paused != last_paused)
            {
                last_paused = paused;
                redraw = true;
            }
            if (deferred_status_save)
            {
                status_save(false);
                deferred_status_save = false;
            }
        }
        if (redraw)
        {
            root_menu_ipodjs_draw_wps_frame();
            volume_overlay_was_active = root_menu_video_volume_overlay_active();
            redraw = false;
        }
        else
        {
            if (status_redraw)
            {
                if (held)
                    root_menu_video_draw_status_title("HOLD");
                else
                {
                    root_menu_video_draw_wps_status_title("Now Playing");
                    root_menu_video_draw_wps_modes();
                }
                lcd_update_rect(0, 0, LCD_WIDTH,
                                IPODJS_HEADER_HEIGHT + 20);
            }
        }
        last_battery = battery;
        last_charging = charging;
        last_bluetooth = bluetooth;
        last_internet = internet;

        if (held)
        {
            if (paused)
            {
                sleep(HZ / 20);
                continue;
            }

            if (!(audio_status() & AUDIO_STATUS_PAUSE) &&
                TIME_AFTER(current_tick, next_hold_refresh))
            {
                next_hold_refresh = current_tick + HZ;
                redraw = true;
            }

            sleep(HZ/20);
            continue;
        }

        action = paused ? skin_wait_for_action(WPS, CONTEXT_WPS|ALLOW_SOFTLOCK,
                                               HZ/5)
                        : get_action(CONTEXT_WPS|ALLOW_SOFTLOCK, HZ/5);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (action == ACTION_NONE)
            continue;
        if (!paused && !IS_SYSEVENT(action))
            storage_spin();
        if (root_menu_video_hold_update(&held, &redraw))
            continue;

        switch (action)
        {
            case ACTION_WPS_PLAY:
                ipodjs_video_play_pause();
                redraw = true;
                break;

            case ACTION_WPS_SKIPNEXT:
                audio_next();
                redraw = true;
                break;

            case ACTION_WPS_SKIPPREV:
                audio_prev();
                redraw = true;
                break;

            case ACTION_WPS_VOLUP:
            case ACTION_WPS_VOLDOWN:
                if (paused)
                {
                    adjust_volume_no_save(action == ACTION_WPS_VOLUP ? 1 : -1);
                    deferred_status_save = true;
                }
                else
                    adjust_volume(action == ACTION_WPS_VOLUP ? 1 : -1);
                redraw = true;
                break;

            case ACTION_WPS_BROWSE:
                return ipodjs_video_wps_finish(GO_TO_PREVIOUS_BROWSER);

            case ACTION_WPS_CONTEXT:
            {
                int ret;

                root_menu_video_finish_native_screen(0);
                wps_state_deinit();
                root_menu_wait_for_button_release();
                button_clear_queue();
                ret = launch_lrcplayer_plugin(NULL);
                root_menu_video_enter_native_screen();
                wps_state_init();
                if (ret == GO_TO_ROOT)
                    return ipodjs_video_wps_finish(ret);
                redraw = true;
                break;
            }

            case ACTION_WPS_VIEW_PLAYLIST:
                return ipodjs_video_wps_finish(GO_TO_PLAYLIST_VIEWER);

            case ACTION_WPS_MENU:
                button_clear_queue();
                return ipodjs_video_wps_finish(GO_TO_ROOT);

            case ACTION_WPS_STOP:
                audio_stop();
                return ipodjs_video_wps_finish(GO_TO_ROOT);

            default:
                redraw = true;
                break;
        }
    }

    return ipodjs_video_wps_finish(GO_TO_ROOT);
}

static void root_menu_video_draw_home(
    int selected, int preview_selected,
    struct root_menu_video_list_draw_state *state)
{
    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);

    if (button_hold() &&
        global_settings.ui_engine_hold_effect == UI_ENGINE_HOLD_LOCKSCREEN &&
        root_menu_video_hold_static_frame_valid)
        return;

    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    if (button_hold() &&
        global_settings.ui_engine_hold_effect == UI_ENGINE_HOLD_LOCKSCREEN)
    {
        ipodjs_video_draw_hold_overlay();
        lcd_update();
        ipodjs_trace_screen("Lockscreen", "full", selected, 0,
                            root_menu_video_count(), 0, 0,
                            LCD_WIDTH, LCD_HEIGHT);
        root_menu_video_hold_static_frame_valid = true;
        return;
    }

    root_menu_video_draw_status();
    root_menu_video_draw_list(selected, state);
    root_menu_video_draw_preview(preview_selected);
    ipodjs_video_draw_hold_overlay();

    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
    ipodjs_trace_screen("Home", "full", selected,
                        state && state->valid ? state->top : 0,
                        root_menu_video_count(), 0, 0,
                        LCD_WIDTH, LCD_HEIGHT);
}

static bool root_menu_video_draw_home_selection_delta(
    int previous_selected, int selected,
    struct root_menu_video_list_draw_state *state)
{
    int previous_row;
    int selected_row;
    bool previous_visible;
    bool selected_visible;

    if (button_hold())
    {
        root_menu_video_draw_home(selected, selected, state);
        return true;
    }

    if (!state || !state->valid || previous_selected == selected)
        return false;

    if (root_menu_video_list_top(selected, state->visible) != state->top)
        return false;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_setfont(root_menu_video_font());

    previous_row = previous_selected - state->top;
    selected_row = selected - state->top;
    previous_visible = previous_row >= 0 && previous_row < state->visible;
    selected_visible = selected_row >= 0 && selected_row < state->visible;
    if (previous_visible)
        root_menu_video_draw_list_row(previous_selected, previous_row,
                                      state->row_h, false);
    if (selected_visible)
        root_menu_video_draw_list_row(selected, selected_row,
                                      state->row_h, true);

    state->selected = selected;
    if (previous_visible && selected_visible &&
        (previous_row + 1 == selected_row ||
         selected_row + 1 == previous_row))
    {
        int first_row = MIN(previous_row, selected_row);
        int last_row = MAX(previous_row, selected_row);

        lcd_update_rect(0, IPODJS_HEADER_HEIGHT + first_row * state->row_h,
                        IPODJS_LIST_WIDTH,
                        (last_row - first_row + 1) * state->row_h);
    }
    else if (previous_visible)
        lcd_update_rect(0, IPODJS_HEADER_HEIGHT + previous_row * state->row_h,
                        IPODJS_LIST_WIDTH, state->row_h);
    else if (selected_visible)
        lcd_update_rect(0, IPODJS_HEADER_HEIGHT + selected_row * state->row_h,
                        IPODJS_LIST_WIDTH, state->row_h);
    ipodjs_trace_screen("Home", "rows", selected, state->top,
                        root_menu_video_count(), 0,
                        IPODJS_HEADER_HEIGHT +
                            MIN(previous_row, selected_row) * state->row_h,
                        IPODJS_LIST_WIDTH,
                        (abs(previous_row - selected_row) + 1) * state->row_h);
    return true;
}

static void root_menu_video_draw_home_preview_only(int selected,
                                                   bool crossfade)
{
    bool fading;

    if (button_hold())
    {
        root_menu_video_draw_home(selected, selected, NULL);
        return;
    }

    int x = IPODJS_PREVIEW_X;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    fading = crossfade &&
        ipodjs_ui_preview_fade_begin(&screens[SCREEN_MAIN], x, 0,
                                    LCD_WIDTH - x, LCD_HEIGHT);
    root_menu_video_draw_preview(selected);
    if (!fading || !ipodjs_ui_preview_fade_present(&screens[SCREEN_MAIN]))
        lcd_update_rect(x, 0, LCD_WIDTH - x, LCD_HEIGHT);
}

struct root_menu_video_music_item {
    const char *label;
    int screen;
};

#ifdef HAVE_TAGCACHE
enum root_menu_video_music_native {
    IPODJS_MUSIC_NATIVE_NONE = 0,
    IPODJS_MUSIC_NATIVE_ARTISTS,
    IPODJS_MUSIC_NATIVE_ALBUM_ARTISTS,
    IPODJS_MUSIC_NATIVE_ALBUMS,
    IPODJS_MUSIC_NATIVE_SONGS,
};

#define IPODJS_SEARCH_MAX_RESULTS 72
#define IPODJS_SEARCH_QUERY_SIZE  32
#define IPODJS_SEARCH_ICON_SIZE   16
#define IPODJS_SEARCH_ROW_HEIGHT  36

enum root_menu_video_search_type {
    IPODJS_SEARCH_SONG = 0,
    IPODJS_SEARCH_ARTIST,
    IPODJS_SEARCH_ALBUM,
    IPODJS_SEARCH_PLAYLIST,
    IPODJS_SEARCH_TYPE_COUNT,
};

struct root_menu_video_search_result {
    int type;
    int tag;
    int seek;
    int idxid;
    char label[96];
    char sublabel[96];
};

static struct root_menu_video_search_result
    root_menu_video_search_results[IPODJS_SEARCH_MAX_RESULTS];
static int root_menu_video_search_result_count;
static int root_menu_video_search_total;
#endif

static const struct root_menu_video_music_item root_menu_video_music_items[] = {
    { "Now Playing", GO_TO_WPS },
#ifdef HAVE_TAGCACHE
    { "Artists", -IPODJS_MUSIC_NATIVE_ARTISTS },
    { "Album Artists", -IPODJS_MUSIC_NATIVE_ALBUM_ARTISTS },
    { "Albums", -IPODJS_MUSIC_NATIVE_ALBUMS },
    { "Songs", -IPODJS_MUSIC_NATIVE_SONGS },
    { "Cover Flow", GO_TO_PICTUREFLOW },
#endif
    { "Playlists", GO_TO_PLAYLISTS_SCREEN },
    { "Files", GO_TO_FILEBROWSER },
};

static int root_menu_video_music_count(void)
{
    return ARRAYLEN(root_menu_video_music_items);
}

static int root_menu_video_music_top(int selected, int visible)
{
    if (selected >= visible)
        return selected - visible + 1;

    return 0;
}

static void root_menu_video_draw_music_row(int index, int screen_row,
                                           int row_h, bool active)
{
    int item_y = IPODJS_HEADER_HEIGHT + screen_row * row_h;

    if (active)
    {
        unsigned accent = root_menu_video_accent();
        root_menu_video_selection_gradient(0, item_y, LCD_WIDTH, row_h);
        lcd_set_foreground(IPODJS_PREVIEW_TEXT);
        lcd_set_background(accent);
    }
    else
    {
        lcd_set_foreground(root_menu_video_row_bg());
        lcd_fillrect(0, item_y, LCD_WIDTH, row_h);
        lcd_set_foreground(root_menu_video_text());
        lcd_set_background(root_menu_video_row_bg());
    }

    root_menu_video_puts_fit(7, item_y + 4, LCD_WIDTH - 28,
                             root_menu_video_music_items[index].label,
                             false);
    if (active)
        root_menu_video_draw_arrow(LCD_WIDTH - 16,
                                   item_y + (row_h - 6) / 2);
}

static void root_menu_video_draw_music_selection_delta(
    int previous_selected, int selected,
    struct root_menu_video_list_draw_state *state)
{
    int previous_row;
    int selected_row;

    if (!state || !state->valid || previous_selected == selected)
        return;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_setfont(root_menu_video_font());

    previous_row = previous_selected - state->top;
    selected_row = selected - state->top;
    if (previous_row >= 0 && previous_row < state->visible)
        root_menu_video_draw_music_row(previous_selected, previous_row,
                                       state->row_h, false);
    if (selected_row >= 0 && selected_row < state->visible)
        root_menu_video_draw_music_row(selected, selected_row,
                                       state->row_h, true);

    state->selected = selected;
    lcd_update_rect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                    LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
}

static void root_menu_video_draw_music_menu(
    int selected, struct root_menu_video_list_draw_state *state)
{
    int count = root_menu_video_music_count();
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int top = root_menu_video_music_top(selected, visible);
    int i;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_status_title("Music");

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_screen_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    for (i = 0; i < visible && top + i < count; i++)
    {
        int index = top + i;
        root_menu_video_draw_music_row(index, i, row_h, index == selected);
    }

    if (state)
    {
        state->selected = selected;
        state->top = top;
        state->row_h = row_h;
        state->visible = visible;
        state->valid = true;
    }

    ipodjs_video_draw_hold_overlay();
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
    ipodjs_trace_screen("Music", "full", selected, top, count,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

#ifdef HAVE_TAGCACHE
struct root_menu_video_db_row {
    char label[IPODJS_DB_LABEL_LEN];
    char sublabel[IPODJS_DB_LABEL_LEN];
    int seek;
    int idxid;
    int disc;
    int track;
};

enum root_menu_video_db_context {
    IPODJS_DB_CTX_NONE = 0,
    IPODJS_DB_CTX_ARTIST_ALBUM,
    IPODJS_DB_CTX_ALBUMARTIST_ALBUM,
    IPODJS_DB_CTX_ALBUMS,
};

static struct root_menu_video_db_row
    root_menu_video_db_rows[IPODJS_DB_MAX_DEPTH][IPODJS_DB_WINDOW_ROWS];
static uint32_t root_menu_video_db_uniq[IPODJS_DB_MAX_ROWS * 2];

struct root_menu_video_album_group_cache {
    bool valid;
    int filter_tag;
    int filter_seek;
    int filter_tag2;
    int filter_seek2;
    long built_tick;
    int count;
    uint16_t group_index[IPODJS_DB_MAX_ROWS * 2];
    struct root_menu_video_db_row rows[IPODJS_DB_MAX_ROWS];
};

static struct root_menu_video_album_group_cache
    root_menu_video_album_groups;

static int root_menu_video_ascii_cmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);

        if (ca != cb)
            return ca - cb;
    }

    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static int root_menu_video_db_track_cmp(const void *pa, const void *pb)
{
    const struct root_menu_video_db_row *a = pa;
    const struct root_menu_video_db_row *b = pb;
    int adisc = a->disc > 0 ? a->disc : 0;
    int bdisc = b->disc > 0 ? b->disc : 0;
    int atrack = a->track > 0 ? a->track : 99999;
    int btrack = b->track > 0 ? b->track : 99999;

    if (adisc != bdisc)
        return adisc - bdisc;
    if (atrack != btrack)
        return atrack - btrack;
    return root_menu_video_ascii_cmp(a->label, b->label);
}

static bool root_menu_video_valid_artist_name(const char *name)
{
    return name && name[0] && strcmp(name, UNTAGGED) != 0;
}

static bool root_menu_video_db_single_album_tag_value(int result_tag,
                                                      int album_seek,
                                                      int filter_tag,
                                                      int filter_seek,
                                                      int filter_tag2,
                                                      int filter_seek2,
                                                      char *buf,
                                                      size_t buf_size,
                                                      bool *multiple)
{
    struct tagcache_search tcs;
    char tcs_buf[TAGCACHE_BUFSZ];
    char first[IPODJS_DB_LABEL_LEN];
    bool found = false;

    if (multiple)
        *multiple = false;
    if (buf && buf_size > 0)
        buf[0] = '\0';

    if (!tagcache_search(&tcs, result_tag))
        return false;

    tagcache_search_set_uniqbuf(&tcs, root_menu_video_db_uniq,
                                sizeof(root_menu_video_db_uniq));
    if (!tagcache_search_add_filter(&tcs, tag_album, album_seek))
    {
        tagcache_search_finish(&tcs);
        return false;
    }
    if (filter_tag >= 0 &&
        !tagcache_search_add_filter(&tcs, filter_tag, filter_seek))
    {
        tagcache_search_finish(&tcs);
        return false;
    }
    if (filter_tag2 >= 0 &&
        !tagcache_search_add_filter(&tcs, filter_tag2, filter_seek2))
    {
        tagcache_search_finish(&tcs);
        return false;
    }

    first[0] = '\0';
    while (tagcache_get_next(&tcs, tcs_buf, sizeof(tcs_buf)))
    {
        if (!root_menu_video_valid_artist_name(tcs.result))
            continue;

        if (!found)
        {
            strmemccpy(first, tcs.result, sizeof(first));
            found = true;
        }
        else if (strcasecmp(first, tcs.result) != 0)
        {
            if (multiple)
                *multiple = true;
            found = false;
            break;
        }
    }

    tagcache_search_finish(&tcs);

    if (found && buf && buf_size > 0)
        strmemccpy(buf, first, buf_size);

    return found;
}

/* Native iPodJS lists bypass tagtree, so they need the same bounded recovery
 * at their top-level search boundary.  tagcache_recover distinguishes a stale
 * tagcache owner, descriptor exhaustion and ATA I/O; it never resets playback
 * or borrows the audio buffer. */
static bool root_menu_video_db_start_search(struct tagcache_search *tcs,
                                            int tag)
{
    long deadline;
    long next_recovery = 0;

    if (tagcache_search(tcs, tag))
        return true;

    deadline = current_tick + HZ * 15;
    while (TIME_BEFORE(current_tick, deadline))
    {
        splash(0, "Loading Music...");
        if (TIME_AFTER(current_tick, next_recovery))
        {
            next_recovery = current_tick + HZ * 3;
            tagcache_recover();
            if (tagcache_search(tcs, tag))
                return true;
        }
        if (action_userabort(HZ / 5))
            break;
    }

    return false;
}

static void root_menu_video_db_resolve_album_artist(int album_seek,
                                                    int filter_tag,
                                                    int filter_seek,
                                                    int filter_tag2,
                                                    int filter_seek2,
                                                    int fallback_idxid,
                                                    char *buf,
                                                    size_t buf_size)
{
    struct tagcache_search tcs;
    bool multiple = false;

    if (!buf || buf_size == 0)
        return;

    buf[0] = '\0';
    if (root_menu_video_db_single_album_tag_value(tag_albumartist,
            album_seek, filter_tag, filter_seek, filter_tag2, filter_seek2,
            buf, buf_size, &multiple))
        return;
    if (multiple)
    {
        strmemccpy(buf, "Various Artists", buf_size);
        return;
    }

    if (root_menu_video_db_single_album_tag_value(tag_artist,
            album_seek, filter_tag, filter_seek, filter_tag2, filter_seek2,
            buf, buf_size, &multiple))
        return;
    if (multiple)
    {
        strmemccpy(buf, "Various Artists", buf_size);
        return;
    }

    if (!tagcache_search(&tcs, tag_filename))
        return;

    if (tagcache_retrieve(&tcs, fallback_idxid, tag_albumartist, buf,
                          buf_size) &&
        root_menu_video_valid_artist_name(buf))
    {
        tagcache_search_finish(&tcs);
        return;
    }
    if (!tagcache_retrieve(&tcs, fallback_idxid, tag_artist, buf, buf_size) ||
        !root_menu_video_valid_artist_name(buf))
        buf[0] = '\0';

    tagcache_search_finish(&tcs);
}

static int root_menu_video_db_album_cmp(const void *pa, const void *pb)
{
    const struct root_menu_video_db_row *a = pa;
    const struct root_menu_video_db_row *b = pb;
    int cmp = root_menu_video_ascii_cmp(a->label, b->label);

    if (cmp)
        return cmp;

    return root_menu_video_ascii_cmp(a->sublabel, b->sublabel);
}

static bool root_menu_video_db_album_cache_matches(int filter_tag,
                                                   int filter_seek,
                                                   int filter_tag2,
                                                   int filter_seek2)
{
    return root_menu_video_album_groups.valid &&
           TIME_BEFORE(current_tick,
                       root_menu_video_album_groups.built_tick +
                       IPODJS_DB_ALBUM_CACHE_TTL) &&
           root_menu_video_album_groups.filter_tag == filter_tag &&
           root_menu_video_album_groups.filter_seek == filter_seek &&
           root_menu_video_album_groups.filter_tag2 == filter_tag2 &&
           root_menu_video_album_groups.filter_seek2 == filter_seek2;
}

static unsigned root_menu_video_db_group_hash(const char *album,
                                              const char *artist)
{
    unsigned hash = 2166136261u;
    const unsigned char *p;

    for (p = (const unsigned char *)album; p && *p; p++)
        hash = (hash ^ (unsigned)tolower(*p)) * 16777619u;
    hash = (hash ^ 0xffu) * 16777619u;
    for (p = (const unsigned char *)artist; p && *p; p++)
        hash = (hash ^ (unsigned)tolower(*p)) * 16777619u;
    return hash;
}

static int root_menu_video_db_find_album_group(const char *album,
                                               const char *artist,
                                               int *empty_slot)
{
    unsigned hash = root_menu_video_db_group_hash(album, artist);
    int mask = ARRAYLEN(root_menu_video_album_groups.group_index) - 1;
    int slot = hash & mask;

    if (empty_slot)
        *empty_slot = -1;

    for (int probe = 0; probe <= mask; probe++)
    {
        int index = root_menu_video_album_groups.group_index[slot];

        if (index == 0)
        {
            if (empty_slot)
                *empty_slot = slot;
            return -1;
        }

        index--;
        struct root_menu_video_db_row *row =
            &root_menu_video_album_groups.rows[index];

        if (!strcasecmp(row->label, album) &&
            !strcasecmp(row->sublabel, artist))
            return index;

        slot = (slot + 1) & mask;
    }

    return -1;
}

static void root_menu_video_db_row_artist(struct tagcache_search *tcs,
                                          int idxid, char *artist,
                                          size_t artist_size)
{
    if (!artist || artist_size == 0)
        return;

    artist[0] = '\0';
    if (tagcache_retrieve(tcs, idxid, tag_albumartist, artist,
                          artist_size) &&
        root_menu_video_valid_artist_name(artist))
        return;

    if (!tagcache_retrieve(tcs, idxid, tag_artist, artist, artist_size) ||
        !root_menu_video_valid_artist_name(artist))
        artist[0] = '\0';
}

static int root_menu_video_db_build_album_cache(int filter_tag,
                                                int filter_seek,
                                                int filter_tag2,
                                                int filter_seek2)
{
    struct tagcache_search tcs;
    char album[TAGCACHE_BUFSZ];
    char artist[IPODJS_DB_LABEL_LEN];

    if (root_menu_video_db_album_cache_matches(filter_tag, filter_seek,
                                               filter_tag2, filter_seek2))
        return root_menu_video_album_groups.count;

    memset(&root_menu_video_album_groups, 0,
           sizeof(root_menu_video_album_groups));
    root_menu_video_album_groups.filter_tag = filter_tag;
    root_menu_video_album_groups.filter_seek = filter_seek;
    root_menu_video_album_groups.filter_tag2 = filter_tag2;
    root_menu_video_album_groups.filter_seek2 = filter_seek2;
    root_menu_video_album_groups.built_tick = current_tick;
    root_menu_video_album_groups.valid = true;

    if (!root_menu_video_db_start_search(&tcs, tag_album))
    {
        root_menu_video_album_groups.valid = false;
        return -1;
    }

    if (filter_tag >= 0)
        tagcache_search_add_filter(&tcs, filter_tag, filter_seek);
    if (filter_tag2 >= 0)
        tagcache_search_add_filter(&tcs, filter_tag2, filter_seek2);

    while (tagcache_get_next(&tcs, album, sizeof(album)))
    {
        int group;
        int hash_slot;

        if (!album[0] || !strcasecmp(album, UNTAGGED))
            continue;

        root_menu_video_db_row_artist(&tcs, tcs.idx_id, artist,
                                      sizeof(artist));

        group = root_menu_video_db_find_album_group(album, artist,
                                                    &hash_slot);
        if (group >= 0)
            continue;

        if (root_menu_video_album_groups.count >= IPODJS_DB_MAX_ROWS ||
            hash_slot < 0)
            break;

        struct root_menu_video_db_row *row =
            &root_menu_video_album_groups.rows[
                root_menu_video_album_groups.count];
        strmemccpy(row->label, album, sizeof(row->label));
        strmemccpy(row->sublabel, artist, sizeof(row->sublabel));
        row->seek = tcs.result_seek;
        row->idxid = tcs.idx_id;
        row->disc = 0;
        row->track = 0;
        root_menu_video_album_groups.group_index[hash_slot] =
            ++root_menu_video_album_groups.count;
    }

    tagcache_search_finish(&tcs);

    if (root_menu_video_album_groups.count > 1)
        qsort(root_menu_video_album_groups.rows,
              root_menu_video_album_groups.count,
              sizeof(root_menu_video_album_groups.rows[0]),
              root_menu_video_db_album_cmp);

    return root_menu_video_album_groups.count;
}

static int root_menu_video_db_load_album_groups(int filter_tag,
                                                int filter_seek,
                                                int filter_tag2,
                                                int filter_seek2,
                                                int offset,
                                                struct root_menu_video_db_row *rows,
                                                int row_count,
                                                bool *has_more)
{
    int total = root_menu_video_db_build_album_cache(filter_tag, filter_seek,
                                                     filter_tag2,
                                                     filter_seek2);
    int copied = 0;

    *has_more = false;
    if (total < 0)
        return -1;

    if (offset < 0)
        offset = 0;
    while (copied < row_count && offset + copied < total)
    {
        rows[copied] = root_menu_video_album_groups.rows[offset + copied];
        copied++;
    }

    if (offset + copied < total)
        *has_more = true;

    return copied;
}

static int root_menu_video_db_load(int tag, int filter_tag, int filter_seek,
                                   int filter_tag2, int filter_seek2,
                                   int offset,
                                   struct root_menu_video_db_row *rows,
                                   int row_count, bool sort_tracks,
                                   bool *has_more)
{
    struct tagcache_search tcs;
    char buf[TAGCACHE_BUFSZ];
    int skipped = 0;
    int count = 0;

    *has_more = false;
    if (tag == tag_album)
        return root_menu_video_db_load_album_groups(filter_tag, filter_seek,
                                                    filter_tag2,
                                                    filter_seek2, offset,
                                                    rows, row_count,
                                                    has_more);

    if (!root_menu_video_db_start_search(&tcs, tag))
        return -1;

    if (tag != tag_title && tag != tag_filename)
        tagcache_search_set_uniqbuf(&tcs, root_menu_video_db_uniq,
                                    sizeof(root_menu_video_db_uniq));

    if (filter_tag >= 0)
        tagcache_search_add_filter(&tcs, filter_tag, filter_seek);
    if (filter_tag2 >= 0)
        tagcache_search_add_filter(&tcs, filter_tag2, filter_seek2);

    while (tagcache_get_next(&tcs, buf, sizeof(buf)))
    {
        if (skipped++ < offset)
            continue;

        if (count >= row_count)
        {
            *has_more = true;
            break;
        }

        strmemccpy(rows[count].label, buf, sizeof(rows[count].label));
        rows[count].sublabel[0] = '\0';
        rows[count].seek = tcs.result_seek;
        rows[count].idxid = tcs.idx_id;
        rows[count].disc = 0;
        rows[count].track = 0;
        if (tag == tag_album)
        {
            root_menu_video_db_resolve_album_artist(rows[count].seek,
                filter_tag, filter_seek, filter_tag2, filter_seek2,
                rows[count].idxid, rows[count].sublabel,
                sizeof(rows[count].sublabel));
        }
        if (tag == tag_title)
        {
            rows[count].disc = tagcache_get_numeric(&tcs, tag_discnumber);
            rows[count].track = tagcache_get_numeric(&tcs, tag_tracknumber);
        }
        count++;
    }

    tagcache_search_finish(&tcs);
    if (sort_tracks && count > 1)
        qsort(rows, count, sizeof(rows[0]), root_menu_video_db_track_cmp);
    return count;
}

#define IPODJS_DB_FAST_SCROLL_BUCKETS 27
#define IPODJS_DB_FAST_SCROLL_MIN_ROWS 12
#define IPODJS_DB_FAST_SCROLL_TRIGGER 2

static int root_menu_video_db_fast_scroll_bucket(const char *label)
{
    return ipodjs_ui_fast_scroll_bucket(label);
}

static bool root_menu_video_db_fast_scroll_eligible(int tag)
{
    return tag == tag_artist || tag == tag_albumartist ||
           tag == tag_album || tag == tag_title;
}

static int root_menu_video_db_fast_scroll_build(
    int tag, int filter_tag, int filter_seek,
    int filter_tag2, int filter_seek2,
    int first[IPODJS_DB_FAST_SCROLL_BUCKETS])
{
    struct tagcache_search tcs;
    char label[TAGCACHE_BUFSZ];
    int count = 0;

    for (int i = 0; i < IPODJS_DB_FAST_SCROLL_BUCKETS; i++)
        first[i] = -1;

    if (tag == tag_album)
    {
        int total = root_menu_video_db_build_album_cache(
            filter_tag, filter_seek, filter_tag2, filter_seek2);

        if (total < 0)
            return -1;
        for (int i = 0; i < total; i++)
        {
            int bucket = root_menu_video_db_fast_scroll_bucket(
                root_menu_video_album_groups.rows[i].label);
            if (bucket < 0)
                return -1;
            if (first[bucket] < 0)
                first[bucket] = i;
        }
        return total;
    }

    if (!root_menu_video_db_start_search(&tcs, tag))
        return -1;
    if (tag != tag_title && tag != tag_filename)
        tagcache_search_set_uniqbuf(&tcs, root_menu_video_db_uniq,
                                    sizeof(root_menu_video_db_uniq));
    if (filter_tag >= 0 &&
        !tagcache_search_add_filter(&tcs, filter_tag, filter_seek))
    {
        tagcache_search_finish(&tcs);
        return -1;
    }
    if (filter_tag2 >= 0 &&
        !tagcache_search_add_filter(&tcs, filter_tag2, filter_seek2))
    {
        tagcache_search_finish(&tcs);
        return -1;
    }

    while (tagcache_get_next(&tcs, label, sizeof(label)))
    {
        int bucket = root_menu_video_db_fast_scroll_bucket(label);

        if (bucket < 0)
        {
            tagcache_search_finish(&tcs);
            return -1;
        }

        if (first[bucket] < 0)
            first[bucket] = count;
        count++;
    }
    tagcache_search_finish(&tcs);
    return count;
}

static bool root_menu_video_db_fast_scroll_move(
    struct root_menu_video_db_row *rows, int row_count, int selected_local,
    int direction, int first[IPODJS_DB_FAST_SCROLL_BUCKETS],
    int *active_bucket, int *selected_abs)
{
    int bucket = *active_bucket;
    char label[2];

    if (bucket < 0)
    {
        if (row_count <= 0 || selected_local < 0 ||
            selected_local >= row_count)
            return false;
        bucket = root_menu_video_db_fast_scroll_bucket(
            rows[selected_local].label);
    }
    else
    {
        int candidate = bucket + direction;

        while (candidate >= 0 &&
               candidate < IPODJS_DB_FAST_SCROLL_BUCKETS &&
               first[candidate] < 0)
            candidate += direction;
        if (candidate >= 0 &&
            candidate < IPODJS_DB_FAST_SCROLL_BUCKETS)
            bucket = candidate;
    }

    if (first[bucket] < 0)
        return false;
    *active_bucket = bucket;
    *selected_abs = first[bucket];
    label[0] = bucket == 0 ? '#' : (char)('A' + bucket - 1);
    label[1] = '\0';
    ipodjs_ui_fast_scroll_show(label);
    return true;
}

static int root_menu_video_db_start_playlist(int start_index)
{
    if (start_index < 0)
        start_index = 0;

    if (global_settings.playlist_shuffle)
    {
        start_index = playlist_shuffle(current_tick, start_index);
        if (!global_settings.play_selected)
            start_index = 0;
    }

    playlist_start(start_index, 0, 0);
    return GO_TO_WPS;
}

static int root_menu_video_db_play_tracks(int selected_idxid,
                                          int filter_tag, int filter_seek,
                                          int filter_tag2, int filter_seek2)
{
    struct tagcache_search tcs;
    struct playlist_insert_context context;
    char title[TAGCACHE_BUFSZ];
    char path[MAX_PATH];
    int playlist_index = 0;
    int start_index = -1;

    if (!tagcache_search(&tcs, tag_title))
        return GO_TO_ROOT;

    if (filter_tag >= 0 &&
        !tagcache_search_add_filter(&tcs, filter_tag, filter_seek))
    {
        tagcache_search_finish(&tcs);
        return GO_TO_ROOT;
    }
    if (filter_tag2 >= 0 &&
        !tagcache_search_add_filter(&tcs, filter_tag2, filter_seek2))
    {
        tagcache_search_finish(&tcs);
        return GO_TO_ROOT;
    }

    if (playlist_create(NULL, NULL) < 0)
    {
        tagcache_search_finish(&tcs);
        return GO_TO_ROOT;
    }
    if (playlist_insert_context_create(NULL, &context, PLAYLIST_INSERT_LAST,
                                       false, true) < 0)
    {
        tagcache_search_finish(&tcs);
        return GO_TO_ROOT;
    }

    while (tagcache_get_next(&tcs, title, sizeof(title)))
    {
        int idxid = tcs.idx_id;

        if (!tagcache_retrieve(&tcs, idxid, tag_filename,
                               path, sizeof(path)))
            continue;

        if (playlist_insert_context_add(&context, path) < 0)
            break;

        if (idxid == selected_idxid && start_index < 0)
            start_index = playlist_index;

        playlist_index++;
        if ((playlist_index & 0xf) == 0)
            yield();
    }

    playlist_insert_context_release(&context);
    tagcache_search_finish(&tcs);

    if (playlist_index <= 0)
        return GO_TO_ROOT;

    IPODJS_LOGF("ipodjs: list play tracks=%d start=%d shuffle=%d repeat=%d",
                playlist_index, start_index,
                global_settings.playlist_shuffle ? 1 : 0,
                global_settings.repeat_mode);
    return root_menu_video_db_start_playlist(start_index);
}

static int root_menu_video_db_play_album_track(
    int selected_idxid, int album_seek, int filter_tag, int filter_seek)
{
    struct root_menu_video_db_row *rows =
        root_menu_video_album_groups.rows;
    struct tagcache_search tcs;
    char path[MAX_PATH];
    bool has_more = false;
    int row_count;
    int playlist_index = 0;
    int start_index = -1;
    int i;

    root_menu_video_album_groups.valid = false;
    row_count = root_menu_video_db_load(tag_title, tag_album, album_seek,
                                        filter_tag, filter_seek,
                                        0, rows, IPODJS_DB_MAX_ROWS,
                                        true, &has_more);
    if (row_count <= 0)
        return GO_TO_ROOT;

    if (!tagcache_search(&tcs, tag_filename))
        return GO_TO_ROOT;

    if (playlist_create(NULL, NULL) < 0)
    {
        tagcache_search_finish(&tcs);
        return GO_TO_ROOT;
    }

    for (i = 0; i < row_count; i++)
    {
        if (!tagcache_retrieve(&tcs, rows[i].idxid, tag_filename,
                               path, sizeof(path)))
            continue;

        if (playlist_insert_track(NULL, path, PLAYLIST_INSERT_LAST,
                                  false, true) < 0)
            continue;

        if (rows[i].idxid == selected_idxid && start_index < 0)
            start_index = playlist_index;

        playlist_index++;
    }

    tagcache_search_finish(&tcs);

    if (playlist_index <= 0)
        return GO_TO_ROOT;

    IPODJS_LOGF("ipodjs: album play tracks=%d start=%d more=%d shuffle=%d "
                "repeat=%d", playlist_index, start_index, has_more ? 1 : 0,
                global_settings.playlist_shuffle ? 1 : 0,
                global_settings.repeat_mode);
    return root_menu_video_db_start_playlist(start_index);
}

static int root_menu_video_db_context_for_level(int tag, int filter_tag,
                                                int filter_tag2)
{
    if (tag == tag_title && filter_tag == tag_album)
    {
        if (filter_tag2 == tag_artist)
            return IPODJS_DB_CTX_ARTIST_ALBUM;
        if (filter_tag2 == tag_albumartist)
            return IPODJS_DB_CTX_ALBUMARTIST_ALBUM;
        return IPODJS_DB_CTX_ALBUMS;
    }

    return IPODJS_DB_CTX_NONE;
}

static void root_menu_video_draw_db_menu(const char *title, int tag,
                                         int filter_tag, int filter_seek,
                                         struct root_menu_video_db_row *rows,
                                         int row_count, int selected)
{
    int row_h = root_menu_video_row_height();
    int visible;
    int draw_rows;
    int font_id;
    int font_h;
    int total = row_count;
    int top = 0;
    int i;
    bool album_rows = tag == tag_album;
    bool dark = root_menu_video_dark();
    unsigned screen_bg = root_menu_video_screen_bg();
    unsigned row_bg = root_menu_video_row_bg();
    unsigned text = root_menu_video_text();
    unsigned muted = root_menu_video_muted_text();
    unsigned separator = dark ? LCD_RGBPACK(43, 48, 56) :
                                LCD_RGBPACK(222, 225, 229);

    if (album_rows)
        row_h = MAX(row_h, IPODJS_DB_ALBUM_ROW_H);
    visible = root_menu_video_visible_rows(row_h);
    visible = MIN(visible, total);

    if (selected >= visible)
        top = selected - visible + 1;

    draw_rows = visible;
    if (album_rows && (top + visible) < total)
        draw_rows++;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(screen_bg);
    lcd_clear_display();

    root_menu_video_draw_status_title(title);

    font_id = root_menu_video_font();
    lcd_setfont(font_id);
    font_h = font_get(font_id)->height;
    lcd_set_foreground(screen_bg);
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    for (i = 0; i < draw_rows && top + i < total; i++)
    {
        int index = top + i;
        int item_y = IPODJS_HEADER_HEIGHT + i * row_h;
        bool active = index == selected;
        const char *label = "";
        const char *sublabel = "";
        int text_x = 7;
        int text_w = LCD_WIDTH - 28;
        int text_y = item_y + MAX(2, (row_h - font_h) / 2);

        if (index < row_count)
        {
            label = rows[index].label;
            sublabel = rows[index].sublabel;
        }
        if (active)
        {
            unsigned accent = root_menu_video_accent();
            root_menu_video_selection_gradient(0, item_y, LCD_WIDTH, row_h);
            lcd_set_foreground(IPODJS_PREVIEW_TEXT);
            lcd_set_background(accent);
        }
        else
        {
            lcd_set_foreground(row_bg);
            lcd_fillrect(0, item_y, LCD_WIDTH, row_h);
            if (album_rows)
            {
                lcd_set_foreground(separator);
                lcd_hline(0, LCD_WIDTH - 1, item_y + row_h - 1);
            }
            lcd_set_foreground(text);
            lcd_set_background(row_bg);
        }

        if (album_rows && index < row_count)
        {
#ifdef HAVE_ALBUMART
            int art_size = IPODJS_DB_ART_STOCK_SIZE;
            int art_x = 4;
            int art_y = item_y + (row_h - art_size) / 2;
            root_menu_video_draw_album_thumb(rows[index].label,
                                             rows[index].sublabel,
                                             rows[index].seek, filter_tag,
                                             filter_seek, art_x, art_y,
                                             art_size, active);
            text_x = art_x + art_size + 10;
            text_w = LCD_WIDTH - text_x - 25;
            text_y = item_y + MAX(3, (row_h - (font_h * 2)) / 2);
#endif
        }

        if (album_rows && index < row_count && sublabel[0])
        {
            lcd_set_foreground(active ? IPODJS_PREVIEW_TEXT :
                                        text);
            lcd_set_background(active ? root_menu_video_accent() :
                                        row_bg);
            root_menu_video_puts_fit(text_x, text_y, text_w, label, false);
            lcd_set_foreground(active ? LCD_RGBPACK(224, 242, 255) :
                                        muted);
            root_menu_video_puts_fit(text_x, text_y + font_h, text_w,
                                     sublabel, false);
        }
        else
        {
            root_menu_video_puts_fit(text_x, text_y,
                                     text_w, label, false);
        }
        if (active)
            root_menu_video_draw_arrow(LCD_WIDTH - 16,
                                       item_y + (row_h - 6) / 2);
    }

    ipodjs_ui_draw_fast_scroll(&screens[SCREEN_MAIN]);
    ipodjs_video_draw_hold_overlay();
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
    ipodjs_trace_screen(title, "full", selected, top, row_count,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static bool root_menu_video_draw_db_selection_delta(
    int tag, struct root_menu_video_db_row *rows, int row_count,
    int previous_selected, int selected)
{
    int row_h = root_menu_video_row_height();
    int visible;
    int previous_top;
    int selected_top;
    int previous_row;
    int selected_row;

    if (tag == tag_album || row_count <= 0)
        return false;

    visible = MIN(root_menu_video_visible_rows(row_h), row_count);
    previous_top = root_menu_video_list_top(previous_selected, visible);
    selected_top = root_menu_video_list_top(selected, visible);
    if (previous_top != selected_top)
        return false;

    previous_row = previous_selected - selected_top;
    selected_row = selected - selected_top;
    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_setfont(root_menu_video_font());
    root_menu_video_draw_native_row(rows[previous_selected].label,
                                    previous_row, row_h, LCD_WIDTH, false);
    root_menu_video_draw_native_row(rows[selected].label, selected_row,
                                    row_h, LCD_WIDTH, true);
    lcd_update_rect(0, IPODJS_HEADER_HEIGHT +
                    MIN(previous_row, selected_row) * row_h,
                    LCD_WIDTH,
                    (abs(previous_row - selected_row) + 1) * row_h);
    ipodjs_trace_screen("Database Rows", "rows", selected, selected_top,
                        row_count, 0, IPODJS_HEADER_HEIGHT +
                            MIN(previous_row, selected_row) * row_h,
                        LCD_WIDTH,
                        (abs(previous_row - selected_row) + 1) * row_h);
    return true;
}

static int root_menu_video_db_browser_level(const char *title, int tag,
                                            int filter_tag, int filter_seek,
                                            int filter_tag2, int filter_seek2,
                                            bool tracks, int level)
{
    struct root_menu_video_db_row *rows;
    int selected_abs = 0;
    int selected_local = 0;
    int window_start = 0;
    int row_count = 0;
    bool has_more = false;
    bool reload = true;
    bool redraw = true;
    bool redraw_rows = false;
    int previous_local = 0;
    int fast_first[IPODJS_DB_FAST_SCROLL_BUCKETS];
    int fast_total = -2;
    int fast_bucket = -1;
    bool held = button_hold();
    long next_hold_refresh = 0;
    int context = root_menu_video_db_context_for_level(tag, filter_tag,
                                                       filter_tag2);

    level = MAX(0, MIN(level, IPODJS_DB_MAX_DEPTH - 1));
    rows = root_menu_video_db_rows[level];
    ipodjs_ui_fast_scroll_clear();
    button_clear_queue();

    while (true)
    {
        int action;

        root_menu_video_hold_update(&held, &redraw);
        if (reload)
        {
            row_count = root_menu_video_db_load(tag, filter_tag, filter_seek,
                                                filter_tag2, filter_seek2,
                                                window_start, rows,
                                                IPODJS_DB_WINDOW_ROWS,
                                                tracks, &has_more);
            if (row_count < 0)
            {
                ipodjs_ui_fast_scroll_clear();
                splash(HZ, ID2P(LANG_TAGCACHE_BUSY));
                return GO_TO_DBBROWSER;
            }
            if (row_count > 0)
            {
                selected_local = selected_abs - window_start;
                selected_local = MAX(0, MIN(selected_local, row_count - 1));
                selected_abs = window_start + selected_local;
            }
            else
            {
                selected_local = 0;
                selected_abs = window_start;
            }
            IPODJS_LOGF("ipodjs: db load tag=%d rows=%d off=%d more=%d", tag,
                        row_count, window_start, has_more ? 1 : 0);
            reload = false;
            redraw = true;
            redraw_rows = false;
        }

        if (redraw)
        {
            root_menu_video_draw_db_menu(title, tag, filter_tag, filter_seek,
                                         rows, row_count, selected_local);
            redraw = false;
        }
        else if (redraw_rows)
        {
            if (!root_menu_video_draw_db_selection_delta(tag, rows, row_count,
                    previous_local, selected_local))
            {
                root_menu_video_draw_db_menu(title, tag, filter_tag,
                                             filter_seek, rows, row_count,
                                             selected_local);
            }
            redraw_rows = false;
        }

        action = get_action(CONTEXT_TREE|ALLOW_SOFTLOCK,
                            root_menu_video_paused_locked() ? HZ : HZ/20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        if (!ipodjs_ui_fast_scroll_active())
            fast_bucket = -1;
        if (action != ACTION_NONE &&
            action != ACTION_STD_PREV &&
            action != ACTION_STD_PREVREPEAT &&
            action != ACTION_STD_NEXT &&
            action != ACTION_STD_NEXTREPEAT)
        {
            ipodjs_ui_fast_scroll_clear();
            fast_bucket = -1;
        }
        switch (action)
        {
            case ACTION_NONE:
                if (ipodjs_ui_fast_scroll_take_expired())
                {
                    fast_bucket = -1;
                    redraw = true;
                }
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
            {
                int modifier = 1;

                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (row_count <= 0)
                    break;
#ifdef HAVE_WHEEL_ACCELERATION
                modifier = button_apply_acceleration(get_action_data());
#else
                if (action == ACTION_STD_PREVREPEAT)
                    modifier = IPODJS_DB_FAST_SCROLL_TRIGGER;
#endif
                if (root_menu_video_db_fast_scroll_eligible(tag) &&
                    (ipodjs_ui_fast_scroll_active() ||
                     modifier >= IPODJS_DB_FAST_SCROLL_TRIGGER))
                {
                    if (fast_total == -2)
                        fast_total = root_menu_video_db_fast_scroll_build(
                            tag, filter_tag, filter_seek,
                            filter_tag2, filter_seek2, fast_first);
                    if (fast_total >= IPODJS_DB_FAST_SCROLL_MIN_ROWS &&
                        root_menu_video_db_fast_scroll_move(
                            rows, row_count, selected_local, -1, fast_first,
                            &fast_bucket, &selected_abs))
                    {
                        window_start = MAX(0, selected_abs -
                                           IPODJS_DB_WINDOW_ROWS / 3);
                        reload = true;
                        redraw = true;
                        break;
                    }
                }
                if (selected_abs <= 0)
                    break;
                previous_local = selected_local;
                selected_abs--;
                selected_local--;
                if (selected_local < IPODJS_DB_WINDOW_MARGIN &&
                    window_start > 0)
                {
                    window_start = MAX(0, selected_abs -
                                       (IPODJS_DB_WINDOW_ROWS * 2) / 3);
                    reload = true;
                }
                if (!reload)
                    redraw_rows = true;
                break;
            }

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
            {
                int modifier = 1;

                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (row_count <= 0)
                    break;
#ifdef HAVE_WHEEL_ACCELERATION
                modifier = button_apply_acceleration(get_action_data());
#else
                if (action == ACTION_STD_NEXTREPEAT)
                    modifier = IPODJS_DB_FAST_SCROLL_TRIGGER;
#endif
                if (root_menu_video_db_fast_scroll_eligible(tag) &&
                    (ipodjs_ui_fast_scroll_active() ||
                     modifier >= IPODJS_DB_FAST_SCROLL_TRIGGER))
                {
                    if (fast_total == -2)
                        fast_total = root_menu_video_db_fast_scroll_build(
                            tag, filter_tag, filter_seek,
                            filter_tag2, filter_seek2, fast_first);
                    if (fast_total >= IPODJS_DB_FAST_SCROLL_MIN_ROWS &&
                        root_menu_video_db_fast_scroll_move(
                            rows, row_count, selected_local, 1, fast_first,
                            &fast_bucket, &selected_abs))
                    {
                        window_start = MAX(0, selected_abs -
                                           IPODJS_DB_WINDOW_ROWS / 3);
                        reload = true;
                        redraw = true;
                        break;
                    }
                }
                if (selected_local >= row_count - 1 && !has_more)
                    break;
                previous_local = selected_local;
                selected_abs++;
                selected_local++;
                if (selected_local >= row_count - IPODJS_DB_WINDOW_MARGIN &&
                    has_more)
                {
                    window_start = MAX(0, selected_abs -
                                       IPODJS_DB_WINDOW_ROWS / 3);
                    reload = true;
                }
                if (!reload)
                    redraw_rows = true;
                break;
            }

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (row_count <= 0)
                {
                    redraw = true;
                    break;
                }
                if (selected_local < row_count)
                {
                    if (tracks)
                    {
                        if (context != IPODJS_DB_CTX_NONE)
                        {
                            return root_menu_video_db_play_album_track(
                                rows[selected_local].idxid, filter_seek,
                                (context == IPODJS_DB_CTX_ALBUMS) ?
                                -1 : filter_tag2,
                                (context == IPODJS_DB_CTX_ALBUMS) ?
                                0 : filter_seek2);
                        }
                        return root_menu_video_db_play_tracks(
                            rows[selected_local].idxid, filter_tag, filter_seek,
                            filter_tag2, filter_seek2);
                    }
                    if (tag == tag_artist)
                    {
                        ipodjs_ui_transition_begin(1);
                        int ret = root_menu_video_db_browser_level("Albums",
                            tag_album, tag_artist, rows[selected_local].seek,
                            -1, 0, false, level + 1);
                        if (ret == GO_TO_PREVIOUS)
                        {
                            ipodjs_ui_transition_begin(-1);
                            redraw = true;
                            break;
                        }
                        return ret;
                    }
                    else if (tag == tag_albumartist)
                    {
                        ipodjs_ui_transition_begin(1);
                        int ret = root_menu_video_db_browser_level("Albums",
                            tag_album, tag_albumartist,
                            rows[selected_local].seek,
                            -1, 0, false, level + 1);
                        if (ret == GO_TO_PREVIOUS)
                        {
                            ipodjs_ui_transition_begin(-1);
                            redraw = true;
                            break;
                        }
                        return ret;
                    }
                    else
                    {
                        ipodjs_ui_transition_begin(1);
                        int ret = root_menu_video_db_browser_level("Songs",
                            tag_title, tag_album, rows[selected_local].seek,
                            filter_tag, filter_seek,
                            true,
                            level + 1);
                        if (ret == GO_TO_PREVIOUS)
                        {
                            ipodjs_ui_transition_begin(-1);
                            redraw = true;
                            break;
                        }
                        return ret;
                    }
                }
                redraw = true;
                break;

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
            {
                bool started_playback = false;

                if (root_menu_video_handle_play_pause(true,
                                                     &started_playback))
                    return GO_TO_WPS;
                redraw = true;
                break;
            }

            case ACTION_STD_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return GO_TO_PREVIOUS;

            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return GO_TO_PREVIOUS;
        }
    }
}

static int root_menu_video_db_browser(const char *title, int tag,
                                      int filter_tag, int filter_seek,
                                      int filter_tag2, int filter_seek2,
                                      bool tracks)
{
    return root_menu_video_db_browser_level(title, tag, filter_tag,
                                            filter_seek, filter_tag2,
                                            filter_seek2, tracks, 0);
}

struct root_menu_video_search_icon_cache {
    struct bitmap bm;
    unsigned char data[
        BM_SIZE(IPODJS_SEARCH_ICON_SIZE, IPODJS_SEARCH_ICON_SIZE,
                FORMAT_NATIVE, false)];
    bool tried;
    bool valid;
};

static struct root_menu_video_search_icon_cache
    root_menu_video_search_icons[IPODJS_SEARCH_TYPE_COUNT];

static const char *root_menu_video_search_icon_path(int type)
{
    switch (type)
    {
        case IPODJS_SEARCH_ARTIST:
            return IPODJS_APPLE_ASSET_DIR
                   "/search-artist.apple.16x16x24.bmp";
        case IPODJS_SEARCH_ALBUM:
            return IPODJS_APPLE_ASSET_DIR
                   "/search-album.apple.16x16x24.bmp";
        case IPODJS_SEARCH_PLAYLIST:
            return IPODJS_APPLE_ASSET_DIR
                   "/search-playlist.apple.16x16x24.bmp";
        case IPODJS_SEARCH_SONG:
        default:
            return IPODJS_APPLE_ASSET_DIR
                   "/search-song.apple.16x16x24.bmp";
    }
}

static struct bitmap *root_menu_video_search_icon(int type)
{
    struct root_menu_video_search_icon_cache *cache;

    type = MAX(0, MIN(type, IPODJS_SEARCH_TYPE_COUNT - 1));
    cache = &root_menu_video_search_icons[type];
    return root_menu_video_load_ui_bmp(
        root_menu_video_search_icon_path(type), &cache->bm, cache->data,
        sizeof(cache->data), IPODJS_SEARCH_ICON_SIZE,
        IPODJS_SEARCH_ICON_SIZE, &cache->tried, &cache->valid);
}

static int root_menu_video_search_font(void)
{
    /* Keep Search on the same locked UI font as every other iPodJS screen.
     * Loading a second font here can evict the list font from Rockbox's small
     * font cache and leave its cached id pointing at the replacement. */
    return root_menu_video_font();
}

bool root_menu_ipodjs_search_available(void)
{
    if (!root_menu_video_enabled() || !tagcache_is_usable())
        return false;

    for (int type = 0; type < IPODJS_SEARCH_TYPE_COUNT; type++)
    {
        if (!file_exists(root_menu_video_search_icon_path(type)))
            return false;
    }
    return ipodjs_ui_search_surfaces_available();
}

static bool root_menu_video_search_is_duplicate(int type,
                                                 const char *label)
{
    for (int i = 0; i < root_menu_video_search_result_count; i++)
    {
        struct root_menu_video_search_result *result =
            &root_menu_video_search_results[i];
        if (result->type == type && !strcasecmp(result->label, label))
            return true;
    }
    return false;
}

static void root_menu_video_search_retrieve_artist(
    struct tagcache_search *tcs, int idxid, char *buffer, size_t size)
{
    buffer[0] = '\0';
    if (!tagcache_retrieve(tcs, idxid, tag_albumartist, buffer, size) ||
        !buffer[0])
    {
        buffer[0] = '\0';
        tagcache_retrieve(tcs, idxid, tag_artist, buffer, size);
    }
}

static void root_menu_video_search_add_tag(const char *query, int tag,
                                           int type)
{
    struct tagcache_search tcs;
    struct tagcache_search_clause clause;
    char label[TAGCACHE_BUFSZ];

    /* Once a prior tag proved that the fixed result window overflowed,
     * later tag indexes cannot add anything visible.  Stock Search only
     * displays a '+' in this case, so rescanning them changes no result. */
    if (root_menu_video_search_result_count >= IPODJS_SEARCH_MAX_RESULTS &&
        root_menu_video_search_total > root_menu_video_search_result_count)
        return;

    memset(&clause, 0, sizeof(clause));
    clause.tag = tag;
    clause.type = clause_contains;
    clause.source = source_constant;
    clause.str = (char *)query;

    if (!tagcache_search(&tcs, tag))
        return;
    if (tag != tag_title && tag != tag_filename)
        tagcache_search_set_uniqbuf(&tcs, root_menu_video_db_uniq,
                                    sizeof(root_menu_video_db_uniq));
    if (!tagcache_search_add_clause(&tcs, &clause))
    {
        tagcache_search_finish(&tcs);
        return;
    }

    while (tagcache_get_next(&tcs, label, sizeof(label)))
    {
        struct root_menu_video_search_result *result;

        if (!label[0] || root_menu_video_search_is_duplicate(type, label))
            continue;
        root_menu_video_search_total++;
        if (root_menu_video_search_result_count >=
            IPODJS_SEARCH_MAX_RESULTS)
            break;

        result = &root_menu_video_search_results[
            root_menu_video_search_result_count++];
        memset(result, 0, sizeof(*result));
        result->type = type;
        result->tag = tag;
        result->seek = tcs.result_seek;
        result->idxid = tcs.idx_id;
        strmemccpy(result->label, label, sizeof(result->label));

        if (type == IPODJS_SEARCH_SONG || type == IPODJS_SEARCH_ALBUM)
            root_menu_video_search_retrieve_artist(
                &tcs, result->idxid, result->sublabel,
                sizeof(result->sublabel));
        else
            strmemccpy(result->sublabel, "Artist",
                       sizeof(result->sublabel));
    }
    tagcache_search_finish(&tcs);
}

static void root_menu_video_search_build(const char *query)
{
    root_menu_video_search_result_count = 0;
    root_menu_video_search_total = 0;
    if (!query || !query[0])
        return;

    root_menu_video_search_add_tag(query, tag_artist,
                                   IPODJS_SEARCH_ARTIST);
    root_menu_video_search_add_tag(query, tag_albumartist,
                                   IPODJS_SEARCH_ARTIST);
    root_menu_video_search_add_tag(query, tag_album,
                                   IPODJS_SEARCH_ALBUM);
    root_menu_video_search_add_tag(query, tag_title,
                                   IPODJS_SEARCH_SONG);

    if (root_menu_video_search_result_count >= IPODJS_SEARCH_MAX_RESULTS &&
        root_menu_video_search_total > root_menu_video_search_result_count)
        return;

    /* Playlist catalogue search is deliberately read-only: unlike
     * catalog_get_directory(), this does not create a missing directory. */
    char directory[MAX_PATH];
    const char *configured = global_settings.playlist_catalog_dir[0] ?
        (const char *)global_settings.playlist_catalog_dir :
        PLAYLIST_CATALOG_DEFAULT_DIR;
    path_append(directory, configured, PA_SEP_SOFT, sizeof(directory));
    DIR *dir = opendir(directory);
    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            char display[96];
            char *extension;
            struct root_menu_video_search_result *result;

            if (entry->d_name[0] == '.')
                continue;
            strmemccpy(display, entry->d_name, sizeof(display));
            extension = strrchr(display, '.');
            if (!extension ||
                (strcasecmp(extension, ".m3u") &&
                 strcasecmp(extension, ".m3u8")))
                continue;
            *extension = '\0';
            if (!strcasestr(display, query))
                continue;

            root_menu_video_search_total++;
            if (root_menu_video_search_result_count >=
                IPODJS_SEARCH_MAX_RESULTS)
                break;
            result = &root_menu_video_search_results[
                root_menu_video_search_result_count++];
            memset(result, 0, sizeof(*result));
            result->type = IPODJS_SEARCH_PLAYLIST;
            strmemccpy(result->label, entry->d_name,
                       sizeof(result->label));
            strmemccpy(result->sublabel, "Playlist",
                       sizeof(result->sublabel));
        }
        closedir(dir);
    }
}

static void root_menu_video_stock_keyboard_draw(const char *query,
                                                 const char *characters,
                                                 int character_count,
                                                 int character)
{
    const int panel_x = 20;
    const int panel_y = 171;
    const int panel_w = 278;
    const int panel_h = 50;
    const int field_x = 27;
    const int field_y = 183;
    const int field_w = 68;
    const int field_h = 25;
    const int center_x = 151;
    int font = root_menu_video_search_font();
    int font_h;
    int query_w;
    int query_h;
    const char *display_query = query;

    if (font < 0)
        return;

    if (!ipodjs_ui_draw_search_surface(&screens[SCREEN_MAIN],
                                       IPODJS_UI_SEARCH_PANEL,
                                       panel_x, panel_y, panel_w, panel_h) ||
        !ipodjs_ui_draw_search_surface(&screens[SCREEN_MAIN],
                                       IPODJS_UI_SEARCH_FIELD,
                                       field_x, field_y, field_w, field_h))
        return;

    lcd_setfont(font);
    font_h = font_get(font)->height;
    lcd_getstringsize(display_query, &query_w, &query_h);
    while (*display_query && query_w > field_w - 8)
    {
        display_query++;
        lcd_getstringsize(display_query, &query_w, &query_h);
    }
    lcd_set_foreground(LCD_RGBPACK(20, 24, 27));
    lcd_set_drawmode(DRMODE_FG);
    lcd_putsxy(MAX(field_x + 4, field_x + field_w - query_w - 4),
               field_y + MAX(0, (field_h - query_h) / 2), display_query);
    lcd_set_drawmode(DRMODE_SOLID);

    for (int offset = -5; offset <= 6; offset++)
    {
        int index = character + offset;
        char glyph[2];
        int glyph_w;
        int glyph_h;
        int x = center_x + offset * 19;

        while (index < 0)
            index += character_count;
        index %= character_count;
        glyph[0] = characters[index];
        glyph[1] = '\0';
        lcd_getstringsize(glyph, &glyph_w, &glyph_h);
        /* The stock query field masks the alphabet to its left; do not paint
         * wheel letters through the white field. */
        if (x < field_x + field_w + 9)
            continue;
        if (offset == 0)
        {
            int selected_w = MAX(glyph_w + 6, 16);
            int selected_x = x - (selected_w - glyph_w) / 2;

            /* Narrow glyphs such as I are smaller than the Apple surface's
             * minimum nine-slice width.  Keep a stock-sized selection pill
             * so every letter remains drawable and selectable. */
            if (!ipodjs_ui_draw_search_surface(
                    &screens[SCREEN_MAIN], IPODJS_UI_SEARCH_SELECTED,
                    selected_x, panel_y + 10, selected_w, font_h + 3))
                return;
            lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
        }
        else
        {
            lcd_set_foreground(LCD_RGBPACK(239, 244, 246));
        }
        lcd_set_drawmode(DRMODE_FG);
        lcd_putsxy(x, panel_y + 11, glyph);
        lcd_set_drawmode(DRMODE_SOLID);
    }
}

bool root_menu_ipodjs_text_input(const char *title, char *text, size_t size)
{
    static const char * const banks[] = {
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        "abcdefghijklmnopqrstuvwxyz",
        "0123456789.-_/:?&=%+"
    };
    int bank = 0;
    int character = 0;
    bool redraw = true;
    bool held = button_hold();

    if (!text || size < 2 || !ipodjs_ui_search_surfaces_available() ||
        !ipodjs_ui_prepare_search_surfaces())
        return false;
    text[size - 1] = '\0';
    button_clear_queue();

    while (true)
    {
        const char *characters = banks[bank];
        int character_count = strlen(characters);
        int action;

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            char help[80];

            lcd_set_viewport(NULL);
            lcd_set_drawmode(DRMODE_SOLID);
            lcd_set_background(root_menu_video_screen_bg());
            lcd_clear_display();
            root_menu_video_draw_status_title(title ? title : "Search");
            lcd_set_foreground(root_menu_video_row_bg());
            lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                         LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
            lcd_setfont(root_menu_video_search_font());
            lcd_set_foreground(root_menu_video_text());
            lcd_set_background(root_menu_video_row_bg());
            root_menu_video_puts_fit(8, IPODJS_HEADER_HEIGHT + 25,
                                     LCD_WIDTH - 16,
                                     text[0] ? text : "Search or enter address",
                                     true);
            snprintf(help, sizeof(help),
                     "Play: keys   Left: delete   Menu: go");
            lcd_set_foreground(root_menu_video_muted_text());
            root_menu_video_puts_fit(8, IPODJS_HEADER_HEIGHT + 60,
                                     LCD_WIDTH - 16, help, true);
            root_menu_video_stock_keyboard_draw(text, characters,
                                                 character_count, character);
            ipodjs_video_draw_hold_overlay();
            lcd_update();
            redraw = false;
        }

        action = get_action(CONTEXT_TREE | ALLOW_SOFTLOCK, HZ / 20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;

        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (!root_menu_video_hold_update(&held, &redraw))
                {
                    character = character <= 0 ? character_count - 1 :
                                                 character - 1;
                    redraw = true;
                }
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (!root_menu_video_hold_update(&held, &redraw))
                {
                    character = (character + 1) % character_count;
                    redraw = true;
                }
                break;
            case ACTION_TREE_PGLEFT:
                if (!root_menu_video_hold_update(&held, &redraw) && text[0])
                {
                    text[strlen(text) - 1] = '\0';
                    redraw = true;
                }
                break;
            case ACTION_TREE_PGRIGHT:
                if (!root_menu_video_hold_update(&held, &redraw) &&
                    strlen(text) + 1 < size)
                {
                    size_t length = strlen(text);
                    text[length] = ' ';
                    text[length + 1] = '\0';
                    redraw = true;
                }
                break;
            case ACTION_STD_OK:
                if (!root_menu_video_hold_update(&held, &redraw) &&
                    strlen(text) + 1 < size)
                {
                    size_t length = strlen(text);
                    text[length] = characters[character];
                    text[length + 1] = '\0';
                    redraw = true;
                }
                break;
            case ACTION_TREE_WPS:
                if (!root_menu_video_hold_update(&held, &redraw))
                {
                    bank = (bank + 1) % ARRAYLEN(banks);
                    character = 0;
                    redraw = true;
                }
                break;
            case ACTION_STD_MENU:
                if (!root_menu_video_hold_update(&held, &redraw) && text[0])
                {
                    button_clear_queue();
                    return true;
                }
                break;
            case ACTION_STD_CONTEXT:
                if (!root_menu_video_hold_update(&held, &redraw))
                {
                    button_clear_queue();
                    return false;
                }
                break;
        }
    }
}

static void root_menu_video_search_draw(const char *query, bool entering,
                                        int character, int selected)
{
    int content_bottom = entering ? 171 : LCD_HEIGHT;
    int visible = MAX(1, (content_bottom - IPODJS_HEADER_HEIGHT) /
                         IPODJS_SEARCH_ROW_HEIGHT);
    int top = selected >= visible ? selected - visible + 1 : 0;
    char title[48];

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();
    if (query[0])
        snprintf(title, sizeof(title), "Search Results: %d%s",
                 root_menu_video_search_result_count,
                 root_menu_video_search_total >
                    root_menu_video_search_result_count ? "+" : "");
    else
        strmemccpy(title, "Search", sizeof(title));
    root_menu_video_draw_status_title(title);

    lcd_set_foreground(root_menu_video_row_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
    lcd_setfont(root_menu_video_font());

    if (!query[0] || root_menu_video_search_result_count == 0)
    {
        const char *message = query[0] ? "No Results" :
            "Select letters with the Click Wheel";
        lcd_set_foreground(root_menu_video_muted_text());
        lcd_set_background(root_menu_video_row_bg());
        root_menu_video_puts_fit(4, IPODJS_HEADER_HEIGHT + 35,
                                 LCD_WIDTH - 8, message, true);
    }

    for (int row = 0; row < visible && top + row <
         root_menu_video_search_result_count; row++)
    {
        int index = top + row;
        int y = IPODJS_HEADER_HEIGHT + row * IPODJS_SEARCH_ROW_HEIGHT;
        bool active = !entering && index == selected;
        struct root_menu_video_search_result *result =
            &root_menu_video_search_results[index];
        struct bitmap *icon = root_menu_video_search_icon(result->type);
        char display_label[sizeof(result->label)];
        const char *label = result->label;

        if (result->type == IPODJS_SEARCH_PLAYLIST)
        {
            char *extension;
            strmemccpy(display_label, result->label,
                       sizeof(display_label));
            extension = strrchr(display_label, '.');
            if (extension)
                *extension = '\0';
            label = display_label;
        }

        if (active)
            root_menu_video_selection_gradient(0, y, LCD_WIDTH,
                                               IPODJS_SEARCH_ROW_HEIGHT);
        else
        {
            lcd_set_foreground(root_menu_video_row_bg());
            lcd_fillrect(0, y, LCD_WIDTH, IPODJS_SEARCH_ROW_HEIGHT);
            lcd_set_foreground(root_menu_video_dark() ?
                LCD_RGBPACK(44, 49, 57) : LCD_RGBPACK(222, 225, 229));
            lcd_hline(0, LCD_WIDTH - 1,
                      y + IPODJS_SEARCH_ROW_HEIGHT - 1);
        }
        if (icon)
            lcd_bmp(icon, 8, y + 10);
        lcd_set_foreground(active ? LCD_RGBPACK(255, 255, 255) :
                                      root_menu_video_text());
        lcd_set_background(active ? root_menu_video_accent() :
                                      root_menu_video_row_bg());
        root_menu_video_puts_fit(31, y + 3, LCD_WIDTH - 38,
                                 label, false);
        lcd_set_foreground(active ? LCD_RGBPACK(224, 242, 255) :
                                      root_menu_video_muted_text());
        root_menu_video_puts_fit(31, y + 19, LCD_WIDTH - 38,
                                 result->sublabel, false);
    }

    if (entering)
        root_menu_video_stock_keyboard_draw(
            query, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26, character);
    ipodjs_video_draw_hold_overlay();
    lcd_update();
    ipodjs_trace_screen("Search", entering ? "input" : "results",
                        entering ? character : selected, top,
                        root_menu_video_search_result_count,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static int root_menu_video_search_open_result(int selected)
{
    struct root_menu_video_search_result *result;

    if (selected < 0 || selected >= root_menu_video_search_result_count)
        return GO_TO_PREVIOUS;
    result = &root_menu_video_search_results[selected];
    if (result->type == IPODJS_SEARCH_SONG)
        return root_menu_video_db_play_tracks(result->idxid,
                                              -1, 0, -1, 0);
    if (result->type == IPODJS_SEARCH_ARTIST)
        return root_menu_video_db_browser_level(
            "Albums", tag_album, result->tag, result->seek,
            -1, 0, false, 1);
    if (result->type == IPODJS_SEARCH_PLAYLIST)
    {
        char directory[MAX_PATH];
        char path[MAX_PATH];
        const char *configured = global_settings.playlist_catalog_dir[0] ?
            (const char *)global_settings.playlist_catalog_dir :
            PLAYLIST_CATALOG_DEFAULT_DIR;
        path_append(directory, configured, PA_SEP_SOFT,
                    sizeof(directory));
        path_append(path, directory, result->label, sizeof(path));
        playlist_viewer_ex(path, NULL);
        return GO_TO_PREVIOUS;
    }
    return root_menu_video_db_browser_level(
        "Songs", tag_title, tag_album, result->seek,
        -1, 0, true, 1);
}

int root_menu_ipodjs_search(void)
{
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char query[IPODJS_SEARCH_QUERY_SIZE] = "";
    int character = 0;
    int selected = 0;
    bool entering = true;
    bool redraw = true;
    bool held = button_hold();
    long next_hold_refresh = 0;

    if (!root_menu_ipodjs_search_available() ||
        !ipodjs_ui_prepare_search_surfaces())
        return GO_TO_PREVIOUS;
    root_menu_video_search_build(query);
    button_clear_queue();

    while (true)
    {
        int action;

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_search_draw(query, entering, character,
                                        selected);
            redraw = false;
        }
        action = get_action(CONTEXT_TREE|ALLOW_SOFTLOCK, HZ/20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;

        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (entering)
                    character = character <= 0 ? 25 : character - 1;
                else if (root_menu_video_search_result_count > 0)
                    selected = selected <= 0 ?
                        root_menu_video_search_result_count - 1 :
                        selected - 1;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (entering)
                    character = (character + 1) % 26;
                else if (root_menu_video_search_result_count > 0)
                    selected = (selected + 1) %
                        root_menu_video_search_result_count;
                redraw = true;
                break;

            case ACTION_TREE_PGLEFT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (entering && query[0])
                {
                    query[strlen(query) - 1] = '\0';
                    root_menu_video_search_build(query);
                    selected = 0;
                    redraw = true;
                }
                break;

            case ACTION_TREE_PGRIGHT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (entering && query[0] &&
                    strlen(query) + 1 < sizeof(query))
                {
                    size_t length = strlen(query);
                    query[length] = ' ';
                    query[length + 1] = '\0';
                    root_menu_video_search_build(query);
                    selected = 0;
                    redraw = true;
                }
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (entering)
                {
                    size_t length = strlen(query);
                    if (length + 1 < sizeof(query))
                    {
                        query[length] = alphabet[character];
                        query[length + 1] = '\0';
                        root_menu_video_search_build(query);
                        selected = 0;
                        redraw = true;
                    }
                }
                else if (root_menu_video_search_result_count > 0)
                {
                    int result;
                    ipodjs_ui_transition_begin(1);
                    result = root_menu_video_search_open_result(selected);
                    if (result == GO_TO_WPS)
                        return GO_TO_WPS;
                    ipodjs_ui_transition_begin(-1);
                    redraw = true;
                }
                break;

            case ACTION_TREE_WPS:
                if (!root_menu_video_hold_update(&held, &redraw))
                {
                    (void)root_menu_video_handle_play_pause(false, NULL);
                    redraw = true;
                }
                break;

            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (!entering)
                {
                    entering = true;
                    redraw = true;
                }
                else if (query[0] && root_menu_video_search_result_count > 0)
                {
                    entering = false;
                    selected = MIN(selected,
                                   root_menu_video_search_result_count - 1);
                    redraw = true;
                }
                else
                    return GO_TO_PREVIOUS;
                break;
        }
    }
}

#define IPODJS_QRCODE_TEXT_SIZE 256
#define IPODJS_QRCODE_QUIET_ZONE 4
#define IPODJS_QRCODE_SAVE_SCALE 4
#define IPODJS_QRCODE_SAVE_DIR HOME_DIR "QR Codes"
#define IPODJS_QRCODE_META_DIR ROCKBOX_DIR "/qrcodes"
#define IPODJS_QRCODE_MAX_SAVED 64
#define IPODJS_QRCODE_LABEL_SIZE 32

struct root_menu_video_qrcode_bank
{
    const char *label;
    const char *characters;
};

static const struct root_menu_video_qrcode_bank
root_menu_video_qrcode_banks[] = {
    { "ABC", "ABCDEFGHIJKLMNOPQRSTUVWXYZ" },
    { "abc", "abcdefghijklmnopqrstuvwxyz" },
    { "123", "0123456789" },
    { "#+=", ":/.-_?&=%+@#" },
};

static uint8_t root_menu_video_qrcode_temp[qrcodegen_BUFFER_LEN_MAX];
static uint8_t root_menu_video_qrcode_data[qrcodegen_BUFFER_LEN_MAX];
static char root_menu_video_qrcode_saved[IPODJS_QRCODE_MAX_SAVED]
                                          [IPODJS_QRCODE_LABEL_SIZE];

static void root_menu_video_qrcode_le16(unsigned char *buffer,
                                        unsigned value)
{
    buffer[0] = value & 0xff;
    buffer[1] = (value >> 8) & 0xff;
}

static void root_menu_video_qrcode_le32(unsigned char *buffer,
                                        uint32_t value)
{
    buffer[0] = value & 0xff;
    buffer[1] = (value >> 8) & 0xff;
    buffer[2] = (value >> 16) & 0xff;
    buffer[3] = (value >> 24) & 0xff;
}

static bool root_menu_video_qrcode_write_all(int fd, const void *buffer,
                                              size_t size)
{
    const unsigned char *data = buffer;

    while (size > 0)
    {
        ssize_t written = write(fd, data, size);

        if (written <= 0)
            return false;
        data += written;
        size -= written;
    }
    return true;
}

static bool root_menu_video_qrcode_meta_path(const char *image_path,
                                              char *meta_path,
                                              size_t meta_size)
{
    const char *name = strrchr(image_path, '/');
    char stem[IPODJS_QRCODE_LABEL_SIZE];
    char *extension;

    name = name ? name + 1 : image_path;
    strmemccpy(stem, name, sizeof(stem));
    extension = strrchr(stem, '.');
    if (!extension || strcasecmp(extension, ".bmp"))
        return false;
    *extension = '\0';
    return snprintf(meta_path, meta_size, "%s/%s.txt",
                    IPODJS_QRCODE_META_DIR, stem) < (int)meta_size;
}

static bool root_menu_video_qrcode_save_source(const char *image_path,
                                                const char *text)
{
    char meta_path[MAX_PATH];
    char temp_path[MAX_PATH];
    int fd;
    bool ok;

    if (mkdir(ROCKBOX_DIR) < 0 && !dir_exists(ROCKBOX_DIR))
        return false;
    if (mkdir(IPODJS_QRCODE_META_DIR) < 0 &&
        !dir_exists(IPODJS_QRCODE_META_DIR))
        return false;
    if (!root_menu_video_qrcode_meta_path(image_path, meta_path,
                                           sizeof(meta_path)) ||
        snprintf(temp_path, sizeof(temp_path), "%s.tmp", meta_path) >=
            (int)sizeof(temp_path))
        return false;

    fd = creat(temp_path, 0666);
    if (fd < 0)
        return false;
    ok = root_menu_video_qrcode_write_all(fd, text, strlen(text));
    if (close(fd) < 0)
        ok = false;
    if (ok)
    {
        remove(meta_path);
        ok = rename(temp_path, meta_path) >= 0;
    }
    if (!ok)
        remove(temp_path);
    return ok;
}

static bool root_menu_video_qrcode_save(char *path, size_t path_size,
                                         const char *text)
{
    unsigned char header[62] = { 0 };
    unsigned char row[((qrcodegen_VERSION_MAX * 4 + 17 +
                        IPODJS_QRCODE_QUIET_ZONE * 2) *
                       IPODJS_QRCODE_SAVE_SCALE + 31) / 32 * 4];
    int modules = qrcodegen_getSize(root_menu_video_qrcode_data);
    int total_modules = modules + IPODJS_QRCODE_QUIET_ZONE * 2;
    int pixels = total_modules * IPODJS_QRCODE_SAVE_SCALE;
    int row_size = ((pixels + 31) / 32) * 4;
    uint32_t image_size = row_size * pixels;
    int fd;
    bool ok = true;

    if (modules <= 0 || path_size < MAX_PATH)
        return false;
    if (mkdir(IPODJS_QRCODE_SAVE_DIR) < 0 &&
        !dir_exists(IPODJS_QRCODE_SAVE_DIR))
        return false;
    if (!create_numbered_filename(path, IPODJS_QRCODE_SAVE_DIR,
                                  "QR Code ", ".bmp", 3
                                  IF_CNFN_NUM_(, NULL)))
        return false;

    header[0] = 'B';
    header[1] = 'M';
    root_menu_video_qrcode_le32(header + 2, 62 + image_size);
    root_menu_video_qrcode_le32(header + 10, 62);
    root_menu_video_qrcode_le32(header + 14, 40);
    root_menu_video_qrcode_le32(header + 18, pixels);
    root_menu_video_qrcode_le32(header + 22, pixels);
    root_menu_video_qrcode_le16(header + 26, 1);
    root_menu_video_qrcode_le16(header + 28, 1);
    root_menu_video_qrcode_le32(header + 34, image_size);
    root_menu_video_qrcode_le32(header + 38, 3780);
    root_menu_video_qrcode_le32(header + 42, 3780);
    root_menu_video_qrcode_le32(header + 46, 2);
    root_menu_video_qrcode_le32(header + 50, 2);
    header[54] = 0xff;
    header[55] = 0xff;
    header[56] = 0xff;
    header[57] = 0x00;
    header[58] = 0x00;
    header[59] = 0x00;
    header[60] = 0x00;
    header[61] = 0x00;

    fd = creat(path, 0666);
    if (fd < 0)
        return false;
    ok = root_menu_video_qrcode_write_all(fd, header, sizeof(header));

    for (int pixel_y = pixels - 1; ok && pixel_y >= 0; pixel_y--)
    {
        int module_y = pixel_y / IPODJS_QRCODE_SAVE_SCALE -
                       IPODJS_QRCODE_QUIET_ZONE;

        memset(row, 0, row_size);
        if (module_y >= 0 && module_y < modules)
        {
            for (int pixel_x = 0; pixel_x < pixels; pixel_x++)
            {
                int module_x = pixel_x / IPODJS_QRCODE_SAVE_SCALE -
                               IPODJS_QRCODE_QUIET_ZONE;

                if (module_x >= 0 && module_x < modules &&
                    qrcodegen_getModule(root_menu_video_qrcode_data,
                                        module_x, module_y))
                    row[pixel_x >> 3] |= 0x80 >> (pixel_x & 7);
            }
        }
        ok = root_menu_video_qrcode_write_all(fd, row, row_size);
    }
    if (close(fd) < 0)
        ok = false;
    if (ok)
        ok = root_menu_video_qrcode_save_source(path, text);
    if (!ok)
        remove(path);
    return ok;
}

static int root_menu_video_qrcode_saved_compare(const void *a,
                                                 const void *b)
{
    return strcasecmp(a, b);
}

static int root_menu_video_qrcode_load_saved(void)
{
    DIR *directory = opendir(IPODJS_QRCODE_SAVE_DIR);
    struct dirent *entry;
    int count = 0;

    memset(root_menu_video_qrcode_saved, 0,
           sizeof(root_menu_video_qrcode_saved));
    if (!directory)
        return 0;
    while (count < IPODJS_QRCODE_MAX_SAVED &&
           (entry = readdir(directory)) != NULL)
    {
        char image_path[MAX_PATH];
        char meta_path[MAX_PATH];
        char label[IPODJS_QRCODE_LABEL_SIZE];
        char *extension;

        if (strncasecmp(entry->d_name, "QR Code ", 8))
            continue;
        strmemccpy(label, entry->d_name, sizeof(label));
        extension = strrchr(label, '.');
        if (!extension || strcasecmp(extension, ".bmp"))
            continue;
        *extension = '\0';
        if (snprintf(image_path, sizeof(image_path), "%s/%s",
                     IPODJS_QRCODE_SAVE_DIR, entry->d_name) >=
            (int)sizeof(image_path) ||
            !root_menu_video_qrcode_meta_path(image_path, meta_path,
                                               sizeof(meta_path)) ||
            !file_exists(meta_path))
            continue;
        strmemccpy(root_menu_video_qrcode_saved[count], label,
                   sizeof(root_menu_video_qrcode_saved[count]));
        count++;
    }
    closedir(directory);
    qsort(root_menu_video_qrcode_saved, count,
          sizeof(root_menu_video_qrcode_saved[0]),
          root_menu_video_qrcode_saved_compare);
    return count;
}

static bool root_menu_video_qrcode_load_source(const char *label,
                                                char *text,
                                                size_t text_size)
{
    char image_path[MAX_PATH];
    char meta_path[MAX_PATH];
    off_t file_size;
    int fd;
    ssize_t read_size;

    if (snprintf(image_path, sizeof(image_path), "%s/%s.bmp",
                 IPODJS_QRCODE_SAVE_DIR, label) >=
        (int)sizeof(image_path) ||
        !root_menu_video_qrcode_meta_path(image_path, meta_path,
                                           sizeof(meta_path)))
        return false;
    fd = open(meta_path, O_RDONLY);
    if (fd < 0)
        return false;
    file_size = filesize(fd);
    if (file_size < 0 || file_size >= (off_t)text_size)
    {
        close(fd);
        return false;
    }
    read_size = read(fd, text, file_size);
    close(fd);
    if (read_size != file_size)
        return false;
    text[read_size] = '\0';
    return text[0] != '\0';
}

static bool root_menu_video_qrcode_encode(const char *text)
{
    memset(root_menu_video_qrcode_temp, 0,
           sizeof(root_menu_video_qrcode_temp));
    memset(root_menu_video_qrcode_data, 0,
           sizeof(root_menu_video_qrcode_data));
    return qrcodegen_encodeText(
        text, root_menu_video_qrcode_temp, root_menu_video_qrcode_data,
        qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);
}

static void root_menu_video_qrcode_draw_input(const char *text, int bank,
                                               int character)
{
    const struct root_menu_video_qrcode_bank *active =
        &root_menu_video_qrcode_banks[bank];
    char detail[48];

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();
    root_menu_video_draw_status_title("QR Code");

    lcd_set_foreground(root_menu_video_row_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_text());
    lcd_set_background(root_menu_video_row_bg());
    root_menu_video_puts_fit(8, IPODJS_HEADER_HEIGHT + 18,
                             LCD_WIDTH - 16,
                             text[0] ? "Enter QR code text" :
                                       "Create a QR code", true);
    lcd_set_foreground(root_menu_video_muted_text());
    root_menu_video_puts_fit(8, IPODJS_HEADER_HEIGHT + 45,
                             LCD_WIDTH - 16,
                             "Wheel chooses  |  Center types", true);
    root_menu_video_puts_fit(8, IPODJS_HEADER_HEIGHT + 64,
                             LCD_WIDTH - 16,
                             "Prev deletes  |  Next adds space", true);
    root_menu_video_puts_fit(8, IPODJS_HEADER_HEIGHT + 83,
                             LCD_WIDTH - 16,
                             "Play changes keys  |  Menu creates", true);

    snprintf(detail, sizeof(detail), "%s  %d/%d", active->label,
             (int)strlen(text), IPODJS_QRCODE_TEXT_SIZE - 1);
    root_menu_video_puts_fit(8, IPODJS_HEADER_HEIGHT + 112,
                             LCD_WIDTH - 16, detail, true);

    root_menu_video_stock_keyboard_draw(
        text, active->characters, strlen(active->characters), character);
    ipodjs_video_draw_hold_overlay();
    lcd_update();
    ipodjs_trace_screen("QR Code", "input", bank, character,
                        strlen(text), 0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static void root_menu_video_qrcode_draw_code(const char *text, bool can_save)
{
    int size = qrcodegen_getSize(root_menu_video_qrcode_data);
    int total_modules = size + IPODJS_QRCODE_QUIET_ZONE * 2;
    int content_h = LCD_HEIGHT - IPODJS_HEADER_HEIGHT;
    int scale = MIN(LCD_WIDTH / total_modules,
                    content_h / total_modules);
    int drawn = total_modules * scale;
    int left = (LCD_WIDTH - drawn) / 2;
    int top = IPODJS_HEADER_HEIGHT + (content_h - drawn) / 2;
    int origin_x = left + IPODJS_QRCODE_QUIET_ZONE * scale;
    int origin_y = top + IPODJS_QRCODE_QUIET_ZONE * scale;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(LCD_RGBPACK(255, 255, 255));
    lcd_clear_display();
    root_menu_video_draw_status_title(can_save ? "QR - Center Saves" :
                                                 "QR Code");
    lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH, content_h);

    lcd_set_foreground(LCD_RGBPACK(0, 0, 0));
    for (int y = 0; y < size; y++)
    {
        int x = 0;

        while (x < size)
        {
            int start;

            while (x < size &&
                   !qrcodegen_getModule(root_menu_video_qrcode_data, x, y))
                x++;
            start = x;
            while (x < size &&
                   qrcodegen_getModule(root_menu_video_qrcode_data, x, y))
                x++;
            if (start < x)
                lcd_fillrect(origin_x + start * scale,
                             origin_y + y * scale,
                             (x - start) * scale, scale);
        }
    }

    ipodjs_video_draw_hold_overlay();
    lcd_update();
    ipodjs_trace_screen("QR Code", "display", size, scale,
                        strlen(text), left, top, drawn, drawn);
}

static void root_menu_video_qrcode_clear_buffers(void)
{
    memset(root_menu_video_qrcode_temp, 0,
           sizeof(root_menu_video_qrcode_temp));
    memset(root_menu_video_qrcode_data, 0,
           sizeof(root_menu_video_qrcode_data));
}

enum root_menu_video_qrcode_editor_result {
    IPODJS_QRCODE_EDITOR_BACK = 0,
    IPODJS_QRCODE_EDITOR_SAVED,
};

static int root_menu_video_qrcode_editor(const char *initial_text)
{
    char text[IPODJS_QRCODE_TEXT_SIZE] = "";
    int bank = 0;
    int character = 0;
    bool is_new = initial_text == NULL;
    bool displaying = !is_new;
    bool redraw = true;
    bool held = button_hold();
    long next_hold_refresh = 0;

    if (initial_text)
    {
        strmemccpy(text, initial_text, sizeof(text));
        if (!root_menu_video_qrcode_encode(text))
        {
            memset(text, 0, sizeof(text));
            root_menu_video_qrcode_clear_buffers();
            return IPODJS_QRCODE_EDITOR_BACK;
        }
    }
    button_clear_queue();

    while (true)
    {
        const struct root_menu_video_qrcode_bank *active =
            &root_menu_video_qrcode_banks[bank];
        int character_count = strlen(active->characters);
        int action;

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            if (displaying)
                root_menu_video_qrcode_draw_code(text, is_new);
            else
                root_menu_video_qrcode_draw_input(text, bank, character);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE | ALLOW_SOFTLOCK, HZ / 20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;

        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw) || displaying)
                    break;
                character = character <= 0 ? character_count - 1 :
                                             character - 1;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw) || displaying)
                    break;
                character = (character + 1) % character_count;
                redraw = true;
                break;

            case ACTION_TREE_PGLEFT:
                if (root_menu_video_hold_update(&held, &redraw) || displaying)
                    break;
                if (text[0])
                {
                    text[strlen(text) - 1] = '\0';
                    redraw = true;
                }
                break;

            case ACTION_TREE_PGRIGHT:
                if (root_menu_video_hold_update(&held, &redraw) || displaying)
                    break;
                if (strlen(text) + 1 < sizeof(text))
                {
                    size_t length = strlen(text);
                    text[length] = ' ';
                    text[length + 1] = '\0';
                    redraw = true;
                }
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (displaying)
                {
                    char path[MAX_PATH];

                    if (!is_new)
                        break;
                    if (root_menu_video_qrcode_save(path, sizeof(path), text))
                    {
                        splashf(HZ * 2, "Saved\n%s", path);
                        ipodjs_trace_screen("QR Code", "saved",
                            qrcodegen_getSize(root_menu_video_qrcode_data),
                            IPODJS_QRCODE_SAVE_SCALE, strlen(text),
                            0, 0, 0, 0);
                        memset(text, 0, sizeof(text));
                        root_menu_video_qrcode_clear_buffers();
                        return IPODJS_QRCODE_EDITOR_SAVED;
                    }
                    else
                        splash(HZ * 2, "Could not save QR code");
                    redraw = true;
                }
                else if (strlen(text) + 1 < sizeof(text))
                {
                    size_t length = strlen(text);
                    text[length] = active->characters[character];
                    text[length + 1] = '\0';
                    redraw = true;
                }
                break;

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw) || displaying)
                    break;
                bank = (bank + 1) % ARRAYLEN(root_menu_video_qrcode_banks);
                character = 0;
                redraw = true;
                break;

            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (displaying)
                {
                    if (is_new)
                    {
                        displaying = false;
                        redraw = true;
                    }
                    else
                    {
                        memset(text, 0, sizeof(text));
                        root_menu_video_qrcode_clear_buffers();
                        return IPODJS_QRCODE_EDITOR_BACK;
                    }
                }
                else if (!text[0])
                {
                    memset(text, 0, sizeof(text));
                    root_menu_video_qrcode_clear_buffers();
                    return IPODJS_QRCODE_EDITOR_BACK;
                }
                else if (root_menu_video_qrcode_encode(text))
                {
                    displaying = true;
                    redraw = true;
                }
                else
                {
                    splash(HZ * 2, "Text is too long for a QR code");
                    redraw = true;
                }
                break;
        }
    }
}

static const char *root_menu_video_qrcode_list_label(int index)
{
    return index == 0 ? "Add QR Code" :
                        root_menu_video_qrcode_saved[index - 1];
}

static void root_menu_video_qrcode_draw_list_row(int index, int screen_row,
                                                  int row_h, bool active)
{
    int y = IPODJS_HEADER_HEIGHT + screen_row * row_h;

    if (active)
    {
        unsigned accent = root_menu_video_accent();
        root_menu_video_selection_gradient(0, y, LCD_WIDTH, row_h);
        lcd_set_foreground(IPODJS_PREVIEW_TEXT);
        lcd_set_background(accent);
    }
    else
    {
        lcd_set_foreground(root_menu_video_row_bg());
        lcd_fillrect(0, y, LCD_WIDTH, row_h);
        lcd_set_foreground(root_menu_video_text());
        lcd_set_background(root_menu_video_row_bg());
    }

    root_menu_video_puts_fit(7, y + 4, LCD_WIDTH - 28,
                             root_menu_video_qrcode_list_label(index), false);
    if (active)
        root_menu_video_draw_arrow(LCD_WIDTH - 16,
                                   y + (row_h - 6) / 2);
}

static void root_menu_video_qrcode_draw_list(int selected, int saved_count)
{
    int count = saved_count + 1;
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int top = root_menu_video_list_top(selected, visible);

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();
    root_menu_video_draw_status_title("QR Codes");

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_row_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
    for (int i = 0; i < visible && top + i < count; i++)
    {
        int index = top + i;
        root_menu_video_qrcode_draw_list_row(index, i, row_h,
                                              index == selected);
    }

    ipodjs_video_draw_hold_overlay();
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
    ipodjs_trace_screen("QR Codes", "list", selected, top, count,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static int root_menu_video_qrcode_screen(void)
{
    char source[IPODJS_QRCODE_TEXT_SIZE];
    int saved_count;
    int selected = 0;
    bool redraw = true;
    bool held = button_hold();
    long next_hold_refresh = 0;

    root_menu_video_enter_native_screen();
    if (!ipodjs_ui_search_surfaces_available() ||
        !ipodjs_ui_prepare_search_surfaces())
    {
        splash(HZ * 2, "QR keyboard assets missing");
        return root_menu_video_finish_native_screen(GO_TO_PREVIOUS);
    }
    saved_count = root_menu_video_qrcode_load_saved();
    button_clear_queue();

    while (true)
    {
        int action;
        int count = saved_count + 1;

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_qrcode_draw_list(selected, saved_count);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE | ALLOW_SOFTLOCK, HZ / 20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;

        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (!root_menu_video_hold_update(&held, &redraw))
                {
                    selected = MAX(0, selected - 1);
                    redraw = true;
                }
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (!root_menu_video_hold_update(&held, &redraw))
                {
                    selected = MIN(count - 1, selected + 1);
                    redraw = true;
                }
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                memset(source, 0, sizeof(source));
                if (selected == 0)
                {
                    if (root_menu_video_qrcode_editor(NULL) ==
                        IPODJS_QRCODE_EDITOR_SAVED)
                    {
                        saved_count = root_menu_video_qrcode_load_saved();
                        selected = saved_count;
                    }
                }
                else if (!root_menu_video_qrcode_load_source(
                             root_menu_video_qrcode_saved[selected - 1],
                             source, sizeof(source)))
                    splash(HZ * 2, "Could not open saved QR code");
                else
                    (void)root_menu_video_qrcode_editor(source);
                memset(source, 0, sizeof(source));
                button_clear_queue();
                redraw = true;
                break;

            case ACTION_TREE_WPS:
                if (!root_menu_video_hold_update(&held, &redraw))
                {
                    (void)root_menu_video_handle_play_pause(false, NULL);
                    redraw = true;
                }
                break;

            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                if (!root_menu_video_hold_update(&held, &redraw))
                {
                    memset(source, 0, sizeof(source));
                    memset(root_menu_video_qrcode_saved, 0,
                           sizeof(root_menu_video_qrcode_saved));
                    root_menu_video_qrcode_clear_buffers();
                    return root_menu_video_finish_native_screen(
                        GO_TO_PREVIOUS);
                }
                break;
        }
    }
}
#endif /* HAVE_TAGCACHE */

static int root_menu_video_music_menu(void)
{
    int selected = 0;
    int previous_selected = 0;
    bool redraw = true;
    bool redraw_rows = false;
    bool held = button_hold();
    long next_hold_refresh = 0;
    struct root_menu_video_list_draw_state draw_state = {
        .valid = false
    };

    root_menu_video_enter_native_screen();
    root_menu_video_ensure_aa_slot();
    button_clear_queue();

    while (true)
    {
        int action;
        int count = root_menu_video_music_count();

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_music_menu(selected, &draw_state);
            redraw = false;
            redraw_rows = false;
        }
        else if (redraw_rows)
        {
            root_menu_video_draw_music_selection_delta(previous_selected,
                                                      selected, &draw_state);
            redraw_rows = false;
        }

        action = get_action(CONTEXT_TREE, HZ/20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MAX(0, selected - 1);
                if (draw_state.valid &&
                    root_menu_video_music_top(selected,
                        draw_state.visible) == draw_state.top)
                    redraw_rows = true;
                else
                    redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MIN(count - 1, selected + 1);
                if (draw_state.valid &&
                    root_menu_video_music_top(selected,
                        draw_state.visible) == draw_state.top)
                    redraw_rows = true;
                else
                    redraw = true;
                break;

            case ACTION_STD_OK:
            {
                int screen = root_menu_video_music_items[selected].screen;
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                ipodjs_ui_transition_begin(1);
#ifdef HAVE_TAGCACHE
                if (screen < 0)
                {
                    int ret = GO_TO_ROOT;
                    switch (-screen)
                    {
                        case IPODJS_MUSIC_NATIVE_ARTISTS:
                            ret = root_menu_video_db_browser("Artists",
                                tag_artist, -1, 0, -1, 0, false);
                            break;
                        case IPODJS_MUSIC_NATIVE_ALBUM_ARTISTS:
                            ret = root_menu_video_db_browser("Album Artists",
                                tag_albumartist, -1, 0, -1, 0, false);
                            break;
                        case IPODJS_MUSIC_NATIVE_ALBUMS:
                            ret = root_menu_video_db_browser("Albums",
                                tag_album, -1, 0, -1, 0, false);
                            break;
                        case IPODJS_MUSIC_NATIVE_SONGS:
                            ret = root_menu_video_db_browser("Songs",
                                tag_title, -1, 0, -1, 0, true);
                            break;
                        default:
                            ret = GO_TO_ROOT;
                            break;
                    }
                    if (ret == GO_TO_PREVIOUS)
                    {
                        ipodjs_ui_transition_begin(-1);
                        redraw = true;
                        break;
                    }
                    return root_menu_video_finish_native_screen(ret);
                }
#endif
                return root_menu_video_finish_native_screen(screen);
            }

            case ACTION_STD_CONTEXT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (root_menu_video_music_items[selected].screen == GO_TO_WPS)
                {
                    int ret;

                    root_menu_video_finish_native_screen(0);
                    ret = launch_lrcplayer_plugin(NULL);
                    root_menu_video_enter_native_screen();
                    if (ret == GO_TO_ROOT || ret == GO_TO_WPS)
                        return root_menu_video_finish_native_screen(ret);
                    redraw = true;
                    break;
                }
                redraw = true;
                break;

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
            {
                bool started_playback = false;

                if (root_menu_video_handle_play_pause(true,
                                                     &started_playback))
                    return root_menu_video_finish_native_screen(GO_TO_WPS);
                redraw = true;
                break;
            }

            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_ROOT);
        }
    }
}

static int root_menu_video_launch_menu_item(const struct menu_item_ex *item)
{
    int type = item->flags & MENU_TYPE_MASK;

    if (root_menu_video_item_is_extras(item))
        return root_menu_video_extras_menu();

    /* Steam appearance owns every Games entry point, including a customized
     * Home menu's Games/PokeMini rows. Classic keeps the direct Cover Flow
     * and console-launcher behavior below. */
    if (root_menu_video_item_is_games(item) &&
        global_settings.ui_engine_games_appearance ==
            UI_ENGINE_GAMES_STEAM)
        return root_menu_video_games_menu();

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 220)
    if (item == &gameboy_browser)
    {
        int ret;
        root_menu_video_finish_native_screen(0);
        ret = launch_gameboy_browser(NULL);
        return ret ? ret : GO_TO_ROOT;
    }

    if (item == &pokemini_item)
    {
        int ret;
        root_menu_video_finish_native_screen(0);
        ret = launch_pokemini(NULL);
        return ret ? ret : GO_TO_ROOT;
    }
#endif

    if (root_menu_video_item_is_games(item))
        return root_menu_video_games_menu();

    if (type == MT_RETURN_VALUE)
    {
#ifdef HAVE_TAGCACHE
        if (item->value == GO_TO_DBBROWSER)
        {
            if (root_menu_video_uses_stock_music())
                return GO_TO_DBBROWSER;
            return root_menu_video_music_menu();
        }
#endif
        if (item->value == GO_TO_MAINMENU)
        {
            if (videos_settings_lock_active() &&
                !videos_unlock_settings_menu())
                return GO_TO_ROOT;
            return root_menu_video_settings_menu();
        }
        return item->value;
    }

    if (type == MT_FUNCTION_CALL_W_PARAM)
    {
        int ret = item->function_param->function_w_param(item->function_param->param);
        if ((item->flags & MENU_FUNC_CHECK_RETVAL) && ret != 0)
            return ret;
        return GO_TO_ROOT;
    }

    if (type == MT_FUNCTION_CALL)
    {
        int ret = item->function->function();
        if ((item->flags & MENU_FUNC_CHECK_RETVAL) && ret != 0)
            return ret;
        return GO_TO_ROOT;
    }

    if (type == MT_MENU)
        return do_menu(item, NULL, NULL, false);

    return GO_TO_ROOT;
}

struct root_menu_video_extras_item {
    const char *label;
    int screen;
};

enum {
    IPODJS_EXTRAS_CLOCK = -1000,
    IPODJS_EXTRAS_GAMES,
    IPODJS_EXTRAS_ACHIEVEMENTS,
    IPODJS_EXTRAS_APPLICATIONS,
    IPODJS_EXTRAS_MAGAZINES,
    IPODJS_EXTRAS_COMICS,
};

static const struct root_menu_video_extras_item root_menu_video_extras_items[] = {
    { "Clock", IPODJS_EXTRAS_CLOCK },
    { "Games", IPODJS_EXTRAS_GAMES },
    { "Achievements", IPODJS_EXTRAS_ACHIEVEMENTS },
    { "Applications", IPODJS_EXTRAS_APPLICATIONS },
    { "Magazines", IPODJS_EXTRAS_MAGAZINES },
    { "Comics", IPODJS_EXTRAS_COMICS },
    { "Files", GO_TO_FILEBROWSER },
    { "Playlists", GO_TO_PLAYLISTS_SCREEN },
    { "Plugins", GO_TO_BROWSEPLUGINS },
    { "Shortcuts", GO_TO_SHORTCUTMENU },
    { "System", GO_TO_SYSTEM_SCREEN },
};

enum root_menu_video_clock_item {
    IPODJS_CLOCK_WORLD = 0,
    IPODJS_CLOCK_STOPWATCH,
    IPODJS_CLOCK_TIMER,
    IPODJS_CLOCK_ALARMS,
    IPODJS_CLOCK_COUNT,
};

static const char * const root_menu_video_clock_rows[IPODJS_CLOCK_COUNT] = {
    "World Clock", "Stopwatch", "Timer", "Alarms"
};

static const char *root_menu_video_sleep_timer_text(char *buf, size_t size)
{
    int seconds = get_sleep_timer();

    if (seconds <= 0)
        snprintf(buf, size, "Sleep Timer Off");
    else
        snprintf(buf, size, "%d min remaining", (seconds + 59) / 60);

    return buf;
}

static void root_menu_video_draw_clock_pane(int selected, int x, int y,
                                            int w, int h)
{
    struct tm *tm = get_time();
    char line[48];
    int center_y = y + h / 2 - 33;
    int hour;

    root_menu_video_preview_gradient(x, y, w, h);
    root_menu_video_draw_clock_date(x, y, w, h, selected == IPODJS_CLOCK_WORLD);

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(IPODJS_PREVIEW_TEXT);
    lcd_set_background(IPODJS_PREVIEW_BOTTOM);

    if (!tm)
        return;

    switch (selected)
    {
        case IPODJS_CLOCK_WORLD:
            hour = tm->tm_hour % 12;
            if (hour == 0)
                hour = 12;
            snprintf(line, sizeof(line), "%d:%02d %s", hour, tm->tm_min,
                     tm->tm_hour >= 12 ? "PM" : "AM");
            root_menu_video_puts_fit(x + 16, center_y, w - 32,
                                     "Local Time", true);
            root_menu_video_puts_fit(x + 16, center_y + 20, w - 32,
                                     line, true);
            root_menu_video_puts_fit(x + 16, center_y + 42, w - 32,
                                     "Select for Clock", true);
            break;

        case IPODJS_CLOCK_STOPWATCH:
            root_menu_video_puts_fit(x + 16, center_y, w - 32,
                                     "Stopwatch", true);
            root_menu_video_puts_fit(x + 16, center_y + 22, w - 32,
                                     "Laps and elapsed time", true);
            root_menu_video_puts_fit(x + 16, center_y + 44, w - 32,
                                     "Select to open", true);
            break;

        case IPODJS_CLOCK_TIMER:
            root_menu_video_puts_fit(x + 16, center_y, w - 32,
                                     "Timer", true);
            root_menu_video_puts_fit(x + 16, center_y + 22, w - 32,
                                     root_menu_video_sleep_timer_text(
                                         line, sizeof(line)), true);
            root_menu_video_puts_fit(x + 16, center_y + 44, w - 32,
                                     "Select toggles", true);
            break;

        case IPODJS_CLOCK_ALARMS:
        default:
            root_menu_video_puts_fit(x + 16, center_y, w - 32,
                                     "Alarms", true);
#ifdef HAVE_RTC_ALARM
            root_menu_video_puts_fit(x + 16, center_y + 22, w - 32,
                                     "Wake alarm settings", true);
#else
            root_menu_video_puts_fit(x + 16, center_y + 22, w - 32,
                                     "Alarm Clock app", true);
#endif
            root_menu_video_puts_fit(x + 16, center_y + 44, w - 32,
                                     "Select to open", true);
            break;
    }
}

static void root_menu_video_draw_analog_clock(int selected)
{
    int row_h = root_menu_video_row_height();
    int x = IPODJS_PREVIEW_X;
    int y = 0;
    int w = LCD_WIDTH - x;
    int h = LCD_HEIGHT - y;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_status();
    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_screen_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, IPODJS_LIST_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    for (int i = 0; i < IPODJS_CLOCK_COUNT; i++)
    {
        int item_y = IPODJS_HEADER_HEIGHT + i * row_h;
        bool active = i == selected;

        if (active)
        {
            root_menu_video_selection_gradient(0, item_y,
                                               IPODJS_LIST_WIDTH, row_h);
            lcd_set_foreground(IPODJS_PREVIEW_TEXT);
            lcd_set_background(root_menu_video_accent());
        }
        else
        {
            lcd_set_foreground(root_menu_video_row_bg());
            lcd_fillrect(0, item_y, IPODJS_LIST_WIDTH, row_h);
            lcd_set_foreground(root_menu_video_text());
            lcd_set_background(root_menu_video_row_bg());
        }

        root_menu_video_puts_fit(6, item_y + 4, IPODJS_LIST_WIDTH - 24,
                                 root_menu_video_clock_rows[i], false);
        if (active)
            root_menu_video_draw_arrow(IPODJS_LIST_WIDTH - 13,
                                       item_y + (row_h - 6) / 2);
    }

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(54, 60, 70) : IPODJS_SPLIT);
    lcd_vline(IPODJS_SPLIT_X, 0, LCD_HEIGHT - 1);

    root_menu_video_draw_clock_pane(selected, x, y, w, h);

    ipodjs_video_draw_hold_overlay();
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
}

static void root_menu_video_set_sleep_duration(void)
{
    int duration = global_settings.sleeptimer_duration;

    root_menu_video_finish_native_screen(0);
    if (set_int((const unsigned char *)"Timer Duration", "min", UNIT_MIN,
                &duration, NULL, 5, 5, 300, NULL))
    {
        global_settings.sleeptimer_duration = duration;
        if (get_sleep_timer())
            set_sleeptimer_duration(duration);
    }
    root_menu_video_enter_native_screen();
}

static int root_menu_video_clock_open(int selected)
{
    int ret = GO_TO_PREVIOUS;

    root_menu_video_finish_native_screen(0);
    switch (selected)
    {
        case IPODJS_CLOCK_WORLD:
            ret = launch_clock_plugin(NULL);
            break;
        case IPODJS_CLOCK_STOPWATCH:
            ret = launch_stopwatch_plugin(NULL);
            break;
        case IPODJS_CLOCK_TIMER:
        {
            char timer_status[48];

            toggle_sleeptimer();
            splash(HZ, root_menu_video_sleep_timer_text(
                       timer_status, sizeof(timer_status)));
            ret = GO_TO_PREVIOUS;
            break;
        }
        case IPODJS_CLOCK_ALARMS:
        default:
            ret = launch_alarmclock_plugin(NULL);
            break;
    }
    root_menu_video_enter_native_screen();
    return ret;
}

static int root_menu_video_clock_screen(void)
{
    int selected = 0;
    bool redraw = true;
    bool held = button_hold();
    long next_refresh = 0;

    button_clear_queue();
    while (true)
    {
        int action;

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_analog_clock(selected);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, HZ / 5);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
                if (TIME_AFTER(current_tick, next_refresh))
                {
                    next_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = MAX(0, selected - 1);
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = MIN(IPODJS_CLOCK_COUNT - 1, selected + 1);
                redraw = true;
                break;
            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
            {
                int ret = root_menu_video_clock_open(selected);
                if (ret == GO_TO_WPS || ret == MENU_ATTACHED_USB)
                    return ret;
                redraw = true;
                break;
            }
            case ACTION_STD_CONTEXT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (selected == IPODJS_CLOCK_TIMER)
                    root_menu_video_set_sleep_duration();
                redraw = true;
                break;
            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return GO_TO_PREVIOUS;
            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
            {
                bool started_playback = false;

                if (root_menu_video_handle_play_pause(true,
                                                     &started_playback))
                    return GO_TO_WPS;
                redraw = true;
                break;
            }
        }
    }
}

static void root_menu_video_draw_extras_menu(int selected)
{
    int count = ARRAYLEN(root_menu_video_extras_items);
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int top = 0;
    int i;
    int x = IPODJS_PREVIEW_X;
    int y = 0;
    int w = LCD_WIDTH - x;
    int h = LCD_HEIGHT - y;

    if (selected >= visible)
        top = selected - visible + 1;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_status();
    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_screen_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, IPODJS_LIST_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    for (i = 0; i < visible && top + i < count; i++)
    {
        int index = top + i;
        int item_y = IPODJS_HEADER_HEIGHT + i * row_h;
        bool active = index == selected;

        if (active)
        {
            unsigned accent = root_menu_video_accent();
            root_menu_video_selection_gradient(0, item_y, IPODJS_LIST_WIDTH,
                                               row_h);
            lcd_set_foreground(IPODJS_PREVIEW_TEXT);
            lcd_set_background(accent);
        }
        else
        {
            lcd_set_foreground(root_menu_video_row_bg());
            lcd_fillrect(0, item_y, IPODJS_LIST_WIDTH, row_h);
            lcd_set_foreground(root_menu_video_text());
            lcd_set_background(root_menu_video_row_bg());
        }

        root_menu_video_puts_fit(6, item_y + 4, IPODJS_LIST_WIDTH - 24,
                                 root_menu_video_extras_items[index].label,
                                 false);
        if (active)
            root_menu_video_draw_arrow(IPODJS_LIST_WIDTH - 13,
                                       item_y + (row_h - 6) / 2);
    }

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(54, 60, 70) : IPODJS_SPLIT);
    lcd_vline(IPODJS_SPLIT_X, 0, LCD_HEIGHT - 1);
    root_menu_video_draw_preview_for_title(
        root_menu_video_extras_items[selected].label, x, y, w, h);
    ipodjs_video_draw_hold_overlay();
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
    ipodjs_trace_screen("Extras", "full", selected, top, count,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static int root_menu_video_extras_menu(void)
{
    int selected = 0;
    int previous_selected = 0;
    bool redraw = true;
    bool redraw_rows = false;
    bool held = button_hold();
    long next_slideshow = 0;
    long next_hold_refresh = 0;
    long preview_io_settle_tick = current_tick + HZ / 2;

    root_menu_video_enter_native_screen();
    root_menu_video_prepare_sitekick_preview();
    button_clear_queue();

    while (true)
    {
        int action;
        int count = ARRAYLEN(root_menu_video_extras_items);

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_extras_menu(selected);
            redraw = false;
            redraw_rows = false;
        }
        else if (redraw_rows)
        {
            if (!root_menu_video_draw_native_pane_delta(previous_selected,
                    selected, count,
                    root_menu_video_extras_items[previous_selected].label,
                    root_menu_video_extras_items[selected].label, false))
                root_menu_video_draw_extras_menu(selected);
            redraw_rows = false;
        }

        action = get_action(CONTEXT_TREE, HZ/20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
            {
                enum root_menu_video_preview_source preview_source =
                    root_menu_video_preview_source_for_title(
                        root_menu_video_extras_items[selected].label);

                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                    break;
                }

                /* Preview frames are opened here, at the idle service point,
                 * never from a draw function. */
                if (!held && preview_source > IPODJS_PREVIEW_MUSIC &&
                    TIME_AFTER(current_tick, preview_io_settle_tick) &&
                    button_queue_count() == 0 &&
                    root_menu_video_preview_service(preview_source))
                    redraw = true;

                if (!held && !redraw &&
                    root_menu_video_should_animate(preview_source,
                                                   &next_slideshow))
                    redraw = true;
                break;
            }
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MAX(0, selected - 1);
                redraw_rows = previous_selected != selected;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MIN(count - 1, selected + 1);
                redraw_rows = previous_selected != selected;
                break;
            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (root_menu_video_extras_items[selected].screen ==
                    IPODJS_EXTRAS_CLOCK)
                {
                    ipodjs_ui_transition_begin(1);
                    int ret = root_menu_video_clock_screen();
                    if (ret == GO_TO_WPS)
                    {
                        return root_menu_video_finish_native_screen(ret);
                    }
                    ipodjs_ui_transition_begin(-1);
                    redraw = true;
                    break;
                }
                if (root_menu_video_extras_items[selected].screen ==
                    IPODJS_EXTRAS_APPLICATIONS)
                {
                    int ret;
                    ipodjs_ui_transition_begin(1);
                    ret = root_menu_video_applications_menu();
                    if (ret == MENU_ATTACHED_USB)
                        return root_menu_video_finish_native_screen(ret);
                    ipodjs_ui_transition_begin(-1);
                    redraw = true;
                    break;
                }
                if (root_menu_video_extras_items[selected].screen ==
                    IPODJS_EXTRAS_GAMES)
                {
                    ipodjs_ui_transition_begin(1);
                    int ret = root_menu_video_games_menu();
                    if (ret == GO_TO_WPS)
                        return root_menu_video_finish_native_screen(ret);
                    ipodjs_ui_transition_begin(-1);
                    redraw = true;
                    break;
                }
                if (root_menu_video_extras_items[selected].screen ==
                    IPODJS_EXTRAS_ACHIEVEMENTS)
                {
                    int ret;

                    ipodjs_ui_transition_begin(1);
                    root_menu_video_finish_native_screen(0);
                    ret = launch_achievements_plugin(NULL);
                    root_menu_video_enter_native_screen();
                    if (ret == MENU_ATTACHED_USB)
                        return root_menu_video_finish_native_screen(ret);
                    ipodjs_ui_transition_begin(-1);
                    redraw = true;
                    break;
                }
                if (root_menu_video_extras_items[selected].screen ==
                    IPODJS_EXTRAS_MAGAZINES)
                {
                    int ret;

                    ipodjs_ui_transition_begin(1);
                    root_menu_video_finish_native_screen(0);
                    ret = launch_magazines_plugin(NULL);
                    root_menu_video_enter_native_screen();
                    if (ret == MENU_ATTACHED_USB)
                        return root_menu_video_finish_native_screen(ret);
                    ipodjs_ui_transition_begin(-1);
                    redraw = true;
                    break;
                }
                if (root_menu_video_extras_items[selected].screen ==
                    IPODJS_EXTRAS_COMICS)
                {
                    int ret;

                    ipodjs_ui_transition_begin(1);
                    root_menu_video_finish_native_screen(0);
                    ret = launch_comics_plugin(NULL);
                    root_menu_video_enter_native_screen();
                    if (ret == MENU_ATTACHED_USB)
                        return root_menu_video_finish_native_screen(ret);
                    ipodjs_ui_transition_begin(-1);
                    redraw = true;
                    break;
                }
                return root_menu_video_finish_native_screen(
                    root_menu_video_extras_items[selected].screen);
            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_ROOT);
        }
    }
}

struct root_menu_video_application_item {
    const char *label;
    int (*function)(void *param);
};

static const struct root_menu_video_application_item
root_menu_video_application_items[] = {
    { "Clock", launch_clock_plugin },
    { "Desktop Mode", launch_desktop_mode },
    { "Achievements", launch_achievements_plugin },
    { "DIRECTV", launch_livetv_plugin },
    { "Sitekick", launch_sitekick_plugin },
    { "Tamagotchi", launch_tamagotchi_plugin },
    { "Maps", launch_maps_plugin },
    { "Weather", launch_weather_plugin },
    { "Calm", launch_calm_plugin },
    { "Internet", launch_offlineweb_plugin },
    { "Pocket Sky", launch_pocketsky_plugin },
#ifdef HAVE_TAGCACHE
    { "QR Codes", launch_qrcode_tool },
#endif
    { "Pokedex", launch_pokedex_plugin },
};

static void root_menu_video_draw_applications_menu(int selected)
{
    int count = ARRAYLEN(root_menu_video_application_items);
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int top = selected >= visible ? selected - visible + 1 : 0;
    int x = IPODJS_PREVIEW_X;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();
    root_menu_video_draw_status();
    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_screen_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, IPODJS_LIST_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    for (int i = 0; i < visible && top + i < count; i++)
    {
        int index = top + i;
        int item_y = IPODJS_HEADER_HEIGHT + i * row_h;
        bool active = index == selected;

        if (active)
        {
            root_menu_video_selection_gradient(0, item_y, IPODJS_LIST_WIDTH,
                                               row_h);
            lcd_set_foreground(IPODJS_PREVIEW_TEXT);
            lcd_set_background(root_menu_video_accent());
        }
        else
        {
            lcd_set_foreground(root_menu_video_row_bg());
            lcd_fillrect(0, item_y, IPODJS_LIST_WIDTH, row_h);
            lcd_set_foreground(root_menu_video_text());
            lcd_set_background(root_menu_video_row_bg());
        }
        root_menu_video_puts_fit(
            6, item_y + 4, IPODJS_LIST_WIDTH - 24,
            root_menu_video_application_items[index].label, false);
        if (active)
            root_menu_video_draw_arrow(IPODJS_LIST_WIDTH - 13,
                                       item_y + (row_h - 6) / 2);
    }

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(54, 60, 70) : IPODJS_SPLIT);
    lcd_vline(IPODJS_SPLIT_X, 0, LCD_HEIGHT - 1);
    root_menu_video_draw_preview_for_title(
        root_menu_video_application_items[selected].label,
        x, 0, LCD_WIDTH - x, LCD_HEIGHT);
    ipodjs_video_draw_hold_overlay();
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
    ipodjs_trace_screen("Applications", "full", selected, top, count,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static void root_menu_video_draw_applications_preview_only(int selected)
{
    int count = ARRAYLEN(root_menu_video_application_items);
    int x = IPODJS_PREVIEW_X;

    selected = MAX(0, MIN(selected, count - 1));
    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    root_menu_video_draw_preview_for_title(
        root_menu_video_application_items[selected].label,
        x, 0, LCD_WIDTH - x, LCD_HEIGHT);
    lcd_update_rect(x, 0, LCD_WIDTH - x, LCD_HEIGHT);
}

static int root_menu_video_applications_menu(void)
{
    int selected = 0;
    int previous_selected = 0;
    bool redraw = true;
    bool redraw_rows = false;
    bool held = button_hold();
    long next_slideshow = 0;
    long next_hold_refresh = 0;
    long preview_io_settle_tick =
        current_tick + IPODJS_ROOT_PREVIEW_SETTLE_DELAY;

    root_menu_video_enter_native_screen();
    root_menu_video_prepare_sitekick_preview();
    button_clear_queue();

    while (true)
    {
        int action;
        int count = ARRAYLEN(root_menu_video_application_items);

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_applications_menu(selected);
            redraw = false;
            redraw_rows = false;
        }
        else if (redraw_rows)
        {
            if (!root_menu_video_draw_native_pane_delta(
                    previous_selected, selected, count,
                    root_menu_video_application_items[previous_selected].label,
                    root_menu_video_application_items[selected].label, false))
                root_menu_video_draw_applications_menu(selected);
            redraw_rows = false;
        }

        action = get_action(CONTEXT_TREE, HZ / 20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
            {
                const char *title =
                    root_menu_video_application_items[selected].label;
                enum root_menu_video_preview_source source =
                    root_menu_video_preview_source_for_title(title);
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                else if (!held &&
                         TIME_AFTER(current_tick, preview_io_settle_tick) &&
                         button_queue_count() == 0 &&
                         ((source > IPODJS_PREVIEW_MUSIC &&
                           root_menu_video_preview_service(source)) ||
                          (source == IPODJS_PREVIEW_NONE &&
                           root_menu_video_menu_preview_service(title))))
                {
                    redraw = true;
                }
                else if (!held &&
                         root_menu_video_sitekick_animation_due(
                             title,
                             &next_slideshow))
                    root_menu_video_draw_applications_preview_only(selected);
                else if (!held &&
                         root_menu_video_custom_preview_animation_due(
                             title, &next_slideshow))
                    root_menu_video_draw_applications_preview_only(selected);
                else if (!held &&
                         root_menu_video_should_animate(source,
                                                       &next_slideshow))
                    redraw = true;
                break;
            }
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MAX(0, selected - 1);
                if (selected != previous_selected &&
                    !strcmp(root_menu_video_application_items[selected].label,
                            "Sitekick"))
                    root_menu_video_prepare_sitekick_preview();
                if (selected != previous_selected)
                    preview_io_settle_tick =
                        current_tick + IPODJS_ROOT_PREVIEW_SETTLE_DELAY;
                redraw_rows = previous_selected != selected;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MIN(count - 1, selected + 1);
                if (selected != previous_selected &&
                    !strcmp(root_menu_video_application_items[selected].label,
                            "Sitekick"))
                    root_menu_video_prepare_sitekick_preview();
                if (selected != previous_selected)
                    preview_io_settle_tick =
                        current_tick + IPODJS_ROOT_PREVIEW_SETTLE_DELAY;
                redraw_rows = previous_selected != selected;
                break;
            case ACTION_STD_OK:
            {
                int ret;
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                ipodjs_ui_transition_begin(1);
                root_menu_video_finish_native_screen(0);
                ret = root_menu_video_application_items[selected].function(
                    NULL);
                root_menu_video_enter_native_screen();
                root_menu_video_prepare_sitekick_preview();
                if (ret == MENU_ATTACHED_USB || ret == GO_TO_WPS)
                    return root_menu_video_finish_native_screen(ret);
                ipodjs_ui_transition_begin(-1);
                redraw = true;
                break;
            }
            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
            {
                bool started_playback = false;
                if (root_menu_video_handle_play_pause(true,
                                                     &started_playback))
                    return root_menu_video_finish_native_screen(GO_TO_WPS);
                redraw = true;
                break;
            }
            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_PREVIOUS);
        }
    }
}

struct root_menu_video_games_item {
    const char *label;
    int (*function)(void *param);
};

static const struct root_menu_video_games_item root_menu_video_games_items[] = {
    { "Maker Lite", launch_maker_lite },
    { "RuneScape Classic", launch_runescape_classic },
    { "Stick RPG", launch_stickrpg },
    { "Club Penguin", launch_club_penguin },
    { "Sega Master System / Game Gear", launch_smsgg },
    { "Game Cover Flow", launch_gameboy_browser },
    { "Browse ROM Files", browse_gameboy_roms },
    { "PokeMini", launch_pokemini },
    { "Browse PokeMini ROMs", browse_pokemini_roms },
    { "Toggle NES Sound", infones_toggle_sound },
    { "Toggle NES Autosave", infones_toggle_autosave },
    { "Toggle NES Audio Quality", infones_toggle_audio_quality },
    { "Clear NES Saves", infones_clear_saves },
};

#define IPODJS_STEAM_MAX_GAMES 128
#define IPODJS_STEAM_TITLE_MAX 64
#define IPODJS_STEAM_PLATFORM_MAX 32
#define IPODJS_STEAM_GENRE_MAX 48
#define IPODJS_STEAM_STUDIO_MAX 64
#define IPODJS_STEAM_DESCRIPTION_MAX 160
#define IPODJS_STEAM_PLUGIN_MAX 112
#define IPODJS_STEAM_COVER_W 144
#define IPODJS_STEAM_COVER_H 108
#define IPODJS_STEAM_COVER_CACHE 3
#define IPODJS_STEAM_MAX_CONSOLES 24
#define IPODJS_STEAM_SCALE_EXTRA \
    (IPODJS_STEAM_COVER_W * (int)sizeof(uint32_t) * 4)
#define IPODJS_STEAM_NATIVE_COVERS \
    ROCKBOX_DIR "/games/library/covers/native"
#define IPODJS_STEAM_ACHIEVEMENTS_ROOT ROCKBOX_DIR "/achievements"
#define IPODJS_STEAM_HEADER       LCD_RGBPACK(23, 26, 33)   /* #171a21 */
#define IPODJS_STEAM_BODY         LCD_RGBPACK(27, 40, 56)   /* #1b2838 */
#define IPODJS_STEAM_PANEL        LCD_RGBPACK(42, 71, 94)   /* #2a475e */
#define IPODJS_STEAM_BLUE         LCD_RGBPACK(102, 192, 244)/* #66c0f4 */
#define IPODJS_STEAM_TEXT         LCD_RGBPACK(199, 213, 224)/* #c7d5e0 */
#define IPODJS_STEAM_MUTED        LCD_RGBPACK(143, 152, 160)/* #8f98a0 */
#define IPODJS_STEAM_GREEN_TOP    LCD_RGBPACK(117, 176, 34) /* #75b022 */
#define IPODJS_STEAM_GREEN_BOTTOM LCD_RGBPACK(88, 138, 27)  /* #588a1b */

struct ipodjs_steam_game_entry {
    char title[IPODJS_STEAM_TITLE_MAX];
    char platform[IPODJS_STEAM_PLATFORM_MAX];
    char genre[IPODJS_STEAM_GENRE_MAX];
    char developer[IPODJS_STEAM_STUDIO_MAX];
    char publisher[IPODJS_STEAM_STUDIO_MAX];
    char description[IPODJS_STEAM_DESCRIPTION_MAX];
    char plugin[IPODJS_STEAM_PLUGIN_MAX];
    char param[MAX_PATH];
    char cover[MAX_PATH];
    int year;
};

struct ipodjs_steam_cover_slot {
    bool valid;
    unsigned long stamp;
    char path[MAX_PATH];
    struct bitmap bm;
    unsigned char data[
        BM_SCALED_SIZE(IPODJS_STEAM_COVER_W, IPODJS_STEAM_COVER_H,
                       FORMAT_NATIVE, false) +
        IPODJS_STEAM_SCALE_EXTRA];
};

static struct ipodjs_steam_game_entry
    ipodjs_steam_games[IPODJS_STEAM_MAX_GAMES];
static int ipodjs_steam_game_count;
static int ipodjs_steam_visible_indices[IPODJS_STEAM_MAX_GAMES];
static int ipodjs_steam_visible_count;
static char ipodjs_steam_consoles[IPODJS_STEAM_MAX_CONSOLES]
                                  [IPODJS_STEAM_PLATFORM_MAX];
static int ipodjs_steam_console_count;
static int ipodjs_steam_console;
static struct ipodjs_steam_cover_slot
    ipodjs_steam_cover_cache[IPODJS_STEAM_COVER_CACHE];
static unsigned long ipodjs_steam_cover_stamp;

static struct ipodjs_steam_game_entry *ipodjs_steam_visible_game(int index)
{
    if (index < 0 || index >= ipodjs_steam_visible_count)
        return NULL;
    return &ipodjs_steam_games[ipodjs_steam_visible_indices[index]];
}

static int ipodjs_steam_compare_console_names(const void *left,
                                              const void *right)
{
    const char *a = left;
    const char *b = right;

    /* Keep newly installed Uxn titles visible without scrolling through
     * every emulator platform in the console chooser. */
    if (!strcasecmp(a, "Uxn"))
        return strcasecmp(b, "Uxn") ? -1 : 0;
    if (!strcasecmp(b, "Uxn"))
        return 1;
    return strcasecmp(a, b);
}

static void ipodjs_steam_build_consoles(void)
{
    ipodjs_steam_console_count = 1;
    strmemccpy(ipodjs_steam_consoles[0], "All Games",
               sizeof(ipodjs_steam_consoles[0]));

    for (int game = 0; game < ipodjs_steam_game_count &&
         ipodjs_steam_console_count < IPODJS_STEAM_MAX_CONSOLES; game++)
    {
        const char *platform = ipodjs_steam_games[game].platform;
        bool duplicate = false;

        for (int console = 1; console < ipodjs_steam_console_count; console++)
        {
            if (!strcasecmp(ipodjs_steam_consoles[console], platform))
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate && platform[0])
            strmemccpy(ipodjs_steam_consoles[ipodjs_steam_console_count++],
                       platform, IPODJS_STEAM_PLATFORM_MAX);
    }

    if (ipodjs_steam_console_count > 2)
        qsort(&ipodjs_steam_consoles[1], ipodjs_steam_console_count - 1,
              sizeof(ipodjs_steam_consoles[0]),
              ipodjs_steam_compare_console_names);
}

static int ipodjs_steam_find_console(const char *name)
{
    for (int console = 0; console < ipodjs_steam_console_count; console++)
    {
        if (!strcasecmp(ipodjs_steam_consoles[console], name))
            return console;
    }
    return 0;
}

static void ipodjs_steam_build_visible_games(void)
{
    const char *console = ipodjs_steam_console > 0 ?
        ipodjs_steam_consoles[ipodjs_steam_console] : NULL;

    ipodjs_steam_visible_count = 0;
    for (int game = 0; game < ipodjs_steam_game_count; game++)
    {
        if (console && strcasecmp(ipodjs_steam_games[game].platform,
                                  console))
            continue;
        ipodjs_steam_visible_indices[ipodjs_steam_visible_count++] = game;
    }
}

static const char *ipodjs_steam_platform_for_path(const char *path)
{
    const char *ext = path ? strrchr(path, '.') : NULL;

    if (!ext)
        return "Game";
    if (!strcasecmp(ext, ".gb") || !strcasecmp(ext, ".gbc") ||
        !strcasecmp(ext, ".sgb"))
        return "Game Boy";
    if (!strcasecmp(ext, ".nes"))
        return "NES";
    if (!strcasecmp(ext, ".sms") || !strcasecmp(ext, ".gg"))
        return "Sega 8-bit";
    if (!strcasecmp(ext, ".sfc") || !strcasecmp(ext, ".smc"))
        return "Super Nintendo";
    if (!strcasecmp(ext, ".md") || !strcasecmp(ext, ".gen") ||
        !strcasecmp(ext, ".bin"))
        return "Genesis";
    if (!strcasecmp(ext, ".min"))
        return "Pokemon Mini";
    if (!strcasecmp(ext, ".mgw"))
        return "Game & Watch";
    if (!strcasecmp(ext, ".mlp"))
        return "Maker Lite";
    if (!strcasecmp(ext, ".rom"))
        return "Uxn";
    if (!strcasecmp(ext, ".zip") &&
        path && strstr(path, "/games/cps1/roms/"))
        return "CPS1 Arcade";
    return "Game";
}

static bool ipodjs_steam_game_duplicate(const char *plugin,
                                        const char *param)
{
    for (int i = 0; i < ipodjs_steam_game_count; i++)
    {
        if (!strcmp(ipodjs_steam_games[i].plugin, plugin ? plugin : "") &&
            !strcmp(ipodjs_steam_games[i].param, param ? param : ""))
            return true;
    }
    return false;
}

static bool ipodjs_steam_add_game(const char *title, const char *platform,
                                  const char *genre, const char *developer,
                                  const char *publisher,
                                  const char *description,
                                  const char *plugin, const char *param,
                                  const char *cover, int year)
{
    struct ipodjs_steam_game_entry *entry;

    if (ipodjs_steam_game_count >= IPODJS_STEAM_MAX_GAMES ||
        !title || !title[0] || !cover || !cover[0] ||
        !file_exists(cover) ||
        ((!plugin || !plugin[0]) && (!param || !param[0])) ||
        (plugin && plugin[0] && !file_exists(plugin)) ||
        (param && param[0] && !file_exists(param)) ||
        ipodjs_steam_game_duplicate(plugin, param))
        return false;

    entry = &ipodjs_steam_games[ipodjs_steam_game_count++];
    memset(entry, 0, sizeof(*entry));
    strmemccpy(entry->title, title, sizeof(entry->title));
    strmemccpy(entry->platform, platform ? platform : "Game",
               sizeof(entry->platform));
    strmemccpy(entry->genre, genre ? genre : "",
               sizeof(entry->genre));
    strmemccpy(entry->developer, developer ? developer : "",
               sizeof(entry->developer));
    strmemccpy(entry->publisher, publisher ? publisher : "",
               sizeof(entry->publisher));
    strmemccpy(entry->description, description ? description : "",
               sizeof(entry->description));
    strmemccpy(entry->plugin, plugin ? plugin : "",
               sizeof(entry->plugin));
    strmemccpy(entry->param, param ? param : "", sizeof(entry->param));
    strmemccpy(entry->cover, cover, sizeof(entry->cover));
    entry->year = year;
    return true;
}

static void ipodjs_steam_title_from_stem(char *title, size_t title_size,
                                         const char *stem)
{
    bool word_start = true;
    size_t used = 0;

    if (!title || title_size == 0)
        return;

    while (stem && *stem && used + 1 < title_size)
    {
        unsigned char ch = (unsigned char)*stem++;
        if (ch == '_')
            ch = ' ';
        if (word_start && isalpha(ch))
            ch = toupper(ch);
        title[used++] = ch;
        word_start = ch == ' ' || ch == '-';
    }
    title[used] = '\0';
}

static void ipodjs_steam_load_native_games(void)
{
    DIR *dir = opendir(IPODJS_STEAM_NATIVE_COVERS);
    struct dirent *entry;

    if (!dir)
        return;

    while ((entry = readdir(dir)) != NULL &&
           ipodjs_steam_game_count < IPODJS_STEAM_MAX_GAMES)
    {
        const char *ext = strrchr(entry->d_name, '.');
        char stem[IPODJS_STEAM_TITLE_MAX];
        char title[IPODJS_STEAM_TITLE_MAX];
        char plugin[IPODJS_STEAM_PLUGIN_MAX];
        char cover[MAX_PATH];
        size_t stem_len;

        if (!ext || strcasecmp(ext, ".bmp"))
            continue;
        stem_len = ext - entry->d_name;
        if (stem_len == 0 || stem_len >= sizeof(stem))
            continue;
        memcpy(stem, entry->d_name, stem_len);
        stem[stem_len] = '\0';
        snprintf(plugin, sizeof(plugin), PLUGIN_GAMES_DIR "/%s.rock",
                 stem);
        snprintf(cover, sizeof(cover), IPODJS_STEAM_NATIVE_COVERS "/%s",
                 entry->d_name);
        ipodjs_steam_title_from_stem(title, sizeof(title), stem);
        (void)ipodjs_steam_add_game(
            title, "Rockbox", "Native game", "Rockbox", "Rockbox",
            "A native Rockbox game. Artwork is an official Rockbox manual "
            "screenshot or the title's existing cover.",
            plugin, NULL, cover, 0);
    }
    closedir(dir);
}

static void ipodjs_steam_load_special_games(void)
{
    (void)ipodjs_steam_add_game(
        "Super Mario 64", "Nintendo 64", "Platformer", "Nintendo EAD",
        "Nintendo", "The installed native Super Mario 64 port.",
        PLUGIN_GAMES_DIR "/sm64.rock", NULL,
        ROCKBOX_DIR "/games/library/covers/n64/Super Mario 64 (USA).bmp",
        1996);
    (void)ipodjs_steam_add_game(
        "Stick RPG", "Flash", "Role-playing", "XGen Studios",
        "XGen Studios", "The installed Stick RPG Flash game.",
        VIEWERS_DIR "/flashplayer.rock",
        ROCKBOX_DIR "/flash/stickrpg/stickrpg.swf",
        IPODJS_ASSET_DIR "/stickrpg/covers/Stick RPG.bmp", 2003);
}

static void ipodjs_steam_load_uxn_games(void)
{
    static const char * const plugin = VIEWERS_DIR "/uxn.rock";

    /* These titles and assets are part of the RockPod package.  Register
     * them explicitly so the Uxn console does not depend on the generic
     * third-party manifest parser or disappear silently on hardware. */
    (void)ipodjs_steam_add_game(
        "Donsol", "Uxn", "Card dungeon", "Hundred Rabbits",
        "Hundred Rabbits",
        "Explore a dungeon made from a shuffled deck of cards.",
        plugin, "/Uxn/donsol.rom",
        ROCKBOX_DIR "/games/library/covers/uxn/donsol.bmp", 2024);
    (void)ipodjs_steam_add_game(
        "Niju", "Uxn", "Learning puzzle", "Hundred Rabbits",
        "Hundred Rabbits",
        "Review Japanese hiragana and katakana with two study modes.",
        plugin, "/Uxn/niju.rom",
        ROCKBOX_DIR "/games/library/covers/uxn/niju.bmp", 0);
    (void)ipodjs_steam_add_game(
        "Worm", "Uxn", "Arcade", "origedit", "origedit",
        "Guide a growing sandworm with the click-wheel directions.",
        plugin, "/Uxn/worm.rom",
        ROCKBOX_DIR "/games/library/covers/uxn/worm.bmp", 2024);
}

static void ipodjs_steam_load_ipod_games(void)
{
    char generation[40];
    char catalog[MAX_PATH];
    char line[1024];
    int fd = open(IPODJS_STEAM_ACHIEVEMENTS_ROOT "/current", O_RDONLY);

    if (fd < 0 || read_line(fd, generation, sizeof(generation)) <= 0)
    {
        if (fd >= 0)
            close(fd);
        return;
    }
    close(fd);
    video_trim_line(generation);
    if (!generation[0] || strchr(generation, '/') || strstr(generation, ".."))
        return;
    snprintf(catalog, sizeof(catalog),
             IPODJS_STEAM_ACHIEVEMENTS_ROOT "/generations/%s/catalog.tsv",
             generation);
    fd = open(catalog, O_RDONLY);
    if (fd < 0)
        return;
    (void)read_line(fd, line, sizeof(line));
    while (ipodjs_steam_game_count < IPODJS_STEAM_MAX_GAMES &&
           read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[14];

        video_trim_line(line);
        if (!video_split_tsv(line, fields, ARRAYLEN(fields)) ||
            strcasecmp(fields[2], "iPod Games"))
            continue;
        (void)ipodjs_steam_add_game(
            fields[1], "iPod Games", "iPod game", "Apple", "Apple",
            "An installed click-wheel iPod game.",
            PLUGIN_GAMES_DIR "/ipodgames.rock", fields[11], fields[4], 0);
    }
    close(fd);
}

static void ipodjs_steam_load_manifest(const char *path)
{
    char line[768];
    int fd = open(path, O_RDONLY);

    if (fd < 0)
        return;

    while (ipodjs_steam_game_count < IPODJS_STEAM_MAX_GAMES &&
           read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[11];
        const char *plugin = "";
        const char *param = "";
        const char *platform_path;

        video_trim_line(line);
        if (!line[0] || line[0] == '#' ||
            !video_split_tsv(line, fields, ARRAYLEN(fields)))
            continue;

        if (fields[10][0])
        {
            plugin = fields[1];
            param = fields[10];
        }
        else if (strstr(fields[1], ".rock"))
            plugin = fields[1];
        else
            param = fields[1];

        platform_path = param[0] ? param : plugin;
        (void)ipodjs_steam_add_game(
            fields[0], ipodjs_steam_platform_for_path(platform_path),
            fields[6], fields[8], fields[7], fields[9],
            plugin, param, fields[2], atoi(fields[5]));
    }
    close(fd);
}

static int ipodjs_steam_compare_games(const void *left, const void *right)
{
    const struct ipodjs_steam_game_entry *a = left;
    const struct ipodjs_steam_game_entry *b = right;
    return strcasecmp(a->title, b->title);
}

static void ipodjs_steam_load_library(void)
{
    static const char * const indexes[] = {
        ROCKBOX_DIR "/rocks/games/rockboy_launcher/games.tsv",
        ROCKBOX_DIR "/rocks/games/pokemini_launcher/games.tsv",
        ROCKBOX_DIR "/rocks/games/maker_lite/games.tsv",
        ROCKBOX_DIR "/rocks/games/cps1/games.tsv",
    };

    ipodjs_steam_game_count = 0;
    ipodjs_steam_load_native_games();
    ipodjs_steam_load_special_games();
    ipodjs_steam_load_uxn_games();
    ipodjs_steam_load_ipod_games();
    for (int i = 0; i < (int)ARRAYLEN(indexes); i++)
        ipodjs_steam_load_manifest(indexes[i]);
    qsort(ipodjs_steam_games, ipodjs_steam_game_count,
          sizeof(ipodjs_steam_games[0]), ipodjs_steam_compare_games);
    ipodjs_steam_build_consoles();
    ipodjs_steam_console = ipodjs_steam_find_console("Uxn");
    ipodjs_steam_build_visible_games();
}

static void ipodjs_steam_cover_cache_clear(void)
{
    for (int i = 0; i < IPODJS_STEAM_COVER_CACHE; i++)
        ipodjs_steam_cover_cache[i].valid = false;
    ipodjs_steam_cover_stamp = 0;
}

static struct bitmap *ipodjs_steam_cover_find(const char *path)
{
    for (int i = 0; i < IPODJS_STEAM_COVER_CACHE; i++)
    {
        struct ipodjs_steam_cover_slot *slot =
            &ipodjs_steam_cover_cache[i];
        if (slot->valid && !strcmp(slot->path, path))
        {
            slot->stamp = ++ipodjs_steam_cover_stamp;
            return &slot->bm;
        }
    }
    return NULL;
}

static struct bitmap *ipodjs_steam_cover_load(const char *path)
{
    struct ipodjs_steam_cover_slot *slot = NULL;
    unsigned long oldest = 0;
    int victim = 0;

    struct bitmap *cached = ipodjs_steam_cover_find(path);
    if (cached)
        return cached;

    for (int i = 0; i < IPODJS_STEAM_COVER_CACHE; i++)
    {
        if (!ipodjs_steam_cover_cache[i].valid)
        {
            victim = i;
            break;
        }
        if (i == 0 || ipodjs_steam_cover_cache[i].stamp < oldest)
        {
            oldest = ipodjs_steam_cover_cache[i].stamp;
            victim = i;
        }
    }

    slot = &ipodjs_steam_cover_cache[victim];
    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->valid = false;
    slot->bm.width = IPODJS_STEAM_COVER_W;
    slot->bm.height = IPODJS_STEAM_COVER_H;
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;
    if (read_bmp_file(path, &slot->bm, sizeof(slot->data),
                      FORMAT_NATIVE | FORMAT_DITHER | FORMAT_RESIZE |
                      FORMAT_KEEP_ASPECT, NULL) < 0)
        return NULL;

    slot->valid = true;
    slot->stamp = ++ipodjs_steam_cover_stamp;
    strmemccpy(slot->path, path, sizeof(slot->path));
    return &slot->bm;
}

static void ipodjs_steam_cache_window(int selected)
{
    if (ipodjs_steam_visible_count <= 0 || !button_queue_empty() ||
        button_hold())
        return;

    for (int offset = -1; offset <= 1; offset++)
    {
        int index = (selected + offset + ipodjs_steam_visible_count) %
                    ipodjs_steam_visible_count;
        const char *path = ipodjs_steam_visible_game(index)->cover;
        if (!ipodjs_steam_cover_find(path))
            (void)ipodjs_steam_cover_load(path);
    }
}

static void ipodjs_steam_draw_cover_placeholder(int x, int y,
                                                int width, int height)
{
    struct bitmap *logo = root_menu_video_steam_logo_cached();

    lcd_set_foreground(IPODJS_STEAM_BODY);
    lcd_fillrect(x, y, width, height);
    lcd_set_foreground(IPODJS_STEAM_PANEL);
    lcd_drawrect(x, y, width, height);
    if (logo && logo->width <= width && logo->height <= height)
        lcd_bmp(logo, x + (width - logo->width) / 2,
                y + (height - logo->height) / 2);
}

static void ipodjs_steam_draw_header(const char *section)
{
    struct bitmap *logo = root_menu_video_steam_logo_cached();

    lcd_set_foreground(IPODJS_STEAM_HEADER);
    lcd_fillrect(0, 0, LCD_WIDTH, 40);
    if (logo)
        lcd_bmp(logo, 7, 4);
    else
    {
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(IPODJS_STEAM_TEXT);
        lcd_set_background(IPODJS_STEAM_HEADER);
        root_menu_video_puts_fit(8, 10, 104, "STEAM", false);
    }
    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(IPODJS_STEAM_BLUE);
    lcd_set_background(IPODJS_STEAM_HEADER);
    root_menu_video_puts_fit(164, 13, 146, section, true);
}

static void ipodjs_steam_draw_background(void)
{
    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(IPODJS_STEAM_BODY);
    lcd_clear_display();
    root_menu_video_gradient(0, 40, LCD_WIDTH, LCD_HEIGHT - 40,
                             IPODJS_STEAM_PANEL,
                             IPODJS_STEAM_BODY);
}

static void ipodjs_steam_draw_peek(const struct ipodjs_steam_game_entry *entry,
                                   bool right)
{
    struct bitmap *bm = ipodjs_steam_cover_find(entry->cover);
    int visible = 34;
    int x;
    int y;

    if (!bm)
        return;
    x = right ? LCD_WIDTH - visible : visible - bm->width;
    y = 48 + (IPODJS_STEAM_COVER_H - bm->height) / 2;
    lcd_bmp(bm, x, y);
}

static void ipodjs_steam_draw_landing(int selected)
{
    struct ipodjs_steam_game_entry *game;
    char metadata[96];

    ipodjs_steam_draw_background();
    ipodjs_steam_draw_header(
        ipodjs_steam_consoles[ipodjs_steam_console]);
    if (ipodjs_steam_visible_count <= 0)
    {
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(IPODJS_STEAM_TEXT);
        lcd_set_background(IPODJS_STEAM_BODY);
        root_menu_video_puts_fit(24, 104, LCD_WIDTH - 48,
                                 "No games with cover art found", true);
        lcd_setfont(FONT_SYSFIXED);
        lcd_set_foreground(IPODJS_STEAM_MUTED);
        root_menu_video_puts_fit(8, 224, LCD_WIDTH - 16,
                                 "MENU  Back", true);
        lcd_update();
        ipodjs_trace_screen("Steam Games", "empty", selected, 0, 0,
                            0, 0, LCD_WIDTH, LCD_HEIGHT);
        return;
    }

    selected = MAX(0, MIN(selected, ipodjs_steam_visible_count - 1));
    game = ipodjs_steam_visible_game(selected);
    if (ipodjs_steam_visible_count > 1)
    {
        int previous = (selected + ipodjs_steam_visible_count - 1) %
                       ipodjs_steam_visible_count;
        int next = (selected + 1) % ipodjs_steam_visible_count;
        ipodjs_steam_draw_peek(ipodjs_steam_visible_game(previous), false);
        ipodjs_steam_draw_peek(ipodjs_steam_visible_game(next), true);
    }

    struct bitmap *bm = ipodjs_steam_cover_find(game->cover);
    if (bm)
    {
        int x = (LCD_WIDTH - bm->width) / 2;
        int y = 48 + (IPODJS_STEAM_COVER_H - bm->height) / 2;
        lcd_set_foreground(IPODJS_STEAM_BLUE);
        lcd_fillrect(x - 3, y - 3, bm->width + 6, bm->height + 6);
        lcd_bmp(bm, x, y);
    }
    else
    {
        int x = (LCD_WIDTH - IPODJS_STEAM_COVER_W) / 2;
        int y = 48;
        lcd_set_foreground(IPODJS_STEAM_BLUE);
        lcd_fillrect(x - 3, y - 3, IPODJS_STEAM_COVER_W + 6,
                     IPODJS_STEAM_COVER_H + 6);
        ipodjs_steam_draw_cover_placeholder(
            x, y, IPODJS_STEAM_COVER_W, IPODJS_STEAM_COVER_H);
    }

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(IPODJS_STEAM_TEXT);
    lcd_set_background(IPODJS_STEAM_BODY);
    root_menu_video_puts_fit(42, 163, LCD_WIDTH - 84,
                             game->title, true);
    if (game->year > 0)
        snprintf(metadata, sizeof(metadata), "%s  |  %d%s%s",
                 game->platform, game->year,
                 game->genre[0] ? "  |  " : "", game->genre);
    else
        snprintf(metadata, sizeof(metadata), "%s%s%s",
                 game->platform, game->genre[0] ? "  |  " : "",
                 game->genre);
    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(IPODJS_STEAM_BLUE);
    root_menu_video_puts_fit(12, 187, LCD_WIDTH - 24, metadata, true);
    lcd_set_foreground(IPODJS_STEAM_MUTED);
    root_menu_video_puts_fit(8, 224, LCD_WIDTH - 16,
                             "SELECT Details   HOLD SELECT Consoles", true);
    ipodjs_video_draw_hold_overlay();
    lcd_update();
    ipodjs_trace_screen("Steam Games", "library", selected, 0,
                        ipodjs_steam_visible_count,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static int ipodjs_steam_launch_game(
    const struct ipodjs_steam_game_entry *entry)
{
    int result;

    if (entry->plugin[0])
        result = load_plugin_path_screen(
            entry->plugin, entry->param[0] ? entry->param : NULL);
    else
    {
        char viewer[MAX_PATH];

        if (filetype_get_viewer(viewer, sizeof(viewer), entry->param))
            result = load_plugin_path_screen(viewer, entry->param);
        else
        {
            splash(HZ * 2, "No emulator for this game");
            return GO_TO_ROOT;
        }
    }

    return result;
}

static void ipodjs_steam_draw_console_picker(int selected)
{
    const int row_height = 28;
    const int visible_rows = 6;
    int top = selected >= visible_rows ? selected - visible_rows + 1 : 0;

    ipodjs_steam_draw_background();
    ipodjs_steam_draw_header("CONSOLES");
    lcd_setfont(root_menu_video_font());

    for (int row = 0; row < visible_rows &&
         top + row < ipodjs_steam_console_count; row++)
    {
        int index = top + row;
        int y = 43 + row * row_height;
        bool active = index == selected;

        lcd_set_foreground(active ? IPODJS_STEAM_PANEL :
                                    IPODJS_STEAM_BODY);
        lcd_fillrect(5, y, LCD_WIDTH - 10, row_height - 2);
        lcd_set_foreground(active ? IPODJS_STEAM_BLUE :
                                    IPODJS_STEAM_TEXT);
        lcd_set_background(active ? IPODJS_STEAM_PANEL :
                                    IPODJS_STEAM_BODY);
        root_menu_video_puts_fit(14, y + 5, LCD_WIDTH - 42,
                                 ipodjs_steam_consoles[index], false);
        if (active)
            root_menu_video_draw_arrow(LCD_WIDTH - 20, y + 10);
    }

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(IPODJS_STEAM_MUTED);
    lcd_set_background(IPODJS_STEAM_BODY);
    root_menu_video_puts_fit(8, 224, LCD_WIDTH - 16,
                             "MENU  Back       SELECT  Filter", true);
    ipodjs_video_draw_hold_overlay();
    lcd_update();
    ipodjs_trace_screen("Steam Consoles", "list", selected, top,
                        ipodjs_steam_console_count,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static bool ipodjs_steam_console_picker(void)
{
    int selected = ipodjs_steam_console;
    bool redraw = true;

    root_menu_wait_for_button_release();
    button_clear_queue();
    while (true)
    {
        int action;

        if (redraw)
        {
            ipodjs_steam_draw_console_picker(selected);
            redraw = false;
        }
        action = get_action(CONTEXT_LIST, HZ / 10);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                selected = MAX(0, selected - 1);
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                selected = MIN(ipodjs_steam_console_count - 1,
                               selected + 1);
                redraw = true;
                break;
            case ACTION_STD_OK:
                ipodjs_steam_console = selected;
                ipodjs_steam_build_visible_games();
                button_clear_queue();
                return true;
            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                button_clear_queue();
                return false;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                    return false;
                break;
        }
    }
}

static bool ipodjs_steam_detail_screen(int selected, int *result)
{
    struct ipodjs_steam_game_entry *entry =
        ipodjs_steam_visible_game(selected);
    bool redraw = true;

    while (true)
    {
        int action;
        if (redraw)
        {
            char metadata[96];
            char studio[96];
            const char *description = entry->description[0] ?
                                      entry->description :
                                      "Installed game ready to play.";
            struct bitmap *bm = ipodjs_steam_cover_find(entry->cover);
            int text_x = 158;
            int text_w = LCD_WIDTH - text_x - 8;

            ipodjs_steam_draw_background();
            ipodjs_steam_draw_header("GAME DETAILS");
            if (bm)
                lcd_bmp(bm, 8 + (IPODJS_STEAM_COVER_W - bm->width) / 2,
                        50 + (IPODJS_STEAM_COVER_H - bm->height) / 2);
            else
                ipodjs_steam_draw_cover_placeholder(
                    8, 50, IPODJS_STEAM_COVER_W, IPODJS_STEAM_COVER_H);

            lcd_setfont(root_menu_video_font());
            lcd_set_foreground(IPODJS_STEAM_TEXT);
            lcd_set_background(IPODJS_STEAM_BODY);
            root_menu_video_puts_fit(text_x, 49, text_w,
                                     entry->title, false);
            if (entry->year > 0)
                snprintf(metadata, sizeof(metadata), "%s  |  %d",
                         entry->platform, entry->year);
            else
                strmemccpy(metadata, entry->platform, sizeof(metadata));
            lcd_setfont(FONT_SYSFIXED);
            lcd_set_foreground(IPODJS_STEAM_BLUE);
            root_menu_video_puts_fit(text_x, 73, text_w,
                                     metadata, false);
            root_menu_video_puts_fit(text_x, 89, text_w,
                                     entry->genre, false);
            snprintf(studio, sizeof(studio), "%s%s%s",
                     entry->developer,
                     entry->developer[0] && entry->publisher[0] ? " / " : "",
                     entry->publisher);
            root_menu_video_puts_fit(text_x, 105, text_w, studio, false);
            lcd_set_foreground(IPODJS_STEAM_TEXT);
            video_draw_netflix_wrapped(&screens[SCREEN_MAIN], description,
                                       text_x, 122, text_w, 3);
            root_menu_video_gradient(text_x, 169, text_w, 28,
                                     IPODJS_STEAM_GREEN_TOP,
                                     IPODJS_STEAM_GREEN_BOTTOM);
            lcd_setfont(root_menu_video_font());
            lcd_set_foreground(LCD_WHITE);
            lcd_set_background(IPODJS_STEAM_GREEN_BOTTOM);
            root_menu_video_puts_fit(text_x + 5, 174, text_w - 10,
                                     "PLAY", true);
            lcd_setfont(FONT_SYSFIXED);
            lcd_set_foreground(IPODJS_STEAM_MUTED);
            lcd_set_background(IPODJS_STEAM_BODY);
            root_menu_video_puts_fit(8, 224, LCD_WIDTH - 16,
                                     "MENU  Back       SELECT  Play", true);
            ipodjs_video_draw_hold_overlay();
            lcd_update();
            ipodjs_trace_screen("Steam Game Details", "full", selected,
                                0, ipodjs_steam_game_count,
                                0, 0, LCD_WIDTH, LCD_HEIGHT);
            redraw = false;
        }

        action = get_action(CONTEXT_STD, HZ / 10);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_STD_OK:
                root_menu_video_finish_native_screen(0);
                *result = ipodjs_steam_launch_game(entry);
                return true;
            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                return false;
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    *result = GO_TO_ROOT;
                    return true;
                }
                break;
        }
    }
}

static int root_menu_video_steam_games_menu(void)
{
    int selected = 0;
    int result = GO_TO_ROOT;
    bool redraw = true;
    bool selection_pending = true;
    long settle_tick = current_tick;

    ipodjs_steam_cover_cache_clear();
    ipodjs_steam_load_library();
    root_menu_video_prepare_steam_logo();
    root_menu_video_enter_native_screen();
    button_clear_queue();

    /* Console sections are a primary part of the Steam library.  Present
     * them on entry instead of hiding them behind ACTION_STD_CONTEXT. */
    if (ipodjs_steam_console_count > 1 &&
        !ipodjs_steam_console_picker())
    {
        ipodjs_steam_cover_cache_clear();
        return root_menu_video_finish_native_screen(GO_TO_ROOT);
    }

    while (true)
    {
        int action;
        if (redraw)
        {
            ipodjs_steam_draw_landing(selected);
            redraw = false;
        }

        action = get_action(CONTEXT_LIST, HZ / 20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
                if (selection_pending &&
                    TIME_AFTER(current_tick, settle_tick) &&
                    button_queue_count() == 0 && !button_hold())
                {
                    ipodjs_steam_cache_window(selected);
                    selection_pending = false;
                    redraw = true;
                }
                break;
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (ipodjs_steam_visible_count > 0)
                {
                    selected = (selected + ipodjs_steam_visible_count - 1) %
                               ipodjs_steam_visible_count;
                    selection_pending = true;
                    settle_tick = current_tick + MAX(1, HZ / 12);
                    redraw = true;
                }
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (ipodjs_steam_visible_count > 0)
                {
                    selected = (selected + 1) % ipodjs_steam_visible_count;
                    selection_pending = true;
                    settle_tick = current_tick + MAX(1, HZ / 12);
                    redraw = true;
                }
                break;
            case ACTION_STD_OK:
                if (ipodjs_steam_visible_count > 0)
                {
                    (void)ipodjs_steam_cover_load(
                        ipodjs_steam_visible_game(selected)->cover);
                    if (ipodjs_steam_detail_screen(selected, &result))
                    {
                        ipodjs_steam_cover_cache_clear();
                        return result;
                    }
                    selection_pending = true;
                    settle_tick = current_tick;
                    redraw = true;
                }
                break;
            case ACTION_STD_CONTEXT:
                if (ipodjs_steam_console_count > 1)
                {
                    if (ipodjs_steam_console_picker())
                    {
                        selected = 0;
                        ipodjs_steam_cover_cache_clear();
                    }
                    selection_pending = true;
                    settle_tick = current_tick;
                    redraw = true;
                }
                break;
            case ACTION_TREE_WPS:
            {
                bool started_playback = false;
                if (root_menu_video_handle_play_pause(true,
                                                     &started_playback))
                {
                    ipodjs_steam_cover_cache_clear();
                    return root_menu_video_finish_native_screen(GO_TO_WPS);
                }
                redraw = true;
                break;
            }
            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                ipodjs_steam_cover_cache_clear();
                return root_menu_video_finish_native_screen(GO_TO_ROOT);
            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    ipodjs_steam_cover_cache_clear();
                    return root_menu_video_finish_native_screen(GO_TO_ROOT);
                }
                break;
        }
    }
}

static void root_menu_video_draw_games_menu(int selected)
{
    int count = ARRAYLEN(root_menu_video_games_items);
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int top = 0;
    int i;
    int x = IPODJS_PREVIEW_X;
    int y = 0;
    int w = LCD_WIDTH - x;
    int h = LCD_HEIGHT - y;

    if (selected >= visible)
        top = selected - visible + 1;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_status();
    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_screen_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, IPODJS_LIST_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    for (i = 0; i < visible && top + i < count; i++)
    {
        int index = top + i;
        int item_y = IPODJS_HEADER_HEIGHT + i * row_h;
        bool active = index == selected;

        if (active)
        {
            unsigned accent = root_menu_video_accent();
            root_menu_video_selection_gradient(0, item_y, IPODJS_LIST_WIDTH,
                                               row_h);
            lcd_set_foreground(IPODJS_PREVIEW_TEXT);
            lcd_set_background(accent);
        }
        else
        {
            lcd_set_foreground(root_menu_video_row_bg());
            lcd_fillrect(0, item_y, IPODJS_LIST_WIDTH, row_h);
            lcd_set_foreground(root_menu_video_text());
            lcd_set_background(root_menu_video_row_bg());
        }

        root_menu_video_puts_fit(6, item_y + 4, IPODJS_LIST_WIDTH - 24,
                                 root_menu_video_games_items[index].label,
                                 false);
        if (active)
            root_menu_video_draw_arrow(IPODJS_LIST_WIDTH - 13,
                                       item_y + (row_h - 6) / 2);
    }

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(54, 60, 70) : IPODJS_SPLIT);
    lcd_vline(IPODJS_SPLIT_X, 0, LCD_HEIGHT - 1);
    root_menu_video_draw_preview_for_title(
        root_menu_video_games_items[selected].label, x, y, w, h);
    ipodjs_video_draw_hold_overlay();
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
    ipodjs_trace_screen("Classic Games", "list", selected, top, count,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static void root_menu_video_draw_games_preview_only(int selected)
{
    if (button_hold())
    {
        root_menu_video_draw_games_menu(selected);
        return;
    }

    int count = ARRAYLEN(root_menu_video_games_items);
    int x = IPODJS_PREVIEW_X;

    selected = MAX(0, MIN(selected, count - 1));
    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    root_menu_video_draw_preview_for_title(
        root_menu_video_games_items[selected].label,
        x, 0, LCD_WIDTH - x, LCD_HEIGHT);
    lcd_update_rect(x, 0, LCD_WIDTH - x, LCD_HEIGHT);
}

static int root_menu_video_games_menu(void)
{
    int selected = 0;
    int previous_selected = 0;
    bool redraw = true;
    bool redraw_rows = false;
    bool held = button_hold();
    long next_slideshow = 0;
    long next_hold_refresh = 0;

    if (global_settings.ui_engine_games_appearance ==
        UI_ENGINE_GAMES_STEAM)
        return root_menu_video_steam_games_menu();

    root_menu_video_enter_native_screen();
    button_clear_queue();

    while (true)
    {
        int action;
        int count = ARRAYLEN(root_menu_video_games_items);

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_games_menu(selected);
            redraw = false;
            redraw_rows = false;
        }
        else if (redraw_rows)
        {
            if (!root_menu_video_draw_native_pane_delta(previous_selected,
                    selected, count,
                    root_menu_video_games_items[previous_selected].label,
                    root_menu_video_games_items[selected].label, false))
                root_menu_video_draw_games_menu(selected);
            redraw_rows = false;
        }

        action = get_action(CONTEXT_TREE, HZ/20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                else if (!held &&
                         root_menu_video_should_animate(
                            root_menu_video_preview_source_for_title(
                                root_menu_video_games_items[selected].label),
                            &next_slideshow))
                    root_menu_video_draw_games_preview_only(selected);
                break;
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MAX(0, selected - 1);
                redraw_rows = previous_selected != selected;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MIN(count - 1, selected + 1);
                redraw_rows = previous_selected != selected;
                break;
            case ACTION_STD_OK:
            {
                int ret;

                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                root_menu_video_finish_native_screen(0);
                ret = root_menu_video_games_items[selected].function(NULL);
                if (selected <= 4)
                    return ret;
                root_menu_video_enter_native_screen();
                redraw = true;
                break;
            }
            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
            {
                bool started_playback = false;

                if (root_menu_video_handle_play_pause(true,
                                                     &started_playback))
                    return root_menu_video_finish_native_screen(GO_TO_WPS);
                redraw = true;
                break;
            }
            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_ROOT);
        }
    }
}

#if defined(IPOD_6G) && !defined(SIMULATOR)
void settings_apply_ipod6g_videoout(int mode);
#endif

enum root_menu_video_qs_item {
    IPODJS_QS_VOLUME = 0,
    IPODJS_QS_BRIGHTNESS,
    IPODJS_QS_DARK_MODE,
    IPODJS_QS_SHUFFLE,
    IPODJS_QS_REPEAT,
    IPODJS_QS_ACCENT,
    IPODJS_QS_SURFACE,
    IPODJS_QS_DENSITY,
    IPODJS_QS_FONT,
    IPODJS_QS_EXTRAS_PANE,
#ifdef HAVE_HARDWARE_CLICK
    IPODJS_QS_HAPTICS,
#endif
    IPODJS_QS_HOLD,
#ifdef USB_ENABLE_ETHERNET
    IPODJS_QS_USB_CONNECTION,
#endif
#ifdef IPOD_ACCESSORY_PROTOCOL
    IPODJS_QS_KOKKIA,
#endif
#if defined(IPOD_6G) && !defined(SIMULATOR)
    IPODJS_QS_COMPOSITE_OUT,
#endif
    IPODJS_QS_CACHE_MEMORY,
    IPODJS_QS_COUNT,
};

#define IPODJS_QS_ICON_SIZE 18

struct root_menu_video_qs_icon_slot {
    bool valid;
    bool tried;
    int dark;
    struct bitmap bm;
    unsigned char data[BM_SIZE(IPODJS_QS_ICON_SIZE, IPODJS_QS_ICON_SIZE,
                               FORMAT_NATIVE, false)];
};

static struct root_menu_video_qs_icon_slot
    root_menu_video_qs_icons[IPODJS_QS_COUNT];

static const char *root_menu_video_qs_icon_path(int item)
{
    switch (item)
    {
        case IPODJS_QS_VOLUME:
            return IPODJS_ASSET_DIR "/qs/volume.18x18x24.bmp";
        case IPODJS_QS_BRIGHTNESS:
            return IPODJS_ASSET_DIR "/qs/brightness.18x18x24.bmp";
        case IPODJS_QS_DARK_MODE:
            return IPODJS_ASSET_DIR "/qs/dark-mode.18x18x24.bmp";
        case IPODJS_QS_SHUFFLE:
            return IPODJS_ASSET_DIR "/qs/shuffle.18x18x24.bmp";
        case IPODJS_QS_REPEAT:
            return IPODJS_ASSET_DIR "/qs/repeat.18x18x24.bmp";
        case IPODJS_QS_ACCENT:
            return IPODJS_ASSET_DIR "/qs/accent.18x18x24.bmp";
        case IPODJS_QS_SURFACE:
            return IPODJS_ASSET_DIR "/qs/surface.18x18x24.bmp";
        case IPODJS_QS_DENSITY:
            return IPODJS_ASSET_DIR "/qs/density.18x18x24.bmp";
        case IPODJS_QS_FONT:
            return IPODJS_ASSET_DIR "/qs/font.18x18x24.bmp";
        case IPODJS_QS_EXTRAS_PANE:
            return IPODJS_ASSET_DIR "/qs/surface.18x18x24.bmp";
#ifdef HAVE_HARDWARE_CLICK
        case IPODJS_QS_HAPTICS:
            return IPODJS_ASSET_DIR "/qs/volume.18x18x24.bmp";
#endif
        case IPODJS_QS_HOLD:
            return IPODJS_ASSET_DIR "/qs/hold.18x18x24.bmp";
#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
        case IPODJS_QS_USB_CONNECTION:
            return NULL;
#endif
#ifdef IPOD_ACCESSORY_PROTOCOL
        case IPODJS_QS_KOKKIA:
            return NULL;
#endif
#if defined(IPOD_6G) && !defined(SIMULATOR)
        case IPODJS_QS_COMPOSITE_OUT:
            return NULL;
#endif
        case IPODJS_QS_CACHE_MEMORY:
            return IPODJS_ASSET_DIR "/qs/cache-memory.18x18x24.bmp";
        default:
            return NULL;
    }
}

static struct bitmap *root_menu_video_qs_icon_asset(int item)
{
    struct root_menu_video_qs_icon_slot *slot;
    const char *path = root_menu_video_qs_icon_path(item);
    const char *load_path;
    char themed_path[MAX_PATH];
    int dark = root_menu_video_dark() ? 1 : 0;
    int rc;

    if (item < 0 || item >= IPODJS_QS_COUNT || !path)
        return NULL;

    slot = &root_menu_video_qs_icons[item];
    if (slot->dark != dark)
    {
        slot->valid = false;
        slot->tried = false;
        slot->dark = dark;
    }
    if (slot->valid)
        return &slot->bm;
    if (slot->tried)
        return NULL;

    load_path = root_menu_video_asset_path(path, themed_path,
                                           sizeof(themed_path));
    slot->tried = true;
    if (!file_exists(load_path))
        return NULL;

    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->bm.width = IPODJS_QS_ICON_SIZE;
    slot->bm.height = IPODJS_QS_ICON_SIZE;
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;

    rc = read_bmp_file(load_path, &slot->bm, sizeof(slot->data),
                       FORMAT_NATIVE, NULL);
    if (rc < 0)
        return NULL;

    slot->valid = true;
    return &slot->bm;
}

static const char *root_menu_video_qs_label(int item)
{
    switch (item)
    {
        case IPODJS_QS_VOLUME:
            return "Volume";
        case IPODJS_QS_BRIGHTNESS:
            return "Brightness";
        case IPODJS_QS_DARK_MODE:
            return "Dark";
        case IPODJS_QS_SHUFFLE:
            return "Shuffle";
        case IPODJS_QS_REPEAT:
            return "Repeat";
        case IPODJS_QS_ACCENT:
            return "Accent";
        case IPODJS_QS_SURFACE:
            return "Style";
        case IPODJS_QS_DENSITY:
            return "Rows";
        case IPODJS_QS_FONT:
            return "Font";
        case IPODJS_QS_EXTRAS_PANE:
            return "Extras Pane";
#ifdef HAVE_HARDWARE_CLICK
        case IPODJS_QS_HAPTICS:
            return "Haptics";
#endif
        case IPODJS_QS_HOLD:
            return "Hold";
#if defined(USB_ENABLE_ETHERNET) && !defined(SIMULATOR)
        case IPODJS_QS_USB_CONNECTION:
            return "USB Connection";
#endif
#ifdef IPOD_ACCESSORY_PROTOCOL
        case IPODJS_QS_KOKKIA:
            return "Kokkia";
#endif
#if defined(IPOD_6G) && !defined(SIMULATOR)
        case IPODJS_QS_COMPOSITE_OUT:
            return "Composite Out";
#endif
        case IPODJS_QS_CACHE_MEMORY:
            return "Cache & Memory";
        default:
            return "";
    }
}

static int root_menu_video_qs_percent(int item)
{
    if (item == IPODJS_QS_VOLUME)
        return root_menu_video_volume_percent();

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    if (item == IPODJS_QS_BRIGHTNESS)
    {
        int range = MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING;
        if (range <= 0)
            return 100;
        return (global_settings.brightness - MIN_BRIGHTNESS_SETTING) *
               100 / range;
    }
#endif

    return 0;
}

#ifdef HAVE_HARDWARE_CLICK
static bool root_menu_video_qs_click_feedback_enabled(void)
{
    return global_settings.haptics_enabled ||
           global_settings.keyclick_hardware ||
           global_settings.keyclick != 0;
}
#endif

static void root_menu_video_qs_value(int item, char *buf, size_t buf_size)
{
    static const char *repeat[] = {
        "Off", "All", "One", "Shuffle"
#ifdef AB_REPEAT_ENABLE
        , "A-B"
#endif
    };
    int percent;

    switch (item)
    {
        case IPODJS_QS_VOLUME:
            percent = MAX(0, MIN(root_menu_video_qs_percent(item), 100));
            snprintf(buf, buf_size, "%d%%", percent);
            break;

        case IPODJS_QS_BRIGHTNESS:
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
            percent = MAX(0, MIN(root_menu_video_qs_percent(item), 100));
            snprintf(buf, buf_size, "%d%%", percent);
#else
            strmemccpy(buf, "Fixed", buf_size);
#endif
            break;

        case IPODJS_QS_SHUFFLE:
            strmemccpy(buf, global_settings.playlist_shuffle ? "On" : "Off",
                       buf_size);
            break;

        case IPODJS_QS_DARK_MODE:
            strmemccpy(buf, global_settings.ui_engine_dark_mode ? "On" :
                       "Off", buf_size);
            break;

        case IPODJS_QS_REPEAT:
            strmemccpy(buf, repeat[MAX(0, MIN(global_settings.repeat_mode,
                       (int)ARRAYLEN(repeat) - 1))], buf_size);
            break;

        case IPODJS_QS_ACCENT:
            strmemccpy(buf, root_menu_video_accent_name(), buf_size);
            break;

        case IPODJS_QS_SURFACE:
            if (global_settings.ui_engine_surface ==
                UI_ENGINE_SURFACE_TRANSPARENT)
                strmemccpy(buf, "Glass", buf_size);
            else if (global_settings.ui_engine_surface ==
                     UI_ENGINE_SURFACE_SOFT)
                strmemccpy(buf, "Soft", buf_size);
            else
                strmemccpy(buf, "Solid", buf_size);
            break;

        case IPODJS_QS_DENSITY:
            strmemccpy(buf, global_settings.ui_engine_density ==
                       UI_ENGINE_DENSITY_COMPACT ? "Compact" : "Classic",
                       buf_size);
            break;

        case IPODJS_QS_FONT:
            if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_SMALL)
                strmemccpy(buf, "Small", buf_size);
            else if (global_settings.ui_engine_font_scale ==
                     UI_ENGINE_FONT_LARGE)
                strmemccpy(buf, "Large", buf_size);
            else
                strmemccpy(buf, "Helvetica", buf_size);
            break;

        case IPODJS_QS_EXTRAS_PANE:
            if (global_settings.ui_engine_extras_pane ==
                UI_ENGINE_EXTRAS_SITEKICK)
                strmemccpy(buf, "Sitekick", buf_size);
            else if (global_settings.ui_engine_extras_pane ==
                     UI_ENGINE_EXTRAS_AVATAR)
                strmemccpy(buf, "Avatar", buf_size);
            else
                strmemccpy(buf, "Clock", buf_size);
            break;

#ifdef HAVE_HARDWARE_CLICK
        case IPODJS_QS_HAPTICS:
            strmemccpy(buf, root_menu_video_qs_click_feedback_enabled() ?
                       "On" : "Off",
                       buf_size);
            break;
#endif

        case IPODJS_QS_HOLD:
            strmemccpy(buf, global_settings.ui_engine_hold_effect ==
                       UI_ENGINE_HOLD_LOCKSCREEN ? "Lock" : "Dim",
                       buf_size);
            break;

#if (defined(USB_ENABLE_ETHERNET) || defined(USB_ENABLE_IPHETH_HOST)) && \
    !defined(SIMULATOR)
        case IPODJS_QS_USB_CONNECTION:
            if (global_settings.usb_mode == USB_MODE_INTERNET)
                strmemccpy(buf, usb_internet_connected() ? "Internet" :
                           "Internet (plug in)", buf_size);
#ifdef USB_ENABLE_IPHETH_HOST
            else if (global_settings.usb_mode == USB_MODE_IPHONE_TETHER)
            {
                strmemccpy(buf, usb_internet_connected() ?
                           "iPhone Internet" : "Open RockPod Link",
                           buf_size);
            }
#endif
            else if (global_settings.usb_mode == USB_MODE_CHARGE)
                strmemccpy(buf, "Charge Only", buf_size);
            else
                strmemccpy(buf, "Storage", buf_size);
            break;
#endif

#ifdef IPOD_ACCESSORY_PROTOCOL
        case IPODJS_QS_KOKKIA:
        {
            static const char * const states[] = {
                "Disconnected", "Detecting", "Authenticating",
                "Connected", "Retrying"
            };
            enum iap_connection_status status = iap_connection_status();

            strmemccpy(buf, states[MAX(0, MIN((int)status,
                       (int)ARRAYLEN(states) - 1))], buf_size);
            break;
        }
#endif

#if defined(IPOD_6G) && !defined(SIMULATOR)
        case IPODJS_QS_COMPOSITE_OUT:
        {
            static const char * const states[] = {"Off", "Auto", "On"};
            int mode = MAX(IPOD6G_VIDEOOUT_OFF,
                           MIN(global_settings.composite_video_output,
                               IPOD6G_VIDEOOUT_ON));
#ifdef IPOD_ACCESSORY_PROTOCOL
            if (iap_kokkia_present() && mode != IPOD6G_VIDEOOUT_OFF)
            {
                strmemccpy(buf, "Blocked", buf_size);
                break;
            }
#endif
            strmemccpy(buf, states[mode], buf_size);
            break;
        }
#endif

        case IPODJS_QS_CACHE_MEMORY:
            strmemccpy(buf, "Open", buf_size);
            break;

        default:
            buf[0] = '\0';
            break;
    }
}

static void root_menu_video_qs_apply_shuffle(bool enabled)
{
    struct playlist_info *playlist;

    if (global_settings.playlist_shuffle == enabled)
        return;

    global_settings.playlist_shuffle = enabled;
    playlist = playlist_get_current();
    if (playlist && playlist->started &&
        (audio_status() & AUDIO_STATUS_PLAY) == AUDIO_STATUS_PLAY)
    {
        if (enabled)
            playlist_randomise(playlist, current_tick, true);
        else
            playlist_sort(playlist, true);
    }
}

static bool root_menu_video_qs_adjust(int item, int delta)
{
    bool changed = false;

    switch (item)
    {
        case IPODJS_QS_VOLUME:
        {
            int old_volume = global_status.volume;

            adjust_volume(delta < 0 ? -1 : 1);
            if (global_status.volume != old_volume)
            {
                global_status.last_volume_change = current_tick;
                changed = true;
            }
            break;
        }

        case IPODJS_QS_BRIGHTNESS:
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
        {
            int range = MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING;
            int step = MAX(1, range / 16);
            int value = global_settings.brightness +
                        (delta < 0 ? -step : step);
            value = MAX(MIN_BRIGHTNESS_SETTING,
                        MIN(MAX_BRIGHTNESS_SETTING, value));
            if (value != global_settings.brightness)
            {
                global_settings.brightness = value;
                backlight_set_brightness(value);
                changed = true;
            }
            break;
        }
#else
            break;
#endif

        case IPODJS_QS_SHUFFLE:
            root_menu_video_qs_apply_shuffle(!global_settings.playlist_shuffle);
            changed = true;
            break;

        case IPODJS_QS_DARK_MODE:
            global_settings.ui_engine_dark_mode =
                !global_settings.ui_engine_dark_mode;
            changed = true;
            break;

        case IPODJS_QS_REPEAT:
        {
            int value = global_settings.repeat_mode + (delta < 0 ? -1 : 1);
            if (value < 0)
                value = NUM_REPEAT_MODES - 1;
            else if (value >= NUM_REPEAT_MODES)
                value = 0;
            if (value != global_settings.repeat_mode)
            {
                global_settings.repeat_mode = value;
                if ((audio_status() & AUDIO_STATUS_PLAY) == AUDIO_STATUS_PLAY)
                    audio_flush_and_reload_tracks();
                changed = true;
            }
            break;
        }

        case IPODJS_QS_ACCENT:
            global_settings.ui_engine_accent =
                (global_settings.ui_engine_accent +
                 (delta < 0 ? UI_ENGINE_ACCENT_COUNT - 1 : 1)) %
                UI_ENGINE_ACCENT_COUNT;
            changed = true;
            break;

        case IPODJS_QS_SURFACE:
            global_settings.ui_engine_surface =
                (global_settings.ui_engine_surface + (delta < 0 ? 2 : 1)) % 3;
            changed = true;
            break;

        case IPODJS_QS_DENSITY:
            global_settings.ui_engine_density =
                global_settings.ui_engine_density ==
                UI_ENGINE_DENSITY_COMPACT ? UI_ENGINE_DENSITY_COMFORTABLE :
                UI_ENGINE_DENSITY_COMPACT;
            changed = true;
            break;

        case IPODJS_QS_FONT:
            global_settings.ui_engine_font_scale =
                (global_settings.ui_engine_font_scale + (delta < 0 ? 2 : 1))
                % 3;
            changed = true;
            break;

        case IPODJS_QS_EXTRAS_PANE:
            global_settings.ui_engine_extras_pane =
                (global_settings.ui_engine_extras_pane +
                 (delta < 0 ? 2 : 1)) % 3;
            changed = true;
            break;

#ifdef HAVE_HARDWARE_CLICK
        case IPODJS_QS_HAPTICS:
            if (root_menu_video_qs_click_feedback_enabled())
            {
                if (global_settings.haptics_enabled)
                    haptic_feedback(24, 48);
                global_settings.haptics_enabled = false;
                global_settings.keyclick_hardware = false;
                global_settings.keyclick = 0;
            }
            else
            {
                global_settings.haptics_enabled = true;
                haptic_feedback(24, 48);
            }
            changed = true;
            break;
#endif

        case IPODJS_QS_HOLD:
            global_settings.ui_engine_hold_effect =
                global_settings.ui_engine_hold_effect ==
                UI_ENGINE_HOLD_LOCKSCREEN ? UI_ENGINE_HOLD_DIM :
                UI_ENGINE_HOLD_LOCKSCREEN;
            changed = true;
            break;

#if defined(IPOD_6G) && !defined(SIMULATOR)
        case IPODJS_QS_COMPOSITE_OUT:
        {
            int value;

#ifdef IPOD_ACCESSORY_PROTOCOL
            if (iap_kokkia_present())
                break;
#endif
            (void)delta;
            value = global_settings.composite_video_output ==
                    IPOD6G_VIDEOOUT_ON ? IPOD6G_VIDEOOUT_OFF :
                                        IPOD6G_VIDEOOUT_ON;
            global_settings.composite_video_output = value;
            settings_apply_ipod6g_videoout(value);
            changed = true;
            break;
        }
#endif

#if (defined(USB_ENABLE_ETHERNET) || defined(USB_ENABLE_IPHETH_HOST)) && \
    !defined(SIMULATOR)
        case IPODJS_QS_USB_CONNECTION:
        {
            int mode_count = 2;
#ifdef USB_ENABLE_ETHERNET
            mode_count++;
#endif
#ifdef USB_ENABLE_IPHETH_HOST
            mode_count++;
#endif
            int value = global_settings.usb_mode +
                        (delta < 0 ? mode_count - 1 : 1);
            value %= mode_count;
            if (value != global_settings.usb_mode)
            {
                global_settings.usb_mode = value;
                usb_set_mode(value);
                changed = true;
            }
            break;
        }
#endif
    }

    return changed;
}

static void root_menu_video_clear_preview_caches(void)
{
    root_menu_video_preview_cache_mode = IPODJS_PREVIEW_CACHE_NONE;
    root_menu_video_preview_loaded = IPODJS_PREVIEW_NONE;
    root_menu_video_preview_path_count = 0;
    root_menu_video_preview_victim = 0;
    root_menu_video_preview_decode_window_tick = 0;
    root_menu_video_preview_decode_budget = 0;
    root_menu_video_preview_last_drawn_source = IPODJS_PREVIEW_NONE;
    root_menu_video_preview_last_drawn_index = -1;
    root_menu_video_menu_preview_stamp = 0;

    for (int i = 0; i < IPODJS_PREVIEW_IMAGE_CACHE; i++)
        root_menu_video_preview_slots[i].valid = false;
    for (int i = 0; i < IPODJS_MENU_PREVIEW_CACHE; i++)
        root_menu_video_menu_preview_slots[i].valid = false;
    for (int i = 0; i < (int)ARRAYLEN(root_menu_video_preview_failures); i++)
        root_menu_video_preview_failures[i].valid = false;
    for (int i = 0;
         i < (int)ARRAYLEN(root_menu_video_menu_preview_failures); i++)
        root_menu_video_menu_preview_failures[i].valid = false;
    for (int i = 0;
         i < (int)ARRAYLEN(root_menu_video_preview_source_caches); i++)
    {
        root_menu_video_preview_source_caches[i].loaded = false;
        root_menu_video_preview_source_caches[i].next_reload = 0;
        root_menu_video_preview_source_caches[i].path_count = 0;
    }
}

static void root_menu_video_clear_database_view_caches(void)
{
#ifdef HAVE_TAGCACHE
    if (!root_menu_video_uses_stock_music())
    {
        for (int i = 0; i < IPODJS_DB_ART_CACHE; i++)
        {
            root_menu_video_db_art_cache[i].valid = false;
            root_menu_video_db_art_cache[i].miss = false;
            root_menu_video_db_art_cache[i].album_seek = -1;
        }
        root_menu_video_db_art_next = 0;
        root_menu_video_album_groups.valid = false;
        root_menu_video_album_groups.count = 0;
    }
#endif
}

static void root_menu_video_clear_asset_caches(void)
{
    root_menu_video_asset_dark_state = -1;
    root_menu_video_invalidate_ui_asset_cache();
    for (int i = 0; i < IPODJS_QS_COUNT; i++)
    {
        root_menu_video_qs_icons[i].valid = false;
        root_menu_video_qs_icons[i].tried = false;
        root_menu_video_qs_icons[i].dark = -1;
    }

    memset(&root_menu_video_weather_cache, 0,
           sizeof(root_menu_video_weather_cache));
    root_menu_video_weather_icon_valid = false;
    root_menu_video_weather_icon_path[0] = '\0';
    root_menu_video_weather_icon_miss[0] = '\0';
    root_menu_video_weather_icon_miss_night = false;
    root_menu_video_weather_icon_retry = 0;

    root_menu_video_storage_cached[0] = '\0';
    root_menu_video_storage_cached_used_pct = 0;
    root_menu_video_storage_cached_tick = 0;
    root_menu_video_storage_cached_valid = false;
}

static void root_menu_video_clear_disposable_caches(void)
{
    lcd_setfont(FONT_SYSFIXED);
    ipodjs_ui_label_cache_reset();
    root_menu_video_clear_asset_caches();
    root_menu_video_clear_preview_caches();
    root_menu_video_clear_database_view_caches();

#ifdef HAVE_LCD_COLOR
    for (int i = 0; i < VIDEO_LIST_LOOKUP_CACHE; i++)
        video_thumb_lookup_cache[i].valid = false;
    for (int i = 0; i < VIDEO_LIST_BITMAP_CACHE; i++)
        video_thumb_bitmap_cache[i].valid = false;
    video_thumb_lookup_victim = 0;
    video_thumb_bitmap_tick = 0;
#endif
}

static bool root_menu_video_draw_qs_icon(int x, int y, int item, bool active)
{
    struct bitmap *bm;

#if defined(IPOD_6G) && !defined(SIMULATOR)
    if (item == IPODJS_QS_COMPOSITE_OUT)
    {
        /* External display with a dock/composite lead.  This is rendered
         * instead of loaded from disk so the control always has its icon,
         * including on incomplete or third-party themes. */
        lcd_set_foreground(active ? IPODJS_PREVIEW_TEXT :
                           root_menu_video_muted_text());
        lcd_drawrect(x + 1, y + 1, 14, 11);
        lcd_drawrect(x + 3, y + 3, 10, 7);
        lcd_vline(x + 8, y + 12, y + 14);
        lcd_hline(x + 5, x + 11, y + 15);
        lcd_hline(x + 15, x + 17, y + 7);
        lcd_fillrect(x + 16, y + 6, 2, 3);
        return true;
    }
#else
    (void)active;
#endif

#ifdef USB_ENABLE_ETHERNET
    if (item == IPODJS_QS_USB_CONNECTION)
    {
        ipodjs_ui_draw_wifi_indicator(&screens[SCREEN_MAIN], x - 2, y + 1);
        return true;
    }
#endif

#ifdef IPOD_ACCESSORY_PROTOCOL
    if (item == IPODJS_QS_KOKKIA)
    {
        ipodjs_ui_draw_bluetooth_indicator(&screens[SCREEN_MAIN],
                                           x + 3, y);
        return true;
    }
#endif

    if (item == IPODJS_QS_VOLUME)
    {
        bm = root_menu_video_apple_slider_asset(
            IPODJS_APPLE_VOLUME_HIGH);
        if (bm)
        {
            lcd_bmp(bm, x, y);
            return true;
        }
    }
    else if (item == IPODJS_QS_BRIGHTNESS)
    {
        bm = root_menu_video_apple_slider_asset(
            IPODJS_APPLE_BRIGHTNESS_LOW);
        if (bm)
        {
            lcd_bmp(bm, x, y - (bm->height - IPODJS_QS_ICON_SIZE) / 2);
            return true;
        }
    }

    bm = root_menu_video_qs_icon_asset(item);

    if (bm)
    {
        lcd_bitmap_transparent((fb_data *)bm->data, x, y,
                               bm->width, bm->height);
        return true;
    }

    return false;
}

static int root_menu_video_qs_row_height(void)
{
    return MAX(21,
               font_get(root_menu_video_font())->height + 2);
}

static int root_menu_video_qs_visible_rows(void)
{
    const int list_h = LCD_HEIGHT - IPODJS_HEADER_HEIGHT -
                       IPODJS_MENU_BOTTOM_INSET;

    return MAX(1, list_h / root_menu_video_qs_row_height());
}

static int root_menu_video_qs_top(int selected)
{
    int visible = root_menu_video_qs_visible_rows();
    int top = selected >= visible ? selected - visible + 1 : 0;

    return MIN(top, MAX(0, IPODJS_QS_COUNT - visible));
}

static void root_menu_video_qs_rect(int item, int top, int *xp, int *yp,
                                    int *wp, int *hp)
{
    const int row_h = root_menu_video_qs_row_height();
    const int visible = root_menu_video_qs_visible_rows();

    *xp = 0;
    *yp = IPODJS_HEADER_HEIGHT + (item - top) * row_h;
    *wp = IPODJS_QS_COUNT > visible ? LCD_WIDTH - 4 : LCD_WIDTH;
    *hp = row_h;
}

static void root_menu_video_draw_qs_row(int x, int y, int w, int h,
                                        bool active, bool adjusting)
{
    if (active)
    {
        root_menu_video_selection_gradient(x, y, w, h);
        lcd_set_foreground(LCD_RGBPACK(166, 224, 255));
        lcd_hline(x, x + w - 1, y);
        lcd_set_foreground(LCD_RGBPACK(0, 75, 165));
        lcd_hline(x, x + w - 1, y + h - 1);
        if (adjusting)
        {
            lcd_set_foreground(LCD_RGBPACK(235, 248, 255));
            lcd_drawrect(x + 1, y + 1, w - 2, h - 2);
        }
    }
    else
    {
        lcd_set_foreground(root_menu_video_row_bg());
        lcd_fillrect(x, y, w, h);
        lcd_set_foreground(root_menu_video_dark() ?
                           LCD_RGBPACK(45, 50, 58) :
                           IPODJS_SPLIT);
        lcd_hline(x, x + w - 1, y + h - 1);
    }
}

static void root_menu_video_draw_qs_footer_text(const char *message)
{
    int y = LCD_HEIGHT - IPODJS_MENU_BOTTOM_INSET;
    unsigned top;
    unsigned bottom;
    unsigned text;

    if (root_menu_video_dark())
    {
        top = LCD_RGBPACK(48, 53, 62);
        bottom = LCD_RGBPACK(28, 32, 39);
        text = LCD_RGBPACK(204, 209, 217);
    }
    else
    {
        top = LCD_RGBPACK(246, 247, 249);
        bottom = LCD_RGBPACK(202, 205, 210);
        text = LCD_RGBPACK(72, 75, 80);
    }

    root_menu_video_gradient(0, y, LCD_WIDTH, IPODJS_MENU_BOTTOM_INSET,
                             top, bottom);
    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(78, 84, 94) : LCD_RGBPACK(151, 154, 159));
    lcd_hline(0, LCD_WIDTH - 1, y);
    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(text);
    lcd_set_background(bottom);
    root_menu_video_puts_fit(8, y + 3, LCD_WIDTH - 16, message, true);
}

static void root_menu_video_draw_qs_footer(int selected, bool adjusting)
{
    if (adjusting)
        root_menu_video_draw_qs_footer_text(
            "Scroll to adjust - Select when done");
#if defined(IPOD_6G) && !defined(SIMULATOR)
    else if (selected == IPODJS_QS_COMPOSITE_OUT)
        root_menu_video_draw_qs_footer_text(
            "Select to turn composite output on or off");
#endif
    else if (selected == IPODJS_QS_CACHE_MEMORY)
        root_menu_video_draw_qs_footer_text(
            "Select for cache and memory");
    else if (selected == IPODJS_QS_EXTRAS_PANE)
        root_menu_video_draw_qs_footer_text(
            "Select to change the Extras preview");
#ifdef IPOD_ACCESSORY_PROTOCOL
    else if (selected == IPODJS_QS_KOKKIA)
        root_menu_video_draw_qs_footer_text(
            "Select for connection status");
#endif
    else
        root_menu_video_draw_qs_footer_text(
            "Select Volume or Brightness to adjust");
}

static void root_menu_video_draw_qs_scrollbar(int top, int visible)
{
    const int list_h = LCD_HEIGHT - IPODJS_HEADER_HEIGHT -
                       IPODJS_MENU_BOTTOM_INSET;
    int track_y;
    int track_h;
    int thumb_h;
    int thumb_y;

    if (visible >= IPODJS_QS_COUNT)
        return;

    track_y = IPODJS_HEADER_HEIGHT + 2;
    track_h = list_h - 4;
    thumb_h = MAX(10, track_h * visible / IPODJS_QS_COUNT);
    thumb_y = track_y + (track_h - thumb_h) * top /
        MAX(1, IPODJS_QS_COUNT - visible);

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(53, 58, 66) : LCD_RGBPACK(213, 216, 221));
    lcd_fillrect(LCD_WIDTH - 3, track_y, 2, track_h);
    lcd_set_foreground(root_menu_video_accent());
    lcd_fillrect(LCD_WIDTH - 3, thumb_y, 2, thumb_h);
}

static void root_menu_video_draw_apple_slider_screen(int item)
{
    const bool brightness = item == IPODJS_QS_BRIGHTNESS;
    const char *title = brightness ? "Brightness" : "Volume";
    int percent = MAX(0, MIN(100, root_menu_video_qs_percent(item)));
    unsigned panel = root_menu_video_panel();

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();
    root_menu_video_draw_status_title(title);
    lcd_set_foreground(panel);
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
    if (!root_menu_video_draw_apple_slider(36, 105, percent, brightness))
    {
        root_menu_video_draw_stock_meter(58, 108, 204, 13, percent,
                                         root_menu_video_accent());
    }
    ipodjs_video_draw_hold_overlay();
    lcd_update();
    ipodjs_trace_screen(title, "apple-slider", percent, 0, 101,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static void root_menu_video_draw_quick_settings(int selected, bool adjusting)
{
    int i;
    int top = root_menu_video_qs_top(selected);
    int visible = root_menu_video_qs_visible_rows();
    unsigned bg = root_menu_video_screen_bg();
    unsigned list_bg = root_menu_video_row_bg();

    if (adjusting &&
        (selected == IPODJS_QS_VOLUME ||
         selected == IPODJS_QS_BRIGHTNESS))
    {
        root_menu_video_draw_apple_slider_screen(selected);
        return;
    }

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(bg);
    lcd_clear_display();

    root_menu_video_draw_status_title("Quick Settings");

    lcd_set_foreground(list_bg);
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    lcd_setfont(root_menu_video_font());

    for (i = top; i < IPODJS_QS_COUNT && i < top + visible; i++)
    {
        char value[32];
        int x, y, w, h;
        bool active = i == selected;
        bool slider = i == IPODJS_QS_VOLUME || i == IPODJS_QS_BRIGHTNESS;
        unsigned text = active ? IPODJS_PREVIEW_TEXT : root_menu_video_text();
        unsigned muted = active ? LCD_RGBPACK(230, 245, 255) :
            root_menu_video_muted_text();
        int icon_y;
        int text_y;
        int label_x;
        int label_w;
        bool has_icon;

        root_menu_video_qs_rect(i, top, &x, &y, &w, &h);
        root_menu_video_qs_value(i, value, sizeof(value));
        root_menu_video_draw_qs_row(x, y, w, h,
                                    active, active && adjusting);

        icon_y = y + (h - IPODJS_QS_ICON_SIZE) / 2;
        text_y = y + MAX(1, (h - font_get(root_menu_video_font())->height) /
                            2) + root_menu_video_text_y_offset();

        has_icon = root_menu_video_draw_qs_icon(x + 7, icon_y, i, active);
        label_x = has_icon ? x + 32 : x + 9;
        label_w = slider ? (has_icon ? 118 : 141) : w - 104;
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(text);
        lcd_set_background(active ? root_menu_video_accent() :
                           list_bg);
        root_menu_video_puts_fit(label_x, text_y, label_w,
                                 root_menu_video_qs_label(i), false);

        if (slider)
        {
            int percent = MAX(0, MIN(root_menu_video_qs_percent(i), 100));
            int meter_x = x + 154;
            int meter_w = 82;
            root_menu_video_draw_meter(meter_x, y + (h - 8) / 2, meter_w, 8,
                                       percent,
                                       root_menu_video_accent());
            lcd_set_foreground(muted);
            root_menu_video_puts_fit(x + w - 47, text_y, 40, value, true);
        }
        else
        {
            lcd_set_foreground(muted);
            root_menu_video_puts_fit(x + w - 82, text_y, 68, value, true);
        }

        if (active && !adjusting)
            root_menu_video_draw_arrow(x + w - 9, y + (h - 6) / 2);
        else if (active)
        {
            int cy = y + h / 2;

            lcd_set_foreground(IPODJS_PREVIEW_TEXT);
            lcd_hline(x + w - 12, x + w - 8, cy - 3);
            lcd_hline(x + w - 14, x + w - 6, cy);
            lcd_hline(x + w - 12, x + w - 8, cy + 3);
        }
    }

    root_menu_video_draw_qs_scrollbar(top, visible);
    root_menu_video_draw_qs_footer(selected, adjusting);
    ipodjs_video_draw_hold_overlay();
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
    ipodjs_trace_screen("Quick Settings", "full", selected, top,
                        IPODJS_QS_COUNT, 0, 0, LCD_WIDTH, LCD_HEIGHT);
}

enum root_menu_video_cache_item {
    IPODJS_CACHE_REFRESH = 0,
    IPODJS_CACHE_RESTART,
    IPODJS_CACHE_BACK,
    IPODJS_CACHE_COUNT,
};

static const char * const root_menu_video_cache_labels[IPODJS_CACHE_COUNT] = {
    "Refresh UI Cache",
    "Restart to Clear RAM",
    "Back",
};

static const char * const root_menu_video_cache_values[IPODJS_CACHE_COUNT] = {
    "Safe",
    "Full Reset",
    "",
};

static const char * const root_menu_video_cache_help[IPODJS_CACHE_COUNT] = {
    "Keeps music, settings, and files",
    "Stops music and resets volatile RAM",
    "Return to Quick Settings",
};

static void root_menu_video_draw_cache_memory(int selected)
{
    const int row_h = 42;
    const int start_y = IPODJS_HEADER_HEIGHT + 12;
    unsigned list_bg = root_menu_video_row_bg();

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();
    root_menu_video_draw_status_title("Cache & Memory");

    lcd_set_foreground(list_bg);
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
    lcd_setfont(root_menu_video_font());

    for (int i = 0; i < IPODJS_CACHE_COUNT; i++)
    {
        int y = start_y + i * row_h;
        int text_y = y + MAX(1,
            (row_h - font_get(root_menu_video_font())->height) / 2) +
            root_menu_video_text_y_offset();
        bool active = i == selected;

        root_menu_video_draw_qs_row(0, y, LCD_WIDTH, row_h, active, false);
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(active ? IPODJS_PREVIEW_TEXT :
                           root_menu_video_text());
        lcd_set_background(active ? root_menu_video_accent() : list_bg);
        root_menu_video_puts_fit(10, text_y, 205,
                                 root_menu_video_cache_labels[i], false);
        lcd_set_foreground(active ? LCD_RGBPACK(230, 245, 255) :
                           root_menu_video_muted_text());
        root_menu_video_puts_fit(218, text_y, 86,
                                 root_menu_video_cache_values[i], true);
        if (active)
            root_menu_video_draw_arrow(LCD_WIDTH - 9,
                                       y + (row_h - 6) / 2);
    }

    root_menu_video_draw_qs_footer_text(
        root_menu_video_cache_help[selected]);
    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

static void root_menu_video_draw_reboot_confirmation(void)
{
    unsigned bg = root_menu_video_screen_bg();

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(bg);
    lcd_clear_display();
    root_menu_video_draw_status_title("Restart Rockbox?");

    lcd_set_foreground(root_menu_video_row_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
    lcd_setfont(root_menu_video_font());
    lcd_set_background(root_menu_video_row_bg());
    lcd_set_foreground(root_menu_video_text());
    root_menu_video_puts_fit(12, 80, LCD_WIDTH - 24,
                            "Clears RAM; music will stop.", true);
    lcd_set_foreground(root_menu_video_muted_text());
    root_menu_video_puts_fit(12, 116, LCD_WIDTH - 24,
                            "Files and the music database stay intact.",
                            true);
    root_menu_video_draw_qs_footer_text(
        "Select: Restart - Menu: Cancel");
    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

static bool root_menu_video_confirm_reboot(void)
{
    bool redraw = true;
    bool held = button_hold();
    long next_hold_refresh = 0;

    root_menu_wait_for_button_release();
    button_clear_queue();

    while (true)
    {
        int action;

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_reboot_confirmation();
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, HZ/8);
        if (action == SYS_USB_CONNECTED)
        {
            ipodjs_ui_handle_system_event(action, &redraw);
            return false;
        }
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;

        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;

            case ACTION_STD_OK:
                if (!root_menu_video_hold_update(&held, &redraw))
                    return true;
                break;

            case ACTION_STD_CANCEL:
            case ACTION_STD_CONTEXT:
            case ACTION_STD_QUICKSCREEN:
            case ACTION_STD_MENU:
                if (!root_menu_video_hold_update(&held, &redraw))
                    return false;
                break;
        }
    }
}

static bool root_menu_video_cache_memory_menu(bool changed_settings,
                                               bool changed_status)
{
    int selected = 0;
    bool redraw = true;
    bool held = button_hold();
    long next_hold_refresh = 0;

    root_menu_wait_for_button_release();
    button_clear_queue();

    while (true)
    {
        int action;

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_cache_memory(selected);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, HZ/8);
        if (action == SYS_USB_CONNECTED)
        {
            ipodjs_ui_handle_system_event(action, &redraw);
            return false;
        }
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;

        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = selected <= 0 ? IPODJS_CACHE_COUNT - 1 :
                           selected - 1;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = selected >= IPODJS_CACHE_COUNT - 1 ? 0 :
                           selected + 1;
                redraw = true;
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (selected == IPODJS_CACHE_REFRESH)
                {
                    root_menu_video_clear_disposable_caches();
                    splash(HZ, "UI cache refreshed");
                    root_menu_wait_for_button_release();
                    button_clear_queue();
                    redraw = true;
                }
                else if (selected == IPODJS_CACHE_RESTART)
                {
                    if (!root_menu_video_confirm_reboot())
                    {
                        redraw = true;
                        break;
                    }
                    if (changed_settings)
                        settings_save();
                    if (changed_status)
                        status_save(false);
                    if (root_menu_request_reboot())
                        return true;
                    redraw = true;
                }
                else
                    return false;
                break;

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                (void)root_menu_video_handle_play_pause(false, NULL);
                redraw = true;
                break;

            case ACTION_STD_CANCEL:
            case ACTION_STD_CONTEXT:
            case ACTION_STD_QUICKSCREEN:
            case ACTION_STD_MENU:
                if (!root_menu_video_hold_update(&held, &redraw))
                    return false;
                break;
        }
    }
}

#ifdef IPOD_ACCESSORY_PROTOCOL
enum root_menu_video_kokkia_item {
    IPODJS_KOKKIA_STATUS = 0,
    IPODJS_KOKKIA_ACTIVATION,
    IPODJS_KOKKIA_SERIAL,
    IPODJS_KOKKIA_TRAFFIC,
    IPODJS_KOKKIA_RETRIES,
    IPODJS_KOKKIA_RECONNECT,
    IPODJS_KOKKIA_HELP,
    IPODJS_KOKKIA_PAUSE_UNPLUG,
    IPODJS_KOKKIA_BACK,
    IPODJS_KOKKIA_COUNT,
};

static const char * const
root_menu_video_kokkia_labels[IPODJS_KOKKIA_COUNT] = {
    "Status",
    "Activation",
    "Serial",
    "Traffic",
    "Retries",
    "Reconnect Now",
    "Pairing Help",
    "Pause on Unplug",
    "Back",
};

static const char *root_menu_video_kokkia_state_name(
    enum iap_connection_status status)
{
    static const char * const names[] = {
        "Disconnected", "Detecting", "Authenticating",
        "Connected", "Retrying"
    };

    return names[MAX(0, MIN((int)status, (int)ARRAYLEN(names) - 1))];
}

static const char *root_menu_video_kokkia_reason_name(
    enum iap_reconnect_reason reason)
{
    static const char * const names[] = {
        "None", "No data", "Autobaud", "Auth timeout",
        "Activation", "Restart", "Manual", "Link errors"
    };

    return names[MAX(0, MIN((int)reason, (int)ARRAYLEN(names) - 1))];
}

static void root_menu_video_kokkia_value(
    int item, const struct iap_connection_info *info,
    char *value, size_t value_size)
{
    switch (item)
    {
        case IPODJS_KOKKIA_STATUS:
            strmemccpy(value,
                       root_menu_video_kokkia_state_name(info->status),
                       value_size);
            break;

        case IPODJS_KOKKIA_ACTIVATION:
            if (info->activated)
                strmemccpy(value, "Activated", value_size);
            else if (info->authenticated)
                strmemccpy(value, "Waiting", value_size);
            else if (info->kokkia_seen)
                strmemccpy(value, "Authenticating", value_size);
            else
                strmemccpy(value, "Not detected", value_size);
            break;

        case IPODJS_KOKKIA_SERIAL:
            if (info->bitrate > 0)
                snprintf(value, value_size, "%d bps", info->bitrate);
            else if (info->bitrate < 0)
                snprintf(value, value_size, "Auto %d / %u ABR",
                         -info->bitrate, info->autobaud_relaunches);
            else
                strmemccpy(value, "Auto waiting", value_size);
            break;

        case IPODJS_KOKKIA_TRAFFIC:
            snprintf(value, value_size, "%u B / E%u / C%u",
                     info->rx_bytes, info->uart_errors,
                     info->checksum_errors);
            break;

        case IPODJS_KOKKIA_RETRIES:
            if (info->retry_count)
                snprintf(value, value_size, "%u - %s",
                         info->retry_count,
                         root_menu_video_kokkia_reason_name(info->reason));
            else
                strmemccpy(value,
                           root_menu_video_kokkia_reason_name(info->reason),
                           value_size);
            break;

        case IPODJS_KOKKIA_RECONNECT:
            strmemccpy(value, "Run", value_size);
            break;

        case IPODJS_KOKKIA_HELP:
            strmemccpy(value, "Open", value_size);
            break;

        case IPODJS_KOKKIA_PAUSE_UNPLUG:
            strmemccpy(value,
                       global_settings.kokkia_pause_on_unplug ?
                       "On" : "Off", value_size);
            break;

        default:
            value[0] = '\0';
            break;
    }
}

static void root_menu_video_draw_kokkia_status(
    int selected, const struct iap_connection_info *info)
{
    const int row_h = 21;
    const int start_y = IPODJS_HEADER_HEIGHT;
    unsigned list_bg = root_menu_video_row_bg();

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();
    root_menu_video_draw_status_title("Kokkia");
    lcd_set_foreground(list_bg);
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
    lcd_setfont(root_menu_video_font());

    for (int item = 0; item < IPODJS_KOKKIA_COUNT; item++)
    {
        char value[64];
        int y = start_y + item * row_h;
        int text_y = y + MAX(1,
            (row_h - font_get(root_menu_video_font())->height) / 2) +
            root_menu_video_text_y_offset();
        bool active = item == selected;
        bool action = item >= IPODJS_KOKKIA_RECONNECT;

        root_menu_video_kokkia_value(item, info, value, sizeof(value));
        root_menu_video_draw_qs_row(0, y, LCD_WIDTH, row_h, active, false);
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(active ? IPODJS_PREVIEW_TEXT :
                           root_menu_video_text());
        lcd_set_background(active ? root_menu_video_accent() : list_bg);
        root_menu_video_puts_fit(8, text_y, 150,
                                 root_menu_video_kokkia_labels[item], false);
        lcd_set_foreground(active ? LCD_RGBPACK(230, 245, 255) :
                           root_menu_video_muted_text());
        root_menu_video_puts_fit(158, text_y,
                                 action ? 142 : 152, value, true);
        if (active && action)
            root_menu_video_draw_arrow(LCD_WIDTH - 9,
                                       y + (row_h - 6) / 2);
    }

    if (selected == IPODJS_KOKKIA_RECONNECT)
        root_menu_video_draw_qs_footer_text(
            "Restarts dock handshake only");
    else if (selected == IPODJS_KOKKIA_HELP)
        root_menu_video_draw_qs_footer_text(
            "AirPods pairing and memory help");
    else if (selected == IPODJS_KOKKIA_PAUSE_UNPLUG)
        root_menu_video_draw_qs_footer_text(
            "Optional; disabled by default");
    else
        root_menu_video_draw_qs_footer_text(
            "Live dock diagnostics");

    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

static void root_menu_video_draw_kokkia_help(void)
{
    unsigned bg = root_menu_video_row_bg();

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();
    root_menu_video_draw_status_title("Pairing Help");
    lcd_set_foreground(bg);
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);
    lcd_setfont(root_menu_video_font());
    lcd_set_background(bg);
    lcd_set_foreground(root_menu_video_text());
    root_menu_video_puts_fit(12, 39, LCD_WIDTH - 24,
        "1. Keep Kokkia firmly seated.", false);
    root_menu_video_puts_fit(12, 68, LCD_WIDTH - 24,
        "2. Put AirPods in pairing mode.", false);
    root_menu_video_puts_fit(12, 97, LCD_WIDTH - 24,
        "3. Disable Bluetooth on devices", false);
    root_menu_video_puts_fit(25, 116, LCD_WIDTH - 37,
        "that may claim the AirPods.", false);
    root_menu_video_puts_fit(12, 145, LCD_WIDTH - 24,
        "4. Choose Reconnect Now.", false);
    lcd_set_foreground(root_menu_video_muted_text());
    root_menu_video_puts_fit(12, 174, LCD_WIDTH - 24,
        "Pair memory is stored by Kokkia.", false);
    root_menu_video_draw_qs_footer_text("Menu returns to Kokkia");
    lcd_update();
}

static void root_menu_video_kokkia_help(void)
{
    bool redraw = true;

    root_menu_wait_for_button_release();
    button_clear_queue();
    while (true)
    {
        int action;

        if (redraw)
        {
            root_menu_video_draw_kokkia_help();
            redraw = false;
        }
        action = get_action(CONTEXT_TREE, HZ/5);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        if (action == ACTION_STD_CANCEL || action == ACTION_STD_MENU ||
            action == ACTION_STD_OK)
            return;
    }
}

static void root_menu_video_kokkia_menu(void)
{
    struct iap_connection_info drawn_info;
    int selected = 0;
    bool redraw = true;
    bool held = button_hold();

    memset(&drawn_info, 0, sizeof(drawn_info));
    root_menu_wait_for_button_release();
    button_clear_queue();

    while (true)
    {
        struct iap_connection_info info;
        int action;

        iap_get_connection_info(&info);
        if (memcmp(&info, &drawn_info, sizeof(info)))
            redraw = true;
        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_kokkia_status(selected, &info);
            drawn_info = info;
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, HZ/5);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (!root_menu_video_hold_update(&held, &redraw))
                    selected = selected <= 0 ? IPODJS_KOKKIA_COUNT - 1 :
                               selected - 1;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (!root_menu_video_hold_update(&held, &redraw))
                    selected = selected >= IPODJS_KOKKIA_COUNT - 1 ? 0 :
                               selected + 1;
                redraw = true;
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (selected == IPODJS_KOKKIA_RECONNECT)
                {
                    splash(HZ, iap_restart_kokkia() ?
                           "Kokkia link restarting" :
                           "No serial accessory");
                    root_menu_wait_for_button_release();
                    button_clear_queue();
                    redraw = true;
                }
                else if (selected == IPODJS_KOKKIA_HELP)
                {
                    root_menu_video_kokkia_help();
                    redraw = true;
                }
                else if (selected == IPODJS_KOKKIA_PAUSE_UNPLUG)
                {
                    global_settings.kokkia_pause_on_unplug =
                        !global_settings.kokkia_pause_on_unplug;
                    settings_save();
                    redraw = true;
                }
                else if (selected == IPODJS_KOKKIA_BACK)
                    return;
                break;

            case ACTION_TREE_WPS:
                if (!root_menu_video_hold_update(&held, &redraw))
                    (void)root_menu_video_handle_play_pause(false, NULL);
                redraw = true;
                break;

            case ACTION_STD_CANCEL:
            case ACTION_STD_CONTEXT:
            case ACTION_STD_QUICKSCREEN:
            case ACTION_STD_MENU:
                if (!root_menu_video_hold_update(&held, &redraw))
                    return;
                break;
        }
    }
}
#endif

static int root_menu_video_quick_settings(void)
{
    int selected = 0;
    bool redraw = true;
    bool changed_settings = false;
    bool changed_status = false;
    bool adjusting = false;
    bool held = button_hold();
    long next_hold_refresh = 0;
#ifdef IPOD_ACCESSORY_PROTOCOL
    enum iap_connection_status last_kokkia_status = -1;
#endif

    root_menu_wait_for_button_release();
    button_clear_queue();

    while (true)
    {
        int action;

#ifdef IPOD_ACCESSORY_PROTOCOL
        if (iap_connection_status() != last_kokkia_status)
            redraw = true;
#endif
        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_quick_settings(selected, adjusting);
#ifdef IPOD_ACCESSORY_PROTOCOL
            last_kokkia_status = iap_connection_status();
#endif
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, HZ/8);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (adjusting)
                {
                    if (root_menu_video_qs_adjust(selected, -1))
                    {
                        changed_settings = changed_settings ||
                                           selected != IPODJS_QS_VOLUME;
                        changed_status = changed_status ||
                                         selected == IPODJS_QS_VOLUME;
                    }
                }
                else
                    selected = selected <= 0 ? IPODJS_QS_COUNT - 1 :
                               selected - 1;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (adjusting)
                {
                    if (root_menu_video_qs_adjust(selected, 1))
                    {
                        changed_settings = changed_settings ||
                                           selected != IPODJS_QS_VOLUME;
                        changed_status = changed_status ||
                                         selected == IPODJS_QS_VOLUME;
                    }
                }
                else
                    selected = selected >= IPODJS_QS_COUNT - 1 ? 0 :
                               selected + 1;
                redraw = true;
                break;

            case ACTION_TREE_PGLEFT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (root_menu_video_qs_adjust(selected, -1))
                {
                    changed_settings = changed_settings ||
                                       selected != IPODJS_QS_VOLUME;
                    changed_status = changed_status ||
                                     selected == IPODJS_QS_VOLUME;
                    redraw = true;
                }
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
#ifdef IPOD_ACCESSORY_PROTOCOL
                if (selected == IPODJS_QS_KOKKIA)
                {
                    root_menu_video_kokkia_menu();
                    redraw = true;
                    break;
                }
#endif
                if (selected == IPODJS_QS_CACHE_MEMORY)
                {
                    if (root_menu_video_cache_memory_menu(changed_settings,
                                                          changed_status))
                        return GO_TO_ROOT;
                    redraw = true;
                    break;
                }
                if (selected == IPODJS_QS_VOLUME ||
                    selected == IPODJS_QS_BRIGHTNESS)
                {
                    adjusting = !adjusting;
                    redraw = true;
                    break;
                }
                if (root_menu_video_qs_adjust(selected, 1))
                {
                    changed_settings = true;
                    redraw = true;
                }
                break;

            case ACTION_TREE_PGRIGHT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (root_menu_video_qs_adjust(selected, 1))
                {
                    changed_settings = changed_settings ||
                                       selected != IPODJS_QS_VOLUME;
                    changed_status = changed_status ||
                                     selected == IPODJS_QS_VOLUME;
                    redraw = true;
                }
                break;

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                (void)root_menu_video_handle_play_pause(false, NULL);
                if (changed_settings)
                    settings_save();
                if (changed_status)
                    status_save(false);
                redraw = true;
                break;

            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (adjusting)
                {
                    adjusting = false;
                    redraw = true;
                    break;
                }
                if (changed_settings)
                    settings_save();
                if (changed_status)
                    status_save(false);
                button_clear_queue();
                return GO_TO_ROOT;

            case ACTION_STD_CONTEXT:
            case ACTION_STD_QUICKSCREEN:
            case ACTION_STD_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (adjusting)
                {
                    adjusting = false;
                    redraw = true;
                    break;
                }
                if (changed_settings)
                    settings_save();
                if (changed_status)
                    status_save(false);
                button_clear_queue();
                return GO_TO_ROOT;
        }
    }
}

struct root_menu_video_settings_item {
    const char *label;
    const struct menu_item_ex *menu;
    int (*function)(void);
};

static int root_menu_video_main_menu_config(void)
{
    int ret;

    if (!global_settings.root_menu_customized)
        root_menu_set_default(&global_settings.root_menu_customized, NULL);

    ret = plugin_load(PLUGIN_APPS_DIR "/main_menu_config.rock", NULL);

    if (ret == PLUGIN_USB_CONNECTED)
        return MENU_ATTACHED_USB;

    return 0;
}

static const struct root_menu_video_settings_item
root_menu_video_settings_items[] = {
    { "Main Menu", NULL, root_menu_video_main_menu_config },
    { "Sound", &sound_settings, NULL },
    { "Playback", &playback_settings, NULL },
    { "General", &settings_menu_item, NULL },
    { "Themes", &theme_menu, NULL },
#ifdef HAVE_RECORDING
    { "Recording", &recording_settings, NULL },
#endif
    { "System", &system_menu, NULL },
    { "Manage Settings", &manage_settings, NULL },
    { "All Settings", &main_menu_, NULL },
};

static void root_menu_video_draw_settings_menu(int selected)
{
    int count = ARRAYLEN(root_menu_video_settings_items);
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int top = 0;
    int i;
    int x = IPODJS_PREVIEW_X;
    int y = 0;
    int w = LCD_WIDTH - x;
    int h = LCD_HEIGHT - y;

    if (selected >= visible)
        top = selected - visible + 1;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_status();

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_screen_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, IPODJS_LIST_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    for (i = 0; i < visible && top + i < count; i++)
    {
        int index = top + i;
        int item_y = IPODJS_HEADER_HEIGHT + i * row_h;
        bool active = index == selected;

        if (active)
        {
            unsigned accent = root_menu_video_accent();
            root_menu_video_selection_gradient(0, item_y, IPODJS_LIST_WIDTH,
                                               row_h);
            lcd_set_foreground(IPODJS_PREVIEW_TEXT);
            lcd_set_background(accent);
        }
        else
        {
            lcd_set_foreground(root_menu_video_row_bg());
            lcd_fillrect(0, item_y, IPODJS_LIST_WIDTH, row_h);
            lcd_set_foreground(root_menu_video_text());
            lcd_set_background(root_menu_video_row_bg());
        }

        root_menu_video_puts_fit(6, item_y + 4, IPODJS_LIST_WIDTH - 24,
                                 root_menu_video_settings_items[index].label,
                                 false);
        if (active)
            root_menu_video_draw_arrow(IPODJS_LIST_WIDTH - 13,
                                       item_y + (row_h - 6) / 2);
    }

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(54, 60, 70) : IPODJS_SPLIT);
    lcd_vline(IPODJS_SPLIT_X, 0, LCD_HEIGHT - 1);

    root_menu_video_draw_settings_preview(x, y, w, h, true);

    ipodjs_video_draw_hold_overlay();
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
    ipodjs_trace_screen("Settings", "full", selected, top, count,
                        0, 0, LCD_WIDTH, LCD_HEIGHT);
}

static int root_menu_video_settings_menu(void)
{
    int selected = 0;
    int previous_selected = 0;
    bool redraw = true;
    bool redraw_rows = false;
    bool held = button_hold();
    long next_hold_refresh = 0;

    root_menu_video_enter_native_screen();
    button_clear_queue();

    while (true)
    {
        int action;
        int count = ARRAYLEN(root_menu_video_settings_items);

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_settings_menu(selected);
            redraw = false;
            redraw_rows = false;
        }
        else if (redraw_rows)
        {
            if (!root_menu_video_draw_native_pane_delta(previous_selected,
                    selected, count,
                    root_menu_video_settings_items[previous_selected].label,
                    root_menu_video_settings_items[selected].label, true))
                root_menu_video_draw_settings_menu(selected);
            redraw_rows = false;
        }

        action = get_action(CONTEXT_TREE, HZ/20);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        if (root_menu_video_handle_tree_stop(action, &redraw))
            continue;
        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MAX(0, selected - 1);
                redraw_rows = previous_selected != selected;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MIN(count - 1, selected + 1);
                redraw_rows = previous_selected != selected;
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
            {
                int ret;
                ipodjs_ui_transition_begin(1);
                root_menu_video_finish_native_screen(0);
                if (root_menu_video_settings_items[selected].function)
                    ret = root_menu_video_settings_items[selected].function();
                else
                    ret = do_menu(root_menu_video_settings_items[selected].menu,
                                  NULL, NULL, true);
                root_menu_video_enter_native_screen();
                if (ret == MENU_ATTACHED_USB)
                    return root_menu_video_finish_native_screen(ret);
                ipodjs_ui_transition_begin(-1);
                redraw = true;
                break;
            }

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
            {
                bool started_playback = false;

                if (root_menu_video_handle_play_pause(true,
                                                     &started_playback))
                    return root_menu_video_finish_native_screen(GO_TO_WPS);
                redraw = true;
                break;
            }

            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_ROOT);
        }
    }
}

static bool root_menu_video_preview_switch_is_immediate(int old_index,
                                                        int new_index)
{
    const char *old_label =
        root_menu_video_label(root_menu_video_item(old_index));
    const char *new_label =
        root_menu_video_label(root_menu_video_item(new_index));

    /* The Netflix pane is a static brand card. Clear or reveal it on the
     * same wheel event; media preview I/O remains deferred and cache-only
     * drawing can fill the replacement pane immediately. */
    return !strcmp(old_label, "Netflix") || !strcmp(new_label, "Netflix");
}

#define NOTIFICATION_IOS5_ASSET_DIR ROCKBOX_DIR \
    "/ipodjs/notifications"
#define NOTIFICATION_CENTER_LINEN_W 32
#define NOTIFICATION_CENTER_LINEN_H 32
#define NOTIFICATION_SECTION_W 32
#define NOTIFICATION_SECTION_H 24
#define NOTIFICATION_CLOSE_W 22
#define NOTIFICATION_CLOSE_H 22
#define NOTIFICATION_SOURCE_ICON_W 22
#define NOTIFICATION_SOURCE_ICON_H 22

static struct {
    struct bitmap linen;
    struct bitmap section;
    struct bitmap close;
    struct bitmap achievement;
    struct bitmap sitekick;
    unsigned char linen_data[
        BM_SIZE(NOTIFICATION_CENTER_LINEN_W,
                NOTIFICATION_CENTER_LINEN_H, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    unsigned char section_data[
        BM_SIZE(NOTIFICATION_SECTION_W,
                NOTIFICATION_SECTION_H, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    unsigned char close_data[
        BM_SIZE(NOTIFICATION_CLOSE_W,
                NOTIFICATION_CLOSE_H, FORMAT_NATIVE, false)] IPODJS_BM_ALIGN;
    unsigned char achievement_data[
        BM_SIZE(NOTIFICATION_SOURCE_ICON_W, NOTIFICATION_SOURCE_ICON_H,
                FORMAT_NATIVE, false) +
        NOTIFICATION_SOURCE_ICON_W * NOTIFICATION_SOURCE_ICON_H / 2] IPODJS_BM_ALIGN;
    unsigned char sitekick_data[
        BM_SIZE(NOTIFICATION_SOURCE_ICON_W, NOTIFICATION_SOURCE_ICON_H,
                FORMAT_NATIVE, false) +
        NOTIFICATION_SOURCE_ICON_W * NOTIFICATION_SOURCE_ICON_H / 2] IPODJS_BM_ALIGN;
    bool prepared;
    bool valid;
} root_menu_video_notification_assets;

static bool root_menu_video_load_notification_asset(const char *path,
                                                     struct bitmap *bm,
                                                     unsigned char *data,
                                                     size_t data_size,
                                                     int width, int height)
{
    int rc;

    if (!file_exists(path))
        return false;
    memset(bm, 0, sizeof(*bm));
    bm->width = width;
    bm->height = height;
    bm->format = FORMAT_NATIVE;
    bm->data = data;
    rc = read_bmp_file(path, bm, data_size,
                       FORMAT_NATIVE | FORMAT_DITHER | FORMAT_TRANSPARENT,
                       NULL);
    return rc >= 0 && bm->width == width && bm->height == height;
}

static void root_menu_video_prepare_notification_assets(void)
{
    char path[MAX_PATH];

    if (root_menu_video_notification_assets.prepared)
        return;
    root_menu_video_notification_assets.prepared = true;

    snprintf(path, sizeof(path), "%s/%s", NOTIFICATION_IOS5_ASSET_DIR,
             "notification-center-linen.ios5.32x32x24.bmp");
    root_menu_video_notification_assets.valid =
        root_menu_video_load_notification_asset(path,
            &root_menu_video_notification_assets.linen,
            root_menu_video_notification_assets.linen_data,
            sizeof(root_menu_video_notification_assets.linen_data),
            NOTIFICATION_CENTER_LINEN_W, NOTIFICATION_CENTER_LINEN_H);
    snprintf(path, sizeof(path), "%s/%s", NOTIFICATION_IOS5_ASSET_DIR,
             "notification-section.ios5.32x24x24.bmp");
    root_menu_video_notification_assets.valid &=
        root_menu_video_load_notification_asset(path,
            &root_menu_video_notification_assets.section,
            root_menu_video_notification_assets.section_data,
            sizeof(root_menu_video_notification_assets.section_data),
            NOTIFICATION_SECTION_W, NOTIFICATION_SECTION_H);
    snprintf(path, sizeof(path), "%s/%s", NOTIFICATION_IOS5_ASSET_DIR,
             "notification-close.ios5.22x22x24.bmp");
    root_menu_video_notification_assets.valid &=
        root_menu_video_load_notification_asset(path,
            &root_menu_video_notification_assets.close,
            root_menu_video_notification_assets.close_data,
            sizeof(root_menu_video_notification_assets.close_data),
            NOTIFICATION_CLOSE_W, NOTIFICATION_CLOSE_H);

    root_menu_video_load_notification_asset(
        NOTIFICATION_IOS5_ASSET_DIR "/achievement-icon.22x22x24.bmp",
        &root_menu_video_notification_assets.achievement,
        root_menu_video_notification_assets.achievement_data,
        sizeof(root_menu_video_notification_assets.achievement_data),
        NOTIFICATION_SOURCE_ICON_W, NOTIFICATION_SOURCE_ICON_H);
    root_menu_video_load_notification_asset(
        NOTIFICATION_IOS5_ASSET_DIR "/sitekick-icon.22x22x24.bmp",
        &root_menu_video_notification_assets.sitekick,
        root_menu_video_notification_assets.sitekick_data,
        sizeof(root_menu_video_notification_assets.sitekick_data),
        NOTIFICATION_SOURCE_ICON_W, NOTIFICATION_SOURCE_ICON_H);
    notification_manager_prepare_visuals();
}

static void root_menu_video_tile_notification_asset(struct bitmap *bm,
                                                    int x, int y,
                                                    int width, int height)
{
    int dx;
    int dy;

    for (dy = 0; dy < height; dy += bm->height)
    {
        int draw_h = MIN(bm->height, height - dy);

        for (dx = 0; dx < width; dx += bm->width)
        {
            int draw_w = MIN(bm->width, width - dx);

            lcd_bmp_part(bm, 0, 0, x + dx, y + dy, draw_w, draw_h);
        }
    }
}

static void root_menu_video_draw_notification_source_icon(unsigned source,
                                                          int x, int y)
{
    struct bitmap *bm = NULL;

    if (source == NOTIFICATION_SOURCE_ACHIEVEMENTS)
        bm = &root_menu_video_notification_assets.achievement;
    else if (source == NOTIFICATION_SOURCE_SITEKICK)
        bm = &root_menu_video_notification_assets.sitekick;

    if (bm && bm->data && bm->width > 0 && bm->height > 0)
        lcd_bmp(bm, x, y);
    else if (source == NOTIFICATION_SOURCE_MUSIC)
    {
        lcd_set_drawmode(DRMODE_SOLID);
        lcd_set_foreground(LCD_WHITE);
        lcd_fillrect(x + 13, y + 3, 3, 13);
        lcd_hline(x + 7, x + 15, y + 3);
        lcd_fillrect(x + 5, y + 14, 8, 6);
        lcd_fillrect(x + 11, y + 12, 8, 6);
    }
    else if (source == NOTIFICATION_SOURCE_LIVETV)
    {
        lcd_set_drawmode(DRMODE_SOLID);
        lcd_set_foreground(LCD_WHITE);
        lcd_drawrect(x + 2, y + 4, 19, 14);
        lcd_hline(x + 8, x + 14, y + 20);
        lcd_vline(x + 11, y + 18, y + 20);
    }
    else if (source == NOTIFICATION_SOURCE_WEATHER)
    {
        lcd_set_drawmode(DRMODE_SOLID);
        lcd_set_foreground(LCD_WHITE);
        lcd_fillrect(x + 4, y + 11, 15, 7);
        lcd_fillrect(x + 8, y + 7, 8, 9);
        lcd_fillrect(x + 2, y + 13, 19, 4);
    }
    else if (source == NOTIFICATION_SOURCE_BATTERY)
    {
        lcd_set_drawmode(DRMODE_SOLID);
        lcd_set_foreground(LCD_WHITE);
        lcd_drawrect(x + 2, y + 5, 18, 12);
        lcd_fillrect(x + 20, y + 9, 2, 4);
        lcd_fillrect(x + 5, y + 8, 5, 6);
    }
    else if (source == NOTIFICATION_SOURCE_STORAGE)
    {
        lcd_set_drawmode(DRMODE_SOLID);
        lcd_set_foreground(LCD_WHITE);
        lcd_hline(x + 3, x + 19, y + 5);
        lcd_hline(x + 1, x + 21, y + 8);
        lcd_hline(x + 1, x + 21, y + 17);
        lcd_vline(x + 1, y + 8, y + 17);
        lcd_vline(x + 21, y + 8, y + 17);
        lcd_hline(x + 5, x + 17, y + 13);
    }
}

static const char *notification_source_label(unsigned source)
{
    if (source == NOTIFICATION_SOURCE_ACHIEVEMENTS)
        return "Achievements";
    if (source == NOTIFICATION_SOURCE_SITEKICK)
        return "Sitekick";
    if (source == NOTIFICATION_SOURCE_MUSIC)
        return "Music";
    if (source == NOTIFICATION_SOURCE_LIVETV)
        return "DIRECTV";
    if (source == NOTIFICATION_SOURCE_WEATHER)
        return "Weather";
    if (source == NOTIFICATION_SOURCE_BATTERY)
        return "Battery";
    if (source == NOTIFICATION_SOURCE_STORAGE)
        return "Storage";
    return "iPod";
}

static void root_menu_video_notification_age(char *buf, size_t size,
                                             long timestamp)
{
    long now;
    long age;

#if CONFIG_RTC
    now = (long)mktime(get_time());
#else
    now = current_tick / HZ;
#endif
    age = timestamp > 0 && now > timestamp ? now - timestamp : 0;
    if (age < 60)
        strmemccpy(buf, "now", size);
    else if (age < 60 * 60)
        snprintf(buf, size, "%ldm", age / 60);
    else if (age < 24 * 60 * 60)
        snprintf(buf, size, "%ldh", age / (60 * 60));
    else
        snprintf(buf, size, "%ldd", age / (24 * 60 * 60));
}

static void root_menu_video_draw_notification_dot(int x, int y,
                                                  bool unread)
{
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_foreground(unread ? LCD_RGBPACK(38, 151, 244) :
                                LCD_RGBPACK(101, 106, 113));
    lcd_hline(x + 2, x + 5, y);
    lcd_hline(x, x + 7, y + 2);
    lcd_fillrect(x, y + 3, 8, 3);
    lcd_hline(x + 1, x + 6, y + 6);
    lcd_hline(x + 2, x + 5, y + 7);
}

static void root_menu_video_draw_notification_grabber(void)
{
    const int x = LCD_WIDTH / 2 - 23;
    const int y = LCD_HEIGHT - 9;

    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_foreground(LCD_RGBPACK(8, 9, 10));
    lcd_fillrect(x + 3, y - 2, 40, 10);
    lcd_fillrect(x, y + 1, 46, 5);
    lcd_set_foreground(LCD_RGBPACK(108, 111, 116));
    lcd_hline(x + 4, x + 41, y);
    lcd_hline(x + 2, x + 43, y + 3);
    lcd_hline(x + 4, x + 41, y + 6);
}

static void root_menu_video_draw_notification_center(int selected)
{
    const int top_h = 24;
    const int section_h = 28;
    const int item_h = 60;
    const int bottom_h = 12;
    const int block_h = section_h + item_h;
    const int visible = MAX(1,
        (LCD_HEIGHT - top_h - bottom_h) / block_h);
    int count = notification_manager_count();
    int top = selected >= visible ? selected - visible + 1 : 0;
    int bold_font = notification_manager_visual_font(true);
    int regular_font = notification_manager_visual_font(false);
    int bold_h = font_get(bold_font)->height;
    int row;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    if (root_menu_video_notification_assets.valid)
        root_menu_video_tile_notification_asset(
            &root_menu_video_notification_assets.linen,
            0, 0, LCD_WIDTH, LCD_HEIGHT);
    else
        root_menu_video_gradient(0, 0, LCD_WIDTH, LCD_HEIGHT,
                                 LCD_RGBPACK(70, 75, 82),
                                 LCD_RGBPACK(25, 28, 33));
    if (root_menu_video_notification_assets.valid)
        root_menu_video_tile_notification_asset(
            &root_menu_video_notification_assets.section,
            0, 0, LCD_WIDTH, top_h);
    else
        root_menu_video_gradient(0, 0, LCD_WIDTH, top_h,
                                 LCD_RGBPACK(75, 80, 88),
                                 LCD_RGBPACK(27, 30, 35));
    lcd_set_foreground(LCD_WHITE);
    lcd_set_drawmode(DRMODE_FG);
    lcd_setfont(bold_font);
    root_menu_video_puts_fit(42, (top_h - bold_h) / 2,
                             LCD_WIDTH - 84, "Notifications", true);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_foreground(LCD_RGBPACK(11, 12, 14));
    lcd_hline(0, LCD_WIDTH - 1, top_h - 1);

    if (count == 0)
    {
        const int panel_y = 88;

        root_menu_video_gradient(18, panel_y, LCD_WIDTH - 36, 58,
                                 LCD_RGBPACK(53, 57, 64),
                                 LCD_RGBPACK(23, 26, 31));
        lcd_set_foreground(LCD_RGBPACK(8, 9, 11));
        lcd_drawrect(18, panel_y, LCD_WIDTH - 36, 58);
        lcd_setfont(bold_font);
        lcd_set_drawmode(DRMODE_FG);
        lcd_set_foreground(LCD_WHITE);
        root_menu_video_puts_fit(30, panel_y + 10,
                                 LCD_WIDTH - 60, "No Notifications", true);
        lcd_setfont(regular_font);
        lcd_set_foreground(LCD_RGBPACK(194, 197, 202));
        root_menu_video_puts_fit(30, panel_y + 34,
                                 LCD_WIDTH - 60, "You're all caught up.",
                                 true);
    }
    for (row = 0; row < visible && top + row < count; ++row)
    {
        struct notification_record record;
        int index = top + row;
        int y = top_h + row * block_h;
        int content_y = y + section_h;
        bool highlighted = index == selected;
        char age[12];
        int age_w;

        if (!notification_manager_get(index, &record))
            continue;
        if (root_menu_video_notification_assets.valid)
            root_menu_video_tile_notification_asset(
                &root_menu_video_notification_assets.section,
                0, y, LCD_WIDTH, section_h);
        else
            root_menu_video_gradient(0, y, LCD_WIDTH, section_h,
                                     LCD_RGBPACK(66, 71, 78),
                                     LCD_RGBPACK(27, 30, 35));
        root_menu_video_draw_notification_source_icon(
            record.request.source, 6,
            y + (section_h - NOTIFICATION_SOURCE_ICON_H) / 2);
        lcd_set_drawmode(DRMODE_FG);
        lcd_setfont(bold_font);
        lcd_set_foreground(LCD_WHITE);
        root_menu_video_puts_fit(34, y + (section_h - bold_h) / 2,
                                 LCD_WIDTH - 70,
                                 notification_source_label(
                                     record.request.source), false);
        if (root_menu_video_notification_assets.valid)
            lcd_bmp(&root_menu_video_notification_assets.close,
                    LCD_WIDTH - 24,
                    y + (section_h - NOTIFICATION_CLOSE_H) / 2);

        if (highlighted)
        {
            root_menu_video_gradient(5, content_y + 3,
                                     LCD_WIDTH - 10, item_h - 6,
                                     LCD_RGBPACK(64, 70, 80),
                                     LCD_RGBPACK(31, 35, 42));
            lcd_set_foreground(LCD_RGBPACK(8, 9, 11));
            lcd_drawrect(5, content_y + 3,
                         LCD_WIDTH - 10, item_h - 6);
            lcd_set_foreground(LCD_RGBPACK(105, 110, 119));
            lcd_hline(7, LCD_WIDTH - 8, content_y + 4);
        }

        root_menu_video_draw_notification_dot(
            21, content_y + (item_h - 8) / 2, !record.read);
        root_menu_video_notification_age(age, sizeof(age),
                                         record.request.timestamp);
        font_getstringsize(age, &age_w, NULL, bold_font);
        lcd_set_drawmode(DRMODE_FG);
        lcd_setfont(bold_font);
        lcd_set_foreground(LCD_WHITE);
        root_menu_video_puts_fit(42, content_y + 8,
                                 LCD_WIDTH - 58 - age_w,
                                 record.request.title, false);
        root_menu_video_puts_fit(LCD_WIDTH - age_w - 9,
                                 content_y + 8, age_w, age, false);
        lcd_setfont(regular_font);
        lcd_set_foreground(highlighted ? LCD_RGBPACK(232, 233, 236) :
                                         LCD_RGBPACK(210, 213, 218));
        root_menu_video_puts_fit(42,
                                 content_y + 12 + bold_h,
                                 LCD_WIDTH - 54,
                                 record.request.body, false);
        lcd_set_drawmode(DRMODE_SOLID);
        lcd_set_foreground(LCD_RGBPACK(18, 20, 23));
        lcd_hline(0, LCD_WIDTH - 1, content_y + item_h - 1);
    }
    root_menu_video_draw_notification_grabber();
    if (!ipodjs_ui_transition_present(&screens[SCREEN_MAIN]))
        lcd_update();
}

static int root_menu_video_launch_notification(
    const struct notification_record *record)
{
    int ret = 0;

    if (record->request.source == NOTIFICATION_SOURCE_MUSIC)
        return GO_TO_WPS;

    root_menu_video_finish_native_screen(0);
    if (record->request.source == NOTIFICATION_SOURCE_ACHIEVEMENTS)
        ret = launch_achievements_plugin(NULL);
    else if (record->request.source == NOTIFICATION_SOURCE_SITEKICK)
        ret = launch_sitekick_plugin(record->request.route[0] ?
                                     (void *)record->request.route : NULL);
    else if (record->request.source == NOTIFICATION_SOURCE_LIVETV)
        ret = launch_livetv_plugin(NULL);
    else if (record->request.source == NOTIFICATION_SOURCE_WEATHER)
        ret = launch_weather_plugin(NULL);
    root_menu_video_enter_native_screen();
    return ret;
}

static int root_menu_video_notification_center(void)
{
    int selected = 0;
    bool redraw = true;

    root_menu_video_prepare_notification_assets();
    notification_manager_set_center_active(true);
    button_clear_queue();
    while (true)
    {
        int count = notification_manager_count();
        int action;

        if (count > 0)
            selected = MAX(0, MIN(selected, count - 1));
        else
            selected = 0;
        if (redraw)
        {
            root_menu_video_draw_notification_center(selected);
            redraw = false;
        }
        action = get_action(CONTEXT_TREE|ALLOW_SOFTLOCK, HZ / 5);
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
        switch (action)
        {
        case ACTION_STD_PREV:
        case ACTION_STD_PREVREPEAT:
            if (selected > 0)
            {
                selected--;
                redraw = true;
            }
            break;
        case ACTION_STD_NEXT:
        case ACTION_STD_NEXTREPEAT:
            if (selected + 1 < count)
            {
                selected++;
                redraw = true;
            }
            break;
        case ACTION_STD_OK:
            if (count > 0)
            {
                struct notification_record record;

                if (notification_manager_get(selected, &record))
                {
                    int launch_result;

                    notification_manager_mark_read(selected);
                    launch_result =
                        root_menu_video_launch_notification(&record);
                    if (launch_result == GO_TO_WPS)
                    {
                        notification_manager_set_center_active(false);
                        button_clear_queue();
                        return GO_TO_WPS;
                    }
                    redraw = true;
                }
            }
            break;
        case ACTION_STD_CONTEXT:
            if (count > 0 && yesno_pop("Clear all notifications?"))
            {
                notification_manager_clear();
                selected = 0;
                redraw = true;
            }
            break;
        case ACTION_STD_CANCEL:
            notification_manager_set_center_active(false);
            button_clear_queue();
            return 0;
        case ACTION_STD_MENU:
            notification_manager_set_center_active(false);
            button_clear_queue();
            return 0;
        default:
            if (default_event_handler(action) == SYS_USB_CONNECTED)
            {
                notification_manager_set_center_active(false);
                return MENU_ATTACHED_USB;
            }
            break;
        }
    }
}

static int root_menu_video_present_notification_center(void)
{
    int result;

    ipodjs_ui_transition_begin_vertical(1);
    result = root_menu_video_notification_center();
    if (result == 0)
        ipodjs_ui_transition_begin_vertical(-1);
    button_clear_queue();
    return result;
}

static int root_menu_video_dashboard(int *selectedp)
{
    int selected = MAX(0, MIN(*selectedp, root_menu_video_count() - 1));
    int preview_selected = selected;
    int previous_selected = selected;
    int drawn_selected = -1;
    bool redraw = true;
    bool redraw_list_only = false;
    bool held = button_hold();
    bool last_paused = false;
    bool last_bluetooth = false;
    bool last_internet = false;
    bool play_hold_stopped = false;
    long next_slideshow = 0;
    long preview_settle_tick = 0;
    long preview_io_settle_tick = current_tick + HZ / 2;
    long next_hold_refresh = 0;
    bool left_toggle_armed = true;
    bool preview_pending = false;
    struct root_menu_video_list_draw_state draw_state = {
        .valid = false
    };

    root_menu_video_enter_native_screen();
    root_menu_video_prepare_netflix_logo();
    root_menu_video_prepare_sitekick_preview();
    root_menu_video_prepare_notification_assets();
    button_clear_queue();

    while (true)
    {
        int action;
        int count = root_menu_video_count();
        bool paused;
        bool bluetooth = false;
        bool internet;
        bool poweroff_hold;
        const struct menu_item_ex *item;
        enum root_menu_video_preview_source preview_source;

        notification_manager_service();
        internet = usb_internet_connected();

#ifdef IPOD_ACCESSORY_PROTOCOL
        bluetooth = iap_kokkia_present();
#endif

        if (count <= 0)
            return root_menu_video_finish_native_screen(GO_TO_ROOT);

        selected = MAX(0, MIN(selected, count - 1));
        root_menu_video_hold_update(&held, &redraw);
        poweroff_hold = (button_status() & BUTTON_PLAY) != 0;
        if (!poweroff_hold && play_hold_stopped)
            redraw = true;
        if (poweroff_hold)
        {
            /*
             * Play-down has no mapped action until it repeats or is
             * released. Freeze the exact dashboard frame during that gap so
             * previews and playback-state redraws cannot leak into the
             * software-poweroff countdown.
             */
            redraw = false;
            redraw_list_only = false;
            preview_pending = false;
        }
        paused = root_menu_video_paused_playback();
        if (!poweroff_hold && paused != last_paused)
        {
            last_paused = paused;
            redraw = true;
        }
        if (!poweroff_hold && selected != drawn_selected && !redraw_list_only)
            redraw = true;

        if (!poweroff_hold && (bluetooth != last_bluetooth ||
                              internet != last_internet))
        {
            last_bluetooth = bluetooth;
            last_internet = internet;
            if (!redraw && drawn_selected >= 0 && !held)
            {
                /* iAP state changes arrive independently of button events.
                 * Refresh only the left-pane status bar so plugging or
                 * unplugging Kokkia updates immediately without disturbing
                 * the menu, preview, animation, or playback surfaces. */
                lcd_set_viewport(NULL);
                lcd_set_drawmode(DRMODE_SOLID);
                root_menu_video_draw_status();
                lcd_update_rect(0, 0, IPODJS_LIST_WIDTH,
                                IPODJS_HEADER_HEIGHT);
                ipodjs_trace_screen("Home", "status", selected,
                                    draw_state.valid ? draw_state.top : 0,
                                    count, 0, 0, IPODJS_LIST_WIDTH,
                                    IPODJS_HEADER_HEIGHT);
            }
        }

        if (redraw)
        {
            root_menu_video_draw_home(selected, preview_selected,
                                      &draw_state);
            drawn_selected = selected;
            redraw = false;
            redraw_list_only = false;
        }
        else if (redraw_list_only)
        {
            if (!root_menu_video_draw_home_selection_delta(previous_selected,
                    selected, &draw_state))
            {
                root_menu_video_draw_home(selected, preview_selected,
                                          &draw_state);
            }
            drawn_selected = selected;
            redraw_list_only = false;
        }

        item = root_menu_video_item(selected);
        action = get_action(CONTEXT_TREE|ALLOW_SOFTLOCK,
                            paused ? HZ/5 : HZ/20);
        if (button_status() & BUTTON_LEFT)
            left_toggle_armed = true;
        if (ipodjs_ui_handle_system_event(action, &redraw))
            continue;
#ifdef IPOD_ACCESSORY_PROTOCOL
        if (action == ACTION_NONE && !button_hold() &&
            iap_take_kokkia_connection_event())
        {
            ipodjs_ui_airpods_connected_animation();
            redraw = true;
            redraw_list_only = false;
            continue;
        }
#endif
        poweroff_hold = (button_status() & BUTTON_PLAY) != 0;
        if (poweroff_hold)
        {
            if (action == ACTION_TREE_STOP && !play_hold_stopped)
            {
                if (audio_status())
                    audio_stop();
                play_hold_stopped = true;
            }
            redraw = false;
            redraw_list_only = false;
            preview_pending = false;
            continue;
        }
        if (play_hold_stopped)
        {
            /* Consume the release that ends a medium Play hold. */
            play_hold_stopped = false;
            redraw = true;
            continue;
        }
        if (action != ACTION_NONE && !IS_SYSEVENT(action))
        {
            storage_spin();
            preview_io_settle_tick = current_tick + HZ / 2;
        }
        switch (action)
        {
            case ACTION_NONE:
                if (preview_pending &&
                    TIME_AFTER(current_tick, preview_settle_tick))
                {
                    preview_selected = selected;
                    preview_pending = false;
                    next_slideshow = current_tick +
                                     IPODJS_SLIDESHOW_DELAY;
                    preview_io_settle_tick = current_tick + HZ / 2;
                    if (global_settings.ui_engine_extras_pane ==
                            UI_ENGINE_EXTRAS_SITEKICK &&
                        !strcmp(root_menu_video_label(
                                    root_menu_video_item(preview_selected)),
                                "Extras"))
                        root_menu_video_prepare_sitekick_preview();
                    root_menu_video_draw_home_preview_only(preview_selected,
                                                           true);
                    break;
                }

                preview_source = root_menu_video_preview_source_for_item(
                    root_menu_video_item(preview_selected));
                if (!held &&
                    TIME_AFTER(current_tick, preview_io_settle_tick))
                {
                    bool loaded = false;

                    if (preview_source == IPODJS_PREVIEW_MUSIC)
                        loaded = albumlist_slideshow_service();
                    else if (preview_source > IPODJS_PREVIEW_MUSIC)
                        loaded = root_menu_video_preview_service(
                            preview_source);
                    else
                    {
                        const char *title = root_menu_video_preview(
                            root_menu_video_item(preview_selected));

                        loaded = root_menu_video_menu_preview_service(title);
                    }

                    if (loaded)
                        root_menu_video_draw_home_preview_only(
                            preview_selected, true);
                }

                if (!held &&
                    root_menu_video_sitekick_animation_due(
                        root_menu_video_label(
                            root_menu_video_item(preview_selected)),
                        &next_slideshow))
                {
                    root_menu_video_draw_home_preview_only(preview_selected,
                                                           false);
                    break;
                }

                if (!held &&
                    root_menu_video_custom_preview_animation_due(
                        root_menu_video_preview(
                            root_menu_video_item(preview_selected)),
                        &next_slideshow))
                {
                    root_menu_video_draw_home_preview_only(preview_selected,
                                                           false);
                    break;
                }

                if (paused)
                    break;

                if (held && !(audio_status() & AUDIO_STATUS_PAUSE) &&
                    TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                else if (!held &&
                         root_menu_video_should_animate(preview_source,
                                                        &next_slideshow))
                {
                    bool changed = preview_source == IPODJS_PREVIEW_MUSIC ?
                        albumlist_slideshow_frame_changed() :
                        root_menu_video_source_slideshow_frame_changed(
                            preview_source);
                    root_menu_video_draw_home_preview_only(preview_selected,
                                                           changed);
                }
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MAX(0, selected - 1);
                if (selected != previous_selected)
                {
                    if (root_menu_video_preview_switch_is_immediate(
                            preview_selected, selected))
                    {
                        preview_selected = selected;
                        preview_pending = false;
                        next_slideshow = current_tick +
                                         IPODJS_SLIDESHOW_DELAY;
                        root_menu_video_draw_home_preview_only(
                            preview_selected, false);
                    }
                    else
                    {
                        preview_pending = selected != preview_selected;
                        preview_settle_tick = current_tick +
                            IPODJS_ROOT_PREVIEW_SETTLE_DELAY;
                    }
                }
                if (draw_state.valid &&
                    root_menu_video_list_top(selected,
                        draw_state.visible) == draw_state.top)
                    redraw_list_only = true;
                else
                    redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                previous_selected = selected;
                selected = MIN(count - 1, selected + 1);
                if (selected != previous_selected)
                {
                    if (root_menu_video_preview_switch_is_immediate(
                            preview_selected, selected))
                    {
                        preview_selected = selected;
                        preview_pending = false;
                        next_slideshow = current_tick +
                                         IPODJS_SLIDESHOW_DELAY;
                        root_menu_video_draw_home_preview_only(
                            preview_selected, false);
                    }
                    else
                    {
                        preview_pending = selected != preview_selected;
                        preview_settle_tick = current_tick +
                            IPODJS_ROOT_PREVIEW_SETTLE_DELAY;
                    }
                }
                if (draw_state.valid &&
                    root_menu_video_list_top(selected,
                        draw_state.visible) == draw_state.top)
                    redraw_list_only = true;
                else
                    redraw = true;
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                *selectedp = selected;
            {
                int ret;

                ipodjs_ui_transition_begin(1);
                ret = root_menu_video_launch_menu_item(item);
                if (ret == GO_TO_ROOT || ret == GO_TO_PREVIOUS)
                    ipodjs_ui_transition_begin(-1);
                return root_menu_video_finish_native_screen(ret);
            }

            case ACTION_STD_CONTEXT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (button_status() & BUTTON_MENU)
                {
                    ipodjs_ui_transition_begin(1);
                    int ret = root_menu_video_quick_settings();
                    if (ret == GO_TO_WPS)
                    {
                        *selectedp = selected;
                        return root_menu_video_finish_native_screen(ret);
                    }
                    ipodjs_ui_transition_begin(-1);
                    redraw = true;
                    break;
                }
#ifdef HAVE_TAGCACHE
                if (item == &db_browser)
                {
                    int ret;

                    root_menu_video_finish_native_screen(0);
                    ret = launch_lrcplayer_plugin(NULL);
                    root_menu_video_enter_native_screen();
                    if (ret == GO_TO_ROOT || ret == GO_TO_WPS)
                    {
                        *selectedp = selected;
                        return root_menu_video_finish_native_screen(ret);
                    }
                    redraw = true;
                    break;
                }
#endif
                *selectedp = selected;
                ipodjs_ui_transition_begin(1);
                return root_menu_video_finish_native_screen(
                    GO_TO_ROOTITEM_CONTEXT);

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                *selectedp = selected;
            {
                bool started_playback = false;

                if (root_menu_video_handle_play_pause(true,
                                                     &started_playback))
                {
                    return root_menu_video_finish_native_screen(GO_TO_WPS);
                }
                redraw = true;
                break;
            }

            case ACTION_TREE_STOP:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                /*
                 * A repeat can escape just as raw Play is released. Preserve
                 * the same medium-hold Stop behavior in that race without
                 * entering the WPS.
                 */
                root_menu_video_handle_tree_stop(action, &redraw);
                break;

            case ACTION_TREE_POWER_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                root_menu_open_power_menu();
                redraw = true;
                button_clear_queue();
                break;

            case ACTION_STD_QUICKSCREEN:
            {
                int ret;

                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                ipodjs_ui_transition_begin(1);
                ret = root_menu_video_quick_settings();
                if (ret == GO_TO_WPS)
                {
                    *selectedp = selected;
                    return root_menu_video_finish_native_screen(ret);
                }
                ipodjs_ui_transition_begin(-1);
                redraw = true;
                break;
            }

            case ACTION_STD_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                /* Notification Center is intentionally Left-toggle only. */
                break;

            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (!left_toggle_armed)
                {
                    button_clear_queue();
                    break;
                }
                left_toggle_armed = false;
                {
                    int notification_result =
                        root_menu_video_present_notification_center();

                    if (notification_result == MENU_ATTACHED_USB)
                        return root_menu_video_finish_native_screen(
                            MENU_ATTACHED_USB);
                    if (notification_result == GO_TO_WPS)
                        return root_menu_video_finish_native_screen(
                            GO_TO_WPS);
                    redraw = true;
                    redraw_list_only = false;
                }
                break;
        }
    }
}
#endif /* HAVE_IPODJS_UI */

#ifndef HAVE_IPODJS_UI
bool root_menu_ipodjs_native_screen_active(void)
{
    return false;
}

bool root_menu_ipodjs_handle_lockscreen(void)
{
    return false;
}
#endif

static int get_selection(int last_screen)
{
    int i;
    int len = MENU_GET_COUNT(root_menu_.flags);

    /* root_menu__ is fixed-capacity storage; only the count encoded in the
     * menu flags is initialized. Screens such as the WPS playlist viewer do
     * not have a root item of their own, so scan only configured entries. */
    for(i=0; i < len; i++)
    {
        if (((root_menu__[i]->flags&MENU_TYPE_MASK) == MT_RETURN_VALUE) &&
            (root_menu__[i]->value == last_screen))
        {
            return i;
        }
    }
    return 0;
}

static inline int load_screen(int screen)
{
    /* set the global_status.last_screen before entering,
        if we dont we will always return to the wrong screen on boot */
    int old_previous = last_screen;
    int ret_val;
    enum current_activity activity = ACTIVITY_UNKNOWN;
    if (screen <= GO_TO_ROOT)
        return screen;
    if (screen == old_previous)
        old_previous = GO_TO_ROOT;
    global_status.last_screen = (char)screen;
    status_save(false);

    if (screen == GO_TO_BROWSEPLUGINS)
        activity = ACTIVITY_PLUGINBROWSER;
    else if (screen == GO_TO_MAINMENU)
        activity = ACTIVITY_SETTINGS;
    else if (screen == GO_TO_SYSTEM_SCREEN)
        activity =  ACTIVITY_SYSTEMSCREEN;

    if (activity != ACTIVITY_UNKNOWN)
        push_current_activity(activity);

    ret_val = items[screen].function(items[screen].param);

    if (activity != ACTIVITY_UNKNOWN)
    {
        if (ret_val == GO_TO_PLUGIN
            || ret_val == GO_TO_WPS
            || ret_val == GO_TO_PREVIOUS_MUSIC
            || ret_val == GO_TO_PREVIOUS_BROWSER
            || ret_val == GO_TO_FILEBROWSER)
        {
            pop_current_activity_without_refresh();
        }
        else
            pop_current_activity();
    }

    last_screen = screen;
    if (ret_val == GO_TO_PREVIOUS)
        last_screen = old_previous;
    return ret_val;
}

static int load_context_screen(int selection)
{
    const struct menu_item_ex *context_menu = NULL;
    int retval = GO_TO_PREVIOUS;
    push_current_activity(ACTIVITY_CONTEXTMENU);
    if ((root_menu__[selection]->flags&MENU_TYPE_MASK) == MT_RETURN_VALUE)
    {
        int item = root_menu__[selection]->value;
        context_menu = items[item].context_menu;
    }
    /* special cases */
    else if (root_menu__[selection] == &info_menu)
    {
        context_menu = &system_menu;
    }
#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 220)
    else if (root_menu__[selection] == &gameboy_browser)
    {
        context_menu = &gameboy_context_menu;
    }
    else if (root_menu__[selection] == &pokemini_item)
    {
        context_menu = &pokemini_context_menu;
    }
#endif

    if (context_menu)
        retval = do_menu(context_menu, NULL, NULL, false);
    pop_current_activity();
    return retval;
}

static int load_plugin_screen(char *key)
{
    int ret_val = PLUGIN_ERROR;
    int loops = 100;
    int old_previous = last_screen;
    int old_global = global_status.last_screen;
    last_screen = next_screen;
    global_status.last_screen = (char)next_screen;

    while(loops-- > 0) /* just to keep things from getting out of hand */
    {
        int opret = open_plugin_load_entry(key);
        struct open_plugin_entry_t *op_entry = open_plugin_get_entry();
        char *path = op_entry->path;
        char *param = op_entry->param;
        if (param[0] == '\0')
            param = NULL;
        if (path[0] == '\0' && key)
            path = P2STR((unsigned char *)key);
        char achievement_target[MAX_PATH];
        long achievement_started = current_tick;
        strmemccpy(achievement_target, param ? param : path,
                   sizeof(achievement_target));
        int ret = plugin_load(path, param);
        if (ret != PLUGIN_ERROR)
            rockachievements_record_session(achievement_target,
                                            achievement_started);

        if (ret == PLUGIN_USB_CONNECTED || ret == PLUGIN_ERROR)
            ret_val = GO_TO_ROOT;
        else if (ret == PLUGIN_GOTO_WPS)
            ret_val = GO_TO_WPS;
        else if (ret == PLUGIN_GOTO_PLUGIN)
        {
            if(op_entry->lang_id == LANG_OPEN_PLUGIN)
            {
                if (key == (char*)ID2P(LANG_SHORTCUTS))
                {
                    op_entry->lang_id = LANG_SHORTCUTS;
                }
                else /* Bugfix ensure proper key */
                {
                    key = ID2P(LANG_OPEN_PLUGIN);
                }
            }
            continue;
        }
        else
        {
            if (ret == PLUGIN_GOTO_ROOT)
                ret_val = GO_TO_ROOT;
            else
                ret_val = GO_TO_PREVIOUS;
            /* Prevents infinite loop with WPS, Plugins, Previous Screen*/
            if (ret == PLUGIN_OK && old_global == GO_TO_WPS && !audio_status())
                ret_val = GO_TO_ROOT;
            last_screen = (old_previous == next_screen || old_global == GO_TO_ROOT)
                ? GO_TO_ROOT : old_previous;
            if (last_screen == GO_TO_ROOT)
                global_status.last_screen = GO_TO_ROOT;
        }
        /* ret_val != GO_TO_PLUGIN */

        if (opret != OPEN_PLUGIN_NEEDS_FLUSHED || last_screen != GO_TO_WPS)
        {
            /* Keep the entry in case of GO_TO_PREVIOUS */
            op_entry->hash = 0; /*remove hash -- prevents flush to disk */
            op_entry->lang_id = LANG_PREVIOUS_SCREEN;
            /*open_plugin_add_path(NULL, NULL, NULL);// clear entry */
        }
        break;
    } /*while */
    return ret_val;
}

static int load_plugin_path_screen(const char *path, const char *param)
{
    int ret_val = PLUGIN_ERROR;
    int loops = 100;
    int old_previous = last_screen;
    int old_global = global_status.last_screen;
    const char *next_path = path;
    const char *next_param = param;

    last_screen = next_screen;
    global_status.last_screen = (char)next_screen;

    while (loops-- > 0 && next_path)
    {
        char achievement_target[MAX_PATH];
        long achievement_started = current_tick;
        strmemccpy(achievement_target,
                   next_param ? next_param : next_path,
                   sizeof(achievement_target));
        int ret = plugin_load(next_path, next_param);
        struct open_plugin_entry_t *op_entry = open_plugin_get_entry();

        if (ret != PLUGIN_ERROR)
            rockachievements_record_session(achievement_target,
                                            achievement_started);

        if (ret == PLUGIN_GOTO_PLUGIN)
        {
            next_path = op_entry->path;
            next_param = op_entry->param[0] ? op_entry->param : NULL;
            continue;
        }

        if (ret == PLUGIN_USB_CONNECTED || ret == PLUGIN_ERROR)
            ret_val = GO_TO_ROOT;
        else if (ret == PLUGIN_GOTO_WPS)
            ret_val = GO_TO_WPS;
        else
        {
            if (ret == PLUGIN_GOTO_ROOT)
                ret_val = GO_TO_ROOT;
            else
                ret_val = GO_TO_PREVIOUS;
            if (ret == PLUGIN_OK && old_global == GO_TO_WPS && !audio_status())
                ret_val = GO_TO_ROOT;
            last_screen = (old_previous == next_screen || old_global == GO_TO_ROOT)
                ? GO_TO_ROOT : old_previous;
            if (last_screen == GO_TO_ROOT)
                global_status.last_screen = GO_TO_ROOT;
        }

        op_entry->hash = 0;
        op_entry->lang_id = LANG_PREVIOUS_SCREEN;
        break;
    }

    return ret_val;
}

#if defined(IPOD_NANO2G)
static bool root_menu_maybe_run_nano2g_bootloader_stage(void)
{
    if (!file_exists(ROCKPOD_NANO2G_BOOTLOADER_STAGE_MARKER) ||
        !file_exists(ROCKPOD_NANO2G_BOOTLOADER_STAGE_INPUT) ||
        file_exists(ROCKPOD_NANO2G_BOOTLOADER_STAGE_OUTPUT))
    {
        return false;
    }

    /* One-shot marker removal prevents a broken staged file from trapping boot. */
    remove(ROCKPOD_NANO2G_BOOTLOADER_STAGE_MARKER);
    push_activity_without_refresh(ACTIVITY_UNKNOWN);
    (void)load_plugin_path_screen(VIEWERS_DIR "/crypt_firmware.rock",
                                  ROCKPOD_NANO2G_BOOTLOADER_STAGE_INPUT);
    pop_current_activity_without_refresh();
    global_status.last_screen = GO_TO_ROOT;
    last_screen = GO_TO_ROOT;
    return true;
}
#endif

static void ignore_back_button_stub(bool ignore)
{
#if (CONFIG_PLATFORM&PLATFORM_ANDROID)
    /* BACK button to be handled by Android instead of rockbox */
    android_ignore_back_button(ignore);
#else
    (void) ignore;
#endif
}

static int root_menu_setup_screens(void)
{
    int new_screen = next_screen;
    if (global_settings.start_in_screen == 0)
        new_screen = (int)global_status.last_screen;
    else new_screen = global_settings.start_in_screen - 2;
    if (new_screen == GO_TO_PLUGIN)
    {
        if (global_status.last_screen == GO_TO_SHORTCUTMENU)
        {
            /* Can make this any value other than GO_TO_SHORTCUTMENU
               otherwise it takes over on startup when the user wanted
               the plugin at key - LANG_START_SCREEN */
            global_status.last_screen = GO_TO_PLUGIN;
        }
        if(global_status.last_screen == GO_TO_SHORTCUTMENU ||
           global_status.last_screen == GO_TO_PLUGIN)
        {
            if (global_settings.start_in_screen == 0)
            {  /* Start in: Previous Screen */
                last_screen = GO_TO_PREVIOUS;
                global_status.last_screen = GO_TO_ROOT;
                /* since the plugin has GO_TO_PLUGIN as origin it
                   will just return GO_TO_PREVIOUS <=> GO_TO_PLUGIN in a loop
                   To allow exit after restart we check for GO_TO_ROOT
                   if so exit to ROOT after the plugin exits */
            }
        }
    }
#if CONFIG_TUNER
    add_event(PLAYBACK_EVENT_START_PLAYBACK, rootmenu_start_playback_callback);
#endif
    add_event(PLAYBACK_EVENT_TRACK_CHANGE, rootmenu_track_changed_callback);
#ifdef HAVE_RTC_ALARM
    int alarm_wake_up_screen = 0;
    if ( rtc_check_alarm_started(true) )
    {
        rtc_enable_alarm(false);

#if (defined(HAVE_RECORDING) || CONFIG_TUNER)
        alarm_wake_up_screen = global_settings.alarm_wake_up_screen;
#endif
        switch (alarm_wake_up_screen)
        {
#if CONFIG_TUNER
            case ALARM_START_FM:
                new_screen = GO_TO_FM;
                break;
#endif
#ifdef HAVE_RECORDING
            case ALARM_START_REC:
                recording_start_automatic = true;
                new_screen = GO_TO_RECSCREEN;
                break;
#endif
            default:
                new_screen = GO_TO_WPS;
                break;
        } /* switch() */
    }
#endif /* HAVE_RTC_ALARM */

#if defined(HAVE_HEADPHONE_DETECTION) || defined(HAVE_LINEOUT_DETECTION)
    if (new_screen == GO_TO_WPS && global_settings.unplug_autoresume)
    {
       new_screen = GO_TO_ROOT;
#ifdef HAVE_HEADPHONE_DETECTION
        if (headphones_inserted())
            new_screen = GO_TO_WPS;
#endif
#ifdef HAVE_LINEOUT_DETECTION
        if (lineout_inserted())
            new_screen = GO_TO_WPS;
#endif
    }
#endif /*(HAVE_HEADPHONE_DETECTION) || (HAVE_LINEOUT_DETECTION)*/
    return new_screen;
}

static int browser_default(void)
{
    switch (global_settings.browser_default)
    {
#ifdef HAVE_TAGCACHE
        case BROWSER_DEFAULT_DB:
            return GO_TO_DBBROWSER;
#endif
        case BROWSER_DEFAULT_PL_CAT:
            return GO_TO_PLAYLISTS_SCREEN;
        case BROWSER_DEFAULT_FILES:
        default:
            return GO_TO_FILEBROWSER;
    }
}

void root_menu(void)
{
    int previous_browser = browser_default();
    int selected = 0;
#ifdef HAVE_IPODJS_UI
    int ipodjs_selected = 0;
#endif
    int shortcut_origin = GO_TO_ROOT;

    push_current_activity(ACTIVITY_MAINMENU);
    next_screen = root_menu_setup_screens();

#if defined(IPOD_NANO2G)
    if (root_menu_maybe_run_nano2g_bootloader_stage())
        next_screen = GO_TO_ROOT;
#endif

    while (true)
    {
        switch (next_screen)
        {
            case MENU_ATTACHED_USB:
            case MENU_SELECTED_EXIT:
                /* fall through */
            case GO_TO_ROOT:
                if (last_screen != GO_TO_ROOT)
                    selected = get_selection(last_screen);
                global_status.last_screen = GO_TO_ROOT; /* We've returned to ROOT */
                /* When we are in the main menu we want the hardware BACK
                 * button to be handled by HOST instead of rockbox */
                ignore_back_button_stub(true);

#if defined(IPOD_NANO2G)
                next_screen = root_menu_nano2g_dashboard(&selected);
#elif defined(HAVE_IPODJS_UI)
                if (root_menu_video_enabled())
                    next_screen = root_menu_video_dashboard(&ipodjs_selected);
                else
                    next_screen = do_menu(&root_menu_, &selected, NULL, false);
#else
                next_screen = do_menu(&root_menu_, &selected, NULL, false);
#endif

                ignore_back_button_stub(false);

                if (next_screen != GO_TO_PREVIOUS)
                    last_screen = GO_TO_ROOT;
                break;
#ifdef HAVE_TAGCACHE
            case GO_TO_DBBROWSER:
#endif
            case GO_TO_FILEBROWSER:
            case GO_TO_VIDEOS:
            case GO_TO_PLAYLISTS_SCREEN:
                previous_browser = next_screen;
                goto load_next_screen;
                break;
#if CONFIG_TUNER
            case GO_TO_WPS:
            case GO_TO_FM:
                previous_music = next_screen;
                goto load_next_screen;
                break;
#endif /* With !CONFIG_TUNER previous_music is always GO_TO_WPS */

            case GO_TO_PREVIOUS:
            {
                next_screen = last_screen;
                if (last_screen == GO_TO_PLUGIN)/* for WPS */
                    last_screen = GO_TO_PREVIOUS;
                else if (last_screen == GO_TO_PREVIOUS)
                    next_screen = GO_TO_ROOT;
                break;
            }

            case GO_TO_PREVIOUS_BROWSER:
                next_screen = previous_browser;
                break;

            case GO_TO_PREVIOUS_MUSIC:
                next_screen = previous_music;
                break;
            case GO_TO_ROOTITEM_CONTEXT:
                next_screen = load_context_screen(selected);
                break;
            case GO_TO_PLUGIN:
            {

                char *key;
                if (global_status.last_screen == GO_TO_SHORTCUTMENU)
                {
                    struct open_plugin_entry_t *op_entry = open_plugin_get_entry();
                    if (op_entry->lang_id == LANG_OPEN_PLUGIN)
                        op_entry->lang_id = LANG_SHORTCUTS;
                    shortcut_origin = last_screen;
                    key = ID2P(LANG_SHORTCUTS);
                }
                else
                {
                    switch (last_screen)
                    {
                        case GO_TO_ROOT:
                            key = ID2P(LANG_START_SCREEN);
                            break;
                        case GO_TO_WPS:
                            key = ID2P(LANG_OPEN_PLUGIN_SET_WPS_CONTEXT_PLUGIN);
                            break;
                        case GO_TO_SHORTCUTMENU:
                            key = ID2P(LANG_SHORTCUTS);
                            break;
                        case GO_TO_PREVIOUS:
                            key = ID2P(LANG_PREVIOUS_SCREEN);
                            break;
                        default:
                            key = ID2P(LANG_OPEN_PLUGIN);
                            break;
                    }
                }


                push_activity_without_refresh(ACTIVITY_UNKNOWN); /* prevent plugin_load */
                next_screen = load_plugin_screen(key);           /* from flashing root  */
                pop_current_activity_without_refresh();          /* menu activity       */

                if (next_screen == GO_TO_PREVIOUS)
                {
                    /* shortcuts may take several trips through the GO_TO_PLUGIN
                       case make sure we preserve and restore the origin */
                    if(tree_get_context()->out_of_tree > 0) /* a shortcut has been selected */
                    {
                        next_screen = GO_TO_FILEBROWSER;
                        shortcut_origin = GO_TO_ROOT;
                        /* note in some cases there is a screen to return to
                        but the history is rewritten as if you browsed here
                        from the root so return there when finished */
                    }
                    else if (shortcut_origin != GO_TO_ROOT)
                    {
                        if (shortcut_origin != GO_TO_WPS)
                            next_screen = shortcut_origin;
                        shortcut_origin = GO_TO_ROOT;
                    }
                    /* skip GO_TO_PREVIOUS */
                    if (last_screen == GO_TO_BROWSEPLUGINS)
                    {
                        next_screen = last_screen;
                        last_screen = GO_TO_PLUGIN;
                    }
                }
                previous_browser = (next_screen != GO_TO_WPS) ? browser_default() :
                                                                GO_TO_PLUGIN;
                break;
            }
            default:
                goto load_next_screen;
                break;
        } /* switch() */
        continue;
load_next_screen: /* load_screen is inlined */
        next_screen = load_screen(next_screen);
    }

}
