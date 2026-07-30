/***************************************************************************
 * iPod click-wheel input: touched radial pad plus physical actions.
 ***************************************************************************/

#include "maker_lite.h"

void maker_lite_haptic(int duration, int strength)
{
    if (rb->haptic_feedback_enabled && rb->haptic_feedback &&
        rb->haptic_feedback_enabled())
        rb->haptic_feedback(duration, strength);
}

void maker_lite_input_init(void)
{
    maker_lite.wheel_zone = -1;
    maker_lite.last_physical = 0;
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    rb->button_clear_queue();
}

static uint32_t wheel_input(void)
{
#ifdef HAVE_WHEEL_POSITION
    int position = rb->wheel_status();
    int zone;
    int delta;

    if (position < 0)
    {
        maker_lite.wheel_zone = -1;
        return 0;
    }
    zone = ((position + 6) / 12) & 7;
    if (maker_lite.wheel_zone >= 0)
    {
        delta = position - maker_lite.wheel_zone * 12;
        while (delta > 48)
            delta -= 96;
        while (delta < -48)
            delta += 96;
        if (delta >= -8 && delta <= 8)
            zone = maker_lite.wheel_zone;
    }
    maker_lite.wheel_zone = zone;
    switch (zone)
    {
        case 0: return ML_INPUT_UP;
        case 1: return ML_INPUT_UP | ML_INPUT_RIGHT;
        case 2: return ML_INPUT_RIGHT;
        case 3: return ML_INPUT_RIGHT | ML_INPUT_DOWN;
        case 4: return ML_INPUT_DOWN;
        case 5: return ML_INPUT_DOWN | ML_INPUT_LEFT;
        case 6: return ML_INPUT_LEFT;
        default: return ML_INPUT_LEFT | ML_INPUT_UP;
    }
#else
    return 0;
#endif
}

uint32_t maker_lite_input_poll(int event)
{
    uint32_t input = 0;
    int held = rb->button_status();
    int physical = 0;
    int action_physical = 0;

#ifdef BUTTON_SELECT
    physical |= BUTTON_SELECT;
    action_physical |= BUTTON_SELECT;
    if (held & BUTTON_SELECT)
        input |= maker_lite.physical_map[0];
#endif
#ifdef BUTTON_PLAY
    physical |= BUTTON_PLAY;
    action_physical |= BUTTON_PLAY;
    if (held & BUTTON_PLAY)
        input |= maker_lite.physical_map[1];
#endif
#ifdef BUTTON_LEFT
    physical |= BUTTON_LEFT;
    action_physical |= BUTTON_LEFT;
    if (held & BUTTON_LEFT)
        input |= maker_lite.physical_map[2];
#endif
#ifdef BUTTON_RIGHT
    physical |= BUTTON_RIGHT;
    action_physical |= BUTTON_RIGHT;
    if (held & BUTTON_RIGHT)
        input |= maker_lite.physical_map[3];
#endif
#ifdef BUTTON_MENU
    physical |= BUTTON_MENU;
    if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_MENU)
    {
        if (!(event & (BUTTON_REPEAT | BUTTON_REL)))
        {
            maker_lite.menu_down = true;
            maker_lite.menu_pressed_tick = *rb->current_tick;
        }
        else if (event & BUTTON_REPEAT)
        {
            maker_lite.save_requested = true;
            maker_lite.quit = true;
        }
        else if (event & BUTTON_REL)
        {
            if (maker_lite.menu_down && !maker_lite.quit)
            {
                maker_lite.world.paused = !maker_lite.world.paused;
                if (maker_lite.world.paused)
                    maker_lite.menu_pause_count++;
            }
            maker_lite.menu_down = false;
        }
    }
#endif

    if ((held & action_physical) & ~maker_lite.last_physical)
        maker_lite_haptic(12, 28);
    maker_lite.last_physical = held & action_physical;

    /* Pressing a physical control suppresses its underlying touch sector. */
    if (!(held & physical))
        input |= wheel_input();
#ifdef SIMULATOR
    /* A backgroundless simulator has no clickable wheel surface. Preserve a
     * repeatable keyboard gate without changing physical-device behavior. */
#ifdef BUTTON_SCROLL_BACK
    if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_SCROLL_BACK)
        input |= ML_INPUT_LEFT;
#endif
#ifdef BUTTON_SCROLL_FWD
    if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_SCROLL_FWD)
        input |= ML_INPUT_RIGHT;
#endif
#else
#ifdef BUTTON_SCROLL_BACK
    if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_SCROLL_BACK)
        input |= ML_INPUT_PREVIOUS;
#endif
#ifdef BUTTON_SCROLL_FWD
    if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_SCROLL_FWD)
        input |= ML_INPUT_NEXT;
#endif
#endif
    if (maker_lite.world.paused)
        input |= ML_INPUT_PAUSE;
    return input;
}
