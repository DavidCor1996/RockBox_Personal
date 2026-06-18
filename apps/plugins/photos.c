/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Photos launcher plugin with folder browsing, a thumbnail grid, rename,
 * new-folder creation, and simple 4-digit locks for files and folders.
 *
 ****************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"

#define PHOTOS_ROOT      "/Photos"
#define PHOTOS_THUMB_DIR PHOTOS_ROOT "/.photo_thumbs"
#define PHOTOS_VIEWER    VIEWERS_DIR "/imageviewer.rock"
#define PHOTOS_STATE     PLUGIN_APPS_DATA_DIR "/photos.state"
#define PHOTOS_LOCKS     PLUGIN_APPS_DATA_DIR "/photos.locks"

#define MAX_VISIBLE_THUMBS 6
#define MIN_ENTRY_CAPACITY 32
#define MAX_LOCKS 128
#define MAX_MOVE_DIRS 96
#define PIN_LEN 4

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) \
    || (CONFIG_KEYPAD == IPOD_3G_PAD) \
    || (CONFIG_KEYPAD == IPOD_4G_PAD)
#define PHOTOS_IPOD_CONTROLS
#endif

enum photos_entry_kind {
    PHOTOS_ENTRY_DIR = 0,
    PHOTOS_ENTRY_FILE,
};

struct photo_entry {
    char path[MAX_PATH];
    bool is_dir;
    bool locked;
};

struct photo_lock {
    char relpath[MAX_PATH];
    char pin[PIN_LEN + 1];
};

struct photo_move_dir {
    char path[MAX_PATH];
};

struct thumb_slot {
    int entry_index;
    bool loaded;
    struct bitmap bitmap;
    fb_data *data;
    size_t bytes;
};

struct photos_state {
    struct photo_entry *entries;
    int entry_count;
    int entry_capacity;
    int selected;
    int page_start;

    int cols;
    int rows;
    int visible_count;
    int margin;
    int gap;
    int tile_w;
    int tile_h;
    int thumb_w;
    int thumb_h;
    int header_h;
    int footer_h;
    int line_h;

    char current_dir[MAX_PATH];
    char selected_path[MAX_PATH];

    struct thumb_slot thumbs[MAX_VISIBLE_THUMBS];
};

static struct photos_state photos;
static struct photo_lock photo_locks[MAX_LOCKS];
static struct photo_move_dir photo_move_dirs[MAX_MOVE_DIRS];
static int photo_lock_count;
static int photo_move_dir_count;

static int photos_tsr_exit(bool reenter)
{
    return reenter ? PLUGIN_TSR_TERMINATE : PLUGIN_TSR_SUSPEND;
}

#ifdef PHOTOS_IPOD_CONTROLS
static const struct button_mapping photos_ipod_ctx[] =
{
    { PLA_SCROLL_BACK,        BUTTON_SCROLL_BACK,                BUTTON_NONE },
    { PLA_SCROLL_FWD,         BUTTON_SCROLL_FWD,                 BUTTON_NONE },
    { PLA_SCROLL_BACK_REPEAT, BUTTON_SCROLL_BACK|BUTTON_REPEAT,  BUTTON_NONE },
    { PLA_SCROLL_FWD_REPEAT,  BUTTON_SCROLL_FWD|BUTTON_REPEAT,   BUTTON_NONE },
    { PLA_CANCEL,             BUTTON_MENU|BUTTON_REL,            BUTTON_MENU },
    { PLA_CANCEL,             BUTTON_LEFT|BUTTON_REL,            BUTTON_LEFT },
    { PLA_SELECT_REL,         BUTTON_SELECT|BUTTON_REL,          BUTTON_SELECT },
    { PLA_UP_REPEAT,          BUTTON_SELECT|BUTTON_REPEAT,       BUTTON_SELECT },
    LAST_ITEM_IN_LIST
};
#endif

static bool photos_is_hidden(const char *name)
{
    return name[0] == '.';
}

static bool photos_has_supported_ext(const char *name)
{
    static const char *const exts[] = {
        ".bmp", ".gif", ".jpg", ".jpe", ".jpeg", ".png",
#ifdef HAVE_LCD_COLOR
        ".ppm",
#endif
    };
    const char *ext = rb->strrchr(name, '.');
    size_t i;

    if (!ext)
        return false;

    for (i = 0; i < ARRAYLEN(exts); i++)
    {
        if (!rb->strcasecmp(ext, exts[i]))
            return true;
    }

    return false;
}

static const char *photos_basename(const char *path)
{
    const char *base = rb->strrchr(path, '/');
    return base ? base + 1 : path;
}

static bool photos_path_is_root(const char *path)
{
    return !rb->strcmp(path, PHOTOS_ROOT);
}

static bool photos_relpath(const char *path, char *out, size_t out_size)
{
    size_t root_len = rb->strlen(PHOTOS_ROOT);

    if (rb->strncmp(path, PHOTOS_ROOT, root_len) != 0)
        return false;

    path += root_len;
    if (*path == '/')
        path++;

    rb->strlcpy(out, path, out_size);
    return true;
}

static void photos_dirname(const char *path, char *out, size_t out_size)
{
    char tmp[MAX_PATH];
    char *slash;

    rb->strlcpy(tmp, path, sizeof(tmp));
    slash = rb->strrchr(tmp, '/');
    if (!slash || slash == tmp)
    {
        rb->strlcpy(out, PHOTOS_ROOT, out_size);
        return;
    }

    *slash = '\0';
    rb->strlcpy(out, tmp, out_size);
}

static void photos_ensure_parent_dirs(const char *path)
{
    char tmp[MAX_PATH];
    char *p;

    rb->strlcpy(tmp, path, sizeof(tmp));
    for (p = tmp + 1; *p; p++)
    {
        if (*p != '/')
            continue;

        *p = '\0';
        rb->mkdir(tmp);
        *p = '/';
    }
}

static int photos_find_lock_rel(const char *relpath)
{
    int i;

    for (i = 0; i < photo_lock_count; i++)
    {
        if (!rb->strcmp(photo_locks[i].relpath, relpath))
            return i;
    }

    return -1;
}

