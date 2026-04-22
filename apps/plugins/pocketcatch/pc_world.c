#include "pocketcatch.h"

/*
 * Pallet Town block layout sourced from pret/pokered:
 * maps/PalletTown.blk (10x9 blocks), plus object positions from
 * data/maps/objects/PalletTown.asm.
 *
 * We render these original block ids with a lightweight in-plugin
 * renderer instead of loading a large bitmap, which keeps the iPod
 * build stable while still using the real Gen 1 map layout.
 */

static const unsigned char pc_pallet_blocks[PC_WORLD_H][PC_WORLD_W] = {
    { 0x52, 0x4f, 0x52, 0x52, 0x4f, 0x0b, 0x50, 0x52, 0x52, 0x50 },
    { 0x4e, 0x01, 0x38, 0x39, 0x01, 0x01, 0x38, 0x39, 0x01, 0x4d },
    { 0x4e, 0x08, 0x3c, 0x3d, 0x01, 0x08, 0x3c, 0x3d, 0x01, 0x4d },
    { 0x4e, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x4d },
    { 0x4e, 0x01, 0x77, 0x56, 0x01, 0x0c, 0x0d, 0x0e, 0x01, 0x4d },
    { 0x4e, 0x01, 0x74, 0x74, 0x01, 0x10, 0x3a, 0x00, 0x01, 0x4d },
    { 0x4e, 0x01, 0x01, 0x01, 0x01, 0x77, 0x56, 0x77, 0x31, 0x4d },
    { 0x4e, 0x0a, 0x1d, 0x1e, 0x31, 0x74, 0x74, 0x0a, 0x31, 0x4d },
    { 0x50, 0x0a, 0x65, 0x64, 0x61, 0x61, 0x61, 0x61, 0x61, 0x4f },
};

static fb_data pc_world_trainer_pixels[4][PC_WORLD_WALK_FRAMES]
                                      [PC_WORLD_TRAINER_MAX_W * PC_WORLD_TRAINER_MAX_H];
static fb_data pc_world_creature_pixels[PC_WORLD_MAX_SPAWNS]
                                       [PC_WORLD_CREATURE_MAX_W * PC_WORLD_CREATURE_MAX_H];

static void clear_bitmap(struct pc_asset_bitmap *asset)
{
    rb->memset(&asset->bmp, 0, sizeof(asset->bmp));
    asset->loaded = false;
    asset->external = false;
    asset->path[0] = '\0';
}

static void set_banner(struct pc_world_state *world,
                       const char *line1, const char *line2)
{
    rb->strlcpy(world->banner.line1, line1, sizeof(world->banner.line1));
    rb->strlcpy(world->banner.line2, line2, sizeof(world->banner.line2));
}

static int block_screen_x(int block_x)
{
    return PC_WORLD_ORIGIN_X + block_x * PC_WORLD_TILE_SIZE;
}

static int block_screen_y(int block_y)
{
    return PC_WORLD_ORIGIN_Y + block_y * PC_WORLD_TILE_SIZE;
}

static int block_center_x(int block_x)
{
    return block_screen_x(block_x) + PC_WORLD_TILE_SIZE / 2;
}

static int block_center_y(int block_y)
{
    return block_screen_y(block_y) + PC_WORLD_TILE_SIZE / 2;
}

static bool block_walkable(unsigned char block_id)
{
    switch (block_id)
    {
        case 0x52:
        case 0x4f:
        case 0x50:
        case 0x4d:
        case 0x4e:
        case 0x0b:
        case 0x0a:
        case 0x38:
        case 0x39:
        case 0x3c:
        case 0x3d:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x10:
        case 0x3a:
        case 0x00:
        case 0x1d:
        case 0x1e:
        case 0x65:
        case 0x64:
        case 0x61:
            return false;

        default:
            return true;
    }
}

