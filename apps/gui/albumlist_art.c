#include "config.h"

#if defined(HAVE_TAGCACHE) && defined(HAVE_LCD_COLOR)

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "string-extra.h"
#include "albumlist_art.h"
#include "button.h"
#include "file.h"
#include "font.h"
#include "icons.h"
#include "lcd.h"
#include "misc.h"
#include "screen_access.h"
#include "settings.h"
#include "system.h"
#include "tagtree.h"
#include "tree.h"
#include "viewport.h"
#include "recorder/bmp.h"
#include "slideshow_order.h"
#ifdef HAVE_IPODJS_UI
#include "ipodjs_ui.h"
#endif

#define ALBUMLIST_INDEX ROCKBOX_DIR "/albumlist/index.tsv"
#define ALBUMLIST_ROOT ROCKBOX_DIR "/albumlist"
#define ALBUMLIST_PACK ROCKBOX_DIR "/albumlist/thumbs.pack"
#define ALBUMLIST_THUMB_SIZE 40
#define ALBUMLIST_COMPACT_THUMB_SIZE 24
#define ALBUMLIST_COMPACT_TEXT_PAD 5
#define ALBUMLIST_TEXT_PAD 8
#define ALBUMLIST_ROW_HEIGHT 44
#define ALBUMLIST_LOOKUP_CACHE 16
/* 64 slots x ~3.9 KB = ~250 KB BSS.  Sized so a long scroll through the
 * album list does not evict rows the user is about to scroll back to;
 * with thumbs.pack, reloads after eviction are one small read anyway. */
#define ALBUMLIST_BITMAP_CACHE 64
#define ALBUMLIST_PENDING_MAX 8
#define ALBUMLIST_MANIFEST_CACHE_MAX 384
#define ALBUMLIST_MANIFEST_RELOAD_DELAY (HZ * 300)
#define ALBUMLIST_ALBUM_LEN 96
#define ALBUMLIST_ARTIST_LEN 80
#define ALBUMLIST_PATH_LEN 256
#define ALBUMLIST_SCALE_SCRATCH_EXTRA(width) ((width) * (int)sizeof(uint32_t) * 4)
#define ALBUMLIST_SCALED_BYTES(width, height) \
    (BM_SCALED_SIZE(width, height, FORMAT_NATIVE, false) + \
     ALBUMLIST_SCALE_SCRATCH_EXTRA(width))

struct albumlist_lookup_slot {
    bool valid;
    bool found;
    int pack_row;
    char album[ALBUMLIST_ALBUM_LEN];
    char artist[ALBUMLIST_ARTIST_LEN];
    char path[ALBUMLIST_PATH_LEN];
};

struct albumlist_bitmap_slot {
    bool valid;
    int size;
    unsigned long last_used;
    char path[ALBUMLIST_PATH_LEN];
    struct bitmap bm;
    unsigned char data[ALBUMLIST_SCALED_BYTES(ALBUMLIST_THUMB_SIZE,
                                              ALBUMLIST_THUMB_SIZE)];
};

static struct albumlist_lookup_slot lookup_cache[ALBUMLIST_LOOKUP_CACHE];
static int lookup_victim;
static struct albumlist_bitmap_slot bitmap_cache[ALBUMLIST_BITMAP_CACHE];
static unsigned long bitmap_tick;

struct albumlist_pending_item {
    struct tree_context *tc;
    int line;
    int size;
    int pack_row;
    char album[ALBUMLIST_ALBUM_LEN];
    char artist[ALBUMLIST_ARTIST_LEN];
    char path[ALBUMLIST_PATH_LEN];
};

static struct albumlist_pending_item pending_items[ALBUMLIST_PENDING_MAX];
static int pending_count;

static void trim_line(char *line);
static int split_tsv(char *line, char *fields[], int max_fields);

struct albumlist_manifest_entry {
    int pack_row;
    char thumb_path[ALBUMLIST_PATH_LEN];
    char slide_path[ALBUMLIST_PATH_LEN];
    char album[ALBUMLIST_ALBUM_LEN];
    char artist[ALBUMLIST_ARTIST_LEN];
};

static struct albumlist_manifest_entry
    manifest_cache[ALBUMLIST_MANIFEST_CACHE_MAX];
static int manifest_cache_count;
static bool manifest_cache_loaded;
static long manifest_cache_next_reload;

/* thumbs.pack: raw RGB565 thumbnail records written by RockPod in
 * index.tsv data-line order.  One seek+read replaces the per-file BMP
 * open/parse/scale, which is what makes list art keep up with wheel
 * scrolling the way the stock firmware's packed artwork database does. */
#define ALBUMLIST_PACK_HEADER_SIZE 16
#define ALBUMLIST_PACK_RECORD_HEADER 4
#define ALBUMLIST_PACK_RECORD_SIZE \
    (ALBUMLIST_PACK_RECORD_HEADER + \
     ALBUMLIST_THUMB_SIZE * ALBUMLIST_THUMB_SIZE * 2)
static bool pack_valid;
static int pack_count;

static void albumlist_check_pack(void)
{
    unsigned char header[ALBUMLIST_PACK_HEADER_SIZE];
    int fd;

    pack_valid = false;
    pack_count = 0;

    fd = open(ALBUMLIST_PACK, O_RDONLY);
    if (fd < 0)
        return;

    if (read(fd, header, sizeof(header)) == (ssize_t)sizeof(header) &&
        memcmp(header, "ALTB", 4) == 0 &&
        (header[4] | (header[5] << 8)) == 1 &&
        (header[6] | (header[7] << 8)) == ALBUMLIST_THUMB_SIZE &&
        (header[8] | (header[9] << 8)) == ALBUMLIST_THUMB_SIZE)
    {
        pack_count = header[12] | (header[13] << 8) |
                     (header[14] << 16) | (header[15] << 24);
        pack_valid = pack_count > 0;
    }

    close(fd);
}

#if defined(IPOD_VIDEO) || defined(IPOD_6G)
static void albumlist_slideshow_invalidate_failures(void);
#endif

static bool albumlist_path_exists(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;

    close(fd);
    return true;
}

