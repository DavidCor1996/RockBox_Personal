#include "pocketcatch.h"
#include "pc_red_gfx.h"
#include "pc_red_dojo_gfx.h"
#include "pc_red_pokecenter_gfx.h"
#include "lib/xlcd.h"

#define PC_GB_LIGHT    LCD_RGBPACK(0xe0, 0xf8, 0xd0)
#define PC_GB_MID      LCD_RGBPACK(0x88, 0xc0, 0x70)
#define PC_GB_DARK     LCD_RGBPACK(0x34, 0x68, 0x56)
#define PC_GB_DEEP     LCD_RGBPACK(0x08, 0x18, 0x20)
#define PC_GB_PANEL    LCD_RGBPACK(0xf0, 0xf7, 0xe0)

static const fb_data pc_gb_palette[4] = {
    PC_GB_DEEP,
    PC_GB_DARK,
    PC_GB_MID,
    PC_GB_LIGHT,
};

#define PC_WORLD_TILESET_MAX 96
#define PC_WORLD_TILE_CACHE_PIXELS (PC_WORLD_SUBTILE_SIZE * PC_WORLD_SUBTILE_SIZE)
#define PC_WORLD_NPC_TRAINERS 3

static fb_data pc_world_tile_cache[PC_WORLD_TILESET_MAX][PC_WORLD_TILE_CACHE_PIXELS];
static fb_data pc_world_npc_pixels[PC_WORLD_NPC_TRAINERS][4][PC_WORLD_WALK_FRAMES]
                                  [PC_WORLD_TRAINER_MAX_W * PC_WORLD_TRAINER_MAX_H];
static enum pc_world_scene pc_world_cached_scene = (enum pc_world_scene)-1;
static int pc_world_cached_tile_count;
static struct pc_asset_bitmap pc_world_npc_assets[PC_WORLD_NPC_TRAINERS][4][PC_WORLD_WALK_FRAMES];

static void draw_asset_bitmap(const struct bitmap *bmp, int x, int y)
{
#ifdef HAVE_LCD_COLOR
    rb->lcd_bitmap_transparent((const fb_data *)bmp->data, x, y,
                               bmp->width, bmp->height);
#else
    rb->lcd_bitmap_part((const fb_data *)bmp->data, 0, 0, bmp->width,
                        x, y, bmp->width, bmp->height);
#endif
}

static const struct pc_asset_bitmap *ensure_npc_trainer_asset(int trainer_index,
                                                              int heading, int frame);

static void draw_text_small(int x, int y, fb_data color, const char *text)
{
    int old_mode = rb->lcd_get_drawmode();

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(color);
    rb->lcd_putsxy(x, y, text);
    rb->lcd_set_drawmode(old_mode);
    rb->lcd_setfont(FONT_UI);
}

static void fit_text_small(char *buffer, size_t buffer_size,
                           const char *text, int max_width)
{
    static const char ellipsis[] = "...";
    int text_width;
    int ellipsis_width;
    size_t len;

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->strlcpy(buffer, text, buffer_size);
    rb->lcd_getstringsize(buffer, &text_width, NULL);
    if (text_width <= max_width)
    {
        rb->lcd_setfont(FONT_UI);
        return;
    }

    rb->lcd_getstringsize(ellipsis, &ellipsis_width, NULL);
    len = rb->strlen(buffer);
    while (len > 0)
    {
        buffer[--len] = '\0';
        rb->lcd_getstringsize(buffer, &text_width, NULL);
        if (text_width + ellipsis_width <= max_width)
        {
            rb->strlcat(buffer, ellipsis, buffer_size);
            rb->lcd_setfont(FONT_UI);
            return;
        }
    }

    buffer[0] = '\0';
    rb->lcd_setfont(FONT_UI);
}

static void fill_capsule(int x, int y, int w, int h, fb_data color)
{
    int radius = h / 2;

    rb->lcd_set_foreground(color);
    rb->lcd_fillrect(x + radius, y, w - 2 * radius, h);
    xlcd_fillcircle(x + radius, y + radius, radius);
    xlcd_fillcircle(x + w - radius - 1, y + radius, radius);
}

static void draw_flat_panel(int x, int y, int w, int h,
                            fb_data fill, fb_data outline, fb_data accent)
{
    rb->lcd_set_foreground(LCD_RGBPACK(0xa8, 0xb8, 0x98));
    rb->lcd_fillrect(x + 2, y + 2, w, h);
    rb->lcd_set_foreground(fill);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(accent);
    rb->lcd_fillrect(x, y, w, 2);
    rb->lcd_set_foreground(outline);
    rb->lcd_drawrect(x, y, w, h);
}

static int block_screen_x(const struct pc_world_state *world, int tx)
{
    return world->origin_x + tx * PC_WORLD_TILE_SIZE;
}

static int block_screen_y(const struct pc_world_state *world, int ty)
{
    return world->origin_y + ty * PC_WORLD_TILE_SIZE;
}

static bool outdoor_scene(enum pc_world_scene scene)
{
    return scene == PC_WORLD_SCENE_PALLET ||
           scene == PC_WORLD_SCENE_ROUTE1_SOUTH ||
           scene == PC_WORLD_SCENE_VIRIDIAN_SOUTH ||
           scene == PC_WORLD_SCENE_ROUTE2_SOUTH ||
           scene == PC_WORLD_SCENE_ROUTE21_NORTH;
}

