/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * C-Dogs Rockbox vertical slice built from upstream C-Dogs SDL campaign data
 * and curated upstream art/audio assets.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "plugin.h"

#include "cdogs_audio_game.h"
#include "cdogs_audio_door.h"
#include "cdogs_audio_flamer.h"
#include "cdogs_audio_launch.h"
#include "cdogs_audio_map_open.h"
#include "cdogs_audio_menu_move.h"
#include "cdogs_audio_menu_start.h"
#include "cdogs_audio_menu.h"
#include "cdogs_audio_menu_back.h"
#include "cdogs_audio_menu_enter.h"
#include "cdogs_audio_mg.h"
#include "cdogs_audio_mission_complete.h"
#include "cdogs_audio_pistol.h"
#include "cdogs_audio_pickup.h"
#include "cdogs_audio_powergun.h"
#include "cdogs_audio_rescue.h"
#include "cdogs_audio_shotgun.h"
#include "cdogs_terror_campaign.h"
#include "cdogs_terror_characters.h"
#include "pluginbitmaps/cdogs_bench.h"
#include "pluginbitmaps/cdogs_bookshelf.h"
#include "pluginbitmaps/cdogs_box.h"
#include "pluginbitmaps/cdogs_box2.h"
#include "pluginbitmaps/cdogs_barrel_blue.h"
#include "pluginbitmaps/cdogs_barrel_wood.h"
#include "pluginbitmaps/cdogs_cabinet.h"
#include "pluginbitmaps/cdogs_chair.h"
#include "pluginbitmaps/cdogs_enemy_legs.h"
#include "pluginbitmaps/cdogs_enemy_upper.h"
#include "pluginbitmaps/cdogs_enemy_upper_a.h"
#include "pluginbitmaps/cdogs_enemy_upper_b.h"
#include "pluginbitmaps/cdogs_enemy_upper_c.h"
#include "pluginbitmaps/cdogs_enemy_upper_d.h"
#include "pluginbitmaps/cdogs_logo.h"
#include "pluginbitmaps/cdogs_objective.h"
#include "pluginbitmaps/cdogs_objective_kill.h"
#include "pluginbitmaps/cdogs_player_legs.h"
#include "pluginbitmaps/cdogs_player_upper.h"
#include "pluginbitmaps/cdogs_pulse.h"
#include "pluginbitmaps/cdogs_safe.h"
#include "pluginbitmaps/cdogs_table.h"
#include "pluginbitmaps/cdogs_table_steel.h"
#include "pluginbitmaps/cdogs_wall_n.h"
#include "pluginbitmaps/cdogs_wall_w.h"

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320)
#include "pluginbitmaps/back_bar.h"
#include "pluginbitmaps/cdogs_floor.h"
#include "pluginbitmaps/cdogs_floor_tile.h"
#include "pluginbitmaps/cdogs_floor_flat.h"
#include "pluginbitmaps/cdogs_floor_grid.h"
#include "pluginbitmaps/cdogs_floor_biggrid.h"
#include "pluginbitmaps/cdogs_floor_smallsquare.h"
#include "pluginbitmaps/cdogs_floor_checker.h"
#include "pluginbitmaps/cdogs_floor_dirt.h"
#include "pluginbitmaps/cdogs_floor_wood.h"
#include "pluginbitmaps/cdogs_floor_stone.h"
#include "pluginbitmaps/cdogs_floor_recessed.h"
#include "pluginbitmaps/cdogs_plant.h"
#include "pluginbitmaps/cdogs_paper.h"
#include "pluginbitmaps/cdogs_sack.h"
#include "pluginbitmaps/cdogs_wall_n_steel.h"
#include "pluginbitmaps/cdogs_wall_w_steel.h"
#include "pluginbitmaps/cdogs_wall_n_plasteel.h"
#include "pluginbitmaps/cdogs_wall_w_plasteel.h"
#include "pluginbitmaps/cdogs_wall_n_brick.h"
#include "pluginbitmaps/cdogs_wall_w_brick.h"
#include "pluginbitmaps/cdogs_wall_n_stone.h"
#include "pluginbitmaps/cdogs_wall_w_stone.h"
#include "pluginbitmaps/cdogs_wall_n_granite.h"
#include "pluginbitmaps/cdogs_wall_w_granite.h"
#include "pluginbitmaps/player_frame.h"
#include "pluginbitmaps/player_frame_underlay.h"
#include "pluginbitmaps/objective_kill.h"
#include "pluginbitmaps/gauge_back.h"
#include "pluginbitmaps/gauge_inner.h"
#include "pluginbitmaps/button_bg.h"
#include "pluginbitmaps/arrow.h"
#include "pluginbitmaps/gun_bg_30x23.h"
#endif

#include <stdbool.h>
#include <stdint.h>

enum
{
    CDOGS_CMD_LEFT = 1,
    CDOGS_CMD_RIGHT = 2,
    CDOGS_CMD_UP = 4,
    CDOGS_CMD_DOWN = 8,
    CDOGS_CMD_FIRE = 16,
    CDOGS_CMD_MAP = 32,
    CDOGS_CMD_ACCEPT = 64,
    CDOGS_CMD_BACK = 128,
    CDOGS_CMD_GRENADE = 256
};

enum cdogs_screen
{
    CDOGS_SCREEN_TITLE = 0,
    CDOGS_SCREEN_BRIEFING,
    CDOGS_SCREEN_MISSION,
    CDOGS_SCREEN_COMPLETE,
    CDOGS_SCREEN_GAME_OVER
};

enum prop_kind
{
    PROP_TABLE = 0,
    PROP_TABLE_STEEL,
    PROP_CABINET,
    PROP_BOOKSHELF,
    PROP_BENCH,
    PROP_CHAIR,
    PROP_SAFE,
    PROP_PLANT,
    PROP_BOX,
    PROP_BOX2,
    PROP_BARREL_BLUE,
    PROP_BARREL_WOOD,
    PROP_COUNT
};

enum floor_style
{
    FLOOR_STYLE_TILE = 0,
    FLOOR_STYLE_FLAT,
    FLOOR_STYLE_GRID,
    FLOOR_STYLE_BIGGRID,
    FLOOR_STYLE_SMALLSQUARE,
    FLOOR_STYLE_CHECKER,
    FLOOR_STYLE_DIRT,
    FLOOR_STYLE_WOOD,
    FLOOR_STYLE_STONE,
    FLOOR_STYLE_RECESSED
};

enum wall_style
{
    WALL_STYLE_STEEL = 0,
    WALL_STYLE_PLASTEEL,
    WALL_STYLE_BRICK,
    WALL_STYLE_STONE,
    WALL_STYLE_GRANITE
};

struct actor
{
    int x;
    int y;
    int size;
    int hp;
    int max_hp;
    bool active;
    int dir_x;
    int dir_y;
    int sprite_variant;
    int weapon_type;
    int fire_cooldown;
    int action_delay;
    int shoot_chance;
    int track_chance;
    int move_chance;
    int char_id;
    int move_speed;
    int ai_delay;
    int burn_timer;
    int poison_timer;
    int confuse_timer;
    bool sleeping;
    bool waking;
    bool fire_immune;
    bool objective_target;
};

enum objective_kind
{
    OBJECTIVE_KILL = 0,
    OBJECTIVE_COLLECT,
    OBJECTIVE_DESTROY,
    OBJECTIVE_RESCUE,
    OBJECTIVE_EXPLORE
};

struct objective_entity
{
    int x;
    int y;
    int size;
    int hp;
    int kind;
    bool following;
    bool active;
};

enum projectile_kind
{
    PROJ_NORMAL = 0,
    PROJ_FLAME,
    PROJ_GRENADE,
    PROJ_SHRAPNEL_BOMB,
    PROJ_CHEMO_BOMB,
    PROJ_LASER,
    PROJ_SNIPER
};

struct bullet
{
    int x;
    int y;
    int vx;
    int vy;
    int ttl;
    int damage;
    int splash;
    uint8_t kind;
    uint8_t source_weapon;
    bool active;
};

struct prop_spawn
{
    int x;
    int y;
    int kind;
};

struct audio_clip
{
    const uint8_t *samples;
    int byte_count;
};

struct audio_state
{
    struct audio_clip bgm;
    struct audio_clip sfx;
    int bgm_pos;
    int sfx_pos;
    int bgm_start;
    int bgm_end;
    int sfx_start;
    int sfx_end;
    bool bgm_loop;
    bool active;
};

struct game_state
{
    enum cdogs_screen screen;
    struct actor player;
    struct actor enemies[8];
    struct bullet bullets[20];
    struct bullet enemy_bullets[16];
    int facing_x;
    int facing_y;
    int player_anim_offset;
    int player_anim_frame;
    int score;
    int wave;
    int fire_cooldown;
    int grenade_cooldown;
    int enemy_fire_cooldown;
    int damage_flash;
    int grenade_flash;
    int enemy_step;
    int player_shot_timer;
    int player_burn_timer;
    int player_poison_timer;
    int player_confuse_timer;
    int mission_time;
    int hud_message_ticks;
    char hud_message[64];
    int font_h;
    int field_x;
    int field_y;
    int field_w;
    int field_h;
    int selected_mission;
    int title_menu_index;
    int mission_index;
    int player_primary;
    int player_secondary;
    int objective_kind;
    int objective_required;
    int objective_progress;
    int objective_spawned;
    int enemy_spawn_limit;
    int available_enemies;
    int briefing_scroll;
    int complete_timer;
    bool show_map;
    bool combo_latched;
    bool mission_failed;

    int map_w;
    int map_h;
    int floor_style;
    int room_floor_style;
    int wall_style;
    uint8_t map_tiles[64 * 64];
    uint8_t room_x[64];
    uint8_t room_y[64];
    uint8_t room_w[64];
    uint8_t room_h[64];
    uint8_t room_visited[64];
    int room_count;
    struct prop_spawn props[96];
    int prop_count;
    struct objective_entity objectives[24];
    int objective_entity_count;
    unsigned rng;
};

#define MAP_MAX_W 64
#define MAP_MAX_H 64
enum { TILE_FLOOR, TILE_WALL, TILE_DOOR_CLOSED, TILE_DOOR_OPEN };
#define CDOGS_TILE_W 16
#define CDOGS_TILE_H 12

#if LCD_DEPTH > 1
#define COLOR_BG LCD_RGBPACK(9, 11, 15)
#define COLOR_BG_ALT LCD_RGBPACK(17, 23, 31)
#define COLOR_PANEL LCD_RGBPACK(21, 28, 38)
#define COLOR_PANEL_ALT LCD_RGBPACK(33, 44, 57)
#define COLOR_BORDER LCD_RGBPACK(95, 180, 145)
#define COLOR_PLAYER LCD_RGBPACK(235, 240, 255)
#define COLOR_ENEMY LCD_RGBPACK(255, 110, 95)
#define COLOR_BULLET LCD_RGBPACK(255, 215, 80)
#define COLOR_FLASH LCD_RGBPACK(255, 245, 180)
#define COLOR_TEXT LCD_RGBPACK(222, 232, 240)
#define COLOR_TEXT_DIM LCD_RGBPACK(150, 165, 180)
#define COLOR_MAP LCD_RGBPACK(90, 130, 255)
#define COLOR_DANGER LCD_RGBPACK(255, 80, 80)
#define COLOR_EBULLET LCD_RGBPACK(110, 255, 190)
#define COLOR_ACCENT LCD_RGBPACK(210, 90, 70)
#define COLOR_OBJECTIVE LCD_RGBPACK(245, 220, 110)
#define COLOR_SUCCESS LCD_RGBPACK(110, 230, 150)
#define COLOR_SHADOW LCD_RGBPACK(18, 24, 30)
#define COLOR_WALL_SIDE LCD_RGBPACK(52, 60, 72)
#define COLOR_FLOOR_SHADE LCD_RGBPACK(28, 34, 42)
#else
#define COLOR_BG LCD_BLACK
#define COLOR_BG_ALT LCD_BLACK
#define COLOR_PANEL LCD_BLACK
#define COLOR_PANEL_ALT LCD_BLACK
#define COLOR_BORDER LCD_WHITE
#define COLOR_PLAYER LCD_WHITE
#define COLOR_ENEMY LCD_WHITE
#define COLOR_BULLET LCD_WHITE
#define COLOR_FLASH LCD_WHITE
#define COLOR_TEXT LCD_WHITE
#define COLOR_TEXT_DIM LCD_WHITE
#define COLOR_MAP LCD_WHITE
#define COLOR_DANGER LCD_WHITE
#define COLOR_EBULLET LCD_WHITE
#define COLOR_ACCENT LCD_WHITE
#define COLOR_OBJECTIVE LCD_WHITE
#define COLOR_SUCCESS LCD_WHITE
#define COLOR_SHADOW LCD_WHITE
#define COLOR_WALL_SIDE LCD_WHITE
#define COLOR_FLOOR_SHADE LCD_WHITE
#endif

#define CDOGS_BODY_FRAME_SIZE 24
#define CDOGS_BODY_DIRECTIONS 8
#define CDOGS_BODY_ANIM_FRAMES 8
#define CDOGS_BODY_SHEET_SIZE (CDOGS_BODY_FRAME_SIZE * CDOGS_BODY_DIRECTIONS)
#define CDOGS_PULSE_FRAME_SIZE 5
#define CDOGS_PULSE_FRAMES 8
#define CDOGS_FLOOR_FRAMES 3
#define CDOGS_AUDIO_SAMPLE_RATE 8000
#define CDOGS_AUDIO_SAMPLES 512

enum weapon_type
{
    WEAPON_NONE = 0,
    WEAPON_KNIFE,
    WEAPON_MACHINE_GUN,
    WEAPON_GRENADES,
    WEAPON_FLAMER,
    WEAPON_SHOTGUN,
    WEAPON_POWERGUN,
    WEAPON_SHRAPNEL_BOMBS,
    WEAPON_MOLOTOVS,
    WEAPON_SNIPER_RIFLE,
    WEAPON_CHEMO_BOMBS,
    WEAPON_CONFUSION_BOMBS
};

struct weapon_spec
{
    enum weapon_type type;
    const char *name;
    uint8_t projectile_kind;
    uint8_t speed;
    uint8_t ttl;
    uint8_t damage;
    uint8_t cooldown;
    uint8_t pellets;
    uint8_t spread;
    uint8_t splash;
    bool explosive;
};

static const struct weapon_spec weapon_specs[] =
{
    { WEAPON_NONE, "", PROJ_NORMAL, 0, 0, 0, 0, 0, 0, 0, false },
    { WEAPON_KNIFE, "Knife", PROJ_NORMAL, 0, 0, 2, 0, 0, 0, 0, false },
    { WEAPON_MACHINE_GUN, "Machine gun", PROJ_NORMAL, 6, 25, 1, 6, 1, 1, 0, false },
    { WEAPON_GRENADES, "Grenades", PROJ_GRENADE, 4, 43, 4, 30, 1, 0, 42, true },
    { WEAPON_FLAMER, "Flamer", PROJ_FLAME, 7, 13, 2, 6, 1, 2, 0, false },
    { WEAPON_SHOTGUN, "Shotgun", PROJ_NORMAL, 5, 21, 2, 50, 5, 3, 0, false },
    { WEAPON_POWERGUN, "Powergun", PROJ_LASER, 6, 38, 3, 20, 1, 0, 0, false },
    { WEAPON_SHRAPNEL_BOMBS, "Shrapnel bombs", PROJ_SHRAPNEL_BOMB, 4, 43, 4, 30, 1, 0, 0, true },
    { WEAPON_MOLOTOVS, "Molotovs", PROJ_GRENADE, 4, 20, 3, 30, 1, 0, 36, true },
    { WEAPON_SNIPER_RIFLE, "Sniper rifle", PROJ_SNIPER, 8, 13, 5, 100, 1, 0, 0, false },
    { WEAPON_CHEMO_BOMBS, "Chemo bombs", PROJ_CHEMO_BOMB, 4, 43, 1, 30, 1, 0, 36, true },
    { WEAPON_CONFUSION_BOMBS, "Confusion bombs", PROJ_GRENADE, 4, 43, 1, 30, 1, 0, 30, true }
};

static struct audio_state g_audio;
static int16_t g_mixbuf[CDOGS_AUDIO_SAMPLES * 2];
#define CLIP_PTR(arr) ((const uint8_t *)(arr))
#define CLIP_COUNT(arr) ((int)sizeof(arr))

static const struct audio_clip clip_menu_music = {
    CLIP_PTR(_tmp_cdogs_menu_raw),
    CLIP_COUNT(_tmp_cdogs_menu_raw)
};
static const struct audio_clip clip_game_music = {
    CLIP_PTR(_tmp_cdogs_game_raw),
    CLIP_COUNT(_tmp_cdogs_game_raw)
};
static const struct audio_clip clip_menu_move = {
    CLIP_PTR(_tmp_cdogs_menu_move_raw),
    CLIP_COUNT(_tmp_cdogs_menu_move_raw)
};
static const struct audio_clip clip_menu_start = {
    CLIP_PTR(_tmp_cdogs_menu_start_raw),
    CLIP_COUNT(_tmp_cdogs_menu_start_raw)
};
static const struct audio_clip clip_menu_back = {
    CLIP_PTR(_tmp_cdogs_menu_back_raw),
    CLIP_COUNT(_tmp_cdogs_menu_back_raw)
};
static const struct audio_clip clip_pistol = {
    CLIP_PTR(_tmp_cdogs_pistol_raw),
    CLIP_COUNT(_tmp_cdogs_pistol_raw)
};
static const struct audio_clip clip_mg = {
    CLIP_PTR(_tmp_cdogs_mg_raw),
    CLIP_COUNT(_tmp_cdogs_mg_raw)
};
static const struct audio_clip clip_shotgun = {
    CLIP_PTR(_tmp_cdogs_shotgun_raw),
    CLIP_COUNT(_tmp_cdogs_shotgun_raw)
};
static const struct audio_clip clip_flamer = {
    CLIP_PTR(_tmp_cdogs_flamer_raw),
    CLIP_COUNT(_tmp_cdogs_flamer_raw)
};
static const struct audio_clip clip_powergun = {
    CLIP_PTR(_tmp_cdogs_powergun_raw),
    CLIP_COUNT(_tmp_cdogs_powergun_raw)
};
static const struct audio_clip clip_launch = {
    CLIP_PTR(_tmp_cdogs_launch_raw),
    CLIP_COUNT(_tmp_cdogs_launch_raw)
};
static const struct audio_clip clip_pickup = {
    CLIP_PTR(_tmp_cdogs_pickup_raw),
    CLIP_COUNT(_tmp_cdogs_pickup_raw)
};
static const struct audio_clip clip_rescue = {
    CLIP_PTR(_tmp_cdogs_rescue_raw),
    CLIP_COUNT(_tmp_cdogs_rescue_raw)
};
static const struct audio_clip clip_map_open = {
    CLIP_PTR(_tmp_cdogs_map_open_raw),
    CLIP_COUNT(_tmp_cdogs_map_open_raw)
};
static const struct audio_clip clip_door = {
    CLIP_PTR(_tmp_cdogs_door_raw),
    CLIP_COUNT(_tmp_cdogs_door_raw)
};
static const struct audio_clip clip_mission_complete = {
    CLIP_PTR(_tmp_cdogs_mission_complete_raw),
    CLIP_COUNT(_tmp_cdogs_mission_complete_raw)
};

