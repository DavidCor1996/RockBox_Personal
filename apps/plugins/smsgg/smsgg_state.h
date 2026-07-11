#ifndef SMSGG_STATE_H
#define SMSGG_STATE_H

#include "plugin.h"

struct smsgg_settings
{
    char last_rom[MAX_PATH];
    int scaling_mode;
    int frameskip;
    bool audio_enabled;
    bool haptics_enabled;
    int haptic_strength;
    int controls_preset;
    int wheel_mode;
    int wheel_sensitivity;
    int wheel_deadzone;
    bool show_fps;
    bool auto_save_sram;
    bool auto_load_state;
    bool input_debug;
    char profile_name[32];
};

void smsgg_settings_defaults(struct smsgg_settings *settings);
void smsgg_settings_load(struct smsgg_settings *settings);
void smsgg_settings_save(const struct smsgg_settings *settings);
void smsgg_settings_apply_rom_profile(struct smsgg_settings *settings,
                                      const char *rom_path);

#endif
