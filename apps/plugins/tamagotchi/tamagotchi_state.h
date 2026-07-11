#ifndef TAMAGOTCHI_STATE_H
#define TAMAGOTCHI_STATE_H

#include "tamagotchi.h"

void tamagotchi_settings_default(struct tamagotchi_settings *settings);
void tamagotchi_config_load(struct tamagotchi_settings *settings);
void tamagotchi_config_save(const struct tamagotchi_settings *settings);
bool tamagotchi_state_save(void);
bool tamagotchi_state_load(void);
bool tamagotchi_state_delete(void);

#endif
