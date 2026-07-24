#include "picodrive.h"
#include "lib/stdio_compat.h"
#include "upstream/zlib.h"
#include "upstream/unzip/unzip.h"

int inflateInit2(z_stream *stream, int window_bits)
{
    (void)stream;
    (void)window_bits;
    return -1;
}

int inflate(z_stream *stream, int flush)
{
    (void)stream;
    (void)flush;
    return -1;
}

int inflateEnd(z_stream *stream)
{
    (void)stream;
    return Z_OK;
}

int inflateReset(z_stream *stream)
{
    (void)stream;
    return -1;
}

unsigned long crc32(unsigned long crc, const Bytef *buffer, uInt length)
{
    uInt index;

    crc ^= 0xffffffffUL;
    for (index = 0; index < length; index++)
    {
        unsigned int bit;

        crc ^= buffer[index];
        for (bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320UL &
                                (0UL - (crc & 1UL)));
    }
    return crc ^ 0xffffffffUL;
}

gzFile gzopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return NULL;
}

int gzread(gzFile file, void *buffer, unsigned int length)
{
    (void)file;
    (void)buffer;
    (void)length;
    return -1;
}

int gzwrite(gzFile file, const void *buffer, unsigned int length)
{
    (void)file;
    (void)buffer;
    (void)length;
    return -1;
}

int gzclose(gzFile file) { (void)file; return 0; }
int gzeof(gzFile file) { (void)file; return 1; }
long gzseek(gzFile file, long offset, int whence)
{
    (void)file;
    (void)offset;
    (void)whence;
    return -1;
}
int gzsetparams(gzFile file, int level, int strategy)
{
    (void)file;
    (void)level;
    (void)strategy;
    return -1;
}

ZIP *openzip(const char *path)
{
    (void)path;
    return NULL;
}

struct zipent *readzip(ZIP *zip)
{
    (void)zip;
    return NULL;
}

int seekcompresszip(ZIP *zip, struct zipent *entry)
{
    (void)zip;
    (void)entry;
    return -1;
}

void closezip(ZIP *zip)
{
    (void)zip;
}
