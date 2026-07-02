#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

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

#define HOUSE_EXIT_SAFE_X 0x290
#define HOUSE_EXIT_SAFE_Y 0x188

#define TMC_TO_PLAYER_X(tmc_x) ((tmc_x) - 7)
#define TMC_TO_PLAYER_Y(tmc_y) ((tmc_y) - 16 + 2)
#define MEADOW_WORLD_X 0
#define MEADOW_WORLD_Y 0
#define MEADOW_ROOM_W 1008
#define MEADOW_ROOM_H 688
#define CREEK_WORLD_X 0
#define CREEK_WORLD_Y 0
#define CREEK_ROOM_W 320
#define CREEK_ROOM_H 208
#define HOUSE_DOOR_Y 82
#define HOUSE_DOOR_H 30
#define HOUSE_EXIT_LOCK_X 98
#define HOUSE_EXIT_LOCK_Y 142
#define HOUSE_EXIT_LOCK_W 44
#define HOUSE_EXIT_LOCK_H 18
#define HOUSE_FRONT_EXIT_X 84
#define HOUSE_FRONT_EXIT_Y 136
#define HOUSE_FRONT_EXIT_W 72
#define HOUSE_FRONT_EXIT_H 28
#define MEADOW_HOUSE_ENTRY_X (HOUSE_EXIT_SAFE_X - 14)
#define MEADOW_HOUSE_ENTRY_Y (HOUSE_EXIT_SAFE_Y - 22)
#define MEADOW_HOUSE_ENTRY_W 30
#define MEADOW_HOUSE_ENTRY_H 20
#define ENTRANCE_FROM_MEADOW_X 120
#define ENTRANCE_FROM_MEADOW_Y 118

#define SOUTH_HYRULE_LINK_HOUSE_EXIT_X 0x290
#define SOUTH_HYRULE_LINK_HOUSE_EXIT_Y 0x188
#define SOUTH_HYRULE_TREE_HOLE_X 0x3a0
#define SOUTH_HYRULE_TREE_HOLE_Y 0x228
#define SOUTH_HYRULE_FAIRY_CAVE_X 0x118
#define SOUTH_HYRULE_FAIRY_CAVE_Y 0x0a8
#define SOUTH_HYRULE_MINISH_CAVE_X 0x178
#define SOUTH_HYRULE_MINISH_CAVE_Y 0x0d8
#define SOUTH_HYRULE_MINISH_CAVE_RETURN_X 0x178
#define SOUTH_HYRULE_MINISH_CAVE_RETURN_Y 0x0e8
#define SOUTH_HYRULE_POI_TRIGGER_HALF 10

#define MEADOW_TO_CREEK_X (MEADOW_WORLD_X + MEADOW_ROOM_W - 10)
#define MEADOW_TO_CREEK_Y (MEADOW_WORLD_Y + 8)
#define MEADOW_TO_CREEK_W 12
#define MEADOW_TO_CREEK_H (MEADOW_ROOM_H - 16)
#define CREEK_TO_MEADOW_X 0
#define CREEK_TO_MEADOW_Y 8
#define CREEK_TO_MEADOW_W 12
#define CREEK_TO_MEADOW_H (CREEK_ROOM_H - 16)
#define SOUTH_HYRULE_BMP_FILE PLUGIN_GAMES_DIR "/minishcap_south_hyrule_full.bmp"
#define SOUTH_HYRULE_BMP_FILE_ALT PLUGIN_GAMES_DIR "/minishcap_south_hyrule_full.1008x688x24.bmp"
#define SOUTH_HYRULE_CHUNK_FILE_FMT PLUGIN_GAMES_DIR "/minishcap_south_hyrule_chunk_%d_%d.bmp"
#define SOUTH_HYRULE_CHUNK_FILE_FMT_ALT PLUGIN_GAMES_DIR "/minishcap_south_hyrule_chunk_%d_%d.252x172x24.bmp"
#define MEADOW_CHUNK_W 252
#define MEADOW_CHUNK_H 172
#define MEADOW_CHUNK_COLS 4
#define MEADOW_CHUNK_ROWS 4
#define MEADOW_CHUNK_PIXELS (MEADOW_CHUNK_W * MEADOW_CHUNK_H)
#define MEADOW_CHUNK_BYTES (MEADOW_CHUNK_PIXELS * sizeof(fb_data))
#define MEADOW_CHUNK_SLOTS 4
#define SWORD_PICKUP_X 136
#define SWORD_PICKUP_Y 88
#define SWORD_PICKUP_W 28
#define SWORD_PICKUP_H 24
#define SMITH_INTRO_X 0x80
#define SMITH_INTRO_Y 0x50
#define ZELDA_SMITH_X 0x60
#define ZELDA_SMITH_Y 0x50
#define SMITH_POST_X 0xb8
#define SMITH_POST_Y 0x60

#define SOUTH_HYRULE_TILE_W 63
#define SOUTH_HYRULE_TILE_H 43

#define TMC_UI_X_OFF ((LCD_WIDTH - 240) / 2)
#define TMC_UI_BTN_B_X (0xb8 + TMC_UI_X_OFF)
#define TMC_UI_BTN_A_X (0xd8 + TMC_UI_X_OFF)
#define TMC_UI_BTN_R_X (0xd0 + TMC_UI_X_OFF)
#define TMC_UI_BTN_AB_Y 0x1c
#define TMC_UI_BTN_R_Y 0x0e

enum { FACE_DOWN = 0, FACE_UP, FACE_LEFT, FACE_RIGHT };
enum { CMD_LEFT = 1, CMD_RIGHT = 2, CMD_UP = 4, CMD_DOWN = 8 };
enum { ROOM_MEADOW = 0, ROOM_CREEK, ROOM_SHRINE, ROOM_MINISH_CAVE, ROOM_LINKS_HOUSE_BEDROOM, ROOM_LINKS_HOUSE_ENTRANCE, ROOM_LINKS_HOUSE_SMITH, ROOM_COUNT };
enum { THEME_MEADOW = 0, THEME_CREEK, THEME_SHRINE, THEME_HOUSE };
enum { AXIS_HORIZONTAL = 0, AXIS_VERTICAL };
enum { NPCTYPE_NONE = 0, NPCTYPE_ZELDA, NPCTYPE_SMITH };
enum { MENU_TAB_STATUS = 0, MENU_TAB_ITEMS, MENU_TAB_QUEST, MENU_TAB_MAP, MENU_TAB_COUNT };
enum {
    TRANSITION_FAIRY_CAVE = 0,
    TRANSITION_MINISH_CAVE,
    TRANSITION_TREE_HOLLOW,
    TRANSITION_MEADOW_EDGE_TO_CREEK,
    TRANSITION_MEADOW_ZONE_TO_CREEK,
    TRANSITION_CREEK_EDGE_TO_MEADOW,
    TRANSITION_CREEK_ZONE_TO_MEADOW,
    TRANSITION_SHRINE_EXIT,
    TRANSITION_MINISH_CAVE_EXIT,
    TRANSITION_BEDROOM_TO_ENTRANCE,
    TRANSITION_ENTRANCE_TO_BEDROOM,
    TRANSITION_ENTRANCE_TO_MEADOW,
    TRANSITION_ENTRANCE_TO_SMITH,
    TRANSITION_SMITH_TO_ENTRANCE,
    TRANSITION_MEADOW_TO_ENTRANCE,
};

#define COLOR_BLACK 0
#define COLOR_WHITE 255
#define COLOR_HEART 0xF800
#define MINISHCAP_USE_REAL_ASSETS 0
#define MINISHCAP_SWAP_LEFT_RIGHT 1
#define COLOR_SWORD 0x39E7
#define COLOR_HUD_PARCHMENT_DARK 0x18C3
#define COLOR_HUD_PARCHMENT_SHADOW 0x1059
#define COLOR_HUD_INSET 0x18C3
#define COLOR_PANEL_ACCENT 0x31A0
#define COLOR_PANEL_TEXT 0xFFFF
#define COLOR_DIALOG 0x3186
#define COLOR_DIALOG_BORDER 0x0000
#define COLOR_DIALOG_TEXT 0xFFFF
#define COLOR_ROOM_FLOOR 0x18C3
#define COLOR_ROOM_WALL 0x2945
#define COLOR_ROOM_OBJECT 0x31A0
#define COLOR_MINIMAP_MARKER 0xFF00
#define MINISHCAP_BUILD_TAG "MC-SIM r4"
#define LINK_USE_BITMAP_SPRITES 1
#define LINK_TRANSPARENT_KEY 0x0000
#define MINISHCAP_DEBUG_OVERLAY_DEFAULT 1
#define COLOR_UI_BG 0x10A2
#define COLOR_UI_FRAME 0x6B4D
#define COLOR_UI_TEXT 0xFFFF

enum {
    ASSET_WARN_NONE = 0,
    ASSET_WARN_SMITH = 1 << 0,
    ASSET_WARN_ZELDA = 1 << 1,
};

#define NPC_BMP_W 24
#define NPC_BMP_H 24
#define NPC_BMP_PIXELS (NPC_BMP_W * NPC_BMP_H)
#define NPC_BMP_BYTES (NPC_BMP_PIXELS * sizeof(fb_data))
#define SMITH_BMP_FILE PLUGIN_GAMES_DIR "/minishcap_smith_real.bmp"
#define SMITH_BMP_FILE_ALT PLUGIN_GAMES_DIR "/minishcap_smith_real.24x24x24.bmp"
#define ZELDA_BMP_FILE PLUGIN_GAMES_DIR "/minishcap_zelda_real.bmp"
#define ZELDA_BMP_FILE_ALT PLUGIN_GAMES_DIR "/minishcap_zelda_real.24x24x24.bmp"
#define MINISHCAP_TRACE_FILE PLUGIN_GAMES_DIR "/minishcap_trace.log"

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
    int interaction_cooldown;
    bool menu_open;
    int menu_tab;
    bool debug_overlay;
    int debug_last_cmd;
    int debug_last_event;
};

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_MIX_SAMPLES 1024
#define FRAME_TIME_TICKS ((HZ / 30) > 0 ? (HZ / 30) : 1)
#define MINISHCAP_TRACE_MOVEMENT 0
#define MINISHCAP_ENABLE_DEBUG_JUMPS 0