static const struct cdogs_character_data *character_for_id(const int char_id);
static void audio_play_sfx(const struct audio_clip *clip);

static int clean_button(const long button)
{
    return (int)(button & ~(BUTTON_REL | BUTTON_REPEAT | BUTTON_REDRAW));
}

static int map_buttons(const long button)
{
    int cmd = 0;
    const int clean = clean_button(button);

    if (clean & BUTTON_LEFT)
        cmd |= CDOGS_CMD_LEFT;
    if (clean & BUTTON_RIGHT)
        cmd |= CDOGS_CMD_RIGHT;
    if (clean & BUTTON_MENU)
        cmd |= CDOGS_CMD_UP;
    if (clean & BUTTON_PLAY)
        cmd |= CDOGS_CMD_DOWN;
    if (clean & BUTTON_SELECT)
    {
        cmd |= CDOGS_CMD_FIRE;
        cmd |= CDOGS_CMD_ACCEPT;
    }
    if ((clean & BUTTON_PLAY) && (clean & BUTTON_SELECT))
        cmd |= CDOGS_CMD_GRENADE;
    if ((clean & BUTTON_MENU) && (clean & BUTTON_SELECT))
        cmd |= CDOGS_CMD_MAP;
    return cmd;
}

static int clampi(const int value, const int low, const int high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static int16_t clip_sample_at(const struct audio_clip *clip, const int sample_index)
{
    const int byte_index = sample_index * 2;
    int16_t value;
    if (!clip || !clip->samples || byte_index + 1 >= clip->byte_count)
        return 0;
    value = (int16_t)(
        (uint16_t)clip->samples[byte_index] |
        ((uint16_t)clip->samples[byte_index + 1] << 8));
    return value;
}

static int max_i(const int a, const int b)
{
    return a > b ? a : b;
}

static int min_i(const int a, const int b)
{
    return a < b ? a : b;
}

static int step_dir(const int delta)
{
    if (delta < 0)
        return -1;
    if (delta > 0)
        return 1;
    return 0;
}

static int map_index(const struct game_state *g, const int tile_x, const int tile_y)
{
    return tile_y * g->map_w + tile_x;
}

static int world_w(const struct game_state *g)
{
    return g->map_w * CDOGS_TILE_W;
}

static int world_h(const struct game_state *g)
{
    return g->map_h * CDOGS_TILE_H;
}

static bool prop_blocks_tile(const struct game_state *g, const int tile_x, const int tile_y)
{
    int i;

    for (i = 0; i < g->prop_count; ++i)
    {
        if (g->props[i].x == tile_x && g->props[i].y == tile_y)
            return true;
    }
    return false;
}

static unsigned cdogs_rand_next(unsigned *state)
{
    *state = *state * 1103515245u + 12345u;
    return *state;
}

static int cdogs_rand_range(unsigned *state, const int low, const int high)
{
    if (high <= low)
        return low;
    return low + (int)(cdogs_rand_next(state) % (unsigned)(high - low + 1));
}

static int cdogs_rand_percent(unsigned *state)
{
    return (int)(cdogs_rand_next(state) % 100u);
}

static const struct weapon_spec *weapon_spec_for_type(const enum weapon_type type)
{
    int i;
    for (i = 0; i < (int)ARRAYLEN(weapon_specs); ++i)
    {
        if (weapon_specs[i].type == type)
            return &weapon_specs[i];
    }
    return &weapon_specs[0];
}

static enum weapon_type weapon_type_from_name(const char *name)
{
    int i;
    if (!name)
        return WEAPON_NONE;
    for (i = 0; i < (int)ARRAYLEN(weapon_specs); ++i)
    {
        if (!rb->strcmp(name, weapon_specs[i].name))
            return weapon_specs[i].type;
    }
    return WEAPON_NONE;
}

static void aim_velocity(
    const int dir_x, const int dir_y, const int speed, int *vx, int *vy)
{
    if (dir_x != 0 && dir_y != 0)
    {
        const int diag = max_i(1, (speed * 181) / 256);
        *vx = dir_x * diag;
        *vy = dir_y * diag;
    }
    else
    {
        *vx = dir_x * speed;
        *vy = dir_y * speed;
    }
}

static bool map_is_walkable(const struct game_state *g, const int tile_x, const int tile_y)
{
    if (tile_x < 0 || tile_x >= g->map_w || tile_y < 0 || tile_y >= g->map_h)
        return false;
    return g->map_tiles[map_index(g, tile_x, tile_y)] == TILE_FLOOR ||
        g->map_tiles[map_index(g, tile_x, tile_y)] == TILE_DOOR_OPEN;
}

static bool map_is_opaque(const struct game_state *g, const int tile_x, const int tile_y)
{
    if (tile_x < 0 || tile_x >= g->map_w || tile_y < 0 || tile_y >= g->map_h)
        return true;
    return g->map_tiles[map_index(g, tile_x, tile_y)] == TILE_WALL ||
        g->map_tiles[map_index(g, tile_x, tile_y)] == TILE_DOOR_CLOSED;
}

static bool has_line_of_sight(
    const struct game_state *g, const int x0, const int y0,
    const int x1, const int y1)
{
    int tx0 = x0 / CDOGS_TILE_W;
    int ty0 = y0 / CDOGS_TILE_H;
    const int tx1 = x1 / CDOGS_TILE_W;
    const int ty1 = y1 / CDOGS_TILE_H;
    const int dx = abs(tx1 - tx0);
    const int dy = abs(ty1 - ty0);
    const int sx = tx0 < tx1 ? 1 : -1;
    const int sy = ty0 < ty1 ? 1 : -1;
    int err = dx - dy;

    while (true)
    {
        if (!(tx0 == x0 / CDOGS_TILE_W && ty0 == y0 / CDOGS_TILE_H) &&
            map_is_opaque(g, tx0, ty0))
            return false;
        if (tx0 == tx1 && ty0 == ty1)
            return true;
        if (err * 2 > -dy)
        {
            err -= dy;
            tx0 += sx;
        }
        if (err * 2 < dx)
        {
            err += dx;
            ty0 += sy;
        }
    }
}

static int weapon_range_pixels(const struct weapon_spec *spec)
{
    return spec->speed * spec->ttl;
}

static bool weapon_bounces(const struct bullet *b)
{
    return b->kind == PROJ_GRENADE || b->kind == PROJ_SHRAPNEL_BOMB ||
        b->kind == PROJ_CHEMO_BOMB;
}

static bool should_wake_enemy(
    const struct game_state *g, const struct actor *enemy,
    const int player_cx, const int player_cy)
{
    const int enemy_cx = enemy->x + enemy->size / 2;
    const int enemy_cy = enemy->y + enemy->size / 2;
    const int dx = player_cx - enemy_cx;
    const int dy = player_cy - enemy_cy;
    const int dist2 = dx * dx + dy * dy;

    if (dist2 <= (12 * CDOGS_TILE_W) * (12 * CDOGS_TILE_W))
        return true;
    if (has_line_of_sight(g, enemy_cx, enemy_cy, player_cx, player_cy))
        return true;
    if (g->player_shot_timer > 0 && dist2 <= (8 * CDOGS_TILE_W) * (8 * CDOGS_TILE_W))
        return true;
    if (g->player_shot_timer > 0 &&
        dist2 <= (4 * CDOGS_TILE_W) * (4 * CDOGS_TILE_W) &&
        has_line_of_sight(g, enemy_cx, enemy_cy, player_cx, player_cy))
        return true;
    return false;
}

static bool actor_fits_at(
    struct game_state *g, const struct actor *a, const int new_x, const int new_y,
    const bool open_doors)
{
    const int inset_x = max_i(1, a->size / 4);
    const int inset_y = max_i(1, a->size / 5);
    const int left = (new_x + inset_x) / CDOGS_TILE_W;
    const int right = (new_x + a->size - 1 - inset_x) / CDOGS_TILE_W;
    const int top = (new_y + inset_y) / CDOGS_TILE_H;
    const int bottom = (new_y + a->size - 1 - inset_y) / CDOGS_TILE_H;
    int x, y;

    for (y = top; y <= bottom; ++y)
    {
        for (x = left; x <= right; ++x)
        {
            if (x < 0 || x >= g->map_w || y < 0 || y >= g->map_h)
                return false;
            if (prop_blocks_tile(g, x, y))
                return false;
            if (g->map_tiles[map_index(g, x, y)] == TILE_DOOR_CLOSED)
            {
                if (open_doors)
                {
                    g->map_tiles[map_index(g, x, y)] = TILE_DOOR_OPEN;
                    audio_play_sfx(&clip_door);
                }
                else
                    return false;
            }
            if (!map_is_walkable(g, x, y))
                return false;
        }
    }

    return true;
}

static bool try_move_actor(
    struct game_state *g, struct actor *a, const int new_x, const int new_y,
    const bool open_doors)
{
    bool moved = false;

    if (actor_fits_at(g, a, new_x, a->y, open_doors))
    {
        a->x = new_x;
        moved = true;
    }
    if (actor_fits_at(g, a, a->x, new_y, open_doors))
    {
        a->y = new_y;
        moved = true;
    }

    return moved;
}

static bool intersects(
    const int ax, const int ay, const int as, const int bx, const int by,
    const int bs)
{
    return ax < bx + bs && ax + as > bx && ay < by + bs && ay + as > by;
}

static bool intersects_rect(
    const int ax, const int ay, const int aw, const int ah,
    const int bx, const int by, const int bw, const int bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void set_fg(const unsigned color)
{
    rb->lcd_set_foreground(color);
}

static void audio_find_bounds(
    const struct audio_clip *clip, int *start, int *end)
{
    int i;
    int first = 0;
    int last = clip ? clip->byte_count / 2 : 0;

    if (!clip || !clip->samples || clip->byte_count <= 0)
    {
        *start = 0;
        *end = 0;
        return;
    }

    while (first < last && clip_sample_at(clip, first) == 0)
        first++;
    while (last > first && clip_sample_at(clip, last - 1) == 0)
        last--;

    for (i = first; i < last; ++i)
    {
        const int16_t sample = clip_sample_at(clip, i);
        if (sample < -8 || sample > 8)
        {
            first = i;
            break;
        }
    }
    for (i = last - 1; i >= first; --i)
    {
        const int16_t sample = clip_sample_at(clip, i);
        if (sample < -8 || sample > 8)
        {
            last = i + 1;
            break;
        }
    }

    *start = first;
    *end = max_i(first + 1, last);
}

static void audio_callback(const void **start, size_t *size)
{
    int i;

    for (i = 0; i < CDOGS_AUDIO_SAMPLES; ++i)
    {
        int sample = 0;

        if (g_audio.bgm.samples && g_audio.bgm_end > g_audio.bgm_start)
        {
            sample += (clip_sample_at(&g_audio.bgm, g_audio.bgm_pos) * 3) / 8;
            g_audio.bgm_pos++;
            if (g_audio.bgm_pos >= g_audio.bgm_end)
            {
                if (g_audio.bgm_loop)
                    g_audio.bgm_pos = g_audio.bgm_start;
                else
                    g_audio.bgm.samples = NULL;
            }
        }

        if (g_audio.sfx.samples && g_audio.sfx_pos < g_audio.sfx_end)
        {
            sample += (clip_sample_at(&g_audio.sfx, g_audio.sfx_pos++) * 3) / 4;
            if (g_audio.sfx_pos >= g_audio.sfx_end)
            {
                g_audio.sfx.samples = NULL;
                g_audio.sfx.byte_count = 0;
                g_audio.sfx_pos = 0;
                g_audio.sfx_start = 0;
                g_audio.sfx_end = 0;
            }
        }

        sample = clampi(sample, -32768, 32767);
        g_mixbuf[i * 2] = (int16_t)sample;
        g_mixbuf[i * 2 + 1] = (int16_t)sample;
    }

    *start = g_mixbuf;
    *size = sizeof(g_mixbuf);
}

static void cdogs_audio_init(void)
{
    long deadline;

    if (g_audio.active)
        return;

    rb->talk_disable(true);
    if (rb->audio_status())
    {
        rb->audio_stop();
        deadline = *rb->current_tick + HZ * 3;
        while ((rb->audio_status() & AUDIO_STATUS_PLAY) &&
               TIME_BEFORE(*rb->current_tick, deadline))
        {
            rb->sleep(1);
        }
    }
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
#if defined(HAVE_CS42L55)
    rb->audiohw_idle_powerup();
#endif
    rb->pcm_play_stop();
    rb->pcm_set_frequency(HW_FREQ_8);
    rb->pcm_apply_settings();
    g_audio.active = true;
    g_audio.bgm.samples = NULL;
    g_audio.bgm.byte_count = 0;
    g_audio.sfx.samples = NULL;
    g_audio.sfx.byte_count = 0;
    g_audio.bgm_pos = 0;
    g_audio.sfx_pos = 0;
    g_audio.bgm_start = 0;
    g_audio.bgm_end = 0;
    g_audio.sfx_start = 0;
    g_audio.sfx_end = 0;
    g_audio.bgm_loop = true;
    rb->pcm_play_data(audio_callback, NULL, NULL, 0);
}

static void audio_shutdown(void)
{
    if (!g_audio.active)
        return;

    rb->pcm_play_stop();
    rb->pcm_set_frequency(HW_FREQ_DEFAULT);
    rb->pcm_apply_settings();
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
#if defined(HAVE_CS42L55)
    rb->audiohw_idle_powerdown();
#endif
    rb->talk_disable(false);
    rb->memset(&g_audio, 0, sizeof(g_audio));
}

static void audio_set_bgm(const struct audio_clip *clip, const bool loop)
{
    if (!clip)
        return;
    cdogs_audio_init();
    g_audio.bgm = *clip;
    audio_find_bounds(clip, &g_audio.bgm_start, &g_audio.bgm_end);
    g_audio.bgm_pos = g_audio.bgm_start;
    g_audio.bgm_loop = loop;
}

static void audio_play_sfx(const struct audio_clip *clip)
{
    if (!clip)
        return;
    cdogs_audio_init();
    g_audio.sfx = *clip;
    audio_find_bounds(clip, &g_audio.sfx_start, &g_audio.sfx_end);
    g_audio.sfx_pos = g_audio.sfx_start;
}

static const struct audio_clip *shot_clip_for_weapon(const enum weapon_type type)
{
    switch (type)
    {
    case WEAPON_MACHINE_GUN:
        return &clip_mg;
    case WEAPON_SHOTGUN:
        return &clip_shotgun;
    case WEAPON_FLAMER:
    case WEAPON_MOLOTOVS:
        return &clip_flamer;
    case WEAPON_POWERGUN:
        return &clip_powergun;
    case WEAPON_SNIPER_RIFLE:
        return &clip_shotgun;
    case WEAPON_SHRAPNEL_BOMBS:
    case WEAPON_CHEMO_BOMBS:
    case WEAPON_CONFUSION_BOMBS:
    case WEAPON_GRENADES:
        return &clip_launch;
    default:
        return &clip_pistol;
    }
}

static void enter_display_mode(void)
{
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_background(LCD_DEFAULT_BG);
    rb->lcd_set_foreground(LCD_DEFAULT_FG);
    rb->lcd_clear_display();
    rb->lcd_update();
}

static void leave_display_mode(void)
{
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_background(LCD_DEFAULT_BG);
    rb->lcd_set_foreground(LCD_DEFAULT_FG);
    rb->lcd_clear_display();
    rb->lcd_update();
    audio_shutdown();
}

static int facing_frame(const int x, const int y)
{
    if (x == 0 && y < 0)
        return 0;
    if (x > 0 && y < 0)
        return 1;
    if (x > 0 && y == 0)
        return 2;
    if (x > 0 && y > 0)
        return 3;
    if (x == 0 && y > 0)
        return 4;
    if (x < 0 && y > 0)
        return 5;
    if (x < 0 && y == 0)
        return 6;
    if (x < 0 && y < 0)
        return 7;
    return 0;
}

static void draw_sheet_frame(
    const fb_data *sheet, const int direction,
    const int anim_frame, const int x, const int y)
{
    const int src_x = direction * CDOGS_BODY_FRAME_SIZE;
    const int src_y = anim_frame * CDOGS_BODY_FRAME_SIZE;
    const int stride = STRIDE(SCREEN_MAIN, CDOGS_BODY_SHEET_SIZE, CDOGS_BODY_SHEET_SIZE);
    rb->lcd_bitmap_transparent_part(
        sheet, src_x, src_y, stride, x, y,
        CDOGS_BODY_FRAME_SIZE, CDOGS_BODY_FRAME_SIZE);
}

static const fb_data *enemy_sheet_for_variant(const int variant)
{
    switch (variant & 3)
    {
    case 1:
        return cdogs_enemy_upper_b;
    case 2:
        return cdogs_enemy_upper_c;
    case 3:
        return cdogs_enemy_upper_d;
    default:
        return cdogs_enemy_upper_a;
    }
}

static const fb_data *floor_sheet_for_style(const int style)
{
    switch (style)
    {
    case FLOOR_STYLE_FLAT:
        return cdogs_floor_flat;
    case FLOOR_STYLE_GRID:
        return cdogs_floor_grid;
    case FLOOR_STYLE_BIGGRID:
        return cdogs_floor_biggrid;
    case FLOOR_STYLE_SMALLSQUARE:
        return cdogs_floor_smallsquare;
    case FLOOR_STYLE_CHECKER:
        return cdogs_floor_checker;
    case FLOOR_STYLE_DIRT:
        return cdogs_floor_dirt;
    case FLOOR_STYLE_WOOD:
        return cdogs_floor_wood;
    case FLOOR_STYLE_STONE:
        return cdogs_floor_stone;
    case FLOOR_STYLE_RECESSED:
        return cdogs_floor_recessed;
    default:
        return cdogs_floor_tile;
    }
}

static const fb_data *wall_n_for_style(const int style)
{
    switch (style)
    {
    case WALL_STYLE_PLASTEEL:
        return cdogs_wall_n_plasteel;
    case WALL_STYLE_BRICK:
        return cdogs_wall_n_brick;
    case WALL_STYLE_STONE:
        return cdogs_wall_n_stone;
    case WALL_STYLE_GRANITE:
        return cdogs_wall_n_granite;
    default:
        return cdogs_wall_n_steel;
    }
}

static const fb_data *wall_w_for_style(const int style)
{
    switch (style)
    {
    case WALL_STYLE_PLASTEEL:
        return cdogs_wall_w_plasteel;
    case WALL_STYLE_BRICK:
        return cdogs_wall_w_brick;
    case WALL_STYLE_STONE:
        return cdogs_wall_w_stone;
    case WALL_STYLE_GRANITE:
        return cdogs_wall_w_granite;
    default:
        return cdogs_wall_w_steel;
    }
}

static void configure_mission_visuals(struct game_state *g, const int mission_index)
{
    const struct cdogs_mission_data *mission = &cdogs_campaign_missions[mission_index];

    g->floor_style = FLOOR_STYLE_TILE;
    g->room_floor_style = FLOOR_STYLE_FLAT;
    g->wall_style = WALL_STYLE_STEEL;

    if (!rb->strcmp(mission->title, "Storeroom plans"))
    {
        g->floor_style = FLOOR_STYLE_FLAT;
        g->room_floor_style = FLOOR_STYLE_FLAT;
        g->wall_style = WALL_STYLE_PLASTEEL;
    }
    else if (!rb->strcmp(mission->title, "B.A.D. storage raid"))
    {
        g->floor_style = FLOOR_STYLE_GRID;
        g->room_floor_style = FLOOR_STYLE_BIGGRID;
        g->wall_style = WALL_STYLE_PLASTEEL;
    }
    else if (!rb->strcmp(mission->title, "Hostage situation"))
    {
        g->floor_style = FLOOR_STYLE_TILE;
        g->room_floor_style = FLOOR_STYLE_CHECKER;
        g->wall_style = WALL_STYLE_STEEL;
    }
    else if (!rb->strcmp(mission->title, "Take out B.A.D. ring leader"))
    {
        g->floor_style = FLOOR_STYLE_SMALLSQUARE;
        g->room_floor_style = FLOOR_STYLE_FLAT;
        g->wall_style = WALL_STYLE_PLASTEEL;
    }
    else if (!rb->strcmp(mission->title, "B.A.D. retaliates"))
    {
        g->floor_style = FLOOR_STYLE_FLAT;
        g->room_floor_style = FLOOR_STYLE_CHECKER;
        g->wall_style = WALL_STYLE_GRANITE;
    }
    else if (!rb->strcmp(mission->title, "Rescue the chief of police"))
    {
        g->floor_style = FLOOR_STYLE_DIRT;
        g->room_floor_style = FLOOR_STYLE_WOOD;
        g->wall_style = WALL_STYLE_BRICK;
    }
    else if (!rb->strcmp(mission->title, "Sack B.A.D. headquarters") ||
             !rb->strcmp(mission->title, "Another B.A.D. stronghold...") ||
             !rb->strcmp(mission->title, "Trash the complex..."))
    {
        g->floor_style = FLOOR_STYLE_DIRT;
        g->room_floor_style = FLOOR_STYLE_STONE;
        g->wall_style = WALL_STYLE_STONE;
    }
    else if (!rb->strcmp(mission->title, "Assassination attempt"))
    {
        g->floor_style = FLOOR_STYLE_DIRT;
        g->room_floor_style = FLOOR_STYLE_RECESSED;
        g->wall_style = WALL_STYLE_STONE;
    }
    else if (!rb->strcmp(mission->title, "Final showdown!"))
    {
        g->floor_style = FLOOR_STYLE_TILE;
        g->room_floor_style = FLOOR_STYLE_RECESSED;
        g->wall_style = WALL_STYLE_BRICK;
    }
}

static bool tile_is_room_floor(const struct game_state *g, const int map_x, const int map_y)
{
    int i;

    for (i = 0; i < g->room_count; ++i)
    {
        if (map_x >= g->room_x[i] && map_x < g->room_x[i] + g->room_w[i] &&
            map_y >= g->room_y[i] && map_y < g->room_y[i] + g->room_h[i])
            return true;
    }
    return false;
}

static const int *prop_cycle_for_mission(
    const struct game_state *g, int *count)
{
    static const int office_props[] = {
        PROP_CABINET, PROP_PLANT, PROP_BENCH, PROP_CHAIR, PROP_TABLE,
        PROP_BOOKSHELF, PROP_TABLE, PROP_SAFE
    };
    static const int warehouse_props[] = {
        PROP_BOX, PROP_BARREL_BLUE, PROP_BOX2, PROP_CABINET,
        PROP_BENCH, PROP_CHAIR, PROP_BARREL_WOOD, PROP_BOX, PROP_TABLE_STEEL
    };
    static const int stronghold_props[] = {
        PROP_BARREL_BLUE, PROP_BOX, PROP_BOX2, PROP_CABINET,
        PROP_CHAIR, PROP_BARREL_WOOD, PROP_BOX, PROP_TABLE_STEEL
    };
    const struct cdogs_mission_data *mission = &cdogs_campaign_missions[g->mission_index];

    if (!rb->strcmp(mission->title, "Storeroom plans") ||
        !rb->strcmp(mission->title, "B.A.D. storage raid") ||
        !rb->strcmp(mission->title, "Take out B.A.D. ring leader"))
    {
        *count = (int)ARRAYLEN(warehouse_props);
        return warehouse_props;
    }
    if (!rb->strcmp(mission->title, "Sack B.A.D. headquarters") ||
        !rb->strcmp(mission->title, "Another B.A.D. stronghold...") ||
        !rb->strcmp(mission->title, "Trash the complex...") ||
        !rb->strcmp(mission->title, "Rescue the chief of police"))
    {
        *count = (int)ARRAYLEN(stronghold_props);
        return stronghold_props;
    }

    *count = (int)ARRAYLEN(office_props);
    return office_props;
}

static const fb_data *collect_sprite_for_mission(
    const struct game_state *g, int *w, int *h)
{
    const struct cdogs_mission_data *mission = &cdogs_campaign_missions[g->mission_index];

    if (!rb->strcmp(mission->title, "Storeroom plans"))
    {
        *w = BMPWIDTH_cdogs_paper;
        *h = BMPHEIGHT_cdogs_paper;
        return cdogs_paper;
    }

    *w = BMPWIDTH_cdogs_sack;
    *h = BMPHEIGHT_cdogs_sack;
    return cdogs_sack;
}

struct enemy_template
{
    int max_hp;
    int action_delay;
    int move_chance;
    int track_chance;
    int shoot_chance;
    enum weapon_type weapon;
};

static const struct enemy_template enemy_templates[] =
{
    { 40, 10, 50, 35, 15, WEAPON_MACHINE_GUN },
    { 40, 10, 30, 35, 30, WEAPON_MACHINE_GUN },
    { 40, 10, 50, 35, 15, WEAPON_SHOTGUN },
    { 40, 10, 50, 35, 15, WEAPON_MACHINE_GUN }
};

static const struct cdogs_character_data *character_for_id(const int char_id)
{
    if (char_id < 0 || char_id >= cdogs_character_count)
        return NULL;
    return &cdogs_characters[char_id];
}

static int sprite_variant_for_character(const int char_id)
{
    return ((char_id % 4) + 4) % 4;
}

static enum objective_kind objective_kind_from_name(const char *name)
{
    if (!rb->strcmp(name, "Collect"))
        return OBJECTIVE_COLLECT;
    if (!rb->strcmp(name, "Destroy"))
        return OBJECTIVE_DESTROY;
    if (!rb->strcmp(name, "Rescue"))
        return OBJECTIVE_RESCUE;
    if (!rb->strcmp(name, "Explore"))
        return OBJECTIVE_EXPLORE;
    return OBJECTIVE_KILL;
}

static int pixels_per_tick_from_speed(const int speed)
{
    if (speed >= 256)
        return 2;
    if (speed >= 192)
        return 1;
    return 1;
}

static bool is_fire_immune_character(const int char_id)
{
    const struct cdogs_character_data *character = character_for_id(char_id);
    if (!character)
        return false;
    return !rb->strcmp(character->class_name, "Cyborg");
}

static bool mission_uses_target_kills(const struct cdogs_mission_data *mission)
{
    if (!mission || mission->special_count <= 0)
        return false;
    if (mission->objective_count <= 3)
        return true;
    if (rb->strstr(mission->objective, "leader"))
        return true;
    if (rb->strstr(mission->objective, "boss"))
        return true;
    return false;
}

static bool mission_is_final_showdown(const struct cdogs_mission_data *mission)
{
    return mission && !rb->strcmp(mission->title, "Final showdown!");
}

static void place_objective_in_room(
    const struct game_state *g, struct objective_entity *o, const int room,
    const int slot, const bool edge_bias)
{
    const int use_room = clampi(room, 0, max_i(0, g->room_count - 1));
    const int room_px = g->room_x[use_room] * CDOGS_TILE_W;
    const int room_py = g->room_y[use_room] * CDOGS_TILE_H;
    const int room_pw = g->room_w[use_room] * CDOGS_TILE_W;
    const int room_ph = g->room_h[use_room] * CDOGS_TILE_H;

    if (edge_bias)
    {
        o->x = room_px + CDOGS_TILE_W + ((slot & 1) ? room_pw - CDOGS_TILE_W * 3 : CDOGS_TILE_W);
        o->y = room_py + room_ph / 2 - o->size / 2;
    }
    else
    {
        o->x = room_px + max_i(4, room_pw / 2 - o->size / 2) + ((slot % 3) - 1) * CDOGS_TILE_W;
        o->y = room_py + max_i(4, room_ph / 2 - o->size / 2) + (((slot / 3) % 3) - 1) * (CDOGS_TILE_H / 2);
    }

    o->x = clampi(o->x, 2, world_w(g) - o->size - 2);
    o->y = clampi(o->y, 2, world_h(g) - o->size - 2);
}

static void apply_actor_burn(struct actor *a, const int ticks)
{
    if (!a->fire_immune)
        a->burn_timer = max_i(a->burn_timer, ticks);
}

static void apply_actor_poison(struct actor *a, const int ticks)
{
    a->poison_timer = max_i(a->poison_timer, ticks);
}

static void apply_actor_confusion(struct actor *a, const int ticks)
{
    a->confuse_timer = max_i(a->confuse_timer, ticks);
}

static void draw_actor_sprite_at(
    const struct actor *a, const fb_data *legs,
    const fb_data *upper, const int direction,
    const int anim_frame, const int screen_x, const int screen_y,
    const bool upper_is_composited)
{
    if (!a->active)
        return;

    if (!upper_is_composited)
        draw_sheet_frame(legs, direction, anim_frame, screen_x, screen_y);
    draw_sheet_frame(upper, direction, anim_frame, screen_x, screen_y);
}

static void draw_pulse_at(
    const struct bullet *b, const int frame, const unsigned fallback,
    const int screen_x, const int screen_y)
{
    if (!b->active)
        return;

    rb->lcd_bitmap_transparent_part(
        cdogs_pulse, frame * CDOGS_PULSE_FRAME_SIZE, 0, BMPWIDTH_cdogs_pulse,
        screen_x, screen_y, CDOGS_PULSE_FRAME_SIZE, CDOGS_PULSE_FRAME_SIZE);

    set_fg(fallback);
    rb->lcd_fillrect(screen_x + 2, screen_y + 2, 1, 1);
}

static void draw_prop(const int x, const int y, const int kind)
{
    switch (kind)
    {
    case PROP_TABLE_STEEL:
        rb->lcd_bitmap_transparent_part(
            cdogs_table_steel, 0, 0, BMPWIDTH_cdogs_table_steel, x, y,
            BMPWIDTH_cdogs_table_steel, BMPHEIGHT_cdogs_table_steel);
        break;
    case PROP_CABINET:
        rb->lcd_bitmap_transparent_part(
            cdogs_cabinet, 0, 0, BMPWIDTH_cdogs_cabinet, x, y,
            BMPWIDTH_cdogs_cabinet, BMPHEIGHT_cdogs_cabinet);
        break;
    case PROP_BOOKSHELF:
        rb->lcd_bitmap_transparent_part(
            cdogs_bookshelf, 0, 0, BMPWIDTH_cdogs_bookshelf, x, y,
            BMPWIDTH_cdogs_bookshelf, BMPHEIGHT_cdogs_bookshelf);
        break;
    case PROP_BENCH:
        rb->lcd_bitmap_transparent_part(
            cdogs_bench, 0, 0, BMPWIDTH_cdogs_bench, x, y,
            BMPWIDTH_cdogs_bench, BMPHEIGHT_cdogs_bench);
        break;
    case PROP_CHAIR:
        rb->lcd_bitmap_transparent_part(
            cdogs_chair, 0, 0, BMPWIDTH_cdogs_chair, x, y,
            BMPWIDTH_cdogs_chair, BMPHEIGHT_cdogs_chair);
        break;
    case PROP_SAFE:
        rb->lcd_bitmap_transparent_part(
            cdogs_safe, 0, 0, BMPWIDTH_cdogs_safe, x, y,
            BMPWIDTH_cdogs_safe, BMPHEIGHT_cdogs_safe);
        break;
    case PROP_PLANT:
        rb->lcd_bitmap_transparent_part(
            cdogs_plant, 0, 0, BMPWIDTH_cdogs_plant, x, y,
            BMPWIDTH_cdogs_plant, BMPHEIGHT_cdogs_plant);
        break;
    case PROP_BOX:
        rb->lcd_bitmap_transparent_part(
            cdogs_box, 0, 0, BMPWIDTH_cdogs_box, x, y,
            BMPWIDTH_cdogs_box, BMPHEIGHT_cdogs_box);
        break;
    case PROP_BOX2:
        rb->lcd_bitmap_transparent_part(
            cdogs_box2, 0, 0, BMPWIDTH_cdogs_box2, x, y,
            BMPWIDTH_cdogs_box2, BMPHEIGHT_cdogs_box2);
        break;
    case PROP_BARREL_BLUE:
        rb->lcd_bitmap_transparent_part(
            cdogs_barrel_blue, 0, 0, BMPWIDTH_cdogs_barrel_blue, x, y,
            BMPWIDTH_cdogs_barrel_blue, BMPHEIGHT_cdogs_barrel_blue);
        break;
    case PROP_BARREL_WOOD:
        rb->lcd_bitmap_transparent_part(
            cdogs_barrel_wood, 0, 0, BMPWIDTH_cdogs_barrel_wood, x, y,
            BMPWIDTH_cdogs_barrel_wood, BMPHEIGHT_cdogs_barrel_wood);
        break;
    default:
        rb->lcd_bitmap_transparent_part(
            cdogs_table, 0, 0, BMPWIDTH_cdogs_table, x, y,
            BMPWIDTH_cdogs_table, BMPHEIGHT_cdogs_table);
        break;
    }
}

static void draw_shadow_ellipse(
    const int x, const int y, const int w, const int h)
{
    set_fg(COLOR_SHADOW);
    rb->lcd_fillrect(x, y, w, h);
}

static void draw_environment(const struct game_state *g, const int cam_x, const int cam_y)
{
    int i;
    int map_x, map_y;
    const fb_data *wall_n = wall_n_for_style(g->wall_style);
    const fb_data *wall_w = wall_w_for_style(g->wall_style);
    const int inner_x = g->field_x + 1;
    const int inner_y = g->field_y + 1;
    const int inner_w = g->field_w - 2;
    const int inner_h = g->field_h - 2;
    const int tile_w = CDOGS_TILE_W;
    const int tile_h = CDOGS_TILE_H;
    const int first_tile_x = cam_x / tile_w;
    const int first_tile_y = cam_y / tile_h;
    const int offset_x = -(cam_x % tile_w);
    const int offset_y = -(cam_y % tile_h);

    set_fg(COLOR_BG);
    rb->lcd_fillrect(inner_x, inner_y, inner_w, inner_h);

#ifdef HAVE_LCD_COLOR
    for (map_y = first_tile_y;
         map_y < g->map_h && inner_y + offset_y + (map_y - first_tile_y) * tile_h < inner_y + inner_h;
         map_y++)
    {
        for (map_x = first_tile_x;
             map_x < g->map_w && inner_x + offset_x + (map_x - first_tile_x) * tile_w < inner_x + inner_w;
             map_x++)
        {
            uint8_t tile = g->map_tiles[map_index(g, map_x, map_y)];
            int draw_x = inner_x + offset_x + (map_x - first_tile_x) * tile_w;
            int draw_y = inner_y + offset_y + (map_y - first_tile_y) * tile_h;

            if (tile == TILE_FLOOR)
            {
                int floor_idx = (map_x + map_y) % CDOGS_FLOOR_FRAMES;
                const fb_data *floor_sheet = floor_sheet_for_style(
                    tile_is_room_floor(g, map_x, map_y) ?
                    g->room_floor_style : g->floor_style);
                const bool north_wall =
                    map_y > 0 && g->map_tiles[map_index(g, map_x, map_y - 1)] == TILE_WALL;
                const bool west_wall =
                    map_x > 0 && g->map_tiles[map_index(g, map_x - 1, map_y)] == TILE_WALL;
                const bool south_wall =
                    map_y + 1 < g->map_h &&
                    g->map_tiles[map_index(g, map_x, map_y + 1)] == TILE_WALL;
                const bool east_wall =
                    map_x + 1 < g->map_w &&
                    g->map_tiles[map_index(g, map_x + 1, map_y)] == TILE_WALL;
                if (((map_x ^ map_y) & 1) == 0)
                {
                    set_fg(COLOR_FLOOR_SHADE);
                    rb->lcd_fillrect(draw_x, draw_y, tile_w, tile_h);
                }
                rb->lcd_bitmap_transparent_part(
                    floor_sheet, 0, floor_idx * tile_h, BMPWIDTH_cdogs_floor_tile,
                    draw_x, draw_y, tile_w, tile_h);
                if (north_wall)
                {
                    set_fg(COLOR_SHADOW);
                    rb->lcd_fillrect(draw_x, draw_y, tile_w, 2);
                }
                if (west_wall)
                {
                    set_fg(COLOR_SHADOW);
                    rb->lcd_fillrect(draw_x, draw_y, 2, tile_h);
                }
                if (south_wall)
                {
                    set_fg(COLOR_BG_ALT);
                    rb->lcd_fillrect(draw_x, draw_y + tile_h - 2, tile_w, 2);
                }
                if (east_wall)
                {
                    set_fg(COLOR_BG_ALT);
                    rb->lcd_fillrect(draw_x + tile_w - 2, draw_y, 2, tile_h);
                }
                if (((map_x + map_y) % 3) == 0)
                {
                    set_fg(COLOR_BG_ALT);
                    rb->lcd_fillrect(draw_x + 3, draw_y + 3, 2, 2);
                }
            }
            else if (tile == TILE_WALL)
            {
                set_fg(COLOR_PANEL_ALT);
                rb->lcd_fillrect(draw_x, draw_y, tile_w, tile_h);
                set_fg(COLOR_TEXT_DIM);
                rb->lcd_fillrect(draw_x, draw_y, tile_w, 1);
                if (map_y == 0 || g->map_tiles[map_index(g, map_x, map_y - 1)] != TILE_WALL)
                {
                    rb->lcd_bitmap_transparent_part(
                        wall_n, 0, 0, BMPWIDTH_cdogs_wall_n_steel, draw_x, draw_y, tile_w, tile_h);
                }
                if (map_x == 0 || g->map_tiles[map_index(g, map_x - 1, map_y)] != TILE_WALL)
                {
                    set_fg(COLOR_WALL_SIDE);
                    rb->lcd_fillrect(draw_x, draw_y, 3, tile_h);
                    rb->lcd_bitmap_transparent_part(
                        wall_w, 0, 0, BMPWIDTH_cdogs_wall_w_steel, draw_x, draw_y, tile_w, tile_h);
                }
            }
            else if (tile == TILE_DOOR_CLOSED)
            {
                set_fg(COLOR_PANEL_ALT);
                rb->lcd_fillrect(draw_x, draw_y, tile_w, tile_h);
                set_fg(COLOR_WALL_SIDE);
                rb->lcd_fillrect(draw_x, draw_y, 3, tile_h);
                set_fg(COLOR_TEXT_DIM);
                rb->lcd_fillrect(draw_x + 4, draw_y + 2, tile_w - 8, 2);
                set_fg(COLOR_BG_ALT);
                rb->lcd_fillrect(draw_x + 4, draw_y + tile_h - 3, tile_w - 8, 1);
            }
        }
    }

    set_fg(COLOR_BORDER);
    rb->lcd_drawrect(inner_x, inner_y, inner_w, inner_h);
#else
    for (map_y = first_tile_y;
         map_y < g->map_h && inner_y + offset_y + (map_y - first_tile_y) * tile_h < inner_y + inner_h;
         map_y++)
    {
        for (map_x = first_tile_x;
             map_x < g->map_w && inner_x + offset_x + (map_x - first_tile_x) * tile_w < inner_x + inner_w;
             map_x++)
        {
            uint8_t tile = g->map_tiles[map_index(g, map_x, map_y)];
            int draw_x = inner_x + offset_x + (map_x - first_tile_x) * tile_w;
            int draw_y = inner_y + offset_y + (map_y - first_tile_y) * tile_h;

            if (tile == TILE_WALL)
                set_fg(LCD_WHITE);
            else if (tile == TILE_DOOR_CLOSED)
                set_fg(LCD_WHITE);
            else
                set_fg(LCD_BLACK);

            rb->lcd_fillrect(draw_x, draw_y, tile_w, tile_h);
        }
    }
#endif

    for (i = 0; i < g->prop_count; ++i)
    {
        const int prop_x = g->props[i].x;
        const int prop_y = g->props[i].y;
        const int kind = g->props[i].kind;
        if (prop_x >= 0 && prop_x < g->map_w && prop_y >= 0 && prop_y < g->map_h)
        {
            if (g->map_tiles[map_index(g, prop_x, prop_y)] == TILE_FLOOR)
            {
                const int draw_x = inner_x + prop_x * tile_w - cam_x;
                const int draw_y = inner_y + prop_y * tile_h - cam_y;
                if (draw_x > inner_x - 24 && draw_x < inner_x + inner_w &&
                    draw_y > inner_y - 24 && draw_y < inner_y + inner_h)
                {
                    draw_shadow_ellipse(draw_x + 1, draw_y + tile_h - 1, tile_w - 2, 3);
                    draw_prop(draw_x, draw_y, kind);
                }
            }
        }
    }
}

static void place_actor_in_room(
    const struct game_state *g, struct actor *a, const int room)
{
    const int use_room = clampi(room, 0, max_i(0, g->room_count - 1));
    a->x = g->room_x[use_room] * CDOGS_TILE_W +
        max_i(2, (g->room_w[use_room] * CDOGS_TILE_W) / 2 - a->size / 2);
    a->y = g->room_y[use_room] * CDOGS_TILE_H +
        max_i(2, (g->room_h[use_room] * CDOGS_TILE_H) / 2 - a->size / 2);
    a->x = clampi(a->x, 2, world_w(g) - a->size - 2);
    a->y = clampi(a->y, 2, world_h(g) - a->size - 2);
}

static int mission_spawn_character_id(
    struct game_state *g, const struct cdogs_mission_data *mission,
    const int index, bool *objective_target)
{
    *objective_target = false;

    if (g->objective_kind == OBJECTIVE_KILL &&
        mission_uses_target_kills(mission) &&
        g->objective_spawned < g->objective_required)
    {
        *objective_target = true;
        return mission->special_ids[g->objective_spawned % max_i(1, mission->special_count)];
    }
    if (mission->enemy_count > 0)
    {
        const int base = g->objective_spawned + index;
        return mission->enemy_ids[((base % mission->enemy_count) + mission->enemy_count) %
            mission->enemy_count];
    }
    if (mission->special_count > 0)
        return mission->special_ids[(g->objective_spawned + index) % mission->special_count];
    return 11;
}

static void respawn_enemy(struct game_state *g, const int index)
{
    struct actor *enemy = &g->enemies[index];
    const struct cdogs_mission_data *mission = &cdogs_campaign_missions[g->mission_index];
    const struct cdogs_character_data *character;
    bool objective_target = false;
    const int char_id = mission_spawn_character_id(g, mission, index, &objective_target);

    if (g->objective_spawned >= g->enemy_spawn_limit)
    {
        enemy->active = false;
        return;
    }

    enemy->size = 12;
    enemy->active = true;
    enemy->char_id = char_id;
    enemy->sprite_variant = sprite_variant_for_character(char_id);
    character = character_for_id(char_id);
    if (character)
    {
        enemy->weapon_type = weapon_type_from_name(character->gun);
        enemy->max_hp = character->max_health;
        enemy->hp = character->max_health;
        enemy->action_delay = max_i(1, character->action_delay);
        enemy->shoot_chance = character->probability_to_shoot;
        enemy->track_chance = character->probability_to_track;
        enemy->move_chance = character->probability_to_move;
        enemy->move_speed = pixels_per_tick_from_speed(character->speed);
    }
    else
    {
        const struct enemy_template *tmpl =
            &enemy_templates[enemy->sprite_variant % (int)ARRAYLEN(enemy_templates)];
        enemy->weapon_type = tmpl->weapon;
        enemy->max_hp = tmpl->max_hp;
        enemy->hp = tmpl->max_hp;
        enemy->action_delay = tmpl->action_delay;
        enemy->shoot_chance = tmpl->shoot_chance;
        enemy->track_chance = tmpl->track_chance;
        enemy->move_chance = tmpl->move_chance;
        enemy->move_speed = 2;
    }
    enemy->fire_cooldown = cdogs_rand_range(&g->rng, 0, enemy->action_delay);
    enemy->dir_x = 0;
    enemy->dir_y = 1;
    enemy->burn_timer = 0;
    enemy->poison_timer = 0;
    enemy->confuse_timer = 0;
    enemy->sleeping = false;
    enemy->waking = false;
    enemy->fire_immune = is_fire_immune_character(char_id);
    enemy->objective_target = objective_target;
    if (g->room_count > 0)
    {
        const int room = (index * 5 + 3 + g->objective_spawned) % g->room_count;
        place_actor_in_room(g, enemy, room);
    }
    else
    {
        enemy->x = world_w(g) / 2;
        enemy->y = world_h(g) / 2;
    }
    g->objective_spawned++;
}

struct bsp_area
{
    int x;
    int y;
    int w;
    int h;
    int parent;
    int child_a;
    int child_b;
};

static void carve_rect(
    uint8_t *tiles, const int map_w, const int x, const int y, const int w,
    const int h, const uint8_t tile)
{
    int ix, iy;

    for (iy = y; iy < y + h; ++iy)
    {
        for (ix = x; ix < x + w; ++ix)
            tiles[iy * map_w + ix] = tile;
    }
}

static void carve_hallway(
    uint8_t *tiles, const int map_w, const int x1, const int y1, const int x2,
    const int y2)
{
    int x = x1;
    int y = y1;

    while (x != x2)
    {
        tiles[y * map_w + x] = TILE_FLOOR;
        x += x < x2 ? 1 : -1;
    }
    while (y != y2)
    {
        tiles[y * map_w + x] = TILE_FLOOR;
        y += y < y2 ? 1 : -1;
    }
    tiles[y * map_w + x] = TILE_FLOOR;
}

static void add_door_between(
    uint8_t *tiles, const int map_w, const int x1, const int y1, const int x2,
    const int y2, const int map_h)
{
    const int mid_x = (x1 + x2) / 2;
    const int mid_y = (y1 + y2) / 2;
    int x;
    int y;

    for (x = mid_x - 2; x <= mid_x + 2; ++x)
    {
        if (x > 0 && x < map_w - 1 && mid_y > 0 && mid_y < map_h - 1 &&
            tiles[mid_y * map_w + x] == TILE_FLOOR)
        {
            if (tiles[(mid_y - 1) * map_w + x] == TILE_WALL ||
                tiles[(mid_y + 1) * map_w + x] == TILE_WALL)
            {
                tiles[mid_y * map_w + x] = TILE_DOOR_CLOSED;
                return;
            }
        }
    }
    for (y = mid_y - 2; y <= mid_y + 2; ++y)
    {
        if (mid_x > 0 && mid_x < map_w - 1 && y > 0 && y < map_h - 1 &&
            tiles[y * map_w + mid_x] == TILE_FLOOR)
        {
            if (tiles[y * map_w + mid_x - 1] == TILE_WALL ||
                tiles[y * map_w + mid_x + 1] == TILE_WALL)
            {
                tiles[y * map_w + mid_x] = TILE_DOOR_CLOSED;
                return;
            }
        }
    }
}

static void clear_level_state(struct game_state *g, const uint8_t fill)
{
    rb->memset(g->map_tiles, fill, sizeof(g->map_tiles));
    rb->memset(g->room_x, 0, sizeof(g->room_x));
    rb->memset(g->room_y, 0, sizeof(g->room_y));
    rb->memset(g->room_w, 0, sizeof(g->room_w));
    rb->memset(g->room_h, 0, sizeof(g->room_h));
    rb->memset(g->room_visited, 0, sizeof(g->room_visited));
    rb->memset(g->props, 0, sizeof(g->props));
    rb->memset(g->objectives, 0, sizeof(g->objectives));
    g->room_count = 0;
    g->prop_count = 0;
    g->objective_entity_count = 0;
}

static void add_room_record(
    struct game_state *g, const int x, const int y, const int w, const int h)
{
    if (g->room_count >= (int)ARRAYLEN(g->room_x))
        return;
    g->room_x[g->room_count] = (uint8_t)x;
    g->room_y[g->room_count] = (uint8_t)y;
    g->room_w[g->room_count] = (uint8_t)w;
    g->room_h[g->room_count] = (uint8_t)h;
    g->room_count++;
}

static void add_prop_tile(struct game_state *g, const int x, const int y, const int kind)
{
    if (g->prop_count >= (int)ARRAYLEN(g->props))
        return;
    g->props[g->prop_count].x = x;
    g->props[g->prop_count].y = y;
    g->props[g->prop_count].kind = kind;
    g->prop_count++;
}

static void add_prop_row(
    struct game_state *g, const int x1, const int x2, const int y,
    const int kind_a, const int kind_b)
{
    int x;

    for (x = x1; x <= x2; x += 3)
        add_prop_tile(g, x, y, ((x - x1) / 3) & 1 ? kind_b : kind_a);
}

static void generate_warehouse_level(struct game_state *g)
{
    int x, y;
    static const int room_defs[][4] = {
        { 2, 2, 12, 10 }, { 18, 2, 12, 10 }, { 34, 2, 12, 10 },
        { 2, 16, 12, 10 }, { 18, 16, 12, 10 }, { 34, 16, 12, 10 }
    };

    clear_level_state(g, TILE_WALL);
    for (y = 1; y < g->map_h - 1; ++y)
        for (x = 1; x < g->map_w - 1; ++x)
            g->map_tiles[map_index(g, x, y)] = TILE_WALL;

    carve_hallway(g->map_tiles, g->map_w, 8, 13, g->map_w - 9, 13);
    for (y = 5; y < g->map_h - 5; ++y)
        g->map_tiles[map_index(g, 16, y)] = TILE_FLOOR;
    for (y = 5; y < g->map_h - 5; ++y)
        g->map_tiles[map_index(g, 32, y)] = TILE_FLOOR;

    for (x = 0; x < (int)ARRAYLEN(room_defs); ++x)
    {
        carve_rect(g->map_tiles, g->map_w, room_defs[x][0], room_defs[x][1], room_defs[x][2], room_defs[x][3], TILE_FLOOR);
        add_room_record(g, room_defs[x][0], room_defs[x][1], room_defs[x][2], room_defs[x][3]);
    }

    g->map_tiles[map_index(g, 16, 6)] = TILE_DOOR_OPEN;
    g->map_tiles[map_index(g, 16, 20)] = TILE_DOOR_OPEN;
    g->map_tiles[map_index(g, 32, 6)] = TILE_DOOR_OPEN;
    g->map_tiles[map_index(g, 32, 20)] = TILE_DOOR_OPEN;
    g->map_tiles[map_index(g, 10, 13)] = TILE_DOOR_OPEN;
    g->map_tiles[map_index(g, 26, 13)] = TILE_DOOR_OPEN;
    g->map_tiles[map_index(g, 42, 13)] = TILE_DOOR_OPEN;

    add_prop_tile(g, 6, 6, PROP_BOX);
    add_prop_tile(g, 10, 6, PROP_BARREL_BLUE);
    add_prop_tile(g, 22, 6, PROP_BOX2);
    add_prop_tile(g, 26, 6, PROP_BENCH);
    add_prop_tile(g, 38, 6, PROP_BARREL_WOOD);
    add_prop_tile(g, 42, 6, PROP_TABLE_STEEL);
    add_prop_tile(g, 6, 20, PROP_BOX2);
    add_prop_tile(g, 10, 20, PROP_BARREL_WOOD);
    add_prop_tile(g, 22, 20, PROP_TABLE_STEEL);
    add_prop_tile(g, 26, 20, PROP_BENCH);
    add_prop_tile(g, 38, 20, PROP_BOX);
    add_prop_tile(g, 42, 20, PROP_BARREL_BLUE);

    add_prop_row(g, 4, 10, 4, PROP_BOX, PROP_BARREL_BLUE);
    add_prop_row(g, 20, 26, 4, PROP_BOX2, PROP_CABINET);
    add_prop_row(g, 36, 42, 4, PROP_BARREL_WOOD, PROP_TABLE_STEEL);
    add_prop_row(g, 4, 10, 8, PROP_TABLE_STEEL, PROP_BOX);
    add_prop_row(g, 20, 26, 8, PROP_BOX2, PROP_BARREL_BLUE);
    add_prop_row(g, 36, 42, 8, PROP_BOX, PROP_BARREL_WOOD);

    add_prop_row(g, 4, 10, 18, PROP_BOX2, PROP_BARREL_WOOD);
    add_prop_row(g, 20, 26, 18, PROP_TABLE_STEEL, PROP_BENCH);
    add_prop_row(g, 36, 42, 18, PROP_BOX, PROP_BARREL_BLUE);
    add_prop_row(g, 4, 10, 22, PROP_BOX, PROP_TABLE_STEEL);
    add_prop_row(g, 20, 26, 22, PROP_BOX2, PROP_BARREL_BLUE);
    add_prop_row(g, 36, 42, 22, PROP_BARREL_WOOD, PROP_BOX);

    add_prop_tile(g, 14, 11, PROP_CHAIR);
    add_prop_tile(g, 30, 11, PROP_CHAIR);
    add_prop_tile(g, 14, 15, PROP_CHAIR);
    add_prop_tile(g, 30, 15, PROP_CHAIR);
}

static void generate_hostage_level(struct game_state *g)
{
    clear_level_state(g, TILE_WALL);

    carve_rect(g->map_tiles, g->map_w, 2, 2, 18, g->map_h - 4, TILE_FLOOR);
    carve_rect(g->map_tiles, g->map_w, 24, 2, 18, g->map_h - 4, TILE_FLOOR);
    carve_hallway(g->map_tiles, g->map_w, 19, g->map_h / 2, 24, g->map_h / 2);
    g->map_tiles[map_index(g, 21, g->map_h / 2)] = TILE_DOOR_CLOSED;

    carve_rect(g->map_tiles, g->map_w, 6, 5, 8, 6, TILE_WALL);
    carve_rect(g->map_tiles, g->map_w, 30, 5, 8, 6, TILE_WALL);
    carve_rect(g->map_tiles, g->map_w, 30, g->map_h - 11, 8, 6, TILE_WALL);
    carve_rect(g->map_tiles, g->map_w, 6, g->map_h - 11, 8, 6, TILE_WALL);

    add_room_record(g, 2, 2, 18, g->map_h - 4);
    add_room_record(g, 24, 2, 18, g->map_h - 4);

    add_prop_tile(g, 4, 4, PROP_TABLE);
    add_prop_tile(g, 4, g->map_h - 5, PROP_BENCH);
    add_prop_tile(g, 39, 4, PROP_SAFE);
    add_prop_tile(g, 39, g->map_h - 5, PROP_BOX);
}

static void generate_stronghold_level(struct game_state *g)
{
    int y;

    clear_level_state(g, TILE_WALL);
    carve_rect(g->map_tiles, g->map_w, 2, 2, g->map_w - 4, g->map_h - 4, TILE_FLOOR);
    for (y = 4; y < g->map_h - 4; ++y)
    {
        g->map_tiles[map_index(g, g->map_w / 3, y)] = TILE_WALL;
        g->map_tiles[map_index(g, (g->map_w * 2) / 3, y)] = TILE_WALL;
    }
    g->map_tiles[map_index(g, g->map_w / 3, g->map_h / 3)] = TILE_DOOR_CLOSED;
    g->map_tiles[map_index(g, g->map_w / 3, (g->map_h * 2) / 3)] = TILE_DOOR_CLOSED;
    g->map_tiles[map_index(g, (g->map_w * 2) / 3, g->map_h / 3)] = TILE_DOOR_CLOSED;
    g->map_tiles[map_index(g, (g->map_w * 2) / 3, (g->map_h * 2) / 3)] = TILE_DOOR_CLOSED;

    add_room_record(g, 2, 2, g->map_w / 3 - 2, g->map_h - 4);
    add_room_record(g, g->map_w / 3 + 1, 2, g->map_w / 3 - 1, g->map_h - 4);
    add_room_record(g, (g->map_w * 2) / 3 + 1, 2, g->map_w / 3 - 3, g->map_h - 4);

    add_prop_tile(g, 5, 5, PROP_CABINET);
    add_prop_tile(g, 5, g->map_h - 6, PROP_BOX);
    add_prop_tile(g, g->map_w / 2, 5, PROP_SAFE);
    add_prop_tile(g, g->map_w / 2, g->map_h - 6, PROP_TABLE_STEEL);
    add_prop_tile(g, g->map_w - 7, 5, PROP_BARREL_BLUE);
    add_prop_tile(g, g->map_w - 7, g->map_h - 6, PROP_BARREL_WOOD);
}

static void generate_final_showdown_level(struct game_state *g)
{
    int x;
    int y;
    static const int pillar_tiles[][2] = {
        { 7, 5 }, { 16, 5 }, { 7, 10 }, { 16, 10 }
    };

    rb->memset(g->map_tiles, TILE_WALL, sizeof(g->map_tiles));
    rb->memset(g->room_x, 0, sizeof(g->room_x));
    rb->memset(g->room_y, 0, sizeof(g->room_y));
    rb->memset(g->room_w, 0, sizeof(g->room_w));
    rb->memset(g->room_h, 0, sizeof(g->room_h));
    rb->memset(g->room_visited, 0, sizeof(g->room_visited));
    rb->memset(g->props, 0, sizeof(g->props));
    rb->memset(g->objectives, 0, sizeof(g->objectives));
    g->room_count = 1;
    g->prop_count = 0;
    g->objective_entity_count = 0;
    g->room_x[0] = 1;
    g->room_y[0] = 1;
    g->room_w[0] = g->map_w - 2;
    g->room_h[0] = g->map_h - 2;

    for (y = 1; y < g->map_h - 1; ++y)
    {
        for (x = 1; x < g->map_w - 1; ++x)
            g->map_tiles[map_index(g, x, y)] = TILE_FLOOR;
    }

    for (x = 5; x < g->map_w - 5; ++x)
        g->map_tiles[map_index(g, x, 3)] = TILE_WALL;
    g->map_tiles[map_index(g, g->map_w / 2, 3)] = TILE_DOOR_OPEN;

    for (x = 0; x < (int)ARRAYLEN(pillar_tiles) && g->prop_count < (int)ARRAYLEN(g->props); ++x)
    {
        g->props[g->prop_count].x = pillar_tiles[x][0];
        g->props[g->prop_count].y = pillar_tiles[x][1];
        g->props[g->prop_count].kind = (x & 1) ? PROP_SAFE : PROP_TABLE;
        g->prop_count++;
    }
}

static void generate_level(struct game_state *g)
{
    const struct cdogs_mission_data *mission = &cdogs_campaign_missions[g->mission_index];
    struct bsp_area areas[128];
    int leaf_indices[64];
    int area_count = 1;
    int leaf_count = 1;
    int target_rooms = clampi((g->map_w * g->map_h) / 72, 12, (int)ARRAYLEN(leaf_indices));
    int i;
    unsigned rng = 0xC0D065u + (unsigned)(g->mission_index * 97);

    if (mission_is_final_showdown(mission))
    {
        generate_final_showdown_level(g);
        return;
    }
    if (!rb->strcmp(mission->title, "Storeroom plans") ||
        !rb->strcmp(mission->title, "B.A.D. storage raid"))
    {
        generate_warehouse_level(g);
        return;
    }
    if (!rb->strcmp(mission->title, "Hostage situation") ||
        !rb->strcmp(mission->title, "Rescue the chief of police"))
    {
        generate_hostage_level(g);
        return;
    }
    if (!rb->strcmp(mission->title, "Another B.A.D. stronghold...") ||
        !rb->strcmp(mission->title, "Trash the complex...") ||
        !rb->strcmp(mission->title, "Sack B.A.D. headquarters"))
    {
        generate_stronghold_level(g);
        return;
    }

    clear_level_state(g, TILE_WALL);

    areas[0].x = 1;
    areas[0].y = 1;
    areas[0].w = g->map_w - 2;
    areas[0].h = g->map_h - 2;
    areas[0].parent = -1;
    areas[0].child_a = -1;
    areas[0].child_b = -1;
    leaf_indices[0] = 0;

    while (leaf_count < target_rooms)
    {
        int best = -1;
        int best_span = 0;

        for (i = 0; i < leaf_count; ++i)
        {
            const struct bsp_area *a = &areas[leaf_indices[i]];
            const int span = max_i(a->w, a->h);
            if (span > best_span)
            {
                best = i;
                best_span = span;
            }
        }
        if (best < 0)
            break;

        {
            struct bsp_area *a = &areas[leaf_indices[best]];
            const bool split_horiz = a->w > a->h;
            const int min_room = 6;
            const int min_side = min_room * 2 + 3;
            struct bsp_area *b1;
            struct bsp_area *b2;
            int split_at;

            if ((split_horiz && a->w < min_side) || (!split_horiz && a->h < min_side) ||
                area_count + 2 >= (int)ARRAYLEN(areas))
                break;

            split_at = cdogs_rand_range(
                &rng, min_room + 1,
                (split_horiz ? a->w : a->h) - min_room - 2);

            a->child_a = area_count++;
            a->child_b = area_count++;
            b1 = &areas[a->child_a];
            b2 = &areas[a->child_b];
            *b1 = *a;
            *b2 = *a;
            b1->parent = leaf_indices[best];
            b2->parent = leaf_indices[best];
            b1->child_a = b1->child_b = -1;
            b2->child_a = b2->child_b = -1;

            if (split_horiz)
            {
                b1->w = split_at;
                b2->x = a->x + split_at;
                b2->w = a->w - split_at;
            }
            else
            {
                b1->h = split_at;
                b2->y = a->y + split_at;
                b2->h = a->h - split_at;
            }

            leaf_indices[best] = a->child_a;
            leaf_indices[leaf_count++] = a->child_b;
        }
    }

    for (i = 0; i < leaf_count && g->room_count < (int)ARRAYLEN(g->room_x); ++i)
    {
        const struct bsp_area *a = &areas[leaf_indices[i]];
        const int room_w = cdogs_rand_range(&rng, 6, min_i(10, a->w - 2));
        const int room_h = cdogs_rand_range(&rng, 6, min_i(10, a->h - 2));
        const int room_x = cdogs_rand_range(&rng, a->x + 1, a->x + a->w - room_w - 1);
        const int room_y = cdogs_rand_range(&rng, a->y + 1, a->y + a->h - room_h - 1);

        carve_rect(g->map_tiles, g->map_w, room_x, room_y, room_w, room_h, TILE_FLOOR);
        g->room_x[g->room_count] = (uint8_t)room_x;
        g->room_y[g->room_count] = (uint8_t)room_y;
        g->room_w[g->room_count] = (uint8_t)room_w;
        g->room_h[g->room_count] = (uint8_t)room_h;
        g->room_count++;
    }

    for (i = 0; i < area_count; ++i)
    {
        const struct bsp_area *a = &areas[i];
        if (a->child_a >= 0 && a->child_b >= 0)
        {
            const struct bsp_area *b1 = &areas[a->child_a];
            const struct bsp_area *b2 = &areas[a->child_b];
            const int c1x = b1->x + b1->w / 2;
            const int c1y = b1->y + b1->h / 2;
            const int c2x = b2->x + b2->w / 2;
            const int c2y = b2->y + b2->h / 2;
            carve_hallway(g->map_tiles, g->map_w, c1x, c1y, c2x, c2y);
            add_door_between(g->map_tiles, g->map_w, c1x, c1y, c2x, c2y, g->map_h);
        }
    }

    for (i = 0; i < g->room_count && g->prop_count < (int)ARRAYLEN(g->props); ++i)
    {
        int prop_kind_count;
        const int *prop_kinds = prop_cycle_for_mission(g, &prop_kind_count);
        const int room_x = g->room_x[i];
        const int room_y = g->room_y[i];
        const int room_w = g->room_w[i];
        const int room_h = g->room_h[i];
        int local_slots = 1 + ((room_w * room_h) >= 56 ? 1 : 0);
        int j;
        for (j = 0; j < local_slots && g->prop_count < (int)ARRAYLEN(g->props); ++j)
        {
            const int px = room_x + 1 + ((j * 3 + i) % max_i(1, room_w - 2));
            const int py = room_y + 1 + ((j * 2 + i) % max_i(1, room_h - 2));
            if (g->map_tiles[map_index(g, px, py)] != TILE_FLOOR ||
                prop_blocks_tile(g, px, py))
                continue;
            g->props[g->prop_count].x = px;
            g->props[g->prop_count].y = py;
            g->props[g->prop_count].kind = prop_kinds[(i + j) % prop_kind_count];
            g->prop_count++;
        }
    }
}

static void configure_mission(struct game_state *g, const int mission_index)
{
    const struct cdogs_mission_data *mission = &cdogs_campaign_missions[mission_index];
    int i;

    rb->memset(g->enemies, 0, sizeof(g->enemies));
    rb->memset(g->bullets, 0, sizeof(g->bullets));
    rb->memset(g->enemy_bullets, 0, sizeof(g->enemy_bullets));

    g->mission_index = mission_index;
    g->map_w = clampi(mission->width, 24, MAP_MAX_W);
    g->map_h = clampi(mission->height, 24, MAP_MAX_H);
    g->rng = 0xC0D065u + (unsigned)(mission_index * 977);
    g->player_primary = weapon_type_from_name(mission->weapon_b);
    g->player_secondary = weapon_type_from_name(mission->weapon_c);
    g->objective_kind = objective_kind_from_name(mission->objective_type);
    g->objective_required = mission->objective_required;
    g->objective_progress = 0;
    g->objective_spawned = 0;
    g->enemy_spawn_limit = mission->objective_required;
    g->wave = 1;
    g->score = mission_index * 100;
    g->fire_cooldown = 0;
    g->grenade_cooldown = 0;
    g->enemy_fire_cooldown = 0;
    g->damage_flash = 0;
    g->grenade_flash = 0;
    g->enemy_step = 0;
    g->player_shot_timer = 0;
    g->mission_time = 0;
    g->hud_message_ticks = 0;
    g->hud_message[0] = '\0';
    g->show_map = false;
    g->combo_latched = false;
    g->mission_failed = false;
    g->complete_timer = 0;
    configure_mission_visuals(g, mission_index);

    g->field_x = 2;
#ifdef HAVE_LCD_COLOR
    g->field_y = BMPHEIGHT_back_bar + 2;
#else
    g->field_y = g->font_h * 3 + 8;
#endif
    g->field_w = LCD_WIDTH - 4;
    g->field_h = LCD_HEIGHT - g->field_y - 4;

    g->player.size = 12;
    g->player.max_hp = 20;
    g->player.hp = 20;
    g->player.active = true;
    g->player.x = 4 * CDOGS_TILE_W;
    g->player.y = 24 * CDOGS_TILE_H;
    g->facing_x = 0;
    g->facing_y = -1;
    g->player_anim_offset = 0;
    g->player_anim_frame = 0;
    g->available_enemies = clampi(max_i(1, mission->enemy_density / 2), 1, (int)ARRAYLEN(g->enemies));
    if (mission_uses_target_kills(mission))
        g->enemy_spawn_limit = g->objective_required + max_i(g->available_enemies, mission->enemy_density);
    if (mission_is_final_showdown(mission))
        g->enemy_spawn_limit = g->objective_required;
    if (g->objective_kind != OBJECTIVE_KILL)
        g->enemy_spawn_limit = max_i(g->available_enemies * 2, mission->enemy_density + mission->objective_required);

    generate_level(g);

    if (g->room_count > 0)
    {
        if (mission_is_final_showdown(mission))
        {
            g->player.x = world_w(g) / 2 - g->player.size / 2;
            g->player.y = CDOGS_TILE_H * 2;
            g->room_visited[0] = 1;
        }
        else
        {
            const int start_room = 0;
            place_actor_in_room(g, &g->player, start_room);
            g->room_visited[start_room] = 1;
        }
        if (g->objective_kind == OBJECTIVE_EXPLORE)
            g->objective_progress = 1;
    }

    if (g->objective_kind != OBJECTIVE_KILL && g->room_count > 1)
    {
        const int place_count = min_i(
            mission->objective_count, min_i((int)ARRAYLEN(g->objectives), max_i(1, g->room_count - 1)));
        for (i = 0; i < place_count; ++i)
        {
            const int room = g->room_count - 1 - (i % max_i(1, g->room_count - 1));
            struct objective_entity *o = &g->objectives[g->objective_entity_count++];
            o->size = 12;
            o->hp = 6;
            o->kind = g->objective_kind;
            o->following = false;
            o->active = true;
            if (g->objective_kind == OBJECTIVE_DESTROY)
            {
                o->size = 14;
                o->hp = 10;
                place_objective_in_room(g, o, room, i, true);
            }
            else if (g->objective_kind == OBJECTIVE_RESCUE)
            {
                o->hp = 4;
                place_objective_in_room(g, o, room, i, false);
            }
            else
            {
                place_objective_in_room(g, o, room, i, false);
            }
        }
    }
    if (g->objective_kind == OBJECTIVE_EXPLORE)
        g->objective_required = min_i(g->objective_required, g->room_count);

    for (i = 0; i < g->available_enemies; ++i)
        respawn_enemy(g, i);
    for (; i < (int)ARRAYLEN(g->enemies); ++i)
        g->enemies[i].active = false;

    if (mission_is_final_showdown(mission))
    {
        for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
        {
            if (!g->enemies[i].active || !g->enemies[i].objective_target)
                continue;
            g->enemies[i].x = world_w(g) / 2 - g->enemies[i].size / 2;
            g->enemies[i].y = world_h(g) - CDOGS_TILE_H * 4;
            g->enemies[i].sleeping = false;
            g->enemies[i].waking = false;
            break;
        }
    }
}

static void reset_game(struct game_state *g)
{
    rb->memset(g, 0, sizeof(*g));
    rb->lcd_getstringsize("A", NULL, &g->font_h);
    g->screen = CDOGS_SCREEN_TITLE;
    g->selected_mission = 0;
    g->briefing_scroll = 0;
    g->rng = 0xCD065123u;
    audio_set_bgm(&clip_menu_music, true);
}

static void hud_message(struct game_state *g, const char *msg, const int ticks)
{
    rb->strlcpy(g->hud_message, msg, sizeof(g->hud_message));
    g->hud_message_ticks = ticks;
}

static bool projectile_hits_wall(const struct game_state *g, const struct bullet *b)
{
    const int tile_x = b->x / CDOGS_TILE_W;
    const int tile_y = b->y / CDOGS_TILE_H;
    if (tile_x < 0 || tile_x >= g->map_w || tile_y < 0 || tile_y >= g->map_h)
        return true;
    return g->map_tiles[map_index(g, tile_x, tile_y)] == TILE_WALL ||
        g->map_tiles[map_index(g, tile_x, tile_y)] == TILE_DOOR_CLOSED ||
        prop_blocks_tile(g, tile_x, tile_y);
}

static bool projectile_advance(struct game_state *g, struct bullet *b)
{
    const int next_x = b->x + b->vx;
    const int next_y = b->y + b->vy;
    struct bullet probe = *b;
    bool bounced = false;

    probe.x = next_x;
    probe.y = b->y;
    if (projectile_hits_wall(g, &probe))
    {
        if (weapon_bounces(b))
        {
            b->vx = -b->vx;
            bounced = true;
        }
        else
            return false;
    }
    else
        b->x = next_x;

    probe.x = b->x;
    probe.y = next_y;
    if (projectile_hits_wall(g, &probe))
    {
        if (weapon_bounces(b))
        {
            b->vy = -b->vy;
            bounced = true;
        }
        else
            return false;
    }
    else
        b->y = next_y;

    if (bounced)
        b->ttl = max_i(1, b->ttl - 1);
    return true;
}

static void damage_player(struct game_state *g, const int amount)
{
    if (amount <= 0 || g->damage_flash > 0)
        return;

    g->player.hp -= amount;
    g->damage_flash = HZ / 2;
    if (g->player.hp <= 0)
    {
        g->mission_failed = true;
        g->screen = CDOGS_SCREEN_GAME_OVER;
        audio_set_bgm(&clip_menu_music, true);
        audio_play_sfx(&clip_menu_back);
    }
}

static void on_enemy_killed(struct game_state *g, const struct actor *enemy)
{
    const struct cdogs_mission_data *mission = &cdogs_campaign_missions[g->mission_index];
    g->score += 10;
    if (g->objective_kind == OBJECTIVE_KILL)
    {
        if (!mission_uses_target_kills(mission) || (enemy && enemy->objective_target))
            g->objective_progress++;
    }
}

static void apply_status_to_enemies_in_radius(
    struct game_state *g, const int center_x, const int center_y,
    const int radius, const enum projectile_kind kind)
{
    int i;

    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        struct actor *enemy = &g->enemies[i];
        const int ex = enemy->x + enemy->size / 2;
        const int ey = enemy->y + enemy->size / 2;
        const int dx = ex - center_x;
        const int dy = ey - center_y;

        if (!enemy->active || dx * dx + dy * dy > radius * radius)
            continue;

        if (kind == PROJ_FLAME)
            apply_actor_burn(enemy, HZ * 2);
        else if (kind == PROJ_GRENADE)
            apply_actor_confusion(enemy, HZ * 3);
        else if (kind == PROJ_CHEMO_BOMB)
            apply_actor_poison(enemy, HZ * 4);
    }
}

static void damage_enemies_in_radius(
    struct game_state *g, const int center_x, const int center_y,
    const int radius, const int damage)
{
    int i;
    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        struct actor *enemy = &g->enemies[i];
        const int ex = enemy->x + enemy->size / 2;
        const int ey = enemy->y + enemy->size / 2;
        const int dx = ex - center_x;
        const int dy = ey - center_y;
        if (!enemy->active)
            continue;
        if (dx * dx + dy * dy > radius * radius)
            continue;
        enemy->hp -= damage;
        if (enemy->hp <= 0)
        {
            enemy->active = false;
            on_enemy_killed(g, enemy);
        }
    }
}

