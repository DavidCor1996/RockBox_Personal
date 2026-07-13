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
#include <stdlib.h>
#include "config.h"
#include "system.h"
#include "font.h"
#include "lcd.h"
#include "file.h"
#include "settings.h"
#include "action.h"
#include "appevents.h"
#include "backlight.h"
#include "button.h"
#include "misc.h"
#include "power.h"
#include "powermgmt.h"
#include "rbpaths.h"
#include "string-extra.h"
#include "rbunicode.h"
#include "usb.h"
#include "ipodjs_ui.h"

#if defined(HAVE_LCD_COLOR) && (defined(IPOD_VIDEO) || defined(IPOD_6G) || defined(IPOD_NANO3G))

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
#define IPODJS_UI_TEXT_SIZE        128
#define IPODJS_CHARGE_BODY_X       92
#define IPODJS_CHARGE_BODY_Y       75
#define IPODJS_CHARGE_BODY_W       145
#define IPODJS_CHARGE_BODY_H       76
#define IPODJS_CHARGE_WELL_X       96
#define IPODJS_CHARGE_WELL_Y       81
#define IPODJS_CHARGE_WELL_W       136
#define IPODJS_CHARGE_WELL_H       64
#define IPODJS_CHARGE_DAMAGE_X     84
#define IPODJS_CHARGE_DAMAGE_Y     70
#define IPODJS_CHARGE_DAMAGE_W     165
#define IPODJS_CHARGE_DAMAGE_H     130
#define IPODJS_CHARGE_FPS          20

struct ipodjs_ui_label_cache_entry {
    bool valid;
    unsigned long stamp;
    int font;
    int width;
    int fit_width;
    int fit_pixels;
    char text[IPODJS_UI_TEXT_SIZE];
    char fit[IPODJS_UI_TEXT_SIZE];
};

static struct ipodjs_ui_label_cache_entry
    ipodjs_ui_label_cache[IPODJS_UI_LABEL_CACHE_SIZE];
static unsigned long ipodjs_ui_label_cache_stamp;
static bool ipodjs_ui_charging_active;
static bool ipodjs_ui_charging_seen;

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

