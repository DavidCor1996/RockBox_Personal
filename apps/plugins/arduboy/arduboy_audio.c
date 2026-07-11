#include "plugin.h"
#include "arduboy_audio.h"

#define ARDUBOY_AUDIO_SAMPLES 512
#define ARDUBOY_AUDIO_AMPLITUDE 7000

static int16_t audio_buffer[ARDUBOY_AUDIO_SAMPLES * 2];
static bool audio_enabled;
static bool audio_started;
static unsigned int saved_mixer_frequency;
static volatile int audio_level;
static volatile uint16_t audio_frequency;
static uint32_t audio_phase;

static void audio_get_more(const void **start, size_t *size)
{
    int i;
    int level = audio_level;
    uint16_t frequency = audio_frequency;
    uint32_t step = 0;

    if (frequency > 0)
        step = ((uint32_t)frequency << 16) / ARDUBOY_AUDIO_RATE;

    for (i = 0; i < ARDUBOY_AUDIO_SAMPLES; i++)
    {
        int16_t sample = 0;

        if (frequency > 0)
        {
            audio_phase += step;
            sample = (audio_phase & 0x8000) ?
                     ARDUBOY_AUDIO_AMPLITUDE : -ARDUBOY_AUDIO_AMPLITUDE;
        }
        else if (level != 0)
            sample = level > 0 ? ARDUBOY_AUDIO_AMPLITUDE :
                                 -ARDUBOY_AUDIO_AMPLITUDE;

        audio_buffer[i * 2] = sample;
        audio_buffer[i * 2 + 1] = sample;
    }

    *start = audio_buffer;
    *size = sizeof(audio_buffer);
}

void arduboy_audio_init(bool enabled)
{
    audio_enabled = enabled;
    audio_started = false;
    audio_level = 0;
    audio_frequency = 0;
    audio_phase = 0;
    saved_mixer_frequency = rb->mixer_get_frequency();

    if (!audio_enabled)
        return;

    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcmbuf_fade(false, true);
    rb->pcmbuf_set_low_latency(true);
    rb->mixer_set_frequency(ARDUBOY_AUDIO_RATE);
    rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                audio_get_more, NULL, 0);
    audio_started = true;
}

void arduboy_audio_update(int level, uint16_t frequency)
{
    if (!audio_enabled)
        return;

    audio_level = level;
    audio_frequency = frequency;
}

void arduboy_audio_shutdown(void)
{
    if (audio_enabled || audio_started)
    {
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        rb->pcmbuf_set_low_latency(false);
        rb->pcmbuf_fade(false, false);
        if (saved_mixer_frequency != 0)
            rb->mixer_set_frequency(saved_mixer_frequency);
    }

    audio_enabled = false;
    audio_started = false;
    audio_level = 0;
    audio_frequency = 0;
}

bool arduboy_audio_is_enabled(void)
{
    return audio_enabled;
}
