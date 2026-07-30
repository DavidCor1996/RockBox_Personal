/***************************************************************************
 * Bounded pack, private art-kit, save, and launcher-manifest I/O.
 ***************************************************************************/

#include "maker_lite.h"

#define ML_SAVE_SIZE 384
#define ML_SAVE_VERSION 5
#define ML_SAVE_LEGACY_VERSION 3
#define ML_SAVE_LIFE_VERSION 4
#define ML_SAVE_ENTITY_OFFSET 96
#define ML_SAVE_EVENT_OFFSET 288
#define ML_SAVE_CRC_OFFSET 380
#define ML_SAVE_LIFE_CREDITS_OFFSET 324
#define ML_SAVE_LIFE_DEBT_OFFSET 328
#define ML_SAVE_LIFE_HOUSE_LEVEL_OFFSET 332
#define ML_SAVE_LIFE_CAR_LEVEL_OFFSET 334
#define ML_SAVE_LIFE_HOUSE_STYLE_OFFSET 336
#define ML_SAVE_LIFE_CAR_ACTIVE_OFFSET 337
#define ML_SAVE_LIFE_PLAYER_X_OFFSET 340
#define ML_SAVE_LIFE_PLAYER_Y_OFFSET 344
#define ML_SAVE_LIFE_CAR_X_OFFSET 348
#define ML_SAVE_LIFE_CAR_Y_OFFSET 352
#define ML_SAVE_LIFE_FURNITURE_A_OFFSET 288
#define ML_SAVE_LIFE_FURNITURE_A_COUNT 8
#define ML_SAVE_LIFE_FURNITURE_B_OFFSET 356
#define ML_SAVE_LIFE_FURNITURE_B_COUNT 4
#define ML_SAVE_LIFE_FURNITURE_HELD_OFFSET 372
#define ML_SAVE_LIFE_FURNITURE_COUNT_OFFSET 374
#define ML_SAVE_LIFE_FURNITURE_MAX \
    (ML_SAVE_LIFE_FURNITURE_A_COUNT + ML_SAVE_LIFE_FURNITURE_B_COUNT)

struct browser_entry {
    char title[64];
    char path[MAX_PATH];
};

static struct browser_entry browser_entries[MAKER_LITE_MAX_BROWSER_PROJECTS];
static int browser_count;

static uint16_t read_u16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u16(unsigned char *p, uint16_t value)
{
    p[0] = value;
    p[1] = value >> 8;
}

static int16_t read_i16(const unsigned char *p)
{
    return (int16_t)read_u16(p);
}

static void write_i16(unsigned char *p, int16_t value)
{
    write_u16(p, (uint16_t)value);
}

static void write_u32(unsigned char *p, uint32_t value)
{
    p[0] = value;
    p[1] = value >> 8;
    p[2] = value >> 16;
    p[3] = value >> 24;
}

static bool read_exact(int fd, void *destination, size_t size)
{
    unsigned char *out = destination;

    while (size > 0)
    {
        ssize_t count = rb->read(fd, out, size);

        if (count <= 0)
            return false;
        out += count;
        size -= count;
    }
    return true;
}

static void *arena_take(size_t size, size_t alignment)
{
    uintptr_t address = (uintptr_t)maker_lite.arena + maker_lite.arena_used;
    uintptr_t aligned = (address + alignment - 1) & ~(alignment - 1);
    size_t used = aligned - (uintptr_t)maker_lite.arena;

    if (used > maker_lite.arena_size || size > maker_lite.arena_size - used)
        return NULL;
    maker_lite.arena_used = used + size;
    return (void *)aligned;
}

bool maker_lite_storage_load(const char *path)
{
    enum ml_pack_error error;
    unsigned char *data;
    off_t file_size;
    int fd;

    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
    {
        rb->splash(HZ * 2, "Maker Lite project missing");
        return false;
    }
    file_size = rb->filesize(fd);
    if (file_size < ML_PACK_HEADER_SIZE ||
        file_size > (off_t)(maker_lite.arena_size - maker_lite.arena_used))
    {
        rb->close(fd);
        rb->splash(HZ * 2, "Project is too large");
        return false;
    }
    data = arena_take((size_t)file_size, 4);
    if (!data || !read_exact(fd, data, (size_t)file_size))
    {
        rb->close(fd);
        rb->splash(HZ * 2, "Could not read project");
        return false;
    }
    rb->close(fd);

    error = ml_pack_open(data, (size_t)file_size, &maker_lite.level);
    if (error != ML_PACK_OK)
    {
        rb->splashf(HZ * 3, "Invalid project: %s",
                    ml_pack_error_string(error));
        return false;
    }
    maker_lite.pack_data = data;
    maker_lite.pack_size = file_size;
    rb->strlcpy(maker_lite.pack_path, path,
                sizeof(maker_lite.pack_path));
    rb->snprintf(maker_lite.save_path, sizeof(maker_lite.save_path),
                 MAKER_LITE_SAVE_ROOT "/%s.sav",
                 maker_lite.level.project_id);
    return ml_world_init(&maker_lite.world, &maker_lite.level);
}

