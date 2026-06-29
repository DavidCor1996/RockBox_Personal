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

#ifndef SCUMMVM_UPSTREAM_BRIDGE_H
#define SCUMMVM_UPSTREAM_BRIDGE_H

#include "engine.h"
#include "video.h"

#ifdef __cplusplus
extern "C" {
#endif

bool scummvm_upstream_can_run(const struct scummvm_target *target);
bool scummvm_upstream_init(const struct scummvm_target *target,
                           struct scummvm_engine_state *state);
bool scummvm_upstream_frame(const struct scummvm_target *target,
                            struct scummvm_engine_state *state,
                            struct scummvm_video *video);

#ifdef __cplusplus
}
#endif

#endif
