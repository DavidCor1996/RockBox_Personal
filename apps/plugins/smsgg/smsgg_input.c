#include "plugin.h"
#include "smsgg_input.h"
#include "smsgg_haptics.h"

#define SMSGG_LONG_PRESS_TICKS (HZ / 2)

static long select_down_tick;
static long play_down_tick;
static bool start_combo_latched;
static bool hold_exit_armed;

void smsgg_input_enable_wheel_events(bool enabled)
{
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(enabled);
#else
    (void)enabled;
#endif
}

void smsgg_input_init(void)
{
    select_down_tick = 0;
    play_down_tick = 0;
    start_combo_latched = false;
    hold_exit_armed = false;
    smsgg_input_enable_wheel_events(false);
    rb->button_clear_queue();
}

void smsgg_input_shutdown(void)
{
    smsgg_input_enable_wheel_events(true);
}

static bool long_held(int clean, int mask, long *down_tick, long now)
{
    if ((clean & mask) == 0)
    {
        *down_tick = 0;
        return false;
    }

    if (*down_tick == 0)
        *down_tick = now;

    return now - *down_tick >= SMSGG_LONG_PRESS_TICKS;
}

static bool touch_controls_enabled(enum smsgg_control_preset preset,
                                   enum smsgg_wheel_mode wheel_mode)
{
#ifdef HAVE_WHEEL_POSITION
    return preset != SMSGG_PRESET_CLASSIC &&
           preset != SMSGG_PRESET_MENU_SAFE &&
           wheel_mode != SMSGG_WHEEL_DISABLED;
#else
    (void)preset;
    (void)wheel_mode;
    return false;
#endif
}

#ifdef HAVE_WHEEL_POSITION
static int wheel_delta(int current, int previous)
{
    int delta = current - previous;

    if (delta > 48)
        delta -= 96;
    else if (delta < -48)
        delta += 96;

    return delta;
}

static uint8 wheel_position_to_pad(int position,
                                   enum smsgg_wheel_mode wheel_mode)
{
    if (position < 0)
        return 0;

    if (wheel_mode == SMSGG_WHEEL_4WAY)
    {
        int zone = ((position + 12) / 24) & 3;

        switch (zone)
        {
            case 0:
                return INPUT_UP;
            case 1:
                return INPUT_RIGHT;
            case 2:
                return INPUT_DOWN;
            case 3:
                return INPUT_LEFT;
        }
    }
    else if (wheel_mode == SMSGG_WHEEL_ANALOG_8WAY)
    {
        int zone = (position + 6) / 12;

        if (zone > 7)
            zone = 0;

        switch (zone)
        {
            case 0:
                return INPUT_UP;
            case 1:
                return INPUT_UP | INPUT_RIGHT;
            case 2:
                return INPUT_RIGHT;
            case 3:
                return INPUT_DOWN | INPUT_RIGHT;
            case 4:
                return INPUT_DOWN;
            case 5:
                return INPUT_DOWN | INPUT_LEFT;
            case 6:
                return INPUT_LEFT;
            case 7:
                return INPUT_UP | INPUT_LEFT;
        }
    }

    return 0;
}
#endif

static void apply_button(struct smsgg_input_state *state, int button,
                         int event, long now,
                         enum smsgg_control_preset preset,
                         enum smsgg_wheel_mode wheel_mode)
{
    int clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
    int event_clean = event & ~(BUTTON_REPEAT | BUTTON_REL);
    bool select_long = false;
    bool touch_controls = touch_controls_enabled(preset, wheel_mode);

#ifdef HAS_BUTTON_HOLD
    if (rb->button_hold())
    {
        if (!hold_exit_armed)
        {
            hold_exit_armed = true;
            state->quit_requested = true;
        }
        return;
    }
    hold_exit_armed = false;
#endif

#ifdef BUTTON_SELECT
    select_long = long_held(clean, BUTTON_SELECT, &select_down_tick, now);
#endif
#ifdef BUTTON_PLAY
    (void)long_held(clean, BUTTON_PLAY, &play_down_tick, now);
#endif

#if defined(BUTTON_MENU) && defined(BUTTON_SELECT)
    if ((clean & BUTTON_MENU) && (clean & BUTTON_SELECT))
    {
        state->menu_requested = true;
        return;
    }
#endif

