#include "pocketcatch.h"

static enum plugin_status run_pocketcatch(void)
{
    struct pc_encounter_state state;
    enum plugin_status status = PLUGIN_OK;

    pc_state_init(&state, true);

    while (true)
    {
        struct pc_input_command command;
        struct pc_throw_request throw_request;
        long event = rb->button_get_w_tmo(PC_FRAME_TICKS);
        long now = *rb->current_tick;

        if (event == SYS_USB_CONNECTED ||
            rb->default_event_handler(event) == SYS_USB_CONNECTED)
        {
            status = PLUGIN_USB_CONNECTED;
            break;
        }

        pc_input_handle_event(&state.input, event, now, &command, &throw_request);
        pc_input_animate(&state.input, now);
        pc_state_update(&state, &command, &throw_request);

        if (command.exit_requested)
            break;

        pc_render_frame(&state);
    }

    pc_assets_teardown(&state.assets);
    return status;
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;

#if !defined(HAVE_LCD_COLOR)
    rb->splash(HZ * 2, "Podemon Go needs color LCD");
    return PLUGIN_ERROR;
#else
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->srand((unsigned int)*rb->current_tick);
    return run_pocketcatch();
#endif
}
