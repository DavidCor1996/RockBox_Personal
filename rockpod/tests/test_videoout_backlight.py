"""Execute the real backlight transition code with a fake hardware backend.

The harness extracts production function bodies and event cases so the test
checks their behavior without requiring the Rockbox scheduler or an iPod.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, declaration):
    start = source.index(declaration)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def test_videoout_backlight_transitions():
    source = (ROOT / 'firmware/backlight.c').read_text()
    cases = source[source.index('            case BACKLIGHT_VIDEOOUT_ACTIVE:'):
                   source.index('            case BACKLIGHT_TMO_CHANGED:')]
    cases = cases.replace('#endif', '')
    bodies = '\n'.join(function(source, declaration) for declaration in (
        'static inline void do_backlight_off(void)',
        'static void backlight_update_state(void)',
        'void backlight_set_videoout_active(bool active)',
        'void backlight_set_videoout_off(bool enabled)',
    ))
    harness = r'''
#include <assert.h>
#include <stdbool.h>
#define HAVE_VIDEOOUT_BACKLIGHT_OFF
#define CONFIG_BACKLIGHT_FADING 0
#define BACKLIGHT_FADING_SW_SETTING 1
#define BACKLIGHT_FADING_SW_HW_REG 2
#define BACKLIGHT_FADE_IN_THREAD 0
#define UNLIKELY(x) (x)
enum { BACKLIGHT_VIDEOOUT_ACTIVE, BACKLIGHT_VIDEOOUT_OFF };
static bool backlight_videoout_active, backlight_videoout_off = true;
static bool lit;
static int timeout = 30, backlight_timer, backlight_queue;
static struct { int id, data; } events[64];
static int head, tail;
static int backlight_get_current_timeout(void) { return timeout; }
static void backlight_hw_off(void) { lit = false; }
static void backlight_hw_on(void) { lit = true; }
static void queue_post(int *q, int id, int data)
{
    (void)q;
    assert(tail < 64);
    events[tail].id = id; events[tail++].data = data;
}
''' + bodies + r'''
static void drain(void)
{
    while (head < tail)
    {
        struct { int id, data; } ev = {events[head].id, events[head].data};
        head++;
        switch (ev.id) {
''' + cases + r'''
        }
    }
}
int main(void)
{
    /* Default on must leave an undocked player's normal timeout alone. */
    backlight_update_state(); assert(lit && backlight_timer == 30);
    backlight_set_videoout_active(true);
    assert(lit); /* Producer posts; hardware is changed only by the owner. */
    drain(); assert(!lit && backlight_timer == 0);
    for (int i = 0; i < 20; i++) {
        backlight_update_state(); assert(!lit); /* buttons/charger/hold */
    }
    backlight_set_videoout_off(false); drain(); assert(lit);
    backlight_set_videoout_off(true); drain(); assert(!lit);
    backlight_set_videoout_active(false); drain(); assert(lit);
    timeout = -1; /* Do not override the user's always-off preference. */
    backlight_set_videoout_active(true); drain(); assert(!lit);
    backlight_set_videoout_active(false); drain(); assert(!lit);
    timeout = 0; /* Always-on still yields to active TV output. */
    backlight_update_state(); assert(lit);
    backlight_set_videoout_active(true); drain(); assert(!lit);
    backlight_set_videoout_active(false);
    backlight_set_videoout_active(true);
    backlight_set_videoout_off(false); drain(); assert(lit);
    backlight_set_videoout_off(true); drain(); assert(!lit);
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='videoout-backlight-test-') as temp:
        src = Path(temp) / 'test.c'
        exe = Path(temp) / 'test'
        src.write_text(harness)
        subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                        str(src), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
