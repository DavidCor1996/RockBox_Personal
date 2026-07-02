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
#include "shortcuts.h"
#include "version.h"

#ifdef HAVE_HOTSWAP
#include "storage.h"
#include "mv.h"
#endif
/* gui api */
#include "list.h"
#include "splash.h"
#include "action.h"
#include "yesno.h"
#include "viewport.h"
#include "core_alloc.h"
#include "rbpaths.h"
#include "bmp.h"

#include "tree.h"
#if CONFIG_TUNER
#include "radio.h"
#endif
#ifdef HAVE_RECORDING
#include "recording.h"
#endif
#include "wps.h"
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
#include "jpeg_load.h"
#endif
#endif
#include "language.h"
#include "plugin.h"
#include "filetypes.h"
#include "file.h"
#include "disk.h"
#include "mv.h"
#include "dir.h"
#if defined(IPOD_NANO2G) || defined(IPOD_VIDEO) || defined(IPOD_6G)
#include "lcd.h"
#include "font.h"
#include "timefuncs.h"
#endif
#if defined(IPOD_VIDEO) || defined(IPOD_6G)
#include "gui/albumlist_art.h"
static bool root_menu_video_enabled(void);
static int ipodjs_video_wps(void);
static bool ipodjs_video_wps_empty(const char *title, const char *message,
                                   bool allow_replay);
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
static int browser(void* param)
{
    int ret_val;
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
                bool reinit_attempted = false;

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

                    /* Re-init if required */
                    if (!reinit_attempted && !stat->ready &&
                        stat->processed_entries == 0 && stat->commit_step == 0)
                    {
                        reinit_attempted = true;
                        tagcache_rebuild();
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
                return GO_TO_PREVIOUS;
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
    return ret_val;
}

#define VIDEO_BROWSER_MAX_FILES 192
#define VIDEO_BROWSER_MAX_DEPTH 6
#define VIDEO_BROWSER_TITLE_MAX 64
#define VIDEO_LIST_INDEX ROCKBOX_DIR "/videolist/index.tsv"
#define VIDEO_LIST_ROOT ROCKBOX_DIR "/videolist"
#define VIDEO_LIST_THUMB_SIZE 32
#define VIDEO_LIST_TEXT_PAD 6
#define VIDEO_LIST_LOOKUP_CACHE 16
#define VIDEO_LIST_BITMAP_CACHE 8
#define VIDEO_LIST_PATH_LEN MAX_PATH

struct video_entry
{
    char path[MAX_PATH];
    char title[VIDEO_BROWSER_TITLE_MAX];
    char format[8];  /* File extension for display (e.g., "MPEG", "MP4") */
    bool is_directory;
    bool playable;
    off_t filesize;  /* File size in bytes */
    time_t mtime;    /* Modification time for sorting */
    int width;       /* Video width (0 = unknown) */
    int height;      /* Video height (0 = unknown) */
    unsigned duration_sec;  /* Duration in seconds (0 = unknown) */
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

#ifdef HAVE_LCD_COLOR
struct video_thumb_lookup_slot {
    bool valid;
    bool found;
    char video_path[VIDEO_LIST_PATH_LEN];
    char thumb_path[VIDEO_LIST_PATH_LEN];
};

struct video_thumb_bitmap_slot {
    bool valid;
    unsigned long last_used;
    char path[VIDEO_LIST_PATH_LEN];
    struct bitmap bm;
    unsigned char data[BM_SIZE(VIDEO_LIST_THUMB_SIZE, VIDEO_LIST_THUMB_SIZE,
                               FORMAT_NATIVE, false)];
};

static struct video_thumb_lookup_slot video_thumb_lookup_cache[VIDEO_LIST_LOOKUP_CACHE];
static int video_thumb_lookup_victim;
static struct video_thumb_bitmap_slot video_thumb_bitmap_cache[VIDEO_LIST_BITMAP_CACHE];
static unsigned long video_thumb_bitmap_tick;
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

static bool video_find_manifest_thumb(const char *video_path,
                                      char *thumb_path,
                                      size_t thumb_path_size)
{
    int fd = open(VIDEO_LIST_INDEX, O_RDONLY);
    if (fd < 0)
        return false;

    char line[512];
    bool found = false;
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char parsed[512];
        char *fields7[7];
        char *fields6[6];
        const char *thumb;
        const char *device_path;

        video_trim_line(line);
        if (line[0] == '#' || line[0] == '\0' ||
            strncmp(line, "video_id\t", 9) == 0)
            continue;

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

        if (!thumb[0])
            continue;

        if (video_manifest_path_matches(video_path, device_path))
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
                                    char *thumb_path,
                                    size_t thumb_path_size)
{
    int i;

    for (i = 0; i < VIDEO_LIST_LOOKUP_CACHE; i++)
    {
        struct video_thumb_lookup_slot *slot = &video_thumb_lookup_cache[i];
        if (!slot->valid)
            continue;
        if (video_ascii_casecmp(slot->video_path, video_path) == 0)
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
    strmemccpy(slot->video_path, video_path, sizeof(slot->video_path));
    slot->found = video_find_manifest_thumb(video_path, slot->thumb_path,
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

static struct bitmap *video_load_thumb_bitmap(const char *path)
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
    slot->bm.width = VIDEO_LIST_THUMB_SIZE;
    slot->bm.height = VIDEO_LIST_THUMB_SIZE;
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

static void videos_scan_dir(struct video_browser_state *state)
{
    int i;

    state->count = 0;
    state->truncated = false;

    if (state->depth == 0 && state->current_path[0] == '\0')
    {
        for (i = 0; i < (int)ARRAYLEN(video_scan_roots); i++)
            videos_scan_single_dir(state, video_scan_roots[i]);
    }
    else
    {
        videos_scan_single_dir(state, state->current_path);
    }

    if (state->count > 1)
        videos_sort(state);
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

    char thumb_path[VIDEO_LIST_PATH_LEN];
    if (!video_lookup_thumb_path(state->entries[list_info->line].path,
                                 thumb_path, sizeof(thumb_path)))
    {
        gui_list_draw_item_default(list_info);
        return;
    }

    struct bitmap *bm = video_load_thumb_bitmap(thumb_path);
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

    int fd = open(VIDEO_LIST_INDEX, O_RDONLY);
    if (fd < 0)
        return;
    close(fd);

    list->callback_draw_item = videos_draw_item;
    FOR_NB_SCREENS(i)
    {
        if (list->line_height[i] < VIDEO_LIST_THUMB_SIZE + 2)
            list->line_height[i] = VIDEO_LIST_THUMB_SIZE + 2;
    }
}
#endif

/* Current video for preview screen - used by context menu */
static struct video_entry *current_preview_entry;
static struct video_browser_state *current_video_browser_state;

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
                    filetype_load_plugin(video_plugin_for_ext(entry->format),
                                         entry->path);
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

    switch (filetype_load_plugin(video_plugin_for_ext(state->entries[selected].format),
                                 state->entries[selected].path))
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
    bool usb;
    int ret = GO_TO_PREVIOUS;

    (void)param;

    state.count = 0;
    state.truncated = false;
    state.next_screen = GO_TO_PREVIOUS;
    state.sort_order = VIDEO_SORT_NAME_AZ;  /* Default sort by name A-Z */
    state.depth = 0;
    state.current_path[0] = '\0';

    videos_scan_dir(&state);

    if (state.count == 0)
    {
        splash(HZ * 2, "No videos found");
        return GO_TO_PREVIOUS;
    }

    if (state.truncated)
        splashf(HZ, "Showing first %d videos", VIDEO_BROWSER_MAX_FILES);

    simplelist_info_init(&list, "Videos", state.count, &state);
    list.get_name = videos_get_name;
    list.get_icon = global_settings.show_icons ? videos_get_icon : NULL;
    list.action_callback = videos_action_cb;
    list.title_icon = Icon_file_view_menu;
    list.selection = MIN(state.selection, state.count - 1);

    push_current_activity(ACTIVITY_FILEBROWSER);
    usb = simplelist_show_list(&list);
    pop_current_activity();

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
    (void)param;
    push_current_activity(ACTIVITY_WPS);

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
#if defined(IPOD_VIDEO) || defined(IPOD_6G)
        if (root_menu_video_enabled())
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
#if defined(IPOD_VIDEO) || defined(IPOD_6G)
            if (root_menu_video_enabled())
                ret_val = ipodjs_video_wps();
            else
#endif
            ret_val = gui_wps_show();
        }
    }
    else if (!file_exists(PLAYLIST_CONTROL_FILE))
    {
#if defined(IPOD_VIDEO) || defined(IPOD_6G)
        if (root_menu_video_enabled())
        {
            ipodjs_video_wps_empty("No Music", "Nothing to resume", false);
            ret_val = GO_TO_ROOT;
        }
        else
#endif
        splash(HZ*2, ID2P(LANG_NOTHING_TO_RESUME));
    }
    else if (
#if defined(IPOD_VIDEO) || defined(IPOD_6G)
             (root_menu_video_enabled() ?
              ipodjs_video_wps_empty("Playlist Finished",
                                     "Select to replay", true) :
              yesno_pop(ID2P(LANG_REPLAY_FINISHED_PLAYLIST))) &&
#else
             yesno_pop(ID2P(LANG_REPLAY_FINISHED_PLAYLIST)) &&
#endif
             playlist_resume() != -1)
    {
        playlist_start(0, 0, 0);
#if defined(IPOD_VIDEO) || defined(IPOD_6G)
        if (root_menu_video_enabled())
            ret_val = ipodjs_video_wps();
        else
#endif
        ret_val = gui_wps_show();
    }

#ifdef HAVE_TAGCACHE
    /* When WPS was entered from cover flow, route WPS exits that normally
     * jump to root/files back through screen history so GO_TO_PREVIOUS
     * reopens pictureflow instead. Do the same for WPS launched from the
     * root menu so browse/back returns there instead of the default browser. */
    if ((ret_val == GO_TO_ROOT || ret_val == GO_TO_PREVIOUS_BROWSER) &&
        (last_screen == GO_TO_PICTUREFLOW || last_screen == GO_TO_ROOT) &&
        audio_status())
    {
        ret_val = GO_TO_PREVIOUS;
    }
#endif

    if (ret_val == GO_TO_PLAYLIST_VIEWER
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
    int ret = filetype_load_plugin("pictureflow", NULL);
    switch (ret)
    {
        case PLUGIN_GOTO_WPS:
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
    (void)param;
    return load_plugin_path_screen(PLUGIN_APPS_DIR "/photos.rock", NULL);
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

static int launch_pokemini(void *param);

    MENUITEM_FUNCTION(weather_item, MENU_FUNC_CHECK_RETVAL,
                  "Weather", launch_weather_plugin,
                  NULL, Icon_Plugin);
    MENUITEM_FUNCTION(maps_item, MENU_FUNC_CHECK_RETVAL,
                  "Maps", launch_maps_plugin,
                  NULL, Icon_Folder);
    MENUITEM_FUNCTION(applications_pokemini_item, MENU_FUNC_CHECK_RETVAL,
                  "PokeMini", launch_pokemini,
                  NULL, Icon_Plugin);
    MAKE_MENU(applications_menu, "Extras", NULL, Icon_Plugin,
          &maps_item, &weather_item, &applications_pokemini_item);

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
          &gameboy_coverflow_item, &gameboy_files_item,
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
    { "applications", &applications_menu },
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
    bool main_menu_added = false;
    bool games_added = false;
    int insert_at = -1;

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
                if (menu_table[i].item == &menu_)
                    main_menu_added = true;
                if (menu_table[i].item == &gameboy_browser ||
                    menu_table[i].item == &pokemini_item)
                    games_added = true;
                if (menu_table[i].item == &videos
#ifdef HAVE_TAGCACHE
                    || menu_table[i].item == &db_browser
#endif
                   )
                    insert_at = (int)menu_item_count;
                break;
            }
        }
    }
    if (!games_added)
    {
        if (insert_at < 0 || (unsigned)insert_at > menu_item_count)
            insert_at = menu_item_count;
        for (i = menu_item_count; i > (unsigned)insert_at; i--)
            root_menu__[i] = root_menu__[i - 1];
        root_menu__[insert_at] = (struct menu_item_ex *)&gameboy_browser;
        menu_item_count++;
    }
    if (!main_menu_added)
        root_menu__[menu_item_count++] = (struct menu_item_ex *)&menu_;
    root_menu_.flags |= MENU_ITEM_COUNT(menu_item_count);
    *(bool*)setting = true;
}

char* root_menu_write_to_cfg(void* setting, char*buf, int buf_len)
{
    (void)setting;
    unsigned i, written, j;
    for (i = 0; i < MENU_GET_COUNT(root_menu_.flags); i++)
    {
        for (j=0; j<MAX_MENU_ITEMS; j++)
        {
            if (menu_table[j].item == root_menu__[i])
            {
                written = snprintf(buf, buf_len, "%s, ", menu_table[j].string);
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

    for (i=0; i<MAX_MENU_ITEMS; i++)
    {
        if (menu_table[i].item == &podemon_go_item ||
            menu_table[i].item == &pokemini_item ||
            menu_table[i].item == &applications_menu ||
            menu_table[i].item == &desktop_mode_item)
            continue;

        root_menu__[count++] = (struct menu_item_ex *)menu_table[i].item;
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
#if CONFIG_CHARGING && !defined(HAVE_POWEROFF_WHILE_CHARGING)
        if (charger_inserted())
            charging_splash();
        else
#endif
            sys_reboot();
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

#if defined(IPOD_VIDEO) || defined(IPOD_6G)
#define IPODJS_HEADER_TOP       LCD_RGBPACK(252, 253, 253)
#define IPODJS_HEADER_MID       LCD_RGBPACK(216, 219, 223)
#define IPODJS_HEADER_BOTTOM    LCD_RGBPACK(174, 178, 183)
#define IPODJS_HEADER_BORDER    LCD_RGBPACK(126, 134, 143)
#define IPODJS_SCREEN_BG        LCD_RGBPACK(255, 255, 255)
#define IPODJS_TEXT             LCD_RGBPACK(0, 0, 0)
#define IPODJS_PREVIEW_TEXT     LCD_RGBPACK(255, 255, 255)
#define IPODJS_PREVIEW_TOP      LCD_RGBPACK(185, 192, 202)
#define IPODJS_PREVIEW_MID      LCD_RGBPACK(130, 138, 151)
#define IPODJS_PREVIEW_BOTTOM   LCD_RGBPACK(94, 102, 116)
#define IPODJS_LOCK_TOP         LCD_RGBPACK(247, 248, 249)
#define IPODJS_LOCK_MID         LCD_RGBPACK(199, 204, 211)
#define IPODJS_LOCK_BOTTOM      LCD_RGBPACK(116, 126, 140)
#define IPODJS_ACTIVE_TOP       LCD_RGBPACK(107, 200, 254)
#define IPODJS_ACTIVE_MID       LCD_RGBPACK(38, 146, 226)
#define IPODJS_ACTIVE_BOTTOM    LCD_RGBPACK(0, 92, 192)
#define IPODJS_GRAPHITE         LCD_RGBPACK(84, 90, 100)
#define IPODJS_U2_RED           LCD_RGBPACK(182, 24, 35)
#define IPODJS_TEAL             LCD_RGBPACK(0, 128, 132)
#define IPODJS_GREEN            LCD_RGBPACK(55, 142, 64)
#define IPODJS_GOLD             LCD_RGBPACK(184, 135, 38)
#define IPODJS_ORANGE           LCD_RGBPACK(208, 104, 32)
#define IPODJS_PURPLE           LCD_RGBPACK(113, 82, 170)
#define IPODJS_PINK             LCD_RGBPACK(195, 72, 128)
#define IPODJS_SPLIT            LCD_RGBPACK(210, 210, 210)
#define IPODJS_MUTED_TEXT       LCD_RGBPACK(99, 101, 103)
#define IPODJS_BATTERY_BORDER   LCD_RGBPACK(98, 98, 98)
#define IPODJS_BATTERY_BG       LCD_RGBPACK(84, 88, 91)
#define IPODJS_BATTERY_HEALTHY  LCD_RGBPACK(165, 224, 127)
#define IPODJS_BATTERY_WARNING  LCD_RGBPACK(209, 127, 107)
#define IPODJS_BATTERY_CAP      LCD_RGBPACK(196, 196, 196)
#define IPODJS_LIST_WIDTH       145
#define IPODJS_HEADER_HEIGHT    20
#define IPODJS_MENU_BOTTOM_INSET 18
#define IPODJS_DB_MAX_ROWS      72
#define IPODJS_DB_MAX_DEPTH     3
#define IPODJS_DB_LABEL_LEN     64
#define IPODJS_WPS_ART_MAX      128
#define IPODJS_DB_ART_MAX       40
#define IPODJS_DB_ART_CACHE     8
#define IPODJS_DB_ART_WORK_EXTRA (44 * 1024)
#define IPODJS_STOCK_ART_SIZE   96
#define IPODJS_PREVIEW_IMAGE_SIZE 320
#define IPODJS_PREVIEW_IMAGE_CACHE 2
#define IPODJS_PREVIEW_MAX_ITEMS 64
#define IPODJS_SLIDESHOW_DELAY  MAX(1, HZ / 12)
#define IPODJS_SLIDESHOW_AUDIO_DELAY MAX(1, HZ / 10)
#define IPODJS_ASSET_DIR        ROCKBOX_DIR "/ipodjs"

static int root_menu_video_theme_depth;

static int root_menu_video_count(void)
{
    return MENU_GET_COUNT(root_menu_.flags);
}

static bool root_menu_video_enabled(void)
{
    return global_settings.ui_engine == UI_ENGINE_IPODJS;
}

static void root_menu_video_enter_native_screen(void)
{
    if (root_menu_video_theme_depth++ == 0)
        viewportmanager_theme_enable(SCREEN_MAIN, false, NULL);
}

static int root_menu_video_finish_native_screen(int ret)
{
    if (root_menu_video_theme_depth > 0 &&
        --root_menu_video_theme_depth == 0)
        viewportmanager_theme_undo(SCREEN_MAIN, false);

    return ret;
}

static int root_menu_video_row_height(void)
{
    int row_h = global_settings.ui_engine_density == UI_ENGINE_DENSITY_COMPACT ?
        20 : 24;

    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_SMALL)
        row_h -= 2;
    else if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_LARGE)
        row_h += 4;

    return MAX(18, row_h);
}

static int root_menu_video_visible_rows(int row_h)
{
    int h = LCD_HEIGHT - IPODJS_HEADER_HEIGHT - IPODJS_MENU_BOTTOM_INSET;

    return MAX(1, h / MAX(1, row_h));
}

static unsigned root_menu_video_accent(void)
{
    switch (global_settings.ui_engine_accent)
    {
        case UI_ENGINE_ACCENT_GRAPHITE:
            return IPODJS_GRAPHITE;
        case UI_ENGINE_ACCENT_U2:
            return IPODJS_U2_RED;
        case UI_ENGINE_ACCENT_TEAL:
            return IPODJS_TEAL;
        case UI_ENGINE_ACCENT_GREEN:
            return IPODJS_GREEN;
        case UI_ENGINE_ACCENT_GOLD:
            return IPODJS_GOLD;
        case UI_ENGINE_ACCENT_ORANGE:
            return IPODJS_ORANGE;
        case UI_ENGINE_ACCENT_PURPLE:
            return IPODJS_PURPLE;
        case UI_ENGINE_ACCENT_PINK:
            return IPODJS_PINK;
        case UI_ENGINE_ACCENT_BLUE:
        default:
            return IPODJS_ACTIVE_BOTTOM;
    }
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
    return root_menu_video_dark() ? LCD_RGBPACK(18, 20, 24) :
                                    IPODJS_SCREEN_BG;
}

static unsigned root_menu_video_row_bg(void)
{
    return root_menu_video_dark() ? LCD_RGBPACK(24, 27, 32) :
                                    IPODJS_SCREEN_BG;
}

static unsigned root_menu_video_text(void)
{
    return root_menu_video_dark() ? LCD_RGBPACK(239, 242, 246) :
                                    IPODJS_TEXT;
}

static unsigned root_menu_video_muted_text(void)
{
    return root_menu_video_dark() ? LCD_RGBPACK(166, 173, 184) :
                                    IPODJS_MUTED_TEXT;
}

static unsigned root_menu_video_header_text(void)
{
    return root_menu_video_dark() ? LCD_RGBPACK(246, 248, 250) :
                                    IPODJS_TEXT;
}

static unsigned root_menu_video_header_bg(void)
{
    return root_menu_video_dark() ? LCD_RGBPACK(24, 29, 38) :
                                    IPODJS_HEADER_BOTTOM;
}

static int root_menu_video_font(void)
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
        path = IPODJS_ASSET_DIR "/14-Adobe-Helvetica-Bold.fnt";
        fallback_path = FONT_DIR "/14-Adobe-Helvetica-Bold.fnt";
    }
    else if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_LARGE)
    {
        fontp = &large_font;
        path = IPODJS_ASSET_DIR "/18-Adobe-Helvetica-Bold.fnt";
        fallback_path = FONT_DIR "/18-Adobe-Helvetica-Bold.fnt";
    }
    else
    {
        fontp = &normal_font;
        path = IPODJS_ASSET_DIR "/16-Adobe-Helvetica-Bold.fnt";
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

static int root_menu_video_text_y_offset(void)
{
    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_SMALL)
        return -1;
    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_LARGE)
        return 1;
    return 0;
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
    if (root_menu_video_dark())
        return LCD_RGBPACK(24, 27, 32);
    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_TRANSPARENT)
        return LCD_RGBPACK(248, 249, 250);
    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_SOFT)
        return LCD_RGBPACK(236, 238, 241);
    return IPODJS_SCREEN_BG;
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
                return "Videos";
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
    alpha = MAX(0, MIN(alpha, 255));
    return LCD_RGBPACK((br * (255 - alpha) + fr * alpha) / 255,
                       (bg * (255 - alpha) + fg * alpha) / 255,
                       (bb * (255 - alpha) + fb * alpha) / 255);
}

