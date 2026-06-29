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

#if LCD_WIDTH >= 320
#define DM_DOCK_ITEMS 5
#else
#define DM_DOCK_ITEMS 4
#endif

#ifdef HAVE_LCD_COLOR
#define DM_RGB(r, g, b) LCD_RGBPACK(r, g, b)
#define DM_DESKTOP_TOP DM_RGB(54, 103, 164)
#define DM_DESKTOP_BOTTOM DM_RGB(117, 159, 211)
#define DM_MENUBAR_TOP DM_RGB(250, 250, 252)
#define DM_MENUBAR_BOTTOM DM_RGB(181, 186, 194)
#define DM_PANEL DM_RGB(235, 238, 243)
#define DM_PANEL_DARK DM_RGB(166, 176, 190)
#define DM_BORDER DM_RGB(102, 111, 126)
#define DM_TEXT DM_RGB(18, 24, 31)
#define DM_MUTED DM_RGB(81, 91, 107)
#define DM_WHITE DM_RGB(255, 255, 255)
#define DM_BLUE DM_RGB(42, 128, 224)
#define DM_BLUE_DARK DM_RGB(18, 78, 160)
#define DM_DOCK_TOP DM_RGB(222, 228, 237)
#define DM_DOCK_BOTTOM DM_RGB(129, 143, 163)
#define DM_SHADOW DM_RGB(45, 57, 75)
#else
#define DM_DESKTOP_TOP LCD_DEFAULT_BG
#define DM_DESKTOP_BOTTOM LCD_DEFAULT_BG
#define DM_MENUBAR_TOP LCD_DEFAULT_BG
#define DM_MENUBAR_BOTTOM LCD_DEFAULT_BG
#define DM_PANEL LCD_DEFAULT_BG
#define DM_PANEL_DARK LCD_DEFAULT_FG
#define DM_BORDER LCD_DEFAULT_FG
#define DM_TEXT LCD_DEFAULT_FG
#define DM_MUTED LCD_DEFAULT_FG
#define DM_WHITE LCD_DEFAULT_BG
#define DM_BLUE LCD_DEFAULT_FG
#define DM_BLUE_DARK LCD_DEFAULT_FG
#define DM_DOCK_TOP LCD_DEFAULT_BG
#define DM_DOCK_BOTTOM LCD_DEFAULT_BG
#define DM_SHADOW LCD_DEFAULT_FG
#endif

enum dm_mode
{
    DM_MODE_DESKTOP = 0,
    DM_MODE_FINDER,
    DM_MODE_SETTINGS,
};

enum dm_command
{
    DM_COMMAND_OPEN = 0,
    DM_COMMAND_FINDER,
    DM_COMMAND_SETTINGS,
    DM_COMMAND_REFRESH,
    DM_COMMAND_PARENT,
    DM_COMMAND_DESKTOP,
    DM_COMMAND_EXIT,
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

static struct dm_settings dm_settings =
{
    true,
    false,
};

static const struct dm_app dm_apps[] =
{
    { "Finder", "Files", NULL, NULL, 0 },
    { "Notes", "Text editor", PLUGIN_APPS_DIR "/text_editor.rock", NULL, 1 },
    { "Calendar", "Month view", PLUGIN_APPS_DIR "/calendar.rock", NULL, 2 },
    { "Calc", "Calculator", PLUGIN_APPS_DIR "/calculator.rock", NULL, 3 },
    { "Photos", "Albums", PLUGIN_APPS_DIR "/photos.rock", NULL, 4 },
    { "Games", "Game Boy", PLUGIN_GAMES_DIR "/rockboy_launcher.rock", NULL, 5 },
    { "PokeMini", "Tiny console", PLUGIN_GAMES_DIR "/pokemini_launcher.rock", NULL, 6 },
    { "Plugins", "Launcher", VIEWERS_DIR "/open_plugins.rock", NULL, 7 },
    { "Info", "System", PLUGIN_DEMOS_DIR "/rb_info.rock", NULL, 8 },
};

static struct dm_file dm_files[DM_MAX_FILES];
static int dm_file_count;
static int dm_file_sel;
static int dm_file_top;
static char dm_cwd[MAX_PATH] = "/";
static enum dm_mode dm_settings_return_mode = DM_MODE_DESKTOP;

static const enum dm_command dm_desktop_commands[] =
{
    DM_COMMAND_OPEN,
    DM_COMMAND_FINDER,
    DM_COMMAND_SETTINGS,
    DM_COMMAND_REFRESH,
    DM_COMMAND_EXIT,
};

static const enum dm_command dm_finder_commands[] =
{
    DM_COMMAND_OPEN,
    DM_COMMAND_PARENT,
    DM_COMMAND_SETTINGS,
    DM_COMMAND_REFRESH,
    DM_COMMAND_DESKTOP,
};

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
    dm_set_colors(color, DM_WHITE);
    rb->lcd_fillrect(x, y, w, h);
}

