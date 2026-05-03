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



#include <config.h>
#include <cpu.h>
#include <nand-target.h>
#include <ftl-target.h>
#include <string.h>
#include "system.h"
#include "kernel.h"
#include "panic.h"
#include "debug.h"

#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
extern int printf(const char *format, ...);
#define FTL_PROGRESS(...) do { printf(__VA_ARGS__); } while (0)
#else
#define FTL_PROGRESS(...) do { } while (0)
#endif



#define FTL_COPYBUF_SIZE 32
#define FTL_WRITESPARE_SIZE 32
//#define FTL_FORCEMOUNT



#ifdef FTL_FORCEMOUNT
#ifndef FTL_READONLY
#define FTL_READONLY
#endif
#endif


#ifdef FTL_READONLY
uint32_t ftl_write(uint32_t sector, uint32_t count, const void* buffer)
{
    (void)sector;
    (void)count;
    (void)buffer;
    return -1;
}
uint32_t ftl_sync(void)
{
    return 0;
}
#endif



/* Keeps the state of a scattered page block.
   This structure is used in memory only, not on flash,
   but it equals the one the OFW uses. */
struct ftl_log_type
{

    /* The ftl_cxt.nextblockusn at the time the block was allocated,
       needed in order to be able to remove the oldest ones first. */
    uint32_t usn;

    /* The vBlock number at which the scattered pages are stored */
    uint16_t scatteredvblock;

    /* the lBlock number for which those pages are */
    uint16_t logicalvblock;

    /* Pointer to ftl_offsets, contains the mapping which lPage is
       currently stored at which scattered vPage. */
    uint16_t* pageoffsets;

    /* Pages used in the vBlock, i.e. next page number to be written */
    uint16_t pagesused;

    /* Pages that are still up to date in this block, i.e. need to be
       moved when this vBlock is deallocated. */
    uint16_t pagescurrent;

    /* A flag whether all pages are still sequential in this block.
       Initialized to 1 on allocation, zeroed as soon as anything is
       written out of sequence, so that the block will need copying
       when committing to get the pages back into the right order.
       This is used to half the number of block erases needed when
       writing huge amounts of sequential data. */
    uint32_t issequential;

} __attribute__((packed));


/* Keeps the state of the FTL, both on flash and in memory */
struct ftl_cxt_type
{

    /* Update sequence number of the FTL context, decremented
       every time a new revision of FTL meta data is written. */
    uint32_t usn;

    /* Update sequence number for user data blocks. Incremented
       every time a portion of user pages is written, so that
       a consistency check can determine which copy of a user
       page is the most recent one. */
    uint32_t nextblockusn;

    /* Count of currently free pages in the block pool */
    uint16_t freecount;

    /* Index to the first free hyperblock in the blockpool ring buffer */
    uint16_t nextfreeidx;

    /* This is a counter that is used to better distribute block
       wear. It is incremented on every block erase, and if it
       gets too high (300 on writes, 20 on sync), the most and
       least worn hyperblock will be swapped (causing an additional
       block write) and the counter will be decreased by 20. */
    uint16_t swapcounter;

    /* Ring buffer of currently free hyperblocks. nextfreeidx is the
       index to freecount free ones, the other ones are currently
       allocated for scattered page hyperblocks. */
    uint16_t blockpool[0x14];

    /* Alignment to 32 bits */
    uint16_t field_36;

    /* vPages where the block map is stored */
    uint32_t ftl_map_pages[8];

    /* Probably additional map page number space for bigger chips */
    uint8_t field_58[0x28];

    /* vPages where the erase counters are stored */
    uint32_t ftl_erasectr_pages[8];

    /* Seems to be padding */
    uint8_t field_A0[0x70];

    /* Pointer to ftl_map used by Whimory, not used by us */
    uint32_t ftl_map_ptr;

    /* Pointer to ftl_erasectr used by Whimory, not used by us */
    uint32_t ftl_erasectr_ptr;

    /* Pointer to ftl_log used by Whimory, not used by us */
    uint32_t ftl_log_ptr;

    /* Flag used to indicate that some erase counter pages should be committed
       because they were changed more than 100 times since the last commit. */
    uint32_t erasedirty;

    /* Seems to be unused */
    uint16_t field_120;

    /* vBlocks used to store the FTL context, map, and erase
       counter pages. This is also a ring buffer, and the oldest
       page gets swapped with the least used page from the block
       pool ring buffer when a new one is allocated. */
    uint16_t ftlctrlblocks[3];

    /* The last used vPage number from ftlctrlblocks */
    uint32_t ftlctrlpage;

    /* Set on context sync, reset on write, so obviously never
       zero in the context written to the flash */
    uint32_t clean_flag;

    /* Seems to be unused, but gets loaded from flash by Whimory. */
    uint8_t field_130[0x15C];

} __attribute__((packed));


/* Keeps the state of the bank's VFL, both on flash and in memory.
   There is one of these per bank. */
struct ftl_vfl_cxt_type
{
    /* Cross-bank update sequence number, incremented on every VFL
       context commit on any bank. */
    uint32_t usn;

    /* See ftl_cxt.ftlctrlblocks. This is stored to the VFL contexts
       in order to be able to find the most recent FTL context copy
       when mounting the FTL. The VFL context number this will be
       written to on an FTL context commit is chosen semi-randomly. */
    uint16_t ftlctrlblocks[3];

    /* Alignment to 32 bits */
    uint8_t field_A[2];

    /* Decrementing update counter for VFL context commits per bank */
    uint32_t updatecount;

    /* Number of the currently active VFL context block, it's an index
       into vflcxtblocks. */
    uint16_t activecxtblock;

    /* Number of the first free page in the active VFL context block */
    uint16_t nextcxtpage;

    /* Seems to be unused */
    uint8_t field_14[4];

    /* Incremented every time a block erase error leads to a remap,
       but doesn't seem to be read anywhere. */
    uint16_t field_18;

    /* Number of spare blocks used */
    uint16_t spareused;

    /* pBlock number of the first spare block */
    uint16_t firstspare;

    /* Total number of spare blocks */
    uint16_t sparecount;

    /* Block remap table. Contains the vBlock number the n-th spare
       block is used as a replacement for. 0 = unused, 0xFFFF = bad. */
    uint16_t remaptable[0x334];

    /* Bad block table. Each bit represents 8 blocks. 1 = OK, 0 = Bad.
       If the entry is zero, you should look at the remap table to see
       if the block is remapped, and if yes, where the replacement is. */
    uint8_t bbt[0x11A];

    /* pBlock numbers used to store the VFL context. This is a ring
       buffer. On a VFL context write, always 8 pages are written,
       and it passes if at least 4 of them can be read back. */
    uint16_t vflcxtblocks[4];

    /* Blocks scheduled for remapping are stored at the end of the
       remap table. This is the first index used for them. */
    uint16_t scheduledstart;

    /* Probably padding */
    uint8_t field_7AC[0x4C];

    /* First checksum (addition) */
    uint32_t checksum1;

    /* Second checksum (XOR), there is a bug in whimory regarding this. */
    uint32_t checksum2;

} __attribute__((packed));


/* Layout of the spare bytes of each page on the flash */
union ftl_spare_data_type
{

    /* The layout used for actual user data (types 0x40 and 0x41) */
    struct ftl_spare_data_user_type
    {

        /* The lPage, i.e. Sector, number */
        uint32_t lpn;

        /* The update sequence number of that page,
           copied from ftl_cxt.nextblockusn on write */
        uint32_t usn;

        /* Seems to be unused */
        uint8_t field_8;

        /* Type field, 0x40 (data page) or 0x41
           (last data page of hyperblock) */
        uint8_t type;

        /* ECC mark, usually 0xFF. If an error occurred while reading the
           page during a copying operation earlier, this will be 0x55. */
        uint8_t eccmark;

        /* Seems to be unused */
        uint8_t field_B;

        /* ECC data for the user data */
        uint8_t dataecc[0x28];

        /* ECC data for the first 0xC bytes above */
        uint8_t spareecc[0xC];

    } __attribute__((packed)) user;

    /* The layout used for meta data (other types) */
    struct ftl_spare_data_meta_type
    {

        /* ftl_cxt.usn for FTL stuff, ftl_vfl_cxt.updatecount for VFL stuff */
        uint32_t usn;

        /* Index of the thing inside the page,
           for example number / index of the map or erase counter page */
        uint16_t idx;

        /* Seems to be unused */
        uint8_t field_6;

        /* Seems to be unused */
        uint8_t field_7;

        /* Seems to be unused */
        uint8_t field_8;

       /* Type field:
            0x43: FTL context page
            0x44: Block map page
            0x46: Erase counter page
            0x47: "FTL is currently mounted", i.e. unclean shutdown, mark
            0x80: VFL context page */
        uint8_t type;

        /* ECC mark, usually 0xFF. If an error occurred while reading the
           page during a copying operation earlier, this will be 0x55. */
        uint8_t eccmark;

        /* Seems to be unused */
        uint8_t field_B;

        /* ECC data for the user data */
        uint8_t dataecc[0x28];

        /* ECC data for the first 0xC bytes above */
        uint8_t spareecc[0xC];

    } __attribute__((packed)) meta;

};


/* Keeps track of troublesome blocks, only in memory, lost on unmount. */
struct ftl_trouble_type
{

    /* vBlock number of the block giving trouble */
    uint16_t block;

    /* Bank of the block giving trouble */
    uint8_t bank;

    /* Error counter, incremented by 3 on error, decremented by 1 on erase,
       remaping will be done when it reaches 6. */
    uint8_t errors;

} __attribute__((packed));



/* Pointer to an info structure regarding the flash type used */
const struct nand_device_info_type* ftl_nand_type;

/* Number of banks we detected a chip on */
uint32_t ftl_banks;

/* Block map, used vor pBlock to vBlock mapping */
static uint16_t ftl_map[0x2000];

/* VFL context for each bank */
static struct ftl_vfl_cxt_type ftl_vfl_cxt[4];

/* FTL context */
static struct ftl_cxt_type ftl_cxt;

/* Temporary data buffers for internal use by the FTL */
static uint8_t ftl_buffer[0x800] STORAGE_ALIGN_ATTR;

/* Temporary spare byte buffer for internal use by the FTL */
static union ftl_spare_data_type ftl_sparebuffer[FTL_WRITESPARE_SIZE] STORAGE_ALIGN_ATTR;


#ifndef FTL_READONLY

/* Lowlevel BBT for each bank */
static uint8_t ftl_bbt[4][0x410];

/* Erase counters for the vBlocks */
static uint16_t ftl_erasectr[0x2000];

/* Used by ftl_log */
static uint16_t ftl_offsets[0x11][0x200];

/* Structs keeping record of scattered page blocks */
static struct ftl_log_type ftl_log[0x11];

/* Global cross-bank update sequence number of the VFL context */
static uint32_t ftl_vfl_usn;

/* Keeps track (temporarily) of troublesome blocks */
static struct ftl_trouble_type ftl_troublelog[5];

/* Counts erase counter page changes, after 100 of them the affected
   page will be committed to the flash. */
static uint8_t ftl_erasectr_dirt[8];

/* Buffer needed for copying pages around while moving or committing blocks.
   This can't be shared with ftl_buffer, because this one could be overwritten
   during the copying operation in order to e.g. commit a CXT. */
static uint8_t ftl_copybuffer[FTL_COPYBUF_SIZE][0x800] STORAGE_ALIGN_ATTR;
static union ftl_spare_data_type ftl_copyspare[FTL_COPYBUF_SIZE] STORAGE_ALIGN_ATTR;

/* Needed to store the old scattered page offsets in order to be able to roll
   back if something fails while compacting a scattered page block. */
static uint16_t ftl_offsets_backup[0x200] STORAGE_ALIGN_ATTR;

#endif


static struct mutex ftl_mtx;

/* Pages per hyperblock (ftl_nand_type->pagesperblock * ftl_banks) */
static uint32_t ppb;

/* Reserved hyperblocks (ftl_nand_type->blocks
                       - ftl_nand_type->userblocks - 0x17) */
static uint32_t syshyperblocks;

#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
static uint32_t n3g_best_meta_score;
static uint32_t n3g_best_meta_bank;
static uint32_t n3g_best_meta_block;
static uint32_t n3g_best_meta_page;
static uint32_t n3g_first_structured_seen;
static uint32_t n3g_ctx_trace_compact;

static uint32_t ftl_has_devinfo(void);
static uint32_t ftl_vfl_open(void);
static uint32_t ftl_open(void);

#define N3G_RECON_BANK 0
#define N3G_RECON_BLOCK 8190
#define N3G_RECON_PAGE_COUNT 4
#define N3G_RECON_ONLY 1
#define N3G_DECODE_CLASSIFY_ONLY 1
#define N3G_ACTIVE_DECODE_MODE 4
#define N3G_BASE_PROBE_COMPACT 1
#define N3G_DEVINFO_SCAN_BLOCK_LIMIT 96
#define N3G_VFL_SCAN_BLOCK_LIMIT 64

static uint32_t ftl_n3g_meta_score(const uint32_t* data,
                                   const union ftl_spare_data_type* spare)
{
    uint32_t i, score = 0;

    for (i = 0; i < 16; i++)
        if (data[i] != 0xffffffff && data[i] != 0)
            score++;

    if (memcmp(data, "DEVICEINFOSIGN\0", 0x10) == 0)
        score += 64;
    if (memcmp((const uint8_t*)data + 0x18, "BBT", 3) == 0)
        score += 32;

    if (spare != NULL)
    {
        if (spare->meta.usn != 0 && spare->meta.usn != 0xffffffff)
            score += 4;
        if (spare->meta.idx != 0xffff)
            score += 2;
        switch (spare->meta.type)
        {
            case 0x40:
            case 0x41:
            case 0x43:
            case 0x44:
            case 0x46:
            case 0x47:
            case 0x80:
                score += 8;
                break;
        }
    }

    return score;
}

static uint32_t ftl_n3g_page_structure_score(const uint32_t* data)
{
    uint32_t i, nonff = 0, nonzero = 0, small = 0, blockish = 0, pageish = 0;
    uint32_t total_pages = (uint32_t)ftl_nand_type->blocks
                         * ftl_nand_type->pagesperblock;

    for (i = 0; i < 128; i++)
    {
        uint32_t v = data[i];
        if (v != 0xffffffff)
            nonff++;
        if (v != 0)
            nonzero++;
        if (v > 0 && v < 0x10000)
            small++;
        if (v > 0 && v < ftl_nand_type->blocks)
            blockish++;
        if (v > 0 && v < total_pages)
            pageish++;
    }

    return nonff + nonzero + small + blockish + pageish;
}

static uint32_t ftl_n3g_page_class(const uint32_t* data)
{
    uint32_t i, all_zero = 1, all_ff = 1;

    for (i = 0; i < 128; i++)
    {
        if (data[i] != 0)
            all_zero = 0;
        if (data[i] != 0xffffffff)
            all_ff = 0;
    }

    if (all_zero)
        return 0;
    if (all_ff)
        return 1;
    return 2;
}

static uint32_t ftl_n3g_decode_mapping_entry(uint32_t entry, uint32_t mode,
                                             uint32_t *bank, uint32_t *block,
                                             uint32_t *pageoff, uint32_t *page)
{
    uint32_t total_pages = (uint32_t)ftl_nand_type->blocks
                         * ftl_nand_type->pagesperblock;
    uint32_t pages_per_block = ftl_nand_type->pagesperblock;

    *bank = N3G_RECON_BANK;
    *block = 0xffffffff;
    *pageoff = 0;
    *page = 0xffffffff;

    if (mode == 0)
    {
        *block = entry >> 16;
        *pageoff = entry & 0xffff;
        if (*block < ftl_nand_type->blocks && *pageoff < pages_per_block)
            *page = *block * pages_per_block + *pageoff;
    }
    else if (mode == 1)
    {
        *bank = entry >> 30;
        *block = (entry >> 7) & 0x7fffff;
        *pageoff = entry & 0x7f;
        if (*bank < ftl_banks && *block < ftl_nand_type->blocks)
            *page = *block * pages_per_block + *pageoff;
    }
    else if (mode == 2)
    {
        *bank = entry & 3;
        *block = entry >> 9;
        *pageoff = (entry >> 2) & 0x7f;
        if (*bank < ftl_banks && *block < ftl_nand_type->blocks)
            *page = *block * pages_per_block + *pageoff;
    }
    else if (mode == 3)
    {
        if (entry < total_pages)
            *page = entry;
    }
    else if (mode == 4)
    {
        *bank = entry & 3;
        *page = entry >> 2;
    }
    else if (mode == 5)
    {
        if ((entry & 0x7ff) == 0)
            *page = entry >> 11;
    }
    else if (mode == 6)
    {
        *block = entry;
        if (*block < ftl_nand_type->blocks)
            *page = *block * pages_per_block;
    }
    else
    {
        *block = entry >> 8;
        *pageoff = entry & 0x7f;
        if (*block < ftl_nand_type->blocks)
            *page = *block * pages_per_block + *pageoff;
    }

    if (*bank >= ftl_banks || *page >= total_pages)
        return 1;

    return 0;
}

static uint32_t ftl_n3g_mbr_part_start(const uint8_t* bytes, uint32_t part)
{
    uint32_t off = 446 + part * 16 + 8;
    return bytes[off] | (bytes[off + 1] << 8) | (bytes[off + 2] << 16)
         | (bytes[off + 3] << 24);
}

