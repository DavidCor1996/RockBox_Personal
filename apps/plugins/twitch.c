/***************************************************************************
 * Offline, creator-centric TwitchTV VOD channels for RockPod.
 *
 * Copyright (C) 2026 David Cor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 ****************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "lib/video_player.h"

#if !defined(HAVE_LCD_COLOR) || LCD_WIDTH < 320 || LCD_HEIGHT < 240
#error The Twitch application requires a 320x240 colour display
#endif

#define TW_ROOT             ROCKBOX_DIR "/twitch"
#define TW_CREATORS         TW_ROOT "/creators.tsv"
#define TW_VODS             TW_ROOT "/vods.tsv"
#define TW_ASSET_ROOT       ROCKBOX_DIR "/ipodjs/twitch"
#define TW_LOGO             TW_ASSET_ROOT "/twitch-wordmark-current-white.136x50.bmp"
#define TW_PLAYER_PREFIX    "twitch-app:"
#define TW_LIVE_PREFIX      "twitch-live:"

#define TW_MAX_CREATORS     24
#define TW_MAX_VODS         64
#define TW_LINE_SIZE        1024
#define TW_LOGO_W           136
#define TW_LOGO_H           50
#define TW_THUMB_W          96
#define TW_THUMB_H          54
#define TW_THUMB_SLOTS      2
#define TW_LIST_TOP         75
#define TW_ROW_H            70
#define TW_VISIBLE_ROWS     2

#define TW_DARK             LCD_RGBPACK(0x0e, 0x0e, 0x10)
#define TW_PANEL            LCD_RGBPACK(0x18, 0x18, 0x1b)
#define TW_PURPLE           LCD_RGBPACK(0x91, 0x46, 0xff)
#define TW_PURPLE_DARK      LCD_RGBPACK(0x5c, 0x16, 0xc5)
#define TW_PURPLE_LIGHT     LCD_RGBPACK(0xbf, 0x94, 0xff)
#define TW_TEXT             LCD_RGBPACK(0xef, 0xef, 0xf1)
#define TW_MUTED            LCD_RGBPACK(0xad, 0xad, 0xb8)
#define TW_LIVE             LCD_RGBPACK(0xcc, 0x18, 0x28)

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
static const struct button_mapping tw_main_ctx[] =
{
    { PLA_SCROLL_BACK,        BUTTON_SCROLL_BACK,               BUTTON_NONE },
    { PLA_SCROLL_FWD,         BUTTON_SCROLL_FWD,                BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT, BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT,  BUTTON_SCROLL_FWD|BUTTON_REPEAT,  BUTTON_NONE },
    { PLA_LEFT,               BUTTON_LEFT,                      BUTTON_NONE },
    { PLA_RIGHT,              BUTTON_RIGHT,                     BUTTON_NONE },
    { PLA_LEFT_REPEAT,        BUTTON_LEFT|BUTTON_REPEAT,        BUTTON_NONE },
    { PLA_RIGHT_REPEAT,       BUTTON_RIGHT|BUTTON_REPEAT,       BUTTON_NONE },
    { PLA_SELECT_REL,         BUTTON_SELECT|BUTTON_REL,         BUTTON_NONE },
    { PLA_CANCEL,             BUTTON_MENU,                      BUTTON_NONE },
    { PLA_EXIT,               BUTTON_PLAY|BUTTON_REL,           BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};
static const struct button_mapping *plugin_contexts[] = { tw_main_ctx };
#else
static const struct button_mapping *plugin_contexts[] = { pla_main_ctx };
#endif

enum tw_page
{
    TW_PAGE_LIVE = 0,
    TW_PAGE_VODS,
    TW_PAGE_COUNT,
};

struct tw_creator
{
    char key[17];
    char login[26];
    char display_name[65];
    unsigned long cycle_epoch;
    unsigned long followers;
};

struct tw_vod
{
    char id[40];
    char creator_key[17];
    char title[81];
    char game[41];
    char published[25];
    char views[24];
    char video_path[MAX_PATH];
    char thumb_path[MAX_PATH];
    char description[181];
    unsigned long duration;
};

static struct tw_creator creators[TW_MAX_CREATORS];
static struct tw_vod vods[TW_MAX_VODS];
static int creator_count;
static int vod_count;
static enum tw_page current_page;
static int selection[TW_PAGE_COUNT];
static bool detail_screen;
static int detail_creator;
static int detail_vod;
static int detail_action;

static fb_data logo_data[TW_LOGO_W * TW_LOGO_H] CACHEALIGN_ATTR;
static fb_data thumb_data[TW_THUMB_SLOTS][TW_THUMB_W * TW_THUMB_H]
    CACHEALIGN_ATTR;
static struct bitmap logo_bmp;
static struct bitmap thumb_bmp[TW_THUMB_SLOTS];
static bool logo_valid;
static bool thumb_valid[TW_THUMB_SLOTS];
static int thumb_vod[TW_THUMB_SLOTS];
static int thumb_next_slot;
static int thumb_pending = -1;
static long thumb_due;

static int tw_split_tabs(char *line, char **fields, int maximum)
{
    int count = 0;
    char *cursor = line;

    while (count < maximum)
    {
        char *tab;

        fields[count++] = cursor;
        tab = rb->strchr(cursor, '\t');
        if (tab == NULL)
            break;
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static bool tw_load_bitmap(const char *path, struct bitmap *bitmap,
                           void *buffer, size_t size, int width, int height)
{
    rb->memset(bitmap, 0, sizeof(*bitmap));
    bitmap->data = buffer;
    return rb->read_bmp_file(path, bitmap, size, FORMAT_NATIVE, NULL) > 0 &&
           bitmap->width == width && bitmap->height == height;
}

static void tw_load_library(void)
{
    char line[TW_LINE_SIZE];
    int fd;

    creator_count = 0;
    vod_count = 0;
    fd = rb->open(TW_CREATORS, O_RDONLY);
    if (fd >= 0)
    {
        while (creator_count < TW_MAX_CREATORS &&
               rb->read_line(fd, line, sizeof(line)) > 0)
        {
            char *fields[6];
            struct tw_creator *creator;
            int count = tw_split_tabs(line, fields, ARRAYLEN(fields));

            if (count < 5 || !rb->strcmp(fields[0], "creator_key"))
                continue;
            creator = &creators[creator_count++];
            rb->memset(creator, 0, sizeof(*creator));
            rb->strlcpy(creator->key, fields[0], sizeof(creator->key));
            rb->strlcpy(creator->login, fields[1], sizeof(creator->login));
            rb->strlcpy(creator->display_name, fields[2],
                        sizeof(creator->display_name));
            creator->cycle_epoch = rb->strtoul(fields[3], NULL, 10);
            creator->followers = rb->strtoul(fields[4], NULL, 10);
        }
        rb->close(fd);
    }

    fd = rb->open(TW_VODS, O_RDONLY);
    if (fd < 0)
        return;
    while (vod_count < TW_MAX_VODS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[10];
        struct tw_vod *vod;
        int count = tw_split_tabs(line, fields, ARRAYLEN(fields));

        if (count < 10 || !rb->strcmp(fields[0], "id"))
            continue;
        vod = &vods[vod_count++];
        rb->memset(vod, 0, sizeof(*vod));
        rb->strlcpy(vod->id, fields[0], sizeof(vod->id));
        rb->strlcpy(vod->creator_key, fields[1], sizeof(vod->creator_key));
        rb->strlcpy(vod->title, fields[2], sizeof(vod->title));
        rb->strlcpy(vod->game, fields[3], sizeof(vod->game));
        vod->duration = rb->strtoul(fields[4], NULL, 10);
        rb->strlcpy(vod->published, fields[5], sizeof(vod->published));
        rb->strlcpy(vod->views, fields[6], sizeof(vod->views));
        rb->strlcpy(vod->video_path, fields[7], sizeof(vod->video_path));
        rb->strlcpy(vod->thumb_path, fields[8], sizeof(vod->thumb_path));
        rb->strlcpy(vod->description, fields[9], sizeof(vod->description));
    }
    rb->close(fd);
}

static int tw_creator_index(const char *key)
{
    int i;

    for (i = 0; i < creator_count; i++)
        if (!rb->strcmp(creators[i].key, key))
            return i;
    return -1;
}

static int tw_vod_by_path(const char *path)
{
    int i;

    for (i = 0; i < vod_count; i++)
        if (!rb->strcmp(vods[i].video_path, path))
            return i;
    return -1;
}

static int tw_current_vod(int creator_index, unsigned long *offset,
                          unsigned long *programme_start)
{
    const struct tw_creator *creator;
    unsigned long total = 0;
    unsigned long elapsed;
    unsigned long cursor = 0;
    unsigned long now;
    int i;

    if (creator_index < 0 || creator_index >= creator_count)
        return -1;
    creator = &creators[creator_index];
    for (i = 0; i < vod_count; i++)
        if (!rb->strcmp(vods[i].creator_key, creator->key) &&
            vods[i].duration > 0)
            total += vods[i].duration;
    if (total == 0)
        return -1;
    now = (unsigned long)rb->mktime(rb->get_time());
    elapsed = now > creator->cycle_epoch ? now - creator->cycle_epoch : 0;
    elapsed %= total;
    for (i = 0; i < vod_count; i++)
    {
        if (rb->strcmp(vods[i].creator_key, creator->key) ||
            vods[i].duration == 0)
            continue;
        if (elapsed < cursor + vods[i].duration)
        {
            *offset = elapsed - cursor;
            *programme_start = now - *offset;
            return i;
        }
        cursor += vods[i].duration;
    }
    return -1;
}

static bool tw_resolve_video_path(const char *path, char *resolved,
                                  size_t resolved_size)
{
    static const char *extensions[] = { ".m4v", ".mp4", ".mov", ".mpg", NULL };
    char base[MAX_PATH];
    char *dot;
    int i;

    rb->strlcpy(base, path, sizeof(base));
    rb->strlcpy(resolved, path, resolved_size);
    if (rb->file_exists(resolved))
        return true;
    dot = rb->strrchr(base, '.');
    if (dot == NULL)
        return false;
    for (i = 0; extensions[i] != NULL; i++)
    {
        if (!rb->strcasecmp(dot, extensions[i]))
            continue;
        rb->strcpy(dot, extensions[i]);
        rb->strlcpy(resolved, base, resolved_size);
        if (rb->file_exists(resolved))
            return true;
    }
    return false;
}

static void tw_puts_fit(int x, int y, int width, const char *text)
{
    char buffer[128];
    int text_width;
    int height;
    int length;

    rb->strlcpy(buffer, text ? text : "", sizeof(buffer));
    rb->lcd_getstringsize(buffer, &text_width, &height);
    length = rb->strlen(buffer);
    while (text_width > width && length > 3)
    {
        buffer[--length] = '\0';
        rb->lcd_getstringsize(buffer, &text_width, &height);
    }
    if (text && buffer[0] && rb->strlen(text) > rb->strlen(buffer))
    {
        buffer[length - 3] = '.';
        buffer[length - 2] = '.';
        buffer[length - 1] = '.';
    }
    rb->lcd_putsxy(x, y, buffer);
}

static int tw_cached_thumbnail(int vod_index)
{
    int slot;

    for (slot = 0; slot < TW_THUMB_SLOTS; slot++)
        if (thumb_vod[slot] == vod_index)
            return slot;
    return -1;
}

static void tw_request_thumbnail(int vod_index)
{
    if (vod_index < 0 || vod_index >= vod_count ||
        tw_cached_thumbnail(vod_index) >= 0 || thumb_pending == vod_index)
        return;
    thumb_pending = vod_index;
    thumb_due = *rb->current_tick + HZ / 8;
}

static bool tw_service_thumbnail(void)
{
    int slot;

    if (thumb_pending < 0 || !TIME_AFTER(*rb->current_tick, thumb_due) ||
        rb->button_queue_count() != 0)
        return false;
    slot = thumb_next_slot;
    thumb_next_slot = (thumb_next_slot + 1) % TW_THUMB_SLOTS;
    thumb_vod[slot] = thumb_pending;
    thumb_valid[slot] = false;
    if (vods[thumb_pending].thumb_path[0])
        thumb_valid[slot] = tw_load_bitmap(
            vods[thumb_pending].thumb_path, &thumb_bmp[slot],
            thumb_data[slot], sizeof(thumb_data[slot]),
            TW_THUMB_W, TW_THUMB_H);
    thumb_pending = -1;
    return true;
}

static void tw_draw_header(void)
{
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(TW_DARK);
    rb->lcd_set_foreground(TW_DARK);
    rb->lcd_clear_display();
    rb->lcd_set_foreground(TW_PURPLE);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 52);
    if (logo_valid)
        rb->lcd_bitmap((const fb_data *)logo_bmp.data, 7, 1,
                       TW_LOGO_W, TW_LOGO_H);
    /* Rockbox's solid font renderer paints the configured background behind
     * every glyph.  The page intentionally uses several adjacent surfaces,
     * so that produced dark strips through labels drawn over purple/panels.
     * Draw only the font foreground after the opaque logo has been placed. */
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(219, 18, "on iPod");
    rb->lcd_set_foreground(TW_PANEL);
    rb->lcd_fillrect(0, 52, LCD_WIDTH, 22);
    rb->lcd_set_foreground(TW_PURPLE_DARK);
    if (current_page == TW_PAGE_LIVE)
        rb->lcd_fillrect(16, 54, 128, 18);
    else
        rb->lcd_fillrect(198, 54, 68, 18);
    rb->lcd_set_foreground(current_page == TW_PAGE_LIVE ? LCD_WHITE : TW_TEXT);
    rb->lcd_putsxy(32, 58, "LIVE CHANNELS");
    rb->lcd_set_foreground(current_page == TW_PAGE_VODS ? LCD_WHITE : TW_TEXT);
    rb->lcd_putsxy(216, 58, "VODS");
}

