/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Live TV schedule model and DIRECTV style program guide.
 *
 * Colours here were sampled from the DIRECTV receiver user guide artwork at
 * 600 dpi; see docs/livetv-directv-guide-spec.md for the table and for the
 * layout budget this file implements.
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
#include "mpegplayer.h"
#include "livetv.h"

#ifdef HAVE_LCD_COLOR

/* Sampled DIRECTV palette ------------------------------------------- */

#define LIVETV_BANNER_TOP   LCD_RGBPACK(220, 238, 249) /* #DCEEF9 */
#define LIVETV_BANNER_BOT   LCD_RGBPACK(183, 214, 233) /* #B7D6E9 */
#define LIVETV_STRIP_BG     LCD_RGBPACK(183, 214, 233) /* #B7D6E9 */
#define LIVETV_STRIP_CELL   LCD_RGBPACK(157, 195, 220)
#define LIVETV_STRIP_TEXT   LCD_RGBPACK(16, 56, 107)   /* #10386B */
#define LIVETV_BRAND_BLUE   LCD_RGBPACK(0, 82, 155)
#define LIVETV_DESC_BG      LCD_RGBPACK(2, 111, 175)   /* #026FAF */
#define LIVETV_HDR_BG       LCD_RGBPACK(18, 37, 73)    /* #122549 */
#define LIVETV_CHAN_BG      LCD_RGBPACK(18, 37, 73)    /* #122549 */
#define LIVETV_ROW_BG       LCD_RGBPACK(9, 72, 113)    /* #094871 */
#define LIVETV_GRID_LINE    LCD_RGBPACK(10, 42, 80)    /* #0A2A50 */
#define LIVETV_SEL_BG       LCD_RGBPACK(254, 196, 37)  /* #FEC425 */
#define LIVETV_SEL_TEXT     LCD_RGBPACK(16, 37, 74)    /* #10254A */
#define LIVETV_HINT_BG      LCD_RGBPACK(15, 86, 137)   /* #0F5689 */
#define LIVETV_TEXT         LCD_WHITE
#define LIVETV_DIM_TEXT     LCD_RGBPACK(92, 130, 168)  /* #5C82A8 */
#define LIVETV_DOT_RED      LCD_RGBPACK(194, 42, 24)   /* #C22A18 */
#define LIVETV_DOT_GREEN    LCD_RGBPACK(46, 161, 92)   /* #2EA15C */
#define LIVETV_DOT_YELLOW   LCD_RGBPACK(249, 198, 60)  /* #F9C63C */
#define LIVETV_WORDMARK     LCD_RGBPACK(124, 162, 193) /* #7CA2C1 */

/* Layout ------------------------------------------------------------- */

#define LIVETV_BANNER_H     26
#define LIVETV_STRIP_Y      26
#define LIVETV_STRIP_H      15
#define LIVETV_DESC_Y       41
#define LIVETV_DESC_H       36
#define LIVETV_HDR_Y        77
#define LIVETV_HDR_H        14
#define LIVETV_GRID_Y       91
#define LIVETV_ROW_H        22
#define LIVETV_GRID_ROWS    6
#define LIVETV_GRID_H       (LIVETV_ROW_H * LIVETV_GRID_ROWS)
#define LIVETV_HINT_Y       (LIVETV_GRID_Y + LIVETV_GRID_H)
#define LIVETV_HINT_H       (LCD_HEIGHT - LIVETV_HINT_Y)
/* Wide enough for the DIRECTV style "100 RTRO" call sign at the UI font. */
#define LIVETV_CHANCOL_W    68
#define LIVETV_GRID_COLS    3
#define LIVETV_COL_W        ((LCD_WIDTH - LIVETV_CHANCOL_W) / LIVETV_GRID_COLS)
#define LIVETV_STRIP_CELL_W 78

#define LIVETV_LOGO_W       40
#define LIVETV_LOGO_H       18
#define LIVETV_LOGO_CACHE   8
#define LIVETV_BRAND_W      64
#define LIVETV_BRAND_H      22

#define LIVETV_BANNER_SECS  (5 * HZ)

/* Model -------------------------------------------------------------- */

static struct livetv_channel livetv_channels[LIVETV_MAX_CHANNELS];
static int livetv_channel_num;

static struct livetv_slot livetv_slots[LIVETV_MAX_SLOTS];
static int livetv_slot_num;

static char livetv_text_pool[LIVETV_TEXT_POOL];
static int livetv_text_used;
static char livetv_path_pool[LIVETV_PATH_POOL];
static int livetv_path_used;

static char livetv_root[MAX_PATH];
static int livetv_channel_cur;
static bool livetv_ready;

/* The slot the player is currently playing, so that an end of file can be
 * answered with the programme that follows it. */
static long livetv_play_delta;

/* Guide state -------------------------------------------------------- */

enum livetv_filter
{
    LIVETV_FILTER_ALL = 0,
    LIVETV_FILTER_FAVOURITES,
    LIVETV_FILTER_SHOWS,
    LIVETV_FILTER_COUNT,
};

static struct
{
    int  row_top;        /* first visible channel row */
    int  row_sel;        /* selected channel row */
    long window;         /* seconds from now to the left grid edge */
    long cursor;         /* seconds from now to the selected programme */
    int  filter;
    int  prev_row_sel;
    bool full_redraw;
} guide;

/* Visible channel list under the current filter */
static int livetv_view[LIVETV_MAX_CHANNELS];
static int livetv_view_num;

/* Logo cache --------------------------------------------------------- */

static struct
{
    int chan;
    bool valid;
    struct bitmap bm;
    fb_data data[LIVETV_LOGO_W * LIVETV_LOGO_H];
} livetv_logos[LIVETV_LOGO_CACHE];

static struct bitmap livetv_brand;
static fb_data livetv_brand_data[LIVETV_BRAND_W * LIVETV_BRAND_H];
static bool livetv_brand_valid;
static bool livetv_brand_tried;

static const char * const livetv_wday[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char * const livetv_filter_names[LIVETV_FILTER_COUNT] = {
    "All Channels", "Favorites", "Shows Only"
};

/* Text helpers -------------------------------------------------------- */

static char *livetv_trim(char *text)
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
        *--end = '\0';
    }

    return text;
}

/* Split a tab separated line in place. Returns the field count. */
static int livetv_split(char *line, char **fields, int max_fields)
{
    int count = 0;

    while (count < max_fields)
    {
        char *tab = rb->strchr(line, '\t');

        fields[count++] = line;
        if (tab == NULL)
            break;
        *tab = '\0';
        line = tab + 1;
    }

    for (int i = 0; i < count; i++)
        fields[i] = livetv_trim(fields[i]);

    return count;
}

/* Store one string, reusing an identical one already in the pool.
 *
 * A day of listings repeats the same titles, ratings and synopses many times
 * over: a commercial carries the title of the show it interrupts, and every
 * show on a channel shares a rating. Without this the pool overflows and
 * later channels lose their titles entirely. */
static int livetv_pool_add(char *pool, int *used, int size, const char *text)
{
    int len;
    int offset;

    if (text == NULL)
        text = "";
    if (text[0] == '\0')
        return 0;

    for (offset = 1; offset < *used; offset += rb->strlen(&pool[offset]) + 1)
    {
        if (!rb->strcmp(&pool[offset], text))
            return offset;
    }

    len = rb->strlen(text);
    if (*used + len + 1 > size)
        return 0; /* pool full: fall back to the empty string at offset 0 */

    offset = *used;
    rb->strcpy(&pool[offset], text);
    *used += len + 1;
    return offset;
}

