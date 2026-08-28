/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__ /  \
 *                     \/            \/     \/    \/            \/
 *
 * Club Penguin offline iPod port
 *
 * Copyright (C) 2026 OpenAI
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 ****************************************************************************/

#include "plugin.h"
#include "lib/pluginlib_actions.h"

#define CP_ASSET_DIR ROCKBOX_DIR "/rocks/games/clubpenguin"
#define CP_WORLD_FILE CP_ASSET_DIR "/world.bmp"
#define CP_PLAYER_FILE CP_ASSET_DIR "/player.bmp"
#define CP_WORLD_DATA_FILE CP_ASSET_DIR "/data/world.tsv"
#define CP_ROOMS_DATA_FILE CP_ASSET_DIR "/data/rooms.tsv"
#define CP_INTERACTIONS_DATA_FILE CP_ASSET_DIR "/data/interactions.tsv"
#define CP_SHOP_DATA_FILE CP_ASSET_DIR "/data/shop.tsv"
#define CP_SPORT_DATA_FILE CP_ASSET_DIR "/data/sport_shop.tsv"
#define CP_COSTUME_DATA_FILE CP_ASSET_DIR "/data/costume_shop.tsv"
#define CP_PENGUIN_STYLE_DATA_FILE \
    CP_ASSET_DIR "/data/penguin_style_shop.tsv"
#define CP_NINJA_DATA_FILE CP_ASSET_DIR "/data/ninja_catalog.tsv"
#define CP_PUFFLE_FOOD_DATA_FILE CP_ASSET_DIR "/data/puffle_food.tsv"
#define CP_PUFFLE_TOY_DATA_FILE CP_ASSET_DIR "/data/puffle_toys.tsv"
#define CP_PUFFLE_HAT_DATA_FILE CP_ASSET_DIR "/data/puffle_hats.tsv"
#define CP_PUFFLE_ADOPT_DATA_FILE CP_ASSET_DIR "/data/puffle_adopt.tsv"
#define CP_PET_FURNITURE_DATA_FILE \
    CP_ASSET_DIR "/data/pet_furniture.tsv"
#define CP_FURNITURE_DATA_FILE CP_ASSET_DIR "/data/furniture.tsv"
#define CP_IGLOO_BUILDING_DATA_FILE CP_ASSET_DIR "/data/igloo_buildings.tsv"
#define CP_IGLOO_FLOOR_DATA_FILE CP_ASSET_DIR "/data/igloo_flooring.tsv"
#define CP_IGLOO_LOCATION_DATA_FILE CP_ASSET_DIR "/data/igloo_locations.tsv"
#define CP_APR_FURNITURE_DATA_FILE \
    CP_ASSET_DIR "/data/apr2012_furniture.tsv"
#define CP_FEB_IGLOO_DATA_FILE CP_ASSET_DIR "/data/feb2012_igloo.tsv"
#define CP_FURNITURE_INVENTORY_FILE CP_ASSET_DIR "/furniture_inventory.dat"
#define CP_CLOTHING_INVENTORY_FILE CP_ASSET_DIR "/clothing_inventory.dat"
#define CP_SHOP_PAGE_PATTERN CP_ASSET_DIR "/shop/page%d.bmp"
#define CP_SPORT_PAGE_PATTERN CP_ASSET_DIR "/shop/sport/page%d.bmp"
#define CP_COSTUME_PAGE_PATTERN CP_ASSET_DIR "/shop/costume/page%d.bmp"
#define CP_PENGUIN_STYLE_PAGE_PATTERN \
    CP_ASSET_DIR "/shop/penguin_style/page%d.bmp"
#define CP_NINJA_PAGE_PATTERN CP_ASSET_DIR "/shop/ninja/page%d.bmp"
#define CP_AVATAR_COLOR_PATTERN CP_ASSET_DIR "/avatar/color_%d.bmp"
#define CP_AVATAR_ITEM_PATTERN CP_ASSET_DIR "/avatar/page%d_%d.bmp"
#define CP_SPORT_AVATAR_PATTERN CP_ASSET_DIR "/avatar/sport_%d.bmp"
#define CP_PUFFLE_FOOD_PATTERN CP_ASSET_DIR "/puffles/food/%s.bmp"
#define CP_PUFFLE_WALK_PATTERN CP_ASSET_DIR "/puffles/walk/%s.bmp"
#define CP_PUFFLE_DIG_PATTERN CP_ASSET_DIR "/puffles/dig/%s.bmp"
#define CP_PUFFLE_EAT_PATTERN CP_ASSET_DIR "/puffles/eat/%s.bmp"
#define CP_PUFFLE_TRICK_PATTERN \
    CP_ASSET_DIR "/puffles/tricks/%s_%s.bmp"
#define CP_PUFFLE_TOY_PATTERN \
    CP_ASSET_DIR "/puffles/toys/%s_%s.bmp"
#define CP_PUFFLE_TOY_ICON_PATTERN \
    CP_ASSET_DIR "/puffles/toys/icons/%s.bmp"
#define CP_PUFFLE_HAT_PATTERN CP_ASSET_DIR "/puffles/hats/%s.bmp"
#define CP_PUFFLE_ADOPT_PAGE_PATTERN \
    CP_ASSET_DIR "/puffles/adopt/page%d.bmp"
#define CP_PET_FURNITURE_PAGE_PATTERN \
    CP_ASSET_DIR "/puffles/furniture/page%d.bmp"
#define CP_PET_FURNITURE_SECRET_PATTERN \
    CP_ASSET_DIR "/puffles/furniture/page%d_secret.bmp"
#define CP_PUFFLE_ROOM_HAT_PATTERN \
    CP_ASSET_DIR "/puffles/hats/room/%s/%d_%s.bmp"
#define CP_PUFFLE_CARE_BACKGROUND_FILE \
    CP_ASSET_DIR "/puffles/care/background.bmp"
#define CP_PUFFLE_CARE_ICONS_FILE CP_ASSET_DIR "/puffles/care/icons.bmp"
#define CP_SAVE_FILE CP_ASSET_DIR "/save.dat"
#define CP_SAVE_TMP_FILE CP_ASSET_DIR "/save.tmp"
#define CP_CART_TITLE_FILE CP_ASSET_DIR "/minigames/cart_surfer/title.bmp"
#define CP_CART_TUNNEL_FILE CP_ASSET_DIR "/minigames/cart_surfer/tunnel.bmp"
#define CP_CART_SPRITES_FILE CP_ASSET_DIR "/minigames/cart_surfer/cart.bmp"
#define CP_TOOLBAR_FILE CP_ASSET_DIR "/ui/toolbar.bmp"
#define CP_FURNITURE_ITEM_PATTERN CP_ASSET_DIR "/igloo/items/%d.bmp"
#define CP_IGLOO_BUILDING_PATTERN CP_ASSET_DIR "/igloo/buildings/%d.bmp"
#define CP_IGLOO_FLOOR_PATTERN CP_ASSET_DIR "/igloo/flooring/%d_%d.bmp"
#define CP_IGLOO_LOCATION_PATTERN CP_ASSET_DIR "/igloo/locations/%d.bmp"
#define CP_IGLOO_MASK_PATTERN CP_ASSET_DIR "/igloo/masks/%d.msk"
#define CP_FURNITURE_CATALOG_PAGE_PATTERN \
    CP_ASSET_DIR "/igloo/catalog/furniture/page%d.bmp"
#define CP_IGLOO_BOOK_PAGE_PATTERN \
    CP_ASSET_DIR "/igloo/catalog/upgrades/page%d.bmp"
#define CP_BACKYARD_PATTERN CP_ASSET_DIR "/backyard/%d.bmp"
#define CP_SOUND_TITLE_FILE CP_ASSET_DIR "/soundstudio/title.bmp"
#define CP_SOUND_BOARD_FILE CP_ASSET_DIR "/soundstudio/board.bmp"
#define CP_SOUND_SAVED_FILE CP_ASSET_DIR "/soundstudio/saved_tracks.bmp"
#define CP_SOUND_SAVED_EMPTY_FILE CP_ASSET_DIR "/soundstudio/saved_empty.bmp"
#define CP_SOUND_SAVE_FILE CP_ASSET_DIR "/soundstudio/save_prompt.bmp"
#define CP_SOUND_INSTRUCTION_PATTERN \
    CP_ASSET_DIR "/soundstudio/instruction_%d.bmp"
#define CP_SOUND_ALBUM_PATTERN CP_ASSET_DIR "/soundstudio/%s.cpsa"
#define CP_SOUND_TRACK_PATTERN CP_ASSET_DIR "/soundstudio/track_%d.cptr"
#define CP_WELCOME_PATTERN CP_ASSET_DIR "/tutorial/welcome_%d.bmp"
#define CP_CONCERT_INTRO_FILE CP_ASSET_DIR "/concert/intro.bmp"
#define CP_CONCERT_STRIP_FILE CP_ASSET_DIR "/concert/strip.bmp"
#define CP_CONCERT_AUDIO_FILE CP_ASSET_DIR "/concert/song.pcm"
#define CP_EMMA_CONCERT_INTRO_FILE CP_ASSET_DIR "/rooms/emma_sewer.bmp"
#define CP_EMMA_CONCERT_STRIP_FILE \
    CP_ASSET_DIR "/rooms/night_city/emma_sewer_frames/strip.bmp"
#define CP_EMMA_CONCERT_AUDIO_FILE CP_ASSET_DIR "/music/emma_sewer_set.pcm"
#define CP_TOWN_AUDIO_FILE CP_ASSET_DIR "/music/town_lofi.pcm"
#define CP_HOCKEY_AUDIO_FILE CP_ASSET_DIR "/music/hockey_lofi.pcm"
#define CP_NIGHT_CITY_TOWN_STRIP \
    CP_ASSET_DIR "/rooms/night_city/town_frames/strip.bmp"
#define CP_AFTERLIFE_FRAME_STRIP \
    CP_ASSET_DIR "/rooms/night_city/afterlife_frames/strip.bmp"
#define CP_AFTERLIFE_LOUNGE_STRIP \
    CP_ASSET_DIR "/rooms/night_city/lounge_frames/strip.bmp"
#define CP_NIGHT_CITY_COFFEE_STRIP \
    CP_ASSET_DIR "/rooms/night_city/coffee_frames/strip.bmp"
#define CP_NIGHT_CITY_PLAZA_STRIP \
    CP_ASSET_DIR "/rooms/night_city/plaza_frames/strip.bmp"
#define CP_LAGUNA_BEND_DOCK_STRIP \
    CP_ASSET_DIR "/rooms/night_city/dock_frames/strip.bmp"
#define CP_BUCK_A_SLICE_STRIP \
    CP_ASSET_DIR "/rooms/night_city/pizza_frames/strip.bmp"
#define CP_LAGUNA_COTTAGE_STRIP \
    CP_ASSET_DIR "/rooms/night_city/cottage_frames/strip.bmp"
#define CP_LAGUNA_COTTAGE_ROOF_STRIP \
    CP_ASSET_DIR "/rooms/night_city/cottage_roof_frames/strip.bmp"
#define CP_LAGUNA_BEND_SHORE_STRIP \
    CP_ASSET_DIR "/rooms/night_city/beach_frames/strip.bmp"
#define CP_MONTREAL_HOCKEY_STRIP \
    CP_ASSET_DIR "/rooms/night_city/stadium_frames/strip.bmp"
#define CP_CLOCK_DISTRICT_STRIP \
    CP_ASSET_DIR "/rooms/night_city/snow_forts_frames/strip.bmp"
#define CP_VELVET_ICE_STRIP \
    CP_ASSET_DIR "/rooms/night_city/velvet_ice_frames/strip.bmp"
#define CP_CHROME_CLINIC_STRIP \
    CP_ASSET_DIR "/rooms/night_city/chrome_clinic_frames/strip.bmp"
#define CP_ARASAKA_HQ_EXTERIOR_STRIP \
    CP_ASSET_DIR "/rooms/night_city/dojo_courtyard_frames/strip.bmp"
#define CP_ARASAKA_HQ_LOBBY_STRIP \
    CP_ASSET_DIR "/rooms/night_city/dojo_frames/strip.bmp"
#define CP_MEMORIAL_PARK_STRIP \
    CP_ASSET_DIR "/rooms/night_city/forest_frames/strip.bmp"

#define CP_STATUS_H 20
#define CP_VIEW_W LCD_WIDTH
#define CP_VIEW_H (LCD_HEIGHT - CP_STATUS_H)
#define CP_WORLD_W CP_VIEW_W
#define CP_WORLD_H CP_VIEW_H
#define CP_PLAYER_W 52
#define CP_PLAYER_H 56
#define CP_DOOR_BODY_HALF_W 12
#define CP_DOOR_BODY_HALF_H 16
#define CP_DOOR_MAX_RADIUS 18
#define CP_PLAYER_ANIM_FRAMES 3
#define CP_PLAYER_SELECTOR_FRAME 12
#define CP_PLAYER_FRAMES 13
#define CP_PLAYER_STRIP_W (CP_PLAYER_W * CP_PLAYER_FRAMES)
#define CP_PLAYER_STRIP_H CP_PLAYER_H
#define CP_INPUT_STEP 9
#define CP_WALK_STEP 3
#define CP_WALK_AHEAD 15
#define CP_WALK_RATE 20
#define CP_WALK_CATCHUP_MAX 3
#define CP_MESSAGE_TTL 70
#define CP_MAX_HOTSPOTS 16
#define CP_MAX_ROOMS 64
#define CP_MAX_SHOP_ITEMS 63
#define CP_SHOP_PAGES 5
#define CP_COLOR_PAGE 3
#define CP_FACE_PAGE 4
#define CP_SPORT_PAGES 9
#define CP_COSTUME_PAGES 6
#define CP_PENGUIN_STYLE_PAGES 16
#define CP_PENGUIN_STYLE_ITEMS 186
#define CP_NINJA_PAGES 15
#define CP_COSTUME_NECK 3
#define CP_CLOTHING_FACE 4
#define CP_SPORT_HAND 5
#define CP_SPORT_BACKGROUND 6
#define CP_SPORT_FURNITURE 7
#define CP_CLOTHING_FLAG 8
#define CP_CLOTHING_COLOR 9
#define CP_NINJA_BUILDING 10
#define CP_OLIVER_SLOT 11
#define CP_PUFFLE_W 40
#define CP_PUFFLE_H 40
#define CP_PUFFLE_FRAMES 8
#define CP_PUFFLE_TYPES 12
#define CP_PUFFLE_CARE_ICON_W 16
#define CP_PUFFLE_CARE_ICON_H 16
#define CP_PUFFLE_CARE_ICONS 6
#define CP_PUFFLE_TRICK_W 50
#define CP_PUFFLE_TRICK_H 50
#define CP_PUFFLE_TRICK_FRAMES 8
#define CP_PUFFLE_TRICKS 6
#define CP_PUFFLE_TOY_W 50
#define CP_PUFFLE_TOY_H 50
#define CP_PUFFLE_TOY_FRAMES 8
#define CP_PUFFLE_TOYS_PER_TYPE 2
#define CP_PUFFLE_TOY_ITEMS \
    (CP_PUFFLE_TYPES * CP_PUFFLE_TOYS_PER_TYPE)
#define CP_PUFFLE_HAT_W 40
#define CP_PUFFLE_HAT_H 40
#define CP_PUFFLE_HAT_FRAMES 2
#define CP_PUFFLE_HAT_ITEMS 68
#define CP_PUFFLE_ADOPT_PAGES 11
#define CP_PET_FURNITURE_PAGES 5
#define CP_PUFFLE_HAT_MASKS 3
#define CP_REGULAR_PUFFLE_TYPES 10
#define CP_RAINBOW_PUFFLE 10
#define CP_GOLD_PUFFLE 11
#define CP_PUFFLE_ALL_MASK ((1u << CP_PUFFLE_TYPES) - 1)
#define CP_PUFFLE_FOOD_ITEMS 14
#define CP_PUFFLE_RARE_COST 65000
#define CP_FURNITURE_W 64
#define CP_FURNITURE_H 80
#define CP_MAX_FURNITURE_PLACEMENTS 16
#define CP_PORTAL_BOX_ID 529
#define CP_FURNITURE_LINE 128
#define CP_IGLOO_LINE 128
#define CP_IGLOO_TABS 3
#define CP_FURNITURE_CATALOG_PAGES 14
#define CP_IGLOO_BOOK_PAGES 11
#define CP_IGLOO_MASK_ROW ((CP_VIEW_W + 7) / 8)
#define CP_IGLOO_MASK_HEADER 8
#define CP_TEXT_BUF 8192
#define CP_INTERACTION_LINE 256
#define CP_ANIM_RATE 2
#define CP_CART_FRAME_W 40
#define CP_CART_FRAME_H 40
#define CP_CART_FRAMES 6
#define CP_CART_SHEET_W 320
#define CP_CART_SHEET_H 64
#define CP_CART_TRACK_W 80
#define CP_CART_TRACK_H 24
#define CP_CART_TRACK_Y 40
#define CP_CART_TRACK_FRAMES 4
#define CP_CART_SEGMENT_TICKS 60
#define CP_CART_JUMP_TICKS 20
#define CP_CART_COUNTDOWN_TICKS 75
#define CP_SOUND_RATE 22050
#define CP_SOUND_CLIPS 40
#define CP_SOUND_LOOPS 25
#define CP_SOUND_MIX_FRAMES 512
#define CP_SOUND_MIX_BYTES (CP_SOUND_MIX_FRAMES * 2 * sizeof(int16_t))
#define CP_SOUND_INSTRUCTION_PAGES 5
#define CP_SOUND_MAX_TRACKS 8
#define CP_SOUND_TRACK_HEADER 48
#define CP_SOUND_TRACK_SECONDS 180
#define CP_MUSIC_AMPLITUDE (MIX_AMP_UNITY * 3 / 5)
#define CP_CONCERT_CACHE_FRAMES 18
#define CP_CONCERT_INTRO_TICKS (HZ / 2)
#define CP_CONCERT_FRAME_RATE 6
#define CP_NIGHT_CITY_FRAMES 12
#define CP_NIGHT_CITY_FRAME_RATE 6

/* read_bmp_file()/read_bmp_fd() need extra scratch space *inside* the
 * destination buffer whenever the source bitmap is wider than
 * BM_MAX_WIDTH (i.e. wider than the LCD). The player sprite strip is wider
 * than the iPod LCD, so without this padding read_bmp_file() returns an
 * error (-6) and the asset is reported as "missing" even though it loaded.
 * See apps/recorder/bmp.c around the "bm->width > BM_MAX_WIDTH" checks. */
#define CP_BMP_SCRATCH_ELEMS(w) \
    ((((w) * 4 + 8) + (int)sizeof(fb_data) - 1) / (int)sizeof(fb_data))
#define CP_PLAYER_BUFFER_ELEMS \
    MAX(CP_PLAYER_STRIP_W * CP_PLAYER_STRIP_H + \
        CP_BMP_SCRATCH_ELEMS(CP_PLAYER_STRIP_W), \
        CP_CART_SHEET_W * CP_CART_SHEET_H + \
        CP_BMP_SCRATCH_ELEMS(CP_CART_SHEET_W))
#define CP_PLAYER_BUFFER_BYTES \
    (CP_PLAYER_BUFFER_ELEMS * sizeof(fb_data))
#define CP_ANIMATION_STRIP_W \
    (CP_VIEW_W * CP_CONCERT_CACHE_FRAMES)
#define CP_ANIMATION_BUFFER_ELEMS \
    (CP_ANIMATION_STRIP_W * CP_VIEW_H + \
     CP_BMP_SCRATCH_ELEMS(CP_ANIMATION_STRIP_W))
#define CP_ANIMATION_BUFFER_BYTES \
    (CP_ANIMATION_BUFFER_ELEMS * sizeof(fb_data))

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
static const struct button_mapping cp_main_ctx[] =
{
    { PLA_EXIT,        BUTTON_MENU|BUTTON_SELECT,         BUTTON_NONE },
    { PLA_SELECT,      BUTTON_SELECT,                     BUTTON_NONE },
    { PLA_CANCEL,      BUTTON_SELECT|BUTTON_PLAY,         BUTTON_NONE },
    { PLA_SELECT_REPEAT, BUTTON_SELECT|BUTTON_REPEAT,     BUTTON_NONE },
    { PLA_UP,          BUTTON_MENU,                       BUTTON_NONE },
    { PLA_DOWN,        BUTTON_PLAY,                       BUTTON_NONE },
    { PLA_SELECT_REL,  BUTTON_PLAY|BUTTON_REL,            BUTTON_PLAY },
    { PLA_UP,          BUTTON_SCROLL_BACK,                BUTTON_NONE },
    { PLA_DOWN,        BUTTON_SCROLL_FWD,                 BUTTON_NONE },
    { PLA_LEFT,        BUTTON_LEFT,                       BUTTON_NONE },
    { PLA_RIGHT,       BUTTON_RIGHT,                      BUTTON_NONE },
    { PLA_UP_REPEAT,   BUTTON_SCROLL_BACK|BUTTON_REPEAT,  BUTTON_NONE },
    { PLA_DOWN_REPEAT, BUTTON_SCROLL_FWD|BUTTON_REPEAT,   BUTTON_NONE },
    { PLA_DOWN_REPEAT, BUTTON_PLAY|BUTTON_REPEAT,         BUTTON_NONE },
    { PLA_LEFT_REPEAT, BUTTON_LEFT|BUTTON_REPEAT,         BUTTON_NONE },
    { PLA_RIGHT_REPEAT,BUTTON_RIGHT|BUTTON_REPEAT,        BUTTON_NONE },
    LAST_ITEM_IN_LIST
};

static const struct button_mapping *plugin_contexts[] = { cp_main_ctx };
#else
static const struct button_mapping *plugin_contexts[] = { pla_main_ctx };
#endif

#define CP_QUIT_ACTION PLA_EXIT
#define CP_CANCEL_ACTION PLA_CANCEL
#define CP_UP_ACTION PLA_UP
#define CP_DOWN_ACTION PLA_DOWN
#define CP_LEFT_ACTION PLA_LEFT
#define CP_RIGHT_ACTION PLA_RIGHT
#define CP_SELECT_ACTION PLA_SELECT
#define CP_PLAY_ACTION PLA_SELECT_REL
#define CP_UP_REPEAT PLA_UP_REPEAT
#define CP_DOWN_REPEAT PLA_DOWN_REPEAT
#define CP_LEFT_REPEAT PLA_LEFT_REPEAT
#define CP_RIGHT_REPEAT PLA_RIGHT_REPEAT

enum cp_scene_type
{
    CP_SCENE_MAP = 0,
    CP_SCENE_ROOM,
    CP_SCENE_WELCOME,
    CP_SCENE_SHOP,
    CP_SCENE_SPORT_SHOP,
    CP_SCENE_COSTUME_SHOP,
    CP_SCENE_PENGUIN_STYLE,
    CP_SCENE_NINJA_SHOP,
    CP_SCENE_PUFFLE_SHOP,
    CP_SCENE_PET_FURNITURE,
    CP_SCENE_PUFFLE_CARE,
    CP_SCENE_PUFFLE_FOOD,
    CP_SCENE_PUFFLE_TOYS,
    CP_SCENE_PUFFLE_TRICKS,
    CP_SCENE_PUFFLE_HATS,
    CP_SCENE_IGLOO_EDIT,
    CP_SCENE_IGLOO_CATALOG,
    CP_SCENE_FURNITURE_CATALOG,
    CP_SCENE_IGLOO_BOOK,
    CP_SCENE_SOUND_TITLE,
    CP_SCENE_SOUND_INSTRUCTIONS,
    CP_SCENE_SOUND_SAVED,
    CP_SCENE_SOUND_SAVE,
    CP_SCENE_SOUND_BOARD,
    CP_SCENE_CART_TITLE,
    CP_SCENE_CART_COUNTDOWN,
    CP_SCENE_CART_PLAYING,
    CP_SCENE_CART_PAUSED,
    CP_SCENE_CART_CRASH,
    CP_SCENE_CART_RESULTS
};

enum cp_direction
{
    CP_DIR_DOWN = 0,
    CP_DIR_LEFT,
    CP_DIR_UP,
    CP_DIR_RIGHT
};

enum cp_action_type
{
    CP_ACTION_ROOM = 0,
    CP_ACTION_MAP,
    CP_ACTION_MINIGAME,
    CP_ACTION_SHOP,
    CP_ACTION_PUFFLE,
    CP_ACTION_IGLOO,
    CP_ACTION_MESSAGE
};

enum cp_music_kind
{
    CP_MUSIC_NONE = 0,
    CP_MUSIC_TOWN,
    CP_MUSIC_HOCKEY,
    CP_MUSIC_CONCERT,
    CP_MUSIC_EMMA_CONCERT,
    CP_MUSIC_STUDIO
};

enum cp_cart_trick
{
    CP_CART_TRICK_NONE = 0,
    CP_CART_TRICK_OLLIE,
    CP_CART_TRICK_SPIN,
    CP_CART_TRICK_FLAP,
    CP_CART_TRICK_GRIND
};

struct cp_cart_state
{
    int return_room;
    int return_x;
    int return_y;
    int score;
    int lives;
    int segment;
    int segment_tick;
    int jump_ticks;
    int lean;
    int lean_ticks;
    int crash_ticks;
    int sprite_frame;
    int trick_cooldown;
    int countdown_ticks;
    int speed_stage;
    int combo;
    int run_best_combo;
    int results_choice;
    int reward;
    long next_tick;
    enum cp_cart_trick trick;
    enum cp_cart_trick last_trick;
    bool rewarded;
    bool menu_latched;
};

struct cp_sound_state
{
    unsigned char *buffer;
    int16_t *mix_buffer;
    size_t buffer_size;
    size_t album_size;
    unsigned int old_frequency;
    unsigned int loop_mask;
    unsigned int loop_position;
    int one_shot_id;
    unsigned int one_shot_position;
    int return_room;
    int return_x;
    int return_y;
    int title_choice;
    int instruction_page;
    int saved_slot;
    int save_choice;
    int column;
    int row;
    int genre;
    enum cp_music_kind music_kind;
    int record_fd;
    int playback_fd;
    int record_slot;
    unsigned int record_events;
    unsigned int playback_events;
    unsigned int playback_next_time;
    unsigned int playback_duration;
    uint64_t playback_next_mask;
    long record_start_tick;
    long record_prepare_deadline;
    long playback_start_tick;
    bool buffer_acquired;
    bool configured;
    bool started;
    bool record_preparing;
    bool recording;
    bool playback;
    bool playback_has_event;
    char record_name[32];
};

struct cp_hotspot
{
    char id[24];
    char name[32];
    char detail[80];
    char target[24];
    enum cp_action_type action;
    int x;
    int y;
    int radius;
    int to_x;
    int to_y;
};

struct cp_shop_item
{
    char name[24];
    int page;
    int slot;
    int cost;
};

struct cp_furniture_item
{
    char name[48];
    int id;
    int cost;
    int type;
    int max_quantity;
    bool member_only;
};

struct cp_sport_item
{
    char name[36];
    int page;
    int id;
    int type;
    int cost;
    int owned_bit;
    int max_quantity;
};

struct cp_puffle_food_item
{
    char name[24];
    char asset[24];
    int id;
    int cost;
    int food;
    int rest;
    int happy;
    int clean;
};

struct cp_puffle_toy_item
{
    char name[48];
    char asset[32];
    char kind[8];
    int color;
    int id;
    int cost;
    int food;
    int rest;
    int happy;
    int clean;
    int reaction;
};

struct cp_puffle_hat_item
{
    char name[32];
    char asset[32];
    int id;
    int cost;
    bool available;
};

struct cp_furniture_placement
{
    int item_id;
    int x;
    int y;
};

struct cp_igloo_item
{
    char name[48];
    int id;
    int cost;
    int auxiliary;
};

struct cp_room
{
    char id[24];
    char title[32];
    char bitmap[80];
    int start_x;
    int start_y;
    int walk_left;
    int walk_top;
    int walk_right;
    int walk_bottom;
};

struct cp_hotspot_def
{
    const char *name;
    const char *detail;
    const char *target;
    int x;
    int y;
    int radius;
    int to_x;
    int to_y;
};

struct cp_room_def
{
    const char *id;
    const char *title;
    const char *bitmap;
    int start_x;
    int start_y;
    int walk_left;
    int walk_top;
    int walk_right;
    int walk_bottom;
};

struct cp_game
{
    struct bitmap scene;
    struct bitmap animation;
    struct bitmap player;
    struct bitmap toolbar;
    struct bitmap puffle;
    struct bitmap furniture;
    struct bitmap food_icon;
    struct bitmap care_icons;
    struct bitmap puffle_toy;
    struct bitmap toy_icon;
    struct bitmap puffle_trick;
    struct bitmap puffle_hat;
    enum cp_scene_type scene_type;
    int room_index;
    int x;
    int y;
    int target_x;
    int target_y;
    int cam_x;
    int cam_y;
    int direction;
    int anim_tick;
    int anim_frame;
    int map_x;
    int map_y;
    int selected_hotspot;
    int hotspot_count;
    int map_hotspot_count;
    int room_count;
    int shop_item_count;
    int message_frames;
    int coins;
    unsigned int visited_mask;
    char location[32];
    char message[96];
    char error[96];
    struct cp_hotspot hotspots[CP_MAX_HOTSPOTS];
    struct cp_hotspot map_hotspots[CP_MAX_HOTSPOTS];
    struct cp_room rooms[CP_MAX_ROOMS];
    struct cp_shop_item shop_items[CP_MAX_SHOP_ITEMS];
    struct cp_cart_state cart;
    char saved_room[24];
    int saved_x;
    int saved_y;
    int welcome_page;
    int cart_best_score;
    int cart_best_combo;
    unsigned int owned_lo;
    unsigned int owned_hi;
    int equipped[CP_SHOP_PAGES];
    int shop_page;
    int shop_selection;
    int shop_return_room;
    int shop_return_x;
    int shop_return_y;
    char shop_status[48];
    int sport_page;
    int sport_index;
    int sport_count;
    struct cp_sport_item sport_item;
    unsigned int sport_owned;
    unsigned int costume_owned;
    int sport_equipped[6];
    int sport_background;
    int clothing_flag;
    int special_return_room;
    int special_return_x;
    int special_return_y;
    int puffle_menu;
    int puffle_adopt_page;
    int puffle_adopt_index;
    int puffle_adopt_count;
    int puffle_adopt_cost;
    int puffle_type;
    int puffle_food[CP_PUFFLE_TYPES];
    int puffle_rest[CP_PUFFLE_TYPES];
    int puffle_happy[CP_PUFFLE_TYPES];
    int puffle_clean[CP_PUFFLE_TYPES];
    int puffle_food_quantity[CP_PUFFLE_FOOD_ITEMS];
    struct cp_puffle_food_item puffle_food_item;
    int puffle_food_index;
    int puffle_food_count;
    int puffle_frame;
    int puffle_action_ticks;
    struct cp_puffle_toy_item puffle_toy_item;
    int puffle_toy_index;
    int puffle_toy_frame;
    int puffle_toy_ticks;
    int puffle_trick_index;
    int puffle_trick_frame;
    int puffle_trick_ticks;
    struct cp_puffle_hat_item puffle_hat_item;
    int puffle_hat_index;
    int puffle_hat_count;
    int puffle_hat_equipped[CP_PUFFLE_TYPES];
    unsigned int puffle_hat_owned[CP_PUFFLE_HAT_MASKS];
    int furniture_catalog_count;
    int furniture_catalog_index;
    int furniture_catalog_type;
    int furniture_selected_slot;
    int furniture_save_version;
    unsigned int legacy_furniture_mask;
    struct cp_furniture_item furniture_item;
    struct cp_furniture_placement
        furniture_placements[CP_MAX_FURNITURE_PLACEMENTS];
    struct cp_igloo_item igloo_item;
    int igloo_catalog_count[CP_IGLOO_TABS];
    int igloo_catalog_index;
    int igloo_catalog_tab;
    int exact_catalog_page;
    int exact_catalog_index;
    int exact_catalog_count;
    int igloo_building;
    int igloo_floor;
    int igloo_location;
    unsigned int igloo_building_owned[4];
    unsigned int igloo_floor_owned;
    unsigned int igloo_location_owned;
    long next_walk_tick;
    long wheel_touch_until;
    int wheel_touch_zone;
    long next_puffle_tick;
    long next_puffle_need_tick;
    long next_concert_tick;
    long animation_start_tick;
    int concert_frame;
    int concert_intro_ticks;
    long next_city_tick;
    int city_frame;
    int animation_frames;
    bool assets_loaded;
    bool animation_loaded;
    bool dirty;
    bool welcome_complete;
    bool map_chord_latched;
    bool room_trigger_armed;
    bool puffle_walking;
    bool puffle_digging;
    bool puffle_eating;
    bool puffle_toy_playing;
    bool puffle_trick_playing;
    bool puffle_hat_loaded;
    bool pet_furniture_secret;
    unsigned int puffle_owned_mask;
    unsigned int puffle_toy_owned;
};

static fb_data scene_pixels[CP_WORLD_W * CP_WORLD_H
                            + CP_BMP_SCRATCH_ELEMS(CP_WORLD_W)];
/* The higher-resolution paper doll lives in the normal plugin buffer. Keep
 * it out of static BSS and, critically, out of the playback audio buffer. */
static fb_data *player_pixels;
static fb_data *animation_pixels;
static size_t animation_buffer_size;
static fb_data toolbar_pixels[CP_VIEW_W * CP_STATUS_H];
static fb_data puffle_pixels[CP_PUFFLE_W * CP_PUFFLE_FRAMES * CP_PUFFLE_H
                             + CP_BMP_SCRATCH_ELEMS(CP_PUFFLE_W *
                                                    CP_PUFFLE_FRAMES)];
static fb_data puffle_hat_pixels[
    CP_PUFFLE_HAT_W * CP_PUFFLE_HAT_FRAMES * CP_PUFFLE_HAT_H];
static char text_buf[CP_TEXT_BUF];

static struct cp_game game;
static struct cp_sound_state sound;
static void cp_set_message(const char *text);
static void cp_sound_close_audio(void);
static bool cp_sound_show_save_prompt(int slot);
static bool cp_music_switch_for_room(const char *room_id);
static bool cp_apply_puffle_room_hat(void);
static bool cp_write_save(void);

static const char *puffle_ids[CP_PUFFLE_TYPES] =
{
    "blue", "red", "pink", "black", "green", "purple",
    "yellow", "white", "orange", "brown", "rainbow", "gold"
};

static const char *puffle_names[CP_PUFFLE_TYPES] =
{
    "Blue", "Red", "Pink", "Black", "Green", "Purple",
    "Yellow", "White", "Orange", "Brown", "Rainbow", "Gold"
};

/* Deterministic points inside the official pet_area ellipse. The Flash
 * client chose random safe-zone points; stable positions make every owned
 * puffle reachable with the iPod controls after an offline reload. */
static const unsigned char backyard_puffle_x[CP_PUFFLE_TYPES] =
{
    95, 140, 185, 230, 72, 128, 192, 248, 100, 145, 190, 235
};

static const unsigned char backyard_puffle_y[CP_PUFFLE_TYPES] =
{
    105, 105, 105, 105, 137, 137, 137, 137, 168, 168, 168, 168
};

static const char *puffle_trick_ids[CP_PUFFLE_TRICKS] =
{
    "jumpForward", "jumpSpin", "nuzzle", "roll", "speak",
    "standOnHead"
};

static const char *puffle_trick_names[CP_PUFFLE_TRICKS] =
{
    "Jump Forward", "Jump Spin", "Nuzzle", "Roll", "Speak",
    "Stand on Head"
};

static const unsigned char cp_cart_segments[] =
{
    1, 1, 4, 2, 1, 5, 3, 1, 4, 2, 5, 3, 1,
    1, 1, 4, 2, 4, 2, 1, 1, 5, 3, 1, 1, 6
};

static const struct cp_hotspot_def builtin_hotspots[] =
{
    { "My Place", "Enter your offline igloo.",
      "player_home", 160, 110, 17, 160, 170 },
    { "Town", "Enter Town.", "town", 219, 175, 18, 160, 170 },
    { "Plaza", "Enter the Plaza.", "plaza", 239, 134, 19, 160, 170 },
    { "Dock", "Enter the Dock.", "dock", 42, 166, 19, 160, 170 },
    { "Ski Village", "Enter Ski Village.",
      "ski_village", 120, 118, 16, 160, 170 },
    { "Arasaka Headquarters", "Enter Arasaka Headquarters.",
      "dojo", 185, 24, 16, 160, 170 },
    { "Cove", "Enter the Cove.", "cove", 271, 74, 19, 160, 170 },
    { "Beach", "Enter the Beach.", "beach", 47, 51, 16, 160, 170 },
    { "Clock District", "Enter the redesigned Snow Forts district.",
      "snow_forts", 171, 161, 17, 160, 170 },
    { "Memorial Park", "Enter Night City's central park.",
      "forest", 260, 188, 16, 160, 170 },
    { "Mine", "Enter the Mine Shack.", "mine", 283, 152, 16, 160, 170 },
    { "Iceberg", "Enter the Iceberg.",
      "iceberg", 285, 31, 16, 160, 170 },
};

static const struct cp_room_def builtin_rooms[] =
{
    { "player_home", "My Place", "rooms/player_home.bmp",
      160, 170, 45, 120, 275, 198 },
    { "town", "Town", "rooms/town.bmp",
      160, 170, 25, 120, 295, 198 },
    { "plaza", "Plaza", "rooms/plaza.bmp",
      160, 170, 20, 120, 300, 198 },
    { "dock", "Dock", "rooms/dock.bmp",
      160, 170, 25, 100, 295, 198 },
    { "ski_village", "Ski Village", "rooms/ski_village.bmp",
      160, 170, 25, 115, 295, 198 },
    { "dojo", "Arasaka HQ Lobby", "rooms/dojo.bmp",
      160, 170, 25, 75, 295, 198 },
    { "cove", "Cove", "rooms/cove.bmp",
      160, 170, 25, 115, 295, 198 },
    { "beach", "Beach", "rooms/beach.bmp",
      160, 170, 40, 110, 280, 198 },
    { "snow_forts", "Clock District", "rooms/snow_forts.bmp",
      160, 170, 20, 82, 300, 198 },
    { "forest", "Memorial Park", "rooms/forest.bmp",
      160, 170, 20, 84, 300, 198 },
    { "mine", "Mine Shack", "rooms/mine.bmp",
      160, 170, 30, 120, 290, 198 },
    { "iceberg", "Iceberg", "rooms/iceberg.bmp",
      160, 170, 35, 105, 285, 198 },
};

static int cp_abs(int value)
{
    return value < 0 ? -value : value;
}

static int cp_dist_score(int x1, int y1, int x2, int y2)
{
    int dx = cp_abs(x1 - x2);
    int dy = cp_abs(y1 - y2);
    return dx * dx + dy * dy;
}

static bool cp_streq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0')
    {
        if (*a != *b)
            return false;
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static bool cp_sound_scene(void)
{
    return game.scene_type >= CP_SCENE_SOUND_TITLE &&
           game.scene_type <= CP_SCENE_SOUND_BOARD;
}

static bool cp_has_text(const char *text)
{
    return text != NULL && text[0] != '\0';
}

static void cp_copy(char *dst, int dst_size, const char *src)
{
    int i = 0;

    if (dst_size <= 0)
        return;

    if (src == NULL)
        src = "";

    while (i < dst_size - 1 && src[i] != '\0')
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static const char *cp_display_path(const char *path)
{
    const char *prefix = CP_ASSET_DIR "/";
    const char *a = path;
    const char *b = prefix;

    while (*a != '\0' && *b != '\0' && *a == *b)
    {
        a++;
        b++;
    }

    if (*b == '\0')
        return a;

    return path;
}

static void cp_set_message(const char *text)
{
    cp_copy(game.message, sizeof(game.message), text);
    game.message_frames = CP_MESSAGE_TTL;
    game.dirty = true;
}

static void cp_set_missing_message(const char *path)
{
    rb->snprintf(game.error, sizeof(game.error), "Missing %s",
                 cp_display_path(path));
    cp_set_message(game.error);
}

static void cp_join_asset_path(char *dst, int dst_size, const char *rel)
{
    if (rel[0] == '/')
        cp_copy(dst, dst_size, rel);
    else
        rb->snprintf(dst, dst_size, "%s/%s", CP_ASSET_DIR, rel);
}

static bool cp_parse_int(const char *text, int *value)
{
    int sign = 1;
    int out = 0;
    int i = 0;

    if (!cp_has_text(text))
        return false;

    if (text[0] == '-')
    {
        sign = -1;
        i = 1;
    }

    if (text[i] == '\0')
        return false;

    while (text[i] != '\0')
    {
        if (text[i] < '0' || text[i] > '9')
            return false;

        out = out * 10 + text[i] - '0';
        i++;
    }

    *value = out * sign;
    return true;
}

static char *cp_next_field(char **cursor)
{
    char *field = *cursor;
    char *p;

    if (field == NULL)
        return NULL;

    p = field;
    while (*p != '\0' && *p != '\t')
        p++;

    if (*p == '\t')
    {
        *p = '\0';
        *cursor = p + 1;
    }
    else
    {
        *cursor = NULL;
    }

    return field;
}

static int cp_read_text_file(const char *path)
{
    int fd;
    int got;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    got = rb->read(fd, text_buf, sizeof(text_buf) - 1);
    rb->close(fd);

    if (got < 0)
        return -1;

    text_buf[got] = '\0';
    return got;
}

static char *cp_next_line(char **cursor)
{
    char *line = *cursor;
    char *p;

    if (line == NULL)
        return NULL;

    p = line;
    while (*p != '\0' && *p != '\n' && *p != '\r')
        p++;

    if (*p == '\0')
    {
        *cursor = NULL;
    }
    else
    {
        *p = '\0';
        p++;
        while (*p == '\n' || *p == '\r')
            p++;
        *cursor = *p == '\0' ? NULL : p;
    }

    return line;
}

static bool cp_data_line(const char *line)
{
    return line[0] != '\0' && line[0] != '#';
}

static void cp_copy_hotspot(struct cp_hotspot *dst,
                            const struct cp_hotspot_def *src)
{
    cp_copy(dst->id, sizeof(dst->id), src->target);
    cp_copy(dst->name, sizeof(dst->name), src->name);
    cp_copy(dst->detail, sizeof(dst->detail), src->detail);
    cp_copy(dst->target, sizeof(dst->target), src->target);
    dst->x = src->x;
    dst->y = src->y;
    dst->radius = src->radius;
    dst->to_x = src->to_x;
    dst->to_y = src->to_y;
    dst->action = CP_ACTION_ROOM;
}

static void cp_copy_room(struct cp_room *dst, const struct cp_room_def *src)
{
    cp_copy(dst->id, sizeof(dst->id), src->id);
    cp_copy(dst->title, sizeof(dst->title), src->title);
    cp_copy(dst->bitmap, sizeof(dst->bitmap), src->bitmap);
    dst->start_x = src->start_x;
    dst->start_y = src->start_y;
    dst->walk_left = src->walk_left;
    dst->walk_top = src->walk_top;
    dst->walk_right = src->walk_right;
    dst->walk_bottom = src->walk_bottom;
}

static void cp_use_builtin_hotspots(void)
{
    int i;

    game.map_hotspot_count = ARRAYLEN(builtin_hotspots);
    for (i = 0; i < game.map_hotspot_count; i++)
        cp_copy_hotspot(&game.map_hotspots[i], &builtin_hotspots[i]);
}

static void cp_use_builtin_rooms(void)
{
    int i;

    game.room_count = ARRAYLEN(builtin_rooms);
    for (i = 0; i < game.room_count; i++)
        cp_copy_room(&game.rooms[i], &builtin_rooms[i]);
}

static void cp_parse_hotspot_line(char *line)
{
    char *cursor = line;
    char *name = cp_next_field(&cursor);
    char *detail = cp_next_field(&cursor);
    char *x_text = cp_next_field(&cursor);
    char *y_text = cp_next_field(&cursor);
    char *radius_text = cp_next_field(&cursor);
    char *target = cp_next_field(&cursor);
    char *to_x_text = cp_next_field(&cursor);
    char *to_y_text = cp_next_field(&cursor);
    struct cp_hotspot *hotspot;
    int x;
    int y;
    int radius;
    int to_x = 0;
    int to_y = 0;

    if (game.map_hotspot_count >= CP_MAX_HOTSPOTS)
        return;

    if (!cp_parse_int(x_text, &x) || !cp_parse_int(y_text, &y) ||
        !cp_parse_int(radius_text, &radius))
        return;

    if (cp_has_text(to_x_text))
        cp_parse_int(to_x_text, &to_x);
    if (cp_has_text(to_y_text))
        cp_parse_int(to_y_text, &to_y);

    hotspot = &game.map_hotspots[game.map_hotspot_count++];
    cp_copy(hotspot->name, sizeof(hotspot->name), name);
    cp_copy(hotspot->detail, sizeof(hotspot->detail), detail);
    cp_copy(hotspot->target, sizeof(hotspot->target), target);
    hotspot->x = x;
    hotspot->y = y;
    hotspot->radius = radius;
    hotspot->to_x = to_x;
    hotspot->to_y = to_y;
}

static bool cp_load_hotspots(void)
{
    char *cursor;
    char *line;

    if (cp_read_text_file(CP_WORLD_DATA_FILE) < 0)
        return false;

    game.map_hotspot_count = 0;
    cursor = text_buf;
    while ((line = cp_next_line(&cursor)) != NULL)
    {
        if (cp_data_line(line))
            cp_parse_hotspot_line(line);
    }

    return game.map_hotspot_count > 0;
}

static void cp_parse_room_line(char *line)
{
    char *cursor = line;
    char *id = cp_next_field(&cursor);
    char *title = cp_next_field(&cursor);
    char *bitmap = cp_next_field(&cursor);
    char *x_text = cp_next_field(&cursor);
    char *y_text = cp_next_field(&cursor);
    char *left_text = cp_next_field(&cursor);
    char *top_text = cp_next_field(&cursor);
    char *right_text = cp_next_field(&cursor);
    char *bottom_text = cp_next_field(&cursor);
    struct cp_room *room;
    int x;
    int y;

    if (game.room_count >= CP_MAX_ROOMS)
        return;

    if (!cp_parse_int(x_text, &x) || !cp_parse_int(y_text, &y))
        return;

    room = &game.rooms[game.room_count++];
    cp_copy(room->id, sizeof(room->id), id);
    cp_copy(room->title, sizeof(room->title), title);
    cp_copy(room->bitmap, sizeof(room->bitmap), bitmap);
    room->start_x = x;
    room->start_y = y;
    room->walk_left = CP_PLAYER_W / 2;
    room->walk_top = CP_PLAYER_H / 2;
    room->walk_right = CP_VIEW_W - CP_PLAYER_W / 2;
    room->walk_bottom = CP_VIEW_H - CP_PLAYER_H / 2;
    cp_parse_int(left_text, &room->walk_left);
    cp_parse_int(top_text, &room->walk_top);
    cp_parse_int(right_text, &room->walk_right);
    cp_parse_int(bottom_text, &room->walk_bottom);
}

static bool cp_load_rooms(void)
{
    char *cursor;
    char *line;

    if (cp_read_text_file(CP_ROOMS_DATA_FILE) < 0)
        return false;

    game.room_count = 0;
    cursor = text_buf;
    while ((line = cp_next_line(&cursor)) != NULL)
    {
        if (cp_data_line(line))
            cp_parse_room_line(line);
    }

    return game.room_count > 0;
}

static bool cp_parse_action(const char *text, enum cp_action_type *action)
{
    if (cp_streq(text, "room"))
        *action = CP_ACTION_ROOM;
    else if (cp_streq(text, "map"))
        *action = CP_ACTION_MAP;
    else if (cp_streq(text, "minigame"))
        *action = CP_ACTION_MINIGAME;
    else if (cp_streq(text, "shop"))
        *action = CP_ACTION_SHOP;
    else if (cp_streq(text, "puffle"))
        *action = CP_ACTION_PUFFLE;
    else if (cp_streq(text, "igloo"))
        *action = CP_ACTION_IGLOO;
    else if (cp_streq(text, "message"))
        *action = CP_ACTION_MESSAGE;
    else
        return false;

    return true;
}

static void cp_parse_interaction_line(char *line, const char *room_id)
{
    char *cursor = line;
    char *room = cp_next_field(&cursor);
    char *id = cp_next_field(&cursor);
    char *x_text = cp_next_field(&cursor);
    char *y_text = cp_next_field(&cursor);
    char *radius_text = cp_next_field(&cursor);
    char *action_text = cp_next_field(&cursor);
    char *target = cp_next_field(&cursor);
    char *label = cp_next_field(&cursor);
    char *to_x_text = cp_next_field(&cursor);
    char *to_y_text = cp_next_field(&cursor);
    struct cp_hotspot *hotspot;
    int x;
    int y;
    int radius;
    int to_x = 0;
    int to_y = 0;
    enum cp_action_type action;

    if (game.hotspot_count >= CP_MAX_HOTSPOTS - 1 ||
        !cp_has_text(room) || !cp_streq(room, room_id) ||
        !cp_has_text(action_text) || !cp_has_text(target) ||
        !cp_has_text(id) ||
        !cp_has_text(label) ||
        !cp_parse_int(x_text, &x) || !cp_parse_int(y_text, &y) ||
        !cp_parse_int(radius_text, &radius) || radius <= 0 ||
        !cp_parse_action(action_text, &action))
        return;

    hotspot = &game.hotspots[game.hotspot_count++];
    rb->memset(hotspot, 0, sizeof(*hotspot));
    cp_copy(hotspot->id, sizeof(hotspot->id), id);
    cp_copy(hotspot->name, sizeof(hotspot->name), label);
    cp_copy(hotspot->detail, sizeof(hotspot->detail), label);
    cp_copy(hotspot->target, sizeof(hotspot->target), target);
    hotspot->x = x;
    hotspot->y = y;
    hotspot->radius = radius;
    hotspot->action = action;
    if (cp_parse_int(to_x_text, &to_x) && cp_parse_int(to_y_text, &to_y))
    {
        hotspot->to_x = to_x;
        hotspot->to_y = to_y;
    }
}

static bool cp_load_room_interactions(const char *room_id)
{
    char line[CP_INTERACTION_LINE];
    int fd;

    fd = rb->open(CP_INTERACTIONS_DATA_FILE, O_RDONLY);
    if (fd < 0)
        return false;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        if (cp_data_line(line))
            cp_parse_interaction_line(line, room_id);
    }
    rb->close(fd);

    return true;
}

static void cp_parse_shop_line(char *line)
{
    char *cursor = line;
    char *page_text = cp_next_field(&cursor);
    char *slot_text = cp_next_field(&cursor);
    char *name = cp_next_field(&cursor);
    char *cost_text = cp_next_field(&cursor);
    struct cp_shop_item *item;
    int page;
    int slot;
    int cost;

    if (game.shop_item_count >= CP_MAX_SHOP_ITEMS || !cp_has_text(name) ||
        !cp_parse_int(page_text, &page) ||
        !cp_parse_int(slot_text, &slot) ||
        !cp_parse_int(cost_text, &cost) || page < 0 ||
        page >= CP_SHOP_PAGES || slot < 0 || slot >= 15 || cost < 0)
        return;

    item = &game.shop_items[game.shop_item_count++];
    rb->memset(item, 0, sizeof(*item));
    cp_copy(item->name, sizeof(item->name), name);
    item->page = page;
    item->slot = slot;
    item->cost = cost;
}

static bool cp_load_shop_data(void)
{
    char *cursor;
    char *line;

    if (cp_read_text_file(CP_SHOP_DATA_FILE) < 0)
        return false;

    game.shop_item_count = 0;
    cursor = text_buf;
    while ((line = cp_next_line(&cursor)) != NULL)
    {
        if (cp_data_line(line))
            cp_parse_shop_line(line);
    }

    return game.shop_item_count > 0;
}

static bool cp_load_bitmap(const char *path, struct bitmap *bmp,
                           fb_data *pixels, int maxsize, int width,
                           int height, int format)
{
    int rc;

    rb->memset(bmp, 0, sizeof(*bmp));
    bmp->data = (char *)pixels;
    bmp->width = width;
    bmp->height = height;

    rc = rb->read_bmp_file(path, bmp, maxsize, format, NULL);

    if (rc <= 0 || bmp->width != width || bmp->height != height)
    {
        cp_set_missing_message(path);
        return false;
    }

    return true;
}

static bool cp_load_scene(const char *path, int width, int height)
{
    return cp_load_bitmap(path, &game.scene, scene_pixels,
                          sizeof(scene_pixels), width, height,
                          FORMAT_NATIVE | FORMAT_DITHER);
}

static bool cp_load_animation_strip(const char *path, int frames)
{
    int width;

    game.animation_loaded = false;
    game.animation_frames = 0;
    if (frames <= 0 || frames > CP_CONCERT_CACHE_FRAMES)
        return false;

    width = CP_VIEW_W * frames;
    if (animation_pixels == NULL ||
        animation_buffer_size < (size_t)(width * CP_VIEW_H *
                                         sizeof(fb_data) +
                                         CP_BMP_SCRATCH_ELEMS(width) *
                                         sizeof(fb_data)))
    {
        cp_set_message("Animation cache is unavailable.");
        return false;
    }

    if (!cp_load_bitmap(path, &game.animation, animation_pixels,
                        (int)animation_buffer_size, width, CP_VIEW_H,
                        FORMAT_NATIVE | FORMAT_DITHER))
        return false;

    game.animation_frames = frames;
    game.animation_loaded = true;
    return true;
}

static bool cp_load_player_asset(void)
{
    static const int layer_pages[] = { 2, 1, CP_FACE_PAGE, 0 };
    static const int sport_layers[] = { 2, 1, 4, 5, 3, 0 };
    struct bitmap layer;
    char path[MAX_PATH];
    int color = game.equipped[CP_COLOR_PAGE];
    int i;
    int page;

    if (color < 0 || color >= 15)
        color = 5;
    rb->snprintf(path, sizeof(path), CP_AVATAR_COLOR_PATTERN, color);
    if (!cp_load_bitmap(path, &game.player, player_pixels,
                        CP_PLAYER_BUFFER_BYTES, CP_PLAYER_STRIP_W,
                        CP_PLAYER_STRIP_H, FORMAT_NATIVE))
    {
        /* Keep packages made before wardrobe support usable. */
        return cp_load_bitmap(CP_PLAYER_FILE, &game.player, player_pixels,
                              CP_PLAYER_BUFFER_BYTES, CP_PLAYER_STRIP_W,
                              CP_PLAYER_STRIP_H, FORMAT_NATIVE);
    }

    for (i = 0; i < (int)ARRAYLEN(layer_pages); i++)
    {
        int slot;
        int pixel;

        page = layer_pages[i];
        slot = game.equipped[page];
        if (slot < 0 || slot >= 15)
            continue;
        rb->snprintf(path, sizeof(path), CP_AVATAR_ITEM_PATTERN,
                     page, slot);
        if (!cp_load_bitmap(path, &layer, scene_pixels,
                            sizeof(scene_pixels), CP_PLAYER_STRIP_W,
                            CP_PLAYER_STRIP_H, FORMAT_NATIVE))
            continue;
        for (pixel = 0; pixel < CP_PLAYER_STRIP_W * CP_PLAYER_STRIP_H;
             pixel++)
        {
            fb_data value = ((fb_data *)layer.data)[pixel];
            if (value != TRANSPARENT_COLOR)
                player_pixels[pixel] = value;
        }
    }

    for (i = 0; i < (int)ARRAYLEN(sport_layers); i++)
    {
        int layer_index = sport_layers[i];
        int item_id = game.sport_equipped[layer_index];
        int pixel;

        if (item_id <= 0)
            continue;
        rb->snprintf(path, sizeof(path), CP_SPORT_AVATAR_PATTERN, item_id);
        if (!cp_load_bitmap(path, &layer, scene_pixels,
                            sizeof(scene_pixels), CP_PLAYER_STRIP_W,
                            CP_PLAYER_STRIP_H, FORMAT_NATIVE))
            continue;
        for (pixel = 0; pixel < CP_PLAYER_STRIP_W * CP_PLAYER_STRIP_H;
             pixel++)
        {
            fb_data value = ((fb_data *)layer.data)[pixel];
            if (value != TRANSPARENT_COLOR)
                player_pixels[pixel] = value;
        }
    }

    return true;
}

static bool cp_load_puffle_asset(int type)
{
    char path[MAX_PATH];

    if (type < 0 || type >= CP_PUFFLE_TYPES)
        type = 0;
    rb->snprintf(path, sizeof(path), "%s/puffles/%s.bmp",
                 CP_ASSET_DIR, puffle_ids[type]);
    return cp_load_bitmap(path, &game.puffle, puffle_pixels,
                          sizeof(puffle_pixels),
                          CP_PUFFLE_W * CP_PUFFLE_FRAMES, CP_PUFFLE_H,
                          FORMAT_NATIVE);
}

static bool cp_load_puffle_motion(int type, const char *pattern)
{
    char path[MAX_PATH];

    if (type < 0 || type >= CP_PUFFLE_TYPES)
        type = 0;
    rb->snprintf(path, sizeof(path), pattern, puffle_ids[type]);
    return cp_load_bitmap(path, &game.puffle, puffle_pixels,
                          sizeof(puffle_pixels),
                          CP_PUFFLE_W * CP_PUFFLE_FRAMES, CP_PUFFLE_H,
                          FORMAT_NATIVE);
}

static bool cp_load_puffle_walk(int type)
{
    return cp_load_puffle_motion(type, CP_PUFFLE_WALK_PATTERN) &&
           cp_apply_puffle_room_hat();
}

static bool cp_load_puffle_dig(int type)
{
    return cp_load_puffle_motion(type, CP_PUFFLE_DIG_PATTERN);
}

static bool cp_load_puffle_eat(int type)
{
    return cp_load_puffle_motion(type, CP_PUFFLE_EAT_PATTERN);
}

static bool cp_load_puffle_food_icon(const char *asset)
{
    char path[MAX_PATH];

    rb->snprintf(path, sizeof(path), CP_PUFFLE_FOOD_PATTERN, asset);
    return cp_load_bitmap(path, &game.food_icon, player_pixels,
                          CP_PLAYER_BUFFER_BYTES, CP_PUFFLE_W, CP_PUFFLE_H,
                          FORMAT_NATIVE);
}

static bool cp_load_puffle_care_icons(void)
{
    return cp_load_bitmap(CP_PUFFLE_CARE_ICONS_FILE, &game.care_icons,
                          player_pixels, CP_PLAYER_BUFFER_BYTES,
                          CP_PUFFLE_CARE_ICON_W * CP_PUFFLE_CARE_ICONS,
                          CP_PUFFLE_CARE_ICON_H, FORMAT_NATIVE);
}

static bool cp_load_puffle_trick(int type, int trick)
{
    char path[MAX_PATH];

    if (type < 0 || type >= CP_PUFFLE_TYPES)
        type = 0;
    if (trick < 0 || trick >= CP_PUFFLE_TRICKS)
        trick = 0;
    rb->snprintf(path, sizeof(path), CP_PUFFLE_TRICK_PATTERN,
                 puffle_ids[type], puffle_trick_ids[trick]);
    return cp_load_bitmap(path, &game.puffle_trick, player_pixels,
                          CP_PLAYER_BUFFER_BYTES,
                          CP_PUFFLE_TRICK_W * CP_PUFFLE_TRICK_FRAMES,
                          CP_PUFFLE_TRICK_H, FORMAT_NATIVE);
}

static bool cp_load_puffle_toy(int type, const char *kind)
{
    char path[MAX_PATH];

    if (type < 0 || type >= CP_PUFFLE_TYPES)
        type = 0;
    rb->snprintf(path, sizeof(path), CP_PUFFLE_TOY_PATTERN,
                 puffle_ids[type], kind);
    return cp_load_bitmap(path, &game.puffle_toy, player_pixels,
                          CP_PLAYER_BUFFER_BYTES,
                          CP_PUFFLE_TOY_W * CP_PUFFLE_TOY_FRAMES,
                          CP_PUFFLE_TOY_H, FORMAT_NATIVE);
}

static bool cp_load_puffle_toy_icon(const char *asset)
{
    char path[MAX_PATH];

    rb->snprintf(path, sizeof(path), CP_PUFFLE_TOY_ICON_PATTERN, asset);
    return cp_load_bitmap(path, &game.toy_icon, puffle_hat_pixels,
                          sizeof(puffle_hat_pixels), CP_PUFFLE_HAT_W,
                          CP_PUFFLE_HAT_H, FORMAT_NATIVE);
}

static bool cp_load_puffle_hat_asset(const char *asset)
{
    char path[MAX_PATH];

    rb->snprintf(path, sizeof(path), CP_PUFFLE_HAT_PATTERN, asset);
    return cp_load_bitmap(path, &game.puffle_hat, puffle_hat_pixels,
                          sizeof(puffle_hat_pixels),
                          CP_PUFFLE_HAT_W * CP_PUFFLE_HAT_FRAMES,
                          CP_PUFFLE_HAT_H, FORMAT_NATIVE);
}

static bool cp_puffle_is_owned(int type)
{
    return type >= 0 && type < CP_PUFFLE_TYPES &&
           (game.puffle_owned_mask & (1u << type)) != 0;
}

static bool cp_load_assets(void)
{
    bool scene_ok;
    bool player_ok;
    bool toolbar_ok;
    bool puffle_ok;

    scene_ok = cp_load_scene(CP_WORLD_FILE, CP_WORLD_W, CP_WORLD_H);
    player_ok = cp_load_player_asset();
    toolbar_ok = cp_load_bitmap(CP_TOOLBAR_FILE, &game.toolbar,
                                toolbar_pixels, sizeof(toolbar_pixels),
                                CP_VIEW_W, CP_STATUS_H, FORMAT_NATIVE);
    puffle_ok = cp_load_puffle_asset(0);
    game.assets_loaded = scene_ok && player_ok && toolbar_ok && puffle_ok;
    return game.assets_loaded;
}

static int cp_find_room(const char *id)
{
    int i;

    for (i = 0; i < game.room_count; i++)
    {
        if (cp_streq(game.rooms[i].id, id))
            return i;
    }

    return -1;
}

static int cp_nearest_hotspot(void)
{
    int i;
    int best = 0;
    int best_score;

    if (game.hotspot_count <= 0)
        return -1;

    best_score = cp_dist_score(game.x, game.y, game.hotspots[0].x,
                               game.hotspots[0].y);

    for (i = 1; i < game.hotspot_count; i++)
    {
        int score = cp_dist_score(game.x, game.y, game.hotspots[i].x,
                                  game.hotspots[i].y);
        if (score < best_score)
        {
            best = i;
            best_score = score;
        }
    }

    return best;
}

static void cp_select_map_direction(int dx, int dy)
{
    int current = game.selected_hotspot;
    int best = -1;
    int best_score = 0;
    int i;

    if (game.hotspot_count <= 0)
        return;
    if (current < 0 || current >= game.hotspot_count)
        current = 0;

    for (i = 0; i < game.hotspot_count; i++)
    {
        int delta_x;
        int delta_y;
        int forward;
        int side;
        int score;

        if (i == current)
            continue;

        delta_x = game.hotspots[i].x - game.hotspots[current].x;
        delta_y = game.hotspots[i].y - game.hotspots[current].y;
        forward = delta_x * dx + delta_y * dy;
        if (forward <= 0)
            continue;

        side = cp_abs(delta_x * dy - delta_y * dx);
        score = side * 4 + forward;
        if (best < 0 || score < best_score)
        {
            best = i;
            best_score = score;
        }
    }

    if (best < 0)
    {
        best = current;
        for (i = 0; i < game.hotspot_count; i++)
        {
            int coordinate = dx != 0 ? game.hotspots[i].x :
                                       game.hotspots[i].y;
            int best_coordinate = dx != 0 ? game.hotspots[best].x :
                                            game.hotspots[best].y;
            if ((dx + dy > 0 && coordinate < best_coordinate) ||
                (dx + dy < 0 && coordinate > best_coordinate))
                best = i;
        }
    }

    game.selected_hotspot = best;
    game.x = game.hotspots[best].x;
    game.y = game.hotspots[best].y;
    if (dx < 0)
        game.direction = CP_DIR_LEFT;
    else if (dx > 0)
        game.direction = CP_DIR_RIGHT;
    else if (dy < 0)
        game.direction = CP_DIR_UP;
    else if (dy > 0)
        game.direction = CP_DIR_DOWN;
    game.anim_frame = (game.anim_frame + 1) % CP_PLAYER_ANIM_FRAMES;
    game.map_x = game.x;
    game.map_y = game.y;
    game.dirty = true;
}

static bool cp_inside_hotspot(int index)
{
    int score;
    int radius;

    if (index < 0 || index >= game.hotspot_count)
        return false;

    score = cp_dist_score(game.x, game.y, game.hotspots[index].x,
                          game.hotspots[index].y);
    radius = game.hotspots[index].radius;

    return score <= radius * radius;
}

static bool cp_inside_room_door(int index)
{
    int closest_x;
    int closest_y;
    int dx;
    int dy;
    int radius;

    if (index < 0 || index >= game.hotspot_count ||
        game.hotspots[index].action != CP_ACTION_ROOM)
        return false;

    /* Door contact follows the penguin's compact walking body.  Clothing
     * and the wide side-facing artwork must not pull the player through a
     * nearby room from the middle of the floor. */
    closest_x = MIN(MAX(game.hotspots[index].x,
                        game.x - CP_DOOR_BODY_HALF_W),
                    game.x + CP_DOOR_BODY_HALF_W);
    closest_y = MIN(MAX(game.hotspots[index].y,
                        game.y - CP_DOOR_BODY_HALF_H),
                    game.y + CP_DOOR_BODY_HALF_H);
    dx = game.hotspots[index].x - closest_x;
    dy = game.hotspots[index].y - closest_y;
    radius = MIN(game.hotspots[index].radius, CP_DOOR_MAX_RADIUS);
    return dx * dx + dy * dy <= radius * radius;
}

static void cp_get_walk_bounds(int *left, int *top, int *right, int *bottom)
{
    if (game.scene_type == CP_SCENE_ROOM && game.room_index >= 0 &&
        game.room_index < game.room_count)
    {
        const struct cp_room *room = &game.rooms[game.room_index];

        *left = MAX(CP_PLAYER_W / 2, room->walk_left);
        *top = MAX(CP_PLAYER_H / 2, room->walk_top);
        *right = MIN(game.scene.width - CP_PLAYER_W / 2,
                     room->walk_right);
        *bottom = MIN(game.scene.height - CP_PLAYER_H / 2,
                      room->walk_bottom);
    }
    else
    {
        *left = CP_PLAYER_W / 2;
        *top = CP_PLAYER_H / 2;
        *right = game.scene.width - CP_PLAYER_W / 2;
        *bottom = game.scene.height - CP_PLAYER_H / 2;
    }
}

static void cp_clamp_player(void)
{
    int left;
    int top;
    int right;
    int bottom;

    cp_get_walk_bounds(&left, &top, &right, &bottom);

    game.x = MIN(MAX(game.x, left), right);
    game.y = MIN(MAX(game.y, top), bottom);
}

static void cp_clamp_walk_target(void)
{
    int left;
    int top;
    int right;
    int bottom;

    cp_get_walk_bounds(&left, &top, &right, &bottom);
    game.target_x = MIN(MAX(game.target_x, left), right);
    game.target_y = MIN(MAX(game.target_y, top), bottom);
    game.target_x = MIN(MAX(game.target_x, game.x - CP_WALK_AHEAD),
                        game.x + CP_WALK_AHEAD);
    game.target_y = MIN(MAX(game.target_y, game.y - CP_WALK_AHEAD),
                        game.y + CP_WALK_AHEAD);
}

static void cp_update_camera(void)
{
    game.cam_x = game.x - CP_VIEW_W / 2;
    game.cam_y = game.y - CP_VIEW_H / 2;

    if (game.cam_x < 0)
        game.cam_x = 0;
    if (game.cam_y < 0)
        game.cam_y = 0;
    if (game.cam_x > game.scene.width - CP_VIEW_W)
        game.cam_x = MAX(0, game.scene.width - CP_VIEW_W);
    if (game.cam_y > game.scene.height - CP_VIEW_H)
        game.cam_y = MAX(0, game.scene.height - CP_VIEW_H);
}

static void cp_set_current_hotspots(const struct cp_hotspot *hotspots,
                                    int count)
{
    int i;

    if (count > CP_MAX_HOTSPOTS)
        count = CP_MAX_HOTSPOTS;

    game.hotspot_count = count;
    for (i = 0; i < count; i++)
        game.hotspots[i] = hotspots[i];

    game.selected_hotspot = cp_nearest_hotspot();
}

static bool cp_enter_map_at(int x, int y)
{
    if (sound.started || sound.buffer_acquired)
        cp_sound_close_audio();
    if (!cp_load_scene(CP_WORLD_FILE, CP_WORLD_W, CP_WORLD_H))
        return false;

    game.scene_type = CP_SCENE_MAP;
    game.room_index = -1;
    cp_copy(game.location, sizeof(game.location), "Island Map");
    cp_set_current_hotspots(game.map_hotspots, game.map_hotspot_count);
    game.x = x;
    game.y = y;
    cp_clamp_player();
    game.target_x = game.x;
    game.target_y = game.y;
    cp_update_camera();
    game.selected_hotspot = cp_nearest_hotspot();
    if (game.selected_hotspot >= 0)
    {
        game.x = game.hotspots[game.selected_hotspot].x;
        game.y = game.hotspots[game.selected_hotspot].y;
        game.map_x = game.x;
        game.map_y = game.y;
    }
    game.dirty = true;
    return true;
}

static bool cp_show_welcome_page(int page)
{
    char path[MAX_PATH];
    int room_index = cp_find_room("welcome");

    if (page < 0 || page > 2 || room_index < 0)
        return false;
    rb->snprintf(path, sizeof(path), CP_WELCOME_PATTERN, page);
    if (!cp_load_scene(path, CP_VIEW_W, CP_VIEW_H))
        return false;

    game.scene_type = CP_SCENE_WELCOME;
    game.room_index = room_index;
    game.welcome_page = page;
    game.x = game.rooms[room_index].start_x;
    game.y = game.rooms[room_index].start_y;
    game.target_x = game.x;
    game.target_y = game.y;
    game.cam_x = 0;
    game.cam_y = 0;
    game.hotspot_count = 0;
    game.selected_hotspot = -1;
    cp_copy(game.location, sizeof(game.location), "Welcome Solo");
    cp_set_message("Select to continue the original first-login guide.");
    game.dirty = true;
    return true;
}

static void cp_advance_welcome(void)
{
    if (game.scene_type != CP_SCENE_WELCOME)
        return;
    if (game.welcome_page < 2)
    {
        if (!cp_show_welcome_page(game.welcome_page + 1))
            cp_set_message("Welcome guide assets are incomplete.");
        return;
    }

    game.welcome_complete = true;
    if (cp_enter_map_at(game.map_x, game.map_y))
    {
        cp_set_message("Welcome to Club Penguin.");
        cp_write_save();
    }
}

static void cp_set_room_hotspots(void)
{
    const char *room_id = game.rooms[game.room_index].id;
    struct cp_hotspot *back;
    int slot;
    int type;

    game.hotspot_count = 0;
    cp_load_room_interactions(room_id);

    if (cp_streq(room_id, "backyard"))
    {
        for (type = 0; type < CP_PUFFLE_TYPES &&
             game.hotspot_count < CP_MAX_HOTSPOTS - 1; type++)
        {
            struct cp_hotspot *puffle;

            if (!cp_puffle_is_owned(type) ||
                (game.puffle_walking && type == game.puffle_type))
                continue;
            puffle = &game.hotspots[game.hotspot_count++];
            rb->memset(puffle, 0, sizeof(*puffle));
            rb->snprintf(puffle->id, sizeof(puffle->id), "puffle_%s",
                         puffle_ids[type]);
            rb->snprintf(puffle->name, sizeof(puffle->name), "%s Puffle",
                         puffle_names[type]);
            rb->snprintf(puffle->detail, sizeof(puffle->detail),
                         "Care for your %s Puffle.", puffle_names[type]);
            rb->snprintf(puffle->target, sizeof(puffle->target), "care_%s",
                         puffle_ids[type]);
            puffle->action = CP_ACTION_PUFFLE;
            puffle->x = backyard_puffle_x[type];
            puffle->y = backyard_puffle_y[type];
            puffle->radius = 20;
        }
    }

    if (cp_streq(room_id, "player_home"))
    {
        for (slot = 0; slot < CP_MAX_FURNITURE_PLACEMENTS; slot++)
        {
            struct cp_furniture_placement *placement =
                &game.furniture_placements[slot];
            struct cp_hotspot *portal;

            if (placement->item_id != CP_PORTAL_BOX_ID ||
                game.hotspot_count >= CP_MAX_HOTSPOTS - 1)
                continue;
            portal = &game.hotspots[game.hotspot_count++];
            rb->memset(portal, 0, sizeof(*portal));
            cp_copy(portal->id, sizeof(portal->id), "portal_box");
            cp_copy(portal->name, sizeof(portal->name), "Portal Box");
            cp_copy(portal->detail, sizeof(portal->detail),
                    "Enter the Box Dimension.");
            cp_copy(portal->target, sizeof(portal->target),
                    "box_dimension");
            portal->action = CP_ACTION_ROOM;
            portal->x = placement->x;
            portal->y = placement->y - CP_FURNITURE_H / 2;
            portal->radius = 28;
            portal->to_x = 242;
            portal->to_y = 66;
            break;
        }
    }

    back = &game.hotspots[game.hotspot_count++];
    rb->memset(back, 0, sizeof(*back));
    cp_copy(back->id, sizeof(back->id), "map_exit");
    cp_copy(back->name, sizeof(back->name), "Map");
    if (cp_streq(room_id, "box_dimension"))
        cp_copy(back->detail, sizeof(back->detail),
                "Step through the portal to the island map.");
    else
        cp_copy(back->detail, sizeof(back->detail),
                "Return to the island map.");
    cp_copy(back->target, sizeof(back->target), "map");
    back->action = CP_ACTION_MAP;
    if (cp_streq(room_id, "box_dimension"))
    {
        back->x = 254;
        back->y = 59;
        back->radius = 28;
    }
    else
    {
        back->x = game.rooms[game.room_index].walk_left;
        back->y = game.rooms[game.room_index].walk_bottom;
        back->radius = 24;
    }
    back->to_x = game.map_x;
    back->to_y = game.map_y;

    game.selected_hotspot = cp_nearest_hotspot();
}

static const char *cp_igloo_catalog_file(int tab)
{
    if (tab == 0)
        return CP_IGLOO_BUILDING_DATA_FILE;
    if (tab == 1)
        return CP_IGLOO_FLOOR_DATA_FILE;
    return CP_IGLOO_LOCATION_DATA_FILE;
}

static bool cp_parse_igloo_line(char *line, struct cp_igloo_item *item)
{
    char *cursor = line;
    char *id_text = cp_next_field(&cursor);
    char *name = cp_next_field(&cursor);
    char *cost_text = cp_next_field(&cursor);
    char *auxiliary_text = cp_next_field(&cursor);

    if (!cp_has_text(name) || !cp_parse_int(id_text, &item->id) ||
        !cp_parse_int(cost_text, &item->cost) || item->id < 0)
        return false;
    item->auxiliary = 0;
    cp_parse_int(auxiliary_text, &item->auxiliary);
    cp_copy(item->name, sizeof(item->name), name);
    return true;
}

static bool cp_read_igloo_item(int tab, int wanted,
                               struct cp_igloo_item *item)
{
    char line[CP_IGLOO_LINE];
    int index = 0;
    int fd = rb->open(cp_igloo_catalog_file(tab), O_RDONLY);

    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        if (!cp_data_line(line))
            continue;
        if (index == wanted)
        {
            bool parsed = cp_parse_igloo_line(line, item);
            rb->close(fd);
            return parsed;
        }
        index++;
    }
    rb->close(fd);
    return false;
}

static bool cp_find_igloo_item(int tab, int item_id,
                               struct cp_igloo_item *item)
{
    int i;

    for (i = 0; i < game.igloo_catalog_count[tab]; i++)
    {
        if (!cp_read_igloo_item(tab, i, item))
            return false;
        if (item->id == item_id)
            return true;
    }
    return false;
}

static bool cp_load_igloo_catalogs(void)
{
    char line[CP_IGLOO_LINE];
    int tab;

    for (tab = 0; tab < CP_IGLOO_TABS; tab++)
    {
        int fd = rb->open(cp_igloo_catalog_file(tab), O_RDONLY);

        if (fd < 0)
            return false;
        game.igloo_catalog_count[tab] = 0;
        while (rb->read_line(fd, line, sizeof(line)) > 0)
        {
            if (cp_data_line(line))
                game.igloo_catalog_count[tab]++;
        }
        rb->close(fd);
        if (game.igloo_catalog_count[tab] <= 0)
            return false;
    }
    return true;
}

static bool cp_igloo_item_owned(int tab, int item_id)
{
    if (item_id == 0)
        return true;
    if (tab == 0)
    {
        if (item_id < 0 || item_id >= 128)
            return false;
        return (game.igloo_building_owned[item_id / 32] &
                (1u << (item_id % 32))) != 0;
    }
    if (item_id < 0 || item_id >= 32)
        return false;
    if (tab == 1)
        return (game.igloo_floor_owned & (1u << item_id)) != 0;
    return (game.igloo_location_owned & (1u << item_id)) != 0;
}

static void cp_own_igloo_item(int tab, int item_id)
{
    if (item_id <= 0)
        return;
    if (tab == 0 && item_id < 128)
        game.igloo_building_owned[item_id / 32] |=
            1u << (item_id % 32);
    else if (tab == 1 && item_id < 32)
        game.igloo_floor_owned |= 1u << item_id;
    else if (tab == 2 && item_id < 32)
        game.igloo_location_owned |= 1u << item_id;
}

static int cp_current_igloo_item(int tab)
{
    if (tab == 0)
        return game.igloo_building;
    if (tab == 1)
        return game.igloo_floor;
    return game.igloo_location;
}

static void cp_set_current_igloo_item(int tab, int item_id)
{
    if (tab == 0)
        game.igloo_building = item_id;
    else if (tab == 1)
        game.igloo_floor = item_id;
    else
        game.igloo_location = item_id;
}

static bool cp_parse_furniture_line(char *line,
                                    struct cp_furniture_item *item)
{
    char *cursor = line;
    char *id_text = cp_next_field(&cursor);
    char *name = cp_next_field(&cursor);
    char *cost_text = cp_next_field(&cursor);
    char *type_text = cp_next_field(&cursor);
    char *member_text = cp_next_field(&cursor);
    char *maximum_text = cp_next_field(&cursor);
    int member;

    if (!cp_has_text(name) || !cp_parse_int(id_text, &item->id) ||
        !cp_parse_int(cost_text, &item->cost) ||
        !cp_parse_int(type_text, &item->type) ||
        !cp_parse_int(member_text, &member) || item->id <= 0)
        return false;
    item->max_quantity = 99;
    cp_parse_int(maximum_text, &item->max_quantity);
    item->max_quantity = MIN(MAX(item->max_quantity, 1), 99);
    cp_copy(item->name, sizeof(item->name), name);
    item->member_only = member != 0;
    return true;
}

static bool cp_parse_exact_furniture_line(
    char *line, int *page, struct cp_furniture_item *item)
{
    char *cursor = line;
    char *page_text = cp_next_field(&cursor);
    char *id_text = cp_next_field(&cursor);
    char *name = cp_next_field(&cursor);
    char *cost_text = cp_next_field(&cursor);
    char *type_text = cp_next_field(&cursor);
    char *member_text = cp_next_field(&cursor);
    char *maximum_text = cp_next_field(&cursor);
    int member;

    if (!cp_parse_int(page_text, page) || *page < 1 ||
        *page > CP_FURNITURE_CATALOG_PAGES || !cp_has_text(name) ||
        !cp_parse_int(id_text, &item->id) || item->id <= 0 ||
        !cp_parse_int(cost_text, &item->cost) ||
        !cp_parse_int(type_text, &item->type) ||
        !cp_parse_int(member_text, &member))
        return false;
    item->max_quantity = 99;
    cp_parse_int(maximum_text, &item->max_quantity);
    item->max_quantity = MIN(MAX(item->max_quantity, 1), 99);
    item->member_only = member != 0;
    cp_copy(item->name, sizeof(item->name), name);
    return true;
}

static bool cp_read_exact_furniture_item(
    int page, int wanted, struct cp_furniture_item *item)
{
    char line[CP_FURNITURE_LINE];
    int index = 0;
    int fd = rb->open(CP_APR_FURNITURE_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        struct cp_furniture_item candidate;
        int candidate_page;

        if (!cp_data_line(line) ||
            !cp_parse_exact_furniture_line(line, &candidate_page,
                                           &candidate) ||
            candidate_page != page)
            continue;
        if (index == wanted)
        {
            *item = candidate;
            rb->close(fd);
            return true;
        }
        index++;
    }
    rb->close(fd);
    return false;
}

static int cp_count_exact_furniture_items(int page)
{
    char line[CP_FURNITURE_LINE];
    int count = 0;
    int fd = rb->open(CP_APR_FURNITURE_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return -1;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        struct cp_furniture_item item;
        int candidate_page;

        if (cp_data_line(line) &&
            cp_parse_exact_furniture_line(line, &candidate_page, &item) &&
            candidate_page == page)
            count++;
    }
    rb->close(fd);
    return count;
}

static bool cp_parse_pet_furniture_line(
    char *line, int *page, bool *secret, struct cp_furniture_item *item)
{
    char *cursor = line;
    char *page_text = cp_next_field(&cursor);
    char *secret_text = cp_next_field(&cursor);
    char *id_text = cp_next_field(&cursor);
    char *name = cp_next_field(&cursor);
    char *cost_text = cp_next_field(&cursor);
    char *type_text = cp_next_field(&cursor);
    char *member_text = cp_next_field(&cursor);
    char *maximum_text = cp_next_field(&cursor);
    int secret_value;
    int member;

    if (!cp_parse_int(page_text, page) || *page < 1 ||
        *page > CP_PET_FURNITURE_PAGES ||
        !cp_parse_int(secret_text, &secret_value) ||
        !cp_parse_int(id_text, &item->id) || item->id <= 0 ||
        !cp_has_text(name) || !cp_parse_int(cost_text, &item->cost) ||
        !cp_parse_int(type_text, &item->type) ||
        !cp_parse_int(member_text, &member))
        return false;
    item->max_quantity = 99;
    cp_parse_int(maximum_text, &item->max_quantity);
    item->max_quantity = MIN(MAX(item->max_quantity, 1), 99);
    item->member_only = member != 0;
    *secret = secret_value != 0;
    cp_copy(item->name, sizeof(item->name), name);
    return true;
}

static bool cp_read_pet_furniture_item(
    int page, bool secret, int wanted, struct cp_furniture_item *item)
{
    char line[CP_FURNITURE_LINE];
    int index = 0;
    int fd = rb->open(CP_PET_FURNITURE_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        struct cp_furniture_item candidate;
        int candidate_page;
        bool candidate_secret;

        if (!cp_data_line(line) ||
            !cp_parse_pet_furniture_line(line, &candidate_page,
                                         &candidate_secret, &candidate) ||
            candidate_page != page || candidate_secret != secret)
            continue;
        if (index == wanted)
        {
            *item = candidate;
            rb->close(fd);
            return true;
        }
        index++;
    }
    rb->close(fd);
    return false;
}

static int cp_count_pet_furniture_items(int page, bool secret)
{
    char line[CP_FURNITURE_LINE];
    int count = 0;
    int fd = rb->open(CP_PET_FURNITURE_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return -1;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        struct cp_furniture_item item;
        int candidate_page;
        bool candidate_secret;

        if (cp_data_line(line) &&
            cp_parse_pet_furniture_line(line, &candidate_page,
                                        &candidate_secret, &item) &&
            candidate_page == page && candidate_secret == secret)
            count++;
    }
    rb->close(fd);
    return count;
}

static bool cp_parse_exact_igloo_line(char *line, int *page, int *tab,
                                      struct cp_igloo_item *item)
{
    char *cursor = line;
    char *page_text = cp_next_field(&cursor);
    char *kind = cp_next_field(&cursor);
    char *id_text = cp_next_field(&cursor);
    char *name = cp_next_field(&cursor);
    char *cost_text = cp_next_field(&cursor);

    if (!cp_parse_int(page_text, page) || *page < 1 ||
        *page > CP_IGLOO_BOOK_PAGES || !cp_has_text(name) ||
        !cp_parse_int(id_text, &item->id) || item->id < 0 ||
        !cp_parse_int(cost_text, &item->cost))
        return false;
    if (cp_streq(kind, "building"))
        *tab = 0;
    else if (cp_streq(kind, "floor"))
        *tab = 1;
    else
        return false;
    item->auxiliary = 0;
    cp_copy(item->name, sizeof(item->name), name);
    return true;
}

static bool cp_read_exact_igloo_item(int page, int wanted, int *tab,
                                     struct cp_igloo_item *item)
{
    char line[CP_IGLOO_LINE];
    int index = 0;
    int fd = rb->open(CP_FEB_IGLOO_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        struct cp_igloo_item candidate;
        int candidate_page;
        int candidate_tab;

        if (!cp_data_line(line) ||
            !cp_parse_exact_igloo_line(line, &candidate_page,
                                       &candidate_tab, &candidate) ||
            candidate_page != page)
            continue;
        if (index == wanted)
        {
            *item = candidate;
            *tab = candidate_tab;
            rb->close(fd);
            return true;
        }
        index++;
    }
    rb->close(fd);
    return false;
}

static int cp_count_exact_igloo_items(int page)
{
    char line[CP_IGLOO_LINE];
    int count = 0;
    int fd = rb->open(CP_FEB_IGLOO_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return -1;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        struct cp_igloo_item item;
        int candidate_page;
        int candidate_tab;

        if (cp_data_line(line) &&
            cp_parse_exact_igloo_line(line, &candidate_page,
                                      &candidate_tab, &item) &&
            candidate_page == page)
            count++;
    }
    rb->close(fd);
    return count;
}

static bool cp_read_furniture_item(int wanted,
                                   struct cp_furniture_item *item)
{
    char line[CP_FURNITURE_LINE];
    int index = 0;
    int fd = rb->open(CP_FURNITURE_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        struct cp_furniture_item candidate;

        if (!cp_data_line(line))
            continue;
        if (!cp_parse_furniture_line(line, &candidate) ||
            candidate.type != game.furniture_catalog_type)
            continue;
        if (index == wanted)
        {
            *item = candidate;
            rb->close(fd);
            return true;
        }
        index++;
    }
    rb->close(fd);
    return false;
}

static bool cp_load_furniture_catalog(void)
{
    char line[CP_FURNITURE_LINE];
    int fd = rb->open(CP_FURNITURE_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return false;
    game.furniture_catalog_type = 1;
    game.furniture_catalog_count = 0;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        struct cp_furniture_item item;

        if (cp_data_line(line) && cp_parse_furniture_line(line, &item) &&
            item.type == game.furniture_catalog_type)
            game.furniture_catalog_count++;
    }
    rb->close(fd);
    game.furniture_catalog_index = 0;
    return game.furniture_catalog_count > 0 &&
           cp_read_furniture_item(0, &game.furniture_item);
}

static int cp_furniture_quantity(int item_id)
{
    unsigned char quantity = 0;
    int fd;

    if (item_id <= 0 || item_id > 99999)
        return 0;
    fd = rb->open(CP_FURNITURE_INVENTORY_FILE, O_RDONLY);
    if (fd < 0)
        return 0;
    if (rb->lseek(fd, item_id, SEEK_SET) >= 0)
        rb->read(fd, &quantity, sizeof(quantity));
    rb->close(fd);
    return quantity;
}

static bool cp_set_furniture_quantity(int item_id, int quantity)
{
    unsigned char stored;
    int fd;
    int wrote;

    if (item_id <= 0 || item_id > 99999)
        return false;
    stored = MIN(MAX(quantity, 0), 99);
    fd = rb->open(CP_FURNITURE_INVENTORY_FILE,
                  O_RDWR | O_CREAT, 0666);
    if (fd < 0)
        return false;
    if (rb->lseek(fd, item_id, SEEK_SET) < 0)
    {
        rb->close(fd);
        return false;
    }
    wrote = rb->write(fd, &stored, sizeof(stored));
    rb->close(fd);
    return wrote == (int)sizeof(stored);
}

static int cp_furniture_placed_count(int item_id)
{
    int count = 0;
    int i;

    for (i = 0; i < CP_MAX_FURNITURE_PLACEMENTS; i++)
    {
        if (game.furniture_placements[i].item_id == item_id)
            count++;
    }
    return count;
}

static void cp_migrate_furniture_inventory(void)
{
    int i;

    for (i = 0; i < CP_MAX_FURNITURE_PLACEMENTS; i++)
    {
        int item_id = game.furniture_placements[i].item_id;
        int placed;
        int j;

        if (item_id <= 0)
            continue;
        for (j = 0; j < i; j++)
        {
            if (game.furniture_placements[j].item_id == item_id)
                break;
        }
        if (j < i)
            continue;
        placed = cp_furniture_placed_count(item_id);
        if (cp_furniture_quantity(item_id) < placed)
            cp_set_furniture_quantity(item_id, placed);
    }
}

static int cp_find_furniture_slot(int item_id)
{
    int i;

    for (i = 0; i < CP_MAX_FURNITURE_PLACEMENTS; i++)
    {
        if (game.furniture_placements[i].item_id == item_id)
            return i;
    }
    return -1;
}

static unsigned int cp_bmp_le16(const unsigned char *value)
{
    return (unsigned int)value[0] | ((unsigned int)value[1] << 8);
}

static unsigned int cp_bmp_le32(const unsigned char *value)
{
    return (unsigned int)value[0] | ((unsigned int)value[1] << 8) |
           ((unsigned int)value[2] << 16) | ((unsigned int)value[3] << 24);
}

static bool cp_read_exact(int fd, void *buffer, int size)
{
    unsigned char *destination = buffer;
    int total = 0;

    while (total < size)
    {
        int count = rb->read(fd, destination + total, size - total);

        if (count <= 0)
            return false;
        total += count;
    }
    return true;
}

static bool cp_overlay_bmp(const char *path, const char *mask_path)
{
    unsigned char header[54];
    unsigned char mask_header[CP_IGLOO_MASK_HEADER];
    unsigned char *row = (unsigned char *)text_buf;
    unsigned char *mask_row = row + CP_VIEW_W * 3;
    fb_data *destination = (fb_data *)game.scene.data;
    int mask_fd = -1;
    int fd = rb->open(path, O_RDONLY);
    unsigned int pixel_offset;
    int row_size;
    int width = 0;
    int height = 0;
    int y;
    bool ok = false;

    if (fd < 0 || !cp_read_exact(fd, header, sizeof(header)) ||
        header[0] != 'B' || header[1] != 'M')
        goto done;
    width = (int)cp_bmp_le32(header + 18);
    height = (int)cp_bmp_le32(header + 22);
    pixel_offset = cp_bmp_le32(header + 10);
    if (width != CP_VIEW_W || height != CP_VIEW_H ||
        cp_bmp_le16(header + 28) != 24 || cp_bmp_le32(header + 30) != 0)
        goto done;
    row_size = (width * 3 + 3) & ~3;
    if (row_size + CP_IGLOO_MASK_ROW > CP_TEXT_BUF)
        goto done;

    if (mask_path != NULL)
    {
        mask_fd = rb->open(mask_path, O_RDONLY);
        if (mask_fd < 0 ||
            !cp_read_exact(mask_fd, mask_header, sizeof(mask_header)) ||
            rb->memcmp(mask_header, "CPMASK1\n", sizeof(mask_header)) != 0)
            goto done;
    }

    for (y = 0; y < height; y++)
    {
        int x;

        if (rb->lseek(fd, pixel_offset + (height - 1 - y) * row_size,
                      SEEK_SET) < 0 || !cp_read_exact(fd, row, row_size))
            goto done;
        if (mask_fd >= 0 &&
            !cp_read_exact(mask_fd, mask_row, CP_IGLOO_MASK_ROW))
            goto done;
        for (x = 0; x < width; x++)
        {
            unsigned char blue;
            unsigned char green;
            unsigned char red;

            if (mask_fd >= 0 &&
                (mask_row[x / 8] & (1u << (7 - x % 8))) == 0)
                continue;
            blue = row[x * 3];
            green = row[x * 3 + 1];
            red = row[x * 3 + 2];
            if (red == 255 && green == 0 && blue == 255)
                continue;
            destination[y * game.scene.width + x] =
                LCD_RGBPACK(red, green, blue);
        }
    }
    ok = true;

done:
    if (mask_fd >= 0)
        rb->close(mask_fd);
    if (fd >= 0)
        rb->close(fd);
    return ok;
}

static bool cp_load_igloo_base(const char *path)
{
    game.scene.width = CP_VIEW_W;
    game.scene.height = CP_VIEW_H;
    game.scene.data = (unsigned char *)scene_pixels;
    rb->memset(scene_pixels, 0,
               CP_VIEW_W * CP_VIEW_H * (int)sizeof(fb_data));
    return cp_overlay_bmp(path, NULL);
}

static bool cp_compose_igloo(int building, int floor, int location)
{
    struct cp_igloo_item building_item;
    struct cp_igloo_item floor_item;
    char path[MAX_PATH];
    char mask_path[MAX_PATH];
    int floor_frame;

    rb->snprintf(path, sizeof(path), CP_IGLOO_LOCATION_PATTERN, location);
    if (!cp_load_igloo_base(path))
        return false;
    if (building > 0 && building != 81)
    {
        rb->snprintf(path, sizeof(path), CP_IGLOO_BUILDING_PATTERN,
                     building);
        if (!cp_overlay_bmp(path, NULL))
            return false;
    }
    if (floor <= 0 || building <= 0 || building == 81)
        return true;
    if (!cp_find_igloo_item(0, building, &building_item) ||
        !cp_find_igloo_item(1, floor, &floor_item) ||
        building_item.auxiliary <= 0 || floor_item.auxiliary <= 0)
        return false;
    floor_frame = MIN(building_item.auxiliary, floor_item.auxiliary);
    rb->snprintf(path, sizeof(path), CP_IGLOO_FLOOR_PATTERN,
                 floor, floor_frame);
    rb->snprintf(mask_path, sizeof(mask_path), CP_IGLOO_MASK_PATTERN,
                 building);
    return cp_overlay_bmp(path, mask_path);
}

static bool cp_compose_current_igloo(void)
{
    return cp_compose_igloo(game.igloo_building, game.igloo_floor,
                            game.igloo_location);
}

static bool cp_load_furniture_asset(int item_id)
{
    char path[MAX_PATH];

    rb->snprintf(path, sizeof(path), CP_FURNITURE_ITEM_PATTERN, item_id);
    return cp_load_bitmap(path, &game.furniture,
                          puffle_pixels, sizeof(puffle_pixels),
                          CP_FURNITURE_W, CP_FURNITURE_H, FORMAT_NATIVE);
}

static void cp_composite_loaded_furniture(int center_x, int bottom_y)
{
    fb_data transparent = LCD_RGBPACK(255, 0, 255);
    fb_data *source = (fb_data *)game.furniture.data;
    fb_data *destination = (fb_data *)game.scene.data;
    int left = center_x - CP_FURNITURE_W / 2;
    int top = bottom_y - CP_FURNITURE_H;
    int y;

    for (y = 0; y < CP_FURNITURE_H; y++)
    {
        int destination_y = top + y;
        int x;

        if (destination_y < 0 || destination_y >= CP_VIEW_H)
            continue;
        for (x = 0; x < CP_FURNITURE_W; x++)
        {
            int destination_x = left + x;
            fb_data pixel = source[y * game.furniture.width + x];

            if (destination_x >= 0 && destination_x < CP_VIEW_W &&
                pixel != transparent)
                destination[destination_y * game.scene.width +
                            destination_x] = pixel;
        }
    }
}

static bool cp_bake_furniture_except(int skip_slot)
{
    int i;

    for (i = 0; i < CP_MAX_FURNITURE_PLACEMENTS; i++)
    {
        struct cp_furniture_placement *placement =
            &game.furniture_placements[i];

        if (i == skip_slot || placement->item_id <= 0)
            continue;
        if (!cp_load_furniture_asset(placement->item_id))
            return false;
        cp_composite_loaded_furniture(placement->x, placement->y);
    }
    return true;
}

static bool cp_bake_igloo_furniture(void)
{
    return cp_bake_furniture_except(-1) &&
           cp_load_puffle_asset(game.puffle_type);
}

static void cp_composite_loaded_puffle(int center_x, int bottom_y)
{
    fb_data transparent = LCD_RGBPACK(255, 0, 255);
    fb_data *source = (fb_data *)game.puffle.data;
    fb_data *destination = (fb_data *)game.scene.data;
    int left = center_x - CP_PUFFLE_W / 2;
    int top = bottom_y - CP_PUFFLE_H;
    int y;

    for (y = 0; y < CP_PUFFLE_H; y++)
    {
        int destination_y = top + y;
        int x;

        if (destination_y < 0 || destination_y >= CP_VIEW_H)
            continue;
        for (x = 0; x < CP_PUFFLE_W; x++)
        {
            int destination_x = left + x;
            fb_data pixel = source[y * game.puffle.width + x];

            if (destination_x >= 0 && destination_x < CP_VIEW_W &&
                pixel != transparent)
                destination[destination_y * game.scene.width +
                            destination_x] = pixel;
        }
    }
}

static bool cp_bake_backyard_puffles(void)
{
    int type;

    for (type = 0; type < CP_PUFFLE_TYPES; type++)
    {
        if (!cp_puffle_is_owned(type) ||
            (game.puffle_walking && type == game.puffle_type))
            continue;
        if (!cp_load_puffle_walk(type))
            return false;
        cp_composite_loaded_puffle(backyard_puffle_x[type],
                                   backyard_puffle_y[type]);
    }
    return true;
}

static const char *cp_night_city_strip(const char *room_id)
{
    if (cp_streq(room_id, "town"))
        return CP_NIGHT_CITY_TOWN_STRIP;
    if (cp_streq(room_id, "night_club"))
        return CP_AFTERLIFE_FRAME_STRIP;
    if (cp_streq(room_id, "lounge"))
        return CP_AFTERLIFE_LOUNGE_STRIP;
    if (cp_streq(room_id, "coffee_shop"))
        return CP_NIGHT_CITY_COFFEE_STRIP;
    if (cp_streq(room_id, "plaza"))
        return CP_NIGHT_CITY_PLAZA_STRIP;
    if (cp_streq(room_id, "dock"))
        return CP_LAGUNA_BEND_DOCK_STRIP;
    if (cp_streq(room_id, "pizza_parlor"))
        return CP_BUCK_A_SLICE_STRIP;
    if (cp_streq(room_id, "lighthouse"))
        return CP_LAGUNA_COTTAGE_STRIP;
    if (cp_streq(room_id, "beacon"))
        return CP_LAGUNA_COTTAGE_ROOF_STRIP;
    if (cp_streq(room_id, "beach"))
        return CP_LAGUNA_BEND_SHORE_STRIP;
    if (cp_streq(room_id, "stadium"))
        return CP_MONTREAL_HOCKEY_STRIP;
    if (cp_streq(room_id, "snow_forts"))
        return CP_CLOCK_DISTRICT_STRIP;
    if (cp_streq(room_id, "velvet_ice"))
        return CP_VELVET_ICE_STRIP;
    if (cp_streq(room_id, "chrome_clinic"))
        return CP_CHROME_CLINIC_STRIP;
    if (cp_streq(room_id, "dojo_courtyard"))
        return CP_ARASAKA_HQ_EXTERIOR_STRIP;
    if (cp_streq(room_id, "dojo"))
        return CP_ARASAKA_HQ_LOBBY_STRIP;
    if (cp_streq(room_id, "forest"))
        return CP_MEMORIAL_PARK_STRIP;
    return NULL;
}

static bool cp_enter_room(int room_index, int x, int y)
{
    char path[MAX_PATH];
    struct cp_room *room;

    if (room_index < 0 || room_index >= game.room_count)
    {
        cp_set_message("Room metadata missing.");
        return false;
    }

    room = &game.rooms[room_index];
    game.animation_loaded = false;
    game.animation_frames = 0;
    if (cp_streq(room->id, "player_home"))
    {
        if (!cp_compose_current_igloo() || !cp_bake_igloo_furniture())
            return false;
    }
    else if (cp_streq(room->id, "backyard"))
    {
        int location = MIN(MAX(game.igloo_location, 1), 8);

        rb->snprintf(path, sizeof(path), CP_BACKYARD_PATTERN, location);
        if (!cp_load_scene(path, CP_VIEW_W, CP_VIEW_H) ||
            !cp_bake_backyard_puffles())
            return false;
    }
    else if (cp_streq(room->id, "stage") ||
             cp_streq(room->id, "emma_sewer"))
    {
        bool emma = cp_streq(room->id, "emma_sewer");
        const char *intro = emma ? CP_EMMA_CONCERT_INTRO_FILE :
                                   CP_CONCERT_INTRO_FILE;
        const char *strip = emma ? CP_EMMA_CONCERT_STRIP_FILE :
                                   CP_CONCERT_STRIP_FILE;

        if (!cp_load_scene(intro, CP_VIEW_W, CP_VIEW_H))
            return false;
        if (!cp_load_animation_strip(strip,
                                     CP_CONCERT_CACHE_FRAMES))
            return false;
        game.concert_frame = 0;
        game.concert_intro_ticks = emma ? 0 : CP_CONCERT_INTRO_TICKS;
        game.animation_start_tick = *rb->current_tick +
                                    CP_CONCERT_INTRO_TICKS;
        if (emma)
            game.animation_start_tick = *rb->current_tick;
        game.next_concert_tick = *rb->current_tick + 1;
    }
    else if (cp_streq(room->id, "town") ||
             cp_streq(room->id, "night_club") ||
             cp_streq(room->id, "lounge") ||
             cp_streq(room->id, "coffee_shop") ||
             cp_streq(room->id, "plaza") ||
             cp_streq(room->id, "dock") ||
             cp_streq(room->id, "pizza_parlor") ||
             cp_streq(room->id, "lighthouse") ||
             cp_streq(room->id, "beacon") ||
             cp_streq(room->id, "beach") ||
             cp_streq(room->id, "stadium") ||
             cp_streq(room->id, "snow_forts") ||
             cp_streq(room->id, "velvet_ice") ||
             cp_streq(room->id, "chrome_clinic") ||
             cp_streq(room->id, "dojo_courtyard") ||
             cp_streq(room->id, "dojo") ||
             cp_streq(room->id, "forest"))
    {
        const char *strip = cp_night_city_strip(room->id);

        game.city_frame = 0;
        game.animation_start_tick = *rb->current_tick;
        cp_join_asset_path(path, sizeof(path), room->bitmap);
        if (!cp_load_scene(path, CP_VIEW_W, CP_VIEW_H))
            return false;
        if (strip == NULL ||
            !cp_load_animation_strip(strip, CP_NIGHT_CITY_FRAMES))
            return false;
        game.next_city_tick = *rb->current_tick +
            MAX(1, HZ / CP_NIGHT_CITY_FRAME_RATE);
    }
    else
    {
        cp_join_asset_path(path, sizeof(path), room->bitmap);
        if (!cp_load_scene(path, CP_VIEW_W, CP_VIEW_H))
            return false;
    }
    if (game.puffle_owned_mask != 0 && game.puffle_walking)
    {
        if (!cp_load_puffle_walk(game.puffle_type))
            return false;
    }
    else if (!cp_load_puffle_asset(game.puffle_type))
        return false;

    game.scene_type = CP_SCENE_ROOM;
    game.room_index = room_index;
    cp_copy(game.location, sizeof(game.location), room->title);
    if (x <= 0)
        x = room->start_x;
    if (y <= 0)
        y = room->start_y;
    game.x = x;
    game.y = y;
    cp_clamp_player();
    game.target_x = game.x;
    game.target_y = game.y;
    game.next_walk_tick = *rb->current_tick;
    /* Room exits are commonly also the arrival point. Do not bounce back
     * through one until the penguin has first waddled clear of every door. */
    game.room_trigger_armed = false;
    cp_update_camera();
    cp_set_room_hotspots();
    if (!cp_music_switch_for_room(room->id))
    {
        if (cp_streq(room->id, "stage"))
            cp_set_message("Concert animation is live; audio is unavailable.");
        else
            cp_set_message("Room ambience is unavailable.");
    }
    game.selected_hotspot = cp_nearest_hotspot();
    game.dirty = true;
    return true;
}

static void cp_place_at_matching_door(int source_room_index)
{
    struct cp_room *room;
    const char *source_id;
    int i;

    if (game.scene_type != CP_SCENE_ROOM || game.room_index < 0 ||
        source_room_index < 0 || source_room_index >= game.room_count)
        return;

    room = &game.rooms[game.room_index];
    source_id = game.rooms[source_room_index].id;
    for (i = 0; i < game.hotspot_count; i++)
    {
        struct cp_hotspot *door = &game.hotspots[i];
        int dx;
        int dy;
        int clearance;

        if (door->action != CP_ACTION_ROOM ||
            !cp_streq(door->target, source_id))
            continue;

        dx = room->start_x - door->x;
        dy = room->start_y - door->y;
        if (cp_abs(dx) > cp_abs(dy))
        {
            clearance = MIN(door->radius, CP_DOOR_MAX_RADIUS) +
                        CP_DOOR_BODY_HALF_W + 1;
            game.x = door->x + (dx < 0 ? -clearance : clearance);
            game.y = door->y;
        }
        else
        {
            clearance = MIN(door->radius, CP_DOOR_MAX_RADIUS) +
                        CP_DOOR_BODY_HALF_H + 1;
            game.x = door->x;
            game.y = door->y + (dy < 0 ? -clearance : clearance);
        }
        cp_clamp_player();
        game.target_x = game.x;
        game.target_y = game.y;
        /* Matching-door placement is deliberately one pixel beyond exact
         * sprite/trigger contact, so the return door is safe to arm now. */
        game.room_trigger_armed = true;
        cp_update_camera();
        game.selected_hotspot = cp_nearest_hotspot();
        game.dirty = true;
        return;
    }
}

static int cp_clamp_save_value(int value, int maximum)
{
    if (value < 0)
        return 0;
    if (value > maximum)
        return maximum;
    return value;
}

static bool cp_load_puffle_need(const char *key, int value)
{
    char *end;
    unsigned long type;

    if (key[0] != 'p' || key[1] < '0' || key[1] > '9')
        return false;
    type = rb->strtoul(key + 1, &end, 10);
    if (type >= CP_PUFFLE_TYPES || end == key + 1)
        return false;
    value = cp_clamp_save_value(value, 100);
    if (cp_streq(end, "_food"))
        game.puffle_food[type] = value;
    else if (cp_streq(end, "_rest"))
        game.puffle_rest[type] = value;
    else if (cp_streq(end, "_happy"))
        game.puffle_happy[type] = value;
    else if (cp_streq(end, "_clean"))
        game.puffle_clean[type] = value;
    else
        return false;
    return true;
}

static bool cp_load_puffle_food_quantity(const char *key, int value)
{
    char *end;
    unsigned long item;

    if (key[0] != 'p' || key[1] != 'f' ||
        key[2] < '0' || key[2] > '9')
        return false;
    item = rb->strtoul(key + 2, &end, 10);
    if (item >= CP_PUFFLE_FOOD_ITEMS || end == key + 2 ||
        !cp_streq(end, "_qty"))
        return false;
    game.puffle_food_quantity[item] = cp_clamp_save_value(value, 99);
    return true;
}

static bool cp_load_puffle_hat_equipped(const char *key, int value)
{
    char *end;
    unsigned long type;

    if (key[0] != 'p' || key[1] != 'h' ||
        key[2] < '0' || key[2] > '9')
        return false;
    type = rb->strtoul(key + 2, &end, 10);
    if (type >= CP_PUFFLE_TYPES || end == key + 2 ||
        !cp_streq(end, "_hat"))
        return false;
    game.puffle_hat_equipped[type] =
        cp_clamp_save_value(value, 99999);
    return true;
}

static bool cp_load_furniture_field(const char *key, int value)
{
    char *end;
    unsigned long slot;
    struct cp_furniture_placement *placement;

    if (key[0] != 'f' || key[1] < '0' || key[1] > '9')
        return false;
    slot = rb->strtoul(key + 1, &end, 10);
    if (slot >= CP_MAX_FURNITURE_PLACEMENTS || end == key + 1)
        return false;
    placement = &game.furniture_placements[slot];
    if (cp_streq(end, "_id"))
        placement->item_id = cp_clamp_save_value(value, 99999);
    else if (cp_streq(end, "_x"))
        placement->x = value;
    else if (cp_streq(end, "_y"))
        placement->y = value;
    else
        return false;
    return true;
}

static void cp_load_save(void)
{
    char *cursor;
    char *line;

    cp_copy(game.saved_room, sizeof(game.saved_room), "map");
    game.saved_x = game.map_hotspots[0].x;
    game.saved_y = game.map_hotspots[0].y;

    if (cp_read_text_file(CP_SAVE_FILE) < 0)
        return;

    cursor = text_buf;
    line = cp_next_line(&cursor);
    if (line == NULL || !cp_streq(line, "CLUBPENGUIN_SAVE_V1"))
        return;

    /* Saves written before the Welcome Solo import belong to established
     * players. Only a genuinely new profile should see the one-time guide. */
    game.welcome_complete = true;

    while ((line = cp_next_line(&cursor)) != NULL)
    {
        char *equals = line;
        char *value;
        int parsed;

        while (*equals != '\0' && *equals != '=')
            equals++;
        if (*equals != '=')
            continue;
        *equals = '\0';
        value = equals + 1;

        if (cp_streq(line, "room"))
            cp_copy(game.saved_room, sizeof(game.saved_room), value);
        else if (cp_streq(line, "owned_lo"))
            game.owned_lo = (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_streq(line, "owned_hi"))
            game.owned_hi = (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_streq(line, "sport_owned"))
            game.sport_owned =
                (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_streq(line, "costume_owned"))
            game.costume_owned =
                (unsigned int)rb->strtoul(value, NULL, 16) & 0x7fffu;
        else if (cp_streq(line, "puffle_owned_mask"))
            game.puffle_owned_mask =
                (unsigned int)rb->strtoul(value, NULL, 16) &
                CP_PUFFLE_ALL_MASK;
        else if (cp_streq(line, "puffle_toy_owned"))
            game.puffle_toy_owned =
                (unsigned int)rb->strtoul(value, NULL, 16) &
                CP_PUFFLE_ALL_MASK;
        else if (cp_streq(line, "puffle_hat_owned0"))
            game.puffle_hat_owned[0] =
                (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_streq(line, "puffle_hat_owned1"))
            game.puffle_hat_owned[1] =
                (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_streq(line, "puffle_hat_owned2"))
            game.puffle_hat_owned[2] =
                (unsigned int)rb->strtoul(value, NULL, 16) & 0xfu;
        else if (cp_streq(line, "igloo_building_owned0"))
            game.igloo_building_owned[0] =
                (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_streq(line, "igloo_building_owned1"))
            game.igloo_building_owned[1] =
                (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_streq(line, "igloo_building_owned2"))
            game.igloo_building_owned[2] =
                (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_streq(line, "igloo_building_owned3"))
            game.igloo_building_owned[3] =
                (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_streq(line, "igloo_floor_owned"))
            game.igloo_floor_owned =
                (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_streq(line, "igloo_location_owned"))
            game.igloo_location_owned =
                (unsigned int)rb->strtoul(value, NULL, 16);
        else if (cp_parse_int(value, &parsed))
        {
            if (cp_streq(line, "coins"))
                game.coins = cp_clamp_save_value(parsed, 999999999);
            else if (cp_streq(line, "x"))
                game.saved_x = parsed;
            else if (cp_streq(line, "y"))
                game.saved_y = parsed;
            else if (cp_streq(line, "map_x"))
                game.map_x = parsed;
            else if (cp_streq(line, "map_y"))
                game.map_y = parsed;
            else if (cp_streq(line, "cart_best_score"))
                game.cart_best_score =
                    cp_clamp_save_value(parsed, 999999999);
            else if (cp_streq(line, "cart_best_combo"))
                game.cart_best_combo =
                    cp_clamp_save_value(parsed, 999999999);
            else if (cp_streq(line, "equipped_head"))
                game.equipped[0] = MIN(MAX(parsed, -1), 14);
            else if (cp_streq(line, "equipped_body"))
                game.equipped[1] = MIN(MAX(parsed, -1), 14);
            else if (cp_streq(line, "equipped_feet"))
                game.equipped[2] = MIN(MAX(parsed, -1), 14);
            else if (cp_streq(line, "equipped_color"))
                game.equipped[3] = MIN(MAX(parsed, -1), 14);
            else if (cp_streq(line, "equipped_face"))
                game.equipped[CP_FACE_PAGE] = MIN(MAX(parsed, -1), 14);
            else if (cp_streq(line, "sport_head"))
                game.sport_equipped[0] =
                    cp_clamp_save_value(parsed, 99999);
            else if (cp_streq(line, "sport_body"))
                game.sport_equipped[1] =
                    cp_clamp_save_value(parsed, 99999);
            else if (cp_streq(line, "sport_feet"))
                game.sport_equipped[2] =
                    cp_clamp_save_value(parsed, 99999);
            else if (cp_streq(line, "sport_hand"))
                game.sport_equipped[3] =
                    cp_clamp_save_value(parsed, 99999);
            else if (cp_streq(line, "clothing_neck"))
                game.sport_equipped[4] =
                    cp_clamp_save_value(parsed, 99999);
            else if (cp_streq(line, "clothing_face"))
                game.sport_equipped[5] =
                    cp_clamp_save_value(parsed, 99999);
            else if (cp_streq(line, "sport_background"))
                game.sport_background =
                    cp_clamp_save_value(parsed, 99999);
            else if (cp_streq(line, "clothing_flag"))
                game.clothing_flag =
                    cp_clamp_save_value(parsed, 99999);
            else if (cp_streq(line, "puffle_owned"))
                game.puffle_owned_mask |= parsed != 0 ? 1u : 0u;
            else if (cp_streq(line, "puffle_type"))
                game.puffle_type =
                    MIN(MAX(parsed, 0), CP_PUFFLE_TYPES - 1);
            else if (cp_streq(line, "puffle_walking"))
                game.puffle_walking = parsed != 0;
            else if (cp_streq(line, "welcome_complete"))
                game.welcome_complete = parsed != 0;
            else if (cp_streq(line, "igloo_building"))
                game.igloo_building = MIN(MAX(parsed, 0), 127);
            else if (cp_streq(line, "igloo_floor"))
                game.igloo_floor = MIN(MAX(parsed, 0), 31);
            else if (cp_streq(line, "igloo_location"))
                game.igloo_location = MIN(MAX(parsed, 1), 31);
            else if (cp_streq(line, "puffle_food"))
                game.puffle_food[0] = cp_clamp_save_value(parsed, 100);
            else if (cp_streq(line, "puffle_rest"))
                game.puffle_rest[0] = cp_clamp_save_value(parsed, 100);
            else if (cp_streq(line, "puffle_happy"))
                game.puffle_happy[0] = cp_clamp_save_value(parsed, 100);
            else if (cp_streq(line, "puffle_clean"))
                game.puffle_clean[0] = cp_clamp_save_value(parsed, 100);
            else if (cp_load_puffle_food_quantity(line, parsed))
            {
                /* Parsed by cp_load_puffle_food_quantity(). */
            }
            else if (cp_load_puffle_hat_equipped(line, parsed))
            {
                /* Parsed by cp_load_puffle_hat_equipped(). */
            }
            else if (cp_load_puffle_need(line, parsed))
            {
                /* Parsed by cp_load_puffle_need(). */
            }
            else if (cp_streq(line, "furniture_version"))
                game.furniture_save_version = parsed;
            else if (cp_streq(line, "furniture_mask"))
                game.legacy_furniture_mask = (unsigned int)parsed & 0xf;
            else if (cp_load_furniture_field(line, parsed))
            {
                /* Parsed by cp_load_furniture_field(). */
            }
        }
    }

    if (game.furniture_save_version < 2)
    {
        static const int legacy_ids[4] = { 21, 23, 24, 26 };
        int i;

        for (i = 0; i < 4; i++)
        {
            game.furniture_placements[i].item_id =
                (game.legacy_furniture_mask & (1u << i)) != 0 ?
                legacy_ids[i] : 0;
        }
    }
}

static bool cp_write_save(void)
{
    char save[4096];
    const char *room = "map";
    int save_x = game.x;
    int save_y = game.y;
    int length;
    int written;
    int i;
    int fd;
    int wrote;

    if (game.scene_type == CP_SCENE_ROOM && game.room_index >= 0 &&
        game.room_index < game.room_count)
        room = game.rooms[game.room_index].id;
    else if ((game.scene_type == CP_SCENE_SHOP ||
              game.scene_type == CP_SCENE_SPORT_SHOP ||
              game.scene_type == CP_SCENE_COSTUME_SHOP ||
              game.scene_type == CP_SCENE_PENGUIN_STYLE ||
              game.scene_type == CP_SCENE_NINJA_SHOP) &&
             game.shop_return_room >= 0 &&
             game.shop_return_room < game.room_count)
    {
        room = game.rooms[game.shop_return_room].id;
        save_x = game.shop_return_x;
        save_y = game.shop_return_y;
    }
    else if ((game.scene_type == CP_SCENE_PUFFLE_SHOP ||
              game.scene_type == CP_SCENE_PUFFLE_CARE ||
              game.scene_type == CP_SCENE_PUFFLE_FOOD ||
              game.scene_type == CP_SCENE_PUFFLE_TOYS ||
              game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
              game.scene_type == CP_SCENE_PUFFLE_HATS ||
              game.scene_type == CP_SCENE_PET_FURNITURE ||
              game.scene_type == CP_SCENE_IGLOO_EDIT ||
              game.scene_type == CP_SCENE_IGLOO_CATALOG ||
              game.scene_type == CP_SCENE_FURNITURE_CATALOG ||
              game.scene_type == CP_SCENE_IGLOO_BOOK) &&
             game.special_return_room >= 0 &&
             game.special_return_room < game.room_count)
    {
        room = game.rooms[game.special_return_room].id;
        save_x = game.special_return_x;
        save_y = game.special_return_y;
    }
    else if (game.scene_type >= CP_SCENE_CART_TITLE &&
             game.cart.return_room >= 0 &&
             game.cart.return_room < game.room_count)
    {
        room = game.rooms[game.cart.return_room].id;
        save_x = game.cart.return_x;
        save_y = game.cart.return_y;
    }

    length = rb->snprintf(save, sizeof(save),
                          "CLUBPENGUIN_SAVE_V1\n"
                          "coins=%d\nroom=%s\nx=%d\ny=%d\n"
                          "map_x=%d\nmap_y=%d\n"
                          "welcome_complete=%d\n"
                          "cart_best_score=%d\ncart_best_combo=%d\n"
                          "owned_lo=%08x\nowned_hi=%08x\n"
                          "sport_owned=%08x\n"
                          "costume_owned=%08x\n"
                          "equipped_head=%d\nequipped_body=%d\n"
                          "equipped_feet=%d\nequipped_color=%d\n"
                          "equipped_face=%d\n"
                          "sport_head=%d\nsport_body=%d\n"
                          "sport_feet=%d\nsport_hand=%d\n"
                          "clothing_neck=%d\nclothing_face=%d\n"
                          "sport_background=%d\n"
                          "clothing_flag=%d\n"
                          "puffle_owned=%d\npuffle_food=%d\n"
                          "puffle_rest=%d\npuffle_happy=%d\n"
                          "puffle_clean=%d\npuffle_owned_mask=%08x\n"
                          "puffle_type=%d\npuffle_walking=%d\n"
                          "puffle_toy_owned=%08x\n"
                          "puffle_hat_owned0=%08x\n"
                          "puffle_hat_owned1=%08x\n"
                          "puffle_hat_owned2=%08x\n"
                          "igloo_building=%d\nigloo_floor=%d\n"
                          "igloo_location=%d\n"
                          "igloo_building_owned0=%08x\n"
                          "igloo_building_owned1=%08x\n"
                          "igloo_building_owned2=%08x\n"
                          "igloo_building_owned3=%08x\n"
                          "igloo_floor_owned=%08x\n"
                          "igloo_location_owned=%08x\n"
                          "furniture_version=2\n",
                          game.coins, room, save_x, save_y,
                          game.map_x, game.map_y, game.welcome_complete,
                          game.cart_best_score, game.cart_best_combo,
                          game.owned_lo, game.owned_hi,
                          game.sport_owned,
                          game.costume_owned,
                          game.equipped[0], game.equipped[1],
                          game.equipped[2], game.equipped[3],
                          game.equipped[CP_FACE_PAGE],
                          game.sport_equipped[0], game.sport_equipped[1],
                          game.sport_equipped[2], game.sport_equipped[3],
                          game.sport_equipped[4],
                          game.sport_equipped[5],
                          game.sport_background,
                          game.clothing_flag,
                          game.puffle_owned_mask != 0,
                          game.puffle_food[0], game.puffle_rest[0],
                          game.puffle_happy[0], game.puffle_clean[0],
                          game.puffle_owned_mask, game.puffle_type,
                          game.puffle_walking,
                          game.puffle_toy_owned,
                          game.puffle_hat_owned[0],
                          game.puffle_hat_owned[1],
                          game.puffle_hat_owned[2], game.igloo_building,
                          game.igloo_floor, game.igloo_location,
                          game.igloo_building_owned[0],
                          game.igloo_building_owned[1],
                          game.igloo_building_owned[2],
                          game.igloo_building_owned[3],
                          game.igloo_floor_owned,
                          game.igloo_location_owned);
    if (length <= 0 || length >= (int)sizeof(save))
        return false;

    for (i = 0; i < CP_PUFFLE_TYPES; i++)
    {
        written = rb->snprintf(save + length, sizeof(save) - length,
                               "p%d_food=%d\np%d_rest=%d\n"
                               "p%d_happy=%d\np%d_clean=%d\n"
                               "ph%d_hat=%d\n",
                               i, game.puffle_food[i],
                               i, game.puffle_rest[i],
                               i, game.puffle_happy[i],
                               i, game.puffle_clean[i], i,
                               game.puffle_hat_equipped[i]);
        if (written <= 0 || written >= (int)sizeof(save) - length)
            return false;
        length += written;
    }

    for (i = 0; i < CP_PUFFLE_FOOD_ITEMS; i++)
    {
        written = rb->snprintf(save + length, sizeof(save) - length,
                               "pf%d_qty=%d\n", i,
                               game.puffle_food_quantity[i]);
        if (written <= 0 || written >= (int)sizeof(save) - length)
            return false;
        length += written;
    }

    for (i = 0; i < CP_MAX_FURNITURE_PLACEMENTS; i++)
    {
        struct cp_furniture_placement *placement =
            &game.furniture_placements[i];

        written = rb->snprintf(save + length, sizeof(save) - length,
                               "f%d_id=%d\nf%d_x=%d\nf%d_y=%d\n",
                               i, placement->item_id, i, placement->x,
                               i, placement->y);
        if (written <= 0 || written >= (int)sizeof(save) - length)
            return false;
        length += written;
    }

    fd = rb->open(CP_SAVE_TMP_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    wrote = rb->write(fd, save, length);
    rb->close(fd);
    if (wrote != length)
    {
        rb->remove(CP_SAVE_TMP_FILE);
        return false;
    }

    if (rb->rename(CP_SAVE_TMP_FILE, CP_SAVE_FILE) < 0)
    {
        rb->remove(CP_SAVE_TMP_FILE);
        return false;
    }

    return true;
}

static int cp_cart_trick_score(enum cp_cart_trick trick)
{
    switch (trick)
    {
        case CP_CART_TRICK_OLLIE: return 20;
        case CP_CART_TRICK_SPIN: return 80;
        case CP_CART_TRICK_FLAP: return 50;
        case CP_CART_TRICK_GRIND: return 80;
        default: return 0;
    }
}

static void cp_cart_award_trick(enum cp_cart_trick trick)
{
    int points = cp_cart_trick_score(trick);

    if (trick == CP_CART_TRICK_NONE || points <= 0)
        return;
    if (game.cart.last_trick == trick)
    {
        points /= 2;
        game.cart.combo = 1;
    }
    else
        game.cart.combo++;
    if (game.cart.combo > game.cart.run_best_combo)
        game.cart.run_best_combo = game.cart.combo;
    if (game.cart.score <= 999999999 - points)
        game.cart.score += points;
    else
        game.cart.score = 999999999;
    game.cart.last_trick = trick;
    game.cart.trick_cooldown = 10;
}

static void cp_cart_finish(void)
{
    int reward;

    game.scene_type = CP_SCENE_CART_RESULTS;
    reward = game.cart.score / 10;
    game.cart.reward = reward;
    if (!game.cart.rewarded)
    {
        if (game.coins <= 999999999 - reward)
            game.coins += reward;
        else
            game.coins = 999999999;
        game.cart.rewarded = true;
        if (game.cart.score > game.cart_best_score)
            game.cart_best_score = game.cart.score;
        if (game.cart.run_best_combo > game.cart_best_combo)
            game.cart_best_combo = game.cart.run_best_combo;
        cp_write_save();
    }
    game.dirty = true;
}

static void cp_cart_crash(void)
{
    if (game.scene_type == CP_SCENE_CART_CRASH ||
        game.scene_type == CP_SCENE_CART_RESULTS)
        return;

    game.cart.lives--;
    game.cart.sprite_frame = 5;
    game.cart.crash_ticks = 35;
    game.cart.combo = 0;
    game.scene_type = CP_SCENE_CART_CRASH;
    game.dirty = true;
}

static bool cp_cart_start(void)
{
    if (!cp_load_scene(CP_CART_TUNNEL_FILE, CP_VIEW_W, CP_VIEW_H))
        return false;
    if (!cp_load_bitmap(CP_CART_SPRITES_FILE, &game.player, player_pixels,
                        CP_PLAYER_BUFFER_BYTES, CP_CART_SHEET_W,
                        CP_CART_SHEET_H, FORMAT_NATIVE))
        return false;

    game.cart.score = 0;
    game.cart.lives = 4;
    game.cart.segment = 0;
    game.cart.segment_tick = 0;
    game.cart.jump_ticks = 0;
    game.cart.lean = 0;
    game.cart.lean_ticks = 0;
    game.cart.crash_ticks = 0;
    game.cart.sprite_frame = 0;
    game.cart.trick_cooldown = 0;
    game.cart.countdown_ticks = CP_CART_COUNTDOWN_TICKS;
    game.cart.speed_stage = 0;
    game.cart.combo = 0;
    game.cart.run_best_combo = 0;
    game.cart.results_choice = 0;
    game.cart.reward = 0;
    game.cart.next_tick = *rb->current_tick + MAX(1, HZ / 25);
    game.cart.trick = CP_CART_TRICK_NONE;
    game.cart.last_trick = CP_CART_TRICK_NONE;
    game.cart.rewarded = false;
    game.cart.menu_latched = false;
    game.scene_type = CP_SCENE_CART_COUNTDOWN;
    game.dirty = true;
    return true;
}

static bool cp_enter_cart_surfer(void)
{
    game.cart.return_room = game.room_index;
    game.cart.return_x = game.x;
    game.cart.return_y = game.y;
    if (!cp_load_scene(CP_CART_TITLE_FILE, CP_VIEW_W, CP_VIEW_H))
        return false;

    game.scene_type = CP_SCENE_CART_TITLE;
    cp_copy(game.location, sizeof(game.location), "Cart Surfer");
    game.dirty = true;
    return true;
}

static void cp_leave_cart_surfer(const char *message)
{
    int room = game.cart.return_room;
    int x = game.cart.return_x;
    int y = game.cart.return_y;

    if (!cp_load_player_asset() || !cp_enter_room(room, x, y))
        return;
    cp_set_message(message);
}

static void cp_cart_tick(void)
{
    int segment_type;
    int segment_length;
    int decision_tick;

    if (game.scene_type == CP_SCENE_CART_COUNTDOWN)
    {
        if (game.cart.countdown_ticks > 0)
            game.cart.countdown_ticks--;
        if (game.cart.countdown_ticks == 0)
        {
            game.scene_type = CP_SCENE_CART_PLAYING;
            game.cart.segment_tick = 0;
        }
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_CART_CRASH)
    {
        if (game.cart.crash_ticks > 0)
            game.cart.crash_ticks--;
        if (game.cart.crash_ticks == 0)
        {
            if (game.cart.lives <= 0)
                cp_cart_finish();
            else
            {
                game.scene_type = CP_SCENE_CART_PLAYING;
                game.cart.segment_tick = 0;
                game.cart.jump_ticks = 0;
                game.cart.lean_ticks = 0;
                game.cart.sprite_frame = 0;
                game.cart.combo = 0;
                game.cart.trick = CP_CART_TRICK_NONE;
                game.dirty = true;
            }
        }
        return;
    }

    if (game.scene_type != CP_SCENE_CART_PLAYING)
        return;

    if (game.cart.trick_cooldown > 0)
        game.cart.trick_cooldown--;
    if (game.cart.lean_ticks > 0)
        game.cart.lean_ticks--;
    else
    {
        game.cart.lean = 0;
        if (game.cart.jump_ticks <= 0)
            game.cart.sprite_frame = (game.cart.segment_tick / 4) & 1;
    }

    if (game.cart.jump_ticks > 0)
    {
        game.cart.jump_ticks--;
        if (game.cart.jump_ticks == 0)
        {
            cp_cart_award_trick(game.cart.trick == CP_CART_TRICK_NONE ?
                                CP_CART_TRICK_OLLIE : game.cart.trick);
            game.cart.trick = CP_CART_TRICK_NONE;
            game.cart.sprite_frame = (game.cart.segment_tick / 4) & 1;
        }
    }

    game.cart.segment_tick++;
    game.cart.speed_stage = MIN(3, game.cart.segment / 7);
    segment_length = CP_CART_SEGMENT_TICKS - game.cart.speed_stage * 5;
    decision_tick = segment_length - 15;
    segment_type = cp_cart_segments[game.cart.segment];
    if (game.cart.segment_tick == decision_tick)
    {
        if ((segment_type == 2 && game.cart.lean <= 0) ||
            (segment_type == 3 && game.cart.lean >= 0) ||
            (segment_type == 4 && game.cart.jump_ticks <= 0))
        {
            cp_cart_crash();
            return;
        }
        if ((segment_type == 2 || segment_type == 3) &&
            game.cart.score <= 999999989)
            game.cart.score += 10;
    }

    if (game.cart.segment_tick >= segment_length)
    {
        game.cart.segment_tick = 0;
        game.cart.segment++;
        if (game.cart.segment >= (int)ARRAYLEN(cp_cart_segments) ||
            cp_cart_segments[game.cart.segment] == 6)
            cp_cart_finish();
    }
    game.dirty = true;
}

static void cp_cart_tick_if_due(void)
{
    if ((game.scene_type == CP_SCENE_CART_COUNTDOWN ||
         game.scene_type == CP_SCENE_CART_PLAYING ||
         game.scene_type == CP_SCENE_CART_CRASH) &&
        (TIME_AFTER(*rb->current_tick, game.cart.next_tick) ||
         *rb->current_tick == game.cart.next_tick))
    {
        cp_cart_tick();
        game.cart.next_tick = *rb->current_tick + MAX(1, HZ / 25);
    }
}

static bool cp_menu_held(void)
{
#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
    return (rb->button_status() & BUTTON_MENU) != 0;
#else
    return false;
#endif
}

static bool cp_play_held(void)
{
#ifdef BUTTON_PLAY
    return (rb->button_status() & BUTTON_PLAY) != 0;
#else
    return false;
#endif
}

static void cp_cart_update_menu_latch(void)
{
    if (game.cart.menu_latched && !cp_menu_held())
        game.cart.menu_latched = false;
}

static void cp_cart_action(int action)
{
    if (game.scene_type == CP_SCENE_CART_TITLE)
    {
        if (action == CP_SELECT_ACTION && !cp_cart_start())
            cp_leave_cart_surfer("Cart Surfer assets are incomplete.");
        else if (action == CP_PLAY_ACTION)
            cp_leave_cart_surfer("Returned to the Mine.");
        return;
    }
    if (game.scene_type == CP_SCENE_CART_COUNTDOWN)
        return;
    if (game.scene_type == CP_SCENE_CART_PAUSED)
    {
        if (action == CP_SELECT_ACTION)
        {
            game.scene_type = CP_SCENE_CART_PLAYING;
            game.cart.next_tick = *rb->current_tick + MAX(1, HZ / 25);
            game.dirty = true;
        }
        else if (action == CP_UP_ACTION && cp_menu_held() &&
                 !game.cart.menu_latched)
        {
            game.cart.menu_latched = true;
            cp_leave_cart_surfer("Returned to the Mine.");
        }
        return;
    }
    if (game.scene_type == CP_SCENE_CART_RESULTS)
    {
        if (action == CP_UP_ACTION || action == CP_UP_REPEAT ||
            action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
        {
            game.cart.results_choice = 1 - game.cart.results_choice;
            game.dirty = true;
        }
        else if (action == CP_SELECT_ACTION && game.cart.results_choice == 0)
            cp_cart_start();
        else if (action == CP_SELECT_ACTION || action == CP_PLAY_ACTION)
            cp_leave_cart_surfer("Cart Surfer complete.");
        return;
    }
    if (game.scene_type != CP_SCENE_CART_PLAYING)
        return;

    if (action == CP_SELECT_ACTION && game.cart.jump_ticks <= 0)
    {
        game.cart.jump_ticks = CP_CART_JUMP_TICKS;
        game.cart.trick = CP_CART_TRICK_OLLIE;
        game.cart.sprite_frame = 3;
    }
    else if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT ||
             action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
    {
        int right = action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT;
        game.cart.lean = right ? 1 : -1;
        game.cart.lean_ticks = 9;
        if (game.cart.jump_ticks > 0)
        {
            game.cart.trick = CP_CART_TRICK_SPIN;
            game.cart.sprite_frame = 4;
        }
        else
            game.cart.sprite_frame = 2;
    }
    else if ((action == CP_UP_ACTION || action == CP_UP_REPEAT) &&
             game.cart.jump_ticks > 0)
    {
        game.cart.trick = CP_CART_TRICK_FLAP;
        game.cart.sprite_frame = 4;
    }
    else if ((action == CP_UP_ACTION || action == CP_UP_REPEAT) &&
             cp_menu_held() && !game.cart.menu_latched)
    {
        game.cart.menu_latched = true;
        game.scene_type = CP_SCENE_CART_PAUSED;
    }
    else if (action == CP_PLAY_ACTION && game.cart.trick_cooldown == 0)
    {
        cp_cart_award_trick(CP_CART_TRICK_GRIND);
        game.cart.sprite_frame = 2;
        game.cart.lean_ticks = 8;
    }
    game.dirty = true;
}

static const char *cp_sound_genres[] =
{
    "pop", "rock", "dance", "dubstep", "spooky"
};

static unsigned int cp_sound_u16(const unsigned char *data)
{
    return data[0] | ((unsigned int)data[1] << 8);
}

static unsigned int cp_sound_u32(const unsigned char *data)
{
    return data[0] | ((unsigned int)data[1] << 8) |
           ((unsigned int)data[2] << 16) |
           ((unsigned int)data[3] << 24);
}

static uint64_t cp_sound_u64(const unsigned char *data)
{
    return cp_sound_u32(data) | ((uint64_t)cp_sound_u32(data + 4) << 32);
}

static void cp_sound_put_u16(unsigned char *data, unsigned int value)
{
    data[0] = value;
    data[1] = value >> 8;
}

static void cp_sound_put_u32(unsigned char *data, unsigned int value)
{
    data[0] = value;
    data[1] = value >> 8;
    data[2] = value >> 16;
    data[3] = value >> 24;
}

static void cp_sound_put_u64(unsigned char *data, uint64_t value)
{
    cp_sound_put_u32(data, (unsigned int)value);
    cp_sound_put_u32(data + 4, (unsigned int)(value >> 32));
}

static void cp_sound_track_path(char *path, size_t size, int slot)
{
    rb->snprintf(path, size, CP_SOUND_TRACK_PATTERN, slot);
}

static bool cp_sound_read_track_header(int slot, unsigned char *header)
{
    char path[MAX_PATH];
    int fd;

    cp_sound_track_path(path, sizeof(path), slot);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    if (rb->read(fd, header, CP_SOUND_TRACK_HEADER) != CP_SOUND_TRACK_HEADER)
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    return header[0] == 'C' && header[1] == 'P' && header[2] == 'T' &&
           header[3] == 'R' && cp_sound_u16(header + 4) == 1 &&
           cp_sound_u16(header + 6) < 4;
}

static bool cp_sound_track_exists(int slot)
{
    unsigned char header[CP_SOUND_TRACK_HEADER];

    return slot >= 0 && slot < CP_SOUND_MAX_TRACKS &&
           cp_sound_read_track_header(slot, header);
}

static bool cp_sound_track_name(int slot, char *name, int size)
{
    unsigned char header[CP_SOUND_TRACK_HEADER];

    if (!cp_sound_read_track_header(slot, header))
        return false;
    header[CP_SOUND_TRACK_HEADER - 1] = '\0';
    cp_copy(name, size, (const char *)header + 16);
    return cp_has_text(name);
}

static bool cp_sound_write_track_name(int slot, const char *name)
{
    unsigned char header[CP_SOUND_TRACK_HEADER];
    char path[MAX_PATH];
    int fd;

    if (!cp_has_text(name) || !cp_sound_read_track_header(slot, header))
        return false;
    rb->memset(header + 16, 0, 32);
    cp_copy((char *)header + 16, 32, name);
    cp_sound_track_path(path, sizeof(path), slot);
    fd = rb->open(path, O_WRONLY);
    if (fd < 0)
        return false;
    if (rb->write(fd, header, sizeof(header)) != sizeof(header))
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    return true;
}

static int cp_sound_find_track(int from, int direction)
{
    int step;

    for (step = 1; step <= CP_SOUND_MAX_TRACKS; step++)
    {
        int slot = (from + direction * step + CP_SOUND_MAX_TRACKS * 2) %
                   CP_SOUND_MAX_TRACKS;

        if (cp_sound_track_exists(slot))
            return slot;
    }
    return -1;
}

static void cp_sound_delete_track(int slot)
{
    char path[MAX_PATH];

    if (!cp_sound_track_exists(slot))
        return;
    cp_sound_track_path(path, sizeof(path), slot);
    rb->remove(path);
    sound.saved_slot = cp_sound_find_track(slot, 1);
    cp_set_message("Offline track deleted.");
    game.dirty = true;
}

static bool cp_sound_clip_info(int id, unsigned int *offset,
                               unsigned int *samples)
{
    const unsigned char *entry;
    unsigned int clip_offset;
    unsigned int clip_samples;

    if (sound.buffer == NULL || id < 0 || id >= CP_SOUND_CLIPS ||
        sound.album_size < 16 + CP_SOUND_CLIPS * 8)
        return false;
    entry = sound.buffer + 16 + id * 8;
    clip_offset = cp_sound_u32(entry);
    clip_samples = cp_sound_u32(entry + 4);
    if (clip_offset > sound.album_size ||
        clip_samples > (sound.album_size - clip_offset) / 2)
        return false;
    *offset = clip_offset;
    *samples = clip_samples;
    return true;
}

static int cp_sound_sample(int id, unsigned int position)
{
    unsigned int offset;
    unsigned int samples;
    unsigned int value;

    if (!cp_sound_clip_info(id, &offset, &samples) || position >= samples)
        return 0;
    offset += position * 2;
    value = sound.buffer[offset] | ((unsigned int)sound.buffer[offset + 1]
                                    << 8);
    return value & 0x8000 ? (int)value - 0x10000 : (int)value;
}

static int16_t cp_sound_clamp(int sample)
{
    if (sample > 32767)
        return 32767;
    if (sample < -32768)
        return -32768;
    return sample;
}

static void cp_sound_get_more(const void **start, size_t *size)
{
    unsigned int loop_offset;
    unsigned int loop_samples;
    unsigned int frame;

    if (!sound.configured || sound.mix_buffer == NULL ||
        !cp_sound_clip_info(0, &loop_offset, &loop_samples) ||
        loop_samples == 0)
    {
        *start = NULL;
        *size = 0;
        return;
    }

    for (frame = 0; frame < CP_SOUND_MIX_FRAMES; frame++)
    {
        int sample = 0;
        int id;

        for (id = 0; id < CP_SOUND_LOOPS; id++)
        {
            if (sound.loop_mask & (1u << id))
                sample += cp_sound_sample(id, sound.loop_position);
        }
        if (sound.one_shot_id >= CP_SOUND_LOOPS)
        {
            unsigned int offset;
            unsigned int samples;

            if (cp_sound_clip_info(sound.one_shot_id, &offset, &samples) &&
                sound.one_shot_position < samples)
            {
                sample += cp_sound_sample(sound.one_shot_id,
                                          sound.one_shot_position++);
            }
            else
            {
                sound.one_shot_id = -1;
                sound.one_shot_position = 0;
            }
        }
        sound.mix_buffer[frame * 2] = cp_sound_clamp(sample);
        sound.mix_buffer[frame * 2 + 1] = sound.mix_buffer[frame * 2];
        sound.loop_position++;
        if (sound.loop_position >= loop_samples)
            sound.loop_position = 0;
    }
    *start = sound.mix_buffer;
    *size = CP_SOUND_MIX_BYTES;
}

static void cp_music_get_more(const void **start, size_t *size)
{
    unsigned int frame;
    unsigned int samples = (unsigned int)(sound.album_size / 2);

    if (!sound.configured || sound.mix_buffer == NULL || samples == 0)
    {
        *start = NULL;
        *size = 0;
        return;
    }

    for (frame = 0; frame < CP_SOUND_MIX_FRAMES; frame++)
    {
        unsigned int offset = sound.loop_position * 2;
        unsigned int value = sound.buffer[offset] |
            ((unsigned int)sound.buffer[offset + 1] << 8);
        int16_t sample = (int16_t)value;

        sound.mix_buffer[frame * 2] = sample;
        sound.mix_buffer[frame * 2 + 1] = sample;
        sound.loop_position++;
        if (sound.loop_position >= samples)
            sound.loop_position = 0;
    }
    *start = sound.mix_buffer;
    *size = CP_SOUND_MIX_BYTES;
}

static bool cp_music_open_audio(const char *path, enum cp_music_kind kind)
{
    int fd;
    off_t file_size;
    uintptr_t mix_address;

    if (sound.started && sound.music_kind == kind)
        return true;
    if (sound.started || sound.buffer_acquired)
        cp_sound_close_audio();
    sound.old_frequency = rb->mixer_get_frequency();
    sound.buffer = rb->plugin_get_audio_buffer(&sound.buffer_size);
    sound.buffer_acquired = sound.buffer != NULL;
    if (!sound.buffer_acquired)
        return false;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
    {
        cp_sound_close_audio();
        return false;
    }
    file_size = rb->filesize(fd);
    if (file_size <= 0 || (size_t)file_size + CP_SOUND_MIX_BYTES + 4 >
        sound.buffer_size || rb->read(fd, sound.buffer, file_size) != file_size)
    {
        rb->close(fd);
        cp_sound_close_audio();
        return false;
    }
    rb->close(fd);

    sound.album_size = (size_t)file_size;
    sound.loop_position = 0;
    mix_address = (uintptr_t)(sound.buffer + file_size + 3) &
                  ~(uintptr_t)3;
    sound.mix_buffer = (int16_t *)mix_address;
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcmbuf_fade(false, true);
    rb->pcmbuf_set_low_latency(true);
    rb->mixer_set_frequency(CP_SOUND_RATE);
    sound.configured = true;
    sound.music_kind = kind;
    rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_PLAYBACK,
                                    CP_MUSIC_AMPLITUDE);
    rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                cp_music_get_more, NULL, 0);
    sound.started = true;
    return true;
}

static bool cp_music_switch_for_room(const char *room_id)
{
    enum cp_music_kind kind = CP_MUSIC_NONE;
    const char *path = NULL;

    if (cp_streq(room_id, "stage"))
    {
        kind = CP_MUSIC_CONCERT;
        path = CP_CONCERT_AUDIO_FILE;
    }
    else if (cp_streq(room_id, "emma_sewer"))
    {
        kind = CP_MUSIC_EMMA_CONCERT;
        path = CP_EMMA_CONCERT_AUDIO_FILE;
    }
    else if (cp_streq(room_id, "stadium"))
    {
        kind = CP_MUSIC_HOCKEY;
        path = CP_HOCKEY_AUDIO_FILE;
    }
    else if (cp_night_city_strip(room_id) != NULL)
    {
        kind = CP_MUSIC_TOWN;
        path = CP_TOWN_AUDIO_FILE;
    }

    if (kind == CP_MUSIC_NONE)
    {
        if (sound.started || sound.buffer_acquired)
            cp_sound_close_audio();
        return true;
    }
    return cp_music_open_audio(path, kind);
}

static void cp_sound_stop_channel(void)
{
    long deadline;

    if (!sound.started)
        return;
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    deadline = *rb->current_tick + MAX(1, HZ / 4);
    while (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) !=
           CHANNEL_STOPPED && TIME_BEFORE(*rb->current_tick, deadline))
        rb->sleep(1);
    sound.started = false;
}

static bool cp_sound_validate_album(size_t size)
{
    unsigned int data_offset;
    int id;

    if (size < 16 + CP_SOUND_CLIPS * 8 || sound.buffer[0] != 'C' ||
        sound.buffer[1] != 'P' || sound.buffer[2] != 'S' ||
        sound.buffer[3] != 'A' || cp_sound_u16(sound.buffer + 4) != 1 ||
        cp_sound_u16(sound.buffer + 6) != CP_SOUND_RATE ||
        cp_sound_u16(sound.buffer + 8) != CP_SOUND_CLIPS)
        return false;
    data_offset = cp_sound_u32(sound.buffer + 12);
    if (data_offset < 16 + CP_SOUND_CLIPS * 8 || data_offset > size)
        return false;
    sound.album_size = size;
    for (id = 0; id < CP_SOUND_CLIPS; id++)
    {
        unsigned int offset;
        unsigned int samples;

        if (!cp_sound_clip_info(id, &offset, &samples) || samples == 0 ||
            offset < data_offset)
            return false;
    }
    return true;
}

static bool cp_sound_load_album(int genre)
{
    char path[MAX_PATH];
    int fd;
    off_t file_size;
    uintptr_t mix_address;

    if (genre < 0 || genre >= 4 || sound.buffer == NULL)
        return false;
    rb->snprintf(path, sizeof(path), CP_SOUND_ALBUM_PATTERN,
                 cp_sound_genres[genre]);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    file_size = rb->filesize(fd);
    if (file_size <= 0 || (size_t)file_size + CP_SOUND_MIX_BYTES + 4 >
        sound.buffer_size ||
        rb->read(fd, sound.buffer, file_size) != file_size)
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    sound.album_size = 0;
    if (!cp_sound_validate_album((size_t)file_size))
        return false;
    mix_address = (uintptr_t)(sound.buffer + file_size + 3) &
                  ~(uintptr_t)3;
    sound.mix_buffer = (int16_t *)mix_address;
    sound.loop_mask = 0;
    sound.loop_position = 0;
    sound.one_shot_id = -1;
    sound.one_shot_position = 0;
    return true;
}

static void cp_sound_start_channel(void)
{
    rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_PLAYBACK,
                                    CP_MUSIC_AMPLITUDE);
    rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                cp_sound_get_more, NULL, 0);
    sound.started = true;
}

static bool cp_sound_open_audio(void)
{
    if (sound.started || sound.buffer_acquired)
        cp_sound_close_audio();
    sound.old_frequency = rb->mixer_get_frequency();
    sound.buffer = rb->plugin_get_audio_buffer(&sound.buffer_size);
    sound.buffer_acquired = sound.buffer != NULL;
    if (!sound.buffer_acquired || !cp_sound_load_album(sound.genre))
    {
        cp_sound_close_audio();
        return false;
    }
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcmbuf_fade(false, true);
    rb->pcmbuf_set_low_latency(true);
    rb->mixer_set_frequency(CP_SOUND_RATE);
    sound.configured = true;
    sound.music_kind = CP_MUSIC_STUDIO;
    cp_sound_start_channel();
    return true;
}

static void cp_sound_write_track_header(unsigned int events,
                                        unsigned int duration)
{
    unsigned char header[CP_SOUND_TRACK_HEADER];

    if (sound.record_fd < 0)
        return;
    rb->memset(header, 0, sizeof(header));
    header[0] = 'C';
    header[1] = 'P';
    header[2] = 'T';
    header[3] = 'R';
    cp_sound_put_u16(header + 4, 1);
    cp_sound_put_u16(header + 6, sound.genre);
    cp_sound_put_u32(header + 8, events);
    cp_sound_put_u32(header + 12, duration);
    if (cp_has_text(sound.record_name))
        cp_copy((char *)header + 16, 32, sound.record_name);
    else
        rb->snprintf((char *)header + 16, 32, "My Track %d",
                     sound.record_slot + 1);
    rb->lseek(sound.record_fd, 0, SEEK_SET);
    rb->write(sound.record_fd, header, sizeof(header));
    rb->lseek(sound.record_fd, 0, SEEK_END);
}

static unsigned int cp_sound_record_elapsed(void)
{
    long ticks;

    if (!sound.recording)
        return 0;
    ticks = *rb->current_tick - sound.record_start_tick;
    if (ticks < 0)
        ticks = 0;
    return (unsigned int)((uint64_t)ticks * 1000 / HZ);
}

static void cp_sound_record_event(int one_shot)
{
    unsigned char event[12];
    uint64_t mask = sound.loop_mask;

    if (!sound.recording || sound.record_fd < 0)
        return;
    if (one_shot >= CP_SOUND_LOOPS && one_shot < CP_SOUND_CLIPS)
        mask |= (uint64_t)1 << one_shot;
    cp_sound_put_u32(event, cp_sound_record_elapsed());
    cp_sound_put_u64(event + 4, mask);
    if (rb->write(sound.record_fd, event, sizeof(event)) == sizeof(event))
        sound.record_events++;
}

static void cp_sound_finish_recording(bool keep)
{
    char path[MAX_PATH];
    unsigned int duration = cp_sound_record_elapsed();

    if (sound.record_fd < 0)
        return;
    if (sound.recording && keep)
        cp_sound_write_track_header(sound.record_events, duration);
    rb->close(sound.record_fd);
    sound.record_fd = -1;
    cp_sound_track_path(path, sizeof(path), sound.record_slot);
    if (!keep || sound.record_events == 0)
        rb->remove(path);
    sound.record_preparing = false;
    sound.recording = false;
    sound.record_events = 0;
    if (keep)
    {
        sound.saved_slot = sound.record_slot;
        cp_set_message("Track saved offline in Saved Tracks.");
    }
    game.dirty = true;
}

static void cp_sound_prepare_recording(void)
{
    unsigned char header[CP_SOUND_TRACK_HEADER];
    char path[MAX_PATH];
    int slot;

    if (sound.recording)
    {
        cp_sound_finish_recording(true);
        if (!cp_sound_show_save_prompt(sound.saved_slot))
            cp_set_message("Track saved with its default name.");
        return;
    }
    if (sound.record_preparing)
    {
        cp_sound_finish_recording(false);
        cp_set_message("Recording cancelled.");
        return;
    }
    for (slot = 0; slot < CP_SOUND_MAX_TRACKS; slot++)
    {
        if (!cp_sound_track_exists(slot))
            break;
    }
    if (slot >= CP_SOUND_MAX_TRACKS)
    {
        cp_set_message("Saved Tracks is full. Delete a track first.");
        return;
    }
    cp_sound_track_path(path, sizeof(path), slot);
    sound.record_fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (sound.record_fd < 0)
    {
        cp_set_message("Could not create the offline track file.");
        return;
    }
    sound.record_slot = slot;
    rb->snprintf(sound.record_name, sizeof(sound.record_name),
                 "My Track %d", slot + 1);
    rb->memset(header, 0, sizeof(header));
    if (rb->write(sound.record_fd, header, sizeof(header)) != sizeof(header))
    {
        cp_sound_finish_recording(false);
        cp_set_message("Could not write the offline track file.");
        return;
    }
    sound.record_events = 0;
    sound.record_preparing = true;
    sound.record_prepare_deadline = *rb->current_tick + 3 * HZ;
    cp_set_message("Recording starts in 3 seconds. Play stops and saves.");
    game.dirty = true;
}

static void cp_sound_record_tick(void)
{
    if (sound.record_preparing &&
        !TIME_BEFORE(*rb->current_tick, sound.record_prepare_deadline))
    {
        sound.record_preparing = false;
        sound.recording = true;
        sound.record_start_tick = *rb->current_tick;
        cp_sound_write_track_header(0, 0);
        cp_sound_record_event(-1);
        cp_set_message("Recording. Play stops and saves the track.");
        game.dirty = true;
    }
    if (sound.recording &&
        cp_sound_record_elapsed() >= CP_SOUND_TRACK_SECONDS * 1000)
    {
        cp_sound_finish_recording(true);
        if (!cp_sound_show_save_prompt(sound.saved_slot))
            cp_set_message("Track saved with its default name.");
    }
    else if (sound.recording && *rb->current_tick % HZ == 0)
        game.dirty = true;
}

static void cp_sound_stop_playback_data(void)
{
    if (sound.playback_fd >= 0)
        rb->close(sound.playback_fd);
    sound.playback_fd = -1;
    sound.playback = false;
    sound.playback_events = 0;
    sound.playback_next_time = 0;
    sound.playback_next_mask = 0;
    sound.playback_has_event = false;
}

static bool cp_sound_read_playback_event(void)
{
    unsigned char event[12];

    if (sound.playback_fd < 0 || sound.playback_events == 0)
    {
        sound.playback_has_event = false;
        return false;
    }
    if (rb->read(sound.playback_fd, event, sizeof(event)) != sizeof(event))
    {
        sound.playback_events = 0;
        sound.playback_has_event = false;
        return false;
    }
    sound.playback_next_time = cp_sound_u32(event);
    sound.playback_next_mask = cp_sound_u64(event + 4);
    sound.playback_events--;
    sound.playback_has_event = true;
    return true;
}

static bool cp_sound_start_saved_track(int slot)
{
    unsigned char header[CP_SOUND_TRACK_HEADER];
    char path[MAX_PATH];
    off_t file_size;
    unsigned int events;

    cp_sound_close_audio();
    cp_sound_track_path(path, sizeof(path), slot);
    sound.playback_fd = rb->open(path, O_RDONLY);
    if (sound.playback_fd < 0 ||
        rb->read(sound.playback_fd, header, sizeof(header)) != sizeof(header) ||
        header[0] != 'C' || header[1] != 'P' || header[2] != 'T' ||
        header[3] != 'R' || cp_sound_u16(header + 4) != 1 ||
        cp_sound_u16(header + 6) >= 4)
    {
        cp_sound_stop_playback_data();
        return false;
    }
    file_size = rb->filesize(sound.playback_fd);
    events = cp_sound_u32(header + 8);
    sound.playback_duration = cp_sound_u32(header + 12);
    if (events == 0 || file_size < CP_SOUND_TRACK_HEADER ||
        events > ((unsigned long)file_size - CP_SOUND_TRACK_HEADER) / 12 ||
        sound.playback_duration > CP_SOUND_TRACK_SECONDS * 1000)
    {
        cp_sound_stop_playback_data();
        return false;
    }
    sound.genre = cp_sound_u16(header + 6);
    sound.playback_events = events;
    if (!cp_sound_open_audio())
    {
        cp_sound_stop_playback_data();
        cp_sound_close_audio();
        return false;
    }
    sound.playback = true;
    sound.playback_start_tick = *rb->current_tick;
    if (!cp_sound_read_playback_event())
    {
        cp_sound_close_audio();
        return false;
    }
    cp_set_message("Playing the saved offline track. Select stops.");
    game.dirty = true;
    return true;
}

static unsigned int cp_sound_playback_elapsed(void)
{
    long ticks = *rb->current_tick - sound.playback_start_tick;

    if (ticks < 0)
        ticks = 0;
    return (unsigned int)((uint64_t)ticks * 1000 / HZ);
}

static void cp_sound_playback_tick(void)
{
    unsigned int elapsed;

    if (!sound.playback)
        return;
    elapsed = cp_sound_playback_elapsed();
    while (sound.playback && sound.playback_has_event &&
           elapsed >= sound.playback_next_time)
    {
        uint64_t mask = sound.playback_next_mask;
        int id;

        rb->pcm_play_lock();
        sound.loop_mask = (unsigned int)(mask & 0x1ffffffu);
        for (id = CP_SOUND_LOOPS; id < CP_SOUND_CLIPS; id++)
        {
            if (mask & ((uint64_t)1 << id))
            {
                sound.one_shot_id = id;
                sound.one_shot_position = 0;
                break;
            }
        }
        rb->pcm_play_unlock();
        sound.playback_has_event = false;
        if (!cp_sound_read_playback_event())
            break;
    }
    if (sound.playback_events == 0 &&
        elapsed >= sound.playback_duration)
    {
        cp_sound_close_audio();
        cp_set_message("Saved track playback complete.");
        game.dirty = true;
    }
    else if (*rb->current_tick % HZ == 0)
        game.dirty = true;
}

static void cp_sound_close_audio(void)
{
    cp_sound_stop_playback_data();
    if (sound.recording)
        cp_sound_finish_recording(true);
    else if (sound.record_preparing)
        cp_sound_finish_recording(false);
    cp_sound_stop_channel();
    if (sound.configured)
    {
        rb->pcmbuf_set_low_latency(false);
        rb->pcmbuf_fade(false, false);
    }
    if (sound.old_frequency != 0)
        rb->mixer_set_frequency(sound.old_frequency);
    sound.configured = false;
    sound.mix_buffer = NULL;
    sound.album_size = 0;
    if (sound.buffer_acquired)
        rb->plugin_release_audio_buffer();
    sound.buffer_acquired = false;
    sound.buffer = NULL;
    sound.buffer_size = 0;
    sound.old_frequency = 0;
    sound.music_kind = CP_MUSIC_NONE;
}

static bool cp_sound_show_title(void)
{
    cp_sound_close_audio();
    if (!cp_load_scene(CP_SOUND_TITLE_FILE, CP_VIEW_W, CP_VIEW_H))
        return false;
    game.scene_type = CP_SCENE_SOUND_TITLE;
    cp_copy(game.location, sizeof(game.location), "Sound Studio");
    game.dirty = true;
    return true;
}

static bool cp_sound_show_instruction_page(int page)
{
    char path[MAX_PATH];

    if (page < 0 || page >= CP_SOUND_INSTRUCTION_PAGES)
        return false;
    cp_sound_close_audio();
    rb->snprintf(path, sizeof(path), CP_SOUND_INSTRUCTION_PATTERN, page + 1);
    if (!cp_load_scene(path, CP_VIEW_W, CP_VIEW_H))
        return false;
    sound.instruction_page = page;
    game.scene_type = CP_SCENE_SOUND_INSTRUCTIONS;
    rb->snprintf(game.location, sizeof(game.location),
                 "Sound Studio Instructions %d/5", page + 1);
    game.dirty = true;
    return true;
}

static bool cp_sound_show_saved(void)
{
    const char *asset;
    int first;

    cp_sound_close_audio();
    first = cp_sound_track_exists(sound.saved_slot) ? sound.saved_slot :
            cp_sound_find_track(0, 1);
    asset = first >= 0 ? CP_SOUND_SAVED_FILE : CP_SOUND_SAVED_EMPTY_FILE;
    if (!cp_load_scene(asset, CP_VIEW_W, CP_VIEW_H))
        return false;
    sound.saved_slot = first;
    game.scene_type = CP_SCENE_SOUND_SAVED;
    cp_copy(game.location, sizeof(game.location), "Sound Studio Saved Tracks");
    game.dirty = true;
    return true;
}

static bool cp_sound_show_save_prompt(int slot)
{
    cp_sound_close_audio();
    if (!cp_sound_track_name(slot, sound.record_name,
                             sizeof(sound.record_name)) ||
        !cp_load_scene(CP_SOUND_SAVE_FILE, CP_VIEW_W, CP_VIEW_H))
        return false;
    sound.saved_slot = slot;
    sound.save_choice = 1;
    game.scene_type = CP_SCENE_SOUND_SAVE;
    cp_copy(game.location, sizeof(game.location), "Name Your Track");
    cp_set_message("Play edits the name. Wheel chooses Cancel or Save.");
    game.dirty = true;
    return true;
}

static bool cp_enter_sound_studio(void)
{
    sound.return_room = game.room_index;
    sound.return_x = game.x;
    sound.return_y = game.y;
    sound.title_choice = 0;
    sound.column = 1;
    sound.row = 0;
    sound.genre = 0;
    return cp_sound_show_title();
}

static void cp_sound_leave(void)
{
    cp_sound_close_audio();
    if (sound.return_room >= 0 && sound.return_room < game.room_count)
        cp_enter_room(sound.return_room, sound.return_x, sound.return_y);
}

static bool cp_sound_show_board(void)
{
    if (!cp_load_scene(CP_SOUND_BOARD_FILE, CP_VIEW_W, CP_VIEW_H))
        return false;
    game.scene_type = CP_SCENE_SOUND_BOARD;
    rb->snprintf(game.location, sizeof(game.location), "Sound Studio: %s",
                 cp_sound_genres[sound.genre]);
    if (!cp_sound_open_audio())
    {
        cp_sound_show_title();
        return false;
    }
    game.dirty = true;
    return true;
}

static bool cp_sound_change_genre(int genre)
{
    int old_genre = sound.genre;

    if (sound.recording || sound.record_preparing)
    {
        cp_set_message("Stop and save the recording before changing genre.");
        return false;
    }
    if (genre == 4)
    {
        cp_set_message("Spooky album is absent from the verified archive.");
        return false;
    }
    if (genre < 0 || genre >= 4 || genre == sound.genre)
        return genre == sound.genre;
    cp_sound_stop_channel();
    sound.genre = genre;
    if (!cp_sound_load_album(genre))
    {
        sound.genre = old_genre;
        if (cp_sound_load_album(old_genre))
            cp_sound_start_channel();
        cp_set_message("Official Sound Studio album is unavailable.");
        return false;
    }
    rb->snprintf(game.location, sizeof(game.location), "Sound Studio: %s",
                 cp_sound_genres[genre]);
    cp_sound_start_channel();
    game.dirty = true;
    return true;
}

static void cp_sound_toggle_selected(void)
{
    int id;

    if (sound.column == 0)
    {
        cp_sound_change_genre(sound.row);
        return;
    }
    if (sound.column <= 5)
    {
        unsigned int column_mask = 0x1fu << ((sound.column - 1) * 5);

        id = (sound.column - 1) * 5 + sound.row;
        rb->pcm_play_lock();
        if (sound.loop_mask & (1u << id))
            sound.loop_mask &= ~(1u << id);
        else
            sound.loop_mask = (sound.loop_mask & ~column_mask) | (1u << id);
        rb->pcm_play_unlock();
        cp_sound_record_event(-1);
    }
    else
    {
        id = CP_SOUND_LOOPS + (sound.column - 6) * 5 + sound.row;
        rb->pcm_play_lock();
        sound.one_shot_id = id;
        sound.one_shot_position = 0;
        rb->pcm_play_unlock();
        cp_sound_record_event(id);
    }
    game.dirty = true;
}

static void cp_sound_action(int action)
{
    if (game.scene_type == CP_SCENE_SOUND_TITLE)
    {
        if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            sound.title_choice = (sound.title_choice + 2) % 3;
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            sound.title_choice = (sound.title_choice + 1) % 3;
        else if (action == CP_SELECT_ACTION)
        {
            if (sound.title_choice == 0)
            {
                if (!cp_sound_show_board())
                    cp_set_message("Sound Studio assets are incomplete.");
            }
            else if (sound.title_choice == 1)
            {
                if (!cp_sound_show_instruction_page(0))
                    cp_set_message("Sound Studio instructions are missing.");
            }
            else
            {
                if (!cp_sound_show_saved())
                    cp_set_message("Saved Tracks art is missing.");
            }
        }
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_SOUND_INSTRUCTIONS)
    {
        int page = sound.instruction_page;

        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT ||
            action == CP_UP_ACTION || action == CP_UP_REPEAT)
            page = MAX(0, page - 1);
        else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT ||
                 action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            page = MIN(CP_SOUND_INSTRUCTION_PAGES - 1, page + 1);
        else if (action == CP_SELECT_ACTION &&
                 page == CP_SOUND_INSTRUCTION_PAGES - 1)
        {
            if (!cp_sound_show_board())
                cp_set_message("Sound Studio assets are incomplete.");
            return;
        }
        if (page != sound.instruction_page)
            cp_sound_show_instruction_page(page);
        return;
    }

    if (game.scene_type == CP_SCENE_SOUND_SAVED)
    {
        if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            sound.saved_slot = cp_sound_find_track(sound.saved_slot, -1);
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            sound.saved_slot = cp_sound_find_track(sound.saved_slot, 1);
        else if (action == CP_SELECT_ACTION && sound.playback)
        {
            cp_sound_close_audio();
            cp_set_message("Saved track playback stopped.");
        }
        else if (action == CP_SELECT_ACTION && sound.saved_slot < 0)
        {
            if (!cp_sound_show_board())
                cp_set_message("Sound Studio assets are incomplete.");
        }
        else if (action == CP_SELECT_ACTION &&
                 !cp_sound_start_saved_track(sound.saved_slot))
            cp_set_message("Saved track data or album is unavailable.");
        else if (action == CP_PLAY_ACTION && sound.saved_slot >= 0)
        {
            cp_sound_close_audio();
            cp_sound_delete_track(sound.saved_slot);
            if (!cp_sound_show_saved())
                cp_set_message("Saved Tracks art is missing.");
        }
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_SOUND_SAVE)
    {
        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT ||
            action == CP_UP_ACTION || action == CP_UP_REPEAT)
            sound.save_choice = 0;
        else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT ||
                 action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            sound.save_choice = 1;
        else if (action == CP_PLAY_ACTION)
        {
            char name[32];

            cp_copy(name, sizeof(name), sound.record_name);
            if (rb->kbd_input(name, sizeof(name), NULL) == 0 &&
                cp_has_text(name))
            {
                cp_copy(sound.record_name, sizeof(sound.record_name), name);
                cp_set_message("Track name ready. Select Save to keep it.");
            }
        }
        else if (action == CP_SELECT_ACTION && sound.save_choice == 0)
        {
            cp_sound_delete_track(sound.saved_slot);
            if (!cp_sound_show_board())
                cp_sound_show_title();
        }
        else if (action == CP_SELECT_ACTION)
        {
            if (!cp_sound_write_track_name(sound.saved_slot,
                                           sound.record_name))
                cp_set_message("Could not save the track name.");
            else
            {
                cp_set_message("Track saved offline in Saved Tracks.");
                cp_sound_show_saved();
            }
        }
        game.dirty = true;
        return;
    }

    if (game.scene_type != CP_SCENE_SOUND_BOARD)
        return;
    if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
        sound.row = (sound.row + 4) % 5;
    else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
        sound.row = (sound.row + 1) % 5;
    else if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
        sound.column = (sound.column + 8) % 9;
    else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
        sound.column = (sound.column + 1) % 9;
    else if (action == CP_SELECT_ACTION)
        cp_sound_toggle_selected();
    else if (action == CP_PLAY_ACTION)
        cp_sound_prepare_recording();
    game.dirty = true;
}

static bool cp_parse_puffle_food_line(char *line,
                                      struct cp_puffle_food_item *item)
{
    char *cursor = line;
    char *id_text = cp_next_field(&cursor);
    char *name = cp_next_field(&cursor);
    char *asset = cp_next_field(&cursor);
    char *cost_text = cp_next_field(&cursor);
    char *food_text = cp_next_field(&cursor);
    char *rest_text = cp_next_field(&cursor);
    char *happy_text = cp_next_field(&cursor);
    char *clean_text = cp_next_field(&cursor);

    if (!cp_has_text(name) || !cp_has_text(asset) ||
        !cp_parse_int(id_text, &item->id) ||
        !cp_parse_int(cost_text, &item->cost) ||
        !cp_parse_int(food_text, &item->food) ||
        !cp_parse_int(rest_text, &item->rest) ||
        !cp_parse_int(happy_text, &item->happy) ||
        !cp_parse_int(clean_text, &item->clean) || item->id <= 0 ||
        item->cost < 0)
        return false;
    cp_copy(item->name, sizeof(item->name), name);
    cp_copy(item->asset, sizeof(item->asset), asset);
    return true;
}

static bool cp_read_puffle_food_item(int wanted,
                                     struct cp_puffle_food_item *item)
{
    char line[CP_FURNITURE_LINE];
    int index = 0;
    int fd = rb->open(CP_PUFFLE_FOOD_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        if (!cp_data_line(line))
            continue;
        if (index++ == wanted)
        {
            bool parsed = cp_parse_puffle_food_line(line, item);
            rb->close(fd);
            return parsed;
        }
    }
    rb->close(fd);
    return false;
}

static int cp_count_puffle_food_items(void)
{
    struct cp_puffle_food_item item;
    int count = 0;

    while (cp_read_puffle_food_item(count, &item))
        count++;
    return count;
}

static bool cp_load_puffle_food_item(int index)
{
    if (index < 0 || index >= game.puffle_food_count ||
        !cp_read_puffle_food_item(index, &game.puffle_food_item) ||
        !cp_load_puffle_food_icon(game.puffle_food_item.asset))
        return false;
    game.puffle_food_index = index;
    return true;
}

static bool cp_parse_puffle_toy_line(char *line,
                                     struct cp_puffle_toy_item *item)
{
    char *cursor = line;
    char *color_text = cp_next_field(&cursor);
    char *kind = cp_next_field(&cursor);
    char *id_text = cp_next_field(&cursor);
    char *name = cp_next_field(&cursor);
    char *asset = cp_next_field(&cursor);
    char *cost_text = cp_next_field(&cursor);
    char *food_text = cp_next_field(&cursor);
    char *rest_text = cp_next_field(&cursor);
    char *happy_text = cp_next_field(&cursor);
    char *clean_text = cp_next_field(&cursor);
    char *reaction_text = cp_next_field(&cursor);

    if (!cp_has_text(kind) || !cp_has_text(name) || !cp_has_text(asset) ||
        (!cp_streq(kind, "normal") && !cp_streq(kind, "super")) ||
        !cp_parse_int(color_text, &item->color) ||
        !cp_parse_int(id_text, &item->id) ||
        !cp_parse_int(cost_text, &item->cost) ||
        !cp_parse_int(food_text, &item->food) ||
        !cp_parse_int(rest_text, &item->rest) ||
        !cp_parse_int(happy_text, &item->happy) ||
        !cp_parse_int(clean_text, &item->clean) ||
        !cp_parse_int(reaction_text, &item->reaction) ||
        item->color < 0 || item->color >= CP_PUFFLE_TYPES ||
        item->id <= 0 || item->cost < 0 || item->reaction != 2)
        return false;
    cp_copy(item->kind, sizeof(item->kind), kind);
    cp_copy(item->name, sizeof(item->name), name);
    cp_copy(item->asset, sizeof(item->asset), asset);
    return true;
}

static bool cp_read_puffle_toy_item(int wanted,
                                    struct cp_puffle_toy_item *item)
{
    char line[CP_FURNITURE_LINE];
    int index = 0;
    int fd = rb->open(CP_PUFFLE_TOY_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        if (!cp_data_line(line))
            continue;
        if (index++ == wanted)
        {
            bool parsed = cp_parse_puffle_toy_line(line, item);
            rb->close(fd);
            return parsed;
        }
    }
    rb->close(fd);
    return false;
}

static int cp_count_puffle_toy_items(void)
{
    struct cp_puffle_toy_item item;
    int count = 0;

    while (cp_read_puffle_toy_item(count, &item))
        count++;
    return count;
}

static bool cp_load_puffle_toy_item(int kind)
{
    int index = game.puffle_type * CP_PUFFLE_TOYS_PER_TYPE + kind;
    const char *wanted_kind = kind == 0 ? "normal" : "super";

    if (kind < 0 || kind >= CP_PUFFLE_TOYS_PER_TYPE ||
        !cp_read_puffle_toy_item(index, &game.puffle_toy_item) ||
        game.puffle_toy_item.color != game.puffle_type ||
        !cp_streq(game.puffle_toy_item.kind, wanted_kind) ||
        !cp_load_puffle_toy(game.puffle_type, wanted_kind) ||
        !cp_load_puffle_toy_icon(game.puffle_toy_item.asset))
        return false;
    game.puffle_toy_index = kind;
    return true;
}

static bool cp_parse_puffle_hat_line(char *line,
                                     struct cp_puffle_hat_item *item)
{
    char *cursor = line;
    char *id_text = cp_next_field(&cursor);
    char *name = cp_next_field(&cursor);
    char *asset = cp_next_field(&cursor);
    char *cost_text = cp_next_field(&cursor);
    char *available_text = cp_next_field(&cursor);
    int available;

    if (!cp_has_text(name) || !cp_has_text(asset) ||
        !cp_parse_int(id_text, &item->id) ||
        !cp_parse_int(cost_text, &item->cost) ||
        !cp_parse_int(available_text, &available) || item->id <= 0 ||
        item->cost < 0 || (available != 0 && available != 1))
        return false;
    cp_copy(item->name, sizeof(item->name), name);
    cp_copy(item->asset, sizeof(item->asset), asset);
    item->available = available != 0;
    return true;
}

static bool cp_read_puffle_hat_item(int wanted,
                                    struct cp_puffle_hat_item *item)
{
    char line[CP_FURNITURE_LINE];
    int index = 0;
    int fd = rb->open(CP_PUFFLE_HAT_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        if (!cp_data_line(line))
            continue;
        if (index++ == wanted)
        {
            bool parsed = cp_parse_puffle_hat_line(line, item);
            rb->close(fd);
            return parsed;
        }
    }
    rb->close(fd);
    return false;
}

static int cp_count_puffle_hat_items(void)
{
    struct cp_puffle_hat_item item;
    int count = 0;

    while (cp_read_puffle_hat_item(count, &item))
        count++;
    return count;
}

static bool cp_load_puffle_hat_item(int index)
{
    if (index < 0 || index >= game.puffle_hat_count ||
        !cp_read_puffle_hat_item(index, &game.puffle_hat_item) ||
        !cp_load_puffle_hat_asset(game.puffle_hat_item.asset))
        return false;
    game.puffle_hat_index = index;
    return true;
}

static bool cp_puffle_hat_owned(int index)
{
    int word;
    int bit;

    if (index < 0 || index >= CP_PUFFLE_HAT_ITEMS)
        return false;
    word = index / 32;
    bit = index % 32;
    return (game.puffle_hat_owned[word] & (1u << bit)) != 0;
}

static void cp_own_puffle_hat(int index)
{
    if (index >= 0 && index < CP_PUFFLE_HAT_ITEMS)
        game.puffle_hat_owned[index / 32] |= 1u << (index % 32);
}

static bool cp_load_equipped_puffle_hat(void)
{
    struct cp_puffle_hat_item item;
    int equipped = game.puffle_hat_equipped[game.puffle_type];
    int index;

    game.puffle_hat_loaded = false;
    if (equipped <= 0)
        return false;
    for (index = 0; index < CP_PUFFLE_HAT_ITEMS; index++)
    {
        if (!cp_read_puffle_hat_item(index, &item))
            return false;
        if (item.id == equipped && item.available &&
            cp_load_puffle_hat_asset(item.asset))
        {
            game.puffle_hat_loaded = true;
            return true;
        }
    }
    return false;
}

static bool cp_apply_puffle_room_hat(void)
{
    static const char *layers[] = { "back", "front" };
    struct cp_puffle_hat_item item;
    fb_data *puffle = (fb_data *)game.puffle.data;
    int equipped = game.puffle_hat_equipped[game.puffle_type];
    int index;
    int direction;
    int layer;

    if (equipped <= 0)
        return true;
    for (index = 0; index < CP_PUFFLE_HAT_ITEMS; index++)
    {
        if (!cp_read_puffle_hat_item(index, &item))
            return true;
        if (item.id != equipped)
            continue;
        if (!item.available)
            return true;
        for (direction = 0; direction < 4; direction++)
        {
            for (layer = 0; layer < 2; layer++)
            {
                char path[MAX_PATH];
                fb_data *hat;
                int animation;

                rb->snprintf(path, sizeof(path),
                             CP_PUFFLE_ROOM_HAT_PATTERN, item.asset,
                             direction, layers[layer]);
                if (!cp_load_bitmap(path, &game.puffle_hat,
                                    puffle_hat_pixels,
                                    sizeof(puffle_hat_pixels),
                                    CP_PUFFLE_HAT_W *
                                    CP_PUFFLE_HAT_FRAMES,
                                    CP_PUFFLE_HAT_H, FORMAT_NATIVE))
                    return false;
                hat = (fb_data *)game.puffle_hat.data;
                for (animation = 0; animation < 2; animation++)
                {
                    int destination_frame = direction * 2 + animation;
                    int y;

                    for (y = 0; y < CP_PUFFLE_H; y++)
                    {
                        int x;

                        for (x = 0; x < CP_PUFFLE_W; x++)
                        {
                            int source_pixel = y * game.puffle_hat.width +
                                               animation * CP_PUFFLE_W + x;
                            int destination_pixel =
                                y * game.puffle.width +
                                destination_frame * CP_PUFFLE_W + x;
                            fb_data value = hat[source_pixel];

                            if (value == TRANSPARENT_COLOR)
                                continue;
                            if (layer != 0 ||
                                puffle[destination_pixel] ==
                                TRANSPARENT_COLOR)
                                puffle[destination_pixel] = value;
                        }
                    }
                }
            }
        }
        return true;
    }
    return true;
}

static bool cp_parse_sport_line(char *line, int owned_bit,
                                struct cp_sport_item *item)
{
    char *cursor = line;
    char *page_text = cp_next_field(&cursor);
    char *id_text = cp_next_field(&cursor);
    char *type_text = cp_next_field(&cursor);
    char *name = cp_next_field(&cursor);
    char *cost_text = cp_next_field(&cursor);
    char *maximum_text = cp_next_field(&cursor);

    item->max_quantity = 99;
    if (cp_has_text(maximum_text) &&
        !cp_parse_int(maximum_text, &item->max_quantity))
        return false;

    if (!cp_has_text(name) || !cp_parse_int(page_text, &item->page) ||
        !cp_parse_int(id_text, &item->id) ||
        !cp_parse_int(type_text, &item->type) ||
        !cp_parse_int(cost_text, &item->cost) || item->page < 1 ||
        item->page > CP_PENGUIN_STYLE_PAGES || item->id <= 0 ||
        item->cost < 0 || owned_bit < 0 ||
        owned_bit >= CP_PENGUIN_STYLE_ITEMS || item->max_quantity < 1 ||
        item->max_quantity > 99)
        return false;
    cp_copy(item->name, sizeof(item->name), name);
    item->owned_bit = owned_bit;
    return true;
}

static bool cp_costume_scene(void)
{
    return game.scene_type == CP_SCENE_COSTUME_SHOP;
}

static bool cp_penguin_style_scene(void)
{
    return game.scene_type == CP_SCENE_PENGUIN_STYLE;
}

static bool cp_ninja_scene(void)
{
    return game.scene_type == CP_SCENE_NINJA_SHOP;
}

static int cp_catalog_pages(void)
{
    if (cp_ninja_scene())
        return CP_NINJA_PAGES;
    if (cp_penguin_style_scene())
        return CP_PENGUIN_STYLE_PAGES;
    return cp_costume_scene() ? CP_COSTUME_PAGES : CP_SPORT_PAGES;
}

static const char *cp_catalog_data_file(void)
{
    if (cp_ninja_scene())
        return CP_NINJA_DATA_FILE;
    if (cp_penguin_style_scene())
        return CP_PENGUIN_STYLE_DATA_FILE;
    return cp_costume_scene() ? CP_COSTUME_DATA_FILE : CP_SPORT_DATA_FILE;
}

static const char *cp_catalog_page_pattern(void)
{
    if (cp_ninja_scene())
        return CP_NINJA_PAGE_PATTERN;
    if (cp_penguin_style_scene())
        return CP_PENGUIN_STYLE_PAGE_PATTERN;
    return cp_costume_scene() ? CP_COSTUME_PAGE_PATTERN :
                                CP_SPORT_PAGE_PATTERN;
}

static const char *cp_catalog_short_name(void)
{
    if (cp_ninja_scene())
        return "NINJA";
    if (cp_penguin_style_scene())
        return "STYLE";
    return cp_costume_scene() ? "COSTUME" : "SPORT";
}

static bool cp_clothing_item_owned(int item_id)
{
    unsigned char owned = 0;
    int fd;

    if (item_id <= 0 || item_id > 99999)
        return false;
    fd = rb->open(CP_CLOTHING_INVENTORY_FILE, O_RDONLY);
    if (fd < 0)
        return false;
    if (rb->lseek(fd, item_id, SEEK_SET) >= 0)
        rb->read(fd, &owned, sizeof(owned));
    rb->close(fd);
    return owned != 0;
}

static bool cp_own_clothing_item(int item_id)
{
    unsigned char owned = 1;
    int fd;
    int wrote;

    if (item_id <= 0 || item_id > 99999)
        return false;
    fd = rb->open(CP_CLOTHING_INVENTORY_FILE,
                  O_RDWR | O_CREAT, 0666);
    if (fd < 0)
        return false;
    if (rb->lseek(fd, item_id, SEEK_SET) < 0)
    {
        rb->close(fd);
        return false;
    }
    wrote = rb->write(fd, &owned, sizeof(owned));
    rb->close(fd);
    return wrote == (int)sizeof(owned);
}

static bool cp_read_catalog_item(const char *data_file, int page, int wanted,
                                 struct cp_sport_item *item)
{
    char line[CP_FURNITURE_LINE];
    int index = 0;
    int owned_bit = 0;
    int fd = rb->open(data_file, O_RDONLY);

    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        struct cp_sport_item candidate;

        if (!cp_data_line(line))
            continue;
        if (!cp_parse_sport_line(line, owned_bit++, &candidate))
            continue;
        if (candidate.page != page)
            continue;
        if (index++ == wanted)
        {
            *item = candidate;
            rb->close(fd);
            return true;
        }
    }
    rb->close(fd);
    return false;
}

static bool cp_read_current_catalog_item(int page, int wanted,
                                         struct cp_sport_item *item)
{
    return cp_read_catalog_item(
        cp_catalog_data_file(), page, wanted, item
    );
}

static int cp_count_catalog_items(int page)
{
    struct cp_sport_item item;
    int count = 0;

    while (cp_read_current_catalog_item(page, count, &item))
        count++;
    return count;
}

static bool cp_catalog_item_owned(const struct cp_sport_item *item)
{
    unsigned int owned;

    if (cp_ninja_scene())
    {
        if (item->type == CP_SPORT_FURNITURE)
            return cp_furniture_quantity(item->id) > 0;
        if (item->type == CP_NINJA_BUILDING)
            return cp_igloo_item_owned(0, item->id);
        return cp_clothing_item_owned(item->id);
    }
    if (cp_penguin_style_scene())
        return cp_clothing_item_owned(item->id);
    owned = cp_costume_scene() ? game.costume_owned : game.sport_owned;

    return (owned & (1u << item->owned_bit)) != 0;
}

static bool cp_catalog_own_item(const struct cp_sport_item *item)
{
    if (cp_ninja_scene())
    {
        if (item->type == CP_SPORT_FURNITURE)
            return true;
        if (item->type == CP_NINJA_BUILDING)
        {
            cp_own_igloo_item(0, item->id);
            return true;
        }
        return cp_own_clothing_item(item->id);
    }
    if (cp_penguin_style_scene())
        return cp_own_clothing_item(item->id);
    if (cp_costume_scene())
        game.costume_owned |= 1u << item->owned_bit;
    else
        game.sport_owned |= 1u << item->owned_bit;
    return true;
}

static bool cp_catalog_item_wearable(const struct cp_sport_item *item)
{
    if (cp_ninja_scene())
        return item->type == 0 || item->type == 1 ||
               item->type == CP_SPORT_HAND;
    if (cp_penguin_style_scene())
        return item->type >= 0 && item->type <= CP_SPORT_HAND;
    if (cp_costume_scene())
        return item->type == 0 || item->type == 1 || item->type == 2 ||
               item->type == CP_COSTUME_NECK || item->type == CP_SPORT_HAND;

    switch (item->id)
    {
        case 254: case 255: case 385: case 435: case 436: case 717:
        case 775: case 778: case 791: case 792: case 836: case 837:
            return true;
        default:
            return false;
    }
}

static int cp_sport_equipped_slot(int type)
{
    if (type >= 0 && type <= 2)
        return type;
    if (type == CP_SPORT_HAND)
        return 3;
    if (type == CP_COSTUME_NECK)
        return 4;
    if (type == CP_CLOTHING_FACE)
        return 5;
    return -1;
}

static bool cp_load_sport_page(int page)
{
    char path[MAX_PATH];
    int pages = cp_catalog_pages();

    if (page < 1 || page > pages)
        return false;
    rb->snprintf(path, sizeof(path), cp_catalog_page_pattern(), page);
    if (!cp_load_scene(path, CP_VIEW_W, CP_VIEW_H))
        return false;
    game.sport_page = page;
    game.sport_index = 0;
    game.sport_count = cp_count_catalog_items(page);
    if (game.sport_count > 0)
    {
        if (!cp_read_current_catalog_item(page, 0, &game.sport_item))
            return false;
        cp_copy(game.shop_status, sizeof(game.shop_status),
                cp_catalog_item_owned(&game.sport_item) ? "Owned" :
                "Select to buy");
    }
    else
    {
        rb->memset(&game.sport_item, 0, sizeof(game.sport_item));
        if (cp_ninja_scene())
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    page == 1 ? "Martial Artworks - Dec 2011" :
                    "Original catalog information");
        else if (cp_penguin_style_scene())
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    page == 1 ? "Penguin Style - April 2012" :
                    "Original catalog information");
        else
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    cp_costume_scene() ?
                    "Ruby and the Ruby - April 2012" :
                    (page == 1 ? "Snow and Sports June 2008" :
                     "Original catalog information"));
    }
    game.dirty = true;
    return true;
}

static bool cp_enter_sport_shop(void)
{
    enum cp_scene_type old_scene = game.scene_type;

    game.shop_return_room = game.room_index;
    game.shop_return_x = game.x;
    game.shop_return_y = game.y;
    game.scene_type = CP_SCENE_SPORT_SHOP;
    if (!cp_load_sport_page(1))
    {
        game.scene_type = old_scene;
        return false;
    }
    cp_copy(game.location, sizeof(game.location), "Snow and Sports");
    return true;
}

static bool cp_enter_costume_shop(void)
{
    enum cp_scene_type old_scene = game.scene_type;

    game.shop_return_room = game.room_index;
    game.shop_return_x = game.x;
    game.shop_return_y = game.y;
    game.scene_type = CP_SCENE_COSTUME_SHOP;
    if (!cp_load_sport_page(1))
    {
        game.scene_type = old_scene;
        return false;
    }
    cp_copy(game.location, sizeof(game.location), "Costume Trunk");
    return true;
}

static bool cp_enter_penguin_style(void)
{
    enum cp_scene_type old_scene = game.scene_type;

    game.shop_return_room = game.room_index;
    game.shop_return_x = game.x;
    game.shop_return_y = game.y;
    game.scene_type = CP_SCENE_PENGUIN_STYLE;
    if (!cp_load_sport_page(1))
    {
        game.scene_type = old_scene;
        return false;
    }
    cp_copy(game.location, sizeof(game.location), "Penguin Style");
    return true;
}

static bool cp_enter_ninja_shop(void)
{
    enum cp_scene_type old_scene = game.scene_type;

    game.shop_return_room = game.room_index;
    game.shop_return_x = game.x;
    game.shop_return_y = game.y;
    game.scene_type = CP_SCENE_NINJA_SHOP;
    if (!cp_load_sport_page(1))
    {
        game.scene_type = old_scene;
        return false;
    }
    cp_copy(game.location, sizeof(game.location), "Martial Artworks");
    return true;
}

static void cp_leave_sport_shop(void)
{
    if (!cp_load_player_asset() ||
        !cp_enter_room(game.shop_return_room, game.shop_return_x,
                       game.shop_return_y))
        return;
    cp_write_save();
}

static void cp_sport_move(int direction)
{
    int next;

    if (game.sport_count <= 0)
        return;
    next = game.sport_index + direction;
    if (next < 0)
        next = game.sport_count - 1;
    else if (next >= game.sport_count)
        next = 0;
    if (!cp_read_current_catalog_item(game.sport_page, next,
                                      &game.sport_item))
        return;
    game.sport_index = next;
    cp_copy(game.shop_status, sizeof(game.shop_status),
            cp_catalog_item_owned(&game.sport_item) ? "Owned" :
            "Select to buy");
    game.dirty = true;
}

static bool cp_refresh_sport_preview(void)
{
    int page = game.sport_page;
    int index = game.sport_index;

    if (!cp_load_player_asset() || !cp_load_sport_page(page))
        return false;
    if (game.sport_count > 0 && index < game.sport_count)
    {
        if (!cp_read_current_catalog_item(page, index, &game.sport_item))
            return false;
        game.sport_index = index;
    }
    return true;
}

static int cp_clothing_color_slot(int item_id)
{
    switch (item_id)
    {
        case 2: return 0;   /* Green */
        case 3: return 1;   /* Pink */
        case 4: return 2;   /* Black */
        case 10: return 3;  /* Peach */
        case 11: return 4;  /* Dark Green */
        case 12: return 5;  /* Light Blue */
        case 13: return 6;  /* Lime Green */
        case 15: return 7;  /* Aqua */
        case 14: return 8;  /* Gray */
        case 5: return 9;   /* Red */
        case 6: return 10;  /* Orange */
        case 7: return 11;  /* Yellow */
        case 8: return 12;  /* Dark Purple */
        case 9: return 13;  /* Brown */
        case 1: return 14;  /* Blue */
        default: return -1;
    }
}

static bool cp_catalog_item_equipped(const struct cp_sport_item *item)
{
    int slot = cp_sport_equipped_slot(item->type);

    if (slot >= 0)
        return game.sport_equipped[slot] == item->id;
    if (item->type == CP_SPORT_BACKGROUND)
        return game.sport_background == item->id;
    if (item->type == CP_CLOTHING_FLAG)
        return game.clothing_flag == item->id;
    if (item->type == CP_CLOTHING_COLOR)
        return game.equipped[CP_COLOR_PAGE] ==
               cp_clothing_color_slot(item->id);
    if (item->type == CP_NINJA_BUILDING)
        return game.igloo_building == item->id;
    return false;
}

static void cp_sport_purchase(void)
{
    struct cp_sport_item *item = &game.sport_item;
    bool owned;
    int slot;

    if (game.sport_count <= 0)
        return;
    owned = cp_catalog_item_owned(item);
    if (item->type == CP_SPORT_FURNITURE)
    {
        int quantity = cp_furniture_quantity(item->id);

        if (quantity >= item->max_quantity)
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Quantity limit reached");
            return;
        }
        if (game.coins < item->cost)
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Not enough coins");
            return;
        }
        if (!cp_set_furniture_quantity(item->id, quantity + 1))
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Inventory save failed");
            return;
        }
        game.coins -= item->cost;
        cp_catalog_own_item(item);
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Furniture purchased");
        cp_write_save();
        game.dirty = true;
        return;
    }
    if (!owned)
    {
        if (game.coins < item->cost)
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Not enough coins");
            game.dirty = true;
            return;
        }
        if (!cp_catalog_own_item(item))
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Inventory save failed");
            game.dirty = true;
            return;
        }
        game.coins -= item->cost;
    }
    if (item->type == CP_CLOTHING_COLOR)
    {
        int color = cp_clothing_color_slot(item->id);

        if (color < 0)
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Color mapping missing");
        else
        {
            game.equipped[CP_COLOR_PAGE] = color;
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Color equipped");
            if (!cp_refresh_sport_preview())
                cp_copy(game.shop_status, sizeof(game.shop_status),
                        "Color art missing");
        }
    }
    else if (item->type == CP_CLOTHING_FLAG)
    {
        game.clothing_flag = item->id;
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Player-card flag equipped");
    }
    else if (item->type == CP_SPORT_BACKGROUND)
    {
        game.sport_background = item->id;
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Background equipped");
    }
    else if (item->type == CP_NINJA_BUILDING)
    {
        game.igloo_building = item->id;
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Dojo Igloo equipped");
    }
    else if (cp_catalog_item_wearable(item) &&
             (slot = cp_sport_equipped_slot(item->type)) >= 0)
    {
        game.sport_equipped[slot] = item->id;
        if (item->type >= 0 && item->type <= 2)
            game.equipped[item->type] = -1;
        else if (item->type == CP_CLOTHING_FACE)
            game.equipped[CP_FACE_PAGE] = -1;
        cp_copy(game.shop_status, sizeof(game.shop_status), "Equipped");
        if (!cp_refresh_sport_preview())
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Catalog paper doll missing");
    }
    else
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Owned special-action item");
    cp_write_save();
    game.dirty = true;
}

static void cp_sport_remove(void)
{
    int slot;

    if (game.sport_count <= 0)
        return;
    slot = cp_sport_equipped_slot(game.sport_item.type);
    if (slot >= 0 && game.sport_equipped[slot] == game.sport_item.id)
    {
        game.sport_equipped[slot] = 0;
        cp_copy(game.shop_status, sizeof(game.shop_status), "Item removed");
        if (!cp_refresh_sport_preview())
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Catalog art missing");
    }
    else if (game.sport_item.type == CP_SPORT_BACKGROUND &&
             game.sport_background == game.sport_item.id)
    {
        game.sport_background = 0;
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Background removed");
    }
    else if (game.sport_item.type == CP_CLOTHING_FLAG &&
             game.clothing_flag == game.sport_item.id)
    {
        game.clothing_flag = 0;
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Player-card flag removed");
    }
    else if (game.sport_item.type == CP_CLOTHING_COLOR)
    {
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Choose another color");
    }
    cp_write_save();
    game.dirty = true;
}

static void cp_sport_action(int action)
{
    int pages = cp_catalog_pages();

    if ((action == CP_UP_ACTION || action == CP_UP_REPEAT) &&
        cp_menu_held())
    {
        cp_leave_sport_shop();
        return;
    }
    if ((action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT) &&
        cp_play_held())
        return;
    if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT ||
        action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
    {
        int direction = (action == CP_RIGHT_ACTION ||
                         action == CP_RIGHT_REPEAT) ? 1 : -1;
        int page = game.sport_page + direction;

        if (page < 1)
            page = pages;
        else if (page > pages)
            page = 1;
        cp_load_sport_page(page);
    }
    else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
        cp_sport_move(-1);
    else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
        cp_sport_move(1);
    else if (action == CP_SELECT_ACTION)
        cp_sport_purchase();
    else if (action == CP_PLAY_ACTION)
        cp_sport_remove();
}

static bool cp_shop_item_owned(int index)
{
    if (index < 0 || index >= game.shop_item_count)
        return false;
    if (index < 32)
        return (game.owned_lo & (1u << index)) != 0;
    return (game.owned_hi & (1u << (index - 32))) != 0;
}

static void cp_shop_own_item(int index)
{
    if (index < 0 || index >= game.shop_item_count)
        return;
    if (index < 32)
        game.owned_lo |= 1u << index;
    else
        game.owned_hi |= 1u << (index - 32);
}

static int cp_shop_find_item(int page, int slot);

static void cp_shop_own_slot(int page, int slot)
{
    int index = cp_shop_find_item(page, slot);

    if (index >= 0)
        cp_shop_own_item(index);
}

static int cp_shop_first_item(int page)
{
    int i;

    for (i = 0; i < game.shop_item_count; i++)
    {
        if (game.shop_items[i].page == page)
            return i;
    }
    return 0;
}

static int cp_shop_find_item(int page, int slot)
{
    int i;

    for (i = 0; i < game.shop_item_count; i++)
    {
        if (game.shop_items[i].page == page &&
            game.shop_items[i].slot == slot)
            return i;
    }
    return -1;
}

static bool cp_load_shop_page(int page)
{
    char path[MAX_PATH];

    rb->snprintf(path, sizeof(path), CP_SHOP_PAGE_PATTERN, page);
    if (!cp_load_scene(path, CP_VIEW_W, CP_VIEW_H))
        return false;
    game.shop_page = page;
    game.shop_selection = cp_shop_first_item(page);
    game.dirty = true;
    return true;
}

static bool cp_enter_shop(void)
{
    if (game.shop_item_count <= 0)
        return false;
    game.shop_return_room = game.room_index;
    game.shop_return_x = game.x;
    game.shop_return_y = game.y;
    if (!cp_load_shop_page(0))
        return false;
    game.scene_type = CP_SCENE_SHOP;
    cp_copy(game.shop_status, sizeof(game.shop_status), "Penguin Style");
    return true;
}

static void cp_leave_shop(void)
{
    if (!cp_load_player_asset() ||
        !cp_enter_room(game.shop_return_room, game.shop_return_x,
                       game.shop_return_y))
        return;
    cp_write_save();
}

static void cp_shop_move(int direction)
{
    int index = game.shop_selection;
    int tries;

    for (tries = 0; tries < game.shop_item_count; tries++)
    {
        index += direction;
        if (index < 0)
            index = game.shop_item_count - 1;
        else if (index >= game.shop_item_count)
            index = 0;
        if (game.shop_items[index].page == game.shop_page)
        {
            game.shop_selection = index;
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    cp_shop_item_owned(index) ? "Owned" : "Select to buy");
            game.dirty = true;
            return;
        }
    }
}

static void cp_shop_purchase(void)
{
    int index = game.shop_selection;
    int selected;
    int page;
    struct cp_shop_item *item;

    if (index < 0 || index >= game.shop_item_count)
        return;
    item = &game.shop_items[index];
    if (!cp_shop_item_owned(index))
    {
        if (game.coins < item->cost)
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Not enough coins");
            game.dirty = true;
            return;
        }
        game.coins -= item->cost;
        cp_shop_own_item(index);
        cp_copy(game.shop_status, sizeof(game.shop_status), "Purchased");
    }
    else
        cp_copy(game.shop_status, sizeof(game.shop_status), "Equipped");

    if (item->page == CP_FACE_PAGE && item->slot == 0)
    {
        game.equipped[0] = CP_OLIVER_SLOT;
        game.equipped[1] = CP_OLIVER_SLOT;
        game.equipped[2] = CP_OLIVER_SLOT;
        game.equipped[CP_COLOR_PAGE] = 2;
        game.equipped[CP_FACE_PAGE] = 0;
        game.sport_equipped[0] = 0;
        game.sport_equipped[1] = 0;
        game.sport_equipped[2] = 0;
        game.sport_equipped[3] = 0;
        game.sport_equipped[4] = 0;
        game.sport_equipped[5] = 0;
        cp_shop_own_slot(0, CP_OLIVER_SLOT);
        cp_shop_own_slot(1, CP_OLIVER_SLOT);
        cp_shop_own_slot(2, CP_OLIVER_SLOT);
        cp_shop_own_slot(CP_COLOR_PAGE, 2);
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Full look equipped");
    }
    else
    {
        game.equipped[item->page] = item->slot;
        if (item->page >= 0 && item->page <= 2)
            game.sport_equipped[item->page] = 0;
        else if (item->page == CP_FACE_PAGE)
            game.sport_equipped[5] = 0;
    }

    selected = game.shop_selection;
    page = game.shop_page;
    if (cp_load_player_asset() && cp_load_shop_page(page))
        game.shop_selection = selected;
    cp_write_save();
    game.dirty = true;
}

static void cp_shop_remove(void)
{
    int page = game.shop_page;
    int selected = game.shop_selection;

    if (page == CP_COLOR_PAGE)
    {
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Choose another color");
        game.dirty = true;
        return;
    }
    game.equipped[page] = -1;
    if (page >= 0 && page <= 2)
        game.sport_equipped[page] = 0;
    else if (page == CP_FACE_PAGE)
        game.sport_equipped[5] = 0;
    cp_copy(game.shop_status, sizeof(game.shop_status), "Item removed");
    if (cp_load_player_asset() && cp_load_shop_page(page))
        game.shop_selection = selected;
    cp_write_save();
    game.dirty = true;
}

static void cp_shop_action(int action)
{
    if ((action == CP_UP_ACTION || action == CP_UP_REPEAT) &&
        cp_menu_held())
    {
        cp_leave_shop();
        return;
    }
    if ((action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT) &&
        cp_play_held())
        return;
    if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT ||
        action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
    {
        int direction = (action == CP_RIGHT_ACTION ||
                         action == CP_RIGHT_REPEAT) ? 1 : -1;
        int page = game.shop_page + direction;

        if (page < 0)
            page = CP_SHOP_PAGES - 1;
        else if (page >= CP_SHOP_PAGES)
            page = 0;
        cp_load_shop_page(page);
        cp_copy(game.shop_status, sizeof(game.shop_status), "Penguin Style");
    }
    else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
        cp_shop_move(-1);
    else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
        cp_shop_move(1);
    else if (action == CP_SELECT_ACTION)
        cp_shop_purchase();
    else if (action == CP_PLAY_ACTION)
        cp_shop_remove();
}

static void cp_remember_special_return(void)
{
    game.special_return_room = game.room_index;
    game.special_return_x = game.x;
    game.special_return_y = game.y;
}

static void cp_leave_special(const char *message)
{
    cp_load_player_asset();
    if (game.puffle_owned_mask != 0)
        cp_load_puffle_asset(game.puffle_type);
    else
        cp_load_puffle_asset(0);
    if (game.special_return_room >= 0 &&
        game.special_return_room < game.room_count)
        cp_enter_room(game.special_return_room, game.special_return_x,
                      game.special_return_y);
    cp_set_message(message);
    cp_write_save();
}

static bool cp_read_puffle_adopt_item(int page, int wanted,
                                      int *type, int *cost)
{
    char line[CP_FURNITURE_LINE];
    int index = 0;
    int fd = rb->open(CP_PUFFLE_ADOPT_DATA_FILE, O_RDONLY);

    if (fd < 0)
        return false;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *cursor = line;
        char *page_text;
        char *type_text;
        char *name;
        char *cost_text;
        int candidate_page;
        int candidate_type;
        int candidate_cost;

        if (!cp_data_line(line))
            continue;
        page_text = cp_next_field(&cursor);
        type_text = cp_next_field(&cursor);
        name = cp_next_field(&cursor);
        cost_text = cp_next_field(&cursor);
        if (!cp_parse_int(page_text, &candidate_page) ||
            !cp_parse_int(type_text, &candidate_type) ||
            !cp_has_text(name) ||
            !cp_parse_int(cost_text, &candidate_cost) ||
            candidate_page < 1 ||
            candidate_page > CP_PUFFLE_ADOPT_PAGES ||
            candidate_type < 0 ||
            candidate_type >= CP_REGULAR_PUFFLE_TYPES ||
            candidate_cost < 0)
            continue;
        if (candidate_page != page)
            continue;
        if (index++ == wanted)
        {
            *type = candidate_type;
            *cost = candidate_cost;
            rb->close(fd);
            return true;
        }
    }
    rb->close(fd);
    return false;
}

static int cp_count_puffle_adopt_items(int page)
{
    int type;
    int cost;
    int count = 0;

    while (cp_read_puffle_adopt_item(page, count, &type, &cost))
        count++;
    return count;
}

static void cp_update_puffle_adopt_status(void)
{
    int type = game.puffle_menu;

    if (game.puffle_adopt_count <= 0)
    {
        cp_copy(game.shop_status, sizeof(game.shop_status),
                game.puffle_adopt_page == 1 ?
                "Adopt a Puffle - February 2011" :
                "Original puffle-care information");
    }
    else if (game.puffle_type == type && cp_puffle_is_owned(type))
        cp_copy(game.shop_status, sizeof(game.shop_status), "ACTIVE");
    else if (cp_puffle_is_owned(type))
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Select to activate");
    else
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Select to adopt");
}

static bool cp_load_puffle_adopt_page(int page)
{
    char path[MAX_PATH];

    if (page < 1 || page > CP_PUFFLE_ADOPT_PAGES)
        return false;
    rb->snprintf(path, sizeof(path), CP_PUFFLE_ADOPT_PAGE_PATTERN, page);
    if (!cp_load_scene(path, CP_VIEW_W, CP_VIEW_H))
        return false;
    game.puffle_adopt_page = page;
    game.puffle_adopt_index = 0;
    game.puffle_adopt_count = cp_count_puffle_adopt_items(page);
    game.puffle_adopt_cost = 0;
    if (game.puffle_adopt_count > 0 &&
        !cp_read_puffle_adopt_item(page, 0, &game.puffle_menu,
                                   &game.puffle_adopt_cost))
        return false;
    cp_update_puffle_adopt_status();
    game.dirty = true;
    return true;
}

static void cp_enter_puffle_shop(void)
{
    cp_remember_special_return();
    game.scene_type = CP_SCENE_PUFFLE_SHOP;
    if (!cp_load_puffle_adopt_page(1))
    {
        cp_leave_special("Adoption catalog assets are incomplete.");
        return;
    }
    cp_copy(game.location, sizeof(game.location), "Adopt a Puffle");
}

static void cp_enter_puffle_care(void)
{
    char title[32];
    int i;

    if (game.puffle_owned_mask == 0)
    {
        cp_set_message("Adopt a puffle at the Pet Shop first.");
        return;
    }
    if (!cp_puffle_is_owned(game.puffle_type))
    {
        for (i = 0; i < CP_PUFFLE_TYPES; i++)
        {
            if (cp_puffle_is_owned(i))
            {
                game.puffle_type = i;
                break;
            }
        }
    }
    cp_remember_special_return();
    if (!cp_load_scene(CP_PUFFLE_CARE_BACKGROUND_FILE,
                       CP_VIEW_W, CP_VIEW_H) ||
        !cp_load_puffle_care_icons())
    {
        cp_load_player_asset();
        cp_enter_room(game.special_return_room, game.special_return_x,
                      game.special_return_y);
        cp_set_message("Authentic puffle care UI is missing.");
        return;
    }
    cp_load_puffle_asset(game.puffle_type);
    cp_load_equipped_puffle_hat();
    game.puffle_digging = false;
    game.puffle_eating = false;
    game.puffle_toy_playing = false;
    game.puffle_trick_playing = false;
    game.puffle_action_ticks = 0;
    game.puffle_toy_ticks = 0;
    game.puffle_toy_frame = 0;
    game.puffle_trick_ticks = 0;
    game.puffle_trick_frame = 0;
    game.puffle_frame = 0;
    game.scene_type = CP_SCENE_PUFFLE_CARE;
    game.puffle_menu = 0;
    cp_copy(game.shop_status, sizeof(game.shop_status), "Choose puffle care");
    rb->snprintf(title, sizeof(title), "%s Puffle Care",
                 puffle_names[game.puffle_type]);
    cp_copy(game.location, sizeof(game.location), title);
    game.dirty = true;
}

static bool cp_enter_puffle_food(void)
{
    game.puffle_food_count = cp_count_puffle_food_items();
    if (game.puffle_food_count != CP_PUFFLE_FOOD_ITEMS ||
        !cp_load_puffle_food_item(game.puffle_food_index))
    {
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Puffle food archive missing");
        game.dirty = true;
        return false;
    }
    game.scene_type = CP_SCENE_PUFFLE_FOOD;
    cp_copy(game.location, sizeof(game.location), "Puffle Food");
    cp_copy(game.shop_status, sizeof(game.shop_status),
            "Select to feed");
    game.dirty = true;
    return true;
}

static bool cp_enter_puffle_tricks(void)
{
    game.puffle_trick_playing = false;
    game.puffle_trick_ticks = 0;
    game.puffle_trick_frame = 0;
    if (!cp_load_puffle_trick(game.puffle_type,
                              game.puffle_trick_index))
    {
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Authentic puffle trick archive missing");
        game.dirty = true;
        return false;
    }
    game.scene_type = CP_SCENE_PUFFLE_TRICKS;
    cp_copy(game.location, sizeof(game.location), "Puffle Tricks");
    cp_copy(game.shop_status, sizeof(game.shop_status),
            "Select to perform");
    game.dirty = true;
    return true;
}

static void cp_update_puffle_toy_status(void)
{
    if (game.puffle_toy_index == 0 ||
        (game.puffle_toy_owned & (1u << game.puffle_type)) != 0)
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Select to play");
    else
        rb->snprintf(game.shop_status, sizeof(game.shop_status),
                     "Select to buy for %d", game.puffle_toy_item.cost);
}

static bool cp_enter_puffle_toys(void)
{
    game.puffle_toy_playing = false;
    game.puffle_toy_ticks = 0;
    game.puffle_toy_frame = 0;
    if (cp_count_puffle_toy_items() != CP_PUFFLE_TOY_ITEMS ||
        !cp_load_puffle_toy_item(game.puffle_toy_index))
    {
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Authentic puffle toy archive missing");
        game.dirty = true;
        return false;
    }
    game.scene_type = CP_SCENE_PUFFLE_TOYS;
    cp_copy(game.location, sizeof(game.location), "Puffle Toys");
    cp_update_puffle_toy_status();
    game.dirty = true;
    return true;
}

static void cp_update_puffle_hat_status(void)
{
    struct cp_puffle_hat_item *item = &game.puffle_hat_item;

    if (!item->available)
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Icon only; SWF missing");
    else if (game.puffle_hat_equipped[game.puffle_type] == item->id)
        cp_copy(game.shop_status, sizeof(game.shop_status), "EQUIPPED");
    else if (cp_puffle_hat_owned(game.puffle_hat_index))
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Select to equip");
    else
        cp_copy(game.shop_status, sizeof(game.shop_status), "Select to buy");
}

static bool cp_enter_puffle_hats(void)
{
    int equipped = game.puffle_hat_equipped[game.puffle_type];
    int index;

    game.puffle_hat_count = cp_count_puffle_hat_items();
    if (game.puffle_hat_count != CP_PUFFLE_HAT_ITEMS)
    {
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Puffle hat catalog incomplete");
        game.dirty = true;
        return false;
    }
    if (equipped > 0)
    {
        for (index = 0; index < game.puffle_hat_count; index++)
        {
            struct cp_puffle_hat_item item;

            if (cp_read_puffle_hat_item(index, &item) &&
                item.id == equipped)
            {
                game.puffle_hat_index = index;
                break;
            }
        }
    }
    if (!cp_load_puffle_hat_item(game.puffle_hat_index))
    {
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Authentic puffle hat assets missing");
        game.dirty = true;
        return false;
    }
    game.puffle_hat_loaded = game.puffle_hat_item.available;
    game.scene_type = CP_SCENE_PUFFLE_HATS;
    cp_copy(game.location, sizeof(game.location), "Puffle Hats");
    cp_update_puffle_hat_status();
    game.dirty = true;
    return true;
}

static bool cp_refresh_igloo_edit(void)
{
    game.furniture_selected_slot =
        cp_find_furniture_slot(game.furniture_item.id);
    return cp_compose_current_igloo() &&
           cp_bake_furniture_except(game.furniture_selected_slot) &&
           cp_load_furniture_asset(game.furniture_item.id);
}

static void cp_enter_igloo_edit(void)
{
    cp_remember_special_return();
    game.furniture_catalog_index = 0;
    if (game.furniture_catalog_count <= 0 ||
        !cp_read_furniture_item(0, &game.furniture_item) ||
        !cp_refresh_igloo_edit())
    {
        cp_load_player_asset();
        cp_set_message("Igloo furniture assets are incomplete.");
        return;
    }
    game.scene_type = CP_SCENE_IGLOO_EDIT;
    cp_copy(game.location, sizeof(game.location), "Edit Igloo");
    game.dirty = true;
}

static void cp_change_igloo_item(int direction)
{
    int next;

    if (game.furniture_catalog_count <= 0)
        return;
    next = game.furniture_catalog_index + direction;
    if (next < 0)
        next = game.furniture_catalog_count - 1;
    else if (next >= game.furniture_catalog_count)
        next = 0;
    if (!cp_read_furniture_item(next, &game.furniture_item))
    {
        cp_set_message("Furniture catalog entry is invalid.");
        return;
    }
    game.furniture_catalog_index = next;
    if (!cp_refresh_igloo_edit())
        cp_set_message("Furniture art is missing from the package.");
}

static void cp_change_igloo_furniture_type(int direction)
{
    char line[CP_FURNITURE_LINE];
    int fd;

    game.furniture_catalog_type += direction;
    if (game.furniture_catalog_type < 1)
        game.furniture_catalog_type = 4;
    else if (game.furniture_catalog_type > 4)
        game.furniture_catalog_type = 1;

    fd = rb->open(CP_FURNITURE_DATA_FILE, O_RDONLY);
    if (fd < 0)
        return;
    game.furniture_catalog_count = 0;
    while (rb->read_line(fd, line, sizeof(line)) > 0)
    {
        struct cp_furniture_item item;

        if (cp_data_line(line) && cp_parse_furniture_line(line, &item) &&
            item.type == game.furniture_catalog_type)
            game.furniture_catalog_count++;
    }
    rb->close(fd);
    game.furniture_catalog_index = 0;
    if (game.furniture_catalog_count <= 0 ||
        !cp_read_furniture_item(0, &game.furniture_item) ||
        !cp_refresh_igloo_edit())
        cp_set_message("Furniture category is incomplete.");
}

static bool cp_add_current_furniture_placement(void)
{
    int owned = cp_furniture_quantity(game.furniture_item.id);
    int placed = cp_furniture_placed_count(game.furniture_item.id);
    int slot;

    if (placed >= owned)
    {
        cp_set_message(owned == 0 ? "Buy this item with Play first." :
                       "Every owned copy is already placed.");
        return false;
    }
    for (slot = 0; slot < CP_MAX_FURNITURE_PLACEMENTS; slot++)
    {
        if (game.furniture_placements[slot].item_id == 0)
            break;
    }
    if (slot >= CP_MAX_FURNITURE_PLACEMENTS)
    {
        cp_set_message("Store an item before placing another.");
        return false;
    }
    game.furniture_placements[slot].item_id = game.furniture_item.id;
    game.furniture_placements[slot].x =
        CP_VIEW_W / 2 + ((placed % 3) - 1) * 12;
    game.furniture_placements[slot].y = 175 - (placed / 3) * 6;
    game.furniture_selected_slot = slot;
    return true;
}

static bool cp_purchase_current_furniture(void)
{
    char message[64];
    int quantity = cp_furniture_quantity(game.furniture_item.id);

    if (quantity >= game.furniture_item.max_quantity)
    {
        cp_set_message("Official quantity limit reached.");
        return false;
    }
    if (game.coins < game.furniture_item.cost)
    {
        rb->snprintf(message, sizeof(message), "Need %d coins.",
                     game.furniture_item.cost);
        cp_set_message(message);
        return false;
    }
    if (!cp_set_furniture_quantity(game.furniture_item.id, quantity + 1))
    {
        cp_set_message("Could not save furniture inventory.");
        return false;
    }
    game.coins -= game.furniture_item.cost;
    rb->snprintf(message, sizeof(message), "%s purchased (%d owned).",
                 game.furniture_item.name, quantity + 1);
    cp_set_message(message);
    return true;
}

static void cp_toggle_igloo_item(void)
{
    int slot = game.furniture_selected_slot;

    if (slot >= 0)
    {
        game.furniture_placements[slot].item_id = 0;
        game.furniture_selected_slot = -1;
    }
    else
    {
        if (!cp_add_current_furniture_placement())
            return;
    }
    if (!cp_refresh_igloo_edit())
        cp_set_message("Furniture art is missing from the package.");
}

static void cp_buy_or_add_igloo_item(void)
{
    int owned = cp_furniture_quantity(game.furniture_item.id);
    int placed = cp_furniture_placed_count(game.furniture_item.id);
    bool place_after_purchase = game.furniture_selected_slot >= 0;

    if (placed < owned)
    {
        if (!cp_add_current_furniture_placement())
            return;
    }
    else
    {
        if (!cp_purchase_current_furniture())
            return;
        if (place_after_purchase && !cp_add_current_furniture_placement())
            return;
    }
    if (!cp_refresh_igloo_edit())
        cp_set_message("Furniture art is missing from the package.");
}

static void cp_place_or_buy_igloo_item(void)
{
    if (game.furniture_selected_slot < 0 &&
        cp_furniture_quantity(game.furniture_item.id) == 0)
    {
        if (!cp_purchase_current_furniture() ||
            !cp_add_current_furniture_placement())
            return;
        if (!cp_refresh_igloo_edit())
            cp_set_message("Furniture art is missing from the package.");
        cp_write_save();
        return;
    }
    cp_toggle_igloo_item();
}

static bool cp_refresh_igloo_catalog(void)
{
    int building = game.igloo_building;
    int floor = game.igloo_floor;
    int location = game.igloo_location;

    if (game.igloo_catalog_tab == 0)
        building = game.igloo_item.id;
    else if (game.igloo_catalog_tab == 1)
        floor = game.igloo_item.id;
    else
        location = game.igloo_item.id;
    return cp_compose_igloo(building, floor, location) &&
           cp_bake_furniture_except(-1);
}

static bool cp_select_current_igloo_catalog_item(void)
{
    int wanted = cp_current_igloo_item(game.igloo_catalog_tab);
    int i;

    game.igloo_catalog_index = 0;
    for (i = 0; i < game.igloo_catalog_count[game.igloo_catalog_tab]; i++)
    {
        if (!cp_read_igloo_item(game.igloo_catalog_tab, i,
                                &game.igloo_item))
            return false;
        if (game.igloo_item.id == wanted)
        {
            game.igloo_catalog_index = i;
            return true;
        }
    }
    return cp_read_igloo_item(game.igloo_catalog_tab, 0,
                              &game.igloo_item);
}

static void cp_enter_igloo_catalog(void)
{
    cp_remember_special_return();
    game.igloo_catalog_tab = 0;
    if (!cp_select_current_igloo_catalog_item() ||
        !cp_refresh_igloo_catalog())
    {
        cp_load_player_asset();
        cp_set_message("Igloo upgrade assets are incomplete.");
        return;
    }
    game.scene_type = CP_SCENE_IGLOO_CATALOG;
    cp_copy(game.location, sizeof(game.location), "Igloo Upgrades");
    game.dirty = true;
}

static void cp_change_igloo_catalog_tab(int direction)
{
    game.igloo_catalog_tab =
        (game.igloo_catalog_tab + direction + CP_IGLOO_TABS) %
        CP_IGLOO_TABS;
    if (!cp_select_current_igloo_catalog_item() ||
        !cp_refresh_igloo_catalog())
        cp_set_message("Igloo catalog art is missing.");
}

static void cp_change_igloo_catalog_item(int direction)
{
    int count = game.igloo_catalog_count[game.igloo_catalog_tab];

    if (count <= 0)
        return;
    game.igloo_catalog_index =
        (game.igloo_catalog_index + direction + count) % count;
    if (!cp_read_igloo_item(game.igloo_catalog_tab,
                            game.igloo_catalog_index, &game.igloo_item) ||
        !cp_refresh_igloo_catalog())
        cp_set_message("Igloo catalog art is missing.");
}

static void cp_buy_or_use_igloo_catalog_item(void)
{
    bool owned = cp_igloo_item_owned(game.igloo_catalog_tab,
                                     game.igloo_item.id);
    bool service = game.igloo_item.id == 0 && game.igloo_item.cost > 0;
    char message[72];

    if ((!owned || service) && game.coins < game.igloo_item.cost)
    {
        rb->snprintf(message, sizeof(message), "Need %d coins.",
                     game.igloo_item.cost);
        cp_set_message(message);
        return;
    }
    if (!owned || service)
        game.coins -= game.igloo_item.cost;
    if (!owned)
        cp_own_igloo_item(game.igloo_catalog_tab, game.igloo_item.id);
    cp_set_current_igloo_item(game.igloo_catalog_tab, game.igloo_item.id);
    if (!cp_refresh_igloo_catalog())
    {
        cp_set_message("Igloo catalog art is missing.");
        return;
    }
    rb->snprintf(message, sizeof(message), "%s %s.",
                 game.igloo_item.name, owned ? "selected" : "purchased");
    cp_set_message(message);
    cp_write_save();
}

static bool cp_load_exact_catalog_page(enum cp_scene_type scene, int page)
{
    char path[MAX_PATH];
    int pages;
    int count;

    if (scene == CP_SCENE_FURNITURE_CATALOG)
    {
        pages = CP_FURNITURE_CATALOG_PAGES;
        rb->snprintf(path, sizeof(path),
                     CP_FURNITURE_CATALOG_PAGE_PATTERN, page);
        count = cp_count_exact_furniture_items(page);
    }
    else
    {
        pages = CP_IGLOO_BOOK_PAGES;
        rb->snprintf(path, sizeof(path), CP_IGLOO_BOOK_PAGE_PATTERN, page);
        count = cp_count_exact_igloo_items(page);
    }
    if (page < 1 || page > pages || count < 0 ||
        !cp_load_scene(path, CP_VIEW_W, CP_VIEW_H))
        return false;

    game.exact_catalog_page = page;
    game.exact_catalog_count = count;
    if (count <= 0)
    {
        game.exact_catalog_index = 0;
        game.furniture_item.id = 0;
        game.igloo_item.id = 0;
    }
    else
    {
        game.exact_catalog_index = MIN(game.exact_catalog_index, count - 1);
        if (scene == CP_SCENE_FURNITURE_CATALOG)
        {
            if (!cp_read_exact_furniture_item(
                    page, game.exact_catalog_index, &game.furniture_item))
                return false;
        }
        else if (!cp_read_exact_igloo_item(
                     page, game.exact_catalog_index,
                     &game.igloo_catalog_tab, &game.igloo_item))
            return false;
    }
    game.scene_type = scene;
    cp_copy(game.location, sizeof(game.location),
            scene == CP_SCENE_FURNITURE_CATALOG ?
            "Better Igloos" : "Igloo Upgrades");
    game.dirty = true;
    return true;
}

static void cp_enter_furniture_catalog(void)
{
    cp_remember_special_return();
    game.exact_catalog_index = 0;
    if (!cp_load_exact_catalog_page(CP_SCENE_FURNITURE_CATALOG, 1))
        cp_set_message("April 2012 Better Igloos archive is incomplete.");
}

static void cp_enter_igloo_book(void)
{
    cp_remember_special_return();
    game.exact_catalog_index = 0;
    if (!cp_load_exact_catalog_page(CP_SCENE_IGLOO_BOOK, 1))
        cp_set_message("February 2012 Igloo Upgrades archive is incomplete.");
}

static bool cp_load_pet_furniture_page(int page)
{
    char path[MAX_PATH];
    int count;

    if (page < 1 || page > CP_PET_FURNITURE_PAGES)
        return false;
    if (page == 1 || page == CP_PET_FURNITURE_PAGES)
        game.pet_furniture_secret = false;
    count = cp_count_pet_furniture_items(page,
                                         game.pet_furniture_secret);
    rb->snprintf(path, sizeof(path),
                 game.pet_furniture_secret ?
                 CP_PET_FURNITURE_SECRET_PATTERN :
                 CP_PET_FURNITURE_PAGE_PATTERN, page);
    if (count < 0 || !cp_load_scene(path, CP_VIEW_W, CP_VIEW_H))
        return false;
    game.exact_catalog_page = page;
    game.exact_catalog_count = count;
    game.exact_catalog_index = 0;
    game.furniture_item.id = 0;
    if (count > 0 && !cp_read_pet_furniture_item(
            page, game.pet_furniture_secret, 0, &game.furniture_item))
        return false;
    game.scene_type = CP_SCENE_PET_FURNITURE;
    cp_copy(game.location, sizeof(game.location), "Pet Furniture");
    game.dirty = true;
    return true;
}

static void cp_enter_pet_furniture(void)
{
    cp_remember_special_return();
    game.pet_furniture_secret = false;
    if (!cp_load_pet_furniture_page(1))
        cp_set_message("March 2010 Pet Furniture archive is incomplete.");
}

static void cp_change_pet_furniture_page(int direction)
{
    int page = game.exact_catalog_page + direction;

    if (page < 1)
        page = CP_PET_FURNITURE_PAGES;
    else if (page > CP_PET_FURNITURE_PAGES)
        page = 1;
    game.pet_furniture_secret = false;
    if (!cp_load_pet_furniture_page(page))
        cp_set_message("Authentic Pet Furniture page is missing.");
}

static void cp_change_pet_furniture_item(int direction)
{
    if (game.exact_catalog_count <= 0)
        return;
    game.exact_catalog_index =
        (game.exact_catalog_index + direction + game.exact_catalog_count) %
        game.exact_catalog_count;
    if (!cp_read_pet_furniture_item(
            game.exact_catalog_page, game.pet_furniture_secret,
            game.exact_catalog_index, &game.furniture_item))
        cp_set_message("Pet Furniture catalog entry is invalid.");
}

static void cp_toggle_pet_furniture_secret(void)
{
    int page = game.exact_catalog_page;

    if (page < 2 || page > 4)
    {
        cp_set_message("This page has no secret item.");
        return;
    }
    game.pet_furniture_secret = !game.pet_furniture_secret;
    if (!cp_load_pet_furniture_page(page))
    {
        game.pet_furniture_secret = !game.pet_furniture_secret;
        cp_set_message("Authentic secret-item art is missing.");
    }
}

static void cp_change_exact_catalog_page(int direction)
{
    enum cp_scene_type scene = game.scene_type;
    int pages = scene == CP_SCENE_FURNITURE_CATALOG ?
                CP_FURNITURE_CATALOG_PAGES : CP_IGLOO_BOOK_PAGES;
    int page = game.exact_catalog_page + direction;

    if (page < 1)
        page = pages;
    else if (page > pages)
        page = 1;
    game.exact_catalog_index = 0;
    if (!cp_load_exact_catalog_page(scene, page))
        cp_set_message("Authentic catalog page is missing.");
}

static void cp_change_exact_catalog_item(int direction)
{
    if (game.exact_catalog_count <= 0)
        return;
    game.exact_catalog_index =
        (game.exact_catalog_index + direction + game.exact_catalog_count) %
        game.exact_catalog_count;
    if (game.scene_type == CP_SCENE_FURNITURE_CATALOG)
    {
        if (!cp_read_exact_furniture_item(
                game.exact_catalog_page, game.exact_catalog_index,
                &game.furniture_item))
            cp_set_message("Furniture catalog entry is invalid.");
    }
    else if (!cp_read_exact_igloo_item(
                 game.exact_catalog_page, game.exact_catalog_index,
                 &game.igloo_catalog_tab, &game.igloo_item))
        cp_set_message("Igloo catalog entry is invalid.");
}

static void cp_purchase_exact_furniture(void)
{
    if (game.exact_catalog_count <= 0)
    {
        cp_set_message("This is a cover page.");
        return;
    }
    if (cp_purchase_current_furniture())
        cp_write_save();
}

static void cp_buy_or_use_exact_igloo_item(void)
{
    bool owned;
    bool service;
    char message[72];

    if (game.exact_catalog_count <= 0)
    {
        cp_set_message("This is a cover page.");
        return;
    }
    owned = cp_igloo_item_owned(game.igloo_catalog_tab,
                                game.igloo_item.id);
    service = game.igloo_catalog_tab == 1 && game.igloo_item.id == 0 &&
              game.igloo_item.cost > 0;
    if ((!owned || service) && game.coins < game.igloo_item.cost)
    {
        rb->snprintf(message, sizeof(message), "Need %d coins.",
                     game.igloo_item.cost);
        cp_set_message(message);
        return;
    }
    if (!owned || service)
        game.coins -= game.igloo_item.cost;
    if (!owned)
        cp_own_igloo_item(game.igloo_catalog_tab, game.igloo_item.id);
    cp_set_current_igloo_item(game.igloo_catalog_tab, game.igloo_item.id);
    rb->snprintf(message, sizeof(message), "%s %s.",
                 game.igloo_item.name,
                 (!owned || service) ? "purchased" : "selected");
    cp_set_message(message);
    cp_write_save();
}

static void cp_puffle_adopt(void)
{
    int type = game.puffle_menu;

    if (cp_puffle_is_owned(type))
    {
        game.puffle_type = type;
        rb->snprintf(game.shop_status, sizeof(game.shop_status),
                     "%s puffle is now active", puffle_names[type]);
        cp_write_save();
        game.dirty = true;
        return;
    }
    if (game.coins < game.puffle_adopt_cost)
    {
        rb->snprintf(game.shop_status, sizeof(game.shop_status),
                     "Need %d coins", game.puffle_adopt_cost);
        game.dirty = true;
        return;
    }
    game.coins -= game.puffle_adopt_cost;
    game.puffle_owned_mask |= 1u << type;
    game.puffle_type = type;
    game.puffle_food[type] = 70;
    game.puffle_rest[type] = 70;
    game.puffle_happy[type] = 70;
    game.puffle_clean[type] = 70;
    rb->snprintf(game.shop_status, sizeof(game.shop_status),
                 "%s puffle adopted!", puffle_names[type]);
    cp_write_save();
    game.dirty = true;
}

static void cp_unlock_special_puffle(int type)
{
    unsigned int regular_mask =
        game.puffle_owned_mask & ((1u << CP_REGULAR_PUFFLE_TYPES) - 1);

    if (regular_mask == 0)
    {
        cp_set_message("Adopt a Pet Shop puffle before this quest.");
        return;
    }
    if (!cp_puffle_is_owned(type))
    {
        game.puffle_owned_mask |= 1u << type;
        game.puffle_food[type] = 80;
        game.puffle_rest[type] = 80;
        game.puffle_happy[type] = 100;
        game.puffle_clean[type] = 80;
    }
    game.puffle_type = type;
    cp_load_puffle_asset(type);
    rb->snprintf(game.shop_status, sizeof(game.shop_status),
                 "%s puffle is now active", puffle_names[type]);
    cp_set_message(type == CP_RAINBOW_PUFFLE ?
                   "Rainbow Puffle quest complete!" :
                   "Gold Puffle quest complete!");
    cp_write_save();
    game.dirty = true;
}

static void cp_puffle_care_action(void)
{
    static const int rare_food[] =
    {
        1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13
    };
    int type = game.puffle_type;

    if (game.puffle_menu == 0)
    {
        cp_enter_puffle_food();
        return;
    }
    else if (game.puffle_menu == 1)
    {
        cp_enter_puffle_toys();
        return;
    }
    else if (game.puffle_menu == 2)
    {
        game.puffle_food[type] = MAX(0, game.puffle_food[type] - 5);
        game.puffle_rest[type] = MIN(100, game.puffle_rest[type] + 80);
        game.puffle_happy[type] = MAX(0, game.puffle_happy[type] - 10);
        game.puffle_frame = 7;
        cp_copy(game.shop_status, sizeof(game.shop_status), "Puffle rested");
    }
    else if (game.puffle_menu == 3)
    {
        game.puffle_food[type] = MAX(0, game.puffle_food[type] - 5);
        game.puffle_rest[type] = MAX(0, game.puffle_rest[type] - 5);
        game.puffle_happy[type] = MIN(100, game.puffle_happy[type] + 10);
        game.puffle_clean[type] = MIN(100, game.puffle_clean[type] + 100);
        game.puffle_frame = 5;
        cp_copy(game.shop_status, sizeof(game.shop_status), "Puffle bathed");
    }
    else if (game.puffle_menu == 4)
    {
        game.puffle_walking = !game.puffle_walking;
        cp_copy(game.shop_status, sizeof(game.shop_status),
                game.puffle_walking ? "Walking with you" :
                "Staying in the igloo");
        cp_write_save();
        game.dirty = true;
        return;
    }
    else
    {
        struct cp_puffle_food_item reward;
        int reward_index = rare_food[rb->rand() % ARRAYLEN(rare_food)];

        if (!cp_load_puffle_dig(type))
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Dig animation missing");
            game.dirty = true;
            return;
        }
        game.puffle_food_quantity[reward_index] =
            MIN(99, game.puffle_food_quantity[reward_index] + 1);
        game.puffle_frame = 0;
        game.puffle_action_ticks = CP_PUFFLE_FRAMES;
        game.puffle_digging = true;
        if (cp_read_puffle_food_item(reward_index, &reward))
            rb->snprintf(game.shop_status, sizeof(game.shop_status),
                         "Dug up %s!", reward.name);
        else
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Treasure found!");
        cp_write_save();
        game.dirty = true;
        return;
    }
    game.puffle_action_ticks = 6;
    cp_write_save();
    game.dirty = true;
}

static void cp_puffle_food_move(int direction)
{
    int index = game.puffle_food_index + direction;

    if (index < 0)
        index = game.puffle_food_count - 1;
    else if (index >= game.puffle_food_count)
        index = 0;
    if (cp_load_puffle_food_item(index))
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Select to feed");
    game.dirty = true;
}

static void cp_puffle_feed(void)
{
    struct cp_puffle_food_item *item = &game.puffle_food_item;
    int type = game.puffle_type;
    int index = game.puffle_food_index;

    if (item->cost >= CP_PUFFLE_RARE_COST)
    {
        if (game.puffle_food_quantity[index] <= 0)
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Find this by digging");
            game.dirty = true;
            return;
        }
        game.puffle_food_quantity[index]--;
    }
    else if (item->cost > 0)
    {
        if (game.coins < item->cost)
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Not enough coins");
            game.dirty = true;
            return;
        }
        game.coins -= item->cost;
    }
    game.puffle_food[type] =
        MIN(100, MAX(0, game.puffle_food[type] + item->food));
    game.puffle_rest[type] =
        MIN(100, MAX(0, game.puffle_rest[type] + item->rest));
    game.puffle_happy[type] =
        MIN(100, MAX(0, game.puffle_happy[type] + item->happy));
    game.puffle_clean[type] =
        MIN(100, MAX(0, game.puffle_clean[type] + item->clean));
    game.puffle_frame = 0;
    if (cp_load_puffle_eat(type))
    {
        game.puffle_action_ticks = CP_PUFFLE_FRAMES;
        game.puffle_eating = true;
    }
    else
    {
        /* The pinned archive has no Gold Puffle eat timeline. Keep its
         * authentic idle art instead of substituting another color. */
        cp_load_puffle_asset(type);
        game.puffle_action_ticks = 6;
    }
    rb->snprintf(game.shop_status, sizeof(game.shop_status),
                 "Fed %s", item->name);
    cp_write_save();
    game.dirty = true;
}

static void cp_puffle_trick_move(int direction)
{
    int next = game.puffle_trick_index + direction;

    if (next < 0)
        next = CP_PUFFLE_TRICKS - 1;
    else if (next >= CP_PUFFLE_TRICKS)
        next = 0;
    game.puffle_trick_playing = false;
    game.puffle_trick_ticks = 0;
    game.puffle_trick_frame = 0;
    if (cp_load_puffle_trick(game.puffle_type, next))
    {
        game.puffle_trick_index = next;
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Select to perform");
    }
    else
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Authentic trick atlas missing");
    game.dirty = true;
}

static void cp_puffle_toy_move(int direction)
{
    int next = game.puffle_toy_index + direction;

    if (next < 0)
        next = CP_PUFFLE_TOYS_PER_TYPE - 1;
    else if (next >= CP_PUFFLE_TOYS_PER_TYPE)
        next = 0;
    game.puffle_toy_playing = false;
    game.puffle_toy_ticks = 0;
    game.puffle_toy_frame = 0;
    if (cp_load_puffle_toy_item(next))
        cp_update_puffle_toy_status();
    else
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Authentic toy atlas missing");
    game.dirty = true;
}

static void cp_puffle_play_toy(void)
{
    struct cp_puffle_toy_item *item = &game.puffle_toy_item;
    int type = game.puffle_type;

    if (game.puffle_toy_index == 1 &&
        (game.puffle_toy_owned & (1u << type)) == 0)
    {
        if (game.coins < item->cost)
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Not enough coins");
            game.dirty = true;
            return;
        }
        game.coins -= item->cost;
        game.puffle_toy_owned |= 1u << type;
    }
    game.puffle_food[type] =
        MIN(100, MAX(0, game.puffle_food[type] + item->food));
    game.puffle_rest[type] =
        MIN(100, MAX(0, game.puffle_rest[type] + item->rest));
    game.puffle_happy[type] =
        MIN(100, MAX(0, game.puffle_happy[type] + item->happy));
    game.puffle_clean[type] =
        MIN(100, MAX(0, game.puffle_clean[type] + item->clean));
    game.puffle_toy_frame = 0;
    game.puffle_toy_ticks = CP_PUFFLE_TOY_FRAMES;
    game.puffle_toy_playing = true;
    rb->snprintf(game.shop_status, sizeof(game.shop_status),
                 "Playing with %.30s", item->name);
    cp_write_save();
    game.dirty = true;
}

static void cp_puffle_perform_trick(void)
{
    game.puffle_trick_frame = 0;
    game.puffle_trick_ticks = CP_PUFFLE_TRICK_FRAMES;
    game.puffle_trick_playing = true;
    rb->snprintf(game.shop_status, sizeof(game.shop_status),
                 "Performing %s", puffle_trick_names[game.puffle_trick_index]);
    game.dirty = true;
}

static void cp_puffle_hat_move(int direction)
{
    int next = game.puffle_hat_index + direction;

    if (next < 0)
        next = game.puffle_hat_count - 1;
    else if (next >= game.puffle_hat_count)
        next = 0;
    if (cp_load_puffle_hat_item(next))
    {
        game.puffle_hat_loaded = game.puffle_hat_item.available;
        cp_update_puffle_hat_status();
    }
    else
    {
        game.puffle_hat_loaded = false;
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Authentic hat atlas missing");
    }
    game.dirty = true;
}

static void cp_puffle_hat_purchase(void)
{
    struct cp_puffle_hat_item *item = &game.puffle_hat_item;

    if (!item->available)
    {
        cp_copy(game.shop_status, sizeof(game.shop_status),
                "Missing SWF; no charge");
        game.dirty = true;
        return;
    }
    if (!cp_puffle_hat_owned(game.puffle_hat_index))
    {
        if (game.coins < item->cost)
        {
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Not enough coins");
            game.dirty = true;
            return;
        }
        game.coins -= item->cost;
        cp_own_puffle_hat(game.puffle_hat_index);
    }
    game.puffle_hat_equipped[game.puffle_type] = item->id;
    game.puffle_hat_loaded = true;
    cp_copy(game.shop_status, sizeof(game.shop_status), "EQUIPPED");
    cp_write_save();
    game.dirty = true;
}

static void cp_puffle_hat_remove(void)
{
    game.puffle_hat_equipped[game.puffle_type] = 0;
    game.puffle_hat_loaded = false;
    cp_copy(game.shop_status, sizeof(game.shop_status), "Hat removed");
    cp_write_save();
    game.dirty = true;
}

static void cp_cycle_owned_puffle(int direction)
{
    int type = game.puffle_type;
    int i;

    for (i = 0; i < CP_PUFFLE_TYPES; i++)
    {
        type += direction;
        if (type < 0)
            type = CP_PUFFLE_TYPES - 1;
        else if (type >= CP_PUFFLE_TYPES)
            type = 0;
        if (cp_puffle_is_owned(type))
        {
            game.puffle_type = type;
            if (game.scene_type == CP_SCENE_PUFFLE_TOYS)
            {
                game.puffle_toy_playing = false;
                game.puffle_toy_ticks = 0;
                game.puffle_toy_frame = 0;
                if (!cp_load_puffle_toy_item(game.puffle_toy_index))
                {
                    cp_copy(game.shop_status, sizeof(game.shop_status),
                            "Authentic toy atlas missing");
                    cp_write_save();
                    game.dirty = true;
                    return;
                }
            }
            else if (game.scene_type == CP_SCENE_PUFFLE_TRICKS)
                cp_load_puffle_trick(type, game.puffle_trick_index);
            else
            {
                cp_load_puffle_asset(type);
                if (game.scene_type == CP_SCENE_PUFFLE_CARE)
                    cp_load_equipped_puffle_hat();
            }
            if (game.scene_type == CP_SCENE_PUFFLE_TOYS)
                cp_update_puffle_toy_status();
            else if (game.scene_type == CP_SCENE_PUFFLE_HATS)
                cp_update_puffle_hat_status();
            else
                rb->snprintf(game.shop_status, sizeof(game.shop_status),
                             "%s puffle selected", puffle_names[type]);
            cp_write_save();
            return;
        }
    }
}

static void cp_special_action(int action)
{
    if ((action == CP_UP_ACTION || action == CP_UP_REPEAT) &&
        cp_menu_held())
    {
        if (game.scene_type == CP_SCENE_PUFFLE_FOOD ||
            game.scene_type == CP_SCENE_PUFFLE_TOYS ||
            game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
            game.scene_type == CP_SCENE_PUFFLE_HATS)
        {
            cp_enter_puffle_care();
            if (game.scene_type == CP_SCENE_PUFFLE_CARE)
                game.puffle_menu = 1;
        }
        else
            cp_leave_special("Returned to the room.");
        return;
    }

    if (game.scene_type == CP_SCENE_PUFFLE_SHOP)
    {
        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT ||
            action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
        {
            int page = game.puffle_adopt_page +
                ((action == CP_RIGHT_ACTION ||
                  action == CP_RIGHT_REPEAT) ? 1 : -1);

            if (page < 1)
                page = CP_PUFFLE_ADOPT_PAGES;
            else if (page > CP_PUFFLE_ADOPT_PAGES)
                page = 1;
            cp_load_puffle_adopt_page(page);
        }
        else if ((action == CP_UP_ACTION || action == CP_UP_REPEAT ||
                  action == CP_DOWN_ACTION ||
                  action == CP_DOWN_REPEAT) &&
                 game.puffle_adopt_count > 0)
        {
            int direction = (action == CP_DOWN_ACTION ||
                             action == CP_DOWN_REPEAT) ? 1 : -1;
            int next = game.puffle_adopt_index + direction;

            if (next < 0)
                next = game.puffle_adopt_count - 1;
            else if (next >= game.puffle_adopt_count)
                next = 0;
            if (cp_read_puffle_adopt_item(
                    game.puffle_adopt_page, next, &game.puffle_menu,
                    &game.puffle_adopt_cost))
            {
                game.puffle_adopt_index = next;
                cp_update_puffle_adopt_status();
            }
        }
        else if (action == CP_SELECT_ACTION)
        {
            if (game.puffle_adopt_count > 0)
                cp_puffle_adopt();
        }
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_PET_FURNITURE)
    {
        /* PLAY is also the iPod down button; reveal secrets on release. */
        if ((action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT) &&
            cp_play_held())
            return;
        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
            cp_change_pet_furniture_page(-1);
        else if (action == CP_RIGHT_ACTION ||
                 action == CP_RIGHT_REPEAT)
            cp_change_pet_furniture_page(1);
        else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            cp_change_pet_furniture_item(-1);
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            cp_change_pet_furniture_item(1);
        else if (action == CP_SELECT_ACTION)
            cp_purchase_exact_furniture();
        else if (action == CP_PLAY_ACTION)
            cp_toggle_pet_furniture_secret();
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_PUFFLE_CARE)
    {
        if ((action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT) &&
            cp_play_held())
            return;
        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
            cp_cycle_owned_puffle(-1);
        else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
            cp_cycle_owned_puffle(1);
        else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            game.puffle_menu = (game.puffle_menu + 5) % 6;
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            game.puffle_menu = (game.puffle_menu + 1) % 6;
        else if (action == CP_SELECT_ACTION)
            cp_puffle_care_action();
        else if (action == CP_PLAY_ACTION)
            cp_enter_puffle_hats();
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_PUFFLE_FOOD)
    {
        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
            cp_cycle_owned_puffle(-1);
        else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
            cp_cycle_owned_puffle(1);
        else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            cp_puffle_food_move(-1);
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            cp_puffle_food_move(1);
        else if (action == CP_SELECT_ACTION)
            cp_puffle_feed();
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_PUFFLE_TOYS)
    {
        if ((action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT) &&
            cp_play_held())
            return;
        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
            cp_cycle_owned_puffle(-1);
        else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
            cp_cycle_owned_puffle(1);
        else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            cp_puffle_toy_move(-1);
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            cp_puffle_toy_move(1);
        else if (action == CP_SELECT_ACTION)
            cp_puffle_play_toy();
        else if (action == CP_PLAY_ACTION)
            cp_enter_puffle_tricks();
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_PUFFLE_TRICKS)
    {
        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
            cp_cycle_owned_puffle(-1);
        else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
            cp_cycle_owned_puffle(1);
        else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            cp_puffle_trick_move(-1);
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            cp_puffle_trick_move(1);
        else if (action == CP_SELECT_ACTION)
            cp_puffle_perform_trick();
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_PUFFLE_HATS)
    {
        if ((action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT) &&
            cp_play_held())
            return;
        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
            cp_cycle_owned_puffle(-1);
        else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
            cp_cycle_owned_puffle(1);
        else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            cp_puffle_hat_move(-1);
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            cp_puffle_hat_move(1);
        else if (action == CP_SELECT_ACTION)
            cp_puffle_hat_purchase();
        else if (action == CP_PLAY_ACTION)
            cp_puffle_hat_remove();
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_IGLOO_CATALOG)
    {
        /* PLAY is also the iPod down button; buy/use only on release. */
        if ((action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT) &&
            cp_play_held())
            return;
        if (action == CP_PLAY_ACTION || action == CP_SELECT_ACTION)
            cp_buy_or_use_igloo_catalog_item();
        else if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
            cp_change_igloo_catalog_tab(-1);
        else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
            cp_change_igloo_catalog_tab(1);
        else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            cp_change_igloo_catalog_item(-1);
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            cp_change_igloo_catalog_item(1);
        game.dirty = true;
        return;
    }

    if (game.scene_type == CP_SCENE_FURNITURE_CATALOG ||
        game.scene_type == CP_SCENE_IGLOO_BOOK)
    {
        /* PLAY is also the iPod down button; switch views on release. */
        if ((action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT) &&
            cp_play_held())
            return;
        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
            cp_change_exact_catalog_page(-1);
        else if (action == CP_RIGHT_ACTION ||
                 action == CP_RIGHT_REPEAT)
            cp_change_exact_catalog_page(1);
        else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            cp_change_exact_catalog_item(-1);
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            cp_change_exact_catalog_item(1);
        else if (action == CP_SELECT_ACTION)
        {
            if (game.scene_type == CP_SCENE_FURNITURE_CATALOG)
                cp_purchase_exact_furniture();
            else
                cp_buy_or_use_exact_igloo_item();
        }
        else if (action == CP_PLAY_ACTION)
        {
            if (game.scene_type == CP_SCENE_FURNITURE_CATALOG)
                cp_enter_igloo_edit();
            else
                cp_enter_igloo_catalog();
        }
        game.dirty = true;
        return;
    }

    if (game.scene_type != CP_SCENE_IGLOO_EDIT)
        return;

    /* PLAY is also the normal iPod down button. Ignore its press-side down
     * action here and handle the release as the furniture buy/add action. */
    if ((action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT) &&
        cp_play_held())
        return;

    if (action == CP_PLAY_ACTION)
    {
        if (game.furniture_selected_slot < 0)
            cp_enter_furniture_catalog();
        else
            cp_buy_or_add_igloo_item();
    }
    else if (action == CP_SELECT_ACTION)
        cp_place_or_buy_igloo_item();
    else if (game.furniture_selected_slot < 0)
    {
        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
            cp_change_igloo_furniture_type(-1);
        else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
            cp_change_igloo_furniture_type(1);
        else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            cp_change_igloo_item(-1);
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            cp_change_igloo_item(1);
    }
    else
    {
        struct cp_furniture_placement *placement =
            &game.furniture_placements[game.furniture_selected_slot];

        if (action == CP_LEFT_ACTION || action == CP_LEFT_REPEAT)
            placement->x -= 4;
        else if (action == CP_RIGHT_ACTION || action == CP_RIGHT_REPEAT)
            placement->x += 4;
        else if (action == CP_UP_ACTION || action == CP_UP_REPEAT)
            placement->y -= 4;
        else if (action == CP_DOWN_ACTION || action == CP_DOWN_REPEAT)
            placement->y += 4;
        placement->x =
            MIN(MAX(placement->x, CP_FURNITURE_W / 2),
                CP_VIEW_W - CP_FURNITURE_W / 2);
        placement->y =
            MIN(MAX(placement->y, CP_FURNITURE_H / 2), CP_VIEW_H);
    }
    cp_write_save();
    game.dirty = true;
}

static void cp_puffle_tick(void)
{
    int type = game.puffle_type;

    if (game.puffle_owned_mask == 0 || !cp_puffle_is_owned(type) ||
        !(TIME_AFTER(*rb->current_tick, game.next_puffle_tick) ||
          *rb->current_tick == game.next_puffle_tick))
        return;

    if (game.scene_type == CP_SCENE_PUFFLE_TOYS &&
        game.puffle_toy_playing)
    {
        if (game.puffle_toy_ticks > 0)
            game.puffle_toy_ticks--;
        if (game.puffle_toy_ticks == 0)
        {
            game.puffle_toy_playing = false;
            game.puffle_toy_frame = 0;
            cp_update_puffle_toy_status();
        }
        else
        {
            game.puffle_toy_frame =
                (game.puffle_toy_frame + 1) % CP_PUFFLE_TOY_FRAMES;
        }
    }
    else if (game.scene_type == CP_SCENE_PUFFLE_TRICKS &&
        game.puffle_trick_playing)
    {
        if (game.puffle_trick_ticks > 0)
            game.puffle_trick_ticks--;
        if (game.puffle_trick_ticks == 0)
        {
            game.puffle_trick_playing = false;
            game.puffle_trick_frame = 0;
            cp_copy(game.shop_status, sizeof(game.shop_status),
                    "Select to perform");
        }
        else
        {
            game.puffle_trick_frame =
                (game.puffle_trick_frame + 1) % CP_PUFFLE_TRICK_FRAMES;
        }
    }
    else if (game.puffle_digging || game.puffle_eating)
    {
        if (game.puffle_action_ticks > 0)
        {
            game.puffle_action_ticks--;
            game.puffle_frame =
                (game.puffle_frame + 1) % CP_PUFFLE_FRAMES;
        }
        if (game.puffle_action_ticks == 0)
        {
            game.puffle_digging = false;
            game.puffle_eating = false;
            game.puffle_frame = 0;
            cp_load_puffle_asset(type);
        }
    }
    else if (game.puffle_action_ticks > 0)
        game.puffle_action_ticks--;
    else if (game.scene_type == CP_SCENE_ROOM && game.puffle_walking)
        game.puffle_frame = game.direction * 2 +
                            ((game.puffle_frame + 1) & 1);
    else
        game.puffle_frame = (game.puffle_frame + 1) % 4;
    if (TIME_AFTER(*rb->current_tick, game.next_puffle_need_tick) ||
        *rb->current_tick == game.next_puffle_need_tick)
    {
        game.puffle_food[type] = MAX(0, game.puffle_food[type] - 1);
        game.puffle_rest[type] = MAX(0, game.puffle_rest[type] - 1);
        game.puffle_happy[type] = MAX(0, game.puffle_happy[type] - 1);
        game.puffle_clean[type] = MAX(0, game.puffle_clean[type] - 1);
        game.next_puffle_need_tick = *rb->current_tick + 30 * HZ;
    }
    game.next_puffle_tick = *rb->current_tick + MAX(1, HZ / 6);
    if (game.scene_type == CP_SCENE_PUFFLE_CARE ||
        game.scene_type == CP_SCENE_PUFFLE_FOOD ||
        game.scene_type == CP_SCENE_PUFFLE_TOYS ||
        game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
        game.scene_type == CP_SCENE_PUFFLE_HATS ||
        (game.scene_type == CP_SCENE_ROOM && game.puffle_walking) ||
        (game.scene_type == CP_SCENE_ROOM && game.room_index >= 0 &&
         cp_streq(game.rooms[game.room_index].id, "player_home")))
        game.dirty = true;
}

static void cp_init(void)
{
    int i;

    rb->memset(&game, 0, sizeof(game));
    rb->memset(&sound, 0, sizeof(sound));
    sound.return_room = -1;
    sound.one_shot_id = -1;
    sound.record_fd = -1;
    sound.playback_fd = -1;
    game.coins = 500;
    game.direction = CP_DIR_DOWN;
    game.wheel_touch_zone = -1;
    game.special_return_room = -1;
    game.igloo_building = 1;
    game.igloo_floor = 0;
    game.igloo_location = 1;
    cp_own_igloo_item(0, 1);
    cp_own_igloo_item(2, 1);
    for (i = 0; i < CP_PUFFLE_TYPES; i++)
    {
        game.puffle_food[i] = 75;
        game.puffle_rest[i] = 75;
        game.puffle_happy[i] = 75;
        game.puffle_clean[i] = 75;
    }
    rb->srand(*rb->current_tick);
    game.next_puffle_tick = *rb->current_tick;
    game.next_puffle_need_tick = *rb->current_tick + 30 * HZ;
    game.furniture_selected_slot = -1;
    for (i = 0; i < CP_MAX_FURNITURE_PLACEMENTS; i++)
    {
        game.furniture_placements[i].x = CP_VIEW_W / 2;
        game.furniture_placements[i].y = 175;
    }
    game.furniture_placements[0].x = 90;
    game.furniture_placements[0].y = 180;
    game.furniture_placements[1].x = 235;
    game.furniture_placements[1].y = 155;
    game.furniture_placements[2].x = 160;
    game.furniture_placements[2].y = 185;
    game.furniture_placements[3].x = 270;
    game.furniture_placements[3].y = 145;
    if (!cp_load_rooms())
        cp_use_builtin_rooms();

    if (!cp_load_hotspots())
        cp_use_builtin_hotspots();

    cp_load_shop_data();
    for (i = 0; i < CP_SHOP_PAGES; i++)
        game.equipped[i] = -1;
    if (game.shop_item_count > 0)
    {
        int blue = cp_shop_first_item(CP_COLOR_PAGE) + 5;
        cp_shop_own_item(blue);
        game.equipped[CP_COLOR_PAGE] = 5;
    }

    if (!cp_load_assets() || !cp_load_furniture_catalog() ||
        !cp_load_igloo_catalogs())
    {
        game.assets_loaded = false;
        return;
    }
    game.puffle_food_count = cp_count_puffle_food_items();
    if (game.puffle_food_count != CP_PUFFLE_FOOD_ITEMS)
    {
        cp_copy(game.error, sizeof(game.error),
                "Puffle food data missing or incomplete.");
        game.assets_loaded = false;
        return;
    }
    if (cp_count_puffle_toy_items() != CP_PUFFLE_TOY_ITEMS)
    {
        cp_copy(game.error, sizeof(game.error),
                "Puffle toy data missing or incomplete.");
        game.assets_loaded = false;
        return;
    }

    game.map_x = game.map_hotspots[0].x;
    game.map_y = game.map_hotspots[0].y;
    cp_load_save();
    cp_own_igloo_item(0, 1);
    cp_own_igloo_item(2, 1);
    if (!cp_igloo_item_owned(0, game.igloo_building))
        game.igloo_building = 1;
    if (!cp_igloo_item_owned(1, game.igloo_floor))
        game.igloo_floor = 0;
    if (!cp_igloo_item_owned(2, game.igloo_location))
        game.igloo_location = 1;
    cp_migrate_furniture_inventory();
    if (game.puffle_owned_mask != 0 &&
        !cp_puffle_is_owned(game.puffle_type))
    {
        for (i = 0; i < CP_PUFFLE_TYPES; i++)
        {
            if (cp_puffle_is_owned(i))
            {
                game.puffle_type = i;
                break;
            }
        }
    }
    if (!cp_load_puffle_asset(game.puffle_type))
    {
        game.assets_loaded = false;
        return;
    }
    for (i = 0; i < CP_MAX_FURNITURE_PLACEMENTS; i++)
    {
        game.furniture_placements[i].x =
            MIN(MAX(game.furniture_placements[i].x, CP_FURNITURE_W / 2),
                CP_VIEW_W - CP_FURNITURE_W / 2);
        game.furniture_placements[i].y =
            MIN(MAX(game.furniture_placements[i].y,
                    CP_FURNITURE_H / 2), CP_VIEW_H);
    }
    for (i = 0; i < CP_SHOP_PAGES; i++)
    {
        int equipped = cp_shop_find_item(i, game.equipped[i]);
        if (game.equipped[i] >= 0 &&
            (equipped < 0 || !cp_shop_item_owned(equipped)))
            game.equipped[i] = -1;
    }
    if (game.equipped[CP_COLOR_PAGE] < 0 && game.shop_item_count > 0)
    {
        int blue = cp_shop_find_item(CP_COLOR_PAGE, 5);
        cp_shop_own_item(blue);
        game.equipped[CP_COLOR_PAGE] = 5;
    }
    if (game.map_x <= 0 || game.map_y <= 0)
    {
        game.map_x = game.map_hotspots[0].x;
        game.map_y = game.map_hotspots[0].y;
    }

    /* cp_load_assets() runs before the save is parsed. Rebuild once more so
     * the persisted paper-doll layers are visible immediately at startup. */
    if (!cp_load_player_asset())
    {
        game.assets_loaded = false;
        return;
    }

    if (!game.welcome_complete)
    {
        if (!cp_show_welcome_page(0))
        {
            cp_copy(game.error, sizeof(game.error),
                    "Welcome Solo assets are incomplete.");
            game.assets_loaded = false;
            return;
        }
    }
    else if (!cp_streq(game.saved_room, "map") &&
        cp_find_room(game.saved_room) >= 0)
        cp_enter_room(cp_find_room(game.saved_room), game.saved_x,
                      game.saved_y);
    else
        cp_enter_map_at(game.saved_x, game.saved_y);

    if (game.scene_type != CP_SCENE_WELCOME)
        cp_set_message("Select a marked area to enter it.");

}

static void cp_move_player(int dx, int dy)
{
    if (dx < 0)
        game.direction = CP_DIR_LEFT;
    else if (dx > 0)
        game.direction = CP_DIR_RIGHT;
    else if (dy < 0)
        game.direction = CP_DIR_UP;
    else if (dy > 0)
        game.direction = CP_DIR_DOWN;

    game.x += dx;
    game.y += dy;
    cp_clamp_player();
    cp_update_camera();
    game.anim_tick++;
    if (game.anim_tick >= CP_ANIM_RATE)
    {
        game.anim_tick = 0;
        game.anim_frame = (game.anim_frame + 1) % CP_PLAYER_ANIM_FRAMES;
    }
    game.selected_hotspot = cp_nearest_hotspot();
    game.dirty = true;
}

static void cp_queue_move(int dx, int dy)
{
    if (game.scene_type == CP_SCENE_MAP)
    {
        cp_select_map_direction(dx, dy);
        return;
    }

    if (game.scene_type != CP_SCENE_ROOM)
        return;

    if (dx < 0)
        game.direction = CP_DIR_LEFT;
    else if (dx > 0)
        game.direction = CP_DIR_RIGHT;
    else if (dy < 0)
        game.direction = CP_DIR_UP;
    else if (dy > 0)
        game.direction = CP_DIR_DOWN;

    game.target_x += dx;
    game.target_y += dy;
    cp_clamp_walk_target();
}

static void cp_check_room_trigger(void)
{
    int nearest = -1;
    int nearest_score = 0;
    int i;

    if (game.scene_type != CP_SCENE_ROOM)
        return;

    for (i = 0; i < game.hotspot_count; i++)
    {
        struct cp_hotspot *hotspot = &game.hotspots[i];
        int score;

        if (!cp_inside_room_door(i))
            continue;

        score = cp_dist_score(game.x, game.y, hotspot->x, hotspot->y);
        if (nearest < 0 || score < nearest_score)
        {
            nearest = i;
            nearest_score = score;
        }
    }

    if (nearest >= 0)
    {
        struct cp_hotspot *hotspot = &game.hotspots[nearest];

        if (game.room_trigger_armed)
        {
            int source_room_index = game.room_index;
            int room_index = cp_find_room(hotspot->target);
            int to_x = hotspot->to_x;
            int to_y = hotspot->to_y;

            game.room_trigger_armed = false;
            if (room_index < 0)
                cp_set_message("Room target missing from rooms.tsv.");
            else if (cp_enter_room(room_index, to_x, to_y))
                cp_place_at_matching_door(source_room_index);
        }
        return;
    }

    game.room_trigger_armed = true;
}

static bool cp_touch_walk_direction(int *dx, int *dy)
{
#ifdef HAVE_WHEEL_POSITION
    int wheel = rb->wheel_status();

    if (wheel >= 0)
    {
        game.wheel_touch_zone = ((wheel + 12) / 24) & 3;
        /* The hardware can report -1 between touch packets. Keep a short
         * latch so a resting finger produces continuous walking. */
        game.wheel_touch_until = *rb->current_tick + MAX(1, HZ / 8);
    }
    if (game.wheel_touch_zone < 0 ||
        TIME_AFTER(*rb->current_tick, game.wheel_touch_until))
    {
        game.wheel_touch_zone = -1;
        return false;
    }

    *dx = 0;
    *dy = 0;
    switch (game.wheel_touch_zone)
    {
        case 0: *dy = -CP_WALK_STEP; break;
        case 1: *dx = CP_WALK_STEP; break;
        case 2: *dy = CP_WALK_STEP; break;
        default: *dx = -CP_WALK_STEP; break;
    }
    return true;
#else
    (void)dx;
    (void)dy;
    return false;
#endif
}

static bool cp_touch_walk_active(void)
{
#ifdef HAVE_WHEEL_POSITION
    return game.wheel_touch_zone >= 0 &&
           !TIME_AFTER(*rb->current_tick, game.wheel_touch_until);
#else
    return false;
#endif
}

static void cp_walk_tick_if_due(void)
{
    int touch_dx = 0;
    int touch_dy = 0;
    int due_steps;
    int run_steps;
    int period = MAX(1, HZ / CP_WALK_RATE);
    int held_dx = 0;
    int held_dy = 0;
    int step;
    bool touch_moving;
    bool moved = false;
    long now = *rb->current_tick;

    if (game.scene_type != CP_SCENE_ROOM ||
        !(TIME_AFTER(now, game.next_walk_tick) ||
          now == game.next_walk_tick))
        return;

    due_steps = 1 + (int)(now - game.next_walk_tick) / period;
    run_steps = MIN(due_steps, CP_WALK_CATCHUP_MAX);
    game.next_walk_tick += due_steps * period;
    touch_moving = cp_touch_walk_direction(&touch_dx, &touch_dy);

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
    if (!touch_moving)
    {
        int held = rb->button_status();

        if (held & BUTTON_LEFT)
            held_dx--;
        if (held & BUTTON_RIGHT)
            held_dx++;
        if ((held & (BUTTON_MENU | BUTTON_SELECT)) !=
            (BUTTON_MENU | BUTTON_SELECT) && (held & BUTTON_MENU))
            held_dy--;
        if (held & BUTTON_PLAY)
            held_dy++;
    }
#endif

    for (step = 0; step < run_steps; step++)
    {
        int dx = 0;
        int dy = 0;

        if (touch_moving)
        {
            game.target_x += touch_dx;
            game.target_y += touch_dy;
        }
        else
        {
            game.target_x += held_dx * CP_WALK_STEP;
            game.target_y += held_dy * CP_WALK_STEP;
        }
        cp_clamp_walk_target();

        if (game.target_x < game.x)
            dx = -MIN(CP_WALK_STEP, game.x - game.target_x);
        else if (game.target_x > game.x)
            dx = MIN(CP_WALK_STEP, game.target_x - game.x);
        if (game.target_y < game.y)
            dy = -MIN(CP_WALK_STEP, game.y - game.target_y);
        else if (game.target_y > game.y)
            dy = MIN(CP_WALK_STEP, game.target_y - game.y);

        if (dx != 0 || dy != 0)
        {
            moved = true;
            cp_move_player(dx, dy);
            cp_check_room_trigger();
            if (game.scene_type != CP_SCENE_ROOM)
                break;
        }
    }

    if (!moved && game.anim_frame != 0)
    {
        game.anim_tick = 0;
        game.anim_frame = 0;
        game.dirty = true;
    }
}

static void cp_concert_tick(void)
{
    int frame;
    long now = *rb->current_tick;

    if (game.scene_type != CP_SCENE_ROOM || game.room_index < 0 ||
        (!cp_streq(game.rooms[game.room_index].id, "stage") &&
         !cp_streq(game.rooms[game.room_index].id, "emma_sewer")) ||
        !game.animation_loaded ||
        TIME_BEFORE(now, game.next_concert_tick))
        return;

    if (game.concert_intro_ticks > 0)
    {
        if (TIME_BEFORE(now, game.animation_start_tick))
        {
            game.next_concert_tick = now +
                MAX(1, HZ / CP_CONCERT_FRAME_RATE);
            return;
        }
        game.concert_intro_ticks = 0;
        game.concert_frame = 0;
        game.dirty = true;
    }
    else
    {
        frame = (int)(((uint64_t)(now - game.animation_start_tick) *
                       CP_CONCERT_FRAME_RATE / HZ) %
                      game.animation_frames);
        if (frame != game.concert_frame)
        {
            game.concert_frame = frame;
            game.dirty = true;
        }
    }
    game.next_concert_tick = now +
        MAX(1, HZ / CP_CONCERT_FRAME_RATE);
}

static void cp_night_city_tick(void)
{
    int frame;
    long now = *rb->current_tick;

    if (game.scene_type != CP_SCENE_ROOM || game.room_index < 0 ||
        !game.animation_loaded || TIME_BEFORE(now, game.next_city_tick) ||
        cp_night_city_strip(game.rooms[game.room_index].id) == NULL)
        return;

    frame = (int)(((uint64_t)(now - game.animation_start_tick) *
                   CP_NIGHT_CITY_FRAME_RATE / HZ) %
                  game.animation_frames);
    if (frame != game.city_frame)
    {
        game.city_frame = frame;
        game.dirty = true;
    }
    game.next_city_tick = now + MAX(1, HZ / CP_NIGHT_CITY_FRAME_RATE);
}

static void cp_interact(void)
{
    int nearest = cp_nearest_hotspot();
    struct cp_hotspot *hotspot;
    int room_index;

    game.selected_hotspot = nearest;
    if (game.scene_type != CP_SCENE_MAP && !cp_inside_hotspot(nearest))
    {
        cp_set_message("Waddle closer to a room marker.");
        return;
    }

    hotspot = &game.hotspots[nearest];
    if (!cp_has_text(hotspot->target))
    {
        if (game.scene_type == CP_SCENE_MAP && nearest >= 0 &&
            nearest < (int)(sizeof(game.visited_mask) * 8) &&
            (game.visited_mask & (1u << nearest)) == 0)
        {
            game.visited_mask |= 1u << nearest;
            game.coins += 25;
            rb->snprintf(game.message, sizeof(game.message),
                         "%s +25 coins", hotspot->detail);
            game.message_frames = CP_MESSAGE_TTL;
        }
        else
        {
            cp_set_message(hotspot->detail);
        }
        return;
    }

    if (hotspot->action == CP_ACTION_MESSAGE)
    {
        cp_set_message(hotspot->detail);
        return;
    }

    if (hotspot->action == CP_ACTION_MINIGAME)
    {
        if (cp_streq(hotspot->target, "cart_surfer"))
        {
            if (!cp_enter_cart_surfer())
                cp_set_message("Cart Surfer assets are incomplete.");
        }
        else if (cp_streq(hotspot->target, "sound_studio"))
        {
            if (!cp_enter_sound_studio())
                cp_set_message("Sound Studio assets are incomplete.");
        }
        else
            cp_set_message("Unknown minigame target.");
        return;
    }

    if (hotspot->action == CP_ACTION_SHOP)
    {
        if (cp_streq(hotspot->target, "sport_shop"))
        {
            if (!cp_enter_sport_shop())
                cp_set_message("Snow and Sports assets are incomplete.");
        }
        else if (cp_streq(hotspot->target, "costume_trunk"))
        {
            if (!cp_enter_costume_shop())
                cp_set_message("Costume Trunk assets are incomplete.");
        }
        else if (cp_streq(hotspot->target, "penguin_style"))
        {
            if (!cp_enter_penguin_style())
                cp_set_message("Penguin Style assets are incomplete.");
        }
        else if (cp_streq(hotspot->target, "ninja_catalog"))
        {
            if (!cp_enter_ninja_shop())
                cp_set_message("Martial Artworks assets are incomplete.");
        }
        else if (cp_streq(hotspot->target, "pet_furniture"))
            cp_enter_pet_furniture();
        else if (cp_streq(hotspot->target, "wardrobe"))
        {
            if (!cp_enter_shop())
                cp_set_message("Wardrobe assets are incomplete.");
        }
        else
            cp_set_message("Unknown shop target.");
        return;
    }

    if (hotspot->action == CP_ACTION_PUFFLE)
    {
        int type;

        if (cp_streq(hotspot->target, "adopt"))
            cp_enter_puffle_shop();
        else if (cp_streq(hotspot->target, "care"))
            cp_enter_puffle_care();
        else if (cp_streq(hotspot->target, "rainbow"))
            cp_unlock_special_puffle(CP_RAINBOW_PUFFLE);
        else if (cp_streq(hotspot->target, "gold"))
            cp_unlock_special_puffle(CP_GOLD_PUFFLE);
        else
        {
            for (type = 0; type < CP_PUFFLE_TYPES; type++)
            {
                char target[24];

                rb->snprintf(target, sizeof(target), "care_%s",
                             puffle_ids[type]);
                if (cp_streq(hotspot->target, target))
                {
                    game.puffle_type = type;
                    cp_enter_puffle_care();
                    return;
                }
            }
            cp_set_message("Unknown puffle interaction.");
        }
        return;
    }

    if (hotspot->action == CP_ACTION_IGLOO)
    {
        if (cp_streq(hotspot->target, "upgrade"))
            cp_enter_igloo_book();
        else
            cp_enter_igloo_edit();
        return;
    }

    if (hotspot->action == CP_ACTION_MAP ||
        cp_streq(hotspot->target, "map"))
    {
        if (cp_enter_map_at(hotspot->to_x, hotspot->to_y))
            cp_write_save();
        return;
    }

    room_index = cp_find_room(hotspot->target);
    if (room_index < 0)
    {
        cp_set_message("Room target missing from rooms.tsv.");
        return;
    }

    if (game.scene_type == CP_SCENE_MAP)
    {
        game.map_x = game.x;
        game.map_y = game.y;
    }

    if (!cp_enter_room(room_index, hotspot->to_x, hotspot->to_y))
        return;

    cp_set_message(hotspot->detail);
    cp_write_save();
}

static void cp_go_back(void)
{
    if (game.scene_type == CP_SCENE_WELCOME)
    {
        cp_advance_welcome();
        return;
    }

    if (game.scene_type == CP_SCENE_SHOP)
    {
        cp_leave_shop();
        return;
    }

    if (game.scene_type == CP_SCENE_SPORT_SHOP ||
        game.scene_type == CP_SCENE_COSTUME_SHOP ||
        game.scene_type == CP_SCENE_PENGUIN_STYLE ||
        game.scene_type == CP_SCENE_NINJA_SHOP)
    {
        cp_leave_sport_shop();
        return;
    }

    if (game.scene_type == CP_SCENE_PUFFLE_FOOD ||
        game.scene_type == CP_SCENE_PUFFLE_TOYS ||
        game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
        game.scene_type == CP_SCENE_PUFFLE_HATS)
    {
        cp_enter_puffle_care();
        return;
    }

    if (game.scene_type == CP_SCENE_PUFFLE_SHOP ||
        game.scene_type == CP_SCENE_PET_FURNITURE ||
        game.scene_type == CP_SCENE_PUFFLE_CARE ||
        game.scene_type == CP_SCENE_IGLOO_EDIT ||
        game.scene_type == CP_SCENE_IGLOO_BOOK)
    {
        cp_leave_special("Returned to the room.");
        return;
    }

    if (game.scene_type == CP_SCENE_FURNITURE_CATALOG)
    {
        cp_enter_igloo_edit();
        return;
    }

    if (game.scene_type == CP_SCENE_IGLOO_CATALOG)
    {
        cp_enter_igloo_book();
        return;
    }

    if (game.scene_type == CP_SCENE_SOUND_SAVE)
    {
        cp_sound_delete_track(sound.saved_slot);
        if (!cp_sound_show_board())
            cp_sound_show_title();
        return;
    }

    if (game.scene_type == CP_SCENE_SOUND_BOARD ||
        game.scene_type == CP_SCENE_SOUND_INSTRUCTIONS ||
        game.scene_type == CP_SCENE_SOUND_SAVED)
    {
        if (!cp_sound_show_title())
            cp_sound_leave();
        return;
    }

    if (game.scene_type == CP_SCENE_SOUND_TITLE)
    {
        cp_sound_leave();
        return;
    }

    if (game.scene_type >= CP_SCENE_CART_TITLE &&
        game.scene_type <= CP_SCENE_CART_RESULTS)
    {
        cp_leave_cart_surfer("Returned to the Mine.");
        return;
    }

    if (game.scene_type == CP_SCENE_ROOM)
    {
        cp_enter_map_at(game.map_x, game.map_y);
        cp_set_message("Returned to the island map.");
        cp_write_save();
        return;
    }
}

static bool cp_map_chord_held(void)
{
#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
    return (rb->button_status() & (BUTTON_LEFT | BUTTON_RIGHT)) ==
           (BUTTON_LEFT | BUTTON_RIGHT);
#else
    return false;
#endif
}

static void cp_open_map(void)
{
    if (game.scene_type == CP_SCENE_MAP)
        return;

    if (game.scene_type == CP_SCENE_WELCOME)
    {
        if (game.welcome_page >= 2)
            cp_advance_welcome();
        return;
    }

    if (cp_sound_scene())
        cp_sound_close_audio();

    if (game.scene_type != CP_SCENE_ROOM && !cp_load_player_asset())
        return;

    if (cp_enter_map_at(game.map_x, game.map_y))
    {
        cp_set_message("Returned to the island map.");
        cp_write_save();
    }
}

static void cp_quit(enum plugin_status *status, bool *running)
{
    cp_sound_close_audio();
    cp_write_save();
    *running = false;
    *status = PLUGIN_OK;
}

static bool cp_quit_chord_held(void)
{
#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
    return (rb->button_status() & (BUTTON_MENU | BUTTON_SELECT)) ==
           (BUTTON_MENU | BUTTON_SELECT);
#else
    return false;
#endif
}

static void cp_draw_status(void)
{
    char text[28];
    int width;
    int height;
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();

    rb->lcd_bitmap((const fb_data *)game.toolbar.data, 0, CP_VIEW_H,
                   CP_VIEW_W, CP_STATUS_H);

    if (game.message_frames > 0)
        cp_copy(text, sizeof(text), game.message);
    else if (game.selected_hotspot >= 0 &&
             game.selected_hotspot < game.hotspot_count &&
             cp_inside_hotspot(game.selected_hotspot))
        cp_copy(text, sizeof(text),
                game.hotspots[game.selected_hotspot].name);
    else
        cp_copy(text, sizeof(text), game.location);

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    while (text[0] != '\0')
    {
        rb->lcd_getstringsize(text, &width, &height);
        if (width <= 150)
            break;
        text[rb->strlen(text) - 1] = '\0';
    }
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_putsxy(75 + MAX(0, (150 - width) / 2),
                   CP_VIEW_H + MAX(1, (CP_STATUS_H - height) / 2), text);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
}

static void cp_draw_player_frame(int frame, int x, int y);

static void cp_draw_room_markers(void)
{
    int i;
    int old_fg = rb->lcd_get_foreground();

    if (game.scene_type == CP_SCENE_MAP)
    {
        if (game.selected_hotspot >= 0 &&
            game.selected_hotspot < game.hotspot_count)
        {
            int x = game.hotspots[game.selected_hotspot].x - game.cam_x;
            int y = game.hotspots[game.selected_hotspot].y - game.cam_y;

            cp_draw_player_frame(CP_PLAYER_SELECTOR_FRAME,
                                 x - CP_PLAYER_W / 2,
                                 y - CP_PLAYER_H + 4);
        }
        return;
    }

    for (i = 0; i < game.hotspot_count; i++)
    {
        int x = game.hotspots[i].x - game.cam_x;
        int y = game.hotspots[i].y - game.cam_y;

        if (game.scene_type == CP_SCENE_ROOM)
            continue;

        if (x < -5 || y < -5 || x >= CP_VIEW_W + 5 || y >= CP_VIEW_H + 5)
            continue;

        rb->lcd_set_foreground(i == game.selected_hotspot ?
                               LCD_RGBPACK(255, 215, 0) : LCD_WHITE);
        rb->lcd_drawrect(x - 3, y - 3, 7, 7);
        rb->lcd_drawline(x - 5, y, x + 5, y);
        rb->lcd_drawline(x, y - 5, x, y + 5);
    }

    rb->lcd_set_foreground(old_fg);
}

static void cp_draw_player_frame(int frame, int x, int y)
{
    int src_x = frame * CP_PLAYER_W;
    int src_y = 0;
    int width = CP_PLAYER_W;
    int height = CP_PLAYER_H;

    if (x < 0)
    {
        src_x -= x;
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        src_y -= y;
        height += y;
        y = 0;
    }
    if (x + width > CP_VIEW_W)
        width = CP_VIEW_W - x;
    if (y + height > CP_VIEW_H)
        height = CP_VIEW_H - y;

    if (width > 0 && height > 0)
        rb->lcd_bitmap_transparent_part((const fb_data *)game.player.data,
                                        src_x, src_y, game.player.width,
                                        x, y, width, height);
}

static void cp_draw_player(int x, int y)
{
    int frame = game.direction * CP_PLAYER_ANIM_FRAMES + game.anim_frame;

    cp_draw_player_frame(frame, x, y);
}

static void cp_draw_transparent_frame(const struct bitmap *bitmap, int frame,
                                      int frame_width, int frame_height,
                                      int x, int y)
{
    int source_x = frame * frame_width;
    int source_y = 0;
    int width = frame_width;
    int height = frame_height;

    if (x < 0)
    {
        source_x -= x;
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        source_y -= y;
        height += y;
        y = 0;
    }
    if (x + width > CP_VIEW_W)
        width = CP_VIEW_W - x;
    if (y + height > CP_VIEW_H)
        height = CP_VIEW_H - y;
    if (width > 0 && height > 0)
        rb->lcd_bitmap_transparent_part((const fb_data *)bitmap->data,
                                        source_x, source_y, bitmap->width,
                                        x, y, width, height);
}

static bool cp_in_player_home(void)
{
    return game.room_index >= 0 && game.room_index < game.room_count &&
           cp_streq(game.rooms[game.room_index].id, "player_home");
}

static bool cp_in_backyard(void)
{
    return game.room_index >= 0 && game.room_index < game.room_count &&
           cp_streq(game.rooms[game.room_index].id, "backyard");
}

static void cp_draw_igloo_furniture(bool editing)
{
    struct cp_furniture_placement *placement;
    int x;
    int y;
    int old_fg;

    if (!editing)
        return;
    if (game.furniture_selected_slot < 0)
    {
        x = CP_VIEW_W / 2 - CP_FURNITURE_W / 2;
        y = game.furniture_item.type == 2 ? 42 : 112;
    }
    else
    {
        placement =
            &game.furniture_placements[game.furniture_selected_slot];
        x = placement->x - CP_FURNITURE_W / 2;
        y = placement->y - CP_FURNITURE_H;
    }
    cp_draw_transparent_frame(&game.furniture, 0, CP_FURNITURE_W,
                              CP_FURNITURE_H, x, y);
    old_fg = rb->lcd_get_foreground();
    rb->lcd_set_foreground(LCD_RGBPACK(255, 220, 32));
    rb->lcd_drawrect(x, y, CP_FURNITURE_W, CP_FURNITURE_H);
    rb->lcd_set_foreground(old_fg);
}

static void cp_draw_puffle(int x, int y)
{
    cp_draw_transparent_frame(&game.puffle, game.puffle_frame,
                              CP_PUFFLE_W, CP_PUFFLE_H,
                              x - CP_PUFFLE_W / 2, y - CP_PUFFLE_H);
}

static void cp_draw_puffle_trick(int x, int y)
{
    cp_draw_transparent_frame(&game.puffle_trick,
                              game.puffle_trick_frame,
                              CP_PUFFLE_TRICK_W, CP_PUFFLE_TRICK_H,
                              x - CP_PUFFLE_TRICK_W / 2,
                              y - CP_PUFFLE_TRICK_H);
}

static void cp_draw_puffle_toy(int x, int y)
{
    cp_draw_transparent_frame(&game.puffle_toy,
                              game.puffle_toy_frame,
                              CP_PUFFLE_TOY_W, CP_PUFFLE_TOY_H,
                              x - CP_PUFFLE_TOY_W / 2,
                              y - CP_PUFFLE_TOY_H);
}

static void cp_draw_puffle_hat(int x, int y)
{
    cp_draw_transparent_frame(&game.puffle_hat, 1,
                              CP_PUFFLE_HAT_W, CP_PUFFLE_HAT_H,
                              x - CP_PUFFLE_HAT_W / 2,
                              y - CP_PUFFLE_HAT_H - 10);
}

static void cp_draw_cart_sprite(void)
{
    int frame = game.cart.sprite_frame;
    int x = 140 + game.cart.lean * 8;
    int y = 157;

    if (frame < 0 || frame >= CP_CART_FRAMES)
        frame = 0;
    if (game.cart.jump_ticks > 0)
    {
        int progress = CP_CART_JUMP_TICKS - game.cart.jump_ticks;
        int height = progress <= CP_CART_JUMP_TICKS / 2 ?
                     progress * 4 : (CP_CART_JUMP_TICKS - progress) * 4;
        y -= height;
    }

    rb->lcd_bitmap_transparent_part((const fb_data *)game.player.data,
                                    frame * CP_CART_FRAME_W, 0,
                                    game.player.width, x, y,
                                    CP_CART_FRAME_W, CP_CART_FRAME_H);
}

static void cp_draw_cart_track(void)
{
    int frame = ((game.cart.segment_tick *
                 (game.cart.speed_stage + 1)) / 3) % CP_CART_TRACK_FRAMES;

    rb->lcd_bitmap_transparent_part((const fb_data *)game.player.data,
                                    frame * CP_CART_TRACK_W,
                                    CP_CART_TRACK_Y, game.player.width,
                                    120, 125, CP_CART_TRACK_W,
                                    CP_CART_TRACK_H);
}

static void cp_render_cart(void)
{
    char line[96];
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();

    rb->lcd_bitmap((const fb_data *)game.scene.data, 0, 0,
                   CP_VIEW_W, CP_VIEW_H);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);

    if (game.scene_type == CP_SCENE_CART_COUNTDOWN ||
        game.scene_type == CP_SCENE_CART_PLAYING ||
        game.scene_type == CP_SCENE_CART_PAUSED ||
        game.scene_type == CP_SCENE_CART_CRASH)
    {
        int type = cp_cart_segments[game.cart.segment];
        const char *prompt = "STRAIGHT";

        if (type == 2)
            prompt = "TURN RIGHT";
        else if (type == 3)
            prompt = "TURN LEFT";
        else if (type == 4)
            prompt = "JUMP";
        else if (type == 5)
            prompt = "TRICK BONUS";

        cp_draw_cart_track();
        cp_draw_cart_sprite();
        rb->lcd_fillrect(0, 0, LCD_WIDTH, 13);
        rb->snprintf(line, sizeof(line), "Score %d  Carts %d  x%d  %s",
                     game.cart.score, game.cart.lives,
                     MAX(1, game.cart.combo), prompt);
        rb->lcd_putsxy(2, 2, line);
        if (game.scene_type == CP_SCENE_CART_COUNTDOWN)
        {
            int count = (game.cart.countdown_ticks + 24) / 25;
            rb->lcd_fillrect(130, 78, 60, 38);
            rb->snprintf(line, sizeof(line), "%d", MAX(1, count));
            rb->lcd_putsxy(156, 91, line);
        }
        else if (game.scene_type == CP_SCENE_CART_PAUSED)
        {
            rb->lcd_fillrect(75, 76, 170, 60);
            rb->lcd_putsxy(137, 87, "PAUSED");
            rb->lcd_putsxy(91, 106, "Select resume");
            rb->lcd_putsxy(91, 121, "Menu abandon");
        }
        else if (game.scene_type == CP_SCENE_CART_CRASH)
        {
            rb->lcd_fillrect(95, 92, 130, 30);
            rb->lcd_putsxy(132, 101, "CRASH!");
        }
    }
    else if (game.scene_type == CP_SCENE_CART_RESULTS)
    {
        rb->lcd_fillrect(55, 58, 210, 105);
        rb->lcd_putsxy(119, 68, "RUN COMPLETE");
        rb->snprintf(line, sizeof(line), "Score: %d", game.cart.score);
        rb->lcd_putsxy(108, 91, line);
        rb->snprintf(line, sizeof(line), "Coins: +%d", game.cart.reward);
        rb->lcd_putsxy(108, 108, line);
        rb->snprintf(line, sizeof(line), "Best: %d", game.cart_best_score);
        rb->lcd_putsxy(108, 125, line);
        rb->snprintf(line, sizeof(line), "Best combo: %d",
                     game.cart_best_combo);
        rb->lcd_putsxy(108, 142, line);
        rb->lcd_putsxy(88, 159,
                       game.cart.results_choice == 0 ?
                       "> Retry    Return" : "  Retry  > Return");
    }

    rb->lcd_bitmap((const fb_data *)game.toolbar.data, 0, CP_VIEW_H,
                   CP_VIEW_W, CP_STATUS_H);

    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
    rb->lcd_update();
    game.dirty = false;
}

static void cp_render_shop(void)
{
    static const char *departments[CP_SHOP_PAGES] =
    {
        "HEAD ITEMS", "BODY ITEMS", "FEET ITEMS", "COLORS", "FACE ITEMS"
    };
    struct cp_shop_item *item = &game.shop_items[game.shop_selection];
    char line[48];
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();
    int col = item->slot % 3;
    int row = item->slot / 3;
    int x = 46 + col * 44;
    int y = game.shop_page == 3 ? 34 + row * 36 : 34 + row * 48;
    bool equipped = game.equipped[item->page] == item->slot;

    rb->lcd_bitmap((const fb_data *)game.scene.data, 0, 0,
                   CP_VIEW_W, CP_VIEW_H);
    rb->lcd_set_foreground(LCD_RGBPACK(255, 220, 32));
    rb->lcd_drawrect(x - 26, y - 19, 52, 39);
    rb->lcd_drawrect(x - 27, y - 20, 54, 41);

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_RGBPACK(8, 117, 185));
    rb->lcd_putsxy(186, 8, departments[game.shop_page]);
    rb->snprintf(line, sizeof(line), "Coins: %d", game.coins);
    rb->lcd_putsxy(186, 28, line);
    rb->lcd_putsxy(186, 54, item->name);
    rb->snprintf(line, sizeof(line), "Cost: %d", item->cost);
    rb->lcd_putsxy(186, 72, line);
    rb->lcd_putsxy(186, 92, equipped ? "EQUIPPED" : game.shop_status);
    cp_draw_player_frame(CP_DIR_DOWN * CP_PLAYER_ANIM_FRAMES,
                         250, 108);
    rb->lcd_putsxy(186, 130, "Wheel: item");
    rb->lcd_putsxy(186, 146, "Left/Right: page");
    rb->lcd_putsxy(186, 162, "Select: buy/equip");
    rb->lcd_putsxy(186, 178, "Play: remove  Menu:back");

    cp_draw_status();
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
    rb->lcd_update();
    game.dirty = false;
}

static void cp_render_sport_shop(void)
{
    char line[96];
    bool equipped = false;
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();

    if (game.sport_count > 0)
    {
        equipped = cp_catalog_item_equipped(&game.sport_item);
    }
    rb->lcd_bitmap((const fb_data *)game.scene.data, 0, 0,
                   CP_VIEW_W, CP_VIEW_H);
    cp_draw_status();
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    if (game.message_frames > 0)
        rb->lcd_putsxy(3, CP_VIEW_H + 5, game.message);
    else if (game.sport_count > 0)
    {
        rb->snprintf(line, sizeof(line),
                     "%s P%d %d/%d %.18s %dc %s C:%d",
                     cp_catalog_short_name(), game.sport_page,
                     game.sport_index + 1, game.sport_count,
                     game.sport_item.name, game.sport_item.cost,
                     equipped ? "ON" : game.shop_status, game.coins);
        rb->lcd_putsxy(3, CP_VIEW_H + 5, line);
    }
    else
    {
        rb->snprintf(line, sizeof(line), "%s P%d/%d %s C:%d",
                     cp_catalog_short_name(), game.sport_page,
                     cp_catalog_pages(), game.shop_status, game.coins);
        rb->lcd_putsxy(3, CP_VIEW_H + 5, line);
    }
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
    rb->lcd_update();
    game.dirty = false;
}

static void cp_render_puffle_menu(void)
{
    static const char *care_items[] =
    {
        "Food", "Play", "Sleep", "Bath", "Walk with me",
        "Dig for treasure"
    };
    char line[96];
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();
    int i;

    if (game.scene_type == CP_SCENE_PUFFLE_SHOP)
    {
        rb->lcd_bitmap((const fb_data *)game.scene.data, 0, 0,
                       CP_VIEW_W, CP_VIEW_H);
        cp_draw_status();
        rb->lcd_setfont(FONT_SYSFIXED);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_set_background(LCD_BLACK);
        if (game.message_frames > 0)
            rb->lcd_putsxy(3, CP_VIEW_H + 5, game.message);
        else if (game.puffle_adopt_count > 0)
        {
            rb->snprintf(line, sizeof(line),
                         "ADOPT P%d %d/%d %s %dc %s C:%d",
                         game.puffle_adopt_page,
                         game.puffle_adopt_index + 1,
                         game.puffle_adopt_count,
                         puffle_names[game.puffle_menu],
                         game.puffle_adopt_cost, game.shop_status,
                         game.coins);
            rb->lcd_putsxy(3, CP_VIEW_H + 5, line);
        }
        else
        {
            rb->snprintf(line, sizeof(line), "ADOPT P%d/%d %s C:%d",
                         game.puffle_adopt_page, CP_PUFFLE_ADOPT_PAGES,
                         game.shop_status, game.coins);
            rb->lcd_putsxy(3, CP_VIEW_H + 5, line);
        }
        rb->lcd_set_foreground(old_fg);
        rb->lcd_set_background(old_bg);
        rb->lcd_update();
        game.dirty = false;
        return;
    }

    rb->lcd_bitmap((const fb_data *)game.scene.data, 0, 0,
                   CP_VIEW_W, CP_VIEW_H);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_RGBPACK(8, 117, 185));
    rb->lcd_fillrect(174, 0, 146, CP_VIEW_H);
    if (game.scene_type == CP_SCENE_PUFFLE_TOYS)
        cp_draw_puffle_toy(87, 158);
    else if (game.scene_type == CP_SCENE_PUFFLE_TRICKS)
        cp_draw_puffle_trick(87, 158);
    else
        cp_draw_puffle(87, 158);
    if ((game.scene_type == CP_SCENE_PUFFLE_CARE ||
         game.scene_type == CP_SCENE_PUFFLE_HATS) &&
        game.puffle_hat_loaded)
        cp_draw_puffle_hat(87, 158);

    if (game.scene_type == CP_SCENE_PUFFLE_FOOD)
    {
        struct cp_puffle_food_item *item = &game.puffle_food_item;
        int quantity = game.puffle_food_quantity[game.puffle_food_index];

        cp_draw_transparent_frame(&game.food_icon, 0, CP_PUFFLE_W,
                                  CP_PUFFLE_H, 126, 106);
        rb->lcd_putsxy(184, 8, "PUFFLE FOOD");
        rb->snprintf(line, sizeof(line), "%d/%d %.18s",
                     game.puffle_food_index + 1, game.puffle_food_count,
                     item->name);
        rb->lcd_putsxy(184, 28, line);
        if (item->cost >= CP_PUFFLE_RARE_COST)
            rb->snprintf(line, sizeof(line), "Dig inventory: %d", quantity);
        else if (item->cost == 0)
            cp_copy(line, sizeof(line), "Free staple food");
        else
            rb->snprintf(line, sizeof(line), "Cost: %d  Coins:%d",
                         item->cost, game.coins);
        rb->lcd_putsxy(184, 48, line);
        rb->snprintf(line, sizeof(line), "Food %+d Rest %+d",
                     item->food, item->rest);
        rb->lcd_putsxy(184, 72, line);
        rb->snprintf(line, sizeof(line), "Happy %+d Clean %+d",
                     item->happy, item->clean);
        rb->lcd_putsxy(184, 90, line);
        rb->lcd_putsxy(184, 118, game.shop_status);
        rb->lcd_putsxy(184, 150, "Wheel: food");
        rb->lcd_putsxy(184, 168, "Left/Right: puffle");
    }
    else if (game.scene_type == CP_SCENE_PUFFLE_TOYS)
    {
        struct cp_puffle_toy_item *item = &game.puffle_toy_item;
        int type = game.puffle_type;
        bool owned = game.puffle_toy_index == 0 ||
            (game.puffle_toy_owned & (1u << type)) != 0;

        cp_draw_transparent_frame(&game.toy_icon, 0,
                                  CP_PUFFLE_HAT_W, CP_PUFFLE_HAT_H,
                                  126, 106);
        rb->snprintf(line, sizeof(line), "%s PUFFLE TOYS",
                     puffle_names[type]);
        rb->lcd_putsxy(184, 8, line);
        rb->snprintf(line, sizeof(line), "%d/2 %.16s",
                     game.puffle_toy_index + 1, item->name);
        rb->lcd_putsxy(184, 28, line);
        if (game.puffle_toy_index == 0)
            cp_copy(line, sizeof(line), "Normal toy: free");
        else if (owned)
            cp_copy(line, sizeof(line), "Super toy: OWNED");
        else
            rb->snprintf(line, sizeof(line), "Cost:%d Coins:%d",
                         item->cost, game.coins);
        rb->lcd_putsxy(184, 48, line);
        rb->snprintf(line, sizeof(line), "Food %+d Rest %+d",
                     item->food, item->rest);
        rb->lcd_putsxy(184, 70, line);
        rb->snprintf(line, sizeof(line), "Happy %+d Clean %+d",
                     item->happy, item->clean);
        rb->lcd_putsxy(184, 88, line);
        rb->lcd_putsxy(184, 116, game.shop_status);
        rb->lcd_putsxy(184, 150, "Wheel: toy");
        rb->lcd_putsxy(184, 168, "L/R:puffle Play:tricks");
    }
    else if (game.scene_type == CP_SCENE_PUFFLE_TRICKS)
    {
        int type = game.puffle_type;

        rb->snprintf(line, sizeof(line), "%s PUFFLE TRICKS",
                     puffle_names[type]);
        rb->lcd_putsxy(184, 8, line);
        rb->snprintf(line, sizeof(line), "Happy %d",
                     game.puffle_happy[type]);
        rb->lcd_putsxy(184, 26, line);
        for (i = 0; i < CP_PUFFLE_TRICKS; i++)
        {
            int item_y = 46 + i * 18;

            if (i == game.puffle_trick_index)
                rb->lcd_drawrect(180, item_y - 2, 138, 17);
            rb->lcd_putsxy(184, item_y, puffle_trick_names[i]);
        }
        rb->lcd_putsxy(184, 158, game.shop_status);
        rb->lcd_putsxy(184, 176, "Left/Right: puffle");
    }
    else if (game.scene_type == CP_SCENE_PUFFLE_HATS)
    {
        struct cp_puffle_hat_item *item = &game.puffle_hat_item;

        cp_draw_transparent_frame(&game.puffle_hat, 0,
                                  CP_PUFFLE_HAT_W, CP_PUFFLE_HAT_H,
                                  126, 106);
        rb->snprintf(line, sizeof(line), "%s PUFFLE HATS",
                     puffle_names[game.puffle_type]);
        rb->lcd_putsxy(184, 8, line);
        rb->snprintf(line, sizeof(line), "%d/%d %.16s",
                     game.puffle_hat_index + 1, game.puffle_hat_count,
                     item->name);
        rb->lcd_putsxy(184, 28, line);
        rb->snprintf(line, sizeof(line), "Cost:%d Coins:%d",
                     item->cost, game.coins);
        rb->lcd_putsxy(184, 48, line);
        rb->lcd_putsxy(184, 70, game.shop_status);
        rb->lcd_putsxy(184, 144, "Wheel: hat");
        rb->lcd_putsxy(184, 160, "L/R: puffle");
        rb->lcd_putsxy(184, 176, "Select: buy/equip");
        rb->lcd_putsxy(184, 192, "Play:remove Menu:back");
    }
    else
    {
        int type = game.puffle_type;

        rb->snprintf(line, sizeof(line), "%s PUFFLE CARE",
                     puffle_names[type]);
        rb->lcd_putsxy(184, 8, line);
        rb->snprintf(line, sizeof(line), "Food %d",
                     game.puffle_food[type]);
        rb->lcd_putsxy(184, 28, line);
        rb->snprintf(line, sizeof(line), "Rest %d",
                     game.puffle_rest[type]);
        rb->lcd_putsxy(184, 44, line);
        rb->snprintf(line, sizeof(line), "Happy %d",
                     game.puffle_happy[type]);
        rb->lcd_putsxy(184, 60, line);
        rb->snprintf(line, sizeof(line), "Clean %d",
                     game.puffle_clean[type]);
        rb->lcd_putsxy(254, 60, line);
        for (i = 0; i < 6; i++)
        {
            const char *label = care_items[i];
            int icon_y = 78 + i * 16;

            if (i == 4 && game.puffle_walking)
                label = "Stop walking";
            cp_draw_transparent_frame(&game.care_icons, i,
                                      CP_PUFFLE_CARE_ICON_W,
                                      CP_PUFFLE_CARE_ICON_H, 182, icon_y);
            if (i == game.puffle_menu)
                rb->lcd_drawrect(180, icon_y - 1, 20, 18);
            rb->lcd_putsxy(204, icon_y, label);
        }
        rb->lcd_putsxy(184, 174, game.shop_status);
        rb->lcd_putsxy(184, 190, "L/R:puffle Play:hats");
    }
    if (game.scene_type != CP_SCENE_PUFFLE_CARE &&
        game.scene_type != CP_SCENE_PUFFLE_HATS)
        rb->lcd_putsxy(184, 192, "Menu: back");
    cp_draw_status();
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
    rb->lcd_update();
    game.dirty = false;
}

static void cp_render_igloo_edit(void)
{
    static const char *types[] =
    {
        "", "ROOM", "WALL", "FLOOR", "PUFFLE"
    };
    char line[64];
    int owned = cp_furniture_quantity(game.furniture_item.id);
    int placed = cp_furniture_placed_count(game.furniture_item.id);
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();

    rb->lcd_bitmap((const fb_data *)game.scene.data, 0, 0,
                   CP_VIEW_W, CP_VIEW_H);
    cp_draw_igloo_furniture(true);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_RGBPACK(8, 117, 185));
    rb->lcd_fillrect(0, 0, CP_VIEW_W, 32);
    rb->snprintf(line, sizeof(line), "%s %d/%d %.16s O%d/P%d %dc",
                 types[game.furniture_catalog_type],
                 game.furniture_catalog_index + 1,
                 game.furniture_catalog_count, game.furniture_item.name,
                 owned, placed, game.furniture_item.cost);
    rb->lcd_putsxy(6, 3, line);
    rb->lcd_putsxy(6, 17, game.furniture_selected_slot >= 0 ?
                   "Wheel:move Select:store Play:buy/add" :
                   "L/R:type Wheel:item Select:place/buy Play:catalog");
    cp_draw_status();
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
    rb->lcd_update();
    game.dirty = false;
}

static void cp_render_igloo_catalog(void)
{
    static const char *tabs[CP_IGLOO_TABS] =
    {
        "BUILDING", "FLOOR", "LOCATION"
    };
    char line[72];
    bool owned = cp_igloo_item_owned(game.igloo_catalog_tab,
                                     game.igloo_item.id);
    bool active = cp_current_igloo_item(game.igloo_catalog_tab) ==
                  game.igloo_item.id;
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();
    rb->lcd_bitmap((const fb_data *)game.scene.data, 0, 0,
                   CP_VIEW_W, CP_VIEW_H);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_RGBPACK(8, 117, 185));
    rb->lcd_fillrect(0, 0, CP_VIEW_W, 48);
    rb->snprintf(line, sizeof(line), "%s %d/%d  Coins:%d",
                 tabs[game.igloo_catalog_tab],
                 game.igloo_catalog_index + 1,
                 game.igloo_catalog_count[game.igloo_catalog_tab],
                 game.coins);
    rb->lcd_putsxy(6, 3, line);
    rb->snprintf(line, sizeof(line), "%.34s  %dc  %s",
                 game.igloo_item.name, game.igloo_item.cost,
                 active ? "ACTIVE" : (owned ? "OWNED" : "BUY"));
    rb->lcd_putsxy(6, 18, line);
    rb->lcd_putsxy(6, 33,
                   "Left/Right:tab Wheel:item Select/Play:buy/use");
    cp_draw_status();
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
    rb->lcd_update();
    game.dirty = false;
}

static void cp_render_exact_catalog(void)
{
    char line[96];
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();

    rb->lcd_bitmap((const fb_data *)game.scene.data, 0, 0,
                   CP_VIEW_W, CP_VIEW_H);
    cp_draw_status();
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    if (game.message_frames > 0)
        rb->lcd_putsxy(3, CP_VIEW_H + 5, game.message);
    else if (game.exact_catalog_count <= 0)
    {
        int pages = game.scene_type == CP_SCENE_PET_FURNITURE ?
                    CP_PET_FURNITURE_PAGES :
                    (game.scene_type == CP_SCENE_FURNITURE_CATALOG ?
                     CP_FURNITURE_CATALOG_PAGES : CP_IGLOO_BOOK_PAGES);

        rb->snprintf(line, sizeof(line),
                     "Page %d/%d  L/R:page Play:%s",
                     game.exact_catalog_page, pages,
                     game.scene_type == CP_SCENE_PET_FURNITURE ?
                     "secret" :
                     (game.scene_type == CP_SCENE_FURNITURE_CATALOG ?
                      "edit" : "archive"));
        rb->lcd_putsxy(3, CP_VIEW_H + 5, line);
    }
    else if (game.scene_type == CP_SCENE_FURNITURE_CATALOG ||
             game.scene_type == CP_SCENE_PET_FURNITURE)
    {
        rb->snprintf(line, sizeof(line),
                     "P%d%s %d/%d %.18s %dc O%d C:%d",
                     game.exact_catalog_page,
                     game.scene_type == CP_SCENE_PET_FURNITURE &&
                     game.pet_furniture_secret ? "S" : "",
                     game.exact_catalog_index + 1,
                     game.exact_catalog_count, game.furniture_item.name,
                     game.furniture_item.cost,
                     cp_furniture_quantity(game.furniture_item.id),
                     game.coins);
        rb->lcd_putsxy(3, CP_VIEW_H + 5, line);
    }
    else
    {
        bool owned = cp_igloo_item_owned(game.igloo_catalog_tab,
                                         game.igloo_item.id);
        bool active = cp_current_igloo_item(game.igloo_catalog_tab) ==
                      game.igloo_item.id;

        rb->snprintf(line, sizeof(line),
                     "P%d %d/%d %.18s %dc %s C:%d",
                     game.exact_catalog_page,
                     game.exact_catalog_index + 1,
                     game.exact_catalog_count, game.igloo_item.name,
                     game.igloo_item.cost,
                     active ? "ACTIVE" : (owned ? "OWNED" : "BUY"),
                     game.coins);
        rb->lcd_putsxy(3, CP_VIEW_H + 5, line);
    }
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
    rb->lcd_update();
    game.dirty = false;
}

static void cp_render_sound_studio(void)
{
    static const unsigned short sound_x[8] =
    {
        83, 114, 145, 177, 208, 245, 275, 301
    };
    static const unsigned char sound_y[5] =
    {
        64, 97, 130, 163, 196
    };
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();
    int id;

    rb->lcd_bitmap((const fb_data *)game.scene.data, 0, 0,
                   CP_VIEW_W, CP_VIEW_H);
    rb->lcd_set_foreground(LCD_RGBPACK(255, 220, 32));
    if (game.scene_type == CP_SCENE_SOUND_TITLE)
    {
        static const unsigned char title_y[3] = { 108, 145, 168 };
        static const unsigned char title_h[3] = { 31, 20, 20 };

        rb->lcd_drawrect(72, title_y[sound.title_choice], 79,
                         title_h[sound.title_choice]);
        rb->lcd_drawrect(71, title_y[sound.title_choice] - 1, 81,
                         title_h[sound.title_choice] + 2);
    }
    else if (game.scene_type == CP_SCENE_SOUND_SAVE)
    {
        char display_name[20];
        int x = sound.save_choice == 0 ? 97 : 167;
        int width;
        int height;

        rb->lcd_drawrect(x, 140, 59, 21);
        rb->lcd_drawrect(x - 1, 139, 61, 23);
        rb->lcd_setfont(FONT_UI);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_set_drawmode(DRMODE_FG);
        cp_copy(display_name, sizeof(display_name), sound.record_name);
        rb->lcd_getstringsize(display_name, &width, &height);
        rb->lcd_putsxy(MAX(88, (CP_VIEW_W - width) / 2), 119,
                       display_name);
        rb->lcd_set_drawmode(DRMODE_SOLID);
    }
    else if (game.scene_type == CP_SCENE_SOUND_INSTRUCTIONS)
    {
        /* The preserved page already contains its exact selection state. */
    }
    else if (game.scene_type == CP_SCENE_SOUND_SAVED)
    {
        if (sound.saved_slot >= 0)
        {
            int base = sound.saved_slot / 4 * 4;
            int row;

            rb->lcd_setfont(FONT_UI);
            rb->lcd_set_foreground(LCD_WHITE);
            rb->lcd_set_drawmode(DRMODE_FG);
            for (row = 0; row < 4; row++)
            {
                char display_name[16];

                if (cp_sound_track_name(base + row, display_name,
                                        sizeof(display_name)))
                    rb->lcd_putsxy(84, 61 + row * 37, display_name);
            }
            rb->lcd_set_drawmode(DRMODE_SOLID);
            row = sound.saved_slot % 4;
            rb->lcd_set_foreground(LCD_RGBPACK(255, 220, 32));
            rb->lcd_drawrect(8, 58 + row * 37, 183, 31);
            rb->lcd_drawrect(7, 57 + row * 37, 185, 33);
        }
    }
    else if (game.scene_type == CP_SCENE_SOUND_BOARD)
    {
        rb->lcd_set_foreground(LCD_RGBPACK(0, 235, 255));
        rb->lcd_drawrect(3, 68 + sound.genre * 24, 56, 21);
        for (id = 0; id < CP_SOUND_LOOPS; id++)
        {
            if (sound.loop_mask & (1u << id))
            {
                int column = id / 5;
                int row = id % 5;

                rb->lcd_drawrect(sound_x[column] - 14,
                                 sound_y[row] - 16, 29, 33);
            }
        }
        rb->lcd_set_foreground(LCD_RGBPACK(255, 220, 32));
        if (sound.column == 0)
        {
            rb->lcd_drawrect(3, 68 + sound.row * 24, 56, 21);
            rb->lcd_drawrect(2, 67 + sound.row * 24, 58, 23);
        }
        else
        {
            int x = sound_x[sound.column - 1];
            int y = sound_y[sound.row];

            rb->lcd_drawrect(x - 14, y - 16, 29, 33);
            rb->lcd_drawrect(x - 15, y - 17, 31, 35);
        }
    }
    cp_draw_status();
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    if (game.message_frames > 0)
        rb->lcd_putsxy(3, CP_VIEW_H + 5, game.message);
    else if (game.scene_type == CP_SCENE_SOUND_TITLE)
        rb->lcd_putsxy(3, CP_VIEW_H + 5,
                       "Wheel:choice Select:open Menu:close");
    else if (game.scene_type == CP_SCENE_SOUND_INSTRUCTIONS)
    {
        char line[64];

        rb->snprintf(line, sizeof(line),
                     "Instructions %d/5  Wheel/L/R:page%s",
                     sound.instruction_page + 1,
                     sound.instruction_page == 4 ? " Select:make" : "");
        rb->lcd_putsxy(3, CP_VIEW_H + 5, line);
    }
    else if (game.scene_type == CP_SCENE_SOUND_SAVED)
    {
        char line[64];
        char name[32];

        if (sound.saved_slot >= 0 &&
            !cp_sound_track_name(sound.saved_slot, name, sizeof(name)))
            rb->snprintf(name, sizeof(name), "My Track %d",
                         sound.saved_slot + 1);

        if (sound.saved_slot < 0)
            cp_copy(line, sizeof(line),
                    "No saved tracks. Select: Make Music  Menu:back");
        else if (sound.playback)
            rb->snprintf(line, sizeof(line),
                         "Playing %.20s %u:%02u Select:stop", name,
                         cp_sound_playback_elapsed() / 60000,
                         cp_sound_playback_elapsed() / 1000 % 60);
        else
            rb->snprintf(line, sizeof(line),
                         "%.22s Select:play Play:delete", name);
        rb->lcd_putsxy(3, CP_VIEW_H + 5, line);
    }
    else if (game.scene_type == CP_SCENE_SOUND_SAVE)
        rb->lcd_putsxy(3, CP_VIEW_H + 5,
                       "Play:name Wheel:choice Select:confirm");
    else
    {
        char line[64];

        if (sound.record_preparing)
            cp_copy(line, sizeof(line), "Recording countdown... Play:cancel");
        else if (sound.recording)
            rb->snprintf(line, sizeof(line),
                         "RECORDING %u:%02u  Play:stop/save",
                         cp_sound_record_elapsed() / 60000,
                         cp_sound_record_elapsed() / 1000 % 60);
        else
            cp_copy(line, sizeof(line),
                    "Wheel:row L/R:column Select:play Play:record");
        rb->lcd_putsxy(3, CP_VIEW_H + 5, line);
    }
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
    rb->lcd_update();
    game.dirty = false;
}

static void cp_render(void)
{
    const fb_data *background;
    int background_width;
    int background_x;
    int player_screen_x;
    int player_screen_y;

    if (game.scene_type == CP_SCENE_SHOP)
    {
        cp_render_shop();
        return;
    }

    if (game.scene_type == CP_SCENE_SPORT_SHOP ||
        game.scene_type == CP_SCENE_COSTUME_SHOP ||
        game.scene_type == CP_SCENE_PENGUIN_STYLE ||
        game.scene_type == CP_SCENE_NINJA_SHOP)
    {
        cp_render_sport_shop();
        return;
    }

    if (game.scene_type == CP_SCENE_PUFFLE_SHOP ||
        game.scene_type == CP_SCENE_PUFFLE_CARE ||
        game.scene_type == CP_SCENE_PUFFLE_FOOD ||
        game.scene_type == CP_SCENE_PUFFLE_TOYS ||
        game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
        game.scene_type == CP_SCENE_PUFFLE_HATS)
    {
        cp_render_puffle_menu();
        return;
    }

    if (game.scene_type == CP_SCENE_IGLOO_EDIT)
    {
        cp_render_igloo_edit();
        return;
    }

    if (game.scene_type == CP_SCENE_IGLOO_CATALOG)
    {
        cp_render_igloo_catalog();
        return;
    }

    if (game.scene_type == CP_SCENE_PET_FURNITURE ||
        game.scene_type == CP_SCENE_FURNITURE_CATALOG ||
        game.scene_type == CP_SCENE_IGLOO_BOOK)
    {
        cp_render_exact_catalog();
        return;
    }

    if (cp_sound_scene())
    {
        cp_render_sound_studio();
        return;
    }

    if (game.scene_type >= CP_SCENE_CART_TITLE &&
        game.scene_type <= CP_SCENE_CART_RESULTS)
    {
        cp_render_cart();
        return;
    }

    rb->lcd_setfont(FONT_UI);
    background = (const fb_data *)game.scene.data;
    background_width = game.scene.width;
    background_x = game.cam_x;
    if (game.scene_type == CP_SCENE_ROOM && game.animation_loaded &&
        !(game.room_index >= 0 &&
          cp_streq(game.rooms[game.room_index].id, "stage") &&
          game.concert_intro_ticks > 0))
    {
        int frame = game.room_index >= 0 &&
                    (cp_streq(game.rooms[game.room_index].id, "stage") ||
                     cp_streq(game.rooms[game.room_index].id,
                              "emma_sewer")) ?
                    game.concert_frame : game.city_frame;

        frame = MIN(MAX(frame, 0), game.animation_frames - 1);
        background = (const fb_data *)game.animation.data;
        background_width = game.animation.width;
        background_x += frame * CP_VIEW_W;
    }
    rb->lcd_bitmap_part(background,
                        background_x, game.cam_y, background_width,
                        0, 0, CP_VIEW_W, CP_VIEW_H);
    cp_draw_room_markers();
    if (game.scene_type == CP_SCENE_ROOM)
    {
        if (game.puffle_owned_mask != 0 &&
            cp_puffle_is_owned(game.puffle_type) &&
            (game.puffle_walking || cp_in_player_home()) &&
            !(cp_in_backyard() && !game.puffle_walking))
            cp_draw_puffle(game.x - game.cam_x < CP_VIEW_W - 55 ?
                           game.x - game.cam_x + 42 :
                           game.x - game.cam_x - 42,
                           game.y - game.cam_y + 4);
        player_screen_x = game.x - game.cam_x - CP_PLAYER_W / 2;
        player_screen_y = game.y - game.cam_y - CP_PLAYER_H / 2;
        cp_draw_player(player_screen_x, player_screen_y);
    }
    else if (game.scene_type == CP_SCENE_WELCOME)
    {
        player_screen_x = game.x - CP_PLAYER_W / 2;
        player_screen_y = game.y - CP_PLAYER_H / 2;
        cp_draw_player(player_screen_x, player_screen_y);
    }
    cp_draw_status();
    rb->lcd_update();
    game.dirty = false;
}

static int cp_get_input(void)
{
    return pluginlib_getaction(MAX(1, HZ / 30), plugin_contexts,
                              ARRAYLEN(plugin_contexts));
}

enum plugin_status plugin_start(const void *parameter)
{
    int action;
    size_t plugin_buffer_size;
    enum plugin_status status = PLUGIN_OK;
    bool running = true;

    (void)parameter;

    player_pixels = rb->plugin_get_buffer(&plugin_buffer_size);
    if (player_pixels == NULL ||
        plugin_buffer_size < CP_PLAYER_BUFFER_BYTES +
                             CP_ANIMATION_BUFFER_BYTES)
    {
        rb->splash(HZ * 3, "Club Penguin needs its 3 MiB plugin buffer.");
        return PLUGIN_ERROR;
    }
    /* Both caches are inside Rockbox's fixed plugin arena.  Neither requests
     * core memory nor borrows/shrinks the playback audio buffer. */
    animation_pixels = player_pixels + CP_PLAYER_BUFFER_ELEMS;
    animation_buffer_size = plugin_buffer_size - CP_PLAYER_BUFFER_BYTES;

    cp_init();
    if (!game.assets_loaded)
    {
        rb->splash(HZ * 3, game.error);
        return PLUGIN_ERROR;
    }

    while (running)
    {
        cp_cart_update_menu_latch();
        cp_cart_tick_if_due();
        cp_walk_tick_if_due();
        cp_night_city_tick();
        cp_concert_tick();
        cp_puffle_tick();
        cp_sound_record_tick();
        cp_sound_playback_tick();

        if (game.message_frames > 0)
        {
            game.message_frames--;
            if (game.message_frames == 0)
                game.dirty = true;
        }

        if (game.dirty)
            cp_render();
        action = cp_get_input();
        if (!cp_map_chord_held())
            game.map_chord_latched = false;
        if (cp_quit_chord_held())
            action = CP_QUIT_ACTION;
        else if (cp_map_chord_held() && !game.map_chord_latched)
        {
            game.map_chord_latched = true;
            cp_open_map();
            continue;
        }

        switch (action)
        {
            case CP_QUIT_ACTION:
                cp_quit(&status, &running);
                break;

            case CP_CANCEL_ACTION:
                cp_go_back();
                break;

            case CP_UP_ACTION:
            case CP_UP_REPEAT:
                if (game.scene_type == CP_SCENE_SHOP)
                    cp_shop_action(action);
                else if (game.scene_type == CP_SCENE_SPORT_SHOP ||
                         game.scene_type == CP_SCENE_COSTUME_SHOP ||
                         game.scene_type == CP_SCENE_PENGUIN_STYLE ||
                         game.scene_type == CP_SCENE_NINJA_SHOP)
                    cp_sport_action(action);
                else if (game.scene_type == CP_SCENE_PUFFLE_SHOP ||
                         game.scene_type == CP_SCENE_PUFFLE_CARE ||
                         game.scene_type == CP_SCENE_PUFFLE_FOOD ||
                         game.scene_type == CP_SCENE_PUFFLE_TOYS ||
                         game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
                         game.scene_type == CP_SCENE_PUFFLE_HATS ||
                         game.scene_type == CP_SCENE_PET_FURNITURE ||
                         game.scene_type == CP_SCENE_IGLOO_EDIT ||
                         game.scene_type == CP_SCENE_IGLOO_CATALOG ||
                         game.scene_type == CP_SCENE_FURNITURE_CATALOG ||
                         game.scene_type == CP_SCENE_IGLOO_BOOK)
                    cp_special_action(action);
                else if (cp_sound_scene())
                    cp_sound_action(action);
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                else if (game.scene_type != CP_SCENE_WELCOME &&
                         !(game.scene_type == CP_SCENE_ROOM &&
                           cp_touch_walk_active()))
                    cp_queue_move(0, -CP_INPUT_STEP);
                break;

            case CP_DOWN_ACTION:
            case CP_DOWN_REPEAT:
                if (game.scene_type == CP_SCENE_SHOP)
                    cp_shop_action(action);
                else if (game.scene_type == CP_SCENE_SPORT_SHOP ||
                         game.scene_type == CP_SCENE_COSTUME_SHOP ||
                         game.scene_type == CP_SCENE_PENGUIN_STYLE ||
                         game.scene_type == CP_SCENE_NINJA_SHOP)
                    cp_sport_action(action);
                else if (game.scene_type == CP_SCENE_PUFFLE_SHOP ||
                         game.scene_type == CP_SCENE_PUFFLE_CARE ||
                         game.scene_type == CP_SCENE_PUFFLE_FOOD ||
                         game.scene_type == CP_SCENE_PUFFLE_TOYS ||
                         game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
                         game.scene_type == CP_SCENE_PUFFLE_HATS ||
                         game.scene_type == CP_SCENE_PET_FURNITURE ||
                         game.scene_type == CP_SCENE_IGLOO_EDIT ||
                         game.scene_type == CP_SCENE_IGLOO_CATALOG ||
                         game.scene_type == CP_SCENE_FURNITURE_CATALOG ||
                         game.scene_type == CP_SCENE_IGLOO_BOOK)
                    cp_special_action(action);
                else if (cp_sound_scene())
                    cp_sound_action(action);
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                else if (game.scene_type != CP_SCENE_WELCOME &&
                         !(game.scene_type == CP_SCENE_ROOM &&
                           cp_touch_walk_active()))
                    cp_queue_move(0, CP_INPUT_STEP);
                break;

            case CP_LEFT_ACTION:
            case CP_LEFT_REPEAT:
                if (game.scene_type == CP_SCENE_SHOP)
                    cp_shop_action(action);
                else if (game.scene_type == CP_SCENE_SPORT_SHOP ||
                         game.scene_type == CP_SCENE_COSTUME_SHOP ||
                         game.scene_type == CP_SCENE_PENGUIN_STYLE ||
                         game.scene_type == CP_SCENE_NINJA_SHOP)
                    cp_sport_action(action);
                else if (game.scene_type == CP_SCENE_PUFFLE_SHOP ||
                         game.scene_type == CP_SCENE_PUFFLE_CARE ||
                         game.scene_type == CP_SCENE_PUFFLE_FOOD ||
                         game.scene_type == CP_SCENE_PUFFLE_TOYS ||
                         game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
                         game.scene_type == CP_SCENE_PUFFLE_HATS ||
                         game.scene_type == CP_SCENE_PET_FURNITURE ||
                         game.scene_type == CP_SCENE_IGLOO_EDIT ||
                         game.scene_type == CP_SCENE_IGLOO_CATALOG ||
                         game.scene_type == CP_SCENE_FURNITURE_CATALOG ||
                         game.scene_type == CP_SCENE_IGLOO_BOOK)
                    cp_special_action(action);
                else if (cp_sound_scene())
                    cp_sound_action(action);
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                else if (game.scene_type != CP_SCENE_WELCOME &&
                         !(game.scene_type == CP_SCENE_ROOM &&
                           cp_touch_walk_active()))
                    cp_queue_move(-CP_INPUT_STEP, 0);
                break;

            case CP_RIGHT_ACTION:
            case CP_RIGHT_REPEAT:
                if (game.scene_type == CP_SCENE_SHOP)
                    cp_shop_action(action);
                else if (game.scene_type == CP_SCENE_SPORT_SHOP ||
                         game.scene_type == CP_SCENE_COSTUME_SHOP ||
                         game.scene_type == CP_SCENE_PENGUIN_STYLE ||
                         game.scene_type == CP_SCENE_NINJA_SHOP)
                    cp_sport_action(action);
                else if (game.scene_type == CP_SCENE_PUFFLE_SHOP ||
                         game.scene_type == CP_SCENE_PUFFLE_CARE ||
                         game.scene_type == CP_SCENE_PUFFLE_FOOD ||
                         game.scene_type == CP_SCENE_PUFFLE_TOYS ||
                         game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
                         game.scene_type == CP_SCENE_PUFFLE_HATS ||
                         game.scene_type == CP_SCENE_PET_FURNITURE ||
                         game.scene_type == CP_SCENE_IGLOO_EDIT ||
                         game.scene_type == CP_SCENE_IGLOO_CATALOG ||
                         game.scene_type == CP_SCENE_FURNITURE_CATALOG ||
                         game.scene_type == CP_SCENE_IGLOO_BOOK)
                    cp_special_action(action);
                else if (cp_sound_scene())
                    cp_sound_action(action);
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                else if (game.scene_type != CP_SCENE_WELCOME &&
                         !(game.scene_type == CP_SCENE_ROOM &&
                           cp_touch_walk_active()))
                    cp_queue_move(CP_INPUT_STEP, 0);
                break;

            case CP_SELECT_ACTION:
                if (game.scene_type == CP_SCENE_WELCOME)
                    cp_advance_welcome();
                else if (game.scene_type == CP_SCENE_SHOP)
                    cp_shop_action(action);
                else if (game.scene_type == CP_SCENE_SPORT_SHOP ||
                         game.scene_type == CP_SCENE_COSTUME_SHOP ||
                         game.scene_type == CP_SCENE_PENGUIN_STYLE ||
                         game.scene_type == CP_SCENE_NINJA_SHOP)
                    cp_sport_action(action);
                else if (game.scene_type == CP_SCENE_PUFFLE_SHOP ||
                         game.scene_type == CP_SCENE_PUFFLE_CARE ||
                         game.scene_type == CP_SCENE_PUFFLE_FOOD ||
                         game.scene_type == CP_SCENE_PUFFLE_TOYS ||
                         game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
                         game.scene_type == CP_SCENE_PUFFLE_HATS ||
                         game.scene_type == CP_SCENE_PET_FURNITURE ||
                         game.scene_type == CP_SCENE_IGLOO_EDIT ||
                         game.scene_type == CP_SCENE_IGLOO_CATALOG ||
                         game.scene_type == CP_SCENE_FURNITURE_CATALOG ||
                         game.scene_type == CP_SCENE_IGLOO_BOOK)
                    cp_special_action(action);
                else if (cp_sound_scene())
                    cp_sound_action(action);
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                else
                    cp_interact();
                break;

            case CP_PLAY_ACTION:
                if (game.scene_type == CP_SCENE_SHOP)
                    cp_shop_action(action);
                else if (game.scene_type == CP_SCENE_SPORT_SHOP ||
                         game.scene_type == CP_SCENE_COSTUME_SHOP ||
                         game.scene_type == CP_SCENE_PENGUIN_STYLE ||
                         game.scene_type == CP_SCENE_NINJA_SHOP)
                    cp_sport_action(action);
                else if (game.scene_type == CP_SCENE_PUFFLE_SHOP ||
                    game.scene_type == CP_SCENE_PUFFLE_CARE ||
                    game.scene_type == CP_SCENE_PUFFLE_FOOD ||
                    game.scene_type == CP_SCENE_PUFFLE_TOYS ||
                    game.scene_type == CP_SCENE_PUFFLE_TRICKS ||
                    game.scene_type == CP_SCENE_PUFFLE_HATS ||
                    game.scene_type == CP_SCENE_PET_FURNITURE ||
                    game.scene_type == CP_SCENE_IGLOO_EDIT ||
                    game.scene_type == CP_SCENE_IGLOO_CATALOG ||
                    game.scene_type == CP_SCENE_FURNITURE_CATALOG ||
                    game.scene_type == CP_SCENE_IGLOO_BOOK)
                    cp_special_action(action);
                else if (cp_sound_scene())
                    cp_sound_action(action);
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                break;

            default:
                if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    cp_sound_close_audio();
                    cp_write_save();
                    return PLUGIN_USB_CONNECTED;
                }
                break;
        }
    }

    return status;
}
