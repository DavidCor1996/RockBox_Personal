#include "plugin.h"
#include "smsgg_platform.h"
#include "smsgg_state.h"

#define PROFILE_PRESET_CLASSIC 0
#define PROFILE_PRESET_IPOD 1
#define PROFILE_PRESET_SONIC 2
#define PROFILE_PRESET_FIGHTING 4
#define PROFILE_WHEEL_DISABLED 0
#define PROFILE_WHEEL_4WAY 3
#define PROFILE_WHEEL_ANALOG_LR 4
#define PROFILE_WHEEL_ANALOG_8WAY 5

void smsgg_settings_defaults(struct smsgg_settings *settings)
{
    rb->memset(settings, 0, sizeof(*settings));
    settings->scaling_mode = 0;
    settings->frameskip = 1;
    settings->audio_enabled = true;
    settings->haptics_enabled = true;
    settings->haptic_strength = 60;
    settings->controls_preset = 2;
    settings->wheel_mode = 4;
    settings->wheel_sensitivity = 1;
    settings->wheel_deadzone = 1;
    settings->show_fps = false;
    settings->auto_save_sram = true;
    settings->auto_load_state = false;
    settings->input_debug = false;
    rb->strlcpy(settings->profile_name, "Sonic / platform",
                sizeof(settings->profile_name));
}

static char lower_ascii(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    return c;
}

static bool contains_ci(const char *haystack, const char *needle)
{
    size_t i;
    size_t j;

    if (haystack == NULL || needle == NULL || needle[0] == '\0')
        return false;

    for (i = 0; haystack[i] != '\0'; i++)
    {
        for (j = 0; needle[j] != '\0'; j++)
        {
            if (haystack[i + j] == '\0')
                return false;
            if (lower_ascii(haystack[i + j]) != lower_ascii(needle[j]))
                break;
        }

        if (needle[j] == '\0')
            return true;
    }

    return false;
}

static void set_profile(struct smsgg_settings *settings, const char *name,
                        int preset, int wheel_mode,
                        int sensitivity, int deadzone)
{
    rb->strlcpy(settings->profile_name, name, sizeof(settings->profile_name));
    settings->controls_preset = preset;
    settings->wheel_mode = wheel_mode;
    settings->wheel_sensitivity = sensitivity;
    settings->wheel_deadzone = deadzone;
}

void smsgg_settings_apply_rom_profile(struct smsgg_settings *settings,
                                      const char *rom_path)
{
    const char *name = smsgg_basename(rom_path);

    if (contains_ci(name, "sonic"))
    {
        set_profile(settings, "Sonic wheel", PROFILE_PRESET_SONIC,
                    PROFILE_WHEEL_ANALOG_8WAY, 0, 0);
    }
    else if (contains_ci(name, "road rash"))
    {
        set_profile(settings, "Racing wheel", PROFILE_PRESET_IPOD,
                    PROFILE_WHEEL_ANALOG_LR, 0, 1);
    }
    else if (contains_ci(name, "nba jam") ||
             contains_ci(name, "super smash"))
    {
        set_profile(settings, "4-way action", PROFILE_PRESET_IPOD,
                    PROFILE_WHEEL_ANALOG_8WAY, 1, 1);
    }
    else if (contains_ci(name, "mortal kombat"))
    {
        set_profile(settings, "Fighting", PROFILE_PRESET_FIGHTING,
                    PROFILE_WHEEL_ANALOG_8WAY, 0, 1);
    }
    else if (contains_ci(name, "streets of rage") ||
             contains_ci(name, "shinobi") ||
             contains_ci(name, "earthworm"))
    {
        set_profile(settings, "Action platform", PROFILE_PRESET_IPOD,
                    PROFILE_WHEEL_ANALOG_8WAY, 0, 0);
    }
    else
    {
        set_profile(settings, "Stock iPod", PROFILE_PRESET_IPOD,
                    PROFILE_WHEEL_ANALOG_LR, 1, 1);
    }
}

