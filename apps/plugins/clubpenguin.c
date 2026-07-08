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
#include "clubpenguin_player_fallback.h"

#define CP_ASSET_DIR ROCKBOX_DIR "/rocks/games/clubpenguin"
#define CP_WORLD_FILE CP_ASSET_DIR "/world.bmp"
#define CP_PLAYER_FILE CP_ASSET_DIR "/player.bmp"
#define CP_WORLD_DATA_FILE CP_ASSET_DIR "/data/world.tsv"
#define CP_ROOMS_DATA_FILE CP_ASSET_DIR "/data/rooms.tsv"

#define CP_STATUS_H 20
#define CP_VIEW_W LCD_WIDTH
#define CP_VIEW_H (LCD_HEIGHT - CP_STATUS_H)
#define CP_WORLD_W 854
#define CP_WORLD_H 480
#define CP_PLAYER_W 40
#define CP_PLAYER_H 42
#define CP_PLAYER_FRAMES 16
#define CP_PLAYER_STRIP_W (CP_PLAYER_W * CP_PLAYER_FRAMES)
#define CP_PLAYER_STRIP_H CP_PLAYER_H
#define CP_STEP 4
#define CP_MESSAGE_TTL 70
#define CP_MAX_HOTSPOTS 16
#define CP_MAX_ROOMS 12
#define CP_TEXT_BUF 4096
#define CP_ANIM_RATE 3

/* read_bmp_file()/read_bmp_fd() need extra scratch space *inside* the
 * destination buffer whenever the source bitmap is wider than
 * BM_MAX_WIDTH (i.e. wider than the LCD). world.bmp (CP_WORLD_W) and the
 * player sprite strip (CP_PLAYER_STRIP_W) are both wider than the iPod's
 * LCD, so without this padding read_bmp_file() returns an error (-6) and
 * the asset is reported as "missing" even though the file loaded fine.
 * See apps/recorder/bmp.c around the "bm->width > BM_MAX_WIDTH" checks. */
#define CP_BMP_SCRATCH_ELEMS(w) \
    ((((w) * 4 + 8) + (int)sizeof(fb_data) - 1) / (int)sizeof(fb_data))

static const struct button_mapping *plugin_contexts[] = { pla_main_ctx };

#define CP_QUIT_ACTION PLA_EXIT
#define CP_CANCEL_ACTION PLA_CANCEL
#define CP_UP_ACTION PLA_UP
#define CP_DOWN_ACTION PLA_DOWN
#define CP_LEFT_ACTION PLA_LEFT
#define CP_RIGHT_ACTION PLA_RIGHT
#define CP_SELECT_ACTION PLA_SELECT
#define CP_UP_REPEAT PLA_UP_REPEAT
#define CP_DOWN_REPEAT PLA_DOWN_REPEAT
#define CP_LEFT_REPEAT PLA_LEFT_REPEAT
#define CP_RIGHT_REPEAT PLA_RIGHT_REPEAT

enum cp_scene_type
{
    CP_SCENE_MAP = 0,
    CP_SCENE_ROOM
};

enum cp_direction
{
    CP_DIR_DOWN = 0,
    CP_DIR_LEFT,
    CP_DIR_UP,
    CP_DIR_RIGHT
};

struct cp_hotspot
{
    char name[32];
    char detail[80];
    char target[24];
    int x;
    int y;
    int radius;
    int to_x;
    int to_y;
};

struct cp_room
{
    char id[24];
    char title[32];
    char bitmap[80];
    int start_x;
    int start_y;
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
};

struct cp_game
{
    struct bitmap scene;
    struct bitmap player;
    enum cp_scene_type scene_type;
    int room_index;
    int x;
    int y;
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
    int message_frames;
    int coins;
    unsigned int visited_mask;
    char location[32];
    char message[96];
    char error[96];
    struct cp_hotspot hotspots[CP_MAX_HOTSPOTS];
    struct cp_hotspot map_hotspots[CP_MAX_HOTSPOTS];
    struct cp_room rooms[CP_MAX_ROOMS];
    bool assets_loaded;
};

