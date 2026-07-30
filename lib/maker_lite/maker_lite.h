/***************************************************************************
 * Portable deterministic core for Rockpod Maker Lite.
 *
 * This file intentionally has no Rockbox, libc allocation, display, audio,
 * filesystem, or host UI dependencies.  The same source is built into the
 * native plugin and the Rockpod preview/test helper.
 ***************************************************************************/

#ifndef MAKER_LITE_H
#define MAKER_LITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ML_PACK_MAGIC "RPML"
#define ML_PACK_VERSION 3
#define ML_PACK_HEADER_SIZE 128
#define ML_PACK_ENTITY_V1_SIZE 16
#define ML_PACK_ENTITY_SIZE 18

#define ML_MAX_MAP_WIDTH 512
#define ML_MAX_MAP_HEIGHT 64
#define ML_MAX_ENTITIES 192
#define ML_MAX_EVENTS 256
#define ML_MAX_PATHS 64
#define ML_MAX_PATH_POINTS 512
#define ML_MAX_LOOSE_RINGS 16
#define ML_MAX_TITLE 80
#define ML_ID_SIZE 32
#define ML_PACK_EVENT_SIZE 24
#define ML_PACK_PATH_SIZE 12
#define ML_PACK_PATH_POINT_SIZE 4

#define ML_LEVEL_LIFE_SIM (1u << 0)
#define ML_LEVEL_BRAWL (1u << 1)

#define ML_FIXED_SHIFT 16
#define ML_FIXED_ONE (1 << ML_FIXED_SHIFT)

enum ml_ruleset {
    ML_RULESET_MARIO = 1,
    ML_RULESET_ZELDA = 2,
    ML_RULESET_SONIC = 3,
};

enum ml_input {
    ML_INPUT_LEFT = 1u << 0,
    ML_INPUT_RIGHT = 1u << 1,
    ML_INPUT_UP = 1u << 2,
    ML_INPUT_DOWN = 1u << 3,
    ML_INPUT_PRIMARY = 1u << 4,
    ML_INPUT_SECONDARY = 1u << 5,
    ML_INPUT_PREVIOUS = 1u << 6,
    ML_INPUT_NEXT = 1u << 7,
    ML_INPUT_PAUSE = 1u << 8,
};

enum ml_collision {
    ML_COLLISION_SOLID = 1u << 0,
    ML_COLLISION_HAZARD = 1u << 1,
    ML_COLLISION_ONE_WAY = 1u << 2,
    ML_COLLISION_SLOPE_UP = 1u << 3,
    ML_COLLISION_SLOPE_DOWN = 1u << 4,
    ML_COLLISION_WATER = 1u << 5,
    ML_COLLISION_CLIMB = 1u << 6,
    ML_COLLISION_LOOP = 1u << 7,
};

enum ml_entity_kind {
    ML_ENTITY_NONE = 0,
    ML_ENTITY_PLAYER = 1,
    ML_ENTITY_GOAL = 2,
    ML_ENTITY_COLLECTIBLE = 3,
    ML_ENTITY_ENEMY = 4,
    ML_ENTITY_CHECKPOINT = 5,
    ML_ENTITY_KEY = 6,
    ML_ENTITY_DOOR = 7,
    ML_ENTITY_SWITCH = 8,
    ML_ENTITY_BLOCK = 9,
    ML_ENTITY_SPRING = 10,
    ML_ENTITY_ITEM = 11,
    ML_ENTITY_POT = 12,
    ML_ENTITY_NPC = 13,
    ML_ENTITY_SHOP = 14,
    ML_ENTITY_HOUSE = 15,
    ML_ENTITY_CAR = 16,
    ML_ENTITY_FURNITURE = 17,
    ML_ENTITY_DECORATION = 18,
};

enum ml_life_interaction {
    ML_LIFE_INTERACTION_NONE = 0,
    ML_LIFE_INTERACTION_HOUSE,
    ML_LIFE_INTERACTION_SHOP,
    ML_LIFE_INTERACTION_NPC,
    ML_LIFE_INTERACTION_FURNITURE,
};

enum ml_life_notice {
    ML_LIFE_NOTICE_NONE = 0,
    ML_LIFE_NOTICE_SOLD,
    ML_LIFE_NOTICE_NO_SALVAGE,
    ML_LIFE_NOTICE_PAID,
    ML_LIFE_NOTICE_NO_CREDITS,
    ML_LIFE_NOTICE_UPGRADED_HOUSE,
    ML_LIFE_NOTICE_DEBT_REMAINS,
    ML_LIFE_NOTICE_MAX_HOUSE,
    ML_LIFE_NOTICE_STYLED_HOUSE,
    ML_LIFE_NOTICE_UPGRADED_CAR,
    ML_LIFE_NOTICE_MAX_CAR,
    ML_LIFE_NOTICE_JOB_PAID,
    ML_LIFE_NOTICE_JOB_COOLDOWN,
    ML_LIFE_NOTICE_FURNITURE_PICKED,
    ML_LIFE_NOTICE_FURNITURE_PLACED,
    ML_LIFE_NOTICE_FURNITURE_SOLD,
    ML_LIFE_NOTICE_FURNITURE_BLOCKED,
};

