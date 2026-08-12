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
#define CP_SHOP_PAGE_PATTERN CP_ASSET_DIR "/shop/page%d.bmp"
#define CP_SAVE_FILE CP_ASSET_DIR "/save.dat"
#define CP_SAVE_TMP_FILE CP_ASSET_DIR "/save.tmp"
#define CP_CART_TITLE_FILE CP_ASSET_DIR "/minigames/cart_surfer/title.bmp"
#define CP_CART_TUNNEL_FILE CP_ASSET_DIR "/minigames/cart_surfer/tunnel.bmp"
#define CP_CART_SPRITES_FILE CP_ASSET_DIR "/minigames/cart_surfer/cart.bmp"
#define CP_TOOLBAR_FILE CP_ASSET_DIR "/ui/toolbar.bmp"

#define CP_STATUS_H 20
#define CP_VIEW_W LCD_WIDTH
#define CP_VIEW_H (LCD_HEIGHT - CP_STATUS_H)
#define CP_WORLD_W CP_VIEW_W
#define CP_WORLD_H CP_VIEW_H
#define CP_PLAYER_W 36
#define CP_PLAYER_H 38
#define CP_PLAYER_ANIM_FRAMES 3
#define CP_PLAYER_SELECTOR_FRAME 12
#define CP_PLAYER_FRAMES 13
#define CP_PLAYER_STRIP_W (CP_PLAYER_W * CP_PLAYER_FRAMES)
#define CP_PLAYER_STRIP_H CP_PLAYER_H
#define CP_INPUT_STEP 6
#define CP_WALK_STEP 2
#define CP_WALK_AHEAD 12
#define CP_WALK_RATE 25
#define CP_MESSAGE_TTL 70
#define CP_MAX_HOTSPOTS 16
#define CP_MAX_ROOMS 16
#define CP_MAX_INTERACTIONS 24
#define CP_MAX_SHOP_ITEMS 51
#define CP_SHOP_PAGES 4
#define CP_TEXT_BUF 4096
#define CP_ANIM_RATE 3
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

/* read_bmp_file()/read_bmp_fd() need extra scratch space *inside* the
 * destination buffer whenever the source bitmap is wider than
 * BM_MAX_WIDTH (i.e. wider than the LCD). The player sprite strip is wider
 * than the iPod LCD, so without this padding read_bmp_file() returns an
 * error (-6) and the asset is reported as "missing" even though it loaded.
 * See apps/recorder/bmp.c around the "bm->width > BM_MAX_WIDTH" checks. */