static void dm_drawrect_c(int x, int y, int w, int h, unsigned color)
{
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

static void dm_status_text(char *buf, size_t size, const char *left,
                           const char *right)
{
    if (right && right[0])
        rb->snprintf(buf, size, "%s  |  %s", left, right);
    else
        rb->strlcpy(buf, left, size);
}

static void dm_gradient(int y, int h, unsigned top, unsigned bottom)
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
        dm_fillrect_c(0, y + i, LCD_WIDTH, 1, DM_RGB(r, g, b));
    }
#else
    (void)top;
    (void)bottom;
    dm_fillrect_c(0, y, LCD_WIDTH, h, DM_WHITE);
#endif
}

static void dm_bar(int y, int h, unsigned top, unsigned bottom)
{
    dm_gradient(y, h, top, bottom);
    dm_drawrect_c(0, y, LCD_WIDTH, h, DM_BORDER);
}

static void dm_clock_text(char *buf, size_t size)
{
    struct tm *tm = rb->get_time();
    int hour = tm ? tm->tm_hour : 0;
    int min = tm ? tm->tm_min : 0;

    rb->snprintf(buf, size, "%d:%02d", hour, min);
}

static void dm_draw_battery(int x, int y)
{
    int level = rb->battery_level();
    int fill;

    if (level < 0)
        level = 0;
    if (level > 100)
        level = 100;
    fill = (18 * level) / 100;

    dm_drawrect_c(x, y, 22, 8, DM_MUTED);
    dm_fillrect_c(x + 22, y + 2, 2, 4, DM_MUTED);
    dm_fillrect_c(x + 2, y + 2, fill, 4, level > 20 ? DM_BLUE_DARK : DM_MUTED);
}

static void dm_draw_menubar(void)
{
    char buf[16];
    struct mp3entry *id3 = dm_settings.show_now_playing ?
                            rb->audio_current_track() : NULL;

    dm_bar(0, 18, DM_MENUBAR_TOP, DM_MENUBAR_BOTTOM);
    dm_text_c(7, 4, 110, "Desktop Mode", DM_TEXT, false);
    if (id3 && id3->title)
        dm_text_fit(112, 4, LCD_WIDTH - 198, id3->title, DM_MUTED, true);
    dm_clock_text(buf, sizeof(buf));
    dm_text_c(LCD_WIDTH - 76, 4, 40, buf, DM_TEXT, true);
    dm_draw_battery(LCD_WIDTH - 31, 5);
}

