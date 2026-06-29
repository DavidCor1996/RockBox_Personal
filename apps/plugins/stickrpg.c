/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/                \/
 *
 * Stick RPG native iPod play layer.
 *
 ****************************************************************************/

#include "plugin.h"

#define FLASH_DIR       ROCKBOX_DIR "/flash"
#define STICK_DIR       FLASH_DIR "/stickrpg"
#define STICK_SAVE_DIR  STICK_DIR "/save"
#define STICK_SAVE_PATH STICK_SAVE_DIR "/native.sav"
#define STICK_SWF_NAME  "stickrpg.swf"
#define STICK_SWF_PATH  STICK_DIR "/" STICK_SWF_NAME
#define STICK_SIM_SWF   "/home/david/Downloads/stickrpg.swf"

#define MAP_TOP 20
#define MAP_BOTTOM (LCD_HEIGHT - 20)
#define PLAYER_W 7
#define PLAYER_H 17
#define MAX_ACTIONS 4

#if defined(HAVE_LCD_COLOR)
#define COL_BG       LCD_RGBPACK(16, 18, 18)
#define COL_PANEL    LCD_RGBPACK(30, 34, 35)
#define COL_PANEL2   LCD_RGBPACK(47, 53, 55)
#define COL_ROAD     LCD_RGBPACK(82, 86, 84)
#define COL_ROADLINE LCD_RGBPACK(139, 142, 132)
#define COL_GRASS    LCD_RGBPACK(63, 116, 66)
#define COL_PATH     LCD_RGBPACK(145, 135, 100)
#define COL_HOME     LCD_RGBPACK(175, 128, 91)
#define COL_SCHOOL   LCD_RGBPACK(176, 161, 111)
#define COL_BANK     LCD_RGBPACK(112, 148, 177)
#define COL_GYM      LCD_RGBPACK(165, 112, 105)
#define COL_BAR      LCD_RGBPACK(110, 88, 134)
#define COL_STORE    LCD_RGBPACK(182, 151, 79)
#define COL_INK      LCD_RGBPACK(235, 238, 232)
#define COL_DIM      LCD_RGBPACK(167, 174, 166)
#define COL_WARN     LCD_RGBPACK(230, 92, 72)
#define COL_ACCENT   LCD_RGBPACK(243, 194, 61)
#define COL_PLAYER   LCD_RGBPACK(19, 20, 20)
#define SET_FG(c)    rb->lcd_set_foreground(c)
#define SET_BG(c)    rb->lcd_set_background(c)
#else
#define COL_BG       LCD_WHITE
#define COL_PANEL    LCD_BLACK
#define COL_PANEL2   LCD_DARKGRAY
#define COL_ROAD     LCD_LIGHTGRAY
#define COL_ROADLINE LCD_DARKGRAY
#define COL_GRASS    LCD_WHITE
#define COL_PATH     LCD_LIGHTGRAY
#define COL_HOME     LCD_LIGHTGRAY
#define COL_SCHOOL   LCD_LIGHTGRAY
#define COL_BANK     LCD_LIGHTGRAY
#define COL_GYM      LCD_LIGHTGRAY
#define COL_BAR      LCD_LIGHTGRAY
#define COL_STORE    LCD_LIGHTGRAY
#define COL_INK      LCD_BLACK
#define COL_DIM      LCD_DARKGRAY
#define COL_WARN     LCD_BLACK
#define COL_ACCENT   LCD_BLACK
#define COL_PLAYER   LCD_BLACK
#define SET_FG(c)    rb->lcd_set_foreground(c)
#define SET_BG(c)    rb->lcd_set_background(c)
#endif

enum ui_mode {
    UI_PLAY = 0,
    UI_ACTIONS,
    UI_STATUS,
    UI_GAMEOVER,
};

enum place_id {
    PLACE_NONE = -1,
    PLACE_HOME = 0,
    PLACE_SCHOOL,
    PLACE_BANK,
    PLACE_GYM,
    PLACE_BAR,
    PLACE_STORE,
};

