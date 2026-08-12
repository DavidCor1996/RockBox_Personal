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

#define IPODJS_UI_HEADER_HEIGHT 24

enum ipodjs_ui_search_surface {
    IPODJS_UI_SEARCH_PANEL = 0,
    IPODJS_UI_SEARCH_FIELD,
    IPODJS_UI_SEARCH_SELECTED,
};

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
void ipodjs_ui_draw_header_background(struct screen *display, int width);
void ipodjs_ui_draw_playback_indicator(struct screen *display, int x, int y);
void ipodjs_ui_draw_hold_indicator(struct screen *display, int x, int y);
void ipodjs_ui_draw_repeat_indicator(struct screen *display, int x, int y,
                                     int repeat_mode);
void ipodjs_ui_draw_shuffle_indicator(struct screen *display, int x, int y);
void ipodjs_ui_draw_header_battery(struct screen *display, int x, int y);
void ipodjs_ui_prepare_bluetooth_indicator(void);
void ipodjs_ui_draw_bluetooth_indicator(struct screen *display, int x, int y);
void ipodjs_ui_draw_wifi_indicator(struct screen *display, int x, int y);
void ipodjs_ui_airpods_connected_animation(void);
bool ipodjs_ui_fast_scroll_available(void);
/* Returns #/A-Z bucket 0..26, or -1 for an unsupported script. */
int ipodjs_ui_fast_scroll_bucket(const char *text);
void ipodjs_ui_fast_scroll_show(const char *label);
void ipodjs_ui_fast_scroll_clear(void);
bool ipodjs_ui_fast_scroll_active(void);
bool ipodjs_ui_fast_scroll_take_expired(void);
void ipodjs_ui_draw_fast_scroll(struct screen *display);
bool ipodjs_ui_search_surfaces_available(void);
bool ipodjs_ui_prepare_search_surfaces(void);
bool ipodjs_ui_draw_search_surface(struct screen *display,
                                   enum ipodjs_ui_search_surface surface,
                                   int x, int y, int width, int height);
void ipodjs_ui_transition_begin(int direction);
void ipodjs_ui_transition_begin_vertical(int direction);
bool ipodjs_ui_transition_present(struct screen *display);
void ipodjs_ui_transition_cancel(void);
/* Returns true only when USB interrupted the launch. */
bool ipodjs_ui_netflix_launch(void);
bool ipodjs_ui_preview_fade_begin(struct screen *display,
                                  int x, int y, int width, int height);
bool ipodjs_ui_preview_fade_present(struct screen *display);
void ipodjs_ui_prepare_native_frame(void);
void ipodjs_ui_shutdown_animation(void);
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
static inline void ipodjs_ui_draw_playback_indicator(struct screen *display,
                                                      int x, int y)
{
    (void)display; (void)x; (void)y;
}
static inline void ipodjs_ui_draw_header_background(struct screen *display,
                                                     int width)
{
    (void)display; (void)width;
}
static inline void ipodjs_ui_draw_hold_indicator(struct screen *display,
                                                  int x, int y)
{
    (void)display; (void)x; (void)y;
}
static inline void ipodjs_ui_draw_repeat_indicator(struct screen *display,
                                                    int x, int y,
                                                    int repeat_mode)
{
    (void)display; (void)x; (void)y; (void)repeat_mode;
}
static inline void ipodjs_ui_draw_shuffle_indicator(struct screen *display,
                                                     int x, int y)
{
    (void)display; (void)x; (void)y;
}
static inline void ipodjs_ui_draw_header_battery(struct screen *display,
                                                 int x, int y)
{
    (void)display; (void)x; (void)y;
}
static inline void ipodjs_ui_prepare_bluetooth_indicator(void) { }
static inline void ipodjs_ui_draw_bluetooth_indicator(
    struct screen *display, int x, int y)
{
    (void)display; (void)x; (void)y;
}
static inline void ipodjs_ui_draw_wifi_indicator(
    struct screen *display, int x, int y)
{
    (void)display; (void)x; (void)y;
}
static inline void ipodjs_ui_airpods_connected_animation(void) { }
static inline void ipodjs_ui_fast_scroll_show(const char *label)
{
    (void)label;
}
static inline bool ipodjs_ui_fast_scroll_available(void) { return false; }
static inline int ipodjs_ui_fast_scroll_bucket(const char *text)
{
    (void)text;
    return -1;
}
static inline void ipodjs_ui_fast_scroll_clear(void) { }
static inline bool ipodjs_ui_fast_scroll_active(void) { return false; }
static inline bool ipodjs_ui_fast_scroll_take_expired(void) { return false; }
static inline void ipodjs_ui_draw_fast_scroll(struct screen *display)
{
    (void)display;
}
static inline bool ipodjs_ui_search_surfaces_available(void)
{
    return false;
}
static inline bool ipodjs_ui_prepare_search_surfaces(void)
{
    return false;
}
static inline bool ipodjs_ui_draw_search_surface(
    struct screen *display, enum ipodjs_ui_search_surface surface,
    int x, int y, int width, int height)
{
    (void)display; (void)surface; (void)x; (void)y;
    (void)width; (void)height;
    return false;
}
static inline void ipodjs_ui_transition_begin(int direction)
{
    (void)direction;
}
static inline void ipodjs_ui_transition_begin_vertical(int direction)
{
    (void)direction;
}
static inline bool ipodjs_ui_transition_present(struct screen *display)
{
    (void)display;
    return false;
}
static inline void ipodjs_ui_transition_cancel(void) { }
static inline bool ipodjs_ui_netflix_launch(void) { return false; }
static inline bool ipodjs_ui_preview_fade_begin(struct screen *display,
                                                int x, int y,
                                                int width, int height)
{
    (void)display; (void)x; (void)y; (void)width; (void)height;
    return false;
}
static inline bool ipodjs_ui_preview_fade_present(struct screen *display)
{
    (void)display;
    return false;
}
static inline void ipodjs_ui_prepare_native_frame(void) { }
static inline void ipodjs_ui_shutdown_animation(void) { }
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