static void root_menu_video_gradient(int x, int y, int w, int h,
                                     unsigned top, unsigned bottom)
{
#ifdef HAVE_LCD_COLOR
    lcd_gradient_fillrect(x, y, w, h, top, bottom);
#else
    lcd_set_foreground(bottom);
    lcd_fillrect(x, y, w, h);
#endif
}

static void root_menu_video_glass_gradient(int x, int y, int w, int h,
                                           unsigned top, unsigned mid,
                                           unsigned bottom)
{
    int upper;

    if (h <= 1)
    {
        root_menu_video_gradient(x, y, w, h, top, bottom);
        return;
    }

    upper = MAX(1, (h * 45) / 100);
    root_menu_video_gradient(x, y, w, upper, top, mid);
    root_menu_video_gradient(x, y + upper, w, h - upper, mid, bottom);
}

static void root_menu_video_header_gradient(int x, int y, int w, int h)
{
    if (root_menu_video_dark())
    {
        root_menu_video_glass_gradient(x, y, w, h,
                                       LCD_RGBPACK(92, 98, 108),
                                       LCD_RGBPACK(50, 56, 66),
                                       LCD_RGBPACK(24, 29, 38));
        lcd_set_foreground(LCD_RGBPACK(121, 128, 140));
    }
    else
    {
        root_menu_video_glass_gradient(x, y, w, h, IPODJS_HEADER_TOP,
                                       IPODJS_HEADER_MID,
                                       IPODJS_HEADER_BOTTOM);
        lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
    }
    lcd_hline(x, x + w - 1, y);
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
    unsigned accent = root_menu_video_accent();
    unsigned top;
    unsigned bottom;

    if (global_settings.ui_engine_accent == UI_ENGINE_ACCENT_BLUE)
    {
        root_menu_video_glass_gradient(x, y, w, h, IPODJS_ACTIVE_TOP,
                                       IPODJS_ACTIVE_MID,
                                       IPODJS_ACTIVE_BOTTOM);
        return;
    }

    top = root_menu_video_rgb_blend(FB_UNPACK_RED(accent),
                                    FB_UNPACK_GREEN(accent),
                                    FB_UNPACK_BLUE(accent),
                                    255, 255, 255, 112);
    bottom = root_menu_video_rgb_blend(FB_UNPACK_RED(accent),
                                       FB_UNPACK_GREEN(accent),
                                       FB_UNPACK_BLUE(accent),
                                       0, 0, 0, 70);
    root_menu_video_glass_gradient(x, y, w, h, top, accent, bottom);
}

static void root_menu_video_puts_fit(int x, int y, int width,
                                     const char *text, bool center);
static struct bitmap *root_menu_video_load_ui_bmp(const char *path,
                                                  struct bitmap *bm,
                                                  unsigned char *data,
                                                  size_t data_size,
                                                  int width, int height,
                                                  bool *tried, bool *valid);

static void root_menu_video_storage_info(char *buf, size_t buf_size,
                                         int *used_pctp, bool allow_refresh)
{
    static char cached[32];
    static int cached_used_pct;
    static long cached_tick;
    static bool cached_valid;
    sector_t size = 0;
    sector_t free = 0;
    char avail[32];
    unsigned long long total_kib;
    unsigned long long used_kib;

    if (cached_valid &&
        (!allow_refresh || TIME_BEFORE(current_tick, cached_tick + HZ * 300)))
    {
        strmemccpy(buf, cached, buf_size);
        if (used_pctp)
            *used_pctp = cached_used_pct;
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
    cached_used_pct = MIN(100, (int)((used_kib * 100) / total_kib));
    output_dyn_value(avail, sizeof(avail), free, kibyte_units, 3, true);
    snprintf(cached, sizeof(cached), "%s Free", avail);
    cached_tick = current_tick;
    cached_valid = true;
    strmemccpy(buf, cached, buf_size);
    if (used_pctp)
        *used_pctp = cached_used_pct;
}

static struct bitmap root_menu_video_apple_logo_bm;
static unsigned char root_menu_video_apple_logo_data[
    BM_SIZE(48, 58, FORMAT_NATIVE, false)];
static bool root_menu_video_apple_logo_tried;
static bool root_menu_video_apple_logo_valid;

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
    int inner_x = x + 2;
    int inner_y = y + 2;
    int inner_w = w - 4;
    int inner_h = h - 4;
    int fill_w = inner_w * MAX(0, MIN(used_pct, 100)) / 100;

    lcd_set_foreground(LCD_RGBPACK(58, 62, 68));
    lcd_fillrect(x + 1, y, w - 2, h);
    lcd_fillrect(x, y + 1, w, h - 2);

    lcd_set_foreground(LCD_RGBPACK(226, 229, 234));
    lcd_fillrect(x + 1, y + 1, w - 2, h - 2);

    root_menu_video_gradient(inner_x, inner_y, inner_w, inner_h,
                             LCD_RGBPACK(248, 249, 251),
                             LCD_RGBPACK(201, 207, 216));

    if (fill_w > 0)
    {
        if (fill_w < 2)
            fill_w = 2;
        root_menu_video_gradient(inner_x, inner_y, fill_w, inner_h,
                                 LCD_RGBPACK(112, 190, 245),
                                 LCD_RGBPACK(0, 105, 205));
        lcd_set_foreground(LCD_RGBPACK(182, 226, 255));
        lcd_hline(inner_x, inner_x + fill_w - 1, inner_y);
    }

    lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
    lcd_hline(x + 2, x + w - 3, y + 1);
    lcd_set_foreground(LCD_RGBPACK(86, 91, 100));
    lcd_drawrect(x, y, w, h);
}

