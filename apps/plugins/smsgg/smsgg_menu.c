#include "plugin.h"
#include "smsgg_haptics.h"
#include "smsgg_menu.h"
#include "smsgg_video.h"
#include "smsgg_input.h"

#define SMSGG_MENU_COUNT 17

static void clamp_settings(struct smsgg_settings *settings)
{
    if (settings->scaling_mode < 0 || settings->scaling_mode >= SMSGG_SCALE_COUNT)
        settings->scaling_mode = 0;
    if (settings->frameskip < 0)
        settings->frameskip = 0;
    if (settings->frameskip > 2)
        settings->frameskip = 2;
    if (settings->controls_preset < 0 ||
        settings->controls_preset >= SMSGG_PRESET_COUNT)
        settings->controls_preset = SMSGG_PRESET_SONIC;
    if (settings->wheel_mode < 0 || settings->wheel_mode >= SMSGG_WHEEL_COUNT)
        settings->wheel_mode = SMSGG_WHEEL_ANALOG_LR;
    if (settings->haptic_strength < 0)
        settings->haptic_strength = 0;
    if (settings->haptic_strength > 100)
        settings->haptic_strength = 100;
}

static const char *onoff(bool enabled)
{
    return enabled ? "On" : "Off";
}

static const char *scaling_name(int mode)
{
    switch (mode)
    {
        case SMSGG_SCALE_FIT:
            return "Fit";
        case SMSGG_SCALE_SMS_320X230:
            return "SMS 320x230";
        case SMSGG_SCALE_CONSERVATIVE:
            return "Conservative";
        case SMSGG_SCALE_FAST:
            return "Fast";
        default:
            return "?";
    }
}

static const char *preset_name(int preset)
{
    switch (preset)
    {
        case SMSGG_PRESET_CLASSIC:
            return "D-pad";
        case SMSGG_PRESET_IPOD:
            return "iPod";
        case SMSGG_PRESET_SONIC:
            return "Sonic";
        case SMSGG_PRESET_MENU_SAFE:
            return "Menu-safe";
        case SMSGG_PRESET_FIGHTING:
            return "Fighting";
        default:
            return "?";
    }
}

static const char *wheel_name(int mode)
{
    switch (mode)
    {
        case SMSGG_WHEEL_DISABLED:
            return "Disabled";
        case SMSGG_WHEEL_HORIZONTAL:
            return "Horizontal";
        case SMSGG_WHEEL_VERTICAL:
            return "Vertical";
        case SMSGG_WHEEL_4WAY:
            return "4-way";
        case SMSGG_WHEEL_ANALOG_LR:
            return "Analog L/R";
        case SMSGG_WHEEL_ANALOG_8WAY:
            return "Analog 8-way";
        case SMSGG_WHEEL_TURBO:
            return "Turbo";
        case SMSGG_WHEEL_PADDLE:
            return "Paddle";
        default:
            return "?";
    }
}

static const char *frameskip_name(int frameskip)
{
    switch (frameskip)
    {
        case 0:
            return "Off";
        case 1:
            return "1";
        case 2:
            return "2";
        default:
            return "?";
    }
}

static void build_item(char *buf, size_t size, int item,
                       const struct smsgg_settings *settings)
{
    switch (item)
    {
        case 0:
            rb->strlcpy(buf, "Resume", size);
            break;
        case 1:
            rb->strlcpy(buf, "Select ROM", size);
            break;
        case 2:
            rb->strlcpy(buf, "Reset Console", size);
            break;
        case 3:
            rb->strlcpy(buf, "Save State Now", size);
            break;
        case 4:
            rb->strlcpy(buf, "Load State", size);
            break;
        case 5:
            rb->strlcpy(buf, "Send START", size);
            break;
        case 6:
            rb->strlcpy(buf, "Send PAUSE", size);
            break;
        case 7:
            rb->snprintf(buf, size, "Scaling: %s",
                         scaling_name(settings->scaling_mode));
            break;
        case 8:
            rb->snprintf(buf, size, "Frameskip: %s",
                         frameskip_name(settings->frameskip));
            break;
        case 9:
            rb->snprintf(buf, size, "Audio: %s",
                         onoff(settings->audio_enabled));
            break;
        case 10:
            rb->snprintf(buf, size, "Haptics: %s",
                         onoff(settings->haptics_enabled));
            break;
        case 11:
            rb->snprintf(buf, size, "Haptic Strength: %d",
                         settings->haptic_strength);
            break;
        case 12:
            rb->snprintf(buf, size, "Controls: %s (%s)",
                         settings->profile_name,
                         preset_name(settings->controls_preset));
            break;
        case 13:
            rb->snprintf(buf, size, "Wheel: %s",
                         wheel_name(settings->wheel_mode));
            break;
        case 14:
            rb->snprintf(buf, size, "FPS Overlay: %s",
                         onoff(settings->show_fps));
            break;
        case 15:
            rb->snprintf(buf, size, "Input Debug: %s",
                         onoff(settings->input_debug));
            break;
        case 16:
            rb->strlcpy(buf, "Quit to Rockbox", size);
            break;
        default:
            rb->strlcpy(buf, "?", size);
            break;
    }
}

