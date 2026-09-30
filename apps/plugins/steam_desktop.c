/***************************************************************************
 * Windowed Steam-style launcher for the native 320x240 Desktop Mode.
 * Reads only existing Rockbox game indexes and launches installed plugins.
 ****************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "lib/pluginlib_bmp.h"

static const struct button_mapping *plugin_contexts[] = { pla_main_ctx };

#define ST_UNDERLAY_FILE \
    PLUGIN_APPS_DATA_DIR "/desktop_mode_steam_underlay.raw"
#define ST_UNDERLAY_MAGIC 0x44535431u /* "DST1" */
#define ST_WINDOW_FILE \
    PLUGIN_APPS_DATA_DIR \
    "/desktop_mode_snow_leopard/320x240/chrome/" \
    "window-plain.304x174x16.bmp"
#define ST_LOGO_FILE ROCKBOX_DIR "/ipodjs/steam/steam-logo-official.110x32x24.bmp"
#define ST_NIBIRU_COVER \
    "/ScummVM/nibiru/rockpod/steam/nibiru-cover.144x108.bmp"

#define ST_WIN_X 8
#define ST_WIN_Y 21
#define ST_WIN_W 304
#define ST_WIN_H 174
#define ST_MAX_GAMES 96
#define ST_TITLE 64
#define ST_META 48
#define ST_PLUGIN 112
#define ST_COVER_W 144
#define ST_COVER_H 108
#define ST_COVER_RESIZE_SCRATCH (32 * 1024)

#define ST_HEADER LCD_RGBPACK(23, 26, 33)
#define ST_BODY LCD_RGBPACK(27, 40, 56)
#define ST_PANEL LCD_RGBPACK(42, 71, 94)
#define ST_BLUE LCD_RGBPACK(102, 192, 244)
#define ST_TEXT LCD_RGBPACK(199, 213, 224)
#define ST_MUTED LCD_RGBPACK(143, 152, 160)
#define ST_GREEN LCD_RGBPACK(88, 138, 27)

struct st_game
{
    char title[ST_TITLE];
    char platform[ST_META];
    char genre[ST_META];
    char plugin[ST_PLUGIN];
    char parameter[MAX_PATH];
    char cover[MAX_PATH];
    int year;
};

static struct st_game st_games[ST_MAX_GAMES];
static int st_count;
static int st_selected;
static fb_data *st_underlay;
static fb_data *st_window;
static fb_data *st_logo;
static fb_data *st_cover;
static bool st_logo_valid;
static bool st_cover_valid;
static unsigned char *st_arena;
static size_t st_left;

static void *st_alloc(size_t bytes)
{
    size_t aligned = ALIGN_UP(bytes, 4);
    void *result;

    if (!st_arena || aligned > st_left)
        return NULL;
    result = st_arena;
    st_arena += aligned;
    st_left -= aligned;
    return result;
}

static bool st_read_all(int fd, void *buffer, size_t size)
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

static bool st_load_bitmap(const char *path, fb_data *pixels,
                           int width, int height)
{
    struct bitmap bitmap;
    size_t bytes = BM_SIZE(width, height, FORMAT_NATIVE, false);
    int format = FORMAT_NATIVE;

    if (pixels == st_cover)
    {
        format |= FORMAT_RESIZE;
        bytes += ST_COVER_RESIZE_SCRATCH;
    }

    rb->memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = width;
    bitmap.height = height;
    bitmap.format = FORMAT_NATIVE;
    bitmap.data = (unsigned char *)pixels;
    return rb->read_bmp_file(path, &bitmap, bytes, format, NULL) > 0 &&
           bitmap.width == width && bitmap.height == height;
}