static void root_menu_video_draw_settings_preview(int x, int y, int w, int h,
                                                  bool refresh_storage)
{
    char storage[32];
    int used_pct;
    int bar_w = w - 44;
    int bar_x = x + (w - bar_w) / 2;

    root_menu_video_preview_gradient(x, y, w, h);
    root_menu_video_puts_fit(x + 12, y + 18, w - 24, "David's iPod", true);
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
static int root_menu_video_games_menu(void);
static int root_menu_video_clock_screen(void);

#ifdef HAVE_ALBUMART
static struct bitmap root_menu_video_wps_art_bm;
static unsigned char root_menu_video_wps_art_data[
    BM_SCALED_SIZE(IPODJS_WPS_ART_MAX, IPODJS_WPS_ART_MAX,
                   FORMAT_NATIVE, false)];
static char root_menu_video_wps_art_path[MAX_PATH];
static char root_menu_video_wps_art_track_path[MAX_PATH];
static char root_menu_video_wps_art_miss_path[MAX_PATH];
static int root_menu_video_wps_art_size;
static int root_menu_video_wps_art_miss_size;
static bool root_menu_video_wps_art_valid;
static int root_menu_video_aa_slot = -1;

static struct bitmap root_menu_video_default_art_bm;
static unsigned char root_menu_video_default_art_data[
    BM_SCALED_SIZE(IPODJS_WPS_ART_MAX, IPODJS_WPS_ART_MAX,
                   FORMAT_NATIVE, false)];
static bool root_menu_video_default_art_tried;
static bool root_menu_video_default_art_valid;

static struct bitmap root_menu_video_volume_full_bm;
static unsigned char root_menu_video_volume_full_data[
    BM_SIZE(24, 24, FORMAT_NATIVE, false)];
static bool root_menu_video_volume_full_tried;
static bool root_menu_video_volume_full_valid;

static struct bitmap root_menu_video_volume_mute_bm;
static unsigned char root_menu_video_volume_mute_data[
    BM_SIZE(24, 24, FORMAT_NATIVE, false)];
static bool root_menu_video_volume_mute_tried;
static bool root_menu_video_volume_mute_valid;

static struct bitmap root_menu_video_volume_left_stock_bm;
static unsigned char root_menu_video_volume_left_stock_data[
    BM_SIZE(13, 19, FORMAT_NATIVE, false)];
static bool root_menu_video_volume_left_stock_tried;
static bool root_menu_video_volume_left_stock_valid;

static struct bitmap root_menu_video_volume_right_stock_bm;
static unsigned char root_menu_video_volume_right_stock_data[
    BM_SIZE(21, 21, FORMAT_NATIVE, false)];
static bool root_menu_video_volume_right_stock_tried;
static bool root_menu_video_volume_right_stock_valid;

static struct bitmap root_menu_video_gloss_blue_bm;
static unsigned char root_menu_video_gloss_blue_data[
    BM_SIZE(64, 9, FORMAT_NATIVE, false)];
static bool root_menu_video_gloss_blue_tried;
static bool root_menu_video_gloss_blue_valid;
#endif

static struct bitmap root_menu_video_play_bm;
static unsigned char root_menu_video_play_data[
    BM_SIZE(12, 12, FORMAT_NATIVE, false)];
static bool root_menu_video_play_tried;
static bool root_menu_video_play_valid;

static struct bitmap root_menu_video_pause_bm;
static unsigned char root_menu_video_pause_data[
    BM_SIZE(12, 12, FORMAT_NATIVE, false)];
static bool root_menu_video_pause_tried;
static bool root_menu_video_pause_valid;

static struct bitmap root_menu_video_battery_frame_bm;
static unsigned char root_menu_video_battery_frame_data[
    BM_SIZE(27, 12, FORMAT_NATIVE, false)];
static bool root_menu_video_battery_frame_tried;
static bool root_menu_video_battery_frame_valid;
static int root_menu_video_asset_dark_state = -1;

static void root_menu_video_invalidate_ui_asset_cache(void)
{
    root_menu_video_apple_logo_tried = false;
    root_menu_video_apple_logo_valid = false;
    root_menu_video_play_tried = false;
    root_menu_video_play_valid = false;
    root_menu_video_pause_tried = false;
    root_menu_video_pause_valid = false;
    root_menu_video_battery_frame_tried = false;
    root_menu_video_battery_frame_valid = false;
#ifdef HAVE_ALBUMART
    root_menu_video_default_art_tried = false;
    root_menu_video_default_art_valid = false;
    root_menu_video_volume_full_tried = false;
    root_menu_video_volume_full_valid = false;
    root_menu_video_volume_mute_tried = false;
    root_menu_video_volume_mute_valid = false;
    root_menu_video_volume_left_stock_tried = false;
    root_menu_video_volume_left_stock_valid = false;
    root_menu_video_volume_right_stock_tried = false;
    root_menu_video_volume_right_stock_valid = false;
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

static const char *root_menu_video_asset_path(const char *path,
                                              char *buf,
                                              size_t buf_size)
{
    const char *dot;
    size_t base_len;

    if (!root_menu_video_dark() || !path || !buf || buf_size == 0)
        return path;

    dot = strrchr(path, '.');
    if (!dot)
        return path;

    base_len = dot - path;
    if (base_len + sizeof("-dark.bmp") > buf_size)
        return path;

    memcpy(buf, path, base_len);
    snprintf(buf + base_len, buf_size - base_len, "-dark%s", dot);
    if (file_exists(buf))
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

static struct bitmap *root_menu_video_status_play_asset(bool paused)
{
    if (paused)
        return root_menu_video_load_ui_bmp(
            IPODJS_ASSET_DIR "/pause.12x12x24.bmp",
            &root_menu_video_pause_bm, root_menu_video_pause_data,
            sizeof(root_menu_video_pause_data), 12, 12,
            &root_menu_video_pause_tried, &root_menu_video_pause_valid);

    return root_menu_video_load_ui_bmp(
        IPODJS_ASSET_DIR "/play.12x12x24.bmp",
        &root_menu_video_play_bm, root_menu_video_play_data,
        sizeof(root_menu_video_play_data), 12, 12,
        &root_menu_video_play_tried, &root_menu_video_play_valid);
}

static struct bitmap *root_menu_video_battery_frame_asset(void)
{
    return root_menu_video_load_ui_bmp(
        IPODJS_ASSET_DIR "/battery-frame.27x12x24.bmp",
        &root_menu_video_battery_frame_bm,
        root_menu_video_battery_frame_data,
        sizeof(root_menu_video_battery_frame_data), 27, 12,
        &root_menu_video_battery_frame_tried,
        &root_menu_video_battery_frame_valid);
}

static void root_menu_video_draw_play_icon(int x, int y, bool paused)
{
    struct bitmap *bm = root_menu_video_status_play_asset(paused);

    if (bm)
        lcd_bmp(bm, x, y);
}

static void root_menu_video_draw_bolt_icon(int x, int y)
{
    lcd_set_foreground(LCD_RGBPACK(36, 36, 36));
    lcd_hline(x + 12, x + 16, y + 1);
    lcd_hline(x + 11, x + 15, y + 2);
    lcd_hline(x + 10, x + 14, y + 3);
    lcd_hline(x + 9, x + 13, y + 4);
    lcd_hline(x + 8, x + 12, y + 5);
    lcd_hline(x + 12, x + 16, y + 6);
    lcd_hline(x + 11, x + 15, y + 7);
    lcd_hline(x + 10, x + 14, y + 8);
    lcd_hline(x + 9, x + 13, y + 9);
}

static void root_menu_video_draw_plug_icon(int x, int y)
{
    lcd_set_foreground(LCD_RGBPACK(36, 36, 36));
    lcd_fillrect(x + 4, y + 5, 6, 2);
    lcd_fillrect(x + 9, y + 3, 6, 6);
    lcd_fillrect(x + 15, y + 3, 2, 2);
    lcd_fillrect(x + 15, y + 7, 2, 2);
    lcd_fillrect(x + 17, y + 4, 2, 1);
    lcd_fillrect(x + 17, y + 8, 2, 1);
}

static void root_menu_video_draw_battery(int x, int y, int percent,
                                         bool charging)
{
    const int inner_w = 22;
    const int inner_h = 10;
    int draw_percent;
    int fill_w;
    unsigned fill;
    unsigned shine;
    unsigned shade;
    struct bitmap *frame;

    percent = MAX(0, MIN(percent, 100));
    draw_percent = percent;
    if (draw_percent <= 15)
        draw_percent = 15;

    fill = (percent > 20 || charging) ? IPODJS_BATTERY_HEALTHY :
                                        IPODJS_BATTERY_WARNING;
    shine = (percent > 20 || charging) ?
        root_menu_video_rgb_blend(165, 224, 127, 255, 255, 255, 120) :
        root_menu_video_rgb_blend(209, 127, 107, 255, 255, 255, 120);
    shade = root_menu_video_rgb_blend(FB_UNPACK_RED(fill),
                                      FB_UNPACK_GREEN(fill),
                                      FB_UNPACK_BLUE(fill),
                                      0, 0, 0, 82);
    fill_w = (inner_w * draw_percent) / 100;
    frame = root_menu_video_battery_frame_asset();

    lcd_set_foreground(IPODJS_BATTERY_BG);
    lcd_fillrect(x + 1, y + 1, inner_w, inner_h);
    root_menu_video_gradient(x + 1, y + 1, inner_w, inner_h,
                             IPODJS_BATTERY_BG,
                             LCD_RGBPACK(126, 130, 133));
    lcd_set_foreground(fill);
    lcd_fillrect(x + 1, y + 1, fill_w, inner_h);
    if (fill_w > 0)
    {
        lcd_set_foreground(shine);
        lcd_hline(x + 1, x + fill_w, y + 2);
        lcd_hline(x + 1, x + fill_w, y + 3);
        lcd_set_foreground(fill);
        lcd_hline(x + 1, x + fill_w, y + 4);
        lcd_hline(x + 1, x + fill_w, y + 5);
        lcd_set_foreground(shade);
        lcd_hline(x + 1, x + fill_w, y + 8);
        lcd_hline(x + 1, x + fill_w, y + 9);
        lcd_hline(x + 1, x + fill_w, y + 10);
        lcd_hline(x + 1, x + fill_w, y + 11);
    }

    if (frame)
        lcd_bmp(frame, x, y);
    else
    {
        lcd_set_foreground(IPODJS_BATTERY_BORDER);
        lcd_drawrect(x, y, 24, 12);
        lcd_set_foreground(IPODJS_BATTERY_CAP);
        lcd_fillrect(x + 24, y + 3, 3, 6);
    }

    if (charging)
    {
        if (percent >= 100)
            root_menu_video_draw_plug_icon(x, y);
        else
            root_menu_video_draw_bolt_icon(x, y);
    }
}

static void root_menu_video_draw_album_card(int x, int y, int size,
                                            unsigned accent,
                                            const char *label,
                                            bool skew)
{
    int i;

    lcd_set_foreground(LCD_RGBPACK(235, 235, 235));
    if (skew)
    {
        for (i = 0; i < size; i++)
        {
            int inset = i / 7;
            lcd_hline(x + inset, x + size - 1, y + i);
        }
    }
    else
        lcd_fillrect(x, y, size, size);

    lcd_set_foreground(LCD_RGBPACK(243, 243, 243));
    lcd_drawrect(x, y, size, size);
    root_menu_video_gradient(x + 4, y + 4, size - 8, size - 8,
                             accent, LCD_RGBPACK(30, 34, 42));
    lcd_set_foreground(IPODJS_PREVIEW_TEXT);
    lcd_set_background(accent);
    lcd_setfont(root_menu_video_font());
    root_menu_video_puts_fit(x + 8, y + size / 2 - 6, size - 16,
                             label, true);

    if (global_settings.ui_engine_surface != UI_ENGINE_SURFACE_SOLID)
    {
        unsigned reflection = root_menu_video_rgb_blend(104, 110, 122,
                                                       240, 240, 240, 55);
        lcd_set_foreground(reflection);
        lcd_fillrect(x + 8, y + size + 2, size - 16, 5);
        lcd_set_foreground(root_menu_video_rgb_blend(104, 110, 122,
                                                     240, 240, 240, 25));
        lcd_fillrect(x + 12, y + size + 8, size - 24, 3);
    }
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
    if (root_menu_video_aa_slot < 0)
    {
        struct dim dim = { IPODJS_STOCK_ART_SIZE, IPODJS_STOCK_ART_SIZE };
        root_menu_video_aa_slot = playback_claim_aa_slot(&dim);
        if (root_menu_video_aa_slot >= 0)
            playback_update_aa_dims();
    }
}

static struct bitmap *root_menu_video_buffered_art(void)
{
    int handle;
    struct bitmap *bm = NULL;

    root_menu_video_ensure_aa_slot();
    if (root_menu_video_aa_slot < 0)
        return NULL;

    handle = playback_current_aa_hid(root_menu_video_aa_slot);
    if (handle < 0)
        return NULL;

    if (bufgetdata(handle, 0, (void *)&bm) <= 0)
        return NULL;

    return bm;
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

static struct bitmap *root_menu_video_volume_full_icon(void)
{
    return root_menu_video_load_cached_bmp(
        IPODJS_ASSET_DIR "/volume_full.24x24x24.bmp",
        &root_menu_video_volume_full_bm,
        root_menu_video_volume_full_data,
        sizeof(root_menu_video_volume_full_data),
        24, 24,
        &root_menu_video_volume_full_tried,
        &root_menu_video_volume_full_valid);
}

static struct bitmap *root_menu_video_volume_mute_icon(void)
{
    return root_menu_video_load_cached_bmp(
        IPODJS_ASSET_DIR "/volume_mute.24x24x24.bmp",
        &root_menu_video_volume_mute_bm,
        root_menu_video_volume_mute_data,
        sizeof(root_menu_video_volume_mute_data),
        24, 24,
        &root_menu_video_volume_mute_tried,
        &root_menu_video_volume_mute_valid);
}

static struct bitmap *root_menu_video_volume_left_stock_icon(void)
{
    return root_menu_video_load_cached_bmp(
        IPODJS_ASSET_DIR "/volume_left_stock.13x19x24.bmp",
        &root_menu_video_volume_left_stock_bm,
        root_menu_video_volume_left_stock_data,
        sizeof(root_menu_video_volume_left_stock_data),
        13, 19,
        &root_menu_video_volume_left_stock_tried,
        &root_menu_video_volume_left_stock_valid);
}

static struct bitmap *root_menu_video_volume_right_stock_icon(void)
{
    return root_menu_video_load_cached_bmp(
        IPODJS_ASSET_DIR "/volume_right_stock.21x21x24.bmp",
        &root_menu_video_volume_right_stock_bm,
        root_menu_video_volume_right_stock_data,
        sizeof(root_menu_video_volume_right_stock_data),
        21, 21,
        &root_menu_video_volume_right_stock_tried,
        &root_menu_video_volume_right_stock_valid);
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

static struct bitmap *root_menu_video_load_wps_art(struct mp3entry *id3,
                                                   int size)
{
    char path[MAX_PATH];
    char normalized_path[MAX_PATH];
    struct mp3entry normalized_id3;
    const struct mp3entry *lookup_id3 = id3;
    struct dim dim = { size, size };
    int rc;

    if (!id3 || id3->path[0] == '\0' ||
        size <= 0 || size > IPODJS_WPS_ART_MAX)
    {
        root_menu_video_wps_art_valid = false;
        return NULL;
    }

    if (id3->path[0] != '/')
    {
        normalized_id3 = *id3;
        snprintf(normalized_path, sizeof(normalized_path), "/%s", id3->path);
        strmemccpy(normalized_id3.path, normalized_path,
                   sizeof(normalized_id3.path));
        lookup_id3 = &normalized_id3;
    }

    if (root_menu_video_wps_art_miss_size == size &&
        !strcmp(root_menu_video_wps_art_miss_path, lookup_id3->path))
        return NULL;

    if (root_menu_video_wps_art_valid &&
        root_menu_video_wps_art_size == size &&
        !strcmp(root_menu_video_wps_art_track_path, lookup_id3->path))
        return &root_menu_video_wps_art_bm;

    if (!root_menu_video_find_local_cover(lookup_id3, size, path,
                                          sizeof(path)) &&
        !find_albumart(lookup_id3, path, sizeof(path), &dim))
    {
        root_menu_video_wps_art_valid = false;
        root_menu_video_wps_art_path[0] = '\0';
        root_menu_video_wps_art_track_path[0] = '\0';
        root_menu_video_wps_art_miss_size = size;
        strmemccpy(root_menu_video_wps_art_miss_path, lookup_id3->path,
                   sizeof(root_menu_video_wps_art_miss_path));
        return NULL;
    }

    if (root_menu_video_wps_art_valid &&
        root_menu_video_wps_art_size == size &&
        !strcmp(root_menu_video_wps_art_path, path))
        return &root_menu_video_wps_art_bm;

    memset(&root_menu_video_wps_art_bm, 0, sizeof(root_menu_video_wps_art_bm));
    root_menu_video_wps_art_bm.width = size;
    root_menu_video_wps_art_bm.height = size;
    root_menu_video_wps_art_bm.format = FORMAT_NATIVE;
    root_menu_video_wps_art_bm.data = root_menu_video_wps_art_data;

#ifdef HAVE_JPEG
    if (!root_menu_video_path_is_bmp(path))
        rc = read_jpeg_file(path, &root_menu_video_wps_art_bm,
                            sizeof(root_menu_video_wps_art_data),
                            FORMAT_NATIVE | FORMAT_RESIZE |
                            FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);
    else
#endif
    {
        rc = read_bmp_file(path, &root_menu_video_wps_art_bm,
                           sizeof(root_menu_video_wps_art_data),
                           FORMAT_NATIVE | FORMAT_DITHER, NULL);
        if (rc < 0)
            rc = read_bmp_file(path, &root_menu_video_wps_art_bm,
                               sizeof(root_menu_video_wps_art_data),
                               FORMAT_NATIVE | FORMAT_RESIZE |
                               FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);
    }

    if (rc < 0)
    {
        root_menu_video_wps_art_valid = false;
        root_menu_video_wps_art_path[0] = '\0';
        root_menu_video_wps_art_track_path[0] = '\0';
        root_menu_video_wps_art_miss_size = size;
        strmemccpy(root_menu_video_wps_art_miss_path, lookup_id3->path,
                   sizeof(root_menu_video_wps_art_miss_path));
        return NULL;
    }

    root_menu_video_wps_art_valid = true;
    root_menu_video_wps_art_size = size;
    root_menu_video_wps_art_miss_path[0] = '\0';
    root_menu_video_wps_art_miss_size = 0;
    strmemccpy(root_menu_video_wps_art_path, path,
               sizeof(root_menu_video_wps_art_path));
    strmemccpy(root_menu_video_wps_art_track_path, lookup_id3->path,
               sizeof(root_menu_video_wps_art_track_path));
    return &root_menu_video_wps_art_bm;
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
                               FORMAT_NATIVE, false) +
                       IPODJS_DB_ART_WORK_EXTRA];
};

static struct root_menu_video_db_art_slot
    root_menu_video_db_art_cache[IPODJS_DB_ART_CACHE];
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
    char path[MAX_PATH];
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

    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->bm.width = size;
    slot->bm.height = size;
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;

    rc = read_bmp_file(path, &slot->bm, sizeof(slot->data),
                       FORMAT_NATIVE | FORMAT_RESIZE |
                       FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);

    if (rc < 0 || !root_menu_video_bitmap_has_pixels(&slot->bm))
        return NULL;

    slot->valid = true;
    slot->miss = false;
    return &slot->bm;
}

static void root_menu_video_draw_album_thumb(const char *album,
                                             int album_seek, int filter_tag,
                                             int filter_seek, int x, int y,
                                             int size, bool active)
{
    struct bitmap *bm = albumlist_art_get_thumb(album, "", size);

    if (!bm)
        bm = root_menu_video_db_album_art(album_seek, filter_tag,
                                          filter_seek, size);

    lcd_set_foreground(active ? LCD_RGBPACK(206, 231, 252) :
                                LCD_RGBPACK(238, 238, 238));
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
                                 LCD_RGBPACK(230, 233, 238),
                                 LCD_RGBPACK(190, 196, 205));
        lcd_set_foreground(active ? LCD_RGBPACK(245, 250, 255) :
                                    LCD_RGBPACK(150, 156, 166));
        lcd_drawline(x + 5, y + size - 7, x + size / 2, y + 6);
        lcd_drawline(x + size / 2, y + 6, x + size - 5, y + size - 7);
    }

    lcd_set_foreground(active ? LCD_RGBPACK(235, 246, 255) :
                                LCD_RGBPACK(168, 172, 178));
    lcd_drawrect(x - 1, y - 1, size + 2, size + 2);
}
#endif /* HAVE_TAGCACHE */
#endif

