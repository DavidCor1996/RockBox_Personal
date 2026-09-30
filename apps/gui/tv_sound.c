/* SPDX-License-Identifier: GPL-2.0-or-later
 * Original Apple navigation recordings on the short-effects mixer channel.
 * Resident data and a bounded output buffer never borrow playback memory. */
#include "config.h"
#ifdef HAVE_COMPOSITE_VIDEO_OUT
#include <stdint.h>
#include "system.h"
#include "kernel.h"
#include "action.h"
#include "settings.h"
#include "tv_ui.h"
#include "tv_sound.h"
#include "pcm_mixer.h"
#include "tv_apple_sounds.h"

#define TV_SOUND_RATE 44100u
#define TV_SOUND_FRAMES 128
static int16_t sound_buffer[TV_SOUND_FRAMES * 2]
    __attribute__((aligned(4)));
static const int16_t *sound_samples;
static uint32_t sound_frames, sound_phase, sound_step;
static long move_tick;
static bool move_started;

/* Follow the existing mixer rate, including music at 48/96 kHz. Never change
 * the playback clock to accommodate a UI cue. No I/O or allocation here. */
static void sound_get_more(const void **start, size_t *size)
{
    unsigned count = 0;
    while (count < TV_SOUND_FRAMES && (sound_phase >> 16) < sound_frames)
    {
        unsigned index = sound_phase >> 16;
        unsigned next = MIN(index + 1, sound_frames - 1);
        int fraction = (sound_phase >> 8) & 255;
        for (unsigned ch = 0; ch < 2; ch++)
        {
            int value = sound_samples[index * 2 + ch];
            int delta = sound_samples[next * 2 + ch] - value;
            sound_buffer[count * 2 + ch] = value + delta * fraction / 256;
        }
        count++;
        sound_phase += sound_step;
    }
    *start = count ? sound_buffer : NULL;
    *size = count * 2 * sizeof(int16_t);
}

bool tv_sound_action(int action)
{
    if (!tv_ui_active()) return false;
    const int16_t *samples;
    unsigned frames;
    bool move = false;
    switch (action)
    {
        case ACTION_STD_PREV: case ACTION_STD_PREVREPEAT:
        case ACTION_STD_NEXT: case ACTION_STD_NEXTREPEAT:
        case ACTION_TREE_PGLEFT: case ACTION_TREE_PGRIGHT:
        case ACTION_SETTINGS_INC: case ACTION_SETTINGS_INCREPEAT:
        case ACTION_SETTINGS_DEC: case ACTION_SETTINGS_DECREPEAT:
            samples = tv_apple_sound_move;
            frames = ARRAYLEN(tv_apple_sound_move) / 2;
            move = true;
            break;
        case ACTION_STD_OK:
            samples = tv_apple_sound_select;
            frames = ARRAYLEN(tv_apple_sound_select) / 2;
            break;
        case ACTION_STD_CANCEL: case ACTION_STD_MENU:
            samples = tv_apple_sound_back;
            frames = ARRAYLEN(tv_apple_sound_back) / 2;
            break;
        case ACTION_NONE: case ACTION_UNKNOWN:
            /* Pre-button and release events must not add a hardware click
             * before/after the accepted action's Apple recording. */
            return true;
        default:
            return false;
    }
    if (!global_settings.tv_ui_sounds) return true;
    if (move && move_started && TIME_BEFORE(current_tick, move_tick + HZ/10))
        return true;
    move_started = move;
    move_tick = current_tick;
    unsigned rate = mixer_get_frequency();
    if (!rate) return true;

    /* Stop only the previous short effect before replacing its callback data.
     * Core-resident buffers remain valid across menus and plugin transitions. */
    mixer_channel_stop(PCM_MIXER_CHAN_BEEP);
    sound_samples = samples;
    sound_frames = frames;
    sound_phase = 0;
    sound_step = (TV_SOUND_RATE << 16) / rate;
    /* The original Apple recordings are already quiet UI effects. */
    mixer_channel_set_amplitude(PCM_MIXER_CHAN_BEEP, MIX_AMP_UNITY);
    mixer_channel_play_data(PCM_MIXER_CHAN_BEEP, sound_get_more, NULL, 0);
    return true;
}
#endif
