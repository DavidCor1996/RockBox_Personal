#include "pocketcatch.h"
#include "pc_red_gfx.h"
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

static fb_data pc_world_bg_cache[LCD_WIDTH * LCD_HEIGHT];
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

static int tile_screen_x(int tx)
{
    return PC_WORLD_ORIGIN_X + tx * PC_WORLD_TILE_SIZE;
}

static int tile_screen_y(int ty)
{
    return PC_WORLD_ORIGIN_Y + ty * PC_WORLD_TILE_SIZE;
}

static void draw_spawn(const struct pc_world_state *world, int index)
{
    const struct pc_world_spawn *spawn = &world->spawns[index];
    const struct pc_creature_def *creature;

    if (!spawn->active)
        return;

    creature = pc_assets_get_creature(spawn->species_index);
    if (world->assets.creature[index].loaded)
    {
        rb->lcd_bitmap_transparent((const fb_data *)world->assets.creature[index].bmp.data,
                                   spawn->x - world->assets.creature[index].bmp.width / 2,
                                   spawn->y - world->assets.creature[index].bmp.height / 2,
                                   world->assets.creature[index].bmp.width,
                                   world->assets.creature[index].bmp.height);
    }
    else if (creature != NULL)
    {
        rb->lcd_set_foreground(creature->primary);
        xlcd_fillcircle(spawn->x, spawn->y, 8);
        rb->lcd_set_foreground(creature->accent);
        rb->lcd_fillrect(spawn->x - 4, spawn->y - 2, 8, 4);
    }
}

static void draw_home_marker(const struct pc_world_state *world)
{
    int x = world->home_x - 18;
    int y = world->home_y - 28;

    if (x < -36 || x > LCD_WIDTH || y < -12 || y > LCD_HEIGHT)
        return;

    fill_capsule(x, y, 36, 12, LCD_RGBPACK(0xff, 0xf0, 0xb0));
    draw_text_small(world->home_x - 11, world->home_y - 24, PC_GB_DEEP, "HOME");
}

static void cache_fill(fb_data color)
{
    int i;

    for (i = 0; i < LCD_WIDTH * LCD_HEIGHT; ++i)
        pc_world_bg_cache[i] = color;
}

static void cache_put_pixel(int x, int y, fb_data color)
{
    if ((unsigned)x >= LCD_WIDTH || (unsigned)y >= LCD_HEIGHT)
        return;
    pc_world_bg_cache[y * LCD_WIDTH + x] = color;
}

static void blit_tile_to_cache(unsigned char tile_id, int dst_x, int dst_y)
{
    int py;
    int px;

    if (tile_id >= PC_RED_TILE_COUNT)
        return;

    for (py = 0; py < 8; ++py)
    {
        for (px = 0; px < 8; ++px)
        {
            cache_put_pixel(dst_x + px, dst_y + py,
                            pc_gb_palette[pc_red_overworld_tiles[tile_id][py][px]]);
        }
    }
}

static void build_world_cache(const struct pc_world_state *world)
{
    int by;
    int bx;
    int sub_y;
    int sub_x;

    cache_fill(PC_GB_LIGHT);

    for (by = 0; by < PC_WORLD_H; ++by)
    {
        for (bx = 0; bx < PC_WORLD_W; ++bx)
        {
            unsigned char block_id = world->tiles[by][bx];
            int base_x = tile_screen_x(bx);
            int base_y = tile_screen_y(by);

            if (block_id >= PC_RED_BLOCK_COUNT)
                continue;

            for (sub_y = 0; sub_y < 4; ++sub_y)
            {
                for (sub_x = 0; sub_x < 4; ++sub_x)
                {
                    blit_tile_to_cache(pc_red_overworld_blocks[block_id][sub_y][sub_x],
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
    if (!pc_world_bg_ready || world->frame == 0)
        build_world_cache(world);

    rb->lcd_bitmap(pc_world_bg_cache, 0, 0, LCD_WIDTH, LCD_HEIGHT);
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
    int x = world->player_x - 8;
    int y = world->player_y - 9;

    fill_capsule(world->player_x - 7, world->player_y + 7, 14, 3,
                 LCD_RGBPACK(0x60, 0x70, 0x58));
    if (trainer->loaded)
    {
        rb->lcd_bitmap_transparent((const fb_data *)trainer->bmp.data,
                                   world->player_x - trainer->bmp.width / 2,
                                   world->player_y - trainer->bmp.height + 10,
                                   trainer->bmp.width,
                                   trainer->bmp.height);
        return;
    }

    frame = select_player_frame(world, &flip_x);
    draw_red_sprite(frame, x, y, flip_x);
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
                   "Hold dir to walk   Menu+Select exits",
                   LCD_WIDTH - 50);
    fill_capsule(20, LCD_HEIGHT - 16, LCD_WIDTH - 40, 12, PC_GB_PANEL);
    draw_text_small(28, LCD_HEIGHT - 13, PC_GB_DEEP, hud);
}

void pc_world_render_frame(const struct pc_world_state *world)
{
    int i;

    draw_map(world);
    draw_home_marker(world);
    for (i = 0; i < PC_WORLD_MAX_SPAWNS; ++i)
        draw_spawn(world, i);
    draw_player(world);
    draw_banner(world);
    draw_hud();
    rb->lcd_update();
}