static void draw_spawn(const struct pc_world_state *world, int index)
{
    const struct pc_world_spawn *spawn = &world->spawns[index];
    const struct pc_creature_def *creature;
    int screen_x;
    int screen_y;
    int shadow_w = PC_WORLD_SCALE(12);

    if (!spawn->active)
        return;

    screen_x = world->origin_x + spawn->x;
    screen_y = world->origin_y + spawn->y;
    creature = pc_assets_get_creature(spawn->species_index);
    if (world->assets.creature[index].loaded)
        shadow_w = MAX(PC_WORLD_SCALE(12), world->assets.creature[index].bmp.width / 2);

    fill_capsule(screen_x - shadow_w / 2,
                 screen_y + PC_WORLD_FOOT_Y_OFFSET,
                 shadow_w,
                 PC_WORLD_SCALE(4),
                 LCD_RGBPACK(0x60, 0x70, 0x58));

    if (world->assets.creature[index].loaded)
    {
        draw_asset_bitmap(&world->assets.creature[index].bmp,
                          screen_x - world->assets.creature[index].bmp.width / 2,
                          screen_y - world->assets.creature[index].bmp.height / 2);
    }
    else if (creature != NULL)
    {
        rb->lcd_set_foreground(creature->primary);
        xlcd_fillcircle(screen_x, screen_y, PC_WORLD_SCALE(10));
        rb->lcd_set_foreground(creature->accent);
        rb->lcd_fillrect(screen_x - PC_WORLD_SCALE(5),
                         screen_y - PC_WORLD_SCALE(2),
                         PC_WORLD_SCALE(10),
                         PC_WORLD_SCALE(4));
    }
}

static void draw_creature_panel_fallback(const struct pc_creature_def *creature,
                                         int x, int y, int w, int h)
{
    int center_x;
    int center_y;
    int body_w;
    int body_h;

    if (creature == NULL)
        return;

    center_x = x + w / 2;
    center_y = y + h / 2 - 2;
    body_w = MIN(w - 20, 32);
    body_h = MIN(h - 24, 30);

    rb->lcd_set_foreground(LCD_RGBPACK(0xb8, 0xc8, 0xa8));
    rb->lcd_fillrect(center_x - body_w / 2 - 6, y + h - 18, body_w + 12, 4);

    rb->lcd_set_foreground(creature->primary);
    xlcd_fillcircle(center_x, center_y - 8, 12);
    rb->lcd_fillrect(center_x - body_w / 2, center_y - 4, body_w, body_h);

    rb->lcd_set_foreground(creature->secondary);
    rb->lcd_fillrect(center_x - body_w / 2 + 4, center_y + 2, body_w - 8, body_h / 3);
    xlcd_fillcircle(center_x - 8, center_y - 14, 5);
    xlcd_fillcircle(center_x + 8, center_y - 14, 5);

    rb->lcd_set_foreground(creature->accent);
    rb->lcd_fillrect(center_x - 10, center_y - 10, 5, 3);
    rb->lcd_fillrect(center_x + 5, center_y - 10, 5, 3);
    rb->lcd_fillrect(center_x - 6, center_y + 8, 12, 3);
}

static void scale_tile_to_cache(const unsigned char tiles[][8][8], int tile_count,
                                int tile_id)
{
    int py;
    int px;
    int sy;
    int sx;
    fb_data *dst;

    if (tile_id < 0 || tile_id >= tile_count || tile_id >= PC_WORLD_TILESET_MAX)
        return;

    dst = pc_world_tile_cache[tile_id];
    for (py = 0; py < 8; ++py)
    {
        for (px = 0; px < 8; ++px)
        {
            fb_data color = pc_gb_palette[tiles[tile_id][py][px]];

            for (sy = 0; sy < PC_WORLD_PIXEL_SCALE; ++sy)
            {
                for (sx = 0; sx < PC_WORLD_PIXEL_SCALE; ++sx)
                {
                    dst[(py * PC_WORLD_PIXEL_SCALE + sy) * PC_WORLD_SUBTILE_SIZE
                        + (px * PC_WORLD_PIXEL_SCALE + sx)] = color;
                }
            }
        }
    }
}

static void select_tileset(enum pc_world_scene scene,
                           const unsigned char (**tiles)[8][8],
                           const unsigned char (**blocks)[4][4],
                           int *tile_count,
                           int *block_count)
{
    if (outdoor_scene(scene))
    {
        *tiles = pc_red_overworld_tiles;
        *blocks = pc_red_overworld_blocks;
        *tile_count = PC_RED_TILE_COUNT;
        *block_count = PC_RED_BLOCK_COUNT;
    }
    else if (scene == PC_WORLD_SCENE_OAKS_LAB ||
             scene == PC_WORLD_SCENE_VIRIDIAN_GYM)
    {
        *tiles = pc_red_dojo_tiles;
        *blocks = pc_red_dojo_blocks;
        *tile_count = PC_RED_DOJO_TILE_COUNT;
        *block_count = PC_RED_DOJO_BLOCK_COUNT;
    }
    else if (scene == PC_WORLD_SCENE_VIRIDIAN_MART ||
             scene == PC_WORLD_SCENE_VIRIDIAN_POKECENTER)
    {
        *tiles = pc_red_pokecenter_tiles;
        *blocks = pc_red_pokecenter_blocks;
        *tile_count = PC_RED_POKECENTER_TILE_COUNT;
        *block_count = PC_RED_POKECENTER_BLOCK_COUNT;
    }
    else
    {
        *tiles = pc_red_house_tiles;
        *blocks = pc_red_house_blocks;
        *tile_count = PC_RED_HOUSE_TILE_COUNT;
        *block_count = PC_RED_HOUSE_BLOCK_COUNT;
    }
}