static int photos_find_lock(const char *path)
{
    char relpath[MAX_PATH];

    if (!photos_relpath(path, relpath, sizeof(relpath)))
        return -1;

    return photos_find_lock_rel(relpath);
}

static bool photos_is_valid_pin(const char *pin)
{
    int i;

    if ((int)rb->strlen(pin) != PIN_LEN)
        return false;

    for (i = 0; i < PIN_LEN; i++)
    {
        if (pin[i] < '0' || pin[i] > '9')
            return false;
    }

    return true;
}

static void photos_ensure_app_dir(void)
{
    rb->mkdir(PLUGIN_APPS_DATA_DIR);
}

static void photos_load_locks(void)
{
    int fd;
    char line[MAX_PATH + 16];

    photo_lock_count = 0;
    fd = rb->open(PHOTOS_LOCKS, O_RDONLY);
    if (fd < 0)
        return;

    while (photo_lock_count < MAX_LOCKS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *sep = rb->strrchr(line, '|');
        if (!sep)
            continue;

        *sep++ = '\0';
        if (!photos_is_valid_pin(sep) || line[0] == '\0')
            continue;

        rb->strlcpy(photo_locks[photo_lock_count].relpath, line,
                    sizeof(photo_locks[photo_lock_count].relpath));
        rb->strlcpy(photo_locks[photo_lock_count].pin, sep,
                    sizeof(photo_locks[photo_lock_count].pin));
        photo_lock_count++;
    }

    rb->close(fd);
}