static int16_t audio_mix_buf[AUDIO_MIX_SAMPLES];
static bool audio_initialized;
static bool audio_stream_running;
static struct bitmap smith_bmp;
static struct bitmap zelda_bmp;
static fb_data smith_bmp_data[NPC_BMP_PIXELS];
static fb_data zelda_bmp_data[NPC_BMP_PIXELS];
static bool smith_bmp_loaded;
static bool zelda_bmp_loaded;
static struct bitmap south_hyrule_bmp;
static fb_data *south_hyrule_bmp_data;
static bool south_hyrule_bmp_loaded;
static uint8_t asset_warning_mask;
static struct bitmap meadow_chunk_bmps[MEADOW_CHUNK_ROWS][MEADOW_CHUNK_COLS];
static bool meadow_chunk_loaded[MEADOW_CHUNK_ROWS][MEADOW_CHUNK_COLS];
static bool meadow_chunk_missing[MEADOW_CHUNK_ROWS][MEADOW_CHUNK_COLS];
static fb_data meadow_chunk_storage[MEADOW_CHUNK_SLOTS][MEADOW_CHUNK_PIXELS];
static int meadow_chunk_slot_map[MEADOW_CHUNK_ROWS][MEADOW_CHUNK_COLS];
static int meadow_chunk_slot_row[MEADOW_CHUNK_SLOTS];
static int meadow_chunk_slot_col[MEADOW_CHUNK_SLOTS];
static int meadow_chunk_lru_tick;

struct synth_voice
{
    bool active;
    unsigned freq;
    int samples_left;
    uint32_t phase;
    int amplitude;
};

static struct synth_voice bgm_voice;
static struct synth_voice sfx_voice;
static int bgm_profile_logged;

static bool rect_overlap(int x, int y, int w, int h, int rx, int ry, int rw, int rh);
static void play_sfx(struct game_state *game, unsigned freq, unsigned duration_ms, unsigned amplitude);
static void begin_room(struct game_state *game, int room_id, int x, int y);
static bool ensure_meadow_chunk_loaded(int row, int col);

static void trace_log(const char *fmt, ...)
{
    va_list ap;
    char line[192];
    int fd;

    va_start(ap, fmt);
    rb->vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    fd = rb->open(MINISHCAP_TRACE_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;

    rb->fdprintf(fd, "%s\n", line);
    rb->close(fd);
}

static void log_transition_event(const char *phase, const char *kind, int index,
                                 int from_room, int from_x, int from_y,
                                 int to_room, int to_x, int to_y)
{
    DEBUGF("minishcap transition %s kind=%s idx=%d from_room=%d from=(%d,%d) to_room=%d spawn=(%d,%d)\n",
           phase, kind, index, from_room, from_x, from_y, to_room, to_x, to_y);
    trace_log("minishcap transition %s kind=%s idx=%d from_room=%d from=(%d,%d) to_room=%d spawn=(%d,%d)",
              phase, kind, index, from_room, from_x, from_y, to_room, to_x, to_y);
}

static void begin_transition(struct game_state *game, const char *kind, int index,
                             int room_id, int x, int y)
{
    int from_room = game->room_id;
    int from_x = game->x;
    int from_y = game->y;

    log_transition_event("pre", kind, index, from_room, from_x, from_y,
                         room_id, x, y);
    begin_room(game, room_id, x, y);
    log_transition_event("post", kind, index, from_room, from_x, from_y,
                         game->room_id, game->x, game->y);
}

static void set_message(struct game_state *game, const char *l1, const char *l2, const char *l3, int ticks)
{
    rb->snprintf(game->line1, sizeof(game->line1), "%s", l1 ? l1 : "");
    rb->snprintf(game->line2, sizeof(game->line2), "%s", l2 ? l2 : "");
    rb->snprintf(game->line3, sizeof(game->line3), "%s", l3 ? l3 : "");
    game->message_timer = ticks > 0 ? ticks : FRAME_TIME_TICKS;
}

static void timer_step_down(int *value)
{
    if (*value <= 0)
        return;

    *value -= FRAME_TIME_TICKS;
    if (*value < 0)
        *value = 0;
}

static void blit_skip_black(const fb_data *src, int width, int height, int dst_x, int dst_y)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            fb_data pixel = src[y * width + x];
            if (pixel == (fb_data)COLOR_BLACK)
                continue;
            rb->lcd_set_foreground(pixel);
            rb->lcd_drawpixel(dst_x + x, dst_y + y);
        }
    }
}

static void draw_smith_at(int x, int y)
{
    if (smith_bmp_loaded)
    {
        blit_skip_black((const fb_data*)smith_bmp.data, NPC_BMP_W, NPC_BMP_H, x, y);
    }
    else
    {
        rb->lcd_set_foreground(COLOR_ROOM_OBJECT);
        rb->lcd_fillrect(x + 6, y + 4, 11, 16);
    }
}

static void draw_zelda_at(int x, int y)
{
    if (zelda_bmp_loaded)
    {
        blit_skip_black((const fb_data*)zelda_bmp.data, NPC_BMP_W, NPC_BMP_H, x, y);
    }
    else
    {
        rb->lcd_set_foreground(COLOR_HEART);
        rb->lcd_fillrect(x + 5, y + 5, 10, 14);
    }
}

static int voice_sample(struct synth_voice *voice)
{
    if (!voice->active || voice->freq < 80)
        return 0;

    uint32_t inc = (uint32_t)(((uint64_t)voice->freq << 32) / AUDIO_SAMPLE_RATE);
    voice->phase += inc;

    int out = (voice->phase & 0x80000000u) ? voice->amplitude : -voice->amplitude;

    if (voice->samples_left > 0)
    {
        voice->samples_left--;
        if (voice->samples_left == 0)
            voice->active = false;
    }

    return out;
}

static void audio_callback(const void **start, size_t *size)
{
    for (int i = 0; i < AUDIO_MIX_SAMPLES; ++i)
    {
        int mixed = voice_sample(&bgm_voice) + voice_sample(&sfx_voice);

        if (mixed > 32767)
            mixed = 32767;
        else if (mixed < -32768)
            mixed = -32768;

        audio_mix_buf[i] = (int16_t)mixed;
    }

    *start = audio_mix_buf;
    *size = sizeof(audio_mix_buf);
}

static bool try_select_interaction(struct game_state *game, int event)
{
    if (!(event & BUTTON_SELECT)) return false;
    if (game->menu_open)
    {
        game->menu_open = false;
        game->interaction_cooldown = HZ / 4;
        return true;
    }
    if (game->message_timer > 0) return true;
    if (game->interaction_cooldown > 0) return true;

    if (game->room_id == ROOM_LINKS_HOUSE_SMITH &&
        rect_overlap(game->x, game->y, PLAYER_W, PLAYER_H, SMITH_INTRO_X - 18, SMITH_INTRO_Y - 8, 64, 44))
    {
        if (!game->sword_equipped)
        {
            set_message(game, "Smith: Take this sword.", "", "", HZ);
        }
        else
        {
            set_message(game, "Smith: Go outside now!", "", "", HZ);
        }
        game->interaction_cooldown = HZ / 3;
        return true;
    }

    if (game->sword_equipped)
    {
        game->sword_dir = game->facing;
        game->sword_timer = 7;
        play_sfx(game, 620, 50, 1200);
        return true;
    }

    return false;
}

static void init_audio(void)
{
    long deadline;

    if (audio_initialized) return;
    if (rb->audio_status())
    {
        rb->audio_stop();
        deadline = *rb->current_tick + HZ * 3;
        while ((rb->audio_status() & AUDIO_STATUS_PLAY) &&
               TIME_BEFORE(*rb->current_tick, deadline))
        {
            rb->sleep(1);
        }
    }
    rb->pcm_play_stop();
    rb->pcm_set_frequency(AUDIO_SAMPLE_RATE);
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
#if defined(HAVE_CS42L55) && !defined(SIMULATOR)
    rb->audiohw_idle_powerup();
#endif
    memset(&bgm_voice, 0, sizeof(bgm_voice));
    memset(&sfx_voice, 0, sizeof(sfx_voice));
    bgm_profile_logged = -1;
    audio_initialized = true;
    audio_stream_running = false;
    trace_log("minishcap audio init rate=%d", AUDIO_SAMPLE_RATE);

}

static void shutdown_audio(void)
{
    if (!audio_initialized)
        return;

    rb->pcm_play_stop();
    rb->pcm_set_frequency(HW_SAMPR_DEFAULT);
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
#if defined(HAVE_CS42L55) && !defined(SIMULATOR)
    rb->audiohw_idle_powerdown();
#endif
    audio_initialized = false;
    audio_stream_running = false;
}

static void ensure_audio_stream(void)
{
    if (!audio_initialized || audio_stream_running)
        return;

    audio_stream_running = true;
    rb->pcm_play_data(audio_callback, NULL, audio_mix_buf, sizeof(audio_mix_buf));
}

static void play_bgm_tone(unsigned freq, unsigned duration_ms)
{
    if (!audio_initialized) return;

    if (freq < 100 || freq > 2000)
    {
        bgm_voice.active = false;
        return;
    }

    bgm_voice.freq = freq;
    bgm_voice.phase = 0;
    bgm_voice.amplitude = 900;
    bgm_voice.samples_left = (int)(((uint64_t)AUDIO_SAMPLE_RATE * duration_ms) / 1000);
    if (bgm_voice.samples_left <= 0)
        bgm_voice.samples_left = AUDIO_SAMPLE_RATE / 18;

    bgm_voice.active = true;
    ensure_audio_stream();
}

static void play_sfx_tone(unsigned freq, unsigned duration_ms, int amplitude)
{
    if (!audio_initialized) return;
    if (freq < 100 || freq > 2400) return;

    if (amplitude < 400)
        amplitude = 400;
    else if (amplitude > 9000)
        amplitude = 9000;

    sfx_voice.freq = freq;
    sfx_voice.phase = 0;
    sfx_voice.amplitude = amplitude;
    sfx_voice.samples_left = (int)(((uint64_t)AUDIO_SAMPLE_RATE * duration_ms) / 1000);
    if (sfx_voice.samples_left <= 0)
        sfx_voice.samples_left = AUDIO_SAMPLE_RATE / 25;

    sfx_voice.active = true;
    ensure_audio_stream();
}

