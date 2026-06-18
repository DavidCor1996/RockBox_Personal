#include "config.h"

#if defined(HAVE_TAGCACHE) && defined(HAVE_LCD_COLOR)

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "string-extra.h"
#include "albumlist_art.h"
#include "file.h"
#include "icons.h"
#include "lcd.h"
#include "misc.h"
#include "screen_access.h"
#include "system.h"
#include "tagtree.h"
#include "tree.h"
#include "viewport.h"
#include "recorder/bmp.h"

#define ALBUMLIST_INDEX ROCKBOX_DIR "/albumlist/index.tsv"
#define ALBUMLIST_ROOT ROCKBOX_DIR "/albumlist"
#define ALBUMLIST_THUMB_SIZE 32
#define ALBUMLIST_TEXT_PAD 6
#define ALBUMLIST_LOOKUP_CACHE 16
#define ALBUMLIST_BITMAP_CACHE 8
#define ALBUMLIST_ALBUM_LEN 96
#define ALBUMLIST_ARTIST_LEN 80
#define ALBUMLIST_PATH_LEN 96

struct albumlist_lookup_slot {
    bool valid;
    bool found;
    char album[ALBUMLIST_ALBUM_LEN];
    char artist[ALBUMLIST_ARTIST_LEN];
    char path[ALBUMLIST_PATH_LEN];
};

struct albumlist_bitmap_slot {
    bool valid;
    unsigned long last_used;
    char path[ALBUMLIST_PATH_LEN];
    struct bitmap bm;
    unsigned char data[BM_SIZE(ALBUMLIST_THUMB_SIZE, ALBUMLIST_THUMB_SIZE,
                               FORMAT_NATIVE, false)];
};

static struct albumlist_lookup_slot lookup_cache[ALBUMLIST_LOOKUP_CACHE];
static int lookup_victim;
static struct albumlist_bitmap_slot bitmap_cache[ALBUMLIST_BITMAP_CACHE];
static unsigned long bitmap_tick;

static int ascii_casecmp(const char *a, const char *b)
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

static void trim_line(char *line)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
        line[--len] = '\0';
}

static bool split_tsv(char *line, char *fields[], int field_count)
{
    for (int i = 0; i < field_count; i++)
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

static bool manifest_entry_matches(const char *album, const char *artist,
                                   char *fields[])
{
    const char *row_artist = fields[2];
    const char *row_album = fields[3];

    if (!row_album[0] || ascii_casecmp(row_album, album) != 0)
        return false;

    if (!artist[0])
        return true;

    return row_artist[0] && ascii_casecmp(row_artist, artist) == 0;
}

static bool find_manifest_thumb(const char *album, const char *artist,
                                char *path, size_t path_size)
{
    int fd = open(ALBUMLIST_INDEX, O_RDONLY);
    if (fd < 0)
        return false;

    char line[512];
    bool found = false;
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        trim_line(line);
        if (line[0] == '#' || line[0] == '\0' ||
            strncmp(line, "album_id\t", 9) == 0)
            continue;

        char *fields[6];
        if (!split_tsv(line, fields, 6) || !fields[1][0])
            continue;

        if (manifest_entry_matches(album, artist, fields))
        {
            snprintf(path, path_size, "%s/%s", ALBUMLIST_ROOT, fields[1]);
            found = true;
            break;
        }
    }

    close(fd);
    return found;
}

static bool lookup_thumb_path(const char *album, const char *artist,
                              char *path, size_t path_size)
{
    for (int i = 0; i < ALBUMLIST_LOOKUP_CACHE; i++)
    {
        struct albumlist_lookup_slot *slot = &lookup_cache[i];
        if (!slot->valid)
            continue;
        if (ascii_casecmp(slot->album, album) == 0 &&
            ascii_casecmp(slot->artist, artist) == 0)
        {
            if (slot->found)
                strmemccpy(path, slot->path, path_size);
            return slot->found;
        }
    }

    struct albumlist_lookup_slot *slot = &lookup_cache[lookup_victim];
    lookup_victim = (lookup_victim + 1) % ALBUMLIST_LOOKUP_CACHE;
    slot->valid = true;
    strmemccpy(slot->album, album, sizeof(slot->album));
    strmemccpy(slot->artist, artist, sizeof(slot->artist));
    slot->found = find_manifest_thumb(album, artist, slot->path,
                                      sizeof(slot->path));
    if (slot->found)
        strmemccpy(path, slot->path, path_size);
    return slot->found;
}