static void spawn_projectile(
    struct bullet *pool, const int pool_len, const int origin_x,
    const int origin_y, const int vx, const int vy,
    const struct weapon_spec *spec)
{
    int i;
    for (i = 0; i < pool_len; ++i)
    {
        struct bullet *b = &pool[i];
        if (b->active)
            continue;
        b->active = true;
        b->x = origin_x;
        b->y = origin_y;
        b->vx = vx;
        b->vy = vy;
        b->ttl = spec->ttl;
        b->damage = spec->damage;
        b->splash = spec->splash;
        b->kind = spec->projectile_kind;
        b->source_weapon = spec->type;
        return;
    }
}

static void explode_projectile(
    struct game_state *g, struct bullet *b, const bool friendly)
{
    const int radius = max_i(16, b->splash);
    const int player_dx = g->player.x + g->player.size / 2 - b->x;
    const int player_dy = g->player.y + g->player.size / 2 - b->y;
    const bool hits_player =
        player_dx * player_dx + player_dy * player_dy <= radius * radius;

    if (friendly)
    {
        damage_enemies_in_radius(g, b->x, b->y, radius, max_i(1, b->damage));
        if (b->source_weapon == WEAPON_MOLOTOVS)
            apply_status_to_enemies_in_radius(g, b->x, b->y, radius, PROJ_FLAME);
        else if (b->source_weapon == WEAPON_CONFUSION_BOMBS)
            apply_status_to_enemies_in_radius(g, b->x, b->y, radius, PROJ_GRENADE);
        else if (b->source_weapon == WEAPON_CHEMO_BOMBS)
            apply_status_to_enemies_in_radius(g, b->x, b->y, radius, PROJ_CHEMO_BOMB);
    }
    else if (hits_player)
    {
        damage_player(g, max_i(1, b->damage));
        if (b->source_weapon == WEAPON_CONFUSION_BOMBS)
        {
            g->player_confuse_timer = max_i(g->player_confuse_timer, HZ * 3);
            hud_message(g, "Confused", HZ);
        }
        else if (b->source_weapon == WEAPON_CHEMO_BOMBS)
        {
            g->player_poison_timer = max_i(g->player_poison_timer, HZ * 4);
            hud_message(g, "Poisoned", HZ);
        }
        else if (b->source_weapon == WEAPON_MOLOTOVS)
        {
            g->player_burn_timer = max_i(g->player_burn_timer, HZ * 2);
            hud_message(g, "Burning", HZ);
        }
    }

