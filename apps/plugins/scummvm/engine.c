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
#include "queen_loader.h"
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

static uint16_t queen_default_verb(uint16_t object_state)
{
    static const uint16_t verbs[] = {
        0, 1, 0, 2,
        0, 0, 9, 3,
        5, 8, 0, 0,
        6, 0, 7, 0
    };

    return verbs[(object_state >> 4) & 0x0f];
}

static bool prepare_queen(const struct scummvm_target *target,
                          struct scummvm_engine_state *state)
{
    struct scummvm_file queen;
    struct scummvm_queen_jas_info jas_info;
    struct scummvm_queen_room_object_summary room_summary;
    struct scummvm_queen_room_entity_summary entity_summary;
    struct scummvm_queen_text_info text_info;
    struct scummvm_queen_room_asset_info room_assets;
    uint32_t entries = 0;

    if (!scummvm_file_open_game(&queen, target, "queen.1") &&
        !scummvm_file_open_game(&queen, target, "queen.1c"))
        return false;

    state->primary_size = scummvm_file_size(&queen);
    scummvm_file_close(&queen);

    if (!scummvm_queen_loader_table_info(target, &entries,
                                         state->status,
                                         sizeof(state->status)))
        return false;
    if (!scummvm_queen_loader_verify_jas(target,
                                         state->status,
                                         sizeof(state->status)))
        return false;
    if (!scummvm_queen_loader_jas_info(target, &jas_info,
                                       state->status,
                                       sizeof(state->status)))
        return false;
    if (!scummvm_queen_loader_room_summary(target, &jas_info,
                                           jas_info.current_room,
                                           &room_summary,
                                           state->status,
                                           sizeof(state->status)))
        return false;
    if (!scummvm_queen_loader_room_entities(target, &jas_info,
                                            jas_info.current_room,
                                            &entity_summary,
                                            state->status,
                                            sizeof(state->status)))
        return false;
    if (jas_info.items > 0) {
        struct scummvm_queen_item_data item;

        if (!scummvm_queen_loader_read_item(target, &jas_info, 1, &item,
                                            state->status,
                                            sizeof(state->status)))
            return false;
    }
    if (jas_info.walk_offs > 0) {
        struct scummvm_queen_walk_off_data walk_off;

        if (!scummvm_queen_loader_read_walk_off(target, &jas_info, 1,
                                                &walk_off, state->status,
                                                sizeof(state->status)))
            return false;
    }
    if (jas_info.object_descriptions > 0) {
        struct scummvm_queen_object_description description;

        if (!scummvm_queen_loader_read_object_description(target, &jas_info,
                                                         1, &description,
                                                         state->status,
                                                         sizeof(state->status)))
            return false;
    }
    if (jas_info.furniture > 0) {
        struct scummvm_queen_furniture_data furniture;

        if (!scummvm_queen_loader_read_furniture(target, &jas_info, 1,
                                                 &furniture, state->status,
                                                 sizeof(state->status)))
            return false;
    }
    if (jas_info.actors > 0) {
        struct scummvm_queen_actor_data actor;

        if (!scummvm_queen_loader_read_actor(target, &jas_info, 1, &actor,
                                             state->status,
                                             sizeof(state->status)))
            return false;
    }
    if (jas_info.graphic_anims > 0) {
        struct scummvm_queen_graphic_anim anim;

        if (!scummvm_queen_loader_read_graphic_anim(target, &jas_info, 1,
                                                    &anim, state->status,
                                                    sizeof(state->status)))
            return false;
    }
    if (jas_info.command_lists > 0) {
        struct scummvm_queen_command_list_data command;

        if (!scummvm_queen_loader_read_command_list(target, &jas_info, 1,
                                                    &command, state->status,
                                                    sizeof(state->status)))
            return false;
    }
    if (jas_info.command_areas > 0) {
        struct scummvm_queen_command_area area;

        if (!scummvm_queen_loader_read_command_area(target, &jas_info, 1,
                                                    &area, state->status,
                                                    sizeof(state->status)))
            return false;
    }
    if (jas_info.command_objects > 0) {
        struct scummvm_queen_command_object object;

        if (!scummvm_queen_loader_read_command_object(target, &jas_info, 1,
                                                      &object, state->status,
                                                      sizeof(state->status)))
            return false;
    }
    if (jas_info.command_inventory > 0) {
        struct scummvm_queen_command_inventory inventory;

        if (!scummvm_queen_loader_read_command_inventory(target, &jas_info, 1,
                                                         &inventory,
                                                         state->status,
                                                         sizeof(state->status)))
            return false;
    }
    if (jas_info.command_game_state > 0) {
        struct scummvm_queen_command_game_state game_state;

        if (!scummvm_queen_loader_read_command_game_state(target, &jas_info,
                                                          1, &game_state,
                                                          state->status,
                                                          sizeof(state->status)))
            return false;
    }
    if (!scummvm_queen_loader_text_info(target, &jas_info, &text_info,
                                        state->status,
                                        sizeof(state->status)))
        return false;
    if (!scummvm_queen_loader_room_assets(target, &jas_info, &text_info,
                                          jas_info.current_room,
                                          &room_assets,
                                          state->status,
                                          sizeof(state->status)))
        return false;
    if (rb->strlen(room_assets.backdrop_name) >= 4 &&
        !rb->strcasecmp(room_assets.backdrop_name +
                        rb->strlen(room_assets.backdrop_name) - 4,
                        ".PCX")) {
        struct scummvm_queen_pcx_info pcx_info;

        if (!scummvm_queen_loader_pcx_info(target,
                                           room_assets.backdrop_name,
                                           &pcx_info,
                                           state->status,
                                           sizeof(state->status)))
            return false;
    }
    {
        struct scummvm_queen_grid_room grid_room;
        struct scummvm_queen_box object_box;

        if (!scummvm_queen_loader_grid_room(target, &jas_info,
                                            jas_info.current_room,
                                            &grid_room,
                                            state->status,
                                            sizeof(state->status)))
            return false;
        if (grid_room.area_max > 0) {
            struct scummvm_queen_grid_area grid_area;

            if (!scummvm_queen_loader_read_grid_area(target, &jas_info,
                                                     jas_info.current_room,
                                                     1, &grid_area,
                                                     state->status,
                                                     sizeof(state->status)))
                return false;
        }
        if (!scummvm_queen_loader_read_object_box(
                target, &jas_info, jas_info.current_room_first_object,
                &object_box, state->status, sizeof(state->status)))
            return false;
    }
    {
        char text_line[64];

        if (!scummvm_queen_loader_read_text_line(
                target,
                text_info.room_name_offset + jas_info.current_room - 1,
                text_line, sizeof(text_line),
                state->status, sizeof(state->status)))
            return false;
    }
    {
        uint16_t object_index;

        for (object_index = jas_info.current_room_first_object;
             jas_info.current_room_first_object <=
                 jas_info.current_room_last_object &&
             object_index <= jas_info.current_room_last_object;
             object_index++) {
            struct scummvm_queen_object_data object;

            if (!scummvm_queen_loader_read_object(target, &jas_info,
                                                  object_index, &object,
                                                  state->status,
                                                  sizeof(state->status)))
                return false;
            if (object.name > 0) {
                char text_line[64];

                if (!scummvm_queen_loader_read_text_line(
                        target,
                        text_info.object_name_offset + object.name - 1,
                        text_line, sizeof(text_line),
                        state->status, sizeof(state->status)))
                    return false;
                break;
            }
        }
    }
    {
        uint16_t object_index;

        for (object_index = jas_info.current_room_first_object;
             jas_info.current_room_first_object <=
                 jas_info.current_room_last_object &&
             object_index <= jas_info.current_room_last_object;
             object_index++) {
            struct scummvm_queen_object_data object;
            uint16_t verb;

            if (!scummvm_queen_loader_read_object(target, &jas_info,
                                                  object_index, &object,
                                                  state->status,
                                                  sizeof(state->status)))
                return false;

            verb = queen_default_verb(object.state);
            if (object.name > 0 && verb > 0) {
                struct scummvm_queen_command_match_summary matches;

                if (!scummvm_queen_loader_find_commands(
                        target, &jas_info, verb, (int16_t)object_index, 0,
                        &matches, state->status, sizeof(state->status)))
                    return false;
                if (matches.matches > 0) {
                    struct scummvm_queen_command_batch_summary batch;

                    if (!scummvm_queen_loader_command_batch(
                            target, &jas_info, matches.first_match, &batch,
                            state->status, sizeof(state->status)))
                        return false;
                }
                break;
            }
        }
    }

