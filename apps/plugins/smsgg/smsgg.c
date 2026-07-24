#include "plugin.h"

#include "lib/helper.h"
#include "lib/rockachievements.h"
#include "smsgg_audio.h"
#include "smsgg_core.h"
#include "smsgg_haptics.h"
#include "smsgg_input.h"
#include "smsgg_menu.h"
#include "smsgg_platform.h"
#include "smsgg_rom.h"
#include "smsgg_state.h"
#include "smsgg_video.h"

static struct smsgg_core core;
static struct smsgg_settings settings;
static char sram_path[MAX_PATH];
static char state_path[MAX_PATH];
static bool usb_connected;
static struct rockachievements_runtime achievements;

static uint32_t achievements_peek(uint32_t address, uint32_t num_bytes,
                                  void *userdata)
{
    uint32_t value = 0;
    uint32_t index;

    (void)userdata;
    if (num_bytes > 4)
        num_bytes = 4;
    for (index = 0; index < num_bytes; ++index)
    {
        uint32_t current = address + index;
        uint8 byte = 0;

        if (current < 0x2000)
            byte = sms.wram[current];
        else if (current < 0xa000 && cart.sram != NULL)
            byte = cart.sram[current - 0x2000];
        value |= (uint32_t)byte << (index * 8);
    }
    return value;
}

static void achievements_stop(void)
{
    rockachievements_shutdown(&achievements);
}

static void achievements_start(const char *rom_path)
{
    void *workspace;

    if (!rockachievements_available(rom_path))
        return;
    workspace = smsgg_malloc(ROCKACHIEVEMENTS_WORKSPACE_TARGET);
    if (workspace != NULL)
        rockachievements_init(&achievements, rom_path, achievements_peek,
                              NULL, workspace,
                              ROCKACHIEVEMENTS_WORKSPACE_TARGET);
}

static void run_core_frame(bool skip)
{
    smsgg_core_run_frame(skip);
    rockachievements_do_frame(&achievements);
}

static bool load_rom_path(const char *rom_path)
{
    if (rom_path == NULL || rom_path[0] == '\0')
        return false;

    smsgg_settings_apply_rom_profile(&settings, rom_path);
    rb->strlcpy(settings.last_rom, rom_path, sizeof(settings.last_rom));
    smsgg_settings_save(&settings);

    achievements_stop();
    smsgg_platform_reset_temp();
    if (!smsgg_core_load(&core, settings.last_rom, settings.audio_enabled))
    {
        rb->splash(HZ * 2, "Could not load ROM");
        return false;
    }

    smsgg_rom_build_save_paths(settings.last_rom, core.crc,
                               sram_path, sizeof(sram_path),
                               state_path, sizeof(state_path));
    smsgg_core_load_sram(&core, sram_path);

    achievements_start(settings.last_rom);

    if (settings.auto_load_state && rb->file_exists(state_path))
    {
        if (rockachievements_hardcore_active(&achievements))
            rb->splash(HZ, "iPod Hardcore: state load blocked");
        else
            smsgg_core_load_state(&core, state_path);
    }

    return true;
}

static bool choose_and_load_rom(void)
{
    struct smsgg_rom_list list;
    int selected;

    if (!smsgg_rom_scan(&list))
        return false;

    smsgg_input_enable_wheel_events(true);
    rb->button_clear_queue();
    selected = smsgg_rom_select(&list, settings.last_rom);
    smsgg_input_enable_wheel_events(false);
    rb->button_clear_queue();
    if (selected < 0)
        return false;

    return load_rom_path(list.entries[selected].path);
}

static bool handle_menu(void)
{
    enum smsgg_menu_action action;
    bool audio_was_enabled = settings.audio_enabled;

    smsgg_input_enable_wheel_events(true);
    rb->button_clear_queue();
    action = smsgg_menu_run(&settings);
    smsgg_input_enable_wheel_events(false);
    rb->button_clear_queue();

    smsgg_settings_save(&settings);

    if (audio_was_enabled != settings.audio_enabled && core.loaded)
    {
        smsgg_audio_shutdown();
        smsgg_core_set_audio(settings.audio_enabled);
        smsgg_audio_init(settings.audio_enabled);
    }

    switch (action)
    {
        case SMSGG_MENU_RESUME:
            return true;
        case SMSGG_MENU_SELECT_ROM:
            if (settings.auto_save_sram)
                smsgg_core_save_sram(&core, sram_path);
            smsgg_audio_shutdown();
            achievements_stop();
            smsgg_core_unload(&core);
            if (!choose_and_load_rom())
                return false;
            smsgg_audio_init(settings.audio_enabled);
            return true;
        case SMSGG_MENU_RESET:
            smsgg_core_reset(&core);
            rockachievements_reset(&achievements);
            return true;
        case SMSGG_MENU_SAVE_STATE:
            if (smsgg_core_save_state(&core, state_path))
            {
                smsgg_haptic_save();
                rb->splash(HZ, "State saved");
            }
            else
                rb->splash(HZ, "State save failed");
            return true;
        case SMSGG_MENU_LOAD_STATE:
            if (rockachievements_hardcore_active(&achievements))
            {
                rb->splash(HZ, "iPod Hardcore: state load blocked");
                return true;
            }
            if (smsgg_core_load_state(&core, state_path))
            {
                rockachievements_reset(&achievements);
                smsgg_haptic_load();
                rb->splash(HZ, "State loaded");
            }
            else
                rb->splash(HZ, "No valid state");
            return true;
        case SMSGG_MENU_SEND_START:
            smsgg_core_set_buttons(0, INPUT_START);
            run_core_frame(false);
            return true;
        case SMSGG_MENU_SEND_PAUSE:
            smsgg_core_set_buttons(0, INPUT_PAUSE);
            run_core_frame(false);
            smsgg_haptic_pause();
            return true;
        case SMSGG_MENU_QUIT:
        default:
            return false;
    }
}

