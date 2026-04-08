#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "plugin.h"


#define LCD_WIDTH 320
#define LCD_HEIGHT 240

#define PLAYER_W 14
#define PLAYER_H 16
#define PLAYER_SPEED 2
#define PLAYER_MAX_HEALTH 3

#define INTERIOR_ROOM_W 240
#define INTERIOR_ROOM_H 160
#define INTERIOR_ORIGIN_X ((LCD_WIDTH - INTERIOR_ROOM_W) / 2)
#define INTERIOR_ORIGIN_Y 18

#define ROOM_LEFT 12
#define ROOM_TOP 18
#define ROOM_RIGHT (LCD_WIDTH - 13)
#define ROOM_BOTTOM (LCD_HEIGHT - 13)

#define BEDROOM_START_X 12
#define BEDROOM_START_Y 86

#define HOUSE_EXIT_SAFE_X TMC_TO_PLAYER_X(0x290)
#define HOUSE_EXIT_SAFE_Y TMC_TO_PLAYER_Y(0x1e0)

#define TMC_TO_PLAYER_X(tmc_x) ((tmc_x) - 7)
#define TMC_TO_PLAYER_Y(tmc_y) ((tmc_y) - 16 + 2)
#define MEADOW_WORLD_X 470
#define MEADOW_WORLD_Y 210
#define MEADOW_ROOM_W 320
#define MEADOW_ROOM_H 208

#define SOUTH_HYRULE_TILE_W 63
#define SOUTH_HYRULE_TILE_H 43

enum { FACE_DOWN = 0, FACE_UP, FACE_LEFT, FACE_RIGHT };
enum { CMD_LEFT = 1, CMD_RIGHT = 2, CMD_UP = 4, CMD_DOWN = 8 };
enum { ROOM_MEADOW = 0, ROOM_CREEK, ROOM_SHRINE, ROOM_LINKS_HOUSE_BEDROOM, ROOM_LINKS_HOUSE_ENTRANCE, ROOM_LINKS_HOUSE_SMITH, ROOM_COUNT };
enum { THEME_MEADOW = 0, THEME_CREEK, THEME_SHRINE, THEME_HOUSE };
enum { AXIS_HORIZONTAL = 0, AXIS_VERTICAL };
enum { NPCTYPE_NONE = 0, NPCTYPE_ZELDA, NPCTYPE_SMITH };

#define COLOR_BLACK 0
#define COLOR_WHITE 255
#define COLOR_HEART 0xF800
#define MINISHCAP_USE_REAL_ASSETS 0
#define MINISHCAP_SWAP_LEFT_RIGHT 0
#define COLOR_SWORD 0x39E7
#define COLOR_HUD_PARCHMENT_DARK 0x18C3
#define COLOR_HUD_PARCHMENT_SHADOW 0x1059
#define COLOR_HUD_INSET 0x18C3
#define COLOR_PANEL_ACCENT 0x31A0
#define COLOR_PANEL_TEXT 0x0000
#define COLOR_DIALOG 0x3186
#define COLOR_DIALOG_BORDER 0x0000
#define COLOR_DIALOG_TEXT 0x0000
#define COLOR_ROOM_FLOOR 0x18C3
#define COLOR_ROOM_WALL 0x2945
#define COLOR_ROOM_OBJECT 0x31A0
#define COLOR_MINIMAP_MARKER 0xFF00
#define MINISHCAP_BUILD_TAG "MC-SIM r3"

#include "pluginbitmaps/minishcap_link_back.h"
#include "pluginbitmaps/minishcap_link_front.h"
#include "pluginbitmaps/minishcap_link_front_step.h"
#include "pluginbitmaps/minishcap_link_left.h"
#include "pluginbitmaps/minishcap_link_left_step.h"
#include "pluginbitmaps/minishcap_link_right.h"
#include "pluginbitmaps/minishcap_link_right_step.h"
#include "pluginbitmaps/minishcap_room_links_house_bedroom.h"
#include "pluginbitmaps/minishcap_room_links_house_entrance.h"
#include "pluginbitmaps/minishcap_room_links_house_smith.h"
#include "pluginbitmaps/minishcap_room_creek_real.h"
#include "pluginbitmaps/minishcap_room_meadow_real.h"
#include "minishcap_south_hyrule_collision.h"