static void ensure_tile_cache(enum pc_world_scene scene)
{
    const unsigned char (*tiles)[8][8];
    const unsigned char (*blocks)[4][4];
    int tile_count;
    int block_count;
    int tile_id;

    if (pc_world_cached_scene == scene)
        return;

    select_tileset(scene, &tiles, &blocks, &tile_count, &block_count);
    for (tile_id = 0; tile_id < tile_count; ++tile_id)
        scale_tile_to_cache(tiles, tile_count, tile_id);

    pc_world_cached_scene = scene;
    pc_world_cached_tile_count = tile_count;
    (void)blocks;
    (void)block_count;
}

static void draw_cached_tile(unsigned char tile_id, int screen_x, int screen_y)
{
    int src_x = 0;
    int src_y = 0;
    int width = PC_WORLD_SUBTILE_SIZE;
    int height = PC_WORLD_SUBTILE_SIZE;

    if (tile_id >= pc_world_cached_tile_count)
        return;

    if (screen_x < 0)
    {
        src_x = -screen_x;
        width += screen_x;
        screen_x = 0;
    }
    if (screen_y < 0)
    {
        src_y = -screen_y;
        height += screen_y;
        screen_y = 0;
    }
    if (screen_x + width > LCD_WIDTH)
        width = LCD_WIDTH - screen_x;
    if (screen_y + height > LCD_HEIGHT)
        height = LCD_HEIGHT - screen_y;
    if (width <= 0 || height <= 0)
        return;

    rb->lcd_bitmap_part(pc_world_tile_cache[tile_id],
                        src_x, src_y,
                        PC_WORLD_SUBTILE_SIZE,
                        screen_x, screen_y,
                        width, height);
}

static void draw_map(const struct pc_world_state *world)
{
    const unsigned char (*tiles)[8][8];
    const unsigned char (*blocks)[4][4];
    int tile_count;
    int block_count;
    int by;
    int bx;
    int start_by;
    int end_by;
    int start_bx;
    int end_bx;
    int sub_y;
    int sub_x;

    ensure_tile_cache(world->scene);
    select_tileset(world->scene, &tiles, &blocks, &tile_count, &block_count);
    (void)tiles;
    (void)tile_count;
    ((struct pc_world_state *)world)->map_dirty = false;

    rb->lcd_set_background(PC_GB_LIGHT);
    rb->lcd_clear_display();

    start_bx = MAX(0, (-world->origin_x) / PC_WORLD_TILE_SIZE);
    start_by = MAX(0, (-world->origin_y) / PC_WORLD_TILE_SIZE);
    end_bx = MIN(world->map_w, ((-world->origin_x) + LCD_WIDTH + PC_WORLD_TILE_SIZE - 1) /
                               PC_WORLD_TILE_SIZE + 1);
    end_by = MIN(world->map_h, ((-world->origin_y) + LCD_HEIGHT + PC_WORLD_TILE_SIZE - 1) /
                               PC_WORLD_TILE_SIZE + 1);

    for (by = start_by; by < end_by; ++by)
    {
        for (bx = start_bx; bx < end_bx; ++bx)
        {
            int block_x = block_screen_x(world, bx);
            int block_y = block_screen_y(world, by);
            unsigned char block_id = world->tiles[by][bx];

            if (block_id >= block_count)
                continue;
            if (block_x >= LCD_WIDTH || block_y >= LCD_HEIGHT ||
                block_x + PC_WORLD_TILE_SIZE <= 0 || block_y + PC_WORLD_TILE_SIZE <= 0)
            {
                continue;
            }

            for (sub_y = 0; sub_y < 4; ++sub_y)
            {
                for (sub_x = 0; sub_x < 4; ++sub_x)
                {
                    draw_cached_tile(blocks[block_id][sub_y][sub_x],
                                     block_x + sub_x * PC_WORLD_SUBTILE_SIZE,
                                     block_y + sub_y * PC_WORLD_SUBTILE_SIZE);
                }
            }
        }
    }
}

static const unsigned char (*select_player_frame(const struct pc_world_state *world,
                                                 bool *flip_x))[16]
{
    bool walking = world->moving && world->walk_frame != 1;

    *flip_x = false;

    switch (world->heading)
    {
        case PC_HEADING_N:
            *flip_x = world->walk_frame == 2;
            return pc_red_player_frames[walking ? 4 : 1];

        case PC_HEADING_E:
            *flip_x = true;
            return pc_red_player_frames[walking ? 5 : 2];

        case PC_HEADING_W:
            return pc_red_player_frames[walking ? 5 : 2];

        case PC_HEADING_S:
        default:
            *flip_x = world->walk_frame == 2;
            return pc_red_player_frames[walking ? 3 : 0];
    }
}