bool maker_lite_storage_load_art(void)
{
    unsigned char header[MAKER_LITE_ART_HEADER_SIZE];
    unsigned char *content;
    const unsigned char *animation_table;
    unsigned char extra;
    char path[MAX_PATH];
    uint16_t version;
    uint16_t cell_size;
    uint16_t cell_count;
    uint16_t player_frame_count = 0;
    uint32_t expected_crc;
    size_t pixel_bytes;
    size_t content_bytes;
    size_t animation_bytes;
    off_t file_size;
    unsigned i;
    int fd;

    rb->snprintf(path, sizeof(path), MAKER_LITE_KIT_ROOT "/%s/art.mla",
                 maker_lite.level.kit_id);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0 || !read_exact(fd, header, sizeof(header)))
    {
        if (fd >= 0)
            rb->close(fd);
        rb->splashf(HZ * 3, "Private kit missing: %s",
                    maker_lite.level.kit_id);
        return false;
    }
    file_size = rb->filesize(fd);
    version = read_u16(header + 4);
    cell_size = read_u16(header + 6);
    cell_count = read_u16(header + 8);
    expected_crc = read_u32(header + 12);
    if (rb->memcmp(header, "MLAR", 4) ||
        (version != 1 && version != 2 && version != 3 && version != 4) ||
        cell_size != 16 || cell_count == 0 ||
        cell_count > MAKER_LITE_MAX_ART_CELLS ||
        (version < 4 &&
         read_u16(header + 10) + ML_ACTION_COUNT > cell_count) ||
        rb->strncmp((const char *)header + 16, maker_lite.level.kit_id,
                    ML_ID_SIZE))
    {
        rb->close(fd);
        rb->splash(HZ * 3, "Private art kit is incompatible");
        return false;
    }
    pixel_bytes = (size_t)cell_size * cell_size * cell_count *
                  sizeof(fb_data);
    animation_bytes =
        version == 2 ? MAKER_LITE_ANIMATION_TABLE_V2_SIZE :
        version >= 3 ? MAKER_LITE_ANIMATION_TABLE_V3_SIZE : 0;
    if (version == 4)
    {
        if (file_size < MAKER_LITE_ART_HEADER_SIZE ||
            (size_t)(file_size - MAKER_LITE_ART_HEADER_SIZE) <
                pixel_bytes + animation_bytes + 4)
        {
            rb->close(fd);
            rb->splash(HZ * 3, "Private metasprite table is truncated");
            return false;
        }
        content_bytes = (size_t)file_size - MAKER_LITE_ART_HEADER_SIZE;
    }
    else
        content_bytes = pixel_bytes + animation_bytes;
    content = arena_take(content_bytes, sizeof(fb_data));
    if (!content || !read_exact(fd, content, content_bytes) ||
        rb->read(fd, &extra, 1) != 0)
    {
        rb->close(fd);
        rb->splash(HZ * 2, "Art kit exceeds memory budget");
        return false;
    }
    rb->close(fd);
    if (ml_crc32(content, content_bytes) != expected_crc)
    {
        rb->splash(HZ * 3, "Art kit checksum mismatch");
        return false;
    }
    maker_lite.art.cells = (const fb_data *)content;
    maker_lite.art.cell_size = cell_size;
    maker_lite.art.cell_count = cell_count;
    maker_lite.art.player_base = read_u16(header + 10);
    rb->memcpy(maker_lite.art.entity_cells, header + 48,
               sizeof(maker_lite.art.entity_cells));
    animation_table = content + pixel_bytes;
    maker_lite.art.player_frames = NULL;
    maker_lite.art.player_frame_count = 0;
    if (version == 4)
    {
        const unsigned char *frames =
            animation_table + MAKER_LITE_ANIMATION_TABLE_V3_SIZE;
        size_t expected_size;

        player_frame_count = read_u16(frames);
        expected_size = pixel_bytes + animation_bytes + 4 +
            (size_t)player_frame_count * MAKER_LITE_PLAYER_FRAME_SIZE;
        if (read_u16(frames + 2) != 0 ||
            player_frame_count < ML_ACTION_COUNT ||
            player_frame_count > MAKER_LITE_MAX_PLAYER_FRAMES ||
            content_bytes != expected_size ||
            maker_lite.art.player_base + ML_ACTION_COUNT >
                player_frame_count)
        {
            rb->splash(HZ * 3, "Private metasprite table is invalid");
            return false;
        }
        maker_lite.art.player_frames = frames + 4;
        maker_lite.art.player_frame_count = player_frame_count;
        for (i = 0; i < player_frame_count; ++i)
        {
            const unsigned char *frame =
                maker_lite.art.player_frames +
                i * MAKER_LITE_PLAYER_FRAME_SIZE;
            unsigned columns = frame[0];
            unsigned rows = frame[1];
            unsigned cell;

            if (columns < 1 || columns > 4 || rows < 1 || rows > 4 ||
                (int8_t)frame[2] < -64 || (int8_t)frame[2] > 64 ||
                (int8_t)frame[3] < -64 || (int8_t)frame[3] > 64)
            {
                rb->splash(HZ * 3, "Private metasprite frame is invalid");
                return false;
            }
            for (cell = 0; cell < 16; ++cell)
            {
                uint16_t atlas_cell = read_u16(frame + 4 + cell * 2);

                if ((cell < columns * rows &&
                     atlas_cell != 0xffff && atlas_cell >= cell_count) ||
                    (cell >= columns * rows && atlas_cell != 0xffff))
                {
                    rb->splash(HZ * 3,
                               "Private metasprite cell is invalid");
                    return false;
                }
            }
        }
    }
    for (i = 0; i < ML_ACTION_COUNT; ++i)
    {
        unsigned direction;

        for (direction = 0; direction < MAKER_LITE_DIRECTION_COUNT;
             ++direction)
        {
            uint16_t start;
            uint8_t count;
            uint8_t raw_ticks;
            uint8_t ticks;
            size_t entry = version >= 3 ?
                (i * MAKER_LITE_DIRECTION_COUNT + direction) * 4 :
                i * 4;

            if (version >= 2)
            {
                start = read_u16(animation_table + entry);
                count = animation_table[entry + 2];
                raw_ticks = animation_table[entry + 3];
            }
            else
            {
                start = maker_lite.art.player_base + i;
                count = 1;
                raw_ticks = 1;
            }
            ticks = raw_ticks & 0x7f;
            {
                unsigned limit = version == 4 ?
                    player_frame_count : cell_count;

                if (count == 0 || ticks == 0 || ticks > 60 ||
                    start >= limit || count > limit - start)
                {
                    rb->splash(HZ * 3,
                               "Private animation table is invalid");
                    return false;
                }
            }
            maker_lite.art.animation_start[direction][i] = start;
            maker_lite.art.animation_count[direction][i] = count;
            maker_lite.art.animation_ticks[direction][i] = ticks;
            maker_lite.art.animation_mirror[direction][i] =
                version >= 3 && (raw_ticks & 0x80);
        }
    }
    return true;
}

