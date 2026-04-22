#ifndef POCKETCATCH_H
#define POCKETCATCH_H

#include "plugin.h"
#include "lib/pluginlib_bmp.h"

#define PC_FRAME_HZ            25
#define PC_FRAME_TICKS         MAX(1, HZ / PC_FRAME_HZ)
#define PC_ABS(x)              ((x) < 0 ? -(x) : (x))

#define PC_ASSET_ROOT          PLUGIN_GAMES_DIR "/pocketcatch"
#define PC_ASSET_ROOT_ALT      PLUGIN_DATA_DIR "/pocketcatch"
#define PC_BG_PATH             PC_ASSET_ROOT "/backgrounds/scene_day_layer0.bmp"
#define PC_BALL_PATH           PC_ASSET_ROOT "/sprites/balls/ball_default_idle_0.bmp"
#define PC_PACK_JSON_PATH      PC_ASSET_ROOT "/pack.json"

#define PC_BG_MAX_BYTES        (LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data))
#define PC_CREATURE_MAX_W      112
#define PC_CREATURE_MAX_H      112
#define PC_CREATURE_MAX_BYTES  (PC_CREATURE_MAX_W * PC_CREATURE_MAX_H * sizeof(fb_data))
#define PC_BALL_MAX_W          48
#define PC_BALL_MAX_H          48
#define PC_BALL_MAX_BYTES      (PC_BALL_MAX_W * PC_BALL_MAX_H * sizeof(fb_data))
#define PC_BALL_SPIN_FRAMES    4
#ifdef SIMULATOR
#define PC_WORLD_CREATURE_MAX_W 28
#define PC_WORLD_CREATURE_MAX_H 28
#define PC_WORLD_TRAINER_MAX_W  28
#define PC_WORLD_TRAINER_MAX_H  28
#else
#define PC_WORLD_CREATURE_MAX_W 24
#define PC_WORLD_CREATURE_MAX_H 24
#define PC_WORLD_TRAINER_MAX_W  24
#define PC_WORLD_TRAINER_MAX_H  24
#endif
#define PC_WORLD_CREATURE_BYTES (PC_WORLD_CREATURE_MAX_W * PC_WORLD_CREATURE_MAX_H * sizeof(fb_data))
#define PC_WORLD_TRAINER_BYTES  (PC_WORLD_TRAINER_MAX_W * PC_WORLD_TRAINER_MAX_H * sizeof(fb_data))

#define PC_BANNER_LINE_CHARS   48
#define PC_RESULT_HOLD_FRAMES  14
#define PC_AUTO_RESET_FRAMES   18
#define PC_INTRO_FRAMES        28
#define PC_HIT_FRAMES          8
#define PC_SHAKE_FRAMES        10
#define PC_WHEEL_RELEASE_TICKS MAX(1, HZ / 7)

#define PC_BALL_HOME_X         (LCD_WIDTH / 2)
#define PC_BALL_HOME_Y         (LCD_HEIGHT - 36)
#define PC_TARGET_X            (LCD_WIDTH / 2)
#define PC_TARGET_Y            94
#define PC_GROUND_Y            (LCD_HEIGHT - 54)
#define PC_CREATURE_BASE_Y     146
#define PC_WORLD_TILE_SIZE     32
#define PC_WORLD_W             10
#define PC_WORLD_H             9
#define PC_WORLD_ORIGIN_X      0
#define PC_WORLD_ORIGIN_Y      (-28)
#define PC_WORLD_MAX_SPAWNS    4
#define PC_WORLD_STEP_PX       4
#define PC_WORLD_WALK_FRAMES   3

enum pc_phase {
    PC_PHASE_INTRO = 0,
    PC_PHASE_IDLE_READY,
    PC_PHASE_BALL_HELD,
    PC_PHASE_BALL_THROWN,
    PC_PHASE_HIT_RESOLVE,
    PC_PHASE_SHAKE_1,
    PC_PHASE_SHAKE_2,
    PC_PHASE_SHAKE_3,
    PC_PHASE_CAUGHT,
    PC_PHASE_BROKE_OUT,
    PC_PHASE_RESULT,
    PC_PHASE_RESET_NEXT
};