    if (b->kind == PROJ_FLAME)
    {
        if (friendly)
            apply_status_to_enemies_in_radius(g, b->x, b->y, radius, PROJ_FLAME);
        else if (hits_player)
            g->player_burn_timer = max_i(g->player_burn_timer, HZ * 2);
    }
    else if (b->source_weapon == WEAPON_CHEMO_BOMBS && friendly)
    {
        hud_message(g, "Chemo cloud", HZ);
    }
    else if (b->source_weapon == WEAPON_CONFUSION_BOMBS && friendly)
    {
        hud_message(g, "Confusion blast", HZ);
    }

    if (b->kind == PROJ_SHRAPNEL_BOMB)
    {
        int dir;
        const struct weapon_spec *shard = weapon_spec_for_type(WEAPON_SHOTGUN);
        for (dir = 0; dir < 16; ++dir)
        {
            static const int dirs[16][2] = {
                { 0, -4 }, { 1, -4 }, { 3, -3 }, { 4, -1 },
                { 4, 0 }, { 4, 1 }, { 3, 3 }, { 1, 4 },
                { 0, 4 }, { -1, 4 }, { -3, 3 }, { -4, 1 },
                { -4, 0 }, { -4, -1 }, { -3, -3 }, { -1, -4 }
            };
            int vx, vy;
            aim_velocity(dirs[dir][0], dirs[dir][1], 5, &vx, &vy);
            spawn_projectile(
                friendly ? g->bullets : g->enemy_bullets,
                friendly ? (int)ARRAYLEN(g->bullets) : (int)ARRAYLEN(g->enemy_bullets),
                b->x, b->y, vx, vy, shard);
        }
    }
    else if (b->source_weapon == WEAPON_MOLOTOVS)
    {
        if (friendly)
            apply_status_to_enemies_in_radius(g, b->x, b->y, radius, PROJ_FLAME);
        else if (hits_player)
            g->player_burn_timer = max_i(g->player_burn_timer, HZ * 2);
    }
    g->grenade_flash = 8;
    b->active = false;
}