static void draw_red_sprite(const unsigned char frame[16][16],
                            int x, int y, bool flip_x)
{
    int py;
    int px;
    int sy;
    int sx;

    for (py = 0; py < 16; ++py)
    {
        for (px = 0; px < 16; ++px)
        {
            int src_x = flip_x ? (15 - px) : px;
            unsigned char shade = frame[py][src_x];

            if (shade == 3)
                continue;

            rb->lcd_set_foreground(pc_gb_palette[shade]);
            for (sy = 0; sy < PC_WORLD_PIXEL_SCALE; ++sy)
            {
                for (sx = 0; sx < PC_WORLD_PIXEL_SCALE; ++sx)
                {
                    rb->lcd_drawpixel(x + px * PC_WORLD_PIXEL_SCALE + sx,
                                      y + py * PC_WORLD_PIXEL_SCALE + sy);
                }
            }
        }
    }
}

static void draw_red_sprite_scaled(const unsigned char frame[16][16],
                                   int x, int y, int scale, bool flip_x)
{
    int py;
    int px;
    int sy;
    int sx;

    for (py = 0; py < 16; ++py)
    {
        for (px = 0; px < 16; ++px)
        {
            int src_x = flip_x ? (15 - px) : px;
            unsigned char shade = frame[py][src_x];

            if (shade == 3)
                continue;

            rb->lcd_set_foreground(pc_gb_palette[shade]);
            for (sy = 0; sy < scale; ++sy)
            {
                for (sx = 0; sx < scale; ++sx)
                {
                    rb->lcd_drawpixel(x + px * scale + sx,
                                      y + py * scale + sy);
                }
            }
        }
    }
}

static void draw_pokeball_icon(int x, int y, int radius)
{
    int size = MAX(7, radius * 2 - 1);
    int left = x - size / 2;
    int top = y - size / 2;

    rb->lcd_set_foreground(PC_GB_DEEP);
    rb->lcd_drawrect(left, top, size, size);
    rb->lcd_set_foreground(PC_GB_MID);
    rb->lcd_fillrect(left + 1, top + 1, size - 2, (size - 2) / 2);
    rb->lcd_set_foreground(PC_GB_LIGHT);
    rb->lcd_fillrect(left + 1, top + 1 + (size - 2) / 2,
                     size - 2, size - 2 - (size - 2) / 2);
    rb->lcd_set_foreground(PC_GB_DEEP);
    rb->lcd_fillrect(left + 1, y, size - 2, 1);
    rb->lcd_fillrect(x - 1, y - 1, 3, 3);
}

static void draw_player(const struct pc_world_state *world)
{
    const struct pc_asset_bitmap *trainer =
        &world->assets.trainer[world->heading][world->walk_frame];
    const unsigned char (*frame)[16];
    bool flip_x;
    int screen_x = world->origin_x + world->player_x;
    int screen_y = world->origin_y + world->player_y;
    int x = screen_x - PC_WORLD_SCALE(8);
    int y = screen_y - PC_WORLD_SCALE(9);

    fill_capsule(screen_x - PC_WORLD_SCALE(7),
                 screen_y + PC_WORLD_FOOT_Y_OFFSET,
                 PC_WORLD_SCALE(14),
                 PC_WORLD_SCALE(3),
                 LCD_RGBPACK(0x60, 0x70, 0x58));
    if (trainer->loaded)
    {
        draw_asset_bitmap(&trainer->bmp,
                          screen_x - trainer->bmp.width / 2,
                          screen_y - trainer->bmp.height + PC_WORLD_SCALE(10));
        return;
    }

    frame = select_player_frame(world, &flip_x);
    draw_red_sprite(frame, x, y, flip_x);
}

static void draw_static_red_npc(const struct pc_world_state *world,
                                int metatile_x, int metatile_y,
                                enum pc_heading heading)
{
    const struct pc_asset_bitmap *trainer = &world->assets.trainer[heading][1];
    const unsigned char (*frame)[16];
    bool flip_x;
    int step = PC_WORLD_TILE_SIZE / 2;
    int screen_x = world->origin_x + metatile_x * step + step / 2;
    int screen_y = world->origin_y + metatile_y * step + step / 2
                   - PC_WORLD_FOOT_Y_OFFSET;

    if (screen_x < -PC_WORLD_SCALE(16) || screen_x > LCD_WIDTH + PC_WORLD_SCALE(16) ||
        screen_y < -PC_WORLD_SCALE(20) || screen_y > LCD_HEIGHT + PC_WORLD_SCALE(12))
    {
        return;
    }

    switch (heading)
    {
        case PC_HEADING_N:
            flip_x = false;
            frame = pc_red_player_frames[1];
            break;

        case PC_HEADING_E:
            flip_x = true;
            frame = pc_red_player_frames[2];
            break;

        case PC_HEADING_W:
            flip_x = false;
            frame = pc_red_player_frames[2];
            break;

        case PC_HEADING_S:
        default:
            flip_x = false;
            frame = pc_red_player_frames[0];
            break;
    }

