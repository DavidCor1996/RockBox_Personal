#ifndef TAMAGOTCHI_HAL_H
#define TAMAGOTCHI_HAL_H

#include "tamagotchi.h"
#include "tamagotchi_input.h"
#include "upstream/tamalib/hal.h"

void tamagotchi_hal_init(const struct tamagotchi_settings *settings);
hal_t *tamagotchi_hal_get(void);
void tamagotchi_hal_poll(struct tamagotchi_input_state *state);
bool tamagotchi_hal_halted(void);

#endif