static bool save_valid(const unsigned char *data, bool *pack_matches)
{
    uint32_t payload_crc;
    uint16_t version = read_u16(data + 4);
    bool same_pack;

    if (rb->memcmp(data, "RMLS", 4) ||
        version < ML_SAVE_LEGACY_VERSION ||
        version > ML_SAVE_VERSION ||
        data[6] != maker_lite.level.ruleset ||
        rb->strncmp((const char *)data + 16, maker_lite.level.project_id,
                    ML_ID_SIZE))
        return false;
    payload_crc = read_u32(data + ML_SAVE_CRC_OFFSET);
    if (ml_crc32(data, ML_SAVE_CRC_OFFSET) != payload_crc)
        return false;
    same_pack = read_u32(data + 8) ==
                read_u32(maker_lite.pack_data + 40);
    if (!same_pack &&
        (!(maker_lite.level.flags & ML_LEVEL_LIFE_SIM) ||
         version < ML_SAVE_LIFE_VERSION))
        return false;
    if (pack_matches)
        *pack_matches = same_pack;
    return true;
}

static unsigned char *furniture_slot(unsigned char *data, unsigned index)
{
    if (index < ML_SAVE_LIFE_FURNITURE_A_COUNT)
        return data + ML_SAVE_LIFE_FURNITURE_A_OFFSET + index * 4;
    index -= ML_SAVE_LIFE_FURNITURE_A_COUNT;
    if (index < ML_SAVE_LIFE_FURNITURE_B_COUNT)
        return data + ML_SAVE_LIFE_FURNITURE_B_OFFSET + index * 4;
    return NULL;
}