static void livetv_fit(char *buf, size_t size, const char *src, int max_width)
{
    int width;

    rb->strlcpy(buf, src, size);
    rb->lcd_getstringsize(buf, &width, NULL);
    if (width <= max_width)
        return;

    while (buf[0] != '\0')
    {
        size_t len = rb->strlen(buf);

        if (len <= 4)
            return;
        rb->strlcpy(&buf[len - 4], "...", 4);
        rb->lcd_getstringsize(buf, &width, NULL);
        if (width <= max_width)
            return;
        buf[len - 4] = '\0';
    }
}

/* Loading ------------------------------------------------------------- */

static void livetv_make_path(char *buf, size_t size, const char *name)
{
    rb->snprintf(buf, size, "%s/%s", livetv_root, name);
}

static bool livetv_load_channels(void)
{
    char path[MAX_PATH];
    char line[256];
    int fd;

    livetv_channel_num = 0;
    livetv_make_path(path, sizeof(path), LIVETV_CHANNELS_FILE);

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    while (livetv_channel_num < LIVETV_MAX_CHANNELS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[6];
        char *text = livetv_trim(line);
        int count;
        struct livetv_channel *chan;

        if (text[0] == '\0' || text[0] == '#')
            continue;

        count = livetv_split(text, fields, 6);
        if (count < 2)
            continue;

        chan = &livetv_channels[livetv_channel_num];
        rb->memset(chan, 0, sizeof(*chan));
        chan->number = rb->atoi(fields[0]);
        rb->strlcpy(chan->callsign, fields[1], sizeof(chan->callsign));
        if (count > 2)
            rb->strlcpy(chan->name, fields[2], sizeof(chan->name));
        else
            rb->strlcpy(chan->name, fields[1], sizeof(chan->name));
        if (count > 3)
            rb->strlcpy(chan->category, fields[3], sizeof(chan->category));
        if (count > 4)
            rb->strlcpy(chan->logo, fields[4], sizeof(chan->logo));
        if (count > 5)
            chan->favourite = rb->atoi(fields[5]) != 0;
        chan->first_slot = -1;
        livetv_channel_num++;
    }

    rb->close(fd);
    return livetv_channel_num > 0;
}

static int livetv_channel_by_number(int number)
{
    for (int i = 0; i < livetv_channel_num; i++)
    {
        if (livetv_channels[i].number == number)
            return i;
    }
    return -1;
}

static bool livetv_load_guide(void)
{
    char path[MAX_PATH];
    char line[512];
    int fd;
    int today = livetv_now_day();
    int tomorrow = (today + 1) % 7;

    livetv_slot_num = 0;
    livetv_text_used = 1;
    livetv_text_pool[0] = '\0';
    livetv_path_used = 1;
    livetv_path_pool[0] = '\0';

    livetv_make_path(path, sizeof(path), LIVETV_GUIDE_FILE);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    while (livetv_slot_num < LIVETV_MAX_SLOTS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[11];
        char *text = livetv_trim(line);
        struct livetv_slot *slot;
        int chan_index;
        int day;
        int field_count;

        if (text[0] == '\0' || text[0] == '#')
            continue;

        field_count = livetv_split(text, fields, 11);
        if (field_count < 9)
            continue;

        chan_index = livetv_channel_by_number(rb->atoi(fields[0]));
        if (chan_index < 0)
            continue;

        /* Only today and tomorrow are held in memory. */
        day = rb->atoi(fields[1]);
        if (day != today && day != tomorrow)
            continue;

        slot = &livetv_slots[livetv_slot_num];
        slot->chan = chan_index;
        slot->day = day;
        slot->start = (uint32_t)rb->atoi(fields[2]);
        slot->dur = (uint16_t)rb->atoi(fields[3]);
        slot->kind = (fields[4][0] == LIVETV_KIND_AD) ? LIVETV_KIND_AD
                                                      : LIVETV_KIND_SHOW;
        slot->title_off = livetv_pool_add(livetv_text_pool, &livetv_text_used,
                                          LIVETV_TEXT_POOL, fields[5]);
        slot->rating_off = livetv_pool_add(livetv_text_pool, &livetv_text_used,
                                           LIVETV_TEXT_POOL, fields[6]);
        slot->desc_off = livetv_pool_add(livetv_text_pool, &livetv_text_used,
                                         LIVETV_TEXT_POOL, fields[7]);
        slot->path_off = livetv_pool_add(livetv_path_pool, &livetv_path_used,
                                         LIVETV_PATH_POOL, fields[8]);

        if (field_count >= 11)
        {
            slot->block_start = (uint32_t)rb->atoi(fields[9]);
            slot->block_dur = (uint16_t)rb->atoi(fields[10]);
        }
        if (slot->block_dur == 0)
        {
            slot->block_start = slot->start;
            slot->block_dur = slot->dur;
        }

        if (slot->dur == 0)
            continue;

        livetv_slot_num++;
    }

    rb->close(fd);

    /* Index the slots per channel. The generator writes them grouped by
     * channel and ordered by day then start, which the resolver relies on. */
    for (int i = 0; i < livetv_slot_num; i++)
    {
        struct livetv_channel *chan = &livetv_channels[livetv_slots[i].chan];

        if (chan->first_slot < 0)
            chan->first_slot = i;
        chan->slot_count++;
    }

    return livetv_slot_num > 0;
}

static void livetv_load_state(void)
{
    char path[MAX_PATH];
    char line[32];
    int fd;

    livetv_channel_cur = 0;
    livetv_make_path(path, sizeof(path), LIVETV_STATE_FILE);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return;

    if (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        int index = livetv_channel_by_number(rb->atoi(livetv_trim(line)));

        if (index >= 0)
            livetv_channel_cur = index;
    }

    rb->close(fd);
}

void livetv_save_state(void)
{
    char path[MAX_PATH];
    int fd;

    if (!livetv_ready || livetv_channel_cur >= livetv_channel_num)
        return;

    livetv_make_path(path, sizeof(path), LIVETV_STATE_FILE);
    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;

    rb->fdprintf(fd, "%d\n", livetv_channels[livetv_channel_cur].number);
    rb->close(fd);
}

static void livetv_rebuild_view(void)
{
    livetv_view_num = 0;

    for (int i = 0; i < livetv_channel_num; i++)
    {
        const struct livetv_channel *chan = &livetv_channels[i];

        if (guide.filter == LIVETV_FILTER_FAVOURITES && !chan->favourite)
            continue;
        if (guide.filter == LIVETV_FILTER_SHOWS && chan->slot_count == 0)
            continue;
        livetv_view[livetv_view_num++] = i;
    }

    if (livetv_view_num == 0)
    {
        /* Never present an empty grid: fall back to every channel. */
        guide.filter = LIVETV_FILTER_ALL;
        for (int i = 0; i < livetv_channel_num; i++)
            livetv_view[livetv_view_num++] = i;
    }
}

bool livetv_load(const char *root)
{
    livetv_ready = false;
    rb->memset(livetv_logos, 0, sizeof(livetv_logos));
    livetv_brand_valid = false;
    livetv_brand_tried = false;

    if (root == NULL || root[0] == '\0')
        root = LIVETV_DEFAULT_ROOT;
    rb->strlcpy(livetv_root, root, sizeof(livetv_root));

    /* Tolerate a trailing slash from the launcher */
    size_t len = rb->strlen(livetv_root);
    while (len > 1 && livetv_root[len - 1] == '/')
        livetv_root[--len] = '\0';

    if (!livetv_load_channels())
        return false;
    if (!livetv_load_guide())
        return false;

    livetv_load_state();
    guide.filter = LIVETV_FILTER_ALL;
    livetv_rebuild_view();
    livetv_ready = true;
    return true;
}

bool livetv_is_active(void)
{
    return livetv_ready;
}