static void fire_bullet(struct game_state *g)
{
    const struct weapon_spec *spec = weapon_spec_for_type((enum weapon_type)g->player_primary);
    const int origin_x = g->player.x + g->player.size / 2;
    const int origin_y = g->player.y + g->player.size / 2;
    int dir_x = g->facing_x;
    int dir_y = g->facing_y;
    int pellet;

    if (spec->type == WEAPON_NONE || spec->type == WEAPON_KNIFE)
        return;

    if (dir_x == 0 && dir_y == 0)
        dir_y = -1;

    for (pellet = 0; pellet < spec->pellets; ++pellet)
    {
        int vx, vy;
        int perp_x = -dir_y;
        int perp_y = dir_x;
        int offset = pellet - spec->pellets / 2;
        aim_velocity(dir_x, dir_y, spec->speed, &vx, &vy);
        vx += perp_x * offset * spec->spread;
        vy += perp_y * offset * spec->spread;
        if (spec->type == WEAPON_MACHINE_GUN)
        {
            vx += cdogs_rand_range(&g->rng, -spec->spread, spec->spread);
            vy += cdogs_rand_range(&g->rng, -spec->spread, spec->spread);
        }
        if (spec->type == WEAPON_FLAMER)
        {
            vx += cdogs_rand_range(&g->rng, -2, 2);
            vy += cdogs_rand_range(&g->rng, -2, 2);
        }
        spawn_projectile(
            g->bullets, (int)ARRAYLEN(g->bullets),
            origin_x, origin_y, vx, vy, spec);
    }
    g->fire_cooldown = spec->cooldown;
    g->player_shot_timer = 10;
    audio_play_sfx(shot_clip_for_weapon(spec->type));
}

