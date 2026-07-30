/***************************************************************************
 * Portable deterministic core for Rockpod Maker Lite.
 ***************************************************************************/

#include "maker_lite.h"

#include <string.h>

#define ML_FP_INT(value) ((int32_t)(value) * ML_FIXED_ONE)
#define ML_FP_RATIO(numerator, denominator) \
    ((int32_t)(((int64_t)(numerator) * ML_FIXED_ONE) / (denominator)))
#define ML_ABS(value) ((value) < 0 ? -(value) : (value))
#define ML_MIN(a, b) ((a) < (b) ? (a) : (b))
#define ML_MAX(a, b) ((a) > (b) ? (a) : (b))
#define ML_CLAMP(value, low, high) ML_MIN(ML_MAX((value), (low)), (high))

static uint16_t ml_read_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int16_t ml_read_i16(const uint8_t *p)
{
    return (int16_t)ml_read_u16(p);
}

static uint32_t ml_read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool ml_valid_id(const uint8_t *id)
{
    unsigned i;
    bool found = false;

    for (i = 0; i < ML_ID_SIZE; ++i)
    {
        unsigned char ch = id[i];

        if (ch == '\0')
            break;
        if (!((ch >= 'a' && ch <= 'z') ||
              (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') ||
              ch == '-' || ch == '_'))
            return false;
        found = true;
    }
    return found;
}

uint32_t ml_crc32(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    uint32_t crc = 0xffffffffu;
    size_t i;

    for (i = 0; i < size; ++i)
    {
        unsigned bit;

        crc ^= bytes[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}

static bool ml_range_ok(size_t size, uint32_t offset, size_t length)
{
    return offset >= ML_PACK_HEADER_SIZE &&
           offset <= size &&
           length <= size - offset;
}

enum ml_pack_error ml_pack_open(const void *data, size_t size,
                                struct ml_level *level)
{
    const uint8_t *bytes = data;
    uint16_t version;
    uint32_t tiles_offset;
    uint32_t collision_offset;
    uint32_t entities_offset;
    uint32_t title_offset;
    uint32_t events_offset;
    uint32_t paths_offset;
    uint32_t path_points_offset;
    uint16_t title_length;
    uint32_t expected_crc;
    size_t map_cells;
    unsigned i;
    unsigned player_count = 0;
    unsigned opponent_count = 0;
    bool found_goal = false;

    if (!data || !level || size < ML_PACK_HEADER_SIZE)
        return ML_PACK_TOO_SMALL;
    if (memcmp(bytes, ML_PACK_MAGIC, 4) != 0)
        return ML_PACK_BAD_MAGIC;

    version = ml_read_u16(bytes + 4);
    if (version != 1 && version != 2 && version != ML_PACK_VERSION)
        return ML_PACK_BAD_VERSION;
    if (bytes[6] < ML_RULESET_MARIO || bytes[6] > ML_RULESET_SONIC)
        return ML_PACK_BAD_RULESET;

    memset(level, 0, sizeof(*level));
    level->data = bytes;
    level->size = size;
    level->ruleset = bytes[6];
    level->flags = bytes[7];
    level->view_width = ml_read_u16(bytes + 8);
    level->view_height = ml_read_u16(bytes + 10);
    level->map_width = ml_read_u16(bytes + 12);
    level->map_height = ml_read_u16(bytes + 14);
    level->tile_size = ml_read_u16(bytes + 16);
    level->entity_count = ml_read_u16(bytes + 18);
    level->entity_size =
        version == 1 ? ML_PACK_ENTITY_V1_SIZE : ML_PACK_ENTITY_SIZE;
    level->tile_entry_size = version >= 3 ? 2 : 1;

    if (level->view_width == 0 || level->view_height == 0 ||
        level->view_width > 320 || level->view_height > 240 ||
        level->map_width == 0 || level->map_height == 0 ||
        level->map_width > ML_MAX_MAP_WIDTH ||
        level->map_height > ML_MAX_MAP_HEIGHT ||
        level->tile_size != 16)
        return ML_PACK_BAD_DIMENSIONS;
    if ((level->flags & ~(ML_LEVEL_LIFE_SIM | ML_LEVEL_BRAWL)) != 0 ||
        (level->flags & ML_LEVEL_LIFE_SIM &&
         level->flags & ML_LEVEL_BRAWL) ||
        ((level->flags & ML_LEVEL_LIFE_SIM) &&
         level->ruleset != ML_RULESET_ZELDA))
        return ML_PACK_BAD_CONTENT;
    if (level->entity_count > ML_MAX_ENTITIES)
        return ML_PACK_TOO_MANY_ENTITIES;

    map_cells = (size_t)level->map_width * level->map_height;
    tiles_offset = ml_read_u32(bytes + 20);
    collision_offset = ml_read_u32(bytes + 24);
    entities_offset = ml_read_u32(bytes + 28);
    title_offset = ml_read_u32(bytes + 32);
    title_length = ml_read_u16(bytes + 36);
    expected_crc = ml_read_u32(bytes + 40);
    events_offset = ml_read_u32(bytes + 108);
    level->event_count = ml_read_u16(bytes + 112);
    level->path_count = ml_read_u16(bytes + 114);
    paths_offset = ml_read_u32(bytes + 116);
    path_points_offset = ml_read_u32(bytes + 120);
    level->path_point_count = ml_read_u16(bytes + 124);

    if (!ml_range_ok(size, tiles_offset,
                     map_cells * level->tile_entry_size) ||
        !ml_range_ok(size, collision_offset, map_cells) ||
        !ml_range_ok(size, entities_offset,
                     (size_t)level->entity_count * level->entity_size) ||
        title_length == 0 || title_length > ML_MAX_TITLE ||
        !ml_range_ok(size, title_offset, title_length))
        return ML_PACK_BAD_OFFSET;
    if (level->event_count > ML_MAX_EVENTS ||
        level->path_count > ML_MAX_PATHS ||
        level->path_point_count > ML_MAX_PATH_POINTS)
        return ML_PACK_BAD_OFFSET;
    if ((level->event_count &&
         !ml_range_ok(size, events_offset,
                      (size_t)level->event_count * ML_PACK_EVENT_SIZE)) ||
        (level->path_count &&
         !ml_range_ok(size, paths_offset,
                      (size_t)level->path_count * ML_PACK_PATH_SIZE)) ||
        (level->path_point_count &&
         !ml_range_ok(size, path_points_offset,
                      (size_t)level->path_point_count *
                          ML_PACK_PATH_POINT_SIZE)))
        return ML_PACK_BAD_OFFSET;
    if (ml_crc32(bytes + ML_PACK_HEADER_SIZE,
                 size - ML_PACK_HEADER_SIZE) != expected_crc)
        return ML_PACK_BAD_CRC;
    if (!ml_valid_id(bytes + 44) || !ml_valid_id(bytes + 76))
        return ML_PACK_BAD_ID;

    level->tiles = bytes + tiles_offset;
    level->collision = bytes + collision_offset;
    level->entity_data = bytes + entities_offset;
    level->event_data = level->event_count ? bytes + events_offset : NULL;
    level->path_data = level->path_count ? bytes + paths_offset : NULL;
    level->path_point_data = level->path_point_count ?
                             bytes + path_points_offset : NULL;
    level->title = (const char *)(bytes + title_offset);
    level->title_length = title_length;
    memcpy(level->kit_id, bytes + 44, ML_ID_SIZE);
    memcpy(level->project_id, bytes + 76, ML_ID_SIZE);
    level->kit_id[ML_ID_SIZE] = '\0';
    level->project_id[ML_ID_SIZE] = '\0';

    for (i = 0; i < level->entity_count; ++i)
    {
        struct ml_entity entity;

        if (!ml_level_entity(level, i, &entity))
            return ML_PACK_BAD_OFFSET;
        if (entity.kind < ML_ENTITY_PLAYER ||
            entity.kind > ML_ENTITY_DECORATION ||
            entity.x < 0 || entity.y < 0 ||
            entity.x >= (int)level->map_width * level->tile_size ||
            entity.y > (int)level->map_height * level->tile_size + 32)
            return ML_PACK_BAD_CONTENT;
        if (entity.kind == ML_ENTITY_PLAYER)
            player_count++;
        else if (entity.kind == ML_ENTITY_ENEMY)
            opponent_count++;
        else if (entity.kind == ML_ENTITY_GOAL)
            found_goal = true;
    }
    for (i = 0; i < level->event_count; ++i)
    {
        struct ml_event event;

        if (!ml_level_event(level, i, &event) ||
            event.trigger < ML_TRIGGER_ENTER_REGION ||
            event.trigger > ML_TRIGGER_ENEMY_GROUP_CLEAR ||
            event.condition > ML_CONDITION_ENEMY_GROUP_CLEAR ||
            event.action < ML_ACTION_OPEN_DOOR ||
            event.action > ML_ACTION_COMPLETE_LEVEL ||
            (event.flags & ~ML_EVENT_ONE_SHOT) != 0 ||
            event.subject > 255 ||
            (event.condition == ML_CONDITION_RINGS_GTE &&
             event.value < 0) ||
            (event.trigger == ML_TRIGGER_ENTER_REGION &&
             (event.x < 0 || event.y < 0 ||
              event.width <= 0 || event.height <= 0 ||
              (int)event.x + event.width >
                  (int)level->map_width * level->tile_size ||
              (int)event.y + event.height >
                  (int)level->map_height * level->tile_size)) ||
            (event.action == ML_ACTION_PLAY_EFFECT &&
             (event.param0 < 0 || event.param0 > 5)) ||
            (event.action == ML_ACTION_SET_CHECKPOINT &&
             (event.param0 < 0 || event.param1 < 0 ||
              event.param0 >=
                  (int)level->map_width * level->tile_size ||
              event.param1 >
                  (int)level->map_height * level->tile_size + 32)) ||
            (event.trigger == ML_TRIGGER_ENEMY_GROUP_CLEAR &&
             event.action == ML_ACTION_SPAWN_GROUP &&
             !(event.flags & ML_EVENT_ONE_SHOT) &&
             event.delay == 0))
            return ML_PACK_BAD_CONTENT;
    }
    for (i = 0; i < level->path_count; ++i)
    {
        struct ml_path path;

        unsigned j;

        if (!ml_level_path(level, i, &path) || path.id == 0 ||
            (path.flags & ~(ML_PATH_PING_PONG | ML_PATH_SURFACE)) != 0 ||
            path.speed < 1 || path.speed > 4096 ||
            path.point_count < 2 ||
            path.point_start > level->path_point_count ||
            path.point_count >
                level->path_point_count - path.point_start)
            return ML_PACK_BAD_CONTENT;
        for (j = 0; j < i; ++j)
        {
            struct ml_path previous;

            if (!ml_level_path(level, j, &previous) ||
                previous.id == path.id)
                return ML_PACK_BAD_CONTENT;
        }
        for (j = 0; j < path.point_count; ++j)
        {
            struct ml_path_point point;

            if (!ml_level_path_point(level, path.point_start + j, &point) ||
                point.x < 0 || point.y < 0 ||
                point.x >=
                    (int)level->map_width * level->tile_size ||
                point.y >
                    (int)level->map_height * level->tile_size + 32)
                return ML_PACK_BAD_CONTENT;
        }
    }
    for (i = 0; i < level->entity_count; ++i)
    {
        struct ml_entity entity;
        bool path_found = false;
        unsigned j;

        if (!ml_level_entity(level, i, &entity))
            return ML_PACK_BAD_OFFSET;
        if (entity.kind != ML_ENTITY_BLOCK || entity.param[3] == 0)
            continue;
        for (j = 0; j < level->path_count; ++j)
        {
            struct ml_path path;

            if (ml_level_path(level, j, &path) &&
                entity.param[3] == (int16_t)path.id)
            {
                path_found = true;
                break;
            }
        }
        if (!path_found)
            return ML_PACK_BAD_CONTENT;
    }
    if (player_count == 0)
        return ML_PACK_NO_PLAYER;
    if (player_count != 1)
        return ML_PACK_BAD_CONTENT;
    if ((level->flags & ML_LEVEL_BRAWL) && opponent_count != 1)
        return ML_PACK_BAD_CONTENT;
    if (!found_goal &&
        !(level->flags & (ML_LEVEL_LIFE_SIM | ML_LEVEL_BRAWL)))
        return ML_PACK_NO_GOAL;
    return ML_PACK_OK;
}

bool ml_level_event(const struct ml_level *level, unsigned index,
                    struct ml_event *event)
{
    const uint8_t *p;

    if (!level || !event || !level->event_data ||
        index >= level->event_count)
        return false;
    p = level->event_data + index * ML_PACK_EVENT_SIZE;
    memset(event, 0, sizeof(*event));
    event->trigger = p[0];
    event->condition = p[1];
    event->action = p[2];
    event->flags = p[3];
    event->subject = ml_read_u16(p + 4);
    event->value = ml_read_i16(p + 6);
    event->x = ml_read_i16(p + 8);
    event->y = ml_read_i16(p + 10);
    event->width = ml_read_i16(p + 12);
    event->height = ml_read_i16(p + 14);
    event->param0 = ml_read_i16(p + 16);
    event->param1 = ml_read_i16(p + 18);
    event->delay = ml_read_u16(p + 20);
    return true;
}

bool ml_level_path(const struct ml_level *level, unsigned index,
                   struct ml_path *path)
{
    const uint8_t *p;

    if (!level || !path || !level->path_data || index >= level->path_count)
        return false;
    p = level->path_data + index * ML_PACK_PATH_SIZE;
    memset(path, 0, sizeof(*path));
    path->id = ml_read_u16(p);
    path->flags = ml_read_u16(p + 2);
    path->point_start = ml_read_u16(p + 4);
    path->point_count = ml_read_u16(p + 6);
    path->speed = ml_read_i16(p + 8);
    return true;
}

bool ml_level_path_point(const struct ml_level *level, unsigned index,
                         struct ml_path_point *point)
{
    const uint8_t *p;

    if (!level || !point || !level->path_point_data ||
        index >= level->path_point_count)
        return false;
    p = level->path_point_data + index * ML_PACK_PATH_POINT_SIZE;
    point->x = ml_read_i16(p);
    point->y = ml_read_i16(p + 2);
    return true;
}

const char *ml_pack_error_string(enum ml_pack_error error)
{
    static const char * const messages[] = {
        "ok",
        "pack is too small",
        "invalid pack magic",
        "unsupported pack version",
        "unsupported ruleset",
        "invalid dimensions",
        "too many entities",
        "invalid pack offset",
        "pack checksum mismatch",
        "invalid kit or project id",
        "invalid pack content",
        "missing player spawn",
        "missing completion goal",
    };

    if ((unsigned)error >= sizeof(messages) / sizeof(messages[0]))
        return "unknown pack error";
    return messages[error];
}

bool ml_level_entity(const struct ml_level *level, unsigned index,
                     struct ml_entity *entity)
{
    const uint8_t *p;
    unsigned i;

    if (!level || !entity || index >= level->entity_count)
        return false;
    p = level->entity_data + index * level->entity_size;
    entity->kind = ml_read_u16(p);
    entity->flags = ml_read_u16(p + 2);
    entity->x = ml_read_i16(p + 4);
    entity->y = ml_read_i16(p + 6);
    for (i = 0; i < 4; ++i)
        entity->param[i] = ml_read_i16(p + 8 + i * 2);
    entity->render_cell = level->entity_size >= ML_PACK_ENTITY_SIZE ?
                          ml_read_u16(p + 16) : 0;
    return true;
}

bool ml_world_entity(const struct ml_world *world, unsigned index,
                     struct ml_entity *entity)
{
    if (!world || !entity || !ml_level_entity(world->level, index, entity))
        return false;
    entity->x = (int16_t)ml_fixed_to_int(world->entity_x[index]);
    entity->y = (int16_t)ml_fixed_to_int(world->entity_y[index]);
    return true;
}

uint16_t ml_level_tile(const struct ml_level *level, int tile_x, int tile_y)
{
    size_t index;

    if (!level || tile_x < 0 || tile_y < 0 ||
        tile_x >= level->map_width || tile_y >= level->map_height)
        return 0;
    index = (size_t)tile_y * level->map_width + tile_x;
    return level->tile_entry_size == 2 ?
           ml_read_u16(level->tiles + index * 2) :
           level->tiles[index];
}

uint8_t ml_level_collision(const struct ml_level *level,
                           int tile_x, int tile_y)
{
    if (!level || tile_x < 0 || tile_y < 0 ||
        tile_x >= level->map_width || tile_y >= level->map_height)
        return tile_y >= 0 ? ML_COLLISION_SOLID : 0;
    return level->collision[tile_y * level->map_width + tile_x];
}

int ml_fixed_to_int(int32_t value)
{
    return value >> ML_FIXED_SHIFT;
}

int32_t ml_int_to_fixed(int value)
{
    return (int32_t)value << ML_FIXED_SHIFT;
}

static bool ml_input_pressed(const struct ml_world *world, uint32_t bit)
{
    return (world->input & bit) && !(world->previous_input & bit);
}

static bool ml_input_released(const struct ml_world *world, uint32_t bit)
{
    return !(world->input & bit) && (world->previous_input & bit);
}

static bool ml_collision_blocks(uint8_t flags, bool moving_down,
                                int old_bottom, int tile_top)
{
    if (flags & ML_COLLISION_SOLID)
        return true;
    if ((flags & ML_COLLISION_ONE_WAY) && moving_down &&
        old_bottom <= tile_top)
        return true;
    return false;
}

static void ml_player_bounds(const struct ml_player *player,
                             int *left, int *top, int *right, int *bottom)
{
    int x = ml_fixed_to_int(player->x);
    int y = ml_fixed_to_int(player->y);

    *left = x - player->width / 2;
    *right = *left + player->width - 1;
    *top = y - player->height;
    *bottom = y - 1;
}

static bool ml_aabb_overlap(int left_a, int top_a, int right_a, int bottom_a,
                            int left_b, int top_b, int right_b, int bottom_b)
{
    return left_a <= right_b && right_a >= left_b &&
           top_a <= bottom_b && bottom_a >= top_b;
}

static bool ml_player_overlaps_entity(const struct ml_world *world,
                                      const struct ml_entity *entity,
                                      int width, int height)
{
    int left;
    int top;
    int right;
    int bottom;

    ml_player_bounds(&world->player, &left, &top, &right, &bottom);
    return ml_aabb_overlap(left, top, right, bottom,
                           entity->x - width / 2,
                           entity->y - height,
                           entity->x + (width - 1) / 2,
                           entity->y - 1);
}

static bool ml_sword_reaches_entity(const struct ml_world *world,
                                    const struct ml_entity *entity)
{
    int left;
    int top;
    int right;
    int bottom;
    int reach = 12;

    if (world->sword_ticks <= 0)
        return false;
    ml_player_bounds(&world->player, &left, &top, &right, &bottom);
    if (world->player.facing_x < 0)
        left -= reach;
    else if (world->player.facing_x > 0)
        right += reach;
    if (world->player.facing_y < 0)
        top -= reach;
    else if (world->player.facing_y > 0)
        bottom += reach;
    return ml_aabb_overlap(left, top, right, bottom,
                           entity->x - 8, entity->y - 16,
                           entity->x + 7, entity->y - 1);
}

static bool ml_position_blocked(const struct ml_world *world,
                                int32_t x, int32_t y,
                                bool moving_down, int old_bottom)
{
    int left = ml_fixed_to_int(x) - world->player.width / 2;
    int right = left + world->player.width - 1;
    int top = ml_fixed_to_int(y) - world->player.height;
    int bottom = ml_fixed_to_int(y) - 1;
    int tile_size = world->level->tile_size;
    int tx0 = left / tile_size;
    int tx1 = right / tile_size;
    int ty0 = top / tile_size;
    int ty1 = bottom / tile_size;
    int tx;
    int ty;

    for (ty = ty0; ty <= ty1; ++ty)
    {
        for (tx = tx0; tx <= tx1; ++tx)
        {
            uint8_t flags = ml_level_collision(world->level, tx, ty);

            if (ml_collision_blocks(flags, moving_down, old_bottom,
                                    ty * tile_size))
                return true;
        }
    }
    for (unsigned i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity entity;
        int width;
        int height;
        int entity_left;
        int entity_top;

        if (!world->entity_alive[i] ||
            !ml_world_entity(world, i, &entity) ||
            entity.kind != ML_ENTITY_BLOCK)
            continue;
        width = entity.param[0] > 0 ? entity.param[0] : tile_size;
        height = entity.param[1] > 0 ? entity.param[1] : tile_size;
        entity_left = entity.x - width / 2;
        entity_top = entity.y - height;
        if (ml_aabb_overlap(left, top, right, bottom,
                            entity_left, entity_top,
                            entity_left + width - 1, entity.y - 1))
            return true;
    }
    return false;
}

static bool ml_path_position(const struct ml_world *world,
                             const struct ml_path *path,
                             int32_t *x, int32_t *y)
{
    struct ml_path_point first;
    struct ml_path_point previous;
    uint64_t total = 0;
    uint64_t distance;
    unsigned point_index;

    if (!ml_level_path_point(world->level, path->point_start, &first))
        return false;
    previous = first;
    for (point_index = 1; point_index < path->point_count; ++point_index)
    {
        struct ml_path_point point;
        unsigned length;

        if (!ml_level_path_point(
                world->level, path->point_start + point_index, &point))
            return false;
        length = ML_MAX(ML_ABS(point.x - previous.x),
                        ML_ABS(point.y - previous.y));
        total += (uint64_t)ML_MAX(1, length) << 8;
        previous = point;
    }
    if (total == 0)
        return false;
    distance = (uint64_t)world->tick * (unsigned)ML_MAX(1, path->speed);
    if (path->flags & 1)
    {
        distance %= total * 2;
        if (distance > total)
            distance = total * 2 - distance;
    }
    else
        distance %= total;

    previous = first;
    for (point_index = 1; point_index < path->point_count; ++point_index)
    {
        struct ml_path_point point;
        uint64_t segment;

        if (!ml_level_path_point(
                world->level, path->point_start + point_index, &point))
            return false;
        segment = (uint64_t)ML_MAX(
            1, ML_MAX(ML_ABS(point.x - previous.x),
                      ML_ABS(point.y - previous.y))) << 8;
        if (distance <= segment)
        {
            *x = ml_int_to_fixed(previous.x) +
                 (int32_t)(((int64_t)(point.x - previous.x) *
                            ML_FIXED_ONE * (int64_t)distance) /
                           (int64_t)segment);
            *y = ml_int_to_fixed(previous.y) +
                 (int32_t)(((int64_t)(point.y - previous.y) *
                            ML_FIXED_ONE * (int64_t)distance) /
                           (int64_t)segment);
            return true;
        }
        distance -= segment;
        previous = point;
    }
    *x = ml_int_to_fixed(previous.x);
    *y = ml_int_to_fixed(previous.y);
    return true;
}

static void ml_update_paths(struct ml_world *world)
{
    unsigned path_index;

    for (path_index = 0; path_index < world->level->path_count; ++path_index)
    {
        struct ml_path path;
        struct ml_path_point first;
        int32_t path_x;
        int32_t path_y;
        unsigned entity_index;

        if (!ml_level_path(world->level, path_index, &path) ||
            !ml_level_path_point(world->level, path.point_start, &first) ||
            !ml_path_position(world, &path, &path_x, &path_y))
            continue;
        for (entity_index = 0;
             entity_index < world->level->entity_count; ++entity_index)
        {
            struct ml_entity entity;
            int32_t old_x;
            int32_t old_y;
            int32_t new_x;
            int32_t new_y;

            if (!world->entity_alive[entity_index] ||
                !ml_level_entity(world->level, entity_index, &entity) ||
                entity.param[3] != (int16_t)path.id)
                continue;
            old_x = world->entity_x[entity_index];
            old_y = world->entity_y[entity_index];
            new_x = path_x + ml_int_to_fixed(entity.x - first.x);
            new_y = path_y + ml_int_to_fixed(entity.y - first.y);
            world->entity_x[entity_index] = new_x;
            world->entity_y[entity_index] = new_y;
            if (entity.kind == ML_ENTITY_BLOCK)
            {
                int width = entity.param[0] > 0 ? entity.param[0] :
                            world->level->tile_size;
                int player_x = ml_fixed_to_int(world->player.x);
                int player_y = ml_fixed_to_int(world->player.y);
                int old_top = ml_fixed_to_int(old_y) -
                              (entity.param[1] > 0 ? entity.param[1] :
                               world->level->tile_size);
                int old_left = ml_fixed_to_int(old_x) - width / 2;

                if (player_y >= old_top - 2 && player_y <= old_top + 2 &&
                    player_x + world->player.width / 2 >= old_left &&
                    player_x - world->player.width / 2 <
                        old_left + width)
                {
                    world->player.x += new_x - old_x;
                    world->player.y += new_y - old_y;
                }
            }
        }
    }
}

static bool ml_entity_blocked(const struct ml_world *world,
                              int x, int y, int width, int height)
{
    int tile_size = world->level->tile_size;
    int left = x - width / 2;
    int right = left + width - 1;
    int top = y - height;
    int bottom = y - 1;
    int tile_x;
    int tile_y;

    for (tile_y = top / tile_size; tile_y <= bottom / tile_size; ++tile_y)
    {
        for (tile_x = left / tile_size; tile_x <= right / tile_size; ++tile_x)
        {
            if (ml_level_collision(world->level, tile_x, tile_y) &
                ML_COLLISION_SOLID)
                return true;
        }
    }
    return false;
}

static void ml_update_enemy_motion(struct ml_world *world)
{
    unsigned i;

    for (i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity entity;
        int width;
        int height;
        int current_x;
        int current_y;
        int proposed_x;

        if (!world->entity_alive[i] ||
            !ml_world_entity(world, i, &entity) ||
            entity.kind != ML_ENTITY_ENEMY || entity.param[3] != 0 ||
            (entity.flags & 0x800u))
            continue;
        width = entity.param[0] > 0 ? entity.param[0] : 16;
        height = entity.param[1] > 0 ? entity.param[1] : 16;
        current_x = entity.x;
        current_y = entity.y;
        if (world->level->ruleset == ML_RULESET_ZELDA)
        {
            int target_x = ml_fixed_to_int(world->player.x);
            int target_y = ml_fixed_to_int(world->player.y);
            int32_t speed = entity.param[2] > 0 ?
                ML_FP_RATIO(entity.param[2], 256) : ML_FP_RATIO(1, 2);
            int32_t step_x = target_x == current_x ? 0 :
                (target_x > current_x ? speed : -speed);
            int32_t step_y = target_y == current_y ? 0 :
                (target_y > current_y ? speed : -speed);
            int next_x = ml_fixed_to_int(world->entity_x[i] + step_x);
            int next_y = ml_fixed_to_int(world->entity_y[i] + step_y);

            if (!ml_entity_blocked(
                    world, next_x, current_y, width, height))
                world->entity_x[i] += step_x;
            if (!ml_entity_blocked(
                    world, ml_fixed_to_int(world->entity_x[i]), next_y,
                    width, height))
                world->entity_y[i] += step_y;
            continue;
        }

        proposed_x = ml_fixed_to_int(
            world->entity_x[i] + world->entity_vx[i]);
        {
            int tile_size = world->level->tile_size;
            int front_x = proposed_x +
                (world->entity_vx[i] > 0 ? width / 2 : -width / 2);
            int below_y = current_y + 1;
            uint8_t floor = ml_level_collision(
                world->level, front_x / tile_size, below_y / tile_size);

            if (ml_entity_blocked(
                    world, proposed_x, current_y, width, height) ||
                !(floor & (ML_COLLISION_SOLID |
                           ML_COLLISION_ONE_WAY |
                           ML_COLLISION_SLOPE_UP |
                           ML_COLLISION_SLOPE_DOWN)))
            {
                world->entity_vx[i] = -world->entity_vx[i];
                proposed_x = current_x;
            }
        }
        world->entity_x[i] = ml_int_to_fixed(proposed_x);
    }
}

static void ml_move_horizontal(struct ml_world *world)
{
    struct ml_player *player = &world->player;
    int32_t target = player->x + player->vx;
    int step = player->vx < 0 ? -ML_FIXED_ONE : ML_FIXED_ONE;
    int32_t cursor = player->x;

    while ((step > 0 && cursor + step < target) ||
           (step < 0 && cursor + step > target))
    {
        if (ml_position_blocked(world, cursor + step, player->y,
                                false, 0))
        {
            player->vx = 0;
            return;
        }
        cursor += step;
    }
    if (!ml_position_blocked(world, target, player->y, false, 0))
        player->x = target;
    else
        player->x = cursor;
    if (player->x != target)
        player->vx = 0;
}

static void ml_apply_slopes(struct ml_world *world)
{
    struct ml_player *player = &world->player;
    int tile_size = world->level->tile_size;
    int x = ml_fixed_to_int(player->x);
    int y = ml_fixed_to_int(player->y);
    int tile_x = x / tile_size;
    int tile_y = y / tile_size;
    uint8_t flags = ml_level_collision(world->level, tile_x, tile_y);
    int local_x = x - tile_x * tile_size;
    int surface;

    if (!(flags & (ML_COLLISION_SLOPE_UP | ML_COLLISION_SLOPE_DOWN)))
        return;
    if (flags & ML_COLLISION_SLOPE_UP)
        surface = tile_y * tile_size + tile_size - local_x;
    else
        surface = tile_y * tile_size + local_x + 1;
    if (y >= surface - 2 && y <= tile_y * tile_size + tile_size + 4 &&
        player->vy >= 0)
    {
        player->y = ml_int_to_fixed(surface);
        player->vy = 0;
        player->grounded = true;
    }
}

static void ml_move_vertical(struct ml_world *world)
{
    struct ml_player *player = &world->player;
    int old_bottom = ml_fixed_to_int(player->y) - 1;
    int32_t target = player->y + player->vy;
    int step = player->vy < 0 ? -ML_FIXED_ONE : ML_FIXED_ONE;
    int32_t cursor = player->y;
    bool moving_down = player->vy > 0;

    player->grounded = false;
    while ((step > 0 && cursor + step < target) ||
           (step < 0 && cursor + step > target))
    {
        if (ml_position_blocked(world, player->x, cursor + step,
                                moving_down, old_bottom))
        {
            if (moving_down)
                player->grounded = true;
            player->vy = 0;
            return;
        }
        cursor += step;
    }
    if (!ml_position_blocked(world, player->x, target,
                             moving_down, old_bottom))
        player->y = target;
    else
    {
        player->y = cursor;
        if (moving_down)
            player->grounded = true;
        player->vy = 0;
    }
    ml_apply_slopes(world);
}

static bool ml_player_in_collision(const struct ml_world *world,
                                   uint8_t collision)
{
    int left;
    int top;
    int right;
    int bottom;
    int tile_size = world->level->tile_size;
    int tx;
    int ty;

    ml_player_bounds(&world->player, &left, &top, &right, &bottom);
    for (ty = top / tile_size; ty <= bottom / tile_size; ++ty)
    {
        for (tx = left / tile_size; tx <= right / tile_size; ++tx)
        {
            if (ml_level_collision(world->level, tx, ty) & collision)
                return true;
        }
    }
    return false;
}

static bool ml_player_on_hazard(const struct ml_world *world)
{
    return ml_player_in_collision(world, ML_COLLISION_HAZARD);
}

static void ml_hurt_player(struct ml_world *world, int knockback)
{
    struct ml_player *player = &world->player;
    static const int8_t ring_velocity[ML_MAX_LOOSE_RINGS][2] = {
        { 3, -4 }, { 2, -5 }, { 1, -4 }, { 0, -5 },
        {-1, -4 }, {-2, -5 }, {-3, -4 }, {-2, -3 },
        { 3, -2 }, { 2, -3 }, { 1, -2 }, { 0, -3 },
        {-1, -2 }, {-2, -3 }, {-3, -2 }, { 2, -2 },
    };
    int i;

    if (player->invulnerable_ticks > 0 || world->complete)
        return;
    if (world->level->ruleset == ML_RULESET_SONIC && world->rings > 0)
    {
        world->loose_rings = ML_MIN(world->rings, 16);
        world->rings = 0;
        for (i = 0; i < world->loose_rings; ++i)
        {
            world->loose_ring_active[i] = 1;
            world->loose_ring_x[i] = player->x;
            world->loose_ring_y[i] = player->y - ML_FP_INT(8);
            world->loose_ring_vx[i] =
                ML_FP_RATIO(ring_velocity[i][0], 2);
            world->loose_ring_vy[i] =
                ML_FP_RATIO(ring_velocity[i][1], 2);
            world->loose_ring_ticks[i] = 240;
        }
    }
    else if (world->level->ruleset == ML_RULESET_MARIO &&
             player->power_state > 0)
        player->power_state--;
    else
        player->health--;
    player->invulnerable_ticks = 90;
    player->action = ML_ACTION_HURT;
    player->action_ticks = 20;
    player->vx = ml_int_to_fixed(knockback * -player->facing_x);
    player->vy = ML_FP_RATIO(-5, 2);
    if (player->health <= 0)
        ml_world_respawn(world);
}

static void ml_spawn_projectile(struct ml_world *world, unsigned kind,
                                int32_t vx, int32_t vy)
{
    world->projectile_active = 1;
    world->projectile_kind = (uint8_t)kind;
    world->projectile_x = world->player.x +
        world->player.facing_x * ML_FP_INT(10);
    world->projectile_y = world->player.y - ML_FP_INT(8);
    world->projectile_vx = vx;
    world->projectile_vy = vy;
    world->projectile_ticks = 180;
}

static void ml_update_projectile(struct ml_world *world)
{
    int x;
    int y;
    int tile_size;
    unsigned i;

    if (!world->projectile_active)
        return;
    world->projectile_x += world->projectile_vx;
    world->projectile_y += world->projectile_vy;
    if (world->projectile_kind == ML_ENTITY_ITEM)
        world->projectile_vy = ML_MIN(
            world->projectile_vy + ML_FP_RATIO(1, 8),
            ML_FP_INT(3));
    else
        world->projectile_vy = ML_MIN(
            world->projectile_vy + ML_FP_RATIO(3, 16),
            ML_FP_INT(5));
    x = ml_fixed_to_int(world->projectile_x);
    y = ml_fixed_to_int(world->projectile_y);
    tile_size = world->level->tile_size;
    if (--world->projectile_ticks == 0 || x < 0 || y < 0 ||
        x >= world->level->map_width * tile_size ||
        y >= world->level->map_height * tile_size + 32)
    {
        world->projectile_active = 0;
        return;
    }
    if (ml_level_collision(world->level, x / tile_size, y / tile_size) &
        ML_COLLISION_SOLID)
    {
        if (world->projectile_kind == ML_ENTITY_ITEM &&
            world->projectile_vy > 0)
        {
            world->projectile_y -= world->projectile_vy;
            world->projectile_vy = -ML_FP_INT(2);
        }
        else
        {
            world->projectile_active = 0;
            return;
        }
    }
    for (i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity entity;

        if (!world->entity_alive[i] ||
            !ml_world_entity(world, i, &entity) ||
            entity.kind != ML_ENTITY_ENEMY)
            continue;
        if (ML_ABS(entity.x - x) <= 12 && ML_ABS(entity.y - y) <= 16)
        {
            world->entity_alive[i] = 0;
            world->projectile_active = 0;
            world->score += world->projectile_kind == ML_ENTITY_POT ?
                            100 : 200;
            return;
        }
    }
}

static void ml_update_loose_rings(struct ml_world *world)
{
    int tile_size = world->level->tile_size;
    int remaining = 0;
    unsigned i;

    for (i = 0; i < ML_MAX_LOOSE_RINGS; ++i)
    {
        int x;
        int y;

        if (!world->loose_ring_active[i])
            continue;
        world->loose_ring_x[i] += world->loose_ring_vx[i];
        world->loose_ring_y[i] += world->loose_ring_vy[i];
        world->loose_ring_vy[i] = ML_MIN(
            world->loose_ring_vy[i] + ML_FP_RATIO(3, 16),
            ML_FP_INT(4));
        x = ml_fixed_to_int(world->loose_ring_x[i]);
        y = ml_fixed_to_int(world->loose_ring_y[i]);
        if (world->loose_ring_ticks[i] > 0)
            world->loose_ring_ticks[i]--;
        if (world->loose_ring_ticks[i] == 0 || x < 0 ||
            x >= world->level->map_width * tile_size ||
            y >= world->level->map_height * tile_size + 32)
        {
            world->loose_ring_active[i] = 0;
            continue;
        }
        if (world->loose_ring_vy[i] > 0 &&
            (ml_level_collision(
                world->level, x / tile_size, y / tile_size) &
             ML_COLLISION_SOLID))
        {
            world->loose_ring_y[i] -= world->loose_ring_vy[i];
            world->loose_ring_vy[i] =
                -ML_MAX(ML_FP_INT(1), world->loose_ring_vy[i] / 2);
        }
        remaining++;
    }
    world->loose_rings = (int16_t)remaining;
}

static void ml_update_entities(struct ml_world *world)
{
    unsigned i;

    for (i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity entity;

        if (!world->entity_alive[i] ||
            !ml_world_entity(world, i, &entity) ||
            entity.kind == ML_ENTITY_PLAYER)
            continue;
        if (!ml_player_overlaps_entity(world, &entity,
                                       entity.param[0] > 0 ?
                                           entity.param[0] : 16,
                                       entity.param[1] > 0 ?
                                           entity.param[1] : 16) &&
            !(world->level->ruleset == ML_RULESET_ZELDA &&
              (entity.kind == ML_ENTITY_ENEMY ||
               entity.kind == ML_ENTITY_POT) &&
              ml_sword_reaches_entity(world, &entity)))
            continue;

        switch (entity.kind)
        {
            case ML_ENTITY_GOAL:
                world->complete = true;
                if (world->best_ticks == 0 || world->tick < world->best_ticks)
                    world->best_ticks = world->tick;
                world->player.action = ML_ACTION_COMPLETE;
                world->player.vx = 0;
                world->player.vy = 0;
                break;
            case ML_ENTITY_COLLECTIBLE:
                world->entity_alive[i] = false;
                if (world->level->ruleset == ML_RULESET_SONIC)
                    world->rings++;
                else
                    world->collectibles++;
                world->score += entity.param[2] > 0 ?
                                entity.param[2] : 10;
                break;
            case ML_ENTITY_CHECKPOINT:
                world->checkpoint_x = ml_int_to_fixed(entity.x);
                world->checkpoint_y = ml_int_to_fixed(entity.y);
                break;
            case ML_ENTITY_KEY:
                world->entity_alive[i] = false;
                world->keys++;
                break;
            case ML_ENTITY_DOOR:
                if (world->room_transition_ticks == 0 &&
                    (!(entity.flags & 0x100u) || world->keys > 0))
                {
                    if ((entity.flags & 0x100u) && world->keys > 0)
                        world->keys--;
                    if (world->level->ruleset == ML_RULESET_ZELDA &&
                        (entity.param[2] || entity.param[3]))
                    {
                        world->player.x = ml_int_to_fixed(entity.param[2]);
                        world->player.y = ml_int_to_fixed(entity.param[3]);
                        world->player.vx = 0;
                        world->player.vy = 0;
                        world->room_transition_ticks = 18;
                    }
                    else
                        world->entity_alive[i] = false;
                }
                break;
            case ML_ENTITY_SWITCH:
                if (ml_input_pressed(world, ML_INPUT_PRIMARY))
                    world->switch_state ^= 1;
                break;
            case ML_ENTITY_SPRING:
                world->player.vy = entity.param[2] ?
                    -ml_int_to_fixed(entity.param[2]) : ML_FP_INT(-7);
                world->player.grounded = false;
                break;
            case ML_ENTITY_ENEMY:
                if (world->level->ruleset == ML_RULESET_ZELDA &&
                    world->sword_ticks > 0)
                {
                    world->entity_alive[i] = false;
                    world->score += 100;
                }
                else if (world->level->ruleset == ML_RULESET_MARIO &&
                         world->player.vy > 0)
                {
                    world->entity_alive[i] = false;
                    world->player.vy = ML_FP_RATIO(-7, 2);
                    world->score += 100;
                }
                else if (world->level->ruleset == ML_RULESET_SONIC &&
                         (world->player.rolling ||
                          world->player.action == ML_ACTION_JUMP))
                {
                    world->entity_alive[i] = false;
                    world->player.vy = ML_FP_INT(-3);
                    world->score += 100;
                }
                else if (world->level->ruleset == ML_RULESET_ZELDA &&
                         world->player.shield)
                {
                    world->player.vx = -world->player.facing_x *
                                       ML_FP_RATIO(1, 2);
                    world->player.vy = -world->player.facing_y *
                                       ML_FP_RATIO(1, 2);
                }
                else
                    ml_hurt_player(world, 2);
                break;
            case ML_ENTITY_ITEM:
                world->entity_alive[i] = false;
                if (world->level->ruleset == ML_RULESET_MARIO)
                    world->player.power_state =
                        ML_MIN(2, world->player.power_state + 1);
                else
                    world->selected_item = ML_MAX(1, entity.param[2]);
                break;
            case ML_ENTITY_POT:
                if (ml_input_pressed(world, ML_INPUT_SECONDARY) &&
                    !world->player.carrying)
                {
                    world->entity_alive[i] = false;
                    world->player.carrying = 1;
                    world->player.action = ML_ACTION_ITEM;
                }
                else if (world->sword_ticks > 0)
                {
                    world->entity_alive[i] = false;
                    world->score += 5;
                }
                break;
            default:
                break;
        }
    }
}

static bool ml_group_is_clear(const struct ml_world *world,
                              unsigned group)
{
    unsigned i;

    for (i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity entity;

        if (world->entity_alive[i] &&
            ml_level_entity(world->level, i, &entity) &&
            entity.kind == ML_ENTITY_ENEMY &&
            (entity.flags & 0xffu) == group)
            return false;
    }
    return true;
}

static bool ml_event_triggered(const struct ml_world *world,
                               const struct ml_event *event)
{
    int player_x = ml_fixed_to_int(world->player.x);
    int player_y = ml_fixed_to_int(world->player.y);

    switch (event->trigger)
    {
        case ML_TRIGGER_ENTER_REGION:
            return player_x >= event->x &&
                   player_y >= event->y &&
                   player_x < event->x + event->width &&
                   player_y < event->y + event->height;
        case ML_TRIGGER_SWITCH_ON:
            return world->switch_state != 0;
        case ML_TRIGGER_ENEMY_GROUP_CLEAR:
            return ml_group_is_clear(world, event->subject);
        default:
            return false;
    }
}

static bool ml_event_condition_met(const struct ml_world *world,
                                   const struct ml_event *event)
{
    switch (event->condition)
    {
        case ML_CONDITION_ALWAYS:
            return true;
        case ML_CONDITION_HAS_KEY:
            return world->keys >= ML_MAX(1, event->value);
        case ML_CONDITION_RINGS_GTE:
            return world->rings >= event->value;
        case ML_CONDITION_SWITCH_ON:
            return world->switch_state != 0;
        case ML_CONDITION_ENEMY_GROUP_CLEAR:
            return ml_group_is_clear(world, event->subject);
        default:
            return false;
    }
}

static void ml_set_group_alive(struct ml_world *world, unsigned group,
                               uint16_t kind, int mode)
{
    unsigned i;

    for (i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity entity;

        if (!ml_level_entity(world->level, i, &entity) ||
            entity.kind != kind || (entity.flags & 0xffu) != group)
            continue;
        if (mode < 0)
            world->entity_alive[i] = !world->entity_alive[i];
        else
            world->entity_alive[i] = mode != 0;
    }
}

static void ml_apply_event(struct ml_world *world,
                           const struct ml_event *event)
{
    switch (event->action)
    {
        case ML_ACTION_OPEN_DOOR:
            ml_set_group_alive(world, event->subject,
                               ML_ENTITY_DOOR, 0);
            break;
        case ML_ACTION_TOGGLE_BLOCK_GROUP:
            ml_set_group_alive(world, event->subject,
                               ML_ENTITY_BLOCK, -1);
            break;
        case ML_ACTION_SPAWN_GROUP:
        {
            unsigned i;

            for (i = 0; i < world->level->entity_count; ++i)
            {
                struct ml_entity entity;

                if (ml_level_entity(world->level, i, &entity) &&
                    entity.kind != ML_ENTITY_PLAYER &&
                    (entity.flags & 0xffu) == event->subject)
                    world->entity_alive[i] = true;
            }
            break;
        }
        case ML_ACTION_PLAY_EFFECT:
            world->pending_effect = ML_CLAMP(event->param0, 0, 5);
            break;
        case ML_ACTION_SET_CHECKPOINT:
            world->checkpoint_x = ml_int_to_fixed(event->param0);
            world->checkpoint_y = ml_int_to_fixed(event->param1);
            break;
        case ML_ACTION_COMPLETE_LEVEL:
            world->complete = true;
            if (world->best_ticks == 0 || world->tick < world->best_ticks)
                world->best_ticks = world->tick;
            world->player.action = ML_ACTION_COMPLETE;
            world->player.vx = 0;
            world->player.vy = 0;
            break;
        default:
            break;
    }
}

static void ml_update_events(struct ml_world *world)
{
    unsigned i;

    for (i = 0; i < world->level->event_count; ++i)
    {
        struct ml_event event;
        unsigned byte = i >> 3;
        unsigned mask = 1u << (i & 7);

        if (!ml_level_event(world->level, i, &event) ||
            ((event.flags & ML_EVENT_ONE_SHOT) &&
             (world->event_fired[byte] & mask)))
            continue;
        if (!ml_event_triggered(world, &event) ||
            !ml_event_condition_met(world, &event))
        {
            if (!(event.flags & ML_EVENT_ONE_SHOT))
                world->event_delay[i] = 0;
            continue;
        }
        if (world->event_delay[i] > 0)
        {
            world->event_delay[i]--;
            continue;
        }
        ml_apply_event(world, &event);
        if (event.flags & ML_EVENT_ONE_SHOT)
            world->event_fired[byte] |= mask;
        else
            world->event_delay[i] = event.delay;
    }
}

static void ml_tick_mario(struct ml_world *world)
{
    struct ml_player *player = &world->player;
    int direction = !!(world->input & ML_INPUT_RIGHT) -
                    !!(world->input & ML_INPUT_LEFT);
    int32_t maximum = world->input & ML_INPUT_SECONDARY ?
                      ML_FP_RATIO(5, 2) : ML_FP_RATIO(3, 2);
    int32_t acceleration = player->grounded ?
                           ML_FP_RATIO(1, 8) : ML_FP_RATIO(1, 16);
    bool climbing = ml_player_in_collision(world, ML_COLLISION_CLIMB);

    if (player->carrying &&
        ml_input_released(world, ML_INPUT_SECONDARY))
    {
        ml_spawn_projectile(
            world, ML_ENTITY_POT,
            player->facing_x * ML_FP_INT(3), ML_FP_INT(-2));
        player->carrying = 0;
        player->action = ML_ACTION_ITEM;
    }
    else if (!player->carrying && player->power_state >= 2 &&
             ml_input_pressed(world, ML_INPUT_SECONDARY))
    {
        ml_spawn_projectile(
            world, ML_ENTITY_ITEM,
            player->facing_x * ML_FP_INT(3), ML_FP_INT(-1));
        player->action = ML_ACTION_ITEM;
    }

    if (direction)
    {
        player->facing_x = direction;
        player->vx += direction * acceleration;
        player->vx = ML_CLAMP(player->vx, -maximum, maximum);
        player->action = world->input & ML_INPUT_SECONDARY ?
                         ML_ACTION_RUN : ML_ACTION_WALK;
    }
    else if (player->grounded)
    {
        int32_t friction = ML_FP_RATIO(3, 16);

        if (ML_ABS(player->vx) <= friction)
            player->vx = 0;
        else
            player->vx += player->vx > 0 ? -friction : friction;
        player->action = player->vx ? ML_ACTION_SKID : ML_ACTION_IDLE;
    }

    if (player->grounded && (world->input & ML_INPUT_DOWN))
    {
        player->action = ML_ACTION_CROUCH;
        player->vx /= 2;
    }
    if (ml_input_pressed(world, ML_INPUT_PRIMARY) && player->grounded)
    {
        player->vy = world->input & ML_INPUT_SECONDARY ?
                     ML_FP_RATIO(-21, 4) : ML_FP_RATIO(-9, 2);
        player->grounded = false;
        player->action = ML_ACTION_JUMP;
    }
    if (ml_input_released(world, ML_INPUT_PRIMARY) &&
        player->vy < ML_FP_INT(-2))
        player->vy = ML_FP_INT(-2);

    player->in_water = ml_player_in_collision(world, ML_COLLISION_WATER);
    if (climbing && (world->input & (ML_INPUT_UP | ML_INPUT_DOWN)))
    {
        int climb_direction = !!(world->input & ML_INPUT_DOWN) -
                              !!(world->input & ML_INPUT_UP);

        player->vy = climb_direction * ML_FP_INT(1);
        player->vx = direction * ML_FP_RATIO(1, 2);
        player->action = ML_ACTION_SWIM;
    }
    else if (player->in_water)
    {
        if (ml_input_pressed(world, ML_INPUT_PRIMARY))
            player->vy = ML_FP_RATIO(-5, 2);
        player->vx = ML_CLAMP(player->vx, -ML_FP_RATIO(3, 2),
                             ML_FP_RATIO(3, 2));
        player->vy += ML_FP_RATIO(3, 32);
        player->vy = ML_MIN(player->vy, ML_FP_INT(2));
        player->action = ML_ACTION_SWIM;
    }
    else
    {
        player->vy += ML_FP_RATIO(7, 32);
        player->vy = ML_MIN(player->vy, ML_FP_INT(5));
    }
    ml_move_horizontal(world);
    ml_move_vertical(world);
    if (!player->grounded && !player->in_water && !climbing)
        player->action = player->vy < 0 ? ML_ACTION_JUMP : ML_ACTION_FALL;
}

static bool ml_block_position_clear(const struct ml_world *world,
                                    unsigned self, int x, int y,
                                    int width, int height)
{
    int tile_size = world->level->tile_size;
    int left = x - width / 2;
    int right = left + width - 1;
    int top = y - height;
    int bottom = y - 1;
    int tx;
    int ty;
    unsigned i;

    for (ty = top / tile_size; ty <= bottom / tile_size; ++ty)
    {
        for (tx = left / tile_size; tx <= right / tile_size; ++tx)
        {
            if (ml_level_collision(world->level, tx, ty) &
                ML_COLLISION_SOLID)
                return false;
        }
    }
    for (i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity other;
        int other_width;
        int other_height;

        if (i == self || !world->entity_alive[i] ||
            !ml_world_entity(world, i, &other) ||
            other.kind != ML_ENTITY_BLOCK)
            continue;
        other_width = other.param[0] > 0 ? other.param[0] : tile_size;
        other_height = other.param[1] > 0 ? other.param[1] : tile_size;
        if (ml_aabb_overlap(
                left, top, right, bottom,
                other.x - other_width / 2, other.y - other_height,
                other.x + (other_width - 1) / 2, other.y - 1))
            return false;
    }
    return true;
}

static bool ml_push_zelda_block(struct ml_world *world, int dx, int dy)
{
    struct ml_player *player = &world->player;
    int target_x = ml_fixed_to_int(player->x + player->vx);
    int target_y = ml_fixed_to_int(player->y + player->vy);
    int left = target_x - player->width / 2;
    int right = left + player->width - 1;
    int top = target_y - player->height;
    int bottom = target_y - 1;
    unsigned i;

    for (i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity entity;
        int width;
        int height;

        if (!world->entity_alive[i] ||
            !ml_world_entity(world, i, &entity) ||
            entity.kind != ML_ENTITY_BLOCK || entity.param[3] != 0)
        {
            world->block_push_ticks[i] = 0;
            continue;
        }
        width = entity.param[0] > 0 ? entity.param[0] :
                world->level->tile_size;
        height = entity.param[1] > 0 ? entity.param[1] :
                 world->level->tile_size;
        if (!ml_aabb_overlap(
                left, top, right, bottom,
                entity.x - width / 2, entity.y - height,
                entity.x + (width - 1) / 2, entity.y - 1))
        {
            world->block_push_ticks[i] = 0;
            continue;
        }
        if (world->block_push_ticks[i] < 255)
            world->block_push_ticks[i]++;
        player->vx = 0;
        player->vy = 0;
        if (world->block_push_ticks[i] >= 12)
        {
            int push_x = dx ? (dx > 0 ? 1 : -1) : 0;
            int push_y = !push_x && dy ? (dy > 0 ? 1 : -1) : 0;
            int new_x = entity.x + push_x * world->level->tile_size;
            int new_y = entity.y + push_y * world->level->tile_size;

            if ((push_x || push_y) &&
                ml_block_position_clear(world, i, new_x, new_y,
                                        width, height))
            {
                world->entity_x[i] = ml_int_to_fixed(new_x);
                world->entity_y[i] = ml_int_to_fixed(new_y);
            }
            world->block_push_ticks[i] = 0;
        }
        return true;
    }
    return false;
}

static void ml_life_notice(struct ml_world *world, uint8_t notice,
                           bool success)
{
    world->interaction_notice = notice;
    world->interaction_notice_ticks = 120;
    world->pending_effect = success ? 1 : 2;
}

static void ml_life_add_credits(struct ml_world *world, uint32_t amount)
{
    if (UINT32_MAX - world->credits < amount)
        world->credits = UINT32_MAX;
    else
        world->credits += amount;
}

static void ml_life_close_interaction(struct ml_world *world)
{
    world->interaction = ML_LIFE_INTERACTION_NONE;
    world->interaction_choice = 0;
    world->interaction_entity = UINT16_MAX;
}

static unsigned ml_life_choice_count(uint8_t interaction)
{
    switch (interaction)
    {
        case ML_LIFE_INTERACTION_HOUSE:
            return 4;
        case ML_LIFE_INTERACTION_SHOP:
            return 5;
        case ML_LIFE_INTERACTION_NPC:
            return 2;
        case ML_LIFE_INTERACTION_FURNITURE:
            return 3;
        default:
            return 1;
    }
}

static void ml_life_confirm(struct ml_world *world)
{
    unsigned choice = world->interaction_choice;

    switch (world->interaction)
    {
        case ML_LIFE_INTERACTION_HOUSE:
            if (choice <= 1)
            {
                uint32_t amount = choice == 0 ? 100 : world->debt;

                amount = ML_MIN(amount, world->credits);
                amount = ML_MIN(amount, world->debt);
                if (amount == 0)
                    ml_life_notice(world, ML_LIFE_NOTICE_NO_CREDITS, false);
                else
                {
                    world->credits -= amount;
                    world->debt -= amount;
                    ml_life_notice(world, ML_LIFE_NOTICE_PAID, true);
                }
            }
            else if (choice == 2)
            {
                static const uint32_t debts[] = {
                    1500, 3500, 7000,
                };

                if (world->debt != 0)
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_DEBT_REMAINS, false);
                else if (world->house_level >=
                         sizeof(debts) / sizeof(debts[0]))
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_MAX_HOUSE, false);
                else
                {
                    world->debt = debts[world->house_level];
                    world->house_level++;
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_UPGRADED_HOUSE, true);
                }
            }
            else
            {
                world->house_style = (world->house_style + 1) % 3;
                ml_life_notice(
                    world, ML_LIFE_NOTICE_STYLED_HOUSE, true);
            }
            break;
        case ML_LIFE_INTERACTION_SHOP:
            if (choice <= 1)
            {
                unsigned count = choice == 0 ? 1 :
                    (unsigned)ML_MAX(0, world->collectibles);

                count = ML_MIN(
                    count, (unsigned)ML_MAX(0, world->collectibles));
                if (count == 0)
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_NO_SALVAGE, false);
                else
                {
                    world->collectibles -= (int16_t)count;
                    ml_life_add_credits(world, count * 50u);
                    ml_life_notice(world, ML_LIFE_NOTICE_SOLD, true);
                }
            }
            else if (choice == 2)
            {
                uint32_t cost = 300u * (world->car_level + 1u);

                if (world->car_level >= 3)
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_MAX_CAR, false);
                else if (world->credits < cost)
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_NO_CREDITS, false);
                else
                {
                    world->credits -= cost;
                    world->car_level++;
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_UPGRADED_CAR, true);
                }
            }
            else if (choice == 3)
            {
                unsigned index = world->furniture_held_entity;

                if (index >= world->level->entity_count)
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_NO_SALVAGE, false);
                else
                {
                    struct ml_entity furniture;

                    if (!ml_level_entity(
                            world->level, index, &furniture) ||
                        furniture.kind != ML_ENTITY_FURNITURE)
                        ml_life_notice(
                            world, ML_LIFE_NOTICE_NO_SALVAGE, false);
                    else
                    {
                        ml_life_add_credits(
                            world,
                            furniture.param[2] > 0 ?
                                (uint32_t)furniture.param[2] : 75u);
                        world->furniture_held_entity = UINT16_MAX;
                        world->player.carrying = 0;
                        ml_life_notice(
                            world, ML_LIFE_NOTICE_FURNITURE_SOLD, true);
                    }
                }
            }
            else
                ml_life_close_interaction(world);
            break;
        case ML_LIFE_INTERACTION_NPC:
            if (choice == 0)
            {
                if (world->job_cooldown != 0)
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_JOB_COOLDOWN, false);
                else
                {
                    ml_life_add_credits(world, 25);
                    world->job_cooldown = 600;
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_JOB_PAID, true);
                }
            }
            else
                ml_life_close_interaction(world);
            break;
        case ML_LIFE_INTERACTION_FURNITURE:
            if (world->interaction_entity >= world->level->entity_count)
                ml_life_close_interaction(world);
            else if (choice == 0)
            {
                world->furniture_held_entity =
                    world->interaction_entity;
                world->entity_alive[world->interaction_entity] = false;
                world->player.carrying = 1;
                ml_life_close_interaction(world);
                ml_life_notice(
                    world, ML_LIFE_NOTICE_FURNITURE_PICKED, true);
            }
            else if (choice == 1)
            {
                struct ml_entity furniture;

                if (!ml_level_entity(
                        world->level, world->interaction_entity,
                        &furniture))
                    ml_life_close_interaction(world);
                else
                {
                    world->entity_alive[world->interaction_entity] = false;
                    ml_life_add_credits(
                        world,
                        furniture.param[2] > 0 ?
                            (uint32_t)furniture.param[2] : 75u);
                    ml_life_close_interaction(world);
                    ml_life_notice(
                        world, ML_LIFE_NOTICE_FURNITURE_SOLD, true);
                }
            }
            else
                ml_life_close_interaction(world);
            break;
        default:
            ml_life_close_interaction(world);
            break;
    }
}

