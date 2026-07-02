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
#define MAX_COVER_VARIANTS 16
#define COVER_W 96
#define COVER_H 134
#define HEADER_H 20
#define FOOTER_H 16
#define PANE_X 205
#define LIST_X 7
#define LIST_Y 25
#define LIST_W (PANE_X - LIST_X - 12)
#define ROW_H 16
#define KEY_SIZE 96

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

static bool add_cover_variant(char variants[][MAX_FILENAME], int *count,
                              const char *title)
{
    int i;

    if (!title || !title[0] || *count >= MAX_COVER_VARIANTS)
        return false;

    for (i = 0; i < *count; i++)
    {
        if (!rb->strcasecmp(variants[i], title))
            return false;
    }

    copy_title_variant(variants[*count], MAX_FILENAME, title);
    (*count)++;
    return true;
}

static void strip_trailing_parenthetical(char *title)
{
    size_t len = rb->strlen(title);

    while (len > 0)
    {
        char *open;

        while (len > 0 && title[len - 1] == ' ')
            title[--len] = '\0';

        if (len < 3 || title[len - 1] != ')')
            return;

        open = rb->strrchr(title, '(');
        if (!open || open == title || open[-1] != ' ')
            return;

        open[-1] = '\0';
        len = rb->strlen(title);
    }
}

static int build_cover_variants(const char *title, char variants[][MAX_FILENAME])
{
    int count = 0;
    char base[MAX_FILENAME];

    add_cover_variant(variants, &count, title);

    if (title_ends_with(title, " (GameCube Preview)"))
    {
        rb->strlcpy(base, title, sizeof(base));
        base[rb->strlen(base) - rb->strlen(" (GameCube Preview)")] = '\0';
        add_cover_variant(variants, &count, base);
    }

    if (title_ends_with(title, " (Preview)"))
    {
        rb->strlcpy(base, title, sizeof(base));
        base[rb->strlen(base) - rb->strlen(" (Preview)")] = '\0';
        add_cover_variant(variants, &count, base);
    }

    if (title_ends_with(title, " (GameCube)"))
    {
        rb->strlcpy(base, title, sizeof(base));
        base[rb->strlen(base) - rb->strlen(" (GameCube)")] = '\0';
        add_cover_variant(variants, &count, base);
    }

    rb->strlcpy(base, title, sizeof(base));
    strip_trailing_parenthetical(base);
    add_cover_variant(variants, &count, base);

    if (rb->strstr(base, " - ") != NULL)
    {
        char parent[MAX_FILENAME];
        size_t prefix_len = (size_t)(rb->strstr(base, " - ") - base);

        if (prefix_len > 0 && prefix_len < sizeof(parent))
        {
            rb->strlcpy(parent, base, sizeof(parent));
            parent[prefix_len] = '\0';
            add_cover_variant(variants, &count, parent);
        }
    }

    if (rb->strstr(title, " - ") != NULL)
    {
        size_t prefix_len = (size_t)(rb->strstr(title, " - ") - title);

        if (prefix_len > 0 && prefix_len < sizeof(base))
        {
            rb->strlcpy(base, title, sizeof(base));
            base[prefix_len] = '\0';
            add_cover_variant(variants, &count, base);
        }
    }

    return count;
}

static bool has_bmp_ext(const char *name)
{
    const char *ext = rb->strrchr(name, '.');
    return ext && !rb->strcasecmp(ext, ".bmp");
}

static void strip_bmp_ext(char *name)
{
    size_t len = rb->strlen(name);

    if (len > rb->strlen(".pane.bmp") &&
        !rb->strcasecmp(name + len - rb->strlen(".pane.bmp"), ".pane.bmp"))
    {
        name[len - rb->strlen(".pane.bmp")] = '\0';
        return;
    }

    if (len > rb->strlen(".bmp") &&
        !rb->strcasecmp(name + len - rb->strlen(".bmp"), ".bmp"))
        name[len - rb->strlen(".bmp")] = '\0';
}

static char ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

static void normalize_cover_key(char *dst, size_t dst_size, const char *src)
{
    size_t out = 0;
    int paren_depth = 0;

    if (dst_size == 0)
        return;

    while (*src && out + 1 < dst_size)
    {
        char c = *src++;

        if (c == '(')
        {
            paren_depth++;
            continue;
        }
        if (c == ')' && paren_depth > 0)
        {
            paren_depth--;
            continue;
        }
        if (paren_depth > 0)
            continue;

        c = ascii_lower(c);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            dst[out++] = c;
    }

    dst[out] = '\0';
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
                           FORMAT_NATIVE | FORMAT_RESIZE |
                           FORMAT_KEEP_ASPECT | FORMAT_DITHER, NULL);
    if (rc >= 0)
        return true;

    return false;
}

