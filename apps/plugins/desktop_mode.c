/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> ) \___\|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/             \/
 *
 * Copyright (C) 2026 The Rockbox Team
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

#define DM_MAX_FILES 96
#define DM_FILE_ROWS 7
#define DM_CONFIG_FILE PLUGIN_APPS_DATA_DIR "/desktop_mode.cfg"
#define DM_XP_ASSET_DIR PLUGIN_APPS_DATA_DIR "/desktop_mode_xp"
#define DM_XP_WALLPAPER_BMP DM_XP_ASSET_DIR "/bliss.320x212.bmp"
#define DM_SCALE_SCRATCH_EXTRA(width) ((width) * (int)sizeof(uint32_t) * 4)
#define DM_SCALED_BYTES(width, height) \
    (BM_SCALED_SIZE(width, height, FORMAT_NATIVE, false) + \
     DM_SCALE_SCRATCH_EXTRA(width))

#define DM_TASKBAR_H 28
#define DM_WALLPAPER_H (LCD_HEIGHT - DM_TASKBAR_H)
#define DM_START_W 66
#define DM_ICON_W 56
#define DM_ICON_H 50
#define DM_START_MENU_W 224
#define DM_START_MENU_H 178
#define DM_CURSOR_STEP 8
#define DM_CURSOR_FAST_STEP 15

#ifdef HAVE_LCD_COLOR
#define DM_RGB(r, g, b) LCD_RGBPACK(r, g, b)
#define DM_SKY_TOP DM_RGB(23, 116, 209)
#define DM_SKY_BOTTOM DM_RGB(137, 199, 255)
#define DM_HILL_A DM_RGB(65, 156, 45)
#define DM_HILL_B DM_RGB(31, 113, 31)
#define DM_HILL_C DM_RGB(134, 194, 61)
#define DM_TASKBAR_TOP DM_RGB(47, 123, 237)
#define DM_TASKBAR_BOTTOM DM_RGB(21, 68, 174)
#define DM_TASKBAR_EDGE DM_RGB(15, 48, 142)
#define DM_START_TOP DM_RGB(124, 212, 86)
#define DM_START_BOTTOM DM_RGB(26, 135, 29)
#define DM_START_EDGE DM_RGB(11, 89, 21)
#define DM_PANEL DM_RGB(236, 233, 216)
#define DM_PANEL_LIGHT DM_RGB(255, 255, 255)
#define DM_PANEL_DARK DM_RGB(128, 128, 128)
#define DM_TITLE_TOP DM_RGB(12, 89, 214)
#define DM_TITLE_BOTTOM DM_RGB(3, 56, 180)
#define DM_TITLE_INACTIVE DM_RGB(122, 150, 210)
#define DM_MENU_LEFT DM_RGB(255, 255, 255)
#define DM_MENU_RIGHT DM_RGB(211, 229, 250)
#define DM_SELECT_TOP DM_RGB(49, 106, 197)
#define DM_SELECT_BOTTOM DM_RGB(25, 74, 175)
#define DM_SHADOW DM_RGB(56, 74, 108)
#define DM_TEXT DM_RGB(0, 0, 0)
#define DM_MUTED DM_RGB(78, 78, 78)
#define DM_WHITE DM_RGB(255, 255, 255)
#define DM_RED DM_RGB(218, 42, 34)
#define DM_YELLOW DM_RGB(255, 215, 64)
#define DM_FOLDER DM_RGB(248, 209, 69)
#define DM_CURSOR_EDGE DM_RGB(0, 0, 0)
#else
#define DM_SKY_TOP LCD_DEFAULT_BG
#define DM_SKY_BOTTOM LCD_DEFAULT_BG
#define DM_HILL_A LCD_DEFAULT_BG
#define DM_HILL_B LCD_DEFAULT_BG
#define DM_HILL_C LCD_DEFAULT_BG
#define DM_TASKBAR_TOP LCD_DEFAULT_FG
#define DM_TASKBAR_BOTTOM LCD_DEFAULT_FG
#define DM_TASKBAR_EDGE LCD_DEFAULT_FG
#define DM_START_TOP LCD_DEFAULT_BG
#define DM_START_BOTTOM LCD_DEFAULT_BG
#define DM_START_EDGE LCD_DEFAULT_FG
#define DM_PANEL LCD_DEFAULT_BG
#define DM_PANEL_LIGHT LCD_DEFAULT_BG
#define DM_PANEL_DARK LCD_DEFAULT_FG
#define DM_TITLE_TOP LCD_DEFAULT_FG
#define DM_TITLE_BOTTOM LCD_DEFAULT_FG
#define DM_TITLE_INACTIVE LCD_DEFAULT_FG
#define DM_MENU_LEFT LCD_DEFAULT_BG
#define DM_MENU_RIGHT LCD_DEFAULT_BG
#define DM_SELECT_TOP LCD_DEFAULT_FG
#define DM_SELECT_BOTTOM LCD_DEFAULT_FG
#define DM_SHADOW LCD_DEFAULT_FG
#define DM_TEXT LCD_DEFAULT_FG
#define DM_MUTED LCD_DEFAULT_FG
#define DM_WHITE LCD_DEFAULT_BG
#define DM_RED LCD_DEFAULT_FG
#define DM_YELLOW LCD_DEFAULT_FG
#define DM_FOLDER LCD_DEFAULT_FG
#define DM_CURSOR_EDGE LCD_DEFAULT_FG
#endif

enum dm_mode
{
    DM_MODE_DESKTOP = 0,
    DM_MODE_EXPLORER,
    DM_MODE_SETTINGS,
};

enum dm_start_item
{
    DM_START_APP_BASE = 0,
    DM_START_FINDER = 100,
    DM_START_SETTINGS,
    DM_START_REFRESH,
    DM_START_EXIT,
};

enum dm_setting_item
{
    DM_SETTING_NOW_PLAYING = 0,
    DM_SETTING_START_VIEW,
    DM_SETTING_RETURN,
    DM_SETTING_COUNT,
};

struct dm_app
{
    const char *name;
    const char *subtitle;
    const char *path;
    const char *param;
    int glyph;
};

struct dm_file
{
    char name[MAX_PATH];
    bool is_dir;
};

struct dm_settings
{
    bool show_now_playing;
    bool start_in_finder;
};

struct dm_rect
{
    int x;
    int y;
    int w;
    int h;
};

static struct dm_settings dm_settings =
{
    true,
    false,
};

