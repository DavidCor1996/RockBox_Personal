/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id$
*
* Copyright (C) 2009 by Michael Sevakis
*
* Miscellaneous compiler support routines.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied
*
****************************************************************************/
#include "plugin.h"

#if defined(ARM_NEED_DIV0)
void __attribute__((naked)) __div0(void)
{
    asm volatile("bx %0" : : "r"(rb->__div0));
}

#if defined(CPU_ARM_MICRO)
void __aeabi_idiv0(void) __attribute__((alias("__div0")));
void __aeabi_ldiv0(void) __attribute__((alias("__div0")));
#endif
#endif

void *memcpy(void *dest, const void *src, size_t n)
{
#ifdef SIMULATOR
    if (!rb) {
        unsigned char *d = dest;
        const unsigned char *s = src;
        size_t i;
        for (i = 0; i < n; i++)
            d[i] = s[i];
        return dest;
    }
#endif
    return rb->memcpy(dest, src, n);
}

void *memset(void *dest, int c, size_t n)
{
#ifdef SIMULATOR
    if (!rb) {
        unsigned char *d = dest;
        size_t i;
        for (i = 0; i < n; i++)
            d[i] = (unsigned char)c;
        return dest;
    }
#endif
    return rb->memset(dest, c, n);
}

void *memmove(void *dest, const void *src, size_t n)
{
#ifdef SIMULATOR
    if (!rb) {
        unsigned char *d = dest;
        const unsigned char *s = src;
        size_t i;
        if (d < s) {
            for (i = 0; i < n; i++)
                d[i] = s[i];
        } else {
            while (n-- > 0)
                d[n] = s[n];
        }
        return dest;
    }
#endif
    return rb->memmove(dest, src, n);
}

int memcmp(const void *s1, const void *s2, size_t n)
{
#ifdef SIMULATOR
    if (!rb) {
        const unsigned char *a = s1;
        const unsigned char *b = s2;
        size_t i;
        for (i = 0; i < n; i++) {
            if (a[i] != b[i])
                return a[i] - b[i];
        }
        return 0;
    }
#endif
    return rb->memcmp(s1, s2, n);
}
