/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2009 by Michael Sparmann
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

/*
 * iPod Nano 3G NAND driver — STUB (hardware not yet implemented)
 *
 * Architecture notes (do not implement without hardware access):
 *
 *  - The S5L8702 NAND Flash Controller (FMC) base address is NOT defined in
 *    s5l87xx.h.  On S5L8700 the FMC is at 0x3C200000, but on S5L8702 that
 *    address is the clickwheel controller (WHEEL_BASE).  The real S5L8702
 *    FMC base must be determined by OF reverse-engineering or hardware
 *    probing before any register access can be written.
 *
 *  - No Flash Translation Layer (FTL) exists for S5L8702.  The only Rockbox
 *    FTL for Apple NAND targets is ftl-nano2g.c (S5L8700).  A new FTL must
 *    be written or adapted once the FMC base and register layout are known.
 *
 *  - The Nano 4G (also S5L8702) has an identical stub; neither target has
 *    a working NAND driver.
 *
 *  - nand_read_sectors() returns zero-filled sectors (success) so behaviour
 *    is deterministic while the driver remains stubbed. nand_write_sectors()
 *    returns -1 to gate writes until hardware support exists.
 *
 *  - nand_event() uses storage_event_default_handler() so the storage
 *    thread's idle-notification fires at most once (after ~3 s of
 *    inactivity) rather than every 500 ms as it would with an empty stub.
 */

#include "mv.h"
#include "storage.h"
#include <stdbool.h>
#include <string.h>

int nand_init(void)
{
    // TODO
    return 0;
}

void nand_spindown(int seconds)
{
    (void)seconds;
}

void nand_spin(void)
{
}

#ifdef HAVE_STORAGE_FLUSH
int nand_flush(void)
{
    return 0;
}
#endif

int nand_read_sectors(IF_MD(int drive,) sector_t start, int incount,
                     void* inbuf)
{
#ifdef HAVE_MULTIDRIVE
    (void) drive;
#endif

    if (incount < 0 || (incount > 0 && inbuf == NULL))
        return -1;

    if (inbuf != NULL && incount > 0)
        memset(inbuf, 0, (size_t)incount * SECTOR_SIZE);

    (void) start;
    return 0;
}

int nand_write_sectors(IF_MD(int drive,) sector_t start, int count,
                      const void* outbuf)
{
#ifdef HAVE_MULTIDRIVE
    (void) drive;
#endif
    (void) start;
    (void) count;
    (void) outbuf;
    return -1;
}

long nand_last_disk_activity(void)
{
    return 0;
}

int nand_event(long id, intptr_t data)
{
    return storage_event_default_handler(id, data, nand_last_disk_activity(),
                                         STORAGE_NAND);
}

#ifdef STORAGE_GET_INFO
void nand_get_info(IF_MD(int drive,) struct storage_info *info)
{
    IF_MD((void)drive);
    info->sector_size = SECTOR_SIZE;
    info->num_sectors = 0;
    info->vendor = "";
    info->product = "";
    info->revision = "";
}
#endif

/* nand_get_ssd_mode: backlight.c calls storage_get_ssd_mode() unconditionally
 * for STORAGE_NAND targets; return false until real NAND driver is implemented. */
bool nand_get_ssd_mode(void)
{
    return false;
}
