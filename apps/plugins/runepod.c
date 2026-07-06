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
#define RP_WORLD_W 960
#define RP_WORLD_H 640
#define RP_TILE_SIZE 32
#define RP_TARGET_RADIUS 14
#define RP_CURSOR_RADIUS 18
#define RP_CURSOR_STEP 7
#define RP_CURSOR_REPEAT_STEP 12
#define RP_PLAYER_SPEED 3
#define RP_MAX_TARGETS 36
#define RP_SAVE_V3_TARGETS 28
#define RP_SAVE_V2_TARGETS 20
#define RP_SAVE_V1_TARGETS 12
#define RP_MAX_ACTIONS 4
#define RP_SAVE_MAGIC 0x52505631u
#define RP_SAVE_VERSION 4u
#define RP_SAVE_VERSION_V3 3u
#define RP_SAVE_VERSION_V2 2u
#define RP_SAVE_VERSION_V1 1u
#define RP_AUTOSAVE_TICKS (HZ * 5)
#define RP_SAVE_FILE PLUGIN_GAMES_DATA_DIR "/runepod.save"
#define RP_SMOKE_LOG PLUGIN_GAMES_DATA_DIR "/runepod-smoke.log"
#define RP_SPRITES_PATH PLUGIN_GAMES_DATA_DIR "/runepod/sprites/runepod_sprites.320x160x24.bmp"
#define RP_PLAYER_DIRS_PATH PLUGIN_GAMES_DATA_DIR "/runepod/sprites/runepod_player_dirs.384x32x24.bmp"
#define RP_TERRAIN_PATH PLUGIN_GAMES_DATA_DIR "/runepod/tiles/runepod_terrain_tiles.256x32x24.bmp"
#define RP_SPRITE_W 32
#define RP_SPRITE_H 32
#define RP_SPRITE_SHEET_W 320
#define RP_SPRITE_SHEET_H 160
#define RP_SPRITE_PIXELS (RP_SPRITE_SHEET_W * RP_SPRITE_SHEET_H)
#define RP_SPRITE_BYTES (RP_SPRITE_PIXELS * (int)sizeof(fb_data))
#define RP_PLAYER_WALK_FRAMES 3
#define RP_PLAYER_DIR_SHEET_W (RP_SPRITE_W * 4 * RP_PLAYER_WALK_FRAMES)
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
    RP_SPR_COMBAT_ICON,
    RP_SPR_CITY_GATE = 20,
    RP_SPR_SMITH,
    RP_SPR_INN,
    RP_SPR_HEALER,
    RP_SPR_SLIME,
    RP_SPR_BANDIT,
    RP_SPR_BAT,
    RP_SPR_MARKET,
    RP_SPR_ANVIL,
    RP_SPR_SHIELD,
    RP_SPR_MARKET_COUNTER = 30,
    RP_SPR_FORGE,
    RP_SPR_INN_BED,
    RP_SPR_INN_TABLE,
    RP_SPR_HEALER_SHRINE,
    RP_SPR_CITY_WELL,
    RP_SPR_SIGNPOST,
    RP_SPR_CRATE,
    RP_SPR_BOOKSHELF,
    RP_SPR_CHEST,
    RP_SPR_GUARD = 40,
    RP_SPR_BAKER,
    RP_SPR_TRAINER,
    RP_SPR_DUMMY,
    RP_SPR_HERB_BED,
    RP_SPR_BAKERY,
    RP_SPR_BANK_CHEST,
    RP_SPR_SKELETON,
    RP_SPR_WOLF,
    RP_SPR_SEWER_RAT
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
    RP_ACTIVITY_EAT,
    RP_ACTIVITY_PICK,
    RP_ACTIVITY_TRAIN,
    RP_ACTIVITY_PRAY,
    RP_ACTIVITY_BUY
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
    bool save_dirty;
    bool save_loaded;
    long next_autosave_tick;
    bool quit;
};

static const struct rp_target rp_targets[RP_MAX_TARGETS] =
{
    { "Guide", "Talk", RP_TARGET_NPC, RP_GROUP_NPC, 286, 178 },
    { "Shop", "Trade", RP_TARGET_NPC, RP_GROUP_NPC, 430, 148 },
    { "Old Druid", "Talk", RP_TARGET_NPC, RP_GROUP_NPC, 728, 188 },
    { "Oak", "Chop", RP_TARGET_TREE, RP_GROUP_RESOURCE, 164, 306 },
    { "Pine", "Chop", RP_TARGET_TREE, RP_GROUP_RESOURCE, 168, 512 },
    { "Copper", "Mine", RP_TARGET_ROCK, RP_GROUP_RESOURCE, 486, 332 },
    { "Iron", "Mine", RP_TARGET_ROCK, RP_GROUP_RESOURCE, 786, 448 },
    { "Pond", "Fish", RP_TARGET_FISH, RP_GROUP_RESOURCE, 526, 234 },
    { "Creek", "Fish", RP_TARGET_FISH, RP_GROUP_RESOURCE, 742, 308 },
    { "Fire", "Cook", RP_TARGET_FIRE, RP_GROUP_CRAFT, 332, 246 },
    { "Workbench", "Craft", RP_TARGET_BENCH, RP_GROUP_CRAFT, 366, 172 },
    { "Ratling", "Attack", RP_TARGET_ENEMY, RP_GROUP_COMBAT, 842, 534 },
    { "City Gate", "Enter", RP_TARGET_EXIT, RP_GROUP_EXIT, 610, 198 },
    { "Market", "Trade", RP_TARGET_NPC, RP_GROUP_NPC, 650, 150 },
    { "Smith", "Forge", RP_TARGET_NPC, RP_GROUP_NPC, 712, 150 },
    { "Inn", "Rest", RP_TARGET_NPC, RP_GROUP_NPC, 766, 184 },
    { "Healer", "Heal", RP_TARGET_NPC, RP_GROUP_NPC, 724, 232 },
    { "Slime", "Attack", RP_TARGET_ENEMY, RP_GROUP_COMBAT, 566, 438 },
    { "Bandit", "Attack", RP_TARGET_ENEMY, RP_GROUP_COMBAT, 696, 542 },
    { "Cave Bat", "Attack", RP_TARGET_ENEMY, RP_GROUP_COMBAT, 888, 470 },
    { "Market Counter", "Buy", RP_TARGET_NPC, RP_GROUP_NPC, 650, 184 },
    { "Forge", "Forge", RP_TARGET_BENCH, RP_GROUP_CRAFT, 690, 184 },
    { "Anvil", "Craft", RP_TARGET_BENCH, RP_GROUP_CRAFT, 722, 184 },
    { "Inn Bed", "Rest", RP_TARGET_NPC, RP_GROUP_NPC, 780, 214 },
    { "Inn Table", "Eat", RP_TARGET_NPC, RP_GROUP_NPC, 748, 214 },
    { "Healing Shrine", "Pray", RP_TARGET_NPC, RP_GROUP_NPC, 710, 246 },
    { "City Well", "Draw", RP_TARGET_NPC, RP_GROUP_NPC, 662, 236 },
    { "Notice Board", "Read", RP_TARGET_NPC, RP_GROUP_NPC, 610, 166 },
    { "Guard", "Talk", RP_TARGET_NPC, RP_GROUP_NPC, 608, 224 },
    { "Bakery", "Buy", RP_TARGET_NPC, RP_GROUP_NPC, 792, 144 },
    { "Trainer", "Talk", RP_TARGET_NPC, RP_GROUP_NPC, 628, 286 },
    { "Training Dummy", "Train", RP_TARGET_BENCH, RP_GROUP_COMBAT, 658, 286 },
    { "Herb Bed", "Pick", RP_TARGET_NPC, RP_GROUP_RESOURCE, 812, 236 },
    { "Bank Chest", "Sort", RP_TARGET_NPC, RP_GROUP_NPC, 628, 246 },
    { "Skeleton", "Attack", RP_TARGET_ENEMY, RP_GROUP_COMBAT, 906, 558 },
    { "Wolf", "Attack", RP_TARGET_ENEMY, RP_GROUP_COMBAT, 226, 566 },
};

