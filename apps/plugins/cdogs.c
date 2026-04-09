/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * C-Dogs Rockbox frontend prototype for iPod-style controls.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "plugin.h"
#include "pluginbitmaps/cdogs_enemy_legs.h"
#include "pluginbitmaps/cdogs_enemy_upper.h"
#include "pluginbitmaps/cdogs_objective.h"
#include "pluginbitmaps/cdogs_player_legs.h"
#include "pluginbitmaps/cdogs_player_upper.h"
#include "pluginbitmaps/cdogs_pulse.h"
#include "pluginbitmaps/cdogs_table.h"
#include "pluginbitmaps/cdogs_wall_n.h"
#include "pluginbitmaps/cdogs_wall_w.h"

#include <stdbool.h>
#include <string.h>

enum
{
    CDOGS_CMD_LEFT = 1,
    CDOGS_CMD_RIGHT = 2,
    CDOGS_CMD_UP = 4,
    CDOGS_CMD_DOWN = 8,
    CDOGS_CMD_FIRE = 16,
    CDOGS_CMD_MAP = 64,
    CDOGS_CMD_GRENADE = 256
};

struct actor
{
    int x;
    int y;
    int size;
    int hp;
    bool active;
};

struct bullet
{
    int x;
    int y;
    int vx;
    int vy;
    bool active;
};

struct game_state
{
    struct actor player;
    struct actor enemies[4];
    struct bullet bullets[10];
    struct bullet enemy_bullets[8];
    int facing_x;
    int facing_y;
    int score;
    int wave;
    int fire_cooldown;
    int grenade_cooldown;
    int enemy_fire_cooldown;
    int damage_flash;
    int grenade_flash;
    int enemy_step;
    int font_h;
    int field_x;
    int field_y;
    int field_w;
    int field_h;
    bool show_map;
    bool combo_latched;
    bool game_over;
};

#if LCD_DEPTH > 1
#define COLOR_BG LCD_RGBPACK(10, 12, 18)
#define COLOR_BORDER LCD_RGBPACK(80, 180, 140)
#define COLOR_PLAYER LCD_RGBPACK(235, 240, 255)
#define COLOR_ENEMY LCD_RGBPACK(255, 110, 95)
#define COLOR_BULLET LCD_RGBPACK(255, 215, 80)
#define COLOR_FLASH LCD_RGBPACK(255, 245, 180)
#define COLOR_TEXT LCD_RGBPACK(220, 230, 240)
#define COLOR_PANEL LCD_RGBPACK(22, 30, 42)
#define COLOR_MAP LCD_RGBPACK(90, 130, 255)
#define COLOR_DANGER LCD_RGBPACK(255, 80, 80)
#define COLOR_EBULLET LCD_RGBPACK(110, 255, 190)
#else
#define COLOR_BG LCD_BLACK
#define COLOR_BORDER LCD_WHITE
#define COLOR_PLAYER LCD_WHITE
#define COLOR_ENEMY LCD_WHITE
#define COLOR_BULLET LCD_WHITE
#define COLOR_FLASH LCD_WHITE
#define COLOR_TEXT LCD_WHITE
#define COLOR_PANEL LCD_BLACK
#define COLOR_MAP LCD_WHITE
#define COLOR_DANGER LCD_WHITE
#define COLOR_EBULLET LCD_WHITE
#endif

#define CDOGS_BODY_FRAME_SIZE 24
#define CDOGS_BODY_FRAMES 8
#define CDOGS_PULSE_FRAME_SIZE 5
#define CDOGS_PULSE_FRAMES 8

static const int env_tables[][2] = {
    {28, 30},
    {LCD_WIDTH - 54, 34},
    {42, LCD_HEIGHT - 62},
    {LCD_WIDTH - 66, LCD_HEIGHT - 66},
};

static int clean_button(const long button)
{
    return (int)(button & ~(BUTTON_REL | BUTTON_REPEAT | BUTTON_REDRAW));
}

static int map_buttons(const long button)
{
    int cmd = 0;
    const int clean = clean_button(button);

    if (clean & BUTTON_LEFT)
    {
        cmd |= CDOGS_CMD_LEFT;
    }
    if (clean & BUTTON_RIGHT)
    {
        cmd |= CDOGS_CMD_RIGHT;
    }
    if (clean & BUTTON_MENU)
    {
        cmd |= CDOGS_CMD_UP;
    }
    if (clean & BUTTON_PLAY)
    {
        cmd |= CDOGS_CMD_DOWN;
    }
    if (clean & BUTTON_SELECT)
    {
        cmd |= CDOGS_CMD_FIRE;
    }
    if ((clean & BUTTON_PLAY) && (clean & BUTTON_SELECT))
    {
        cmd |= CDOGS_CMD_GRENADE;
    }
    if ((clean & BUTTON_MENU) && (clean & BUTTON_SELECT))
    {
        cmd |= CDOGS_CMD_MAP;
    }

    return cmd;
}