#define CP_BMP_SCRATCH_ELEMS(w) \
    ((((w) * 4 + 8) + (int)sizeof(fb_data) - 1) / (int)sizeof(fb_data))

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
static const struct button_mapping cp_main_ctx[] =
{
    { PLA_EXIT,        BUTTON_MENU|BUTTON_SELECT,         BUTTON_NONE },
    { PLA_SELECT,      BUTTON_SELECT,                     BUTTON_NONE },
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
    { PLA_UP_REPEAT,   BUTTON_MENU|BUTTON_REPEAT,         BUTTON_NONE },
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
#define CP_MULTIPLAYER_ACTION PLA_SELECT_REPEAT
#define CP_UP_REPEAT PLA_UP_REPEAT
#define CP_DOWN_REPEAT PLA_DOWN_REPEAT
#define CP_LEFT_REPEAT PLA_LEFT_REPEAT
#define CP_RIGHT_REPEAT PLA_RIGHT_REPEAT

enum cp_scene_type
{
    CP_SCENE_MAP = 0,
    CP_SCENE_ROOM,
    CP_SCENE_SHOP,
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
    CP_ACTION_MESSAGE
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

struct cp_interaction
{
    char room[24];
    struct cp_hotspot hotspot;
};

struct cp_shop_item
{
    char name[24];
    int page;
    int slot;
    int cost;
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
    struct bitmap player;
    struct bitmap toolbar;
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
    int interaction_count;
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
    struct cp_interaction interactions[CP_MAX_INTERACTIONS];
    struct cp_shop_item shop_items[CP_MAX_SHOP_ITEMS];
    struct cp_cart_state cart;
    char saved_room[24];
    int saved_x;
    int saved_y;
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
    long next_walk_tick;
    bool assets_loaded;
    bool dirty;
    bool map_chord_latched;
#ifdef USB_ENABLE_ETHERNET
    bool multiplayer_enabled;
    bool remote_visible;
    uint32_t multiplayer_session;
    uint32_t remote_session;
    int remote_x;
    int remote_y;
    int remote_direction;
    int remote_frame;
    char remote_room[24];
    long multiplayer_next_send;
    long remote_last_seen;
#endif
};

static fb_data scene_pixels[CP_WORLD_W * CP_WORLD_H
                            + CP_BMP_SCRATCH_ELEMS(CP_WORLD_W)];
static fb_data player_pixels[
    MAX(CP_PLAYER_STRIP_W * CP_PLAYER_STRIP_H +
        CP_BMP_SCRATCH_ELEMS(CP_PLAYER_STRIP_W),
        CP_CART_SHEET_W * CP_CART_SHEET_H +
        CP_BMP_SCRATCH_ELEMS(CP_CART_SHEET_W))];
static fb_data toolbar_pixels[CP_VIEW_W * CP_STATUS_H];
static char text_buf[CP_TEXT_BUF];

static struct cp_game game;
static void cp_set_message(const char *text);

#ifdef USB_ENABLE_ETHERNET
#define CP_NET_PACKET_SIZE 44
#define CP_NET_RATE MAX(1, HZ / 10)
#define CP_NET_TIMEOUT (3 * HZ)

static uint16_t cp_net_get_be16(const unsigned char *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t cp_net_get_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void cp_net_put_be16(unsigned char *p, uint16_t value)
{
    p[0] = value >> 8;
    p[1] = value;
}

static void cp_net_put_be32(unsigned char *p, uint32_t value)
{
    p[0] = value >> 24;
    p[1] = value >> 16;
    p[2] = value >> 8;
    p[3] = value;
}

static const char *cp_net_room(void)
{
    if (game.scene_type == CP_SCENE_ROOM && game.room_index >= 0 &&
        game.room_index < game.room_count)
        return game.rooms[game.room_index].id;
    return "";
}

static void cp_multiplayer_tick(void)
{
    unsigned char packet[CP_NET_PACKET_SIZE];
    int length;

    rb->usb_internet_service();
    if (!game.multiplayer_enabled || !rb->usb_internet_connected())
    {
        if (game.remote_visible)
        {
            game.remote_visible = false;
            game.dirty = true;
        }
        return;
    }

    if (!game.multiplayer_next_send ||
        !TIME_BEFORE(*rb->current_tick, game.multiplayer_next_send))
    {
        rb->memset(packet, 0, sizeof(packet));
        rb->memcpy(packet, "CPM1", 4);
        packet[4] = 1;
        cp_net_put_be32(packet + 5, game.multiplayer_session);
        rb->strlcpy((char *)packet + 9, cp_net_room(), 24);
        cp_net_put_be16(packet + 33, game.x);
        cp_net_put_be16(packet + 35, game.y);
        packet[37] = game.direction;
        packet[38] = game.anim_frame;
        packet[39] = game.scene_type;
        rb->usb_internet_send(47701, packet, sizeof(packet));
        game.multiplayer_next_send = *rb->current_tick + CP_NET_RATE;
    }

    while ((length = rb->usb_internet_receive(47701, packet,
                                               sizeof(packet))) > 0)
    {
        uint32_t session;
        if (length != CP_NET_PACKET_SIZE || rb->memcmp(packet, "CPM1", 4) ||
            packet[4] != 1)
            continue;
        session = cp_net_get_be32(packet + 5);
        if (session == game.multiplayer_session)
            continue;
        game.remote_session = session;
        rb->memcpy(game.remote_room, packet + 9, 24);
        game.remote_room[sizeof(game.remote_room) - 1] = '\0';
        game.remote_x = cp_net_get_be16(packet + 33);
        game.remote_y = cp_net_get_be16(packet + 35);
        game.remote_direction = packet[37] % 4;
        game.remote_frame = packet[38] % CP_PLAYER_ANIM_FRAMES;
        game.remote_last_seen = *rb->current_tick;
        game.remote_visible = true;
        game.dirty = true;
    }

    if (game.remote_visible &&
        TIME_AFTER(*rb->current_tick, game.remote_last_seen + CP_NET_TIMEOUT))
    {
        game.remote_visible = false;
        game.dirty = true;
    }
}

static void cp_toggle_multiplayer(void)
{
    game.multiplayer_enabled = !game.multiplayer_enabled;
    game.remote_visible = false;
    if (game.multiplayer_enabled)
        cp_set_message(rb->usb_internet_connected() ?
                       "Multiplayer on - looking for one penguin." :
                       "Multiplayer on - connect USB Internet.");
    else
        cp_set_message("Multiplayer off.");
}
#endif

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
    { "Dojo", "Enter the Dojo.", "dojo", 185, 24, 16, 160, 170 },
    { "Cove", "Enter the Cove.", "cove", 271, 74, 19, 160, 170 },
    { "Beach", "Enter the Beach.", "beach", 47, 51, 16, 160, 170 },
    { "Snow Forts", "Enter the Snow Forts.",
      "snow_forts", 171, 161, 17, 160, 170 },
    { "Forest", "Enter the Forest.", "forest", 260, 188, 16, 160, 170 },
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
    { "dojo", "Dojo", "rooms/dojo.bmp",
      160, 170, 35, 135, 285, 198 },
    { "cove", "Cove", "rooms/cove.bmp",
      160, 170, 25, 115, 295, 198 },
    { "beach", "Beach", "rooms/beach.bmp",
      160, 170, 40, 110, 280, 198 },
    { "snow_forts", "Snow Forts", "rooms/snow_forts.bmp",
      160, 170, 20, 105, 300, 198 },
    { "forest", "Forest", "rooms/forest.bmp",
      160, 170, 35, 100, 285, 198 },
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
    else if (cp_streq(text, "message"))
        *action = CP_ACTION_MESSAGE;
    else
        return false;

    return true;
}

static void cp_parse_interaction_line(char *line)
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
    struct cp_interaction *interaction;
    int x;
    int y;
    int radius;
    int to_x = 0;
    int to_y = 0;
    enum cp_action_type action;

    if (game.interaction_count >= CP_MAX_INTERACTIONS ||
        !cp_has_text(room) || !cp_has_text(id) || !cp_has_text(label) ||
        !cp_parse_int(x_text, &x) || !cp_parse_int(y_text, &y) ||
        !cp_parse_int(radius_text, &radius) || radius <= 0 ||
        !cp_parse_action(action_text, &action))
        return;

    interaction = &game.interactions[game.interaction_count++];
    rb->memset(interaction, 0, sizeof(*interaction));
    cp_copy(interaction->room, sizeof(interaction->room), room);
    cp_copy(interaction->hotspot.id, sizeof(interaction->hotspot.id), id);
    cp_copy(interaction->hotspot.name, sizeof(interaction->hotspot.name),
            label);
    cp_copy(interaction->hotspot.detail,
            sizeof(interaction->hotspot.detail), label);
    cp_copy(interaction->hotspot.target,
            sizeof(interaction->hotspot.target), target);
    interaction->hotspot.x = x;
    interaction->hotspot.y = y;
    interaction->hotspot.radius = radius;
    interaction->hotspot.action = action;
    if (cp_parse_int(to_x_text, &to_x) && cp_parse_int(to_y_text, &to_y))
    {
        interaction->hotspot.to_x = to_x;
        interaction->hotspot.to_y = to_y;
    }
}

static bool cp_load_interactions(void)
{
    char *cursor;
    char *line;

    if (cp_read_text_file(CP_INTERACTIONS_DATA_FILE) < 0)
        return false;

    game.interaction_count = 0;
    cursor = text_buf;
    while ((line = cp_next_line(&cursor)) != NULL)
    {
        if (cp_data_line(line))
            cp_parse_interaction_line(line);
    }

    return game.interaction_count > 0;
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

static bool cp_load_player_asset(void)
{
    return cp_load_bitmap(CP_PLAYER_FILE, &game.player, player_pixels,
                          sizeof(player_pixels), CP_PLAYER_STRIP_W,
                          CP_PLAYER_STRIP_H, FORMAT_NATIVE);
}

static bool cp_load_assets(void)
{
    bool scene_ok;
    bool player_ok;
    bool toolbar_ok;

    scene_ok = cp_load_scene(CP_WORLD_FILE, CP_WORLD_W, CP_WORLD_H);
    player_ok = cp_load_player_asset();
    toolbar_ok = cp_load_bitmap(CP_TOOLBAR_FILE, &game.toolbar,
                                toolbar_pixels, sizeof(toolbar_pixels),
                                CP_VIEW_W, CP_STATUS_H, FORMAT_NATIVE);

    game.assets_loaded = scene_ok && player_ok && toolbar_ok;
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

static void cp_get_walk_bounds(int *left, int *top, int *right, int *bottom)
{
    *left = CP_PLAYER_W / 2;
    *top = CP_PLAYER_H / 2;
    *right = game.scene.width - CP_PLAYER_W / 2;
    *bottom = game.scene.height - CP_PLAYER_H / 2;

    if (game.scene_type == CP_SCENE_ROOM && game.room_index >= 0 &&
        game.room_index < game.room_count)
    {
        struct cp_room *room = &game.rooms[game.room_index];
        *left = MAX(*left, room->walk_left);
        *top = MAX(*top, room->walk_top);
        *right = MIN(*right, room->walk_right);
        *bottom = MIN(*bottom, room->walk_bottom);
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

static void cp_set_room_hotspots(void)
{
    const char *room_id = game.rooms[game.room_index].id;
    struct cp_hotspot *back;
    int i;

    game.hotspot_count = 0;
    for (i = 0; i < game.interaction_count &&
                game.hotspot_count < CP_MAX_HOTSPOTS - 1; i++)
    {
        if (cp_streq(game.interactions[i].room, room_id))
            game.hotspots[game.hotspot_count++] =
                game.interactions[i].hotspot;
    }

    back = &game.hotspots[game.hotspot_count++];
    rb->memset(back, 0, sizeof(*back));
    cp_copy(back->id, sizeof(back->id), "map_exit");
    cp_copy(back->name, sizeof(back->name), "Map");
    cp_copy(back->detail, sizeof(back->detail),
            "Return to the island map.");
    cp_copy(back->target, sizeof(back->target), "map");
    back->action = CP_ACTION_MAP;
    back->x = game.rooms[game.room_index].walk_left;
    back->y = game.rooms[game.room_index].walk_bottom;
    back->radius = 24;
    back->to_x = game.map_x;
    back->to_y = game.map_y;

    game.selected_hotspot = cp_nearest_hotspot();
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
    cp_join_asset_path(path, sizeof(path), room->bitmap);
    if (!cp_load_scene(path, CP_VIEW_W, CP_VIEW_H))
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
    cp_update_camera();
    cp_set_room_hotspots();
    game.selected_hotspot = cp_nearest_hotspot();
    game.dirty = true;
    return true;
}

static int cp_clamp_save_value(int value, int maximum)
{
    if (value < 0)
        return 0;
    if (value > maximum)
        return maximum;
    return value;
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
        }
    }
}

static bool cp_write_save(void)
{
    char save[512];
    const char *room = "map";
    int save_x = game.x;
    int save_y = game.y;
    int length;
    int fd;
    int wrote;

    if (game.scene_type == CP_SCENE_ROOM && game.room_index >= 0 &&
        game.room_index < game.room_count)
        room = game.rooms[game.room_index].id;
    else if (game.scene_type == CP_SCENE_SHOP &&
             game.shop_return_room >= 0 &&
             game.shop_return_room < game.room_count)
    {
        room = game.rooms[game.shop_return_room].id;
        save_x = game.shop_return_x;
        save_y = game.shop_return_y;
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
                          "cart_best_score=%d\ncart_best_combo=%d\n"
                          "owned_lo=%08x\nowned_hi=%08x\n"
                          "equipped_head=%d\nequipped_body=%d\n"
                          "equipped_feet=%d\nequipped_color=%d\n",
                          game.coins, room, save_x, save_y,
                          game.map_x, game.map_y, game.cart_best_score,
                          game.cart_best_combo, game.owned_lo, game.owned_hi,
                          game.equipped[0], game.equipped[1],
                          game.equipped[2], game.equipped[3]);
    if (length <= 0 || length >= (int)sizeof(save))
        return false;

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
                        sizeof(player_pixels), CP_CART_SHEET_W,
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

    game.equipped[item->page] = item->slot;
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
}

static void cp_init(void)
{
    int i;

    rb->memset(&game, 0, sizeof(game));
    game.coins = 500;
    game.direction = CP_DIR_DOWN;
#ifdef USB_ENABLE_ETHERNET
    game.multiplayer_session = (uint32_t)*rb->current_tick ^
                               (uint32_t)(uintptr_t)&game;
    if (game.multiplayer_session == 0)
        game.multiplayer_session = 1;
#endif

    if (!cp_load_rooms())
        cp_use_builtin_rooms();

    if (!cp_load_hotspots())
        cp_use_builtin_hotspots();

    cp_load_interactions();
    cp_load_shop_data();
    for (i = 0; i < CP_SHOP_PAGES; i++)
        game.equipped[i] = -1;
    if (game.shop_item_count > 0)
    {
        int blue = cp_shop_first_item(3) + 5;
        cp_shop_own_item(blue);
        game.equipped[3] = 5;
    }

    if (!cp_load_assets())
        return;

    game.map_x = game.map_hotspots[0].x;
    game.map_y = game.map_hotspots[0].y;
    cp_load_save();
    for (i = 0; i < CP_SHOP_PAGES; i++)
    {
        int equipped = cp_shop_find_item(i, game.equipped[i]);
        if (game.equipped[i] >= 0 &&
            (equipped < 0 || !cp_shop_item_owned(equipped)))
            game.equipped[i] = -1;
    }
    if (game.equipped[3] < 0 && game.shop_item_count > 0)
    {
        int blue = cp_shop_find_item(3, 5);
        cp_shop_own_item(blue);
        game.equipped[3] = 5;
    }
    if (game.map_x <= 0 || game.map_y <= 0)
    {
        game.map_x = game.map_hotspots[0].x;
        game.map_y = game.map_hotspots[0].y;
    }

    if (!cp_streq(game.saved_room, "map") &&
        cp_find_room(game.saved_room) >= 0)
        cp_enter_room(cp_find_room(game.saved_room), game.saved_x,
                      game.saved_y);
    else
        cp_enter_map_at(game.saved_x, game.saved_y);

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

    game.target_x += dx;
    game.target_y += dy;
    cp_clamp_walk_target();
}

static void cp_walk_tick_if_due(void)
{
    int dx = 0;
    int dy = 0;

    if (game.scene_type != CP_SCENE_ROOM ||
        !(TIME_AFTER(*rb->current_tick, game.next_walk_tick) ||
          *rb->current_tick == game.next_walk_tick))
        return;

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
    {
        int held = rb->button_status();

        if (held & BUTTON_LEFT)
            game.target_x -= CP_WALK_STEP;
        if (held & BUTTON_RIGHT)
            game.target_x += CP_WALK_STEP;
        if ((held & (BUTTON_MENU | BUTTON_SELECT)) !=
            (BUTTON_MENU | BUTTON_SELECT) && (held & BUTTON_MENU))
            game.target_y -= CP_WALK_STEP;
        if (held & BUTTON_PLAY)
            game.target_y += CP_WALK_STEP;
        cp_clamp_walk_target();
    }
#endif

    if (game.target_x < game.x)
        dx = -MIN(CP_WALK_STEP, game.x - game.target_x);
    else if (game.target_x > game.x)
        dx = MIN(CP_WALK_STEP, game.target_x - game.x);
    if (game.target_y < game.y)
        dy = -MIN(CP_WALK_STEP, game.y - game.target_y);
    else if (game.target_y > game.y)
        dy = MIN(CP_WALK_STEP, game.target_y - game.y);

    if (dx != 0 || dy != 0)
        cp_move_player(dx, dy);
    game.next_walk_tick = *rb->current_tick + MAX(1, HZ / CP_WALK_RATE);
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
        else
            cp_set_message("Unknown minigame target.");
        return;
    }

    if (hotspot->action == CP_ACTION_SHOP)
    {
        if (!cp_enter_shop())
            cp_set_message("Penguin Style assets are incomplete.");
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
    if (game.scene_type == CP_SCENE_SHOP)
    {
        cp_leave_shop();
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

    if (game.scene_type >= CP_SCENE_CART_TITLE &&
        game.scene_type <= CP_SCENE_CART_RESULTS && !cp_load_player_asset())
        return;

    if (cp_enter_map_at(game.map_x, game.map_y))
    {
        cp_set_message("Returned to the island map.");
        cp_write_save();
    }
}

static void cp_quit(enum plugin_status *status, bool *running)
{
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
    rb->lcd_bitmap((const fb_data *)game.toolbar.data, 0, CP_VIEW_H,
                   CP_VIEW_W, CP_STATUS_H);
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
        "HEAD ITEMS", "BODY ITEMS", "FEET ITEMS", "COLORS"
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
    rb->lcd_putsxy(186, 130, "Wheel: item");
    rb->lcd_putsxy(186, 146, "Left/Right: page");
    rb->lcd_putsxy(186, 162, "Select: buy/equip");
    rb->lcd_putsxy(186, 178, "Menu: back");

    cp_draw_status();
    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
    rb->lcd_update();
    game.dirty = false;
}

static void cp_render(void)
{
    int player_screen_x;
    int player_screen_y;

    if (game.scene_type == CP_SCENE_SHOP)
    {
        cp_render_shop();
        return;
    }

    if (game.scene_type >= CP_SCENE_CART_TITLE &&
        game.scene_type <= CP_SCENE_CART_RESULTS)
    {
        cp_render_cart();
        return;
    }

    rb->lcd_setfont(FONT_UI);
    rb->lcd_bitmap_part((const fb_data *)game.scene.data,
                        game.cam_x, game.cam_y, game.scene.width,
                        0, 0, CP_VIEW_W, CP_VIEW_H);
    cp_draw_room_markers();
    if (game.scene_type == CP_SCENE_ROOM)
    {
#ifdef USB_ENABLE_ETHERNET
        if (game.multiplayer_enabled && game.remote_visible &&
            !cp_streq(game.remote_room, cp_net_room()))
            game.remote_visible = false;
        if (game.multiplayer_enabled && game.remote_visible)
        {
            int remote_frame = game.remote_direction * CP_PLAYER_ANIM_FRAMES +
                               game.remote_frame;
            int remote_x = game.remote_x - game.cam_x - CP_PLAYER_W / 2;
            int remote_y = game.remote_y - game.cam_y - CP_PLAYER_H / 2;
            cp_draw_player_frame(remote_frame, remote_x, remote_y);
        }
#endif
        player_screen_x = game.x - game.cam_x - CP_PLAYER_W / 2;
        player_screen_y = game.y - game.cam_y - CP_PLAYER_H / 2;
        cp_draw_player(player_screen_x, player_screen_y);
    }
    cp_draw_status();
    rb->lcd_update();
    game.dirty = false;
}

static int cp_get_input(void)
{
    return pluginlib_getaction(MAX(1, HZ / 25), plugin_contexts,
                              ARRAYLEN(plugin_contexts));
}

enum plugin_status plugin_start(const void *parameter)
{
    int action;
    enum plugin_status status = PLUGIN_OK;
    bool running = true;

    (void)parameter;

    cp_init();
    if (!game.assets_loaded)
    {
        rb->splash(HZ * 3, game.error);
        return PLUGIN_ERROR;
    }

    while (running)
    {
#ifdef USB_ENABLE_ETHERNET
        cp_multiplayer_tick();
#endif
        cp_cart_update_menu_latch();
        cp_cart_tick_if_due();
        cp_walk_tick_if_due();

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
#ifdef USB_ENABLE_ETHERNET
            case CP_MULTIPLAYER_ACTION:
                cp_toggle_multiplayer();
                break;
#endif
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
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                else
                    cp_queue_move(0, -CP_INPUT_STEP);
                break;

            case CP_DOWN_ACTION:
            case CP_DOWN_REPEAT:
                if (game.scene_type == CP_SCENE_SHOP)
                    cp_shop_action(action);
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                else
                    cp_queue_move(0, CP_INPUT_STEP);
                break;

            case CP_LEFT_ACTION:
            case CP_LEFT_REPEAT:
                if (game.scene_type == CP_SCENE_SHOP)
                    cp_shop_action(action);
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                else
                    cp_queue_move(-CP_INPUT_STEP, 0);
                break;

            case CP_RIGHT_ACTION:
            case CP_RIGHT_REPEAT:
                if (game.scene_type == CP_SCENE_SHOP)
                    cp_shop_action(action);
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                else
                    cp_queue_move(CP_INPUT_STEP, 0);
                break;

            case CP_SELECT_ACTION:
                if (game.scene_type == CP_SCENE_SHOP)
                    cp_shop_action(action);
                else if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                else
                    cp_interact();
                break;

            case CP_PLAY_ACTION:
                if (game.scene_type >= CP_SCENE_CART_TITLE)
                    cp_cart_action(action);
                break;

            default:
                if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    cp_write_save();
                    return PLUGIN_USB_CONNECTED;
                }
                break;
        }
    }

    return status;
}
