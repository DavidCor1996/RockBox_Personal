#include "pocketsky.h"
#include "lib/pluginlib_exit.h"
#include "lib/helper.h"

#include <tlsf.h>

static bool ps_valid_clock(void)
{
    struct tm *clock = rb->get_time();
    return clock && clock->tm_year + 1900 >= 1900 &&
           clock->tm_year + 1900 <= 2099 &&
           clock->tm_mon >= 0 && clock->tm_mon < 12 &&
           clock->tm_mday >= 1 && clock->tm_mday <= 31;
}

static void ps_handle_sky_button(struct ps_app *app, int button)
{
    int clean = button & ~(BUTTON_REPEAT | BUTTON_REL);
    int azimuth_step = button & BUTTON_REPEAT ? 12 : 2;

    if ((clean == BUTTON_SCROLL_FWD || clean == BUTTON_SCROLL_BACK) &&
        (rb->button_status() & BUTTON_SELECT))
    {
        if (clean == BUTTON_SCROLL_FWD && app->settings.fov_index < 4)
            ++app->settings.fov_index;
        else if (clean == BUTTON_SCROLL_BACK && app->settings.fov_index > 0)
            --app->settings.fov_index;
        ps_scene_mark_view_dirty(app);
    }
    else if (clean == BUTTON_SCROLL_FWD)
    {
        app->center_azimuth += azimuth_step;
        if (app->center_azimuth >= 360.0f)
            app->center_azimuth -= 360.0f;
        ps_scene_mark_view_dirty(app);
    }
    else if (clean == BUTTON_SCROLL_BACK)
    {
        app->center_azimuth -= azimuth_step;
        if (app->center_azimuth < 0.0f)
            app->center_azimuth += 360.0f;
        ps_scene_mark_view_dirty(app);
    }
    else if (clean == BUTTON_RIGHT)
    {
        app->center_altitude += 5.0f;
        if (app->center_altitude > 90.0f)
            app->center_altitude = 90.0f;
        ps_scene_mark_view_dirty(app);
    }
    else if (clean == BUTTON_LEFT)
    {
        app->center_altitude -= 5.0f;
        if (app->center_altitude < -10.0f)
            app->center_altitude = -10.0f;
        ps_scene_mark_view_dirty(app);
    }
    else if (clean == BUTTON_SELECT && !(button & BUTTON_REL))
    {
        ps_select_nearest(app);
        ps_show_object_card(app);
    }
    else if (clean == BUTTON_PLAY && (button & BUTTON_REPEAT))
        ps_show_time_machine(app);
    else if (clean == BUTTON_PLAY && (button & BUTTON_REL))
    {
        if (app->live)
        {
            app->simulated_time = ps_current_time(app);
            app->live = false;
        }
        else
            app->live = true;
        ps_scene_mark_time_dirty(app);
    }
    else if (clean == BUTTON_MENU && !(button & BUTTON_REL))
    {
        if (ps_show_main_menu(app))
            app->running = false;
    }
    else if (button == SYS_USB_CONNECTED)
    {
        app->usb = true;
        app->running = false;
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    struct ps_app app;
    char error[192];
    struct tm *clock;
    int last_minute = -1;

    (void)parameter;
    rb->memset(&app, 0, sizeof(app));
    app.running = true;
    app.pool = rb->plugin_get_buffer(&app.pool_size);
    if (!app.pool || app.pool_size < 512 * 1024 ||
        init_memory_pool(app.pool_size, app.pool) == (size_t)-1)
    {
        rb->splash(HZ * 3, "Pocket Sky: insufficient plugin memory");
        return PLUGIN_ERROR;
    }

    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    backlight_ignore_timeout();
    ps_settings_load(&app.settings);
    if (!ps_data_load(&app, error, sizeof(error)))
    {
        ps_put_error("Pocket Sky data error", error);
        while (true)
        {
            int button = rb->button_get(true);
            if ((button & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_MENU)
                break;
            if (button == SYS_USB_CONNECTED)
            {
                app.usb = true;
                break;
            }
        }
        backlight_use_settings();
        destroy_memory_pool(app.pool);
        return app.usb ? PLUGIN_USB_CONNECTED : PLUGIN_ERROR;
    }

    ps_scene_init(&app);
    if (!app.settings.location_set)
    {
        if (ps_use_default_location(&app))
            ps_settings_save(&app.settings);
        else
            ps_show_location(&app);
    }
    if (!ps_valid_clock())
    {
        rb->splash(HZ * 3, "RTC is invalid\nSet the Rockbox clock before using live sky");
        app.live = false;
        app.simulated_time = Astronomy_MakeTime(2000, 1, 1, 0, 0, 0.0);
    }

    while (app.running)
    {
        int button;
        clock = rb->get_time();
        if (app.live && clock && clock->tm_min != last_minute)
        {
            last_minute = clock->tm_min;
            ps_scene_mark_time_dirty(&app);
        }
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
        if (app.scene.horizontal_dirty)
            rb->cpu_boost(true);
#endif
        if (!ps_scene_update(&app))
        {
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
            rb->cpu_boost(false);
#endif
            rb->splash(HZ * 2, "Astronomy calculation failed");
            break;
        }
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
        rb->cpu_boost(false);
#endif
        ps_render_sky(&app);
        button = rb->button_get_w_tmo(HZ);
        if (button != BUTTON_NONE)
            ps_handle_sky_button(&app, button);
    }

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
    rb->cpu_boost(false);
#endif
    ps_settings_save(&app.settings);
    backlight_use_settings();
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();
    rb->lcd_update();
    Astronomy_Reset();
    destroy_memory_pool(app.pool);
    return app.usb ? PLUGIN_USB_CONNECTED : PLUGIN_OK;
}