static void play_sfx(struct game_state *game, unsigned freq, unsigned duration_ms, unsigned amplitude)
{
    int sfx_amp = 2600;

    if (amplitude > 0)
        sfx_amp = (int)amplitude;

    play_sfx_tone(freq, duration_ms, sfx_amp);
    trace_log("minishcap sfx freq=%u dur_ms=%u amp=%d", freq, duration_ms, sfx_amp);
    if (game != NULL)
        game->bgm_cooldown = 2;
}

static const unsigned bgm_decomp_house[] = {262, 294, 330, 392, 330, 294, 262, 220, 0, 220, 262, 294};
static const unsigned bgm_decomp_hyrule_field[] = {392, 440, 494, 587, 659, 587, 494, 440, 392, 349, 330, 294};
static const unsigned bgm_decomp_minish_cap[] = {523, 587, 659, 784, 659, 587, 523, 494, 440, 392, 349, 330};
static const unsigned bgm_decomp_cave[] = {196, 220, 247, 262, 247, 220, 196, 175};

static int bgm_note_idx;
static int bgm_note_timer;

struct bgm_track {
    const unsigned *notes;
    int length;
    int step_frames;
    int note_ms;
};

static void update_bgm(struct game_state *game)
{
    int bgm_profile = 0;

    if (game->bgm_cooldown > 0)
    {
        game->bgm_cooldown--;
        return;
    }
    
    struct bgm_track track;
    if (game->menu_open)
    {
        bgm_profile = 1;
        track.notes = bgm_decomp_house;
        track.length = 12;
        track.step_frames = 12;
        track.note_ms = 120;
    }
    else if (game->room_id == ROOM_LINKS_HOUSE_BEDROOM || game->room_id == ROOM_LINKS_HOUSE_ENTRANCE || game->room_id == ROOM_LINKS_HOUSE_SMITH)
    {
        bgm_profile = 2;
        track.notes = bgm_decomp_house;
        track.length = 12;
        track.step_frames = 8;
        track.note_ms = 160;
    }
    else if (game->room_id == ROOM_MEADOW)
    {
        bgm_profile = game->sword_equipped ? 3 : 4;
        track.notes = game->sword_equipped ? bgm_decomp_hyrule_field : bgm_decomp_minish_cap;
        track.length = 12;
        track.step_frames = game->sword_equipped ? 7 : 8;
        track.note_ms = game->sword_equipped ? 180 : 160;
    }
    else if (game->room_id == ROOM_MINISH_CAVE)
    {
        bgm_profile = 5;
        track.notes = bgm_decomp_cave;
        track.length = 8;
        track.step_frames = 10;
        track.note_ms = 170;
    }
    else
    {
        bgm_profile = 6;
        track.notes = bgm_decomp_hyrule_field;
        track.length = 12;
        track.step_frames = 7;
        track.note_ms = 175;
    }

    if (!track.notes || track.length <= 0) return;

    if (bgm_profile != bgm_profile_logged)
    {
        trace_log("minishcap bgm profile=%d room=%d sword=%d menu=%d", bgm_profile,
                  game->room_id, game->sword_equipped ? 1 : 0, game->menu_open ? 1 : 0);
        bgm_profile_logged = bgm_profile;
    }
    
    bgm_note_timer++;
    if (bgm_note_timer >= track.step_frames)
    {
        bgm_note_timer = 0;
        unsigned freq = track.notes[bgm_note_idx];
        if (freq > 0)
            play_bgm_tone(freq, track.note_ms);
        else
            bgm_voice.active = false;
        bgm_note_idx = (bgm_note_idx + 1) % track.length;
        game->bgm_cooldown = 0;
    }
}

static const char *asset_warning_text(int *out_duration)
{
    if (asset_warning_mask == ASSET_WARN_NONE)
        return NULL;

    if (asset_warning_mask == (ASSET_WARN_SMITH | ASSET_WARN_ZELDA))
    {
        if (out_duration) *out_duration = HZ * 3;
        return "Missing NPC BMPs";
    }

    if (asset_warning_mask & ASSET_WARN_SMITH)
    {
        if (out_duration) *out_duration = HZ * 3;
        return "Missing Smith BMP";
    }

    if (asset_warning_mask & ASSET_WARN_ZELDA)
    {
        if (out_duration) *out_duration = HZ * 3;
        return "Missing Zelda BMP";
    }

    if (out_duration) *out_duration = HZ * 2;
    return "Missing optional assets";
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

static bool meadow_player_blocked_at(int x, int y)
{
    int foot_y = y + PLAYER_H - 2;
    int knee_y = y + PLAYER_H - 7;
    int center_x = x + (PLAYER_W / 2);

    return south_hyrule_pixel_blocked(x + 2, knee_y) ||
           south_hyrule_pixel_blocked(x + PLAYER_W - 3, knee_y) ||
           south_hyrule_pixel_blocked(center_x, knee_y) ||
           south_hyrule_pixel_blocked(x + 2, y + PLAYER_H - 2) ||
           south_hyrule_pixel_blocked(x + PLAYER_W - 3, foot_y) ||
           south_hyrule_pixel_blocked(center_x, foot_y);
}

static void clamp_meadow_player_pos(int *x, int *y)
{
    if (*x < MEADOW_WORLD_X)
        *x = MEADOW_WORLD_X;
    if (*x + PLAYER_W > MEADOW_WORLD_X + MEADOW_ROOM_W)
        *x = MEADOW_WORLD_X + MEADOW_ROOM_W - PLAYER_W;
    if (*y < MEADOW_WORLD_Y)
        *y = MEADOW_WORLD_Y;
    if (*y + PLAYER_H > MEADOW_WORLD_Y + MEADOW_ROOM_H)
        *y = MEADOW_WORLD_Y + MEADOW_ROOM_H - PLAYER_H;
}

static void resolve_meadow_spawn(struct game_state *game)
{
    int base_x = game->x;
    int base_y = game->y;

    clamp_meadow_player_pos(&base_x, &base_y);
    if (!meadow_player_blocked_at(base_x, base_y))
    {
        game->x = base_x;
        game->y = base_y;
        return;
    }

    for (int radius = 4; radius <= 160; radius += 4)
    {
        for (int dy = -radius; dy <= radius; dy += 4)
        {
            int dx = radius - abs(dy);
            int c1x = base_x + dx;
            int c2x = base_x - dx;
            int cy = base_y + dy;

            clamp_meadow_player_pos(&c1x, &cy);
            if (!meadow_player_blocked_at(c1x, cy))
            {
                game->x = c1x;
                game->y = cy;
                return;
            }

            cy = base_y + dy;
            clamp_meadow_player_pos(&c2x, &cy);
            if (!meadow_player_blocked_at(c2x, cy))
            {
                game->x = c2x;
                game->y = cy;
                return;
            }
        }
    }

    game->x = MEADOW_WORLD_X + 24;
    game->y = MEADOW_WORLD_Y + 24;
}

static void update_camera(struct game_state *game)
{
    int world_x = 0;
    int world_y = 0;
    int room_w = 0;
    int room_h = 0;
    if (game->room_id == ROOM_MEADOW)
    {
        world_x = MEADOW_WORLD_X;
        world_y = MEADOW_WORLD_Y;
        room_w = MEADOW_ROOM_W;
        room_h = MEADOW_ROOM_H;
    }
    else if (game->room_id == ROOM_CREEK)
    {
        world_x = CREEK_WORLD_X;
        world_y = CREEK_WORLD_Y;
        room_w = CREEK_ROOM_W;
        room_h = CREEK_ROOM_H;
    }
    else
    {
        game->camera_x = 0;
        game->camera_y = 0;
        return;
    }

    int view_top = ROOM_TOP + 18;
    int view_w = ROOM_RIGHT - ROOM_LEFT;
    int view_h = ROOM_BOTTOM - view_top;

    int target_x = game->x + (PLAYER_W / 2) - (view_w / 2);
    int target_y = game->y + (PLAYER_H / 2) - (view_h / 2);

    int cam_min_x = world_x;
    int cam_max_x = world_x + room_w - view_w;
    int cam_min_y = world_y;
    int cam_max_y = world_y + room_h - view_h;

    if (cam_max_x < cam_min_x) cam_max_x = cam_min_x;
    if (cam_max_y < cam_min_y) cam_max_y = cam_min_y;

    if (target_x < cam_min_x) target_x = cam_min_x;
    if (target_x > cam_max_x) target_x = cam_max_x;
    if (target_y < cam_min_y) target_y = cam_min_y;
    if (target_y > cam_max_y) target_y = cam_max_y;

    game->camera_x = target_x;
    game->camera_y = target_y;
}

static void normalize_meadow_spawn(struct game_state *game)
{
    if (game->room_id != ROOM_MEADOW) return;

    resolve_meadow_spawn(game);
}

static const struct room_def room_defs[ROOM_COUNT] = {
    {
        THEME_MEADOW, 3, {{MEADOW_WORLD_X + 90, 226}, {MEADOW_WORLD_X + 262, 236}, {MEADOW_WORLD_X + 506, 318}},
        2, {{MEADOW_WORLD_X + 144, 256, AXIS_HORIZONTAL, 34}, {MEADOW_WORLD_X + 552, 334, AXIS_VERTICAL, 42}},
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
        THEME_SHRINE, 1, {{120, 80}, {0,0}, {0,0}}, 0, {{0,0,0,0}, {0,0,0,0}},
        4, {{0, 0, 240, 8}, {0, 0, 8, 160}, {232, 0, 8, 160}, {0, 0, 240, 8}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}},
        false, {0,0,0,0}, false, {0,0,0,0}, 120, 120,
        {{-1,0,0}, {-1,0,0}, {-1,0,0}, {ROOM_MEADOW, TMC_TO_PLAYER_X(SOUTH_HYRULE_MINISH_CAVE_RETURN_X), TMC_TO_PLAYER_Y(SOUTH_HYRULE_MINISH_CAVE_RETURN_Y)}},
    },
    {
        THEME_HOUSE, 0, {{0,0}, {0,0}, {0,0}}, 0, {{0,0,0,0}, {0,0,0,0}},
        4, {{0, 0, 240, 8}, {0, 0, 8, 160}, {232, 0, 8, 160}, {0, 150, 240, 10}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}},
        false, {0,0,0,0}, false, {0,0,0,0}, BEDROOM_START_X, BEDROOM_START_Y,
        {{ROOM_LINKS_HOUSE_ENTRANCE, 12, 86}, {-1,0,0}, {-1,0,0}, {-1,0,0}},
    },
    {
        THEME_HOUSE, 0, {{0,0}, {0,0}, {0,0}}, 0, {{0,0,0,0}, {0,0,0,0}},
        6, {{0, 0, 240, 8}, {0, 0, 8, 160}, {232, 0, 8, HOUSE_DOOR_Y}, {232, HOUSE_DOOR_Y + HOUSE_DOOR_H, 8, 160 - (HOUSE_DOOR_Y + HOUSE_DOOR_H)}, {0, 152, 86, 8}, {154, 152, 86, 8}, {0,0,0,0}, {0,0,0,0}},
        false, {0,0,0,0}, false, {0,0,0,0}, 12, 86,
        {{ROOM_MEADOW, HOUSE_EXIT_SAFE_X, HOUSE_EXIT_SAFE_Y}, {-1,0,0}, {ROOM_LINKS_HOUSE_BEDROOM, BEDROOM_START_X, BEDROOM_START_Y}, {ROOM_LINKS_HOUSE_SMITH, 154, 86}},
    },
    {
        THEME_HOUSE, 0, {{0,0}, {0,0}, {0,0}}, 0, {{0,0,0,0}, {0,0,0,0}},
        5, {{0, 0, 240, 8}, {0, 0, 8, HOUSE_DOOR_Y}, {0, HOUSE_DOOR_Y + HOUSE_DOOR_H, 8, 160 - (HOUSE_DOOR_Y + HOUSE_DOOR_H)}, {232, 0, 8, 160}, {0, 150, 240, 10}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}},
        false, {0,0,0,0}, false, {0,0,0,0}, 148, 110,
        {{-1,0,0}, {-1,0,0}, {ROOM_LINKS_HOUSE_ENTRANCE, 12, 86}, {-1,0,0}},
    },
};

