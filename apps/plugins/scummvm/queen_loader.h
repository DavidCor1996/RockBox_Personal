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

#ifndef SCUMMVM_QUEEN_LOADER_H
#define SCUMMVM_QUEEN_LOADER_H

#include "scummvm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct scummvm_queen_resource_info {
    char filename[13];
    uint8_t bundle;
    uint32_t offset;
    uint32_t size;
};

struct scummvm_queen_jas_info {
    uint16_t rooms;
    uint16_t names;
    uint16_t objects;
    uint16_t descriptions;
    uint16_t items;
    uint16_t graphics;
    uint16_t walk_offs;
    uint16_t object_descriptions;
    uint16_t furniture;
    uint16_t actors;
    uint16_t actor_anims;
    uint16_t actor_names;
    uint16_t actor_files;
    uint16_t graphic_anims;
    uint16_t command_lists;
    uint16_t command_areas;
    uint16_t command_objects;
    uint16_t command_inventory;
    uint16_t command_game_state;
    uint16_t entry_object;
    uint16_t current_room;
    uint16_t current_room_first_object;
    uint16_t current_room_last_object;
    uint32_t size;
    uint32_t object_data_offset;
    uint32_t room_data_offset;
    uint32_t sfx_name_offset;
    uint32_t item_data_offset;
    uint32_t graphic_data_offset;
    uint32_t grid_data_offset;
    uint32_t grid_object_box_offset;
    uint32_t walk_off_data_offset;
    uint32_t object_description_offset;
    uint32_t command_data_offset;
    uint32_t command_list_offset;
    uint32_t command_area_offset;
    uint32_t command_object_offset;
    uint32_t command_inventory_offset;
    uint32_t command_game_state_offset;
    uint32_t furniture_data_offset;
    uint32_t actor_data_offset;
    uint32_t graphic_anim_offset;
    uint32_t version_offset;
};

struct scummvm_queen_object_data {
    int16_t name;
    uint16_t x;
    uint16_t y;
    uint16_t description;
    int16_t entry_object;
    uint16_t room;
    uint16_t state;
    int16_t image;
};

struct scummvm_queen_item_data {
    int16_t name;
    uint16_t description;
    uint16_t state;
    uint16_t frame;
    int16_t sfx_description;
};

struct scummvm_queen_graphic_data {
    uint16_t x;
    uint16_t y;
    int16_t first_frame;
    int16_t last_frame;
    uint16_t speed;
};

struct scummvm_queen_walk_off_data {
    int16_t entry_object;
    uint16_t x;
    uint16_t y;
};

struct scummvm_queen_object_description {
    uint16_t object;
    uint16_t type;
    uint16_t last_description;
    uint16_t last_seen_number;
};

struct scummvm_queen_furniture_data {
    int16_t room;
    int16_t object_number;
};

struct scummvm_queen_actor_data {
    int16_t room;
    int16_t bob_number;
    uint16_t name;
    int16_t game_state_slot;
    int16_t game_state_value;
    uint16_t color;
    uint16_t standing_frame;
    uint16_t x;
    uint16_t y;
    uint16_t anim;
    uint16_t bank_number;
    uint16_t file;
};

struct scummvm_queen_graphic_anim {
    int16_t key_frame;
    int16_t frame;
    uint16_t speed;
};

struct scummvm_queen_box {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
};

struct scummvm_queen_grid_area {
    int16_t neighbors;
    struct scummvm_queen_box box;
    uint16_t bottom_scale;
    uint16_t top_scale;
    uint16_t object;
};

struct scummvm_queen_grid_room {
    uint16_t room;
    int16_t object_max;
    int16_t area_max;
    uint32_t area_offset;
};

struct scummvm_queen_command_list_data {
    uint16_t verb;
    int16_t noun_object_1;
    int16_t noun_object_2;
    int16_t song;
    bool set_areas;
    bool set_objects;
    bool set_items;
    bool set_conditions;
    int16_t image_order;
    int16_t special_section;
};

struct scummvm_queen_command_area {
    int16_t id;
    int16_t area;
    uint16_t room;
};

struct scummvm_queen_command_object {
    int16_t id;
    int16_t destination_object;
    int16_t source_object;
};

struct scummvm_queen_command_inventory {
    int16_t id;
    int16_t destination_item;
    int16_t source_item;
};

struct scummvm_queen_command_game_state {
    int16_t id;
    int16_t slot;
    int16_t value;
    uint16_t speak_value;
};

struct scummvm_queen_room_object_range {
    uint16_t room;
    uint16_t base_object;
    uint16_t first_object;
    uint16_t last_object;
};

struct scummvm_queen_room_object_summary {
    uint16_t room;
    uint16_t objects;
    uint16_t visible_objects;
    uint16_t hidden_objects;
    uint16_t static_bobs;
    uint16_t animated_bobs;
    uint16_t static_off_bobs;
    uint16_t animated_off_bobs;
    uint16_t person_objects;
    uint16_t paste_downs;
};

