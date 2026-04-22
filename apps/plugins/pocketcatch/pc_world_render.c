#include "pocketcatch.h"
#include "lib/xlcd.h"

#define PC_RED_LIGHT   LCD_RGBPACK(0xe0, 0xf8, 0xd0)
#define PC_RED_MID     LCD_RGBPACK(0x88, 0xc0, 0x70)
#define PC_RED_DARK    LCD_RGBPACK(0x34, 0x68, 0x56)
#define PC_RED_DEEP    LCD_RGBPACK(0x08, 0x18, 0x20)
#define PC_RED_PANEL   LCD_RGBPACK(0xf4, 0xf6, 0xea)
#define PC_RED_ROOF_A  LCD_RGBPACK(0xc8, 0x58, 0x50)
#define PC_RED_ROOF_B  LCD_RGBPACK(0xf0, 0x96, 0x78)
#define PC_RED_WALL    LCD_RGBPACK(0xf6, 0xe8, 0xc0)
#define PC_RED_TRIM    LCD_RGBPACK(0x78, 0x54, 0x38)
#define PC_LAB_ROOF_A  LCD_RGBPACK(0x5d, 0x83, 0xa4)
#define PC_LAB_ROOF_B  LCD_RGBPACK(0xa1, 0xc6, 0xdb)
#define PC_LAB_WALL    LCD_RGBPACK(0xe4, 0xee, 0xf4)
#define PC_WINDOW      LCD_RGBPACK(0xb2, 0xd9, 0xe8)

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

static bool is_house_block(unsigned char block_id)
{
    return block_id == 0x38 || block_id == 0x39 ||
           block_id == 0x3c || block_id == 0x3d;
}

static bool is_lab_block(unsigned char block_id)
{
    return block_id == 0x0c || block_id == 0x0d || block_id == 0x0e ||
           block_id == 0x10 || block_id == 0x3a || block_id == 0x00;
}

static bool is_tree_border(unsigned char block_id)
{
    return block_id == 0x52 || block_id == 0x4f || block_id == 0x50 ||
           block_id == 0x4d || block_id == 0x4e;
}

static bool is_water_block(unsigned char block_id)
{
    return block_id == 0x65 || block_id == 0x64 || block_id == 0x61;
}

static int tile_screen_x(int tx)
{
    return PC_WORLD_ORIGIN_X + tx * PC_WORLD_TILE_SIZE;
}

static int tile_screen_y(int ty)
{
    return PC_WORLD_ORIGIN_Y + ty * PC_WORLD_TILE_SIZE;
}

static void draw_grass_tile(int x, int y, unsigned char block_id)
{
    int inset = 0;

    rb->lcd_set_foreground(((x / PC_WORLD_TILE_SIZE + y / PC_WORLD_TILE_SIZE) & 1)
                           ? PC_RED_MID : LCD_RGBPACK(0x7a, 0xb0, 0x62));
    rb->lcd_fillrect(x, y, PC_WORLD_TILE_SIZE, PC_WORLD_TILE_SIZE);

    if (block_id == 0x77 || block_id == 0x56 || block_id == 0x74)
    {
        rb->lcd_set_foreground(PC_RED_DARK);
        rb->lcd_fillrect(x + 4, y + 5, 12, 10);
        rb->lcd_set_foreground(PC_RED_LIGHT);
        rb->lcd_drawline(x + 5, y + 9, x + 14, y + 9);
        return;
    }

    if (block_id == 0x31)
        inset = 3;

    rb->lcd_set_foreground(PC_RED_LIGHT);
    rb->lcd_drawpixel(x + 4 + inset, y + 5);
    rb->lcd_drawpixel(x + 9, y + 12);
    rb->lcd_drawpixel(x + 13 - inset, y + 7);
}

static void draw_tree_border_tile(int x, int y, unsigned char block_id)
{
    rb->lcd_set_foreground(PC_RED_DARK);
    rb->lcd_fillrect(x, y, PC_WORLD_TILE_SIZE, PC_WORLD_TILE_SIZE);
    rb->lcd_set_foreground(PC_RED_DEEP);
    rb->lcd_fillrect(x, y + 14, PC_WORLD_TILE_SIZE, 6);
    rb->lcd_set_foreground(PC_RED_MID);
    rb->lcd_fillrect(x + 2, y + 2, 6, 6);
    rb->lcd_fillrect(x + 10, y + 3, 7, 7);
    rb->lcd_fillrect(x + 6, y + 8, 5, 5);

    if (block_id == 0x0b || block_id == 0x0a)
    {
        rb->lcd_set_foreground(PC_RED_LIGHT);
        rb->lcd_fillrect(x + 7, y + 8, 6, 2);
        rb->lcd_set_foreground(PC_RED_DEEP);
        rb->lcd_fillrect(x + 5, y + 7, 2, 5);
        rb->lcd_fillrect(x + 13, y + 7, 2, 5);
    }
}

