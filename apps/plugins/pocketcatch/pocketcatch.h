#ifndef POCKETCATCH_H
#define POCKETCATCH_H

#include "plugin.h"
#include "lib/pluginlib_bmp.h"

#ifndef HAVE_LCD_COLOR
#ifndef LCD_RGBPACK
#define PC_MONO_LUMA(r, g, b) (((r) * 30 + (g) * 59 + (b) * 11) / 100)
#define LCD_RGBPACK(r, g, b) \
    ((fb_data)(PC_MONO_LUMA((r), (g), (b)) >= 224 ? LCD_WHITE : \
               PC_MONO_LUMA((r), (g), (b)) >= 160 ? LCD_LIGHTGRAY : \
               PC_MONO_LUMA((r), (g), (b)) >= 96 ? LCD_DARKGRAY : \
               LCD_BLACK))
#endif
#endif

#define PC_FRAME_HZ            25
#define PC_FRAME_TICKS         MAX(1, HZ / PC_FRAME_HZ)
#define PC_ABS(x)              ((x) < 0 ? -(x) : (x))

#define PC_ASSET_ROOT          PLUGIN_GAMES_DIR "/pocketcatch"
#define PC_ASSET_ROOT_ALT      PLUGIN_DATA_DIR "/pocketcatch"
#define PC_BG_PATH             PC_ASSET_ROOT "/backgrounds/scene_day_layer0.bmp"
#define PC_BALL_PATH           PC_ASSET_ROOT "/sprites/balls/ball_default_idle_0.bmp"
#define PC_PACK_JSON_PATH      PC_ASSET_ROOT "/pack.json"
#define PC_SAVE_PATH           PC_ASSET_ROOT_ALT "/save.dat"
#define PC_MUSIC_ROOT_PATH     "/Music"
#define PC_MUSIC_HISTORY_PATH  PC_ASSET_ROOT_ALT "/music_history.dat"
#define PC_MUSIC_UNLOCKS_PATH  PC_ASSET_ROOT_ALT "/unlocked_songs.dat"

#define PC_BG_MAX_BYTES        (LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data))
#define PC_CREATURE_MAX_W      112
#define PC_CREATURE_MAX_H      112
#define PC_CREATURE_MAX_BYTES  (PC_CREATURE_MAX_W * PC_CREATURE_MAX_H * sizeof(fb_data))
#define PC_BALL_MAX_W          48
#define PC_BALL_MAX_H          48
#define PC_BALL_MAX_BYTES      (PC_BALL_MAX_W * PC_BALL_MAX_H * sizeof(fb_data))
#define PC_BALL_SPIN_FRAMES    4
#define PC_WORLD_PIXEL_SCALE   2
#define PC_WORLD_SCALE(px)     ((px) * PC_WORLD_PIXEL_SCALE)
#define PC_WORLD_SUBTILE_SIZE  PC_WORLD_SCALE(8)
#define PC_WORLD_TILE_SIZE     PC_WORLD_SCALE(32)
#define PC_WORLD_STEP_PX       PC_WORLD_SCALE(4)
#define PC_WORLD_FOOT_Y_OFFSET PC_WORLD_SCALE(7)
#define PC_WORLD_SPAWN_Y_OFFSET PC_WORLD_SCALE(3)
#define PC_WORLD_SPAWN_FOOT_Y_OFFSET PC_WORLD_SCALE(6)
#define PC_WORLD_ENCOUNTER_RADIUS PC_WORLD_SCALE(14)
#define PC_WORLD_SAFE_PLAYER_RADIUS PC_WORLD_SCALE(28)
#define PC_WORLD_SAFE_SPAWN_RADIUS PC_WORLD_SCALE(20)
#define PC_WORLD_SPAWN_MOVE_PX  PC_WORLD_PIXEL_SCALE

#ifdef SIMULATOR
#define PC_WORLD_CREATURE_MAX_W PC_WORLD_SCALE(24)
#define PC_WORLD_CREATURE_MAX_H PC_WORLD_SCALE(24)
#define PC_WORLD_TRAINER_MAX_W  PC_WORLD_SCALE(18)
#define PC_WORLD_TRAINER_MAX_H  PC_WORLD_SCALE(20)
#else
#define PC_WORLD_CREATURE_MAX_W PC_WORLD_SCALE(24)
#define PC_WORLD_CREATURE_MAX_H PC_WORLD_SCALE(24)
#define PC_WORLD_TRAINER_MAX_W  PC_WORLD_SCALE(18)
#define PC_WORLD_TRAINER_MAX_H  PC_WORLD_SCALE(20)
#endif
#define PC_WORLD_CREATURE_BYTES (PC_WORLD_CREATURE_MAX_W * PC_WORLD_CREATURE_MAX_H * sizeof(fb_data))
#define PC_WORLD_TRAINER_BYTES  (PC_WORLD_TRAINER_MAX_W * PC_WORLD_TRAINER_MAX_H * sizeof(fb_data))
#define PC_POKEDEX_MAX         128

