#include "pocketcatch.h"

#define PC_FP_SHIFT 8
#define PC_FP_ONE   (1 << PC_FP_SHIFT)

static int lerp_int(int start, int end, int t, int duration)
{
    if (duration <= 0)
        return end;
    return start + ((end - start) * t) / duration;
}

static int parabola_q8(int t, int duration)
{
    int denom;

    if (duration <= 0)
        return 0;

    denom = duration * duration;
    return (4 * t * (duration - t) * PC_FP_ONE) / denom;
}

static int tier_from_ring(const struct pc_ring_state *ring)
{
    int span = MAX(1, ring->max_radius - ring->min_radius);
    int inner = ring->min_radius + span / 3;
    int mid = ring->min_radius + (span * 2) / 3;

    if (ring->radius <= inner)
        return PC_THROW_TIER_EXCELLENT;
    if (ring->radius <= mid)
        return PC_THROW_TIER_GREAT;
    return PC_THROW_TIER_NICE;
}

static bool point_hits_creature(const struct pc_creature_def *creature,
                                int dx, int dy)
{
    long scaled_x = (long)dx * dx * creature->hit_radius_y * creature->hit_radius_y;
    long scaled_y = (long)dy * dy * creature->hit_radius_x * creature->hit_radius_x;
    long limit = (long)creature->hit_radius_x * creature->hit_radius_x *
                 creature->hit_radius_y * creature->hit_radius_y;
    return scaled_x + scaled_y <= limit;
}

void pc_physics_build_throw(struct pc_throw_state *throw_state,
                            const struct pc_creature_def *creature,
                            const struct pc_ring_state *ring,
                            const struct pc_throw_request *request)
{
    int hold_score;
    int power_score;
    int spin_score;
    int overspin_error;
    int power_error;
    int curve_px;
    int target_y;

    rb->memset(throw_state, 0, sizeof(*throw_state));
    if (!request->valid)
        return;

    hold_score = MIN(28, request->hold_ticks * 20 / HZ);
    power_score = MIN(100, 18 + hold_score + request->charge / 2 +
                      request->release_velocity * 3 / 2 + request->wheel_events * 2);
    spin_score = MIN(100, PC_ABS(request->signed_spin) * 10 + request->total_spin * 4);
    overspin_error = MAX(0, spin_score - 76);
    power_error = 68 - power_score;
    curve_px = request->signed_spin * 6;

    target_y = PC_TARGET_Y + creature->target_y_offset;

    throw_state->active = true;
    throw_state->start_x = PC_BALL_HOME_X;
    throw_state->start_y = PC_BALL_HOME_Y - 10;
    throw_state->end_x = PC_TARGET_X +
                         (request->signed_spin >= 0 ? 1 : -1) * (overspin_error / 2);
    if (request->signed_spin == 0)
        throw_state->end_x = PC_TARGET_X;
    throw_state->end_x += request->release_bias_x;
    throw_state->end_y = target_y + power_error / 2 + request->release_bias_y;
    throw_state->curve_px = MAX(-34, MIN(34, curve_px));
    throw_state->arc_height = 40 + power_score / 3;
    throw_state->duration_frames = MAX(12, 22 - (request->release_velocity / 2));
    throw_state->power_score = power_score;
    throw_state->spin_score = spin_score;
    throw_state->travel_score = request->release_velocity * 4;
    throw_state->spin_phase = request->spin_phase;
    throw_state->spin_velocity = request->spin_velocity;
    throw_state->curve_bonus = request->total_spin >= 5 &&
                               PC_ABS(request->signed_spin) * 100 / MAX(1, request->total_spin) >= 55;
    throw_state->x = throw_state->start_x;
    throw_state->y = throw_state->start_y;
    throw_state->frame = 0;

    throw_state->impact_dx = throw_state->end_x - PC_TARGET_X;
    throw_state->impact_dy = throw_state->end_y - target_y;
    throw_state->hit = point_hits_creature(creature,
                                           throw_state->impact_dx,
                                           throw_state->impact_dy);
    throw_state->tier = throw_state->hit ? PC_THROW_TIER_HIT : PC_THROW_TIER_MISS;

    if (throw_state->hit &&
        PC_ABS(throw_state->impact_dx) <= ring->radius &&
        PC_ABS(throw_state->impact_dy) <= ring->radius)
    {
        throw_state->tier = tier_from_ring(ring);
    }
}

void pc_physics_update_throw(struct pc_throw_state *throw_state,
                             const struct pc_creature_def *creature,
                             const struct pc_ring_state *ring)
{
    int t;
    int quad_q8;
    (void)creature;
    (void)ring;

    if (!throw_state->active || throw_state->finished)
        return;

    if (throw_state->frame >= throw_state->duration_frames)
    {
        throw_state->x = throw_state->end_x;
        throw_state->y = throw_state->end_y;
        throw_state->finished = true;
        return;
    }

    t = throw_state->frame;
    quad_q8 = parabola_q8(t, throw_state->duration_frames);
    throw_state->x = lerp_int(throw_state->start_x, throw_state->end_x,
                              t, throw_state->duration_frames) +
                     ((throw_state->curve_px * quad_q8) >> PC_FP_SHIFT);
    throw_state->y = lerp_int(throw_state->start_y, throw_state->end_y,
                              t, throw_state->duration_frames) -
                     ((throw_state->arc_height * quad_q8) >> PC_FP_SHIFT);
    throw_state->spin_phase += throw_state->spin_velocity;
    throw_state->frame++;
}
