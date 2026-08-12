/* Varvara four-voice audio device adapted from the official Uxn emulator. */

#ifndef UXN_AUDIO_H
#define UXN_AUDIO_H

#include "plugin.h"
#include "uxn.h"

#define UXN_AUDIO_RATE 44100
#define UXN_AUDIO_VOICES 4

void uxn_audio_init(void);
void uxn_audio_shutdown(void);
uint8_t uxn_audio_dei(int voice, uint8_t port);
void uxn_audio_deo(int voice, uint8_t port);
void uxn_audio_poll(void);

#endif
