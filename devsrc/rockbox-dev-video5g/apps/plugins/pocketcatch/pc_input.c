#include "pocketcatch.h"

static void clear_command(struct pc_input_command *command)
{
    rb->memset(command, 0, sizeof(*command));
}

static void clear_request(struct pc_throw_request *request)
{
    rb->memset(request, 0, sizeof(*request));
}

static int decay_signed_value(int value, int amount)
{
    if (value > 0)
        return MAX(0, value - amount);
    if (value < 0)
        return MIN(0, value + amount);
    return 0;
}

static int wheel_delta_from_event(long event)
{
    int step = (event & BUTTON_REPEAT) ? 2 : 1;

    if (event & BUTTON_SCROLL_FWD)
        return step;
    if (event & BUTTON_SCROLL_BACK)
        return -step;
    return 0;
}

static void apply_wheel_event(struct pc_input_state *input, int delta, long now)
{
    long dt = now - input->last_wheel_tick;
    int speed;
    int velocity;

    if (dt <= 0)
        dt = 1;

    speed = MIN(24, (HZ * (PC_ABS(delta) + 1)) / dt);
    velocity = MIN(24, speed + PC_ABS(delta) * 3);

    input->signed_spin += delta;
    input->total_spin += PC_ABS(delta);
    input->spin_energy += 6 + speed + PC_ABS(delta) * 2;
    input->release_velocity = speed;
    input->wheel_events += PC_ABS(delta);
    input->last_wheel_dir = delta > 0 ? 1 : -1;
    input->prev_wheel_tick = input->last_wheel_tick;
    input->last_wheel_tick = now;
    input->spin_velocity = delta > 0 ? velocity : -velocity;
    input->spin_phase += input->spin_velocity;
}

static bool wheel_release_detected(const struct pc_input_state *input, long now)
{
    long idle_ticks = now - input->last_wheel_tick;

    if (!input->grabbing || input->wheel_events <= 0)
        return false;

    return idle_ticks >= PC_WHEEL_RELEASE_TICKS;
}

static void compute_release_bias(int spin_phase, int *bias_x, int *bias_y)
{
    static const signed char bias_table_x[8] = { 4, 3, 0, -3, -4, -3, 0, 3 };
    static const signed char bias_table_y[8] = { 0, 2, 4, 2, 0, -2, -4, -2 };
    int sector = (spin_phase >> 5) & 7;

    *bias_x = bias_table_x[sector];
    *bias_y = bias_table_y[sector];
}

void pc_input_reset(struct pc_input_state *input)
{
    rb->memset(input, 0, sizeof(*input));
}

void pc_input_animate(struct pc_input_state *input, long now)
{
    long idle_ticks;

    if (!input->grabbing || input->spin_velocity == 0)
        return;

    idle_ticks = now - input->last_wheel_tick;
    if (idle_ticks < 0)
        idle_ticks = 0;

    input->spin_phase += input->spin_velocity;

    if (idle_ticks > 0)
        input->spin_velocity = decay_signed_value(input->spin_velocity, 1);
}

void pc_input_snapshot_throw(const struct pc_input_state *input, long now,
                             struct pc_throw_request *request)
{
    int release_velocity;
    int spin_velocity;
    long decay_ticks;

    clear_request(request);
    if (!input->grabbing)
        return;

    release_velocity = input->release_velocity;
    spin_velocity = input->spin_velocity;
    decay_ticks = now - input->last_wheel_tick;
    if (decay_ticks > 0)
    {
        release_velocity = MAX(0, release_velocity - (int)decay_ticks * 4);
        spin_velocity = decay_signed_value(spin_velocity, (int)decay_ticks * 3);
    }

    request->valid = true;
    request->hold_ticks = (int)(now - input->hold_start_tick);
    request->signed_spin = input->signed_spin;
    request->total_spin = input->total_spin;
    request->charge = MIN(120, input->spin_energy + request->hold_ticks * 8 / HZ);
    request->release_velocity = release_velocity;
    request->wheel_events = input->wheel_events;
    request->spin_phase = input->spin_phase;
    request->spin_velocity = spin_velocity;
    compute_release_bias(request->spin_phase,
                         &request->release_bias_x,
                         &request->release_bias_y);
}

void pc_input_handle_event(struct pc_input_state *input, long event, long now,
                           struct pc_input_command *command,
                           struct pc_throw_request *throw_request)
{
    int wheel_delta;

    clear_command(command);
    clear_request(throw_request);

    if ((event & BUTTON_MENU) && !(event & (BUTTON_REPEAT | BUTTON_REL)))
        command->exit_requested = true;

#ifdef SIMULATOR
    if ((event & BUTTON_LEFT) && !(event & (BUTTON_REPEAT | BUTTON_REL)))
        command->prev_species = true;
    if ((event & BUTTON_RIGHT) && !(event & (BUTTON_REPEAT | BUTTON_REL)))
        command->next_species = true;
#endif

    wheel_delta = wheel_delta_from_event(event);
    if (wheel_delta != 0)
    {
        if (!input->grabbing)
        {
            pc_input_reset(input);
            input->grabbing = true;
            input->hold_start_tick = now;
            input->last_wheel_tick = now;
            input->prev_wheel_tick = now;
            command->start_grab = true;
        }

        apply_wheel_event(input, wheel_delta, now);
    }

    if (wheel_release_detected(input, now))
    {
        pc_input_snapshot_throw(input, now, throw_request);
        input->grabbing = false;
    }
}
