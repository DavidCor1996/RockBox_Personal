/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2010 by Thomas Martitz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "file.h"
#include "debug.h"
#include "load_code.h"
#include "string.h"

static enum lc_open_error last_error;

enum lc_open_error lc_open_last_error(void)
{
    return last_error;
}

const char *lc_open_error_string(enum lc_open_error error)
{
    switch (error)
    {
        case LC_OPEN_FILE:
            return "file open";
        case LC_OPEN_HEADER_READ:
            return "header read";
        case LC_OPEN_HEADER_INVALID:
            return "invalid header";
        case LC_OPEN_TOO_LARGE:
            return "image too large";
        case LC_OPEN_SEEK:
            return "file seek";
        case LC_OPEN_IMAGE_READ:
            return "image read";
        case LC_OPEN_OK:
        default:
            return "unknown";
    }
}

/* load binary blob from disk to memory, returning a handle */
void * lc_open(const char *filename, unsigned char *buf, size_t buf_size)
{
    int fd = open(filename, O_RDONLY);
    ssize_t read_size;
    struct lc_header hdr;
    unsigned char *buf_end = buf+buf_size;
    off_t copy_size;
    off_t file_size;

    last_error = LC_OPEN_OK;
    if (fd < 0)
    {
        last_error = LC_OPEN_FILE;
        DEBUGF("Could not open file: %s\n", filename);
        goto error;
    }

#if NUM_CORES > 1
    /* Make sure COP cache is flushed and invalidated before loading */
    {
        int my_core = switch_core(CURRENT_CORE ^ 1);
        switch_core(my_core);
    }
#endif

    /* read the header to obtain the load address */
    read_size = read(fd, &hdr, sizeof(hdr));

    if (read_size != (ssize_t)sizeof(hdr))
    {
        last_error = LC_OPEN_HEADER_READ;
        DEBUGF("Could not read complete header: %s\n", filename);
        goto error_fd;
    }

    /* hdr.end_addr points to the end of the bss section,
     * but there might be idata/icode behind that so the bytes to copy
     * can be larger */
    file_size = filesize(fd);
    if (file_size < (off_t)sizeof(hdr) || hdr.load_addr < buf ||
        hdr.load_addr > buf_end || hdr.end_addr < hdr.load_addr)
    {
        last_error = LC_OPEN_HEADER_INVALID;
        DEBUGF("Invalid binary header: %s\n", filename);
        goto error_fd;
    }

    copy_size = MAX(file_size, (off_t)(hdr.end_addr - hdr.load_addr));

    if (copy_size < 0 || (size_t)copy_size > (size_t)(buf_end - hdr.load_addr))
    {
        last_error = LC_OPEN_TOO_LARGE;
        DEBUGF("Binary doesn't fit into memory: %s\n", filename);
        goto error_fd;
    }

    /* Go back to the beginning to load the on-disk image (including the
     * header). copy_size may be larger than file_size because it also spans
     * the plugin's BSS, which is deliberately absent from the .rock file. */
    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        last_error = LC_OPEN_SEEK;
        DEBUGF("lseek failed: %s\n", filename);
        goto error_fd;
    }

    /* Zero the complete memory image first, then require an exact read of
     * only the bytes that actually exist in the file. */
    memset(hdr.load_addr, 0, copy_size);
    read_size = read(fd, hdr.load_addr, file_size);
    close(fd);

    if (read_size != file_size)
    {
        last_error = LC_OPEN_IMAGE_READ;
        DEBUGF("Could not read complete binary: %s\n", filename);
        goto error;
    }

    /* commit dcache and discard icache */
    commit_discard_idcache();
    /* return a pointer the header, reused by lc_get_header() */
    return hdr.load_addr;

error_fd:
    close(fd);
error:
    return NULL;
}