static fb_data scene_pixels[CP_WORLD_W * CP_WORLD_H
                            + CP_BMP_SCRATCH_ELEMS(CP_WORLD_W)];
static fb_data player_pixels[CP_PLAYER_STRIP_W * CP_PLAYER_STRIP_H
                             + CP_BMP_SCRATCH_ELEMS(CP_PLAYER_STRIP_W)];
static char text_buf[CP_TEXT_BUF];

static struct cp_game game;

static const struct cp_hotspot_def builtin_hotspots[] =
{
    {
        "My Place",
        "Source-scale preserved area. Offline membership is free.",
        "", 427, 240, 45, 427, 240
    },
    {
        "Town",
        "Town marker in the preserved offline area.",
        "", 585, 382, 48, 427, 240
    },
    {
        "Plaza",
        "Plaza marker in the preserved offline area.",
        "", 638, 292, 50, 427, 240
    },
    {
        "Dock",
        "Dock marker in the preserved offline area.",
        "", 112, 362, 50, 427, 240
    },
    {
        "Ski Village",
        "Ski Village marker in the preserved offline area.",
        "", 320, 257, 44, 427, 240
    },
    {
        "Dojo",
        "Dojo marker in the preserved offline area.",
        "", 494, 52, 44, 427, 240
    },
    {
        "Cove",
        "Cove marker in the preserved offline area.",
        "", 723, 161, 50, 427, 240
    },
    {
        "Beach",
        "Beach marker in the preserved offline area.",
        "", 126, 112, 42, 427, 240
    },
    {
        "Snow Forts",
        "Snow Forts marker in the preserved offline area.",
        "", 456, 352, 45, 427, 240
    },
    {
        "Forest",
        "Forest marker in the preserved offline area.",
        "", 694, 410, 44, 427, 240
    },
    {
        "Mine",
        "Mine marker in the preserved offline area.",
        "", 756, 332, 42, 427, 240
    },
    {
        "Iceberg",
        "Iceberg marker in the preserved offline area.",
        "", 760, 67, 42, 427, 240
    },
};

static const struct cp_room_def builtin_rooms[] =
{
    { "player_home", "My Place", "rooms/player_home.bmp", 160, 150 },
    { "town", "Town", "rooms/town.bmp", 160, 150 },
    { "plaza", "Plaza", "rooms/plaza.bmp", 160, 150 },
    { "dock", "Dock", "rooms/dock.bmp", 160, 150 },
    { "ski_village", "Ski Village", "rooms/ski_village.bmp", 160, 150 },
    { "dojo", "Dojo", "rooms/dojo.bmp", 160, 150 },
    { "cove", "Cove", "rooms/cove.bmp", 160, 150 },
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
    cp_copy(dst->name, sizeof(dst->name), src->name);
    cp_copy(dst->detail, sizeof(dst->detail), src->detail);
    cp_copy(dst->target, sizeof(dst->target), src->target);
    dst->x = src->x;
    dst->y = src->y;
    dst->radius = src->radius;
    dst->to_x = src->to_x;
    dst->to_y = src->to_y;
}

