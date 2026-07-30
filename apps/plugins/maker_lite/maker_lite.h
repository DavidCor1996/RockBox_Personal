/***************************************************************************
 * Rockpod Maker Lite native Rockbox frontend.
 ***************************************************************************/

#ifndef ROCKBOX_MAKER_LITE_H
#define ROCKBOX_MAKER_LITE_H

#include "plugin.h"
#include "../../../lib/maker_lite/maker_lite.h"

#define MAKER_LITE_ROOT ROCKBOX_DIR "/games/maker_lite"
#define MAKER_LITE_KIT_ROOT MAKER_LITE_ROOT "/kits"
#define MAKER_LITE_PROJECT_ROOT MAKER_LITE_ROOT "/projects"
#define MAKER_LITE_SAVE_ROOT MAKER_LITE_ROOT "/saves"
#define MAKER_LITE_SETTINGS_ROOT MAKER_LITE_ROOT "/settings"
#define MAKER_LITE_BROWSER_MANIFEST \
    MAKER_LITE_ROOT "/projects.tsv"
#define MAKER_LITE_LEGACY_MANIFEST \
    ROCKBOX_DIR "/rocks/games/maker_lite/games.tsv"

#define MAKER_LITE_ART_HEADER_SIZE 64
#define MAKER_LITE_DIRECTION_COUNT 4
#define MAKER_LITE_ANIMATION_TABLE_V2_SIZE (ML_ACTION_COUNT * 4)
#define MAKER_LITE_ANIMATION_TABLE_V3_SIZE \
    (ML_ACTION_COUNT * MAKER_LITE_DIRECTION_COUNT * 4)
#define MAKER_LITE_PLAYER_FRAME_SIZE 36
#define MAKER_LITE_MAX_PLAYER_FRAMES 512
#define MAKER_LITE_MAX_ART_CELLS 1536
#define MAKER_LITE_MAX_BROWSER_PROJECTS 48
#define MAKER_LITE_AUDIO_EFFECTS 6

struct maker_lite_art {
    const fb_data *cells;
    fb_data mirror_scratch[16 * 16];
    uint16_t cell_size;
    uint16_t cell_count;
    uint16_t player_base;
    uint8_t entity_cells[16];
    uint16_t animation_start[MAKER_LITE_DIRECTION_COUNT][ML_ACTION_COUNT];
    uint8_t animation_count[MAKER_LITE_DIRECTION_COUNT][ML_ACTION_COUNT];
    uint8_t animation_ticks[MAKER_LITE_DIRECTION_COUNT][ML_ACTION_COUNT];
    uint8_t animation_mirror[MAKER_LITE_DIRECTION_COUNT][ML_ACTION_COUNT];
    const unsigned char *player_frames;
    uint16_t player_frame_count;
};

struct maker_lite_app {
    struct ml_level level;
    struct ml_world world;
    struct maker_lite_art art;
    unsigned char *arena;
    size_t arena_size;
    size_t arena_used;
    const unsigned char *pack_data;
    size_t pack_size;
    char pack_path[MAX_PATH];
    char save_path[MAX_PATH];
    long start_tick;
    long last_tick;
    long menu_pressed_tick;
    int wheel_zone;
    int last_physical;
    uint32_t physical_map[4];
    unsigned simulation_fraction;
    unsigned rendered_frames;
    unsigned simulation_ticks;
    unsigned missed_deadlines;
    unsigned max_render_ticks;
    unsigned hold_pause_count;
    unsigned menu_pause_count;
    unsigned controls_screen_count;
    unsigned room_transition_count;
    unsigned car_entry_count;
    unsigned car_exit_count;
    unsigned life_interaction_count;
    bool quit;
    bool usb;
    bool save_requested;
    bool hold_paused;
    bool menu_down;
    const unsigned char *audio_data;
    uint32_t audio_offsets[MAKER_LITE_AUDIO_EFFECTS];
    uint32_t audio_lengths[MAKER_LITE_AUDIO_EFFECTS];
    bool audio_ready;
    bool playback_active_at_start;
};

extern struct maker_lite_app maker_lite;

bool maker_lite_storage_load(const char *path);
bool maker_lite_storage_load_art(void);
bool maker_lite_storage_load_save(void);
bool maker_lite_storage_load_settings(void);
bool maker_lite_storage_save(void);
bool maker_lite_storage_pick_project(char *path, size_t path_size);
void maker_lite_storage_log_session(void);
#ifdef SIMULATOR
bool maker_lite_storage_dump_test_frame(void);
#endif
bool maker_lite_audio_init(void);
void maker_lite_audio_play(unsigned effect);
void maker_lite_audio_shutdown(void);

uint32_t maker_lite_input_poll(int event);
void maker_lite_input_init(void);
void maker_lite_render(void);
void maker_lite_render_pause(bool locked);
void maker_lite_render_controls(void);
void maker_lite_haptic(int duration, int strength);

#endif
