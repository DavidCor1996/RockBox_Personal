#include "ac_demake.h"

#define AC_TILE 16
#define AC_SKY LCD_RGBPACK(117, 190, 226)
#define AC_GRASS LCD_RGBPACK(111, 181, 91)
#define AC_GRASS_ALT LCD_RGBPACK(103, 172, 83)
#define AC_WATER LCD_RGBPACK(73, 148, 206)
#define AC_TREE LCD_RGBPACK(46, 130, 62)
#define AC_TREE_LIGHT LCD_RGBPACK(83, 164, 78)
#define AC_TRUNK LCD_RGBPACK(116, 76, 45)
#define AC_HOUSE LCD_RGBPACK(235, 213, 166)
#define AC_ROOF LCD_RGBPACK(193, 75, 70)
#define AC_INK LCD_RGBPACK(49, 44, 40)
#define AC_PANEL LCD_RGBPACK(250, 244, 218)

static void ac_text_center(int y, const char *text)
{
    int width;
    rb->lcd_getstringsize(text, &width, NULL);
    rb->lcd_putsxy((LCD_WIDTH - width) / 2, y, text);
}

static void ac_world_to_screen(const struct ac_state *state, int wx, int wy,
                               int *sx, int *sy)
{
    *sx = LCD_WIDTH / 2 + (wx - state->player_x) * AC_TILE;
    *sy = LCD_HEIGHT / 2 + 16 + (wy - state->player_y) * AC_TILE;
}

static void ac_draw_tree(const struct ac_state *state, int x, int y)
{
    int sx;
    int sy;
    ac_world_to_screen(state, x, y, &sx, &sy);
    if (sx < -20 || sx > LCD_WIDTH + 20 || sy < 20 ||
        sy > LCD_HEIGHT + 24)
        return;
    rb->lcd_set_foreground(AC_TRUNK);
    rb->lcd_fillrect(sx - 3, sy - 4, 6, 16);
    rb->lcd_set_foreground(AC_TREE);
    rb->lcd_fillrect(sx - 11, sy - 17, 22, 16);
    rb->lcd_set_foreground(AC_TREE_LIGHT);
    rb->lcd_fillrect(sx - 7, sy - 21, 14, 7);
}

static void ac_draw_house(const struct ac_state *state)
{
    int sx;
    int sy;
    ac_world_to_screen(state, 23, 18, &sx, &sy);
    if (sx < -70 || sx > LCD_WIDTH + 70 || sy < -50 ||
        sy > LCD_HEIGHT + 50)
        return;
    rb->lcd_set_foreground(AC_HOUSE);
    rb->lcd_fillrect(sx - 38, sy - 26, 76, 48);
    rb->lcd_set_foreground(AC_ROOF);
    rb->lcd_fillrect(sx - 45, sy - 36, 90, 14);
    rb->lcd_set_foreground(AC_INK);
    rb->lcd_fillrect(sx - 7, sy - 2, 14, 24);
}

static void ac_draw_villager(const struct ac_state *state,
                             const struct ac_villager *villager)
{
    static const fb_data colors[] = {
        LCD_RGBPACK(226, 152, 71), LCD_RGBPACK(123, 158, 211),
        LCD_RGBPACK(213, 116, 143), LCD_RGBPACK(142, 105, 174)
    };
    int sx;
    int sy;
    ac_world_to_screen(state, villager->position.x,
                       villager->position.y, &sx, &sy);
    if (sx < -12 || sx > LCD_WIDTH + 12 || sy < 20 ||
        sy > LCD_HEIGHT + 20)
        return;
    rb->lcd_set_foreground(colors[villager->kind % ARRAYLEN(colors)]);
    rb->lcd_fillrect(sx - 7, sy - 12, 14, 18);
    rb->lcd_fillrect(sx - 5, sy - 18, 10, 8);
    rb->lcd_set_foreground(AC_INK);
    rb->lcd_drawpixel(sx - 2, sy - 15);
    rb->lcd_drawpixel(sx + 2, sy - 15);
}

static void ac_draw_player(void)
{
    int x = LCD_WIDTH / 2;
    int y = LCD_HEIGHT / 2 + 16;
    rb->lcd_set_foreground(LCD_RGBPACK(53, 90, 164));
    rb->lcd_fillrect(x - 6, y - 11, 12, 17);
    rb->lcd_set_foreground(LCD_RGBPACK(236, 193, 148));
    rb->lcd_fillrect(x - 5, y - 18, 10, 8);
    rb->lcd_set_foreground(AC_INK);
    rb->lcd_drawpixel(x - 2, y - 15);
    rb->lcd_drawpixel(x + 2, y - 15);
}