static int clampi(const int value, const int low, const int high)
{
    if (value < low)
    {
        return low;
    }
    if (value > high)
    {
        return high;
    }
    return value;
}

static int step_dir(const int delta)
{
    if (delta < 0)
    {
        return -1;
    }
    if (delta > 0)
    {
        return 1;
    }
    return 0;
}

static bool intersects(
    const int ax, const int ay, const int as, const int bx, const int by,
    const int bs)
{
    return ax < bx + bs && ax + as > bx && ay < by + bs && ay + as > by;
}

static void set_fg(const unsigned color)
{
    rb->lcd_set_foreground(color);
}

static void enter_display_mode(void)
{
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_background(LCD_DEFAULT_BG);
    rb->lcd_set_foreground(LCD_DEFAULT_FG);
    rb->lcd_clear_display();
    rb->lcd_update();
}

static void leave_display_mode(void)
{
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_background(LCD_DEFAULT_BG);
    rb->lcd_set_foreground(LCD_DEFAULT_FG);
    rb->lcd_clear_display();
    rb->lcd_update();
}

static int facing_frame(const int x, const int y)
{
    if (x == 0 && y < 0)
    {
        return 0;
    }
    if (x > 0 && y < 0)
    {
        return 1;
    }
    if (x > 0 && y == 0)
    {
        return 2;
    }
    if (x > 0 && y > 0)
    {
        return 3;
    }
    if (x == 0 && y > 0)
    {
        return 4;
    }
    if (x < 0 && y > 0)
    {
        return 5;
    }
    if (x < 0 && y == 0)
    {
        return 6;
    }
    if (x < 0 && y < 0)
    {
        return 7;
    }
    return 0;
}

static void draw_sheet_frame(
    const fb_data *sheet, const int stride, const int frame, const int x,
    const int y)
{
    rb->lcd_bitmap_transparent_part(
        sheet, 0, frame * CDOGS_BODY_FRAME_SIZE, stride, x, y,
        CDOGS_BODY_FRAME_SIZE, CDOGS_BODY_FRAME_SIZE);
}

static void draw_actor_sprite(
    const struct actor *a, const fb_data *legs, const int legs_stride,
    const fb_data *upper, const int upper_stride, const int frame)
{
    const int draw_x = a->x - (CDOGS_BODY_FRAME_SIZE - a->size) / 2;
    const int draw_y = a->y - (CDOGS_BODY_FRAME_SIZE - a->size) / 2;

    if (!a->active)
    {
        return;
    }

    draw_sheet_frame(legs, legs_stride, frame, draw_x, draw_y);
    draw_sheet_frame(upper, upper_stride, frame, draw_x, draw_y);
}

static void draw_pulse(const struct bullet *b, const int frame, const unsigned fallback)
{
    if (!b->active)
    {
        return;
    }

    rb->lcd_bitmap_transparent_part(
        cdogs_pulse, frame * CDOGS_PULSE_FRAME_SIZE, 0, BMPWIDTH_cdogs_pulse,
        b->x - 1, b->y - 1, CDOGS_PULSE_FRAME_SIZE, CDOGS_PULSE_FRAME_SIZE);

    set_fg(fallback);
    rb->lcd_fillrect(b->x + 1, b->y + 1, 1, 1);
}

