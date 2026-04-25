#include "pocketcatch.h"

static void set_banner(struct pc_encounter_state *state,
                       const char *line1, const char *line2)
{
    rb->strlcpy(state->banner.line1, line1, sizeof(state->banner.line1));
    rb->strlcpy(state->banner.line2, line2, sizeof(state->banner.line2));
}

static void set_intro_banner(struct pc_encounter_state *state)
{
    char line1[PC_BANNER_LINE_CHARS];

    rb->snprintf(line1, sizeof(line1), "A wild %s appeared!", state->creature->name);
    set_banner(state, line1, "Spin the wheel, then let go to throw");
}

static void reset_ring(struct pc_ring_state *ring)
{
    ring->min_radius = 14;
    ring->max_radius = 42;
    ring->period_frames = 48;
    ring->radius = ring->max_radius;
}

static void update_ring(struct pc_encounter_state *state)
{
    int phase;
    int half;
    int span;

    half = MAX(1, state->ring.period_frames / 2);
    span = state->ring.max_radius - state->ring.min_radius;
    phase = state->total_frames % state->ring.period_frames;

    if (phase < half)
        state->ring.radius = state->ring.max_radius - (span * phase) / half;
    else
        state->ring.radius = state->ring.min_radius + (span * (phase - half)) / half;
}

static int pick_random_species_index(int current_index)
{
    int count = pc_assets_get_total_creature_count();
    int next_index;

    if (count <= 1)
        return 0;

    next_index = rb->rand() % count;
    if (next_index == current_index)
        next_index = (next_index + 1 + (rb->rand() % (count - 1))) % count;
    return next_index;
}

static void reset_encounter(struct pc_encounter_state *state)
{
    pc_input_reset(&state->input);
    rb->memset(&state->throw_state, 0, sizeof(state->throw_state));

    state->creature = pc_assets_select_creature(&state->assets, state->species_index);
    state->phase = PC_PHASE_INTRO;
    state->phase_frame = 0;
    state->total_frames = 0;
    state->last_tier = PC_THROW_TIER_MISS;
    state->outcome = PC_CATCH_OUTCOME_NONE;
    state->last_catch_chance = 0;
    state->breakout_after_shake = 0;
    state->shake_offset = 0;
    state->finished = false;
    reset_ring(&state->ring);
    set_intro_banner(state);
}

static void begin_shake_phase(struct pc_encounter_state *state, enum pc_phase phase)
{
    state->phase = phase;
    state->phase_frame = 0;
}

static void maybe_beep(enum pc_phase phase, enum pc_throw_tier tier,
                       enum pc_catch_outcome outcome)
{
    switch (phase)
    {
        case PC_PHASE_BALL_THROWN:
            rb->beep_play(960, 2, 800);
            break;
        case PC_PHASE_HIT_RESOLVE:
            rb->beep_play(tier >= PC_THROW_TIER_GREAT ? 1550 : 1250, 3, 900);
            break;
        case PC_PHASE_CAUGHT:
            rb->beep_play(1750, 5, 900);
            break;
        case PC_PHASE_BROKE_OUT:
            if (outcome == PC_CATCH_OUTCOME_BREAKOUT)
                rb->beep_play(720, 4, 900);
            break;
        default:
            break;
    }
}

void pc_state_init(struct pc_encounter_state *state, bool simulator_debug)
{
    rb->memset(state, 0, sizeof(*state));
    state->simulator_debug = simulator_debug;
    pc_assets_init(&state->assets);
    state->species_index = pick_random_species_index(-1);
    reset_encounter(state);
}

void pc_state_begin(struct pc_encounter_state *state, int species_index)
{
    int count = pc_assets_get_total_creature_count();

    if (count <= 0)
        species_index = 0;
    else
    {
        species_index %= count;
        if (species_index < 0)
            species_index += count;
    }

    state->species_index = species_index;
    reset_encounter(state);
}

void pc_state_cycle_species(struct pc_encounter_state *state, int delta)
{
    int count = pc_assets_get_total_creature_count();

    if (count <= 0)
        return;

    state->species_index = (state->species_index + delta) % count;
    if (state->species_index < 0)
        state->species_index += count;
    reset_encounter(state);
}