void ipodjs_ui_prepare_native_frame(void)
{
    struct screen *display = &screens[SCREEN_MAIN];
    unsigned top;
    unsigned mid;
    unsigned bottom;

    if (!ipodjs_ui_enabled(SCREEN_MAIN))
        return;

    if (ipodjs_ui_dark())
    {
        top = LCD_RGBPACK(73, 81, 94);
        mid = LCD_RGBPACK(39, 45, 56);
        bottom = LCD_RGBPACK(18, 22, 30);
    }
    else
    {
        top = LCD_RGBPACK(247, 248, 249);
        mid = LCD_RGBPACK(199, 204, 211);
        bottom = LCD_RGBPACK(116, 126, 140);
    }

    display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(ipodjs_ui_screen_bg());
    display->clear_display();
    ipodjs_ui_glass_gradient(display, 0, 0, display->lcdwidth,
                             display->lcdheight, top, mid, bottom);
    display->update();
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

    if (!display || !text || !text[0] ||
        strlen(text) >= IPODJS_UI_TEXT_SIZE)
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

static int ipodjs_ui_utf8_prefix_bytes(const char *text, int chars,
                                       size_t max_bytes)
{
    int bytes;

    if (!text || chars <= 0 || max_bytes == 0)
        return 0;

    bytes = utf8seek((const unsigned char *)text, chars);
    while (bytes > (int)max_bytes && chars > 0)
        bytes = utf8seek((const unsigned char *)text, --chars);
    return MAX(0, bytes);
}

static void ipodjs_ui_fit_text(struct screen *display, const char *text,
                               int width, char *buf, size_t buf_size,
                               int *pixel_width)
{
    static const char ellipsis[] = "...";
    int chars = utf8length((const unsigned char *)text);
    int low = 0;
    int high = chars;
    int best = 0;
    int bytes;
    int w = 0;
    int ellipsis_w = 0;
    bool clipped_by_buffer;

    bytes = ipodjs_ui_utf8_prefix_bytes(text, chars, buf_size - 1);
    memcpy(buf, text, bytes);
    buf[bytes] = '\0';
    clipped_by_buffer = text[bytes] != '\0';
    display->getstringsize((const unsigned char *)buf, &w, NULL);
    if (!clipped_by_buffer && w <= width)
    {
        *pixel_width = w;
        return;
    }

    display->getstringsize((const unsigned char *)ellipsis,
                           &ellipsis_w, NULL);
    width = MAX(0, width - ellipsis_w);
    while (low <= high)
    {
        int mid = low + (high - low) / 2;
        int candidate_w;

        bytes = ipodjs_ui_utf8_prefix_bytes(text, mid, buf_size - 4);
        memcpy(buf, text, bytes);
        buf[bytes] = '\0';
        display->getstringsize((const unsigned char *)buf,
                               &candidate_w, NULL);
        if (candidate_w <= width)
        {
            best = mid;
            low = mid + 1;
        }
        else
            high = mid - 1;
    }

    bytes = ipodjs_ui_utf8_prefix_bytes(text, best, buf_size - 4);
    memcpy(buf, text, bytes);
    memcpy(buf + bytes, ellipsis, sizeof(ellipsis));
    display->getstringsize((const unsigned char *)buf, &w, NULL);
    *pixel_width = w;
}

void ipodjs_ui_puts_fit(struct screen *display, int x, int y, int width,
                        const char *text, bool center)
{
    char buf[IPODJS_UI_TEXT_SIZE];
    int w = 0;
    struct ipodjs_ui_label_cache_entry *entry;

    if (!display || !text || !text[0] || width <= 0)
        return;

    entry = ipodjs_ui_label_cache_get(display, text);
    if (!entry)
        ipodjs_ui_fit_text(display, text, width, buf, sizeof(buf), &w);
    else if (entry->fit_width == width)
    {
        strmemccpy(buf, entry->fit, sizeof(buf));
        w = entry->fit_pixels;
    }
    else
    {
        ipodjs_ui_fit_text(display, entry->text, width, buf, sizeof(buf), &w);

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

struct ipodjs_ui_point {
    int x;
    int y;
};

static int ipodjs_ui_charge_font(void)
{
    static int charge_font = -2;
    const char *path = IPODJS_UI_ASSET_DIR "/14-Adobe-Helvetica-Bold.fnt";
    const char *fallback = FONT_DIR "/14-Adobe-Helvetica-Bold.fnt";

    if (charge_font < 0 && file_exists(path))
        charge_font = font_load(path);
    if (charge_font < 0 && file_exists(fallback))
        charge_font = font_load(fallback);
    if (charge_font >= 0)
        font_lock(charge_font, true);

    return charge_font >= 0 ? charge_font : FONT_SYSFIXED;
}

static void ipodjs_ui_fill_polygon(struct screen *display,
                                   const struct ipodjs_ui_point *points,
                                   int count, int offset_x, int offset_y,
                                   unsigned color)
{
    int intersections[8];
    int min_y = points[0].y;
    int max_y = points[0].y;
    int y;
    int i;

    for (i = 1; i < count; i++)
    {
        min_y = MIN(min_y, points[i].y);
        max_y = MAX(max_y, points[i].y);
    }

    display->set_foreground(color);
    for (y = min_y; y <= max_y; y++)
    {
        int found = 0;
        int edge;

        for (edge = 0; edge < count; edge++)
        {
            const struct ipodjs_ui_point *a = &points[edge];
            const struct ipodjs_ui_point *b = &points[(edge + 1) % count];
            int low_y = MIN(a->y, b->y);
            int high_y = MAX(a->y, b->y);

            if (a->y == b->y || y < low_y || y >= high_y)
                continue;
            if (found < (int)ARRAYLEN(intersections))
            {
                intersections[found++] = a->x +
                    (y - a->y) * (b->x - a->x) / (b->y - a->y);
            }
        }

        for (i = 1; i < found; i++)
        {
            int value = intersections[i];
            int j = i - 1;

            while (j >= 0 && intersections[j] > value)
            {
                intersections[j + 1] = intersections[j];
                j--;
            }
            intersections[j + 1] = value;
        }

        for (i = 0; i + 1 < found; i += 2)
        {
            display->hline(offset_x + intersections[i],
                           offset_x + intersections[i + 1],
                           offset_y + y);
        }
    }
}

static unsigned ipodjs_ui_charge_mix(unsigned from, unsigned to,
                                     int amount, int total)
{
    int alpha = total > 0 ? (amount * 255) / total : 255;

    return ipodjs_ui_rgb_blend(RGB_UNPACK_RED(from),
                               RGB_UNPACK_GREEN(from),
                               RGB_UNPACK_BLUE(from),
                               RGB_UNPACK_RED(to),
                               RGB_UNPACK_GREEN(to),
                               RGB_UNPACK_BLUE(to), alpha);
}

static int ipodjs_ui_charge_round_inset(int row, int height, int radius)
{
    int edge_row;
    int dy;
    int dx = 0;

    radius = MIN(radius, height / 2);
    if (radius <= 0 || (row >= radius && row < height - radius))
        return 0;

    edge_row = row < radius ? row : height - 1 - row;
    dy = radius - 1 - edge_row;
    while ((dx + 1) * (dx + 1) + dy * dy <= radius * radius)
        dx++;

    return MAX(0, radius - 1 - dx);
}

static unsigned ipodjs_ui_charge_gradient_color(unsigned top, unsigned middle,
                                                 unsigned bottom, int row,
                                                 int height)
{
    int split = MAX(1, (height * 42) / 100);

    if (row < split)
        return ipodjs_ui_charge_mix(top, middle, row, MAX(1, split - 1));
    return ipodjs_ui_charge_mix(middle, bottom, row - split,
                                MAX(1, height - split - 1));
}

static void ipodjs_ui_charge_rounded_gradient(struct screen *display,
                                              int x, int y, int width,
                                              int height, int radius,
                                              unsigned top, unsigned middle,
                                              unsigned bottom, int clip_x,
                                              int clip_width)
{
    int clip_right = clip_x + clip_width - 1;
    int row;

    if (width <= 0 || height <= 0 || clip_width <= 0)
        return;

    for (row = 0; row < height; row++)
    {
        int inset = ipodjs_ui_charge_round_inset(row, height, radius);
        int left = MAX(x + inset, clip_x);
        int right = MIN(x + width - 1 - inset, clip_right);

        if (left > right)
            continue;
        display->set_foreground(ipodjs_ui_charge_gradient_color(
            top, middle, bottom, row, height));
        display->hline(left, right, y + row);
    }
}

static void ipodjs_ui_draw_usb_battery(struct screen *display, int x, int y)
{
    int level = MAX(0, MIN(100, battery_level()));
    int fill = 16 * level / 100;

    display->set_foreground(LCD_RGBPACK(218, 222, 226));
    display->drawrect(x, y, 21, 9);
    display->fillrect(x + 21, y + 3, 2, 4);
    display->set_foreground(LCD_RGBPACK(38, 43, 48));
    display->fillrect(x + 2, y + 2, 16, 5);
    if (fill > 0)
    {
        display->set_foreground(level <= 15 ?
            LCD_RGBPACK(220, 54, 48) : LCD_RGBPACK(122, 190, 76));
        display->fillrect(x + 2, y + 2, fill, 5);
    }
}

static void ipodjs_ui_draw_usb_lock(struct screen *display, int x, int y)
{
    unsigned color = LCD_RGBPACK(222, 225, 229);

    display->set_foreground(color);
    display->drawrect(x + 2, y, 7, 7);
    display->fillrect(x, y + 5, 11, 8);
    display->set_foreground(LCD_RGBPACK(42, 47, 52));
    display->fillrect(x + 5, y + 8, 1, 3);
}

static void ipodjs_ui_draw_thick_segment(struct screen *display,
                                         int x1, int y1, int x2, int y2,
                                         unsigned color)
{
    display->set_foreground(color);
    for (int offset = -3; offset <= 3; offset++)
    {
        display->drawline(x1 + offset, y1, x2 + offset, y2);
        display->drawline(x1, y1 + offset, x2, y2 + offset);
    }
}

static void ipodjs_ui_draw_usb_sync_mark(struct screen *display, int cx,
                                         int cy, bool dark)
{
    static const struct ipodjs_ui_point upper_arrow[] = {
        { 13, -10 }, { 27, -8 }, { 20, 5 }
    };
    static const struct ipodjs_ui_point lower_arrow[] = {
        { -13, 10 }, { -27, 8 }, { -20, -5 }
    };
    unsigned mark = dark ? LCD_RGBPACK(22, 25, 29) :
                           LCD_RGBPACK(34, 37, 40);

    ipodjs_ui_draw_thick_segment(display, cx - 20, cy - 5,
                                 cx - 17, cy - 14, mark);
    ipodjs_ui_draw_thick_segment(display, cx - 17, cy - 14,
                                 cx - 9, cy - 20, mark);
    ipodjs_ui_draw_thick_segment(display, cx - 9, cy - 20,
                                 cx + 3, cy - 22, mark);
    ipodjs_ui_draw_thick_segment(display, cx + 3, cy - 22,
                                 cx + 15, cy - 17, mark);
    ipodjs_ui_draw_thick_segment(display, cx + 15, cy - 17,
                                 cx + 20, cy - 8, mark);
    ipodjs_ui_fill_polygon(display, upper_arrow,
                           ARRAYLEN(upper_arrow), cx, cy, mark);

    ipodjs_ui_draw_thick_segment(display, cx + 20, cy + 5,
                                 cx + 17, cy + 14, mark);
    ipodjs_ui_draw_thick_segment(display, cx + 17, cy + 14,
                                 cx + 9, cy + 20, mark);
    ipodjs_ui_draw_thick_segment(display, cx + 9, cy + 20,
                                 cx - 3, cy + 22, mark);
    ipodjs_ui_draw_thick_segment(display, cx - 3, cy + 22,
                                 cx - 15, cy + 17, mark);
    ipodjs_ui_draw_thick_segment(display, cx - 15, cy + 17,
                                 cx - 20, cy + 8, mark);
    ipodjs_ui_fill_polygon(display, lower_arrow,
                           ARRAYLEN(lower_arrow), cx, cy, mark);
}

void ipodjs_ui_usb_prepare(void)
{
    int normal = ipodjs_ui_font();
    int bold = ipodjs_ui_charge_font();

    font_getstringsize("iPod", NULL, NULL, normal);
    font_getstringsize("Connected", NULL, NULL, bold);
    font_getstringsize("Eject Before Disconnecting", NULL, NULL, normal);
}

void ipodjs_ui_draw_usb_connected(struct screen *display)
{
    struct viewport *last_vp;
    bool dark = global_settings.ui_engine_dark_mode;
    int cx = display->lcdwidth / 2;
    int icon_cy = 101;
    int normal = ipodjs_ui_font();
    int bold = ipodjs_ui_charge_font();

    last_vp = display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(dark ? LCD_RGBPACK(3, 12, 21) :
                                   LCD_RGBPACK(4, 43, 64));
    display->clear_display();
    ipodjs_ui_glass_gradient(display, 0, 23, display->lcdwidth,
                             display->lcdheight - 23,
                             dark ? LCD_RGBPACK(18, 91, 128) :
                                    LCD_RGBPACK(29, 137, 181),
                             dark ? LCD_RGBPACK(5, 49, 76) :
                                    LCD_RGBPACK(5, 91, 132),
                             dark ? LCD_RGBPACK(1, 13, 24) :
                                    LCD_RGBPACK(1, 29, 48));

    ipodjs_ui_glass_gradient(display, 0, 0, display->lcdwidth, 23,
                             LCD_RGBPACK(70, 70, 70),
                             LCD_RGBPACK(32, 36, 40),
                             LCD_RGBPACK(8, 14, 19));
    display->set_foreground(LCD_RGBPACK(106, 116, 124));
    display->hline(0, display->lcdwidth - 1, 22);
    display->setfont(normal);
    display->set_foreground(LCD_RGBPACK(234, 237, 240));
    ipodjs_ui_puts_fit(display, 90, 3, display->lcdwidth - 180,
                       "iPod", true);
    ipodjs_ui_draw_usb_battery(display, display->lcdwidth - 32, 7);
    if (button_hold())
        ipodjs_ui_draw_usb_lock(display, 8, 5);

    ipodjs_ui_charge_rounded_gradient(display, cx - 43, icon_cy - 42,
                                      88, 88, 44,
                                      LCD_RGBPACK(10, 27, 36),
                                      LCD_RGBPACK(3, 14, 21),
                                      LCD_RGBPACK(1, 8, 13),
                                      cx - 43, 88);
    ipodjs_ui_charge_rounded_gradient(display, cx - 44, icon_cy - 44,
                                      88, 88, 44,
                                      LCD_RGBPACK(254, 223, 125),
                                      LCD_RGBPACK(222, 165, 55),
                                      LCD_RGBPACK(160, 100, 18),
                                      cx - 44, 88);
    ipodjs_ui_draw_usb_sync_mark(display, cx, icon_cy, dark);

    display->setfont(bold);
    display->set_foreground(LCD_RGBPACK(246, 248, 250));
    ipodjs_ui_puts_fit(display, 24, 167, display->lcdwidth - 48,
                       "Connected", true);
    display->setfont(normal);
    display->set_foreground(LCD_RGBPACK(224, 233, 239));
    ipodjs_ui_puts_fit(display, 20, 190, display->lcdwidth - 40,
                       "Eject Before Disconnecting", true);

    display->update();
    display->set_viewport(last_vp);
}

static void ipodjs_ui_draw_charge_bolt(struct screen *display, bool dark)
{
    static const struct ipodjs_ui_point bolt[] = {
        { 15, 0 }, { 3, 25 }, { 12, 24 }, { 7, 48 },
        { 28, 17 }, { 18, 18 }, { 23, 0 }
    };
    unsigned edge = dark ? LCD_RGBPACK(20, 23, 28) :
                           LCD_RGBPACK(38, 41, 45);
    unsigned face = dark ? LCD_RGBPACK(39, 43, 49) :
                           LCD_RGBPACK(52, 55, 59);

    ipodjs_ui_fill_polygon(display, bolt, ARRAYLEN(bolt),
                           148, 88, edge);
    ipodjs_ui_fill_polygon(display, bolt, ARRAYLEN(bolt),
                           149, 89, face);
}

static void ipodjs_ui_draw_charge_plug(struct screen *display, bool dark)
{
    unsigned top = dark ? LCD_RGBPACK(55, 61, 68) :
                          LCD_RGBPACK(65, 69, 73);
    unsigned color = dark ? LCD_RGBPACK(26, 30, 35) :
                            LCD_RGBPACK(38, 42, 45);

    /* Stock Classic silhouette: cable at left and two right-facing pins. */
    display->set_foreground(color);
    display->fillrect(138, 110, 17, 7);
    ipodjs_ui_charge_rounded_gradient(display, 151, 99, 27, 28, 5,
                                      top, color, color, 151, 27);
    display->set_foreground(color);
    display->fillrect(176, 102, 10, 6);
    display->fillrect(176, 118, 10, 6);
}

static void ipodjs_ui_draw_charge_background(struct screen *display,
                                             int x, int y, int width,
                                             int height, bool dark)
{
    unsigned base_top = dark ? LCD_RGBPACK(66, 75, 89) :
                               LCD_RGBPACK(132, 149, 162);
    unsigned base_middle = dark ? LCD_RGBPACK(39, 46, 57) :
                                  LCD_RGBPACK(101, 113, 125);
    unsigned base_bottom = dark ? LCD_RGBPACK(17, 22, 30) :
                                  LCD_RGBPACK(70, 79, 86);
    int upper = MAX(1, (display->lcdheight * 45) / 100);
    int lower = MAX(1, display->lcdheight - upper);
    int end_y = y + height;
    int end_x = x + width;
    int band_x;

    for (band_x = x; band_x < end_x; band_x += 4)
    {
        int band_width = MIN(4, end_x - band_x);
        int center_x = band_x + band_width / 2;
        int distance = abs(center_x - display->lcdwidth / 2);
        int focus = MAX(0, 160 - distance);
        int focus2 = (focus * focus) / 160;
        int top_glow = dark ? (focus2 * 11) / 160 :
                              (focus2 * 25) / 160;
        int middle_glow = dark ? (focus2 * 7) / 160 :
                                 (focus2 * 14) / 160;
        int bottom_glow = dark ? (focus2 * 3) / 160 :
                                 (focus2 * 7) / 160;
        unsigned top = LCD_RGBPACK(
            MIN(255, RGB_UNPACK_RED(base_top) + top_glow),
            MIN(255, RGB_UNPACK_GREEN(base_top) + top_glow),
            MIN(255, RGB_UNPACK_BLUE(base_top) + top_glow));
        unsigned middle = LCD_RGBPACK(
            MIN(255, RGB_UNPACK_RED(base_middle) + middle_glow),
            MIN(255, RGB_UNPACK_GREEN(base_middle) + middle_glow),
            MIN(255, RGB_UNPACK_BLUE(base_middle) + middle_glow));
        unsigned bottom = LCD_RGBPACK(
            MIN(255, RGB_UNPACK_RED(base_bottom) + bottom_glow),
            MIN(255, RGB_UNPACK_GREEN(base_bottom) + bottom_glow),
            MIN(255, RGB_UNPACK_BLUE(base_bottom) + bottom_glow));

        if (y < upper)
        {
            int part_end = MIN(end_y, upper);

            display->gradient_fillrect_part(band_x, y, band_width,
                                            part_end - y, top, middle,
                                            upper, y);
        }
        if (end_y > upper)
        {
            int part_y = MAX(y, upper);

            display->gradient_fillrect_part(band_x, part_y, band_width,
                                            end_y - part_y, middle, bottom,
                                            lower, part_y - upper);
        }
    }
}

static unsigned ipodjs_ui_charge_background_color(int x, int y, int width,
                                                  int height,
                                                  bool dark)
{
    unsigned base_top = dark ? LCD_RGBPACK(66, 75, 89) :
                               LCD_RGBPACK(132, 149, 162);
    unsigned base_middle = dark ? LCD_RGBPACK(39, 46, 57) :
                                  LCD_RGBPACK(101, 113, 125);
    unsigned base_bottom = dark ? LCD_RGBPACK(17, 22, 30) :
                                  LCD_RGBPACK(70, 79, 86);
    int distance = abs(x - width / 2);
    int focus = MAX(0, width / 2 - distance);
    int focus2 = width > 0 ? (focus * focus) / MAX(1, width / 2) : 0;
    int top_glow = dark ? (focus2 * 11) / MAX(1, width / 2) :
                          (focus2 * 25) / MAX(1, width / 2);
    int middle_glow = dark ? (focus2 * 7) / MAX(1, width / 2) :
                             (focus2 * 14) / MAX(1, width / 2);
    int bottom_glow = dark ? (focus2 * 3) / MAX(1, width / 2) :
                             (focus2 * 7) / MAX(1, width / 2);
    unsigned top = LCD_RGBPACK(
        MIN(255, RGB_UNPACK_RED(base_top) + top_glow),
        MIN(255, RGB_UNPACK_GREEN(base_top) + top_glow),
        MIN(255, RGB_UNPACK_BLUE(base_top) + top_glow));
    unsigned middle = LCD_RGBPACK(
        MIN(255, RGB_UNPACK_RED(base_middle) + middle_glow),
        MIN(255, RGB_UNPACK_GREEN(base_middle) + middle_glow),
        MIN(255, RGB_UNPACK_BLUE(base_middle) + middle_glow));
    unsigned bottom = LCD_RGBPACK(
        MIN(255, RGB_UNPACK_RED(base_bottom) + bottom_glow),
        MIN(255, RGB_UNPACK_GREEN(base_bottom) + bottom_glow),
        MIN(255, RGB_UNPACK_BLUE(base_bottom) + bottom_glow));
    int split = MAX(1, (height * 45) / 100);

    if (y < split)
        return ipodjs_ui_charge_mix(top, middle, y, MAX(1, split - 1));
    return ipodjs_ui_charge_mix(middle, bottom, y - split,
                                MAX(1, height - split - 1));
}

static void ipodjs_ui_draw_charge_reflection(struct screen *display,
                                             int fill_width, bool dark)
{
    const unsigned shell = dark ? LCD_RGBPACK(91, 98, 108) :
                                  LCD_RGBPACK(86, 92, 98);
    const unsigned graphite = dark ? LCD_RGBPACK(57, 63, 72) :
                                     LCD_RGBPACK(62, 67, 71);
    const unsigned green = dark ? LCD_RGBPACK(66, 151, 49) :
                                  LCD_RGBPACK(69, 158, 47);
    const int reflection_y = IPODJS_CHARGE_BODY_Y + IPODJS_CHARGE_BODY_H + 1;
    const int reflection_h = 43;
    int row;

    for (row = 0; row < reflection_h; row++)
    {
        int source_row = IPODJS_CHARGE_BODY_H - 1 - row;
        int inset = ipodjs_ui_charge_round_inset(source_row,
                         IPODJS_CHARGE_BODY_H, 8);
        int left = IPODJS_CHARGE_BODY_X + inset;
        int right = IPODJS_CHARGE_BODY_X + IPODJS_CHARGE_BODY_W - 1 - inset;
        int y = reflection_y + row;
        int alpha = (86 * (reflection_h - row)) / reflection_h;
        unsigned background = ipodjs_ui_charge_background_color(
                                  display->lcdwidth / 2, y,
                                  display->lcdwidth,
                                  display->lcdheight, dark);
        unsigned body = row < 4 ? shell : graphite;

        body = ipodjs_ui_charge_mix(background, body, alpha, 255);
        display->set_foreground(body);
        display->hline(left, right, y);

        if (fill_width > 0)
        {
            int fill_left = MAX(left, IPODJS_CHARGE_WELL_X);
            int fill_right = MIN(right, IPODJS_CHARGE_WELL_X +
                                 fill_width - 1);

            if (fill_left <= fill_right)
            {
                unsigned reflected_green = ipodjs_ui_charge_mix(
                    background, green, alpha, 255);
                display->set_foreground(reflected_green);
                display->hline(fill_left, fill_right, y);
            }
        }
    }
}

static void ipodjs_ui_draw_charge_frame(struct screen *display, bool full,
                                        int fill_width, bool first)
{
    bool dark = global_settings.ui_engine_dark_mode;
    unsigned title = LCD_RGBPACK(244, 246, 248);
    unsigned outer_top = dark ? LCD_RGBPACK(238, 241, 244) :
                                LCD_RGBPACK(248, 249, 250);
    unsigned outer_mid = dark ? LCD_RGBPACK(132, 139, 148) :
                                LCD_RGBPACK(160, 165, 171);
    unsigned outer_bottom = dark ? LCD_RGBPACK(39, 44, 51) :
                                   LCD_RGBPACK(39, 43, 48);
    unsigned clear_top = dark ? LCD_RGBPACK(166, 172, 181) :
                                LCD_RGBPACK(181, 181, 186);
    unsigned clear_mid = dark ? LCD_RGBPACK(66, 72, 81) :
                                LCD_RGBPACK(69, 73, 77);
    unsigned clear_bottom = dark ? LCD_RGBPACK(73, 79, 87) :
                                   LCD_RGBPACK(93, 95, 97);
    unsigned green_top = dark ? LCD_RGBPACK(174, 239, 146) :
                                LCD_RGBPACK(181, 242, 158);
    unsigned green_mid = dark ? LCD_RGBPACK(62, 160, 47) :
                                LCD_RGBPACK(62, 155, 45);
    unsigned green_bottom = dark ? LCD_RGBPACK(60, 105, 57) :
                                   LCD_RGBPACK(74, 112, 66);
    unsigned shadow = dark ? LCD_RGBPACK(10, 12, 16) :
                             LCD_RGBPACK(73, 79, 87);
    unsigned rim = dark ? LCD_RGBPACK(208, 214, 221) :
                          LCD_RGBPACK(234, 236, 239);
    struct viewport *last_vp;
    int font;

    fill_width = MAX(0, MIN(fill_width, IPODJS_CHARGE_WELL_W));
    if (full)
        fill_width = IPODJS_CHARGE_WELL_W;

    last_vp = display->set_viewport(NULL);
    display->set_drawmode(DRMODE_SOLID);
    display->set_background(dark ? LCD_RGBPACK(52, 59, 71) :
                                   LCD_RGBPACK(199, 204, 211));
    if (first)
    {
        ipodjs_ui_draw_charge_background(display, 0, 0,
                                         display->lcdwidth,
                                         display->lcdheight, dark);
    }

    ipodjs_ui_glass_gradient(display, 0, 0, display->lcdwidth, 23,
                             LCD_RGBPACK(68, 68, 68),
                             LCD_RGBPACK(31, 35, 38),
                             LCD_RGBPACK(9, 16, 21));
    display->set_foreground(LCD_RGBPACK(105, 116, 125));
    display->hline(0, display->lcdwidth - 1, 22);

    font = ipodjs_ui_charge_font();
    display->setfont(font);
    display->set_foreground(title);
    ipodjs_ui_puts_fit(display, 10, 3, display->lcdwidth - 20,
                       full ? "Charged" : "Charging", true);

    ipodjs_ui_draw_charge_background(display, IPODJS_CHARGE_DAMAGE_X,
                                     IPODJS_CHARGE_DAMAGE_Y,
                                     IPODJS_CHARGE_DAMAGE_W,
                                     IPODJS_CHARGE_DAMAGE_H, dark);

    ipodjs_ui_draw_charge_reflection(display, fill_width, dark);

    /* Recessed left cap and the small cylindrical positive terminal. */
    ipodjs_ui_charge_rounded_gradient(display, 86, 82, 11, 62, 5,
                                      outer_top, outer_mid, shadow, 86, 11);
    ipodjs_ui_charge_rounded_gradient(display, 231, 81, 9, 66, 4,
                                      outer_top, outer_mid, outer_bottom,
                                      231, 9);
    ipodjs_ui_charge_rounded_gradient(display, 238, 94, 8, 40, 4,
                                      rim, outer_mid, outer_bottom, 238, 8);

    display->set_foreground(shadow);
    display->hline(94, 238, 152);
    ipodjs_ui_charge_rounded_gradient(display, IPODJS_CHARGE_BODY_X,
                                      IPODJS_CHARGE_BODY_Y,
                                      IPODJS_CHARGE_BODY_W,
                                      IPODJS_CHARGE_BODY_H, 8,
                                      outer_top, outer_mid, outer_bottom,
                                      IPODJS_CHARGE_BODY_X,
                                      IPODJS_CHARGE_BODY_W);
    ipodjs_ui_charge_rounded_gradient(display, IPODJS_CHARGE_WELL_X,
                                      IPODJS_CHARGE_WELL_Y,
                                      IPODJS_CHARGE_WELL_W,
                                      IPODJS_CHARGE_WELL_H, 4,
                                      clear_top, clear_mid, clear_bottom,
                                      IPODJS_CHARGE_WELL_X,
                                      IPODJS_CHARGE_WELL_W);

    if (fill_width > 0)
    {
        ipodjs_ui_charge_rounded_gradient(display, IPODJS_CHARGE_WELL_X,
                                          IPODJS_CHARGE_WELL_Y,
                                          IPODJS_CHARGE_WELL_W,
                                          IPODJS_CHARGE_WELL_H, 4,
                                          green_top, green_mid, green_bottom,
                                          IPODJS_CHARGE_WELL_X, fill_width);
        if (fill_width < IPODJS_CHARGE_WELL_W)
        {
            int fade_width = MIN(5, fill_width);
            int fade;

            /* Stock boundary is a short glass blend, not a bright divider. */
            for (fade = 0; fade < fade_width; fade++)
            {
                int amount = fade + 1;
                int total = fade_width + 1;
                int edge_x = IPODJS_CHARGE_WELL_X + fill_width -
                             fade_width + fade;

                ipodjs_ui_charge_rounded_gradient(display,
                    IPODJS_CHARGE_WELL_X, IPODJS_CHARGE_WELL_Y,
                    IPODJS_CHARGE_WELL_W, IPODJS_CHARGE_WELL_H, 4,
                    ipodjs_ui_charge_mix(green_top, clear_top,
                                         amount, total),
                    ipodjs_ui_charge_mix(green_mid, clear_mid,
                                         amount, total),
                    ipodjs_ui_charge_mix(green_bottom, clear_bottom,
                                         amount, total),
                    edge_x, 1);
            }
        }
    }

    display->set_foreground(rim);
    display->hline(97, 231, IPODJS_CHARGE_WELL_Y);
    display->set_foreground(outer_bottom);
    display->hline(97, 231, IPODJS_CHARGE_WELL_Y +
                             IPODJS_CHARGE_WELL_H - 1);

    if (full)
        ipodjs_ui_draw_charge_plug(display, dark);
    else
        ipodjs_ui_draw_charge_bolt(display, dark);

    if (first)
        display->update();
    else
    {
        display->update_rect(0, 0, display->lcdwidth, 23);
        display->update_rect(IPODJS_CHARGE_DAMAGE_X,
                             IPODJS_CHARGE_DAMAGE_Y,
                             IPODJS_CHARGE_DAMAGE_W,
                             IPODJS_CHARGE_DAMAGE_H);
    }
    display->set_viewport(last_vp);
}

static bool ipodjs_ui_charging_dismiss_action(int action)
{
    if (action == ACTION_NONE || IS_SYSEVENT(action) || button_hold())
        return false;

    return action != ACTION_STD_PREV &&
           action != ACTION_STD_PREVREPEAT &&
           action != ACTION_STD_NEXT &&
           action != ACTION_STD_NEXTREPEAT;
}

static bool ipodjs_ui_charging_handle_event(int action)
{
    if (!IS_SYSEVENT(action))
        return false;

    if (action != SYS_CHARGER_CONNECTED)
        default_event_handler(action);
    return action == SYS_USB_CONNECTED ||
           action == SYS_CHARGER_DISCONNECTED;
}

void ipodjs_ui_charging_screen(bool classify_usb)
{
    struct screen *display = &screens[SCREEN_MAIN];
    long classify_until = current_tick + (HZ * 3) / 4;
    long animation_start;
    long next_sample = current_tick;
    int full_samples = 0;
    int last_fill = -1;
    bool last_full = false;
    bool first = true;

    if (ipodjs_ui_charging_active || ipodjs_ui_charging_seen ||
        !ipodjs_ui_enabled(SCREEN_MAIN))
        return;

    ipodjs_ui_charging_active = true;
    ipodjs_ui_charging_seen = true;
    backlight_on();

    while (classify_usb && charger_inserted() &&
           TIME_BEFORE(current_tick, classify_until))
    {
        int action = get_action(CONTEXT_STD | ALLOW_SOFTLOCK, HZ / 10);

        if (ipodjs_ui_charging_handle_event(action) ||
            ipodjs_ui_charging_dismiss_action(action))
            goto charging_done;
    }

    animation_start = current_tick;

    while (charger_inserted())
    {
        long now = current_tick;
        int fill_width;
        bool full;
        int action;

        if (!TIME_BEFORE(now, next_sample))
        {
            int level = battery_level();

            if (!charging_state() && level >= 99)
                full_samples++;
            else
                full_samples = 0;
            next_sample = now + HZ;
        }

        full = full_samples >= 2;
        if (full)
            fill_width = IPODJS_CHARGE_WELL_W;
        else
        {
            const long fill_ticks = HZ * 4;
            const long cycle_ticks = (HZ * 19) / 4;
            long elapsed = (now - animation_start) % MAX(1, cycle_ticks);

            /* A continuous stock-style sweep, followed by a short full hold. */
            fill_width = elapsed >= fill_ticks ? IPODJS_CHARGE_WELL_W :
                (int)((elapsed * IPODJS_CHARGE_WELL_W) /
                      MAX(1, fill_ticks));
        }
        if (display->is_backlight_on(false) &&
            (first || fill_width != last_fill || full != last_full))
        {
            ipodjs_ui_draw_charge_frame(display, full, fill_width, first);
            first = false;
            last_fill = fill_width;
            last_full = full;
        }

        action = get_action(CONTEXT_STD | ALLOW_SOFTLOCK,
                            MAX(1, HZ / IPODJS_CHARGE_FPS));
        if (ipodjs_ui_charging_handle_event(action) ||
            ipodjs_ui_charging_dismiss_action(action))
            break;
    }

charging_done:
    ipodjs_ui_charging_active = false;
    send_event(GUI_EVENT_ACTIONUPDATE, (void *)1);
}

void ipodjs_ui_charging_disconnected(void)
{
    ipodjs_ui_charging_seen = false;
}

bool ipodjs_ui_handle_system_event(int action, bool *redraw)
{
    unsigned int power;
    bool usb_power;
    bool power_only;

    if (!IS_SYSEVENT(action))
        return false;

    default_event_handler(action);
    if (action == SYS_CHARGER_CONNECTED && charger_inserted())
    {
        power = power_input_status();
        usb_power = (power & POWER_INPUT_USB) != 0;
        power_only = !usb_power;
#ifdef HAVE_USB_POWER
        if (usb_power)
            power_only = usb_powered_only();
#endif
        if (power_only)
            ipodjs_ui_charging_screen(usb_power);
    }
    if (redraw)
        *redraw = true;
    return true;
}

void ipodjs_ui_label_cache_reset(void)
{
    memset(ipodjs_ui_label_cache, 0, sizeof(ipodjs_ui_label_cache));
    ipodjs_ui_label_cache_stamp = 0;
}

#endif