struct rect { int x, y, w, h; };
struct pickup_seed { int x, y; };
struct enemy_seed { int x, y, axis, range; };
struct room_transition { int room, x, y; };

struct room_def
{
    int theme;
    int pickup_count;
    struct pickup_seed pickups[3];
    int enemy_count;
    struct enemy_seed enemies[2];
    int solid_count;
    struct rect solids[8];
    bool has_stump;
    struct rect stump;
    bool has_pedestal;
    struct rect pedestal;
    int safe_x, safe_y;
    struct room_transition exits[4];
};

struct room_state { uint8_t pickup_mask; uint8_t enemy_mask; uint8_t flags; };
struct enemy_state { int x, y, origin_x, origin_y, axis, range, dir; bool alive; };
struct npc_state { int kind; int x, y; int message_index; bool present; };

struct game_state
{
    int room_id;
    int x, y;
    int facing;
    int sword_dir;
    int health;
    int rupees;
    int room_timer;
    int step_clock;
    int sword_timer;
    int message_timer;
    char line1[32], line2[32], line3[32];
    int hurt_timer;
    int quest_stage;
    int house_exit_assist;
    int house_reentry_cooldown;
    int bgm_cooldown;
    int enemy_count;
    struct enemy_state enemies[2];
    int npc_count;
    struct npc_state npcs[2];
    int pickup_mask;
    bool save_dirty;
    int save_delay;
    int camera_x, camera_y;
    bool moving;
    bool sword_equipped;
};

#define AUDIO_SAMPLE_RATE 11025
#define AUDIO_BUF_SIZE 256

static int16_t audio_buf[AUDIO_BUF_SIZE];
static int audio_buf_pos;
static bool audio_initialized;

static void audio_callback(const void **start, size_t *size)
{
    *start = audio_buf;
    *size = audio_buf_pos * sizeof(int16_t);
    audio_buf_pos = 0;
}

static void init_audio(void)
{
    if (audio_initialized) return;
    rb->pcm_play_stop();
    rb->pcm_set_frequency(AUDIO_SAMPLE_RATE);
    audio_buf_pos = 0;
    audio_initialized = true;
}

static void play_tone(unsigned freq, unsigned duration_ms)
{
    if (!audio_initialized) return;
    int samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    int phase = 0;
    int phase_inc = (freq * 256) / AUDIO_SAMPLE_RATE;
    for (int i = 0; i < samples && audio_buf_pos < AUDIO_BUF_SIZE; i++)
    {
        audio_buf[audio_buf_pos++] = (int16_t)((phase >> 8) & 0xFF);
        phase += phase_inc;
    }
    if (audio_buf_pos > 0)
        rb->pcm_play_data(audio_callback, NULL, audio_buf, audio_buf_pos * sizeof(int16_t));
}

static void play_sfx(struct game_state *game, unsigned freq, unsigned duration_ms, unsigned amplitude)
{
    (void)amplitude;
    play_tone(freq * 2, duration_ms);
    game->bgm_cooldown = 3;
}

static const unsigned bgm_bedroom[] = {392, 440, 523, 0, 523, 440, 392, 0, 0};
static const unsigned bgm_entrance[] = {330, 392, 440, 523, 440, 392, 330, 0, 0};
static const unsigned bgm_meadow[] = {523, 659, 784, 0, 784, 659, 523, 0, 0};

static int bgm_note_idx;
static int bgm_note_timer;

static void update_bgm(struct game_state *game)
{
    if (game->bgm_cooldown > 0)
    {
        game->bgm_cooldown--;
        return;
    }
    
    const unsigned *bgm = NULL;
    if (game->room_id == ROOM_LINKS_HOUSE_BEDROOM || game->room_id == ROOM_LINKS_HOUSE_ENTRANCE || game->room_id == ROOM_LINKS_HOUSE_SMITH)
        bgm = bgm_bedroom;
    else if (game->room_id == ROOM_MEADOW)
        bgm = bgm_meadow;
    else
        bgm = bgm_entrance;
    
    if (!bgm) return;
    
    bgm_note_timer++;
    if (bgm_note_timer >= 8)
    {
        bgm_note_timer = 0;
        unsigned freq = bgm[bgm_note_idx];
        if (freq > 0)
            play_tone(freq, 100);
        bgm_note_idx = (bgm_note_idx + 1) % 9;
        game->bgm_cooldown = 2;
    }
}

