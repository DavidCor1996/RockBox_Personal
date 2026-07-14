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

#ifndef IPODJS_UI_H
#define IPODJS_UI_H

#include <stdbool.h>
#include "screen_access.h"

#define IPODJS_UI_HEADER_HEIGHT 20

#ifdef HAVE_IPODJS_UI
bool ipodjs_ui_enabled(enum screen_type screen);
int ipodjs_ui_row_height(void);
int ipodjs_ui_font(void);
int ipodjs_ui_text_y_offset(void);

unsigned ipodjs_ui_accent(void);
unsigned ipodjs_ui_screen_bg(void);
unsigned ipodjs_ui_row_bg(void);
unsigned ipodjs_ui_text(void);
unsigned ipodjs_ui_muted_text(void);
unsigned ipodjs_ui_header_text(void);
unsigned ipodjs_ui_header_bg(void);
unsigned ipodjs_ui_panel(void);

unsigned ipodjs_ui_rgb_blend(int br, int bg, int bb,
                             int fr, int fg, int fb,
                             int alpha);
void ipodjs_ui_gradient(struct screen *display, int x, int y, int w, int h,
                        unsigned top, unsigned bottom);
void ipodjs_ui_glass_gradient(struct screen *display, int x, int y,
                              int w, int h, unsigned top,
                              unsigned mid, unsigned bottom);
void ipodjs_ui_selection_gradient(struct screen *display, int x, int y,
                                  int w, int h, unsigned *midp);
void ipodjs_ui_puts_fit(struct screen *display, int x, int y, int width,
                        const char *text, bool center);
void ipodjs_ui_draw_arrow(struct screen *display, int x, int y,
                          unsigned color);
void ipodjs_ui_prepare_native_frame(void);
void ipodjs_ui_usb_prepare(void);
void ipodjs_ui_draw_usb_connected(struct screen *display);
void ipodjs_ui_charging_screen(bool classify_usb);
void ipodjs_ui_charging_disconnected(void);
bool ipodjs_ui_handle_system_event(int action, bool *redraw);
void ipodjs_ui_label_cache_reset(void);
#else
static inline bool ipodjs_ui_enabled(enum screen_type screen)
{
    (void)screen;
    return false;
}

static inline int ipodjs_ui_row_height(void) { return 0; }
static inline int ipodjs_ui_font(void) { return 0; }
static inline int ipodjs_ui_text_y_offset(void) { return 0; }
static inline unsigned ipodjs_ui_accent(void) { return 0; }
static inline unsigned ipodjs_ui_screen_bg(void) { return 0; }
static inline unsigned ipodjs_ui_row_bg(void) { return 0; }
static inline unsigned ipodjs_ui_text(void) { return 0; }
static inline unsigned ipodjs_ui_muted_text(void) { return 0; }
static inline unsigned ipodjs_ui_header_text(void) { return 0; }
static inline unsigned ipodjs_ui_header_bg(void) { return 0; }
static inline unsigned ipodjs_ui_panel(void) { return 0; }
static inline unsigned ipodjs_ui_rgb_blend(int br, int bg, int bb,
                                            int fr, int fg, int fb,
                                            int alpha)
{
    (void)br; (void)bg; (void)bb; (void)fr;
    (void)fg; (void)fb; (void)alpha;
    return 0;
}
static inline void ipodjs_ui_gradient(struct screen *display, int x, int y,
                                      int w, int h, unsigned top,
                                      unsigned bottom)
{
    (void)display; (void)x; (void)y; (void)w;
    (void)h; (void)top; (void)bottom;
}
static inline void ipodjs_ui_glass_gradient(struct screen *display, int x,
                                            int y, int w, int h,
                                            unsigned top, unsigned mid,
                                            unsigned bottom)
{
    (void)display; (void)x; (void)y; (void)w;
    (void)h; (void)top; (void)mid; (void)bottom;
}
static inline void ipodjs_ui_selection_gradient(struct screen *display, int x,
                                                int y, int w, int h,
                                                unsigned *midp)
{
    (void)display; (void)x; (void)y; (void)w; (void)h; (void)midp;
}
static inline void ipodjs_ui_puts_fit(struct screen *display, int x, int y,
                                     int width, const char *text, bool center)
{
    (void)display; (void)x; (void)y; (void)width; (void)text; (void)center;
}
static inline void ipodjs_ui_draw_arrow(struct screen *display, int x, int y,
                                        unsigned color)
{
    (void)display; (void)x; (void)y; (void)color;
}
static inline void ipodjs_ui_prepare_native_frame(void) { }
static inline void ipodjs_ui_usb_prepare(void) { }
static inline void ipodjs_ui_draw_usb_connected(struct screen *display)
{
    (void)display;
}
static inline void ipodjs_ui_charging_screen(bool classify_usb)
{
    (void)classify_usb;
}
static inline void ipodjs_ui_charging_disconnected(void) { }
static inline bool ipodjs_ui_handle_system_event(int action, bool *redraw)
{
    (void)action; (void)redraw;
    return false;
}
static inline void ipodjs_ui_label_cache_reset(void) { }
#endif

#endif