static void draw_environment(const struct game_state *g)
{
    int x;
    int y;
    int i;
    const int inner_x = g->field_x + 1;
    const int inner_y = g->field_y + 1;
    const int inner_w = g->field_w - 2;
    const int inner_h = g->field_h - 2;
    const int objective_x = g->field_x + g->field_w / 2 - BMPWIDTH_cdogs_objective / 2;
    const int objective_y = g->field_y + g->field_h / 2 - BMPHEIGHT_cdogs_objective / 2;

    set_fg(COLOR_BG);
    rb->lcd_fillrect(inner_x, inner_y, inner_w, inner_h);

    for (x = g->field_x + 2; x < g->field_x + g->field_w - 16; x += BMPWIDTH_cdogs_wall_n)
    {
        rb->lcd_bitmap_transparent_part(
            cdogs_wall_n, 0, 0, BMPWIDTH_cdogs_wall_n,
            x, g->field_y + 1, BMPWIDTH_cdogs_wall_n, BMPHEIGHT_cdogs_wall_n);
        rb->lcd_bitmap_transparent_part(
            cdogs_wall_n, 0, 0, BMPWIDTH_cdogs_wall_n,
            x, g->field_y + g->field_h - BMPHEIGHT_cdogs_wall_n - 1,
            BMPWIDTH_cdogs_wall_n, BMPHEIGHT_cdogs_wall_n);
    }

    for (y = g->field_y + 10; y < g->field_y + g->field_h - 24; y += BMPHEIGHT_cdogs_wall_w)
    {
        rb->lcd_bitmap_transparent_part(
            cdogs_wall_w, 0, 0, BMPWIDTH_cdogs_wall_w,
            g->field_x + 1, y, BMPWIDTH_cdogs_wall_w, BMPHEIGHT_cdogs_wall_w);
        rb->lcd_bitmap_transparent_part(
            cdogs_wall_w, 0, 0, BMPWIDTH_cdogs_wall_w,
            g->field_x + g->field_w - BMPWIDTH_cdogs_wall_w - 1, y,
            BMPWIDTH_cdogs_wall_w, BMPHEIGHT_cdogs_wall_w);
    }

    rb->lcd_bitmap_transparent_part(
        cdogs_objective, 0, 0, BMPWIDTH_cdogs_objective,
        objective_x, objective_y, BMPWIDTH_cdogs_objective, BMPHEIGHT_cdogs_objective);

    for (i = 0; i < (int)ARRAYLEN(env_tables); ++i)
    {
        rb->lcd_bitmap_transparent_part(
            cdogs_table, 0, 0, BMPWIDTH_cdogs_table,
            env_tables[i][0], env_tables[i][1],
            BMPWIDTH_cdogs_table, BMPHEIGHT_cdogs_table);
    }
}

static void respawn_enemy(struct game_state *g, const int index)
{
    static const int spawn_offsets[4][2] = {
        {8, 8},
        {LCD_WIDTH - 28, 8},
        {8, LCD_HEIGHT - 28},
        {LCD_WIDTH - 28, LCD_HEIGHT - 28},
    };
    struct actor *enemy = &g->enemies[index];

    enemy->size = 12;
    enemy->hp = 2 + (g->wave / 3);
    enemy->active = true;
    enemy->x = clampi(
        spawn_offsets[index % 4][0], g->field_x + 2,
        g->field_x + g->field_w - enemy->size - 2);
    enemy->y = clampi(
        spawn_offsets[index % 4][1], g->field_y + 2,
        g->field_y + g->field_h - enemy->size - 2);
}

static void reset_game(struct game_state *g)
{
    int i;

    rb->memset(g, 0, sizeof(*g));
    rb->lcd_getstringsize("A", NULL, &g->font_h);
    g->field_x = 4;
    g->field_y = g->font_h * 2 + 6;
    g->field_w = LCD_WIDTH - 8;
    g->field_h = LCD_HEIGHT - g->field_y - 4;

    g->player.size = 12;
    g->player.hp = 5;
    g->player.active = true;
    g->player.x = g->field_x + g->field_w / 2 - g->player.size / 2;
    g->player.y = g->field_y + g->field_h / 2 - g->player.size / 2;
    g->facing_x = 0;
    g->facing_y = -1;
    g->wave = 1;

    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        respawn_enemy(g, i);
    }
}

static void fire_bullet(struct game_state *g)
{
    int i;
    for (i = 0; i < (int)ARRAYLEN(g->bullets); ++i)
    {
        struct bullet *b = &g->bullets[i];
        if (b->active)
        {
            continue;
        }

        b->active = true;
        b->vx = g->facing_x * 5;
        b->vy = g->facing_y * 5;
        if (b->vx == 0 && b->vy == 0)
        {
            b->vy = -5;
        }
        b->x = g->player.x + g->player.size / 2 - 1;
        b->y = g->player.y + g->player.size / 2 - 1;
        g->fire_cooldown = HZ / 7;
        return;
    }
}