static bool south_hyrule_tile_blocked(int tile_x, int tile_y)
{
    extern const unsigned char south_hyrule_blocked_bits[];
    if (tile_x < 0 || tile_x >= SOUTH_HYRULE_TILE_W || tile_y < 0 || tile_y >= SOUTH_HYRULE_TILE_H)
        return true;
    int index = tile_y * SOUTH_HYRULE_TILE_W + tile_x;
    int byte_idx = index >> 3;
    int bit_idx = index & 7;
    return (south_hyrule_blocked_bits[byte_idx] & (1 << bit_idx)) != 0;
}

static bool south_hyrule_pixel_blocked(int world_x, int world_y)
{
    return south_hyrule_tile_blocked(world_x >> 4, world_y >> 4);
}

static void normalize_meadow_spawn(struct game_state *game)
{
    if (game->room_id != ROOM_MEADOW) return;
    if (south_hyrule_pixel_blocked(game->x + 2, game->y + 5) ||
        south_hyrule_pixel_blocked(game->x + PLAYER_W - 3, game->y + 5) ||
        south_hyrule_pixel_blocked(game->x + 2, game->y + PLAYER_H - 2) ||
        south_hyrule_pixel_blocked(game->x + PLAYER_W - 3, game->y + PLAYER_H - 2))
    {
        game->x = HOUSE_EXIT_SAFE_X;
        game->y = HOUSE_EXIT_SAFE_Y;
    }
}

static const struct room_def room_defs[ROOM_COUNT] = {
    {
        THEME_MEADOW, 2, {{MEADOW_WORLD_X + 90, 226}, {MEADOW_WORLD_X + 262, 236}, {0,0}},
        1, {{MEADOW_WORLD_X + 144, 256, AXIS_HORIZONTAL, 34}, {0,0,0,0}},
        0, {{0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}},
        false, {0,0,0,0}, false, {0,0,0,0},
        HOUSE_EXIT_SAFE_X, HOUSE_EXIT_SAFE_Y,
        {{ROOM_LINKS_HOUSE_ENTRANCE, 120, 144}, {-1,0,0}, {-1,0,0}, {-1,0,0}},
    },
    {
        THEME_CREEK, 2, {{144, 96}, {242, 152}, {0,0}}, 2, {{152, 116, AXIS_VERTICAL, 28}, {230, 150, AXIS_HORIZONTAL, 24}},
        7, {{0, 52, 90, 118}, {88, 44, 48, 88}, {190, 40, 130, 82}, {246, 116, 48, 24}, {0, 172, 70, 36}, {214, 160, 62, 28}, {288, 132, 22, 30}, {0,0,0,0}},
        false, {0,0,0,0}, true, {136, 62, 48, 34}, 148, 164,
        {{ROOM_CREEK, 150, 150}, {-1,0,0}, {-1,0,0}, {-1,0,0}},
    },
    {
        THEME_SHRINE, 1, {{228, 60}, {0,0}, {0,0}}, 1, {{160, 160, AXIS_HORIZONTAL, 28}, {0,0,0,0}},
        4, {{50, 54, 18, 102}, {252, 54, 18, 102}, {90, 46, 18, 26}, {212, 46, 18, 26}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}},
        false, {0,0,0,0}, true, {136, 62, 48, 34}, 148, 164,
        {{ROOM_SHRINE, 148, 164}, {-1,0,0}, {-1,0,0}, {-1,0,0}},
    },
    {
        THEME_HOUSE, 0, {{0,0}, {0,0}, {0,0}}, 0, {{0,0,0,0}, {0,0,0,0}},
        4, {{0, 0, 240, 8}, {0, 0, 8, 160}, {232, 0, 8, 160}, {0, 150, 240, 10}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}},
        false, {0,0,0,0}, false, {0,0,0,0}, BEDROOM_START_X, BEDROOM_START_Y,
        {{ROOM_LINKS_HOUSE_ENTRANCE, 12, 86}, {-1,0,0}, {-1,0,0}, {-1,0,0}},
    },
    {
        THEME_HOUSE, 0, {{0,0}, {0,0}, {0,0}}, 0, {{0,0,0,0}, {0,0,0,0}},
        4, {{0, 0, 240, 8}, {0, 0, 8, 160}, {232, 0, 8, 160}, {0, 152, 86, 8}, {154, 152, 86, 8}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}},
        false, {0,0,0,0}, false, {0,0,0,0}, 12, 86,
        {{ROOM_MEADOW, HOUSE_EXIT_SAFE_X, HOUSE_EXIT_SAFE_Y}, {-1,0,0}, {ROOM_LINKS_HOUSE_BEDROOM, BEDROOM_START_X, BEDROOM_START_Y}, {ROOM_LINKS_HOUSE_SMITH, 154, 86}},
    },
    {
        THEME_HOUSE, 0, {{0,0}, {0,0}, {0,0}}, 0, {{0,0,0,0}, {0,0,0,0}},
        4, {{0, 0, 240, 8}, {0, 0, 8, 160}, {232, 0, 8, 160}, {0, 150, 240, 10}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}},
        false, {0,0,0,0}, false, {0,0,0,0}, 148, 110,
        {{-1,0,0}, {-1,0,0}, {ROOM_LINKS_HOUSE_ENTRANCE, 12, 86}, {-1,0,0}},
    },
};