static void root_menu_video_draw_slanted_bitmap(struct bitmap *bm,
                                                int x, int y, int size)
{
    lcd_set_foreground(root_menu_video_rgb_blend(255, 255, 255,
                                                 0, 0, 0, 40));
    lcd_fillrect(x + 3, y + 3, size, size);

    lcd_set_foreground(LCD_RGBPACK(243, 243, 243));
    lcd_fillrect(x - 2, y - 2, size + 4, size + 4);
    lcd_bmp_part(bm, 0, 0, x, y, MIN(size, bm->width),
                 MIN(size, bm->height));

    lcd_set_foreground(LCD_RGBPACK(243, 243, 243));
    lcd_drawrect(x - 1, y - 1, size + 2, size + 2);
    lcd_set_foreground(root_menu_video_rgb_blend(255, 255, 255,
                                                 0, 0, 0, 44));
    lcd_hline(x + 2, x + size + 1, y + size + 2);
}

static bool root_menu_video_draw_slanted_art(int x, int y, int size,
                                             unsigned accent,
                                             struct mp3entry *id3)
{
#ifdef HAVE_ALBUMART
    struct bitmap *bm = root_menu_video_load_wps_art(id3, size);

    if (!bm)
        bm = root_menu_video_buffered_art();
    if (!bm)
        bm = root_menu_video_default_art();

    if (bm)
    {
        root_menu_video_draw_slanted_bitmap(bm, x, y, size);
        return true;
    }
#else
    (void)id3;
#endif

    root_menu_video_draw_album_card(x, y, size, accent, "iPod", true);
    return false;
}