    state->aux_value = jas_info.objects;
    rb->snprintf(state->status, sizeof(state->status),
                 "Queen ready: %lu res, %s, %u objs, %u ent, %lu text",
                 (unsigned long)entries,
                 room_assets.room_name,
                 (unsigned)room_summary.objects,
                 (unsigned)(entity_summary.furniture +
                            entity_summary.actors),
                 (unsigned long)text_info.lines);
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

    DEBUGF("scummvm: prepare upstream_can=%d engine=%s\n",
           scummvm_upstream_can_run(target), target->engine);
    if (scummvm_upstream_can_run(target) &&
        scummvm_upstream_init(target, state)) {
        DEBUGF("scummvm: upstream init succeeded: %s\n", state->status);
        state->initialized = true;
        return true;
    }
    DEBUGF("scummvm: upstream init skipped/failed: %s\n", state->status);

    if (!rb->strcasecmp(target->engine, "queen")) {
        if (state->status[0] == '\0')
            rb->strlcpy(state->status, "Queen upstream init failed",
                        sizeof(state->status));
        return false;
    }

    if (!rb->strcasecmp(target->engine, "sky"))
        return prepare_sky(target, state);

    rb->strlcpy(state->status, "No runtime for engine",
                sizeof(state->status));
    return false;
}