static uint32_t ftl_n3g_mbr_part_size(const uint8_t* bytes, uint32_t part)
{
    uint32_t off = 446 + part * 16 + 12;
    return bytes[off] | (bytes[off + 1] << 8) | (bytes[off + 2] << 16)
         | (bytes[off + 3] << 24);
}

static int ftl_n3g_is_win_part_type(uint8_t type)
{
    return type == 0x0b || type == 0x0c || type == 0x0e
        || type == 0x0f || type == 0x07;
}

static int ftl_n3g_read_phys_page0(uint32_t phys, uint32_t total_pages,
                                   uint32_t total_global_pages,
                                   uint32_t *sig0, uint32_t *sig510,
                                   uint32_t first[4])
{
    uint32_t read_bank;
    uint32_t read_page;
    uint32_t rc;
    uint8_t* bytes = (uint8_t*)ftl_buffer;

    if (phys >= total_global_pages)
        return -1;

    read_bank = phys / total_pages;
    read_page = phys % total_pages;
    if (read_bank >= ftl_banks || read_page >= total_pages)
        return -2;

    rc = nand_read_page(read_bank, read_page, ftl_buffer,
                        &ftl_sparebuffer[0], 1, 0);
    if (rc != 0)
        return -3;

    *sig0 = bytes[0] | (bytes[1] << 8);
    *sig510 = bytes[510] | (bytes[511] << 8);
    first[0] = ((uint32_t*)bytes)[0];
    first[1] = ((uint32_t*)bytes)[1];
    first[2] = ((uint32_t*)bytes)[2];
    first[3] = ((uint32_t*)bytes)[3];
    return 0;
}

static void ftl_n3g_targeted_meta_decode(const uint32_t pages[],
                                         uint32_t recon_data[][128])
{
    uint32_t i;
    uint32_t ret;
    uint8_t* bytes = (uint8_t*)ftl_buffer;

    (void)pages;
    (void)recon_data;

    n3g_ctx_trace_compact = 1;

    FTL_PROGRESS("N3G_CTX_TRACE_START");
    FTL_PROGRESS("N3G_CTX_GEOM banks=%lu blocks=%u user=%u ppb=%lu sys=%lu pb=%u",
                 (unsigned long)ftl_banks, ftl_nand_type->blocks,
                 ftl_nand_type->userblocks, (unsigned long)ppb,
                 (unsigned long)syshyperblocks,
                 ftl_nand_type->pagesperblock);

    if (!ftl_has_devinfo())
    {
        FTL_PROGRESS("N3G_CTX_DEVINFO_FAIL");
        return;
    }

    FTL_PROGRESS("N3G_CTX_DEVINFO_OK");

    ret = ftl_vfl_open();
    FTL_PROGRESS("N3G_VFL_OPEN rc=%lu", (unsigned long)ret);
    if (ret != 0)
        return;

    for (i = 0; i < ftl_banks; i++)
    {
        FTL_PROGRESS("N3G_VFL_CXT bank=%lu usn=%08lx upd=%08lx active=%u next=%u",
                     (unsigned long)i, (unsigned long)ftl_vfl_cxt[i].usn,
                     (unsigned long)ftl_vfl_cxt[i].updatecount,
                     ftl_vfl_cxt[i].activecxtblock,
                     ftl_vfl_cxt[i].nextcxtpage);
        FTL_PROGRESS("N3G_VFL_CTRL bank=%lu ctrl=%04x %04x %04x vfl=%04x %04x %04x %04x",
                     (unsigned long)i,
                     ftl_vfl_cxt[i].ftlctrlblocks[0],
                     ftl_vfl_cxt[i].ftlctrlblocks[1],
                     ftl_vfl_cxt[i].ftlctrlblocks[2],
                     ftl_vfl_cxt[i].vflcxtblocks[0],
                     ftl_vfl_cxt[i].vflcxtblocks[1],
                     ftl_vfl_cxt[i].vflcxtblocks[2],
                     ftl_vfl_cxt[i].vflcxtblocks[3]);
    }

    ret = ftl_open();
    FTL_PROGRESS("N3G_FTL_OPEN rc=%lu", (unsigned long)ret);
    if (ret != 0)
        return;

    FTL_PROGRESS("N3G_FTL_CXT usn=%08lx next=%08lx ctrlpage=%08lx clean=%08lx",
                 (unsigned long)ftl_cxt.usn,
                 (unsigned long)ftl_cxt.nextblockusn,
                 (unsigned long)ftl_cxt.ftlctrlpage,
                 (unsigned long)ftl_cxt.clean_flag);
    FTL_PROGRESS("N3G_FTL_CTRL %04x %04x %04x map0=%08lx map1=%08lx",
                 ftl_cxt.ftlctrlblocks[0], ftl_cxt.ftlctrlblocks[1],
                 ftl_cxt.ftlctrlblocks[2],
                 (unsigned long)ftl_cxt.ftl_map_pages[0],
                 (unsigned long)ftl_cxt.ftl_map_pages[1]);

    FTL_PROGRESS("N3G_FTL_MAP0 %04x %04x %04x %04x %04x %04x %04x %04x",
                 ftl_map[0], ftl_map[1], ftl_map[2], ftl_map[3],
                 ftl_map[4], ftl_map[5], ftl_map[6], ftl_map[7]);

    ret = ftl_read(0, 1, ftl_buffer);
    FTL_PROGRESS("N3G_SECTOR0_FTL rc=%ld sig=%02x%02x first=%08lx %08lx %08lx %08lx",
                 (long)ret, bytes[510], bytes[511],
                 (unsigned long)((uint32_t*)bytes)[0],
                 (unsigned long)((uint32_t*)bytes)[1],
                 (unsigned long)((uint32_t*)bytes)[2],
                 (unsigned long)((uint32_t*)bytes)[3]);

    if (bytes[510] == 0x55 && bytes[511] == 0xaa)
    {
        FTL_PROGRESS("N3G_WINPOD_MBR_FOUND type0=%02x start0=%lu size0=%lu",
                     bytes[450],
                     (unsigned long)ftl_n3g_mbr_part_start(bytes, 0),
                     (unsigned long)ftl_n3g_mbr_part_size(bytes, 0));
        FTL_PROGRESS("N3G_PART1 type=%02x start=%lu size=%lu",
                     bytes[466],
                     (unsigned long)ftl_n3g_mbr_part_start(bytes, 1),
                     (unsigned long)ftl_n3g_mbr_part_size(bytes, 1));
    }
    else if (bytes[0] == 'E' && bytes[1] == 'R')
    {
        FTL_PROGRESS("N3G_MACPOD_DRIVER_DESC_FOUND");
    }
    else
    {
        FTL_PROGRESS("N3G_NO_BOOT_SIGNATURE_FOUND");
    }
}

static void ftl_n3g_dump_meta_candidate(uint32_t bank, uint32_t block,
                                        uint32_t page, uint32_t pagenum,
                                        const void* databuffer,
                                        const union ftl_spare_data_type* spare)
{
    const uint32_t* data = (const uint32_t*)databuffer;
    uint32_t score = ftl_n3g_meta_score(data, spare);
    uint32_t devsig = (memcmp(databuffer, "DEVICEINFOSIGN\0", 0x10) == 0);
    uint32_t bbtsig = (memcmp((const uint8_t*)databuffer + 0x18, "BBT", 3) == 0);

    FTL_PROGRESS("META_CAND b=%lu p=%lu w0=%08lx w1=%08lx w2=%08lx w3=%08lx sig=%lu aux=%lu",
                 (unsigned long)block, (unsigned long)pagenum,
                 (unsigned long)data[0], (unsigned long)data[1],
                 (unsigned long)data[2], (unsigned long)data[3],
                 (unsigned long)devsig, (unsigned long)bbtsig);
    FTL_PROGRESS("META_CAND4 b=%lu p=%lu w4=%08lx w5=%08lx w6=%08lx w7=%08lx",
                 (unsigned long)block, (unsigned long)pagenum,
                 (unsigned long)data[4], (unsigned long)data[5],
                 (unsigned long)data[6], (unsigned long)data[7]);
    FTL_PROGRESS("META_CAND8 b=%lu p=%lu w8=%08lx w9=%08lx w10=%08lx w11=%08lx",
                 (unsigned long)block, (unsigned long)pagenum,
                 (unsigned long)data[8], (unsigned long)data[9],
                 (unsigned long)data[10], (unsigned long)data[11]);
    FTL_PROGRESS("META_CAND12 b=%lu p=%lu w12=%08lx w13=%08lx w14=%08lx w15=%08lx",
                 (unsigned long)block, (unsigned long)pagenum,
                 (unsigned long)data[12], (unsigned long)data[13],
                 (unsigned long)data[14], (unsigned long)data[15]);

    if (spare != NULL)
    {
        const uint32_t* oob = (const uint32_t*)spare;
        FTL_PROGRESS("META_OOB bank=%lu b=%lu p=%lu o0=%08lx o1=%08lx o2=%08lx o3=%08lx",
                     (unsigned long)bank, (unsigned long)block,
                     (unsigned long)pagenum, (unsigned long)oob[0],
                     (unsigned long)oob[1], (unsigned long)oob[2],
                     (unsigned long)oob[3]);
        FTL_PROGRESS("META_SCORE bank=%lu b=%lu pp=%lu abs=%lu type=%02x usn=%08lx idx=%04x score=%lu",
                     (unsigned long)bank, (unsigned long)block,
                     (unsigned long)page, (unsigned long)pagenum,
                     spare->meta.type, (unsigned long)spare->meta.usn,
                     spare->meta.idx, (unsigned long)score);
    }
    else
    {
        FTL_PROGRESS("META_SCORE bank=%lu b=%lu pp=%lu abs=%lu score=%lu no_oob",
                     (unsigned long)bank, (unsigned long)block,
                     (unsigned long)page, (unsigned long)pagenum,
                     (unsigned long)score);
    }

    if (score != 0 && n3g_first_structured_seen == 0)
    {
        n3g_first_structured_seen = 1;
        FTL_PROGRESS("FIRST_STRUCTURED_METADATA_PAGE bank=%lu b=%lu p=%lu score=%lu",
                     (unsigned long)bank, (unsigned long)block,
                     (unsigned long)pagenum, (unsigned long)score);
    }

    if (score > n3g_best_meta_score)
    {
        n3g_best_meta_score = score;
        n3g_best_meta_bank = bank;
        n3g_best_meta_block = block;
        n3g_best_meta_page = pagenum;
        FTL_PROGRESS("NANO3G_SIGNATURE_GUESS bank=%lu b=%lu p=%lu score=%lu sig=%lu aux=%lu",
                     (unsigned long)n3g_best_meta_bank,
                     (unsigned long)n3g_best_meta_block,
                     (unsigned long)n3g_best_meta_page,
                     (unsigned long)n3g_best_meta_score,
                     (unsigned long)devsig, (unsigned long)bbtsig);
        FTL_PROGRESS("NEXT_FTL_PARSE_PATCH inspect best candidate bank=%lu b=%lu p=%lu",
                     (unsigned long)n3g_best_meta_bank,
                     (unsigned long)n3g_best_meta_block,
                     (unsigned long)n3g_best_meta_page);
    }
}

