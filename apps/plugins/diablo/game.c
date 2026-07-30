/***************************************************************************
 * Gameplay layer on top of world.c's tile renderer: level transitions
 * (town <-> the level-1 dungeon), monsters with simple step-toward-player
 * AI, melee combat, a ground-item + inventory system, and save/load.
 *
 * Monster and item placement is data, not code: tools/diablo_assets/
 * dungeon_gen.py writes <name>.spawns (player start, the stairs trigger
 * cell, and initial monster/item positions) alongside the tile pack, so
 * this file never generates a layout itself -- it only interprets one.
 *
 * Entities are simple colored markers (no CL2 sprite decode -- see
 * apps/plugins/diablo/README.md for why that's out of scope right now),
 * drawn on top of world.c's tile background using world_screen_pos() so
 * they line up with whatever viewport world_render_background() just
 * composited.
 ****************************************************************************/
#include "plugin.h"
#include "lib/pluginlib_actions.h"
#include "world.h"
#include "game.h"

#define DIABLO_DATA_DIR PLUGIN_GAMES_DATA_DIR "/diablo"
#define SAVE_PATH DIABLO_DATA_DIR "/save.dat"
#define SAVE_MAGIC "SAVE"
#define SPAWN_MAGIC "SPWN"

#define MAX_MONSTERS 8
#define MAX_ITEMS 8
#define MAX_INVENTORY 20

#define LEVEL_TOWN 0
#define LEVEL_L1 1

#define MONSTER_FALLEN 0
#define MONSTER_ZOMBIE 1

#define ITEM_POTION 0

#define MOVE_SPEED 4                 /* held-button move, sub-cells/tick */
#define TAP_MOVE_SPEED (MOVE_SPEED * 4) /* discrete one-shot fallback move */
#define MONSTER_MOVE_SPEED 2

#define PLAYER_MAX_HP 100
#define PLAYER_ATTACK_DAMAGE 15
#define MONSTER_ATTACK_DAMAGE 8

#define STAIRS_RANGE_FP (WORLD_FP_ONE * 2)
#define ITEM_RANGE_FP WORLD_FP_ONE
#define ATTACK_RANGE_FP (WORLD_FP_ONE + WORLD_FP_ONE / 2)
#define ADJACENT_RANGE_FP WORLD_FP_ONE
#define AGGRO_RANGE_FP (WORLD_FP_ONE * 10)

#define MONSTER_AI_PERIOD_TICKS (HZ / 6)
#define MONSTER_ATTACK_COOLDOWN_TICKS HZ

enum { FACE_DOWN = 0, FACE_UP, FACE_LEFT, FACE_RIGHT };

struct monster
{
    bool alive;
    int kind;
    int x_fp, y_fp;
    int hp, max_hp;
    long next_attack_tick;
};

struct item_entity
{
    bool present;
    int kind;
    int x_fp, y_fp;
};

struct level_data
{
    bool loaded;
    int start_x_fp, start_y_fp;
    int stairs_x_fp, stairs_y_fp;
    int num_monsters;
    struct monster monsters[MAX_MONSTERS];
    int num_items;
    struct item_entity items[MAX_ITEMS];
};

static const struct button_mapping *plugin_contexts[] = {
    pla_main_ctx,
#ifdef HAVE_REMOTE_LCD
    pla_remote_ctx,
#endif
};

static struct level_data town_level, l1_level;
static int current_level;
static struct level_data *cur;

static int player_x_fp, player_y_fp;
static int player_hp, player_max_hp;
static int facing;

static int inventory[MAX_INVENTORY];
static int inventory_count;

static bool inventory_open;
static int inventory_sel;

