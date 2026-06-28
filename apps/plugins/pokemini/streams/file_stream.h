#ifndef ROCKBOX_POKEMINI_FILE_STREAM_H
#define ROCKBOX_POKEMINI_FILE_STREAM_H

#include <stdint.h>

#define RETRO_VFS_FILE_ACCESS_READ 1
#define RETRO_VFS_FILE_ACCESS_WRITE 2
#define RETRO_VFS_FILE_ACCESS_HINT_NONE 0

typedef struct RFILE
{
    int fd;
} RFILE;

static inline RFILE *filestream_open(const char *path,
                                     unsigned mode,
                                     unsigned hints)
{
    (void)path;
    (void)mode;
    (void)hints;
    return 0;
}

static inline int64_t filestream_read(RFILE *stream, void *data, int64_t size)
{
    (void)stream;
    (void)data;
    (void)size;
    return 0;
}

static inline int64_t filestream_write(RFILE *stream,
                                       const void *data,
                                       int64_t size)
{
    (void)stream;
    (void)data;
    (void)size;
    return 0;
}

static inline void filestream_close(RFILE *stream)
{
    (void)stream;
}

#endif