static void ftl_n3g_dump_reconstruction_pages(void)
{
    static const uint32_t pages[N3G_RECON_PAGE_COUNT] =
    {
        1048443, 1048444, 1048445, 1048446
    };
    static uint32_t recon_data[N3G_RECON_PAGE_COUNT][128];
    static union ftl_spare_data_type recon_spare[N3G_RECON_PAGE_COUNT];
    uint32_t pageidx, wordidx;
    uint32_t class_by_word[128];

    ftl_n3g_targeted_meta_decode(pages, recon_data);
    return;

    FTL_PROGRESS("FIRST_STRUCTURED_METADATA_PAGE bank=%lu b=%lu p=%lu",
                 (unsigned long)N3G_RECON_BANK,
                 (unsigned long)N3G_RECON_BLOCK,
                 (unsigned long)pages[0]);
    FTL_PROGRESS("FIELD_LAYOUT_MAP cls 1=ff 2=zero 3=const 4=inc1 5=stride 6=small 7=table");
    FTL_PROGRESS("HEADER_PATTERN compare p1048443-p1048445");
    FTL_PROGRESS("MAPPING_STRUCTURE_GUESS scan nonempty table-like runs");

    for (pageidx = 0; pageidx < N3G_RECON_PAGE_COUNT; pageidx++)
    {
        uint32_t pagenum = pages[pageidx];
        uint32_t page = pagenum - (N3G_RECON_BLOCK * ftl_nand_type->pagesperblock);
        uint32_t rc = nand_read_page(N3G_RECON_BANK, pagenum, ftl_buffer,
                                     &recon_spare[pageidx], 1, 0);
        const uint32_t* data = (const uint32_t*)ftl_buffer;
        const uint32_t* oob = (const uint32_t*)&recon_spare[pageidx];

        FTL_PROGRESS("META_FULL_PAGE bank=%lu b=%lu pp=%lu abs=%lu rc=%08lx",
                     (unsigned long)N3G_RECON_BANK,
                     (unsigned long)N3G_RECON_BLOCK,
                     (unsigned long)page, (unsigned long)pagenum,
                     (unsigned long)rc);
        FTL_PROGRESS("META_FULL_OOB abs=%lu o0=%08lx o1=%08lx o2=%08lx o3=%08lx",
                     (unsigned long)pagenum, (unsigned long)oob[0],
                     (unsigned long)oob[1], (unsigned long)oob[2],
                     (unsigned long)oob[3]);
        FTL_PROGRESS("META_FULL_OOB2 abs=%lu o4=%08lx o5=%08lx o6=%08lx o7=%08lx type=%02x usn=%08lx idx=%04x",
                     (unsigned long)pagenum, (unsigned long)oob[4],
                     (unsigned long)oob[5], (unsigned long)oob[6],
                     (unsigned long)oob[7], recon_spare[pageidx].meta.type,
                     (unsigned long)recon_spare[pageidx].meta.usn,
                     recon_spare[pageidx].meta.idx);

        for (wordidx = 0; wordidx < 128; wordidx += 4)
        {
            recon_data[pageidx][wordidx + 0] = data[wordidx + 0];
            recon_data[pageidx][wordidx + 1] = data[wordidx + 1];
            recon_data[pageidx][wordidx + 2] = data[wordidx + 2];
            recon_data[pageidx][wordidx + 3] = data[wordidx + 3];
#if !N3G_BASE_PROBE_COMPACT
            FTL_PROGRESS("META512 abs=%lu off=%03lx %08lx %08lx %08lx %08lx",
                         (unsigned long)pagenum,
                         (unsigned long)(wordidx * 4),
                         (unsigned long)data[wordidx + 0],
                         (unsigned long)data[wordidx + 1],
                         (unsigned long)data[wordidx + 2],
                         (unsigned long)data[wordidx + 3]);
#endif
        }
    }

    ftl_n3g_targeted_meta_decode(pages, recon_data);
    return;

    for (wordidx = 0; wordidx < 128; wordidx++)
    {
        uint32_t total_pages = (uint32_t)ftl_nand_type->blocks
                             * ftl_nand_type->pagesperblock;
        uint32_t a = recon_data[0][wordidx];
        uint32_t b = recon_data[1][wordidx];
        uint32_t c = recon_data[2][wordidx];
        uint32_t cls;

        if (a == 0xffffffff && b == 0xffffffff && c == 0xffffffff)
            cls = 1;
        else if (a == 0 && b == 0 && c == 0)
            cls = 2;
        else if (a == b && b == c)
            cls = 3;
        else if (b == a + 1 && c == b + 1)
            cls = 4;
        else if ((b - a) == (c - b) && (b - a) != 0)
            cls = 5;
        else if (((a ^ b) | (b ^ c)) < 0x10000)
            cls = 6;
        else
            cls = 7;

        class_by_word[wordidx] = cls;

        if (wordidx < 16)
        {
            FTL_PROGRESS("HEADER_WORD off=%03lx cls=%lu %08lx %08lx %08lx",
                         (unsigned long)(wordidx * 4), (unsigned long)cls,
                         (unsigned long)a, (unsigned long)b,
                         (unsigned long)c);
        }

        if (cls == 4 || cls == 5)
        {
            FTL_PROGRESS("SEQUENCE_FIELD off=%03lx cls=%lu stride=%08lx values=%08lx,%08lx,%08lx",
                         (unsigned long)(wordidx * 4), (unsigned long)cls,
                         (unsigned long)(b - a), (unsigned long)a,
                         (unsigned long)b, (unsigned long)c);
        }

        if (a < ftl_nand_type->blocks && b < ftl_nand_type->blocks
         && c < ftl_nand_type->blocks && (a | b | c) != 0)
        {
            FTL_PROGRESS("BLOCK_INDEX_CANDIDATE off=%03lx %lu %lu %lu",
                         (unsigned long)(wordidx * 4),
                         (unsigned long)a, (unsigned long)b,
                         (unsigned long)c);
        }
        else if (a < total_pages
              && b < total_pages
              && c < total_pages
              && (a | b | c) != 0)
        {
            FTL_PROGRESS("PAGE_INDEX_CANDIDATE off=%03lx %lu %lu %lu",
                         (unsigned long)(wordidx * 4),
                         (unsigned long)a, (unsigned long)b,
                         (unsigned long)c);
        }
    }

    {
        uint32_t start = 0;
        uint32_t best_start = 0xffffffff;
        uint32_t best_len = 0;
        while (start < 128)
        {
            uint32_t cls = class_by_word[start];
            uint32_t end = start + 1;
            while (end < 128 && class_by_word[end] == cls)
                end++;

            FTL_PROGRESS("FIELD_LAYOUT_MAP off=%03lx-%03lx cls=%lu",
                         (unsigned long)(start * 4),
                         (unsigned long)((end * 4) - 4),
                         (unsigned long)cls);

            if (start >= 16 && cls >= 3 && cls <= 7 && (end - start) > best_len)
            {
                best_start = start;
                best_len = end - start;
            }

            start = end;
        }

        FTL_PROGRESS("HEADER_CANDIDATE off=000 len=040 first=%08lx %08lx %08lx %08lx",
                     (unsigned long)recon_data[0][0],
                     (unsigned long)recon_data[0][1],
                     (unsigned long)recon_data[0][2],
                     (unsigned long)recon_data[0][3]);

        if (best_start != 0xffffffff)
        {
            FTL_PROGRESS("MAPPING_TABLE_OFFSET off=%03lx len=%lu cls=%lu",
                         (unsigned long)(best_start * 4),
                         (unsigned long)(best_len * 4),
                         (unsigned long)class_by_word[best_start]);
        }
        else
        {
            FTL_PROGRESS("MAPPING_TABLE_OFFSET not_found");
        }
    }

    FTL_PROGRESS("NEXT_PARSE_FUNCTION parse_n3g_meta_stage2 bank=%lu block=%lu pages=123-126",
                 (unsigned long)N3G_RECON_BANK,
                 (unsigned long)N3G_RECON_BLOCK);

    {
        uint32_t total_pages = (uint32_t)ftl_nand_type->blocks
                             * ftl_nand_type->pagesperblock;
        uint32_t pages_per_block = ftl_nand_type->pagesperblock;
        uint32_t ptr_page[128];
        uint32_t ptr_bank[128];
        uint32_t ptr_src_mask[128];
        uint32_t ptr_src_off[128];
        uint32_t ptr_value[128];
        uint32_t ptr_kind[128];
        uint32_t ptr_align[128];
        uint32_t ptr_count = 0;
        uint32_t best_score = 0;
        uint32_t best_idx = 0xffffffff;

        for (pageidx = 0; pageidx < N3G_RECON_PAGE_COUNT; pageidx++)
        {
            for (wordidx = 0; wordidx < 128; wordidx++)
            {
                uint32_t v = recon_data[pageidx][wordidx];
                uint32_t seen_mask = 0;
                uint32_t repeat, k;
                uint32_t bank, target;

                if (v == 0 || v == 0xffffffff)
                    continue;

                for (repeat = 0; repeat < N3G_RECON_PAGE_COUNT; repeat++)
                {
                    for (k = 0; k < 128; k++)
                    {
                        if (recon_data[repeat][k] == v)
                        {
                            seen_mask |= 1u << repeat;
                            break;
                        }
                    }
                }

                if (seen_mask == (1u << pageidx)
                 && (v & (pages_per_block - 1)) != 0
                 && (v >= ftl_nand_type->blocks))
                    continue;

                if (v < total_pages)
                {
                    if (ptr_count >= 128)
                        break;
                    ptr_bank[ptr_count] = N3G_RECON_BANK;
                    ptr_page[ptr_count] = v;
                    ptr_kind[ptr_count] = 0;
                    ptr_src_mask[ptr_count] = seen_mask;
                    ptr_src_off[ptr_count] = wordidx * 4;
                    ptr_align[ptr_count] = (v & (pages_per_block - 1)) == 0;
                    ptr_value[ptr_count++] = v;
                }

                bank = v & 3;
                target = v >> 2;
                if (bank < ftl_banks && target < total_pages && target > 0
                 && ptr_count < 128)
                {
                    ptr_bank[ptr_count] = bank;
                    ptr_page[ptr_count] = target;
                    ptr_kind[ptr_count] = 1;
                    ptr_src_mask[ptr_count] = seen_mask;
                    ptr_src_off[ptr_count] = wordidx * 4;
                    ptr_align[ptr_count] = (target & (pages_per_block - 1)) == 0;
                    ptr_value[ptr_count++] = v;
                }

                if (v < ftl_nand_type->blocks && ptr_count < 128)
                {
                    ptr_bank[ptr_count] = N3G_RECON_BANK;
                    ptr_page[ptr_count] = v * pages_per_block;
                    ptr_kind[ptr_count] = 2;
                    ptr_src_mask[ptr_count] = seen_mask;
                    ptr_src_off[ptr_count] = wordidx * 4;
                    ptr_align[ptr_count] = 1;
                    ptr_value[ptr_count++] = v;
                }

                bank = v & 3;
                target = v >> 2;
                if (bank < ftl_banks && target < ftl_nand_type->blocks
                 && target > 0 && ptr_count < 128)
                {
                    ptr_bank[ptr_count] = bank;
                    ptr_page[ptr_count] = target * pages_per_block;
                    ptr_kind[ptr_count] = 3;
                    ptr_src_mask[ptr_count] = seen_mask;
                    ptr_src_off[ptr_count] = wordidx * 4;
                    ptr_align[ptr_count] = 1;
                    ptr_value[ptr_count++] = v;
                }
            }
        }

#if N3G_DECODE_CLASSIFY_ONLY
        (void)ptr_src_off;
        (void)ptr_value;
        (void)ptr_kind;
#endif
        FTL_PROGRESS("METADATA_POINTER_FIELDS count=%lu", (unsigned long)ptr_count);
#if !N3G_DECODE_CLASSIFY_ONLY
        for (wordidx = 0; wordidx < ptr_count && wordidx < 64; wordidx++)
        {
            FTL_PROGRESS("POINTER_FIELD_VALUES idx=%lu val=%08lx kind=%lu mask=%lx off=%03lx align=%lu bank=%lu page=%lu",
                         (unsigned long)wordidx,
                         (unsigned long)ptr_value[wordidx],
                         (unsigned long)ptr_kind[wordidx],
                         (unsigned long)ptr_src_mask[wordidx],
                         (unsigned long)ptr_src_off[wordidx],
                         (unsigned long)ptr_align[wordidx],
                         (unsigned long)ptr_bank[wordidx],
                         (unsigned long)ptr_page[wordidx]);
        }
#endif

        for (wordidx = 0; wordidx < ptr_count; wordidx++)
        {
            uint32_t rc;
            uint32_t score = 0;
            uint32_t* data = (uint32_t*)ftl_buffer;

            if (ptr_src_mask[wordidx] == 0 || ptr_bank[wordidx] >= ftl_banks
             || ptr_page[wordidx] >= total_pages)
                continue;

            rc = nand_read_page(ptr_bank[wordidx], ptr_page[wordidx],
                                ftl_buffer, &ftl_sparebuffer[0], 1, 0);
            if (rc == 0)
                score = ftl_n3g_page_structure_score(data);
            if (ptr_align[wordidx])
                score += 32;
            if (ptr_src_mask[wordidx] == 0xf)
                score += 64;
            else if ((ptr_src_mask[wordidx] & (ptr_src_mask[wordidx] - 1)) != 0)
                score += 24;

#if !N3G_DECODE_CLASSIFY_ONLY
            FTL_PROGRESS("MAPPING_BLOCK_CANDIDATES idx=%lu mask=%lx off=%03lx val=%08lx kind=%lu align=%lu bank=%lu page=%lu rc=%08lx score=%lu",
                         (unsigned long)wordidx,
                         (unsigned long)ptr_src_mask[wordidx],
                         (unsigned long)ptr_src_off[wordidx],
                         (unsigned long)ptr_value[wordidx],
                         (unsigned long)ptr_kind[wordidx],
                         (unsigned long)ptr_align[wordidx],
                         (unsigned long)ptr_bank[wordidx],
                         (unsigned long)ptr_page[wordidx],
                         (unsigned long)rc, (unsigned long)score);

            if (rc == 0)
            {
                FTL_PROGRESS("CAND128 idx=%lu off=000 %08lx %08lx %08lx %08lx",
                             (unsigned long)wordidx,
                             (unsigned long)data[0],
                             (unsigned long)data[1],
                             (unsigned long)data[2],
                             (unsigned long)data[3]);
                FTL_PROGRESS("CAND128 idx=%lu off=010 %08lx %08lx %08lx %08lx",
                             (unsigned long)wordidx,
                             (unsigned long)data[4],
                             (unsigned long)data[5],
                             (unsigned long)data[6],
                             (unsigned long)data[7]);
                FTL_PROGRESS("CAND128 idx=%lu off=020 %08lx %08lx %08lx %08lx",
                             (unsigned long)wordidx,
                             (unsigned long)data[8],
                             (unsigned long)data[9],
                             (unsigned long)data[10],
                             (unsigned long)data[11]);
                FTL_PROGRESS("CAND128 idx=%lu off=030 %08lx %08lx %08lx %08lx",
                             (unsigned long)wordidx,
                             (unsigned long)data[12],
                             (unsigned long)data[13],
                             (unsigned long)data[14],
                             (unsigned long)data[15]);
                FTL_PROGRESS("CAND128 idx=%lu off=040 %08lx %08lx %08lx %08lx",
                             (unsigned long)wordidx,
                             (unsigned long)data[16],
                             (unsigned long)data[17],
                             (unsigned long)data[18],
                             (unsigned long)data[19]);
                FTL_PROGRESS("CAND128 idx=%lu off=050 %08lx %08lx %08lx %08lx",
                             (unsigned long)wordidx,
                             (unsigned long)data[20],
                             (unsigned long)data[21],
                             (unsigned long)data[22],
                             (unsigned long)data[23]);
                FTL_PROGRESS("CAND128 idx=%lu off=060 %08lx %08lx %08lx %08lx",
                             (unsigned long)wordidx,
                             (unsigned long)data[24],
                             (unsigned long)data[25],
                             (unsigned long)data[26],
                             (unsigned long)data[27]);
                FTL_PROGRESS("CAND128 idx=%lu off=070 %08lx %08lx %08lx %08lx",
                             (unsigned long)wordidx,
                             (unsigned long)data[28],
                             (unsigned long)data[29],
                             (unsigned long)data[30],
                             (unsigned long)data[31]);
            }
#endif

            if (score > best_score)
            {
                best_score = score;
                best_idx = wordidx;
            }
        }

        if (best_idx != 0xffffffff)
        {
            static uint32_t map_words[128];
            uint32_t rc, i;
            uint32_t map_valid = 0;
            uint32_t map_zero = 0;
            uint32_t map_erased = 0;
            uint32_t map_err = 0;
            uint32_t first_valid_entry = 0xffffffff;
            uint32_t first_valid_bank = 0xffffffff;
            uint32_t first_valid_page = 0xffffffff;

            FTL_PROGRESS("FIRST_MAPPING_PAGE idx=%lu bank=%lu page=%lu score=%lu",
                         (unsigned long)best_idx,
                         (unsigned long)ptr_bank[best_idx],
                         (unsigned long)ptr_page[best_idx],
                         (unsigned long)best_score);
            FTL_PROGRESS("FIRST_NON_METADATA_STRUCTURED_PAGE idx=%lu bank=%lu page=%lu score=%lu",
                         (unsigned long)best_idx,
                         (unsigned long)ptr_bank[best_idx],
                         (unsigned long)ptr_page[best_idx],
                         (unsigned long)best_score);
            FTL_PROGRESS("CONFIRMED_MAPPING_REGION pending validate entries");

            rc = nand_read_page(ptr_bank[best_idx], ptr_page[best_idx],
                                ftl_buffer, &ftl_sparebuffer[0], 1, 0);
            for (i = 0; i < 128; i++)
                map_words[i] = ((uint32_t*)ftl_buffer)[i];

            FTL_PROGRESS("RAW_MAPPING_ENTRIES page=%lu bank=%lu rc=%08lx",
                         (unsigned long)ptr_page[best_idx],
                         (unsigned long)ptr_bank[best_idx],
                         (unsigned long)rc);
            for (i = 0; i < 32; i += 4)
            {
                FTL_PROGRESS("RAW_MAPPING_ENTRIES idx=%lu %08lx %08lx %08lx %08lx",
                             (unsigned long)i,
                             (unsigned long)map_words[i + 0],
                             (unsigned long)map_words[i + 1],
                             (unsigned long)map_words[i + 2],
                             (unsigned long)map_words[i + 3]);
            }

            FTL_PROGRESS("BEST_DECODE_MODE_USED mode=%lu",
                         (unsigned long)N3G_ACTIVE_DECODE_MODE);

            for (i = 0; i < 128; i++)
            {
                uint32_t entry = map_words[i];
                uint32_t bank, block, pageoff, page;
                uint32_t cls;
                uint32_t* data = (uint32_t*)ftl_buffer;

                if (entry == 0 || entry == 0xffffffff
                 || ftl_n3g_decode_mapping_entry(entry, N3G_ACTIVE_DECODE_MODE,
                                                 &bank, &block, &pageoff,
                                                 &page) != 0)
                {
                    map_err++;
                    continue;
                }

                rc = nand_read_page(bank, page, ftl_buffer,
                                    &ftl_sparebuffer[0], 1, 0);
                if (rc != 0)
                {
                    map_err++;
                    continue;
                }

                cls = ftl_n3g_page_class(data);
                if (cls == 0)
                    map_zero++;
                else if (cls == 1)
                    map_erased++;
                else
                {
                    map_valid++;
                    if (first_valid_entry == 0xffffffff)
                    {
                        first_valid_entry = i;
                        first_valid_bank = bank;
                        first_valid_page = page;
                    }
                }
            }

            FTL_PROGRESS("LOGICAL_TO_PHYSICAL_MAP entries=128 valid=%lu zero=%lu erased=%lu err=%lu",
                         (unsigned long)map_valid, (unsigned long)map_zero,
                         (unsigned long)map_erased, (unsigned long)map_err);

            if (first_valid_entry != 0xffffffff)
            {
                FTL_PROGRESS("FIRST_VALID_PHYSICAL_PAGE entry=%lu bank=%lu page=%lu",
                             (unsigned long)first_valid_entry,
                             (unsigned long)first_valid_bank,
                             (unsigned long)first_valid_page);
                FTL_PROGRESS("CONFIRMED_MAPPING_FORMAT mode=%lu",
                             (unsigned long)N3G_ACTIVE_DECODE_MODE);
            }
            else
            {
                FTL_PROGRESS("FIRST_VALID_PHYSICAL_PAGE not_found");
                FTL_PROGRESS("NO_VALID_FORMAT");
            }

            for (i = 0; i < 16; i++)
            {
                uint32_t entry = map_words[i];
                uint32_t bank, block, pageoff, page;

                if (map_words[i] != 0 && map_words[i] != 0xffffffff
                 && ftl_n3g_decode_mapping_entry(entry, N3G_ACTIVE_DECODE_MODE,
                                                 &bank, &block, &pageoff,
                                                 &page) == 0)
                    FTL_PROGRESS("MAP_ENTRY_DECODE lsec=%lu raw=%08lx bank=%lu block=%lu po=%lu page=%lu",
                                 (unsigned long)i, (unsigned long)entry,
                                 (unsigned long)bank, (unsigned long)block,
                                 (unsigned long)pageoff,
                                 (unsigned long)page);
            }

            {
                static uint32_t second_map_words[128];
                uint32_t found_decode = 0xffffffff;
                uint32_t found_bank = 0xffffffff;
                uint32_t found_block = 0xffffffff;
                uint32_t found_pageoff = 0xffffffff;
                uint32_t found_page = 0xffffffff;
                uint32_t found_rc = 0xffffffff;
                uint32_t* data = (uint32_t*)ftl_buffer;
                uint8_t* bytes = (uint8_t*)ftl_buffer;
                uint32_t entry = map_words[0];
                uint32_t mapping_bank = entry & 3;
                uint32_t mapping_index = entry >> 2;
                uint32_t entries_per_page = 128;
                uint32_t inner_index = mapping_index % entries_per_page;
                uint32_t base_candidates[12];
                uint32_t base_count = 0;

                FTL_PROGRESS("BEST_DECODE_MODE_USED mode=%lu",
                             (unsigned long)N3G_ACTIVE_DECODE_MODE);
                FTL_PROGRESS("SECOND_LEVEL_LOOKUP lsec=0 raw=%08lx bank=%lu mapping_index=%lu inner=%lu",
                             (unsigned long)entry,
                             (unsigned long)mapping_bank,
                             (unsigned long)mapping_index,
                             (unsigned long)inner_index);

                base_candidates[base_count++] = pages[0];
                base_candidates[base_count++] = pages[1];
                base_candidates[base_count++] = pages[2];
                base_candidates[base_count++] = pages[3];
                base_candidates[base_count++] =
                    (N3G_RECON_BLOCK + 1) * ftl_nand_type->pagesperblock
                    + ftl_nand_type->pagesperblock - 4;
                base_candidates[base_count++] =
                    (N3G_RECON_BLOCK + 1) * ftl_nand_type->pagesperblock
                    + ftl_nand_type->pagesperblock - 3;
                base_candidates[base_count++] =
                    (N3G_RECON_BLOCK + 1) * ftl_nand_type->pagesperblock
                    + ftl_nand_type->pagesperblock - 2;
                base_candidates[base_count++] =
                    (N3G_RECON_BLOCK + 1) * ftl_nand_type->pagesperblock
                    + ftl_nand_type->pagesperblock - 1;
                base_candidates[base_count++] = ptr_page[best_idx];
                base_candidates[base_count++] = ptr_page[best_idx] + 1;
                base_candidates[base_count++] =
                    ptr_page[best_idx] & ~(ftl_nand_type->pagesperblock - 1);
                base_candidates[base_count++] =
                    (ptr_page[best_idx] & ~(ftl_nand_type->pagesperblock - 1))
                    + ftl_nand_type->pagesperblock - 4;

                for (uint32_t base_i = 0;
                     found_decode == 0xffffffff && base_i < base_count;
                     base_i++)
                {
                    uint32_t base = base_candidates[base_i];
                    uint32_t second_page = base
                                         + (mapping_index / entries_per_page);
                    uint32_t inner_entry = 0;

                    if (base_i != 0)
                    {
                        uint32_t prev = 0;
                        while (prev < base_i && base_candidates[prev] != base)
                            prev++;
                        if (prev != base_i)
                            continue;
                    }

                    rc = 0xffffffff;
                    if (entry != 0 && entry != 0xffffffff
                     && mapping_bank < ftl_banks)
                        rc = nand_read_page(mapping_bank, second_page,
                                            second_map_words,
                                            &ftl_sparebuffer[0], 1, 0);

                    if (rc == 0)
                        inner_entry = second_map_words[inner_index];

                    FTL_PROGRESS("SECOND_LEVEL_BASE_TRY base=%lu mapping_index=%lu second_page=%lu inner=%08lx rc=%08lx",
                                 (unsigned long)base,
                                 (unsigned long)mapping_index,
                                 (unsigned long)second_page,
                                 (unsigned long)inner_entry,
                                 (unsigned long)rc);

                    for (uint32_t decode = 0; rc == 0 && decode < 8; decode++)
                    {
                        uint32_t bank, block, pageoff, decoded_page;
                        uint32_t phys_page = 0xffffffff;
                        uint32_t read_rc;

                        if (inner_entry == 0 || inner_entry == 0xffffffff
                         || ftl_n3g_decode_mapping_entry(inner_entry, decode,
                                                         &bank, &block, &pageoff,
                                                         &decoded_page) != 0)
                            continue;

                        if (block < ftl_nand_type->blocks
                         && pageoff < ftl_nand_type->pagesperblock)
                            phys_page = block * ftl_nand_type->pagesperblock
                                      + pageoff;
                        else if (decoded_page < (uint32_t)ftl_nand_type->blocks
                                             * ftl_nand_type->pagesperblock)
                        {
                            phys_page = decoded_page;
                            block = decoded_page / ftl_nand_type->pagesperblock;
                            pageoff = decoded_page & (ftl_nand_type->pagesperblock - 1);
                        }

                        if (phys_page >= (uint32_t)ftl_nand_type->blocks
                                      * ftl_nand_type->pagesperblock)
                            continue;

                        read_rc = nand_read_page(bank, phys_page, ftl_buffer,
                                                 &ftl_sparebuffer[0], 1, 0);
                        FTL_PROGRESS("INNER_DECODE_ATTEMPT mode=%lu block=%lu page=%lu phys=%lu sig=%02x%02x rc=%08lx",
                                     (unsigned long)decode,
                                     (unsigned long)block,
                                     (unsigned long)pageoff,
                                     (unsigned long)phys_page,
                                     bytes[510], bytes[511],
                                     (unsigned long)read_rc);

                        if (read_rc == 0 && bytes[510] == 0x55
                         && bytes[511] == 0xaa)
                        {
                            found_decode = decode;
                            found_bank = bank;
                            found_block = block;
                            found_pageoff = pageoff;
                            found_page = phys_page;
                            found_rc = read_rc;
                            FTL_PROGRESS("FOUND_VALID_MBR base=%lu mode=%lu logical_sector=0 mapping_index=%lu block=%lu page=%lu phys=%lu",
                                         (unsigned long)base,
                                         (unsigned long)found_decode,
                                         (unsigned long)mapping_index,
                                         (unsigned long)found_block,
                                         (unsigned long)found_pageoff,
                                         (unsigned long)found_page);
                            break;
                        }
                    }
                }

                if (found_decode != 0xffffffff)
                {
                    uint32_t nonempty = 0;
                    for (wordidx = 0; wordidx < 128; wordidx++)
                        if (data[wordidx] != 0 && data[wordidx] != 0xffffffff)
                            nonempty++;

                    FTL_PROGRESS("SECTOR0_DATA mode=%lu bank=%lu block=%lu page=%lu phys_page=%lu rc=%08lx nonempty=%lu sig=%02x%02x",
                                 (unsigned long)found_decode,
                                 (unsigned long)found_bank,
                                 (unsigned long)found_block,
                                 (unsigned long)found_pageoff,
                                 (unsigned long)found_page,
                                 (unsigned long)found_rc,
                                 (unsigned long)nonempty,
                                 bytes[510], bytes[511]);
                    FTL_PROGRESS("SECTOR0_DATA off=000 %08lx %08lx %08lx %08lx",
                                 (unsigned long)data[0],
                                 (unsigned long)data[1],
                                 (unsigned long)data[2],
                                 (unsigned long)data[3]);
                    FTL_PROGRESS("SECTOR0_DATA off=010 %08lx %08lx %08lx %08lx",
                                 (unsigned long)data[4],
                                 (unsigned long)data[5],
                                 (unsigned long)data[6],
                                 (unsigned long)data[7]);
                    FTL_PROGRESS("SECTOR0_DATA off=020 %08lx %08lx %08lx %08lx",
                                 (unsigned long)data[8],
                                 (unsigned long)data[9],
                                 (unsigned long)data[10],
                                 (unsigned long)data[11]);
                    FTL_PROGRESS("SECTOR0_DATA off=030 %08lx %08lx %08lx %08lx",
                                 (unsigned long)data[12],
                                 (unsigned long)data[13],
                                 (unsigned long)data[14],
                                 (unsigned long)data[15]);
                    FTL_PROGRESS("MBR_FOUND");
                    for (i = 0; i < 4; i++)
                    {
                        uint32_t off = 446 + i * 16;
                        FTL_PROGRESS("PARTITION_TABLE idx=%lu %02x %02x%02x%02x type=%02x %02x%02x%02x lba=%08lx sectors=%08lx",
                                     (unsigned long)i, bytes[off],
                                     bytes[off + 1], bytes[off + 2],
                                     bytes[off + 3], bytes[off + 4],
                                     bytes[off + 5], bytes[off + 6],
                                     bytes[off + 7],
                                     (unsigned long)(bytes[off + 8]
                                      | (bytes[off + 9] << 8)
                                      | (bytes[off + 10] << 16)
                                      | (bytes[off + 11] << 24)),
                                     (unsigned long)(bytes[off + 12]
                                      | (bytes[off + 13] << 8)
                                      | (bytes[off + 14] << 16)
                                      | (bytes[off + 15] << 24)));
                    }
                }
                else
                {
                    FTL_PROGRESS("MBR_NOT_FOUND second_level_base_probe");
                }
            }
        }
        else
        {
            FTL_PROGRESS("FIRST_MAPPING_PAGE not_found");
            FTL_PROGRESS("FIRST_NON_METADATA_STRUCTURED_PAGE not_found");
            FTL_PROGRESS("CONFIRMED_MAPPING_REGION no");
        }
    }
}
#else
static inline void ftl_n3g_dump_meta_candidate(uint32_t bank, uint32_t block,
                                               uint32_t page, uint32_t pagenum,
                                               const void* databuffer,
                                               const union ftl_spare_data_type* spare)
{
    (void)bank;
    (void)block;
    (void)page;
    (void)pagenum;
    (void)databuffer;
    (void)spare;
}

