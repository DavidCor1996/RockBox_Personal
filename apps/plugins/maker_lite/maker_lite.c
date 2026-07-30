/***************************************************************************
 * Rockpod Maker Lite native plugin entry and fixed 60 Hz loop.
 ***************************************************************************/

#include "maker_lite.h"

struct maker_lite_app maker_lite;

#ifdef SIMULATOR
static uint32_t simulator_scripted_input(void)
{
    if (maker_lite.level.flags & ML_LEVEL_BRAWL)
    {
        unsigned opponent = maker_lite.world.brawl_opponent_entity;
        uint32_t input = 0;

        if (opponent < maker_lite.level.entity_count)
        {
            if (maker_lite.world.entity_x[opponent] <
                maker_lite.world.player.x)
                input |= ML_INPUT_LEFT;
            else
                input |= ML_INPUT_RIGHT;
            if (maker_lite.world.player.grounded &&
                maker_lite.world.entity_y[opponent] + ML_FIXED_ONE * 20 <
                maker_lite.world.player.y)
                input |= ML_INPUT_UP;
        }
        if (maker_lite.world.tick % 20 == 0)
            input |= ML_INPUT_SECONDARY;
        else if (maker_lite.world.tick % 8 == 0)
            input |= ML_INPUT_PRIMARY;
        return input;
    }
    if (maker_lite.level.flags & ML_LEVEL_LIFE_SIM)
    {
        /*
         * Neon Nook gate path: move a starter furnishing, leave the
         * apartment through its reciprocal doorway, cross downtown,
         * enter the parked car, drive, and exit.
         */
        if (maker_lite.world.tick < 28)
            return ML_INPUT_RIGHT;
        if (maker_lite.world.tick == 28 ||
            maker_lite.world.tick == 30 ||
            maker_lite.world.tick == 32)
            return ML_INPUT_PRIMARY;
        if (maker_lite.world.tick < 69)
            return ML_INPUT_LEFT;
        if (maker_lite.world.tick < 92)
            return ML_INPUT_DOWN;
        if (maker_lite.world.tick < 271)
            return ML_INPUT_RIGHT;
        if (maker_lite.world.tick < 336)
            return ML_INPUT_DOWN;
        if (maker_lite.world.tick == 336)
            return ML_INPUT_PRIMARY;
        if (maker_lite.world.tick < 405)
            return ML_INPUT_RIGHT;
        if (maker_lite.world.tick == 405)
            return ML_INPUT_PRIMARY;
        if (maker_lite.world.tick < 441)
            return ML_INPUT_DOWN;
        if (maker_lite.world.tick < 481)
            return ML_INPUT_RIGHT;
        return 0;
    }
    switch (maker_lite.level.ruleset)
    {
        case ML_RULESET_MARIO:
            if (maker_lite.world.tick == 20 ||
                maker_lite.world.tick == 105)
                return ML_INPUT_RIGHT | ML_INPUT_PRIMARY |
                       ML_INPUT_SECONDARY;
            return ML_INPUT_RIGHT | ML_INPUT_SECONDARY;
        case ML_RULESET_ZELDA:
            if (maker_lite.world.tick == 15)
                return ML_INPUT_PRIMARY;
            if (maker_lite.world.tick == 35)
                return ML_INPUT_SECONDARY;
            if (maker_lite.world.tick == 55)
                return ML_INPUT_NEXT;
            if (ml_fixed_to_int(maker_lite.world.player.x) < 104 &&
                ml_fixed_to_int(maker_lite.world.player.y) < 80)
                return ML_INPUT_RIGHT;
            if (ml_fixed_to_int(maker_lite.world.player.y) < 200)
                return ML_INPUT_DOWN;
            return ML_INPUT_RIGHT;
        case ML_RULESET_SONIC:
            if (maker_lite.world.player.grounded &&
                maker_lite.world.tick >= 90 &&
                maker_lite.world.tick < 150 &&
                maker_lite.world.player.spindash_charge < 3)
                return ML_INPUT_DOWN |
                    ((maker_lite.world.tick & 1) ? ML_INPUT_PRIMARY : 0);
            if (maker_lite.world.player.grounded &&
                maker_lite.world.tick % 90 == 0)
                return ML_INPUT_RIGHT | ML_INPUT_PRIMARY;
            return ML_INPUT_RIGHT;
        default:
            return 0;
    }
}
#endif

