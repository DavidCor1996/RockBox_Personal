/* SPDX-License-Identifier: GPL-2.0-or-later
 * Pure presentation geometry. Raster samples are not physical square pixels.
 * No target state, decoder state, allocations, or floating point. */
#ifndef VIDEOOUT_GEOMETRY_H
#define VIDEOOUT_GEOMETRY_H
#include <stdbool.h>
#include <stdint.h>
struct videoout_rect { int x, y, w, h; };
struct videoout_geometry {
    struct videoout_rect crop, destination, safe;
};
static inline int videoout_even(int n) { return n > 1 ? n & ~1 : 2; }
static inline int videoout_inset(int mode)
{
    const int percent[] = {0, 3, 5, 8};
    return percent[mode >= 0 && mode < 4 ? mode : 0];
}
/* dar is the source DISPLAY aspect, independently of its stored raster.
 * active is in output-raster coordinates; TV aspect describes the full raster.
 * UI contains within safe; video contains/fills active, never overscan-cropped. */
static inline bool videoout_calc_geometry(int sw, int sh, int dar_n, int dar_d,
    int rw, int rh, struct videoout_rect active, bool wide, bool fill,
    int overscan, bool ui, struct videoout_geometry *g)
{
    if (!g || sw < 2 || sh < 2 || sw > 16384 || sh > 16384 ||
        dar_n < 1 || dar_d < 1 || dar_n > 65536 || dar_d > 65536 ||
        rw < 2 || rh < 2 || rw > 16384 || rh > 16384 || active.x < 0 ||
        active.y < 0 || active.w < 2 || active.h < 2 ||
        active.x > rw - active.w || active.y > rh - active.h)
        return false;
    int inset = videoout_inset(overscan);
    int ix = (active.w * inset / 100) & ~1;
    int iy = (active.h * inset / 100) & ~1;
    g->safe = (struct videoout_rect){active.x + ix, active.y + iy,
                                   (active.w - 2*ix) & ~1,
                                   (active.h - 2*iy) & ~1};
    struct videoout_rect box = ui ? g->safe : active;
    int tvn = wide ? 16 : 4, tvd = wide ? 9 : 3;
    /* Desired width/height in OUTPUT samples, including sample aspect. */
    int64_t n = (int64_t)dar_n * tvd * rw;
    int64_t d = (int64_t)dar_d * tvn * rh;
    g->crop = (struct videoout_rect){0, 0, sw & ~1, sh & ~1};
    g->destination = box;
    if (fill && !ui)
    {
        if (n * box.h > d * box.w)
            g->crop.w = videoout_even((int)((int64_t)sw * box.w * d /
                                           (box.h * n)));
        else
            g->crop.h = videoout_even((int)((int64_t)sh * box.h * n /
                                           (box.w * d)));
        g->crop.x = ((sw - g->crop.w) / 2) & ~1;
        g->crop.y = ((sh - g->crop.h) / 2) & ~1;
    }
    else
    {
        if (n * box.h > d * box.w)
            g->destination.h = videoout_even((int)(box.w * d / n));
        else
            g->destination.w = videoout_even((int)(box.h * n / d));
        g->destination.x += ((box.w - g->destination.w) / 2) & ~1;
        g->destination.y += ((box.h - g->destination.h) / 2) & ~1;
    }
    return true;
}
#endif
