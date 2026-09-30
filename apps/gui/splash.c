/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) Daniel Stenberg (2002)
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
#include "stdarg.h"
#include "string.h"
#include "rbunicode.h"
#include "stdio.h"
#include "kernel.h"
#include "screen_access.h"
#include "lang.h"
#include "settings.h"
#include "talk.h"
#include "splash.h"
#include "tv_ui.h"
#include "viewport.h"
#include "strptokspn_r.h"
#include "scrollbar.h"
#include "font.h"
#include "ipodjs_ui.h"
#ifdef HAVE_IPODJS_UI
#include "ipodjs_settings.h"
#endif
#ifndef BOOTLOADER
#include "misc.h" /* get_current_activity */
#endif

static long progress_next_tick, talked_tick;

#define MAXLINES  (LCD_HEIGHT/6)
#define MAXBUFFER 512
#define RECT_SPACING 3
#define SPLASH_MEMORY_INTERVAL (HZ)

#ifdef HAVE_LCD_COLOR
static void splash_fill_roundish(struct screen *screen,
                                 int x, int y, int w, int h,
                                 unsigned color)
{
    if (w <= 0 || h <= 0)
        return;

    screen->set_foreground(color);

    if (w < 5 || h < 5)
    {
        screen->fillrect(x, y, w, h);
        return;
    }

    screen->fillrect(x + 2, y, w - 4, h);
    screen->fillrect(x + 1, y + 1, w - 2, h - 2);
    screen->fillrect(x, y + 2, w, h - 4);
}

static bool splash_draw_modern_panel(struct screen *screen,
                                     const struct viewport *vp)
{
    if (screen->depth <= 1)
        return false;
#ifdef HAVE_IPODJS_UI
    if (ipodjs_settings_draw_notice(screen, vp->x, vp->y, vp->width, vp->height))
        return true;
#endif

    const bool ipodjs = ipodjs_ui_enabled(screen->screen_type);
    const unsigned edge = ipodjs ? ipodjs_ui_header_bg() :
                                   (unsigned)global_settings.lss_color;
    const unsigned panel = ipodjs ? ipodjs_ui_panel() :
                                    (unsigned)global_settings.bg_color;
    const unsigned top = ipodjs ? ipodjs_ui_header_bg() :
                                  (unsigned)global_settings.lss_color;
    const unsigned accent = ipodjs ? ipodjs_ui_accent() :
                                    (unsigned)global_settings.lse_color;
    int accent_w = vp->width - 12;
    if (accent_w > 54)
        accent_w = 54;

    splash_fill_roundish(screen, 0, 0, vp->width, vp->height, edge);
    splash_fill_roundish(screen, 1, 1, vp->width - 2, vp->height - 2, panel);

    if (vp->width > 12 && vp->height > 8)
    {
        screen->set_foreground(top);
        screen->fillrect(5, 2, vp->width - 10, 1);
    }

    if (accent_w > 0 && vp->height > 5)
    {
        screen->set_foreground(accent);
        screen->fillrect((vp->width - accent_w) / 2, vp->height - 3,
                         accent_w, 1);
    }

    return true;
}
#endif

static bool splash_internal(struct screen * screen, const char *fmt, va_list ap,
                            struct viewport *vp, int addl_lines)
{
    static int max_width[NB_SCREENS] = {2*RECT_SPACING};
    struct viewport bounds = *vp;
#ifndef BOOTLOADER
    static enum current_activity last_act = ACTIVITY_UNKNOWN;
    enum current_activity act = get_current_activity();

    if (last_act != act) /* changed activities reset max_width */
    {
        FOR_NB_SCREENS(i)
            max_width[i] = 2*RECT_SPACING;
        last_act = act;
    }
#endif
    /* prevent screen artifacts by keeping the max width seen */
    int min_width = max_width[screen->screen_type];
    char splash_buf[MAXBUFFER];
    struct splash_lines {
        const char *str;
        size_t len;
    } lines[MAXLINES];
    const char *next;
    const char *lastbreak = NULL;
    const char *store = NULL;
    int line = 0;
    int x = 0;
    int y, i;
    int space_w, w, chr_h;
    int width, height;
    int maxw = min_width - 2*RECT_SPACING;
    int fontnum = vp->font;
    bool modern_panel = false;
    bool retail_notice = false;