static bool point_walkable(const struct pc_world_state *world, int x, int y)
{
    int local_x = x - PC_WORLD_ORIGIN_X;
    int local_y = y - PC_WORLD_ORIGIN_Y;
    int tx;
    int ty;

    (void)world;

    if (local_x < 0 || local_y < 0)
        return false;

    tx = local_x / PC_WORLD_TILE_SIZE;
    ty = local_y / PC_WORLD_TILE_SIZE;
    if (tx < 0 || ty < 0 || tx >= PC_WORLD_W || ty >= PC_WORLD_H)
        return false;

    return block_walkable(world->tiles[ty][tx]);
}

static bool player_walkable(const struct pc_world_state *world, int x, int y)
{
    int foot_y = y + 7;

    return point_walkable(world, x, foot_y) &&
           point_walkable(world, x - 5, foot_y) &&
           point_walkable(world, x + 5, foot_y);
}

static void init_assets(struct pc_world_state *world)
{
    int heading;
    int frame;
    int i;

    rb->memset(&world->assets, 0, sizeof(world->assets));
    for (heading = 0; heading < 4; ++heading)
    {
        for (frame = 0; frame < PC_WORLD_WALK_FRAMES; ++frame)
        {
            world->assets.trainer[heading][frame].pixels =
                pc_world_trainer_pixels[heading][frame];
            world->assets.trainer[heading][frame].capacity = PC_WORLD_TRAINER_BYTES;
            clear_bitmap(&world->assets.trainer[heading][frame]);
            pc_assets_load_world_trainer(&world->assets.trainer[heading][frame],
                                         heading, frame);
        }
    }

    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
    {
        world->assets.creature[i].pixels = pc_world_creature_pixels[i];
        world->assets.creature[i].capacity = PC_WORLD_CREATURE_BYTES;
        clear_bitmap(&world->assets.creature[i]);
    }
}

static void build_pallet_layout(struct pc_world_state *world)
{
    int x;
    int y;

    for (y = 0; y < PC_WORLD_H; ++y)
    {
        for (x = 0; x < PC_WORLD_W; ++x)
            world->tiles[y][x] = pc_pallet_blocks[y][x];
    }

    /* Start just outside the player's house door. */
    world->home_x = block_center_x(3);
    world->home_y = block_center_y(3) + 4;
    world->spawn_x = world->home_x;
    world->spawn_y = world->home_y;
}

static void place_spawn(struct pc_world_state *world, int slot,
                        int block_x, int block_y, int species_index)
{
    struct pc_world_spawn *spawn = &world->spawns[slot];

    spawn->active = true;
    spawn->species_index = species_index % MAX(1, pc_assets_get_creature_count());
    if (spawn->species_index < 0)
        spawn->species_index += pc_assets_get_creature_count();
    spawn->x = block_center_x(block_x);
    spawn->y = block_center_y(block_y) + 3;
    spawn->step = 0;
    spawn->dir_x = (slot & 1) ? 1 : -1;
    spawn->dir_y = 0;
    pc_assets_load_world_creature(&world->assets.creature[slot], spawn->species_index);
}

static void init_spawns(struct pc_world_state *world)
{
    place_spawn(world, 0, 1, 3, 0);
    place_spawn(world, 1, 5, 3, 6);
    place_spawn(world, 2, 8, 3, 3);
    place_spawn(world, 3, 4, 6, 9);
}

static void maybe_move_spawn(struct pc_world_state *world, struct pc_world_spawn *spawn)
{
    int next_x;
    int next_y;

    if (!spawn->active)
        return;

    spawn->step++;
    if ((spawn->step % 24) == 0)
    {
        spawn->dir_x = -spawn->dir_x;
        if ((spawn->step / 24) & 1)
            spawn->dir_y = spawn->dir_x;
        else
            spawn->dir_y = 0;
    }

    if ((world->frame & 3) != 0)
        return;

    next_x = spawn->x + spawn->dir_x;
    next_y = spawn->y + spawn->dir_y;
    if (point_walkable(world, next_x, next_y + 6))
    {
        spawn->x = next_x;
        spawn->y = next_y;
    }
    else
    {
        spawn->dir_x = -spawn->dir_x;
        spawn->dir_y = 0;
    }
}