static void tw_draw_thumbnail(int vod_index, int x, int y, bool selected)
{
    int slot = tw_cached_thumbnail(vod_index);

    if (slot >= 0 && thumb_valid[slot])
        rb->lcd_bitmap((const fb_data *)thumb_bmp[slot].data, x, y,
                       TW_THUMB_W, TW_THUMB_H);
    else
    {
        rb->lcd_set_foreground(selected ? TW_PURPLE : TW_DARK);
        rb->lcd_fillrect(x, y, TW_THUMB_W, TW_THUMB_H);
        rb->lcd_set_foreground(TW_MUTED);
        rb->lcd_drawrect(x, y, TW_THUMB_W, TW_THUMB_H);
    }
}

static void tw_live_row(int row, int creator_index, bool selected)
{
    struct tw_creator *creator = &creators[creator_index];
    unsigned long offset;
    unsigned long start;
    int vod_index = tw_current_vod(creator_index, &offset, &start);
    int y = TW_LIST_TOP + row * TW_ROW_H;
    char line[64];

    rb->lcd_set_foreground(selected ? TW_PANEL : TW_DARK);
    rb->lcd_fillrect(0, y, LCD_WIDTH, TW_ROW_H - 2);
    if (selected)
    {
        rb->lcd_set_foreground(TW_PURPLE);
        rb->lcd_fillrect(0, y, 4, TW_ROW_H - 2);
    }
    if (vod_index >= 0)
    {
        tw_draw_thumbnail(vod_index, 8, y + 7, selected);
        if (selected)
            tw_request_thumbnail(vod_index);
        rb->lcd_set_foreground(TW_LIVE);
        rb->lcd_fillrect(112, y + 7, 35, 14);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_putsxy(116, y + 10, "LIVE");
        rb->lcd_set_foreground(LCD_WHITE);
        tw_puts_fit(153, y + 8, 159,
                    creator->display_name[0] ? creator->display_name : creator->login);
        rb->lcd_set_foreground(TW_TEXT);
        tw_puts_fit(112, y + 28, 200, vods[vod_index].title);
        rb->snprintf(line, sizeof(line), "%s  %lu:%02lu in",
                     vods[vod_index].game, offset / 60, offset % 60);
        rb->lcd_set_foreground(TW_MUTED);
        tw_puts_fit(112, y + 46, 200, line);
    }
    else
    {
        rb->lcd_set_foreground(TW_TEXT);
        tw_puts_fit(12, y + 18, 298,
                    creator->display_name[0] ? creator->display_name : creator->login);
        rb->lcd_set_foreground(TW_MUTED);
        rb->lcd_putsxy(12, y + 38, "No playable synced VOD");
    }
}