static void dm_draw_glyph(int x, int y, int size, int glyph, bool selected)
{
    unsigned base = selected ? DM_BLUE : DM_PANEL;
    unsigned dark = selected ? DM_BLUE_DARK : DM_PANEL_DARK;
    int cx = x + size / 2;
    int cy = y + size / 2;

    dm_fillrect_c(x + 2, y + 2, size - 4, size - 4, base);
    dm_drawrect_c(x + 2, y + 2, size - 4, size - 4, DM_WHITE);
    dm_drawrect_c(x + 3, y + 3, size - 6, size - 6, dark);

    switch (glyph)
    {
        case 0:
            dm_fillrect_c(x + 9, y + 12, size - 18, size / 2, DM_WHITE);
            dm_drawrect_c(x + 9, y + 12, size - 18, size / 2, dark);
            dm_fillrect_c(x + 13, y + 9, size / 3, 5, DM_WHITE);
            break;
        case 1:
            dm_fillrect_c(x + 12, y + 8, size - 24, size - 16, DM_WHITE);
            dm_drawrect_c(x + 12, y + 8, size - 24, size - 16, dark);
            dm_fillrect_c(x + 16, y + 16, size - 32, 2, dark);
            dm_fillrect_c(x + 16, y + 23, size - 32, 2, dark);
            break;
        case 2:
            dm_fillrect_c(x + 10, y + 10, size - 20, size - 20, DM_WHITE);
            dm_drawrect_c(x + 10, y + 10, size - 20, size - 20, dark);
            dm_fillrect_c(x + 10, y + 17, size - 20, 2, dark);
            dm_fillrect_c(x + 18, y + 20, 7, 6, DM_BLUE_DARK);
            break;
        case 3:
            dm_fillrect_c(x + 12, y + 9, size - 24, size - 18, DM_WHITE);
            dm_drawrect_c(x + 12, y + 9, size - 24, size - 18, dark);
            dm_fillrect_c(cx - 8, cy - 1, 16, 2, dark);
            dm_fillrect_c(cx - 1, cy - 8, 2, 16, dark);
            break;
        case 4:
            dm_fillrect_c(x + 10, y + 14, size - 20, size - 22, DM_WHITE);
            dm_drawrect_c(x + 10, y + 14, size - 20, size - 22, dark);
            dm_drawrect_c(cx - 7, cy - 5, 14, 14, dark);
            break;
        case 5:
        case 6:
            dm_drawrect_c(x + 13, y + 12, size - 26, size - 24, DM_WHITE);
            dm_fillrect_c(cx - 12, cy - 2, 24, 4, DM_WHITE);
            dm_fillrect_c(cx - 2, cy - 12, 4, 24, DM_WHITE);
            break;
        case 7:
            dm_fillrect_c(x + 12, y + 11, size - 24, 8, DM_WHITE);
            dm_fillrect_c(x + 12, y + 23, size - 24, 8, DM_WHITE);
            dm_fillrect_c(x + 12, y + 35, size - 24, 8, DM_WHITE);
            break;
        default:
            dm_drawrect_c(cx - 10, cy - 10, 20, 20, DM_WHITE);
            dm_fillrect_c(cx - 2, cy - 2, 4, 4, DM_WHITE);
            break;
    }
}

static bool dm_app_available(int selected)
{
    const struct dm_app *app = &dm_apps[selected];

    return selected == 0 || (app->path && rb->file_exists(app->path));
}

static void dm_draw_file_badge(int x, int y, bool is_dir, bool selected)
{
    unsigned fg = selected ? DM_WHITE : is_dir ? DM_BLUE_DARK : DM_MUTED;
    unsigned bg = selected ? DM_BLUE : DM_WHITE;

    dm_fillrect_c(x, y + 2, 11, 10, bg);
    dm_drawrect_c(x, y + 2, 11, 10, fg);
    if (is_dir)
    {
        dm_fillrect_c(x + 2, y, 5, 3, bg);
        dm_drawrect_c(x + 2, y, 5, 4, fg);
    }
    else
    {
        dm_fillrect_c(x + 7, y + 3, 3, 3, fg);
    }
}

static void dm_draw_scrollbar(int x, int y, int h, int count, int top, int rows)
{
    int thumb_h;
    int thumb_y;

    if (count <= rows || h <= 8)
        return;

    thumb_h = MAX(8, (h * rows) / count);
    thumb_y = y + ((h - thumb_h) * top) / MAX(1, count - rows);

    dm_fillrect_c(x, y, 4, h, DM_PANEL_DARK);
    dm_fillrect_c(x + 1, thumb_y, 2, thumb_h, DM_BLUE_DARK);
}

