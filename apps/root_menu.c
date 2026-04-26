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
#include "talk.h"
#include "audio.h"
#include "shortcuts.h"

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
#include "menus/exported_menus.h"
#ifdef HAVE_RTC_ALARM
#include "rtc.h"
#endif
#ifdef HAVE_TAGCACHE
#include "tagcache.h"
#endif
#include "language.h"
#include "plugin.h"
#include "filetypes.h"
#include "disk.h"
#include "dir.h"
#if defined(IPOD_NANO2G)
#include "lcd.h"
#include "font.h"
#include "timefuncs.h"
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
                        /* Prompt the user */
                        reinit_attempted = true;
                        static const char *lines[]={
                            ID2P(LANG_TAGCACHE_BUSY), ID2P(LANG_TAGCACHE_FORCE_UPDATE)};
                        static const struct text_message message={lines, 2};
                        if(gui_syncyesno_run(&message, NULL, NULL) == YESNO_NO)
                            break;
                        FOR_NB_SCREENS(i)
                            screens[i].clear_display();

                        /* Start initialisation */
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
                        /* (prevent redundant voicing by splash_progress */
                        bool tmp = global_settings.talk_menu;
                        global_settings.talk_menu = false;

                        if (lang_is_rtl())
                        {
                            splash_progress(stat->commit_step,
                                            tagcache_get_max_commit_step(),
                                            "[%d/%d] %s", stat->commit_step,
                                            tagcache_get_max_commit_step(),
                                            str(LANG_TAGCACHE_INIT));
                        }
                        else
                        {
                            splash_progress(stat->commit_step,
                                            tagcache_get_max_commit_step(),
                                            "%s [%d/%d]", str(LANG_TAGCACHE_INIT),
                                            stat->commit_step,
                                            tagcache_get_max_commit_step());
                        }
                        global_settings.talk_menu = tmp;
                    }
                    else
                    {
                        splashf(0, str(LANG_BUILDING_DATABASE),
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

struct video_entry
{
    char path[MAX_PATH];
    char title[VIDEO_BROWSER_TITLE_MAX];
    char format[8];  /* File extension for display (e.g., "MPEG", "MP4") */
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
    int count;
    int selection;
    int next_screen;
    bool truncated;
    enum video_sort_order sort_order;
};

static const char * const video_scan_roots[] = {
    "/Videos",
    "/Video",
    "/iPod_Control/Video",
    "/iPod_Control/Videos",
    "/iPod_Control/Movies",
};

static bool is_supported_video_ext(const char *ext)
{
    return !strcasecmp(ext, "mpg") || !strcasecmp(ext, "mpeg")
        || !strcasecmp(ext, "mpv") || !strcasecmp(ext, "m2v");
}

static bool is_known_video_ext(const char *ext)
{
    return is_supported_video_ext(ext)
        || !strcasecmp(ext, "mp4")
        || !strcasecmp(ext, "m4v")
        || !strcasecmp(ext, "mov");
}

/* Comparison functions for qsort */
static int video_compare_name_az(const void *a, const void *b)
{
    const struct video_entry *va = (const struct video_entry *)a;
    const struct video_entry *vb = (const struct video_entry *)b;
    return strcasecmp(va->title, vb->title);
}

static int video_compare_name_za(const void *a, const void *b)
{
    return -video_compare_name_az(a, b);
}

static int video_compare_date_new(const void *a, const void *b)
{
    const struct video_entry *va = (const struct video_entry *)a;
    const struct video_entry *vb = (const struct video_entry *)b;
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
    
    vid->playable = playable;
    
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

static void videos_scan_dir(struct video_browser_state *state,
                            const char *dir,
                            int depth)
{
    DIR *dp;
    struct dirent *entry;

    if (state->truncated || depth > VIDEO_BROWSER_MAX_DEPTH)
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
            videos_scan_dir(state, fullpath, depth + 1);
            continue;
        }

        ext = video_get_ext(entry->d_name);
        if (!ext || !is_known_video_ext(ext))
            continue;

        videos_add_file(state, fullpath, ext, is_supported_video_ext(ext), &info);
    }

    closedir(dp);
}

static const char *videos_get_name(int selected_item,
                                   void *data,
                                   char *buffer,
                                   size_t buffer_len)
{
    struct video_browser_state *state = data;

    if (selected_item < 0 || selected_item >= state->count)
        return "";

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

    if (state->entries[selected_item].playable)
        return Icon_file_view_menu;

    return Icon_Questionmark;
}

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
                    filetype_load_plugin("mpegplayer", entry->path);
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

    /* Set global state pointer for sort menu access */
    current_video_browser_state = state;

    if (action == ACTION_STD_CANCEL)
    {
        state->selection = selected;
        current_video_browser_state = NULL;
        return action;
    }

    /* Handle context menu (long press on SELECT) */
    if (action == ACTION_STD_CONTEXT)
    {
        if (selected >= 0 && selected < state->count)
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

    switch (filetype_load_plugin("mpegplayer", state->entries[selected].path))
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
    int i;
    int ret = GO_TO_PREVIOUS;

    (void)param;

    state.count = 0;
    state.truncated = false;
    state.next_screen = GO_TO_PREVIOUS;
    state.sort_order = VIDEO_SORT_NAME_AZ;  /* Default sort by name A-Z */

    for (i = 0; i < (int)ARRAYLEN(video_scan_roots) && !state.truncated; i++)
        videos_scan_dir(&state, video_scan_roots[i], 0);

    /* Sort videos by current sort order */
    if (state.count > 1)
        videos_sort(&state);

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
            ret_val = gui_wps_show();
        }
    }
    else if (!file_exists(PLAYLIST_CONTROL_FILE))
        splash(HZ*2, ID2P(LANG_NOTHING_TO_RESUME));
    else if (yesno_pop(ID2P(LANG_REPLAY_FINISHED_PLAYLIST)) &&
             playlist_resume() != -1)
    {
        playlist_start(0, 0, 0);
        ret_val = gui_wps_show();
    }

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

