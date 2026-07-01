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

#include "queen_runtime.h"
#include "queen_loader.h"
#include "rbfile.h"

#define QUEEN_PCX_MAX_SIZE 180000
#define QUEEN_PCX_HEADER_SIZE 128
#define QUEEN_PCX_VGA_PALETTE_SIZE 769
#define QUEEN_BBK_MAX_SIZE 524288
#define QUEEN_BBK_MAX_FRAMES 110
#define QUEEN_JOE_BBK_MAX_SIZE 65536
#define QUEEN_SCRIPT_HEADER_SKIP 20
#define QUEEN_SCRIPT_HEADER_MAX 8192
#define QUEEN_RUNTIME_MAX_OBJECTS 4096
#define QUEEN_RUNTIME_MAX_ITEMS 512
#define QUEEN_RUNTIME_MAX_OBJECT_DESCRIPTIONS 4096
#define QUEEN_RUNTIME_GAME_STATE_COUNT 211
#define QUEEN_RUNTIME_MAX_ROOM_AREAS 128
#define QUEEN_RUNTIME_MAX_RENDER_ITEMS 256
#define QUEEN_RUNTIME_MAX_CUTAWAY_CHAIN 16
#define QUEEN_RUNTIME_SAVE_VERSION 9
#define QUEEN_RUNTIME_TALK_SELECTED_COUNT 86
#define QUEEN_VERB_NONE 0
#define QUEEN_VERB_OPEN 1
#define QUEEN_VERB_CLOSE 2
#define QUEEN_VERB_MOVE 3
#define QUEEN_VERB_GIVE 5
#define QUEEN_VERB_USE 6
#define QUEEN_VERB_PICK_UP 7
#define QUEEN_VERB_TALK_TO 8
#define QUEEN_VERB_LOOK_AT 9
#define QUEEN_VERB_WALK_TO 10
#define QUEEN_DIR_LEFT 1
#define QUEEN_DIR_RIGHT 2
#define QUEEN_DIR_FRONT 3
#define QUEEN_DIR_BACK 4
#define QUEEN_GAME_STATE_JOE_DRESSING_MODE 19
#define QUEEN_GAME_STATE_BYPASS_ZOMBIES 21
#define QUEEN_GAME_STATE_BYPASS_FLODA_RECEPTIONIST 35
#define QUEEN_GAME_STATE_GUARDS_TURNED_ON 85
#define QUEEN_GAME_STATE_HOTEL_ESCAPE_STATE 93
#define QUEEN_GAME_STATE_INTRO_PLAYED 117
#define QUEEN_GAME_STATE_AZURA_IN_LOVE 167
#define QUEEN_ROOM_JUNGLE_BRIDGE 4
#define QUEEN_ROOM_JUNGLE_GORILLA_1 6
#define QUEEN_ROOM_JUNGLE_GORILLA_2 14
#define QUEEN_ROOM_AMAZON_ENTRANCE 16
#define QUEEN_ROOM_AMAZON_HIDEOUT 17
#define QUEEN_ROOM_FLODA_OUTSIDE 22
#define QUEEN_ROOM_FLODA_KITCHEN 26
#define QUEEN_ROOM_FLODA_KLUNK 30
#define QUEEN_ROOM_FLODA_HENRY 32
#define QUEEN_ROOM_TEMPLE_ZOMBIES 50
#define QUEEN_ROOM_TEMPLE_SNAKE 53
#define QUEEN_ROOM_TEMPLE_LIZARD_LASER 55
#define QUEEN_ROOM_HOTEL_DOWNSTAIRS 71
#define QUEEN_ROOM_HOTEL_LOBBY 73
#define QUEEN_ROOM_FOTAQ_LOGO 95
#define QUEEN_ROOM_TEMPLE_MAZE_5 100
#define QUEEN_ROOM_TEMPLE_MAZE_6 101
#define QUEEN_ROOM_FLODA_FRONTDESK 103
#define QUEEN_ENTRY_HOTEL_LOBBY 584

struct queen_bbk_frame {
    uint32_t offset;
    uint16_t width;
    uint16_t height;
    uint16_t xhotspot;
    uint16_t yhotspot;
};

struct queen_bbk_bank {
    unsigned char *data;
    size_t capacity;
    struct queen_bbk_frame frames[QUEEN_BBK_MAX_FRAMES + 1];
    char name[13];
    uint16_t frame_count;
    bool loaded;
};

struct queen_render_item {
    struct scummvm_queen_graphic_data graphic;
    const struct queen_bbk_bank *bank;
    uint16_t frame_override;
    uint16_t order;
    bool hotspot;
    bool xflip;
};

struct queen_cutaway_object {
    int16_t object_number;
    int16_t move_to_x;
    int16_t move_to_y;
    int16_t bank;
    int16_t anim_list;
    int16_t execute;
    int16_t limit_bob_x1;
    int16_t limit_bob_y1;
    int16_t limit_bob_x2;
    int16_t limit_bob_y2;
    int16_t special_move;
    int16_t anim_type;
    int16_t from_object;
    int16_t bob_start_x;
    int16_t bob_start_y;
    int16_t room;
    int16_t scale;
    int16_t song;
    int16_t person_count;
    int16_t person[6];
};

static unsigned char queen_pcx_data[QUEEN_PCX_MAX_SIZE];
static unsigned char queen_room_bbk_data[QUEEN_BBK_MAX_SIZE];
static unsigned char queen_actor_bbk_data[QUEEN_BBK_MAX_SIZE];
static unsigned char queen_joe_bbk_data[QUEEN_JOE_BBK_MAX_SIZE];
static unsigned char queen_joe_anim_bbk_data[QUEEN_JOE_BBK_MAX_SIZE];
static unsigned char queen_script_header[QUEEN_SCRIPT_HEADER_MAX];
static fb_data queen_pcx_palette[256];
static struct queen_bbk_bank queen_room_bbk = {
    .data = queen_room_bbk_data,
    .capacity = sizeof(queen_room_bbk_data)
};
static struct queen_bbk_bank queen_actor_bbk = {
    .data = queen_actor_bbk_data,
    .capacity = sizeof(queen_actor_bbk_data)
};
static struct queen_bbk_bank queen_joe_bbk = {
    .data = queen_joe_bbk_data,
    .capacity = sizeof(queen_joe_bbk_data)
};
static struct queen_bbk_bank queen_joe_anim_bbk = {
    .data = queen_joe_anim_bbk_data,
    .capacity = sizeof(queen_joe_anim_bbk_data)
};
static struct scummvm_queen_object_data queen_runtime_objects[QUEEN_RUNTIME_MAX_OBJECTS + 1];
static uint16_t queen_runtime_object_frame_override[QUEEN_RUNTIME_MAX_OBJECTS + 1];
static bool queen_runtime_object_xflip[QUEEN_RUNTIME_MAX_OBJECTS + 1];
static struct scummvm_queen_item_data queen_runtime_items[QUEEN_RUNTIME_MAX_ITEMS + 1];
static struct scummvm_queen_object_description queen_runtime_object_descriptions[QUEEN_RUNTIME_MAX_OBJECT_DESCRIPTIONS + 1];
static struct scummvm_queen_grid_area queen_runtime_room_areas[QUEEN_RUNTIME_MAX_ROOM_AREAS + 1];
static int16_t queen_runtime_game_state[QUEEN_RUNTIME_GAME_STATE_COUNT];
static bool queen_runtime_inventory[QUEEN_RUNTIME_MAX_ITEMS + 1];
static uint16_t queen_runtime_inventory_slots[SCUMMVM_QUEEN_INVENTORY_SLOTS];
static uint16_t queen_runtime_inventory_page_first;
static uint16_t queen_runtime_selected_inventory_item;
static uint16_t queen_runtime_selected_verb;
static bool queen_runtime_talked_to[QUEEN_RUNTIME_TALK_SELECTED_COUNT];
static int16_t queen_runtime_talk_values[QUEEN_RUNTIME_TALK_SELECTED_COUNT][4];
static char queen_runtime_message[SCUMMVM_QUEEN_OVERLAY_TEXT];
static char queen_runtime_dialog_file[16];
static char queen_runtime_dialog_options[SCUMMVM_QUEEN_DIALOG_OPTIONS]
                                        [SCUMMVM_QUEEN_OVERLAY_TEXT];
static bool queen_runtime_dialog_option_active[SCUMMVM_QUEEN_DIALOG_OPTIONS];
static int16_t queen_runtime_dialog_option_head[SCUMMVM_QUEEN_DIALOG_OPTIONS];
static int16_t queen_runtime_dialog_option_return[SCUMMVM_QUEEN_DIALOG_OPTIONS];
static int16_t queen_runtime_dialog_option_game_state_index
    [SCUMMVM_QUEEN_DIALOG_OPTIONS];
static int16_t queen_runtime_dialog_option_game_state_value
    [SCUMMVM_QUEEN_DIALOG_OPTIONS];
static uint16_t queen_runtime_dialog_option_count;
static uint16_t queen_runtime_dialog_talk_slot;
static uint16_t queen_runtime_dialog_level_max;
static uint16_t queen_runtime_dialog_joe_ptr_off;
static uint16_t queen_runtime_dialog_person1_off;
static uint16_t queen_runtime_dialog_person2_off;
static uint16_t queen_runtime_dialog_cutaway_off;
static uint32_t queen_runtime_dialog_payload_size;
static int16_t queen_runtime_dialog_joe_max;
static int16_t queen_runtime_dialog_person_max;
static int16_t queen_runtime_dialog_post_game_state[2];
static int16_t queen_runtime_dialog_post_test_value[2];
static int16_t queen_runtime_dialog_post_item[2];
static bool queen_runtime_dialog_talk_slot_valid;
static bool queen_runtime_dialog_was_repeat;
static struct scummvm_target queen_runtime_target;
static struct scummvm_queen_jas_info queen_runtime_jas;
static struct scummvm_queen_text_info queen_runtime_text;
static struct scummvm_queen_room_object_range queen_runtime_range;
static struct scummvm_video *queen_runtime_video;
static uint16_t queen_runtime_current_room;
static uint16_t queen_runtime_room_area_count;
static uint16_t queen_runtime_anim_tick;
static uint16_t queen_runtime_selected_object;
static int16_t queen_runtime_joe_x;
static int16_t queen_runtime_joe_y;
static int16_t queen_runtime_joe_start_x;
static int16_t queen_runtime_joe_start_y;
static int16_t queen_runtime_joe_target_x;
static int16_t queen_runtime_joe_target_y;
static uint16_t queen_runtime_joe_facing;
static uint16_t queen_runtime_joe_walk_step;
static uint16_t queen_runtime_joe_walk_steps;
static uint16_t queen_runtime_puzzle_attempt_count;
static bool queen_runtime_joe_walking;
static bool queen_runtime_scene_has_animation;
static bool queen_runtime_dirty;
static bool queen_runtime_active;

static bool queen_runtime_decode_pcx(struct scummvm_video *video,
                                     const char *filename,
                                     uint32_t size,
                                     char *status,
                                     size_t status_size);
static bool queen_runtime_load_room_areas(char *status, size_t status_size);
static void queen_runtime_update_linked_entry_object(int16_t entry_object,
                                                     uint16_t verb);
static bool queen_runtime_describe_object(uint16_t object_index,
                                          char *status,
                                          size_t status_size);
static bool queen_runtime_show_object_description(uint16_t description,
                                                  char *status,
                                                  size_t status_size);
static bool queen_runtime_execute_inventory_item_verb(uint16_t item,
                                                      uint16_t verb,
                                                      char *status,
                                                      size_t status_size);
static bool queen_runtime_probe_script_resource(const char *filename,
                                                bool dialog,
                                                char *status,
                                                size_t status_size);
static bool queen_runtime_probe_script_resource_chained(const char *filename,
                                                        bool dialog,
                                                        char *status,
                                                        size_t status_size);
static bool queen_runtime_show_joe_response(uint16_t response,
                                            char *status,
                                            size_t status_size);
static bool queen_runtime_handle_inventory_input(int x,
                                                 int y,
                                                 bool clicked,
                                                 bool *handled,
                                                 char *status,
                                                 size_t status_size);
static bool queen_runtime_finish_dialog(char *status, size_t status_size);
static bool queen_runtime_dialog_auto_finish_if_exit_only(char *status,
                                                          size_t status_size);
static bool queen_runtime_apply_special_section(int16_t special_section,
                                                char *status,
                                                size_t status_size);
static bool queen_runtime_apply_special_move(int16_t special_move,
                                             bool *applied,
                                             char *status,
                                             size_t status_size);
static bool queen_runtime_apply_cutaway_person_list(
    const struct queen_cutaway_object *object,
    bool *applied,
    char *status,
    size_t status_size);
static bool queen_runtime_apply_cutaway_person_object(
    const struct queen_cutaway_object *object,
    bool *applied,
    char *status,
    size_t status_size);
static bool queen_runtime_find_walk_off(uint16_t object_index,
                                        struct scummvm_queen_walk_off_data *walk_off,
                                        bool *found,
                                        char *status,
                                        size_t status_size);
static void queen_runtime_tick_joe(void);
static uint16_t queen_runtime_abs16(int16_t value);
static bool queen_runtime_find_dialog_string(uint16_t offset,
                                             int16_t id,
                                             int16_t max,
                                             uint32_t payload_size,
                                             char *text,
                                             size_t text_size);

static void queen_runtime_set_message(const char *text)
{
    if (!text)
        queen_runtime_message[0] = '\0';
    else
        rb->strlcpy(queen_runtime_message, text,
                    sizeof(queen_runtime_message));
}

static void queen_runtime_clear_dialog_options(void)
{
    rb->memset(queen_runtime_dialog_options, 0,
               sizeof(queen_runtime_dialog_options));
    rb->memset(queen_runtime_dialog_option_active, 0,
               sizeof(queen_runtime_dialog_option_active));
    rb->memset(queen_runtime_dialog_option_head, 0,
               sizeof(queen_runtime_dialog_option_head));
    rb->memset(queen_runtime_dialog_option_return, 0,
               sizeof(queen_runtime_dialog_option_return));
    rb->memset(queen_runtime_dialog_option_game_state_index, 0,
               sizeof(queen_runtime_dialog_option_game_state_index));
    rb->memset(queen_runtime_dialog_option_game_state_value, 0,
               sizeof(queen_runtime_dialog_option_game_state_value));
    queen_runtime_dialog_option_count = 0;
    queen_runtime_dialog_talk_slot = 0;
    queen_runtime_dialog_level_max = 0;
    queen_runtime_dialog_joe_ptr_off = 0;
    queen_runtime_dialog_person1_off = 0;
    queen_runtime_dialog_person2_off = 0;
    queen_runtime_dialog_cutaway_off = 0;
    queen_runtime_dialog_payload_size = 0;
    queen_runtime_dialog_joe_max = 0;
    queen_runtime_dialog_person_max = 0;
    rb->memset(queen_runtime_dialog_post_game_state, 0,
               sizeof(queen_runtime_dialog_post_game_state));
    rb->memset(queen_runtime_dialog_post_test_value, 0,
               sizeof(queen_runtime_dialog_post_test_value));
    rb->memset(queen_runtime_dialog_post_item, 0,
               sizeof(queen_runtime_dialog_post_item));
    queen_runtime_dialog_talk_slot_valid = false;
    queen_runtime_dialog_was_repeat = false;
    queen_runtime_dialog_file[0] = '\0';
}

static uint16_t read_le16_rt(const unsigned char *p)
{
    return ((uint16_t)p[1] << 8) | p[0];
}