static void run_emulator(void)
{
    bool quit = false;
    long fps_start = *rb->current_tick;
    int frames = 0;
    int fps = 0;

    while (!quit)
    {
        struct smsgg_input_state istate;
        bool skip = false;

        smsgg_input_poll(&istate, settings.controls_preset,
                         settings.wheel_mode, settings.wheel_sensitivity,
                         settings.wheel_deadzone);

        if (istate.quit_requested)
        {
            if (istate.usb_requested)
                usb_connected = true;
            break;
        }

        if (istate.menu_requested)
        {
            if (!handle_menu())
                break;
            rb->button_clear_queue();
            continue;
        }

        if (istate.pause_requested)
        {
            smsgg_haptic_pause();
            if (!handle_menu())
                break;
        }

        if (settings.frameskip > 0)
            skip = (frames % (settings.frameskip + 1)) != 0;

        smsgg_core_set_buttons(istate.pad, istate.system);
        run_core_frame(skip);
        smsgg_audio_submit_frame();

        if (!skip)
            smsgg_video_draw(&core, settings.scaling_mode, settings.show_fps,
                             fps, istate.pad, istate.wheel_delta,
                             istate.wheel_zone, settings.input_debug);

        frames++;
        if (*rb->current_tick - fps_start >= HZ)
        {
            fps = frames;
            frames = 0;
            fps_start = *rb->current_tick;
        }

        rb->yield();
    }
}

enum plugin_status plugin_start(const void *parameter)
{
    char launch_rom[MAX_PATH];
    void *audio_buffer;
    size_t audio_size;
    enum plugin_status status = PLUGIN_OK;

    usb_connected = false;
    launch_rom[0] = '\0';
    rb->lcd_setfont(FONT_SYSFIXED);
    backlight_ignore_timeout();
#if defined(HAVE_ADJUSTABLE_CPU_FREQ) && (CONFIG_PLATFORM & PLATFORM_NATIVE)
    rb->cpu_boost(true);
#endif

    if (parameter != NULL)
    {
        const char *path = (const char *)parameter;

        if (path[0] == '@')
            path++;
        rb->strlcpy(launch_rom, path, sizeof(launch_rom));
    }

    audio_buffer = rb->plugin_get_audio_buffer(&audio_size);
    smsgg_platform_init_alloc(audio_buffer, audio_size);

    if (!smsgg_ensure_dirs())
    {
        rb->splash(HZ * 2, "Could not create .rockbox/games/smsgg");
        status = PLUGIN_ERROR;
        goto out;
    }

    smsgg_settings_load(&settings);
    smsgg_haptic_set_enabled(settings.haptics_enabled,
                             settings.haptic_strength);
    smsgg_video_init();
    smsgg_input_init();
    smsgg_audio_init(settings.audio_enabled);

    if ((launch_rom[0] != '\0' && load_rom_path(launch_rom)) ||
        (launch_rom[0] == '\0' && choose_and_load_rom()))
        run_emulator();

    if (core.loaded)
        smsgg_core_save_sram(&core, sram_path);

    smsgg_audio_shutdown();
    achievements_stop();
    smsgg_core_unload(&core);
    smsgg_input_shutdown();
    smsgg_settings_save(&settings);

out:
#if defined(HAVE_ADJUSTABLE_CPU_FREQ) && (CONFIG_PLATFORM & PLATFORM_NATIVE)
    rb->cpu_boost(false);
#endif
    backlight_use_settings();
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_clear_display();
    rb->lcd_update();
    rb->plugin_release_audio_buffer();
    return usb_connected ? PLUGIN_USB_CONNECTED : status;
}
