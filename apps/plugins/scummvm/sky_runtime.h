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

#ifndef SCUMMVM_SKY_RUNTIME_H
#define SCUMMVM_SKY_RUNTIME_H

#include "scummvm.h"
#include "video.h"

#ifdef __cplusplus
extern "C" {
#endif

struct scummvm_sky_runtime_info {
    uint32_t logic_list_id;
    uint32_t logic_entries;
    uint32_t active_logic_entries;
    uint32_t redirects;
    uint32_t current_section;
    uint32_t current_screen;
    uint16_t first_logic_id;
    uint16_t first_logic_status;
    char first_logic_name[32];
};

#define SCUMMVM_SKY_OVERLAY_LINES 8
#define SCUMMVM_SKY_OVERLAY_TEXT 96

struct scummvm_sky_overlay_line {
    char text[SCUMMVM_SKY_OVERLAY_TEXT];
    uint16_t value;
    int y;
    bool selectable;
};

struct scummvm_sky_overlay {
    struct scummvm_sky_overlay_line lines[SCUMMVM_SKY_OVERLAY_LINES];
    uint16_t count;
    bool choosing;
};

bool scummvm_sky_runtime_init(const struct scummvm_target *target,
                              char *status,
                              size_t status_size);
bool scummvm_sky_runtime_step(char *status, size_t status_size);
bool scummvm_sky_runtime_render(struct scummvm_video *video);
void scummvm_sky_runtime_input(int x, int y, bool down, bool clicked);
bool scummvm_sky_runtime_overlay(struct scummvm_sky_overlay *overlay);
bool scummvm_sky_runtime_scan(struct scummvm_sky_runtime_info *info,
                              char *status,
                              size_t status_size);
uint32_t scummvm_sky_runtime_get_var(uint16_t index);
void scummvm_sky_runtime_set_var(uint16_t index, uint32_t value);
void scummvm_sky_runtime_reset(void);

#ifdef __cplusplus
}
#endif

#endif
