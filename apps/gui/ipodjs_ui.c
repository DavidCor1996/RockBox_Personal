/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 by The Rockbox Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include <string.h>
#include <limits.h>
#include "config.h"
#include "system.h"
#include "font.h"
#include "lcd.h"
#include "file.h"
#include "settings.h"
#include "rbpaths.h"
#include "string-extra.h"
#include "ipodjs_ui.h"

#if defined(HAVE_LCD_COLOR) && (defined(IPOD_VIDEO) || defined(IPOD_6G))

#define IPODJS_UI_HEADER_TOP       LCD_RGBPACK(252, 253, 253)
#define IPODJS_UI_HEADER_MID       LCD_RGBPACK(216, 219, 223)
#define IPODJS_UI_HEADER_BOTTOM    LCD_RGBPACK(174, 178, 183)
#define IPODJS_UI_SCREEN_BG        LCD_RGBPACK(255, 255, 255)
#define IPODJS_UI_TEXT             LCD_RGBPACK(0, 0, 0)
#define IPODJS_UI_ACTIVE_TOP       LCD_RGBPACK(107, 200, 254)
#define IPODJS_UI_ACTIVE_MID       LCD_RGBPACK(38, 146, 226)
#define IPODJS_UI_ACTIVE_BOTTOM    LCD_RGBPACK(0, 92, 192)
#define IPODJS_UI_GRAPHITE         LCD_RGBPACK(84, 90, 100)
#define IPODJS_UI_U2_RED           LCD_RGBPACK(182, 24, 35)
#define IPODJS_UI_TEAL             LCD_RGBPACK(0, 128, 132)
#define IPODJS_UI_GREEN            LCD_RGBPACK(55, 142, 64)
#define IPODJS_UI_GOLD             LCD_RGBPACK(184, 135, 38)
#define IPODJS_UI_ORANGE           LCD_RGBPACK(208, 104, 32)
#define IPODJS_UI_PURPLE           LCD_RGBPACK(113, 82, 170)
#define IPODJS_UI_PINK             LCD_RGBPACK(195, 72, 128)
#define IPODJS_UI_MUTED_TEXT       LCD_RGBPACK(99, 101, 103)
#define IPODJS_UI_ASSET_DIR        ROCKBOX_DIR "/ipodjs"
#define IPODJS_UI_LABEL_CACHE_SIZE 32

struct ipodjs_ui_label_cache_entry {
    bool valid;
    unsigned long stamp;
    int font;
    int width;
    int fit_width;
    int fit_pixels;
    char text[64];
    char fit[64];
};

static struct ipodjs_ui_label_cache_entry
    ipodjs_ui_label_cache[IPODJS_UI_LABEL_CACHE_SIZE];
static unsigned long ipodjs_ui_label_cache_stamp;

bool ipodjs_ui_enabled(enum screen_type screen)
{
    return screen == SCREEN_MAIN &&
           global_settings.ui_engine == UI_ENGINE_IPODJS;
}

int ipodjs_ui_row_height(void)
{
    int row_h = global_settings.ui_engine_density == UI_ENGINE_DENSITY_COMPACT ?
        20 : 24;

    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_SMALL)
        row_h -= 2;
    else if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_LARGE)
        row_h += 4;

    return MAX(18, row_h);
}