static const struct browse_folder_info gameboy_folder = {"/gameboy/", SHOW_ALL};
#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 220)
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

MENUITEM_FUNCTION(gameboy_coverflow_item, MENU_FUNC_CHECK_RETVAL,
                  "Game Cover Flow", launch_gameboy_browser,
                  NULL, Icon_NOICON);
MENUITEM_FUNCTION(gameboy_files_item, MENU_FUNC_CHECK_RETVAL,
                  "Browse ROM Files", browse_gameboy_roms,
                  NULL, Icon_NOICON);
MAKE_MENU(gameboy_context_menu, "Games", NULL, Icon_NOICON,
          &gameboy_coverflow_item, &gameboy_files_item);

MENUITEM_FUNCTION(gameboy_browser, MENU_FUNC_CHECK_RETVAL,
                  ID2P(LANG_PLUGIN_GAMES), launch_gameboy_browser,
                  NULL, Icon_Folder);
MENUITEM_FUNCTION(podemon_go_item, MENU_FUNC_CHECK_RETVAL,
                  "Podemon Go", launch_podemon_go,
                  NULL, Icon_Plugin);
#else
MENUITEM_FUNCTION_W_PARAM(gameboy_browser, MENU_FUNC_CHECK_RETVAL,
                          ID2P(LANG_PLUGIN_GAMES), browse_folder,
                          (void *)&gameboy_folder, NULL, Icon_Folder);