static void root_menu_video_puts_fit(int x, int y, int width,
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

static void root_menu_video_duration(char *buf, size_t buf_size,
                                     unsigned long ms)
{
    unsigned long seconds = ms / 1000;

    snprintf(buf, buf_size, "%lu:%02lu", seconds / 60, seconds % 60);
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

static const char *root_menu_video_now_artist(void)
{
    struct mp3entry *id3 = audio_current_track();

    if (id3 && id3->artist && id3->artist[0])
        return id3->artist;

    if (audio_status() & AUDIO_STATUS_PAUSE)
        return "Paused";
    if (audio_status() & AUDIO_STATUS_PLAY)
        return "Playing";

    return "No track";
}

static const char *root_menu_video_now_album(void)
{
    struct mp3entry *id3 = audio_current_track();

    if (id3 && id3->album && id3->album[0])
        return id3->album;

    return "Rockbox";
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

static bool root_menu_video_volume_overlay_active(void)
{
    return global_status.last_volume_change &&
           TIME_BEFORE(current_tick, global_status.last_volume_change + HZ * 2);
}

static void root_menu_video_draw_volume_overlay(void)
{
    int minvol;
    int maxvol;
    int percent;
    int fill_px;
    int x = 34;
    int y = LCD_HEIGHT - 53;
    int track_x = 58;
    int track_w = 204;
    int track_y = y + 6;
    unsigned accent = root_menu_video_accent();
    struct bitmap *left;
    struct bitmap *right;

    if (!root_menu_video_volume_overlay_active())
        return;

    minvol = sound_min(SOUND_VOLUME);
    maxvol = sound_max(SOUND_VOLUME);
    percent = to_normalized_volume(global_status.volume, minvol, maxvol, 100);
    percent = MAX(0, MIN(percent, 100));
    fill_px = (track_w - 2) * percent / 100;

    lcd_set_foreground(root_menu_video_panel());
    lcd_fillrect(0, y - 3, LCD_WIDTH, 34);

#ifdef HAVE_ALBUMART
    left = root_menu_video_volume_left_stock_icon();
    right = root_menu_video_volume_right_stock_icon();
    if (!left)
        left = root_menu_video_volume_mute_icon();
    if (!right)
        right = root_menu_video_volume_full_icon();
    if (left)
        lcd_bmp(left, x, y + 4);
#endif
    ;

    root_menu_video_draw_meter(track_x, track_y, track_w, 18, percent,
                               accent);
    if (fill_px > 0)
    {
        int marker_x = track_x + 1 + fill_px;
        marker_x = MAX(track_x + 1, MIN(marker_x, track_x + track_w - 2));
        lcd_set_foreground(root_menu_video_rgb_blend(38, 129, 205,
                                                     255, 255, 255, 80));
        lcd_vline(marker_x, track_y + 1, track_y + 16);
    }

#ifdef HAVE_ALBUMART
    if (right)
        lcd_bmp(right, LCD_WIDTH - x - right->width, y + 3);
#endif
    ;
}

enum root_menu_video_preview_source {
    IPODJS_PREVIEW_NONE = 0,
    IPODJS_PREVIEW_MUSIC,
    IPODJS_PREVIEW_VIDEOS,
    IPODJS_PREVIEW_GAMES,
};

struct root_menu_video_preview_slot {
    bool valid;
    enum root_menu_video_preview_source source;
    int index;
    char path[MAX_PATH];
    struct bitmap bm;
    unsigned char data[BM_SIZE(IPODJS_PREVIEW_IMAGE_SIZE,
                               IPODJS_PREVIEW_IMAGE_SIZE,
                               FORMAT_NATIVE, false)];
};

static enum root_menu_video_preview_source root_menu_video_preview_loaded;
static int root_menu_video_preview_path_count;
static char root_menu_video_preview_paths[IPODJS_PREVIEW_MAX_ITEMS][MAX_PATH];
static struct root_menu_video_preview_slot
    root_menu_video_preview_slots[IPODJS_PREVIEW_IMAGE_CACHE];
static int root_menu_video_preview_victim;
static struct bitmap root_menu_video_menu_preview_bm;
static unsigned char root_menu_video_menu_preview_data[
    BM_SIZE(174, 220, FORMAT_NATIVE, false)];
static char root_menu_video_menu_preview_path[MAX_PATH];
static int root_menu_video_menu_preview_dark = -1;
static bool root_menu_video_menu_preview_valid;

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
    if (!strcmp(title, "Music"))
        return "music";
    if (!strcmp(title, "Videos"))
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

static bool root_menu_video_draw_menu_preview_asset(const char *title,
                                                    int x, int y, int w,
                                                    int h)
{
    char path[MAX_PATH];
    char themed_path[MAX_PATH];
    const char *load_path;
    int dark = root_menu_video_dark() ? 1 : 0;
    int rc;

    snprintf(path, sizeof(path), IPODJS_ASSET_DIR
             "/previews/%s.174x220x24.bmp",
             root_menu_video_preview_asset_name(title));
    load_path = root_menu_video_asset_path(path, themed_path,
                                           sizeof(themed_path));

    if (!root_menu_video_menu_preview_valid ||
        root_menu_video_menu_preview_dark != dark ||
        strcmp(root_menu_video_menu_preview_path, load_path))
    {
        root_menu_video_menu_preview_valid = false;
        root_menu_video_menu_preview_dark = dark;
        root_menu_video_menu_preview_path[0] = '\0';
        if (!file_exists(load_path))
            return false;

        memset(&root_menu_video_menu_preview_bm, 0,
               sizeof(root_menu_video_menu_preview_bm));
        root_menu_video_menu_preview_bm.width = 174;
        root_menu_video_menu_preview_bm.height = 220;
        root_menu_video_menu_preview_bm.format = FORMAT_NATIVE;
        root_menu_video_menu_preview_bm.data =
            root_menu_video_menu_preview_data;
        rc = read_bmp_file(load_path, &root_menu_video_menu_preview_bm,
                           sizeof(root_menu_video_menu_preview_data),
                           FORMAT_NATIVE | FORMAT_TRANSPARENT, NULL);
        if (rc < 0)
            return false;

        strmemccpy(root_menu_video_menu_preview_path, load_path,
                   sizeof(root_menu_video_menu_preview_path));
        root_menu_video_menu_preview_valid = true;
    }

    lcd_bmp_part(&root_menu_video_menu_preview_bm, 0, 0, x,
                 y + MAX(0, h - root_menu_video_menu_preview_bm.height),
                 MIN(w, root_menu_video_menu_preview_bm.width),
                 MIN(h, root_menu_video_menu_preview_bm.height));
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
    if (!strcmp(title, "Music") || !strcmp(title, "Cover Flow") ||
        !strcmp(title, "Now Playing"))
        return IPODJS_PREVIEW_MUSIC;
    return IPODJS_PREVIEW_NONE;
}

static bool root_menu_video_preview_uses_slideshow(
    enum root_menu_video_preview_source source)
{
    return source != IPODJS_PREVIEW_NONE;
}

static void root_menu_video_preview_reset_slots(void)
{
    for (int i = 0; i < IPODJS_PREVIEW_IMAGE_CACHE; i++)
        root_menu_video_preview_slots[i].valid = false;
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
    char line[512];
    DIR *dir;
    struct dirent *entry;

    while (fd >= 0 &&
           root_menu_video_preview_path_count < IPODJS_PREVIEW_MAX_ITEMS &&
           read_line(fd, line, sizeof(line)) > 0)
    {
        char parsed[512];
        char *fields7[7];
        char *fields6[6];
        const char *art_path;

        video_trim_line(line);
        if (line[0] == '#' || line[0] == '\0' ||
            !strncmp(line, "video_id\t", 9))
            continue;

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

        if (!art_path[0])
            continue;

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", VIDEO_LIST_ROOT, art_path);
        if (file_exists(path))
            root_menu_video_preview_add_path(path);
    }

    if (fd >= 0)
        close(fd);

    bool scanning_previews = true;
    dir = opendir(VIDEO_LIST_ROOT "/previews");
    if (!dir)
    {
        scanning_previews = false;
        dir = opendir(VIDEO_LIST_ROOT "/thumbs");
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

static void root_menu_video_preview_load_game_paths(void)
{
    static const char * const indexes[] = {
        ROCKBOX_DIR "/rocks/games/rockboy_launcher/games.tsv",
        ROCKBOX_DIR "/rocks/games/pokemini_launcher/games.tsv",
    };
    static const char * const cover_dirs[] = {
        ROCKBOX_DIR "/rocks/games/rockboy_launcher/covers",
        ROCKBOX_DIR "/rocks/games/pokemini_launcher/covers",
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
        DIR *dir = opendir(cover_dirs[i]);
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

static void root_menu_video_preview_ensure_paths(
    enum root_menu_video_preview_source source)
{
    if (root_menu_video_preview_loaded == source)
        return;

    root_menu_video_preview_loaded = source;
    root_menu_video_preview_path_count = 0;
    root_menu_video_preview_reset_slots();

    if (source == IPODJS_PREVIEW_VIDEOS)
        root_menu_video_preview_load_video_paths();
    else if (source == IPODJS_PREVIEW_GAMES)
        root_menu_video_preview_load_game_paths();
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

static struct root_menu_video_preview_slot *
root_menu_video_preview_get_slot(enum root_menu_video_preview_source source,
                                 int index)
{
    const char *path;

    if (index < 0 || index >= root_menu_video_preview_path_count)
        return NULL;

    path = root_menu_video_preview_paths[index];
    for (int i = 0; i < IPODJS_PREVIEW_IMAGE_CACHE; i++)
    {
        struct root_menu_video_preview_slot *slot =
            &root_menu_video_preview_slots[i];

        if (slot->valid && slot->source == source &&
            slot->index == index && !strcmp(slot->path, path))
            return slot;
    }

    struct root_menu_video_preview_slot *slot =
        root_menu_video_preview_slot_victim();
    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->bm.width = IPODJS_PREVIEW_IMAGE_SIZE;
    slot->bm.height = IPODJS_PREVIEW_IMAGE_SIZE;
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;

    int rc = read_bmp_file(path, &slot->bm, sizeof(slot->data),
                           FORMAT_NATIVE | FORMAT_RESIZE |
                           FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);
    if (rc < 0)
    {
        slot->valid = false;
        return NULL;
    }

    slot->valid = true;
    slot->source = source;
    slot->index = index;
    strmemccpy(slot->path, path, sizeof(slot->path));
    return slot;
}

static bool root_menu_video_draw_source_slideshow(
    enum root_menu_video_preview_source source, int x, int y, int w, int h)
{
    root_menu_video_preview_ensure_paths(source);
    if (root_menu_video_preview_path_count <= 0)
        return false;

    long tick = current_tick;
    long cycle = tick / (HZ * 3);
    long phase = tick % (HZ * 3);
    int wanted = (int)(cycle % root_menu_video_preview_path_count);
    long pan_phase = MIN(phase, HZ * 2);
    long pan_scale = 1000;
    long pan_pos = pan_phase * pan_scale / (HZ * 2);
    struct root_menu_video_preview_slot *slot =
        root_menu_video_preview_get_slot(source, wanted);

    if (!slot)
        return false;

    int pan_range_x = MAX(0, slot->bm.width - w);
    int pan_range_y = MAX(0, slot->bm.height - h);
    long rev_pan_pos = pan_scale - pan_pos;
    int src_x = pan_range_x / 2;
    int src_y = pan_range_y / 2;

    switch ((int)(cycle % 4))
    {
        case 0:
            src_x = pan_range_x * pan_pos / pan_scale;
            break;
        case 1:
            src_x = pan_range_x * rev_pan_pos / pan_scale;
            break;
        case 2:
            src_y = pan_range_y * pan_pos / pan_scale;
            break;
        default:
            src_y = pan_range_y * rev_pan_pos / pan_scale;
            break;
    }

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(18, 22, 30) : IPODJS_PREVIEW_BOTTOM);
    lcd_fillrect(x, y, w, h);

    int draw_w = MIN(w, slot->bm.width - src_x);
    int draw_h = MIN(h, slot->bm.height - src_y);
    int draw_x = x;
    int draw_y = y;

    if (draw_w <= 0 || draw_h <= 0)
        return false;

    lcd_bmp_part(&slot->bm, src_x, src_y, draw_x, draw_y, draw_w, draw_h);
    return true;
}

static bool root_menu_video_should_animate(
    enum root_menu_video_preview_source source, long *next_tick)
{
    long delay = (audio_status() & AUDIO_STATUS_PLAY) ?
        HZ / 2 : IPODJS_SLIDESHOW_DELAY;

    if (!root_menu_video_preview_uses_slideshow(source))
        return false;

    if (!TIME_AFTER(current_tick, *next_tick))
        return false;

    *next_tick = current_tick + delay;
    return true;
}

static bool root_menu_video_hold_update(bool *held, bool *redraw)
{
    bool now = button_hold();

    albumlist_slideshow_set_paused(now);
    if (now != *held)
    {
        *held = now;
        *redraw = true;
        button_clear_queue();
    }

    return now;
}

static void root_menu_video_draw_status_title_width(const char *title,
                                                    int width,
                                                    bool pane_mode)
{
    int batt = battery_level();
    int batt_x = width - 31;
    int icon_x = batt_x - 17;

    root_menu_video_header_gradient(0, 0, width, IPODJS_HEADER_HEIGHT);
    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(11, 14, 19) : IPODJS_HEADER_BORDER);
    lcd_hline(0, width - 1, IPODJS_HEADER_HEIGHT - 1);

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_header_text());
    lcd_set_background(root_menu_video_header_bg());
    if (pane_mode)
    {
        root_menu_video_puts_fit(6, 3 + root_menu_video_text_y_offset(),
                                 icon_x - 10, "iPod", false);

        if (audio_status() & AUDIO_STATUS_PLAY)
            root_menu_video_draw_play_icon(icon_x, 4,
                audio_status() & AUDIO_STATUS_PAUSE);
    }
    else if (title && !strcmp(title, "Now Playing"))
    {
        root_menu_video_puts_fit(50, 3 + root_menu_video_text_y_offset(),
                                 width - 100, title, true);
        if (audio_status() & AUDIO_STATUS_PLAY)
            root_menu_video_draw_play_icon(6, 4,
                audio_status() & AUDIO_STATUS_PAUSE);
    }
    else
    {
        root_menu_video_puts_fit(6, 3 + root_menu_video_text_y_offset(),
                                 width - 52, title ? title : "iPod",
                                 false);

        if (audio_status() & AUDIO_STATUS_PLAY)
        {
            root_menu_video_draw_play_icon(icon_x, 4,
                audio_status() & AUDIO_STATUS_PAUSE);
        }
    }

    root_menu_video_draw_battery(batt_x, 4, batt, charger_inserted());
}

static void root_menu_video_draw_status_title(const char *title)
{
    root_menu_video_draw_status_title_width(title, LCD_WIDTH, false);
}

static void root_menu_video_draw_status(void)
{
    root_menu_video_draw_status_title_width("iPod", IPODJS_LIST_WIDTH,
                                            true);
}

static void root_menu_video_draw_arrow(int x, int y)
{
    lcd_set_foreground(IPODJS_PREVIEW_TEXT);
    lcd_fillrect(x, y + 1, 2, 1);
    lcd_fillrect(x + 2, y + 2, 2, 1);
    lcd_fillrect(x + 4, y + 3, 2, 1);
    lcd_fillrect(x + 2, y + 4, 2, 1);
    lcd_fillrect(x, y + 5, 2, 1);
}

static void root_menu_video_draw_preview(int selected)
{
    int count = root_menu_video_count();
    const struct menu_item_ex *item = NULL;
    const char *title = "iPod";
    enum root_menu_video_preview_source source = IPODJS_PREVIEW_NONE;
    int x = IPODJS_LIST_WIDTH + 1;
    int w = LCD_WIDTH - x;
    int y = 0;
    int h = LCD_HEIGHT - y;

    if (selected >= 0 && selected < count)
    {
        item = root_menu__[selected];
        title = root_menu_video_preview(item);
        source = root_menu_video_preview_source_for_item(item);
    }

    root_menu_video_preview_gradient(x, y, w, h);

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(IPODJS_PREVIEW_TEXT);
    lcd_set_background(IPODJS_PREVIEW_BOTTOM);

    if (root_menu_video_item_is_settings(item))
    {
        root_menu_video_draw_settings_preview(x, y, w, h, false);
    }
    else
    {
        bool drew = false;

        if (source == IPODJS_PREVIEW_MUSIC)
            drew = albumlist_draw_slideshow(&screens[SCREEN_MAIN], x, y, w, h);
        else if (source == IPODJS_PREVIEW_VIDEOS ||
                 source == IPODJS_PREVIEW_GAMES)
            drew = root_menu_video_draw_source_slideshow(source, x, y, w, h);

        if (!drew)
        {
            if (!root_menu_video_draw_menu_preview_asset(title, x, y, w, h))
            {
                lcd_set_foreground(root_menu_video_dark() ?
                                   LCD_RGBPACK(18, 22, 30) :
                                   IPODJS_PREVIEW_BOTTOM);
                lcd_fillrect(x, y, w, h);
                root_menu_video_puts_fit(x + 14, y + h / 2 - 8,
                                         w - 28, title, true);
            }
        }
    }
}

struct root_menu_video_weather {
    bool available;
    bool hourly;
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
    BM_SIZE(40, 40, FORMAT_NATIVE, false)];
static char root_menu_video_weather_icon_path[MAX_PATH];
static bool root_menu_video_weather_icon_valid;

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
    const char *name = root_menu_video_weather_icon_name(icon, night);
    int rc;

    snprintf(path, sizeof(path), ROCKBOX_DIR
             "/rockpod/weather/icons/%s.40x40x24.bmp", name);
    if (root_menu_video_weather_icon_valid &&
        !strcmp(path, root_menu_video_weather_icon_path))
        return &root_menu_video_weather_icon_bm;

    root_menu_video_weather_icon_valid = false;
    root_menu_video_weather_icon_path[0] = '\0';
    if (!file_exists(path))
        return NULL;

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
        return NULL;

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
                    root_menu_video_weather_cache.checked_tick + HZ * 600))
        return root_menu_video_weather_cache.available;

    root_menu_video_weather_cache.checked_tick = current_tick;
    root_menu_video_weather_cache.available = false;

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

        if (!strcmp(first, "hourly"))
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
        return;

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
}

static void root_menu_video_draw_list(int selected)
{
    int count = root_menu_video_count();
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int top = 0;
    int i;

    if (selected >= visible)
        top = selected - visible + 1;

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(root_menu_video_row_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, IPODJS_LIST_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    for (i = 0; i < visible && top + i < count; i++)
    {
        int index = top + i;
        int y = IPODJS_HEADER_HEIGHT + i * row_h;
        bool active = index == selected;
        const char *label = root_menu_video_label(root_menu__[index]);

        if (active)
        {
            unsigned accent = root_menu_video_accent();
            root_menu_video_selection_gradient(0, y, IPODJS_LIST_WIDTH,
                                               row_h);
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

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(58, 64, 74) : IPODJS_SPLIT);
    lcd_vline(IPODJS_LIST_WIDTH, 0, LCD_HEIGHT - 1);
}

static void ipodjs_video_draw_hold_overlay(void)
{
    if (!button_hold())
        return;

    lcd_set_drawmode(DRMODE_SOLID);
    if (global_settings.ui_engine_hold_effect == UI_ENGINE_HOLD_LOCKSCREEN)
    {
        struct mp3entry *id3 = audio_current_track();
        if (!(audio_status() & AUDIO_STATUS_PLAY))
        {
            root_menu_video_draw_clock_lock();
            return;
        }

        lcd_set_foreground(LCD_RGBPACK(34, 36, 42));
        lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
        root_menu_video_draw_status_title("HOLD");
        root_menu_video_draw_slanted_art(32, 54, IPODJS_STOCK_ART_SIZE,
                                         root_menu_video_accent(), id3);
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(IPODJS_PREVIEW_TEXT);
        lcd_set_background(LCD_RGBPACK(34, 36, 42));
        root_menu_video_puts_fit(150, 78, 142,
                                 root_menu_video_now_title(), false);
        lcd_set_foreground(LCD_RGBPACK(190, 194, 200));
        root_menu_video_puts_fit(150, 100, 142,
                                 root_menu_video_now_artist(), false);
        root_menu_video_puts_fit(150, 140, 142, "Hold", false);
        root_menu_video_puts_fit(150, 160, 142, "Controls Locked", false);
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

static void ipodjs_video_draw_wps(void)
{
    struct mp3entry *id3 = audio_current_track();
    unsigned accent = root_menu_video_accent();
    unsigned panel = root_menu_video_panel();
    unsigned text = root_menu_video_text();
    unsigned muted = root_menu_video_muted_text();
    unsigned split = root_menu_video_dark() ?
        LCD_RGBPACK(54, 60, 70) : IPODJS_SPLIT;
    char elapsed[16];
    char total[16];
    int artwork = IPODJS_STOCK_ART_SIZE;
    int art_x = 38;
    int art_y = 55;
    int info_x = 152;
    int info_w = LCD_WIDTH - info_x - 20;
    int progress_x = 58;
    int progress_w = LCD_WIDTH - progress_x * 2;
    int progress_y = LCD_HEIGHT - 50;
    bool volume_active = root_menu_video_volume_overlay_active();

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_status_title("Now Playing");

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

    root_menu_video_draw_slanted_art(art_x, art_y, artwork, accent, id3);

    lcd_set_foreground(text);
    lcd_set_background(panel);
    lcd_setfont(root_menu_video_font());
    root_menu_video_puts_fit(info_x, 62, info_w,
                             root_menu_video_now_title(), false);
    lcd_set_foreground(muted);
    root_menu_video_puts_fit(info_x, 84, info_w,
                             root_menu_video_now_artist(), false);
    root_menu_video_puts_fit(info_x, 104, info_w,
                             root_menu_video_now_album(), false);

    lcd_set_foreground((audio_status() & AUDIO_STATUS_PAUSE) ?
                       muted : accent);
    lcd_set_background(panel);
    root_menu_video_puts_fit(info_x, 135, info_w,
                             (audio_status() & AUDIO_STATUS_PAUSE) ?
                             "Paused" : "Playing", false);

    root_menu_video_duration(elapsed, sizeof(elapsed),
        id3 ? id3->elapsed : 0);
    root_menu_video_duration(total, sizeof(total),
        (id3 && id3->length > 0) ? id3->length : 0);
    if (!volume_active)
    {
        root_menu_video_draw_meter(progress_x, progress_y, progress_w, 13,
                                   root_menu_video_elapsed_percent(), accent);

        lcd_set_foreground(muted);
        lcd_set_background(panel);
        root_menu_video_puts_fit(progress_x - 40, progress_y + 17, 52,
                                 elapsed, false);
        root_menu_video_puts_fit(progress_x + progress_w - 12,
                                 progress_y + 17, 52, total, true);
    }

    ipodjs_video_draw_hold_overlay();
    root_menu_video_draw_volume_overlay();
    lcd_update();
}

static void ipodjs_video_draw_wps_empty_state(const char *title,
                                              const char *message,
                                              bool allow_replay)
{
    unsigned panel = root_menu_video_panel();
    unsigned text = root_menu_video_text();
    unsigned muted = root_menu_video_muted_text();
    int cx = LCD_WIDTH / 2;
    int icon_y = 64;
    int art_size = 88;
    struct bitmap *art = root_menu_video_default_art();

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_status_title("Now Playing");

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

    if (art)
        lcd_bmp_part(art, 0, 0, cx - art_size / 2, icon_y,
                     MIN(art_size, art->width), MIN(art_size, art->height));

    lcd_setfont(root_menu_video_font());
    lcd_set_foreground(text);
    lcd_set_background(panel);
    root_menu_video_puts_fit(30, 162, LCD_WIDTH - 60,
                             title ? title : "No Music", true);
    lcd_set_foreground(muted);
    root_menu_video_puts_fit(30, 186, LCD_WIDTH - 60,
                             message ? message : "Nothing playing", true);
    if (allow_replay)
        root_menu_video_puts_fit(30, 207, LCD_WIDTH - 60,
                                 "Select replays", true);
    else
        root_menu_video_puts_fit(30, 207, LCD_WIDTH - 60,
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

static int ipodjs_video_wps(void)
{
    bool redraw = true;
    bool held = button_hold();
    bool volume_overlay_was_active = false;
    long last_elapsed_sec = -1;
    bool last_paused = false;
    char last_track_path[MAX_PATH] = "";

    root_menu_video_enter_native_screen();
    root_menu_video_ensure_aa_slot();
#ifdef HAVE_ALBUMART
    root_menu_video_volume_left_stock_icon();
    root_menu_video_volume_right_stock_icon();
#endif
    root_menu_wait_for_button_release();
    button_clear_queue();

    while (audio_status() & AUDIO_STATUS_PLAY)
    {
        int action;

        root_menu_video_hold_update(&held, &redraw);
        if (root_menu_video_volume_overlay_active() != volume_overlay_was_active)
            redraw = true;
        {
            struct mp3entry *id3 = audio_current_track();
            const char *path = id3 ? id3->path : "";
            long elapsed_sec = id3 ? (long)(id3->elapsed / 1000) : -1;
            bool paused = (audio_status() & AUDIO_STATUS_PAUSE) != 0;

            if (strcmp(last_track_path, path))
            {
                strmemccpy(last_track_path, path, sizeof(last_track_path));
                last_elapsed_sec = -1;
                redraw = true;
            }
            if (elapsed_sec != last_elapsed_sec)
            {
                last_elapsed_sec = elapsed_sec;
                redraw = true;
            }
            if (paused != last_paused)
            {
                last_paused = paused;
                redraw = true;
            }
        }
        if (redraw)
        {
            ipodjs_video_draw_wps();
            volume_overlay_was_active = root_menu_video_volume_overlay_active();
            redraw = false;
        }

        action = get_action(CONTEXT_WPS|ALLOW_SOFTLOCK, HZ/5);
        if (action == ACTION_NONE)
            continue;
        if (root_menu_video_hold_update(&held, &redraw))
            continue;

        switch (action)
        {
            case ACTION_WPS_PLAY:
                if (audio_status() & AUDIO_STATUS_PAUSE)
                    audio_resume();
                else
                    audio_pause();
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
                adjust_volume(action == ACTION_WPS_VOLUP ? 1 : -1);
                global_status.last_volume_change = current_tick;
                redraw = true;
                break;

            case ACTION_WPS_BROWSE:
                return root_menu_video_finish_native_screen(
                    GO_TO_PREVIOUS_BROWSER);

            case ACTION_WPS_VIEW_PLAYLIST:
            case ACTION_WPS_CONTEXT:
                return root_menu_video_finish_native_screen(
                    GO_TO_PLAYLIST_VIEWER);

            case ACTION_WPS_MENU:
                return root_menu_video_finish_native_screen(GO_TO_ROOT);

            case ACTION_WPS_STOP:
                audio_pause();
                return root_menu_video_finish_native_screen(GO_TO_ROOT);

            default:
                redraw = true;
                break;
        }
    }

    return root_menu_video_finish_native_screen(GO_TO_ROOT);
}

static void root_menu_video_draw_home(int selected)
{
    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_status();
    root_menu_video_draw_list(selected);
    root_menu_video_draw_preview(selected);
    ipodjs_video_draw_hold_overlay();

    lcd_update();
}

struct root_menu_video_music_item {
    const char *label;
    int screen;
};

#ifdef HAVE_TAGCACHE
enum root_menu_video_music_native {
    IPODJS_MUSIC_NATIVE_NONE = 0,
    IPODJS_MUSIC_NATIVE_ARTISTS,
    IPODJS_MUSIC_NATIVE_ALBUMS,
    IPODJS_MUSIC_NATIVE_SONGS,
};
#endif

static const struct root_menu_video_music_item root_menu_video_music_items[] = {
    { "Now Playing", GO_TO_WPS },
#ifdef HAVE_TAGCACHE
    { "Artists", -IPODJS_MUSIC_NATIVE_ARTISTS },
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

static void root_menu_video_draw_music_menu(int selected)
{
    int count = root_menu_video_music_count();
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int top = 0;
    int i;

    if (selected >= visible)
        top = selected - visible + 1;

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
        int item_y = IPODJS_HEADER_HEIGHT + i * row_h;
        bool active = index == selected;

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

    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

#ifdef HAVE_TAGCACHE
struct root_menu_video_db_row {
    char label[IPODJS_DB_LABEL_LEN];
    int seek;
    int idxid;
    int disc;
    int track;
};

static struct root_menu_video_db_row
    root_menu_video_db_rows[IPODJS_DB_MAX_DEPTH][IPODJS_DB_MAX_ROWS];
static uint32_t root_menu_video_db_uniq[IPODJS_DB_MAX_ROWS * 2];

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

static int root_menu_video_db_load(int tag, int filter_tag, int filter_seek,
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
    if (!tagcache_is_usable() || !tagcache_search(&tcs, tag))
        return -1;

    if (tag != tag_title && tag != tag_filename)
        tagcache_search_set_uniqbuf(&tcs, root_menu_video_db_uniq,
                                    sizeof(root_menu_video_db_uniq));

    if (filter_tag >= 0)
        tagcache_search_add_filter(&tcs, filter_tag, filter_seek);

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
        rows[count].seek = tcs.result_seek;
        rows[count].idxid = tcs.idx_id;
        rows[count].disc = 0;
        rows[count].track = 0;
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

static int root_menu_video_db_play_track(int idxid)
{
    struct tagcache_search tcs;
    char path[MAX_PATH];

    if (!tagcache_search(&tcs, tag_filename))
        return GO_TO_ROOT;

    if (!tagcache_retrieve(&tcs, idxid, tag_filename, path, sizeof(path)))
    {
        tagcache_search_finish(&tcs);
        return GO_TO_ROOT;
    }

    tagcache_search_finish(&tcs);

    if (playlist_create(NULL, NULL) < 0)
        return GO_TO_ROOT;

    if (playlist_insert_track(NULL, path, PLAYLIST_INSERT_LAST,
                              false, true) < 0)
        return GO_TO_ROOT;

    playlist_start(0, 0, 0);
    return GO_TO_WPS;
}

static void root_menu_video_draw_db_menu(const char *title, int tag,
                                         int filter_tag, int filter_seek,
                                         struct root_menu_video_db_row *rows,
                                         int row_count, bool has_more,
                                         int selected)
{
    int row_h = root_menu_video_row_height();
    int visible;
    int font_id;
    int font_h;
    int total = row_count + (has_more ? 2 : 1);
    int top = 0;
    int i;

    if (tag == tag_album)
        row_h = MAX(row_h, 44);
    visible = root_menu_video_visible_rows(row_h);

    if (selected >= visible)
        top = selected - visible + 1;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_status_title(title);

    font_id = root_menu_video_font();
    lcd_setfont(font_id);
    font_h = font_get(font_id)->height;
    lcd_set_foreground(root_menu_video_screen_bg());
    lcd_fillrect(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT);

    for (i = 0; i < visible && top + i < total; i++)
    {
        int index = top + i;
        int item_y = IPODJS_HEADER_HEIGHT + i * row_h;
        bool active = index == selected;
        const char *label = "Back";
        int text_x = 7;
        int text_w = LCD_WIDTH - 28;

        if (index < row_count)
            label = rows[index].label;
        else if (has_more && index == row_count)
            label = "More...";

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

        if (tag == tag_album && index < row_count)
        {
#ifdef HAVE_ALBUMART
            int art_size = MIN(IPODJS_DB_ART_MAX, row_h - 6);
            int art_x = 6;
            int art_y = item_y + (row_h - art_size) / 2;
            root_menu_video_draw_album_thumb(rows[index].label,
                                             rows[index].seek, filter_tag,
                                             filter_seek, art_x, art_y,
                                             art_size, active);
            text_x = art_x + art_size + 8;
            text_w = LCD_WIDTH - text_x - 26;
#endif
        }

        root_menu_video_puts_fit(text_x,
                                 item_y + MAX(2, (row_h - font_h) / 2),
                                 text_w, label, false);
        if (active)
            root_menu_video_draw_arrow(LCD_WIDTH - 16,
                                       item_y + (row_h - 6) / 2);
    }

    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

static int root_menu_video_db_browser_level(const char *title, int tag,
                                            int filter_tag, int filter_seek,
                                            bool tracks, int level)
{
    struct root_menu_video_db_row *rows;
    int selected = 0;
    int offset = 0;
    int row_count = 0;
    bool has_more = false;
    bool reload = true;
    bool redraw = true;
    bool held = button_hold();
    long next_hold_refresh = 0;

    level = MAX(0, MIN(level, IPODJS_DB_MAX_DEPTH - 1));
    rows = root_menu_video_db_rows[level];
    button_clear_queue();

    while (true)
    {
        int action;
        int total;

        root_menu_video_hold_update(&held, &redraw);
        if (reload)
        {
            row_count = root_menu_video_db_load(tag, filter_tag, filter_seek,
                                                offset, rows,
                                                IPODJS_DB_MAX_ROWS,
                                                tracks, &has_more);
            if (row_count < 0)
            {
                splash(HZ, ID2P(LANG_TAGCACHE_BUSY));
                return GO_TO_DBBROWSER;
            }
            selected = MIN(selected, row_count + (has_more ? 1 : 0));
            reload = false;
            redraw = true;
        }

        total = row_count + (has_more ? 2 : 1);
        if (redraw)
        {
            root_menu_video_draw_db_menu(title, tag, filter_tag, filter_seek,
                                         rows, row_count, has_more, selected);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, HZ/20);
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
                selected = selected <= 0 ? total - 1 : selected - 1;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = selected >= total - 1 ? 0 : selected + 1;
                redraw = true;
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (selected < row_count)
                {
                    if (tracks)
                        return root_menu_video_db_play_track(
                            rows[selected].idxid);
                    if (tag == tag_artist)
                    {
                        int ret = root_menu_video_db_browser_level("Albums",
                            tag_album, tag_artist, rows[selected].seek, false,
                            level + 1);
                        if (ret == GO_TO_PREVIOUS)
                        {
                            redraw = true;
                            break;
                        }
                        return ret;
                    }
                    else
                    {
                        int ret = root_menu_video_db_browser_level("Songs",
                            tag_title, tag_album, rows[selected].seek, true,
                            level + 1);
                        if (ret == GO_TO_PREVIOUS)
                        {
                            redraw = true;
                            break;
                        }
                        return ret;
                    }
                }
                if (has_more && selected == row_count)
                {
                    offset += row_count;
                    selected = 0;
                    reload = true;
                    break;
                }
                return GO_TO_PREVIOUS;

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return GO_TO_WPS;

            case ACTION_STD_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return GO_TO_MAINMENU;

            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return GO_TO_PREVIOUS;
        }
    }
}

static int root_menu_video_db_browser(const char *title, int tag,
                                      int filter_tag, int filter_seek,
                                      bool tracks)
{
    return root_menu_video_db_browser_level(title, tag, filter_tag,
                                           filter_seek, tracks, 0);
}
#endif /* HAVE_TAGCACHE */

static int root_menu_video_music_menu(void)
{
    int selected = 0;
    bool redraw = true;
    bool held = button_hold();
    long next_hold_refresh = 0;

    root_menu_video_enter_native_screen();
    button_clear_queue();

    while (true)
    {
        int action;
        int count = root_menu_video_music_count();

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_music_menu(selected);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, HZ/20);
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
                selected = selected <= 0 ? count - 1 : selected - 1;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = selected >= count - 1 ? 0 : selected + 1;
                redraw = true;
                break;

            case ACTION_STD_OK:
            {
                int screen = root_menu_video_music_items[selected].screen;
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
#ifdef HAVE_TAGCACHE
                if (screen < 0)
                {
                    int ret = GO_TO_ROOT;
                    switch (-screen)
                    {
                        case IPODJS_MUSIC_NATIVE_ARTISTS:
                            ret = root_menu_video_db_browser("Artists",
                                tag_artist, -1, 0, false);
                            break;
                        case IPODJS_MUSIC_NATIVE_ALBUMS:
                            ret = root_menu_video_db_browser("Albums",
                                tag_album, -1, 0, false);
                            break;
                        case IPODJS_MUSIC_NATIVE_SONGS:
                            ret = root_menu_video_db_browser("Songs",
                                tag_title, -1, 0, true);
                            break;
                    }
                    if (ret == GO_TO_PREVIOUS)
                    {
                        redraw = true;
                        break;
                    }
                    return root_menu_video_finish_native_screen(ret);
                }
#endif
                return root_menu_video_finish_native_screen(screen);
            }

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_WPS);

            case ACTION_STD_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(
                    root_menu_video_settings_menu());

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
            return root_menu_video_music_menu();
#endif
        if (item->value == GO_TO_MAINMENU)
            return root_menu_video_settings_menu();
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
    IPODJS_EXTRAS_APPLICATIONS,
    IPODJS_EXTRAS_POKEMINI,
};

static const struct root_menu_video_extras_item root_menu_video_extras_items[] = {
    { "Clock", IPODJS_EXTRAS_CLOCK },
    { "Applications", IPODJS_EXTRAS_APPLICATIONS },
#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 220)
    { "PokeMini", IPODJS_EXTRAS_POKEMINI },
#endif
    { "Files", GO_TO_FILEBROWSER },
    { "Playlists", GO_TO_PLAYLISTS_SCREEN },
    { "Plugins", GO_TO_BROWSEPLUGINS },
    { "Shortcuts", GO_TO_SHORTCUTMENU },
    { "System", GO_TO_SYSTEM_SCREEN },
};

static void root_menu_video_clock_point(int minute, int radius,
                                        int *dx, int *dy)
{
    static const signed char sin60[60] = {
         0,   7,  13,  20,  26,  32,  38,  43,  48,  52,
        55,  58,  61,  63,  64,  64,  64,  63,  61,  58,
        55,  52,  48,  43,  38,  32,  26,  20,  13,   7,
         0,  -7, -13, -20, -26, -32, -38, -43, -48, -52,
       -55, -58, -61, -63, -64, -64, -64, -63, -61, -58,
       -55, -52, -48, -43, -38, -32, -26, -20, -13,  -7
    };
    minute %= 60;
    if (minute < 0)
        minute += 60;
    *dx = sin60[minute] * radius / 64;
    *dy = -sin60[(minute + 15) % 60] * radius / 64;
}

static void root_menu_video_draw_analog_clock(bool seconds)
{
    struct tm *tm = get_time();
    int cx = LCD_WIDTH / 2;
    int cy = IPODJS_HEADER_HEIGHT + (LCD_HEIGHT - IPODJS_HEADER_HEIGHT) / 2;
    int radius = 82;
    int last_x = 0;
    int last_y = 0;
    int i;
    char timebuf[24];

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();
    root_menu_video_draw_status_title("Clock");

    if (root_menu_video_dark())
        root_menu_video_gradient(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT,
                                 LCD_RGBPACK(35, 40, 49),
                                 LCD_RGBPACK(18, 22, 29));
    else
        root_menu_video_gradient(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT,
                                 LCD_RGBPACK(250, 251, 252),
                                 LCD_RGBPACK(224, 228, 234));

    lcd_set_foreground(LCD_RGBPACK(168, 174, 182));
    for (i = 0; i <= 60; i += 2)
    {
        int dx;
        int dy;
        root_menu_video_clock_point(i % 60, radius, &dx, &dy);
        if (i > 0)
            lcd_drawline(last_x, last_y, cx + dx, cy + dy);
        last_x = cx + dx;
        last_y = cy + dy;
    }

    for (i = 0; i < 60; i += 5)
    {
        int ox;
        int oy;
        int ix;
        int iy;
        root_menu_video_clock_point(i, radius - 3, &ox, &oy);
        root_menu_video_clock_point(i, radius - 13, &ix, &iy);
        lcd_set_foreground(i % 15 == 0 ? root_menu_video_text() :
                                      root_menu_video_muted_text());
        lcd_drawline(cx + ix, cy + iy, cx + ox, cy + oy);
    }

    if (tm)
    {
        int hx;
        int hy;
        int mx;
        int my;
        int sx;
        int sy;
        int hour_pos = ((tm->tm_hour % 12) * 5) + tm->tm_min / 12;

        root_menu_video_clock_point(hour_pos, radius - 42, &hx, &hy);
        root_menu_video_clock_point(tm->tm_min, radius - 24, &mx, &my);
        lcd_set_foreground(root_menu_video_text());
        lcd_drawline(cx, cy, cx + hx, cy + hy);
        lcd_drawline(cx + 1, cy, cx + hx + 1, cy + hy);
        lcd_set_foreground(IPODJS_GRAPHITE);
        lcd_drawline(cx, cy, cx + mx, cy + my);
        if (seconds)
        {
            root_menu_video_clock_point(tm->tm_sec, radius - 18, &sx, &sy);
            lcd_set_foreground(root_menu_video_accent());
            lcd_drawline(cx, cy, cx + sx, cy + sy);
        }
        lcd_set_foreground(root_menu_video_accent());
        lcd_fillrect(cx - 2, cy - 2, 5, 5);

        int display_hour = tm->tm_hour;
        if (global_settings.timeformat)
        {
            display_hour %= 12;
            if (display_hour == 0)
                display_hour = 12;
        }
        snprintf(timebuf, sizeof(timebuf), "%02d:%02d", display_hour,
                 tm->tm_min);
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(root_menu_video_muted_text());
        lcd_set_background(root_menu_video_screen_bg());
        root_menu_video_puts_fit(80, LCD_HEIGHT - 27, LCD_WIDTH - 160,
                                 timebuf, true);
    }

    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

static int root_menu_video_clock_screen(void)
{
    bool seconds = false;
    bool redraw = true;
    bool held = button_hold();

    button_clear_queue();
    while (true)
    {
        int action;

        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_analog_clock(seconds);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, seconds ? HZ / 2 : HZ);
        switch (action)
        {
            case ACTION_NONE:
                redraw = true;
                break;
            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                seconds = !seconds;
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
                return GO_TO_WPS;
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
    int x = IPODJS_LIST_WIDTH + 1;
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
    lcd_vline(IPODJS_LIST_WIDTH, 0, LCD_HEIGHT - 1);
    root_menu_video_preview_gradient(x, y, w, h);
    root_menu_video_puts_fit(x + 14, y + h / 2 - 12, w - 28,
                             root_menu_video_extras_items[selected].label,
                             true);
    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

static int root_menu_video_extras_menu(void)
{
    int selected = 0;
    bool redraw = true;
    bool held = button_hold();
    long next_hold_refresh = 0;

    root_menu_video_enter_native_screen();
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
        }

        action = get_action(CONTEXT_TREE, HZ/20);
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
                selected = selected <= 0 ? count - 1 : selected - 1;
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = selected >= count - 1 ? 0 : selected + 1;
                redraw = true;
                break;
            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (root_menu_video_extras_items[selected].screen ==
                    IPODJS_EXTRAS_CLOCK)
                {
                    int ret = root_menu_video_clock_screen();
                    if (ret == GO_TO_WPS)
                    {
                        return root_menu_video_finish_native_screen(ret);
                    }
                    redraw = true;
                    break;
                }
                if (root_menu_video_extras_items[selected].screen ==
                    IPODJS_EXTRAS_APPLICATIONS)
                {
                    int ret;
                    root_menu_video_finish_native_screen(0);
                    ret = do_menu(&applications_menu, NULL, NULL, false);
                    root_menu_video_enter_native_screen();
                    if (ret == MENU_ATTACHED_USB)
                        return root_menu_video_finish_native_screen(ret);
                    redraw = true;
                    break;
                }
#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 220)
                if (root_menu_video_extras_items[selected].screen ==
                    IPODJS_EXTRAS_POKEMINI)
                {
                    int ret;
                    root_menu_video_finish_native_screen(0);
                    ret = launch_pokemini(NULL);
                    return ret ? ret : GO_TO_ROOT;
                }
#endif
                return root_menu_video_finish_native_screen(
                    root_menu_video_extras_items[selected].screen);
            case ACTION_STD_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(
                    root_menu_video_settings_menu());
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_ROOT);
        }
    }
}

struct root_menu_video_games_item {
    const char *label;
    int (*function)(void *param);
};

static const struct root_menu_video_games_item root_menu_video_games_items[] = {
    { "Game Cover Flow", launch_gameboy_browser },
    { "Browse ROM Files", browse_gameboy_roms },
    { "PokeMini", launch_pokemini },
    { "Browse PokeMini ROMs", browse_pokemini_roms },
    { "Toggle NES Sound", infones_toggle_sound },
    { "Toggle NES Autosave", infones_toggle_autosave },
    { "Toggle NES Audio Quality", infones_toggle_audio_quality },
    { "Clear NES Saves", infones_clear_saves },
};

static void root_menu_video_draw_games_menu(int selected)
{
    int count = ARRAYLEN(root_menu_video_games_items);
    int row_h = root_menu_video_row_height();
    int visible = root_menu_video_visible_rows(row_h);
    int top = 0;
    int i;
    int x = IPODJS_LIST_WIDTH + 1;
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
    lcd_vline(IPODJS_LIST_WIDTH, 0, LCD_HEIGHT - 1);
    lcd_set_foreground(root_menu_video_dark() ?
                       root_menu_video_panel() : LCD_RGBPACK(255, 255, 255));
    lcd_fillrect(x, y, w, h);
    if (!root_menu_video_draw_menu_preview_asset("Games", x, y, w, h))
    {
        lcd_set_foreground(root_menu_video_text());
        lcd_set_background(root_menu_video_dark() ?
                           root_menu_video_panel() :
                           LCD_RGBPACK(255, 255, 255));
        root_menu_video_puts_fit(x + 14, y + 54, w - 28, "Games", true);
        root_menu_video_puts_fit(x + 14, y + 96, w - 28,
                                 root_menu_video_games_items[selected].label,
                                 true);
        lcd_set_foreground(root_menu_video_muted_text());
        root_menu_video_puts_fit(x + 18, y + 140, w - 36,
                                 "Rockboy / PokeMini", true);
    }
    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

static int root_menu_video_games_menu(void)
{
    int selected = 0;
    bool redraw = true;
    bool held = button_hold();
    long next_hold_refresh = 0;

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
        }

        action = get_action(CONTEXT_TREE, HZ/20);
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
                selected = selected <= 0 ? count - 1 : selected - 1;
                redraw = true;
                break;
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = selected >= count - 1 ? 0 : selected + 1;
                redraw = true;
                break;
            case ACTION_STD_OK:
            {
                int ret;

                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                root_menu_video_finish_native_screen(0);
                ret = root_menu_video_games_items[selected].function(NULL);
                if (selected <= 3)
                    return ret;
                root_menu_video_enter_native_screen();
                redraw = true;
                break;
            }
            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_WPS);
            case ACTION_STD_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(
                    root_menu_video_settings_menu());
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_ROOT);
        }
    }
}

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
    IPODJS_QS_HOLD,
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
        case IPODJS_QS_HOLD:
            return IPODJS_ASSET_DIR "/qs/hold.18x18x24.bmp";
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
        case IPODJS_QS_HOLD:
            return "Hold";
        default:
            return "";
    }
}

static int root_menu_video_qs_percent(int item)
{
    if (item == IPODJS_QS_VOLUME)
    {
        int minvol = sound_min(SOUND_VOLUME);
        int maxvol = sound_max(SOUND_VOLUME);
        return to_normalized_volume(global_status.volume, minvol, maxvol,
                                    100);
    }

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

        case IPODJS_QS_HOLD:
            strmemccpy(buf, global_settings.ui_engine_hold_effect ==
                       UI_ENGINE_HOLD_LOCKSCREEN ? "Lock" : "Dim",
                       buf_size);
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
            adjust_volume(delta < 0 ? -1 : 1);
            global_status.last_volume_change = current_tick;
            return true;

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

        case IPODJS_QS_HOLD:
            global_settings.ui_engine_hold_effect =
                global_settings.ui_engine_hold_effect ==
                UI_ENGINE_HOLD_LOCKSCREEN ? UI_ENGINE_HOLD_DIM :
                UI_ENGINE_HOLD_LOCKSCREEN;
            changed = true;
            break;
    }

    return changed;
}

static void root_menu_video_draw_qs_icon(int x, int y, int item, bool active)
{
    struct bitmap *bm = root_menu_video_qs_icon_asset(item);

    (void)active;
    if (bm)
        lcd_bitmap_transparent((fb_data *)bm->data, x, y,
                               bm->width, bm->height);
}

static void root_menu_video_qs_rect(int item, int *xp, int *yp,
                                    int *wp, int *hp)
{
    const int margin = 10;
    const int gap = 6;
    const int full_w = LCD_WIDTH - margin * 2;
    const int top_y = IPODJS_HEADER_HEIGHT + 10;
    const int slider_h = 32;
    const int tile_h = 28;
    const int tile_w = (full_w - gap) / 2;
    int grid_item;

    if (item <= IPODJS_QS_BRIGHTNESS)
    {
        *xp = margin;
        *yp = top_y + item * (slider_h + gap);
        *wp = full_w;
        *hp = slider_h;
        return;
    }

    grid_item = item - 2;
    *xp = margin + (grid_item & 1) * (tile_w + gap);
    *yp = top_y + 2 * (slider_h + gap) + 4 +
          (grid_item / 2) * (tile_h + gap);
    *wp = tile_w;
    *hp = tile_h;
}

static void root_menu_video_draw_qs_card(int x, int y, int w, int h,
                                         bool active)
{
    unsigned border = active ? root_menu_video_accent() :
        (root_menu_video_dark() ? LCD_RGBPACK(70, 78, 92) :
         LCD_RGBPACK(172, 180, 192));

    lcd_set_foreground(root_menu_video_dark() ?
                       LCD_RGBPACK(8, 10, 14) :
                       LCD_RGBPACK(150, 156, 166));
    lcd_fillrect(x + 2, y + 3, w, h);

    if (active)
        root_menu_video_glass_gradient(x, y, w, h,
                                       LCD_RGBPACK(118, 210, 255),
                                       root_menu_video_accent(),
                                       LCD_RGBPACK(0, 82, 176));
    else if (root_menu_video_dark())
        root_menu_video_glass_gradient(x, y, w, h,
                                       LCD_RGBPACK(58, 65, 78),
                                       LCD_RGBPACK(34, 40, 50),
                                       LCD_RGBPACK(22, 26, 34));
    else
        root_menu_video_glass_gradient(x, y, w, h,
                                       LCD_RGBPACK(255, 255, 255),
                                       LCD_RGBPACK(239, 242, 247),
                                       LCD_RGBPACK(211, 218, 228));

    lcd_set_foreground(border);
    lcd_drawrect(x, y, w, h);
    lcd_set_foreground(active ? LCD_RGBPACK(220, 244, 255) :
                       LCD_RGBPACK(255, 255, 255));
    lcd_hline(x + 1, x + w - 2, y + 1);
}

static void root_menu_video_draw_qs_pill(int x, int y, int w, bool on,
                                         bool active)
{
    unsigned fill = on ? root_menu_video_accent() :
        (root_menu_video_dark() ? LCD_RGBPACK(58, 64, 76) :
         LCD_RGBPACK(198, 204, 214));
    int knob_x = on ? x + w - 12 : x + 2;

    root_menu_video_glass_gradient(x, y, w, 12,
                                   root_menu_video_rgb_blend(
                                       FB_UNPACK_RED(fill),
                                       FB_UNPACK_GREEN(fill),
                                       FB_UNPACK_BLUE(fill),
                                       255, 255, 255, 70),
                                   fill,
                                   root_menu_video_rgb_blend(
                                       FB_UNPACK_RED(fill),
                                       FB_UNPACK_GREEN(fill),
                                       FB_UNPACK_BLUE(fill),
                                       0, 0, 0, 60));
    lcd_set_foreground(active ? LCD_RGBPACK(230, 245, 255) :
                       LCD_RGBPACK(108, 116, 130));
    lcd_drawrect(x, y, w, 12);
    lcd_set_foreground(LCD_RGBPACK(248, 250, 252));
    lcd_fillrect(knob_x, y + 2, 9, 8);
}

static void root_menu_video_draw_quick_settings(int selected)
{
    int i;

    lcd_set_viewport(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_background(root_menu_video_screen_bg());
    lcd_clear_display();

    root_menu_video_draw_status_title("Quick Settings");

    if (root_menu_video_dark())
        root_menu_video_gradient(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT,
                                 LCD_RGBPACK(35, 40, 49),
                                 LCD_RGBPACK(12, 15, 20));
    else
        root_menu_video_gradient(0, IPODJS_HEADER_HEIGHT, LCD_WIDTH,
                                 LCD_HEIGHT - IPODJS_HEADER_HEIGHT,
                                 LCD_RGBPACK(247, 249, 252),
                                 LCD_RGBPACK(217, 223, 232));

    lcd_setfont(root_menu_video_font());

    for (i = 0; i < IPODJS_QS_COUNT; i++)
    {
        char value[32];
        int x, y, w, h;
        bool active = i == selected;
        bool slider = i == IPODJS_QS_VOLUME || i == IPODJS_QS_BRIGHTNESS;
        unsigned text = active ? IPODJS_PREVIEW_TEXT :
            root_menu_video_text();
        unsigned muted = active ? LCD_RGBPACK(230, 245, 255) :
            root_menu_video_muted_text();

        root_menu_video_qs_rect(i, &x, &y, &w, &h);
        root_menu_video_qs_value(i, value, sizeof(value));
        root_menu_video_draw_qs_card(x, y, w, h, active);

        root_menu_video_draw_qs_icon(x + 11, y + h / 2 - 6, i, active);
        lcd_setfont(root_menu_video_font());
        lcd_set_foreground(text);
        lcd_set_background(active ? root_menu_video_accent() :
                           root_menu_video_panel());
        root_menu_video_puts_fit(x + 35,
                                 y + 6 + root_menu_video_text_y_offset(),
                                 slider ? 110 : w - 82,
                                 root_menu_video_qs_label(i), false);

        if (slider)
        {
            int percent = MAX(0, MIN(root_menu_video_qs_percent(i), 100));
            int meter_x = x + 142;
            root_menu_video_draw_meter(meter_x, y + 10, w - 194, 12, percent,
                                       root_menu_video_accent());
            lcd_set_foreground(muted);
            root_menu_video_puts_fit(x + w - 44,
                                     y + 6 +
                                     root_menu_video_text_y_offset(),
                                     36, value, true);
        }
        else
        {
            bool on = !strcmp(value, "On") || !strcmp(value, "Blue") ||
                      !strcmp(value, "Glass") ||
                      !strcmp(value, "Lock");
            if (i == IPODJS_QS_DARK_MODE || i == IPODJS_QS_SHUFFLE)
                root_menu_video_draw_qs_pill(x + w - 44, y + 8, 34, on,
                                             active);
            else
            {
                lcd_set_foreground(muted);
                root_menu_video_puts_fit(x + w - 70,
                                         y + 6 +
                                         root_menu_video_text_y_offset(),
                                         62, value, true);
            }
        }
    }

    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

static int root_menu_video_quick_settings(void)
{
    int selected = 0;
    bool redraw = true;
    bool changed_settings = false;
    bool changed_status = false;
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
            root_menu_video_draw_quick_settings(selected);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, HZ/8);
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
                selected = selected <= 0 ? IPODJS_QS_COUNT - 1 :
                           selected - 1;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
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
                if (changed_settings)
                    settings_save();
                if (changed_status)
                    status_save(false);
                return GO_TO_WPS;

            case ACTION_STD_CANCEL:
            case ACTION_STD_CONTEXT:
            case ACTION_STD_QUICKSCREEN:
            case ACTION_STD_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
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
    int ret = plugin_load(PLUGIN_APPS_DIR "/main_menu_config.rock", NULL);

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
    int x = IPODJS_LIST_WIDTH + 1;
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
    lcd_vline(IPODJS_LIST_WIDTH, 0, LCD_HEIGHT - 1);

    root_menu_video_draw_settings_preview(x, y, w, h, true);

    ipodjs_video_draw_hold_overlay();
    lcd_update();
}

static int root_menu_video_settings_menu(void)
{
    int selected = 0;
    bool redraw = true;
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
        }

        action = get_action(CONTEXT_TREE, HZ/20);
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
                selected = selected <= 0 ? count - 1 : selected - 1;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = selected >= count - 1 ? 0 : selected + 1;
                redraw = true;
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
            {
                int ret;
                root_menu_video_finish_native_screen(0);
                if (root_menu_video_settings_items[selected].function)
                    ret = root_menu_video_settings_items[selected].function();
                else
                    ret = do_menu(root_menu_video_settings_items[selected].menu,
                                  NULL, NULL, true);
                root_menu_video_enter_native_screen();
                if (ret == MENU_ATTACHED_USB)
                    return root_menu_video_finish_native_screen(ret);
                redraw = true;
                break;
            }

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_WPS);

            case ACTION_STD_MENU:
            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                return root_menu_video_finish_native_screen(GO_TO_ROOT);
        }
    }
}

