#include "tamagotchi_input.h"
#include "tamagotchi_display.h"
#include "tamagotchi_haptics.h"
#include "lib/plugin_notifications.h"

static long a_until;
static long b_until;
static long c_until;
static int a_queued;
static int b_queued;
static int c_queued;
static int selected_icon;
static int stock_a_pending;
static int stock_b_pending;
static int stock_c_pending;

enum stock_mode
{
    STOCK_IDLE = 0,
    STOCK_FOOD_MENU,
    STOCK_LIGHT_MENU,
    STOCK_ACTIVE,
};

static enum stock_mode stock_mode;
static enum stock_mode stock_next_mode;

void tamagotchi_input_init(void)
{
    a_until = b_until = c_until = 0;
    a_queued = b_queued = c_queued = 0;
    selected_icon = 0;
    stock_a_pending = stock_b_pending = 0;
    stock_c_pending = 0;
    stock_mode = STOCK_IDLE;
    stock_next_mode = STOCK_IDLE;
}

static long pulse_len(void)
{
    return HZ / 10;
}

static long pulse_gap(void)
{
    return HZ / 18;
}

static void start_pulse(long *until)
{
    *until = *rb->current_tick + pulse_len();
    tamagotchi_haptic_button();
}

static void queue_pulse(long *until, int *queued)
{
    long now = *rb->current_tick;

    if (TIME_AFTER(now, *until + pulse_gap()))
        start_pulse(until);
    else if (*queued < 12)
        (*queued)++;
}

static void service_queued_pulse(long *until, int *queued)
{
    long now = *rb->current_tick;

    if (*queued > 0 && TIME_AFTER(now, *until + pulse_gap()))
    {
        (*queued)--;
        start_pulse(until);
    }
}

static bool pulse_busy(long now)
{
    return TIME_BEFORE(now, a_until) ||
           TIME_BEFORE(now, b_until) ||
           TIME_BEFORE(now, c_until);
}

static long latest_until(void)
{
    long latest = a_until;

    if (TIME_AFTER(b_until, latest))
        latest = b_until;
    if (TIME_AFTER(c_until, latest))
        latest = c_until;

    return latest;
}

static void service_stock_sequence(void)
{
    long now = *rb->current_tick;

    if (pulse_busy(now) || TIME_BEFORE(now, latest_until() + pulse_gap()))
        return;

    if (stock_c_pending > 0)
    {
        stock_c_pending--;
        start_pulse(&c_until);
    }
    else if (stock_a_pending > 0)
    {
        stock_a_pending--;
        start_pulse(&a_until);
    }
    else if (stock_b_pending > 0)
    {
        stock_b_pending--;
        start_pulse(&b_until);
    }
    else if (stock_next_mode != STOCK_IDLE)
    {
        stock_mode = stock_next_mode;
        stock_next_mode = STOCK_IDLE;
    }
}

static void move_icon(const struct tamagotchi_settings *settings, int delta)
{
    selected_icon += delta;
    while (selected_icon < 0)
        selected_icon += TAMA_ICON_COUNT;
    while (selected_icon >= TAMA_ICON_COUNT)
        selected_icon -= TAMA_ICON_COUNT;

    tamagotchi_display_set_selected_icon(selected_icon);

    if (settings->wheel_haptic_ticks)
        tamagotchi_haptic_wheel();
}

static void cancel_stock_sequence(void)
{
    stock_a_pending = stock_b_pending = stock_c_pending = 0;
    stock_next_mode = STOCK_IDLE;
}

static enum stock_mode stock_mode_after_open(void)
{
    switch (selected_icon)
    {
        case 0:
            return STOCK_FOOD_MENU;
        case 1:
            return STOCK_LIGHT_MENU;
        case 2:
        case 5:
            return STOCK_ACTIVE;
        default:
            return STOCK_IDLE;
    }
}

static void queue_stock_open(void)
{
    int steps = selected_icon + 1;

    cancel_stock_sequence();
    stock_a_pending = steps;
    stock_b_pending = 1;
    stock_c_pending = 1;
    stock_mode = STOCK_IDLE;
    stock_next_mode = stock_mode_after_open();
    a_queued = b_queued = c_queued = 0;
}

static void queue_stock_back(void)
{
    cancel_stock_sequence();
    stock_mode = STOCK_IDLE;
    queue_pulse(&c_until, &c_queued);
}

static void stock_cycle_choice(const struct tamagotchi_settings *settings)
{
    queue_pulse(&a_until, &a_queued);
    if (settings->wheel_haptic_ticks)
        tamagotchi_haptic_wheel();
}

static void stock_confirm_choice(void)
{
    queue_pulse(&b_until, &b_queued);
    if (stock_mode == STOCK_FOOD_MENU || stock_mode == STOCK_LIGHT_MENU)
        stock_mode = STOCK_IDLE;
}

void tamagotchi_input_poll(const struct tamagotchi_settings *settings,
                           struct tamagotchi_input_state *state)
{
    int button = rb->button_get(false);
    int held = rb->button_status();
    bool press = !(button & (BUTTON_REL | BUTTON_REPEAT));
    long now = *rb->current_tick;