void pc_world_init(struct pc_world_state *world)
{
    rb->memset(world, 0, sizeof(*world));
    init_assets(world);
    build_pallet_layout(world);
    world->player_x = world->spawn_x;
    world->player_y = world->spawn_y;
    world->heading = PC_HEADING_S;
    world->walk_frame = 1;
    world->last_encounter_slot = -1;
    world->pending_species_index = -1;
    init_spawns(world);
    set_banner(world, "Pallet Town", "Red map layout, exits blocked");
}

void pc_world_teardown(struct pc_world_state *world)
{
    int heading;
    int frame;
    int i;

    for (heading = 0; heading < 4; ++heading)
    {
        for (frame = 0; frame < PC_WORLD_WALK_FRAMES; ++frame)
            clear_bitmap(&world->assets.trainer[heading][frame]);
    }
    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
        clear_bitmap(&world->assets.creature[i]);
}

void pc_world_update(struct pc_world_state *world, const struct pc_world_command *command)
{
    int next_x = world->player_x;
    int next_y = world->player_y;
    bool moved = false;
    int i;

    world->frame++;

    if (command->move_x < 0)
    {
        next_x -= PC_WORLD_STEP_PX;
        world->heading = PC_HEADING_W;
    }
    else if (command->move_x > 0)
    {
        next_x += PC_WORLD_STEP_PX;
        world->heading = PC_HEADING_E;
    }
    else if (command->move_y < 0)
    {
        next_y -= PC_WORLD_STEP_PX;
        world->heading = PC_HEADING_N;
    }
    else if (command->move_y > 0)
    {
        next_y += PC_WORLD_STEP_PX;
        world->heading = PC_HEADING_S;
    }

    if (player_walkable(world, next_x, next_y))
    {
        moved = (next_x != world->player_x) || (next_y != world->player_y);
        world->player_x = next_x;
        world->player_y = next_y;
    }
    world->moving = moved;
    if (world->moving)
    {
        world->walk_tick++;
        if ((world->walk_tick % 4) == 0)
            world->walk_frame = (world->walk_frame + 1) % PC_WORLD_WALK_FRAMES;
    }
    else
    {
        world->walk_tick = 0;
        world->walk_frame = 1;
    }

    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
    {
        struct pc_world_spawn *spawn = &world->spawns[i];
        int dx;
        int dy;

        maybe_move_spawn(world, spawn);
        if (!spawn->active || world->pending_encounter)
            continue;

        dx = world->player_x - spawn->x;
        dy = world->player_y - spawn->y;
        if (dx * dx + dy * dy <= 14 * 14)
        {
            const struct pc_creature_def *creature =
                pc_assets_get_creature(spawn->species_index);

            world->pending_encounter = true;
            world->pending_species_index = spawn->species_index;
            world->last_encounter_slot = i;
            spawn->active = false;
            if (creature != NULL)
            {
                char line1[PC_BANNER_LINE_CHARS];

                rb->snprintf(line1, sizeof(line1), "%s darted out", creature->name);
                set_banner(world, line1, "Encounter loading");
            }
        }
    }
}

void pc_world_finish_encounter(struct pc_world_state *world,
                               enum pc_catch_outcome outcome,
                               int species_index)
{
    int count = pc_assets_get_creature_count();

    world->pending_encounter = false;
    world->pending_species_index = -1;

    if (world->last_encounter_slot >= 0 &&
        world->last_encounter_slot < PC_WORLD_MAX_SPAWNS)
    {
        int next_species = species_index;
        static const unsigned char respawn_blocks[PC_WORLD_MAX_SPAWNS][2] = {
            { 1, 3 }, { 5, 3 }, { 8, 3 }, { 4, 6 }
        };

        if (outcome == PC_CATCH_OUTCOME_CAUGHT && count > 0)
            next_species = rb->rand() % count;

        place_spawn(world,
                    world->last_encounter_slot,
                    respawn_blocks[world->last_encounter_slot][0],
                    respawn_blocks[world->last_encounter_slot][1],
                    next_species);
    }

    if (outcome == PC_CATCH_OUTCOME_CAUGHT)
        set_banner(world, "Caught it", "Keep exploring Pallet Town");
    else
        set_banner(world, "It broke out", "Walk into it again to retry");
}
