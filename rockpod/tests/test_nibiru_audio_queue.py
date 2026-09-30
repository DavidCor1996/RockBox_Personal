"""Exercise the native PCM worker with an interrupt-context mixer stub."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    start = source.index("static ", source.index(name) - 20)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class NibiruAudioQueueTest(unittest.TestCase):
    def test_interrupt_refill_wrap_underrun_restart_and_shutdown(self):
        source = (ROOT / "apps/plugins/scummvm/agds_runtime.c").read_text()
        declarations = source[source.index("#define AGDS_AUDIO_RATE"):
                              source.index("enum agds_boot_phase")]
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
struct mutex { int held; };
#define PCM_MIXER_CHAN_PLAYBACK 0
#define CHANNEL_STOPPED 0
'''+ declarations + r'''
static struct agds_audio_output storage;
static struct agds_audio_output *agds_audio_output = &storage;
static int16_t agds_audio_mix[AGDS_AUDIO_MIX_FRAMES * 2u];
static unsigned irq, channel, generated, received, remaining = 20, sleeps;
static unsigned starts;
static void (*callback)(const void **, size_t *);
static void lock(struct mutex *m) { assert(!irq && !m->held); m->held = 1; }
static void unlock(struct mutex *m) { assert(!irq && m->held); m->held = 0; }
static void pcm_lock(void) { irq++; }
static void pcm_unlock(void) { assert(irq); irq--; }
static unsigned status(unsigned ch) { (void)ch; return channel; }
static void consume(const void *p, size_t n)
{
    const int16_t *samples = p;
    assert(n == sizeof(agds_audio_mix));
    received++;
    for (unsigned i = 0; i < n / sizeof(*samples); i++)
        assert(samples[i] == (int16_t)received);
}
static void play(unsigned ch, void (*cb)(const void **, size_t *),
                 const void *p, size_t n)
{
    (void)ch;
    assert(!irq && !channel && storage.mutex.held);
    starts++;
    callback = cb;
    channel = 1;
    consume(p, n);
}
static void stop(unsigned ch) { (void)ch; channel = 0; callback = NULL; }
static void schedule(void)
{
    assert(!irq && !storage.mutex.held);
    if (channel) {
        const void *p;
        size_t n;
        pcm_lock();
        callback(&p, &n);
        pcm_unlock();
        if (n) consume(p, n);
        else { assert(p == NULL); channel = 0; }
    }
}
static void nap(int ticks)
{
    assert(ticks == 1);
    schedule();
    if (++sleeps == 3) remaining = 5;
    if (sleeps == 12) storage.quit = true;
}
static const struct {
    void *(*memcpy)(void *, const void *, size_t);
    void (*mutex_lock)(struct mutex *);
    void (*mutex_unlock)(struct mutex *);
    void (*pcm_play_lock)(void);
    void (*pcm_play_unlock)(void);
    unsigned (*mixer_channel_status)(unsigned);
    void (*mixer_channel_play_data)(unsigned,
        void (*)(const void **, size_t *), const void *, size_t);
    void (*mixer_channel_stop)(unsigned);
    void (*yield)(void);
    void (*sleep)(int);
} api = {memcpy, lock, unlock, pcm_lock, pcm_unlock, status, play, stop,
         schedule, nap}, *rb = &api;
/* Stand-in for disk-backed mixing: it must always have thread context and
 * exclusive voice ownership.  Each block carries a sequence number. */
static bool audio_mix_block(int16_t *samples)
{
    assert(!irq && storage.mutex.held);
    if (!remaining) return false;
    remaining--;
    generated++;
    for (unsigned i = 0; i < AGDS_AUDIO_MIX_FRAMES * 2u; i++)
        samples[i] = generated;
    return true;
}
'''
        for name in ("audio_get_more(", "audio_worker(", "audio_flush("):
            harness += function(source, name) + "\n"
        harness += r'''
int main(void)
{
    const void *p;
    size_t n;
    audio_worker();
    assert(!irq && !storage.mutex.held);
    assert(generated == 25 && received == 25 && starts == 2);
    assert(storage.count == 0);
    assert(storage.submitted == 25 && storage.completed == 25);
    assert(!storage.in_flight);
    /* Published queue storage can be reused without altering DMA's buffer. */
    storage.blocks[storage.read][0] = 123;
    storage.count = 1;
    pcm_lock();
    audio_get_more(&p, &n);
    pcm_unlock();
    assert(storage.completed == 25 && storage.in_flight);
    pcm_lock();
    audio_get_more(&p, &n);
    pcm_unlock();
    assert(storage.completed == 26 && !storage.in_flight);
    assert(p == NULL && n == 0);
    p = agds_audio_mix;
    memset(storage.blocks, 0, sizeof(storage.blocks));
    assert(((const int16_t *)p)[0] == 123);
    lock(&storage.mutex);
    audio_flush();
    unlock(&storage.mutex);
    assert(storage.read == 0 && storage.write == 0 && storage.count == 0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="nibiru-audio-test-") as tmp:
            cfile = Path(tmp) / "queue.c"
            binary = Path(tmp) / "queue"
            cfile.write_text(harness)
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", str(cfile),
                            "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True,
                           env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})


if __name__ == "__main__":
    unittest.main()
