#include "anarch_platform.h"

#define MENU_HOLD_TICKS HZ

static uint8_t keys[ANARCH_KEY_COUNT];
static int wheel_zone;
static long menu_tick;
static bool in_menu;
static bool usb_requested;
static bool exit_requested;
static bool pause_requested;
#ifdef HAS_BUTTON_HOLD
static bool hold_was_on;
#endif

#ifdef HAVE_WHEEL_POSITION
static void set_wheel_keys(int zone)
{
    keys[ANARCH_KEY_UP] = zone == 7 || zone == 0 || zone == 1;
    keys[ANARCH_KEY_RIGHT] = zone == 1 || zone == 2 || zone == 3;
    keys[ANARCH_KEY_DOWN] = zone == 3 || zone == 4 || zone == 5;
    keys[ANARCH_KEY_LEFT] = zone == 5 || zone == 6 || zone == 7;
}
#endif

static void poll_wheel(void)
{
#ifdef HAVE_WHEEL_POSITION
    int position = rb->wheel_status();

    if (position < 0)
    {
        wheel_zone = -1;
        return;
    }
    {
        int candidate = ((position + 6) / 12) & 7;
        int center = wheel_zone * 12;
        int distance = position - center;

        if (wheel_zone >= 0)
        {
            if (distance > 48)
                distance -= 96;
            else if (distance < -48)
                distance += 96;
        }
        if (wheel_zone < 0 || distance > 8 || distance < -8)
            wheel_zone = candidate;
    }
    set_wheel_keys(wheel_zone);
#endif
}

static void apply_buttons(int held)
{
    keys[ANARCH_KEY_A] = (held & BUTTON_SELECT) != 0;
    keys[ANARCH_KEY_STRAFE_LEFT] = (held & BUTTON_LEFT) != 0;
    keys[ANARCH_KEY_STRAFE_RIGHT] = (held & BUTTON_RIGHT) != 0;
    keys[ANARCH_KEY_PREVIOUS_WEAPON] =
        (held & (BUTTON_SELECT | BUTTON_LEFT)) ==
        (BUTTON_SELECT | BUTTON_LEFT);
    keys[ANARCH_KEY_NEXT_WEAPON] =
        (held & (BUTTON_SELECT | BUTTON_RIGHT)) ==
        (BUTTON_SELECT | BUTTON_RIGHT);
    if (held & BUTTON_PLAY)
        keys[in_menu ? ANARCH_KEY_B : ANARCH_KEY_JUMP] = 1;
}

void anarch_input_init(void)
{
    rb->memset(keys, 0, sizeof(keys));
    wheel_zone = -1;
    menu_tick = 0;
    in_menu = false;
    usb_requested = false;
    exit_requested = false;
    pause_requested = false;
#ifdef HAS_BUTTON_HOLD
    hold_was_on = rb->button_hold();
#endif
    rb->button_clear_queue();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
}

void anarch_input_poll(void)
{
    int event;
    int held;
    long now = *rb->current_tick;

    rb->memset(keys, 0, sizeof(keys));
    anarch_profile_input_queue((unsigned int)rb->button_queue_count());
    while ((event = rb->button_get(false)) != BUTTON_NONE)
    {
        int clean;

        if (event == SYS_USB_CONNECTED ||
            rb->default_event_handler(event) == SYS_USB_CONNECTED)
        {
            usb_requested = true;
            continue;
        }
        clean = event & ~(BUTTON_REPEAT | BUTTON_REL);
        if (clean == BUTTON_MENU)
        {
            if (!(event & (BUTTON_REL | BUTTON_REPEAT)))
                menu_tick = now;
            else if (event & BUTTON_REL)
            {
                if (menu_tick != 0 &&
                    TIME_BEFORE(now, menu_tick + MENU_HOLD_TICKS))
                    pause_requested = true;
                menu_tick = 0;
            }
        }
    }

#ifdef HAS_BUTTON_HOLD
    if (rb->button_hold())
    {
        if (!hold_was_on)
            exit_requested = true;
        hold_was_on = true;
        menu_tick = 0;
        wheel_zone = -1;
        return;
    }
    hold_was_on = false;
#endif

    held = rb->button_status();
    if (menu_tick != 0 && (held & BUTTON_MENU) &&
        !TIME_BEFORE(now, menu_tick + MENU_HOLD_TICKS))
        exit_requested = true;
    if (menu_tick != 0 && !(held & BUTTON_MENU))
    {
        pause_requested = true;
        menu_tick = 0;
    }

    poll_wheel();
    apply_buttons(held);
}

void anarch_input_shutdown(void)
{
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    rb->button_clear_queue();
}

void anarch_input_set_menu(bool menu)
{
    in_menu = menu;
}

int8_t anarch_input_key(uint8_t key)
{
    return key < ANARCH_KEY_COUNT ? keys[key] : 0;
}

bool anarch_input_usb(void)
{
    return usb_requested;
}

bool anarch_input_exit(void)
{
    return exit_requested;
}

void anarch_input_clear_exit(void)
{
    exit_requested = false;
    menu_tick = 0;
}

bool anarch_input_pause_requested(void)
{
    return pause_requested;
}

void anarch_input_clear_pause(void)
{
    pause_requested = false;
}

#ifdef SIMULATOR
bool anarch_input_selftest(void)
{
    bool pass = true;

    rb->memset(keys, 0, sizeof(keys));
    in_menu = false;
    apply_buttons(BUTTON_SELECT | BUTTON_LEFT);
    pass = pass && keys[ANARCH_KEY_A] && keys[ANARCH_KEY_STRAFE_LEFT] &&
           keys[ANARCH_KEY_PREVIOUS_WEAPON] &&
           !keys[ANARCH_KEY_NEXT_WEAPON];
    rb->memset(keys, 0, sizeof(keys));
    apply_buttons(BUTTON_SELECT | BUTTON_RIGHT);
    pass = pass && keys[ANARCH_KEY_A] && keys[ANARCH_KEY_STRAFE_RIGHT] &&
           keys[ANARCH_KEY_NEXT_WEAPON] &&
           !keys[ANARCH_KEY_PREVIOUS_WEAPON];
    rb->memset(keys, 0, sizeof(keys));
    apply_buttons(BUTTON_PLAY);
    pass = pass && keys[ANARCH_KEY_JUMP] && !keys[ANARCH_KEY_B];
    rb->memset(keys, 0, sizeof(keys));
    in_menu = true;
    apply_buttons(BUTTON_PLAY);
    pass = pass && keys[ANARCH_KEY_B] && !keys[ANARCH_KEY_JUMP];
#ifdef HAVE_WHEEL_POSITION
    rb->memset(keys, 0, sizeof(keys));
    set_wheel_keys(1);
    pass = pass && keys[ANARCH_KEY_UP] && keys[ANARCH_KEY_RIGHT];
    rb->memset(keys, 0, sizeof(keys));
    set_wheel_keys(5);
    pass = pass && keys[ANARCH_KEY_DOWN] && keys[ANARCH_KEY_LEFT];
#endif
    rb->memset(keys, 0, sizeof(keys));
    pass = pass && !keys[ANARCH_KEY_A] && !keys[ANARCH_KEY_B] &&
           !keys[ANARCH_KEY_UP] && !keys[ANARCH_KEY_DOWN];
    pause_requested = true;
    exit_requested = true;
    usb_requested = true;
    pass = pass && anarch_input_pause_requested() && anarch_input_exit() &&
           anarch_input_usb();
    anarch_input_init();
    return pass;
}
#endif