static void tw_vod_row(int row, int vod_index, bool selected)
{
    struct tw_vod *vod = &vods[vod_index];
    int creator_index = tw_creator_index(vod->creator_key);
    int y = TW_LIST_TOP + row * TW_ROW_H;
    char line[80];

    rb->lcd_set_foreground(selected ? TW_PANEL : TW_DARK);
    rb->lcd_fillrect(0, y, LCD_WIDTH, TW_ROW_H - 2);
    if (selected)
    {
        rb->lcd_set_foreground(TW_PURPLE);
        rb->lcd_fillrect(0, y, 4, TW_ROW_H - 2);
        tw_request_thumbnail(vod_index);
    }
    tw_draw_thumbnail(vod_index, 8, y + 7, selected);
    rb->lcd_set_foreground(LCD_WHITE);
    tw_puts_fit(112, y + 7, 200, vod->title);
    rb->snprintf(line, sizeof(line), "%s  %s", vod->game,
                 creator_index >= 0 ? creators[creator_index].display_name : "Twitch");
    rb->lcd_set_foreground(TW_TEXT);
    tw_puts_fit(112, y + 27, 200, line);
    rb->snprintf(line, sizeof(line), "%lu:%02lu   %s views",
                 vod->duration / 60, vod->duration % 60, vod->views);
    rb->lcd_set_foreground(TW_MUTED);
    tw_puts_fit(112, y + 46, 200, line);
}