static void dm_draw_desktop(int selected)
{
    int top = 18;
    int dock_h = LCD_HEIGHT >= 220 ? 42 : 32;
    int work_h = LCD_HEIGHT - top - dock_h;
    int cols = LCD_WIDTH >= 300 ? 3 : 2;
    int rows = 3;
    int icon = LCD_WIDTH >= 300 ? 36 : 30;
    int cell_w = LCD_WIDTH / cols;
    int cell_h = work_h / rows;
    int i;
    int dock_y = LCD_HEIGHT - dock_h;
    int dock_item_w = LCD_WIDTH / DM_DOCK_ITEMS;

    dm_gradient(18, LCD_HEIGHT - 18, DM_DESKTOP_TOP, DM_DESKTOP_BOTTOM);
    dm_draw_menubar();

    for (i = 0; i < (int)ARRAYLEN(dm_apps); i++)
    {
        int col = i % cols;
        int row = i / cols;
        int x = col * cell_w + (cell_w - icon) / 2;
        int y = top + row * cell_h + 4;
        bool sel = selected == i;
        bool available = dm_app_available(i);

        if (row >= rows)
            break;

        if (sel)
        {
            dm_fillrect_c(col * cell_w + 6, y - 3, cell_w - 12,
                          cell_h - 6, DM_BLUE_DARK);
            dm_drawrect_c(col * cell_w + 6, y - 3, cell_w - 12,
                          cell_h - 6, DM_WHITE);
        }
        dm_draw_glyph(x, y, icon, dm_apps[i].glyph, sel);
        dm_text_fit(col * cell_w + 4, y + icon + 3, cell_w - 8,
                    dm_apps[i].name,
                    sel ? DM_WHITE : available ? DM_TEXT : DM_MUTED, true);
        if (LCD_HEIGHT >= 220)
            dm_text_fit(col * cell_w + 4, y + icon + 14, cell_w - 8,
                        available ? dm_apps[i].subtitle : "Missing",
                        sel ? DM_WHITE : DM_MUTED, true);
    }

    dm_bar(dock_y, dock_h, DM_DOCK_TOP, DM_DOCK_BOTTOM);
    for (i = 0; i < DM_DOCK_ITEMS; i++)
    {
        int app = i;
        int size = selected == app ? 28 : 23;
        int x = i * dock_item_w + (dock_item_w - size) / 2;
        int y = dock_y + (dock_h - size) / 2 - 1;

        dm_draw_glyph(x, y, size, dm_apps[app].glyph, selected == app);
    }
}

static const char *dm_command_name(enum dm_command command)
{
    switch (command)
    {
        case DM_COMMAND_OPEN:
            return "Open";
        case DM_COMMAND_FINDER:
            return "Finder";
        case DM_COMMAND_SETTINGS:
            return "Settings";
        case DM_COMMAND_REFRESH:
            return "Refresh";
        case DM_COMMAND_PARENT:
            return "Parent";
        case DM_COMMAND_DESKTOP:
            return "Desktop";
        case DM_COMMAND_EXIT:
            return "Exit";
    }
    return "";
}

static const char *dm_command_hint(enum dm_command command)
{
    switch (command)
    {
        case DM_COMMAND_OPEN:
            return "Activate the selected item";
        case DM_COMMAND_FINDER:
            return "Open the file browser";
        case DM_COMMAND_SETTINGS:
            return "Adjust Desktop Mode";
        case DM_COMMAND_REFRESH:
            return "Redraw and rescan";
        case DM_COMMAND_PARENT:
            return "Move up one folder";
        case DM_COMMAND_DESKTOP:
            return "Return to the desktop";
        case DM_COMMAND_EXIT:
            return "Close Desktop Mode";
    }
    return "";
}

static const enum dm_command *dm_commands_for_mode(enum dm_mode mode, int *count)
{
    if (mode == DM_MODE_FINDER)
    {
        *count = ARRAYLEN(dm_finder_commands);
        return dm_finder_commands;
    }

    *count = ARRAYLEN(dm_desktop_commands);
    return dm_desktop_commands;
}

static const char *dm_setting_name(int setting)
{
    switch (setting)
    {
        case DM_SETTING_NOW_PLAYING:
            return "Now Playing";
        case DM_SETTING_START_VIEW:
            return "Start View";
        case DM_SETTING_RETURN:
            return "Done";
    }
    return "";
}

