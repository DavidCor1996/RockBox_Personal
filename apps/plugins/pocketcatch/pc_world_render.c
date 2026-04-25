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

#define PC_WORLD_CACHE_W (PC_WORLD_W * PC_WORLD_TILE_SIZE)
#define PC_WORLD_CACHE_H (PC_WORLD_H * PC_WORLD_TILE_SIZE)

static fb_data pc_world_bg_cache[PC_WORLD_CACHE_W * PC_WORLD_CACHE_H];
static bool pc_world_bg_ready;

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
    int shadow_w = 12;

    if (!spawn->active)
        return;

    screen_x = world->origin_x + spawn->x;
    screen_y = world->origin_y + spawn->y;
    creature = pc_assets_get_creature(spawn->species_index);
    if (world->assets.creature[index].loaded)
        shadow_w = MAX(12, world->assets.creature[index].bmp.width / 2);

    fill_capsule(screen_x - shadow_w / 2, screen_y + 7,
                 shadow_w, 4, LCD_RGBPACK(0x60, 0x70, 0x58));

    if (world->assets.creature[index].loaded)
    {
        rb->lcd_bitmap_transparent((const fb_data *)world->assets.creature[index].bmp.data,
                                   screen_x - world->assets.creature[index].bmp.width / 2,
                                   screen_y - world->assets.creature[index].bmp.height / 2,
                                   world->assets.creature[index].bmp.width,
                                   world->assets.creature[index].bmp.height);
    }
    else if (creature != NULL)
    {
        rb->lcd_set_foreground(creature->primary);
        xlcd_fillcircle(screen_x, screen_y, 10);
        rb->lcd_set_foreground(creature->accent);
        rb->lcd_fillrect(screen_x - 5, screen_y - 2, 10, 4);
    }
}

static void cache_fill(fb_data color)
{
    int i;

    for (i = 0; i < PC_WORLD_CACHE_W * PC_WORLD_CACHE_H; ++i)
        pc_world_bg_cache[i] = color;
}

static void cache_put_pixel(int x, int y, fb_data color)
{
    if ((unsigned)x >= PC_WORLD_CACHE_W || (unsigned)y >= PC_WORLD_CACHE_H)
        return;
    pc_world_bg_cache[y * PC_WORLD_CACHE_W + x] = color;
}

static void blit_tile_to_cache(const unsigned char tiles[][8][8], int tile_count,
                               unsigned char tile_id, int dst_x, int dst_y)
{
    int py;
    int px;

    if (tile_id >= tile_count)
        return;

    for (py = 0; py < 8; ++py)
    {
        for (px = 0; px < 8; ++px)
        {
            cache_put_pixel(dst_x + px, dst_y + py,
                            pc_gb_palette[tiles[tile_id][py][px]]);
        }
    }
}

static void build_world_cache(const struct pc_world_state *world)
{
    int by;
    int bx;
    int sub_y;
    int sub_x;
    const unsigned char (*tiles)[8][8];
    const unsigned char (*blocks)[4][4];
    int tile_count;
    int block_count;

    cache_fill(PC_GB_LIGHT);

    if (outdoor_scene(world->scene))
    {
        tiles = pc_red_overworld_tiles;
        blocks = pc_red_overworld_blocks;
        tile_count = PC_RED_TILE_COUNT;
        block_count = PC_RED_BLOCK_COUNT;
    }
    else if (world->scene == PC_WORLD_SCENE_OAKS_LAB)
    {
        tiles = pc_red_dojo_tiles;
        blocks = pc_red_dojo_blocks;
        tile_count = PC_RED_DOJO_TILE_COUNT;
        block_count = PC_RED_DOJO_BLOCK_COUNT;
    }
    else if (world->scene == PC_WORLD_SCENE_VIRIDIAN_MART)
    {
        tiles = pc_red_pokecenter_tiles;
        blocks = pc_red_pokecenter_blocks;
        tile_count = PC_RED_POKECENTER_TILE_COUNT;
        block_count = PC_RED_POKECENTER_BLOCK_COUNT;
    }
    else
    {
        tiles = pc_red_house_tiles;
        blocks = pc_red_house_blocks;
        tile_count = PC_RED_HOUSE_TILE_COUNT;
        block_count = PC_RED_HOUSE_BLOCK_COUNT;
    }

    for (by = 0; by < world->map_h; ++by)
    {
        for (bx = 0; bx < world->map_w; ++bx)
        {
            unsigned char block_id = world->tiles[by][bx];
            int base_x = bx * PC_WORLD_TILE_SIZE;
            int base_y = by * PC_WORLD_TILE_SIZE;

            if (block_id >= block_count)
                continue;

            for (sub_y = 0; sub_y < 4; ++sub_y)
            {
                for (sub_x = 0; sub_x < 4; ++sub_x)
                {
                    blit_tile_to_cache(tiles, tile_count,
                                       blocks[block_id][sub_y][sub_x],
                                       base_x + sub_x * 8,
                                       base_y + sub_y * 8);
                }
            }
        }
    }

    pc_world_bg_ready = true;
}

