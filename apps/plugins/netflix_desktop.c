/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__ /  \
 *                     \/            \/     \/    \/            \/
 *
 * Netflix window for the 1920x1080 Desktop Mode host profile.
 *
 * Copyright (C) 2026 David Cor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "lib/pluginlib_bmp.h"

#if LCD_WIDTH < 1920
#error netflix_desktop is only supported by the desktop1080 profile
#endif

static const struct button_mapping *plugin_contexts[] = { pla_main_ctx };

#define NF_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_netflix_underlay.raw"
#define NF_UNDERLAY_MAGIC 0x444e4631u /* "DNF1" */
#define NF_WINDOW_FILE \
    PLUGIN_APPS_DATA_DIR \
    "/desktop_mode_snow_leopard/1920x1080/chrome/" \
    "window-plain.897x671x16.bmp"
#define NF_LOGO_FILE \
    ROCKBOX_DIR "/ipodjs/netflix/netflix-logo-2001.150x70x24.bmp"
#define NF_VIDEO_INDEX ROCKBOX_DIR "/videolist/index.tsv"
#define NF_WATCHED_FILE ROCKBOX_DIR "/videolist/netflix-watched.tsv"

#define NF_WIN_W 897
#define NF_WIN_H 671
#define NF_WIN_X ((LCD_WIDTH - NF_WIN_W) / 2)
#define NF_WIN_Y 170
#define NF_TITLE_H 24
#define NF_TOOLBAR_H 29
#define NF_STATUS_H 24
#define NF_BODY_X (NF_WIN_X + 1)
#define NF_BODY_Y (NF_WIN_Y + NF_TITLE_H + NF_TOOLBAR_H)
#define NF_BODY_W (NF_WIN_W - 2)
#define NF_BODY_H \
    (NF_WIN_H - NF_TITLE_H - NF_TOOLBAR_H - NF_STATUS_H)
#define NF_HEADER_H 72

#define NF_MAX_ROWS 96
#define NF_VISIBLE 5
#define NF_POSTER_W 96
#define NF_POSTER_H 144
#define NF_HERO_W (NF_POSTER_W * 2)
#define NF_HERO_H (NF_POSTER_H * 2)
#define NF_CARD_STEP 110
#define NF_CARD_X ((LCD_WIDTH - NF_VISIBLE * NF_CARD_STEP) / 2)
#define NF_CARD_Y (NF_BODY_Y + 412)
#define NF_PLAY_X (NF_BODY_X + 252)
#define NF_PLAY_Y (NF_BODY_Y + 282)
#define NF_PLAY_W 126
#define NF_PLAY_H 36

#define NF_RED LCD_RGBPACK(180, 19, 29)
#define NF_RED_DARK LCD_RGBPACK(112, 12, 18)
#define NF_BLACK LCD_RGBPACK(10, 10, 10)
#define NF_PANEL LCD_RGBPACK(25, 25, 25)
#define NF_WHITE LCD_RGBPACK(245, 245, 245)
#define NF_DIM LCD_RGBPACK(178, 178, 178)
#define NF_RULE LCD_RGBPACK(70, 70, 70)

enum nf_kind
{
    NF_KIND_MOVIE = 0,
    NF_KIND_SHOW,
    NF_KIND_MUSIC_VIDEO,
    NF_KIND_HOME_VIDEO,
};

enum nf_category
{
    NF_CATEGORY_HOME = 0,
    NF_CATEGORY_MOVIES,
    NF_CATEGORY_SHOWS,
    NF_CATEGORY_MUSIC,
    NF_CATEGORY_HOME_VIDEOS,
    NF_CATEGORY_COUNT,
};

struct nf_row
{
    char title[96];
    char show[64];
    char genre[64];
    char plot[192];
    char path[MAX_PATH];
    char poster[MAX_PATH];
    int year;
    int season;
    int episode;
    enum nf_kind kind;
    bool locked;
    bool watched;
};

struct nf_cache
{
    fb_data *pixels;
    int row;
    bool valid;
};

static struct nf_row nf_rows[NF_MAX_ROWS];
static struct nf_cache nf_posters[NF_VISIBLE];
static int nf_row_count;
static int nf_excluded_count;
static int nf_selected = -1;
static int nf_visible[NF_VISIBLE];
static int nf_visible_count;
static enum nf_category nf_category = NF_CATEGORY_HOME;

