/***************************************************************************
 * Magazines: offline bookshelf and page-turn reader for 320x240 iPods.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 ****************************************************************************/

#include "plugin.h"
#include "lib/ipodjs_retailos_controls.h"
#include "lib/helper.h"
#include "lib/pluginlib_actions.h"

#if !defined(HAVE_LCD_COLOR) || LCD_WIDTH < 320 || LCD_HEIGHT < 240
#error Magazines requires a 320x240 colour display
#endif

#define MAG_ROOT                "/Magazines"
#define MAG_CATALOG             MAG_ROOT "/catalog.mgi"
#define MAG_STATE               PLUGIN_APPS_DATA_DIR "/magazines.cfg"
#define MAG_STATE_TMP           PLUGIN_APPS_DATA_DIR "/magazines.cfg.tmp"
#define MAG_LOCK_PIN            ROCKBOX_DIR "/videolist/locked.pin"
#define MAG_MAX_ISSUES          64
#define MAG_MAX_CATEGORIES      32
#define MAG_MAX_BOOKMARKS       8
#define MAG_VISIBLE_COVERS      6
#define MAG_TITLE_LEN           96
#define MAG_CREATOR_LEN         64
#define MAG_DIR_LEN             96
#define MAG_CATEGORY_LEN        49
#define MAG_COVER_W             68
#define MAG_COVER_H             88
#define MAG_PIN_LEN             4
#define MAG_APPLE_DIR           ROCKBOX_DIR "/ipodjs/apple"
#define MAG_FIT_W               316
#define MAG_FIT_H               236
#define MAG_ZOOM_SOURCE_W       720
#define MAG_ZOOM_SOURCE_H       960
#define MAG_ZOOM_MIN_PERCENT    100
#define MAG_ZOOM_MAX_PERCENT    500
#define MAG_ZOOM_START_PERCENT  200
#define MAG_ZOOM_STEP           10
#define MAG_ZOOM_REPEAT_STEP    25
#define MAG_JPEG_TAIL           (128 * 1024)
#define MAG_TURN_TICKS          (HZ / 4)
#define MAG_TURN_FRAMES         14
#define MAG_SAVE_SETTLE         (HZ * 2)

#define MAG_WOOD_DARK           LCD_RGBPACK(65, 36, 18)
#define MAG_WOOD                LCD_RGBPACK(126, 75, 36)
#define MAG_WOOD_LIGHT          LCD_RGBPACK(178, 116, 59)
#define MAG_WOOD_GLOW           LCD_RGBPACK(206, 147, 79)
#define MAG_HEADER_TOP          LCD_RGBPACK(252, 253, 253)
#define MAG_HEADER_MID          LCD_RGBPACK(216, 219, 223)
#define MAG_HEADER_BOTTOM       LCD_RGBPACK(174, 178, 183)
#define MAG_PAPER               LCD_RGBPACK(255, 253, 247)
#define MAG_READER_BG           LCD_RGBPACK(17, 18, 20)
#define MAG_ACCENT              LCD_RGBPACK(50, 132, 238)

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) \
    || (CONFIG_KEYPAD == IPOD_3G_PAD) \
    || (CONFIG_KEYPAD == IPOD_4G_PAD)
#define MAG_IPOD_CONTROLS
#endif

#ifdef MAG_IPOD_CONTROLS
static const struct button_mapping magazines_context[] =
{
    { PLA_SCROLL_BACK,        BUTTON_SCROLL_BACK,                BUTTON_NONE },
    { PLA_SCROLL_FWD,         BUTTON_SCROLL_FWD,                 BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT,  BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT,  BUTTON_SCROLL_FWD|BUTTON_REPEAT,   BUTTON_NONE },
    { PLA_CANCEL,             BUTTON_MENU,                       BUTTON_NONE },
    { PLA_LEFT,               BUTTON_LEFT|BUTTON_REL,            BUTTON_LEFT },
    { PLA_LEFT_REPEAT,        BUTTON_LEFT|BUTTON_REPEAT,         BUTTON_LEFT },
    { PLA_RIGHT,              BUTTON_RIGHT|BUTTON_REL,           BUTTON_RIGHT },
    { PLA_RIGHT_REPEAT,       BUTTON_RIGHT|BUTTON_REPEAT,        BUTTON_RIGHT },
    { PLA_SELECT,             BUTTON_SELECT,                     BUTTON_NONE },
    { PLA_UP,                 BUTTON_PLAY|BUTTON_REL,            BUTTON_PLAY },
    LAST_ITEM_IN_LIST
};

static const struct button_mapping magazines_zoom_context[] =
{
    { PLA_CANCEL,             BUTTON_MENU|BUTTON_SELECT,         BUTTON_NONE },
    { PLA_SCROLL_BACK,        BUTTON_SCROLL_BACK,                BUTTON_NONE },
    { PLA_SCROLL_FWD,         BUTTON_SCROLL_FWD,                 BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT,  BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT,  BUTTON_SCROLL_FWD|BUTTON_REPEAT,   BUTTON_NONE },
    { PLA_UP,                 BUTTON_MENU,                       BUTTON_NONE },
    { PLA_UP_REPEAT,          BUTTON_MENU|BUTTON_REPEAT,         BUTTON_NONE },
    { PLA_DOWN,               BUTTON_PLAY,                       BUTTON_NONE },
    { PLA_DOWN_REPEAT,        BUTTON_PLAY|BUTTON_REPEAT,         BUTTON_NONE },
    { PLA_LEFT,               BUTTON_LEFT,                       BUTTON_NONE },
    { PLA_LEFT_REPEAT,        BUTTON_LEFT|BUTTON_REPEAT,         BUTTON_NONE },
    { PLA_RIGHT,              BUTTON_RIGHT,                      BUTTON_NONE },
    { PLA_RIGHT_REPEAT,       BUTTON_RIGHT|BUTTON_REPEAT,        BUTTON_NONE },
    LAST_ITEM_IN_LIST
};
#endif

struct magazine_issue
{
    char directory[MAG_DIR_LEN];
    char id[MAG_DIR_LEN];
    char title[MAG_TITLE_LEN];
    char creator[MAG_CREATOR_LEN];
    char category[MAG_CATEGORY_LEN];
    int year;
    int page_count;
    int saved_page;
    unsigned opened;
    bool completed;
    bool locked;
    int bookmarks[MAG_MAX_BOOKMARKS];
    int bookmark_count;
};

struct magazine_category
{
    char name[MAG_CATEGORY_LEN];
    int unlocked_count;
    int locked_count;
};

/* PIN pixels are held in the existing plugin-owned arena. */

struct magazine_cover
{
    int issue_index;
    bool ready;
    struct bitmap bitmap;
    fb_data *pixels;
};

struct magazine_page
{
    int number;
    bool ready;
    struct bitmap bitmap;
    fb_data *pixels;
};

struct magazine_app
{
    unsigned char *arena;
    size_t arena_size;
    size_t arena_used;

    struct magazine_issue *issues;
    int issue_count;
    int selected;
    int shelf_start;
    int visible[MAG_MAX_ISSUES];
    int visible_count;
    struct magazine_category categories[MAG_MAX_CATEGORIES];
    int category_count;
    int category_selected;
    int category_visible[MAG_MAX_CATEGORIES + 1];
    int category_visible_count;
    bool category_locked_view;
    bool selection_bookmarks;
    int bookmark_selected;
    int active_category;
    bool active_locked;
    bool locks_unlocked;
    bool back_to_categories;
    bool sort_recent;
    unsigned open_sequence;
    char last_issue[MAG_DIR_LEN];

    struct magazine_cover covers[MAG_VISIBLE_COVERS];
    struct magazine_page previous;
    struct magazine_page current;
    struct magazine_page next;

    fb_data *fit_pixels[3];
    fb_data *compose;
    unsigned char *fit_decode;
    unsigned char *zoom_decode;
    size_t fit_decode_size;
    size_t zoom_decode_size;
    struct bitmap zoom;
    int zoom_x;
    int zoom_y;
    int zoom_page;
    int zoom_percent;
    bool zoom_ready;

    struct magazine_issue *reading;
    long save_deadline;
    bool state_dirty;
    struct ipodjs_retailos_pin_cache pin;
    int category_font;
};

static struct magazine_app app;

static void mag_page_path(char *path, size_t size, int number);

static void mag_settle_select(void)
{
#ifdef MAG_IPOD_CONTROLS
    long deadline = *rb->current_tick + HZ / 2;

    while ((rb->button_status() & BUTTON_SELECT) &&
           TIME_BEFORE(*rb->current_tick, deadline))
        rb->sleep(1);
#else
    rb->sleep(MAX(1, HZ / 20));
#endif
    rb->button_clear_queue();
}

static void *mag_alloc(size_t bytes)
{
    size_t position = (app.arena_used + 7) & ~(size_t)7;

    if (bytes > app.arena_size || position > app.arena_size - bytes)
        return NULL;
    app.arena_used = position + bytes;
    return app.arena + position;
}

static bool mag_allocate(void)
{
    size_t size;
    unsigned char *buffer = rb->plugin_get_buffer(&size);
    int i;

    if (!buffer)
        return false;

    rb->memset(&app, 0, sizeof(app));
    app.arena = buffer;
    app.arena_size = size;
    app.issues = mag_alloc(sizeof(*app.issues) * MAG_MAX_ISSUES);
    app.pin.panel_data = mag_alloc(
        IPODJS_RETAILOS_PIN_PANEL_BYTES);
    app.pin.field_data = mag_alloc(
        IPODJS_RETAILOS_PIN_FIELD_BYTES);
    app.pin.selected_data = mag_alloc(
        IPODJS_RETAILOS_PIN_SELECTED_BYTES);
    for (i = 0; i < 3; i++)
        app.fit_pixels[i] = mag_alloc(
            BM_SIZE(MAG_FIT_W, MAG_FIT_H, FORMAT_NATIVE, false));
    app.compose = mag_alloc(
        BM_SIZE(LCD_WIDTH, LCD_HEIGHT, FORMAT_NATIVE, false));
    app.fit_decode_size =
        BM_SIZE(MAG_FIT_W, MAG_FIT_H, FORMAT_NATIVE, false) + MAG_JPEG_TAIL;
    app.fit_decode = mag_alloc(app.fit_decode_size);
    /* Retain the completed zoom inside its decoder workspace. Zoom and
     * spread decode lifetimes never overlap, so no duplicate page copy is
     * needed: 720 * 960 * sizeof(fb_data), plus the JPEG decoder tail.
     * Continuous zoom renders through app.compose without re-decoding. */
    app.zoom_decode_size =
        BM_SIZE(MAG_ZOOM_SOURCE_W, MAG_ZOOM_SOURCE_H,
                FORMAT_NATIVE, false) +
        MAG_JPEG_TAIL;
    app.zoom_decode = mag_alloc(app.zoom_decode_size);
    for (i = 0; i < MAG_VISIBLE_COVERS; i++)
    {
        app.covers[i].pixels = mag_alloc(
            BM_SIZE(MAG_COVER_W, MAG_COVER_H, FORMAT_NATIVE, false));
        app.covers[i].issue_index = -1;
    }

    if (!app.issues || !app.pin.panel_data || !app.pin.field_data ||
        !app.pin.selected_data ||
        !app.fit_pixels[0] || !app.fit_pixels[1] ||
        !app.fit_pixels[2] || !app.compose || !app.fit_decode ||
        !app.zoom_decode ||
        !app.covers[MAG_VISIBLE_COVERS - 1].pixels)
        return false;

    app.previous.pixels = app.fit_pixels[0];
    app.current.pixels = app.fit_pixels[1];
    app.next.pixels = app.fit_pixels[2];
    app.selected = 0;
    app.category_font = -1;
    return true;
}

