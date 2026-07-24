#ifndef ROCKBOX_PICODRIVE_ZLIB_STUB_H
#define ROCKBOX_PICODRIVE_ZLIB_STUB_H

#include <stddef.h>

typedef unsigned char Bytef;
typedef unsigned int uInt;
typedef void *gzFile;

typedef struct z_stream_s
{
    Bytef *next_in;
    uInt avail_in;
    Bytef *next_out;
    uInt avail_out;
    void *zalloc;
    void *zfree;
    unsigned long total_out;
} z_stream;

#define Z_OK 0
#define Z_STREAM_END 1
#define Z_NO_FLUSH 0
#define Z_FINISH 4
#define Z_DEFAULT_STRATEGY 0

int inflateInit2(z_stream *stream, int window_bits);
int inflate(z_stream *stream, int flush);
int inflateEnd(z_stream *stream);
int inflateReset(z_stream *stream);
unsigned long crc32(unsigned long crc, const Bytef *buffer, uInt length);
gzFile gzopen(const char *path, const char *mode);
int gzread(gzFile file, void *buffer, unsigned int length);
int gzwrite(gzFile file, const void *buffer, unsigned int length);
int gzclose(gzFile file);
int gzeof(gzFile file);
long gzseek(gzFile file, long offset, int whence);
int gzsetparams(gzFile file, int level, int strategy);

#endif