static void draw_menu(int selected, const struct smsgg_settings *settings)
{
    int i;
    int line = 0;
    int start = selected - 5;

    if (start < 0)
        start = 0;
    if (start > SMSGG_MENU_COUNT - 10)
        start = SMSGG_MENU_COUNT - 10;
    if (start < 0)
        start = 0;

    rb->lcd_clear_display();
    rb->lcd_puts(0, line++, "SMSGG Menu");

    for (i = start; i < SMSGG_MENU_COUNT && line < 10; i++, line++)
    {
        char text[64];
        char row[68];

        build_item(text, sizeof(text), i, settings);
        rb->snprintf(row, sizeof(row), "%c %s",
                     i == selected ? '>' : ' ', text);
        rb->lcd_puts(0, line, row);
    }

    rb->lcd_update();
}

static void next_control_profile(struct smsgg_settings *settings)
{
    settings->controls_preset =
        (settings->controls_preset + 1) % SMSGG_PRESET_COUNT;
    rb->strlcpy(settings->profile_name, "Manual",
                sizeof(settings->profile_name));
}

enum smsgg_menu_action smsgg_menu_run(struct smsgg_settings *settings)
{
    int selected = 0;

    while (1)
    {
        int button;
        int clean;

        draw_menu(selected, settings);
        button = rb->button_get(true);
        clean = button & ~(BUTTON_REPEAT | BUTTON_REL);

        if (IS_SYSEVENT(button))
        {
            if (button == SYS_USB_CONNECTED)
                return SMSGG_MENU_QUIT;
            continue;
        }
        if (button & BUTTON_REL)
            continue;

#ifdef BUTTON_MENU
        if (clean == BUTTON_MENU && selected > 0)
        {
            selected--;
            smsgg_haptic_menu();
            continue;
        }
#endif
#ifdef BUTTON_PLAY
        if (clean == BUTTON_PLAY && selected + 1 < SMSGG_MENU_COUNT)
        {
            selected++;
            smsgg_haptic_menu();
            continue;
        }
#endif
#ifdef BUTTON_SCROLL_BACK
        if (clean == BUTTON_SCROLL_BACK && selected > 0)
        {
            selected--;
            smsgg_haptic_menu();
            continue;
        }
#endif
#ifdef BUTTON_SCROLL_FWD
        if (clean == BUTTON_SCROLL_FWD && selected + 1 < SMSGG_MENU_COUNT)
        {
            selected++;
            smsgg_haptic_menu();
            continue;
        }
#endif
#ifdef BUTTON_SELECT
        if (clean != BUTTON_SELECT)
            continue;
#else
        continue;
#endif

        smsgg_haptic_menu();
        switch (selected)
        {
            case 0:
                return SMSGG_MENU_RESUME;
            case 1:
                return SMSGG_MENU_SELECT_ROM;
            case 2:
                return SMSGG_MENU_RESET;
            case 3:
                return SMSGG_MENU_SAVE_STATE;
            case 4:
                return SMSGG_MENU_LOAD_STATE;
            case 5:
                return SMSGG_MENU_SEND_START;
            case 6:
                return SMSGG_MENU_SEND_PAUSE;
            case 7:
                settings->scaling_mode =
                    (settings->scaling_mode + 1) % SMSGG_SCALE_COUNT;
                break;
            case 8:
                settings->frameskip = (settings->frameskip + 1) % 3;
                break;
            case 9:
                settings->audio_enabled = !settings->audio_enabled;
                break;
            case 10:
                settings->haptics_enabled = !settings->haptics_enabled;
                smsgg_haptic_set_enabled(settings->haptics_enabled,
                                         settings->haptic_strength);
                break;
            case 11:
                settings->haptic_strength += 20;
                if (settings->haptic_strength > 100)
                    settings->haptic_strength = 0;
                smsgg_haptic_set_enabled(settings->haptics_enabled,
                                         settings->haptic_strength);
                break;
            case 12:
                next_control_profile(settings);
                smsgg_haptic_pause();
                break;
            case 13:
                settings->wheel_mode =
                    (settings->wheel_mode + 1) % SMSGG_WHEEL_COUNT;
                break;
            case 14:
                settings->show_fps = !settings->show_fps;
                break;
            case 15:
                settings->input_debug = !settings->input_debug;
                break;
            case 16:
                return SMSGG_MENU_QUIT;
            default:
                return SMSGG_MENU_RESUME;
        }
        clamp_settings(settings);
        smsgg_settings_save(settings);
    }
}