static void mag_gradient(int y, int height, unsigned top, unsigned bottom)
{
    int row;
    int tr = RGB_UNPACK_RED(top);
    int tg = RGB_UNPACK_GREEN(top);
    int tb = RGB_UNPACK_BLUE(top);
    int br = RGB_UNPACK_RED(bottom);
    int bg = RGB_UNPACK_GREEN(bottom);
    int bb = RGB_UNPACK_BLUE(bottom);

    for (row = 0; row < height; row++)
    {
        unsigned color = LCD_RGBPACK(
            tr + (br - tr) * row / MAX(1, height - 1),
            tg + (bg - tg) * row / MAX(1, height - 1),
            tb + (bb - tb) * row / MAX(1, height - 1));
        rb->lcd_set_foreground(color);
        rb->lcd_hline(0, LCD_WIDTH - 1, y + row);
    }
}

static void mag_puts_fit(int x, int y, int width, const char *text,
                         bool center)
{
    char buffer[MAG_TITLE_LEN];
    int text_width;
    int height;
    int length;

    rb->strlcpy(buffer, text, sizeof(buffer));
    length = rb->strlen(buffer);
    while (length > 1)
    {
        rb->lcd_getstringsize((const unsigned char *)buffer,
                              &text_width, &height);
        if (text_width <= width)
            break;
        buffer[--length] = '\0';
    }
    if (rb->strcmp(buffer, text) && length > 3)
    {
        buffer[length - 1] = '.';
        buffer[length - 2] = '.';
        buffer[length - 3] = '.';
    }
    rb->lcd_getstringsize((const unsigned char *)buffer, &text_width, &height);
    if (center)
        x += MAX(0, (width - text_width) / 2);
    rb->lcd_putsxy(x, y, (const unsigned char *)buffer);
}

static char *mag_trim(char *line)
{
    char *end;

    while (*line == ' ' || *line == '\t')
        line++;
    end = line + rb->strlen(line);
    while (end > line &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return line;
}

static bool mag_safe_component(const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

    if (!value[0] || !rb->strcmp(value, ".") || !rb->strcmp(value, ".."))
        return false;
    while (*cursor)
    {
        if (*cursor == '/' || *cursor == '\\' || *cursor < 32)
            return false;
        cursor++;
    }
    return true;
}

static bool mag_parse_manifest(struct magazine_issue *issue)
{
    char path[MAX_PATH];
    char line[256];
    int fd;
    int schema = 0;

    rb->snprintf(path, sizeof(path), "%s/%s/issue.mgi",
                 MAG_ROOT, issue->directory);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;

    issue->saved_page = 1;
    rb->strlcpy(issue->category, "Uncategorized",
                sizeof(issue->category));
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *value;
        char *key = mag_trim(line);

        if (!key[0] || key[0] == '#')
            continue;
        value = rb->strchr(key, '=');
        if (!value)
            continue;
        *value++ = '\0';
        value = mag_trim(value);
        if (!rb->strcmp(key, "schema"))
            schema = rb->atoi(value);
        else if (!rb->strcmp(key, "id"))
            rb->strlcpy(issue->id, value, sizeof(issue->id));
        else if (!rb->strcmp(key, "title"))
            rb->strlcpy(issue->title, value, sizeof(issue->title));
        else if (!rb->strcmp(key, "creator"))
            rb->strlcpy(issue->creator, value, sizeof(issue->creator));
        else if (!rb->strcmp(key, "category") && value[0])
            rb->strlcpy(issue->category, value, sizeof(issue->category));
        else if (!rb->strcmp(key, "locked"))
            issue->locked = rb->atoi(value) != 0;
        else if (!rb->strcmp(key, "year"))
            issue->year = rb->atoi(value);
        else if (!rb->strcmp(key, "page_count"))
            issue->page_count = rb->atoi(value);
    }
    rb->close(fd);

    if (!issue->id[0])
        rb->strlcpy(issue->id, issue->directory, sizeof(issue->id));
    return schema == 1 && issue->title[0] && issue->page_count > 0 &&
           issue->page_count <= 9999 && mag_safe_component(issue->id);
}

static bool mag_load_catalog(void)
{
    char line[256];
    int fd = rb->open(MAG_CATALOG, O_RDONLY);

    app.issue_count = 0;
    if (fd < 0)
        return false;

    while (app.issue_count < MAG_MAX_ISSUES &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *directory = mag_trim(line);
        struct magazine_issue *issue;

        if (!directory[0] || directory[0] == '#' ||
            !mag_safe_component(directory))
            continue;
        issue = &app.issues[app.issue_count];
        rb->memset(issue, 0, sizeof(*issue));
        rb->strlcpy(issue->directory, directory, sizeof(issue->directory));
        if (mag_parse_manifest(issue))
            app.issue_count++;
    }
    rb->close(fd);
    return true;
}

static void mag_load_state(void)
{
    char line[256];
    int fd;
    int i;

    rb->mkdir(PLUGIN_APPS_DATA_DIR);
    fd = rb->open(MAG_STATE, O_RDONLY);
    if (fd < 0)
        return;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *value;
        char *key = mag_trim(line);

        if (!key[0] || key[0] == '#')
            continue;
        value = rb->strchr(key, '=');
        if (!value)
            continue;
        *value++ = '\0';
        value = mag_trim(value);
        if (!rb->strcmp(key, "sort"))
            app.sort_recent = rb->atoi(value) != 0;
        else if (!rb->strcmp(key, "last"))
            rb->strlcpy(app.last_issue, value, sizeof(app.last_issue));
        else if (!rb->strncmp(key, "issue.", 6))
        {
            char *field1 = rb->strchr(value, '|');
            char *field2;
            char *field3;

            if (!field1)
                continue;
            *field1++ = '\0';
            field2 = rb->strchr(field1, '|');
            if (!field2)
                continue;
            *field2++ = '\0';
            field3 = rb->strchr(field2, '|');
            if (!field3)
                continue;
            *field3++ = '\0';
            for (i = 0; i < app.issue_count; i++)
            {
                if (!rb->strcmp(app.issues[i].id, key + 6))
                {
                    char *bookmark = field3;

                    app.issues[i].saved_page = rb->atoi(value);
                    app.issues[i].completed = rb->atoi(field1) != 0;
                    app.issues[i].opened = rb->atoi(field2);
                    app.open_sequence = MAX(app.open_sequence,
                                            app.issues[i].opened);
                    while (bookmark && *bookmark &&
                           app.issues[i].bookmark_count < MAG_MAX_BOOKMARKS)
                    {
                        char *next = rb->strchr(bookmark, ',');
                        int page;

                        if (next)
                            *next++ = '\0';
                        page = rb->atoi(bookmark);
                        if (page > 0 &&
                            page <= app.issues[i].page_count)
                        {
                            int position;

                            if (page > 1 &&
                                page < app.issues[i].page_count &&
                                (page & 1))
                                page--;
                            position = app.issues[i].bookmark_count;
                            while (position > 0 &&
                                   app.issues[i].bookmarks[position - 1] >
                                   page)
                            {
                                app.issues[i].bookmarks[position] =
                                    app.issues[i].bookmarks[position - 1];
                                position--;
                            }
                            if ((position == 0 ||
                                 app.issues[i].bookmarks[position - 1] !=
                                 page) &&
                                (position ==
                                 app.issues[i].bookmark_count ||
                                 app.issues[i].bookmarks[position] != page))
                            {
                                app.issues[i].bookmarks[position] = page;
                                app.issues[i].bookmark_count++;
                            }
                        }
                        bookmark = next;
                    }
                    break;
                }
            }
        }
    }
    rb->close(fd);
}