static const unsigned char *furniture_slot_const(
    const unsigned char *data, unsigned index)
{
    return furniture_slot((unsigned char *)data, index);
}

static void load_furniture(const unsigned char *data)
{
    unsigned saved_count =
        data[ML_SAVE_LIFE_FURNITURE_COUNT_OFFSET];
    unsigned furniture_index = 0;
    unsigned entity_index;
    uint16_t held = read_u16(
        data + ML_SAVE_LIFE_FURNITURE_HELD_OFFSET);

    if (saved_count > ML_SAVE_LIFE_FURNITURE_MAX)
        saved_count = ML_SAVE_LIFE_FURNITURE_MAX;
    for (entity_index = 0;
         entity_index < maker_lite.level.entity_count;
         ++entity_index)
    {
        struct ml_entity entity;
        const unsigned char *slot;

        if (!ml_level_entity(
                &maker_lite.level, entity_index, &entity) ||
            entity.kind != ML_ENTITY_FURNITURE)
            continue;
        if (furniture_index >= saved_count)
            break;
        slot = furniture_slot_const(data, furniture_index++);
        maker_lite.world.entity_x[entity_index] =
            ml_int_to_fixed(read_i16(slot));
        maker_lite.world.entity_y[entity_index] =
            ml_int_to_fixed(read_i16(slot + 2));
    }
    if (held < maker_lite.level.entity_count)
    {
        struct ml_entity entity;

        if (ml_level_entity(&maker_lite.level, held, &entity) &&
            entity.kind == ML_ENTITY_FURNITURE)
        {
            maker_lite.world.furniture_held_entity = held;
            maker_lite.world.entity_alive[held] = false;
            maker_lite.world.player.carrying = 1;
        }
    }
}

