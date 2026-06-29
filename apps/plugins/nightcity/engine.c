/***************************************************************************
 * nightcity - engine, save handling, and game loop
 ***************************************************************************/

#include "nightcity.h"

struct nc_save_blob
{
    char magic[8];
    int version;
    struct nc_game_state state;
};

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

static void clamp_state(struct nc_game_state *state)
{
    state->street_cred = clamp_int(state->street_cred, NC_STAT_MIN, NC_STAT_MAX);
    state->corp_heat = clamp_int(state->corp_heat, NC_STAT_MIN, NC_STAT_MAX);
    state->humanity = clamp_int(state->humanity, NC_STAT_MIN, NC_STAT_MAX);
    state->ghost_sync = clamp_int(state->ghost_sync, NC_STAT_MIN, NC_STAT_MAX);
    state->max_health = clamp_int(state->max_health, 12, 40);
    state->health = clamp_int(state->health, 1, state->max_health);
    state->credits = clamp_int(state->credits, 0, 999);
    state->medkits = clamp_int(state->medkits, 0, 9);
    state->stims = clamp_int(state->stims, 0, 9);
    state->scrap = clamp_int(state->scrap, 0, 9);
}

static bool save_exists(void)
{
    int fd = rb->open(NC_SAVE_FILE, O_RDONLY);
    if (fd < 0)
        return false;
    rb->close(fd);
    return true;
}

static void save_clear(void)
{
    rb->remove(NC_SAVE_FILE);
}

