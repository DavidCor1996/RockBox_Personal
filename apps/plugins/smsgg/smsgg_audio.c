#include "plugin.h"
#include "smsgg_audio.h"
#include "smsgg_platform.h"
#include "upstream/shared.h"

#define SMSGG_AUDIO_BUFS 4
#define SMSGG_AUDIO_START_BUFS 2
#define SMSGG_AUDIO_MAX_SAMPLES 1024

static int16 *audio_buf;
static int16 *audio_hwbuf;
static int16 *audio_silence;
static int audio_samples;
static volatile int audio_queued;
static int audio_read_idx;
static int audio_write_idx;
static bool audio_enabled;
static bool audio_ready;
static bool audio_started;
static unsigned int saved_mixer_frequency;

static void audio_get_more(const void **start, size_t *size)
{
    if (audio_ready && audio_queued > 0)
    {
        rb->memcpy(audio_hwbuf,
                   &audio_buf[audio_samples * 2 * audio_read_idx],
                   audio_samples * 2 * sizeof(int16));
        audio_read_idx++;
        if (audio_read_idx >= SMSGG_AUDIO_BUFS)
            audio_read_idx = 0;
        audio_queued--;
    }
    else if (audio_hwbuf != NULL)
    {
        rb->memcpy(audio_hwbuf, audio_silence,
                   audio_samples * 2 * sizeof(int16));
    }

    *start = audio_hwbuf;
    *size = audio_samples * 2 * sizeof(int16);
}

void smsgg_audio_init(bool enabled)
{
    audio_enabled = enabled;
    audio_ready = false;
    audio_started = false;
    audio_queued = 0;
    audio_read_idx = 0;
    audio_write_idx = 0;
    audio_samples = 0;
    saved_mixer_frequency = rb->mixer_get_frequency();

    if (!audio_enabled)
        return;

    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcmbuf_fade(false, true);
    rb->pcmbuf_set_low_latency(true);
    rb->mixer_set_frequency(SMSGG_AUDIO_RATE);
}

void smsgg_audio_submit_frame(void)
{
    int i;
    int16 *dst;

    if (!audio_enabled || !snd.enabled || snd.sample_count <= 0 ||
        snd.output[0] == NULL || snd.output[1] == NULL)
        return;

    if (!audio_ready)
    {
        audio_samples = snd.sample_count;
        if (audio_samples > SMSGG_AUDIO_MAX_SAMPLES)
        {
            audio_enabled = false;
            return;
        }

        audio_buf = smsgg_malloc(SMSGG_AUDIO_BUFS * audio_samples * 2 *
                                 sizeof(int16));
        audio_hwbuf = smsgg_malloc(audio_samples * 2 * sizeof(int16));
        audio_silence = smsgg_calloc(audio_samples * 2, sizeof(int16));

        if (audio_buf == NULL || audio_hwbuf == NULL || audio_silence == NULL)
        {
            audio_enabled = false;
            return;
        }

        audio_ready = true;
    }

    rb->pcm_play_lock();
    if (audio_queued >= SMSGG_AUDIO_BUFS - 1)
    {
        rb->pcm_play_unlock();
        return;
    }
    rb->pcm_play_unlock();

    dst = &audio_buf[audio_samples * 2 * audio_write_idx];
    for (i = 0; i < audio_samples; i++)
    {
        dst[i * 2] = snd.output[0][i];
        dst[i * 2 + 1] = snd.output[1][i];
    }

    rb->pcm_play_lock();
    audio_queued++;
    audio_write_idx++;
    if (audio_write_idx >= SMSGG_AUDIO_BUFS)
        audio_write_idx = 0;
    rb->pcm_play_unlock();

    if (!audio_started && audio_queued >= SMSGG_AUDIO_START_BUFS)
    {
        rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                    audio_get_more, NULL, 0);
        audio_started = true;
    }
}

void smsgg_audio_shutdown(void)
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
    audio_ready = false;
    audio_started = false;
    audio_queued = 0;
}

bool smsgg_audio_is_enabled(void)
{
    return audio_enabled;
}