static bool albumlist_cover_path_for_dir(const char *dirs, char *path,
                                         size_t path_size)
{
    static const char * const cover_names[] = {
        "cover.240x240.bmp",
        "cover.160x160.bmp",
        "cover.138x138.bmp",
        "cover.bmp",
        "folder.bmp",
        "albumart.bmp",
    };
    char dir[ALBUMLIST_PATH_LEN];

    if (!dirs || !dirs[0])
        return false;

    strmemccpy(dir, dirs, sizeof(dir));
    char *sep = strchr(dir, '|');
    if (!sep)
        sep = strchr(dir, ';');
    if (sep)
        *sep = '\0';

    for (size_t i = 0; i < ARRAYLEN(cover_names); i++)
    {
        snprintf(path, path_size, "/%s/%s", dir, cover_names[i]);
        if (albumlist_path_exists(path))
            return true;
    }

    return false;
}

static bool albumlist_cover_path_for_track(const char *track_path, char *path,
                                           size_t path_size)
{
    static const char * const cover_names[] = {
        "cover.240x240.bmp",
        "cover.160x160.bmp",
        "cover.138x138.bmp",
        "cover.bmp",
        "folder.bmp",
        "albumart.bmp",
    };
    char dir[ALBUMLIST_PATH_LEN];
    char *slash;

    if (!track_path || !track_path[0])
        return false;

    strmemccpy(dir, track_path, sizeof(dir));
    slash = strrchr(dir, '/');
    if (!slash)
        return false;
    *slash = '\0';

    for (size_t i = 0; i < ARRAYLEN(cover_names); i++)
    {
        snprintf(path, path_size, "%s/%s", dir, cover_names[i]);
        if (albumlist_path_exists(path))
            return true;
    }

    return false;
}

static bool albumlist_slide_path_for_id(const char *album_id, char *path,
                                        size_t path_size)
{
    if (!album_id || !album_id[0])
        return false;

    snprintf(path, path_size, "%s/slides/%s.bmp", ALBUMLIST_ROOT, album_id);
    return albumlist_path_exists(path);
}

static void albumlist_invalidate_lookup_cache(void)
{
    for (int i = 0; i < ALBUMLIST_LOOKUP_CACHE; i++)
        lookup_cache[i].valid = false;
    lookup_victim = 0;
}

static void albumlist_manifest_path_join(const char *relative_path, char *path,
                                         size_t path_size)
{
    if (!relative_path || !relative_path[0])
    {
        if (path_size > 0)
            path[0] = '\0';
        return;
    }

    if (relative_path[0] == '/')
        strmemccpy(path, relative_path, path_size);
    else
        snprintf(path, path_size, "%s/%s", ALBUMLIST_ROOT, relative_path);
}

static void albumlist_load_manifest_cache(void)
{
    if (manifest_cache_loaded &&
        !TIME_AFTER(current_tick, manifest_cache_next_reload))
        return;

    manifest_cache_count = 0;
    manifest_cache_loaded = true;
    manifest_cache_next_reload = current_tick + ALBUMLIST_MANIFEST_RELOAD_DELAY;
    albumlist_invalidate_lookup_cache();
#if defined(IPOD_VIDEO) || defined(IPOD_6G)
    albumlist_slideshow_invalidate_failures();
#endif

    albumlist_check_pack();

    int fd = open(ALBUMLIST_INDEX, O_RDONLY);
    if (fd < 0)
        return;

    char line[512];
    int pack_row_counter = 0;
    while (manifest_cache_count < ALBUMLIST_MANIFEST_CACHE_MAX &&
           read_line(fd, line, sizeof(line)) > 0)
    {
        trim_line(line);
        if (line[0] == '#' || line[0] == '\0' ||
            strncmp(line, "album_id\t", 9) == 0)
            continue;

        /* Pack records align with data-line ordinals, including lines the
         * validation below rejects. */
        int pack_row = pack_row_counter++;

        char *fields[7] = {0};
        int field_count = split_tsv(line, fields, ARRAYLEN(fields));
        bool has_slide_field = field_count >= 7;
        int slide_field = has_slide_field ? 2 : -1;
        int artist_field = has_slide_field ? 3 : 2;
        int album_field = has_slide_field ? 4 : 3;
        int dirs_field = has_slide_field ? 6 : 5;

        if (field_count < 6 ||
            (!fields[1][0] &&
             !(has_slide_field && fields[slide_field][0]) &&
             !fields[dirs_field][0]))
            continue;

        struct albumlist_manifest_entry *entry =
            &manifest_cache[manifest_cache_count];
        memset(entry, 0, sizeof(*entry));
        entry->pack_row = pack_row;
        strmemccpy(entry->artist, fields[artist_field], sizeof(entry->artist));
        strmemccpy(entry->album, fields[album_field], sizeof(entry->album));

        if (fields[1][0])
            albumlist_manifest_path_join(fields[1], entry->thumb_path,
                                         sizeof(entry->thumb_path));

        if (has_slide_field && fields[slide_field][0])
        {
            albumlist_manifest_path_join(fields[slide_field], entry->slide_path,
                                         sizeof(entry->slide_path));
        }
        else if (!albumlist_slide_path_for_id(fields[0], entry->slide_path,
                                              sizeof(entry->slide_path)) &&
                 !albumlist_cover_path_for_dir(fields[dirs_field],
                                               entry->slide_path,
                                               sizeof(entry->slide_path)) &&
                 entry->thumb_path[0])
        {
            strmemccpy(entry->slide_path, entry->thumb_path,
                       sizeof(entry->slide_path));
        }

        if (entry->thumb_path[0] || entry->slide_path[0])
            manifest_cache_count++;
    }

    close(fd);
}

static int albumlist_manifest_count(void)
{
    albumlist_load_manifest_cache();
    return manifest_cache_count;
}

static struct albumlist_manifest_entry *albumlist_manifest_at(int index)
{
    albumlist_load_manifest_cache();
    if (index < 0 || index >= manifest_cache_count)
        return NULL;
    return &manifest_cache[index];
}

