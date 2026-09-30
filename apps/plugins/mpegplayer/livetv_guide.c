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
#define LIVETV_GUIDE_W      \
    (mpegplayer_livetv_desktop ? LIVETV_DM_WIN_W : LCD_WIDTH)
#define LIVETV_GUIDE_H      \
    (mpegplayer_livetv_desktop ? LIVETV_DM_BODY_H : LCD_HEIGHT)
#define LIVETV_GRID_ROWS    \
    (mpegplayer_livetv_desktop ? \
        (LIVETV_GUIDE_H - LIVETV_GRID_Y - 18) / LIVETV_ROW_H : 6)
#define LIVETV_GRID_H       (LIVETV_ROW_H * LIVETV_GRID_ROWS)
#define LIVETV_HINT_Y       (LIVETV_GRID_Y + LIVETV_GRID_H)
#define LIVETV_HINT_H       (LIVETV_GUIDE_H - LIVETV_HINT_Y)
/* Wide enough for the DIRECTV style "100 RTRO" call sign at the UI font. */
#define LIVETV_CHANCOL_W    68
#define LIVETV_GRID_COLS    3
#define LIVETV_SHORT_CLIP_SECONDS 300
#define LIVETV_SHORT_GUIDE_BLOCK  900
#define LIVETV_COL_W        \
    ((LIVETV_GUIDE_W - LIVETV_CHANCOL_W) / LIVETV_GRID_COLS)
#define LIVETV_STRIP_CELL_W 78

#define LIVETV_LOGO_W       40
#define LIVETV_LOGO_H       18
#define LIVETV_LOGO_CACHE   8
#define LIVETV_BRAND_W      64
#define LIVETV_BRAND_H      22
#define LIVETV_SLOT_TITLE_FALLBACK 96

#define LIVETV_BANNER_SECS  (5 * HZ)
#define LIVETV_PARENTAL_PIN ROCKBOX_DIR "/videolist/locked.pin"

/* Model -------------------------------------------------------------- */

static struct livetv_channel livetv_channels[LIVETV_MAX_CHANNELS];
static int livetv_channel_num;

/* Sized for every current/tomorrow listing published by the Live TV sync.
 * Keep this static: guide rendering must not borrow playback memory. */
static struct livetv_slot livetv_slots[LIVETV_MAX_SLOTS];
static int livetv_slot_num;

static char livetv_text_pool[LIVETV_TEXT_POOL];
static int livetv_text_used;
static char livetv_path_pool[LIVETV_PATH_POOL];
static int livetv_path_used;

static char livetv_root[MAX_PATH];
static int livetv_channel_cur;
static bool livetv_ready;
static bool livetv_parental_unlocked;

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
static struct bitmap livetv_dm_title;
static fb_data livetv_dm_title_data[LIVETV_DM_WIN_W * LIVETV_DM_TITLE_H];
static bool livetv_dm_title_valid;
static bool livetv_dm_title_tried;

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
        char *fields[7];
        char *text = livetv_trim(line);
        int count;
        struct livetv_channel *chan;

        if (text[0] == '\0' || text[0] == '#')
            continue;

        count = livetv_split(text, fields, 7);
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
        if (count > 6)
            chan->parental_locked = rb->atoi(fields[6]) != 0;
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
        rb->memset(slot, 0, sizeof(*slot));
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

        if (slot->dur == 0 || slot->path_off == 0)
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

        if (chan->parental_locked && !livetv_parental_unlocked)
            continue;
        if (guide.filter == LIVETV_FILTER_FAVOURITES && !chan->favourite)
            continue;
        if (guide.filter == LIVETV_FILTER_SHOWS && chan->slot_count == 0)
            continue;
        livetv_view[livetv_view_num++] = i;
    }

    if (livetv_view_num == 0)
    {
        /* Never bypass a parental lock while falling back from a filter. */
        guide.filter = LIVETV_FILTER_ALL;
        for (int i = 0; i < livetv_channel_num; i++)
            if (!livetv_channels[i].parental_locked ||
                livetv_parental_unlocked)
                livetv_view[livetv_view_num++] = i;
    }
}