static void fire_secondary_weapon(struct game_state *g)
{
    const struct weapon_spec *spec = weapon_spec_for_type((enum weapon_type)g->player_secondary);
    int dir_x = g->facing_x;
    int dir_y = g->facing_y;
    int vx, vy;

    if (spec->type == WEAPON_NONE || spec->type == WEAPON_KNIFE)
        return;

    if (dir_x == 0 && dir_y == 0)
        dir_y = -1;

    aim_velocity(dir_x, dir_y, spec->speed, &vx, &vy);
    if (spec->type == WEAPON_FLAMER)
    {
        vx += cdogs_rand_range(&g->rng, -2, 2);
        vy += cdogs_rand_range(&g->rng, -2, 2);
    }
    spawn_projectile(
        g->bullets, (int)ARRAYLEN(g->bullets),
        g->player.x + g->player.size / 2, g->player.y + g->player.size / 2,
        vx, vy, spec);
    g->grenade_cooldown = spec->cooldown;
    g->player_shot_timer = 10;
    audio_play_sfx(shot_clip_for_weapon(spec->type));
}

static void fire_enemy_bullet(struct game_state *g, const struct actor *enemy)
{
    const struct weapon_spec *spec = weapon_spec_for_type((enum weapon_type)enemy->weapon_type);
    const int dir_x = step_dir(g->player.x - enemy->x);
    const int dir_y = step_dir(g->player.y - enemy->y);
    int pellet;

    if (spec->type == WEAPON_NONE || spec->type == WEAPON_KNIFE)
        return;

    for (pellet = 0; pellet < spec->pellets; ++pellet)
    {
        int vx, vy;
        int perp_x = -dir_y;
        int perp_y = dir_x;
        int offset = pellet - spec->pellets / 2;
        aim_velocity(dir_x, dir_y, spec->speed, &vx, &vy);
        vx += perp_x * offset * spec->spread;
        vy += perp_y * offset * spec->spread;
        if (spec->type == WEAPON_MACHINE_GUN || spec->type == WEAPON_FLAMER)
        {
            vx += cdogs_rand_range(&g->rng, -spec->spread, spec->spread);
            vy += cdogs_rand_range(&g->rng, -spec->spread, spec->spread);
        }
        spawn_projectile(
            g->enemy_bullets, (int)ARRAYLEN(g->enemy_bullets),
            enemy->x + enemy->size / 2, enemy->y + enemy->size / 2, vx, vy, spec);
    }
}

static void update_status_effects(struct game_state *g)
{
    int i;

    if (g->player_burn_timer > 0)
    {
        g->player_burn_timer--;
        if ((g->player_burn_timer % (HZ / 4)) == 0)
            damage_player(g, 1);
    }
    if (g->player_poison_timer > 0)
    {
        g->player_poison_timer--;
        if ((g->player_poison_timer % (HZ / 2)) == 0)
            damage_player(g, 1);
    }
    if (g->player_confuse_timer > 0)
        g->player_confuse_timer--;

    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        struct actor *enemy = &g->enemies[i];
        if (!enemy->active)
            continue;

        if (enemy->burn_timer > 0)
        {
            enemy->burn_timer--;
            if ((enemy->burn_timer % (HZ / 4)) == 0 && !enemy->fire_immune)
                enemy->hp--;
        }
        if (enemy->poison_timer > 0)
        {
            enemy->poison_timer--;
            if ((enemy->poison_timer % (HZ / 2)) == 0)
                enemy->hp--;
        }
        if (enemy->confuse_timer > 0)
            enemy->confuse_timer--;

        if (enemy->hp <= 0)
        {
            enemy->active = false;
            on_enemy_killed(g, enemy);
        }
    }
}

static void update_player(struct game_state *g, const int cmd)
{
    int effective_cmd = cmd;
    int dx = 0;
    int dy = 0;
    const int speed = 3;
    int new_x, new_y;
    int i;

    if (g->player_confuse_timer > 0)
    {
        const int horizontal = effective_cmd & (CDOGS_CMD_LEFT | CDOGS_CMD_RIGHT);
        const int vertical = effective_cmd & (CDOGS_CMD_UP | CDOGS_CMD_DOWN);
        effective_cmd &= ~(CDOGS_CMD_LEFT | CDOGS_CMD_RIGHT | CDOGS_CMD_UP | CDOGS_CMD_DOWN);
        if (horizontal & CDOGS_CMD_LEFT)
            effective_cmd |= CDOGS_CMD_RIGHT;
        if (horizontal & CDOGS_CMD_RIGHT)
            effective_cmd |= CDOGS_CMD_LEFT;
        if (vertical & CDOGS_CMD_UP)
            effective_cmd |= CDOGS_CMD_DOWN;
        if (vertical & CDOGS_CMD_DOWN)
            effective_cmd |= CDOGS_CMD_UP;
    }

    if (effective_cmd & CDOGS_CMD_LEFT)
        dx -= speed;
    if (effective_cmd & CDOGS_CMD_RIGHT)
        dx += speed;
    if (effective_cmd & CDOGS_CMD_UP)
        dy -= speed;
    if (effective_cmd & CDOGS_CMD_DOWN)
        dy += speed;

    if (dx != 0 || dy != 0)
    {
        g->facing_x = dx < 0 ? -1 : (dx > 0 ? 1 : 0);
        g->facing_y = dy < 0 ? -1 : (dy > 0 ? 1 : 0);

        new_x = g->player.x + dx;
        new_y = g->player.y + dy;

        new_x = clampi(new_x, 1, world_w(g) - g->player.size - 1);
        new_y = clampi(new_y, 1, world_h(g) - g->player.size - 1);

        try_move_actor(g, &g->player, new_x, new_y, true);

        g->player_anim_frame++;
    }
    else
    {
        g->player_anim_frame = 0;
    }

    if ((effective_cmd & CDOGS_CMD_FIRE) && g->fire_cooldown <= 0)
        fire_bullet(g);
    if ((effective_cmd & CDOGS_CMD_GRENADE) && g->grenade_cooldown <= 0)
        fire_secondary_weapon(g);

    if (g->objective_kind == OBJECTIVE_EXPLORE)
    {
        for (i = 0; i < g->room_count; ++i)
        {
            const int rx = g->room_x[i] * CDOGS_TILE_W;
            const int ry = g->room_y[i] * CDOGS_TILE_H;
            const int rw = g->room_w[i] * CDOGS_TILE_W;
            const int rh = g->room_h[i] * CDOGS_TILE_H;
            if (g->room_visited[i])
                continue;
            if (!intersects_rect(g->player.x, g->player.y, g->player.size, g->player.size, rx, ry, rw, rh))
                continue;
            g->room_visited[i] = 1;
            g->objective_progress++;
            audio_play_sfx(&clip_pickup);
            hud_message(g, "Area explored", HZ);
        }
    }
    else if (g->objective_kind == OBJECTIVE_COLLECT || g->objective_kind == OBJECTIVE_RESCUE)
    {
        for (i = 0; i < g->objective_entity_count; ++i)
        {
            struct objective_entity *o = &g->objectives[i];
            if (!o->active)
                continue;
            if (!intersects(g->player.x, g->player.y, g->player.size, o->x, o->y, o->size))
                continue;
            if (g->objective_kind == OBJECTIVE_RESCUE)
            {
                if (!o->following)
                {
                    audio_play_sfx(&clip_rescue);
                    hud_message(g, "Hostage secured", HZ);
                }
                o->following = true;
            }
            else
            {
                o->active = false;
                g->objective_progress++;
                audio_play_sfx(&clip_pickup);
                hud_message(g, "Objective collected", HZ);
            }
        }
    }

    if (g->objective_kind == OBJECTIVE_RESCUE)
    {
        for (i = 0; i < g->objective_entity_count; ++i)
        {
            struct objective_entity *o = &g->objectives[i];
            if (!o->active || !o->following)
                continue;

            if (!intersects(g->player.x, g->player.y, g->player.size, o->x, o->y, o->size))
            {
                const int dx_to_player = step_dir(g->player.x - o->x);
                const int dy_to_player = step_dir(g->player.y - o->y);
                struct actor escort;
                rb->memset(&escort, 0, sizeof(escort));
                escort.x = o->x;
                escort.y = o->y;
                escort.size = o->size;
                try_move_actor(
                    g, &escort, o->x + dx_to_player * 2, o->y + dy_to_player * 2, true);
                o->x = escort.x;
                o->y = escort.y;
            }

            if (g->room_count > 0)
            {
                const int rx = g->room_x[0] * CDOGS_TILE_W;
                const int ry = g->room_y[0] * CDOGS_TILE_H;
                const int rw = g->room_w[0] * CDOGS_TILE_W;
                const int rh = g->room_h[0] * CDOGS_TILE_H;
                if (intersects_rect(o->x, o->y, o->size, o->size, rx, ry, rw, rh))
                {
                    o->active = false;
                    o->following = false;
                    g->objective_progress++;
                    audio_play_sfx(&clip_rescue);
                    hud_message(g, "Hostage extracted", HZ);
                }
            }
        }
    }

    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        struct actor *enemy = &g->enemies[i];
        if (!enemy->active)
            continue;
        if (!intersects(
                g->player.x, g->player.y, g->player.size,
                enemy->x, enemy->y, enemy->size))
            continue;
        if (g->damage_flash == 0)
            damage_player(g, 1);
        if (weapon_spec_for_type((enum weapon_type)g->player_primary)->type == WEAPON_KNIFE)
            enemy->hp -= 2;
        else if (weapon_spec_for_type((enum weapon_type)g->player_secondary)->type == WEAPON_KNIFE)
            enemy->hp -= 2;
        if (enemy->hp <= 0)
        {
            enemy->active = false;
            on_enemy_killed(g, enemy);
        }
    }
}

static void update_bullets(struct game_state *g)
{
    int i;
    int j;

    for (i = 0; i < (int)ARRAYLEN(g->bullets); ++i)
    {
        struct bullet *b = &g->bullets[i];
        if (!b->active)
            continue;

        b->ttl--;
        if (!projectile_advance(g, b) ||
            b->x < 0 || b->y < 0 || b->x >= world_w(g) || b->y >= world_h(g))
        {
            b->active = false;
            continue;
        }
        if (b->ttl <= 0)
        {
            if (b->kind == PROJ_GRENADE || b->kind == PROJ_SHRAPNEL_BOMB || b->kind == PROJ_CHEMO_BOMB)
                explode_projectile(g, b, true);
            else
                b->active = false;
            continue;
        }

        for (j = 0; j < (int)ARRAYLEN(g->enemies); ++j)
        {
            struct actor *enemy = &g->enemies[j];
            if (!enemy->active)
                continue;
            if (!intersects(b->x, b->y, 2, enemy->x, enemy->y, enemy->size))
                continue;

            b->active = false;
            enemy->hp -= max_i(1, b->damage);
            if (enemy->hp <= 0)
            {
                enemy->active = false;
                on_enemy_killed(g, enemy);
            }
            else if (b->kind == PROJ_FLAME)
            {
                enemy->dir_y = 1;
                apply_actor_burn(enemy, HZ * 2);
            }
            else if (b->source_weapon == WEAPON_CHEMO_BOMBS)
                apply_actor_poison(enemy, HZ * 4);
            else if (b->source_weapon == WEAPON_CONFUSION_BOMBS)
                apply_actor_confusion(enemy, HZ * 3);
            if (b->kind == PROJ_GRENADE || b->kind == PROJ_SHRAPNEL_BOMB || b->kind == PROJ_CHEMO_BOMB)
                explode_projectile(g, b, true);
            break;
        }
        if (!b->active)
            continue;
        if (g->objective_kind == OBJECTIVE_DESTROY)
        {
            for (j = 0; j < g->objective_entity_count; ++j)
            {
                struct objective_entity *o = &g->objectives[j];
                if (!o->active)
                    continue;
                if (!intersects(b->x, b->y, 3, o->x, o->y, o->size))
                    continue;
                b->active = false;
                o->active = false;
                g->objective_progress++;
                hud_message(g, "Objective destroyed", HZ);
                break;
            }
        }
    }
}

static void update_enemy_bullets(struct game_state *g)
{
    int i;

    for (i = 0; i < (int)ARRAYLEN(g->enemy_bullets); ++i)
    {
        struct bullet *b = &g->enemy_bullets[i];
        if (!b->active)
            continue;

        b->ttl--;
        if (!projectile_advance(g, b) ||
            b->x < 0 || b->y < 0 || b->x >= world_w(g) || b->y >= world_h(g))
        {
            b->active = false;
            continue;
        }
        if (b->ttl <= 0)
        {
            if (b->kind == PROJ_GRENADE || b->kind == PROJ_SHRAPNEL_BOMB || b->kind == PROJ_CHEMO_BOMB)
                explode_projectile(g, b, false);
            else
                b->active = false;
            continue;
        }

        if (intersects(b->x, b->y, 3, g->player.x, g->player.y, g->player.size))
        {
            if (b->kind == PROJ_GRENADE || b->kind == PROJ_SHRAPNEL_BOMB || b->kind == PROJ_CHEMO_BOMB)
                explode_projectile(g, b, false);
            else
            {
                b->active = false;
                damage_player(g, max_i(1, b->damage));
                if (b->kind == PROJ_FLAME)
                    g->player_burn_timer = max_i(g->player_burn_timer, HZ * 2);
            }
        }

        if (!b->active)
            continue;

        for (i = 0; i < g->objective_entity_count; ++i)
        {
            struct objective_entity *o = &g->objectives[i];
            if (!o->active)
                continue;
            if (!intersects(b->x, b->y, 3, o->x, o->y, o->size))
                continue;

            b->active = false;
            if (o->kind == OBJECTIVE_RESCUE)
            {
                o->hp -= max_i(1, b->damage);
                if (o->hp <= 0)
                {
                    o->active = false;
                    g->mission_failed = true;
                    g->screen = CDOGS_SCREEN_GAME_OVER;
                    hud_message(g, "Hostage lost", HZ);
                }
            }
            break;
        }
    }
}

