/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
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

#ifndef SCUMMVM_ROCKBOX_H
#define SCUMMVM_ROCKBOX_H

#include "plugin.h"

#define SCUMMVM_DEFAULT_SAVE_DIR ROCKBOX_DIR "/scummvm/saves"
#define SCUMMVM_ENGINE_DATA_DIR ROCKBOX_DIR "/scummvm/engine-data"

struct scummvm_target {
    char gameid[64];
    char engine[32];
    char path[MAX_PATH];
    char savepath[MAX_PATH];
};

enum plugin_status scummvm_backend_run(const struct scummvm_target *target);

#endif