struct scummvm_queen_room_entity_summary {
    uint16_t room;
    uint16_t furniture;
    uint16_t furniture_objects;
    uint16_t furniture_paste_downs;
    uint16_t furniture_disabled;
    uint16_t actors;
    uint16_t conditional_actors;
    uint16_t unconditional_actors;
};

struct scummvm_queen_command_match_summary {
    uint16_t matches;
    uint16_t first_match;
    uint16_t last_match;
};

struct scummvm_queen_command_batch_summary {
    uint16_t command;
    uint16_t areas;
    uint16_t areas_on;
    uint16_t areas_off;
    uint16_t objects;
    uint16_t object_shows;
    uint16_t object_hides;
    uint16_t object_copies;
    uint16_t object_deletes;
    uint16_t inventory;
    uint16_t inventory_adds;
    uint16_t inventory_deletes;
    uint16_t game_state_tests;
    uint16_t game_state_sets;
    uint16_t invalid_references;
};

struct scummvm_queen_room_asset_info {
    uint16_t room;
    char room_name[13];
    char backdrop_name[13];
    uint32_t backdrop_size;
    char bank_name[13];
    uint32_t bank_size;
    bool mask_present;
    uint32_t mask_size;
    bool lum_present;
    uint32_t lum_size;
};

struct scummvm_queen_pcx_info {
    uint16_t width;
    uint16_t height;
    uint16_t bytes_per_line;
    uint8_t version;
    uint8_t encoding;
    uint8_t bits_per_pixel;
    uint8_t planes;
    bool has_ega_palette;
    bool has_vga_palette;
};

struct scummvm_queen_text_info {
    uint32_t size;
    uint32_t lines;
    uint32_t object_description_offset;
    uint32_t object_name_offset;
    uint32_t room_name_offset;
    uint32_t verb_name_offset;
    uint32_t joe_response_offset;
    uint32_t actor_anim_offset;
    uint32_t actor_name_offset;
    uint32_t actor_file_offset;
    uint32_t expected_lines;
};

bool scummvm_queen_loader_probe(const struct scummvm_target *target,
                                char *status,
                                size_t status_size);
bool scummvm_queen_loader_table_info(const struct scummvm_target *target,
                                     uint32_t *entries,
                                     char *status,
                                     size_t status_size);
bool scummvm_queen_loader_verify_jas(const struct scummvm_target *target,
                                     char *status,
                                     size_t status_size);
bool scummvm_queen_loader_resource_info(const struct scummvm_target *target,
                                        const char *filename,
                                        struct scummvm_queen_resource_info *info,
                                        uint32_t *entries,
                                        char *status,
                                        size_t status_size);
bool scummvm_queen_loader_read_resource(const struct scummvm_target *target,
                                        const char *filename,
                                        uint32_t skip,
                                        void *dst,
                                        size_t dst_size,
                                        uint32_t *resource_size,
                                        char *status,
                                        size_t status_size);
bool scummvm_queen_loader_jas_info(const struct scummvm_target *target,
                                   struct scummvm_queen_jas_info *info,
                                   char *status,
                                   size_t status_size);
bool scummvm_queen_loader_read_object(const struct scummvm_target *target,
                                      const struct scummvm_queen_jas_info *info,
                                      uint16_t index,
                                      struct scummvm_queen_object_data *object,
                                      char *status,
                                      size_t status_size);
bool scummvm_queen_loader_read_item(const struct scummvm_target *target,
                                    const struct scummvm_queen_jas_info *info,
                                    uint16_t index,
                                    struct scummvm_queen_item_data *item,
                                    char *status,
                                    size_t status_size);
bool scummvm_queen_loader_read_graphic(const struct scummvm_target *target,
                                       const struct scummvm_queen_jas_info *info,
                                       uint16_t index,
                                       struct scummvm_queen_graphic_data *graphic,
                                       char *status,
                                       size_t status_size);
bool scummvm_queen_loader_read_walk_off(const struct scummvm_target *target,
                                        const struct scummvm_queen_jas_info *info,
                                        uint16_t index,
                                        struct scummvm_queen_walk_off_data *walk_off,
                                        char *status,
                                        size_t status_size);
bool scummvm_queen_loader_read_object_description(const struct scummvm_target *target,
                                                 const struct scummvm_queen_jas_info *info,
                                                 uint16_t index,
                                                 struct scummvm_queen_object_description *description,
                                                 char *status,
                                                 size_t status_size);
bool scummvm_queen_loader_read_furniture(const struct scummvm_target *target,
                                         const struct scummvm_queen_jas_info *info,
                                         uint16_t index,
                                         struct scummvm_queen_furniture_data *furniture,
                                         char *status,
                                         size_t status_size);
bool scummvm_queen_loader_read_actor(const struct scummvm_target *target,
                                     const struct scummvm_queen_jas_info *info,
                                     uint16_t index,
                                     struct scummvm_queen_actor_data *actor,
                                     char *status,
                                     size_t status_size);
