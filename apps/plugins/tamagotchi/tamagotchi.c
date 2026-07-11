#include "plugin.h"

#include "tamagotchi.h"
#include "tamagotchi_audio.h"
#include "tamagotchi_clock.h"
#include "tamagotchi_display.h"
#include "tamagotchi_hal.h"
#include "tamagotchi_haptics.h"
#include "tamagotchi_input.h"
#include "tamagotchi_menu.h"
#include "tamagotchi_rom.h"
#include "tamagotchi_state.h"
#include "tamagotchi_time.h"
#include "upstream/tamalib/tamalib.h"
#include "lib/plugin_notifications.h"

static u12_t program[TAMA_ROM_WORDS];
static struct tamagotchi_settings settings;
static bool usb_connected;

static void mkdir_if_needed(const char *path)
{
    if (!rb->dir_exists(path))
        rb->mkdir(path);
}

static bool ensure_dirs(void)
{
    mkdir_if_needed(ROCKBOX_DIR "/apps");
    mkdir_if_needed(TAMAGOTCHI_BASE_DIR);
    mkdir_if_needed(TAMAGOTCHI_ROM_DIR);
    mkdir_if_needed(TAMAGOTCHI_SAVE_DIR);
    mkdir_if_needed(ROCKBOX_DIR "/games");
    mkdir_if_needed(TAMAGOTCHI_GAME_BASE_DIR);
    mkdir_if_needed(TAMAGOTCHI_GAME_ROM_DIR);
    return rb->dir_exists(TAMAGOTCHI_ROM_DIR) &&
           rb->dir_exists(TAMAGOTCHI_SAVE_DIR);
}

static void show_missing_rom(const char *message)
{
    rb->lcd_clear_display();
    rb->lcd_putsxy(6, 16, (const unsigned char *)"Missing tama.b");
    rb->lcd_putsxy(6, 42, (const unsigned char *)"Put your legally");
    rb->lcd_putsxy(6, 58, (const unsigned char *)"obtained P1 ROM at:");
    rb->lcd_putsxy(6, 84, (const unsigned char *)".rockbox/games/");
    rb->lcd_putsxy(6, 100, (const unsigned char *)"tamagotchi/roms/");
    rb->lcd_putsxy(6, 116, (const unsigned char *)"tama.b");
    rb->lcd_update();
    (void)message;
    tamagotchi_haptic_error();
    rb->splashf(HZ * 4, "%s", message);
}

static void run_emulator(void)
{
    bool quit = false;
    long next_frame = *rb->current_tick;
    long next_autosave = *rb->current_tick + HZ * 300;

    rb->button_clear_queue();

    while (!quit)
    {
        struct tamagotchi_input_state input;

        tamagotchi_hal_poll(&input);

        if (input.quit_requested)
        {
            usb_connected = input.usb_requested;
            break;
        }

        if (input.menu_requested)
        {
            if (!tamagotchi_menu_run(&settings))
                break;
            rb->button_clear_queue();
            tamagotchi_display_mark_dirty();
        }

        tamalib_step();

        if (TIME_AFTER(*rb->current_tick, next_frame))
        {
            if (tamagotchi_display_is_dirty())
            {
                tamagotchi_display_render(&settings);
                rb->lcd_update();
            }
            next_frame = *rb->current_tick + HZ / 30;
        }

        if (TIME_AFTER(*rb->current_tick, next_autosave))
        {
            tamagotchi_state_save();
            next_autosave = *rb->current_tick + HZ * 300;
        }

        if (tamagotchi_hal_halted())
            quit = true;

        rb->yield();
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    size_t words = 0;

    (void)parameter;

    tamagotchi_settings_default(&settings);
    ensure_dirs();
    tamagotchi_config_load(&settings);
    settings.wheel_haptic_ticks = false;
    settings.attention_haptics = false;
    settings.haptics_enabled = false;
    settings.quiet_mode = true;
    settings.notifications_enabled = false;

    plugin_notify_init();
    tamagotchi_display_init();
    tamagotchi_input_init();
    tamagotchi_haptics_set_settings(&settings);
    tamagotchi_audio_set_settings(&settings);

    if (!tamagotchi_rom_load(program, ARRAYLEN(program), &words))
    {
        show_missing_rom(tamagotchi_rom_error());
        return PLUGIN_OK;
    }

    tamagotchi_hal_init(&settings);
    tamalib_register_hal(tamagotchi_hal_get());

    if (tamalib_init(program, NULL, 1000000) != 0)
    {
        rb->splash(HZ * 2, "TamaLIB init failed");
        return PLUGIN_ERROR;
    }

    tamalib_set_framerate(30);
    settings.clock_mode = TAMA_CLOCK_REAL_TIME;
    tamagotchi_display_show_loading("Loading...");

    if (tamagotchi_state_load())
    {
        tamagotchi_haptic_load();
        if (!settings.auto_clock_done || tamagotchi_clock_state_needs_seed())
        {
            if (tamagotchi_clock_autoset_from_ipod())
            {
                settings.auto_clock_done = true;
                tamagotchi_state_save();
            }
        }
        else
            tamagotchi_clock_sync_to_ipod();
    }
    else if (tamagotchi_clock_autoset_from_ipod())
    {
        settings.auto_clock_done = true;
        tamagotchi_state_save();
    }
    tamagotchi_config_save(&settings);
    tamagotchi_time_on_launch();

    run_emulator();
    tamagotchi_state_save();
    tamagotchi_time_on_exit();
    tamagotchi_config_save(&settings);
    tamalib_release();

    return usb_connected ? PLUGIN_USB_CONNECTED : PLUGIN_OK;
}
