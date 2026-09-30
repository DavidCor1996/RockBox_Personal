#!/usr/bin/env python3
"""Host checks of extracted production code for the small fork imports."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
SOURCE = Path(os.environ.get('ROCKBOX_TEST_SOURCE', REPO))

class ForkFixTests(unittest.TestCase):
    def run_c(self, code):
        with tempfile.TemporaryDirectory(prefix='rb-fork-') as tmp:
            src = Path(tmp) / 'test.c'
            exe = Path(tmp) / 'test'
            src.write_text('#include <assert.h>\n#include <stddef.h>\n#include <string.h>\n' + code)
            subprocess.run(['cc', '-std=gnu99', '-fsanitize=undefined',
                            '-fno-sanitize-recover=all', str(src), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_iap_metadata_lengths(self):
        text = (SOURCE / 'apps/iap/iap-lingo4.c').read_text()
        start = text.index('            switch(cmd)', text.index('/* Return the requested track data */'))
        end = text.index('            break;', text.index('            }', start))
        switch = text[start:end]
        self.run_c(r'''

#define strlcpy test_strlcpy
static size_t expected;
static size_t strlcpy(char *dst, const char *src, size_t n)
{
    size_t len = strlen(src), copied = len < n - 1 ? len : n - 1;
    memcpy(dst, src, copied); dst[copied] = 0; return len;
}
static void iap_send_pkt(unsigned char *data, size_t len)
{
    assert(len == expected + 4);
    assert(len <= 67);
    assert(data[len - 1] == 0);
    for (size_t i = 3; i + 1 < len; i++) assert(data[i] == 'x');
}
static void reply(int cmd, char *tag)
{
    unsigned char data[70] = {4, 0, 0};
    struct { char *title, *artist, *album; } id3 = {tag, tag, tag};
    size_t len;
''' + switch + r'''
}
int main(void)
{
    char tag[1025];
    size_t sizes[] = {0, 1, 62, 63, 64, 70, 1024};
    for (unsigned i = 0; i < sizeof(sizes)/sizeof(*sizes); i++) {
        memset(tag, 'x', sizes[i]); tag[sizes[i]] = 0;
        expected = sizes[i] < 63 ? sizes[i] : 63;
        for (int cmd = 0x20; cmd <= 0x24; cmd += 2) reply(cmd, tag);
    }
    expected = 0;
    for (int cmd = 0x20; cmd <= 0x24; cmd += 2) reply(cmd, NULL);
}
''')

    def test_keymap_field_bounds_and_spaces(self):
        text = (SOURCE / 'apps/plugins/keyremap.c').read_text()
        start = text.index('                    while ((ch = *(pbuf))', text.index('/* PARSE FIELDS'))
        end = text.index('                    if (pact != NULL)', start)
        self.run_c(r'''
static char *parse(char *pbuf)
{
    char ch, *pfirst = pbuf, *pact = NULL;
''' + text[start:end] + r'''
    return pact;
}
int main(void)
{
    char normal[] = "{ACTION, BUTTON, PREBTN}";
    char padded[] = "{ACTION, BUTTON, PREBTN   }";
    char empty[] = "{}", spaces[] = "{   }", broken[] = "{PREBTN  ";
    assert(strcmp(parse(normal), "ACTION, BUTTON, PREBTN") == 0);
    assert(strcmp(parse(padded), "ACTION, BUTTON, PREBTN") == 0);
    assert(strcmp(parse(empty), "") == 0);
    assert(strcmp(parse(spaces), "") == 0);
    assert(parse(broken) == NULL);
}
''')

    def test_aac_elapsed(self):
        text = (SOURCE / 'lib/rbcodec/codecs/aac_bsf.c').read_text()
        start = text.index('static void update_playing_time(void)')
        end = text.index('/* this is the codec entry point */', start)
        self.run_c(r'''
static int calls;
static unsigned long elapsed;
static void set_elapsed(unsigned long n) { calls++; elapsed = n; }
static struct { long offset, first_frame_offset; int bitrate; } metadata;
static struct { __typeof__(metadata) *id3; void (*set_elapsed)(unsigned long); }
    api = {&metadata, set_elapsed}, *ci = &api;
''' + text[start:end] + r'''
int main(void)
{
    metadata.offset = 16010; metadata.first_frame_offset = 10;
    metadata.bitrate = 0; elapsed = 777; update_playing_time();
    assert(calls == 0 && elapsed == 777);
    metadata.bitrate = -1; update_playing_time();
    assert(calls == 0 && elapsed == 777);
    metadata.bitrate = 128; update_playing_time();
    assert(calls == 1 && elapsed == 1000);
    metadata.offset = 10; update_playing_time();
    assert(calls == 2 && elapsed == 0);
}
''')

if __name__ == '__main__':
    unittest.main()