#if defined(IPOD_VIDEO) || defined(IPOD_6G)
#define ALBUMLIST_SLIDESHOW_SIZE 384
#define ALBUMLIST_SLIDESHOW_SHADOW_WIDTH 18
#define ALBUMLIST_SLIDESHOW_PAN_DURATION (HZ * 6)
#define ALBUMLIST_SLIDESHOW_HOLD_DURATION 0
#define ALBUMLIST_SLIDESHOW_PERIOD \
    (ALBUMLIST_SLIDESHOW_PAN_DURATION + ALBUMLIST_SLIDESHOW_HOLD_DURATION)
#define ALBUMLIST_SLIDESHOW_PAN_SCALE 1024
#define ALBUMLIST_SLIDESHOW_PREFETCH_PHASE \
    (ALBUMLIST_SLIDESHOW_PAN_DURATION / 2)
#define ALBUMLIST_SLIDESHOW_FAILURE_CACHE 8
#define ALBUMLIST_SLIDESHOW_FAILURE_RETRY_DELAY (HZ * 30)
#define ALBUMLIST_SLIDESHOW_BYTES \
    ALBUMLIST_SCALED_BYTES(ALBUMLIST_SLIDESHOW_SIZE, \
                           ALBUMLIST_SLIDESHOW_SIZE)

struct albumlist_slideshow_slot {
    bool valid;
    int manifest_index;
    char path[ALBUMLIST_PATH_LEN];
    struct bitmap bm;
    unsigned char data[ALBUMLIST_SLIDESHOW_BYTES];
};

struct albumlist_slideshow_failure {
    bool valid;
    int manifest_index;
    long retry_after;
    char path[ALBUMLIST_PATH_LEN];
};

static struct albumlist_slideshow_slot slideshow_slots[3];
static struct albumlist_slideshow_failure
    slideshow_failures[ALBUMLIST_SLIDESHOW_FAILURE_CACHE];
static int slideshow_slot_victim;
static int slideshow_last_drawn_index = -1;
static struct slideshow_order slideshow_random_order;
static bool slideshow_paused;
static long slideshow_paused_at;
static long slideshow_paused_total;

void albumlist_slideshow_set_paused(bool paused)
{
    if (paused == slideshow_paused)
        return;

    if (paused)
    {
        slideshow_paused_at = current_tick;
    }
    else
    {
        slideshow_paused_total += current_tick - slideshow_paused_at;
    }

    slideshow_paused = paused;
}

static long albumlist_slideshow_tick(void)
{
    long tick = slideshow_paused ? slideshow_paused_at : current_tick;
    return tick - slideshow_paused_total;
}

static int albumlist_random_manifest_index(long cycle, int entry_count)
{
    return slideshow_order_index(&slideshow_random_order, cycle, entry_count);
}

static bool albumlist_manifest_path_at(int wanted, char *path, size_t path_size)
{
    if (path_size > 0)
        path[0] = '\0';

    struct albumlist_manifest_entry *entry = albumlist_manifest_at(wanted);
    if (!entry || !entry->slide_path[0])
        return false;

    strmemccpy(path, entry->slide_path, path_size);
    return true;
}

static bool albumlist_slideshow_is_prerendered(const char *path)
{
    return path && strstr(path, ALBUMLIST_ROOT "/slides/") != NULL;
}

static void albumlist_slideshow_invalidate_failures(void)
{
    for (int i = 0; i < (int)ARRAYLEN(slideshow_failures); i++)
        slideshow_failures[i].valid = false;
}

