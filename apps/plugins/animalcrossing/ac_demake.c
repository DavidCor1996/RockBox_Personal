#include "ac_demake.h"

static struct ac_state game;

static uint32_t ac_random(uint32_t *seed)
{
    *seed = *seed * 1664525u + 1013904223u;
    return *seed;
}

void ac_state_new(struct ac_state *state)
{
    uint32_t seed;
    int index;

    rb->memset(state, 0, sizeof(*state));
    seed = (uint32_t)rb->mktime(rb->get_time());
    if (seed == 0)
        seed = 0x47414645u;
    state->town_seed = seed;
    state->player_x = AC_WORLD_W / 2;
    state->player_y = AC_WORLD_H / 2;
    state->facing_y = 1;
    state->bells = 1000;
    for (index = 0; index < AC_TREE_COUNT; ++index)
    {
        int x;
        int y;
        do {
            x = 2 + (int)(ac_random(&seed) % (AC_WORLD_W - 4));
            y = 2 + (int)(ac_random(&seed) % (AC_WORLD_H - 4));
        } while ((x > 19 && x < 29 && y > 14 && y < 24) ||
                 (x < 8 && y < 8));
        state->trees[index].x = x;
        state->trees[index].y = y;
    }
    for (index = 0; index < AC_VILLAGER_COUNT; ++index)
    {
        state->villagers[index].position.x =
            8 + (int)(ac_random(&seed) % (AC_WORLD_W - 16));
        state->villagers[index].position.y =
            8 + (int)(ac_random(&seed) % (AC_WORLD_H - 16));
        state->villagers[index].kind = index;
    }
    state->inventory[0] = 1;
    state->inventory[1] = 2;
    state->dirty = true;
}

static bool ac_blocked(int x, int y)
{
    int index;

    if (x < 1 || x >= AC_WORLD_W - 1 || y < 1 || y >= AC_WORLD_H - 1)
        return true;
    if (x == 5 || x == 6) /* river */
        return true;
    if (x >= 21 && x <= 26 && y >= 16 && y <= 20) /* house */
        return true;
    for (index = 0; index < AC_TREE_COUNT; ++index)
        if (game.trees[index].x == x && game.trees[index].y == y)
            return true;
    return false;
}

static void ac_move(int dx, int dy)
{
    int nx = game.player_x + dx;
    int ny = game.player_y + dy;

    game.facing_x = dx;
    game.facing_y = dy;
    if (!ac_blocked(nx, ny))
    {
        game.player_x = nx;
        game.player_y = ny;
        game.dirty = true;
    }
}

static int ac_near_villager(void)
{
    int index;

    for (index = 0; index < AC_VILLAGER_COUNT; ++index)
    {
        int dx = game.villagers[index].position.x - game.player_x;
        int dy = game.villagers[index].position.y - game.player_y;
        if (dx * dx + dy * dy <= 4)
            return index;
    }
    return -1;
}

static void ac_interact(void)
{
    int villager = ac_near_villager();

    if (villager >= 0)
    {
        game.dialogue_villager = villager;
        game.mode = AC_MODE_DIALOGUE;
    }
    else
    {
        game.bells += 10;
        game.dirty = true;
    }
}

static void ac_handle_press(int button)
{
    if (game.mode == AC_MODE_DIALOGUE)
    {
        if (button & (BUTTON_SELECT | BUTTON_PLAY))
            game.mode = AC_MODE_WORLD;
        return;
    }
    if (game.mode == AC_MODE_INVENTORY)
    {
        if (button & BUTTON_SCROLL_BACK)
            game.selected_item =
                (game.selected_item + AC_INVENTORY_SIZE - 1) %
                AC_INVENTORY_SIZE;
        else if (button & BUTTON_SCROLL_FWD)
            game.selected_item =
                (game.selected_item + 1) % AC_INVENTORY_SIZE;
        else if (button & (BUTTON_PLAY | BUTTON_MENU))
            game.mode = AC_MODE_WORLD;
        return;
    }
    if (game.mode == AC_MODE_PAUSE)
    {
        if (button & BUTTON_SELECT)
        {
            if (ac_save_write(&game))
            {
                game.dirty = false;
                game.quit = true;
            }
            else
                rb->splash(HZ * 2, "Could not save town");
        }
        else if (button & (BUTTON_PLAY | BUTTON_MENU))
            game.mode = AC_MODE_WORLD;
        return;
    }

    if (button & BUTTON_SELECT)
        ac_interact();
    else if (button & BUTTON_PLAY)
        game.mode = AC_MODE_INVENTORY;
    else if (button & BUTTON_MENU)
        game.mode = AC_MODE_PAUSE;
    else if (button & BUTTON_LEFT)
        ac_move(-1, 0);
    else if (button & BUTTON_RIGHT)
        ac_move(1, 0);
    else if (button & BUTTON_SCROLL_BACK)
        ac_move(0, -1);
    else if (button & BUTTON_SCROLL_FWD)
        ac_move(0, 1);
}

static void ac_poll_input(void)
{
    int button;

    while ((button = rb->button_get(false)) != BUTTON_NONE)
    {
        if (button == SYS_USB_CONNECTED)
        {
            game.usb = true;
            game.quit = true;
            return;
        }
        if (button & (BUTTON_REL | BUTTON_REPEAT))
            continue;
        ac_handle_press(button);
    }
#ifdef HAVE_WHEEL_POSITION
    if (game.mode == AC_MODE_WORLD && (game.frames % 4) == 0)
    {
        static const int8_t direction[16][2] = {
            { 0,-1}, { 0,-1}, { 1,-1}, { 1, 0},
            { 1, 0}, { 1, 0}, { 1, 1}, { 0, 1},
            { 0, 1}, { 0, 1}, {-1, 1}, {-1, 0},
            {-1, 0}, {-1, 0}, {-1,-1}, { 0,-1}
        };
        int position = rb->wheel_status();
        if (position >= 0)
        {
            int zone = ((position + 3) / 6) & 15;
            ac_move(direction[zone][0], direction[zone][1]);
        }
    }
#endif
#ifdef HAS_BUTTON_HOLD
    if (rb->button_hold())
        game.mode = AC_MODE_PAUSE;
#endif
}

enum plugin_status ac_demake_run(void)
{
    long next_tick;

    ac_state_new(&game);
    ac_save_load(&game);
    game.mode = AC_MODE_WORLD;
    game.dialogue_villager = -1;
    next_tick = *rb->current_tick;
    while (!game.quit)
    {
        ac_poll_input();
        ac_render(&game);
        game.frames++;
        next_tick += MAX(1, HZ / 30);
        while (!game.quit && TIME_BEFORE(*rb->current_tick, next_tick))
            rb->yield();
        if (TIME_AFTER(*rb->current_tick, next_tick + HZ))
            next_tick = *rb->current_tick;
    }
    return game.usb ? PLUGIN_USB_CONNECTED : PLUGIN_OK;
}