static void blast_grenade(struct game_state *g)
{
    int i;
    const int center_x = g->player.x + g->player.size / 2;
    const int center_y = g->player.y + g->player.size / 2;

    g->grenade_cooldown = HZ * 2;
    g->grenade_flash = 8;

    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        struct actor *enemy = &g->enemies[i];
        int dx;
        int dy;
        if (!enemy->active)
        {
            continue;
        }
        dx = (enemy->x + enemy->size / 2) - center_x;
        dy = (enemy->y + enemy->size / 2) - center_y;
        if (dx * dx + dy * dy <= 42 * 42)
        {
            enemy->hp = 0;
        }
    }
}

static void fire_enemy_bullet(struct game_state *g, const struct actor *enemy)
{
    int i;

    for (i = 0; i < (int)ARRAYLEN(g->enemy_bullets); ++i)
    {
        struct bullet *b = &g->enemy_bullets[i];
        if (b->active)
        {
            continue;
        }

        b->active = true;
        b->vx = step_dir(g->player.x - enemy->x) * 2;
        b->vy = step_dir(g->player.y - enemy->y) * 2;
        if (b->vx == 0 && b->vy == 0)
        {
            b->vy = 2;
        }
        b->x = enemy->x + enemy->size / 2 - 1;
        b->y = enemy->y + enemy->size / 2 - 1;
        g->enemy_fire_cooldown = MAX(HZ / 3, HZ - g->wave * 2);
        return;
    }
}

static void update_player(struct game_state *g, const int cmd)
{
    int dx = 0;
    int dy = 0;
    const int speed = 3;

    if (cmd & CDOGS_CMD_LEFT)
    {
        dx -= speed;
    }
    if (cmd & CDOGS_CMD_RIGHT)
    {
        dx += speed;
    }
    if (cmd & CDOGS_CMD_UP)
    {
        dy -= speed;
    }
    if (cmd & CDOGS_CMD_DOWN)
    {
        dy += speed;
    }

    if (dx != 0 || dy != 0)
    {
        g->facing_x = dx < 0 ? -1 : (dx > 0 ? 1 : 0);
        g->facing_y = dy < 0 ? -1 : (dy > 0 ? 1 : 0);
        g->player.x = clampi(
            g->player.x + dx, g->field_x + 1,
            g->field_x + g->field_w - g->player.size - 1);
        g->player.y = clampi(
            g->player.y + dy, g->field_y + 1,
            g->field_y + g->field_h - g->player.size - 1);
    }

    if ((cmd & CDOGS_CMD_FIRE) && g->fire_cooldown <= 0)
    {
        fire_bullet(g);
    }
    if ((cmd & CDOGS_CMD_GRENADE) && g->grenade_cooldown <= 0)
    {
        blast_grenade(g);
    }
}

static void update_bullets(struct game_state *g)
{
    int i;
    int j;

    for (i = 0; i < (int)ARRAYLEN(g->bullets); ++i)
    {
        struct bullet *b = &g->bullets[i];
        if (!b->active)
        {
            continue;
        }

        b->x += b->vx;
        b->y += b->vy;
        if (b->x < g->field_x || b->y < g->field_y ||
            b->x >= g->field_x + g->field_w - 1 ||
            b->y >= g->field_y + g->field_h - 1)
        {
            b->active = false;
            continue;
        }

        for (j = 0; j < (int)ARRAYLEN(g->enemies); ++j)
        {
            struct actor *enemy = &g->enemies[j];
            if (!enemy->active)
            {
                continue;
            }
            if (!intersects(b->x, b->y, 2, enemy->x, enemy->y, enemy->size))
            {
                continue;
            }

            b->active = false;
            enemy->hp--;
            if (enemy->hp <= 0)
            {
                enemy->active = false;
                g->score += 10;
            }
            break;
        }
    }
}

static void update_enemy_bullets(struct game_state *g)
{
    int i;

    for (i = 0; i < (int)ARRAYLEN(g->enemy_bullets); ++i)
    {
        struct bullet *b = &g->enemy_bullets[i];
        if (!b->active)
        {
            continue;
        }

        b->x += b->vx;
        b->y += b->vy;
        if (b->x < g->field_x || b->y < g->field_y ||
            b->x >= g->field_x + g->field_w - 1 ||
            b->y >= g->field_y + g->field_h - 1)
        {
            b->active = false;
            continue;
        }

        if (g->damage_flash == 0 &&
            intersects(b->x, b->y, 3, g->player.x, g->player.y, g->player.size))
        {
            b->active = false;
            g->player.hp--;
            g->damage_flash = HZ / 2;
            if (g->player.hp <= 0)
            {
                g->game_over = true;
            }
        }
    }
}

