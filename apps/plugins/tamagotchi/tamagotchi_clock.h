#ifndef TAMAGOTCHI_CLOCK_H
#define TAMAGOTCHI_CLOCK_H

#include "tamagotchi.h"

bool tamagotchi_clock_autoset_from_ipod(void);
bool tamagotchi_clock_sync_to_ipod(void);
bool tamagotchi_clock_state_needs_seed(void);
bool tamagotchi_clock_state_looks_unset(void);
bool tamagotchi_clock_is_autosetting(void);
void tamagotchi_clock_set_background_work(bool enable);
void tamagotchi_clock_filter_buttons(bool *a, bool *b, bool *c);

#endif
