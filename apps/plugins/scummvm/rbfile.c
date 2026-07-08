/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "rbfile.h"

bool scummvm_make_path(char *dst, size_t dst_size, const char *dir,
                       const char *name)
{
    size_t len = rb->strlcpy(dst, dir, dst_size);

    if (len >= dst_size)
        return false;

    if (len > 0 && dst[len - 1] != '/') {
        if (len + 1 >= dst_size)
            return false;
        dst[len++] = '/';
        dst[len] = '\0';
    }

    return rb->strlcat(dst, name, dst_size) < dst_size;
}

static bool file_open_path(struct scummvm_file *file, const char *path,
                           int flags)
{
    rb->memset(file, 0, sizeof(*file));
    file->fd = rb->open(path, flags, 0666);
    if (file->fd < 0)
        return false;

    file->size = rb->filesize(file->fd);
    return true;
}

bool scummvm_file_open_game(struct scummvm_file *file,
                            const struct scummvm_target *target,
                            const char *name)
{
    char path[MAX_PATH];

    if (!scummvm_make_path(path, sizeof(path), target->path, name))
        return false;

    return file_open_path(file, path, O_RDONLY);
}

bool scummvm_file_open_save(struct scummvm_file *file,
                            const struct scummvm_target *target,
                            const char *name, int flags)
{
    char path[MAX_PATH];

    if (!scummvm_make_path(path, sizeof(path), target->savepath, name))
        return false;

    return file_open_path(file, path, flags);
}

void scummvm_file_close(struct scummvm_file *file)
{
    if (file->fd >= 0)
        rb->close(file->fd);

    file->fd = -1;
    file->size = 0;
}

long scummvm_file_read(struct scummvm_file *file, void *buf, long size)
{
    if (file->fd < 0 || size < 0)
        return -1;

    return rb->read(file->fd, buf, size);
}

bool scummvm_file_seek(struct scummvm_file *file, long offset)
{
    if (file->fd < 0)
        return false;

    return rb->lseek(file->fd, offset, SEEK_SET) == offset;
}

long scummvm_file_size(const struct scummvm_file *file)
{
    return file->size;
}
