#ifndef DIALTONE_H
#define DIALTONE_H

#include <gb/gb.h>
#include <gb/cgb.h>
#include <gbdk/console.h>
#include <stdint.h>
#include <stdbool.h>

#define MAP_W 20u
#define MAP_H 18u
#define MAP_SIZE (MAP_W * MAP_H)

#define ITEM_SOFTPHONES  0x01u
#define ITEM_BROTH       0x02u
#define ITEM_MINIDISC    0x04u
#define ITEM_POSTER      0x08u
#define ITEM_LAMP        0x10u
#define ITEM_SHELF       0x20u
#define ITEM_TANK        0x40u
#define ITEM_DOCK        0x80u

#define MEDIA_RAIN_LOOP      0x01u
#define MEDIA_VENDING_DREAMS 0x02u
#define MEDIA_ROOFTOP_SET    0x04u

#define DECOR_NONE   0u
#define DECOR_POSTER 1u
#define DECOR_LAMP   2u
#define DECOR_SHELF  3u
#define DECOR_TANK   4u
#define DECOR_DOCK   5u

#define ROOM_DISTRICT 0u
#define ROOM_APARTMENT 1u
#define ROOM_RECORDS 2u
#define ROOM_TECH 3u
#define ROOM_RAMEN 4u
#define ROOM_ROOFTOP 5u
#define ROOM_SOUTHLINE 6u
#define ROOM_COUNT 7u
#define DOOR_COUNT 12u
#define HOTSPOT_COUNT 6u

#define NPC_MINA 0u
#define NPC_ROOK 1u
#define NPC_SOMA 2u
#define NPC_IONA 3u
#define NPC_PIX 4u
#define NPC_COUNT 5u

#define TASK_MUSIC_DROP 0u
#define TASK_SOUP_RUN 1u
#define TASK_DISC_SWAP 2u
#define TASK_COUNT 3u

#define DIR_DOWN 0u
#define DIR_UP 1u
#define DIR_LEFT 2u
#define DIR_RIGHT 3u

#define MODE_EXPLORE 0u
#define MODE_DIALOGUE 1u

#define BG_TILE_COUNT 32u
#define SPRITE_TILE_COUNT 52u
#define APARTMENT_DECOR_TILE_COUNT 5u

typedef struct save_data_t {
    uint8_t magic[4];
    uint8_t version;
    uint8_t room_id;
    uint8_t player_x;
    uint8_t player_y;
    uint8_t facing;
    uint8_t day;
    uint8_t credits;
    uint8_t task_id;
    uint8_t task_done;
    uint8_t inventory;
    uint8_t media;
    uint8_t decor_slots[3];
    uint8_t started;
    uint8_t keepsake;
    uint8_t room_palette_mode;
    uint8_t text_speed;
} save_data_t;

typedef struct npc_t {
    uint8_t room_id;
    uint8_t x;
    uint8_t y;
    uint8_t tile;
    const char *name;
} npc_t;

typedef struct door_t {
    uint8_t room_id;
    uint8_t x;
    uint8_t y;
    uint8_t target_room;
    uint8_t target_x;
    uint8_t target_y;
} door_t;

typedef struct hotspot_t {
    uint8_t room_id;
    uint8_t x;
    uint8_t y;
    uint8_t id;
} hotspot_t;

typedef struct task_t {
    uint8_t giver_npc;
    uint8_t required_item;
    uint8_t reward_media;
    uint8_t reward_credits;
    const char *title;
    const char *brief;
    const char *complete;
} task_t;

extern save_data_t g_save;
extern uint8_t g_map_buffer[MAP_SIZE];

extern uint8_t bg_tiles[];
extern uint8_t apartment_decor_tiles[];
extern uint8_t sprite_tiles[];
extern const uint16_t district_bg_pal[];
extern const uint16_t interior_bg_pal[];
extern const uint16_t rooftop_bg_pal[];
extern const uint16_t sprite_pal[];

extern const npc_t npcs[NPC_COUNT];
extern const task_t tasks[TASK_COUNT];
extern const char *const room_maps[ROOM_COUNT][MAP_H];
extern const door_t doors[DOOR_COUNT];
extern const hotspot_t hotspots[HOTSPOT_COUNT];

void dt_init(void);
bool dt_title(void);
void dt_setup_new_game(void);
void dt_game_loop(void);

void dt_show_text(const char *title, const char *body, const char *footer);
uint8_t dt_menu(const char *title, const char *subtitle, const char *const *items, uint8_t count, uint8_t selected);

void dt_load_room(void);
void dt_render_room(void);
void dt_enter_room(uint8_t room_id, uint8_t x, uint8_t y);
void dt_try_move(int8_t dx, int8_t dy, uint8_t facing);
void dt_try_interact(void);
void dt_pause_menu(void);
void dt_inventory_screen(void);
void dt_options_screen(void);
void dt_help_screen(void);
void dt_shop_screen(void);
void dt_decor_screen(void);
void dt_media_screen(void);

void dt_assign_daily_task(void);
const task_t *dt_current_task(void);
bool dt_has_item(uint8_t item);
void dt_add_item(uint8_t item);
void dt_remove_item(uint8_t item);
void dt_save_game(void);
bool dt_has_save(void);
bool dt_load_game(void);
void dt_erase_save(void);
void dt_init_assets(void) BANKED;

#endif
