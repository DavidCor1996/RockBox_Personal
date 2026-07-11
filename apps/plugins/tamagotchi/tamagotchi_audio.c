#include "tamagotchi_audio.h"
#include "tamagotchi_clock.h"
#include "tamagotchi_haptics.h"

static const struct tamagotchi_settings *audio_settings;
static unsigned current_freq_dhz;
static bool playing;

void tamagotchi_audio_set_settings(const struct tamagotchi_settings *settings)
{
    audio_settings = settings;
}

void tamagotchi_audio_set_frequency(unsigned freq_dhz)
{
    current_freq_dhz = freq_dhz;
}

void tamagotchi_audio_play(bool enable)
{
    bool rising = enable && !playing;

    playing = enable;

    if (!rising || audio_settings == NULL)
        return;

    if (tamagotchi_clock_is_autosetting())
        return;

    if (!audio_settings->quiet_mode)
        tamagotchi_haptic_attention();

}

bool tamagotchi_audio_is_playing(void)
{
    return playing;
}

unsigned tamagotchi_audio_frequency(void)
{
    return current_freq_dhz;
}
