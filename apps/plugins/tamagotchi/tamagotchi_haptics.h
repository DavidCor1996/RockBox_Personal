#ifndef TAMAGOTCHI_HAPTICS_H
#define TAMAGOTCHI_HAPTICS_H

#include "tamagotchi.h"

void tamagotchi_haptics_set_settings(const struct tamagotchi_settings *settings);
void tamagotchi_haptic_button(void);
void tamagotchi_haptic_wheel(void);
void tamagotchi_haptic_attention(void);
void tamagotchi_haptic_save(void);
void tamagotchi_haptic_load(void);
void tamagotchi_haptic_error(void);

#endif
