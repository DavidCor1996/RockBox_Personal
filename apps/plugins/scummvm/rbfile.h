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

#ifndef SCUMMVM_RBFILE_H
#define SCUMMVM_RBFILE_H

#include "scummvm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct scummvm_file {
    int fd;
    long size;
};

bool scummvm_make_path(char *dst, size_t dst_size, const char *dir,
                       const char *name);
bool scummvm_file_open_game(struct scummvm_file *file,
                            const struct scummvm_target *target,
                            const char *name);
bool scummvm_file_open_save(struct scummvm_file *file,
                            const struct scummvm_target *target,
                            const char *name, int flags);
void scummvm_file_close(struct scummvm_file *file);
long scummvm_file_read(struct scummvm_file *file, void *buf, long size);
bool scummvm_file_seek(struct scummvm_file *file, long offset);
long scummvm_file_size(const struct scummvm_file *file);

#ifdef __cplusplus
}
#endif

#endif
