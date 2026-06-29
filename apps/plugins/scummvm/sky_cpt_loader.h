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

#ifndef SCUMMVM_SKY_CPT_LOADER_H
#define SCUMMVM_SKY_CPT_LOADER_H

#include "scummvm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct scummvm_sky_cpt_info {
    uint16_t data_lists;
    uint32_t data_entries;
    uint32_t compact_entries;
    uint32_t dlinc_entries;
    uint32_t diff_entries;
    uint32_t save_ids;
    uint32_t raw_words;
    uint32_t src_words;
    uint32_t ascii_bytes;
    char source_path[MAX_PATH];
};

bool scummvm_sky_cpt_load(const struct scummvm_target *target,
                          struct scummvm_sky_cpt_info *info,
                          char *status,
                          size_t status_size);
const uint16_t *scummvm_sky_cpt_fetch(uint16_t cpt_id,
                                      uint16_t *size,
                                      uint16_t *type,
                                      const char **name);
void scummvm_sky_cpt_unload(void);

#ifdef __cplusplus
}
#endif

#endif
