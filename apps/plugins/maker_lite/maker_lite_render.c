/***************************************************************************
 * Direct LCD renderer. No allocation, decode, or filesystem access here.
 ***************************************************************************/

#include "maker_lite.h"

static void draw_cell(unsigned cell, int x, int y, bool transparent)
{
    const fb_data *pixels;
    int size = maker_lite.art.cell_size;

    if (!maker_lite.art.cells || cell >= maker_lite.art.cell_count ||
        x <= -size || y <= -size || x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;
    pixels = maker_lite.art.cells + (size_t)cell * size * size;
    if (transparent)
        rb->lcd_bitmap_transparent(pixels, x, y, size, size);
    else
        rb->lcd_bitmap(pixels, x, y, size, size);
}

static void draw_cell_oriented(unsigned cell, int x, int y,
                               bool transparent, bool mirror)
{
    const fb_data *pixels;
    int row;
    int column;
    int size = maker_lite.art.cell_size;

    if (!mirror)
    {
        draw_cell(cell, x, y, transparent);
        return;
    }
    if (!maker_lite.art.cells || cell >= maker_lite.art.cell_count ||
        size != 16 || x <= -size || y <= -size ||
        x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;
    pixels = maker_lite.art.cells + (size_t)cell * size * size;
    for (row = 0; row < size; ++row)
        for (column = 0; column < size; ++column)
            maker_lite.art.mirror_scratch[row * size + column] =
                pixels[row * size + size - 1 - column];
    if (transparent)
        rb->lcd_bitmap_transparent(maker_lite.art.mirror_scratch,
                                   x, y, size, size);
    else
        rb->lcd_bitmap(maker_lite.art.mirror_scratch, x, y, size, size);
}

static uint16_t frame_cell(const unsigned char *frame, unsigned index)
{
    const unsigned char *cell = frame + 4 + index * 2;

    return (uint16_t)cell[0] | ((uint16_t)cell[1] << 8);
}

static void draw_player_frame(unsigned frame_index, int anchor_x,
                              int anchor_y, bool mirror)
{
    const unsigned char *frame;
    int offset_x;
    int offset_y;
    unsigned columns;
    unsigned rows;
    unsigned row;
    unsigned column;

    if (!maker_lite.art.player_frames ||
        frame_index >= maker_lite.art.player_frame_count)
        return;
    frame = maker_lite.art.player_frames +
        frame_index * MAKER_LITE_PLAYER_FRAME_SIZE;
    columns = frame[0];
    rows = frame[1];
    offset_x = (int8_t)frame[2];
    offset_y = (int8_t)frame[3];
    for (row = 0; row < rows; ++row)
    {
        for (column = 0; column < columns; ++column)
        {
            uint16_t cell = frame_cell(frame, row * columns + column);
            int draw_x;

            if (cell == 0xffff)
                continue;
            draw_x = mirror ?
                anchor_x - offset_x - ((int)column + 1) * 16 :
                anchor_x + offset_x + (int)column * 16;
            draw_cell_oriented(
                cell, draw_x, anchor_y + offset_y + (int)row * 16,
                true, mirror);
        }
    }
}

static void draw_hud(void)
{
    char text[80];

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    if (maker_lite.level.flags & ML_LEVEL_BRAWL)
        rb->snprintf(
            text, sizeof(text), "P1 %03d%% x%d   CPU %03d%% x%d",
            maker_lite.world.brawl_player_damage,
            maker_lite.world.brawl_player_stocks,
            maker_lite.world.brawl_opponent_damage,
            maker_lite.world.brawl_opponent_stocks);
    else if (maker_lite.level.flags & ML_LEVEL_LIFE_SIM)
        rb->snprintf(
            text, sizeof(text), "CR %05lu DEBT %05lu SAL %02d H%d C%d",
            (unsigned long)maker_lite.world.credits,
            (unsigned long)maker_lite.world.debt,
            maker_lite.world.collectibles,
            maker_lite.world.house_level,
            maker_lite.world.car_level);
    else if (maker_lite.level.ruleset == ML_RULESET_SONIC)
        rb->snprintf(text, sizeof(text), "RINGS %03d  SCORE %05d",
                     maker_lite.world.rings, maker_lite.world.score);
    else if (maker_lite.level.ruleset == ML_RULESET_ZELDA)
        rb->snprintf(text, sizeof(text), "LIFE %d  KEYS %d",
                     maker_lite.world.player.health, maker_lite.world.keys);
    else
        rb->snprintf(text, sizeof(text), "x%02d  SCORE %05d",
                     maker_lite.world.collectibles, maker_lite.world.score);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 16);
    rb->lcd_putsxy(8, 3, text);
}

static const char *life_notice_text(unsigned notice)
{
    static const char * const notices[] = {
        "",
        "Salvage sold",
        "No salvage to sell",
        "Debt payment made",
        "Not enough credits",
        "House upgraded",
        "Pay debt before upgrading",
        "House is fully upgraded",
        "House style changed",
        "Car upgraded",
        "Car is fully upgraded",
        "Delivery shift paid",
        "Delivery shift cooling down",
        "Furniture picked up",
        "Furniture placed",
        "Furniture sold",
        "No room to place furniture",
    };

    return notice < ARRAYLEN(notices) ? notices[notice] : "";
}

static void draw_life_interaction(void)
{
    static const char * const house_options[] = {
        "Pay 100 credits",
        "Pay full debt",
        "Build / upgrade",
        "Change house style",
    };
    static const char * const shop_options[] = {
        "Sell one salvage",
        "Sell all salvage",
        "Upgrade car",
        "Sell carried furniture",
        "Leave shop",
    };
    static const char * const npc_options[] = {
        "Take delivery shift",
        "Leave",
    };
    static const char * const furniture_options[] = {
        "Pick up / move",
        "Sell furniture",
        "Leave it",
    };
    const char * const *options = NULL;
    unsigned count = 0;
    const char *title = "";
    char status[72];
    unsigned row;

    if (maker_lite.world.interaction == ML_LIFE_INTERACTION_HOUSE)
    {
        title = "PLAYER HOUSE";
        options = house_options;
        count = ARRAYLEN(house_options);
        rb->snprintf(
            status, sizeof(status), "Tier %u  Style %u  Debt %lu",
            maker_lite.world.house_level,
            maker_lite.world.house_style + 1,
            (unsigned long)maker_lite.world.debt);
    }
    else if (maker_lite.world.interaction == ML_LIFE_INTERACTION_SHOP)
    {
        title = "NEON NOOK SHOP";
        options = shop_options;
        count = ARRAYLEN(shop_options);
        rb->snprintf(
            status, sizeof(status), "Credits %lu  Salvage %d",
            (unsigned long)maker_lite.world.credits,
            maker_lite.world.collectibles);
    }
    else if (maker_lite.world.interaction == ML_LIFE_INTERACTION_NPC)
    {
        title = "CITY WORK";
        options = npc_options;
        count = ARRAYLEN(npc_options);
        rb->snprintf(
            status, sizeof(status), "Shift ready in %u",
            maker_lite.world.job_cooldown);
    }
    else if (
        maker_lite.world.interaction == ML_LIFE_INTERACTION_FURNITURE)
    {
        struct ml_entity furniture;
        unsigned value = 75;

        title = "APARTMENT FURNITURE";
        options = furniture_options;
        count = ARRAYLEN(furniture_options);
        if (maker_lite.world.interaction_entity <
                maker_lite.level.entity_count &&
            ml_level_entity(
                &maker_lite.level,
                maker_lite.world.interaction_entity,
                &furniture) &&
            furniture.param[2] > 0)
            value = furniture.param[2];
        rb->snprintf(
            status, sizeof(status), "Resale value %u credits", value);
    }
    if (!options)
        return;

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_fillrect(38, 42, LCD_WIDTH - 76, 158);
    rb->lcd_drawrect(38, 42, LCD_WIDTH - 76, 158);
    rb->lcd_putsxy(52, 54, title);
    rb->lcd_putsxy(52, 70, status);
    for (row = 0; row < count; ++row)
    {
        int y = 89 + row * 17;

        if (row == maker_lite.world.interaction_choice)
        {
            rb->lcd_set_foreground(LCD_RGBPACK(28, 105, 175));
            rb->lcd_fillrect(48, y - 2, LCD_WIDTH - 96, 17);
            rb->lcd_set_foreground(LCD_WHITE);
        }
        else
            rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_putsxy(55, y, options[row]);
    }
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_putsxy(52, 181, "< > Choose SELECT  PLAY Back");
}

static void draw_life_notice(void)
{
    const char *message;
    int width;

    if (!maker_lite.world.interaction_notice_ticks)
        return;
    message = life_notice_text(maker_lite.world.interaction_notice);
    if (!message[0])
        return;
    width = rb->font_getstringsize(message, NULL, NULL, FONT_SYSFIXED);
    width = MIN(width + 16, LCD_WIDTH - 20);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_fillrect((LCD_WIDTH - width) / 2, LCD_HEIGHT - 34, width, 20);
    rb->lcd_drawrect((LCD_WIDTH - width) / 2, LCD_HEIGHT - 34, width, 20);
    rb->lcd_putsxy((LCD_WIDTH - width) / 2 + 8, LCD_HEIGHT - 29, message);
}

static void draw_circle(int center_x, int center_y, int radius)
{
    int x = radius;
    int y = 0;
    int decision = 1 - radius;

    while (x >= y)
    {
        rb->lcd_drawpixel(center_x + x, center_y + y);
        rb->lcd_drawpixel(center_x + y, center_y + x);
        rb->lcd_drawpixel(center_x - y, center_y + x);
        rb->lcd_drawpixel(center_x - x, center_y + y);
        rb->lcd_drawpixel(center_x - x, center_y - y);
        rb->lcd_drawpixel(center_x - y, center_y - x);
        rb->lcd_drawpixel(center_x + y, center_y - x);
        rb->lcd_drawpixel(center_x + x, center_y - y);
        y++;
        if (decision <= 0)
            decision += 2 * y + 1;
        else
        {
            x--;
            decision += 2 * (y - x) + 1;
        }
    }
}

void maker_lite_render(void)
{
    long render_started = *rb->current_tick;
    int view_w = maker_lite.level.view_width;
    int view_h = maker_lite.level.view_height;
    int origin_x = (LCD_WIDTH - view_w) / 2;
    int origin_y = (LCD_HEIGHT - view_h) / 2;
    int camera_x = ml_fixed_to_int(maker_lite.world.camera_x);
    int camera_y = ml_fixed_to_int(maker_lite.world.camera_y);
    int tile_size = maker_lite.level.tile_size;
    int first_x = camera_x / tile_size;
    int first_y = camera_y / tile_size;
    int tiles_x = view_w / tile_size + 2;
    int tiles_y = view_h / tile_size + 2;
    unsigned i;
    int x;
    int y;
    unsigned player_action;
    unsigned player_cell;
    unsigned player_direction;

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();
    for (y = 0; y < tiles_y; y++)
    {
        for (x = 0; x < tiles_x; x++)
        {
            int map_x = first_x + x;
            int map_y = first_y + y;
            unsigned tile = ml_level_tile(&maker_lite.level, map_x, map_y);
            int screen_x = origin_x + map_x * tile_size - camera_x;
            int screen_y = origin_y + map_y * tile_size - camera_y;

            draw_cell(tile, screen_x, screen_y, false);
        }
    }

    for (i = 0; i < maker_lite.level.entity_count; i++)
    {
        struct ml_entity entity;
        unsigned render_cell;
        bool fighter_frame;
        int screen_x;
        int screen_y;

        if (!maker_lite.world.entity_alive[i] ||
            !ml_world_entity(&maker_lite.world, i, &entity) ||
            entity.kind == ML_ENTITY_PLAYER)
            continue;
        render_cell = entity.render_cell;
        if (!render_cell &&
            entity.kind < ARRAYLEN(maker_lite.art.entity_cells))
            render_cell = maker_lite.art.entity_cells[entity.kind];
        if (entity.kind == ML_ENTITY_HOUSE && entity.param[3] > 0)
        {
            unsigned variant =
                maker_lite.world.house_level * 3 +
                maker_lite.world.house_style;

            if (variant < (unsigned)entity.param[3])
                render_cell = entity.param[2] + variant;
        }
        else if (entity.kind == ML_ENTITY_CAR)
            render_cell += maker_lite.world.car_level;
        fighter_frame =
            (maker_lite.level.flags & ML_LEVEL_BRAWL) &&
            entity.kind == ML_ENTITY_ENEMY &&
            (entity.flags & 0x2000u) &&
            maker_lite.art.player_frames;
        if ((!fighter_frame && render_cell == 0) ||
            (fighter_frame &&
             render_cell >= maker_lite.art.player_frame_count) ||
            (!fighter_frame && render_cell >= maker_lite.art.cell_count))
            continue;
        screen_x = origin_x + entity.x - camera_x - tile_size / 2;
        screen_y = origin_y + entity.y - camera_y - tile_size;
        if (fighter_frame)
            draw_player_frame(
                render_cell,
                screen_x + tile_size / 2,
                screen_y + tile_size,
                maker_lite.world.entity_vx[i] < 0);
        else
            draw_cell(render_cell, screen_x, screen_y, true);
    }
    if (maker_lite.world.projectile_active &&
        maker_lite.world.projectile_kind <
            ARRAYLEN(maker_lite.art.entity_cells) &&
        maker_lite.art.entity_cells[maker_lite.world.projectile_kind])
        draw_cell(
            maker_lite.art.entity_cells[maker_lite.world.projectile_kind],
            origin_x + ml_fixed_to_int(maker_lite.world.projectile_x) -
                camera_x - tile_size / 2,
            origin_y + ml_fixed_to_int(maker_lite.world.projectile_y) -
                camera_y - tile_size / 2,
            true);
    for (i = 0; i < ML_MAX_LOOSE_RINGS; ++i)
    {
        if (!maker_lite.world.loose_ring_active[i] ||
            !maker_lite.art.entity_cells[ML_ENTITY_COLLECTIBLE])
            continue;
        draw_cell(
            maker_lite.art.entity_cells[ML_ENTITY_COLLECTIBLE],
            origin_x + ml_fixed_to_int(maker_lite.world.loose_ring_x[i]) -
                camera_x - tile_size / 2,
            origin_y + ml_fixed_to_int(maker_lite.world.loose_ring_y[i]) -
                camera_y - tile_size / 2,
            true);
    }

    x = origin_x + ml_fixed_to_int(maker_lite.world.player.x) -
        camera_x - tile_size / 2;
    y = origin_y + ml_fixed_to_int(maker_lite.world.player.y) -
        camera_y - tile_size;
    player_action = maker_lite.world.player.action < ML_ACTION_COUNT ?
                    maker_lite.world.player.action : ML_ACTION_IDLE;
    if (maker_lite.level.ruleset == ML_RULESET_ZELDA)
    {
        if (maker_lite.world.player.facing_y < 0)
            player_direction = 3;
        else if (maker_lite.world.player.facing_y > 0)
            player_direction = 1;
        else if (maker_lite.world.player.facing_x < 0)
            player_direction = 2;
        else
            player_direction = 0;
    }
    else
        player_direction = maker_lite.world.player.facing_x < 0 ? 2 : 0;
    player_cell =
        maker_lite.art.animation_start[player_direction][player_action] +
        (maker_lite.world.tick /
         maker_lite.art.animation_ticks[player_direction][player_action]) %
        maker_lite.art.animation_count[player_direction][player_action];
    if (maker_lite.world.car_active &&
        maker_lite.world.vehicle_entity < maker_lite.level.entity_count)
    {
        struct ml_entity vehicle;

        if (ml_level_entity(
                &maker_lite.level, maker_lite.world.vehicle_entity,
                &vehicle))
            draw_cell(
                vehicle.render_cell + maker_lite.world.car_level,
                x, y, true);
    }
    else if (maker_lite.art.player_frames)
        draw_player_frame(
            player_cell, x + tile_size / 2, y + tile_size,
            maker_lite.art.animation_mirror[player_direction][player_action]);
    else
        draw_cell_oriented(
            player_cell, x, y, true,
            maker_lite.art.animation_mirror[player_direction][player_action]);
    if (maker_lite.world.furniture_held_entity <
        maker_lite.level.entity_count)
    {
        struct ml_entity furniture;

        if (ml_level_entity(
                &maker_lite.level,
                maker_lite.world.furniture_held_entity,
                &furniture))
            draw_cell(furniture.render_cell, x, y - tile_size, true);
    }
    else if (maker_lite.world.player.carrying &&
             maker_lite.art.entity_cells[ML_ENTITY_POT])
        draw_cell(maker_lite.art.entity_cells[ML_ENTITY_POT],
                  x, y - tile_size, true);
    draw_hud();
    if (maker_lite.level.flags & ML_LEVEL_BRAWL)
    {
        rb->lcd_putsxy(140, 67, "Wheel: move  Up: jump");
        rb->lcd_putsxy(140, 87, "Select: light attack");
        rb->lcd_putsxy(140, 107, "Play: heavy attack");
        rb->lcd_putsxy(140, 127, "Ring out the CPU");
    }
    else if (maker_lite.level.flags & ML_LEVEL_LIFE_SIM)
    {
        draw_life_interaction();
        draw_life_notice();
    }
    if (maker_lite.world.complete)
    {
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_fillrect(72, 96, LCD_WIDTH - 144, 40);
        rb->lcd_putsxy(
            maker_lite.level.flags & ML_LEVEL_BRAWL ? 104 : 92,
            110,
            maker_lite.level.flags & ML_LEVEL_BRAWL ?
                (maker_lite.world.brawl_opponent_stocks == 0 ?
                 "YOU WIN" : "CPU WINS") :
                "LEVEL COMPLETE");
        rb->lcd_setfont(FONT_SYSFIXED);
        rb->lcd_putsxy(82, 126, "SELECT Replay  MENU Exit");
    }
    rb->lcd_update();
    maker_lite.rendered_frames++;
    if ((unsigned)(*rb->current_tick - render_started) >
        maker_lite.max_render_ticks)
        maker_lite.max_render_ticks =
            (unsigned)(*rb->current_tick - render_started);
}

void maker_lite_render_pause(bool locked)
{
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_fillrect(35, 62, LCD_WIDTH - 70, 116);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_drawrect(35, 62, LCD_WIDTH - 70, 116);
    rb->lcd_setfont(FONT_UI);
    rb->lcd_putsxy(locked ? 112 : 122, 74, locked ? "LOCKED" : "PAUSED");
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_putsxy(60, 108, "SELECT  Resume");
    rb->lcd_putsxy(60, 129, "PLAY    Controls");
    rb->lcd_putsxy(60, 150, "Hold MENU Save & Exit");
    rb->lcd_update();
}

void maker_lite_render_controls(void)
{
    rb->lcd_set_background(LCD_RGBPACK(235, 235, 235));
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_clear_display();
    rb->lcd_setfont(FONT_UI);
    rb->lcd_putsxy(10, 8, "Click Wheel Controls");
    rb->lcd_setfont(FONT_SYSFIXED);
    draw_circle(65, 119, 48);
    draw_circle(65, 119, 15);
    rb->lcd_putsxy(48, 61, "MENU");
    rb->lcd_putsxy(42, 112, "SELECT");
    rb->lcd_putsxy(18, 112, "<");
    rb->lcd_putsxy(106, 112, ">");
    rb->lcd_putsxy(48, 166, "PLAY");
    if (maker_lite.level.flags & ML_LEVEL_LIFE_SIM)
    {
        rb->lcd_putsxy(140, 67, "Wheel: walk / drive");
        rb->lcd_putsxy(140, 87, "Select: interact / place / exit");
        rb->lcd_putsxy(140, 107, "Play: close menu");
        rb->lcd_putsxy(140, 127, "< / >: menu choice");
    }
    else if (maker_lite.level.ruleset == ML_RULESET_ZELDA)
    {
        rb->lcd_putsxy(140, 67, "Wheel: move 8-way");
        rb->lcd_putsxy(140, 87, "Select: sword");
        rb->lcd_putsxy(140, 107, "Play: item");
        rb->lcd_putsxy(140, 127, "< / >: choose item");
    }
    else if (maker_lite.level.ruleset == ML_RULESET_SONIC)
    {
        rb->lcd_putsxy(140, 67, "Wheel: run / roll");
        rb->lcd_putsxy(140, 87, "Select: jump");
        rb->lcd_putsxy(140, 107, "Play: spin modifier");
        rb->lcd_putsxy(140, 127, "< / >: camera");
    }
    else
    {
        rb->lcd_putsxy(140, 67, "Wheel: move / crouch");
        rb->lcd_putsxy(140, 87, "Select: jump");
        rb->lcd_putsxy(140, 107, "Play: run / fire");
        rb->lcd_putsxy(140, 127, "< / >: camera");
    }
    rb->lcd_putsxy(140, 164, "MENU returns");
    rb->lcd_update();
}