static uint16_t read_be16_rt(const unsigned char *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static int16_t read_be16s_rt(const unsigned char *p)
{
    return (int16_t)read_be16_rt(p);
}

static int16_t queen_runtime_dialog_child_selected_value(int16_t head)
{
    uint16_t level;

    if (head <= 0)
        return 0;

    for (level = 0; level < queen_runtime_dialog_level_max; level++) {
        uint32_t base = 32 + (uint32_t)level * 96;

        if (base + 8 > sizeof(queen_script_header))
            break;
        if (read_be16s_rt(queen_script_header + base + 2) == head) {
            int16_t value =
                read_be16s_rt(queen_script_header + base + 6);

            if (value > 0)
                return value;
        }
    }

    return 0;
}

static bool queen_runtime_dialog_find_level(int16_t head, uint16_t *level)
{
    uint16_t index;

    if (head <= 0)
        return false;

    for (index = 0; index < queen_runtime_dialog_level_max; index++) {
        uint32_t base = 32 + (uint32_t)index * 96;

        if (base + 8 > queen_runtime_dialog_payload_size ||
            base + 8 > sizeof(queen_script_header))
            break;
        if (read_be16s_rt(queen_script_header + base + 2) == head) {
            *level = index;
            return true;
        }
    }

    return false;
}

static bool queen_runtime_dialog_find_level_containing(int16_t head,
                                                       uint16_t *level)
{
    uint16_t row;

    if (head <= 0)
        return false;

    for (row = 0; row < queen_runtime_dialog_level_max; row++) {
        uint16_t column;

        for (column = 0; column <= SCUMMVM_QUEEN_DIALOG_OPTIONS + 1;
             column++) {
            uint32_t base = 32 + (uint32_t)row * 96 +
                (uint32_t)column * 16;

            if (base + 4 > queen_runtime_dialog_payload_size ||
                base + 4 > sizeof(queen_script_header))
                return false;
            if (read_be16s_rt(queen_script_header + base + 2) == head) {
                *level = row;
                return true;
            }
        }
    }

    return false;
}

static void queen_runtime_dialog_apply_level_state(uint16_t level)
{
    uint32_t base = 32 + (uint32_t)level * 96;
    int16_t game_state_index;

    if (level >= queen_runtime_dialog_level_max ||
        base + 16 > queen_runtime_dialog_payload_size ||
        base + 16 > sizeof(queen_script_header))
        return;

    game_state_index = read_be16s_rt(queen_script_header + base + 10);
    if (game_state_index > 0 &&
        game_state_index < QUEEN_RUNTIME_GAME_STATE_COUNT) {
        queen_runtime_game_state[(uint16_t)game_state_index] =
            read_be16s_rt(queen_script_header + base + 14);
        queen_runtime_dirty = true;
    }
}

static void queen_runtime_dialog_clear_option_rows(void)
{
    rb->memset(queen_runtime_dialog_options, 0,
               sizeof(queen_runtime_dialog_options));
    rb->memset(queen_runtime_dialog_option_active, 0,
               sizeof(queen_runtime_dialog_option_active));
    rb->memset(queen_runtime_dialog_option_head, 0,
               sizeof(queen_runtime_dialog_option_head));
    rb->memset(queen_runtime_dialog_option_return, 0,
               sizeof(queen_runtime_dialog_option_return));
    rb->memset(queen_runtime_dialog_option_game_state_index, 0,
               sizeof(queen_runtime_dialog_option_game_state_index));
    rb->memset(queen_runtime_dialog_option_game_state_value, 0,
               sizeof(queen_runtime_dialog_option_game_state_value));
    queen_runtime_dialog_option_count = 0;
}

static bool queen_runtime_dialog_load_level_options(uint16_t level)
{
    uint16_t option;

    queen_runtime_dialog_clear_option_rows();
    if (level >= queen_runtime_dialog_level_max)
        return false;

    for (option = 1; option <= SCUMMVM_QUEEN_DIALOG_OPTIONS; option++) {
        uint32_t entry_off = 32 + (uint32_t)level * 96 +
            (uint32_t)option * 16;
        uint16_t option_index = (uint16_t)(option - 1);
        int16_t head;
        int16_t dialogue_return;
        int16_t game_state_index;
        int16_t game_state_value;
        char option_text[96];

        if (entry_off + 16 > queen_runtime_dialog_payload_size ||
            entry_off + 16 > sizeof(queen_script_header))
            break;

        head = read_be16s_rt(queen_script_header + entry_off + 2);
        dialogue_return =
            read_be16s_rt(queen_script_header + entry_off + 6);
        if (level == 0 && queen_runtime_dialog_talk_slot_valid) {
            int16_t selected =
                queen_runtime_talk_values[queen_runtime_dialog_talk_slot]
                                         [option - 1];

            if (selected > 0)
                head = selected;
            else if (selected == -1)
                continue;
        }

        game_state_index =
            read_be16s_rt(queen_script_header + entry_off + 10);
        game_state_value =
            read_be16s_rt(queen_script_header + entry_off + 14);
        if (game_state_index < 0) {
            uint16_t slot = queen_runtime_abs16(game_state_index);

            if (slot >= QUEEN_RUNTIME_GAME_STATE_COUNT ||
                queen_runtime_game_state[slot] != game_state_value)
                continue;
        }

        if (queen_runtime_find_dialog_string(
                queen_runtime_dialog_joe_ptr_off, head,
                queen_runtime_dialog_joe_max,
                queen_runtime_dialog_payload_size, option_text,
                sizeof(option_text)) &&
            option_text[0] != '\0') {
            rb->strlcpy(queen_runtime_dialog_options[option_index],
                        option_text,
                        sizeof(queen_runtime_dialog_options[option_index]));
            queen_runtime_dialog_option_active[option_index] = true;
            queen_runtime_dialog_option_head[option_index] = head;
            queen_runtime_dialog_option_return[option_index] =
                dialogue_return;
            queen_runtime_dialog_option_game_state_index[option_index] =
                game_state_index;
            queen_runtime_dialog_option_game_state_value[option_index] =
                game_state_value;
            queen_runtime_dialog_option_count++;
        }
    }

    return queen_runtime_dialog_option_count > 0;
}

static uint16_t queen_runtime_abs16(int16_t value)
{
    if (value < 0)
        return (uint16_t)-value;

    return (uint16_t)value;
}

static uint16_t queen_runtime_default_verb(uint16_t object_state)
{
    static const uint16_t verbs[] = {
        0, 1, 0, 2,
        0, 0, 9, 3,
        5, 8, 0, 0,
        6, 0, 7, 0
    };

    return verbs[(object_state >> 4) & 0x0f];
}

static const char *queen_runtime_verb_name(uint16_t verb)
{
    switch (verb) {
    case QUEEN_VERB_OPEN:
        return "Open";
    case QUEEN_VERB_CLOSE:
        return "Close";
    case QUEEN_VERB_MOVE:
        return "Move";
    case QUEEN_VERB_GIVE:
        return "Give";
    case QUEEN_VERB_USE:
        return "Use";
    case QUEEN_VERB_PICK_UP:
        return "Pick up";
    case QUEEN_VERB_TALK_TO:
        return "Talk to";
    case QUEEN_VERB_LOOK_AT:
        return "Look at";
    case QUEEN_VERB_WALK_TO:
        return "Walk to";
    default:
        return "Default";
    }
}

static uint16_t queen_runtime_cycle_verb_value(uint16_t current,
                                               int direction)
{
    static const uint16_t verbs[] = {
        QUEEN_VERB_NONE,
        QUEEN_VERB_LOOK_AT,
        QUEEN_VERB_TALK_TO,
        QUEEN_VERB_PICK_UP,
        QUEEN_VERB_USE,
        QUEEN_VERB_GIVE,
        QUEEN_VERB_OPEN,
        QUEEN_VERB_CLOSE,
        QUEEN_VERB_MOVE,
        QUEEN_VERB_WALK_TO
    };
    uint16_t index;

    for (index = 0; index < ARRAYLEN(verbs); index++) {
        if (verbs[index] == current)
            break;
    }
    if (index >= ARRAYLEN(verbs))
        index = 0;

    if (direction < 0)
        index = index == 0 ? (uint16_t)(ARRAYLEN(verbs) - 1) :
            (uint16_t)(index - 1);
    else
        index = (index + 1) % ARRAYLEN(verbs);

    return verbs[index];
}

static bool queen_runtime_verb_is_selectable(uint16_t verb)
{
    switch (verb) {
    case QUEEN_VERB_NONE:
    case QUEEN_VERB_OPEN:
    case QUEEN_VERB_CLOSE:
    case QUEEN_VERB_MOVE:
    case QUEEN_VERB_GIVE:
    case QUEEN_VERB_USE:
    case QUEEN_VERB_PICK_UP:
    case QUEEN_VERB_TALK_TO:
    case QUEEN_VERB_LOOK_AT:
    case QUEEN_VERB_WALK_TO:
        return true;
    default:
        return false;
    }
}

static bool queen_runtime_state_is_on(uint16_t state)
{
    return (state & (1 << 8)) != 0;
}

static void queen_runtime_state_set_on(uint16_t *state, bool on)
{
    if (on)
        *state |= (1 << 8);
    else
        *state &= ~(1 << 8);
}

static void queen_runtime_state_set_default_verb(uint16_t *state,
                                                 uint16_t verb)
{
    uint16_t value = 0;

    switch (verb) {
    case QUEEN_VERB_OPEN:
        value = 1;
        break;
    case QUEEN_VERB_CLOSE:
        value = 3;
        break;
    case QUEEN_VERB_MOVE:
        value = 7;
        break;
    default:
        value = 0;
        break;
    }

    *state = (*state & ~0x00f0) | (value << 4);
}

static uint16_t queen_runtime_find_direction(uint16_t state)
{
    static const uint16_t directions[] = {
        QUEEN_DIR_BACK,
        QUEEN_DIR_RIGHT,
        QUEEN_DIR_LEFT,
        QUEEN_DIR_FRONT
    };

    return directions[(state >> 2) & 0x03];
}

static uint16_t queen_runtime_opposite_direction(uint16_t direction)
{
    switch (direction) {
    case QUEEN_DIR_BACK:
        return QUEEN_DIR_FRONT;
    case QUEEN_DIR_FRONT:
        return QUEEN_DIR_BACK;
    case QUEEN_DIR_LEFT:
        return QUEEN_DIR_RIGHT;
    case QUEEN_DIR_RIGHT:
        return QUEEN_DIR_LEFT;
    default:
        return QUEEN_DIR_FRONT;
    }
}

static void queen_runtime_set_joe_target(uint16_t x,
                                         uint16_t y,
                                         uint16_t facing)
{
    queen_runtime_joe_target_x = (int16_t)x;
    queen_runtime_joe_target_y = (int16_t)y;
    queen_runtime_joe_facing = facing;
    if ((queen_runtime_joe_x == 0 && queen_runtime_joe_y == 0) ||
        (queen_runtime_joe_x == queen_runtime_joe_target_x &&
         queen_runtime_joe_y == queen_runtime_joe_target_y)) {
        queen_runtime_joe_x = queen_runtime_joe_target_x;
        queen_runtime_joe_y = queen_runtime_joe_target_y;
        queen_runtime_joe_walking = false;
        queen_runtime_joe_walk_step = 0;
        queen_runtime_joe_walk_steps = 0;
    } else {
        int dx = queen_runtime_joe_target_x - queen_runtime_joe_x;
        int dy = queen_runtime_joe_target_y - queen_runtime_joe_y;
        int distance = queen_runtime_abs16((int16_t)dx) +
                       queen_runtime_abs16((int16_t)dy);

        queen_runtime_joe_start_x = queen_runtime_joe_x;
        queen_runtime_joe_start_y = queen_runtime_joe_y;
        queen_runtime_joe_walk_steps = (uint16_t)(distance / 10);
        if (queen_runtime_joe_walk_steps < 1)
            queen_runtime_joe_walk_steps = 1;
        if (queen_runtime_joe_walk_steps > 32)
            queen_runtime_joe_walk_steps = 32;
        queen_runtime_joe_walk_step = 0;
        queen_runtime_joe_walking = true;
        queen_runtime_scene_has_animation = true;
    }
    queen_runtime_dirty = true;
}

static bool queen_runtime_item_visible(uint16_t item)
{
    return item > 0 && item <= queen_runtime_jas.items &&
           queen_runtime_items[item].name > 0;
}

static uint16_t queen_runtime_next_inventory_item(uint16_t first)
{
    uint16_t index;

    for (index = first + 1; index <= queen_runtime_jas.items; index++)
        if (queen_runtime_item_visible(index))
            return index;
    for (index = 1; index <= first && index <= queen_runtime_jas.items; index++)
        if (queen_runtime_item_visible(index))
            return index;

    return 0;
}

static uint16_t queen_runtime_previous_inventory_item(uint16_t first)
{
    uint16_t index;

    if (queen_runtime_jas.items == 0)
        return 0;
    if (first == 0 || first > queen_runtime_jas.items)
        first = 1;

    for (index = first - 1; index > 0; index--)
        if (queen_runtime_item_visible(index))
            return index;
    for (index = queen_runtime_jas.items; index >= first; index--) {
        if (queen_runtime_item_visible(index))
            return index;
        if (index == 1)
            break;
    }

    return 0;
}

static void queen_runtime_remove_duplicate_inventory_slots(void)
{
    uint16_t i;
    uint16_t j;

    for (i = 0; i < SCUMMVM_QUEEN_INVENTORY_SLOTS; i++) {
        if (queen_runtime_inventory_slots[i] == 0)
            continue;
        for (j = i + 1; j < SCUMMVM_QUEEN_INVENTORY_SLOTS; j++) {
            if (queen_runtime_inventory_slots[j] ==
                queen_runtime_inventory_slots[i])
                queen_runtime_inventory_slots[j] = 0;
        }
    }
}

static void queen_runtime_refresh_inventory_slots(uint16_t first)
{
    uint16_t index;
    uint16_t item = first;

    rb->memset(queen_runtime_inventory_slots, 0,
               sizeof(queen_runtime_inventory_slots));
    if (!queen_runtime_item_visible(item))
        item = queen_runtime_next_inventory_item(0);
    queen_runtime_inventory_page_first = item;

    for (index = 0; index < SCUMMVM_QUEEN_INVENTORY_SLOTS && item != 0;
         index++) {
        queen_runtime_inventory_slots[index] = item;
        item = queen_runtime_next_inventory_item(item);
        queen_runtime_remove_duplicate_inventory_slots();
    }
}

static bool queen_runtime_add_inventory_item(uint16_t item,
                                             char *status,
                                             size_t status_size)
{
    if (item == 0 || item > queen_runtime_jas.items) {
        rb->snprintf(status, status_size,
                     "Queen inventory item %u is invalid",
                     (unsigned)item);
        return false;
    }

    if (queen_runtime_items[item].name < 0)
        queen_runtime_items[item].name = -queen_runtime_items[item].name;
    queen_runtime_inventory[item] = true;
    queen_runtime_refresh_inventory_slots(item);
    queen_runtime_dirty = true;
    return true;
}

static bool queen_runtime_delete_inventory_item(uint16_t item,
                                                char *status,
                                                size_t status_size)
{
    if (item == 0 || item > queen_runtime_jas.items) {
        rb->snprintf(status, status_size,
                     "Queen inventory item %u is invalid",
                     (unsigned)item);
        return false;
    }

    if (queen_runtime_items[item].name > 0)
        queen_runtime_items[item].name = -queen_runtime_items[item].name;
    queen_runtime_inventory[item] = false;
    queen_runtime_refresh_inventory_slots(item);
    queen_runtime_dirty = true;
    return true;
}

static bool queen_runtime_bbk_load(struct queen_bbk_bank *bank,
                                   const struct scummvm_target *target,
                                   const char *filename,
                                   uint32_t size,
                                   char *status,
                                   size_t status_size)
{
    uint32_t resource_size = 0;
    uint32_t offset;
    uint16_t frame;

    if (bank->loaded && !rb->strcasecmp(bank->name, filename))
        return true;

    if (size > bank->capacity) {
        rb->snprintf(status, status_size,
                     "Queen BBK %s is too large", filename);
        return false;
    }

    if (!scummvm_queen_loader_read_resource(target, filename, 0,
                                            bank->data, size,
                                            &resource_size, status,
                                            status_size))
        return false;
    if (resource_size < 2) {
        rb->snprintf(status, status_size,
                     "Queen BBK %s is truncated", filename);
        return false;
    }

    bank->frame_count = read_le16_rt(bank->data);
    if (bank->frame_count >= QUEEN_BBK_MAX_FRAMES) {
        rb->snprintf(status, status_size,
                     "Queen BBK %s has too many frames", filename);
        return false;
    }

    rb->memset(bank->frames, 0, sizeof(bank->frames));
    offset = 2;
    for (frame = 1; frame <= bank->frame_count; frame++) {
        struct queen_bbk_frame *bf = &bank->frames[frame];
        uint32_t pixels;

        if (offset + 8 > resource_size) {
            rb->snprintf(status, status_size,
                         "Queen BBK %s frame table is truncated",
                         filename);
            return false;
        }

        bf->offset = offset;
        bf->width = read_le16_rt(bank->data + offset);
        bf->height = read_le16_rt(bank->data + offset + 2);
        bf->xhotspot = read_le16_rt(bank->data + offset + 4);
        bf->yhotspot = read_le16_rt(bank->data + offset + 6);

        pixels = (uint32_t)bf->width * (uint32_t)bf->height;
        if (pixels > resource_size - offset - 8) {
            rb->snprintf(status, status_size,
                         "Queen BBK %s frame %u is truncated",
                         filename, (unsigned)frame);
            return false;
        }
        offset += 8 + pixels;
    }

    rb->strlcpy(bank->name, filename, sizeof(bank->name));
    bank->loaded = true;
    rb->snprintf(status, status_size,
                 "Queen BBK %s loaded: %u frames",
                 filename, (unsigned)bank->frame_count);
    return true;
}

static void queen_runtime_draw_bbk_frame(const struct queen_bbk_bank *bank,
                                         struct scummvm_video *video,
                                         uint16_t frame,
                                         int x,
                                         int y,
                                         bool hotspot,
                                         bool xflip)
{
    const struct queen_bbk_frame *bf;
    const unsigned char *src;
    uint16_t row;

    if (!bank || !bank->loaded || !video ||
        frame == 0 || frame > bank->frame_count)
        return;

    bf = &bank->frames[frame];
    if (bf->width == 0 || bf->height == 0)
        return;

    if (hotspot) {
        x -= bf->xhotspot;
        y -= bf->yhotspot;
    }

    src = bank->data + bf->offset + 8;
    for (row = 0; row < bf->height; row++) {
        int dst_y = y + row;
        uint16_t col;

        if (dst_y < 0 || dst_y >= video->height) {
            src += bf->width;
            continue;
        }

        for (col = 0; col < bf->width; col++) {
            int dst_x = x + col;
            uint16_t src_col = xflip ? (uint16_t)(bf->width - col - 1) : col;
            unsigned char color = src[src_col];

            if (color != 0 && dst_x >= 0 && dst_x < video->width)
                video->pixels[dst_y * video->width + dst_x] =
                    queen_pcx_palette[color];
        }
        src += bf->width;
    }
}

static bool queen_runtime_draw_graphic(const struct queen_bbk_bank *bank,
                                       struct scummvm_video *video,
                                       uint16_t graphic_index,
                                       bool hotspot,
                                       char *status,
                                       size_t status_size)
{
    struct scummvm_queen_graphic_data graphic;
    int16_t last_frame;
    uint16_t frame;

    if (graphic_index == 0 || graphic_index > queen_runtime_jas.graphics)
        return true;

    if (!scummvm_queen_loader_read_graphic(&queen_runtime_target,
                                           &queen_runtime_jas,
                                           graphic_index, &graphic,
                                           status, status_size))
        return false;

    frame = (uint16_t)graphic.first_frame;
    last_frame = graphic.last_frame;
    if (last_frame < 0)
        last_frame = -last_frame;
    if (graphic.first_frame > 0 && last_frame > graphic.first_frame) {
        uint16_t count = (uint16_t)(last_frame - graphic.first_frame + 1);
        uint16_t delay = graphic.speed / 4;
        uint16_t phase;

        if (delay == 0)
            delay = 1;
        phase = queen_runtime_anim_tick / delay;
        queen_runtime_scene_has_animation = true;
        frame = (uint16_t)(graphic.first_frame + (phase % count));
    }

    if (graphic.first_frame > 0)
        queen_runtime_draw_bbk_frame(bank, video, frame, graphic.x,
                                     graphic.y, hotspot, false);

    return true;
}

static void queen_runtime_draw_graphic_data(
    const struct queen_bbk_bank *bank,
    struct scummvm_video *video,
    const struct scummvm_queen_graphic_data *graphic,
    bool hotspot,
    bool xflip,
    uint16_t frame_override)
{
    int16_t last_frame;
    uint16_t frame;

    frame = frame_override ? frame_override : (uint16_t)graphic->first_frame;
    last_frame = graphic->last_frame;
    if (last_frame < 0)
        last_frame = -last_frame;
    if (frame_override == 0 &&
        graphic->first_frame > 0 && last_frame > graphic->first_frame) {
        uint16_t count = (uint16_t)(last_frame - graphic->first_frame + 1);
        uint16_t delay = graphic->speed / 4;
        uint16_t phase;

        if (delay == 0)
            delay = 1;
        phase = queen_runtime_anim_tick / delay;
        queen_runtime_scene_has_animation = true;
        frame = (uint16_t)(graphic->first_frame + (phase % count));
    }

    if (frame != 0)
        queen_runtime_draw_bbk_frame(bank, video, frame, graphic->x,
                                     graphic->y, hotspot, xflip);
}

static int queen_runtime_compare_render_items(const void *a, const void *b)
{
    const struct queen_render_item *left = a;
    const struct queen_render_item *right = b;
    int diff = (int)left->graphic.y - (int)right->graphic.y;

    if (diff == 0)
        diff = (int)left->order - (int)right->order;
    return diff;
}

static bool queen_runtime_queue_graphic(
    struct queen_render_item *items,
    uint16_t *count,
    uint16_t graphic_index,
    uint16_t order,
    char *status,
    size_t status_size)
{
    struct scummvm_queen_graphic_data graphic;

    if (graphic_index == 0 || graphic_index > queen_runtime_jas.graphics)
        return true;
    if (*count >= QUEEN_RUNTIME_MAX_RENDER_ITEMS) {
        rb->strlcpy(status, "Queen render queue is full", status_size);
        return false;
    }
    if (!scummvm_queen_loader_read_graphic(&queen_runtime_target,
                                           &queen_runtime_jas,
                                           graphic_index, &graphic,
                                           status, status_size))
        return false;
    if (graphic.first_frame <= 0)
        return true;

    items[*count].graphic = graphic;
    items[*count].bank = &queen_room_bbk;
    items[*count].frame_override = order <= queen_runtime_jas.objects ?
        queen_runtime_object_frame_override[order] : 0;
    items[*count].order = order;
    items[*count].hotspot = true;
    items[*count].xflip = order <= queen_runtime_jas.objects ?
        queen_runtime_object_xflip[order] : false;
    (*count)++;
    return true;
}

static uint16_t queen_runtime_joe_standing_frame(void)
{
    switch (queen_runtime_joe_facing) {
    case QUEEN_DIR_FRONT:
        return 3;
    case QUEEN_DIR_BACK:
        return 5;
    case QUEEN_DIR_LEFT:
    case QUEEN_DIR_RIGHT:
    default:
        return 1;
    }
}

static uint16_t queen_runtime_joe_walk_frame(void)
{
    uint16_t phase = queen_runtime_joe_walk_step & 7;

    switch (queen_runtime_joe_facing) {
    case QUEEN_DIR_FRONT:
        return (uint16_t)(9 + (phase % 6));
    case QUEEN_DIR_BACK:
        return (uint16_t)(15 + (phase % 6));
    case QUEEN_DIR_LEFT:
    case QUEEN_DIR_RIGHT:
    default:
        return (uint16_t)(1 + phase);
    }
}

static bool queen_runtime_load_joe_bank(const struct queen_bbk_bank **bank,
                                        uint16_t *frame,
                                        char *status,
                                        size_t status_size)
{
    struct scummvm_queen_resource_info resource;
    uint32_t entries = 0;
    int16_t dressing_mode =
        queen_runtime_game_state[QUEEN_GAME_STATE_JOE_DRESSING_MODE];
    const char *name;
    struct queen_bbk_bank *target = queen_runtime_joe_walking ?
        &queen_joe_anim_bbk : &queen_joe_bbk;

    if (dressing_mode == 1)
        name = queen_runtime_joe_walking ? "JOEU_A.BBK" : "JOEU_B.BBK";
    else if (dressing_mode == 2)
        name = queen_runtime_joe_walking ? "JOED_A.BBK" : "JOED_B.BBK";
    else
        name = queen_runtime_joe_walking ? "JOE_A.BBK" : "JOE_B.BBK";

    if (!scummvm_queen_loader_resource_info(&queen_runtime_target,
                                            name, &resource, &entries,
                                            status, status_size))
        return false;
    if (!queen_runtime_bbk_load(target, &queen_runtime_target,
                                name, resource.size, status,
                                status_size))
        return false;

    *bank = target;
    *frame = queen_runtime_joe_walking ?
        queen_runtime_joe_walk_frame() : queen_runtime_joe_standing_frame();
    return true;
}

static bool queen_runtime_queue_joe(struct queen_render_item *items,
                                    uint16_t *count,
                                    char *status,
                                    size_t status_size)
{
    const struct queen_bbk_bank *bank = NULL;
    uint16_t frame = 0;

    if (queen_runtime_joe_x == 0 && queen_runtime_joe_y == 0)
        return true;
    if (*count >= QUEEN_RUNTIME_MAX_RENDER_ITEMS) {
        rb->strlcpy(status, "Queen render queue is full", status_size);
        return false;
    }
    if (!queen_runtime_load_joe_bank(&bank, &frame, status, status_size))
        return false;

    rb->memset(&items[*count].graphic, 0, sizeof(items[*count].graphic));
    items[*count].graphic.x = (uint16_t)queen_runtime_joe_x;
    items[*count].graphic.y = (uint16_t)queen_runtime_joe_y;
    items[*count].bank = bank;
    items[*count].frame_override = frame;
    items[*count].order = 0;
    items[*count].hotspot = true;
    items[*count].xflip = queen_runtime_joe_facing == QUEEN_DIR_LEFT;
    (*count)++;
    return true;
}

static bool queen_runtime_actor_is_visible(
    const struct scummvm_queen_actor_data *actor)
{
    uint16_t slot;

    if (actor->room != (int16_t)queen_runtime_current_room)
        return false;
    if (actor->game_state_slot < 0)
        return false;

    slot = (uint16_t)actor->game_state_slot;
    if (slot >= QUEEN_RUNTIME_GAME_STATE_COUNT)
        return false;

    return queen_runtime_game_state[slot] == actor->game_state_value;
}

static bool queen_runtime_draw_actor(
    const struct scummvm_queen_actor_data *actor,
    char *status,
    size_t status_size)
{
    const struct queen_bbk_bank *bank = &queen_room_bbk;

    if (actor->file != 0) {
        char bank_name[32];
        struct scummvm_queen_resource_info resource;
        uint32_t entries = 0;

        if (!scummvm_queen_loader_read_text_line(
                &queen_runtime_target,
                queen_runtime_text.actor_file_offset + actor->file - 1,
                bank_name, sizeof(bank_name), status, status_size))
            return false;
        if (!scummvm_queen_loader_resource_info(&queen_runtime_target,
                                                bank_name, &resource,
                                                &entries, status,
                                                status_size))
            return false;
        if (!queen_runtime_bbk_load(&queen_actor_bbk,
                                    &queen_runtime_target, bank_name,
                                    resource.size, status, status_size))
            return false;
        bank = &queen_actor_bbk;
    }

    queen_runtime_draw_bbk_frame(bank, queen_runtime_video,
                                 actor->standing_frame,
                                 actor->x, actor->y, true, false);
    return true;
}

static bool queen_runtime_redraw_scene(char *status, size_t status_size)
{
    struct scummvm_queen_room_asset_info assets;
    struct queen_render_item render_items[QUEEN_RUNTIME_MAX_RENDER_ITEMS];
    uint32_t resource_size = 0;
    uint16_t render_count = 0;
    uint16_t index;

    if (!queen_runtime_video)
        return true;

    if (!scummvm_queen_loader_room_assets(&queen_runtime_target,
                                          &queen_runtime_jas,
                                          &queen_runtime_text,
                                          queen_runtime_current_room,
                                          &assets, status, status_size))
        return false;
    if (rb->strlen(assets.backdrop_name) < 4 ||
        rb->strcasecmp(assets.backdrop_name +
                       rb->strlen(assets.backdrop_name) - 4, ".PCX")) {
        rb->snprintf(status, status_size,
                     "Queen backdrop %s is not PCX",
                     assets.backdrop_name);
        return false;
    }
    if (assets.backdrop_size > sizeof(queen_pcx_data)) {
        rb->snprintf(status, status_size,
                     "Queen backdrop %s is too large",
                     assets.backdrop_name);
        return false;
    }
    if (!scummvm_queen_loader_read_resource(&queen_runtime_target,
                                            assets.backdrop_name, 0,
                                            queen_pcx_data,
                                            assets.backdrop_size,
                                            &resource_size, status,
                                            status_size))
        return false;
    if (!queen_runtime_decode_pcx(queen_runtime_video,
                                  assets.backdrop_name, resource_size,
                                  status, status_size))
        return false;

    queen_runtime_scene_has_animation = false;

    for (index = 1; index <= queen_runtime_jas.furniture; index++) {
        struct scummvm_queen_furniture_data furniture;
        uint16_t graphic_index;

        if (!scummvm_queen_loader_read_furniture(&queen_runtime_target,
                                                 &queen_runtime_jas,
                                                 index, &furniture,
                                                 status, status_size))
            return false;
        if (furniture.room != (int16_t)queen_runtime_current_room ||
            furniture.object_number <= 0)
            continue;

        graphic_index = queen_runtime_abs16(furniture.object_number);
        if (furniture.object_number > 5000) {
            graphic_index -= 5000;
            if (!queen_runtime_draw_graphic(&queen_room_bbk,
                                            queen_runtime_video,
                                            graphic_index, false,
                                            status, status_size))
                return false;
        } else {
            if (!queen_runtime_queue_graphic(render_items, &render_count,
                                             graphic_index, index,
                                             status, status_size))
                return false;
        }
    }

    for (index = queen_runtime_range.first_object;
         queen_runtime_range.first_object <= queen_runtime_range.last_object &&
         index <= queen_runtime_range.last_object;
         index++) {
        struct scummvm_queen_object_data *object;
        uint16_t graphic_index;

        object = &queen_runtime_objects[index];
        if (object->name <= 0 || object->image <= 0)
            continue;

        graphic_index = (uint16_t)object->image;
        if (graphic_index > 5000) {
            graphic_index -= 5000;
            if (!queen_runtime_draw_graphic(&queen_room_bbk,
                                            queen_runtime_video,
                                            graphic_index, false,
                                            status, status_size))
                return false;
        } else {
            if (!queen_runtime_queue_graphic(render_items, &render_count,
                                             graphic_index, index,
                                             status, status_size))
                return false;
        }
    }

    rb->qsort(render_items, render_count, sizeof(render_items[0]),
              queen_runtime_compare_render_items);
    if (!queen_runtime_queue_joe(render_items, &render_count, status,
                                 status_size))
        return false;
    rb->qsort(render_items, render_count, sizeof(render_items[0]),
              queen_runtime_compare_render_items);
    for (index = 0; index < render_count; index++) {
        queen_runtime_draw_graphic_data(render_items[index].bank,
                                        queen_runtime_video,
                                        &render_items[index].graphic,
                                        render_items[index].hotspot,
                                        render_items[index].xflip,
                                        render_items[index].frame_override);
    }

    for (index = 1; index <= queen_runtime_jas.actors; index++) {
        struct scummvm_queen_actor_data actor;

        if (!scummvm_queen_loader_read_actor(&queen_runtime_target,
                                             &queen_runtime_jas, index,
                                             &actor, status, status_size))
            return false;
        if (queen_runtime_actor_is_visible(&actor) &&
            !queen_runtime_draw_actor(&actor, status, status_size))
            return false;
    }

    rb->strlcpy(status, "Queen scene rendered", status_size);
    return true;
}

static bool queen_runtime_load_room(char *status, size_t status_size)
{
    struct scummvm_queen_room_asset_info assets;

    if (!scummvm_queen_loader_room_range(&queen_runtime_target,
                                         &queen_runtime_jas,
                                         queen_runtime_current_room,
                                         &queen_runtime_range,
                                         status, status_size))
        return false;
    if (!scummvm_queen_loader_room_assets(&queen_runtime_target,
                                          &queen_runtime_jas,
                                          &queen_runtime_text,
                                          queen_runtime_current_room,
                                          &assets, status, status_size))
        return false;
    if (!queen_runtime_bbk_load(&queen_room_bbk, &queen_runtime_target,
                                assets.bank_name, assets.bank_size,
                                status, status_size))
        return false;
    if (!queen_runtime_load_room_areas(status, status_size))
        return false;
    if (!queen_runtime_redraw_scene(status, status_size))
        return false;

    queen_runtime_selected_object = 0;
    rb->snprintf(status, status_size, "Queen room %u loaded",
                 (unsigned)queen_runtime_current_room);
    return true;
}

static void queen_runtime_try_intro_cutaway(const char *filename,
                                            char *status,
                                            size_t status_size)
{
    char scratch[96];

    if (!queen_runtime_probe_script_resource_chained(
            filename, false, scratch, sizeof(scratch)))
        rb->snprintf(status, status_size,
                     "Queen intro skipped %.16s: %.56s",
                     filename, scratch);
    else
        rb->strlcpy(status, scratch, status_size);
}

static bool queen_runtime_startup_room(char *status, size_t status_size)
{
    if (queen_runtime_current_room != QUEEN_ROOM_FOTAQ_LOGO ||
        queen_runtime_game_state[QUEEN_GAME_STATE_INTRO_PLAYED] != 0)
        return queen_runtime_load_room(status, status_size);

    if (!queen_runtime_load_room(status, status_size))
        return false;

    queen_runtime_try_intro_cutaway("COPY.CUT", status, status_size);
    queen_runtime_try_intro_cutaway("CLOGO.CUT", status, status_size);
    queen_runtime_try_intro_cutaway("CDINT.CUT", status, status_size);
    queen_runtime_try_intro_cutaway("CRED.CUT", status, status_size);

    queen_runtime_current_room = QUEEN_ROOM_HOTEL_LOBBY;
    queen_runtime_game_state[QUEEN_GAME_STATE_INTRO_PLAYED] = 1;
    queen_runtime_dirty = true;

    if (!queen_runtime_startup_room(status, status_size))
        return false;

    if (QUEEN_ENTRY_HOTEL_LOBBY <= queen_runtime_jas.objects &&
        (queen_runtime_objects[QUEEN_ENTRY_HOTEL_LOBBY].x != 0 ||
         queen_runtime_objects[QUEEN_ENTRY_HOTEL_LOBBY].y != 0)) {
        queen_runtime_set_joe_target(
            queen_runtime_objects[QUEEN_ENTRY_HOTEL_LOBBY].x,
            queen_runtime_objects[QUEEN_ENTRY_HOTEL_LOBBY].y,
            queen_runtime_opposite_direction(
                queen_runtime_find_direction(
                    queen_runtime_objects[QUEEN_ENTRY_HOTEL_LOBBY].state)));
    }

    queen_runtime_try_intro_cutaway("C70D.CUT", status, status_size);
    if (queen_runtime_current_room != QUEEN_ROOM_HOTEL_LOBBY) {
        queen_runtime_current_room = QUEEN_ROOM_HOTEL_LOBBY;
        if (!queen_runtime_load_room(status, status_size))
            return false;
    }
    if (!queen_runtime_redraw_scene(status, status_size))
        return false;

    rb->snprintf(status, status_size,
                 "Queen startup complete: room %u",
                 (unsigned)queen_runtime_current_room);
    queen_runtime_set_message(status);
    return true;
}

static bool queen_runtime_load_state(char *status, size_t status_size)
{
    uint16_t index;

    if (queen_runtime_jas.objects > QUEEN_RUNTIME_MAX_OBJECTS ||
        queen_runtime_jas.items > QUEEN_RUNTIME_MAX_ITEMS ||
        queen_runtime_jas.object_descriptions >
            QUEEN_RUNTIME_MAX_OBJECT_DESCRIPTIONS) {
        rb->snprintf(status, status_size,
                     "Queen runtime tables are too large");
        return false;
    }

    rb->memset(queen_runtime_objects, 0, sizeof(queen_runtime_objects));
    rb->memset(queen_runtime_object_frame_override, 0,
               sizeof(queen_runtime_object_frame_override));
    rb->memset(queen_runtime_object_xflip, 0,
               sizeof(queen_runtime_object_xflip));
    rb->memset(queen_runtime_items, 0, sizeof(queen_runtime_items));
    rb->memset(queen_runtime_object_descriptions, 0,
               sizeof(queen_runtime_object_descriptions));
    rb->memset(queen_runtime_game_state, 0,
               sizeof(queen_runtime_game_state));
    rb->memset(queen_runtime_inventory, 0,
               sizeof(queen_runtime_inventory));
    rb->memset(queen_runtime_inventory_slots, 0,
               sizeof(queen_runtime_inventory_slots));
    queen_runtime_inventory_page_first = 0;
    queen_runtime_selected_inventory_item = 0;
    queen_runtime_selected_verb = QUEEN_VERB_NONE;
    rb->memset(queen_runtime_talked_to, 0, sizeof(queen_runtime_talked_to));
    rb->memset(queen_runtime_talk_values, 0,
               sizeof(queen_runtime_talk_values));
    rb->memset(queen_runtime_room_areas, 0,
               sizeof(queen_runtime_room_areas));
    queen_runtime_set_message(NULL);
    queen_runtime_clear_dialog_options();
    queen_runtime_room_area_count = 0;
    queen_runtime_joe_x = 0;
    queen_runtime_joe_y = 0;
    queen_runtime_joe_start_x = 0;
    queen_runtime_joe_start_y = 0;
    queen_runtime_joe_target_x = 0;
    queen_runtime_joe_target_y = 0;
    queen_runtime_joe_facing = QUEEN_DIR_FRONT;
    queen_runtime_joe_walk_step = 0;
    queen_runtime_joe_walk_steps = 0;
    queen_runtime_joe_walking = false;

    for (index = 1; index <= queen_runtime_jas.objects; index++) {
        if (!scummvm_queen_loader_read_object(&queen_runtime_target,
                                              &queen_runtime_jas,
                                              index,
                                              &queen_runtime_objects[index],
                                              status, status_size))
            return false;
    }

    for (index = 1; index <= queen_runtime_jas.items; index++) {
        if (!scummvm_queen_loader_read_item(&queen_runtime_target,
                                            &queen_runtime_jas,
                                            index,
                                            &queen_runtime_items[index],
                                            status, status_size))
            return false;
    }

    for (index = 1; index <= queen_runtime_jas.object_descriptions; index++) {
        if (!scummvm_queen_loader_read_object_description(
                &queen_runtime_target, &queen_runtime_jas, index,
                &queen_runtime_object_descriptions[index],
                status, status_size))
            return false;
    }

    queen_runtime_refresh_inventory_slots(1);
    rb->strlcpy(status, "Queen runtime state loaded", status_size);
    return true;
}

static bool queen_runtime_load_room_areas(char *status, size_t status_size)
{
    struct scummvm_queen_grid_room grid_room;
    uint16_t index;

    rb->memset(queen_runtime_room_areas, 0,
               sizeof(queen_runtime_room_areas));
    queen_runtime_room_area_count = 0;

    if (!scummvm_queen_loader_grid_room(&queen_runtime_target,
                                        &queen_runtime_jas,
                                        queen_runtime_current_room,
                                        &grid_room, status, status_size))
        return false;
    if (grid_room.area_max > QUEEN_RUNTIME_MAX_ROOM_AREAS) {
        rb->snprintf(status, status_size,
                     "Queen room has too many grid areas");
        return false;
    }

    queen_runtime_room_area_count = (uint16_t)grid_room.area_max;
    for (index = 1; index <= queen_runtime_room_area_count; index++) {
        if (!scummvm_queen_loader_read_grid_area(
                &queen_runtime_target, &queen_runtime_jas,
                queen_runtime_current_room, index,
                &queen_runtime_room_areas[index],
                status, status_size))
            return false;
    }

    return true;
}

static bool queen_runtime_save_name(char *name, size_t name_size)
{
    return rb->snprintf(name, name_size, "%s.queen.rqsv",
                        queen_runtime_target.gameid) < (int)name_size;
}

static bool queen_runtime_save_path(char *path, size_t path_size)
{
    char name[96];

    return queen_runtime_save_name(name, sizeof(name)) &&
           scummvm_make_path(path, path_size,
                             queen_runtime_target.savepath, name);
}

static bool queen_runtime_write_all(int fd, const void *buf, size_t size)
{
    const unsigned char *p = buf;

    while (size > 0) {
        ssize_t done = rb->write(fd, p, size);

        if (done <= 0)
            return false;
        p += done;
        size -= done;
    }

    return true;
}

static bool queen_runtime_read_all(int fd, void *buf, size_t size)
{
    unsigned char *p = buf;

    while (size > 0) {
        ssize_t done = rb->read(fd, p, size);

        if (done <= 0)
            return false;
        p += done;
        size -= done;
    }

    return true;
}

static bool queen_runtime_write_u16(int fd, uint16_t value)
{
    unsigned char buf[2];

    buf[0] = value & 0xff;
    buf[1] = value >> 8;
    return queen_runtime_write_all(fd, buf, sizeof(buf));
}

static bool queen_runtime_read_u16(int fd, uint16_t *value)
{
    unsigned char buf[2];

    if (!queen_runtime_read_all(fd, buf, sizeof(buf)))
        return false;
    *value = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    return true;
}

bool scummvm_queen_runtime_save(char *status, size_t status_size)
{
    char path[MAX_PATH];
    int fd;

    if (!queen_runtime_active || !queen_runtime_dirty)
        return true;

    if (!queen_runtime_save_path(path, sizeof(path))) {
        rb->strlcpy(status, "Queen save path is too long", status_size);
        return false;
    }

    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        rb->strlcpy(status, "Queen autosave open failed", status_size);
        return false;
    }

    if (!queen_runtime_write_all(fd, "RQSV", 4) ||
        !queen_runtime_write_u16(fd, QUEEN_RUNTIME_SAVE_VERSION) ||
        !queen_runtime_write_u16(fd, queen_runtime_jas.objects) ||
        !queen_runtime_write_u16(fd, queen_runtime_jas.items) ||
        !queen_runtime_write_u16(fd,
                                 queen_runtime_jas.object_descriptions) ||
        !queen_runtime_write_u16(fd, QUEEN_RUNTIME_GAME_STATE_COUNT) ||
        !queen_runtime_write_u16(fd, queen_runtime_room_area_count) ||
        !queen_runtime_write_u16(fd, queen_runtime_current_room) ||
        !queen_runtime_write_u16(fd, (uint16_t)queen_runtime_joe_x) ||
        !queen_runtime_write_u16(fd, (uint16_t)queen_runtime_joe_y) ||
        !queen_runtime_write_u16(fd, queen_runtime_joe_facing) ||
        !queen_runtime_write_u16(fd, queen_runtime_selected_verb) ||
        !queen_runtime_write_u16(fd, queen_runtime_inventory_page_first) ||
        !queen_runtime_write_all(fd, queen_runtime_inventory_slots,
                                 sizeof(queen_runtime_inventory_slots)) ||
        !queen_runtime_write_all(fd, queen_runtime_objects,
                                 (queen_runtime_jas.objects + 1) *
                                 sizeof(queen_runtime_objects[0])) ||
        !queen_runtime_write_all(
            fd, queen_runtime_object_frame_override,
            (queen_runtime_jas.objects + 1) *
            sizeof(queen_runtime_object_frame_override[0])) ||
        !queen_runtime_write_all(fd, queen_runtime_object_xflip,
                                 queen_runtime_jas.objects + 1) ||
        !queen_runtime_write_all(fd, queen_runtime_items,
                                 (queen_runtime_jas.items + 1) *
                                 sizeof(queen_runtime_items[0])) ||
        !queen_runtime_write_all(
            fd, queen_runtime_object_descriptions,
            (queen_runtime_jas.object_descriptions + 1) *
            sizeof(queen_runtime_object_descriptions[0])) ||
        !queen_runtime_write_all(fd, queen_runtime_game_state,
                                 sizeof(queen_runtime_game_state)) ||
        !queen_runtime_write_all(fd, queen_runtime_inventory,
                                 queen_runtime_jas.items + 1) ||
        !queen_runtime_write_all(fd, queen_runtime_room_areas,
                                 (queen_runtime_room_area_count + 1) *
                                 sizeof(queen_runtime_room_areas[0])) ||
        !queen_runtime_write_all(fd, queen_runtime_talked_to,
                                 sizeof(queen_runtime_talked_to)) ||
        !queen_runtime_write_all(fd, queen_runtime_talk_values,
                                 sizeof(queen_runtime_talk_values))) {
        rb->close(fd);
        rb->strlcpy(status, "Queen autosave write failed", status_size);
        return false;
    }

    rb->close(fd);
    queen_runtime_dirty = false;
    rb->strlcpy(status, "Queen autosave written", status_size);
    return true;
}