static bool ml_life_entity_near(
    const struct ml_world *world, const struct ml_entity *entity)
{
    int player_x = ml_fixed_to_int(world->player.x);
    int player_y = ml_fixed_to_int(world->player.y);

    return ML_ABS(entity->x - player_x) <= 24 &&
           ML_ABS(entity->y - player_y) <= 28;
}

static bool ml_life_begin_interaction(struct ml_world *world)
{
    int nearest = 0x7fffffff;
    unsigned nearest_index = UINT16_MAX;
    struct ml_entity nearest_entity;
    unsigned i;

    for (i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity entity;
        int distance;

        if (!world->entity_alive[i] ||
            !ml_world_entity(world, i, &entity) ||
            (entity.kind != ML_ENTITY_HOUSE &&
             entity.kind != ML_ENTITY_SHOP &&
             entity.kind != ML_ENTITY_NPC &&
             entity.kind != ML_ENTITY_CAR &&
             entity.kind != ML_ENTITY_FURNITURE) ||
            !ml_life_entity_near(world, &entity))
            continue;
        distance =
            ML_ABS(entity.x - ml_fixed_to_int(world->player.x)) +
            ML_ABS(entity.y - ml_fixed_to_int(world->player.y));
        if (distance < nearest)
        {
            nearest = distance;
            nearest_index = i;
            nearest_entity = entity;
        }
    }
    if (nearest_index == UINT16_MAX)
        return false;
    world->interaction_entity = (uint16_t)nearest_index;
    world->interaction_choice = 0;
    if (nearest_entity.kind == ML_ENTITY_CAR)
    {
        world->vehicle_entity = (uint16_t)nearest_index;
        world->entity_alive[nearest_index] = false;
        world->car_active = true;
        world->pending_effect = 4;
        return true;
    }
    world->interaction =
        nearest_entity.kind == ML_ENTITY_HOUSE ?
            ML_LIFE_INTERACTION_HOUSE :
        nearest_entity.kind == ML_ENTITY_SHOP ?
            ML_LIFE_INTERACTION_SHOP :
        nearest_entity.kind == ML_ENTITY_NPC ?
            ML_LIFE_INTERACTION_NPC :
            ML_LIFE_INTERACTION_FURNITURE;
    return true;
}