static void update_enemies(struct game_state *g)
{
    int i;
    int alive = 0;
    const int player_cx_global = g->player.x + g->player.size / 2;
    const int player_cy_global = g->player.y + g->player.size / 2;

    if (g->screen != CDOGS_SCREEN_MISSION)
        return;

    g->enemy_step++;
    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        struct actor *enemy = &g->enemies[i];
        const struct weapon_spec *spec;
        int dx;
        int dy;
        int step_x = enemy->x;
        int step_y = enemy->y;
        int player_cx;
        int player_cy;
        int enemy_cx;
        int enemy_cy;
        int dist2;
        bool can_see;
        int desired_range;

        if (!enemy->active)
            continue;

        alive++;
        if (enemy->fire_cooldown > 0)
            enemy->fire_cooldown--;
        if (enemy->ai_delay > 0)
            enemy->ai_delay--;
        spec = weapon_spec_for_type((enum weapon_type)enemy->weapon_type);
        player_cx = player_cx_global;
        player_cy = player_cy_global;
        enemy_cx = enemy->x + enemy->size / 2;
        enemy_cy = enemy->y + enemy->size / 2;
        dx = player_cx - enemy_cx;
        dy = player_cy - enemy_cy;
        if (enemy->confuse_timer > 0)
        {
            dx = -dx + cdogs_rand_range(&g->rng, -CDOGS_TILE_W, CDOGS_TILE_W);
            dy = -dy + cdogs_rand_range(&g->rng, -CDOGS_TILE_H, CDOGS_TILE_H);
        }
        dist2 = dx * dx + dy * dy;
        can_see = dist2 < 32 * 32 ||
            has_line_of_sight(g, enemy_cx, enemy_cy, player_cx, player_cy);
        desired_range = weapon_range_pixels(spec);

        if (enemy->sleeping)
        {
            if (should_wake_enemy(g, enemy, player_cx, player_cy))
            {
                enemy->sleeping = false;
                enemy->waking = true;
                enemy->ai_delay = enemy->action_delay;
            }
            continue;
        }
        if (enemy->waking)
        {
            if (enemy->ai_delay <= 0)
                enemy->waking = false;
            else
                continue;
        }
        if (!can_see && dist2 > (40 * CDOGS_TILE_W) * (40 * CDOGS_TILE_W))
        {
            enemy->sleeping = true;
            enemy->waking = false;
            enemy->ai_delay = enemy->action_delay;
            continue;
        }

        if (enemy->ai_delay <= 0)
        {
            const int roll = cdogs_rand_percent(&g->rng);
            const int move_speed = max_i(1, enemy->move_speed);
            enemy->dir_x = dx < 0 ? -1 : (dx > 0 ? 1 : 0);
            enemy->dir_y = dy < 0 ? -1 : (dy > 0 ? 1 : 0);
            if (can_see && roll < enemy->track_chance)
            {
                if (spec->type == WEAPON_SNIPER_RIFLE && dist2 < (14 * CDOGS_TILE_W) * (14 * CDOGS_TILE_W))
                {
                    if (dx < 0)
                        step_x += move_speed;
                    else if (dx > 0)
                        step_x -= move_speed;
                    if (dy < 0)
                        step_y += move_speed;
                    else if (dy > 0)
                        step_y -= move_speed;
                }
                else if (spec->type == WEAPON_SHOTGUN && dist2 > (10 * CDOGS_TILE_W) * (10 * CDOGS_TILE_W))
                {
                    if (dx < 0)
                        step_x -= move_speed;
                    else if (dx > 0)
                        step_x += move_speed;
                    if (dy < 0)
                        step_y -= move_speed;
                    else if (dy > 0)
                        step_y += move_speed;
                }
                else if (spec->type == WEAPON_KNIFE)
                {
                    if (dx < 0)
                        step_x -= move_speed;
                    else if (dx > 0)
                        step_x += move_speed;
                    if (dy < 0)
                        step_y -= move_speed;
                    else if (dy > 0)
                        step_y += move_speed;
                }
                else if (dist2 > (6 * CDOGS_TILE_W) * (6 * CDOGS_TILE_W) ||
                    spec->type == WEAPON_GRENADES ||
                    spec->type == WEAPON_SHRAPNEL_BOMBS ||
                    spec->type == WEAPON_CHEMO_BOMBS ||
                    spec->type == WEAPON_CONFUSION_BOMBS ||
                    spec->type == WEAPON_FLAMER)
                {
                    if (dx < 0)
                        step_x -= move_speed;
                    else if (dx > 0)
                        step_x += move_speed;
                    if (dy < 0)
                        step_y -= move_speed;
                    else if (dy > 0)
                        step_y += move_speed;
                }
                try_move_actor(g, enemy, step_x, step_y, true);
            }
            else if (roll < enemy->move_chance)
            {
                static const int dir_table[8][2] = {
                    { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 },
                    { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }
                };
                const int rdir = cdogs_rand_range(&g->rng, 0, 7);
                enemy->dir_x = dir_table[rdir][0];
                enemy->dir_y = dir_table[rdir][1];
                step_x += enemy->dir_x * move_speed;
                step_y += enemy->dir_y * move_speed;
                try_move_actor(g, enemy, step_x, step_y, true);
            }
            enemy->ai_delay = enemy->action_delay;
        }

        enemy->x = clampi(enemy->x, 1, world_w(g) - enemy->size - 1);
        enemy->y = clampi(enemy->y, 1, world_h(g) - enemy->size - 1);

        if (intersects(
                g->player.x, g->player.y, g->player.size, enemy->x, enemy->y,
                enemy->size))
        {
            damage_player(g, 1);
            enemy->x = world_w(g) - enemy->size - CDOGS_TILE_W * 2;
            enemy->y = CDOGS_TILE_H * 2;
            if (g->screen == CDOGS_SCREEN_GAME_OVER)
                return;
        }
        if (enemy->fire_cooldown <= 0 &&
            can_see &&
            dist2 <= desired_range * desired_range &&
            cdogs_rand_percent(&g->rng) < enemy->shoot_chance)
        {
            fire_enemy_bullet(g, enemy);
            enemy->fire_cooldown =
                weapon_spec_for_type((enum weapon_type)enemy->weapon_type)->cooldown;
        }
    }

    if (alive == 0 && g->objective_progress < g->objective_required && g->objective_spawned < g->enemy_spawn_limit)
    {
        int spawn_count = min_i(g->available_enemies, g->enemy_spawn_limit - g->objective_spawned);
        for (i = 0; i < spawn_count; ++i)
            respawn_enemy(g, i);
        for (; i < (int)ARRAYLEN(g->enemies); ++i)
            g->enemies[i].active = false;
    }

    if (g->objective_progress >= g->objective_required)
    {
        g->screen = CDOGS_SCREEN_COMPLETE;
        g->complete_timer = HZ / 2;
        audio_play_sfx(&clip_mission_complete);
        audio_set_bgm(&clip_menu_music, true);
    }
}

#ifndef HAVE_LCD_COLOR
static void draw_label_value(
    const int x, const int y, const char *label, const char *value)
{
    set_fg(COLOR_TEXT_DIM);
    rb->lcd_putsxy(x, y, label);
    set_fg(COLOR_TEXT);
    rb->lcd_putsxy(x + 52, y, value);
}
#endif

static const char *objective_type_label(const int objective_kind)
{
    switch (objective_kind)
    {
    case OBJECTIVE_COLLECT:
        return "Collect";
    case OBJECTIVE_DESTROY:
        return "Destroy";
    case OBJECTIVE_RESCUE:
        return "Rescue";
    case OBJECTIVE_EXPLORE:
        return "Explore";
    default:
        return "Kill";
    }
}

static const char *player_status_label(const struct game_state *g)
{
    if (g->player_burn_timer > 0)
        return "Burning";
    if (g->player_poison_timer > 0)
        return "Poisoned";
    if (g->player_confuse_timer > 0)
        return "Confused";
    return "";
}

static void draw_text_block(
    int x, int y, int w, const char *text, const int line_h, const int scroll,
    const unsigned color);

static void draw_hud_message(const struct game_state *g)
{
    int text_w;
    if (g->hud_message_ticks <= 0 || !g->hud_message[0])
        return;
    rb->lcd_getstringsize(g->hud_message, &text_w, NULL);
    set_fg(COLOR_ACCENT);
    rb->lcd_putsxy((LCD_WIDTH - text_w) / 2, g->field_y + 6, g->hud_message);
}

static void draw_hud(const struct game_state *g)
{
    const struct cdogs_mission_data *mission = &cdogs_campaign_missions[g->mission_index];
    const char *status = player_status_label(g);
    char line[96];
    char value[40];
    const int mission_seconds = g->mission_time / HZ;

#ifdef HAVE_LCD_COLOR
    int x;
    const int hud_h = BMPHEIGHT_back_bar;
    set_fg(COLOR_BG);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, hud_h);
    for (x = 22; x < LCD_WIDTH; x += BMPWIDTH_back_bar)
    {
        const int w = min_i(BMPWIDTH_back_bar, LCD_WIDTH - x);
        rb->lcd_bitmap_transparent_part(
            back_bar, 0, 0, BMPWIDTH_back_bar, x, 0, w, BMPHEIGHT_back_bar);
    }
    rb->lcd_bitmap_transparent_part(
        player_frame_underlay, 0, 0, BMPWIDTH_player_frame_underlay, 0, 0,
        BMPWIDTH_player_frame_underlay, BMPHEIGHT_player_frame_underlay);
    rb->lcd_bitmap_transparent_part(
        player_frame, 0, 0, BMPWIDTH_player_frame, 2, 2,
        BMPWIDTH_player_frame, BMPHEIGHT_player_frame);

    rb->lcd_bitmap_transparent_part(
        gauge_back, 0, 0, BMPWIDTH_gauge_back, 31, 2,
        BMPWIDTH_gauge_back, BMPHEIGHT_gauge_back);

    if (g->player.hp > 0)
    {
        int hp_width = (g->player.hp * 6) / max_i(1, g->player.max_hp);
        hp_width = clampi(hp_width, 1, 6);
        rb->lcd_bitmap_transparent_part(
            gauge_inner, 0, 0, BMPWIDTH_gauge_inner, 31, 2,
            hp_width, BMPHEIGHT_gauge_inner);
    }

    set_fg(COLOR_TEXT);
    rb->snprintf(value, sizeof(value), "%d", g->player.hp);
    rb->lcd_putsxy(44, 2, value);

    set_fg(COLOR_TEXT_DIM);
    rb->lcd_putsxy(58, 2, "SCORE");
    set_fg(COLOR_TEXT);
    rb->snprintf(value, sizeof(value), "%d", g->score);
    rb->lcd_putsxy(92, 2, value);

    set_fg(COLOR_OBJECTIVE);
    rb->lcd_bitmap_transparent_part(
        objective_kill, 0, 0, BMPWIDTH_objective_kill, 120, 0,
        BMPWIDTH_objective_kill, BMPHEIGHT_objective_kill);
    set_fg(COLOR_TEXT);
    rb->snprintf(value, sizeof(value), "%d/%d", g->objective_progress, g->objective_required);
    rb->lcd_putsxy(138, 2, value);

    set_fg(COLOR_TEXT_DIM);
    rb->snprintf(line, sizeof(line), "%.12s", mission->title);
    rb->lcd_putsxy(120, 14, line);
    set_fg(COLOR_TEXT);
    rb->snprintf(value, sizeof(value), "%d:%02d", mission_seconds / 60, mission_seconds % 60);
    rb->lcd_putsxy(264, 2, value);

    rb->lcd_bitmap_transparent_part(
        button_bg, 0, 0, BMPWIDTH_button_bg, 176, 0,
        BMPWIDTH_button_bg, BMPHEIGHT_button_bg);
    rb->lcd_bitmap_transparent_part(
        gun_bg_30x23, 0, 0, BMPWIDTH_gun_bg_30x23, 177, 0,
        BMPWIDTH_gun_bg_30x23, BMPHEIGHT_gun_bg_30x23 / 2);
    rb->lcd_bitmap_transparent_part(
        button_bg, 0, 0, BMPWIDTH_button_bg, 208, 0,
        BMPWIDTH_button_bg, BMPHEIGHT_button_bg);
    rb->lcd_bitmap_transparent_part(
        gun_bg_30x23, 0, BMPHEIGHT_gun_bg_30x23 / 2, BMPWIDTH_gun_bg_30x23, 209, 0,
        BMPWIDTH_gun_bg_30x23, BMPHEIGHT_gun_bg_30x23 / 2);
    rb->lcd_bitmap_transparent_part(
        arrow, 0, 0, BMPWIDTH_arrow, 239, 2, BMPWIDTH_arrow, BMPHEIGHT_arrow);
    set_fg(COLOR_TEXT_DIM);
    rb->lcd_putsxy(184, 1, "A");
    rb->lcd_putsxy(216, 1, "B");
    rb->snprintf(line, sizeof(line), "%.8s", mission->weapon_b);
    rb->lcd_putsxy(179, 12, line);
    rb->snprintf(line, sizeof(line), "%.8s", mission->weapon_c);
    rb->lcd_putsxy(211, 12, line);

    set_fg(COLOR_TEXT_DIM);
    rb->snprintf(
        line, sizeof(line), "%s %d/%d",
        objective_type_label(g->objective_kind),
        g->objective_progress, g->objective_required);
    rb->lcd_putsxy(8, 14, line);
    if (status[0])
    {
        set_fg(COLOR_DANGER);
        rb->lcd_putsxy(268, 14, status);
    }
#else
    set_fg(COLOR_TEXT);
    rb->snprintf(
        line, sizeof(line), "%d/%d %s", g->mission_index + 1,
        cdogs_campaign_mission_count, mission->title);
    rb->lcd_putsxy(2, 0, line);

    rb->snprintf(value, sizeof(value), "%d", g->player.hp);
    draw_label_value(2, g->font_h, "HP", value);
    rb->snprintf(value, sizeof(value), "%d", g->score);
    draw_label_value(72, g->font_h, "Score", value);
    rb->snprintf(
        value, sizeof(value), "%d/%d", g->objective_progress,
        g->objective_required);
    draw_label_value(164, g->font_h, "Objective", value);

    set_fg(COLOR_TEXT_DIM);
    rb->snprintf(
        line, sizeof(line), "%s | %s | %s", mission->weapon_a, mission->weapon_b,
        mission->weapon_c);
    rb->lcd_putsxy(2, g->font_h * 2, line);
    rb->snprintf(value, sizeof(value), "%d:%02d", mission_seconds / 60, mission_seconds % 60);
    rb->lcd_putsxy(LCD_WIDTH - 40, 0, value);
    if (status[0])
    {
        set_fg(COLOR_DANGER);
        rb->lcd_putsxy(LCD_WIDTH - 70, g->font_h, status);
    }
#endif
}

static void draw_playfield(const struct game_state *g)
{
    int i;
    const int frame = facing_frame(g->facing_x, g->facing_y);
    const int pulse_frame = g->enemy_step % CDOGS_PULSE_FRAMES;
    const int pulse = g->grenade_flash * 4;
    const int player_anim_frame = (g->player_anim_frame / 4) % CDOGS_BODY_ANIM_FRAMES;
    const int inner_x = g->field_x + 1;
    const int inner_y = g->field_y + 1;
    const int inner_w = g->field_w - 2;
    const int inner_h = g->field_h - 2;
    const int cam_x = clampi(
        g->player.x + g->player.size / 2 - inner_w / 2,
        0, max_i(0, world_w(g) - inner_w));
    const int cam_y = clampi(
        g->player.y + g->player.size / 2 - inner_h / 2,
        0, max_i(0, world_h(g) - inner_h));

    draw_environment(g, cam_x, cam_y);
    set_fg(COLOR_BORDER);
    rb->lcd_drawrect(g->field_x, g->field_y, g->field_w, g->field_h);

    for (i = 0; i < (int)ARRAYLEN(g->bullets); ++i)
        draw_pulse_at(
            &g->bullets[i], pulse_frame, COLOR_BULLET,
            inner_x + g->bullets[i].x - cam_x - 1,
            inner_y + g->bullets[i].y - cam_y - 1);
    for (i = 0; i < (int)ARRAYLEN(g->enemy_bullets); ++i)
        draw_pulse_at(
            &g->enemy_bullets[i], pulse_frame, COLOR_EBULLET,
            inner_x + g->enemy_bullets[i].x - cam_x - 1,
            inner_y + g->enemy_bullets[i].y - cam_y - 1);
    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        const int enemy_dir = facing_frame(g->enemies[i].dir_x, g->enemies[i].dir_y);
        const int enemy_anim = ((g->enemy_step + i) / 2) % CDOGS_BODY_ANIM_FRAMES;
        const int enemy_draw_x =
            inner_x + g->enemies[i].x - cam_x - (CDOGS_BODY_FRAME_SIZE - g->enemies[i].size) / 2;
        const int enemy_draw_y =
            inner_y + g->enemies[i].y - cam_y - (CDOGS_BODY_FRAME_SIZE - g->enemies[i].size) / 2;
        draw_shadow_ellipse(enemy_draw_x + 6, enemy_draw_y + 18, 12, 3);
        draw_actor_sprite_at(
            &g->enemies[i], cdogs_enemy_legs,
            enemy_sheet_for_variant(g->enemies[i].sprite_variant), enemy_dir, enemy_anim,
            enemy_draw_x, enemy_draw_y,
            true);
        if (g->enemies[i].objective_target)
        {
            rb->lcd_bitmap_transparent_part(
                cdogs_objective_kill, 0, 0, BMPWIDTH_cdogs_objective_kill,
                inner_x + g->enemies[i].x - cam_x,
                inner_y + g->enemies[i].y - cam_y - BMPHEIGHT_cdogs_objective_kill,
                BMPWIDTH_cdogs_objective_kill, BMPHEIGHT_cdogs_objective_kill);
        }
    }

    for (i = 0; i < g->objective_entity_count; ++i)
    {
        const struct objective_entity *o = &g->objectives[i];
        if (!o->active)
            continue;
        if (o->kind == OBJECTIVE_DESTROY)
        {
            rb->lcd_bitmap_transparent_part(
                cdogs_safe, 0, 0, BMPWIDTH_cdogs_safe,
                inner_x + o->x - cam_x, inner_y + o->y - cam_y,
                BMPWIDTH_cdogs_safe, BMPHEIGHT_cdogs_safe);
        }
        else if (o->kind == OBJECTIVE_RESCUE)
        {
            const int dir = o->following ? frame : 4;
            const int obj_draw_x =
                inner_x + o->x - cam_x - (CDOGS_BODY_FRAME_SIZE - o->size) / 2;
            const int obj_draw_y =
                inner_y + o->y - cam_y - (CDOGS_BODY_FRAME_SIZE - o->size) / 2;
            draw_shadow_ellipse(obj_draw_x + 6, obj_draw_y + 18, 12, 3);
            draw_actor_sprite_at(
                &(struct actor){ .active = true, .size = o->size }, cdogs_player_legs,
                cdogs_player_upper, dir, 0,
                obj_draw_x, obj_draw_y,
                true);
        }
        else
        {
            int collect_w = BMPWIDTH_cdogs_objective;
            int collect_h = BMPHEIGHT_cdogs_objective;
            const fb_data *collect_sprite = cdogs_objective;
            if (o->kind == OBJECTIVE_COLLECT)
                collect_sprite = collect_sprite_for_mission(g, &collect_w, &collect_h);
            rb->lcd_bitmap_transparent_part(
                collect_sprite, 0, 0, collect_w,
                inner_x + o->x - cam_x, inner_y + o->y - cam_y,
                collect_w, collect_h);
        }
    }

    if (g->grenade_flash > 0)
    {
        set_fg(COLOR_FLASH);
        rb->lcd_drawrect(
            inner_x + g->player.x - cam_x - pulse,
            inner_y + g->player.y - cam_y - pulse,
            g->player.size + pulse * 2, g->player.size + pulse * 2);
    }

    {
        const int player_draw_x =
            inner_x + g->player.x - cam_x - (CDOGS_BODY_FRAME_SIZE - g->player.size) / 2;
        const int player_draw_y =
            inner_y + g->player.y - cam_y - (CDOGS_BODY_FRAME_SIZE - g->player.size) / 2;
        draw_shadow_ellipse(player_draw_x + 6, player_draw_y + 18, 12, 3);
        draw_actor_sprite_at(
            &g->player, cdogs_player_legs,
            cdogs_player_upper, frame, player_anim_frame,
            player_draw_x, player_draw_y,
            true);
    }

    if (g->damage_flash > 0)
    {
        set_fg(COLOR_DANGER);
        rb->lcd_drawrect(
            inner_x + g->player.x - cam_x - 2,
            inner_y + g->player.y - cam_y - 2, g->player.size + 4,
            g->player.size + 4);
    }
}