static inline void ftl_n3g_dump_reconstruction_pages(void)
{
}
#endif



/* Finds a device info page for the specified bank and returns its number.
   Used to check if one is present, and to read the lowlevel BBT. */
static uint32_t ftl_find_devinfo(uint32_t bank)
{
    /* Scan the last 10% of the flash for device info pages */
    uint32_t lowestBlock = ftl_nand_type->blocks
                         - (ftl_nand_type->blocks / 10);
    uint32_t block, page, pagenum;
    FTL_PROGRESS("N3G_DEVINFO_SCAN bank=%lu from=%lu",
                 (unsigned long)bank, (unsigned long)lowestBlock);
    for (block = ftl_nand_type->blocks - 1; block >= lowestBlock; block--)
    {
        uint32_t scanned = ftl_nand_type->blocks - 1 - block;

        if (n3g_ctx_trace_compact && scanned >= N3G_DEVINFO_SCAN_BLOCK_LIMIT)
        {
            FTL_PROGRESS("N3G_DEVINFO_TIMEOUT bank=%lu scanned=%lu",
                         (unsigned long)bank, (unsigned long)scanned);
            break;
        }
        if (n3g_ctx_trace_compact && (scanned & 0x1f) == 0)
            FTL_PROGRESS("N3G_DEVINFO_PROGRESS bank=%lu scanned=%lu block=%lu",
                         (unsigned long)bank, (unsigned long)scanned,
                         (unsigned long)block);
        if (!n3g_ctx_trace_compact
         && ((ftl_nand_type->blocks - 1 - block) & 0xf) == 0)
            FTL_PROGRESS("FTL_SCAN_PROGRESS b%lu p%lu",
                         (unsigned long)block,
                         (unsigned long)(block * ftl_nand_type->pagesperblock));
        page = ftl_nand_type->pagesperblock - 8;
        for (; page < ftl_nand_type->pagesperblock; page++)
        {
            pagenum = block * ftl_nand_type->pagesperblock + page;
            if ((nand_read_page(bank, pagenum, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0) & 0x11F) != 0)
                continue;
            if (!n3g_ctx_trace_compact)
            {
                FTL_PROGRESS("METADATA_SIGNATURE_RESULTS b%lu p%lu %08lx %08lx",
                             (unsigned long)block, (unsigned long)page,
                             (unsigned long)((uint32_t *)ftl_buffer)[0],
                             (unsigned long)((uint32_t *)ftl_buffer)[1]);
                ftl_n3g_dump_meta_candidate(bank, block, page, pagenum,
                                            ftl_buffer, &ftl_sparebuffer[0]);
            }
            if (memcmp(ftl_buffer, "DEVICEINFOSIGN\0", 0x10) == 0)
            {
                FTL_PROGRESS("FIRST_VALID_BLOCK devinfo b%lu p%lu",
                             (unsigned long)block, (unsigned long)pagenum);
                return pagenum;
            }
            if (!n3g_ctx_trace_compact)
                FTL_PROGRESS("METADATA_SIGNATURE_RESULTS mismatch b%lu p%lu",
                             (unsigned long)block, (unsigned long)pagenum);
        }
    }
    FTL_PROGRESS("INIT_FAIL no devinfo bank %lu", (unsigned long)bank);
    return 0;
}


/* Checks if all banks have proper device info pages */
static uint32_t ftl_has_devinfo(void)
{
    uint32_t i;
    for (i = 0; i < ftl_banks; i++) if (ftl_find_devinfo(i) == 0) return 0;
    return 1;
}


/* Loads the lowlevel BBT for a bank to the specified buffer.
   This is based on some cryptic disassembly and not fully understood yet. */
static uint32_t ftl_load_bbt(uint32_t bank, uint8_t* bbt)
{
    uint32_t i, j;
    uint32_t pagebase, page = ftl_find_devinfo(bank), page2;
    uint32_t unk1, unk2, unk3;
    if (page == 0)
    {
        FTL_PROGRESS("ftl bbt no devinfo b%lu", (unsigned long)bank);
        return 1;
    }
    pagebase = page & ~(ftl_nand_type->pagesperblock - 1);
    FTL_PROGRESS("ftl bbt bank %lu base %lu",
                 (unsigned long)bank, (unsigned long)pagebase);
    if ((nand_read_page(bank, page, ftl_buffer,
                        NULL, 1, 0) & 0x11F) != 0)
    {
        FTL_PROGRESS("ftl bbt page read fail b%lu p%lu",
                     (unsigned long)bank, (unsigned long)page);
        return 1;
    }
    FTL_PROGRESS("ftl bbt sig %02x%02x%02x%02x",
                 ftl_buffer[0x18], ftl_buffer[0x19],
                 ftl_buffer[0x1a], ftl_buffer[0x1b]);
    if (memcmp(&ftl_buffer[0x18], "BBT", 4) != 0)
    {
        FTL_PROGRESS("ftl bbt sig mismatch b%lu", (unsigned long)bank);
        return 1;
    }
    unk1 = ((uint16_t*)ftl_buffer)[0x10];
    unk2 = ((uint16_t*)ftl_buffer)[0x11];
    unk3 = ((uint16_t*)ftl_buffer)[((uint32_t*)ftl_buffer)[4] * 6 + 10]
         + ((uint16_t*)ftl_buffer)[((uint32_t*)ftl_buffer)[4] * 6 + 11];
    FTL_PROGRESS("ftl bbt refs %lu %lu %lu",
                 (unsigned long)unk1, (unsigned long)unk2,
                 (unsigned long)unk3);
    for (i = 0; i < unk1; i++)
    {
        for (j = 0; ; j++)
        {
            page2 = unk2 + i + unk3 * j;
            if (page2 >= (uint32_t)(ftl_nand_type->pagesperblock - 8))
                break;
            if (((i + j) & 0xf) == 0)
                FTL_PROGRESS("ftl bbt scan b%lu p%lu",
                             (unsigned long)bank,
                             (unsigned long)(pagebase + page2));
            if ((nand_read_page(bank, pagebase + page2, ftl_buffer,
                                NULL, 1, 0) & 0x11F) == 0)
            {
                memcpy(bbt, ftl_buffer, 0x410);
                FTL_PROGRESS("ftl bbt found b%lu p%lu",
                             (unsigned long)bank,
                             (unsigned long)(pagebase + page2));
                return 0;
            }
        }
    }
    FTL_PROGRESS("ftl bbt not found b%lu", (unsigned long)bank);
    return 1;
}


/* Calculates the checksums for the VFL context page of the specified bank */
static void ftl_vfl_calculate_checksum(uint32_t bank,
                                       uint32_t* checksum1, uint32_t* checksum2)
{
    uint32_t i;
    *checksum1 = 0xAABBCCDD;
    *checksum2 = 0xAABBCCDD;
    for (i = 0; i < 0x1FE; i++)
    {
        *checksum1 += ((uint32_t*)(&ftl_vfl_cxt[bank]))[i];
        *checksum2 ^= ((uint32_t*)(&ftl_vfl_cxt[bank]))[i];
    }
}


/* Checks if the checksums of the VFL context
   of the specified bank are correct */
static uint32_t ftl_vfl_verify_checksum(uint32_t bank)
{
    uint32_t checksum1, checksum2;
    ftl_vfl_calculate_checksum(bank, &checksum1, &checksum2);
    if (checksum1 == ftl_vfl_cxt[bank].checksum1) return 0;
    /* The following line is pretty obviously a bug in Whimory,
       but we do it the same way for compatibility. */
    if (checksum2 != ftl_vfl_cxt[bank].checksum2) return 0;
    DEBUGF("FTL: Bad VFL CXT checksum on bank %d!\n", bank);
    return 1;
}

#ifndef FTL_READONLY

#if __GNUC__ >= 9
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

/* Updates the checksums of the VFL context of the specified bank */
static void ftl_vfl_update_checksum(uint32_t bank)
{
    ftl_vfl_calculate_checksum(bank, &ftl_vfl_cxt[bank].checksum1,
                               &ftl_vfl_cxt[bank].checksum2);
}

#if __GNUC__ >= 9
#pragma GCC diagnostic pop
#endif

/* Writes 8 copies of the VFL context of the specified bank to flash,
   and succeeds if at least 4 can be read back properly. */
static uint32_t ftl_vfl_store_cxt(uint32_t bank)
{
    uint32_t i;
    ftl_vfl_cxt[bank].updatecount--;
    ftl_vfl_cxt[bank].usn = ++ftl_vfl_usn;
    ftl_vfl_cxt[bank].nextcxtpage += 8;
    ftl_vfl_update_checksum(bank);
    memset(&ftl_sparebuffer[0], 0xFF, 0x40);
    ftl_sparebuffer[0].meta.usn = ftl_vfl_cxt[bank].updatecount;
    ftl_sparebuffer[0].meta.field_8 = 0;
    ftl_sparebuffer[0].meta.type = 0x80;
    for (i = 1; i <= 8; i++)
    {
        uint32_t index = ftl_vfl_cxt[bank].activecxtblock;
        uint32_t block = ftl_vfl_cxt[bank].vflcxtblocks[index];
        uint32_t page = block * ftl_nand_type->pagesperblock;
        page += ftl_vfl_cxt[bank].nextcxtpage - i;
        nand_write_page(bank, page, &ftl_vfl_cxt[bank], &ftl_sparebuffer[0], 1);
    }
    uint32_t good = 0;
    for (i = 1; i <= 8; i++)
    {
        uint32_t index = ftl_vfl_cxt[bank].activecxtblock;
        uint32_t block = ftl_vfl_cxt[bank].vflcxtblocks[index];
        uint32_t page = block * ftl_nand_type->pagesperblock;
        page += ftl_vfl_cxt[bank].nextcxtpage - i;
        if ((nand_read_page(bank, page, ftl_buffer,
                            &ftl_sparebuffer[0], 1, 0) & 0x11F) != 0)
            continue;
        if (memcmp(ftl_buffer, &ftl_vfl_cxt[bank], 0x7AC) != 0)
            continue;
        if (ftl_sparebuffer[0].meta.usn != ftl_vfl_cxt[bank].updatecount)
            continue;
        if (ftl_sparebuffer[0].meta.field_8 == 0
         && ftl_sparebuffer[0].meta.type == 0x80) good++;
    }
    return good > 3 ? 0 : 1;
}
#endif


