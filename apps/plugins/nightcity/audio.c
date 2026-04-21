/***************************************************************************
 * nightcity - embedded PCM one-shot audio
 ***************************************************************************/

#include "nightcity.h"
#include "nc_audio.h"
#include "audio_assets.h"

struct nc_sound_asset
{
    const int16_t *data;
    size_t count;
};

static const struct nc_sound_asset sound_assets[] =
{
    { nc_sound_title, NC_SOUND_TITLE_COUNT },
    { nc_sound_afterglow, NC_SOUND_AFTERGLOW_COUNT },
    { nc_sound_move, NC_SOUND_MOVE_COUNT },
    { nc_sound_confirm, NC_SOUND_CONFIRM_COUNT },
    { nc_sound_back, NC_SOUND_BACK_COUNT },
    { nc_sound_transition, NC_SOUND_TRANSITION_COUNT },
};

void nc_audio_init(void)
{
}

void nc_audio_play(enum nc_sound_id sound)
{
    if ((unsigned)sound >= ARRAYLEN(sound_assets))
        return;

    rb->pcm_play_stop();
    rb->pcm_set_frequency(NC_AUDIO_RATE);
    rb->pcm_play_data(NULL, NULL,
                      sound_assets[sound].data,
                      sound_assets[sound].count * sizeof(int16_t));
}

void nc_audio_stop(void)
{
    rb->pcm_play_stop();
    rb->pcm_set_frequency(HW_SAMPR_DEFAULT);
}
