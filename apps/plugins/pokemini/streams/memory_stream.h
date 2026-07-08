#ifndef ROCKBOX_POKEMINI_MEMORY_STREAM_H
#define ROCKBOX_POKEMINI_MEMORY_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef struct memstream
{
    uint8_t *buffer;
    uint64_t size;
    uint64_t pos;
    int write;
} memstream_t;

static uint8_t *pm_memstream_buffer;
static uint64_t pm_memstream_size;
static memstream_t pm_memstream;

static inline void memstream_set_buffer(uint8_t *buffer, uint64_t size)
{
    pm_memstream_buffer = buffer;
    pm_memstream_size = size;
}

static inline memstream_t *memstream_open(int writing)
{
    if (!pm_memstream_buffer)
        return NULL;
    pm_memstream.buffer = pm_memstream_buffer;
    pm_memstream.size = pm_memstream_size;
    pm_memstream.pos = 0;
    pm_memstream.write = writing;
    return &pm_memstream;
}

static inline void memstream_close(memstream_t *stream)
{
    (void)stream;
}

static inline uint64_t memstream_pos(memstream_t *stream)
{
    return stream ? stream->pos : 0;
}

static inline int memstream_seek(memstream_t *stream, int64_t offset, int whence)
{
    uint64_t newpos;
    if (!stream)
        return 0;
    newpos = (whence == 1) ? stream->pos + offset : offset;
    if (newpos > stream->size)
        return 0;
    stream->pos = newpos;
    return 1;
}

static inline int memstream_read(memstream_t *stream, void *data, int size)
{
    if (!stream || !data || size < 0)
        return 0;
    if (stream->pos + (uint64_t)size > stream->size)
        size = (int)(stream->size - stream->pos);
    memcpy(data, stream->buffer + stream->pos, size);
    stream->pos += size;
    return size;
}

static inline int memstream_write(memstream_t *stream, const void *data, int size)
{
    if (!stream || !data || size < 0)
        return 0;
    if (stream->pos + (uint64_t)size > stream->size)
        size = (int)(stream->size - stream->pos);
    memcpy(stream->buffer + stream->pos, data, size);
    stream->pos += size;
    return size;
}

#endif