static void draw_map(const struct pc_world_state *world)
{
    int map_px_w = world->map_w * PC_WORLD_TILE_SIZE;
    int map_px_h = world->map_h * PC_WORLD_TILE_SIZE;

    if (!pc_world_bg_ready || world->map_dirty)
    {
        build_world_cache(world);
        ((struct pc_world_state *)world)->map_dirty = false;
    }

    rb->lcd_set_background(PC_GB_LIGHT);
    rb->lcd_clear_display();

    if (outdoor_scene(world->scene))
    {
        rb->lcd_bitmap_part(pc_world_bg_cache,
                            -world->origin_x,
                            -world->origin_y,
                            PC_WORLD_CACHE_W,
                            0, 0,
                            LCD_WIDTH, LCD_HEIGHT);
    }
    else
    {
        rb->lcd_bitmap_part(pc_world_bg_cache,
                            0, 0,
                            PC_WORLD_CACHE_W,
                            world->origin_x,
                            world->origin_y,
                            map_px_w, map_px_h);
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

    for (py = 0; py < 16; ++py)
    {
        for (px = 0; px < 16; ++px)
        {
            int src_x = flip_x ? (15 - px) : px;
            unsigned char shade = frame[py][src_x];

            if (shade == 3)
                continue;

            rb->lcd_set_foreground(pc_gb_palette[shade]);
            rb->lcd_drawpixel(x + px, y + py);
        }
    }
}

static void draw_player(const struct pc_world_state *world)
{
    const struct pc_asset_bitmap *trainer =
        &world->assets.trainer[world->heading][world->walk_frame];
    const unsigned char (*frame)[16];
    bool flip_x;
    int screen_x = world->origin_x + world->player_x;
    int screen_y = world->origin_y + world->player_y;
    int x = screen_x - 8;
    int y = screen_y - 9;

    fill_capsule(screen_x - 7, screen_y + 7, 14, 3,
                 LCD_RGBPACK(0x60, 0x70, 0x58));
    if (trainer->loaded)
    {
        rb->lcd_bitmap_transparent((const fb_data *)trainer->bmp.data,
                                   screen_x - trainer->bmp.width / 2,
                                   screen_y - trainer->bmp.height + 10,
                                   trainer->bmp.width,
                                   trainer->bmp.height);
        return;
    }

    frame = select_player_frame(world, &flip_x);
    draw_red_sprite(frame, x, y, flip_x);
}

static void draw_static_red_npc(const struct pc_world_state *world,
                                int metatile_x, int metatile_y,
                                enum pc_heading heading)
{
    const unsigned char (*frame)[16];
    bool flip_x;
    int step = PC_WORLD_TILE_SIZE / 2;
    int screen_x = world->origin_x + metatile_x * step + step / 2;
    int screen_y = world->origin_y + metatile_y * step + step / 2 - 7;

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

    fill_capsule(screen_x - 7, screen_y + 7, 14, 3,
                 LCD_RGBPACK(0x60, 0x70, 0x58));
    draw_red_sprite(frame, screen_x - 8, screen_y - 9, flip_x);
}

static void draw_scene_npcs(const struct pc_world_state *world)
{
    if (world->scene == PC_WORLD_SCENE_OAKS_LAB)
    {
        draw_static_red_npc(world, 4, 3, PC_HEADING_E);
        draw_static_red_npc(world, 6, 3, PC_HEADING_W);
        return;
    }

    if (world->scene == PC_WORLD_SCENE_VIRIDIAN_MART)
    {
        draw_static_red_npc(world, 0, 5, PC_HEADING_E);
        draw_static_red_npc(world, 3, 3, PC_HEADING_S);
        draw_static_red_npc(world, 5, 5, PC_HEADING_N);
    }
}

static void draw_banner(const struct pc_world_state *world)
{
    char line1[PC_BANNER_LINE_CHARS];

    fit_text_small(line1, sizeof(line1), world->banner.line1, LCD_WIDTH - 36);
    fill_capsule(10, 6, LCD_WIDTH - 20, 16, PC_GB_PANEL);
    draw_text_small(18, 10, PC_GB_DEEP, line1);
}

static void draw_hud(void)
{
    char hud[PC_BANNER_LINE_CHARS];

    fit_text_small(hud, sizeof(hud),
                   "Hold Select menu   Menu+Select exits",
                   LCD_WIDTH - 50);
    fill_capsule(20, LCD_HEIGHT - 16, LCD_WIDTH - 40, 12, PC_GB_PANEL);
    draw_text_small(28, LCD_HEIGHT - 13, PC_GB_DEEP, hud);
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
    if (world->notice_frames <= 0 || world->detail.line1[0] == '\0')
        return;

    fill_capsule(18, 24, LCD_WIDTH - 36, 16, PC_GB_PANEL);
    draw_text_small(26, 28, PC_GB_DEEP, world->detail.line1);
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
        rb->lcd_bitmap_transparent((const fb_data *)world->assets.dex_creature.bmp.data,
                                   x + 22,
                                   y + 28,
                                   world->assets.dex_creature.bmp.width,
                                   world->assets.dex_creature.bmp.height);
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
    else
        draw_hud();
    draw_notice(world);
    rb->lcd_update();
}
