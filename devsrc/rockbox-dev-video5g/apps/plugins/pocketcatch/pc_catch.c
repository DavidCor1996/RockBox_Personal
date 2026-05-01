#include "pocketcatch.h"

static int tier_bonus(enum pc_throw_tier tier)
{
    switch (tier)
    {
        case PC_THROW_TIER_NICE:
            return 80;
        case PC_THROW_TIER_GREAT:
            return 150;
        case PC_THROW_TIER_EXCELLENT:
            return 240;
        default:
            return 20;
    }
}

enum pc_catch_outcome pc_catch_roll(const struct pc_creature_def *creature,
                                    const struct pc_throw_state *throw_state,
                                    int *chance_out,
                                    int *breakout_after_shake)
{
    int chance;
    int velocity_bonus;
    int roll;

    if (!throw_state->hit)
    {
        if (chance_out)
            *chance_out = 0;
        if (breakout_after_shake)
            *breakout_after_shake = 0;
        return PC_CATCH_OUTCOME_MISS;
    }

    velocity_bonus = throw_state->travel_score >= 40 &&
                     throw_state->travel_score <= 76 ? 50 : 0;
    chance = creature->base_capture_rate + tier_bonus(throw_state->tier) +
             (throw_state->curve_bonus ? 90 : 0) + velocity_bonus;
    chance = MAX(80, MIN(940, chance));
    roll = rb->rand() % 1000;

    if (chance_out)
        *chance_out = chance;

    if (roll < chance)
    {
        if (breakout_after_shake)
            *breakout_after_shake = 3;
        return PC_CATCH_OUTCOME_CAUGHT;
    }

    if (breakout_after_shake)
        *breakout_after_shake = 1 + (rb->rand() % 3);
    return PC_CATCH_OUTCOME_BREAKOUT;
}

const char *pc_throw_tier_label(enum pc_throw_tier tier)
{
    switch (tier)
    {
        case PC_THROW_TIER_NICE:
            return "Nice";
        case PC_THROW_TIER_GREAT:
            return "Great";
        case PC_THROW_TIER_EXCELLENT:
            return "Excellent";
        case PC_THROW_TIER_HIT:
            return "Hit";
        default:
            return "Miss";
    }
}