void pc_state_update(struct pc_encounter_state *state,
                     const struct pc_input_command *command,
                     const struct pc_throw_request *throw_request)
{
    char line2[PC_BANNER_LINE_CHARS];

    if (command->prev_species)
    {
        pc_state_cycle_species(state, -1);
        return;
    }

    if (command->next_species)
    {
        pc_state_cycle_species(state, 1);
        return;
    }

    state->total_frames++;
    state->phase_frame++;

    if (state->phase <= PC_PHASE_BALL_THROWN)
        update_ring(state);

    switch (state->phase)
    {
        case PC_PHASE_INTRO:
            if (state->phase_frame >= PC_INTRO_FRAMES)
            {
                state->phase = PC_PHASE_IDLE_READY;
                state->phase_frame = 0;
                set_banner(state, state->creature->name,
                           "Spin the wheel to grab the ball");
            }
            break;

        case PC_PHASE_IDLE_READY:
            if (command->start_grab)
            {
                state->phase = PC_PHASE_BALL_HELD;
                state->phase_frame = 0;
                set_banner(state, "Keep spinning for curve and speed",
                           "Lift your finger to throw");
            }
            break;

        case PC_PHASE_BALL_HELD:
            if (throw_request->valid)
            {
                pc_physics_build_throw(&state->throw_state, state->creature,
                                       &state->ring, throw_request);
                state->phase = PC_PHASE_BALL_THROWN;
                state->phase_frame = 0;
                rb->snprintf(line2, sizeof(line2), "Power %d  Curve %s",
                             state->throw_state.power_score,
                             state->throw_state.curve_bonus ? "on" : "off");
                set_banner(state, "Throw!", line2);
                maybe_beep(state->phase, PC_THROW_TIER_MISS, PC_CATCH_OUTCOME_NONE);
            }
            break;

        case PC_PHASE_BALL_THROWN:
            pc_physics_update_throw(&state->throw_state, state->creature, &state->ring);
            if (state->throw_state.finished)
            {
                state->last_tier = state->throw_state.tier;
                if (state->throw_state.hit)
                {
                    state->phase = PC_PHASE_HIT_RESOLVE;
                    state->phase_frame = 0;
                    state->outcome = pc_catch_roll(state->creature, &state->throw_state,
                                                   &state->last_catch_chance,
                                                   &state->breakout_after_shake);
                    rb->snprintf(line2, sizeof(line2), "%s throw%s",
                                 pc_throw_tier_label(state->last_tier),
                                 state->throw_state.curve_bonus ? "  Curve bonus" : "");
                    set_banner(state, state->creature->name, line2);
                    maybe_beep(state->phase, state->last_tier, state->outcome);
                }
                else
                {
                    state->outcome = PC_CATCH_OUTCOME_MISS;
                    state->phase = PC_PHASE_RESULT;
                    state->phase_frame = 0;
                    set_banner(state, "Missed the creature",
                               "Spin again for another encounter");
                }
            }
            break;

        case PC_PHASE_HIT_RESOLVE:
            if (state->phase_frame >= PC_HIT_FRAMES)
            {
                begin_shake_phase(state, PC_PHASE_SHAKE_1);
            }
            break;

        case PC_PHASE_SHAKE_1:
            state->shake_offset = (state->phase_frame & 1) ? -5 : 5;
            if (state->phase_frame >= PC_SHAKE_FRAMES)
            {
                if (state->outcome == PC_CATCH_OUTCOME_BREAKOUT &&
                    state->breakout_after_shake == 1)
                {
                    state->phase = PC_PHASE_BROKE_OUT;
                    state->phase_frame = 0;
                    set_banner(state, "Broke out!", "Same species, quick retry ready");
                    maybe_beep(state->phase, state->last_tier, state->outcome);
                }
                else
                {
                    begin_shake_phase(state, PC_PHASE_SHAKE_2);
                }
            }
            break;

        case PC_PHASE_SHAKE_2:
            state->shake_offset = (state->phase_frame & 1) ? 6 : -6;
            if (state->phase_frame >= PC_SHAKE_FRAMES)
            {
                if (state->outcome == PC_CATCH_OUTCOME_BREAKOUT &&
                    state->breakout_after_shake == 2)
                {
                    state->phase = PC_PHASE_BROKE_OUT;
                    state->phase_frame = 0;
                    set_banner(state, "Broke out!", "Same species, quick retry ready");
                    maybe_beep(state->phase, state->last_tier, state->outcome);
                }
                else
                {
                    begin_shake_phase(state, PC_PHASE_SHAKE_3);
                }
            }
            break;

        case PC_PHASE_SHAKE_3:
            state->shake_offset = (state->phase_frame & 1) ? -4 : 4;
            if (state->phase_frame >= PC_SHAKE_FRAMES)
            {
                state->shake_offset = 0;
                if (state->outcome == PC_CATCH_OUTCOME_CAUGHT)
                {
                    state->phase = PC_PHASE_CAUGHT;
                    state->phase_frame = 0;
                    set_banner(state, "Caught!", "Spin again to jump into the next test");
                    maybe_beep(state->phase, state->last_tier, state->outcome);
                }
                else
                {
                    state->phase = PC_PHASE_BROKE_OUT;
                    state->phase_frame = 0;
                    set_banner(state, "Broke out!", "Same species, quick retry ready");
                    maybe_beep(state->phase, state->last_tier, state->outcome);
                }
            }
            break;

        case PC_PHASE_CAUGHT:
        case PC_PHASE_BROKE_OUT:
            if (state->phase_frame >= PC_RESULT_HOLD_FRAMES)
            {
                state->finished = true;
            }
            break;

        case PC_PHASE_RESULT:
            if (command->start_grab || state->phase_frame >= PC_AUTO_RESET_FRAMES)
            {
                state->finished = true;
            }
            break;

        case PC_PHASE_RESET_NEXT:
            state->species_index = pick_random_species_index(state->species_index);
            reset_encounter(state);
            break;
    }
}