int livetv_channel_count(void)
{
    return livetv_channel_num;
}

const struct livetv_channel *livetv_channel(int index)
{
    if (index < 0 || index >= livetv_channel_num)
        return NULL;
    return &livetv_channels[index];
}

int livetv_current_channel(void)
{
    return livetv_channel_cur;
}

void livetv_set_current_channel(int index)
{
    if (index >= 0 && index < livetv_channel_num)
        livetv_channel_cur = index;
}

const char *livetv_slot_title(const struct livetv_slot *slot)
{
    if (slot == NULL)
        return "";
    return &livetv_text_pool[slot->title_off];
}

const char *livetv_slot_rating(const struct livetv_slot *slot)
{
    if (slot == NULL)
        return "";
    return &livetv_text_pool[slot->rating_off];
}

const char *livetv_slot_desc(const struct livetv_slot *slot)
{
    if (slot == NULL)
        return "";
    return &livetv_text_pool[slot->desc_off];
}

bool livetv_slot_path(const struct livetv_slot *slot, char *buf, size_t size)
{
    const char *rel;

    if (slot == NULL)
        return false;

    rel = &livetv_path_pool[slot->path_off];
    if (rel[0] == '\0')
        return false;

    if (rel[0] == '/')
        rb->strlcpy(buf, rel, size);
    else
        rb->snprintf(buf, size, "%s/%s", livetv_root, rel);

    return true;
}

/* Clock --------------------------------------------------------------- */

