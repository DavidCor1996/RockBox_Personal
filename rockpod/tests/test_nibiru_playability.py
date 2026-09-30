"""Native regression checks for streamed effects and touch-wheel pointing."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from rockpod.tests.test_nibiru_audio_queue import ROOT, function


def run_c(source):
    with tempfile.TemporaryDirectory(prefix="nibiru-playability-") as temp:
        cfile = Path(temp) / "test.c"
        binary = Path(temp) / "test"
        cfile.write_text(source)
        subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", str(cfile), "-lm",
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True,
                       env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})


class NibiruPlayabilityTest(unittest.TestCase):
    def test_short_effect_retrigger_keeps_cached_samples(self):
        source = (ROOT / "apps/plugins/scummvm/agds_runtime.c").read_text()
        run_c(r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define AGDS_AUDIO_STREAM_BYTES (16u * 1024u)
#define MIN(a,b) ((a) < (b) ? (a) : (b))
struct scummvm_file { long pos; };
struct agds_audio_voice {
    struct scummvm_file file;
    uint32_t data_size, data_pos, resource_start, data_start, phase;
    uint16_t buffered, consumed;
    bool have_sample;
};
static struct agds_audio_voice agds_audio_voices[8];
static struct { unsigned char streams[8][AGDS_AUDIO_STREAM_BYTES]; } output;
static const typeof(output) *unused;
static typeof(output) *agds_audio_output = &output;
static unsigned reads, seeks;
static bool scummvm_file_seek(struct scummvm_file *file, long offset)
{ file->pos = offset; seeks++; return true; }
static long scummvm_file_read(struct scummvm_file *file, void *buffer, long size)
{
    unsigned char *p = buffer;
    reads++;
    for (long i = 0; i < size; i++) p[i] = (file->pos + i) & 255;
    file->pos += size;
    return size;
}
'''.replace('static const typeof(output) *unused;\n', '')
             .replace('static typeof(output) *agds_audio_output = &output;',
                      '#define agds_audio_output (&output)') +
              function(source, "audio_voice_rewind(") + "\n" +
              function(source, "audio_voice_byte(") + r'''
int main(void)
{
    struct agds_audio_voice *voice = &agds_audio_voices[3];
    unsigned char value;
    voice->data_start = 44;
    voice->data_size = 101;
    for (unsigned repeat = 0; repeat < 10; repeat++) {
        assert(audio_voice_rewind(voice));
        for (unsigned i = 0; i < voice->data_size; i++) {
            assert(audio_voice_byte(voice, &value));
            assert(value == ((44 + i) & 255));
        }
        assert(!audio_voice_byte(voice, &value));
    }
    assert(reads == 1 && seeks == 1);
    voice->data_size = 2 * AGDS_AUDIO_STREAM_BYTES + 7;
    for (unsigned repeat = 0; repeat < 2; repeat++) {
        assert(audio_voice_rewind(voice));
        for (unsigned i = 0; i < voice->data_size; i++) {
            assert(audio_voice_byte(voice, &value));
            assert(value == ((44 + i) & 255));
        }
    }
    assert(reads == 7 && seeks == 3);
    return 0;
}
''')

    def test_touch_glide_lift_button_brake_and_saved_speed(self):
        source = (ROOT / "apps/plugins/scummvm/agds_pointer.h").read_text()
        source = source.replace(function(source, "agds_pointer_load_settings("), "")
        run_c(r'''
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#define HAVE_WHEEL_POSITION
#define HZ 100
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define TIME_BEFORE(a,b) ((a) < (b))
#define BUTTON_MENU 1
#define BUTTON_PLAY 2
#define BUTTON_LEFT 4
#define BUTTON_RIGHT 8
#define BUTTON_SELECT 16
struct scummvm_backend {
    int cursor_x, cursor_y, last_wheel, wheel_velocity, wheel_direction;
    int glide_x, glide_y, frac_x, frac_y;
    long last_wheel_tick, glide_since, glide_tick, glide_emit;
    bool mouse_down, wheel_available;
};
static long now = 100;
static int wheel = -1, buttons;
static int wheel_status(void) { return wheel; }
static long button_status(void) { return buttons; }
static int fp14_cos(int angle)
{ return (int)lround(cos(angle * 3.141592653589793 / 180) * 16384); }
static int fp14_sin(int angle)
{ return (int)lround(sin(angle * 3.141592653589793 / 180) * 16384); }
static const struct {
    long *current_tick;
    int (*wheel_status)(void);
    long (*button_status)(void);
} api = { &now, wheel_status, button_status }, *rb = &api;
''' + source + r'''
int main(void)
{
    struct scummvm_backend state = { .cursor_x = 100, .cursor_y = 100 };
    agds_pointer_reset(&state);
    wheel = 24;
    assert(agds_pointer_poll(&state));
    assert(state.cursor_x == 101 && state.cursor_y == 100);
    for (int i = 0; i < HZ; i += 2) { now += 2; agds_pointer_poll(&state); }
    assert(state.cursor_x > 150 && state.cursor_x < 190);
    int stopped = state.cursor_x;
    wheel = -1;
    now += HZ;
    assert(!agds_pointer_poll(&state) && state.cursor_x == stopped);
    buttons = BUTTON_SELECT;
    wheel = 24;
    assert(!agds_pointer_poll(&state) && state.cursor_x == stopped);
    buttons = 0;
    agds_pointer_settings.reverse_wheel = true;
    assert(agds_pointer_poll(&state) && state.cursor_x == stopped - 1);
    int distances[2];
    agds_pointer_settings.reverse_wheel = false;
    for (int speed = 1; speed <= 4; speed += 3) {
        memset(&state, 0, sizeof(state));
        state.cursor_y = 100;
        agds_pointer_reset(&state);
        agds_pointer_settings.pointer_speed = speed;
        agds_pointer_poll(&state);
        for (int i = 0; i < HZ; i += 2) { now += 2; agds_pointer_poll(&state); }
        distances[speed == 4] = state.cursor_x;
    }
    assert(distances[1] > distances[0]);
    return 0;
}
''')


if __name__ == "__main__":
    unittest.main()