static bool mag_save_state(void)
{
    int fd;
    int i;

    if (!app.state_dirty)
        return true;
    rb->mkdir(PLUGIN_APPS_DATA_DIR);
    fd = rb->open(MAG_STATE_TMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    rb->fdprintf(fd, "schema=1\n");
    rb->fdprintf(fd, "sort=%d\n", app.sort_recent ? 1 : 0);
    rb->fdprintf(fd, "last=%s\n", app.last_issue);
    for (i = 0; i < app.issue_count; i++)
    {
        const struct magazine_issue *issue = &app.issues[i];
        int bookmark;

        rb->fdprintf(fd, "issue.%s=%d|%d|%u|",
                     issue->id, issue->saved_page,
                     issue->completed ? 1 : 0, issue->opened);
        for (bookmark = 0; bookmark < issue->bookmark_count; bookmark++)
            rb->fdprintf(fd, "%s%d", bookmark ? "," : "",
                         issue->bookmarks[bookmark]);
        rb->fdprintf(fd, "\n");
    }
    rb->close(fd);
    rb->remove(MAG_STATE);
    if (rb->rename(MAG_STATE_TMP, MAG_STATE) < 0)
        return false;
    app.state_dirty = false;
    return true;
}

static int mag_compare_title(const void *left, const void *right)
{
    const struct magazine_issue *a = left;
    const struct magazine_issue *b = right;
    return rb->strcasecmp(a->title, b->title);
}

static int mag_compare_recent(const void *left, const void *right)
{
    const struct magazine_issue *a = left;
    const struct magazine_issue *b = right;

    if (a->opened != b->opened)
        return a->opened < b->opened ? 1 : -1;
    return rb->strcasecmp(a->title, b->title);
}

static int mag_compare_category(const void *left, const void *right)
{
    const struct magazine_category *a = left;
    const struct magazine_category *b = right;
    return rb->strcasecmp(a->name, b->name);
}

static int mag_issue_index_at(int position)
{
    if (position < 0 || position >= app.visible_count)
        return -1;
    return app.visible[position];
}

static void mag_build_categories(void)
{
    int i;

    app.category_count = 0;
    for (i = 0; i < app.issue_count; i++)
    {
        int category;

        for (category = 0; category < app.category_count; category++)
        {
            if (!rb->strcasecmp(app.categories[category].name,
                                app.issues[i].category))
                break;
        }
        if (category == app.category_count &&
            app.category_count < MAG_MAX_CATEGORIES)
        {
            rb->memset(&app.categories[category], 0,
                       sizeof(app.categories[category]));
            rb->strlcpy(app.categories[category].name,
                        app.issues[i].category,
                        sizeof(app.categories[category].name));
            app.category_count++;
        }
        if (category < app.category_count)
        {
            if (app.issues[i].locked)
                app.categories[category].locked_count++;
            else
                app.categories[category].unlocked_count++;
        }
    }
    rb->qsort(app.categories, app.category_count,
              sizeof(*app.categories), mag_compare_category);
}

static void mag_refresh_category_view(bool locked_view)
{
    int category;

    app.category_locked_view = locked_view;
    app.category_visible_count = 0;
    for (category = 0; category < app.category_count; category++)
    {
        int count = locked_view ?
                    app.categories[category].locked_count :
                    app.categories[category].unlocked_count;

        if (count > 0)
            app.category_visible[app.category_visible_count++] = category;
    }
    if (!locked_view)
    {
        for (category = 0; category < app.category_count; category++)
        {
            if (app.categories[category].locked_count > 0)
            {
                app.category_visible[app.category_visible_count++] = -1;
                break;
            }
        }
    }
    app.category_selected = MIN(app.category_selected,
                                MAX(0, app.category_visible_count - 1));
}

static int mag_visible_bookmark_count(void)
{
    int count = 0;
    int issue;

    for (issue = 0; issue < app.issue_count; issue++)
    {
        if (app.issues[issue].locked && !app.locks_unlocked)
            continue;
        count += app.issues[issue].bookmark_count;
    }
    return count;
}

static bool mag_bookmark_at(int position, int *issue_index,
                            int *bookmark_index)
{
    int issue;

    if (position < 0)
        return false;
    for (issue = 0; issue < app.issue_count; issue++)
    {
        int count;

        if (app.issues[issue].locked && !app.locks_unlocked)
            continue;
        count = app.issues[issue].bookmark_count;
        if (position < count)
        {
            *issue_index = issue;
            *bookmark_index = position;
            return true;
        }
        position -= count;
    }
    return false;
}

static int mag_find_bookmark_position(const char *issue_id, int page)
{
    int count = mag_visible_bookmark_count();
    int position;

    for (position = 0; position < count; position++)
    {
        int issue_index;
        int bookmark_index;

        if (mag_bookmark_at(position, &issue_index, &bookmark_index) &&
            !rb->strcmp(app.issues[issue_index].id, issue_id) &&
            app.issues[issue_index].bookmarks[bookmark_index] == page)
            return position;
    }
    return 0;
}

static int mag_issue_spread_end(const struct magazine_issue *issue,
                                int start)
{
    if (start <= 1 || start >= issue->page_count)
        return start;
    return MIN(start + 1, issue->page_count - 1);
}

static int mag_select_hold(void)
{
#ifdef MAG_IPOD_CONTROLS
    long deadline = *rb->current_tick + HZ / 2;
    bool held = false;

    while (rb->button_status() & BUTTON_SELECT)
    {
        int button;

        if (TIME_AFTER(*rb->current_tick, deadline))
        {
            held = true;
            break;
        }
        button = rb->button_get_w_tmo(1);
        if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
            return -1;
    }
    while (rb->button_status() & BUTTON_SELECT)
    {
        int button = rb->button_get_w_tmo(1);

        if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
            return -1;
    }
    rb->button_clear_queue();
    return held ? 1 : 0;
#else
    return 0;
#endif
}

static void mag_select_category(int category, const char *issue_id,
                                bool locked)
{
    int i;

    app.active_category = category;
    app.active_locked = locked;
    app.visible_count = 0;
    app.selected = 0;
    for (i = 0; i < app.issue_count; i++)
    {
        if (app.issues[i].locked != locked)
            continue;
        if (category >= 0 &&
            rb->strcasecmp(app.issues[i].category,
                           app.categories[category].name))
            continue;
        app.visible[app.visible_count] = i;
        if (issue_id && issue_id[0] &&
            !rb->strcmp(app.issues[i].id, issue_id))
            app.selected = app.visible_count;
        app.visible_count++;
    }
    app.shelf_start = (app.selected / MAG_VISIBLE_COVERS) *
                      MAG_VISIBLE_COVERS;
}

static void mag_sort_issues(void)
{
    char selected_id[MAG_DIR_LEN] = "";
    int issue_index = mag_issue_index_at(app.selected);

    if (issue_index >= 0)
        rb->strlcpy(selected_id, app.issues[issue_index].id,
                    sizeof(selected_id));
    rb->qsort(app.issues, app.issue_count, sizeof(*app.issues),
              app.sort_recent ? mag_compare_recent : mag_compare_title);
    mag_build_categories();
    mag_select_category(app.active_category, selected_id,
                        app.active_locked);
}

static bool mag_decode_bounded(const char *path, struct bitmap *bitmap,
                               int max_width, int max_height,
                               unsigned char *scratch, size_t scratch_size)
{
    int result;

    rb->memset(bitmap, 0, sizeof(*bitmap));
    bitmap->width = max_width;
    bitmap->height = max_height;
    bitmap->data = scratch;
    result = rb->read_jpeg_file(path, bitmap, scratch_size,
                                FORMAT_NATIVE | FORMAT_RESIZE |
                                FORMAT_KEEP_ASPECT, NULL);
    return result > 0 && bitmap->width > 0 && bitmap->height > 0 &&
           bitmap->width <= max_width && bitmap->height <= max_height;
}

static int mag_spread_start(int page)
{
    if (page <= 1)
        return 1;
    if (page >= app.reading->page_count)
        return app.reading->page_count;
    return page & 1 ? page - 1 : page;
}

static int mag_bookmark_index(const struct magazine_issue *issue, int page)
{
    int i;

    for (i = 0; i < issue->bookmark_count; i++)
    {
        if (issue->bookmarks[i] == page)
            return i;
    }
    return -1;
}

static bool mag_toggle_bookmark(struct magazine_issue *issue, int page)
{
    int index = mag_bookmark_index(issue, page);
    int i;

    if (index >= 0)
    {
        for (i = index; i + 1 < issue->bookmark_count; i++)
            issue->bookmarks[i] = issue->bookmarks[i + 1];
        issue->bookmark_count--;
        app.state_dirty = true;
        return false;
    }
    if (issue->bookmark_count >= MAG_MAX_BOOKMARKS)
        return false;
    i = issue->bookmark_count;
    while (i > 0 && issue->bookmarks[i - 1] > page)
    {
        issue->bookmarks[i] = issue->bookmarks[i - 1];
        i--;
    }
    issue->bookmarks[i] = page;
    issue->bookmark_count++;
    app.state_dirty = true;
    return true;
}

static int mag_near_bookmark(const struct magazine_issue *issue, int page,
                             bool forward)
{
    int i;

    if (issue->bookmark_count == 0)
        return page;
    if (forward)
    {
        for (i = 0; i < issue->bookmark_count; i++)
        {
            if (issue->bookmarks[i] > page)
                return issue->bookmarks[i];
        }
        return issue->bookmarks[0];
    }
    for (i = issue->bookmark_count - 1; i >= 0; i--)
    {
        if (issue->bookmarks[i] < page)
            return issue->bookmarks[i];
    }
    return issue->bookmarks[issue->bookmark_count - 1];
}

static int mag_spread_end(int start)
{
    if (start <= 1 || start >= app.reading->page_count)
        return start;
    return MIN(start + 1, app.reading->page_count - 1);
}

static int mag_adjacent_spread(int start, bool forward)
{
    int candidate;

    if (forward)
    {
        if (start >= app.reading->page_count)
            return 0;
        if (start == 1)
            return app.reading->page_count > 2 ? 2 :
                                                  app.reading->page_count;
        candidate = start + 2;
        return candidate >= app.reading->page_count ?
               app.reading->page_count : candidate;
    }
    if (start <= 1)
        return 0;
    if (start == app.reading->page_count)
    {
        candidate = app.reading->page_count - 1;
        if (candidate & 1)
            candidate--;
        return candidate >= 2 ? candidate : 1;
    }
    return start <= 2 ? 1 : start - 2;
}

static bool mag_load_spread_bitmap(int start, struct bitmap *bitmap,
                                   fb_data *destination, int width, int height,
                                   unsigned char *scratch, size_t scratch_size)
{
    struct bitmap decoded;
    fb_data background = FB_SCALARPACK(MAG_READER_BG);
    int end = mag_spread_end(start);
    int half = (width - 4) / 2;
    int page;
    int i;

    if (start == end)
    {
        char path[MAX_PATH];
        size_t bytes;

        mag_page_path(path, sizeof(path), start);
        if (!mag_decode_bounded(path, &decoded, width, height,
                                scratch, scratch_size))
            return false;
        bytes = BM_SIZE(decoded.width, decoded.height, FORMAT_NATIVE, false);
        rb->memcpy(destination, decoded.data, bytes);
        *bitmap = decoded;
        bitmap->data = (unsigned char *)destination;
        return true;
    }

    for (i = 0; i < width * height; i++)
        destination[i] = background;
    for (page = start; page <= end; page++)
    {
        char path[MAX_PATH];
        const fb_data *source;
        int x;
        int y;
        int row;

        mag_page_path(path, sizeof(path), page);
        if (!mag_decode_bounded(path, &decoded, half, height,
                                scratch, scratch_size))
            return false;
        source = (const fb_data *)decoded.data;
        x = page == start ? half - decoded.width : half + 4;
        y = (height - decoded.height) / 2;
        for (row = 0; row < decoded.height; row++)
            rb->memcpy(
                &destination[(y + row) * width + x],
                &source[row * decoded.width],
                decoded.width * sizeof(fb_data));
    }
    bitmap->width = width;
    bitmap->height = height;
    bitmap->data = (unsigned char *)destination;
    return true;
}

static bool mag_load_cover(struct magazine_cover *cover, int issue_index)
{
    char path[MAX_PATH];
    struct bitmap decoded;
    int result;
    size_t bytes;

    cover->ready = false;
    cover->issue_index = issue_index;
    if (issue_index < 0 || issue_index >= app.issue_count)
        return false;
    rb->snprintf(path, sizeof(path), "%s/%s/cover.jpg",
                 MAG_ROOT, app.issues[issue_index].directory);
    rb->memset(&decoded, 0, sizeof(decoded));
    decoded.width = MAG_COVER_W;
    decoded.height = MAG_COVER_H;
    decoded.data = app.fit_decode;
    result = rb->read_jpeg_file(path, &decoded, app.fit_decode_size,
                                FORMAT_NATIVE | FORMAT_RESIZE |
                                FORMAT_KEEP_ASPECT, NULL);
    if (result < 1 || decoded.width < 1 || decoded.height < 1 ||
        decoded.width > MAG_COVER_W || decoded.height > MAG_COVER_H)
        return false;
    bytes = BM_SIZE(decoded.width, decoded.height, FORMAT_NATIVE, false);
    rb->memcpy(cover->pixels, decoded.data, bytes);
    cover->bitmap = decoded;
    cover->bitmap.data = (unsigned char *)cover->pixels;
    cover->ready = true;
    return true;
}

static void mag_load_shelf_covers(void)
{
    int i;

    for (i = 0; i < MAG_VISIBLE_COVERS; i++)
        mag_load_cover(&app.covers[i],
                       mag_issue_index_at(app.shelf_start + i));
}

static void mag_draw_header(void)
{
    int level = rb->battery_level();
    int fill_width;
    fb_data fill;

    mag_gradient(0, 12, MAG_HEADER_TOP, MAG_HEADER_MID);
    mag_gradient(12, 12, MAG_HEADER_MID, MAG_HEADER_BOTTOM);
    rb->lcd_set_foreground(LCD_RGBPACK(104, 108, 112));
    rb->lcd_hline(0, LCD_WIDTH - 1, 23);
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_set_background(MAG_HEADER_BOTTOM);
    mag_puts_fit(42, 5, LCD_WIDTH - 84, "Magazines", true);
    if (rb->audio_status())
    {
        rb->lcd_vline(270, 8, 16);
        rb->lcd_vline(271, 9, 15);
        rb->lcd_vline(272, 10, 14);
        rb->lcd_vline(273, 11, 13);
        rb->lcd_vline(274, 12, 12);
    }
    rb->lcd_set_drawmode(DRMODE_SOLID);
    level = MAX(0, MIN(100, level));
    fill_width = 22 * MAX(15, level) / 100;
    fill = (level > 20 || rb->charger_inserted()) ?
           LCD_RGBPACK(165, 224, 127) : LCD_RGBPACK(209, 127, 107);
    rb->lcd_set_foreground(LCD_RGBPACK(84, 88, 91));
    rb->lcd_fillrect(282, 6, 22, 10);
    rb->lcd_set_foreground(fill);
    rb->lcd_fillrect(282, 6, fill_width, 10);
    rb->lcd_set_foreground(LCD_RGBPACK(98, 98, 98));
    rb->lcd_drawrect(281, 5, 24, 12);
    rb->lcd_set_foreground(LCD_RGBPACK(196, 196, 196));
    rb->lcd_fillrect(305, 8, 3, 6);
}

static void mag_draw_placeholder(int x, int y, const char *title)
{
    rb->lcd_set_foreground(MAG_PAPER);
    rb->lcd_fillrect(x, y, MAG_COVER_W, MAG_COVER_H);
    rb->lcd_set_foreground(MAG_WOOD_DARK);
    rb->lcd_drawrect(x, y, MAG_COVER_W, MAG_COVER_H);
    rb->lcd_set_background(MAG_PAPER);
    mag_puts_fit(x + 4, y + 34, MAG_COVER_W - 8, title, true);
}

static void mag_draw_progress(const struct magazine_issue *issue,
                              int x, int y, int width)
{
    int amount;

    if (issue->page_count <= 1)
        amount = issue->completed ? width : 0;
    else
        amount = (issue->saved_page - 1) * width / (issue->page_count - 1);
    rb->lcd_set_foreground(MAG_WOOD_DARK);
    rb->lcd_fillrect(x, y, width, 2);
    rb->lcd_set_foreground(issue->completed ? LCD_RGBPACK(76, 190, 96) :
                                             MAG_ACCENT);
    rb->lcd_fillrect(x, y, amount, 2);
}

static void mag_draw_shelf(void)
{
    static const int centers[3] = { 56, 160, 264 };
    int i;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    mag_gradient(24, LCD_HEIGHT - 24, MAG_WOOD_LIGHT, MAG_WOOD);
    rb->lcd_set_foreground(MAG_WOOD_GLOW);
    rb->lcd_hline(8, LCD_WIDTH - 9, 130);
    rb->lcd_set_foreground(MAG_WOOD_DARK);
    rb->lcd_hline(8, LCD_WIDTH - 9, 131);
    mag_draw_header();

    if (app.visible_count == 0)
    {
        rb->lcd_set_foreground(MAG_PAPER);
        rb->lcd_set_background(MAG_WOOD);
        mag_puts_fit(15, 82, LCD_WIDTH - 30, "No magazines installed", true);
        mag_puts_fit(15, 101, LCD_WIDTH - 30,
                     "Use tools/magazine_prepare.py", true);
        rb->lcd_update();
        return;
    }

    for (i = 0; i < MAG_VISIBLE_COVERS; i++)
    {
        int visible_index = app.shelf_start + i;
        int issue_index = mag_issue_index_at(visible_index);
        int row = i / 3;
        int col = i % 3;
        int center_x = app.visible_count == 1 ? centers[1] : centers[col];
        int x = center_x - MAG_COVER_W / 2;
        int y = row == 0 ? 27 : 133;
        bool selected = visible_index == app.selected;
        struct magazine_cover *cover = &app.covers[i];

        if (issue_index < 0)
            continue;
        if (selected)
            y -= 3;
        rb->lcd_set_foreground(LCD_RGBPACK(55, 35, 24));
        rb->lcd_fillrect(x + 4, y + 4, MAG_COVER_W, MAG_COVER_H);
        if (cover->ready && cover->issue_index == issue_index)
        {
            int image_x = x + (MAG_COVER_W - cover->bitmap.width) / 2;
            int image_y = y + (MAG_COVER_H - cover->bitmap.height) / 2;
            rb->lcd_bitmap((const fb_data *)cover->bitmap.data,
                           image_x, image_y,
                           cover->bitmap.width, cover->bitmap.height);
        }
        else
            mag_draw_placeholder(x, y, app.issues[issue_index].title);

        rb->lcd_set_foreground(selected ? MAG_ACCENT : MAG_WOOD_DARK);
        rb->lcd_drawrect(x - (selected ? 2 : 1),
                         y - (selected ? 2 : 1),
                         MAG_COVER_W + (selected ? 4 : 2),
                         MAG_COVER_H + (selected ? 4 : 2));
        rb->lcd_set_foreground(MAG_PAPER);
        rb->lcd_set_background(MAG_WOOD);
        rb->lcd_set_drawmode(DRMODE_FG);
        mag_puts_fit(center_x - 49, row == 0 ? 116 : 222,
                     98, app.issues[issue_index].title, true);
        rb->lcd_set_drawmode(DRMODE_SOLID);
        mag_draw_progress(&app.issues[issue_index],
                          center_x - 34, row == 0 ? 127 : 236, 68);
    }
    rb->lcd_update();
}

static void mag_draw_categories(void)
{
    int first;
    int row;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    mag_gradient(24, LCD_HEIGHT - 24, MAG_WOOD, MAG_WOOD_DARK);
    mag_draw_header();
    rb->lcd_setfont(app.category_font >= 0 ?
                    app.category_font : FONT_UI);
    rb->lcd_set_foreground(LCD_RGBPACK(255, 244, 218));
    rb->lcd_set_background(MAG_WOOD);
    rb->lcd_set_drawmode(DRMODE_FG);
    if (app.category_locked_view)
    {
        mag_puts_fit(14, 31, LCD_WIDTH - 28,
                     "Locked categories", false);
    }
    else
    {
        rb->lcd_set_foreground(
            app.selection_bookmarks ?
            LCD_RGBPACK(255, 252, 242) : LCD_RGBPACK(255, 221, 120));
        mag_puts_fit(17, 31, 128, "Categories", false);
        rb->lcd_set_foreground(
            app.selection_bookmarks ?
            LCD_RGBPACK(255, 221, 120) : LCD_RGBPACK(255, 252, 242));
        mag_puts_fit(167, 31, 136, "Bookmarks", false);
        rb->lcd_set_foreground(LCD_RGBPACK(255, 221, 120));
        if (app.selection_bookmarks)
            rb->lcd_hline(167, 286, 51);
        else
            rb->lcd_hline(17, 128, 51);
    }
    if (app.selection_bookmarks && !app.category_locked_view)
    {
        int count = mag_visible_bookmark_count();

        if (count <= 0)
        {
            rb->lcd_set_foreground(LCD_RGBPACK(255, 252, 242));
            mag_puts_fit(20, 93, LCD_WIDTH - 40,
                         "No saved bookmarks", true);
            rb->lcd_set_drawmode(DRMODE_SOLID);
            rb->lcd_setfont(FONT_UI);
            rb->lcd_update();
            return;
        }
        app.bookmark_selected = MIN(app.bookmark_selected, count - 1);
        first = (app.bookmark_selected / 6) * 6;
        for (row = 0; row < 6; row++)
        {
            int position = first + row;
            int issue_index;
            int bookmark_index;
            int page;
            int end;
            int y = 58 + row * 29;
            char range[24];

            if (position >= count ||
                !mag_bookmark_at(position, &issue_index, &bookmark_index))
                break;
            page = app.issues[issue_index].bookmarks[bookmark_index];
            end = mag_issue_spread_end(&app.issues[issue_index], page);
            if (end != page)
                rb->snprintf(range, sizeof(range), "%d-%d", page, end);
            else
                rb->snprintf(range, sizeof(range), "%d", page);
            rb->lcd_set_foreground(
                position == app.bookmark_selected ?
                LCD_RGBPACK(255, 221, 120) : LCD_RGBPACK(255, 252, 242));
            mag_puts_fit(17, y + 1, 215,
                         app.issues[issue_index].title, false);
            mag_puts_fit(250, y + 1, 52, range, true);
        }
        rb->lcd_set_drawmode(DRMODE_SOLID);
        rb->lcd_setfont(FONT_UI);
        rb->lcd_update();
        return;
    }
    first = (app.category_selected / 6) * 6;
    for (row = 0; row < 6; row++)
    {
        int position = first + row;
        int index;
        int y = 58 + row * 29;
        char count[24];
        const char *name;
        int issue_count;

        if (position >= app.category_visible_count)
            break;
        index = app.category_visible[position];
        if (index < 0)
        {
            name = "Locked:";
            issue_count = 0;
            for (index = 0; index < app.category_count; index++)
                issue_count += app.categories[index].locked_count;
        }
        else
        {
            name = app.categories[index].name;
            issue_count = app.category_locked_view ?
                          app.categories[index].locked_count :
                          app.categories[index].unlocked_count;
        }
        rb->lcd_set_foreground(
            position == app.category_selected ?
            LCD_RGBPACK(255, 221, 120) : LCD_RGBPACK(255, 252, 242));
        mag_puts_fit(17, y + 1, 215, name, false);
        rb->snprintf(count, sizeof(count), "%d", issue_count);
        mag_puts_fit(260, y + 1, 40, count, true);
    }
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_UI);
    rb->lcd_update();
}