static bool load_spawns(const char *name, struct level_data *lvl)
{
    char path[MAX_PATH];
    unsigned char buf[128];
    int fd, n, pos, i;

    rb->snprintf(path, sizeof(path), "%s/%s.spawns", DIABLO_DATA_DIR, name);
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return false;
    n = rb->read(fd, buf, sizeof(buf));
    rb->close(fd);
    if (n < 13 || rb->memcmp(buf, SPAWN_MAGIC, 4) != 0)
        return false;

    pos = 4;
    lvl->start_x_fp  = (buf[pos] | (buf[pos + 1] << 8)) << WORLD_FP_BITS; pos += 2;
    lvl->start_y_fp  = (buf[pos] | (buf[pos + 1] << 8)) << WORLD_FP_BITS; pos += 2;
    lvl->stairs_x_fp = (buf[pos] | (buf[pos + 1] << 8)) << WORLD_FP_BITS; pos += 2;
    lvl->stairs_y_fp = (buf[pos] | (buf[pos + 1] << 8)) << WORLD_FP_BITS; pos += 2;

    lvl->num_monsters = buf[pos++];
    if (lvl->num_monsters > MAX_MONSTERS)
        lvl->num_monsters = MAX_MONSTERS;
    for (i = 0; i < lvl->num_monsters; i++)
    {
        struct monster *m = &lvl->monsters[i];
        m->x_fp = (buf[pos] | (buf[pos + 1] << 8)) << WORLD_FP_BITS; pos += 2;
        m->y_fp = (buf[pos] | (buf[pos + 1] << 8)) << WORLD_FP_BITS; pos += 2;
        m->kind = buf[pos++];
        m->max_hp = m->hp = buf[pos++];
        m->alive = true;
        m->next_attack_tick = 0;
    }

    lvl->num_items = buf[pos++];
    if (lvl->num_items > MAX_ITEMS)
        lvl->num_items = MAX_ITEMS;
    for (i = 0; i < lvl->num_items; i++)
    {
        struct item_entity *it = &lvl->items[i];
        it->x_fp = (buf[pos] | (buf[pos + 1] << 8)) << WORLD_FP_BITS; pos += 2;
        it->y_fp = (buf[pos] | (buf[pos + 1] << 8)) << WORLD_FP_BITS; pos += 2;
        it->kind = buf[pos++];
        it->present = true;
    }

    return true;
}

static bool enter_level(int level)
{
    const char *name = (level == LEVEL_TOWN) ? "town" : "l1";
    struct level_data *lvl = (level == LEVEL_TOWN) ? &town_level : &l1_level;

    if (!world_load(name))
        return false;

    if (!lvl->loaded)
    {
        if (!load_spawns(name, lvl))
            return false;
        lvl->loaded = true;
    }

    current_level = level;
    cur = lvl;
    return true;
}

static bool within_range(int x1, int y1, int x2, int y2, int range_fp)
{
    int dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy <= range_fp * range_fp;
}

static void apply_move(int dx, int dy)
{
    if (dx != 0 && world_position_passable(player_x_fp + dx, player_y_fp))
        player_x_fp += dx;
    if (dy != 0 && world_position_passable(player_x_fp, player_y_fp + dy))
        player_y_fp += dy;

    if (dx > 0)      facing = FACE_RIGHT;
    else if (dx < 0) facing = FACE_LEFT;
    else if (dy > 0) facing = FACE_DOWN;
    else if (dy < 0) facing = FACE_UP;
}

static struct monster *nearest_monster_in_range(int range_fp)
{
    int i;
    struct monster *best = NULL;
    int best_d2 = range_fp * range_fp + 1;

    for (i = 0; i < cur->num_monsters; i++)
    {
        struct monster *m = &cur->monsters[i];
        int dx, dy, d2;
        if (!m->alive)
            continue;
        dx = player_x_fp - m->x_fp;
        dy = player_y_fp - m->y_fp;
        d2 = dx * dx + dy * dy;
        if (d2 <= best_d2)
        {
            best = m;
            best_d2 = d2;
        }
    }
    return best;
}

static void do_context_action(void)
{
    struct monster *m;
    int i;

    if (within_range(player_x_fp, player_y_fp, cur->stairs_x_fp, cur->stairs_y_fp, STAIRS_RANGE_FP))
    {
        if (current_level == LEVEL_TOWN)
        {
            if (enter_level(LEVEL_L1))
            {
                player_x_fp = l1_level.start_x_fp;
                player_y_fp = l1_level.start_y_fp;
            }
        }
        else
        {
            if (enter_level(LEVEL_TOWN))
            {
                player_x_fp = town_level.stairs_x_fp;
                player_y_fp = town_level.stairs_y_fp;
            }
        }
        return;
    }

    m = nearest_monster_in_range(ATTACK_RANGE_FP);
    if (m)
    {
        m->hp -= PLAYER_ATTACK_DAMAGE;
        if (m->hp <= 0)
            m->alive = false;
        return;
    }

    for (i = 0; i < cur->num_items; i++)
    {
        struct item_entity *it = &cur->items[i];
        if (!it->present)
            continue;
        if (within_range(player_x_fp, player_y_fp, it->x_fp, it->y_fp, ITEM_RANGE_FP))
        {
            if (inventory_count < MAX_INVENTORY)
            {
                inventory[inventory_count++] = it->kind;
                it->present = false;
            }
            return;
        }
    }
}