uint32_t livetv_now_secs(void)
{
    struct tm *tm = rb->get_time();

    if (tm == NULL)
        return 0;

    return (uint32_t)(tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}

int livetv_now_day(void)
{
    struct tm *tm = rb->get_time();

    if (tm == NULL)
        return 0;

    return tm->tm_wday % 7;
}

/* Resolve an offset from now into a weekday and a second-of-day. */
static void livetv_resolve_time(long delta, int *day, uint32_t *secs)
{
    long total = (long)livetv_now_secs() + delta;
    int dayshift = 0;

    while (total < 0)
    {
        total += LIVETV_DAY_SECONDS;
        dayshift--;
    }
    while (total >= LIVETV_DAY_SECONDS)
    {
        total -= LIVETV_DAY_SECONDS;
        dayshift++;
    }

    *secs = (uint32_t)total;
    *day = (livetv_now_day() + dayshift % 7 + 7) % 7;
}

static const struct livetv_slot *livetv_find(int chan, int day, uint32_t secs,
                                             uint32_t *offset)
{
    const struct livetv_channel *ch = livetv_channel(chan);
    const struct livetv_slot *best = NULL;

    if (ch == NULL || ch->first_slot < 0)
        return NULL;

    for (int i = ch->first_slot; i < ch->first_slot + ch->slot_count; i++)
    {
        const struct livetv_slot *slot = &livetv_slots[i];

        if (slot->day != day)
            continue;
        if (secs < slot->start)
            continue;
        if (secs >= slot->start + slot->dur)
            continue;
        best = slot;
        break;
    }

    if (best != NULL && offset != NULL)
        *offset = secs - best->start;

    return best;
}

const struct livetv_slot *livetv_slot_at(int chan, long delta,
                                         uint32_t *offset)
{
    int day;
    uint32_t secs;

    livetv_resolve_time(delta, &day, &secs);
    return livetv_find(chan, day, secs, offset);
}

/* Seconds from "delta" to the end of the programme airing then, counting
 * the commercial break as part of the programme. */
static long livetv_block_remaining(const struct livetv_slot *slot,
                                   uint32_t offset)
{
    long into_block = (long)(slot->start - slot->block_start) + (long)offset;

    return (long)slot->block_dur - into_block;
}

const struct livetv_slot *livetv_slot_next(int chan, long delta)
{
    const struct livetv_slot *cur;
    uint32_t offset = 0;

    cur = livetv_slot_at(chan, delta, &offset);
    if (cur == NULL)
        return NULL;

    /* Step just past the end of the whole programme, so "next" is the next
     * show rather than the next commercial. */
    return livetv_slot_at(chan, delta + livetv_block_remaining(cur, offset) + 1,
                          NULL);
}

/* Playback ------------------------------------------------------------ */

bool livetv_tune(int chan, char *videofile, size_t size, uint32_t *resume)
{
    const struct livetv_slot *slot;
    uint32_t offset = 0;

    if (!livetv_ready)
        return false;

    slot = livetv_slot_at(chan, 0, &offset);
    if (slot == NULL)
        return false;
    if (!livetv_slot_path(slot, videofile, size))
        return false;

    livetv_channel_cur = chan;
    livetv_play_delta = 0;

    if (resume != NULL)
    {
        /* Join the programme already in progress, exactly like tuning in
         * to a broadcast that started before you sat down. Leave a small
         * guard so a nearly finished programme is not opened at its very
         * last frame. */
        if (offset + 3 >= slot->dur)
            offset = 0;
        *resume = (uint32_t)offset * TS_SECOND;
    }

    return true;
}

bool livetv_advance(char *videofile, size_t size, uint32_t *resume)
{
    /* A programme just ran out. Whatever the clock says now is what should
     * be on the air; resolving from the clock rather than from a stored
     * index keeps the channel honest even if decoding fell behind. */
    return livetv_tune(livetv_channel_cur, videofile, size, resume);
}

/* Drawing helpers ------------------------------------------------------ */

static bool livetv_pig_active(void)
{
    return mpegplayer_livetv_pig;
}

/* Fill a rectangle, splitting it around the picture in guide window so the
 * decoded video is never painted over. */
static void livetv_fill(int x, int y, int w, int h, unsigned color)
{
    int x2 = x + w;
    int y2 = y + h;
    int px = LIVETV_PIG_BOX_X;
    int py = LIVETV_PIG_BOX_Y;
    int px2 = px + LIVETV_PIG_BOX_W;
    int py2 = py + LIVETV_PIG_BOX_H;

    if (w <= 0 || h <= 0)
        return;

    rb->lcd_set_foreground(color);

    if (!livetv_pig_active() || y2 <= py || y >= py2 || x2 <= px || x >= px2)
    {
        rb->lcd_fillrect(x, y, w, h);
        return;
    }

    if (y < py)
    {
        rb->lcd_fillrect(x, y, w, py - y);
        y = py;
    }
    if (y2 > py2)
    {
        rb->lcd_fillrect(x, py2, w, y2 - py2);
        y2 = py2;
    }
    if (y2 <= y)
        return;
    if (x < px)
        rb->lcd_fillrect(x, y, px - x, y2 - y);
    if (x2 > px2)
        rb->lcd_fillrect(px2, y, x2 - px2, y2 - y);
    (void)px2;
}

/* Commit a region, again skipping the video window. */
static void livetv_update(int x, int y, int w, int h)
{
    int y2 = y + h;
    int py = LIVETV_PIG_BOX_Y;
    int py2 = py + LIVETV_PIG_BOX_H;

    if (w <= 0 || h <= 0)
        return;

    if (!livetv_pig_active() || y2 <= py || y >= py2)
    {
        rb->lcd_update_rect(x, y, w, h);
        return;
    }

    if (y < py)
        rb->lcd_update_rect(x, y, w, py - y);
    if (y2 > py2)
        rb->lcd_update_rect(x, py2, w, y2 - py2);
    if (x < LIVETV_PIG_BOX_X)
        rb->lcd_update_rect(x, MAX(y, py), MIN(x + w, LIVETV_PIG_BOX_X) - x,
                            MIN(y2, py2) - MAX(y, py));
}

static void livetv_gradient(int y, int h, unsigned top, unsigned bottom)
{
    int r1 = RGB_UNPACK_RED(top), g1 = RGB_UNPACK_GREEN(top);
    int b1 = RGB_UNPACK_BLUE(top);
    int r2 = RGB_UNPACK_RED(bottom), g2 = RGB_UNPACK_GREEN(bottom);
    int b2 = RGB_UNPACK_BLUE(bottom);

    for (int i = 0; i < h; i++)
    {
        unsigned color = LCD_RGBPACK(r1 + (r2 - r1) * i / h,
                                     g1 + (g2 - g1) * i / h,
                                     b1 + (b2 - b1) * i / h);
        livetv_fill(0, y + i, LCD_WIDTH, 1, color);
    }
}

static void livetv_text_at(int x, int y, unsigned fg, unsigned bg,
                           const char *text)
{
    rb->lcd_set_foreground(fg);
    rb->lcd_set_background(bg);
    rb->lcd_putsxy(x, y, text);
}

static void livetv_text_fit(int x, int y, int max_width, unsigned fg,
                            unsigned bg, const char *text)
{
    char buf[96];

    livetv_fit(buf, sizeof(buf), text, max_width);
    livetv_text_at(x, y, fg, bg, buf);
}

static int livetv_font_height(void)
{
    int h = 0;

    rb->lcd_getstringsize("Ag", NULL, &h);
    return h > 0 ? h : 8;
}

static void livetv_format_clock(char *buf, size_t size, uint32_t secs)
{
    int hour = (secs / 3600) % 24;
    int minute = (secs / 60) % 60;
    const char *suffix = hour >= 12 ? "p" : "a";
    int display = hour % 12;

    if (display == 0)
        display = 12;

    rb->snprintf(buf, size, "%d:%02d%s", display, minute, suffix);
}

/* Days in a month, honouring leap years. */
static int livetv_days_in_month(int month, int year)
{
    static const int days[12] = { 31, 28, 31, 30, 31, 30,
                                  31, 31, 30, 31, 30, 31 };

    if (month == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month % 12];
}

/* Calendar date of the day "delta" seconds from now. */
static void livetv_format_date(char *buf, size_t size, long delta)
{
    struct tm *now = rb->get_time();
    long total;
    int shift = 0;
    int mon, mday, year, wday;

    if (now == NULL)
    {
        rb->strlcpy(buf, "", size);
        return;
    }

    total = (long)livetv_now_secs() + delta;
    while (total < 0)
    {
        total += LIVETV_DAY_SECONDS;
        shift--;
    }
    while (total >= LIVETV_DAY_SECONDS)
    {
        total -= LIVETV_DAY_SECONDS;
        shift++;
    }

    mon = now->tm_mon;
    mday = now->tm_mday;
    year = now->tm_year + 1900;
    wday = now->tm_wday;

    while (shift > 0)
    {
        mday++;
        if (mday > livetv_days_in_month(mon, year))
        {
            mday = 1;
            if (++mon > 11)
            {
                mon = 0;
                year++;
            }
        }
        wday = (wday + 1) % 7;
        shift--;
    }
    while (shift < 0)
    {
        mday--;
        if (mday < 1)
        {
            if (--mon < 0)
            {
                mon = 11;
                year--;
            }
            mday = livetv_days_in_month(mon, year);
        }
        wday = (wday + 6) % 7;
        shift++;
    }

    rb->snprintf(buf, size, "%s %d/%d", livetv_wday[wday], mon + 1, mday);
}

/* Assets --------------------------------------------------------------- */

static struct bitmap *livetv_brand_logo(void)
{
    char path[MAX_PATH];

    if (livetv_brand_tried)
        return livetv_brand_valid ? &livetv_brand : NULL;

    livetv_brand_tried = true;
    livetv_brand.data = (char *)livetv_brand_data;
    livetv_make_path(path, sizeof(path), LIVETV_BRAND_LOGO);
    if (rb->read_bmp_file(path, &livetv_brand, sizeof(livetv_brand_data),
                          FORMAT_NATIVE, NULL) > 0)
        livetv_brand_valid = true;

    return livetv_brand_valid ? &livetv_brand : NULL;
}

static struct bitmap *livetv_channel_logo(int chan)
{
    const struct livetv_channel *ch = livetv_channel(chan);
    char path[MAX_PATH];
    int slot = -1;

    if (ch == NULL || ch->logo[0] == '\0')
        return NULL;

    for (int i = 0; i < LIVETV_LOGO_CACHE; i++)
    {
        if (livetv_logos[i].chan == chan + 1)
        {
            return livetv_logos[i].valid ? &livetv_logos[i].bm : NULL;
        }
        if (slot < 0 && livetv_logos[i].chan == 0)
            slot = i;
    }

    if (slot < 0)
        slot = chan % LIVETV_LOGO_CACHE; /* simple eviction */

    livetv_logos[slot].chan = chan + 1;
    livetv_logos[slot].valid = false;
    livetv_logos[slot].bm.data = (char *)livetv_logos[slot].data;

    if (ch->logo[0] == '/')
        rb->strlcpy(path, ch->logo, sizeof(path));
    else
        livetv_make_path(path, sizeof(path), ch->logo);

    if (rb->read_bmp_file(path, &livetv_logos[slot].bm,
                          sizeof(livetv_logos[slot].data),
                          FORMAT_NATIVE, NULL) > 0)
        livetv_logos[slot].valid = true;

    return livetv_logos[slot].valid ? &livetv_logos[slot].bm : NULL;
}

/* Guide ---------------------------------------------------------------- */

static int livetv_view_index_of(int chan)
{
    for (int i = 0; i < livetv_view_num; i++)
    {
        if (livetv_view[i] == chan)
            return i;
    }
    return 0;
}

void livetv_guide_reset_to_now(void)
{
    guide.window = 0;
    guide.cursor = 0;
    guide.row_sel = livetv_view_index_of(livetv_channel_cur);
    if (guide.row_sel >= LIVETV_GRID_ROWS)
        guide.row_top = guide.row_sel - LIVETV_GRID_ROWS + 1;
    else
        guide.row_top = 0;
    guide.prev_row_sel = guide.row_sel;
    guide.full_redraw = true;
}

void livetv_guide_enter(void)
{
    livetv_rebuild_view();
    livetv_guide_reset_to_now();
}

int livetv_guide_selected_channel(void)
{
    if (guide.row_sel < 0 || guide.row_sel >= livetv_view_num)
        return livetv_channel_cur;
    return livetv_view[guide.row_sel];
}

bool livetv_guide_selection_is_live(void)
{
    return guide.cursor <= 0;
}

const char *livetv_guide_filter_name(void)
{
    return livetv_filter_names[guide.filter % LIVETV_FILTER_COUNT];
}

void livetv_guide_cycle_filter(void)
{
    guide.filter = (guide.filter + 1) % LIVETV_FILTER_COUNT;
    livetv_rebuild_view();
    guide.row_sel = MIN(guide.row_sel, livetv_view_num - 1);
    guide.row_top = MIN(guide.row_top, MAX(0, livetv_view_num -
                                              LIVETV_GRID_ROWS));
    guide.full_redraw = true;
}

void livetv_guide_jump_hours(int hours)
{
    long delta = (long)hours * 3600;

    guide.window += delta;
    if (guide.window < 0)
        guide.window = 0;
    guide.cursor = guide.window;
    guide.full_redraw = true;
}

bool livetv_guide_move(int dchan, int dtime)
{
    bool changed = false;

    if (dchan != 0 && livetv_view_num > 0)
    {
        int next = guide.row_sel + dchan;

        if (next < 0)
            next = livetv_view_num - 1;
        else if (next >= livetv_view_num)
            next = 0;

        if (next != guide.row_sel)
        {
            guide.prev_row_sel = guide.row_sel;
            guide.row_sel = next;
            changed = true;
        }

        if (guide.row_sel < guide.row_top)
        {
            guide.row_top = guide.row_sel;
            guide.full_redraw = true;
        }
        else if (guide.row_sel >= guide.row_top + LIVETV_GRID_ROWS)
        {
            guide.row_top = guide.row_sel - LIVETV_GRID_ROWS + 1;
            guide.full_redraw = true;
        }
    }

    if (dtime != 0)
    {
        int chan = livetv_guide_selected_channel();
        const struct livetv_slot *slot;
        long probe = guide.cursor;

        uint32_t offset = 0;

        /* Left and right walk whole programmes, never into a commercial. */
        slot = livetv_slot_at(chan, probe, &offset);
        if (dtime > 0)
        {
            if (slot != NULL)
                probe += livetv_block_remaining(slot, offset) + 1;
            else
                probe += LIVETV_SLOT_SECONDS;
        }
        else
        {
            if (slot != NULL)
                probe -= (long)(slot->start - slot->block_start) +
                         (long)offset + 1;
            else
                probe -= LIVETV_SLOT_SECONDS;
            if (probe < 0)
                probe = 0;
        }

        if (probe != guide.cursor)
        {
            guide.cursor = probe;
            changed = true;
        }

        /* Scroll the visible time window to keep the cursor on screen. */
        if (guide.cursor < guide.window)
        {
            guide.window = (guide.cursor / LIVETV_SLOT_SECONDS) *
                           LIVETV_SLOT_SECONDS;
            guide.full_redraw = true;
        }
        else if (guide.cursor >= guide.window +
                                 LIVETV_GRID_COLS * LIVETV_SLOT_SECONDS)
        {
            guide.window = ((guide.cursor / LIVETV_SLOT_SECONDS) -
                            LIVETV_GRID_COLS + 1) * LIVETV_SLOT_SECONDS;
            guide.full_redraw = true;
        }
    }

    return changed;
}

/* Where does the visible time window start, snapped to a half hour? */
static long livetv_window_base(void)
{
    long now = (long)livetv_now_secs();
    long base = ((now + guide.window) / LIVETV_SLOT_SECONDS) *
                LIVETV_SLOT_SECONDS;

    return base - now; /* offset from now */
}

static int livetv_col_x(int col)
{
    return LIVETV_CHANCOL_W + col * LIVETV_COL_W;
}

static void livetv_draw_banner_area(void)
{
    const struct livetv_slot *slot;
    int chan = livetv_guide_selected_channel();
    struct bitmap *brand;
    char buf[96];
    char clock[16];
    char range[40];
    uint32_t offset = 0;
    int text_h = livetv_font_height();
    int desc_right = livetv_pig_active() ? LIVETV_PIG_BOX_X - 4 : LCD_WIDTH - 4;

    slot = livetv_slot_at(chan, guide.cursor, &offset);

    /* Banner gradient and brand mark */
    livetv_gradient(0, LIVETV_BANNER_H, LIVETV_BANNER_TOP, LIVETV_BANNER_BOT);

    brand = livetv_brand_logo();
    if (brand != NULL)
    {
        rb->lcd_bitmap((const fb_data *)brand->data, 4,
                       (LIVETV_BANNER_H - brand->height) / 2,
                       brand->width, brand->height);
    }
    else
    {
        livetv_text_at(4, (LIVETV_BANNER_H - text_h) / 2, LIVETV_BRAND_BLUE,
                       LIVETV_BANNER_TOP, "DIRECTV");
    }

    /* "guide" wordmark, sitting just left of the video window as it does on
     * the receiver. */
    {
        int wordmark_w = 0;

        rb->lcd_getstringsize("guide", &wordmark_w, NULL);
        livetv_text_at(desc_right - wordmark_w - 2,
                       (LIVETV_BANNER_H - text_h) / 2, LIVETV_WORDMARK,
                       LIVETV_BANNER_TOP, "guide");
        desc_right -= wordmark_w + 8;
    }

    /* Programme title, in DIRECTV blue on the pale banner */
    livetv_text_fit(72, (LIVETV_BANNER_H - text_h) / 2, desc_right - 76,
                    LIVETV_BRAND_BLUE, LIVETV_BANNER_BOT,
                    slot != NULL ? livetv_slot_title(slot) : "No Programming");

    /* Information strip: clock cell, air window, rating */
    livetv_fill(0, LIVETV_STRIP_Y, LCD_WIDTH, LIVETV_STRIP_H,
                LIVETV_STRIP_BG);
    livetv_fill(0, LIVETV_STRIP_Y, LIVETV_STRIP_CELL_W, LIVETV_STRIP_H,
                LIVETV_STRIP_CELL);
    livetv_fill(LIVETV_STRIP_CELL_W, LIVETV_STRIP_Y, 1, LIVETV_STRIP_H,
                LCD_WHITE);

    livetv_format_clock(clock, sizeof(clock), livetv_now_secs());
    rb->snprintf(buf, sizeof(buf), "%s %s",
                 livetv_wday[livetv_now_day()], clock);
    livetv_text_fit(3, LIVETV_STRIP_Y + (LIVETV_STRIP_H - text_h) / 2,
                    LIVETV_STRIP_CELL_W - 6, LIVETV_STRIP_TEXT,
                    LIVETV_STRIP_CELL, buf);

    if (slot != NULL)
    {
        char start_txt[16], end_txt[16];

        /* The air window is the programme's, not the individual clip's. */
        livetv_format_clock(start_txt, sizeof(start_txt), slot->block_start);
        livetv_format_clock(end_txt, sizeof(end_txt),
                            (slot->block_start + slot->block_dur) %
                            LIVETV_DAY_SECONDS);
        rb->snprintf(range, sizeof(range), "%s - %s", start_txt, end_txt);
    }
    else
    {
        rb->strlcpy(range, "--", sizeof(range));
    }

    livetv_text_fit(LIVETV_STRIP_CELL_W + 6,
                    LIVETV_STRIP_Y + (LIVETV_STRIP_H - text_h) / 2, 108,
                    LIVETV_STRIP_TEXT, LIVETV_STRIP_BG, range);

    if (slot != NULL && livetv_slot_rating(slot)[0] != '\0')
    {
        const char *rating = livetv_slot_rating(slot);
        int w = 0;

        rb->lcd_getstringsize(rating, &w, NULL);
        livetv_text_at(MAX(172, desc_right - w - 4),
                       LIVETV_STRIP_Y + (LIVETV_STRIP_H - text_h) / 2,
                       LIVETV_STRIP_TEXT, LIVETV_STRIP_BG, rating);
    }

    /* Description block */
    livetv_fill(0, LIVETV_DESC_Y, LCD_WIDTH, LIVETV_DESC_H, LIVETV_DESC_BG);

    if (slot != NULL)
    {
        const char *desc = livetv_slot_desc(slot);
        int y = LIVETV_DESC_Y + 2;
        int line_h = text_h + 1;
        int lines = MAX(1, (LIVETV_DESC_H - 4) / line_h);

        if (desc[0] == '\0')
            desc = livetv_channel(chan)->name;

        {
            /* Wrap the synopsis over the block, exactly as the receiver
             * prints it under the programme title. */
            char line[96];
            const char *p = desc;

            for (int i = 0; i < lines && *p != '\0'; i++)
            {
                int used = 0;
                int width = 0;
                int last_space = 0;

                while (p[used] != '\0' && used < (int)sizeof(line) - 1)
                {
                    line[used] = p[used];
                    line[used + 1] = '\0';
                    if (p[used] == ' ')
                        last_space = used;
                    rb->lcd_getstringsize(line, &width, NULL);
                    if (width > desc_right - 8)
                    {
                        if (last_space > 0)
                            used = last_space;
                        line[used] = '\0';
                        break;
                    }
                    used++;
                }

                livetv_text_at(4, y, LIVETV_TEXT, LIVETV_DESC_BG, line);
                y += line_h;
                p += used;
                while (*p == ' ')
                    p++;
            }
        }
    }
    else
    {
        livetv_text_at(4, LIVETV_DESC_Y + 2, LIVETV_TEXT, LIVETV_DESC_BG,
                       "No programming scheduled.");
        livetv_text_at(4, LIVETV_DESC_Y + 2 + text_h + 1, LIVETV_TEXT,
                       LIVETV_DESC_BG, "Sync shows from rockpod Live TV.");
    }

    /* White frame around the picture in guide window */
    if (livetv_pig_active())
    {
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_drawrect(LIVETV_PIG_BOX_X, LIVETV_PIG_BOX_Y,
                         LIVETV_PIG_BOX_W, LIVETV_PIG_BOX_H);
    }
}

static void livetv_draw_time_header(void)
{
    long base = livetv_window_base();
    int text_h = livetv_font_height();
    int y = LIVETV_HDR_Y + (LIVETV_HDR_H - text_h) / 2;
    char buf[24];

    livetv_fill(0, LIVETV_HDR_Y, LCD_WIDTH, LIVETV_HDR_H, LIVETV_HDR_BG);

    livetv_format_date(buf, sizeof(buf), base);
    livetv_text_fit(3, y, LIVETV_CHANCOL_W - 6, LIVETV_TEXT, LIVETV_HDR_BG,
                    buf);

    for (int col = 0; col < LIVETV_GRID_COLS; col++)
    {
        int day;
        uint32_t secs;

        livetv_resolve_time(base + (long)col * LIVETV_SLOT_SECONDS, &day,
                            &secs);
        livetv_format_clock(buf, sizeof(buf), secs);
        livetv_text_at(livetv_col_x(col) + 3, y, LIVETV_TEXT, LIVETV_HDR_BG,
                       buf);
    }
}

static void livetv_draw_grid_row(int row, bool selected_row)
{
    int view_index = guide.row_top + row;
    int y = LIVETV_GRID_Y + row * LIVETV_ROW_H;
    int text_h = livetv_font_height();
    int text_y = y + (LIVETV_ROW_H - text_h) / 2;
    long base = livetv_window_base();
    long window_end = base + LIVETV_GRID_COLS * LIVETV_SLOT_SECONDS;
    int chan;
    const struct livetv_channel *ch;
    struct bitmap *logo;
    long probe;
    char buf[96];

    livetv_fill(0, y, LCD_WIDTH, LIVETV_ROW_H, LIVETV_ROW_BG);
    livetv_fill(0, y, LIVETV_CHANCOL_W, LIVETV_ROW_H, LIVETV_CHAN_BG);
    livetv_fill(0, y + LIVETV_ROW_H - 1, LCD_WIDTH, 1, LIVETV_GRID_LINE);

    if (view_index >= livetv_view_num)
        return;

    chan = livetv_view[view_index];
    ch = livetv_channel(chan);
    if (ch == NULL)
        return;

    /* Channel column: real logo when one is installed, otherwise the
     * DIRECTV-correct number and call sign. */
    logo = livetv_channel_logo(chan);
    if (logo != NULL && logo->height <= LIVETV_ROW_H - 2)
    {
        rb->lcd_bitmap((const fb_data *)logo->data, 2,
                       y + (LIVETV_ROW_H - logo->height) / 2,
                       MIN(logo->width, LIVETV_CHANCOL_W - 4), logo->height);
    }
    else
    {
        rb->snprintf(buf, sizeof(buf), "%d %s", ch->number, ch->callsign);
        livetv_text_fit(3, text_y, LIVETV_CHANCOL_W - 6,
                        ch->slot_count > 0 ? LIVETV_TEXT : LIVETV_DIM_TEXT,
                        LIVETV_CHAN_BG, buf);
    }

    /* Programme cells across the visible time window */
    probe = base;
    while (probe < window_end)
    {
        const struct livetv_slot *slot;
        uint32_t offset = 0;
        long slot_start, slot_end;
        int x1, x2;
        bool selected;
        bool starts_before;
        bool ends_after;
        unsigned bg, fg;

        slot = livetv_slot_at(chan, probe < 0 ? 0 : probe, &offset);
        if (slot == NULL)
        {
            /* Nothing scheduled: dim filler for the rest of the column */
            long next = probe + LIVETV_SLOT_SECONDS;

            x1 = livetv_col_x(0) + (int)((probe - base) * LIVETV_COL_W /
                                         LIVETV_SLOT_SECONDS);
            x2 = livetv_col_x(0) + (int)((MIN(next, window_end) - base) *
                                         LIVETV_COL_W / LIVETV_SLOT_SECONDS);
            if (x2 > x1 + 2)
                livetv_text_fit(x1 + 4, text_y, x2 - x1 - 8, LIVETV_DIM_TEXT,
                                LIVETV_ROW_BG, "No Programming");
            probe = next;
            continue;
        }

        /* Draw the whole programme, commercial break included. A guide does
         * not list the adverts inside a show. */
        slot_start = probe - (long)offset -
                     ((long)slot->start - (long)slot->block_start);
        slot_end = slot_start + (long)slot->block_dur;

        starts_before = slot_start < base;
        ends_after = slot_end > window_end;

        x1 = livetv_col_x(0) + (int)((MAX(slot_start, base) - base) *
                                     LIVETV_COL_W / LIVETV_SLOT_SECONDS);
        x2 = livetv_col_x(0) + (int)((MIN(slot_end, window_end) - base) *
                                     LIVETV_COL_W / LIVETV_SLOT_SECONDS);
        if (x2 > LCD_WIDTH)
            x2 = LCD_WIDTH;
        if (x2 <= x1)
            x2 = x1 + 2;

        selected = selected_row && guide.cursor >= slot_start &&
                   guide.cursor < slot_end;

        bg = selected ? LIVETV_SEL_BG : LIVETV_ROW_BG;
        fg = selected ? LIVETV_SEL_TEXT : LIVETV_TEXT;

        if (selected)
            livetv_fill(x1, y + 1, x2 - x1 - 1, LIVETV_ROW_H - 3, bg);

        /* Cell divider, as on the real grid */
        if (!starts_before)
            livetv_fill(x1, y + 1, 1, LIVETV_ROW_H - 3, LIVETV_GRID_LINE);

        int tx = x1 + 3;
        int avail = x2 - x1 - 7;

        if (starts_before)
        {
            livetv_text_at(tx, text_y, fg, bg, "<");
            tx += 6;
            avail -= 6;
        }

        if (avail > 8)
            livetv_text_fit(tx, text_y, avail, fg, bg,
                            livetv_slot_title(slot));

        if (ends_after)
        {
            livetv_text_at(LCD_WIDTH - 7, text_y,
                           selected ? LIVETV_SEL_TEXT : LIVETV_DIM_TEXT,
                           bg, ">");
        }

        probe = slot_end;
    }
}

static void livetv_draw_hint_bar(void)
{
    int text_h = livetv_font_height();
    int y = LIVETV_HINT_Y + (LIVETV_HINT_H - text_h) / 2;
    int dot_y = LIVETV_HINT_Y + LIVETV_HINT_H / 2 - 3;

    livetv_fill(0, LIVETV_HINT_Y, LCD_WIDTH, LIVETV_HINT_H, LIVETV_HINT_BG);
    livetv_text_fit(4, y, 90, LIVETV_TEXT, LIVETV_HINT_BG,
                    livetv_guide_filter_name());

    rb->lcd_set_foreground(LIVETV_DOT_RED);
    rb->lcd_fillrect(112, dot_y, 6, 6);
    livetv_text_at(121, y, LIVETV_TEXT, LIVETV_HINT_BG, "-12h");

    rb->lcd_set_foreground(LIVETV_DOT_GREEN);
    rb->lcd_fillrect(158, dot_y, 6, 6);
    livetv_text_at(167, y, LIVETV_TEXT, LIVETV_HINT_BG, "+12h");

    rb->lcd_set_foreground(LIVETV_DOT_YELLOW);
    rb->lcd_fillrect(206, dot_y, 6, 6);
    livetv_text_fit(215, y, LCD_WIDTH - 219, LIVETV_TEXT, LIVETV_HINT_BG,
                    "Guide Options");
}

void livetv_guide_draw(void)
{
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_UI);

    livetv_draw_banner_area();
    livetv_draw_time_header();
    for (int row = 0; row < LIVETV_GRID_ROWS; row++)
        livetv_draw_grid_row(row, guide.row_top + row == guide.row_sel);
    livetv_draw_hint_bar();

    livetv_update(0, 0, LCD_WIDTH, LCD_HEIGHT);
    guide.full_redraw = false;
    guide.prev_row_sel = guide.row_sel;
}