static void show_controls(void)
{
    maker_lite.controls_screen_count++;
    maker_lite_render_controls();
    rb->button_clear_queue();
    while (true)
    {
        int event = rb->button_get(true);

        if (event == SYS_USB_CONNECTED)
        {
            maker_lite.usb = true;
            maker_lite.save_requested = true;
            maker_lite.quit = true;
            return;
        }
#ifdef BUTTON_MENU
        if (event == (BUTTON_MENU | BUTTON_REL))
            return;
#endif
#ifdef BUTTON_SELECT
        if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_SELECT)
            return;
#endif
    }
}

static void pause_loop(void)
{
    maker_lite_render();
    maker_lite_render_pause(false);
    rb->button_clear_queue();
    while (maker_lite.world.paused && !maker_lite.quit)
    {
        int event = rb->button_get_w_tmo(HZ / 10);

        if (event == SYS_USB_CONNECTED)
        {
            maker_lite.usb = true;
            maker_lite.save_requested = true;
            maker_lite.quit = true;
            break;
        }
        if (maker_lite.world.complete)
        {
#ifdef BUTTON_SELECT
            if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_SELECT &&
                !(event & BUTTON_REL))
            {
                uint32_t best_ticks = maker_lite.world.best_ticks;

                ml_world_init(&maker_lite.world, &maker_lite.level);
                maker_lite.world.best_ticks = best_ticks;
                maker_lite_render();
                rb->button_clear_queue();
                continue;
            }
#endif
#ifdef BUTTON_MENU
            if (event == (BUTTON_MENU | BUTTON_REL))
            {
                maker_lite.save_requested = true;
                maker_lite.quit = true;
                continue;
            }
#endif
        }
#ifdef HAS_BUTTON_HOLD
        if (rb->button_hold())
        {
            maker_lite_render_pause(true);
            while (rb->button_hold())
            {
                event = rb->button_get_w_tmo(HZ / 10);
                if (event == SYS_USB_CONNECTED)
                {
                    maker_lite.usb = true;
                    maker_lite.save_requested = true;
                    maker_lite.quit = true;
                    return;
                }
            }
            maker_lite_render_pause(false);
        }
#endif
#ifdef BUTTON_SELECT
        if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_SELECT &&
            !(event & BUTTON_REL))
        {
            maker_lite_haptic(20, 40);
            maker_lite.world.paused = false;
        }
#endif
#ifdef BUTTON_PLAY
        if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_PLAY &&
            !(event & BUTTON_REL))
        {
            show_controls();
            maker_lite_render();
            maker_lite_render_pause(false);
        }
#endif
#ifdef BUTTON_MENU
        if ((event & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_MENU &&
            (event & BUTTON_REPEAT))
        {
            maker_lite.save_requested = true;
            maker_lite.quit = true;
        }
        else if (event == (BUTTON_MENU | BUTTON_REL))
            maker_lite.world.paused = false;
#endif
    }
    maker_lite.last_tick = *rb->current_tick;
    maker_lite.simulation_fraction = 0;
    rb->button_clear_queue();
}

