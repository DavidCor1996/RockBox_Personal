/***************************************************************************
 * Low-jitter SNES audio queue for Rockbox's primary playback mixer.
 *
 * The core is clocked at the actual hardware rate returned by the mixer.
 * This avoids the slow drift caused by generating 32040 Hz while the iPod
 * codec is really consuming 32000 Hz.
 ***************************************************************************/

#include "snes_lite.h"

#define SNES_AUDIO_FULL_RATE 32000
#define SNES_AUDIO_LOW_RATE  22050
#define SNES_AUDIO_BLOCK_FRAMES 256
#define SNES_AUDIO_BLOCKS 16
#define SNES_AUDIO_START_BLOCKS 6
#define SNES_AUDIO_LOW_WATERMARK 3
#define SNES_AUDIO_SAMPLES (SNES_AUDIO_BLOCK_FRAMES * 2)

static int16_t *audio_ring;
static int16_t conceal[SNES_AUDIO_SAMPLES] __attribute__((aligned(4)));
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

    for (frame = 0; frame < SNES_AUDIO_BLOCK_FRAMES; frame++)
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
        int16_t *block = audio_ring + read_block * SNES_AUDIO_SAMPLES;

        *start = block;
        *size = SNES_AUDIO_SAMPLES * sizeof(int16_t);
        last_left = block[SNES_AUDIO_SAMPLES - 2];
        last_right = block[SNES_AUDIO_SAMPLES - 1];
        read_block = (read_block + 1) % SNES_AUDIO_BLOCKS;
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

bool snes_lite_audio_init(void)
{
    unsigned requested;

    if (snes_lite.config.audio == SNES_AUDIO_OFF)
        return false;

    audio_ring = snes_lite_calloc(SNES_AUDIO_BLOCKS * SNES_AUDIO_SAMPLES,
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
    requested = snes_lite.config.audio == SNES_AUDIO_LOW ?
                SNES_AUDIO_LOW_RATE : SNES_AUDIO_FULL_RATE;

#if INPUT_SRC_CAPS != 0
    rb->audio_set_input_source(AUDIO_SRC_PLAYBACK, SRCF_PLAYBACK);
    rb->audio_set_output_source(AUDIO_SRC_PLAYBACK);
#endif
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->mixer_set_frequency(requested);
    snes_lite.audio_rate = rb->mixer_get_frequency();
    if (!snes_lite.audio_rate)
        snes_lite.audio_rate = requested;
    rb->pcmbuf_fade(false, true);
    snes_lite.audio_available = true;
    snes_lite_log("audio init requested=%u actual=%u blocks=%d frames=%d",
                  requested, snes_lite.audio_rate, SNES_AUDIO_BLOCKS,
                  SNES_AUDIO_BLOCK_FRAMES);
    return true;
}

void snes_lite_audio_submit(const int16_t *data, size_t frames)
{
    while (data && frames > 0 && snes_lite.audio_available)
    {
        size_t room = SNES_AUDIO_BLOCK_FRAMES - write_frames;
        size_t take = MIN(frames, room);
        int16_t *destination = audio_ring +
            write_block * SNES_AUDIO_SAMPLES + write_frames * 2;

        rb->memcpy(destination, data, take * 2 * sizeof(int16_t));
        data += take * 2;
        frames -= take;
        write_frames += take;

        if (write_frames == SNES_AUDIO_BLOCK_FRAMES)
        {
            bool queued = false;

            rb->pcm_play_lock();
            if (queued_blocks < SNES_AUDIO_BLOCKS - 1)
            {
                queued_blocks++;
                queued = true;
            }
            rb->pcm_play_unlock();
            if (queued)
            {
                write_block = (write_block + 1) % SNES_AUDIO_BLOCKS;
                if (!snes_lite.audio_started &&
                    queued_blocks >= SNES_AUDIO_START_BLOCKS)
                {
                    rb->mixer_channel_set_amplitude(
                        PCM_MIXER_CHAN_PLAYBACK, MIX_AMP_UNITY);
                    rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                                audio_get_more, NULL, 0);
                    snes_lite.audio_started = true;
                }
            }
            else
                dropped_blocks++;
            write_frames = 0;
        }
    }
}

unsigned snes_lite_audio_rate(void)
{
    if (snes_lite.audio_available && snes_lite.audio_rate)
        return snes_lite.audio_rate;
    return snes_lite.config.audio == SNES_AUDIO_LOW ?
           SNES_AUDIO_LOW_RATE : SNES_AUDIO_FULL_RATE;
}

void snes_lite_audio_buffer_status(unsigned *occupancy,
                                   bool *underrun_likely)
{
    int blocks;

    rb->pcm_play_lock();
    blocks = queued_blocks;
    rb->pcm_play_unlock();
    if (occupancy)
        *occupancy = MIN(100, blocks * 100 / (SNES_AUDIO_BLOCKS - 1));
    if (underrun_likely)
        *underrun_likely = snes_lite.audio_started &&
                           blocks < SNES_AUDIO_LOW_WATERMARK;
}

void snes_lite_audio_pause(bool pause)
{
    if (snes_lite.audio_started &&
        rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) != CHANNEL_STOPPED)
    {
        rb->mixer_channel_play_pause(PCM_MIXER_CHAN_PLAYBACK, !pause);
    }
}

void snes_lite_audio_close(void)
{
    if (!snes_lite.audio_available && !snes_lite.audio_started)
        return;

    rb->pcm_play_lock();
    if (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) != CHANNEL_STOPPED)
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcm_play_unlock();
    snes_lite.audio_started = false;
    snes_lite.audio_available = false;
    queued_blocks = 0;
    rb->pcmbuf_fade(false, false);
    if (old_frequency)
        rb->mixer_set_frequency(old_frequency);
    snes_lite_log("audio dropped=%lu underruns=%lu", dropped_blocks,
                  underruns);
}