    fill_capsule(screen_x - PC_WORLD_SCALE(7),
                 screen_y + PC_WORLD_FOOT_Y_OFFSET,
                 PC_WORLD_SCALE(14),
                 PC_WORLD_SCALE(3),
                 LCD_RGBPACK(0x60, 0x70, 0x58));
    if (trainer->loaded)
    {
        draw_asset_bitmap(&trainer->bmp,
                          screen_x - trainer->bmp.width / 2,
                          screen_y - trainer->bmp.height + PC_WORLD_SCALE(10));
        return;
    }

    draw_red_sprite(frame,
                    screen_x - PC_WORLD_SCALE(8),
                    screen_y - PC_WORLD_SCALE(9),
                    flip_x);
}

static void draw_static_named_trainer_npc(const struct pc_world_state *world,
                                          int trainer_index,
                                          int metatile_x, int metatile_y,
                                          enum pc_heading heading)
{
    const struct pc_asset_bitmap *trainer;
    int step = PC_WORLD_TILE_SIZE / 2;
    int screen_x = world->origin_x + metatile_x * step + step / 2;
    int screen_y = world->origin_y + metatile_y * step + step / 2
                   - PC_WORLD_FOOT_Y_OFFSET;

    if (screen_x < -PC_WORLD_SCALE(16) || screen_x > LCD_WIDTH + PC_WORLD_SCALE(16) ||
        screen_y < -PC_WORLD_SCALE(20) || screen_y > LCD_HEIGHT + PC_WORLD_SCALE(12))
    {
        return;
    }

    trainer = ensure_npc_trainer_asset(trainer_index, heading, 1);
    if (trainer == NULL || !trainer->loaded)
    {
        draw_static_red_npc(world, metatile_x, metatile_y, heading);
        return;
    }

    fill_capsule(screen_x - PC_WORLD_SCALE(7),
                 screen_y + PC_WORLD_FOOT_Y_OFFSET,
                 PC_WORLD_SCALE(14),
                 PC_WORLD_SCALE(3),
                 LCD_RGBPACK(0x60, 0x70, 0x58));
    draw_asset_bitmap(&trainer->bmp,
                      screen_x - trainer->bmp.width / 2,
                      screen_y - trainer->bmp.height + PC_WORLD_SCALE(10));
}

static const char *npc_trainer_name(int trainer_index)
{
    static const char *const names[PC_WORLD_NPC_TRAINERS] = {
        "ethan", "lyra", "leaf"
    };

    if (trainer_index < 0 || trainer_index >= PC_WORLD_NPC_TRAINERS)
        return names[0];
    return names[trainer_index];
}

static const struct pc_asset_bitmap *ensure_npc_trainer_asset(int trainer_index,
                                                              int heading, int frame)
{
    struct pc_asset_bitmap *asset;

    if (trainer_index < 0 || trainer_index >= PC_WORLD_NPC_TRAINERS ||
        heading < 0 || heading >= 4 ||
        frame < 0 || frame >= PC_WORLD_WALK_FRAMES)
    {
        return NULL;
    }

    asset = &pc_world_npc_assets[trainer_index][heading][frame];
    if (asset->loaded)
        return asset;

    rb->memset(asset, 0, sizeof(*asset));
    asset->pixels = pc_world_npc_pixels[trainer_index][heading][frame];
    asset->capacity = PC_WORLD_TRAINER_BYTES;
    if (!pc_assets_load_named_world_trainer(asset, npc_trainer_name(trainer_index),
                                            heading, frame))
    {
        rb->memset(asset, 0, sizeof(*asset));
        return NULL;
    }

    return asset;
}

