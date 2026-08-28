/***************************************************************************
 * Stock-game-oriented iPod click-wheel mapping for Zelda3.
 ****************************************************************************/
#include "zelda3.h"

enum {
    ZELDA_B      = 1 << 0,
    ZELDA_Y      = 1 << 1,
    ZELDA_SELECT = 1 << 2,
    ZELDA_START  = 1 << 3,
    ZELDA_UP     = 1 << 4,
    ZELDA_DOWN   = 1 << 5,
    ZELDA_LEFT   = 1 << 6,
    ZELDA_RIGHT  = 1 << 7,
    ZELDA_A      = 1 << 8,
    ZELDA_X      = 1 << 9,
    ZELDA_L      = 1 << 10,
    ZELDA_R      = 1 << 11,
};

#if defined(HAVE_WHEEL_POSITION) || defined(IPOD_6G)
static unsigned wheel_direction_for_position(int position)
{
    int zone;

    if (position < 0)
        return 0;
    zone = ((position + 6) / 12) & 7;
    switch (zone)
    {
        case 0: return ZELDA_UP;
        case 1: return ZELDA_UP | ZELDA_RIGHT;
        case 2: return ZELDA_RIGHT;
        case 3: return ZELDA_RIGHT | ZELDA_DOWN;
        case 4: return ZELDA_DOWN;
        case 5: return ZELDA_DOWN | ZELDA_LEFT;
        case 6: return ZELDA_LEFT;
        default: return ZELDA_LEFT | ZELDA_UP;
    }
}
#endif

static unsigned map_input_state(int held, int wheel_position)
{
    unsigned input = 0;
    bool menu_select = false;
    bool menu_left = false;
    bool menu_right = false;
    bool menu_play = false;

#if defined(BUTTON_MENU) && defined(BUTTON_SELECT)
    menu_select = (held & (BUTTON_MENU | BUTTON_SELECT)) ==
                  (BUTTON_MENU | BUTTON_SELECT);
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_LEFT)
    menu_left = (held & (BUTTON_MENU | BUTTON_LEFT)) ==
                (BUTTON_MENU | BUTTON_LEFT);
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_RIGHT)
    menu_right = (held & (BUTTON_MENU | BUTTON_RIGHT)) ==
                 (BUTTON_MENU | BUTTON_RIGHT);
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_PLAY)
    menu_play = (held & (BUTTON_MENU | BUTTON_PLAY)) ==
                (BUTTON_MENU | BUTTON_PLAY);
#endif

#if defined(HAVE_WHEEL_POSITION) || defined(IPOD_6G)
    input |= wheel_direction_for_position(wheel_position);
#else
    (void)wheel_position;
#ifdef BUTTON_UP
    if (held & BUTTON_UP)
        input |= ZELDA_UP;
#endif
#ifdef BUTTON_DOWN
    if (held & BUTTON_DOWN)
        input |= ZELDA_DOWN;
#endif
#endif
#ifdef BUTTON_SELECT
    if ((held & BUTTON_SELECT) && !menu_select)
        input |= ZELDA_B;
#endif
#ifdef BUTTON_PLAY
    if ((held & BUTTON_PLAY) && !menu_play)
        input |= ZELDA_A;
#endif
#ifdef BUTTON_LEFT
    if ((held & BUTTON_LEFT) && !menu_left)
        input |= ZELDA_Y;
#endif
#ifdef BUTTON_RIGHT
    if ((held & BUTTON_RIGHT) && !menu_right)
        input |= ZELDA_X;
#endif
#ifdef BUTTON_MENU
    if ((held & BUTTON_MENU) && !menu_select && !menu_left &&
        !menu_right && !menu_play)
        input |= ZELDA_START;
#endif
    if (menu_select)
        input |= ZELDA_SELECT;
    if (menu_left)
        input |= ZELDA_L;
    if (menu_right)
        input |= ZELDA_R;
    if (menu_play)
        input |= ZELDA_SELECT;
    return input;
}

