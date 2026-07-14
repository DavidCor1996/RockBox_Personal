#include "sm64_rockbox.h"
#include "upstream/src/pc/audio/audio_api.h"
#include "tlsf.h"

#define SM64_AUDIO_RATE 32000
#define SM64_AUDIO_BLOCK_FRAMES 256
#define SM64_AUDIO_BLOCKS 20
#define SM64_AUDIO_START_BLOCKS 6
#define SM64_AUDIO_SAMPLES (SM64_AUDIO_BLOCK_FRAMES * 2)

static int16_t *audio_ring;
static int16_t conceal[SM64_AUDIO_SAMPLES] __attribute__((aligned(4)));
static volatile int queued_blocks;
static volatile int read_block;
static int write_block;
static int write_frames;
static int16_t last_left;
static int16_t last_right;
static unsigned old_frequency;
static unsigned long dropped_blocks;
static unsigned long underruns;

static void make_concealment(void)
{
    int frame;

    for (frame = 0; frame < SM64_AUDIO_BLOCK_FRAMES; ++frame)
    {
        int gain = frame < 32 ? 31 - frame : 0;
        conceal[frame * 2] = (int16_t)((last_left * gain) / 32);
        conceal[frame * 2 + 1] = (int16_t)((last_right * gain) / 32);
    }
    last_left = 0;
    last_right = 0;
}

static void audio_get_more(const void **start, size_t *size)
{
    if (queued_blocks > 0 && audio_ring)
    {
        int16_t *block = audio_ring + read_block * SM64_AUDIO_SAMPLES;
        *start = block;
        *size = SM64_AUDIO_SAMPLES * sizeof(int16_t);
        last_left = block[SM64_AUDIO_SAMPLES - 2];
        last_right = block[SM64_AUDIO_SAMPLES - 1];
        read_block = (read_block + 1) % SM64_AUDIO_BLOCKS;
        queued_blocks--;
    }
    else
    {
        make_concealment();
        *start = conceal;
        *size = sizeof(conceal);
        underruns++;
    }
}

bool sm64_audio_init(void)
{
    audio_ring = tlsf_calloc(SM64_AUDIO_BLOCKS * SM64_AUDIO_SAMPLES,
                             sizeof(int16_t));
    if (!audio_ring)
        return false;

    queued_blocks = 0;
    read_block = 0;
    write_block = 0;
    write_frames = 0;
    last_left = 0;
    last_right = 0;
    dropped_blocks = 0;
    underruns = 0;
    old_frequency = rb->mixer_get_frequency();
#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(SM64_AUDIO_RATE);
    rb->pcmbuf_fade(false, true);
    sm64_rb.audio_ready = true;
    sm64_logf("audio init requested=%u actual=%u", SM64_AUDIO_RATE,
              rb->mixer_get_frequency());
    return true;
}

int sm64_audio_buffered(void)
{
    return (queued_blocks * SM64_AUDIO_BLOCK_FRAMES) + write_frames;
}

int sm64_audio_desired(void)
{
    return SM64_AUDIO_BLOCK_FRAMES * 8;
}

void sm64_audio_play(const uint8_t *buffer, size_t length)
{
    const int16_t *data = (const int16_t *)buffer;
    size_t frames = length / (sizeof(int16_t) * 2);

    while (data && frames && sm64_rb.audio_ready)
    {
        size_t room = SM64_AUDIO_BLOCK_FRAMES - write_frames;
        size_t take = MIN(frames, room);
        int16_t *destination = audio_ring +
            write_block * SM64_AUDIO_SAMPLES + write_frames * 2;

        rb->memcpy(destination, data, take * 2 * sizeof(int16_t));
        data += take * 2;
        frames -= take;
        write_frames += take;
        if (write_frames == SM64_AUDIO_BLOCK_FRAMES)
        {
            bool queued = false;
            rb->pcm_play_lock();
            if (queued_blocks < SM64_AUDIO_BLOCKS - 1)
            {
                queued_blocks++;
                queued = true;
            }
            rb->pcm_play_unlock();
            if (queued)
            {
                write_block = (write_block + 1) % SM64_AUDIO_BLOCKS;
                if (!sm64_rb.audio_started &&
                    queued_blocks >= SM64_AUDIO_START_BLOCKS)
                {
                    rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_PLAYBACK,
                                                    MIX_AMP_UNITY);
                    rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                                audio_get_more, NULL, 0);
                    sm64_rb.audio_started = true;
                }
            }
            else
                dropped_blocks++;
            write_frames = 0;
        }
    }
}

void sm64_audio_shutdown(void)
{
    if (!sm64_rb.audio_ready && !sm64_rb.audio_started)
        return;

    rb->pcm_play_lock();
    if (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) != CHANNEL_STOPPED)
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    sm64_rb.audio_ready = false;
    sm64_rb.audio_started = false;
    queued_blocks = 0;
    rb->pcm_play_unlock();
    rb->pcmbuf_fade(false, false);
    if (old_frequency)
        rb->mixer_set_frequency(old_frequency);
    sm64_logf("audio close dropped=%lu underruns=%lu", dropped_blocks,
              underruns);
}

struct AudioAPI sm64_audio_api = {
    sm64_audio_init,
    sm64_audio_buffered,
    sm64_audio_desired,
    sm64_audio_play,
    sm64_audio_shutdown,
};