#ifndef FTL_READONLY
/* Commits the VFL context of the specified bank to flash,
   retries until it works or all available pages have been tried */
static uint32_t ftl_vfl_commit_cxt(uint32_t bank)
{
    DEBUGF("FTL: VFL: Committing context on bank %d\n", bank);
    if (ftl_vfl_cxt[bank].nextcxtpage + 8 <= ftl_nand_type->pagesperblock)
        if (ftl_vfl_store_cxt(bank) == 0) return 0;
    uint32_t current = ftl_vfl_cxt[bank].activecxtblock;
    uint32_t i = current, j;
    while (1)
    {
        i = (i + 1) & 3;
        if (i == current) break;
        if (ftl_vfl_cxt[bank].vflcxtblocks[i] == 0xFFFF) continue;
        for (j = 0; j < 4; j++)
            if (nand_block_erase(bank, ftl_vfl_cxt[bank].vflcxtblocks[i]
                                     * ftl_nand_type->pagesperblock) == 0)
                break;
        if (j == 4) continue;
        ftl_vfl_cxt[bank].activecxtblock = i;
        ftl_vfl_cxt[bank].nextcxtpage = 0;
        if (ftl_vfl_store_cxt(bank) == 0) return 0;
    }
    panicf("VFL: Failed to commit VFL CXT!");
    return 1;
}
#endif


/* Returns a pointer to the most recently updated VFL context,
   used to find out the current FTL context vBlock numbers
   (planetbeing's "maxthing") */
static struct ftl_vfl_cxt_type* ftl_vfl_get_newest_cxt(void)
{
    uint32_t i, maxusn;
    struct ftl_vfl_cxt_type* cxt = NULL;
    maxusn = 0;
    for (i = 0; i < ftl_banks; i++)
        if (ftl_vfl_cxt[i].usn >= maxusn)
        {
            cxt = &ftl_vfl_cxt[i];
            maxusn = ftl_vfl_cxt[i].usn;
        }
    return cxt;
}


/* Checks if the specified pBlock is marked bad in the supplied lowlevel BBT.
   Only used while mounting the VFL. */
static uint32_t ftl_is_good_block(uint8_t* bbt, uint32_t block)
{
    if ((bbt[block >> 3] & (1 << (block & 7))) == 0) return 0;
    else return 1;
}


/* Checks if the specified vBlock could be remapped */
static uint32_t ftl_vfl_is_good_block(uint32_t bank, uint32_t block)
{
    uint8_t bbtentry = ftl_vfl_cxt[bank].bbt[block >> 6];
    if ((bbtentry & (1 << ((7 - (block >> 3)) & 7))) == 0) return 0;
    else return 1;
}


#ifndef FTL_READONLY
/* Sets or unsets the bad bit of the specified vBlock
   in the specified bank's VFL context */
static void ftl_vfl_set_good_block(uint32_t bank, uint32_t block, uint32_t isgood)
{
    uint8_t bit = (1 << ((7 - (block >> 3)) & 7));
    if (isgood == 1) ftl_vfl_cxt[bank].bbt[block >> 6] |= bit;
    else ftl_vfl_cxt[bank].bbt[block >> 6] &= ~bit;
}
#endif


/* Tries to read a VFL context from the specified bank, pBlock and page */
static uint32_t ftl_vfl_read_page(uint32_t bank, uint32_t block,
                                  uint32_t startpage, void* databuffer,
                                  union ftl_spare_data_type* sparebuffer)
{
    uint32_t i;
    FTL_PROGRESS("ftl vfl chk b%lu blk%lu sp%lu",
                 (unsigned long)bank, (unsigned long)block,
                 (unsigned long)startpage);
    for (i = 0; i < 8; i++)
    {
        uint32_t page = block * ftl_nand_type->pagesperblock
                      + startpage + i;
        if ((nand_read_page(bank, page, databuffer,
                            sparebuffer, 1, 1) & 0x11F) == 0)
        {
            ftl_n3g_dump_meta_candidate(bank, block, startpage + i,
                                        page, databuffer, sparebuffer);
            FTL_PROGRESS("ftl vfl meta b%lu p%lu t%02x f8%02x usn%08lx",
                         (unsigned long)bank, (unsigned long)page,
                         sparebuffer->meta.type, sparebuffer->meta.field_8,
                         (unsigned long)sparebuffer->meta.usn);
            if (sparebuffer->meta.field_8 == 0
             && sparebuffer->meta.type == 0x80)
            {
                FTL_PROGRESS("ftl vfl valid b%lu blk%lu p%lu",
                             (unsigned long)bank, (unsigned long)block,
                             (unsigned long)page);
                return 0;
            }
        }
    }
    return 1;
}


/* Translates a bank and vBlock to a pBlock, following remaps */
static uint32_t ftl_vfl_get_physical_block(uint32_t bank, uint32_t block)
{
    if (ftl_vfl_is_good_block(bank, block) == 1) return block;

    uint32_t spareindex;
    uint32_t spareused = ftl_vfl_cxt[bank].spareused;
    for (spareindex = 0; spareindex < spareused; spareindex++)
        if (ftl_vfl_cxt[bank].remaptable[spareindex] == block)
        {
            DEBUGF("FTL: VFL: Following remapped block: %d => %d\n",
                   block, ftl_vfl_cxt[bank].firstspare + spareindex);
            return ftl_vfl_cxt[bank].firstspare + spareindex;
        }
    return block;
}


#ifndef FTL_READONLY
/* Checks if remapping is scheduled for the specified bank and vBlock */
static uint32_t ftl_vfl_check_remap_scheduled(uint32_t bank, uint32_t block)
{
    uint32_t i;
    for (i = 0x333; i > 0 && i > ftl_vfl_cxt[bank].scheduledstart; i--)
        if (ftl_vfl_cxt[bank].remaptable[i] == block) return 1;
    return 0;
}
#endif


#ifndef FTL_READONLY
/* Schedules remapping for the specified bank and vBlock */
static void ftl_vfl_schedule_block_for_remap(uint32_t bank, uint32_t block)
{
    if (ftl_vfl_check_remap_scheduled(bank, block) == 1)
        return;
    panicf("FTL: Scheduling bank %u block %u for remap!", (unsigned)bank, (unsigned)block);
    if (ftl_vfl_cxt[bank].scheduledstart == ftl_vfl_cxt[bank].spareused)
        return;
    ftl_vfl_cxt[bank].remaptable[--ftl_vfl_cxt[bank].scheduledstart] = block;
    ftl_vfl_commit_cxt(bank);
}
#endif


#ifndef FTL_READONLY
/* Removes the specified bank and vBlock combination
   from the remap scheduled list */
static void ftl_vfl_mark_remap_done(uint32_t bank, uint32_t block)
{
    uint32_t i;
    uint32_t start = ftl_vfl_cxt[bank].scheduledstart;
    uint32_t lastscheduled = ftl_vfl_cxt[bank].remaptable[start];
    for (i = 0x333; i > 0 && i > start; i--)
        if (ftl_vfl_cxt[bank].remaptable[i] == block)
        {
            if (i != start && i != 0x333)
                ftl_vfl_cxt[bank].remaptable[i] = lastscheduled;
            ftl_vfl_cxt[bank].scheduledstart++;
            return;
        }
}
#endif


#ifndef FTL_READONLY
/* Logs that there is trouble for the specified vBlock on the specified bank.
   The vBlock will be scheduled for remap
   if there is too much trouble with it. */
static void ftl_vfl_log_trouble(uint32_t bank, uint32_t vblock)
{
    uint32_t i;
    for (i = 0; i < 5; i++)
        if (ftl_troublelog[i].block == vblock
         && ftl_troublelog[i].bank == bank)
        {
            ftl_troublelog[i].errors += 3;
            if (ftl_troublelog[i].errors > 5)
            {
                ftl_vfl_schedule_block_for_remap(bank, vblock);
                ftl_troublelog[i].block = 0xFFFF;
            }
            return;
        }
    for (i = 0; i < 5; i++)
        if (ftl_troublelog[i].block == 0xFFFF)
        {
            ftl_troublelog[i].block = vblock;
            ftl_troublelog[i].bank = bank;
            ftl_troublelog[i].errors = 3;
            return;
        }
}
#endif


#ifndef FTL_READONLY
/* Logs a successful erase for the specified vBlock on the specified bank */
static void ftl_vfl_log_success(uint32_t bank, uint32_t vblock)
{
    uint32_t i;
    for (i = 0; i < 5; i++)
        if (ftl_troublelog[i].block == vblock
         && ftl_troublelog[i].bank == bank)
        {
            if (--ftl_troublelog[i].errors == 0)
                ftl_troublelog[i].block = 0xFFFF;
            return;
        }
}
#endif


#ifndef FTL_READONLY
/* Tries to remap the specified vBlock on the specified bank,
   not caring about data in there.
   If it worked, it will return the new pBlock number,
   if not (no more spare blocks available), it will return zero. */
static uint32_t ftl_vfl_remap_block(uint32_t bank, uint32_t block)
{
    uint32_t i;
    uint32_t newblock = 0, newidx;
    panicf("FTL: Remapping bank %u block %u!", (unsigned)bank, (unsigned)block);
    if (bank >= ftl_banks || block >= ftl_nand_type->blocks) return 0;
    for (i = 0; i < ftl_vfl_cxt[bank].sparecount; i++)
        if (ftl_vfl_cxt[bank].remaptable[i] == 0)
        {
            newblock = ftl_vfl_cxt[bank].firstspare + i;
            newidx = i;
            break;
        }
    if (newblock == 0) return 0;
    for (i = 0; i < 9; i++)
        if (nand_block_erase(bank,
                             newblock * ftl_nand_type->pagesperblock) == 0)
            break;
    for (i = 0; i < newidx; i++)
        if (ftl_vfl_cxt[bank].remaptable[i] == block)
            ftl_vfl_cxt[bank].remaptable[i] = 0xFFFF;
    ftl_vfl_cxt[bank].remaptable[newidx] = block;
    ftl_vfl_cxt[bank].spareused++;
    ftl_vfl_set_good_block(bank, block, 0);
    return newblock;
}
#endif


/* Reads the specified vPage, dealing with all kinds of trouble */
static uint32_t ftl_vfl_read(uint32_t vpage, void* buffer, void* sparebuffer,
                             uint32_t checkempty, uint32_t remaponfail)
{
#ifdef VFL_TRACE
    DEBUGF("FTL: VFL: Reading page %d\n", vpage);
#endif

    uint32_t abspage = vpage + ppb * syshyperblocks;
    if (abspage >= ftl_nand_type->blocks * ppb || abspage < ppb)
    {
        DEBUGF("FTL: Trying to read out-of-bounds vPage %u\n", (unsigned)vpage);
        return 4;
    }

    uint32_t bank = abspage % ftl_banks;
    uint32_t block = abspage / (ftl_nand_type->pagesperblock * ftl_banks);
    uint32_t page = (abspage / ftl_banks) % ftl_nand_type->pagesperblock;
    uint32_t physblock = ftl_vfl_get_physical_block(bank, block);
    uint32_t physpage = physblock * ftl_nand_type->pagesperblock + page;

    uint32_t ret = nand_read_page(bank, physpage, buffer,
                                  sparebuffer, 1, checkempty);

    if ((ret & 0x11D) != 0 && (ret & 2) == 0)
    {
        nand_reset(bank);
        ret = nand_read_page(bank, physpage, buffer,
                             sparebuffer, 1, checkempty);
#ifdef FTL_READONLY
        (void)remaponfail;
#else
        if (remaponfail == 1 &&(ret & 0x11D) != 0 && (ret & 2) == 0)
        {
            DEBUGF("FTL: VFL: Scheduling vBlock %d for remapping!\n", block);
            ftl_vfl_schedule_block_for_remap(bank, block);
        }
#endif
        return ret;
    }

    return ret;
}


/* Multi-bank version of ftl_vfl_read, will read ftl_banks pages in parallel */
static uint32_t ftl_vfl_read_fast(uint32_t vpage, void* buffer, void* sparebuffer,
                                  uint32_t checkempty, uint32_t remaponfail)
{
#ifdef VFL_TRACE
    DEBUGF("FTL: VFL: Fast reading page %d on all banks\n", vpage);
#endif

    uint32_t i, rc = 0;
    uint32_t abspage = vpage + ppb * syshyperblocks;
    if (abspage + ftl_banks - 1 >= ftl_nand_type->blocks * ppb || abspage < ppb)
    {
        DEBUGF("FTL: Trying to read out-of-bounds vPage %u\n", (unsigned)vpage);
        return 4;
    }

    uint32_t bank = abspage % ftl_banks;
    uint32_t block = abspage / (ftl_nand_type->pagesperblock * ftl_banks);
    uint32_t page = (abspage / ftl_banks) % ftl_nand_type->pagesperblock;
    uint32_t remapped = 0;
    for (i = 0; i < ftl_banks; i++)
        if (ftl_vfl_get_physical_block(i, block) != block)
            remapped = 1;
    if (bank || remapped)
    {
        for (i = 0; i < ftl_banks; i++)
        {
            void* databuf = NULL;
            void* sparebuf = NULL;
            if (buffer) databuf = (void*)((uint32_t)buffer + 0x800 * i);
            if (sparebuffer) sparebuf = (void*)((uint32_t)sparebuffer + 0x40 * i);
            uint32_t ret = ftl_vfl_read(vpage + i, databuf, sparebuf, checkempty, remaponfail);
            if (ret & 1) rc |= 1 << (i << 2);
            if (ret & 2) rc |= 2 << (i << 2);
            if (ret & 0x10) rc |= 4 << (i << 2);
            if (ret & 0x100) rc |= 8 << (i << 2);
        }
        return rc;
    }
    uint32_t physpage = block * ftl_nand_type->pagesperblock + page;

    rc = nand_read_page_fast(physpage, buffer, sparebuffer, 1, checkempty);
    if (!(rc & 0xdddd)) return rc;

    for (i = 0; i < ftl_banks; i++)
    {
        if ((rc >> (i << 2)) & 0x2) continue;
        if ((rc >> (i << 2)) & 0xd)
        {
            rc &= ~(0xf << (i << 2));
            nand_reset(i);
            uint32_t ret = nand_read_page(i, physpage,
                                          (void*)((uint32_t)buffer + 0x800 * i),
                                          (void*)((uint32_t)sparebuffer + 0x40 * i),
                                          1, checkempty);
#ifdef FTL_READONLY
            (void)remaponfail;
#else
            if (remaponfail == 1 && (ret & 0x11D) != 0 && (ret & 2) == 0)
                ftl_vfl_schedule_block_for_remap(i, block);
#endif
            if (ret & 1) rc |= 1 << (i << 2);
            if (ret & 2) rc |= 2 << (i << 2);
            if (ret & 0x10) rc |= 4 << (i << 2);
            if (ret & 0x100) rc |= 8 << (i << 2);
        }
    }

    return rc;
}


#ifndef FTL_READONLY
/* Writes the specified vPage, dealing with all kinds of trouble */
static uint32_t ftl_vfl_write(uint32_t vpage, uint32_t count,
                              void* buffer, void* sparebuffer)
{
    uint32_t i, j;
#ifdef VFL_TRACE
    DEBUGF("FTL: VFL: Writing page %d\n", vpage);
#endif

    uint32_t abspage = vpage + ppb * syshyperblocks;
    if (abspage + count > ftl_nand_type->blocks * ppb || abspage < ppb)
    {
        DEBUGF("FTL: Trying to write out-of-bounds vPage %u\n",
               (unsigned)vpage);
        return 4;
    }

    static uint32_t bank[5];
    static uint32_t block[5];
    static uint32_t physpage[5];

    for (i = 0; i < count; i++, abspage++)
    {
        for (j = ftl_banks; j > 0; j--)
        {
            bank[j] = bank[j - 1];
            block[j] = block[j - 1];
            physpage[j] = physpage[j - 1];
        }
        bank[0] = abspage % ftl_banks;
        block[0] = abspage / (ftl_nand_type->pagesperblock * ftl_banks);
        uint32_t page = (abspage / ftl_banks) % ftl_nand_type->pagesperblock;
        uint32_t physblock = ftl_vfl_get_physical_block(bank[0], block[0]);
        physpage[0] = physblock * ftl_nand_type->pagesperblock + page;

        if (i >= ftl_banks)
            if (nand_write_page_collect(bank[ftl_banks]))
                if (nand_read_page(bank[ftl_banks], physpage[ftl_banks],
                                   ftl_buffer, &ftl_sparebuffer[0], 1, 1) & 0x11F)
                {
                    panicf("FTL: write error (2) on vPage %u, bank %u, pPage %u",
                           (unsigned)(vpage + i - ftl_banks),
                           (unsigned)bank[ftl_banks],
                           (unsigned)physpage[ftl_banks]);
                    ftl_vfl_log_trouble(bank[ftl_banks], block[ftl_banks]);
                }
        if (nand_write_page_start(bank[0], physpage[0],
                                  (void*)((uint32_t)buffer + 0x800 * i),
                                  (void*)((uint32_t)sparebuffer + 0x40 * i), 1))
            if (nand_read_page(bank[0], physpage[0], ftl_buffer,
                               &ftl_sparebuffer[0], 1, 1) & 0x11F)
            {
                panicf("FTL: write error (1) on vPage %u, bank %u, pPage %u",
                       (unsigned)(vpage + i), (unsigned)bank[0], (unsigned)physpage[0]);
                ftl_vfl_log_trouble(bank[0], block[0]);
            }
    }

    for (i = (count < ftl_banks ? count : ftl_banks); i > 0; i--)
        if (nand_write_page_collect(bank[i - 1]))
            if (nand_read_page(bank[i - 1], physpage[i - 1],
                               ftl_buffer, &ftl_sparebuffer[0], 1, 1) & 0x11F)
            {
                panicf("FTL: write error (2) on vPage %u, bank %u, pPage %u",
                       (unsigned)(vpage + count - i),
                       (unsigned)bank[i - 1], (unsigned)physpage[i - 1]);
                ftl_vfl_log_trouble(bank[i - 1], block[i - 1]);
            }

    return 0;
}
#endif