bool zelda3_input_self_test(unsigned *coverage)
{
    unsigned seen = 0;
    bool valid = true;

#if defined(HAVE_WHEEL_POSITION) || defined(IPOD_6G)
    static const unsigned expected[8] = {
        ZELDA_UP, ZELDA_UP | ZELDA_RIGHT, ZELDA_RIGHT,
        ZELDA_RIGHT | ZELDA_DOWN, ZELDA_DOWN,
        ZELDA_DOWN | ZELDA_LEFT, ZELDA_LEFT, ZELDA_LEFT | ZELDA_UP,
    };
    int zone;

    for (zone = 0; zone < 8; ++zone)
    {
        unsigned mapped = wheel_direction_for_position(zone * 12);
        valid &= mapped == expected[zone];
        seen |= mapped;
    }
    valid &= wheel_direction_for_position(-1) == 0;
#endif
#ifdef BUTTON_SELECT
    valid &= map_input_state(BUTTON_SELECT, -1) == ZELDA_B;
    seen |= map_input_state(BUTTON_SELECT, -1);
#endif
#ifdef BUTTON_PLAY
    valid &= map_input_state(BUTTON_PLAY, -1) == ZELDA_A;
    seen |= map_input_state(BUTTON_PLAY, -1);
#endif
#ifdef BUTTON_LEFT
    valid &= map_input_state(BUTTON_LEFT, -1) == ZELDA_Y;
    seen |= map_input_state(BUTTON_LEFT, -1);
#endif
#ifdef BUTTON_RIGHT
    valid &= map_input_state(BUTTON_RIGHT, -1) == ZELDA_X;
    seen |= map_input_state(BUTTON_RIGHT, -1);
#endif
#ifdef BUTTON_MENU
    valid &= map_input_state(BUTTON_MENU, -1) == ZELDA_START;
    seen |= map_input_state(BUTTON_MENU, -1);
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_SELECT)
    valid &= map_input_state(BUTTON_MENU | BUTTON_SELECT, -1) == ZELDA_SELECT;
    seen |= map_input_state(BUTTON_MENU | BUTTON_SELECT, -1);
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_LEFT)
    valid &= map_input_state(BUTTON_MENU | BUTTON_LEFT, -1) == ZELDA_L;
    seen |= map_input_state(BUTTON_MENU | BUTTON_LEFT, -1);
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_RIGHT)
    valid &= map_input_state(BUTTON_MENU | BUTTON_RIGHT, -1) == ZELDA_R;
    seen |= map_input_state(BUTTON_MENU | BUTTON_RIGHT, -1);
#endif
#if defined(BUTTON_MENU) && defined(BUTTON_PLAY)
    valid &= map_input_state(BUTTON_MENU | BUTTON_PLAY, -1) == ZELDA_SELECT;
    seen |= map_input_state(BUTTON_MENU | BUTTON_PLAY, -1);
#endif
    valid &= (seen & 0xfff) == 0xfff;
    if (coverage)
        *coverage = seen;
    return valid;
}

static bool hold_exit_requested(void)
{
#ifdef HAS_BUTTON_HOLD
    bool held = rb->button_hold();

    if (!zelda3_rb.hold_initialized)
    {
        zelda3_rb.hold_initialized = true;
        zelda3_rb.hold_armed = held;
        return false;
    }
    if (held && !zelda3_rb.hold_armed)
    {
        zelda3_rb.hold_armed = true;
        return true;
    }
    zelda3_rb.hold_armed = held;
#endif
    return false;
}

unsigned zelda3_input_poll(void)
{
    int held = rb->button_status();
    int event = rb->button_get(false);
    int wheel_position = -1;

    if (hold_exit_requested())
    {
        zelda3_rb.quit = true;
        return 0;
    }
    if (event == SYS_USB_CONNECTED)
    {
        zelda3_rb.usb_connected = true;
        zelda3_rb.quit = true;
        return 0;
    }
#ifdef HAVE_WHEEL_POSITION
    wheel_position = rb->wheel_status();
#endif
#ifdef BUTTON_MENU
    if ((event & BUTTON_REPEAT) && (event & BUTTON_MENU))
        zelda3_rb.quit = true;
#endif
    return map_input_state(held, wheel_position);
}