static const struct room_def *current_room(const struct game_state *game) { return &room_defs[game->room_id]; }
static bool room_uses_world_map(const struct game_state *game)
{
    return game->room_id == ROOM_MEADOW || game->room_id == ROOM_CREEK;
}

static void get_room_bounds(const struct game_state *game, int *left, int *top, int *right, int *bottom)
{
    if (game->room_id == ROOM_MEADOW)
    {
        *left = MEADOW_WORLD_X;
        *top = MEADOW_WORLD_Y;
        *right = MEADOW_WORLD_X + MEADOW_ROOM_W;
        *bottom = MEADOW_WORLD_Y + MEADOW_ROOM_H;
    }
    else if (game->room_id == ROOM_CREEK)
    {
        *left = CREEK_WORLD_X;
        *top = CREEK_WORLD_Y;
        *right = CREEK_WORLD_X + CREEK_ROOM_W;
        *bottom = CREEK_WORLD_Y + CREEK_ROOM_H;
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
    if (room_uses_world_map(game)) return x + ROOM_LEFT - game->camera_x;
    return INTERIOR_ORIGIN_X + x;
}

static int world_to_screen_y(const struct game_state *game, int y)
{
    if (room_uses_world_map(game)) return ROOM_TOP + 18 + y - game->camera_y;
    return INTERIOR_ORIGIN_Y + y;
}

static void set_fg(unsigned color) { rb->lcd_set_foreground(color); }

static void draw_house_background(const fb_data *src, int w, int h)
{
    if (w == 240 && h == 160) rb->lcd_bitmap(src, INTERIOR_ORIGIN_X, INTERIOR_ORIGIN_Y, w, h);
}

static void draw_meadow_background(const struct game_state *game)
{
    int view_x = ROOM_LEFT;
    int view_y = ROOM_TOP + 18;
    int view_w = ROOM_RIGHT - ROOM_LEFT;
    int view_h = ROOM_BOTTOM - view_y;
    int draw_x = ROOM_LEFT + MEADOW_WORLD_X - game->camera_x;
    int draw_y = ROOM_TOP + 18 + MEADOW_WORLD_Y - game->camera_y;

    set_fg(COLOR_ROOM_FLOOR);
    rb->lcd_fillrect(view_x, view_y, view_w, view_h);

    if (south_hyrule_bmp_loaded)
        rb->lcd_bitmap(south_hyrule_bmp_data, draw_x, draw_y, MEADOW_ROOM_W, MEADOW_ROOM_H);
    else
    {
        int start_col = game->camera_x / MEADOW_CHUNK_W;
        int end_col = (game->camera_x + view_w - 1) / MEADOW_CHUNK_W;
        int start_row = game->camera_y / MEADOW_CHUNK_H;
        int end_row = (game->camera_y + view_h - 1) / MEADOW_CHUNK_H;

        if (start_col < 0) start_col = 0;
        if (start_row < 0) start_row = 0;
        if (end_col >= MEADOW_CHUNK_COLS) end_col = MEADOW_CHUNK_COLS - 1;
        if (end_row >= MEADOW_CHUNK_ROWS) end_row = MEADOW_CHUNK_ROWS - 1;

        for (int row = start_row; row <= end_row; ++row)
        {
            for (int col = start_col; col <= end_col; ++col)
            {
                int chunk_x = ROOM_LEFT + col * MEADOW_CHUNK_W - game->camera_x;
                int chunk_y = ROOM_TOP + 18 + row * MEADOW_CHUNK_H - game->camera_y;

                if (ensure_meadow_chunk_loaded(row, col))
                {
                    int slot = meadow_chunk_slot_map[row][col];
                    if (slot >= 0)
                    {
                        rb->lcd_bitmap(meadow_chunk_storage[slot], chunk_x, chunk_y,
                                       MEADOW_CHUNK_W, MEADOW_CHUNK_H);
                        continue;
                    }
                }

                rb->lcd_bitmap(minishcap_room_meadow_real, chunk_x, chunk_y,
                               LCD_WIDTH, CREEK_ROOM_H);
            }
        }
    }
}

static void draw_creek_background(const struct game_state *game)
{
    int draw_x = ROOM_LEFT + CREEK_WORLD_X - game->camera_x;
    int draw_y = ROOM_TOP + 18 + CREEK_WORLD_Y - game->camera_y;
    rb->lcd_bitmap(minishcap_room_creek_real, draw_x, draw_y, CREEK_ROOM_W, CREEK_ROOM_H);
}

static void draw_minish_cave_background(void)
{
    int x = INTERIOR_ORIGIN_X;
    int y = INTERIOR_ORIGIN_Y;
    set_fg(0x0000);
    rb->lcd_fillrect(x, y, 240, 160);
    set_fg(0x2104);
    rb->lcd_fillrect(x + 8, y + 8, 224, 128);
    set_fg(0x39E7);
    rb->lcd_fillrect(x + 92, y + 120, 56, 30);
    set_fg(COLOR_UI_TEXT);
    rb->lcd_putsxy(x + 78, y + 18, "MINISH CAVE");
}

static void draw_room_background(const struct game_state *game)
{
    if (game->room_id == ROOM_LINKS_HOUSE_BEDROOM)
        draw_house_background(minishcap_room_links_house_bedroom, 240, 160);
    else if (game->room_id == ROOM_LINKS_HOUSE_ENTRANCE)
        draw_house_background(minishcap_room_links_house_entrance, 240, 160);
    else if (game->room_id == ROOM_LINKS_HOUSE_SMITH)
        draw_house_background(minishcap_room_links_house_smith, 240, 160);
    else if (game->room_id == ROOM_MINISH_CAVE)
        draw_minish_cave_background();
    else if (game->room_id == ROOM_CREEK)
        draw_creek_background(game);
    else
        draw_meadow_background(game);
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
    int hearts_x = TMC_UI_X_OFF + 10;
    int hearts_y = 6;
    int rupee_x = TMC_UI_X_OFF + 164;
    int rupee_y = 7;
    int b_x = TMC_UI_BTN_B_X;
    int a_x = TMC_UI_BTN_A_X;
    int r_x = TMC_UI_BTN_R_X;
    int b_y = TMC_UI_BTN_AB_Y;
    int a_y = TMC_UI_BTN_AB_Y;
    int r_y = TMC_UI_BTN_R_Y;

    set_fg(COLOR_UI_BG);
    rb->lcd_fillrect(hearts_x - 4, hearts_y - 2, 86, 16);
    rb->lcd_fillrect(rupee_x - 4, rupee_y - 2, 44, 14);
    rb->lcd_fillrect(b_x - 2, b_y - 2, 22, 18);
    rb->lcd_fillrect(a_x - 2, a_y - 2, 22, 18);
    rb->lcd_fillrect(r_x - 2, r_y - 2, 18, 14);
    set_fg(COLOR_UI_FRAME);
    rb->lcd_drawrect(hearts_x - 4, hearts_y - 2, 86, 16);
    rb->lcd_drawrect(rupee_x - 4, rupee_y - 2, 44, 14);
    rb->lcd_drawrect(b_x - 2, b_y - 2, 22, 18);
    rb->lcd_drawrect(a_x - 2, a_y - 2, 22, 18);
    rb->lcd_drawrect(r_x - 2, r_y - 2, 18, 14);

    for (int i = 0; i < PLAYER_MAX_HEALTH; ++i)
        draw_hud_heart(hearts_x + i * 15, hearts_y, i < game->health);

    char line[16];
    rb->snprintf(line, sizeof(line), "%03d", game->rupees);
    set_fg(COLOR_UI_TEXT);
    rb->lcd_putsxy(rupee_x, rupee_y, "R");
    rb->lcd_putsxy(rupee_x + 10, rupee_y, line);
    rb->lcd_putsxy(b_x + 5, b_y, "B");
    rb->lcd_putsxy(a_x + 5, a_y, "A");
    rb->lcd_putsxy(r_x + 5, r_y - 1, "R");

    if (game->sword_equipped)
    {
        int sx = b_x + 7;
        int sy = b_y + 8;
        set_fg(COLOR_SWORD);
        rb->lcd_fillrect(sx, sy, 2, 7);
        rb->lcd_fillrect(sx - 2, sy + 5, 6, 2);
    }
    set_fg(COLOR_UI_TEXT);
    rb->lcd_fillrect(a_x + 6, a_y + 8, 4, 4);

    if (asset_warning_mask != ASSET_WARN_NONE)
    {
        set_fg(COLOR_HEART);
        rb->lcd_putsxy(TMC_UI_X_OFF + 96, 7, "!");
    }
}

static void draw_masked_bitmap(const fb_data *src, int w, int h, int x, int y, fb_data key)
{
    for (int py = 0; py < h; ++py)
    {
        for (int px = 0; px < w; ++px)
        {
            fb_data c = src[py * w + px];
            if (c != key)
            {
                rb->lcd_set_foreground(c);
                rb->lcd_drawpixel(x + px, y + py);
            }
        }
    }
}

static void draw_player(const struct game_state *game)
{
#if LINK_USE_BITMAP_SPRITES
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
    draw_masked_bitmap(sprite, sprite_w, sprite_h, draw_x, draw_y, LINK_TRANSPARENT_KEY);
    
    if (game->sword_equipped && game->sword_timer > 0)
    {
        int base_x = draw_x + 7;
        int base_y = draw_y + 7;
        set_fg(COLOR_SWORD);
        switch (game->sword_dir)
        {
            case FACE_UP:
                rb->lcd_fillrect(base_x - 1, base_y - 12, 3, 9);
                rb->lcd_fillrect(base_x - 3, base_y - 2, 7, 2);
                break;
            case FACE_LEFT:
                rb->lcd_fillrect(base_x - 12, base_y - 1, 9, 3);
                rb->lcd_fillrect(base_x - 2, base_y - 3, 2, 7);
                break;
            case FACE_RIGHT:
                rb->lcd_fillrect(base_x + 3, base_y - 1, 9, 3);
                rb->lcd_fillrect(base_x + 1, base_y - 3, 2, 7);
                break;
            default:
                rb->lcd_fillrect(base_x - 1, base_y + 3, 3, 9);
                rb->lcd_fillrect(base_x - 3, base_y + 2, 7, 2);
                break;
        }
    }
#endif
}

static void draw_npcs(const struct game_state *game)
{
    if (game->room_id == ROOM_LINKS_HOUSE_SMITH)
    {
        int smith_x = game->sword_equipped ? SMITH_POST_X : SMITH_INTRO_X;
        int smith_y = game->sword_equipped ? SMITH_POST_Y : SMITH_INTRO_Y;

        if (!game->sword_equipped)
            draw_zelda_at(world_to_screen_x(game, ZELDA_SMITH_X), world_to_screen_y(game, ZELDA_SMITH_Y));

        draw_smith_at(world_to_screen_x(game, smith_x), world_to_screen_y(game, smith_y));

        if (!game->sword_equipped)
        {
            int sx = world_to_screen_x(game, SWORD_PICKUP_X);
            int sy = world_to_screen_y(game, SWORD_PICKUP_Y);
            set_fg(COLOR_SWORD);
            rb->lcd_fillrect(sx, sy, 8, 16);
            rb->lcd_fillrect(sx + 6, sy + 2, 4, 12);
        }
    }
}

static void draw_dialog(const struct game_state *game)
{
    if (game->message_timer <= 0 || game->line1[0] == '\0')
    {
        int warn_ticks = 0;
        const char *warn = asset_warning_text(&warn_ticks);
        if (warn != NULL)
        {
            int box_x = 8;
            int box_w = LCD_WIDTH - 16;
            int box_h = 24;
            int box_y = LCD_HEIGHT - box_h - 6;

            (void)warn_ticks;
            set_fg(COLOR_UI_BG);
            rb->lcd_fillrect(box_x, box_y, box_w, box_h);
            set_fg(COLOR_UI_FRAME);
            rb->lcd_drawrect(box_x, box_y, box_w, box_h);
            set_fg(COLOR_UI_TEXT);
            rb->lcd_putsxy(box_x + 6, box_y + 8, warn);
        }
        return;
    }

    int box_x = 8;
    int box_w = LCD_WIDTH - 16;
    int box_h = 24;
    int box_y = LCD_HEIGHT - box_h - 6;

    set_fg(COLOR_UI_BG);
    rb->lcd_fillrect(box_x, box_y, box_w, box_h);
    set_fg(COLOR_UI_FRAME);
    rb->lcd_drawrect(box_x, box_y, box_w, box_h);
    set_fg(COLOR_UI_TEXT);
    rb->lcd_putsxy(box_x + 6, box_y + 8, game->line1);
}

static void draw_debug_overlay(const struct game_state *game)
{
    if (!game->debug_overlay)
        return;

    int y = LCD_HEIGHT - 56;
    int w = LCD_WIDTH - 16;
    int h = 50;
    char line[64];
    int stair_hit = 0;
    int front_hit = 0;
    int smith_hit = 0;

    if (game->room_id == ROOM_LINKS_HOUSE_BEDROOM)
    {
        stair_hit = rect_overlap(game->x, game->y, PLAYER_W, PLAYER_H, 0x58, 0x18, 12, 12) ? 1 : 0;
    }
    else if (game->room_id == ROOM_LINKS_HOUSE_ENTRANCE)
    {
        stair_hit = rect_overlap(game->x, game->y, PLAYER_W, PLAYER_H, 0x58, 0x18, 12, 12) ? 1 : 0;
        front_hit = rect_overlap(game->x, game->y, PLAYER_W, PLAYER_H,
                                 HOUSE_FRONT_EXIT_X, HOUSE_FRONT_EXIT_Y,
                                 HOUSE_FRONT_EXIT_W, HOUSE_FRONT_EXIT_H) ? 1 : 0;
        smith_hit = rect_overlap(game->x, game->y, PLAYER_W, PLAYER_H,
                                 224, HOUSE_DOOR_Y, 16, HOUSE_DOOR_H) ? 1 : 0;
    }
    else if (game->room_id == ROOM_LINKS_HOUSE_SMITH)
    {
        smith_hit = rect_overlap(game->x, game->y, PLAYER_W, PLAYER_H,
                                 0, HOUSE_DOOR_Y, 8, HOUSE_DOOR_H) ? 1 : 0;
    }

    if (game->room_id == ROOM_LINKS_HOUSE_BEDROOM)
    {
        set_fg(0x07E0);
        rb->lcd_drawrect(world_to_screen_x(game, 0x58), world_to_screen_y(game, 0x18), 12, 12);
    }
    else if (game->room_id == ROOM_LINKS_HOUSE_ENTRANCE)
    {
        set_fg(0x07E0);
        rb->lcd_drawrect(world_to_screen_x(game, 0x58), world_to_screen_y(game, 0x18), 12, 12);
        set_fg(0x001F);
        rb->lcd_drawrect(world_to_screen_x(game, HOUSE_FRONT_EXIT_X),
                         world_to_screen_y(game, HOUSE_FRONT_EXIT_Y),
                         HOUSE_FRONT_EXIT_W, HOUSE_FRONT_EXIT_H);
        set_fg(0xFFE0);
        rb->lcd_drawrect(world_to_screen_x(game, 224),
                         world_to_screen_y(game, HOUSE_DOOR_Y),
                         16, HOUSE_DOOR_H);
    }
    else if (game->room_id == ROOM_LINKS_HOUSE_SMITH)
    {
        set_fg(0xFFE0);
        rb->lcd_drawrect(world_to_screen_x(game, 0), world_to_screen_y(game, HOUSE_DOOR_Y),
                         8, HOUSE_DOOR_H);
        set_fg(0xF81F);
        rb->lcd_drawrect(world_to_screen_x(game, SWORD_PICKUP_X),
                         world_to_screen_y(game, SWORD_PICKUP_Y),
                         SWORD_PICKUP_W, SWORD_PICKUP_H);
    }

    set_fg(COLOR_UI_BG);
    rb->lcd_fillrect(8, y, w, h);
    set_fg(COLOR_UI_FRAME);
    rb->lcd_drawrect(8, y, w, h);
    set_fg(COLOR_UI_TEXT);

    rb->snprintf(line, sizeof(line), "DBG room=%d x=%d y=%d", game->room_id, game->x, game->y);
    rb->lcd_putsxy(12, y + 4, line);
    rb->snprintf(line, sizeof(line), "stair=%d front=%d smith=%d sword=%d", stair_hit,
                 front_hit, smith_hit, game->sword_equipped ? 1 : 0);
    rb->lcd_putsxy(12, y + 18, line);
    rb->snprintf(line, sizeof(line), "cmd=%d event=%d", game->debug_last_cmd, game->debug_last_event);
    rb->lcd_putsxy(12, y + 32, line);
}

static void draw_menu_overlay(const struct game_state *game)
{
    if (!game->menu_open) return;

    int x = 44, y = 36, w = LCD_WIDTH - 88, h = LCD_HEIGHT - 72;
    set_fg(COLOR_UI_BG);
    rb->lcd_fillrect(x, y, w, h);
    set_fg(COLOR_UI_FRAME);
    rb->lcd_drawrect(x, y, w, h);
    set_fg(COLOR_UI_TEXT);
    rb->lcd_putsxy(x + 10, y + 8, "MINISH CAP MENU");

    const char *tabs[MENU_TAB_COUNT] = {"STATUS", "ITEMS", "QUEST", "MAP"};
    for (int i = 0; i < MENU_TAB_COUNT; ++i)
    {
        int tx = x + 8 + (i * 52);
        if (i == game->menu_tab)
        {
            set_fg(COLOR_UI_FRAME);
            rb->lcd_fillrect(tx - 2, y + 24, 48, 12);
            set_fg(COLOR_UI_TEXT);
        }
        rb->lcd_putsxy(tx, y + 26, tabs[i]);
    }

    if (game->menu_tab == MENU_TAB_STATUS)
    {
        char hp[24];
        char rup[24];
        rb->snprintf(hp, sizeof(hp), "Hearts: %d/%d", game->health, PLAYER_MAX_HEALTH);
        rb->snprintf(rup, sizeof(rup), "Rupees: %d", game->rupees);
        rb->lcd_putsxy(x + 12, y + 52, hp);
        rb->lcd_putsxy(x + 12, y + 66, rup);
        rb->lcd_putsxy(x + 12, y + 82, game->room_id == ROOM_MEADOW ? "Area: South Hyrule" : "Area: Interior");
    }
    else if (game->menu_tab == MENU_TAB_ITEMS)
    {
        rb->lcd_putsxy(x + 12, y + 52, game->sword_equipped ? "B item: Smith Sword" : "B item: Empty");
        rb->lcd_putsxy(x + 12, y + 66, "A item: Context");
        rb->lcd_putsxy(x + 12, y + 80, "Select: Attack/Talk");
    }
    else if (game->menu_tab == MENU_TAB_QUEST)
    {
        rb->lcd_putsxy(x + 12, y + 52, game->sword_equipped ? "Quest: Leave house" : "Quest: Get sword");
        rb->lcd_putsxy(x + 12, y + 66, "Talk: Select near NPC");
        rb->lcd_putsxy(x + 12, y + 80, "Menu: Select+Play");
    }
    else
    {
        rb->lcd_putsxy(x + 12, y + 52, "South Hyrule Warps:");
        rb->lcd_putsxy(x + 12, y + 66, "0x118,0x0a8 Fairy Cave");
        rb->lcd_putsxy(x + 12, y + 80, "0x178,0x0d8 Minish Cave");
    }

    rb->lcd_putsxy(x + 12, y + h - 18, "Select: Close  Left/Right: Tabs");
}

static void draw_screen(const struct game_state *game)
{
    set_fg(COLOR_ROOM_WALL);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    draw_room_background(game);
    draw_npcs(game);
    draw_player(game);
    draw_hud(game);
    draw_dialog(game);
    draw_debug_overlay(game);
    draw_menu_overlay(game);
    rb->lcd_update();
}

static void begin_room(struct game_state *game, int room_id, int x, int y)
{
    int request_x = x;
    int request_y = y;

    if (room_id < 0 || room_id >= ROOM_COUNT) return;
    game->room_id = room_id;
    game->x = x;
    game->y = y;
    game->room_timer = 0;
    game->house_exit_assist = 0;
    game->house_reentry_cooldown = (room_id == ROOM_MEADOW) ? (HZ / 3) : 0;
    game->enemy_count = 0;
    game->npc_count = 0;
    game->pickup_mask = 0;
    game->line1[0] = '\0';
    game->line2[0] = '\0';
    game->line3[0] = '\0';
    game->message_timer = 0;
    game->bgm_cooldown = 0;
    game->interaction_cooldown = 10;
    bgm_profile_logged = -1;
    bgm_note_idx = 0;
    bgm_note_timer = 0;
    normalize_meadow_spawn(game);
    update_camera(game);

    DEBUGF("minishcap begin_room room=%d request=(%d,%d) spawn=(%d,%d) camera=(%d,%d)\n",
           room_id, request_x, request_y, game->x, game->y,
           game->camera_x, game->camera_y);
    trace_log("minishcap begin_room room=%d request=(%d,%d) spawn=(%d,%d) camera=(%d,%d)",
              room_id, request_x, request_y, game->x, game->y,
              game->camera_x, game->camera_y);
}

static void init_default_game(struct game_state *game)
{
    memset(game, 0, sizeof(*game));
    game->health = PLAYER_MAX_HEALTH;
    game->facing = FACE_DOWN;
    game->sword_dir = FACE_DOWN;
    game->sword_equipped = false;
    game->quest_stage = 1;
    game->menu_open = false;
    game->menu_tab = MENU_TAB_STATUS;
    game->debug_overlay = MINISHCAP_DEBUG_OVERLAY_DEFAULT ? true : false;
    game->debug_last_cmd = 0;
    game->debug_last_event = 0;
    game->interaction_cooldown = HZ / 10;
    begin_room(game, ROOM_LINKS_HOUSE_BEDROOM, BEDROOM_START_X, BEDROOM_START_Y);
    game->x = BEDROOM_START_X;
    game->y = BEDROOM_START_Y;
}

static int decode_cmd(int button)
{
    int cmd = 0;
    if (button & BUTTON_LEFT) cmd |= CMD_LEFT;
    if (button & BUTTON_RIGHT) cmd |= CMD_RIGHT;
#if defined(SIMULATOR) && defined(BUTTON_SCROLL_BACK)
    if (button & (BUTTON_MENU | BUTTON_SCROLL_BACK)) cmd |= CMD_UP;
#else
    if (button & BUTTON_MENU) cmd |= CMD_UP;
#endif
#if defined(SIMULATOR) && defined(BUTTON_SCROLL_FWD)
    if (button & (BUTTON_PLAY | BUTTON_SCROLL_FWD)) cmd |= CMD_DOWN;
#else
    if (button & BUTTON_PLAY) cmd |= CMD_DOWN;
#endif
    return cmd;
}

static int clean_button(long button)
{
    int mask = BUTTON_LEFT | BUTTON_RIGHT | BUTTON_MENU | BUTTON_PLAY | BUTTON_SELECT;
#if defined(SIMULATOR) && defined(BUTTON_SCROLL_BACK)
    mask |= BUTTON_SCROLL_BACK;
#endif
#if defined(SIMULATOR) && defined(BUTTON_SCROLL_FWD)
    mask |= BUTTON_SCROLL_FWD;
#endif
    return button & mask;
}

static void apply_one_shot_controls(struct game_state *game, int *cmd, int event)
{
    if (game->menu_open)
        return;

    if (event & BUTTON_LEFT)
        *cmd = CMD_LEFT;
    else if (event & BUTTON_RIGHT)
        *cmd = CMD_RIGHT;

    if (event & BUTTON_MENU)
        *cmd = CMD_UP;
    else if (event & BUTTON_PLAY)
        *cmd = CMD_DOWN;

#if defined(SIMULATOR) && defined(BUTTON_SCROLL_BACK)
    if (event & BUTTON_SCROLL_BACK)
        *cmd = CMD_UP;
#endif
#if defined(SIMULATOR) && defined(BUTTON_SCROLL_FWD)
    if (event & BUTTON_SCROLL_FWD)
        *cmd = CMD_DOWN;
#endif
}

static bool rect_overlap(int x, int y, int w, int h, int rx, int ry, int rw, int rh);

static bool try_transition(struct game_state *game, int edge)
{
    const struct room_transition *exit = &current_room(game)->exits[edge];
    if (exit->room < 0) return false;

    if (exit->room == ROOM_MEADOW && !game->sword_equipped)
    {
        if (!rect_overlap(game->x, game->y, PLAYER_W, PLAYER_H, HOUSE_EXIT_LOCK_X, HOUSE_EXIT_LOCK_Y, HOUSE_EXIT_LOCK_W, HOUSE_EXIT_LOCK_H))
            return false;
        set_message(game, "Need Smith sword.", "", "", HZ);
        play_sfx(game, 220, 80, 1200);
        return false;
    }

    play_sfx(game, 510, 65, 1200);
    begin_transition(game, "edge", edge, exit->room, exit->x, exit->y);
    return true;
}

static bool rect_overlap(int x, int y, int w, int h, int rx, int ry, int rw, int rh)
{
    return x < rx + rw && x + w > rx && y < ry + rh && y + h > ry;
}

static bool try_area_transition(struct game_state *game, int test_x, int test_y)
{
    if (game->room_id == ROOM_MEADOW) {
        if (game->house_reentry_cooldown == 0 &&
            rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H,
                         MEADOW_HOUSE_ENTRY_X,
                         MEADOW_HOUSE_ENTRY_Y,
                         MEADOW_HOUSE_ENTRY_W,
                         MEADOW_HOUSE_ENTRY_H)) {
            begin_transition(game, "warp", TRANSITION_MEADOW_TO_ENTRANCE,
                             ROOM_LINKS_HOUSE_ENTRANCE,
                             ENTRANCE_FROM_MEADOW_X,
                             ENTRANCE_FROM_MEADOW_Y);
            return true;
        }
        if (rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H,
                         TMC_TO_PLAYER_X(SOUTH_HYRULE_FAIRY_CAVE_X) - SOUTH_HYRULE_POI_TRIGGER_HALF,
                         TMC_TO_PLAYER_Y(SOUTH_HYRULE_FAIRY_CAVE_Y) - SOUTH_HYRULE_POI_TRIGGER_HALF,
                         SOUTH_HYRULE_POI_TRIGGER_HALF * 2,
                         SOUTH_HYRULE_POI_TRIGGER_HALF * 2)) {
            play_sfx(game, 510, 65, 1200);
            begin_transition(game, "warp", TRANSITION_FAIRY_CAVE,
                             ROOM_SHRINE, 148, 164);
            set_message(game, "Fairy Cave", "", "", HZ / 2);
            return true;
        }
        if (rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H,
                         TMC_TO_PLAYER_X(SOUTH_HYRULE_MINISH_CAVE_X) - SOUTH_HYRULE_POI_TRIGGER_HALF,
                         TMC_TO_PLAYER_Y(SOUTH_HYRULE_MINISH_CAVE_Y) - SOUTH_HYRULE_POI_TRIGGER_HALF,
                         SOUTH_HYRULE_POI_TRIGGER_HALF * 2,
                         SOUTH_HYRULE_POI_TRIGGER_HALF * 2)) {
            play_sfx(game, 510, 65, 1200);
            begin_transition(game, "warp", TRANSITION_MINISH_CAVE,
                             ROOM_MINISH_CAVE, 120, 122);
            set_message(game, "Minish Passage", "", "", HZ / 2);
            return true;
        }
        if (rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H,
                         TMC_TO_PLAYER_X(SOUTH_HYRULE_TREE_HOLE_X) - SOUTH_HYRULE_POI_TRIGGER_HALF,
                         TMC_TO_PLAYER_Y(SOUTH_HYRULE_TREE_HOLE_Y) - SOUTH_HYRULE_POI_TRIGGER_HALF,
                         SOUTH_HYRULE_POI_TRIGGER_HALF * 2,
                         SOUTH_HYRULE_POI_TRIGGER_HALF * 2)) {
            play_sfx(game, 510, 65, 1200);
            begin_transition(game, "warp", TRANSITION_TREE_HOLLOW,
                             ROOM_CREEK, 18, 156);
            set_message(game, "Tree Hollow", "", "", HZ / 2);
            return true;
        }
        if (test_x + PLAYER_W >= MEADOW_WORLD_X + MEADOW_ROOM_W - 1) {
            begin_transition(game, "warp", TRANSITION_MEADOW_EDGE_TO_CREEK,
                             ROOM_CREEK, 18, 156);
            set_message(game, "Minish Creek", "", "", HZ / 2);
            return true;
        }
        if (rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H, MEADOW_TO_CREEK_X, MEADOW_TO_CREEK_Y, MEADOW_TO_CREEK_W, MEADOW_TO_CREEK_H)) {
            begin_transition(game, "warp", TRANSITION_MEADOW_ZONE_TO_CREEK,
                             ROOM_CREEK, 18, 156);
            set_message(game, "Minish Creek", "", "", HZ / 2);
            return true;
        }
    } else if (game->room_id == ROOM_CREEK) {
        if (test_x <= CREEK_WORLD_X + 1) {
            begin_transition(game, "warp", TRANSITION_CREEK_EDGE_TO_MEADOW,
                             ROOM_MEADOW,
                             MEADOW_WORLD_X + MEADOW_ROOM_W - 30,
                             HOUSE_EXIT_SAFE_Y + 6);
            set_message(game, "South Hyrule", "", "", HZ / 2);
            return true;
        }
        if (rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H, CREEK_TO_MEADOW_X, CREEK_TO_MEADOW_Y, CREEK_TO_MEADOW_W, CREEK_TO_MEADOW_H)) {
            begin_transition(game, "warp", TRANSITION_CREEK_ZONE_TO_MEADOW,
                             ROOM_MEADOW,
                             MEADOW_WORLD_X + MEADOW_ROOM_W - 30,
                             HOUSE_EXIT_SAFE_Y + 6);
            set_message(game, "South Hyrule", "", "", HZ / 2);
            return true;
        }
    } else if (game->room_id == ROOM_SHRINE) {
        if (test_y + PLAYER_H >= INTERIOR_ROOM_H - 1) {
            play_sfx(game, 510, 65, 1200);
            begin_transition(game, "warp", TRANSITION_SHRINE_EXIT,
                             ROOM_MEADOW,
                             TMC_TO_PLAYER_X(SOUTH_HYRULE_FAIRY_CAVE_X),
                             TMC_TO_PLAYER_Y(SOUTH_HYRULE_FAIRY_CAVE_Y + 0x10));
            set_message(game, "South Hyrule", "", "", HZ / 2);
            return true;
        }
    } else if (game->room_id == ROOM_MINISH_CAVE) {
        if (test_y + PLAYER_H >= INTERIOR_ROOM_H - 1) {
            play_sfx(game, 510, 65, 1200);
            begin_transition(game, "warp", TRANSITION_MINISH_CAVE_EXIT,
                             ROOM_MEADOW,
                             TMC_TO_PLAYER_X(SOUTH_HYRULE_MINISH_CAVE_RETURN_X),
                             TMC_TO_PLAYER_Y(SOUTH_HYRULE_MINISH_CAVE_RETURN_Y));
            set_message(game, "South Hyrule", "", "", HZ / 2);
            return true;
        }
    } else if (game->room_id == ROOM_LINKS_HOUSE_BEDROOM) {
        if (rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H, 0x58, 0x18, 12, 12)) {
            begin_transition(game, "warp", TRANSITION_BEDROOM_TO_ENTRANCE,
                             ROOM_LINKS_HOUSE_ENTRANCE, 12, 86);
            return true;
        }
    } else if (game->room_id == ROOM_LINKS_HOUSE_ENTRANCE) {
        if (rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H, 0x58, 0x18, 12, 12)) {
            begin_transition(game, "warp", TRANSITION_ENTRANCE_TO_BEDROOM,
                             ROOM_LINKS_HOUSE_BEDROOM,
                             BEDROOM_START_X,
                             BEDROOM_START_Y);
            return true;
        }
        if (rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H, HOUSE_FRONT_EXIT_X, HOUSE_FRONT_EXIT_Y, HOUSE_FRONT_EXIT_W, HOUSE_FRONT_EXIT_H)) {
            if (game->sword_equipped) {
                trace_log("minishcap front_exit room=%d pos=(%d,%d)", game->room_id, test_x, test_y);
                begin_transition(game, "warp", TRANSITION_ENTRANCE_TO_MEADOW,
                                 ROOM_MEADOW,
                                 HOUSE_EXIT_SAFE_X,
                                 HOUSE_EXIT_SAFE_Y);
                return true;
            }
            set_message(game, "Need Smith sword.", "", "", HZ);
            return false;
        }
        if (rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H, 224, HOUSE_DOOR_Y, 16, HOUSE_DOOR_H)) {
            begin_transition(game, "warp", TRANSITION_ENTRANCE_TO_SMITH,
                             ROOM_LINKS_HOUSE_SMITH, 18, 92);
            return true;
        }
    } else if (game->room_id == ROOM_LINKS_HOUSE_SMITH) {
        if (rect_overlap(test_x, test_y, PLAYER_W, PLAYER_H, 0, HOUSE_DOOR_Y, 8, HOUSE_DOOR_H)) {
            begin_transition(game, "warp", TRANSITION_SMITH_TO_ENTRANCE,
                             ROOM_LINKS_HOUSE_ENTRANCE, 206, 92);
            return true;
        }
    }

    return false;
}