bool scummvm_queen_loader_read_graphic_anim(const struct scummvm_target *target,
                                           const struct scummvm_queen_jas_info *info,
                                           uint16_t index,
                                           struct scummvm_queen_graphic_anim *anim,
                                           char *status,
                                           size_t status_size);
bool scummvm_queen_loader_read_command_list(const struct scummvm_target *target,
                                           const struct scummvm_queen_jas_info *info,
                                           uint16_t index,
                                           struct scummvm_queen_command_list_data *command,
                                           char *status,
                                           size_t status_size);
bool scummvm_queen_loader_read_command_area(const struct scummvm_target *target,
                                           const struct scummvm_queen_jas_info *info,
                                           uint16_t index,
                                           struct scummvm_queen_command_area *area,
                                           char *status,
                                           size_t status_size);
bool scummvm_queen_loader_read_command_object(const struct scummvm_target *target,
                                             const struct scummvm_queen_jas_info *info,
                                             uint16_t index,
                                             struct scummvm_queen_command_object *object,
                                             char *status,
                                             size_t status_size);
bool scummvm_queen_loader_read_command_inventory(const struct scummvm_target *target,
                                                const struct scummvm_queen_jas_info *info,
                                                uint16_t index,
                                                struct scummvm_queen_command_inventory *inventory,
                                                char *status,
                                                size_t status_size);
bool scummvm_queen_loader_read_command_game_state(const struct scummvm_target *target,
                                                 const struct scummvm_queen_jas_info *info,
                                                 uint16_t index,
                                                 struct scummvm_queen_command_game_state *game_state,
                                                 char *status,
                                                 size_t status_size);
bool scummvm_queen_loader_grid_room(const struct scummvm_target *target,
                                    const struct scummvm_queen_jas_info *info,
                                    uint16_t room,
                                    struct scummvm_queen_grid_room *grid_room,
                                    char *status,
                                    size_t status_size);
bool scummvm_queen_loader_read_grid_area(const struct scummvm_target *target,
                                         const struct scummvm_queen_jas_info *info,
                                         uint16_t room,
                                         uint16_t area,
                                         struct scummvm_queen_grid_area *grid_area,
                                         char *status,
                                         size_t status_size);
bool scummvm_queen_loader_read_object_box(const struct scummvm_target *target,
                                          const struct scummvm_queen_jas_info *info,
                                          uint16_t object,
                                          struct scummvm_queen_box *box,
                                          char *status,
                                          size_t status_size);
bool scummvm_queen_loader_room_range(const struct scummvm_target *target,
                                     const struct scummvm_queen_jas_info *info,
                                     uint16_t room,
                                     struct scummvm_queen_room_object_range *range,
                                     char *status,
                                     size_t status_size);
bool scummvm_queen_loader_room_summary(const struct scummvm_target *target,
                                       const struct scummvm_queen_jas_info *info,
                                       uint16_t room,
                                       struct scummvm_queen_room_object_summary *summary,
                                       char *status,
                                       size_t status_size);
bool scummvm_queen_loader_room_entities(const struct scummvm_target *target,
                                        const struct scummvm_queen_jas_info *info,
                                        uint16_t room,
                                        struct scummvm_queen_room_entity_summary *summary,
                                        char *status,
                                        size_t status_size);
bool scummvm_queen_loader_find_commands(const struct scummvm_target *target,
                                        const struct scummvm_queen_jas_info *info,
                                        uint16_t verb,
                                        int16_t noun_object_1,
                                        int16_t noun_object_2,
                                        struct scummvm_queen_command_match_summary *summary,
                                        char *status,
                                        size_t status_size);
bool scummvm_queen_loader_command_batch(const struct scummvm_target *target,
                                        const struct scummvm_queen_jas_info *info,
                                        uint16_t command,
                                        struct scummvm_queen_command_batch_summary *summary,
                                        char *status,
                                        size_t status_size);
bool scummvm_queen_loader_room_assets(const struct scummvm_target *target,
                                      const struct scummvm_queen_jas_info *info,
                                      const struct scummvm_queen_text_info *text,
                                      uint16_t room,
                                      struct scummvm_queen_room_asset_info *assets,
                                      char *status,
                                      size_t status_size);
bool scummvm_queen_loader_pcx_info(const struct scummvm_target *target,
                                   const char *filename,
                                   struct scummvm_queen_pcx_info *pcx,
                                   char *status,
                                   size_t status_size);
bool scummvm_queen_loader_text_info(const struct scummvm_target *target,
                                    const struct scummvm_queen_jas_info *jas,
                                    struct scummvm_queen_text_info *text,
                                    char *status,
                                    size_t status_size);
bool scummvm_queen_loader_read_text_line(const struct scummvm_target *target,
                                         uint32_t line,
                                         char *out,
                                         size_t out_size,
                                         char *status,
                                         size_t status_size);

#ifdef __cplusplus
}
#endif

#endif