    char lastbrkchr;
    size_t len, next_len;
    const char matchstr[] = "\r\n\f\v\t ";
    viewport_set_centered_preset(&bounds, VIEWPORT_OVERLAY_PRESET_SMALL);
#ifdef HAVE_IPODJS_UI
    if (ipodjs_settings_dialog_available(screen))
    {
        fontnum = ipodjs_ui_retailos_detail_font();
        bounds.font = vp->font = fontnum;
    }
#endif
    font_getstringsize(" ", &space_w, &chr_h, fontnum);
    y = chr_h + (addl_lines * chr_h);

    vsnprintf(splash_buf, sizeof(splash_buf), fmt, ap);
    va_end(ap);

    /* break splash string into display lines, doing proper word wrap */
    next = strptokspn_r(splash_buf, matchstr, &next_len, &store);
    if (!next)
        return false; /* nothing to display */

    lines[line].len = next_len;
    lines[line].str = next;
    while (true)
    {
        w = font_getstringnsize(next, next_len, NULL, NULL, fontnum);
        if (lastbreak)
        {
            len = next - lastbreak;
            int next_w = len * space_w;
            if (x + next_w + w > bounds.width - RECT_SPACING*2 || lastbrkchr != ' ')
            {   /* too wide, or control character wrap */
                if (x > maxw)
                    maxw = x;
                if ((y + chr_h * 2 > bounds.height) || (line >= (MAXLINES-1)))
                    break;  /* screen full or out of lines */
                x = 0;
                y += chr_h;

                /* split when it fits since we didn't find a valid token to break on */
                size_t nl = next_len;
                while (w > bounds.width && --nl > 0)
                    w = font_getstringnsize(next, nl, NULL, NULL, fontnum);

                if (nl > 1 && nl != next_len)
                {
                    next_len = nl;
                    store = next + nl; /* move the start pos for the next token read */
                }

                lines[++line].len = next_len;
                lines[line].str = next;
            }
            else
            {
                /*  restore & calculate spacing */
                lines[line].len += next_len + 1;
                x += next_w;
            }
        }
        x += w;

        lastbreak = next + next_len;
        lastbrkchr = *lastbreak;

        next = strptokspn_r(NULL, matchstr, &next_len, &store);

        if (!next)
        {   /* no more words */
            if (x > maxw)
                maxw = x;
            break;
        }
    }

    /* prepare viewport
     * First boundaries, then the background filling, then the border and finally
     * the text*/

    screen->scroll_stop();

    width = maxw + 2*RECT_SPACING;
    height = y + 2*RECT_SPACING;
#ifdef HAVE_IPODJS_UI
    if (ipodjs_settings_dialog_available(screen) && height <= 80)
    {
        height = 80;
        width = MAX(width, 180);
        retail_notice = true;
    }
#endif

    *vp = bounds;
    viewport_set_centered(vp, width, height);

    /* prevent artifacts by locking to max width observed on repeated calls */
    max_width[screen->screen_type] = width;

    vp->flags |=  VP_FLAG_ALIGN_CENTER;
#if LCD_DEPTH > 1
    unsigned fg = 0, bg = 0;
    bool broken = false;