int ipodjs_ui_font(void)
{
    static int small_font = -2;
    static int normal_font = -2;
    static int large_font = -2;
    int *fontp;
    const char *path;
    const char *fallback_path;

    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_SMALL)
    {
        fontp = &small_font;
        path = IPODJS_UI_ASSET_DIR "/14-Adobe-Helvetica-Bold.fnt";
        fallback_path = FONT_DIR "/14-Adobe-Helvetica-Bold.fnt";
    }
    else if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_LARGE)
    {
        fontp = &large_font;
        path = IPODJS_UI_ASSET_DIR "/18-Adobe-Helvetica-Bold.fnt";
        fallback_path = FONT_DIR "/18-Adobe-Helvetica-Bold.fnt";
    }
    else
    {
        fontp = &normal_font;
        path = IPODJS_UI_ASSET_DIR "/16-Adobe-Helvetica-Bold.fnt";
        fallback_path = FONT_DIR "/16-Adobe-Helvetica-Bold.fnt";
    }

    if (*fontp < 0)
    {
        if (file_exists(path))
        {
            int loaded = font_load(path);
            if (loaded >= 0)
            {
                font_lock(loaded, true);
                *fontp = loaded;
            }
        }

        if (*fontp < 0 && file_exists(fallback_path))
        {
            int loaded = font_load(fallback_path);
            if (loaded >= 0)
            {
                font_lock(loaded, true);
                *fontp = loaded;
            }
        }
    }

    return *fontp >= 0 ? *fontp : FONT_SYSFIXED;
}

int ipodjs_ui_text_y_offset(void)
{
    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_SMALL)
        return -1;
    if (global_settings.ui_engine_font_scale == UI_ENGINE_FONT_LARGE)
        return 1;
    return 0;
}

unsigned ipodjs_ui_accent(void)
{
    switch (global_settings.ui_engine_accent)
    {
        case UI_ENGINE_ACCENT_GRAPHITE:
            return IPODJS_UI_GRAPHITE;
        case UI_ENGINE_ACCENT_U2:
            return IPODJS_UI_U2_RED;
        case UI_ENGINE_ACCENT_TEAL:
            return IPODJS_UI_TEAL;
        case UI_ENGINE_ACCENT_GREEN:
            return IPODJS_UI_GREEN;
        case UI_ENGINE_ACCENT_GOLD:
            return IPODJS_UI_GOLD;
        case UI_ENGINE_ACCENT_ORANGE:
            return IPODJS_UI_ORANGE;
        case UI_ENGINE_ACCENT_PURPLE:
            return IPODJS_UI_PURPLE;
        case UI_ENGINE_ACCENT_PINK:
            return IPODJS_UI_PINK;
        case UI_ENGINE_ACCENT_BLUE:
        default:
            return IPODJS_UI_ACTIVE_BOTTOM;
    }
}

static bool ipodjs_ui_dark(void)
{
    return global_settings.ui_engine_dark_mode;
}

unsigned ipodjs_ui_screen_bg(void)
{
    return ipodjs_ui_dark() ? LCD_RGBPACK(18, 20, 24) :
                              IPODJS_UI_SCREEN_BG;
}

unsigned ipodjs_ui_row_bg(void)
{
    return ipodjs_ui_dark() ? LCD_RGBPACK(24, 27, 32) :
                              IPODJS_UI_SCREEN_BG;
}

unsigned ipodjs_ui_text(void)
{
    return ipodjs_ui_dark() ? LCD_RGBPACK(239, 242, 246) :
                              IPODJS_UI_TEXT;
}

unsigned ipodjs_ui_muted_text(void)
{
    return ipodjs_ui_dark() ? LCD_RGBPACK(166, 173, 184) :
                              IPODJS_UI_MUTED_TEXT;
}

unsigned ipodjs_ui_header_text(void)
{
    return ipodjs_ui_dark() ? LCD_RGBPACK(246, 248, 250) :
                              IPODJS_UI_TEXT;
}

unsigned ipodjs_ui_header_bg(void)
{
    return ipodjs_ui_dark() ? LCD_RGBPACK(24, 29, 38) :
                              IPODJS_UI_HEADER_BOTTOM;
}

unsigned ipodjs_ui_panel(void)
{
    if (ipodjs_ui_dark())
        return LCD_RGBPACK(24, 27, 32);
    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_TRANSPARENT)
        return LCD_RGBPACK(248, 249, 250);
    if (global_settings.ui_engine_surface == UI_ENGINE_SURFACE_SOFT)
        return LCD_RGBPACK(236, 238, 241);
    return IPODJS_UI_SCREEN_BG;
}