struct swf_info {
    char sig[4];
    int version;
    unsigned long declared_size;
    unsigned long actual_size;
    int compressed;
    int stage_w;
    int stage_h;
    int fps;
    int frames;
};

struct building {
    int x, y, w, h;
    int door_x, door_y;
    int color;
    const char *name;
};

struct game_state {
    enum ui_mode mode;
    int x, y;
    int day, hour;
    int cash, bank;
    int iq, str, charm;
    int fatigue;
    int focus;
    int rep;
    int action_sel;
    int dirty;
    int message_ticks;
    char message[64];
    int has_swf;
    struct swf_info swf;
};

static const struct building buildings[] = {
    { 14,  34, 64, 48,  47,  84, COL_HOME,   "Home" },
    { 94,  34, 74, 48, 131,  84, COL_SCHOOL, "School" },
    { 196, 34, 76, 48, 234,  84, COL_BANK,   "Bank" },
    { 24, 150, 68, 46,  58, 148, COL_GYM,    "Gym" },
    { 118,150, 70, 46, 153, 148, COL_BAR,    "Bar" },
    { 216,150, 70, 46, 251, 148, COL_STORE,  "Store" },
};

static struct game_state g;

static void set_msg(const char *msg)
{
    rb->strlcpy(g.message, msg, sizeof(g.message));
    g.message_ticks = 42;
    g.dirty = 1;
}

static void ensure_dirs(void)
{
    rb->mkdir(FLASH_DIR);
    rb->mkdir(STICK_DIR);
    rb->mkdir(STICK_SAVE_DIR);
}