bool maker_lite_storage_load_save(void)
{
    unsigned char data[ML_SAVE_SIZE];
    char previous[MAX_PATH];
    char corrupt[MAX_PATH];
    bool using_previous = false;
    bool pack_matches = false;
    uint16_t version;
    int fd = rb->open(maker_lite.save_path, O_RDONLY);

    if (fd < 0)
    {
        rb->snprintf(previous, sizeof(previous), "%s.previous",
                     maker_lite.save_path);
        fd = rb->open(previous, O_RDONLY);
        if (fd < 0)
            return true;
        using_previous = true;
    }
    if (!read_exact(fd, data, sizeof(data)) ||
        !save_valid(data, &pack_matches))
    {
        rb->close(fd);
        if (!using_previous)
        {
            rb->snprintf(corrupt, sizeof(corrupt), "%s.corrupt",
                         maker_lite.save_path);
            rb->remove(corrupt);
            rb->rename(maker_lite.save_path, corrupt);
            rb->snprintf(previous, sizeof(previous), "%s.previous",
                         maker_lite.save_path);
            fd = rb->open(previous, O_RDONLY);
            if (fd < 0)
                return false;
            using_previous = true;
            if (!read_exact(fd, data, sizeof(data)) ||
                !save_valid(data, &pack_matches))
            {
                rb->close(fd);
                return false;
            }
            rb->close(fd);
        }
        else
            return false;
    }
    else
        rb->close(fd);
    if (using_previous)
    {
        rb->remove(maker_lite.save_path);
        (void)rb->rename(previous, maker_lite.save_path);
    }
    version = read_u16(data + 4);
    if (pack_matches)
    {
        maker_lite.world.checkpoint_x = (int32_t)read_u32(data + 48);
        maker_lite.world.checkpoint_y = (int32_t)read_u32(data + 52);
        maker_lite.world.tick = read_u32(data + 56);
        maker_lite.world.switch_state = data[69];
        maker_lite.world.best_ticks = read_u32(data + 72);
        rb->memcpy(maker_lite.world.entity_alive,
                   data + ML_SAVE_ENTITY_OFFSET,
                   maker_lite.level.entity_count);
        if (!(maker_lite.level.flags & ML_LEVEL_LIFE_SIM))
            rb->memcpy(maker_lite.world.event_fired,
                       data + ML_SAVE_EVENT_OFFSET,
                       sizeof(maker_lite.world.event_fired));
        if (maker_lite.world.checkpoint_x ||
            maker_lite.world.checkpoint_y)
            ml_world_respawn(&maker_lite.world);
        maker_lite.world.collectibles = read_u16(data + 60);
        maker_lite.world.rings = read_u16(data + 62);
        maker_lite.world.keys = read_u16(data + 64);
        maker_lite.world.score = read_u16(data + 66);
        maker_lite.world.complete = data[68] != 0;
        maker_lite.world.player.health = data[70] ? data[70] : 3;
        maker_lite.world.player.power_state = data[320];
        maker_lite.world.player.carrying = data[321];
        maker_lite.world.selected_item =
            (int16_t)read_u16(data + 322);
    }
    if (version >= ML_SAVE_LIFE_VERSION &&
        (maker_lite.level.flags & ML_LEVEL_LIFE_SIM))
    {
        unsigned vehicle = maker_lite.world.vehicle_entity;

        maker_lite.world.credits =
            read_u32(data + ML_SAVE_LIFE_CREDITS_OFFSET);
        maker_lite.world.debt =
            read_u32(data + ML_SAVE_LIFE_DEBT_OFFSET);
        maker_lite.world.house_level =
            read_u16(data + ML_SAVE_LIFE_HOUSE_LEVEL_OFFSET);
        maker_lite.world.car_level =
            read_u16(data + ML_SAVE_LIFE_CAR_LEVEL_OFFSET);
        maker_lite.world.house_style =
            data[ML_SAVE_LIFE_HOUSE_STYLE_OFFSET] % 3;
        if (pack_matches)
        {
            maker_lite.world.car_active =
                data[ML_SAVE_LIFE_CAR_ACTIVE_OFFSET] != 0;
            maker_lite.world.player.x =
                (int32_t)read_u32(data + ML_SAVE_LIFE_PLAYER_X_OFFSET);
            maker_lite.world.player.y =
                (int32_t)read_u32(data + ML_SAVE_LIFE_PLAYER_Y_OFFSET);
            if (vehicle < maker_lite.level.entity_count)
            {
                maker_lite.world.entity_x[vehicle] =
                    (int32_t)read_u32(
                        data + ML_SAVE_LIFE_CAR_X_OFFSET);
                maker_lite.world.entity_y[vehicle] =
                    (int32_t)read_u32(
                        data + ML_SAVE_LIFE_CAR_Y_OFFSET);
                maker_lite.world.entity_alive[vehicle] =
                    !maker_lite.world.car_active;
            }
            if (version >= ML_SAVE_VERSION)
                load_furniture(data);
        }
    }
    return true;
}

bool maker_lite_storage_load_settings(void)
{
    static const uint32_t actions[4] = {
        ML_INPUT_PRIMARY, ML_INPUT_SECONDARY,
        ML_INPUT_PREVIOUS, ML_INPUT_NEXT,
    };
    unsigned char data[64];
    bool used[4] = { false, false, false, false };
    char path[MAX_PATH];
    int fd;
    unsigned i;

    for (i = 0; i < 4; ++i)
        maker_lite.physical_map[i] = actions[i];
    rb->snprintf(path, sizeof(path), "%s/%s.mlc",
                 MAKER_LITE_SETTINGS_ROOT, maker_lite.level.project_id);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return true;
    if (!read_exact(fd, data, sizeof(data)))
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);
    if (rb->memcmp(data, "MLCT", 4) || read_u16(data + 4) != 1 ||
        read_u32(data + 60) != ml_crc32(data, 60) ||
        rb->strncmp((const char *)data + 12,
                    maker_lite.level.project_id, ML_ID_SIZE))
        return false;
    for (i = 0; i < 4; ++i)
    {
        if (data[6 + i] >= 4 || used[data[6 + i]])
            return false;
        used[data[6 + i]] = true;
    }
    for (i = 0; i < 4; ++i)
        maker_lite.physical_map[i] = actions[data[6 + i]];
    return true;
}