static const struct room_def *current_room(const struct game_state *game) { return &room_defs[game->room_id]; }
static bool room_uses_world_map(const struct game_state *game) { return game->room_id == ROOM_MEADOW; }
static bool room_uses_local_coords(const struct game_state *game) { return game->room_id >= ROOM_LINKS_HOUSE_BEDROOM; }

static void get_room_bounds(const struct game_state *game, int *left, int *top, int *right, int *bottom)
{
    if (room_uses_world_map(game))
    {
        *left = MEADOW_WORLD_X;
        *top = MEADOW_WORLD_Y;
        *right = MEADOW_WORLD_X + MEADOW_ROOM_W;
        *bottom = MEADOW_WORLD_Y + MEADOW_ROOM_H;
    }
    else
    {
        *left = 0;
        *top = 0;
        *right = INTERIOR_ROOM_W;
        *bottom = INTERIOR_ROOM_H;
    }
}

static int world_to_screen_x(const struct game_state *game, int x)
{
    if (game->room_id == ROOM_MEADOW) return x + 12 - game->camera_x;
    return INTERIOR_ORIGIN_X + x;
}

static int world_to_screen_y(const struct game_state *game, int y)
{
    if (game->room_id == ROOM_MEADOW) return ROOM_TOP + 18 + y - game->camera_y;
    return INTERIOR_ORIGIN_Y + y;
}

static void set_fg(unsigned color) { rb->lcd_set_foreground(color); }

static void draw_house_background(const fb_data *src, int w, int h)
{
    if (w == 240 && h == 160) rb->lcd_bitmap(src, INTERIOR_ORIGIN_X, INTERIOR_ORIGIN_Y, w, h);
}

static void draw_meadow_background(void)
{
    set_fg(0x2945);
    rb->lcd_fillrect(0, ROOM_TOP, LCD_WIDTH, ROOM_BOTTOM - ROOM_TOP);
    set_fg(0x4A52);
    int path_y = 128;
    for (int x = 0; x < LCD_WIDTH; x += 16)
    {
        rb->lcd_fillrect(x, path_y, 14, 16);
        rb->lcd_fillrect(x + 4, path_y + 16, 14, 16);
    }
    set_fg(0x39E7);
    rb->lcd_fillrect(40, 80, 8, 48);
    rb->lcd_fillrect(44, 84, 6, 40);
    rb->lcd_fillrect(42, 120, 10, 8);
    set_fg(0x7FE0);
    rb->lcd_fillrect(48, 72, 10, 10);
    rb->lcd_fillrect(50, 74, 6, 6);
}