static bool try_sword_pickup(struct game_state *game)
{
    if (game->sword_equipped) return false;
    if (game->room_id != ROOM_LINKS_HOUSE_SMITH) return false;

    if (rect_overlap(game->x, game->y, PLAYER_W, PLAYER_H, SWORD_PICKUP_X, SWORD_PICKUP_Y, SWORD_PICKUP_W, SWORD_PICKUP_H))
    {
        trace_log("minishcap sword_pickup room=%d pos=(%d,%d)", game->room_id, game->x, game->y);
        game->sword_equipped = true;
        game->quest_stage = 2;
        set_message(game, "Got Smith's Sword!", "", "", HZ * 2);
        play_sfx(game, 880, 140, 1200);
        return true;
    }

    return false;
}

static void update_game(struct game_state *game, int cmd, int event)
{
#if MINISHCAP_TRACE_MOVEMENT
    int prev_room = game->room_id;
    int prev_x = game->x;
    int prev_y = game->y;
#endif

    apply_one_shot_controls(game, &cmd, event);
    game->debug_last_cmd = cmd;
    game->debug_last_event = event;

    bool select_used = try_select_interaction(game, event);

    if ((event & BUTTON_SELECT) && (event & BUTTON_PLAY) && game->interaction_cooldown == 0)
    {
        game->menu_open = !game->menu_open;
        game->interaction_cooldown = HZ / 3;
        trace_log("minishcap menu %s room=%d pos=(%d,%d)",
                  game->menu_open ? "open" : "close",
                  game->room_id, game->x, game->y);
    }

    if ((event & BUTTON_MENU) && (event & BUTTON_PLAY) && game->interaction_cooldown == 0)
    {
        game->debug_overlay = !game->debug_overlay;
        game->interaction_cooldown = HZ / 4;
        trace_log("minishcap debug_overlay %s room=%d pos=(%d,%d)",
                  game->debug_overlay ? "on" : "off",
                  game->room_id, game->x, game->y);
    }

#if MINISHCAP_ENABLE_DEBUG_JUMPS
    if ((event & BUTTON_SELECT) && (event & BUTTON_RIGHT) && game->interaction_cooldown == 0)
    {
        if (game->room_id == ROOM_LINKS_HOUSE_BEDROOM)
        {
            begin_transition(game, "debug", 200, ROOM_LINKS_HOUSE_ENTRANCE, 120, 120);
            game->interaction_cooldown = HZ / 4;
            return;
        }
        if (game->room_id == ROOM_LINKS_HOUSE_ENTRANCE)
        {
            begin_transition(game, "debug", 201, ROOM_LINKS_HOUSE_SMITH, 36, 104);
            game->interaction_cooldown = HZ / 4;
            return;
        }
        if (game->room_id == ROOM_LINKS_HOUSE_SMITH)
        {
            game->sword_equipped = true;
            game->quest_stage = 2;
            begin_transition(game, "debug", 202, ROOM_LINKS_HOUSE_ENTRANCE, 120, 120);
            game->interaction_cooldown = HZ / 4;
            return;
        }
        if (game->room_id == ROOM_MEADOW)
        {
            begin_transition(game, "debug", 203, ROOM_LINKS_HOUSE_ENTRANCE,
                             ENTRANCE_FROM_MEADOW_X, ENTRANCE_FROM_MEADOW_Y);
            game->interaction_cooldown = HZ / 4;
            return;
        }
    }

    if ((event & BUTTON_SELECT) && (event & BUTTON_LEFT) && game->interaction_cooldown == 0)
    {
        if (game->room_id == ROOM_LINKS_HOUSE_ENTRANCE)
        {
            game->sword_equipped = true;
            game->quest_stage = 2;
            begin_transition(game, "debug", 204, ROOM_MEADOW,
                             HOUSE_EXIT_SAFE_X, HOUSE_EXIT_SAFE_Y);
            game->interaction_cooldown = HZ / 4;
            return;
        }
    }
#endif

    timer_step_down(&game->interaction_cooldown);

    if (game->message_timer > 0)
    {
        timer_step_down(&game->message_timer);
        if (game->message_timer == 0)
        {
            game->line1[0] = '\0';
            game->line2[0] = '\0';
            game->line3[0] = '\0';
        }
        return;
    }

    if (select_used)
        return;

    if (game->menu_open)
    {
        if (game->interaction_cooldown == 0)
        {
            if ((cmd & CMD_LEFT) || (cmd & CMD_UP))
            {
                game->menu_tab = (game->menu_tab + MENU_TAB_COUNT - 1) % MENU_TAB_COUNT;
                game->interaction_cooldown = HZ / 8;
            }
            else if ((cmd & CMD_RIGHT) || (cmd & CMD_DOWN))
            {
                game->menu_tab = (game->menu_tab + 1) % MENU_TAB_COUNT;
                game->interaction_cooldown = HZ / 8;
            }
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
    
    if (try_area_transition(game, next_x, next_y)) return;

    /* Interior collision: clamp movement against defined solids in the room. */
    if (game->room_id == ROOM_MEADOW) {
        if (meadow_player_blocked_at(next_x, next_y)) {
            int sx = next_x;
            int sy = game->y;
            int tx = game->x;
            int ty = next_y;

            if (!meadow_player_blocked_at(sx, sy)) {
                next_y = sy;
            } else if (!meadow_player_blocked_at(tx, ty)) {
                next_x = tx;
            } else {
                next_x = game->x;
                next_y = game->y;
                dx = 0; dy = 0;
            }
        }
    } else {
        const struct room_def *rd = current_room(game);
        for (int i = 0; i < rd->solid_count; i++) {
            struct rect s = rd->solids[i];
            if (next_x < s.x + s.w && next_x + PLAYER_W > s.x &&
                next_y < s.y + s.h && next_y + PLAYER_H > s.y) {
                next_x = game->x;
                next_y = game->y;
                dx = 0; dy = 0;
                break;
            }
        }
    }
    game->x = next_x;
    game->y = next_y;
    if (try_sword_pickup(game)) return;
    if (game->moving) game->step_clock++;
    if (game->sword_timer > 0) game->sword_timer--;
    timer_step_down(&game->house_reentry_cooldown);
    update_camera(game);
    static int last_room = -1;
    if (game->room_id != last_room || game->bgm_cooldown == 0) {
        update_bgm(game);
        last_room = game->room_id;
    }

#if MINISHCAP_TRACE_MOVEMENT
    if (game->x != prev_x || game->y != prev_y || game->room_id != prev_room)
    {
        trace_log("minishcap move room=%d pos=(%d,%d)->(%d,%d) cmd=%d event=%d",
                  game->room_id, prev_x, prev_y, game->x, game->y, cmd, event);
    }
#endif

}

static bool load_bitmap24(const char *path, struct bitmap *bmp, fb_data *pixels)
{
    memset(bmp, 0, sizeof(*bmp));
    bmp->data = (char*)pixels;
    int rc = rb->read_bmp_file(path, bmp, NPC_BMP_BYTES, FORMAT_NATIVE, NULL);
    return rc > 0 && bmp->width == NPC_BMP_W && bmp->height == NPC_BMP_H;
}

static bool load_bitmap_exact(const char *path, struct bitmap *bmp, fb_data *pixels, int w, int h, size_t bytes)
{
    memset(bmp, 0, sizeof(*bmp));
    bmp->data = (char*)pixels;
    int rc = rb->read_bmp_file(path, bmp, bytes, FORMAT_NATIVE, NULL);
    if (!(rc > 0 && bmp->width == w && bmp->height == h))
    {
        trace_log("minishcap asset read fail path=%s rc=%d got=%dx%d need=%dx%d bytes=%u",
                  path, rc, bmp->width, bmp->height, w, h, (unsigned)bytes);
    }
    return rc > 0 && bmp->width == w && bmp->height == h;
}

static bool load_meadow_chunk_from_path(int row, int col, const char *path, int slot,
                                        bool log_fail)
{
    struct bitmap *bmp = &meadow_chunk_bmps[row][col];
    fb_data *pixels = meadow_chunk_storage[slot];

    memset(bmp, 0, sizeof(*bmp));
    bmp->data = (char *)pixels;

    int rc = rb->read_bmp_file(path, bmp, MEADOW_CHUNK_BYTES, FORMAT_NATIVE, NULL);
    if (!(rc > 0 && bmp->width == MEADOW_CHUNK_W && bmp->height == MEADOW_CHUNK_H))
    {
        if (log_fail)
        {
            trace_log("minishcap chunk fail path=%s rc=%d got=%dx%d", path, rc,
                      bmp->width, bmp->height);
        }
        return false;
    }

    return true;
}

static bool load_meadow_chunk_slot(int row, int col, int slot)
{
    char path[128];

    rb->snprintf(path, sizeof(path), SOUTH_HYRULE_CHUNK_FILE_FMT_ALT, row, col);
    if (load_meadow_chunk_from_path(row, col, path, slot, false))
        return true;

    rb->snprintf(path, sizeof(path), SOUTH_HYRULE_CHUNK_FILE_FMT, row, col);
    if (load_meadow_chunk_from_path(row, col, path, slot, true))
        return true;

    return false;
}

static void invalidate_meadow_chunk_cache(void)
{
    for (int r = 0; r < MEADOW_CHUNK_ROWS; ++r)
    {
        for (int c = 0; c < MEADOW_CHUNK_COLS; ++c)
        {
            meadow_chunk_loaded[r][c] = false;
            meadow_chunk_missing[r][c] = false;
            meadow_chunk_slot_map[r][c] = -1;
        }
    }

    for (int i = 0; i < MEADOW_CHUNK_SLOTS; ++i)
    {
        meadow_chunk_slot_row[i] = -1;
        meadow_chunk_slot_col[i] = -1;
    }

    meadow_chunk_lru_tick = 0;
}

static int acquire_chunk_slot(void)
{
    for (int i = 0; i < MEADOW_CHUNK_SLOTS; ++i)
    {
        if (meadow_chunk_slot_row[i] < 0)
            return i;
    }

    return meadow_chunk_lru_tick++ % MEADOW_CHUNK_SLOTS;
}

static bool ensure_meadow_chunk_loaded(int row, int col)
{
    if (row < 0 || row >= MEADOW_CHUNK_ROWS || col < 0 || col >= MEADOW_CHUNK_COLS)
        return false;

    if (meadow_chunk_missing[row][col])
        return false;

    if (meadow_chunk_loaded[row][col] && meadow_chunk_slot_map[row][col] >= 0)
        return true;

    int slot = acquire_chunk_slot();
    int old_row = meadow_chunk_slot_row[slot];
    int old_col = meadow_chunk_slot_col[slot];

    if (old_row >= 0 && old_col >= 0)
    {
        meadow_chunk_loaded[old_row][old_col] = false;
        meadow_chunk_slot_map[old_row][old_col] = -1;
    }

    if (!load_meadow_chunk_slot(row, col, slot))
    {
        meadow_chunk_missing[row][col] = true;
        trace_log("minishcap chunk missing row=%d col=%d", row, col);
        return false;
    }

    meadow_chunk_loaded[row][col] = true;
    meadow_chunk_missing[row][col] = false;
    meadow_chunk_slot_map[row][col] = slot;
    meadow_chunk_slot_row[slot] = row;
    meadow_chunk_slot_col[slot] = col;
    return true;
}

static void load_external_bitmaps(void)
{
    size_t plugin_buf_size = 0;
    void *plugin_buf = rb->plugin_get_buffer(&plugin_buf_size);
    size_t south_hyrule_bytes = (size_t)MEADOW_ROOM_W * MEADOW_ROOM_H * sizeof(fb_data);

    asset_warning_mask = ASSET_WARN_NONE;

    smith_bmp_loaded = load_bitmap24(SMITH_BMP_FILE, &smith_bmp, smith_bmp_data);
    if (!smith_bmp_loaded)
        smith_bmp_loaded = load_bitmap24(SMITH_BMP_FILE_ALT, &smith_bmp, smith_bmp_data);
    if (!smith_bmp_loaded)
    {
        asset_warning_mask |= ASSET_WARN_SMITH;
        trace_log("minishcap asset missing smith_bmp");
    }

    zelda_bmp_loaded = load_bitmap24(ZELDA_BMP_FILE, &zelda_bmp, zelda_bmp_data);
    if (!zelda_bmp_loaded)
        zelda_bmp_loaded = load_bitmap24(ZELDA_BMP_FILE_ALT, &zelda_bmp, zelda_bmp_data);
    if (!zelda_bmp_loaded)
    {
        asset_warning_mask |= ASSET_WARN_ZELDA;
        trace_log("minishcap asset missing zelda_bmp");
    }

    south_hyrule_bmp_loaded = false;
    south_hyrule_bmp_data = NULL;
    invalidate_meadow_chunk_cache();

    if (plugin_buf != NULL && plugin_buf_size >= south_hyrule_bytes)
    {
        south_hyrule_bmp_loaded = load_bitmap_exact(
            SOUTH_HYRULE_BMP_FILE,
            &south_hyrule_bmp,
            (fb_data *)plugin_buf,
            MEADOW_ROOM_W,
            MEADOW_ROOM_H,
            south_hyrule_bytes);
        if (!south_hyrule_bmp_loaded)
        {
            south_hyrule_bmp_loaded = load_bitmap_exact(
                SOUTH_HYRULE_BMP_FILE_ALT,
                &south_hyrule_bmp,
                (fb_data *)plugin_buf,
                MEADOW_ROOM_W,
                MEADOW_ROOM_H,
                south_hyrule_bytes);
        }

        if (south_hyrule_bmp_loaded)
            south_hyrule_bmp_data = (fb_data *)plugin_buf;
    }
    else
    {
        trace_log("minishcap asset meadow_full skipped buf=%u need=%u", (unsigned)plugin_buf_size,
                  (unsigned)south_hyrule_bytes);
    }

    if (!south_hyrule_bmp_loaded)
    {
        bool chunk0_ready = ensure_meadow_chunk_loaded(0, 0);
        if (!chunk0_ready)
            trace_log("minishcap asset missing meadow chunks; using tiled fallback");
        else
            trace_log("minishcap asset using chunked meadow renderer");
    }
}

static void set_title_message(struct game_state *game)
{
    int warn_ticks = 0;
    const char *warn = asset_warning_text(&warn_ticks);

    if (warn != NULL)
    {
        set_message(game, "Minish Cap (assets fallback)", warn, "", warn_ticks);
        return;
    }

    set_message(game, "The Minish Cap", "Select: Sword / Talk", "Select+Play: Menu", HZ * 3);
}

enum plugin_status plugin_start(const void *parameter)
{
    struct game_state game;
    enum plugin_status status = PLUGIN_OK;
    (void)parameter;

    int trace_fd = rb->open(MINISHCAP_TRACE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (trace_fd >= 0)
        rb->close(trace_fd);
    trace_log("minishcap plugin_start build=%s", MINISHCAP_BUILD_TAG);

    rb->splash(HZ, MINISHCAP_BUILD_TAG);
    rb->lcd_set_foreground(COLOR_BLACK);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    rb->lcd_update();
    init_audio();
    ensure_audio_stream();
    load_external_bitmaps();
    init_default_game(&game);
    set_title_message(&game);
    while (true)
    {
        long event = rb->button_get_w_tmo(HZ / 30);
        int held = clean_button(rb->button_status());
        int cmd = decode_cmd(held);
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
    shutdown_audio();
    return status;
}