static unsigned long read_le32(const unsigned char *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static int read_rect_bits(const unsigned char *buf, int bit, int count)
{
    int value = 0;
    int i;

    for (i = 0; i < count; i++, bit++) {
        int byte = bit >> 3;
        int shift = 7 - (bit & 7);
        value = (value << 1) | ((buf[byte] >> shift) & 1);
    }

    return value;
}

static int inspect_swf(const char *path, struct swf_info *info)
{
    unsigned char buf[32];
    int fd;
    int got;

    rb->memset(info, 0, sizeof(*info));
    fd = rb->open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    got = rb->read(fd, buf, sizeof(buf));
    info->actual_size = rb->filesize(fd);
    rb->close(fd);

    if (got < 8)
        return -1;

    info->sig[0] = buf[0];
    info->sig[1] = buf[1];
    info->sig[2] = buf[2];
    info->sig[3] = '\0';
    info->version = buf[3];
    info->declared_size = read_le32(buf + 4);

    if ((buf[0] != 'F' && buf[0] != 'C') || buf[1] != 'W' || buf[2] != 'S')
        return -1;

    info->compressed = buf[0] == 'C';

    if (!info->compressed && got >= 21) {
        int nbits = read_rect_bits(buf + 8, 0, 5);
        int bit = 5;
        int xmin = read_rect_bits(buf + 8, bit, nbits);
        int xmax;
        int ymin;
        int ymax;
        int rect_bytes;

        bit += nbits;
        xmax = read_rect_bits(buf + 8, bit, nbits);
        bit += nbits;
        ymin = read_rect_bits(buf + 8, bit, nbits);
        bit += nbits;
        ymax = read_rect_bits(buf + 8, bit, nbits);
        bit += nbits;
        rect_bytes = (bit + 7) >> 3;

        info->stage_w = (xmax - xmin) / 20;
        info->stage_h = (ymax - ymin) / 20;
        if (8 + rect_bytes + 3 < got) {
            unsigned char *p = buf + 8 + rect_bytes;
            info->fps = p[1];
            info->frames = p[2] | (p[3] << 8);
        }
    }

    return 0;
}

static void init_new_game(void)
{
    rb->memset(&g, 0, sizeof(g));
    g.mode = UI_PLAY;
    g.x = LCD_WIDTH / 2;
    g.y = 116;
    g.day = 1;
    g.hour = 8;
    g.cash = 40;
    g.iq = 10;
    g.str = 10;
    g.charm = 10;
    g.focus = 10;
    g.rep = 1;
    g.dirty = 1;
    set_msg("Fresh start. Make rent by day 7.");
}

static void save_game(void)
{
    int fd = rb->creat(STICK_SAVE_PATH, 0666);
    if (fd < 0) {
        set_msg("Save failed.");
        return;
    }

    rb->write(fd, &g.day, sizeof(g.day));
    rb->write(fd, &g.hour, sizeof(g.hour));
    rb->write(fd, &g.cash, sizeof(g.cash));
    rb->write(fd, &g.bank, sizeof(g.bank));
    rb->write(fd, &g.iq, sizeof(g.iq));
    rb->write(fd, &g.str, sizeof(g.str));
    rb->write(fd, &g.charm, sizeof(g.charm));
    rb->write(fd, &g.fatigue, sizeof(g.fatigue));
    rb->write(fd, &g.focus, sizeof(g.focus));
    rb->write(fd, &g.rep, sizeof(g.rep));
    rb->write(fd, &g.x, sizeof(g.x));
    rb->write(fd, &g.y, sizeof(g.y));
    rb->close(fd);
    set_msg("Saved.");
}

static void load_game(void)
{
    int fd = rb->open(STICK_SAVE_PATH, O_RDONLY);
    int ok = 1;

    if (fd < 0) {
        set_msg("No save found.");
        return;
    }

#define READ_FIELD(f) do { if (rb->read(fd, &g.f, sizeof(g.f)) != (int)sizeof(g.f)) ok = 0; } while (0)
    READ_FIELD(day);
    READ_FIELD(hour);
    READ_FIELD(cash);
    READ_FIELD(bank);
    READ_FIELD(iq);
    READ_FIELD(str);
    READ_FIELD(charm);
    READ_FIELD(fatigue);
    READ_FIELD(focus);
    READ_FIELD(rep);
    READ_FIELD(x);
    READ_FIELD(y);
#undef READ_FIELD

    rb->close(fd);
    if (!ok) {
        init_new_game();
        set_msg("Save corrupt. New game.");
    } else {
        g.mode = UI_PLAY;
        g.action_sel = 0;
        set_msg("Loaded.");
    }
    g.dirty = 1;
}

static void advance_time(int hours, int fatigue)
{
    g.hour += hours;
    g.fatigue += fatigue;
    while (g.hour >= 24) {
        g.hour -= 24;
        g.day++;
        if (g.fatigue > 25)
            g.fatigue -= 25;
    }
    if (g.fatigue > 100)
        g.fatigue = 100;
    if (g.fatigue < 0)
        g.fatigue = 0;
}

static int near_building(int idx)
{
    const struct building *b = &buildings[idx];
    int dx = g.x - b->door_x;
    int dy = g.y - b->door_y;
    return dx > -18 && dx < 18 && dy > -18 && dy < 18;
}

static int current_place(void)
{
    int i;

    for (i = 0; i < (int)ARRAYLEN(buildings); i++) {
        if (near_building(i))
            return i;
    }
    return PLACE_NONE;
}

static int stat_total(void)
{
    return g.iq + g.str + g.charm + g.focus + g.rep;
}

static int net_worth(void)
{
    return g.cash + g.bank;
}

static void sleep_home(void)
{
    if (g.hour < 22)
        g.hour = 22;
    advance_time(10, -100);
    g.focus += 2;
    if (g.focus > 99)
        g.focus = 99;
    save_game();
    set_msg("Slept, recovered, autosaved.");
}

static void do_job(void)
{
    int pay;

    if (g.fatigue > 85) {
        set_msg("Too tired to work.");
        return;
    }

    pay = 14 + stat_total() / 7;
    g.cash += pay;
    advance_time(4, 23);
    set_msg("Shift complete. Cash up.");
}

static void execute_action(int place, int action)
{
    if (place == PLACE_HOME) {
        if (action == 0)
            sleep_home();
        else if (action == 1)
            save_game();
        else
            load_game();
    } else if (place == PLACE_SCHOOL) {
        if (action == 0) {
            if (g.cash < 20) { set_msg("Need $20 for class."); return; }
            g.cash -= 20; g.iq += 4; g.focus += 1; advance_time(3, 14);
            set_msg("Studied. IQ increased.");
        } else if (action == 1) {
            do_job();
        } else {
            if (g.cash < 12) { set_msg("Need $12 for tutoring."); return; }
            g.cash -= 12; g.iq += 2; advance_time(1, 6);
            set_msg("Quick lesson complete.");
        }
    } else if (place == PLACE_BANK) {
        if (action == 0) {
            int dep = g.cash > 50 ? 50 : g.cash;
            if (dep <= 0) { set_msg("No cash to deposit."); return; }
            g.cash -= dep; g.bank += dep; set_msg("Deposited cash.");
        } else if (action == 1) {
            int wd = g.bank > 50 ? 50 : g.bank;
            if (wd <= 0) { set_msg("No bank funds."); return; }
            g.bank -= wd; g.cash += wd; set_msg("Withdrew cash.");
        } else {
            int interest = g.bank / 25;
            g.bank += interest; advance_time(1, 3); set_msg("Interest collected.");
        }
    } else if (place == PLACE_GYM) {
        if (action == 0) {
            if (g.cash < 15) { set_msg("Need $15 for gym."); return; }
            if (g.fatigue > 90) { set_msg("Too tired to train."); return; }
            g.cash -= 15; g.str += 4; advance_time(2, 20);
            set_msg("Workout done. Strength up.");
        } else if (action == 1) {
            do_job();
        } else {
            if (g.cash < 25) { set_msg("Need $25 for protein."); return; }
            g.cash -= 25; g.str += 2; g.fatigue -= 12; advance_time(1, 0);
            set_msg("Protein helped recovery.");
        }
    } else if (place == PLACE_BAR) {
        if (action == 0) {
            if (g.cash < 12) { set_msg("Need $12 to socialize."); return; }
            g.cash -= 12; g.charm += 3; g.rep += 1; advance_time(2, 12);
            set_msg("Socialized. Charm up.");
        } else if (action == 1) {
            int win = (g.charm + g.focus + g.rep + g.day) % 3;
            if (g.cash < 20) { set_msg("Need $20 stake."); return; }
            g.cash += win ? 25 : -20; advance_time(1, 10);
            set_msg(win ? "Won the bet." : "Lost the bet.");
        } else {
            if (g.cash < 18) { set_msg("Need $18 for food."); return; }
            g.cash -= 18; g.fatigue -= 25; advance_time(1, -5);
            set_msg("Meal restored energy.");
        }
    } else if (place == PLACE_STORE) {
        if (action == 0) {
            if (g.cash < 10) { set_msg("Need $10 for coffee."); return; }
            g.cash -= 10; g.focus += 3; g.fatigue -= 10; advance_time(1, -4);
            set_msg("Coffee boosted focus.");
        } else if (action == 1) {
            if (g.cash < 35) { set_msg("Need $35 for outfit."); return; }
            g.cash -= 35; g.charm += 3; g.rep += 1; set_msg("New outfit equipped.");
        } else {
            do_job();
        }
    }

    if (g.day > 7 && net_worth() < 500) {
        g.mode = UI_GAMEOVER;
        set_msg("Rent missed. Run over.");
    } else if (net_worth() >= 1000 && stat_total() >= 125) {
        g.mode = UI_GAMEOVER;
        set_msg("Penthouse ending unlocked.");
    }
    g.dirty = 1;
}

static const char *action_name(int place, int action)
{
    static const char *home[] = { "Sleep", "Save", "Load" };
    static const char *school[] = { "Class $20", "Work", "Tutor $12" };
    static const char *bank[] = { "Deposit $50", "Withdraw $50", "Interest" };
    static const char *gym[] = { "Train $15", "Work", "Protein $25" };
    static const char *bar[] = { "Social $12", "Bet $20", "Meal $18" };
    static const char *store[] = { "Coffee $10", "Outfit $35", "Work" };
    static const char **lists[] = { home, school, bank, gym, bar, store };

    if (place < 0 || place >= (int)ARRAYLEN(lists) || action < 0 || action >= 3)
        return "";
    return lists[place][action];
}

static void move_player(int dx, int dy)
{
    int old_x = g.x;
    int old_y = g.y;

    g.x += dx;
    g.y += dy;

    if (g.x < 4)
        g.x = 4;
    if (g.x > LCD_WIDTH - 5)
        g.x = LCD_WIDTH - 5;
    if (g.y < MAP_TOP + 8)
        g.y = MAP_TOP + 8;
    if (g.y > MAP_BOTTOM - 2)
        g.y = MAP_BOTTOM - 2;

    if (g.x != old_x || g.y != old_y) {
        g.dirty = 1;
        if (g.message_ticks <= 0)
            g.message[0] = '\0';
    }
}

static int step_for_button(unsigned int button)
{
    return (button & BUTTON_REPEAT) ? 7 : 4;
}

static void draw_bar(int x, int y, int w, int value, int color)
{
    int fill = (w - 2) * value / 100;
    SET_FG(COL_PANEL2);
    rb->lcd_drawrect(x, y, w, 6);
    SET_FG(color);
    rb->lcd_fillrect(x + 1, y + 1, fill, 4);
}

static void draw_building(int idx)
{
    const struct building *b = &buildings[idx];

    SET_FG(b->color);
    rb->lcd_fillrect(b->x, b->y, b->w, b->h);
    SET_FG(COL_PANEL);
    rb->lcd_drawrect(b->x, b->y, b->w, b->h);
    rb->lcd_hline(b->x + 5, b->x + b->w - 6, b->y + 13);
    SET_FG(COL_PANEL2);
    rb->lcd_fillrect(b->door_x - 5, b->door_y - 14, 10, 14);
    SET_FG(COL_INK);
    rb->lcd_putsxy(b->x + 5, b->y + 4, b->name);
}

static void draw_map(void)
{
    int i;

    SET_FG(COL_GRASS);
    rb->lcd_fillrect(0, MAP_TOP, LCD_WIDTH, MAP_BOTTOM - MAP_TOP);

    SET_FG(COL_ROAD);
    rb->lcd_fillrect(0, 96, LCD_WIDTH, 39);
    rb->lcd_fillrect(144, MAP_TOP, 36, MAP_BOTTOM - MAP_TOP);
    SET_FG(COL_ROADLINE);
    rb->lcd_hline(0, LCD_WIDTH - 1, 116);
    rb->lcd_vline(162, MAP_TOP, MAP_BOTTOM - 1);

    SET_FG(COL_PATH);
    for (i = 0; i < (int)ARRAYLEN(buildings); i++) {
        rb->lcd_drawline(buildings[i].door_x, buildings[i].door_y, 162, 116);
        draw_building(i);
    }
}

static void draw_player(void)
{
    int x = g.x;
    int y = g.y;

    SET_FG(COL_PLAYER);
    rb->lcd_fillrect(x - 2, y - 13, 5, 8);
    rb->lcd_vline(x, y - 5, y + 3);
    rb->lcd_drawline(x, y - 2, x - 6, y + 3);
    rb->lcd_drawline(x, y - 2, x + 6, y + 3);
    rb->lcd_drawline(x, y + 3, x - 5, y + 10);
    rb->lcd_drawline(x, y + 3, x + 5, y + 10);
}

static void draw_hud(void)
{
    SET_FG(COL_PANEL);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, MAP_TOP);
    rb->lcd_fillrect(0, MAP_BOTTOM, LCD_WIDTH, LCD_HEIGHT - MAP_BOTTOM);

    SET_FG(COL_INK);
    rb->lcd_putsxyf(4, 3, "D%d %02d:00 $%d B%d", g.day, g.hour, g.cash, g.bank);
    SET_FG(COL_DIM);
    rb->lcd_putsxyf(196, 3, "IQ%d S%d C%d", g.iq, g.str, g.charm);
    draw_bar(4, LCD_HEIGHT - 14, 76, g.fatigue, COL_WARN);
    SET_FG(COL_DIM);
    rb->lcd_putsxy(84, LCD_HEIGHT - 16, "Fatigue");

    if (g.message[0]) {
        SET_FG(COL_ACCENT);
        rb->lcd_putsxy(148, LCD_HEIGHT - 16, g.message);
    } else {
        int place = current_place();
        SET_FG(COL_DIM);
        if (place >= 0)
            rb->lcd_putsxyf(148, LCD_HEIGHT - 16, "Select: %s", buildings[place].name);
        else
            rb->lcd_putsxy(148, LCD_HEIGHT - 16, "Select near a door");
    }
}