unsigned ipodjs_ui_rgb_blend(int br, int bg, int bb,
                             int fr, int fg, int fb,
                             int alpha)
{
    alpha = MAX(0, MIN(alpha, 255));
    return LCD_RGBPACK((br * (255 - alpha) + fr * alpha) / 255,
                       (bg * (255 - alpha) + fg * alpha) / 255,
                       (bb * (255 - alpha) + fb * alpha) / 255);
}

void ipodjs_ui_gradient(struct screen *display, int x, int y, int w, int h,
                        unsigned top, unsigned bottom)
{
    if (!display || h <= 0 || w <= 0)
        return;

#ifdef HAVE_LCD_COLOR
    if (display->screen_type == SCREEN_MAIN)
    {
        lcd_gradient_fillrect(x, y, w, h, top, bottom);
        return;
    }
#endif

    int tr = RGB_UNPACK_RED(top);
    int tg = RGB_UNPACK_GREEN(top);
    int tb = RGB_UNPACK_BLUE(top);
    int br = RGB_UNPACK_RED(bottom);
    int bg = RGB_UNPACK_GREEN(bottom);
    int bb = RGB_UNPACK_BLUE(bottom);
    int denom = MAX(1, h - 1);

    for (int row = 0; row < h; row++)
    {
        int r = (tr * (denom - row) + br * row) / denom;
        int g = (tg * (denom - row) + bg * row) / denom;
        int b = (tb * (denom - row) + bb * row) / denom;
        display->set_foreground(LCD_RGBPACK(r, g, b));
        display->hline(x, x + w - 1, y + row);
    }
}

void ipodjs_ui_glass_gradient(struct screen *display, int x, int y,
                              int w, int h, unsigned top,
                              unsigned mid, unsigned bottom)
{
    int upper;

    if (h <= 1)
    {
        ipodjs_ui_gradient(display, x, y, w, h, top, bottom);
        return;
    }

    upper = MAX(1, (h * 45) / 100);
    ipodjs_ui_gradient(display, x, y, w, upper, top, mid);
    ipodjs_ui_gradient(display, x, y + upper, w, h - upper, mid, bottom);
}

void ipodjs_ui_selection_gradient(struct screen *display, int x, int y,
                                  int w, int h, unsigned *midp)
{
    unsigned accent = ipodjs_ui_accent();
    unsigned top;
    unsigned bottom;

    if (global_settings.ui_engine_accent == UI_ENGINE_ACCENT_BLUE)
    {
        ipodjs_ui_glass_gradient(display, x, y, w, h,
                                 IPODJS_UI_ACTIVE_TOP,
                                 IPODJS_UI_ACTIVE_MID,
                                 IPODJS_UI_ACTIVE_BOTTOM);
        if (midp)
            *midp = IPODJS_UI_ACTIVE_MID;
        return;
    }

    top = ipodjs_ui_rgb_blend(FB_UNPACK_RED(accent),
                              FB_UNPACK_GREEN(accent),
                              FB_UNPACK_BLUE(accent),
                              255, 255, 255, 112);
    bottom = ipodjs_ui_rgb_blend(FB_UNPACK_RED(accent),
                                 FB_UNPACK_GREEN(accent),
                                 FB_UNPACK_BLUE(accent),
                                 0, 0, 0, 70);
    ipodjs_ui_glass_gradient(display, x, y, w, h, top, accent, bottom);
    if (midp)
        *midp = accent;
}

static struct ipodjs_ui_label_cache_entry *ipodjs_ui_label_cache_find(
    const char *text, int font)
{
    int i;

    if (!text)
        return NULL;

    for (i = 0; i < IPODJS_UI_LABEL_CACHE_SIZE; i++)
    {
        if (!ipodjs_ui_label_cache[i].valid)
            continue;
        if (ipodjs_ui_label_cache[i].font != font)
            continue;
        if (!strcmp(ipodjs_ui_label_cache[i].text, text))
            return &ipodjs_ui_label_cache[i];
    }

    return NULL;
}