    if (screen->depth > 1)
    {
        fg = screen->get_foreground();
        bg = screen->get_background();

#ifdef HAVE_LCD_COLOR
        if (ipodjs_ui_enabled(screen->screen_type))
            fg = ipodjs_ui_text();
#ifdef HAVE_IPODJS_UI
        if (retail_notice)
            fg = LCD_RGBPACK(255,255,255);
#endif
#endif

        broken = (fg == bg) ||
                 (bg == 63422 && fg == 65535); /* -> iPod reFresh themes from '22 */

        vp->drawmode = DRMODE_FG;
#ifdef HAVE_LCD_COLOR
        modern_panel = splash_draw_modern_panel(screen, vp);
#endif
        if (!modern_panel)
        {
            /* can't do vp->fg_pattern here, since set_foreground does a bit more on
             * greyscale */
            screen->set_foreground(broken ? SCREEN_COLOR_TO_NATIVE(screen, LCD_LIGHTGRAY) :
                                   bg);     /* gray as fallback for broken themes */
            screen->fill_viewport();
        }
    }
    else
#endif
    {
        vp->drawmode = (DRMODE_SOLID|DRMODE_INVERSEVID);
        screen->fill_viewport();
    }

#if LCD_DEPTH > 1
    if (screen->depth > 1)
        /* can't do vp->fg_pattern here, since set_foreground does a bit more on
         * greyscale */
        screen->set_foreground(broken ? SCREEN_COLOR_TO_NATIVE(screen, LCD_BLACK) :
                               fg);     /* black as fallback for broken themes */
    else
#endif
        vp->drawmode = DRMODE_SOLID;

    if (!modern_panel)
        screen->draw_border_viewport();

    /* print the message to screen */
    for(i = 0, y = retail_notice ? (height - (line + 1) * chr_h) / 2 : RECT_SPACING;
        i <= line; i++, y+= chr_h)
    {
        screen->putsxyf(0, y, "%.*s", lines[i].len, lines[i].str);
    }
    return true; /* needs update */
}

void splashf(int ticks, const char *fmt, ...)
{
    /* Startup errors and explicit notices must remain visible. */
    lcd_boot_frame_hold(false);
    va_list ap;

    /* fmt may be a so called virtual pointer. See settings.h. */
    long id;
    if((id = P2ID((const unsigned char*)fmt)) >= 0)
        /* If fmt specifies a voicefont ID, and voice menus are
           enabled, then speak it. */
        cond_talk_ids_fq(id);

    /* If fmt is a lang ID then get the corresponding string (which
       still might contain % place holders). */
    fmt = P2STR((unsigned char *)fmt);
    FOR_NB_SCREENS(i)
    {
        struct screen * screen = &(screens[i]);
        struct viewport vp;
        viewport_set_defaults(&vp, screen->screen_type);
        struct viewport *last_vp = screen->set_viewport(&vp);

        va_start(ap, fmt);
        if (splash_internal(screen, fmt, ap, &vp, 0))
            screen->update_viewport();
        va_end(ap);

        screen->set_viewport(last_vp);
    }
#ifdef HAVE_COMPOSITE_VIDEO_OUT
    char tv_message[256];
    va_start(ap, fmt);
    vsnprintf(tv_message, sizeof(tv_message), fmt, ap);
    va_end(ap);
    const char *tv_lines[] = {tv_message};
    tv_dialog_draw("Rockbox", tv_lines, 1, "");
#endif
    if (ticks)
        sleep(ticks);
}

#ifdef HAVE_LCD_COLOR
static int splash_progress_fill_width(int current, int total, int width)
{
    if (total <= 0 || width <= 0)
        return 0;

    if (current < 0)
        current = 0;
    else if (current > total)
        current = total;

    int fill_width = current * width / total;

    if (current > 0 && fill_width == 0)
        fill_width = 1;

    return fill_width;
}

static void splash_progress_fill_roundish(struct screen *screen,
                                          int x, int y, int w, int h,
                                          unsigned color)
{
    if (w <= 0 || h <= 0)
        return;

    screen->set_foreground(color);

    if (w < 5 || h < 5)
    {
        screen->fillrect(x, y, w, h);
        return;
    }

    screen->fillrect(x + 2, y, w - 4, h);
    screen->fillrect(x + 1, y + 1, w - 2, h - 2);
    screen->fillrect(x, y + 2, w, h - 4);
}