/* Mounts the VFL on all banks */
static uint32_t ftl_vfl_open(void)
{
    uint32_t i, j, k;
    uint32_t minusn, vflcxtidx, last;
    struct ftl_vfl_cxt_type* cxt;
    uint16_t vflcxtblock[4];
#ifndef FTL_READONLY
    ftl_vfl_usn = 0;
#else
    /* Temporary BBT buffer if we're readonly,
       as we won't need it again after mounting */
    uint8_t bbt[0x410];
#endif

    FTL_PROGRESS("N3G_VFL_SCAN banks=%lu sys=%lu ppb=%lu",
                 (unsigned long)ftl_banks,
                 (unsigned long)syshyperblocks, (unsigned long)ppb);
    for (i = 0; i < ftl_banks; i++)
#ifndef FTL_READONLY
        if (ftl_load_bbt(i, ftl_bbt[i]) == 0)
#else
        if (ftl_load_bbt(i, bbt) == 0)
#endif
        {
            FTL_PROGRESS("N3G_BBT_OK bank=%lu", (unsigned long)i);
            for (j = 1; j <= syshyperblocks; j++)
            {
                if (n3g_ctx_trace_compact && j > N3G_VFL_SCAN_BLOCK_LIMIT)
                {
                    FTL_PROGRESS("N3G_VFL_TIMEOUT bank=%lu scanned=%lu",
                                 (unsigned long)i,
                                 (unsigned long)(j - 1));
                    break;
                }
                if (n3g_ctx_trace_compact && (j & 0x0f) == 1)
                    FTL_PROGRESS("N3G_VFL_PROGRESS bank=%lu block=%lu",
                                 (unsigned long)i, (unsigned long)j);
                if (!n3g_ctx_trace_compact && (j & 0xf) == 0)
                    FTL_PROGRESS("FTL_SCAN_PROGRESS vfl bank %lu blk %lu page %lu",
                                 (unsigned long)i, (unsigned long)j,
                                 (unsigned long)(j * ftl_nand_type->pagesperblock));
#ifndef FTL_READONLY
                if (ftl_is_good_block(ftl_bbt[i], j) != 0)
#else
                if (ftl_is_good_block(bbt, j) != 0)
#endif
                    if (ftl_vfl_read_page(i, j, 0, ftl_buffer,
                                          &ftl_sparebuffer[0]) == 0)
                    {
                        struct ftl_vfl_cxt_type* cxt;
                        cxt = (struct ftl_vfl_cxt_type*)ftl_buffer;
                        memcpy(vflcxtblock, &cxt->vflcxtblocks, 8);
                        minusn = 0xFFFFFFFF;
                        vflcxtidx = 4;
                        for (k = 0; k < 4; k++)
                            if (vflcxtblock[k] != 0xFFFF)
                                if (ftl_vfl_read_page(i, vflcxtblock[k], 0,
                                                      ftl_buffer,
                                                      &ftl_sparebuffer[0]) == 0)
                                    if (ftl_sparebuffer[0].meta.usn > 0
                                     && ftl_sparebuffer[0].meta.usn <= minusn)
                                    {
                                        minusn = ftl_sparebuffer[0].meta.usn;
                                        vflcxtidx = k;
                                    }
                        if (vflcxtidx == 4)
                        {
                            DEBUGF("FTL: No VFL CXT block found on bank %u!\n",
                                   (unsigned)i);
                            FTL_PROGRESS("N3G_VFL_CXT_FAIL bank=%lu", (unsigned long)i);
                            return 1;
                        }
                        last = 0;
                        uint32_t max = ftl_nand_type->pagesperblock;
                        for (k = 8; k < max; k += 8)
                        {
                            if (ftl_vfl_read_page(i, vflcxtblock[vflcxtidx],
                                                  k, ftl_buffer,
                                                  &ftl_sparebuffer[0]) != 0)
                                break;
                            last = k;
                        }
                        if (ftl_vfl_read_page(i, vflcxtblock[vflcxtidx],
                                              last, ftl_buffer,
                                              &ftl_sparebuffer[0]) != 0)
                            panicf("FTL: Re-reading VFL CXT block "
                                        "on bank %u failed!?", (unsigned)i);
                            //return 1;
                        memcpy(&ftl_vfl_cxt[i], ftl_buffer, 0x800);
                        if (ftl_vfl_verify_checksum(i) != 0)
                        {
                            FTL_PROGRESS("N3G_VFL_CKSUM_FAIL bank=%lu",
                                         (unsigned long)i);
                            return 1;
                        }
                        FTL_PROGRESS("N3G_VFL_FOUND bank=%lu ctrl=%04x %04x %04x",
                                     (unsigned long)i,
                                     ftl_vfl_cxt[i].ftlctrlblocks[0],
                                     ftl_vfl_cxt[i].ftlctrlblocks[1],
                                     ftl_vfl_cxt[i].ftlctrlblocks[2]);
#ifndef FTL_READONLY
                        if (ftl_vfl_usn < ftl_vfl_cxt[i].usn)
                            ftl_vfl_usn = ftl_vfl_cxt[i].usn;
#endif
                        break;
                    }
            }
        }
        else
        {
            DEBUGF("FTL: Couldn't load bank %u lowlevel BBT!\n", (unsigned)i);
            FTL_PROGRESS("N3G_BBT_FAIL bank=%lu", (unsigned long)i);
            return 1;
        }
    cxt = ftl_vfl_get_newest_cxt();
    if (cxt == NULL)
    {
        FTL_PROGRESS("N3G_VFL_NEWEST_FAIL");
        return 1;
    }
    for (i = 0; i < ftl_banks; i++)
        memcpy(ftl_vfl_cxt[i].ftlctrlblocks, cxt->ftlctrlblocks, 6);
    FTL_PROGRESS("N3G_VFL_OPEN_OK");
    return 0;
}


/* Mounts the actual FTL */
static uint32_t ftl_open(void)
{
    uint32_t i;
    uint32_t ret;
    struct ftl_vfl_cxt_type* cxt = ftl_vfl_get_newest_cxt();

    FTL_PROGRESS("ftl open ctrl %04x %04x %04x",
                 cxt->ftlctrlblocks[0],
                 cxt->ftlctrlblocks[1],
                 cxt->ftlctrlblocks[2]);
    uint32_t ftlcxtblock = 0xffffffff;
    uint32_t minusn = 0xffffffff;
    for (i = 0; i < 3; i++)
    {
        FTL_PROGRESS("ftl cxt cand %lu vb %u",
                     (unsigned long)i, cxt->ftlctrlblocks[i]);
        ret = ftl_vfl_read(ppb * cxt->ftlctrlblocks[i],
                           ftl_buffer, &ftl_sparebuffer[0], 1, 0);
        if ((ret &= 0x11F) != 0)
        {
            FTL_PROGRESS("ftl cxt cand read fail ret %08lx",
                         (unsigned long)ret);
            continue;
        }
        FTL_PROGRESS("ftl cxt sig type %02x usn %08lx",
                     ftl_sparebuffer[0].meta.type,
                     (unsigned long)ftl_sparebuffer[0].meta.usn);
        if (ftl_sparebuffer[0].meta.type - 0x43 > 4)
        {
            FTL_PROGRESS("ftl cxt sig mismatch type %02x",
                         ftl_sparebuffer[0].meta.type);
            continue;
        }
        if (ftlcxtblock != 0xffffffff && ftl_sparebuffer[0].meta.usn >= minusn)
            continue;
        minusn = ftl_sparebuffer[0].meta.usn;
        ftlcxtblock = cxt->ftlctrlblocks[i];
    }

    if (ftlcxtblock == 0xffffffff)
    {
        DEBUGF("FTL: Couldn't find readable FTL CXT block!\n");
        FTL_PROGRESS("ftl no readable cxt");
        return 1;
    }

    DEBUGF("FTL: Found FTL context block: vBlock %d\n", ftlcxtblock);
    FTL_PROGRESS("FIRST_VALID_BLOCK ftlcxt %lu", (unsigned long)ftlcxtblock);
    uint32_t ftlcxtfound = 0;
    for (i = ftl_nand_type->pagesperblock * ftl_banks - 1; i > 0; i--)
    {
        if (!n3g_ctx_trace_compact && (i & 0xf) == 0)
            FTL_PROGRESS("ftl cxt page scan %lu",
                         (unsigned long)(ppb * ftlcxtblock + i));
        ret = ftl_vfl_read(ppb * ftlcxtblock + i,
                           ftl_buffer, &ftl_sparebuffer[0], 1, 0);
        if ((ret & 0x11F) != 0) continue;
        else if (ftl_sparebuffer[0].meta.type == 0x43)
        {
            memcpy(&ftl_cxt, ftl_buffer, 0x28C);
            ftlcxtfound = 1;
            FTL_PROGRESS("ftl cxt page found i%lu usn %08lx",
                         (unsigned long)i,
                         (unsigned long)ftl_sparebuffer[0].meta.usn);
            break;
        }
        else
        {
            /* This will trip if there was an unclean unmount before. */
            DEBUGF("FTL: Unclean shutdown before!\n");
            FTL_PROGRESS("ftl cxt type mismatch %02x at i%lu",
                         ftl_sparebuffer[0].meta.type, (unsigned long)i);
#ifdef FTL_FORCEMOUNT
            DEBUGF("FTL: Forcing mount nevertheless...\n");
#else
            break;
#endif
        }
    }

    if (ftlcxtfound == 0)
    {
        DEBUGF("FTL: Couldn't find FTL CXT page!\n");
        FTL_PROGRESS("ftl cxt page not found");
        return 1;
    }

    DEBUGF("FTL: Successfully read FTL context block\n");
    FTL_PROGRESS("ftl cxt ok map0 %08lx",
                 (unsigned long)ftl_cxt.ftl_map_pages[0]);
    uint32_t pagestoread = ftl_nand_type->userblocks >> 10;
    if ((ftl_nand_type->userblocks & 0x1FF) != 0) pagestoread++;

    for (i = 0; i < pagestoread; i++)
    {
        FTL_PROGRESS("ftl map page %lu vp %08lx",
                     (unsigned long)i,
                     (unsigned long)ftl_cxt.ftl_map_pages[i]);
        if ((ftl_vfl_read(ftl_cxt.ftl_map_pages[i],
                          ftl_buffer, &ftl_sparebuffer[0], 1, 1) & 0x11F) != 0)
        {
            DEBUGF("FTL: Failed to read block map page %u\n", (unsigned)i);
            FTL_PROGRESS("ftl map read fail %lu", (unsigned long)i);
            return 1;
        }

        uint32_t toread = 2048;
        if (toread > (ftl_nand_type->userblocks << 1) - (i << 11))
            toread = (ftl_nand_type->userblocks << 1) - (i << 11);

        memcpy(&ftl_map[i << 10], ftl_buffer, toread);
    }

    uint32_t map_nonempty = 0;
    for (i = 0; i < ftl_nand_type->userblocks; i++)
        if (ftl_map[i] != 0 && ftl_map[i] != 0xffff)
        {
            map_nonempty = 1;
            break;
        }
    FTL_PROGRESS("FTL_SCAN_PROGRESS map built first %04x %04x nonempty %lu",
                 ftl_map[0], ftl_map[1], (unsigned long)map_nonempty);
    if (!map_nonempty)
        FTL_PROGRESS("INIT_FAIL map empty");

#ifndef FTL_READONLY
    pagestoread = (ftl_nand_type->userblocks + 23) >> 10;
    if (((ftl_nand_type->userblocks + 23) & 0x1FF) != 0) pagestoread++;

    for (i = 0; i < pagestoread; i++)
    {
        if ((ftl_vfl_read(ftl_cxt.ftl_erasectr_pages[i],
                          ftl_buffer, &ftl_sparebuffer[0], 1, 1) & 0x11F) != 0)
        {
            DEBUGF("FTL: Failed to read erase counter page %u\n", (unsigned)i);
            return 1;
        }

        uint32_t toread = 2048;
        if (toread > ((ftl_nand_type->userblocks + 23) << 1) - (i << 11))
            toread = ((ftl_nand_type->userblocks + 23) << 1) - (i << 11);

        memcpy(&ftl_erasectr[i << 10], ftl_buffer, toread);
    }

    for (i = 0; i < 0x11; i++)
    {
        ftl_log[i].scatteredvblock = 0xFFFF;
        ftl_log[i].logicalvblock = 0xFFFF;
        ftl_log[i].pageoffsets = ftl_offsets[i];
    }

    memset(ftl_troublelog, 0xFF, 20);
    memset(ftl_erasectr_dirt, 0, 8);
#endif

#ifdef FTL_DEBUG
    uint32_t j, k;
    for (i = 0; i < ftl_banks; i++)
    {
        uint32_t badblocks = 0;
#ifndef FTL_READONLY
        for (j = 0; j < ftl_nand_type->blocks >> 3; j++)
        {
            uint8_t bbtentry = ftl_bbt[i][j];
            for (k = 0; k < 8; k++) if ((bbtentry & (1 << k)) == 0) badblocks++;
        }
        DEBUGF("FTL: BBT for bank %d: %d bad blocks\n", i, badblocks);
        badblocks = 0;
#endif
        for (j = 0; j < ftl_vfl_cxt[i].sparecount; j++)
            if (ftl_vfl_cxt[i].remaptable[j] == 0xFFFF) badblocks++;
        DEBUGF("FTL: VFL: Bank %d: %d of %d spare blocks are bad\n",
               i, badblocks, ftl_vfl_cxt[i].sparecount);
        DEBUGF("FTL: VFL: Bank %d: %d blocks remapped\n",
               i, ftl_vfl_cxt[i].spareused);
        DEBUGF("FTL: VFL: Bank %d: %d blocks scheduled for remapping\n",
               i, 0x334 - ftl_vfl_cxt[i].scheduledstart);
    }
#ifndef FTL_READONLY
    uint32_t min = 0xFFFFFFFF, max = 0, total = 0;
    for (i = 0; i < ftl_nand_type->userblocks + 23; i++)
    {
        if (ftl_erasectr[i] > max) max = ftl_erasectr[i];
        if (ftl_erasectr[i] < min) min = ftl_erasectr[i];
        total += ftl_erasectr[i];
    }
    DEBUGF("FTL: Erase counters: Minimum: %d, maximum %d, average: %d, total: %d\n",
           min, max, total / (ftl_nand_type->userblocks + 23), total);
#endif
#endif

    return 0;
}


#ifndef FTL_READONLY
/* Returns a pointer to the ftl_log entry for the specified vBlock,
   or null, if there is none */
static struct ftl_log_type* ftl_get_log_entry(uint32_t block)
{
    uint32_t i;
    for (i = 0; i < 0x11; i++)
    {
        if (ftl_log[i].scatteredvblock == 0xFFFF) continue;
        if (ftl_log[i].logicalvblock == block) return &ftl_log[i];
    }
    return NULL;
}
#endif

