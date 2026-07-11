#ifndef TAMAGOTCHI_AUDIO_H
#define TAMAGOTCHI_AUDIO_H

#include "tamagotchi.h"

void tamagotchi_audio_set_settings(const struct tamagotchi_settings *settings);
void tamagotchi_audio_set_frequency(unsigned freq_dhz);
void tamagotchi_audio_play(bool enable);
bool tamagotchi_audio_is_playing(void);
unsigned tamagotchi_audio_frequency(void);

#endif