static void draw_actions(void)
{
    int place = current_place();
    int x = 46;
    int y = 58;
    int i;

    SET_FG(COL_PANEL);
    rb->lcd_fillrect(x, y, LCD_WIDTH - x * 2, 106);
    SET_FG(COL_ACCENT);
    rb->lcd_drawrect(x, y, LCD_WIDTH - x * 2, 106);
    SET_FG(COL_INK);
    rb->lcd_putsxyf(x + 8, y + 8, "%s", place >= 0 ? buildings[place].name : "Street");

    if (place < 0) {
        SET_FG(COL_DIM);
        rb->lcd_putsxy(x + 8, y + 30, "Stand by a doorway.");
        return;
    }

    for (i = 0; i < 3; i++) {
        SET_FG(i == g.action_sel ? COL_ACCENT : COL_PANEL2);
        rb->lcd_fillrect(x + 8, y + 28 + i * 22, LCD_WIDTH - x * 2 - 16, 17);
        SET_FG(i == g.action_sel ? COL_PANEL : COL_INK);
        rb->lcd_putsxy(x + 14, y + 31 + i * 22, action_name(place, i));
    }
}

static void draw_status(void)
{
    int x = 36;
    int y = 48;

    SET_FG(COL_PANEL);
    rb->lcd_fillrect(x, y, LCD_WIDTH - x * 2, 128);
    SET_FG(COL_ACCENT);
    rb->lcd_drawrect(x, y, LCD_WIDTH - x * 2, 128);
    SET_FG(COL_INK);
    rb->lcd_putsxy(x + 8, y + 8, "Status");
    SET_FG(COL_DIM);
    rb->lcd_putsxyf(x + 8, y + 28, "Cash %d  Bank %d", g.cash, g.bank);
    rb->lcd_putsxyf(x + 8, y + 44, "IQ %d  Strength %d", g.iq, g.str);
    rb->lcd_putsxyf(x + 8, y + 60, "Charm %d  Focus %d", g.charm, g.focus);
    rb->lcd_putsxyf(x + 8, y + 76, "Rep %d  Total %d", g.rep, stat_total());
    rb->lcd_putsxyf(x + 8, y + 92, "Goal: $1000 + stats 125");
    rb->lcd_putsxy(x + 8, y + 108, "Play closes. Hold Menu quits.");
}