static bool queen_runtime_load_save(char *status, size_t status_size)
{
    char path[MAX_PATH];
    unsigned char magic[4];
    uint16_t version;
    uint16_t objects;
    uint16_t items;
    uint16_t object_descriptions;
    uint16_t game_state_count;
    uint16_t room_area_count;
    uint16_t current_room;
    uint16_t joe_x;
    uint16_t joe_y;
    uint16_t joe_facing;
    uint16_t selected_verb;
    uint16_t inventory_page_first;
    uint16_t saved_inventory_slots[SCUMMVM_QUEEN_INVENTORY_SLOTS];
    uint16_t previous_inventory_slots[SCUMMVM_QUEEN_INVENTORY_SLOTS];
    uint16_t previous_inventory_page_first;
    uint16_t previous_room;
    int16_t previous_joe_x;
    int16_t previous_joe_y;
    uint16_t previous_joe_facing;
    uint16_t previous_selected_verb;
    uint16_t previous_frame_override[QUEEN_RUNTIME_MAX_OBJECTS + 1];
    bool previous_object_xflip[QUEEN_RUNTIME_MAX_OBJECTS + 1];
    bool previous_talked_to[QUEEN_RUNTIME_TALK_SELECTED_COUNT];
    int16_t previous_talk_values[QUEEN_RUNTIME_TALK_SELECTED_COUNT][4];
    int fd;

    if (!queen_runtime_save_path(path, sizeof(path)))
        return true;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return true;

    if (!queen_runtime_read_all(fd, magic, sizeof(magic)) ||
        rb->memcmp(magic, "RQSV", 4) ||
        !queen_runtime_read_u16(fd, &version) ||
        !queen_runtime_read_u16(fd, &objects) ||
        !queen_runtime_read_u16(fd, &items) ||
        !queen_runtime_read_u16(fd, &object_descriptions) ||
        !queen_runtime_read_u16(fd, &game_state_count) ||
        !queen_runtime_read_u16(fd, &room_area_count) ||
        !queen_runtime_read_u16(fd, &current_room) ||
        !queen_runtime_read_u16(fd, &joe_x) ||
        !queen_runtime_read_u16(fd, &joe_y) ||
        !queen_runtime_read_u16(fd, &joe_facing) ||
        !queen_runtime_read_u16(fd, &selected_verb) ||
        !queen_runtime_read_u16(fd, &inventory_page_first) ||
        !queen_runtime_read_all(fd, saved_inventory_slots,
                                sizeof(saved_inventory_slots)) ||
        version != QUEEN_RUNTIME_SAVE_VERSION ||
        objects != queen_runtime_jas.objects ||
        items != queen_runtime_jas.items ||
        object_descriptions != queen_runtime_jas.object_descriptions ||
        game_state_count != QUEEN_RUNTIME_GAME_STATE_COUNT ||
        current_room == 0 ||
        current_room > queen_runtime_jas.rooms ||
        joe_facing < QUEEN_DIR_LEFT ||
        joe_facing > QUEEN_DIR_BACK ||
        !queen_runtime_verb_is_selectable(selected_verb) ||
        inventory_page_first > queen_runtime_jas.items) {
        rb->close(fd);
        rb->strlcpy(status, "Queen autosave ignored", status_size);
        return true;
    }

    previous_room = queen_runtime_current_room;
    previous_joe_x = queen_runtime_joe_x;
    previous_joe_y = queen_runtime_joe_y;
    previous_joe_facing = queen_runtime_joe_facing;
    previous_selected_verb = queen_runtime_selected_verb;
    previous_inventory_page_first = queen_runtime_inventory_page_first;
    rb->memcpy(previous_inventory_slots, queen_runtime_inventory_slots,
               sizeof(previous_inventory_slots));
    rb->memcpy(previous_frame_override, queen_runtime_object_frame_override,
               sizeof(previous_frame_override));
    rb->memcpy(previous_object_xflip, queen_runtime_object_xflip,
               sizeof(previous_object_xflip));
    rb->memcpy(previous_talked_to, queen_runtime_talked_to,
               sizeof(previous_talked_to));
    rb->memcpy(previous_talk_values, queen_runtime_talk_values,
               sizeof(previous_talk_values));
    queen_runtime_current_room = current_room;
    queen_runtime_joe_x = (int16_t)joe_x;
    queen_runtime_joe_y = (int16_t)joe_y;
    queen_runtime_joe_start_x = queen_runtime_joe_x;
    queen_runtime_joe_start_y = queen_runtime_joe_y;
    queen_runtime_joe_target_x = queen_runtime_joe_x;
    queen_runtime_joe_target_y = queen_runtime_joe_y;
    queen_runtime_joe_facing = joe_facing;
    queen_runtime_joe_walk_step = 0;
    queen_runtime_joe_walk_steps = 0;
    queen_runtime_joe_walking = false;
    queen_runtime_selected_verb = selected_verb;
    queen_runtime_inventory_page_first = inventory_page_first;
    rb->memcpy(queen_runtime_inventory_slots, saved_inventory_slots,
               sizeof(queen_runtime_inventory_slots));
    if (!queen_runtime_load_room_areas(status, status_size) ||
        room_area_count != queen_runtime_room_area_count ||
        !queen_runtime_read_all(fd, queen_runtime_objects,
                                (objects + 1) *
                                sizeof(queen_runtime_objects[0])) ||
        !queen_runtime_read_all(
            fd, queen_runtime_object_frame_override,
            (objects + 1) *
            sizeof(queen_runtime_object_frame_override[0])) ||
        !queen_runtime_read_all(fd, queen_runtime_object_xflip,
                                objects + 1) ||
        !queen_runtime_read_all(fd, queen_runtime_items,
                                (items + 1) *
                                sizeof(queen_runtime_items[0])) ||
        !queen_runtime_read_all(
            fd, queen_runtime_object_descriptions,
            (object_descriptions + 1) *
            sizeof(queen_runtime_object_descriptions[0])) ||
        !queen_runtime_read_all(fd, queen_runtime_game_state,
                                sizeof(queen_runtime_game_state)) ||
        !queen_runtime_read_all(fd, queen_runtime_inventory, items + 1) ||
        !queen_runtime_read_all(fd, queen_runtime_room_areas,
                                (room_area_count + 1) *
                                sizeof(queen_runtime_room_areas[0])) ||
        !queen_runtime_read_all(fd, queen_runtime_talked_to,
                                sizeof(queen_runtime_talked_to)) ||
        !queen_runtime_read_all(fd, queen_runtime_talk_values,
                                sizeof(queen_runtime_talk_values))) {
        queen_runtime_current_room = previous_room;
        queen_runtime_joe_x = previous_joe_x;
        queen_runtime_joe_y = previous_joe_y;
        queen_runtime_joe_start_x = previous_joe_x;
        queen_runtime_joe_start_y = previous_joe_y;
        queen_runtime_joe_target_x = previous_joe_x;
        queen_runtime_joe_target_y = previous_joe_y;
        queen_runtime_joe_facing = previous_joe_facing;
        queen_runtime_joe_walk_step = 0;
        queen_runtime_joe_walk_steps = 0;
        queen_runtime_joe_walking = false;
        queen_runtime_selected_verb = previous_selected_verb;
        queen_runtime_inventory_page_first = previous_inventory_page_first;
        rb->memcpy(queen_runtime_inventory_slots, previous_inventory_slots,
                   sizeof(queen_runtime_inventory_slots));
        rb->memcpy(queen_runtime_object_frame_override,
                   previous_frame_override,
                   sizeof(queen_runtime_object_frame_override));
        rb->memcpy(queen_runtime_object_xflip, previous_object_xflip,
                   sizeof(queen_runtime_object_xflip));
        rb->memcpy(queen_runtime_talked_to, previous_talked_to,
                   sizeof(queen_runtime_talked_to));
        rb->memcpy(queen_runtime_talk_values, previous_talk_values,
                   sizeof(queen_runtime_talk_values));
        rb->close(fd);
        rb->strlcpy(status, "Queen autosave ignored", status_size);
        return true;
    }

    rb->close(fd);
    rb->strlcpy(status, "Queen autosave restored", status_size);
    return true;
}

