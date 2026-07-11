#ifndef SMSGG_AUDIO_H
#define SMSGG_AUDIO_H

#include "plugin.h"

#define SMSGG_AUDIO_RATE 44100

void smsgg_audio_init(bool enabled);
void smsgg_audio_submit_frame(void);
void smsgg_audio_shutdown(void);
bool smsgg_audio_is_enabled(void);

#endif