static void draw_gameover(void)
{
    int won = net_worth() >= 1000 && stat_total() >= 125;

    SET_FG(COL_PANEL);
    rb->lcd_fillrect(24, 58, LCD_WIDTH - 48, 94);
    SET_FG(won ? COL_ACCENT : COL_WARN);
    rb->lcd_drawrect(24, 58, LCD_WIDTH - 48, 94);
    SET_FG(COL_INK);
    rb->lcd_putsxy(38, 74, won ? "Penthouse ending" : "Game over");
    SET_FG(COL_DIM);
    rb->lcd_putsxyf(38, 98, "Net $%d  Stats %d", net_worth(), stat_total());
    rb->lcd_putsxy(38, 120, "Select new run  Menu exits");
}

static void redraw(bool force)
{
    if (!force && !g.dirty && g.message_ticks <= 0)
        return;

    SET_BG(COL_BG);
    SET_FG(COL_BG);
    rb->lcd_clear_display();
    draw_map();
    draw_player();
    draw_hud();

    if (g.mode == UI_ACTIONS)
        draw_actions();
    else if (g.mode == UI_STATUS)
        draw_status();
    else if (g.mode == UI_GAMEOVER)
        draw_gameover();

    rb->lcd_update();

    if (g.message_ticks > 0) {
        g.message_ticks--;
        if (g.message_ticks == 0) {
            g.message[0] = '\0';
            g.dirty = 1;
        }
    } else {
        g.dirty = 0;
    }
}