enum ml_player_action {
    ML_ACTION_IDLE = 0,
    ML_ACTION_WALK,
    ML_ACTION_RUN,
    ML_ACTION_JUMP,
    ML_ACTION_FALL,
    ML_ACTION_CROUCH,
    ML_ACTION_SKID,
    ML_ACTION_SWIM,
    ML_ACTION_SWORD,
    ML_ACTION_ITEM,
    ML_ACTION_ROLL,
    ML_ACTION_SPINDASH,
    ML_ACTION_HURT,
    ML_ACTION_COMPLETE,
    ML_ACTION_COUNT,
};

enum ml_event_trigger {
    ML_TRIGGER_ENTER_REGION = 1,
    ML_TRIGGER_SWITCH_ON,
    ML_TRIGGER_ENEMY_GROUP_CLEAR,
};

enum ml_event_condition {
    ML_CONDITION_ALWAYS = 0,
    ML_CONDITION_HAS_KEY,
    ML_CONDITION_RINGS_GTE,
    ML_CONDITION_SWITCH_ON,
    ML_CONDITION_ENEMY_GROUP_CLEAR,
};

enum ml_event_action {
    ML_ACTION_OPEN_DOOR = 1,
    ML_ACTION_TOGGLE_BLOCK_GROUP,
    ML_ACTION_SPAWN_GROUP,
    ML_ACTION_PLAY_EFFECT,
    ML_ACTION_SET_CHECKPOINT,
    ML_ACTION_COMPLETE_LEVEL,
};

enum ml_event_flags {
    ML_EVENT_ONE_SHOT = 1u << 0,
};

enum ml_path_flags {
    ML_PATH_PING_PONG = 1u << 0,
    ML_PATH_SURFACE = 1u << 1,
};

enum ml_pack_error {
    ML_PACK_OK = 0,
    ML_PACK_TOO_SMALL,
    ML_PACK_BAD_MAGIC,
    ML_PACK_BAD_VERSION,
    ML_PACK_BAD_RULESET,
    ML_PACK_BAD_DIMENSIONS,
    ML_PACK_TOO_MANY_ENTITIES,
    ML_PACK_BAD_OFFSET,
    ML_PACK_BAD_CRC,
    ML_PACK_BAD_ID,
    ML_PACK_BAD_CONTENT,
    ML_PACK_NO_PLAYER,
    ML_PACK_NO_GOAL,
};

struct ml_entity {
    uint16_t kind;
    uint16_t flags;
    int16_t x;
    int16_t y;
    int16_t param[4];
    uint16_t render_cell;
};

struct ml_event {
    uint8_t trigger;
    uint8_t condition;
    uint8_t action;
    uint8_t flags;
    uint16_t subject;
    int16_t value;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int16_t param0;
    int16_t param1;
    uint16_t delay;
};

struct ml_path {
    uint16_t id;
    uint16_t flags;
    uint16_t point_start;
    uint16_t point_count;
    int16_t speed;
};

struct ml_path_point {
    int16_t x;
    int16_t y;
};

struct ml_level {
    const uint8_t *data;
    size_t size;
    uint8_t ruleset;
    uint8_t flags;
    uint16_t view_width;
    uint16_t view_height;
    uint16_t map_width;
    uint16_t map_height;
    uint16_t tile_size;
    uint16_t entity_count;
    uint16_t entity_size;
    uint16_t tile_entry_size;
    const uint8_t *tiles;
    const uint8_t *collision;
    const uint8_t *entity_data;
    const uint8_t *event_data;
    const uint8_t *path_data;
    const uint8_t *path_point_data;
    const char *title;
    uint16_t title_length;
    uint16_t event_count;
    uint16_t path_count;
    uint16_t path_point_count;
    char kit_id[ML_ID_SIZE + 1];
    char project_id[ML_ID_SIZE + 1];
};

struct ml_player {
    int32_t x;
    int32_t y;
    int32_t vx;
    int32_t vy;
    int32_t ground_speed;
    int16_t width;
    int16_t height;
    int16_t facing_x;
    int16_t facing_y;
    int16_t health;
    int16_t invulnerable_ticks;
    int16_t action_ticks;
    uint8_t action;
    uint8_t grounded;
    uint8_t rolling;
    uint8_t in_water;
    uint8_t power_state;
    uint8_t carrying;
    uint8_t shield;
    uint8_t spindash_charge;
    uint8_t surface_attached;
    uint16_t surface_path;
    uint16_t surface_segment;
};