static struct ipodjs_ui_label_cache_entry *ipodjs_ui_label_cache_get(
    struct screen *display, const char *text)
{
    struct ipodjs_ui_label_cache_entry *entry;
    int font;
    int i;

    if (!display || !text || !text[0])
        return NULL;

    font = lcd_getfont();
    if (display->screen_type != SCREEN_MAIN)
        font = ipodjs_ui_font();

    entry = ipodjs_ui_label_cache_find(text, font);
    if (entry)
    {
        entry->stamp = ++ipodjs_ui_label_cache_stamp;
        return entry;
    }

    {
        int victim = -1;
        unsigned long oldest = ULONG_MAX;

        for (i = 0; i < IPODJS_UI_LABEL_CACHE_SIZE; i++)
        {
            if (!ipodjs_ui_label_cache[i].valid)
            {
                victim = i;
                break;
            }

            if (ipodjs_ui_label_cache[i].stamp < oldest)
            {
                oldest = ipodjs_ui_label_cache[i].stamp;
                victim = i;
            }
        }

        entry = &ipodjs_ui_label_cache[victim];
        entry->valid = true;
        entry->stamp = ++ipodjs_ui_label_cache_stamp;
        entry->font = font;
        strmemccpy(entry->text, text, sizeof(entry->text));
        display->getstringsize((const unsigned char *)entry->text,
                               &entry->width, NULL);
        entry->fit_width = -1;
        entry->fit_pixels = -1;
        entry->fit[0] = '\0';
    }

    return entry;
}

void ipodjs_ui_puts_fit(struct screen *display, int x, int y, int width,
                        const char *text, bool center)
{
    char buf[64];
    int w = 0;
    int h = 0;
    int len;
    struct ipodjs_ui_label_cache_entry *entry;

    if (!display || !text || !text[0] || width <= 0)
        return;

    entry = ipodjs_ui_label_cache_get(display, text);
    if (!entry)
    {
        strmemccpy(buf, text, sizeof(buf));
        len = strlen(buf);
        display->getstringsize((const unsigned char *)buf, &w, &h);
        while (len > 1 && w > width)
        {
            buf[--len] = '\0';
            display->getstringsize((const unsigned char *)buf, &w, &h);
        }
    }
    else if (entry->fit_width == width)
    {
        strmemccpy(buf, entry->fit, sizeof(buf));
        w = entry->fit_pixels;
    }
    else
    {
        strmemccpy(buf, entry->text, sizeof(buf));
        len = strlen(buf);
        w = entry->width;
        while (len > 1 && w > width)
        {
            buf[--len] = '\0';
            display->getstringsize((const unsigned char *)buf, &w, &h);
        }

        entry->fit_width = width;
        entry->fit_pixels = w;
        strmemccpy(entry->fit, buf, sizeof(entry->fit));
    }

    if (center && w < width)
        x += (width - w) / 2;

    display->set_drawmode(DRMODE_FG);
    display->putsxy(x, y, (const unsigned char *)buf);
    display->set_drawmode(DRMODE_SOLID);
}

void ipodjs_ui_draw_arrow(struct screen *display, int x, int y,
                          unsigned color)
{
    if (!display)
        return;

    display->set_foreground(color);
    display->fillrect(x, y + 1, 2, 1);
    display->fillrect(x + 2, y + 2, 2, 1);
    display->fillrect(x + 4, y + 3, 2, 1);
    display->fillrect(x + 2, y + 4, 2, 1);
    display->fillrect(x, y + 5, 2, 1);
}

void ipodjs_ui_label_cache_reset(void)
{
    memset(ipodjs_ui_label_cache, 0, sizeof(ipodjs_ui_label_cache));
    ipodjs_ui_label_cache_stamp = 0;
}

#endif
