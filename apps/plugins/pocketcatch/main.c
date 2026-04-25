#include "pocketcatch.h"

static struct pc_game_state pc_game;

static void set_world_wheel_mode(bool world_mode)
{
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(!world_mode);
#else
    (void)world_mode;
#endif
}

static enum plugin_status run_pocketcatch(void)
{
    enum plugin_status status = PLUGIN_OK;
    bool world_wheel_mode = false;
    bool simulator_debug =
#ifdef SIMULATOR
        true;
#else
        false;
#endif

    rb->memset(&pc_game, 0, sizeof(pc_game));
    pc_game.mode = PC_MODE_WORLD;
    pc_world_init(&pc_game.world);
    pc_state_init(&pc_game.encounter, simulator_debug);
    set_world_wheel_mode(true);
    world_wheel_mode = true;

    while (true)
    {
        long event = rb->button_get_w_tmo(PC_FRAME_TICKS);

        if (event == SYS_USB_CONNECTED ||
            rb->default_event_handler(event) == SYS_USB_CONNECTED)
        {
            status = PLUGIN_USB_CONNECTED;
            break;
        }

        rb->backlight_on();

        if (pc_game.mode == PC_MODE_WORLD && !world_wheel_mode)
        {
            set_world_wheel_mode(true);
            world_wheel_mode = true;
        }
        else if (pc_game.mode == PC_MODE_ENCOUNTER && world_wheel_mode)
        {
            set_world_wheel_mode(false);
            world_wheel_mode = false;
        }

        if (pc_game.mode == PC_MODE_WORLD)
        {
            struct pc_world_command command;

            pc_world_input_handle_event(event, &command);
            pc_world_update(&pc_game.world, &command);
            if (pc_game.world.quit_requested)
                break;
            if (pc_game.world.pending_encounter)
            {
                pc_state_begin(&pc_game.encounter, pc_game.world.pending_species_index);
                pc_game.mode = PC_MODE_ENCOUNTER;
            }
            pc_world_render_frame(&pc_game.world);
        }
        else
        {
            struct pc_input_command command;
            struct pc_throw_request throw_request;
            long now = *rb->current_tick;

            pc_input_handle_event(&pc_game.encounter.input, event, now, &command, &throw_request);
            pc_input_animate(&pc_game.encounter.input, now);
            pc_state_update(&pc_game.encounter, &command, &throw_request);

            if (command.exit_requested)
                break;

            pc_render_frame(&pc_game.encounter);
            if (pc_game.encounter.finished)
            {
                pc_world_finish_encounter(&pc_game.world,
                                          pc_game.encounter.outcome,
                                          pc_game.encounter.species_index);
                pc_game.mode = PC_MODE_WORLD;
            }
        }
    }

    set_world_wheel_mode(false);
    pc_world_teardown(&pc_game.world);
    pc_assets_teardown(&pc_game.encounter.assets);
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