static bool queen_runtime_decode_pcx(struct scummvm_video *video,
                                     const char *filename,
                                     uint32_t size,
                                     char *status,
                                     size_t status_size)
{
    const unsigned char *header = queen_pcx_data;
    const unsigned char *palette;
    uint32_t data_end;
    uint32_t src = QUEEN_PCX_HEADER_SIZE;
    uint16_t xmin;
    uint16_t ymin;
    uint16_t xmax;
    uint16_t ymax;
    uint16_t width;
    uint16_t height;
    uint16_t bytes_per_line;
    uint16_t y;
    uint16_t i;

    if (size < QUEEN_PCX_HEADER_SIZE + QUEEN_PCX_VGA_PALETTE_SIZE ||
        queen_pcx_data[size - QUEEN_PCX_VGA_PALETTE_SIZE] != 0x0c) {
        rb->snprintf(status, status_size,
                     "Queen PCX %s has no VGA palette", filename);
        return false;
    }

    if (header[0] != 0x0a || header[2] > 1 ||
        header[3] != 8 || header[65] != 1) {
        rb->snprintf(status, status_size,
                     "Queen PCX %s format is unsupported", filename);
        return false;
    }

    xmin = read_le16_rt(header + 4);
    ymin = read_le16_rt(header + 6);
    xmax = read_le16_rt(header + 8);
    ymax = read_le16_rt(header + 10);
    bytes_per_line = read_le16_rt(header + 66);
    if (xmax < xmin || ymax < ymin) {
        rb->snprintf(status, status_size,
                     "Queen PCX %s dimensions are invalid", filename);
        return false;
    }

    width = xmax - xmin + 1;
    height = ymax - ymin + 1;
    if (bytes_per_line < width ||
        width > SCUMMVM_SURFACE_W || height > SCUMMVM_SURFACE_H) {
        rb->snprintf(status, status_size,
                     "Queen PCX %s dimensions are unsupported", filename);
        return false;
    }

    palette = queen_pcx_data + size - 768;
    for (i = 0; i < 256; i++) {
        queen_pcx_palette[i] = LCD_RGBPACK(palette[i * 3],
                                           palette[i * 3 + 1],
                                           palette[i * 3 + 2]);
    }

    data_end = size - QUEEN_PCX_VGA_PALETTE_SIZE;
    for (y = 0; y < height; y++) {
        uint16_t written = 0;

        while (written < bytes_per_line) {
            unsigned char value;
            unsigned char count = 1;

            if (src >= data_end) {
                rb->snprintf(status, status_size,
                             "Queen PCX %s RLE ended early", filename);
                return false;
            }

            value = queen_pcx_data[src++];
            if (header[2] == 1 && (value & 0xc0) == 0xc0) {
                count = value & 0x3f;
                if (src >= data_end) {
                    rb->snprintf(status, status_size,
                                 "Queen PCX %s RLE run is truncated",
                                 filename);
                    return false;
                }
                value = queen_pcx_data[src++];
            }

            while (count-- && written < bytes_per_line) {
                if (written < width)
                    video->pixels[y * video->width + written] =
                        queen_pcx_palette[value];
                written++;
            }
        }
    }

    rb->snprintf(status, status_size,
                 "Queen backdrop %s rendered %ux%u",
                 filename, (unsigned)width, (unsigned)height);
    return true;
}

bool scummvm_queen_runtime_init(const struct scummvm_target *target,
                                struct scummvm_video *video,
                                char *status,
                                size_t status_size)
{
    struct scummvm_queen_jas_info jas_info;
    struct scummvm_queen_text_info text_info;

    if (!target || !video) {
        rb->strlcpy(status, "Queen runtime target is invalid",
                    status_size);
        return false;
    }

    if (!scummvm_queen_loader_jas_info(target, &jas_info,
                                       status, status_size))
        return false;
    if (!scummvm_queen_loader_text_info(target, &jas_info, &text_info,
                                        status, status_size))
        return false;

    queen_runtime_target = *target;
    queen_runtime_jas = jas_info;
    queen_runtime_text = text_info;
    queen_runtime_video = video;
    queen_runtime_current_room = jas_info.current_room;

    if (!queen_runtime_load_state(status, status_size))
        return false;
    if (!queen_runtime_load_save(status, status_size))
        return false;
    if (!queen_runtime_startup_room(status, status_size))
        return false;

    queen_runtime_anim_tick = 0;
    queen_runtime_selected_object = 0;
    queen_runtime_dirty = false;
    queen_runtime_active = true;
    return true;
}

static bool queen_runtime_conditions_pass(uint16_t command,
                                          uint16_t *sets,
                                          uint16_t max_sets,
                                          uint16_t *set_count,
                                          uint16_t *speak_description,
                                          bool *passed,
                                          char *status,
                                          size_t status_size)
{
    uint16_t index;

    *set_count = 0;
    *speak_description = 0;
    *passed = true;
    for (index = 1; index <= queen_runtime_jas.command_game_state; index++) {
        struct scummvm_queen_command_game_state game_state;
        uint16_t slot;

        if (!scummvm_queen_loader_read_command_game_state(
                &queen_runtime_target, &queen_runtime_jas, index,
                &game_state, status, status_size))
            return false;
        if (game_state.id != (int16_t)command)
            continue;

        slot = queen_runtime_abs16(game_state.slot);
        if (slot >= QUEEN_RUNTIME_GAME_STATE_COUNT) {
            rb->snprintf(status, status_size,
                         "Queen command %u game-state slot %u is invalid",
                         (unsigned)command, (unsigned)slot);
            return false;
        }

        if (game_state.slot > 0) {
            if (queen_runtime_game_state[slot] != game_state.value) {
                rb->snprintf(status, status_size,
                             "Queen command %u condition failed",
                             (unsigned)command);
                *speak_description = game_state.speak_value;
                *passed = false;
                return true;
            }
        } else if (*set_count < max_sets) {
            sets[*set_count] = index;
            (*set_count)++;
        }
    }

    return true;
}

static bool queen_runtime_apply_game_state_sets(const uint16_t *sets,
                                                uint16_t set_count,
                                                uint16_t *speak_description,
                                                char *status,
                                                size_t status_size)
{
    uint16_t index;

    for (index = 0; index < set_count; index++) {
        struct scummvm_queen_command_game_state game_state;
        uint16_t slot;

        if (!scummvm_queen_loader_read_command_game_state(
                &queen_runtime_target, &queen_runtime_jas, sets[index],
                &game_state, status, status_size))
            return false;

        slot = queen_runtime_abs16(game_state.slot);
        if (slot >= QUEEN_RUNTIME_GAME_STATE_COUNT)
            return false;
        queen_runtime_game_state[slot] = game_state.value;
        if (game_state.speak_value != 0)
            *speak_description = game_state.speak_value;
    }

    return true;
}

static bool queen_runtime_apply_objects(uint16_t command,
                                        uint16_t *applied,
                                        char *status,
                                        size_t status_size)
{
    uint16_t index;

    for (index = 1; index <= queen_runtime_jas.command_objects; index++) {
        struct scummvm_queen_command_object object;
        uint16_t destination;
        uint16_t source;

        if (!scummvm_queen_loader_read_command_object(
                &queen_runtime_target, &queen_runtime_jas, index, &object,
                status, status_size))
            return false;
        if (object.id != (int16_t)command)
            continue;

        destination = queen_runtime_abs16(object.destination_object);
        source = queen_runtime_abs16(object.source_object);
        if (destination == 0 || destination > queen_runtime_jas.objects ||
            (object.source_object > 0 &&
             source > queen_runtime_jas.objects)) {
            rb->snprintf(status, status_size,
                         "Queen command %u object reference is invalid",
                         (unsigned)command);
            return false;
        }

        if (object.destination_object > 0) {
            if (queen_runtime_objects[destination].name < 0)
                queen_runtime_objects[destination].name =
                    -queen_runtime_objects[destination].name;
            if (object.source_object == -1 &&
                queen_runtime_objects[destination].name != 0) {
                queen_runtime_objects[destination].name = 0;
                queen_runtime_object_frame_override[destination] = 0;
                queen_runtime_object_xflip[destination] = false;
                if (queen_runtime_objects[destination].image != -3 &&
                    queen_runtime_objects[destination].image != -4)
                    queen_runtime_objects[destination].image =
                        -(queen_runtime_objects[destination].image + 10);
            } else if (object.source_object > 0) {
                queen_runtime_objects[destination] =
                    queen_runtime_objects[source];
                queen_runtime_object_frame_override[destination] =
                    queen_runtime_object_frame_override[source];
                queen_runtime_object_xflip[destination] =
                    queen_runtime_object_xflip[source];
            }
        } else if (object.destination_object < 0 &&
                   queen_runtime_objects[destination].name > 0) {
            queen_runtime_objects[destination].name =
                -queen_runtime_objects[destination].name;
            queen_runtime_object_frame_override[destination] = 0;
            queen_runtime_object_xflip[destination] = false;
        }
        (*applied)++;
    }

    return true;
}

static bool queen_runtime_apply_inventory(uint16_t command,
                                          uint16_t *applied,
                                          char *status,
                                          size_t status_size)
{
    uint16_t index;

    for (index = 1; index <= queen_runtime_jas.command_inventory; index++) {
        struct scummvm_queen_command_inventory inventory;
        uint16_t destination;
        uint16_t source;

        if (!scummvm_queen_loader_read_command_inventory(
                &queen_runtime_target, &queen_runtime_jas, index,
                &inventory, status, status_size))
            return false;
        if (inventory.id != (int16_t)command)
            continue;

        destination = queen_runtime_abs16(inventory.destination_item);
        source = queen_runtime_abs16(inventory.source_item);
        if (destination == 0 || destination > queen_runtime_jas.items ||
            (inventory.source_item > 0 &&
             source > queen_runtime_jas.items)) {
            rb->snprintf(status, status_size,
                         "Queen command %u inventory reference is invalid",
                         (unsigned)command);
            return false;
        }

        if (inventory.destination_item > 0) {
            if (inventory.source_item > 0)
                queen_runtime_items[destination] =
                    queen_runtime_items[source];
            if (!queen_runtime_add_inventory_item(destination, status,
                                                  status_size))
                return false;
        } else if (inventory.destination_item < 0) {
            if (inventory.source_item > 0) {
                queen_runtime_items[destination] =
                    queen_runtime_items[source];
            }
            if (!queen_runtime_delete_inventory_item(destination, status,
                                                     status_size))
                return false;
        }
        (*applied)++;
    }

    return true;
}

static bool queen_runtime_apply_areas(uint16_t command,
                                      uint16_t *applied,
                                      char *status,
                                      size_t status_size)
{
    uint16_t index;

    for (index = 1; index <= queen_runtime_jas.command_areas; index++) {
        struct scummvm_queen_command_area area;
        uint16_t area_number;

        if (!scummvm_queen_loader_read_command_area(
                &queen_runtime_target, &queen_runtime_jas, index, &area,
                status, status_size))
            return false;
        if (area.id != (int16_t)command)
            continue;

        area_number = queen_runtime_abs16(area.area);
        if (area.room == queen_runtime_current_room) {
            if (area_number == 0 ||
                area_number > queen_runtime_room_area_count) {
                rb->snprintf(status, status_size,
                             "Queen command %u area reference is invalid",
                             (unsigned)command);
                return false;
            }

            if (area.area > 0 &&
                queen_runtime_room_areas[area_number].neighbors < 0)
                queen_runtime_room_areas[area_number].neighbors =
                    -queen_runtime_room_areas[area_number].neighbors;
            else if (area.area < 0 &&
                     queen_runtime_room_areas[area_number].neighbors > 0)
                queen_runtime_room_areas[area_number].neighbors =
                    -queen_runtime_room_areas[area_number].neighbors;
        }
        (*applied)++;
    }

    return true;
}

static bool queen_runtime_execute_command_match(uint16_t command,
                                                bool last_command,
                                                bool *handled,
                                                bool *applied,
                                                char *status,
                                                size_t status_size)
{
    uint16_t sets[32];
    uint16_t set_count = 0;
    uint16_t object_ops = 0;
    uint16_t inventory_ops = 0;
    uint16_t area_ops = 0;
    uint16_t speak_description = 0;
    bool passed = false;

    *handled = false;
    *applied = false;
    if (!queen_runtime_conditions_pass(command, sets, ARRAYLEN(sets),
                                       &set_count, &speak_description,
                                       &passed,
                                       status, status_size))
        return false;
    if (!passed) {
        if (!last_command)
            return true;

        *handled = true;
        if (speak_description != 0)
            return queen_runtime_show_object_description(speak_description,
                                                        status,
                                                        status_size);
        return true;
    }

    if (!queen_runtime_apply_game_state_sets(sets, set_count,
                                             &speak_description,
                                             status, status_size))
        return false;
    if (!queen_runtime_apply_objects(command, &object_ops,
                                     status, status_size))
        return false;
    if (!queen_runtime_apply_inventory(command, &inventory_ops,
                                       status, status_size))
        return false;
    if (!queen_runtime_apply_areas(command, &area_ops,
                                   status, status_size))
        return false;

    if (area_ops > 0 || object_ops > 0 || inventory_ops > 0 ||
        set_count > 0) {
        queen_runtime_dirty = true;
        if (object_ops > 0 && !queen_runtime_redraw_scene(status,
                                                          status_size))
            return false;
    }

    rb->snprintf(status, status_size,
                 "Queen command %u applied: %u area, %u obj, %u inv, %u gs",
                 (unsigned)command,
                 (unsigned)area_ops,
                 (unsigned)object_ops,
                 (unsigned)inventory_ops,
                 (unsigned)set_count);
    *handled = true;
    *applied = true;
    if (speak_description != 0)
        return queen_runtime_show_object_description(speak_description,
                                                    status, status_size);
    return true;
}

static bool queen_runtime_apply_special_section(int16_t special_section,
                                                char *status,
                                                size_t status_size)
{
    if (special_section == 0)
        return true;

    switch (special_section) {
    case 1:
        rb->strlcpy(status, "Queen journal section pending", status_size);
        return true;
    case 2:
        queen_runtime_game_state[QUEEN_GAME_STATE_JOE_DRESSING_MODE] = 2;
        queen_runtime_dirty = true;
        rb->strlcpy(status, "Queen Joe dress mode applied", status_size);
        return true;
    case 3:
        queen_runtime_game_state[QUEEN_GAME_STATE_JOE_DRESSING_MODE] = 0;
        queen_runtime_dirty = true;
        rb->strlcpy(status, "Queen Joe clothes mode applied", status_size);
        return true;
    case 4:
        queen_runtime_game_state[QUEEN_GAME_STATE_JOE_DRESSING_MODE] = 1;
        queen_runtime_dirty = true;
        rb->strlcpy(status, "Queen Joe underwear mode applied", status_size);
        return true;
    default:
        rb->snprintf(status, status_size,
                     "Queen special section %d pending",
                     (int)special_section);
        return true;
    }
}

static void queen_runtime_set_object_visible(uint16_t object_index,
                                             bool visible,
                                             bool *changed)
{
    if (object_index == 0 || object_index > queen_runtime_jas.objects)
        return;

    if (visible && queen_runtime_objects[object_index].name < 0) {
        queen_runtime_objects[object_index].name =
            -queen_runtime_objects[object_index].name;
        *changed = true;
    } else if (!visible && queen_runtime_objects[object_index].name > 0) {
        queen_runtime_objects[object_index].name =
            -queen_runtime_objects[object_index].name;
        *changed = true;
    }
}

static bool queen_runtime_apply_special_move(int16_t special_move,
                                             bool *applied,
                                             char *status,
                                             size_t status_size)
{
    bool changed = false;

    *applied = false;
    if (special_move <= 0)
        return true;

    switch (special_move) {
    case 2:
    case 3:
    case 4:
        if (!queen_runtime_apply_special_section(special_move, status,
                                                 status_size))
            return false;
        *applied = true;
        changed = true;
        break;
    case 5:
    case 6:
        rb->snprintf(status, status_size,
                     "Queen special move %d palette pending",
                     (int)special_move);
        *applied = true;
        break;
    case 8:
        queen_runtime_set_object_visible(594, false, &changed);
        rb->strlcpy(status, "Queen car animation stopped", status_size);
        *applied = true;
        break;
    case 9:
        queen_runtime_game_state[148] = 1;
        rb->strlcpy(status, "Queen fight animation state applied",
                    status_size);
        *applied = true;
        changed = true;
        break;
    case 11:
        queen_runtime_set_object_visible(521, true, &changed);
        queen_runtime_set_object_visible(526, true, &changed);
        queen_runtime_set_object_visible(522, false, &changed);
        queen_runtime_set_object_visible(525, false, &changed);
        queen_runtime_set_object_visible(523, false, &changed);
        queen_runtime_game_state[157] = 1;
        rb->strlcpy(status, "Queen Frank growth state applied",
                    status_size);
        *applied = true;
        changed = true;
        break;
    case 12:
        queen_runtime_set_object_visible(524, false, &changed);
        queen_runtime_set_object_visible(526, false, &changed);
        rb->strlcpy(status, "Queen robot growth state applied",
                    status_size);
        *applied = true;
        break;
    case 19:
        queen_runtime_game_state[QUEEN_GAME_STATE_AZURA_IN_LOVE] = 1;
        rb->strlcpy(status, "Queen Azura state applied", status_size);
        *applied = true;
        changed = true;
        break;
    case 21:
    case 22:
        rb->snprintf(status, status_size,
                     "Queen special move %d light palette pending",
                     (int)special_move);
        *applied = true;
        break;
    case 23:
        if (queen_runtime_current_room == QUEEN_ROOM_FLODA_FRONTDESK &&
            queen_runtime_room_area_count >= 7 &&
            queen_runtime_room_areas[7].neighbors < 0) {
            queen_runtime_room_areas[7].neighbors =
                -queen_runtime_room_areas[7].neighbors;
            changed = true;
        }
        rb->strlcpy(status, "Queen mannequin area state applied",
                    status_size);
        *applied = true;
        break;
    case 25:
        queen_runtime_game_state[QUEEN_GAME_STATE_GUARDS_TURNED_ON] = 1;
        rb->strlcpy(status, "Queen guard state applied", status_size);
        *applied = true;
        changed = true;
        break;
    case 33:
        queen_runtime_puzzle_attempt_count++;
        if (queen_runtime_puzzle_attempt_count >= 4) {
            queen_runtime_puzzle_attempt_count = 0;
            if (!queen_runtime_show_joe_response(226, status, status_size))
                return false;
        } else {
            rb->snprintf(status, status_size,
                         "Queen puzzle attempt %u",
                         (unsigned)queen_runtime_puzzle_attempt_count);
        }
        *applied = true;
        break;
    default:
        rb->snprintf(status, status_size,
                     "Queen special move %d pending",
                     (int)special_move);
        *applied = true;
        break;
    }

    if (changed) {
        queen_runtime_dirty = true;
        if (!queen_runtime_redraw_scene(status, status_size))
            return false;
        if (special_move == 2 || special_move == 3 || special_move == 4)
            rb->snprintf(status, status_size,
                         "Queen special move %d Joe state applied",
                         (int)special_move);
    }

    return true;
}

static bool queen_runtime_find_walk_off(uint16_t object_index,
                                        struct scummvm_queen_walk_off_data *walk_off,
                                        bool *found,
                                        char *status,
                                        size_t status_size)
{
    uint16_t index;

    *found = false;
    for (index = 1; index <= queen_runtime_jas.walk_offs; index++) {
        if (!scummvm_queen_loader_read_walk_off(
                &queen_runtime_target, &queen_runtime_jas, index,
                walk_off, status, status_size))
            return false;
        if (walk_off->entry_object == (int16_t)object_index) {
            *found = true;
            return true;
        }
    }

    return true;
}

static bool queen_runtime_follow_entry_object(uint16_t object_index,
                                              char *status,
                                              size_t status_size)
{
    uint16_t entry_object;
    uint16_t next_room;
    struct scummvm_queen_walk_off_data walk_off;
    struct scummvm_queen_walk_off_data entry_walk_off;
    bool has_walk_off = false;
    bool has_entry_walk_off = false;
    uint16_t walk_x;
    uint16_t walk_y;
    uint16_t entry_x;
    uint16_t entry_y;

    if (object_index == 0 || object_index > queen_runtime_jas.objects)
        return true;
    if (queen_runtime_objects[object_index].entry_object == 0) {
        if (queen_runtime_objects[object_index].x != 0 ||
            queen_runtime_objects[object_index].y != 0) {
            queen_runtime_set_joe_target(
                queen_runtime_objects[object_index].x,
                queen_runtime_objects[object_index].y,
                queen_runtime_find_direction(
                    queen_runtime_objects[object_index].state));
            if (!queen_runtime_redraw_scene(status, status_size))
                return false;
            rb->snprintf(status, status_size,
                         "Queen Joe walked to object %u at %d,%d",
                         (unsigned)object_index,
                         (int)queen_runtime_joe_x,
                         (int)queen_runtime_joe_y);
        }
        return true;
    }
    if (queen_runtime_objects[object_index].entry_object < 0) {
        rb->snprintf(status, status_size,
                     "Queen exit object %u is closed",
                     (unsigned)object_index);
        return true;
    }

    entry_object = (uint16_t)queen_runtime_objects[object_index].entry_object;
    if (entry_object == 0 || entry_object > queen_runtime_jas.objects) {
        rb->snprintf(status, status_size,
                     "Queen object %u entry reference is invalid",
                     (unsigned)object_index);
        return false;
    }

    next_room = queen_runtime_objects[entry_object].room;
    if (next_room == 0 || next_room > queen_runtime_jas.rooms) {
        rb->snprintf(status, status_size,
                     "Queen entry object %u room is invalid",
                     (unsigned)entry_object);
        return false;
    }

    if (!queen_runtime_find_walk_off(object_index, &walk_off, &has_walk_off,
                                     status, status_size))
        return false;
    if (has_walk_off) {
        walk_x = walk_off.x;
        walk_y = walk_off.y;
    } else {
        walk_x = queen_runtime_objects[object_index].x;
        walk_y = queen_runtime_objects[object_index].y;
    }
    queen_runtime_set_joe_target(
        walk_x, walk_y,
        queen_runtime_find_direction(queen_runtime_objects[object_index].state));

    if (next_room == queen_runtime_current_room) {
        if (!queen_runtime_redraw_scene(status, status_size))
            return false;
        rb->snprintf(status, status_size,
                     "Queen Joe walked to room object %u at %d,%d",
                     (unsigned)object_index,
                     (int)queen_runtime_joe_x,
                     (int)queen_runtime_joe_y);
        return true;
    }

    if (!queen_runtime_find_walk_off(entry_object, &entry_walk_off,
                                     &has_entry_walk_off, status,
                                     status_size))
        return false;

    queen_runtime_current_room = next_room;
    queen_runtime_dirty = true;
    if (!queen_runtime_load_room(status, status_size))
        return false;

    if (queen_runtime_objects[entry_object].x != 0 ||
        queen_runtime_objects[entry_object].y != 0) {
        entry_x = queen_runtime_objects[entry_object].x;
        entry_y = queen_runtime_objects[entry_object].y;
    } else if (has_entry_walk_off) {
        entry_x = entry_walk_off.x;
        entry_y = entry_walk_off.y;
    } else {
        entry_x = walk_x;
        entry_y = walk_y;
    }
    queen_runtime_set_joe_target(
        entry_x, entry_y,
        queen_runtime_opposite_direction(
            queen_runtime_find_direction(
                queen_runtime_objects[entry_object].state)));
    if (!queen_runtime_redraw_scene(status, status_size))
        return false;

    rb->snprintf(status, status_size,
                 "Queen moved to room %u via object %u at %u,%u; Joe %d,%d",
                 (unsigned)next_room, (unsigned)object_index,
                 (unsigned)walk_x, (unsigned)walk_y,
                 (int)queen_runtime_joe_x,
                 (int)queen_runtime_joe_y);
    return true;
}

static uint16_t queen_runtime_find_area_for_position(int x, int y)
{
    uint16_t index;

    for (index = 1; index <= queen_runtime_room_area_count; index++) {
        const struct scummvm_queen_box *box =
            &queen_runtime_room_areas[index].box;
        int left = box->x1 < box->x2 ? box->x1 : box->x2;
        int right = box->x1 < box->x2 ? box->x2 : box->x1;
        int top = box->y1 < box->y2 ? box->y1 : box->y2;
        int bottom = box->y1 < box->y2 ? box->y2 : box->y1;

        if (queen_runtime_room_areas[index].neighbors == 0)
            continue;
        if (x >= left && x <= right && y >= top && y <= bottom)
            return index;
    }

    return 0;
}

