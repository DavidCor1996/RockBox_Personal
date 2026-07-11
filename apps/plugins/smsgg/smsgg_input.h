#ifndef SMSGG_INPUT_H
#define SMSGG_INPUT_H

#include "plugin.h"
#include "upstream/shared.h"

enum smsgg_control_preset
{
    SMSGG_PRESET_CLASSIC = 0,
    SMSGG_PRESET_IPOD,
    SMSGG_PRESET_SONIC,
    SMSGG_PRESET_MENU_SAFE,
    SMSGG_PRESET_FIGHTING,
    SMSGG_PRESET_COUNT
};

enum smsgg_wheel_mode
{
    SMSGG_WHEEL_DISABLED = 0,
    SMSGG_WHEEL_HORIZONTAL,
    SMSGG_WHEEL_VERTICAL,
    SMSGG_WHEEL_4WAY,
    SMSGG_WHEEL_ANALOG_LR,
    SMSGG_WHEEL_ANALOG_8WAY,
    SMSGG_WHEEL_TURBO,
    SMSGG_WHEEL_PADDLE,
    SMSGG_WHEEL_COUNT
};

struct smsgg_input_state
{
    uint8 pad;
    uint8 system;
    int raw_button;
    int held_button;
    int wheel_delta;
    int wheel_zone;
    bool wheel_touched;
    bool menu_requested;
    bool pause_requested;
    bool quit_requested;
    bool usb_requested;
};

void smsgg_input_init(void);
void smsgg_input_shutdown(void);
void smsgg_input_enable_wheel_events(bool enabled);
void smsgg_input_poll(struct smsgg_input_state *state,
                      enum smsgg_control_preset preset,
                      enum smsgg_wheel_mode wheel_mode,
                      int sensitivity, int deadzone);
uint8 smsgg_input_get_buttons(const struct smsgg_input_state *state);
int smsgg_input_get_wheel_delta(const struct smsgg_input_state *state);
int smsgg_input_get_wheel_zone(const struct smsgg_input_state *state);
bool smsgg_input_is_wheel_touched(const struct smsgg_input_state *state);

#endif