static struct albumlist_bitmap_slot *bitmap_cache_victim(void)
{
    int victim = 0;
    unsigned long oldest = bitmap_cache[0].last_used;

    for (int i = 0; i < ALBUMLIST_BITMAP_CACHE; i++)
    {
        if (!bitmap_cache[i].valid)
            return &bitmap_cache[i];
        if (bitmap_cache[i].last_used < oldest)
        {
            oldest = bitmap_cache[i].last_used;
            victim = i;
        }
    }

    return &bitmap_cache[victim];
}

static struct bitmap *load_thumb_bitmap(const char *path)
{
    for (int i = 0; i < ALBUMLIST_BITMAP_CACHE; i++)
    {
        struct albumlist_bitmap_slot *slot = &bitmap_cache[i];
        if (slot->valid && strcmp(slot->path, path) == 0)
        {
            slot->last_used = ++bitmap_tick;
            return &slot->bm;
        }
    }

    struct albumlist_bitmap_slot *slot = bitmap_cache_victim();
    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->bm.width = ALBUMLIST_THUMB_SIZE;
    slot->bm.height = ALBUMLIST_THUMB_SIZE;
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
    slot->last_used = ++bitmap_tick;
    strmemccpy(slot->path, path, sizeof(slot->path));
    return &slot->bm;
}

void albumlist_art_setup_list(struct gui_synclist *list)
{
    struct tree_context *tc = list ? (struct tree_context *)list->data : NULL;
    char album[ALBUMLIST_ALBUM_LEN];
    char artist[ALBUMLIST_ARTIST_LEN];

    if (!list || !tc ||
        !tagtree_get_album_art_row(tc, tc->special_entry_count,
                                   album, sizeof(album),
                                   artist, sizeof(artist)))
        return;

    list->callback_draw_item = albumlist_art_draw_item;
    FOR_NB_SCREENS(i)
    {
        if (list->line_height[i] < ALBUMLIST_THUMB_SIZE + 2)
            list->line_height[i] = ALBUMLIST_THUMB_SIZE + 2;
    }
}

void albumlist_art_draw_item(struct list_putlineinfo_t *list_info)
{
    if (!list_info || list_info->is_title)
    {
        gui_list_draw_item_default(list_info);
        return;
    }

    struct tree_context *tc = (struct tree_context *)list_info->list->data;
    char album[ALBUMLIST_ALBUM_LEN];
    char artist[ALBUMLIST_ARTIST_LEN];
    char path[ALBUMLIST_PATH_LEN];

    if (!tagtree_get_album_art_row(tc, list_info->line,
                                   album, sizeof(album),
                                   artist, sizeof(artist)) ||
        !lookup_thumb_path(album, artist, path, sizeof(path)))
    {
        gui_list_draw_item_default(list_info);
        return;
    }

    struct bitmap *bm = load_thumb_bitmap(path);
    if (!bm)
    {
        gui_list_draw_item_default(list_info);
        return;
    }

    struct list_putlineinfo_t text_info = *list_info;
    text_info.item_indent += ALBUMLIST_THUMB_SIZE + ALBUMLIST_TEXT_PAD;
    text_info.icon = Icon_NOICON;
    text_info.have_icons = false;
    gui_list_draw_item_default(&text_info);

    int x = list_info->item_indent + 1;
    int y = list_info->y + MAX(0, (list_info->linedes->height - bm->height) / 2);
    list_info->display->bmp_part(bm, 0, 0, x, y, bm->width, bm->height);
}

#endif
