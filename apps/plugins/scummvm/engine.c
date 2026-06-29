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

#include "engine.h"
#include "rbfile.h"
#include "sky_cpt_loader.h"
#include "upstream_bridge.h"

static bool read_u32le_file(struct scummvm_file *file, unsigned long *value)
{
    unsigned char buf[4];

    if (!scummvm_file_seek(file, 0))
        return false;

    if (scummvm_file_read(file, buf, sizeof(buf)) != (long)sizeof(buf))
        return false;

    *value = (unsigned long)buf[0] |
             ((unsigned long)buf[1] << 8) |
             ((unsigned long)buf[2] << 16) |
             ((unsigned long)buf[3] << 24);
    return true;
}

static bool prepare_sky(const struct scummvm_target *target,
                        struct scummvm_engine_state *state)
{
    struct scummvm_file dsk;
    struct scummvm_file dnr;
    struct scummvm_sky_cpt_info cpt_info;
    unsigned long entries = 0;

    if (!scummvm_file_open_game(&dsk, target, "sky.dsk"))
        return false;

    state->primary_size = scummvm_file_size(&dsk);
    scummvm_file_close(&dsk);

    if (!scummvm_file_open_game(&dnr, target, "sky.dnr"))
        return false;

    if (!read_u32le_file(&dnr, &entries)) {
        scummvm_file_close(&dnr);
        return false;
    }

    state->aux_value = entries;
    scummvm_file_close(&dnr);

    if (!scummvm_sky_cpt_load(target, &cpt_info, state->status,
                              sizeof(state->status)))
        return false;

    rb->snprintf(state->status, sizeof(state->status),
                 "Sky ready: %lu resources, %lu compacts",
                 entries, (unsigned long)cpt_info.compact_entries);
    state->initialized = true;
    return true;
}

static bool prepare_queen(const struct scummvm_target *target,
                          struct scummvm_engine_state *state)
{
    struct scummvm_file queen;

    if (!scummvm_file_open_game(&queen, target, "queen.1") &&
        !scummvm_file_open_game(&queen, target, "queen.1c"))
        return false;

    state->primary_size = scummvm_file_size(&queen);
    scummvm_file_close(&queen);

    rb->snprintf(state->status, sizeof(state->status),
                 "Queen ready: %ld byte resource", state->primary_size);
    state->initialized = true;
    return true;
}

bool scummvm_engine_prepare(const struct scummvm_target *target,
                            const struct scummvm_probe_result *probe,
                            struct scummvm_engine_state *state)
{
    rb->memset(state, 0, sizeof(*state));

    if (!probe->supported) {
        rb->strlcpy(state->status, "Engine not supported",
                    sizeof(state->status));
        return false;
    }

    if (!probe->data_found) {
        rb->strlcpy(state->status, probe->detail, sizeof(state->status));
        return false;
    }

    if (scummvm_upstream_can_run(target) &&
        scummvm_upstream_init(target, state)) {
        state->initialized = true;
        return true;
    }

    if (!rb->strcasecmp(target->engine, "sky"))
        return prepare_sky(target, state);

    if (!rb->strcasecmp(target->engine, "queen"))
        return prepare_queen(target, state);

    rb->strlcpy(state->status, "No runtime for engine",
                sizeof(state->status));
    return false;
}