MENUITEM_RETURNVALUE(podemon_go_item, "Podemon Go", GO_TO_ROOT,
                     NULL, Icon_Plugin);
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
    { "photos", &photos_item },
    { "games", &gameboy_browser },
    { "podemon_go", &podemon_go_item },
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
    bool photos_added = false;
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
                if (menu_table[i].item == &gameboy_browser)
                    games_added = true;
                if (menu_table[i].item == &photos_item)
                    photos_added = true;
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
    if (!photos_added)
    {
        if (insert_at < 0 || (unsigned)insert_at > menu_item_count)
            insert_at = menu_item_count;
        for (i = menu_item_count; i > (unsigned)insert_at; i--)
            root_menu__[i] = root_menu__[i - 1];
        root_menu__[insert_at] = (struct menu_item_ex *)&photos_item;
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
        if (menu_table[i].item == &podemon_go_item)
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
    while (button_status() & (BUTTON_MENU | BUTTON_SELECT))
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

static bool root_menu_nano2g_draw_bitmap(const char *name)
{
    char path[MAX_PATH];

    snprintf(path, sizeof(path), WPS_DIR "/iPone_nano2g/%s", name);

    if (strcmp(root_menu_nano2g_bmp_path, path))
    {
        root_menu_nano2g_bmp.data = root_menu_nano2g_bmp_buf;
        root_menu_nano2g_bmp.width = LCD_WIDTH;
        root_menu_nano2g_bmp.height = LCD_HEIGHT;
        if (read_bmp_file(path, &root_menu_nano2g_bmp,
                          sizeof(root_menu_nano2g_bmp_buf),
                          FORMAT_NATIVE | FORMAT_DITHER, NULL) < 0)
        {
            root_menu_nano2g_bmp_path[0] = '\0';
            return false;
        }

        strmemccpy(root_menu_nano2g_bmp_path, path,
                   sizeof(root_menu_nano2g_bmp_path));
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

    lcd_putsxy(x, y, (const unsigned char *)buf);
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

static void root_menu_nano2g_draw_status(void)
{
    char buf[24];
    struct tm *tm = get_time();
    int batt = battery_level();

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(NANO2G_DASH_TEXT);
    root_menu_nano2g_puts_fit(6, 4, 62, "iPone", false);

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

static void root_menu_nano2g_draw_tile(int item_index, int slot, int selected)
{
    int col = slot % 2;
    int row = slot / 2;
    int x = 7 + col * 83;
    int y = 51 + row * 22;
    bool is_selected = item_index == selected;
    const struct menu_item_ex *item = root_menu__[item_index];

    root_menu_nano2g_fill_roundish(x, y, 79, 19,
        is_selected ? LCD_RGBPACK(86, 56, 126) : NANO2G_DASH_PANEL_2);

    lcd_set_foreground(is_selected ? NANO2G_DASH_ACCENT_2 : NANO2G_DASH_ACCENT);
    lcd_fillrect(x + 4, y + 4, 11, 11);
    lcd_set_foreground(is_selected ? LCD_RGBPACK(86, 56, 126) : NANO2G_DASH_PANEL_2);
    root_menu_nano2g_puts_fit(x + 6, y + 5, 7,
                              root_menu_nano2g_glyph(item), true);

    lcd_set_foreground(is_selected ? NANO2G_DASH_TEXT : NANO2G_DASH_DIM);
    root_menu_nano2g_puts_fit(x + 20, y + 6, 52,
                              root_menu_nano2g_label(item), false);

    if (is_selected)
    {
        lcd_set_foreground(NANO2G_DASH_ACCENT_2);
        lcd_drawrect(x, y, 79, 19);
    }
}

static void root_menu_nano2g_draw_dashboard(int selected)
{
    int count = root_menu_nano2g_count();
    int page = selected < 0 ? 0 : selected / 6;
    int start = page * 6;
    int pages = (count + 5) / 6;
    int i;
    char page_label[16];

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
    root_menu_nano2g_draw_now_playing(selected < 0);

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(NANO2G_DASH_ACCENT_2);
    snprintf(page_label, sizeof(page_label), "Page %d/%d", page + 1, pages);
    root_menu_nano2g_puts_fit(112, 36, 55, page_label, false);

    for (i = 0; i < 6 && start + i < count; i++)
        root_menu_nano2g_draw_tile(start + i, i, selected);

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
    struct tm *tm = get_time();
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
        lcd_set_foreground(LCD_RGBPACK(27, 15, 45));
        lcd_fillrect(0, 86, LCD_WIDTH, 46);
    }
    lcd_set_foreground(LCD_RGBPACK(10, 6, 16));
    lcd_fillrect(0, 0, LCD_WIDTH, 18);
    lcd_set_foreground(LCD_RGBPACK(18, 11, 30));
    lcd_fillrect(0, 86, LCD_WIDTH, 46);

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(NANO2G_DASH_ACCENT_2);
    root_menu_nano2g_puts_fit(8, 7, 48, "HOLD", false);
    lcd_set_foreground(charger_inserted() ? NANO2G_DASH_ACCENT : NANO2G_DASH_TEXT);
    snprintf(buf, sizeof(buf), "%s%d%%", charger_inserted() ? "CHG " : "", batt);
    root_menu_nano2g_puts_fit(121, 7, 48, buf, false);

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(NANO2G_DASH_TEXT);
    root_menu_nano2g_time12(buf, sizeof(buf), tm, true);
    root_menu_nano2g_puts_fit(0, 39, LCD_WIDTH, buf, true);

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_foreground(NANO2G_DASH_DIM);
    if (tm && valid_time(tm))
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    else
        snprintf(buf, sizeof(buf), "Locked");
    root_menu_nano2g_puts_fit(0, 58, LCD_WIDTH, buf, true);

    root_menu_nano2g_fill_roundish(12, 88, 152, 30, NANO2G_DASH_PANEL);
    lcd_set_foreground(NANO2G_DASH_TEXT);
    root_menu_nano2g_puts_fit(18, 93, 108, root_menu_nano2g_now_title(), false);
    lcd_set_foreground(NANO2G_DASH_DIM);
    root_menu_nano2g_puts_fit(18, 105, 108, root_menu_nano2g_now_subtitle(), false);
    lcd_set_foreground(NANO2G_DASH_ACCENT);
    root_menu_nano2g_puts_fit(132, 99, 24,
                              (audio_status() & AUDIO_STATUS_PLAY) ? "PLAY" : "IDLE",
                              true);

    lcd_set_foreground(NANO2G_DASH_DIM);
    root_menu_nano2g_puts_fit(0, 124, LCD_WIDTH, "Slide hold switch to unlock", true);
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
            root_menu_nano2g_draw_dashboard(selected);
            redraw = false;
        }

        action = get_action(CONTEXT_TREE, HZ/5);
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (selected < 0)
                    selected = count - 1;
                else if (selected == 0)
                    selected = -1;
                else
                    selected--;
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (selected < 0)
                    selected = 0;
                else if (selected >= count - 1)
                    selected = -1;
                else
                    selected++;
                redraw = true;
                break;

            case ACTION_STD_OK:
                *selectedp = selected;
                viewportmanager_theme_undo(SCREEN_MAIN, false);
                if (selected < 0)
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
                if (selected < 0 && (audio_status() & AUDIO_STATUS_PLAY))
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