static void draw_room_background(const struct game_state *game)
{
    if (game->room_id == ROOM_LINKS_HOUSE_BEDROOM)
        draw_house_background(minishcap_room_links_house_bedroom, 240, 160);
    else if (game->room_id == ROOM_LINKS_HOUSE_ENTRANCE)
        draw_house_background(minishcap_room_links_house_entrance, 240, 160);
    else if (game->room_id == ROOM_LINKS_HOUSE_SMITH)
        draw_house_background(minishcap_room_links_house_smith, 240, 160);
    else
        draw_meadow_background();
}

static void draw_hud_heart(int x, int y, bool filled)
{
    set_fg(COLOR_HUD_INSET);
    rb->lcd_fillrect(x + 2, y + 1, 3, 3);
    rb->lcd_fillrect(x + 7, y + 1, 3, 3);
    rb->lcd_fillrect(x + 1, y + 4, 10, 3);
    rb->lcd_fillrect(x + 3, y + 7, 6, 3);
    rb->lcd_fillrect(x + 4, y + 10, 4, 2);
    set_fg(filled ? COLOR_HEART : COLOR_HUD_PARCHMENT_DARK);
    rb->lcd_fillrect(x + 3, y + 2, 2, 2);
    rb->lcd_fillrect(x + 7, y + 2, 2, 2);
    rb->lcd_fillrect(x + 2, y + 5, 8, 1);
    rb->lcd_fillrect(x + 3, y + 6, 6, 2);
    rb->lcd_fillrect(x + 4, y + 8, 4, 2);
    rb->lcd_fillrect(x + 5, y + 10, 2, 1);
}

static void draw_hud(const struct game_state *game)
{
    int left_x = 8;
    int right_x = LCD_WIDTH - 80;
    set_fg(COLOR_HUD_PARCHMENT_SHADOW);
    rb->lcd_fillrect(4, 4, 70, 24);
    rb->lcd_fillrect(LCD_WIDTH - 76, 4, 72, 24);
    set_fg(COLOR_HUD_PARCHMENT_DARK);
    rb->lcd_fillrect(6, 6, 66, 20);
    rb->lcd_fillrect(LCD_WIDTH - 74, 6, 68, 20);
    set_fg(COLOR_PANEL_ACCENT);
    rb->lcd_drawrect(6, 6, 66, 20);
    rb->lcd_drawrect(LCD_WIDTH - 74, 6, 68, 20);
    set_fg(COLOR_HUD_INSET);
    rb->lcd_putsxy(left_x + 8, 6, "LIFE");
    for (int i = 0; i < PLAYER_MAX_HEALTH; ++i)
        draw_hud_heart(left_x + 8 + i * 15, 11, i < game->health);
    set_fg(COLOR_MINIMAP_MARKER);
    rb->lcd_fillrect(right_x + 10, 10, 12, 12);
    set_fg(COLOR_PANEL_TEXT);
    rb->lcd_fillrect(right_x + 12, 12, 8, 8);
    char line[16];
    rb->snprintf(line, sizeof(line), "%d", game->rupees);
    set_fg(COLOR_PANEL_TEXT);
    rb->lcd_putsxy(right_x + 26, 13, line);
    rb->lcd_putsxy(8, LCD_HEIGHT - 10, MINISHCAP_BUILD_TAG);
}