static void draw_map_overlay(const struct game_state *g)
{
    int i;
    const int map_w = 72;
    const int map_h = 54;
    const int map_x = LCD_WIDTH - map_w - 6;
    const int map_y = g->field_y + 6;
    const int px =
        map_x + (g->player.x * (map_w - 4)) / max_i(1, world_w(g)) + 2;
    const int py =
        map_y + (g->player.y * (map_h - 4)) / max_i(1, world_h(g)) + 2;

    set_fg(COLOR_PANEL);
    rb->lcd_fillrect(map_x, map_y, map_w, map_h);
    set_fg(COLOR_MAP);
    rb->lcd_drawrect(map_x, map_y, map_w, map_h);
    rb->lcd_fillrect(px, py, 3, 3);

    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        int ex;
        int ey;
        if (!g->enemies[i].active)
            continue;
        ex = map_x + (g->enemies[i].x * (map_w - 4)) / max_i(1, world_w(g)) + 2;
        ey = map_y + (g->enemies[i].y * (map_h - 4)) / max_i(1, world_h(g)) + 2;
        set_fg(COLOR_ENEMY);
        rb->lcd_fillrect(ex, ey, 2, 2);
    }
}

static void draw_center_panel(
    const int y, const int h, const char *title, const char *line1,
    const char *line2, const char *line3, const bool success)
{
    const int box_w = LCD_WIDTH - 44;
    const int box_x = (LCD_WIDTH - box_w) / 2;
    const int text_x = box_x + 12;

    set_fg(COLOR_PANEL_ALT);
    rb->lcd_fillrect(box_x, y, box_w, h);
    set_fg(success ? COLOR_SUCCESS : COLOR_BORDER);
    rb->lcd_drawrect(box_x, y, box_w, h);
    set_fg(COLOR_TEXT);
    rb->lcd_putsxy(text_x, y + 8, title);
    set_fg(COLOR_TEXT_DIM);
    rb->lcd_putsxy(text_x, y + 8 + 16, line1);
    rb->lcd_putsxy(text_x, y + 8 + 32, line2);
    rb->lcd_putsxy(text_x, y + 8 + 48, line3);
}

static void draw_title_screen(const struct game_state *g)
{
    const struct cdogs_mission_data *mission =
        &cdogs_campaign_missions[g->selected_mission];
    char line[96];
    const int content_top = BMPHEIGHT_cdogs_logo + 18;
    const int desc_y = content_top + g->font_h + 2;
    const int panel_y = desc_y + g->font_h * 2 + 8;
    const int panel_h = LCD_HEIGHT - panel_y - (g->font_h + 12);
    const int left_w = 112;
    const int right_x = 140;
    const int right_w = LCD_WIDTH - right_x - 16;
    const int campaign_y = panel_y + 8;

    set_fg(COLOR_BG);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    set_fg(COLOR_BG_ALT);
    rb->lcd_fillrect(0, content_top - 6, LCD_WIDTH, LCD_HEIGHT - (content_top - 6));
    rb->lcd_bitmap_transparent_part(
        cdogs_logo, 0, 0, BMPWIDTH_cdogs_logo, (LCD_WIDTH - BMPWIDTH_cdogs_logo) / 2,
        10, BMPWIDTH_cdogs_logo, BMPHEIGHT_cdogs_logo);

    set_fg(COLOR_TEXT);
    rb->lcd_putsxy(18, content_top, cdogs_campaign_title);
    set_fg(COLOR_TEXT_DIM);
    draw_text_block(18, desc_y, LCD_WIDTH - 36, cdogs_campaign_description, g->font_h + 1, 0, COLOR_TEXT_DIM);

    set_fg(COLOR_BORDER);
    rb->lcd_drawrect(16, panel_y, left_w, panel_h);
    set_fg(COLOR_TEXT_DIM);
    rb->lcd_putsxy(24, panel_y + 10, "Mode");
    set_fg(COLOR_TEXT);
    rb->lcd_putsxy(24, panel_y + 10 + g->font_h + 4, "Campaign");
    set_fg(COLOR_TEXT_DIM);
    rb->lcd_putsxy(24, panel_y + 10 + (g->font_h + 4) * 2, "Select: Briefing");
    rb->lcd_putsxy(24, panel_y + 10 + (g->font_h + 4) * 3, "Back: Exit");

    set_fg(COLOR_BORDER);
    rb->lcd_drawrect(right_x, panel_y, right_w, panel_h);
    set_fg(COLOR_TEXT_DIM);
    rb->lcd_putsxy(right_x + 8, campaign_y, "Campaign");
    set_fg(COLOR_TEXT);
    rb->snprintf(
        line, sizeof(line), "%d/%d %s", g->selected_mission + 1,
        cdogs_campaign_mission_count, mission->title);
    draw_text_block(right_x + 8, campaign_y + g->font_h + 4, right_w - 16, line, g->font_h + 1, 0, COLOR_TEXT);
    set_fg(COLOR_TEXT_DIM);
    rb->snprintf(line, sizeof(line), "%s", mission->objective);
    draw_text_block(right_x + 8, campaign_y + (g->font_h + 1) * 2 + 8, right_w - 16, line, g->font_h + 1, 0, COLOR_TEXT_DIM);
    rb->lcd_putsxy(18, LCD_HEIGHT - g->font_h - 6, "Any direction mission  Select briefing");
}

static void draw_text_block(
    int x, int y, int w, const char *text, const int line_h, const int scroll,
    const unsigned color)
{
    char buf[320];
    int start = 0;
    int line = 0;
    int len = (int)rb->strlen(text);

    while (start < len)
    {
        int i = start;
        int out = 0;
        int last_space = -1;
        int pixel_w = 0;

        while (i < len && out < (int)sizeof(buf) - 1)
        {
            int test_w;
            buf[out++] = text[i];
            buf[out] = '\0';
            rb->lcd_getstringsize(buf, &test_w, NULL);
            if (buf[out - 1] == ' ')
                last_space = out - 1;
            if (test_w > w)
            {
                if (last_space >= 0)
                    out = last_space;
                break;
            }
            pixel_w = test_w;
            if (text[i] == '\n')
                break;
            i++;
        }

        if (out <= 0)
            break;
        buf[out] = '\0';
        if (line >= scroll)
        {
            set_fg(color);
            rb->lcd_putsxy(x, y + (line - scroll) * line_h, buf);
        }

        start += out;
        while (text[start] == ' ')
            start++;
        line++;
        (void)pixel_w;
    }
}

static void draw_briefing_screen(const struct game_state *g)
{
    const struct cdogs_mission_data *mission =
        &cdogs_campaign_missions[g->mission_index];
    char line[96];
    const int title_y = 24;
    const int subtitle_y = title_y + g->font_h + 4;
    const int body_y = subtitle_y + g->font_h + 10;
    const int footer_y = LCD_HEIGHT - 44;
    const int body_w = LCD_WIDTH - 48;

    set_fg(COLOR_BG);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    set_fg(COLOR_PANEL);
    rb->lcd_fillrect(12, 12, LCD_WIDTH - 24, LCD_HEIGHT - 24);
    set_fg(COLOR_BORDER);
    rb->lcd_drawrect(12, 12, LCD_WIDTH - 24, LCD_HEIGHT - 24);

    set_fg(COLOR_ACCENT);
    rb->lcd_putsxy(24, title_y, "Brotherhood briefing");
    set_fg(COLOR_TEXT);
    rb->snprintf(
        line, sizeof(line), "Mission %d/%d: %s", g->mission_index + 1,
        cdogs_campaign_mission_count, mission->title);
    draw_text_block(24, subtitle_y, body_w, line, g->font_h + 1, 0, COLOR_TEXT);

    draw_text_block(
        24, body_y, body_w, mission->description,
        g->font_h + 2, g->briefing_scroll, COLOR_TEXT);

    rb->lcd_bitmap_transparent_part(
        cdogs_objective_kill, 0, 0, BMPWIDTH_cdogs_objective_kill, 24, footer_y,
        BMPWIDTH_cdogs_objective_kill, BMPHEIGHT_cdogs_objective_kill);
    set_fg(COLOR_OBJECTIVE);
    rb->snprintf(
        line, sizeof(line), "%s (%d)", mission->objective,
        mission->objective_required);
    draw_text_block(42, footer_y - 1, LCD_WIDTH - 66, line, g->font_h + 1, 0, COLOR_OBJECTIVE);

    set_fg(COLOR_TEXT_DIM);
    rb->snprintf(
        line, sizeof(line), "Loadout: %s | %s | %s", mission->weapon_a,
        mission->weapon_b, mission->weapon_c);
    draw_text_block(24, footer_y + g->font_h + 2, LCD_WIDTH - 48, line, g->font_h + 1, 0, COLOR_TEXT_DIM);
    rb->lcd_putsxy(
        24, LCD_HEIGHT - g->font_h - 8, "Select deploy  Menu/Play scroll  Power back");
}

static void draw_complete_screen(const struct game_state *g)
{
    char line1[96];
    char line2[96];

    rb->snprintf(
        line1, sizeof(line1), "Objective complete %d/%d",
        g->mission_index + 1, cdogs_campaign_mission_count);
    if (g->mission_index + 1 < cdogs_campaign_mission_count)
        rb->snprintf(
            line2, sizeof(line2), "Next: %s",
            cdogs_campaign_missions[g->mission_index + 1].title);
    else
        rb->snprintf(line2, sizeof(line2), "Campaign cleared");

    set_fg(COLOR_BG);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    draw_center_panel(
        72, 96, "B.A.D. cell neutralized", line1, line2,
        "Select continue  Power title", true);
}

static void draw_game_over_screen(const struct game_state *g)
{
    char line[64];

    rb->snprintf(line, sizeof(line), "Score %d  Kills %d/%d", g->score,
        g->objective_progress, g->objective_required);
    set_fg(COLOR_BG);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    draw_center_panel(
        72, 96, "Operative down", line, "Select retry mission",
        "Power returns to title", false);
}

static void draw_screen(const struct game_state *g)
{
    rb->lcd_clear_display();

    switch (g->screen)
    {
    case CDOGS_SCREEN_TITLE:
        draw_title_screen(g);
        break;
    case CDOGS_SCREEN_BRIEFING:
        draw_briefing_screen(g);
        break;
    case CDOGS_SCREEN_COMPLETE:
        draw_complete_screen(g);
        break;
    case CDOGS_SCREEN_GAME_OVER:
        draw_game_over_screen(g);
        break;
    case CDOGS_SCREEN_MISSION:
    default:
        draw_hud(g);
        draw_playfield(g);
        draw_hud_message(g);
        if (g->show_map)
            draw_map_overlay(g);
        break;
    }

    rb->lcd_update();
}

static void start_briefing(struct game_state *g, const int mission_index)
{
    g->mission_index = mission_index;
    g->briefing_scroll = 0;
    g->screen = CDOGS_SCREEN_BRIEFING;
    audio_set_bgm(&clip_menu_music, true);
    audio_play_sfx(&clip_menu_start);
}

static void start_mission(struct game_state *g)
{
    configure_mission(g, g->mission_index);
    g->screen = CDOGS_SCREEN_MISSION;
    audio_set_bgm(&clip_game_music, true);
    audio_play_sfx(&clip_menu_start);
}

static void update_game(struct game_state *g, const int cmd)
{
    switch (g->screen)
    {
    case CDOGS_SCREEN_TITLE:
        if (cmd & (CDOGS_CMD_UP | CDOGS_CMD_LEFT))
        {
            g->selected_mission =
                (g->selected_mission + cdogs_campaign_mission_count - 1) %
                cdogs_campaign_mission_count;
            audio_play_sfx(&clip_menu_move);
        }
        else if (cmd & (CDOGS_CMD_DOWN | CDOGS_CMD_RIGHT))
        {
            g->selected_mission =
                (g->selected_mission + 1) % cdogs_campaign_mission_count;
            audio_play_sfx(&clip_menu_move);
        }
        if (cmd & CDOGS_CMD_ACCEPT)
            start_briefing(g, g->selected_mission);
        else if (cmd & CDOGS_CMD_BACK)
            g->mission_failed = true;
        break;

    case CDOGS_SCREEN_BRIEFING:
        if (cmd & CDOGS_CMD_UP)
            g->briefing_scroll = max_i(0, g->briefing_scroll - 1);
        else if (cmd & CDOGS_CMD_DOWN)
            g->briefing_scroll = min_i(10, g->briefing_scroll + 1);
        if (cmd & CDOGS_CMD_BACK)
        {
            g->screen = CDOGS_SCREEN_TITLE;
            audio_set_bgm(&clip_menu_music, true);
            audio_play_sfx(&clip_menu_back);
        }
        else if (cmd & CDOGS_CMD_ACCEPT)
            start_mission(g);
        break;

    case CDOGS_SCREEN_COMPLETE:
        if (g->complete_timer > 0)
            g->complete_timer--;
        if (!(cmd & CDOGS_CMD_ACCEPT) && !(cmd & CDOGS_CMD_BACK))
            break;
        if (cmd & CDOGS_CMD_BACK)
        {
            g->screen = CDOGS_SCREEN_TITLE;
            g->selected_mission = g->mission_index;
            audio_set_bgm(&clip_menu_music, true);
            audio_play_sfx(&clip_menu_back);
        }
        else if (g->mission_index + 1 < cdogs_campaign_mission_count)
        {
            g->selected_mission = g->mission_index + 1;
            start_briefing(g, g->mission_index + 1);
        }
        else
        {
            g->screen = CDOGS_SCREEN_TITLE;
            g->selected_mission = 0;
            audio_set_bgm(&clip_menu_music, true);
        }
        break;

    case CDOGS_SCREEN_GAME_OVER:
        if (cmd & CDOGS_CMD_BACK)
        {
            g->screen = CDOGS_SCREEN_TITLE;
            g->selected_mission = g->mission_index;
            audio_set_bgm(&clip_menu_music, true);
            audio_play_sfx(&clip_menu_back);
        }
        else if (cmd & CDOGS_CMD_ACCEPT)
        {
            start_briefing(g, g->mission_index);
        }
        break;

    case CDOGS_SCREEN_MISSION:
        g->mission_time++;
        if (g->fire_cooldown > 0)
            g->fire_cooldown--;
        if (g->grenade_cooldown > 0)
            g->grenade_cooldown--;
        if (g->enemy_fire_cooldown > 0)
            g->enemy_fire_cooldown--;
        if (g->damage_flash > 0)
            g->damage_flash--;
        if (g->grenade_flash > 0)
            g->grenade_flash--;
        if (g->player_shot_timer > 0)
            g->player_shot_timer--;
        if (g->hud_message_ticks > 0)
            g->hud_message_ticks--;

        update_status_effects(g);
        if (g->screen != CDOGS_SCREEN_MISSION)
            break;

        update_player(g, cmd);
        update_bullets(g);
        update_enemy_bullets(g);
        update_enemies(g);
        break;
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    struct game_state game;
    enum plugin_status status = PLUGIN_OK;

    (void)parameter;

    enter_display_mode();
    reset_game(&game);

    while (true)
    {
        const long event = rb->button_get_w_tmo(HZ / 30);
        const long buttons = rb->button_status();
        const int clean_event = clean_button(event);
        const int clean_status = clean_button(buttons);
        int cmd = map_buttons(buttons);

        if (event == SYS_USB_CONNECTED ||
            rb->default_event_handler(event) == SYS_USB_CONNECTED)
        {
            status = PLUGIN_USB_CONNECTED;
            break;
        }

#ifdef HAS_BUTTON_HOLD
        if (rb->button_hold())
        {
            status = PLUGIN_OK;
            break;
        }
#endif

        if (game.screen == CDOGS_SCREEN_MISSION &&
            (clean_status & BUTTON_MENU) && (clean_status & BUTTON_SELECT))
        {
            if (!game.combo_latched)
            {
                game.show_map = !game.show_map;
                game.combo_latched = true;
                audio_play_sfx(&clip_map_open);
            }
        }
        else
        {
            game.combo_latched = false;
        }

        if (game.combo_latched)
            cmd &= ~(CDOGS_CMD_MAP | CDOGS_CMD_FIRE | CDOGS_CMD_UP);
        if ((clean_status & BUTTON_PLAY) && (clean_status & BUTTON_SELECT))
            cmd &= ~CDOGS_CMD_DOWN;

        if (clean_event & BUTTON_SELECT)
            cmd |= CDOGS_CMD_ACCEPT;
        if (clean_event & BUTTON_LEFT)
            cmd |= CDOGS_CMD_BACK;

        update_game(&game, cmd);
        if (game.screen == CDOGS_SCREEN_TITLE && game.mission_failed)
        {
            status = PLUGIN_OK;
            break;
        }
        draw_screen(&game);
    }

    leave_display_mode();
    return status;
}