static bool st_init_desktop(void)
{
    uint32_t header[3];
    size_t size;
    size_t screen_bytes = LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data);
    int fd;

    st_arena = rb->plugin_get_buffer(&size);
    st_left = size;
    st_underlay = st_alloc(screen_bytes);
    st_window = st_alloc(BM_SIZE(ST_WIN_W, ST_WIN_H, FORMAT_NATIVE, false));
    st_logo = st_alloc(BM_SIZE(110, 32, FORMAT_NATIVE, false));
    st_cover = st_alloc(BM_SIZE(ST_COVER_W, ST_COVER_H,
                                FORMAT_NATIVE, false) +
                         ST_COVER_RESIZE_SCRATCH);
    if (!st_underlay || !st_window || !st_logo || !st_cover)
        return false;
    fd = rb->open(ST_UNDERLAY_FILE, O_RDONLY);
    if (fd < 0)
        return false;
    if (!st_read_all(fd, header, sizeof(header)) ||
        header[0] != ST_UNDERLAY_MAGIC || header[1] != LCD_WIDTH ||
        header[2] != LCD_HEIGHT ||
        !st_read_all(fd, st_underlay, screen_bytes))
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    if (!st_load_bitmap(ST_WINDOW_FILE, st_window, ST_WIN_W, ST_WIN_H))
        return false;
    st_logo_valid = st_load_bitmap(ST_LOGO_FILE, st_logo, 110, 32);
    return true;
}

static bool st_duplicate(const char *plugin, const char *parameter)
{
    int index;

    for (index = 0; index < st_count; ++index)
        if (!rb->strcmp(st_games[index].plugin, plugin ? plugin : "") &&
            !rb->strcmp(st_games[index].parameter,
                        parameter ? parameter : ""))
            return true;
    return false;
}

static void st_add(const char *title, const char *platform,
                   const char *genre, const char *plugin,
                   const char *parameter, const char *cover, int year)
{
    struct st_game *game;

    if (st_count >= ST_MAX_GAMES || !title || !title[0] ||
        !cover || !cover[0] || !rb->file_exists(cover) ||
        ((!plugin || !plugin[0]) && (!parameter || !parameter[0])) ||
        (plugin && plugin[0] && !rb->file_exists(plugin)) ||
        (parameter && parameter[0] && !rb->file_exists(parameter)) ||
        st_duplicate(plugin, parameter))
        return;
    game = &st_games[st_count++];
    rb->memset(game, 0, sizeof(*game));
    rb->strlcpy(game->title, title, sizeof(game->title));
    rb->strlcpy(game->platform, platform ? platform : "Game",
                sizeof(game->platform));
    rb->strlcpy(game->genre, genre ? genre : "", sizeof(game->genre));
    rb->strlcpy(game->plugin, plugin ? plugin : "", sizeof(game->plugin));
    rb->strlcpy(game->parameter, parameter ? parameter : "",
                sizeof(game->parameter));
    rb->strlcpy(game->cover, cover, sizeof(game->cover));
    game->year = year;
}

static void st_load_library(void)
{
    st_count = 0;
    /* Keep the desktop library separate without removing existing games. */
    st_add("NiBiRu: Age of Secrets", "PC", "Adventure",
           PLUGIN_APPS_DIR "/scummvm.rock",
           "/ScummVM/nibiru.scummvm", ST_NIBIRU_COVER, 2005);
    st_add("RuneScape Classic", "PC", "Role-playing",
           PLUGIN_GAMES_DIR "/runescape_classic.rock", NULL,
           ROCKBOX_DIR "/games/library/covers/native/runescape_classic.bmp",
           2001);
    st_selected = 0;
    DEBUGF("steam desktop: library count=%d\n", st_count);
}