static void update_monsters(void)
{
    int i;
    long now = *rb->current_tick;

    for (i = 0; i < cur->num_monsters; i++)
    {
        struct monster *m = &cur->monsters[i];
        int dx, dy, d2;

        if (!m->alive)
            continue;

        dx = player_x_fp - m->x_fp;
        dy = player_y_fp - m->y_fp;
        d2 = dx * dx + dy * dy;
        if (d2 > AGGRO_RANGE_FP * AGGRO_RANGE_FP)
            continue;

        if (d2 > ADJACENT_RANGE_FP * ADJACENT_RANGE_FP)
        {
            int sx = (dx > 0) - (dx < 0);
            int sy = (dy > 0) - (dy < 0);
            if (sx != 0 && world_position_passable(m->x_fp + sx * MONSTER_MOVE_SPEED, m->y_fp))
                m->x_fp += sx * MONSTER_MOVE_SPEED;
            if (sy != 0 && world_position_passable(m->x_fp, m->y_fp + sy * MONSTER_MOVE_SPEED))
                m->y_fp += sy * MONSTER_MOVE_SPEED;
        }
        else if (now >= m->next_attack_tick)
        {
            player_hp -= MONSTER_ATTACK_DAMAGE;
            m->next_attack_tick = now + MONSTER_ATTACK_COOLDOWN_TICKS;
        }
    }
}

static void draw_hp_bar_screen(int sx, int sy, int hp, int max_hp, int width)
{
    int fill;
    if (max_hp <= 0)
        max_hp = 1;
    if (hp < 0)
        hp = 0;
    fill = width * hp / max_hp;
    if (fill > width)
        fill = width;
    if (fill < 0)
        fill = 0;
    rb->lcd_set_foreground(LCD_RGBPACK(80, 0, 0));
    rb->lcd_fillrect(sx - width / 2, sy, width, 3);
    rb->lcd_set_foreground(LCD_RGBPACK(0, 200, 0));
    rb->lcd_fillrect(sx - width / 2, sy, fill, 3);
}

static void draw_player(void)
{
    int sx, sy, nx = 0, ny = 0;

    world_screen_pos(player_x_fp, player_y_fp, &sx, &sy);
    rb->lcd_set_foreground(LCD_RGBPACK(255, 230, 60));
    rb->lcd_fillrect(sx - 6, sy - 6, 12, 12);

    switch (facing)
    {
    case FACE_DOWN:  ny = 9;  break;
    case FACE_UP:    ny = -9; break;
    case FACE_LEFT:  nx = -9; break;
    case FACE_RIGHT: nx = 9;  break;
    }
    rb->lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
    rb->lcd_fillrect(sx + nx - 2, sy + ny - 2, 4, 4);

    draw_hp_bar_screen(sx, sy - 14, player_hp, player_max_hp, 20);
}

static void draw_monster(const struct monster *m)
{
    int sx, sy, size;
    unsigned color;

    world_screen_pos(m->x_fp, m->y_fp, &sx, &sy);
    size = (m->kind == MONSTER_ZOMBIE) ? 14 : 10;
    if (sx < -size || sx > LCD_WIDTH + size || sy < -size || sy > LCD_HEIGHT + size)
        return;

    color = (m->kind == MONSTER_ZOMBIE) ? LCD_RGBPACK(60, 120, 40) : LCD_RGBPACK(150, 60, 50);
    rb->lcd_set_foreground(color);
    rb->lcd_fillrect(sx - size / 2, sy - size / 2, size, size);
    draw_hp_bar_screen(sx, sy - size / 2 - 6, m->hp, m->max_hp, size + 4);
}

static void draw_item(const struct item_entity *it)
{
    int sx, sy;

    world_screen_pos(it->x_fp, it->y_fp, &sx, &sy);
    if (sx < -8 || sx > LCD_WIDTH + 8 || sy < -8 || sy > LCD_HEIGHT + 8)
        return;

    rb->lcd_set_foreground(LCD_RGBPACK(60, 160, 255));
    rb->lcd_fillrect(sx - 4, sy - 4, 8, 8);
}

static void draw_stairs_marker(void)
{
    int sx, sy;

    world_screen_pos(cur->stairs_x_fp, cur->stairs_y_fp, &sx, &sy);
    if (sx < -10 || sx > LCD_WIDTH + 10 || sy < -10 || sy > LCD_HEIGHT + 10)
        return;

    rb->lcd_set_foreground(LCD_RGBPACK(230, 180, 255));
    rb->lcd_drawrect(sx - 10, sy - 6, 20, 12);
}

static const char *item_name(int kind)
{
    switch (kind)
    {
    case ITEM_POTION: return "Healing Potion";
    default:           return "Unknown Item";
    }
}

