/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Non-repeating pseudo-random slideshow ordering shared by the iPodJS
 * preview panes.  A coprime step through the entry range visits every
 * index exactly once per cycle period without storing a permutation.
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
#ifndef _SLIDESHOW_ORDER_H_
#define _SLIDESHOW_ORDER_H_

#include "kernel.h"

struct slideshow_order {
    int count;
    int base;
    int step;
    unsigned long seed;
};

static inline unsigned long slideshow_order_next_random(
    struct slideshow_order *order)
{
    if (order->seed == 0)
        order->seed = current_tick ? (unsigned long)current_tick :
                      0x9e3779b9ul;

    order->seed ^= order->seed << 13;
    order->seed ^= order->seed >> 17;
    order->seed ^= order->seed << 5;
    return order->seed;
}

static inline int slideshow_order_gcd(int a, int b)
{
    while (b != 0)
    {
        int t = a % b;
        a = b;
        b = t;
    }

    return a < 0 ? -a : a;
}

static inline void slideshow_order_prepare(struct slideshow_order *order,
                                           int entry_count)
{
    if (entry_count <= 0 || order->count == entry_count)
        return;

    order->count = entry_count;
    order->base =
        (int)(slideshow_order_next_random(order) %
              (unsigned long)entry_count);

    if (entry_count == 1)
    {
        order->step = 0;
        return;
    }

    order->step =
        (int)(slideshow_order_next_random(order) %
              (unsigned long)(entry_count - 1)) + 1;
    while (slideshow_order_gcd(order->step, entry_count) != 1)
    {
        order->step++;
        if (order->step >= entry_count)
            order->step = 1;
    }
}

static inline int slideshow_order_index(struct slideshow_order *order,
                                        long cycle, int entry_count)
{
    slideshow_order_prepare(order, entry_count);

    if (entry_count <= 1)
        return 0;

    return (order->base + (int)((cycle % entry_count) * order->step)) %
           entry_count;
}

static inline void slideshow_order_reset(struct slideshow_order *order)
{
    order->count = 0;
    order->base = 0;
    order->step = 0;
}

#endif /* _SLIDESHOW_ORDER_H_ */