static const char *dm_setting_value(int setting)
{
    switch (setting)
    {
        case DM_SETTING_NOW_PLAYING:
            return dm_settings.show_now_playing ? "On" : "Off";
        case DM_SETTING_START_VIEW:
            return dm_settings.start_in_finder ? "Finder" : "Desktop";
        case DM_SETTING_RETURN:
            return "Close";
    }
    return "";
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

static void dm_draw_command_overlay(enum dm_mode mode, int selected)
{
    int count;
    int i;
    const enum dm_command *commands = dm_commands_for_mode(mode, &count);
    int w = MIN(LCD_WIDTH - 28, 244);
    int h = 28 + count * 24;
    int x = (LCD_WIDTH - w) / 2;
    int y = (LCD_HEIGHT - h) / 2;

    dm_fillrect_c(x + 3, y + 4, w, h, DM_SHADOW);
    dm_fillrect_c(x, y, w, h, DM_PANEL);
    dm_drawrect_c(x, y, w, h, DM_WHITE);
    dm_drawrect_c(x + 1, y + 1, w - 2, h - 2, DM_BORDER);
    dm_bar(y + 1, 18, DM_MENUBAR_TOP, DM_MENUBAR_BOTTOM);
    dm_text_fit(x + 7, y + 5, w - 14, "Desktop Commands", DM_TEXT, false);

    for (i = 0; i < count; i++)
    {
        int row_y = y + 24 + i * 24;

        if (i == selected)
        {
            dm_fillrect_c(x + 6, row_y, w - 12, 21, DM_BLUE);
            dm_text_fit(x + 13, row_y + 3, 76,
                        dm_command_name(commands[i]), DM_WHITE, false);
            dm_text_fit(x + 94, row_y + 3, w - 106,
                        dm_command_hint(commands[i]), DM_WHITE, false);
        }
        else
        {
            dm_text_fit(x + 13, row_y + 3, 76,
                        dm_command_name(commands[i]), DM_TEXT, false);
            dm_text_fit(x + 94, row_y + 3, w - 106,
                        dm_command_hint(commands[i]), DM_MUTED, false);
        }
    }
}

static void dm_draw_settings(int selected)
{
    int w = MIN(LCD_WIDTH - 24, 250);
    int h = 104;
    int x = (LCD_WIDTH - w) / 2;
    int y = 54;
    int i;

    dm_gradient(18, LCD_HEIGHT - 18, DM_DESKTOP_TOP, DM_DESKTOP_BOTTOM);
    dm_draw_menubar();

    dm_fillrect_c(x + 3, y + 4, w, h, DM_SHADOW);
    dm_fillrect_c(x, y, w, h, DM_PANEL);
    dm_drawrect_c(x, y, w, h, DM_WHITE);
    dm_drawrect_c(x + 1, y + 1, w - 2, h - 2, DM_BORDER);
    dm_bar(y + 1, 18, DM_MENUBAR_TOP, DM_MENUBAR_BOTTOM);
    dm_text_fit(x + 7, y + 5, w - 14, "Desktop Settings", DM_TEXT, false);

    for (i = 0; i < DM_SETTING_COUNT; i++)
    {
        int row_y = y + 27 + i * 23;

        if (i == selected)
        {
            dm_fillrect_c(x + 6, row_y, w - 12, 20, DM_BLUE);
            dm_text_fit(x + 13, row_y + 3, w - 82,
                        dm_setting_name(i), DM_WHITE, false);
            dm_text_fit(x + w - 68, row_y + 3, 54,
                        dm_setting_value(i), DM_WHITE, true);
        }
        else
        {
            dm_text_fit(x + 13, row_y + 3, w - 82,
                        dm_setting_name(i), DM_TEXT, false);
            dm_text_fit(x + w - 68, row_y + 3, 54,
                        dm_setting_value(i), DM_MUTED, true);
        }
    }

    dm_text_fit(10, LCD_HEIGHT - 14, LCD_WIDTH - 20,
                "Changes save automatically",
                DM_MUTED, true);
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

static void dm_draw_finder(void)
{
    int i;
    int row_h = (LCD_HEIGHT - 62) / DM_FILE_ROWS;
    int list_x = LCD_WIDTH >= 300 ? 84 : 6;
    int list_w = LCD_WIDTH - list_x - 6;
    int text_w = list_w - (dm_file_count > DM_FILE_ROWS ? 32 : 24);
    char status[80];
    char count_text[24];

    dm_gradient(18, LCD_HEIGHT - 18, DM_DESKTOP_TOP, DM_DESKTOP_BOTTOM);
    dm_draw_menubar();

    dm_fillrect_c(6, 24, LCD_WIDTH - 12, LCD_HEIGHT - 30, DM_PANEL);
    dm_drawrect_c(6, 24, LCD_WIDTH - 12, LCD_HEIGHT - 30, DM_BORDER);
    dm_bar(25, 18, DM_MENUBAR_TOP, DM_MENUBAR_BOTTOM);
    dm_text_fit(12, 29, LCD_WIDTH - 24, dm_cwd, DM_TEXT, false);

    if (LCD_WIDTH >= 300)
    {
        dm_fillrect_c(12, 50, 62, LCD_HEIGHT - 62, DM_DOCK_TOP);
        dm_drawrect_c(12, 50, 62, LCD_HEIGHT - 62, DM_PANEL_DARK);
        dm_text_c(18, 58, 50, "Places", DM_MUTED, false);
        dm_fillrect_c(16, 73, 52, 18, DM_PANEL);
        dm_text_c(18, 76, 50, "Disk", DM_TEXT, false);
        dm_text_c(18, 94, 50, "Apps", DM_TEXT, false);
    }

    if (dm_file_count == 0)
    {
        dm_text_fit(list_x + 4, 107, list_w - 8,
                    "Folder is empty", DM_MUTED, true);
    }

    for (i = 0; i < DM_FILE_ROWS; i++)
    {
        int idx = dm_file_top + i;
        int row_y = 50 + i * row_h;
        bool selected = idx == dm_file_sel;

        if (idx >= dm_file_count)
            break;

        if (selected)
        {
            dm_fillrect_c(list_x, row_y, list_w, row_h - 2, DM_BLUE);
            dm_draw_file_badge(list_x + 6, row_y + 5,
                               dm_files[idx].is_dir, true);
            dm_text_fit(list_x + 22, row_y + 4, text_w,
                        dm_files[idx].name, DM_WHITE, false);
        }
        else
        {
            dm_fillrect_c(list_x, row_y, list_w, row_h - 2,
                          (i & 1) ? DM_WHITE : DM_PANEL);
            dm_draw_file_badge(list_x + 6, row_y + 5,
                               dm_files[idx].is_dir, false);
            dm_text_fit(list_x + 22, row_y + 4, text_w,
                        dm_files[idx].name,
                        dm_files[idx].is_dir ? DM_BLUE_DARK : DM_TEXT, false);
        }
        if (dm_files[idx].is_dir)
            dm_text_c(list_x + list_w - 16, row_y + 4, 10, ">",
                      selected ? DM_WHITE : DM_MUTED, true);
    }

    dm_draw_scrollbar(list_x + list_w - 6, 52, DM_FILE_ROWS * row_h - 4,
                      dm_file_count, dm_file_top, DM_FILE_ROWS);
    rb->snprintf(count_text, sizeof(count_text), "%d item%s", dm_file_count,
                 dm_file_count == 1 ? "" : "s");
    dm_status_text(status, sizeof(status), dm_cwd, count_text);
    dm_text_fit(10, LCD_HEIGHT - 14, LCD_WIDTH - 20, status, DM_MUTED, true);
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

static int dm_launch_app(int selected)
{
    const struct dm_app *app = &dm_apps[selected];

    if (selected == 0)
    {
        rb->strcpy(dm_cwd, "/");
        dm_scan_dir();
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

static int dm_handle_command(enum dm_command command, enum dm_mode *mode,
                             int selected)
{
    switch (command)
    {
        case DM_COMMAND_OPEN:
            if (*mode == DM_MODE_DESKTOP)
                return dm_launch_app(selected);
            return dm_open_selected_file();

        case DM_COMMAND_FINDER:
            rb->strcpy(dm_cwd, "/");
            dm_scan_dir();
            *mode = DM_MODE_FINDER;
            break;

        case DM_COMMAND_SETTINGS:
            dm_settings_return_mode = *mode;
            *mode = DM_MODE_SETTINGS;
            break;

        case DM_COMMAND_REFRESH:
            if (*mode == DM_MODE_FINDER)
                dm_scan_dir();
            break;

        case DM_COMMAND_PARENT:
            if (*mode == DM_MODE_FINDER)
                dm_finder_parent(mode);
            break;

        case DM_COMMAND_DESKTOP:
            *mode = DM_MODE_DESKTOP;
            break;

        case DM_COMMAND_EXIT:
            return PLUGIN_OK + 1;
    }

    return PLUGIN_OK;
}

enum plugin_status plugin_start(const void *parameter)
{
    enum dm_mode mode = DM_MODE_DESKTOP;
    int selected = 0;
    int setting_sel = 0;
    int command_sel = 0;
    int ret = PLUGIN_OK;
    bool redraw = true;
    bool commands_open = false;

    (void)parameter;
    rb->lcd_setfont(FONT_UI);
    dm_load_settings();
    if (dm_settings.start_in_finder)
    {
        rb->strcpy(dm_cwd, "/");
        dm_scan_dir();
        mode = DM_MODE_FINDER;
    }

    while (ret == PLUGIN_OK)
    {
        int action;

        if (redraw)
        {
            if (mode == DM_MODE_DESKTOP)
                dm_draw_desktop(selected);
            else if (mode == DM_MODE_FINDER)
                dm_draw_finder();
            else
                dm_draw_settings(setting_sel);
            if (commands_open)
                dm_draw_command_overlay(mode, command_sel);
            rb->lcd_update();
            redraw = false;
        }

        action = rb->get_action(CONTEXT_LIST, HZ / 10);
        switch (action)
        {
            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (commands_open)
                {
                    int command_count;

                    dm_commands_for_mode(mode, &command_count);
                    command_sel--;
                    if (command_sel < 0)
                        command_sel = command_count - 1;
                }
                else if (mode == DM_MODE_SETTINGS)
                {
                    setting_sel--;
                    if (setting_sel < 0)
                        setting_sel = DM_SETTING_COUNT - 1;
                }
                else if (mode == DM_MODE_DESKTOP)
                {
                    selected--;
                    if (selected < 0)
                        selected = ARRAYLEN(dm_apps) - 1;
                }
                else if (dm_file_count > 0)
                {
                    dm_file_sel--;
                    if (dm_file_sel < 0)
                        dm_file_sel = dm_file_count - 1;
                    if (dm_file_sel < dm_file_top)
                        dm_file_top = dm_file_sel;
                    if (dm_file_sel >= dm_file_top + DM_FILE_ROWS)
                        dm_file_top = dm_file_sel - DM_FILE_ROWS + 1;
                }
                redraw = true;
                break;

            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (commands_open)
                {
                    int command_count;

                    dm_commands_for_mode(mode, &command_count);
                    command_sel++;
                    if (command_sel >= command_count)
                        command_sel = 0;
                }
                else if (mode == DM_MODE_SETTINGS)
                {
                    setting_sel++;
                    if (setting_sel >= DM_SETTING_COUNT)
                        setting_sel = 0;
                }
                else if (mode == DM_MODE_DESKTOP)
                {
                    selected++;
                    if (selected >= (int)ARRAYLEN(dm_apps))
                        selected = 0;
                }
                else if (dm_file_count > 0)
                {
                    dm_file_sel++;
                    if (dm_file_sel >= dm_file_count)
                        dm_file_sel = 0;
                    if (dm_file_sel < dm_file_top)
                        dm_file_top = dm_file_sel;
                    if (dm_file_sel >= dm_file_top + DM_FILE_ROWS)
                        dm_file_top = dm_file_sel - DM_FILE_ROWS + 1;
                }
                redraw = true;
                break;

            case ACTION_STD_OK:
                if (commands_open)
                {
                    int command_count;
                    const enum dm_command *commands =
                        dm_commands_for_mode(mode, &command_count);

                    commands_open = false;
                    if (command_sel >= command_count)
                        command_sel = 0;
                    ret = dm_handle_command(commands[command_sel],
                                            &mode, selected);
                    if (ret == PLUGIN_OK && mode == DM_MODE_DESKTOP &&
                        commands[command_sel] == DM_COMMAND_OPEN &&
                        selected == 0)
                        mode = DM_MODE_FINDER;
                }
                else if (mode == DM_MODE_DESKTOP)
                {
                    ret = dm_launch_app(selected);
                    if (ret == PLUGIN_OK && selected == 0)
                        mode = DM_MODE_FINDER;
                }
                else if (mode == DM_MODE_SETTINGS)
                {
                    if (setting_sel == DM_SETTING_RETURN)
                        mode = dm_settings_return_mode;
                    else
                        dm_toggle_setting(setting_sel);
                }
                else
                {
                    ret = dm_open_selected_file();
                }
                redraw = true;
                break;

            case ACTION_STD_CONTEXT:
            case ACTION_STD_QUICKSCREEN:
                if (mode != DM_MODE_SETTINGS)
                {
                    commands_open = !commands_open;
                    command_sel = 0;
                    redraw = true;
                }
                break;

            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                if (commands_open)
                {
                    commands_open = false;
                    redraw = true;
                }
                else if (mode == DM_MODE_SETTINGS)
                {
                    dm_save_settings();
                    mode = dm_settings_return_mode;
                    redraw = true;
                }
                else if (mode == DM_MODE_FINDER)
                {
                    dm_finder_parent(&mode);
                    redraw = true;
                }
                else
                    ret = PLUGIN_OK + 1;
                break;

            default:
                if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
                break;
        }
    }

    rb->lcd_setfont(FONT_UI);
    return ret == PLUGIN_OK + 1 ? PLUGIN_OK : ret;
}
