/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * RunePod prototype: click-wheel-first fantasy RPG vertical slice.
 *
 ****************************************************************************/

#include "plugin.h"

#include <stdbool.h>
#include <stdint.h>

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320)

#define RP_FRAME_TICKS MAX(1, HZ / 30)
#define RP_TOP_H 18
#define RP_BOTTOM_H 34
#define RP_WORLD_TOP RP_TOP_H
#define RP_WORLD_BOTTOM (LCD_HEIGHT - RP_BOTTOM_H)
#define RP_WORLD_VIEW_H (RP_WORLD_BOTTOM - RP_WORLD_TOP)
#define RP_WORLD_W 640
#define RP_WORLD_H 420
#define RP_TILE_SIZE 32
#define RP_TARGET_RADIUS 14
#define RP_CURSOR_RADIUS 18
#define RP_CURSOR_STEP 7
#define RP_CURSOR_REPEAT_STEP 12
#define RP_PLAYER_SPEED 3
#define RP_MAX_TARGETS 8
#define RP_MAX_ACTIONS 4
#define RP_SMOKE_LOG PLUGIN_GAMES_DATA_DIR "/runepod-smoke.log"
#define RP_SPRITES_PATH PLUGIN_GAMES_DATA_DIR "/runepod/sprites/runepod_sprites.320x64x24.bmp"
#define RP_PLAYER_DIRS_PATH PLUGIN_GAMES_DATA_DIR "/runepod/sprites/runepod_player_dirs.128x32x24.bmp"
#define RP_TERRAIN_PATH PLUGIN_GAMES_DATA_DIR "/runepod/tiles/runepod_terrain_tiles.256x32x24.bmp"
#define RP_MUSIC_PATH PLUGIN_GAMES_DATA_DIR "/runepod/audio/Harmony.mp3"
#define RP_SPRITE_W 32
#define RP_SPRITE_H 32
#define RP_SPRITE_SHEET_W 320
#define RP_SPRITE_SHEET_H 64
#define RP_SPRITE_PIXELS (RP_SPRITE_SHEET_W * RP_SPRITE_SHEET_H)
#define RP_SPRITE_BYTES (RP_SPRITE_PIXELS * (int)sizeof(fb_data))
#define RP_PLAYER_DIR_SHEET_W 128
#define RP_PLAYER_DIR_SHEET_H 32
#define RP_PLAYER_DIR_PIXELS (RP_PLAYER_DIR_SHEET_W * RP_PLAYER_DIR_SHEET_H)
#define RP_PLAYER_DIR_BYTES (RP_PLAYER_DIR_PIXELS * (int)sizeof(fb_data))
#define RP_TERRAIN_SHEET_W 256
#define RP_TERRAIN_SHEET_H 32
#define RP_TERRAIN_PIXELS (RP_TERRAIN_SHEET_W * RP_TERRAIN_SHEET_H)
#define RP_TERRAIN_BYTES (RP_TERRAIN_PIXELS * (int)sizeof(fb_data))

#define RP_COL_SKY LCD_RGBPACK(108, 158, 164)
#define RP_COL_GRASS LCD_RGBPACK(72, 126, 79)
#define RP_COL_GRASS_DARK LCD_RGBPACK(48, 92, 58)
#define RP_COL_PATH LCD_RGBPACK(143, 118, 82)
#define RP_COL_WOOD LCD_RGBPACK(99, 66, 42)
#define RP_COL_ROOF LCD_RGBPACK(155, 72, 42)
#define RP_COL_TREE LCD_RGBPACK(45, 112, 52)
#define RP_COL_STONE LCD_RGBPACK(112, 116, 111)
#define RP_COL_WATER LCD_RGBPACK(47, 99, 146)
#define RP_COL_FIRE LCD_RGBPACK(220, 92, 43)
#define RP_COL_PANEL LCD_RGBPACK(24, 25, 24)
#define RP_COL_PANEL_2 LCD_RGBPACK(42, 44, 42)
#define RP_COL_TEXT LCD_RGBPACK(238, 236, 220)
#define RP_COL_MUTED LCD_RGBPACK(175, 174, 159)
#define RP_COL_ACCENT LCD_RGBPACK(230, 194, 90)
#define RP_COL_PLAYER LCD_RGBPACK(68, 90, 167)
#define RP_COL_ENEMY LCD_RGBPACK(132, 55, 64)
#define RP_COL_FOCUS LCD_RGBPACK(255, 232, 92)

enum rp_view
{
    RP_VIEW_TITLE = 0,
    RP_VIEW_WORLD,
    RP_VIEW_ACTIONS,
    RP_VIEW_INVENTORY,
    RP_VIEW_LEVELS,
    RP_VIEW_MAP,
    RP_VIEW_DIALOGUE
};

enum rp_target_kind
{
    RP_TARGET_NPC = 0,
    RP_TARGET_TREE,
    RP_TARGET_ROCK,
    RP_TARGET_FISH,
    RP_TARGET_FIRE,
    RP_TARGET_BENCH,
    RP_TARGET_ENEMY,
    RP_TARGET_EXIT
};

enum rp_group
{
    RP_GROUP_NPC = 0,
    RP_GROUP_RESOURCE,
    RP_GROUP_CRAFT,
    RP_GROUP_COMBAT,
    RP_GROUP_EXIT,
    RP_GROUP_COUNT
};

enum rp_sprite
{
    RP_SPR_PLAYER = 0,
    RP_SPR_GUIDE,
    RP_SPR_SHOPKEEPER,
    RP_SPR_RATLING,
    RP_SPR_OAK,
    RP_SPR_COPPER,
    RP_SPR_POND,
    RP_SPR_FIRE,
    RP_SPR_WORKBENCH,
    RP_SPR_COMBAT,
    RP_SPR_HP,
    RP_SPR_COINS,
    RP_SPR_LOGS,
    RP_SPR_ORE,
    RP_SPR_FISH,
    RP_SPR_FOOD,
    RP_SPR_COMBAT_ICON
};

enum rp_direction
{
    RP_DIR_SOUTH = 0,
    RP_DIR_EAST,
    RP_DIR_NORTH,
    RP_DIR_WEST
};

enum rp_tile
{
    RP_TILE_GRASS = 0,
    RP_TILE_DARK_GRASS,
    RP_TILE_PATH,
    RP_TILE_WOOD,
    RP_TILE_STONE,
    RP_TILE_WATER,
    RP_TILE_VILLAGE,
    RP_TILE_CAVE
};

enum rp_activity
{
    RP_ACTIVITY_NONE = 0,
    RP_ACTIVITY_CHOP,
    RP_ACTIVITY_MINE,
    RP_ACTIVITY_FISH,
    RP_ACTIVITY_COOK,
    RP_ACTIVITY_CRAFT,
    RP_ACTIVITY_FIGHT,
    RP_ACTIVITY_EAT
};

struct rp_target
{
    const char *name;
    const char *default_action;
    enum rp_target_kind kind;
    enum rp_group group;
    int x;
    int y;
};

struct rp_inventory
{
    int logs;
    int ore;
    int raw_fish;
    int food;
    int coins;
    int charms;
};

struct rp_skills
{
    int combat;
    int mining;
    int woodcutting;
    int fishing;
    int cooking;
    int crafting;
};

struct rp_game
{
    enum rp_view view;
    int player_x;
    int player_y;
    int dest_x;
    int dest_y;
    int cursor_x;
    int cursor_y;
    int camera_x;
    int camera_y;
    enum rp_direction player_dir;
    bool moving;
    bool pending_action;
    int selected;
    int action_selected;
    int action_count;
    const char *actions[RP_MAX_ACTIONS];
    struct rp_inventory inv;
    struct rp_skills xp;
    int hp;
    int enemy_hp;
    int quest_stage;
    long cooldown_until[RP_MAX_TARGETS];
    enum rp_activity activity;
    int activity_target;
    long activity_started;
    long activity_until;
    char message[96];
    char detail[96];
    bool music_started;
    bool quit;
};

static const struct rp_target rp_targets[RP_MAX_TARGETS] =
{
    { "Guide", "Talk", RP_TARGET_NPC, RP_GROUP_NPC, 286, 178 },
    { "Shop", "Trade", RP_TARGET_NPC, RP_GROUP_NPC, 430, 148 },
    { "Oak", "Chop", RP_TARGET_TREE, RP_GROUP_RESOURCE, 164, 306 },
    { "Copper", "Mine", RP_TARGET_ROCK, RP_GROUP_RESOURCE, 486, 332 },
    { "Pond", "Fish", RP_TARGET_FISH, RP_GROUP_RESOURCE, 526, 234 },
    { "Fire", "Cook", RP_TARGET_FIRE, RP_GROUP_CRAFT, 332, 246 },
    { "Workbench", "Craft", RP_TARGET_BENCH, RP_GROUP_CRAFT, 366, 172 },
    { "Ratling", "Attack", RP_TARGET_ENEMY, RP_GROUP_COMBAT, 548, 368 },
};