static bool ml_life_furniture_position_clear(
    const struct ml_world *world, unsigned held, int x, int y,
    int width, int height)
{
    unsigned i;
    int map_width = world->level->map_width * world->level->tile_size;
    int map_height = world->level->map_height * world->level->tile_size;
    int left = x - width / 2;
    int right = left + width - 1;
    int top = y - height;
    int bottom = y - 1;

    if (left < 0 || top < 0 || right >= map_width ||
        bottom >= map_height ||
        ml_entity_blocked(world, x, y, width, height))
        return false;
    for (i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity other;
        int other_width;
        int other_height;

        if (i == held || !world->entity_alive[i] ||
            !ml_world_entity(world, i, &other) ||
            other.kind != ML_ENTITY_FURNITURE)
            continue;
        other_width = other.param[0] > 0 ? other.param[0] : 14;
        other_height = other.param[1] > 0 ? other.param[1] : 14;
        if (ml_aabb_overlap(
                x - width / 2, y - height,
                x + (width - 1) / 2, y - 1,
                other.x - other_width / 2, other.y - other_height,
                other.x + (other_width - 1) / 2, other.y - 1))
            return false;
    }
    return true;
}

static bool ml_life_place_furniture(struct ml_world *world)
{
    unsigned index = world->furniture_held_entity;
    struct ml_entity furniture;
    int player_x;
    int player_y;
    int target_x;
    int target_y;
    int width;
    int height;

    if (index >= world->level->entity_count ||
        !ml_level_entity(world->level, index, &furniture) ||
        furniture.kind != ML_ENTITY_FURNITURE)
        return false;
    player_x = ml_fixed_to_int(world->player.x);
    player_y = ml_fixed_to_int(world->player.y);
    target_x = player_x + world->player.facing_x * 20;
    target_y = player_y + world->player.facing_y * 20;
    width = furniture.param[0] > 0 ? furniture.param[0] : 14;
    height = furniture.param[1] > 0 ? furniture.param[1] : 14;
    if (!ml_life_furniture_position_clear(
            world, index, target_x, target_y, width, height))
    {
        ml_life_notice(world, ML_LIFE_NOTICE_FURNITURE_BLOCKED, false);
        return false;
    }
    world->entity_x[index] = ml_int_to_fixed(target_x);
    world->entity_y[index] = ml_int_to_fixed(target_y);
    world->entity_alive[index] = true;
    world->furniture_held_entity = UINT16_MAX;
    world->player.carrying = 0;
    ml_life_notice(world, ML_LIFE_NOTICE_FURNITURE_PLACED, true);
    return true;
}

