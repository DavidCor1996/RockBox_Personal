/***************************************************************************
 * iPod Hero - five-lane rhythm game for click-wheel iPods
 *
 * Copyright (C) 2026 OpenAI
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2.
 ****************************************************************************/

#ifndef IPODHERO_H
#define IPODHERO_H

#include "plugin.h"

#include <stdbool.h>
#include <stdint.h>

#define IH_DATA_DIR ROCKBOX_DIR "/rocks.data/ipodhero"
#define IH_INDEX_FILE IH_DATA_DIR "/index.tsv"
#define IH_CONFIG_FILE IH_DATA_DIR "/config.cfg"
#define IH_SCORE_FILE IH_DATA_DIR "/scores.dat"
#define IH_SCORE_BACKUP IH_DATA_DIR "/scores.bak"
#define IH_PERF_FILE IH_DATA_DIR "/performance.log"
#define IH_INPUT_TRACE_FILE IH_DATA_DIR "/input-trace.log"
#define IH_RUNTIME_FILE IH_DATA_DIR "/runtime.log"
#ifdef SIMULATOR
#define IH_SIM_TEST_FILE IH_DATA_DIR "/sim-test.log"
#define IH_SIM_ERROR_FILE IH_DATA_DIR "/sim-error.log"
#define IH_SIM_LIBRARY_FILE IH_DATA_DIR "/sim-library.log"
#endif

#define IH_LANE_COUNT 5
#define IH_MAX_EVENTS 20000u
#define IH_MAX_SECTIONS 512u
#define IH_MAX_SONGS 64u
#define IH_CHART_LIMIT (256u * 1024u)
#define IH_SKIN_LIMIT (1024u * 1024u)
#define IH_FRAME_TICKS MAX(1, HZ / 30)
#define IH_APPROACH_MS 2000
#define IH_HIT_WINDOW_MS 160
#define IH_MAX_HIT_WINDOW_MS 240
#define IH_SUSTAIN_GRACE_MS 80
#define IH_GEM_BANDS 4
#define IH_WHEEL_STAR_STEPS 8
#define IH_WHEEL_GESTURE_TICKS (HZ * 3 / 4)

#define IH_NOTE_HOPO 0x01
#define IH_NOTE_TAP 0x02
#define IH_NOTE_STAR 0x04
#define IH_NOTE_PHRASE_END 0x08
#define IH_NOTE_GENERATED 0x10
#define IH_NOTE_FORCED 0x20
#define IH_NOTE_SUSTAIN_BROKEN 0x80

enum ih_difficulty
{
    IH_EASY = 0,
    IH_MEDIUM,
    IH_HARD,
    IH_EXPERT,
    IH_DIFFICULTY_COUNT
};

enum ih_origin
{
    IH_ORIGIN_AUTHORED = 0,
    IH_ORIGIN_IMPORTED,
    IH_ORIGIN_GENERATED
};

enum ih_judgement
{
    IH_JUDGE_NONE = 0,
    IH_JUDGE_PERFECT,
    IH_JUDGE_GREAT,
    IH_JUDGE_GOOD,
    IH_JUDGE_GRACE,
    IH_JUDGE_MISS
};

enum ih_event_state
{
    IH_EVENT_WAITING = 0,
    IH_EVENT_HIT,
    IH_EVENT_MISSED
};

struct ih_arena
{
    unsigned char *base;
    size_t size;
    size_t used;
};

struct ih_note_event
{
    uint32_t time_ms;
    uint32_t duration_ms;
    uint16_t phrase_id;
    uint8_t lane_mask;
    uint8_t flags;
    uint8_t state;
    uint8_t collected;
};

struct ih_section
{
    uint32_t time_ms;
    uint32_t name_offset;
};

struct ih_chart
{
    struct ih_note_event *events;
    struct ih_section *sections;
    char *section_strings;
    uint32_t event_count;
    uint32_t section_count;
    uint32_t section_string_bytes;
    uint32_t song_length_ms;
    int32_t audio_offset_ms;
    uint32_t payload_crc32;
    uint8_t difficulty;
    uint8_t origin;
};

struct ih_index_entry
{
    char audio_path[MAX_PATH];
    char chart_path[IH_DIFFICULTY_COUNT][MAX_PATH];
    char title[64];
    char artist[64];
    char skin_id[32];
    uint32_t audio_size;
    uint32_t audio_length_ms;
    uint32_t audio_crc32;
};

struct ih_song_library
{
    struct ih_index_entry *entries;
    int count;
    int selected;
};

struct ih_image
{
    struct bitmap bitmap;
    bool loaded;
};

struct ih_skin
{
    char name[48];
    char root[MAX_PATH];
    struct ih_image background;
    struct ih_image background_alt;
    struct ih_image highway;
    struct ih_image logo;
    struct ih_image gems;
    struct ih_image hopo;
    struct ih_image star;
    struct ih_image rings;
    struct ih_image flames;
    struct ih_image hud;
    struct ih_image rock_meter;
    struct ih_image star_meter;
    struct ih_image sustains;
    struct ih_image results_stars;
    size_t decoded_bytes;
};

enum ih_input_kind
{
    IH_INPUT_NONE = 0,
    IH_INPUT_PRESS,
    IH_INPUT_RELEASE,
    IH_INPUT_PAUSE,
    IH_INPUT_WHEEL,
    IH_INPUT_STAR
};

