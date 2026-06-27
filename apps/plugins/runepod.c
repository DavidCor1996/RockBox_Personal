/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * RunePod prototype: click-wheel-first fantasy RPG vertical slice.
 *
 ****************************************************************************/

#include "plugin.h"

#include <stdbool.h>
#include <stdint.h>

#if defined(HAVE_LCD_COLOR) && (LCD_WIDTH >= 320)

#define RP_FRAME_TICKS MAX(1, HZ / 30)
#define RP_TOP_H 18
#define RP_BOTTOM_H 34
#define RP_WORLD_TOP RP_TOP_H
#define RP_WORLD_BOTTOM (LCD_HEIGHT - RP_BOTTOM_H)
#define RP_TARGET_RADIUS 14
#define RP_PLAYER_SPEED 3
#define RP_MAX_TARGETS 8
#define RP_MAX_ACTIONS 4
#define RP_SMOKE_LOG PLUGIN_GAMES_DATA_DIR "/runepod-smoke.log"
#define RP_SPRITES_PATH PLUGIN_GAMES_DATA_DIR "/runepod/sprites/runepod_sprites.320x64x24.bmp"
#define RP_SPRITE_W 32
#define RP_SPRITE_H 32
#define RP_SPRITE_SHEET_W 320
#define RP_SPRITE_SHEET_H 64
#define RP_SPRITE_PIXELS (RP_SPRITE_SHEET_W * RP_SPRITE_SHEET_H)
#define RP_SPRITE_BYTES (RP_SPRITE_PIXELS * (int)sizeof(fb_data))

#define RP_COL_SKY LCD_RGBPACK(108, 158, 164)
#define RP_COL_GRASS LCD_RGBPACK(72, 126, 79)
#define RP_COL_GRASS_DARK LCD_RGBPACK(48, 92, 58)
#define RP_COL_PATH LCD_RGBPACK(143, 118, 82)
#define RP_COL_WOOD LCD_RGBPACK(99, 66, 42)
#define RP_COL_TREE LCD_RGBPACK(45, 112, 52)
#define RP_COL_STONE LCD_RGBPACK(112, 116, 111)
#define RP_COL_WATER LCD_RGBPACK(47, 99, 146)
#define RP_COL_FIRE LCD_RGBPACK(220, 92, 43)
#define RP_COL_PANEL LCD_RGBPACK(24, 25, 24)
#define RP_COL_PANEL_2 LCD_RGBPACK(42, 44, 42)
#define RP_COL_TEXT LCD_RGBPACK(238, 236, 220)
#define RP_COL_MUTED LCD_RGBPACK(175, 174, 159)
#define RP_COL_ACCENT LCD_RGBPACK(230, 194, 90)
#define RP_COL_PLAYER LCD_RGBPACK(68, 90, 167)
#define RP_COL_ENEMY LCD_RGBPACK(132, 55, 64)
#define RP_COL_FOCUS LCD_RGBPACK(255, 232, 92)

enum rp_view
{
    RP_VIEW_TITLE = 0,
    RP_VIEW_WORLD,
    RP_VIEW_ACTIONS,
    RP_VIEW_INVENTORY,
    RP_VIEW_LEVELS,
    RP_VIEW_DIALOGUE
};

enum rp_target_kind
{
    RP_TARGET_NPC = 0,
    RP_TARGET_TREE,
    RP_TARGET_ROCK,
    RP_TARGET_FISH,
    RP_TARGET_FIRE,
    RP_TARGET_BENCH,
    RP_TARGET_ENEMY,
    RP_TARGET_EXIT
};

enum rp_group
{
    RP_GROUP_NPC = 0,
    RP_GROUP_RESOURCE,
    RP_GROUP_CRAFT,
    RP_GROUP_COMBAT,
    RP_GROUP_EXIT,
    RP_GROUP_COUNT
};

enum rp_sprite
{
    RP_SPR_PLAYER = 0,
    RP_SPR_GUIDE,
    RP_SPR_SHOPKEEPER,
    RP_SPR_RATLING,
    RP_SPR_OAK,
    RP_SPR_COPPER,
    RP_SPR_POND,
    RP_SPR_FIRE,
    RP_SPR_WORKBENCH,
    RP_SPR_COMBAT,
    RP_SPR_HP,
    RP_SPR_COINS,
    RP_SPR_LOGS,
    RP_SPR_ORE,
    RP_SPR_FISH,
    RP_SPR_FOOD,
    RP_SPR_COMBAT_ICON
};

struct rp_target
{
    const char *name;
    const char *default_action;
    enum rp_target_kind kind;
    enum rp_group group;
    int x;
    int y;
};

struct rp_inventory
{
    int logs;
    int ore;
    int raw_fish;
    int food;
    int coins;
    int charms;
};

struct rp_skills
{
    int combat;
    int mining;
    int woodcutting;
    int fishing;
    int cooking;
    int crafting;
};

struct rp_game
{
    enum rp_view view;
    int player_x;
    int player_y;
    int dest_x;
    int dest_y;
    bool moving;
    bool pending_action;
    int selected;
    int action_selected;
    int action_count;
    const char *actions[RP_MAX_ACTIONS];
    struct rp_inventory inv;
    struct rp_skills xp;
    int hp;
    int enemy_hp;
    int quest_stage;
    long cooldown_until[RP_MAX_TARGETS];
    char message[96];
    char detail[96];
    bool quit;
};