static int root_menu_video_dashboard(int *selectedp)
{
    int selected = MAX(0, MIN(*selectedp, root_menu_video_count() - 1));
    bool redraw = true;
    bool held = button_hold();
    long next_slideshow = 0;
    long next_hold_refresh = 0;

    root_menu_video_enter_native_screen();
    root_menu_video_ensure_aa_slot();
    button_clear_queue();

    while (true)
    {
        int action;
        int count = root_menu_video_count();
        const struct menu_item_ex *item;
        enum root_menu_video_preview_source preview_source;

        if (count <= 0)
            return root_menu_video_finish_native_screen(GO_TO_ROOT);

        selected = MAX(0, MIN(selected, count - 1));
        root_menu_video_hold_update(&held, &redraw);
        if (redraw)
        {
            root_menu_video_draw_home(selected);
            redraw = false;
        }

        item = root_menu__[selected];
        action = get_action(CONTEXT_TREE, HZ/20);
        preview_source = root_menu_video_preview_source_for_item(item);
        switch (action)
        {
            case ACTION_NONE:
                if (held && TIME_AFTER(current_tick, next_hold_refresh))
                {
                    next_hold_refresh = current_tick + HZ;
                    redraw = true;
                }
                else if (!held &&
                         root_menu_video_should_animate(preview_source,
                                                        &next_slideshow))
                    redraw = true;
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = selected <= 0 ? count - 1 : selected - 1;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                selected = selected >= count - 1 ? 0 : selected + 1;
                redraw = true;
                break;

            case ACTION_STD_OK:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                *selectedp = selected;
                return root_menu_video_finish_native_screen(
                    root_menu_video_launch_menu_item(item));

            case ACTION_STD_CONTEXT:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (button_status() & BUTTON_MENU)
                {
                    int ret = root_menu_video_quick_settings();
                    if (ret == GO_TO_WPS)
                    {
                        *selectedp = selected;
                        return root_menu_video_finish_native_screen(ret);
                    }
                    redraw = true;
                    break;
                }
                *selectedp = selected;
                return root_menu_video_finish_native_screen(
                    GO_TO_ROOTITEM_CONTEXT);

            case ACTION_TREE_WPS:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                *selectedp = selected;
                return root_menu_video_finish_native_screen(GO_TO_WPS);

            case ACTION_TREE_STOP:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                if (audio_status())
                {
                    audio_stop();
                    redraw = true;
                }
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
                ret = root_menu_video_quick_settings();
                if (ret == GO_TO_WPS)
                {
                    *selectedp = selected;
                    return root_menu_video_finish_native_screen(ret);
                }
                redraw = true;
                break;
            }

            case ACTION_STD_MENU:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                *selectedp = selected;
                return root_menu_video_finish_native_screen(
                    root_menu_video_settings_menu());

            case ACTION_STD_CANCEL:
                if (root_menu_video_hold_update(&held, &redraw))
                    break;
                *selectedp = selected;
                return root_menu_video_finish_native_screen(GO_TO_PREVIOUS);
        }
    }
}
#endif /* IPOD_VIDEO || IPOD_6G */

static int get_selection(int last_screen)
{
    int i;
    int len = ARRAYLEN(root_menu__);
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
        int ret = plugin_load(path, param);

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
        int ret = plugin_load(next_path, next_param);
        struct open_plugin_entry_t *op_entry = open_plugin_get_entry();

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
#elif defined(IPOD_VIDEO) || defined(IPOD_6G)
                if (root_menu_video_enabled())
                    next_screen = root_menu_video_dashboard(&selected);
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