static void draw_player(const struct game_state *game)
{
    const fb_data *sprite = minishcap_link_front;
    int sprite_w = 24, sprite_h = 24;
    bool step = game->moving && ((game->step_clock / 3) & 1);
    int draw_x = world_to_screen_x(game, game->x);
    int draw_y = world_to_screen_y(game, game->y);
    switch (game->facing)
    {
        case FACE_UP: sprite = minishcap_link_back; break;
        case FACE_LEFT: sprite = (MINISHCAP_SWAP_LEFT_RIGHT) ? (step ? minishcap_link_right_step : minishcap_link_right) : (step ? minishcap_link_left_step : minishcap_link_left); break;
        case FACE_RIGHT: sprite = (MINISHCAP_SWAP_LEFT_RIGHT) ? (step ? minishcap_link_left_step : minishcap_link_left) : (step ? minishcap_link_right_step : minishcap_link_right); break;
        default: sprite = (step ? minishcap_link_front_step : minishcap_link_front); break;
    }
    rb->lcd_bitmap_transparent(sprite, draw_x, draw_y, sprite_w, sprite_h);
    
    if (game->sword_equipped && game->sword_timer > 0)
    {
        int base_x = draw_x + 7;
        int base_y = draw_y + 7;
        set_fg(COLOR_SWORD);
        switch (game->sword_dir)
        {
            case FACE_UP:
                rb->lcd_fillrect(base_x - 1, base_y - 12, 3, 10);
                rb->lcd_fillrect(base_x - 3, base_y - 15, 7, 2);
                break;
            case FACE_LEFT:
                rb->lcd_fillrect(base_x - 12, base_y - 1, 10, 3);
                rb->lcd_fillrect(base_x - 15, base_y - 3, 2, 7);
                break;
            case FACE_RIGHT:
                rb->lcd_fillrect(base_x + 2, base_y - 1, 10, 3);
                rb->lcd_fillrect(base_x + 4, base_y - 3, 2, 7);
                break;
            default:
                rb->lcd_fillrect(base_x - 1, base_y + 4, 3, 10);
                rb->lcd_fillrect(base_x - 3, base_y + 14, 7, 2);
                break;
        }
    }
}

static void draw_dialog(const struct game_state *game)
{
    if (game->message_timer <= 0 || game->line1[0] == '\0') return;
    
    int box_x = 18;
    int box_w = LCD_WIDTH - (box_x * 2);
    int box_h = 32;
    int box_y = LCD_HEIGHT - 50;
    
    set_fg(COLOR_DIALOG_BORDER);
    rb->lcd_drawrect(box_x, box_y, box_w, box_h);
    set_fg(COLOR_DIALOG);
    rb->lcd_drawrect(box_x + 1, box_y + 1, box_w - 2, box_h - 2);
    set_fg(COLOR_DIALOG_TEXT);
    rb->lcd_putsxy(box_x + 8, box_y + 4, game->line1);
    if (game->line2[0])
        rb->lcd_putsxy(box_x + 8, box_y + 14, game->line2);
    if (game->line3[0])
        rb->lcd_putsxy(box_x + 8, box_y + 24, game->line3);
}

static void draw_screen(const struct game_state *game)
{
    // Clear frame to a stable background to avoid black HUD artifacts
    set_fg(COLOR_ROOM_WALL);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    draw_room_background(game);
    draw_player(game);
    draw_hud(game);
    draw_dialog(game);
    rb->lcd_update();
}

static void begin_room(struct game_state *game, int room_id, int x, int y)
{
    game->room_id = room_id;
    game->x = x;
    game->y = y;
    game->room_timer = 0;
    game->house_exit_assist = 0;
    game->house_reentry_cooldown = 0;
    game->enemy_count = 0;
    game->npc_count = 0;
    game->pickup_mask = 0;
    game->line1[0] = '\0';
    game->line2[0] = '\0';
    game->line3[0] = '\0';
    game->message_timer = 0;
    normalize_meadow_spawn(game);
}

static void init_default_game(struct game_state *game)
{
    memset(game, 0, sizeof(*game));
    game->health = PLAYER_MAX_HEALTH;
    game->facing = FACE_DOWN;
    game->sword_dir = FACE_DOWN;
    game->sword_equipped = false;
    game->quest_stage = 1;
    begin_room(game, ROOM_LINKS_HOUSE_BEDROOM, BEDROOM_START_X, BEDROOM_START_Y);
    game->x = BEDROOM_START_X;
    game->y = BEDROOM_START_Y;
}

static int map_buttons(long button)
{
    int cmd = 0;
    if (button & BUTTON_LEFT) cmd |= CMD_LEFT;
    if (button & BUTTON_RIGHT) cmd |= CMD_RIGHT;
    if (button & BUTTON_MENU) cmd |= CMD_UP;
    if (button & BUTTON_PLAY) cmd |= CMD_DOWN;
    return cmd;
}

static int clean_button(long button)
{
    return button & (BUTTON_LEFT | BUTTON_RIGHT | BUTTON_MENU | BUTTON_PLAY | BUTTON_SELECT);
}

