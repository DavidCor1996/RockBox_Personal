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

#ifndef SCUMMVM_SKY_SOUND_MAP_H
#define SCUMMVM_SKY_SOUND_MAP_H

#include "scummvm.h"

#define SCUMMVM_SKY_SFX_BASE 256
#define SCUMMVM_SKY_SFX_MAP_COUNT 139
#define SCUMMVM_SKY_SFX_ROOM_COUNT 10
#define SCUMMVM_SKY_SFXF_SAVE 0x20
#define SCUMMVM_SKY_SFXF_START_DELAY 0x80

struct scummvm_sky_sfx_room {
    uint8_t room;
    uint8_t adlib_volume;
    uint8_t roland_volume;
};

struct scummvm_sky_sfx_entry {
    uint8_t sound_no;
    uint8_t flags;
    struct scummvm_sky_sfx_room rooms[SCUMMVM_SKY_SFX_ROOM_COUNT];
};

extern const struct scummvm_sky_sfx_entry
    scummvm_sky_sfx_map[SCUMMVM_SKY_SFX_MAP_COUNT];

#endif