    if (touch_controls)
    {
#ifdef BUTTON_LEFT
        if (clean & BUTTON_LEFT)
            state->pad |= INPUT_LEFT;
#endif
#ifdef BUTTON_RIGHT
        if (clean & BUTTON_RIGHT)
            state->pad |= INPUT_RIGHT;
#endif
#ifdef BUTTON_MENU
        if (clean & BUTTON_MENU)
        {
            state->system |= INPUT_START | INPUT_PAUSE;
            smsgg_haptic_pause();
        }
#endif
#ifdef BUTTON_PLAY
        if (clean & BUTTON_PLAY)
            state->pad |= INPUT_BUTTON2;
#endif
#ifdef BUTTON_SELECT
        if (clean & BUTTON_SELECT)
            state->pad |= INPUT_BUTTON1;
#endif

        if ((state->pad & (INPUT_BUTTON1 | INPUT_BUTTON2)) != 0)
            smsgg_haptic_button();
        return;
    }

#if defined(BUTTON_SELECT) && defined(BUTTON_PLAY)
    if ((clean & BUTTON_SELECT) && (clean & BUTTON_PLAY))
    {
        long diff = select_down_tick - play_down_tick;

        if (diff < 0)
            diff = -diff;

        if (diff <= HZ / 5)
        {
            if (!start_combo_latched)
            {
                state->system |= INPUT_START | INPUT_PAUSE;
                smsgg_haptic_pause();
                start_combo_latched = true;
            }
            return;
        }
    }
    else
    {
        start_combo_latched = false;
    }
#endif

#ifdef BUTTON_LEFT
    if (clean & BUTTON_LEFT)
        state->pad |= INPUT_LEFT;
#endif
#ifdef BUTTON_RIGHT
    if (clean & BUTTON_RIGHT)
        state->pad |= INPUT_RIGHT;
#endif
#ifdef BUTTON_MENU
    if (clean & BUTTON_MENU)
        state->pad |= INPUT_UP;
#endif
#ifdef BUTTON_PLAY
    if (clean & BUTTON_PLAY)
    {
        if (preset == SMSGG_PRESET_IPOD)
            state->pad |= INPUT_BUTTON2;
        else
            state->pad |= INPUT_DOWN;
    }
#endif
#ifdef BUTTON_SELECT
    if (clean & BUTTON_SELECT)
    {
        if (select_long ||
            ((event_clean & BUTTON_SELECT) && (event & BUTTON_REPEAT)))
            state->pad |= INPUT_BUTTON2;
        else
            state->pad |= INPUT_BUTTON1;
    }
#endif

    if ((state->pad & (INPUT_BUTTON1 | INPUT_BUTTON2)) != 0)
        smsgg_haptic_button();
}

static void apply_wheel(struct smsgg_input_state *state,
                        enum smsgg_control_preset preset,
                        enum smsgg_wheel_mode wheel_mode,
                        int sensitivity, int deadzone)
{
    int threshold = 1 + deadzone + sensitivity;
#ifdef HAVE_WHEEL_POSITION
    static int last_wheel = -1;
    static int last_zone = -1;
#endif

    if (threshold < 1)
        threshold = 1;

    if (preset == SMSGG_PRESET_MENU_SAFE ||
        wheel_mode == SMSGG_WHEEL_DISABLED)
        return;

#ifdef HAVE_WHEEL_POSITION
    {
        int wheel = rb->wheel_status();

        if (wheel < 0)
        {
            last_wheel = -1;
            last_zone = -1;
        }
        else
        {
            if (last_wheel >= 0)
                state->wheel_delta = wheel_delta(wheel, last_wheel);
            last_wheel = wheel;
        }

        if (wheel >= 0 &&
            (wheel_mode == SMSGG_WHEEL_4WAY ||
             wheel_mode == SMSGG_WHEEL_ANALOG_8WAY))
        {
            uint8 wheel_pad = wheel_position_to_pad(wheel, wheel_mode);
            int zone = (wheel_mode == SMSGG_WHEEL_4WAY) ?
                       (((wheel + 12) / 24) & 3) :
                       ((wheel + 6) / 12);

            if (zone > 7)
                zone = 0;

            state->wheel_touched = true;
            state->wheel_zone = zone;
            state->pad |= wheel_pad;

            if (zone != last_zone)
            {
                smsgg_haptic_menu();
                last_zone = zone;
            }
            return;
        }
    }
#endif

#ifdef BUTTON_SCROLL_FWD
    if (state->held_button & BUTTON_SCROLL_FWD)
        state->wheel_delta++;
#endif
#ifdef BUTTON_SCROLL_BACK
    if (state->held_button & BUTTON_SCROLL_BACK)
        state->wheel_delta--;
#endif

    if (state->wheel_delta > threshold)
    {
        state->wheel_touched = true;
        state->wheel_zone = 1;
        if (wheel_mode != SMSGG_WHEEL_VERTICAL)
            state->pad |= INPUT_RIGHT;
        smsgg_haptic_menu();
    }
    else if (state->wheel_delta < -threshold)
    {
        state->wheel_touched = true;
        state->wheel_zone = 3;
        if (wheel_mode != SMSGG_WHEEL_VERTICAL)
            state->pad |= INPUT_LEFT;
        smsgg_haptic_menu();
    }
}

void smsgg_input_poll(struct smsgg_input_state *state,
                      enum smsgg_control_preset preset,
                      enum smsgg_wheel_mode wheel_mode,
                      int sensitivity, int deadzone)
{
    int button = rb->button_get(false);
    int held = rb->button_status();
    long now = *rb->current_tick;

    rb->memset(state, 0, sizeof(*state));
    state->raw_button = button;
    state->held_button = held;

    if (IS_SYSEVENT(button))
    {
        if (button == SYS_USB_CONNECTED)
        {
            state->quit_requested = true;
            state->usb_requested = true;
        }
        return;
    }

    apply_button(state, held, button, now, preset, wheel_mode);
    apply_wheel(state, preset, wheel_mode, sensitivity, deadzone);
}

uint8 smsgg_input_get_buttons(const struct smsgg_input_state *state)
{
    return state->pad;
}

int smsgg_input_get_wheel_delta(const struct smsgg_input_state *state)
{
    return state->wheel_delta;
}

int smsgg_input_get_wheel_zone(const struct smsgg_input_state *state)
{
    return state->wheel_zone;
}

bool smsgg_input_is_wheel_touched(const struct smsgg_input_state *state)
{
    return state->wheel_touched;
}