static bool queen_runtime_apply_special_area(uint16_t area,
                                             char *status,
                                             size_t status_size)
{
    if (area == 0)
        return true;

    switch (queen_runtime_current_room) {
    case QUEEN_ROOM_JUNGLE_BRIDGE:
        return queen_runtime_show_joe_response(16, status, status_size);
    case QUEEN_ROOM_JUNGLE_GORILLA_1:
        return queen_runtime_probe_script_resource_chained("C6C.CUT", false,
                                                           status,
                                                           status_size);
    case QUEEN_ROOM_JUNGLE_GORILLA_2:
        return queen_runtime_probe_script_resource_chained("C14B.CUT", false,
                                                           status,
                                                           status_size);
    case QUEEN_ROOM_AMAZON_ENTRANCE:
        if (area == 3)
            return queen_runtime_probe_script_resource_chained(
                "C16A.CUT", false, status, status_size);
        break;
    case QUEEN_ROOM_AMAZON_HIDEOUT:
        if (area == 4)
            return queen_runtime_probe_script_resource_chained(
                "C17A.CUT", false, status, status_size);
        if (area == 2)
            return queen_runtime_probe_script_resource_chained(
                "C17B.CUT", false, status, status_size);
        break;
    case QUEEN_ROOM_FLODA_OUTSIDE:
        return queen_runtime_probe_script_resource_chained("C22A.CUT", false,
                                                           status,
                                                           status_size);
    case QUEEN_ROOM_FLODA_KITCHEN:
        return queen_runtime_probe_script_resource_chained("C26B.CUT", false,
                                                           status,
                                                           status_size);
    case QUEEN_ROOM_FLODA_KLUNK:
        return queen_runtime_probe_script_resource_chained("C30A.CUT", false,
                                                           status,
                                                           status_size);
    case QUEEN_ROOM_FLODA_HENRY:
        return queen_runtime_probe_script_resource_chained("C32C.CUT", false,
                                                           status,
                                                           status_size);
    case QUEEN_ROOM_TEMPLE_ZOMBIES:
        if (area == 6) {
            if (queen_runtime_game_state
                    [QUEEN_GAME_STATE_BYPASS_ZOMBIES] == 0) {
                queen_runtime_game_state
                    [QUEEN_GAME_STATE_BYPASS_ZOMBIES] = 1;
                queen_runtime_dirty = true;
                return queen_runtime_probe_script_resource_chained(
                    "C50D.CUT", false, status, status_size);
            }
            return queen_runtime_probe_script_resource_chained(
                "C50H.CUT", false, status, status_size);
        }
        break;
    case QUEEN_ROOM_TEMPLE_SNAKE:
        return queen_runtime_probe_script_resource_chained("C53B.CUT", false,
                                                           status,
                                                           status_size);
    case QUEEN_ROOM_TEMPLE_LIZARD_LASER:
        return queen_runtime_show_joe_response(19, status, status_size);
    case QUEEN_ROOM_HOTEL_DOWNSTAIRS:
        return queen_runtime_show_joe_response(21, status, status_size);
    case QUEEN_ROOM_HOTEL_LOBBY:
        switch (queen_runtime_game_state
                    [QUEEN_GAME_STATE_HOTEL_ESCAPE_STATE]) {
        case 0:
            queen_runtime_game_state[QUEEN_GAME_STATE_JOE_DRESSING_MODE] = 1;
            queen_runtime_game_state[QUEEN_GAME_STATE_HOTEL_ESCAPE_STATE] = 1;
            queen_runtime_dirty = true;
            return queen_runtime_probe_script_resource_chained(
                "C73A.CUT", false, status, status_size);
        case 1:
            queen_runtime_game_state[QUEEN_GAME_STATE_HOTEL_ESCAPE_STATE] = 2;
            queen_runtime_dirty = true;
            return queen_runtime_probe_script_resource_chained(
                "C73B.CUT", false, status, status_size);
        case 2:
            return queen_runtime_probe_script_resource_chained(
                "C73C.CUT", false, status, status_size);
        default:
            break;
        }
        break;
    case QUEEN_ROOM_TEMPLE_MAZE_5:
        if (area == 7)
            return queen_runtime_show_joe_response(17, status, status_size);
        break;
    case QUEEN_ROOM_TEMPLE_MAZE_6:
        if (area == 5 && queen_runtime_game_state[187] == 0)
            return queen_runtime_probe_script_resource_chained(
                "C101B.CUT", false, status, status_size);
        break;
    case QUEEN_ROOM_FLODA_FRONTDESK:
        if (area == 3) {
            if (queen_runtime_game_state
                    [QUEEN_GAME_STATE_BYPASS_FLODA_RECEPTIONIST] == 0) {
                queen_runtime_game_state
                    [QUEEN_GAME_STATE_BYPASS_FLODA_RECEPTIONIST] = 1;
                queen_runtime_dirty = true;
                return queen_runtime_probe_script_resource_chained(
                    "C103B.CUT", false, status, status_size);
            }
            return queen_runtime_probe_script_resource_chained(
                "C103E.CUT", false, status, status_size);
        }
        break;
    default:
        break;
    }

    return true;
}

static bool queen_runtime_apply_subject_command_effects(uint16_t command,
                                                        uint16_t verb,
                                                        int16_t subject,
                                                        char *status,
                                                        size_t status_size)
{
    struct scummvm_queen_command_list_data command_data;
    struct scummvm_queen_object_data *object;
    uint16_t object_index;

    if (!scummvm_queen_loader_read_command_list(&queen_runtime_target,
                                                &queen_runtime_jas,
                                                command, &command_data,
                                                status, status_size))
        return false;
    if (!queen_runtime_apply_special_section(command_data.special_section,
                                             status, status_size))
        return false;

    if (subject <= 0)
        return true;

    object_index = (uint16_t)subject;
    if (object_index == 0 || object_index > queen_runtime_jas.objects)
        return true;

    object = &queen_runtime_objects[object_index];
    if (command_data.image_order != 0) {
        if (command_data.image_order < 0) {
            if (object->image > 0)
                object->image = -(object->image + 10);
        } else {
            object->image = command_data.image_order;
        }
        queen_runtime_object_frame_override[object_index] = 0;
        queen_runtime_object_xflip[object_index] = false;
        queen_runtime_dirty = true;
    }

    if (verb == QUEEN_VERB_OPEN) {
        if (queen_runtime_state_is_on(object->state)) {
            queen_runtime_state_set_on(&object->state, false);
            queen_runtime_state_set_default_verb(&object->state,
                                                 QUEEN_VERB_NONE);
            if (object->entry_object != 0) {
                queen_runtime_update_linked_entry_object(
                    object->entry_object, verb);
                object->entry_object =
                    (int16_t)queen_runtime_abs16(object->entry_object);
            }
            queen_runtime_dirty = true;
        }
    } else if (verb == QUEEN_VERB_CLOSE) {
        if (!queen_runtime_state_is_on(object->state)) {
            queen_runtime_state_set_on(&object->state, true);
            queen_runtime_state_set_default_verb(&object->state,
                                                 QUEEN_VERB_OPEN);
            if (object->entry_object != 0) {
                queen_runtime_update_linked_entry_object(
                    object->entry_object, verb);
                object->entry_object =
                    -(int16_t)queen_runtime_abs16(object->entry_object);
            }
            queen_runtime_dirty = true;
        }
    } else if (verb == QUEEN_VERB_MOVE) {
        queen_runtime_state_set_on(&object->state, false);
        queen_runtime_dirty = true;
    }

    return true;
}

static void queen_runtime_update_linked_entry_object(int16_t entry_object,
                                                     uint16_t verb)
{
    uint16_t linked;
    struct scummvm_queen_object_data *object;

    if (entry_object == 0)
        return;

    linked = queen_runtime_abs16(entry_object);
    if (linked == 0 || linked > queen_runtime_jas.objects)
        return;

    object = &queen_runtime_objects[linked];
    if (verb == QUEEN_VERB_OPEN) {
        if (queen_runtime_state_is_on(object->state)) {
            queen_runtime_state_set_on(&object->state, false);
            queen_runtime_state_set_default_verb(&object->state,
                                                 QUEEN_VERB_NONE);
            if (object->entry_object != 0)
                object->entry_object =
                    (int16_t)queen_runtime_abs16(object->entry_object);
            queen_runtime_dirty = true;
        }
    } else if (verb == QUEEN_VERB_CLOSE) {
        if (!queen_runtime_state_is_on(object->state)) {
            queen_runtime_state_set_on(&object->state, true);
            queen_runtime_state_set_default_verb(&object->state,
                                                 QUEEN_VERB_OPEN);
            if (object->entry_object != 0)
                object->entry_object =
                    -(int16_t)queen_runtime_abs16(object->entry_object);
            queen_runtime_dirty = true;
        }
    }
}

static uint16_t queen_runtime_random_description(uint16_t first,
                                                 uint16_t last,
                                                 uint16_t avoid)
{
    uint16_t span;
    uint16_t selected;

    if (last <= first)
        return first;

    span = last - first + 1;
    selected = first + (uint16_t)(rb->rand() % span);
    if (span > 1 && selected == avoid)
        selected = first + (uint16_t)((selected - first + 1) % span);

    return selected;
}

static bool queen_runtime_suffix_is(const char *text, const char *suffix)
{
    size_t text_len = rb->strlen(text);
    size_t suffix_len = rb->strlen(suffix);

    return text_len >= suffix_len &&
           !rb->strcasecmp(text + text_len - suffix_len, suffix);
}

static uint16_t queen_runtime_next_description(uint16_t object_index,
                                               uint16_t first_description)
{
    uint16_t index;

    for (index = 1; index <= queen_runtime_jas.object_descriptions; index++) {
        struct scummvm_queen_object_description *description =
            &queen_runtime_object_descriptions[index];
        uint16_t last_description;
        uint16_t selected;

        if (description->object != object_index)
            continue;

        last_description = description->last_description;
        if (last_description < first_description)
            last_description = first_description;

        switch (description->type) {
        case 0:
            if (description->last_seen_number == 0)
                selected = first_description;
            else
                selected = queen_runtime_random_description(
                    first_description, last_description,
                    description->last_seen_number);
            break;
        case 1:
            selected = queen_runtime_random_description(
                first_description, last_description,
                description->last_seen_number);
            break;
        case 2:
            if (description->last_seen_number < first_description ||
                description->last_seen_number >= last_description)
                selected = first_description;
            else
                selected = description->last_seen_number + 1;
            break;
        case 3:
            if (description->last_seen_number < first_description)
                selected = first_description;
            else if (description->last_seen_number < last_description)
                selected = description->last_seen_number + 1;
            else
                selected = last_description;
            break;
        default:
            selected = first_description;
            break;
        }

        description->last_seen_number = selected;
        queen_runtime_dirty = true;
        return selected;
    }

    return first_description;
}

static bool queen_runtime_script_offset_is_valid(uint16_t offset,
                                                 uint32_t payload_size)
{
    return offset != 0 && (uint32_t)offset < payload_size;
}

static bool queen_runtime_read_talk_string_ex(uint16_t *offset,
                                              uint32_t payload_size,
                                              unsigned align,
                                              char *text,
                                              size_t text_size,
                                              bool apply_joe_controls);

static bool queen_runtime_apply_speech_control(const char *control,
                                               bool apply_joe_controls)
{
    int x;
    int y;

    if (!apply_joe_controls)
        return false;

    if (control[0] == 'F') {
        switch (control[1]) {
        case 'L':
            queen_runtime_joe_facing = QUEEN_DIR_LEFT;
            queen_runtime_dirty = true;
            return true;
        case 'R':
            queen_runtime_joe_facing = QUEEN_DIR_RIGHT;
            queen_runtime_dirty = true;
            return true;
        case 'F':
            queen_runtime_joe_facing = QUEEN_DIR_FRONT;
            queen_runtime_dirty = true;
            return true;
        case 'B':
            queen_runtime_joe_facing = QUEEN_DIR_BACK;
            queen_runtime_dirty = true;
            return true;
        default:
            break;
        }
    }

    if (control[0] != 'X' || control[1] != 'Y')
        return false;

    x = rb->atoi(control + 5);
    y = rb->atoi(control + 9);
    if (x > 0 || y > 0) {
        uint16_t target_x = x > 0 ? (uint16_t)x :
            (uint16_t)queen_runtime_joe_x;
        uint16_t target_y = y > 0 ? (uint16_t)y :
            (uint16_t)queen_runtime_joe_y;

        queen_runtime_set_joe_target(target_x, target_y,
                                     queen_runtime_joe_facing);
        return true;
    }

    return false;
}

static void queen_runtime_clean_speech_text(char *text,
                                            bool apply_joe_controls)
{
    char *readp = text;
    char *writep = text;

    if (!text)
        return;

    while (*readp) {
        if (*readp == '*') {
            if (readp[1] == 'X' && readp[2] == 'Y') {
                queen_runtime_apply_speech_control(readp + 1,
                                                   apply_joe_controls);
                readp++;
                while (*readp && *readp != ')')
                    readp++;
                if (*readp == ')')
                    readp++;
                continue;
            }
            if (readp[1] && readp[2]) {
                queen_runtime_apply_speech_control(readp + 1,
                                                   apply_joe_controls);
                readp += 3;
                continue;
            }
            readp++;
            continue;
        }
        *writep++ = *readp++;
    }
    *writep = '\0';
}

static bool queen_runtime_read_talk_string(uint16_t *offset,
                                           uint32_t payload_size,
                                           unsigned align,
                                           char *text,
                                           size_t text_size)
{
    return queen_runtime_read_talk_string_ex(offset, payload_size, align,
                                            text, text_size, false);
}

static bool queen_runtime_read_talk_string_ex(uint16_t *offset,
                                              uint32_t payload_size,
                                              unsigned align,
                                              char *text,
                                              size_t text_size,
                                              bool apply_joe_controls)
{
    uint16_t pos = *offset;
    uint8_t length;
    uint16_t next;
    size_t copy;

    if ((align & 1) != 0 || align == 0 || pos >= payload_size ||
        pos >= sizeof(queen_script_header)) {
        if (text_size > 0)
            text[0] = '\0';
        return false;
    }

    length = queen_script_header[pos++];
    if (length == 0) {
        *offset = pos;
        if (text_size > 0)
            text[0] = '\0';
        return true;
    }

    if ((uint32_t)pos + length > payload_size ||
        (size_t)pos + length > sizeof(queen_script_header)) {
        if (text_size > 0)
            text[0] = '\0';
        return false;
    }

    copy = length;
    if (text_size == 0)
        copy = 0;
    else if (copy >= text_size)
        copy = text_size - 1;
    if (text_size > 0) {
        rb->memcpy(text, queen_script_header + pos, copy);
        text[copy] = '\0';
        queen_runtime_clean_speech_text(text, apply_joe_controls);
    }

    next = (uint16_t)((pos + length + align - 1) & ~(align - 1));
    *offset = next;
    return true;
}

static bool queen_runtime_find_dialog_string(uint16_t offset,
                                             int16_t id,
                                             int16_t max,
                                             uint32_t payload_size,
                                             char *text,
                                             size_t text_size)
{
    int16_t i;

    if (text_size > 0)
        text[0] = '\0';
    if (id <= 0 || max <= 0 || max > 128)
        return false;

    for (i = 1; i <= max; i++) {
        int16_t current_id;

        if ((uint32_t)offset + 4 > payload_size ||
            (size_t)offset + 4 > sizeof(queen_script_header))
            return false;

        offset += 2;
        current_id = read_be16s_rt(queen_script_header + offset);
        offset += 2;
        if (current_id == id)
            return queen_runtime_read_talk_string(&offset, payload_size, 4,
                                                  text, text_size);

        if (!queen_runtime_read_talk_string(&offset, payload_size, 4,
                                            NULL, 0))
            return false;
    }

    return false;
}

static bool queen_runtime_read_cutaway_object(uint16_t offset,
                                              uint32_t payload_size,
                                              struct queen_cutaway_object *object,
                                              uint16_t *next_offset)
{
    const unsigned char *data;
    int16_t person_count;
    uint16_t index;

    if ((uint32_t)offset + 34 > payload_size ||
        (size_t)offset + 34 > sizeof(queen_script_header))
        return false;

    rb->memset(object, 0, sizeof(*object));
    data = queen_script_header + offset;
    object->object_number = read_be16s_rt(data);
    object->move_to_x = read_be16s_rt(data + 2);
    object->move_to_y = read_be16s_rt(data + 4);
    object->bank = read_be16s_rt(data + 6);
    object->anim_list = read_be16s_rt(data + 8);
    object->execute = read_be16s_rt(data + 10);
    object->limit_bob_x1 = read_be16s_rt(data + 12);
    object->limit_bob_y1 = read_be16s_rt(data + 14);
    object->limit_bob_x2 = read_be16s_rt(data + 16);
    object->limit_bob_y2 = read_be16s_rt(data + 18);
    object->special_move = read_be16s_rt(data + 20);
    object->anim_type = read_be16s_rt(data + 22);
    object->from_object = read_be16s_rt(data + 24);
    object->bob_start_x = read_be16s_rt(data + 26);
    object->bob_start_y = read_be16s_rt(data + 28);
    object->room = read_be16s_rt(data + 30);
    object->scale = read_be16s_rt(data + 32);
    if (object->limit_bob_x1 < 0) {
        object->song = -object->limit_bob_x1;
        object->limit_bob_x1 = 0;
    } else {
        object->song = 0;
    }

    offset += 34;
    if (next_offset)
        *next_offset = offset;
    if ((uint32_t)offset + 2 > payload_size ||
        (size_t)offset + 2 > sizeof(queen_script_header))
        return true;

    person_count = read_be16s_rt(queen_script_header + offset);
    offset += 2;
    if (person_count < 0)
        person_count = 0;
    if (person_count > (int16_t)ARRAYLEN(object->person))
        person_count = (int16_t)ARRAYLEN(object->person);

    object->person_count = person_count;
    for (index = 0; index < (uint16_t)person_count; index++) {
        if ((uint32_t)offset + 2 > payload_size ||
            (size_t)offset + 2 > sizeof(queen_script_header)) {
            object->person_count = (int16_t)index;
            break;
        }
        object->person[index] = read_be16s_rt(queen_script_header + offset);
        offset += 2;
    }

    if (next_offset)
        *next_offset = offset;
    return true;
}

static bool queen_runtime_cutaway_object_has_animation(
    const struct queen_cutaway_object *object)
{
    if (object->from_object > 0 || object->execute == 0)
        return false;
    if (object->object_number == -2 ||
        object->object_number == -3 ||
        object->object_number == -4)
        return false;
    if ((object->object_number == 0 || object->object_number > 0) &&
        object->anim_list == 0)
        return false;

    return true;
}

static bool queen_runtime_skip_cutaway_animation(uint16_t offset,
                                                 uint16_t limit,
                                                 uint32_t payload_size,
                                                 uint16_t *next_offset)
{
    uint16_t frame_count = 0;

    while (offset + 2 <= limit &&
           (uint32_t)offset + 2 <= payload_size &&
           (size_t)offset + 2 <= sizeof(queen_script_header)) {
        int16_t header = read_be16s_rt(queen_script_header + offset);

        offset += 2;
        if (header == -2) {
            *next_offset = offset;
            return true;
        }
        if (header > 1000 || frame_count >= 56 ||
            offset + 18 > limit ||
            (uint32_t)offset + 18 > payload_size ||
            (size_t)offset + 18 > sizeof(queen_script_header))
            return false;
        offset += 18;
        frame_count++;
    }

    return false;
}

static bool queen_runtime_apply_cutaway_animation(uint16_t offset,
                                                  uint16_t limit,
                                                  uint32_t payload_size,
                                                  const struct queen_cutaway_object *object,
                                                  uint16_t *next_offset,
                                                  bool *applied,
                                                  char *status,
                                                  size_t status_size)
{
    uint16_t frame_count = 0;
    bool changed = false;

    *applied = false;
    while (offset + 2 <= limit &&
           (uint32_t)offset + 2 <= payload_size &&
           (size_t)offset + 2 <= sizeof(queen_script_header)) {
        int16_t header = read_be16s_rt(queen_script_header + offset);
        int16_t unpack_frame;
        bool xflip = false;
        int16_t mx;
        int16_t my;
        int16_t cx;
        int16_t cy;

        offset += 2;
        if (header == -2) {
            *next_offset = offset;
            if (changed) {
                queen_runtime_dirty = true;
                if (!queen_runtime_redraw_scene(status, status_size))
                    return false;
                rb->snprintf(status, status_size,
                             "Queen cutaway animation state applied");
                *applied = true;
            }
            return true;
        }
        if (header > 1000 || frame_count >= 56 ||
            offset + 18 > limit ||
            (uint32_t)offset + 18 > payload_size ||
            (size_t)offset + 18 > sizeof(queen_script_header))
            return false;

        unpack_frame = read_be16s_rt(queen_script_header + offset);
        if (unpack_frame < 0) {
            unpack_frame = -unpack_frame;
            xflip = true;
        }
        mx = read_be16s_rt(queen_script_header + offset + 6);
        my = read_be16s_rt(queen_script_header + offset + 8);
        cx = read_be16s_rt(queen_script_header + offset + 10);
        cy = read_be16s_rt(queen_script_header + offset + 12);
        offset += 18;
        frame_count++;

        if (object->object_number == 0) {
            if (cx || cy) {
                queen_runtime_joe_x = cx;
                queen_runtime_joe_y = cy;
                queen_runtime_joe_start_x = queen_runtime_joe_x;
                queen_runtime_joe_start_y = queen_runtime_joe_y;
                queen_runtime_joe_target_x = queen_runtime_joe_x;
                queen_runtime_joe_target_y = queen_runtime_joe_y;
                queen_runtime_joe_walk_step = 0;
                queen_runtime_joe_walk_steps = 0;
                queen_runtime_joe_walking = false;
                changed = true;
            }
            if (mx || my) {
                uint16_t target_x = mx ?
                    (uint16_t)mx : (uint16_t)queen_runtime_joe_x;
                uint16_t target_y = my ?
                    (uint16_t)my : (uint16_t)queen_runtime_joe_y;

                queen_runtime_set_joe_target(target_x, target_y,
                                             queen_runtime_joe_facing);
                changed = true;
            }
        } else if (object->object_number > 0 &&
                   (uint16_t)object->object_number <=
                       queen_runtime_jas.objects) {
            struct scummvm_queen_object_data *runtime_object =
                &queen_runtime_objects[(uint16_t)object->object_number];
            uint16_t object_index = (uint16_t)object->object_number;

            if (unpack_frame == 0 && runtime_object->name > 0) {
                runtime_object->name = -runtime_object->name;
                changed = true;
                queen_runtime_object_frame_override[object_index] = 0;
                queen_runtime_object_xflip[object_index] = false;
            } else {
                if (queen_runtime_object_xflip[object_index] != xflip) {
                    queen_runtime_object_xflip[object_index] = xflip;
                    changed = true;
                }
                if (unpack_frame > 0 &&
                    queen_runtime_object_frame_override[object_index] !=
                        (uint16_t)unpack_frame) {
                    queen_runtime_object_frame_override[object_index] =
                        (uint16_t)unpack_frame;
                    changed = true;
                }
            }
            if (cx || cy) {
                runtime_object->x = (uint16_t)cx;
                runtime_object->y = (uint16_t)cy;
                changed = true;
            } else if (mx || my) {
                if (mx)
                    runtime_object->x = (uint16_t)mx;
                if (my)
                    runtime_object->y = (uint16_t)my;
                changed = true;
            }
        }
    }

    return false;
}