bool maker_lite_storage_save(void)
{
    unsigned char data[ML_SAVE_SIZE];
    unsigned char verify[ML_SAVE_SIZE];
    char temporary[MAX_PATH];
    char previous[MAX_PATH];
    int fd;

    rb->memset(data, 0, sizeof(data));
    rb->memcpy(data, "RMLS", 4);
    write_u16(data + 4, ML_SAVE_VERSION);
    data[6] = maker_lite.level.ruleset;
    write_u32(data + 8, read_u32(maker_lite.pack_data + 40));
    rb->memcpy(data + 16, maker_lite.level.project_id, ML_ID_SIZE);
    write_u32(data + 48, maker_lite.world.checkpoint_x);
    write_u32(data + 52, maker_lite.world.checkpoint_y);
    write_u32(data + 56, maker_lite.world.tick);
    write_u16(data + 60, maker_lite.world.collectibles);
    write_u16(data + 62, maker_lite.world.rings);
    write_u16(data + 64, maker_lite.world.keys);
    write_u16(data + 66, maker_lite.world.score);
    data[68] = maker_lite.world.complete;
    data[69] = maker_lite.world.switch_state;
    data[70] = maker_lite.world.player.health;
    write_u32(data + 72, maker_lite.world.best_ticks);
    rb->memcpy(data + ML_SAVE_ENTITY_OFFSET,
               maker_lite.world.entity_alive,
               maker_lite.level.entity_count);
    rb->memcpy(data + ML_SAVE_EVENT_OFFSET,
               maker_lite.world.event_fired,
               sizeof(maker_lite.world.event_fired));
    data[320] = maker_lite.world.player.power_state;
    data[321] = maker_lite.world.player.carrying;
    write_u16(data + 322, maker_lite.world.selected_item);
    if (maker_lite.level.flags & ML_LEVEL_LIFE_SIM)
    {
        unsigned vehicle = maker_lite.world.vehicle_entity;
        unsigned furniture_count = 0;
        unsigned entity_index;
        int32_t car_x = maker_lite.world.player.x;
        int32_t car_y = maker_lite.world.player.y;

        if (!maker_lite.world.car_active &&
            vehicle < maker_lite.level.entity_count)
        {
            car_x = maker_lite.world.entity_x[vehicle];
            car_y = maker_lite.world.entity_y[vehicle];
        }
        write_u32(
            data + ML_SAVE_LIFE_CREDITS_OFFSET,
            maker_lite.world.credits);
        write_u32(
            data + ML_SAVE_LIFE_DEBT_OFFSET,
            maker_lite.world.debt);
        write_u16(
            data + ML_SAVE_LIFE_HOUSE_LEVEL_OFFSET,
            maker_lite.world.house_level);
        write_u16(
            data + ML_SAVE_LIFE_CAR_LEVEL_OFFSET,
            maker_lite.world.car_level);
        data[ML_SAVE_LIFE_HOUSE_STYLE_OFFSET] =
            maker_lite.world.house_style;
        data[ML_SAVE_LIFE_CAR_ACTIVE_OFFSET] =
            maker_lite.world.car_active;
        write_u32(
            data + ML_SAVE_LIFE_PLAYER_X_OFFSET,
            maker_lite.world.player.x);
        write_u32(
            data + ML_SAVE_LIFE_PLAYER_Y_OFFSET,
            maker_lite.world.player.y);
        write_u32(data + ML_SAVE_LIFE_CAR_X_OFFSET, car_x);
        write_u32(data + ML_SAVE_LIFE_CAR_Y_OFFSET, car_y);
        for (entity_index = 0;
             entity_index < maker_lite.level.entity_count &&
             furniture_count < ML_SAVE_LIFE_FURNITURE_MAX;
             ++entity_index)
        {
            struct ml_entity entity;
            unsigned char *slot;

            if (!ml_level_entity(
                    &maker_lite.level, entity_index, &entity) ||
                entity.kind != ML_ENTITY_FURNITURE)
                continue;
            slot = furniture_slot(data, furniture_count++);
            write_i16(slot, (int16_t)ml_fixed_to_int(
                maker_lite.world.entity_x[entity_index]));
            write_i16(slot + 2, (int16_t)ml_fixed_to_int(
                maker_lite.world.entity_y[entity_index]));
        }
        data[ML_SAVE_LIFE_FURNITURE_COUNT_OFFSET] =
            (unsigned char)furniture_count;
        write_u16(data + ML_SAVE_LIFE_FURNITURE_HELD_OFFSET,
                  maker_lite.world.furniture_held_entity);
    }
    write_u32(data + ML_SAVE_CRC_OFFSET,
              ml_crc32(data, ML_SAVE_CRC_OFFSET));

    rb->mkdir(MAKER_LITE_ROOT);
    rb->mkdir(MAKER_LITE_SAVE_ROOT);
    rb->snprintf(temporary, sizeof(temporary), "%s.tmp",
                 maker_lite.save_path);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0 || rb->write(fd, data, sizeof(data)) != sizeof(data))
    {
        if (fd >= 0)
            rb->close(fd);
        rb->remove(temporary);
        return false;
    }
    rb->close(fd);

    fd = rb->open(temporary, O_RDONLY);
    if (fd < 0 || !read_exact(fd, verify, sizeof(verify)) ||
        rb->memcmp(data, verify, sizeof(data)))
    {
        if (fd >= 0)
            rb->close(fd);
        rb->remove(temporary);
        return false;
    }
    rb->close(fd);
    rb->snprintf(previous, sizeof(previous), "%s.previous",
                 maker_lite.save_path);
    rb->remove(previous);
    if (rb->file_exists(maker_lite.save_path) &&
        rb->rename(maker_lite.save_path, previous) < 0)
    {
        rb->remove(temporary);
        return false;
    }
    if (rb->rename(temporary, maker_lite.save_path) < 0)
    {
        (void)rb->rename(previous, maker_lite.save_path);
        rb->remove(temporary);
        return false;
    }
    rb->remove(previous);
    return true;
}

