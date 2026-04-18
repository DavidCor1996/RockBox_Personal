/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * nightcity - a compact cyberpunk narrative RPG for Rockbox
 *
 ***************************************************************************/

#ifndef NIGHTCITY_H
#define NIGHTCITY_H

#include "plugin.h"
#include "lib/pluginlib_actions.h"

#define NC_SAVE_FILE PLUGIN_GAMES_DATA_DIR "/nightcity.sav"
#define NC_SAVE_MAGIC "NCITY01"
#define NC_SAVE_VERSION 2

#define NC_MAX_WRAP_LINES 48
#define NC_MAX_LINE_CHARS 96
#define NC_TEXT_PAGE_LINES 8
#define NC_MAX_VISIBLE_CHOICES 8
#define NC_MAX_LOG_LINES 3

#define NC_STAT_MIN 0
#define NC_STAT_MAX 9
#define NC_ANY_MIN (-99)
#define NC_ANY_MAX 999
#define NC_DYNAMIC_ENDING (-100)

enum nc_lifepath
{
    NC_LIFEPATH_NONE = 0,
    NC_LIFEPATH_STREETKID,
    NC_LIFEPATH_CORPO,
    NC_LIFEPATH_NOMAD,
};

enum nc_gender
{
    NC_GENDER_NONE = 0,
    NC_GENDER_MASC,
    NC_GENDER_FEMME,
    NC_GENDER_NONBINARY,
};

enum nc_node_kind
{
    NC_NODE_SCENE = 0,
    NC_NODE_ENCOUNTER,
    NC_NODE_ENDING,
};

enum nc_scene_result
{
    NC_SCENE_NEXT = -1,
    NC_SCENE_MENU = -2,
    NC_SCENE_PANEL = -3,
    NC_SCENE_EXIT = -4,
};

enum nc_pause_result
{
    NC_PAUSE_RESUME = 0,
    NC_PAUSE_PANEL,
    NC_PAUSE_HELP,
    NC_PAUSE_TITLE,
    NC_PAUSE_EXIT,
};

enum nc_encounter_result
{
    NC_ENCOUNTER_WIN = 0,
    NC_ENCOUNTER_LOSE,
    NC_ENCOUNTER_MENU,
    NC_ENCOUNTER_PANEL,
    NC_ENCOUNTER_EXIT,
};

enum nc_story_flag
{
    NC_FLAG_CORP_CONTACT  = 1u << 0,
    NC_FLAG_NOMAD_ROUTE   = 1u << 1,
    NC_FLAG_DOCK_COVER    = 1u << 2,
    NC_FLAG_BADGE_SCHEMA  = 1u << 3,
    NC_FLAG_TUNNEL_MAP    = 1u << 4,
    NC_FLAG_LEDGER        = 1u << 5,
    NC_FLAG_SAVED_ROOK    = 1u << 6,
    NC_FLAG_BLOCKERS      = 1u << 7,
    NC_FLAG_REBEL_PLAN    = 1u << 8,
    NC_FLAG_MASTER_KEY    = 1u << 9,
    NC_FLAG_CORP_DEAL     = 1u << 10,
    NC_FLAG_ESCAPE_ROUTE  = 1u << 11,
    NC_FLAG_BLAST_PACK    = 1u << 12,
    NC_FLAG_END_REBEL     = 1u << 13,
    NC_FLAG_END_CORP      = 1u << 14,
    NC_FLAG_END_GHOST     = 1u << 15,
    NC_FLAG_END_ESCAPE    = 1u << 16,
    NC_FLAG_SPARK_MIRA    = 1u << 17,
    NC_FLAG_SPARK_ROOK    = 1u << 18,
    NC_FLAG_SPARK_JUNO    = 1u << 19,
    NC_FLAG_ROMANCE_MIRA  = 1u << 20,
    NC_FLAG_ROMANCE_ROOK  = 1u << 21,
    NC_FLAG_ROMANCE_JUNO  = 1u << 22,
    NC_FLAG_SPARK_NYRA    = 1u << 23,
    NC_FLAG_ROMANCE_NYRA  = 1u << 24,
    NC_FLAG_GENDER_MASC   = 1u << 25,
    NC_FLAG_GENDER_FEMME  = 1u << 26,
    NC_FLAG_GENDER_NONBINARY = 1u << 27,
};