/* The banner, the highlight and the clock all move together, and the grid is
 * only ever a handful of filled rectangles, so the guide repaints whole.
 * Painting regions piecemeal left stale text behind wherever a new string
 * was shorter than the one it replaced. */
void livetv_guide_draw_rows(void)
{
    livetv_guide_draw();
}

/* Guide Options -------------------------------------------------------- */

static const char * const livetv_option_labels[LIVETV_OPTION_COUNT] = {
    "Sort programs by category",
    "Jump to a date & time",
    "Change favorites list",
    "Back to guide",
};

const char *livetv_options_label(int index)
{
    if (index < 0 || index >= LIVETV_OPTION_COUNT)
        return "";
    return livetv_option_labels[index];
}

void livetv_options_draw(int selected)
{
    int text_h = livetv_font_height();
    int row_h = text_h + 6;
    int w = 190;
    int h = row_h * (LIVETV_OPTION_COUNT + 1) + 4;
    int x = (LCD_WIDTH - w) / 2;
    int y = LIVETV_HDR_Y + 6;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_UI);

    /* Yellow header over a DIRECTV blue body, as on the receiver. */
    rb->lcd_set_foreground(LIVETV_SEL_BG);
    rb->lcd_fillrect(x, y, w, row_h);
    livetv_text_at(x + 6, y + 3, LIVETV_SEL_TEXT, LIVETV_SEL_BG,
                   "Guide Options");

    rb->lcd_set_foreground(LIVETV_DESC_BG);
    rb->lcd_fillrect(x, y + row_h, w, h - row_h);

    for (int i = 0; i < LIVETV_OPTION_COUNT; i++)
    {
        int row_y = y + row_h * (i + 1) + 2;
        unsigned bg = LIVETV_DESC_BG;
        unsigned fg = LIVETV_TEXT;

        if (i == selected)
        {
            bg = LIVETV_SEL_BG;
            fg = LIVETV_SEL_TEXT;
            rb->lcd_set_foreground(bg);
            rb->lcd_fillrect(x + 2, row_y - 1, w - 4, row_h);
        }

        livetv_text_fit(x + 6, row_y + 2, w - 14, fg, bg,
                        livetv_option_labels[i]);
    }

    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_drawrect(x, y, w, h);
    rb->lcd_update_rect(x, y, w, h);
}