static const struct dm_app dm_apps[] =
{
    { "My Files", "Browse the disk", NULL, NULL, 0 },
    { "Notepad", "Text editor", PLUGIN_APPS_DIR "/text_editor.rock", NULL, 1 },
    { "Calendar", "Month view", PLUGIN_APPS_DIR "/calendar.rock", NULL, 2 },
    { "Calculator", "Desk accessory", PLUGIN_APPS_DIR "/calculator.rock",
      NULL, 3 },
    { "My Pictures", "Photo library", PLUGIN_APPS_DIR "/photos.rock",
      NULL, 4 },
    { "Game Boy", "Rockboy launcher", PLUGIN_GAMES_DIR "/rockboy_launcher.rock",
      NULL, 5 },
    { "PokeMini", "Tiny console", PLUGIN_GAMES_DIR "/pokemini_launcher.rock",
      NULL, 6 },
    { "Plugins", "All programs", VIEWERS_DIR "/open_plugins.rock", NULL, 7 },
    { "System Info", "About this iPod", PLUGIN_DEMOS_DIR "/rb_info.rock",
      NULL, 8 },
};

static struct dm_file dm_files[DM_MAX_FILES];
static int dm_file_count;
static int dm_file_sel;
static int dm_file_top;
static char dm_cwd[MAX_PATH] = "/";
static enum dm_mode dm_settings_return_mode = DM_MODE_DESKTOP;
static int dm_cursor_x = LCD_WIDTH / 2;
static int dm_cursor_y = LCD_HEIGHT / 2;
static int dm_selected_app;
static int dm_setting_sel;
static int dm_start_sel = DM_START_FINDER;
static struct bitmap dm_wallpaper_bmp;
static unsigned char *dm_wallpaper_data;
static size_t dm_wallpaper_data_size;
static bool dm_wallpaper_loaded;

static bool dm_config_bool(const char *value)
{
    return !rb->strcasecmp(value, "1") ||
           !rb->strcasecmp(value, "on") ||
           !rb->strcasecmp(value, "yes") ||
           !rb->strcasecmp(value, "true");
}

static void dm_load_settings(void)
{
    char line[64];
    int fd = rb->open(DM_CONFIG_FILE, O_RDONLY);

    if (fd < 0)
        return;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *name = NULL;
        char *value = NULL;

        if (!rb->settings_parseline(line, &name, &value))
            continue;
        if (!name || !value)
            continue;

        if (!rb->strcmp(name, "show now playing"))
            dm_settings.show_now_playing = dm_config_bool(value);
        else if (!rb->strcmp(name, "start in finder"))
            dm_settings.start_in_finder = dm_config_bool(value);
    }

    rb->close(fd);
}