static void update_enemies(struct game_state *g)
{
    int i;
    int alive = 0;
    int shooter = -1;

    g->enemy_step++;
    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        struct actor *enemy = &g->enemies[i];
        int dx;
        int dy;

        if (!enemy->active)
        {
            continue;
        }
        alive++;
        shooter = i;

        if ((g->enemy_step + i) % 2 == 0)
        {
            dx = g->player.x - enemy->x;
            dy = g->player.y - enemy->y;
            if (dx < 0)
            {
                enemy->x--;
            }
            else if (dx > 0)
            {
                enemy->x++;
            }
            if (dy < 0)
            {
                enemy->y--;
            }
            else if (dy > 0)
            {
                enemy->y++;
            }
        }

        enemy->x = clampi(
            enemy->x, g->field_x + 1, g->field_x + g->field_w - enemy->size - 1);
        enemy->y = clampi(
            enemy->y, g->field_y + 1, g->field_y + g->field_h - enemy->size - 1);

        if (g->damage_flash == 0 &&
            intersects(
                g->player.x, g->player.y, g->player.size, enemy->x, enemy->y,
                enemy->size))
        {
            g->player.hp--;
            g->damage_flash = HZ / 2;
            enemy->x = g->field_x + g->field_w - enemy->size - 8;
            enemy->y = g->field_y + 8;
            if (g->player.hp <= 0)
            {
                g->game_over = true;
                return;
            }
        }
    }

    if (g->enemy_fire_cooldown <= 0 && shooter >= 0)
    {
        fire_enemy_bullet(g, &g->enemies[shooter]);
    }

    if (alive == 0)
    {
        g->wave++;
        for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
        {
            respawn_enemy(g, i);
        }
    }
}

static void draw_hud(const struct game_state *g)
{
    char line[64];

    set_fg(COLOR_TEXT);
    rb->snprintf(line, sizeof(line), "C-Dogs proto  HP:%d  Score:%d",
        g->player.hp, g->score);
    rb->lcd_putsxy(2, 0, line);

    rb->snprintf(line, sizeof(line), "Wave:%d  Play=down  Play+Sel=gren", g->wave);
    rb->lcd_putsxy(2, g->font_h, line);
}

static void draw_playfield(const struct game_state *g)
{
    int i;
    const int frame = facing_frame(g->facing_x, g->facing_y);
    const int pulse_frame = g->enemy_step % CDOGS_PULSE_FRAMES;
    const int pulse = g->grenade_flash * 4;

    draw_environment(g);
    set_fg(COLOR_BORDER);
    rb->lcd_drawrect(g->field_x, g->field_y, g->field_w, g->field_h);

    for (i = 0; i < (int)ARRAYLEN(g->bullets); ++i)
    {
        if (!g->bullets[i].active)
        {
            continue;
        }
        draw_pulse(&g->bullets[i], pulse_frame, COLOR_BULLET);
    }

    for (i = 0; i < (int)ARRAYLEN(g->enemy_bullets); ++i)
    {
        draw_pulse(&g->enemy_bullets[i], pulse_frame, COLOR_EBULLET);
    }

    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        draw_actor_sprite(
            &g->enemies[i], cdogs_enemy_legs, BMPWIDTH_cdogs_enemy_legs,
            cdogs_enemy_upper, BMPWIDTH_cdogs_enemy_upper, frame);
    }

    if (g->grenade_flash > 0)
    {
        set_fg(COLOR_FLASH);
        rb->lcd_drawrect(
            g->player.x - pulse, g->player.y - pulse,
            g->player.size + pulse * 2, g->player.size + pulse * 2);
    }

    draw_actor_sprite(
        &g->player, cdogs_player_legs, BMPWIDTH_cdogs_player_legs,
        cdogs_player_upper, BMPWIDTH_cdogs_player_upper, frame);

    if (g->damage_flash > 0)
    {
        set_fg(COLOR_DANGER);
        rb->lcd_drawrect(g->player.x - 2, g->player.y - 2,
            g->player.size + 4, g->player.size + 4);
    }
}