static void use_selected_item(void)
{
    int kind, i;

    if (inventory_count == 0)
        return;

    kind = inventory[inventory_sel];
    if (kind == ITEM_POTION)
    {
        player_hp += 40;
        if (player_hp > player_max_hp)
            player_hp = player_max_hp;
    }

    for (i = inventory_sel; i < inventory_count - 1; i++)
        inventory[i] = inventory[i + 1];
    inventory_count--;
    if (inventory_sel >= inventory_count)
        inventory_sel = inventory_count - 1;
    if (inventory_sel < 0)
        inventory_sel = 0;
}

static void draw_inventory(void)
{
    int i;
    char line[40];

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();
    rb->lcd_puts(0, 0, "INVENTORY");

    if (inventory_count == 0)
    {
        rb->lcd_puts(0, 2, "(empty)");
    }
    else
    {
        for (i = 0; i < inventory_count; i++)
        {
            rb->snprintf(line, sizeof(line), "%c %s",
                         (i == inventory_sel) ? '>' : ' ', item_name(inventory[i]));
            rb->lcd_puts(0, 2 + i, line);
        }
    }

    rb->snprintf(line, sizeof(line), "HP %d/%d", player_hp, player_max_hp);
    rb->lcd_puts(0, 14, line);
    rb->lcd_puts(0, 16, "SELECT:use  MENU/BACK:close");
    rb->lcd_update();
}