static void ml_life_exit_car(struct ml_world *world)
{
    unsigned index = world->vehicle_entity;

    if (index < world->level->entity_count)
    {
        world->entity_x[index] = world->player.x;
        world->entity_y[index] = world->player.y;
        world->entity_alive[index] = true;
    }
    world->car_active = false;
    world->pending_effect = 4;
}

static void ml_tick_life_sim(struct ml_world *world)
{
    struct ml_player *player = &world->player;
    int dx;
    int dy;
    int32_t speed;

    if (world->interaction != ML_LIFE_INTERACTION_NONE)
    {
        unsigned choices = ml_life_choice_count(world->interaction);

        player->vx = 0;
        player->vy = 0;
        player->action = ML_ACTION_IDLE;
        if (ml_input_pressed(world, ML_INPUT_PREVIOUS))
            world->interaction_choice =
                (world->interaction_choice + choices - 1) % choices;
        if (ml_input_pressed(world, ML_INPUT_NEXT))
            world->interaction_choice =
                (world->interaction_choice + 1) % choices;
        if (ml_input_pressed(world, ML_INPUT_SECONDARY))
            ml_life_close_interaction(world);
        else if (ml_input_pressed(world, ML_INPUT_PRIMARY))
            ml_life_confirm(world);
        return;
    }

    dx = !!(world->input & ML_INPUT_RIGHT) -
         !!(world->input & ML_INPUT_LEFT);
    dy = !!(world->input & ML_INPUT_DOWN) -
         !!(world->input & ML_INPUT_UP);
    speed = world->car_active ?
        ML_FP_RATIO(5 + world->car_level, 2) :
        ML_FP_RATIO(3, 2);
    if (dx && dy)
        speed = speed * 3 / 4;
    player->vx = dx * speed;
    player->vy = dy * speed;
    if (dx || dy)
    {
        player->facing_x = dx;
        player->facing_y = dy;
        player->action = world->car_active ?
            ML_ACTION_RUN : ML_ACTION_WALK;
    }
    else
        player->action = ML_ACTION_IDLE;

    if (ml_input_pressed(world, ML_INPUT_PRIMARY))
    {
        if (world->car_active)
            ml_life_exit_car(world);
        else if (world->furniture_held_entity <
                 world->level->entity_count)
            (void)ml_life_place_furniture(world);
        else if (ml_life_begin_interaction(world))
            return;
    }
    ml_move_horizontal(world);
    ml_move_vertical(world);
    player->grounded = true;
}