static void draw_scene_npcs(const struct pc_world_state *world)
{
    if (world->scene == PC_WORLD_SCENE_PALLET)
    {
        draw_static_named_trainer_npc(world, 1, 3, 8, PC_HEADING_S);
        draw_static_red_npc(world, 8, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 11, 14, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_SOUTH)
    {
        draw_static_named_trainer_npc(world, 0, 13, 20, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 30, 8, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 30, 25, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 17, 9, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 18, 9, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 6, 23, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 17, 5, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_OAKS_LAB)
    {
        draw_static_red_npc(world, 4, 3, PC_HEADING_E);
        draw_static_red_npc(world, 6, 3, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_BLUES_HOUSE)
    {
        draw_static_named_trainer_npc(world, 2, 2, 3, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 6, 4, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_MART)
    {
        draw_static_named_trainer_npc(world, 0, 0, 5, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 2, 3, 3, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 5, 5, PC_HEADING_N);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_POKECENTER)
    {
        draw_static_named_trainer_npc(world, 1, 3, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 10, 5, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 4, 3, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 1, 11, 2, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_SCHOOL_HOUSE)
    {
        draw_static_named_trainer_npc(world, 1, 3, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 2, 4, 1, PC_HEADING_S);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_NICKNAME_HOUSE)
    {
        draw_static_named_trainer_npc(world, 0, 5, 3, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 1, 1, 4, PC_HEADING_E);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_GYM)
    {
        draw_static_named_trainer_npc(world, 0, 2, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 12, 7, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 11, 11, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 10, 7, PC_HEADING_W);
        draw_static_named_trainer_npc(world, 0, 3, 7, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 13, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 10, 1, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 2, 16, PC_HEADING_E);
        draw_static_named_trainer_npc(world, 0, 6, 5, PC_HEADING_S);
        draw_static_named_trainer_npc(world, 0, 16, 15, PC_HEADING_E);
    }
}

static void draw_banner(const struct pc_world_state *world)
{
    char line1[PC_BANNER_LINE_CHARS];
    char line2[PC_BANNER_LINE_CHARS];
    const struct pc_asset_bitmap *trainer;
    int x = 8;
    int y = 6;
    int w = 154;
    const unsigned char (*frame)[16];
    bool flip_x;

    fit_text_small(line1, sizeof(line1), world->banner.line1, w - 52);
    fit_text_small(line2, sizeof(line2), world->banner.line2, w - 52);
    draw_flat_panel(x, y, w, 24, PC_GB_PANEL, PC_GB_DEEP, PC_GB_MID);

    trainer = &world->assets.trainer[world->heading][world->walk_frame];
    if (trainer->loaded)
    {
        draw_asset_bitmap(&trainer->bmp,
                          x + 17 - trainer->bmp.width / 2,
                          y + 20 - trainer->bmp.height);
    }
    else
    {
        frame = select_player_frame(world, &flip_x);
        draw_red_sprite_scaled(frame, x + 8, y + 4, 1, flip_x);
    }

    draw_text_small(x + 30, y + 5, PC_GB_DEEP, line1);
    if (line2[0] != '\0')
        draw_text_small(x + 30, y + 13, PC_GB_DARK, line2);
}

static void draw_hud(const struct pc_world_state *world)
{
    char money[24];
    char balls[24];
    char hint[48];
    int money_x = LCD_WIDTH - 74;
    int balls_x = LCD_WIDTH - 140;

    rb->snprintf(money, sizeof(money), "$%u", world->money);
    rb->snprintf(balls, sizeof(balls), "%u", world->pokeballs);

    if (outdoor_scene(world->scene))
        rb->snprintf(hint, sizeof(hint), "Walk into a Pokemon to catch it");
    else if (world->scene == PC_WORLD_SCENE_VIRIDIAN_MART)
        rb->snprintf(hint, sizeof(hint), "Press Select to talk");
    else
        rb->snprintf(hint, sizeof(hint), "Hold Select for menu");

    fit_text_small(hint, sizeof(hint), hint, LCD_WIDTH - 64);

    draw_flat_panel(balls_x, 8, 58, 18, PC_GB_PANEL, PC_GB_DEEP, PC_GB_MID);
    draw_pokeball_icon(balls_x + 12, 17, 4);
    draw_text_small(balls_x + 22, 13, PC_GB_DEEP, balls);

    draw_flat_panel(money_x, 8, 66, 18, PC_GB_PANEL, PC_GB_DEEP, PC_GB_MID);
    draw_text_small(money_x + 10, 13, PC_GB_DEEP, money);

    draw_flat_panel(18, LCD_HEIGHT - 20, LCD_WIDTH - 36, 14,
                    PC_GB_PANEL, PC_GB_DEEP, PC_GB_MID);
    draw_text_small(28, LCD_HEIGHT - 16, PC_GB_DARK, hint);
}

static void fill_rect_outline(int x, int y, int w, int h,
                              fb_data fill, fb_data outline)
{
    rb->lcd_set_foreground(fill);
    rb->lcd_fillrect(x, y, w, h);
    rb->lcd_set_foreground(outline);
    rb->lcd_drawrect(x, y, w, h);
}

static int count_caught_species(const struct pc_world_state *world)
{
    int i;
    int total = 0;

    for (i = 0; i < PC_POKEDEX_MAX; ++i)
    {
        if (world->caught_counts[i] > 0)
            total++;
    }
    return total;
}

static void draw_notice(const struct pc_world_state *world)
{
    char line1[PC_BANNER_LINE_CHARS];
    char line2[PC_BANNER_LINE_CHARS];
    int w = LCD_WIDTH - 56;
    int h = world->detail.line2[0] != '\0' ? 26 : 16;
    int x = 28;
    int y = 34;

    if (world->notice_frames <= 0 || world->detail.line1[0] == '\0')
        return;

    fit_text_small(line1, sizeof(line1), world->detail.line1, w - 20);
    fit_text_small(line2, sizeof(line2), world->detail.line2, w - 20);
    draw_flat_panel(x, y, w, h, PC_GB_PANEL, PC_GB_DEEP, PC_GB_MID);
    draw_text_small(x + 10, y + 4, PC_GB_DEEP, line1);
    if (world->detail.line2[0] != '\0')
        draw_text_small(x + 10, y + 12, PC_GB_DARK, line2);
}

static void draw_menu_overlay(const struct pc_world_state *world)
{
    static const char *const items[] = {
        "Resume", "Backpack", "Pokedex", "Save Game", "Quit"
    };
    int x = 52;
    int y = 34;
    int i;

    fill_rect_outline(x, y, LCD_WIDTH - 104, 126, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 36, y + 10, PC_GB_DEEP, "PAUSE");
    for (i = 0; i < 5; ++i)
    {
        int row_y = y + 28 + i * 18;

        if (i == world->menu_index)
            fill_rect_outline(x + 10, row_y - 2, LCD_WIDTH - 124, 14, PC_GB_MID, PC_GB_DEEP);
        draw_text_small(x + 18, row_y, PC_GB_DEEP, items[i]);
    }
    if (world->detail.line2[0] != '\0')
        draw_text_small(x + 10, y + 108, PC_GB_DEEP, world->detail.line2);
}

static void draw_bag_overlay(const struct pc_world_state *world)
{
    int x = 22;
    int y = 24;
    int shown = 0;
    int i;
    char count_text[16];

    fill_rect_outline(x, y, LCD_WIDTH - 44, LCD_HEIGHT - 48, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 10, y + 8, PC_GB_DEEP, "BACKPACK");
    if (count_caught_species(world) == 0)
    {
        draw_text_small(x + 10, y + 28, PC_GB_DEEP, "No Pokemon caught yet");
        return;
    }

    for (i = 0; i < PC_POKEDEX_MAX && shown < 5; ++i)
    {
        const struct pc_creature_def *creature;
        int row_y;

        if (world->caught_counts[i] == 0)
            continue;

        creature = pc_assets_get_creature(i);
        if (creature == NULL)
            continue;

        row_y = y + 26 + shown * 16;
        if (i == world->bag_index)
            fill_rect_outline(x + 8, row_y - 2, LCD_WIDTH - 60, 14, PC_GB_MID, PC_GB_DEEP);
        draw_text_small(x + 14, row_y, PC_GB_DEEP, creature->name);
        rb->snprintf(count_text, sizeof(count_text), "x%u", world->caught_counts[i]);
        draw_text_small(LCD_WIDTH - 58, row_y, PC_GB_DEEP, count_text);
        shown++;
    }
    draw_text_small(x + 10, y + 110, PC_GB_DEEP, "Menu/Play scroll");
    draw_text_small(x + 10, y + 124, PC_GB_DEEP, "Left back");
}

static void draw_pokedex_overlay(const struct pc_world_state *world)
{
    const struct pc_creature_def *creature = pc_assets_get_creature(world->dex_index);
    int family = pc_assets_get_family_index(world->dex_index);
    int evolve_target = pc_assets_get_evolution_target(world->dex_index);
    int evolve_cost = pc_assets_get_evolution_cost(world->dex_index);
    unsigned candy = 0;
    int x = 18;
    int y = 18;
    char line[PC_BANNER_LINE_CHARS];

    fill_rect_outline(x, y, LCD_WIDTH - 36, LCD_HEIGHT - 36, PC_GB_DEEP, PC_GB_MID);
    fill_rect_outline(x + 8, y + 8, LCD_WIDTH - 52, LCD_HEIGHT - 52, PC_GB_LIGHT, PC_GB_DEEP);
    fill_rect_outline(x + 14, y + 18, 86, 86, PC_GB_PANEL, PC_GB_DEEP);
    fill_rect_outline(x + 108, y + 18, 84, 86, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 120, y + 26, PC_GB_DEEP, "POKEDEX");
    if (creature == NULL)
        return;

    if (world->assets.dex_creature.loaded)
    {
        draw_asset_bitmap(&world->assets.dex_creature.bmp, x + 22, y + 28);
    }
    else
    {
        draw_creature_panel_fallback(creature, x + 18, y + 24, 78, 78);
        draw_text_small(x + 30, y + 92, PC_GB_DARK, "Fallback art");
    }

    rb->snprintf(line, sizeof(line), "#%03d", creature->species_id);
    draw_text_small(x + 120, y + 44, PC_GB_DEEP, line);
    fit_text_small(line, sizeof(line), creature->name, 70);
    draw_text_small(x + 120, y + 58, PC_GB_DEEP, line);
    if (world->dex_index < PC_POKEDEX_MAX && world->caught_counts[world->dex_index] > 0)
        rb->snprintf(line, sizeof(line), "Caught %u", world->caught_counts[world->dex_index]);
    else
        rb->snprintf(line, sizeof(line), "Uncaught");
    draw_text_small(x + 120, y + 74, PC_GB_DEEP, line);
    if (family >= 0 && family < PC_POKEDEX_MAX)
        candy = world->family_candy[family];
    rb->snprintf(line, sizeof(line), "Candy %u", candy);
    draw_text_small(x + 120, y + 88, PC_GB_DEEP, line);
    if (evolve_target >= 0 && evolve_cost > 0)
    {
        const struct pc_creature_def *next = pc_assets_get_creature(evolve_target);

        if (next != NULL)
            rb->snprintf(line, sizeof(line), "Evolve %s %d", next->name, evolve_cost);
        else
            rb->snprintf(line, sizeof(line), "Evolve %d candy", evolve_cost);
    }
    else
    {
        rb->snprintf(line, sizeof(line), "No evolve");
    }
    fit_text_small(line, sizeof(line), line, 70);
    draw_text_small(x + 120, y + 100, PC_GB_DEEP, line);
    draw_text_small(x + 22, y + 118, PC_GB_DEEP, "Menu/Play or Left/Right scroll");
    draw_text_small(x + 22, y + 132, PC_GB_DEEP, "Select evolve  Left back");
}

static void draw_mart_overlay(const struct pc_world_state *world)
{
    static const char *const items[] = {
        "Poke Ball x10", "HM03 Surf", "Assign Surf", "Leave"
    };
    int assigned = world->ability_species[PC_FIELD_ABILITY_SURF];
    const struct pc_creature_def *partner = NULL;
    int x = 28;
    int y = 26;
    int i;
    char line[PC_BANNER_LINE_CHARS];

    if (assigned >= 0 && assigned < PC_POKEDEX_MAX &&
        world->caught_counts[assigned] > 0)
    {
        partner = pc_assets_get_creature(assigned);
    }

    fill_rect_outline(x, y, LCD_WIDTH - 56, LCD_HEIGHT - 52, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 10, y + 8, PC_GB_DEEP, "VIRIDIAN MART");
    rb->snprintf(line, sizeof(line), "$%u   Balls %u", world->money, world->pokeballs);
    draw_text_small(x + 10, y + 22, PC_GB_DEEP, line);

    for (i = 0; i < (int)ARRAYLEN(items); ++i)
    {
        int row_y = y + 42 + i * 18;

        if (i == world->mart_index)
            fill_rect_outline(x + 8, row_y - 2, LCD_WIDTH - 72, 14, PC_GB_MID, PC_GB_DEEP);
        draw_text_small(x + 14, row_y, PC_GB_DEEP, items[i]);

        if (i == 0)
            draw_text_small(LCD_WIDTH - 82, row_y, PC_GB_DEEP, "$20");
        else if (i == 1)
            draw_text_small(LCD_WIDTH - 82, row_y, PC_GB_DEEP,
                            world->ability_owned[PC_FIELD_ABILITY_SURF] ? "OWND" : "$80");
    }

    if (!world->ability_owned[PC_FIELD_ABILITY_SURF])
        draw_text_small(x + 10, y + 118, PC_GB_DEEP, "Buy HM03 Surf for water travel");
    else if (partner != NULL)
    {
        rb->snprintf(line, sizeof(line), "Surf partner: %s", partner->name);
        draw_text_small(x + 10, y + 118, PC_GB_DEEP, line);
    }
    else
        draw_text_small(x + 10, y + 118, PC_GB_DEEP, "Surf is unassigned");

    draw_text_small(x + 10, y + 132, PC_GB_DEEP, "Menu/Play scroll  Select choose");
    draw_text_small(x + 10, y + 146, PC_GB_DEEP, "Left back");
}

static void draw_mart_assign_overlay(const struct pc_world_state *world)
{
    int assigned = world->ability_species[PC_FIELD_ABILITY_SURF];
    const struct pc_creature_def *current = NULL;
    const struct pc_creature_def *partner = NULL;
    int x = 40;
    int y = 34;
    char line[PC_BANNER_LINE_CHARS];

    if (world->mart_assign_index >= 0 && world->mart_assign_index < PC_POKEDEX_MAX &&
        world->caught_counts[world->mart_assign_index] > 0)
    {
        current = pc_assets_get_creature(world->mart_assign_index);
    }

    if (assigned >= 0 && assigned < PC_POKEDEX_MAX &&
        world->caught_counts[assigned] > 0)
    {
        partner = pc_assets_get_creature(assigned);
    }

    fill_rect_outline(x, y, LCD_WIDTH - 80, LCD_HEIGHT - 68, PC_GB_PANEL, PC_GB_DEEP);
    draw_text_small(x + 10, y + 8, PC_GB_DEEP, "ASSIGN HM03 SURF");

    if (current == NULL)
    {
        draw_text_small(x + 10, y + 36, PC_GB_DEEP, "No compatible Pokemon");
        draw_text_small(x + 10, y + 104, PC_GB_DEEP, "Left back");
        return;
    }

    fill_rect_outline(x + 10, y + 28, LCD_WIDTH - 100, 38, PC_GB_MID, PC_GB_DEEP);
    draw_text_small(x + 18, y + 38, PC_GB_DEEP, current->name);
    rb->snprintf(line, sizeof(line), "Caught x%u", world->caught_counts[world->mart_assign_index]);
    draw_text_small(x + 18, y + 52, PC_GB_DEEP, line);

    if (partner != NULL)
    {
        rb->snprintf(line, sizeof(line), "Current: %s", partner->name);
        draw_text_small(x + 10, y + 78, PC_GB_DEEP, line);
    }
    else
        draw_text_small(x + 10, y + 78, PC_GB_DEEP, "Current: none");

    draw_text_small(x + 10, y + 104, PC_GB_DEEP, "Menu/Play cycle  Select assign");
    draw_text_small(x + 10, y + 118, PC_GB_DEEP, "Left back");
}

void pc_world_render_frame(const struct pc_world_state *world)
{
    int i;

    draw_map(world);
    if (outdoor_scene(world->scene))
    {
        for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
            draw_spawn(world, i);
    }
    draw_scene_npcs(world);
    draw_player(world);
    draw_banner(world);
    if (world->view == PC_WORLD_VIEW_MENU)
        draw_menu_overlay(world);
    else if (world->view == PC_WORLD_VIEW_BAG)
        draw_bag_overlay(world);
    else if (world->view == PC_WORLD_VIEW_POKEDEX)
        draw_pokedex_overlay(world);
    else if (world->view == PC_WORLD_VIEW_MART)
        draw_mart_overlay(world);
    else if (world->view == PC_WORLD_VIEW_MART_ASSIGN)
        draw_mart_assign_overlay(world);
    else
        draw_hud(world);
    draw_notice(world);
    rb->lcd_update();
}
