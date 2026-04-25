/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: wmcodec-s5l8700.c 22025 2009-07-25 00:49:13Z dave $
 *
 * S5L8702-specific code for Cirrus codecs
 *
 * Copyright (c) 2010 Michael Sparmann
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

#include <string.h>

#include "audiohw.h"
#include "cscodec.h"

static unsigned char codec_shadow[256];
static bool codec_shadow_valid = false;

static void codec_shadow_init(void)
{
    if (!codec_shadow_valid)
    {
        memset(codec_shadow, 0, sizeof(codec_shadow));
        codec_shadow_valid = true;
    }
}

void audiohw_init(void)
{
    codec_shadow_init();
#ifdef HAVE_CS42L55
    audiohw_preinit();
#endif
}

unsigned char cscodec_read(int reg)
{
    codec_shadow_init();
    return codec_shadow[(unsigned char)reg];
}

void cscodec_write(int reg, unsigned char data)
{
    codec_shadow_init();
    codec_shadow[(unsigned char)reg] = data;
}

void cscodec_power(bool state)
{
    (void)state;
}

void cscodec_reset(bool state)
{
    (void)state;
}

void cscodec_clock(bool state)
{
    (void)state;
}