static void ml_update_life_npcs(struct ml_world *world)
{
    unsigned i;

    for (i = 0; i < world->level->entity_count; ++i)
    {
        struct ml_entity entity;
        int phase;
        int32_t speed;
        int32_t step_x = 0;
        int32_t step_y = 0;
        int next_x;
        int next_y;

        if (!world->entity_alive[i] ||
            !ml_world_entity(world, i, &entity) ||
            entity.kind != ML_ENTITY_NPC ||
            (world->interaction == ML_LIFE_INTERACTION_NPC &&
             world->interaction_entity == i))
            continue;
        phase = ((world->tick / 90) + i + (entity.flags & 3u)) & 3u;
        speed = entity.param[2] > 0 ?
            ML_FP_RATIO(entity.param[2], 256) : ML_FP_RATIO(1, 4);
        if (phase == 0)
            step_x = speed;
        else if (phase == 1)
            step_y = speed;
        else if (phase == 2)
            step_x = -speed;
        else
            step_y = -speed;
        next_x = ml_fixed_to_int(world->entity_x[i] + step_x);
        next_y = ml_fixed_to_int(world->entity_y[i] + step_y);
        if (!ml_entity_blocked(
                world, next_x, next_y,
                entity.param[0] > 0 ? entity.param[0] : 14,
                entity.param[1] > 0 ? entity.param[1] : 16))
        {
            world->entity_x[i] += step_x;
            world->entity_y[i] += step_y;
        }
    }
}