static void st_text(int x, int y, fb_data colour, const char *text)
{
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(colour);
    rb->lcd_putsxy(x, y, (const unsigned char *)text);
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static void st_draw(void)
{
    char line[96];

    rb->lcd_bitmap(st_underlay, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rb->lcd_bitmap(st_window, ST_WIN_X, ST_WIN_Y, ST_WIN_W, ST_WIN_H);
    rb->lcd_set_foreground(ST_BODY);
    rb->lcd_fillrect(ST_WIN_X + 1, ST_WIN_Y + 24, ST_WIN_W - 2,
                     ST_WIN_H - 24);
    rb->lcd_set_foreground(ST_HEADER);
    rb->lcd_fillrect(ST_WIN_X + 1, ST_WIN_Y + 24, ST_WIN_W - 2, 32);
    if (st_logo_valid)
        rb->lcd_bitmap(st_logo, ST_WIN_X + 8, ST_WIN_Y + 24, 110, 32);
    st_text(ST_WIN_X + 214, ST_WIN_Y + 34, ST_BLUE, "LIBRARY");
    if (st_count > 0)
    {
        const struct st_game *game = &st_games[st_selected];

        st_cover_valid = st_load_bitmap(game->cover, st_cover,
                                        ST_COVER_W, ST_COVER_H);
        rb->lcd_set_foreground(ST_BLUE);
        rb->lcd_fillrect(ST_WIN_X + 9, ST_WIN_Y + 61,
                         ST_COVER_W + 4, ST_COVER_H + 4);
        if (st_cover_valid)
            rb->lcd_bitmap(st_cover, ST_WIN_X + 11, ST_WIN_Y + 63,
                           ST_COVER_W, ST_COVER_H);
        else
        {
            rb->lcd_set_foreground(ST_PANEL);
            rb->lcd_fillrect(ST_WIN_X + 11, ST_WIN_Y + 63,
                             ST_COVER_W, ST_COVER_H);
        }
        st_text(ST_WIN_X + 164, ST_WIN_Y + 64, ST_TEXT, game->title);
        rb->snprintf(line, sizeof(line), "%s%s%d",
                     game->platform, game->year ? "  " : "", game->year);
        st_text(ST_WIN_X + 164, ST_WIN_Y + 84, ST_MUTED, line);
        st_text(ST_WIN_X + 164, ST_WIN_Y + 103, ST_MUTED, game->genre);
        rb->lcd_set_foreground(ST_GREEN);
        rb->lcd_fillrect(ST_WIN_X + 164, ST_WIN_Y + 128, 91, 25);
        st_text(ST_WIN_X + 190, ST_WIN_Y + 134, LCD_RGBPACK(255,255,255),
                "PLAY");
        rb->snprintf(line, sizeof(line), "%d of %d", st_selected + 1,
                     st_count);
        st_text(ST_WIN_X + 164, ST_WIN_Y + 160, ST_MUTED, line);
        st_text(ST_WIN_X + 232, ST_WIN_Y + 160, ST_BLUE, "<  >");
    }
    else
        st_text(ST_WIN_X + 58, ST_WIN_Y + 105, ST_TEXT,
                "No installed games with cover art");
    rb->lcd_update();
}

static int st_launch(void)
{
    const struct st_game *game;
    char viewer[MAX_PATH];
    int attribute;

    if (st_selected < 0 || st_selected >= st_count)
        return PLUGIN_OK;
    game = &st_games[st_selected];
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    if (game->plugin[0])
        return rb->plugin_open(game->plugin,
            !rb->strcmp(game->plugin, PLUGIN_GAMES_DIR "/runescape_classic.rock") ?
            "-desktop" : game->parameter[0] ? game->parameter : NULL);
    attribute = rb->filetype_get_attr(game->parameter);
    if (rb->filetype_get_plugin(attribute, viewer, sizeof(viewer)))
        return rb->plugin_open(viewer, game->parameter);
    rb->splash(HZ * 2, "No emulator for this game");
    return PLUGIN_OK;
}

enum plugin_status plugin_start(const void *parameter)
{
    int result = PLUGIN_OK;
    bool redraw = true;

    rb->lcd_set_viewport(NULL);
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_clear_display();
    if (!parameter || rb->strcmp((const char *)parameter, "-desktop") ||
        !st_init_desktop())
    {
        rb->splash(HZ * 2, "Could not prepare Steam Desktop window");
        return PLUGIN_ERROR;
    }
    st_load_library();
    while (result == PLUGIN_OK)
    {
        int action;

        if (redraw)
        {
            st_draw();
            redraw = false;
        }
        action = pluginlib_getaction(HZ / 20, plugin_contexts,
                                     ARRAYLEN(plugin_contexts));
        switch (action)
        {
            case PLA_LEFT:
            case PLA_LEFT_REPEAT:
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                if (st_count > 0)
                    st_selected = (st_selected + st_count - 1) % st_count;
                redraw = true;
                break;
            case PLA_RIGHT:
            case PLA_RIGHT_REPEAT:
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                if (st_count > 0)
                    st_selected = (st_selected + 1) % st_count;
                redraw = true;
                break;
            case PLA_SELECT:
                result = st_launch();
                redraw = true;
                break;
            case PLA_CANCEL:
                result = PLUGIN_GOTO_ROOT;
                break;
            default:
                if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                    result = PLUGIN_USB_CONNECTED;
                break;
        }
    }
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    return result;
}
