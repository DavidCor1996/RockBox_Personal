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

#define PC_BANNER_LINE_CHARS   48
#define PC_RESULT_HOLD_FRAMES  24
#define PC_AUTO_RESET_FRAMES   72
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
};

void pc_assets_init(struct pc_asset_provider *assets);
void pc_assets_teardown(struct pc_asset_provider *assets);
const struct pc_creature_def *pc_assets_select_creature(struct pc_asset_provider *assets,
                                                        int species_index);
int pc_assets_get_creature_count(void);
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
void pc_state_cycle_species(struct pc_encounter_state *state, int delta);
void pc_state_update(struct pc_encounter_state *state,
                     const struct pc_input_command *command,
                     const struct pc_throw_request *throw_request);

void pc_render_frame(const struct pc_encounter_state *state);

#endif