static bool try_transition(struct game_state *game, int edge)
{
    const struct room_transition *exit = &current_room(game)->exits[edge];
    if (exit->room < 0) return false;
    play_sfx(game, 510, 65, 1200);
    begin_room(game, exit->room, exit->x, exit->y);
    return true;
}

static void update_game(struct game_state *game, int cmd, int event)
{
    if (game->message_timer > 0)
    {
        game->message_timer--;
        if (game->message_timer == 0)
        {
            game->line1[0] = '\0';
            game->line2[0] = '\0';
            game->line3[0] = '\0';
        }
        return;
    }

    int dx = 0, dy = 0;
    game->moving = false;
    int ndx = 0, ndy = 0;
    if (cmd & CMD_LEFT) ndx = -PLAYER_SPEED;
    if (cmd & CMD_RIGHT) ndx = PLAYER_SPEED;
    if (cmd & CMD_UP) ndy = -PLAYER_SPEED;
    if (cmd & CMD_DOWN) ndy = PLAYER_SPEED;
    dx = ndx;
    dy = ndy;
    if (dx != 0 || dy != 0) {
        game->moving = true;
        if (dx != 0) game->facing = (dx < 0) ? FACE_LEFT : FACE_RIGHT;
        else if (dy != 0) game->facing = (dy < 0) ? FACE_UP : FACE_DOWN;
    }
    
    int next_x = game->x + dx;
    int next_y = game->y + dy;

    int room_left, room_top, room_right, room_bottom;
    get_room_bounds(game, &room_left, &room_top, &room_right, &room_bottom);
    
    if (next_x < room_left && try_transition(game, FACE_LEFT)) return;
    if (next_x + PLAYER_W > room_right && try_transition(game, FACE_RIGHT)) return;
    if (next_y < room_top && try_transition(game, FACE_UP)) return;
    if (next_y + PLAYER_H > room_bottom && try_transition(game, FACE_DOWN)) return;

    if (next_x < room_left) next_x = room_left;
    if (next_x + PLAYER_W > room_right) next_x = room_right - PLAYER_W;
    if (next_y < room_top) next_y = room_top;
    if (next_y + PLAYER_H > room_bottom) next_y = room_bottom - PLAYER_H;
    
    // Interior collision: clamp movement against defined solids in the room
    if (game->room_id != ROOM_MEADOW) {
        const struct room_def *rd = current_room(game);
        for (int i = 0; i < rd->solid_count; i++) {
            struct rect s = rd->solids[i];
            if (next_x < s.x + s.w && next_x + PLAYER_W > s.x &&
                next_y < s.y + s.h && next_y + PLAYER_H > s.y) {
                // collision: cancel movement along both axes for simplicity
                next_x = game->x;
                next_y = game->y;
                dx = 0; dy = 0;
                break;
            }
        }
    }
    game->x = next_x;
    game->y = next_y;
    if (game->moving) game->step_clock++;
    if (game->sword_timer > 0) game->sword_timer--;
    update_bgm(game);
}

static void load_external_bitmaps(void) { }

enum plugin_status plugin_start(const void *parameter)
{
    struct game_state game;
    enum plugin_status status = PLUGIN_OK;
    (void)parameter;
    rb->splash(HZ, MINISHCAP_BUILD_TAG);
    rb->lcd_set_foreground(COLOR_BLACK);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    rb->lcd_update();
    init_audio();
    load_external_bitmaps();
    init_default_game(&game);
    while (true)
    {
        long event = rb->button_get_w_tmo(HZ / 30);
        int held = clean_button(rb->button_status());
        int cmd = map_buttons(held);
        int clean_event = clean_button(event);
        if (event == SYS_USB_CONNECTED || rb->default_event_handler(event) == SYS_USB_CONNECTED)
        {
            status = PLUGIN_USB_CONNECTED;
            break;
        }
        if ((held & (BUTTON_MENU | BUTTON_SELECT)) == (BUTTON_MENU | BUTTON_SELECT))
        {
            status = PLUGIN_OK;
            break;
        }
        update_game(&game, cmd, clean_event);
        draw_screen(&game);
    }
    return status;
}