bool livetv_options_activate(int index)
{
    switch (index)
    {
    case 0: /* Sort programs by category */
        livetv_guide_cycle_filter();
        return true;

    case 1: /* Jump to a date & time */
        livetv_guide_jump_hours(12);
        return true;

    case 2: /* Change favorites list */
    {
        int chan = livetv_guide_selected_channel();

        if (chan >= 0 && chan < livetv_channel_num)
            livetv_channels[chan].favourite = !livetv_channels[chan].favourite;
        return true;
    }

    default:
        return true;
    }
}

/* Full screen overlays -------------------------------------------------- */

void livetv_draw_info_banner(int chan)
{
    const struct livetv_slot *slot;
    const struct livetv_channel *ch = livetv_channel(chan);
    uint32_t offset = 0;
    int text_h;
    int y = LCD_HEIGHT - LIVETV_INFO_H;
    char buf[96];
    char clock[16];

    if (ch == NULL)
        return;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_UI);
    text_h = livetv_font_height();

    slot = livetv_slot_at(chan, 0, &offset);

    rb->lcd_set_foreground(LIVETV_HDR_BG);
    rb->lcd_fillrect(0, y, LCD_WIDTH, LIVETV_INFO_H);
    rb->lcd_set_foreground(LIVETV_DESC_BG);
    rb->lcd_fillrect(0, y + 1, LCD_WIDTH, LIVETV_INFO_H - 2);

    rb->snprintf(buf, sizeof(buf), "%d %s", ch->number, ch->callsign);
    livetv_text_fit(4, y + 3, 90, LIVETV_SEL_BG, LIVETV_DESC_BG, buf);

    livetv_format_clock(clock, sizeof(clock), livetv_now_secs());
    rb->snprintf(buf, sizeof(buf), "%s %s", livetv_wday[livetv_now_day()],
                 clock);
    livetv_text_fit(LCD_WIDTH - 80, y + 3, 76, LIVETV_TEXT, LIVETV_DESC_BG,
                    buf);

    livetv_text_fit(4, y + 4 + text_h, LCD_WIDTH - 8, LIVETV_TEXT,
                    LIVETV_DESC_BG,
                    slot != NULL ? livetv_slot_title(slot) : "No Programming");

    if (slot != NULL)
    {
        char start_txt[16], end_txt[16];

        livetv_format_clock(start_txt, sizeof(start_txt), slot->block_start);
        livetv_format_clock(end_txt, sizeof(end_txt),
                            (slot->block_start + slot->block_dur) %
                            LIVETV_DAY_SECONDS);
        rb->snprintf(buf, sizeof(buf), "%s - %s   %s", start_txt, end_txt,
                     livetv_slot_rating(slot));
        livetv_text_fit(4, y + 5 + text_h * 2, LCD_WIDTH - 8, LIVETV_DIM_TEXT,
                        LIVETV_DESC_BG, buf);
    }

    rb->lcd_update_rect(0, y, LCD_WIDTH, LIVETV_INFO_H);
}