static void splash_progress_draw_pill(struct screen *screen,
                                      int x, int y, int w, int h,
                                      int current, int total)
{
    const unsigned old_fg = screen->get_foreground();
    const unsigned track_edge = global_settings.lss_color;
    const unsigned track = global_settings.bg_color;
    const unsigned track_top = global_settings.lss_color;
    const unsigned fill = global_settings.lse_color;
    const unsigned fill_top = global_settings.lst_color;
    int inner_x = x + 1;
    int inner_y = y + 1;
    int inner_w = w - 2;
    int inner_h = h - 2;
    int fill_w = splash_progress_fill_width(current, total, inner_w);

    splash_progress_fill_roundish(screen, x, y, w, h, track_edge);
    splash_progress_fill_roundish(screen, inner_x, inner_y, inner_w, inner_h,
                                  track);

    if (inner_w > 4 && inner_h > 4)
    {
        screen->set_foreground(track_top);
        screen->fillrect(inner_x + 2, inner_y + 1, inner_w - 4, 1);
    }

    if (fill_w > 0)
    {
        splash_progress_fill_roundish(screen, inner_x, inner_y, fill_w,
                                      inner_h, fill);

        if (fill_w > 4 && inner_h > 4)
        {
            screen->set_foreground(fill_top);
            screen->fillrect(inner_x + 2, inner_y + 1, fill_w - 4, 1);
        }
    }

    screen->set_foreground(old_fg);
}
#endif

/* set delay before progress meter is shown */
void splash_progress_set_delay(long delay_ticks)
{
    progress_next_tick = current_tick + delay_ticks;
    talked_tick = 0;
}

/* splash a progress meter */
void splash_progress(int current, int total, const char *fmt, ...)
{
    va_list ap;
    int vp_flag = VP_FLAG_VP_DIRTY;
    /* progress update tick */
    long now = current_tick;

    if (current < total)
    {
        if(TIME_BEFORE(now, progress_next_tick))
            return;
        /* limit to 20fps */
        progress_next_tick = now + HZ/20;
        vp_flag = 0; /* don't mark vp dirty to prevent flashing */
    }

    if (global_settings.talk_menu &&
        total > 0 &&
        TIME_AFTER(current_tick, talked_tick + HZ*5))
    {
        talked_tick = current_tick;
        talk_ids(false, LANG_LOADING_PERCENT,
                 TALK_ID(current * 100 / total, UNIT_PERCENT));
    }

    /* If fmt is a lang ID then get the corresponding string (which
       still might contain % place holders). */
    fmt = P2STR((unsigned char *)fmt);
    FOR_NB_SCREENS(i)
    {
        struct screen * screen = &(screens[i]);
        struct viewport vp;
        viewport_set_defaults(&vp, screen->screen_type);
        struct viewport *last_vp = screen->set_viewport_ex(&vp, vp_flag);

        va_start(ap, fmt);
        if (splash_internal(screen, fmt, ap, &vp, 1))
        {
            int size = screen->getcharheight();
            int h = size > 12 ? 8 : size - 2;
            int x = RECT_SPACING * 2;
            int y;
            int w = vp.width - RECT_SPACING * 4;

            if (h < 5)
                h = 5;

            y = vp.height - h - RECT_SPACING - 1;
#ifdef HAVE_LCD_COLOR
            if (screen->depth > 1)
                splash_progress_draw_pill(screen, x, y, w, h, current, total);
            else
#endif
            {
#ifdef HAVE_LCD_COLOR
                const int sb_flags = HORIZONTAL | FOREGROUND;
#else
                const int sb_flags = HORIZONTAL;
#endif
                gui_scrollbar_draw(screen, x, y, w, h, total, 0, current,
                                   sb_flags);
            }

            screen->update_viewport();
        }
        va_end(ap);

        screen->set_viewport(last_vp);
    }
}
