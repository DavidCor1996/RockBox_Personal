"""Exercise the actual streaming ring with synthetic PCM, without game data."""

import ctypes
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "apps/plugins/ipodgames"
STUB = r"""
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#undef strstr
#undef strchr
#define MAX_PATH 260
#define MIN(a,b) ((a) < (b) ? (a) : (b))
struct plugin_api {
    void *(*memset)(void *, int, size_t);
    size_t (*strlcpy)(char *, const char *, size_t);
    char *(*strstr)(const char *, const char *);
    char *(*strchr)(const char *, int);
    size_t (*strlen)(const char *);
    void (*pcm_play_lock)(void);
    void (*pcm_play_unlock)(void);
    int (*close)(int);
    int (*snprintf)(char *, size_t, const char *, ...);
    int (*open)(const char *, int, ...);
    off_t (*filesize)(int);
    ssize_t (*read)(int, void *, size_t);
    off_t (*lseek)(int, off_t, int);
    int (*fdprintf)(int, const char *, ...);
};
extern const struct plugin_api *rb;
"""
HARNESS = r"""
#include "plugin.h"
static int opened;
static size_t copy_string(char *out, const char *in, size_t size)
{
    size_t length = strlen(in);
    if (size) { size_t n = MIN(length, size - 1); memcpy(out, in, n); out[n] = 0; }
    return length;
}
static void lock(void) {}
static off_t file_size(int fd) { struct stat s; return fstat(fd, &s) ? -1 : s.st_size; }
static int file_open(const char *name, int flags, ...)
{ int fd = open(name, flags); if (fd >= 0) ++opened; return fd; }
static int file_close(int fd) { int result = close(fd); if (!result) --opened; return result; }
int open_files(void) { return opened; }
static const struct plugin_api api = {
    memset, copy_string, strstr, strchr, strlen, lock, lock, file_close,
    snprintf, file_open, file_size, read, lseek, dprintf
};
const struct plugin_api *rb = &api;
"""


class MusicRingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory(prefix="vortex-music-test-")
        cls.addClassCleanup(cls.workspace.cleanup)
        root = Path(cls.workspace.name)
        (root / "plugin.h").write_text(STUB)
        (root / "harness.c").write_text(HARNESS)
        subprocess.run([
            "gcc", "-std=gnu99", "-shared", "-fPIC", "-O2", "-I", str(root),
            "-I", str(SOURCE), str(root / "harness.c"), str(SOURCE / "music.c"),
            "-o", str(root / "music.so"),
        ], check=True, capture_output=True, text=True)
        cls.lib = ctypes.CDLL(str(root / "music.so"))
        cls.lib.ig_music_init.argtypes = [ctypes.c_char_p]
        cls.lib.ig_music_register.argtypes = [ctypes.c_char_p]
        cls.lib.ig_music_register.restype = ctypes.c_uint
        cls.lib.ig_music_play.argtypes = [ctypes.c_uint]
        cls.lib.ig_music_pause.argtypes = [ctypes.c_bool]
        cls.lib.ig_music_repeat.argtypes = [ctypes.c_uint]
        cls.lib.ig_music_mix.argtypes = [ctypes.POINTER(ctypes.c_int16), ctypes.c_size_t]
        cls.lib.ig_music_active.restype = ctypes.c_bool

    def setUp(self):
        self.files = tempfile.TemporaryDirectory(prefix="vortex-music-data-")
        self.addCleanup(self.files.cleanup)
        self.root = Path(self.files.name)
        (self.root / "assets").mkdir()
        self.lib.ig_music_init(str(self.root).encode())

    def tearDown(self):
        self.lib.ig_music_shutdown()
        self.assertEqual(self.lib.open_files(), 0)

    def track(self, name, samples):
        (self.root / "assets" / (name + ".pcm")).write_bytes(
            b"".join(struct.pack("<hh", *sample) for sample in samples))
        return self.lib.ig_music_register(name.encode())

    def mix(self, frames):
        output = (ctypes.c_int16 * (frames * 2))()
        self.lib.ig_music_mix(output, frames)
        return list(zip(output[::2], output[1::2]))

    def test_wrap_partial_block_pause_and_resume(self):
        samples = [(i % 32768, -(i % 32768)) for i in range(20003)]
        self.lib.ig_music_play(self.track("test", samples))
        actual = []
        for _ in range(40):
            self.lib.ig_music_service()
            self.lib.ig_music_pause(True)
            self.assertEqual(self.mix(17), [(0, 0)] * 17)
            self.lib.ig_music_pause(False)
            actual += self.mix(512)
        self.assertEqual(actual[:len(samples)], samples)
        self.assertEqual(actual[len(samples):], [(0, 0)] * (len(actual) - len(samples)))
        self.lib.ig_music_service()
        self.mix(1)
        self.assertFalse(self.lib.ig_music_active())

    def test_repeat_one_and_all_and_switch(self):
        first = self.track("first", [(100, -100)] * 3)
        self.track("second", [(200, -200)] * 5)
        self.lib.ig_music_repeat(1)
        self.lib.ig_music_play(first)
        self.lib.ig_music_service()
        self.assertEqual(self.mix(12), [(100, -100)] * 12)
        self.lib.ig_music_repeat(2)
        self.lib.ig_music_play(first)
        self.lib.ig_music_service()
        self.assertEqual(self.mix(16), ([(100, -100)] * 3 + [(200, -200)] * 5) * 2)
        self.lib.ig_music_stop()
        self.assertEqual(self.mix(10), [(0, 0)] * 10)
        self.assertEqual(self.lib.open_files(), 0)

    def test_missing_invalid_and_traversal(self):
        self.assertEqual(self.lib.ig_music_register(b"../escape"), 0xffffffff)
        missing = self.lib.ig_music_register(b"missing")
        self.lib.ig_music_play(missing)
        self.lib.ig_music_service()
        self.assertFalse(self.lib.ig_music_active())
        (self.root / "assets/bad.pcm").write_bytes(b"abc")
        self.lib.ig_music_play(self.lib.ig_music_register(b"bad"))
        self.lib.ig_music_service()
        self.assertFalse(self.lib.ig_music_active())
        self.assertEqual(self.lib.open_files(), 0)


if __name__ == "__main__":
    unittest.main()
