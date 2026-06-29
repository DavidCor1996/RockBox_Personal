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

#ifndef SCUMMVM_SKY_LOADER_H
#define SCUMMVM_SKY_LOADER_H

#include "scummvm.h"
#include "video.h"

struct scummvm_sky_resource {
    uint16_t file_nr;
    uint8_t *data;
    uint32_t size;
};

#ifdef __cplusplus
extern "C" {
#endif

bool scummvm_sky_loader_probe(const struct scummvm_target *target,
                              char *status,
                              size_t status_size);
bool scummvm_sky_loader_load_startup(const struct scummvm_target *target,
                                     char *status,
                                     size_t status_size);
bool scummvm_sky_loader_load_screen(const struct scummvm_target *target,
                                    uint16_t screen_file,
                                    uint16_t palette_file,
                                    char *status,
                                    size_t status_size);
bool scummvm_sky_loader_load_sequence(const struct scummvm_target *target,
                                      uint16_t sequence_file,
                                      char *status,
                                      size_t status_size);
bool scummvm_sky_loader_load_resource(const struct scummvm_target *target,
                                      uint16_t file_nr,
                                      struct scummvm_sky_resource *resource,
                                      char *status,
                                      size_t status_size);
void scummvm_sky_loader_release_resource(
    struct scummvm_sky_resource *resource);
bool scummvm_sky_loader_sequence_running(void);
bool scummvm_sky_loader_step_sequence(void);
bool scummvm_sky_loader_bootstrap_section0(const struct scummvm_target *target,
                                           char *status,
                                           size_t status_size);
bool scummvm_sky_loader_render_current(struct scummvm_video *video);
void scummvm_sky_loader_reset(void);

#ifdef __cplusplus
}
#endif

#endif