static void tw_draw_browser(void)
{
    int count = current_page == TW_PAGE_LIVE ? creator_count : vod_count;
    int first;
    int shown;
    int row;

    tw_draw_header();
    if (count <= 0)
    {
        rb->lcd_set_foreground(TW_TEXT);
        rb->lcd_putsxy(75, 119, current_page == TW_PAGE_LIVE ?
                       "No creators synced" : "No Twitch VODs synced");
        rb->lcd_set_foreground(TW_MUTED);
        rb->lcd_putsxy(53, 143, "Connect to RockPod and Sync Twitch");
    }
    else
    {
        selection[current_page] = MAX(0, MIN(selection[current_page], count - 1));
        first = MAX(0, MIN(selection[current_page], count - TW_VISIBLE_ROWS));
        shown = MIN(TW_VISIBLE_ROWS, count - first);
        for (row = 0; row < shown; row++)
        {
            int index = first + row;

            if (current_page == TW_PAGE_LIVE)
                tw_live_row(row, index, index == selection[current_page]);
            else
                tw_vod_row(row, index, index == selection[current_page]);
        }
    }
    rb->lcd_set_foreground(TW_PANEL);
    rb->lcd_fillrect(0, 216, LCD_WIDTH, 24);
    rb->lcd_set_foreground(TW_MUTED);
    rb->lcd_putsxy(8, 222, "MENU Exit");
    rb->lcd_putsxy(126, 222, "PLAY Watch");
    rb->lcd_putsxy(260, 222, "SELECT");
    rb->lcd_update();
}