static void parse_line(struct smsgg_settings *settings, char *line)
{
    char *eq = rb->strchr(line, '=');
    char *key;
    char *val;

    if (eq == NULL)
        return;

    *eq = '\0';
    key = line;
    val = eq + 1;

    if (rb->strcmp(key, "last_rom") == 0)
        rb->strlcpy(settings->last_rom, val, sizeof(settings->last_rom));
    else if (rb->strcmp(key, "scaling_mode") == 0)
        settings->scaling_mode = rb->atoi(val);
    else if (rb->strcmp(key, "frameskip") == 0)
        settings->frameskip = rb->atoi(val);
    else if (rb->strcmp(key, "audio_enabled") == 0)
        settings->audio_enabled = rb->atoi(val) != 0;
    else if (rb->strcmp(key, "haptics_enabled") == 0)
        settings->haptics_enabled = rb->atoi(val) != 0;
    else if (rb->strcmp(key, "haptic_strength") == 0)
        settings->haptic_strength = rb->atoi(val);
    else if (rb->strcmp(key, "controls_preset") == 0)
        settings->controls_preset = rb->atoi(val);
    else if (rb->strcmp(key, "wheel_mode") == 0)
        settings->wheel_mode = rb->atoi(val);
    else if (rb->strcmp(key, "wheel_sensitivity") == 0)
        settings->wheel_sensitivity = rb->atoi(val);
    else if (rb->strcmp(key, "wheel_deadzone") == 0)
        settings->wheel_deadzone = rb->atoi(val);
    else if (rb->strcmp(key, "show_fps") == 0)
        settings->show_fps = rb->atoi(val) != 0;
    else if (rb->strcmp(key, "auto_save_sram") == 0)
        settings->auto_save_sram = rb->atoi(val) != 0;
    else if (rb->strcmp(key, "auto_load_state") == 0)
        settings->auto_load_state = rb->atoi(val) != 0;
    else if (rb->strcmp(key, "input_debug") == 0)
        settings->input_debug = rb->atoi(val) != 0;
    else if (rb->strcmp(key, "profile_name") == 0)
        rb->strlcpy(settings->profile_name, val,
                    sizeof(settings->profile_name));
}

void smsgg_settings_load(struct smsgg_settings *settings)
{
    int fd;
    char line[256];

    smsgg_settings_defaults(settings);
    fd = rb->open(SMSGG_CONFIG_PATH, O_RDONLY);
    if (fd < 0)
        return;

    while (rb->read_line(fd, line, sizeof(line)) > 0)
        parse_line(settings, line);

    rb->close(fd);
}

void smsgg_settings_save(const struct smsgg_settings *settings)
{
    int fd = rb->open(SMSGG_CONFIG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0)
        return;

    rb->fdprintf(fd, "last_rom=%s\n", settings->last_rom);
    rb->fdprintf(fd, "scaling_mode=%d\n", settings->scaling_mode);
    rb->fdprintf(fd, "frameskip=%d\n", settings->frameskip);
    rb->fdprintf(fd, "audio_enabled=%d\n", settings->audio_enabled ? 1 : 0);
    rb->fdprintf(fd, "haptics_enabled=%d\n", settings->haptics_enabled ? 1 : 0);
    rb->fdprintf(fd, "haptic_strength=%d\n", settings->haptic_strength);
    rb->fdprintf(fd, "controls_preset=%d\n", settings->controls_preset);
    rb->fdprintf(fd, "wheel_mode=%d\n", settings->wheel_mode);
    rb->fdprintf(fd, "wheel_sensitivity=%d\n", settings->wheel_sensitivity);
    rb->fdprintf(fd, "wheel_deadzone=%d\n", settings->wheel_deadzone);
    rb->fdprintf(fd, "show_fps=%d\n", settings->show_fps ? 1 : 0);
    rb->fdprintf(fd, "auto_save_sram=%d\n", settings->auto_save_sram ? 1 : 0);
    rb->fdprintf(fd, "auto_load_state=%d\n", settings->auto_load_state ? 1 : 0);
    rb->fdprintf(fd, "input_debug=%d\n", settings->input_debug ? 1 : 0);
    rb->fdprintf(fd, "profile_name=%s\n", settings->profile_name);
    rb->close(fd);
}