/* Exposed function: Read highlevel sectors */
uint32_t ftl_read(uint32_t sector, uint32_t count, void* buffer)
{
    uint32_t i, j;
    uint32_t error = 0;

#ifdef FTL_TRACE
    DEBUGF("FTL: Reading %d sectors starting at %d\n", count, sector);
#endif

    if (sector + count > ftl_nand_type->userblocks * ppb)
    {
        DEBUGF("FTL: Sector %d is out of range!\n", sector + count - 1);
        return -2;
    }
    if (count == 0) return 0;

    mutex_lock(&ftl_mtx);

    for (i = 0; i < count; i++)
    {
        uint32_t block = (sector + i) / ppb;
        uint32_t page = (sector + i) % ppb;

        uint32_t abspage = ftl_map[block] * ppb + page;
#ifndef FTL_READONLY
        struct ftl_log_type* logentry = ftl_get_log_entry(block);
        if (logentry != NULL)
        {
#ifdef FTL_TRACE
            DEBUGF("FTL: Block %d has a log entry\n", block);
#endif
            if (logentry->scatteredvblock != 0xFFFF
             && logentry->pageoffsets[page] != 0xFFFF)
            {
#ifdef FTL_TRACE
             DEBUGF("FTL: Found page %d at block %d, page %d\n", page,
                    (*logentry).scatteredvblock, (*logentry).pageoffsets[page]);
#endif
                abspage = logentry->scatteredvblock * ppb
                        + logentry->pageoffsets[page];
            }
        }
#endif

#ifndef FTL_READONLY
        if (count >= i + ftl_banks && !(page & (ftl_banks - 1))
         && logentry == NULL)
#else
        if (count >= i + ftl_banks && !(page & (ftl_banks - 1)))
#endif
        {
            uint32_t ret = ftl_vfl_read_fast(abspage, &((uint8_t*)buffer)[i << 11],
                                             &ftl_sparebuffer[0], 1, 1);
            for (j = 0; j < ftl_banks; j++)
                if (ret & (2 << (j << 2)))
                    memset(&((uint8_t*)buffer)[(i + j) << 11], 0, 0x800);
                else if ((ret & (0xd << (j << 2))) || ftl_sparebuffer[j].user.eccmark != 0xFF)
                {
                    DEBUGF("FTL: Error while reading sector %d!\n", (sector + i));
                    error = -3;
                    memset(&((uint8_t*)buffer)[(i + j) << 11], 0, 0x800);
                }
            i += ftl_banks - 1;
        }
        else
        {
            uint32_t ret = ftl_vfl_read(abspage, &((uint8_t*)buffer)[i << 11],
                                        &ftl_sparebuffer[0], 1, 1);
            if (ret & 2) memset(&((uint8_t*)buffer)[i << 11], 0, 0x800);
            else if ((ret & 0x11D) != 0 || ftl_sparebuffer[0].user.eccmark != 0xFF)
            {
                DEBUGF("FTL: Error while reading sector %d!\n", (sector + i));
                error = -4;
                memset(&((uint8_t*)buffer)[i << 11], 0, 0x800);
            }
        }
    }

    mutex_unlock(&ftl_mtx);

    return error;
}


#ifndef FTL_READONLY
/* Performs a vBlock erase, dealing with hardware,
   remapping and all kinds of trouble */
static uint32_t ftl_erase_block_internal(uint32_t block)
{
    uint32_t i, j;
    block = block + ftl_nand_type->blocks
          - ftl_nand_type->userblocks - 0x17;
    if (block == 0 || block >= ftl_nand_type->blocks) return 1;
    for (i = 0; i < ftl_banks; i++)
    {
        if (ftl_vfl_check_remap_scheduled(i, block) == 1)
        {
            ftl_vfl_remap_block(i, block);
            ftl_vfl_mark_remap_done(i, block);
        }
        ftl_vfl_log_success(i, block);
        uint32_t pblock = ftl_vfl_get_physical_block(i, block);
        uint32_t rc;
        for (j = 0; j < 3; j++)
        {
            rc = nand_block_erase(i, pblock * ftl_nand_type->pagesperblock);
            if (rc == 0) break;
        }
        if (rc != 0)
        {
            panicf("FTL: Block erase failed on bank %u block %u",
                   (unsigned)i, (unsigned)block);
            if (pblock != block)
            {
                uint32_t spareindex = pblock - ftl_vfl_cxt[i].firstspare;
                ftl_vfl_cxt[i].remaptable[spareindex] = 0xFFFF;
            }
            ftl_vfl_cxt[i].field_18++;
            if (ftl_vfl_remap_block(i, block) == 0) return 1;
            if (ftl_vfl_commit_cxt(i) != 0) return 1;
            memset(&ftl_sparebuffer, 0, 0x40);
            nand_write_page(i, pblock, &ftl_vfl_cxt[0], &ftl_sparebuffer, 1);
        }
    }
    return 0;
}
#endif


#ifndef FTL_READONLY
/* Highlevel vBlock erase, that increments the erase counter for the block */
static uint32_t ftl_erase_block(uint32_t block)
{
    ftl_erasectr[block]++;
    if (ftl_erasectr_dirt[block >> 10] == 100) ftl_cxt.erasedirty = 1;
    else ftl_erasectr_dirt[block >> 10]++;
    return ftl_erase_block_internal(block);
}
#endif


#ifndef FTL_READONLY
/* Allocates a block from the pool,
   returning its vBlock number, or 0xFFFFFFFF on error */
static uint32_t ftl_allocate_pool_block(void)
{
    uint32_t i;
    uint32_t erasectr = 0xFFFFFFFF, bestidx = 0xFFFFFFFF, block;
    for (i = 0; i < ftl_cxt.freecount; i++)
    {
        uint32_t idx = ftl_cxt.nextfreeidx + i;
        if (idx >= 0x14) idx -= 0x14;
        if (!ftl_cxt.blockpool[idx]) continue;
        if (ftl_erasectr[ftl_cxt.blockpool[idx]] < erasectr)
        {
            erasectr = ftl_erasectr[ftl_cxt.blockpool[idx]];
            bestidx = idx;
        }
    }
    if (bestidx == 0xFFFFFFFF) panicf("FTL: Out of pool blocks!");
    block = ftl_cxt.blockpool[bestidx];
    if (bestidx != ftl_cxt.nextfreeidx)
    {
        ftl_cxt.blockpool[bestidx] = ftl_cxt.blockpool[ftl_cxt.nextfreeidx];
        ftl_cxt.blockpool[ftl_cxt.nextfreeidx] = block;
    }
    if (block > (uint32_t)ftl_nand_type->userblocks + 0x17)
        panicf("FTL: Bad block number in pool: %u", (unsigned)block);
    if (ftl_erase_block(block) != 0) return 0xFFFFFFFF;
    if (++ftl_cxt.nextfreeidx == 0x14) ftl_cxt.nextfreeidx = 0;
    ftl_cxt.freecount--;
    return block;
}
#endif


#ifndef FTL_READONLY
/* Releases a vBlock back into the pool */
static void ftl_release_pool_block(uint32_t block)
{
    if (block >= (uint32_t)ftl_nand_type->userblocks + 0x17)
        panicf("FTL: Tried to release block %u", (unsigned)block);
    uint32_t idx = ftl_cxt.nextfreeidx + ftl_cxt.freecount++;
    if (idx >= 0x14) idx -= 0x14;
    ftl_cxt.blockpool[idx] = block;
}
#endif


#ifndef FTL_READONLY
/* Commits the location of the FTL context blocks
   to a semi-randomly chosen VFL context */
static uint32_t ftl_store_ctrl_block_list(void)
{
    uint32_t i;
    for (i = 0; i < ftl_banks; i++)
        memcpy(ftl_vfl_cxt[i].ftlctrlblocks, ftl_cxt.ftlctrlblocks, 6);
    return ftl_vfl_commit_cxt(ftl_vfl_usn % ftl_banks);
}
#endif


#ifndef FTL_READONLY
/* Saves the n-th erase counter page to the flash,
   because it is too dirty or needs to be moved. */
static uint32_t ftl_save_erasectr_page(uint32_t index)
{
    memset(&ftl_sparebuffer[0], 0xFF, 0x40);
    ftl_sparebuffer[0].meta.usn = ftl_cxt.usn;
    ftl_sparebuffer[0].meta.idx = index;
    ftl_sparebuffer[0].meta.type = 0x46;
    if (ftl_vfl_write(ftl_cxt.ftlctrlpage, 1, &ftl_erasectr[index << 10],
                      &ftl_sparebuffer[0]) != 0)
        return 1;
    if ((ftl_vfl_read(ftl_cxt.ftlctrlpage, ftl_buffer,
                      &ftl_sparebuffer[0], 1, 1) & 0x11F) != 0)
        return 1;
    if (memcmp(ftl_buffer, &ftl_erasectr[index << 10], 0x800) != 0) return 1;
    if (ftl_sparebuffer[0].meta.type != 0x46) return 1;
    if (ftl_sparebuffer[0].meta.idx != index) return 1;
    if (ftl_sparebuffer[0].meta.usn != ftl_cxt.usn) return 1;
    ftl_cxt.ftl_erasectr_pages[index] = ftl_cxt.ftlctrlpage;
    ftl_erasectr_dirt[index] = 0;
    return 0;
}
#endif


#ifndef FTL_READONLY
/* Increments ftl_cxt.ftlctrlpage to the next available FTL context page,
   allocating a new context block if neccessary. */
static uint32_t ftl_next_ctrl_pool_page(void)
{
    uint32_t i;
    if (++ftl_cxt.ftlctrlpage % ppb != 0) return 0;
    for (i = 0; i < 3; i++)
        if ((ftl_cxt.ftlctrlblocks[i] + 1) * ppb == ftl_cxt.ftlctrlpage)
            break;
    i = (i + 1) % 3;
    uint32_t oldblock = ftl_cxt.ftlctrlblocks[i];
    uint32_t newblock = ftl_allocate_pool_block();
    if (newblock == 0xFFFFFFFF) return 1;
    ftl_cxt.ftlctrlblocks[i] = newblock;
    ftl_cxt.ftlctrlpage = newblock * ppb;
    DEBUGF("Starting new FTL control block at %d\n", ftl_cxt.ftlctrlpage);
    uint32_t pagestoread = (ftl_nand_type->userblocks + 23) >> 10;
    if (((ftl_nand_type->userblocks + 23) & 0x1FF) != 0) pagestoread++;
    for (i = 0; i < pagestoread; i++)
        if (oldblock * ppb <= ftl_cxt.ftl_erasectr_pages[i]
         && (oldblock + 1) * ppb > ftl_cxt.ftl_erasectr_pages[i])
         {
            ftl_cxt.usn--;
            if (ftl_save_erasectr_page(i) != 0)
            {
                ftl_cxt.ftlctrlblocks[i] = oldblock;
                ftl_cxt.ftlctrlpage = oldblock * (ppb + 1) - 1;
                ftl_release_pool_block(newblock);
                return 1;
            }
            ftl_cxt.ftlctrlpage++;
         }
    ftl_release_pool_block(oldblock);
    return ftl_store_ctrl_block_list();
}
#endif


#ifndef FTL_READONLY
/* Copies a vPage from one location to another */
static uint32_t ftl_copy_page(uint32_t source, uint32_t destination,
                              uint32_t lpn, uint32_t type)
{
    uint32_t rc = ftl_vfl_read(source, ftl_copybuffer[0],
                               &ftl_copyspare[0], 1, 1) & 0x11F;
    memset(&ftl_copyspare[0], 0xFF, 0x40);
    ftl_copyspare[0].user.lpn = lpn;
    ftl_copyspare[0].user.usn = ++ftl_cxt.nextblockusn;
    ftl_copyspare[0].user.type = 0x40;
    if ((rc & 2) != 0) memset(ftl_copybuffer[0], 0, 0x800);
    else if (rc != 0) ftl_copyspare[0].user.eccmark = 0x55;
    if (type == 1 && destination % ppb == ppb - 1)
        ftl_copyspare[0].user.type = 0x41;
    return ftl_vfl_write(destination, 1, ftl_copybuffer[0], &ftl_copyspare[0]);
}
#endif


#ifndef FTL_READONLY
/* Copies a pBlock to a vBlock */
static uint32_t ftl_copy_block(uint32_t source, uint32_t destination)
{
    uint32_t i, j;
    uint32_t error = 0;
    ftl_cxt.nextblockusn++;
    for (i = 0; i < ppb; i += FTL_COPYBUF_SIZE)
    {
        uint32_t rc = ftl_read(source * ppb + i,
                               FTL_COPYBUF_SIZE, ftl_copybuffer[0]);
        memset(&ftl_copyspare[0], 0xFF, 0x40 * FTL_COPYBUF_SIZE);
        for (j = 0; j < FTL_COPYBUF_SIZE; j++)
        {
            ftl_copyspare[j].user.lpn = source * ppb + i + j;
            ftl_copyspare[j].user.usn = ftl_cxt.nextblockusn;
            ftl_copyspare[j].user.type = 0x40;
            if (rc)
            {
                if (ftl_read(source * ppb + i + j, 1, ftl_copybuffer[j]))
                    ftl_copyspare[j].user.eccmark = 0x55;
            }
            if (i + j == ppb - 1) ftl_copyspare[j].user.type = 0x41;
        }
        if (ftl_vfl_write(destination * ppb + i, FTL_COPYBUF_SIZE,
                          ftl_copybuffer[0], &ftl_copyspare[0]))
        {
            error = 1;
            break;
        }
    }
    if (error != 0)
    {
        ftl_erase_block(destination);
        return 1;
    }
    return 0;
}
#endif


#ifndef FTL_READONLY
/* Clears ftl_log.issequential, if something violating that is written. */
static void ftl_check_still_sequential(struct ftl_log_type* entry, uint32_t page)
{
    if (entry->pagesused != entry->pagescurrent
     || entry->pageoffsets[page] != page)
        entry->issequential = 0;
}
#endif


#ifndef FTL_READONLY
/* Copies all pages that are currently used from the scattered page block in
   use by the supplied ftl_log entry to a newly-allocated one, and releases
   the old one.
   In other words: It kicks the pages containing old garbage out of it to make
   space again. This is usually done when a scattered page block is being
   removed because it is full, but less than half of the pages in there are
   still in use and rest is just filled with old crap. */
static uint32_t ftl_compact_scattered(struct ftl_log_type* entry)
{
    uint32_t i, j;
    uint32_t error;
    struct ftl_log_type backup;
    if (entry->pagescurrent == 0)
    {
        ftl_release_pool_block(entry->scatteredvblock);
        entry->scatteredvblock = 0xFFFF;
        return 0;
    }
    backup = *entry;
    memcpy(ftl_offsets_backup, entry->pageoffsets, 0x400);
    for (i = 0; i < 4; i++)
    {
        uint32_t block = ftl_allocate_pool_block();
        if (block == 0xFFFFFFFF) return 1;
        entry->pagesused = 0;
        entry->pagescurrent = 0;
        entry->issequential = 1;
        entry->scatteredvblock = block;
        error = 0;
        for (j = 0; j < ppb; j++)
            if (entry->pageoffsets[j] != 0xFFFF)
            {
                uint32_t lpn = entry->logicalvblock * ppb + j;
                uint32_t newpage = block * ppb + entry->pagesused;
                uint32_t oldpage = backup.scatteredvblock * ppb
                                 + entry->pageoffsets[j];
                if (ftl_copy_page(oldpage, newpage, lpn,
                                  entry->issequential) != 0)
                {
                    error = 1;
                    break;
                }
                entry->pageoffsets[j] = entry->pagesused++;
                entry->pagescurrent++;
                ftl_check_still_sequential(entry, j);
            }
        if (backup.pagescurrent != entry->pagescurrent) error = 1;
        if (error == 0)
        {
            ftl_release_pool_block(backup.scatteredvblock);
            break;
        }
        *entry = backup;
        memcpy(entry->pageoffsets, ftl_offsets_backup, 0x400);
    }
    return error;
}
#endif


#ifndef FTL_READONLY
/* Commits an ftl_log entry to proper blocks, no matter what's in there. */
static uint32_t ftl_commit_scattered(struct ftl_log_type* entry)
{
    uint32_t i;
    uint32_t error;
    uint32_t block;
    for (i = 0; i < 4; i++)
    {
        block = ftl_allocate_pool_block();
        if (block == 0xFFFFFFFF) return 1;
        error = ftl_copy_block(entry->logicalvblock, block);
        if (error == 0) break;
        ftl_release_pool_block(block);
    }
    if (error != 0) return 1;
    ftl_release_pool_block(entry->scatteredvblock);
    entry->scatteredvblock = 0xFFFF;
    ftl_release_pool_block(ftl_map[entry->logicalvblock]);
    ftl_map[entry->logicalvblock] = block;
    return 0;
}
#endif


#ifndef FTL_READONLY
/* Fills the rest of a scattered page block that was actually written
   sequentially until now, in order to be able to save a block erase by
   committing it without needing to copy it again.
   If this fails for whichever reason, it will be committed the usual way. */
static uint32_t ftl_commit_sequential(struct ftl_log_type* entry)
{
    uint32_t i;

    if (entry->issequential != 1
     || entry->pagescurrent != entry->pagesused)
        return 1;

    for (; entry->pagesused < ppb; )
    {
        uint32_t lpn = entry->logicalvblock * ppb + entry->pagesused;
        uint32_t newpage = entry->scatteredvblock * ppb
                         + entry->pagesused;
        uint32_t count = FTL_COPYBUF_SIZE < ppb - entry->pagesused
                       ? FTL_COPYBUF_SIZE : ppb - entry->pagesused;
        for (i = 0; i < count; i++)
            if (entry->pageoffsets[entry->pagesused + i] != 0xFFFF)
                return ftl_commit_scattered(entry);
        uint32_t rc = ftl_read(lpn, count, ftl_copybuffer[0]);
        memset(&ftl_copyspare[0], 0xFF, 0x40 * FTL_COPYBUF_SIZE);
        for (i = 0; i < count; i++)
        {
            ftl_copyspare[i].user.lpn = lpn + i;
            ftl_copyspare[i].user.usn = ++ftl_cxt.nextblockusn;
            ftl_copyspare[i].user.type = 0x40;
            if (rc) ftl_copyspare[i].user.eccmark = 0x55;
            if (entry->pagesused + i == ppb - 1)
                ftl_copyspare[i].user.type = 0x41;
        }
        if (ftl_vfl_write(newpage, count, ftl_copybuffer[0], &ftl_copyspare[0]))
            return ftl_commit_scattered(entry);
        entry->pagesused += count;
    }
    ftl_release_pool_block(ftl_map[entry->logicalvblock]);
    ftl_map[entry->logicalvblock] = entry->scatteredvblock;
    entry->scatteredvblock = 0xFFFF;
    return 0;
}
#endif