static unsigned char *nf_arena;
static size_t nf_arena_left;
static fb_data *nf_underlay;
static fb_data *nf_window;
static fb_data *nf_logo;
static fb_data *nf_hero;
static bool nf_logo_valid;
static bool nf_hero_valid;

static int nf_pointer_x;
static int nf_pointer_y;
static unsigned int nf_pointer_buttons;
static bool nf_pointer_active;
static long nf_pointer_tick;

static const char * const nf_category_names[NF_CATEGORY_COUNT] =
{
    "HOME", "MOVIES", "TV SHOWS", "MUSIC VIDEOS", "HOME VIDEOS"
};

static void *nf_alloc(size_t bytes)
{
    size_t aligned = ALIGN_UP(bytes, 4);
    void *result;

    if (!nf_arena || aligned > nf_arena_left)
        return NULL;
    result = nf_arena;
    nf_arena += aligned;
    nf_arena_left -= aligned;
    return result;
}

static bool nf_read_all(int fd, void *buffer, size_t size)
{
    unsigned char *cursor = buffer;

    while (size > 0)
    {
        ssize_t got = rb->read(fd, cursor, size);

        if (got <= 0)
            return false;
        cursor += got;
        size -= got;
    }
    return true;
}

static bool nf_load_bitmap(const char *path, fb_data *pixels,
                           int width, int height)
{
    struct bitmap bitmap;
    size_t bytes = BM_SIZE(width, height, FORMAT_NATIVE, false);

    rb->memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = width;
    bitmap.height = height;
    bitmap.format = FORMAT_NATIVE;
    bitmap.data = (unsigned char *)pixels;
    return rb->read_bmp_file(path, &bitmap, bytes, FORMAT_NATIVE, NULL) > 0 &&
           bitmap.width == width && bitmap.height == height;
}

static bool nf_init_desktop(void)
{
    uint32_t header[3];
    size_t size;
    size_t screen_bytes =
        (size_t)LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data);
    int index;
    int fd;

    nf_arena = rb->plugin_get_buffer(&size);
    nf_arena_left = size;
    nf_underlay = nf_alloc(screen_bytes);
    nf_window = nf_alloc(BM_SIZE(NF_WIN_W, NF_WIN_H,
                                 FORMAT_NATIVE, false));
    nf_logo = nf_alloc(BM_SIZE(150, 70, FORMAT_NATIVE, false));
    nf_hero = nf_alloc(BM_SIZE(NF_HERO_W, NF_HERO_H,
                               FORMAT_NATIVE, false));
    for (index = 0; index < NF_VISIBLE; ++index)
    {
        nf_posters[index].pixels =
            nf_alloc(BM_SIZE(NF_POSTER_W, NF_POSTER_H,
                             FORMAT_NATIVE, false));
        nf_posters[index].row = -1;
    }
    if (!nf_underlay || !nf_window || !nf_logo || !nf_hero)
        return false;
    for (index = 0; index < NF_VISIBLE; ++index)
        if (!nf_posters[index].pixels)
            return false;

    fd = rb->open(NF_UNDERLAY_FILE, O_RDONLY);
    if (fd < 0)
        return false;
    if (!nf_read_all(fd, header, sizeof(header)) ||
        header[0] != NF_UNDERLAY_MAGIC ||
        header[1] != LCD_WIDTH || header[2] != LCD_HEIGHT ||
        !nf_read_all(fd, nf_underlay, screen_bytes))
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);

    if (!nf_load_bitmap(NF_WINDOW_FILE, nf_window, NF_WIN_W, NF_WIN_H))
        return false;
    nf_logo_valid = nf_load_bitmap(NF_LOGO_FILE, nf_logo, 150, 70);
    return true;
}