static bool save_game(void)
{
    unsigned char buf[256];
    int pos, i, fd;

    rb->memcpy(buf, SAVE_MAGIC, 4);
    pos = 4;
    buf[pos++] = (unsigned char)current_level;
    buf[pos++] = (unsigned char)(player_x_fp & 0xFF);
    buf[pos++] = (unsigned char)((player_x_fp >> 8) & 0xFF);
    buf[pos++] = (unsigned char)(player_y_fp & 0xFF);
    buf[pos++] = (unsigned char)((player_y_fp >> 8) & 0xFF);
    buf[pos++] = (unsigned char)(player_hp & 0xFF);
    buf[pos++] = (unsigned char)((player_hp >> 8) & 0xFF);
    buf[pos++] = (unsigned char)(player_max_hp & 0xFF);
    buf[pos++] = (unsigned char)((player_max_hp >> 8) & 0xFF);
    buf[pos++] = (unsigned char)inventory_count;
    for (i = 0; i < inventory_count; i++)
        buf[pos++] = (unsigned char)inventory[i];

    buf[pos++] = (unsigned char)l1_level.num_monsters;
    for (i = 0; i < l1_level.num_monsters; i++)
    {
        buf[pos++] = (unsigned char)l1_level.monsters[i].alive;
        buf[pos++] = (unsigned char)(l1_level.monsters[i].hp & 0xFF);
        buf[pos++] = (unsigned char)((l1_level.monsters[i].hp >> 8) & 0xFF);
    }
    buf[pos++] = (unsigned char)l1_level.num_items;
    for (i = 0; i < l1_level.num_items; i++)
        buf[pos++] = (unsigned char)l1_level.items[i].present;

    fd = rb->open(SAVE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    rb->write(fd, buf, pos);
    rb->close(fd);
    return true;
}

static bool load_save(void)
{
    unsigned char buf[256];
    int fd, n, pos, i, lim, ilim, saved_num_monsters, saved_num_items;

    fd = rb->open(SAVE_PATH, O_RDONLY);
    if (fd < 0)
        return false;
    n = rb->read(fd, buf, sizeof(buf));
    rb->close(fd);
    if (n < 14 || rb->memcmp(buf, SAVE_MAGIC, 4) != 0)
        return false;

    /* Load the fresh (dead-reset) spawn layouts first, then overlay the
     * saved alive/hp/present state on top -- keeps this file oblivious
     * to how many monsters/items a level happens to have. */
    if (!load_spawns("town", &town_level))
        return false;
    town_level.loaded = true;
    if (!load_spawns("l1", &l1_level))
        return false;
    l1_level.loaded = true;

    pos = 4;
    current_level = buf[pos++];
    player_x_fp = buf[pos] | (buf[pos + 1] << 8); pos += 2;
    player_y_fp = buf[pos] | (buf[pos + 1] << 8); pos += 2;
    player_hp = buf[pos] | (buf[pos + 1] << 8); pos += 2;
    player_max_hp = buf[pos] | (buf[pos + 1] << 8); pos += 2;
    inventory_count = buf[pos++];
    if (inventory_count > MAX_INVENTORY)
        inventory_count = MAX_INVENTORY;
    for (i = 0; i < inventory_count; i++)
        inventory[i] = buf[pos++];

    saved_num_monsters = buf[pos++];
    lim = saved_num_monsters < l1_level.num_monsters ? saved_num_monsters : l1_level.num_monsters;
    for (i = 0; i < lim; i++)
    {
        l1_level.monsters[i].alive = buf[pos++];
        l1_level.monsters[i].hp = buf[pos] | (buf[pos + 1] << 8);
        pos += 2;
    }
    pos += (saved_num_monsters - lim) * 3;

    saved_num_items = buf[pos++];
    ilim = saved_num_items < l1_level.num_items ? saved_num_items : l1_level.num_items;
    for (i = 0; i < ilim; i++)
        l1_level.items[i].present = buf[pos++];

    facing = FACE_DOWN;
    return enter_level(current_level);
}

bool game_run(void)
{
    long last_monster_tick = 0;
    bool select_repeat_armed = true;

    if (!load_save())
    {
        if (!enter_level(LEVEL_TOWN))
            return false;
        player_x_fp = town_level.start_x_fp;
        player_y_fp = town_level.start_y_fp;
        player_hp = player_max_hp = PLAYER_MAX_HP;
        inventory_count = 0;
        facing = FACE_DOWN;
    }

    rb->button_clear_queue();

    while (true)
    {
        int action = pluginlib_getaction(TIMEOUT_NOBLOCK, plugin_contexts, ARRAYLEN(plugin_contexts));
        int buttons;
        int dx = 0, dy = 0;

        if (inventory_open)
        {
            if (action == PLA_CANCEL || action == PLA_EXIT)
                inventory_open = false;
            else if (action == PLA_SELECT)
                use_selected_item();
            else if (action == PLA_UP || action == PLA_SCROLL_BACK)
            {
                if (inventory_sel > 0)
                    inventory_sel--;
            }
            else if (action == PLA_DOWN || action == PLA_SCROLL_FWD)
            {
                if (inventory_sel < inventory_count - 1)
                    inventory_sel++;
            }
            draw_inventory();
            rb->yield();
            continue;
        }

        if (action == PLA_CANCEL || action == PLA_EXIT)
            break;

        if (action == PLA_SELECT_REPEAT)
        {
            if (select_repeat_armed)
            {
                inventory_open = true;
                inventory_sel = 0;
                select_repeat_armed = false;
            }
        }
        else
        {
            select_repeat_armed = true;
            if (action == PLA_SELECT)
                do_context_action();
        }

        buttons = rb->button_status();
        if (buttons & BUTTON_LEFT)  dx -= MOVE_SPEED;
        if (buttons & BUTTON_RIGHT) dx += MOVE_SPEED;
        if (buttons & BUTTON_MENU)  dy -= MOVE_SPEED;
        if (buttons & BUTTON_PLAY)  dy += MOVE_SPEED;

        /* Discrete one-shot fallback: some hosts (notably the desktop
         * simulator) don't keep button_status() live between polls
         * unless the action queue is also drained, so a single tap
         * should still move a visible step even without a held state. */
        if (dx == 0 && dy == 0)
        {
            if (action == PLA_LEFT)  dx -= TAP_MOVE_SPEED;
            if (action == PLA_RIGHT) dx += TAP_MOVE_SPEED;
            if (action == PLA_UP)    dy -= TAP_MOVE_SPEED;
            if (action == PLA_DOWN)  dy += TAP_MOVE_SPEED;
        }

        if (dx != 0 || dy != 0)
            apply_move(dx, dy);

        if (*rb->current_tick - last_monster_tick >= MONSTER_AI_PERIOD_TICKS)
        {
            update_monsters();
            last_monster_tick = *rb->current_tick;
        }

        if (player_hp <= 0)
        {
            rb->splash(HZ, "You have died...");
            enter_level(LEVEL_TOWN);
            player_x_fp = town_level.start_x_fp;
            player_y_fp = town_level.start_y_fp;
            player_hp = player_max_hp;
        }

        world_render_background(player_x_fp, player_y_fp);
        draw_stairs_marker();
        {
            int i;
            for (i = 0; i < cur->num_items; i++)
                if (cur->items[i].present)
                    draw_item(&cur->items[i]);
            for (i = 0; i < cur->num_monsters; i++)
                if (cur->monsters[i].alive)
                    draw_monster(&cur->monsters[i]);
        }
        draw_player();
        world_present();

        rb->yield();
    }

    save_game();
    world_unload();
    return true;
}