static void photos_save_locks(void)
{
    int fd;
    int i;

    photos_ensure_app_dir();
    fd = rb->open(PHOTOS_LOCKS, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;

    for (i = 0; i < photo_lock_count; i++)
    {
        rb->fdprintf(fd, "%s|%s\n", photo_locks[i].relpath, photo_locks[i].pin);
    }

    rb->close(fd);
}

static void photos_update_lock_paths(const char *old_path, const char *new_path)
{
    char old_rel[MAX_PATH];
    char new_rel[MAX_PATH];
    size_t old_len;
    int i;

    if (!photos_relpath(old_path, old_rel, sizeof(old_rel)) ||
        !photos_relpath(new_path, new_rel, sizeof(new_rel)))
        return;

    old_len = rb->strlen(old_rel);
    for (i = 0; i < photo_lock_count; i++)
    {
        if (!rb->strcmp(photo_locks[i].relpath, old_rel))
        {
            rb->strlcpy(photo_locks[i].relpath, new_rel,
                        sizeof(photo_locks[i].relpath));
        }
        else if (old_len > 0 &&
                 rb->strncmp(photo_locks[i].relpath, old_rel, old_len) == 0 &&
                 photo_locks[i].relpath[old_len] == '/')
        {
            char tail[MAX_PATH];
            rb->strlcpy(tail, photo_locks[i].relpath + old_len,
                        sizeof(tail));
            rb->snprintf(photo_locks[i].relpath,
                         sizeof(photo_locks[i].relpath),
                         "%s%s", new_rel, tail);
        }
    }
}

static bool photos_prompt_pin(const char *title, char *pin, size_t pin_size)
{
    int rc;

    rb->splash(HZ / 2, title);
    pin[0] = '\0';
    rc = rb->kbd_input(pin, pin_size, NULL);
    if (rc < 0)
        return false;

    if (!photos_is_valid_pin(pin))
    {
        rb->splash(HZ * 2, "Use 4 digits");
        return false;
    }

    return true;
}

static bool photos_verify_pin(const char *expected)
{
    char pin[PIN_LEN + 2];

    if (!photos_prompt_pin("Enter 4-digit code", pin, sizeof(pin)))
        return false;

    if (rb->strcmp(pin, expected))
    {
        rb->splash(HZ * 2, "Wrong code");
        return false;
    }

    return true;
}

static bool photos_unlock_if_needed(const char *path)
{
    int lock_index = photos_find_lock(path);

    if (lock_index < 0)
        return true;

    return photos_verify_pin(photo_locks[lock_index].pin);
}

static bool photos_prompt_name(const char *title, const char *initial,
                               char *out, size_t out_size)
{
    int rc;

    rb->strlcpy(out, initial, out_size);
    rb->splash(HZ / 2, title);
    rc = rb->kbd_input(out, out_size, NULL);
    if (rc < 0 || out[0] == '\0')
        return false;

    if (rb->strchr(out, '/'))
    {
        rb->splash(HZ * 2, "No / in names");
        return false;
    }

    return true;
}

static void photos_layout_init(void)
{
    rb->lcd_setfont(FONT_UI);
    rb->lcd_getstringsize("Ag", NULL, &photos.line_h);

    photos.cols = (LCD_WIDTH >= 300) ? 3 : 2;
    photos.rows = (LCD_HEIGHT >= 180) ? 2 : 1;
    photos.visible_count = photos.cols * photos.rows;
    if (photos.visible_count > MAX_VISIBLE_THUMBS)
        photos.visible_count = MAX_VISIBLE_THUMBS;

    photos.margin = 8;
    photos.gap = 6;
    photos.header_h = photos.line_h + 10;
    photos.footer_h = photos.line_h + 14;

    photos.tile_w = (LCD_WIDTH - photos.margin * 2
                     - photos.gap * (photos.cols - 1)) / photos.cols;
    photos.tile_h = (LCD_HEIGHT - photos.margin * 2 - photos.header_h
                     - photos.footer_h - photos.gap * (photos.rows - 1))
                    / photos.rows;

    photos.thumb_w = photos.tile_w - 10;
    photos.thumb_h = photos.tile_h - 10;
    if (photos.thumb_w < 16)
        photos.thumb_w = 16;
    if (photos.thumb_h < 16)
        photos.thumb_h = 16;
}

static bool photos_allocate_buffers(void)
{
    unsigned char *buffer;
    size_t buffer_size;
    size_t slot_bytes;
    size_t entry_bytes;
    int i;

    photos_layout_init();

    buffer = rb->plugin_get_buffer(&buffer_size);
    slot_bytes = BM_SIZE(photos.thumb_w, photos.thumb_h, FORMAT_NATIVE, false);
    entry_bytes = sizeof(struct photo_entry);

    if (!buffer || photos.visible_count <= 0 || slot_bytes == 0)
        return false;

    if (buffer_size < slot_bytes * (size_t)photos.visible_count +
                      entry_bytes * MIN_ENTRY_CAPACITY)
        return false;

    for (i = 0; i < photos.visible_count; i++)
    {
        photos.thumbs[i].entry_index = -1;
        photos.thumbs[i].loaded = false;
        photos.thumbs[i].bytes = slot_bytes;
        photos.thumbs[i].data = (fb_data *)buffer;
        photos.thumbs[i].bitmap.data = (unsigned char *)photos.thumbs[i].data;
        photos.thumbs[i].bitmap.format = FORMAT_NATIVE;
        photos.thumbs[i].bitmap.width = 0;
        photos.thumbs[i].bitmap.height = 0;
        buffer += slot_bytes;
        buffer_size -= slot_bytes;
    }

    photos.entries = (struct photo_entry *)buffer;
    photos.entry_capacity = (int)(buffer_size / entry_bytes);
    photos.entry_count = 0;
    photos.selected = 0;
    photos.page_start = 0;
    photos.current_dir[0] = '\0';
    photos.selected_path[0] = '\0';

    return photos.entry_capacity >= MIN_ENTRY_CAPACITY;
}

static void photos_set_selected_path_from_index(void)
{
    if (photos.entry_count <= 0 || photos.selected < 0 ||
        photos.selected >= photos.entry_count)
    {
        photos.selected_path[0] = '\0';
        return;
    }

    rb->strlcpy(photos.selected_path, photos.entries[photos.selected].path,
                sizeof(photos.selected_path));
}

static int compare_photo_entries(const void *left, const void *right)
{
    const struct photo_entry *a = left;
    const struct photo_entry *b = right;

    if (a->is_dir != b->is_dir)
        return a->is_dir ? -1 : 1;

    return rb->strcasecmp(photos_basename(a->path), photos_basename(b->path));
}

static void photos_add_entry(const char *path, bool is_dir)
{
    if (photos.entry_count >= photos.entry_capacity)
        return;

    rb->strlcpy(photos.entries[photos.entry_count].path, path,
                sizeof(photos.entries[photos.entry_count].path));
    photos.entries[photos.entry_count].is_dir = is_dir;
    photos.entries[photos.entry_count].locked = (photos_find_lock(path) >= 0);
    photos.entry_count++;
}

static void photos_invalidate_thumbs(void)
{
    int i;

    for (i = 0; i < photos.visible_count; i++)
    {
        photos.thumbs[i].entry_index = -1;
        photos.thumbs[i].loaded = false;
        photos.thumbs[i].bitmap.width = 0;
        photos.thumbs[i].bitmap.height = 0;
    }
}

static void photos_thumb_path(const char *photo_path, char *thumb_path,
                              size_t thumb_path_size)
{
    char relpath[MAX_PATH];

    if (!photos_relpath(photo_path, relpath, sizeof(relpath)))
    {
        thumb_path[0] = '\0';
        return;
    }

    rb->snprintf(thumb_path, thumb_path_size, "%s/%s.bmp",
                 PHOTOS_THUMB_DIR, relpath);
}

static bool photos_load_thumb(struct thumb_slot *slot, const char *photo_path)
{
    char thumb_path[MAX_PATH];
    int rc;

    photos_thumb_path(photo_path, thumb_path, sizeof(thumb_path));
    if (thumb_path[0] == '\0')
        return false;

    slot->bitmap.data = (unsigned char *)slot->data;
    slot->bitmap.format = FORMAT_NATIVE;
    slot->bitmap.width = photos.thumb_w;
    slot->bitmap.height = photos.thumb_h;

    rc = rb->read_bmp_file(thumb_path, &slot->bitmap, (int)slot->bytes,
                           FORMAT_NATIVE | FORMAT_DITHER, NULL);
    if (rc > 0 && slot->bitmap.width > 0 && slot->bitmap.height > 0 &&
        slot->bitmap.width <= photos.thumb_w &&
        slot->bitmap.height <= photos.thumb_h)
    {
        return true;
    }

    slot->bitmap.width = photos.thumb_w;
    slot->bitmap.height = photos.thumb_h;
    rc = rb->read_bmp_file(thumb_path, &slot->bitmap, (int)slot->bytes,
                           FORMAT_NATIVE | FORMAT_RESIZE |
                           FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);
    return rc > 0 && slot->bitmap.width > 0 && slot->bitmap.height > 0;
}

static void photos_refresh_page(void)
{
    int i;

    for (i = 0; i < photos.visible_count; i++)
    {
        int entry_index = photos.page_start + i;
        struct thumb_slot *slot = &photos.thumbs[i];

        if (entry_index >= photos.entry_count)
        {
            slot->entry_index = -1;
            slot->loaded = false;
            slot->bitmap.width = 0;
            slot->bitmap.height = 0;
            continue;
        }

        if (slot->entry_index == entry_index &&
            (photos.entries[entry_index].is_dir || slot->loaded))
            continue;

        slot->entry_index = entry_index;
        if (photos.entries[entry_index].is_dir)
        {
            slot->loaded = false;
            slot->bitmap.width = 0;
            slot->bitmap.height = 0;
        }
        else
        {
            slot->loaded = photos_load_thumb(slot, photos.entries[entry_index].path);
        }
    }
}

static void photos_ensure_selection_visible(void)
{
    int new_page_start;

    if (photos.entry_count <= 0)
    {
        photos.selected = 0;
        photos.page_start = 0;
        photos.selected_path[0] = '\0';
        return;
    }

    if (photos.selected < 0)
        photos.selected = 0;
    if (photos.selected >= photos.entry_count)
        photos.selected = photos.entry_count - 1;

    new_page_start = (photos.selected / photos.visible_count) * photos.visible_count;
    if (new_page_start != photos.page_start)
    {
        photos.page_start = new_page_start;
        photos_invalidate_thumbs();
    }

    photos_set_selected_path_from_index();
    photos_refresh_page();
}

static void photos_rescan_current_dir(void)
{
    DIR *dir;
    struct dirent *entry;
    char wanted[MAX_PATH];

    rb->strlcpy(wanted, photos.selected_path, sizeof(wanted));
    photos.entry_count = 0;

    dir = rb->opendir(photos.current_dir);
    if (!dir)
        return;

    while ((entry = rb->readdir(dir)) != NULL &&
           photos.entry_count < photos.entry_capacity)
    {
        struct dirinfo info;
        char path[MAX_PATH];

        if (!rb->strcmp(entry->d_name, ".") || !rb->strcmp(entry->d_name, ".."))
            continue;
        if (photos_is_hidden(entry->d_name))
            continue;

        info = rb->dir_get_info(dir, entry);
        rb->snprintf(path, sizeof(path), "%s/%s", photos.current_dir, entry->d_name);

        if (info.attribute & ATTR_DIRECTORY)
        {
            photos_add_entry(path, true);
        }
        else if (photos_has_supported_ext(entry->d_name))
        {
            photos_add_entry(path, false);
        }
    }

    rb->closedir(dir);

    if (photos.entry_count > 1)
    {
        rb->qsort(photos.entries, photos.entry_count,
                  sizeof(struct photo_entry), compare_photo_entries);
    }

    photos.selected = 0;
    if (wanted[0] != '\0')
    {
        int i;
        for (i = 0; i < photos.entry_count; i++)
        {
            if (!rb->strcmp(photos.entries[i].path, wanted))
            {
                photos.selected = i;
                break;
            }
        }
    }

    photos.page_start = 0;
    photos_invalidate_thumbs();
    photos_ensure_selection_visible();
}

static void photos_clear_resume_state(void)
{
    int fd;

    photos_ensure_app_dir();
    fd = rb->open(PHOTOS_STATE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
        rb->close(fd);
}

static void photos_save_resume_state(void)
{
    int fd;

    photos_ensure_app_dir();
    fd = rb->open(PHOTOS_STATE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;

    rb->fdprintf(fd, "%s\n%s\n", photos.current_dir, photos.selected_path);
    rb->close(fd);
}

static void photos_restore_resume_state(void)
{
    int fd;
    char dir[MAX_PATH];
    char path[MAX_PATH];

    fd = rb->open(PHOTOS_STATE, O_RDONLY);
    if (fd < 0)
        return;

    if (rb->read_line(fd, dir, sizeof(dir)) > 0 && rb->dir_exists(dir))
        rb->strlcpy(photos.current_dir, dir, sizeof(photos.current_dir));

    if (rb->read_line(fd, path, sizeof(path)) > 0)
        rb->strlcpy(photos.selected_path, path, sizeof(photos.selected_path));

    rb->close(fd);
}

static void photos_truncate_to_width(const char *src, char *dst,
                                     size_t dst_size, int max_width)
{
    int width = 0;
    size_t len;

    if (dst_size == 0)
        return;

    rb->strlcpy(dst, src, dst_size);
    rb->lcd_getstringsize(dst, &width, NULL);
    if (width <= max_width)
        return;

    len = rb->strlen(dst);
    while (len > 3)
    {
        dst[--len] = '\0';
        dst[len - 1] = '.';
        dst[len - 2] = '.';
        dst[len - 3] = '.';
        rb->lcd_getstringsize(dst, &width, NULL);
        if (width <= max_width)
            return;
    }
}

static void photos_draw_tile_placeholder(int x, int y, int w, int h,
                                         const char *label)
{
    int text_w = 0;
    int text_h = 0;
    int text_x;
    int text_y;

    rb->lcd_set_foreground(LCD_RGBPACK(60, 60, 60));
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(LCD_RGBPACK(110, 110, 110));
    rb->lcd_drawrect(x, y, w, h);
    rb->lcd_getstringsize(label, &text_w, &text_h);
    text_x = x + (w - text_w) / 2;
    text_y = y + (h - text_h) / 2;
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(text_x, text_y, label);
}

static void photos_draw_folder_icon(int x, int y, int w, int h)
{
    int tab_w = MAX(16, w / 2);
    int tab_h = MAX(5, h / 5);
    int body_y = y + tab_h - 1;
    int body_h = h - tab_h + 1;

    rb->lcd_set_foreground(LCD_RGBPACK(230, 190, 74));
    rb->lcd_fillrect(x + 3, y, tab_w, tab_h);
    rb->lcd_fillrect(x, body_y, w, body_h);

    rb->lcd_set_foreground(LCD_RGBPACK(250, 220, 112));
    rb->lcd_hline(x + 4, x + tab_w - 1, y + 1);
    rb->lcd_hline(x + 2, x + w - 3, body_y + 2);

    rb->lcd_set_foreground(LCD_RGBPACK(124, 88, 32));
    rb->lcd_drawrect(x + 3, y, tab_w, tab_h);
    rb->lcd_drawrect(x, body_y, w, body_h);
}

static void photos_draw_folder_tile_contents(const struct photo_entry *entry,
                                             int x, int y, int w, int h)
{
    char label[64];
    int label_h = photos.line_h;
    int label_y = y + h - label_h;
    int icon_w = MIN(w - 8, 42);
    int icon_h = MIN(h - label_h - 8, 32);
    int icon_x;
    int icon_y;
    int label_w = 0;

    if (icon_w < 24)
        icon_w = MAX(16, w - 4);
    if (icon_h < 18)
        icon_h = MAX(12, h - label_h - 4);

    icon_x = x + (w - icon_w) / 2;
    icon_y = y + MAX(2, (h - label_h - icon_h) / 2);

    rb->lcd_set_foreground(LCD_RGBPACK(34, 34, 34));
    rb->lcd_fillrect(x, y, w, h);
    photos_draw_folder_icon(icon_x, icon_y, icon_w, icon_h);

    photos_truncate_to_width(photos_basename(entry->path), label,
                             sizeof(label), w);
    rb->lcd_getstringsize(label, &label_w, NULL);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(x + MAX(0, (w - label_w) / 2), label_y, label);
}

static void photos_draw_tile(int slot_index)
{
    int x;
    int y;
    int col;
    int row;
    int entry_index;
    int inner_x;
    int inner_y;
    int inner_w;
    int inner_h;
    const bool selected = (photos.page_start + slot_index) == photos.selected;
    const struct photo_entry *entry;
    struct thumb_slot *slot = &photos.thumbs[slot_index];

    entry_index = photos.page_start + slot_index;
    if (entry_index >= photos.entry_count)
        return;

    entry = &photos.entries[entry_index];
    col = slot_index % photos.cols;
    row = slot_index / photos.cols;

    x = photos.margin + col * (photos.tile_w + photos.gap);
    y = photos.margin + photos.header_h + row * (photos.tile_h + photos.gap);

    rb->lcd_set_foreground(selected ? LCD_WHITE : LCD_RGBPACK(85, 85, 85));
    rb->lcd_drawrect(x - 1, y - 1, photos.tile_w + 2, photos.tile_h + 2);
    if (selected)
        rb->lcd_drawrect(x - 2, y - 2, photos.tile_w + 4, photos.tile_h + 4);

    rb->lcd_set_foreground(LCD_RGBPACK(20, 20, 20));
    rb->lcd_fillrect(x, y, photos.tile_w, photos.tile_h);

    inner_w = photos.tile_w - 8;
    inner_h = photos.tile_h - 8;
    inner_x = x + 4;
    inner_y = y + 4;

    if (entry->is_dir)
    {
        photos_draw_folder_tile_contents(entry, inner_x, inner_y,
                                         inner_w, inner_h);
    }
    else if (slot->loaded)
    {
        int draw_x = inner_x + (inner_w - slot->bitmap.width) / 2;
        int draw_y = inner_y + (inner_h - slot->bitmap.height) / 2;

        rb->lcd_bitmap((const fb_data *)slot->bitmap.data,
                       draw_x, draw_y, slot->bitmap.width, slot->bitmap.height);
    }
    else
    {
        photos_draw_tile_placeholder(inner_x, inner_y, inner_w, inner_h, "No Thumb");
    }

    if (entry->locked)
    {
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_putsxy(x + photos.tile_w - 10, y + 2, "L");
    }
}

static void photos_draw_header(void)
{
    char header[96];
    char relpath[MAX_PATH];

    if (photos_relpath(photos.current_dir, relpath, sizeof(relpath)) && relpath[0] != '\0')
        rb->snprintf(header, sizeof(header), "Photos/%s", relpath);
    else
        rb->snprintf(header, sizeof(header), "Photos");

    photos_truncate_to_width(header, header, sizeof(header),
                             LCD_WIDTH - photos.margin * 2);
    rb->lcd_putsxy(photos.margin, photos.margin, header);
}

static void photos_draw_footer(void)
{
    char label[96];
    char page[32];
    const struct photo_entry *entry;
    int pages;
    int page_num;

    if (photos.entry_count <= 0)
    {
        rb->lcd_putsxy(photos.margin, LCD_HEIGHT - photos.footer_h + 2,
#ifdef PHOTOS_IPOD_CONTROLS
                       photos_path_is_root(photos.current_dir) ?
                       "Menu: exit" : "Menu: back");
#else
                       "Hold Select: menu");
#endif
        return;
    }

    entry = &photos.entries[photos.selected];
    rb->snprintf(label, sizeof(label), "%s%s%s",
                 entry->is_dir ? "[Dir] " : "",
                 entry->locked ? "[Lock] " : "",
                 photos_basename(entry->path));
    photos_truncate_to_width(label, label, sizeof(label), LCD_WIDTH - 70);
    rb->lcd_putsxy(photos.margin, LCD_HEIGHT - photos.footer_h + 2, label);

    pages = (photos.entry_count + photos.visible_count - 1) / photos.visible_count;
    page_num = photos.selected / photos.visible_count + 1;
    rb->snprintf(page, sizeof(page), "%d/%d", page_num, pages);
    rb->lcd_putsxy(LCD_WIDTH - photos.margin - 36,
                   LCD_HEIGHT - photos.footer_h + 2, page);
}

static void photos_draw_screen(void)
{
    int i;

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_setfont(FONT_UI);

    photos_draw_header();

    for (i = 0; i < photos.visible_count; i++)
        photos_draw_tile(i);

    if (photos.entry_count <= 0)
    {
        rb->lcd_putsxy(photos.margin,
                       photos.margin + photos.header_h + photos.line_h,
                       "Empty folder");
    }

    photos_draw_footer();
    rb->lcd_update();
}

static void photos_move_selection(int delta)
{
    int next = photos.selected + delta;

    if (photos.entry_count <= 0)
        return;

    if (next < 0)
        next = 0;
    else if (next >= photos.entry_count)
        next = photos.entry_count - 1;

    if (next != photos.selected)
    {
        photos.selected = next;
        photos_ensure_selection_visible();
    }
}

static enum plugin_status photos_open_entry(struct photo_entry *entry)
{
    if (!photos_unlock_if_needed(entry->path))
        return PLUGIN_OK;

    if (entry->is_dir)
    {
        rb->strlcpy(photos.current_dir, entry->path, sizeof(photos.current_dir));
        photos.selected_path[0] = '\0';
        photos_rescan_current_dir();
        return PLUGIN_OK;
    }

    photos_set_selected_path_from_index();
    photos_save_resume_state();
    rb->plugin_tsr(photos_tsr_exit);
    return rb->plugin_open(PHOTOS_VIEWER, entry->path);
}

static bool photos_rename_selected(void)
{
    struct photo_entry *entry;
    char name[MAX_PATH];
    char parent[MAX_PATH];
    char newpath[MAX_PATH];

    if (photos.entry_count <= 0)
        return false;

    entry = &photos.entries[photos.selected];
    if (!photos_prompt_name("Rename", photos_basename(entry->path),
                            name, sizeof(name)))
        return false;

    photos_dirname(entry->path, parent, sizeof(parent));
    rb->snprintf(newpath, sizeof(newpath), "%s/%s", parent, name);
    if (!rb->strcmp(newpath, entry->path))
        return false;

    if (rb->rename(entry->path, newpath) < 0)
    {
        rb->splash(HZ * 2, "Rename failed");
        return false;
    }

    photos_update_lock_paths(entry->path, newpath);
    photos_save_locks();
    rb->strlcpy(photos.selected_path, newpath, sizeof(photos.selected_path));
    photos_rescan_current_dir();
    return true;
}

static bool photos_create_folder(void)
{
    char name[MAX_PATH];
    char path[MAX_PATH];

    if (!photos_prompt_name("New folder", "", name, sizeof(name)))
        return false;

    rb->snprintf(path, sizeof(path), "%s/%s", photos.current_dir, name);
    if (rb->mkdir(path) < 0)
    {
        rb->splash(HZ * 2, "mkdir failed");
        return false;
    }

    rb->strlcpy(photos.selected_path, path, sizeof(photos.selected_path));
    photos_rescan_current_dir();
    return true;
}

static bool photos_lock_selected(void)
{
    struct photo_entry *entry;
    char pin[PIN_LEN + 2];
    char relpath[MAX_PATH];
    int lock_index;

    if (photos.entry_count <= 0)
        return false;

    entry = &photos.entries[photos.selected];
    if (!photos_prompt_pin("Set 4-digit code", pin, sizeof(pin)))
        return false;
    if (!photos_relpath(entry->path, relpath, sizeof(relpath)))
        return false;

    lock_index = photos_find_lock_rel(relpath);
    if (lock_index < 0)
    {
        if (photo_lock_count >= MAX_LOCKS)
        {
            rb->splash(HZ * 2, "Lock list full");
            return false;
        }
        lock_index = photo_lock_count++;
    }

    rb->strlcpy(photo_locks[lock_index].relpath, relpath,
                sizeof(photo_locks[lock_index].relpath));
    rb->strlcpy(photo_locks[lock_index].pin, pin,
                sizeof(photo_locks[lock_index].pin));
    photos_save_locks();
    photos_rescan_current_dir();
    return true;
}

static bool photos_unlock_selected(void)
{
    struct photo_entry *entry;
    int lock_index;
    int i;

    if (photos.entry_count <= 0)
        return false;

    entry = &photos.entries[photos.selected];
    lock_index = photos_find_lock(entry->path);
    if (lock_index < 0)
        return false;

    if (!photos_verify_pin(photo_locks[lock_index].pin))
        return false;

    for (i = lock_index; i < photo_lock_count - 1; i++)
        photo_locks[i] = photo_locks[i + 1];
    photo_lock_count--;

    photos_save_locks();
    photos_rescan_current_dir();
    return true;
}

static void photos_remove_locks_under(const char *path)
{
    char relpath[MAX_PATH];
    size_t rel_len;
    int i = 0;

    if (!photos_relpath(path, relpath, sizeof(relpath)) || relpath[0] == '\0')
        return;

    rel_len = rb->strlen(relpath);
    while (i < photo_lock_count)
    {
        if (!rb->strcmp(photo_locks[i].relpath, relpath) ||
            (rb->strncmp(photo_locks[i].relpath, relpath, rel_len) == 0 &&
             photo_locks[i].relpath[rel_len] == '/'))
        {
            int j;
            for (j = i; j < photo_lock_count - 1; j++)
                photo_locks[j] = photo_locks[j + 1];
            photo_lock_count--;
        }
        else
        {
            i++;
        }
    }
}

static bool photos_delete_tree(const char *path)
{
    DIR *dir = rb->opendir(path);
    struct dirent *entry;
    bool ok = true;

    if (!dir)
        return rb->remove(path) == 0;

    while ((entry = rb->readdir(dir)) != NULL)
    {
        struct dirinfo info;
        char child[MAX_PATH];

        if (!rb->strcmp(entry->d_name, ".") || !rb->strcmp(entry->d_name, ".."))
            continue;

        rb->snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        info = rb->dir_get_info(dir, entry);
        if (info.attribute & ATTR_DIRECTORY)
            ok = photos_delete_tree(child) && ok;
        else if (rb->remove(child) < 0)
            ok = false;
    }

    rb->closedir(dir);
    return (rb->rmdir(path) == 0) && ok;
}

static void photos_delete_thumbs_for(const char *path, bool is_dir)
{
    char thumb_path[MAX_PATH];
    char relpath[MAX_PATH];

    if (is_dir)
    {
        if (!photos_relpath(path, relpath, sizeof(relpath)) || relpath[0] == '\0')
            return;

        rb->snprintf(thumb_path, sizeof(thumb_path), "%s/%s",
                     PHOTOS_THUMB_DIR, relpath);
        photos_delete_tree(thumb_path);
    }
    else
    {
        photos_thumb_path(path, thumb_path, sizeof(thumb_path));
        if (thumb_path[0] != '\0')
            rb->remove(thumb_path);
    }
}

static bool photos_delete_selected(void)
{
    struct photo_entry *entry;
    char delete_path[MAX_PATH];
    bool is_dir;

    if (photos.entry_count <= 0)
        return false;

    entry = &photos.entries[photos.selected];
    if (photos_path_is_root(entry->path))
        return false;

    if (!photos_unlock_if_needed(entry->path))
        return false;

    if (!rb->yesno_pop_confirm(entry->is_dir ?
                               "Delete this folder?" :
                               "Delete this photo?"))
        return false;

    rb->strlcpy(delete_path, entry->path, sizeof(delete_path));
    is_dir = entry->is_dir;

    photos.selected_path[0] = '\0';
    if (photos.selected + 1 < photos.entry_count)
        rb->strlcpy(photos.selected_path, photos.entries[photos.selected + 1].path,
                    sizeof(photos.selected_path));
    else if (photos.selected > 0)
        rb->strlcpy(photos.selected_path, photos.entries[photos.selected - 1].path,
                    sizeof(photos.selected_path));

    if (!photos_delete_tree(delete_path))
    {
        rb->splash(HZ * 2, "Delete failed");
        photos_rescan_current_dir();
        return false;
    }

    photos_delete_thumbs_for(delete_path, is_dir);
    photos_remove_locks_under(delete_path);
    photos_save_locks();
    photos_rescan_current_dir();
    return true;
}

static int compare_move_dirs(const void *left, const void *right)
{
    const struct photo_move_dir *a = left;
    const struct photo_move_dir *b = right;

    if (photos_path_is_root(a->path))
        return photos_path_is_root(b->path) ? 0 : -1;
    if (photos_path_is_root(b->path))
        return 1;

    return rb->strcasecmp(a->path, b->path);
}

static void photos_add_move_dir(const char *path)
{
    if (photo_move_dir_count >= MAX_MOVE_DIRS)
        return;

    rb->strlcpy(photo_move_dirs[photo_move_dir_count].path, path,
                sizeof(photo_move_dirs[photo_move_dir_count].path));
    photo_move_dir_count++;
}

static void photos_collect_move_dirs(const char *path)
{
    DIR *dir;
    struct dirent *entry;

    photos_add_move_dir(path);
    if (photo_move_dir_count >= MAX_MOVE_DIRS)
        return;

    dir = rb->opendir(path);
    if (!dir)
        return;

    while ((entry = rb->readdir(dir)) != NULL &&
           photo_move_dir_count < MAX_MOVE_DIRS)
    {
        struct dirinfo info;
        char child[MAX_PATH];

        if (!rb->strcmp(entry->d_name, ".") || !rb->strcmp(entry->d_name, ".."))
            continue;
        if (photos_is_hidden(entry->d_name))
            continue;

        rb->snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        info = rb->dir_get_info(dir, entry);
        if (info.attribute & ATTR_DIRECTORY)
            photos_collect_move_dirs(child);
    }

    rb->closedir(dir);
}

static const char *photos_move_dir_name(int selected_item, void *data,
                                        char *buffer, size_t buffer_len)
{
    char relpath[MAX_PATH];
    (void)data;

    if (selected_item < 0 || selected_item >= photo_move_dir_count)
        return "";

    if (photos_relpath(photo_move_dirs[selected_item].path, relpath,
                       sizeof(relpath)) && relpath[0] != '\0')
    {
        rb->snprintf(buffer, buffer_len, "Photos/%s", relpath);
        return buffer;
    }

    rb->strlcpy(buffer, "Photos", buffer_len);
    return buffer;
}

static enum themable_icons photos_move_dir_icon(int selected_item, void *data)
{
    (void)selected_item;
    (void)data;
    return Icon_Folder;
}

static bool photos_pick_move_destination(char *out, size_t out_size)
{
    struct gui_synclist list;
    int action;
    int selected = 0;
    bool done = false;
    bool picked = false;

    photo_move_dir_count = 0;
    photos_collect_move_dirs(PHOTOS_ROOT);
    if (photo_move_dir_count <= 0)
        return false;

    if (photo_move_dir_count > 1)
    {
        rb->qsort(photo_move_dirs, photo_move_dir_count,
                  sizeof(struct photo_move_dir), compare_move_dirs);
    }

    rb->gui_synclist_init(&list, photos_move_dir_name, NULL, false, 1, NULL);
    rb->gui_synclist_set_icon_callback(&list, photos_move_dir_icon);
    rb->gui_synclist_set_nb_items(&list, photo_move_dir_count);
    rb->gui_synclist_set_title(&list, "Move to Folder", Icon_Folder);
    rb->gui_synclist_select_item(&list, selected);
    rb->gui_synclist_draw(&list);

    while (!done)
    {
        action = rb->get_action(CONTEXT_LIST, HZ / 10);
        if (rb->gui_synclist_do_button(&list, &action))
            continue;

        switch (action)
        {
            case ACTION_STD_OK:
                selected = rb->gui_synclist_get_sel_pos(&list);
                rb->strlcpy(out, photo_move_dirs[selected].path, out_size);
                picked = true;
                done = true;
                break;

            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                done = true;
                break;

            default:
                if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                    done = true;
                break;
        }
    }

    return picked;
}

static bool photos_move_selected_to_folder(void)
{
    struct photo_entry *entry;
    char destination[MAX_PATH];
    char current_parent[MAX_PATH];
    char newpath[MAX_PATH];
    char old_thumb[MAX_PATH];
    char new_thumb[MAX_PATH];

    if (photos.entry_count <= 0)
        return false;

    entry = &photos.entries[photos.selected];
    if (entry->is_dir)
        return false;

    if (!photos_unlock_if_needed(entry->path))
        return false;

    if (!photos_pick_move_destination(destination, sizeof(destination)))
        return false;

    photos_dirname(entry->path, current_parent, sizeof(current_parent));
    if (!rb->strcmp(current_parent, destination))
    {
        rb->splash(HZ, "Already there");
        return false;
    }

    rb->snprintf(newpath, sizeof(newpath), "%s/%s",
                 destination, photos_basename(entry->path));
    if (rb->file_exists(newpath) || rb->dir_exists(newpath))
    {
        rb->splash(HZ * 2, "Name exists");
        return false;
    }

    if (rb->rename(entry->path, newpath) < 0)
    {
        rb->splash(HZ * 2, "Move failed");
        return false;
    }

    photos_thumb_path(entry->path, old_thumb, sizeof(old_thumb));
    photos_thumb_path(newpath, new_thumb, sizeof(new_thumb));
    if (old_thumb[0] != '\0' && new_thumb[0] != '\0' &&
        rb->file_exists(old_thumb))
    {
        photos_ensure_parent_dirs(new_thumb);
        rb->rename(old_thumb, new_thumb);
    }

    photos_update_lock_paths(entry->path, newpath);
    photos_save_locks();
    rb->strlcpy(photos.selected_path, newpath, sizeof(photos.selected_path));
    photos_rescan_current_dir();
    rb->splash(HZ, "Moved");
    return true;
}

static enum plugin_status photos_open_context_menu(void)
{
    int selection = 0;

    if (photos.entry_count <= 0)
    {
        MENUITEM_STRINGLIST(empty_menu, "Photos", NULL,
                            "New Folder", "Refresh", "Back");
        switch (rb->do_menu(&empty_menu, &selection, NULL, false))
        {
            case 0:
                photos_create_folder();
                break;
            default:
                break;
        }
        return PLUGIN_OK;
    }

    {
        struct photo_entry *entry = &photos.entries[photos.selected];

        if (entry->is_dir && entry->locked)
        {
            MENUITEM_STRINGLIST(menu, "Folder", NULL,
                                "Open", "Rename", "Delete", "Unlock",
                                "New Folder", "Refresh", "Back");
            switch (rb->do_menu(&menu, &selection, NULL, false))
            {
                case 0: return photos_open_entry(entry);
                case 1: photos_rename_selected(); break;
                case 2: photos_delete_selected(); break;
                case 3: photos_unlock_selected(); break;
                case 4: photos_create_folder(); break;
                default: break;
            }
            return PLUGIN_OK;
        }

        if (entry->is_dir)
        {
            MENUITEM_STRINGLIST(menu, "Folder", NULL,
                                "Open", "Rename", "Delete", "Lock",
                                "New Folder", "Refresh", "Back");
            switch (rb->do_menu(&menu, &selection, NULL, false))
            {
                case 0: return photos_open_entry(entry);
                case 1: photos_rename_selected(); break;
                case 2: photos_delete_selected(); break;
                case 3: photos_lock_selected(); break;
                case 4: photos_create_folder(); break;
                default: break;
            }
            return PLUGIN_OK;
        }

        if (entry->locked)
        {
            MENUITEM_STRINGLIST(menu, "Photo", NULL,
                                "View", "Rename", "Move to Folder",
                                "Delete", "Unlock", "New Folder",
                                "Refresh", "Back");
            switch (rb->do_menu(&menu, &selection, NULL, false))
            {
                case 0: return photos_open_entry(entry);
                case 1: photos_rename_selected(); break;
                case 2: photos_move_selected_to_folder(); break;
                case 3: photos_delete_selected(); break;
                case 4: photos_unlock_selected(); break;
                case 5: photos_create_folder(); break;
                default: break;
            }
            return PLUGIN_OK;
        }

        MENUITEM_STRINGLIST(menu, "Photo", NULL,
                            "View", "Rename", "Move to Folder",
                            "Delete", "Lock", "New Folder",
                            "Refresh", "Back");
        switch (rb->do_menu(&menu, &selection, NULL, false))
        {
            case 0: return photos_open_entry(entry);
            case 1: photos_rename_selected(); break;
            case 2: photos_move_selected_to_folder(); break;
            case 3: photos_delete_selected(); break;
            case 4: photos_lock_selected(); break;
            case 5: photos_create_folder(); break;
            default: break;
        }
    }

    return PLUGIN_OK;
}

static enum plugin_status photos_grid_loop(void)
{
#ifdef PHOTOS_IPOD_CONTROLS
    static const struct button_mapping *plugin_contexts[] = {
        photos_ipod_ctx,
    };
#else
    static const struct button_mapping *plugin_contexts[] = {
        pla_main_ctx,
    };
#endif

    photos_ensure_selection_visible();
    photos_draw_screen();

    while (true)
    {
        int action = pluginlib_getaction(TIMEOUT_BLOCK, plugin_contexts,
                                         ARRAYLEN(plugin_contexts));

        switch (action)
        {
#ifdef HAVE_SCROLLWHEEL
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                photos_move_selection(-1);
                photos_draw_screen();
                break;

            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                photos_move_selection(1);
                photos_draw_screen();
                break;

            case PLA_UP:
            case PLA_DOWN:
            case PLA_DOWN_REPEAT:
            case PLA_LEFT:
            case PLA_LEFT_REPEAT:
                break;

            case PLA_RIGHT:
            case PLA_RIGHT_REPEAT:
                break;
#else
            case PLA_LEFT:
            case PLA_LEFT_REPEAT:
                photos_move_selection(-1);
                photos_draw_screen();
                break;

            case PLA_RIGHT:
            case PLA_RIGHT_REPEAT:
                photos_move_selection(1);
                photos_draw_screen();
                break;

            case PLA_UP:
                photos_move_selection(-photos.visible_count);
                photos_draw_screen();
                break;

            case PLA_DOWN:
            case PLA_DOWN_REPEAT:
                photos_move_selection(photos.visible_count);
                photos_draw_screen();
                break;
#endif

            case PLA_UP_REPEAT:
            {
                enum plugin_status status = photos_open_context_menu();
                if (status != PLUGIN_OK)
                    return status;
                photos_draw_screen();
                break;
            }

            case PLA_SELECT:
            case PLA_SELECT_REL:
                if (photos.entry_count > 0)
                {
                    enum plugin_status status =
                        photos_open_entry(&photos.entries[photos.selected]);
                    if (status != PLUGIN_OK)
                        return status;
                    photos_draw_screen();
                }
                break;

            case PLA_CANCEL:
            case ACTION_STD_CANCEL:
                if (!photos_path_is_root(photos.current_dir))
                {
                    photos_dirname(photos.current_dir, photos.current_dir,
                                   sizeof(photos.current_dir));
                    photos.selected_path[0] = '\0';
                    photos_rescan_current_dir();
                    photos_draw_screen();
                }
                else
                {
                    return PLUGIN_OK;
                }
                break;

            case PLA_EXIT:
                return PLUGIN_OK;
        }

        if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    const char *path = parameter;
    bool resume = (parameter == rb->plugin_tsr);

    if (path && !resume && path[0] && rb->file_exists(path))
        return rb->plugin_open(PHOTOS_VIEWER, path);

    if (!rb->dir_exists(PHOTOS_ROOT))
    {
        rb->splash(HZ * 2, "Create /Photos");
        return PLUGIN_OK;
    }

    if (!photos_allocate_buffers())
    {
        rb->splash(HZ * 2, "Photos buffer failed");
        return PLUGIN_ERROR;
    }

    photos_load_locks();
    rb->strlcpy(photos.current_dir, PHOTOS_ROOT, sizeof(photos.current_dir));

    if (resume)
        photos_restore_resume_state();
    else
        photos_clear_resume_state();

    if (!rb->dir_exists(photos.current_dir))
        rb->strlcpy(photos.current_dir, PHOTOS_ROOT, sizeof(photos.current_dir));

    photos_rescan_current_dir();
    if (photos.entry_count <= 0)
        photos_draw_screen();

    return photos_grid_loop();
}