static void ml_tick_zelda(struct ml_world *world)
{
    struct ml_player *player = &world->player;
    int dx = !!(world->input & ML_INPUT_RIGHT) -
             !!(world->input & ML_INPUT_LEFT);
    int dy = !!(world->input & ML_INPUT_DOWN) -
             !!(world->input & ML_INPUT_UP);
    int32_t speed = ML_FP_RATIO(3, 2);

    if (world->level->flags & ML_LEVEL_LIFE_SIM)
    {
        ml_tick_life_sim(world);
        return;
    }
    player->shield = world->sword_ticks == 0 && world->item_ticks == 0;
    if (dx && dy)
        speed = ML_FP_RATIO(17, 16);
    player->vx = dx * speed;
    player->vy = dy * speed;
    if (dx || dy)
    {
        player->facing_x = dx;
        player->facing_y = dy;
        player->action = ML_ACTION_WALK;
    }
    else
        player->action = ML_ACTION_IDLE;

    if (ml_input_pressed(world, ML_INPUT_PRIMARY))
    {
        world->sword_ticks = 10;
        player->action = ML_ACTION_SWORD;
    }
    if (ml_input_pressed(world, ML_INPUT_SECONDARY))
    {
        world->item_ticks = 12;
        if (player->carrying)
        {
            ml_spawn_projectile(
                world, ML_ENTITY_POT,
                player->facing_x * ML_FP_INT(3),
                player->facing_y * ML_FP_INT(3));
            player->carrying = 0;
        }
        else if (world->selected_item > 0)
            ml_spawn_projectile(
                world, ML_ENTITY_ITEM,
                player->facing_x * ML_FP_INT(3),
                player->facing_y * ML_FP_INT(3));
        player->action = ML_ACTION_ITEM;
    }
    if (ml_input_pressed(world, ML_INPUT_PREVIOUS))
        world->selected_item = ML_MAX(0, world->selected_item - 1);
    if (ml_input_pressed(world, ML_INPUT_NEXT))
        world->selected_item = ML_MIN(7, world->selected_item + 1);
    if (world->sword_ticks > 0)
    {
        world->sword_ticks--;
        player->vx = 0;
        player->vy = 0;
        player->action = ML_ACTION_SWORD;
    }
    else if (world->item_ticks > 0)
    {
        world->item_ticks--;
        player->action = ML_ACTION_ITEM;
    }

    if (!ml_push_zelda_block(world, dx, dy))
    {
        ml_move_horizontal(world);
        ml_move_vertical(world);
    }
    player->grounded = true;
}

struct ml_surface_contact {
    uint16_t path_id;
    uint16_t segment;
    int x;
    int y;
    int dx;
    int dy;
    int32_t scale;
    uint8_t first_segment;
    uint8_t last_segment;
    int64_t distance_squared;
};

static bool ml_sonic_nearest_surface(
    const struct ml_world *world, uint16_t desired_path, int maximum_distance,
    struct ml_surface_contact *contact)
{
    int64_t best = (int64_t)maximum_distance * maximum_distance + 1;
    int player_x = ml_fixed_to_int(world->player.x);
    int player_y = ml_fixed_to_int(world->player.y);
    unsigned path_index;
    bool found = false;

    for (path_index = 0; path_index < world->level->path_count; ++path_index)
    {
        struct ml_path path;
        unsigned segment;

        if (!ml_level_path(world->level, path_index, &path) ||
            !(path.flags & ML_PATH_SURFACE) ||
            (desired_path && path.id != desired_path))
            continue;
        for (segment = 0; segment + 1 < path.point_count; ++segment)
        {
            struct ml_path_point first;
            struct ml_path_point second;
            int dx;
            int dy;
            int64_t length_squared;
            int64_t dot;
            int64_t scale;
            int nearest_x;
            int nearest_y;
            int64_t distance_x;
            int64_t distance_y;
            int64_t distance_squared;

            if (!ml_level_path_point(
                    world->level, path.point_start + segment, &first) ||
                !ml_level_path_point(
                    world->level, path.point_start + segment + 1, &second))
                continue;
            dx = second.x - first.x;
            dy = second.y - first.y;
            length_squared = (int64_t)dx * dx + (int64_t)dy * dy;
            if (length_squared == 0)
                continue;
            dot = (int64_t)(player_x - first.x) * dx +
                  (int64_t)(player_y - first.y) * dy;
            scale = ML_CLAMP(
                (dot << ML_FIXED_SHIFT) / length_squared,
                0, ML_FIXED_ONE);
            nearest_x = first.x +
                (int)(((int64_t)dx * scale) >> ML_FIXED_SHIFT);
            nearest_y = first.y +
                (int)(((int64_t)dy * scale) >> ML_FIXED_SHIFT);
            distance_x = player_x - nearest_x;
            distance_y = player_y - nearest_y;
            distance_squared = distance_x * distance_x +
                               distance_y * distance_y;
            if (distance_squared < best)
            {
                best = distance_squared;
                contact->path_id = path.id;
                contact->segment = (uint16_t)segment;
                contact->x = nearest_x;
                contact->y = nearest_y;
                contact->dx = dx;
                contact->dy = dy;
                contact->scale = (int32_t)scale;
                contact->first_segment = segment == 0;
                contact->last_segment =
                    segment + 2 == path.point_count;
                contact->distance_squared = distance_squared;
                found = true;
            }
        }
    }
    return found && best <= (int64_t)maximum_distance * maximum_distance;
}

/*
 * Return 0 for ordinary tile physics, 1 when attached surface movement
 * completed this tick, or 2 when a jump/low-speed release supplied airborne
 * velocity that ordinary movement should integrate.
 */
static int ml_sonic_surface_step(struct ml_world *world, bool jump_pressed)
{
    struct ml_player *player = &world->player;
    struct ml_surface_contact contact;
    int length;
    int32_t tangent_x;
    int32_t tangent_y;

    if (!player->surface_attached &&
        (!player->grounded ||
         ML_ABS(player->ground_speed) < ML_FP_INT(2)))
        return 0;
    if (!ml_sonic_nearest_surface(
            world, player->surface_attached ? player->surface_path : 0,
            player->surface_attached ? 24 : 8, &contact))
    {
        player->surface_attached = 0;
        player->surface_path = 0;
        return 0;
    }
    player->surface_attached = 1;
    player->surface_path = contact.path_id;
    player->surface_segment = contact.segment;
    if (ML_ABS(contact.dy) > ML_ABS(contact.dx) || contact.dx < 0)
        player->rolling = true;
    player->x = ml_int_to_fixed(contact.x);
    player->y = ml_int_to_fixed(contact.y);
    length = ML_MAX(1, ML_MAX(ML_ABS(contact.dx), ML_ABS(contact.dy)));
    tangent_x = (int32_t)(((int64_t)player->ground_speed *
                           contact.dx) / length);
    tangent_y = (int32_t)(((int64_t)player->ground_speed *
                           contact.dy) / length);

    if (jump_pressed)
    {
        int32_t jump = ML_FP_RATIO(9, 2);

        player->vx = tangent_x +
            (int32_t)(((int64_t)jump * contact.dy) / length);
        player->vy = tangent_y -
            (int32_t)(((int64_t)jump * contact.dx) / length);
        player->surface_attached = 0;
        player->surface_path = 0;
        player->grounded = false;
        player->rolling = true;
        return 2;
    }
    if (ML_ABS(player->ground_speed) < ML_FP_INT(2) &&
        (ML_ABS(contact.dy) > ML_ABS(contact.dx) || contact.dx < 0))
    {
        player->vx = tangent_x;
        player->vy = tangent_y;
        player->surface_attached = 0;
        player->surface_path = 0;
        player->grounded = false;
        return 2;
    }

    player->ground_speed +=
        (int32_t)(((int64_t)ML_FP_RATIO(7, 32) * contact.dy) / length);
    player->ground_speed = ML_CLAMP(
        player->ground_speed, -ML_FP_INT(8), ML_FP_INT(8));
    player->x +=
        (int32_t)(((int64_t)player->ground_speed * contact.dx) / length);
    player->y +=
        (int32_t)(((int64_t)player->ground_speed * contact.dy) / length);
    if (ml_sonic_nearest_surface(
            world, player->surface_path, 24, &contact))
    {
        player->x = ml_int_to_fixed(contact.x);
        player->y = ml_int_to_fixed(contact.y);
        player->surface_segment = contact.segment;
        length = ML_MAX(
            1, ML_MAX(ML_ABS(contact.dx), ML_ABS(contact.dy)));
        player->vx =
            (int32_t)(((int64_t)player->ground_speed * contact.dx) /
                      length);
        player->vy =
            (int32_t)(((int64_t)player->ground_speed * contact.dy) /
                      length);
        if ((contact.last_segment &&
             contact.scale == ML_FIXED_ONE &&
             player->ground_speed > 0) ||
            (contact.first_segment &&
             contact.scale == 0 &&
             player->ground_speed < 0))
        {
            player->surface_attached = 0;
            player->surface_path = 0;
            player->grounded = false;
            return 2;
        }
    }
    player->grounded = true;
    return 1;
}

static void ml_tick_sonic(struct ml_world *world)
{
    struct ml_player *player = &world->player;
    int direction = !!(world->input & ML_INPUT_RIGHT) -
                    !!(world->input & ML_INPUT_LEFT);
    int32_t acceleration = player->grounded ?
                           ML_FP_RATIO(3, 64) : ML_FP_RATIO(3, 32);
    int32_t maximum = player->rolling ? ML_FP_INT(6) : ML_FP_INT(4);
    bool crouch = (world->input &
                   (ML_INPUT_DOWN | ML_INPUT_SECONDARY)) != 0;
    bool jump_pressed = ml_input_pressed(world, ML_INPUT_PRIMARY) &&
                        !crouch;
    int surface_state;

    if (direction)
    {
        player->facing_x = direction;
        player->ground_speed += direction * acceleration;
        player->ground_speed = ML_CLAMP(player->ground_speed,
                                        -maximum, maximum);
    }
    else if (player->grounded)
    {
        int32_t friction = ML_FP_RATIO(3, 64);

        if (ML_ABS(player->ground_speed) <= friction)
            player->ground_speed = 0;
        else
            player->ground_speed += player->ground_speed > 0 ?
                                   -friction : friction;
    }

    if (player->grounded && crouch)
    {
        player->rolling = true;
        player->action = ML_ACTION_ROLL;
        if (ml_input_pressed(world, ML_INPUT_PRIMARY))
        {
            if (player->spindash_charge < 8)
                player->spindash_charge++;
            player->action = ML_ACTION_SPINDASH;
        }
        else if (player->spindash_charge > 0)
            player->action = ML_ACTION_SPINDASH;
    }
    else
    {
        if (player->spindash_charge > 0)
        {
            player->ground_speed = player->facing_x *
                (ML_FP_INT(2) +
                 player->spindash_charge * ML_FP_RATIO(1, 2));
            player->spindash_charge = 0;
            player->rolling = true;
        }
        else if (ML_ABS(player->ground_speed) < ML_FP_RATIO(1, 2))
            player->rolling = false;
    }

    surface_state = ml_sonic_surface_step(world, jump_pressed);
    if (surface_state == 1)
    {
        player->action = player->rolling ? ML_ACTION_ROLL :
            (ML_ABS(player->ground_speed) > ML_FP_INT(2) ?
             ML_ACTION_RUN : ML_ACTION_WALK);
        return;
    }
    if (surface_state == 2)
    {
        player->vy += ML_FP_RATIO(7, 32);
        player->vy = ML_MIN(player->vy, ML_FP_INT(8));
        ml_move_horizontal(world);
        ml_move_vertical(world);
        player->action = player->vy < 0 ? ML_ACTION_JUMP : ML_ACTION_FALL;
        return;
    }

    if (jump_pressed && player->grounded)
    {
        player->vy = ML_FP_RATIO(-9, 2);
        player->grounded = false;
        player->rolling = true;
        player->action = ML_ACTION_JUMP;
    }

    player->vx = player->ground_speed;
    if (player->grounded)
    {
        int tile_size = world->level->tile_size;
        int tile_x = ml_fixed_to_int(player->x) / tile_size;
        int tile_y = ml_fixed_to_int(player->y) / tile_size;
        uint8_t under = ml_level_collision(world->level, tile_x, tile_y);

        if (under & ML_COLLISION_SLOPE_UP)
            player->ground_speed -= ML_FP_RATIO(1, 32);
        else if (under & ML_COLLISION_SLOPE_DOWN)
            player->ground_speed += ML_FP_RATIO(1, 32);
    }
    player->vy += ML_FP_RATIO(7, 32);
    player->vy = ML_MIN(player->vy, ML_FP_INT(8));
    ml_move_horizontal(world);
    ml_move_vertical(world);
    if (!player->grounded)
        player->action = player->vy < 0 ? ML_ACTION_JUMP : ML_ACTION_FALL;
    else if (player->spindash_charge > 0)
        player->action = ML_ACTION_SPINDASH;
    else if (player->rolling)
        player->action = ML_ACTION_ROLL;
    else
        player->action = ML_ABS(player->ground_speed) > ML_FP_INT(2) ?
                         ML_ACTION_RUN :
                         (player->ground_speed ? ML_ACTION_WALK :
                                                ML_ACTION_IDLE);
}