void livetv_draw_mini_guide(int chan)
{
    const struct livetv_slot *now_slot;
    const struct livetv_slot *next_slot;
    const struct livetv_channel *ch = livetv_channel(chan);
    int y = LCD_HEIGHT - LIVETV_MINI_H;
    int text_h;
    int half = (LCD_WIDTH - 70) / 2;
    char buf[96];

    if (ch == NULL)
        return;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_UI);
    text_h = livetv_font_height();

    now_slot = livetv_slot_at(chan, 0, NULL);
    next_slot = livetv_slot_next(chan, 0);

    rb->lcd_set_foreground(LIVETV_HDR_BG);
    rb->lcd_fillrect(0, y, LCD_WIDTH, text_h + 4);
    rb->lcd_set_foreground(LIVETV_ROW_BG);
    rb->lcd_fillrect(0, y + text_h + 4, LCD_WIDTH, LIVETV_MINI_H - text_h - 4);

    livetv_format_clock(buf, sizeof(buf), livetv_now_secs());
    livetv_text_at(4, y + 2, LIVETV_TEXT, LIVETV_HDR_BG, buf);
    if (next_slot != NULL)
    {
        char clock[16];

        livetv_format_clock(clock, sizeof(clock), next_slot->start);
        livetv_text_at(70 + half, y + 2, LIVETV_TEXT, LIVETV_HDR_BG, clock);
    }

    rb->snprintf(buf, sizeof(buf), "%d %s", ch->number, ch->callsign);
    rb->lcd_set_foreground(LIVETV_CHAN_BG);
    rb->lcd_fillrect(0, y + text_h + 4, 68, LIVETV_MINI_H - text_h - 4);
    livetv_text_fit(3, y + text_h + 6, 62, LIVETV_TEXT, LIVETV_CHAN_BG, buf);

    rb->lcd_set_foreground(LIVETV_SEL_BG);
    rb->lcd_fillrect(70, y + text_h + 5, half - 2,
                     LIVETV_MINI_H - text_h - 7);
    livetv_text_fit(73, y + text_h + 6, half - 8, LIVETV_SEL_TEXT,
                    LIVETV_SEL_BG,
                    now_slot != NULL ? livetv_slot_title(now_slot) : "--");

    livetv_text_fit(72 + half, y + text_h + 6, half - 8, LIVETV_TEXT,
                    LIVETV_ROW_BG,
                    next_slot != NULL ? livetv_slot_title(next_slot) : "--");

    rb->lcd_update_rect(0, y, LCD_WIDTH, LIVETV_MINI_H);
}