static int nf_split_tabs(char *line, char **fields, int maximum)
{
    int count = 0;
    char *cursor = line;

    while (count < maximum)
    {
        char *tab;

        fields[count++] = cursor;
        tab = rb->strchr(cursor, '\t');
        if (!tab)
            break;
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static bool nf_contains_ci(const char *text, const char *needle)
{
    size_t length = rb->strlen(needle);

    if (length == 0)
        return true;
    while (*text)
    {
        if (!rb->strncasecmp(text, needle, length))
            return true;
        text++;
    }
    return false;
}

/* Netflix is deliberately backed only by Video Sync. Live TV is a separate
 * application and YouTube syncs land in Videos/Downloaded. Keep both out even
 * if either source is accidentally appended to a future videolist manifest. */
static bool nf_excluded_source(char **fields)
{
    const char *path = fields[6];
    const char *kind = fields[4];
    const char *group = fields[5];

    return nf_contains_ci(path, "/livetv/") ||
           nf_contains_ci(path, "/live/") ||
           nf_contains_ci(path, "/youtube/") ||
           nf_contains_ci(path, "/downloaded/") ||
           nf_contains_ci(kind, "youtube") ||
           nf_contains_ci(kind, "live_tv") ||
           nf_contains_ci(group, "youtube") ||
           nf_contains_ci(group, "livetv");
}

static enum nf_kind nf_parse_kind(const char *kind)
{
    if (!rb->strcasecmp(kind, "show"))
        return NF_KIND_SHOW;
    if (!rb->strcasecmp(kind, "music_video"))
        return NF_KIND_MUSIC_VIDEO;
    if (!rb->strcasecmp(kind, "home_video"))
        return NF_KIND_HOME_VIDEO;
    return NF_KIND_MOVIE;
}

static const char *nf_category_poster(enum nf_kind kind, bool locked)
{
    if (locked)
        return ROCKBOX_DIR "/ipodjs/netflix/locked.96x144x24.bmp";
    switch (kind)
    {
        case NF_KIND_SHOW:
            return ROCKBOX_DIR
                "/ipodjs/netflix/categories/tv-shows.96x144x24.bmp";
        case NF_KIND_MUSIC_VIDEO:
            return ROCKBOX_DIR
                "/ipodjs/netflix/categories/music-videos.96x144x24.bmp";
        case NF_KIND_HOME_VIDEO:
            return ROCKBOX_DIR
                "/ipodjs/netflix/categories/home-videos.96x144x24.bmp";
        default:
            return ROCKBOX_DIR
                "/ipodjs/netflix/categories/movies.96x144x24.bmp";
    }
}

static void nf_manifest_path(char *destination, size_t size,
                             const char *path)
{
    if (path[0] == '/')
        rb->strlcpy(destination, path, size);
    else
        rb->snprintf(destination, size, "/%s", path);
}

static void nf_poster_path(char *destination, size_t size,
                           const char *relative)
{
    if (!relative[0])
    {
        destination[0] = '\0';
        return;
    }
    if (relative[0] == '/')
        rb->strlcpy(destination, relative, size);
    else
        rb->snprintf(destination, size, "%s/%s",
                     ROCKBOX_DIR "/videolist", relative);
}

static void nf_load_manifest(void)
{
    char line[1024];
    int fd = rb->open(NF_VIDEO_INDEX, O_RDONLY);

    nf_row_count = 0;
    nf_excluded_count = 0;
    if (fd < 0)
        return;
    while (nf_row_count < NF_MAX_ROWS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[23];
        int count;
        struct nf_row *row;
        const char *poster;

        if (!line[0] || line[0] == '#' ||
            !rb->strncmp(line, "video_id\t", 9))
            continue;
        count = nf_split_tabs(line, fields, ARRAYLEN(fields));
        if (count < 12 || !fields[6][0])
            continue;
        if (nf_excluded_source(fields))
        {
            nf_excluded_count++;
            continue;
        }

        row = &nf_rows[nf_row_count++];
        rb->memset(row, 0, sizeof(*row));
        row->kind = nf_parse_kind(fields[4]);
        row->locked = rb->atoi(fields[11]) != 0;
        rb->strlcpy(row->title,
                    row->locked ? "Locked title" :
                    (fields[3][0] ? fields[3] : "Untitled video"),
                    sizeof(row->title));
        if (!row->locked && count > 7)
            rb->strlcpy(row->show, fields[7], sizeof(row->show));
        if (!row->locked && count > 13)
            rb->strlcpy(row->genre, fields[13], sizeof(row->genre));
        if (!row->locked && count > 15)
            rb->strlcpy(row->plot, fields[15], sizeof(row->plot));
        if (!row->plot[0] && !row->locked && count > 16)
            rb->strlcpy(row->plot, fields[16], sizeof(row->plot));
        if (!row->plot[0] && !row->locked && count > 22)
            rb->strlcpy(row->plot, fields[22], sizeof(row->plot));
        row->year = count > 12 ? rb->atoi(fields[12]) : 0;
        row->season = count > 8 ? rb->atoi(fields[8]) : 0;
        row->episode = count > 9 ? rb->atoi(fields[9]) : 0;
        nf_manifest_path(row->path, sizeof(row->path), fields[6]);
        poster = count > 19 && fields[19][0] ? fields[19] :
                 count > 18 ? fields[18] : "";
        nf_poster_path(row->poster, sizeof(row->poster), poster);
        if (!row->poster[0])
            rb->strlcpy(row->poster,
                        nf_category_poster(row->kind, row->locked),
                        sizeof(row->poster));
    }
    rb->close(fd);
}

static void nf_load_watched(void)
{
    char line[MAX_PATH];
    int fd = rb->open(NF_WATCHED_FILE, O_RDONLY);

    if (fd < 0)
        return;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char path[MAX_PATH];
        int index;

        if (!line[0] || line[0] == '#')
            continue;
        nf_manifest_path(path, sizeof(path), line);
        for (index = 0; index < nf_row_count; ++index)
            if (!rb->strcasecmp(path, nf_rows[index].path))
                nf_rows[index].watched = true;
    }
    rb->close(fd);
}

static bool nf_category_matches(int row)
{
    if (row < 0 || row >= nf_row_count)
        return false;
    switch (nf_category)
    {
        case NF_CATEGORY_MOVIES:
            return nf_rows[row].kind == NF_KIND_MOVIE;
        case NF_CATEGORY_SHOWS:
            return nf_rows[row].kind == NF_KIND_SHOW;
        case NF_CATEGORY_MUSIC:
            return nf_rows[row].kind == NF_KIND_MUSIC_VIDEO;
        case NF_CATEGORY_HOME_VIDEOS:
            return nf_rows[row].kind == NF_KIND_HOME_VIDEO;
        default:
            return true;
    }
}

static int nf_matching_rows(int *matches)
{
    int count = 0;
    int row;

    for (row = 0; row < nf_row_count; ++row)
        if (nf_category_matches(row))
            matches[count++] = row;
    return count;
}

static void nf_fill_placeholder(fb_data *pixels)
{
    int index;

    for (index = 0; index < NF_POSTER_W * NF_POSTER_H; ++index)
        pixels[index] = (index / NF_POSTER_W < 8) ? NF_RED : NF_PANEL;
}

static void nf_scale_hero(const fb_data *source)
{
    int x;
    int y;

    for (y = 0; y < NF_HERO_H; ++y)
        for (x = 0; x < NF_HERO_W; ++x)
            nf_hero[y * NF_HERO_W + x] =
                source[(y / 2) * NF_POSTER_W + x / 2];
    nf_hero_valid = true;
}

static void nf_cache_visible(void)
{
    int matches[NF_MAX_ROWS];
    int count = nf_matching_rows(matches);
    int position = 0;
    int start;
    int slot;

    nf_visible_count = 0;
    nf_hero_valid = false;
    if (count == 0)
    {
        nf_selected = -1;
        return;
    }
    while (position < count && matches[position] != nf_selected)
        position++;
    if (position >= count)
    {
        position = 0;
        nf_selected = matches[0];
    }
    start = MAX(0, MIN(position - NF_VISIBLE / 2,
                       MAX(0, count - NF_VISIBLE)));
    for (slot = 0; slot < NF_VISIBLE && start + slot < count; ++slot)
    {
        int row = matches[start + slot];
        struct nf_cache *cache = &nf_posters[slot];

        nf_visible[nf_visible_count++] = row;
        cache->row = row;
        cache->valid =
            nf_load_bitmap(nf_rows[row].poster, cache->pixels,
                           NF_POSTER_W, NF_POSTER_H);
        if (!cache->valid)
        {
            const char *fallback =
                nf_category_poster(nf_rows[row].kind, nf_rows[row].locked);

            cache->valid =
                nf_load_bitmap(fallback, cache->pixels,
                               NF_POSTER_W, NF_POSTER_H);
        }
        if (!cache->valid)
            nf_fill_placeholder(cache->pixels);
        if (row == nf_selected)
            nf_scale_hero(cache->pixels);
    }
}

static void nf_select_step(int direction)
{
    int matches[NF_MAX_ROWS];
    int count = nf_matching_rows(matches);
    int position = 0;

    if (count == 0)
        return;
    while (position < count && matches[position] != nf_selected)
        position++;
    if (position >= count)
        position = 0;
    position = (position + direction + count) % count;
    nf_selected = matches[position];
    DEBUGF("netflix desktop: selection step=%d selected=%d\n",
           direction, nf_selected);
    nf_cache_visible();
}

static void nf_set_category(enum nf_category category)
{
    int row;

    nf_category = category;
    nf_selected = -1;
    for (row = 0; row < nf_row_count; ++row)
    {
        if (nf_category_matches(row))
        {
            nf_selected = row;
            break;
        }
    }
    DEBUGF("netflix desktop: category=%s selected=%d\n",
           nf_category_names[nf_category], nf_selected);
    nf_cache_visible();
}

static void nf_text(int x, int y, fb_data color, const char *text)
{
    /* Text belongs to the surface already drawn underneath it.  Using the
     * default solid draw mode paints the current LCD background colour
     * behind every glyph, which looks like a black strip across the red
     * navigation bar and Aqua chrome. */
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(color);
    rb->lcd_putsxy(x, y, (const unsigned char *)text);
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static void nf_center_text(int x, int y, int width, fb_data color,
                           const char *text)
{
    int text_w;
    int text_h;

    rb->lcd_getstringsize((const unsigned char *)text, &text_w, &text_h);
    nf_text(x + MAX(0, (width - text_w) / 2), y, color, text);
}

static void nf_wrapped_text(int x, int y, int width, int lines,
                            fb_data color, const char *text)
{
    char work[192];
    char line[96];
    char *cursor;
    int row = 0;

    rb->strlcpy(work, text, sizeof(work));
    cursor = work;
    while (*cursor && row < lines)
    {
        char *word = cursor;
        int used = 0;

        line[0] = '\0';
        while (*word)
        {
            char *space = rb->strchr(word, ' ');
            int piece = space ? (int)(space - word) :
                                (int)rb->strlen(word);
            int text_w;
            int text_h;

            if (used && used + piece + 1 < (int)sizeof(line))
                line[used++] = ' ';
            if (used + piece >= (int)sizeof(line))
                break;
            rb->memcpy(line + used, word, piece);
            used += piece;
            line[used] = '\0';
            rb->lcd_getstringsize((const unsigned char *)line,
                                  &text_w, &text_h);
            if (text_w > width)
            {
                used -= piece + (used > piece ? 1 : 0);
                line[MAX(0, used)] = '\0';
                break;
            }
            if (!space)
            {
                word += piece;
                break;
            }
            word = space + 1;
        }
        if (!line[0])
            break;
        nf_text(x, y + row * 18, color, line);
        cursor = word;
        while (*cursor == ' ')
            cursor++;
        row++;
    }
}

static const char *nf_kind_label(enum nf_kind kind)
{
    switch (kind)
    {
        case NF_KIND_SHOW:
            return "TV SHOW";
        case NF_KIND_MUSIC_VIDEO:
            return "MUSIC VIDEO";
        case NF_KIND_HOME_VIDEO:
            return "HOME VIDEO";
        default:
            return "MOVIE";
    }
}

static bool nf_point_in(int x, int y, int left, int top,
                        int width, int height)
{
    return x >= left && x < left + width &&
           y >= top && y < top + height;
}

static bool nf_pointer_in(int left, int top, int width, int height)
{
    return nf_pointer_active &&
           nf_point_in(nf_pointer_x, nf_pointer_y,
                       left, top, width, height);
}

static void nf_draw_pointer(void)
{
    int row;

    if (!nf_pointer_active)
        return;
    rb->lcd_set_foreground(LCD_RGBPACK(0, 0, 0));
    for (row = 0; row < 18; ++row)
        rb->lcd_hline(nf_pointer_x, nf_pointer_x + row / 2 + 1,
                      nf_pointer_y + row);
    rb->lcd_set_foreground(NF_WHITE);
    for (row = 1; row < 15; ++row)
        rb->lcd_hline(nf_pointer_x + 1,
                      nf_pointer_x + row / 2, nf_pointer_y + row);
}

static void nf_draw(void)
{
    char metadata[128];
    char status[96];
    int index;

    rb->lcd_bitmap(nf_underlay, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rb->lcd_bitmap(nf_window, NF_WIN_X, NF_WIN_Y, NF_WIN_W, NF_WIN_H);
    nf_center_text(NF_WIN_X, NF_WIN_Y + 5, NF_WIN_W,
                   LCD_RGBPACK(60, 60, 60), "Netflix - Video Sync");

    rb->lcd_set_foreground(NF_BLACK);
    rb->lcd_fillrect(NF_BODY_X, NF_BODY_Y, NF_BODY_W, NF_BODY_H);
    rb->lcd_set_foreground(NF_RED);
    rb->lcd_fillrect(NF_BODY_X, NF_BODY_Y, NF_BODY_W, NF_HEADER_H);
    if (nf_logo_valid)
        rb->lcd_bitmap(nf_logo, NF_BODY_X + 12, NF_BODY_Y + 1, 150, 70);
    for (index = 0; index < NF_CATEGORY_COUNT; ++index)
    {
        int x = NF_BODY_X + 180 + index * 136;
        bool selected = index == (int)nf_category;
        bool hovered = nf_pointer_in(x - 7, NF_BODY_Y + 20, 130, 32);
        fb_data ink = selected || hovered ? NF_WHITE :
                                           LCD_RGBPACK(45, 0, 0);

        if (selected || hovered)
        {
            rb->lcd_set_foreground(selected ? NF_RED_DARK :
                                              LCD_RGBPACK(150, 16, 24));
            rb->lcd_fillrect(x - 7, NF_BODY_Y + 22, 130, 28);
        }
        nf_center_text(x - 7, NF_BODY_Y + 30, 130, ink,
                       nf_category_names[index]);
    }

    if (nf_selected >= 0)
    {
        const struct nf_row *row = &nf_rows[nf_selected];
        int poster_x = NF_BODY_X + 34;
        int poster_y = NF_BODY_Y + 88;
        int text_x = NF_BODY_X + 252;

        rb->lcd_set_foreground(NF_RULE);
        rb->lcd_fillrect(poster_x - 4, poster_y - 4,
                         NF_HERO_W + 8, NF_HERO_H + 8);
        if (nf_hero_valid)
            rb->lcd_bitmap(nf_hero, poster_x, poster_y,
                           NF_HERO_W, NF_HERO_H);
        nf_text(text_x, poster_y + 4, NF_WHITE, row->title);
        if (row->show[0])
            nf_text(text_x, poster_y + 30, NF_DIM, row->show);
        rb->snprintf(metadata, sizeof(metadata), "%s%s%s%d%s%s",
                     nf_kind_label(row->kind),
                     row->year ? "  |  " : "",
                     row->year ? "" : "",
                     row->year,
                     row->genre[0] ? "  |  " : "",
                     row->genre);
        if (!row->year)
            rb->snprintf(metadata, sizeof(metadata), "%s%s%s",
                         nf_kind_label(row->kind),
                         row->genre[0] ? "  |  " : "", row->genre);
        nf_text(text_x, poster_y + 54, NF_DIM, metadata);
        if (row->kind == NF_KIND_SHOW && row->season > 0)
        {
            rb->snprintf(metadata, sizeof(metadata),
                         "Season %d, Episode %d",
                         row->season, row->episode);
            nf_text(text_x, poster_y + 78, NF_DIM, metadata);
        }
        nf_wrapped_text(text_x, poster_y + 110, NF_BODY_W - 290, 6,
                        NF_WHITE,
                        row->plot[0] ? row->plot :
                        "Synced from your iPod video library.");
        rb->lcd_set_foreground(
            nf_pointer_in(NF_PLAY_X, NF_PLAY_Y, NF_PLAY_W, NF_PLAY_H) &&
            !row->locked ? LCD_RGBPACK(222, 28, 40) :
            row->locked ? NF_RULE : NF_RED);
        rb->lcd_fillrect(NF_PLAY_X, NF_PLAY_Y, NF_PLAY_W, NF_PLAY_H);
        nf_center_text(NF_PLAY_X, NF_PLAY_Y + 11, NF_PLAY_W, NF_WHITE,
                       row->locked ? "LOCKED" : "PLAY");
        if (row->watched)
            nf_text(NF_PLAY_X + NF_PLAY_W + 20, NF_PLAY_Y + 11,
                    NF_RED, "WATCHED");
    }
    else
    {
        nf_center_text(NF_BODY_X, NF_BODY_Y + 250, NF_BODY_W, NF_WHITE,
                       "No matching Video Sync titles");
    }

    nf_text(NF_BODY_X + 34, NF_BODY_Y + 388, NF_WHITE,
            "POPULAR ON YOUR IPOD");
    for (index = 0; index < nf_visible_count; ++index)
    {
        int x = NF_CARD_X + index * NF_CARD_STEP;
        bool selected = nf_visible[index] == nf_selected;
        bool hovered = nf_pointer_in(x - 3, NF_CARD_Y - 3,
                                    NF_POSTER_W + 6, NF_POSTER_H + 6);

        rb->lcd_set_foreground(hovered ? NF_WHITE :
                               selected ? NF_RED : NF_RULE);
        rb->lcd_fillrect(x - 3, NF_CARD_Y - 3,
                         NF_POSTER_W + 6, NF_POSTER_H + 6);
        rb->lcd_bitmap(nf_posters[index].pixels, x, NF_CARD_Y,
                       NF_POSTER_W, NF_POSTER_H);
    }
    if (nf_pointer_in(NF_CARD_X - 50, NF_CARD_Y + 45, 40, 60))
    {
        rb->lcd_set_foreground(NF_RED_DARK);
        rb->lcd_fillrect(NF_CARD_X - 50, NF_CARD_Y + 45, 40, 60);
    }
    if (nf_pointer_in(NF_CARD_X + NF_VISIBLE * NF_CARD_STEP,
                      NF_CARD_Y + 45, 50, 60))
    {
        rb->lcd_set_foreground(NF_RED_DARK);
        rb->lcd_fillrect(NF_CARD_X + NF_VISIBLE * NF_CARD_STEP,
                         NF_CARD_Y + 45, 50, 60);
    }
    nf_center_text(NF_CARD_X - 50, NF_CARD_Y + 67, 40, NF_WHITE, "<");
    nf_center_text(NF_CARD_X + NF_VISIBLE * NF_CARD_STEP,
                   NF_CARD_Y + 67, 50, NF_WHITE, ">");

    rb->snprintf(status, sizeof(status),
                 "%d VIDEO SYNC TITLES - MOUSE CONTROLS - NO LIVE TV/YOUTUBE",
                 nf_row_count);
    nf_center_text(NF_WIN_X, NF_WIN_Y + NF_WIN_H - 18,
                   NF_WIN_W, LCD_RGBPACK(70, 70, 70), status);
    nf_draw_pointer();
    rb->lcd_update();
}

static int nf_open_selected(void)
{
    const struct nf_row *row;
    char parameter[MAX_PATH + 17];
    const char *plugin;
    const char *extension;

    if (nf_selected < 0 || nf_selected >= nf_row_count)
        return PLUGIN_OK;
    row = &nf_rows[nf_selected];
    DEBUGF("netflix desktop: play row=%d path=%s\n",
           nf_selected, row->path);
    if (row->locked)
    {
        rb->splash(HZ * 2, "Unlock this title on the iPod");
        return PLUGIN_OK;
    }
    if (!rb->file_exists(row->path))
    {
        DEBUGF("netflix desktop: missing video %s\n", row->path);
        rb->splash(HZ * 2, "Synced video file is missing");
        return PLUGIN_OK;
    }
    extension = rb->strrchr(row->path, '.');
    if (extension &&
        (!rb->strcasecmp(extension, ".rvp") ||
         !rb->strcasecmp(extension, ".h264")))
        plugin = ROCKBOX_DIR "/rocks/viewers/openh264_player.rock";
    else if (extension &&
             (!rb->strcasecmp(extension, ".mpg") ||
              !rb->strcasecmp(extension, ".mpeg") ||
              !rb->strcasecmp(extension, ".mpv") ||
              !rb->strcasecmp(extension, ".m2v")))
        plugin = ROCKBOX_DIR "/rocks/viewers/mpegplayer.rock";
    else
    {
        DEBUGF("netflix desktop: unsupported video path=%s\n", row->path);
        rb->splash(HZ * 2, "No video player is installed");
        return PLUGIN_OK;
    }
    rb->snprintf(parameter, sizeof(parameter), "netflix:%s", row->path);
    DEBUGF("netflix desktop: handoff plugin=%s param=%s\n",
           plugin, parameter);
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    return rb->plugin_open(plugin, parameter);
}

static int nf_handle_click(int x, int y)
{
    int index;

    DEBUGF("netflix desktop: click x=%d y=%d\n", x, y);
    if (nf_point_in(x, y, NF_WIN_X + 8, NF_WIN_Y + 5, 16, 15))
        return PLUGIN_GOTO_ROOT;
    for (index = 0; index < NF_CATEGORY_COUNT; ++index)
    {
        int left = NF_BODY_X + 173 + index * 136;

        if (nf_point_in(x, y, left, NF_BODY_Y + 20, 130, 32))
        {
            nf_set_category(index);
            return PLUGIN_OK;
        }
    }
    if (nf_point_in(x, y, NF_PLAY_X, NF_PLAY_Y,
                    NF_PLAY_W, NF_PLAY_H))
        return nf_open_selected();
    for (index = 0; index < nf_visible_count; ++index)
    {
        int left = NF_CARD_X + index * NF_CARD_STEP - 3;

        if (nf_point_in(x, y, left, NF_CARD_Y - 3,
                        NF_POSTER_W + 6, NF_POSTER_H + 6))
        {
            nf_selected = nf_visible[index];
            DEBUGF("netflix desktop: mouse card selected=%d\n",
                   nf_selected);
            nf_cache_visible();
            return PLUGIN_OK;
        }
    }
    if (nf_point_in(x, y, NF_CARD_X - 50, NF_CARD_Y + 45, 40, 60))
        nf_select_step(-1);
    else if (nf_point_in(x, y,
                         NF_CARD_X + NF_VISIBLE * NF_CARD_STEP,
                         NF_CARD_Y + 45, 50, 60))
        nf_select_step(1);
    return PLUGIN_OK;
}

static bool nf_poll_pointer(int *result)
{
    char record[14];
    unsigned int buttons;
    unsigned int changed;
    int fd;

    if (TIME_BEFORE(*rb->current_tick,
                    nf_pointer_tick + MAX(1, HZ / 50)))
        return false;
    nf_pointer_tick = *rb->current_tick;
    fd = rb->open(ROCKBOX_DIR "/host-pointer", O_RDONLY);
    if (fd < 0)
        return false;
    if (rb->read(fd, record, 13) != 13)
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    if (record[4] != ' ' || record[9] != ' ')
        return false;
    record[4] = '\0';
    record[9] = '\0';
    record[12] = '\0';
    nf_pointer_x = MAX(0, MIN(LCD_WIDTH - 1, rb->atoi(record)));
    nf_pointer_y = MAX(0, MIN(LCD_HEIGHT - 1, rb->atoi(record + 5)));
    buttons = (unsigned int)rb->atoi(record + 10);
    changed = buttons ^ nf_pointer_buttons;
    nf_pointer_active = true;
    if ((changed & 1u) && !(buttons & 1u))
        *result = nf_handle_click(nf_pointer_x, nf_pointer_y);
    nf_pointer_buttons = buttons;
    return true;
}

enum plugin_status plugin_start(const void *parameter)
{
    bool desktop = parameter &&
        !rb->strcmp((const char *)parameter, "-desktop");
    int result = PLUGIN_OK;
    bool redraw = true;

    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_backdrop(NULL);
    if (!desktop || !nf_init_desktop())
    {
        rb->splash(HZ * 2, "Could not prepare Netflix Desktop window");
        return PLUGIN_ERROR;
    }
    nf_load_manifest();
    DEBUGF("netflix desktop: catalogue=%d excluded=%d source=videolist\n",
           nf_row_count, nf_excluded_count);
    nf_load_watched();
    nf_set_category(NF_CATEGORY_HOME);

    while (result == PLUGIN_OK)
    {
        int button;

        if (redraw)
        {
            nf_draw();
            redraw = false;
        }
        button = pluginlib_getaction(MAX(1, HZ / 50),
                                     plugin_contexts,
                                     ARRAYLEN(plugin_contexts));
        if (nf_poll_pointer(&result))
            redraw = true;
        if (result != PLUGIN_OK)
            break;
        switch (button)
        {
            case PLA_LEFT:
            case PLA_LEFT_REPEAT:
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                nf_select_step(-1);
                redraw = true;
                break;
            case PLA_RIGHT:
            case PLA_RIGHT_REPEAT:
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                nf_select_step(1);
                redraw = true;
                break;
            case PLA_UP:
            case PLA_UP_REPEAT:
                nf_set_category(
                    (nf_category + NF_CATEGORY_COUNT - 1) %
                    NF_CATEGORY_COUNT);
                redraw = true;
                break;
            case PLA_DOWN:
            case PLA_DOWN_REPEAT:
                nf_set_category((nf_category + 1) % NF_CATEGORY_COUNT);
                redraw = true;
                break;
            case PLA_SELECT:
                result = nf_open_selected();
                redraw = true;
                break;
            case PLA_CANCEL:
                result = PLUGIN_GOTO_ROOT;
                break;
            default:
                if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                    result = PLUGIN_USB_CONNECTED;
                break;
        }
    }
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    return result;
}