static bool queen_runtime_apply_cutaway_joe_object(
    const struct queen_cutaway_object *object,
    bool *applied,
    char *status,
    size_t status_size)
{
    bool changed = false;

    *applied = false;
    if (object->object_number != 0)
        return true;

    if (object->bob_start_x > 0 || object->bob_start_y > 0) {
        queen_runtime_joe_x = object->bob_start_x;
        queen_runtime_joe_y = object->bob_start_y;
        queen_runtime_joe_start_x = queen_runtime_joe_x;
        queen_runtime_joe_start_y = queen_runtime_joe_y;
        queen_runtime_joe_target_x = queen_runtime_joe_x;
        queen_runtime_joe_target_y = queen_runtime_joe_y;
        queen_runtime_joe_walk_step = 0;
        queen_runtime_joe_walk_steps = 0;
        queen_runtime_joe_walking = false;
        changed = true;
    }

    if (object->move_to_x > 0 || object->move_to_y > 0) {
        uint16_t target_x = object->move_to_x > 0 ?
            (uint16_t)object->move_to_x : (uint16_t)queen_runtime_joe_x;
        uint16_t target_y = object->move_to_y > 0 ?
            (uint16_t)object->move_to_y : (uint16_t)queen_runtime_joe_y;

        queen_runtime_set_joe_target(target_x, target_y,
                                     queen_runtime_joe_facing);
        changed = true;
    }

    if (!changed)
        return true;

    queen_runtime_dirty = true;
    if (!queen_runtime_redraw_scene(status, status_size))
        return false;

    rb->snprintf(status, status_size,
                 "Queen cutaway Joe: %d,%d -> %d,%d",
                 (int)object->bob_start_x, (int)object->bob_start_y,
                 (int)queen_runtime_joe_target_x,
                 (int)queen_runtime_joe_target_y);
    *applied = true;
    return true;
}

static bool queen_runtime_apply_cutaway_object_copy(
    const struct queen_cutaway_object *object,
    bool *applied,
    char *status,
    size_t status_size)
{
    uint16_t destination;
    uint16_t source;

    *applied = false;
    if (object->from_object <= 0 || object->object_number <= 0)
        return true;

    destination = (uint16_t)object->object_number;
    source = (uint16_t)object->from_object;
    if (destination > queen_runtime_jas.objects ||
        source > queen_runtime_jas.objects) {
        rb->snprintf(status, status_size,
                     "Queen cutaway object copy %u <- %u invalid",
                     (unsigned)destination, (unsigned)source);
        return false;
    }

    if (destination == source) {
        if (queen_runtime_objects[destination].name < 0)
            queen_runtime_objects[destination].name =
                -queen_runtime_objects[destination].name;
    } else {
        queen_runtime_objects[destination] = queen_runtime_objects[source];
        queen_runtime_object_frame_override[destination] =
            queen_runtime_object_frame_override[source];
        queen_runtime_object_xflip[destination] =
            queen_runtime_object_xflip[source];
    }

    queen_runtime_dirty = true;
    if (!queen_runtime_redraw_scene(status, status_size))
        return false;

    rb->snprintf(status, status_size,
                 "Queen cutaway object %u copied from %u",
                 (unsigned)destination, (unsigned)source);
    *applied = true;
    return true;
}

static bool queen_runtime_cutaway_has_joe(
    const struct queen_cutaway_object *object)
{
    int16_t index;

    for (index = 0; index < object->person_count; index++) {
        if (object->person[index] == -1)
            return true;
    }

    return false;
}

static bool queen_runtime_cutaway_person_is_listed(
    const struct queen_cutaway_object *object,
    uint16_t person)
{
    int16_t index;

    for (index = 0; index < object->person_count; index++) {
        if (object->person[index] == (int16_t)person)
            return true;
    }

    return false;
}

static bool queen_runtime_apply_cutaway_person_list(
    const struct queen_cutaway_object *object,
    bool *applied,
    char *status,
    size_t status_size)
{
    struct scummvm_queen_room_object_range range;
    uint16_t index;
    uint16_t person_ops = 0;

    *applied = false;
    if (object->person_count <= 0 || object->room <= 0 ||
        (uint16_t)object->room != queen_runtime_current_room)
        return true;

    if (!scummvm_queen_loader_room_range(&queen_runtime_target,
                                         &queen_runtime_jas,
                                         queen_runtime_current_room,
                                         &range, status, status_size))
        return false;

    for (index = range.first_object; index <= range.last_object; index++) {
        struct scummvm_queen_object_data *runtime_object;
        bool visible;

        if (index == 0 || index > queen_runtime_jas.objects)
            continue;

        runtime_object = &queen_runtime_objects[index];
        if (runtime_object->image != -3 && runtime_object->image != -4)
            continue;

        visible = queen_runtime_cutaway_person_is_listed(object, index);
        if (visible && runtime_object->name < 0) {
            runtime_object->name = -runtime_object->name;
            person_ops++;
        } else if (!visible && runtime_object->name > 0) {
            runtime_object->name = -runtime_object->name;
            person_ops++;
        }
    }

    if (queen_runtime_cutaway_has_joe(object) &&
        (object->bob_start_x > 0 || object->bob_start_y > 0)) {
        queen_runtime_joe_x = object->bob_start_x;
        queen_runtime_joe_y = object->bob_start_y;
        queen_runtime_joe_start_x = queen_runtime_joe_x;
        queen_runtime_joe_start_y = queen_runtime_joe_y;
        queen_runtime_joe_target_x = queen_runtime_joe_x;
        queen_runtime_joe_target_y = queen_runtime_joe_y;
        queen_runtime_joe_walk_step = 0;
        queen_runtime_joe_walk_steps = 0;
        queen_runtime_joe_walking = false;
        person_ops++;
    }

    if (person_ops == 0)
        return true;

    queen_runtime_dirty = true;
    if (!queen_runtime_redraw_scene(status, status_size))
        return false;

    rb->snprintf(status, status_size,
                 "Queen cutaway person list applied: %u",
                 (unsigned)person_ops);
    *applied = true;
    return true;
}

static bool queen_runtime_apply_cutaway_person_object(
    const struct queen_cutaway_object *object,
    bool *applied,
    char *status,
    size_t status_size)
{
    struct scummvm_queen_object_data *runtime_object;
    bool changed = false;
    uint16_t object_index;

    *applied = false;
    if (object->object_number <= 0 ||
        (uint16_t)object->object_number > queen_runtime_jas.objects ||
        object->anim_list != 0)
        return true;

    object_index = (uint16_t)object->object_number;
    runtime_object = &queen_runtime_objects[object_index];
    if (runtime_object->room != (int16_t)queen_runtime_current_room)
        return true;
    if (runtime_object->image != -3 && runtime_object->image != -4)
        return true;

    if (runtime_object->name < 0) {
        runtime_object->name = -runtime_object->name;
        changed = true;
    }
    if (object->bob_start_x > 0 || object->bob_start_y > 0) {
        runtime_object->x = (uint16_t)object->bob_start_x;
        runtime_object->y = (uint16_t)object->bob_start_y;
        changed = true;
    }
    if (object->move_to_x > 0 || object->move_to_y > 0) {
        if (object->move_to_x > 0)
            runtime_object->x = (uint16_t)object->move_to_x;
        if (object->move_to_y > 0)
            runtime_object->y = (uint16_t)object->move_to_y;
        changed = true;
    }

    if (!changed)
        return true;

    queen_runtime_dirty = true;
    if (!queen_runtime_redraw_scene(status, status_size))
        return false;

    rb->snprintf(status, status_size,
                 "Queen cutaway person object %u positioned",
                 (unsigned)object_index);
    *applied = true;
    return true;
}

static bool queen_runtime_apply_cutaway_room_change(
    const struct queen_cutaway_object *object,
    bool *applied,
    char *status,
    size_t status_size)
{
    *applied = false;
    if (object->room <= 0)
        return true;
    if ((uint16_t)object->room > queen_runtime_jas.rooms) {
        rb->snprintf(status, status_size,
                     "Queen cutaway room %d is invalid",
                     (int)object->room);
        return false;
    }

    queen_runtime_current_room = (uint16_t)object->room;
    if (queen_runtime_cutaway_has_joe(object) &&
        (object->bob_start_x > 0 || object->bob_start_y > 0)) {
        queen_runtime_joe_x = object->bob_start_x;
        queen_runtime_joe_y = object->bob_start_y;
        queen_runtime_joe_start_x = queen_runtime_joe_x;
        queen_runtime_joe_start_y = queen_runtime_joe_y;
        queen_runtime_joe_target_x = queen_runtime_joe_x;
        queen_runtime_joe_target_y = queen_runtime_joe_y;
        queen_runtime_joe_walk_step = 0;
        queen_runtime_joe_walk_steps = 0;
        queen_runtime_joe_walking = false;
    } else if (!queen_runtime_cutaway_has_joe(object)) {
        queen_runtime_joe_x = 0;
        queen_runtime_joe_y = 0;
        queen_runtime_joe_walking = false;
    }

    if (!queen_runtime_load_room(status, status_size))
        return false;

    rb->snprintf(status, status_size,
                 "Queen cutaway room %u loaded",
                 (unsigned)queen_runtime_current_room);
    *applied = true;
    return true;
}

static bool queen_runtime_apply_cutaway_tail(uint16_t offset,
                                             uint32_t payload_size,
                                             bool *applied,
                                             char *status,
                                             size_t status_size)
{
    int16_t count;
    int16_t index;
    uint16_t final_room;
    uint16_t final_x;
    uint16_t final_y;

    *applied = false;
    if ((uint32_t)offset + 2 > payload_size ||
        (size_t)offset + 2 > sizeof(queen_script_header))
        return true;

    count = read_be16s_rt(queen_script_header + offset);
    offset += 2;
    if (count < 0 || count > 128)
        return true;
    if ((uint32_t)offset + (uint32_t)count * 12 + 6 > payload_size ||
        (size_t)offset + (size_t)count * 12 + 6 >
            sizeof(queen_script_header))
        return true;

    for (index = 0; index < count; index++) {
        int16_t state_index = read_be16s_rt(queen_script_header + offset);
        int16_t state_value = read_be16s_rt(queen_script_header + offset + 2);
        int16_t object_index = read_be16s_rt(queen_script_header + offset + 4);
        int16_t area_index = read_be16s_rt(queen_script_header + offset + 6);
        int16_t area_sub_index =
            read_be16s_rt(queen_script_header + offset + 8);
        int16_t from_object = read_be16s_rt(queen_script_header + offset + 10);
        bool update = false;

        offset += 12;
        if (state_index > 0) {
            uint16_t slot = (uint16_t)state_index;

            if (slot < QUEEN_RUNTIME_GAME_STATE_COUNT &&
                queen_runtime_game_state[slot] == state_value)
                update = true;
        } else if (state_index < 0) {
            uint16_t slot = queen_runtime_abs16(state_index);

            if (slot < QUEEN_RUNTIME_GAME_STATE_COUNT) {
                queen_runtime_game_state[slot] = state_value;
                update = true;
            }
        }

        if (!update)
            continue;

        if (object_index > 0 &&
            (uint16_t)object_index <= queen_runtime_jas.objects) {
            uint16_t object_id = (uint16_t)object_index;

            if (from_object > 0 &&
                (uint16_t)from_object <= queen_runtime_jas.objects) {
                queen_runtime_objects[object_id] =
                    queen_runtime_objects[(uint16_t)from_object];
                queen_runtime_object_frame_override[object_id] =
                    queen_runtime_object_frame_override[(uint16_t)from_object];
                queen_runtime_object_xflip[object_id] =
                    queen_runtime_object_xflip[(uint16_t)from_object];
            }
            if (queen_runtime_objects[object_id].name < 0)
                queen_runtime_objects[object_id].name =
                    -queen_runtime_objects[object_id].name;
            *applied = true;
        } else if (object_index < 0) {
            uint16_t object_id = queen_runtime_abs16(object_index);

            if (object_id <= queen_runtime_jas.objects &&
                queen_runtime_objects[object_id].name > 0) {
                queen_runtime_objects[object_id].name =
                    -queen_runtime_objects[object_id].name;
                queen_runtime_object_frame_override[object_id] = 0;
                queen_runtime_object_xflip[object_id] = false;
                *applied = true;
            }
        }

        if (area_index > 0 &&
            (uint16_t)area_index == queen_runtime_current_room) {
            uint16_t area_number = queen_runtime_abs16(area_sub_index);

            if (area_number > 0 &&
                area_number <= queen_runtime_room_area_count) {
                if (area_sub_index > 0 &&
                    queen_runtime_room_areas[area_number].neighbors < 0) {
                    queen_runtime_room_areas[area_number].neighbors =
                        -queen_runtime_room_areas[area_number].neighbors;
                    *applied = true;
                } else if (area_sub_index < 0 &&
                           queen_runtime_room_areas[area_number].neighbors >
                               0) {
                    queen_runtime_room_areas[area_number].neighbors =
                        -queen_runtime_room_areas[area_number].neighbors;
                    *applied = true;
                }
            }
        }
    }

    final_room = read_be16_rt(queen_script_header + offset);
    final_x = read_be16_rt(queen_script_header + offset + 2);
    final_y = read_be16_rt(queen_script_header + offset + 4);
    if (final_room > 0 && final_room <= queen_runtime_jas.rooms) {
        queen_runtime_current_room = final_room;
        queen_runtime_joe_x = (int16_t)final_x;
        queen_runtime_joe_y = (int16_t)final_y;
        queen_runtime_joe_start_x = queen_runtime_joe_x;
        queen_runtime_joe_start_y = queen_runtime_joe_y;
        queen_runtime_joe_target_x = queen_runtime_joe_x;
        queen_runtime_joe_target_y = queen_runtime_joe_y;
        queen_runtime_joe_walk_step = 0;
        queen_runtime_joe_walk_steps = 0;
        queen_runtime_joe_walking = false;
        if (!queen_runtime_load_room(status, status_size))
            return false;
        *applied = true;
    } else if ((final_x != 0 || final_y != 0) &&
               final_room == queen_runtime_current_room) {
        queen_runtime_joe_x = (int16_t)final_x;
        queen_runtime_joe_y = (int16_t)final_y;
        queen_runtime_joe_start_x = queen_runtime_joe_x;
        queen_runtime_joe_start_y = queen_runtime_joe_y;
        queen_runtime_joe_target_x = queen_runtime_joe_x;
        queen_runtime_joe_target_y = queen_runtime_joe_y;
        queen_runtime_joe_walking = false;
        queen_runtime_dirty = true;
        if (!queen_runtime_redraw_scene(status, status_size))
            return false;
        *applied = true;
    } else if (*applied) {
        queen_runtime_dirty = true;
        if (!queen_runtime_redraw_scene(status, status_size))
            return false;
    }

    if (*applied)
        rb->snprintf(status, status_size,
                     "Queen cutaway tail applied");
    return true;
}

static bool queen_runtime_apply_cutaway_sequence(uint16_t object_count,
                                                 uint16_t game_state_off,
                                                 uint16_t next_sentence_off,
                                                 uint32_t payload_size,
                                                 bool *applied,
                                                 char *status,
                                                 size_t status_size)
{
    uint16_t object_off = 12;
    uint16_t string_off = next_sentence_off;
    uint16_t index;
    bool any_applied = false;
    bool tail_applied = false;

    *applied = false;
    for (index = 0; index < object_count; index++) {
        struct queen_cutaway_object object;
        uint16_t next_object_off = object_off;
        bool applied_special = false;
        bool applied_room = false;
        bool applied_person = false;
        bool applied_person_object = false;
        bool applied_joe = false;
        bool applied_object = false;
        char sentence[96];

        if (object_off >= game_state_off ||
            !queen_runtime_read_cutaway_object(object_off, payload_size,
                                               &object,
                                               &next_object_off) ||
            next_object_off > game_state_off)
            break;

        sentence[0] = '\0';
        queen_runtime_read_talk_string_ex(
            &string_off, payload_size, 2, sentence, sizeof(sentence),
            object.object_number == -1 || object.object_number == 0);

        if (object.object_number == -1)
            object.object_number = 0;

        if (object.move_to_x == 0 && object.move_to_y == 0 &&
            object.special_move > 0 && object.object_number >= 0) {
            if (!queen_runtime_apply_special_move(object.special_move,
                                                  &applied_special,
                                                  status, status_size))
                return false;
            object.special_move = 0;
        }

        if (!queen_runtime_apply_cutaway_room_change(&object,
                                                     &applied_room,
                                                     status, status_size))
            return false;
        if (!queen_runtime_apply_cutaway_person_list(&object,
                                                     &applied_person,
                                                     status, status_size))
            return false;
        if (!queen_runtime_apply_cutaway_person_object(
                &object, &applied_person_object, status, status_size))
            return false;
        if (!queen_runtime_apply_cutaway_joe_object(&object, &applied_joe,
                                                    status, status_size))
            return false;
        if (!queen_runtime_apply_cutaway_object_copy(&object,
                                                     &applied_object,
                                                     status, status_size))
            return false;

        if ((object.object_number == -2 ||
             object.object_number == -3 ||
             object.object_number == -4 ||
             object.object_number == 0) &&
            sentence[0] != '\0' && sentence[0] != '*') {
            if (sentence[0] == '#')
                rb->snprintf(status, status_size,
                             "Queen cutaway credits %u: %.64s",
                             (unsigned)(index + 1), sentence + 1);
            else
                rb->snprintf(status, status_size,
                             "Queen cutaway line %u: %.70s",
                             (unsigned)(index + 1), sentence);
            queen_runtime_set_message(status);
            any_applied = true;
        }

        if (applied_special || applied_room || applied_person ||
            applied_person_object || applied_joe || applied_object)
            any_applied = true;

        object_off = next_object_off;
        if (queen_runtime_cutaway_object_has_animation(&object)) {
            bool applied_animation = false;

            if (!queen_runtime_apply_cutaway_animation(
                    object_off, game_state_off, payload_size, &object,
                    &object_off, &applied_animation, status, status_size) &&
                !queen_runtime_skip_cutaway_animation(object_off,
                                                      game_state_off,
                                                      payload_size,
                                                      &object_off))
                break;
            if (applied_animation)
                any_applied = true;
        }
    }

    if (!queen_runtime_apply_cutaway_tail(game_state_off, payload_size,
                                          &tail_applied, status,
                                          status_size))
        return false;

    *applied = any_applied || tail_applied;
    if (*applied && !tail_applied)
        rb->snprintf(status, status_size,
                     "Queen cutaway applied %u/%u object records",
                     (unsigned)index, (unsigned)object_count);
    return true;
}

static bool queen_runtime_finish_cutaway(const char *talk_file,
                                         bool applied_effects,
                                         char *status,
                                         size_t status_size)
{
    if (talk_file && queen_runtime_suffix_is(talk_file, ".DOG"))
        return queen_runtime_probe_script_resource_chained(talk_file, true,
                                                           status,
                                                           status_size);

    return applied_effects;
}

static bool queen_runtime_probe_cutaway_resource(const char *filename,
                                                 uint32_t payload_size,
                                                 char *status,
                                                 size_t status_size)
{
    uint16_t com_panel;
    uint16_t object_count;
    int16_t raw_object_count;
    int16_t flags;
    uint16_t game_state_off;
    uint16_t next_sentence_off;
    uint16_t object_sentence_off;
    uint16_t bank_names_off;
    uint16_t string_off;
    uint16_t bank_count;
    uint16_t i;
    char entry_string[96];
    char first_sentence[96];
    char bank_name[16];
    char talk_file[16];
    char candidate[16];
    struct queen_cutaway_object first_object;
    bool have_first_object = false;
    bool applied_effects = false;

    if (payload_size < 12) {
        rb->snprintf(status, status_size,
                     "Queen cutaway %.32s is too small",
                     filename);
        return false;
    }

    com_panel = read_be16_rt(queen_script_header);
    raw_object_count = read_be16s_rt(queen_script_header + 2);
    flags = read_be16s_rt(queen_script_header + 4);
    game_state_off = read_be16_rt(queen_script_header + 6);
    next_sentence_off = read_be16_rt(queen_script_header + 8);
    bank_names_off = read_be16_rt(queen_script_header + 10);
    object_count = queen_runtime_abs16(raw_object_count);

    if (!queen_runtime_script_offset_is_valid(game_state_off, payload_size) ||
        !queen_runtime_script_offset_is_valid(next_sentence_off, payload_size) ||
        !queen_runtime_script_offset_is_valid(bank_names_off, payload_size)) {
        rb->snprintf(status, status_size,
                     "Queen cutaway %.32s has invalid offsets",
                     filename);
        return false;
    }

    if (object_count > 128) {
        rb->snprintf(status, status_size,
                     "Queen cutaway %.32s has %u objects",
                     filename, (unsigned)object_count);
        return false;
    }

    if (object_count > 0) {
        uint16_t unused_next = 0;

        have_first_object =
            queen_runtime_read_cutaway_object(12, payload_size,
                                              &first_object,
                                              &unused_next);
    }

    entry_string[0] = '\0';
    first_sentence[0] = '\0';
    bank_name[0] = '\0';
    talk_file[0] = '\0';
    string_off = next_sentence_off;
    queen_runtime_read_talk_string_ex(&string_off, payload_size, 2,
                                      entry_string, sizeof(entry_string),
                                      true);
    object_sentence_off = string_off;
    queen_runtime_read_talk_string(&string_off, payload_size, 2,
                                   first_sentence,
                                   sizeof(first_sentence));

    if ((uint32_t)bank_names_off + 2 <= payload_size &&
        (size_t)bank_names_off + 2 <= sizeof(queen_script_header)) {
        string_off = bank_names_off;
        bank_count = read_be16_rt(queen_script_header + string_off);
        string_off += 2;
        if (bank_count > 5)
            bank_count = 5;
        for (i = 0; i < bank_count; i++) {
            candidate[0] = '\0';
            if (!queen_runtime_read_talk_string(&string_off, payload_size, 2,
                                                candidate,
                                                sizeof(candidate)))
                break;
            if (bank_name[0] == '\0' && candidate[0] != '\0')
                rb->strlcpy(bank_name, candidate, sizeof(bank_name));
        }
        queen_runtime_read_talk_string(&string_off, payload_size, 2,
                                       talk_file, sizeof(talk_file));
    }

    if (entry_string[0] != '\0') {
        if (entry_string[0] == '*' && entry_string[1] == 'F' &&
            entry_string[3] == '\0') {
            bool face_command = true;

            switch (entry_string[2]) {
            case 'L':
                queen_runtime_joe_facing = QUEEN_DIR_LEFT;
                break;
            case 'R':
                queen_runtime_joe_facing = QUEEN_DIR_RIGHT;
                break;
            case 'F':
                queen_runtime_joe_facing = QUEEN_DIR_FRONT;
                break;
            case 'B':
                queen_runtime_joe_facing = QUEEN_DIR_BACK;
                break;
            default:
                face_command = false;
                break;
            }

            if (face_command) {
                queen_runtime_dirty = true;
                if (!queen_runtime_redraw_scene(status, status_size))
                    return false;
                if (!queen_runtime_apply_cutaway_sequence(
                        object_count, game_state_off, object_sentence_off,
                        payload_size, &applied_effects, status, status_size))
                    return false;
                if (applied_effects)
                    return queen_runtime_finish_cutaway(talk_file,
                                                        applied_effects,
                                                        status,
                                                        status_size);
                rb->snprintf(status, status_size,
                             "Queen cutaway %.32s applied: %.8s",
                             filename, entry_string);
                return true;
            }
        }

        if (!queen_runtime_apply_cutaway_sequence(
                object_count, game_state_off, object_sentence_off,
                payload_size, &applied_effects, status, status_size))
            return false;
        if (applied_effects)
            return queen_runtime_finish_cutaway(talk_file, applied_effects,
                                                status, status_size);

        if (have_first_object && first_sentence[0] != '\0' &&
            (first_object.object_number == 0 ||
             first_object.object_number == -2 ||
             first_object.object_number == -3 ||
             first_object.object_number == -4)) {
            rb->snprintf(status, status_size,
                         "Queen cutaway %.32s line: %.70s",
                         filename, first_sentence);
            return true;
        }

        if (talk_file[0] != '\0' &&
            queen_runtime_suffix_is(talk_file, ".DOG"))
            return queen_runtime_probe_script_resource_chained(talk_file,
                                                               true,
                                                               status,
                                                               status_size);

        rb->snprintf(status, status_size,
                     "Queen cutaway %.32s: %.70s",
                     filename, entry_string);
        return true;
    }

    if (!queen_runtime_apply_cutaway_sequence(
            object_count, game_state_off, object_sentence_off, payload_size,
            &applied_effects, status, status_size))
        return false;
    if (applied_effects)
        return queen_runtime_finish_cutaway(talk_file, applied_effects,
                                            status, status_size);

    if (have_first_object && first_sentence[0] != '\0' &&
        (first_object.object_number == 0 ||
         first_object.object_number == -2 ||
         first_object.object_number == -3 ||
         first_object.object_number == -4)) {
        rb->snprintf(status, status_size,
                     "Queen cutaway %.32s line: %.70s",
                     filename, first_sentence);
        return true;
    }

    if (talk_file[0] != '\0' && queen_runtime_suffix_is(talk_file, ".DOG"))
        return queen_runtime_probe_script_resource_chained(talk_file, true,
                                                           status,
                                                           status_size);

    if (bank_name[0] != '\0' || talk_file[0] != '\0') {
        rb->snprintf(status, status_size,
                     "Queen cutaway %.32s verified: bank %.12s talk %.12s",
                     filename, bank_name, talk_file);
        return true;
    }

    rb->snprintf(status, status_size,
                 "Queen cutaway %.32s verified: panel %u objects %u flags %d",
                 filename, (unsigned)com_panel, (unsigned)object_count,
                 (int)flags);
    return true;
}

