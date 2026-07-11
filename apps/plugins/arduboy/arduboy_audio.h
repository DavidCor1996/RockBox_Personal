#ifndef ARDUBOY_AUDIO_H
#define ARDUBOY_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#define ARDUBOY_AUDIO_RATE 44100

void arduboy_audio_init(bool enabled);
void arduboy_audio_update(int level, uint16_t frequency);
void arduboy_audio_shutdown(void);
bool arduboy_audio_is_enabled(void);

#endif
