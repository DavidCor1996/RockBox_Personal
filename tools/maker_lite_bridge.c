/*
 * Stable, allocation-free host ABI used by Rockpod's live Maker Lite preview.
 * The pack memory remains owned by the caller for the lifetime of a session.
 */

#include "maker_lite.h"

#include <string.h>

static struct ml_level bridge_level;
static struct ml_world bridge_world;
static int bridge_opened;

int ml_bridge_open(const void *data, size_t size)
{
    enum ml_pack_error error;

    memset(&bridge_level, 0, sizeof(bridge_level));
    memset(&bridge_world, 0, sizeof(bridge_world));
    bridge_opened = 0;
    error = ml_pack_open(data, size, &bridge_level);
    if (error != ML_PACK_OK)
        return -(int)error;
    if (!ml_world_init(&bridge_world, &bridge_level))
        return -100;
    bridge_opened = 1;
    return 0;
}

int ml_bridge_reset(void)
{
    if (!bridge_opened)
        return -1;
    return ml_world_init(&bridge_world, &bridge_level) ? 0 : -1;
}

int ml_bridge_tick(uint32_t input)
{
    if (!bridge_opened)
        return -1;
    ml_world_tick(&bridge_world, input);
    return 0;
}

int ml_bridge_snapshot(struct ml_snapshot *snapshot)
{
    if (!bridge_opened || !snapshot)
        return -1;
    ml_world_snapshot(&bridge_world, snapshot);
    return 0;
}

int ml_bridge_entity(unsigned index, int *x, int *y, int *alive,
                     int *kind, int *render_cell)
{
    struct ml_entity entity;

    if (!bridge_opened || index >= bridge_level.entity_count ||
        !ml_world_entity(&bridge_world, index, &entity))
        return -1;
    if (x)
        *x = entity.x;
    if (y)
        *y = entity.y;
    if (alive)
        *alive = bridge_world.entity_alive[index] ? 1 : 0;
    if (kind)
        *kind = entity.kind;
    if (render_cell)
        *render_cell = entity.render_cell;
    return 0;
}

int ml_bridge_counts(unsigned *entities, unsigned *events, unsigned *paths)
{
    if (!bridge_opened)
        return -1;
    if (entities)
        *entities = bridge_level.entity_count;
    if (events)
        *events = bridge_level.event_count;
    if (paths)
        *paths = bridge_level.path_count;
    return 0;
}

int ml_bridge_event_fired(unsigned index)
{
    if (!bridge_opened || index >= bridge_level.event_count)
        return -1;
    return !!(bridge_world.event_fired[index >> 3] &
              (1u << (index & 7)));
}

int ml_bridge_pending_effect(void)
{
    return bridge_opened ? bridge_world.pending_effect : -1;
}

int ml_bridge_level_flags(void)
{
    return bridge_opened ? bridge_level.flags : -1;
}

unsigned ml_bridge_dynamic_count(void)
{
    unsigned count = bridge_world.projectile_active ? 1u : 0u;
    unsigned i;

    if (!bridge_opened)
        return 0;
    for (i = 0; i < ML_MAX_LOOSE_RINGS; ++i)
        if (bridge_world.loose_ring_active[i])
            count++;
    return count;
}

int ml_bridge_dynamic(unsigned index, int *x, int *y, int *kind)
{
    unsigned cursor = 0;
    unsigned i;

    if (!bridge_opened)
        return -1;
    if (bridge_world.projectile_active)
    {
        if (index == 0)
        {
            if (x)
                *x = ml_fixed_to_int(bridge_world.projectile_x);
            if (y)
                *y = ml_fixed_to_int(bridge_world.projectile_y);
            if (kind)
                *kind = bridge_world.projectile_kind;
            return 0;
        }
        cursor = 1;
    }
    for (i = 0; i < ML_MAX_LOOSE_RINGS; ++i)
    {
        if (!bridge_world.loose_ring_active[i])
            continue;
        if (cursor++ != index)
            continue;
        if (x)
            *x = ml_fixed_to_int(bridge_world.loose_ring_x[i]);
        if (y)
            *y = ml_fixed_to_int(bridge_world.loose_ring_y[i]);
        if (kind)
            *kind = ML_ENTITY_COLLECTIBLE;
        return 0;
    }
    return -1;
}

uint32_t ml_bridge_digest(void)
{
    return bridge_opened ? ml_world_digest(&bridge_world) : 0;
}

size_t ml_bridge_snapshot_size(void)
{
    return sizeof(struct ml_snapshot);
}