static void mag_delete_selected_bookmark(void)
{
    int issue_index;
    int bookmark_index;
    int page;
    int end;
    int count;
    char prompt[64];

    if (!mag_bookmark_at(app.bookmark_selected,
                         &issue_index, &bookmark_index))
        return;
    page = app.issues[issue_index].bookmarks[bookmark_index];
    end = mag_issue_spread_end(&app.issues[issue_index], page);
    if (end != page)
        rb->snprintf(prompt, sizeof(prompt),
                     "Delete bookmark for pages %d-%d?", page, end);
    else
        rb->snprintf(prompt, sizeof(prompt),
                     "Delete bookmark for page %d?", page);
    if (!rb->yesno_pop_confirm(prompt))
    {
        mag_draw_categories();
        return;
    }
    mag_toggle_bookmark(&app.issues[issue_index], page);
    mag_save_state();
    count = mag_visible_bookmark_count();
    app.bookmark_selected =
        MIN(app.bookmark_selected, MAX(0, count - 1));
    rb->splash(HZ / 2, "Bookmark deleted");
    mag_draw_categories();
}

static int mag_category_loop(void)
{
#ifdef MAG_IPOD_CONTROLS
    static const struct button_mapping *contexts[] = { magazines_context };
#else
    static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif

    if (app.category_visible_count <= 0 &&
        mag_visible_bookmark_count() <= 0)
        return -1;
    mag_draw_categories();
    while (true)
    {
        int action = pluginlib_getaction(TIMEOUT_BLOCK, contexts,
                                         ARRAYLEN(contexts));

        switch (action)
        {
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                if (app.selection_bookmarks &&
                    !app.category_locked_view)
                    app.bookmark_selected =
                        MAX(0, app.bookmark_selected - 1);
                else
                    app.category_selected =
                        MAX(0, app.category_selected - 1);
                mag_draw_categories();
                break;
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                if (app.selection_bookmarks &&
                    !app.category_locked_view)
                    app.bookmark_selected =
                        MIN(MAX(0, mag_visible_bookmark_count() - 1),
                            app.bookmark_selected + 1);
                else
                    app.category_selected =
                        MIN(app.category_visible_count - 1,
                            app.category_selected + 1);
                mag_draw_categories();
                break;
            case PLA_LEFT:
            case PLA_LEFT_REPEAT:
                if (!app.category_locked_view)
                    app.selection_bookmarks = false;
                else
                    app.category_selected =
                        MAX(0, app.category_selected - 1);
                mag_draw_categories();
                break;
            case PLA_RIGHT:
            case PLA_RIGHT_REPEAT:
                if (!app.category_locked_view)
                    app.selection_bookmarks = true;
                else
                    app.category_selected =
                        MIN(app.category_visible_count - 1,
                            app.category_selected + 1);
                mag_draw_categories();
                break;
            case PLA_SELECT:
            case PLA_SELECT_REL:
                if (app.selection_bookmarks &&
                    !app.category_locked_view)
                {
                    if (mag_visible_bookmark_count() > 0)
                    {
                        int held = mag_select_hold();

                        if (held < 0)
                            return -2;
                        if (held)
                        {
                            mag_delete_selected_bookmark();
                            break;
                        }
                        return -4;
                    }
                    break;
                }
                mag_settle_select();
                if (app.category_visible[app.category_selected] < 0)
                    return -3;
                return app.category_visible[app.category_selected];
            case PLA_CANCEL:
            case PLA_EXIT:
                return -1;
        }
        if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
            return -2;
    }
}