static void reload_swf(const char *path)
{
    g.has_swf = inspect_swf(path, &g.swf) == 0;
    g.dirty = 1;
}

static bool handle_button(unsigned int button)
{
    unsigned int bare = button & ~(BUTTON_REPEAT | BUTTON_REL);
    int step = step_for_button(button);
    int place;

    if (button & BUTTON_REL)
        return false;

    if (g.mode == UI_GAMEOVER) {
        if (bare == BUTTON_SELECT)
            init_new_game();
        else if (bare == BUTTON_MENU)
            return true;
        return false;
    }

    if (g.mode == UI_STATUS) {
        if (bare == BUTTON_PLAY || bare == BUTTON_MENU || bare == BUTTON_SELECT) {
            g.mode = UI_PLAY;
            g.dirty = 1;
        }
        return false;
    }

    if (g.mode == UI_ACTIONS) {
        place = current_place();
        if (bare == BUTTON_SCROLL_FWD || bare == BUTTON_RIGHT) {
            g.action_sel = (g.action_sel + 1) % 3;
            g.dirty = 1;
        } else if (bare == BUTTON_SCROLL_BACK || bare == BUTTON_LEFT) {
            g.action_sel = (g.action_sel + 2) % 3;
            g.dirty = 1;
        } else if (bare == BUTTON_SELECT) {
            if (place >= 0)
                execute_action(place, g.action_sel);
        } else if (bare == BUTTON_PLAY || bare == BUTTON_MENU) {
            g.mode = UI_PLAY;
            g.dirty = 1;
        }
        return false;
    }

    if (bare == BUTTON_SCROLL_FWD)
        move_player(0, step);
    else if (bare == BUTTON_SCROLL_BACK)
        move_player(0, -step);
    else if (bare == BUTTON_LEFT)
        move_player(-step, 0);
    else if (bare == BUTTON_RIGHT)
        move_player(step, 0);
    else if (bare == BUTTON_SELECT) {
        g.action_sel = 0;
        g.mode = UI_ACTIONS;
        g.dirty = 1;
    } else if (bare == BUTTON_PLAY) {
        g.mode = UI_STATUS;
        g.dirty = 1;
    } else if (bare == BUTTON_MENU && (button & BUTTON_REPEAT)) {
        return true;
    }

    return false;
}

enum plugin_status plugin_start(const void *parameter)
{
    const char *swf_path = parameter ? (const char *)parameter : STICK_SWF_PATH;
    bool quit = false;

    ensure_dirs();
    init_new_game();

    if (!swf_path[0])
        swf_path = STICK_SWF_PATH;

    reload_swf(swf_path);
#ifdef SIMULATOR
    if (!g.has_swf) {
        swf_path = STICK_SIM_SWF;
        reload_swf(swf_path);
    }
#endif

    rb->lcd_setfont(FONT_SYSFIXED);
    redraw(true);

    while (!quit) {
        int button = rb->button_get_w_tmo(HZ / 20);

        redraw(false);

        if (button == BUTTON_NONE)
            continue;
        if (button == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;

        quit = handle_button(button);
    }

    return PLUGIN_OK;
}