    rb->memset(state, 0, sizeof(*state));
    state->selected_icon = selected_icon;
    service_queued_pulse(&a_until, &a_queued);
    service_queued_pulse(&b_until, &b_queued);
    service_queued_pulse(&c_until, &c_queued);
    service_stock_sequence();

    if (IS_SYSEVENT(button))
    {
        if (button == SYS_USB_CONNECTED)
        {
            state->quit_requested = true;
            state->usb_requested = true;
        }
        return;
    }

    if (plugin_notify_handle_button(button))
        return;

#if defined(BUTTON_MENU) && defined(BUTTON_SELECT)
    if ((held & BUTTON_MENU) && (held & BUTTON_SELECT))
    {
        cancel_stock_sequence();
        stock_mode = STOCK_IDLE;
        state->menu_requested = true;
        return;
    }
#endif

    if (settings->control_mode == TAMA_CONTROL_RAW_ABC)
    {
#ifdef BUTTON_LEFT
        if ((button & BUTTON_LEFT) && press)
            queue_pulse(&a_until, &a_queued);
#endif
#ifdef BUTTON_SELECT
        if ((button & BUTTON_SELECT) && press)
            queue_pulse(&b_until, &b_queued);
#endif
#if defined(BUTTON_RIGHT)
        if ((button & BUTTON_RIGHT) && press)
            queue_pulse(&c_until, &c_queued);
#endif
#ifdef BUTTON_PLAY
        if ((button & BUTTON_PLAY) && press)
            queue_pulse(&c_until, &c_queued);
#endif
    }
    else
    {
        if (stock_mode != STOCK_IDLE)
        {
#ifdef BUTTON_SCROLL_FWD
            if ((button & BUTTON_SCROLL_FWD) && !(button & BUTTON_REL))
                stock_cycle_choice(settings);
#endif
#ifdef BUTTON_SCROLL_BACK
            if ((button & BUTTON_SCROLL_BACK) && !(button & BUTTON_REL))
                stock_cycle_choice(settings);
#endif
#ifdef BUTTON_RIGHT
            if ((button & BUTTON_RIGHT) && press)
                stock_cycle_choice(settings);
#endif
#ifdef BUTTON_LEFT
            if ((button & BUTTON_LEFT) && press)
                stock_cycle_choice(settings);
#endif
#ifdef BUTTON_SELECT
            if ((button & BUTTON_SELECT) && press)
                stock_confirm_choice();
#endif
#ifdef BUTTON_PLAY
            if ((button & BUTTON_PLAY) && press)
                queue_stock_back();
#endif
#ifdef BUTTON_MENU
            if ((button & BUTTON_MENU) && press)
                queue_stock_back();
#endif
            goto finish;
        }

#ifdef BUTTON_SCROLL_FWD
        if ((button & BUTTON_SCROLL_FWD) && !(button & BUTTON_REL))
            move_icon(settings, 1);
#endif
#ifdef BUTTON_SCROLL_BACK
        if ((button & BUTTON_SCROLL_BACK) && !(button & BUTTON_REL))
            move_icon(settings, -1);
#endif
#ifdef BUTTON_RIGHT
        if ((button & BUTTON_RIGHT) && press)
            move_icon(settings, 1);
#endif
#ifdef BUTTON_LEFT
        if ((button & BUTTON_LEFT) && press)
            move_icon(settings, -1);
#endif
#ifdef BUTTON_SELECT
        if ((button & BUTTON_SELECT) && press)
            queue_stock_open();
#endif
#ifdef BUTTON_PLAY
        if ((button & BUTTON_PLAY) && press)
        {
            cancel_stock_sequence();
            queue_pulse(&c_until, &c_queued);
        }
#endif
#ifdef BUTTON_MENU
        if ((button & BUTTON_MENU) && press)
        {
            cancel_stock_sequence();
            queue_pulse(&c_until, &c_queued);
        }
#endif
    }

finish:
#ifdef BUTTON_LEFT
    if (settings->control_mode == TAMA_CONTROL_RAW_ABC && (held & BUTTON_LEFT))
        state->a = true;
#endif
#ifdef BUTTON_SELECT
    if (settings->control_mode == TAMA_CONTROL_RAW_ABC &&
        (held & BUTTON_SELECT))
        state->b = true;
#endif
#if defined(BUTTON_RIGHT)
    if (settings->control_mode == TAMA_CONTROL_RAW_ABC && (held & BUTTON_RIGHT))
        state->c = true;
#endif
#ifdef BUTTON_PLAY
    if (settings->control_mode == TAMA_CONTROL_RAW_ABC &&
        (held & BUTTON_PLAY))
        state->c = true;
#endif

    state->a |= TIME_BEFORE(now, a_until);
    state->b |= TIME_BEFORE(now, b_until);
    state->c |= TIME_BEFORE(now, c_until);
    state->selected_icon = selected_icon;
}
