#ifndef SMSGG_HAPTICS_H
#define SMSGG_HAPTICS_H

#include "plugin.h"

void smsgg_haptic_set_enabled(bool enabled, int strength);
void smsgg_haptic_button(void);
void smsgg_haptic_menu(void);
void smsgg_haptic_pause(void);
void smsgg_haptic_save(void);
void smsgg_haptic_load(void);
void smsgg_haptic_damage(void);
void smsgg_haptic_collect(void);
void smsgg_haptic_collision(void);

#endif