static struct rp_game game;
static struct bitmap rp_sprite_sheet;
static fb_data rp_sprite_pixels[RP_SPRITE_PIXELS];
static bool rp_sprites_loaded;
static struct bitmap rp_player_dir_sheet;
static fb_data rp_player_dir_pixels[RP_PLAYER_DIR_PIXELS];
static bool rp_player_dirs_loaded;
static struct bitmap rp_terrain_sheet;
static fb_data rp_terrain_pixels[RP_TERRAIN_PIXELS];
static bool rp_terrain_loaded;

#ifdef SIMULATOR
static void rp_smoke_log(const char *event, int value)
{
    int fd = rb->open(RP_SMOKE_LOG, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
    {
        rb->fdprintf(fd, "event=%s value=%d tick=%ld\n",
                     event, value, *rb->current_tick);
        rb->close(fd);
    }
}
#else
static void rp_smoke_log(const char *event, int value)
{
    (void)event;
    (void)value;
}
#endif

static void rp_set_wheel_events(bool enabled)
{
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(enabled);
#else
    (void)enabled;
#endif
}

static bool rp_load_sprites(void)
{
    int rc;

    rb->memset(&rp_sprite_sheet, 0, sizeof(rp_sprite_sheet));
    rp_sprite_sheet.data = (char *)rp_sprite_pixels;
    rc = rb->read_bmp_file(RP_SPRITES_PATH, &rp_sprite_sheet,
                           RP_SPRITE_BYTES, FORMAT_NATIVE, NULL);
    rp_sprites_loaded = rc > 0 &&
                        rp_sprite_sheet.width == RP_SPRITE_SHEET_W &&
                        rp_sprite_sheet.height == RP_SPRITE_SHEET_H;
#ifdef SIMULATOR
    if (rp_sprites_loaded)
        rp_smoke_log("sprites_loaded", rc);
    else
        rp_smoke_log("sprites_missing", rc);
#endif
    return rp_sprites_loaded;
}

static bool rp_load_player_dirs(void)
{
    int rc;

    rb->memset(&rp_player_dir_sheet, 0, sizeof(rp_player_dir_sheet));
    rp_player_dir_sheet.data = (char *)rp_player_dir_pixels;
    rc = rb->read_bmp_file(RP_PLAYER_DIRS_PATH, &rp_player_dir_sheet,
                           RP_PLAYER_DIR_BYTES, FORMAT_NATIVE, NULL);
    rp_player_dirs_loaded = rc > 0 &&
                            rp_player_dir_sheet.width == RP_PLAYER_DIR_SHEET_W &&
                            rp_player_dir_sheet.height == RP_PLAYER_DIR_SHEET_H;
#ifdef SIMULATOR
    rp_smoke_log(rp_player_dirs_loaded ? "player_dirs_loaded"
                                       : "player_dirs_missing", rc);
#endif
    return rp_player_dirs_loaded;
}

static bool rp_load_terrain(void)
{
    int rc;

    rb->memset(&rp_terrain_sheet, 0, sizeof(rp_terrain_sheet));
    rp_terrain_sheet.data = (char *)rp_terrain_pixels;
    rc = rb->read_bmp_file(RP_TERRAIN_PATH, &rp_terrain_sheet,
                           RP_TERRAIN_BYTES, FORMAT_NATIVE, NULL);
    rp_terrain_loaded = rc > 0 &&
                        rp_terrain_sheet.width == RP_TERRAIN_SHEET_W &&
                        rp_terrain_sheet.height == RP_TERRAIN_SHEET_H;
#ifdef SIMULATOR
    rp_smoke_log(rp_terrain_loaded ? "terrain_loaded"
                                   : "terrain_missing", rc);
#endif
    return rp_terrain_loaded;
}

static bool rp_start_music(void)
{
    int fd = rb->open(RP_MUSIC_PATH, O_RDONLY);

    if (fd < 0)
    {
        rp_smoke_log("music_missing", fd);
        return false;
    }

    rb->close(fd);
    rb->audio_stop();
    rb->playlist_remove_all_tracks(NULL);
    if (rb->playlist_create(NULL, NULL) < 0)
    {
        rp_smoke_log("music_playlist_create_failed", 0);
        return false;
    }

    if (rb->playlist_insert_track(NULL, RP_MUSIC_PATH,
                                  PLAYLIST_INSERT_LAST, false, true) < 0)
    {
        rp_smoke_log("music_insert_failed", 0);
        return false;
    }

    rb->plugin_release_audio_buffer();
    rb->playlist_set_modified(NULL, true);
    rb->playlist_start(0, 0, 0);
    rp_smoke_log("music_started", 1);
    return true;
}

static int rp_iabs(int v)
{
    return v < 0 ? -v : v;
}

static int rp_clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static void rp_update_camera(void)
{
    game.camera_x = rp_clamp_int(game.cursor_x - LCD_WIDTH / 2,
                                 0, RP_WORLD_W - LCD_WIDTH);
    game.camera_y = rp_clamp_int(game.cursor_y - RP_WORLD_VIEW_H / 2,
                                 0, RP_WORLD_H - RP_WORLD_VIEW_H);
}

static int rp_screen_x(int world_x)
{
    return world_x - game.camera_x;
}

static int rp_screen_y(int world_y)
{
    return world_y - game.camera_y + RP_WORLD_TOP;
}

static bool rp_world_visible(int world_x, int world_y, int margin)
{
    int x = rp_screen_x(world_x);
    int y = rp_screen_y(world_y);

    return x >= -margin && x < LCD_WIDTH + margin &&
           y >= RP_WORLD_TOP - margin && y < RP_WORLD_BOTTOM + margin;
}

static bool rp_near_target(int target_index)
{
    const struct rp_target *target = &rp_targets[target_index];
    return rp_iabs(game.player_x - target->x) <= RP_TARGET_RADIUS &&
           rp_iabs(game.player_y - target->y) <= RP_TARGET_RADIUS;
}

static int rp_find_cursor_target(void)
{
    int i;
    int best = -1;
    int best_score = RP_CURSOR_RADIUS * 2 + 1;

    for (i = 0; i < RP_MAX_TARGETS; i++)
    {
        int dx = rp_iabs(game.cursor_x - rp_targets[i].x);
        int dy = rp_iabs(game.cursor_y - rp_targets[i].y);
        int score = dx + dy;

        if (dx <= RP_CURSOR_RADIUS && dy <= RP_CURSOR_RADIUS &&
            score < best_score)
        {
            best = i;
            best_score = score;
        }
    }

    return best;
}

static void rp_update_cursor_selection(void)
{
    game.selected = rp_find_cursor_target();
}

static void rp_move_cursor(int dx, int dy)
{
    game.cursor_x = rp_clamp_int(game.cursor_x + dx, 8, RP_WORLD_W - 9);
    game.cursor_y = rp_clamp_int(game.cursor_y + dy, 8, RP_WORLD_H - 9);
    rp_update_cursor_selection();
    rp_update_camera();
}

static void rp_set_message(const char *line1, const char *line2)
{
    rb->strlcpy(game.message, line1 ? line1 : "", sizeof(game.message));
    rb->strlcpy(game.detail, line2 ? line2 : "", sizeof(game.detail));
}

static void rp_face_point(int x, int y)
{
    int dx = x - game.player_x;
    int dy = y - game.player_y;

    if (rp_iabs(dx) > rp_iabs(dy))
        game.player_dir = dx >= 0 ? RP_DIR_EAST : RP_DIR_WEST;
    else if (dy != 0)
        game.player_dir = dy >= 0 ? RP_DIR_SOUTH : RP_DIR_NORTH;
}

static bool rp_activity_active(void)
{
    return game.activity != RP_ACTIVITY_NONE &&
           TIME_BEFORE(*rb->current_tick, game.activity_until);
}

static void rp_update_activity(void)
{
    if (game.activity != RP_ACTIVITY_NONE &&
        !TIME_BEFORE(*rb->current_tick, game.activity_until))
    {
        game.activity = RP_ACTIVITY_NONE;
        game.activity_target = -1;
    }
}

static int rp_activity_phase(int phases)
{
    long total = game.activity_until - game.activity_started;
    long elapsed = *rb->current_tick - game.activity_started;
    int phase;

    if (phases <= 1 || total <= 0)
        return 0;

    if (elapsed < 0)
        elapsed = 0;
    if (elapsed >= total)
        return phases - 1;

    phase = (int)((elapsed * phases) / total);
    return rp_clamp_int(phase, 0, phases - 1);
}

static void rp_start_activity(enum rp_activity activity, int target_index,
                              int duration_ticks)
{
    game.activity = activity;
    game.activity_target = target_index;
    game.activity_started = *rb->current_tick;
    game.activity_until = *rb->current_tick + MAX(1, duration_ticks);

    if (target_index >= 0)
        rp_face_point(rp_targets[target_index].x, rp_targets[target_index].y);
}

static void rp_init_game(void)
{
    rb->memset(&game, 0, sizeof(game));
    game.view = RP_VIEW_TITLE;
    game.player_x = 320;
    game.player_y = 210;
    game.dest_x = game.player_x;
    game.dest_y = game.player_y;
    game.cursor_x = game.player_x;
    game.cursor_y = game.player_y;
    game.player_dir = RP_DIR_SOUTH;
    game.selected = -1;
    game.activity = RP_ACTIVITY_NONE;
    game.activity_target = -1;
    rp_update_camera();
    game.hp = 10;
    game.enemy_hp = 6;
    game.inv.coins = 4;
    game.inv.food = 1;
    rp_set_message("RunePod prototype", "Select starts, Menu exits");
}

static const char *rp_group_name(enum rp_group group)
{
    switch (group)
    {
        case RP_GROUP_NPC: return "NPC";
        case RP_GROUP_RESOURCE: return "Resource";
        case RP_GROUP_CRAFT: return "Craft";
        case RP_GROUP_COMBAT: return "Combat";
        case RP_GROUP_EXIT: return "Exit";
        default: return "Target";
    }
}

static void rp_begin_walk_to(int x, int y, bool with_action)
{
    game.dest_x = x;
    game.dest_y = y;
    game.moving = true;
    game.pending_action = with_action;
    game.activity = RP_ACTIVITY_NONE;
    rp_face_point(x, y);
}

static void rp_begin_walk_to_cursor(void)
{
    rp_begin_walk_to(game.cursor_x, game.cursor_y, false);
    rp_set_message("Walking", "Cursor destination");
}

static void rp_begin_walk_to_selected(bool with_action)
{
    if (game.selected < 0)
    {
        rp_begin_walk_to_cursor();
        return;
    }

    rp_begin_walk_to(rp_targets[game.selected].x,
                     rp_targets[game.selected].y,
                     with_action);
    rp_set_message(with_action ? "Walking to target" : "Walking",
                   rp_targets[game.selected].name);
}

static void rp_add_xp(enum rp_target_kind kind, int amount)
{
    switch (kind)
    {
        case RP_TARGET_TREE: game.xp.woodcutting += amount; break;
        case RP_TARGET_ROCK: game.xp.mining += amount; break;
        case RP_TARGET_FISH: game.xp.fishing += amount; break;
        case RP_TARGET_FIRE: game.xp.cooking += amount; break;
        case RP_TARGET_BENCH: game.xp.crafting += amount; break;
        case RP_TARGET_ENEMY: game.xp.combat += amount; break;
        default: break;
    }
}

static void rp_talk_guide(void)
{
    if (game.quest_stage == 0)
    {
        game.quest_stage = 1;
        rp_set_message("Guide: Bring supplies",
                       "3 logs, 2 ore, and 1 cooked fish");
    }
    else if (game.quest_stage == 1 &&
             game.inv.logs >= 3 && game.inv.ore >= 2 && game.inv.food >= 1)
    {
        game.inv.logs -= 3;
        game.inv.ore -= 2;
        game.inv.food -= 1;
        game.inv.coins += 12;
        game.inv.charms += 1;
        game.quest_stage = 2;
        rp_set_message("Quest complete",
                       "+12 coins, +1 village charm");
    }
    else if (game.quest_stage == 1)
    {
        rp_set_message("Guide: Keep gathering",
                       "Need 3 logs, 2 ore, 1 cooked fish");
    }
    else
    {
        rp_set_message("Guide: The cave is open",
                       "Try fighting the ratling");
    }
}

static void rp_trade_shop(void)
{
    if (game.inv.coins >= 3)
    {
        game.inv.coins -= 3;
        game.inv.food += 1;
        rp_set_message("Bought field ration",
                       "-3 coins, +1 food");
    }
    else
    {
        rp_set_message("Shopkeeper",
                       "Food costs 3 coins");
    }
}

static void rp_execute_selected_action(void)
{
    const struct rp_target *target;
    long now = *rb->current_tick;

    game.pending_action = false;

    if (game.selected < 0)
    {
        rp_begin_walk_to_cursor();
        return;
    }

    target = &rp_targets[game.selected];

    if (!rp_near_target(game.selected))
    {
        rp_begin_walk_to_selected(true);
        return;
    }

    if (game.cooldown_until[game.selected] > now)
    {
        rp_set_message("Still recovering", target->name);
        return;
    }

    switch (target->kind)
    {
        case RP_TARGET_NPC:
            if (game.selected == 0)
                rp_talk_guide();
            else
                rp_trade_shop();
            break;

        case RP_TARGET_TREE:
            game.inv.logs++;
            rp_add_xp(target->kind, 5);
            game.cooldown_until[game.selected] = now + HZ * 3;
            rp_start_activity(RP_ACTIVITY_CHOP, game.selected, HZ * 3 / 4);
            rp_set_message("You chop the oak",
                           "+1 log, +5 woodcutting XP");
            break;

        case RP_TARGET_ROCK:
            game.inv.ore++;
            rp_add_xp(target->kind, 5);
            game.cooldown_until[game.selected] = now + HZ * 3;
            rp_start_activity(RP_ACTIVITY_MINE, game.selected, HZ * 3 / 4);
            rp_set_message("You mine copper",
                           "+1 ore, +5 mining XP");
            break;

        case RP_TARGET_FISH:
            game.inv.raw_fish++;
            rp_add_xp(target->kind, 4);
            game.cooldown_until[game.selected] = now + HZ * 2;
            rp_start_activity(RP_ACTIVITY_FISH, game.selected, HZ);
            rp_set_message("You catch a fish",
                           "+1 raw fish, +4 fishing XP");
            break;

        case RP_TARGET_FIRE:
            if (game.inv.raw_fish > 0)
            {
                game.inv.raw_fish--;
                game.inv.food++;
                rp_add_xp(target->kind, 4);
                rp_start_activity(RP_ACTIVITY_COOK, game.selected, HZ * 3 / 4);
                rp_set_message("The fish cooks cleanly",
                               "+1 food, +4 cooking XP");
            }
            else
            {
                rp_set_message("Nothing to cook",
                               "Catch fish at the pond");
            }
            break;

        case RP_TARGET_BENCH:
            if (game.inv.logs >= 2 && game.inv.ore >= 1)
            {
                game.inv.logs -= 2;
                game.inv.ore -= 1;
                game.inv.coins += 4;
                rp_add_xp(target->kind, 6);
                rp_start_activity(RP_ACTIVITY_CRAFT, game.selected, HZ * 3 / 4);
                rp_set_message("You craft a tool haft",
                               "+4 coins, +6 crafting XP");
            }
            else
            {
                rp_set_message("Workbench",
                               "Needs 2 logs and 1 ore");
            }
            break;

        case RP_TARGET_ENEMY:
            rp_start_activity(RP_ACTIVITY_FIGHT, game.selected, HZ * 2 / 3);
            game.enemy_hp -= 2 + (rb->rand() % 2);
            if (game.enemy_hp <= 0)
            {
                game.inv.coins += 2;
                game.enemy_hp = 6;
                rp_add_xp(target->kind, 8);
                rp_set_message("Ratling defeated",
                               "+2 coins, +8 combat XP");
            }
            else
            {
                if (game.inv.food > 0 && game.hp <= 4)
                {
                    game.inv.food--;
                    game.hp += 4;
                    if (game.hp > 10)
                        game.hp = 10;
                    rp_start_activity(RP_ACTIVITY_EAT, game.selected, HZ / 2);
                    rp_set_message("You eat food mid-fight",
                                   "Recovered health");
                }
                else
                {
                    game.hp--;
                    rp_set_message("You strike the ratling",
                                   "It claws back");
                    if (game.hp <= 0)
                    {
                        game.hp = 10;
                        game.player_x = 320;
                        game.player_y = 210;
                        game.dest_x = game.player_x;
                        game.dest_y = game.player_y;
                        game.cursor_x = game.player_x;
                        game.cursor_y = game.player_y;
                        game.player_dir = RP_DIR_SOUTH;
                        rp_update_cursor_selection();
                        rp_update_camera();
                        game.moving = false;
                        rp_set_message("You retreat to the square",
                                       "Health restored");
                    }
                }
            }
            break;

        case RP_TARGET_EXIT:
            rp_set_message("Village gate",
                           "More zones land after the input slice");
            break;
    }
}

static void rp_open_action_menu(void)
{
    const struct rp_target *target;

    if (game.selected < 0)
    {
        rp_begin_walk_to_cursor();
        return;
    }

    target = &rp_targets[game.selected];

    game.action_selected = 0;
    game.actions[0] = target->default_action;
    game.actions[1] = "Walk here";
    game.actions[2] = "Examine";
    game.action_count = 3;

    if (target->kind == RP_TARGET_ENEMY)
    {
        game.actions[1] = "Use food";
        game.actions[2] = "Walk here";
        game.actions[3] = "Examine";
        game.action_count = 4;
    }

    game.view = RP_VIEW_ACTIONS;
}

static void rp_execute_menu_action(void)
{
    const char *action = game.actions[game.action_selected];

    if (!rb->strcmp(action, "Walk here"))
    {
        game.view = RP_VIEW_WORLD;
        rp_begin_walk_to_selected(false);
    }
    else if (!rb->strcmp(action, "Use food"))
    {
        if (game.inv.food > 0 && game.hp < 10)
        {
            game.inv.food--;
            game.hp += 4;
            if (game.hp > 10)
                game.hp = 10;
            rp_start_activity(RP_ACTIVITY_EAT, game.selected, HZ / 2);
            rp_set_message("You eat food", "Health recovered");
        }
        else
        {
            rp_set_message("No useful food", "Buy or cook more");
        }
        game.view = RP_VIEW_WORLD;
    }
    else if (!rb->strcmp(action, "Examine"))
    {
        if (game.selected < 0)
        {
            rp_set_message("Open ground", "Walk here");
            game.view = RP_VIEW_DIALOGUE;
            return;
        }

        rb->snprintf(game.message, sizeof(game.message), "%s",
                     rp_targets[game.selected].name);
        rb->snprintf(game.detail, sizeof(game.detail), "%s target, %s group",
                     rp_targets[game.selected].default_action,
                     rp_group_name(rp_targets[game.selected].group));
        game.view = RP_VIEW_DIALOGUE;
    }
    else
    {
        game.view = RP_VIEW_WORLD;
        rp_execute_selected_action();
    }
}

static void rp_update_movement(void)
{
    int dx;
    int dy;

    if (!game.moving)
        return;

    dx = game.dest_x - game.player_x;
    dy = game.dest_y - game.player_y;

    if (rp_iabs(dx) > rp_iabs(dy))
        game.player_dir = dx >= 0 ? RP_DIR_EAST : RP_DIR_WEST;
    else if (dy != 0)
        game.player_dir = dy >= 0 ? RP_DIR_SOUTH : RP_DIR_NORTH;

    if (rp_iabs(dx) <= RP_PLAYER_SPEED && rp_iabs(dy) <= RP_PLAYER_SPEED)
    {
        game.player_x = game.dest_x;
        game.player_y = game.dest_y;
        game.moving = false;
        if (game.pending_action)
            rp_execute_selected_action();
        return;
    }

    if (dx > 0)
        game.player_x += MIN(dx, RP_PLAYER_SPEED);
    else if (dx < 0)
        game.player_x += MAX(dx, -RP_PLAYER_SPEED);

    if (dy > 0)
        game.player_y += MIN(dy, RP_PLAYER_SPEED);
    else if (dy < 0)
        game.player_y += MAX(dy, -RP_PLAYER_SPEED);
}

static void rp_draw_text_clip(int x, int y, const char *text, int max_chars)
{
    char buf[64];
    if ((int)rb->strlen(text) > max_chars)
    {
        rb->strlcpy(buf, text, MIN((int)sizeof(buf), max_chars + 1));
        rb->strlcat(buf, "...", sizeof(buf));
        rb->lcd_putsxy(x, y, buf);
    }
    else
    {
        rb->lcd_putsxy(x, y, text);
    }
}

static void rp_fill(int color, int x, int y, int w, int h)
{
    rb->lcd_set_foreground(color);
    rb->lcd_fillrect(x, y, w, h);
}

static void rp_rect(int color, int x, int y, int w, int h)
{
    rb->lcd_set_foreground(color);
    rb->lcd_drawrect(x, y, w, h);
}

static int rp_anim_frame(int frames_per_second)
{
    int ticks_per_frame = MAX(1, HZ / frames_per_second);
    return (int)((*rb->current_tick / ticks_per_frame) & 1);
}

static int rp_skill_level(int xp)
{
    return 1 + xp / 10;
}

static int rp_skill_progress(int xp)
{
    return xp % 10;
}

static void rp_draw_sprite(enum rp_sprite sprite, int x, int y)
{
    int sx;
    int sy;
    int stride;

    if (!rp_sprites_loaded)
        return;

    sx = ((int)sprite % 10) * RP_SPRITE_W;
    sy = ((int)sprite / 10) * RP_SPRITE_H;
    stride = STRIDE(SCREEN_MAIN, RP_SPRITE_SHEET_W, RP_SPRITE_SHEET_H);
    rb->lcd_bitmap_transparent_part(rp_sprite_pixels, sx, sy,
                                    stride, x, y,
                                    RP_SPRITE_W, RP_SPRITE_H);
}

static void rp_draw_sprite_anchor(enum rp_sprite sprite, int x, int y)
{
    rp_draw_sprite(sprite, x - RP_SPRITE_W / 2, y - RP_SPRITE_H);
}

static void rp_draw_player_dir(int x, int y)
{
    int sx;
    int stride;

    if (!rp_player_dirs_loaded)
        return;

    sx = (int)game.player_dir * RP_SPRITE_W;
    stride = STRIDE(SCREEN_MAIN, RP_PLAYER_DIR_SHEET_W, RP_PLAYER_DIR_SHEET_H);
    rb->lcd_bitmap_transparent_part(rp_player_dir_pixels, sx, 0,
                                    stride, x, y,
                                    RP_SPRITE_W, RP_SPRITE_H);
}

static void rp_draw_player_dir_anchor(int x, int y)
{
    rp_draw_player_dir(x - RP_SPRITE_W / 2, y - RP_SPRITE_H);
}

static void rp_draw_tile(enum rp_tile tile, int x, int y)
{
    int stride;

    if (!rp_terrain_loaded)
        return;

    stride = STRIDE(SCREEN_MAIN, RP_TERRAIN_SHEET_W, RP_TERRAIN_SHEET_H);
    rb->lcd_bitmap_part(rp_terrain_pixels, (int)tile * RP_TILE_SIZE, 0,
                        stride, x, y, RP_TILE_SIZE, RP_TILE_SIZE);
}

static int rp_target_anim_offset(enum rp_target_kind kind, bool selected)
{
    int frame = rp_anim_frame(3);

    switch (kind)
    {
        case RP_TARGET_FIRE:
        case RP_TARGET_FISH:
            return frame ? -1 : 0;
        case RP_TARGET_NPC:
        case RP_TARGET_ENEMY:
            return (selected && frame) ? -1 : 0;
        default:
            return 0;
    }
}

static void rp_draw_sprite_effect(enum rp_target_kind kind, int x, int y)
{
    int frame = rp_anim_frame(4);

    switch (kind)
    {
        case RP_TARGET_FIRE:
            rp_fill(frame ? RP_COL_ACCENT : LCD_RGBPACK(245, 142, 58),
                    x - 2, y - 20, 4, 9);
            break;
        case RP_TARGET_FISH:
            rb->lcd_set_foreground(frame ? LCD_RGBPACK(164, 210, 222)
                                         : LCD_RGBPACK(91, 151, 181));
            rb->lcd_hline(x - 9, x + 10, y - 15);
            rb->lcd_hline(x - 6, x + 7, y - 10);
            break;
        case RP_TARGET_ENEMY:
            if (frame)
                rp_rect(LCD_RGBPACK(180, 68, 72), x - 13, y - 28, 26, 25);
            break;
        default:
            break;
    }
}

static void rp_draw_tree(int x, int y)
{
    rp_fill(RP_COL_WOOD, x - 3, y - 2, 6, 16);
    rp_fill(RP_COL_TREE, x - 12, y - 14, 24, 18);
    rp_fill(RP_COL_GRASS_DARK, x - 8, y - 19, 16, 11);
}

static void rp_draw_rock(int x, int y)
{
    rp_fill(RP_COL_STONE, x - 10, y - 8, 20, 15);
    rp_fill(LCD_RGBPACK(82, 85, 82), x - 5, y - 12, 13, 9);
}

static void rp_draw_fish(int x, int y)
{
    rp_fill(RP_COL_WATER, x - 18, y - 10, 36, 20);
    rp_rect(LCD_RGBPACK(143, 184, 205), x - 18, y - 10, 36, 20);
    rp_fill(LCD_RGBPACK(190, 210, 218), x - 5, y - 2, 10, 4);
}

static void rp_draw_npc(int x, int y, int color)
{
    rp_fill(color, x - 5, y - 14, 10, 16);
    rp_fill(LCD_RGBPACK(204, 164, 120), x - 4, y - 21, 8, 7);
}

static bool rp_in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static bool rp_building_edge(int x, int y, int rx, int ry, int rw, int rh)
{
    return rp_in_rect(x, y, rx, ry, rw, rh) &&
           (x < rx + RP_TILE_SIZE || x >= rx + rw - RP_TILE_SIZE ||
            y < ry + RP_TILE_SIZE || y >= ry + rh - RP_TILE_SIZE);
}

static enum rp_tile rp_tile_at(int world_x, int world_y)
{
    if (world_x > 500 && world_y > 300)
        return RP_TILE_CAVE;
    if (world_x > 485 && world_y > 185 && world_y < 285)
        return RP_TILE_WATER;
    if (rp_building_edge(world_x, world_y, 368, 96, 96, 96) ||
        rp_building_edge(world_x, world_y, 248, 128, 96, 80))
        return RP_TILE_VILLAGE;
    if (rp_in_rect(world_x, world_y, 400, 128, 32, 32) ||
        rp_in_rect(world_x, world_y, 280, 160, 32, 32) ||
        rp_in_rect(world_x, world_y, 344, 144, 48, 48))
        return RP_TILE_WOOD;
    if (rp_in_rect(world_x, world_y, 360, 176, 112, 64))
        return RP_TILE_STONE;
    if (world_y > 188 && world_y < 234)
        return RP_TILE_PATH;
    if (world_x > 300 && world_x < 344)
        return RP_TILE_PATH;
    if (world_x > 420 && world_y > 268)
        return RP_TILE_STONE;
    if (((world_x / RP_TILE_SIZE) + (world_y / RP_TILE_SIZE)) & 1)
        return RP_TILE_DARK_GRASS;
    return RP_TILE_GRASS;
}

static void rp_draw_terrain(void)
{
    int sx;
    int sy;
    int start_x = (game.camera_x / RP_TILE_SIZE) * RP_TILE_SIZE;
    int start_y = (game.camera_y / RP_TILE_SIZE) * RP_TILE_SIZE;
    int world_y;
    int world_x;

    if (!rp_terrain_loaded)
    {
        rp_fill(RP_COL_GRASS, 0, RP_WORLD_TOP, LCD_WIDTH,
                RP_WORLD_BOTTOM - RP_WORLD_TOP);
        for (sy = RP_WORLD_TOP; sy < RP_WORLD_BOTTOM; sy += 16)
        {
            for (sx = 0; sx < LCD_WIDTH; sx += 16)
            {
                if (((sx + sy) / 16) & 1)
                    rp_fill(LCD_RGBPACK(66, 118, 72), sx, sy, 16, 16);
            }
        }
        return;
    }

    for (world_y = start_y; world_y < game.camera_y + RP_WORLD_VIEW_H + RP_TILE_SIZE;
         world_y += RP_TILE_SIZE)
    {
        for (world_x = start_x; world_x < game.camera_x + LCD_WIDTH + RP_TILE_SIZE;
             world_x += RP_TILE_SIZE)
        {
            sx = rp_screen_x(world_x);
            sy = rp_screen_y(world_y);
            rp_draw_tile(rp_tile_at(world_x + RP_TILE_SIZE / 2,
                                    world_y + RP_TILE_SIZE / 2),
                         sx, sy);
        }
    }
}

static void rp_draw_target(int i)
{
    const struct rp_target *target = &rp_targets[i];
    bool selected = i == game.selected;
    enum rp_sprite sprite = RP_SPR_GUIDE;
    int y_offset;
    int sx;
    int sy;

    if (!rp_world_visible(target->x, target->y, 48))
        return;

    sx = rp_screen_x(target->x);
    sy = rp_screen_y(target->y);

    if (selected)
    {
        int pulse = rp_anim_frame(5);
        rp_rect(pulse ? RP_COL_FOCUS : RP_COL_ACCENT,
                sx - 17, sy - 24, 34, 34);
    }

    switch (target->kind)
    {
        case RP_TARGET_TREE: sprite = RP_SPR_OAK; break;
        case RP_TARGET_ROCK: sprite = RP_SPR_COPPER; break;
        case RP_TARGET_FISH: sprite = RP_SPR_POND; break;
        case RP_TARGET_FIRE: sprite = RP_SPR_FIRE; break;
        case RP_TARGET_BENCH: sprite = RP_SPR_WORKBENCH; break;
        case RP_TARGET_ENEMY: sprite = RP_SPR_RATLING; break;
        case RP_TARGET_EXIT: sprite = RP_SPR_COMBAT; break;
        case RP_TARGET_NPC:
        default:
            sprite = i == 0 ? RP_SPR_GUIDE : RP_SPR_SHOPKEEPER;
            break;
    }

    if (rp_sprites_loaded)
    {
        y_offset = rp_target_anim_offset(target->kind, selected);
        rp_draw_sprite_anchor(sprite, sx, sy + y_offset);
        rp_draw_sprite_effect(target->kind, sx, sy + y_offset);
        return;
    }

    switch (target->kind)
    {
        case RP_TARGET_TREE:
            rp_draw_tree(sx, sy);
            break;
        case RP_TARGET_ROCK:
            rp_draw_rock(sx, sy);
            break;
        case RP_TARGET_FISH:
            rp_draw_fish(sx, sy);
            break;
        case RP_TARGET_FIRE:
            rp_fill(RP_COL_FIRE, sx - 6, sy - 10, 12, 16);
            rp_fill(RP_COL_ACCENT, sx - 3, sy - 7, 6, 10);
            break;
        case RP_TARGET_BENCH:
            rp_fill(RP_COL_WOOD, sx - 16, sy - 7, 32, 8);
            rp_fill(RP_COL_STONE, sx - 12, sy + 1, 24, 7);
            break;
        case RP_TARGET_ENEMY:
            rp_draw_npc(sx, sy, RP_COL_ENEMY);
            break;
        case RP_TARGET_EXIT:
            rp_rect(RP_COL_ACCENT, sx - 12, sy - 18, 24, 26);
            break;
        case RP_TARGET_NPC:
        default:
            rp_draw_npc(sx, sy,
                        i == 0 ? LCD_RGBPACK(86, 116, 74)
                               : LCD_RGBPACK(126, 88, 48));
            break;
    }
}

static void rp_draw_player(void)
{
    int sx;
    int sy;
    int bob = 0;

    if (!rp_world_visible(game.player_x, game.player_y, 48))
        return;

    sx = rp_screen_x(game.player_x);
    sy = rp_screen_y(game.player_y);

    if (game.moving && rp_anim_frame(6))
        bob = -1;
    else if (rp_activity_active() &&
             game.activity != RP_ACTIVITY_FISH &&
             rp_activity_phase(4) == 1)
        bob = -1;

    if (rp_player_dirs_loaded)
    {
        rp_draw_player_dir_anchor(sx, sy + bob);
        return;
    }

    if (rp_sprites_loaded)
    {
        rp_draw_sprite_anchor(RP_SPR_PLAYER, sx, sy + bob);
        return;
    }

    rp_fill(RP_COL_PLAYER, sx - 6, sy - 15, 12, 17);
    rp_fill(LCD_RGBPACK(218, 174, 126), sx - 4,
            sy - 22, 8, 8);
    rp_fill(LCD_RGBPACK(36, 45, 75), sx - 7,
            sy + 2, 14, 4);
}

static void rp_draw_line(int color, int x1, int y1, int x2, int y2)
{
    rb->lcd_set_foreground(color);
    rb->lcd_drawline(x1, y1, x2, y2);
}

static void rp_draw_sparks(int x, int y, int phase)
{
    int c = phase & 1 ? RP_COL_ACCENT : LCD_RGBPACK(245, 226, 130);

    rp_draw_line(c, x - 10, y - 16, x - 4, y - 20);
    rp_draw_line(c, x + 5, y - 14, x + 12, y - 18);
    rp_draw_line(c, x - 2, y - 22, x + 3, y - 27);
}

static void rp_draw_action_tool(int color, int x1, int y1, int x2, int y2)
{
    rp_draw_line(color, x1, y1, x2, y2);
    rp_draw_line(color, x1 + 1, y1, x2 + 1, y2);
}

static void rp_draw_action_animation(void)
{
    const struct rp_target *target = NULL;
    int phase;
    int psx;
    int psy;
    int tx = 0;
    int ty = 0;
    int sign;
    int hand_x;
    int hand_y;

    if (!rp_activity_active() || !rp_world_visible(game.player_x, game.player_y, 48))
        return;

    if (game.activity_target >= 0 && game.activity_target < RP_MAX_TARGETS)
    {
        target = &rp_targets[game.activity_target];
        if (!rp_world_visible(target->x, target->y, 48))
            target = NULL;
    }

    if (!target && game.activity != RP_ACTIVITY_EAT)
        return;

    phase = rp_activity_phase(4);
    psx = rp_screen_x(game.player_x);
    psy = rp_screen_y(game.player_y);
    if (target)
    {
        tx = rp_screen_x(target->x);
        ty = rp_screen_y(target->y);
    }

    sign = target && target->x < game.player_x ? -1 : 1;
    hand_x = psx + sign * 7;
    hand_y = psy - 18;

    switch (game.activity)
    {
        case RP_ACTIVITY_CHOP:
        {
            int head_x = phase < 2 ? hand_x + sign * (11 + phase * 3)
                                   : tx - sign * 4;
            int head_y = phase < 2 ? hand_y - 13 + phase * 9 : ty - 20;

            rp_draw_action_tool(RP_COL_WOOD, hand_x, hand_y, head_x, head_y);
            rp_draw_line(RP_COL_STONE, head_x - sign * 4, head_y - 3,
                         head_x + sign * 5, head_y + 3);
            if (phase >= 2)
            {
                rp_draw_line(RP_COL_GRASS_DARK, tx - 12, ty - 24, tx - 5, ty - 30);
                rp_draw_line(RP_COL_GRASS_DARK, tx + 5, ty - 21, tx + 12, ty - 27);
            }
            break;
        }

        case RP_ACTIVITY_MINE:
        {
            int head_x = phase < 2 ? hand_x + sign * (10 + phase * 4)
                                   : tx - sign * 3;
            int head_y = phase < 2 ? hand_y - 10 + phase * 8 : ty - 10;

            rp_draw_action_tool(RP_COL_WOOD, hand_x, hand_y, head_x, head_y);
            rp_draw_line(RP_COL_STONE, head_x - sign * 6, head_y,
                         head_x + sign * 6, head_y - 4);
            if (phase >= 2)
                rp_draw_sparks(tx, ty, phase);
            break;
        }

        case RP_ACTIVITY_FISH:
        {
            int rod_x = psx + sign * 8;
            int rod_y = psy - 22;
            int bob_x = tx - 8 + phase * 5;
            int bob_y = ty - 11 + ((phase & 1) ? 2 : -1);

            rp_draw_line(RP_COL_WOOD, rod_x, rod_y, rod_x + sign * 12, rod_y - 8);
            rp_draw_line(RP_COL_MUTED, rod_x + sign * 12, rod_y - 8,
                         bob_x, bob_y);
            rp_fill(RP_COL_FIRE, bob_x - 1, bob_y - 2, 3, 4);
            rp_rect(LCD_RGBPACK(164, 210, 222),
                    bob_x - 7 - phase, bob_y + 4 - phase,
                    14 + phase * 2, 5 + phase);
            break;
        }

        case RP_ACTIVITY_COOK:
            rp_fill(LCD_RGBPACK(245, 142, 58), tx - 8, ty - 23 - phase,
                    16, 12 + phase);
            rp_fill(RP_COL_ACCENT, tx - 4, ty - 19 - phase, 8, 8);
            rp_draw_line(RP_COL_MUTED, tx - 6, ty - 31 - phase,
                         tx - 10, ty - 38 - phase * 2);
            rp_draw_line(RP_COL_MUTED, tx + 4, ty - 29 - phase,
                         tx + 9, ty - 36 - phase * 2);
            break;

        case RP_ACTIVITY_CRAFT:
        {
            int hammer_x = phase < 2 ? hand_x + sign * 10 : tx;
            int hammer_y = phase < 2 ? hand_y - 11 + phase * 8 : ty - 13;

            rp_draw_action_tool(RP_COL_WOOD, hand_x, hand_y, hammer_x, hammer_y);
            rp_draw_line(RP_COL_STONE, hammer_x - 6, hammer_y,
                         hammer_x + 6, hammer_y);
            if (phase >= 2)
                rp_draw_sparks(tx, ty, phase);
            break;
        }

        case RP_ACTIVITY_FIGHT:
        {
            int slash_x = (psx + tx) / 2;
            int slash_y = (psy + ty) / 2 - 16;

            rp_draw_line(RP_COL_ACCENT, hand_x, hand_y,
                         tx - sign * (8 - phase), ty - 18 + phase);
            rp_draw_line(LCD_RGBPACK(235, 235, 220),
                         slash_x - 10, slash_y - 6,
                         slash_x + 12, slash_y + 8);
            if (phase >= 1)
                rp_rect(LCD_RGBPACK(180, 68, 72), tx - 14, ty - 29, 28, 26);
            break;
        }

        case RP_ACTIVITY_EAT:
            rp_fill(RP_COL_ACCENT, psx - 4, psy - 27 - phase, 8, 6);
            rp_draw_line(LCD_RGBPACK(116, 190, 100),
                         psx - 9, psy - 34 - phase,
                         psx + 9, psy - 34 - phase);
            rp_draw_line(LCD_RGBPACK(116, 190, 100),
                         psx, psy - 42 - phase,
                         psx, psy - 27 - phase);
            break;

        case RP_ACTIVITY_NONE:
        default:
            break;
    }
}

static void rp_draw_cursor(void)
{
    int pulse = rp_anim_frame(5);
    int color = game.selected >= 0 ? RP_COL_FOCUS : RP_COL_ACCENT;
    int sx = rp_screen_x(game.cursor_x);
    int sy = rp_screen_y(game.cursor_y);

    if (!pulse && game.selected >= 0)
        color = RP_COL_ACCENT;

    rb->lcd_set_foreground(color);
    rb->lcd_hline(sx - 7, sx - 3, sy);
    rb->lcd_hline(sx + 3, sx + 7, sy);
    rb->lcd_vline(sx, sy - 7, sy - 3);
    rb->lcd_vline(sx, sy + 3, sy + 7);
    rb->lcd_drawrect(sx - 2, sy - 2, 5, 5);
}

static void rp_draw_world(void)
{
    int x;
    char buf[64];

    rp_update_camera();
    rp_draw_terrain();

    for (x = 0; x < RP_MAX_TARGETS; x++)
        rp_draw_target(x);

    rp_draw_player();
    rp_draw_action_animation();
    rp_draw_cursor();

    rp_fill(RP_COL_PANEL, 0, 0, LCD_WIDTH, RP_TOP_H);
    rb->lcd_set_foreground(RP_COL_TEXT);
    if (game.selected >= 0)
        rb->snprintf(buf, sizeof(buf), "HP %d  Coins %d  Q%d  %s",
                     game.hp, game.inv.coins, game.quest_stage,
                     rp_group_name(rp_targets[game.selected].group));
    else
        rb->snprintf(buf, sizeof(buf), "HP %d  Coins %d  Q%d  Ground",
                     game.hp, game.inv.coins, game.quest_stage);
    rb->lcd_putsxy(4, 5, buf);

    rp_fill(RP_COL_PANEL, 0, RP_WORLD_BOTTOM, LCD_WIDTH, RP_BOTTOM_H);
    rb->lcd_set_foreground(RP_COL_ACCENT);
    if (game.selected >= 0)
        rb->snprintf(buf, sizeof(buf), "%s: %s",
                     rp_targets[game.selected].name,
                     rp_targets[game.selected].default_action);
    else
        rb->snprintf(buf, sizeof(buf), "Cursor: Walk here");
    rb->lcd_putsxy(5, RP_WORLD_BOTTOM + 3, buf);
    rb->lcd_set_foreground(RP_COL_TEXT);
    rp_draw_text_clip(5, RP_WORLD_BOTTOM + 17,
                      game.message[0] ? game.message : game.detail, 30);
    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(204, RP_WORLD_BOTTOM + 17, "Play: status");
}

static void rp_draw_title(void)
{
    rp_fill(LCD_RGBPACK(28, 43, 42), 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rp_fill(RP_COL_GRASS_DARK, 0, 138, LCD_WIDTH, 102);
    rp_fill(RP_COL_PATH, 118, 138, 82, 102);
    if (rp_sprites_loaded)
    {
        rp_draw_sprite_anchor(RP_SPR_OAK, 66, 164);
        rp_draw_sprite_anchor(RP_SPR_OAK, 256, 156);
        rp_draw_sprite_anchor(RP_SPR_COPPER, 92, 200);
        if (rp_player_dirs_loaded)
            rp_draw_player_dir_anchor(160, 170);
        else
            rp_draw_sprite_anchor(RP_SPR_PLAYER, 160, 170);
    }
    else
    {
        rp_draw_tree(66, 164);
        rp_draw_tree(256, 156);
        rp_draw_rock(92, 200);
        rp_draw_npc(160, 170, RP_COL_PLAYER);
    }

    rb->lcd_set_foreground(RP_COL_ACCENT);
    rb->lcd_putsxy(104, 55, "RunePod");
    rb->lcd_set_foreground(RP_COL_TEXT);
    rb->lcd_putsxy(54, 82, "Click-wheel fantasy RPG");
    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(50, 112, "Select starts   Menu exits");
}

static void rp_draw_menu_panel(const char *title)
{
    rp_fill(RP_COL_PANEL, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rp_fill(RP_COL_PANEL_2, 0, 0, LCD_WIDTH, 22);
    rb->lcd_set_foreground(RP_COL_ACCENT);
    rb->lcd_putsxy(5, 7, title);
}

static void rp_draw_actions(void)
{
    int i;

    rp_draw_menu_panel(rp_targets[game.selected].name);
    for (i = 0; i < game.action_count; i++)
    {
        int y = 36 + i * 22;
        if (i == game.action_selected)
        {
            rp_fill(RP_COL_ACCENT, 14, y - 3, LCD_WIDTH - 28, 18);
            rb->lcd_set_foreground(LCD_BLACK);
        }
        else
        {
            rb->lcd_set_foreground(RP_COL_TEXT);
        }
        rb->lcd_putsxy(24, y, game.actions[i]);
    }
    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(14, LCD_HEIGHT - 17, "Wheel scrolls  Select chooses  Menu backs");
}

static void rp_draw_inventory(void)
{
    char buf[64];
    int y = 36;

    rp_draw_menu_panel("Inventory");
    rb->lcd_set_foreground(RP_COL_TEXT);
    if (rp_sprites_loaded)
    {
        rp_draw_sprite(RP_SPR_LOGS, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Logs: %d", game.inv.logs);
        rb->lcd_putsxy(54, y, buf); y += 24;
        rp_draw_sprite(RP_SPR_ORE, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Ore: %d", game.inv.ore);
        rb->lcd_putsxy(54, y, buf); y += 24;
        rp_draw_sprite(RP_SPR_FISH, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Raw fish: %d", game.inv.raw_fish);
        rb->lcd_putsxy(54, y, buf); y += 24;
        rp_draw_sprite(RP_SPR_FOOD, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Food: %d", game.inv.food);
        rb->lcd_putsxy(54, y, buf); y += 24;
        rp_draw_sprite(RP_SPR_COINS, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Coins: %d", game.inv.coins);
        rb->lcd_putsxy(54, y, buf); y += 24;
        rp_draw_sprite(RP_SPR_COMBAT_ICON, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Charms: %d", game.inv.charms);
        rb->lcd_putsxy(54, y, buf);
    }
    else
    {
        rb->snprintf(buf, sizeof(buf), "Logs: %d", game.inv.logs);
        rb->lcd_putsxy(20, y, buf); y += 18;
        rb->snprintf(buf, sizeof(buf), "Ore: %d", game.inv.ore);
        rb->lcd_putsxy(20, y, buf); y += 18;
        rb->snprintf(buf, sizeof(buf), "Raw fish: %d", game.inv.raw_fish);
        rb->lcd_putsxy(20, y, buf); y += 18;
        rb->snprintf(buf, sizeof(buf), "Food: %d", game.inv.food);
        rb->lcd_putsxy(20, y, buf); y += 18;
        rb->snprintf(buf, sizeof(buf), "Coins: %d", game.inv.coins);
        rb->lcd_putsxy(20, y, buf); y += 18;
        rb->snprintf(buf, sizeof(buf), "Charms: %d", game.inv.charms);
        rb->lcd_putsxy(20, y, buf);
    }

    rb->lcd_set_foreground(RP_COL_ACCENT);
    rb->snprintf(buf, sizeof(buf), "XP C%d M%d W%d F%d Cook%d Cr%d",
                 game.xp.combat, game.xp.mining, game.xp.woodcutting,
                 game.xp.fishing, game.xp.cooking, game.xp.crafting);
    rp_draw_text_clip(20, 164, buf, 38);
    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(20, LCD_HEIGHT - 17, "Select levels  Left map  Menu returns");
}

static void rp_draw_level_row(int y, enum rp_sprite icon, const char *name,
                              int xp)
{
    char buf[48];
    int progress = rp_skill_progress(xp);
    int filled = progress * 9;

    if (rp_sprites_loaded)
        rp_draw_sprite(icon, 14, y - 10);

    rb->lcd_set_foreground(RP_COL_TEXT);
    rb->snprintf(buf, sizeof(buf), "%s  L%d", name, rp_skill_level(xp));
    rb->lcd_putsxy(rp_sprites_loaded ? 52 : 20, y, buf);

    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->snprintf(buf, sizeof(buf), "%d/10", progress);
    rb->lcd_putsxy(142, y, buf);

    rp_rect(RP_COL_MUTED, 190, y + 1, 96, 7);
    rp_fill(RP_COL_ACCENT, 191, y + 2, filled, 5);
}

static void rp_draw_levels(void)
{
    int y = 34;

    rp_draw_menu_panel("Levels");
    rp_draw_level_row(y, RP_SPR_COMBAT_ICON, "Combat", game.xp.combat);
    y += 26;
    rp_draw_level_row(y, RP_SPR_ORE, "Mining", game.xp.mining);
    y += 26;
    rp_draw_level_row(y, RP_SPR_LOGS, "Woodcut", game.xp.woodcutting);
    y += 26;
    rp_draw_level_row(y, RP_SPR_FISH, "Fishing", game.xp.fishing);
    y += 26;
    rp_draw_level_row(y, RP_SPR_FOOD, "Cooking", game.xp.cooking);
    y += 26;
    rp_draw_level_row(y, RP_SPR_WORKBENCH, "Crafting", game.xp.crafting);

    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(20, LCD_HEIGHT - 17, "Select map  Left bag  Menu returns");
}

static int rp_map_x(int world_x)
{
    return 20 + (world_x * 280) / RP_WORLD_W;
}

static int rp_map_y(int world_y)
{
    return 38 + (world_y * 154) / RP_WORLD_H;
}

static void rp_draw_map_marker(int color, int world_x, int world_y, int size)
{
    int x = rp_map_x(world_x);
    int y = rp_map_y(world_y);

    rb->lcd_set_foreground(color);
    rb->lcd_fillrect(x - size / 2, y - size / 2, size, size);
}

static void rp_draw_map(void)
{
    int i;
    int map_x = 20;
    int map_y = 38;
    int map_w = 280;
    int map_h = 154;
    int view_x = map_x + (game.camera_x * map_w) / RP_WORLD_W;
    int view_y = map_y + (game.camera_y * map_h) / RP_WORLD_H;
    int view_w = (LCD_WIDTH * map_w) / RP_WORLD_W;
    int view_h = (RP_WORLD_VIEW_H * map_h) / RP_WORLD_H;

    rp_draw_menu_panel("Map");

    rp_fill(RP_COL_GRASS_DARK, map_x, map_y, map_w, map_h);
    rp_fill(RP_COL_PATH, map_x + (300 * map_w) / RP_WORLD_W,
            map_y, (44 * map_w) / RP_WORLD_W, map_h);
    rp_fill(RP_COL_PATH, map_x, map_y + (188 * map_h) / RP_WORLD_H,
            map_w, (46 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WATER, map_x + (485 * map_w) / RP_WORLD_W,
            map_y + (185 * map_h) / RP_WORLD_H,
            (85 * map_w) / RP_WORLD_W, (100 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_STONE, map_x + (420 * map_w) / RP_WORLD_W,
            map_y + (268 * map_h) / RP_WORLD_H,
            (210 * map_w) / RP_WORLD_W, (140 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_STONE, map_x + (360 * map_w) / RP_WORLD_W,
            map_y + (176 * map_h) / RP_WORLD_H,
            (112 * map_w) / RP_WORLD_W, (64 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_ROOF, map_x + (368 * map_w) / RP_WORLD_W,
            map_y + (96 * map_h) / RP_WORLD_H,
            (96 * map_w) / RP_WORLD_W, (96 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WOOD, map_x + (400 * map_w) / RP_WORLD_W,
            map_y + (128 * map_h) / RP_WORLD_H,
            (32 * map_w) / RP_WORLD_W, (32 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_ROOF, map_x + (248 * map_w) / RP_WORLD_W,
            map_y + (128 * map_h) / RP_WORLD_H,
            (96 * map_w) / RP_WORLD_W, (80 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WOOD, map_x + (280 * map_w) / RP_WORLD_W,
            map_y + (160 * map_h) / RP_WORLD_H,
            (32 * map_w) / RP_WORLD_W, (32 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WOOD, map_x + (344 * map_w) / RP_WORLD_W,
            map_y + (144 * map_h) / RP_WORLD_H,
            (48 * map_w) / RP_WORLD_W, (48 * map_h) / RP_WORLD_H);

    rp_rect(RP_COL_MUTED, map_x, map_y, map_w, map_h);
    rp_rect(RP_COL_ACCENT, view_x, view_y, view_w, view_h);

    for (i = 0; i < RP_MAX_TARGETS; i++)
        rp_draw_map_marker(i == game.selected ? RP_COL_FOCUS : RP_COL_TEXT,
                           rp_targets[i].x, rp_targets[i].y, 4);

    rp_draw_map_marker(RP_COL_PLAYER, game.player_x, game.player_y, 6);
    rp_draw_map_marker(RP_COL_ACCENT, game.cursor_x, game.cursor_y, 4);

    rb->lcd_set_foreground(RP_COL_TEXT);
    rb->lcd_putsxy(22, 200, "Player blue  Cursor gold  Targets white");
    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(20, LCD_HEIGHT - 17, "Select bag  Left levels  Menu returns");
}

static void rp_draw_dialogue(void)
{
    rp_draw_world();
    rp_fill(RP_COL_PANEL, 22, 62, LCD_WIDTH - 44, 80);
    rp_rect(RP_COL_ACCENT, 22, 62, LCD_WIDTH - 44, 80);
    rb->lcd_set_foreground(RP_COL_TEXT);
    rp_draw_text_clip(34, 82, game.message, 34);
    rb->lcd_set_foreground(RP_COL_MUTED);
    rp_draw_text_clip(34, 104, game.detail, 34);
    rb->lcd_putsxy(34, 126, "Select/Menu closes");
}

static void rp_render(void)
{
    switch (game.view)
    {
        case RP_VIEW_TITLE:
            rp_draw_title();
            break;
        case RP_VIEW_ACTIONS:
            rp_draw_actions();
            break;
        case RP_VIEW_INVENTORY:
            rp_draw_inventory();
            break;
        case RP_VIEW_LEVELS:
            rp_draw_levels();
            break;
        case RP_VIEW_MAP:
            rp_draw_map();
            break;
        case RP_VIEW_DIALOGUE:
            rp_draw_dialogue();
            break;
        case RP_VIEW_WORLD:
        default:
            rp_draw_world();
            break;
    }
    rb->lcd_update();
}

static void rp_handle_world_event(long event)
{
    int step = (event & BUTTON_REPEAT) ? RP_CURSOR_REPEAT_STEP : RP_CURSOR_STEP;

    if (event == BUTTON_NONE)
        return;

    if ((event & BUTTON_SCROLL_FWD) && !(event & BUTTON_REL))
    {
        rp_move_cursor(0, step);
    }
    else if ((event & BUTTON_SCROLL_BACK) && !(event & BUTTON_REL))
    {
        rp_move_cursor(0, -step);
    }
    else if ((event & BUTTON_RIGHT) && !(event & BUTTON_REL))
    {
        rp_move_cursor(step, 0);
    }
    else if ((event & BUTTON_LEFT) && !(event & BUTTON_REL))
    {
        rp_move_cursor(-step, 0);
    }
    else if ((event & BUTTON_SELECT) && (event & BUTTON_REPEAT))
    {
        rp_open_action_menu();
    }
    else if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
    {
        rp_execute_selected_action();
    }
    else if ((event & BUTTON_PLAY) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
    {
        game.view = RP_VIEW_INVENTORY;
    }
    else if ((event & BUTTON_MENU) && !(event & BUTTON_REPEAT))
    {
        game.quit = true;
    }
}

static void rp_handle_event(long event)
{
    if (event == SYS_USB_CONNECTED ||
        rb->default_event_handler(event) == SYS_USB_CONNECTED)
    {
        game.quit = true;
        return;
    }

    switch (game.view)
    {
        case RP_VIEW_TITLE:
            if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
            {
                game.view = RP_VIEW_WORLD;
                rp_set_message("Welcome to the village",
                               "Wheel up/down, Left/Right cursor");
            }
            else if ((event & BUTTON_MENU) && !(event & BUTTON_REPEAT))
            {
                game.quit = true;
            }
            break;

        case RP_VIEW_WORLD:
            rp_handle_world_event(event);
            break;

        case RP_VIEW_ACTIONS:
            if ((event & BUTTON_SCROLL_FWD) && !(event & BUTTON_REL))
            {
                game.action_selected++;
                if (game.action_selected >= game.action_count)
                    game.action_selected = 0;
            }
            else if ((event & BUTTON_SCROLL_BACK) && !(event & BUTTON_REL))
            {
                game.action_selected--;
                if (game.action_selected < 0)
                    game.action_selected = game.action_count - 1;
            }
            else if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
            {
                rp_execute_menu_action();
            }
            else if ((event & BUTTON_MENU) && !(event & BUTTON_REPEAT))
            {
                game.view = RP_VIEW_WORLD;
            }
            break;

        case RP_VIEW_INVENTORY:
            if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_LEVELS;
            else if ((event & BUTTON_RIGHT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_LEVELS;
            else if ((event & BUTTON_LEFT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_MAP;
            else if ((event & BUTTON_MENU) && !(event & BUTTON_REPEAT))
                game.view = RP_VIEW_WORLD;
            break;

        case RP_VIEW_LEVELS:
            if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_MAP;
            else if ((event & BUTTON_RIGHT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_MAP;
            else if ((event & BUTTON_LEFT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_INVENTORY;
            else if ((event & BUTTON_MENU) && !(event & BUTTON_REPEAT))
                game.view = RP_VIEW_WORLD;
            break;

        case RP_VIEW_MAP:
            if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_INVENTORY;
            else if ((event & BUTTON_RIGHT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_INVENTORY;
            else if ((event & BUTTON_LEFT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_LEVELS;
            else if ((event & BUTTON_MENU) && !(event & BUTTON_REPEAT))
                game.view = RP_VIEW_WORLD;
            break;

        case RP_VIEW_DIALOGUE:
            if ((event & (BUTTON_MENU | BUTTON_SELECT)) &&
                !(event & BUTTON_REPEAT))
                game.view = RP_VIEW_WORLD;
            break;
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    int rendered_frames = 0;

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->srand((unsigned int)*rb->current_tick);
    rp_set_wheel_events(true);
    rp_load_sprites();
    rp_load_player_dirs();
    rp_load_terrain();
    rp_init_game();
    game.music_started = rp_start_music();
    if (game.music_started)
        rp_set_message("RunePod prototype", "Harmony background music");
    rp_smoke_log("start", 0);

    while (!game.quit)
    {
        long event = rb->button_get_w_tmo(RP_FRAME_TICKS);
        rp_handle_event(event);
        rp_update_movement();
        rp_update_activity();
        rp_render();
        rendered_frames++;
        if (rendered_frames == 3)
            rp_smoke_log("rendered_frames", rendered_frames);
    }

    rp_smoke_log("exit", rendered_frames);
    if (game.music_started)
        rb->audio_stop();
    rp_set_wheel_events(true);
    return PLUGIN_OK;
}

#else

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    return PLUGIN_OK;
}

#endif
