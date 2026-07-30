/*
 * Host-side deterministic exercise for the shared Maker Lite core.
 */

#include "maker_lite.h"

#include <stdio.h>
#include <stdlib.h>

#define FEATURE_JUMP       (1u << 0)
#define FEATURE_RUN        (1u << 1)
#define FEATURE_SWORD      (1u << 2)
#define FEATURE_ITEM       (1u << 3)
#define FEATURE_ROLL       (1u << 4)
#define FEATURE_SPINDASH   (1u << 5)
#define FEATURE_COMPLETE   (1u << 6)
#define FEATURE_EVENT      (1u << 7)
#define FEATURE_PATH       (1u << 8)
#define FEATURE_SURFACE    (1u << 9)
#define FEATURE_ENEMY      (1u << 10)
#define FEATURE_ROOM       (1u << 11)

static void *read_file(const char *path, size_t *size)
{
    FILE *file;
    long length;
    void *data;

    file = fopen(path, "rb");
    if (!file)
        return NULL;
    if (fseek(file, 0, SEEK_END) != 0 ||
        (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }
    data = malloc((size_t)length);
    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length)
    {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size = (size_t)length;
    return data;
}

static uint32_t scripted_input(const struct ml_world *world)
{
    switch (world->level->ruleset)
    {
        case ML_RULESET_MARIO:
            if (world->tick == 20 || world->tick == 105)
                return ML_INPUT_RIGHT | ML_INPUT_PRIMARY |
                       ML_INPUT_SECONDARY;
            return ML_INPUT_RIGHT | ML_INPUT_SECONDARY;
        case ML_RULESET_ZELDA:
            if (world->tick == 15)
                return ML_INPUT_PRIMARY;
            if (world->tick == 35)
                return ML_INPUT_SECONDARY;
            if (world->tick == 55)
                return ML_INPUT_NEXT;
            if (ml_fixed_to_int(world->player.x) < 104 &&
                ml_fixed_to_int(world->player.y) < 80)
                return ML_INPUT_RIGHT;
            if (ml_fixed_to_int(world->player.y) < 200)
                return ML_INPUT_DOWN;
            return ML_INPUT_RIGHT;
        case ML_RULESET_SONIC:
            if (world->player.grounded && world->tick >= 90 &&
                world->tick < 150 &&
                world->player.spindash_charge < 3)
                return ML_INPUT_DOWN |
                       ((world->tick & 1) ? ML_INPUT_PRIMARY : 0);
            if (world->player.grounded && world->tick % 90 == 0)
                return ML_INPUT_RIGHT | ML_INPUT_PRIMARY;
            return ML_INPUT_RIGHT;
        default:
            return 0;
    }
}

int main(int argc, char **argv)
{
    struct ml_level level;
    struct ml_world world;
    struct ml_snapshot snapshot;
    enum ml_pack_error error;
    size_t size = 0;
    void *data;
    unsigned frames = 600;
    unsigned frame;
    unsigned features = 0;

    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "usage: %s PACK [FRAMES]\n", argv[0]);
        return 2;
    }
    if (argc == 3)
        frames = (unsigned)strtoul(argv[2], NULL, 10);
    data = read_file(argv[1], &size);
    if (!data)
    {
        fprintf(stderr, "unable to read %s\n", argv[1]);
        return 1;
    }
    error = ml_pack_open(data, size, &level);
    if (error != ML_PACK_OK)
    {
        fprintf(stderr, "%s: %s\n", argv[1],
                ml_pack_error_string(error));
        free(data);
        return 1;
    }
    if (!ml_world_init(&world, &level))
    {
        fprintf(stderr, "world initialization failed\n");
        free(data);
        return 1;
    }
    for (frame = 0; frame < frames && !world.complete; ++frame)
    {
        ml_world_tick(&world, scripted_input(&world));
        switch (world.player.action)
        {
            case ML_ACTION_JUMP: features |= FEATURE_JUMP; break;
            case ML_ACTION_RUN: features |= FEATURE_RUN; break;
            case ML_ACTION_SWORD: features |= FEATURE_SWORD; break;
            case ML_ACTION_ITEM: features |= FEATURE_ITEM; break;
            case ML_ACTION_ROLL: features |= FEATURE_ROLL; break;
            case ML_ACTION_SPINDASH: features |= FEATURE_SPINDASH; break;
            case ML_ACTION_COMPLETE: features |= FEATURE_COMPLETE; break;
            default: break;
        }
        if (world.level->ruleset == ML_RULESET_SONIC)
        {
            if (world.player.rolling)
                features |= FEATURE_ROLL;
            if (world.player.ground_speed > ml_int_to_fixed(2) ||
                world.player.ground_speed < ml_int_to_fixed(-2))
                features |= FEATURE_RUN;
            if (world.player.surface_attached)
                features |= FEATURE_SURFACE;
        }
        if (world.room_transition_ticks > 0)
            features |= FEATURE_ROOM;
        for (unsigned index = 0;
             index < world.level->event_count; ++index)
        {
            if (world.event_fired[index >> 3] & (1u << (index & 7)))
                features |= FEATURE_EVENT;
        }
        for (unsigned index = 0;
             index < world.level->entity_count; ++index)
        {
            struct ml_entity entity;

            if (ml_level_entity(world.level, index, &entity) &&
                (world.entity_x[index] != ml_int_to_fixed(entity.x) ||
                 world.entity_y[index] != ml_int_to_fixed(entity.y)))
            {
                if (entity.kind == ML_ENTITY_BLOCK)
                    features |= FEATURE_PATH;
                else if (entity.kind == ML_ENTITY_ENEMY)
                    features |= FEATURE_ENEMY;
            }
        }
    }
    ml_world_snapshot(&world, &snapshot);
    printf(
        "ruleset=%u tick=%lu x=%ld y=%ld vx=%ld vy=%ld "
        "action=%u grounded=%u complete=%u collectibles=%d rings=%d "
        "keys=%d health=%d pack_crc=%08lx state_crc=%08lx "
        "world_digest=%08lx features=%03x\n",
        level.ruleset,
        (unsigned long)snapshot.tick,
        (long)snapshot.player_x,
        (long)snapshot.player_y,
        (long)snapshot.player_vx,
        (long)snapshot.player_vy,
        snapshot.action,
        snapshot.grounded,
        snapshot.complete,
        snapshot.collectibles,
        snapshot.rings,
        snapshot.keys,
        snapshot.health,
        (unsigned long)ml_crc32(data, size),
        (unsigned long)ml_crc32(&snapshot, sizeof(snapshot)),
        (unsigned long)ml_world_digest(&world),
        features);
    free(data);
    return 0;
}