static bool ml_brawl_fighters_near(const struct ml_world *world)
{
    unsigned opponent = world->brawl_opponent_entity;

    return opponent < world->level->entity_count &&
        ML_ABS(ml_fixed_to_int(world->player.x) -
               ml_fixed_to_int(world->entity_x[opponent])) <= 24 &&
        ML_ABS(ml_fixed_to_int(world->player.y) -
               ml_fixed_to_int(world->entity_y[opponent])) <= 26;
}

static void ml_brawl_respawn_opponent(struct ml_world *world)
{
    struct ml_entity entity;
    unsigned opponent = world->brawl_opponent_entity;

    if (opponent >= world->level->entity_count ||
        !ml_level_entity(world->level, opponent, &entity))
        return;
    world->entity_x[opponent] = ml_int_to_fixed(entity.x);
    world->entity_y[opponent] = ml_int_to_fixed(entity.y);
    world->entity_vx[opponent] = 0;
    world->entity_vy[opponent] = 0;
    world->entity_alive[opponent] = true;
    world->brawl_opponent_damage = 0;
    world->brawl_opponent_invulnerable_ticks = 90;
}

static void ml_brawl_hit_opponent(struct ml_world *world, int power)
{
    struct ml_entity opponent_entity;
    unsigned opponent = world->brawl_opponent_entity;
    int direction;
    int weight;
    int knockback;

    if (!ml_brawl_fighters_near(world) ||
        world->brawl_opponent_invulnerable_ticks > 0 ||
        !ml_level_entity(world->level, opponent, &opponent_entity))
        return;
    direction = ml_fixed_to_int(world->entity_x[opponent]) >=
        ml_fixed_to_int(world->player.x) ? 1 : -1;
    weight = opponent_entity.param[2] > 0 ?
        opponent_entity.param[2] : 100;
    world->brawl_opponent_damage =
        ML_MIN(999, world->brawl_opponent_damage + power);
    knockback = ML_MAX(
        2,
        (power * 3 + world->brawl_opponent_damage / 5) * 100 / weight);
    world->entity_vx[opponent] =
        direction * ML_FP_RATIO(knockback, 2);
    world->entity_vy[opponent] =
        -ML_FP_RATIO(knockback + 3, 3);
    world->brawl_opponent_invulnerable_ticks = 12;
    world->score += power * 10;
    world->pending_effect = 4;
}

static void ml_brawl_hit_player(struct ml_world *world, int power)
{
    unsigned opponent = world->brawl_opponent_entity;
    int direction;
    int knockback;

    if (!ml_brawl_fighters_near(world) ||
        world->player.invulnerable_ticks > 0)
        return;
    direction = ml_fixed_to_int(world->player.x) >=
        ml_fixed_to_int(world->entity_x[opponent]) ? 1 : -1;
    world->brawl_player_damage =
        ML_MIN(999, world->brawl_player_damage + power);
    knockback = ML_MAX(2, power * 2 + world->brawl_player_damage / 8);
    world->player.vx = direction * ML_FP_RATIO(knockback, 2);
    world->player.vy = -ML_FP_RATIO(knockback + 3, 3);
    world->player.invulnerable_ticks = 18;
    world->player.action = ML_ACTION_HURT;
    world->player.action_ticks = 12;
    world->pending_effect = 2;
}

static void ml_tick_brawl(struct ml_world *world)
{
    struct ml_player *player = &world->player;
    unsigned opponent = world->brawl_opponent_entity;
    int direction = !!(world->input & ML_INPUT_RIGHT) -
                    !!(world->input & ML_INPUT_LEFT);
    int player_x;
    int player_y;
    int opponent_x;
    int opponent_y;
    int cpu_direction;
    bool opponent_grounded;
    int32_t target;

    if (opponent >= world->level->entity_count)
        return;
    if (direction)
    {
        player->facing_x = direction;
        player->vx += direction * ML_FP_RATIO(3, 16);
        player->vx = ML_CLAMP(
            player->vx, -ML_FP_RATIO(5, 2), ML_FP_RATIO(5, 2));
        player->action = ML_ACTION_RUN;
    }
    else if (player->grounded)
    {
        player->vx = player->vx * 3 / 4;
        if (ML_ABS(player->vx) < ML_FP_RATIO(1, 8))
            player->vx = 0;
        player->action = player->vx ? ML_ACTION_WALK : ML_ACTION_IDLE;
    }
    if (ml_input_pressed(world, ML_INPUT_UP) && player->grounded)
    {
        player->vy = ML_FP_RATIO(-19, 4);
        player->grounded = false;
        player->action = ML_ACTION_JUMP;
        world->pending_effect = 0;
    }
    if (ml_input_pressed(world, ML_INPUT_PRIMARY))
    {
        world->brawl_attack_ticks = 8;
        player->action = ML_ACTION_SWORD;
        player->action_ticks = 8;
        ml_brawl_hit_opponent(world, 7);
    }
    if (ml_input_pressed(world, ML_INPUT_SECONDARY))
    {
        world->brawl_attack_ticks = 14;
        player->action = ML_ACTION_ITEM;
        player->action_ticks = 14;
        ml_brawl_hit_opponent(world, 13);
    }
    if (world->brawl_attack_ticks > 0)
        world->brawl_attack_ticks--;
    player->vy = ML_MIN(player->vy + ML_FP_RATIO(7, 32), ML_FP_INT(6));
    ml_move_horizontal(world);
    ml_move_vertical(world);
    if (!player->grounded && player->action_ticks == 0)
        player->action = player->vy < 0 ? ML_ACTION_JUMP : ML_ACTION_FALL;

    player_x = ml_fixed_to_int(player->x);
    player_y = ml_fixed_to_int(player->y);
    opponent_x = ml_fixed_to_int(world->entity_x[opponent]);
    opponent_y = ml_fixed_to_int(world->entity_y[opponent]);
    cpu_direction = player_x > opponent_x ? 1 : -1;
    opponent_grounded = ml_entity_blocked(
        world, opponent_x, opponent_y + 1, 14, 26);
    world->entity_vx[opponent] +=
        cpu_direction * ML_FP_RATIO(1, 12);
    world->entity_vx[opponent] = ML_CLAMP(
        world->entity_vx[opponent],
        -ML_FP_RATIO(9, 4), ML_FP_RATIO(9, 4));
    if (opponent_grounded &&
        (player_y < opponent_y - 20 ||
         !ml_entity_blocked(
             world, opponent_x + cpu_direction * 12,
             opponent_y + 2, 14, 26)))
        world->entity_vy[opponent] = ML_FP_RATIO(-17, 4);
    world->entity_vy[opponent] = ML_MIN(
        world->entity_vy[opponent] + ML_FP_RATIO(7, 32),
        ML_FP_INT(6));
    target = world->entity_x[opponent] + world->entity_vx[opponent];
    if (!ml_entity_blocked(
            world, ml_fixed_to_int(target), opponent_y, 14, 26))
        world->entity_x[opponent] = target;
    else
        world->entity_vx[opponent] = -world->entity_vx[opponent] / 2;
    opponent_x = ml_fixed_to_int(world->entity_x[opponent]);
    target = world->entity_y[opponent] + world->entity_vy[opponent];
    if (!ml_entity_blocked(
            world, opponent_x, ml_fixed_to_int(target), 14, 26))
        world->entity_y[opponent] = target;
    else
        world->entity_vy[opponent] = 0;

    if (world->brawl_opponent_attack_ticks > 0)
        world->brawl_opponent_attack_ticks--;
    else if (ml_brawl_fighters_near(world) &&
             world->brawl_opponent_invulnerable_ticks == 0)
    {
        world->brawl_opponent_attack_ticks =
            30 + (world->tick + opponent) % 24;
        ml_brawl_hit_player(world, 6 + (world->tick % 5));
    }
    if (world->brawl_opponent_invulnerable_ticks > 0)
        world->brawl_opponent_invulnerable_ticks--;

    opponent_x = ml_fixed_to_int(world->entity_x[opponent]);
    opponent_y = ml_fixed_to_int(world->entity_y[opponent]);
    if (opponent_x < -32 ||
        opponent_x > world->level->map_width * world->level->tile_size + 32 ||
        opponent_y > world->level->map_height * world->level->tile_size + 32)
    {
        if (world->brawl_opponent_stocks > 0)
            world->brawl_opponent_stocks--;
        if (world->brawl_opponent_stocks == 0)
        {
            world->complete = true;
            world->entity_alive[opponent] = false;
            world->best_ticks = world->tick;
        }
        else
            ml_brawl_respawn_opponent(world);
    }
    if (player_x < -32 ||
        player_x > world->level->map_width * world->level->tile_size + 32 ||
        player_y > world->level->map_height * world->level->tile_size + 32 ||
        ml_player_on_hazard(world))
    {
        if (world->brawl_player_stocks > 0)
            world->brawl_player_stocks--;
        if (world->brawl_player_stocks == 0)
        {
            world->complete = true;
            player->health = 0;
        }
        else
        {
            world->brawl_player_damage = 0;
            ml_world_respawn(world);
        }
    }
}

static void ml_update_camera(struct ml_world *world)
{
    int map_width = world->level->map_width * world->level->tile_size;
    int map_height = world->level->map_height * world->level->tile_size;
    int player_x = ml_fixed_to_int(world->player.x);
    int player_y = ml_fixed_to_int(world->player.y);
    int target_x = player_x - world->level->view_width / 2;
    int target_y = player_y - world->level->view_height / 2;
    int max_x = ML_MAX(0, map_width - world->level->view_width);
    int max_y = ML_MAX(0, map_height - world->level->view_height);

    if ((world->level->flags & ML_LEVEL_BRAWL) &&
        world->brawl_opponent_entity < world->level->entity_count)
    {
        int opponent_x = ml_fixed_to_int(
            world->entity_x[world->brawl_opponent_entity]);
        int opponent_y = ml_fixed_to_int(
            world->entity_y[world->brawl_opponent_entity]);

        target_x = (player_x + opponent_x) / 2 -
                   world->level->view_width / 2;
        target_y = (player_y + opponent_y) / 2 -
                   world->level->view_height / 2;
    }
    if (world->level->ruleset != ML_RULESET_ZELDA)
    {
        if (world->input & ML_INPUT_PREVIOUS)
            target_x -= world->level->view_width / 6;
        if (world->input & ML_INPUT_NEXT)
            target_x += world->level->view_width / 6;
    }
    target_x = ML_CLAMP(target_x, 0, max_x);
    target_y = ML_CLAMP(target_y, 0, max_y);
    if (world->level->ruleset == ML_RULESET_ZELDA)
    {
        target_x = player_x / world->level->view_width *
                   world->level->view_width;
        target_y = player_y / world->level->view_height *
                   world->level->view_height;
        world->camera_x = ml_int_to_fixed(ML_CLAMP(target_x, 0, max_x));
        world->camera_y = ml_int_to_fixed(ML_CLAMP(target_y, 0, max_y));
    }
    else
    {
        world->camera_x += (ml_int_to_fixed(target_x) - world->camera_x) / 4;
        world->camera_y += (ml_int_to_fixed(target_y) - world->camera_y) / 4;
    }
}

bool ml_world_init(struct ml_world *world, const struct ml_level *level)
{
    unsigned i;
    bool found_player = false;

    if (!world || !level)
        return false;
    memset(world, 0, sizeof(*world));
    world->level = level;
    world->player.health = 3;
    world->player.facing_x = 1;
    world->player.facing_y = 1;
    world->pending_effect = -1;
    world->vehicle_entity = UINT16_MAX;
    world->interaction_entity = UINT16_MAX;
    world->furniture_held_entity = UINT16_MAX;
    world->brawl_opponent_entity = UINT16_MAX;
    if (level->flags & ML_LEVEL_LIFE_SIM)
        world->debt = 500;

    if (level->ruleset == ML_RULESET_ZELDA)
    {
        world->player.width = 14;
        world->player.height = 16;
        world->player.facing_x = 0;
        world->player.facing_y = 1;
    }
    else if (level->ruleset == ML_RULESET_SONIC)
    {
        world->player.width = 14;
        world->player.height = 28;
    }
    else
    {
        world->player.width = 14;
        world->player.height = 26;
    }

    for (i = 0; i < level->entity_count; ++i)
    {
        struct ml_entity entity;

        world->entity_alive[i] = true;
        if (!ml_level_entity(level, i, &entity))
            return false;
        world->entity_x[i] = ml_int_to_fixed(entity.x);
        world->entity_y[i] = ml_int_to_fixed(entity.y);
        if (entity.kind == ML_ENTITY_ENEMY)
        {
            int32_t speed = entity.param[2] > 0 ?
                ML_FP_RATIO(entity.param[2], 256) : ML_FP_RATIO(1, 2);

            world->entity_vx[i] =
                (entity.flags & 0x400u) ? speed : -speed;
            if ((level->flags & ML_LEVEL_BRAWL) &&
                world->brawl_opponent_entity == UINT16_MAX)
            {
                world->brawl_opponent_entity = (uint16_t)i;
                world->brawl_player_stocks = 3;
                world->brawl_opponent_stocks =
                    entity.param[3] > 0 ?
                    ML_CLAMP(entity.param[3], 1, 9) : 3;
            }
        }
        else if (entity.kind == ML_ENTITY_CAR &&
                 world->vehicle_entity == UINT16_MAX)
            world->vehicle_entity = (uint16_t)i;
        if (!found_player && entity.kind == ML_ENTITY_PLAYER)
        {
            world->spawn_x = ml_int_to_fixed(entity.x);
            world->spawn_y = ml_int_to_fixed(entity.y);
            world->checkpoint_x = world->spawn_x;
            world->checkpoint_y = world->spawn_y;
            found_player = true;
        }
    }
    if (!found_player)
        return false;
    ml_world_respawn(world);
    ml_update_camera(world);
    return true;
}