static void tw_draw_button(int x, int y, int width, const char *label,
                           bool selected)
{
    int text_width;
    int height;

    rb->lcd_set_foreground(selected ? TW_PURPLE : TW_PANEL);
    rb->lcd_fillrect(x, y, width, 22);
    rb->lcd_set_foreground(selected ? TW_PURPLE_LIGHT : TW_MUTED);
    rb->lcd_drawrect(x, y, width, 22);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_getstringsize(label, &text_width, &height);
    rb->lcd_putsxy(x + (width - text_width) / 2, y + 6, label);
}

static void tw_draw_detail(void)
{
    int vod_index = detail_vod;
    int creator_index;
    unsigned long offset = 0;
    unsigned long start = 0;
    bool live = current_page == TW_PAGE_LIVE;
    char line[96];

    if (live)
        vod_index = tw_current_vod(detail_creator, &offset, &start);
    tw_draw_header();
    if (vod_index < 0)
    {
        rb->lcd_set_foreground(TW_TEXT);
        rb->lcd_putsxy(66, 120, "No playable VOD for this creator");
    }
    else
    {
        creator_index = tw_creator_index(vods[vod_index].creator_key);
        tw_request_thumbnail(vod_index);
        tw_draw_thumbnail(vod_index, 8, 82, true);
        rb->lcd_set_foreground(LCD_WHITE);
        tw_puts_fit(112, 82, 200, vods[vod_index].title);
        rb->lcd_set_foreground(TW_PURPLE_LIGHT);
        tw_puts_fit(112, 101, 200, creator_index >= 0 ?
                    creators[creator_index].display_name : "Twitch");
        rb->lcd_set_foreground(TW_TEXT);
        tw_puts_fit(112, 120, 200, vods[vod_index].game);
        if (live)
            rb->snprintf(line, sizeof(line), "LIVE  joined at %lu:%02lu",
                         offset / 60, offset % 60);
        else
            rb->snprintf(line, sizeof(line), "%lu:%02lu  %s views",
                         vods[vod_index].duration / 60,
                         vods[vod_index].duration % 60,
                         vods[vod_index].views);
        rb->lcd_set_foreground(live ? TW_LIVE : TW_MUTED);
        tw_puts_fit(112, 139, 200, line);
        rb->lcd_set_foreground(TW_MUTED);
        tw_puts_fit(8, 159, 304, vods[vod_index].description);
    }
    tw_draw_button(62, 187, 92, live ? "Watch LIVE" : "Watch",
                   detail_action == 0);
    tw_draw_button(166, 187, 92, "Back", detail_action == 1);
    rb->lcd_set_foreground(TW_MUTED);
    rb->lcd_putsxy(8, 222, "MENU Back");
    rb->lcd_putsxy(225, 222, "SELECT");
    rb->lcd_update();
}

