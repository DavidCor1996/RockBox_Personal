#include "anarch_platform.h"

#define SFG_PROGRAM_MEMORY static const
#define SFG_PROGRAM_MEMORY_U8(address) ((uint8_t)(*(address)))
#include "upstream/sounds.h"

#define ANARCH_AUDIO_RATE 8000
#define ANARCH_AUDIO_BLOCK_SAMPLES 256
#define ANARCH_AUDIO_BLOCKS 4
#define ANARCH_AUDIO_COMMANDS 16
#define ANARCH_AUDIO_VOICES 6

struct sound_command {
    uint8_t sound;
    uint8_t volume;
};

struct sound_voice {
    uint16_t position;
    uint8_t sound;
    uint8_t volume;
    bool active;
};

static int16_t blocks[ANARCH_AUDIO_BLOCKS][ANARCH_AUDIO_BLOCK_SAMPLES * 2];
static struct sound_command commands[ANARCH_AUDIO_COMMANDS];
static struct sound_voice voices[ANARCH_AUDIO_VOICES];
static volatile unsigned int queued;
static unsigned int read_index;
static unsigned int write_index;
static unsigned int command_read;
static unsigned int command_write;
static unsigned int command_count;
static unsigned int saved_frequency;
static volatile unsigned long underruns;
static bool enabled;
static bool started;
static bool configured;
static bool music_enabled;
static bool paused;

static int16_t clamp_sample(int value)
{
    if (value > 32767)
        return 32767;
    if (value < -32768)
        return -32768;
    return value;
}

static void get_more(const void **start, size_t *size)
{
    static int16_t silence[ANARCH_AUDIO_BLOCK_SAMPLES * 2];

    if (queued > 0)
    {
        *start = blocks[read_index];
        read_index = (read_index + 1) % ANARCH_AUDIO_BLOCKS;
        queued--;
    }
    else
    {
        *start = silence;
        underruns++;
    }
    *size = ANARCH_AUDIO_BLOCK_SAMPLES * 2 * sizeof(int16_t);
}

static void accept_commands(void)
{
    while (command_count > 0)
    {
        struct sound_command command = commands[command_read];
        int i;
        int target = -1;

        command_read = (command_read + 1) % ANARCH_AUDIO_COMMANDS;
        command_count--;
        for (i = 0; i < ANARCH_AUDIO_VOICES; ++i)
            if (!voices[i].active)
            {
                target = i;
                break;
            }
        if (target < 0)
            target = command.sound % ANARCH_AUDIO_VOICES;
        voices[target].position = 0;
        voices[target].sound = command.sound % ANARCH_AUDIO_VOICES;
        voices[target].volume = command.volume;
        voices[target].active = true;
    }
}

static void generate_block(int16_t *output)
{
    int i;

    accept_commands();
    for (i = 0; i < ANARCH_AUDIO_BLOCK_SAMPLES; ++i)
    {
        int sample = 0;
        int voice;

        if (music_enabled)
        {
            int value = SFG_getNextMusicSample();
            value -= SFG_musicTrackAverages[SFG_MusicState.track];
            sample += value * 48;
        }
        for (voice = 0; voice < ANARCH_AUDIO_VOICES; ++voice)
        {
            struct sound_voice *v = &voices[voice];

            if (!v->active)
                continue;
            sample += ((128 - SFG_GET_SFX_SAMPLE(v->sound, v->position)) *
                       (int)v->volume) / 2;
            v->position++;
            if (v->position >= SFG_SFX_SAMPLE_COUNT)
                v->active = false;
        }
        output[i * 2] = clamp_sample(sample);
        output[i * 2 + 1] = output[i * 2];
    }
}

void anarch_audio_init(bool requested)
{
    rb->memset(voices, 0, sizeof(voices));
    queued = 0;
    read_index = 0;
    write_index = 0;
    command_read = 0;
    command_write = 0;
    command_count = 0;
    underruns = 0;
    started = false;
    configured = false;
    music_enabled = false;
    paused = false;
    saved_frequency = rb->mixer_get_frequency();
    enabled = requested && rb->audio_status() == 0;
    if (!enabled || paused)
        return;
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcmbuf_fade(false, true);
    rb->mixer_set_frequency(ANARCH_AUDIO_RATE);
    rb->mixer_channel_set_amplitude(PCM_MIXER_CHAN_PLAYBACK,
                                    MIX_AMP_UNITY);
    configured = true;
}

void anarch_audio_pump(void)
{
    if (!enabled)
        return;
    while (queued < ANARCH_AUDIO_BLOCKS - 1)
    {
        int16_t *output = blocks[write_index];

        generate_block(output);
        rb->pcm_play_lock();
        queued++;
        write_index = (write_index + 1) % ANARCH_AUDIO_BLOCKS;
        rb->pcm_play_unlock();
    }
    if (!started && queued >= 2)
    {
        rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                    get_more, NULL, 0);
        started = true;
    }
}

void anarch_audio_pause(bool should_pause)
{
    if (!configured || paused == should_pause)
        return;
    paused = should_pause;
    if (paused && started)
    {
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        started = false;
    }
}

void anarch_audio_shutdown(void)
{
    long deadline;

    enabled = false;
    command_count = 0;
    music_enabled = false;
    if (!configured)
        return;
    if (started)
    {
        rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
        deadline = *rb->current_tick + MAX(1, HZ / 4);
        while (rb->mixer_channel_status(PCM_MIXER_CHAN_PLAYBACK) !=
               CHANNEL_STOPPED && TIME_BEFORE(*rb->current_tick, deadline))
            rb->sleep(1);
    }
    rb->pcmbuf_fade(false, false);
    if (saved_frequency != 0)
        rb->mixer_set_frequency(saved_frequency);
    started = false;
    configured = false;
    queued = 0;
}

void anarch_audio_sound(uint8_t sound, uint8_t volume)
{
    if (!enabled || paused || sound >= ANARCH_AUDIO_VOICES ||
        command_count >= ANARCH_AUDIO_COMMANDS)
        return;
    commands[command_write].sound = sound;
    commands[command_write].volume = volume;
    command_write = (command_write + 1) % ANARCH_AUDIO_COMMANDS;
    command_count++;
}

void anarch_audio_music(uint8_t command)
{
    if (!enabled)
        return;
    if (command == 0)
        music_enabled = false;
    else if (command == 1)
        music_enabled = true;
    else if (command == 2)
        SFG_nextMusicTrack();
}

unsigned long anarch_audio_underruns(void)
{
    return underruns;
}