enum pc_game_mode {
    PC_MODE_WORLD = 0,
    PC_MODE_ENCOUNTER
};

enum pc_heading {
    PC_HEADING_N = 0,
    PC_HEADING_E,
    PC_HEADING_S,
    PC_HEADING_W
};

enum pc_throw_tier {
    PC_THROW_TIER_MISS = 0,
    PC_THROW_TIER_HIT,
    PC_THROW_TIER_NICE,
    PC_THROW_TIER_GREAT,
    PC_THROW_TIER_EXCELLENT
};

enum pc_catch_outcome {
    PC_CATCH_OUTCOME_NONE = 0,
    PC_CATCH_OUTCOME_MISS,
    PC_CATCH_OUTCOME_BREAKOUT,
    PC_CATCH_OUTCOME_CAUGHT
};

enum pc_asset_source {
    PC_ASSET_SOURCE_BUILTIN = 0,
    PC_ASSET_SOURCE_PACK_V0
};

struct pc_asset_bitmap {
    struct bitmap bmp;
    fb_data *pixels;
    int capacity;
    bool loaded;
    bool external;
    char path[MAX_PATH];
};

struct pc_creature_def {
    int species_id;
    const char *name;
    const char *sprite_prefix;
    int base_capture_rate;
    int hit_radius_x;
    int hit_radius_y;
    int sprite_w;
    int sprite_h;
    int target_y_offset;
    fb_data primary;
    fb_data secondary;
    fb_data accent;
};

struct pc_ring_state {
    int min_radius;
    int max_radius;
    int radius;
    int period_frames;
};

struct pc_input_state {
    bool grabbing;
    long hold_start_tick;
    long last_wheel_tick;
    long prev_wheel_tick;
    int signed_spin;
    int total_spin;
    int spin_energy;
    int release_velocity;
    int last_wheel_dir;
    int wheel_events;
    int spin_phase;
    int spin_velocity;
};

struct pc_throw_request {
    bool valid;
    int hold_ticks;
    int signed_spin;
    int total_spin;
    int charge;
    int release_velocity;
    int wheel_events;
    int spin_phase;
    int spin_velocity;
    int release_bias_x;
    int release_bias_y;
};

struct pc_throw_state {
    bool active;
    bool finished;
    bool hit;
    bool curve_bonus;
    int x;
    int y;
    int start_x;
    int start_y;
    int end_x;
    int end_y;
    int curve_px;
    int arc_height;
    int duration_frames;
    int frame;
    int impact_dx;
    int impact_dy;
    int power_score;
    int spin_score;
    int travel_score;
    int spin_phase;
    int spin_velocity;
    enum pc_throw_tier tier;
};

struct pc_input_command {
    bool exit_requested;
    bool next_species;
    bool prev_species;
    bool start_grab;
    bool quick_reset;
};

struct pc_message {
    char line1[PC_BANNER_LINE_CHARS];
    char line2[PC_BANNER_LINE_CHARS];
};

struct pc_asset_provider {
    enum pc_asset_source source;
    struct pc_asset_bitmap background;
    struct pc_asset_bitmap creature;
    struct pc_asset_bitmap ball_idle;
    struct pc_asset_bitmap ball_spin[PC_BALL_SPIN_FRAMES];
    const struct pc_creature_def *active_creature;
};

struct pc_encounter_state {
    enum pc_phase phase;
    int phase_frame;
    int total_frames;
    int species_index;
    int breakout_after_shake;
    int shake_offset;
    bool simulator_debug;

    struct pc_ring_state ring;
    struct pc_input_state input;
    struct pc_throw_state throw_state;
    struct pc_asset_provider assets;
    struct pc_message banner;

    const struct pc_creature_def *creature;
    enum pc_throw_tier last_tier;
    enum pc_catch_outcome outcome;
    int last_catch_chance;
    bool finished;
};