static bool try_load_cover_name(const char *dir, const char *name)
{
    char cover_path[MAX_PATH];

    rb->snprintf(cover_path, sizeof(cover_path), "%s/%s.bmp", dir, name);
    if (try_load_cover_path(cover_path))
        return true;

    rb->snprintf(cover_path, sizeof(cover_path), "%s/%s.pane.bmp", dir, name);
    return try_load_cover_path(cover_path);
}

static bool try_load_matching_cover(const char *dir, const char *title)
{
    DIR *dirp;
    struct dirent *entry;
    char wanted[KEY_SIZE];
    char file_key[KEY_SIZE];
    char stem[MAX_FILENAME];
    char cover_path[MAX_PATH];
    bool found = false;

    normalize_cover_key(wanted, sizeof(wanted), title);
    if (!wanted[0])
        return false;

    dirp = rb->opendir(dir);
    if (!dirp)
        return false;

    while ((entry = rb->readdir(dirp)) != NULL)
    {
        if (!has_bmp_ext(entry->d_name))
            continue;

        rb->strlcpy(stem, entry->d_name, sizeof(stem));
        strip_bmp_ext(stem);
        normalize_cover_key(file_key, sizeof(file_key), stem);
        if (rb->strcmp(file_key, wanted))
            continue;

        rb->snprintf(cover_path, sizeof(cover_path), "%s/%s",
                     dir, entry->d_name);
        found = try_load_cover_path(cover_path);
        if (found)
            break;
    }

    rb->closedir(dirp);
    return found;
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
            (void)cover_path;
            if (try_load_cover_name(cover_dirs[dir], variants[i]))
            {
                cover_valid = true;
                return;
            }
        }
    }

    for (i = 0; i < variant_count && !cover_valid; i++)
    {
        int dir;

        for (dir = 0; dir < (int)ARRAYLEN(cover_dirs); dir++)
        {
            if (try_load_matching_cover(cover_dirs[dir], variants[i]))
            {
                cover_valid = true;
                return;
            }
        }
    }
}

static int mix_channel(int a, int b, int pos, int den)
{
    if (den <= 0)
        return b;
    return a + ((b - a) * pos) / den;
}

static void gradient_rect(int x, int y, int w, int h,
                          int r1, int g1, int b1,
                          int r2, int g2, int b2)
{
    int row;

    if (h <= 0 || w <= 0)
        return;

    for (row = 0; row < h; row++)
    {
        int den = h > 1 ? h - 1 : 1;
        rb->lcd_set_foreground(LCD_RGBPACK(mix_channel(r1, r2, row, den),
                                           mix_channel(g1, g2, row, den),
                                           mix_channel(b1, b2, row, den)));
        rb->lcd_hline(x, x + w - 1, y + row);
    }
}