static bool albumlist_slideshow_recent_failure(int index, const char *path)
{
    for (int i = 0; i < (int)ARRAYLEN(slideshow_failures); i++)
    {
        struct albumlist_slideshow_failure *failure = &slideshow_failures[i];
        if (!failure->valid ||
            failure->manifest_index != index ||
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

static void albumlist_slideshow_clear_failure(int index, const char *path)
{
    for (int i = 0; i < (int)ARRAYLEN(slideshow_failures); i++)
    {
        struct albumlist_slideshow_failure *failure = &slideshow_failures[i];
        if (failure->valid &&
            failure->manifest_index == index &&
            strcmp(failure->path, path) == 0)
        {
            failure->valid = false;
            return;
        }
    }
}

static void albumlist_slideshow_record_failure(int index, const char *path)
{
    int victim = 0;
    long oldest = slideshow_failures[0].retry_after;

    for (int i = 0; i < (int)ARRAYLEN(slideshow_failures); i++)
    {
        struct albumlist_slideshow_failure *failure = &slideshow_failures[i];
        if (!failure->valid ||
            (failure->manifest_index == index && strcmp(failure->path, path) == 0))
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

    struct albumlist_slideshow_failure *failure = &slideshow_failures[victim];
    failure->valid = true;
    failure->manifest_index = index;
    failure->retry_after = current_tick + ALBUMLIST_SLIDESHOW_FAILURE_RETRY_DELAY;
    strmemccpy(failure->path, path, sizeof(failure->path));
}

static bool albumlist_load_slideshow_slot(struct albumlist_slideshow_slot *slot,
                                          int index)
{
    char path[ALBUMLIST_PATH_LEN];

    if (slot->valid && slot->manifest_index == index)
        return true;

    if (!albumlist_manifest_path_at(index, path, sizeof(path)))
        return false;

    if (albumlist_slideshow_recent_failure(index, path))
        return false;

    if (slot->valid && strcmp(path, slot->path) == 0)
        return true;

    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->bm.width = ALBUMLIST_SLIDESHOW_SIZE;
    slot->bm.height = ALBUMLIST_SLIDESHOW_SIZE;
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;

    bool prerendered = albumlist_slideshow_is_prerendered(path);
    int format = FORMAT_NATIVE;
    if (!prerendered)
        format |= FORMAT_RESIZE | FORMAT_KEEP_ASPECT | FORMAT_DITHER;

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    cpu_boost(true);
#endif
    int rc = read_bmp_file(path, &slot->bm, sizeof(slot->data), format, NULL);
    if (rc < 0 && prerendered)
    {
        slot->bm.width = ALBUMLIST_SLIDESHOW_SIZE;
        slot->bm.height = ALBUMLIST_SLIDESHOW_SIZE;
        rc = read_bmp_file(path, &slot->bm, sizeof(slot->data),
                           FORMAT_NATIVE | FORMAT_RESIZE |
                           FORMAT_KEEP_ASPECT, NULL);
    }
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    cpu_boost(false);
#endif
    if (rc < 0)
    {
        slot->valid = false;
        slot->manifest_index = -1;
        slot->path[0] = '\0';
        albumlist_slideshow_record_failure(index, path);
        return false;
    }

    slot->manifest_index = index;
    strmemccpy(slot->path, path, sizeof(slot->path));
    slot->valid = true;
    albumlist_slideshow_clear_failure(index, path);
    return true;
}

static struct albumlist_slideshow_slot *albumlist_choose_slideshow_victim(
    int protected_index)
{
    for (int i = 0; i < (int)ARRAYLEN(slideshow_slots); i++)
    {
        if (!slideshow_slots[i].valid)
            return &slideshow_slots[i];
    }

    for (int attempts = 0; attempts < (int)ARRAYLEN(slideshow_slots); attempts++)
    {
        int candidate = slideshow_slot_victim;
        slideshow_slot_victim =
            (slideshow_slot_victim + 1) % (int)ARRAYLEN(slideshow_slots);

        if (slideshow_slots[candidate].manifest_index != protected_index)
            return &slideshow_slots[candidate];
    }

    return NULL;
}

static struct albumlist_slideshow_slot *albumlist_get_slideshow_slot(
    int index, int protected_index)
{
    for (int i = 0; i < (int)ARRAYLEN(slideshow_slots); i++)
    {
        if (slideshow_slots[i].valid &&
            slideshow_slots[i].manifest_index == index)
            return &slideshow_slots[i];
    }

    struct albumlist_slideshow_slot *slot =
        albumlist_choose_slideshow_victim(protected_index);
    if (!slot)
        return NULL;

    return albumlist_load_slideshow_slot(slot, index) ? slot : NULL;
}

static struct albumlist_slideshow_slot *albumlist_find_slideshow_slot(int index)
{
    for (int i = 0; i < (int)ARRAYLEN(slideshow_slots); i++)
    {
        if (slideshow_slots[i].valid &&
            slideshow_slots[i].manifest_index == index)
            return &slideshow_slots[i];
    }

    return NULL;
}

static void albumlist_prefetch_slideshow_slot(int index, int protected_index)
{
    (void)albumlist_get_slideshow_slot(index, protected_index);
}

static void albumlist_draw_slideshow_shadow(struct screen *display, int x, int y,
                                            int width, int height)
{
    static const unsigned char alpha[ALBUMLIST_SLIDESHOW_SHADOW_WIDTH] = {
        116, 106, 96, 86, 76, 66, 56, 47, 39,
        32, 25, 19, 14, 10, 7, 5, 3, 2,
    };
    (void)display;

    int draw_w = MIN(width, ALBUMLIST_SLIDESHOW_SHADOW_WIDTH);
    for (int yy = 0; yy < height; yy++)
    {
        for (int xx = 0; xx < draw_w; xx++)
        {
            fb_data *pixel = FBADDR(x + xx, y + yy);
            unsigned keep = 256 - alpha[xx];
            unsigned r = (FB_UNPACK_RED(*pixel) * keep) >> 8;
            unsigned g = (FB_UNPACK_GREEN(*pixel) * keep) >> 8;
            unsigned b = (FB_UNPACK_BLUE(*pixel) * keep) >> 8;
            *pixel = FB_RGBPACK(r, g, b);
        }
    }
}

static bool albumlist_draw_slideshow_slot(
    struct screen *display, struct albumlist_slideshow_slot *slot,
    long cycle, long phase, int x, int y, int width, int height)
{
    if (!display || !slot)
        return false;

    long pan_phase = MIN(phase, ALBUMLIST_SLIDESHOW_PAN_DURATION);
    long pan_pos = pan_phase * ALBUMLIST_SLIDESHOW_PAN_SCALE /
                   ALBUMLIST_SLIDESHOW_PAN_DURATION;
    int pan_range_x = MAX(0, slot->bm.width - width);
    int pan_range_y = MAX(0, slot->bm.height - height);
    int pan_mode = (int)(cycle % 6);
    long rev_pan_pos = ALBUMLIST_SLIDESHOW_PAN_SCALE - pan_pos;
    int pan_x = pan_range_x * pan_pos / ALBUMLIST_SLIDESHOW_PAN_SCALE;
    int pan_x_rev = pan_range_x * rev_pan_pos /
                    ALBUMLIST_SLIDESHOW_PAN_SCALE;
    int pan_y = pan_range_y * pan_pos / ALBUMLIST_SLIDESHOW_PAN_SCALE;
    int pan_y_rev = pan_range_y * rev_pan_pos /
                    ALBUMLIST_SLIDESHOW_PAN_SCALE;
    int src_x = pan_range_x / 2;
    int src_y = pan_range_y / 2;

    switch (pan_mode)
    {
        case 0: /* left to right */
            src_x = pan_x;
            break;
        case 1: /* right to left */
            src_x = pan_x_rev;
            break;
        case 2: /* top to bottom */
            src_y = pan_y;
            break;
        case 3: /* bottom to top */
            src_y = pan_y_rev;
            break;
        case 4: /* upper-left to lower-right */
            src_x = pan_x;
            src_y = pan_y;
            break;
        default: /* upper-right to lower-left */
            src_x = pan_x_rev;
            src_y = pan_y;
            break;
    }
    int draw_y = y + MAX(0, (height - slot->bm.height) / 2);
    int draw_w = MIN(width, slot->bm.width - src_x);
    int draw_h = MIN(height, slot->bm.height - src_y);

    if (draw_w > 0 && draw_h > 0)
    {
        if (draw_w < width || draw_h < height || draw_y > y)
        {
            display->set_background(LCD_RGBPACK(104, 110, 122));
            display->fillrect(x, y, width, height);
        }
        display->bmp_part(&slot->bm, src_x, src_y, x, draw_y,
                          draw_w, draw_h);
    }
    albumlist_draw_slideshow_shadow(display, x, y, width, height);
    slideshow_last_drawn_index = slot->manifest_index;
    return true;
}

static bool albumlist_slideshow_frame(int entry_count, long *cycle,
                                      long *phase, int *wanted,
                                      int *next_wanted)
{
    if (entry_count <= 0)
        return false;

    long tick = albumlist_slideshow_tick();
    *cycle = tick / ALBUMLIST_SLIDESHOW_PERIOD;
    *phase = tick % ALBUMLIST_SLIDESHOW_PERIOD;
    *wanted = albumlist_random_manifest_index(*cycle, entry_count);
    *next_wanted = albumlist_random_manifest_index(*cycle + 1, entry_count);
    return true;
}

bool albumlist_slideshow_service(void)
{
    long cycle, phase;
    int wanted, next_wanted;

    /* Artwork I/O is optional. Never contend with queued navigation or a
     * database transition; the cached frame remains available to the UI. */
    if (!button_queue_empty() || !tagcache_is_usable() ||
        tagcache_commit_active())
        return false;

    int entry_count = albumlist_manifest_count();
    if (!albumlist_slideshow_frame(entry_count, &cycle, &phase,
                                   &wanted, &next_wanted))
        return false;

    if (!albumlist_find_slideshow_slot(wanted))
        return albumlist_get_slideshow_slot(wanted, -1) != NULL;

    if (phase >= ALBUMLIST_SLIDESHOW_PREFETCH_PHASE &&
        !albumlist_find_slideshow_slot(next_wanted))
        albumlist_prefetch_slideshow_slot(next_wanted, wanted);

    return false;
}

bool albumlist_slideshow_frame_changed(void)
{
    long cycle, phase;
    int wanted, next_wanted;
    struct albumlist_slideshow_slot *slot;

    if (!manifest_cache_loaded ||
        !albumlist_slideshow_frame(manifest_cache_count, &cycle, &phase,
                                   &wanted, &next_wanted))
        return false;

    slot = albumlist_find_slideshow_slot(wanted);
    return slot && slot->manifest_index != slideshow_last_drawn_index;
}

bool albumlist_draw_slideshow_cached(struct screen *display, int x, int y,
                                     int width, int height)
{
    long cycle, phase;
    int wanted, next_wanted;
    struct albumlist_slideshow_slot *slot;

    /* Do not reload the manifest here: this function is intentionally safe
     * to call from a navigation redraw. */
    if (!display || !manifest_cache_loaded ||
        !albumlist_slideshow_frame(manifest_cache_count, &cycle, &phase,
                                   &wanted, &next_wanted))
        return false;

    slot = albumlist_find_slideshow_slot(wanted);
    if (!slot && slideshow_last_drawn_index >= 0)
        slot = albumlist_find_slideshow_slot(slideshow_last_drawn_index);

    return albumlist_draw_slideshow_slot(display, slot, cycle, phase,
                                         x, y, width, height);
}

bool albumlist_draw_slideshow(struct screen *display, int x, int y,
                              int width, int height)
{
    long cycle, phase;
    int wanted, next_wanted;

    if (!display)
        return false;

    int entry_count = albumlist_manifest_count();
    if (!albumlist_slideshow_frame(entry_count, &cycle, &phase,
                                   &wanted, &next_wanted))
        return false;

    struct albumlist_slideshow_slot *slot =
        albumlist_get_slideshow_slot(wanted, -1);
    bool drew = albumlist_draw_slideshow_slot(display, slot, cycle, phase,
                                              x, y, width, height);

    if (phase >= ALBUMLIST_SLIDESHOW_PREFETCH_PHASE)
        albumlist_prefetch_slideshow_slot(next_wanted, wanted);

    return drew;
}

#else
void albumlist_slideshow_set_paused(bool paused)
{
    (void)paused;
}

bool albumlist_slideshow_service(void)
{
    return false;
}

bool albumlist_slideshow_frame_changed(void)
{
    return false;
}

bool albumlist_draw_slideshow_cached(struct screen *display, int x, int y,
                                     int width, int height)
{
    (void)display;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return false;
}

bool albumlist_draw_slideshow(struct screen *display, int x, int y,
                              int width, int height)
{
    (void)display;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return false;
}

#endif

static bool albumlist_get_album_row(struct tree_context *tc, int id,
                                    char *album, size_t album_size,
                                    char *artist, size_t artist_size)
{
    if (!tc ||
        !tagtree_get_album_art_row(tc, id, album, album_size,
                                  artist, artist_size))
        return false;

    return album && album[0] != '\0';
}

static bool albumlist_has_album_rows(struct gui_synclist *list)
{
    struct tree_context *tc = list ? (struct tree_context *)list->data : NULL;
    char album[ALBUMLIST_ALBUM_LEN];
    char artist[ALBUMLIST_ARTIST_LEN];

    if (!list || !tc)
        return false;

    for (int i = tc->special_entry_count; i < list->nb_items; i++)
    {
        if (albumlist_get_album_row(tc, i, album, sizeof(album),
                                    artist, sizeof(artist)))
            return true;
    }

    return false;
}

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

static int split_tsv(char *line, char *fields[], int max_fields)
{
    int count = 0;

    while (count < max_fields)
    {
        fields[count++] = line;
        char *tab = strchr(line, '\t');
        if (tab)
        {
            *tab = '\0';
            line = tab + 1;
        }
        else
        {
            break;
        }
    }

    return count;
}

/* Load one raw RGB565 record from thumbs.pack into a cache slot.  The fd
 * is opened and closed within the call so the browser never holds one
 * across a yield. */
static bool albumlist_load_thumb_from_pack(struct albumlist_bitmap_slot *slot,
                                           int pack_row, int size)
{
    unsigned char rec[ALBUMLIST_PACK_RECORD_HEADER];
    int fd;
    bool ok = false;

    if (!pack_valid || pack_row < 0 || pack_row >= pack_count ||
        size != ALBUMLIST_THUMB_SIZE)
        return false;

    fd = open(ALBUMLIST_PACK, O_RDONLY);
    if (fd < 0)
        return false;

    if (lseek(fd, ALBUMLIST_PACK_HEADER_SIZE +
              (off_t)pack_row * ALBUMLIST_PACK_RECORD_SIZE, SEEK_SET) >= 0 &&
        read(fd, rec, sizeof(rec)) == (ssize_t)sizeof(rec) &&
        rec[0] == 1)
    {
        int w = rec[2];
        int h = rec[3];

        if (w > 0 && h > 0 && w <= ALBUMLIST_THUMB_SIZE &&
            h <= ALBUMLIST_THUMB_SIZE)
        {
            ssize_t bytes = (ssize_t)w * h * 2;

            if (read(fd, slot->data, bytes) == bytes)
            {
                memset(&slot->bm, 0, sizeof(slot->bm));
                slot->bm.width = w;
                slot->bm.height = h;
                slot->bm.format = FORMAT_NATIVE;
                slot->bm.data = slot->data;
                ok = true;
            }
        }
    }

    close(fd);
    return ok;
}

static bool manifest_entry_matches(const char *album, const char *artist,
                                   char *fields[])
{
    const char *row_artist = fields[2];
    const char *row_album = fields[3];

    if (!row_album[0] || ascii_casecmp(row_album, album) != 0)
        return false;

    if (!artist[0])
        return false;

    return row_artist[0] && ascii_casecmp(row_artist, artist) == 0;
}

static bool find_manifest_thumb(const char *album, const char *artist,
                                char *path, size_t path_size,
                                int *pack_row)
{
    albumlist_load_manifest_cache();
    *pack_row = -1;
    if (!artist[0])
    {
        int matches = 0;
        int candidate_row = -1;
        char candidate[ALBUMLIST_PATH_LEN];

        candidate[0] = '\0';
        for (int i = 0; i < manifest_cache_count; i++)
        {
            struct albumlist_manifest_entry *entry = &manifest_cache[i];
            if (!entry->thumb_path[0] || !entry->album[0] ||
                ascii_casecmp(entry->album, album) != 0)
                continue;

            matches++;
            if (matches == 1)
            {
                strmemccpy(candidate, entry->thumb_path, sizeof(candidate));
                candidate_row = entry->pack_row;
            }
            else
                return false;
        }

        if (matches == 1)
        {
            strmemccpy(path, candidate, path_size);
            *pack_row = candidate_row;
            return true;
        }

        return false;
    }

    for (int i = 0; i < manifest_cache_count; i++)
    {
        struct albumlist_manifest_entry *entry = &manifest_cache[i];
        char *fields[6] = {
            NULL,
            entry->thumb_path,
            entry->artist,
            entry->album,
            NULL,
            NULL,
        };

        if (!entry->thumb_path[0])
            continue;

        if (manifest_entry_matches(album, artist, fields))
        {
            strmemccpy(path, entry->thumb_path, path_size);
            *pack_row = entry->pack_row;
            return true;
        }
    }

    /* Tagcache album rows can expose albumartist while the artwork manifest
     * was indexed with the track artist (or vice versa).  If the strict
     * album+artist match misses, accept a unique album match so covers still
     * appear in both Albums and Artist -> Albums views. */
    int matches = 0;
    int candidate_row = -1;
    const char *candidate = NULL;
    for (int i = 0; i < manifest_cache_count; i++)
    {
        struct albumlist_manifest_entry *entry = &manifest_cache[i];
        if (!entry->thumb_path[0] ||
            ascii_casecmp(entry->album, album) != 0)
            continue;
        candidate = entry->thumb_path;
        candidate_row = entry->pack_row;
        if (++matches > 1)
            return false;
    }
    if (matches == 1)
    {
        strmemccpy(path, candidate, path_size);
        *pack_row = candidate_row;
        return true;
    }

    return false;
}

static bool lookup_thumb_path(const char *album, const char *artist,
                              char *path, size_t path_size,
                              int *pack_row)
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
            {
                strmemccpy(path, slot->path, path_size);
                *pack_row = slot->pack_row;
            }
            return slot->found;
        }
    }

    struct albumlist_lookup_slot *slot = &lookup_cache[lookup_victim];
    lookup_victim = (lookup_victim + 1) % ALBUMLIST_LOOKUP_CACHE;
    slot->valid = true;
    strmemccpy(slot->album, album, sizeof(slot->album));
    strmemccpy(slot->artist, artist, sizeof(slot->artist));
    slot->found = find_manifest_thumb(album, artist, slot->path,
                                      sizeof(slot->path), &slot->pack_row);
    if (slot->found)
    {
        strmemccpy(path, slot->path, path_size);
        *pack_row = slot->pack_row;
    }
    return slot->found;
}

static void cache_thumb_path(const char *album, const char *artist,
                             const char *path)
{
    struct albumlist_lookup_slot *slot = NULL;

    for (int i = 0; i < ALBUMLIST_LOOKUP_CACHE; i++)
    {
        if (lookup_cache[i].valid &&
            ascii_casecmp(lookup_cache[i].album, album) == 0 &&
            ascii_casecmp(lookup_cache[i].artist, artist) == 0)
        {
            slot = &lookup_cache[i];
            break;
        }
    }

    if (!slot)
    {
        slot = &lookup_cache[lookup_victim];
        lookup_victim = (lookup_victim + 1) % ALBUMLIST_LOOKUP_CACHE;
        slot->valid = true;
        strmemccpy(slot->album, album, sizeof(slot->album));
        strmemccpy(slot->artist, artist, sizeof(slot->artist));
    }

    slot->found = true;
    slot->pack_row = -1;
    strmemccpy(slot->path, path, sizeof(slot->path));
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

static struct bitmap *load_thumb_bitmap(const char *path, int size,
                                        int pack_row)
{
    size = MIN(size, ALBUMLIST_THUMB_SIZE);
    if (size <= 0)
        return NULL;

    for (int i = 0; i < ALBUMLIST_BITMAP_CACHE; i++)
    {
        struct albumlist_bitmap_slot *slot = &bitmap_cache[i];
        if (slot->valid && slot->size == size && strcmp(slot->path, path) == 0)
        {
            slot->last_used = ++bitmap_tick;
            return &slot->bm;
        }
    }

    struct albumlist_bitmap_slot *slot = bitmap_cache_victim();

    if (albumlist_load_thumb_from_pack(slot, pack_row, size))
    {
        slot->valid = true;
        slot->size = size;
        slot->last_used = ++bitmap_tick;
        strmemccpy(slot->path, path, sizeof(slot->path));
        return &slot->bm;
    }

    memset(&slot->bm, 0, sizeof(slot->bm));
    slot->bm.width = size;
    slot->bm.height = size;
    slot->bm.format = FORMAT_NATIVE;
    slot->bm.data = slot->data;

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    cpu_boost(true);
#endif
    int rc = read_bmp_file(path, &slot->bm, sizeof(slot->data),
                           FORMAT_NATIVE | FORMAT_RESIZE |
                           FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    cpu_boost(false);
#endif
    if (rc < 0)
    {
        slot->valid = false;
        return NULL;
    }

    slot->valid = true;
    slot->size = size;
    slot->last_used = ++bitmap_tick;
    strmemccpy(slot->path, path, sizeof(slot->path));
    return &slot->bm;
}

static struct bitmap *find_thumb_bitmap(const char *path, int size)
{
    size = MIN(size, ALBUMLIST_THUMB_SIZE);
    for (int i = 0; i < ALBUMLIST_BITMAP_CACHE; i++)
    {
        struct albumlist_bitmap_slot *slot = &bitmap_cache[i];
        if (slot->valid && slot->size == size &&
            strcmp(slot->path, path) == 0)
        {
            slot->last_used = ++bitmap_tick;
            return &slot->bm;
        }
    }

    return NULL;
}

static void albumlist_queue_pending(struct tree_context *tc, int line,
                                    const char *album, const char *artist,
                                    const char *path, int size, int pack_row)
{
    for (int i = 0; i < pending_count; i++)
    {
        if (pending_items[i].tc == tc && pending_items[i].line == line &&
            pending_items[i].size == size)
            return;
    }

    if (pending_count >= ALBUMLIST_PENDING_MAX)
        return;

    struct albumlist_pending_item *item = &pending_items[pending_count++];
    item->tc = tc;
    item->line = line;
    item->size = size;
    item->pack_row = pack_row;
    strmemccpy(item->album, album, sizeof(item->album));
    strmemccpy(item->artist, artist, sizeof(item->artist));
    if (path)
        strmemccpy(item->path, path, sizeof(item->path));
    else
        item->path[0] = '\0';
}

static bool albumlist_art_service_item(struct albumlist_pending_item *item)
{
    static char track_path[MAX_PATH];
    static char cover_path[ALBUMLIST_PATH_LEN];
    const char *path = item->path;

    if (!path[0])
    {
        if (!tagtree_get_album_art_path(item->tc, item->line,
                                        track_path,
                                        sizeof(track_path)) ||
            !albumlist_cover_path_for_track(track_path, cover_path,
                                            sizeof(cover_path)))
            return false;

        cache_thumb_path(item->album, item->artist, cover_path);
        path = cover_path;
    }

    return load_thumb_bitmap(path, item->size, item->pack_row) != NULL;
}

bool albumlist_art_service_pending(void)
{
    bool changed = false;
    int count = pending_count;

    pending_count = 0;
    for (int i = 0; i < count; i++)
    {
        if (albumlist_art_service_item(&pending_items[i]))
            changed = true;
    }

    return changed;
}

/* Bounded variant for use between wheel detents: loads at most one missing
 * thumbnail, so queued navigation is never delayed by more than one small
 * pack read.  Remaining work stays queued for the idle burst. */
bool albumlist_art_service_one(void)
{
    bool changed = false;

    if (pending_count <= 0)
        return false;

    changed = albumlist_art_service_item(&pending_items[0]);
    pending_count--;
    memmove(&pending_items[0], &pending_items[1],
            pending_count * sizeof(pending_items[0]));

    return changed;
}

/* Queue up to two not-yet-cached rows beyond the selection in the scroll
 * direction.  RAM-only: manifest and cache lookups here, loads later from
 * the bounded service points. */
void albumlist_art_prefetch_direction(struct gui_synclist *list,
                                      int direction)
{
    struct tree_context *tc = list ? (struct tree_context *)list->data : NULL;
    char album[ALBUMLIST_ALBUM_LEN];
    char artist[ALBUMLIST_ARTIST_LEN];
    char path[ALBUMLIST_PATH_LEN];
    int queued = 0;
    int thumb_size;

    if (!list || !tc || direction == 0 ||
        list->callback_draw_item != albumlist_art_draw_item)
        return;

    thumb_size = MIN(ALBUMLIST_THUMB_SIZE,
                     list->line_height[SCREEN_MAIN] - 2);

    for (int step = 1; step <= 4 && queued < 2; step++)
    {
        int line = list->selected_item + direction * step;
        int pack_row = -1;

        if (line < 0 || line >= list->nb_items)
            break;

        if (!albumlist_get_album_row(tc, line, album, sizeof(album),
                                     artist, sizeof(artist)))
            continue;

        if (!lookup_thumb_path(album, artist, path, sizeof(path), &pack_row))
            continue;

        if (find_thumb_bitmap(path, thumb_size))
            continue;

        albumlist_queue_pending(tc, line, album, artist, path, thumb_size,
                                pack_row);
        queued++;
    }
}

struct bitmap *albumlist_art_get_thumb(const char *album, const char *artist,
                                       int size)
{
    char path[ALBUMLIST_PATH_LEN];
    int pack_row = -1;

    if (!album || !album[0])
        return NULL;
    if (!artist)
        artist = "";

    if (!lookup_thumb_path(album, artist, path, sizeof(path), &pack_row))
        return NULL;

    return load_thumb_bitmap(path, size, pack_row);
}

#ifdef HAVE_IPODJS_UI
static void albumlist_draw_item_ipodjs(struct list_putlineinfo_t *list_info,
                                       int thumb_size, int text_pad)
{
    static char album[ALBUMLIST_ALBUM_LEN];
    static char artist[ALBUMLIST_ARTIST_LEN];
    static char path[ALBUMLIST_PATH_LEN];
    struct bitmap *bm = NULL;

    if (!list_info || list_info->is_title)
    {
        gui_list_draw_item_default(list_info);
        return;
    }

    struct tree_context *tc = (struct tree_context *)list_info->list->data;
    if (albumlist_get_album_row(tc, list_info->line,
                                album, sizeof(album),
                                artist, sizeof(artist)))
    {
        int pack_row = -1;

        thumb_size = MIN(thumb_size, list_info->linedes->height - 2);
        if (lookup_thumb_path(album, artist, path, sizeof(path), &pack_row))
        {
            bm = find_thumb_bitmap(path, thumb_size);
            if (!bm)
                albumlist_queue_pending(tc, list_info->line, album, artist,
                                        path, thumb_size, pack_row);
        }
        else
        {
            albumlist_queue_pending(tc, list_info->line, album, artist,
                                    NULL, thumb_size, -1);
        }
    }

    int font_h = font_get(ipodjs_ui_font())->height;
    int text_x = list_info->item_indent;
    int text_y = list_info->y +
        MAX(0, (list_info->linedes->height - font_h) / 2);

    if (bm)
        text_x += bm->width + text_pad;
    ipodjs_ui_puts_fit(list_info->display, text_x, text_y,
                       list_info->display->lcdwidth - text_x - 28,
                       list_info->dsp_text, false);
    if (bm)
    {
        int art_x = list_info->item_indent + 1;
        int art_y = list_info->y +
            MAX(0, (list_info->linedes->height - bm->height) / 2);
        list_info->display->bmp_part(bm, 0, 0, art_x, art_y,
                                     bm->width, bm->height);
    }
}
#endif

static void albumlist_draw_item_with_art(struct list_putlineinfo_t *list_info,
                                         int thumb_size, int text_pad)
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
    int pack_row = -1;

    if (!albumlist_get_album_row(tc, list_info->line,
                                 album, sizeof(album),
                                 artist, sizeof(artist)) ||
        !lookup_thumb_path(album, artist, path, sizeof(path), &pack_row))
    {
        gui_list_draw_item_default(list_info);
        return;
    }

    thumb_size = MIN(thumb_size, list_info->linedes->height - 2);
    struct bitmap *bm = load_thumb_bitmap(path, thumb_size, pack_row);

    if (!bm)
    {
        gui_list_draw_item_default(list_info);
        return;
    }

    struct list_putlineinfo_t text_info = *list_info;
    text_info.item_indent += bm->width + text_pad;
    text_info.icon = Icon_NOICON;
    text_info.have_icons = false;
    gui_list_draw_item_default(&text_info);

    int x = list_info->item_indent + 1;
    int y = list_info->y + MAX(0, (list_info->linedes->height - bm->height) / 2);
    list_info->display->bmp_part(bm, 0, 0, x, y, bm->width, bm->height);
}

static void albumlist_art_draw_item_compact(struct list_putlineinfo_t *list_info)
{
    albumlist_draw_item_with_art(list_info, ALBUMLIST_COMPACT_THUMB_SIZE,
                                 ALBUMLIST_COMPACT_TEXT_PAD);
}

static int albumlist_first_album_row(struct gui_synclist *list,
                                     struct tree_context *tc)
{
    char album[ALBUMLIST_ALBUM_LEN];
    char artist[ALBUMLIST_ARTIST_LEN];

    if (!list || !tc)
        return -1;

    for (int i = tc->special_entry_count; i < list->nb_items; i++)
    {
        if (albumlist_get_album_row(tc, i, album, sizeof(album),
                                    artist, sizeof(artist)))
            return i;
    }

    return -1;
}

void albumlist_setup_list(struct gui_synclist *list)
{
    struct tree_context *tc = list ? (struct tree_context *)list->data : NULL;
    int first_album_row;

    if (!list)
        return;

    list->show_icons = global_settings.show_icons;
    list->callback_draw_item = NULL;
    pending_count = 0;
    gui_synclist_set_fullscreen_albumlist(list, false);

    if (!albumlist_has_album_rows(list))
        return;

    list->scroll_paginated = false;

#ifdef HAVE_IPODJS_UI
    /* The stock 6G album browser keeps a cover thumbnail on every album row.
     * Retain iPodJS' own full-width header/list renderer, but give it the
     * stock-sized art callback instead of falling back to text-only rows. */
    if (global_settings.ui_engine == UI_ENGINE_IPODJS)
    {
        /* Load the manifest before the draw callback is on the stack. */
        albumlist_load_manifest_cache();
        list->callback_draw_item = albumlist_art_draw_item;
        list->callback_get_item_icon = NULL;
        list->show_icons = false;
        FOR_NB_SCREENS(i)
        {
            if (screens[i].lcdwidth == 320 && screens[i].lcdheight == 240)
                list->line_height[i] = MAX(list->line_height[i],
                                           ALBUMLIST_ROW_HEIGHT);
        }
        return;
    }
#endif

    if (global_settings.album_list_layout == ALBUM_LIST_LAYOUT_COMPACT)
    {
        list->callback_draw_item = albumlist_art_draw_item_compact;
        return;
    }

    gui_synclist_set_fullscreen_albumlist(list, true);
    list->callback_get_item_icon = NULL;
    list->show_icons = false;
    list->callback_draw_item = albumlist_art_draw_item;
    FOR_NB_SCREENS(i)
    {
        if (screens[i].lcdwidth == 320 && screens[i].lcdheight == 240)
            list->line_height[i] = MAX(list->line_height[i],
                                       ALBUMLIST_ROW_HEIGHT);
    }

    first_album_row = albumlist_first_album_row(list, tc);
    list->fullscreen_albumlist_first_item = MAX(0, first_album_row);
    if (tc && first_album_row >= 0 && tc->selected_item < first_album_row)
        tc->selected_item = first_album_row;

    if (first_album_row >= 0 && list->selected_item < first_album_row)
        gui_synclist_select_item(list, first_album_row);

}

void albumlist_art_draw_item(struct list_putlineinfo_t *list_info)
{
#ifdef HAVE_IPODJS_UI
    if (global_settings.ui_engine == UI_ENGINE_IPODJS)
    {
        albumlist_draw_item_ipodjs(list_info, ALBUMLIST_THUMB_SIZE,
                                   ALBUMLIST_TEXT_PAD);
        return;
    }
#endif
    albumlist_draw_item_with_art(list_info, ALBUMLIST_THUMB_SIZE,
                                 ALBUMLIST_TEXT_PAD);
}

#endif
