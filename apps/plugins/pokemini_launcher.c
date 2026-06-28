/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/                \/
 ****************************************************************************/

#include "plugin.h"
#define POKEMINI_ROM_DIR "/PokeMini"
#define POKEMINI_VIEWER VIEWERS_DIR "/pokemini.rock"
#define POKEMINI_COVER_DIR PLUGIN_GAMES_DATA_DIR "/pokemini_launcher/covers"
#define POKEMINI_COVER_LOGO_DIR POKEMINI_COVER_DIR "/logos"
#define POKEMINI_COVER_TITLE_DIR POKEMINI_COVER_DIR "/titles"
#define MAX_GAMES 96
#define MAX_COVER_VARIANTS 6
#define COVER_W 92
#define COVER_H 128
#define LIST_X 6
#define LIST_Y 22
#define LIST_W 198
#define COVER_X 218
#define COVER_Y 28
#define ROW_H 13

struct game_entry
{
    char name[MAX_FILENAME];
    char path[MAX_PATH];
};

static struct game_entry games[MAX_GAMES];
static int game_count;
static fb_data cover_data[COVER_W * COVER_H];
static struct bitmap cover_bitmap;
static int loaded_cover = -1;
static bool cover_valid;

static bool has_min_ext(const char *name)
{
    const char *ext = rb->strrchr(name, '.');
    return ext && !rb->strcasecmp(ext, ".min");
}

static bool title_ends_with(const char *title, const char *suffix)
{
    size_t title_len = rb->strlen(title);
    size_t suffix_len = rb->strlen(suffix);

    return title_len >= suffix_len &&
        !rb->strcasecmp(title + title_len - suffix_len, suffix);
}

static void copy_title_variant(char *dst, size_t dst_size, const char *src)
{
    rb->strlcpy(dst, src, dst_size);
}

static int build_cover_variants(const char *title, char variants[][MAX_FILENAME])
{
    int count = 0;
    char base[MAX_FILENAME];

    copy_title_variant(variants[count++], MAX_FILENAME, title);

    if (title_ends_with(title, " (GameCube Preview)"))
    {
        rb->strlcpy(base, title, sizeof(base));
        base[rb->strlen(base) - rb->strlen(" (GameCube Preview)")] = '\0';
        copy_title_variant(variants[count++], MAX_FILENAME, base);
    }

    if (title_ends_with(title, " (Preview)"))
    {
        rb->strlcpy(base, title, sizeof(base));
        base[rb->strlen(base) - rb->strlen(" (Preview)")] = '\0';
        copy_title_variant(variants[count++], MAX_FILENAME, base);
    }

    if (title_ends_with(title, " (GameCube)"))
    {
        rb->strlcpy(base, title, sizeof(base));
        base[rb->strlen(base) - rb->strlen(" (GameCube)")] = '\0';
        copy_title_variant(variants[count++], MAX_FILENAME, base);
    }

    if (rb->strstr(title, " - ") != NULL)
    {
        size_t prefix_len = (size_t)(rb->strstr(title, " - ") - title);

        if (prefix_len > 0 && prefix_len < sizeof(base))
        {
            rb->strlcpy(base, title, sizeof(base));
            base[prefix_len] = '\0';
            copy_title_variant(variants[count++], MAX_FILENAME, base);
        }
    }

    if (count > MAX_COVER_VARIANTS)
        count = MAX_COVER_VARIANTS;

    return count;
}

static bool try_load_cover_path(const char *path)
{
    int rc;

    if (!rb->file_exists(path))
        return false;

    cover_bitmap.width = COVER_W;
    cover_bitmap.height = COVER_H;
    cover_bitmap.data = (unsigned char *)cover_data;
    cover_bitmap.format = FORMAT_NATIVE;

    rc = rb->read_bmp_file(path, &cover_bitmap, sizeof(cover_data),
                           FORMAT_NATIVE, NULL);
    return rc >= 0;
}

static int compare_games(const void *a, const void *b)
{
    const struct game_entry *ga = a;
    const struct game_entry *gb = b;
    return rb->strcasecmp(ga->name, gb->name);
}

static void strip_min_ext(char *name)
{
    char *ext = rb->strrchr(name, '.');
    if (ext && !rb->strcasecmp(ext, ".min"))
        *ext = '\0';
}

static bool scan_games(void)
{
    DIR *dir;
    struct dirent *entry;

    game_count = 0;
    dir = rb->opendir(POKEMINI_ROM_DIR);
    if (!dir)
        return false;

    while ((entry = rb->readdir(dir)) != NULL && game_count < MAX_GAMES)
    {
        if (entry->d_name[0] == '[')
            continue;
        if (!has_min_ext(entry->d_name))
            continue;

        rb->strlcpy(games[game_count].name, entry->d_name,
                    sizeof(games[game_count].name));
        strip_min_ext(games[game_count].name);
        rb->snprintf(games[game_count].path, sizeof(games[game_count].path),
                     "%s/%s", POKEMINI_ROM_DIR, entry->d_name);
        game_count++;
    }

    rb->closedir(dir);

    rb->qsort(games, game_count, sizeof(games[0]), compare_games);
    return true;
}