static bool mag_read_pin(char pin[5])
{
    char line[16];
    char *value;
    int fd = rb->open(MAG_LOCK_PIN, O_RDONLY);

    if (fd < 0)
        return false;
    if (rb->read_line(fd, line, sizeof(line)) <= 0)
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    value = mag_trim(line);
    if (rb->strlen(value) != 4 ||
        value[0] < '0' || value[0] > '9' ||
        value[1] < '0' || value[1] > '9' ||
        value[2] < '0' || value[2] > '9' ||
        value[3] < '0' || value[3] > '9')
        return false;
    rb->strlcpy(pin, value, 5);
    return true;
}

static bool mag_load_pin_surfaces(void)
{
    return ipodjs_retailos_prepare_pin(&app.pin);
}

static void mag_draw_pin_surface(
    const struct ipodjs_retailos_image *image, int border,
    int x, int y, int width, int height)
{
    if (image == &app.pin.panel)
        ipodjs_retailos_draw_nine_slice(rb->screens[SCREEN_MAIN],
            image, x, y, width, height, border);
    else
        ipodjs_retailos_draw_parts(rb->screens[SCREEN_MAIN],
            image, x, y, width, height);
}

static void mag_draw_pin_prompt(const char *pin, int digit)
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
    char masked[MAG_PIN_LEN + 1];
    int title_w;
    int text_h;
    int masked_w;
    int length = rb->strlen(pin);
    int i;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_clear_display();
    rb->lcd_setfont(FONT_UI);
    rb->lcd_getstringsize((const unsigned char *)"Unlock Magazines",
                          &title_w, &text_h);
    rb->lcd_putsxy(MAX(4, (LCD_WIDTH - title_w) / 2), 38,
                   (const unsigned char *)"Unlock Magazines");
    rb->lcd_putsxy(42, 78,
        (const unsigned char *)"Scroll to choose, Select to enter");

    mag_draw_pin_surface(&app.pin.panel, 16,
                         panel_x, panel_y, panel_w, panel_h);
    mag_draw_pin_surface(app.pin.field, 6,
                         field_x, field_y, field_w, field_h);
    for (i = 0; i < length && i < MAG_PIN_LEN; i++)
        masked[i] = '*';
    masked[MIN(length, MAG_PIN_LEN)] = '\0';
    rb->lcd_getstringsize((const unsigned char *)masked,
                          &masked_w, &text_h);
    rb->lcd_set_foreground(LCD_RGBPACK(20, 24, 27));
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy(MAX(field_x + 4,
                       field_x + field_w - masked_w - 4),
                   field_y + MAX(0, (field_h - text_h) / 2),
                   (const unsigned char *)masked);
    for (i = -5; i <= 6; i++)
    {
        int index = (digit + i + 20) % 10;
        int x = center_x + i * 19;
        char glyph[2] = { digits[index], '\0' };
        int glyph_w;
        int glyph_h;

        if (x < field_x + field_w + 9)
            continue;
        rb->lcd_getstringsize((const unsigned char *)glyph,
                              &glyph_w, &glyph_h);
        if (i == 0)
        {
            int selected_w = MAX(glyph_w + 6, 16);
            int selected_x = x - (selected_w - glyph_w) / 2;

            mag_draw_pin_surface(app.pin.selected, 6,
                                 selected_x, panel_y + 10,
                                 selected_w, text_h + 3);
            rb->lcd_set_foreground(LCD_WHITE);
        }
        else
            rb->lcd_set_foreground(LCD_RGBPACK(239, 244, 246));
        rb->lcd_set_drawmode(DRMODE_FG);
        rb->lcd_putsxy(x, panel_y + 11,
                       (const unsigned char *)glyph);
    }
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_update();
}

static int mag_prompt_pin(void)
{
#ifdef MAG_IPOD_CONTROLS
    static const struct button_mapping *contexts[] = { magazines_context };
#else
    static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif
    char expected[5];
    char entered[5] = "";
    int digit = 0;

    if (!mag_read_pin(expected))
    {
        rb->splash(HZ * 2, "Set a PIN in RockPod Magazine Sync");
        return 0;
    }
    if (!mag_load_pin_surfaces())
    {
        rb->splash(HZ * 2, "iPod code-entry assets unavailable");
        return 0;
    }
    while (true)
    {
        size_t length;
        int action;

        mag_draw_pin_prompt(entered, digit);
        action = pluginlib_getaction(TIMEOUT_BLOCK, contexts,
                                     ARRAYLEN(contexts));
        switch (action)
        {
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                digit = (digit + 9) % 10;
                break;
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                digit = (digit + 1) % 10;
                break;
            case PLA_SELECT:
            case PLA_SELECT_REL:
                length = rb->strlen(entered);
                if (length < MAG_PIN_LEN)
                {
                    entered[length] = '0' + digit;
                    entered[length + 1] = '\0';
                }
                if (length + 1 == MAG_PIN_LEN)
                {
                    if (!rb->strcmp(entered, expected))
                    {
                        app.locks_unlocked = true;
                        mag_settle_select();
                        return 1;
                    }
                    rb->splash(HZ * 2, "Wrong code");
                    return 0;
                }
                break;
            case PLA_LEFT:
            case PLA_LEFT_REPEAT:
                length = rb->strlen(entered);
                if (length > 0)
                    entered[length - 1] = '\0';
                break;
            case PLA_CANCEL:
            case PLA_EXIT:
                return 0;
        }
        if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
            return -1;
    }
}

static void mag_page_path(char *path, size_t size, int number)
{
    rb->snprintf(path, size, "%s/%s/pages/%04d.jpg",
                 MAG_ROOT, app.reading->directory, number);
}

static bool mag_load_page(struct magazine_page *page, int number)
{
    page->ready = false;
    if (!app.reading || number < 1 || number > app.reading->page_count)
        return false;
    page->number = mag_spread_start(number);
    if (!mag_load_spread_bitmap(
            page->number, &page->bitmap, page->pixels,
            MAG_FIT_W, MAG_FIT_H, app.fit_decode, app.fit_decode_size))
        return false;
    page->ready = true;
    return true;
}

static void mag_compose_fill(fb_data color)
{
    size_t count = LCD_WIDTH * LCD_HEIGHT;
    size_t i;

    for (i = 0; i < count; i++)
        app.compose[i] = color;
}

static void mag_compose_bitmap(const struct bitmap *bitmap, int x, int y)
{
    const fb_data *source = (const fb_data *)bitmap->data;
    int row;

    for (row = 0; row < bitmap->height; row++)
    {
        if (y + row < 0 || y + row >= LCD_HEIGHT)
            continue;
        if (x >= 0 && x + bitmap->width <= LCD_WIDTH)
        {
            rb->memcpy(&app.compose[(y + row) * LCD_WIDTH + x],
                       &source[row * bitmap->width],
                       bitmap->width * sizeof(fb_data));
        }
    }
}

static fb_data mag_shade(fb_data pixel, int keep)
{
    return FB_RGBPACK(FB_UNPACK_RED(pixel) * keep / 256,
                      FB_UNPACK_GREEN(pixel) * keep / 256,
                      FB_UNPACK_BLUE(pixel) * keep / 256);
}

static fb_data mag_lighten(fb_data pixel, int amount)
{
    int red = FB_UNPACK_RED(pixel);
    int green = FB_UNPACK_GREEN(pixel);
    int blue = FB_UNPACK_BLUE(pixel);

    return FB_RGBPACK(red + (255 - red) * amount / 256,
                      green + (255 - green) * amount / 256,
                      blue + (255 - blue) * amount / 256);
}

