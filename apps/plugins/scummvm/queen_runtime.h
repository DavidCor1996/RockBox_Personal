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

#ifndef SCUMMVM_QUEEN_RUNTIME_H
#define SCUMMVM_QUEEN_RUNTIME_H

#include "scummvm.h"
#include "video.h"

#define SCUMMVM_QUEEN_INVENTORY_SLOTS 4
#define SCUMMVM_QUEEN_OVERLAY_TEXT 64
#define SCUMMVM_QUEEN_DIALOG_OPTIONS 4

struct scummvm_queen_overlay {
    bool active;
    char verb[SCUMMVM_QUEEN_OVERLAY_TEXT];
    char inventory[SCUMMVM_QUEEN_INVENTORY_SLOTS][SCUMMVM_QUEEN_OVERLAY_TEXT];
    char message[SCUMMVM_QUEEN_OVERLAY_TEXT];
    char options[SCUMMVM_QUEEN_DIALOG_OPTIONS][SCUMMVM_QUEEN_OVERLAY_TEXT];
    bool option_active[SCUMMVM_QUEEN_DIALOG_OPTIONS];
    uint16_t option_count;
};

bool scummvm_queen_runtime_init(const struct scummvm_target *target,
                                struct scummvm_video *video,
                                char *status,
                                size_t status_size);
void scummvm_queen_runtime_input(int x,
                                 int y,
                                 bool clicked,
                                 char *status,
                                 size_t status_size);
void scummvm_queen_runtime_cycle_verb(int direction,
                                      char *status,
                                      size_t status_size);
void scummvm_queen_runtime_cycle_inventory(int direction,
                                           char *status,
                                           size_t status_size);
bool scummvm_queen_runtime_overlay(struct scummvm_queen_overlay *overlay);
bool scummvm_queen_runtime_save(char *status, size_t status_size);

#endif