void livetv_clear_overlay(void)
{
    /* The video thread repaints the whole frame, so simply asking for a
     * redraw is enough; the caller does that after clearing. */
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(0, LCD_HEIGHT - LIVETV_INFO_H, LCD_WIDTH,
                     LIVETV_INFO_H);
    rb->lcd_update_rect(0, LCD_HEIGHT - LIVETV_INFO_H, LCD_WIDTH,
                        LIVETV_INFO_H);
}

/* Guide session --------------------------------------------------------- */

/* Guide controls. The wheel walks channels, left and right walk the grid in
 * time, SELECT tunes and MENU leaves Live TV, which is how the receiver's
 * guide behaves and how an iPod application is expected to behave. */
#if (CONFIG_KEYPAD == IPOD_4G_PAD) || (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_1G2G_PAD)
#define LIVETV_BTN_UP       BUTTON_SCROLL_BACK
#define LIVETV_BTN_DOWN     BUTTON_SCROLL_FWD
#define LIVETV_BTN_LEFT     BUTTON_LEFT
#define LIVETV_BTN_RIGHT    BUTTON_RIGHT
#define LIVETV_BTN_SELECT   (BUTTON_SELECT | BUTTON_REL)
#define LIVETV_BTN_EXIT     BUTTON_MENU
#define LIVETV_BTN_OPTIONS  (BUTTON_PLAY | BUTTON_REPEAT)
#define LIVETV_BTN_BACK12   (BUTTON_LEFT | BUTTON_REPEAT)
#define LIVETV_BTN_FWD12    (BUTTON_RIGHT | BUTTON_REPEAT)
#else
#define LIVETV_BTN_UP       BUTTON_UP
#define LIVETV_BTN_DOWN     BUTTON_DOWN
#define LIVETV_BTN_LEFT     BUTTON_LEFT
#define LIVETV_BTN_RIGHT    BUTTON_RIGHT
#define LIVETV_BTN_SELECT   BUTTON_SELECT
#define LIVETV_BTN_EXIT     BUTTON_POWER
#define LIVETV_BTN_OPTIONS  (BUTTON_SELECT | BUTTON_REPEAT)
#define LIVETV_BTN_BACK12   (BUTTON_LEFT | BUTTON_REPEAT)
#define LIVETV_BTN_FWD12    (BUTTON_RIGHT | BUTTON_REPEAT)
#endif

static int livetv_options_run(void)
{
    int selected = 0;
    bool done = false;
    bool repaint = false;

    rb->button_clear_queue();
    livetv_options_draw(selected);

    while (!done)
    {
        int button = mpeg_button_get(HZ / 4);

        if (mpeg_sysevent() != 0)
            return -1;

        switch (button)
        {
        case LIVETV_BTN_UP:
        case LIVETV_BTN_UP | BUTTON_REPEAT:
            selected = (selected + LIVETV_OPTION_COUNT - 1) %
                       LIVETV_OPTION_COUNT;
            livetv_options_draw(selected);
            break;

        case LIVETV_BTN_DOWN:
        case LIVETV_BTN_DOWN | BUTTON_REPEAT:
            selected = (selected + 1) % LIVETV_OPTION_COUNT;
            livetv_options_draw(selected);
            break;

        case LIVETV_BTN_SELECT:
            repaint = livetv_options_activate(selected);
            done = true;
            break;

        case LIVETV_BTN_EXIT:
        case LIVETV_BTN_LEFT:
            done = true;
            break;

        default:
            break;
        }
    }

    return repaint ? 1 : 0;
}

int livetv_guide_run(void)
{
    int result = LIVETV_GUIDE_EXIT;
    long next_tick = *rb->current_tick + HZ;
    bool done = false;

    if (!livetv_ready)
        return LIVETV_GUIDE_EXIT;

    livetv_guide_enter();
    rb->button_clear_queue();
    livetv_guide_draw();

    while (!done)
    {
        int button = mpeg_button_get(HZ / 4);

        if (mpeg_sysevent() != 0)
            return LIVETV_GUIDE_EXIT;

        switch (button)
        {
        case BUTTON_NONE:
            /* Keep the clock honest and let the grid roll forward on its
             * own, as a receiver left sitting on the guide does. */
            if (!TIME_BEFORE(*rb->current_tick, next_tick))
            {
                next_tick = *rb->current_tick + HZ;
                livetv_guide_draw_rows();
            }
            break;

        case LIVETV_BTN_UP:
        case LIVETV_BTN_UP | BUTTON_REPEAT:
            livetv_guide_move(-1, 0);
            livetv_guide_draw_rows();
            break;

        case LIVETV_BTN_DOWN:
        case LIVETV_BTN_DOWN | BUTTON_REPEAT:
            livetv_guide_move(1, 0);
            livetv_guide_draw_rows();
            break;

        case LIVETV_BTN_BACK12:
            livetv_guide_jump_hours(-12);
            livetv_guide_draw();
            break;

        case LIVETV_BTN_FWD12:
            livetv_guide_jump_hours(12);
            livetv_guide_draw();
            break;

        case LIVETV_BTN_LEFT:
            livetv_guide_move(0, -1);
            livetv_guide_draw_rows();
            break;

        case LIVETV_BTN_RIGHT:
            livetv_guide_move(0, 1);
            livetv_guide_draw_rows();
            break;

        case LIVETV_BTN_OPTIONS:
        {
            int ret = livetv_options_run();

            if (ret < 0)
                return LIVETV_GUIDE_EXIT;
            rb->button_clear_queue();
            livetv_guide_draw();
            break;
        }

        case LIVETV_BTN_SELECT:
        {
            int chan = livetv_guide_selected_channel();

            /* Selecting a programme that is not on yet snaps the guide to
             * it rather than tuning, the same as the receiver. */
            if (!livetv_guide_selection_is_live())
            {
                livetv_guide_reset_to_now();
                livetv_guide_draw();
                break;
            }

            result = (chan == livetv_channel_cur) ? LIVETV_GUIDE_WATCH
                                                  : LIVETV_GUIDE_TUNE;
            livetv_channel_cur = chan;
            livetv_save_state();
            done = true;
            break;
        }

        case LIVETV_BTN_EXIT:
            result = LIVETV_GUIDE_EXIT;
            done = true;
            break;

        default:
            break;
        }
    }

    return result;
}

#endif /* HAVE_LCD_COLOR */