static bool queen_runtime_probe_dialog_resource(const char *filename,
                                                uint32_t payload_size,
                                                char *status,
                                                size_t status_size)
{
    int16_t raw_level_max;
    uint16_t level_max;
    int16_t unique_key;
    int16_t talk_key;
    int16_t joe_max;
    int16_t person_max;
    uint16_t person1_off;
    uint16_t cutaway_off;
    uint16_t person2_off;
    uint32_t tree_end;
    uint16_t string_off;
    uint16_t joe_ptr_off;
    uint16_t has_not_string;
    uint16_t option;
    uint16_t option_count;
    uint16_t talk_slot = 0;
    bool talk_slot_valid = false;
    bool has_talked = false;
    char joe_string[96];
    char person_string[96];
    char joe2_string[96];
    char first_option[96];

    if (payload_size < 32) {
        rb->snprintf(status, status_size,
                     "Queen dialog %.32s is too small",
                     filename);
        return false;
    }

    raw_level_max = read_be16s_rt(queen_script_header);
    level_max = queen_runtime_abs16(raw_level_max);
    unique_key = read_be16s_rt(queen_script_header + 2);
    talk_key = read_be16s_rt(queen_script_header + 4);
    joe_max = read_be16s_rt(queen_script_header + 6);
    person_max = read_be16s_rt(queen_script_header + 8);
    person1_off = read_be16_rt(queen_script_header + 26);
    cutaway_off = read_be16_rt(queen_script_header + 28);
    person2_off = read_be16_rt(queen_script_header + 30);
    tree_end = 32 + (uint32_t)level_max * 96;
    joe_ptr_off = (uint16_t)tree_end;
    if (unique_key >= 0 &&
        unique_key < QUEEN_RUNTIME_TALK_SELECTED_COUNT) {
        talk_slot = (uint16_t)unique_key;
        talk_slot_valid = true;
        has_talked = queen_runtime_talked_to[talk_slot];
    }

    if (level_max > 64 || tree_end > payload_size) {
        rb->snprintf(status, status_size,
                     "Queen dialog %.32s has invalid level count %u",
                     filename, (unsigned)level_max);
        return false;
    }

    if (!queen_runtime_script_offset_is_valid(person1_off, payload_size) ||
        !queen_runtime_script_offset_is_valid(cutaway_off, payload_size) ||
        !queen_runtime_script_offset_is_valid(person2_off, payload_size)) {
        rb->snprintf(status, status_size,
                     "Queen dialog %.32s has invalid offsets",
                     filename);
        return false;
    }

    joe_string[0] = '\0';
    person_string[0] = '\0';
    joe2_string[0] = '\0';
    first_option[0] = '\0';
    queen_runtime_clear_dialog_options();
    rb->strlcpy(queen_runtime_dialog_file, filename,
                sizeof(queen_runtime_dialog_file));
    queen_runtime_dialog_talk_slot = talk_slot;
    queen_runtime_dialog_level_max = level_max;
    queen_runtime_dialog_joe_ptr_off = joe_ptr_off;
    queen_runtime_dialog_person1_off = person1_off;
    queen_runtime_dialog_person2_off = person2_off;
    queen_runtime_dialog_cutaway_off = cutaway_off;
    queen_runtime_dialog_payload_size = payload_size;
    queen_runtime_dialog_joe_max = joe_max;
    queen_runtime_dialog_person_max = person_max;
    queen_runtime_dialog_talk_slot_valid = talk_slot_valid;
    queen_runtime_dialog_was_repeat = has_talked;
    for (option = 0; option < 2; option++) {
        uint16_t post_off = 10 + option * 6;

        queen_runtime_dialog_post_game_state[option] =
            read_be16s_rt(queen_script_header + post_off);
        queen_runtime_dialog_post_test_value[option] =
            read_be16s_rt(queen_script_header + post_off + 2);
        queen_runtime_dialog_post_item[option] =
            read_be16s_rt(queen_script_header + post_off + 4);
    }
    string_off = joe_ptr_off + 2;
    if ((uint32_t)string_off + 2 < payload_size &&
        (size_t)string_off + 2 < sizeof(queen_script_header)) {
        has_not_string = read_be16_rt(queen_script_header + string_off);
        string_off += 2;
        if (has_not_string == 0)
            queen_runtime_read_talk_string_ex(&string_off, payload_size, 2,
                                              joe_string,
                                              sizeof(joe_string),
                                              true);
    }

    string_off = person2_off;
    queen_runtime_read_talk_string(&string_off, payload_size, 2,
                                   person_string, sizeof(person_string));
    queen_runtime_read_talk_string_ex(&string_off, payload_size, 2,
                                      joe2_string, sizeof(joe2_string),
                                      true);

    option_count = 0;
    for (option = 1; option <= 4; option++) {
        uint32_t entry_off = 32 + (uint32_t)option * 16;
        int16_t head;
        int16_t dialogue_return;
        int16_t game_state_index;
        int16_t game_state_value;
        char option_text[96];
        uint16_t option_index = (uint16_t)(option - 1);

        if (entry_off + 16 > tree_end ||
            entry_off + 16 > sizeof(queen_script_header))
            break;

        head = read_be16s_rt(queen_script_header + entry_off + 2);
        dialogue_return =
            read_be16s_rt(queen_script_header + entry_off + 6);
        if (talk_slot_valid) {
            int16_t selected =
                queen_runtime_talk_values[talk_slot][option - 1];

            if (selected > 0)
                head = selected;
            else if (selected == -1)
                continue;
        }
        game_state_index =
            read_be16s_rt(queen_script_header + entry_off + 10);
        game_state_value =
            read_be16s_rt(queen_script_header + entry_off + 14);
        if (game_state_index < 0) {
            uint16_t slot = queen_runtime_abs16(game_state_index);

            if (slot >= QUEEN_RUNTIME_GAME_STATE_COUNT ||
                queen_runtime_game_state[slot] != game_state_value)
                continue;
        }

        if (queen_runtime_find_dialog_string(joe_ptr_off, head, joe_max,
                                             payload_size, option_text,
                                             sizeof(option_text)) &&
            option_text[0] != '\0') {
            option_count++;
            rb->strlcpy(queen_runtime_dialog_options[option_index],
                        option_text,
                        sizeof(queen_runtime_dialog_options[option_index]));
            queen_runtime_dialog_option_active[option_index] = true;
            queen_runtime_dialog_option_head[option_index] = head;
            queen_runtime_dialog_option_return[option_index] =
                dialogue_return;
            queen_runtime_dialog_option_game_state_index[option_index] =
                game_state_index;
            queen_runtime_dialog_option_game_state_value[option_index] =
                game_state_value;
            if (first_option[0] == '\0')
                rb->strlcpy(first_option, option_text,
                            sizeof(first_option));
        }
    }
    queen_runtime_dialog_option_count = option_count;

    if (!queen_runtime_dialog_auto_finish_if_exit_only(status, status_size))
        return false;
    if (queen_runtime_dialog_option_count == 0)
        return true;

    if (talk_slot_valid && !queen_runtime_talked_to[talk_slot]) {
        queen_runtime_talked_to[talk_slot] = true;
        queen_runtime_dirty = true;
    }

    if (has_talked && joe2_string[0] != '\0' && joe2_string[0] != '0') {
        if (first_option[0] != '\0') {
            rb->snprintf(status, status_size,
                         "Queen dialog %.32s: Joe %.45s | %u opts: %.35s",
                         filename, joe2_string, (unsigned)option_count,
                         first_option);
            return true;
        }
        rb->snprintf(status, status_size,
                     "Queen dialog %.32s: Joe %.70s",
                     filename, joe2_string);
        return true;
    }

    if (joe_string[0] != '\0' && joe_string[0] != '0') {
        if (first_option[0] != '\0') {
            rb->snprintf(status, status_size,
                         "Queen dialog %.32s: Joe %.45s | %u opts: %.35s",
                         filename, joe_string, (unsigned)option_count,
                         first_option);
            return true;
        }
        rb->snprintf(status, status_size,
                     "Queen dialog %.32s: Joe %.70s",
                     filename, joe_string);
        return true;
    }

    if (person_string[0] != '\0' && person_string[0] != '0') {
        if (first_option[0] != '\0') {
            rb->snprintf(status, status_size,
                         "Queen dialog %.32s: %.45s | %u opts: %.35s",
                         filename, person_string, (unsigned)option_count,
                         first_option);
            return true;
        }
        rb->snprintf(status, status_size,
                     "Queen dialog %.32s: %.70s",
                     filename, person_string);
        return true;
    }

    if (!has_talked && joe2_string[0] != '\0' && joe2_string[0] != '0') {
        if (first_option[0] != '\0') {
            rb->snprintf(status, status_size,
                         "Queen dialog %.32s: Joe %.45s | %u opts: %.35s",
                         filename, joe2_string, (unsigned)option_count,
                         first_option);
            return true;
        }
        rb->snprintf(status, status_size,
                     "Queen dialog %.32s: Joe %.70s",
                     filename, joe2_string);
        return true;
    }

    if (first_option[0] != '\0') {
        rb->snprintf(status, status_size,
                     "Queen dialog %.32s: %u opts: %.70s",
                     filename, (unsigned)option_count, first_option);
        return true;
    }

    rb->snprintf(status, status_size,
                 "Queen dialog %.32s verified: levels %u keys %d/%d npc %d",
                 filename, (unsigned)level_max, (int)unique_key,
                 (int)talk_key, (int)(joe_max + person_max));
    return true;
}

static bool queen_runtime_probe_script_resource(const char *filename,
                                                bool dialog,
                                                char *status,
                                                size_t status_size)
{
    uint32_t resource_size = 0;
    uint32_t payload_size;
    size_t header_size;
    bool ok;

    if (!dialog)
        queen_runtime_clear_dialog_options();

    if (!scummvm_queen_loader_read_resource(
            &queen_runtime_target, filename, QUEEN_SCRIPT_HEADER_SKIP,
            queen_script_header, sizeof(queen_script_header),
            &resource_size, status, status_size))
        return false;

    if (resource_size <= QUEEN_SCRIPT_HEADER_SKIP) {
        rb->snprintf(status, status_size,
                     "Queen script %.32s has no payload",
                     filename);
        return false;
    }

    payload_size = resource_size - QUEEN_SCRIPT_HEADER_SKIP;
    header_size = dialog ? 32 : 12;
    if (payload_size < header_size) {
        rb->snprintf(status, status_size,
                     "Queen script %.32s header is truncated",
                     filename);
        return false;
    }

    if (dialog)
        ok = queen_runtime_probe_dialog_resource(filename, payload_size,
                                                status, status_size);
    else
        ok = queen_runtime_probe_cutaway_resource(filename, payload_size,
                                                 status, status_size);

    if (ok)
        queen_runtime_set_message(status);
    return ok;
}

static bool queen_runtime_probe_script_resource_chained(const char *filename,
                                                        bool dialog,
                                                        char *status,
                                                        size_t status_size)
{
    char current[20];
    uint16_t count;

    if (!filename || filename[0] == '\0')
        return true;

    rb->strlcpy(current, filename, sizeof(current));
    for (count = 0; count < QUEEN_RUNTIME_MAX_CUTAWAY_CHAIN; count++) {
        bool current_is_dialog = dialog ||
            queen_runtime_suffix_is(current, ".DOG");

        if (!queen_runtime_probe_script_resource(current, current_is_dialog,
                                                 status, status_size))
            return false;
        if (!current_is_dialog)
            return true;
        if (queen_runtime_dialog_file[0] == '\0' ||
            !queen_runtime_suffix_is(status, ".CUT"))
            return true;
        rb->strlcpy(current, status, sizeof(current));
        dialog = false;
    }

    rb->snprintf(status, status_size,
                 "Queen cutaway chain exceeded %u steps",
                 (unsigned)QUEEN_RUNTIME_MAX_CUTAWAY_CHAIN);
    queen_runtime_set_message(status);
    return false;
}

static bool queen_runtime_show_object_description(uint16_t description,
                                                  char *status,
                                                  size_t status_size)
{
    char text[128];

    if (description == 0 || description > queen_runtime_jas.descriptions) {
        rb->snprintf(status, status_size,
                     "Queen description %u is invalid",
                     (unsigned)description);
        return true;
    }

    if (!scummvm_queen_loader_read_text_line(
            &queen_runtime_target,
            queen_runtime_text.object_description_offset + description - 1,
            text, sizeof(text), status, status_size))
        return false;

    if (queen_runtime_suffix_is(text, ".CUT"))
        return queen_runtime_probe_script_resource_chained(text, false,
                                                           status,
                                                           status_size);
    else if (queen_runtime_suffix_is(text, ".DOG"))
        return queen_runtime_probe_script_resource_chained(text, true,
                                                           status,
                                                           status_size);
    else
        rb->snprintf(status, status_size, "Joe: %.110s", text);

    queen_runtime_set_message(status);
    return true;
}

static bool queen_runtime_show_joe_response(uint16_t response,
                                            char *status,
                                            size_t status_size)
{
    char text[128];

    if (response == 0 || response > 40) {
        rb->snprintf(status, status_size,
                     "Queen Joe response %u is invalid",
                     (unsigned)response);
        return true;
    }

    if (!scummvm_queen_loader_read_text_line(
            &queen_runtime_target,
            queen_runtime_text.joe_response_offset + response - 1,
            text, sizeof(text), status, status_size))
        return false;

    rb->snprintf(status, status_size, "Joe: %.110s", text);
    queen_runtime_clear_dialog_options();
    queen_runtime_set_message(status);
    return true;
}

static bool queen_runtime_wrong_action(uint16_t verb,
                                       uint16_t object_index,
                                       char *status,
                                       size_t status_size)
{
    int16_t image = 0;

    if (object_index != 0 && object_index <= queen_runtime_jas.objects)
        image = queen_runtime_objects[object_index].image;

    switch (verb) {
    case QUEEN_VERB_OPEN:
        return queen_runtime_show_joe_response(1, status, status_size);
    case QUEEN_VERB_CLOSE:
    case QUEEN_VERB_USE:
        return queen_runtime_show_joe_response(2, status, status_size);
    case QUEEN_VERB_MOVE:
        if (image == -4 || image == -3)
            return queen_runtime_show_joe_response(18, status, status_size);
        return queen_runtime_show_joe_response(3, status, status_size);
    case QUEEN_VERB_TALK_TO:
        return queen_runtime_show_joe_response(
            24 + (uint16_t)(rb->rand() % 3), status, status_size);
    case QUEEN_VERB_GIVE:
        return queen_runtime_show_joe_response(12, status, status_size);
    case QUEEN_VERB_PICK_UP:
        if (image == -4 || image == -3)
            return queen_runtime_show_joe_response(20, status, status_size);
        return queen_runtime_show_joe_response(
            5 + (uint16_t)(rb->rand() % 4), status, status_size);
    default:
        break;
    }

    rb->snprintf(status, status_size,
                 "Queen verb %u object %u has no command",
                 (unsigned)verb, (unsigned)object_index);
    return true;
}

static bool queen_runtime_handle_inventory_input(int x,
                                                 int y,
                                                 bool clicked,
                                                 bool *handled,
                                                 char *status,
                                                 size_t status_size)
{
    uint16_t slot;
    uint16_t item;
    char name[48];

    *handled = false;
    if (x < 178 || y < 2 || y >= 60)
        return true;

    slot = (uint16_t)((y - 14) / 8);
    if (y < 14 || slot >= SCUMMVM_QUEEN_INVENTORY_SLOTS)
        return true;

    *handled = true;
    item = queen_runtime_inventory_slots[slot];
    if (!queen_runtime_item_visible(item)) {
        rb->snprintf(status, status_size,
                     "Queen inventory slot %u is empty",
                     (unsigned)(slot + 1));
        return true;
    }

    if (!scummvm_queen_loader_read_text_line(
            &queen_runtime_target,
            queen_runtime_text.object_name_offset +
            queen_runtime_items[item].name - 1,
            name, sizeof(name), status, status_size))
        return false;

    if (clicked) {
        if (queen_runtime_selected_inventory_item == item ||
            queen_runtime_selected_verb != QUEEN_VERB_NONE) {
            uint16_t verb = queen_runtime_selected_verb;

            queen_runtime_selected_inventory_item = 0;
            queen_runtime_selected_verb = QUEEN_VERB_NONE;
            if (verb == QUEEN_VERB_NONE)
                verb = queen_runtime_default_verb(
                    queen_runtime_items[item].state);
            if (verb == QUEEN_VERB_NONE)
                verb = QUEEN_VERB_LOOK_AT;
            return queen_runtime_execute_inventory_item_verb(
                item, verb, status, status_size);
        } else {
            queen_runtime_selected_inventory_item = item;
            rb->snprintf(status, status_size,
                         "Queen selected item: %.40s", name);
            queen_runtime_set_message(status);
        }
    } else {
        rb->snprintf(status, status_size,
                     "Queen inventory: %.40s", name);
    }

    return true;
}

static bool queen_runtime_handle_dialog_input(int x,
                                              int y,
                                              bool clicked,
                                              bool *handled,
                                              char *status,
                                              size_t status_size)
{
    uint16_t option;
    uint16_t slot;
    int16_t selected_head;
    int16_t selected_return;
    int16_t selected_game_state_index;
    int16_t selected_game_state_value;
    int top = 176;

    *handled = false;
    if (queen_runtime_dialog_option_count == 0 ||
        x < 6 || x >= 314 || y < top || y >= top + 58)
        return true;

    option = (uint16_t)((y - top - 14) / 10);
    if (y < top + 14 || option >= SCUMMVM_QUEEN_DIALOG_OPTIONS ||
        !queen_runtime_dialog_option_active[option])
        return true;

    *handled = true;
    if (!clicked) {
        rb->snprintf(status, status_size,
                     "Queen dialog option %u: %.52s",
                     (unsigned)(option + 1),
                     queen_runtime_dialog_options[option]);
        queen_runtime_set_message(status);
        return true;
    }

    selected_head = queen_runtime_dialog_option_head[option];
    selected_return = queen_runtime_dialog_option_return[option];
    selected_game_state_index =
        queen_runtime_dialog_option_game_state_index[option];
    selected_game_state_value =
        queen_runtime_dialog_option_game_state_value[option];
    rb->snprintf(status, status_size, "Joe: %.58s",
                 queen_runtime_dialog_options[option]);
    queen_runtime_set_message(status);

    {
        uint16_t child_level = 0;
        char reply[96];

        reply[0] = '\0';
        if (queen_runtime_dialog_was_repeat && selected_head == 1) {
            uint16_t repeat_off = queen_runtime_dialog_person2_off;

            queen_runtime_read_talk_string(
                &repeat_off, queen_runtime_dialog_payload_size, 2,
                reply, sizeof(reply));
        } else {
            queen_runtime_find_dialog_string(
                queen_runtime_dialog_person1_off, selected_head,
                queen_runtime_dialog_person_max,
                queen_runtime_dialog_payload_size, reply, sizeof(reply));
        }
        if (reply[0] == '\0' && selected_return > 1)
            queen_runtime_find_dialog_string(
                queen_runtime_dialog_person1_off, selected_return,
                queen_runtime_dialog_person_max,
                queen_runtime_dialog_payload_size, reply, sizeof(reply));
        if (reply[0] != '\0' && reply[0] != '0') {
            rb->snprintf(status, status_size, "Queen dialog: %.62s",
                         reply);
            queen_runtime_set_message(status);
        }

        if (queen_runtime_dialog_find_level(selected_head, &child_level) ||
            queen_runtime_dialog_find_level_containing(selected_return,
                                                       &child_level)) {
            queen_runtime_dialog_apply_level_state(child_level);
            queen_runtime_dialog_load_level_options(child_level);
            if (!queen_runtime_dialog_auto_finish_if_exit_only(status,
                                                               status_size))
                return false;
        } else if (selected_return == -1) {
            queen_runtime_clear_dialog_options();
        }
    }

    if (queen_runtime_dialog_talk_slot_valid) {
        int16_t selected_value =
            queen_runtime_dialog_child_selected_value(selected_head);

        slot = queen_runtime_dialog_talk_slot;
        if (slot < QUEEN_RUNTIME_TALK_SELECTED_COUNT) {
            queen_runtime_talked_to[slot] = true;
            queen_runtime_talk_values[slot][option] =
                selected_value > 0 ? selected_value : -1;
            queen_runtime_dirty = true;
        }
    }

    if (selected_game_state_index > 0) {
        uint16_t state = (uint16_t)selected_game_state_index;

        if (state < QUEEN_RUNTIME_GAME_STATE_COUNT) {
            queen_runtime_game_state[state] = selected_game_state_value;
            queen_runtime_dirty = true;
        }
    }

    if (selected_return == -1 ||
        queen_runtime_dialog_file[0] == '\0') {
        return queen_runtime_finish_dialog(status, status_size);
    }

    return true;
}