void ml_world_respawn(struct ml_world *world)
{
    if (!world)
        return;
    world->player.x = world->checkpoint_x;
    world->player.y = world->checkpoint_y;
    world->player.vx = 0;
    world->player.vy = 0;
    world->player.ground_speed = 0;
    world->player.grounded = false;
    world->player.rolling = false;
    world->player.spindash_charge = 0;
    world->player.invulnerable_ticks = 90;
    world->player.action = ML_ACTION_IDLE;
    world->player.health = ML_MAX(world->player.health, 3);
    world->rings = 0;
    world->loose_rings = 0;
    world->projectile_active = 0;
    ml_life_close_interaction(world);
    if (world->car_active)
        ml_life_exit_car(world);
    memset(world->loose_ring_active, 0,
           sizeof(world->loose_ring_active));
}

void ml_world_tick(struct ml_world *world, uint32_t input)
{
    if (!world || !world->level)
        return;
    world->previous_input = world->input;
    world->input = input;
    world->pending_effect = -1;

    if (ml_input_pressed(world, ML_INPUT_PAUSE))
        world->paused ^= 1;
    if (world->paused || world->complete)
        return;

    world->tick++;
    if (world->player.invulnerable_ticks > 0)
        world->player.invulnerable_ticks--;
    if (world->player.action_ticks > 0)
        world->player.action_ticks--;
    if (world->room_transition_ticks > 0)
        world->room_transition_ticks--;
    if (world->job_cooldown > 0)
        world->job_cooldown--;
    if (world->interaction_notice_ticks > 0)
    {
        world->interaction_notice_ticks--;
        if (world->interaction_notice_ticks == 0)
            world->interaction_notice = ML_LIFE_NOTICE_NONE;
    }

    ml_update_paths(world);
    if (world->level->flags & ML_LEVEL_BRAWL)
    {
        ml_tick_brawl(world);
        ml_update_events(world);
        ml_update_camera(world);
        return;
    }
    switch (world->level->ruleset)
    {
        case ML_RULESET_MARIO:
            ml_tick_mario(world);
            break;
        case ML_RULESET_ZELDA:
            ml_tick_zelda(world);
            break;
        case ML_RULESET_SONIC:
            ml_tick_sonic(world);
            break;
        default:
            return;
    }

    if (ml_player_on_hazard(world) ||
        ml_fixed_to_int(world->player.y) >
            world->level->map_height * world->level->tile_size + 32)
    {
        if (world->level->ruleset == ML_RULESET_ZELDA &&
            world->player.invulnerable_ticks == 0)
        {
            int health = ML_MAX(1, world->player.health - 1);

            ml_world_respawn(world);
            world->player.health = health;
            world->player.invulnerable_ticks = 60;
        }
        else
            ml_hurt_player(world, 1);
    }
    ml_update_enemy_motion(world);
    if (world->level->flags & ML_LEVEL_LIFE_SIM)
        ml_update_life_npcs(world);
    ml_update_projectile(world);
    ml_update_loose_rings(world);
    ml_update_entities(world);
    ml_update_events(world);
    ml_update_camera(world);
}

void ml_world_snapshot(const struct ml_world *world,
                       struct ml_snapshot *snapshot)
{
    if (!world || !snapshot)
        return;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->tick = world->tick;
    snapshot->player_x = world->player.x;
    snapshot->player_y = world->player.y;
    snapshot->player_vx = world->player.vx;
    snapshot->player_vy = world->player.vy;
    snapshot->camera_x = world->camera_x;
    snapshot->camera_y = world->camera_y;
    snapshot->collectibles = world->collectibles;
    snapshot->rings = world->rings;
    snapshot->keys = world->keys;
    snapshot->health = world->player.health;
    snapshot->action = world->player.action;
    snapshot->grounded = world->player.grounded;
    snapshot->complete = world->complete;
    snapshot->paused = world->paused;
    snapshot->facing_x = world->player.facing_x;
    snapshot->facing_y = world->player.facing_y;
    snapshot->power_state = world->player.power_state;
    snapshot->carrying = world->player.carrying;
    snapshot->shield = world->player.shield;
    snapshot->credits = world->credits;
    snapshot->debt = world->debt;
    snapshot->house_level = world->house_level;
    snapshot->car_level = world->car_level;
    snapshot->job_cooldown = world->job_cooldown;
    snapshot->furniture_held_entity =
        world->furniture_held_entity;
    snapshot->house_style = world->house_style;
    snapshot->car_active = world->car_active;
    snapshot->interaction = world->interaction;
    snapshot->interaction_choice = world->interaction_choice;
    snapshot->interaction_notice = world->interaction_notice;
    snapshot->brawl_player_damage = world->brawl_player_damage;
    snapshot->brawl_opponent_damage = world->brawl_opponent_damage;
    snapshot->brawl_opponent_entity = world->brawl_opponent_entity;
    snapshot->brawl_player_stocks = world->brawl_player_stocks;
    snapshot->brawl_opponent_stocks = world->brawl_opponent_stocks;
}

static uint32_t ml_digest_byte(uint32_t digest, uint8_t value)
{
    return (digest ^ value) * 16777619u;
}

static uint32_t ml_digest_u16(uint32_t digest, uint16_t value)
{
    digest = ml_digest_byte(digest, (uint8_t)value);
    return ml_digest_byte(digest, (uint8_t)(value >> 8));
}

static uint32_t ml_digest_u32(uint32_t digest, uint32_t value)
{
    digest = ml_digest_u16(digest, (uint16_t)value);
    return ml_digest_u16(digest, (uint16_t)(value >> 16));
}

uint32_t ml_world_digest(const struct ml_world *world)
{
    const struct ml_player *player;
    uint32_t digest = 2166136261u;
    unsigned i;

    if (!world || !world->level)
        return 0;
    player = &world->player;
    digest = ml_digest_u32(digest, (uint32_t)player->x);
    digest = ml_digest_u32(digest, (uint32_t)player->y);
    digest = ml_digest_u32(digest, (uint32_t)player->vx);
    digest = ml_digest_u32(digest, (uint32_t)player->vy);
    digest = ml_digest_u32(digest, (uint32_t)player->ground_speed);
    digest = ml_digest_u16(digest, (uint16_t)player->width);
    digest = ml_digest_u16(digest, (uint16_t)player->height);
    digest = ml_digest_u16(digest, (uint16_t)player->facing_x);
    digest = ml_digest_u16(digest, (uint16_t)player->facing_y);
    digest = ml_digest_u16(digest, (uint16_t)player->health);
    digest = ml_digest_u16(digest, (uint16_t)player->invulnerable_ticks);
    digest = ml_digest_u16(digest, (uint16_t)player->action_ticks);
    digest = ml_digest_byte(digest, player->action);
    digest = ml_digest_byte(digest, player->grounded);
    digest = ml_digest_byte(digest, player->rolling);
    digest = ml_digest_byte(digest, player->in_water);
    digest = ml_digest_byte(digest, player->power_state);
    digest = ml_digest_byte(digest, player->carrying);
    digest = ml_digest_byte(digest, player->shield);
    digest = ml_digest_byte(digest, player->spindash_charge);
    digest = ml_digest_byte(digest, player->surface_attached);
    digest = ml_digest_u16(digest, player->surface_path);
    digest = ml_digest_u16(digest, player->surface_segment);
    digest = ml_digest_u32(digest, world->input);
    digest = ml_digest_u32(digest, world->previous_input);
    digest = ml_digest_u32(digest, world->tick);
    digest = ml_digest_u32(digest, (uint32_t)world->camera_x);
    digest = ml_digest_u32(digest, (uint32_t)world->camera_y);
    digest = ml_digest_u32(digest, (uint32_t)world->spawn_x);
    digest = ml_digest_u32(digest, (uint32_t)world->spawn_y);
    digest = ml_digest_u32(digest, (uint32_t)world->checkpoint_x);
    digest = ml_digest_u32(digest, (uint32_t)world->checkpoint_y);
    digest = ml_digest_u16(digest, (uint16_t)world->collectibles);
    digest = ml_digest_u16(digest, (uint16_t)world->rings);
    digest = ml_digest_u16(digest, (uint16_t)world->keys);
    digest = ml_digest_u16(digest, (uint16_t)world->score);
    digest = ml_digest_u16(digest, (uint16_t)world->sword_ticks);
    digest = ml_digest_u16(digest, (uint16_t)world->item_ticks);
    digest = ml_digest_u16(digest, (uint16_t)world->hurt_ticks);
    digest = ml_digest_u16(digest, (uint16_t)world->loose_rings);
    digest = ml_digest_u16(digest, (uint16_t)world->selected_item);
    digest = ml_digest_u16(digest, (uint16_t)world->pending_effect);
    digest = ml_digest_u32(digest, world->best_ticks);
    digest = ml_digest_byte(digest, world->complete);
    digest = ml_digest_byte(digest, world->paused);
    digest = ml_digest_byte(digest, world->switch_state);
    digest = ml_digest_byte(digest, world->room_transition_ticks);
    if (world->level->flags & ML_LEVEL_LIFE_SIM)
    {
        digest = ml_digest_u32(digest, world->credits);
        digest = ml_digest_u32(digest, world->debt);
        digest = ml_digest_u16(digest, world->house_level);
        digest = ml_digest_u16(digest, world->car_level);
        digest = ml_digest_u16(digest, world->vehicle_entity);
        digest = ml_digest_u16(digest, world->interaction_entity);
        digest = ml_digest_u16(digest, world->furniture_held_entity);
        digest = ml_digest_u16(digest, world->job_cooldown);
        digest = ml_digest_u16(digest, world->interaction_notice_ticks);
        digest = ml_digest_byte(digest, world->house_style);
        digest = ml_digest_byte(digest, world->car_active);
        digest = ml_digest_byte(digest, world->interaction);
        digest = ml_digest_byte(digest, world->interaction_choice);
        digest = ml_digest_byte(digest, world->interaction_notice);
    }
    if (world->level->flags & ML_LEVEL_BRAWL)
    {
        digest = ml_digest_u16(
            digest, (uint16_t)world->brawl_player_damage);
        digest = ml_digest_u16(
            digest, (uint16_t)world->brawl_opponent_damage);
        digest = ml_digest_u16(digest, world->brawl_opponent_entity);
        digest = ml_digest_u16(digest, world->brawl_attack_ticks);
        digest = ml_digest_u16(
            digest, world->brawl_opponent_attack_ticks);
        digest = ml_digest_u16(
            digest, world->brawl_opponent_invulnerable_ticks);
        digest = ml_digest_byte(digest, world->brawl_player_stocks);
        digest = ml_digest_byte(digest, world->brawl_opponent_stocks);
    }
    digest = ml_digest_u32(digest, (uint32_t)world->projectile_x);
    digest = ml_digest_u32(digest, (uint32_t)world->projectile_y);
    digest = ml_digest_u32(digest, (uint32_t)world->projectile_vx);
    digest = ml_digest_u32(digest, (uint32_t)world->projectile_vy);
    digest = ml_digest_u16(digest, world->projectile_ticks);
    digest = ml_digest_byte(digest, world->projectile_kind);
    digest = ml_digest_byte(digest, world->projectile_active);
    for (i = 0; i < ML_MAX_LOOSE_RINGS; ++i)
    {
        digest = ml_digest_u32(digest, (uint32_t)world->loose_ring_x[i]);
        digest = ml_digest_u32(digest, (uint32_t)world->loose_ring_y[i]);
        digest = ml_digest_u32(digest, (uint32_t)world->loose_ring_vx[i]);
        digest = ml_digest_u32(digest, (uint32_t)world->loose_ring_vy[i]);
        digest = ml_digest_u16(digest, world->loose_ring_ticks[i]);
        digest = ml_digest_byte(digest, world->loose_ring_active[i]);
    }
    for (i = 0; i < world->level->entity_count; ++i)
    {
        digest = ml_digest_byte(digest, world->entity_alive[i]);
        digest = ml_digest_byte(digest, world->block_push_ticks[i]);
        digest = ml_digest_u32(digest, (uint32_t)world->entity_x[i]);
        digest = ml_digest_u32(digest, (uint32_t)world->entity_y[i]);
        digest = ml_digest_u32(digest, (uint32_t)world->entity_vx[i]);
        if (world->level->flags & ML_LEVEL_BRAWL)
            digest = ml_digest_u32(
                digest, (uint32_t)world->entity_vy[i]);
    }
    for (i = 0; i < world->level->event_count; ++i)
    {
        if ((i & 7u) == 0)
            digest = ml_digest_byte(digest, world->event_fired[i >> 3]);
        digest = ml_digest_u16(digest, world->event_delay[i]);
    }
    return digest;
}