static void game_loop(void)
{
    maker_lite.start_tick = *rb->current_tick;
    maker_lite.last_tick = maker_lite.start_tick;
    maker_lite_render();
    while (!maker_lite.quit)
    {
        long now;
        long elapsed;
        unsigned ticks;
        uint32_t input;
        int event = rb->button_get_w_tmo(1);

        if (event == SYS_USB_CONNECTED)
        {
            maker_lite.usb = true;
            maker_lite.save_requested = true;
            break;
        }
#ifdef HAS_BUTTON_HOLD
        if (rb->button_hold())
        {
            maker_lite.world.paused = true;
            maker_lite.hold_paused = true;
            maker_lite.hold_pause_count++;
            maker_lite_render_pause(true);
            while (rb->button_hold())
            {
                event = rb->button_get_w_tmo(HZ / 10);
                if (event == SYS_USB_CONNECTED)
                {
                    maker_lite.usb = true;
                    maker_lite.save_requested = true;
                    maker_lite.quit = true;
                    return;
                }
            }
            maker_lite.hold_paused = false;
            maker_lite.world.paused = false;
            maker_lite.last_tick = *rb->current_tick;
            maker_lite.simulation_fraction = 0;
            rb->button_clear_queue();
            maker_lite_render();
            continue;
        }
#endif
        input = maker_lite_input_poll(event);
#ifdef SIMULATOR
        if (getenv("MAKER_LITE_TEST_SCRIPTED_INPUT"))
            input = simulator_scripted_input();
#endif
        if (maker_lite.world.paused)
        {
            pause_loop();
            continue;
        }
        now = *rb->current_tick;
        elapsed = now - maker_lite.last_tick;
        if (elapsed < 0)
            elapsed = 0;
        maker_lite.last_tick = now;
        maker_lite.simulation_fraction += elapsed * 60;
        ticks = maker_lite.simulation_fraction / HZ;
        maker_lite.simulation_fraction %= HZ;
        if (ticks > 2)
        {
            maker_lite.missed_deadlines += ticks - 2;
            ticks = 2;
        }
        while (ticks-- > 0)
        {
            int old_collectibles = maker_lite.world.collectibles;
            int old_rings = maker_lite.world.rings;
            int old_health = maker_lite.world.player.health;
            bool was_complete = maker_lite.world.complete;
            uint8_t old_room_transition =
                maker_lite.world.room_transition_ticks;
            uint8_t old_action = maker_lite.world.player.action;
            uint8_t old_car_active = maker_lite.world.car_active;
            uint8_t old_interaction = maker_lite.world.interaction;

            ml_world_tick(&maker_lite.world, input);
            maker_lite.simulation_ticks++;
            if (!old_car_active && maker_lite.world.car_active)
                maker_lite.car_entry_count++;
            else if (old_car_active && !maker_lite.world.car_active)
                maker_lite.car_exit_count++;
            if (old_interaction == ML_LIFE_INTERACTION_NONE &&
                maker_lite.world.interaction != ML_LIFE_INTERACTION_NONE)
                maker_lite.life_interaction_count++;
            if (old_room_transition == 0 &&
                maker_lite.world.room_transition_ticks > 0)
                maker_lite.room_transition_count++;
            if (maker_lite.world.pending_effect >= 0)
                maker_lite_audio_play(
                    (unsigned)maker_lite.world.pending_effect);
            else if (maker_lite.world.complete && !was_complete)
                maker_lite_audio_play(3);
            else if (maker_lite.world.collectibles > old_collectibles ||
                     maker_lite.world.rings > old_rings)
                maker_lite_audio_play(1);
            else if (maker_lite.world.player.health < old_health ||
                     maker_lite.world.rings < old_rings)
                maker_lite_audio_play(2);
            else if (maker_lite.world.player.action == ML_ACTION_JUMP &&
                     old_action != ML_ACTION_JUMP)
                maker_lite_audio_play(0);
            else if ((maker_lite.world.player.action == ML_ACTION_SWORD ||
                      maker_lite.world.player.action == ML_ACTION_ITEM) &&
                     maker_lite.world.player.action != old_action)
                maker_lite_audio_play(4);
            if ((maker_lite.simulation_ticks & 1) == 0)
                maker_lite_render();
        }
#ifdef SIMULATOR
        {
            const char *limit_text = getenv("MAKER_LITE_TEST_TICKS");
            unsigned long limit = limit_text ?
                                  strtoul(limit_text, NULL, 10) : 0;
            if (limit && maker_lite.simulation_ticks >= limit)
            {
                maker_lite_render();
                (void)maker_lite_storage_dump_test_frame();
                maker_lite.save_requested = true;
                maker_lite.quit = true;
            }
        }
#endif
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    char selected_path[MAX_PATH];
    const char *path = parameter;
    size_t arena_size;

    rb->memset(&maker_lite, 0, sizeof(maker_lite));
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);

    if (!path || !path[0])
    {
        if (!maker_lite_storage_pick_project(selected_path,
                                             sizeof(selected_path)))
            return PLUGIN_OK;
        path = selected_path;
    }

    maker_lite.arena = rb->plugin_get_buffer(&arena_size);
    maker_lite.arena_size = arena_size;
    if (!maker_lite.arena || arena_size < 256 * 1024)
    {
        rb->splash(HZ * 2, "Maker Lite needs 256 KiB");
        return PLUGIN_ERROR;
    }
    if (!maker_lite_storage_load(path) ||
        !maker_lite_storage_load_art())
        return PLUGIN_ERROR;
    if (!maker_lite_storage_load_settings())
        rb->splash(HZ, "Custom controls ignored");
    maker_lite.playback_active_at_start =
        (rb->audio_status() & AUDIO_STATUS_PLAY) != 0;
    (void)maker_lite_audio_init();
    if (!maker_lite_storage_load_save())
        rb->splash(HZ, "Old or corrupt save ignored");

    maker_lite_input_init();
    game_loop();
    if (maker_lite.save_requested && !maker_lite_storage_save())
        rb->splash(HZ * 2, "Could not save project");

    maker_lite_audio_shutdown();
    maker_lite_storage_log_session();
    /* Maker Lite never claims playback memory or installs PCM callbacks. */
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(false);
#endif
    rb->button_clear_queue();
    if (maker_lite.usb)
        return PLUGIN_USB_CONNECTED;
    return PLUGIN_OK;
}