enum nc_cyberware
{
    NC_CYBER_COMBAT_RIG = 1u << 0,
    NC_CYBER_GHOSTWALL  = 1u << 1,
    NC_CYBER_SOCIAL     = 1u << 2,
};

struct nc_condition
{
    int min_street_cred;
    int min_humanity;
    int min_ghost_sync;
    int min_credits;
    int min_scrap;
    int max_corp_heat;
    enum nc_lifepath lifepath;
    unsigned require_flags;
    unsigned forbid_flags;
    unsigned require_cyberware;
};

struct nc_effect
{
    int street_cred_delta;
    int corp_heat_delta;
    int humanity_delta;
    int ghost_sync_delta;
    int credits_delta;
    int health_delta;
    int max_health_delta;
    int medkit_delta;
    int stim_delta;
    int scrap_delta;
    unsigned add_flags;
    unsigned clear_flags;
    unsigned add_cyberware;
};

struct nc_choice
{
    const char *label;
    int next_id;
    struct nc_condition condition;
    struct nc_effect effect;
};

struct nc_node
{
    int id;
    enum nc_node_kind kind;
    const char *title;
    const char *speaker;
    const char *text;
    const struct nc_choice *choices;
    int choice_count;
    int next_id;
    int fail_id;
    int encounter_id;
    struct nc_effect on_enter;
};

struct nc_enemy
{
    int id;
    const char *name;
    const char *intro;
    int max_health;
    int attack_min;
    int attack_max;
    int firewall;
    int payout;
    int street_cred_reward;
    int heat_delta;
};

struct nc_game_state
{
    int current_node_id;
    enum nc_lifepath lifepath;
    enum nc_gender gender;
    int street_cred;
    int corp_heat;
    int humanity;
    int ghost_sync;
    int credits;
    int health;
    int max_health;
    int medkits;
    int stims;
    int scrap;
    unsigned flags;
    unsigned cyberware;
    bool active_save;
};

struct nc_visible_choice
{
    const struct nc_choice *choice;
    int source_index;
};

const char *nc_lifepath_name(enum nc_lifepath lifepath);
const char *nc_gender_name(enum nc_gender gender);
const char *nc_cyberware_name(unsigned item);

enum plugin_status nc_engine_run(void);

void nc_ui_init(void);
int nc_ui_input(int timeout);
int nc_ui_wrap_text(const char *text, int width,
                    char lines[][NC_MAX_LINE_CHARS], int max_lines);
void nc_ui_fill_background(void);
void nc_ui_frame(const char *title, const char *subtitle);
void nc_ui_footer(const char *left, const char *center, const char *right);
void nc_ui_box(int x, int y, int w, int h, bool selected);
void nc_ui_meter(int x, int y, int w, int value, int max_value, bool danger);
void nc_ui_update(void);
void nc_ui_transition(void);
void nc_ui_story_intro(const struct nc_node *node);
int nc_ui_run_title(bool has_continue);
enum nc_lifepath nc_ui_choose_lifepath(void);
enum nc_gender nc_ui_choose_gender(void);
void nc_ui_show_help(void);
void nc_ui_show_panel(const struct nc_game_state *state);
enum nc_pause_result nc_ui_run_pause_menu(void);
int nc_ui_run_scene(const struct nc_game_state *state,
                    const struct nc_node *node,
                    const struct nc_visible_choice *choices,
                    int choice_count);
void nc_ui_draw_encounter(const struct nc_game_state *state,
                          const struct nc_enemy *enemy,
                          int player_hp,
                          int enemy_hp,
                          const char log_lines[][NC_MAX_LINE_CHARS],
                          int selection,
                          bool defending,
                          int stim_turns);
void nc_ui_show_ending(const struct nc_game_state *state,
                       const struct nc_node *node);
void nc_ui_flash_message(const char *title, const char *text, int ticks);

void nc_story_start_run(struct nc_game_state *state, enum nc_lifepath lifepath);
const struct nc_node *nc_story_get_node(int id);
const struct nc_enemy *nc_story_get_enemy(int id);
int nc_story_choose_ending(const struct nc_game_state *state);

int nc_run_encounter(struct nc_game_state *state, const struct nc_enemy *enemy);

#endif