static void cp_copy_room(struct cp_room *dst, const struct cp_room_def *src)
{
    cp_copy(dst->id, sizeof(dst->id), src->id);
    cp_copy(dst->title, sizeof(dst->title), src->title);
    cp_copy(dst->bitmap, sizeof(dst->bitmap), src->bitmap);
    dst->start_x = src->start_x;
    dst->start_y = src->start_y;
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

static bool cp_load_scene(const char *path)
{
    return cp_load_bitmap(path, &game.scene, scene_pixels,
                          sizeof(scene_pixels), CP_WORLD_W, CP_WORLD_H,
                          FORMAT_NATIVE | FORMAT_DITHER);
}

static bool cp_load_assets(void)
{
    bool scene_ok;
    bool player_ok;

    scene_ok = cp_load_scene(CP_WORLD_FILE);
    player_ok = cp_load_bitmap(CP_PLAYER_FILE, &game.player, player_pixels,
                               sizeof(player_pixels), CP_PLAYER_STRIP_W,
                               CP_PLAYER_STRIP_H, FORMAT_NATIVE);
    if (!player_ok)
    {
        rb->memset(&game.player, 0, sizeof(game.player));
        game.player.data = (char *)cp_player_fallback_pixels;
        game.player.width = CP_PLAYER_STRIP_W;
        game.player.height = CP_PLAYER_STRIP_H;
        player_ok = true;
    }

    game.assets_loaded = scene_ok && player_ok;
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

static void cp_clamp_player(void)
{
    if (game.x < CP_PLAYER_W / 2)
        game.x = CP_PLAYER_W / 2;
    if (game.y < CP_PLAYER_H / 2)
        game.y = CP_PLAYER_H / 2;
    if (game.x > CP_WORLD_W - CP_PLAYER_W / 2)
        game.x = CP_WORLD_W - CP_PLAYER_W / 2;
    if (game.y > CP_WORLD_H - CP_PLAYER_H / 2)
        game.y = CP_WORLD_H - CP_PLAYER_H / 2;
}

static void cp_update_camera(void)
{
    game.cam_x = game.x - CP_VIEW_W / 2;
    game.cam_y = game.y - CP_VIEW_H / 2;

    if (game.cam_x < 0)
        game.cam_x = 0;
    if (game.cam_y < 0)
        game.cam_y = 0;
    if (game.cam_x > CP_WORLD_W - CP_VIEW_W)
        game.cam_x = CP_WORLD_W - CP_VIEW_W;
    if (game.cam_y > CP_WORLD_H - CP_VIEW_H)
        game.cam_y = CP_WORLD_H - CP_VIEW_H;
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
    if (!cp_load_scene(CP_WORLD_FILE))
        return false;

    game.scene_type = CP_SCENE_MAP;
    game.room_index = -1;
    cp_copy(game.location, sizeof(game.location), "Island Map");
    cp_set_current_hotspots(game.map_hotspots, game.map_hotspot_count);
    game.x = x;
    game.y = y;
    cp_clamp_player();
    cp_update_camera();
    game.selected_hotspot = cp_nearest_hotspot();
    return true;
}

static void cp_set_room_hotspots(void)
{
    struct cp_hotspot back;

    rb->memset(&back, 0, sizeof(back));
    cp_copy(back.name, sizeof(back.name), "Map");
    cp_copy(back.detail, sizeof(back.detail), "Return to the island map.");
    cp_copy(back.target, sizeof(back.target), "map");
    back.x = CP_PLAYER_W / 2 + 6;
    back.y = CP_WORLD_H - CP_PLAYER_H / 2 - 6;
    back.radius = 28;
    back.to_x = game.map_x;
    back.to_y = game.map_y;

    cp_set_current_hotspots(&back, 1);
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
    if (!cp_load_scene(path))
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
    cp_update_camera();
    cp_set_room_hotspots();
    game.selected_hotspot = cp_nearest_hotspot();
    return true;
}

static void cp_init(void)
{
    rb->memset(&game, 0, sizeof(game));
    game.coins = 500;
    game.direction = CP_DIR_DOWN;

    if (!cp_load_rooms())
        cp_use_builtin_rooms();

    if (!cp_load_hotspots())
        cp_use_builtin_hotspots();

    if (!cp_load_assets())
        return;

    game.map_x = game.map_hotspots[0].x;
    game.map_y = game.map_hotspots[0].y;
    cp_enter_map_at(game.map_x, game.map_y);
    cp_set_message("Loaded real Club Penguin assets.");
}

static void cp_move(int dx, int dy)
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
        game.anim_frame = (game.anim_frame + 1) & 3;
    }
    game.selected_hotspot = cp_nearest_hotspot();
}