bool livetv_load(const char *root)
{
    livetv_ready = false;
    livetv_parental_unlocked = false;
    rb->memset(livetv_logos, 0, sizeof(livetv_logos));
    livetv_brand_valid = false;
    livetv_brand_tried = false;
    livetv_dm_title_valid = false;
    livetv_dm_title_tried = false;

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
    if (livetv_channel_cur >= 0 &&
        livetv_channels[livetv_channel_cur].parental_locked &&
        livetv_view_num > 0)
        livetv_channel_cur = livetv_view[0];
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

bool livetv_step_channel(int delta)
{
    char path[MAX_PATH];
    int probe;
    int step;
    int tried;

    if (livetv_channel_num <= 1 || delta == 0)
        return false;

    step = delta > 0 ? 1 : -1;
    probe = livetv_channel_cur;

    /* Walk the lineup at most once around, so a run of channels with
     * nothing on the air cannot spin here forever. */
    for (tried = 0; tried < livetv_channel_num; tried++)
    {
        probe += step;
        if (probe < 0)
            probe = livetv_channel_num - 1;
        else if (probe >= livetv_channel_num)
            probe = 0;

        if (probe == livetv_channel_cur)
            break;
        if (livetv_channels[probe].parental_locked &&
            !livetv_parental_unlocked)
            continue;
        if (!livetv_tune(probe, path, sizeof(path), NULL))
            continue;
        return true;
    }

    return false;
}

const char *livetv_slot_title(const struct livetv_slot *slot)
{
    if (slot == NULL)
        return "";
    return &livetv_text_pool[slot->title_off];
}

static const char *livetv_slot_display_title(const struct livetv_slot *slot)
{
    static char fallback[LIVETV_SLOT_TITLE_FALLBACK];
    const char *title = slot != NULL ? &livetv_text_pool[slot->title_off] : "";
    const char *path;
    char *base;
    char *dot;

    if (title[0] != '\0')
        return title;
    if (slot == NULL || slot->path_off == 0)
        return "Program";

    path = &livetv_path_pool[slot->path_off];
    if (path[0] == '\0')
        return "Program";

    rb->strlcpy(fallback, path, sizeof(fallback));
    if (fallback[0] == '\0')
        return "Program";

    base = rb->strrchr(fallback, '/');
    if (base == NULL)
        base = rb->strrchr(fallback, '\\');
    if (base != NULL)
        base++;
    else
        base = fallback;

    rb->strlcpy(fallback, base, sizeof(fallback));
    if (fallback[0] == '\0')
        return "Program";

    dot = rb->strrchr(fallback, '.');
    if (dot != NULL && dot > fallback)
        *dot = '\0';

    return fallback[0] == '\0' ? "Program" : fallback;
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
        return rb->strlcpy(buf, rel, size) < size;
    return rb->snprintf(buf, size, "%s/%s", livetv_root, rel) < (int)size;
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
    int fd;
    off_t filesize;

    if (!livetv_ready || chan < 0 || chan >= livetv_channel_num ||
        (livetv_channels[chan].parental_locked && !livetv_parental_unlocked))
        return false;

    slot = livetv_slot_at(chan, 0, &offset);
    if (slot == NULL)
        return false;
    if (!livetv_slot_path(slot, videofile, size))
        return false;

    fd = rb->open(videofile, O_RDONLY);
    if (fd < 0)
        return false;
    filesize = rb->filesize(fd);
    rb->close(fd);
    if (filesize <= 0 || (uint64_t)filesize > 0x7fffffffUL)
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

static bool livetv_tv_drawing, livetv_tv_restore_lcd;
static void livetv_tv(enum tv_guide_command command, int x, int y,
                       int w, int h, unsigned color, const void *data)
{
#ifdef HAVE_COMPOSITE_VIDEO_OUT
    if (livetv_tv_drawing || command==TV_GUIDE_RELEASE)
    {
        bool owned=rb->tv_guide_render(command,x,y,w,h,color,data);
        if (command==TV_GUIDE_RELEASE && owned) livetv_tv_restore_lcd=true;
    }
#else
    (void)command; (void)x; (void)y; (void)w; (void)h; (void)color; (void)data;
#endif
}

static void livetv_commit_rect(int x, int y, int w, int h)
{
    if (livetv_tv_restore_lcd)
    {
        livetv_tv_restore_lcd=false;
        rb->lcd_update();
    }
    else rb->lcd_update_rect(x,y,w,h);
}

/* Fill a rectangle, splitting it around the picture in guide window so the
 * decoded video is never painted over. */
static void livetv_fill(int x, int y, int w, int h, unsigned color)
{
    livetv_tv(TV_GUIDE_FILL,x,y,w,h,color,NULL);
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
    int origin_x = mpegplayer_livetv_desktop ? LIVETV_DM_WIN_X : 0;
    int origin_y = mpegplayer_livetv_desktop ?
                   LIVETV_DM_WIN_Y + LIVETV_DM_TITLE_H : 0;
    int y2 = y + h;
    int py = LIVETV_PIG_BOX_Y;
    int py2 = py + LIVETV_PIG_BOX_H;

    if (w <= 0 || h <= 0)
        return;

    if (!livetv_pig_active() || y2 <= py || y >= py2)
    {
        livetv_commit_rect(origin_x + x, origin_y + y, w, h);
        return;
    }

    if (y < py)
        livetv_commit_rect(origin_x + x, origin_y + y, w, py - y);
    if (y2 > py2)
        livetv_commit_rect(origin_x + x, origin_y + py2, w, y2 - py2);
    if (x < LIVETV_PIG_BOX_X)
        livetv_commit_rect(
            origin_x + x, origin_y + MAX(y, py),
            MIN(x + w, LIVETV_PIG_BOX_X) - x,
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
        livetv_fill(0, y + i, LIVETV_GUIDE_W, 1, color);
    }
}

static int livetv_font_height(void);

static void livetv_text_at(int x, int y, unsigned fg, unsigned bg,
                           const char *text)
{
    livetv_tv(TV_GUIDE_TEXT,x,y,MAX(0,LIVETV_GUIDE_W-x),
              livetv_font_height(),fg,text);
    rb->lcd_set_foreground(fg);
    rb->lcd_set_background(bg);
    rb->lcd_putsxy(x, y, text);
}

static void livetv_text_fit(int x, int y, int max_width, unsigned fg,
                            unsigned bg, const char *text)
{
    char buf[96];

    livetv_tv(TV_GUIDE_TEXT,x,y,max_width,livetv_font_height(),fg,text);
    livetv_fit(buf, sizeof(buf), text, max_width);
    rb->lcd_set_foreground(fg);
    rb->lcd_set_background(bg);
    rb->lcd_putsxy(x,y,buf);
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
    int desc_right = livetv_pig_active() ? LIVETV_PIG_BOX_X - 4 :
                                           LIVETV_GUIDE_W - 4;

    slot = livetv_slot_at(chan, guide.cursor, &offset);

    /* Banner gradient and brand mark */
    livetv_gradient(0, LIVETV_BANNER_H, LIVETV_BANNER_TOP, LIVETV_BANNER_BOT);

    brand = livetv_brand_logo();
    if (brand != NULL)
    {
        livetv_tv(TV_GUIDE_BITMAP,4,(LIVETV_BANNER_H-brand->height)/2,
                  brand->width,brand->height,0,brand);
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
                    slot != NULL ? livetv_slot_display_title(slot) : "No Programming");

    /* Information strip: clock cell, air window, rating */
    livetv_fill(0, LIVETV_STRIP_Y, LIVETV_GUIDE_W, LIVETV_STRIP_H,
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
    livetv_fill(0, LIVETV_DESC_Y, LIVETV_GUIDE_W, LIVETV_DESC_H,
                LIVETV_DESC_BG);

    if (slot != NULL)
    {
        const char *desc = livetv_slot_desc(slot);
        int y = LIVETV_DESC_Y + 2;
        int line_h = text_h + 1;
        int lines = MAX(1, (LIVETV_DESC_H - 4) / line_h);

        if (desc[0] == '\0')
            desc = livetv_channel(chan)->name;

        livetv_tv(TV_GUIDE_PARAGRAPH,4,LIVETV_DESC_Y+4,
                  desc_right-8,LIVETV_DESC_H-4,LIVETV_TEXT,desc);
        bool drawing=livetv_tv_drawing;
        livetv_tv_drawing=false;
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

                livetv_text_fit(4, y, desc_right-8, LIVETV_TEXT, LIVETV_DESC_BG, line);
                y += line_h;
                p += used;
                while (*p == ' ')
                    p++;
            }
        }
        livetv_tv_drawing=drawing;
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

    livetv_fill(0, LIVETV_HDR_Y, LIVETV_GUIDE_W, LIVETV_HDR_H,
                LIVETV_HDR_BG);

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

static bool livetv_channel_short_clips(int chan)
{
    const struct livetv_channel *ch = livetv_channel(chan);

    if (ch == NULL || ch->slot_count == 0)
        return false;
    for (int i = ch->first_slot; i < ch->first_slot + ch->slot_count; i++)
        if (livetv_slots[i].block_dur > LIVETV_SHORT_CLIP_SECONDS)
            return false;
    return true;
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

    livetv_fill(0, y, LIVETV_GUIDE_W, LIVETV_ROW_H, LIVETV_ROW_BG);
    livetv_fill(0, y, LIVETV_CHANCOL_W, LIVETV_ROW_H, LIVETV_CHAN_BG);
    livetv_fill(0, y + LIVETV_ROW_H - 1, LIVETV_GUIDE_W, 1,
                LIVETV_GRID_LINE);

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
        livetv_tv(TV_GUIDE_BITMAP,2,y+(LIVETV_ROW_H-logo->height)/2,
                  MIN(logo->width,LIVETV_CHANCOL_W-4),logo->height,0,logo);
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

        /* Preserve the real clip timeline for playback, while rendering an
         * all-short-clip station as conventional guide-sized blocks. */
        if (livetv_channel_short_clips(chan))
        {
            slot_start = (probe / LIVETV_SHORT_GUIDE_BLOCK) *
                         LIVETV_SHORT_GUIDE_BLOCK;
            slot_end = slot_start + LIVETV_SHORT_GUIDE_BLOCK;
        }

        starts_before = slot_start < base;
        ends_after = slot_end > window_end;

        x1 = livetv_col_x(0) + (int)((MAX(slot_start, base) - base) *
                                     LIVETV_COL_W / LIVETV_SLOT_SECONDS);
        x2 = livetv_col_x(0) + (int)((MIN(slot_end, window_end) - base) *
                                     LIVETV_COL_W / LIVETV_SLOT_SECONDS);
        if (x2 > LIVETV_GUIDE_W)
            x2 = LIVETV_GUIDE_W;
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
                            livetv_slot_display_title(slot));

        if (ends_after)
        {
            livetv_text_at(LIVETV_GUIDE_W - 7, text_y,
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

    livetv_fill(0, LIVETV_HINT_Y, LIVETV_GUIDE_W, LIVETV_HINT_H,
                LIVETV_HINT_BG);
    livetv_text_fit(4, y, 90, LIVETV_TEXT, LIVETV_HINT_BG,
                    livetv_guide_filter_name());

    rb->lcd_set_foreground(LIVETV_DOT_RED);
    livetv_tv(TV_GUIDE_FILL,112,dot_y,6,6,LIVETV_DOT_RED,NULL);
    rb->lcd_fillrect(112, dot_y, 6, 6);
    livetv_text_at(121, y, LIVETV_TEXT, LIVETV_HINT_BG, "-12h");

    rb->lcd_set_foreground(LIVETV_DOT_GREEN);
    livetv_tv(TV_GUIDE_FILL,158,dot_y,6,6,LIVETV_DOT_GREEN,NULL);
    rb->lcd_fillrect(158, dot_y, 6, 6);
    livetv_text_at(167, y, LIVETV_TEXT, LIVETV_HINT_BG, "+12h");

    rb->lcd_set_foreground(LIVETV_DOT_YELLOW);
    livetv_tv(TV_GUIDE_FILL,206,dot_y,6,6,LIVETV_DOT_YELLOW,NULL);
    rb->lcd_fillrect(206, dot_y, 6, 6);
    livetv_text_fit(215, y, LIVETV_GUIDE_W - 219, LIVETV_TEXT,
                    LIVETV_HINT_BG,
                    "Guide Options");
}

void livetv_guide_draw(void)
{
    livetv_tv_restore_lcd=false;
#ifdef HAVE_COMPOSITE_VIDEO_OUT
    livetv_tv_drawing = !mpegplayer_livetv_desktop &&
        rb->tv_guide_render(TV_GUIDE_BEGIN,0,0,0,0,livetv_pig_active(),NULL);
#endif
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_UI);

    livetv_draw_banner_area();
    livetv_draw_time_header();
    for (int row = 0; row < LIVETV_GRID_ROWS; row++)
        livetv_draw_grid_row(row, guide.row_top + row == guide.row_sel);
    livetv_draw_hint_bar();

    livetv_tv(TV_GUIDE_PRESENT,0,0,0,0,0,NULL);
    livetv_tv_drawing=false;
    livetv_update(0, 0, LIVETV_GUIDE_W, LIVETV_GUIDE_H);
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
    livetv_tv(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
    int text_h = livetv_font_height();
    int row_h = text_h + 6;
    int w = 190;
    int h = row_h * (LIVETV_OPTION_COUNT + 1) + 4;
    int x = (LIVETV_GUIDE_W - w) / 2;
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
    livetv_commit_rect(x, y, w, h);
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
    livetv_tv(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
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
                    slot != NULL ? livetv_slot_display_title(slot) : "No Programming");

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

    livetv_commit_rect(0, y, LCD_WIDTH, LIVETV_INFO_H);
}

void livetv_draw_mini_guide(int chan)
{
    livetv_tv(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
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
                    now_slot != NULL ? livetv_slot_display_title(now_slot) : "--");

    livetv_text_fit(72 + half, y + text_h + 6, half - 8, LIVETV_TEXT,
                    LIVETV_ROW_BG,
                    next_slot != NULL ? livetv_slot_display_title(next_slot) : "--");

    livetv_commit_rect(0, y, LCD_WIDTH, LIVETV_MINI_H);
}

bool livetv_desktop_prepare(void)
{
    if (livetv_dm_title_tried)
        return livetv_dm_title_valid;

    livetv_dm_title_tried = true;
    livetv_dm_title.data = (char *)livetv_dm_title_data;
    if (rb->read_bmp_file(LIVETV_DM_TITLE_PATH, &livetv_dm_title,
                          sizeof(livetv_dm_title_data), FORMAT_NATIVE,
                          NULL) > 0)
        livetv_dm_title_valid = true;
    /* The sidebar uses the guide's existing brand bitmap. Prime it here too,
     * so every playback-time repaint is cached pixels and schedule state. */
    livetv_brand_logo();
    return livetv_dm_title_valid;
}

void livetv_desktop_draw_window(void)
{
    livetv_tv(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
    const struct livetv_channel *ch =
        livetv_channel(livetv_current_channel());
#if LCD_WIDTH < 1920
    const struct livetv_slot *now_slot;
    const struct livetv_slot *next_slot;
    struct bitmap *brand;
#endif
    unsigned oldfg = rb->lcd_get_foreground();
    unsigned oldbg = rb->lcd_get_background();
    int body_y = LIVETV_DM_WIN_Y + LIVETV_DM_TITLE_H;
    int body_bottom = LIVETV_DM_WIN_Y + LIVETV_DM_WIN_H;
    int video_right = LIVETV_DM_VIDEO_BOX_X + LIVETV_DM_VIDEO_BOX_W;
    int video_bottom = LIVETV_DM_VIDEO_BOX_Y + LIVETV_DM_VIDEO_BOX_H;
    int text_h;
    int width;
#if LCD_WIDTH < 1920
    int y;
    char buf[96];
#endif

    if (ch == NULL)
        return;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_UI);
    text_h = livetv_font_height();

    if (livetv_dm_title_valid)
        rb->lcd_bitmap((const fb_data *)livetv_dm_title.data,
                       LIVETV_DM_WIN_X, LIVETV_DM_WIN_Y,
                       livetv_dm_title.width, livetv_dm_title.height);
    else
    {
        rb->lcd_set_foreground(LIVETV_BANNER_BOT);
        rb->lcd_fillrect(LIVETV_DM_WIN_X, LIVETV_DM_WIN_Y,
                         LIVETV_DM_WIN_W, LIVETV_DM_TITLE_H);
    }

    rb->lcd_getstringsize("DIRECTV", &width, NULL);
    rb->lcd_set_foreground(LCD_RGBPACK(60, 60, 60));
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy(
        LIVETV_DM_WIN_X + (LIVETV_DM_WIN_W - width) / 2,
        LIVETV_DM_WIN_Y + (LIVETV_DM_TITLE_H - text_h) / 2, "DIRECTV");
    rb->lcd_set_drawmode(DRMODE_SOLID);

    /* Repaint only around the decoder-owned rectangle. The desktop behind
     * this window is the framebuffer Desktop Mode committed before handoff. */
    rb->lcd_set_foreground(LIVETV_HDR_BG);
    if (LIVETV_DM_VIDEO_BOX_Y > body_y)
        rb->lcd_fillrect(LIVETV_DM_WIN_X, body_y, LIVETV_DM_WIN_W,
                         LIVETV_DM_VIDEO_BOX_Y - body_y);
    rb->lcd_fillrect(LIVETV_DM_WIN_X, LIVETV_DM_VIDEO_BOX_Y,
                     LIVETV_DM_VIDEO_BOX_X - LIVETV_DM_WIN_X,
                     LIVETV_DM_VIDEO_BOX_H);
    rb->lcd_fillrect(video_right, LIVETV_DM_VIDEO_BOX_Y,
                     LIVETV_DM_WIN_X + LIVETV_DM_WIN_W - video_right,
                     LIVETV_DM_VIDEO_BOX_H);
    if (video_bottom < body_bottom)
        rb->lcd_fillrect(LIVETV_DM_WIN_X, video_bottom, LIVETV_DM_WIN_W,
                         body_bottom - video_bottom);

    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_drawrect(LIVETV_DM_VIDEO_BOX_X, LIVETV_DM_VIDEO_BOX_Y,
                     LIVETV_DM_VIDEO_BOX_W, LIVETV_DM_VIDEO_BOX_H);

#if LCD_WIDTH < 1920
    now_slot = livetv_slot_at(livetv_current_channel(), 0, NULL);
    next_slot = livetv_slot_next(livetv_current_channel(), 0);
    brand = livetv_brand_logo();
    y = body_y + 8;
    if (brand != NULL && brand->width <= LIVETV_DM_SIDEBAR_W - 4)
    {
        rb->lcd_bitmap(
            (const fb_data *)brand->data,
            LIVETV_DM_SIDEBAR_X +
                (LIVETV_DM_SIDEBAR_W - brand->width) / 2,
            y, brand->width, brand->height);
        y += brand->height + 5;
    }

    rb->snprintf(buf, sizeof(buf), "%d  %s", ch->number, ch->callsign);
    livetv_text_fit(LIVETV_DM_SIDEBAR_X + 3, y,
                    LIVETV_DM_SIDEBAR_W - 6, LIVETV_SEL_BG,
                    LIVETV_HDR_BG, buf);
    y += text_h + 5;
    livetv_text_at(LIVETV_DM_SIDEBAR_X + 3, y, LIVETV_DIM_TEXT,
                   LIVETV_HDR_BG, "NOW");
    y += text_h + 1;
    livetv_text_fit(
        LIVETV_DM_SIDEBAR_X + 3, y, LIVETV_DM_SIDEBAR_W - 6,
        LIVETV_TEXT, LIVETV_HDR_BG,
        now_slot != NULL ? livetv_slot_display_title(now_slot) :
                           "No programming");
    y += text_h + 6;
    if (y + text_h * 2 < body_bottom)
    {
        livetv_text_at(LIVETV_DM_SIDEBAR_X + 3, y, LIVETV_DIM_TEXT,
                       LIVETV_HDR_BG, "NEXT");
        y += text_h + 1;
        livetv_text_fit(
            LIVETV_DM_SIDEBAR_X + 3, y, LIVETV_DM_SIDEBAR_W - 6,
            LIVETV_TEXT, LIVETV_HDR_BG,
            next_slot != NULL ? livetv_slot_display_title(next_slot) : "--");
    }
#endif

    livetv_commit_rect(LIVETV_DM_WIN_X, LIVETV_DM_WIN_Y,
                        LIVETV_DM_WIN_W, LIVETV_DM_TITLE_H);
    if (LIVETV_DM_VIDEO_BOX_Y > body_y)
        livetv_commit_rect(LIVETV_DM_WIN_X, body_y, LIVETV_DM_WIN_W,
                            LIVETV_DM_VIDEO_BOX_Y - body_y);
    livetv_commit_rect(LIVETV_DM_WIN_X, LIVETV_DM_VIDEO_BOX_Y,
                        LIVETV_DM_VIDEO_BOX_X - LIVETV_DM_WIN_X,
                        LIVETV_DM_VIDEO_BOX_H);
    livetv_commit_rect(video_right, LIVETV_DM_VIDEO_BOX_Y,
                        LIVETV_DM_WIN_X + LIVETV_DM_WIN_W - video_right,
                        LIVETV_DM_VIDEO_BOX_H);
    if (video_bottom < body_bottom)
        livetv_commit_rect(LIVETV_DM_WIN_X, video_bottom,
                            LIVETV_DM_WIN_W, body_bottom - video_bottom);

    rb->lcd_set_foreground(oldfg);
    rb->lcd_set_background(oldbg);
}

void livetv_clear_overlay(void)
{
    /* The video thread repaints the whole frame, so simply asking for a
     * redraw is enough; the caller does that after clearing. */
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(0, LCD_HEIGHT - LIVETV_INFO_H, LCD_WIDTH,
                     LIVETV_INFO_H);
    livetv_commit_rect(0, LCD_HEIGHT - LIVETV_INFO_H, LCD_WIDTH,
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
#define LIVETV_BTN_EXIT_REL (BUTTON_MENU | BUTTON_REL)
#define LIVETV_BTN_UNLOCK   (BUTTON_SELECT | BUTTON_REPEAT)
#define LIVETV_BTN_UNLOCK_REL \
    (BUTTON_SELECT | BUTTON_REPEAT | BUTTON_REL)
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

/* mpegplayer reads raw plugin buttons, so the normal action/keymap layer does
 * not translate 30-pin iAP remote events for this modal guide.  Normalize the
 * standard remote buttons here and retain the clickwheel mapping above.  A
 * remote Select/Play completes on release, like the local center button.
 * Consuming the whole gesture here prevents its trailing release from being
 * interpreted as a fresh Open Guide command after fullscreen takes over. */
static int livetv_remote_button(int button)
{
#ifdef BUTTON_RC_PLAY
    int base = button & ~(BUTTON_REPEAT | BUTTON_REL);
    bool repeat = (button & BUTTON_REPEAT) != 0;
    bool release = (button & BUTTON_REL) != 0;

    switch (base)
    {
    case BUTTON_RC_UP:
#ifdef BUTTON_RC_VOL_UP
    case BUTTON_RC_VOL_UP:
#endif
        if (release)
            return BUTTON_NONE;
        return LIVETV_BTN_UP | (repeat ? BUTTON_REPEAT : 0);
    case BUTTON_RC_DOWN:
#ifdef BUTTON_RC_VOL_DOWN
    case BUTTON_RC_VOL_DOWN:
#endif
        if (release)
            return BUTTON_NONE;
        return LIVETV_BTN_DOWN | (repeat ? BUTTON_REPEAT : 0);
    case BUTTON_RC_LEFT:
    {
        static bool held;
        if (!repeat && !release) { held = false; return BUTTON_NONE; }
        if (repeat)
        {
            if (held) return BUTTON_NONE;
            held = true;
            return LIVETV_BTN_EXIT;
        }
        return held ? BUTTON_NONE : LIVETV_BTN_LEFT;
    }
    case BUTTON_RC_RIGHT:
        if (release)
            return BUTTON_NONE;
        return LIVETV_BTN_RIGHT;
    case BUTTON_RC_PLAY:
    case BUTTON_RC_SELECT:
        return (release && !repeat) ? LIVETV_BTN_SELECT : BUTTON_NONE;
    case BUTTON_RC_MENU:
    case BUTTON_RC_STOP:
#ifdef LIVETV_BTN_EXIT_REL
        return release ? LIVETV_BTN_EXIT_REL : LIVETV_BTN_EXIT;
#else
        return release ? BUTTON_NONE : LIVETV_BTN_EXIT;
#endif
    default:
        break;
    }
#else
    (void)button;
#endif
    return button;
}

static void livetv_parental_draw_pin(const char *pin, int digit)
{
    livetv_tv(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
    char masked[5];
    char glyph[2];
    int length = rb->strlen(pin);
    int title_w;
    int text_h;
    int panel_y = LCD_HEIGHT - 69;

    rb->lcd_set_viewport(NULL);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(
        rb->global_settings->ui_engine_dark_mode ?
        LCD_RGBPACK(18, 20, 24) : LCD_WHITE);
    rb->lcd_set_foreground(
        rb->global_settings->ui_engine_dark_mode ?
        LCD_RGBPACK(239, 242, 246) : LCD_BLACK);
    rb->lcd_clear_display();
    rb->lcd_setfont(FONT_UI);
    rb->lcd_getstringsize("Unlock Settings", &title_w, &text_h);
    rb->lcd_putsxy(MAX(4, (LCD_WIDTH - title_w) / 2), 38,
                   "Unlock Settings");
    rb->lcd_putsxy(42, 78, "Scroll to choose, Select to enter");

    rb->lcd_set_foreground(LCD_RGBPACK(31, 46, 65));
    rb->lcd_fillrect(20, panel_y, LCD_WIDTH - 42, 50);
    rb->lcd_set_foreground(LCD_RGBPACK(207, 220, 231));
    rb->lcd_drawrect(20, panel_y, LCD_WIDTH - 42, 50);
    rb->lcd_set_foreground(LCD_RGBPACK(239, 244, 246));
    /* Text over a painted surface must be foreground-only. In SOLID mode
     * Rockbox fills each glyph cell with the viewport background, which
     * appears as a row of white boxes on the dark PIN panel. */
    rb->lcd_set_drawmode(DRMODE_FG);

    for (int i = 0; i < length && i < 4; i++)
        masked[i] = '*';
    masked[MIN(length, 4)] = '\0';
    rb->lcd_putsxy(31, panel_y + 15, masked);

    for (int i = -5; i <= 6; i++)
    {
        int value = (digit + i + 20) % 10;
        int x = LCD_WIDTH / 2 - 9 + i * 19;

        if (x < 104)
            continue;
        glyph[0] = (char)('0' + value);
        glyph[1] = '\0';
        if (i == 0)
        {
            rb->lcd_set_foreground(LCD_RGBPACK(0, 92, 192));
            rb->lcd_fillrect(x - 3, panel_y + 9, 16, text_h + 5);
            rb->lcd_set_foreground(LCD_WHITE);
        }
        else
            rb->lcd_set_foreground(LCD_RGBPACK(239, 244, 246));
        rb->lcd_putsxy(x, panel_y + 11, glyph);
    }
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_update();
}

static bool livetv_parental_prompt_pin(char *pin, size_t size)
{
    int digit = 0;

    if (size < 5)
        return false;
    pin[0] = '\0';
    rb->button_clear_queue();
    mpegplayer_livetv_pin_active = true;

    while (true)
    {
        int button;
        size_t length;

        livetv_parental_draw_pin(pin, digit);
        button = livetv_remote_button(mpeg_button_get(TIMEOUT_BLOCK));
        if (mpeg_sysevent() != 0)
        {
            mpegplayer_livetv_pin_active = false;
            return false;
        }

        switch (button)
        {
        case LIVETV_BTN_UP:
        case LIVETV_BTN_UP | BUTTON_REPEAT:
            digit = digit <= 0 ? 9 : digit - 1;
            break;
        case LIVETV_BTN_DOWN:
        case LIVETV_BTN_DOWN | BUTTON_REPEAT:
            digit = (digit + 1) % 10;
            break;
        case LIVETV_BTN_LEFT:
        case LIVETV_BTN_LEFT | BUTTON_REL:
            length = rb->strlen(pin);
            if (length > 0)
                pin[length - 1] = '\0';
            break;
        case LIVETV_BTN_SELECT:
            length = rb->strlen(pin);
            if (length < 4)
            {
                pin[length] = (char)('0' + digit);
                pin[length + 1] = '\0';
            }
            if (length + 1 == 4)
            {
                mpegplayer_livetv_pin_active = false;
                return true;
            }
            break;
        case LIVETV_BTN_EXIT:
#ifdef LIVETV_BTN_EXIT_REL
        case LIVETV_BTN_EXIT_REL:
#endif
            mpegplayer_livetv_pin_active = false;
            return false;
        default:
            break;
        }
    }
}

static bool livetv_parental_unlock(void)
{
    char expected[16];
    char entered[16];
    int fd = rb->open(LIVETV_PARENTAL_PIN, O_RDONLY);
    int length;

    if (fd < 0 || rb->read_line(fd, expected, sizeof(expected)) <= 0)
    {
        if (fd >= 0)
            rb->close(fd);
        rb->splash(HZ * 2, "Settings lock unavailable");
        return false;
    }
    rb->close(fd);

    length = rb->strlen(expected);
    while (length > 0 &&
           (expected[length - 1] == '\r' || expected[length - 1] == '\n' ||
            expected[length - 1] == ' ' || expected[length - 1] == '\t'))
        expected[--length] = '\0';
    if (length != 4)
    {
        rb->splash(HZ * 2, "Settings lock PIN invalid");
        return false;
    }
    for (int i = 0; i < 4; i++)
        if (expected[i] < '0' || expected[i] > '9')
        {
            rb->splash(HZ * 2, "Settings lock PIN invalid");
            return false;
        }

    if (!livetv_parental_prompt_pin(entered, sizeof(entered)))
        return false;
    if (rb->strcmp(entered, expected) != 0)
    {
        rb->splash(HZ * 2, "Wrong code");
        return false;
    }
    return true;
}

static void livetv_parental_finish_unlock(void)
{
    if (!livetv_parental_unlocked && livetv_parental_unlock())
    {
        livetv_parental_unlocked = true;
        livetv_rebuild_view();
        livetv_guide_reset_to_now();
    }
    rb->button_clear_queue();
    livetv_guide_draw();
}

static int livetv_options_run(void)
{
    int selected = 0;
    bool done = false;
    bool repaint = false;

    rb->button_clear_queue();
    livetv_options_draw(selected);

    while (!done)
    {
        int button = livetv_remote_button(mpeg_button_get(HZ / 4));

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

#ifdef HAVE_IPODJS_UI
static uint32_t livetv_reminder_id(const struct livetv_slot *slot, int chan)
{
    uint32_t hash = 2166136261u;
    const unsigned char *text = (const unsigned char *)
        livetv_slot_display_title(slot);

    hash = (hash ^ (uint32_t)chan) * 16777619u;
    hash = (hash ^ slot->day) * 16777619u;
    hash = (hash ^ slot->block_start) * 16777619u;
    while (*text)
        hash = (hash ^ *text++) * 16777619u;
    return hash ? hash : 1;
}

static long livetv_reminder_start_delta(const struct livetv_slot *slot,
                                        uint32_t offset)
{
    return guide.cursor - (long)offset -
        ((long)slot->start - (long)slot->block_start);
}

static void livetv_reminder_draw(const struct livetv_slot *slot, int chan,
                                 int selected)
{
    livetv_tv(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
    static const char * const labels[] = {
        "Set Reminder", "Cancel Reminder", "Back to Guide"
    };
    const struct livetv_channel *channel = livetv_channel(chan);
    int text_h = livetv_font_height();
    int row_h = text_h + 7;
    int w = 238;
    int h = 58 + row_h * (int)ARRAYLEN(labels);
    int x = (LIVETV_GUIDE_W - w) / 2;
    int y = MAX(2, (LIVETV_GUIDE_H - h) / 2);
    char clock[16];
    char details[64];

    livetv_fill(x, y, w, 22, LIVETV_SEL_BG);
    livetv_text_at(x + 7, y + 4, LIVETV_SEL_TEXT, LIVETV_SEL_BG,
                   "Upcoming Program");
    livetv_fill(x, y + 22, w, h - 22, LIVETV_DESC_BG);
    livetv_text_fit(x + 7, y + 27, w - 14, LIVETV_TEXT, LIVETV_DESC_BG,
                    livetv_slot_display_title(slot));
    livetv_format_clock(clock, sizeof(clock), slot->block_start);
    rb->snprintf(details, sizeof(details), "%s  Ch %d %s", clock,
                 channel ? channel->number : 0,
                 channel ? channel->callsign : "");
    livetv_text_fit(x + 7, y + 29 + text_h, w - 14, LIVETV_DIM_TEXT,
                    LIVETV_DESC_BG, details);

    for (int i = 0; i < (int)ARRAYLEN(labels); i++)
    {
        int row_y = y + 54 + i * row_h;
        unsigned bg = LIVETV_DESC_BG;
        unsigned fg = LIVETV_TEXT;

        if (i == selected)
        {
            bg = LIVETV_SEL_BG;
            fg = LIVETV_SEL_TEXT;
            livetv_fill(x + 3, row_y, w - 6, row_h, bg);
        }
        livetv_text_fit(x + 10, row_y + 3, w - 20, fg, bg, labels[i]);
    }
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_drawrect(x, y, w, h);
    livetv_update(x, y, w, h);
}

static bool livetv_set_reminder(const struct livetv_slot *slot, int chan,
                                uint32_t offset)
{
    const struct livetv_channel *channel = livetv_channel(chan);
    struct notification_request request;
    long now = (long)rb->mktime(rb->get_time());
    long start = now + livetv_reminder_start_delta(slot, offset);
    long notify_at = start - 5 * 60;
    char clock[16];

    if (now <= 0 || start <= now)
        return false;
    if (notify_at <= now)
        notify_at = now + 1;
    rb->memset(&request, 0, sizeof(request));
    request.source = NOTIFICATION_SOURCE_LIVETV;
    request.kind = NOTIFICATION_LIVETV_SHOW_REMINDER;
    request.stable_id = livetv_reminder_id(slot, chan);
    livetv_format_clock(clock, sizeof(clock), slot->block_start);
    rb->strlcpy(request.title, livetv_slot_display_title(slot),
                sizeof(request.title));
    rb->snprintf(request.body, sizeof(request.body),
                 "Starts at %s on %d %s", clock,
                 channel ? channel->number : 0,
                 channel ? channel->callsign : "");
    rb->strlcpy(request.route, "guide", sizeof(request.route));
    return rb->notification_schedule(&request, notify_at);
}

static void livetv_reminder_run(void)
{
    int chan = livetv_guide_selected_channel();
    const struct livetv_slot *slot;
    uint32_t offset = 0;
    int selected = 0;
    bool done = false;

    slot = livetv_slot_at(chan, guide.cursor, &offset);
    if (!slot)
        return;
    rb->button_clear_queue();
    livetv_reminder_draw(slot, chan, selected);
    while (!done)
    {
        int button = livetv_remote_button(mpeg_button_get(HZ / 4));

        if (mpeg_sysevent() != 0)
            return;
        switch (button)
        {
        case LIVETV_BTN_UP:
        case LIVETV_BTN_UP | BUTTON_REPEAT:
            selected = (selected + 2) % 3;
            livetv_reminder_draw(slot, chan, selected);
            break;
        case LIVETV_BTN_DOWN:
        case LIVETV_BTN_DOWN | BUTTON_REPEAT:
            selected = (selected + 1) % 3;
            livetv_reminder_draw(slot, chan, selected);
            break;
        case LIVETV_BTN_SELECT:
            if (selected == 0)
            {
                if (livetv_set_reminder(slot, chan, offset))
                    rb->splash(HZ, "Reminder set");
                else
                    rb->splash(HZ * 2,
                               "Enable Live TV in Notification Settings");
            }
            else if (selected == 1)
            {
                rb->notification_cancel(NOTIFICATION_SOURCE_LIVETV,
                    NOTIFICATION_LIVETV_SHOW_REMINDER,
                    livetv_reminder_id(slot, chan));
                rb->splash(HZ, "Reminder cancelled");
            }
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
    rb->button_clear_queue();
}
#endif

int livetv_guide_run(void)
{
    int result = LIVETV_GUIDE_EXIT;
    long next_tick = *rb->current_tick + HZ;
    bool done = false;
#ifdef LIVETV_BTN_UNLOCK
    bool menu_pending = false;
    bool unlock_pending = false;
#endif

    if (!livetv_ready)
        return LIVETV_GUIDE_EXIT;

    livetv_guide_enter();
    rb->button_clear_queue();
    livetv_guide_draw();

    while (!done)
    {
        int raw_button = mpeg_button_get(HZ / 4);
        int button = livetv_remote_button(raw_button);

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
            char path[MAX_PATH];
            if (unlock_pending)
            {
                unlock_pending = false;
                livetv_parental_finish_unlock();
                break;
            }
            int chan = livetv_guide_selected_channel();

            /* Selecting a programme that is not on yet snaps the guide to
             * its DIRECTV-style reminder sheet rather than tuning. */
            if (!livetv_guide_selection_is_live())
            {
#ifdef HAVE_IPODJS_UI
                livetv_reminder_run();
#else
                livetv_guide_reset_to_now();
#endif
                livetv_guide_draw();
                break;
            }

            result = (chan == livetv_channel_cur) ? LIVETV_GUIDE_WATCH
                                                  : LIVETV_GUIDE_TUNE;
            if (!livetv_tune(chan, path, sizeof(path), NULL))
            {
                rb->splash(HZ, "No playable programming");
                livetv_guide_draw();
                break;
            }
            livetv_save_state();
            done = true;
            break;
        }

#ifdef LIVETV_BTN_UNLOCK
        case LIVETV_BTN_EXIT:
#ifdef BUTTON_RC_LEFT
            /* Held remote Left is a complete Back gesture. Its release is
             * consumed by the adapter, so it cannot use Menu's release gate. */
            if (raw_button == (BUTTON_RC_LEFT | BUTTON_REPEAT))
            {
                result = LIVETV_GUIDE_EXIT;
                done = true;
                break;
            }
#endif
            menu_pending = true;
            break;

        case LIVETV_BTN_UNLOCK:
            unlock_pending = true;
            break;

        case LIVETV_BTN_UNLOCK_REL:
            if (!unlock_pending)
                break;
            unlock_pending = false;
            livetv_parental_finish_unlock();
            break;

        case LIVETV_BTN_EXIT_REL:
            if (!menu_pending)
                break;
            menu_pending = false;
            result = LIVETV_GUIDE_EXIT;
            done = true;
            break;
#else
        case LIVETV_BTN_EXIT:
            result = LIVETV_GUIDE_EXIT;
            done = true;
            break;
#endif

        default:
            break;
        }
    }

    return result;
}

/* Weather channel ----------------------------------------------------
 *
 * Not a new slot kind: the Weather channel is an ordinary channel whose
 * shows are real MPEG files (a timed panel/presenter/video carrier carrying
 * the user's own looped music), so livetv_tune()/stream_open()/the audio
 * lifecycle above are all reused unmodified.
 *
 * Panel phases suppress decoded framebuffer blits while these native panels
 * use the whole screen; presenter and report phases expose the carrier
 * full-screen. The guide's picture-in-guide remains untouched.
 * See docs/livetv-weather-channel-spec.md.
 */

#define LIVETV_WX_FORECAST_PATH ROCKBOX_DIR "/rockpod/weather/forecast.tsv"

#define LIVETV_WX_MAX_DAYS      7
/* Today and tomorrow, matching the "only today and tomorrow" discipline
 * the guide's own schedule already follows. */
#define LIVETV_WX_MAX_HOURS     48

#define LIVETV_WX_PANEL_COUNT   4
#define LIVETV_WX_CLOCK_SECS    64
#define LIVETV_WX_PHASE_SECS    8

/* No video box to dodge while watching (see above), so panels use the
 * full screen width. */
#define LIVETV_WX_CONTENT_W     LCD_WIDTH
#define LIVETV_WX_HDR_H         22
#define LIVETV_WX_TICKER_H      16
#define LIVETV_WX_BODY_Y        LIVETV_WX_HDR_H
#define LIVETV_WX_BODY_H        (LCD_HEIGHT - LIVETV_WX_HDR_H - \
                                 LIVETV_WX_TICKER_H)
#define LIVETV_WX_TICKER_Y      (LCD_HEIGHT - LIVETV_WX_TICKER_H)
#define LIVETV_WX_AD_OVERLAY_H  LIVETV_WEATHER_COMMERCIAL_OVERLAY_H
#define LIVETV_WX_AD_OVERLAY_Y  (LCD_HEIGHT - LIVETV_WX_AD_OVERLAY_H)
#define LIVETV_WX_SCENE_Y       LIVETV_WX_BODY_Y
#define LIVETV_WX_SCENE_H       70
#define LIVETV_WX_CARD_Y        (LIVETV_WX_SCENE_Y + LIVETV_WX_SCENE_H)
#define LIVETV_WX_CARD_H        (LIVETV_WX_TICKER_Y - LIVETV_WX_CARD_Y)
#define LIVETV_WX_ANIM_RATE     MAX(1, HZ / 5)

/* Deliberately narrow: only what the four panels below print, not a copy
 * of weather.c's much larger state (see docs/livetv-weather-channel-spec.md
 * section 5.2 on the BSS budget this keeps clear of). */
struct livetv_wx_day
{
    char date[11];
    char code[16];
    char text[24];
    char temp_min[6];
    char temp_max[6];
    char precip[5];
    char wind_speed[6];
    char wind_dir[4];
    char sunrise[6];
    char sunset[6];
};

struct livetv_wx_hour
{
    char stamp[17];
    char code[16];
    char text[24];
    char temp[6];
    char precip[5];
    char is_day[2];
};

struct livetv_wx_state
{
    char location[40];
    char generated[24];
    char units[12];
    struct livetv_wx_day days[LIVETV_WX_MAX_DAYS];
    struct livetv_wx_hour hours[LIVETV_WX_MAX_HOURS];
    int day_count;
    int hour_count;
    bool loaded;
};

static struct livetv_wx_state wx;
static int wx_panel;
static long wx_anim_epoch;
static long wx_anim_next_tick;
static uint32_t wx_clock_base;
static long wx_clock_epoch;

enum livetv_wx_scene
{
    LIVETV_WX_SCENE_CLEAR,
    LIVETV_WX_SCENE_PARTLY,
    LIVETV_WX_SCENE_CLOUDY,
    LIVETV_WX_SCENE_RAIN,
    LIVETV_WX_SCENE_THUNDER,
    LIVETV_WX_SCENE_SNOW,
    LIVETV_WX_SCENE_FOG
};

static char *wx_field(char **cursor)
{
    char *start = *cursor;
    char *tab;

    if (!start)
        return "";

    tab = rb->strchr(start, '\t');
    if (tab)
    {
        *tab = '\0';
        *cursor = tab + 1;
    }
    else
        *cursor = NULL;

    return start;
}

static void wx_copy(char *dst, size_t size, char **cursor)
{
    rb->strlcpy(dst, wx_field(cursor), size);
}

/* Read forecast.tsv - the same file the standalone weather.rock reads - and
 * keep only what the panels below need. Called once when tuning into the
 * channel, not per frame or per panel. */
static bool livetv_wx_load(void)
{
    int fd;
    char line[400];
    char *cursor;

    rb->memset(&wx, 0, sizeof(wx));
    rb->strcpy(wx.units, "metric");

    fd = rb->open(LIVETV_WX_FORECAST_PATH, O_RDONLY);
    if (fd < 0)
        return false;

    if (rb->read_line(fd, line, sizeof(line)) <= 0)
    {
        rb->close(fd);
        return false;
    }
    cursor = line;
    if (rb->strcmp(wx_field(&cursor), "rockpod_weather_v1"))
    {
        rb->close(fd);
        return false;
    }
    wx_copy(wx.location, sizeof(wx.location), &cursor);
    wx_field(&cursor); /* latitude */
    wx_field(&cursor); /* longitude */
    wx_field(&cursor); /* timezone */
    wx_copy(wx.generated, sizeof(wx.generated), &cursor);
    wx_field(&cursor); /* valid_from_local */
    wx_copy(wx.units, sizeof(wx.units), &cursor);

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char first[16];

        if (!line[0])
            continue;
        cursor = line;
        rb->strlcpy(first, wx_field(&cursor), sizeof(first));

        if (!rb->strcmp(first, "hourly"))
        {
            struct livetv_wx_hour *hour;

            if (wx.hour_count >= LIVETV_WX_MAX_HOURS)
                continue;
            hour = &wx.hours[wx.hour_count];
            wx_copy(hour->stamp, sizeof(hour->stamp), &cursor);
            wx_copy(hour->code, sizeof(hour->code), &cursor);
            wx_copy(hour->text, sizeof(hour->text), &cursor);
            wx_copy(hour->temp, sizeof(hour->temp), &cursor);
            wx_copy(hour->precip, sizeof(hour->precip), &cursor);
            wx_field(&cursor); /* wind_speed */
            wx_field(&cursor); /* wind_direction */
            wx_copy(hour->is_day, sizeof(hour->is_day), &cursor);
            wx.hour_count++;
            continue;
        }

        if (wx.day_count >= LIVETV_WX_MAX_DAYS)
            continue;

        struct livetv_wx_day *day = &wx.days[wx.day_count];
        rb->strlcpy(day->date, first, sizeof(day->date));
        wx_copy(day->code, sizeof(day->code), &cursor);
        wx_copy(day->text, sizeof(day->text), &cursor);
        wx_copy(day->temp_min, sizeof(day->temp_min), &cursor);
        wx_copy(day->temp_max, sizeof(day->temp_max), &cursor);
        wx_copy(day->precip, sizeof(day->precip), &cursor);
        wx_copy(day->wind_speed, sizeof(day->wind_speed), &cursor);
        wx_copy(day->wind_dir, sizeof(day->wind_dir), &cursor);
        wx_copy(day->sunrise, sizeof(day->sunrise), &cursor);
        wx_copy(day->sunset, sizeof(day->sunset), &cursor);
        wx.day_count++;
    }

    rb->close(fd);
    wx.loaded = wx.day_count > 0;
    return wx.loaded;
}

/* Ported from weather.c's weather_status(): never claim a stale or expired
 * forecast is current (docs/rockpod-weather-app-spec.md's "never hide
 * stale data" release blocker applies here exactly as it does there). */
static const char *livetv_wx_status(void)
{
    struct tm *tm = rb->get_time();

    if (!tm || !wx.generated[0] || wx.day_count == 0)
        return "Synced forecast";

    const char *last = wx.days[wx.day_count - 1].date;
    if (rb->strlen(last) >= 10)
    {
        int today = (tm->tm_year + 1900) * 10000 +
                    (tm->tm_mon + 1) * 100 + tm->tm_mday;
        int last_val = rb->atoi(last) * 10000 +
                       rb->atoi(last + 5) * 100 + rb->atoi(last + 8);

        if (last_val < today)
            return "Expired forecast";
    }

    return "Updated by RockPod";
}

static const char *wx_unit_suffix(void)
{
    return !rb->strcmp(wx.units, "imperial") ? "F" : "C";
}

/* The same dimensional CGI icon set the standalone weather.rock plugin uses
 * (tools/generate_weather_icons.py, committed under
 * rockpod/assets/weather/icons/ from one consistent broadcast atlas),
 * read from the same device path weather.c already reads
 * (WEATHER_ICON_DIR in weather.c); ensure_weather_channel() on the PC side
 * deploys them there. Loading one bitmap on demand, single-slot cache, is
 * the same pattern livetv_channel_logo() above already uses for channel
 * logos. */
#define LIVETV_WX_ICON_DIR ROCKBOX_DIR "/rockpod/weather/icons"
#define LIVETV_WX_ICON_SIZE_SMALL 40
#define LIVETV_WX_ICON_SIZE_LARGE 64

static struct bitmap wx_icon_bmp;
static fb_data wx_icon_data[LIVETV_WX_ICON_SIZE_LARGE * LIVETV_WX_ICON_SIZE_LARGE];
static char wx_icon_loaded_path[MAX_PATH];
static bool wx_icon_loaded;

static const char *wx_icon_name(const char *code, bool night)
{
    if (rb->strstr(code, "clear"))
        return night ? "clear_night" : "clear_day";
    if (rb->strstr(code, "partly_cloudy"))
        return "partly_cloudy";
    if (rb->strstr(code, "cloudy"))
        return "cloudy";
    if (rb->strstr(code, "drizzle"))
        return "drizzle";
    if (rb->strstr(code, "rain"))
        return "rain";
    if (rb->strstr(code, "snow"))
        return "snow";
    if (rb->strstr(code, "fog"))
        return "fog";
    if (rb->strstr(code, "thunder"))
        return "thunderstorm";
    return "unknown";
}

/* Ported from weather.c's weather_clean_icon_transparency(): the generator
 * pads icons with magenta as a colour key, which this turns into
 * TRANSPARENT_COLOR so the panel background shows through around the
 * shape instead of a magenta box. */
static void wx_icon_clean_transparency(struct bitmap *bm)
{
    fb_data *pixels;
    int count;
    int i;

    if (!bm || !bm->data)
        return;

    pixels = (fb_data *)bm->data;
    count = bm->width * bm->height;
    for (i = 0; i < count; i++)
    {
        unsigned px = pixels[i];
        int r = RGB_UNPACK_RED(px);
        int g = RGB_UNPACK_GREEN(px);
        int b = RGB_UNPACK_BLUE(px);

        if (r >= 150 && b >= 170 && g + 28 < r && g + 28 < b)
            pixels[i] = TRANSPARENT_COLOR;
    }
}

static struct bitmap *wx_load_icon(const char *code, bool night, int size)
{
    char path[MAX_PATH];
    int px = size >= 56 ? LIVETV_WX_ICON_SIZE_LARGE : LIVETV_WX_ICON_SIZE_SMALL;

    rb->snprintf(path, sizeof(path), "%s/%s.%dx%dx24.bmp",
                LIVETV_WX_ICON_DIR, wx_icon_name(code, night), px, px);

    if (wx_icon_loaded && !rb->strcmp(wx_icon_loaded_path, path))
        return &wx_icon_bmp;

    wx_icon_loaded = false;
    wx_icon_loaded_path[0] = '\0';
    if (!rb->file_exists(path))
        return NULL;

    rb->memset(&wx_icon_bmp, 0, sizeof(wx_icon_bmp));
    wx_icon_bmp.width = px;
    wx_icon_bmp.height = px;
    wx_icon_bmp.format = FORMAT_NATIVE;
    wx_icon_bmp.data = (char *)wx_icon_data;
    if (rb->read_bmp_file(path, &wx_icon_bmp, sizeof(wx_icon_data),
                          FORMAT_NATIVE | FORMAT_TRANSPARENT | FORMAT_DITHER,
                          NULL) <= 0)
        return NULL;

    wx_icon_clean_transparency(&wx_icon_bmp);
    rb->strlcpy(wx_icon_loaded_path, path, sizeof(wx_icon_loaded_path));
    wx_icon_loaded = true;
    return &wx_icon_bmp;
}

/* Falls back to a plain soft circle - never the icon bitmap's shape - only
 * when the icon pack has not reached the device yet, so a fresh install
 * still shows something better than a blank hole rather than nothing. */
static void wx_icon(int cx, int cy, int r, const char *code, bool night)
{
    struct bitmap *icon = wx_load_icon(code, night, r);

    if (icon != NULL)
    {
        /* Not lcd_bitmap(): it draws every pixel opaque, including the
         * magenta colour key wx_icon_clean_transparency() just converted
         * to TRANSPARENT_COLOR, and would paint a solid magenta square. */
        rb->lcd_bitmap_transparent((const fb_data *)icon->data,
                                   cx - icon->width / 2,
                                   cy - icon->height / 2,
                                   icon->width, icon->height);
        return;
    }

    livetv_fill(cx - r / 2, cy - r / 2, r, r,
               night ? LCD_RGBPACK(237, 240, 210) : LCD_RGBPACK(235, 239, 244));
}

static void livetv_wx_header(void)
{
    int text_h = livetv_font_height();

    livetv_fill(0, 0, LIVETV_WX_CONTENT_W, LIVETV_WX_HDR_H, LIVETV_HDR_BG);
    livetv_text_at(4, (LIVETV_WX_HDR_H - text_h) / 2, LIVETV_TEXT,
                  LIVETV_HDR_BG, wx.location[0] ? wx.location : "Weather");
    livetv_update(0, 0, LIVETV_WX_CONTENT_W, LIVETV_WX_HDR_H);
}

static void livetv_wx_ticker(void)
{
    char buf[80];
    int text_h = livetv_font_height();

    /* Below the video box entirely (LIVETV_WX_TICKER_Y > box bottom), so
     * this is safe to draw full width. */
    livetv_fill(0, LIVETV_WX_TICKER_Y, LCD_WIDTH, LIVETV_WX_TICKER_H,
               LIVETV_HINT_BG);
    rb->snprintf(buf, sizeof(buf), "%s - %s",
                wx.location[0] ? wx.location : "Weather", livetv_wx_status());
    livetv_text_at(4, LIVETV_WX_TICKER_Y + (LIVETV_WX_TICKER_H - text_h) / 2,
                  LIVETV_TEXT, LIVETV_HINT_BG, buf);
    livetv_update(0, LIVETV_WX_TICKER_Y, LCD_WIDTH, LIVETV_WX_TICKER_H);
}

static const struct livetv_wx_hour *wx_hour_near(int target_hour)
{
    const struct livetv_wx_hour *best = NULL;
    int best_diff = 999;
    int i;

    if (wx.day_count == 0)
        return NULL;

    for (i = 0; i < wx.hour_count; i++)
    {
        const char *stamp = wx.hours[i].stamp;
        int hour;
        int diff;

        if (rb->strlen(stamp) < 13 ||
            rb->strncmp(stamp, wx.days[0].date, 10))
            continue;

        hour = rb->atoi(stamp + 11);
        diff = hour - target_hour;
        if (diff < 0)
            diff = -diff;
        if (diff < best_diff)
        {
            best_diff = diff;
            best = &wx.hours[i];
        }
    }

    return best;
}

static const struct livetv_wx_hour *wx_current_hour(void)
{
    return wx_hour_near((int)(livetv_now_secs() / 3600));
}

static const char *wx_current_code(void)
{
    const struct livetv_wx_hour *hour = wx_current_hour();

    if (hour != NULL && hour->code[0])
        return hour->code;
    if (wx.day_count > 0 && wx.days[0].code[0])
        return wx.days[0].code;
    return "cloudy";
}

static bool wx_current_is_day(void)
{
    const struct livetv_wx_hour *hour = wx_current_hour();
    int clock_hour = (int)(livetv_now_secs() / 3600);

    if (hour != NULL && hour->is_day[0])
        return hour->is_day[0] != '0';
    return clock_hour >= 6 && clock_hour < 19;
}

static enum livetv_wx_scene wx_scene_for_code(const char *code)
{
    if (rb->strstr(code, "thunder"))
        return LIVETV_WX_SCENE_THUNDER;
    if (rb->strstr(code, "snow") || rb->strstr(code, "sleet") ||
        rb->strstr(code, "ice"))
        return LIVETV_WX_SCENE_SNOW;
    if (rb->strstr(code, "fog") || rb->strstr(code, "mist"))
        return LIVETV_WX_SCENE_FOG;
    if (rb->strstr(code, "rain") || rb->strstr(code, "drizzle") ||
        rb->strstr(code, "shower"))
        return LIVETV_WX_SCENE_RAIN;
    if (rb->strstr(code, "partly") || rb->strstr(code, "mostly_clear"))
        return LIVETV_WX_SCENE_PARTLY;
    if (rb->strstr(code, "cloud") || rb->strstr(code, "overcast"))
        return LIVETV_WX_SCENE_CLOUDY;
    return LIVETV_WX_SCENE_CLEAR;
}

static void wx_scene_gradient(unsigned top, unsigned bottom)
{
    int r1 = RGB_UNPACK_RED(top);
    int g1 = RGB_UNPACK_GREEN(top);
    int b1 = RGB_UNPACK_BLUE(top);
    int r2 = RGB_UNPACK_RED(bottom);
    int g2 = RGB_UNPACK_GREEN(bottom);
    int b2 = RGB_UNPACK_BLUE(bottom);
    int y;

    for (y = 0; y < LIVETV_WX_SCENE_H; y++)
    {
        unsigned color =
            LCD_RGBPACK(r1 + (r2 - r1) * y / LIVETV_WX_SCENE_H,
                        g1 + (g2 - g1) * y / LIVETV_WX_SCENE_H,
                        b1 + (b2 - b1) * y / LIVETV_WX_SCENE_H);
        livetv_fill(0, LIVETV_WX_SCENE_Y + y,
                    LIVETV_WX_CONTENT_W, 1, color);
    }
}

static void wx_scene_draw(bool commit)
{
    const char *code = wx_current_code();
    const struct livetv_wx_hour *hour = wx_current_hour();
    enum livetv_wx_scene scene = wx_scene_for_code(code);
    bool day = wx_current_is_day();
    unsigned frame = (unsigned)((*rb->current_tick - wx_anim_epoch) /
                                LIVETV_WX_ANIM_RATE);
    int shine_x =
        (int)((frame * 4) % (LIVETV_WX_CONTENT_W + 72)) - 56;
    int ribbon_y = LIVETV_WX_SCENE_Y + 9;
    unsigned ribbon_bg = LCD_RGBPACK(12, 68, 126);
    char temperature[16];
    int i;

    if (!day)
    {
        wx_scene_gradient(LCD_RGBPACK(4, 16, 48),
                          LCD_RGBPACK(28, 66, 108));
    }
    else if (scene == LIVETV_WX_SCENE_THUNDER)
    {
        wx_scene_gradient(LCD_RGBPACK(25, 37, 61),
                          LCD_RGBPACK(69, 82, 98));
    }
    else if (scene == LIVETV_WX_SCENE_RAIN ||
             scene == LIVETV_WX_SCENE_CLOUDY)
    {
        wx_scene_gradient(LCD_RGBPACK(48, 78, 108),
                          LCD_RGBPACK(135, 157, 171));
    }
    else if (scene == LIVETV_WX_SCENE_SNOW)
    {
        wx_scene_gradient(LCD_RGBPACK(91, 131, 164),
                          LCD_RGBPACK(207, 221, 230));
    }
    else if (scene == LIVETV_WX_SCENE_FOG)
    {
        wx_scene_gradient(LCD_RGBPACK(102, 128, 143),
                          LCD_RGBPACK(190, 200, 201));
    }
    else
    {
        wx_scene_gradient(LCD_RGBPACK(18, 98, 177),
                          LCD_RGBPACK(107, 196, 231));
    }

    /* IntelliSTAR-era broadcast glass: a restrained technical grid, hard
     * chrome rules and one elapsed-time sheen. There are deliberately no
     * hand-built sun, cloud or precipitation shapes here; condition artwork
     * comes only from the dimensional icon atlas in the data cards below. */
    for (i = 0; i < LIVETV_WX_CONTENT_W; i += 40)
    {
        livetv_fill(i, LIVETV_WX_SCENE_Y, 1, LIVETV_WX_SCENE_H,
                    LCD_RGBPACK(117, 164, 199));
    }
    for (i = LIVETV_WX_SCENE_Y + 17;
         i < LIVETV_WX_SCENE_Y + LIVETV_WX_SCENE_H; i += 18)
    {
        livetv_fill(0, i, LIVETV_WX_CONTENT_W, 1,
                    LCD_RGBPACK(94, 145, 184));
    }
    /* A fully-painted blue receiver ribbon replaces the old near-black
     * title slab. Its complete inner rectangle is redrawn before every
     * update, so no partially cleared frame can present as a black bar. */
    livetv_fill(6, ribbon_y, LIVETV_WX_CONTENT_W - 12, 50,
                LCD_RGBPACK(172, 211, 234));
    livetv_fill(8, ribbon_y + 2, LIVETV_WX_CONTENT_W - 16, 46, ribbon_bg);
    livetv_fill(9, ribbon_y + 3, LIVETV_WX_CONTENT_W - 18, 2,
                LCD_RGBPACK(65, 151, 205));
    livetv_fill(9, ribbon_y + 45, LIVETV_WX_CONTENT_W - 18, 2,
                LCD_RGBPACK(4, 39, 82));
    livetv_fill(8, ribbon_y + 2, 5, 46, LCD_RGBPACK(45, 176, 230));
    livetv_fill(216, ribbon_y + 6, 1, 38, LCD_RGBPACK(91, 164, 207));

    livetv_text_at(20, ribbon_y + 10, LIVETV_TEXT, ribbon_bg,
                   "LOCAL WEATHER");
    livetv_text_at(20, ribbon_y + 28,
                   LCD_RGBPACK(105, 214, 255),
                   ribbon_bg, "WX 102  -  LIVE");

    rb->snprintf(temperature, sizeof(temperature), "%s%s",
                 hour != NULL && hour->temp[0] ? hour->temp : "--",
                 wx_unit_suffix());
    livetv_text_fit(226, ribbon_y + 10, 82, LIVETV_TEXT, ribbon_bg,
                    hour != NULL && hour->text[0] ? hour->text : "Weather");
    livetv_text_fit(226, ribbon_y + 28, 82,
                    LCD_RGBPACK(105, 214, 255), ribbon_bg, temperature);

    /* A two-pixel data pulse rides the chrome baseline. Unlike the former
     * full-height wipe it can never resemble a blank vertical or black bar. */
    livetv_fill(shine_x, LIVETV_WX_SCENE_Y + 63, 54, 1,
                LCD_RGBPACK(207, 235, 248));
    livetv_fill(shine_x + 9, LIVETV_WX_SCENE_Y + 64, 36, 1,
                LCD_RGBPACK(72, 178, 225));

    /* Chrome horizon rule tying the station ID to the data cards below. */
    livetv_fill(0, LIVETV_WX_CARD_Y - 3, LIVETV_WX_CONTENT_W, 1,
               LCD_RGBPACK(222, 235, 244));
    livetv_fill(0, LIVETV_WX_CARD_Y - 2, LIVETV_WX_CONTENT_W, 2,
               LCD_RGBPACK(18, 58, 101));

    if (commit)
        livetv_update(0, LIVETV_WX_SCENE_Y,
                      LIVETV_WX_CONTENT_W, LIVETV_WX_SCENE_H);
}

/* Panel 1: Current Conditions, laid out after the reference screenshot the
 * user supplied (a "Currently" stat list beside a big icon and the current
 * temperature) rather than a generic vertical stack. Humidity, dew point,
 * pressure and gusts are on the reference but not yet in forecast.tsv - a
 * later slice (see docs/livetv-weather-channel-spec.md) - so this only
 * shows stats the bundle actually carries; it never invents a number. */
static void livetv_wx_panel_current(void)
{
    struct livetv_wx_day *today = wx.day_count > 0 ? &wx.days[0] : NULL;
    const struct livetv_wx_hour *hour =
        wx_hour_near((int)(livetv_now_secs() / 3600));
    const char *code = hour ? hour->code : (today ? today->code : "cloudy");
    bool night = (livetv_now_secs() / 3600 >= 19) ||
                (livetv_now_secs() / 3600 < 6);
    int text_h = livetv_font_height();
    int stat_x = 10;
    int icon_col_x = 168;
    int icon_col_w = LIVETV_WX_CONTENT_W - icon_col_x - 4;
    int y = LIVETV_WX_CARD_Y;
    int i;
    struct { const char *label; char value[24]; } rows[4];
    int row_count = 0;
    char buf[24];

    livetv_fill(0, LIVETV_WX_CARD_Y, LIVETV_WX_CONTENT_W, LIVETV_WX_CARD_H,
               LIVETV_ROW_BG);

    livetv_text_fit(4, y + 4, LIVETV_WX_CONTENT_W - 8, LIVETV_DIM_TEXT,
                    LIVETV_ROW_BG, "Currently");
    y += text_h + 12;

    if (today)
    {
        rows[row_count].label = "Wind";
        rb->snprintf(rows[row_count].value, sizeof(rows[row_count].value),
                    "%s %s", today->wind_speed[0] ? today->wind_speed : "--",
                    today->wind_dir);
        row_count++;

        rows[row_count].label = "Precip";
        rb->snprintf(rows[row_count].value, sizeof(rows[row_count].value),
                    "%s%%", today->precip[0] ? today->precip : "--");
        row_count++;

        rows[row_count].label = "High";
        rb->snprintf(rows[row_count].value, sizeof(rows[row_count].value),
                    "%s%s", today->temp_max[0] ? today->temp_max : "--",
                    wx_unit_suffix());
        row_count++;

        rows[row_count].label = "Low";
        rb->snprintf(rows[row_count].value, sizeof(rows[row_count].value),
                    "%s%s", today->temp_min[0] ? today->temp_min : "--",
                    wx_unit_suffix());
        row_count++;
    }

    for (i = 0; i < row_count; i++)
    {
        int ry = y + i * (text_h + 8);

        livetv_text_at(stat_x, ry, LIVETV_SEL_BG, LIVETV_ROW_BG,
                       rows[i].label);
        livetv_text_at(stat_x + 64, ry, LIVETV_TEXT, LIVETV_ROW_BG,
                       rows[i].value);
    }

    wx_icon(icon_col_x + icon_col_w / 2, y + 40, 64, code, night);

    livetv_text_fit(icon_col_x, y + 78, icon_col_w, LIVETV_DIM_TEXT,
                    LIVETV_ROW_BG,
                    hour && hour->text[0] ? hour->text :
                    (today && today->text[0] ? today->text : "Weather"));

    if (hour && hour->temp[0])
        rb->snprintf(buf, sizeof(buf), "%s%s", hour->temp, wx_unit_suffix());
    else if (today && today->temp_max[0])
        rb->snprintf(buf, sizeof(buf), "%s%s", today->temp_max,
                    wx_unit_suffix());
    else
        rb->strcpy(buf, "--");
    livetv_text_fit(icon_col_x, y + 78 + text_h + 4, icon_col_w, LIVETV_TEXT,
                    LIVETV_ROW_BG, buf);

    livetv_update(0, LIVETV_WX_CARD_Y, LIVETV_WX_CONTENT_W, LIVETV_WX_CARD_H);
}

/* Panel 2: Today's Forecast, four time-of-day columns ------------------ */
static void livetv_wx_panel_today(void)
{
    static const int targets[4]         = {8, 12, 17, 21};
    static const char * const labels[4] = {"Morn", "Noon", "Eve", "Night"};
    int col_w = (LIVETV_WX_CONTENT_W - 8) / 4;
    int i;

    livetv_fill(0, LIVETV_WX_CARD_Y, LIVETV_WX_CONTENT_W, LIVETV_WX_CARD_H,
               LIVETV_ROW_BG);
    livetv_text_fit(4, LIVETV_WX_CARD_Y + 4, LIVETV_WX_CONTENT_W - 8,
                    LIVETV_TEXT, LIVETV_ROW_BG, "Today's Forecast");

    for (i = 0; i < 4; i++)
    {
        const struct livetv_wx_hour *hour = wx_hour_near(targets[i]);
        int x = 4 + i * col_w;
        char buf[16];

        livetv_text_fit(x, LIVETV_WX_CARD_Y + 24, col_w - 4,
                        LIVETV_DIM_TEXT, LIVETV_ROW_BG, labels[i]);
        wx_icon(x + col_w / 2 - 2, LIVETV_WX_CARD_Y + 56, 28,
               hour ? hour->code : "cloudy",
               targets[i] >= 19 || targets[i] < 6);
        if (hour && hour->temp[0])
            rb->snprintf(buf, sizeof(buf), "%s%s", hour->temp,
                        wx_unit_suffix());
        else
            rb->strcpy(buf, "--");
        livetv_text_fit(x, LIVETV_WX_CARD_Y + 83, col_w - 4, LIVETV_TEXT,
                        LIVETV_ROW_BG, buf);
    }

    livetv_update(0, LIVETV_WX_CARD_Y, LIVETV_WX_CONTENT_W, LIVETV_WX_CARD_H);
}

/* Panel 3: Extended Outlook, the same 7 days weather.c's overview shows.
 * No Regional/Travel Cities panel: forecast.tsv only ever holds one
 * location, and a second city would have to be invented to fill one in. */
static void livetv_wx_panel_extended(void)
{
    int text_h = livetv_font_height();
    int row_h = text_h + 2;
    int i;

    livetv_fill(0, LIVETV_WX_CARD_Y, LIVETV_WX_CONTENT_W, LIVETV_WX_CARD_H,
               LIVETV_ROW_BG);
    livetv_text_fit(4, LIVETV_WX_CARD_Y + 2, LIVETV_WX_CONTENT_W - 8,
                    LIVETV_TEXT, LIVETV_ROW_BG, "Extended Outlook");

    for (i = 0; i < wx.day_count; i++)
    {
        struct livetv_wx_day *day = &wx.days[i];
        int y = LIVETV_WX_CARD_Y + 18 + i * row_h;
        const char *label = (i == 0) ? "Today" :
                            (day->date[5] ? day->date + 5 : "--");
        char buf[24];

        livetv_text_fit(4, y, 44, LIVETV_TEXT, LIVETV_ROW_BG, label);
        livetv_text_fit(52, y, LIVETV_WX_CONTENT_W - 130, LIVETV_DIM_TEXT,
                        LIVETV_ROW_BG, day->text[0] ? day->text : "--");
        rb->snprintf(buf, sizeof(buf), "%s/%s%s",
                    day->temp_max[0] ? day->temp_max : "--",
                    day->temp_min[0] ? day->temp_min : "--",
                    wx_unit_suffix());
        livetv_text_at(LIVETV_WX_CONTENT_W - 76, y, LIVETV_TEXT,
                      LIVETV_ROW_BG, buf);
    }

    livetv_update(0, LIVETV_WX_CARD_Y, LIVETV_WX_CONTENT_W, LIVETV_WX_CARD_H);
}

/* Panel 4: Almanac ------------------------------------------------------ */
static void livetv_wx_panel_almanac(void)
{
    struct livetv_wx_day *today = wx.day_count > 0 ? &wx.days[0] : NULL;
    int text_h = livetv_font_height();
    int y = LIVETV_WX_CARD_Y + 8;
    char buf[32];

    livetv_fill(0, LIVETV_WX_CARD_Y, LIVETV_WX_CONTENT_W, LIVETV_WX_CARD_H,
               LIVETV_ROW_BG);
    livetv_text_fit(4, y, LIVETV_WX_CONTENT_W - 8, LIVETV_TEXT,
                    LIVETV_ROW_BG, "Almanac");
    y += text_h + 10;

    rb->snprintf(buf, sizeof(buf), "Sunrise  %s",
                today && today->sunrise[0] ? today->sunrise : "--");
    livetv_text_at(4, y, LIVETV_TEXT, LIVETV_ROW_BG, buf);
    y += text_h + 6;

    rb->snprintf(buf, sizeof(buf), "Sunset   %s",
                today && today->sunset[0] ? today->sunset : "--");
    livetv_text_at(4, y, LIVETV_TEXT, LIVETV_ROW_BG, buf);
    y += text_h + 6;

    rb->snprintf(buf, sizeof(buf), "Wind     %s %s",
                today && today->wind_speed[0] ? today->wind_speed : "--",
                today ? today->wind_dir : "");
    livetv_text_at(4, y, LIVETV_TEXT, LIVETV_ROW_BG, buf);

    livetv_update(0, LIVETV_WX_CARD_Y, LIVETV_WX_CONTENT_W, LIVETV_WX_CARD_H);
}

static void livetv_wx_draw_empty(void)
{
    int text_h = livetv_font_height();

    livetv_wx_header();
    livetv_fill(0, LIVETV_WX_BODY_Y, LIVETV_WX_CONTENT_W, LIVETV_WX_BODY_H,
               LIVETV_ROW_BG);
    livetv_text_fit(4, LIVETV_WX_BODY_Y + LIVETV_WX_BODY_H / 2 - text_h - 4,
                    LIVETV_WX_CONTENT_W - 8, LIVETV_TEXT, LIVETV_ROW_BG,
                    "Forecast unavailable");
    livetv_text_fit(4, LIVETV_WX_BODY_Y + LIVETV_WX_BODY_H / 2 + 4,
                    LIVETV_WX_CONTENT_W - 8, LIVETV_DIM_TEXT, LIVETV_ROW_BG,
                    "Sync with RockPod");
    livetv_update(0, LIVETV_WX_BODY_Y, LIVETV_WX_CONTENT_W,
                  LIVETV_WX_BODY_H);
    livetv_wx_ticker();
}

bool livetv_channel_is_weather(int chan)
{
    const struct livetv_channel *ch = livetv_channel(chan);

    return ch != NULL && !rb->strcmp(ch->category, LIVETV_WEATHER_CATEGORY);
}

bool livetv_weather_program_active(void)
{
    const struct livetv_slot *slot;
    int chan = livetv_current_channel();

    if (!livetv_channel_is_weather(chan))
        return false;

    slot = livetv_slot_at(chan, 0, NULL);
    return slot != NULL && slot->kind == LIVETV_KIND_SHOW;
}

bool livetv_weather_commercial_active(void)
{
    const struct livetv_slot *slot;
    int chan = livetv_current_channel();

    if (!livetv_channel_is_weather(chan))
        return false;

    slot = livetv_slot_at(chan, 0, NULL);
    return slot != NULL && slot->kind == LIVETV_KIND_AD;
}

uint32_t livetv_weather_program_seconds(void)
{
    uint32_t elapsed = (uint32_t)(*rb->current_tick - wx_clock_epoch);

    return wx_clock_base + elapsed / HZ;
}

bool livetv_weather_wants_video(uint32_t stream_seconds)
{
    (void)stream_seconds;

    /* Forecast panels are rendered into the same MPEG carrier as presenter
     * clips. Keeping one video clock avoids an iPod hardware race where the
     * native overlay could cover decoded presenter frames while their audio
     * was already playing. */
    return true;
}

static int livetv_wx_panel_for_time(uint32_t stream_seconds)
{
    switch ((stream_seconds % LIVETV_WX_CLOCK_SECS) /
            LIVETV_WX_PHASE_SECS)
    {
    case 0:
        return 0; /* Current conditions */
    case 1:
        return 1; /* Today's forecast */
    case 3:
        return 2; /* Extended outlook */
    default:
        return 3; /* Almanac (phase 5; harmless fallback during video) */
    }
}

void livetv_weather_enter(uint32_t stream_seconds)
{
    livetv_wx_load();
    wx_clock_base = stream_seconds;
    wx_clock_epoch = *rb->current_tick;
    wx_panel = livetv_wx_panel_for_time(stream_seconds);
    wx_anim_epoch = *rb->current_tick;
    wx_anim_next_tick = *rb->current_tick + LIVETV_WX_ANIM_RATE;
}

void livetv_weather_tick(uint32_t stream_seconds)
{
    long now = *rb->current_tick;
    int panel;

    if (!wx.loaded)
        return;

    panel = livetv_wx_panel_for_time(stream_seconds);
    if (panel != wx_panel)
    {
        wx_panel = panel;
        wx_anim_epoch = now;
        livetv_weather_draw();
        return;
    }

    if (TIME_BEFORE(now, wx_anim_next_tick))
        return;

    /* Use elapsed ticks for position, but schedule from "now" so a slow
     * decoder never creates a catch-up repaint storm. This refresh touches
     * only the scene band; forecast text and cached icons stay undisturbed. */
    wx_anim_next_tick = now + LIVETV_WX_ANIM_RATE;
    wx_scene_draw(true);
}

void livetv_weather_draw(void)
{
    livetv_tv(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
    if (!wx.loaded)
    {
        livetv_wx_draw_empty();
        return;
    }

    livetv_wx_header();
    wx_scene_draw(false);
    switch (wx_panel % LIVETV_WX_PANEL_COUNT)
    {
    case 0:
        livetv_wx_panel_current();
        break;
    case 1:
        livetv_wx_panel_today();
        break;
    case 2:
        livetv_wx_panel_extended();
        break;
    default:
        livetv_wx_panel_almanac();
        break;
    }
    livetv_wx_ticker();
}

void livetv_weather_draw_commercial_overlay(void)
{
    livetv_tv(TV_GUIDE_RELEASE,0,0,0,0,0,NULL);
    const struct livetv_wx_hour *hour;
    struct livetv_wx_day *today;
    int text_h = livetv_font_height();
    char primary[64];
    char secondary[64];
    char tertiary[64];
    const int station_w = 58;
    const int icon_w = 54;
    const int copy_x = station_w + icon_w + 4;
    const fb_data station_bg = LCD_RGBPACK(7, 66, 122);
    const fb_data icon_bg = LCD_RGBPACK(8, 42, 77);
    const fb_data copy_bg = LCD_RGBPACK(3, 22, 55);

    if (!wx.loaded)
        livetv_wx_load();
    hour = wx_current_hour();
    today = wx.day_count > 0 ? &wx.days[0] : NULL;

    livetv_fill(0, LIVETV_WX_AD_OVERLAY_Y, LCD_WIDTH,
                LIVETV_WX_AD_OVERLAY_H, copy_bg);
    livetv_fill(0, LIVETV_WX_AD_OVERLAY_Y, LCD_WIDTH, 3,
                LCD_RGBPACK(62, 188, 240));
    livetv_fill(0, LIVETV_WX_AD_OVERLAY_Y + 3, station_w,
                LIVETV_WX_AD_OVERLAY_H - 3, station_bg);
    livetv_fill(station_w, LIVETV_WX_AD_OVERLAY_Y + 3, icon_w,
                LIVETV_WX_AD_OVERLAY_H - 3, icon_bg);

    livetv_text_at(6, LIVETV_WX_AD_OVERLAY_Y + 11,
                   LCD_WHITE, station_bg, "WX 102");
    livetv_text_at(6, LIVETV_WX_AD_OVERLAY_Y + 13 + text_h,
                   LCD_RGBPACK(105, 214, 255), station_bg,
                   "LOCAL");
    livetv_text_at(6, LIVETV_WX_AD_OVERLAY_Y + 15 + text_h * 2,
                   LCD_RGBPACK(219, 235, 245), station_bg,
                   "LIVE");

    /* Use the same dimensional bitmap pack as the forecast panels. This is
     * synced forecast artwork, not a hand-drawn approximation. */
    wx_icon(station_w + icon_w / 2, LIVETV_WX_AD_OVERLAY_Y + 34, 40,
            wx_current_code(), !wx_current_is_day());

    rb->snprintf(
        primary, sizeof(primary), "%s  %s%s",
        wx.location[0] ? wx.location : "Weather",
        hour && hour->temp[0] ? hour->temp : "--",
        wx_unit_suffix());
    livetv_text_fit(copy_x, LIVETV_WX_AD_OVERLAY_Y + 6,
                    LCD_WIDTH - copy_x - 4, LCD_WHITE, copy_bg, primary);

    rb->snprintf(
        secondary, sizeof(secondary), "%s  Rain %s%%",
        hour && hour->text[0] ? hour->text : "Current conditions",
        hour && hour->precip[0] ? hour->precip : "--");
    livetv_text_fit(copy_x, LIVETV_WX_AD_OVERLAY_Y + 8 + text_h,
                    LCD_WIDTH - copy_x - 4, LCD_RGBPACK(166, 218, 247),
                    copy_bg, secondary);

    rb->snprintf(
        tertiary, sizeof(tertiary), "Wind %s %s  H/L %s/%s%s",
        today && today->wind_dir[0] ? today->wind_dir : "--",
        today && today->wind_speed[0] ? today->wind_speed : "--",
        today && today->temp_max[0] ? today->temp_max : "--",
        today && today->temp_min[0] ? today->temp_min : "--",
        wx_unit_suffix());
    livetv_text_fit(copy_x, LIVETV_WX_AD_OVERLAY_Y + 10 + text_h * 2,
                    LCD_WIDTH - copy_x - 4, LCD_RGBPACK(219, 235, 245),
                    copy_bg, tertiary);
    livetv_update(0, LIVETV_WX_AD_OVERLAY_Y, LCD_WIDTH,
                  LIVETV_WX_AD_OVERLAY_H);
}

void livetv_weather_commercial_text(char *primary, size_t primary_size,
                                    char *secondary, size_t secondary_size,
                                    char *tertiary, size_t tertiary_size)
{
    const struct livetv_wx_hour *hour;
    const struct livetv_wx_day *today;

    if (!wx.loaded)
        livetv_wx_load();
    hour = wx_current_hour();
    today = wx.day_count > 0 ? &wx.days[0] : NULL;

    rb->snprintf(
        primary, primary_size, "%s  %s%s",
        wx.location[0] ? wx.location : "Weather",
        hour && hour->temp[0] ? hour->temp : "--",
        wx_unit_suffix());
    rb->snprintf(
        secondary, secondary_size, "%s  Rain %s%%",
        hour && hour->text[0] ? hour->text : "Current conditions",
        hour && hour->precip[0] ? hour->precip : "--");
    rb->snprintf(
        tertiary, tertiary_size, "Wind %s %s  H/L %s/%s%s",
        today && today->wind_dir[0] ? today->wind_dir : "--",
        today && today->wind_speed[0] ? today->wind_speed : "--",
        today && today->temp_max[0] ? today->temp_max : "--",
        today && today->temp_min[0] ? today->temp_min : "--",
        wx_unit_suffix());
}

#endif /* HAVE_LCD_COLOR */