static enum plugin_status tw_play(int creator_index, int vod_index, bool live)
{
    static char launch[MAX_PATH + 80];
    char path[MAX_PATH];
    unsigned long offset = 0;
    unsigned long start = 0;

    if (live)
        vod_index = tw_current_vod(creator_index, &offset, &start);
    if (vod_index < 0 ||
        !tw_resolve_video_path(vods[vod_index].video_path, path, sizeof(path)))
    {
        rb->splash(HZ * 2, "Twitch VOD Not Found");
        return PLUGIN_OK;
    }
    if (live)
        rb->snprintf(launch, sizeof(launch), "%s%lu:%s:%s",
                     TW_LIVE_PREFIX, start, creators[creator_index].key, path);
    else
        rb->snprintf(launch, sizeof(launch), "%s%s", TW_PLAYER_PREFIX, path);
    return rb->plugin_open(plugin_video_player_for(path), launch);
}

static enum plugin_status tw_activate(void)
{
    if (!detail_screen)
    {
        if ((current_page == TW_PAGE_LIVE && creator_count <= 0) ||
            (current_page == TW_PAGE_VODS && vod_count <= 0))
            return PLUGIN_OK;
        detail_screen = true;
        detail_action = 0;
        if (current_page == TW_PAGE_LIVE)
            detail_creator = selection[current_page];
        else
            detail_vod = selection[current_page];
        return PLUGIN_OK;
    }
    if (detail_action == 1)
    {
        detail_screen = false;
        return PLUGIN_OK;
    }
    return tw_play(detail_creator, detail_vod, current_page == TW_PAGE_LIVE);
}

static void tw_restore_lcd(void)
{
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(rb->global_settings->bg_color);
    rb->lcd_set_foreground(rb->global_settings->fg_color);
    rb->lcd_setfont(FONT_UI);
}

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status status = PLUGIN_OK;
    bool redraw = true;
    bool continue_live = false;
    int i;

#if defined(HAVE_LCD_MODES) && (HAVE_LCD_MODES & LCD_MODE_RGB565)
    rb->lcd_set_mode(LCD_MODE_RGB565);