#define PC_BANNER_LINE_CHARS   48
#define PC_RESULT_HOLD_FRAMES  14
#define PC_AUTO_RESET_FRAMES   18
#define PC_INTRO_FRAMES        28
#define PC_HIT_FRAMES          8
#define PC_SHAKE_FRAMES        10
#define PC_WHEEL_RELEASE_TICKS MAX(1, HZ / 7)

#define PC_BALL_HOME_X         (LCD_WIDTH / 2)
#ifdef HAVE_LCD_COLOR
#define PC_BALL_HOME_Y         (LCD_HEIGHT - 36)
#define PC_TARGET_X            (LCD_WIDTH / 2)
#define PC_TARGET_Y            94
#define PC_GROUND_Y            (LCD_HEIGHT - 54)
#define PC_CREATURE_BASE_Y     146
#else
#define PC_BALL_HOME_Y         (LCD_HEIGHT - 18)
#define PC_TARGET_X            (LCD_WIDTH / 2)
#define PC_TARGET_Y            54
#define PC_GROUND_Y            (LCD_HEIGHT - 28)
#define PC_CREATURE_BASE_Y     86
#endif
#define PC_WORLD_W             10
#define PC_WORLD_H             9
#define PC_WORLD_MAP_MAX_W     50
#define PC_WORLD_MAP_MAX_H     72
#define PC_WORLD_ORIGIN_X      0
#define PC_WORLD_ORIGIN_Y      (-28)
#define PC_WORLD_MAX_SPAWNS    4
#define PC_WORLD_WALK_FRAMES   3
#define PC_WORLD_SECRET_COUNT  8
#define PC_WORLD_POKESTOP_COUNT 10
#define PC_POKESTOP_SPIN_TARGET 84
#define PC_BUDDY_CANDY_STEPS   48
#define PC_MART_CATEGORY_COUNT 3
#define PC_PLAYER_TRAINER_COUNT 6

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
    int home_block_x;
    int home_block_y;
    int x;
    int y;
    int step;
    int dir_x;
    int dir_y;
    int respawn_frames;
    int lifetime_frames;
};

enum pc_world_scene {
    PC_WORLD_SCENE_PALLET = 0,
    PC_WORLD_SCENE_ROUTE1_SOUTH,
    PC_WORLD_SCENE_VIRIDIAN_SOUTH,
    PC_WORLD_SCENE_ROUTE2_SOUTH,
    PC_WORLD_SCENE_ROUTE21_NORTH,
    PC_WORLD_SCENE_VIRIDIAN_MART,
    PC_WORLD_SCENE_HOUSE_1F,
    PC_WORLD_SCENE_HOUSE_2F,
    PC_WORLD_SCENE_OAKS_LAB,
    PC_WORLD_SCENE_BLUES_HOUSE,
    PC_WORLD_SCENE_VIRIDIAN_POKECENTER,
    PC_WORLD_SCENE_VIRIDIAN_SCHOOL_HOUSE,
    PC_WORLD_SCENE_VIRIDIAN_NICKNAME_HOUSE,
    PC_WORLD_SCENE_VIRIDIAN_GYM,
    PC_WORLD_SCENE_ROUTE22,
    PC_WORLD_SCENE_ROUTE22_GATE,
    PC_WORLD_SCENE_ROUTE2_GATE,
    PC_WORLD_SCENE_ROUTE2_TRADE_HOUSE,
    PC_WORLD_SCENE_VIRIDIAN_FOREST_SOUTH_GATE,
    PC_WORLD_SCENE_VIRIDIAN_FOREST,
    PC_WORLD_SCENE_VIRIDIAN_FOREST_NORTH_GATE,
    PC_WORLD_SCENE_PEWTER,
    PC_WORLD_SCENE_PEWTER_GYM,
    PC_WORLD_SCENE_PEWTER_MART,
    PC_WORLD_SCENE_PEWTER_NIDORAN_HOUSE,
    PC_WORLD_SCENE_PEWTER_SPEECH_HOUSE,
    PC_WORLD_SCENE_PEWTER_POKECENTER,
    PC_WORLD_SCENE_MUSEUM_1F,
    PC_WORLD_SCENE_MUSEUM_2F,
    PC_WORLD_SCENE_ROUTE3,
    PC_WORLD_SCENE_ROUTE4,
    PC_WORLD_SCENE_MT_MOON_POKECENTER,
    PC_WORLD_SCENE_MT_MOON_1F,
    PC_WORLD_SCENE_MT_MOON_B1F,
    PC_WORLD_SCENE_MT_MOON_B2F,
    PC_WORLD_SCENE_CERULEAN,
    PC_WORLD_SCENE_CERULEAN_TRADE_HOUSE,
    PC_WORLD_SCENE_CERULEAN_POKECENTER,
    PC_WORLD_SCENE_CERULEAN_GYM,
    PC_WORLD_SCENE_BIKE_SHOP,
    PC_WORLD_SCENE_CERULEAN_MART,
    PC_WORLD_SCENE_CERULEAN_TRASHED_HOUSE,
    PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE
};

#define PC_WORLD_SCENE_MAX PC_WORLD_SCENE_CERULEAN_BADGE_HOUSE