static void draw_map_overlay(const struct game_state *g)
{
    int i;
    const int map_w = 72;
    const int map_h = 54;
    const int map_x = LCD_WIDTH - map_w - 6;
    const int map_y = g->field_y + 6;
    const int px =
        map_x + ((g->player.x - g->field_x) * (map_w - 4)) / g->field_w + 2;
    const int py =
        map_y + ((g->player.y - g->field_y) * (map_h - 4)) / g->field_h + 2;

    set_fg(COLOR_PANEL);
    rb->lcd_fillrect(map_x, map_y, map_w, map_h);
    set_fg(COLOR_MAP);
    rb->lcd_drawrect(map_x, map_y, map_w, map_h);
    rb->lcd_fillrect(px, py, 3, 3);

    for (i = 0; i < (int)ARRAYLEN(g->enemies); ++i)
    {
        int ex;
        int ey;
        if (!g->enemies[i].active)
        {
            continue;
        }
        ex = map_x + ((g->enemies[i].x - g->field_x) * (map_w - 4)) / g->field_w + 2;
        ey = map_y + ((g->enemies[i].y - g->field_y) * (map_h - 4)) / g->field_h + 2;
        set_fg(COLOR_ENEMY);
        rb->lcd_fillrect(ex, ey, 2, 2);
    }
}

static void draw_game_over(const struct game_state *g)
{
    char line[32];
    const int box_w = LCD_WIDTH - 60;
    const int box_h = g->font_h * 4 + 12;
    const int box_x = (LCD_WIDTH - box_w) / 2;
    const int box_y = (LCD_HEIGHT - box_h) / 2;

    set_fg(COLOR_PANEL);
    rb->lcd_fillrect(box_x, box_y, box_w, box_h);
    set_fg(COLOR_BORDER);
    rb->lcd_drawrect(box_x, box_y, box_w, box_h);
    rb->lcd_putsxy(box_x + 10, box_y + 6, "Down for now");
    rb->snprintf(line, sizeof(line), "Final score: %d", g->score);
    rb->lcd_putsxy(box_x + 10, box_y + 6 + g->font_h, line);
    rb->lcd_putsxy(box_x + 10, box_y + 6 + g->font_h * 2, "Select = restart");
    rb->lcd_putsxy(box_x + 10, box_y + 6 + g->font_h * 3, "Hold switch = exit");
}

static void draw_screen(const struct game_state *g)
{
    rb->lcd_clear_display();
    draw_hud(g);
    draw_playfield(g);
    if (g->show_map)
    {
        draw_map_overlay(g);
    }
    if (g->game_over)
    {
        draw_game_over(g);
    }
    rb->lcd_update();
}

static void update_game(struct game_state *g, const int cmd)
{
    if (g->fire_cooldown > 0)
    {
        g->fire_cooldown--;
    }
    if (g->grenade_cooldown > 0)
    {
        g->grenade_cooldown--;
    }
    if (g->enemy_fire_cooldown > 0)
    {
        g->enemy_fire_cooldown--;
    }
    if (g->damage_flash > 0)
    {
        g->damage_flash--;
    }
    if (g->grenade_flash > 0)
    {
        g->grenade_flash--;
    }

    if (g->game_over)
    {
        return;
    }

    update_player(g, cmd);
    update_bullets(g);
    update_enemy_bullets(g);
    update_enemies(g);
}

enum plugin_status plugin_start(const void *parameter)
{
    struct game_state game;
    enum plugin_status status = PLUGIN_OK;

    (void)parameter;

    enter_display_mode();
    reset_game(&game);

    while (true)
    {
        const long event = rb->button_get_w_tmo(HZ / 30);
        const long buttons = rb->button_status();
        const int clean_event = clean_button(event);
        const int clean_status = clean_button(buttons);
        int cmd = map_buttons(buttons);

        if (event == SYS_USB_CONNECTED ||
            rb->default_event_handler(event) == SYS_USB_CONNECTED)
        {
            status = PLUGIN_USB_CONNECTED;
            break;
        }

#ifdef HAS_BUTTON_HOLD
        if (rb->button_hold())
        {
            rb->splash(HZ / 3, "Hold exit");
            status = PLUGIN_OK;
            break;
        }
#endif

        if ((clean_status & BUTTON_MENU) && (clean_status & BUTTON_SELECT))
        {
            if (!game.combo_latched)
            {
                game.show_map = !game.show_map;
                game.combo_latched = true;
            }
        }
        else
        {
            game.combo_latched = false;
        }

        if (game.game_over && (clean_event & BUTTON_SELECT))
        {
            reset_game(&game);
            continue;
        }

        if (game.combo_latched)
            cmd &= ~(CDOGS_CMD_MAP | CDOGS_CMD_FIRE | CDOGS_CMD_UP);

        if ((clean_status & BUTTON_PLAY) && (clean_status & BUTTON_SELECT))
            cmd &= ~CDOGS_CMD_DOWN;

        update_game(&game, cmd);
        draw_screen(&game);
    }

    leave_display_mode();
    return status;
}
