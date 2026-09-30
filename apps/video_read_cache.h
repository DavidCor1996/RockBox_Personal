/* Bounded compressed read-ahead, using the player's existing read workspace. */
#ifndef VIDEO_READ_CACHE_H
#define VIDEO_READ_CACHE_H
struct video_read_cache { uint32_t offset; size_t bytes; };
static inline const uint8_t *video_read_cached(int fd, uint8_t *buffer,
    size_t capacity, struct video_read_cache *cache, uint32_t offset, size_t size)
{
    if (!size || size > capacity) return NULL;
    if (offset >= cache->offset && offset - cache->offset <= cache->bytes &&
        size <= cache->bytes - (offset - cache->offset))
        return buffer + offset - cache->offset;
    ssize_t got = file_read_at(fd, buffer, capacity, offset);
    cache->offset = offset;
    cache->bytes = got > 0 ? (size_t)got : 0;
    return cache->bytes >= size ? buffer : NULL;
}
#endif