struct pc_world_spawn {
    bool active;
    int species_index;
    int x;
    int y;
    int step;
    int dir_x;
    int dir_y;
};

enum pc_world_scene {
    PC_WORLD_SCENE_PALLET = 0,
    PC_WORLD_SCENE_HOUSE_1F,
    PC_WORLD_SCENE_HOUSE_2F
};

struct pc_world_command {
    bool exit_requested;
    int move_x;
    int move_y;
};

struct pc_world_assets {
    struct pc_asset_bitmap trainer[4][PC_WORLD_WALK_FRAMES];
    struct pc_asset_bitmap creature[PC_WORLD_MAX_SPAWNS];
};

struct pc_world_state {
    int frame;
    int player_x;
    int player_y;
    int spawn_x;
    int spawn_y;
    int home_x;
    int home_y;
    int map_w;
    int map_h;
    int origin_x;
    int origin_y;
    int heading;
    int walk_frame;
    int walk_tick;
    int last_encounter_slot;
    int pending_species_index;
    enum pc_world_scene scene;
    bool moving;
    bool map_dirty;
    bool pending_encounter;
    struct pc_message banner;
    unsigned char tiles[PC_WORLD_H][PC_WORLD_W];
    struct pc_world_assets assets;
    struct pc_world_spawn spawns[PC_WORLD_MAX_SPAWNS];
};

struct pc_game_state {
    enum pc_game_mode mode;
    struct pc_world_state world;
    struct pc_encounter_state encounter;
};

void pc_assets_init(struct pc_asset_provider *assets);
void pc_assets_teardown(struct pc_asset_provider *assets);
const struct pc_creature_def *pc_assets_select_creature(struct pc_asset_provider *assets,
                                                        int species_index);
int pc_assets_get_creature_count(void);
const struct pc_creature_def *pc_assets_get_creature(int species_index);
bool pc_assets_load_world_creature(struct pc_asset_bitmap *asset, int species_index);
bool pc_assets_load_world_trainer(struct pc_asset_bitmap *asset, int heading, int frame);
const char *pc_assets_source_label(const struct pc_asset_provider *assets);

void pc_input_reset(struct pc_input_state *input);
void pc_input_animate(struct pc_input_state *input, long now);
void pc_input_snapshot_throw(const struct pc_input_state *input, long now,
                             struct pc_throw_request *request);
void pc_input_handle_event(struct pc_input_state *input, long event, long now,
                           struct pc_input_command *command,
                           struct pc_throw_request *throw_request);

void pc_physics_build_throw(struct pc_throw_state *throw_state,
                            const struct pc_creature_def *creature,
                            const struct pc_ring_state *ring,
                            const struct pc_throw_request *request);
void pc_physics_update_throw(struct pc_throw_state *throw_state,
                             const struct pc_creature_def *creature,
                             const struct pc_ring_state *ring);

enum pc_catch_outcome pc_catch_roll(const struct pc_creature_def *creature,
                                    const struct pc_throw_state *throw_state,
                                    int *chance_out,
                                    int *breakout_after_shake);
const char *pc_throw_tier_label(enum pc_throw_tier tier);

void pc_state_init(struct pc_encounter_state *state, bool simulator_debug);
void pc_state_begin(struct pc_encounter_state *state, int species_index);
void pc_state_cycle_species(struct pc_encounter_state *state, int delta);
void pc_state_update(struct pc_encounter_state *state,
                     const struct pc_input_command *command,
                     const struct pc_throw_request *throw_request);

void pc_render_frame(const struct pc_encounter_state *state);

void pc_world_init(struct pc_world_state *world);
void pc_world_teardown(struct pc_world_state *world);
void pc_world_input_handle_event(long event, struct pc_world_command *command);
void pc_world_update(struct pc_world_state *world, const struct pc_world_command *command);
void pc_world_finish_encounter(struct pc_world_state *world,
                               enum pc_catch_outcome outcome,
                               int species_index);
void pc_world_render_frame(const struct pc_world_state *world);

#endif