static bool save_write(const struct nc_game_state *state)
{
    struct nc_save_blob blob;
    int fd;

    rb->memset(&blob, 0, sizeof(blob));
    rb->strlcpy(blob.magic, NC_SAVE_MAGIC, sizeof(blob.magic));
    blob.version = NC_SAVE_VERSION;
    blob.state = *state;

    fd = rb->open(NC_SAVE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    if (rb->write(fd, &blob, sizeof(blob)) != (long)sizeof(blob))
    {
        rb->close(fd);
        return false;
    }

    rb->close(fd);
    return true;
}

static bool save_load(struct nc_game_state *state)
{
    struct nc_save_blob blob;
    int fd = rb->open(NC_SAVE_FILE, O_RDONLY);

    if (fd < 0)
        return false;

    if (rb->read(fd, &blob, sizeof(blob)) != (long)sizeof(blob))
    {
        rb->close(fd);
        return false;
    }
    rb->close(fd);

    if (rb->strcmp(blob.magic, NC_SAVE_MAGIC) != 0 || blob.version != NC_SAVE_VERSION)
        return false;

    *state = blob.state;
    clamp_state(state);
    return true;
}

static void apply_effect(struct nc_game_state *state, const struct nc_effect *effect)
{
    if (effect == NULL)
        return;

    state->street_cred += effect->street_cred_delta;
    state->corp_heat += effect->corp_heat_delta;
    state->humanity += effect->humanity_delta;
    state->ghost_sync += effect->ghost_sync_delta;
    state->credits += effect->credits_delta;
    state->max_health += effect->max_health_delta;
    state->health += effect->health_delta;
    state->medkits += effect->medkit_delta;
    state->stims += effect->stim_delta;
    state->scrap += effect->scrap_delta;
    state->flags |= effect->add_flags;
    state->flags &= ~effect->clear_flags;
    state->cyberware |= effect->add_cyberware;

    clamp_state(state);
}

static bool condition_met(const struct nc_game_state *state,
                          const struct nc_condition *condition)
{
    if (condition == NULL)
        return true;

    if (condition->lifepath != NC_LIFEPATH_NONE &&
        state->lifepath != condition->lifepath)
        return false;
    if (condition->min_street_cred != NC_ANY_MIN &&
        state->street_cred < condition->min_street_cred)
        return false;
    if (condition->min_humanity != NC_ANY_MIN &&
        state->humanity < condition->min_humanity)
        return false;
    if (condition->min_ghost_sync != NC_ANY_MIN &&
        state->ghost_sync < condition->min_ghost_sync)
        return false;
    if (condition->min_credits != NC_ANY_MIN &&
        state->credits < condition->min_credits)
        return false;
    if (condition->min_scrap != NC_ANY_MIN &&
        state->scrap < condition->min_scrap)
        return false;
    if (condition->max_corp_heat != NC_ANY_MAX &&
        state->corp_heat > condition->max_corp_heat)
        return false;
    if ((state->flags & condition->require_flags) != condition->require_flags)
        return false;
    if ((state->flags & condition->forbid_flags) != 0)
        return false;
    if ((state->cyberware & condition->require_cyberware) !=
        condition->require_cyberware)
        return false;

    return true;
}

static int build_visible_choices(const struct nc_game_state *state,
                                 const struct nc_node *node,
                                 struct nc_visible_choice *out_choices)
{
    int visible = 0;
    int i;

    for (i = 0; i < node->choice_count && visible < NC_MAX_VISIBLE_CHOICES; ++i)
    {
        if (!condition_met(state, &node->choices[i].condition))
            continue;

        out_choices[visible].choice = &node->choices[i];
        out_choices[visible].source_index = i;
        ++visible;
    }

    return visible;
}

static void start_radio_shuffle_once(bool *radio_started)
{
    if (*radio_started)
        return;

    *radio_started = true;
}

static void apply_profile_bonus(struct nc_game_state *state, enum nc_profile profile)
{
    state->profile = profile;

    switch (profile)
    {
        case NC_PROFILE_RAZOR:
            state->street_cred += 1;
            state->max_health += 2;
            state->health += 2;
            break;
        case NC_PROFILE_VELVET:
            state->credits += 20;
            state->humanity += 1;
            state->corp_heat += 1;
            break;
        case NC_PROFILE_DRIFT:
            state->ghost_sync += 1;
            state->scrap += 1;
            state->stims += 1;
            break;
        default:
            break;
    }
}

static void enter_node(struct nc_game_state *state, int node_id)
{
    const struct nc_node *node;

    state->current_node_id = node_id;
    node = nc_story_get_node(node_id);
    if (node != NULL)
        apply_effect(state, &node->on_enter);
    save_write(state);
}

static int run_pause_menu(struct nc_game_state *state)
{
    enum nc_pause_result result = nc_ui_run_pause_menu();

    if (result == NC_PAUSE_PANEL)
    {
        nc_ui_show_panel(state);
        return 0;
    }
    if (result == NC_PAUSE_HELP)
    {
        nc_ui_show_help();
        return 0;
    }
    if (result == NC_PAUSE_TITLE)
        return 1;
    if (result == NC_PAUSE_EXIT)
        return 2;
    return 0;
}

static int run_story_loop(struct nc_game_state *state)
{
    struct nc_visible_choice visible_choices[NC_MAX_VISIBLE_CHOICES];
    int displayed_node_id = -1;
    bool radio_started = false;

    while (1)
    {
        const struct nc_node *node = nc_story_get_node(state->current_node_id);
        int result;
        int visible_count;

        if (node == NULL)
        {
            nc_ui_flash_message("nightcity", "Story node missing.", HZ * 2);
            return 1;
        }

        if (node->id != displayed_node_id)
        {
            nc_ui_transition();
            if (node->kind == NC_NODE_SCENE)
            {
                start_radio_shuffle_once(&radio_started);
                nc_ui_story_intro(state, node);
            }
            displayed_node_id = node->id;
        }

        if (node->kind == NC_NODE_ENDING)
        {
            nc_ui_show_ending(state, node);
            save_clear();
            return 0;
        }

        if (node->kind == NC_NODE_ENCOUNTER)
        {
            const struct nc_enemy *enemy = nc_story_get_enemy(node->encounter_id);

            result = nc_run_encounter(state, enemy);
            if (result == NC_ENCOUNTER_PANEL)
            {
                nc_ui_show_panel(state);
                continue;
            }
            if (result == NC_ENCOUNTER_MENU)
            {
                result = run_pause_menu(state);
                if (result == 1)
                    return 0;
                if (result == 2)
                    return 1;
                continue;
            }
            if (result == NC_ENCOUNTER_EXIT)
                return 1;
            if (result == NC_ENCOUNTER_WIN)
            {
                if (node->next_id == NC_DYNAMIC_ENDING)
                    enter_node(state, nc_story_choose_ending(state));
                else
                    enter_node(state, node->next_id);
            }
            else
            {
                int fail_id = node->fail_id;
                if (fail_id == NC_DYNAMIC_ENDING)
                    fail_id = nc_story_choose_ending(state);
                enter_node(state, fail_id);
            }
            continue;
        }

        visible_count = build_visible_choices(state, node, visible_choices);
        result = nc_ui_run_scene(state, node, visible_choices, visible_count);

        if (result == NC_SCENE_PANEL)
        {
            nc_ui_show_panel(state);
            continue;
        }
        if (result == NC_SCENE_MENU)
        {
            result = run_pause_menu(state);
            if (result == 1)
                return 0;
            if (result == 2)
                return 1;
            continue;
        }
        if (result == NC_SCENE_EXIT)
            return 1;

        if (result >= 0 && result < visible_count)
        {
            const struct nc_choice *choice = visible_choices[result].choice;
            apply_effect(state, &choice->effect);
            enter_node(state, choice->next_id);
            continue;
        }

        if (node->next_id == NC_DYNAMIC_ENDING)
            enter_node(state, nc_story_choose_ending(state));
        else if (node->next_id >= 0)
            enter_node(state, node->next_id);
        else
            return 0;
    }
}

static int start_new_game(struct nc_game_state *state)
{
    enum nc_lifepath lifepath = nc_ui_choose_lifepath();
    enum nc_gender gender;
    enum nc_profile profile;

    if (lifepath == NC_LIFEPATH_NONE)
        return 0;

    gender = nc_ui_choose_gender();
    if (gender == NC_GENDER_NONE)
        return 0;

    profile = nc_ui_choose_profile();
    if (profile == NC_PROFILE_NONE)
        return 0;

    nc_story_start_run(state, lifepath);
    state->gender = gender;
    if (gender == NC_GENDER_MASC)
        state->flags |= NC_FLAG_GENDER_MASC;
    else if (gender == NC_GENDER_FEMME)
        state->flags |= NC_FLAG_GENDER_FEMME;
    else if (gender == NC_GENDER_NONBINARY)
        state->flags |= NC_FLAG_GENDER_NONBINARY;
    apply_profile_bonus(state, profile);
    clamp_state(state);
    enter_node(state, state->current_node_id);
    return run_story_loop(state);
}

enum plugin_status nc_engine_run(void)
{
    struct nc_game_state state;
    bool quitting = false;

    rb->srand(*rb->current_tick);
    nc_ui_init();

    while (!quitting)
    {
        int title_result = nc_ui_run_title(save_exists());

        switch (title_result)
        {
            case 0:
                if (start_new_game(&state) == 1)
                    quitting = true;
                break;
            case 1:
                if (save_load(&state))
                {
                    if (run_story_loop(&state) == 1)
                        quitting = true;
                }
                else
                {
                    nc_ui_flash_message("Continue", "No valid save found.", HZ * 2);
                }
                break;
            case 2:
                nc_ui_show_help();
                break;
            default:
                quitting = true;
                break;
        }
    }

    return PLUGIN_OK;
}