static void mag_compose_turn(const struct magazine_page *source,
                             const struct magazine_page *destination,
                             int progress, bool forward)
{
    const fb_data *src = (const fb_data *)source->bitmap.data;
    int source_x = (LCD_WIDTH - source->bitmap.width) / 2;
    int source_y = (LCD_HEIGHT - source->bitmap.height) / 2;
    int dest_x = (LCD_WIDTH - destination->bitmap.width) / 2;
    int dest_y = (LCD_HEIGHT - destination->bitmap.height) / 2;
    int edge;
    int hidden;
    int curl;
    int shadow;
    int row;
    int x;

    mag_compose_fill(FB_SCALARPACK(MAG_READER_BG));
    if (progress <= 0)
    {
        mag_compose_bitmap(&source->bitmap, source_x, source_y);
        return;
    }
    mag_compose_bitmap(&destination->bitmap, dest_x, dest_y);
    if (progress >= 256)
        return;

    progress = progress * progress * (768 - progress * 2) / (256 * 256);
    if (forward)
    {
        edge = source->bitmap.width * (256 - progress) / 256;
        hidden = source->bitmap.width - edge;
    }
    else
    {
        edge = source->bitmap.width * progress / 256;
        hidden = edge;
    }
    curl = MIN(hidden,
               10 + 40 * MIN(progress, 256 - progress) / 128);
    shadow = MIN(18, 5 + curl / 3);

    for (x = 0; x < shadow; x++)
    {
        int display_x = source_x +
            (forward ? edge + x : edge - 1 - x);
        int keep = 116 + 128 * x / MAX(1, shadow - 1);

        if (display_x < 0 || display_x >= LCD_WIDTH)
            continue;
        for (row = 0; row < source->bitmap.height; row++)
        {
            int display_y = source_y + row;

            if (display_y >= 0 && display_y < LCD_HEIGHT)
                app.compose[display_y * LCD_WIDTH + display_x] =
                    mag_shade(
                        app.compose[display_y * LCD_WIDTH + display_x],
                        keep);
        }
    }

    for (row = 0; row < source->bitmap.height; row++)
    {
        int display_y = source_y + row;
        if (display_y < 0 || display_y >= LCD_HEIGHT)
            continue;
        if (forward)
        {
            for (x = 0; x < edge; x++)
                app.compose[display_y * LCD_WIDTH + source_x + x] =
                    src[row * source->bitmap.width + x];
        }
        else
        {
            for (x = edge; x < source->bitmap.width; x++)
                app.compose[display_y * LCD_WIDTH + source_x + x] =
                    src[row * source->bitmap.width + x];
        }
    }

    if (curl > 0)
    {
        for (x = 0; x < curl; x++)
        {
            int phase = x * 256 / MAX(1, curl - 1);
            int curve = phase * phase / 256;
            int source_column;
            int display_x;
            int inset = 5 * 4 * x * (curl - 1 - x) /
                        MAX(1, (curl - 1) * (curl - 1));
            int display_height = MAX(1, source->bitmap.height - inset * 2);
            int keep = 236 - 92 * phase / 256;
            int distance = phase > 78 ? phase - 78 : 78 - phase;
            int highlight = MAX(0, 46 - distance / 2);

            if (forward)
            {
                display_x = source_x + edge + x;
                source_column = MIN(source->bitmap.width - 1,
                                    edge + (hidden - 1) * curve / 256);
            }
            else
            {
                display_x = source_x + edge - 1 - x;
                source_column = MAX(0, edge - 1 -
                                       (hidden - 1) * curve / 256);
            }
            if (display_x < 0 || display_x >= LCD_WIDTH)
                continue;
            for (row = 0; row < display_height; row++)
            {
                int display_y = source_y + inset + row;
                int source_row = row * source->bitmap.height /
                                 display_height;
                fb_data pixel;

                if (display_y < 0 || display_y >= LCD_HEIGHT)
                    continue;
                pixel = src[source_row * source->bitmap.width +
                            source_column];
                pixel = mag_shade(pixel, keep);
                if (highlight > 0)
                    pixel = mag_lighten(pixel, highlight);
                app.compose[display_y * LCD_WIDTH + display_x] = pixel;
            }
        }
    }

    if (mag_spread_end(destination->number) != destination->number)
    {
        int spine = dest_x + destination->bitmap.width / 2;

        for (x = -1; x <= 1; x++)
        {
            int display_x = spine + x;
            int keep = x == 0 ? 188 : 222;

            if (display_x < 0 || display_x >= LCD_WIDTH)
                continue;
            for (row = 0; row < destination->bitmap.height; row++)
            {
                int display_y = dest_y + row;

                if (display_y >= 0 && display_y < LCD_HEIGHT)
                    app.compose[display_y * LCD_WIDTH + display_x] =
                        mag_shade(
                            app.compose[display_y * LCD_WIDTH + display_x],
                            keep);
            }
        }
    }
}

static void mag_draw_page(const struct magazine_page *page)
{
    int x;
    int y;

    rb->lcd_set_background(MAG_READER_BG);
    rb->lcd_clear_display();
    if (!page->ready)
    {
        rb->lcd_set_foreground(LCD_WHITE);
        mag_puts_fit(10, 106, LCD_WIDTH - 20, "Page image unavailable", true);
        rb->lcd_update();
        return;
    }
    x = (LCD_WIDTH - page->bitmap.width) / 2;
    y = (LCD_HEIGHT - page->bitmap.height) / 2;
    rb->lcd_bitmap((const fb_data *)page->bitmap.data,
                   x, y, page->bitmap.width, page->bitmap.height);
    rb->lcd_update();
}

static void mag_reader_chrome(void)
{
    char page[40];

    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 23);
    rb->lcd_fillrect(0, LCD_HEIGHT - 18, LCD_WIDTH, 18);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    mag_puts_fit(8, 5, LCD_WIDTH - 16, app.reading->title, true);
    if (mag_spread_end(app.current.number) != app.current.number)
        rb->snprintf(page, sizeof(page), "Pages %d-%d of %d",
                     app.current.number, mag_spread_end(app.current.number),
                     app.reading->page_count);
    else
        rb->snprintf(page, sizeof(page), "Page %d of %d",
                     app.current.number, app.reading->page_count);
    mag_puts_fit(8, LCD_HEIGHT - 15, LCD_WIDTH - 16, page, true);
    rb->lcd_update();
}

static void mag_draw_scrubber(int target)
{
    char page[48];
    int bar_x = 12;
    int bar_y = 204;
    int bar_w = LCD_WIDTH - 24;
    int amount;
    int percent;
    int i;
    bool bookmarked = mag_bookmark_index(app.reading, target) >= 0;

    mag_draw_page(&app.current);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(0, 174, LCD_WIDTH, LCD_HEIGHT - 174);
    percent = app.reading->page_count <= 1 ? 100 :
              (target - 1) * 100 / (app.reading->page_count - 1);
    if (mag_spread_end(target) != target)
        rb->snprintf(page, sizeof(page), "Selected: %d-%d of %d  %d%%",
                     target, mag_spread_end(target),
                     app.reading->page_count, percent);
    else
        rb->snprintf(page, sizeof(page), "Selected: %d of %d  %d%%",
                     target, app.reading->page_count, percent);
    rb->lcd_set_foreground(MAG_ACCENT);
    rb->lcd_fillrect(8, 176, LCD_WIDTH - 16, 23);
    if (bookmarked)
    {
        rb->lcd_set_foreground(LCD_RGBPACK(255, 196, 45));
        rb->lcd_drawrect(8, 176, LCD_WIDTH - 16, 23);
    }
    rb->lcd_set_background(MAG_ACCENT);
    rb->lcd_set_foreground(LCD_WHITE);
    mag_puts_fit(12, 180, LCD_WIDTH - 24, page, true);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_DARKGRAY);
    rb->lcd_fillrect(bar_x, bar_y, bar_w, 3);
    amount = app.reading->page_count <= 1 ? bar_w :
             (target - 1) * bar_w / (app.reading->page_count - 1);
    rb->lcd_set_foreground(MAG_ACCENT);
    rb->lcd_fillrect(bar_x, bar_y, amount, 3);
    for (i = 0; i < app.reading->bookmark_count; i++)
    {
        int marker = app.reading->page_count <= 1 ? 0 :
            (app.reading->bookmarks[i] - 1) * (bar_w - 1) /
            (app.reading->page_count - 1);

        rb->lcd_set_foreground(LCD_RGBPACK(255, 196, 45));
        rb->lcd_vline(bar_x + marker, bar_y - 2, bar_y + 4);
    }
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_fillrect(bar_x + MIN(amount, bar_w - 2) - 1,
                     bar_y - 2, 3, 7);
    rb->lcd_set_foreground(bookmarked ?
                          LCD_RGBPACK(255, 196, 45) : LCD_LIGHTGRAY);
    mag_puts_fit(6, 217, LCD_WIDTH - 12,
                 bookmarked ?
                 "Bookmarked  Play: remove" :
                 "Wheel: choose  Play: bookmark", true);
    rb->lcd_update();
}

static enum plugin_status mag_animate_turn(bool forward)
{
    const struct magazine_page *destination = forward ? &app.next :
                                                        &app.previous;
    long start = *rb->current_tick;
    int frame;

    for (frame = 0; frame <= MAG_TURN_FRAMES; frame++)
    {
        int progress = frame * 256 / MAG_TURN_FRAMES;
        long target = start + (long)MAG_TURN_TICKS * frame /
                              MAG_TURN_FRAMES;
        int button;

        mag_compose_turn(&app.current, destination, progress, forward);
        rb->lcd_bitmap(app.compose, 0, 0, LCD_WIDTH, LCD_HEIGHT);
        rb->lcd_update();
        while (TIME_BEFORE(*rb->current_tick, target))
        {
            button = rb->button_get_w_tmo(1);
            if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
                return PLUGIN_USB_CONNECTED;
        }
    }
    return PLUGIN_OK;
}

static bool mag_turn_page(bool forward, enum plugin_status *status)
{
    struct magazine_page old;
    int wanted = mag_adjacent_spread(app.current.number, forward);

    if (wanted == 0)
    {
        mag_reader_chrome();
        rb->sleep(HZ / 8);
        mag_draw_page(&app.current);
        return false;
    }

    if (forward && (!app.next.ready || app.next.number != wanted))
        mag_load_page(&app.next, wanted);
    if (!forward && (!app.previous.ready || app.previous.number != wanted))
        mag_load_page(&app.previous, wanted);
    if ((forward && !app.next.ready) || (!forward && !app.previous.ready))
    {
        mag_reader_chrome();
        return false;
    }

    *status = mag_animate_turn(forward);
    if (*status != PLUGIN_OK)
        return false;

    if (forward)
    {
        old = app.previous;
        app.previous = app.current;
        app.current = app.next;
        app.next = old;
        app.next.ready = false;
    }
    else
    {
        old = app.next;
        app.next = app.current;
        app.current = app.previous;
        app.previous = old;
        app.previous.ready = false;
    }
    app.reading->saved_page = app.current.number;
    app.reading->completed =
        app.current.number == app.reading->page_count;
    app.save_deadline = *rb->current_tick + MAG_SAVE_SETTLE;
    app.state_dirty = true;
    mag_draw_page(&app.current);
    /* A simulator keypress and a quick click-wheel sweep can enqueue several
     * wheel pulses during the 200 ms animation. One gesture owns one turn. */
    rb->button_clear_queue();

    wanted = mag_adjacent_spread(app.current.number, forward);
    if (wanted)
    {
        if (forward)
            mag_load_page(&app.next, wanted);
        else
            mag_load_page(&app.previous, wanted);
    }
    return true;
}