enum rp_target_index
{
    RP_IDX_GUIDE = 0,
    RP_IDX_SHOP = 1,
    RP_IDX_DRUID = 2,
    RP_IDX_RATLING = 11,
    RP_IDX_CITY_GATE = 12,
    RP_IDX_MARKET = 13,
    RP_IDX_SMITH = 14,
    RP_IDX_INN = 15,
    RP_IDX_HEALER = 16,
    RP_IDX_SLIME = 17,
    RP_IDX_BANDIT = 18,
    RP_IDX_BAT = 19,
    RP_IDX_MARKET_COUNTER = 20,
    RP_IDX_FORGE = 21,
    RP_IDX_ANVIL = 22,
    RP_IDX_INN_BED = 23,
    RP_IDX_INN_TABLE = 24,
    RP_IDX_SHRINE = 25,
    RP_IDX_WELL = 26,
    RP_IDX_NOTICE = 27,
    RP_IDX_GUARD = 28,
    RP_IDX_BAKERY = 29,
    RP_IDX_TRAINER = 30,
    RP_IDX_DUMMY = 31,
    RP_IDX_HERB_BED = 32,
    RP_IDX_BANK_CHEST = 33,
    RP_IDX_SKELETON = 34,
    RP_IDX_WOLF = 35
};

struct rp_save_data
{
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    int32_t player_x;
    int32_t player_y;
    int32_t dest_x;
    int32_t dest_y;
    int32_t cursor_x;
    int32_t cursor_y;
    int32_t player_dir;
    int32_t selected;
    struct rp_inventory inv;
    struct rp_skills xp;
    int32_t hp;
    int32_t enemy_hp;
    int32_t quest_stage;
    int32_t cooldown_remaining[RP_MAX_TARGETS];
};

struct rp_save_data_v1
{
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    int32_t player_x;
    int32_t player_y;
    int32_t dest_x;
    int32_t dest_y;
    int32_t cursor_x;
    int32_t cursor_y;
    int32_t player_dir;
    int32_t selected;
    struct rp_inventory inv;
    struct rp_skills xp;
    int32_t hp;
    int32_t enemy_hp;
    int32_t quest_stage;
    int32_t cooldown_remaining[RP_SAVE_V1_TARGETS];
};

struct rp_save_data_v2
{
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    int32_t player_x;
    int32_t player_y;
    int32_t dest_x;
    int32_t dest_y;
    int32_t cursor_x;
    int32_t cursor_y;
    int32_t player_dir;
    int32_t selected;
    struct rp_inventory inv;
    struct rp_skills xp;
    int32_t hp;
    int32_t enemy_hp;
    int32_t quest_stage;
    int32_t cooldown_remaining[RP_SAVE_V2_TARGETS];
};

