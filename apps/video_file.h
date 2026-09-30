/* Core video file access, including the native FAT range above 2 GiB. */
#ifndef VIDEO_FILE_H
#define VIDEO_FILE_H

#include "file.h"

#if !(CONFIG_PLATFORM & PLATFORM_NATIVE) || defined(SIMULATOR)
static inline ssize_t file_read_at(int fildes, void *buf, size_t nbyte,
                                   uint32_t offset)
{
    off_t saved = lseek(fildes, 0, SEEK_CUR);
    ssize_t count;
    if (saved < 0 || lseek(fildes, (off_t)offset, SEEK_SET) < 0)
        return -1;
    count = read(fildes, buf, nbyte);
    if (lseek(fildes, saved, SEEK_SET) < 0)
        return -1;
    return count;
}

static inline int64_t file_size64(int fildes)
{
    return filesize(fildes);
}
#endif

#endif
