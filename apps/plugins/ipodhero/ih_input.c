/***************************************************************************
 * iPod Hero click-wheel input normalization
 ****************************************************************************/

#include "ipodhero.h"

static uint8_t ih_lane_for_button(int clean)
{
    switch (clean)
    {
        case BUTTON_LEFT: return 1u << 0;
        case BUTTON_MENU: return 1u << 1;
        case BUTTON_SELECT: return 1u << 2;
        case BUTTON_PLAY: return 1u << 3;
        case BUTTON_RIGHT: return 1u << 4;
        default: return 0;
    }
}

void ih_input_reset(struct ih_input *input)
{
    rb->memset(input, 0, sizeof(*input));
}

struct ih_input_event ih_input_normalize(struct ih_input *input,
                                         long button)
{
    struct ih_input_event event = { IH_INPUT_NONE, 0 };
    int clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
    uint8_t lane;

    if ((clean & (BUTTON_SELECT | BUTTON_MENU)) ==
        (BUTTON_SELECT | BUTTON_MENU))
    {
        input->clockwise_steps = 0;
        if (!(button & (BUTTON_REPEAT | BUTTON_REL)))
            event.kind = IH_INPUT_PAUSE;
        return event;
    }

    if (clean == BUTTON_SCROLL_FWD || clean == BUTTON_SCROLL_BACK)
    {
        long now = *rb->current_tick;

        event.kind = IH_INPUT_WHEEL;
        if (TIME_AFTER(now, input->gesture_deadline))
            input->clockwise_steps = 0;
        input->gesture_deadline = now + IH_WHEEL_GESTURE_TICKS;
        if (clean == BUTTON_SCROLL_FWD)
        {
            input->clockwise_steps++;
            if (input->clockwise_steps >= IH_WHEEL_STAR_STEPS)
            {
                input->clockwise_steps = 0;
                event.kind = IH_INPUT_STAR;
            }
        }
        else
            input->clockwise_steps = 0;
        return event;
    }

    lane = ih_lane_for_button(clean);
    if (lane == 0 || (button & BUTTON_REPEAT))
        return event;
    event.lane_mask = lane;
    event.kind = button & BUTTON_REL ? IH_INPUT_RELEASE : IH_INPUT_PRESS;
    return event;
}