static int split_tabs(char *line, char **fields, int count)
{
    int used = 1;
    char *cursor = line;

    fields[0] = line;
    while (*cursor && used < count)
    {
        if (*cursor == '\t')
        {
            *cursor = '\0';
            fields[used++] = cursor + 1;
        }
        cursor++;
    }
    return used;
}

static void load_browser_entries(void)
{
    char line[768];
    int fd = rb->open(MAKER_LITE_BROWSER_MANIFEST, O_RDONLY);

    browser_count = 0;
    if (fd < 0)
        fd = rb->open(MAKER_LITE_LEGACY_MANIFEST, O_RDONLY);
    if (fd < 0)
        return;
    while (browser_count < MAKER_LITE_MAX_BROWSER_PROJECTS &&
           rb->read_line(fd, line, sizeof(line)) > 0)
    {
        char *fields[11];

        if (line[0] == '#' || split_tabs(line, fields, 11) != 11 ||
            !fields[0][0] || !fields[10][0])
            continue;
        rb->strlcpy(browser_entries[browser_count].title, fields[0],
                    sizeof(browser_entries[browser_count].title));
        rb->strlcpy(browser_entries[browser_count].path, fields[10],
                    sizeof(browser_entries[browser_count].path));
        browser_count++;
    }
    rb->close(fd);
}

bool maker_lite_storage_pick_project(char *path, size_t path_size)
{
    int selected = 0;
    int event;

    load_browser_entries();
    if (browser_count == 0)
    {
        rb->splash(HZ * 2, "No synced Maker Lite projects");
        return false;
    }
    rb->button_clear_queue();
    while (true)
    {
        int first = selected > 6 ? selected - 6 : 0;
        int row;

        rb->lcd_set_background(LCD_RGBPACK(238, 238, 238));
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_clear_display();
        rb->lcd_setfont(FONT_UI);
        rb->lcd_putsxy(10, 8, "Maker Lite");
        rb->lcd_setfont(FONT_SYSFIXED);
        rb->lcd_putsxy(10, 30, "Choose a project");
        for (row = 0; row < 8 && first + row < browser_count; row++)
        {
            int index = first + row;
            int y = 52 + row * 20;

            if (index == selected)
            {
                rb->lcd_set_foreground(LCD_RGBPACK(40, 96, 180));
                rb->lcd_fillrect(6, y - 2, LCD_WIDTH - 12, 18);
                rb->lcd_set_foreground(LCD_WHITE);
            }
            else
                rb->lcd_set_foreground(LCD_BLACK);
            rb->lcd_putsxy(12, y, browser_entries[index].title);
        }
        rb->lcd_set_foreground(LCD_BLACK);
        rb->lcd_putsxy(8, LCD_HEIGHT - 15, "MENU Back   SELECT Play");
        rb->lcd_update();
        event = rb->button_get(true);
        if (event == SYS_USB_CONNECTED)
            return false;
#ifdef BUTTON_SCROLL_BACK
        if (event == BUTTON_SCROLL_BACK ||
            event == (BUTTON_SCROLL_BACK | BUTTON_REPEAT))
            selected = (selected + browser_count - 1) % browser_count;
#endif
#ifdef BUTTON_SCROLL_FWD
        if (event == BUTTON_SCROLL_FWD ||
            event == (BUTTON_SCROLL_FWD | BUTTON_REPEAT))
            selected = (selected + 1) % browser_count;
#endif
#ifdef BUTTON_LEFT
        if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_LEFT)
            selected = (selected + browser_count - 1) % browser_count;
#endif
#ifdef BUTTON_RIGHT
        if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_RIGHT)
            selected = (selected + 1) % browser_count;