static void dm_save_settings(void)
{
    char buf[96];
    int len;
    int fd;

    rb->mkdir(PLUGIN_APPS_DATA_DIR);
    fd = rb->open(DM_CONFIG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;

    len = rb->snprintf(buf, sizeof(buf),
                       "show now playing: %s\nstart in finder: %s\n",
                       dm_settings.show_now_playing ? "on" : "off",
                       dm_settings.start_in_finder ? "on" : "off");
    rb->write(fd, buf, len);
    rb->close(fd);
}

static void dm_set_colors(unsigned fg, unsigned bg)
{
#ifdef HAVE_LCD_COLOR
    rb->lcd_set_foreground(fg);
    rb->lcd_set_background(bg);
#else
    (void)fg;
    (void)bg;
#endif
}

static void dm_fillrect_c(int x, int y, int w, int h, unsigned color)
{
    if (w <= 0 || h <= 0)
        return;

    dm_set_colors(color, DM_WHITE);
    rb->lcd_fillrect(x, y, w, h);
}

static void dm_drawrect_c(int x, int y, int w, int h, unsigned color)
{
    if (w <= 0 || h <= 0)
        return;

    dm_set_colors(color, DM_WHITE);
    rb->lcd_drawrect(x, y, w, h);
}

static void dm_text_c(int x, int y, int width, const char *text,
                      unsigned color, bool center)
{
    int tw;
    int th;
    int tx = x;

    dm_set_colors(color, DM_WHITE);
    rb->lcd_getstringsize((const unsigned char *)text, &tw, &th);
    if (center && tw < width)
        tx = x + (width - tw) / 2;
    rb->lcd_putsxy(tx, y, (const unsigned char *)text);
}

static void dm_text_fit(int x, int y, int width, const char *text,
                        unsigned color, bool center)
{
    char buf[64];
    int tw;
    int th;
    int len;

    if (width <= 0)
        return;

    rb->strlcpy(buf, text, sizeof(buf));
    rb->lcd_getstringsize((const unsigned char *)buf, &tw, &th);
    len = rb->strlen(buf);
    while (len > 3 && tw > width)
    {
        buf[--len] = '\0';
        buf[len - 1] = '.';
        buf[len - 2] = '.';
        rb->lcd_getstringsize((const unsigned char *)buf, &tw, &th);
    }
    dm_text_c(x, y, width, buf, color, center);
}

static void dm_gradient_rect(int x, int y, int w, int h,
                             unsigned top, unsigned bottom)
{
#ifdef HAVE_LCD_COLOR
    int i;
    int tr = RGB_UNPACK_RED(top);
    int tg = RGB_UNPACK_GREEN(top);
    int tb = RGB_UNPACK_BLUE(top);
    int br = RGB_UNPACK_RED(bottom);
    int bg = RGB_UNPACK_GREEN(bottom);
    int bb = RGB_UNPACK_BLUE(bottom);

    for (i = 0; i < h; i++)
    {
        int r = tr + ((br - tr) * i) / MAX(1, h - 1);
        int g = tg + ((bg - tg) * i) / MAX(1, h - 1);
        int b = tb + ((bb - tb) * i) / MAX(1, h - 1);
        dm_fillrect_c(x, y + i, w, 1, DM_RGB(r, g, b));
    }
#else
    (void)top;
    (void)bottom;
    dm_fillrect_c(x, y, w, h, DM_WHITE);
#endif
}

static bool dm_pt_in_rect(int px, int py, struct dm_rect r)
{
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

static void dm_clock_text(char *buf, size_t size)
{
    struct tm *tm = rb->get_time();
    int hour = tm ? tm->tm_hour : 0;
    int min = tm ? tm->tm_min : 0;

    rb->snprintf(buf, size, "%d:%02d", hour, min);
}

static int dm_taskbar_y(void)
{
    return LCD_HEIGHT - DM_TASKBAR_H;
}

static void dm_load_wallpaper(void)
{
#ifdef HAVE_LCD_COLOR
    int needed = DM_SCALED_BYTES(LCD_WIDTH, DM_WALLPAPER_H);

    if (dm_wallpaper_loaded)
        return;
    if (!rb->file_exists(DM_XP_WALLPAPER_BMP))
        return;

    if (!dm_wallpaper_data)
        dm_wallpaper_data =
            (unsigned char *)rb->plugin_get_buffer(&dm_wallpaper_data_size);
    if (!dm_wallpaper_data || dm_wallpaper_data_size < (size_t)needed)
        return;

    rb->memset(&dm_wallpaper_bmp, 0, sizeof(dm_wallpaper_bmp));
    dm_wallpaper_bmp.width = LCD_WIDTH;
    dm_wallpaper_bmp.height = DM_WALLPAPER_H;
    dm_wallpaper_bmp.format = FORMAT_NATIVE;
    dm_wallpaper_bmp.data = dm_wallpaper_data;

    dm_wallpaper_loaded =
        rb->read_bmp_file(DM_XP_WALLPAPER_BMP, &dm_wallpaper_bmp,
                          needed, FORMAT_NATIVE | FORMAT_RESIZE |
                          FORMAT_DITHER, NULL) > 0;
#endif
}

static struct dm_rect dm_start_button_rect(void)
{
    struct dm_rect r = { 4, dm_taskbar_y() + 3, DM_START_W, 22 };

    return r;
}

static struct dm_rect dm_icon_rect(int app)
{
    int cols = LCD_WIDTH >= 300 ? 3 : 2;
    int col = app % cols;
    int row = app / cols;
    int left = 8 + col * ((LCD_WIDTH - 16) / cols);
    int top = 18 + row * 57;
    struct dm_rect r = { left, top, DM_ICON_W, DM_ICON_H };

    return r;
}

static struct dm_rect dm_start_menu_rect(void)
{
    struct dm_rect r =
    {
        4,
        dm_taskbar_y() - DM_START_MENU_H + 1,
        DM_START_MENU_W,
        DM_START_MENU_H
    };

    if (r.y < 0)
        r.y = 0;
    return r;
}

static void dm_draw_cloud(int x, int y, int w, int h)
{
    dm_fillrect_c(x + w / 5, y + h / 2, w * 3 / 5, h / 3, DM_WHITE);
    dm_fillrect_c(x + w / 3, y + h / 3, w / 3, h / 2, DM_WHITE);
    dm_fillrect_c(x, y + h * 2 / 3, w, h / 4, DM_WHITE);
}

static void dm_draw_wallpaper(void)
{
    int task_y = dm_taskbar_y();
    int hill_y = task_y - 56;
    int x;

    if (dm_wallpaper_loaded)
    {
        rb->lcd_bmp_part(&dm_wallpaper_bmp, 0, 0, 0, 0,
                         LCD_WIDTH, task_y);
        return;
    }

    dm_gradient_rect(0, 0, LCD_WIDTH, task_y, DM_SKY_TOP, DM_SKY_BOTTOM);
    dm_draw_cloud(LCD_WIDTH / 5, 25, 58, 22);
    dm_draw_cloud(LCD_WIDTH - 92, 38, 68, 24);
    dm_draw_cloud(LCD_WIDTH / 2 - 24, 73, 50, 18);

    for (x = 0; x < LCD_WIDTH; x++)
    {
        int wave = ((x * x) / MAX(1, LCD_WIDTH)) % 36;
        int y = hill_y + wave / 2 - x / 10;
        int y2 = hill_y + 24 + x / 14;

        if (y < 92)
            y = 92;
        dm_fillrect_c(x, y, 1, task_y - y, x & 1 ? DM_HILL_A : DM_HILL_C);
        if (y2 < task_y)
            dm_fillrect_c(x, y2, 1, task_y - y2, DM_HILL_B);
    }
}

static void dm_draw_xp_icon(int x, int y, int glyph, bool selected)
{
    unsigned frame = selected ? DM_SELECT_BOTTOM : DM_PANEL_DARK;

    if (selected)
    {
        dm_gradient_rect(x - 4, y - 3, 40, 34, DM_SELECT_TOP,
                         DM_SELECT_BOTTOM);
        dm_drawrect_c(x - 4, y - 3, 40, 34, DM_WHITE);
    }

    switch (glyph)
    {
        case 0:
            dm_fillrect_c(x + 2, y + 10, 29, 17, DM_FOLDER);
            dm_fillrect_c(x + 5, y + 6, 12, 6, DM_FOLDER);
            dm_drawrect_c(x + 2, y + 10, 29, 17, frame);
            dm_drawrect_c(x + 5, y + 6, 12, 7, frame);
            break;

        case 1:
            dm_fillrect_c(x + 6, y + 3, 21, 27, DM_WHITE);
            dm_drawrect_c(x + 6, y + 3, 21, 27, frame);
            dm_fillrect_c(x + 10, y + 10, 13, 2, DM_TITLE_TOP);
            dm_fillrect_c(x + 10, y + 16, 13, 2, DM_TITLE_TOP);
            dm_fillrect_c(x + 10, y + 22, 10, 2, DM_TITLE_TOP);
            break;

        case 2:
            dm_fillrect_c(x + 4, y + 6, 27, 23, DM_WHITE);
            dm_drawrect_c(x + 4, y + 6, 27, 23, frame);
            dm_fillrect_c(x + 4, y + 12, 27, 3, DM_TITLE_TOP);
            dm_fillrect_c(x + 10, y + 18, 7, 6, DM_RED);
            dm_fillrect_c(x + 21, y + 18, 5, 6, DM_YELLOW);
            break;

        case 3:
            dm_fillrect_c(x + 6, y + 5, 22, 26, DM_PANEL_LIGHT);
            dm_drawrect_c(x + 6, y + 5, 22, 26, frame);
            dm_fillrect_c(x + 10, y + 10, 14, 5, DM_MENU_RIGHT);
            dm_fillrect_c(x + 10, y + 20, 14, 2, DM_TITLE_TOP);
            dm_fillrect_c(x + 16, y + 14, 2, 14, DM_TITLE_TOP);
            break;

        case 4:
            dm_fillrect_c(x + 4, y + 9, 27, 19, DM_PANEL_LIGHT);
            dm_drawrect_c(x + 4, y + 9, 27, 19, frame);
            dm_fillrect_c(x + 8, y + 14, 9, 7, DM_HILL_A);
            dm_fillrect_c(x + 18, y + 14, 8, 7, DM_SKY_TOP);
            break;

        case 5:
        case 6:
            dm_fillrect_c(x + 7, y + 12, 20, 12, DM_PANEL_LIGHT);
            dm_drawrect_c(x + 7, y + 12, 20, 12, frame);
            dm_fillrect_c(x + 12, y + 17, 10, 2, DM_TITLE_TOP);
            dm_fillrect_c(x + 16, y + 13, 2, 10, DM_TITLE_TOP);
            dm_fillrect_c(x + 10, y + 8, 5, 5, DM_RED);
            dm_fillrect_c(x + 22, y + 8, 5, 5, DM_YELLOW);
            break;

        case 7:
            dm_fillrect_c(x + 5, y + 5, 24, 25, DM_MENU_RIGHT);
            dm_drawrect_c(x + 5, y + 5, 24, 25, frame);
            dm_fillrect_c(x + 9, y + 10, 16, 3, DM_TITLE_TOP);
            dm_fillrect_c(x + 9, y + 17, 16, 3, DM_HILL_A);
            dm_fillrect_c(x + 9, y + 24, 16, 3, DM_RED);
            break;

        default:
            dm_fillrect_c(x + 8, y + 5, 19, 25, DM_PANEL_LIGHT);
            dm_drawrect_c(x + 8, y + 5, 19, 25, frame);
            dm_fillrect_c(x + 14, y + 11, 7, 7, DM_TITLE_TOP);
            dm_fillrect_c(x + 16, y + 21, 3, 5, DM_TITLE_TOP);
            break;
    }
}

static void dm_draw_taskbar(bool start_open, enum dm_mode mode)
{
    struct dm_rect start = dm_start_button_rect();
    char clock[16];
    int x = start.x + start.w + 6;
    int task_y = dm_taskbar_y();
    int i;

    dm_gradient_rect(0, task_y, LCD_WIDTH, DM_TASKBAR_H, DM_TASKBAR_TOP,
                     DM_TASKBAR_BOTTOM);
    dm_fillrect_c(0, task_y, LCD_WIDTH, 1, DM_TASKBAR_EDGE);
    dm_gradient_rect(start.x, start.y, start.w, start.h,
                     start_open ? DM_START_BOTTOM : DM_START_TOP,
                     start_open ? DM_START_TOP : DM_START_BOTTOM);
    dm_drawrect_c(start.x, start.y, start.w, start.h, DM_START_EDGE);
    dm_text_c(start.x + 20, start.y + 5, start.w - 22, "start", DM_WHITE,
              false);
    dm_fillrect_c(start.x + 7, start.y + 6, 5, 5, DM_RED);
    dm_fillrect_c(start.x + 13, start.y + 6, 5, 5, DM_HILL_A);
    dm_fillrect_c(start.x + 7, start.y + 12, 5, 5, DM_SKY_BOTTOM);
    dm_fillrect_c(start.x + 13, start.y + 12, 5, 5, DM_YELLOW);

    for (i = 0; i < 3 && x + 58 < LCD_WIDTH - 52; i++)
    {
        int app = i;
        bool active = (mode == DM_MODE_EXPLORER && i == 0) ||
                      (mode == DM_MODE_SETTINGS && i == 1);

        dm_gradient_rect(x, task_y + 4, 58, 20,
                         active ? DM_TITLE_BOTTOM : DM_TASKBAR_TOP,
                         active ? DM_TITLE_TOP : DM_TASKBAR_BOTTOM);
        dm_drawrect_c(x, task_y + 4, 58, 20, DM_TASKBAR_EDGE);
        dm_text_fit(x + 5, task_y + 8, 48, i == 0 ? "Explorer" :
                    app == 1 ? "Settings" : "Desktop", DM_WHITE, false);
        x += 62;
    }

    dm_fillrect_c(LCD_WIDTH - 49, task_y + 4, 45, 20, DM_TITLE_TOP);
    dm_drawrect_c(LCD_WIDTH - 49, task_y + 4, 45, 20, DM_TASKBAR_EDGE);
    dm_clock_text(clock, sizeof(clock));
    dm_text_c(LCD_WIDTH - 45, task_y + 8, 37, clock, DM_WHITE, true);
}

static void dm_draw_desktop_icons(void)
{
    int i;

    for (i = 0; i < (int)ARRAYLEN(dm_apps); i++)
    {
        struct dm_rect r = dm_icon_rect(i);
        int icon_x = r.x + (r.w - 32) / 2;
        bool selected = i == dm_selected_app;

        if (r.y + r.h > dm_taskbar_y() - 2)
            break;

        dm_draw_xp_icon(icon_x, r.y, dm_apps[i].glyph, selected);
        if (selected)
            dm_gradient_rect(r.x, r.y + 34, r.w, 14, DM_SELECT_TOP,
                             DM_SELECT_BOTTOM);
        dm_text_fit(r.x + 1, r.y + 36, r.w - 2, dm_apps[i].name,
                    DM_WHITE, true);
    }
}

static void dm_draw_window_frame(int x, int y, int w, int h,
                                 const char *title, bool active)
{
    dm_fillrect_c(x + 4, y + 5, w, h, DM_SHADOW);
    dm_fillrect_c(x, y, w, h, DM_PANEL);
    dm_drawrect_c(x, y, w, h, DM_PANEL_DARK);
    dm_gradient_rect(x + 2, y + 2, w - 4, 20,
                     active ? DM_TITLE_TOP : DM_TITLE_INACTIVE,
                     active ? DM_TITLE_BOTTOM : DM_TITLE_INACTIVE);
    dm_text_fit(x + 8, y + 6, w - 58, title, DM_WHITE, false);

    dm_fillrect_c(x + w - 48, y + 5, 12, 12, DM_TITLE_TOP);
    dm_drawrect_c(x + w - 48, y + 5, 12, 12, DM_WHITE);
    dm_text_c(x + w - 45, y + 7, 6, "_", DM_WHITE, true);

    dm_fillrect_c(x + w - 32, y + 5, 12, 12, DM_TITLE_TOP);
    dm_drawrect_c(x + w - 32, y + 5, 12, 12, DM_WHITE);
    dm_drawrect_c(x + w - 29, y + 8, 6, 5, DM_WHITE);

    dm_fillrect_c(x + w - 16, y + 5, 12, 12, DM_RED);
    dm_drawrect_c(x + w - 16, y + 5, 12, 12, DM_WHITE);
    dm_text_c(x + w - 14, y + 6, 8, "x", DM_WHITE, true);
}

static void dm_draw_desktop(bool start_open)
{
    dm_draw_wallpaper();
    dm_draw_desktop_icons();
    dm_draw_taskbar(start_open, DM_MODE_DESKTOP);
}

static void dm_draw_file_badge(int x, int y, bool is_dir, bool selected)
{
    unsigned fg = selected ? DM_WHITE : is_dir ? DM_FOLDER : DM_TITLE_TOP;
    unsigned bg = selected ? DM_SELECT_TOP : DM_PANEL_LIGHT;

    if (is_dir)
    {
        dm_fillrect_c(x + 1, y + 6, 17, 12, fg);
        dm_fillrect_c(x + 4, y + 3, 8, 5, fg);
        dm_drawrect_c(x + 1, y + 6, 17, 12, DM_PANEL_DARK);
    }
    else
    {
        dm_fillrect_c(x + 4, y + 2, 13, 17, bg);
        dm_drawrect_c(x + 4, y + 2, 13, 17, fg);
        dm_fillrect_c(x + 7, y + 7, 7, 2, fg);
        dm_fillrect_c(x + 7, y + 12, 7, 2, fg);
    }
}

static void dm_draw_scrollbar(int x, int y, int h, int count, int top, int rows)
{
    int thumb_h;
    int thumb_y;

    if (count <= rows || h <= 8)
        return;

    thumb_h = MAX(9, (h * rows) / count);
    thumb_y = y + ((h - thumb_h) * top) / MAX(1, count - rows);

    dm_fillrect_c(x, y, 10, h, DM_PANEL);
    dm_drawrect_c(x, y, 10, h, DM_PANEL_DARK);
    dm_gradient_rect(x + 2, thumb_y, 6, thumb_h, DM_TITLE_TOP,
                     DM_TITLE_BOTTOM);
}

static void dm_draw_explorer(void)
{
    int x = 7;
    int y = 13;
    int w = LCD_WIDTH - 14;
    int h = dm_taskbar_y() - 20;
    int list_x = x + 8;
    int list_y = y + 49;
    int list_w = w - 16;
    int row_h = MAX(18, (h - 63) / DM_FILE_ROWS);
    int i;
    char status[48];
    struct mp3entry *id3 = dm_settings.show_now_playing ?
                            rb->audio_current_track() : NULL;

    dm_draw_wallpaper();
    dm_draw_window_frame(x, y, w, h, dm_cwd, true);

    dm_fillrect_c(x + 4, y + 24, w - 8, 20, DM_PANEL);
    dm_drawrect_c(x + 4, y + 24, w - 8, 20, DM_PANEL_DARK);
    dm_text_c(x + 11, y + 29, 36, "Back", DM_MUTED, false);
    dm_text_c(x + 53, y + 29, 54, "Folders", DM_MUTED, false);
    dm_text_fit(x + 116, y + 29, w - 130, dm_cwd, DM_TEXT, false);

    dm_fillrect_c(list_x, list_y, list_w, row_h * DM_FILE_ROWS + 1,
                  DM_PANEL_LIGHT);
    dm_drawrect_c(list_x, list_y, list_w, row_h * DM_FILE_ROWS + 1,
                  DM_PANEL_DARK);

    if (dm_file_count == 0)
        dm_text_fit(list_x + 8, list_y + 50, list_w - 16, "Folder is empty",
                    DM_MUTED, true);

    for (i = 0; i < DM_FILE_ROWS; i++)
    {
        int idx = dm_file_top + i;
        int row_y = list_y + 1 + i * row_h;
        bool selected = idx == dm_file_sel;

        if (idx >= dm_file_count)
            break;

        if (selected)
            dm_gradient_rect(list_x + 1, row_y, list_w - 12, row_h - 1,
                             DM_SELECT_TOP, DM_SELECT_BOTTOM);
        else if (i & 1)
            dm_fillrect_c(list_x + 1, row_y, list_w - 12, row_h - 1,
                          DM_PANEL);

        dm_draw_file_badge(list_x + 6, row_y + 1, dm_files[idx].is_dir,
                           selected);
        dm_text_fit(list_x + 29, row_y + 4, list_w - 49,
                    dm_files[idx].name,
                    selected ? DM_WHITE : DM_TEXT, false);
    }

    dm_draw_scrollbar(list_x + list_w - 10, list_y + 1,
                      row_h * DM_FILE_ROWS - 1, dm_file_count, dm_file_top,
                      DM_FILE_ROWS);
    rb->snprintf(status, sizeof(status), "%d item%s", dm_file_count,
                 dm_file_count == 1 ? "" : "s");
    dm_text_fit(x + 8, y + h - 15, 70, status, DM_MUTED, false);
    if (id3 && id3->title)
        dm_text_fit(x + 84, y + h - 15, w - 96, id3->title, DM_MUTED, false);

    dm_draw_taskbar(false, DM_MODE_EXPLORER);
}

static void dm_draw_settings(void)
{
    int w = MIN(LCD_WIDTH - 28, 252);
    int h = 116;
    int x = (LCD_WIDTH - w) / 2;
    int y = 52;
    int i;

    dm_draw_wallpaper();
    dm_draw_window_frame(x, y, w, h, "Desktop Mode Settings", true);

    for (i = 0; i < DM_SETTING_COUNT; i++)
    {
        int row_y = y + 31 + i * 24;
        const char *name;
        const char *value;

        if (i == DM_SETTING_NOW_PLAYING)
        {
            name = "Show now playing";
            value = dm_settings.show_now_playing ? "On" : "Off";
        }
        else if (i == DM_SETTING_START_VIEW)
        {
            name = "Start view";
            value = dm_settings.start_in_finder ? "Explorer" : "Desktop";
        }
        else
        {
            name = "OK";
            value = "Close";
        }

        if (i == dm_setting_sel)
            dm_gradient_rect(x + 8, row_y, w - 16, 20, DM_SELECT_TOP,
                             DM_SELECT_BOTTOM);
        dm_text_fit(x + 15, row_y + 4, w - 92, name,
                    i == dm_setting_sel ? DM_WHITE : DM_TEXT, false);
        dm_text_fit(x + w - 76, row_y + 4, 54, value,
                    i == dm_setting_sel ? DM_WHITE : DM_MUTED, true);
    }

    dm_draw_taskbar(false, DM_MODE_SETTINGS);
}

static int dm_start_item_at(int px, int py)
{
    struct dm_rect m = dm_start_menu_rect();
    int row_h = 20;
    int app_rows = MIN(7, (int)ARRAYLEN(dm_apps));
    int i;

    if (!dm_pt_in_rect(px, py, m))
        return -1;
    if (py < m.y + 30)
        return -1;

    for (i = 0; i < app_rows; i++)
    {
        struct dm_rect r = { m.x + 6, m.y + 34 + i * row_h,
                             116, row_h };
        if (dm_pt_in_rect(px, py, r))
            return DM_START_APP_BASE + i;
    }

    for (i = 0; i < 4; i++)
    {
        struct dm_rect r = { m.x + 130, m.y + 40 + i * 25,
                             m.w - 138, 22 };
        if (dm_pt_in_rect(px, py, r))
            return DM_START_FINDER + i;
    }

    return -1;
}

static void dm_draw_start_row(struct dm_rect r, int id, const char *label,
                              const char *sub)
{
    bool selected = id == dm_start_sel;

    if (selected)
        dm_gradient_rect(r.x, r.y, r.w, r.h, DM_SELECT_TOP,
                         DM_SELECT_BOTTOM);
    dm_text_fit(r.x + 24, r.y + 4, r.w - 28, label,
                selected ? DM_WHITE : DM_TEXT, false);
    if (sub)
        dm_text_fit(r.x + 24, r.y + 13, r.w - 28, sub,
                    selected ? DM_WHITE : DM_MUTED, false);
}

static void dm_draw_start_menu(void)
{
    struct dm_rect m = dm_start_menu_rect();
    int row_h = 20;
    int app_rows = MIN(7, (int)ARRAYLEN(dm_apps));
    int i;

    dm_fillrect_c(m.x + 4, m.y + 5, m.w, m.h, DM_SHADOW);
    dm_fillrect_c(m.x, m.y, m.w, m.h, DM_PANEL_LIGHT);
    dm_drawrect_c(m.x, m.y, m.w, m.h, DM_TASKBAR_EDGE);
    dm_gradient_rect(m.x + 1, m.y + 1, m.w - 2, 28, DM_TITLE_TOP,
                     DM_TITLE_BOTTOM);
    dm_text_c(m.x + 34, m.y + 8, 136, "Rockbox XP", DM_WHITE, false);
    dm_fillrect_c(m.x + 9, m.y + 6, 18, 18, DM_PANEL_LIGHT);
    dm_drawrect_c(m.x + 9, m.y + 6, 18, 18, DM_WHITE);
    dm_fillrect_c(m.x + 13, m.y + 10, 5, 5, DM_RED);
    dm_fillrect_c(m.x + 19, m.y + 10, 5, 5, DM_HILL_A);
    dm_fillrect_c(m.x + 13, m.y + 16, 5, 5, DM_SKY_BOTTOM);
    dm_fillrect_c(m.x + 19, m.y + 16, 5, 5, DM_YELLOW);

    dm_fillrect_c(m.x + 1, m.y + 30, 126, m.h - 58, DM_MENU_LEFT);
    dm_fillrect_c(m.x + 128, m.y + 30, m.w - 129, m.h - 58,
                  DM_MENU_RIGHT);

    for (i = 0; i < app_rows; i++)
    {
        struct dm_rect r = { m.x + 6, m.y + 34 + i * row_h, 116, row_h };

        dm_draw_xp_icon(r.x + 2, r.y + 1, dm_apps[i].glyph, false);
        dm_draw_start_row(r, DM_START_APP_BASE + i, dm_apps[i].name,
                          NULL);
    }

    for (i = 0; i < 4; i++)
    {
        static const char *labels[] =
        {
            "My Files", "Control Panel", "Refresh", "Shut Down"
        };
        static const char *subs[] =
        {
            "Explorer", "Settings", "Redraw", "Exit"
        };
        struct dm_rect r = { m.x + 130, m.y + 40 + i * 25,
                             m.w - 138, 22 };

        dm_draw_start_row(r, DM_START_FINDER + i, labels[i], subs[i]);
    }

    dm_gradient_rect(m.x + 1, m.y + m.h - 26, m.w - 2, 25,
                     DM_TITLE_BOTTOM, DM_TITLE_TOP);
    dm_text_fit(m.x + 10, m.y + m.h - 18, m.w - 20,
                "Personal asset pack: " DM_XP_ASSET_DIR,
                DM_WHITE, false);
}

static void dm_draw_cursor(void)
{
    int x = dm_cursor_x;
    int y = dm_cursor_y;
    int i;

    for (i = 0; i < 14; i++)
        dm_fillrect_c(x + i / 2, y + i, MAX(1, 8 - i / 2), 1, DM_WHITE);
    for (i = 0; i < 15; i++)
        dm_fillrect_c(x, y + i, 1, 1, DM_CURSOR_EDGE);
    for (i = 0; i < 8; i++)
        dm_fillrect_c(x + i, y + i * 2, 1, 1, DM_CURSOR_EDGE);
    dm_fillrect_c(x + 5, y + 12, 6, 2, DM_CURSOR_EDGE);
    dm_fillrect_c(x + 7, y + 14, 2, 5, DM_CURSOR_EDGE);
}

static void dm_parent_dir(char *path)
{
    char *p;
    int len = rb->strlen(path);

    if (len <= 1)
    {
        rb->strcpy(path, "/");
        return;
    }

    if (path[len - 1] == '/')
        path[len - 1] = '\0';
    p = rb->strrchr(path, '/');
    if (!p || p == path)
        rb->strcpy(path, "/");
    else
        *p = '\0';
}

static void dm_join_path(char *out, size_t out_size, const char *dir,
                         const char *name)
{
    if (!rb->strcmp(dir, "/"))
        rb->snprintf(out, out_size, "/%s", name);
    else
        rb->snprintf(out, out_size, "%s/%s", dir, name);
}

static void dm_file_swap(int a, int b)
{
    struct dm_file tmp = dm_files[a];

    dm_files[a] = dm_files[b];
    dm_files[b] = tmp;
}

static int dm_file_compare(const struct dm_file *a, const struct dm_file *b)
{
    if (a->is_dir != b->is_dir)
        return a->is_dir ? -1 : 1;

    return rb->strcasecmp(a->name, b->name);
}

static void dm_sort_files(void)
{
    int i;

    for (i = 1; i < dm_file_count; i++)
    {
        int j = i;

        while (j > 0 && dm_file_compare(&dm_files[j], &dm_files[j - 1]) < 0)
        {
            dm_file_swap(j, j - 1);
            j--;
        }
    }
}

static void dm_scan_dir(void)
{
    DIR *dir;
    struct dirent *entry;

    dm_file_count = 0;
    dm_file_sel = 0;
    dm_file_top = 0;

    dir = rb->opendir(dm_cwd);
    if (!dir)
        return;

    while ((entry = rb->readdir(dir)) && dm_file_count < DM_MAX_FILES)
    {
        struct dirinfo info;

        if (!rb->strcmp(entry->d_name, ".") || !rb->strcmp(entry->d_name, ".."))
            continue;

        info = rb->dir_get_info(dir, entry);
        rb->strlcpy(dm_files[dm_file_count].name, entry->d_name,
                    sizeof(dm_files[dm_file_count].name));
        dm_files[dm_file_count].is_dir = (info.attribute & ATTR_DIRECTORY) != 0;
        dm_file_count++;
    }
    rb->closedir(dir);
    dm_sort_files();
}

static int dm_open_selected_file(void)
{
    char path[MAX_PATH];
    char plugin[MAX_PATH];
    int attr;
    int len;

    if (dm_file_sel < 0 || dm_file_sel >= dm_file_count)
        return PLUGIN_OK;

    dm_join_path(path, sizeof(path), dm_cwd, dm_files[dm_file_sel].name);
    if (dm_files[dm_file_sel].is_dir)
    {
        rb->strlcpy(dm_cwd, path, sizeof(dm_cwd));
        dm_scan_dir();
        return PLUGIN_OK;
    }

    len = rb->strlen(path);
    if (len > 5 && !rb->strcasecmp(path + len - 5, ".rock"))
        return rb->plugin_open(path, NULL);

    attr = rb->filetype_get_attr(path);
    if (rb->filetype_get_plugin(attr, plugin, sizeof(plugin)))
        return rb->plugin_open(plugin, path);

    rb->splash(HZ, "No viewer");
    return PLUGIN_OK;
}

static int dm_launch_app(int selected, enum dm_mode *mode)
{
    const struct dm_app *app = &dm_apps[selected];

    if (selected == 0)
    {
        rb->strcpy(dm_cwd, "/");
        dm_scan_dir();
        *mode = DM_MODE_EXPLORER;
        return PLUGIN_OK;
    }

    if (!app->path || !rb->file_exists(app->path))
    {
        rb->splash(HZ, "Not installed");
        return PLUGIN_OK;
    }

    return rb->plugin_open(app->path, app->param);
}

static void dm_finder_parent(enum dm_mode *mode)
{
    if (!rb->strcmp(dm_cwd, "/"))
        *mode = DM_MODE_DESKTOP;
    else
    {
        dm_parent_dir(dm_cwd);
        dm_scan_dir();
    }
}

static int dm_handle_start_item(int item, enum dm_mode *mode)
{
    if (item >= 0 && item < (int)ARRAYLEN(dm_apps))
        return dm_launch_app(item, mode);

    switch (item)
    {
        case DM_START_FINDER:
            rb->strcpy(dm_cwd, "/");
            dm_scan_dir();
            *mode = DM_MODE_EXPLORER;
            break;

        case DM_START_SETTINGS:
            dm_settings_return_mode = *mode;
            *mode = DM_MODE_SETTINGS;
            break;

        case DM_START_REFRESH:
            if (*mode == DM_MODE_EXPLORER)
                dm_scan_dir();
            break;

        case DM_START_EXIT:
            return PLUGIN_OK + 1;
    }

    return PLUGIN_OK;
}

static int dm_file_hit_row(int px, int py)
{
    int x = 7;
    int y = 13;
    int w = LCD_WIDTH - 14;
    int h = dm_taskbar_y() - 20;
    int list_x = x + 8;
    int list_y = y + 49;
    int list_w = w - 16;
    int row_h = MAX(18, (h - 63) / DM_FILE_ROWS);
    int i;

    if (px < list_x || px >= list_x + list_w || py < list_y)
        return -1;

    for (i = 0; i < DM_FILE_ROWS; i++)
    {
        int idx = dm_file_top + i;
        struct dm_rect r = { list_x, list_y + 1 + i * row_h,
                             list_w - 12, row_h - 1 };

        if (idx < dm_file_count && dm_pt_in_rect(px, py, r))
            return idx;
    }

    return -1;
}

static int dm_settings_hit_row(int px, int py)
{
    int w = MIN(LCD_WIDTH - 28, 252);
    int x = (LCD_WIDTH - w) / 2;
    int y = 52;
    int i;

    for (i = 0; i < DM_SETTING_COUNT; i++)
    {
        struct dm_rect r = { x + 8, y + 31 + i * 24, w - 16, 20 };

        if (dm_pt_in_rect(px, py, r))
            return i;
    }

    return -1;
}

static void dm_toggle_setting(int setting)
{
    switch (setting)
    {
        case DM_SETTING_NOW_PLAYING:
            dm_settings.show_now_playing = !dm_settings.show_now_playing;
            dm_save_settings();
            break;

        case DM_SETTING_START_VIEW:
            dm_settings.start_in_finder = !dm_settings.start_in_finder;
            dm_save_settings();
            break;

        case DM_SETTING_RETURN:
            break;
    }
}

static int dm_click(enum dm_mode *mode, bool *start_open)
{
    struct dm_rect start = dm_start_button_rect();
    int i;

    if (*start_open)
    {
        int item = dm_start_item_at(dm_cursor_x, dm_cursor_y);

        if (item >= 0)
        {
            *start_open = false;
            return dm_handle_start_item(item, mode);
        }
        if (!dm_pt_in_rect(dm_cursor_x, dm_cursor_y, dm_start_menu_rect()))
            *start_open = false;
        return PLUGIN_OK;
    }

    if (dm_pt_in_rect(dm_cursor_x, dm_cursor_y, start))
    {
        *start_open = true;
        return PLUGIN_OK;
    }

    if (*mode == DM_MODE_DESKTOP)
    {
        for (i = 0; i < (int)ARRAYLEN(dm_apps); i++)
        {
            if (dm_pt_in_rect(dm_cursor_x, dm_cursor_y, dm_icon_rect(i)))
            {
                dm_selected_app = i;
                return dm_launch_app(i, mode);
            }
        }
    }
    else if (*mode == DM_MODE_EXPLORER)
    {
        int row = dm_file_hit_row(dm_cursor_x, dm_cursor_y);

        if (row >= 0)
        {
            dm_file_sel = row;
            return dm_open_selected_file();
        }
    }
    else if (*mode == DM_MODE_SETTINGS)
    {
        int row = dm_settings_hit_row(dm_cursor_x, dm_cursor_y);

        if (row >= 0)
        {
            dm_setting_sel = row;
            if (row == DM_SETTING_RETURN)
                *mode = dm_settings_return_mode;
            else
                dm_toggle_setting(row);
        }
    }

    return PLUGIN_OK;
}

static void dm_move_cursor(int dx, int dy)
{
    dm_cursor_x += dx;
    dm_cursor_y += dy;

    if (dm_cursor_x < 0)
        dm_cursor_x = 0;
    if (dm_cursor_y < 0)
        dm_cursor_y = 0;
    if (dm_cursor_x > LCD_WIDTH - 2)
        dm_cursor_x = LCD_WIDTH - 2;
    if (dm_cursor_y > LCD_HEIGHT - 2)
        dm_cursor_y = LCD_HEIGHT - 2;
}

static void dm_update_hover(enum dm_mode mode, bool start_open)
{
    int i;

    if (start_open)
    {
        int item = dm_start_item_at(dm_cursor_x, dm_cursor_y);

        if (item >= 0)
            dm_start_sel = item;
        return;
    }

    if (mode == DM_MODE_DESKTOP)
    {
        for (i = 0; i < (int)ARRAYLEN(dm_apps); i++)
        {
            if (dm_pt_in_rect(dm_cursor_x, dm_cursor_y, dm_icon_rect(i)))
            {
                dm_selected_app = i;
                return;
            }
        }
    }
    else if (mode == DM_MODE_EXPLORER)
    {
        int row = dm_file_hit_row(dm_cursor_x, dm_cursor_y);

        if (row >= 0)
            dm_file_sel = row;
    }
    else if (mode == DM_MODE_SETTINGS)
    {
        int row = dm_settings_hit_row(dm_cursor_x, dm_cursor_y);

        if (row >= 0)
            dm_setting_sel = row;
    }
}

static void dm_scroll_selection(enum dm_mode mode, bool start_open, int delta)
{
    if (start_open)
    {
        if (dm_start_sel >= 0 && dm_start_sel < (int)ARRAYLEN(dm_apps))
        {
            dm_start_sel += delta;
            if (dm_start_sel < 0)
                dm_start_sel = DM_START_EXIT;
            else if (dm_start_sel >= (int)MIN(7, (int)ARRAYLEN(dm_apps)))
                dm_start_sel = DM_START_FINDER;
        }
        else
        {
            dm_start_sel += delta;
            if (dm_start_sel < DM_START_FINDER)
                dm_start_sel = MIN(7, (int)ARRAYLEN(dm_apps)) - 1;
            else if (dm_start_sel > DM_START_EXIT)
                dm_start_sel = 0;
        }
    }
    else if (mode == DM_MODE_EXPLORER && dm_file_count > 0)
    {
        dm_file_sel += delta;
        if (dm_file_sel < 0)
            dm_file_sel = dm_file_count - 1;
        if (dm_file_sel >= dm_file_count)
            dm_file_sel = 0;
        if (dm_file_sel < dm_file_top)
            dm_file_top = dm_file_sel;
        if (dm_file_sel >= dm_file_top + DM_FILE_ROWS)
            dm_file_top = dm_file_sel - DM_FILE_ROWS + 1;
    }
    else if (mode == DM_MODE_SETTINGS)
    {
        dm_setting_sel += delta;
        if (dm_setting_sel < 0)
            dm_setting_sel = DM_SETTING_COUNT - 1;
        if (dm_setting_sel >= DM_SETTING_COUNT)
            dm_setting_sel = 0;
    }
    else if (mode == DM_MODE_DESKTOP)
    {
        dm_selected_app += delta;
        if (dm_selected_app < 0)
            dm_selected_app = ARRAYLEN(dm_apps) - 1;
        if (dm_selected_app >= (int)ARRAYLEN(dm_apps))
            dm_selected_app = 0;
    }
}

static bool dm_button_has(long button, long mask)
{
    return mask != 0 && (button & mask) == mask;
}

static bool dm_button_repeat(long button)
{
#ifdef BUTTON_REPEAT
    return (button & BUTTON_REPEAT) != 0;
#else
    (void)button;
    return false;
#endif
}

static bool dm_button_left(long button)
{
#ifdef BUTTON_LEFT
    if (dm_button_has(button, BUTTON_LEFT))
        return true;
#endif
    (void)button;
    return false;
}

static bool dm_button_right(long button)
{
#ifdef BUTTON_RIGHT
    if (dm_button_has(button, BUTTON_RIGHT))
        return true;
#endif
    (void)button;
    return false;
}

static bool dm_button_up(long button)
{
#ifdef BUTTON_UP
    if (dm_button_has(button, BUTTON_UP))
        return true;
#endif
#ifdef BUTTON_SCROLL_BACK
    if (dm_button_has(button, BUTTON_SCROLL_BACK))
        return true;
#endif
    (void)button;
    return false;
}

static bool dm_button_down(long button)
{
#ifdef BUTTON_DOWN
    if (dm_button_has(button, BUTTON_DOWN))
        return true;
#endif
#ifdef BUTTON_SCROLL_FWD
    if (dm_button_has(button, BUTTON_SCROLL_FWD))
        return true;
#endif
    (void)button;
    return false;
}

static bool dm_button_select_rel(long button)
{
#if defined(BUTTON_SELECT) && defined(BUTTON_REL)
    return dm_button_has(button, BUTTON_SELECT | BUTTON_REL);
#else
    (void)button;
    return false;
#endif
}

static bool dm_button_select_repeat(long button)
{
#if defined(BUTTON_SELECT) && defined(BUTTON_REPEAT)
    return dm_button_has(button, BUTTON_SELECT | BUTTON_REPEAT);
#else
    (void)button;
    return false;
#endif
}

static bool dm_button_menu(long button)
{
#ifdef BUTTON_MENU
    if (dm_button_has(button, BUTTON_MENU))
        return true;
#endif
    (void)button;
    return false;
}

static bool dm_button_play_exit(long button)
{
#if defined(BUTTON_PLAY) && defined(BUTTON_REPEAT)
    return dm_button_has(button, BUTTON_PLAY | BUTTON_REPEAT);
#else
    (void)button;
    return false;
#endif
}

enum plugin_status plugin_start(const void *parameter)
{
    enum dm_mode mode = DM_MODE_DESKTOP;
    int ret = PLUGIN_OK;
    bool redraw = true;
    bool start_open = false;

    (void)parameter;
    rb->lcd_setfont(FONT_UI);
    dm_load_wallpaper();
    dm_load_settings();
    if (dm_settings.start_in_finder)
    {
        rb->strcpy(dm_cwd, "/");
        dm_scan_dir();
        mode = DM_MODE_EXPLORER;
    }

    while (ret == PLUGIN_OK)
    {
        long button;

        if (redraw)
        {
            if (mode == DM_MODE_DESKTOP)
                dm_draw_desktop(start_open);
            else if (mode == DM_MODE_EXPLORER)
                dm_draw_explorer();
            else
                dm_draw_settings();
            if (start_open)
                dm_draw_start_menu();
            dm_draw_cursor();
            rb->lcd_update();
            redraw = false;
        }

        button = rb->button_get_w_tmo(HZ / 20);
        if (button == BUTTON_NONE)
            continue;

        if (rb->default_event_handler(button) == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;

        if (dm_button_play_exit(button))
            ret = PLUGIN_OK + 1;
        else if (dm_button_select_repeat(button))
        {
            start_open = true;
            redraw = true;
        }
        else if (dm_button_select_rel(button))
        {
            ret = dm_click(&mode, &start_open);
            redraw = true;
        }
        else if (dm_button_menu(button))
        {
            if (start_open)
                start_open = false;
            else if (mode == DM_MODE_SETTINGS)
            {
                dm_save_settings();
                mode = dm_settings_return_mode;
            }
            else if (mode == DM_MODE_EXPLORER)
                dm_finder_parent(&mode);
            else
                ret = PLUGIN_OK + 1;
            redraw = true;
        }
        else if (dm_button_left(button))
        {
            dm_move_cursor(dm_button_repeat(button) ?
                           -DM_CURSOR_FAST_STEP : -DM_CURSOR_STEP, 0);
            dm_update_hover(mode, start_open);
            redraw = true;
        }
        else if (dm_button_right(button))
        {
            dm_move_cursor(dm_button_repeat(button) ?
                           DM_CURSOR_FAST_STEP : DM_CURSOR_STEP, 0);
            dm_update_hover(mode, start_open);
            redraw = true;
        }
        else if (dm_button_up(button))
        {
            dm_move_cursor(0, dm_button_repeat(button) ?
                           -DM_CURSOR_FAST_STEP : -DM_CURSOR_STEP);
            dm_scroll_selection(mode, start_open, -1);
            dm_update_hover(mode, start_open);
            redraw = true;
        }
        else if (dm_button_down(button))
        {
            dm_move_cursor(0, dm_button_repeat(button) ?
                           DM_CURSOR_FAST_STEP : DM_CURSOR_STEP);
            dm_scroll_selection(mode, start_open, 1);
            dm_update_hover(mode, start_open);
            redraw = true;
        }
    }

    rb->lcd_setfont(FONT_UI);
    return ret == PLUGIN_OK + 1 ? PLUGIN_OK : ret;
}