static const struct rp_target rp_targets[RP_MAX_TARGETS] =
{
    { "Guide", "Talk", RP_TARGET_NPC, RP_GROUP_NPC, 64, 66 },
    { "Shop", "Trade", RP_TARGET_NPC, RP_GROUP_NPC, 252, 64 },
    { "Oak", "Chop", RP_TARGET_TREE, RP_GROUP_RESOURCE, 48, 146 },
    { "Copper", "Mine", RP_TARGET_ROCK, RP_GROUP_RESOURCE, 116, 168 },
    { "Pond", "Fish", RP_TARGET_FISH, RP_GROUP_RESOURCE, 258, 154 },
    { "Fire", "Cook", RP_TARGET_FIRE, RP_GROUP_CRAFT, 206, 116 },
    { "Workbench", "Craft", RP_TARGET_BENCH, RP_GROUP_CRAFT, 174, 72 },
    { "Ratling", "Attack", RP_TARGET_ENEMY, RP_GROUP_COMBAT, 266, 190 },
};

static struct rp_game game;
static struct bitmap rp_sprite_sheet;
static fb_data rp_sprite_pixels[RP_SPRITE_PIXELS];
static bool rp_sprites_loaded;

#ifdef SIMULATOR
static void rp_smoke_log(const char *event, int value)
{
    int fd = rb->open(RP_SMOKE_LOG, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
    {
        rb->fdprintf(fd, "event=%s value=%d tick=%ld\n",
                     event, value, *rb->current_tick);
        rb->close(fd);
    }
}
#else
static void rp_smoke_log(const char *event, int value)
{
    (void)event;
    (void)value;
}
#endif

static void rp_set_wheel_events(bool enabled)
{
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(enabled);
#else
    (void)enabled;
#endif
}

static bool rp_load_sprites(void)
{
    int rc;

    rb->memset(&rp_sprite_sheet, 0, sizeof(rp_sprite_sheet));
    rp_sprite_sheet.data = (char *)rp_sprite_pixels;
    rc = rb->read_bmp_file(RP_SPRITES_PATH, &rp_sprite_sheet,
                           RP_SPRITE_BYTES, FORMAT_NATIVE, NULL);
    rp_sprites_loaded = rc > 0 &&
                        rp_sprite_sheet.width == RP_SPRITE_SHEET_W &&
                        rp_sprite_sheet.height == RP_SPRITE_SHEET_H;
#ifdef SIMULATOR
    if (rp_sprites_loaded)
        rp_smoke_log("sprites_loaded", rc);
    else
        rp_smoke_log("sprites_missing", rc);
#endif
    return rp_sprites_loaded;
}

static int rp_iabs(int v)
{
    return v < 0 ? -v : v;
}

static bool rp_near_target(int target_index)
{
    const struct rp_target *target = &rp_targets[target_index];
    return rp_iabs(game.player_x - target->x) <= RP_TARGET_RADIUS &&
           rp_iabs(game.player_y - target->y) <= RP_TARGET_RADIUS;
}

static void rp_set_message(const char *line1, const char *line2)
{
    rb->strlcpy(game.message, line1 ? line1 : "", sizeof(game.message));
    rb->strlcpy(game.detail, line2 ? line2 : "", sizeof(game.detail));
}

static void rp_init_game(void)
{
    rb->memset(&game, 0, sizeof(game));
    game.view = RP_VIEW_TITLE;
    game.player_x = 160;
    game.player_y = 126;
    game.dest_x = game.player_x;
    game.dest_y = game.player_y;
    game.selected = 0;
    game.hp = 10;
    game.enemy_hp = 6;
    game.inv.coins = 4;
    game.inv.food = 1;
    rp_set_message("RunePod prototype", "Select starts, Menu exits");
}

static void rp_cycle_target(int delta)
{
    game.selected += delta;
    if (game.selected < 0)
        game.selected = RP_MAX_TARGETS - 1;
    else if (game.selected >= RP_MAX_TARGETS)
        game.selected = 0;
}

static void rp_cycle_group(int delta)
{
    enum rp_group group = rp_targets[game.selected].group;
    int wanted = (int)group + delta;
    int i;

    if (wanted < 0)
        wanted = RP_GROUP_COUNT - 1;
    else if (wanted >= RP_GROUP_COUNT)
        wanted = 0;

    for (i = 0; i < RP_MAX_TARGETS; i++)
    {
        int idx = (game.selected + 1 + i) % RP_MAX_TARGETS;
        if ((int)rp_targets[idx].group == wanted)
        {
            game.selected = idx;
            return;
        }
    }
}

static const char *rp_group_name(enum rp_group group)
{
    switch (group)
    {
        case RP_GROUP_NPC: return "NPC";
        case RP_GROUP_RESOURCE: return "Resource";
        case RP_GROUP_CRAFT: return "Craft";
        case RP_GROUP_COMBAT: return "Combat";
        case RP_GROUP_EXIT: return "Exit";
        default: return "Target";
    }
}

static void rp_begin_walk_to_selected(bool with_action)
{
    const struct rp_target *target = &rp_targets[game.selected];
    game.dest_x = target->x;
    game.dest_y = target->y;
    game.moving = true;
    game.pending_action = with_action;
    rp_set_message(with_action ? "Walking to target" : "Walking",
                   target->name);
}

static void rp_add_xp(enum rp_target_kind kind, int amount)
{
    switch (kind)
    {
        case RP_TARGET_TREE: game.xp.woodcutting += amount; break;
        case RP_TARGET_ROCK: game.xp.mining += amount; break;
        case RP_TARGET_FISH: game.xp.fishing += amount; break;
        case RP_TARGET_FIRE: game.xp.cooking += amount; break;
        case RP_TARGET_BENCH: game.xp.crafting += amount; break;
        case RP_TARGET_ENEMY: game.xp.combat += amount; break;
        default: break;
    }
}

static void rp_talk_guide(void)
{
    if (game.quest_stage == 0)
    {
        game.quest_stage = 1;
        rp_set_message("Guide: Bring supplies",
                       "3 logs, 2 ore, and 1 cooked fish");
    }
    else if (game.quest_stage == 1 &&
             game.inv.logs >= 3 && game.inv.ore >= 2 && game.inv.food >= 1)
    {
        game.inv.logs -= 3;
        game.inv.ore -= 2;
        game.inv.food -= 1;
        game.inv.coins += 12;
        game.inv.charms += 1;
        game.quest_stage = 2;
        rp_set_message("Quest complete",
                       "+12 coins, +1 village charm");
    }
    else if (game.quest_stage == 1)
    {
        rp_set_message("Guide: Keep gathering",
                       "Need 3 logs, 2 ore, 1 cooked fish");
    }
    else
    {
        rp_set_message("Guide: The cave is open",
                       "Try fighting the ratling");
    }
}

static void rp_trade_shop(void)
{
    if (game.inv.coins >= 3)
    {
        game.inv.coins -= 3;
        game.inv.food += 1;
        rp_set_message("Bought field ration",
                       "-3 coins, +1 food");
    }
    else
    {
        rp_set_message("Shopkeeper",
                       "Food costs 3 coins");
    }
}

static void rp_execute_selected_action(void)
{
    const struct rp_target *target = &rp_targets[game.selected];
    long now = *rb->current_tick;

    game.pending_action = false;

    if (!rp_near_target(game.selected))
    {
        rp_begin_walk_to_selected(true);
        return;
    }

    if (game.cooldown_until[game.selected] > now)
    {
        rp_set_message("Still recovering", target->name);
        return;
    }

    switch (target->kind)
    {
        case RP_TARGET_NPC:
            if (game.selected == 0)
                rp_talk_guide();
            else
                rp_trade_shop();
            break;

        case RP_TARGET_TREE:
            game.inv.logs++;
            rp_add_xp(target->kind, 5);
            game.cooldown_until[game.selected] = now + HZ * 3;
            rp_set_message("You chop the oak",
                           "+1 log, +5 woodcutting XP");
            break;

        case RP_TARGET_ROCK:
            game.inv.ore++;
            rp_add_xp(target->kind, 5);
            game.cooldown_until[game.selected] = now + HZ * 3;
            rp_set_message("You mine copper",
                           "+1 ore, +5 mining XP");
            break;

        case RP_TARGET_FISH:
            game.inv.raw_fish++;
            rp_add_xp(target->kind, 4);
            game.cooldown_until[game.selected] = now + HZ * 2;
            rp_set_message("You catch a fish",
                           "+1 raw fish, +4 fishing XP");
            break;

        case RP_TARGET_FIRE:
            if (game.inv.raw_fish > 0)
            {
                game.inv.raw_fish--;
                game.inv.food++;
                rp_add_xp(target->kind, 4);
                rp_set_message("The fish cooks cleanly",
                               "+1 food, +4 cooking XP");
            }
            else
            {
                rp_set_message("Nothing to cook",
                               "Catch fish at the pond");
            }
            break;

        case RP_TARGET_BENCH:
            if (game.inv.logs >= 2 && game.inv.ore >= 1)
            {
                game.inv.logs -= 2;
                game.inv.ore -= 1;
                game.inv.coins += 4;
                rp_add_xp(target->kind, 6);
                rp_set_message("You craft a tool haft",
                               "+4 coins, +6 crafting XP");
            }
            else
            {
                rp_set_message("Workbench",
                               "Needs 2 logs and 1 ore");
            }
            break;

        case RP_TARGET_ENEMY:
            game.enemy_hp -= 2 + (rb->rand() % 2);
            if (game.enemy_hp <= 0)
            {
                game.inv.coins += 2;
                game.enemy_hp = 6;
                rp_add_xp(target->kind, 8);
                rp_set_message("Ratling defeated",
                               "+2 coins, +8 combat XP");
            }
            else
            {
                if (game.inv.food > 0 && game.hp <= 4)
                {
                    game.inv.food--;
                    game.hp += 4;
                    if (game.hp > 10)
                        game.hp = 10;
                    rp_set_message("You eat food mid-fight",
                                   "Recovered health");
                }
                else
                {
                    game.hp--;
                    rp_set_message("You strike the ratling",
                                   "It claws back");
                    if (game.hp <= 0)
                    {
                        game.hp = 10;
                        game.player_x = 160;
                        game.player_y = 126;
                        game.moving = false;
                        rp_set_message("You retreat to the square",
                                       "Health restored");
                    }
                }
            }
            break;

        case RP_TARGET_EXIT:
            rp_set_message("Village gate",
                           "More zones land after the input slice");
            break;
    }
}

static void rp_open_action_menu(void)
{
    const struct rp_target *target = &rp_targets[game.selected];

    game.action_selected = 0;
    game.actions[0] = target->default_action;
    game.actions[1] = "Walk here";
    game.actions[2] = "Examine";
    game.action_count = 3;

    if (target->kind == RP_TARGET_ENEMY)
    {
        game.actions[1] = "Use food";
        game.actions[2] = "Walk here";
        game.actions[3] = "Examine";
        game.action_count = 4;
    }

    game.view = RP_VIEW_ACTIONS;
}

static void rp_execute_menu_action(void)
{
    const char *action = game.actions[game.action_selected];

    if (!rb->strcmp(action, "Walk here"))
    {
        game.view = RP_VIEW_WORLD;
        rp_begin_walk_to_selected(false);
    }
    else if (!rb->strcmp(action, "Use food"))
    {
        if (game.inv.food > 0 && game.hp < 10)
        {
            game.inv.food--;
            game.hp += 4;
            if (game.hp > 10)
                game.hp = 10;
            rp_set_message("You eat food", "Health recovered");
        }
        else
        {
            rp_set_message("No useful food", "Buy or cook more");
        }
        game.view = RP_VIEW_WORLD;
    }
    else if (!rb->strcmp(action, "Examine"))
    {
        rb->snprintf(game.message, sizeof(game.message), "%s",
                     rp_targets[game.selected].name);
        rb->snprintf(game.detail, sizeof(game.detail), "%s target, %s group",
                     rp_targets[game.selected].default_action,
                     rp_group_name(rp_targets[game.selected].group));
        game.view = RP_VIEW_DIALOGUE;
    }
    else
    {
        game.view = RP_VIEW_WORLD;
        rp_execute_selected_action();
    }
}

static void rp_update_movement(void)
{
    int dx;
    int dy;

    if (!game.moving)
        return;

    dx = game.dest_x - game.player_x;
    dy = game.dest_y - game.player_y;

    if (rp_iabs(dx) <= RP_PLAYER_SPEED && rp_iabs(dy) <= RP_PLAYER_SPEED)
    {
        game.player_x = game.dest_x;
        game.player_y = game.dest_y;
        game.moving = false;
        if (game.pending_action)
            rp_execute_selected_action();
        return;
    }

    if (dx > 0)
        game.player_x += MIN(dx, RP_PLAYER_SPEED);
    else if (dx < 0)
        game.player_x += MAX(dx, -RP_PLAYER_SPEED);

    if (dy > 0)
        game.player_y += MIN(dy, RP_PLAYER_SPEED);
    else if (dy < 0)
        game.player_y += MAX(dy, -RP_PLAYER_SPEED);
}

static void rp_draw_text_clip(int x, int y, const char *text, int max_chars)
{
    char buf[64];
    if ((int)rb->strlen(text) > max_chars)
    {
        rb->strlcpy(buf, text, MIN((int)sizeof(buf), max_chars + 1));
        rb->strlcat(buf, "...", sizeof(buf));
        rb->lcd_putsxy(x, y, buf);
    }
    else
    {
        rb->lcd_putsxy(x, y, text);
    }
}

static void rp_fill(int color, int x, int y, int w, int h)
{
    rb->lcd_set_foreground(color);
    rb->lcd_fillrect(x, y, w, h);
}

static void rp_rect(int color, int x, int y, int w, int h)
{
    rb->lcd_set_foreground(color);
    rb->lcd_drawrect(x, y, w, h);
}

static int rp_anim_frame(int frames_per_second)
{
    int ticks_per_frame = MAX(1, HZ / frames_per_second);
    return (int)((*rb->current_tick / ticks_per_frame) & 1);
}

static int rp_skill_level(int xp)
{
    return 1 + xp / 10;
}

static int rp_skill_progress(int xp)
{
    return xp % 10;
}

static void rp_draw_sprite(enum rp_sprite sprite, int x, int y)
{
    int sx;
    int sy;
    int stride;

    if (!rp_sprites_loaded)
        return;

    sx = ((int)sprite % 10) * RP_SPRITE_W;
    sy = ((int)sprite / 10) * RP_SPRITE_H;
    stride = STRIDE(SCREEN_MAIN, RP_SPRITE_SHEET_W, RP_SPRITE_SHEET_H);
    rb->lcd_bitmap_transparent_part(rp_sprite_pixels, sx, sy,
                                    stride, x, y,
                                    RP_SPRITE_W, RP_SPRITE_H);
}

static void rp_draw_sprite_anchor(enum rp_sprite sprite, int x, int y)
{
    rp_draw_sprite(sprite, x - RP_SPRITE_W / 2, y - RP_SPRITE_H);
}

static int rp_target_anim_offset(enum rp_target_kind kind, bool selected)
{
    int frame = rp_anim_frame(3);

    switch (kind)
    {
        case RP_TARGET_FIRE:
        case RP_TARGET_FISH:
            return frame ? -1 : 0;
        case RP_TARGET_NPC:
        case RP_TARGET_ENEMY:
            return (selected && frame) ? -1 : 0;
        default:
            return 0;
    }
}

static void rp_draw_sprite_effect(enum rp_target_kind kind, int x, int y)
{
    int frame = rp_anim_frame(4);

    switch (kind)
    {
        case RP_TARGET_FIRE:
            rp_fill(frame ? RP_COL_ACCENT : LCD_RGBPACK(245, 142, 58),
                    x - 2, y - 20, 4, 9);
            break;
        case RP_TARGET_FISH:
            rb->lcd_set_foreground(frame ? LCD_RGBPACK(164, 210, 222)
                                         : LCD_RGBPACK(91, 151, 181));
            rb->lcd_hline(x - 9, x + 10, y - 15);
            rb->lcd_hline(x - 6, x + 7, y - 10);
            break;
        case RP_TARGET_ENEMY:
            if (frame)
                rp_rect(LCD_RGBPACK(180, 68, 72), x - 13, y - 28, 26, 25);
            break;
        default:
            break;
    }
}

static void rp_draw_tree(int x, int y)
{
    rp_fill(RP_COL_WOOD, x - 3, y - 2, 6, 16);
    rp_fill(RP_COL_TREE, x - 12, y - 14, 24, 18);
    rp_fill(RP_COL_GRASS_DARK, x - 8, y - 19, 16, 11);
}

static void rp_draw_rock(int x, int y)
{
    rp_fill(RP_COL_STONE, x - 10, y - 8, 20, 15);
    rp_fill(LCD_RGBPACK(82, 85, 82), x - 5, y - 12, 13, 9);
}

static void rp_draw_fish(int x, int y)
{
    rp_fill(RP_COL_WATER, x - 18, y - 10, 36, 20);
    rp_rect(LCD_RGBPACK(143, 184, 205), x - 18, y - 10, 36, 20);
    rp_fill(LCD_RGBPACK(190, 210, 218), x - 5, y - 2, 10, 4);
}

static void rp_draw_npc(int x, int y, int color)
{
    rp_fill(color, x - 5, y - 14, 10, 16);
    rp_fill(LCD_RGBPACK(204, 164, 120), x - 4, y - 21, 8, 7);
}

static void rp_draw_target(int i)
{
    const struct rp_target *target = &rp_targets[i];
    bool selected = i == game.selected;
    enum rp_sprite sprite = RP_SPR_GUIDE;
    int y_offset;

    if (selected)
    {
        int pulse = rp_anim_frame(5);
        rp_rect(pulse ? RP_COL_FOCUS : RP_COL_ACCENT,
                target->x - 17, target->y - 24, 34, 34);
    }

    switch (target->kind)
    {
        case RP_TARGET_TREE: sprite = RP_SPR_OAK; break;
        case RP_TARGET_ROCK: sprite = RP_SPR_COPPER; break;
        case RP_TARGET_FISH: sprite = RP_SPR_POND; break;
        case RP_TARGET_FIRE: sprite = RP_SPR_FIRE; break;
        case RP_TARGET_BENCH: sprite = RP_SPR_WORKBENCH; break;
        case RP_TARGET_ENEMY: sprite = RP_SPR_RATLING; break;
        case RP_TARGET_EXIT: sprite = RP_SPR_COMBAT; break;
        case RP_TARGET_NPC:
        default:
            sprite = i == 0 ? RP_SPR_GUIDE : RP_SPR_SHOPKEEPER;
            break;
    }

    if (rp_sprites_loaded)
    {
        y_offset = rp_target_anim_offset(target->kind, selected);
        rp_draw_sprite_anchor(sprite, target->x, target->y + y_offset);
        rp_draw_sprite_effect(target->kind, target->x, target->y + y_offset);
        return;
    }

    switch (target->kind)
    {
        case RP_TARGET_TREE:
            rp_draw_tree(target->x, target->y);
            break;
        case RP_TARGET_ROCK:
            rp_draw_rock(target->x, target->y);
            break;
        case RP_TARGET_FISH:
            rp_draw_fish(target->x, target->y);
            break;
        case RP_TARGET_FIRE:
            rp_fill(RP_COL_FIRE, target->x - 6, target->y - 10, 12, 16);
            rp_fill(RP_COL_ACCENT, target->x - 3, target->y - 7, 6, 10);
            break;
        case RP_TARGET_BENCH:
            rp_fill(RP_COL_WOOD, target->x - 16, target->y - 7, 32, 8);
            rp_fill(RP_COL_STONE, target->x - 12, target->y + 1, 24, 7);
            break;
        case RP_TARGET_ENEMY:
            rp_draw_npc(target->x, target->y, RP_COL_ENEMY);
            break;
        case RP_TARGET_EXIT:
            rp_rect(RP_COL_ACCENT, target->x - 12, target->y - 18, 24, 26);
            break;
        case RP_TARGET_NPC:
        default:
            rp_draw_npc(target->x, target->y,
                        i == 0 ? LCD_RGBPACK(86, 116, 74)
                               : LCD_RGBPACK(126, 88, 48));
            break;
    }
}

static void rp_draw_player(void)
{
    if (rp_sprites_loaded)
    {
        int bob = game.moving && rp_anim_frame(6) ? -1 : 0;
        rp_draw_sprite_anchor(RP_SPR_PLAYER, game.player_x, game.player_y + bob);
        return;
    }

    rp_fill(RP_COL_PLAYER, game.player_x - 6, game.player_y - 15, 12, 17);
    rp_fill(LCD_RGBPACK(218, 174, 126), game.player_x - 4,
            game.player_y - 22, 8, 8);
    rp_fill(LCD_RGBPACK(36, 45, 75), game.player_x - 7,
            game.player_y + 2, 14, 4);
}

static void rp_draw_world(void)
{
    int x;
    int y;
    char buf[64];

    rp_fill(RP_COL_GRASS, 0, RP_WORLD_TOP, LCD_WIDTH,
            RP_WORLD_BOTTOM - RP_WORLD_TOP);

    for (y = RP_WORLD_TOP; y < RP_WORLD_BOTTOM; y += 16)
    {
        for (x = 0; x < LCD_WIDTH; x += 16)
        {
            if (((x + y) / 16) & 1)
                rp_fill(LCD_RGBPACK(66, 118, 72), x, y, 16, 16);
        }
    }

    rp_fill(RP_COL_PATH, 132, RP_WORLD_TOP, 56,
            RP_WORLD_BOTTOM - RP_WORLD_TOP);
    rp_fill(RP_COL_PATH, 24, 102, 272, 34);
    rp_fill(LCD_RGBPACK(117, 93, 63), 138, 24, 44, 28);
    rp_fill(LCD_RGBPACK(93, 66, 47), 246, 28, 34, 26);

    for (x = 0; x < RP_MAX_TARGETS; x++)
        rp_draw_target(x);

    rp_draw_player();

    rp_fill(RP_COL_PANEL, 0, 0, LCD_WIDTH, RP_TOP_H);
    rb->lcd_set_foreground(RP_COL_TEXT);
    rb->snprintf(buf, sizeof(buf), "HP %d  Coins %d  Q%d  %s",
                 game.hp, game.inv.coins, game.quest_stage,
                 rp_group_name(rp_targets[game.selected].group));
    rb->lcd_putsxy(4, 5, buf);

    rp_fill(RP_COL_PANEL, 0, RP_WORLD_BOTTOM, LCD_WIDTH, RP_BOTTOM_H);
    rb->lcd_set_foreground(RP_COL_ACCENT);
    rb->snprintf(buf, sizeof(buf), "%s: %s",
                 rp_targets[game.selected].name,
                 rp_targets[game.selected].default_action);
    rb->lcd_putsxy(5, RP_WORLD_BOTTOM + 3, buf);
    rb->lcd_set_foreground(RP_COL_TEXT);
    rp_draw_text_clip(5, RP_WORLD_BOTTOM + 17,
                      game.message[0] ? game.message : game.detail, 41);
    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(215, RP_WORLD_BOTTOM + 17, "Hold Select: menu");
}

static void rp_draw_title(void)
{
    rp_fill(LCD_RGBPACK(28, 43, 42), 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rp_fill(RP_COL_GRASS_DARK, 0, 138, LCD_WIDTH, 102);
    rp_fill(RP_COL_PATH, 118, 138, 82, 102);
    if (rp_sprites_loaded)
    {
        rp_draw_sprite_anchor(RP_SPR_OAK, 66, 164);
        rp_draw_sprite_anchor(RP_SPR_OAK, 256, 156);
        rp_draw_sprite_anchor(RP_SPR_COPPER, 92, 200);
        rp_draw_sprite_anchor(RP_SPR_PLAYER, 160, 170);
    }
    else
    {
        rp_draw_tree(66, 164);
        rp_draw_tree(256, 156);
        rp_draw_rock(92, 200);
        rp_draw_npc(160, 170, RP_COL_PLAYER);
    }

    rb->lcd_set_foreground(RP_COL_ACCENT);
    rb->lcd_putsxy(104, 55, "RunePod");
    rb->lcd_set_foreground(RP_COL_TEXT);
    rb->lcd_putsxy(54, 82, "Click-wheel fantasy RPG");
    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(50, 112, "Select starts   Menu exits");
}

static void rp_draw_menu_panel(const char *title)
{
    rp_fill(RP_COL_PANEL, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    rp_fill(RP_COL_PANEL_2, 0, 0, LCD_WIDTH, 22);
    rb->lcd_set_foreground(RP_COL_ACCENT);
    rb->lcd_putsxy(5, 7, title);
}

static void rp_draw_actions(void)
{
    int i;

    rp_draw_menu_panel(rp_targets[game.selected].name);
    for (i = 0; i < game.action_count; i++)
    {
        int y = 36 + i * 22;
        if (i == game.action_selected)
        {
            rp_fill(RP_COL_ACCENT, 14, y - 3, LCD_WIDTH - 28, 18);
            rb->lcd_set_foreground(LCD_BLACK);
        }
        else
        {
            rb->lcd_set_foreground(RP_COL_TEXT);
        }
        rb->lcd_putsxy(24, y, game.actions[i]);
    }
    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(14, LCD_HEIGHT - 17, "Wheel scrolls  Select chooses  Menu backs");
}

static void rp_draw_inventory(void)
{
    char buf[64];
    int y = 36;

    rp_draw_menu_panel("Inventory");
    rb->lcd_set_foreground(RP_COL_TEXT);
    if (rp_sprites_loaded)
    {
        rp_draw_sprite(RP_SPR_LOGS, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Logs: %d", game.inv.logs);
        rb->lcd_putsxy(54, y, buf); y += 24;
        rp_draw_sprite(RP_SPR_ORE, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Ore: %d", game.inv.ore);
        rb->lcd_putsxy(54, y, buf); y += 24;
        rp_draw_sprite(RP_SPR_FISH, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Raw fish: %d", game.inv.raw_fish);
        rb->lcd_putsxy(54, y, buf); y += 24;
        rp_draw_sprite(RP_SPR_FOOD, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Food: %d", game.inv.food);
        rb->lcd_putsxy(54, y, buf); y += 24;
        rp_draw_sprite(RP_SPR_COINS, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Coins: %d", game.inv.coins);
        rb->lcd_putsxy(54, y, buf); y += 24;
        rp_draw_sprite(RP_SPR_COMBAT_ICON, 16, y - 10);
        rb->snprintf(buf, sizeof(buf), "Charms: %d", game.inv.charms);
        rb->lcd_putsxy(54, y, buf);
    }
    else
    {
        rb->snprintf(buf, sizeof(buf), "Logs: %d", game.inv.logs);
        rb->lcd_putsxy(20, y, buf); y += 18;
        rb->snprintf(buf, sizeof(buf), "Ore: %d", game.inv.ore);
        rb->lcd_putsxy(20, y, buf); y += 18;
        rb->snprintf(buf, sizeof(buf), "Raw fish: %d", game.inv.raw_fish);
        rb->lcd_putsxy(20, y, buf); y += 18;
        rb->snprintf(buf, sizeof(buf), "Food: %d", game.inv.food);
        rb->lcd_putsxy(20, y, buf); y += 18;
        rb->snprintf(buf, sizeof(buf), "Coins: %d", game.inv.coins);
        rb->lcd_putsxy(20, y, buf); y += 18;
        rb->snprintf(buf, sizeof(buf), "Charms: %d", game.inv.charms);
        rb->lcd_putsxy(20, y, buf);
    }

    rb->lcd_set_foreground(RP_COL_ACCENT);
    rb->snprintf(buf, sizeof(buf), "XP C%d M%d W%d F%d Cook%d Cr%d",
                 game.xp.combat, game.xp.mining, game.xp.woodcutting,
                 game.xp.fishing, game.xp.cooking, game.xp.crafting);
    rp_draw_text_clip(20, 164, buf, 38);
    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(20, LCD_HEIGHT - 17, "Select levels  Menu/Play returns");
}

static void rp_draw_level_row(int y, enum rp_sprite icon, const char *name,
                              int xp)
{
    char buf[48];
    int progress = rp_skill_progress(xp);
    int filled = progress * 9;

    if (rp_sprites_loaded)
        rp_draw_sprite(icon, 14, y - 10);

    rb->lcd_set_foreground(RP_COL_TEXT);
    rb->snprintf(buf, sizeof(buf), "%s  L%d", name, rp_skill_level(xp));
    rb->lcd_putsxy(rp_sprites_loaded ? 52 : 20, y, buf);

    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->snprintf(buf, sizeof(buf), "%d/10", progress);
    rb->lcd_putsxy(142, y, buf);

    rp_rect(RP_COL_MUTED, 190, y + 1, 96, 7);
    rp_fill(RP_COL_ACCENT, 191, y + 2, filled, 5);
}

static void rp_draw_levels(void)
{
    int y = 34;

    rp_draw_menu_panel("Levels");
    rp_draw_level_row(y, RP_SPR_COMBAT_ICON, "Combat", game.xp.combat);
    y += 26;
    rp_draw_level_row(y, RP_SPR_ORE, "Mining", game.xp.mining);
    y += 26;
    rp_draw_level_row(y, RP_SPR_LOGS, "Woodcut", game.xp.woodcutting);
    y += 26;
    rp_draw_level_row(y, RP_SPR_FISH, "Fishing", game.xp.fishing);
    y += 26;
    rp_draw_level_row(y, RP_SPR_FOOD, "Cooking", game.xp.cooking);
    y += 26;
    rp_draw_level_row(y, RP_SPR_WORKBENCH, "Crafting", game.xp.crafting);

    rb->lcd_set_foreground(RP_COL_MUTED);
    rb->lcd_putsxy(20, LCD_HEIGHT - 17, "Select bag  Menu/Play returns");
}

static void rp_draw_dialogue(void)
{
    rp_draw_world();
    rp_fill(RP_COL_PANEL, 22, 62, LCD_WIDTH - 44, 80);
    rp_rect(RP_COL_ACCENT, 22, 62, LCD_WIDTH - 44, 80);
    rb->lcd_set_foreground(RP_COL_TEXT);
    rp_draw_text_clip(34, 82, game.message, 34);
    rb->lcd_set_foreground(RP_COL_MUTED);
    rp_draw_text_clip(34, 104, game.detail, 34);
    rb->lcd_putsxy(34, 126, "Select/Menu closes");
}

static void rp_render(void)
{
    switch (game.view)
    {
        case RP_VIEW_TITLE:
            rp_draw_title();
            break;
        case RP_VIEW_ACTIONS:
            rp_draw_actions();
            break;
        case RP_VIEW_INVENTORY:
            rp_draw_inventory();
            break;
        case RP_VIEW_LEVELS:
            rp_draw_levels();
            break;
        case RP_VIEW_DIALOGUE:
            rp_draw_dialogue();
            break;
        case RP_VIEW_WORLD:
        default:
            rp_draw_world();
            break;
    }
    rb->lcd_update();
}

static void rp_handle_world_event(long event)
{
    if (event == BUTTON_NONE)
        return;

    if (event & BUTTON_SCROLL_FWD)
    {
        if (event & BUTTON_REPEAT)
            rp_cycle_group(1);
        else
            rp_cycle_target(1);
    }
    else if (event & BUTTON_SCROLL_BACK)
    {
        if (event & BUTTON_REPEAT)
            rp_cycle_group(-1);
        else
            rp_cycle_target(-1);
    }
    else if ((event & BUTTON_RIGHT) && !(event & BUTTON_REL))
    {
        rp_cycle_group(1);
    }
    else if ((event & BUTTON_LEFT) && !(event & BUTTON_REL))
    {
        rp_cycle_group(-1);
    }
    else if ((event & BUTTON_SELECT) && (event & BUTTON_REPEAT))
    {
        rp_open_action_menu();
    }
    else if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
    {
        rp_execute_selected_action();
    }
    else if ((event & BUTTON_PLAY) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
    {
        game.view = RP_VIEW_INVENTORY;
    }
    else if ((event & BUTTON_MENU) && !(event & BUTTON_REPEAT))
    {
        game.quit = true;
    }
}

static void rp_handle_event(long event)
{
    if (event == SYS_USB_CONNECTED ||
        rb->default_event_handler(event) == SYS_USB_CONNECTED)
    {
        game.quit = true;
        return;
    }

    switch (game.view)
    {
        case RP_VIEW_TITLE:
            if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
            {
                game.view = RP_VIEW_WORLD;
                rp_set_message("Welcome to the village",
                               "Wheel chooses targets");
            }
            else if ((event & BUTTON_MENU) && !(event & BUTTON_REPEAT))
            {
                game.quit = true;
            }
            break;

        case RP_VIEW_WORLD:
            rp_handle_world_event(event);
            break;

        case RP_VIEW_ACTIONS:
            if ((event & BUTTON_SCROLL_FWD) && !(event & BUTTON_REL))
            {
                game.action_selected++;
                if (game.action_selected >= game.action_count)
                    game.action_selected = 0;
            }
            else if ((event & BUTTON_SCROLL_BACK) && !(event & BUTTON_REL))
            {
                game.action_selected--;
                if (game.action_selected < 0)
                    game.action_selected = game.action_count - 1;
            }
            else if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
            {
                rp_execute_menu_action();
            }
            else if ((event & BUTTON_MENU) && !(event & BUTTON_REPEAT))
            {
                game.view = RP_VIEW_WORLD;
            }
            break;

        case RP_VIEW_INVENTORY:
            if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_LEVELS;
            else if ((event & BUTTON_RIGHT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_LEVELS;
            else if ((event & (BUTTON_MENU | BUTTON_PLAY)) &&
                     !(event & BUTTON_REPEAT))
                game.view = RP_VIEW_WORLD;
            break;

        case RP_VIEW_LEVELS:
            if ((event & BUTTON_SELECT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_INVENTORY;
            else if ((event & BUTTON_LEFT) && !(event & (BUTTON_REL | BUTTON_REPEAT)))
                game.view = RP_VIEW_INVENTORY;
            else if ((event & (BUTTON_MENU | BUTTON_PLAY)) &&
                     !(event & BUTTON_REPEAT))
                game.view = RP_VIEW_WORLD;
            break;

        case RP_VIEW_DIALOGUE:
            if ((event & (BUTTON_MENU | BUTTON_SELECT)) &&
                !(event & BUTTON_REPEAT))
                game.view = RP_VIEW_WORLD;
            break;
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    int rendered_frames = 0;

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->srand((unsigned int)*rb->current_tick);
    rp_set_wheel_events(true);
    rp_load_sprites();
    rp_init_game();
    rp_smoke_log("start", 0);

    while (!game.quit)
    {
        long event = rb->button_get_w_tmo(RP_FRAME_TICKS);
        rp_handle_event(event);
        rp_update_movement();
        rp_render();
        rendered_frames++;
        if (rendered_frames == 3)
            rp_smoke_log("rendered_frames", rendered_frames);
    }

    rp_smoke_log("exit", rendered_frames);
    rp_set_wheel_events(false);
    return PLUGIN_OK;
}

#else

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    return PLUGIN_OK;
}

#endif
