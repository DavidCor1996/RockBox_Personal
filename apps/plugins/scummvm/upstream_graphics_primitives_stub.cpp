/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_ /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "lib/plugin_cxx_compat.h"
#include "graphics/primitives.h"

namespace Graphics {

void drawLine(int x0, int y0, int x1, int y1, int color,
              void (*plotProc)(int, int, int, void *), void *data)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        plotProc(x0, y0, color, data);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void drawHLine(int x1, int x2, int y, int color,
               void (*plotProc)(int, int, int, void *), void *data)
{
    int x;
    if (x2 < x1) {
        int tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    for (x = x1; x <= x2; x++)
        plotProc(x, y, color, data);
}

void drawVLine(int x, int y1, int y2, int color,
               void (*plotProc)(int, int, int, void *), void *data)
{
    int y;
    if (y2 < y1) {
        int tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    for (y = y1; y <= y2; y++)
        plotProc(x, y, color, data);
}

void drawThickLine(int x0, int y0, int x1, int y1, int penX, int penY,
                   int color, void (*plotProc)(int, int, int, void *),
                   void *data)
{
    (void)penX;
    (void)penY;
    drawLine(x0, y0, x1, y1, color, plotProc, data);
}

void drawThickLine2(int x1, int y1, int x2, int y2, int thick, int color,
                    void (*plotProc)(int, int, int, void *), void *data)
{
    (void)thick;
    drawLine(x1, y1, x2, y2, color, plotProc, data);
}

void drawFilledRect(Common::Rect &rect, int color,
                    void (*plotProc)(int, int, int, void *), void *data)
{
    int y;
    for (y = rect.top; y < rect.bottom; y++)
        drawHLine(rect.left, rect.right - 1, y, color, plotProc, data);
}

void drawRoundRect(Common::Rect &rect, int arc, int color, bool filled,
                   void (*plotProc)(int, int, int, void *), void *data)
{
    (void)arc;
    if (filled)
        drawFilledRect(rect, color, plotProc, data);
    else {
        drawHLine(rect.left, rect.right - 1, rect.top, color, plotProc, data);
        drawHLine(rect.left, rect.right - 1, rect.bottom - 1, color,
                  plotProc, data);
        drawVLine(rect.left, rect.top, rect.bottom - 1, color, plotProc,
                  data);
        drawVLine(rect.right - 1, rect.top, rect.bottom - 1, color,
                  plotProc, data);
    }
}

void drawPolygonScan(int *polyX, int *polyY, int npoints, Common::Rect &bbox,
                     int color, void (*plotProc)(int, int, int, void *),
                     void *data)
{
    (void)polyX;
    (void)polyY;
    (void)npoints;
    drawFilledRect(bbox, color, plotProc, data);
}

void drawEllipse(int x0, int y0, int x1, int y1, int color, bool filled,
                 void (*plotProc)(int, int, int, void *), void *data)
{
    Common::Rect rect(x0, y0, x1, y1);
    if (filled)
        drawFilledRect(rect, color, plotProc, data);
    else {
        drawHLine(x0, x1, y0, color, plotProc, data);
        drawHLine(x0, x1, y1, color, plotProc, data);
        drawVLine(x0, y0, y1, color, plotProc, data);
        drawVLine(x1, y0, y1, color, plotProc, data);
    }
}

} /* namespace Graphics */