struct ih_input
{
    int clockwise_steps;
    long gesture_deadline;
};

struct ih_input_event
{
    enum ih_input_kind kind;
    uint8_t lane_mask;
};

struct ih_audio_clock
{
    struct mp3entry *id3;
    char path[MAX_PATH];
    long last_sync_elapsed;
    long last_sync_tick;
    long elapsed;
    int status;
    bool practice;
    long practice_start_tick;
    long practice_pause_ms;
    uint32_t correction_count;
    uint32_t max_interpolation_lead_ms;
};

struct ih_score
{
    uint32_t points;
    uint32_t hit_count;
    uint32_t miss_count;
    uint32_t perfect_count;
    uint32_t great_count;
    uint32_t good_count;
    uint32_t grace_count;
    uint32_t streak;
    uint32_t max_streak;
    uint32_t sustain_ms;
    int rock;
    int last_error_ms;
    enum ih_judgement last_judgement;
    long feedback_until;
    uint32_t star_ms;
    uint16_t phrase_id;
    uint8_t last_hit_mask;
    bool phrase_failed;
    bool star_active;
};

struct ih_game
{
    struct ih_chart chart;
    struct ih_score score;
    uint32_t judge_cursor;
    uint32_t section_cursor;
    uint8_t held_mask;
    bool no_fail;
    bool finished;
    bool failed;
    bool paused;
    bool track_changed;
    bool assist;
    int calibration_ms;
    int hit_window_ms;
    int song_time_ms;
    int last_update_ms;
    uint32_t frame_count;
    uint32_t frame_ticks_total;
    uint32_t frame_ticks_max;
    uint32_t missed_frame_deadlines;
    uint32_t frame_tick_histogram[16];
    uint32_t max_input_judgement_ticks;
    uint32_t max_active_sprites;
};

struct ih_app
{
    struct ih_arena arena;
    struct ih_song_library library;
    struct ih_index_entry index;
    struct ih_skin skin;
    struct ih_chart chart;
    struct ih_audio_clock clock;
    struct ih_game game;
    struct ih_input input;
    size_t library_mark;
    size_t chart_mark;
    uint32_t load_ticks;
    uint32_t best_score;
    uint32_t best_accuracy_bp;
    int selected_difficulty;
    bool last_practice;
    bool scores_valid;
    bool usb;
#ifdef SIMULATOR
    int sim_test_mode;
    bool sim_star_activated;
    int sim_release_at[IH_LANE_COUNT];
    uint32_t sim_runs;
    uint32_t sim_pause_count;
    uint32_t sim_scripted_pause_count;
    bool sim_pause_done;
#endif
};

void *ih_arena_alloc(struct ih_arena *arena, size_t bytes);
size_t ih_arena_mark(const struct ih_arena *arena);
void ih_arena_reset(struct ih_arena *arena, size_t mark);

bool ih_index_find(const char *audio_path, struct ih_index_entry *entry,
                   char *error, size_t error_size);
bool ih_index_load_library(struct ih_song_library *library,
                           struct ih_arena *arena,
                           char *error, size_t error_size);
bool ih_chart_load(struct ih_chart *chart, struct ih_arena *arena,
                   const char *path, char *error, size_t error_size);

void ih_clock_init(struct ih_audio_clock *clock, bool practice);
long ih_clock_update(struct ih_audio_clock *clock);
void ih_clock_rebase(struct ih_audio_clock *clock);
bool ih_clock_track_changed(const struct ih_audio_clock *clock);

bool ih_skin_load(struct ih_skin *skin, struct ih_arena *arena,
                  const char *skin_id, char *error, size_t error_size);

void ih_input_reset(struct ih_input *input);
struct ih_input_event ih_input_normalize(struct ih_input *input,
                                         long button);

void ih_game_reset(struct ih_game *game, const struct ih_chart *chart,
                   int calibration_ms, bool no_fail, int hit_window_ms);
void ih_game_press(struct ih_game *game, uint8_t lane_mask);
void ih_game_release(struct ih_game *game, uint8_t lane_mask);
void ih_game_activate_star(struct ih_game *game);
void ih_game_whammy(struct ih_game *game);
void ih_game_update(struct ih_game *game, int song_time_ms);
int ih_score_multiplier(const struct ih_score *score);

void ih_render_menu(const struct ih_app *app, int selected,
                    const char *message);
void ih_render_song_library(const struct ih_song_library *library,
                            int selected);
void ih_render_game(const struct ih_app *app);
uint32_t ih_render_active_sprites(const struct ih_game *game);
void ih_render_pause(const struct ih_app *app, int selected);
void ih_render_results(const struct ih_app *app, int selected);
void ih_render_calibration(const struct ih_app *app, int proposed,
                           bool allow_tap);
void ih_render_calibration_tap(const struct ih_app *app, int tap_count);
void ih_render_settings(const struct ih_app *app, int selected);
void ih_render_input_test(uint32_t count, const char *last_event,
                          bool hold, bool full);
void ih_render_error(const char *title, const char *detail);

bool ih_input_probe_run(struct ih_app *app, char *error,
                        size_t error_size);

bool ih_scores_load_best(struct ih_app *app, char *error,
                         size_t error_size);
bool ih_scores_save(const struct ih_app *app, char *error,
                    size_t error_size);

#endif