static void draw_house_tile(int x, int y, unsigned char block_id)
{
    bool top = block_id == 0x38 || block_id == 0x39;
    bool right = block_id == 0x39 || block_id == 0x3d;
    int ts = PC_WORLD_TILE_SIZE;
    int inset = MAX(2, ts / 10);
    int roof_y = y + MAX(2, ts / 8);
    int roof_h = MAX(8, ts / 2);
    int wall_y = y + MAX(1, ts / 12);
    int wall_h = ts - MAX(5, ts / 5);

    draw_grass_tile(x, y, 0x01);

    if (top)
    {
        rb->lcd_set_foreground(PC_RED_ROOF_A);
        rb->lcd_fillrect(x + 1, roof_y, ts - 2, roof_h);
        rb->lcd_set_foreground(PC_RED_ROOF_B);
        rb->lcd_fillrect(x + inset, roof_y + 2, ts - 2 * inset, MAX(3, roof_h / 3));
        rb->lcd_set_foreground(PC_RED_DEEP);
        rb->lcd_drawrect(x + 1, roof_y, ts - 2, roof_h);
        rb->lcd_drawline(x + 2, roof_y + roof_h - 2, x + ts - 3, roof_y + roof_h - 2);
        if (!right)
            rb->lcd_drawline(x + ts - 1, roof_y + 2, x + ts - 1, roof_y + roof_h - 1);
    }
    else
    {
        rb->lcd_set_foreground(PC_RED_WALL);
        rb->lcd_fillrect(x + 2, wall_y, ts - 4, wall_h);
        rb->lcd_set_foreground(PC_RED_TRIM);
        rb->lcd_drawrect(x + 2, wall_y, ts - 4, wall_h);
        rb->lcd_fillrect(x + 2, wall_y + wall_h - 3, ts - 4, 3);
        rb->lcd_set_foreground(PC_WINDOW);
        rb->lcd_fillrect(x + inset + 2, wall_y + 4, MAX(5, ts / 5), MAX(5, ts / 5));
        rb->lcd_set_foreground(PC_RED_DEEP);
        rb->lcd_drawrect(x + inset + 2, wall_y + 4, MAX(5, ts / 5), MAX(5, ts / 5));
        if (right)
            rb->lcd_fillrect(x + inset + 2, wall_y + 10, MAX(5, ts / 5), ts / 2);
        else
            rb->lcd_fillrect(x + ts - inset - 7, wall_y + 10, MAX(5, ts / 5), ts / 2);
        rb->lcd_set_foreground(PC_RED_ROOF_A);
        rb->lcd_fillrect(x + 1, y, ts - 2, 3);
    }
}

static void draw_lab_tile(int x, int y, unsigned char block_id)
{
    bool top = block_id == 0x0c || block_id == 0x0d || block_id == 0x0e;
    int ts = PC_WORLD_TILE_SIZE;
    int roof_y = y + MAX(2, ts / 8);
    int roof_h = MAX(8, ts / 2);
    int wall_y = y + MAX(1, ts / 12);
    int wall_h = ts - MAX(5, ts / 5);

    draw_grass_tile(x, y, 0x01);

    if (top)
    {
        rb->lcd_set_foreground(PC_LAB_ROOF_A);
        rb->lcd_fillrect(x + 1, roof_y, ts - 2, roof_h);
        rb->lcd_set_foreground(PC_LAB_ROOF_B);
        rb->lcd_fillrect(x + 3, roof_y + 2, ts - 6, MAX(3, roof_h / 3));
        rb->lcd_set_foreground(PC_RED_DEEP);
        rb->lcd_drawrect(x + 1, roof_y, ts - 2, roof_h);
        rb->lcd_drawline(x + 2, roof_y + roof_h - 2, x + ts - 3, roof_y + roof_h - 2);
    }
    else
    {
        rb->lcd_set_foreground(PC_LAB_WALL);
        rb->lcd_fillrect(x + 2, wall_y, ts - 4, wall_h);
        rb->lcd_set_foreground(PC_RED_DARK);
        rb->lcd_drawrect(x + 2, wall_y, ts - 4, wall_h);
        rb->lcd_set_foreground(PC_WINDOW);
        rb->lcd_fillrect(x + 4, wall_y + 4, ts - 8, MAX(5, ts / 4));
        rb->lcd_set_foreground(PC_RED_DEEP);
        rb->lcd_drawrect(x + 4, wall_y + 4, ts - 8, MAX(5, ts / 4));
        if (block_id == 0x3a)
        {
            rb->lcd_fillrect(x + ts / 2 - 3, wall_y + 11, 6, ts / 2);
            rb->lcd_set_foreground(PC_LAB_ROOF_A);
            rb->lcd_fillrect(x + 1, y, ts - 2, 3);
        }
    }
}

