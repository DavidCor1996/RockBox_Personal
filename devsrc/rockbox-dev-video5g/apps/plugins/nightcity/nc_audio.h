/***************************************************************************
 * nightcity - embedded PCM one-shot audio
 ***************************************************************************/

#ifndef NIGHTCITY_NC_AUDIO_H
#define NIGHTCITY_NC_AUDIO_H

enum nc_sound_id
{
    NC_SOUND_TITLE = 0,
    NC_SOUND_AFTERGLOW,
    NC_SOUND_MOVE,
    NC_SOUND_CONFIRM,
    NC_SOUND_BACK,
    NC_SOUND_TRANSITION,
};

void nc_audio_init(void);
void nc_audio_play(enum nc_sound_id sound);
void nc_audio_stop(void);

#endif