#endif
#ifdef BUTTON_SELECT
        if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_SELECT &&
            !(event & BUTTON_REL))
        {
            rb->strlcpy(path, browser_entries[selected].path, path_size);
            return true;
        }
#endif
#ifdef BUTTON_MENU
        if (event == (BUTTON_MENU | BUTTON_REL))
            return false;
#endif
    }
}

void maker_lite_storage_log_session(void)
{
    char line[512];
    int length;
    int fd;

    rb->mkdir(ROCKBOX_DIR "/logs");
    fd = rb->open(ROCKBOX_DIR "/logs/maker_lite.log",
                  O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;
    length = rb->snprintf(
        line, sizeof(line),
        "project=%s ruleset=%u ticks=%u frames=%u missed=%u "
        "max_render_ticks=%u arena=%lu/%lu playback_start=%u "
        "playback_end=%u audio_effects=%u hold_pauses=%u menu_pauses=%u "
        "controls_screens=%u complete=%u world_tick=%lu "
        "room_transitions=%u credits=%lu debt=%lu house=%u car=%u "
        "driving=%u car_entries=%u car_exits=%u life_interactions=%u "
        "brawl_damage=%d,%d brawl_stocks=%u,%u "
        "controls=%lu,%lu,%lu,%lu\n",
        maker_lite.level.project_id, maker_lite.level.ruleset,
        maker_lite.simulation_ticks, maker_lite.rendered_frames,
        maker_lite.missed_deadlines, maker_lite.max_render_ticks,
        (unsigned long)maker_lite.arena_used,
        (unsigned long)maker_lite.arena_size,
        maker_lite.playback_active_at_start ? 1 : 0,
        (rb->audio_status() & AUDIO_STATUS_PLAY) ? 1 : 0,
        maker_lite.audio_ready ? 1 : 0,
        maker_lite.hold_pause_count,
        maker_lite.menu_pause_count,
        maker_lite.controls_screen_count,
        maker_lite.world.complete ? 1 : 0,
        (unsigned long)maker_lite.world.tick,
        maker_lite.room_transition_count,
        (unsigned long)maker_lite.world.credits,
        (unsigned long)maker_lite.world.debt,
        maker_lite.world.house_level,
        maker_lite.world.car_level,
        maker_lite.world.car_active ? 1 : 0,
        maker_lite.car_entry_count,
        maker_lite.car_exit_count,
        maker_lite.life_interaction_count,
        maker_lite.world.brawl_player_damage,
        maker_lite.world.brawl_opponent_damage,
        maker_lite.world.brawl_player_stocks,
        maker_lite.world.brawl_opponent_stocks,
        (unsigned long)maker_lite.physical_map[0],
        (unsigned long)maker_lite.physical_map[1],
        (unsigned long)maker_lite.physical_map[2],
        (unsigned long)maker_lite.physical_map[3]);
    if (length > 0)
        rb->write(fd, line, MIN((int)sizeof(line) - 1, length));
    rb->close(fd);
}

#ifdef SIMULATOR
bool maker_lite_storage_dump_test_frame(void)
{
    const char *path = getenv("MAKER_LITE_TEST_FRAME");
    struct viewport *viewport;
    unsigned char row[LCD_WIDTH * 3];
    char header[32];
    int header_size;
    int fd;
    int x;
    int y;

    if (!path || !path[0])
        return false;
    viewport = *(rb->screens[SCREEN_MAIN]->current_viewport);
    if (!viewport || !viewport->buffer || !viewport->buffer->get_address_fn)
        return false;
    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    header_size = rb->snprintf(header, sizeof(header), "P6\n%d %d\n255\n",
                               LCD_WIDTH, LCD_HEIGHT);
    if (rb->write(fd, header, header_size) != header_size)
    {
        rb->close(fd);
        return false;
    }
    for (y = 0; y < LCD_HEIGHT; ++y)
    {
        for (x = 0; x < LCD_WIDTH; ++x)
        {
            fb_data pixel =
                *(fb_data *)viewport->buffer->get_address_fn(x, y);

            row[x * 3] = FB_UNPACK_RED(pixel);
            row[x * 3 + 1] = FB_UNPACK_GREEN(pixel);
            row[x * 3 + 2] = FB_UNPACK_BLUE(pixel);
        }
        if (rb->write(fd, row, sizeof(row)) != sizeof(row))
        {
            rb->close(fd);
            return false;
        }
    }
    rb->close(fd);
    return true;
}
#endif
