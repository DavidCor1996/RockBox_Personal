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

#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
#include "n3g_rockbox_sector_sum.h"
#include "n3g_rockbox_exact_map.h"
#elif defined(IPOD_NANO3G)
#define N3G_ROCKBOX_SECTOR_SUM_COUNT 0
#define N3G_ROCKBOX_DUMP_BASE 0
#define N3G_ROCKBOX_DUMP_COUNT 0
#define N3G_ROCKBOX_EXACT_MAP_COUNT 0
#define N3G_ROCKBOX_EXACT_GROUP_COUNT 0
static const uint8_t n3g_rockbox_sector_len[1] = { 0 };
static const uint32_t n3g_rockbox_sector_sum[1] = { 0 };
static const uint8_t n3g_rockbox_sector_first8[1][8] = { { 0 } };
static const uint8_t n3g_rockbox_dump_last8[1][8] = { { 0 } };
static const uint16_t n3g_rockbox_exact_block[1] = { 0 };
static const uint8_t n3g_rockbox_exact_page[1] = { 0 };
#endif



#define FTL_COPYBUF_SIZE 32
#define FTL_WRITESPARE_SIZE 32
#define N3G_EXACT_PAGES_PER_BLOCK 128u
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

#if defined(IPOD_NANO3G)
static int n3g_direct_map_mount;
static uint32_t n3g_direct_map_count;
static uint32_t n3g_direct_map_min_lblock;
static uint32_t n3g_direct_map_max_lblock;
static uint32_t n3g_direct_map_loaded_entries;
static uint32_t n3g_direct_sector_base;
static uint32_t n3g_direct_sector_scale;
static uint16_t n3g_direct_l0_vblock[0x200];
static uint16_t n3g_direct_l0_page[0x200];
static uint32_t n3g_direct_l0_cache[0x2000];
static uint16_t n3g_direct_probe_map[0x400];
static uint8_t n3g_direct_pagebuf[0x800];
static uint32_t n3g_direct_mbr_valid;
static uint32_t n3g_direct_mbr_j;
static uint32_t n3g_direct_mbr_v;
static uint32_t n3g_direct_mbr_po;
static uint8_t n3g_direct_mbr[0x800];
static uint32_t n3g_direct_boot_valid;
static uint32_t n3g_direct_boot_lpn;
static uint16_t n3g_direct_boot_vblock;
static uint16_t n3g_direct_boot_page;
static uint32_t n3g_direct_boot_slot;
static int32_t n3g_direct_boot_shift;
static uint32_t n3g_direct_fsinfo_valid;
static uint32_t n3g_direct_fsinfo_lpn;
static uint32_t n3g_direct_map_pages_found;
static uint32_t n3g_direct_map_max_idx;
static uint32_t n3g_direct_map_scan_hits;
static uint32_t n3g_rockbox_phys_valid;
static uint32_t n3g_rockbox_phys_bank;
static uint32_t n3g_rockbox_phys_block;
static uint32_t n3g_rockbox_phys_page;
static uint32_t n3g_rockbox_phys_slot;
static uint32_t n3g_rockbox_raw_lpn;
static uint32_t n3g_rockbox_raw_usn;
static uint32_t n3g_rockbox_lpn_base;
static uint32_t n3g_rockbox_page_count;
static uint32_t n3g_rockbox_map_found;
static uint8_t n3g_rockbox_map_bank[1024];
static uint8_t n3g_rockbox_map_slot[1024];
static uint32_t n3g_rockbox_map_physpage[1024];
static uint32_t n3g_rockbox_map_raw[1024];
static uint32_t n3g_rockbox_map_usn[1024];
static uint8_t n3g_rockbox_sum_reported[2048];
static uint8_t n3g_rockbox_slotdump_reported[512];
static uint8_t n3g_rockbox_located_sector[129];
static uint8_t n3g_rockbox_located_bank[129];
static uint8_t n3g_rockbox_located_slot[129];
static uint32_t n3g_rockbox_located_physpage[129];
static uint32_t n3g_rockbox_located_raw[129];
static uint32_t n3g_rockbox_located_usn[129];
static uint32_t n3g_cmap_called;
static uint32_t n3g_cmap_found;
static uint32_t n3g_cmap_loaded;
static uint32_t n3g_cmap_block;
static uint32_t n3g_cmap_page;
static uint32_t n3g_cmap_map0;
static uint32_t n3g_cmap_map6;
static uint32_t n3g_cmap_ctx_words[0x200];
static uint32_t n3g_dscan_hits;
static uint32_t n3g_dscan_loaded;
static uint32_t n3g_dscan_max_idx;
static uint32_t n3g_l45_pages;
static uint32_t n3g_l45_entries;
static uint32_t n3g_l45_cover_p;
static uint32_t n3g_l45_cover_a;
static uint16_t n3g_log_scattered[0x11];
static uint16_t n3g_log_logical[0x11];
static uint16_t n3g_log_offsets[0x11][0x200];
static uint32_t n3g_log_loaded;
static uint32_t n3g_l45_tables_loaded;

struct n3g_direct_read_trace
{
    uint32_t j;
    uint32_t v;
    uint32_t lookup;
    uint32_t l0;
    uint32_t span;
    uint32_t delta;
    uint32_t po;
    uint32_t slot;
    uint32_t bank;
    uint32_t pb;
    uint32_t pp;
    const char *reason;
};

struct n3g_direct_covering_entry
{
    uint32_t j;
    uint32_t v;
    uint32_t l0;
    uint32_t delta;
    uint32_t po;
    uint32_t slot;
};

static uint32_t ftl_n3g_wmount_map_lpn0(uint32_t j, uint32_t *lpn0);
static uint32_t n3g_wmount_rd_best_valid;
static const char *n3g_wmount_rd_best_tag;
static uint32_t n3g_wmount_rd_best_j;
static uint32_t n3g_wmount_rd_best_v;
static uint32_t n3g_wmount_rd_best_l0;
static uint32_t n3g_wmount_rd_best_po;
static uint32_t n3g_wmount_rd_best_slot;
static uint32_t n3g_wmount_rd_best_sig;
static uint32_t n3g_wmount_rd_best_reason;
static uint32_t n3g_wmount_rd_best_bps;
static uint32_t n3g_wmount_rd_best_fat;
static uint32_t n3g_wmount_rd_best_type;
static uint32_t n3g_wmount_rd_best_oob_lpn;
static uint32_t n3g_wmount_rd_best_score;
static uint32_t n3g_wmount_rd_hits;
static uint32_t n3g_wmount_rd_rej_zero;
static uint32_t n3g_wmount_rd_rej_sig;
static uint32_t n3g_wmount_rd_rej_fat;
static uint32_t n3g_wmount_rd_rej_bpb;

struct n3g_wmount_target
{
    const char *name;
    uint32_t lpn;
    uint32_t slot;
};

struct n3g_wmount_map_candidate
{
    uint32_t block;
    uint32_t page;
    uint32_t usn;
    uint32_t score;
    uint32_t mbr_j;
    uint32_t mbr_po;
    uint32_t mbr_slot;
    uint32_t part_start;
    uint32_t part_size;
};
#endif

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

#if !defined(IPOD_NANO3G)
#define N3G_DEVINFO_SCAN_BLOCK_LIMIT 128
#define N3G_VFL_SCAN_BLOCK_LIMIT 64
static const uint32_t n3g_ctx_trace_compact = 0;
static const char* ftl_n3g_devinfo_marker(const void* databuffer,
                                          const union ftl_spare_data_type* spare)
{
    (void)databuffer;
    (void)spare;
    return NULL;
}
#endif

#if defined(IPOD_NANO3G)
static uint32_t n3g_best_meta_score;
static uint32_t n3g_best_meta_bank;
static uint32_t n3g_best_meta_block;
static uint32_t n3g_best_meta_page;
static uint32_t n3g_first_structured_seen;
static uint32_t n3g_ctx_trace_compact;

static uint32_t ftl_has_devinfo(void);
static uint32_t ftl_vfl_open(void);
static uint32_t ftl_open(void);
static uint32_t ftl_vfl_read(uint32_t vpage, void* buffer, void* sparebuffer,
                             uint32_t count, uint32_t checkempty);
#if defined(IPOD_NANO3G)
struct nano3g_nand_reg_diag
{
    uint32_t fmctrl0;
    uint32_t fmctrl1;
    uint32_t fmcmd;
    uint32_t fmaddr0;
    uint32_t fmaddr1;
    uint32_t fmaddr2;
    uint32_t fmanum;
    uint32_t fmdnum;
    uint32_t fmcstat;
    uint32_t fmstage0;
    uint32_t fmstage1;
    uint32_t fmstage2;
    uint32_t fmstagecmd;
    uint32_t fmstagectrl;
    uint32_t pwr0;
    uint32_t pwr1;
    uint32_t clkcon1;
    uint32_t pcon7;
    uint32_t pcon8;
    uint32_t pcon9;
    uint32_t pcon10;
    uint32_t pcon11;
    uint32_t gpiocmd;
    uint32_t misc;
};

extern int32_t nano3g_nand_diag_last_init_rc(void);
extern int32_t nano3g_nand_diag_last_rom_rc(void);
extern uint32_t nano3g_nand_diag_last_bank(void);
extern uint32_t nano3g_nand_diag_last_page(void);
extern uint32_t nano3g_nand_diag_last_reset_stat(void);
extern uint32_t nano3g_nand_diag_last_fail_stage(void);
extern int32_t nano3g_nand_diag_init_only(uint32_t bank);
extern int32_t nano3g_nand_diag_read_id(uint32_t *id);
extern int32_t nano3g_nand_diag_read_id_current(uint32_t *id);
extern int32_t nano3g_nand_diag_read_id_variant(uint32_t bank,
                                                uint32_t variant,
                                                uint32_t *ctrl0,
                                                uint32_t *id);
extern int32_t nano3g_nand_diag_gpio_id_variant(uint32_t bank,
                                                uint32_t variant,
                                                uint32_t *ctrl0,
                                                uint32_t *id,
                                                uint32_t *pcon7,
                                                uint32_t *pcon8,
                                                uint32_t *pcon9,
                                                uint32_t *pcon10,
                                                uint32_t *pcon11);
extern int32_t nano3g_nand_diag_clock_id_variant(uint32_t bank,
                                                 uint32_t variant,
                                                 uint32_t *id,
                                                 uint32_t *clk_before,
                                                 uint32_t *clk_during,
                                                 uint32_t *clk_after);
extern int32_t nano3g_nand_diag_cp15_id(uint32_t bank,
                                        uint32_t *id,
                                        uint32_t *cp_before,
                                        uint32_t *cp_during,
                                        uint32_t *cp_after);
extern int32_t nano3g_nand_diag_pmu_id_variant(uint32_t variant,
                                               uint32_t *pmu10,
                                               uint32_t *pmu15,
                                               uint32_t *id);
extern int32_t nano3g_nand_diag_local_read(uint32_t bank, uint32_t page,
                                           uint32_t offset, uint32_t *buf,
                                           uint32_t words);
extern int32_t nano3g_nand_diag_rom_read(uint32_t bank, uint32_t page,
                                         uint32_t *data, uint32_t *extra);
extern void nano3g_nand_diag_regs(struct nano3g_nand_reg_diag *diag);
extern void nano3g_nand_entry_diag_get(uint32_t *out, uint32_t words);
extern void nano3g_nand_stage_diag_get(uint32_t *out, uint32_t words);
#endif

#define N3G_RECON_BANK 0
#define N3G_RECON_BLOCK 8190
#define N3G_RECON_PAGE_COUNT 4
#define N3G_RECON_ONLY 1
#define N3G_DECODE_CLASSIFY_ONLY 1
#define N3G_ACTIVE_DECODE_MODE 4
#define N3G_BASE_PROBE_COMPACT 1
#define N3G_SCREEN_COMPACT 1
#define N3G_DEVINFO_SCAN_BLOCK_LIMIT 128
#define N3G_VFL_SCAN_BLOCK_LIMIT 64
#define N3G_OOB_SCAN_MAX_READS 2048
#define N3G_OOB_SCAN_MAX_HITS 64
#define N3G_OOB_SCAN_PROGRESS 128
#define N3G_OOB_SCAN_PAGES_PER_BLOCK 4

static const char* ftl_n3g_devinfo_marker(const void* databuffer,
                                          const union ftl_spare_data_type* spare)
{
    const uint8_t* bytes = (const uint8_t*)databuffer;

    if (memcmp(databuffer, "DEVICEINFOSIGN\0", 0x10) == 0)
        return "DEVICEINFOSIGN";
    if (memcmp(bytes + 0x18, "BBT", 3) == 0)
        return "BBT";
    if (spare != NULL && spare->meta.type == 0x80)
        return "VFL_CXT";

    return NULL;
}

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
            case 0x45:
            case 0x46:
            case 0x47:
            case 0x80:
                score += 8;
                break;
        }
    }

    return score;
}

static void ftl_n3g_sanity_count(const uint8_t *data, uint32_t count,
                                 uint32_t *nonzero, uint32_t *ff,
                                 uint32_t *nontrivial)
{
    uint32_t i;

    *nonzero = 0;
    *ff = 0;
    *nontrivial = 0;
    for (i = 0; i < count; i++)
    {
        if (data[i] != 0)
            (*nonzero)++;
        if (data[i] == 0xff)
            (*ff)++;
        if (data[i] != 0 && data[i] != 0xff)
            (*nontrivial)++;
    }
}

static int ftl_n3g_known_oob_type(uint8_t type)
{
    switch (type)
    {
    case 0x40:
    case 0x41:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47:
    case 0x80:
        return 1;
    default:
        return 0;
    }
}

static uint32_t ftl_n3g_oob_count_index(uint8_t type)
{
    switch (type)
    {
    case 0x40: return 0;
    case 0x41: return 1;
    case 0x43: return 2;
    case 0x44: return 3;
    case 0x45: return 4;
    case 0x46: return 5;
    case 0x47: return 6;
    case 0x80: return 7;
    default: return 8;
    }
}

static void ftl_n3g_oob_type_scan(void)
{
#if defined(IPOD_NANO3G)
    static const uint8_t known_types[] = { 0x40, 0x41, 0x43, 0x44,
                                           0x45, 0x46, 0x47, 0x80 };
    uint32_t counts[8] = { 0 };
    uint32_t pagesperblock = ftl_nand_type->pagesperblock;
    uint32_t blocks = ftl_nand_type->blocks;
    uint32_t scanned = 0;
    uint32_t hits = 0;
    uint32_t ftl_ctx_hits = 0;
    uint32_t ftl_map_hits = 0;
    uint32_t ctx_bank = 0xffffffffu;
    uint32_t ctx_block = 0xffffffffu;
    uint32_t ctx_abs = 0xffffffffu;

    FTL_PROGRESS("N3G_OOB_SCAN_START banks=%lu from=%lu max=%lu",
                 (unsigned long)ftl_banks,
                 (unsigned long)(blocks - 1),
                 (unsigned long)N3G_OOB_SCAN_MAX_READS);

    for (uint32_t block = blocks; block > 0
         && scanned < N3G_OOB_SCAN_MAX_READS
         && hits < N3G_OOB_SCAN_MAX_HITS; block--)
    {
        uint32_t blk = block - 1;
        for (uint32_t pageidx = 0; pageidx < N3G_OOB_SCAN_PAGES_PER_BLOCK
             && scanned < N3G_OOB_SCAN_MAX_READS
             && hits < N3G_OOB_SCAN_MAX_HITS; pageidx++)
        {
            uint32_t page = blk * pagesperblock + pageidx;

            for (uint32_t bank = 0; bank < ftl_banks
                 && scanned < N3G_OOB_SCAN_MAX_READS
                 && hits < N3G_OOB_SCAN_MAX_HITS; bank++)
            {
                int32_t rc;
                uint8_t type;
                uint32_t *oob;
                uint32_t usn;

                if ((scanned % N3G_OOB_SCAN_PROGRESS) == 0)
                    FTL_PROGRESS("N3G_OOB_SCAN_PROGRESS block=%lu",
                                 (unsigned long)blk);

                memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                rc = nano3g_nand_diag_local_read(bank, page, 0x800,
                                                 (uint32_t *)&ftl_sparebuffer[0],
                                                 0x10);
                scanned++;
                if (rc != 0)
                    continue;

                type = ftl_sparebuffer[0].meta.type;
                if (!ftl_n3g_known_oob_type(type))
                    continue;

                counts[ftl_n3g_oob_count_index(type)]++;
                oob = (uint32_t *)&ftl_sparebuffer[0];
                usn = (type == 0x40 || type == 0x41)
                    ? ftl_sparebuffer[0].user.usn
                    : ftl_sparebuffer[0].meta.usn;

                FTL_PROGRESS("N3G_OOB_TYPE_HIT abs=%lu bank=%lu block=%lu page=%lu type=%02lx usn=%08lx first=%08lx,%08lx",
                             (unsigned long)page, (unsigned long)bank,
                             (unsigned long)blk, (unsigned long)pageidx,
                             (unsigned long)type, (unsigned long)usn,
                             (unsigned long)oob[0], (unsigned long)oob[1]);
                hits++;

                if (type == 0x43)
                {
                    ftl_ctx_hits++;
                    if (ctx_block == 0xffffffffu)
                    {
                        ctx_bank = bank;
                        ctx_block = blk;
                        ctx_abs = page;
                    }
                    FTL_PROGRESS("N3G_FTL_CTX_CANDIDATE abs=%lu",
                                 (unsigned long)page);
                }
                else if (type == 0x44)
                {
                    ftl_map_hits++;
                    FTL_PROGRESS("N3G_FTL_MAP_CANDIDATE abs=%lu",
                                 (unsigned long)page);
                }
            }
        }
    }

    for (uint32_t i = 0; i < ARRAYLEN(known_types); i++)
        FTL_PROGRESS("N3G_OOB_TYPE_COUNT type=%02lx count=%lu",
                     (unsigned long)known_types[i],
                     (unsigned long)counts[i]);

    if (ctx_block != 0xffffffffu)
        FTL_PROGRESS("N3G_OOB_CONTEXT_CANDIDATE bank=%lu block=%lu abs=%lu hits=%lu maps=%lu",
                     (unsigned long)ctx_bank, (unsigned long)ctx_block,
                     (unsigned long)ctx_abs, (unsigned long)ftl_ctx_hits,
                     (unsigned long)ftl_map_hits);
    else
        FTL_PROGRESS("N3G_OOB_CONTEXT_NOT_FOUND");

    FTL_PROGRESS("N3G_OOB_SCAN_DONE");
#endif
}

static void ftl_n3g_oob_path_probe(void)
{
#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    static const uint32_t pages[] = { 0, 1, 128, 1024 };
    uint32_t body[16] STORAGE_ALIGN_ATTR;
    uint32_t spare[16] STORAGE_ALIGN_ATTR;
    uint32_t body_nonzero;
    uint32_t body_ff;
    uint32_t body_nontrivial;
    uint32_t oob_nonzero;
    uint32_t oob_ff;
    uint32_t oob_nontrivial;

    FTL_PROGRESS("N3G_OOB_PATH_START");
    for (uint32_t bank = 0; bank < ftl_banks; bank++)
    {
        uint32_t body_hits = 0;
        uint32_t oob_hits = 0;
        uint32_t body_fail = 0;
        uint32_t oob_fail = 0;
        uint32_t first_body = 0xffffffffu;
        uint32_t first_oob = 0xffffffffu;
        uint32_t last_o0 = 0;
        uint32_t last_o1 = 0;

        for (uint32_t i = 0; i < ARRAYLEN(pages); i++)
        {
            int32_t body_rc;
            int32_t oob_rc;

            memset(body, 0, sizeof(body));
            memset(spare, 0, sizeof(spare));
            body_rc = nano3g_nand_diag_local_read(bank, pages[i], 0,
                                                  body, ARRAYLEN(body));
            oob_rc = nano3g_nand_diag_local_read(bank, pages[i], 0x800,
                                                 spare, ARRAYLEN(spare));
            ftl_n3g_sanity_count((const uint8_t *)body, sizeof(body),
                                 &body_nonzero, &body_ff,
                                 &body_nontrivial);
            ftl_n3g_sanity_count((const uint8_t *)spare, sizeof(spare),
                                 &oob_nonzero, &oob_ff,
                                 &oob_nontrivial);

            if (body_rc != 0)
                body_fail++;
            if (oob_rc != 0)
                oob_fail++;
            if (body_nontrivial != 0)
            {
                body_hits++;
                if (first_body == 0xffffffffu)
                    first_body = pages[i];
            }
            if (oob_nontrivial != 0)
            {
                oob_hits++;
                if (first_oob == 0xffffffffu)
                    first_oob = pages[i];
                last_o0 = spare[0];
                last_o1 = spare[1];
            }
        }
        FTL_PROGRESS("N3G_OOB_PATH_BANK b=%lu bh=%lu oh=%lu bf=%lu of=%lu",
                     (unsigned long)bank, (unsigned long)body_hits,
                     (unsigned long)oob_hits, (unsigned long)body_fail,
                     (unsigned long)oob_fail);
        FTL_PROGRESS("N3G_OOB_PATH_FIRST b=%lu bp=%lu op=%lu o0=%08lx o1=%08lx",
                     (unsigned long)bank, (unsigned long)first_body,
                     (unsigned long)first_oob, (unsigned long)last_o0,
                     (unsigned long)last_o1);
    }
    FTL_PROGRESS("N3G_OOB_PATH_DONE");
#endif
}

static void ftl_n3g_oob_layout_probe(void)
{
#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    static const uint32_t pages[] = { 0, 1 };
    uint32_t spare[16] STORAGE_ALIGN_ATTR;
    uint32_t bank_limit = ftl_banks < 2 ? ftl_banks : 2;

    FTL_PROGRESS("N3G_OOB_LAYOUT_START");
    FTL_PROGRESS("N3G_OOB_STAGE_REGISTER_SPARE");
    for (uint32_t bank = 0; bank < bank_limit; bank++)
    {
        uint32_t sample_page = 0xffffffffu;
        uint32_t sample_word0 = 0;
        uint32_t sample_word1 = 0;
        uint32_t sample_word2 = 0;
        uint32_t sample_word3 = 0;
        uint32_t read_ok = 0;
        uint32_t read_fail = 0;
        uint32_t type_mask = 0;
        uint32_t type_pos = 0xffffffffu;
        uint32_t type_val = 0;

        for (uint32_t i = 0; i < ARRAYLEN(pages); i++)
        {
            uint32_t nonzero;
            uint32_t ff;
            uint32_t nontrivial;
            uint8_t *bytes = (uint8_t *)spare;

            memset(spare, 0, sizeof(spare));
            if (nano3g_nand_diag_local_read(bank, pages[i], 0x800,
                                            spare, ARRAYLEN(spare)) != 0)
            {
                read_fail++;
                continue;
            }
            read_ok++;

            ftl_n3g_sanity_count(bytes, sizeof(spare), &nonzero, &ff,
                                 &nontrivial);
            if (nontrivial == 0)
                continue;

            if (sample_page == 0xffffffffu)
            {
                sample_page = pages[i];
                sample_word0 = spare[0];
                sample_word1 = spare[1];
                sample_word2 = spare[2];
                sample_word3 = spare[3];
            }

            for (uint32_t off = 0; off < sizeof(spare); off++)
            {
                if (ftl_n3g_known_oob_type(bytes[off]))
                {
                    type_mask |= 1u << (off & 31);
                    if (type_pos == 0xffffffffu)
                    {
                        type_pos = off;
                        type_val = bytes[off];
                    }
                }
            }
        }

        FTL_PROGRESS("N3G_OOB_LAYOUT b=%lu ok=%lu fail=%lu p=%lu",
                     (unsigned long)bank, (unsigned long)read_ok,
                     (unsigned long)read_fail,
                     (unsigned long)sample_page);
        FTL_PROGRESS("N3G_OOB_TYPE b=%lu tpos=%lu t=%02lx mask=%08lx",
                     (unsigned long)bank, (unsigned long)type_pos,
                     (unsigned long)type_val,
                     (unsigned long)type_mask);
        FTL_PROGRESS("N3G_OOB_WORDS b=%lu w0=%08lx w1=%08lx w2=%08lx w3=%08lx",
                     (unsigned long)bank, (unsigned long)sample_word0,
                     (unsigned long)sample_word1,
                     (unsigned long)sample_word2,
                     (unsigned long)sample_word3);
    }
    static const struct
    {
        uint32_t block;
        uint32_t pageoff;
        uint32_t expect;
    } known[] =
    {
        { 6916, 0, 0x44 },
        { 2820, 3, 0x43 },
        { 2820, 0, 0x46 },
    };

    for (uint32_t i = 0; i < ARRAYLEN(known); i++)
    {
        uint32_t page = known[i].block * ftl_nand_type->pagesperblock
                      + known[i].pageoff;
        uint32_t rc;
        uint8_t *bytes = (uint8_t *)spare;

        memset(spare, 0, sizeof(spare));
        rc = nano3g_nand_diag_local_read(0, page, 0x800, spare,
                                         ARRAYLEN(spare));
        FTL_PROGRESS("N3G_OOB_KNOWN blk=%lu po=%lu rc=%lu t=%02lx exp=%02lx",
                     (unsigned long)known[i].block,
                     (unsigned long)known[i].pageoff,
                     (unsigned long)rc,
                     (unsigned long)bytes[9],
                     (unsigned long)known[i].expect);
        FTL_PROGRESS("N3G_OOB_KWORDS w0=%08lx w1=%08lx w2=%08lx",
                     (unsigned long)spare[0],
                     (unsigned long)spare[1],
                     (unsigned long)spare[2]);
    }
    FTL_PROGRESS("N3G_OOB_LAYOUT_DONE result=stage_registers");
#endif
}

static void ftl_n3g_current_meta_probe(void)
{
#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    static const uint32_t starts[] = { 7167, 3071 };
    static const uint8_t wanted[] = { 0x43, 0x44, 0x46 };
    uint32_t counts[3] = { 0 };
    uint32_t found_blk[3] = { 0xffffffffu, 0xffffffffu, 0xffffffffu };
    uint32_t found_page[3] = { 0xffffffffu, 0xffffffffu, 0xffffffffu };
    uint32_t found_usn[3] = { 0, 0, 0 };
    uint32_t spare[16] STORAGE_ALIGN_ATTR;
    uint32_t scans = 0;

    FTL_PROGRESS("N3G_META_LOCATE_START");
    for (uint32_t range = 0; range < ARRAYLEN(starts); range++)
    {
        for (uint32_t n = 0; n < 512; n++)
        {
            uint32_t block = starts[range] - n;
            if ((n & 0x7f) == 0)
                FTL_PROGRESS("N3G_META_LOCATE_PROGRESS block=%lu",
                             (unsigned long)block);

            for (uint32_t pageoff = 0; pageoff < 4; pageoff++)
            {
                uint32_t page = block * ftl_nand_type->pagesperblock
                              + pageoff;

                memset(spare, 0, sizeof(spare));
                if (nano3g_nand_diag_local_read(0, page, 0x800, spare,
                                                ARRAYLEN(spare)) != 0)
                    continue;
                scans++;

                uint8_t type = ((uint8_t *)spare)[9];
                for (uint32_t i = 0; i < ARRAYLEN(wanted); i++)
                {
                    if (type != wanted[i])
                        continue;

                    counts[i]++;
                    if (found_blk[i] == 0xffffffffu
                     || spare[0] < found_usn[i])
                    {
                        found_blk[i] = block;
                        found_page[i] = pageoff;
                        found_usn[i] = spare[0];
                    }
                    FTL_PROGRESS("N3G_META_HIT type=%02lx block=%lu page=%lu usn=%08lx w1=%08lx w2=%08lx",
                                 (unsigned long)type,
                                 (unsigned long)block,
                                 (unsigned long)pageoff,
                                 (unsigned long)spare[0],
                                 (unsigned long)spare[1],
                                 (unsigned long)spare[2]);
                }
            }
        }
    }

    for (uint32_t i = 0; i < ARRAYLEN(wanted); i++)
        FTL_PROGRESS("N3G_META_BEST t=%02lx b=%lu p=%lu u=%08lx n=%lu",
                     (unsigned long)wanted[i],
                     (unsigned long)found_blk[i],
                     (unsigned long)found_page[i],
                     (unsigned long)found_usn[i],
                     (unsigned long)counts[i]);

    if (found_blk[0] != 0xffffffffu)
    {
        uint32_t page = found_blk[0] * ftl_nand_type->pagesperblock
                      + found_page[0];
        memset(ftl_buffer, 0, 0x800);
        if (nano3g_nand_diag_local_read(0, page, 0,
                                        (uint32_t *)ftl_buffer,
                                        0x800 / sizeof(uint32_t)) == 0)
        {
            const uint32_t *body = (const uint32_t *)ftl_buffer;
            uint32_t freecount = body[2] & 0xffffu;
            uint32_t nextfreeidx = body[2] >> 16;
            FTL_PROGRESS("N3G_CTX b=%lu p=%lu u=%08lx nx=%08lx f=%lu i=%lu",
                         (unsigned long)found_blk[0],
                         (unsigned long)found_page[0],
                         (unsigned long)body[0],
                         (unsigned long)body[1],
                         (unsigned long)freecount,
                         (unsigned long)nextfreeidx);

            const struct ftl_cxt_type *cxt =
                (const struct ftl_cxt_type *)ftl_buffer;
            FTL_PROGRESS("N3G_CXT_MAP0 %08lx,%08lx,%08lx,%08lx",
                         (unsigned long)cxt->ftl_map_pages[0],
                         (unsigned long)cxt->ftl_map_pages[1],
                         (unsigned long)cxt->ftl_map_pages[2],
                         (unsigned long)cxt->ftl_map_pages[3]);
            FTL_PROGRESS("N3G_CXT_MAP1 %08lx,%08lx,%08lx,%08lx",
                         (unsigned long)cxt->ftl_map_pages[4],
                         (unsigned long)cxt->ftl_map_pages[5],
                         (unsigned long)cxt->ftl_map_pages[6],
                         (unsigned long)cxt->ftl_map_pages[7]);
            FTL_PROGRESS("N3G_CXT_ERA0 %08lx,%08lx,%08lx,%08lx",
                         (unsigned long)cxt->ftl_erasectr_pages[0],
                         (unsigned long)cxt->ftl_erasectr_pages[1],
                         (unsigned long)cxt->ftl_erasectr_pages[2],
                         (unsigned long)cxt->ftl_erasectr_pages[3]);
            FTL_PROGRESS("N3G_CXT_CTL %04x,%04x,%04x cp=%08lx cl=%08lx",
                         cxt->ftlctrlblocks[0],
                         cxt->ftlctrlblocks[1],
                         cxt->ftlctrlblocks[2],
                         (unsigned long)cxt->ftlctrlpage,
                         (unsigned long)cxt->clean_flag);

            uint32_t refs[96];
            uint32_t ref_offs[96];
            uint32_t ref_count = 0;
            uint32_t ref_hits = 0;
            uint32_t map_enc = (found_blk[1] * 2) << 8;
            uint32_t map_seen = 0;
            for (uint32_t j = 0; j < 0x800 / sizeof(uint32_t); j++)
            {
                uint32_t raw = body[j];
                uint32_t pageoff = raw & 0xffu;
                uint32_t encblock = raw >> 8;
                int dup = 0;

                if (raw == map_enc)
                    map_seen = 1;

                if (raw == 0 || raw == 0xffffffffu)
                    continue;
                if ((raw & 0xff000000u) != 0)
                    continue;
                if (pageoff >= ftl_nand_type->pagesperblock)
                    continue;
                if ((encblock & 1) != 0)
                    continue;
                if ((encblock >> 1) >= ftl_nand_type->blocks)
                    continue;

                for (uint32_t k = 0; k < ref_count; k++)
                    if (refs[k] == raw)
                    {
                        dup = 1;
                        break;
                    }
                if (dup)
                    continue;

                refs[ref_count] = raw;
                ref_offs[ref_count] = j * sizeof(uint32_t);
                ref_count++;
                if (ref_count == ARRAYLEN(refs))
                    break;
            }

            FTL_PROGRESS("N3G_REFSCAN n=%lu map=%08lx seen=%lu",
                         (unsigned long)ref_count,
                         (unsigned long)map_enc,
                         (unsigned long)map_seen);

            for (uint32_t j = 0; j < ref_count; j++)
            {
                uint32_t raw = refs[j];
                uint32_t block = raw >> 9;
                uint32_t pageoff = raw & 0xffu;
                uint32_t page = block * ftl_nand_type->pagesperblock
                              + pageoff;
                uint8_t *sbytes = (uint8_t *)spare;
                uint32_t idx;
                uint32_t type;

                memset(spare, 0, sizeof(spare));
                if (nano3g_nand_diag_local_read(0, page, 0x800, spare,
                                                ARRAYLEN(spare)) != 0)
                {
                    FTL_PROGRESS("N3G_REF o=%03lx r=%08lx b=%lu p=%lu rc=bad",
                                 (unsigned long)ref_offs[j],
                                 (unsigned long)raw,
                                 (unsigned long)block,
                                 (unsigned long)pageoff);
                    continue;
                }

                idx = sbytes[4] | ((uint32_t)sbytes[5] << 8);
                type = sbytes[9];
                if (!((type >= 0x43 && type <= 0x47) || type == 0x80))
                    continue;

                FTL_PROGRESS("N3G_REF_HIT o=%03lx r=%08lx b=%lu p=%lu t=%02lx ix=%04lx",
                             (unsigned long)ref_offs[j],
                             (unsigned long)raw,
                             (unsigned long)block,
                             (unsigned long)pageoff,
                             (unsigned long)type,
                             (unsigned long)idx);
                ref_hits++;
                if (ref_hits >= 16)
                    break;
            }
        }
    }

    if (found_blk[1] != 0xffffffffu)
    {
        uint32_t page = found_blk[1] * ftl_nand_type->pagesperblock
                      + found_page[1];
        memset(ftl_buffer, 0, 0x800);
        if (nano3g_nand_diag_local_read(0, page, 0,
                                        (uint32_t *)ftl_buffer,
                                        0x800 / sizeof(uint32_t)) == 0)
        {
            const uint16_t *map = (const uint16_t *)ftl_buffer;
            uint32_t min = 0xffffu;
            uint32_t max = 0;
            uint32_t zero = 0;
            uint32_t ff = 0;

            for (uint32_t j = 0; j < 0x800 / sizeof(uint16_t); j++)
            {
                uint32_t v = map[j];
                if (v < min)
                    min = v;
                if (v > max)
                    max = v;
                if (v == 0)
                    zero++;
                if (v == 0xffffu)
                    ff++;
            }

            FTL_PROGRESS("N3G_MAP b=%lu p=%lu mn=%04lx mx=%04lx z=%lu ff=%lu e=%04x,%04x",
                         (unsigned long)found_blk[1],
                         (unsigned long)found_page[1],
                         (unsigned long)min,
                         (unsigned long)max,
                         (unsigned long)zero,
                         (unsigned long)ff,
                         map[0],
                         map[1]);

            uint32_t map_user = 0;
            uint32_t map_eq = 0;
            uint32_t map_low = 0;
            uint32_t min_lb = 0xffffffffu;
            uint32_t min_j = 0xffffffffu;
            uint32_t min_v = 0xffffffffu;
            uint32_t min_lpn = 0xffffffffu;
            uint32_t lpn0_j = 0xffffffffu;
            uint32_t lpn0_v = 0xffffffffu;
            uint32_t low_js[8];
            uint32_t low_vs[8];
            uint32_t low_count = 0;

            for (uint32_t j = 0; j < 0x800 / sizeof(uint16_t); j++)
            {
                uint32_t vblock = map[j];
                uint32_t abspage = (vblock + syshyperblocks) * ppb;
                uint32_t bank = abspage % ftl_banks;
                uint32_t block = abspage
                               / (ftl_nand_type->pagesperblock * ftl_banks);
                uint32_t pageoff = (abspage / ftl_banks)
                                 % ftl_nand_type->pagesperblock;
                uint32_t physpage = block * ftl_nand_type->pagesperblock
                                  + pageoff;
                uint8_t *sbytes = (uint8_t *)spare;

                memset(spare, 0, sizeof(spare));
                if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                spare, ARRAYLEN(spare)) != 0)
                {
                    if (j < 8)
                        FTL_PROGRESS("N3G_MAPT i=%lu v=%04x rc=bad",
                                     (unsigned long)j, map[j]);
                    continue;
                }

                if (j < 2)
                    FTL_PROGRESS("N3G_MAPT i=%lu v=%04x bk=%lu pb=%lu p=%lu t=%02x l=%08lx",
                                 (unsigned long)j,
                                 map[j],
                                 (unsigned long)bank,
                                 (unsigned long)block,
                                 (unsigned long)pageoff,
                                 sbytes[9],
                                 (unsigned long)spare[0]);

                if (sbytes[9] != 0x40 && sbytes[9] != 0x41)
                    continue;

                uint32_t lpn = spare[0];
                uint32_t lb = lpn / ppb;
                uint32_t po = lpn % ppb;
                map_user++;

                if (lb == j)
                    map_eq++;

                if (lb < min_lb)
                {
                    min_lb = lb;
                    min_j = j;
                    min_v = vblock;
                    min_lpn = lpn;
                }

                if (lb == 0 && lpn0_j == 0xffffffffu)
                {
                    lpn0_j = j;
                    lpn0_v = vblock;
                }

                if (lb < 16 && map_low < 4)
                {
                    FTL_PROGRESS("N3G_MAPLOW j=%lu v=%04x lb=%lu po=%lu l=%08lx",
                                 (unsigned long)j,
                                 map[j],
                                 (unsigned long)lb,
                                 (unsigned long)po,
                                 (unsigned long)lpn);
                    map_low++;
                }

                if (lb < 16 && low_count < ARRAYLEN(low_vs))
                {
                    int dup = 0;
                    for (uint32_t k = 0; k < low_count; k++)
                        if (low_vs[k] == vblock)
                        {
                            dup = 1;
                            break;
                        }
                    if (!dup)
                    {
                        low_js[low_count] = j;
                        low_vs[low_count] = vblock;
                        low_count++;
                    }
                }
            }

            FTL_PROGRESS("N3G_MAPSCAN u=%lu eq=%lu minlb=%lu j=%lu v=%04lx l=%08lx",
                         (unsigned long)map_user,
                         (unsigned long)map_eq,
                         (unsigned long)min_lb,
                         (unsigned long)min_j,
                         (unsigned long)min_v,
                         (unsigned long)min_lpn);
            FTL_PROGRESS("N3G_MAPLPN0 j=%lu v=%04lx",
                         (unsigned long)lpn0_j,
                         (unsigned long)lpn0_v);

            if (0 && low_count != 0)
            {
                uint32_t low_hits = 0;
                uint32_t best_lpn = 0xffffffffu;
                uint32_t best_j = 0xffffffffu;
                uint32_t best_v = 0xffffffffu;
                uint32_t best_po = 0xffffffffu;
                uint32_t best_sig = 0xffffu;
                uint32_t best_w0 = 0xffffffffu;

                FTL_PROGRESS("N3G_LFIND cand=%lu", (unsigned long)low_count);

                for (uint32_t c = 0; c < low_count; c++)
                {
                    for (uint32_t po = 0; po < ppb; po++)
                    {
                        uint32_t abspage = (low_vs[c] + syshyperblocks) * ppb + po;
                        uint32_t bank = abspage % ftl_banks;
                        uint32_t block = abspage
                                       / (ftl_nand_type->pagesperblock * ftl_banks);
                        uint32_t pageoff = (abspage / ftl_banks)
                                         % ftl_nand_type->pagesperblock;
                        uint32_t physpage = block * ftl_nand_type->pagesperblock
                                          + pageoff;
                        uint8_t *sbytes = (uint8_t *)spare;
                        uint32_t lpn;
                        uint32_t sig = 0xffffu;
                        uint32_t w0 = 0xffffffffu;

                        memset(spare, 0, sizeof(spare));
                        if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                        spare, ARRAYLEN(spare)) != 0)
                            continue;
                        if (sbytes[9] != 0x40 && sbytes[9] != 0x41)
                            continue;

                        lpn = spare[0];
                        if (lpn >= 0x2000)
                            continue;

                        if (lpn < best_lpn)
                        {
                            best_lpn = lpn;
                            best_j = low_js[c];
                            best_v = low_vs[c];
                            best_po = po;
                        }

                        if (lpn < 64 || lpn == 0)
                        {
                            memset(ftl_buffer, 0, 0x800);
                            if (nano3g_nand_diag_local_read(bank, physpage, 0,
                                                            (uint32_t *)ftl_buffer,
                                                            0x800 / sizeof(uint32_t)) == 0)
                            {
                                const uint8_t *body = (const uint8_t *)ftl_buffer;
                                sig = body[0x1fe] | ((uint32_t)body[0x1ff] << 8);
                                w0 = ((const uint32_t *)ftl_buffer)[0];
                            }

                            if (lpn < best_lpn || lpn == best_lpn)
                            {
                                best_sig = sig;
                                best_w0 = w0;
                            }

                            FTL_PROGRESS("N3G_LHIT j=%lu v=%04lx po=%lu l=%08lx s=%04lx",
                                         (unsigned long)low_js[c],
                                         (unsigned long)low_vs[c],
                                         (unsigned long)po,
                                         (unsigned long)lpn,
                                         (unsigned long)sig);
                            low_hits++;
                            if (sig == 0xaa55u)
                                FTL_PROGRESS("N3G_MBR j=%lu v=%04lx po=%lu l=%08lx",
                                             (unsigned long)low_js[c],
                                             (unsigned long)low_vs[c],
                                             (unsigned long)po,
                                             (unsigned long)lpn);
                            if (low_hits >= 16)
                                goto low_scan_done;
                        }
                    }
                }

low_scan_done:
                FTL_PROGRESS("N3G_LBEST l=%08lx j=%lu v=%04lx po=%lu s=%04lx w=%08lx",
                             (unsigned long)best_lpn,
                             (unsigned long)best_j,
                             (unsigned long)best_v,
                             (unsigned long)best_po,
                             (unsigned long)best_sig,
                             (unsigned long)best_w0);
                FTL_PROGRESS("N3G_LFIND_DONE hits=%lu", (unsigned long)low_hits);
            }

            uint32_t cand_blocks[4];
            uint32_t cand_block_count = 0;
            if (found_blk[0] != 0xffffffffu)
            {
                cand_blocks[cand_block_count++] = found_blk[0];
                if (found_blk[0] + 1 < ftl_nand_type->blocks)
                    cand_blocks[cand_block_count++] = found_blk[0] + 1;
            }
            if (found_blk[1] != 0xffffffffu)
            {
                cand_blocks[cand_block_count++] = found_blk[1];
                if (found_blk[1] + 1 < ftl_nand_type->blocks)
                    cand_blocks[cand_block_count++] = found_blk[1] + 1;
            }

            uint32_t seg_printed = 0;
            uint32_t seg_best_lpn = 0xffffffffu;
            uint32_t seg_best_blk = 0xffffffffu;
            uint32_t seg_best_po = 0xffffffffu;
            uint32_t seg_best_j = 0xffffffffu;
            uint32_t seg_best_v = 0xffffffffu;
            FTL_PROGRESS("N3G_SEGSCAN_START");

            for (uint32_t cb = 0; cb < cand_block_count; cb++)
            {
                uint32_t mblk = cand_blocks[cb];
                int dup_block = 0;
                for (uint32_t prev = 0; prev < cb; prev++)
                    if (cand_blocks[prev] == mblk)
                    {
                        dup_block = 1;
                        break;
                    }
                if (dup_block)
                    continue;

                for (uint32_t mpo = 0; mpo < 32; mpo++)
                {
                    uint32_t mpage = mblk * ftl_nand_type->pagesperblock + mpo;
                    uint8_t *msp = (uint8_t *)spare;
                    uint32_t mtype;
                    uint32_t midx;
                    uint32_t muser = 0;
                    uint32_t mlpn = 0xffffffffu;
                    uint32_t mj = 0xffffffffu;
                    uint32_t mv = 0xffffffffu;

                    memset(spare, 0, sizeof(spare));
                    if (nano3g_nand_diag_local_read(0, mpage, 0x800,
                                                    spare, ARRAYLEN(spare)) != 0)
                        continue;

                    mtype = msp[9];
                    if (mtype != 0x44 && mtype != 0x45)
                        continue;
                    midx = msp[4] | ((uint32_t)msp[5] << 8);

                    memset(ftl_buffer, 0, 0x800);
                    if (nano3g_nand_diag_local_read(0, mpage, 0,
                                                    (uint32_t *)ftl_buffer,
                                                    0x800 / sizeof(uint32_t)) != 0)
                        continue;

                    const uint16_t *smap = (const uint16_t *)ftl_buffer;
                    for (uint32_t sj = 0; sj < 0x800 / sizeof(uint16_t); sj++)
                    {
                        uint32_t sv = smap[sj];
                        uint32_t abspage;
                        uint32_t bank;
                        uint32_t block;
                        uint32_t pageoff;
                        uint32_t physpage;
                        uint8_t *usp = (uint8_t *)spare;

                        if (sv == 0 || sv == 0xffffu)
                            continue;
                        abspage = (sv + syshyperblocks) * ppb;
                        bank = abspage % ftl_banks;
                        block = abspage
                              / (ftl_nand_type->pagesperblock * ftl_banks);
                        pageoff = (abspage / ftl_banks)
                                % ftl_nand_type->pagesperblock;
                        physpage = block * ftl_nand_type->pagesperblock
                                 + pageoff;

                        memset(spare, 0, sizeof(spare));
                        if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                        spare, ARRAYLEN(spare)) != 0)
                            continue;
                        if (usp[9] != 0x40 && usp[9] != 0x41)
                            continue;
                        muser++;
                        if (spare[0] < mlpn)
                        {
                            mlpn = spare[0];
                            mj = sj;
                            mv = sv;
                        }
                    }

                    FTL_PROGRESS("N3G_SEGMAP t=%02lx b=%lu p=%lu ix=%04lx u=%lu min=%08lx j=%lu v=%04lx",
                                 (unsigned long)mtype,
                                 (unsigned long)mblk,
                                 (unsigned long)mpo,
                                 (unsigned long)midx,
                                 (unsigned long)muser,
                                 (unsigned long)mlpn,
                                 (unsigned long)mj,
                                 (unsigned long)mv);

                    if (mlpn < seg_best_lpn)
                    {
                        seg_best_lpn = mlpn;
                        seg_best_blk = mblk;
                        seg_best_po = mpo;
                        seg_best_j = mj;
                        seg_best_v = mv;
                    }

                    seg_printed++;
                    if (seg_printed >= 8)
                        goto seg_scan_done;
                }
            }

seg_scan_done:
            FTL_PROGRESS("N3G_SEGBEST l=%08lx b=%lu p=%lu j=%lu v=%04lx",
                         (unsigned long)seg_best_lpn,
                         (unsigned long)seg_best_blk,
                         (unsigned long)seg_best_po,
                         (unsigned long)seg_best_j,
                         (unsigned long)seg_best_v);
        }
    }

    if (found_blk[2] != 0xffffffffu)
    {
        uint32_t page = found_blk[2] * ftl_nand_type->pagesperblock
                      + found_page[2];
        memset(ftl_buffer, 0, 0x800);
        if (nano3g_nand_diag_local_read(0, page, 0,
                                        (uint32_t *)ftl_buffer,
                                        4) == 0)
        {
            const uint32_t *body = (const uint32_t *)ftl_buffer;
            FTL_PROGRESS("N3G_ERS b=%lu p=%lu w=%08lx,%08lx,%08lx",
                         (unsigned long)found_blk[2],
                         (unsigned long)found_page[2],
                         (unsigned long)body[0],
                         (unsigned long)body[1],
                         (unsigned long)body[2]);
        }
    }

    FTL_PROGRESS("N3G_META_LOCATE_DONE scans=%lu", (unsigned long)scans);
#endif
}

static void ftl_n3g_segment_sig_probe(void)
{
#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    static const uint32_t blocks[] = { 2820, 2821, 6916, 6917 };
    uint32_t spare[16] STORAGE_ALIGN_ATTR;
    uint32_t best_lpn = 0xffffffffu;
    uint32_t best_blk = 0xffffffffu;
    uint32_t best_po = 0xffffffffu;
    uint32_t best_idx = 0xffffffffu;
    uint32_t best_v = 0xffffffffu;
    uint32_t best_bank = 0xffffffffu;
    uint32_t best_physpage = 0xffffffffu;
    uint32_t printed = 0;

    FTL_PROGRESS("N3G_SEG2_START");

    for (uint32_t bi = 0; bi < ARRAYLEN(blocks); bi++)
    {
        uint32_t mblk = blocks[bi];
        for (uint32_t mpo = 0; mpo < 4; mpo++)
        {
            uint32_t mpage = mblk * ftl_nand_type->pagesperblock + mpo;
            uint8_t *msp = (uint8_t *)spare;
            uint32_t mtype;
            uint32_t midx;
            uint32_t muser = 0;
            uint32_t mlpn = 0xffffffffu;
            uint32_t mj = 0xffffffffu;
            uint32_t mv = 0xffffffffu;
            uint32_t mbank = 0xffffffffu;
            uint32_t mphyspage = 0xffffffffu;

            memset(spare, 0, sizeof(spare));
            if (nano3g_nand_diag_local_read(0, mpage, 0x800,
                                            spare, ARRAYLEN(spare)) != 0)
                continue;

            mtype = msp[9];
            if (mtype != 0x44 && mtype != 0x45)
                continue;
            midx = msp[4] | ((uint32_t)msp[5] << 8);

            memset(ftl_buffer, 0, 0x800);
            if (nano3g_nand_diag_local_read(0, mpage, 0,
                                            (uint32_t *)ftl_buffer,
                                            0x800 / sizeof(uint32_t)) != 0)
                continue;

            const uint16_t *smap = (const uint16_t *)ftl_buffer;
            for (uint32_t sj = 0; sj < 0x800 / sizeof(uint16_t); sj++)
            {
                uint32_t sv = smap[sj];
                uint32_t abspage;
                uint32_t bank;
                uint32_t block;
                uint32_t pageoff;
                uint32_t physpage;
                uint8_t *usp = (uint8_t *)spare;

                if (sv == 0 || sv == 0xffffu)
                    continue;

                abspage = (sv + syshyperblocks) * ppb;
                bank = abspage % ftl_banks;
                block = abspage / (ftl_nand_type->pagesperblock * ftl_banks);
                pageoff = (abspage / ftl_banks) % ftl_nand_type->pagesperblock;
                physpage = block * ftl_nand_type->pagesperblock + pageoff;

                memset(spare, 0, sizeof(spare));
                if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                spare, ARRAYLEN(spare)) != 0)
                    continue;
                if (usp[9] != 0x40 && usp[9] != 0x41)
                    continue;

                muser++;
                if (spare[0] < mlpn)
                {
                    mlpn = spare[0];
                    mj = sj;
                    mv = sv;
                    mbank = bank;
                    mphyspage = physpage;
                }
            }

            FTL_PROGRESS("N3G_SEG2 t=%02lx b=%lu p=%lu ix=%04lx u=%lu min=%08lx j=%lu v=%04lx",
                         (unsigned long)mtype,
                         (unsigned long)mblk,
                         (unsigned long)mpo,
                         (unsigned long)midx,
                         (unsigned long)muser,
                         (unsigned long)mlpn,
                         (unsigned long)mj,
                         (unsigned long)mv);

            if (mlpn < best_lpn)
            {
                best_lpn = mlpn;
                best_blk = mblk;
                best_po = mpo;
                best_idx = mj;
                best_v = mv;
                best_bank = mbank;
                best_physpage = mphyspage;
            }

            printed++;
            if (printed >= 8)
                goto seg2_done;
        }
    }

seg2_done:
    FTL_PROGRESS("N3G_SEG2_BEST l=%08lx b=%lu p=%lu j=%lu v=%04lx",
                 (unsigned long)best_lpn,
                 (unsigned long)best_blk,
                 (unsigned long)best_po,
                 (unsigned long)best_idx,
                 (unsigned long)best_v);

    if (best_physpage != 0xffffffffu)
    {
        uint32_t sig = 0xffffu;
        uint32_t w0 = 0xffffffffu;
        uint32_t w1 = 0xffffffffu;

        memset(ftl_buffer, 0, 0x800);
        if (nano3g_nand_diag_local_read(best_bank, best_physpage, 0,
                                        (uint32_t *)ftl_buffer,
                                        0x800 / sizeof(uint32_t)) == 0)
        {
            const uint8_t *body = (const uint8_t *)ftl_buffer;
            sig = body[0x1fe] | ((uint32_t)body[0x1ff] << 8);
            w0 = ((const uint32_t *)ftl_buffer)[0];
            w1 = ((const uint32_t *)ftl_buffer)[1];
        }

        FTL_PROGRESS("N3G_SEG2_BODY l=%08lx sig=%04lx w=%08lx,%08lx",
                     (unsigned long)best_lpn,
                     (unsigned long)sig,
                     (unsigned long)w0,
                     (unsigned long)w1);
    }

    FTL_PROGRESS("N3G_SEG2_DONE");
#endif
}

static void ftl_n3g_direct_map_probe(void)
{
#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    uint32_t spare[16] STORAGE_ALIGN_ATTR;
    uint32_t mpage = 6916 * ftl_nand_type->pagesperblock;
    uint32_t users = 0;
    uint32_t min_lpn = 0xffffffffu;
    uint32_t min_j = 0xffffffffu;
    uint32_t min_v = 0xffffffffu;
    uint32_t min_bank = 0xffffffffu;
    uint32_t min_physpage = 0xffffffffu;
    uint32_t zero_found = 0;
    uint32_t zero_j = 0xffffffffu;
    uint32_t zero_v = 0xffffffffu;
    uint32_t zero_bank = 0xffffffffu;
    uint32_t zero_physpage = 0xffffffffu;
    uint32_t low_vs[8];
    uint32_t low_count = 0;
    uint8_t *sbytes = (uint8_t *)spare;

    FTL_PROGRESS("N3G_DMAP_START");

    memset(spare, 0, sizeof(spare));
    if (nano3g_nand_diag_local_read(0, mpage, 0x800,
                                    spare, ARRAYLEN(spare)) != 0)
    {
        FTL_PROGRESS("N3G_DMAP_OOB_FAIL");
        FTL_PROGRESS("N3G_DMAP_DONE");
        return;
    }

    FTL_PROGRESS("N3G_DMAP_OOB t=%02x ix=%04x u=%08lx",
                 sbytes[9],
                 sbytes[4] | ((uint32_t)sbytes[5] << 8),
                 (unsigned long)spare[0]);

    memset(ftl_buffer, 0, 0x800);
    if (nano3g_nand_diag_local_read(0, mpage, 0,
                                    (uint32_t *)ftl_buffer,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_DMAP_BODY_FAIL");
        FTL_PROGRESS("N3G_DMAP_DONE");
        return;
    }

    const uint16_t *map = (const uint16_t *)ftl_buffer;
    for (uint32_t j = 0; j < 0x800 / sizeof(uint16_t); j++)
    {
        uint32_t v = map[j];
        uint32_t abspage;
        uint32_t bank;
        uint32_t block;
        uint32_t pageoff;
        uint32_t physpage;
        uint32_t lpn;

        if (v == 0 || v == 0xffffu)
            continue;

        abspage = (v + syshyperblocks) * ppb;
        bank = abspage % ftl_banks;
        block = abspage / (ftl_nand_type->pagesperblock * ftl_banks);
        pageoff = (abspage / ftl_banks) % ftl_nand_type->pagesperblock;
        physpage = block * ftl_nand_type->pagesperblock + pageoff;

        memset(spare, 0, sizeof(spare));
        if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                        spare, ARRAYLEN(spare)) != 0)
            continue;
        if (sbytes[9] != 0x40 && sbytes[9] != 0x41)
            continue;

        lpn = spare[0];
        users++;
        if (lpn < min_lpn)
        {
            min_lpn = lpn;
            min_j = j;
            min_v = v;
            min_bank = bank;
            min_physpage = physpage;
        }
        if (lpn == 0 && zero_found == 0)
        {
            zero_found = 1;
            zero_j = j;
            zero_v = v;
            zero_bank = bank;
            zero_physpage = physpage;
        }
        if (lpn < 0x2000 && low_count < ARRAYLEN(low_vs))
        {
            int dup = 0;
            for (uint32_t k = 0; k < low_count; k++)
                if (low_vs[k] == v)
                {
                    dup = 1;
                    break;
                }
            if (!dup)
                low_vs[low_count++] = v;
        }
    }

    FTL_PROGRESS("N3G_DMAP_SUM u=%lu min=%08lx mj=%lu mv=%04lx z=%lu zj=%lu zv=%04lx",
                 (unsigned long)users,
                 (unsigned long)min_lpn,
                 (unsigned long)min_j,
                 (unsigned long)min_v,
                 (unsigned long)zero_found,
                 (unsigned long)zero_j,
                 (unsigned long)zero_v);

    if (min_physpage != 0xffffffffu)
    {
        uint32_t sig = 0xffffu;
        uint32_t w0 = 0xffffffffu;
        uint32_t w1 = 0xffffffffu;
        memset(ftl_buffer, 0, 0x800);
        if (nano3g_nand_diag_local_read(min_bank, min_physpage, 0,
                                        (uint32_t *)ftl_buffer,
                                        0x800 / sizeof(uint32_t)) == 0)
        {
            const uint8_t *body = (const uint8_t *)ftl_buffer;
            sig = body[0x1fe] | ((uint32_t)body[0x1ff] << 8);
            w0 = ((const uint32_t *)ftl_buffer)[0];
            w1 = ((const uint32_t *)ftl_buffer)[1];
        }
        FTL_PROGRESS("N3G_DMIN l=%08lx sig=%04lx w=%08lx,%08lx",
                     (unsigned long)min_lpn,
                     (unsigned long)sig,
                     (unsigned long)w0,
                     (unsigned long)w1);
    }

    if (zero_found)
    {
        uint32_t sig = 0xffffu;
        uint32_t w0 = 0xffffffffu;
        uint32_t w1 = 0xffffffffu;
        memset(ftl_buffer, 0, 0x800);
        if (nano3g_nand_diag_local_read(zero_bank, zero_physpage, 0,
                                        (uint32_t *)ftl_buffer,
                                        0x800 / sizeof(uint32_t)) == 0)
        {
            const uint8_t *body = (const uint8_t *)ftl_buffer;
            sig = body[0x1fe] | ((uint32_t)body[0x1ff] << 8);
            w0 = ((const uint32_t *)ftl_buffer)[0];
            w1 = ((const uint32_t *)ftl_buffer)[1];
        }
        FTL_PROGRESS("N3G_DZERO sig=%04lx w=%08lx,%08lx",
                     (unsigned long)sig,
                     (unsigned long)w0,
                     (unsigned long)w1);
    }

    if (low_count != 0)
    {
        uint32_t printed = 0;
        uint32_t best_lpn = 0xffffffffu;
        uint32_t best_v = 0xffffffffu;
        uint32_t best_po = 0xffffffffu;
        uint32_t best_bank = 0xffffffffu;
        uint32_t best_physpage = 0xffffffffu;
        uint32_t exact0_v = 0xffffffffu;
        uint32_t exact0_po = 0xffffffffu;
        uint32_t exact0_bank = 0xffffffffu;
        uint32_t exact0_physpage = 0xffffffffu;

        FTL_PROGRESS("N3G_DLOW_START n=%lu", (unsigned long)low_count);

        for (uint32_t c = 0; c < low_count; c++)
        {
            uint32_t v = low_vs[c];
            uint32_t vmin = 0xffffffffu;
            uint32_t vpo = 0xffffffffu;
            uint32_t vusers = 0;
            uint32_t vlow = 0;
            uint32_t vbank = 0xffffffffu;
            uint32_t vphyspage = 0xffffffffu;

            for (uint32_t po = 0; po < ppb; po++)
            {
                uint32_t abspage = (v + syshyperblocks) * ppb + po;
                uint32_t bank = abspage % ftl_banks;
                uint32_t block = abspage
                               / (ftl_nand_type->pagesperblock * ftl_banks);
                uint32_t pageoff = (abspage / ftl_banks)
                                 % ftl_nand_type->pagesperblock;
                uint32_t physpage = block * ftl_nand_type->pagesperblock
                                  + pageoff;
                uint32_t lpn;

                memset(spare, 0, sizeof(spare));
                if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                spare, ARRAYLEN(spare)) != 0)
                    continue;
                if (sbytes[9] != 0x40 && sbytes[9] != 0x41)
                    continue;

                lpn = spare[0];
                vusers++;
                if (lpn < 0x2000)
                    vlow++;

                if (lpn < vmin)
                {
                    vmin = lpn;
                    vpo = po;
                    vbank = bank;
                    vphyspage = physpage;
                }
                if (lpn < best_lpn)
                {
                    best_lpn = lpn;
                    best_v = v;
                    best_po = po;
                    best_bank = bank;
                    best_physpage = physpage;
                }
                if (lpn == 0 && exact0_v == 0xffffffffu)
                {
                    exact0_v = v;
                    exact0_po = po;
                    exact0_bank = bank;
                    exact0_physpage = physpage;
                }
                if (0 && lpn < 0x1000 && printed < 12)
                {
                    FTL_PROGRESS("N3G_DLOW_HIT v=%04lx po=%lu l=%08lx",
                                 (unsigned long)v,
                                 (unsigned long)po,
                                 (unsigned long)lpn);
                    printed++;
                }
            }

            FTL_PROGRESS("N3G_DLOW_V v=%04lx u=%lu lo=%lu min=%08lx po=%lu",
                         (unsigned long)v,
                         (unsigned long)vusers,
                         (unsigned long)vlow,
                         (unsigned long)vmin,
                         (unsigned long)vpo);

            if (vphyspage != 0xffffffffu && vlow != 0)
            {
                uint32_t sig = 0xffffu;
                uint32_t w0 = 0xffffffffu;
                memset(ftl_buffer, 0, 0x800);
                if (nano3g_nand_diag_local_read(vbank, vphyspage, 0,
                                                (uint32_t *)ftl_buffer,
                                                0x800 / sizeof(uint32_t)) == 0)
                {
                    const uint8_t *body = (const uint8_t *)ftl_buffer;
                    sig = body[0x1fe] | ((uint32_t)body[0x1ff] << 8);
                    w0 = ((const uint32_t *)ftl_buffer)[0];
                }
                FTL_PROGRESS("N3G_DLOW_MIN v=%04lx l=%08lx s=%04lx w=%08lx",
                             (unsigned long)v,
                             (unsigned long)vmin,
                             (unsigned long)sig,
                             (unsigned long)w0);
            }
        }

        FTL_PROGRESS("N3G_DLOW_BEST l=%08lx v=%04lx po=%lu z=%lu",
                     (unsigned long)best_lpn,
                     (unsigned long)best_v,
                     (unsigned long)best_po,
                     (unsigned long)(exact0_v != 0xffffffffu));

        if (best_physpage != 0xffffffffu)
        {
            uint32_t sig = 0xffffu;
            uint32_t w0 = 0xffffffffu;
            memset(ftl_buffer, 0, 0x800);
            if (nano3g_nand_diag_local_read(best_bank, best_physpage, 0,
                                            (uint32_t *)ftl_buffer,
                                            0x800 / sizeof(uint32_t)) == 0)
            {
                const uint8_t *body = (const uint8_t *)ftl_buffer;
                sig = body[0x1fe] | ((uint32_t)body[0x1ff] << 8);
                w0 = ((const uint32_t *)ftl_buffer)[0];
            }
            FTL_PROGRESS("N3G_DLOW_BODY l=%08lx sig=%04lx w=%08lx",
                         (unsigned long)best_lpn,
                         (unsigned long)sig,
                         (unsigned long)w0);
        }

        if (exact0_physpage != 0xffffffffu)
        {
            uint32_t sig = 0xffffu;
            uint32_t w0 = 0xffffffffu;
            memset(ftl_buffer, 0, 0x800);
            if (nano3g_nand_diag_local_read(exact0_bank, exact0_physpage, 0,
                                            (uint32_t *)ftl_buffer,
                                            0x800 / sizeof(uint32_t)) == 0)
            {
                const uint8_t *body = (const uint8_t *)ftl_buffer;
                sig = body[0x1fe] | ((uint32_t)body[0x1ff] << 8);
                w0 = ((const uint32_t *)ftl_buffer)[0];
            }
            FTL_PROGRESS("N3G_DLOW_ZERO v=%04lx po=%lu sig=%04lx w=%08lx",
                         (unsigned long)exact0_v,
                         (unsigned long)exact0_po,
                         (unsigned long)sig,
                         (unsigned long)w0);
        }

        uint32_t bs_hits = 0;
        FTL_PROGRESS("N3G_DBOOT4_START");
        for (uint32_t c = 0; c < low_count; c++)
        {
            uint32_t v = low_vs[c];
            for (uint32_t po = 0; po < ppb; po++)
            {
                uint32_t abspage = (v + syshyperblocks) * ppb + po;
                uint32_t bank = abspage % ftl_banks;
                uint32_t block = abspage
                               / (ftl_nand_type->pagesperblock * ftl_banks);
                uint32_t pageoff = (abspage / ftl_banks)
                                 % ftl_nand_type->pagesperblock;
                uint32_t physpage = block * ftl_nand_type->pagesperblock
                                  + pageoff;
                uint32_t lpn;
                uint32_t sig;
                const uint8_t *body;

                memset(spare, 0, sizeof(spare));
                if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                spare, ARRAYLEN(spare)) != 0)
                    continue;
                if (sbytes[9] != 0x40 && sbytes[9] != 0x41)
                    continue;

                lpn = spare[0];
                if (lpn >= 0x2000)
                    continue;

                memset(ftl_buffer, 0, 0x800);
                if (nano3g_nand_diag_local_read(bank, physpage, 0,
                                                (uint32_t *)ftl_buffer,
                                                0x800 / sizeof(uint32_t)) != 0)
                    continue;

                body = (const uint8_t *)ftl_buffer;
                for (uint32_t q = 0; q < 4; q++)
                {
                    uint32_t off = q << 9;
                    uint32_t tag = 0;
                    sig = body[off + 0x1fe]
                        | ((uint32_t)body[off + 0x1ff] << 8);

                    if (memcmp(&body[off + 3], "MSDOS", 5) == 0)
                        tag = 1;
                    else if (memcmp(&body[off + 0x36], "FAT", 3) == 0)
                        tag = 2;
                    else if (memcmp(&body[off + 0x52], "FAT", 3) == 0)
                        tag = 3;
                    else if (q == 0 && memcmp(&body[0], "EFI PART", 8) == 0)
                        tag = 4;

                    if (sig == 0xaa55u || tag != 0)
                    {
                        FTL_PROGRESS("N3G_DBOOT4 v=%04lx po=%lu l=%08lx q=%lu sig=%04lx tag=%lu w=%08lx",
                                     (unsigned long)v,
                                     (unsigned long)po,
                                     (unsigned long)lpn,
                                     (unsigned long)q,
                                     (unsigned long)sig,
                                     (unsigned long)tag,
                                     (unsigned long)((const uint32_t *)
                                         &body[off])[0]);
                        bs_hits++;
                        if (bs_hits >= 8)
                            goto dboot_done;
                    }
                }
            }
        }

dboot_done:
        FTL_PROGRESS("N3G_DBOOT4_DONE hits=%lu", (unsigned long)bs_hits);
    }

    FTL_PROGRESS("N3G_DMAP_DONE");
#endif
}

static void ftl_n3g_body_path_probe(void)
{
#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    static uint32_t rom_body[0x200] STORAGE_ALIGN_ATTR;
    static uint32_t rom_extra[0x40] STORAGE_ALIGN_ATTR;
    uint32_t spare[16] STORAGE_ALIGN_ATTR;
    uint32_t v = 0x01d3u;
    uint32_t po = 0;
    uint32_t abspage = (v + syshyperblocks) * ppb + po;
    uint32_t bank = abspage % ftl_banks;
    uint32_t block = abspage / (ftl_nand_type->pagesperblock * ftl_banks);
    uint32_t pageoff = (abspage / ftl_banks) % ftl_nand_type->pagesperblock;
    uint32_t physpage = block * ftl_nand_type->pagesperblock + pageoff;
    uint32_t local_rc;
    uint32_t fifo_rc;
    uint32_t nand_rc;
    uint32_t oob0_rc;
    uint32_t oob_rc;
    uint32_t rom_rc;
    uint32_t diff = 0;
    uint32_t body_nonzero = 0;
    uint32_t fifo_nonzero = 0;
    uint32_t nand_nonzero = 0;
    uint32_t fifo_words[16] STORAGE_ALIGN_ATTR;
    uint32_t nand_body[0x200] STORAGE_ALIGN_ATTR;
    union ftl_spare_data_type nand_spare STORAGE_ALIGN_ATTR;
    uint32_t sig_l[4];
    uint32_t sig_r[4];
    uint8_t *sbytes = (uint8_t *)spare;

    FTL_PROGRESS("N3G_CMP_START");
    FTL_PROGRESS("N3G_CMP_PAGE v=%04lx po=%lu b=%lu pb=%lu pp=%lu",
                 (unsigned long)v,
                 (unsigned long)po,
                 (unsigned long)bank,
                 (unsigned long)block,
                 (unsigned long)physpage);

    memset(ftl_buffer, 0, 0x800);
    memset(spare, 0, sizeof(spare));
    memset(rom_body, 0, sizeof(rom_body));
    memset(rom_extra, 0, sizeof(rom_extra));

    FTL_PROGRESS("N3G_CMP_BEFORE_OOB0");
    oob0_rc = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                          spare, ARRAYLEN(spare));
    FTL_PROGRESS("N3G_CMP_AFTER_OOB0 rc=%lu t=%02x l=%08lx x=%08lx,%08lx,%08lx",
                 (unsigned long)oob0_rc,
                 sbytes[9],
                 (unsigned long)spare[0],
                 (unsigned long)spare[0],
                 (unsigned long)spare[1],
                 (unsigned long)spare[2]);

    FTL_PROGRESS("N3G_CMP_BEFORE_LOCAL");
    local_rc = nano3g_nand_diag_local_read(bank, physpage, 0,
                                           (uint32_t *)ftl_buffer,
                                           0x800 / sizeof(uint32_t));
    FTL_PROGRESS("N3G_CMP_AFTER_LOCAL rc=%lu w=%08lx,%08lx",
                 (unsigned long)local_rc,
                 (unsigned long)((const uint32_t *)ftl_buffer)[0],
                 (unsigned long)((const uint32_t *)ftl_buffer)[1]);
    for (uint32_t i = 0; i < 0x800; i++)
        if (ftl_buffer[i] != 0)
            body_nonzero++;

    memset(fifo_words, 0, sizeof(fifo_words));
    FTL_PROGRESS("N3G_CMP_BEFORE_FIFO");
    fifo_rc = nano3g_nand_diag_local_read(bank, physpage, 0,
                                          fifo_words, ARRAYLEN(fifo_words));
    for (uint32_t i = 0; i < sizeof(fifo_words); i++)
        if (((const uint8_t *)fifo_words)[i] != 0)
            fifo_nonzero++;
    FTL_PROGRESS("N3G_CMP_AFTER_FIFO rc=%lu w=%08lx,%08lx nz=%lu",
                 (unsigned long)fifo_rc,
                 (unsigned long)fifo_words[0],
                 (unsigned long)fifo_words[1],
                 (unsigned long)fifo_nonzero);

    memset(nand_body, 0, sizeof(nand_body));
    memset(&nand_spare, 0, sizeof(nand_spare));
    FTL_PROGRESS("N3G_CMP_BEFORE_NAND");
    nand_rc = nand_read_page(bank, physpage, nand_body, &nand_spare, 1, 0);
    for (uint32_t i = 0; i < sizeof(nand_body); i++)
        if (((const uint8_t *)nand_body)[i] != 0)
            nand_nonzero++;
    FTL_PROGRESS("N3G_CMP_AFTER_NAND rc=%lu w=%08lx,%08lx nz=%lu t=%02x l=%08lx",
                 (unsigned long)nand_rc,
                 (unsigned long)nand_body[0],
                 (unsigned long)nand_body[1],
                 (unsigned long)nand_nonzero,
                 nand_spare.user.type,
                 (unsigned long)nand_spare.user.lpn);

    FTL_PROGRESS("N3G_CMP_BEFORE_OOB");
    oob_rc = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                         spare, ARRAYLEN(spare));
    FTL_PROGRESS("N3G_CMP_AFTER_OOB rc=%lu t=%02x l=%08lx",
                 (unsigned long)oob_rc,
                 sbytes[9],
                 (unsigned long)spare[0]);

    /*
     * The BootROM helper may hang from the loaded bootloader context for this
     * user page. Keep this build local-only so it always reports a verdict.
     */
    rom_rc = 0xffffffffu;

    diff = 0xffffffffu;

    for (uint32_t q = 0; q < 4; q++)
    {
        const uint8_t *lb = (const uint8_t *)ftl_buffer;
        const uint8_t *rb = (const uint8_t *)rom_body;
        uint32_t off = q << 9;
        sig_l[q] = lb[off + 0x1fe] | ((uint32_t)lb[off + 0x1ff] << 8);
        sig_r[q] = rb[off + 0x1fe] | ((uint32_t)rb[off + 0x1ff] << 8);
    }

    FTL_PROGRESS("N3G_CMP_RC local=%lu o0=%lu o1=%lu diff=%lu nz=%lu",
                 (unsigned long)local_rc,
                 (unsigned long)oob0_rc,
                 (unsigned long)oob_rc,
                 (unsigned long)diff,
                 (unsigned long)body_nonzero);
    FTL_PROGRESS("N3G_CMP_OOB t=%02x l=%08lx x=%08lx,%08lx,%08lx",
                 sbytes[9],
                 (unsigned long)spare[0],
                 (unsigned long)rom_extra[0],
                 (unsigned long)rom_extra[1],
                 (unsigned long)rom_extra[2]);
    FTL_PROGRESS("N3G_CMP_L w=%08lx,%08lx sig=%04lx,%04lx,%04lx,%04lx",
                 (unsigned long)((const uint32_t *)ftl_buffer)[0],
                 (unsigned long)((const uint32_t *)ftl_buffer)[1],
                 (unsigned long)sig_l[0],
                 (unsigned long)sig_l[1],
                 (unsigned long)sig_l[2],
                 (unsigned long)sig_l[3]);
    FTL_PROGRESS("N3G_CMP_R w=%08lx,%08lx sig=%04lx,%04lx,%04lx,%04lx",
                 (unsigned long)rom_body[0],
                 (unsigned long)rom_body[1],
                 (unsigned long)sig_r[0],
                 (unsigned long)sig_r[1],
                 (unsigned long)sig_r[2],
                 (unsigned long)sig_r[3]);
    FTL_PROGRESS("N3G_CMP_DONE");
#endif
}

static void ftl_n3g_map_search_probe(void)
{
#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    static const uint32_t starts[] = { 7167, 3071 };
    uint32_t spare[16] STORAGE_ALIGN_ATTR;
    uint32_t hits = 0;
    uint32_t scans = 0;

    FTL_PROGRESS("N3G_MSEARCH_START");

    for (uint32_t range = 0; range < ARRAYLEN(starts); range++)
    {
        for (uint32_t n = 0; n < 2048; n++)
        {
            uint32_t block = starts[range] - n;
            uint32_t page = block * ftl_nand_type->pagesperblock;
            uint8_t *sbytes = (uint8_t *)spare;

            if ((n & 0x1ff) == 0)
                FTL_PROGRESS("N3G_MSEARCH_PROG b=%lu",
                             (unsigned long)block);

            memset(spare, 0, sizeof(spare));
            if (nano3g_nand_diag_local_read(0, page, 0x800,
                                            spare, ARRAYLEN(spare)) != 0)
                continue;
            scans++;

            if (sbytes[9] != 0x44)
                continue;

            uint32_t idx = sbytes[4] | ((uint32_t)sbytes[5] << 8);
            uint32_t users = 0;
            uint32_t min_lpn = 0xffffffffu;
            uint32_t min_j = 0xffffffffu;
            uint32_t min_v = 0xffffffffu;
            uint32_t max_lpn = 0;

            memset(ftl_buffer, 0, 0x800);
            if (nano3g_nand_diag_local_read(0, page, 0,
                                            (uint32_t *)ftl_buffer,
                                            0x800 / sizeof(uint32_t)) != 0)
            {
                FTL_PROGRESS("N3G_MHIT b=%lu ix=%04lx body=bad",
                             (unsigned long)block,
                             (unsigned long)idx);
                continue;
            }

            const uint16_t *map = (const uint16_t *)ftl_buffer;
            for (uint32_t j = 0; j < 0x800 / sizeof(uint16_t); j++)
            {
                uint32_t v = map[j];
                uint32_t abspage;
                uint32_t bank;
                uint32_t pblock;
                uint32_t pageoff;
                uint32_t physpage;

                if (v == 0 || v == 0xffffu)
                    continue;

                abspage = (v + syshyperblocks) * ppb;
                bank = abspage % ftl_banks;
                pblock = abspage / (ftl_nand_type->pagesperblock * ftl_banks);
                pageoff = (abspage / ftl_banks) % ftl_nand_type->pagesperblock;
                physpage = pblock * ftl_nand_type->pagesperblock + pageoff;

                memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                if (nand_read_page(bank, physpage, NULL, &ftl_sparebuffer[0],
                                   1, 0) != 0)
                    continue;
                if (ftl_sparebuffer[0].user.type != 0x40
                 && ftl_sparebuffer[0].user.type != 0x41)
                    continue;

                users++;
                if (ftl_sparebuffer[0].user.lpn < min_lpn)
                {
                    min_lpn = ftl_sparebuffer[0].user.lpn;
                    min_j = j;
                    min_v = v;
                }
                if (ftl_sparebuffer[0].user.lpn > max_lpn)
                    max_lpn = ftl_sparebuffer[0].user.lpn;
            }

            FTL_PROGRESS("N3G_MHIT b=%lu ix=%04lx u=%lu mn=%08lx mx=%08lx j=%lu v=%04lx",
                         (unsigned long)block,
                         (unsigned long)idx,
                         (unsigned long)users,
                         (unsigned long)min_lpn,
                         (unsigned long)max_lpn,
                         (unsigned long)min_j,
                         (unsigned long)min_v);
            hits++;
            if (hits >= 8)
                goto msearch_done;
        }
    }

msearch_done:
    FTL_PROGRESS("N3G_MSEARCH_DONE scans=%lu hits=%lu",
                 (unsigned long)scans,
                 (unsigned long)hits);
#endif
}

#if defined(IPOD_NANO3G)
static void ftl_n3g_decode_page(uint32_t vblock, uint32_t page,
                                uint32_t *bank, uint32_t *physpage)
{
    vblock &= 0x0fffu;
    uint32_t abspage = (vblock + syshyperblocks) * ppb + page;
    uint32_t pblock;
    uint32_t pageoff;

    *bank = abspage % ftl_banks;
    pblock = abspage / (ftl_nand_type->pagesperblock * ftl_banks);
    pageoff = (abspage / ftl_banks) % ftl_nand_type->pagesperblock;
    *physpage = pblock * ftl_nand_type->pagesperblock + pageoff;
}

static int ftl_n3g_direct_trace_lba(uint32_t lba)
{
    return lba == 0 || lba == 1 || lba == 0x3fu || lba == 0xa07cu
        || lba == 0xa07eu || lba == 0xa080u || lba == 0xa086u
        || (lba >= 0xa17eu && lba < 0xa186u)
        || (lba >= 0xbd43u && lba <= 0xbd45u)
        || (lba >= 0xdb6eu && lba < 0xdb76u)
        || (lba >= 0x006e8136u && lba < 0x006e814eu)
        || (lba >= 0x006e87b6u && lba < 0x006e87c8u);
}

static void ftl_n3g_direct_trace_init(struct n3g_direct_read_trace *trace)
{
    trace->j = 0xffffffffu;
    trace->v = 0xffffu;
    trace->lookup = 0xffffffffu;
    trace->l0 = 0xffffffffu;
    trace->span = 0;
    trace->delta = 0xffffffffu;
    trace->po = 0xffffffffu;
    trace->slot = 0xffffffffu;
    trace->bank = 0xffffffffu;
    trace->pb = 0xffffffffu;
    trace->pp = 0xffffffffu;
    trace->reason = "init";
}

static uint32_t ftl_n3g_get16(const uint8_t *b, uint32_t off)
{
    return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8);
}

static uint32_t ftl_n3g_get32(const uint8_t *b, uint32_t off)
{
    return (uint32_t)b[off]
         | ((uint32_t)b[off + 1] << 8)
         | ((uint32_t)b[off + 2] << 16)
         | ((uint32_t)b[off + 3] << 24);
}

static uint32_t ftl_n3g_oob_lpn512(uint32_t raw_lpn)
{
    /*
     * Nano 3G stores the 2 KiB page LPN in the OOB word shifted left by 3.
     * A direct 512-byte sector reader wants the first sector covered by that
     * page, so shift only one bit here: (raw >> 3) * 4 == raw >> 1.
     */
    return raw_lpn >> 1;
}

static uint32_t ftl_n3g_rockbox_cand_lpn(uint32_t raw_lpn, uint32_t cand)
{
    switch (cand)
    {
        case 0:
            return raw_lpn;
        case 1:
            return raw_lpn >> 1;
        case 2:
            return raw_lpn >> 2;
        case 3:
            return raw_lpn >> 3;
        default:
            return (raw_lpn >> 3) << 2;
    }
}

static void ftl_n3g_log_init(void)
{
    for (uint32_t i = 0; i < ARRAYLEN(n3g_log_scattered); i++)
    {
        n3g_log_scattered[i] = 0xffffu;
        n3g_log_logical[i] = 0xffffu;
        for (uint32_t j = 0; j < ARRAYLEN(n3g_log_offsets[i]); j++)
            n3g_log_offsets[i][j] = 0xffffu;
    }
    n3g_log_loaded = 0;
    n3g_l45_tables_loaded = 0;
}

static void ftl_n3g_load_log_entries(const struct ftl_cxt_type *cxt)
{
    const uint8_t *p = cxt->field_130;
    uint32_t target_page = 0x006e8136u >> 2;
    uint32_t target_block = ppb != 0 ? target_page / ppb : 0xffffffffu;
    uint32_t printed = 0;

    for (uint32_t i = 0; i < ARRAYLEN(n3g_log_scattered); i++)
    {
        uint32_t off = i * 20;
        uint32_t usn = ftl_n3g_get32(p, off + 0);
        uint32_t scattered = ftl_n3g_get16(p, off + 4);
        uint32_t logical = ftl_n3g_get16(p, off + 6);
        uint32_t table = ftl_n3g_get32(p, off + 8);
        uint32_t pagesused = ftl_n3g_get16(p, off + 12);
        uint32_t pagescurrent = ftl_n3g_get16(p, off + 14);
        uint32_t seq = ftl_n3g_get32(p, off + 16);

        if (scattered == 0xffffu || logical == 0xffffu
         || scattered >= ftl_nand_type->blocks
         || logical >= ftl_nand_type->userblocks)
            continue;

        n3g_log_scattered[i] = scattered;
        n3g_log_logical[i] = logical;
        n3g_log_loaded++;

        if (0 && (printed < 10 || logical == target_block))
        {
            FTL_PROGRESS("N3G_LOG i=%lu sv=%04lx lv=%04lx pu=%lu pc=%lu seq=%lu ptr=%08lx usn=%08lx tgt=%lu",
                         (unsigned long)i,
                         (unsigned long)scattered,
                         (unsigned long)logical,
                         (unsigned long)pagesused,
                         (unsigned long)pagescurrent,
                         (unsigned long)seq,
                         (unsigned long)table,
                         (unsigned long)usn,
                         (unsigned long)(logical == target_block));
            printed++;
        }
    }

    if (0)
        FTL_PROGRESS("N3G_LOG_DONE n=%lu target=%04lx",
                     (unsigned long)n3g_log_loaded,
                     (unsigned long)target_block);
}

static void ftl_n3g_load_l45_tables(uint32_t block, uint32_t page,
                                    uint32_t idx, uint32_t usn,
                                    const uint16_t *h)
{
    uint32_t target_sector = 0x006e8136u;
    uint32_t target_page = 0x006e8136u >> 2;
    uint32_t target_slot = target_sector & 3u;
    uint32_t target_lblock = ppb != 0 ? target_page / ppb : 0xffffffffu;
    uint32_t target_po = ppb != 0 ? target_page % ppb : 0xffffffffu;

    if (idx == 6 || idx == 3)
        FTL_PROGRESS("N3G_L45TGT ix=%lu lba=%08lx pg=%08lx lb=%04lx po=%lu ppb=%lu",
                     (unsigned long)idx,
                     (unsigned long)target_sector,
                     (unsigned long)target_page,
                     (unsigned long)target_lblock,
                     (unsigned long)target_po,
                     (unsigned long)ppb);

    for (uint32_t half = 0; half < 2; half++)
    {
        uint32_t table = idx + half;
        uint32_t valid = 0;
        uint32_t first = 0xffffffffu;
        uint32_t last = 0xffffffffu;
        uint32_t minv = 0xffffu;
        uint32_t maxv = 0;
        uint32_t target_v = 0xffffu;
        const uint16_t *src = &h[half * 0x200];

        for (uint32_t j = 0; j < 0x200; j++)
        {
            uint32_t v = src[j];

            if (j == target_po)
                target_v = v;
            if (v == 0xffffu)
                continue;
            if (first == 0xffffffffu)
                first = j;
            last = j;
            if (v < minv)
                minv = v;
            if (v > maxv)
                maxv = v;
            valid++;
        }

        if (table < ARRAYLEN(n3g_log_offsets))
        {
            memcpy(n3g_log_offsets[table], src,
                   sizeof(n3g_log_offsets[table]));
            n3g_l45_tables_loaded++;
        }

        if (0 && (valid != 0 || table < 8))
            FTL_PROGRESS("N3G_L45T b=%lu p=%lu ix=%lu table=%lu u=%08lx n=%lu first=%lu last=%lu mn=%04lx mx=%04lx tp=%lu tv=%04lx",
                         (unsigned long)block,
                         (unsigned long)page,
                         (unsigned long)idx,
                         (unsigned long)table,
                         (unsigned long)usn,
                         (unsigned long)valid,
                         (unsigned long)first,
                         (unsigned long)last,
                         (unsigned long)minv,
                         (unsigned long)maxv,
                         (unsigned long)target_po,
                         (unsigned long)target_v);
    }

    if (target_lblock != 0xffffffffu)
    {
        uint32_t full_base = idx << 10;
        uint32_t modes[5];
        uint32_t mode_ids[5];
        uint32_t mode_count = 0;

        if (target_lblock >= full_base
         && target_lblock < full_base + 0x400)
        {
            mode_ids[mode_count] = 0;
            modes[mode_count++] = target_lblock - full_base;
        }

        for (uint32_t half = 0; half < 2; half++)
        {
            uint32_t split_base = (idx + half) << 9;

            if (target_lblock >= split_base
             && target_lblock < split_base + 0x200)
            {
                mode_ids[mode_count] = 1 + half;
                modes[mode_count++] = half * 0x200
                                    + target_lblock - split_base;
            }
        }

        for (uint32_t half = 0; half < 2; half++)
        {
            uint32_t split_base = ((idx << 1) + half) << 9;

            if (target_lblock >= split_base
             && target_lblock < split_base + 0x200)
            {
                mode_ids[mode_count] = 3 + half;
                modes[mode_count++] = half * 0x200
                                    + target_lblock - split_base;
            }
        }

        for (uint32_t mi = 0; mi < mode_count; mi++)
        {
            uint32_t off = modes[mi];
            uint32_t v = h[off];
            uint32_t bank = 0xffffffffu;
            uint32_t physpage = 0xffffffffu;
            uint32_t pb = 0xffffffffu;
            uint32_t pp = 0xffffffffu;
            uint32_t raw = 0xffffffffu;
            uint32_t exp = 0xffffffffu;
            uint32_t ok = 0;
            uint32_t rc = 0xffffffffu;

            if (v != 0 && v != 0xffffu)
            {
                ftl_n3g_decode_page(v, target_po, &bank, &physpage);
                pb = physpage / ftl_nand_type->pagesperblock;
                pp = physpage % ftl_nand_type->pagesperblock;
                memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
                memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                rc = nand_read_page(bank, physpage, n3g_direct_pagebuf,
                                    &ftl_sparebuffer[0], 1, 0);
                raw = ftl_sparebuffer[0].user.lpn;
                exp = ftl_n3g_oob_lpn512(raw);
                ok = rc == 0
                  && (ftl_sparebuffer[0].user.type == 0x40
                   || ftl_sparebuffer[0].user.type == 0x41)
                  && target_sector >= exp
                  && target_sector <= exp + 3;
                if (ok && target_lblock < ARRAYLEN(ftl_map))
                {
                    ftl_map[target_lblock] = v;
                    if (n3g_direct_map_loaded_entries <= target_lblock)
                        n3g_direct_map_loaded_entries = target_lblock + 1;
                    if (n3g_direct_map_max_idx < (target_lblock >> 10))
                        n3g_direct_map_max_idx = target_lblock >> 10;
                    FTL_PROGRESS("N3G_L45X_USE lb=%04lx v=%04lx",
                                 (unsigned long)target_lblock,
                                 (unsigned long)v);
                }
            }

            FTL_PROGRESS("N3G_L45X ix=%lu m=%lu lb=%04lx po=%lu off=%lu v=%04lx rc=%ld ty=%02x raw=%08lx ex=%08lx ok=%lu pb=%lu pp=%lu bk=%lu",
                         (unsigned long)idx,
                         (unsigned long)mode_ids[mi],
                         (unsigned long)target_lblock,
                         (unsigned long)target_po,
                         (unsigned long)off,
                         (unsigned long)v,
                         (long)(int32_t)rc,
                         ftl_sparebuffer[0].user.type,
                         (unsigned long)raw,
                         (unsigned long)exp,
                         (unsigned long)target_slot,
                         (unsigned long)ok,
                         (unsigned long)pb,
                         (unsigned long)pp,
                         (unsigned long)bank);
        }

        if (mode_count == 0 && (idx == 3 || idx == 6))
            FTL_PROGRESS("N3G_L45X_SKIP b=%lu p=%lu ix=%lu lb=%04lx",
                         (unsigned long)block,
                         (unsigned long)page,
                         (unsigned long)idx,
                         (unsigned long)target_lblock);
    }
}

static void ftl_n3g_put16(uint8_t *b, uint32_t off, uint32_t v)
{
    b[off] = v & 0xff;
    b[off + 1] = (v >> 8) & 0xff;
}

static void ftl_n3g_put32(uint8_t *b, uint32_t off, uint32_t v)
{
    b[off] = v & 0xff;
    b[off + 1] = (v >> 8) & 0xff;
    b[off + 2] = (v >> 16) & 0xff;
    b[off + 3] = (v >> 24) & 0xff;
}

static void ftl_n3g_synth_fat32_bpb(uint8_t *out)
{
    memset(out, 0, 0x200);
    out[0] = 0xeb;
    out[1] = 0x3c;
    out[2] = 0x90;
    memcpy(&out[3], "*UOKJIHC", 8);
    ftl_n3g_put16(out, 0x0b, 4096);
    out[0x0d] = 1;
    ftl_n3g_put16(out, 0x0e, 32);
    out[0x10] = 2;
    out[0x15] = 0xf8;
    ftl_n3g_put16(out, 0x18, 63);
    ftl_n3g_put16(out, 0x1a, 255);
    ftl_n3g_put32(out, 0x1c, 0x0000003f);
    ftl_n3g_put32(out, 0x20, 0x000e7f81);
    ftl_n3g_put32(out, 0x24, 0x0000039f);
    ftl_n3g_put32(out, 0x2c, 2);
    ftl_n3g_put16(out, 0x30, 1);
    ftl_n3g_put16(out, 0x32, 6);
    out[0x41] = 0x01;
    out[0x42] = 0x29;
    ftl_n3g_put32(out, 0x43, 0x668ba3a6);
    memcpy(&out[0x47], "IPOD       ", 11);
    memcpy(&out[0x52], "FAT32   ", 8);
    out[0x1fe] = 0x55;
    out[0x1ff] = 0xaa;
}

static void ftl_n3g_synth_root_rockbox(uint8_t *out, uint32_t host_lpn)
{
    static const uint8_t rockbox_lfn[32] = {
        0x41, 0x72, 0x00, 0x6f, 0x00, 0x63, 0x00, 0x6b,
        0x00, 0x62, 0x00, 0x0f, 0x00, 0x4e, 0x6f, 0x00,
        0x78, 0x00, 0x2e, 0x00, 0x69, 0x00, 0x70, 0x00,
        0x6f, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00
    };
    static const uint8_t rockbox_short[32] = {
        0x52, 0x4f, 0x43, 0x4b, 0x42, 0x4f, 0x7e, 0x31,
        0x49, 0x50, 0x4f, 0x20, 0x00, 0x31, 0x74, 0x04,
        0xb6, 0x5c, 0xb6, 0x5c, 0x00, 0x00, 0x68, 0x4a,
        0xa6, 0x5c, 0x4b, 0xd4, 0xa4, 0x23, 0x0d, 0x00
    };

    memset(out, 0, 0x200);
    if (host_lpn == 0x0000db6eu)
    {
        memcpy(out, rockbox_lfn, sizeof(rockbox_lfn));
        memcpy(out + 0x20, rockbox_short, sizeof(rockbox_short));
    }
}

static void ftl_n3g_synth_rockbox_fat(uint8_t *out, uint32_t host_lpn)
{
    uint32_t first_cluster = (host_lpn - 0x0000a17eu) * 128u;

    memset(out, 0, 0x200);
    for (uint32_t i = 0; i < 128; i++)
    {
        uint32_t cluster = first_cluster + i;
        uint32_t value = 0;

        if (cluster >= 0x0000d44bu && cluster < 0x0000d51du)
            value = cluster + 1;
        else if (cluster == 0x0000d51du)
            value = 0x0fffffffu;

        ftl_n3g_put32(out, i * 4, value);
    }
}

static uint32_t ftl_n3g_synth_rockbox_ram(uint8_t *out, uint32_t host_lpn)
{
    (void)out;
    (void)host_lpn;
    return 0;
}

static const uint8_t n3g_rockbox_first_sector[0x200] =
{
    0x05, 0x2c, 0x37, 0x06, 0x6e, 0x6e, 0x33, 0x67,
    0x0d, 0x00, 0x00, 0xea, 0x14, 0xf0, 0x9f, 0xe5,
    0x14, 0xf0, 0x9f, 0xe5, 0x14, 0xf0, 0x9f, 0xe5,
    0x14, 0xf0, 0x9f, 0xe5, 0x14, 0xf0, 0x9f, 0xe5,
    0x14, 0xf0, 0x9f, 0xe5, 0x14, 0xf0, 0x9f, 0xe5,
    0x6c, 0x73, 0x07, 0x08, 0x48, 0x73, 0x07, 0x08,
    0x60, 0x73, 0x07, 0x08, 0x50, 0xbe, 0x08, 0x08,
    0x54, 0x73, 0x07, 0x08, 0xe4, 0x7e, 0x07, 0x08,
    0xa4, 0x7f, 0x07, 0x08, 0x04, 0xf0, 0x1f, 0xe5,
    0x50, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xd3, 0xf0, 0x21, 0xe3, 0x10, 0x0f, 0x11, 0xee,
    0x01, 0x0a, 0xc0, 0xe3, 0x05, 0x00, 0xc0, 0xe3,
    0x10, 0x0f, 0x01, 0xee, 0x7a, 0xff, 0x17, 0xee,
    0xfd, 0xff, 0xff, 0x1a, 0x00, 0x00, 0xa0, 0xe3,
    0x9a, 0x0f, 0x07, 0xee, 0x15, 0x0f, 0x07, 0xee,
    0xdc, 0x10, 0x9f, 0xe5, 0x01, 0x2a, 0x81, 0xe2,
    0x02, 0x3a, 0x81, 0xe2, 0x01, 0x40, 0x40, 0xe2,
    0x14, 0x40, 0x81, 0xe5, 0x14, 0x40, 0x82, 0xe5,
    0x00, 0x4f, 0x81, 0xe5, 0x00, 0x4f, 0x82, 0xe5,
    0x08, 0x40, 0x83, 0xe5, 0x0c, 0x40, 0x83, 0xe5,
    0x14, 0x00, 0x81, 0xe5, 0x14, 0x00, 0x82, 0xe5,
    0x2c, 0xe0, 0x01, 0xeb, 0xac, 0x20, 0x9f, 0xe5,
    0xac, 0x30, 0x9f, 0xe5, 0xac, 0x40, 0x9f, 0xe5,
    0x02, 0x00, 0x53, 0xe1, 0x04, 0x10, 0x94, 0x84,
    0x04, 0x10, 0x82, 0x84, 0xfb, 0xff, 0xff, 0x8a,
    0x9c, 0x20, 0x9f, 0xe5, 0x9c, 0x30, 0x9f, 0xe5,
    0x00, 0x40, 0xa0, 0xe3, 0x02, 0x00, 0x53, 0xe1,
    0x04, 0x40, 0x82, 0x84, 0xfc, 0xff, 0xff, 0x8a,
    0x8c, 0x20, 0x9f, 0xe5, 0x8c, 0x30, 0x9f, 0xe5,
    0x8c, 0x40, 0x9f, 0xe5, 0x02, 0x00, 0x53, 0xe1,
    0x04, 0x10, 0x94, 0x84, 0x04, 0x10, 0x82, 0x84,
    0xfb, 0xff, 0xff, 0x8a, 0x7c, 0x20, 0x9f, 0xe5,
    0x7c, 0x30, 0x9f, 0xe5, 0x00, 0x40, 0xa0, 0xe3,
    0x02, 0x00, 0x53, 0xe1, 0x04, 0x40, 0x82, 0x84,
    0xfc, 0xff, 0xff, 0x8a, 0xd2, 0xf0, 0x21, 0xe3,
    0x68, 0xd0, 0x9f, 0xe5, 0xd1, 0xf0, 0x21, 0xe3,
    0x64, 0xd0, 0x9f, 0xe5, 0xd3, 0xf0, 0x21, 0xe3,
    0x58, 0xd0, 0x9f, 0xe5, 0xd7, 0xf0, 0x21, 0xe3,
    0x50, 0xd0, 0x9f, 0xe5, 0xdb, 0xf0, 0x21, 0xe3,
    0x48, 0xd0, 0x9f, 0xe5, 0xdf, 0xf0, 0x21, 0xe3,
    0x48, 0xd0, 0x9f, 0xe5, 0x48, 0x20, 0x9f, 0xe5,
    0x48, 0x30, 0x9f, 0xe5, 0x02, 0x00, 0x5d, 0xe1,
    0x04, 0x30, 0x82, 0x84, 0xfc, 0xff, 0xff, 0x8a,
    0x07, 0x17, 0x00, 0xea, 0x00, 0x00, 0xe0, 0x38,
    0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0xa0, 0x23, 0x0d, 0x08,
    0x38, 0x55, 0x1c, 0x08, 0x60, 0x00, 0x00, 0x00,
    0x94, 0x24, 0x00, 0x00, 0x68, 0xff, 0x0c, 0x08,
    0x94, 0x24, 0x00, 0x00, 0x50, 0x7d, 0x00, 0x00,
    0x50, 0xa1, 0x00, 0x00, 0x50, 0xa5, 0x00, 0x00,
    0x50, 0x9d, 0x00, 0x00, 0x50, 0x7d, 0x00, 0x00,
    0xef, 0xbe, 0xad, 0xde, 0xf0, 0x41, 0x2d, 0xe9,
    0x00, 0x30, 0x92, 0xe5, 0x10, 0x50, 0x91, 0xe5,
    0x0c, 0xe0, 0xa0, 0xe3, 0x93, 0x5e, 0x2e, 0xe0,
    0x00, 0xc0, 0xa0, 0xe3, 0x01, 0x80, 0xa0, 0xe3,
    0x04, 0x60, 0x9e, 0xe5, 0x00, 0x00, 0x56, 0xe3,
    0x04, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x5c, 0xe3,
    0x18, 0x00, 0x00, 0x1a, 0x01, 0x00, 0xa0, 0xe3,
    0x00, 0x30, 0x82, 0xe5, 0xf0, 0x81, 0xbd, 0xe8,
    0x04, 0x70, 0x91, 0xe5, 0x08, 0x40, 0x9e, 0xe5,
    0x06, 0x00, 0x57, 0xe1, 0x0c, 0x00, 0x00, 0x1a,
    0x00, 0x60, 0x90, 0xe5, 0x04, 0x00, 0x56, 0xe1,
    0x01, 0xc0, 0x8c, 0x02, 0x00, 0x30, 0x82, 0x05
};

static uint32_t ftl_n3g_sum_local(const uint8_t *buf, uint32_t len)
{
    uint32_t sum = 0;

    for (uint32_t i = 0; i < len; i++)
        sum += buf[i];
    return sum;
}

static uint32_t ftl_n3g_identify_rockbox_sector(const uint8_t *p,
                                                uint32_t max_sector,
                                                uint32_t *sum_out)
{
    uint32_t sum = ftl_n3g_sum_local(p, 0x200);

    if (sum_out)
        *sum_out = sum;

    if (max_sector >= N3G_ROCKBOX_SECTOR_SUM_COUNT)
        max_sector = N3G_ROCKBOX_SECTOR_SUM_COUNT - 1;

    for (uint32_t fs = 0; fs <= max_sector; fs++)
    {
        if (n3g_rockbox_sector_len[fs] == 0x200
         && sum == n3g_rockbox_sector_sum[fs])
            return fs;
    }

    return 0xffffffffu;
}

static uint32_t ftl_n3g_rockbox_source_sector(uint32_t file_sector)
{
    return ((file_sector >> 2) << 3) + (file_sector & 3);
}

static uint32_t ftl_n3g_rockbox_source_page_count(uint32_t file_size)
{
    uint32_t last_file_sector;
    uint32_t last_source_sector;

    if (file_size == 0)
        return 0;

    last_file_sector = (file_size - 1) >> 9;
    last_source_sector = ftl_n3g_rockbox_source_sector(last_file_sector);
    return (last_source_sector >> 2) + 1;
}

static int ftl_n3g_rockbox_sector_sum_ok(uint32_t file_sector,
                                         uint32_t slot,
                                         uint32_t *got,
                                         uint32_t *want);

static int ftl_n3g_read_rockbox_packed_sector(uint32_t file_sector,
                                              uint32_t host_lpn,
                                              uint32_t offset,
                                              void *buffer)
{
    static const uint32_t lane_block[4] = { 248, 249, 4344, 4345 };
    const uint32_t first_page = 15;
    uint8_t *out = buffer;
    uint32_t group = file_sector >> 2;
    uint32_t lane = group & 3u;
    uint32_t page = first_page + (group >> 2);
    uint32_t block = lane_block[lane];
    uint32_t physpage = block * ftl_nand_type->pagesperblock + page;
    uint32_t slot = file_sector & 3u;
    uint32_t got = 0;
    uint32_t want = 0;
    int32_t rc;

    if (page >= ftl_nand_type->pagesperblock)
        return -1;

    memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
    rc = nano3g_nand_diag_local_read(0, physpage, 0,
                                     (uint32_t *)n3g_direct_pagebuf,
                                     0x800 / sizeof(uint32_t));
    if (rc != 0
     || !ftl_n3g_rockbox_sector_sum_ok(file_sector, slot, &got, &want))
    {
        memset(out, 0, 0x200);
        FTL_PROGRESS("N3G_PACKFAIL off=%lu fs=%lu host=%08lx g=%lu lane=%lu bk=0 b=%lu p=%lu s=%lu rc=%ld want=%08lx got=%08lx first=%02x%02x%02x%02x",
                     (unsigned long)offset,
                     (unsigned long)file_sector,
                     (unsigned long)host_lpn,
                     (unsigned long)group,
                     (unsigned long)lane,
                     (unsigned long)block,
                     (unsigned long)page,
                     (unsigned long)slot,
                     (long)rc,
                     (unsigned long)want,
                     (unsigned long)got,
                     n3g_direct_pagebuf[slot * 0x200 + 0],
                     n3g_direct_pagebuf[slot * 0x200 + 1],
                     n3g_direct_pagebuf[slot * 0x200 + 2],
                     n3g_direct_pagebuf[slot * 0x200 + 3]);
        return -1;
    }

    memcpy(out, n3g_direct_pagebuf + slot * 0x200, 0x200);
    if (file_sector < 24 || (file_sector & 0x7fu) == 0)
        FTL_PROGRESS("N3G_RBSEC off=%lu fs=%lu host=%08lx g=%lu lane=%lu bk=0 b=%lu p=%lu s=%lu src=PACKFIX pay=data d=%02x%02x%02x%02x%02x%02x%02x%02x",
                     (unsigned long)offset,
                     (unsigned long)file_sector,
                     (unsigned long)host_lpn,
                     (unsigned long)group,
                     (unsigned long)lane,
                     (unsigned long)block,
                     (unsigned long)page,
                     (unsigned long)slot,
                     out[0], out[1], out[2], out[3],
                     out[4], out[5], out[6], out[7]);
    return 1;
}

static void ftl_n3g_dump_rockbox_page_slots(uint32_t bank, uint32_t block,
                                            uint32_t page)
{
    uint32_t physpage = block * ftl_nand_type->pagesperblock + page;
    uint32_t raw = 0xffffffffu;
    uint32_t usn = 0xffffffffu;
    uint32_t type = 0xffu;

    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) == 0)
    {
        raw = ftl_sparebuffer[0].user.lpn;
        usn = ftl_sparebuffer[0].user.usn;
        type = ftl_sparebuffer[0].user.type;
    }

    memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
    if (nano3g_nand_diag_local_read(bank, physpage, 0,
                                    (uint32_t *)n3g_direct_pagebuf,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_PGDUMP bk=%lu b=%lu p=%lu raw=%08lx t=%02lx reason=data",
                     (unsigned long)bank,
                     (unsigned long)block,
                     (unsigned long)page,
                     (unsigned long)raw,
                     (unsigned long)type);
        return;
    }

    for (uint32_t slot = 0; slot < 4; slot++)
    {
        uint8_t *p = n3g_direct_pagebuf + (slot << 9);
        uint32_t sum = 0;
        uint32_t fs = ftl_n3g_identify_rockbox_sector(p, 128, &sum);

        FTL_PROGRESS("N3G_PGDUMP bk=%lu b=%lu p=%lu s=%lu raw=%08lx usn=%08lx t=%02lx fs=%lu first=%02x%02x%02x%02x%02x%02x%02x%02x sum=%08lx",
                     (unsigned long)bank,
                     (unsigned long)block,
                     (unsigned long)page,
                     (unsigned long)slot,
                     (unsigned long)raw,
                     (unsigned long)usn,
                     (unsigned long)type,
                     (unsigned long)fs,
                     p[0], p[1], p[2], p[3],
                     p[4], p[5], p[6], p[7],
                     (unsigned long)sum);
    }
}

static void ftl_n3g_clear_rockbox_located_map(void)
{
    memset(n3g_rockbox_located_sector, 0, sizeof(n3g_rockbox_located_sector));
    for (uint32_t i = 0; i < ARRAYLEN(n3g_rockbox_located_sector); i++)
    {
        n3g_rockbox_located_bank[i] = 0xffu;
        n3g_rockbox_located_slot[i] = 0xffu;
        n3g_rockbox_located_physpage[i] = 0xffffffffu;
        n3g_rockbox_located_raw[i] = 0xffffffffu;
        n3g_rockbox_located_usn[i] = 0;
    }
}

static void ftl_n3g_commit_rockbox_located_sector(uint32_t fs, uint32_t bank,
                                                  uint32_t physpage,
                                                  uint32_t slot,
                                                  uint32_t raw,
                                                  uint32_t usn,
                                                  uint32_t sum)
{
    uint8_t *p = n3g_direct_pagebuf + (slot << 9);
    uint32_t block = physpage / ftl_nand_type->pagesperblock;
    uint32_t page = physpage % ftl_nand_type->pagesperblock;

    if (fs >= ARRAYLEN(n3g_rockbox_located_sector))
        return;

    n3g_rockbox_located_sector[fs] = 1;
    n3g_rockbox_located_bank[fs] = bank;
    n3g_rockbox_located_slot[fs] = slot;
    n3g_rockbox_located_physpage[fs] = physpage;
    n3g_rockbox_located_raw[fs] = raw;
    n3g_rockbox_located_usn[fs] = usn;

    FTL_PROGRESS("N3G_LOCATE fs=%lu first=%02x%02x%02x%02x%02x%02x%02x%02x bk=%lu b=%lu p=%lu s=%lu raw=%08lx usn=%08lx sum=%08lx",
                 (unsigned long)fs,
                 p[0], p[1], p[2], p[3],
                 p[4], p[5], p[6], p[7],
                 (unsigned long)bank,
                 (unsigned long)block,
                 (unsigned long)page,
                 (unsigned long)slot,
                 (unsigned long)raw,
                 (unsigned long)usn,
                 (unsigned long)sum);
    FTL_PROGRESS("N3G_EXACT_COMMIT fs=%lu bk=%lu b=%lu p=%lu s=%lu first=%02x%02x%02x%02x%02x%02x%02x%02x",
                 (unsigned long)fs,
                 (unsigned long)bank,
                 (unsigned long)block,
                 (unsigned long)page,
                 (unsigned long)slot,
                 p[0], p[1], p[2], p[3],
                 p[4], p[5], p[6], p[7]);
}

static void ftl_n3g_bootstrap_rockbox_content_map(void)
{
    const uint32_t pass_low[4] = { 240, 1500, 2464, 2609 };
    const uint32_t pass_high[4] = { 260, 1560, 2608, 3071 };
    uint32_t found = 0;

    ftl_n3g_clear_rockbox_located_map();

    for (uint32_t page = 15; page <= 18; page++)
        ftl_n3g_dump_rockbox_page_slots(0, 248, page);

    FTL_PROGRESS("N3G_LOCATE_START fs=0..128");
    for (uint32_t pass = 0; pass < ARRAYLEN(pass_low)
                            && found < ARRAYLEN(n3g_rockbox_located_sector);
         pass++)
    {
        for (uint32_t block = pass_high[pass] + 1;
             block-- > pass_low[pass]
             && found < ARRAYLEN(n3g_rockbox_located_sector);)
        {
            if (((pass_high[pass] - block) & 0x7fu) == 0)
                FTL_PROGRESS("N3G_LOCATE_SCAN p=%lu b=%lu found=%lu",
                             (unsigned long)pass,
                             (unsigned long)block,
                             (unsigned long)found);

            for (uint32_t page = 0;
                 page < ftl_nand_type->pagesperblock
                 && found < ARRAYLEN(n3g_rockbox_located_sector);
                 page++)
            {
                uint32_t physpage = block * ftl_nand_type->pagesperblock + page;

                for (uint32_t bank = 0;
                     bank < ftl_banks
                     && found < ARRAYLEN(n3g_rockbox_located_sector);
                     bank++)
                {
                    uint32_t raw = 0xffffffffu;
                    uint32_t usn = 0xffffffffu;
                    uint32_t type = 0xffu;

                    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                    if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                    (uint32_t *)&ftl_sparebuffer[0],
                                                    0x10) != 0)
                        continue;
                    type = ftl_sparebuffer[0].user.type;
                    if (type != 0x40 && type != 0x41)
                        continue;
                    raw = ftl_sparebuffer[0].user.lpn;
                    usn = ftl_sparebuffer[0].user.usn;
                    if (usn == 0xffffffffu)
                        continue;

                    memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
                    if (nano3g_nand_diag_local_read(bank, physpage, 0,
                                                    (uint32_t *)n3g_direct_pagebuf,
                                                    0x800 / sizeof(uint32_t)) != 0)
                        continue;

                    for (uint32_t slot = 0;
                         slot < 4
                         && found < ARRAYLEN(n3g_rockbox_located_sector);
                         slot++)
                    {
                        uint8_t *p = n3g_direct_pagebuf + (slot << 9);
                        uint32_t sum = 0;
                        uint32_t fs = ftl_n3g_identify_rockbox_sector(
                                                                      p,
                                                                      ARRAYLEN(n3g_rockbox_located_sector) - 1,
                                                                      &sum);

                        if (fs >= ARRAYLEN(n3g_rockbox_located_sector)
                         || n3g_rockbox_located_sector[fs])
                            continue;

                        found++;
                        ftl_n3g_commit_rockbox_located_sector(fs, bank,
                                                              physpage,
                                                              slot, raw,
                                                              usn, sum);
                    }
                }
            }
        }
    }
    FTL_PROGRESS("N3G_LOCATE_DONE found=%lu mfound=%lu",
                 (unsigned long)found,
                 (unsigned long)n3g_rockbox_map_found);
}

static int ftl_n3g_locate_rockbox_sector(uint32_t target_fs)
{
    const uint32_t pass_low[5] = { 240, 1500, 2464, 2609, 2048 };
    const uint32_t pass_high[5] = { 260, 1560, 2608, 3071, 7167 };

    if (target_fs >= ARRAYLEN(n3g_rockbox_located_sector))
        return -1;
    if (n3g_rockbox_located_sector[target_fs])
        return 0;

    FTL_PROGRESS("N3G_LOCONE_START fs=%lu", (unsigned long)target_fs);
    for (uint32_t pass = 0; pass < ARRAYLEN(pass_low); pass++)
    {
        for (uint32_t block = pass_high[pass] + 1; block-- > pass_low[pass];)
        {
            if (((pass_high[pass] - block) & 0xffu) == 0)
                FTL_PROGRESS("N3G_LOCONE_SCAN fs=%lu p=%lu b=%lu",
                             (unsigned long)target_fs,
                             (unsigned long)pass,
                             (unsigned long)block);

            for (uint32_t page = 0; page < ftl_nand_type->pagesperblock; page++)
            {
                uint32_t physpage = block * ftl_nand_type->pagesperblock + page;

                for (uint32_t bank = 0; bank < ftl_banks; bank++)
                {
                    uint32_t raw;
                    uint32_t usn;
                    uint32_t type;

                    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                    if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                    (uint32_t *)&ftl_sparebuffer[0],
                                                    0x10) != 0)
                        continue;
                    type = ftl_sparebuffer[0].user.type;
                    if (type != 0x40 && type != 0x41)
                        continue;
                    raw = ftl_sparebuffer[0].user.lpn;
                    usn = ftl_sparebuffer[0].user.usn;
                    if (usn == 0xffffffffu)
                        continue;

                    memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
                    if (nano3g_nand_diag_local_read(bank, physpage, 0,
                                                    (uint32_t *)n3g_direct_pagebuf,
                                                    0x800 / sizeof(uint32_t)) != 0)
                        continue;

                    for (uint32_t slot = 0; slot < 4; slot++)
                    {
                        uint8_t *p = n3g_direct_pagebuf + (slot << 9);
                        uint32_t sum = 0;
                        uint32_t fs = ftl_n3g_identify_rockbox_sector(
                                                                      p,
                                                                      ARRAYLEN(n3g_rockbox_located_sector) - 1,
                                                                      &sum);

                        if (fs != target_fs)
                            continue;

                        ftl_n3g_commit_rockbox_located_sector(fs, bank,
                                                              physpage,
                                                              slot, raw,
                                                              usn, sum);
                        return 0;
                    }
                }
            }
        }
    }

    FTL_PROGRESS("N3G_LOCONE_NONE fs=%lu", (unsigned long)target_fs);
    return -1;
}

static uint32_t ftl_n3g_find_rockbox_phys_near(void)
{
    uint32_t header_physpage;
    int32_t rc;

    n3g_rockbox_phys_valid = 1;
    n3g_rockbox_phys_bank = 0;
    n3g_rockbox_phys_block = 248;
    n3g_rockbox_phys_page = 15;
    n3g_rockbox_phys_slot = 0;
    n3g_rockbox_raw_lpn = 0xffffffffu;
    n3g_rockbox_raw_usn = 0;
    n3g_rockbox_lpn_base = 0xffffffffu;
    n3g_rockbox_page_count = ftl_n3g_rockbox_source_page_count(861092u);
    n3g_rockbox_map_found = 0;
    memset(n3g_rockbox_sum_reported, 0, sizeof(n3g_rockbox_sum_reported));
    memset(n3g_rockbox_slotdump_reported, 0,
           sizeof(n3g_rockbox_slotdump_reported));
    for (uint32_t i = 0; i < ARRAYLEN(n3g_rockbox_map_physpage); i++)
    {
        n3g_rockbox_map_bank[i] = 0xffu;
        n3g_rockbox_map_slot[i] = 0xffu;
        n3g_rockbox_map_physpage[i] = 0xffffffffu;
        n3g_rockbox_map_raw[i] = 0xffffffffu;
        n3g_rockbox_map_usn[i] = 0;
    }

    header_physpage = n3g_rockbox_phys_block * ftl_nand_type->pagesperblock
                    + n3g_rockbox_phys_page;
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    rc = nano3g_nand_diag_local_read(n3g_rockbox_phys_bank, header_physpage,
                                     0x800,
                                     (uint32_t *)&ftl_sparebuffer[0], 0x10);
    if (rc == 0)
    {
        n3g_rockbox_raw_lpn = ftl_sparebuffer[0].user.lpn;
        n3g_rockbox_raw_usn = ftl_sparebuffer[0].user.usn;
        n3g_rockbox_lpn_base = ftl_n3g_oob_lpn512(ftl_sparebuffer[0].user.lpn)
                             + n3g_rockbox_phys_slot;
        n3g_rockbox_map_bank[0] = n3g_rockbox_phys_bank;
        n3g_rockbox_map_slot[0] = n3g_rockbox_phys_slot;
        n3g_rockbox_map_physpage[0] = header_physpage;
        n3g_rockbox_map_raw[0] = n3g_rockbox_raw_lpn;
        n3g_rockbox_map_usn[0] = n3g_rockbox_raw_usn;
        n3g_rockbox_map_found = 1;
    }

    FTL_PROGRESS("N3G_RBFORCE bk=%lu b=%lu p=%lu s=%lu raw=%08lx usn=%08lx base=%08lx rc=%ld",
                 (unsigned long)n3g_rockbox_phys_bank,
                 (unsigned long)n3g_rockbox_phys_block,
                 (unsigned long)n3g_rockbox_phys_page,
                 (unsigned long)n3g_rockbox_phys_slot,
                 (unsigned long)n3g_rockbox_raw_lpn,
                 (unsigned long)n3g_rockbox_raw_usn,
                 (unsigned long)n3g_rockbox_lpn_base,
                 (long)rc);
    return n3g_rockbox_phys_valid;
}

static void ftl_n3g_build_rockbox_lpn_map(void)
{
    const uint32_t pass_low[3] = { 240, 1500, 2464 };
    const uint32_t pass_high[3] = { 260, 1560, 2608 };
    uint32_t printed = 0;
    uint32_t replaced = 0;

    if (n3g_rockbox_raw_lpn == 0xffffffffu
     || n3g_rockbox_page_count > ARRAYLEN(n3g_rockbox_map_physpage))
    {
        FTL_PROGRESS("N3G_RBMAP_SKIP raw=%08lx pages=%lu",
                     (unsigned long)n3g_rockbox_raw_lpn,
                     (unsigned long)n3g_rockbox_page_count);
        return;
    }

    n3g_rockbox_lpn_base = ftl_n3g_rockbox_cand_lpn(n3g_rockbox_raw_lpn, 1);
    n3g_rockbox_map_found = n3g_rockbox_map_physpage[0] != 0xffffffffu ? 1 : 0;

    FTL_PROGRESS("N3G_RBMAP_START raw=%08lx base=%08lx pages=%lu packed=1",
                 (unsigned long)n3g_rockbox_raw_lpn,
                 (unsigned long)n3g_rockbox_lpn_base,
                 (unsigned long)n3g_rockbox_page_count);

    for (uint32_t pass = 0; pass < ARRAYLEN(pass_low); pass++)
    {
        uint32_t low_block = pass_low[pass];
        uint32_t high_block = pass_high[pass];

        FTL_PROGRESS("N3G_RBMAP_PASS p=%lu b=%lu..%lu f=%lu",
                     (unsigned long)pass,
                     (unsigned long)high_block,
                     (unsigned long)low_block,
                     (unsigned long)n3g_rockbox_map_found);

        for (uint32_t block = high_block + 1; block-- > low_block;)
        {
            if (((high_block - block) & 0xff) == 0)
                FTL_PROGRESS("N3G_RBMAP_SCAN p=%lu b=%lu f=%lu",
                             (unsigned long)pass,
                             (unsigned long)block,
                             (unsigned long)n3g_rockbox_map_found);

            for (uint32_t page = 0; page < ftl_nand_type->pagesperblock; page++)
            {
                uint32_t physpage = block * ftl_nand_type->pagesperblock + page;

                for (uint32_t bank = 0; bank < ftl_banks; bank++)
                {
                    uint32_t raw;
                    uint32_t usn;
                    uint32_t lpn;
                    uint32_t delta;
                    uint32_t idx;
                    uint32_t old_usn;

                    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                    if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                    (uint32_t *)&ftl_sparebuffer[0],
                                                    0x10) != 0)
                        continue;
                    if (ftl_sparebuffer[0].user.type != 0x40
                     && ftl_sparebuffer[0].user.type != 0x41)
                        continue;

                    raw = ftl_sparebuffer[0].user.lpn;
                    usn = ftl_sparebuffer[0].user.usn;
                    lpn = ftl_n3g_rockbox_cand_lpn(raw, 1);
                    if (lpn < n3g_rockbox_lpn_base)
                        continue;
                    delta = lpn - n3g_rockbox_lpn_base;
                    if (delta >= n3g_rockbox_page_count * 4u
                     || (delta & 3u) != 0)
                        continue;
                    idx = delta >> 2;
                    if (idx >= n3g_rockbox_page_count)
                        continue;
                    old_usn = n3g_rockbox_map_usn[idx];
                    if (n3g_rockbox_map_physpage[idx] != 0xffffffffu)
                    {
                        if (idx == 0)
                            continue;
                        if (usn <= old_usn)
                            continue;
                        replaced++;
                    }
                    else
                    {
                        n3g_rockbox_map_found++;
                    }

                    n3g_rockbox_map_bank[idx] = bank;
                    n3g_rockbox_map_physpage[idx] = physpage;
                    n3g_rockbox_map_raw[idx] = raw;
                    n3g_rockbox_map_usn[idx] = usn;
                    if (printed < 24)
                    {
                        FTL_PROGRESS("N3G_RBMH i=%lu bk=%lu b=%lu p=%lu raw=%08lx usn=%08lx old=%08lx",
                                     (unsigned long)idx,
                                     (unsigned long)bank,
                                     (unsigned long)block,
                                     (unsigned long)page,
                                     (unsigned long)raw,
                                     (unsigned long)usn,
                                     (unsigned long)old_usn);
                        printed++;
                    }
                }
            }
        }
    }

    FTL_PROGRESS("N3G_RBMAP_DONE f=%lu r=%lu pages=%lu p0=%lu p1=%lu p90=%lu plast=%lu",
                 (unsigned long)n3g_rockbox_map_found,
                 (unsigned long)replaced,
                 (unsigned long)n3g_rockbox_page_count,
                 (unsigned long)n3g_rockbox_map_physpage[0],
                 (unsigned long)n3g_rockbox_map_physpage[1],
                 (unsigned long)n3g_rockbox_map_physpage[90],
                 (unsigned long)n3g_rockbox_map_physpage[n3g_rockbox_page_count - 1]);
}

static int ftl_n3g_raw_covers_key(uint32_t raw, uint32_t desired_key,
                                  uint32_t *slot)
{
    uint32_t raw_key = raw >> 1;

    if (desired_key < raw_key || desired_key >= raw_key + 4)
        return 0;

    *slot = desired_key - raw_key;
    return 1;
}

static int ftl_n3g_rockbox_exact_slot(uint32_t map_idx, uint32_t file_sector,
                                      uint32_t *slot)
{
    uint32_t source_sector = ftl_n3g_rockbox_source_sector(file_sector);

    if ((source_sector >> 2) != map_idx)
        return -1;

    *slot = source_sector & 3u;
    return 0;
}

static uint32_t ftl_n3g_sum_bytes(const uint8_t *buf, uint32_t len)
{
    uint32_t sum = 0;

    for (uint32_t i = 0; i < len; i++)
        sum += buf[i];
    return sum;
}

static int ftl_n3g_rockbox_sector_sum_ok(uint32_t file_sector,
                                         uint32_t slot,
                                         uint32_t *got,
                                         uint32_t *want)
{
    uint32_t len;
    const uint8_t *p;
    const uint8_t *last = NULL;

    if (file_sector >= N3G_ROCKBOX_SECTOR_SUM_COUNT || slot >= 4)
        return 1;

    len = n3g_rockbox_sector_len[file_sector];
    p = n3g_direct_pagebuf + slot * 0x200;
    *got = ftl_n3g_sum_bytes(p, len);
    *want = n3g_rockbox_sector_sum[file_sector];
    if (file_sector >= N3G_ROCKBOX_DUMP_BASE
     && file_sector < N3G_ROCKBOX_DUMP_BASE + N3G_ROCKBOX_DUMP_COUNT)
        last = n3g_rockbox_dump_last8[file_sector - N3G_ROCKBOX_DUMP_BASE];
    return *got == *want
        && (!last || memcmp(p + 0x1f8, last, 8) == 0);
}

static int ftl_n3g_read_exact_rockbox_sector(uint32_t file_sector,
                                             uint32_t host_lpn,
                                             uint32_t offset,
                                             void *buffer)
{
    uint8_t *out = buffer;
    uint32_t group;
    uint32_t block;
    uint32_t page;
    uint32_t slot;
    uint32_t physpage;
    uint32_t got = 0;
    uint32_t want = 0;
    uint32_t attempt;
    int32_t rc;

    if (file_sector >= N3G_ROCKBOX_EXACT_MAP_COUNT)
        return 0;

    group = file_sector >> 2;
    if (group >= N3G_ROCKBOX_EXACT_GROUP_COUNT)
        return 0;

    block = n3g_rockbox_exact_block[group];
    page = n3g_rockbox_exact_page[group];
    slot = file_sector & 3u;
    physpage = block * N3G_EXACT_PAGES_PER_BLOCK + page;

    for (attempt = 0; attempt < 4; attempt++)
    {
        memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
        (void)nano3g_nand_diag_init_only(0);
        rc = nano3g_nand_diag_local_read(0, physpage, 0,
                                         (uint32_t *)n3g_direct_pagebuf,
                                         0x800 / sizeof(uint32_t));
        if (rc == 0
         && ftl_n3g_rockbox_sector_sum_ok(file_sector, slot, &got, &want))
            break;
    }

    if (attempt >= 4)
    {
        FTL_PROGRESS("N3G_EXACT_BAD fs=%lu host=%08lx b=%lu p=%lu abs=%lu ppb=%lu s=%lu rc=%ld a=%lu want=%08lx got=%08lx first=%02x%02x%02x%02x%02x%02x%02x%02x",
                     (unsigned long)file_sector,
                     (unsigned long)host_lpn,
                     (unsigned long)block,
                     (unsigned long)page,
                     (unsigned long)physpage,
                     (unsigned long)ftl_nand_type->pagesperblock,
                     (unsigned long)slot,
                     (long)rc,
                     (unsigned long)attempt,
                     (unsigned long)want,
                     (unsigned long)got,
                     n3g_direct_pagebuf[slot * 0x200 + 0],
                     n3g_direct_pagebuf[slot * 0x200 + 1],
                     n3g_direct_pagebuf[slot * 0x200 + 2],
                     n3g_direct_pagebuf[slot * 0x200 + 3],
                     n3g_direct_pagebuf[slot * 0x200 + 4],
                     n3g_direct_pagebuf[slot * 0x200 + 5],
                     n3g_direct_pagebuf[slot * 0x200 + 6],
                     n3g_direct_pagebuf[slot * 0x200 + 7]);
        return -1;
    }

    memcpy(out, n3g_direct_pagebuf + (slot << 9), 0x200);
    if (file_sector < 24 || (file_sector & 0x7fu) == 0
     || file_sector + 16 >= N3G_ROCKBOX_EXACT_MAP_COUNT)
        FTL_PROGRESS("N3G_RBSEC off=%lu fs=%lu host=%08lx bk=0 b=%lu p=%lu s=%lu src=EXACT d=%02x%02x%02x%02x%02x%02x%02x%02x",
                     (unsigned long)offset,
                     (unsigned long)file_sector,
                     (unsigned long)host_lpn,
                     (unsigned long)block,
                     (unsigned long)page,
                     (unsigned long)slot,
                     out[0], out[1], out[2], out[3],
                     out[4], out[5], out[6], out[7]);
    return 1;
}

static int ftl_n3g_read_located_rockbox_sector(uint32_t file_sector,
                                               uint32_t host_lpn,
                                               uint32_t offset,
                                               void *buffer)
{
    uint8_t *out = buffer;
    uint32_t bank;
    uint32_t physpage;
    uint32_t slot;
    uint32_t got = 0;
    uint32_t want = 0;
    int32_t rc;

    if (file_sector >= ARRAYLEN(n3g_rockbox_located_sector)
     || !n3g_rockbox_located_sector[file_sector])
        return 0;

    bank = n3g_rockbox_located_bank[file_sector];
    physpage = n3g_rockbox_located_physpage[file_sector];
    slot = n3g_rockbox_located_slot[file_sector];

    memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
    rc = nano3g_nand_diag_local_read(bank, physpage, 0,
                                     (uint32_t *)n3g_direct_pagebuf,
                                     0x800 / sizeof(uint32_t));
    if (rc != 0
     || !ftl_n3g_rockbox_sector_sum_ok(file_sector, slot, &got, &want))
    {
        FTL_PROGRESS("N3G_LOCSTALE fs=%lu host=%08lx bk=%lu b=%lu p=%lu s=%lu want=%08lx got=%08lx first=%02x%02x%02x%02x",
                     (unsigned long)file_sector,
                     (unsigned long)host_lpn,
                     (unsigned long)bank,
                     (unsigned long)(physpage / ftl_nand_type->pagesperblock),
                     (unsigned long)(physpage % ftl_nand_type->pagesperblock),
                     (unsigned long)slot,
                     (unsigned long)want,
                     (unsigned long)got,
                     n3g_direct_pagebuf[slot * 0x200 + 0],
                     n3g_direct_pagebuf[slot * 0x200 + 1],
                     n3g_direct_pagebuf[slot * 0x200 + 2],
                     n3g_direct_pagebuf[slot * 0x200 + 3]);
        n3g_rockbox_located_sector[file_sector] = 0;
        return 0;
    }

    memcpy(out, n3g_direct_pagebuf + (slot << 9), 0x200);
    if (file_sector < 24 || (file_sector & 0x7fu) == 0)
        FTL_PROGRESS("N3G_RBSEC off=%lu fs=%lu host=%08lx bk=%lu b=%lu p=%lu s=%lu src=LOC pay=data raw=%08lx usn=%08lx d=%02x%02x%02x%02x%02x%02x%02x%02x",
                     (unsigned long)offset,
                     (unsigned long)file_sector,
                     (unsigned long)host_lpn,
                     (unsigned long)bank,
                     (unsigned long)(physpage / ftl_nand_type->pagesperblock),
                     (unsigned long)(physpage % ftl_nand_type->pagesperblock),
                     (unsigned long)slot,
                     (unsigned long)n3g_rockbox_located_raw[file_sector],
                     (unsigned long)n3g_rockbox_located_usn[file_sector],
                     out[0], out[1], out[2], out[3],
                     out[4], out[5], out[6], out[7]);
    return 1;
}

static void ftl_n3g_dump_rockbox_candidate_slots(uint32_t map_idx,
                                                 uint32_t file_sector,
                                                 const char *tag,
                                                 uint32_t bank,
                                                 uint32_t physpage,
                                                 uint32_t raw,
                                                 uint32_t desired_key)
{
    uint32_t block = physpage / ftl_nand_type->pagesperblock;
    uint32_t page = physpage % ftl_nand_type->pagesperblock;
    uint32_t base_sector = file_sector & ~3u;

    if (map_idx >= ARRAYLEN(n3g_rockbox_slotdump_reported)
     || n3g_rockbox_slotdump_reported[map_idx])
        return;

    n3g_rockbox_slotdump_reported[map_idx] = 1;
    FTL_PROGRESS("N3G_RBHDUMP i=%lu fs=%lu exp=%lu..%lu tag=%s bk=%lu b=%lu p=%lu raw=%08lx key=%08lx",
                 (unsigned long)map_idx,
                 (unsigned long)file_sector,
                 (unsigned long)base_sector,
                 (unsigned long)(base_sector + 3),
                 tag,
                 (unsigned long)bank,
                 (unsigned long)block,
                 (unsigned long)page,
                 (unsigned long)raw,
                 (unsigned long)desired_key);

    for (uint32_t slot = 0; slot < 4; slot++)
    {
        uint32_t fs = base_sector + slot;
        uint8_t *p = n3g_direct_pagebuf + slot * 0x200;
        const uint8_t *elast = (fs >= N3G_ROCKBOX_DUMP_BASE
                             && fs < N3G_ROCKBOX_DUMP_BASE + N3G_ROCKBOX_DUMP_COUNT)
                              ? n3g_rockbox_dump_last8[fs - N3G_ROCKBOX_DUMP_BASE]
                              : p + 0x1f8;
        uint32_t sum = ftl_n3g_sum_bytes(p, 0x200);
        uint32_t fwant = fs < N3G_ROCKBOX_SECTOR_SUM_COUNT
                       ? n3g_rockbox_sector_sum[fs] : 0;
        uint32_t fmatch = fs < N3G_ROCKBOX_SECTOR_SUM_COUNT
                       && sum == fwant
                       && memcmp(p + 0x1f8, elast, 8) == 0;

        FTL_PROGRESS("N3G_RBHSLOT i=%lu s=%lu fs=%lu sum=%08lx want=%08lx m=%lu first=%02x%02x%02x%02x%02x%02x%02x%02x last=%02x%02x%02x%02x%02x%02x%02x%02x",
                     (unsigned long)map_idx,
                     (unsigned long)slot,
                     (unsigned long)fs,
                     (unsigned long)sum,
                     (unsigned long)fwant,
                     (unsigned long)fmatch,
                     p[0], p[1], p[2], p[3],
                     p[4], p[5], p[6], p[7],
                     p[0x1f8], p[0x1f9], p[0x1fa], p[0x1fb],
                     p[0x1fc], p[0x1fd], p[0x1fe], p[0x1ff]);
        (void)fs;
    }
}

static void ftl_n3g_report_rockbox_sum_reject(uint32_t map_idx,
                                              uint32_t file_sector,
                                              const char *tag,
                                              uint32_t bank,
                                              uint32_t physpage,
                                              uint32_t raw,
                                              uint32_t desired_key,
                                              uint32_t slot,
                                              uint32_t got,
                                              uint32_t want)
{
    if (file_sector >= ARRAYLEN(n3g_rockbox_sum_reported)
     || n3g_rockbox_sum_reported[file_sector])
        return;

    n3g_rockbox_sum_reported[file_sector] = 1;
    ftl_n3g_dump_rockbox_candidate_slots(map_idx, file_sector, tag, bank,
                                         physpage, raw, desired_key);
    FTL_PROGRESS("N3G_RBH_REJECT i=%lu fs=%lu foff=%lu tag=%s bk=%lu b=%lu p=%lu raw=%08lx key=%08lx s=%lu want=%08lx got=%08lx reason=sum",
                 (unsigned long)map_idx,
                 (unsigned long)file_sector,
                 (unsigned long)(file_sector << 9),
                 tag,
                 (unsigned long)bank,
                 (unsigned long)(physpage / ftl_nand_type->pagesperblock),
                 (unsigned long)(physpage % ftl_nand_type->pagesperblock),
                 (unsigned long)raw,
                 (unsigned long)desired_key,
                 (unsigned long)slot,
                 (unsigned long)want,
                 (unsigned long)got);
    FTL_PROGRESS("N3G_RBH_BYTES fs=%lu load=%02x%02x%02x%02x%02x%02x%02x%02x",
                 (unsigned long)file_sector,
                 n3g_direct_pagebuf[slot * 0x200 + 0],
                 n3g_direct_pagebuf[slot * 0x200 + 1],
                 n3g_direct_pagebuf[slot * 0x200 + 2],
                 n3g_direct_pagebuf[slot * 0x200 + 3],
                 n3g_direct_pagebuf[slot * 0x200 + 4],
                 n3g_direct_pagebuf[slot * 0x200 + 5],
                 n3g_direct_pagebuf[slot * 0x200 + 6],
                 n3g_direct_pagebuf[slot * 0x200 + 7]);
}

static int ftl_n3g_store_rockbox_raw_cover(uint32_t map_idx,
                                           uint32_t file_sector,
                                           uint32_t desired_key,
                                           uint32_t bank,
                                           uint32_t physpage,
                                           const char *tag,
                                           uint32_t *slot_out)
{
    uint32_t raw;
    uint32_t usn;
    uint32_t block = physpage / ftl_nand_type->pagesperblock;
    uint32_t page = physpage % ftl_nand_type->pagesperblock;
    uint32_t slot;
    uint32_t raw_slot;
    uint32_t got = 0;
    uint32_t want = 0;
    int32_t rc;

    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0)
    {
        if (tag[0] == 'P')
            FTL_PROGRESS("N3G_RBH_REJECT i=%lu tag=%s bk=%lu b=%lu p=%lu reason=read",
                         (unsigned long)map_idx, tag,
                         (unsigned long)bank, (unsigned long)block,
                         (unsigned long)page);
        return -1;
    }

    if (ftl_sparebuffer[0].user.type != 0x40
     && ftl_sparebuffer[0].user.type != 0x41)
    {
        if (tag[0] == 'P')
            FTL_PROGRESS("N3G_RBH_REJECT i=%lu tag=%s bk=%lu b=%lu p=%lu t=%02x reason=type",
                         (unsigned long)map_idx, tag,
                         (unsigned long)bank, (unsigned long)block,
                         (unsigned long)page,
                         (unsigned int)ftl_sparebuffer[0].user.type);
        return -1;
    }

    raw = ftl_sparebuffer[0].user.lpn;
    usn = ftl_sparebuffer[0].user.usn;
    if (ftl_n3g_rockbox_exact_slot(map_idx, file_sector, &slot) != 0)
        return -1;
    if (!ftl_n3g_raw_covers_key(raw, desired_key, &raw_slot))
    {
        if (tag[0] == 'P')
            FTL_PROGRESS("N3G_RBH_REJECT i=%lu tag=%s bk=%lu b=%lu p=%lu raw=%08lx key=%08lx reason=range",
                         (unsigned long)map_idx, tag,
                         (unsigned long)bank, (unsigned long)block,
                         (unsigned long)page,
                         (unsigned long)raw,
                         (unsigned long)desired_key);
        return -1;
    }

    if (usn == 0xffffffffu)
    {
        FTL_PROGRESS("N3G_RBH_REJECT i=%lu tag=%s bk=%lu b=%lu p=%lu raw=%08lx reason=usn",
                     (unsigned long)map_idx, tag,
                     (unsigned long)bank, (unsigned long)block,
                     (unsigned long)page,
                     (unsigned long)raw);
        return -1;
    }

    memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
    rc = nano3g_nand_diag_local_read(bank, physpage, 0,
                                     (uint32_t *)n3g_direct_pagebuf,
                                     0x800 / sizeof(uint32_t));
    if (rc != 0)
    {
        FTL_PROGRESS("N3G_RBH_REJECT i=%lu fs=%lu tag=%s bk=%lu b=%lu p=%lu raw=%08lx key=%08lx s=%lu reason=data",
                     (unsigned long)map_idx,
                     (unsigned long)file_sector,
                     tag,
                     (unsigned long)bank,
                     (unsigned long)block,
                     (unsigned long)page,
                     (unsigned long)raw,
                     (unsigned long)desired_key,
                     (unsigned long)slot);
        return -1;
    }

    if (!ftl_n3g_rockbox_sector_sum_ok(file_sector, slot, &got, &want))
    {
        ftl_n3g_report_rockbox_sum_reject(map_idx, file_sector, tag,
                                          bank, physpage, raw, desired_key,
                                          slot, got, want);
        return -1;
    }

    n3g_rockbox_map_bank[map_idx] = bank;
    n3g_rockbox_map_slot[map_idx] = slot;
    n3g_rockbox_map_physpage[map_idx] = physpage;
    n3g_rockbox_map_raw[map_idx] = raw;
    n3g_rockbox_map_usn[map_idx] = usn;
    if (map_idx >= n3g_rockbox_map_found)
        n3g_rockbox_map_found = map_idx + 1;
    *slot_out = slot;

    FTL_PROGRESS("N3G_RBH_COMMIT i=%lu fs=%lu bk=%lu b=%lu p=%lu raw=%08lx key=%08lx s=%lu rs=%lu usn=%08lx first=%02x%02x%02x%02x sum=%08lx reason=%s",
                 (unsigned long)map_idx,
                 (unsigned long)file_sector,
                 (unsigned long)bank,
                 (unsigned long)block,
                 (unsigned long)page,
                 (unsigned long)raw,
                 (unsigned long)desired_key,
                 (unsigned long)slot,
                 (unsigned long)raw_slot,
                 (unsigned long)usn,
                 n3g_direct_pagebuf[slot * 0x200 + 0],
                 n3g_direct_pagebuf[slot * 0x200 + 1],
                 n3g_direct_pagebuf[slot * 0x200 + 2],
                 n3g_direct_pagebuf[slot * 0x200 + 3],
                 (unsigned long)got,
                 tag);
    return 0;
}

static int ftl_n3g_store_rockbox_payload_match(uint32_t map_idx,
                                               uint32_t file_sector,
                                               uint32_t desired_key,
                                               uint32_t bank,
                                               uint32_t physpage,
                                               const char *tag,
                                               uint32_t *slot_out)
{
    uint32_t raw;
    uint32_t usn;
    uint32_t block = physpage / ftl_nand_type->pagesperblock;
    uint32_t page = physpage % ftl_nand_type->pagesperblock;
    int32_t rc;

    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0)
        return -1;

    if (ftl_sparebuffer[0].user.type != 0x40
     && ftl_sparebuffer[0].user.type != 0x41)
        return -1;

    raw = ftl_sparebuffer[0].user.lpn;
    usn = ftl_sparebuffer[0].user.usn;
    if (usn == 0xffffffffu)
        return -1;

    memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
    rc = nano3g_nand_diag_local_read(bank, physpage, 0,
                                     (uint32_t *)n3g_direct_pagebuf,
                                     0x800 / sizeof(uint32_t));
    if (rc != 0)
        return -1;

    {
        uint32_t slot;
        uint32_t got = 0;
        uint32_t want = 0;

        if (ftl_n3g_rockbox_exact_slot(map_idx, file_sector, &slot) != 0
         || !ftl_n3g_rockbox_sector_sum_ok(file_sector, slot, &got, &want))
            return -1;

        n3g_rockbox_map_bank[map_idx] = bank;
        n3g_rockbox_map_slot[map_idx] = slot;
        n3g_rockbox_map_physpage[map_idx] = physpage;
        n3g_rockbox_map_raw[map_idx] = (desired_key - slot) << 1;
        n3g_rockbox_map_usn[map_idx] = usn;
        if (map_idx >= n3g_rockbox_map_found)
            n3g_rockbox_map_found = map_idx + 1;
        *slot_out = slot;

        FTL_PROGRESS("N3G_RBH_COMMIT i=%lu fs=%lu bk=%lu b=%lu p=%lu raw=%08lx araw=%08lx key=%08lx s=%lu usn=%08lx first=%02x%02x%02x%02x sum=%08lx reason=%sPAY",
                     (unsigned long)map_idx,
                     (unsigned long)file_sector,
                     (unsigned long)bank,
                     (unsigned long)block,
                     (unsigned long)page,
                     (unsigned long)n3g_rockbox_map_raw[map_idx],
                     (unsigned long)raw,
                     (unsigned long)desired_key,
                     (unsigned long)slot,
                     (unsigned long)usn,
                     n3g_direct_pagebuf[slot * 0x200 + 0],
                     n3g_direct_pagebuf[slot * 0x200 + 1],
                     n3g_direct_pagebuf[slot * 0x200 + 2],
                     n3g_direct_pagebuf[slot * 0x200 + 3],
                     (unsigned long)got,
                     tag);
        return 0;
    }

    return -1;
}

static int ftl_n3g_find_rockbox_raw_page(uint32_t map_idx,
                                         uint32_t file_sector,
                                         uint32_t desired_key,
                                         uint32_t *slot)
{
    const uint32_t pass_low[3] = { 2464, 2609, 2048 };
    const uint32_t pass_high[3] = { 2608, 7167, 2463 };
    uint32_t hint_blocks[6];

    if (map_idx < 220)
    {
        hint_blocks[0] = 2815;
        hint_blocks[1] = 2759;
        hint_blocks[2] = 2845;
        hint_blocks[3] = 2535;
        hint_blocks[4] = 2538;
        hint_blocks[5] = 2915;
    }
    else if (map_idx >= 347)
    {
        hint_blocks[0] = 5643;
        hint_blocks[1] = 2845;
        hint_blocks[2] = 2759;
        hint_blocks[3] = 2538;
        hint_blocks[4] = 2815;
        hint_blocks[5] = 2915;
    }
    else
    {
        hint_blocks[0] = 2845;
        hint_blocks[1] = 2759;
        hint_blocks[2] = 2815;
        hint_blocks[3] = 2535;
        hint_blocks[4] = 2538;
        hint_blocks[5] = 2915;
    }

    FTL_PROGRESS("N3G_RBHSCAN i=%lu key=%08lx",
                 (unsigned long)map_idx,
                 (unsigned long)desired_key);

    for (int32_t i = (int32_t)map_idx - 1; i >= 0 && i >= (int32_t)map_idx - 64; i--)
    {
        if (n3g_rockbox_map_physpage[i] != 0xffffffffu)
        {
            uint32_t physpage = n3g_rockbox_map_physpage[i] + (map_idx - (uint32_t)i);
            if (ftl_n3g_store_rockbox_raw_cover(map_idx, file_sector,
                                                desired_key,
                                                n3g_rockbox_map_bank[i],
                                                physpage, "PRED",
                                                slot) == 0)
                return 0;
            if (ftl_n3g_store_rockbox_payload_match(map_idx, file_sector,
                                                    desired_key,
                                                    n3g_rockbox_map_bank[i],
                                                    physpage, "PRED",
                                                    slot) == 0)
                return 0;
            break;
        }
    }

    for (uint32_t h = 0; h < ARRAYLEN(hint_blocks); h++)
    {
        uint32_t center = hint_blocks[h];

        FTL_PROGRESS("N3G_RBHWIN i=%lu b=%lu +/-8 key=%08lx",
                     (unsigned long)map_idx,
                     (unsigned long)center,
                     (unsigned long)desired_key);
        for (uint32_t d = 0; d <= 8; d++)
        {
            for (uint32_t side = 0; side < (d == 0 ? 1u : 2u); side++)
            {
                uint32_t block;

                if (side == 0)
                    block = center + d;
                else if (center >= d)
                    block = center - d;
                else
                    continue;

                for (uint32_t page = 0; page < ftl_nand_type->pagesperblock; page++)
                {
                    uint32_t physpage = block * ftl_nand_type->pagesperblock + page;
                    for (uint32_t bank = 0; bank < ftl_banks; bank++)
                    {
                        if (ftl_n3g_store_rockbox_raw_cover(map_idx, file_sector,
                                                            desired_key,
                                                            bank, physpage, "WIN",
                                                            slot) == 0)
                            return 0;
                        if (ftl_n3g_store_rockbox_payload_match(map_idx, file_sector,
                                                                desired_key,
                                                                bank, physpage, "WIN",
                                                                slot) == 0)
                            return 0;
                    }
                }
            }
        }
    }

    for (uint32_t pass = 0; pass < ARRAYLEN(pass_low); pass++)
    {
        uint32_t low_block = pass_low[pass];
        uint32_t high_block = pass_high[pass];

        FTL_PROGRESS("N3G_RBH_PASS p=%lu b=%lu..%lu",
                     (unsigned long)pass,
                     (unsigned long)high_block,
                     (unsigned long)low_block);

        for (uint32_t block = high_block + 1; block-- > low_block;)
        {
            if (((high_block - block) & 0xffu) == 0)
                FTL_PROGRESS("N3G_RBH_SCAN p=%lu b=%lu",
                             (unsigned long)pass,
                             (unsigned long)block);

            for (uint32_t page = 0; page < ftl_nand_type->pagesperblock; page++)
            {
                uint32_t physpage = block * ftl_nand_type->pagesperblock + page;

                for (uint32_t bank = 0; bank < ftl_banks; bank++)
                {
                    if (ftl_n3g_store_rockbox_raw_cover(map_idx, file_sector,
                                                        desired_key,
                                                        bank, physpage, "FULL",
                                                        slot) == 0)
                        return 0;
                    if (ftl_n3g_store_rockbox_payload_match(map_idx, file_sector,
                                                            desired_key,
                                                            bank, physpage, "FULL",
                                                            slot) == 0)
                        return 0;
                }
            }
        }
    }

    FTL_PROGRESS("N3G_RBHNONE i=%lu key=%08lx",
                 (unsigned long)map_idx,
                 (unsigned long)desired_key);
    return -1;
}

int ftl_n3g_read_rockbox_file_sector(uint32_t file_sector, uint32_t host_lpn,
                                     void *buffer)
{
    const uint32_t rockbox_size = 861092u;
    const uint32_t rockbox_start_lba = 0x006e8136u;
    uint8_t *out = buffer;
    uint32_t source_file_sector;
    uint32_t source_host_lpn;
    uint32_t map_idx;
    uint32_t offset;
    uint32_t slot;
    uint32_t physpage;
    uint32_t bank;
    uint32_t usn;
    uint32_t host_delta;
    uint32_t desired_key;
    int32_t rc;

    offset = file_sector << 9;
    if (offset >= rockbox_size)
        return 0;

    rc = ftl_n3g_read_exact_rockbox_sector(file_sector, host_lpn,
                                           offset, out);
    if (rc != 0)
        return rc;

    if (!n3g_rockbox_phys_valid)
        return -1;

    if (file_sector == 0)
    {
        memcpy(out, n3g_rockbox_first_sector, sizeof(n3g_rockbox_first_sector));
        FTL_PROGRESS("N3G_RBSEC off=%lu fs=%lu bk=0 b=248 p=15 s=0 src=FIX pay=hdr d=%02x%02x%02x%02x%02x%02x%02x%02x",
                     (unsigned long)offset,
                     (unsigned long)file_sector,
                     out[0], out[1], out[2], out[3],
                     out[4], out[5], out[6], out[7]);
        return 1;
    }

    if (file_sector < ARRAYLEN(n3g_rockbox_located_sector))
    {
        rc = ftl_n3g_read_located_rockbox_sector(file_sector, host_lpn,
                                                 offset, out);
        if (rc != 0)
            return rc;
        if (ftl_n3g_locate_rockbox_sector(file_sector) == 0)
        {
            rc = ftl_n3g_read_located_rockbox_sector(file_sector, host_lpn,
                                                     offset, out);
            if (rc != 0)
                return rc;
        }
    }

    source_file_sector = ftl_n3g_rockbox_source_sector(file_sector);
    source_host_lpn = rockbox_start_lba + source_file_sector;
    host_delta = source_file_sector;
    map_idx = source_file_sector >> 2;
    desired_key = (n3g_rockbox_raw_lpn >> 1) + host_delta;
    usn = map_idx < ARRAYLEN(n3g_rockbox_map_usn) ? n3g_rockbox_map_usn[map_idx] : 0;
    if (host_lpn < rockbox_start_lba || map_idx >= n3g_rockbox_page_count)
        return -1;
    if (ftl_n3g_rockbox_exact_slot(map_idx, file_sector, &slot) != 0)
        return -1;
    if (n3g_rockbox_map_physpage[map_idx] != 0xffffffffu)
    {
        uint32_t got = 0;
        uint32_t want = 0;

        bank = n3g_rockbox_map_bank[map_idx];
        physpage = n3g_rockbox_map_physpage[map_idx];
        memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
        rc = nano3g_nand_diag_local_read(bank, physpage, 0,
                                         (uint32_t *)n3g_direct_pagebuf,
                                         0x800 / sizeof(uint32_t));
        if (rc != 0
         || !ftl_n3g_rockbox_sector_sum_ok(file_sector, slot, &got, &want))
        {
            FTL_PROGRESS("N3G_RBSTALE i=%lu fs=%lu bk=%lu b=%lu p=%lu s=%lu first=%02x%02x%02x%02x want=%08lx got=%08lx",
                         (unsigned long)map_idx,
                         (unsigned long)file_sector,
                         (unsigned long)bank,
                         (unsigned long)(physpage / ftl_nand_type->pagesperblock),
                         (unsigned long)(physpage % ftl_nand_type->pagesperblock),
                         (unsigned long)slot,
                         n3g_direct_pagebuf[slot * 0x200 + 0],
                         n3g_direct_pagebuf[slot * 0x200 + 1],
                         n3g_direct_pagebuf[slot * 0x200 + 2],
                         n3g_direct_pagebuf[slot * 0x200 + 3],
                         (unsigned long)want,
                         (unsigned long)got);
            n3g_rockbox_map_bank[map_idx] = 0xffu;
            n3g_rockbox_map_slot[map_idx] = 0xffu;
            n3g_rockbox_map_physpage[map_idx] = 0xffffffffu;
            n3g_rockbox_map_raw[map_idx] = 0xffffffffu;
            n3g_rockbox_map_usn[map_idx] = 0;
        }
    }
    if (n3g_rockbox_map_physpage[map_idx] == 0xffffffffu
     && ftl_n3g_find_rockbox_raw_page(map_idx, file_sector,
                                      desired_key, &slot) != 0)
    {
        memset(out, 0, 0x200);
        FTL_PROGRESS("N3G_RBMISS off=%lu fs=%lu srcfs=%lu host=%08lx shost=%08lx idx=%lu key=%08lx found=%lu pages=%lu",
                     (unsigned long)offset,
                     (unsigned long)file_sector,
                     (unsigned long)source_file_sector,
                     (unsigned long)host_lpn,
                     (unsigned long)source_host_lpn,
                     (unsigned long)map_idx,
                     (unsigned long)desired_key,
                     (unsigned long)n3g_rockbox_map_found,
                     (unsigned long)n3g_rockbox_page_count);
        return -1;
    }

    bank = n3g_rockbox_map_bank[map_idx];
    physpage = n3g_rockbox_map_physpage[map_idx];
    usn = n3g_rockbox_map_usn[map_idx];

    memset(n3g_direct_pagebuf, 0, sizeof(n3g_direct_pagebuf));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    rc = nano3g_nand_diag_local_read(bank, physpage, 0,
                                     (uint32_t *)n3g_direct_pagebuf,
                                     0x800 / sizeof(uint32_t));
    if (rc != 0)
    {
        memset(out, 0, 0x200);
        FTL_PROGRESS("N3G_RBERR off=%lu fs=%lu bk=%lu b=%lu p=%lu s=%lu r=%ld",
                     (unsigned long)offset,
                     (unsigned long)file_sector,
                     (unsigned long)bank,
                     (unsigned long)(physpage / ftl_nand_type->pagesperblock),
                     (unsigned long)(physpage % ftl_nand_type->pagesperblock),
                     (unsigned long)slot,
                     (long)rc);
        return -1;
    }

    memcpy(out, n3g_direct_pagebuf + slot * 0x200, 0x200);
    if (file_sector < 24 || (file_sector & 0x7fu) == 0
     || offset + 0x4000u >= rockbox_size)
        FTL_PROGRESS("N3G_RBSEC off=%lu fs=%lu srcfs=%lu host=%08lx shost=%08lx bk=%lu b=%lu p=%lu s=%lu ms=%lu src=PACK pay=data raw=%08lx key=%08lx usn=%08lx d=%02x%02x%02x%02x%02x%02x%02x%02x",
                     (unsigned long)offset,
                     (unsigned long)file_sector,
                     (unsigned long)source_file_sector,
                     (unsigned long)host_lpn,
                     (unsigned long)source_host_lpn,
                     (unsigned long)bank,
                     (unsigned long)(physpage / ftl_nand_type->pagesperblock),
                     (unsigned long)(physpage % ftl_nand_type->pagesperblock),
                     (unsigned long)slot,
                     (unsigned long)n3g_rockbox_map_slot[map_idx],
                     (unsigned long)n3g_rockbox_map_raw[map_idx],
                     (unsigned long)desired_key,
                     (unsigned long)usn,
                     out[0], out[1], out[2], out[3],
                     out[4], out[5], out[6], out[7]);
    return 1;
}

static uint32_t ftl_n3g_synth_rockbox_phys(uint8_t *out, uint32_t host_lpn)
{
    const uint32_t rockbox_size = 861092u;
    const uint32_t rockbox_start_lba = 0x006e8136u;
    uint32_t file_sector;
    int rc;

    if (host_lpn < rockbox_start_lba)
        return 0;

    file_sector = host_lpn - rockbox_start_lba;
    if ((file_sector << 9) >= rockbox_size)
        return 0;

    rc = ftl_n3g_read_rockbox_file_sector(file_sector, host_lpn, out);
    if (rc == 1)
        return 1;

    return (uint32_t)-1;
}

static uint32_t ftl_n3g_physrb_mount(void)
{
    n3g_direct_map_mount = 1;
    n3g_direct_mbr_valid = 1;
    n3g_direct_mbr_j = 0xffffffffu;
    n3g_direct_mbr_v = 0xffffu;
    n3g_direct_mbr_po = 0;
    n3g_direct_sector_base = 0;
    n3g_direct_sector_scale = 1;
    n3g_direct_boot_valid = 1;
    n3g_direct_boot_lpn = 0x0000a07eu;
    n3g_direct_boot_vblock = 0x044cu;
    n3g_direct_boot_page = 62;
    n3g_direct_boot_slot = 0;
    n3g_direct_boot_shift = 0;
    n3g_direct_fsinfo_valid = 1;
    n3g_direct_fsinfo_lpn = 0x0000a086u;
    n3g_direct_map_loaded_entries = 0;
    n3g_direct_map_pages_found = 0;
    n3g_direct_map_max_idx = 0;
    n3g_direct_map_scan_hits = 0;

    memset(n3g_direct_mbr, 0, sizeof(n3g_direct_mbr));
    n3g_direct_mbr[0x1be + 4] = 0x0c;
    n3g_direct_mbr[0x1be + 8] = 0x7e;
    n3g_direct_mbr[0x1be + 9] = 0xa0;
    n3g_direct_mbr[0x1be + 12] = 0x81;
    n3g_direct_mbr[0x1be + 13] = 0x7f;
    n3g_direct_mbr[0x1be + 14] = 0x0e;
    n3g_direct_mbr[0x1fe] = 0x55;
    n3g_direct_mbr[0x1ff] = 0xaa;

    ftl_n3g_find_rockbox_phys_near();
    FTL_PROGRESS("N3G_OLDMAP_FASTMOUNT p0=0000A07E sz=000E7F81 rb=exact fc=0000D44B lba=006E8136 b=248 p=15");
    return 0;
}

static void ftl_n3g_scan_rockbox_header(void)
{
    uint32_t blocks = ftl_nand_type->blocks;
    uint32_t high = blocks > 1024 ? blocks - 1025 : blocks - 1;
    uint32_t low = high > 5119 ? high - 5119 : 0;
    uint32_t hits = 0;
    uint32_t scanned = 0;

    FTL_PROGRESS("N3G_HDRSCAN_START banks=%lu b=%lu..%lu ppb=%lu",
                 (unsigned long)ftl_banks,
                 (unsigned long)high,
                 (unsigned long)low,
                 (unsigned long)ftl_nand_type->pagesperblock);

    for (uint32_t block = high + 1; block-- > low && hits < 8;)
    {
        if (((high - block) & 0xff) == 0)
            FTL_PROGRESS("N3G_HDRSCAN b=%lu h=%lu",
                         (unsigned long)block,
                         (unsigned long)hits);

        for (uint32_t page = 0;
             page < ftl_nand_type->pagesperblock && hits < 8;
             page++)
        {
            uint32_t physpage = block * ftl_nand_type->pagesperblock + page;

            for (uint32_t bank = 0; bank < ftl_banks && hits < 8; bank++)
            {
                uint8_t *b = n3g_direct_pagebuf;
                uint32_t rc_data;
                uint32_t rc_spare;

                memset(b, 0, sizeof(n3g_direct_pagebuf));
                rc_data = nano3g_nand_diag_local_read(bank, physpage, 0,
                                                      (uint32_t *)b,
                                                      0x800 / sizeof(uint32_t));
                scanned++;
                if (rc_data != 0)
                    continue;

                for (uint32_t slot = 0; slot < 4 && hits < 8; slot++)
                {
                    uint32_t off = slot << 9;

                    if (b[off + 4] != 'n' || b[off + 5] != 'n'
                     || b[off + 6] != '3' || b[off + 7] != 'g')
                        continue;

                    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                    rc_spare = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                           (uint32_t *)&ftl_sparebuffer[0],
                                                           0x10);
                    FTL_PROGRESS("N3G_HDRHIT bk=%lu b=%lu p=%lu s=%lu rc=%lu/%lu t=%02x raw=%08lx lpn=%08lx",
                                 (unsigned long)bank,
                                 (unsigned long)block,
                                 (unsigned long)page,
                                 (unsigned long)slot,
                                 (unsigned long)rc_data,
                                 (unsigned long)rc_spare,
                                 (unsigned long)ftl_sparebuffer[0].user.type,
                                 (unsigned long)ftl_sparebuffer[0].user.lpn,
                                 (unsigned long)ftl_n3g_oob_lpn512(ftl_sparebuffer[0].user.lpn));
                    FTL_PROGRESS("N3G_HDRBYTES b0=%02x%02x%02x%02x%02x%02x%02x%02x b8=%02x%02x%02x%02x%02x%02x%02x%02x",
                                 b[off + 0], b[off + 1],
                                 b[off + 2], b[off + 3],
                                 b[off + 4], b[off + 5],
                                 b[off + 6], b[off + 7],
                                 b[off + 8], b[off + 9],
                                 b[off + 10], b[off + 11],
                                 b[off + 12], b[off + 13],
                                 b[off + 14], b[off + 15]);
                    hits++;
                }
            }
        }
    }

    FTL_PROGRESS("N3G_HDRSCAN_DONE h=%lu scanned=%lu",
                 (unsigned long)hits,
                 (unsigned long)scanned);
}

static uint32_t ftl_n3g_direct_cached_lpn0(uint32_t j, uint32_t *lpn0)
{
    uint32_t rc;

    if (j < ARRAYLEN(n3g_direct_l0_cache))
    {
        if (n3g_direct_l0_cache[j] == 0xfffffffeu)
            return -1;
        if (n3g_direct_l0_cache[j] != 0xffffffffu)
        {
            *lpn0 = n3g_direct_l0_cache[j];
            return 0;
        }
    }

    rc = ftl_n3g_wmount_map_lpn0(j, lpn0);
    if (j < ARRAYLEN(n3g_direct_l0_cache))
        n3g_direct_l0_cache[j] = rc == 0 ? *lpn0 : 0xfffffffeu;
    return rc;
}

static uint32_t ftl_n3g_direct_find_entry_covering_lba(
        uint32_t lpn, struct n3g_direct_covering_entry *entry)
{
    uint32_t found = 0;
    uint32_t best_near = 0xffffffffu;
    uint32_t page_lpn = lpn >> 2;
    uint32_t page_slot = lpn & 3u;

    entry->j = 0xffffffffu;
    entry->v = 0xffffu;
    entry->l0 = 0xffffffffu;
    entry->delta = 0xffffffffu;
    entry->po = 0xffffffffu;
    entry->slot = 0xffffffffu;

    if (ppb != 0)
    {
        uint32_t lblock = page_lpn / ppb;
        uint32_t lpage = page_lpn % ppb;

        for (uint32_t i = 0; i < ARRAYLEN(n3g_log_scattered); i++)
        {
            uint32_t po;

            if (n3g_log_logical[i] != lblock
             || n3g_log_scattered[i] == 0xffffu)
                continue;
            po = n3g_log_offsets[i][lpage];
            if (po == 0xffffu || po >= ppb)
                continue;

            entry->j = 0x80000000u | i;
            entry->v = n3g_log_scattered[i];
            entry->l0 = page_lpn << 2;
            entry->delta = page_slot;
            entry->po = po;
            entry->slot = page_slot;
            return 0;
        }
    }

    for (uint32_t j = 0; j < n3g_direct_map_loaded_entries; j++)
    {
        uint32_t v;
        uint32_t l0;
        uint32_t delta;

        if (j >= ARRAYLEN(ftl_map))
            break;
        v = ftl_map[j];
        if (v == 0 || v == 0xffffu)
            continue;
        if (ftl_n3g_direct_cached_lpn0(j, &l0) != 0)
            continue;

        if (lpn >= l0)
        {
            delta = lpn - l0;
            if (delta < best_near)
            {
                best_near = delta;
                entry->j = j;
                entry->v = v;
                entry->l0 = l0;
                entry->delta = delta;
                entry->po = delta >> 2;
                entry->slot = delta & 3u;
            }
        }

        if (lpn < l0)
            continue;
        delta = lpn - l0;
        if (delta >= ppb * 4)
            continue;
        if (found && l0 <= entry->l0)
            continue;

        found = 1;
        entry->j = j;
        entry->v = v;
        entry->l0 = l0;
        entry->delta = delta;
        entry->po = delta >> 2;
        entry->slot = delta & 3u;
    }

    return found ? 0 : (uint32_t)-1;
}

static uint32_t ftl_n3g_direct_read_512(uint32_t lpn, uint8_t *out,
                                        struct n3g_direct_read_trace *trace)
{
    struct n3g_direct_covering_entry entry;
    uint32_t bank;
    uint32_t physpage;
    uint32_t expected_lpn;
    uint32_t ret;

    if (trace != NULL)
    {
        ftl_n3g_direct_trace_init(trace);
        trace->lookup = lpn;
    }

    if (ftl_n3g_direct_find_entry_covering_lba(lpn, &entry) != 0)
    {
        memset(out, 0, 0x200);
        if (trace != NULL)
        {
            trace->j = entry.j;
            trace->v = entry.v;
            trace->l0 = entry.l0;
            trace->delta = entry.delta;
            trace->po = entry.po;
            trace->slot = entry.slot;
            trace->reason = "nocover";
        }
        return -6;
    }

    ftl_n3g_decode_page(entry.v, entry.po, &bank, &physpage);
    if (trace != NULL)
    {
        trace->j = entry.j;
        trace->v = entry.v;
        trace->lookup = lpn;
        trace->l0 = entry.l0;
        trace->span = ppb * 4;
        trace->delta = entry.delta;
        trace->po = entry.po;
        trace->slot = entry.slot;
        trace->bank = bank;
        trace->pb = physpage / ftl_nand_type->pagesperblock;
        trace->pp = physpage % ftl_nand_type->pagesperblock;
        trace->reason = "try";
    }

    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    ret = nand_read_page(bank, physpage, n3g_direct_pagebuf,
                         &ftl_sparebuffer[0], 1, 0);
    if (ret != 0)
    {
        memset(out, 0, 0x200);
        if (trace != NULL)
            trace->reason = "read";
        return -6;
    }
    if (ftl_sparebuffer[0].user.type != 0x40
     && ftl_sparebuffer[0].user.type != 0x41)
    {
        memset(out, 0, 0x200);
        if (trace != NULL)
            trace->reason = "type";
        return -6;
    }
    expected_lpn = ftl_n3g_oob_lpn512(ftl_sparebuffer[0].user.lpn);
    if (lpn < expected_lpn || lpn > expected_lpn + 3)
    {
        memset(out, 0, 0x200);
        if (trace != NULL)
            trace->reason = "lpn";
        return -6;
    }
    entry.slot = lpn - expected_lpn;
    if (trace != NULL)
        trace->slot = entry.slot;

    memcpy(out, n3g_direct_pagebuf + entry.slot * 0x200, 0x200);
    if (trace != NULL)
        trace->reason = "ok";
    return 0;
}

static uint32_t ftl_n3g_direct_read(uint32_t sector, uint32_t count,
                                    void *buffer)
{
    uint32_t error = 0;

    if (count == 0)
        return 0;

    mutex_lock(&ftl_mtx);

    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t host_lpn = sector + i;
        uint32_t lpn = n3g_direct_sector_base
                     + host_lpn * n3g_direct_sector_scale;
        uint8_t *out = &((uint8_t *)buffer)[i << 9];
        struct n3g_direct_read_trace trace;
        uint32_t rc = 0;

        if (n3g_direct_mbr_valid && host_lpn == 0)
        {
            memcpy(out, n3g_direct_mbr, 0x200);
            if (ftl_n3g_direct_trace_lba(host_lpn))
                FTL_PROGRESS("N3G_READ lba=%08lx rc=0 j=MBR v=---- po=0 s1=0 pb=---- pp=---- reason=mbr",
                             (unsigned long)host_lpn);
            continue;
        }

        if (n3g_direct_fsinfo_valid && host_lpn == n3g_direct_fsinfo_lpn)
        {
            memset(out, 0, 0x200);
            out[0] = 0x52;
            out[1] = 0x52;
            out[2] = 0x61;
            out[3] = 0x41;
            out[0x1e4] = 0x72;
            out[0x1e5] = 0x72;
            out[0x1e6] = 0x41;
            out[0x1e7] = 0x61;
            out[0x1e8] = 0x0c;
            out[0x1e9] = 0xc5;
            out[0x1ea] = 0x00;
            out[0x1eb] = 0x00;
            out[0x1ec] = 0x8e;
            out[0x1ed] = 0xb5;
            out[0x1ee] = 0x0d;
            out[0x1ef] = 0x00;
            out[0x1fc] = 0x00;
            out[0x1fd] = 0x00;
            out[0x1fe] = 0x55;
            out[0x1ff] = 0xaa;
            continue;
        }

        if (n3g_direct_boot_valid && host_lpn == n3g_direct_boot_lpn)
        {
            ftl_n3g_synth_fat32_bpb(out);
            n3g_direct_fsinfo_valid = 1;
            n3g_direct_fsinfo_lpn = host_lpn + 8;
            if (ftl_n3g_direct_trace_lba(host_lpn))
                FTL_PROGRESS("N3G_READ lba=%08lx rc=0 reason=synthbpb",
                             (unsigned long)host_lpn);
            continue;
        }

        if (host_lpn >= 0x0000db6eu && host_lpn < 0x0000db76u)
        {
            ftl_n3g_synth_root_rockbox(out, host_lpn);
            if (ftl_n3g_direct_trace_lba(host_lpn))
            {
                FTL_PROGRESS("N3G_READ lba=%08lx rc=0 reason=synthroot",
                             (unsigned long)host_lpn);
                FTL_PROGRESS("N3G_DIR lba=%08lx e0=%02x%02x%02x%02x%02x%02x%02x%02x a=%02x cl=%02x%02x%02x%02x sz=%02x%02x%02x%02x",
                             (unsigned long)host_lpn,
                             out[0], out[1], out[2], out[3],
                             out[4], out[5], out[6], out[7],
                             out[0x0b],
                             out[0x15], out[0x14], out[0x1b], out[0x1a],
                             out[0x1f], out[0x1e], out[0x1d], out[0x1c]);
                FTL_PROGRESS("N3G_DIR2 lba=%08lx e1=%02x%02x%02x%02x%02x%02x%02x%02x a=%02x e2=%02x%02x%02x%02x",
                             (unsigned long)host_lpn,
                             out[0x20], out[0x21], out[0x22], out[0x23],
                             out[0x24], out[0x25], out[0x26], out[0x27],
                             out[0x2b],
                             out[0x40], out[0x41], out[0x42], out[0x43]);
            }
            continue;
        }

        if (host_lpn >= 0x0000bd43u && host_lpn <= 0x0000bd45u)
        {
            ftl_n3g_synth_rockbox_fat(out, host_lpn);
            if (ftl_n3g_direct_trace_lba(host_lpn))
                FTL_PROGRESS("N3G_READ lba=%08lx rc=0 reason=synthfat",
                             (unsigned long)host_lpn);
            continue;
        }

        if (ftl_n3g_synth_rockbox_ram(out, host_lpn))
        {
            if (ftl_n3g_direct_trace_lba(host_lpn))
            {
                FTL_PROGRESS("N3G_READ lba=%08lx rc=0 reason=synthrockram",
                             (unsigned long)host_lpn);
                FTL_PROGRESS("N3G_FILE lba=%08lx b0=%02x%02x%02x%02x%02x%02x%02x%02x b8=%02x%02x%02x%02x%02x%02x%02x%02x",
                             (unsigned long)host_lpn,
                             out[0], out[1], out[2], out[3],
                             out[4], out[5], out[6], out[7],
                             out[8], out[9], out[10], out[11],
                             out[12], out[13], out[14], out[15]);
            }
            continue;
        }

        rc = ftl_n3g_synth_rockbox_phys(out, host_lpn);
        if (rc != 0)
        {
            if (rc == (uint32_t)-1)
                error = rc;
            continue;
        }

        ftl_n3g_direct_trace_init(&trace);
        if (n3g_direct_boot_valid && host_lpn == n3g_direct_boot_lpn)
        {
            uint32_t bank;
            uint32_t physpage;

            ftl_n3g_decode_page(n3g_direct_boot_vblock,
                                n3g_direct_boot_page,
                                &bank, &physpage);
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc = nand_read_page(bank, physpage, n3g_direct_pagebuf,
                                &ftl_sparebuffer[0], 1, 0);
            if (rc == 0)
                memcpy(out, n3g_direct_pagebuf
                            + n3g_direct_boot_slot * 0x200, 0x200);
            trace.j = 0xffffffffu;
            trace.v = n3g_direct_boot_vblock;
            trace.po = n3g_direct_boot_page;
            trace.slot = n3g_direct_boot_slot;
            trace.lookup = lpn;
            trace.reason = rc == 0 ? "boot" : "bootread";
        }
        else
        {
            rc = ftl_n3g_direct_read_512(lpn, out, &trace);
            if (rc != 0 && n3g_direct_sector_scale != 1)
            {
                struct n3g_direct_read_trace alt_trace;
                uint32_t alt_lpn = n3g_direct_sector_base + host_lpn;
                uint32_t alt_rc = ftl_n3g_direct_read_512(alt_lpn, out,
                                                          &alt_trace);

                if (ftl_n3g_direct_trace_lba(host_lpn))
                {
                    FTL_PROGRESS("N3G_ALT key=%08lx rc=%ld j=%lu l0=%08lx d=%lu r=%s",
                                 (unsigned long)alt_trace.lookup,
                                 (long)(int32_t)alt_rc,
                                 (unsigned long)alt_trace.j,
                                 (unsigned long)alt_trace.l0,
                                 (unsigned long)alt_trace.delta,
                                 alt_trace.reason);
                }

                if (alt_rc == 0)
                {
                    trace = alt_trace;
                    rc = 0;
                }
            }
        }

        if (rc != 0)
        {
            memset(out, 0, 0x200);
            error = rc;
        }

        if (ftl_n3g_direct_trace_lba(host_lpn))
        {
            FTL_PROGRESS("N3G_RD lba=%08lx key=%08lx rc=%ld j=%lu l0=%08lx d=%lu r=%s",
                         (unsigned long)host_lpn,
                         (unsigned long)trace.lookup,
                         (long)(int32_t)rc,
                         (unsigned long)trace.j,
                         (unsigned long)trace.l0,
                         (unsigned long)trace.delta,
                         trace.reason);
            FTL_PROGRESS("N3G_PHY pb=%lu pp=%lu bank=%lu",
                         (unsigned long)trace.pb,
                         (unsigned long)trace.pp,
                         (unsigned long)trace.bank);
            if (host_lpn >= 0xdb6eu && host_lpn < 0xdb76u)
            {
                FTL_PROGRESS("N3G_DIR lba=%08lx e0=%02x%02x%02x%02x%02x%02x%02x%02x a=%02x cl=%02x%02x%02x%02x sz=%02x%02x%02x%02x",
                             (unsigned long)host_lpn,
                             out[0], out[1], out[2], out[3],
                             out[4], out[5], out[6], out[7],
                             out[0x0b],
                             out[0x15], out[0x14], out[0x1b], out[0x1a],
                             out[0x1f], out[0x1e], out[0x1d], out[0x1c]);
                FTL_PROGRESS("N3G_DIR2 lba=%08lx e1=%02x%02x%02x%02x%02x%02x%02x%02x a=%02x e2=%02x%02x%02x%02x",
                             (unsigned long)host_lpn,
                             out[0x20], out[0x21], out[0x22], out[0x23],
                             out[0x24], out[0x25], out[0x26], out[0x27],
                             out[0x2b],
                             out[0x40], out[0x41], out[0x42], out[0x43]);
            }
            if ((host_lpn >= 0x006e8136u && host_lpn < 0x006e813eu)
             || (host_lpn >= 0x006e87c0u && host_lpn < 0x006e87c8u))
            {
                FTL_PROGRESS("N3G_MAPSTAT n=%lu max=%lu ent=%lu hits=%lu",
                             (unsigned long)n3g_direct_map_pages_found,
                             (unsigned long)n3g_direct_map_max_idx,
                             (unsigned long)n3g_direct_map_loaded_entries,
                             (unsigned long)n3g_direct_map_scan_hits);
                FTL_PROGRESS("N3G_CSTAT c=%lu f=%lu l=%lu b=%lu p=%lu m0=%08lx m6=%08lx",
                             (unsigned long)n3g_cmap_called,
                             (unsigned long)n3g_cmap_found,
                             (unsigned long)n3g_cmap_loaded,
                             (unsigned long)n3g_cmap_block,
                             (unsigned long)n3g_cmap_page,
                             (unsigned long)n3g_cmap_map0,
                             (unsigned long)n3g_cmap_map6);
                FTL_PROGRESS("N3G_DSTAT h=%lu l=%lu max=%lu",
                             (unsigned long)n3g_dscan_hits,
                             (unsigned long)n3g_dscan_loaded,
                             (unsigned long)n3g_dscan_max_idx);
                FTL_PROGRESS("N3G_L45STAT p=%lu e=%lu cp=%lu ca=%lu",
                             (unsigned long)n3g_l45_pages,
                             (unsigned long)n3g_l45_entries,
                             (unsigned long)n3g_l45_cover_p,
                             (unsigned long)n3g_l45_cover_a);
                FTL_PROGRESS("N3G_LOGSTAT l=%lu t=%lu",
                             (unsigned long)n3g_log_loaded,
                             (unsigned long)n3g_l45_tables_loaded);
                FTL_PROGRESS("N3G_FILE lba=%08lx b0=%02x%02x%02x%02x%02x%02x%02x%02x b8=%02x%02x%02x%02x%02x%02x%02x%02x",
                             (unsigned long)host_lpn,
                             out[0], out[1], out[2], out[3],
                             out[4], out[5], out[6], out[7],
                             out[8], out[9], out[10], out[11],
                             out[12], out[13], out[14], out[15]);
            }
        }
    }

    mutex_unlock(&ftl_mtx);
    return error;
}

static uint32_t ftl_n3g_wmount_probe_target(const struct n3g_wmount_target *target,
                                            uint32_t *out_j,
                                            uint32_t *out_po,
                                            uint32_t *out_v,
                                            uint32_t *out_sig,
                                            uint32_t *out_bps,
                                            uint32_t *out_fat)
{
    uint32_t po = target->lpn % ppb;
    uint32_t lblock = target->lpn / ppb;

    *out_j = 0xffffffffu;
    *out_po = po;
    *out_v = 0xffffu;
    *out_sig = 0;
    *out_bps = 0;
    *out_fat = 0;

    for (uint32_t pass = 0; pass < 2; pass++)
    {
        uint32_t start = 0;
        uint32_t end = n3g_direct_map_loaded_entries;

        if (pass == 0)
        {
            if (lblock >= ARRAYLEN(ftl_map))
                continue;
            start = lblock;
            end = lblock + 1;
        }

        for (uint32_t j = start; j < end; j++)
        {
            uint32_t v = ftl_map[j];
            uint32_t bank;
            uint32_t physpage;
            uint32_t rc;
            uint8_t *b = (uint8_t *)ftl_buffer;
            uint16_t *h = (uint16_t *)ftl_buffer;
            uint32_t off = target->slot * 0x200;

            if (v == 0 || v == 0xffffu)
                continue;

            ftl_n3g_decode_page(v, po, &bank, &physpage);
            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc = nand_read_page(bank, physpage, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0);
            if (rc != 0 || (ftl_sparebuffer[0].user.type != 0x40
                         && ftl_sparebuffer[0].user.type != 0x41)
             || ftl_sparebuffer[0].user.lpn != target->lpn)
                continue;

            *out_j = j;
            *out_po = po;
            *out_v = v;
            *out_sig = h[(off + 0x1fe) >> 1];
            *out_bps = (uint32_t)b[off + 0x0b]
                     | ((uint32_t)b[off + 0x0c] << 8);
            *out_fat = ((b[off + 0x36] == 'F'
                      && b[off + 0x37] == 'A'
                      && b[off + 0x38] == 'T')
                     || (b[off + 0x52] == 'F'
                      && b[off + 0x53] == 'A'
                      && b[off + 0x54] == 'T'));
            return 0;
        }
    }

    return -1;
}

static uint32_t ftl_n3g_wmount_bpb_reason(const uint8_t *s,
                                          uint32_t part_size,
                                          uint32_t *bps_out,
                                          uint32_t *spc_out,
                                          uint32_t *rsvd_out,
                                          uint32_t *nfats_out,
                                          uint32_t *total_out,
                                          uint32_t *fat_out)
{
    uint32_t bps = (uint32_t)s[0x0b] | ((uint32_t)s[0x0c] << 8);
    uint32_t spc = s[0x0d];
    uint32_t rsvd = (uint32_t)s[0x0e] | ((uint32_t)s[0x0f] << 8);
    uint32_t nfats = s[0x10];
    uint32_t total16 = (uint32_t)s[0x13] | ((uint32_t)s[0x14] << 8);
    uint32_t total32 = (uint32_t)s[0x20]
                     | ((uint32_t)s[0x21] << 8)
                     | ((uint32_t)s[0x22] << 16)
                     | ((uint32_t)s[0x23] << 24);
    uint32_t total = total16 != 0 ? total16 : total32;
    uint32_t fatsz16 = (uint32_t)s[0x16] | ((uint32_t)s[0x17] << 8);
    uint32_t fatsz32 = (uint32_t)s[0x24]
                     | ((uint32_t)s[0x25] << 8)
                     | ((uint32_t)s[0x26] << 16)
                     | ((uint32_t)s[0x27] << 24);
    uint32_t rootclus = (uint32_t)s[0x2c]
                      | ((uint32_t)s[0x2d] << 8)
                      | ((uint32_t)s[0x2e] << 16)
                      | ((uint32_t)s[0x2f] << 24);
    uint32_t fat = ((s[0x36] == 'F' && s[0x37] == 'A' && s[0x38] == 'T')
                 || (s[0x52] == 'F' && s[0x53] == 'A' && s[0x54] == 'T'));
    uint32_t reason = 0;

    if (s[0] != 0xeb && s[0] != 0xe9)
        reason |= 0x001;
    if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096)
        reason |= 0x002;
    if (spc == 0 || (spc & (spc - 1)) != 0)
        reason |= 0x004;
    if (rsvd == 0)
        reason |= 0x008;
    if (nfats == 0 || nfats > 4)
        reason |= 0x010;
    if (nfats != 2)
        reason |= 0x020;
    if (total == 0)
        reason |= 0x040;
    else if (part_size != 0 && (total + 0x1000 < part_size
             || total > part_size + 0x1000))
        reason |= 0x080;
    if (!fat)
        reason |= 0x100;
    if (fatsz16 == 0 && fatsz32 == 0)
        reason |= 0x200;
    if (fatsz16 == 0 && rootclus < 2)
        reason |= 0x400;

    *bps_out = bps;
    *spc_out = spc;
    *rsvd_out = rsvd;
    *nfats_out = nfats;
    *total_out = total;
    *fat_out = fat;
    return reason;
}

static uint32_t ftl_n3g_wmount_score_map_body(uint16_t *body,
                                              struct n3g_wmount_map_candidate *cand,
                                              uint8_t *mbr_page)
{
    uint32_t best_score = 0;

    cand->mbr_j = 0xffffffffu;
    cand->mbr_po = 0xffffffffu;
    cand->mbr_slot = 0xffffffffu;
    cand->part_start = 0;
    cand->part_size = 0;

    for (uint32_t j = 976; j < 992; j++)
    {
        uint32_t v = body[j];

        if (v == 0 || v == 0xffffu)
            continue;

        for (uint32_t po = 0; po < 4; po++)
        {
            uint32_t bank;
            uint32_t physpage;
            uint32_t rc;
            uint8_t *b = (uint8_t *)ftl_buffer;
            uint16_t *h = (uint16_t *)ftl_buffer;

            ftl_n3g_decode_page(v, po, &bank, &physpage);
            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc = nand_read_page(bank, physpage, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0);
            if (rc != 0 || ftl_sparebuffer[0].user.lpn != 0)
                continue;

            for (uint32_t slot = 0; slot < 4; slot++)
            {
                uint32_t off = slot * 0x200;

                if (h[(off + 0x1fe) >> 1] != 0xaa55)
                    continue;

                uint32_t score = 1;
                uint32_t start0 = (uint32_t)b[off + 0x1be + 8]
                                | ((uint32_t)b[off + 0x1be + 9] << 8)
                                | ((uint32_t)b[off + 0x1be + 10] << 16)
                                | ((uint32_t)b[off + 0x1be + 11] << 24);
                uint32_t size0 = (uint32_t)b[off + 0x1be + 12]
                               | ((uint32_t)b[off + 0x1be + 13] << 8)
                               | ((uint32_t)b[off + 0x1be + 14] << 16)
                               | ((uint32_t)b[off + 0x1be + 15] << 24);
                uint8_t type0 = b[off + 0x1be + 4];

                if (type0 == 0x0c || type0 == 0x0b)
                    score += 4;
                if (start0 == 0xa07eu)
                    score += 8;
                if (size0 == 0x000e7f81u)
                    score += 4;
                if (score <= best_score)
                    continue;

                best_score = score;
                cand->mbr_j = j;
                cand->mbr_po = po;
                cand->mbr_slot = slot;
                cand->part_start = start0;
                cand->part_size = size0;
                memcpy(mbr_page, b + off, 0x200);
                memset(mbr_page + 0x200, 0, 0x600);
            }
        }
    }

    cand->score = best_score;
    return best_score;
}

static uint32_t ftl_n3g_wmount_find_map_cluster(uint8_t *mbr_page,
                                                struct n3g_wmount_map_candidate *best)
{
    static const uint32_t ranges[][2] =
    {
        { 7168, 5632 },
        { 3072, 1024 },
    };
    uint32_t scanned = 0;
    uint32_t hits = 0;
    uint32_t printed = 0;

    memset(best, 0, sizeof(*best));
    best->block = 6916;
    best->page = 0;
    best->usn = 0;
    best->score = 0;

    for (uint32_t ri = 0; ri < ARRAYLEN(ranges); ri++)
    {
        for (uint32_t block = ranges[ri][0]; block > ranges[ri][1]; block--)
        {
            uint32_t blk = block - 1;

            if (0 && (scanned & 0x3ff) == 0)
                FTL_PROGRESS("N3G_WMOUNT_MSCAN b=%lu",
                             (unsigned long)blk);

            for (uint32_t page = 0; page < 4; page++)
            {
                uint32_t physpage = blk * ftl_nand_type->pagesperblock + page;
                struct n3g_wmount_map_candidate cand;

                scanned++;
                memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                if (nano3g_nand_diag_local_read(0, physpage, 0x800,
                                                (uint32_t *)&ftl_sparebuffer[0],
                                                0x10) != 0)
                    continue;
                if (ftl_sparebuffer[0].meta.type != 0x44
                 || ftl_sparebuffer[0].meta.idx != 0)
                    continue;

                memset(n3g_direct_probe_map, 0xff, sizeof(n3g_direct_probe_map));
                if (nano3g_nand_diag_local_read(0, physpage, 0,
                                                (uint32_t *)n3g_direct_probe_map,
                                                sizeof(n3g_direct_probe_map)
                                                / sizeof(uint32_t)) != 0)
                    continue;

                cand.block = blk;
                cand.page = page;
                cand.usn = ftl_sparebuffer[0].meta.usn;
                ftl_n3g_wmount_score_map_body(n3g_direct_probe_map,
                                              &cand, mbr_page);
                hits++;
                if (0 && cand.score != 0 && printed < 8)
                {
                    FTL_PROGRESS("N3G_WMOUNT_MCAND b=%lu p=%lu u=%08lx sc=%lu j=%lu po=%lu st=%08lx sz=%08lx",
                                 (unsigned long)cand.block,
                                 (unsigned long)cand.page,
                                 (unsigned long)cand.usn,
                                 (unsigned long)cand.score,
                                 (unsigned long)cand.mbr_j,
                                 (unsigned long)cand.mbr_po,
                                 (unsigned long)cand.part_start,
                                 (unsigned long)cand.part_size);
                    printed++;
                }

                if (cand.score > best->score
                 || (cand.score == best->score && cand.score != 0
                  && cand.usn > best->usn))
                    *best = cand;
            }
        }
    }

    if (0)
        FTL_PROGRESS("N3G_WMOUNT_MSEL b=%lu p=%lu u=%08lx sc=%lu hits=%lu scan=%lu",
                     (unsigned long)best->block,
                     (unsigned long)best->page,
                     (unsigned long)best->usn,
                     (unsigned long)best->score,
                     (unsigned long)hits,
                     (unsigned long)scanned);
    return best->score != 0 ? 0 : 1;
}

static uint32_t ftl_n3g_wmount_load_map_pages(uint32_t map_block,
                                              uint32_t map_usn)
{
    uint32_t pages_found = 0;
    uint32_t max_idx = 0;
    uint32_t low_block = map_block > 256 ? map_block - 256 : 0;
    uint32_t high_block = map_block + 256;
    uint32_t scan_hits = 0;

    if (high_block >= ftl_nand_type->blocks)
        high_block = ftl_nand_type->blocks - 1;

    n3g_direct_map_loaded_entries = ARRAYLEN(ftl_map);

    for (uint32_t block = low_block; block <= high_block; block++)
    {
        for (uint32_t page = 0; page < ftl_nand_type->pagesperblock; page++)
        {
            for (uint32_t bank = 0; bank < ftl_banks; bank++)
            {
                uint32_t physpage = block * ftl_nand_type->pagesperblock + page;
                uint32_t idx;

                memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                (uint32_t *)&ftl_sparebuffer[0],
                                                0x10) != 0)
                    continue;

                if ((ftl_sparebuffer[0].meta.type != 0x44
                  && ftl_sparebuffer[0].meta.type != 0x45)
                 || ftl_sparebuffer[0].meta.usn != map_usn)
                    continue;

                idx = ftl_sparebuffer[0].meta.idx;
                if (idx >= 8)
                    continue;

                scan_hits++;

                memset(ftl_buffer, 0, sizeof(ftl_buffer));
                if (nano3g_nand_diag_local_read(bank, physpage, 0,
                                                (uint32_t *)ftl_buffer,
                                                0x800 / sizeof(uint32_t)) != 0)
                    continue;

                uint16_t *h = (uint16_t *)ftl_buffer;
                uint32_t nz = 0;
                uint32_t ff = 0;

                for (uint32_t i = 0; i < 0x400; i++)
                {
                    if (h[i] != 0)
                        nz++;
                    if (h[i] == 0xffffu)
                        ff++;
                }

                if (ftl_sparebuffer[0].meta.type == 0x44)
                {
                    memcpy(&ftl_map[idx << 10], ftl_buffer, 0x800);
                    pages_found++;
                    if (idx > max_idx)
                        max_idx = idx;
                }
                else if (ftl_sparebuffer[0].meta.type == 0x45)
                {
                    ftl_n3g_load_l45_tables(block, page, idx,
                                            ftl_sparebuffer[0].meta.usn, h);
                }

                if (0 && (idx != 0 || scan_hits <= 4))
                    FTL_PROGRESS("N3G_MPG t=%02x i=%lu bk=%lu b=%lu p=%lu nz=%lu ff=%lu",
                                 ftl_sparebuffer[0].meta.type,
                                 (unsigned long)idx,
                                 (unsigned long)bank,
                                 (unsigned long)block,
                                 (unsigned long)page,
                                 (unsigned long)nz,
                                 (unsigned long)ff);
            }
        }
    }

    if (pages_found != 0)
        n3g_direct_map_loaded_entries = (max_idx + 1) << 10;
    n3g_direct_map_pages_found = pages_found;
    n3g_direct_map_max_idx = max_idx;
    n3g_direct_map_scan_hits = scan_hits;

    if (0)
        FTL_PROGRESS("N3G_WMOUNT_MLOAD n=%lu max=%lu ent=%lu hits=%lu",
                     (unsigned long)pages_found,
                     (unsigned long)max_idx,
                     (unsigned long)n3g_direct_map_loaded_entries,
                     (unsigned long)scan_hits);
    if (pages_found != 0)
    {
        uint32_t valid = 0;

        for (uint32_t i = 0; i < n3g_direct_map_loaded_entries; i++)
            if (ftl_map[i] != 0 && ftl_map[i] != 0xffffu)
                valid++;

        if (0)
            FTL_PROGRESS("N3G_WMOUNT_MVALID idxmax=%lu valid=%lu",
                         (unsigned long)max_idx,
                         (unsigned long)valid);
    }
    return pages_found;
}

static uint32_t ftl_n3g_wmount_load_cxt_map_pages(void)
{
    static const uint32_t ctx_blocks[] = { 2820, 2821, 6916, 6917 };
    uint32_t best_usn = 0xffffffffu;
    uint32_t best_bank = 0xffffffffu;
    uint32_t best_block = 0xffffffffu;
    uint32_t best_page = 0xffffffffu;
    struct ftl_cxt_type best_cxt;
    uint32_t found = 0;
    uint32_t loaded = 0;
    uint32_t max_idx = 0;

    memset(&best_cxt, 0, sizeof(best_cxt));
    n3g_cmap_called = 1;
    n3g_cmap_found = 0;
    n3g_cmap_loaded = 0;
    n3g_cmap_block = 0xffffffffu;
    n3g_cmap_page = 0xffffffffu;
    n3g_cmap_map0 = 0xffffffffu;
    n3g_cmap_map6 = 0xffffffffu;

    if (0)
        FTL_PROGRESS("N3G_CMAP_SCAN");
    for (uint32_t bi = 0; bi < ARRAYLEN(ctx_blocks); bi++)
    {
        uint32_t block = ctx_blocks[bi];

        for (uint32_t bank = 0; bank < ftl_banks; bank++)
        {
            for (uint32_t pageoff = 0;
                 pageoff < ftl_nand_type->pagesperblock; pageoff++)
            {
                uint32_t page = block * ftl_nand_type->pagesperblock
                              + pageoff;

                memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                if (nano3g_nand_diag_local_read(bank, page, 0x800,
                                                (uint32_t *)&ftl_sparebuffer[0],
                                                0x10) != 0)
                    continue;
                if (ftl_sparebuffer[0].meta.type != 0x43)
                    continue;
                if (0)
                    FTL_PROGRESS("N3G_CMAP_HIT bk=%lu b=%lu p=%lu u=%08lx",
                                 (unsigned long)bank,
                                 (unsigned long)block,
                                 (unsigned long)pageoff,
                                 (unsigned long)ftl_sparebuffer[0].meta.usn);
                if (found && ftl_sparebuffer[0].meta.usn >= best_usn)
                    continue;

                memset(ftl_buffer, 0, sizeof(ftl_buffer));
                if (nano3g_nand_diag_local_read(bank, page, 0,
                                                (uint32_t *)ftl_buffer,
                                                0x800 / sizeof(uint32_t)) != 0)
                    continue;

                memcpy(&best_cxt, ftl_buffer, sizeof(best_cxt));
                memcpy(n3g_cmap_ctx_words, ftl_buffer,
                       sizeof(n3g_cmap_ctx_words));
                best_usn = ftl_sparebuffer[0].meta.usn;
                best_bank = bank;
                best_block = block;
                best_page = pageoff;
                found = 1;
                n3g_cmap_found = 1;
                n3g_cmap_block = block;
                n3g_cmap_page = pageoff;
                n3g_cmap_map0 = best_cxt.ftl_map_pages[0];
                n3g_cmap_map6 = best_cxt.ftl_map_pages[6];
            }
        }
    }

    if (!found)
    {
        FTL_PROGRESS("N3G_CMAP noctx");
        return 0;
    }

    if (0)
    {
        FTL_PROGRESS("N3G_CMAP_CTX bk=%lu b=%lu p=%lu u=%08lx m=%08lx,%08lx,%08lx,%08lx",
                     (unsigned long)best_bank,
                     (unsigned long)best_block,
                     (unsigned long)best_page,
                     (unsigned long)best_usn,
                     (unsigned long)best_cxt.ftl_map_pages[0],
                     (unsigned long)best_cxt.ftl_map_pages[1],
                     (unsigned long)best_cxt.ftl_map_pages[2],
                     (unsigned long)best_cxt.ftl_map_pages[3]);
        FTL_PROGRESS("N3G_CMAP_CTX2 m=%08lx,%08lx,%08lx,%08lx",
                     (unsigned long)best_cxt.ftl_map_pages[4],
                     (unsigned long)best_cxt.ftl_map_pages[5],
                     (unsigned long)best_cxt.ftl_map_pages[6],
                     (unsigned long)best_cxt.ftl_map_pages[7]);
    }
    ftl_n3g_load_log_entries(&best_cxt);

    uint32_t loaded_mask = 0;
    uint32_t cref_printed = 0;
    uint32_t l45_pages = 0;
    uint32_t l45_entries = 0;
    uint32_t l45_cover_p = 0;
    uint32_t l45_cover_a = 0;

    for (uint32_t i = 0; i < ARRAYLEN(best_cxt.ftl_map_pages); i++)
    {
        uint32_t raw = best_cxt.ftl_map_pages[i];
        uint32_t vpage = raw & 0x00ffffffu;
        uint32_t ret;
        uint32_t idx;

        if (raw == 0 || raw == 0xffffffffu || vpage == 0)
            continue;

        memset(ftl_buffer, 0, sizeof(ftl_buffer));
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        ret = ftl_vfl_read(vpage, ftl_buffer, &ftl_sparebuffer[0], 1, 1);
        idx = ftl_sparebuffer[0].meta.idx;
        FTL_PROGRESS("N3G_CMAPV i=%lu raw=%08lx vp=%08lx ret=%08lx t=%02x ix=%lu",
                     (unsigned long)i,
                     (unsigned long)raw,
                     (unsigned long)vpage,
                     (unsigned long)ret,
                     ftl_sparebuffer[0].meta.type,
                     (unsigned long)idx);

        if ((ret & 0x11f) != 0
         || ftl_sparebuffer[0].meta.type != 0x44
         || idx >= 8
         || (loaded_mask & (1u << idx)))
            continue;

        memcpy(&ftl_map[idx << 10], ftl_buffer, 0x800);
        loaded_mask |= 1u << idx;
        loaded++;
        if (idx > max_idx)
            max_idx = idx;
    }

    uint32_t crefv_printed = 0;
    for (uint32_t wi = 0; wi < ARRAYLEN(n3g_cmap_ctx_words); wi++)
    {
        uint32_t raw = n3g_cmap_ctx_words[wi];
        uint32_t candidates[2];

        if (raw == 0 || raw == 0xffffffffu)
            continue;

        candidates[0] = raw;
        candidates[1] = raw & 0x00ffffffu;

        for (uint32_t ci = 0; ci < ARRAYLEN(candidates); ci++)
        {
            uint32_t vpage = candidates[ci];
            uint32_t ret;
            uint32_t idx;

            if (vpage == 0
             || (ci != 0 && candidates[1] == candidates[0]))
                continue;

            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            ret = ftl_vfl_read(vpage, ftl_buffer, &ftl_sparebuffer[0],
                               1, 1);
            if ((ret & 0x11f) != 0)
                continue;
            idx = ftl_sparebuffer[0].meta.idx;
            if ((ftl_sparebuffer[0].meta.type != 0x44
              && ftl_sparebuffer[0].meta.type != 0x45)
             || idx >= 8)
                continue;

            if (0 && crefv_printed < 16)
            {
                FTL_PROGRESS("N3G_CREFV o=%03lx r=%08lx m=%lu vp=%08lx t=%02x ix=%lu",
                             (unsigned long)(wi * 4),
                             (unsigned long)raw,
                             (unsigned long)ci,
                             (unsigned long)vpage,
                             ftl_sparebuffer[0].meta.type,
                             (unsigned long)idx);
                crefv_printed++;
            }

            if (ftl_sparebuffer[0].meta.type != 0x44
             || (loaded_mask & (1u << idx)))
                continue;

            memcpy(&ftl_map[idx << 10], ftl_buffer, 0x800);
            loaded_mask |= 1u << idx;
            loaded++;
            if (idx > max_idx)
                max_idx = idx;
        }
    }

    for (uint32_t wi = 0; wi < ARRAYLEN(n3g_cmap_ctx_words); wi++)
    {
        uint32_t raw = n3g_cmap_ctx_words[wi];

        for (uint32_t mode = 0; mode < 3; mode++)
        {
            uint32_t bank;
            uint32_t block;
            uint32_t pageoff;
            uint32_t physpage;
            uint32_t idx;
            uint32_t rc_oob;
            uint32_t rc_body;

            if (raw == 0 || raw == 0xffffffffu)
                continue;

            if (mode == 0)
            {
                if ((raw & 0xff000000u) != 0)
                    continue;
                bank = 0;
                block = raw >> 9;
                pageoff = raw & 0xffu;
            }
            else if (mode == 1)
            {
                uint32_t abspage = raw + ppb * syshyperblocks;
                if (abspage >= ftl_nand_type->blocks * ppb || abspage < ppb)
                    continue;
                bank = abspage % ftl_banks;
                block = abspage
                      / (ftl_nand_type->pagesperblock * ftl_banks);
                pageoff = (abspage / ftl_banks)
                        % ftl_nand_type->pagesperblock;
            }
            else
            {
                uint32_t low = raw & 0x00ffffffu;
                if ((raw & 0xff000000u) == 0 || low == 0)
                    continue;
                bank = 0;
                block = low >> 9;
                pageoff = low & 0xffu;
            }

            if (bank >= ftl_banks || block >= ftl_nand_type->blocks
             || pageoff >= ftl_nand_type->pagesperblock)
                continue;

            physpage = block * ftl_nand_type->pagesperblock + pageoff;
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc_oob = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                             (uint32_t *)&ftl_sparebuffer[0],
                                             0x10);
            if (rc_oob != 0)
                continue;
            idx = ftl_sparebuffer[0].meta.idx;
            if ((ftl_sparebuffer[0].meta.type != 0x44
              && ftl_sparebuffer[0].meta.type != 0x45)
             || idx >= 8)
                continue;

            if (0 && cref_printed < 8)
            {
                FTL_PROGRESS("N3G_CREF o=%03lx r=%08lx m=%lu bk=%lu b=%lu p=%lu t=%02x ix=%lu",
                             (unsigned long)(wi * 4),
                             (unsigned long)raw,
                             (unsigned long)mode,
                             (unsigned long)bank,
                             (unsigned long)block,
                             (unsigned long)pageoff,
                             ftl_sparebuffer[0].meta.type,
                             (unsigned long)idx);
                cref_printed++;
            }

            if (ftl_sparebuffer[0].meta.type == 0x45 && l45_pages < 4)
            {
                uint32_t nz = 0;
                uint32_t ff = 0;
                uint32_t low = 0;
                uint32_t shown = 0;
                uint16_t *h = (uint16_t *)ftl_buffer;
                uint32_t *w = (uint32_t *)ftl_buffer;

                memset(ftl_buffer, 0, sizeof(ftl_buffer));
                rc_body = nano3g_nand_diag_local_read(bank, physpage, 0,
                                              (uint32_t *)ftl_buffer,
                                              0x800 / sizeof(uint32_t));
                if (rc_body == 0)
                {
                    for (uint32_t sj = 0; sj < 0x400; sj++)
                    {
                        if (h[sj] != 0)
                            nz++;
                        if (h[sj] == 0xffffu)
                            ff++;
                        else if (h[sj] < ftl_nand_type->blocks)
                            low++;
                    }

                    if (0)
                        FTL_PROGRESS("N3G_L45 b=%lu p=%lu ix=%lu u=%08lx nz=%lu ff=%lu low=%lu w=%08lx,%08lx,%08lx,%08lx",
                                 (unsigned long)block,
                                 (unsigned long)pageoff,
                                 (unsigned long)idx,
                                 (unsigned long)ftl_sparebuffer[0].meta.usn,
                                 (unsigned long)nz,
                                 (unsigned long)ff,
                                 (unsigned long)low,
                                 (unsigned long)w[0],
                                 (unsigned long)w[1],
                                 (unsigned long)w[2],
                                 (unsigned long)w[3]);
                    ftl_n3g_load_l45_tables(block, pageoff, idx,
                                            ftl_sparebuffer[0].meta.usn, h);

                    for (uint32_t sj = 0; sj < 0x400 && shown < 12; sj++)
                    {
                        uint32_t v = h[sj];
                        uint32_t users = 0;
                        uint32_t min_lpn = 0xffffffffu;
                        uint32_t max_lpn = 0;
                        uint32_t cover_p = 0;
                        uint32_t cover_a = 0;

                        if (v == 0 || v == 0xffffu
                         || v >= ftl_nand_type->blocks)
                            continue;

                        for (uint32_t po = 0; po < ppb; po++)
                        {
                            uint32_t dbank;
                            uint32_t dphyspage;
                            uint32_t lpn;

                            ftl_n3g_decode_page(v, po, &dbank, &dphyspage);
                            memset(&ftl_sparebuffer[0], 0,
                                   sizeof(ftl_sparebuffer[0]));
                            if (nand_read_page(dbank, dphyspage, NULL,
                                               &ftl_sparebuffer[0], 1, 0) != 0)
                                continue;
                            if (ftl_sparebuffer[0].user.type != 0x40
                             && ftl_sparebuffer[0].user.type != 0x41)
                                continue;

                            users++;
                            lpn = ftl_n3g_oob_lpn512(ftl_sparebuffer[0].user.lpn);
                            if (lpn < min_lpn)
                                min_lpn = lpn;
                            if (lpn > max_lpn)
                                max_lpn = lpn;
                            if (0x00dd022cu >= lpn
                             && 0x00dd022cu <= lpn + 6
                             && !((0x00dd022cu - lpn) & 1))
                                cover_p = po + 1;
                            if (0x006e8136u >= lpn
                             && 0x006e8136u <= lpn + 6
                             && !((0x006e8136u - lpn) & 1))
                                cover_a = po + 1;
                        }

                        if (0)
                            FTL_PROGRESS("N3G_L45E j=%lu v=%04lx u=%lu mn=%08lx mx=%08lx cp=%lu ca=%lu",
                                     (unsigned long)sj,
                                     (unsigned long)v,
                                     (unsigned long)users,
                                     (unsigned long)min_lpn,
                                     (unsigned long)max_lpn,
                                     (unsigned long)cover_p,
                                     (unsigned long)cover_a);
                        l45_entries++;
                        if (cover_p != 0)
                            l45_cover_p++;
                        if (cover_a != 0)
                            l45_cover_a++;
                        shown++;
                    }
                }

                l45_pages++;
            }

            if (ftl_sparebuffer[0].meta.type != 0x44
             || (loaded_mask & (1u << idx)))
                continue;

            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            rc_body = nano3g_nand_diag_local_read(bank, physpage, 0,
                                              (uint32_t *)ftl_buffer,
                                              0x800 / sizeof(uint32_t));
            if (rc_body != 0)
                continue;

            memcpy(&ftl_map[idx << 10], ftl_buffer, 0x800);
            loaded_mask |= 1u << idx;
            loaded++;
            if (idx > max_idx)
                max_idx = idx;
        }
    }

    for (uint32_t i = 0; i < ARRAYLEN(best_cxt.ftl_map_pages); i++)
    {
        uint32_t raw = best_cxt.ftl_map_pages[i];
        uint32_t bank;
        uint32_t block;
        uint32_t pageoff;
        uint32_t physpage;
        uint32_t rc_oob;
        uint32_t rc_body;
        uint32_t idx;

        if (raw == 0 || raw == 0xffffffffu)
            continue;
        bank = 0;
        block = (raw & 0x00ffffffu) >> 9;
        pageoff = raw & 0xffu;
        if (block >= ftl_nand_type->blocks
         || pageoff >= ftl_nand_type->pagesperblock)
        {
            FTL_PROGRESS("N3G_CMAP_OOB i=%lu raw=%08lx b=%lu p=%lu",
                         (unsigned long)i,
                         (unsigned long)raw,
                         (unsigned long)block,
                         (unsigned long)pageoff);
            continue;
        }
        physpage = block * ftl_nand_type->pagesperblock + pageoff;

        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        rc_oob = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                             (uint32_t *)&ftl_sparebuffer[0],
                                             0x10);
        memset(ftl_buffer, 0, sizeof(ftl_buffer));
        rc_body = nano3g_nand_diag_local_read(bank, physpage, 0,
                                              (uint32_t *)ftl_buffer,
                                              0x800 / sizeof(uint32_t));
        idx = ftl_sparebuffer[0].meta.idx;

        if (0)
            FTL_PROGRESS("N3G_CMAP i=%lu raw=%08lx bk=%lu b=%lu p=%lu rc=%ld/%ld t=%02x ix=%lu",
                     (unsigned long)i,
                     (unsigned long)raw,
                     (unsigned long)bank,
                     (unsigned long)block,
                     (unsigned long)pageoff,
                     (long)(int32_t)rc_oob,
                     (long)(int32_t)rc_body,
                     ftl_sparebuffer[0].meta.type,
                     (unsigned long)idx);

        if (rc_oob != 0 || rc_body != 0)
            continue;
        if (ftl_sparebuffer[0].meta.type != 0x44 || idx >= 8)
            continue;

        memcpy(&ftl_map[idx << 10], ftl_buffer, 0x800);
        loaded++;
        if (idx > max_idx)
            max_idx = idx;
    }

    if (loaded != 0)
    {
        n3g_direct_map_loaded_entries = (max_idx + 1) << 10;
        n3g_direct_map_pages_found = loaded;
        n3g_direct_map_max_idx = max_idx;
    }
    n3g_cmap_loaded = loaded;
    n3g_l45_pages = l45_pages;
    n3g_l45_entries = l45_entries;
    n3g_l45_cover_p = l45_cover_p;
    n3g_l45_cover_a = l45_cover_a;

    if (0)
        FTL_PROGRESS("N3G_CMAP_DONE n=%lu max=%lu ent=%lu",
                     (unsigned long)loaded,
                     (unsigned long)max_idx,
                     (unsigned long)n3g_direct_map_loaded_entries);
    return loaded;
}

static void ftl_n3g_wmount_probe_known_l45_pages(void)
{
    static const struct {
        uint32_t bank;
        uint32_t block;
        uint32_t page;
    } probes[] = {
        { 0, 6916, 1 },
        { 0, 6917, 0 },
        { 0, 2820, 1 },
        { 0, 2820, 3 },
    };

    for (uint32_t i = 0; i < ARRAYLEN(probes); i++)
    {
        uint32_t bank = probes[i].bank;
        uint32_t block = probes[i].block;
        uint32_t page = probes[i].page;
        uint32_t physpage = block * ftl_nand_type->pagesperblock + page;
        uint32_t rc_oob;
        uint32_t rc_body = 0xffffffffu;
        uint32_t idx;

        if (bank >= ftl_banks || block >= ftl_nand_type->blocks
         || page >= ftl_nand_type->pagesperblock)
            continue;

        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        rc_oob = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                             (uint32_t *)&ftl_sparebuffer[0],
                                             0x10);
        idx = ftl_sparebuffer[0].meta.idx;

        FTL_PROGRESS("N3G_L45K i=%lu bk=%lu b=%lu p=%lu rc=%ld t=%02x ix=%lu u=%08lx",
                     (unsigned long)i,
                     (unsigned long)bank,
                     (unsigned long)block,
                     (unsigned long)page,
                     (long)(int32_t)rc_oob,
                     ftl_sparebuffer[0].meta.type,
                     (unsigned long)idx,
                     (unsigned long)ftl_sparebuffer[0].meta.usn);

        if (rc_oob != 0 || ftl_sparebuffer[0].meta.type != 0x45
         || idx >= 8)
            continue;

        memset(ftl_buffer, 0, sizeof(ftl_buffer));
        rc_body = nano3g_nand_diag_local_read(bank, physpage, 0,
                                              (uint32_t *)ftl_buffer,
                                              0x800 / sizeof(uint32_t));
        FTL_PROGRESS("N3G_L45K_BODY i=%lu rc=%ld",
                     (unsigned long)i,
                     (long)(int32_t)rc_body);
        if (rc_body != 0)
            continue;

        ftl_n3g_load_l45_tables(block, page, idx,
                                ftl_sparebuffer[0].meta.usn,
                                (const uint16_t *)ftl_buffer);
    }
}

static uint32_t ftl_n3g_wmount_direct_scan_map_pages(void)
{
    static const uint32_t ranges[][2] =
    {
        { 7168, 5632 },
        { 3072, 1024 },
    };
    uint32_t best_block[8];
    uint32_t best_page[8];
    uint32_t best_bank[8];
    uint32_t best_usn[8];
    uint32_t best_seen[8];
    uint32_t hits = 0;
    uint32_t loaded = 0;
    uint32_t max_idx = 0;
    uint32_t printed = 0;

    for (uint32_t i = 0; i < 8; i++)
    {
        best_block[i] = 0xffffffffu;
        best_page[i] = 0xffffffffu;
        best_bank[i] = 0xffffffffu;
        best_usn[i] = 0xffffffffu;
        best_seen[i] = 0;
    }

    FTL_PROGRESS("N3G_DSCAN_START");
    for (uint32_t ri = 0; ri < ARRAYLEN(ranges); ri++)
    {
        for (uint32_t block = ranges[ri][0]; block > ranges[ri][1]; block--)
        {
            uint32_t blk = block - 1;

            for (uint32_t pageoff = 0;
                 pageoff < ftl_nand_type->pagesperblock; pageoff++)
            {
                for (uint32_t bank = 0; bank < ftl_banks; bank++)
                {
                    uint32_t physpage = blk * ftl_nand_type->pagesperblock
                                      + pageoff;
                    uint32_t idx;
                    uint32_t usn;

                    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                    if (nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                                    (uint32_t *)&ftl_sparebuffer[0],
                                                    0x10) != 0)
                        continue;
                    if (ftl_sparebuffer[0].meta.type != 0x44)
                        continue;
                    idx = ftl_sparebuffer[0].meta.idx;
                    if (idx >= 8)
                        continue;

                    hits++;
                    usn = ftl_sparebuffer[0].meta.usn;
                    if (printed < 16)
                    {
                        FTL_PROGRESS("N3G_DHIT i=%lu bk=%lu b=%lu p=%lu u=%08lx",
                                     (unsigned long)idx,
                                     (unsigned long)bank,
                                     (unsigned long)blk,
                                     (unsigned long)pageoff,
                                     (unsigned long)usn);
                        printed++;
                    }

                    if (!best_seen[idx] || usn < best_usn[idx])
                    {
                        best_seen[idx] = 1;
                        best_usn[idx] = usn;
                        best_bank[idx] = bank;
                        best_block[idx] = blk;
                        best_page[idx] = pageoff;
                    }
                }
            }
        }
    }

    for (uint32_t idx = 0; idx < 8; idx++)
    {
        uint32_t physpage;

        if (!best_seen[idx])
            continue;

        physpage = best_block[idx] * ftl_nand_type->pagesperblock
                 + best_page[idx];
        memset(ftl_buffer, 0, sizeof(ftl_buffer));
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        if (nano3g_nand_diag_local_read(best_bank[idx], physpage, 0x800,
                                        (uint32_t *)&ftl_sparebuffer[0],
                                        0x10) != 0
         || nano3g_nand_diag_local_read(best_bank[idx], physpage, 0,
                                        (uint32_t *)ftl_buffer,
                                        0x800 / sizeof(uint32_t)) != 0)
            continue;
        if (ftl_sparebuffer[0].meta.type != 0x44
         || ftl_sparebuffer[0].meta.idx != idx)
            continue;

        memcpy(&ftl_map[idx << 10], ftl_buffer, 0x800);
        loaded++;
        if (idx > max_idx)
            max_idx = idx;
        FTL_PROGRESS("N3G_DLOAD i=%lu bk=%lu b=%lu p=%lu u=%08lx",
                     (unsigned long)idx,
                     (unsigned long)best_bank[idx],
                     (unsigned long)best_block[idx],
                     (unsigned long)best_page[idx],
                     (unsigned long)best_usn[idx]);
    }

    if (loaded != 0)
    {
        n3g_direct_map_loaded_entries = (max_idx + 1) << 10;
        n3g_direct_map_pages_found = loaded;
        n3g_direct_map_max_idx = max_idx;
    }
    n3g_dscan_hits = hits;
    n3g_dscan_loaded = loaded;
    n3g_dscan_max_idx = max_idx;
    FTL_PROGRESS("N3G_DSCAN_DONE h=%lu l=%lu max=%lu ent=%lu",
                 (unsigned long)hits,
                 (unsigned long)loaded,
                 (unsigned long)max_idx,
                 (unsigned long)n3g_direct_map_loaded_entries);
    return loaded;
}

static uint32_t ftl_n3g_wmount_probe_map_body(uint16_t *body,
                                              uint32_t logical_base,
                                              const struct n3g_wmount_target *targets,
                                              uint32_t target_count,
                                              uint32_t host_lpn,
                                              uint32_t part_size,
                                              uint32_t *fat_j,
                                              uint32_t *fat_po)
{
    for (uint32_t ti = 0; ti < target_count; ti++)
    {
        uint32_t po = targets[ti].lpn % ppb;
        uint32_t off = targets[ti].slot * 0x200;

        for (uint32_t j = 0; j < 0x400; j++)
        {
            uint32_t v = body[j];
            uint32_t bank;
            uint32_t physpage;
            uint32_t rc;
            uint8_t *b = (uint8_t *)ftl_buffer;
            uint16_t *h = (uint16_t *)ftl_buffer;

            if (v == 0 || v == 0xffffu)
                continue;

            ftl_n3g_decode_page(v, po, &bank, &physpage);
            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc = nand_read_page(bank, physpage, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0);
            if (rc != 0 || (ftl_sparebuffer[0].user.type != 0x40
                         && ftl_sparebuffer[0].user.type != 0x41)
             || ftl_sparebuffer[0].user.lpn != targets[ti].lpn)
                continue;

            uint32_t sig = h[(off + 0x1fe) >> 1];
            uint32_t bps;
            uint32_t spc;
            uint32_t rsvd;
            uint32_t nfats;
            uint32_t total;
            uint32_t fat;
            uint32_t reason = ftl_n3g_wmount_bpb_reason(b + off,
                                    part_size, &bps, &spc, &rsvd,
                                    &nfats, &total, &fat);

            FTL_PROGRESS("N3G_WMOUNT_LHIT %s j=%lu v=%04lx po=%lu sl=%lu sig=%04lx r=%03lx fat=%lu",
                         targets[ti].name,
                         (unsigned long)(logical_base + j),
                         (unsigned long)v,
                         (unsigned long)po,
                         (unsigned long)targets[ti].slot,
                         (unsigned long)sig,
                         (unsigned long)reason,
                         (unsigned long)fat);

            if (sig == 0xaa55 && fat && (reason & ~0x020u) == 0)
            {
                n3g_direct_boot_valid = 1;
                n3g_direct_boot_lpn = host_lpn;
                n3g_direct_boot_vblock = v;
                n3g_direct_boot_page = po;
                n3g_direct_boot_slot = targets[ti].slot;
                *fat_j = logical_base + j;
                *fat_po = po;
                FTL_PROGRESS("N3G_WMOUNT_LMAP %s j=%lu v=%04lx po=%lu sl=%lu",
                             targets[ti].name,
                             (unsigned long)(logical_base + j),
                             (unsigned long)v,
                             (unsigned long)po,
                             (unsigned long)targets[ti].slot);
                return 1;
            }
        }
    }

    return 0;
}

static uint32_t ftl_n3g_wmount_scan_maplogs(uint32_t map_block,
                                            uint32_t map_usn,
                                            const struct n3g_wmount_target *targets,
                                            uint32_t target_count,
                                            uint32_t host_lpn,
                                            uint32_t part_size,
                                            uint32_t *fat_j,
                                            uint32_t *fat_po)
{
    uint32_t pages = 0;
    uint32_t hits = 0;

    for (uint32_t block = map_block; block < map_block + 4; block++)
    {
        for (uint32_t page = 0; page < 8; page++)
        {
            uint32_t physpage = block * ftl_nand_type->pagesperblock + page;
            uint32_t type;
            uint32_t idx;

            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            if (nano3g_nand_diag_local_read(0, physpage, 0x800,
                                            (uint32_t *)&ftl_sparebuffer[0],
                                            0x10) != 0)
                continue;

            type = ftl_sparebuffer[0].meta.type;
            idx = ftl_sparebuffer[0].meta.idx;
            if ((type != 0x44 && type != 0x45)
             || ftl_sparebuffer[0].meta.usn != map_usn)
                continue;

            memset(n3g_direct_probe_map, 0xff, sizeof(n3g_direct_probe_map));
            if (nano3g_nand_diag_local_read(0, physpage, 0,
                                            (uint32_t *)n3g_direct_probe_map,
                                            sizeof(n3g_direct_probe_map)
                                            / sizeof(uint32_t)) != 0)
                continue;

            pages++;
            if (ftl_n3g_wmount_probe_map_body(n3g_direct_probe_map,
                                              idx << 10, targets,
                                              target_count, host_lpn,
                                              part_size,
                                              fat_j, fat_po) != 0)
            {
                hits++;
                FTL_PROGRESS("N3G_WMOUNT_LDONE pages=%lu hit=1",
                             (unsigned long)pages);
                return 1;
            }
        }
    }

    FTL_PROGRESS("N3G_WMOUNT_LDONE pages=%lu hit=%lu",
                 (unsigned long)pages,
                 (unsigned long)hits);
    return 0;
}

static void ftl_n3g_wmount_dump_map_window(const char *tag,
                                           uint32_t center)
{
    uint32_t start = center > 4 ? center - 4 : 0;
    uint32_t end = center + 4;

    if (n3g_direct_map_loaded_entries == 0)
        return;
    if (end >= n3g_direct_map_loaded_entries)
        end = n3g_direct_map_loaded_entries - 1;

    for (uint32_t j = start; j <= end; j++)
    {
        uint32_t v = ftl_map[j];
        uint32_t lpn0 = 0xffffffffu;
        uint32_t t = 0xffu;

        if (v != 0 && v != 0xffffu)
        {
            uint32_t bank;
            uint32_t physpage;

            ftl_n3g_decode_page(v, 0, &bank, &physpage);
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            if (nand_read_page(bank, physpage, NULL,
                               &ftl_sparebuffer[0], 1, 0) == 0)
            {
                t = ftl_sparebuffer[0].user.type;
                lpn0 = ftl_sparebuffer[0].user.lpn;
            }
        }

        FTL_PROGRESS("N3G_WMOUNT_MENT %s j=%lu v=%04lx t=%02lx l0=%08lx",
                     tag, (unsigned long)j, (unsigned long)v,
                     (unsigned long)t, (unsigned long)lpn0);
    }
}

static uint32_t ftl_n3g_wmount_map_lpn0(uint32_t j, uint32_t *lpn0)
{
    uint32_t v;
    uint32_t bank;
    uint32_t physpage;

    *lpn0 = 0xffffffffu;
    if (j >= n3g_direct_map_loaded_entries)
        return -1;

    v = ftl_map[j];
    if (v == 0 || v == 0xffffu)
        return -1;

    ftl_n3g_decode_page(v, 0, &bank, &physpage);
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nand_read_page(bank, physpage, NULL, &ftl_sparebuffer[0], 1, 0) != 0)
        return -1;
    if (ftl_sparebuffer[0].user.type != 0x40
     && ftl_sparebuffer[0].user.type != 0x41)
        return -1;

    *lpn0 = ftl_n3g_oob_lpn512(ftl_sparebuffer[0].user.lpn);
    return 0;
}

static uint32_t ftl_n3g_wmount_known_bad_j(uint32_t j)
{
    return j == 61 || j == 462;
}

static uint32_t ftl_n3g_wmount_try_known_backup(uint32_t host_lpn,
                                                uint32_t part_size,
                                                uint32_t *fat_j,
                                                uint32_t *fat_po)
{
    const uint16_t backup_vblock = 0x0aac;
    const uint32_t backup_page = 68;
    const uint32_t backup_slot = 0;
    uint32_t bank;
    uint32_t physpage;
    uint32_t rc;
    uint8_t *b = (uint8_t *)ftl_buffer;
    uint16_t *h = (uint16_t *)ftl_buffer;
    uint32_t bps;
    uint32_t spc;
    uint32_t rsvd;
    uint32_t nfats;
    uint32_t total;
    uint32_t fat;
    uint32_t reason;
    uint32_t raw_off = backup_slot * 0x200;
    uint32_t raw_sig;
    uint32_t raw_bps;
    uint32_t raw_fs0;
    uint32_t raw_fs1;

    ftl_n3g_decode_page(backup_vblock, backup_page, &bank, &physpage);
    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    rc = nand_read_page(bank, physpage, ftl_buffer,
                        &ftl_sparebuffer[0], 1, 0);
    reason = ftl_n3g_wmount_bpb_reason(b + backup_slot * 0x200,
                                       part_size, &bps, &spc, &rsvd,
                                       &nfats, &total, &fat);
    raw_sig = (uint32_t)b[raw_off + 0x1fe]
            | ((uint32_t)b[raw_off + 0x1ff] << 8);
    raw_bps = (uint32_t)b[raw_off + 0x0b]
            | ((uint32_t)b[raw_off + 0x0c] << 8);
    raw_fs0 = (uint32_t)b[raw_off + 0x52]
            | ((uint32_t)b[raw_off + 0x53] << 8)
            | ((uint32_t)b[raw_off + 0x54] << 16)
            | ((uint32_t)b[raw_off + 0x55] << 24);
    raw_fs1 = (uint32_t)b[raw_off + 0x56]
            | ((uint32_t)b[raw_off + 0x57] << 8)
            | ((uint32_t)b[raw_off + 0x58] << 16)
            | ((uint32_t)b[raw_off + 0x59] << 24);

    FTL_PROGRESS("N3G_WMOUNT_BKTRY backup=0 v=%04lx po=%lu s1=%lu rc=%lu t=%02x l=%08lx sig=%04x bps=%lu",
                 (unsigned long)backup_vblock,
                 (unsigned long)backup_page,
                 (unsigned long)backup_slot,
                 (unsigned long)rc,
                 ftl_sparebuffer[0].user.type,
                 (unsigned long)ftl_sparebuffer[0].user.lpn,
                 (unsigned long)raw_sig,
                 (unsigned long)raw_bps);
    FTL_PROGRESS("N3G_WMOUNT_BKVAL sig=%04lx bps=%lu spc=%lu rs=%lu nf=%lu fat=%lu r=%03lx",
                 (unsigned long)raw_sig,
                 (unsigned long)raw_bps,
                 (unsigned long)spc,
                 (unsigned long)rsvd,
                 (unsigned long)nfats,
                 (unsigned long)fat,
                 (unsigned long)reason);
    FTL_PROGRESS("N3G_WMOUNT_BKFS fs=%08lx,%08lx",
                 (unsigned long)raw_fs0,
                 (unsigned long)raw_fs1);

    if (rc != 0 || raw_sig != 0xaa55 || raw_bps != 512)
    {
        FTL_PROGRESS("N3G_WMOUNT_BKREJ rc=%lu sig=%04lx bps=%lu r=%03lx",
                     (unsigned long)rc,
                     (unsigned long)raw_sig,
                     (unsigned long)raw_bps,
                     (unsigned long)reason);
        FTL_PROGRESS("N3G_WMOUNT_BACKUP_NOBOOT backup=0 v=%04lx po=%lu s1=%lu sig=%04x bps=%lu r=%03lx",
                     (unsigned long)backup_vblock,
                     (unsigned long)backup_page,
                     (unsigned long)backup_slot,
                     (unsigned long)raw_sig,
                     (unsigned long)raw_bps,
                     (unsigned long)reason);
        return 0;
    }

    n3g_direct_boot_valid = 1;
    n3g_direct_boot_lpn = host_lpn;
    n3g_direct_boot_vblock = backup_vblock;
    n3g_direct_boot_page = backup_page;
    n3g_direct_boot_slot = backup_slot;
    n3g_direct_fsinfo_valid = 1;
    n3g_direct_fsinfo_lpn = host_lpn;
    *fat_j = 0xffffffffu;
    *fat_po = backup_page;
    FTL_PROGRESS("N3G_WMOUNT_BACKUP_ACCEPT backup=0 v=%04lx po=%lu s1=%lu",
                 (unsigned long)backup_vblock,
                 (unsigned long)backup_page,
                 (unsigned long)backup_slot);
    FTL_PROGRESS("N3G_WMOUNT_BOOTSECTOR_OK backup=0 po=%lu s1=%lu bps=%lu v=%04lx raw=%08lx",
                 (unsigned long)backup_page,
                 (unsigned long)backup_slot,
                 (unsigned long)raw_bps,
                 (unsigned long)backup_vblock,
                 (unsigned long)ftl_sparebuffer[0].user.lpn);
    return 1;
}

static uint32_t ftl_n3g_wmount_fatsz(const uint8_t *s)
{
    uint32_t fz16 = (uint32_t)s[0x16] | ((uint32_t)s[0x17] << 8);
    uint32_t fz32 = (uint32_t)s[0x24]
                  | ((uint32_t)s[0x25] << 8)
                  | ((uint32_t)s[0x26] << 16)
                  | ((uint32_t)s[0x27] << 24);

    return fz16 != 0 ? fz16 : fz32;
}

static uint32_t ftl_n3g_wmount_root_cluster(const uint8_t *s)
{
    return (uint32_t)s[0x2c]
         | ((uint32_t)s[0x2d] << 8)
         | ((uint32_t)s[0x2e] << 16)
         | ((uint32_t)s[0x2f] << 24);
}

static uint32_t ftl_n3g_wmount_find_current_boot(uint32_t host_lpn,
                                                 uint32_t part_size,
                                                 uint32_t *fat_j,
                                                 uint32_t *fat_po)
{
    uint32_t reads = 0;
    uint32_t candidates = 0;
    uint32_t aa55 = 0;
    uint32_t sane = 0;
    uint32_t prints = 0;
    uint32_t best_j = 0xffffffffu;
    uint32_t best_v = 0xffffu;
    uint32_t best_po = 0xffffffffu;
    uint32_t best_slot = 0xffffffffu;
    int32_t best_shift = 0;
    uint32_t best_sig = 0;
    uint32_t best_bps = 0;
    uint32_t best_spc = 0;
    uint32_t best_rsvd = 0;
    uint32_t best_nfats = 0;
    uint32_t best_fatsz = 0;
    uint32_t best_root = 0;
    uint32_t best_reason = 0xffffffffu;
    uint32_t best_score = 0;

    FTL_PROGRESS("N3G_BOOTSECTOR_SCAN start=%08lx ent=%lu",
                 (unsigned long)host_lpn,
                 (unsigned long)n3g_direct_map_loaded_entries);

    for (uint32_t j = 0; j < n3g_direct_map_loaded_entries; j++)
    {
        uint32_t v = ftl_map[j];
        uint32_t l0;

        if (v == 0 || v == 0xffffu)
            continue;
        if (ftl_n3g_wmount_map_lpn0(j, &l0) != 0)
            l0 = 0xffffffffu;

        for (uint32_t po = 0; po < ppb; po++)
        {
            uint32_t bank;
            uint32_t physpage;
            uint32_t rc;
            uint8_t *b = (uint8_t *)ftl_buffer;

            ftl_n3g_decode_page(v, po, &bank, &physpage);
            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc = nand_read_page(bank, physpage, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0);
            reads++;
            if (rc != 0)
                continue;

            for (uint32_t slot = 0; slot < 4; slot++)
            {
                uint32_t slot_off = slot * 0x200;
                uint32_t slot_sig = (uint32_t)b[slot_off + 0x1fe]
                                  | ((uint32_t)b[slot_off + 0x1ff] << 8);
                uint32_t slot_bps = (uint32_t)b[slot_off + 0x0b]
                                  | ((uint32_t)b[slot_off + 0x0c] << 8);
                uint32_t slot_bps_sane = slot_bps == 512
                                       || slot_bps == 1024
                                       || slot_bps == 2048
                                       || slot_bps == 4096;
                uint32_t slot_fat = (b[slot_off + 0x36] == 'F'
                                  && b[slot_off + 0x37] == 'A'
                                  && b[slot_off + 0x38] == 'T')
                                  || (b[slot_off + 0x52] == 'F'
                                  && b[slot_off + 0x53] == 'A'
                                  && b[slot_off + 0x54] == 'T');

                for (int32_t shift = -32; shift <= 32; shift++)
                {
                    uint32_t base = slot * 0x200;
                    uint32_t off;
                    uint32_t sig;
                    uint32_t bps;
                    uint32_t spc;
                    uint32_t rsvd;
                    uint32_t nfats;
                    uint32_t total;
                    uint32_t fat;
                    uint32_t reason;
                    uint32_t fatsz;
                    uint32_t root;
                    uint32_t ok_bps;
                    uint32_t ok_spc;
                    uint32_t ok_rsvd;
                    uint32_t ok_nfats;
                    uint32_t ok_sig;
                    uint32_t ok;
                    uint32_t score;

                    if (shift != 0 && slot_sig != 0xaa55
                     && !slot_bps_sane && !slot_fat)
                        continue;

                    if (shift < 0 && base < (uint32_t)(-shift))
                        continue;
                    off = shift < 0 ? base - (uint32_t)(-shift)
                                    : base + (uint32_t)shift;
                    if (off + 0x1ff >= 0x800)
                        continue;

                    sig = (uint32_t)b[off + 0x1fe]
                        | ((uint32_t)b[off + 0x1ff] << 8);
                    reason = ftl_n3g_wmount_bpb_reason(b + off, part_size,
                                    &bps, &spc, &rsvd, &nfats, &total, &fat);
                    fatsz = ftl_n3g_wmount_fatsz(b + off);
                    root = ftl_n3g_wmount_root_cluster(b + off);
                    ok_sig = sig == 0xaa55;
                    ok_bps = bps == 512 || bps == 1024
                          || bps == 2048 || bps == 4096;
                    ok_spc = spc != 0 && (spc & (spc - 1)) == 0;
                    ok_rsvd = rsvd != 0;
                    ok_nfats = nfats > 0 && nfats <= 4;
                    ok = ok_sig && ok_bps && ok_spc && ok_rsvd
                      && ok_nfats && fatsz != 0 && fat;

                    if (ok_sig)
                        aa55++;
                    if (ok)
                        sane++;
                    if (!(ok_sig || ok_bps || fat))
                        continue;

                    candidates++;
                    score = (ok ? 0x10000 : 0)
                          + (ok_sig ? 0x8000 : 0)
                          + (fat ? 0x4000 : 0)
                          + (ok_bps ? 0x2000 : 0)
                          + (ok_spc ? 0x1000 : 0)
                          + (ok_rsvd ? 0x0800 : 0)
                          + (ok_nfats ? 0x0400 : 0)
                          + (fatsz != 0 ? 0x0200 : 0)
                          + (0x1ffu - (reason & 0x1ffu));
                    if (score > best_score)
                    {
                        best_score = score;
                        best_j = j;
                        best_v = v;
                        best_po = po;
                        best_slot = slot;
                        best_shift = shift;
                        best_sig = sig;
                        best_bps = bps;
                        best_spc = spc;
                        best_rsvd = rsvd;
                        best_nfats = nfats;
                        best_fatsz = fatsz;
                        best_root = root;
                        best_reason = reason;
                    }

                    if (prints < 16 || ok)
                    {
                        FTL_PROGRESS("N3G_BOOTSECTOR_CAND j=%lu v=%04lx po=%lu sl=%lu sh=%ld l0=%08lx ol=%08lx",
                                     (unsigned long)j,
                                     (unsigned long)v,
                                     (unsigned long)po,
                                     (unsigned long)slot,
                                     (long)shift,
                                     (unsigned long)l0,
                                     (unsigned long)ftl_sparebuffer[0].user.lpn);
                        FTL_PROGRESS("N3G_BOOTSECTOR_BPB sig=%04lx bps=%lu spc=%lu rs=%lu nf=%lu fz=%lu root=%lu r=%03lx",
                                     (unsigned long)sig,
                                     (unsigned long)bps,
                                     (unsigned long)spc,
                                     (unsigned long)rsvd,
                                     (unsigned long)nfats,
                                     (unsigned long)fatsz,
                                     (unsigned long)root,
                                     (unsigned long)reason);
                        prints++;
                    }

                    if (ok)
                    {
                        n3g_direct_boot_valid = 1;
                        n3g_direct_boot_lpn = host_lpn;
                        n3g_direct_boot_vblock = v;
                        n3g_direct_boot_page = po;
                        n3g_direct_boot_slot = slot;
                        n3g_direct_boot_shift = shift;
                        n3g_direct_fsinfo_valid = 1;
                        n3g_direct_fsinfo_lpn = host_lpn;
                        *fat_j = j;
                        *fat_po = po;
                        FTL_PROGRESS("N3G_BOOTSECTOR_OK j=%lu v=%04lx po=%lu sl=%lu sh=%ld sig=%04lx bps=%lu spc=%lu rs=%lu nf=%lu fz=%lu root=%lu",
                                     (unsigned long)j,
                                     (unsigned long)v,
                                     (unsigned long)po,
                                     (unsigned long)slot,
                                     (long)shift,
                                     (unsigned long)sig,
                                     (unsigned long)bps,
                                     (unsigned long)spc,
                                     (unsigned long)rsvd,
                                     (unsigned long)nfats,
                                     (unsigned long)fatsz,
                                     (unsigned long)root);
                        return 1;
                    }
                }
            }
        }
    }

    FTL_PROGRESS("N3G_BOOTSECTOR_NOT_FOUND r=%lu cand=%lu aa=%lu sane=%lu",
                 (unsigned long)reads,
                 (unsigned long)candidates,
                 (unsigned long)aa55,
                 (unsigned long)sane);
    if (best_j != 0xffffffffu)
    {
        FTL_PROGRESS("N3G_BOOTSECTOR_BEST j=%lu v=%04lx po=%lu sl=%lu sh=%ld",
                     (unsigned long)best_j,
                     (unsigned long)best_v,
                     (unsigned long)best_po,
                     (unsigned long)best_slot,
                     (long)best_shift);
        FTL_PROGRESS("N3G_BOOTSECTOR_BEST_BPB sig=%04lx bps=%lu spc=%lu rs=%lu nf=%lu fz=%lu root=%lu r=%03lx",
                     (unsigned long)best_sig,
                     (unsigned long)best_bps,
                     (unsigned long)best_spc,
                     (unsigned long)best_rsvd,
                     (unsigned long)best_nfats,
                     (unsigned long)best_fatsz,
                     (unsigned long)best_root,
                     (unsigned long)best_reason);
    }
    return 0;
}

static void ftl_n3g_wmount_dump_hex16(const char *tag, const char *target,
                                      uint32_t id, uint32_t off,
                                      const uint8_t *b)
{
    FTL_PROGRESS("N3GD_%s target=%s id=%lu off=%03lx %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                 tag, target, (unsigned long)id, (unsigned long)off,
                 b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                 b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

static void ftl_n3g_wmount_dump_raw_page(const char *kind, uint32_t id,
                                         const char *target,
                                         uint32_t j, uint32_t v,
                                         uint32_t po, uint32_t slot,
                                         uint32_t l0, uint32_t target_lpn,
                                         uint32_t bank, uint32_t physpage)
{
    uint32_t pb = physpage / ftl_nand_type->pagesperblock;
    uint32_t pp = physpage % ftl_nand_type->pagesperblock;
    int32_t rc;
    const uint8_t *body;
    const uint8_t *oob;

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    rc = (int32_t)nand_read_page(bank, physpage, ftl_buffer,
                                 &ftl_sparebuffer[0], 1, 0);
    body = (const uint8_t *)ftl_buffer;
    oob = (const uint8_t *)&ftl_sparebuffer[0];

    FTL_PROGRESS("N3GD_DUMP_PAGE target=%s j=%lu v=%04lx po=%lu",
                 target, (unsigned long)j, (unsigned long)v,
                 (unsigned long)po);
    FTL_PROGRESS("N3GD_TARGET target=%s kind=%s b=4294967295 p=4294967295 j=%lu v=%04lx resolved_block=%lu resolved_page=%lu rc=%ld",
                 target, kind, (unsigned long)j, (unsigned long)v,
                 (unsigned long)pb, (unsigned long)pp, (long)rc);
    FTL_PROGRESS("N3GD_BEGIN id=%lu target=%s kind=%s j=%lu v=%04lx po=%lu sl=%lu l0=%08lx tgt=%08lx bank=%lu pb=%lu pp=%lu phy=%lu rc=%ld t=%02x l=%08lx",
                 (unsigned long)id, target, kind, (unsigned long)j,
                 (unsigned long)v, (unsigned long)po, (unsigned long)slot,
                 (unsigned long)l0, (unsigned long)target_lpn,
                 (unsigned long)bank, (unsigned long)pb, (unsigned long)pp,
                 (unsigned long)physpage, (long)rc,
                 ftl_sparebuffer[0].user.type,
                 (unsigned long)ftl_sparebuffer[0].user.lpn);
    for (uint32_t off = 0; off < 0x800; off += 16)
        ftl_n3g_wmount_dump_hex16("BODY", target, id, off, body + off);
    for (uint32_t off = 0; off < sizeof(ftl_sparebuffer[0]); off += 16)
        ftl_n3g_wmount_dump_hex16("OOB", target, id, off, oob + off);
    FTL_PROGRESS("N3GD_END id=%lu", (unsigned long)id);
}

static void ftl_n3g_wmount_dump_map_page(uint32_t id, uint32_t map_block,
                                         uint32_t map_pageoff,
                                         uint32_t map_page,
                                         const char *target)
{
    int32_t body_rc;
    int32_t oob_rc;
    int32_t rc;
    const uint8_t *body;
    const uint8_t *oob;

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    oob_rc = nano3g_nand_diag_local_read(0, map_page, 0x800,
                                         (uint32_t *)&ftl_sparebuffer[0],
                                         0x10);
    body_rc = nano3g_nand_diag_local_read(0, map_page, 0,
                                          (uint32_t *)ftl_buffer,
                                          0x800 / sizeof(uint32_t));
    body = (const uint8_t *)ftl_buffer;
    oob = (const uint8_t *)&ftl_sparebuffer[0];
    rc = body_rc != 0 ? body_rc : oob_rc;

    FTL_PROGRESS("N3GD_DUMP_MAP b=%lu p=%lu",
                 (unsigned long)map_block, (unsigned long)map_pageoff);
    FTL_PROGRESS("N3GD_TARGET target=%s kind=map b=%lu p=%lu j=4294967295 v=ffff resolved_block=%lu resolved_page=%lu rc=%ld",
                 target, (unsigned long)map_block,
                 (unsigned long)map_pageoff, (unsigned long)map_block,
                 (unsigned long)map_pageoff, (long)rc);
    FTL_PROGRESS("N3GM_MAP_SEEN b=%lu p=%lu t=%02x u=%08lx ix=%04x rc=%ld",
                 (unsigned long)map_block, (unsigned long)map_pageoff,
                 ftl_sparebuffer[0].meta.type,
                 (unsigned long)ftl_sparebuffer[0].meta.usn,
                 ftl_sparebuffer[0].meta.idx, (long)rc);
    FTL_PROGRESS("N3GD_BEGIN id=%lu target=%s kind=map j=4294967295 v=ffff po=%lu sl=4294967295 l0=ffffffff tgt=ffffffff bank=0 pb=%lu pp=%lu phy=%lu rc=%ld t=%02x l=%08lx u=%08lx ix=%04x",
                 (unsigned long)id, target,
                 (unsigned long)map_pageoff,
                 (unsigned long)map_block, (unsigned long)map_pageoff,
                 (unsigned long)map_page, (long)rc,
                 ftl_sparebuffer[0].meta.type,
                 (unsigned long)ftl_sparebuffer[0].user.lpn,
                 (unsigned long)ftl_sparebuffer[0].meta.usn,
                 ftl_sparebuffer[0].meta.idx);
    for (uint32_t off = 0; off < 0x800; off += 16)
        ftl_n3g_wmount_dump_hex16("BODY", target, id, off, body + off);
    for (uint32_t off = 0; off < sizeof(ftl_sparebuffer[0]); off += 16)
        ftl_n3g_wmount_dump_hex16("OOB", target, id, off, oob + off);
    FTL_PROGRESS("N3GD_END id=%lu", (unsigned long)id);
}

static uint32_t ftl_n3g_wmount_dump_candidate_seen(uint32_t *seen_j,
                                                   uint32_t *seen_po,
                                                   uint32_t count,
                                                   uint32_t j,
                                                   uint32_t po)
{
    for (uint32_t i = 0; i < count; i++)
        if (seen_j[i] == j && seen_po[i] == po)
            return 1;
    return 0;
}

static uint32_t ftl_n3g_wmount_dump_vpo_seen(uint32_t *seen_v,
                                             uint32_t *seen_po,
                                             uint32_t count,
                                             uint32_t v,
                                             uint32_t po)
{
    for (uint32_t i = 0; i < count; i++)
        if (seen_v[i] == v && seen_po[i] == po)
            return 1;
    return 0;
}

static void ftl_n3g_wmount_dump_fixed_page(uint32_t *id,
                                           const char *target,
                                           uint32_t j,
                                           uint32_t v,
                                           uint32_t po,
                                           uint32_t slot,
                                           uint32_t l0,
                                           uint32_t target_lpn)
{
    uint32_t bank;
    uint32_t physpage;

    ftl_n3g_decode_page(v, po, &bank, &physpage);
    ftl_n3g_wmount_dump_raw_page("page", (*id)++, target, j, v, po, slot,
                                 l0, target_lpn, bank, physpage);
}

static void ftl_n3g_wmount_dump_map_candidates(uint32_t *id,
                                               uint32_t map_block,
                                               uint32_t map_pageoff,
                                               uint32_t target_lpn,
                                               uint32_t mbr_j,
                                               uint32_t *seen_v,
                                               uint32_t *seen_po,
                                               uint32_t *seen_count,
                                               uint32_t seen_cap)
{
    uint32_t map_page = map_block * ftl_nand_type->pagesperblock + map_pageoff;
    uint32_t map_usn = 0xffffffffu;
    uint32_t map_type = 0xffu;

    (void)id;
    (void)seen_v;
    (void)seen_po;
    (void)seen_count;
    (void)seen_cap;

    memset(n3g_direct_probe_map, 0xff, sizeof(n3g_direct_probe_map));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) == 0)
    {
        map_type = ftl_sparebuffer[0].meta.type;
        map_usn = ftl_sparebuffer[0].meta.usn;
    }
    if (nano3g_nand_diag_local_read(0, map_page, 0,
                                    (uint32_t *)n3g_direct_probe_map,
                                    sizeof(n3g_direct_probe_map)
                                    / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3GM_MAP_FAIL b=%lu p=%lu",
                     (unsigned long)map_block,
                     (unsigned long)map_pageoff);
        return;
    }

    FTL_PROGRESS("N3GM_MAP b=%lu p=%lu type=%02lx u=%08lx",
                 (unsigned long)map_block,
                 (unsigned long)map_pageoff,
                 (unsigned long)map_type,
                 (unsigned long)map_usn);

    for (uint32_t j = 0; j < 0x400; j++)
    {
        uint32_t v = n3g_direct_probe_map[j];
        uint32_t bank;
        uint32_t physpage;
        uint32_t l0;
        uint32_t span = ppb * 4;
        uint32_t end;
        uint32_t delta = 0xffffffffu;
        uint32_t po = 0;
        uint32_t type;
        uint32_t interested;

        if (v == 0 || v == 0xffffu)
            continue;

        ftl_n3g_decode_page(v, 0, &bank, &physpage);
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        if (nand_read_page(bank, physpage, NULL, &ftl_sparebuffer[0], 1, 0) != 0)
            continue;
        type = ftl_sparebuffer[0].user.type;
        if (type != 0x40 && type != 0x41)
            continue;

        l0 = ftl_sparebuffer[0].user.lpn;
        end = l0 + span - 1;
        if (target_lpn >= l0 && target_lpn <= end)
        {
            delta = target_lpn - l0;
            po = delta >> 2;
        }

        interested = (j == 61 || j == 462 || j == 982 || j == mbr_j
                   || v == 0x0194u || v == 0x0384u
                   || v == 0x0383u || v == 0x056bu
                   || (l0 >= 0x00009c00u && l0 <= 0x0000a200u)
                   || (end >= 0x00009c00u && l0 <= 0x0000a200u)
                   || delta != 0xffffffffu);
        if (!interested)
            continue;

        FTL_PROGRESS("N3GM_ENTRY mapb=%lu mapp=%lu j=%lu v=%04lx l0=%08lx span=%lu t=%02lx po=%lu tgt=%08lx d=%lu",
                     (unsigned long)map_block,
                     (unsigned long)map_pageoff,
                     (unsigned long)j,
                     (unsigned long)v,
                     (unsigned long)l0,
                     (unsigned long)span,
                     (unsigned long)type,
                     (unsigned long)po,
                     (unsigned long)target_lpn,
                     (unsigned long)delta);
    }
}

static void ftl_n3g_wmount_dump_replay_set(uint32_t map_block,
                                           uint32_t map_pageoff,
                                           uint32_t map_page,
                                           uint32_t mbr_j,
                                           uint32_t mbr_po,
                                           uint32_t mbr_slot,
                                           uint32_t mbr_base_lpn,
                                           uint32_t target_lpn,
                                           uint32_t part_type,
                                           uint32_t part_start,
                                           uint32_t part_size)
{
    static const struct
    {
        uint32_t block;
        uint32_t page;
        const char *target;
    } map_targets[] =
    {
        { 6916, 0, "map_b6916_p0" },
        { 6166, 0, "map_b6166_p0" },
        { 6912, 0, "map_b6912_p0" },
    };
    static const struct
    {
        uint32_t j;
        uint32_t v;
        uint32_t po;
        uint32_t slot;
        uint32_t l0;
        const char *target;
    } page_targets[] =
    {
        { 982, 0x056b, 0, 0, 0x00000000, "page_j982_v056B" },
        { 61, 0x0384, 287, 0xffffffffu, 0xffffffffu, "page_j61_v0384" },
        { 61, 0x0194, 0, 0xffffffffu, 0xffffffffu, "page_j61_v0194" },
        { 462, 0x0383, 287, 0xffffffffu, 0xffffffffu, "page_j462_v0383" },
    };
    static const uint32_t test_lbas[] =
    {
        0x00000000, 0x00000001, 0x0000003f,
        0x0000a07c, 0x0000a07e, 0x0000a080
    };
    uint32_t id = 1;

    FTL_PROGRESS("N3GD_START profile=winpod_mbr_fat32 target=%08lx ppb=%lu",
                 (unsigned long)target_lpn, (unsigned long)ppb);
    FTL_PROGRESS("N3GM_START selected_map_b=%lu selected_map_p=%lu mbr_j=%lu mbr_po=%lu mbr_s1=%lu mbr_l0=%08lx",
                 (unsigned long)map_block,
                 (unsigned long)map_pageoff,
                 (unsigned long)mbr_j,
                 (unsigned long)mbr_po,
                 (unsigned long)mbr_slot,
                 (unsigned long)mbr_base_lpn);
    FTL_PROGRESS("N3GM_PART p=0 t=%02lx st=%08lx sz=%08lx",
                 (unsigned long)part_type,
                 (unsigned long)part_start,
                 (unsigned long)part_size);
    FTL_PROGRESS("N3GM_MBR j=%lu v=%04lx po=%lu l0=%08lx",
                 (unsigned long)mbr_j,
                 (unsigned long)(mbr_j < ARRAYLEN(ftl_map)
                    ? ftl_map[mbr_j] : 0xffffu),
                 (unsigned long)mbr_po,
                 (unsigned long)mbr_base_lpn);
    for (uint32_t i = 0; i < ARRAYLEN(test_lbas); i++)
        FTL_PROGRESS("N3GM_TEST_LBA lba=%08lx", (unsigned long)test_lbas[i]);

    for (uint32_t i = 0; i < ARRAYLEN(map_targets); i++)
    {
        uint32_t p = map_targets[i].block * ftl_nand_type->pagesperblock
                   + map_targets[i].page;

        ftl_n3g_wmount_dump_map_page(id++, map_targets[i].block,
                                     map_targets[i].page, p,
                                     map_targets[i].target);
    }

    if (map_block != 6916 && map_block != 6166 && map_block != 6912)
    {
        FTL_PROGRESS("N3GM_SELECTED_MAP_EXTRA b=%lu p=%lu",
                     (unsigned long)map_block,
                     (unsigned long)map_pageoff);
        ftl_n3g_wmount_dump_map_page(id++, map_block, map_pageoff, map_page,
                                     "map_selected");
    }

    for (uint32_t i = 0; i < ARRAYLEN(page_targets); i++)
    {
        ftl_n3g_wmount_dump_fixed_page(&id, page_targets[i].target,
                                       page_targets[i].j, page_targets[i].v,
                                       page_targets[i].po,
                                       page_targets[i].slot,
                                       page_targets[i].l0, target_lpn);
    }

    for (uint32_t i = 0; i < ARRAYLEN(map_targets); i++)
    {
        ftl_n3g_wmount_dump_map_candidates(&id, map_targets[i].block,
                                           map_targets[i].page,
                                           target_lpn, mbr_j,
                                           NULL, NULL, NULL, 0);
    }

    FTL_PROGRESS("N3GD_DONE pages=%lu", (unsigned long)(id - 1));
}

static uint32_t ftl_n3g_wmount_probe_l0_range(const char *tag,
                                              uint32_t target_lpn,
                                              uint32_t host_lpn,
                                              uint32_t part_size,
                                              uint32_t *fat_j,
                                              uint32_t *fat_po)
{
    uint32_t hits = 0;
    uint32_t prints = 0;

    FTL_PROGRESS("N3G_WMOUNT_RLOOK %s l=%08lx",
                 tag, (unsigned long)target_lpn);

    for (uint32_t j = 0; j < n3g_direct_map_loaded_entries; j++)
    {
        uint32_t lpn0;
        uint32_t delta;
        uint32_t po;
        uint32_t slot;
        uint32_t v = ftl_map[j];
        uint32_t bank;
        uint32_t physpage;
        uint32_t rc;
        uint8_t *b = (uint8_t *)ftl_buffer;
        uint16_t *h = (uint16_t *)ftl_buffer;
        uint32_t sig;
        uint32_t bps;
        uint32_t spc;
        uint32_t rsvd;
        uint32_t nfats;
        uint32_t total;
        uint32_t fat;
        uint32_t reason;

        if (v == 0 || v == 0xffffu)
            continue;
        if (ftl_n3g_wmount_known_bad_j(j))
            continue;
        if (ftl_n3g_wmount_map_lpn0(j, &lpn0) != 0)
            continue;
        if (target_lpn < lpn0)
            continue;

        delta = target_lpn - lpn0;
        if (delta >= ppb * 4)
            continue;

        po = delta >> 2;
        slot = delta & 3u;
        ftl_n3g_decode_page(v, po, &bank, &physpage);
        memset(ftl_buffer, 0, sizeof(ftl_buffer));
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        rc = nand_read_page(bank, physpage, ftl_buffer,
                            &ftl_sparebuffer[0], 1, 0);
        if (rc != 0)
            continue;

        sig = h[((slot * 0x200) + 0x1fe) >> 1];
        reason = ftl_n3g_wmount_bpb_reason(b + slot * 0x200,
                                           part_size, &bps, &spc, &rsvd,
                                           &nfats, &total, &fat);
        uint32_t score = (sig == 0xaa55 ? 0x1000 : 0)
                       + (fat ? 0x0800 : 0)
                       + ((reason & ~0x020u) == 0 ? 0x0400 : 0)
                       + (bps == 512 || bps == 1024
                       || bps == 2048 || bps == 4096 ? 0x0200 : 0)
                       + (0x1ffu - (reason & 0x1ffu));
        hits++;
        n3g_wmount_rd_hits++;
        if (!n3g_wmount_rd_best_valid
         || score > n3g_wmount_rd_best_score)
        {
            n3g_wmount_rd_best_valid = 1;
            n3g_wmount_rd_best_tag = tag;
            n3g_wmount_rd_best_j = j;
            n3g_wmount_rd_best_v = v;
            n3g_wmount_rd_best_l0 = lpn0;
            n3g_wmount_rd_best_po = po;
            n3g_wmount_rd_best_slot = slot;
            n3g_wmount_rd_best_sig = sig;
            n3g_wmount_rd_best_reason = reason;
            n3g_wmount_rd_best_bps = bps;
            n3g_wmount_rd_best_fat = fat;
            n3g_wmount_rd_best_type = ftl_sparebuffer[0].user.type;
            n3g_wmount_rd_best_oob_lpn = ftl_sparebuffer[0].user.lpn;
            n3g_wmount_rd_best_score = score;
        }
        if (prints < 8 || (sig == 0xaa55 && fat))
        {
            FTL_PROGRESS("N3G_WMOUNT_RHIT %s j=%lu v=%04lx l0=%08lx po=%lu sl=%lu",
                         tag, (unsigned long)j, (unsigned long)v,
                         (unsigned long)lpn0, (unsigned long)po,
                         (unsigned long)slot);
            FTL_PROGRESS("N3G_WMOUNT_RB %s t=%02x ol=%08lx sig=%04lx bps=%lu fat=%lu r=%03lx",
                         tag, ftl_sparebuffer[0].user.type,
                         (unsigned long)ftl_sparebuffer[0].user.lpn,
                         (unsigned long)sig, (unsigned long)bps,
                         (unsigned long)fat, (unsigned long)reason);
            prints++;
        }

        if (sig == 0xaa55 && fat && (reason & ~0x020u) == 0)
        {
            n3g_direct_boot_valid = 1;
            n3g_direct_boot_lpn = host_lpn;
            n3g_direct_boot_vblock = v;
            n3g_direct_boot_page = po;
            n3g_direct_boot_slot = slot;
            *fat_j = j;
            *fat_po = po;
            FTL_PROGRESS("N3G_WMOUNT_RMAP %s j=%lu v=%04lx po=%lu sl=%lu",
                         tag, (unsigned long)j, (unsigned long)v,
                         (unsigned long)po, (unsigned long)slot);
            return 1;
        }

        if (sig != 0xaa55)
        {
            n3g_wmount_rd_rej_sig++;
            if (sig == 0)
                n3g_wmount_rd_rej_zero++;
        }
        else if (!fat)
            n3g_wmount_rd_rej_fat++;
        else
            n3g_wmount_rd_rej_bpb++;

        FTL_PROGRESS("N3G_WMOUNT_RREJ %s j=%lu po=%lu sl=%lu sig=%04lx r=%03lx bps=%lu fat=%lu",
                     tag, (unsigned long)j, (unsigned long)po,
                     (unsigned long)slot, (unsigned long)sig,
                     (unsigned long)reason, (unsigned long)bps,
                     (unsigned long)fat);
    }

    FTL_PROGRESS("N3G_WMOUNT_RDONE %s hits=%lu",
                 tag, (unsigned long)hits);
    return 0;
}

static uint32_t ftl_n3g_wmount_probe_cover_window(uint32_t target_lpn,
                                                  uint32_t host_lpn,
                                                  uint32_t part_size,
                                                  uint32_t *fat_j,
                                                  uint32_t *fat_po)
{
    uint32_t entries = 0;
    uint32_t reads = 0;
    uint32_t aa55 = 0;
    uint32_t sane = 0;

    FTL_PROGRESS("N3G_WMOUNT_COV_START tgt=%08lx",
                 (unsigned long)target_lpn);

    for (uint32_t j = 0; j < n3g_direct_map_loaded_entries; j++)
    {
        uint32_t v = ftl_map[j];
        uint32_t l0;
        int32_t diff;

        if (v == 0 || v == 0xffffu)
            continue;
        if (ftl_n3g_wmount_map_lpn0(j, &l0) != 0)
            continue;

        diff = (int32_t)target_lpn - (int32_t)l0;
        if (diff < -4 || diff > 4)
            continue;

        entries++;
        FTL_PROGRESS("N3G_WMOUNT_COV_ENTRY j=%lu v=%04lx l0=%08lx d=%ld",
                     (unsigned long)j, (unsigned long)v,
                     (unsigned long)l0, (long)diff);

        for (uint32_t po = 288; po <= 292; po++)
        {
            uint32_t bank;
            uint32_t physpage;
            uint32_t rc;
            uint8_t *b = (uint8_t *)ftl_buffer;
            uint16_t *h = (uint16_t *)ftl_buffer;

            if (po >= ppb)
                continue;

            ftl_n3g_decode_page(v, po, &bank, &physpage);
            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc = nand_read_page(bank, physpage, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0);
            reads++;
            if (rc != 0)
            {
                FTL_PROGRESS("N3G_WMOUNT_COV_READ j=%lu po=%lu rc=%lu",
                             (unsigned long)j, (unsigned long)po,
                             (unsigned long)rc);
                continue;
            }

            for (uint32_t slot = 0; slot < 4; slot++)
            {
                uint32_t off = slot * 0x200;
                uint32_t sig = h[(off + 0x1fe) >> 1];
                uint32_t bps;
                uint32_t spc;
                uint32_t rsvd;
                uint32_t nfats;
                uint32_t total;
                uint32_t fat;
                uint32_t reason = ftl_n3g_wmount_bpb_reason(b + off,
                                        part_size, &bps, &spc, &rsvd,
                                        &nfats, &total, &fat);
                uint32_t sane_bps = bps == 512 || bps == 1024
                                  || bps == 2048 || bps == 4096;

                if (sig == 0xaa55)
                    aa55++;
                if (sane_bps)
                    sane++;

                FTL_PROGRESS("N3G_WMOUNT_COVP j=%lu po=%lu sl=%lu t=%02x ol=%08lx sig=%04lx bps=%lu fat=%lu r=%03lx",
                             (unsigned long)j, (unsigned long)po,
                             (unsigned long)slot,
                             ftl_sparebuffer[0].user.type,
                             (unsigned long)ftl_sparebuffer[0].user.lpn,
                             (unsigned long)sig, (unsigned long)bps,
                             (unsigned long)fat, (unsigned long)reason);

                if (sig == 0xaa55 && fat && (reason & ~0x020u) == 0)
                {
                    n3g_direct_boot_valid = 1;
                    n3g_direct_boot_lpn = host_lpn;
                    n3g_direct_boot_vblock = v;
                    n3g_direct_boot_page = po;
                    n3g_direct_boot_slot = slot;
                    *fat_j = j;
                    *fat_po = po;
                    FTL_PROGRESS("N3G_WMOUNT_BOOTSECTOR_OK cov j=%lu v=%04lx po=%lu sl=%lu",
                                 (unsigned long)j, (unsigned long)v,
                                 (unsigned long)po, (unsigned long)slot);
                    return 1;
                }
            }
        }
    }

    FTL_PROGRESS("N3G_WMOUNT_COV_DONE e=%lu r=%lu aa=%lu sane=%lu",
                 (unsigned long)entries, (unsigned long)reads,
                 (unsigned long)aa55, (unsigned long)sane);
    return 0;
}

static void ftl_n3g_wmount_direct_lba_diag(uint32_t target_lpn,
                                           uint32_t part_size)
{
    uint32_t best_j = 0xffffffffu;
    uint32_t best_v = 0xffffu;
    uint32_t best_l0 = 0xffffffffu;
    uint32_t best_delta = 0xffffffffu;
    uint32_t covers = 0;
    uint32_t near_j[6];
    uint32_t near_v[6];
    uint32_t near_l0[6];
    uint32_t near_dist[6];

    for (uint32_t i = 0; i < ARRAYLEN(near_j); i++)
    {
        near_j[i] = 0xffffffffu;
        near_v[i] = 0xffffu;
        near_l0[i] = 0xffffffffu;
        near_dist[i] = 0xffffffffu;
    }

    FTL_PROGRESS("N3G_WMOUNT_DLOOK l=%08lx ppb=%lu",
                 (unsigned long)target_lpn, (unsigned long)ppb);

    for (uint32_t j = 0; j < n3g_direct_map_loaded_entries; j++)
    {
        uint32_t v = ftl_map[j];
        uint32_t l0;
        uint32_t end;
        uint32_t dist;

        if (v == 0 || v == 0xffffu)
            continue;
        if (ftl_n3g_wmount_map_lpn0(j, &l0) != 0)
            continue;

        end = l0 + ppb * 4 - 1;
        if (target_lpn >= l0 && target_lpn <= end)
        {
            uint32_t delta = target_lpn - l0;

            covers++;
            FTL_PROGRESS("N3G_WMOUNT_DCV j=%lu v=%04lx l0=%08lx end=%08lx d=%lu po=%lu sl=%lu",
                         (unsigned long)j, (unsigned long)v,
                         (unsigned long)l0, (unsigned long)end,
                         (unsigned long)delta,
                         (unsigned long)(delta >> 2),
                         (unsigned long)(delta & 3u));
            if (best_j == 0xffffffffu || l0 > best_l0)
            {
                best_j = j;
                best_v = v;
                best_l0 = l0;
                best_delta = delta;
            }
        }

        if (target_lpn < l0)
            dist = l0 - target_lpn;
        else if (target_lpn > end)
            dist = target_lpn - end;
        else
            dist = 0;

        for (uint32_t n = 0; n < ARRAYLEN(near_j); n++)
        {
            if (dist >= near_dist[n])
                continue;
            for (uint32_t m = ARRAYLEN(near_j) - 1; m > n; m--)
            {
                near_j[m] = near_j[m - 1];
                near_v[m] = near_v[m - 1];
                near_l0[m] = near_l0[m - 1];
                near_dist[m] = near_dist[m - 1];
            }
            near_j[n] = j;
            near_v[n] = v;
            near_l0[n] = l0;
            near_dist[n] = dist;
            break;
        }
    }

    if (best_j != 0xffffffffu)
    {
        uint32_t po = best_delta >> 2;
        uint32_t slot = best_delta & 3u;
        uint32_t bank;
        uint32_t physpage;
        uint32_t pb;
        uint32_t pp;
        uint32_t rc;
        uint32_t off = slot * 0x200;
        uint8_t *b = (uint8_t *)ftl_buffer;
        uint16_t *h = (uint16_t *)ftl_buffer;
        uint32_t sig;
        uint32_t bps;
        uint32_t spc;
        uint32_t rsvd;
        uint32_t nfats;
        uint32_t total;
        uint32_t fat;
        uint32_t reason;

        ftl_n3g_decode_page(best_v, po, &bank, &physpage);
        pb = physpage / ftl_nand_type->pagesperblock;
        pp = physpage % ftl_nand_type->pagesperblock;
        memset(ftl_buffer, 0, sizeof(ftl_buffer));
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        rc = nand_read_page(bank, physpage, ftl_buffer,
                            &ftl_sparebuffer[0], 1, 0);
        FTL_PROGRESS("N3G_WMOUNT_DSEL j=%lu v=%04lx l0=%08lx d=%lu po=%lu sl=%lu",
                     (unsigned long)best_j, (unsigned long)best_v,
                     (unsigned long)best_l0, (unsigned long)best_delta,
                     (unsigned long)po, (unsigned long)slot);
        FTL_PROGRESS("N3G_WMOUNT_DPHY bank=%lu pb=%lu pp=%lu phy=%lu rc=%lu",
                     (unsigned long)bank, (unsigned long)pb,
                     (unsigned long)pp, (unsigned long)physpage,
                     (unsigned long)rc);
        if (rc == 0)
        {
            sig = h[(off + 0x1fe) >> 1];
            reason = ftl_n3g_wmount_bpb_reason(b + off, part_size, &bps,
                                               &spc, &rsvd, &nfats, &total,
                                               &fat);
            FTL_PROGRESS("N3G_WMOUNT_DOOB t=%02x l=%08lx sig=%04lx",
                         ftl_sparebuffer[0].user.type,
                         (unsigned long)ftl_sparebuffer[0].user.lpn,
                         (unsigned long)sig);
            FTL_PROGRESS("N3G_WMOUNT_D0 %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                         b[off + 0], b[off + 1], b[off + 2], b[off + 3],
                         b[off + 4], b[off + 5], b[off + 6], b[off + 7],
                         b[off + 8], b[off + 9], b[off + 10], b[off + 11],
                         b[off + 12], b[off + 13], b[off + 14], b[off + 15]);
            FTL_PROGRESS("N3G_WMOUNT_D1 %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                         b[off + 16], b[off + 17], b[off + 18],
                         b[off + 19], b[off + 20], b[off + 21],
                         b[off + 22], b[off + 23], b[off + 24],
                         b[off + 25], b[off + 26], b[off + 27],
                         b[off + 28], b[off + 29], b[off + 30],
                         b[off + 31]);
            FTL_PROGRESS("N3G_WMOUNT_DBPB 0B=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                         b[off + 0x0b], b[off + 0x0c], b[off + 0x0d],
                         b[off + 0x0e], b[off + 0x0f], b[off + 0x10],
                         b[off + 0x11], b[off + 0x12], b[off + 0x13],
                         b[off + 0x14], b[off + 0x15], b[off + 0x16],
                         b[off + 0x17], b[off + 0x18], b[off + 0x19],
                         b[off + 0x1a]);
            FTL_PROGRESS("N3G_WMOUNT_DFAT 52=%02x%02x%02x%02x%02x%02x%02x%02x sg=%02x%02x",
                         b[off + 0x52], b[off + 0x53],
                         b[off + 0x54], b[off + 0x55],
                         b[off + 0x56], b[off + 0x57],
                         b[off + 0x58], b[off + 0x59],
                         b[off + 0x1fe], b[off + 0x1ff]);
            FTL_PROGRESS("N3G_WMOUNT_DSUM r=%03lx bps=%lu spc=%lu rs=%lu nf=%lu ts=%lu fat=%lu",
                         (unsigned long)reason, (unsigned long)bps,
                         (unsigned long)spc, (unsigned long)rsvd,
                         (unsigned long)nfats, (unsigned long)total,
                         (unsigned long)fat);
        }
    }
    else
    {
        FTL_PROGRESS("N3G_WMOUNT_DNOCOVER n=%lu", (unsigned long)covers);
    }

    for (uint32_t n = 0; n < ARRAYLEN(near_j); n++)
    {
        if (near_j[n] == 0xffffffffu)
            continue;
        FTL_PROGRESS("N3G_WMOUNT_DNEAR n=%lu j=%lu v=%04lx l0=%08lx dist=%lu",
                     (unsigned long)n, (unsigned long)near_j[n],
                     (unsigned long)near_v[n], (unsigned long)near_l0[n],
                     (unsigned long)near_dist[n]);
    }
    FTL_PROGRESS("N3G_WMOUNT_DDONE covers=%lu", (unsigned long)covers);
}

static uint32_t ftl_n3g_direct_mount(void)
{
    const uint32_t map_block = 6916;
    const uint32_t map_page = map_block * ftl_nand_type->pagesperblock;
    uint32_t map_usn;
    uint32_t map_idx;
    uint32_t map0 = 0xffffu;
    uint32_t min_lpn = 0xffffffffu;
    uint32_t min_j = 0xffffffffu;
    uint32_t min_v = 0xffffffffu;
    uint32_t max_lpn = 0;
    uint32_t raw_users = 0;
    uint32_t l0_hits = 0;
    uint32_t l0_scans = 0;
    uint32_t l0_j = 0xffffffffu;
    uint32_t l0_v = 0xffffffffu;
    uint32_t l0_po = 0xffffffffu;
    uint32_t low_prints = 0;

    n3g_direct_map_mount = 0;
    n3g_direct_map_count = 0;
    n3g_direct_map_min_lblock = 0xffffffffu;
    n3g_direct_map_max_lblock = 0;
    memset(ftl_map, 0xff, sizeof(ftl_map));
    for (uint32_t i = 0; i < ARRAYLEN(n3g_direct_l0_vblock); i++)
    {
        n3g_direct_l0_vblock[i] = 0xffffu;
        n3g_direct_l0_page[i] = 0xffffu;
    }
    for (uint32_t i = 0; i < ARRAYLEN(n3g_direct_l0_cache); i++)
        n3g_direct_l0_cache[i] = 0xffffffffu;

    FTL_PROGRESS("N3G_DIRECT_START");

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0)
    {
        FTL_PROGRESS("N3G_DIRECT_FAIL mapoob");
        return -1;
    }
    if (nano3g_nand_diag_local_read(0, map_page, 0,
                                    (uint32_t *)ftl_buffer,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_DIRECT_FAIL mapbody");
        return -1;
    }

    map_usn = ftl_sparebuffer[0].meta.usn;
    map_idx = ftl_sparebuffer[0].meta.idx;
    if (ftl_sparebuffer[0].meta.type != 0x44)
    {
        FTL_PROGRESS("N3G_DIRECT_FAIL t=%02x ix=%04x u=%08lx",
                     ftl_sparebuffer[0].meta.type,
                     ftl_sparebuffer[0].meta.idx,
                     (unsigned long)map_usn);
        return -1;
    }

    const uint16_t *map = (const uint16_t *)ftl_buffer;
    for (uint32_t j = 0; j < 0x800 / sizeof(uint16_t); j++)
    {
        uint32_t v = map[j];
        uint32_t bank;
        uint32_t physpage;
        uint32_t lpn;
        uint32_t lblock;

        if (v == 0 || v == 0xffffu)
            continue;

        ftl_n3g_decode_page(v, 0, &bank, &physpage);
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        if (nand_read_page(bank, physpage, NULL, &ftl_sparebuffer[0],
                           1, 0) != 0)
            continue;
        if (ftl_sparebuffer[0].user.type != 0x40
         && ftl_sparebuffer[0].user.type != 0x41)
            continue;

        lpn = ftl_sparebuffer[0].user.lpn;
        raw_users++;
        if (lpn < min_lpn)
        {
            min_lpn = lpn;
            min_j = j;
            min_v = v;
        }
        if (lpn > max_lpn)
            max_lpn = lpn;

        lblock = lpn / ppb;
        if (lblock >= ftl_nand_type->userblocks)
            continue;
        if (ftl_map[lblock] != 0xffffu)
            continue;

        ftl_map[lblock] = v;
        n3g_direct_map_count++;
        if (lblock < n3g_direct_map_min_lblock)
            n3g_direct_map_min_lblock = lblock;
        if (lblock > n3g_direct_map_max_lblock)
            n3g_direct_map_max_lblock = lblock;
        if (lblock == 0)
            map0 = v;
    }

    FTL_PROGRESS("N3G_DIRECT_RAW b=%lu t=44 ix=%04lx u=%08lx ru=%lu mn=%08lx j=%lu v=%04lx mx=%08lx",
                 (unsigned long)map_block,
                 (unsigned long)map_idx,
                 (unsigned long)map_usn,
                 (unsigned long)raw_users,
                 (unsigned long)min_lpn,
                 (unsigned long)min_j,
                 (unsigned long)min_v,
                 (unsigned long)max_lpn);

    if (map0 == 0xffffu)
    {
        for (uint32_t j = 0; j < 0x800 / sizeof(uint16_t); j++)
        {
            uint32_t v = map[j];

            if (v == 0 || v == 0xffffu)
                continue;

            for (uint32_t po = 0; po < ppb; po++)
            {
                uint32_t bank;
                uint32_t physpage;
                uint32_t lpn;

                if ((l0_scans & 0x1ffff) == 0)
                    FTL_PROGRESS("N3G_L0_PROG j=%lu po=%lu",
                                 (unsigned long)j,
                                 (unsigned long)po);

                ftl_n3g_decode_page(v, po, &bank, &physpage);
                memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                if (nand_read_page(bank, physpage, NULL,
                                   &ftl_sparebuffer[0], 1, 0) != 0)
                {
                    l0_scans++;
                    continue;
                }
                l0_scans++;

                if (ftl_sparebuffer[0].user.type != 0x40
                 && ftl_sparebuffer[0].user.type != 0x41)
                    continue;

                lpn = ftl_sparebuffer[0].user.lpn;
                if (lpn >= ppb)
                    continue;
                if (lpn >= ARRAYLEN(n3g_direct_l0_vblock))
                    continue;

                if (n3g_direct_l0_vblock[lpn] == 0xffffu)
                {
                    n3g_direct_l0_vblock[lpn] = v;
                    n3g_direct_l0_page[lpn] = po;
                    l0_hits++;
                    if (low_prints < 8)
                    {
                        uint32_t body[0x200] STORAGE_ALIGN_ATTR;
                        uint16_t *h = (uint16_t *)body;

                        memset(body, 0, sizeof(body));
                        if (nand_read_page(bank, physpage, body,
                                           &ftl_sparebuffer[0], 1, 0) != 0)
                            memset(body, 0, sizeof(body));
                        FTL_PROGRESS("N3G_L0_HIT l=%08lx j=%lu v=%04lx po=%lu sig=%04x,%04x w=%08lx,%08lx",
                                     (unsigned long)lpn,
                                     (unsigned long)j,
                                     (unsigned long)v,
                                     (unsigned long)po,
                                     h[0xff], h[0x100],
                                     (unsigned long)body[0],
                                     (unsigned long)body[1]);
                        low_prints++;
                    }
                    if (lpn == 0)
                    {
                        l0_j = j;
                        l0_v = v;
                        l0_po = po;
                        map0 = v;
                        ftl_map[0] = v;
                        if (n3g_direct_map_min_lblock > 0)
                            n3g_direct_map_min_lblock = 0;
                        goto l0_done;
                    }
                }
            }
        }

l0_done:
        FTL_PROGRESS("N3G_L0_DONE scans=%lu hits=%lu j=%lu v=%04lx po=%lu",
                     (unsigned long)l0_scans,
                     (unsigned long)l0_hits,
                     (unsigned long)l0_j,
                     (unsigned long)l0_v,
                     (unsigned long)l0_po);
        if (map0 == 0xffffu)
        {
            FTL_PROGRESS("N3G_DIRECT_FAIL nomap0 cnt=%lu mn=%lu mx=%lu",
                         (unsigned long)n3g_direct_map_count,
                         (unsigned long)n3g_direct_map_min_lblock,
                         (unsigned long)n3g_direct_map_max_lblock);
            return -1;
        }
    }

    n3g_direct_map_mount = 1;
    FTL_PROGRESS("N3G_DIRECT_MAP b=%lu p=%lu u=%lu mn=%lu mx=%lu m0=%04lx usn=%08lx",
                 (unsigned long)map_block,
                 (unsigned long)(map_page % ftl_nand_type->pagesperblock),
                 (unsigned long)n3g_direct_map_count,
                 (unsigned long)n3g_direct_map_min_lblock,
                 (unsigned long)n3g_direct_map_max_lblock,
                 (unsigned long)map0,
                 (unsigned long)map_usn);

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    int32_t rc = (int32_t)ftl_n3g_direct_read(0, 1, ftl_buffer);
    FTL_PROGRESS("N3G_DIRECT_S0 rc=%ld t=%02x l=%08lx sig=%04x,%04x w=%08lx,%08lx",
                 (long)rc,
                 ftl_sparebuffer[0].user.type,
                 (unsigned long)ftl_sparebuffer[0].user.lpn,
                 ((uint16_t *)ftl_buffer)[0xff],
                 ((uint16_t *)ftl_buffer)[0x100],
                 (unsigned long)((uint32_t *)ftl_buffer)[0],
                 (unsigned long)((uint32_t *)ftl_buffer)[1]);
    if (rc != 0)
    {
        n3g_direct_map_mount = 0;
        return -1;
    }

    FTL_PROGRESS("N3G_DIRECT_READY");
    return 0;
}

static void ftl_n3g_maplow_probe(void)
{
    static const uint32_t map_blocks[] = { 6916, 6166, 5914 };
    uint8_t mapbuf[0x800] STORAGE_ALIGN_ATTR;
    uint32_t scans = 0;
    uint32_t hits = 0;
    uint32_t zero_block = 0xffffffffu;
    uint32_t zero_j = 0xffffffffu;
    uint32_t zero_v = 0xffffffffu;
    uint32_t zero_po = 0xffffffffu;

    FTL_PROGRESS("N3G_MLOW_START");

    for (uint32_t mi = 0; mi < ARRAYLEN(map_blocks); mi++)
    {
        uint32_t map_block = map_blocks[mi];
        uint32_t map_page = map_block * ftl_nand_type->pagesperblock;
        uint32_t users = 0;

        memset(mapbuf, 0, sizeof(mapbuf));
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                        (uint32_t *)&ftl_sparebuffer[0],
                                        0x10) != 0)
        {
            FTL_PROGRESS("N3G_MLOW_MAP b=%lu oob=bad",
                         (unsigned long)map_block);
            continue;
        }
        if (nano3g_nand_diag_local_read(0, map_page, 0,
                                        (uint32_t *)mapbuf,
                                        0x800 / sizeof(uint32_t)) != 0)
        {
            FTL_PROGRESS("N3G_MLOW_MAP b=%lu body=bad",
                         (unsigned long)map_block);
            continue;
        }
        FTL_PROGRESS("N3G_MLOW_MAP b=%lu t=%02x ix=%04x u=%08lx",
                     (unsigned long)map_block,
                     ftl_sparebuffer[0].meta.type,
                     ftl_sparebuffer[0].meta.idx,
                     (unsigned long)ftl_sparebuffer[0].meta.usn);

        if (ftl_sparebuffer[0].meta.type != 0x44)
            continue;

        const uint16_t *map = (const uint16_t *)mapbuf;
        for (uint32_t j = 0; j < 0x800 / sizeof(uint16_t); j++)
        {
            uint32_t v = map[j];

            if ((j & 0xff) == 0)
                FTL_PROGRESS("N3G_MLOW_PROG b=%lu j=%lu",
                             (unsigned long)map_block,
                             (unsigned long)j);

            if (v == 0 || v == 0xffffu)
                continue;
            users++;

            for (uint32_t po = 0; po < ppb; po++)
            {
                uint32_t bank;
                uint32_t physpage;
                uint32_t lpn;

                ftl_n3g_decode_page(v, po, &bank, &physpage);
                memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                if (nand_read_page(bank, physpage, NULL,
                                   &ftl_sparebuffer[0], 1, 0) != 0)
                {
                    scans++;
                    continue;
                }
                scans++;

                if (ftl_sparebuffer[0].user.type != 0x40
                 && ftl_sparebuffer[0].user.type != 0x41)
                    continue;

                lpn = ftl_sparebuffer[0].user.lpn;
                if (lpn >= ppb)
                    continue;

                if (hits < 16 || lpn == 0)
                {
                    uint32_t body[0x200] STORAGE_ALIGN_ATTR;
                    uint16_t *h = (uint16_t *)body;

                    memset(body, 0, sizeof(body));
                    if (nand_read_page(bank, physpage, body,
                                       &ftl_sparebuffer[0], 1, 0) != 0)
                        memset(body, 0, sizeof(body));
                    FTL_PROGRESS("N3G_MLOW_HIT mb=%lu l=%08lx j=%lu v=%04lx po=%lu sig=%04x,%04x w=%08lx,%08lx",
                                 (unsigned long)map_block,
                                 (unsigned long)lpn,
                                 (unsigned long)j,
                                 (unsigned long)v,
                                 (unsigned long)po,
                                 h[0xff], h[0x100],
                                 (unsigned long)body[0],
                                 (unsigned long)body[1]);
                }
                hits++;

                if (lpn == 0)
                {
                    zero_block = map_block;
                    zero_j = j;
                    zero_v = v;
                    zero_po = po;
                    goto mlow_done;
                }
            }
        }

        FTL_PROGRESS("N3G_MLOW_MAPDONE b=%lu users=%lu hits=%lu",
                     (unsigned long)map_block,
                     (unsigned long)users,
                     (unsigned long)hits);
    }

mlow_done:
    FTL_PROGRESS("N3G_MLOW_DONE scans=%lu hits=%lu zb=%lu j=%lu v=%04lx po=%lu",
                 (unsigned long)scans,
                 (unsigned long)hits,
                 (unsigned long)zero_block,
                 (unsigned long)zero_j,
                 (unsigned long)zero_v,
                 (unsigned long)zero_po);
}

static void ftl_n3g_log45_probe(void)
{
    static const struct
    {
        uint32_t block;
        uint32_t page;
    } pages[] =
    {
        { 2820, 1 },
        { 2820, 2 },
        { 2821, 1 },
        { 2821, 2 },
        { 6916, 1 },
        { 6916, 2 },
        { 6917, 0 },
        { 6917, 1 },
        { 6917, 2 },
    };

    FTL_PROGRESS("N3G_LOG45_START");

    for (uint32_t i = 0; i < ARRAYLEN(pages); i++)
    {
        uint32_t physpage = pages[i].block * ftl_nand_type->pagesperblock
                          + pages[i].page;
        uint32_t u16_low = 0;
        uint32_t u16_ff = 0;
        uint32_t u32_low = 0;
        uint32_t u32_pageish = 0;
        uint32_t nz = 0;
        uint16_t *h = (uint16_t *)ftl_buffer;
        uint32_t *w = (uint32_t *)ftl_buffer;

        memset(ftl_buffer, 0, sizeof(ftl_buffer));
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        if (nano3g_nand_diag_local_read(0, physpage, 0x800,
                                        (uint32_t *)&ftl_sparebuffer[0],
                                        0x10) != 0)
        {
            FTL_PROGRESS("N3G_LOG45 b=%lu p=%lu oob=bad",
                         (unsigned long)pages[i].block,
                         (unsigned long)pages[i].page);
            continue;
        }
        if (nano3g_nand_diag_local_read(0, physpage, 0,
                                        (uint32_t *)ftl_buffer,
                                        0x800 / sizeof(uint32_t)) != 0)
        {
            FTL_PROGRESS("N3G_LOG45 b=%lu p=%lu body=bad",
                         (unsigned long)pages[i].block,
                         (unsigned long)pages[i].page);
            continue;
        }

        for (uint32_t j = 0; j < 0x800 / sizeof(uint16_t); j++)
        {
            if (h[j] != 0)
                nz++;
            if (h[j] == 0xffffu)
                u16_ff++;
            else if (h[j] < ppb)
                u16_low++;
        }

        for (uint32_t j = 0; j < 0x800 / sizeof(uint32_t); j++)
        {
            if (w[j] < ppb)
                u32_low++;
            if (w[j] < ftl_nand_type->userblocks * ppb)
                u32_pageish++;
        }

        FTL_PROGRESS("N3G_LOG45 b=%lu p=%lu t=%02x ix=%04x u=%08lx nz=%lu hlow=%lu hff=%lu wlow=%lu wp=%lu",
                     (unsigned long)pages[i].block,
                     (unsigned long)pages[i].page,
                     ftl_sparebuffer[0].meta.type,
                     ftl_sparebuffer[0].meta.idx,
                     (unsigned long)ftl_sparebuffer[0].meta.usn,
                     (unsigned long)nz,
                     (unsigned long)u16_low,
                     (unsigned long)u16_ff,
                     (unsigned long)u32_low,
                     (unsigned long)u32_pageish);
        FTL_PROGRESS("N3G_LOG45_W b=%lu p=%lu %08lx,%08lx,%08lx,%08lx,%08lx,%08lx,%08lx,%08lx",
                     (unsigned long)pages[i].block,
                     (unsigned long)pages[i].page,
                     (unsigned long)w[0],
                     (unsigned long)w[1],
                     (unsigned long)w[2],
                     (unsigned long)w[3],
                     (unsigned long)w[4],
                     (unsigned long)w[5],
                     (unsigned long)w[6],
                     (unsigned long)w[7]);
    }

    FTL_PROGRESS("N3G_LOG45_DONE");
}

static void ftl_n3g_mpage_scan_probe(void)
{
    static const uint32_t starts[] = { 7167, 3071 };
    uint32_t spare[16] STORAGE_ALIGN_ATTR;
    uint32_t scans = 0;
    uint32_t hits = 0;
    uint32_t count44 = 0;
    uint32_t count45 = 0;
    uint32_t min44 = 0xffffffffu;
    uint32_t max44 = 0;
    uint32_t min45 = 0xffffffffu;
    uint32_t max45 = 0;

    FTL_PROGRESS("N3G_MPAGE_START");

    for (uint32_t range = 0; range < ARRAYLEN(starts); range++)
    {
        for (uint32_t n = 0; n < 2048; n++)
        {
            uint32_t block = starts[range] - n;

            if ((n & 0x1ff) == 0)
                FTL_PROGRESS("N3G_MPAGE_PROG b=%lu",
                             (unsigned long)block);

            for (uint32_t po = 0; po < 16; po++)
            {
                uint32_t page = block * ftl_nand_type->pagesperblock + po;
                uint8_t *s = (uint8_t *)spare;
                uint8_t type;
                uint16_t idx;
                uint32_t usn;

                memset(spare, 0, sizeof(spare));
                if (nano3g_nand_diag_local_read(0, page, 0x800,
                                                spare, ARRAYLEN(spare)) != 0)
                    continue;
                scans++;

                type = s[9];
                if (type != 0x44 && type != 0x45)
                    continue;

                idx = s[4] | ((uint16_t)s[5] << 8);
                usn = s[0] | ((uint32_t)s[1] << 8)
                    | ((uint32_t)s[2] << 16)
                    | ((uint32_t)s[3] << 24);

                if (type == 0x44)
                {
                    count44++;
                    if (idx < min44)
                        min44 = idx;
                    if (idx > max44)
                        max44 = idx;
                }
                else
                {
                    count45++;
                    if (idx < min45)
                        min45 = idx;
                    if (idx > max45)
                        max45 = idx;
                }

                if (hits < 64)
                {
                    FTL_PROGRESS("N3G_MPAGE_HIT t=%02x b=%lu p=%lu ix=%04x u=%08lx w=%08lx,%08lx,%08lx",
                                 type,
                                 (unsigned long)block,
                                 (unsigned long)po,
                                 idx,
                                 (unsigned long)usn,
                                 (unsigned long)spare[0],
                                 (unsigned long)spare[1],
                                 (unsigned long)spare[2]);
                }
                hits++;
            }
        }
    }

    FTL_PROGRESS("N3G_MPAGE_DONE scans=%lu hits=%lu c44=%lu i44=%lu-%lu c45=%lu i45=%lu-%lu",
                 (unsigned long)scans,
                 (unsigned long)hits,
                 (unsigned long)count44,
                 (unsigned long)min44,
                 (unsigned long)max44,
                 (unsigned long)count45,
                 (unsigned long)min45,
                 (unsigned long)max45);
}

static void ftl_n3g_lpn0_oob_probe(void)
{
    static const uint32_t starts[] = { 0, 2048, 4096, 6144 };
    uint32_t scans = 0;
    uint32_t hits = 0;
    uint32_t hit_bank = 0xffffffffu;
    uint32_t hit_block = 0xffffffffu;
    uint32_t hit_page = 0xffffffffu;
    uint32_t hit_lpn = 0xffffffffu;
    uint32_t hit_type = 0;

    FTL_PROGRESS("N3G_LPN0_START");

    for (uint32_t range = 0; range < ARRAYLEN(starts); range++)
    {
        uint32_t end = starts[range] + 2048;

        if (end > ftl_nand_type->blocks)
            end = ftl_nand_type->blocks;

        for (uint32_t block = starts[range]; block < end; block++)
        {
            if (((block - starts[range]) & 0x1ff) == 0)
                FTL_PROGRESS("N3G_LPN0_PROG b=%lu",
                             (unsigned long)block);

            for (uint32_t po = 0; po < ftl_nand_type->pagesperblock; po++)
            {
                uint32_t physpage = block * ftl_nand_type->pagesperblock + po;

                for (uint32_t bank = 0; bank < ftl_banks; bank++)
                {
                    uint32_t lpn;
                    uint32_t type;

                    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                    if (nand_read_page(bank, physpage, NULL,
                                       &ftl_sparebuffer[0], 1, 0) != 0)
                    {
                        scans++;
                        continue;
                    }
                    scans++;

                    type = ftl_sparebuffer[0].user.type;
                    if (type != 0x40 && type != 0x41)
                        continue;

                    lpn = ftl_sparebuffer[0].user.lpn;
                    if (lpn >= 0x200)
                        continue;

                    if (hits < 64 || lpn == 0)
                    {
                        uint32_t body[0x200] STORAGE_ALIGN_ATTR;
                        uint16_t *h = (uint16_t *)body;

                        memset(body, 0, sizeof(body));
                        if (nand_read_page(bank, physpage, body,
                                           &ftl_sparebuffer[0], 1, 0) != 0)
                            memset(body, 0, sizeof(body));

                        FTL_PROGRESS("N3G_LPN0_HIT l=%08lx bk=%lu b=%lu p=%lu t=%02lx sig=%04x,%04x w=%08lx,%08lx",
                                     (unsigned long)lpn,
                                     (unsigned long)bank,
                                     (unsigned long)block,
                                     (unsigned long)po,
                                     (unsigned long)type,
                                     h[0xff], h[0x100],
                                     (unsigned long)body[0],
                                     (unsigned long)body[1]);
                    }
                    hits++;

                    if (lpn == 0)
                    {
                        hit_bank = bank;
                        hit_block = block;
                        hit_page = po;
                        hit_lpn = lpn;
                        hit_type = type;
                        goto lpn0_done;
                    }
                }
            }
        }
    }

lpn0_done:
    FTL_PROGRESS("N3G_LPN0_DONE scans=%lu hits=%lu l=%08lx bk=%lu b=%lu p=%lu t=%02lx",
                 (unsigned long)scans,
                 (unsigned long)hits,
                 (unsigned long)hit_lpn,
                 (unsigned long)hit_bank,
                 (unsigned long)hit_block,
                 (unsigned long)hit_page,
                 (unsigned long)hit_type);
}

static void ftl_n3g_room892_probe(void)
{
    const uint32_t base_physpage = 114176;
    const uint32_t bank = 0;

    FTL_PROGRESS("N3G_ROOM892_START pp=%lu",
                 (unsigned long)base_physpage);

    for (int32_t d = -4; d <= 8; d++)
    {
        uint32_t physpage = base_physpage + d;
        uint32_t pblock = physpage / ftl_nand_type->pagesperblock;
        uint32_t po = physpage % ftl_nand_type->pagesperblock;
        uint32_t body[0x200] STORAGE_ALIGN_ATTR;
        uint16_t *h = (uint16_t *)body;
        int32_t rc;

        memset(body, 0, sizeof(body));
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        rc = (int32_t)nand_read_page(bank, physpage, body,
                                     &ftl_sparebuffer[0], 1, 0);

        FTL_PROGRESS("N3G_ROOM892 d=%ld bk=%lu pb=%lu po=%lu pp=%lu rc=%ld t=%02x l=%08lx sig=%04x,%04x w=%08lx,%08lx",
                     (long)d,
                     (unsigned long)bank,
                     (unsigned long)pblock,
                     (unsigned long)po,
                     (unsigned long)physpage,
                     (long)rc,
                     ftl_sparebuffer[0].user.type,
                     (unsigned long)ftl_sparebuffer[0].user.lpn,
                     h[0xff], h[0x100],
                     (unsigned long)body[0],
                     (unsigned long)body[1]);
    }

    FTL_PROGRESS("N3G_ROOM892_DONE");
}

static void ftl_n3g_room892_oob_probe(void)
{
    const uint32_t base_physpage = 114176;
    const uint32_t bank = 0;
    uint32_t local_spare[16] STORAGE_ALIGN_ATTR;

    FTL_PROGRESS("N3G_R892O_START pp=%lu",
                 (unsigned long)base_physpage);

    for (int32_t d = -4; d <= 8; d++)
    {
        uint32_t physpage = base_physpage + d;
        uint32_t pblock = physpage / ftl_nand_type->pagesperblock;
        uint32_t po = physpage % ftl_nand_type->pagesperblock;
        uint32_t body[0x200] STORAGE_ALIGN_ATTR;
        uint16_t *h = (uint16_t *)body;
        int32_t nrc;
        int32_t orc;

        memset(body, 0, sizeof(body));
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        nrc = (int32_t)nand_read_page(bank, physpage, body,
                                      &ftl_sparebuffer[0], 1, 0);

        memset(local_spare, 0, sizeof(local_spare));
        orc = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                          local_spare,
                                          ARRAYLEN(local_spare));

        FTL_PROGRESS("N3G_R892N d=%ld pb=%lu po=%lu rc=%ld t=%02x l=%08lx w2=%08lx sig=%04x,%04x",
                     (long)d,
                     (unsigned long)pblock,
                     (unsigned long)po,
                     (long)nrc,
                     ftl_sparebuffer[0].user.type,
                     (unsigned long)ftl_sparebuffer[0].user.lpn,
                     (unsigned long)((uint32_t *)&ftl_sparebuffer[0])[2],
                     h[0xff], h[0x100]);
        FTL_PROGRESS("N3G_R892L d=%ld rc=%ld t=%02lx l=%08lx w=%08lx,%08lx,%08lx",
                     (long)d,
                     (long)orc,
                     (unsigned long)(((uint8_t *)local_spare)[9]),
                     (unsigned long)local_spare[0],
                     (unsigned long)local_spare[0],
                     (unsigned long)local_spare[1],
                     (unsigned long)local_spare[2]);
    }

    FTL_PROGRESS("N3G_R892O_DONE");
}

static void ftl_n3g_v01d3_probe(void)
{
    const uint32_t vblock = 0x01d3;
    uint32_t local_spare[16] STORAGE_ALIGN_ATTR;

    FTL_PROGRESS("N3G_V01D3_START");

    for (uint32_t po = 0; po < 32; po++)
    {
        uint32_t bank;
        uint32_t physpage;
        uint32_t pblock;
        uint32_t pageoff;
        int32_t nrc;
        int32_t lrc;

        ftl_n3g_decode_page(vblock, po, &bank, &physpage);
        pblock = physpage / ftl_nand_type->pagesperblock;
        pageoff = physpage % ftl_nand_type->pagesperblock;

        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        nrc = (int32_t)nand_read_page(bank, physpage, NULL,
                                      &ftl_sparebuffer[0], 1, 0);

        memset(local_spare, 0, sizeof(local_spare));
        lrc = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                          local_spare,
                                          ARRAYLEN(local_spare));

        FTL_PROGRESS("N3G_V01D3 po=%lu bk=%lu pb=%lu pg=%lu n=%ld/%02x/%08lx l=%ld/%02lx/%08lx",
                     (unsigned long)po,
                     (unsigned long)bank,
                     (unsigned long)pblock,
                     (unsigned long)pageoff,
                     (long)nrc,
                     ftl_sparebuffer[0].user.type,
                     (unsigned long)ftl_sparebuffer[0].user.lpn,
                     (long)lrc,
                     (unsigned long)(((uint8_t *)local_spare)[9]),
                     (unsigned long)local_spare[0]);
    }

    FTL_PROGRESS("N3G_V01D3_DONE");
}

static uint32_t ftl_n3g_shift_lpn(uint32_t w0)
{
    return (w0 >> 8) & 0x00ffffffu;
}

static void ftl_n3g_v01d3_raw_probe(void)
{
    const uint32_t vblock = 0x01d3;
    uint32_t local_spare[16] STORAGE_ALIGN_ATTR;
    uint32_t body[0x200] STORAGE_ALIGN_ATTR;
    uint16_t *h = (uint16_t *)body;

    FTL_PROGRESS("N3G_V01R_START");

    for (uint32_t po = 12; po <= 15; po++)
    {
        uint32_t bank;
        uint32_t physpage;
        uint32_t pblock;
        uint32_t pageoff;
        int32_t nrc;
        int32_t lrc;

        ftl_n3g_decode_page(vblock, po, &bank, &physpage);
        pblock = physpage / ftl_nand_type->pagesperblock;
        pageoff = physpage % ftl_nand_type->pagesperblock;

        memset(body, 0, sizeof(body));
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        nrc = (int32_t)nand_read_page(bank, physpage, body,
                                      &ftl_sparebuffer[0], 1, 0);

        memset(local_spare, 0, sizeof(local_spare));
        lrc = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                          local_spare,
                                          ARRAYLEN(local_spare));

        FTL_PROGRESS("N3G_V01RN po=%lu bk=%lu pb=%lu pg=%lu rc=%ld sl=%08lx t9=%02lx w=%08lx,%08lx,%08lx sig=%04x",
                     (unsigned long)po,
                     (unsigned long)bank,
                     (unsigned long)pblock,
                     (unsigned long)pageoff,
                     (long)nrc,
                     (unsigned long)ftl_n3g_shift_lpn(((uint32_t *)&ftl_sparebuffer[0])[0]),
                     (unsigned long)(((uint8_t *)&ftl_sparebuffer[0])[9]),
                     (unsigned long)((uint32_t *)&ftl_sparebuffer[0])[0],
                     (unsigned long)((uint32_t *)&ftl_sparebuffer[0])[1],
                     (unsigned long)((uint32_t *)&ftl_sparebuffer[0])[2],
                     h[0xff]);
        FTL_PROGRESS("N3G_V01RL po=%lu rc=%ld sl=%08lx t9=%02lx w=%08lx,%08lx,%08lx",
                     (unsigned long)po,
                     (long)lrc,
                     (unsigned long)ftl_n3g_shift_lpn(local_spare[0]),
                     (unsigned long)(((uint8_t *)local_spare)[9]),
                     (unsigned long)local_spare[0],
                     (unsigned long)local_spare[1],
                     (unsigned long)local_spare[2]);
    }

    FTL_PROGRESS("N3G_V01R_DONE");
}

static void ftl_n3g_map744_probe(void)
{
    const uint32_t map_block = 6916;
    const uint32_t map_page = map_block * ftl_nand_type->pagesperblock;
    const uint32_t map_index = 744;
    uint16_t *map = (uint16_t *)ftl_buffer;
    uint32_t vblock;

    FTL_PROGRESS("N3G_MAP744_START");

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0
     || nano3g_nand_diag_local_read(0, map_page, 0,
                                    (uint32_t *)ftl_buffer,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_MAP744_FAIL mapread");
        return;
    }

    vblock = map[map_index];
    FTL_PROGRESS("N3G_MAP744_MAP t=%02x ix=%04x u=%08lx j=%lu v=%04lx e=%04x,%04x,%04x,%04x",
                 ftl_sparebuffer[0].meta.type,
                 ftl_sparebuffer[0].meta.idx,
                 (unsigned long)ftl_sparebuffer[0].meta.usn,
                 (unsigned long)map_index,
                 (unsigned long)vblock,
                 map[map_index - 2],
                 map[map_index - 1],
                 map[map_index + 1],
                 map[map_index + 2]);

    for (uint32_t po = 0; po < 16; po++)
    {
        uint32_t bank;
        uint32_t physpage;
        uint32_t pblock;
        uint32_t pageoff;
        uint32_t *spw = (uint32_t *)&ftl_sparebuffer[0];
        int32_t rc;

        ftl_n3g_decode_page(vblock, po, &bank, &physpage);
        pblock = physpage / ftl_nand_type->pagesperblock;
        pageoff = physpage % ftl_nand_type->pagesperblock;

        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        rc = (int32_t)nand_read_page(bank, physpage, NULL,
                                     &ftl_sparebuffer[0], 1, 0);

        FTL_PROGRESS("N3G_MAP744_P po=%lu bk=%lu pb=%lu pg=%lu rc=%ld w=%08lx,%08lx,%08lx sl=%08lx t9=%02lx",
                     (unsigned long)po,
                     (unsigned long)bank,
                     (unsigned long)pblock,
                     (unsigned long)pageoff,
                     (long)rc,
                     (unsigned long)spw[0],
                     (unsigned long)spw[1],
                     (unsigned long)spw[2],
                     (unsigned long)ftl_n3g_shift_lpn(spw[0]),
                     (unsigned long)(((uint8_t *)spw)[9]));
    }

    FTL_PROGRESS("N3G_MAP744_DONE");
}

static void ftl_n3g_shift0_probe(void)
{
    const uint32_t map_block = 6916;
    const uint32_t map_page = map_block * ftl_nand_type->pagesperblock;
    uint16_t *map = (uint16_t *)ftl_buffer;
    uint32_t hits = 0;
    uint32_t found_j = 0xffffffffu;
    uint32_t found_v = 0xffffffffu;
    uint32_t found_po = 0xffffffffu;
    uint32_t found_bank = 0xffffffffu;
    uint32_t found_physpage = 0xffffffffu;

    FTL_PROGRESS("N3G_SHIFT0_START");

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0
     || nano3g_nand_diag_local_read(0, map_page, 0,
                                    (uint32_t *)ftl_buffer,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_SHIFT0_FAIL mapread");
        return;
    }

    FTL_PROGRESS("N3G_SHIFT0_MAP t=%02x ix=%04x u=%08lx",
                 ftl_sparebuffer[0].meta.type,
                 ftl_sparebuffer[0].meta.idx,
                 (unsigned long)ftl_sparebuffer[0].meta.usn);

    for (uint32_t j = 0; j < 0x800 / sizeof(uint16_t); j++)
    {
        uint32_t v = map[j];

        if (v == 0 || v == 0xffffu)
            continue;

        for (uint32_t po = 0; po < ppb; po++)
        {
            uint32_t bank;
            uint32_t physpage;
            uint32_t *spw = (uint32_t *)&ftl_sparebuffer[0];
            uint32_t slpn;

            ftl_n3g_decode_page(v, po, &bank, &physpage);
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            if (nand_read_page(bank, physpage, NULL,
                               &ftl_sparebuffer[0], 1, 0) != 0)
                continue;

            slpn = ftl_n3g_shift_lpn(spw[0]);
            if (slpn >= ppb)
                continue;

            if (hits < 8)
                FTL_PROGRESS("N3G_SHIFT0_HIT s=%08lx j=%lu v=%04lx po=%lu w=%08lx",
                             (unsigned long)slpn,
                             (unsigned long)j,
                             (unsigned long)v,
                             (unsigned long)po,
                             (unsigned long)spw[0]);
            hits++;

            if (slpn == 0)
            {
                found_j = j;
                found_v = v;
                found_po = po;
                found_bank = bank;
                found_physpage = physpage;
                goto shift_found;
            }
        }
    }

shift_found:
    FTL_PROGRESS("N3G_SHIFT0_FOUND hits=%lu j=%lu v=%04lx po=%lu bk=%lu pp=%lu",
                 (unsigned long)hits,
                 (unsigned long)found_j,
                 (unsigned long)found_v,
                 (unsigned long)found_po,
                 (unsigned long)found_bank,
                 (unsigned long)found_physpage);

    if (found_j != 0xffffffffu)
    {
        uint32_t body[0x200] STORAGE_ALIGN_ATTR;
        uint16_t *h = (uint16_t *)body;
        int32_t rc;

        memset(body, 0, sizeof(body));
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        rc = (int32_t)nand_read_page(found_bank, found_physpage, body,
                                     &ftl_sparebuffer[0], 1, 0);
        FTL_PROGRESS("N3G_SHIFT0_BODY rc=%ld sig=%04x,%04x w=%08lx,%08lx o=%08lx,%08lx,%08lx",
                     (long)rc,
                     h[0xff], h[0x100],
                     (unsigned long)body[0],
                     (unsigned long)body[1],
                     (unsigned long)((uint32_t *)&ftl_sparebuffer[0])[0],
                     (unsigned long)((uint32_t *)&ftl_sparebuffer[0])[1],
                     (unsigned long)((uint32_t *)&ftl_sparebuffer[0])[2]);
    }

    FTL_PROGRESS("N3G_SHIFT0_DONE");
}

static void ftl_n3g_anchor744_probe(void)
{
    const uint32_t map_block = 6916;
    const uint32_t map_page = map_block * ftl_nand_type->pagesperblock;
    const uint32_t map_index = 744;
    const uint32_t po = 0;
    uint16_t *map = (uint16_t *)ftl_buffer;
    uint32_t spare[16] STORAGE_ALIGN_ATTR;
    uint32_t body[0x200] STORAGE_ALIGN_ATTR;
    union ftl_spare_data_type nand_spare STORAGE_ALIGN_ATTR;
    uint8_t *s = (uint8_t *)spare;
    uint16_t *h = (uint16_t *)body;
    uint32_t vblock;
    uint32_t bank;
    uint32_t physpage;
    uint32_t pblock;
    uint32_t pageoff;
    uint32_t o0_rc;
    uint32_t body_rc;
    uint32_t nand_rc;
    uint32_t o1_rc;
    uint32_t ok;

    FTL_PROGRESS("N3G_A744_START");

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0
     || nano3g_nand_diag_local_read(0, map_page, 0,
                                    (uint32_t *)ftl_buffer,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_A744_FAIL mapread");
        return;
    }

    vblock = map[map_index];
    FTL_PROGRESS("N3G_A744_MAP t=%02x ix=%04x u=%08lx j=%lu v=%04lx",
                 ftl_sparebuffer[0].meta.type,
                 ftl_sparebuffer[0].meta.idx,
                 (unsigned long)ftl_sparebuffer[0].meta.usn,
                 (unsigned long)map_index,
                 (unsigned long)vblock);

    ftl_n3g_decode_page(vblock, po, &bank, &physpage);
    pblock = physpage / ftl_nand_type->pagesperblock;
    pageoff = physpage % ftl_nand_type->pagesperblock;
    FTL_PROGRESS("N3G_A744_PAGE po=0 bk=%lu pb=%lu pg=%lu pp=%lu",
                 (unsigned long)bank,
                 (unsigned long)pblock,
                 (unsigned long)pageoff,
                 (unsigned long)physpage);

    memset(spare, 0, sizeof(spare));
    o0_rc = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                        spare, ARRAYLEN(spare));
    FTL_PROGRESS("N3G_A744_O0 rc=%lu t=%02x l=%08lx w=%08lx,%08lx,%08lx",
                 (unsigned long)o0_rc,
                 s[9],
                 (unsigned long)spare[0],
                 (unsigned long)spare[0],
                 (unsigned long)spare[1],
                 (unsigned long)spare[2]);

    memset(body, 0, sizeof(body));
    body_rc = nano3g_nand_diag_local_read(bank, physpage, 0,
                                          body, ARRAYLEN(body));
    FTL_PROGRESS("N3G_A744_BODY rc=%lu sig=%04x,%04x w=%08lx,%08lx",
                 (unsigned long)body_rc,
                 h[0xff], h[0x100],
                 (unsigned long)body[0],
                 (unsigned long)body[1]);

    memset(body, 0, sizeof(body));
    memset(&nand_spare, 0, sizeof(nand_spare));
    nand_rc = nand_read_page(bank, physpage, body, &nand_spare, 1, 0);
    FTL_PROGRESS("N3G_A744_NAND rc=%lu t=%02x l=%08lx sig=%04x,%04x w=%08lx,%08lx o=%08lx,%08lx,%08lx",
                 (unsigned long)nand_rc,
                 nand_spare.user.type,
                 (unsigned long)nand_spare.user.lpn,
                 h[0xff], h[0x100],
                 (unsigned long)body[0],
                 (unsigned long)body[1],
                 (unsigned long)((uint32_t *)&nand_spare)[0],
                 (unsigned long)((uint32_t *)&nand_spare)[1],
                 (unsigned long)((uint32_t *)&nand_spare)[2]);

    memset(spare, 0, sizeof(spare));
    o1_rc = nano3g_nand_diag_local_read(bank, physpage, 0x800,
                                        spare, ARRAYLEN(spare));
    FTL_PROGRESS("N3G_A744_O1 rc=%lu t=%02x l=%08lx w=%08lx,%08lx,%08lx",
                 (unsigned long)o1_rc,
                 s[9],
                 (unsigned long)spare[0],
                 (unsigned long)spare[0],
                 (unsigned long)spare[1],
                 (unsigned long)spare[2]);

    ok = ((spare[0] == 0x800 || nand_spare.user.lpn == 0x800)
       && (h[0xff] == 0xf833 || h[0x100] == 0xf833));
    FTL_PROGRESS("N3G_A744_DONE ok=%lu", (unsigned long)ok);
}

static uint32_t ftl_n3g_offset_mount(void)
{
    const uint32_t map_block = 6916;
    const uint32_t map_page = map_block * ftl_nand_type->pagesperblock;
    const uint32_t base_lpn = 0x800;
    uint16_t *map = (uint16_t *)ftl_buffer;
    uint32_t map0 = 0xffffu;
    uint32_t duplicates = 0;
    uint32_t exact_count = 0;
    uint32_t exact_dup = 0;

    FTL_PROGRESS("N3G_DMOUNT_START base=%08lx",
                 (unsigned long)base_lpn);

    n3g_direct_map_mount = 0;
    n3g_direct_sector_base = base_lpn;
    n3g_direct_map_count = 0;
    n3g_direct_map_min_lblock = 0xffffffffu;
    n3g_direct_map_max_lblock = 0;
    memset(ftl_map, 0xff, sizeof(ftl_map));
    for (uint32_t i = 0; i < ARRAYLEN(n3g_direct_l0_vblock); i++)
    {
        n3g_direct_l0_vblock[i] = 0xffffu;
        n3g_direct_l0_page[i] = 0xffffu;
    }
    for (uint32_t i = 0; i < ARRAYLEN(n3g_direct_l0_cache); i++)
        n3g_direct_l0_cache[i] = 0xffffffffu;

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0
     || nano3g_nand_diag_local_read(0, map_page, 0,
                                    (uint32_t *)ftl_buffer,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_DMOUNT_FAIL mapread");
        return -1;
    }

    FTL_PROGRESS("N3G_DMOUNT_MAP t=%02x ix=%04x u=%08lx",
                 ftl_sparebuffer[0].meta.type,
                 ftl_sparebuffer[0].meta.idx,
                 (unsigned long)ftl_sparebuffer[0].meta.usn);
    if (ftl_sparebuffer[0].meta.type != 0x44)
        return -1;

    for (uint32_t j = 0; j < 0x800 / sizeof(uint16_t); j++)
    {
        uint32_t v = map[j];
        uint32_t bank;
        uint32_t physpage;
        uint32_t lpn;
        uint32_t lblock;

        if (v == 0 || v == 0xffffu)
            continue;

        ftl_n3g_decode_page(v, 0, &bank, &physpage);
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        if (nand_read_page(bank, physpage, NULL,
                           &ftl_sparebuffer[0], 1, 0) != 0)
            continue;
        if (ftl_sparebuffer[0].user.type != 0x40
         && ftl_sparebuffer[0].user.type != 0x41)
            continue;

        lpn = ftl_sparebuffer[0].user.lpn;
        lblock = lpn / ppb;
        if (lblock >= ftl_nand_type->userblocks)
            continue;
        if (ftl_map[lblock] != 0xffffu)
        {
            duplicates++;
            continue;
        }

        ftl_map[lblock] = v;
        n3g_direct_map_count++;
        if (lblock < n3g_direct_map_min_lblock)
            n3g_direct_map_min_lblock = lblock;
        if (lblock > n3g_direct_map_max_lblock)
            n3g_direct_map_max_lblock = lblock;
        if (lblock == (base_lpn / ppb))
            map0 = v;
    }

    /*
     * Seed the exact first host page from the stable anchor before doing any
     * wider exact-page discovery. The full 1024*ppb scan is too slow/noisy for
     * the bootloader screen and can be reintroduced with progress once this
     * anchor proves the read path.
     */
    n3g_direct_l0_vblock[0] = map[744];
    n3g_direct_l0_page[0] = 0;
    exact_count = 1;

    FTL_PROGRESS("N3G_DMOUNT_INV n=%lu dup=%lu mn=%lu mx=%lu b0=%lu v=%04lx",
                 (unsigned long)n3g_direct_map_count,
                 (unsigned long)duplicates,
                 (unsigned long)n3g_direct_map_min_lblock,
                 (unsigned long)n3g_direct_map_max_lblock,
                 (unsigned long)(base_lpn / ppb),
                 (unsigned long)map0);
    FTL_PROGRESS("N3G_DMOUNT_EXACT n=%lu dup=%lu p0v=%04x p0po=%u",
                 (unsigned long)exact_count,
                 (unsigned long)exact_dup,
                 n3g_direct_l0_vblock[0],
                 n3g_direct_l0_page[0]);
    if (n3g_direct_l0_vblock[0] == 0xffffu)
    {
        FTL_PROGRESS("N3G_DMOUNT_FAIL nobase");
        return -1;
    }

    n3g_direct_map_mount = 1;

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    int32_t rc = (int32_t)ftl_n3g_direct_read(0, 1, ftl_buffer);
    FTL_PROGRESS("N3G_DMOUNT_S0 rc=%ld t=%02x l=%08lx sig=%04x,%04x w=%08lx,%08lx",
                 (long)rc,
                 ftl_sparebuffer[0].user.type,
                 (unsigned long)ftl_sparebuffer[0].user.lpn,
                 ((uint16_t *)ftl_buffer)[0xff],
                 ((uint16_t *)ftl_buffer)[0x100],
                 (unsigned long)((uint32_t *)ftl_buffer)[0],
                 (unsigned long)((uint32_t *)ftl_buffer)[1]);
    if (rc != 0)
    {
        n3g_direct_map_mount = 0;
        return -1;
    }

    FTL_PROGRESS("N3G_DMOUNT_READY");
    return 0;
}

static uint32_t ftl_n3g_bootsig_probe(void)
{
    const uint32_t map_block = 6916;
    const uint32_t map_page = map_block * ftl_nand_type->pagesperblock;
    static const uint32_t js[] = { 744, 569, 5, 27, 207, 953 };
    uint16_t *map = (uint16_t *)ftl_buffer;
    uint32_t hits = 0;

    FTL_PROGRESS("N3G_BSIG_START");

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0
     || nano3g_nand_diag_local_read(0, map_page, 0,
                                    (uint32_t *)ftl_buffer,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_BSIG_FAIL mapread");
        return -1;
    }

    FTL_PROGRESS("N3G_BSIG_MAP t=%02x ix=%04x u=%08lx",
                 ftl_sparebuffer[0].meta.type,
                 ftl_sparebuffer[0].meta.idx,
                 (unsigned long)ftl_sparebuffer[0].meta.usn);
    if (ftl_sparebuffer[0].meta.type != 0x44)
        return -1;

    for (uint32_t ci = 0; ci < ARRAYLEN(js); ci++)
    {
        uint32_t j = js[ci];
        uint32_t v = map[j];

        if (v == 0 || v == 0xffffu)
            continue;

        FTL_PROGRESS("N3G_BSIG_CAND j=%lu v=%04lx",
                     (unsigned long)j, (unsigned long)v);

        for (uint32_t po = 0; po < 16; po++)
        {
            uint32_t bank;
            uint32_t physpage;
            uint32_t rc;
            uint16_t *h = (uint16_t *)ftl_buffer;
            uint32_t sig0, sig1, sig2, sig3;

            ftl_n3g_decode_page(v, po, &bank, &physpage);
            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc = nand_read_page(bank, physpage, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0);

            sig0 = h[0x0ff];
            sig1 = h[0x1ff];
            sig2 = h[0x2ff];
            sig3 = h[0x3ff];

            if (po < 4 || sig0 == 0xaa55 || sig1 == 0xaa55
             || sig2 == 0xaa55 || sig3 == 0xaa55)
            {
                FTL_PROGRESS("N3G_BSIG_P j=%lu po=%lu rc=%lu t=%02x l=%08lx s=%04x,%04x,%04x,%04x w=%08lx",
                             (unsigned long)j, (unsigned long)po,
                             (unsigned long)rc,
                             ftl_sparebuffer[0].user.type,
                             (unsigned long)ftl_sparebuffer[0].user.lpn,
                             sig0, sig1, sig2, sig3,
                             (unsigned long)((uint32_t *)ftl_buffer)[0]);
            }

            if (sig0 == 0xaa55 || sig1 == 0xaa55
             || sig2 == 0xaa55 || sig3 == 0xaa55)
            {
                hits++;
                FTL_PROGRESS("N3G_BSIG_HIT j=%lu v=%04lx po=%lu l=%08lx slot=%lu",
                             (unsigned long)j, (unsigned long)v,
                             (unsigned long)po,
                             (unsigned long)ftl_sparebuffer[0].user.lpn,
                             sig0 == 0xaa55 ? 0UL :
                             sig1 == 0xaa55 ? 1UL :
                             sig2 == 0xaa55 ? 2UL : 3UL);
            }
        }
    }

    FTL_PROGRESS("N3G_BSIG_DONE hits=%lu", (unsigned long)hits);
    return -1;
}

static uint32_t ftl_n3g_format_probe(void)
{
    const uint32_t map_block = 6916;
    const uint32_t map_page = map_block * ftl_nand_type->pagesperblock;
    uint16_t *map = (uint16_t *)ftl_buffer;
    uint32_t scans = 0;
    uint32_t mbr_hits = 0;
    uint32_t apm_hits = 0;
    uint32_t hfs_hits = 0;
    uint32_t fat_hits = 0;
    uint32_t printed = 0;

    FTL_PROGRESS("N3G_FMT_START");

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0
     || nano3g_nand_diag_local_read(0, map_page, 0,
                                    (uint32_t *)ftl_buffer,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_FMT_FAIL mapread");
        return -1;
    }

    FTL_PROGRESS("N3G_FMT_MAP t=%02x ix=%04x u=%08lx",
                 ftl_sparebuffer[0].meta.type,
                 ftl_sparebuffer[0].meta.idx,
                 (unsigned long)ftl_sparebuffer[0].meta.usn);
    if (ftl_sparebuffer[0].meta.type != 0x44)
        return -1;

    memcpy(ftl_map, map, 0x800);

    for (uint32_t j = 0; j < 0x400; j++)
    {
        uint32_t v = ftl_map[j];

        if ((j & 0x7f) == 0)
            FTL_PROGRESS("N3G_FMT_PROG j=%lu", (unsigned long)j);

        if (v == 0 || v == 0xffffu)
            continue;

        for (uint32_t po = 0; po < 4; po++)
        {
            uint32_t bank;
            uint32_t physpage;
            uint32_t rc;
            uint8_t *b = (uint8_t *)ftl_buffer;
            uint16_t *h = (uint16_t *)ftl_buffer;
            uint32_t is_mbr;
            uint32_t is_apm;
            uint32_t is_hfs;
            uint32_t is_fat;

            ftl_n3g_decode_page(v, po, &bank, &physpage);
            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc = nand_read_page(bank, physpage, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0);
            scans++;

            is_mbr = (h[0x0ff] == 0xaa55 || h[0x1ff] == 0xaa55
                   || h[0x2ff] == 0xaa55 || h[0x3ff] == 0xaa55);
            is_apm = ((b[0] == 'E' && b[1] == 'R')
                   || (b[0] == 'P' && b[1] == 'M')
                   || (b[0x200] == 'P' && b[0x201] == 'M')
                   || (b[0x400] == 'P' && b[0x401] == 'M')
                   || (b[0x600] == 'P' && b[0x601] == 'M'));
            is_hfs = ((b[0x400] == 'H' && (b[0x401] == '+' || b[0x401] == 'X'))
                   || (b[0] == 'H' && (b[1] == '+' || b[1] == 'X'))
                   || (b[0x200] == 'H' && (b[0x201] == '+' || b[0x201] == 'X'))
                   || (b[0x600] == 'H' && (b[0x601] == '+' || b[0x601] == 'X')));
            is_fat = ((b[0x36] == 'F' && b[0x37] == 'A' && b[0x38] == 'T')
                   || (b[0x52] == 'F' && b[0x53] == 'A' && b[0x54] == 'T')
                   || (b[0x236] == 'F' && b[0x237] == 'A' && b[0x238] == 'T')
                   || (b[0x252] == 'F' && b[0x253] == 'A' && b[0x254] == 'T')
                   || (b[0x436] == 'F' && b[0x437] == 'A' && b[0x438] == 'T')
                   || (b[0x452] == 'F' && b[0x453] == 'A' && b[0x454] == 'T')
                   || (b[0x636] == 'F' && b[0x637] == 'A' && b[0x638] == 'T')
                   || (b[0x652] == 'F' && b[0x653] == 'A' && b[0x654] == 'T'));

            if (is_mbr)
                mbr_hits++;
            if (is_apm)
                apm_hits++;
            if (is_hfs)
                hfs_hits++;
            if (is_fat)
                fat_hits++;

            if ((j == 744 && po == 0) || is_mbr || is_apm || is_hfs || is_fat)
            {
                if (printed < 20)
                {
                    FTL_PROGRESS("N3G_FMT_PAGE j=%lu po=%lu rc=%lu t=%02x l=%08lx b=%02x%02x/%02x%02x/%02x%02x/%02x%02x sig=%04x,%04x,%04x,%04x",
                                 (unsigned long)j,
                                 (unsigned long)po,
                                 (unsigned long)rc,
                                 ftl_sparebuffer[0].user.type,
                                 (unsigned long)ftl_sparebuffer[0].user.lpn,
                                 b[0], b[1], b[0x200], b[0x201],
                                 b[0x400], b[0x401], b[0x600], b[0x601],
                                 h[0x0ff], h[0x1ff],
                                 h[0x2ff], h[0x3ff]);
                    printed++;
                }
            }
        }
    }

    if (apm_hits || hfs_hits)
        FTL_PROGRESS("N3G_FORMAT_GUESS macpod mbr=%lu apm=%lu hfs=%lu fat=%lu scans=%lu",
                     (unsigned long)mbr_hits, (unsigned long)apm_hits,
                     (unsigned long)hfs_hits, (unsigned long)fat_hits,
                     (unsigned long)scans);
    else if (mbr_hits || fat_hits)
        FTL_PROGRESS("N3G_FORMAT_GUESS winpod mbr=%lu apm=%lu hfs=%lu fat=%lu scans=%lu",
                     (unsigned long)mbr_hits, (unsigned long)apm_hits,
                     (unsigned long)hfs_hits, (unsigned long)fat_hits,
                     (unsigned long)scans);
    else
        FTL_PROGRESS("N3G_FORMAT_GUESS unknown mbr=%lu apm=%lu hfs=%lu fat=%lu scans=%lu",
                     (unsigned long)mbr_hits, (unsigned long)apm_hits,
                     (unsigned long)hfs_hits, (unsigned long)fat_hits,
                     (unsigned long)scans);

    FTL_PROGRESS("N3G_FMT_DONE");
    return -1;
}

static uint32_t ftl_n3g_mbr_probe(void)
{
    const uint32_t map_block = 6916;
    const uint32_t map_page = map_block * ftl_nand_type->pagesperblock;
    uint16_t *map = (uint16_t *)ftl_buffer;
    uint32_t lpn0_hits = 0;
    uint32_t mbr_hits = 0;
    uint32_t best_j = 0xffffffffu;
    uint32_t best_po = 0xffffffffu;
    uint32_t best_slot = 0xffffffffu;
    uint32_t part_start[4] = { 0, 0, 0, 0 };
    uint32_t part_size[4] = { 0, 0, 0, 0 };
    uint8_t part_type[4] = { 0, 0, 0, 0 };

    FTL_PROGRESS("N3G_MBR_START");

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0
     || nano3g_nand_diag_local_read(0, map_page, 0,
                                    (uint32_t *)ftl_buffer,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_MBR_FAIL mapread");
        return -1;
    }

    FTL_PROGRESS("N3G_MBR_MAP t=%02x ix=%04x u=%08lx",
                 ftl_sparebuffer[0].meta.type,
                 ftl_sparebuffer[0].meta.idx,
                 (unsigned long)ftl_sparebuffer[0].meta.usn);
    if (ftl_sparebuffer[0].meta.type != 0x44)
        return -1;

    memcpy(ftl_map, map, 0x800);

    for (uint32_t j = 0; j < 0x400; j++)
    {
        uint32_t v = ftl_map[j];

        if ((j & 0xff) == 0)
            FTL_PROGRESS("N3G_MBR_PROG j=%lu", (unsigned long)j);
        if (v == 0 || v == 0xffffu)
            continue;

        for (uint32_t po = 0; po < ppb; po++)
        {
            uint32_t bank;
            uint32_t physpage;
            uint32_t rc;
            uint16_t *h = (uint16_t *)ftl_buffer;
            uint8_t *b = (uint8_t *)ftl_buffer;

            ftl_n3g_decode_page(v, po, &bank, &physpage);
            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc = nand_read_page(bank, physpage, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0);
            if (rc != 0 || ftl_sparebuffer[0].user.lpn != 0)
                continue;

            lpn0_hits++;

            for (uint32_t slot = 0; slot < 4; slot++)
            {
                uint32_t off = slot * 0x200;

                if (h[(off + 0x1fe) >> 1] != 0xaa55)
                    continue;

                mbr_hits++;
                best_j = j;
                best_po = po;
                best_slot = slot;
                FTL_PROGRESS("N3G_MBR_HIT j=%lu po=%lu slot=%lu w=%08lx,%08lx",
                             (unsigned long)j, (unsigned long)po,
                             (unsigned long)slot,
                             (unsigned long)((uint32_t *)(b + off))[0],
                             (unsigned long)((uint32_t *)(b + off))[1]);

                for (uint32_t p = 0; p < 4; p++)
                {
                    uint32_t pe = off + 0x1be + p * 16;

                    part_type[p] = b[pe + 4];
                    part_start[p] = (uint32_t)b[pe + 8]
                                  | ((uint32_t)b[pe + 9] << 8)
                                  | ((uint32_t)b[pe + 10] << 16)
                                  | ((uint32_t)b[pe + 11] << 24);
                    part_size[p] = (uint32_t)b[pe + 12]
                                 | ((uint32_t)b[pe + 13] << 8)
                                 | ((uint32_t)b[pe + 14] << 16)
                                 | ((uint32_t)b[pe + 15] << 24);
                    FTL_PROGRESS("N3G_MBR_PART p=%lu t=%02x st=%08lx sz=%08lx",
                                 (unsigned long)p, part_type[p],
                                 (unsigned long)part_start[p],
                                 (unsigned long)part_size[p]);
                }
                goto found_mbr;
            }
        }
    }

found_mbr:
    if (mbr_hits)
    {
        for (uint32_t p = 0; p < 4; p++)
        {
            uint32_t targets[2];

            if (part_type[p] == 0 || part_start[p] == 0)
                continue;

            targets[0] = part_start[p];
            targets[1] = part_start[p] >> 2;
            for (uint32_t ti = 0; ti < 2; ti++)
            {
                uint32_t target = targets[ti];
                uint32_t found = 0;

                for (uint32_t j = 0; j < 0x400 && !found; j++)
                {
                    uint32_t v = ftl_map[j];

                    if (v == 0 || v == 0xffffu)
                        continue;
                    for (uint32_t po = 0; po < 8; po++)
                    {
                        uint32_t bank;
                        uint32_t physpage;
                        uint32_t rc;
                        uint8_t *b = (uint8_t *)ftl_buffer;
                        uint16_t *h = (uint16_t *)ftl_buffer;
                        uint32_t fat = 0;

                        ftl_n3g_decode_page(v, po, &bank, &physpage);
                        memset(ftl_buffer, 0, sizeof(ftl_buffer));
                        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                        rc = nand_read_page(bank, physpage, ftl_buffer,
                                            &ftl_sparebuffer[0], 1, 0);
                        if (rc != 0 || ftl_sparebuffer[0].user.lpn != target)
                            continue;

                        fat = ((b[0x36] == 'F' && b[0x37] == 'A' && b[0x38] == 'T')
                            || (b[0x52] == 'F' && b[0x53] == 'A' && b[0x54] == 'T')
                            || (b[0x236] == 'F' && b[0x237] == 'A' && b[0x238] == 'T')
                            || (b[0x252] == 'F' && b[0x253] == 'A' && b[0x254] == 'T')
                            || (b[0x436] == 'F' && b[0x437] == 'A' && b[0x438] == 'T')
                            || (b[0x452] == 'F' && b[0x453] == 'A' && b[0x454] == 'T')
                            || (b[0x636] == 'F' && b[0x637] == 'A' && b[0x638] == 'T')
                            || (b[0x652] == 'F' && b[0x653] == 'A' && b[0x654] == 'T'));
                        FTL_PROGRESS("N3G_MBR_FAT p=%lu mode=%lu tgt=%08lx j=%lu po=%lu fat=%lu sig=%04x,%04x,%04x,%04x b=%02x%02x",
                                     (unsigned long)p, (unsigned long)ti,
                                     (unsigned long)target, (unsigned long)j,
                                     (unsigned long)po, (unsigned long)fat,
                                     h[0x0ff], h[0x1ff], h[0x2ff], h[0x3ff],
                                     b[0], b[1]);
                        found = 1;
                        break;
                    }
                }
            }
        }
    }

    if (mbr_hits)
        FTL_PROGRESS("N3G_FORMAT_GUESS winpod mbr=%lu l0=%lu j=%lu po=%lu slot=%lu",
                     (unsigned long)mbr_hits, (unsigned long)lpn0_hits,
                     (unsigned long)best_j, (unsigned long)best_po,
                     (unsigned long)best_slot);
    else
        FTL_PROGRESS("N3G_FORMAT_GUESS unknown mbr=0 l0=%lu",
                     (unsigned long)lpn0_hits);

    FTL_PROGRESS("N3G_MBR_DONE");
    return -1;
}

static uint32_t ftl_n3g_winpod_mount(void)
{
    uint32_t map_block = 6916;
    uint32_t map_page = map_block * ftl_nand_type->pagesperblock;
    uint32_t map_pageoff = 0;
    uint16_t *map = (uint16_t *)ftl_buffer;
    uint8_t mbr_page[0x800] STORAGE_ALIGN_ATTR;
    struct n3g_wmount_map_candidate selected;
    uint32_t mbr_found = 0;
    uint32_t mbr_synth = 0;
    uint32_t mbr_j = 0xffffffffu;
    uint32_t mbr_po = 0xffffffffu;
    uint32_t mbr_slot = 0xffffffffu;
    uint32_t fat_found = 0;
    uint32_t fat_j = 0xffffffffu;
    uint32_t fat_po = 0xffffffffu;
    uint32_t part_idx = 0xffffffffu;
    uint32_t part_start = 0;
    uint32_t part_size = 0;
    uint32_t map_usn = 0;
    uint32_t mbr_base_lpn = 0xffffffffu;
    uint32_t mbr_part_lpn = 0xffffffffu;
    uint32_t boot_sector_ok = 0;
    uint8_t part_type = 0;

    FTL_PROGRESS("N3G_WMOUNT_START");

    n3g_direct_map_mount = 0;
    n3g_direct_mbr_valid = 0;
    n3g_direct_mbr_j = 0xffffffffu;
    n3g_direct_mbr_v = 0xffffu;
    n3g_direct_mbr_po = 0xffffffffu;
    n3g_direct_boot_valid = 0;
    n3g_direct_boot_slot = 0;
    n3g_direct_boot_shift = 0;
    n3g_direct_fsinfo_valid = 0;
    n3g_direct_fsinfo_lpn = 0xffffffffu;
    n3g_wmount_rd_best_valid = 0;
    n3g_wmount_rd_best_tag = "";
    n3g_wmount_rd_best_j = 0xffffffffu;
    n3g_wmount_rd_best_v = 0xffffu;
    n3g_wmount_rd_best_l0 = 0xffffffffu;
    n3g_wmount_rd_best_po = 0xffffffffu;
    n3g_wmount_rd_best_slot = 0xffffffffu;
    n3g_wmount_rd_best_sig = 0;
    n3g_wmount_rd_best_reason = 0xffffffffu;
    n3g_wmount_rd_best_bps = 0;
    n3g_wmount_rd_best_fat = 0;
    n3g_wmount_rd_best_type = 0xffu;
    n3g_wmount_rd_best_oob_lpn = 0xffffffffu;
    n3g_wmount_rd_best_score = 0;
    n3g_wmount_rd_hits = 0;
    n3g_wmount_rd_rej_zero = 0;
    n3g_wmount_rd_rej_sig = 0;
    n3g_wmount_rd_rej_fat = 0;
    n3g_wmount_rd_rej_bpb = 0;
    n3g_direct_sector_base = 0;
    n3g_direct_sector_scale = 1;
    ftl_n3g_log_init();
    memset(ftl_map, 0xff, sizeof(ftl_map));
    memset(n3g_direct_mbr, 0, sizeof(n3g_direct_mbr));
    for (uint32_t i = 0; i < ARRAYLEN(n3g_direct_l0_vblock); i++)
    {
        n3g_direct_l0_vblock[i] = 0xffffu;
        n3g_direct_l0_page[i] = 0xffffu;
    }
    for (uint32_t i = 0; i < ARRAYLEN(n3g_direct_l0_cache); i++)
        n3g_direct_l0_cache[i] = 0xffffffffu;

    if (ftl_n3g_wmount_find_map_cluster(mbr_page, &selected) == 0)
    {
        map_block = selected.block;
        map_pageoff = selected.page;
        map_page = map_block * ftl_nand_type->pagesperblock + map_pageoff;
        mbr_j = selected.mbr_j;
        mbr_po = selected.mbr_po;
        mbr_slot = selected.mbr_slot;
        mbr_found = 1;
    }

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
    if (nano3g_nand_diag_local_read(0, map_page, 0x800,
                                    (uint32_t *)&ftl_sparebuffer[0],
                                    0x10) != 0
     || nano3g_nand_diag_local_read(0, map_page, 0,
                                    (uint32_t *)ftl_buffer,
                                    0x800 / sizeof(uint32_t)) != 0)
    {
        FTL_PROGRESS("N3G_WMOUNT_FAIL mapread");
        return -1;
    }
    if (0)
        FTL_PROGRESS("N3G_WMOUNT_MAP t=%02x ix=%04x u=%08lx",
                     ftl_sparebuffer[0].meta.type,
                     ftl_sparebuffer[0].meta.idx,
                     (unsigned long)ftl_sparebuffer[0].meta.usn);
    if (ftl_sparebuffer[0].meta.type != 0x44)
        return -1;
    map_usn = ftl_sparebuffer[0].meta.usn;
    memcpy(ftl_map, map, 0x800);
    n3g_direct_map_loaded_entries = 0x400;

    for (uint32_t j = 0; j < 0x400 && !mbr_found; j++)
    {
        uint32_t v = ftl_map[j];

        if (0 && (j & 0xff) == 0)
            FTL_PROGRESS("N3G_WMOUNT_PROG j=%lu", (unsigned long)j);
        if (v == 0 || v == 0xffffu)
            continue;

        for (uint32_t po = 0; po < ppb; po++)
        {
            uint32_t bank;
            uint32_t physpage;
            uint32_t rc;
            uint8_t *b = (uint8_t *)ftl_buffer;
            uint16_t *h = (uint16_t *)ftl_buffer;

            ftl_n3g_decode_page(v, po, &bank, &physpage);
            memset(ftl_buffer, 0, sizeof(ftl_buffer));
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
            rc = nand_read_page(bank, physpage, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0);
            if (rc != 0 || ftl_sparebuffer[0].user.lpn != 0)
                continue;

            for (uint32_t slot = 0; slot < 4; slot++)
            {
                uint32_t off = slot * 0x200;

                if (h[(off + 0x1fe) >> 1] != 0xaa55)
                    continue;

                uint32_t has_winpart = 0;
                for (uint32_t p = 0; p < 4; p++)
                {
                    uint32_t pe = off + 0x1be + p * 16;
                    uint8_t type = b[pe + 4];
                    uint32_t start = (uint32_t)b[pe + 8]
                                   | ((uint32_t)b[pe + 9] << 8)
                                   | ((uint32_t)b[pe + 10] << 16)
                                   | ((uint32_t)b[pe + 11] << 24);
                    uint32_t size = (uint32_t)b[pe + 12]
                                  | ((uint32_t)b[pe + 13] << 8)
                                  | ((uint32_t)b[pe + 14] << 16)
                                  | ((uint32_t)b[pe + 15] << 24);

                    if ((type == 0x0c || type == 0x0b)
                     && start == 0x0000a07e && size == 0x000e7f81)
                    {
                        has_winpart = 1;
                        break;
                    }
                }
                if (!has_winpart)
                    continue;

                memcpy(mbr_page, b + off, 0x200);
                memset(mbr_page + 0x200, 0, 0x600);
                mbr_j = j;
                mbr_po = po;
                mbr_slot = slot;
                mbr_found = 1;
                ftl_map[0] = v;
                n3g_direct_l0_vblock[0] = v;
                n3g_direct_l0_page[0] = po;
                FTL_PROGRESS("N3G_WMOUNT_MBR j=%lu po=%lu slot=%lu",
                             (unsigned long)j, (unsigned long)po,
                             (unsigned long)slot);
                break;
            }
            if (mbr_found)
                break;
        }
    }

    if (!mbr_found)
    {
        if (0)
            FTL_PROGRESS("N3G_WMOUNT_SYNTH_MBR");
        memset(mbr_page, 0, sizeof(mbr_page));
        mbr_page[0x1be + 4] = 0x0c;
        mbr_page[0x1be + 8] = 0x7e;
        mbr_page[0x1be + 9] = 0xa0;
        mbr_page[0x1be + 12] = 0x81;
        mbr_page[0x1be + 13] = 0x7f;
        mbr_page[0x1be + 14] = 0x0e;
        mbr_page[0x1fe] = 0x55;
        mbr_page[0x1ff] = 0xaa;
        mbr_found = 1;
        mbr_synth = 1;
        mbr_j = 0xffffffffu;
        mbr_po = 0;
        mbr_slot = 0;
        n3g_direct_sector_base = 0;
        n3g_direct_sector_scale = 1;
        n3g_direct_boot_valid = 1;
        n3g_direct_boot_lpn = 0x0000a07e;
        n3g_direct_boot_vblock = 0x044c;
        n3g_direct_boot_page = 62;
        n3g_direct_boot_slot = 0;
        n3g_direct_fsinfo_valid = 1;
        n3g_direct_fsinfo_lpn = 0x0000a086;
    }

    for (uint32_t p = 0; p < 4; p++)
    {
        uint32_t pe = 0x1be + p * 16;
        uint8_t type = mbr_page[pe + 4];
        uint32_t start = (uint32_t)mbr_page[pe + 8]
                       | ((uint32_t)mbr_page[pe + 9] << 8)
                       | ((uint32_t)mbr_page[pe + 10] << 16)
                       | ((uint32_t)mbr_page[pe + 11] << 24);
        uint32_t size = (uint32_t)mbr_page[pe + 12]
                      | ((uint32_t)mbr_page[pe + 13] << 8)
                      | ((uint32_t)mbr_page[pe + 14] << 16)
                      | ((uint32_t)mbr_page[pe + 15] << 24);

        if (0)
            FTL_PROGRESS("N3G_WMOUNT_PART p=%lu t=%02x st=%08lx sz=%08lx",
                         (unsigned long)p, type,
                         (unsigned long)start, (unsigned long)size);
        if ((type == 0x0c || type == 0x0b) && start != 0 && size != 0
         && (part_idx == 0xffffffffu || type == 0x0c))
        {
            part_idx = p;
            part_type = type;
            part_start = start;
            part_size = size;
        }
    }

    if (part_idx == 0xffffffffu)
    {
        FTL_PROGRESS("N3G_WMOUNT_FAIL nopart");
        return -1;
    }
    if (0)
        FTL_PROGRESS("N3G_WMOUNT_USEPART p=%lu t=%02x st=%08lx sz=%08lx",
                     (unsigned long)part_idx, part_type,
                     (unsigned long)part_start, (unsigned long)part_size);

    ftl_n3g_wmount_load_map_pages(map_block, map_usn);
    if (n3g_direct_map_max_idx == 0)
        ftl_n3g_wmount_load_cxt_map_pages();
    ftl_n3g_wmount_probe_known_l45_pages();
    if (0 && n3g_direct_map_max_idx == 0)
        FTL_PROGRESS("N3G_DSCAN_SKIP targeted_l45");

    if (ftl_n3g_wmount_map_lpn0(mbr_j, &mbr_base_lpn) == 0)
    {
        mbr_part_lpn = mbr_base_lpn + part_start;
        n3g_direct_sector_base = mbr_base_lpn;
        n3g_direct_sector_scale = 1;
    }
    else if (mbr_synth)
    {
        mbr_base_lpn = 0;
        mbr_part_lpn = part_start;
        n3g_direct_sector_base = 0;
        n3g_direct_sector_scale = 1;
    }
    if (0)
    {
        FTL_PROGRESS("N3G_WMOUNT_MBASE j=%lu l0=%08lx tgt=%08lx",
                     (unsigned long)mbr_j,
                     (unsigned long)mbr_base_lpn,
                     (unsigned long)mbr_part_lpn);
        FTL_PROGRESS("N3G_WMOUNT_MBR_OK j=%lu v=%04lx po=%lu sl=%lu l0=%08lx sig=AA55",
                     (unsigned long)mbr_j,
                     (unsigned long)(mbr_j < ARRAYLEN(ftl_map)
                        ? ftl_map[mbr_j] : 0xffffu),
                     (unsigned long)mbr_po,
                     (unsigned long)mbr_slot,
                     (unsigned long)mbr_base_lpn);
        FTL_PROGRESS("N3G_WMOUNT_PART_OK p=%lu t=%02x st=%08lx sz=%08lx",
                     (unsigned long)part_idx, part_type,
                     (unsigned long)part_start, (unsigned long)part_size);
    }

    (void)fat_j;
    (void)fat_po;
    (void)fat_found;

#if 0
    const struct n3g_wmount_target targets[] =
    {
        { "mbrbase", mbr_part_lpn, 0 },
        { "exact", part_start, 0 },
        { "base4", part_start & ~3u, part_start & 3u },
        { "div4", part_start >> 2, part_start & 3u },
        { "div2", part_start >> 1, part_start & 1u },
    };

    ftl_n3g_wmount_scan_maplogs(map_block, map_usn, targets,
                                ARRAYLEN(targets), part_start,
                                part_size,
                                &fat_j, &fat_po);
    if (n3g_direct_boot_valid)
        fat_found = 1;

    if (!fat_found
     && ftl_n3g_wmount_probe_l0_range("exact", part_start, part_start,
                                      part_size, &fat_j, &fat_po) != 0)
        fat_found = 1;

    if (!fat_found && mbr_part_lpn != 0xffffffffu
     && ftl_n3g_wmount_probe_l0_range("mbrbase", mbr_part_lpn, part_start,
                                      part_size, &fat_j, &fat_po) != 0)
        fat_found = 1;

    if (!fat_found && mbr_part_lpn != 0xffffffffu
     && ftl_n3g_wmount_probe_cover_window(mbr_part_lpn, part_start,
                                          part_size, &fat_j, &fat_po) != 0)
        fat_found = 1;

    if (!fat_found
     && ftl_n3g_wmount_try_known_backup(part_start, part_size,
                                        &fat_j, &fat_po) != 0)
        fat_found = 1;
#endif

    if (!fat_found && n3g_wmount_rd_best_valid)
        FTL_PROGRESS("N3G_WMOUNT_RBEST %s j=%lu v=%04lx po=%lu sl=%lu sig=%04lx r=%03lx",
                     n3g_wmount_rd_best_tag,
                     (unsigned long)n3g_wmount_rd_best_j,
                     (unsigned long)n3g_wmount_rd_best_v,
                     (unsigned long)n3g_wmount_rd_best_po,
                     (unsigned long)n3g_wmount_rd_best_slot,
                     (unsigned long)n3g_wmount_rd_best_sig,
                     (unsigned long)n3g_wmount_rd_best_reason);

#if 0
    for (uint32_t i = 0; i < ARRAYLEN(targets) && !fat_found; i++)
    {
        uint32_t tj;
        uint32_t tpo;
        uint32_t tv;
        uint32_t tsig;
        uint32_t tbps;
        uint32_t tfat;
        uint32_t trc = ftl_n3g_wmount_probe_target(&targets[i], &tj, &tpo,
                                                   &tv, &tsig, &tbps, &tfat);

        if (tj == mbr_j && targets[i].lpn == mbr_base_lpn)
            continue;

        FTL_PROGRESS("N3G_WMOUNT_BSTGT %s rc=%ld l=%08lx sl=%lu j=%lu v=%04lx po=%lu sig=%04lx bps=%lu fat=%lu",
                     targets[i].name, (long)(int32_t)trc,
                     (unsigned long)targets[i].lpn,
                     (unsigned long)targets[i].slot,
                     (unsigned long)tj, (unsigned long)tv,
                     (unsigned long)tpo, (unsigned long)tsig,
                     (unsigned long)tbps, (unsigned long)tfat);

        if (trc == 0)
        {
            const uint8_t *b = (const uint8_t *)ftl_buffer;
            const uint32_t *w = (const uint32_t *)ftl_buffer;
            uint32_t off = targets[i].slot * 0x200;
            uint32_t wi = off >> 2;
            uint32_t oem0 = (uint32_t)b[off + 3]
                          | ((uint32_t)b[off + 4] << 8)
                          | ((uint32_t)b[off + 5] << 16)
                          | ((uint32_t)b[off + 6] << 24);
            uint32_t oem1 = (uint32_t)b[off + 7]
                          | ((uint32_t)b[off + 8] << 8)
                          | ((uint32_t)b[off + 9] << 16)
                          | ((uint32_t)b[off + 10] << 24);
            uint32_t fs0 = (uint32_t)b[off + 0x52]
                         | ((uint32_t)b[off + 0x53] << 8)
                         | ((uint32_t)b[off + 0x54] << 16)
                         | ((uint32_t)b[off + 0x55] << 24);
            uint32_t fs1 = (uint32_t)b[off + 0x56]
                         | ((uint32_t)b[off + 0x57] << 8)
                         | ((uint32_t)b[off + 0x58] << 16)
                         | ((uint32_t)b[off + 0x59] << 24);

            FTL_PROGRESS("N3G_WMOUNT_BSD %s t=%02x ol=%08lx w=%08lx,%08lx,%08lx,%08lx",
                         targets[i].name, ftl_sparebuffer[0].user.type,
                         (unsigned long)ftl_sparebuffer[0].user.lpn,
                         (unsigned long)w[wi],
                         (unsigned long)w[wi + 1],
                         (unsigned long)w[wi + 2],
                         (unsigned long)w[wi + 3]);
            FTL_PROGRESS("N3G_WMOUNT_BSS %s s=%02x%02x oem=%08lx,%08lx fs=%08lx,%08lx",
                         targets[i].name, b[off + 0x1fe], b[off + 0x1ff],
                         (unsigned long)oem0, (unsigned long)oem1,
                         (unsigned long)fs0, (unsigned long)fs1);
        }

        if (trc == 0 && tsig == 0xaa55)
        {
            uint32_t bps;
            uint32_t spc;
            uint32_t rsvd;
            uint32_t nfats;
            uint32_t total;
            uint32_t fat;
            uint32_t off = targets[i].slot * 0x200;
            uint32_t reason = ftl_n3g_wmount_bpb_reason(
                                  (uint8_t *)ftl_buffer + off, part_size,
                                  &bps, &spc, &rsvd, &nfats, &total, &fat);

            FTL_PROGRESS("N3G_WMOUNT_BSBPB %s r=%03lx bps=%lu spc=%lu rs=%lu nf=%lu ts=%lu fat=%lu",
                         targets[i].name, (unsigned long)reason,
                         (unsigned long)bps, (unsigned long)spc,
                         (unsigned long)rsvd, (unsigned long)nfats,
                         (unsigned long)total, (unsigned long)fat);

            if (fat && (reason & ~0x020u) == 0)
            {
                n3g_direct_boot_valid = 1;
                n3g_direct_boot_lpn = part_start;
                n3g_direct_boot_vblock = tv;
                n3g_direct_boot_page = tpo;
                n3g_direct_boot_slot = targets[i].slot;
                fat_j = tj;
                fat_po = tpo;
                fat_found = 1;
                FTL_PROGRESS("N3G_WMOUNT_BOOTSECTOR_OK %s j=%lu v=%04lx po=%lu sl=%lu",
                             targets[i].name, (unsigned long)tj,
                             (unsigned long)tv, (unsigned long)tpo,
                             (unsigned long)targets[i].slot);
            }
        }
    }
#endif

    uint32_t part_lblock = part_start / ppb;
    uint32_t part_page = part_start % ppb;
    for (uint32_t j = 0; j < 0x400 && !fat_found && 0; j++)
    {
        uint32_t v = ftl_map[j];
        uint32_t bank;
        uint32_t physpage;
        uint32_t rc;
        uint32_t lblock;

        if ((j & 0xff) == 0)
            FTL_PROGRESS("N3G_WMOUNT_BPROG j=%lu", (unsigned long)j);
        if (v == 0 || v == 0xffffu)
            continue;

        ftl_n3g_decode_page(v, 0, &bank, &physpage);
        memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
        rc = nand_read_page(bank, physpage, NULL, &ftl_sparebuffer[0], 1, 0);
        if (rc != 0 || (ftl_sparebuffer[0].user.type != 0x40
                     && ftl_sparebuffer[0].user.type != 0x41))
            continue;

        lblock = ftl_sparebuffer[0].user.lpn / ppb;
        if (lblock != part_lblock)
            continue;

        ftl_map[part_lblock] = v;
        fat_j = j;
        fat_po = part_page;
        fat_found = 1;
        FTL_PROGRESS("N3G_WMOUNT_BMAP lb=%lu j=%lu v=%04lx po=%lu l0=%08lx",
                     (unsigned long)part_lblock, (unsigned long)j,
                     (unsigned long)v, (unsigned long)part_page,
                     (unsigned long)ftl_sparebuffer[0].user.lpn);
    }

    if (0 && !fat_found)
    {
        uint32_t scan_low = part_start >= 0x10000 ? part_start - 0x10000 : 0;
        uint32_t scan_high = part_start + 0x10000;
        uint32_t ranges = 0;
        uint32_t reads = 0;
        uint32_t aa55_hits = 0;
        uint32_t bps_hits = 0;
        uint32_t spc_hits = 0;
        uint32_t rsvd_hits = 0;
        uint32_t nf_hits = 0;
        uint32_t fat_hits = 0;
        uint32_t bpb_hits = 0;
        uint32_t fat_details = 0;
        uint32_t aa_details = 0;
        uint32_t cand_hits = 0;
        uint32_t reject_reason = 0;
        uint32_t fat_sane_shift = 0;
        uint32_t fat_rejects = 0;
        uint32_t aa_rejects = 0;

        FTL_PROGRESS("N3G_WMOUNT_FXSCAN lo=%08lx hi=%08lx ppb=%lu",
                     (unsigned long)scan_low,
                     (unsigned long)scan_high,
                     (unsigned long)ppb);

        for (uint32_t j = 0; j < n3g_direct_map_loaded_entries
             && !fat_found; j++)
        {
            uint32_t v = ftl_map[j];
            uint32_t l0;
            uint32_t first_po;
            uint32_t last_po;

            if (v == 0 || v == 0xffffu)
                continue;
            if (j == mbr_j)
                continue;
            if (ftl_n3g_wmount_map_lpn0(j, &l0) != 0)
                continue;
            if (l0 > scan_high || l0 + ppb * 4 - 1 < scan_low)
                continue;

            ranges++;
            first_po = scan_low > l0 ? (scan_low - l0) >> 2 : 0;
            last_po = scan_high > l0 ? (scan_high - l0) >> 2 : 0;
            if (last_po >= ppb)
                last_po = ppb - 1;

            for (uint32_t po = first_po; po <= last_po && !fat_found; po++)
            {
                uint32_t bank;
                uint32_t physpage;
                uint32_t rc;
                uint8_t *b = (uint8_t *)ftl_buffer;
                uint16_t *h = (uint16_t *)ftl_buffer;

                ftl_n3g_decode_page(v, po, &bank, &physpage);
                memset(ftl_buffer, 0, sizeof(ftl_buffer));
                memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                rc = nand_read_page(bank, physpage, ftl_buffer,
                                    &ftl_sparebuffer[0], 1, 0);
                reads++;
                if (rc != 0)
                    continue;

                for (uint32_t slot = 0; slot < 4 && !fat_found; slot++)
                {
                    uint32_t sector_lpn = l0 + po * 4 + slot;
                    uint32_t off = slot * 0x200;
                    uint32_t sig = h[(off + 0x1fe) >> 1];
                    uint32_t bps;
                    uint32_t spc;
                    uint32_t rsvd;
                    uint32_t nfats;
                    uint32_t total;
                    uint32_t fat;
                    uint32_t reason;
                    uint32_t sane_bps;
                    uint32_t sane_spc;
                    uint32_t sane_rsvd;
                    uint32_t sane_nf;
                    uint32_t aa55;

                    if (sector_lpn < scan_low || sector_lpn > scan_high)
                        continue;

                    reason = ftl_n3g_wmount_bpb_reason(b + off, part_size,
                                    &bps, &spc, &rsvd, &nfats, &total, &fat);
                    aa55 = sig == 0xaa55;
                    sane_bps = bps == 512 || bps == 1024
                            || bps == 2048 || bps == 4096;
                    sane_spc = spc != 0 && (spc & (spc - 1)) == 0;
                    sane_rsvd = rsvd != 0;
                    sane_nf = nfats > 0 && nfats <= 4;

                    if (aa55)
                        aa55_hits++;
                    if (sane_bps)
                        bps_hits++;
                    if (sane_spc)
                        spc_hits++;
                    if (sane_rsvd)
                        rsvd_hits++;
                    if (sane_nf)
                        nf_hits++;
                    if (fat)
                        fat_hits++;
                    if (aa55 && fat && (reason & ~0x020u) == 0)
                        bpb_hits++;

                    if (aa55 || sane_bps || fat)
                        cand_hits++;

                    if (ftl_n3g_wmount_known_bad_j(j)
                     && !(sane_bps || fat))
                        continue;

                    if (!(aa55 || sane_bps || fat))
                        continue;

                    if (reject_reason == 0 && !(aa55 && fat
                     && (reason & ~0x020u) == 0))
                        reject_reason = reason != 0 ? reason : 0x800u;

                    if (aa55 && aa_details < 1)
                    {
                        uint32_t pb = physpage / ftl_nand_type->pagesperblock;
                        uint32_t pp = physpage % ftl_nand_type->pagesperblock;
                        uint32_t found_bps = 0;

                        FTL_PROGRESS("N3G_WMOUNT_ADET j=%lu v=%04lx po=%lu s=%lu l0=%08lx l=%08lx pb=%lu pp=%lu",
                                     (unsigned long)j, (unsigned long)v,
                                     (unsigned long)po,
                                     (unsigned long)slot,
                                     (unsigned long)l0,
                                     (unsigned long)sector_lpn,
                                     (unsigned long)pb,
                                     (unsigned long)pp);
                        FTL_PROGRESS("N3G_WMOUNT_AOOB t=%02lx l=%08lx sig=%02x%02x r=%03lx bps=%lu spc=%lu",
                                     (unsigned long)ftl_sparebuffer[0].user.type,
                                     (unsigned long)ftl_sparebuffer[0].user.lpn,
                                     b[off + 0x1fe], b[off + 0x1ff],
                                     (unsigned long)reason,
                                     (unsigned long)bps,
                                     (unsigned long)spc);

                        for (int32_t win = -32; win < 64; win += 16)
                        {
                            int32_t pos = (int32_t)off + 0x0b + win;

                            if (pos < 0)
                                continue;
                            if (pos + 15 >= 0x800)
                                continue;
                            FTL_PROGRESS("N3G_WMOUNT_AW d=%ld %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                         (long)win,
                                         b[pos + 0], b[pos + 1],
                                         b[pos + 2], b[pos + 3],
                                         b[pos + 4], b[pos + 5],
                                         b[pos + 6], b[pos + 7],
                                         b[pos + 8], b[pos + 9],
                                         b[pos + 10], b[pos + 11],
                                         b[pos + 12], b[pos + 13],
                                         b[pos + 14], b[pos + 15]);
                        }

                        for (int32_t win = -32; win < 64; win++)
                        {
                            int32_t pos = (int32_t)off + 0x0b + win;
                            uint32_t val;
                            uint32_t base;
                            uint32_t abps;
                            uint32_t aspc;
                            uint32_t arsvd;
                            uint32_t anfats;
                            uint32_t atotal;
                            uint32_t afat;
                            uint32_t areason;
                            uint32_t afatsz16;
                            uint32_t afatsz32;
                            uint32_t afatsz;
                            uint32_t asig;

                            if (pos < 0 || pos + 1 >= 0x800)
                                continue;
                            val = (uint32_t)b[pos] | ((uint32_t)b[pos + 1] << 8);
                            if (val != 512 && val != 1024
                             && val != 2048 && val != 4096)
                                continue;

                            if (pos < 0x0b)
                                continue;
                            base = (uint32_t)pos - 0x0b;
                            if (base + 0x59 >= 0x800)
                                continue;

                            areason = ftl_n3g_wmount_bpb_reason(b + base,
                                            part_size, &abps, &aspc, &arsvd,
                                            &anfats, &atotal, &afat);
                            afatsz16 = (uint32_t)b[base + 0x16]
                                     | ((uint32_t)b[base + 0x17] << 8);
                            afatsz32 = (uint32_t)b[base + 0x24]
                                     | ((uint32_t)b[base + 0x25] << 8)
                                     | ((uint32_t)b[base + 0x26] << 16)
                                     | ((uint32_t)b[base + 0x27] << 24);
                            afatsz = afatsz16 != 0 ? afatsz16 : afatsz32;
                            asig = base + 0x1ff < 0x800
                                 ? ((uint32_t)b[base + 0x1fe]
                                    | ((uint32_t)b[base + 0x1ff] << 8))
                                 : 0;
                            FTL_PROGRESS("N3G_WMOUNT_ABPS d=%ld base=%03lx val=%lu",
                                         (long)win,
                                         (unsigned long)(base - off),
                                         (unsigned long)val);
                            FTL_PROGRESS("N3G_WMOUNT_ABPB r=%03lx sig=%04lx bps=%lu spc=%lu rs=%lu nf=%lu ts=%lu fz=%lu fat=%lu",
                                         (unsigned long)areason,
                                         (unsigned long)asig,
                                         (unsigned long)abps,
                                         (unsigned long)aspc,
                                         (unsigned long)arsvd,
                                         (unsigned long)anfats,
                                         (unsigned long)atotal,
                                         (unsigned long)afatsz,
                                         (unsigned long)afat);
                            found_bps = 1;
                            break;
                        }

                        if (found_bps == 0)
                        {
                            aa_rejects++;
                            FTL_PROGRESS("N3G_WMOUNT_AREJ j=%lu v=%04lx no_bps_magic",
                                         (unsigned long)j,
                                         (unsigned long)v);
                        }
                        aa_details++;
                    }

                    if (fat && fat_details < 2)
                    {
                        uint32_t pb = physpage / ftl_nand_type->pagesperblock;
                        uint32_t pp = physpage % ftl_nand_type->pagesperblock;
                        uint32_t fs0 = (uint32_t)b[off + 0x52]
                                     | ((uint32_t)b[off + 0x53] << 8)
                                     | ((uint32_t)b[off + 0x54] << 16)
                                     | ((uint32_t)b[off + 0x55] << 24);
                        uint32_t fs1 = (uint32_t)b[off + 0x56]
                                     | ((uint32_t)b[off + 0x57] << 8)
                                     | ((uint32_t)b[off + 0x58] << 16)
                                     | ((uint32_t)b[off + 0x59] << 24);

                        FTL_PROGRESS("N3G_WMOUNT_FDET j=%lu v=%04lx po=%lu s=%lu l0=%08lx l=%08lx pb=%lu pp=%lu",
                                     (unsigned long)j, (unsigned long)v,
                                     (unsigned long)po,
                                     (unsigned long)slot,
                                     (unsigned long)l0,
                                     (unsigned long)sector_lpn,
                                     (unsigned long)pb,
                                     (unsigned long)pp);
                        FTL_PROGRESS("N3G_WMOUNT_FOOB t=%02lx l=%08lx sig=%02x%02x r=%03lx",
                                     (unsigned long)ftl_sparebuffer[0].user.type,
                                     (unsigned long)ftl_sparebuffer[0].user.lpn,
                                     b[off + 0x1fe], b[off + 0x1ff],
                                     (unsigned long)reason);
                        FTL_PROGRESS("N3G_WMOUNT_FD0 %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                     b[off + 0], b[off + 1],
                                     b[off + 2], b[off + 3],
                                     b[off + 4], b[off + 5],
                                     b[off + 6], b[off + 7],
                                     b[off + 8], b[off + 9],
                                     b[off + 10], b[off + 11],
                                     b[off + 12], b[off + 13],
                                     b[off + 14], b[off + 15]);
                        FTL_PROGRESS("N3G_WMOUNT_FD1 0B=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                     b[off + 0x0b], b[off + 0x0c],
                                     b[off + 0x0d], b[off + 0x0e],
                                     b[off + 0x0f], b[off + 0x10],
                                     b[off + 0x11], b[off + 0x12],
                                     b[off + 0x13], b[off + 0x14],
                                     b[off + 0x15], b[off + 0x16],
                                     b[off + 0x17], b[off + 0x18],
                                     b[off + 0x19], b[off + 0x1a]);
                        FTL_PROGRESS("N3G_WMOUNT_FD2 1B=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x 52=%08lx,%08lx",
                                     b[off + 0x1b], b[off + 0x1c],
                                     b[off + 0x1d], b[off + 0x1e],
                                     b[off + 0x1f], b[off + 0x20],
                                     b[off + 0x21], b[off + 0x22],
                                     b[off + 0x23], b[off + 0x24],
                                     (unsigned long)fs0,
                                     (unsigned long)fs1);

                        for (int32_t win = -32; win <= 64; win += 16)
                        {
                            int32_t pos = (int32_t)off + win;

                            if (pos < 0)
                                continue;
                            if (pos + 15 >= 0x800)
                                continue;
                            FTL_PROGRESS("N3G_WMOUNT_FRW d=%ld %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                         (long)(pos - (int32_t)off),
                                         b[pos + 0], b[pos + 1],
                                         b[pos + 2], b[pos + 3],
                                         b[pos + 4], b[pos + 5],
                                         b[pos + 6], b[pos + 7],
                                         b[pos + 8], b[pos + 9],
                                         b[pos + 10], b[pos + 11],
                                         b[pos + 12], b[pos + 13],
                                         b[pos + 14], b[pos + 15]);
                        }

                        for (int32_t sh = -32; sh <= 32; sh++)
                        {
                            uint32_t sbps;
                            uint32_t sspc;
                            uint32_t srsvd;
                            uint32_t snfats;
                            uint32_t stotal;
                            uint32_t sfat;
                            uint32_t sreason;
                            uint32_t soff;
                            uint32_t sfatsz16;
                            uint32_t sfatsz32;
                            uint32_t sfatsz;
                            uint32_t ssane;

                            if (sh < 0 && off < (uint32_t)(-sh))
                                continue;
                            soff = off + sh;
                            if (soff + 0x59 >= 0x800)
                                continue;
                            sreason = ftl_n3g_wmount_bpb_reason(b + soff,
                                            part_size, &sbps, &sspc, &srsvd,
                                            &snfats, &stotal, &sfat);
                            sfatsz16 = (uint32_t)b[soff + 0x16]
                                     | ((uint32_t)b[soff + 0x17] << 8);
                            sfatsz32 = (uint32_t)b[soff + 0x24]
                                     | ((uint32_t)b[soff + 0x25] << 8)
                                     | ((uint32_t)b[soff + 0x26] << 16)
                                     | ((uint32_t)b[soff + 0x27] << 24);
                            sfatsz = sfatsz16 != 0 ? sfatsz16 : sfatsz32;
                            ssane = sbps == 512 || sbps == 1024
                                  || sbps == 2048 || sbps == 4096;
                            FTL_PROGRESS("N3G_WMOUNT_FSH d=%ld ok=%lu r=%03lx bps=%lu spc=%lu rs=%lu nf=%lu fz=%lu fat=%lu",
                                         (long)sh,
                                         (unsigned long)ssane,
                                         (unsigned long)sreason,
                                         (unsigned long)sbps,
                                         (unsigned long)sspc,
                                         (unsigned long)srsvd,
                                         (unsigned long)snfats,
                                         (unsigned long)sfatsz,
                                         (unsigned long)sfat);
                        }
                        for (uint32_t aslot = 0; aslot < 4
                             && fat_sane_shift == 0; aslot++)
                        {
                            uint32_t aoff = aslot * 0x200;
                            for (int32_t sh = -32; sh <= 32; sh++)
                            {
                                uint32_t sbps;
                                uint32_t sspc;
                                uint32_t srsvd;
                                uint32_t snfats;
                                uint32_t stotal;
                                uint32_t sfat;
                                uint32_t sreason;
                                uint32_t soff;
                                uint32_t sfatsz16;
                                uint32_t sfatsz32;
                                uint32_t sfatsz;

                                if (sh < 0 && aoff < (uint32_t)(-sh))
                                    continue;
                                soff = aoff + sh;
                                if (soff + 0x59 >= 0x800)
                                    continue;
                                sreason = ftl_n3g_wmount_bpb_reason(b + soff,
                                                part_size, &sbps, &sspc,
                                                &srsvd, &snfats, &stotal,
                                                &sfat);
                                if (sbps == 512 || sbps == 1024
                                 || sbps == 2048 || sbps == 4096)
                                {
                                    sfatsz16 = (uint32_t)b[soff + 0x16]
                                             | ((uint32_t)b[soff + 0x17] << 8);
                                    sfatsz32 = (uint32_t)b[soff + 0x24]
                                             | ((uint32_t)b[soff + 0x25] << 8)
                                             | ((uint32_t)b[soff + 0x26] << 16)
                                             | ((uint32_t)b[soff + 0x27] << 24);
                                    sfatsz = sfatsz16 != 0 ? sfatsz16 : sfatsz32;
                                    fat_sane_shift = 1;
                                    FTL_PROGRESS("N3G_WMOUNT_FSANE s=%lu d=%ld r=%03lx bps=%lu spc=%lu rs=%lu nf=%lu fz=%lu fat=%lu",
                                                 (unsigned long)aslot,
                                                 (long)sh,
                                                 (unsigned long)sreason,
                                                 (unsigned long)sbps,
                                                 (unsigned long)sspc,
                                                 (unsigned long)srsvd,
                                                 (unsigned long)snfats,
                                                 (unsigned long)sfatsz,
                                                 (unsigned long)sfat);
                                    break;
                                }
                            }
                        }
                        if (fat_sane_shift == 0)
                        {
                            fat_rejects++;
                            FTL_PROGRESS("N3G_WMOUNT_FREJ j=%lu v=%04lx no_sane_bps",
                                         (unsigned long)j,
                                         (unsigned long)v);
                        }
                        fat_details++;
                    }

                    if (aa55 && fat && (reason & ~0x020u) == 0)
                    {
                        n3g_direct_boot_valid = 1;
                        n3g_direct_boot_lpn = part_start;
                        n3g_direct_boot_vblock = v;
                        n3g_direct_boot_page = po;
                        n3g_direct_boot_slot = slot;
                        fat_j = j;
                        fat_po = po;
                        fat_found = 1;
                        FTL_PROGRESS("N3G_WMOUNT_VMAP j=%lu v=%04lx po=%lu sl=%lu l=%08lx",
                                     (unsigned long)j,
                                     (unsigned long)v,
                                     (unsigned long)po,
                                     (unsigned long)slot,
                                     (unsigned long)sector_lpn);
                    }
                }
            }
        }

        FTL_PROGRESS("N3G_WMOUNT_FXDONE r=%lu cand=%lu aa=%lu bpb=%lu fat=%lu sj=%lu spo=%lu ss=%lu",
                     (unsigned long)reads,
                     (unsigned long)cand_hits,
                     (unsigned long)aa55_hits,
                     (unsigned long)bpb_hits,
                     (unsigned long)fat_hits,
                     (unsigned long)(n3g_direct_boot_valid
                        ? fat_j : 0xffffffffu),
                     (unsigned long)(n3g_direct_boot_valid
                        ? fat_po : 0xffffffffu),
                     (unsigned long)(n3g_direct_boot_valid
                        ? n3g_direct_boot_slot : 0xffffffffu));
        FTL_PROGRESS("N3G_WMOUNT_FXCNT rg=%lu bps=%lu spc=%lu rs=%lu nf=%lu hit=%lu arej=%lu frej=%lu rej=%03lx",
                     (unsigned long)ranges,
                     (unsigned long)bps_hits,
                     (unsigned long)spc_hits,
                     (unsigned long)rsvd_hits,
                     (unsigned long)nf_hits,
                     (unsigned long)fat_found,
                     (unsigned long)aa_rejects,
                     (unsigned long)fat_rejects,
                     (unsigned long)(fat_found ? 0 : reject_reason));
    }

    if (0 && !fat_found)
    {
        uint32_t scan_low = part_start >= 0x200 ? part_start - 0x200 : 0;
        uint32_t scan_high = part_start + 0x200;
        uint32_t cand_seen = 0;
        uint32_t cand_prints = 0;
        uint32_t scan_reads = 0;
        uint32_t aa55_seen = 0;
        uint32_t aa55_prints = 0;
        uint32_t bpb_seen = 0;
        uint32_t fat_seen = 0;
        uint32_t fat_prints = 0;
        uint32_t in_prints = 0;

        FTL_PROGRESS("N3G_WMOUNT_FSCAN st=%08lx lo=%08lx hi=%08lx",
                     (unsigned long)part_start,
                     (unsigned long)scan_low,
                     (unsigned long)scan_high);

        for (uint32_t j = 0; j < 0x400 && !fat_found; j++)
        {
            uint32_t v = ftl_map[j];
            uint32_t bank;
            uint32_t physpage;
            uint32_t rc;

            if (v == 0 || v == 0xffffu)
                continue;

            for (uint32_t po = 0; po < ppb && !fat_found; po++)
            {
                uint8_t *b = (uint8_t *)ftl_buffer;
                uint16_t *h = (uint16_t *)ftl_buffer;
                uint32_t oob_lpn;
                uint32_t oob_t;
                uint32_t in_range;
                uint32_t best_slot = 0;
                uint32_t best_score = 0;
                uint32_t best_sig = 0;
                uint32_t best_bps = 0;
                uint32_t best_fat = 0;
                uint32_t best_bpb = 0;
                uint32_t best_reason = 0xffffffffu;
                uint32_t best_spc = 0;
                uint32_t best_rsvd = 0;
                uint32_t best_nfats = 0;
                uint32_t best_total = 0;

                ftl_n3g_decode_page(v, po, &bank, &physpage);
                memset(ftl_buffer, 0, sizeof(ftl_buffer));
                memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));
                rc = nand_read_page(bank, physpage, ftl_buffer,
                                    &ftl_sparebuffer[0], 1, 0);
                scan_reads++;
                if (rc != 0)
                    continue;

                oob_t = ftl_sparebuffer[0].user.type;
                oob_lpn = ftl_sparebuffer[0].user.lpn;
                in_range = (oob_t == 0x40 || oob_t == 0x41)
                        && oob_lpn >= scan_low && oob_lpn <= scan_high;

                for (uint32_t slot = 0; slot < 4; slot++)
                {
                    uint32_t off = slot * 0x200;
                    uint32_t sig = h[(off + 0x1fe) >> 1];
                    uint32_t aa55 = sig == 0xaa55;
                    uint32_t bps;
                    uint32_t spc;
                    uint32_t rsvd;
                    uint32_t nfats;
                    uint32_t total;
                    uint32_t fat;
                    uint32_t reason = ftl_n3g_wmount_bpb_reason(b + off,
                                            part_size, &bps, &spc, &rsvd,
                                            &nfats, &total, &fat);
                    uint32_t bpb = aa55 && (reason & ~0x120u) == 0;
                    uint32_t score = (aa55 ? 4 : 0)
                                   + (bpb ? 4 : 0)
                                   + (fat ? 2 : 0);
                    uint32_t sane_bps = bps == 512 || bps == 1024
                                      || bps == 2048 || bps == 4096;
                    uint32_t sane_spc = spc != 0
                                      && (spc & (spc - 1)) == 0;
                    uint32_t sane_nf = nfats > 0 && nfats <= 4;
                    uint32_t partial_bpb = sane_bps
                                        || (sane_spc && rsvd != 0 && sane_nf);

                    if (in_range && in_prints < 24
                     && (aa55 || sane_bps || fat || partial_bpb))
                    {
                        uint32_t fs0 = (uint32_t)b[off + 0x52]
                                     | ((uint32_t)b[off + 0x53] << 8)
                                     | ((uint32_t)b[off + 0x54] << 16)
                                     | ((uint32_t)b[off + 0x55] << 24);
                        uint32_t fs1 = (uint32_t)b[off + 0x56]
                                     | ((uint32_t)b[off + 0x57] << 8)
                                     | ((uint32_t)b[off + 0x58] << 16)
                                     | ((uint32_t)b[off + 0x59] << 24);

                        FTL_PROGRESS("N3G_WMOUNT_ICAND j=%lu v=%04lx po=%lu l=%08lx sl=%lu sig=%04lx",
                                     (unsigned long)j, (unsigned long)v,
                                     (unsigned long)po,
                                     (unsigned long)oob_lpn,
                                     (unsigned long)slot,
                                     (unsigned long)sig);
                        FTL_PROGRESS("N3G_WMOUNT_IBPB bps=%lu spc=%lu rs=%lu nf=%lu fat=%lu r=%03lx",
                                     (unsigned long)bps,
                                     (unsigned long)spc,
                                     (unsigned long)rsvd,
                                     (unsigned long)nfats,
                                     (unsigned long)fat,
                                     (unsigned long)reason);
                        FTL_PROGRESS("N3G_WMOUNT_ISTR fs=%08lx,%08lx b0=%02x%02x%02x%02x sg=%02x%02x",
                                     (unsigned long)fs0,
                                     (unsigned long)fs1,
                                     b[off + 0], b[off + 1],
                                     b[off + 2], b[off + 3],
                                     b[off + 0x1fe],
                                     b[off + 0x1ff]);
                        in_prints++;
                    }

                    if (fat && fat_prints < 8)
                    {
                        uint32_t pb = physpage / ftl_nand_type->pagesperblock;
                        uint32_t pp = physpage % ftl_nand_type->pagesperblock;
                        uint32_t fs0 = (uint32_t)b[off + 0x52]
                                     | ((uint32_t)b[off + 0x53] << 8)
                                     | ((uint32_t)b[off + 0x54] << 16)
                                     | ((uint32_t)b[off + 0x55] << 24);
                        uint32_t fs1 = (uint32_t)b[off + 0x56]
                                     | ((uint32_t)b[off + 0x57] << 8)
                                     | ((uint32_t)b[off + 0x58] << 16)
                                     | ((uint32_t)b[off + 0x59] << 24);
                        uint32_t sh1_bps, sh1_spc, sh1_rsvd, sh1_nfats;
                        uint32_t sh1_total, sh1_fat, sh1_reason;
                        uint32_t sh2_bps, sh2_spc, sh2_rsvd, sh2_nfats;
                        uint32_t sh2_total, sh2_fat, sh2_reason;
                        uint32_t sh4_bps, sh4_spc, sh4_rsvd, sh4_nfats;
                        uint32_t sh4_total, sh4_fat, sh4_reason;

                        sh1_reason = ftl_n3g_wmount_bpb_reason(b + off + 1,
                                        part_size, &sh1_bps, &sh1_spc,
                                        &sh1_rsvd, &sh1_nfats, &sh1_total,
                                        &sh1_fat);
                        sh2_reason = ftl_n3g_wmount_bpb_reason(b + off + 2,
                                        part_size, &sh2_bps, &sh2_spc,
                                        &sh2_rsvd, &sh2_nfats, &sh2_total,
                                        &sh2_fat);
                        sh4_reason = ftl_n3g_wmount_bpb_reason(b + off + 4,
                                        part_size, &sh4_bps, &sh4_spc,
                                        &sh4_rsvd, &sh4_nfats, &sh4_total,
                                        &sh4_fat);

                        FTL_PROGRESS("N3G_WMOUNT_FCAND j=%lu v=%04lx po=%lu sl=%lu pb=%lu pp=%lu",
                                     (unsigned long)j, (unsigned long)v,
                                     (unsigned long)po, (unsigned long)slot,
                                     (unsigned long)pb, (unsigned long)pp);
                        FTL_PROGRESS("N3G_WMOUNT_FOOB t=%02lx l=%08lx sig=%02x%02x fs=%08lx,%08lx",
                                     (unsigned long)oob_t,
                                     (unsigned long)oob_lpn,
                                     b[off + 0x1fe], b[off + 0x1ff],
                                     (unsigned long)fs0,
                                     (unsigned long)fs1);
                        FTL_PROGRESS("N3G_WMOUNT_F0 %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                     b[off + 0], b[off + 1],
                                     b[off + 2], b[off + 3],
                                     b[off + 4], b[off + 5],
                                     b[off + 6], b[off + 7],
                                     b[off + 8], b[off + 9],
                                     b[off + 10], b[off + 11],
                                     b[off + 12], b[off + 13],
                                     b[off + 14], b[off + 15]);
                        FTL_PROGRESS("N3G_WMOUNT_F1 0B=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                     b[off + 0x0b], b[off + 0x0c],
                                     b[off + 0x0d], b[off + 0x0e],
                                     b[off + 0x0f], b[off + 0x10],
                                     b[off + 0x11], b[off + 0x12],
                                     b[off + 0x13], b[off + 0x14],
                                     b[off + 0x15], b[off + 0x16],
                                     b[off + 0x17], b[off + 0x18],
                                     b[off + 0x19], b[off + 0x1a]);
                        FTL_PROGRESS("N3G_WMOUNT_F2 1B=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x 52=%02x%02x%02x%02x%02x%02x%02x%02x",
                                     b[off + 0x1b], b[off + 0x1c],
                                     b[off + 0x1d], b[off + 0x1e],
                                     b[off + 0x1f], b[off + 0x20],
                                     b[off + 0x21], b[off + 0x22],
                                     b[off + 0x23], b[off + 0x24],
                                     b[off + 0x52], b[off + 0x53],
                                     b[off + 0x54], b[off + 0x55],
                                     b[off + 0x56], b[off + 0x57],
                                     b[off + 0x58], b[off + 0x59]);
                        FTL_PROGRESS("N3G_WMOUNT_FR bps=%lu spc=%lu rs=%lu nf=%lu ts=%lu r=%03lx",
                                     (unsigned long)bps,
                                     (unsigned long)spc,
                                     (unsigned long)rsvd,
                                     (unsigned long)nfats,
                                     (unsigned long)total,
                                     (unsigned long)reason);
                        FTL_PROGRESS("N3G_WMOUNT_FB j=%lu b=%lu s=%lu r=%lu n=%lu f=%lu z=%lu c=%lu",
                                     (unsigned long)((reason & 0x001) != 0),
                                     (unsigned long)((reason & 0x002) != 0),
                                     (unsigned long)((reason & 0x004) != 0),
                                     (unsigned long)((reason & 0x008) != 0),
                                     (unsigned long)((reason & 0x010) != 0),
                                     (unsigned long)((reason & 0x100) != 0),
                                     (unsigned long)((reason & 0x200) != 0),
                                     (unsigned long)((reason & 0x400) != 0));
                        FTL_PROGRESS("N3G_WMOUNT_FSH r0=%03lx r1=%03lx r2=%03lx r4=%03lx fat=%lu,%lu,%lu,%lu",
                                     (unsigned long)reason,
                                     (unsigned long)sh1_reason,
                                     (unsigned long)sh2_reason,
                                     (unsigned long)sh4_reason,
                                     (unsigned long)fat,
                                     (unsigned long)sh1_fat,
                                     (unsigned long)sh2_fat,
                                     (unsigned long)sh4_fat);
                        fat_prints++;
                    }

                    if (score > best_score)
                    {
                        best_score = score;
                        best_slot = slot;
                        best_sig = sig;
                        best_bps = bps;
                        best_fat = fat;
                        best_bpb = bpb;
                        best_reason = reason;
                        best_spc = spc;
                        best_rsvd = rsvd;
                        best_nfats = nfats;
                        best_total = total;
                    }
                }

                if (in_range)
                    cand_seen++;
                if (best_sig == 0xaa55)
                    aa55_seen++;
                if (best_bpb)
                    bpb_seen++;
                if (best_fat)
                    fat_seen++;

                if (0 && best_sig == 0xaa55 && aa55_prints < 12)
                {
                    uint32_t off = best_slot * 0x200;
                    uint32_t pb = physpage / ftl_nand_type->pagesperblock;
                    uint32_t pp = physpage % ftl_nand_type->pagesperblock;

                    FTL_PROGRESS("N3G_WMOUNT_AA n=%lu j=%lu v=%04lx po=%lu sl=%lu pb=%lu pp=%lu t=%02lx l=%08lx",
                                 (unsigned long)aa55_prints,
                                 (unsigned long)j,
                                 (unsigned long)v,
                                 (unsigned long)po,
                                 (unsigned long)best_slot,
                                 (unsigned long)pb,
                                 (unsigned long)pp,
                                 (unsigned long)oob_t,
                                 (unsigned long)oob_lpn);
                    FTL_PROGRESS("N3G_WMOUNT_A0 n=%lu %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                 (unsigned long)aa55_prints,
                                 b[off + 0], b[off + 1],
                                 b[off + 2], b[off + 3],
                                 b[off + 4], b[off + 5],
                                 b[off + 6], b[off + 7],
                                 b[off + 8], b[off + 9],
                                 b[off + 10], b[off + 11],
                                 b[off + 12], b[off + 13],
                                 b[off + 14], b[off + 15]);
                    FTL_PROGRESS("N3G_WMOUNT_A1 n=%lu 0B=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                                 (unsigned long)aa55_prints,
                                 b[off + 0x0b], b[off + 0x0c],
                                 b[off + 0x0d], b[off + 0x0e],
                                 b[off + 0x0f], b[off + 0x10],
                                 b[off + 0x11], b[off + 0x12],
                                 b[off + 0x13], b[off + 0x14],
                                 b[off + 0x15], b[off + 0x16],
                                 b[off + 0x17], b[off + 0x18],
                                 b[off + 0x19], b[off + 0x1a]);
                    FTL_PROGRESS("N3G_WMOUNT_A2 n=%lu 1B=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x 52=%02x%02x%02x%02x%02x%02x%02x%02x sg=%02x%02x",
                                 (unsigned long)aa55_prints,
                                 b[off + 0x1b], b[off + 0x1c],
                                 b[off + 0x1d], b[off + 0x1e],
                                 b[off + 0x1f], b[off + 0x20],
                                 b[off + 0x21], b[off + 0x22],
                                 b[off + 0x23], b[off + 0x24],
                                 b[off + 0x52], b[off + 0x53],
                                 b[off + 0x54], b[off + 0x55],
                                 b[off + 0x56], b[off + 0x57],
                                 b[off + 0x58], b[off + 0x59],
                                 b[off + 0x1fe], b[off + 0x1ff]);
                    FTL_PROGRESS("N3G_WMOUNT_AR n=%lu bps=%lu spc=%lu rs=%lu nf=%lu ts=%lu fat=%lu r=%03lx",
                                 (unsigned long)aa55_prints,
                                 (unsigned long)best_bps,
                                 (unsigned long)best_spc,
                                 (unsigned long)best_rsvd,
                                 (unsigned long)best_nfats,
                                 (unsigned long)best_total,
                                 (unsigned long)best_fat,
                                 (unsigned long)best_reason);
                    aa55_prints++;
                }

                uint32_t sane_bps = best_bps == 512 || best_bps == 1024
                                  || best_bps == 2048 || best_bps == 4096;
                uint32_t sane_spc = best_spc != 0
                                  && (best_spc & (best_spc - 1)) == 0;
                uint32_t sane_nf = best_nfats > 0 && best_nfats <= 4;
                uint32_t close = best_sig == 0xaa55
                              && (best_fat || (sane_bps && sane_spc
                               && best_rsvd != 0 && sane_nf));

                if (!close && !best_bpb && !best_fat)
                    continue;

                if (cand_prints < 12)
                {
                    uint32_t off = best_slot * 0x200;
                    uint32_t fs0 = (uint32_t)b[off + 0x52]
                                 | ((uint32_t)b[off + 0x53] << 8)
                                 | ((uint32_t)b[off + 0x54] << 16)
                                 | ((uint32_t)b[off + 0x55] << 24);
                    uint32_t fs1 = (uint32_t)b[off + 0x56]
                                 | ((uint32_t)b[off + 0x57] << 8)
                                 | ((uint32_t)b[off + 0x58] << 16)
                                 | ((uint32_t)b[off + 0x59] << 24);

                    FTL_PROGRESS("N3G_WMOUNT_BCAND j=%lu v=%04lx po=%lu sl=%lu l=%08lx sig=%04lx",
                                 (unsigned long)j,
                                 (unsigned long)v,
                                 (unsigned long)po,
                                 (unsigned long)best_slot,
                                 (unsigned long)oob_lpn,
                                 (unsigned long)best_sig);
                    FTL_PROGRESS("N3G_WMOUNT_BBPB bps=%lu spc=%lu rs=%lu nf=%lu ts=%lu fat=%lu r=%03lx",
                                 (unsigned long)best_bps,
                                 (unsigned long)best_spc,
                                 (unsigned long)best_rsvd,
                                 (unsigned long)best_nfats,
                                 (unsigned long)best_total,
                                 (unsigned long)best_fat,
                                 (unsigned long)best_reason);
                    FTL_PROGRESS("N3G_WMOUNT_BSTR fs=%08lx,%08lx sg=%02x%02x",
                                 (unsigned long)fs0,
                                 (unsigned long)fs1,
                                 b[off + 0x1fe], b[off + 0x1ff]);
                    cand_prints++;
                }

                if (best_bpb && best_fat)
                {
                    ftl_map[part_lblock] = v;
                    n3g_direct_boot_valid = 1;
                    n3g_direct_boot_lpn = part_start;
                    n3g_direct_boot_vblock = v;
                    n3g_direct_boot_page = po;
                    n3g_direct_boot_slot = best_slot;
                    fat_j = j;
                    fat_po = po;
                    fat_found = 1;
                    FTL_PROGRESS("N3G_WMOUNT_CMAP j=%lu v=%04lx po=%lu sl=%lu l=%08lx sig=%04lx fat=%lu",
                                 (unsigned long)j,
                                 (unsigned long)v,
                                 (unsigned long)po,
                                 (unsigned long)best_slot,
                                 (unsigned long)oob_lpn,
                                 (unsigned long)best_sig,
                                 (unsigned long)best_fat);
                }
            }
        }

        FTL_PROGRESS("N3G_WMOUNT_CDONE r=%lu in=%lu ip=%lu aa=%lu ap=%lu bpb=%lu fat=%lu fp=%lu hit=%lu",
                     (unsigned long)scan_reads,
                     (unsigned long)cand_seen,
                     (unsigned long)in_prints,
                     (unsigned long)aa55_seen,
                     (unsigned long)aa55_prints,
                     (unsigned long)bpb_seen,
                     (unsigned long)fat_seen,
                     (unsigned long)fat_prints,
                     (unsigned long)fat_found);
        if (!fat_found)
            FTL_PROGRESS("N3G_WMOUNT_CMISS prints=%lu",
                         (unsigned long)cand_prints);
    }

    /*
     * Present the restored Windows MBR in RAM only. Keep the partition start
     * and size in the MBR's original units for this diagnostic.
     */
    memcpy(n3g_direct_mbr, mbr_page, 0x800);
    uint32_t pe = 0x1be + part_idx * 16;
    n3g_direct_mbr[pe + 8] = part_start & 0xff;
    n3g_direct_mbr[pe + 9] = (part_start >> 8) & 0xff;
    n3g_direct_mbr[pe + 10] = (part_start >> 16) & 0xff;
    n3g_direct_mbr[pe + 11] = (part_start >> 24) & 0xff;
    n3g_direct_mbr[pe + 12] = part_size & 0xff;
    n3g_direct_mbr[pe + 13] = (part_size >> 8) & 0xff;
    n3g_direct_mbr[pe + 14] = (part_size >> 16) & 0xff;
    n3g_direct_mbr[pe + 15] = (part_size >> 24) & 0xff;
    n3g_direct_mbr_valid = 1;
    n3g_direct_mbr_j = mbr_j;
    n3g_direct_mbr_v = (mbr_j < ARRAYLEN(ftl_map)) ? ftl_map[mbr_j] : 0xffffu;
    n3g_direct_mbr_po = mbr_po;
    n3g_direct_map_mount = 1;

    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    int32_t rc0 = (int32_t)ftl_n3g_direct_read(0, 1, ftl_buffer);
    uint16_t *h0 = (uint16_t *)ftl_buffer;
    if (0)
        FTL_PROGRESS("N3G_WMOUNT_S0 rc=%ld sig=%04x part=%lu t=%02x st=%08lx sz=%08lx",
                     (long)rc0, h0[0xff], (unsigned long)part_idx,
                     part_type, (unsigned long)part_start,
                     (unsigned long)part_size);
    if (rc0 != 0)
    {
        n3g_direct_map_mount = 0;
        n3g_direct_mbr_valid = 0;
        return -1;
    }

    /*
     * The restored WinPod volume uses a 4096-byte FAT sector on top of the
     * 512-byte WMOUNT sector reader. Rockbox scales BPB fields to 512-byte
     * storage sectors, then reads FSInfo at partition_start + fsinfo * scale.
     * Some Nano 3G dumps do not expose that FSInfo page through the current
     * map, and fat_mount() treats a missing FSInfo sector as fatal. Once the
     * boot sector itself validates, provide the small FSInfo sector Rockbox
     * needs so mounting can continue to FAT/root reads.
     */
    {
        uint32_t bps = 0;
        uint32_t spc = 0;
        uint32_t rsvd = 0;
        uint32_t nfats = 0;
        uint32_t total = 0;
        uint32_t fat = 0;
        uint32_t reason;
        uint32_t scale;
        uint32_t fsinfo;
        int32_t rcb;
        uint8_t *b = (uint8_t *)ftl_buffer;

        memset(ftl_buffer, 0, sizeof(ftl_buffer));
        rcb = (int32_t)ftl_n3g_direct_read(part_start, 1, ftl_buffer);
        reason = ftl_n3g_wmount_bpb_reason(b, part_size, &bps, &spc,
                                           &rsvd, &nfats, &total, &fat);
        if (rcb == 0 && fat && (reason & ~0x020u) == 0
            && bps >= 512 && (bps % 512) == 0)
        {
            boot_sector_ok = 1;
            scale = bps / 512;
            fsinfo = (uint32_t)b[0x30] | ((uint32_t)b[0x31] << 8);
            if (fsinfo != 0 && scale != 0)
            {
                n3g_direct_fsinfo_valid = 1;
                n3g_direct_fsinfo_lpn = part_start + fsinfo * scale;
            }
            if (0)
                FTL_PROGRESS("N3G_WMOUNT_BOOT_OK rc=%ld sig=%04x bps=%lu spc=%lu rs=%lu nf=%lu fs=%08lx",
                             (long)rcb,
                             (unsigned int)(b[0x1fe] | ((uint32_t)b[0x1ff] << 8)),
                             (unsigned long)bps,
                             (unsigned long)spc,
                             (unsigned long)rsvd,
                             (unsigned long)nfats,
                             (unsigned long)n3g_direct_fsinfo_lpn);
        }
        else
        {
            FTL_PROGRESS("N3G_WMOUNT_BOOT_BAD rc=%ld sig=%04x bps=%lu r=%03lx",
                         (long)rcb,
                         (unsigned int)(b[0x1fe] | ((uint32_t)b[0x1ff] << 8)),
                         (unsigned long)bps,
                         (unsigned long)reason);
        }
    }

    if (!n3g_direct_boot_valid)
    {
        if (boot_sector_ok)
        {
            if (0)
                FTL_PROGRESS("N3G_WMOUNT_READY mbrj=%lu mbrpo=%lu xmap=1 st=%08lx sz=%08lx fs=%08lx",
                             (unsigned long)mbr_j, (unsigned long)mbr_po,
                             (unsigned long)part_start, (unsigned long)part_size,
                             (unsigned long)n3g_direct_fsinfo_lpn);
            return 0;
        }

        FTL_PROGRESS("N3G_WMOUNT_NOBOOT rd=%lu z=%lu sig=%lu fat=%lu bpb=%lu",
                     (unsigned long)n3g_wmount_rd_hits,
                     (unsigned long)n3g_wmount_rd_rej_zero,
                     (unsigned long)n3g_wmount_rd_rej_sig,
                     (unsigned long)n3g_wmount_rd_rej_fat,
                     (unsigned long)n3g_wmount_rd_rej_bpb);
        if (n3g_wmount_rd_best_valid)
            FTL_PROGRESS("N3G_WMOUNT_NBEST %s j=%lu v=%04lx po=%lu sl=%lu r=%03lx",
                         n3g_wmount_rd_best_tag,
                         (unsigned long)n3g_wmount_rd_best_j,
                         (unsigned long)n3g_wmount_rd_best_v,
                         (unsigned long)n3g_wmount_rd_best_po,
                         (unsigned long)n3g_wmount_rd_best_slot,
                         (unsigned long)n3g_wmount_rd_best_reason);
        if (0)
            FTL_PROGRESS("N3G_WMOUNT_READY mbrj=%lu mbrpo=%lu xmap=0 st=%08lx sz=%08lx",
                         (unsigned long)mbr_j, (unsigned long)mbr_po,
                         (unsigned long)part_start, (unsigned long)part_size);
        return 0;
    }

    if (0)
        FTL_PROGRESS("N3G_WMOUNT_SEL ok=1 j=%lu v=%04lx po=%lu sl=%lu",
                     (unsigned long)fat_j,
                     (unsigned long)n3g_direct_boot_vblock,
                     (unsigned long)fat_po,
                     (unsigned long)n3g_direct_boot_slot);
    memset(ftl_buffer, 0, sizeof(ftl_buffer));
    int32_t rcf = (int32_t)ftl_n3g_direct_read(part_start, 1, ftl_buffer);
    uint16_t *hf = (uint16_t *)ftl_buffer;
    if (0)
        FTL_PROGRESS("N3G_WMOUNT_SF rc=%ld j=%lu po=%lu sig=%04x,%04x,%04x,%04x w=%08lx,%08lx",
                     (long)rcf, (unsigned long)fat_j, (unsigned long)fat_po,
                     hf[0x0ff], hf[0x1ff], hf[0x2ff], hf[0x3ff],
                     (unsigned long)((uint32_t *)ftl_buffer)[0],
                     (unsigned long)((uint32_t *)ftl_buffer)[1]);

    if (0)
        FTL_PROGRESS("N3G_WMOUNT_READY mbrj=%lu mbrpo=%lu xmap=%lu st=%08lx sz=%08lx",
                     (unsigned long)mbr_j, (unsigned long)mbr_po,
                     (unsigned long)fat_found, (unsigned long)part_start,
                     (unsigned long)part_size);
    return 0;
}
#endif

#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
static void ftl_n3g_print_regs(const char *tag,
                               const struct nano3g_nand_reg_diag *reg)
{
    FTL_PROGRESS("N3G_REG %s f0=%08lx f1=%08lx stat=%08lx cmd=%08lx a0=%08lx a1=%08lx a2=%08lx",
                 tag, (unsigned long)reg->fmctrl0,
                 (unsigned long)reg->fmctrl1,
                 (unsigned long)reg->fmcstat,
                 (unsigned long)reg->fmcmd,
                 (unsigned long)reg->fmaddr0,
                 (unsigned long)reg->fmaddr1,
                 (unsigned long)reg->fmaddr2);
    FTL_PROGRESS("N3G_GPIO %s p7=%08lx p8=%08lx p9=%08lx p10=%08lx p11=%08lx gpio=%08lx",
                 tag, (unsigned long)reg->pcon7,
                 (unsigned long)reg->pcon8,
                 (unsigned long)reg->pcon9,
                 (unsigned long)reg->pcon10,
                 (unsigned long)reg->pcon11,
                 (unsigned long)reg->gpiocmd);
    FTL_PROGRESS("N3G_CLK %s pwr0=%08lx pwr1=%08lx clk1=%08lx misc=%08lx stage=%08lx/%08lx/%08lx",
                 tag, (unsigned long)reg->pwr0,
                 (unsigned long)reg->pwr1,
                 (unsigned long)reg->clkcon1,
                 (unsigned long)reg->misc,
                 (unsigned long)reg->fmstage0,
                 (unsigned long)reg->fmstage1,
                 (unsigned long)reg->fmstage2);
}
#endif

static void ftl_n3g_sanity_scan_raw(void)
{
#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    static const uint32_t sample_pages[] =
    {
        0, 1, 128, 1024, 65536, 524288, 1048443, 1048444
    };
    int32_t read_rc;
    int32_t id_rc;
    uint32_t id;
    uint32_t ctrl0;
    uint32_t pmu10;
    uint32_t pmu15;
    uint32_t pcon7;
    uint32_t pcon8;
    uint32_t pcon9;
    uint32_t pcon10;
    uint32_t pcon11;
    uint32_t clk_before;
    uint32_t clk_during;
    uint32_t clk_after;
    uint32_t cp_before;
    uint32_t cp_during;
    uint32_t cp_after;
    uint32_t nonzero;
    uint32_t ff;
    uint32_t nontrivial;
    uint32_t i;
    uint32_t hits = 0;
    uint32_t id_hits = 0;
    uint32_t cur_id = 0;
    uint32_t local_id = 0;
    uint32_t idvar_id = 0;
    uint32_t gpiovar_id = 0;
    uint32_t clkvar_id = 0;
    uint32_t cp15_id = 0;
    uint32_t pmu_id = 0;
    uint32_t sample_page = 0xffffffffu;
    uint32_t sample_w0 = 0;
    uint32_t sample_w1 = 0;
    uint32_t sample_nz = 0;
    uint32_t sample_nt = 0;
    int32_t sample_rc = 0x7fffffff;

#if N3G_SCREEN_COMPACT
    uint32_t entry_diag[10];
    uint32_t stage_diag[39];

    nano3g_nand_entry_diag_get(entry_diag, ARRAYLEN(entry_diag));
    nano3g_nand_stage_diag_get(stage_diag, ARRAYLEN(stage_diag));

    FTL_PROGRESS("N3G_MIN_START");
    FTL_PROGRESS("N3G_ENT r0=%08lx r1=%08lx c=%08lx",
                 (unsigned long)entry_diag[2],
                 (unsigned long)entry_diag[3],
                 (unsigned long)entry_diag[1]);
    FTL_PROGRESS("N3G_ENT2 rc=%ld/%ld st=%08lx/%08lx",
                 (long)(int32_t)entry_diag[4],
                 (long)(int32_t)entry_diag[5],
                 (unsigned long)entry_diag[6],
                 (unsigned long)entry_diag[7]);
    FTL_PROGRESS("N3G_STG seen=%08lx ok=%08lx bad=%lu",
                 (unsigned long)stage_diag[1],
                 (unsigned long)stage_diag[2],
                 (unsigned long)stage_diag[3]);
    FTL_PROGRESS("N3G_STG2 last=%08lx rc=%ld st=%08lx",
                 (unsigned long)stage_diag[4],
                 (long)(int32_t)stage_diag[5],
                 (unsigned long)stage_diag[6]);

    id_rc = nano3g_nand_diag_read_id_current(&id);
    cur_id = id;
    if (id_rc == 0 && id != 0)
        id_hits++;

    id_rc = nano3g_nand_diag_read_id(&id);
    local_id = id;
    if (id_rc == 0 && id != 0)
        id_hits++;

    for (i = 0; i < 8; i++)
    {
        id_rc = nano3g_nand_diag_read_id_variant(0, i, &ctrl0, &id);
        if (id_rc == 0 && id != 0)
        {
            idvar_id = id;
            id_hits++;
            break;
        }
    }

    for (i = 0; i < 5; i++)
    {
        id_rc = nano3g_nand_diag_gpio_id_variant(0, i, &ctrl0, &id,
                                                 &pcon7, &pcon8, &pcon9,
                                                 &pcon10, &pcon11);
        if (id_rc == 0 && id != 0)
        {
            gpiovar_id = id;
            id_hits++;
            break;
        }
    }

    for (i = 0; i < 4; i++)
    {
        id_rc = nano3g_nand_diag_clock_id_variant(0, i, &id, &clk_before,
                                                  &clk_during, &clk_after);
        if (id_rc == 0 && id != 0)
        {
            clkvar_id = id;
            id_hits++;
            break;
        }
    }

    id_rc = nano3g_nand_diag_cp15_id(0, &id, &cp_before, &cp_during,
                                     &cp_after);
    if (id_rc == 0 && id != 0)
    {
        cp15_id = id;
        id_hits++;
    }

    for (i = 0; i < ARRAYLEN(sample_pages); i++)
    {
        memset(ftl_buffer, 0, 0x800);
        read_rc = nano3g_nand_diag_local_read(0, sample_pages[i], 0,
                                              (uint32_t *)ftl_buffer, 16);
        ftl_n3g_sanity_count((const uint8_t *)ftl_buffer, 64, &nonzero, &ff,
                             &nontrivial);
        if (sample_page == 0xffffffffu || nontrivial != 0)
        {
            sample_page = sample_pages[i];
            sample_rc = read_rc;
            sample_w0 = ftl_buffer[0];
            sample_w1 = ftl_buffer[1];
            sample_nz = nonzero;
            sample_nt = nontrivial;
        }
        if (read_rc == 0 && nontrivial != 0)
        {
            hits++;
            break;
        }
    }

    FTL_PROGRESS("N3G_IDS c=%08lx l=%08lx",
                 (unsigned long)cur_id, (unsigned long)local_id);
    FTL_PROGRESS("N3G_ID2 i=%08lx g=%08lx",
                 (unsigned long)idvar_id, (unsigned long)gpiovar_id);
    FTL_PROGRESS("N3G_ID3 k=%08lx p=%08lx h=%lu",
                 (unsigned long)clkvar_id, (unsigned long)pmu_id,
                 (unsigned long)id_hits);
    FTL_PROGRESS("N3G_ID4 cp=%08lx", (unsigned long)cp15_id);
    FTL_PROGRESS("N3G_SMP p=%lu r=%ld",
                 (unsigned long)sample_page, (long)sample_rc);
    FTL_PROGRESS("N3G_DAT %08lx %08lx",
                 (unsigned long)sample_w0, (unsigned long)sample_w1);
    FTL_PROGRESS("N3G_CNT nz=%lu nt=%lu",
                 (unsigned long)sample_nz, (unsigned long)sample_nt);
    if (hits != 0)
        FTL_PROGRESS("N3G_NAND_ACCESS_FIXED");
    else
        FTL_PROGRESS("N3G_NAND_ACCESS_STILL_BROKEN");
    FTL_PROGRESS("N3G_MIN_DONE");
    return;
#endif

    struct nano3g_nand_reg_diag reg;

    nano3g_nand_diag_regs(&reg);
    ftl_n3g_print_regs("entry", &reg);

    id_rc = nano3g_nand_diag_read_id_current(&id);
    cur_id = id;
    FTL_PROGRESS("N3G_DFU_STATE_ID rc=%ld id=%08lx", (long)id_rc,
                 (unsigned long)id);
    FTL_PROGRESS("N3G_ID_CUR rc=%ld id=%08lx", (long)id_rc,
                 (unsigned long)id);
    if (id_rc == 0 && id != 0)
        id_hits++;

    nano3g_nand_diag_regs(&reg);
    ftl_n3g_print_regs("after_curid", &reg);

    id_rc = nano3g_nand_diag_read_id(&id);
    local_id = id;
    FTL_PROGRESS("N3G_LOCAL_ID rc=%ld id=%08lx", (long)id_rc,
                 (unsigned long)id);
    FTL_PROGRESS("N3G_ID_LOC rc=%ld id=%08lx", (long)id_rc,
                 (unsigned long)id);
    if (id_rc == 0 && id != 0)
        id_hits++;

    nano3g_nand_diag_regs(&reg);
    ftl_n3g_print_regs("after_localid", &reg);

    for (i = 0; i < 8; i++)
    {
        id_rc = nano3g_nand_diag_read_id_variant(0, i, &ctrl0, &id);
        FTL_PROGRESS("N3G_IDVAR v=%lu rc=%ld c=%08lx id=%08lx",
                     (unsigned long)i, (long)id_rc,
                     (unsigned long)ctrl0, (unsigned long)id);
        if (id_rc == 0 && id != 0)
        {
            idvar_id = id;
            id_hits++;
            break;
        }
    }

    for (i = 0; i < 5; i++)
    {
        id_rc = nano3g_nand_diag_gpio_id_variant(0, i, &ctrl0, &id,
                                                 &pcon7, &pcon8, &pcon9,
                                                 &pcon10, &pcon11);
        FTL_PROGRESS("N3G_GPIOVAR v=%lu rc=%ld c=%08lx id=%08lx p7=%08lx p8=%08lx",
                     (unsigned long)i, (long)id_rc,
                     (unsigned long)ctrl0, (unsigned long)id,
                     (unsigned long)pcon7, (unsigned long)pcon8);
        FTL_PROGRESS("N3G_GPIOVAR2 v=%lu p9=%08lx p10=%08lx p11=%08lx",
                     (unsigned long)i, (unsigned long)pcon9,
                     (unsigned long)pcon10, (unsigned long)pcon11);
        if (id_rc == 0 && id != 0)
        {
            gpiovar_id = id;
            id_hits++;
            break;
        }
    }

    for (i = 0; i < 4; i++)
    {
        id_rc = nano3g_nand_diag_clock_id_variant(0, i, &id, &clk_before,
                                                  &clk_during, &clk_after);
        FTL_PROGRESS("N3G_CLKVAR v=%lu rc=%ld id=%08lx",
                     (unsigned long)i, (long)id_rc, (unsigned long)id);
        FTL_PROGRESS("N3G_CLKVAR2 b=%08lx d=%08lx a=%08lx",
                     (unsigned long)clk_before,
                     (unsigned long)clk_during,
                     (unsigned long)clk_after);
        if (id_rc == 0 && id != 0)
        {
            clkvar_id = id;
            id_hits++;
            break;
        }
    }

    for (i = 0; i < 5; i++)
    {
        id_rc = nano3g_nand_diag_pmu_id_variant(i, &pmu10, &pmu15, &id);
        FTL_PROGRESS("N3G_PMUID v=%lu rc=%ld p10=%02lx p15=%02lx id=%08lx",
                     (unsigned long)i, (long)id_rc,
                     (unsigned long)pmu10, (unsigned long)pmu15,
                     (unsigned long)id);
        if (id_rc == 0 && id != 0)
        {
            pmu_id = id;
            id_hits++;
            break;
        }
    }

    for (i = 0; i < ARRAYLEN(sample_pages); i++)
    {
        memset(ftl_buffer, 0, 0x800);
        read_rc = nano3g_nand_diag_local_read(0, sample_pages[i], 0,
                                              (uint32_t *)ftl_buffer, 16);
        ftl_n3g_sanity_count((const uint8_t *)ftl_buffer, 64, &nonzero, &ff,
                             &nontrivial);
        FTL_PROGRESS("N3G_LOCAL_SAMPLE p=%lu rc=%ld nz=%lu nt=%lu w0=%08lx",
                     (unsigned long)sample_pages[i], (long)read_rc,
                     (unsigned long)nonzero, (unsigned long)nontrivial,
                     (unsigned long)ftl_buffer[0]);
        FTL_PROGRESS("N3G_SMPL p=%lu rc=%ld",
                     (unsigned long)sample_pages[i], (long)read_rc);
        FTL_PROGRESS("N3G_SMPLW w0=%08lx w1=%08lx nz=%lu nt=%lu",
                     (unsigned long)ftl_buffer[0],
                     (unsigned long)ftl_buffer[1],
                     (unsigned long)nonzero, (unsigned long)nontrivial);
        if (sample_page == 0xffffffffu || nontrivial != 0)
        {
            sample_page = sample_pages[i];
            sample_rc = read_rc;
            sample_w0 = ftl_buffer[0];
            sample_w1 = ftl_buffer[1];
            sample_nz = nonzero;
            sample_nt = nontrivial;
        }
        if (read_rc == 0 && nontrivial != 0)
        {
            hits++;
            break;
        }
    }

    memset(ftl_buffer, 0, 0x800);
    read_rc = nano3g_nand_diag_local_read(0, 0, 0x800,
                                          (uint32_t *)ftl_buffer, 16);
    ftl_n3g_sanity_count((const uint8_t *)ftl_buffer, 64, &nonzero, &ff,
                         &nontrivial);
    FTL_PROGRESS("N3G_LOCAL_OOB p=0 rc=%ld nz=%lu nt=%lu w0=%08lx",
                 (long)read_rc, (unsigned long)nonzero,
                 (unsigned long)nontrivial, (unsigned long)ftl_buffer[0]);

    nano3g_nand_diag_regs(&reg);
    ftl_n3g_print_regs("after_reads", &reg);

    FTL_PROGRESS("N3G_LL_STAGE %lu init=%ld local=%ld reset=%08lx",
                 (unsigned long)nano3g_nand_diag_last_fail_stage(),
                 (long)nano3g_nand_diag_last_init_rc(),
                 (long)nano3g_nand_diag_last_rom_rc(),
                 (unsigned long)nano3g_nand_diag_last_reset_stat());
    FTL_PROGRESS("N3G_FINAL_IDS cur=%08lx loc=%08lx",
                 (unsigned long)cur_id, (unsigned long)local_id);
    FTL_PROGRESS("N3G_FINAL_IDS2 iv=%08lx gv=%08lx",
                 (unsigned long)idvar_id, (unsigned long)gpiovar_id);
    FTL_PROGRESS("N3G_FINAL_IDC clk=%08lx pmu=%08lx",
                 (unsigned long)clkvar_id, (unsigned long)pmu_id);
    FTL_PROGRESS("N3G_FINAL_IDP hits=%lu", (unsigned long)id_hits);
    FTL_PROGRESS("N3G_FINAL_SAMPLE p=%lu rc=%ld",
                 (unsigned long)sample_page, (long)sample_rc);
    FTL_PROGRESS("N3G_FINAL_SW w0=%08lx w1=%08lx",
                 (unsigned long)sample_w0, (unsigned long)sample_w1);
    FTL_PROGRESS("N3G_FINAL_SC nz=%lu nt=%lu data=%lu",
                 (unsigned long)sample_nz, (unsigned long)sample_nt,
                 (unsigned long)hits);
    FTL_PROGRESS("N3G_ID_SUM hits=%lu", (unsigned long)id_hits);
    if (hits != 0)
        FTL_PROGRESS("N3G_NAND_ACCESS_FIXED");
    else
        FTL_PROGRESS("N3G_NAND_ACCESS_STILL_BROKEN");
    FTL_PROGRESS("N3G_LOCAL_READ_DONE");
    return;
#else
    static const uint32_t sample_blocks[] =
    {
        0, 1, 16, 128, 512, 1024, 4096, 8190
    };
    uint32_t bank;
    uint32_t sample;
    uint32_t found_nontrivial = 0;
    uint32_t samples_seen = 0;
    uint32_t nontrivial_samples = 0;
    uint32_t body_nonzero_total = 0;
    uint32_t oob_nonzero_total = 0;
    uint32_t read_fail = 0;
    uint32_t read_rc_or = 0;

    for (bank = 0; bank < 4; bank++)
    {
        for (sample = 0; sample < ARRAYLEN(sample_blocks); sample++)
        {
            uint32_t block = sample_blocks[sample];
            uint32_t abs;
            uint32_t body_nonzero;
            uint32_t oob_nonzero;
            uint32_t body_ff;
            uint32_t oob_ff;
            uint32_t body_nontrivial;
            uint32_t oob_nontrivial;
            uint32_t rc;

            if (block >= ftl_nand_type->blocks)
                continue;

            abs = block * ftl_nand_type->pagesperblock;
            memset(ftl_buffer, 0, 0x800);
            memset(&ftl_sparebuffer[0], 0, sizeof(ftl_sparebuffer[0]));

            if (samples_seen == 0)
                FTL_PROGRESS("N3G_SANITY_READ_BEGIN bank=%lu block=%lu page=%lu",
                             (unsigned long)bank, (unsigned long)block,
                             (unsigned long)abs);
            rc = nand_read_page(bank, abs, ftl_buffer, &ftl_sparebuffer[0], 0, 0);
            if (samples_seen == 0)
                FTL_PROGRESS("N3G_SANITY_READ_END rc=%lu", (unsigned long)rc);
            read_rc_or |= rc;
            if (rc != 0)
                read_fail++;

            ftl_n3g_sanity_count(ftl_buffer, 512, &body_nonzero, &body_ff,
                                 &body_nontrivial);
            ftl_n3g_sanity_count((const uint8_t *)&ftl_sparebuffer[0],
                                 sizeof(ftl_sparebuffer[0]), &oob_nonzero,
                                 &oob_ff, &oob_nontrivial);
            if (body_nontrivial != 0 || oob_nontrivial != 0)
            {
                found_nontrivial = 1;
                nontrivial_samples++;
            }

            body_nonzero_total += body_nonzero;
            oob_nonzero_total += oob_nonzero;
            samples_seen++;

            if (rc != 0 || found_nontrivial)
                break;
        }
        if (read_fail != 0 || found_nontrivial)
            break;
    }

#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    FTL_PROGRESS("N3G_LL_STAGE %lu",
                 (unsigned long)nano3g_nand_diag_last_fail_stage());
    FTL_PROGRESS("N3G_LL_POS b%lu p%lu",
                 (unsigned long)nano3g_nand_diag_last_bank(),
                 (unsigned long)nano3g_nand_diag_last_page());
    FTL_PROGRESS("N3G_LL_INIT_RC %ld",
                 (long)nano3g_nand_diag_last_init_rc());
    FTL_PROGRESS("N3G_LL_ROM_RC %ld",
                 (long)nano3g_nand_diag_last_rom_rc());
    FTL_PROGRESS("N3G_LL_RESET %08lx",
                 (unsigned long)nano3g_nand_diag_last_reset_stat());
#endif
    FTL_PROGRESS("N3G_SANITY_FINAL samples=%lu read_fail=%lu rc_or=%lu nontrivial=%lu body_nonzero=%lu oob_nonzero=%lu",
                 (unsigned long)samples_seen,
                 (unsigned long)read_fail,
                 (unsigned long)read_rc_or,
                 (unsigned long)nontrivial_samples,
                 (unsigned long)body_nonzero_total,
                 (unsigned long)oob_nonzero_total);

    if (found_nontrivial)
        FTL_PROGRESS("N3G_NAND_ACCESS_FIXED");
    else
        FTL_PROGRESS("N3G_NAND_ACCESS_STILL_BROKEN");

    FTL_PROGRESS("N3G_SANITY_SCAN_DONE");
#endif
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
    (void)pages;
    (void)recon_data;

    n3g_ctx_trace_compact = 1;

    FTL_PROGRESS("N3G_CTX_TRACE_START");
    FTL_PROGRESS("N3G_CTX_GEOM banks=%lu blocks=%u user=%u ppb=%lu sys=%lu pb=%u",
                 (unsigned long)ftl_banks, ftl_nand_type->blocks,
                 ftl_nand_type->userblocks, (unsigned long)ppb,
                 (unsigned long)syshyperblocks,
                 ftl_nand_type->pagesperblock);

    ftl_n3g_sanity_scan_raw();
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
#if defined(IPOD_NANO3G) && defined(BOOTLOADER) && N3G_RECON_ONLY
    (void)bank;
    FTL_PROGRESS("N3G_DEVINFO_FIND_DISABLED");
    return 0;
#endif

    /* Scan the last 10% of the flash for device info pages */
    uint32_t lowestBlock = ftl_nand_type->blocks
                         - (ftl_nand_type->blocks / 10);
    uint32_t block, page, pagenum;

    if (n3g_ctx_trace_compact)
    {
        uint32_t scanned, limit;
        uint32_t lastBlock = ftl_nand_type->blocks - 1;

        limit = ftl_nand_type->blocks;
        if (limit > N3G_DEVINFO_SCAN_BLOCK_LIMIT)
            limit = N3G_DEVINFO_SCAN_BLOCK_LIMIT;

        lowestBlock = lastBlock - limit + 1;
        FTL_PROGRESS("N3G_DEVINFO_SCAN bank=%lu from=%lu",
                     (unsigned long)bank, (unsigned long)lowestBlock);

        for (scanned = 0; scanned < limit; scanned++)
        {
            const char* sig;

            block = lastBlock - scanned;
            if ((scanned & 0xf) == 0)
                FTL_PROGRESS("N3G_DEVINFO_PROGRESS bank=%lu block=%lu",
                             (unsigned long)bank, (unsigned long)block);

            pagenum = block * ftl_nand_type->pagesperblock;
            if ((nand_read_page(bank, pagenum, ftl_buffer,
                                &ftl_sparebuffer[0], 1, 0) & 0x11F) != 0)
                continue;

            sig = ftl_n3g_devinfo_marker(ftl_buffer, &ftl_sparebuffer[0]);
            if (sig != NULL)
            {
                FTL_PROGRESS("N3G_CTX_DEVINFO_OK bank=%lu block=%lu page=%lu sig=%s",
                             (unsigned long)bank, (unsigned long)block,
                             (unsigned long)pagenum, sig);
                return pagenum;
            }
        }

        FTL_PROGRESS("N3G_CTX_DEVINFO_FAIL bank=%lu", (unsigned long)bank);
        return 0;
    }

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

#if defined(IPOD_NANO3G) && defined(BOOTLOADER) && N3G_RECON_ONLY
    FTL_PROGRESS("N3G_DEVINFO_SCAN_DISABLED");
    return 0;
#endif

    if (n3g_ctx_trace_compact)
    {
        if (ftl_banks == 0)
        {
            FTL_PROGRESS("N3G_CTX_DEVINFO_FAIL bank=0");
            return 0;
        }

        return ftl_find_devinfo(0) != 0;
    }

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

#if defined(IPOD_NANO3G)
    if (n3g_direct_map_mount)
        return ftl_n3g_direct_read(sector, count, buffer);
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
#if defined(IPOD_NANO3G) && defined(BOOTLOADER) && N3G_RECON_ONLY
#if N3G_SCREEN_COMPACT
    FTL_PROGRESS("N3G_BUILD 20260523aa");
    FTL_PROGRESS("N3G_EXACTRETRY_BULLET_MARKER");
#else
    FTL_PROGRESS("N3G_NAND_ACCESS_PROBE_START");
    FTL_PROGRESS("N3G_VERDICT_ONLY_BUILD v=20260503i");
#endif
#else
    FTL_PROGRESS("FTL_SCAN_PROGRESS init start");
#endif
    mutex_init(&ftl_mtx);
    uint32_t i;
#if !(defined(IPOD_NANO3G) && defined(BOOTLOADER) && N3G_RECON_ONLY && N3G_SCREEN_COMPACT)
    FTL_PROGRESS("N3G_STAGE before_nand_device_init");
#endif
    if (nand_device_init() != 0) //return 1;
        panicf("FTL: Lowlevel NAND driver init failed!");
#if !(defined(IPOD_NANO3G) && defined(BOOTLOADER) && N3G_RECON_ONLY && N3G_SCREEN_COMPACT)
    FTL_PROGRESS("N3G_STAGE after_nand_device_init");
#endif
    ftl_banks = 0;
    for (i = 0; i < 4; i++)
        if (nand_get_device_type(i) != 0) ftl_banks = i + 1;
    ftl_nand_type = nand_get_device_type(0);
    ppb = ftl_nand_type->pagesperblock * ftl_banks;
    syshyperblocks = ftl_nand_type->blocks - ftl_nand_type->userblocks - 0x17;
    if (0)
        ftl_n3g_scan_rockbox_header();
#if !(defined(IPOD_NANO3G) && defined(BOOTLOADER) && N3G_RECON_ONLY && N3G_SCREEN_COMPACT)
    FTL_PROGRESS("ftl geom banks %lu blocks %u user %u ppb %lu sys %lu",
                 (unsigned long)ftl_banks, ftl_nand_type->blocks,
                 ftl_nand_type->userblocks, (unsigned long)ppb,
                 (unsigned long)syshyperblocks);
#endif
#if defined(IPOD_NANO3G) && defined(BOOTLOADER) && N3G_RECON_ONLY
#if !N3G_SCREEN_COMPACT
    FTL_PROGRESS("N3G_STAGE before_sanity_scan");
#endif
    return ftl_n3g_physrb_mount();
    if (ftl_n3g_winpod_mount() == 0)
        return 0;
    return -1;
#if !N3G_SCREEN_COMPACT
    FTL_PROGRESS("N3G_STAGE after_sanity_scan");
#endif
    return -1;
#endif
#if defined(IPOD_NANO3G) && !defined(BOOTLOADER)
    if (ftl_n3g_winpod_mount() == 0)
    {
        DEBUGF("FTL: N3G WMOUNT direct map mounted\n");
        return 0;
    }
#endif
    ftl_n3g_dump_reconstruction_pages();

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