static bool queen_runtime_dialog_auto_finish_if_exit_only(char *status,
                                                          size_t status_size)
{
    uint16_t option;
    uint16_t active = 0;
    uint16_t active_option = 0;

    for (option = 0; option < SCUMMVM_QUEEN_DIALOG_OPTIONS; option++) {
        if (!queen_runtime_dialog_option_active[option])
            continue;
        active++;
        active_option = option;
    }

    if (active != 1 ||
        queen_runtime_dialog_option_return[active_option] != -1)
        return true;

    rb->snprintf(status, status_size,
                 "Queen dialog auto-exit: %.58s",
                 queen_runtime_dialog_options[active_option]);
    queen_runtime_set_message(status);

    if (queen_runtime_dialog_talk_slot_valid) {
        uint16_t slot = queen_runtime_dialog_talk_slot;

        if (slot < QUEEN_RUNTIME_TALK_SELECTED_COUNT) {
            queen_runtime_talked_to[slot] = true;
            queen_runtime_talk_values[slot][active_option] = -1;
            queen_runtime_dirty = true;
        }
    }

    return queen_runtime_finish_dialog(status, status_size);
}

static bool queen_runtime_finish_dialog(char *status, size_t status_size)
{
    uint16_t index;

    for (index = 0; index < 2; index++) {
        int16_t game_state = queen_runtime_dialog_post_game_state[index];
        int16_t item = queen_runtime_dialog_post_item[index];

        if (game_state <= 0 || item == 0)
            continue;
        if (game_state >= QUEEN_RUNTIME_GAME_STATE_COUNT ||
            queen_runtime_game_state[(uint16_t)game_state] !=
                queen_runtime_dialog_post_test_value[index])
            continue;

        if (item > 0) {
            if (!queen_runtime_add_inventory_item((uint16_t)item, status,
                                                  status_size))
                return false;
        } else {
            if (!queen_runtime_delete_inventory_item(queen_runtime_abs16(item),
                                                     status, status_size))
                return false;
        }
    }

    if (queen_runtime_script_offset_is_valid(
            queen_runtime_dialog_cutaway_off,
            queen_runtime_dialog_payload_size) &&
        (uint32_t)queen_runtime_dialog_cutaway_off + 4 <=
            queen_runtime_dialog_payload_size &&
        (size_t)queen_runtime_dialog_cutaway_off + 4 <=
            sizeof(queen_script_header)) {
        uint16_t offset = queen_runtime_dialog_cutaway_off;
        int16_t game_state = read_be16s_rt(queen_script_header + offset);
        int16_t test_value = read_be16s_rt(queen_script_header + offset + 2);
        char cutaway_file[20];

        offset += 4;
        cutaway_file[0] = '\0';
        if (game_state > 0 &&
            game_state < QUEEN_RUNTIME_GAME_STATE_COUNT &&
            queen_runtime_game_state[(uint16_t)game_state] == test_value &&
            queen_runtime_read_talk_string(&offset,
                                           queen_runtime_dialog_payload_size,
                                           2, cutaway_file,
                                           sizeof(cutaway_file)) &&
            queen_runtime_suffix_is(cutaway_file, ".CUT"))
            return queen_runtime_probe_script_resource_chained(
                cutaway_file, false, status, status_size);
    }

    queen_runtime_clear_dialog_options();
    rb->strlcpy(status, "Queen dialog complete", status_size);
    queen_runtime_set_message(status);
    return true;
}

static bool queen_runtime_describe_object(uint16_t object_index,
                                          char *status,
                                          size_t status_size)
{
    struct scummvm_queen_object_data *object;
    uint16_t description;

    if (object_index == 0 || object_index > queen_runtime_jas.objects)
        return true;

    object = &queen_runtime_objects[object_index];
    description = queen_runtime_next_description(object_index,
                                                 object->description);
    if (description == 0) {
        rb->snprintf(status, status_size,
                     "Queen object %u has no description",
                     (unsigned)object_index);
        return true;
    }

    return queen_runtime_show_object_description(description, status,
                                                status_size);
}

static bool queen_runtime_execute_object_verb(uint16_t object_index,
                                              uint16_t verb,
                                              char *status,
                                              size_t status_size)
{
    struct scummvm_queen_command_match_summary matches;
    uint16_t command_index;
    uint16_t match_seen = 0;
    uint16_t applied_command = 0;
    bool handled = false;
    bool applied = false;

    if (object_index == 0 || object_index > queen_runtime_jas.objects)
        return false;
    if (!queen_runtime_verb_is_selectable(verb) ||
        verb == QUEEN_VERB_NONE)
        return false;

    if (!scummvm_queen_loader_find_commands(&queen_runtime_target,
                                            &queen_runtime_jas,
                                            verb, (int16_t)object_index,
                                            0, &matches,
                                            status, status_size))
        return false;
    if (matches.matches == 0) {
        if (verb == QUEEN_VERB_LOOK_AT)
            return queen_runtime_describe_object(object_index, status,
                                                 status_size);
        if (verb == QUEEN_VERB_WALK_TO)
            return queen_runtime_follow_entry_object(object_index, status,
                                                     status_size);
        return queen_runtime_wrong_action(verb, object_index, status,
                                          status_size);
    }

    for (command_index = matches.first_match;
         command_index <= matches.last_match;
         command_index++) {
        struct scummvm_queen_command_list_data command;
        bool last_match;

        if (!scummvm_queen_loader_read_command_list(
                &queen_runtime_target, &queen_runtime_jas, command_index,
                &command, status, status_size))
            return false;
        if (command.verb != verb ||
            command.noun_object_1 != (int16_t)object_index ||
            command.noun_object_2 != 0)
            continue;

        match_seen++;
        if (command_index == 649)
            continue;

        last_match = match_seen >= matches.matches;
        if (!queen_runtime_execute_command_match(command_index, last_match,
                                                 &handled, &applied,
                                                 status, status_size))
            return false;
        if (handled) {
            if (applied)
                applied_command = command_index;
            break;
        }
    }

    if (applied_command != 0 &&
        !queen_runtime_apply_subject_command_effects(applied_command,
                                                     verb, object_index,
                                                     status, status_size))
        return false;
    if (queen_runtime_dirty && !queen_runtime_redraw_scene(status,
                                                          status_size))
        return false;

    if (!handled) {
        if (verb == QUEEN_VERB_LOOK_AT)
            return queen_runtime_describe_object(object_index, status,
                                                 status_size);
        return queen_runtime_wrong_action(verb, object_index, status,
                                          status_size);
    }

    if (verb == QUEEN_VERB_WALK_TO)
        return queen_runtime_follow_entry_object(object_index, status,
                                                 status_size);
    if (verb == QUEEN_VERB_LOOK_AT && applied_command != 0)
        return queen_runtime_describe_object(object_index, status,
                                             status_size);

    return true;
}

static bool queen_runtime_execute_object_default(uint16_t object_index,
                                                 char *status,
                                                 size_t status_size)
{
    uint16_t verb;

    if (object_index == 0 || object_index > queen_runtime_jas.objects)
        return false;

    verb = queen_runtime_default_verb(queen_runtime_objects[object_index].state);
    if (verb == QUEEN_VERB_NONE)
        verb = QUEEN_VERB_WALK_TO;

    return queen_runtime_execute_object_verb(object_index, verb,
                                             status, status_size);
}

static bool queen_runtime_execute_inventory_item_verb(uint16_t item,
                                                      uint16_t verb,
                                                      char *status,
                                                      size_t status_size)
{
    struct scummvm_queen_command_match_summary matches;
    uint16_t command_index;
    uint16_t match_seen = 0;
    uint16_t applied_command = 0;
    int16_t item_subject;
    bool handled = false;
    bool applied = false;

    if (item == 0 || item > queen_runtime_jas.items ||
        !queen_runtime_item_visible(item))
        return true;
    if (!queen_runtime_verb_is_selectable(verb) ||
        verb == QUEEN_VERB_NONE)
        return true;

    if (item > 32767) {
        rb->snprintf(status, status_size,
                     "Queen inventory item %u is out of command range",
                     (unsigned)item);
        return false;
    }
    item_subject = -(int16_t)item;

    if (!scummvm_queen_loader_find_commands(&queen_runtime_target,
                                            &queen_runtime_jas,
                                            verb, item_subject, 0,
                                            &matches,
                                            status, status_size))
        return false;
    if (matches.matches == 0) {
        if (verb == QUEEN_VERB_LOOK_AT)
            return queen_runtime_show_object_description(
                queen_runtime_items[item].description, status, status_size);

        rb->snprintf(status, status_size,
                     "Queen item %u has no %s command",
                     (unsigned)item, queen_runtime_verb_name(verb));
        queen_runtime_set_message(status);
        return true;
    }

    for (command_index = matches.first_match;
         command_index <= matches.last_match;
         command_index++) {
        struct scummvm_queen_command_list_data command;
        bool last_match;

        if (!scummvm_queen_loader_read_command_list(
                &queen_runtime_target, &queen_runtime_jas, command_index,
                &command, status, status_size))
            return false;
        if (command.verb != verb ||
            command.noun_object_1 != item_subject ||
            command.noun_object_2 != 0)
            continue;

        match_seen++;
        last_match = match_seen >= matches.matches;
        if (!queen_runtime_execute_command_match(command_index, last_match,
                                                 &handled, &applied,
                                                 status, status_size))
            return false;
        if (handled) {
            if (applied)
                applied_command = command_index;
            break;
        }
    }

    if (applied_command != 0 &&
        !queen_runtime_apply_subject_command_effects(applied_command,
                                                     verb, item_subject,
                                                     status, status_size))
        return false;
    if (queen_runtime_dirty && !queen_runtime_redraw_scene(status,
                                                          status_size))
        return false;
    if (!handled) {
        if (verb == QUEEN_VERB_LOOK_AT)
            return queen_runtime_show_object_description(
                queen_runtime_items[item].description, status, status_size);

        rb->snprintf(status, status_size,
                     "Queen item %u has no %s command",
                     (unsigned)item, queen_runtime_verb_name(verb));
        queen_runtime_set_message(status);
    }

    return true;
}

static bool queen_runtime_execute_item_on_object(uint16_t item,
                                                 uint16_t object_index,
                                                 char *status,
                                                 size_t status_size)
{
    struct scummvm_queen_command_match_summary matches;
    uint16_t command_index;
    uint16_t match_seen = 0;
    uint16_t applied_command = 0;
    uint16_t verb = QUEEN_VERB_USE;
    int16_t item_subject;
    bool handled = false;
    bool applied = false;

    if (item == 0 || item > queen_runtime_jas.items ||
        object_index == 0 || object_index > queen_runtime_jas.objects)
        return true;
    if (item > 32767) {
        rb->snprintf(status, status_size,
                     "Queen inventory item %u is out of command range",
                     (unsigned)item);
        return false;
    }
    item_subject = -(int16_t)item;

    if (!scummvm_queen_loader_find_commands(&queen_runtime_target,
                                            &queen_runtime_jas,
                                            QUEEN_VERB_USE,
                                            (int16_t)object_index,
                                            item_subject,
                                            &matches,
                                            status, status_size))
        return false;
    if (matches.matches == 0 &&
        !scummvm_queen_loader_find_commands(&queen_runtime_target,
                                            &queen_runtime_jas,
                                            QUEEN_VERB_USE,
                                            item_subject,
                                            (int16_t)object_index,
                                            &matches,
                                            status, status_size))
        return false;
    if (matches.matches == 0) {
        verb = QUEEN_VERB_GIVE;
        if (!scummvm_queen_loader_find_commands(&queen_runtime_target,
                                                &queen_runtime_jas,
                                                verb,
                                                (int16_t)object_index,
                                                item_subject,
                                                &matches,
                                                status, status_size))
            return false;
        if (matches.matches == 0 &&
            !scummvm_queen_loader_find_commands(&queen_runtime_target,
                                                &queen_runtime_jas,
                                                verb,
                                                item_subject,
                                                (int16_t)object_index,
                                                &matches,
                                                status, status_size))
            return false;
    }
    if (matches.matches == 0) {
        char item_name[48];

        if (!scummvm_queen_loader_read_text_line(
                &queen_runtime_target,
                queen_runtime_text.object_name_offset +
                queen_runtime_items[item].name - 1,
                item_name, sizeof(item_name), status, status_size))
            return false;
        rb->snprintf(status, status_size,
                     "Queen item %.32s: no command", item_name);
        queen_runtime_set_message(status);
        return true;
    }

    for (command_index = matches.first_match;
         command_index <= matches.last_match;
         command_index++) {
        struct scummvm_queen_command_list_data command;
        bool forward_match;
        bool reverse_match;
        bool last_match;

        if (!scummvm_queen_loader_read_command_list(
                &queen_runtime_target, &queen_runtime_jas, command_index,
                &command, status, status_size))
            return false;

        forward_match =
            command.verb == verb &&
            command.noun_object_1 == (int16_t)object_index &&
            command.noun_object_2 == item_subject;
        reverse_match =
            command.verb == verb &&
            command.noun_object_1 == item_subject &&
            command.noun_object_2 == (int16_t)object_index;
        if (!forward_match && !reverse_match)
            continue;

        match_seen++;
        last_match = match_seen >= matches.matches;
        if (!queen_runtime_execute_command_match(command_index, last_match,
                                                 &handled, &applied,
                                                 status, status_size))
            return false;
        if (handled) {
            if (applied)
                applied_command = command_index;
            break;
        }
    }

    if (applied_command != 0 &&
        !queen_runtime_apply_subject_command_effects(applied_command,
                                                     verb,
                                                     (int16_t)object_index,
                                                     status, status_size))
        return false;
    if (queen_runtime_dirty && !queen_runtime_redraw_scene(status,
                                                          status_size))
        return false;
    if (!handled)
        return queen_runtime_wrong_action(verb, object_index,
                                          status, status_size);

    return true;
}

static bool queen_runtime_click_object(uint16_t object_index,
                                       char *status,
                                       size_t status_size)
{
    struct scummvm_queen_object_data object;
    char name[48];

    if (object_index == 0 || object_index > queen_runtime_jas.objects)
        return true;

    object = queen_runtime_objects[object_index];
    if (!scummvm_queen_loader_read_text_line(
            &queen_runtime_target,
            queen_runtime_text.object_name_offset + object.name - 1,
            name, sizeof(name), status, status_size))
        return false;

    rb->snprintf(status, status_size, "Queen click: %.44s", name);
    if (queen_runtime_selected_inventory_item != 0) {
        uint16_t item = queen_runtime_selected_inventory_item;

        queen_runtime_selected_inventory_item = 0;
        return queen_runtime_execute_item_on_object(item, object_index,
                                                    status, status_size);
    }
    if (queen_runtime_selected_verb != QUEEN_VERB_NONE) {
        rb->snprintf(status, status_size, "Queen %s: %.36s",
                     queen_runtime_verb_name(queen_runtime_selected_verb),
                     name);
        queen_runtime_set_message(status);
        return queen_runtime_execute_object_verb(
            object_index, queen_runtime_selected_verb, status, status_size);
    }

    return queen_runtime_execute_object_default(object_index, status,
                                                status_size);
}

static void queen_runtime_tick_joe(void)
{
    int dx;
    int dy;

    if (!queen_runtime_joe_walking)
        return;

    queen_runtime_joe_walk_step++;
    if (queen_runtime_joe_walk_step >= queen_runtime_joe_walk_steps) {
        queen_runtime_joe_x = queen_runtime_joe_target_x;
        queen_runtime_joe_y = queen_runtime_joe_target_y;
        queen_runtime_joe_walking = false;
        return;
    }

    dx = queen_runtime_joe_target_x - queen_runtime_joe_start_x;
    dy = queen_runtime_joe_target_y - queen_runtime_joe_start_y;
    queen_runtime_joe_x = queen_runtime_joe_start_x +
        (int16_t)((dx * queen_runtime_joe_walk_step) /
                  queen_runtime_joe_walk_steps);
    queen_runtime_joe_y = queen_runtime_joe_start_y +
        (int16_t)((dy * queen_runtime_joe_walk_step) /
                  queen_runtime_joe_walk_steps);
}

static void queen_runtime_tick(char *status, size_t status_size)
{
    char scratch[96];

    (void)status;
    (void)status_size;
    if (!queen_runtime_scene_has_animation && !queen_runtime_joe_walking)
        return;

    queen_runtime_anim_tick++;
    queen_runtime_tick_joe();
    if ((queen_runtime_anim_tick & 3) == 0)
        queen_runtime_redraw_scene(scratch, sizeof(scratch));
}

void scummvm_queen_runtime_input(int x,
                                 int y,
                                 bool clicked,
                                 char *status,
                                 size_t status_size)
{
    uint16_t object_index;
    uint16_t selected = 0;
    bool inventory_handled = false;
    bool dialog_handled = false;

    if (!queen_runtime_active)
        return;

    queen_runtime_tick(status, status_size);
    if (!queen_runtime_handle_dialog_input(x, y, clicked, &dialog_handled,
                                           status, status_size))
        return;
    if (dialog_handled)
        return;

    if (!queen_runtime_handle_inventory_input(x, y, clicked,
                                              &inventory_handled,
                                              status, status_size))
        return;
    if (inventory_handled)
        return;

    for (object_index = queen_runtime_range.first_object;
         queen_runtime_range.first_object <= queen_runtime_range.last_object &&
         object_index <= queen_runtime_range.last_object;
         object_index++) {
        struct scummvm_queen_box box;
        struct scummvm_queen_object_data object;
        int left;
        int right;
        int top;
        int bottom;

        if (!scummvm_queen_loader_read_object_box(&queen_runtime_target,
                                                  &queen_runtime_jas,
                                                  object_index, &box,
                                                  status, status_size))
            return;
        object = queen_runtime_objects[object_index];
        if (object.name <= 0)
            continue;

        left = box.x1 < box.x2 ? box.x1 : box.x2;
        right = box.x1 < box.x2 ? box.x2 : box.x1;
        top = box.y1 < box.y2 ? box.y1 : box.y2;
        bottom = box.y1 < box.y2 ? box.y2 : box.y1;
        if (x >= left && x <= right && y >= top && y <= bottom) {
            selected = object_index;
            break;
        }
    }

    if (selected != queen_runtime_selected_object) {
        queen_runtime_selected_object = selected;
        if (selected == 0) {
            if (clicked && y < 176) {
                uint16_t area =
                    queen_runtime_find_area_for_position(x, y);

                queen_runtime_set_joe_target((uint16_t)x, (uint16_t)y,
                                             queen_runtime_joe_facing);
                rb->snprintf(status, status_size,
                             "Queen walk to %d,%d", x, y);
                queen_runtime_set_message(status);
                if (!queen_runtime_apply_special_area(area, status,
                                                      status_size))
                    return;
            } else {
                rb->strlcpy(status, "Queen backdrop ready", status_size);
            }
        } else {
            struct scummvm_queen_object_data object;
            char name[48];

            object = queen_runtime_objects[selected];
            if (!scummvm_queen_loader_read_text_line(
                    &queen_runtime_target,
                    queen_runtime_text.object_name_offset + object.name - 1,
                    name, sizeof(name), status, status_size))
                return;

            rb->snprintf(status, status_size,
                         clicked ? "Queen click: %.44s" :
                         "Queen hover: %.44s",
                         name);
            if (clicked)
                queen_runtime_click_object(selected, status, status_size);
        }
    }
    else if (clicked && selected != 0) {
        queen_runtime_click_object(selected, status, status_size);
    } else if (clicked && selected == 0 && y < 176) {
        uint16_t area = queen_runtime_find_area_for_position(x, y);

        queen_runtime_set_joe_target((uint16_t)x, (uint16_t)y,
                                     queen_runtime_joe_facing);
        rb->snprintf(status, status_size, "Queen walk to %d,%d", x, y);
        queen_runtime_set_message(status);
        if (!queen_runtime_apply_special_area(area, status, status_size))
            return;
    }
}

void scummvm_queen_runtime_cycle_verb(int direction,
                                      char *status,
                                      size_t status_size)
{
    if (!queen_runtime_active)
        return;

    queen_runtime_selected_inventory_item = 0;
    queen_runtime_selected_verb =
        queen_runtime_cycle_verb_value(queen_runtime_selected_verb,
                                       direction);
    rb->snprintf(status, status_size, "Queen verb: %s",
                 queen_runtime_verb_name(queen_runtime_selected_verb));
    queen_runtime_set_message(status);
}

void scummvm_queen_runtime_cycle_inventory(int direction,
                                           char *status,
                                           size_t status_size)
{
    uint16_t first;

    if (!queen_runtime_active)
        return;

    if (direction < 0)
        first = queen_runtime_previous_inventory_item(
            queen_runtime_inventory_page_first);
    else
        first = queen_runtime_next_inventory_item(
            queen_runtime_inventory_slots[SCUMMVM_QUEEN_INVENTORY_SLOTS - 1]);
    if (first == 0) {
        queen_runtime_set_message("Queen inventory is empty");
        rb->strlcpy(status, "Queen inventory is empty", status_size);
        return;
    }

    queen_runtime_selected_inventory_item = 0;
    queen_runtime_refresh_inventory_slots(first);
    rb->snprintf(status, status_size, "Queen inventory page: item %u",
                 (unsigned)first);
    queen_runtime_set_message(status);
}

bool scummvm_queen_runtime_overlay(struct scummvm_queen_overlay *overlay)
{
    uint16_t index;

    if (!overlay)
        return false;

    rb->memset(overlay, 0, sizeof(*overlay));
    if (!queen_runtime_active)
        return false;

    overlay->active = true;
    rb->snprintf(overlay->verb, sizeof(overlay->verb), "Verb: %s",
                 queen_runtime_verb_name(queen_runtime_selected_verb));
    rb->strlcpy(overlay->message, queen_runtime_message,
                sizeof(overlay->message));
    overlay->option_count = queen_runtime_dialog_option_count;
    for (index = 0; index < SCUMMVM_QUEEN_DIALOG_OPTIONS; index++) {
        rb->strlcpy(overlay->options[index],
                    queen_runtime_dialog_options[index],
                    sizeof(overlay->options[index]));
        overlay->option_active[index] =
            queen_runtime_dialog_option_active[index];
    }

    for (index = 0; index < SCUMMVM_QUEEN_INVENTORY_SLOTS; index++) {
        uint16_t item = queen_runtime_inventory_slots[index];
        char item_name[SCUMMVM_QUEEN_OVERLAY_TEXT];

        if (!queen_runtime_item_visible(item)) {
            rb->strlcpy(overlay->inventory[index], "-",
                        sizeof(overlay->inventory[index]));
            continue;
        }

        if (!scummvm_queen_loader_read_text_line(
                &queen_runtime_target,
                queen_runtime_text.object_name_offset +
                queen_runtime_items[item].name - 1,
                item_name,
                sizeof(item_name),
                item_name,
                sizeof(item_name))) {
            rb->snprintf(overlay->inventory[index],
                         sizeof(overlay->inventory[index]),
                         "Item %u", (unsigned)item);
            continue;
        }
        rb->snprintf(overlay->inventory[index],
                     sizeof(overlay->inventory[index]),
                     item == queen_runtime_selected_inventory_item ?
                     "*%.58s" : "%.59s", item_name);
    }

    return true;
}
