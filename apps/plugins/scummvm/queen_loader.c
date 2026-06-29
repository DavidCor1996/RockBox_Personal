/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#include "queen_loader.h"
#include "rbfile.h"

struct queen_version {
    long data_size;
    const char *version;
    const char *variant;
};

static const struct queen_version queen_versions[] = {
    { 351775, "aEM10", "Amiga floppy" },
    { 344575, "aEM10", "Amiga floppy" },
    { 22677657, "PEM10", "DOS floppy" },
    { 22157304, "PFM10", "DOS floppy" },
    { 22240013, "PGM10", "DOS floppy" },
    { 190787021, "CEM10", "DOS CD" },
    { 186689095, "CFM10", "DOS CD" },
    { 217648975, "CGM10", "DOS CD" },
    { 190705558, "CHM10", "DOS CD" },
    { 3732177, "PE100", "DOS demo" },
    { 3735447, "PE100", "DOS demo" },
    { 3724538, "PE100", "DOS demo" },
    { 1915913, "PEint", "DOS interview" },
    { 1889658, "PEint", "DOS interview" },
    { 563335, "CE101", "Amiga demo" },
    { 597032, "PE100", "Amiga interview" },
    { 0, NULL, NULL }
};

static bool make_queen_path(const char *dir, const char *file,
                            char *out, size_t out_size)
{
    return scummvm_make_path(out, out_size, dir, file);
}

static bool open_resource_file(const struct scummvm_target *target,
                               char *path,
                               size_t path_size,
                               int *fd)
{
    if (make_queen_path(target->path, "queen.1c", path, path_size)) {
        *fd = rb->open(path, O_RDONLY);
        if (*fd >= 0)
            return true;
    }

    if (make_queen_path(target->path, "queen.1", path, path_size)) {
        *fd = rb->open(path, O_RDONLY);
        if (*fd >= 0)
            return true;
    }

    return false;
}

bool scummvm_queen_loader_probe(const struct scummvm_target *target,
                                char *status,
                                size_t status_size)
{
    char path[MAX_PATH];
    unsigned char header[16];
    int fd = -1;
    long size;
    const struct queen_version *version;

    if (!open_resource_file(target, path, sizeof(path), &fd)) {
        rb->strlcpy(status, "Unable to open queen.1/queen.1c", status_size);
        return false;
    }

    size = rb->filesize(fd);
    if (size < 0) {
        rb->close(fd);
        rb->strlcpy(status, "Unable to size Queen resource", status_size);
        return false;
    }

    if (rb->read(fd, header, sizeof(header)) == (ssize_t)sizeof(header) &&
        header[0] == 'Q' && header[1] == 'T' &&
        header[2] == 'B' && header[3] == 'L') {
        char version_str[7];
        rb->memcpy(version_str, header + 4, 6);
        version_str[6] = '\0';
        rb->snprintf(status, status_size,
                     "Queen rebuilt resource %s (%ld bytes)",
                     version_str, size);
        rb->close(fd);
        return true;
    }

    rb->close(fd);

    for (version = queen_versions; version->version; version++) {
        if (version->data_size == size) {
            rb->snprintf(status, status_size,
                         "Queen %s %s (%ld bytes)",
                         version->version, version->variant, size);
            return true;
        }
    }

    rb->snprintf(status, status_size,
                 "Queen resource opened, unknown size %ld", size);
    return true;
}