#ifndef FTL_READONLY
/* If a log entry is supplied, its scattered page block will be removed in
   whatever way seems most appropriate. Else, the oldest scattered page block
   will be freed by committing it. */
static uint32_t ftl_remove_scattered_block(struct ftl_log_type* entry)
{
    uint32_t i;
    uint32_t age = 0xFFFFFFFF, used = 0;
    if (entry == NULL)
    {
        for (i = 0; i < 0x11; i++)
        {
            if (ftl_log[i].scatteredvblock == 0xFFFF) continue;
            if (ftl_log[i].pagesused == 0 || ftl_log[i].pagescurrent == 0)
                return 1;
            if (ftl_log[i].usn < age
             || (ftl_log[i].usn == age && ftl_log[i].pagescurrent > used))
            {
                age = ftl_log[i].usn;
                used = ftl_log[i].pagescurrent;
                entry = &ftl_log[i];
            }
        }
        if (entry == NULL) return 1;
    }
    else if (entry->pagescurrent < ppb / 2)
    {
        ftl_cxt.swapcounter++;
        return ftl_compact_scattered(entry);
    }
    ftl_cxt.swapcounter++;
    if (entry->issequential == 1) return ftl_commit_sequential(entry);
    else return ftl_commit_scattered(entry);
}
#endif


#ifndef FTL_READONLY
/* Initialize a log entry to the values for an empty scattered page block */
static void ftl_init_log_entry(struct ftl_log_type* entry)
{
    entry->issequential = 1;
    entry->pagescurrent = 0;
    entry->pagesused = 0;
    memset(entry->pageoffsets, 0xFF, 0x400);
}
#endif


#ifndef FTL_READONLY
/* Allocates a log entry for the specified vBlock,
   first making space, if neccessary. */
static struct ftl_log_type* ftl_allocate_log_entry(uint32_t block)
{
    uint32_t i;
    struct ftl_log_type* entry = ftl_get_log_entry(block);
    if (entry != NULL)
    {
        entry->usn = ftl_cxt.nextblockusn - 1;
        return entry;
    }

    for (i = 0; i < 0x11; i++)
    {
        if (ftl_log[i].scatteredvblock == 0xFFFF) continue;
        if (ftl_log[i].pagesused == 0)
        {
            entry = &ftl_log[i];
            break;
        }
    }

    if (entry == NULL)
    {
        if (ftl_cxt.freecount < 3) panicf("FTL: Detected a pool block leak!");
        else if (ftl_cxt.freecount == 3)
            if (ftl_remove_scattered_block(NULL) != 0)
                return NULL;
        entry = ftl_log;
        while (entry->scatteredvblock != 0xFFFF) entry = &entry[1];
        entry->scatteredvblock = ftl_allocate_pool_block();
        if (entry->scatteredvblock == 0xFFFF)
            return NULL;
    }

    ftl_init_log_entry(entry);
    entry->logicalvblock = block;
    entry->usn = ftl_cxt.nextblockusn - 1;

    return entry;
}
#endif


#ifndef FTL_READONLY
/* Commits the FTL block map, erase counters, and context to flash */
static uint32_t ftl_commit_cxt(void)
{
    uint32_t i;
    uint32_t mappages = (ftl_nand_type->userblocks + 0x3ff) >> 10;
    uint32_t ctrpages = (ftl_nand_type->userblocks + 23 + 0x3ff) >> 10;
    uint32_t endpage = ftl_cxt.ftlctrlpage + mappages + ctrpages + 1;
    DEBUGF("FTL: Committing context\n");
    if (endpage >= (ftl_cxt.ftlctrlpage / ppb + 1) * ppb)
        ftl_cxt.ftlctrlpage |= ppb - 1;
    for (i = 0; i < ctrpages; i++)
    {
        if (ftl_next_ctrl_pool_page() != 0) return 1;
        if (ftl_save_erasectr_page(i) != 0) return 1;
    }
    for (i = 0; i < mappages; i++)
    {
        if (ftl_next_ctrl_pool_page() != 0) return 1;
        memset(&ftl_sparebuffer[0], 0xFF, 0x40);
        ftl_sparebuffer[0].meta.usn = ftl_cxt.usn;
        ftl_sparebuffer[0].meta.idx = i;
        ftl_sparebuffer[0].meta.type = 0x44;
        if (ftl_vfl_write(ftl_cxt.ftlctrlpage, 1, &ftl_map[i << 10],
                          &ftl_sparebuffer[0]) != 0)
            return 1;
        ftl_cxt.ftl_map_pages[i] = ftl_cxt.ftlctrlpage;
    }
    if (ftl_next_ctrl_pool_page() != 0) return 1;
    ftl_cxt.clean_flag = 1;
    memset(&ftl_sparebuffer[0], 0xFF, 0x40);
    ftl_sparebuffer[0].meta.usn = ftl_cxt.usn;
    ftl_sparebuffer[0].meta.type = 0x43;
    if (ftl_vfl_write(ftl_cxt.ftlctrlpage, 1, &ftl_cxt, &ftl_sparebuffer[0]) != 0)
        return 1;
    DEBUGF("FTL: Wrote context to page %d\n", ftl_cxt.ftlctrlpage);
    return 0;
}
#endif


#ifndef FTL_READONLY
/* Swaps the most and least worn block on the flash,
   to better distribute wear. It will not do anything
   if the wear spread is lower than 5 erases. */
static uint32_t ftl_swap_blocks(void)
{
    uint32_t i;
    uint32_t min = 0xFFFFFFFF, max = 0, maxidx = 0x14;
    uint32_t minidx = 0, minvb = 0, maxvb = 0;
    for (i = 0; i < ftl_cxt.freecount; i++)
    {
        uint32_t idx = ftl_cxt.nextfreeidx + i;
        if (idx >= 0x14) idx -= 0x14;
        if (ftl_erasectr[ftl_cxt.blockpool[idx]] > max)
        {
            maxidx = idx;
            maxvb = ftl_cxt.blockpool[idx];
            max = ftl_erasectr[maxidx];
        }
    }
    if (maxidx == 0x14) return 0;
    for (i = 0; i < ftl_nand_type->userblocks; i++)
    {
        if (ftl_erasectr[ftl_map[i]] > max) max = ftl_erasectr[ftl_map[i]];
        if (ftl_get_log_entry(i) != NULL) continue;
        if (ftl_erasectr[ftl_map[i]] < min)
        {
            minidx = i;
            minvb = ftl_map[i];
            min = ftl_erasectr[minidx];
        }
    }
    if (max - min < 5) return 0;
    if (minvb == maxvb) return 0;
    if (ftl_erase_block(maxvb) != 0) return 1;
    if (ftl_copy_block(minidx, maxvb) != 0) return 1;
    ftl_cxt.blockpool[maxidx] = minvb;
    ftl_map[minidx] = maxvb;
    return 0;
}
#endif


#ifndef FTL_READONLY
/* Exposed function: Write highlevel sectors */
uint32_t ftl_write(uint32_t sector, uint32_t count, const void* buffer)
{
    uint32_t i, j, k;

#ifdef FTL_TRACE
    DEBUGF("FTL: Writing %d sectors starting at %d\n", count, sector);
#endif

    if (sector + count > ftl_nand_type->userblocks * ppb)
    {
        DEBUGF("FTL: Sector %d is out of range!\n", sector + count - 1);
        return -2;
    }
    if (count == 0) return 0;

    mutex_lock(&ftl_mtx);

    if (ftl_cxt.clean_flag == 1)
    {
        for (i = 0; i < 3; i++)
        {
            DEBUGF("FTL: Marking dirty, try %d\n", i);
            if (ftl_next_ctrl_pool_page() != 0)
            {
                mutex_unlock(&ftl_mtx);
                return -3;
            }
            memset(ftl_buffer, 0xFF, 0x800);
            memset(&ftl_sparebuffer[0], 0xFF, 0x40);
            ftl_sparebuffer[0].meta.usn = ftl_cxt.usn;
            ftl_sparebuffer[0].meta.type = 0x47;
            if (ftl_vfl_write(ftl_cxt.ftlctrlpage, 1, ftl_buffer,
                              &ftl_sparebuffer[0]) == 0)
                break;
        }
        if (i == 3)
        {
            mutex_unlock(&ftl_mtx);
            return -4;
        }
        DEBUGF("FTL: Wrote dirty mark to %d\n", ftl_cxt.ftlctrlpage);
        ftl_cxt.clean_flag = 0;
    }

    for (i = 0; i < count; )
    {
        uint32_t block = (sector + i) / ppb;
        uint32_t page = (sector + i) % ppb;

        struct ftl_log_type* logentry = ftl_allocate_log_entry(block);
        if (logentry == NULL)
        {
            mutex_unlock(&ftl_mtx);
            return -5;
        }
        if (page == 0 && count - i >= ppb)
        {
#ifdef FTL_TRACE
            DEBUGF("FTL: Going to write a full hyperblock in one shot\n");
#endif
            uint32_t vblock = logentry->scatteredvblock;
            logentry->scatteredvblock = 0xFFFF;
            if (logentry->pagesused != 0)
            {
#ifdef FTL_TRACE
                DEBUGF("FTL: Scattered block had some pages already used, committing\n");
#endif
                ftl_release_pool_block(vblock);
                vblock = ftl_allocate_pool_block();
                if (vblock == 0xFFFFFFFF)
                {
                    mutex_unlock(&ftl_mtx);
                    return -6;
                }
            }
            ftl_cxt.nextblockusn++;
            for (j = 0; j < ppb; j += FTL_WRITESPARE_SIZE)
            {
                memset(&ftl_sparebuffer[0], 0xFF, 0x40 * FTL_WRITESPARE_SIZE);
                for (k = 0; k < FTL_WRITESPARE_SIZE; k++)
                {
                    ftl_sparebuffer[k].user.lpn = sector + i + j + k;
                    ftl_sparebuffer[k].user.usn = ftl_cxt.nextblockusn;
                    ftl_sparebuffer[k].user.type = 0x40;
                    if (j == ppb - 1) ftl_sparebuffer[k].user.type = 0x41;
                }
                uint32_t rc = ftl_vfl_write(vblock * ppb + j, FTL_WRITESPARE_SIZE,
                                            &((uint8_t*)buffer)[(i + j) << 11],
                                            &ftl_sparebuffer[0]);
                if (rc)
                    for (k = 0; k < ftl_banks; k++)
                        if (rc & (1 << k))
                        {
                            while (ftl_vfl_write(vblock * ppb + j + k, 1,
                                                 &((uint8_t*)buffer)[(i + j + k) << 11],
                                                 &ftl_sparebuffer[k]));
                        }
            }
            ftl_release_pool_block(ftl_map[block]);
            ftl_map[block] = vblock;
            i += ppb;
        }
        else
        {
            if (logentry->pagesused == ppb)
            {
#ifdef FTL_TRACE
                DEBUGF("FTL: Scattered block is full, committing\n");
#endif
                ftl_remove_scattered_block(logentry);
                logentry = ftl_allocate_log_entry(block);
                if (logentry == NULL)
                {
                    mutex_unlock(&ftl_mtx);
                    return -7;
                }
            }
            uint32_t cnt = FTL_WRITESPARE_SIZE;
            if (cnt > count - i) cnt = count - i;
            if (cnt > ppb - logentry->pagesused) cnt = ppb - logentry->pagesused;
            if (cnt > ppb - page) cnt = ppb - page;
            memset(&ftl_sparebuffer[0], 0xFF, 0x40 * cnt);
            for (j = 0; j < cnt; j++)
            {
                ftl_sparebuffer[j].user.lpn = sector + i + j;
                ftl_sparebuffer[j].user.usn = ++ftl_cxt.nextblockusn;
                ftl_sparebuffer[j].user.type = 0x40;
                if (logentry->pagesused + j == ppb - 1 && logentry->issequential)
                    ftl_sparebuffer[j].user.type = 0x41;
            }
            uint32_t abspage = logentry->scatteredvblock * ppb
                             + logentry->pagesused;
            logentry->pagesused += cnt;
            if (ftl_vfl_write(abspage, cnt, &((uint8_t*)buffer)[i << 11],
                              &ftl_sparebuffer[0]) == 0)
            {
                for (j = 0; j < cnt; j++)
                {
                    if (logentry->pageoffsets[page + j] == 0xFFFF)
                        logentry->pagescurrent++;
                    logentry->pageoffsets[page + j] = logentry->pagesused - cnt + j;
                    if (logentry->pagesused - cnt + j + 1 != logentry->pagescurrent
                     || logentry->pageoffsets[page + j] != page + j)
                        logentry->issequential = 0;
                }
                i += cnt;
            }
            else panicf("FTL: Write error: %u %u %u!",
                        (unsigned)sector, (unsigned)count, (unsigned)i);
        }
        if (logentry->pagesused == ppb) ftl_remove_scattered_block(logentry);
    }
    if (ftl_cxt.swapcounter >= 300)
    {
        ftl_cxt.swapcounter -= 20;
        for (i = 0; i < 4; i++) if (ftl_swap_blocks() == 0) break;
    }
    if (ftl_cxt.erasedirty == 1)
    {
        ftl_cxt.erasedirty = 0;
        for (i = 0; i < 8; i++)
            if (ftl_erasectr_dirt[i] >= 100)
            {
                ftl_next_ctrl_pool_page();
                ftl_save_erasectr_page(i);
            }
    }
    mutex_unlock(&ftl_mtx);
    return 0;
}
#endif


#ifndef FTL_READONLY
/* Exposed function: Performes a sync / unmount,
   i.e. commits all scattered page blocks,
   distributes wear, and commits the FTL context. */
uint32_t ftl_sync(void)
{
    uint32_t i;
    uint32_t rc = 0;
    if (ftl_cxt.clean_flag == 1) return 0;

    mutex_lock(&ftl_mtx);

#ifdef FTL_TRACE
    DEBUGF("FTL: Syncing\n");
#endif

    if (ftl_cxt.swapcounter >= 20)
        for (i = 0; i < 4; i++)
            if (ftl_swap_blocks() == 0)
            {
                ftl_cxt.swapcounter -= 20;
                break;
            }
    for (i = 0; i < 0x11; i++)
    {
        if (ftl_log[i].scatteredvblock == 0xFFFF) continue;
        ftl_cxt.nextblockusn++;
        if (ftl_log[i].issequential == 1)
            rc |= ftl_commit_sequential(&ftl_log[i]);
        else rc |= ftl_commit_scattered(&ftl_log[i]);
    }
    if (rc != 0)
    {
        mutex_unlock(&ftl_mtx);
        return -1;
    }
    for (i = 0; i < 5; i++)
        if (ftl_commit_cxt() == 0)
        {
            mutex_unlock(&ftl_mtx);
            return 0;
        }
        else ftl_cxt.ftlctrlpage |= ppb - 1;
    mutex_unlock(&ftl_mtx);
    return -2;
}
#endif


/* Initializes and mounts the FTL.
   As long as nothing was written, you won't need to unmount it.
   Before shutting down after writing something, call ftl_sync(),
   which will just do nothing if everything was already clean. */
uint32_t ftl_init(void)
{
    FTL_PROGRESS("FTL_SCAN_PROGRESS init start");
    FTL_PROGRESS("INIT_STARTED_CONFIRMED");
    FTL_PROGRESS("CLASSIC_6G7G_FTL_COMPARISON classic=ATA_CEATA no_nand_vfl");
    FTL_PROGRESS("NANO2G_VS_CLASSIC_MATCH_SCORE nano2g=whimory classic=ata score=0");
    FTL_PROGRESS("BEST_NANO3G_METADATA_FORMAT_GUESS pending");
    mutex_init(&ftl_mtx);
    uint32_t i;
    if (nand_device_init() != 0) //return 1;
        panicf("FTL: Lowlevel NAND driver init failed!");
    FTL_PROGRESS("ftl nand init ok");
    ftl_banks = 0;
    for (i = 0; i < 4; i++)
        if (nand_get_device_type(i) != 0) ftl_banks = i + 1;
    ftl_nand_type = nand_get_device_type(0);
    ppb = ftl_nand_type->pagesperblock * ftl_banks;
    syshyperblocks = ftl_nand_type->blocks - ftl_nand_type->userblocks - 0x17;
    FTL_PROGRESS("ftl geom banks %lu blocks %u user %u ppb %lu sys %lu",
                 (unsigned long)ftl_banks, ftl_nand_type->blocks,
                 ftl_nand_type->userblocks, (unsigned long)ppb,
                 (unsigned long)syshyperblocks);
    ftl_n3g_dump_reconstruction_pages();
#if defined(IPOD_NANO3G) && defined(BOOTLOADER) && N3G_RECON_ONLY
    FTL_PROGRESS("INIT_FAIL recon_only");
    return -1;
#endif

    if (!ftl_has_devinfo())
    {
        DEBUGF("FTL: No DEVICEINFO found!\n");
        FTL_PROGRESS("ftl init fail no devinfo");
        return -1;
    }
    FTL_PROGRESS("ftl devinfo all banks ok");
    if (ftl_vfl_open() == 0)
    {
        FTL_PROGRESS("ftl vfl ok");
        if (ftl_open() == 0)
        {
            FTL_PROGRESS("INIT_SUCCESS");
            return 0;
        }
        FTL_PROGRESS("INIT_FAIL ftl open");
    }
    else
        FTL_PROGRESS("INIT_FAIL vfl");

    DEBUGF("FTL: Initialization failed!\n");
    FTL_PROGRESS("INIT_FAIL");

    return -2;
}