static void cp_interact(void)
{
    int nearest = cp_nearest_hotspot();
    struct cp_hotspot *hotspot;
    int room_index;

    game.selected_hotspot = nearest;
    if (!cp_inside_hotspot(nearest))
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

    if (cp_streq(hotspot->target, "map"))
    {
        cp_enter_map_at(hotspot->to_x, hotspot->to_y);
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
}

static void cp_cancel_or_quit(enum plugin_status *status, bool *running)
{
    if (game.scene_type == CP_SCENE_ROOM)
    {
        cp_enter_map_at(game.map_x, game.map_y);
        cp_set_message("Returned to the island map.");
        return;
    }

    *running = false;
    *status = PLUGIN_OK;
}

static void cp_draw_status(void)
{
    char line[112];
    int old_fg = rb->lcd_get_foreground();
    int old_bg = rb->lcd_get_background();
    const char *spot = "";

    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_fillrect(0, CP_VIEW_H, LCD_WIDTH, CP_STATUS_H);

    if (game.selected_hotspot >= 0 &&
        game.selected_hotspot < game.hotspot_count)
        spot = game.hotspots[game.selected_hotspot].name;

    if (game.message_frames > 0)
        rb->snprintf(line, sizeof(line), "%s", game.message);
    else
        rb->snprintf(line, sizeof(line), "%s | %s | Free | %d coins",
                     game.location, spot, game.coins);

    rb->lcd_putsxy(2, CP_VIEW_H + 2, line);
    rb->lcd_putsxy(2, CP_VIEW_H + 11, "Select: inspect  Menu: back/quit");

    rb->lcd_set_foreground(old_fg);
    rb->lcd_set_background(old_bg);
}

static void cp_draw_room_markers(void)
{
    int i;
    int old_fg = rb->lcd_get_foreground();

    for (i = 0; i < game.hotspot_count; i++)
    {
        int x = game.hotspots[i].x - game.cam_x;
        int y = game.hotspots[i].y - game.cam_y;

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

static void cp_draw_player(int x, int y)
{
    int frame = game.direction * 4 + game.anim_frame;
    int src_x = frame * CP_PLAYER_W;
    int px;
    int py;
    int old_fg = rb->lcd_get_foreground();
    fb_data key = LCD_RGBPACK(255, 0, 255);
    const fb_data *data = (const fb_data *)game.player.data;

    for (py = 0; py < CP_PLAYER_H; py++)
    {
        int dy = y + py;

        if (dy < 0 || dy >= CP_VIEW_H)
            continue;

        for (px = 0; px < CP_PLAYER_W; px++)
        {
            int dx = x + px;
            fb_data color;

            if (dx < 0 || dx >= CP_VIEW_W)
                continue;

            color = data[py * CP_PLAYER_STRIP_W + src_x + px];
            if (color == key)
                continue;

            rb->lcd_set_foreground(color);
            rb->lcd_drawpixel(dx, dy);
        }
    }

    rb->lcd_set_foreground(old_fg);
}

static void cp_render(void)
{
    int player_screen_x;
    int player_screen_y;

    rb->lcd_setfont(FONT_UI);
    rb->lcd_bitmap_part((const fb_data *)game.scene.data,
                        game.cam_x, game.cam_y, game.scene.width,
                        0, 0, CP_VIEW_W, CP_VIEW_H);
    cp_draw_room_markers();
    player_screen_x = game.x - game.cam_x - CP_PLAYER_W / 2;
    player_screen_y = game.y - game.cam_y - CP_PLAYER_H / 2;
    cp_draw_player(player_screen_x, player_screen_y);
    cp_draw_status();
    rb->lcd_update();
}

static int cp_get_input(void)
{
    return pluginlib_getaction(HZ / 12, plugin_contexts,
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
        if (game.message_frames > 0)
            game.message_frames--;

        cp_render();
        action = cp_get_input();

        switch (action)
        {
            case CP_QUIT_ACTION:
            case CP_CANCEL_ACTION:
                cp_cancel_or_quit(&status, &running);
                break;

            case CP_UP_ACTION:
            case CP_UP_REPEAT:
                cp_move(0, -CP_STEP);
                break;

            case CP_DOWN_ACTION:
            case CP_DOWN_REPEAT:
                cp_move(0, CP_STEP);
                break;

            case CP_LEFT_ACTION:
            case CP_LEFT_REPEAT:
                cp_move(-CP_STEP, 0);
                break;

            case CP_RIGHT_ACTION:
            case CP_RIGHT_REPEAT:
                cp_move(CP_STEP, 0);
                break;

            case CP_SELECT_ACTION:
                cp_interact();
                break;

            default:
                if (rb->default_event_handler(action) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
                break;
        }
    }

    return status;
}