static bool mag_jump_to_spread(int target)
{
    struct magazine_page old;
    int adjacent;

    target = mag_spread_start(target);
    if (target == app.current.number)
        return true;
    if (!mag_load_page(&app.next, target))
        return false;
    old = app.current;
    app.current = app.next;
    app.next = old;
    app.previous.ready = false;
    app.next.ready = false;
    adjacent = mag_adjacent_spread(target, false);
    if (adjacent)
        mag_load_page(&app.previous, adjacent);
    adjacent = mag_adjacent_spread(target, true);
    if (adjacent)
        mag_load_page(&app.next, adjacent);
    app.reading->saved_page = target;
    app.reading->completed = target == app.reading->page_count;
    app.save_deadline = *rb->current_tick + MAG_SAVE_SETTLE;
    app.state_dirty = true;
    return true;
}

static int mag_scrub_step(int page, bool forward, int count)
{
    int next;

    while (count-- > 0)
    {
        next = mag_adjacent_spread(page, forward);
        if (!next)
            break;
        page = next;
    }
    return page;
}

static enum plugin_status mag_scrub_loop(void)
{
#ifdef MAG_IPOD_CONTROLS
    static const struct button_mapping *contexts[] = { magazines_context };
#else
    static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif
    int target = app.current.number;

    rb->button_clear_queue();
    mag_draw_scrubber(target);
    while (true)
    {
        int action = pluginlib_getaction(TIMEOUT_BLOCK, contexts,
                                         ARRAYLEN(contexts));

        switch (action)
        {
            case PLA_SCROLL_FWD:
                target = mag_scrub_step(target, true, 1);
                break;
            case PLA_SCROLL_FWD_REPEAT:
                target = mag_scrub_step(target, true, 5);
                break;
            case PLA_SCROLL_BACK:
                target = mag_scrub_step(target, false, 1);
                break;
            case PLA_SCROLL_BACK_REPEAT:
                target = mag_scrub_step(target, false, 5);
                break;
            case PLA_LEFT:
            case PLA_LEFT_REPEAT:
                target = mag_near_bookmark(app.reading, target, false);
                break;
            case PLA_RIGHT:
            case PLA_RIGHT_REPEAT:
                target = mag_near_bookmark(app.reading, target, true);
                break;
            case PLA_UP:
            {
                int old_count = app.reading->bookmark_count;
                bool existed =
                    mag_bookmark_index(app.reading, target) >= 0;
                bool added = mag_toggle_bookmark(app.reading, target);

                if (!existed &&
                    app.reading->bookmark_count == old_count)
                    rb->splash(HZ, "Bookmark limit reached");
                else
                {
                    mag_save_state();
                    rb->splash(HZ / 2, added ?
                               "Bookmark saved" : "Bookmark removed");
                }
                break;
            }
            case PLA_SELECT:
            case PLA_SELECT_REL:
                if (!mag_jump_to_spread(target))
                    rb->splash(HZ, "Page image unavailable");
                mag_settle_select();
                mag_draw_page(&app.current);
                return PLUGIN_OK;
            case PLA_CANCEL:
                rb->button_clear_queue();
                mag_draw_page(&app.current);
                return PLUGIN_OK;
            case PLA_EXIT:
                return PLUGIN_OK;
        }
        mag_draw_scrubber(target);
        if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
    }
}

static void mag_zoom_dimensions(int *width, int *height)
{
    int fit_width;
    int fit_height;

    if (app.zoom.width * LCD_HEIGHT > app.zoom.height * LCD_WIDTH)
    {
        fit_width = LCD_WIDTH;
        fit_height = MAX(1, app.zoom.height * LCD_WIDTH /
                            MAX(1, app.zoom.width));
    }
    else
    {
        fit_height = LCD_HEIGHT;
        fit_width = MAX(1, app.zoom.width * LCD_HEIGHT /
                           MAX(1, app.zoom.height));
    }
    *width = MAX(1, fit_width * app.zoom_percent / 100);
    *height = MAX(1, fit_height * app.zoom_percent / 100);
}

static void mag_clamp_zoom(void)
{
    int width;
    int height;

    mag_zoom_dimensions(&width, &height);
    app.zoom_x = MAX(0, MIN(app.zoom_x, MAX(0, width - LCD_WIDTH)));
    app.zoom_y = MAX(0, MIN(app.zoom_y, MAX(0, height - LCD_HEIGHT)));
}

static bool mag_load_zoom(int page)
{
    char path[MAX_PATH];
    struct bitmap decoded;
    int width;
    int height;

    mag_page_path(path, sizeof(path), page);
    if (!mag_decode_bounded(
            path, &decoded, MAG_ZOOM_SOURCE_W, MAG_ZOOM_SOURCE_H,
            app.zoom_decode, app.zoom_decode_size))
    {
        app.zoom_ready = false;
        return false;
    }
    app.zoom = decoded;
    app.zoom_percent = MAG_ZOOM_START_PERCENT;
    mag_zoom_dimensions(&width, &height);
    app.zoom_x = MAX(0, (width - LCD_WIDTH) / 2);
    app.zoom_y = 0;
    app.zoom_page = page;
    app.zoom_ready = true;
    return true;
}

static bool mag_change_zoom(int amount)
{
    int old_width;
    int old_height;
    int new_width;
    int new_height;
    int center_x;
    int center_y;
    int wanted = MAX(MAG_ZOOM_MIN_PERCENT,
                     MIN(MAG_ZOOM_MAX_PERCENT,
                         app.zoom_percent + amount));

    if (wanted == app.zoom_percent)
        return false;
    mag_zoom_dimensions(&old_width, &old_height);
    center_x = old_width <= LCD_WIDTH ?
               old_width / 2 : app.zoom_x + LCD_WIDTH / 2;
    center_y = old_height <= LCD_HEIGHT ?
               old_height / 2 : app.zoom_y + LCD_HEIGHT / 2;
    app.zoom_percent = wanted;
    mag_zoom_dimensions(&new_width, &new_height);
    app.zoom_x = center_x * new_width / MAX(1, old_width) -
                 LCD_WIDTH / 2;
    app.zoom_y = center_y * new_height / MAX(1, old_height) -
                 LCD_HEIGHT / 2;
    mag_clamp_zoom();
    return true;
}

static void mag_render_zoom(int virtual_width, int virtual_height)
{
    const fb_data *source = (const fb_data *)app.zoom.data;
    int draw_width = MIN(LCD_WIDTH, virtual_width);
    int draw_height = MIN(LCD_HEIGHT, virtual_height);
    int dest_x = virtual_width < LCD_WIDTH ?
                 (LCD_WIDTH - virtual_width) / 2 : 0;
    int dest_y = virtual_height < LCD_HEIGHT ?
                 (LCD_HEIGHT - virtual_height) / 2 : 0;
    int row;

    mag_compose_fill(FB_SCALARPACK(MAG_READER_BG));
    for (row = 0; row < draw_height; row++)
    {
        int virtual_y = app.zoom_y + row;
        int source_y = MIN(
            app.zoom.height - 1,
            (int)((unsigned long)virtual_y * app.zoom.height /
                  MAX(1, virtual_height)));
        int column;

        for (column = 0; column < draw_width; column++)
        {
            int virtual_x = app.zoom_x + column;
            int source_x = MIN(
                app.zoom.width - 1,
                (int)((unsigned long)virtual_x * app.zoom.width /
                      MAX(1, virtual_width)));

            app.compose[(dest_y + row) * LCD_WIDTH + dest_x + column] =
                source[source_y * app.zoom.width + source_x];
        }
    }
}

