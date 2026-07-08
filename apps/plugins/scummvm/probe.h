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

#ifndef SCUMMVM_PROBE_H
#define SCUMMVM_PROBE_H

#include "scummvm.h"

struct scummvm_probe_result {
    bool supported;
    bool data_found;
    bool engine_data_found;
    char engine_name[32];
    char variant[64];
    char detail[96];
    char engine_data_detail[96];
};

void scummvm_probe_game(const struct scummvm_target *target,
                       struct scummvm_probe_result *result);

#endif
