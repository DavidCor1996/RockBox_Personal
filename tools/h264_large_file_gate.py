#!/usr/bin/env python3
"""Exercise native 32-bit file bounds and AAC callbacks across 2 GiB."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


def main():
    files = (ROOT / 'firmware/common/file.c').read_text()
    audio = (ROOT / 'apps/video_audio.c').read_text()
    m4a = (ROOT / 'lib/rbcodec/codecs/libm4a/m4a.c').read_text()
    # Use the production bounds logic, replacing only FAT sector transfer.
    bounds = files[files.index('static ssize_t readwrite('):]
    bounds = bounds[:bounds.index('    int rc = 0;')]
    bounds += '''
    for (size_t i = 0; i < nbyte; i++)
        ((unsigned char *)buf)[i] = (file->offset + i) % 251;
    file->offset += nbyte;
    return nbyte;
}\n'''
    callbacks = audio[audio.index('static bool audio_refill('):
                      audio.index('static void audio_seek_complete(')]
    code = PRELUDE + bounds
    code += function(files, 'ssize_t file_read_at(')
    code += function(files, 'int64_t file_size64(')
    code += callbacks + function(m4a, 'void stream_read(') + TEST
    with tempfile.TemporaryDirectory(prefix='h264-large-file-') as directory:
        path = Path(directory)
        (path / 'gate.c').write_text(code)
        subprocess.run(['cc', '-m32', '-std=gnu99', '-O2', '-Wall',
                        '-Wextra', '-Wno-unused-parameter',
                        str(path / 'gate.c'), '-o', str(path / 'gate')],
                       check=True)
        subprocess.run([str(path / 'gate')], check=True)


PRELUDE = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#define LOGF_ENABLE
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define FILE_SIZE_MAX INT32_MAX
#define file_size_t uint32_t
#define FD_WRONLY 1
#define FD_APPEND 2
#define GET_FILESTR(a,b) ((b) == 1 ? &descriptor : NULL)
#define RELEASE_FILESTR(a,b) ((void)0)
#define FILE_ERROR_RETURN(e,r) return (r)
#define FILE_ERROR(e,r) do { rc = (r); goto file_error; } while (0)
static uint32_t disk_size;
static struct filestr_desc {
    uint32_t offset;
    uint32_t *sizep;
    struct { int flags; } stream;
} descriptor = { .offset = 17, .sizep = &disk_size };
static size_t audio_file_size, audio_file_pos, audio_file_buffer_offset;
static size_t audio_file_buffer_length;
static unsigned char audio_file_buffer[8192];
static int audio_fd = 1;
static struct { off_t offset; } video_id3;
static struct codec_api {
    off_t curpos, filesize;
    size_t (*read_filebuf)(void *, size_t);
} video_ci;
typedef struct { struct codec_api *ci; int eof; } stream_t;
'''
TEST = r'''
int main(void)
{
    assert(sizeof(off_t) == 4 && sizeof(size_t) == 4);
    disk_size = 0x90000000u;
    assert(file_size64(1) == 0x90000000LL);
    assert(file_size64(-1) < 0);
    unsigned char buffer[64];
    assert(file_read_at(1, buffer, 64, 0x80000010u) == 64);
    assert(descriptor.offset == 17);
    for (size_t i = 0; i < 64; i++)
        assert(buffer[i] == (0x80000010u + i) % 251);
    descriptor.offset = INT32_MAX - 10;
    assert(readwrite(&descriptor, buffer, 64, false, FILE_SIZE_MAX) == 10);
    assert(readwrite(&descriptor, buffer, 64, false, FILE_SIZE_MAX) < 0);
    descriptor.offset = 17;
    descriptor.stream.flags = FD_WRONLY;
    assert(file_read_at(1, buffer, 64, 0) < 0);
    descriptor.stream.flags = 0;
    audio_file_size = disk_size;
    video_ci.filesize = (off_t)audio_file_size;
    video_ci.read_filebuf = audio_read_filebuf;
    stream_t stream = { .ci = &video_ci };
    assert(audio_seek_buffer(0x7ffffff0u));
    stream_read(&stream, 64, buffer);
    assert(!stream.eof);
    assert(audio_file_pos == 0x80000030u);
    assert((size_t)video_ci.curpos == audio_file_pos);
    for (size_t i = 0; i < 64; i++)
        assert(buffer[i] == (0x7ffffff0u + i) % 251);
    size_t actual;
    const unsigned char *data = audio_request_buffer(&actual, 8000);
    assert(actual == 8000 && data[0] == audio_file_pos % 251);
    audio_advance_buffer(actual);
    assert((size_t)video_ci.curpos == 0x80001f70u);
    assert(audio_seek_buffer(disk_size - 10));
    assert(audio_read_filebuf(buffer, 64) == 10);
    assert(audio_read_filebuf(buffer, 64) == 0);
    assert(!audio_seek_buffer((size_t)disk_size + 1));
    assert(audio_seek_buffer(0));
    assert(audio_read_filebuf(buffer, 64) == 64);
    assert(buffer[0] == 0);
    disk_size = audio_file_size = UINT32_MAX;
    video_ci.filesize = (off_t)audio_file_size;
    assert(audio_seek_buffer(UINT32_MAX - 10));
    assert(audio_read_filebuf(buffer, 64) == 10);
    assert(audio_file_pos == UINT32_MAX);
    assert(file_read_at(1, buffer, 64, UINT32_MAX) == 0);
    puts("Native 32-bit large-file/AAC boundary gate passed");
    return 0;
}
'''

if __name__ == '__main__':
    main()