static void draw_shore_tile(int x, int y, unsigned char block_id)
{
    rb->lcd_set_foreground(PC_RED_LIGHT);
    rb->lcd_fillrect(x, y, PC_WORLD_TILE_SIZE, PC_WORLD_TILE_SIZE);
    rb->lcd_set_foreground(PC_RED_MID);
    rb->lcd_fillrect(x, y + 12, PC_WORLD_TILE_SIZE, 8);
    rb->lcd_set_foreground(PC_RED_DARK);

    if (block_id == 0x1d)
        rb->lcd_drawline(x + 1, y + 10, x + 19, y + 19);
    else if (block_id == 0x1e)
        rb->lcd_drawline(x + 1, y + 19, x + 19, y + 10);
    else
        rb->lcd_fillrect(x + 7, y + 4, 6, 5);
}

static void draw_water_tile(int x, int y)
{
    rb->lcd_set_foreground(LCD_RGBPACK(0x78, 0xae, 0x96));
    rb->lcd_fillrect(x, y, PC_WORLD_TILE_SIZE, PC_WORLD_TILE_SIZE);
    rb->lcd_set_foreground(PC_RED_LIGHT);
    rb->lcd_drawline(x + 2, y + 7, x + 17, y + 7);
    rb->lcd_drawline(x + 4, y + 13, x + 15, y + 13);
}

static void draw_block(unsigned char block_id, int tx, int ty)
{
    int x = tile_screen_x(tx);
    int y = tile_screen_y(ty);

    if (block_id == 0x0b || block_id == 0x0a || is_tree_border(block_id))
    {
        draw_tree_border_tile(x, y, block_id);
        return;
    }

    if (is_house_block(block_id))
    {
        draw_house_tile(x, y, block_id);
        return;
    }

    if (is_lab_block(block_id))
    {
        draw_lab_tile(x, y, block_id);
        return;
    }

    if (is_water_block(block_id))
    {
        draw_water_tile(x, y);
        return;
    }

    if (block_id == 0x1d || block_id == 0x1e || block_id == 0x31)
    {
        draw_shore_tile(x, y, block_id);
        return;
    }

    draw_grass_tile(x, y, block_id);
}

static void draw_map(const struct pc_world_state *world)
{
    int tx;
    int ty;

    rb->lcd_set_background(PC_RED_LIGHT);
    rb->lcd_clear_display();

    for (ty = 0; ty < PC_WORLD_H; ++ty)
    {
        for (tx = 0; tx < PC_WORLD_W; ++tx)
            draw_block(world->tiles[ty][tx], tx, ty);
    }
}

static void draw_player_fallback(const struct pc_world_state *world)
{
    int x = world->player_x;
    int y = world->player_y;

    rb->lcd_set_foreground(LCD_RGBPACK(0xca, 0x58, 0x5e));
    rb->lcd_fillrect(x - 7, y - 7, 14, 15);
    rb->lcd_set_foreground(PC_RED_LIGHT);
    xlcd_fillcircle(x, y - 12, 6);
    rb->lcd_set_foreground(PC_RED_DEEP);
    rb->lcd_fillrect(x - 7, y - 17, 14, 3);
}

static void draw_player(const struct pc_world_state *world)
{
    const struct pc_asset_bitmap *trainer =
        &world->assets.trainer[world->heading][world->walk_frame];
    int x = world->player_x;
    int y = world->player_y;

    fill_capsule(x - 10, y + 9, 20, 5, LCD_RGBPACK(0x60, 0x70, 0x58));
    if (trainer->loaded)
    {
        rb->lcd_bitmap_transparent((const fb_data *)trainer->bmp.data,
                                   x - trainer->bmp.width / 2,
                                   y - trainer->bmp.height + 10,
                                   trainer->bmp.width,
                                   trainer->bmp.height);
        return;
    }

    draw_player_fallback(world);
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
    draw_text_small(world->home_x - 11, world->home_y - 24, PC_RED_DEEP, "HOME");
}

static void draw_banner(const struct pc_world_state *world)
{
    char line1[PC_BANNER_LINE_CHARS];
    char line2[PC_BANNER_LINE_CHARS];

    fit_text_small(line1, sizeof(line1), world->banner.line1, LCD_WIDTH - 44);
    fit_text_small(line2, sizeof(line2), world->banner.line2, LCD_WIDTH - 44);
    fill_capsule(12, 6, LCD_WIDTH - 24, 24, PC_RED_PANEL);
    draw_text_small(22, 11, PC_RED_DEEP, line1);
    if (line2[0] != '\0')
        draw_text_small(22, 20, PC_RED_DARK, line2);
}

static void draw_hud(void)
{
    char hud[PC_BANNER_LINE_CHARS];

    fit_text_small(hud, sizeof(hud),
                   "Hold dir to walk   Menu+Select exits",
                   LCD_WIDTH - 66);
    fill_capsule(24, LCD_HEIGHT - 18, LCD_WIDTH - 48, 14, PC_RED_PANEL);
    draw_text_small(34, LCD_HEIGHT - 14, PC_RED_DEEP, hud);
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