enum pc_world_view {
    PC_WORLD_VIEW_MAP = 0,
    PC_WORLD_VIEW_MENU,
    PC_WORLD_VIEW_BAG,
    PC_WORLD_VIEW_POKEDEX,
    PC_WORLD_VIEW_MART,
    PC_WORLD_VIEW_SONGS,
    PC_WORLD_VIEW_BUDDY,
    PC_WORLD_VIEW_FIELD_MOVES,
    PC_WORLD_VIEW_FIELD_ASSIGN,
    PC_WORLD_VIEW_POKESTOP
};

enum pc_field_ability {
    PC_FIELD_ABILITY_SURF = 0,
    PC_FIELD_ABILITY_CUT,
    PC_FIELD_ABILITY_COUNT
};

struct pc_world_command {
    bool exit_requested;
    bool menu_requested;
    bool confirm;
    bool back;
    int move_x;
    int move_y;
    int hold_x;
    int hold_y;
    int release_x;
    int release_y;
    int nav_x;
    int nav_y;
};

struct pc_world_assets {
    struct pc_asset_bitmap trainer[4][PC_WORLD_WALK_FRAMES];
    struct pc_asset_bitmap creature[PC_WORLD_MAX_SPAWNS];
    struct pc_asset_bitmap dex_creature;
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
    int held_move_x;
    int held_move_y;
    int wheel_touch_x;
    int wheel_touch_y;
    int step_dx;
    int step_dy;
    int step_remaining;
    int encounter_cooldown;
    int encounter_grace_steps;
    int last_encounter_slot;
    int pending_species_index;
    int menu_index;
    int mart_category;
    int mart_index;
    int mart_assign_index;
    int field_index;
    int song_index;
    int bag_index;
    int buddy_index;
    int dex_index;
    int notice_frames;
    int outdoor_region;
    int travel_steps;
    int buddy_species;
    int buddy_steps;
    int pokestop_index;
    int pokestop_spin_progress;
    int pokestop_spin_angle;
    int pokestop_last_wheel_angle;
    int pokestop_reward_balls;
    int pokestop_reward_money;
    enum pc_world_scene scene;
    enum pc_world_view view;
    bool moving;
    bool map_dirty;
    bool encounter_armed;
    bool pending_encounter;
    bool quit_requested;
    bool pokestop_spun;
    struct pc_message banner;
    struct pc_message detail;
    unsigned char tiles[PC_WORLD_MAP_MAX_H][PC_WORLD_MAP_MAX_W];
    unsigned short pokeballs;
    unsigned short money;
    unsigned int secret_collected_bits;
    signed char player_trainer;
    unsigned short pokestop_cooldowns[PC_WORLD_POKESTOP_COUNT];
    unsigned short caught_counts[PC_POKEDEX_MAX];
    unsigned short family_candy[PC_POKEDEX_MAX];
    signed short ability_species[PC_FIELD_ABILITY_COUNT];
    unsigned char ability_owned[PC_FIELD_ABILITY_COUNT];
    unsigned char owned_trainers[PC_PLAYER_TRAINER_COUNT];
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
int pc_assets_get_total_creature_count(void);
const struct pc_creature_def *pc_assets_get_creature(int species_index);
int pc_assets_find_species_index(int species_id);
int pc_assets_get_family_index(int species_index);
int pc_assets_get_evolution_target(int species_index);
int pc_assets_get_evolution_cost(int species_index);
int pc_assets_get_catch_candy(int species_index);
bool pc_assets_load_world_creature(struct pc_asset_bitmap *asset, int species_index);
bool pc_assets_load_world_trainer(struct pc_asset_bitmap *asset, int heading, int frame);
bool pc_assets_load_named_world_trainer(struct pc_asset_bitmap *asset, const char *trainer_name,
                                        int heading, int frame);
void pc_assets_set_player_trainer_name(const char *trainer_name);
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
void pc_render_reset_trainer_asset(void);

void pc_world_init(struct pc_world_state *world);
void pc_world_teardown(struct pc_world_state *world);
void pc_world_input_handle_event(long event, struct pc_world_command *command);
void pc_world_update(struct pc_world_state *world, const struct pc_world_command *command);
bool pc_world_save(struct pc_world_state *world);
void pc_world_finish_encounter(struct pc_world_state *world,
                               enum pc_catch_outcome outcome,
                               int species_index);
void pc_world_cancel_encounter(struct pc_world_state *world);
int pc_world_song_count(void);
bool pc_world_song_name(int index, char *buffer, size_t buffer_size);
bool pc_world_secret_draw_info(const struct pc_world_state *world, int index,
                               int *metatile_x, int *metatile_y);
bool pc_world_pokestop_draw_info(const struct pc_world_state *world, int index,
                                 int *metatile_x, int *metatile_y, bool *ready);
int pc_world_nearby_count(const struct pc_world_state *world);
bool pc_world_nearby_name(const struct pc_world_state *world, int index,
                          char *buffer, size_t buffer_size);
int pc_world_player_trainer_count(void);
const char *pc_world_player_trainer_name(int index);
int pc_world_player_trainer_cost(int index);
void pc_world_render_frame(const struct pc_world_state *world);

#endif