static void mag_draw_zoom(bool map)
{
    int virtual_width;
    int virtual_height;

    rb->lcd_set_background(MAG_READER_BG);
    rb->lcd_clear_display();
    if (!app.zoom_ready)
    {
        rb->lcd_set_foreground(LCD_WHITE);
        mag_puts_fit(10, 106, LCD_WIDTH - 20, "Zoom unavailable", true);
        rb->lcd_update();
        return;
    }
    mag_zoom_dimensions(&virtual_width, &virtual_height);
    mag_render_zoom(virtual_width, virtual_height);
    rb->lcd_bitmap(app.compose, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    if (map && (virtual_width > LCD_WIDTH ||
                virtual_height > LCD_HEIGHT))
    {
        int map_w = 48;
        int map_h = 64;
        int map_x = LCD_WIDTH - map_w - 7;
        int view_w = virtual_width <= LCD_WIDTH ? map_w :
                     MAX(4, map_w * LCD_WIDTH / virtual_width);
        int view_h = virtual_height <= LCD_HEIGHT ? map_h :
                     MAX(4, map_h * LCD_HEIGHT / virtual_height);
        int view_x = map_x;
        int view_y = 7;
        char level[8];
        int level_w;
        int level_h;
        int level_x;
        int level_y = map_h + 12;

        if (virtual_width > LCD_WIDTH)
            view_x += app.zoom_x * (map_w - view_w) /
                      MAX(1, virtual_width - LCD_WIDTH);
        if (virtual_height > LCD_HEIGHT)
            view_y += app.zoom_y * (map_h - view_h) /
                      MAX(1, virtual_height - LCD_HEIGHT);
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_fillrect(LCD_WIDTH - map_w - 9, 5, map_w + 4, map_h + 4);
        rb->lcd_set_foreground(MAG_PAPER);
        rb->lcd_drawrect(map_x, 7, map_w, map_h);
        rb->lcd_set_foreground(MAG_ACCENT);
        rb->lcd_drawrect(view_x, view_y, view_w, view_h);
        rb->snprintf(level, sizeof(level), "%d%%", app.zoom_percent);
        rb->lcd_getstringsize((const unsigned char *)level,
                              &level_w, &level_h);
        level_x = LCD_WIDTH - level_w - 9;
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_fillrect(level_x - 2, level_y - 1,
                         level_w + 4, level_h + 2);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_putsxy(level_x, level_y,
                       (const unsigned char *)level);
    }
    rb->lcd_update();
}

static enum plugin_status mag_zoom_loop(void)
{
#ifdef MAG_IPOD_CONTROLS
    static const struct button_mapping *contexts[] = {
        magazines_zoom_context
    };
#else
    static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif

    mag_draw_zoom(true);
    while (true)
    {
        int action = pluginlib_getaction(TIMEOUT_BLOCK, contexts,
                                         ARRAYLEN(contexts));
        int old_x = app.zoom_x;
        int old_y = app.zoom_y;
        int pan = 24;

#ifdef MAG_IPOD_CONTROLS
        if ((rb->button_status() & (BUTTON_MENU | BUTTON_SELECT)) ==
            (BUTTON_MENU | BUTTON_SELECT))
        {
            while (rb->button_status() & (BUTTON_MENU | BUTTON_SELECT))
                rb->sleep(1);
            rb->button_clear_queue();
            app.zoom_ready = false;
            mag_draw_page(&app.current);
            return PLUGIN_OK;
        }
#endif

        switch (action)
        {
            case PLA_SCROLL_FWD:
                mag_change_zoom(MAG_ZOOM_STEP);
                break;
            case PLA_SCROLL_FWD_REPEAT:
                mag_change_zoom(MAG_ZOOM_REPEAT_STEP);
                break;
            case PLA_SCROLL_BACK:
                mag_change_zoom(-MAG_ZOOM_STEP);
                break;
            case PLA_SCROLL_BACK_REPEAT:
                mag_change_zoom(-MAG_ZOOM_REPEAT_STEP);
                break;
            case PLA_UP_REPEAT:
                pan = 64;
                /* Fall through. */
            case PLA_UP:
                app.zoom_y -= pan;
                break;
            case PLA_DOWN_REPEAT:
                pan = 64;
                /* Fall through. */
            case PLA_DOWN:
                app.zoom_y += pan;
                break;
            case PLA_RIGHT_REPEAT:
                pan = 64;
                /* Fall through. */
            case PLA_RIGHT:
                app.zoom_x += pan;
                break;
            case PLA_LEFT_REPEAT:
                pan = 64;
                /* Fall through. */
            case PLA_LEFT:
                app.zoom_x -= pan;
                break;
            case PLA_CANCEL:
                app.zoom_ready = false;
                mag_draw_page(&app.current);
                return PLUGIN_OK;
            case PLA_EXIT:
                return PLUGIN_OK;
        }
        mag_clamp_zoom();
        if (app.zoom_x != old_x || app.zoom_y != old_y ||
            action == PLA_SCROLL_FWD ||
            action == PLA_SCROLL_FWD_REPEAT ||
            action == PLA_SCROLL_BACK ||
            action == PLA_SCROLL_BACK_REPEAT)
        {
            mag_draw_zoom(true);
        }
        if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
    }
}

static enum plugin_status mag_open_zoom(int page)
{
    enum plugin_status status;

    rb->button_clear_queue();
    rb->splash(0, "Loading zoom...");
    app.zoom_ready = false;
    if (!mag_load_zoom(page))
    {
        rb->splash(HZ, "Zoom unavailable");
        mag_draw_page(&app.current);
        return PLUGIN_OK;
    }
    mag_settle_select();
    status = mag_zoom_loop();
    if (status == PLUGIN_OK)
        mag_draw_page(&app.current);
    return status;
}

static enum plugin_status mag_reader_loop(struct magazine_issue *issue,
                                          int start_page)
{
#ifdef MAG_IPOD_CONTROLS
    static const struct button_mapping *contexts[] = { magazines_context };
#else
    static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif
    enum plugin_status status = PLUGIN_OK;
    int page = start_page > 0 ? start_page : issue->saved_page;
    int adjacent;

    page = MAX(1, MIN(page, issue->page_count));
    app.reading = issue;
    page = mag_spread_start(page);
    app.previous.ready = false;
    app.current.ready = false;
    app.next.ready = false;
    rb->splash(0, "Opening magazine...");
    if (!mag_load_page(&app.current, page))
    {
        rb->splash(HZ * 2, "Page image unavailable");
        app.reading = NULL;
        return PLUGIN_OK;
    }
    adjacent = mag_adjacent_spread(page, false);
    if (adjacent)
        mag_load_page(&app.previous, adjacent);
    adjacent = mag_adjacent_spread(page, true);
    if (adjacent)
        mag_load_page(&app.next, adjacent);
    issue->opened = ++app.open_sequence;
    rb->strlcpy(app.last_issue, issue->id, sizeof(app.last_issue));
    app.state_dirty = true;
    mag_draw_page(&app.current);

    while (true)
    {
        int action = pluginlib_getaction(HZ / 10, contexts,
                                         ARRAYLEN(contexts));

        switch (action)
        {
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                mag_turn_page(true, &status);
                if (status != PLUGIN_OK)
                    goto out;
                break;
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                mag_turn_page(false, &status);
                if (status != PLUGIN_OK)
                    goto out;
                break;
            case PLA_LEFT:
            case PLA_LEFT_REPEAT:
                status = mag_open_zoom(app.current.number);
                if (status != PLUGIN_OK)
                    goto out;
                break;
            case PLA_RIGHT:
            case PLA_RIGHT_REPEAT:
                status = mag_open_zoom(mag_spread_end(app.current.number));
                if (status != PLUGIN_OK)
                    goto out;
                break;
            case PLA_SELECT:
            case PLA_SELECT_REL:
                status = mag_open_zoom(app.current.number);
                if (status != PLUGIN_OK)
                    goto out;
                break;
            case PLA_UP:
                status = mag_scrub_loop();
                if (status != PLUGIN_OK)
                    goto out;
                break;
            case PLA_CANCEL:
            case PLA_EXIT:
                goto out;
        }
        if (app.state_dirty && app.save_deadline &&
            TIME_AFTER(*rb->current_tick, app.save_deadline))
        {
            mag_save_state();
            app.save_deadline = 0;
        }
        if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
        {
            status = PLUGIN_USB_CONNECTED;
            goto out;
        }
    }

out:
    issue->saved_page = app.current.number;
    app.state_dirty = true;
    mag_save_state();
    app.reading = NULL;
    app.zoom_ready = false;
    return status;
}

static void mag_move_selection(int delta)
{
    int old_start = app.shelf_start;

    if (app.visible_count <= 0)
        return;
    app.selected = MAX(0, MIN(app.selected + delta,
                              app.visible_count - 1));
    app.shelf_start = (app.selected / MAG_VISIBLE_COVERS) *
                      MAG_VISIBLE_COVERS;
    if (old_start != app.shelf_start)
        mag_load_shelf_covers();
}

static enum plugin_status mag_shelf_loop(void)
{
#ifdef MAG_IPOD_CONTROLS
    static const struct button_mapping *contexts[] = { magazines_context };
#else
    static const struct button_mapping *contexts[] = { pla_main_ctx };
#endif

    mag_load_shelf_covers();
    mag_draw_shelf();
    while (true)
    {
        int action = pluginlib_getaction(TIMEOUT_BLOCK, contexts,
                                         ARRAYLEN(contexts));

        switch (action)
        {
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
            case PLA_LEFT:
            case PLA_LEFT_REPEAT:
                mag_move_selection(-1);
                mag_draw_shelf();
                break;
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
            case PLA_RIGHT:
            case PLA_RIGHT_REPEAT:
                mag_move_selection(1);
                mag_draw_shelf();
                break;
            case PLA_SELECT:
            case PLA_SELECT_REL:
                if (app.issue_count > 0)
                {
                    enum plugin_status result =
                        PLUGIN_OK;
                    int issue_index = mag_issue_index_at(app.selected);
                    int pin_result;

                    mag_settle_select();
                    if (issue_index < 0)
                        break;
                    if (app.issues[issue_index].locked &&
                        !app.locks_unlocked)
                    {
                        pin_result = mag_prompt_pin();
                        if (pin_result < 0)
                            return PLUGIN_USB_CONNECTED;
                        if (!pin_result)
                        {
                            mag_draw_shelf();
                            break;
                        }
                    }
                    result = mag_reader_loop(&app.issues[issue_index], 0);
                    if (result != PLUGIN_OK)
                        return result;
                    mag_sort_issues();
                    mag_load_shelf_covers();
                    mag_draw_shelf();
                }
                break;
            case PLA_UP:
                app.sort_recent = !app.sort_recent;
                app.state_dirty = true;
                mag_sort_issues();
                mag_load_shelf_covers();
                mag_draw_shelf();
                break;
            case PLA_CANCEL:
            case PLA_EXIT:
                mag_save_state();
                app.back_to_categories = true;
                return PLUGIN_OK;
        }
        if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
        {
            mag_save_state();
            return PLUGIN_USB_CONNECTED;
        }
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status status;

    (void)parameter;
    if (!mag_allocate())
    {
        rb->splash(HZ * 2, "Not enough plugin memory");
        return PLUGIN_ERROR;
    }

    rb->viewportmanager_theme_enable(SCREEN_MAIN, false, NULL);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_setfont(FONT_UI);
    backlight_ignore_timeout();
    rb->button_clear_queue();
    app.category_font = rb->font_load(
        FONT_DIR "/18-Adobe-Helvetica-Bold.fnt");

    mag_load_catalog();
    mag_load_state();
    mag_sort_issues();
    status = PLUGIN_OK;
    while (app.category_count > 0)
    {
        int category;

        mag_refresh_category_view(false);
        category = mag_category_loop();
        if (category == -2)
        {
            status = PLUGIN_USB_CONNECTED;
            break;
        }
        if (category == -1)
            break;
        if (category == -4)
        {
            int issue_index;
            int bookmark_index;

            if (mag_bookmark_at(app.bookmark_selected,
                                &issue_index, &bookmark_index))
            {
                char issue_id[MAG_DIR_LEN];
                int page =
                    app.issues[issue_index].bookmarks[bookmark_index];

                rb->strlcpy(issue_id, app.issues[issue_index].id,
                            sizeof(issue_id));
                status = mag_reader_loop(&app.issues[issue_index], page);
                if (status != PLUGIN_OK)
                    break;
                mag_sort_issues();
                app.bookmark_selected =
                    mag_find_bookmark_position(issue_id, page);
            }
            continue;
        }
        if (category == -3)
        {
            int pin_result;

            if (!app.locks_unlocked)
            {
                pin_result = mag_prompt_pin();
                if (pin_result < 0)
                {
                    status = PLUGIN_USB_CONNECTED;
                    break;
                }
                if (!pin_result)
                    continue;
            }
            app.category_selected = 0;
            while (true)
            {
                mag_refresh_category_view(true);
                category = mag_category_loop();
                if (category == -2)
                {
                    status = PLUGIN_USB_CONNECTED;
                    break;
                }
                if (category < 0)
                    break;
                mag_select_category(category, app.last_issue, true);
                app.back_to_categories = false;
                status = mag_shelf_loop();
                if (status != PLUGIN_OK)
                    break;
            }
            if (status != PLUGIN_OK)
                break;
            app.category_selected = 0;
            continue;
        }
        mag_select_category(category, app.last_issue, false);
        app.back_to_categories = false;
        status = mag_shelf_loop();
        if (status != PLUGIN_OK || !app.back_to_categories)
            break;
    }
    if (app.category_count == 0)
    {
        mag_select_category(-1, NULL, false);
        status = mag_shelf_loop();
    }
    mag_save_state();

    backlight_use_settings();
    if (app.category_font >= 0)
        rb->font_unload(app.category_font);
    rb->viewportmanager_theme_undo(SCREEN_MAIN, false);
    rb->lcd_setfont(FONT_UI);
    return status;
}