struct ml_world {
    const struct ml_level *level;
    struct ml_player player;
    uint8_t entity_alive[ML_MAX_ENTITIES];
    uint8_t block_push_ticks[ML_MAX_ENTITIES];
    uint8_t event_fired[(ML_MAX_EVENTS + 7) / 8];
    uint16_t event_delay[ML_MAX_EVENTS];
    int32_t entity_x[ML_MAX_ENTITIES];
    int32_t entity_y[ML_MAX_ENTITIES];
    int32_t entity_vx[ML_MAX_ENTITIES];
    int32_t entity_vy[ML_MAX_ENTITIES];
    uint32_t input;
    uint32_t previous_input;
    uint32_t tick;
    int32_t camera_x;
    int32_t camera_y;
    int32_t spawn_x;
    int32_t spawn_y;
    int32_t checkpoint_x;
    int32_t checkpoint_y;
    int16_t collectibles;
    int16_t rings;
    int16_t keys;
    int16_t score;
    int16_t sword_ticks;
    int16_t item_ticks;
    int16_t hurt_ticks;
    int16_t loose_rings;
    int16_t selected_item;
    int16_t pending_effect;
    uint32_t best_ticks;
    uint8_t complete;
    uint8_t paused;
    uint8_t switch_state;
    uint8_t room_transition_ticks;
    int32_t projectile_x;
    int32_t projectile_y;
    int32_t projectile_vx;
    int32_t projectile_vy;
    uint16_t projectile_ticks;
    uint8_t projectile_kind;
    uint8_t projectile_active;
    int32_t loose_ring_x[ML_MAX_LOOSE_RINGS];
    int32_t loose_ring_y[ML_MAX_LOOSE_RINGS];
    int32_t loose_ring_vx[ML_MAX_LOOSE_RINGS];
    int32_t loose_ring_vy[ML_MAX_LOOSE_RINGS];
    uint16_t loose_ring_ticks[ML_MAX_LOOSE_RINGS];
    uint8_t loose_ring_active[ML_MAX_LOOSE_RINGS];
    uint32_t credits;
    uint32_t debt;
    uint16_t house_level;
    uint16_t car_level;
    uint16_t vehicle_entity;
    uint16_t interaction_entity;
    uint16_t furniture_held_entity;
    uint16_t job_cooldown;
    uint16_t interaction_notice_ticks;
    uint8_t house_style;
    uint8_t car_active;
    uint8_t interaction;
    uint8_t interaction_choice;
    uint8_t interaction_notice;
    uint16_t brawl_opponent_entity;
    int16_t brawl_player_damage;
    int16_t brawl_opponent_damage;
    uint16_t brawl_attack_ticks;
    uint16_t brawl_opponent_attack_ticks;
    uint16_t brawl_opponent_invulnerable_ticks;
    uint8_t brawl_player_stocks;
    uint8_t brawl_opponent_stocks;
};

struct ml_snapshot {
    uint32_t tick;
    int32_t player_x;
    int32_t player_y;
    int32_t player_vx;
    int32_t player_vy;
    int32_t camera_x;
    int32_t camera_y;
    int16_t collectibles;
    int16_t rings;
    int16_t keys;
    int16_t health;
    uint8_t action;
    uint8_t grounded;
    uint8_t complete;
    uint8_t paused;
    int16_t facing_x;
    int16_t facing_y;
    uint8_t power_state;
    uint8_t carrying;
    uint8_t shield;
    uint8_t reserved;
    uint32_t credits;
    uint32_t debt;
    uint16_t house_level;
    uint16_t car_level;
    uint16_t job_cooldown;
    uint16_t furniture_held_entity;
    uint8_t house_style;
    uint8_t car_active;
    uint8_t interaction;
    uint8_t interaction_choice;
    uint8_t interaction_notice;
    uint8_t life_reserved[3];
    int16_t brawl_player_damage;
    int16_t brawl_opponent_damage;
    uint16_t brawl_opponent_entity;
    uint8_t brawl_player_stocks;
    uint8_t brawl_opponent_stocks;
    uint8_t brawl_reserved[2];
};

uint32_t ml_crc32(const void *data, size_t size);
enum ml_pack_error ml_pack_open(const void *data, size_t size,
                                struct ml_level *level);
const char *ml_pack_error_string(enum ml_pack_error error);
bool ml_level_entity(const struct ml_level *level, unsigned index,
                     struct ml_entity *entity);
bool ml_level_event(const struct ml_level *level, unsigned index,
                    struct ml_event *event);
bool ml_level_path(const struct ml_level *level, unsigned index,
                   struct ml_path *path);
bool ml_level_path_point(const struct ml_level *level, unsigned index,
                         struct ml_path_point *point);
bool ml_world_entity(const struct ml_world *world, unsigned index,
                     struct ml_entity *entity);
uint16_t ml_level_tile(const struct ml_level *level, int tile_x, int tile_y);
uint8_t ml_level_collision(const struct ml_level *level,
                           int tile_x, int tile_y);

bool ml_world_init(struct ml_world *world, const struct ml_level *level);
void ml_world_tick(struct ml_world *world, uint32_t input);
void ml_world_snapshot(const struct ml_world *world,
                       struct ml_snapshot *snapshot);
uint32_t ml_world_digest(const struct ml_world *world);
void ml_world_respawn(struct ml_world *world);

int ml_fixed_to_int(int32_t value);
int32_t ml_int_to_fixed(int value);

#endif