#endif
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_backdrop(NULL);
    tw_load_library();
    logo_valid = tw_load_bitmap(TW_LOGO, &logo_bmp, logo_data,
                                sizeof(logo_data), TW_LOGO_W, TW_LOGO_H);
    for (i = 0; i < TW_THUMB_SLOTS; i++)
    {
        thumb_vod[i] = -1;
        thumb_valid[i] = false;
    }
    thumb_next_slot = 0;
    thumb_pending = -1;
    current_page = TW_PAGE_LIVE;
    selection[TW_PAGE_LIVE] = 0;
    selection[TW_PAGE_VODS] = 0;
    detail_screen = false;
    detail_creator = -1;
    detail_vod = -1;
    detail_action = 0;

    if (parameter != NULL &&
        !rb->strncmp((const char *)parameter, "continue:", 9))
    {
        detail_creator = tw_creator_index((const char *)parameter + 9);
        if (detail_creator >= 0)
        {
            selection[TW_PAGE_LIVE] = detail_creator;
            continue_live = true;
        }
    }
    else if (parameter != NULL &&
             !rb->strncmp((const char *)parameter, "return-live:", 12))
    {
        detail_creator = tw_creator_index((const char *)parameter + 12);
        if (detail_creator >= 0)
        {
            current_page = TW_PAGE_LIVE;
            selection[TW_PAGE_LIVE] = detail_creator;
            detail_screen = true;
        }
    }
    else if (parameter != NULL &&
             !rb->strncmp((const char *)parameter, "return:", 7))
    {
        detail_vod = tw_vod_by_path((const char *)parameter + 7);
        if (detail_vod >= 0)
        {
            detail_creator = tw_creator_index(vods[detail_vod].creator_key);
            current_page = TW_PAGE_VODS;
            selection[TW_PAGE_VODS] = detail_vod;
            detail_screen = true;
        }
    }

    if (continue_live)
    {
        status = tw_play(detail_creator, -1, true);
        if (status == PLUGIN_GOTO_PLUGIN)
            return status;
    }

    while (true)
    {
        int action;

        if (redraw)
        {
            if (detail_screen)
                tw_draw_detail();
            else
                tw_draw_browser();
            redraw = false;
        }
        action = pluginlib_getaction_remote(HZ / 10, plugin_contexts,
                                     ARRAYLEN(plugin_contexts));
        if (action == ACTION_NONE && tw_service_thumbnail())
        {
            redraw = true;
            continue;
        }
        switch (action)
        {
        case PLA_SCROLL_BACK:
        case PLA_SCROLL_BACK_REPEAT:
        case PLA_UP:
        case PLA_UP_REPEAT:
            if (detail_screen)
                detail_action = 0;
            else
                selection[current_page] = MAX(0, selection[current_page] - 1);
            redraw = true;
            break;
        case PLA_SCROLL_FWD:
        case PLA_SCROLL_FWD_REPEAT:
        case PLA_DOWN:
        case PLA_DOWN_REPEAT:
            if (detail_screen)
                detail_action = 1;
            else
            {
                int count = current_page == TW_PAGE_LIVE ? creator_count : vod_count;
                selection[current_page] = MIN(MAX(0, count - 1),
                                              selection[current_page] + 1);
            }
            redraw = true;
            break;
        case PLA_LEFT:
        case PLA_LEFT_REPEAT:
            if (detail_screen)
                detail_action = 0;
            else
                current_page = TW_PAGE_LIVE;
            redraw = true;
            break;
        case PLA_RIGHT:
        case PLA_RIGHT_REPEAT:
            if (detail_screen)
                detail_action = 1;
            else
                current_page = TW_PAGE_VODS;
            redraw = true;
            break;
        case PLA_SELECT_REL:
        {
            enum plugin_status result = tw_activate();
            if (result == PLUGIN_GOTO_PLUGIN)
                return result;
            redraw = true;
            break;
        }
        case PLA_EXIT:
        {
            enum plugin_status result;
            if (detail_screen)
                result = tw_play(detail_creator, detail_vod,
                                 current_page == TW_PAGE_LIVE);
            else if (current_page == TW_PAGE_LIVE)
                result = tw_play(selection[current_page], -1, true);
            else
                result = tw_play(-1, selection[current_page], false);
            if (result == PLUGIN_GOTO_PLUGIN)
                return result;
            redraw = true;
            break;
        }
        case ACTION_STD_MENU:
            tw_restore_lcd();
            return status;
        case PLA_CANCEL:
            if (detail_screen)
            {
                detail_screen = false;
                redraw = true;
            }
            else
            {
                tw_restore_lcd();
                return status;
            }
            break;
        default:
            if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
            {
                tw_restore_lcd();
                return PLUGIN_USB_CONNECTED;
            }
            break;
        }
    }
}
