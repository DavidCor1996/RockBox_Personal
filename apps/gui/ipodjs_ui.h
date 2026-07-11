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
void ipodjs_ui_label_cache_reset(void);

#endif
