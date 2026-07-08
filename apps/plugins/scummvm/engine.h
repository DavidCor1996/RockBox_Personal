/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#ifndef SCUMMVM_ENGINE_H
#define SCUMMVM_ENGINE_H

#include "probe.h"

struct scummvm_engine_state {
    bool initialized;
    char status[96];
    long primary_size;
    unsigned long aux_value;
};

bool scummvm_engine_prepare(const struct scummvm_target *target,
                            const struct scummvm_probe_result *probe,
                            struct scummvm_engine_state *state);

#endif