struct rp_save_data_v3
{
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    int32_t player_x;
    int32_t player_y;
    int32_t dest_x;
    int32_t dest_y;
    int32_t cursor_x;
    int32_t cursor_y;
    int32_t player_dir;
    int32_t selected;
    struct rp_inventory inv;
    struct rp_skills xp;
    int32_t hp;
    int32_t enemy_hp;
    int32_t quest_stage;
    int32_t cooldown_remaining[RP_SAVE_V3_TARGETS];
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

static const int rp_level_xp[] =
{
    0, 30, 75, 140, 230, 350, 510, 720, 995, 1350,
    1810, 2400, 3150, 4100, 5300, 6800, 8700, 11100, 14100, 17800
};

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
    rp_smoke_log("music_disabled_playlist_safety", 0);
    return false;
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

static void rp_update_cursor_selection(void);
static void rp_update_camera(void);
static void rp_set_message(const char *line1, const char *line2);

static uint32_t rp_checksum_bytes(unsigned char *bytes, size_t size,
                                  uint32_t *checksum_field)
{
    uint32_t checksum = 2166136261u;
    uint32_t old_checksum = *checksum_field;
    size_t i;

    *checksum_field = 0;
    for (i = 0; i < size; i++)
    {
        checksum ^= bytes[i];
        checksum *= 16777619u;
    }
    *checksum_field = old_checksum;
    return checksum;
}

static uint32_t rp_save_checksum(struct rp_save_data *save)
{
    return rp_checksum_bytes((unsigned char *)save, sizeof(*save),
                             &save->checksum);
}

static uint32_t rp_save_checksum_v1(struct rp_save_data_v1 *save)
{
    return rp_checksum_bytes((unsigned char *)save, sizeof(*save),
                             &save->checksum);
}

static uint32_t rp_save_checksum_v2(struct rp_save_data_v2 *save)
{
    return rp_checksum_bytes((unsigned char *)save, sizeof(*save),
                             &save->checksum);
}

static uint32_t rp_save_checksum_v3(struct rp_save_data_v3 *save)
{
    return rp_checksum_bytes((unsigned char *)save, sizeof(*save),
                             &save->checksum);
}

static void rp_mark_dirty(void)
{
    game.save_dirty = true;
    if (game.next_autosave_tick == 0)
        game.next_autosave_tick = *rb->current_tick + RP_AUTOSAVE_TICKS;
}

static void rp_fill_save(struct rp_save_data *save)
{
    int i;

    rb->memset(save, 0, sizeof(*save));
    save->magic = RP_SAVE_MAGIC;
    save->version = RP_SAVE_VERSION;
    save->player_x = game.player_x;
    save->player_y = game.player_y;
    save->dest_x = game.dest_x;
    save->dest_y = game.dest_y;
    save->cursor_x = game.cursor_x;
    save->cursor_y = game.cursor_y;
    save->player_dir = game.player_dir;
    save->selected = game.selected;
    save->inv = game.inv;
    save->xp = game.xp;
    save->hp = game.hp;
    save->enemy_hp = game.enemy_hp;
    save->quest_stage = game.quest_stage;

    for (i = 0; i < RP_MAX_TARGETS; i++)
    {
        if (TIME_AFTER(game.cooldown_until[i], *rb->current_tick))
            save->cooldown_remaining[i] =
                game.cooldown_until[i] - *rb->current_tick;
    }

    save->checksum = rp_save_checksum(save);
}

static bool rp_save_game(void)
{
    struct rp_save_data save;
    int fd;
    ssize_t written;

    rb->mkdir(PLUGIN_GAMES_DATA_DIR);
    rp_fill_save(&save);
    fd = rb->open(RP_SAVE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    written = rb->write(fd, &save, sizeof(save));
    rb->close(fd);

    if (written == (ssize_t)sizeof(save))
    {
        game.save_dirty = false;
        game.next_autosave_tick = 0;
        rp_smoke_log("save_ok", (int)sizeof(save));
        return true;
    }

    rp_smoke_log("save_failed", (int)written);
    return false;
}

static bool rp_load_game(void)
{
    struct rp_save_data save;
    struct rp_save_data_v1 save_v1;
    struct rp_save_data_v2 save_v2;
    struct rp_save_data_v3 save_v3;
    uint32_t checksum;
    int fd;
    ssize_t read_bytes;
    int i;
    bool migrated = false;

    fd = rb->open(RP_SAVE_FILE, O_RDONLY);
    if (fd < 0)
        return false;

    read_bytes = rb->read(fd, &save, sizeof(save));
    rb->close(fd);

    if (read_bytes == (ssize_t)sizeof(save))
    {
        checksum = save.checksum;
        if (save.magic != RP_SAVE_MAGIC ||
            save.version != RP_SAVE_VERSION ||
            checksum != rp_save_checksum(&save))
        {
            return false;
        }
    }
    else if (read_bytes == (ssize_t)sizeof(save_v3))
    {
        rb->memcpy(&save_v3, &save, sizeof(save_v3));
        checksum = save_v3.checksum;
        if (save_v3.magic != RP_SAVE_MAGIC ||
            save_v3.version != RP_SAVE_VERSION_V3 ||
            checksum != rp_save_checksum_v3(&save_v3))
        {
            return false;
        }
        rb->memset(&save, 0, sizeof(save));
        save.magic = RP_SAVE_MAGIC;
        save.version = RP_SAVE_VERSION;
        save.player_x = save_v3.player_x;
        save.player_y = save_v3.player_y;
        save.dest_x = save_v3.dest_x;
        save.dest_y = save_v3.dest_y;
        save.cursor_x = save_v3.cursor_x;
        save.cursor_y = save_v3.cursor_y;
        save.player_dir = save_v3.player_dir;
        save.selected = save_v3.selected;
        save.inv = save_v3.inv;
        save.xp = save_v3.xp;
        save.hp = save_v3.hp;
        save.enemy_hp = save_v3.enemy_hp;
        save.quest_stage = save_v3.quest_stage;
        rb->memcpy(save.cooldown_remaining, save_v3.cooldown_remaining,
                   sizeof(save_v3.cooldown_remaining));
        migrated = true;
    }
    else if (read_bytes == (ssize_t)sizeof(save_v2))
    {
        rb->memcpy(&save_v2, &save, sizeof(save_v2));
        checksum = save_v2.checksum;
        if (save_v2.magic != RP_SAVE_MAGIC ||
            save_v2.version != RP_SAVE_VERSION_V2 ||
            checksum != rp_save_checksum_v2(&save_v2))
        {
            return false;
        }
        rb->memset(&save, 0, sizeof(save));
        save.magic = RP_SAVE_MAGIC;
        save.version = RP_SAVE_VERSION;
        save.player_x = save_v2.player_x;
        save.player_y = save_v2.player_y;
        save.dest_x = save_v2.dest_x;
        save.dest_y = save_v2.dest_y;
        save.cursor_x = save_v2.cursor_x;
        save.cursor_y = save_v2.cursor_y;
        save.player_dir = save_v2.player_dir;
        save.selected = save_v2.selected;
        save.inv = save_v2.inv;
        save.xp = save_v2.xp;
        save.hp = save_v2.hp;
        save.enemy_hp = save_v2.enemy_hp;
        save.quest_stage = save_v2.quest_stage;
        rb->memcpy(save.cooldown_remaining, save_v2.cooldown_remaining,
                   sizeof(save_v2.cooldown_remaining));
        migrated = true;
    }
    else if (read_bytes == (ssize_t)sizeof(save_v1))
    {
        rb->memcpy(&save_v1, &save, sizeof(save_v1));
        checksum = save_v1.checksum;
        if (save_v1.magic != RP_SAVE_MAGIC ||
            save_v1.version != RP_SAVE_VERSION_V1 ||
            checksum != rp_save_checksum_v1(&save_v1))
        {
            return false;
        }
        rb->memset(&save, 0, sizeof(save));
        save.magic = RP_SAVE_MAGIC;
        save.version = RP_SAVE_VERSION;
        save.player_x = save_v1.player_x;
        save.player_y = save_v1.player_y;
        save.dest_x = save_v1.dest_x;
        save.dest_y = save_v1.dest_y;
        save.cursor_x = save_v1.cursor_x;
        save.cursor_y = save_v1.cursor_y;
        save.player_dir = save_v1.player_dir;
        save.selected = save_v1.selected;
        save.inv = save_v1.inv;
        save.xp = save_v1.xp;
        save.hp = save_v1.hp;
        save.enemy_hp = save_v1.enemy_hp;
        save.quest_stage = save_v1.quest_stage;
        rb->memcpy(save.cooldown_remaining, save_v1.cooldown_remaining,
                   sizeof(save_v1.cooldown_remaining));
        migrated = true;
    }
    else
    {
        return false;
    }

    game.player_x = rp_clamp_int(save.player_x, 8, RP_WORLD_W - 9);
    game.player_y = rp_clamp_int(save.player_y, 8, RP_WORLD_H - 9);
    game.dest_x = rp_clamp_int(save.dest_x, 8, RP_WORLD_W - 9);
    game.dest_y = rp_clamp_int(save.dest_y, 8, RP_WORLD_H - 9);
    game.cursor_x = rp_clamp_int(save.cursor_x, 8, RP_WORLD_W - 9);
    game.cursor_y = rp_clamp_int(save.cursor_y, 8, RP_WORLD_H - 9);
    game.player_dir = rp_clamp_int(save.player_dir, RP_DIR_SOUTH, RP_DIR_WEST);
    game.selected = rp_clamp_int(save.selected, -1, RP_MAX_TARGETS - 1);
    game.inv = save.inv;
    game.xp = save.xp;
    game.hp = rp_clamp_int(save.hp, 1, 10);
    game.enemy_hp = rp_clamp_int(save.enemy_hp, 1, 12);
    game.quest_stage = rp_clamp_int(save.quest_stage, 0, 5);
    game.moving = false;
    game.pending_action = false;
    game.activity = RP_ACTIVITY_NONE;
    game.activity_target = -1;
    for (i = 0; i < RP_MAX_TARGETS; i++)
    {
        if (save.cooldown_remaining[i] > 0)
            game.cooldown_until[i] =
                *rb->current_tick + save.cooldown_remaining[i];
    }
    rp_update_cursor_selection();
    rp_update_camera();
    game.save_dirty = migrated;
    game.save_loaded = true;
    game.next_autosave_tick = migrated ? *rb->current_tick + HZ : 0;
    rp_set_message("Save loaded", "Select resumes your quest");
    rp_smoke_log("load_ok", (int)read_bytes);
    return true;
}

static void rp_autosave_if_needed(void)
{
    if (game.save_dirty &&
        game.next_autosave_tick != 0 &&
        TIME_AFTER(*rb->current_tick, game.next_autosave_tick))
    {
        if (!rp_save_game())
            game.next_autosave_tick = *rb->current_tick + RP_AUTOSAVE_TICKS;
    }
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

static int rp_skill_level_from_kind(enum rp_target_kind kind)
{
    int xp;
    int i;

    switch (kind)
    {
        case RP_TARGET_TREE: xp = game.xp.woodcutting; break;
        case RP_TARGET_ROCK: xp = game.xp.mining; break;
        case RP_TARGET_FISH: xp = game.xp.fishing; break;
        case RP_TARGET_FIRE: xp = game.xp.cooking; break;
        case RP_TARGET_BENCH: xp = game.xp.crafting; break;
        case RP_TARGET_ENEMY: xp = game.xp.combat; break;
        default: xp = 0; break;
    }

    for (i = (int)ARRAYLEN(rp_level_xp) - 1; i > 0; i--)
    {
        if (xp >= rp_level_xp[i])
            return i + 1;
    }

    return 1;
}

static int rp_level_bonus(enum rp_target_kind kind)
{
    return rp_clamp_int((rp_skill_level_from_kind(kind) - 1) / 2, 0, 3);
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
        rp_set_message("Guide: Seek the druid",
                       "The eastern cave needs a charm");
    }
}

static void rp_talk_druid(void)
{
    int total_level = rp_skill_level_from_kind(RP_TARGET_TREE) +
                      rp_skill_level_from_kind(RP_TARGET_ROCK) +
                      rp_skill_level_from_kind(RP_TARGET_FISH) +
                      rp_skill_level_from_kind(RP_TARGET_ENEMY);

    if (game.quest_stage < 2)
    {
        rp_set_message("Druid: Earn trust first",
                       "Help the guide in the village");
    }
    else if (game.quest_stage == 2 && game.inv.charms > 0 &&
             total_level >= 6)
    {
        game.quest_stage = 3;
        rp_set_message("Druid: Cave trial opened",
                       "Defeat the ratling beyond the ridge");
    }
    else if (game.quest_stage == 2 && game.inv.charms > 0)
    {
        rp_set_message("Druid: Train a little more",
                       "Reach total field level 6");
    }
    else if (game.quest_stage == 3)
    {
        rp_set_message("Druid: Follow the ridge",
                       "The ratling guards the cave");
    }
    else if (game.quest_stage >= 4)
    {
        rp_set_message("Druid: The valley is safer",
                       "Keep training for deeper paths");
    }
    else
    {
        rp_set_message("Druid: Gather a charm",
                       "The guide rewards prepared travelers");
    }
}

static int rp_enemy_max_hp(int target_index)
{
    switch (target_index)
    {
        case RP_IDX_SLIME: return 4;
        case RP_IDX_BANDIT: return 8;
        case RP_IDX_BAT: return 7;
        case RP_IDX_SKELETON: return 12;
        case RP_IDX_WOLF: return 9;
        case RP_IDX_RATLING:
        default:
            return game.quest_stage >= 4 ? 10 : 6;
    }
}

static int rp_enemy_coin_reward(int target_index)
{
    switch (target_index)
    {
        case RP_IDX_SLIME: return 1;
        case RP_IDX_BANDIT: return 5;
        case RP_IDX_BAT: return 3;
        case RP_IDX_SKELETON: return 7;
        case RP_IDX_WOLF: return 4;
        case RP_IDX_RATLING: return game.quest_stage == 3 ? 8 : 2;
        default: return 2;
    }
}

static const char *rp_enemy_defeat_message(int target_index)
{
    switch (target_index)
    {
        case RP_IDX_SLIME: return "Slime split apart";
        case RP_IDX_BANDIT: return "Bandit driven off";
        case RP_IDX_BAT: return "Cave bat scattered";
        case RP_IDX_SKELETON: return "Skeleton collapses";
        case RP_IDX_WOLF: return "Wolf flees the woods";
        default: return "Ratling defeated";
    }
}

static const char *rp_enemy_hit_message(int target_index)
{
    switch (target_index)
    {
        case RP_IDX_SLIME: return "You strike the slime";
        case RP_IDX_BANDIT: return "You strike the bandit";
        case RP_IDX_BAT: return "You strike the bat";
        case RP_IDX_SKELETON: return "You crack the skeleton";
        case RP_IDX_WOLF: return "You fend off the wolf";
        default: return "You strike the ratling";
    }
}

static const char *rp_enemy_counter_message(int target_index)
{
    switch (target_index)
    {
        case RP_IDX_SLIME: return "It splashes back";
        case RP_IDX_BANDIT: return "It cuts back";
        case RP_IDX_BAT: return "It dives back";
        case RP_IDX_SKELETON: return "It rattles back";
        case RP_IDX_WOLF: return "It snaps back";
        default: return "It claws back";
    }
}

static const char *rp_enemy_reward_message(int target_index)
{
    switch (target_index)
    {
        case RP_IDX_BANDIT: return "+5 coins, +4 combat XP";
        case RP_IDX_SKELETON: return "+7 coins, +4 combat XP";
        case RP_IDX_WOLF: return "+4 coins, +4 combat XP";
        default: return "+coins, +4 combat XP";
    }
}

static void rp_city_gate(void)
{
    rp_set_message("City gate",
                   "Market, smith, inn, healer ahead");
}

static void rp_trade_market(void)
{
    if (game.inv.coins >= 5)
    {
        game.inv.coins -= 5;
        game.inv.food += 2;
        rp_start_activity(RP_ACTIVITY_BUY, game.selected, HZ / 2);
        rp_set_message("Market bundle",
                       "-5 coins, +2 food");
    }
    else
    {
        rp_set_message("Market",
                       "2 food costs 5 coins");
    }
}

static void rp_read_notice(void)
{
    rp_set_message("Notice board",
                   "Slimes pay small, bandits pay well");
}

static void rp_talk_guard(void)
{
    if (game.quest_stage < 4)
    {
        rp_set_message("Guard",
                       "Clear the cave trial first");
    }
    else if (game.quest_stage == 4)
    {
        game.quest_stage = 5;
        rp_set_message("Guard: crypt bounty",
                       "Skeletons stir beyond the ridge");
    }
    else
    {
        rp_set_message("Guard",
                       "Keep the roads clear");
    }
}

static void rp_buy_bakery(void)
{
    if (game.inv.coins >= 7)
    {
        game.inv.coins -= 7;
        game.inv.food += 3;
        rp_start_activity(RP_ACTIVITY_BUY, game.selected, HZ / 2);
        rp_set_message("Bakery parcel",
                       "-7 coins, +3 food");
    }
    else
    {
        rp_set_message("Bakery",
                       "3 food costs 7 coins");
    }
}

static void rp_talk_trainer(void)
{
    rp_set_message("Trainer",
                   "Use the dummy for slow combat XP");
}

static void rp_draw_well(void)
{
    game.hp = MIN(10, game.hp + 1);
    rp_start_activity(RP_ACTIVITY_PRAY, game.selected, HZ / 2);
    rp_set_message("Cool well water",
                   "Recovered a little health");
}

static void rp_forge_smith(void)
{
    if (game.inv.ore >= 4 && game.inv.logs >= 2 && game.inv.coins >= 4)
    {
        game.inv.ore -= 4;
        game.inv.logs -= 2;
        game.inv.coins -= 4;
        game.inv.charms++;
        rp_add_xp(RP_TARGET_BENCH, 8);
        rp_start_activity(RP_ACTIVITY_CRAFT, game.selected, HZ);
        rp_set_message("Smith forges ward charm",
                       "-4 ore, -2 logs, -4 coins");
    }
    else
    {
        rp_set_message("Smith",
                       "Needs 4 ore, 2 logs, 4 coins");
    }
}

static void rp_train_dummy(void)
{
    game.xp.combat += 2;
    game.cooldown_until[game.selected] = *rb->current_tick + HZ * 2;
    rp_start_activity(RP_ACTIVITY_TRAIN, game.selected, HZ * 2 / 3);
    rp_set_message("You drill footwork",
                   "+2 combat XP");
}

static void rp_pick_herbs(void)
{
    game.inv.food += 1;
    game.xp.cooking += 1;
    game.cooldown_until[game.selected] = *rb->current_tick + HZ * 4;
    rp_start_activity(RP_ACTIVITY_PICK, game.selected, HZ * 3 / 4);
    rp_set_message("You pick kitchen herbs",
                   "+1 food, +1 cooking XP");
}

static void rp_sort_bank_chest(void)
{
    if (game.inv.logs >= 5)
    {
        game.inv.logs -= 5;
        game.inv.coins += 3;
        rp_start_activity(RP_ACTIVITY_BUY, game.selected, HZ / 2);
        rp_set_message("Packed timber crate",
                       "-5 logs, +3 coins");
    }
    else if (game.inv.ore >= 5)
    {
        game.inv.ore -= 5;
        game.inv.coins += 4;
        rp_start_activity(RP_ACTIVITY_BUY, game.selected, HZ / 2);
        rp_set_message("Packed ore crate",
                       "-5 ore, +4 coins");
    }
    else
    {
        rp_set_message("Bank chest",
                       "Sorts 5 logs or 5 ore");
    }
}

static void rp_use_anvil(void)
{
    if (game.inv.ore >= 2 && game.inv.logs >= 1)
    {
        game.inv.ore -= 2;
        game.inv.logs -= 1;
        game.inv.coins += 3;
        rp_add_xp(RP_TARGET_BENCH, 3);
        rp_start_activity(RP_ACTIVITY_CRAFT, game.selected, HZ * 3 / 4);
        rp_set_message("You shape fittings",
                       "-2 ore, -1 log, +3 coins");
    }
    else
    {
        rp_set_message("Anvil",
                       "Needs 2 ore and 1 log");
    }
}

static void rp_rest_inn(void)
{
    if (game.inv.coins >= 2)
    {
        game.inv.coins -= 2;
        game.hp = 10;
        game.enemy_hp = rp_enemy_max_hp(game.selected);
        rp_save_game();
        rp_set_message("Inn rest complete",
                       "Health restored and saved");
    }
    else
    {
        rp_set_message("Inn",
                       "A bed costs 2 coins");
    }
}

static void rp_eat_at_table(void)
{
    if (game.inv.food > 0 && game.hp < 10)
    {
        game.inv.food--;
        game.hp = MIN(10, game.hp + 5);
        rp_start_activity(RP_ACTIVITY_EAT, game.selected, HZ / 2);
        rp_set_message("You eat at the inn table",
                       "-1 food, health recovered");
    }
    else if (game.hp >= 10)
    {
        rp_set_message("Inn table",
                       "You are already full");
    }
    else
    {
        rp_set_message("Inn table",
                       "Bring food from the market");
    }
}

static void rp_heal_service(void)
{
    if (game.hp >= 10)
    {
        rp_set_message("Healer",
                       "You are already healthy");
    }
    else if (game.inv.coins >= 3)
    {
        game.inv.coins -= 3;
        game.hp = 10;
        rp_set_message("Healer restores you",
                       "-3 coins, health full");
    }
    else
    {
        rp_set_message("Healer",
                       "Healing costs 3 coins");
    }
}

static void rp_pray_shrine(void)
{
    if (game.inv.charms > 0)
    {
        game.hp = 10;
        rp_start_activity(RP_ACTIVITY_PRAY, game.selected, HZ / 2);
        rp_set_message("Shrine hums softly",
                       "Charm wards restore health");
    }
    else
    {
        rp_set_message("Healing shrine",
                       "A charm would focus it");
    }
}

static void rp_trade_shop(void)
{
    if (game.inv.coins >= 3)
    {
        game.inv.coins -= 3;
        game.inv.food += 1;
        rp_start_activity(RP_ACTIVITY_BUY, game.selected, HZ / 2);
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
            if (game.selected == RP_IDX_GUIDE)
                rp_talk_guide();
            else if (game.selected == RP_IDX_DRUID)
                rp_talk_druid();
            else if (game.selected == RP_IDX_MARKET)
                rp_trade_market();
            else if (game.selected == RP_IDX_MARKET_COUNTER)
                rp_trade_market();
            else if (game.selected == RP_IDX_SMITH)
                rp_forge_smith();
            else if (game.selected == RP_IDX_INN)
                rp_rest_inn();
            else if (game.selected == RP_IDX_INN_BED)
                rp_rest_inn();
            else if (game.selected == RP_IDX_INN_TABLE)
                rp_eat_at_table();
            else if (game.selected == RP_IDX_HEALER)
                rp_heal_service();
            else if (game.selected == RP_IDX_SHRINE)
                rp_pray_shrine();
            else if (game.selected == RP_IDX_WELL)
                rp_draw_well();
            else if (game.selected == RP_IDX_NOTICE)
                rp_read_notice();
            else if (game.selected == RP_IDX_GUARD)
                rp_talk_guard();
            else if (game.selected == RP_IDX_BAKERY)
                rp_buy_bakery();
            else if (game.selected == RP_IDX_TRAINER)
                rp_talk_trainer();
            else if (game.selected == RP_IDX_HERB_BED)
                rp_pick_herbs();
            else if (game.selected == RP_IDX_BANK_CHEST)
                rp_sort_bank_chest();
            else
                rp_trade_shop();
            break;

        case RP_TARGET_TREE:
        {
            int gained = 1 + rp_level_bonus(target->kind);
            if (game.selected == 4)
                gained++;
            game.inv.logs += gained;
            rp_add_xp(target->kind, 2);
            game.cooldown_until[game.selected] = now + HZ * 3;
            rp_start_activity(RP_ACTIVITY_CHOP, game.selected, HZ * 3 / 4);
            rp_set_message(game.selected == 4 ? "You chop resin pine"
                                              : "You chop the oak",
                           gained > 1 ? "Skill bonus: extra logs"
                                      : "+1 log, +2 woodcutting XP");
            break;
        }

        case RP_TARGET_ROCK:
        {
            int gained = 1 + rp_level_bonus(target->kind);
            if (game.selected == 6)
                gained++;
            game.inv.ore += gained;
            rp_add_xp(target->kind, 2);
            game.cooldown_until[game.selected] = now + HZ * 3;
            rp_start_activity(RP_ACTIVITY_MINE, game.selected, HZ * 3 / 4);
            rp_set_message(game.selected == 6 ? "You mine ironstone"
                                              : "You mine copper",
                           gained > 1 ? "Skill bonus: extra ore"
                                      : "+1 ore, +2 mining XP");
            break;
        }

        case RP_TARGET_FISH:
        {
            int gained = 1 + rp_level_bonus(target->kind);
            if (game.selected == 8)
                gained++;
            game.inv.raw_fish += gained;
            rp_add_xp(target->kind, 2);
            game.cooldown_until[game.selected] = now + HZ * 2;
            rp_start_activity(RP_ACTIVITY_FISH, game.selected, HZ);
            rp_set_message(game.selected == 8 ? "You net creek trout"
                                              : "You catch a fish",
                           gained > 1 ? "Skill bonus: extra fish"
                                      : "+1 raw fish, +2 fishing XP");
            break;
        }

        case RP_TARGET_FIRE:
            if (game.inv.raw_fish > 0)
            {
                game.inv.raw_fish--;
                game.inv.food++;
                rp_add_xp(target->kind, 2);
                rp_start_activity(RP_ACTIVITY_COOK, game.selected, HZ * 3 / 4);
                rp_set_message("The fish cooks cleanly",
                               "+1 food, +2 cooking XP");
            }
            else
            {
                rp_set_message("Nothing to cook",
                               "Catch fish at the pond");
            }
            break;

        case RP_TARGET_BENCH:
            if (game.selected == RP_IDX_DUMMY)
            {
                rp_train_dummy();
                break;
            }
            if (game.selected == RP_IDX_FORGE)
            {
                rp_forge_smith();
                break;
            }
            if (game.selected == RP_IDX_ANVIL)
            {
                rp_use_anvil();
                break;
            }
            if (game.inv.logs >= 2 && game.inv.ore >= 1)
            {
                game.inv.logs -= 2;
                game.inv.ore -= 1;
                game.inv.coins += 4;
                rp_add_xp(target->kind, 3);
                rp_start_activity(RP_ACTIVITY_CRAFT, game.selected, HZ * 3 / 4);
                rp_set_message("You craft a tool haft",
                               "+4 coins, +3 crafting XP");
            }
            else
            {
                rp_set_message("Workbench",
                               "Needs 2 logs and 1 ore");
            }
            break;

        case RP_TARGET_ENEMY:
        {
            int damage;
            int enemy_max = rp_enemy_max_hp(game.selected);

            if (game.selected == RP_IDX_RATLING && game.quest_stage < 3)
            {
                rp_set_message("Cave ward holds",
                               "Ask the druid about the charm");
                break;
            }

            if (game.selected == RP_IDX_BAT && game.quest_stage < 4)
            {
                rp_set_message("Bat roost is too dark",
                               "Clear the ratling trial first");
                break;
            }

            if (game.selected == RP_IDX_SKELETON && game.quest_stage < 5)
            {
                rp_set_message("Crypt gate is watched",
                               "Ask the city guard first");
                break;
            }

            if (game.enemy_hp > enemy_max)
                game.enemy_hp = enemy_max;

            rp_start_activity(RP_ACTIVITY_FIGHT, game.selected, HZ * 2 / 3);
            damage = 2 + (rb->rand() % 2) + rp_level_bonus(target->kind);
            if (game.inv.charms > 0)
                damage++;
            game.enemy_hp -= damage;
            if (game.enemy_hp <= 0)
            {
                game.inv.coins += rp_enemy_coin_reward(game.selected);
                if (game.selected == RP_IDX_RATLING && game.quest_stage == 3)
                {
                    if (game.inv.charms > 0)
                        game.inv.charms--;
                    game.inv.food += 2;
                    game.quest_stage = 4;
                    rp_set_message("Cave trial cleared",
                                   "+8 coins, +2 food, valley safer");
                }
                else
                {
                    rp_set_message(rp_enemy_defeat_message(game.selected),
                                   rp_enemy_reward_message(game.selected));
                }
                game.enemy_hp = enemy_max;
                rp_add_xp(target->kind, 4);
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
                    rp_set_message(rp_enemy_hit_message(game.selected),
                                   rp_enemy_counter_message(game.selected));
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
        }

        case RP_TARGET_EXIT:
            rp_city_gate();
            break;
    }

    rp_mark_dirty();
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
            rp_mark_dirty();
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
        rp_mark_dirty();
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
    int i;

    for (i = (int)ARRAYLEN(rp_level_xp) - 1; i > 0; i--)
    {
        if (xp >= rp_level_xp[i])
            return i + 1;
    }

    return 1;
}

static int rp_skill_progress(int xp)
{
    int level = rp_skill_level(xp);
    int current = rp_level_xp[level - 1];
    int next;

    if (level >= (int)ARRAYLEN(rp_level_xp))
        return 100;

    next = rp_level_xp[level];
    return ((xp - current) * 100) / MAX(1, next - current);
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
    int frame = 1;

    if (!rp_player_dirs_loaded)
        return;

    if (game.moving || rp_activity_active())
        frame = (int)((*rb->current_tick / MAX(1, HZ / 8)) %
                      RP_PLAYER_WALK_FRAMES);

    sx = ((int)game.player_dir * RP_PLAYER_WALK_FRAMES + frame) *
         RP_SPRITE_W;
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
    if (world_x > 760 && world_y > 452)
        return RP_TILE_CAVE;
    if ((world_x > 485 && world_x < 584 && world_y > 185 && world_y < 285) ||
        (world_x > 700 && world_x < 800 && world_y > 262 && world_y < 360))
        return RP_TILE_WATER;
    if (rp_building_edge(world_x, world_y, 368, 96, 96, 96) ||
        rp_building_edge(world_x, world_y, 248, 128, 96, 80) ||
        rp_building_edge(world_x, world_y, 690, 128, 80, 80) ||
        rp_building_edge(world_x, world_y, 628, 104, 80, 112) ||
        rp_building_edge(world_x, world_y, 708, 104, 96, 80) ||
        rp_building_edge(world_x, world_y, 708, 184, 96, 88) ||
        rp_building_edge(world_x, world_y, 776, 104, 88, 72))
        return RP_TILE_VILLAGE;
    if (rp_in_rect(world_x, world_y, 400, 128, 32, 32) ||
        rp_in_rect(world_x, world_y, 280, 160, 32, 32) ||
        rp_in_rect(world_x, world_y, 344, 144, 48, 48) ||
        rp_in_rect(world_x, world_y, 716, 160, 32, 32) ||
        rp_in_rect(world_x, world_y, 660, 136, 32, 32) ||
        rp_in_rect(world_x, world_y, 728, 168, 32, 32) ||
        rp_in_rect(world_x, world_y, 648, 168, 40, 32) ||
        rp_in_rect(world_x, world_y, 736, 204, 56, 24) ||
        rp_in_rect(world_x, world_y, 620, 272, 52, 28) ||
        rp_in_rect(world_x, world_y, 792, 128, 40, 32))
        return RP_TILE_WOOD;
    if (rp_in_rect(world_x, world_y, 360, 176, 112, 64) ||
        rp_in_rect(world_x, world_y, 748, 400, 128, 72) ||
        rp_in_rect(world_x, world_y, 676, 168, 64, 32) ||
        rp_in_rect(world_x, world_y, 704, 232, 40, 32) ||
        rp_in_rect(world_x, world_y, 860, 524, 84, 68))
        return RP_TILE_STONE;
    if (rp_in_rect(world_x, world_y, 792, 216, 64, 44))
        return RP_TILE_DARK_GRASS;
    if (world_y > 188 && world_y < 234)
        return RP_TILE_PATH;
    if (world_x > 300 && world_x < 344)
        return RP_TILE_PATH;
    if (world_x > 622 && world_x < 666 && world_y > 180)
        return RP_TILE_PATH;
    if (world_x > 636 && world_x < 794 && world_y > 132 && world_y < 244)
        return RP_TILE_PATH;
    if (world_y > 496 && world_y < 538 && world_x > 620)
        return RP_TILE_PATH;
    if (world_x > 600 && world_x < 680 && world_y > 224 && world_y < 304)
        return RP_TILE_PATH;
    if (world_x > 776 && world_x < 864 && world_y > 132 && world_y < 188)
        return RP_TILE_PATH;
    if (world_x > 705 && world_y > 390)
        return RP_TILE_STONE;
    if (world_x < 230 && world_y > 390)
        return RP_TILE_DARK_GRASS;
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
        case RP_TARGET_BENCH:
            if (i == RP_IDX_FORGE)
                sprite = RP_SPR_FORGE;
            else if (i == RP_IDX_ANVIL)
                sprite = RP_SPR_ANVIL;
            else if (i == RP_IDX_DUMMY)
                sprite = RP_SPR_DUMMY;
            else
                sprite = RP_SPR_WORKBENCH;
            break;
        case RP_TARGET_ENEMY:
            if (i == RP_IDX_SLIME)
                sprite = RP_SPR_SLIME;
            else if (i == RP_IDX_BANDIT)
                sprite = RP_SPR_BANDIT;
            else if (i == RP_IDX_BAT)
                sprite = RP_SPR_BAT;
            else if (i == RP_IDX_SKELETON)
                sprite = RP_SPR_SKELETON;
            else if (i == RP_IDX_WOLF)
                sprite = RP_SPR_WOLF;
            else
                sprite = RP_SPR_RATLING;
            break;
        case RP_TARGET_EXIT: sprite = RP_SPR_CITY_GATE; break;
        case RP_TARGET_NPC:
        default:
            if (i == RP_IDX_MARKET_COUNTER)
                sprite = RP_SPR_MARKET_COUNTER;
            else if (i == RP_IDX_FORGE)
                sprite = RP_SPR_FORGE;
            else if (i == RP_IDX_ANVIL)
                sprite = RP_SPR_ANVIL;
            else if (i == RP_IDX_INN_BED)
                sprite = RP_SPR_INN_BED;
            else if (i == RP_IDX_INN_TABLE)
                sprite = RP_SPR_INN_TABLE;
            else if (i == RP_IDX_SHRINE)
                sprite = RP_SPR_HEALER_SHRINE;
            else if (i == RP_IDX_WELL)
                sprite = RP_SPR_CITY_WELL;
            else if (i == RP_IDX_NOTICE)
                sprite = RP_SPR_SIGNPOST;
            else if (i == RP_IDX_GUARD)
                sprite = RP_SPR_GUARD;
            else if (i == RP_IDX_BAKERY)
                sprite = RP_SPR_BAKERY;
            else if (i == RP_IDX_TRAINER)
                sprite = RP_SPR_TRAINER;
            else if (i == RP_IDX_HERB_BED)
                sprite = RP_SPR_HERB_BED;
            else if (i == RP_IDX_BANK_CHEST)
                sprite = RP_SPR_BANK_CHEST;
            else if (i == RP_IDX_SMITH)
                sprite = RP_SPR_SMITH;
            else if (i == RP_IDX_INN)
                sprite = RP_SPR_INN;
            else if (i == RP_IDX_HEALER)
                sprite = RP_SPR_HEALER;
            else if (i == RP_IDX_MARKET)
                sprite = RP_SPR_MARKET;
            else
                sprite = (i == RP_IDX_GUIDE || i == RP_IDX_DRUID)
                         ? RP_SPR_GUIDE : RP_SPR_SHOPKEEPER;
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
                        (i == 0 || i == 2) ? LCD_RGBPACK(86, 116, 74)
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

        case RP_ACTIVITY_PICK:
            rp_draw_line(LCD_RGBPACK(116, 190, 100), hand_x, hand_y,
                         tx - sign * phase, ty - 18 - phase);
            rp_fill(LCD_RGBPACK(116, 190, 100),
                    tx - 8 + phase * 2, ty - 24 - phase, 4, 4);
            rp_fill(RP_COL_ACCENT,
                    psx - 3 + phase, psy - 28 - phase, 5, 5);
            break;

        case RP_ACTIVITY_TRAIN:
        {
            int hit_x = tx - sign * (10 - phase * 2);
            int hit_y = ty - 17 + phase;

            rp_draw_action_tool(RP_COL_ACCENT, hand_x, hand_y, hit_x, hit_y);
            rp_rect(RP_COL_FOCUS, tx - 11, ty - 28, 22, 24);
            if (phase >= 1)
                rp_draw_sparks(tx, ty, phase);
            break;
        }

        case RP_ACTIVITY_PRAY:
            rp_draw_line(RP_COL_ACCENT, tx, ty - 30 - phase,
                         tx, ty - 7);
            rp_draw_line(LCD_RGBPACK(164, 210, 222), tx - 10 - phase,
                         ty - 18, tx + 10 + phase, ty - 18);
            rp_rect(RP_COL_ACCENT, tx - 8 - phase, ty - 26 - phase,
                    16 + phase * 2, 18 + phase * 2);
            break;

        case RP_ACTIVITY_BUY:
            rp_fill(RP_COL_ACCENT, tx - 4 + phase * 2, ty - 28 - phase,
                    8, 8);
            rp_fill(RP_COL_ACCENT, psx - 4 - phase, psy - 24 - phase,
                    7, 7);
            rp_draw_line(RP_COL_MUTED, psx, psy - 20, tx, ty - 20);
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
    if (game.save_loaded)
        rb->lcd_putsxy(58, 112, "Select continues   Menu exits");
    else
        rb->lcd_putsxy(50, 112, "Select starts   Menu exits");
}

static void rp_draw_menu_panel(const char *title)
{
    rp_fill(RP_COL_PANEL, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rp_fill(RP_COL_PANEL_2, 0, 0, LCD_WIDTH, 22);
    rb->lcd_set_foreground(RP_COL_ACCENT);
    rb->lcd_putsxy(5, 7, title);
}

static const char *rp_quest_hint(void)
{
    switch (game.quest_stage)
    {
        case 0: return "Quest: talk to the guide";
        case 1: return "Quest: 3 logs, 2 ore, 1 food";
        case 2: return "Quest: train then visit druid";
        case 3: return "Quest: defeat cave ratling";
        case 4: return "Quest: talk to city guard";
        case 5: return "Quest: guard bounty, hunt skeleton";
        default: return "Quest: explore";
    }
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
    rb->lcd_set_foreground(RP_COL_TEXT);
    rp_draw_text_clip(20, 182, rp_quest_hint(), 36);
    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(20, LCD_HEIGHT - 17, "Select levels  Left map  Menu returns");
}

static void rp_draw_level_row(int y, enum rp_sprite icon, const char *name,
                              int xp)
{
    char buf[48];
    int progress = rp_skill_progress(xp);
    int filled = (progress * 94) / 100;

    if (rp_sprites_loaded)
        rp_draw_sprite(icon, 14, y - 10);

    rb->lcd_set_foreground(RP_COL_TEXT);
    rb->snprintf(buf, sizeof(buf), "%s  L%d", name, rp_skill_level(xp));
    rb->lcd_putsxy(rp_sprites_loaded ? 52 : 20, y, buf);

    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->snprintf(buf, sizeof(buf), "%d%%", progress);
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
    rb->lcd_putsxy(20, 198, "Levels use a slow cumulative XP curve");
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
    rp_fill(RP_COL_PATH, map_x + (622 * map_w) / RP_WORLD_W,
            map_y + (180 * map_h) / RP_WORLD_H,
            (44 * map_w) / RP_WORLD_W, (360 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_PATH, map_x + (636 * map_w) / RP_WORLD_W,
            map_y + (132 * map_h) / RP_WORLD_H,
            (158 * map_w) / RP_WORLD_W, (112 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_PATH, map_x + (620 * map_w) / RP_WORLD_W,
            map_y + (496 * map_h) / RP_WORLD_H,
            (260 * map_w) / RP_WORLD_W, (42 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_PATH, map_x + (600 * map_w) / RP_WORLD_W,
            map_y + (224 * map_h) / RP_WORLD_H,
            (80 * map_w) / RP_WORLD_W, (80 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_PATH, map_x + (776 * map_w) / RP_WORLD_W,
            map_y + (132 * map_h) / RP_WORLD_H,
            (88 * map_w) / RP_WORLD_W, (56 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WATER, map_x + (485 * map_w) / RP_WORLD_W,
            map_y + (185 * map_h) / RP_WORLD_H,
            (85 * map_w) / RP_WORLD_W, (100 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WATER, map_x + (700 * map_w) / RP_WORLD_W,
            map_y + (262 * map_h) / RP_WORLD_H,
            (100 * map_w) / RP_WORLD_W, (98 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_STONE, map_x + (705 * map_w) / RP_WORLD_W,
            map_y + (390 * map_h) / RP_WORLD_H,
            (210 * map_w) / RP_WORLD_W, (190 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_STONE, map_x + (860 * map_w) / RP_WORLD_W,
            map_y + (524 * map_h) / RP_WORLD_H,
            (84 * map_w) / RP_WORLD_W, (68 * map_h) / RP_WORLD_H);
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
    rp_fill(RP_COL_ROOF, map_x + (690 * map_w) / RP_WORLD_W,
            map_y + (128 * map_h) / RP_WORLD_H,
            (80 * map_w) / RP_WORLD_W, (80 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WOOD, map_x + (716 * map_w) / RP_WORLD_W,
            map_y + (160 * map_h) / RP_WORLD_H,
            (32 * map_w) / RP_WORLD_W, (32 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_ROOF, map_x + (628 * map_w) / RP_WORLD_W,
            map_y + (104 * map_h) / RP_WORLD_H,
            (176 * map_w) / RP_WORLD_W, (168 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_ROOF, map_x + (776 * map_w) / RP_WORLD_W,
            map_y + (104 * map_h) / RP_WORLD_H,
            (88 * map_w) / RP_WORLD_W, (72 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_PATH, map_x + (660 * map_w) / RP_WORLD_W,
            map_y + (136 * map_h) / RP_WORLD_H,
            (112 * map_w) / RP_WORLD_W, (92 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WOOD, map_x + (660 * map_w) / RP_WORLD_W,
            map_y + (136 * map_h) / RP_WORLD_H,
            (32 * map_w) / RP_WORLD_W, (32 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WOOD, map_x + (728 * map_w) / RP_WORLD_W,
            map_y + (168 * map_h) / RP_WORLD_H,
            (32 * map_w) / RP_WORLD_W, (32 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WOOD, map_x + (620 * map_w) / RP_WORLD_W,
            map_y + (272 * map_h) / RP_WORLD_H,
            (52 * map_w) / RP_WORLD_W, (28 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_WOOD, map_x + (792 * map_w) / RP_WORLD_W,
            map_y + (128 * map_h) / RP_WORLD_H,
            (40 * map_w) / RP_WORLD_W, (32 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_GRASS_DARK, map_x + (520 * map_w) / RP_WORLD_W,
            map_y + (400 * map_h) / RP_WORLD_H,
            (120 * map_w) / RP_WORLD_W, (70 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_GRASS_DARK, map_x + (792 * map_w) / RP_WORLD_W,
            map_y + (216 * map_h) / RP_WORLD_H,
            (64 * map_w) / RP_WORLD_W, (44 * map_h) / RP_WORLD_H);
    rp_fill(RP_COL_GRASS_DARK, map_x + (180 * map_w) / RP_WORLD_W,
            map_y + (532 * map_h) / RP_WORLD_H,
            (90 * map_w) / RP_WORLD_W, (76 * map_h) / RP_WORLD_H);
    rp_fill(LCD_RGBPACK(83, 74, 58), map_x + (650 * map_w) / RP_WORLD_W,
            map_y + (500 * map_h) / RP_WORLD_H,
            (96 * map_w) / RP_WORLD_W, (70 * map_h) / RP_WORLD_H);

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
    rp_load_game();
    game.music_started = rp_start_music();
    rp_smoke_log("start", 0);

    while (!game.quit)
    {
        long event = rb->button_get_w_tmo(RP_FRAME_TICKS);
        rp_handle_event(event);
        rp_update_movement();
        rp_update_activity();
        rp_autosave_if_needed();
        rp_render();
        rendered_frames++;
        if (rendered_frames == 3)
            rp_smoke_log("rendered_frames", rendered_frames);
    }

    rp_smoke_log("exit", rendered_frames);
    rp_save_game();
    rp_set_wheel_events(true);
    rb->button_clear_queue();
    return PLUGIN_OK;
}

#else

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    return PLUGIN_OK;
}

#endif
