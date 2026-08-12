#include "uxn_audio.h"

/*
 * Synth semantics are adapted from the official Uxn C emulator at commit
 * 156ef01226c069ad5930c2655a10f22380a387a8. See LICENSE.uxn.
 */

#define AUDIO_FRAMES 512
#define NOTE_PERIOD (UXN_AUDIO_RATE * 0x4000 / 11025)
#define ADSR_STEP (UXN_AUDIO_RATE / 0xf)

struct uxn_voice
{
    uint8_t *addr;
    uint32_t count;
    uint32_t advance;
    uint32_t period;
    uint32_t age;
    uint32_t attack;
    uint32_t decay;
    uint32_t sustain;
    uint32_t release;
    uint16_t index;
    uint16_t length;
    int8_t volume[2];
    bool repeat;
};

static const uint32_t advances[12] = {
    0x80000, 0x879c8, 0x8facd, 0x9837f, 0xa1451, 0xaadc1,
    0xb504f, 0xbfc88, 0xcb2ff, 0xd7450, 0xe411f, 0xf1a1c
};

static struct uxn_voice voices[UXN_AUDIO_VOICES];
static int16_t audio_buffer[AUDIO_FRAMES * 2];
static volatile unsigned int finished_mask;
static unsigned int saved_frequency;
static bool audio_active;

static int32_t envelope(struct uxn_voice *voice, uint32_t age)
{
    if (!voice->release)
        return 0x0888;
    if (age < voice->attack)
        return 0x0888 * age / voice->attack;
    if (age < voice->decay)
        return 0x0444 *
            (2 * voice->decay - voice->attack - age) /
            (voice->decay - voice->attack);
    if (age < voice->sustain)
        return 0x0444;
    if (age < voice->release)
        return 0x0444 * (voice->release - age) /
            (voice->release - voice->sustain);
    voice->advance = 0;
    return 0;
}

static bool render_voice(int instance, int16_t *sample, int16_t *end)
{
    struct uxn_voice *voice = &voices[instance];

    if (!voice->advance || !voice->period)
        return false;
    while (sample < end)
    {
        int32_t value;

        voice->count += voice->advance;
        voice->index += voice->count / voice->period;
        voice->count %= voice->period;
        if (voice->index >= voice->length)
        {
            if (!voice->repeat)
            {
                voice->advance = 0;
                break;
            }
            voice->index %= voice->length;
        }
        value = (int8_t)(voice->addr[voice->index] + 0x80) *
            envelope(voice, voice->age++);
        *sample++ += value * voice->volume[0] / 0x180;
        *sample++ += value * voice->volume[1] / 0x180;
    }
    if (!voice->advance)
        finished_mask |= 1u << instance;
    return true;
}

static void audio_get_more(const void **start, size_t *size)
{
    int i;

    rb->memset(audio_buffer, 0, sizeof(audio_buffer));
    for (i = 0; i < UXN_AUDIO_VOICES; i++)
        render_voice(i, audio_buffer, audio_buffer + ARRAYLEN(audio_buffer));
    *start = audio_buffer;
    *size = sizeof(audio_buffer);
}

void uxn_audio_init(void)
{
    rb->memset(voices, 0, sizeof(voices));
    finished_mask = 0;
    saved_frequency = rb->mixer_get_frequency();
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcmbuf_fade(false, true);
    rb->pcmbuf_set_low_latency(true);
    rb->mixer_set_frequency(UXN_AUDIO_RATE);
    rb->mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK,
                                audio_get_more, NULL, 0);
    audio_active = true;
}

void uxn_audio_shutdown(void)
{
    if (!audio_active)
        return;
    rb->mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    rb->pcmbuf_set_low_latency(false);
    rb->pcmbuf_fade(false, false);
    if (saved_frequency)
        rb->mixer_set_frequency(saved_frequency);
    rb->memset(voices, 0, sizeof(voices));
    audio_active = false;
}

uint8_t uxn_audio_dei(int instance, uint8_t port)
{
    struct uxn_voice *voice = &voices[instance];
    uint8_t result;

    rb->pcm_play_lock();
    if (port == 0x2)
    {
        uint8_t *device = &uxn.dev[0x30 + instance * 0x10];
        POKE2(device + 0x2, voice->index);
    }
    if (port == 0x4)
    {
        int i;
        int32_t sum[2] = {0, 0};

        if (voice->advance && voice->period)
        {
            for (i = 0; i < 2; i++)
            {
                if (!voice->volume[i])
                    continue;
                sum[i] = 1 + envelope(voice, voice->age) *
                    voice->volume[i] / 0x800;
                if (sum[i] > 0xf)
                    sum[i] = 0xf;
            }
        }
        result = sum[0] << 4 | sum[1];
    }
    else
    {
        result = uxn.dev[0x30 + instance * 0x10 + port];
    }
    rb->pcm_play_unlock();
    return result;
}

void uxn_audio_deo(int instance, uint8_t port)
{
    struct uxn_voice *voice;
    uint8_t *device;
    uint8_t pitch;
    uint16_t addr;
    uint16_t adsr;

    if (port != 0xf)
        return;
    device = &uxn.dev[0x30 + instance * 0x10];
    voice = &voices[instance];
    pitch = device[0xf] & 0x7f;
    addr = PEEK2(device + 0xc);
    adsr = PEEK2(device + 0x8);

    rb->pcm_play_lock();
    voice->length = PEEK2(device + 0xa);
    if (voice->length > UXN_PAGE_SIZE - addr)
        voice->length = UXN_PAGE_SIZE - addr;
    voice->addr = &uxn.ram[addr];
    voice->volume[0] = device[0xe] >> 4;
    voice->volume[1] = device[0xe] & 0xf;
    voice->repeat = !(device[0xf] & 0x80);
    if (pitch < 108 && voice->length)
        voice->advance = advances[pitch % 12] >> (8 - pitch / 12);
    else
        voice->advance = 0;
    voice->attack = ADSR_STEP * (adsr >> 12);
    voice->decay = ADSR_STEP * (adsr >> 8 & 0xf) + voice->attack;
    voice->sustain = ADSR_STEP * (adsr >> 4 & 0xf) + voice->decay;
    voice->release = ADSR_STEP * (adsr & 0xf) + voice->sustain;
    voice->age = 0;
    voice->index = 0;
    voice->count = 0;
    if (voice->length <= 0x100 && voice->length)
        voice->period = NOTE_PERIOD * 337 / 2 / voice->length;
    else
        voice->period = NOTE_PERIOD;
    finished_mask &= ~(1u << instance);
    rb->pcm_play_unlock();
}

void uxn_audio_poll(void)
{
    unsigned int pending;
    int i;

    rb->pcm_play_lock();
    pending = finished_mask;
    finished_mask = 0;
    rb->pcm_play_unlock();
    for (i = 0; i < UXN_AUDIO_VOICES; i++)
    {
        if (pending & 1u << i)
            uxn_eval(PEEK2(&uxn.dev[0x30 + i * 0x10]));
    }
}
