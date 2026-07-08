/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
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

#ifndef SCUMMVM_SKY_TEXT_H
#define SCUMMVM_SKY_TEXT_H

#include "scummvm.h"
#include "sky_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

bool scummvm_sky_text_init(const struct scummvm_target *target,
                           char *status,
                           size_t status_size);
bool scummvm_sky_text_decode(uint32_t text_num,
                             char *out,
                             size_t out_size);
bool scummvm_sky_text_render(uint32_t text_num,
                             uint16_t pixel_width,
                             uint8_t color,
                             bool center,
                             struct scummvm_sky_resource *out,
                             uint16_t *text_width);
void scummvm_sky_text_reset(void);

#ifdef __cplusplus
}
#endif

#endif