static void draw_text_transparent(int x, int y, const char *text,
                                  unsigned color)
{
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(color);
    rb->lcd_putsxy(x, y, text);
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static void draw_chevron(int x, int y, unsigned color)
{
    rb->lcd_set_foreground(color);
    rb->lcd_vline(x, y + 2, y + 6);
    rb->lcd_vline(x + 1, y + 3, y + 5);
    rb->lcd_drawpixel(x + 2, y + 4);
}

static void draw_scrollbar(int selected, int rows)
{
    int track_x = PANE_X - 5;
    int track_y = LIST_Y;
    int track_h = LCD_HEIGHT - LIST_Y - FOOTER_H - 3;
    int thumb_h;
    int thumb_y;

    if (game_count <= rows || track_h <= 0)
        return;

    thumb_h = (rows * track_h) / game_count;
    if (thumb_h < 16)
        thumb_h = 16;
    if (thumb_h > track_h)
        thumb_h = track_h;

    thumb_y = track_y + (selected * (track_h - thumb_h)) /
              (game_count - 1);

    rb->lcd_set_foreground(LCD_RGBPACK(222, 224, 227));
    rb->lcd_vline(track_x, track_y, track_y + track_h - 1);
    gradient_rect(track_x - 1, thumb_y, 3, thumb_h,
                  166, 171, 178, 116, 122, 130);
}

static void draw_cover_placeholder(int x, int y)
{
    gradient_rect(x, y, COVER_W, COVER_H,
                  246, 247, 248, 212, 215, 219);
    rb->lcd_set_foreground(LCD_RGBPACK(156, 162, 170));
    rb->lcd_drawrect(x, y, COVER_W, COVER_H);
    draw_text_transparent(x + 22, y + 58, "No Cover",
                          LCD_RGBPACK(104, 108, 114));
}

static void draw_launcher(int selected)
{
    int i;
    int start;
    int rows = (LCD_HEIGHT - LIST_Y - FOOTER_H - 3) / ROW_H;
    int pane_w = LCD_WIDTH - PANE_X;
    int cover_x = PANE_X + (pane_w - COVER_W) / 2;
    int cover_y = 44;
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

    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();

    gradient_rect(0, 0, LCD_WIDTH, HEADER_H,
                  252, 253, 253, 183, 188, 194);
    rb->lcd_set_foreground(LCD_RGBPACK(128, 134, 142));
    rb->lcd_hline(0, LCD_WIDTH - 1, HEADER_H - 1);
    rb->lcd_set_foreground(LCD_RGBPACK(210, 213, 217));
    rb->lcd_vline(PANE_X - 1, HEADER_H, LCD_HEIGHT - 1);

    gradient_rect(PANE_X, HEADER_H, pane_w, LCD_HEIGHT - HEADER_H,
                  250, 251, 252, 226, 229, 233);

    draw_text_transparent(8, 5, "iPod", LCD_RGBPACK(24, 24, 24));
    draw_text_transparent(PANE_X + 8, 5, "PokeMini",
                          LCD_RGBPACK(24, 24, 24));

    for (i = 0; i < rows && start + i < game_count; i++)
    {
        int idx = start + i;
        int y = LIST_Y + i * ROW_H;
        int row_x = 0;
        int row_w = PANE_X - 1;

        if (idx == selected)
        {
            gradient_rect(row_x, y - 2, row_w, ROW_H,
                          107, 200, 254, 0, 92, 192);
        }
        else
        {
            rb->lcd_set_foreground(LCD_RGBPACK(232, 234, 236));
            rb->lcd_hline(LIST_X, PANE_X - 10, y + ROW_H - 3);
        }

        text_fit(line, sizeof(line), games[idx].name, LIST_W);
        draw_text_transparent(LIST_X, y, line,
                              idx == selected ? LCD_WHITE :
                              LCD_RGBPACK(16, 16, 16));
        if (idx == selected)
            draw_chevron(PANE_X - 14, y + 2, LCD_WHITE);
    }

    draw_scrollbar(selected, rows);

    rb->lcd_set_foreground(LCD_RGBPACK(188, 193, 199));
    rb->lcd_fillrect(cover_x + 5, cover_y + 6, COVER_W, COVER_H);
    rb->lcd_set_foreground(LCD_RGBPACK(142, 148, 156));
    rb->lcd_drawrect(cover_x - 1, cover_y - 1, COVER_W + 2, COVER_H + 2);

    if (cover_valid)
    {
        int x = cover_x + (COVER_W - cover_bitmap.width) / 2;
        int y = cover_y + (COVER_H - cover_bitmap.height) / 2;
        rb->lcd_bitmap((fb_data *)cover_bitmap.data, x, y,
                       cover_bitmap.width, cover_bitmap.height);
    }
    else
    {
        draw_cover_placeholder(cover_x, cover_y);
    }

    text_fit(line, sizeof(line), games[selected].name, pane_w - 12);
    draw_text_transparent(PANE_X + 6, cover_y + COVER_H + 10, line,
                          LCD_RGBPACK(36, 39, 43));

    gradient_rect(0, LCD_HEIGHT - FOOTER_H, LCD_WIDTH, FOOTER_H,
                  240, 242, 244, 214, 218, 222);
    rb->lcd_set_foreground(LCD_RGBPACK(180, 185, 192));
    rb->lcd_hline(0, LCD_WIDTH - 1, LCD_HEIGHT - FOOTER_H);
    rb->snprintf(line, sizeof(line), "%d games", game_count);
    draw_text_transparent(8, LCD_HEIGHT - FOOTER_H + 4, line,
                          LCD_RGBPACK(84, 88, 94));

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