static void text_fit(char *dst, size_t dst_size, const char *src, int max_width)
{
    int w;
    int h;
    size_t len;

    rb->strlcpy(dst, src, dst_size);
    rb->lcd_getstringsize(dst, &w, &h);
    if (w <= max_width)
        return;

    len = rb->strlen(dst);
    while (len > 3)
    {
        dst[--len] = '\0';
        rb->strcat(dst, "...");
        rb->lcd_getstringsize(dst, &w, &h);
        if (w <= max_width)
            return;
        dst[len] = '\0';
    }
}

static void load_cover(int selected)
{
    char variants[MAX_COVER_VARIANTS][MAX_FILENAME];
    int variant_count;
    int i;
    static const char *cover_dirs[] = {
        POKEMINI_COVER_DIR,
        POKEMINI_COVER_LOGO_DIR,
        POKEMINI_COVER_TITLE_DIR,
    };

    if (loaded_cover == selected)
        return;

    loaded_cover = selected;
    cover_valid = false;
    rb->memset(cover_data, 0, sizeof(cover_data));

    variant_count = build_cover_variants(games[selected].name, variants);
    for (i = 0; i < variant_count && !cover_valid; i++)
    {
        int dir;
        char cover_path[MAX_PATH];

        for (dir = 0; dir < (int)ARRAYLEN(cover_dirs); dir++)
        {
            rb->snprintf(cover_path, sizeof(cover_path), "%s/%s.bmp",
                         cover_dirs[dir], variants[i]);
            if (try_load_cover_path(cover_path))
            {
                cover_valid = true;
                return;
            }
        }
    }
}

static void draw_cover_placeholder(void)
{
    rb->lcd_set_foreground(LCD_RGBPACK(80, 84, 88));
    rb->lcd_drawrect(COVER_X, COVER_Y, COVER_W, COVER_H);
    rb->lcd_set_foreground(LCD_RGBPACK(180, 184, 188));
    rb->lcd_putsxy(COVER_X + 14, COVER_Y + 56, "No cover");
}

static void draw_launcher(int selected)
{
    int i;
    int start;
    int rows = (LCD_HEIGHT - LIST_Y - 8) / ROW_H;
    char line[MAX_FILENAME];

    load_cover(selected);

    if (selected < rows / 2)
        start = 0;
    else if (selected > game_count - rows / 2)
        start = game_count - rows;
    else
        start = selected - rows / 2;

    if (start < 0)
        start = 0;

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();

    rb->lcd_putsxy(LIST_X, 4, "PokeMini");
    rb->lcd_set_foreground(LCD_RGBPACK(90, 94, 100));
    rb->lcd_hline(0, LCD_WIDTH - 1, 17);

    for (i = 0; i < rows && start + i < game_count; i++)
    {
        int idx = start + i;
        int y = LIST_Y + i * ROW_H;

        if (idx == selected)
        {
            rb->lcd_set_foreground(LCD_RGBPACK(64, 112, 176));
            rb->lcd_fillrect(2, y - 1, LIST_W + 6, ROW_H);
            rb->lcd_set_foreground(LCD_WHITE);
        }
        else
        {
            rb->lcd_set_foreground(LCD_RGBPACK(210, 214, 218));
        }

        text_fit(line, sizeof(line), games[idx].name, LIST_W);
        rb->lcd_putsxy(LIST_X, y, line);
    }

    rb->lcd_set_foreground(LCD_RGBPACK(36, 40, 45));
    rb->lcd_fillrect(COVER_X - 6, COVER_Y - 6, COVER_W + 12, COVER_H + 12);

    if (cover_valid)
    {
        int x = COVER_X + (COVER_W - cover_bitmap.width) / 2;
        int y = COVER_Y + (COVER_H - cover_bitmap.height) / 2;
        rb->lcd_bitmap((fb_data *)cover_bitmap.data, x, y,
                       cover_bitmap.width, cover_bitmap.height);
    }
    else
    {
        draw_cover_placeholder();
    }

    rb->lcd_set_foreground(LCD_RGBPACK(210, 214, 218));
    text_fit(line, sizeof(line), games[selected].name, LCD_WIDTH - COVER_X);
    rb->lcd_putsxy(COVER_X - 6, COVER_Y + COVER_H + 10, line);

    rb->lcd_update();
}

enum plugin_status plugin_start(const void *parameter)
{
    int selected = 0;
    bool redraw = true;

    (void)parameter;

    if (!scan_games())
    {
        rb->splash(HZ * 2, "No /PokeMini folder");
        return PLUGIN_OK;
    }

    if (game_count <= 0)
    {
        rb->splash(HZ * 2, "No .min games");
        return PLUGIN_OK;
    }

    rb->lcd_setfont(FONT_SYSFIXED);

    while (true)
    {
        int action;

        if (redraw)
        {
            draw_launcher(selected);
            redraw = false;
        }

        action = rb->get_action(CONTEXT_LIST, TIMEOUT_BLOCK);
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (selected > 0)
                {
                    selected--;
                    redraw = true;
                }
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (selected + 1 < game_count)
                {
                    selected++;
                    redraw = true;
                }
                break;

            case ACTION_STD_OK:
                return rb->plugin_open(POKEMINI_VIEWER,
                                       games[selected].path);

            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                return PLUGIN_OK;

            default:
                if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
                break;
        }
    }

    return PLUGIN_OK;
}