static void ac_draw_clock(const struct ac_state *state)
{
    const struct tm *now = rb->get_time();
    char clock[48];

    rb->snprintf(clock, sizeof(clock), "%d:%02d  %lu Bells",
                 now ? now->tm_hour : 0, now ? now->tm_min : 0,
                 (unsigned long)state->bells);
    rb->lcd_set_foreground(AC_PANEL);
    rb->lcd_fillrect(6, 5, LCD_WIDTH - 12, 21);
    rb->lcd_set_foreground(AC_INK);
    rb->lcd_drawrect(6, 5, LCD_WIDTH - 12, 21);
    ac_text_center(9, clock);
}

static void ac_draw_overlay(const struct ac_state *state)
{
    int index;

    if (state->mode == AC_MODE_WORLD)
        return;
    rb->lcd_set_foreground(AC_PANEL);
    rb->lcd_fillrect(14, 54, LCD_WIDTH - 28, 132);
    rb->lcd_set_foreground(AC_INK);
    rb->lcd_drawrect(14, 54, LCD_WIDTH - 28, 132);
    if (state->mode == AC_MODE_DIALOGUE)
    {
        static const char *greetings[] = {
            "Nice weather, isn't it?",
            "I was hoping we'd meet!",
            "The town feels lively today.",
            "Let's talk again soon."
        };
        ac_text_center(78, "A neighbor says:");
        ac_text_center(112, greetings[state->dialogue_villager %
                                      ARRAYLEN(greetings)]);
        ac_text_center(154, "Center / Play to close");
    }
    else if (state->mode == AC_MODE_INVENTORY)
    {
        ac_text_center(66, "Pockets");
        for (index = 0; index < AC_INVENTORY_SIZE; ++index)
        {
            int x = 37 + (index % 4) * 68;
            int y = 98 + (index / 4) * 42;
            rb->lcd_set_foreground(index == state->selected_item ?
                                   LCD_RGBPACK(47, 94, 173) :
                                   LCD_RGBPACK(218, 207, 170));
            rb->lcd_fillrect(x, y, 40, 28);
            rb->lcd_set_foreground(index == state->selected_item ?
                                   LCD_WHITE : AC_INK);
            rb->lcd_drawrect(x, y, 40, 28);
        }
    }
    else
    {
        ac_text_center(72, "Paused");
        ac_text_center(108, "Center: Save & Quit");
        ac_text_center(140, "Play / MENU: Resume");
    }
}

void ac_render(const struct ac_state *state)
{
    int x;
    int y;
    int index;

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_set_background(AC_SKY);
    rb->lcd_clear_display();
    rb->lcd_set_foreground(AC_GRASS);
    rb->lcd_fillrect(0, 32, LCD_WIDTH, LCD_HEIGHT - 32);
    for (y = -9; y <= 9; ++y)
    {
        for (x = -12; x <= 12; ++x)
        {
            int wx = state->player_x + x;
            int wy = state->player_y + y;
            int sx;
            int sy;
            ac_world_to_screen(state, wx, wy, &sx, &sy);
            if ((wx + wy) & 1)
            {
                rb->lcd_set_foreground(AC_GRASS_ALT);
                rb->lcd_fillrect(sx - AC_TILE / 2, sy - AC_TILE / 2,
                                 AC_TILE, AC_TILE);
            }
            if (wx == 5 || wx == 6)
            {
                rb->lcd_set_foreground(AC_WATER);
                rb->lcd_fillrect(sx - AC_TILE / 2, sy - AC_TILE / 2,
                                 AC_TILE, AC_TILE);
            }
        }
    }
    ac_draw_house(state);
    for (index = 0; index < AC_TREE_COUNT; ++index)
        ac_draw_tree(state, state->trees[index].x, state->trees[index].y);
    for (index = 0; index < AC_VILLAGER_COUNT; ++index)
        ac_draw_villager(state, &state->villagers[index]);
    ac_draw_player();
    rb->lcd_set_drawmode(DRMODE_FG);
    ac_draw_clock(state);
    ac_draw_overlay(state);
    rb->lcd_update();
}
