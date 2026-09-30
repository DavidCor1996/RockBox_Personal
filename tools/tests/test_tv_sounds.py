#!/usr/bin/env python3
"""Run the production TV sound callbacks, retaining real Apple PCM samples.
The mixer is a channel/lifecycle stub; this does not qualify hardware audio.
"""
import pathlib, re, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[2]
source = (root/'apps/gui/tv_sound.c').read_text()
source = re.sub(r'^#include .*$', '', source, flags=re.M)
names = sorted(set(re.findall(r'\bACTION_[A-Z_]+', source)))
defs = '\n'.join(f'#define {name} {index + 1}' for index, name in enumerate(names))
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define HAVE_COMPOSITE_VIDEO_OUT 1
#define ARRAYLEN(a) (sizeof(a)/sizeof((a)[0]))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define TIME_BEFORE(a,b) ((long)((a)-(b))<0)
#define HZ 100
#define MIX_AMP_UNITY 65536
#define PCM_MIXER_CHAN_BEEP 2
static struct { bool tv_ui_sounds; } global_settings = {true};
static bool active = true, playing;
static long current_tick;
static unsigned rate = 44100, calls, stops;
static void (*callback)(const void **, size_t *);
static bool tv_ui_active(void) { return active; }
static unsigned mixer_get_frequency(void) { return rate; }
static void mixer_channel_stop(int channel)
{
    assert(channel == PCM_MIXER_CHAN_BEEP);
    stops++; playing = false; callback = NULL;
}
static void mixer_channel_set_amplitude(int channel, unsigned amplitude)
{
    assert(channel == PCM_MIXER_CHAN_BEEP && amplitude == MIX_AMP_UNITY);
}
static void mixer_channel_play_data(int channel, void (*cb)(const void **, size_t *),
                                    const void *start, size_t size)
{
    assert(channel == PCM_MIXER_CHAN_BEEP && cb && !start && !size);
    assert(!playing); callback = cb; playing = true; calls++;
}
''' + defs + '\n' + (root/'apps/gui/tv_apple_sounds.h').read_text() + '\n' + source + r'''
static void drain(void)
{
    const void *data = NULL;
    size_t bytes = 0, total = sound_phase / sound_step;
    unsigned guard = 0;
    while (playing)
    {
        callback(&data, &bytes);
        assert(bytes <= TV_SOUND_FRAMES*4 && bytes%4 == 0);
        if (!bytes) { assert(!data); playing = false; break; }
        assert(data && ((uintptr_t)data & 3) == 0);
        if (rate == 44100)
            assert(!memcmp(data, sound_samples + total*2, bytes));
        total += bytes/4;
        assert(++guard < 10000);
    }
    unsigned expected = ((sound_frames << 16) + sound_step - 1) / sound_step;
    assert(total == expected);
    /* Repeated end callbacks remain empty, even with nonzero old output. */
    data = sound_samples; bytes = 100;
    callback(&data, &bytes); assert(!data && !bytes);
}
int main(void)
{
    const int cues[] = {ACTION_STD_NEXT, ACTION_STD_OK, ACTION_STD_CANCEL};
    const unsigned rates[] = {8000, 22050, 44100, 48000, 88200, 96000, 192000};
    for (unsigned r=0; r<ARRAYLEN(rates); r++)
        for (unsigned i=0; i<ARRAYLEN(cues); i++)
        {
            rate=rates[r]; current_tick+=HZ;
            assert(tv_sound_action(cues[i]));
            assert(sound_samples == (i==0 ? tv_apple_sound_move :
                i==1 ? tv_apple_sound_select : tv_apple_sound_back));
            drain();
        }
    unsigned before=calls;
    active=false; assert(!tv_sound_action(ACTION_STD_OK)); active=true;
    assert(tv_sound_action(ACTION_NONE)); /* silent release / timeout */
    assert(tv_sound_action(ACTION_UNKNOWN)); /* silent pre-button */
    assert(!tv_sound_action(10000)); /* unrelated playback action */
    global_settings.tv_ui_sounds=false;
    assert(tv_sound_action(ACTION_STD_OK));
    global_settings.tv_ui_sounds=true;
    rate=0; assert(tv_sound_action(ACTION_STD_OK)); rate=44100;
    assert(calls==before);
    current_tick+=HZ;
    assert(tv_sound_action(ACTION_STD_NEXT)); before=calls;
    for (int i=0;i<HZ/10;i++)
    { assert(tv_sound_action(ACTION_STD_NEXTREPEAT)); current_tick++; }
    assert(calls==before);
    assert(tv_sound_action(ACTION_STD_NEXTREPEAT)); assert(calls==before+1);
    /* Selection and Back respond immediately even after a movement cue. */
    assert(tv_sound_action(ACTION_STD_OK)); assert(calls==before+2);
    assert(tv_sound_action(ACTION_STD_CANCEL)); assert(calls==before+3);
    drain();
    for (int i=0;i<1000;i++)
    {
        current_tick+=HZ/5;
        assert(tv_sound_action(cues[i%3]));
        const void *data=NULL; size_t size=0;
        callback(&data,&size); assert(data && size);
        /* Next action preempts this cue before it has finished. */
    }
    drain();
    assert(stops==calls);
    puts("PASS: original PCM identity at 44.1kHz; bounded resampling at seven rates; 1000 interrupted cues; action gating, TV/off/disabled/release isolation, delayed repeats; only effects mixer channel used");
}
'''
# Playback ownership changes and disk activity are deliberately unavailable to
# the compiled harness, so introducing one also fails compilation/linking.
assert 'if (!rawbutton && tv_sound_action(action))' in (root/'apps/misc.c').read_text()
with tempfile.TemporaryDirectory(prefix='tv-sounds-') as temp:
    c=pathlib.Path(temp)/'test.c'; c.write_text(code)
    exe=c.with_suffix('')
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-g',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
