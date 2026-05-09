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
#include "system.h"
#include "s5l87xx.h"
#include "clocking-s5l8702.h"
#include "cpucache-arm.h"
#include "pmu-target.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
extern int printf(const char *format, ...);
#define N3G_NAND_DEBUG(...) do { } while (0)
#else
#define N3G_NAND_DEBUG(...) do { } while (0)
#endif

#define N3G_FMC_BASE        0x38a00000
#define N3G_FMCTRL0         (*(volatile uint32_t *)(N3G_FMC_BASE + 0x00))
#define N3G_FMCTRL1         (*(volatile uint32_t *)(N3G_FMC_BASE + 0x04))
#define N3G_FMCMD           (*(volatile uint32_t *)(N3G_FMC_BASE + 0x08))
#define N3G_FMADDR0         (*(volatile uint32_t *)(N3G_FMC_BASE + 0x0c))
#define N3G_FMADDR1         (*(volatile uint32_t *)(N3G_FMC_BASE + 0x10))
#define N3G_FMADDR2         (*(volatile uint32_t *)(N3G_FMC_BASE + 0x14))
#define N3G_FMADDR6         (*(volatile uint32_t *)(N3G_FMC_BASE + 0x24))
#define N3G_FMANUM          (*(volatile uint32_t *)(N3G_FMC_BASE + 0x2c))
#define N3G_FMDNUM          (*(volatile uint32_t *)(N3G_FMC_BASE + 0x30))
#define N3G_FMDATAW0        (*(volatile uint32_t *)(N3G_FMC_BASE + 0x34))
#define N3G_FMDATAW1        (*(volatile uint32_t *)(N3G_FMC_BASE + 0x38))
#define N3G_FMCSTAT         (*(volatile uint32_t *)(N3G_FMC_BASE + 0x48))
#define N3G_FMSTAGE0        (*(volatile uint32_t *)(N3G_FMC_BASE + 0x60))
#define N3G_FMSTAGE1        (*(volatile uint32_t *)(N3G_FMC_BASE + 0x64))
#define N3G_FMSTAGE2        (*(volatile uint32_t *)(N3G_FMC_BASE + 0x68))
#define N3G_FMSTAGECMD      (*(volatile uint32_t *)(N3G_FMC_BASE + 0x78))
#define N3G_FMSTAGECTRL     (*(volatile uint32_t *)(N3G_FMC_BASE + 0x7c))
#define N3G_FMFIFO          (*(volatile uint32_t *)(N3G_FMC_BASE + 0x80))
#define N3G_FMDATA          N3G_FMFIFO
#define N3G_FMUNK840        (*(volatile uint32_t *)(N3G_FMC_BASE + 0x840))
#define N3G_PLATFORM_MISC   (*(volatile uint32_t *)(0x39300000 + 0x3c))
#define N3G_TEST_BANK       0u
#define N3G_MISC_MODE_FORCE 1u
#define N3G_MISC_MODE_VALUE 0x0u
#define N3G_PAGE_SCAN_COUNT 12
#define N3G_NAND_BANKS      4

#define N3G_ROM_CLOCKGATE_ENABLE ((void (*)(uint32_t))0x2000147c)
#define N3G_ROM_NAND_GPIO_SETUP  ((void (*)(uint32_t))0x20001830)
#define N3G_ROM_NAND_RESET       ((int (*)(uint32_t, uint32_t))0x200096e4)
#define N3G_ROM_NAND_READ_PAGE   ((int (*)(uint32_t, uint32_t, uint32_t, void *, void *))0x20009910)
#define N3G_ROM_DISABLE_ICACHE   ((void (*)(void))0x20000418)
#define N3G_ROM_EXTRA_ADDR       ((uint32_t *)0x22000000)
#define N3G_ROM_DATA_ADDR        ((uint32_t *)0x22000100)
#define N3G_ROM_DATA_SIZE        0x800
#define N3G_ROM_EXTRA_SIZE       0x40
#define N3G_FMCSTAT_BANK_READY(bank) (0x1000u << (bank))

volatile uint32_t nano3g_nand_entry_diag[10] =
{
    0x4e334745, /* "N3GE" */
    0xffffffff, 0, 0, 0, 0, 0, 0, 0, 0
};

volatile uint32_t nano3g_nand_stage_diag[39] =
{
    [0] = 0x4e334753, /* "N3GS" */
    [3] = 0xffffffff
};

struct nano3g_nand_direct_diag
{
    uint32_t stat0;
    uint32_t stat1;
    uint32_t id;
    int32_t id_rc;
    int32_t page_rc;
    int32_t spare_rc;
    uint32_t page0;
    uint32_t page1;
    uint32_t spare0;
    uint32_t spare1;
    uint32_t csum1;
    uint32_t csum1_calc;
    uint32_t csum2;
    uint32_t csum2_calc;
    uint32_t pcon8;
    uint32_t pcon9;
    uint32_t pcon10;
    uint32_t stat_reset;
    int32_t reset_rc;
    uint32_t gpiocmd;
    uint32_t pwr0;
    uint32_t pwr1;
    uint32_t clk1;
    int32_t page_status_rc;
    int32_t page_xfer_rc;
    uint32_t xfer_stat;
    uint32_t xfer_ctrl0;
    uint32_t xfer_ctrl1;
    uint32_t xfer_addr2;
    uint32_t xfer_dnum;
    uint32_t read_stat_cmd0;
    uint32_t read_stat_addr;
    uint32_t read_stat_cmd30;
    uint32_t read_stat_cmd70;
    uint32_t read_stat_wait;
    uint32_t read_stat_post0;
    int32_t read_post0_rc;
    uint32_t scratch_addr;
    uint32_t scratch0;
    uint32_t scratch1;
    uint32_t test_bank;
    uint32_t test_ctrl0;
    uint32_t misc_before;
    uint32_t misc_after;
    uint32_t misc_force;
    uint32_t misc_mode;
    uint32_t scratch_words[16];
    uint32_t page_imm_words[16];
    uint32_t scan_pattern;
    uint32_t scan_count;
    uint32_t scan_hits[8];
    uint32_t scan_dumps[4][16];
    uint32_t dataw0;
    uint32_t dataw1;
    uint32_t read_addr0;
    uint32_t read_addr1;
    uint32_t read_anum;
    uint32_t ctrl_before_read0;
    uint32_t ctrl_before_addr;
    uint32_t ctrl_before_read30;
    uint32_t ctrl_before_copy;
    uint32_t stat_before_copy;
    uint32_t source_sweep[9][2];
    uint32_t q78_trace[4][3];
    uint32_t dump_pages[N3G_PAGE_SCAN_COUNT];
    int32_t dump_rc[N3G_PAGE_SCAN_COUNT];
    uint32_t dump_sig510[N3G_PAGE_SCAN_COUNT];
    uint32_t dump_non_ff[N3G_PAGE_SCAN_COUNT];
    uint32_t dump_words[N3G_PAGE_SCAN_COUNT][16];
    uint32_t first_non_ff_page;
    uint32_t first_mbr_page;
    int32_t sector0_rc;
    int32_t sector0_init_rc;
    uint32_t sector0_sig510;
    uint32_t sector0_non_ff;
    uint32_t sector0_words[128];
};

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

struct nand_device_info_type
{
    uint32_t id;
    uint16_t blocks;
    uint16_t userblocks;
    uint16_t pagesperblock;
    uint8_t blocksizeexponent;
    uint8_t tunk1;
    uint8_t twp;
    uint8_t tunk2;
    uint8_t tunk3;
} __attribute__((packed));

uint32_t ftl_init(void);
uint32_t ftl_read(uint32_t sector, uint32_t count, void* buffer);
uint32_t ftl_sync(void);

static uint32_t nano3g_pagebuf[0x200] STORAGE_ALIGN_ATTR;
static uint32_t nano3g_sparebuf[0x10] STORAGE_ALIGN_ATTR;
static uint32_t nano3g_sector0buf[0x200] STORAGE_ALIGN_ATTR;
static int n3g_ftl_init_done;
static int32_t n3g_ftl_init_rc = 0x7fffffff;
static uint32_t n3g_rom_bank_init_mask;
static uint32_t n3g_rom_bank_tried_mask;
static uint32_t n3g_rom_icache_disabled;
static int n3g_rom_bank_init_rc[N3G_NAND_BANKS];
static int32_t n3g_diag_last_init_rc = 0x7fffffff;
static int32_t n3g_diag_last_rom_rc = 0x7fffffff;
static uint32_t n3g_diag_last_bank;
static uint32_t n3g_diag_last_page;
static uint32_t n3g_diag_last_reset_stat;
static uint32_t n3g_diag_last_fail_stage;
static int n3g_last_page_status_rc;
static int n3g_last_page_xfer_rc;
static uint32_t n3g_last_xfer_stat;
static uint32_t n3g_last_xfer_ctrl0;
static uint32_t n3g_last_xfer_ctrl1;
static uint32_t n3g_last_xfer_addr2;
static uint32_t n3g_last_xfer_dnum;
static uint32_t n3g_read_stat_cmd0;
static uint32_t n3g_read_stat_addr;
static uint32_t n3g_read_stat_cmd30;
static uint32_t n3g_read_stat_cmd70;
static uint32_t n3g_read_stat_wait;
static uint32_t n3g_read_stat_post0;
static int n3g_read_post0_rc;
static uint32_t n3g_scratch_addr;
static uint32_t n3g_scratch0;
static uint32_t n3g_scratch1;
static uint32_t n3g_misc_before;
static uint32_t n3g_misc_after;
static uint32_t n3g_scratch_words[16];
static uint32_t n3g_page_imm_words[16];
static uint32_t n3g_scan_count;
static uint32_t n3g_scan_dumps[4][16];
static uint32_t n3g_last_dataw0;
static uint32_t n3g_last_dataw1;
static uint32_t n3g_read_addr0;
static uint32_t n3g_read_addr1;
static uint32_t n3g_read_anum;
static uint32_t n3g_ctrl_before_read0;
static uint32_t n3g_ctrl_before_addr;
static uint32_t n3g_ctrl_before_read30;
static uint32_t n3g_ctrl_before_copy;
static uint32_t n3g_stat_before_copy;
static uint32_t n3g_source_sweep[9][2];
static uint32_t n3g_q78_trace[4][3];
static uint32_t n3g_stage_spare[3];
static const struct nand_device_info_type *n3g_selected_nand_type;
static int n3g_selected_nand_type_done;

static const struct nand_device_info_type n3g_nand_deviceinfotable[] =
{
    /*
     * Real Nano 3G Micron ID observed from DFU/wInd3x:
     * raw bytes 2c d5 d5 a5, little-endian id 0xa5d5d52c.
     * This row matches the existing Nano 2G Whimory table entry for the
     * same chip ID.
     */
    {0xA5D5D52C, 8192, 7744, 0x80, 7, 3, 2, 2, 1},
    /* Older guessed Samsung-style candidates kept only as fallbacks. */
    {0xA555D5AD, 8192, 7744, 0x80, 7, 3, 2, 3, 2},
    {0xA514D3AD, 4096, 3872, 0x80, 7, 3, 2, 3, 2},
};

static int n3g_reset_bank_idx(uint32_t bank, uint32_t *stat);
static const struct nand_device_info_type *n3g_select_nand_type(void);
static int n3g_read_id_exact(uint32_t *id);
static int n3g_read_id_current_state(uint32_t *id);
static int n3g_read_id_ctrl0(uint32_t bank, uint32_t ctrl0, uint32_t *id);
static int n3g_read_id_gpio_profile(uint32_t bank, uint32_t profile,
                                    uint32_t ctrl0, uint32_t *id);
static int n3g_read_id_clock_profile(uint32_t bank, uint32_t profile,
                                     uint32_t *id, uint32_t *clk_before,
                                     uint32_t *clk_during,
                                     uint32_t *clk_after);
static int n3g_read_id_cp15_profile(uint32_t bank, uint32_t *id,
                                    uint32_t *cp_before,
                                    uint32_t *cp_during,
                                    uint32_t *cp_after);
static void n3g_fmc_setup_bank_profile(uint32_t bank, uint32_t profile);
static int n3g_issue_read_bank(uint32_t bank, uint32_t page, uint32_t offset);
static int n3g_internal_page_xfer_bank(uint32_t bank, uint32_t *buf);
static int n3g_read_page_bootrom_xfer_bank(uint32_t bank, uint32_t page,
                                           uint32_t *buf);
static int n3g_read_page_exact_bank(uint32_t bank, uint32_t page,
                                    uint32_t offset, uint32_t *buf,
                                    uint32_t words);
static void n3g_extract_stage_spare(void);
static void n3g_copy_stage_spare(uint32_t *spare);

static uint32_t n3g_mbr_sig510(const uint32_t *buf)
{
    const uint8_t *b = (const uint8_t *)buf;

    return ((uint32_t)b[508] << 0) | ((uint32_t)b[509] << 8) |
           ((uint32_t)b[510] << 16) | ((uint32_t)b[511] << 24);
}

static bool n3g_page_has_non_ff(const uint32_t *buf)
{
    for (int i = 0; i < 0x200; i++)
        if (buf[i] != 0xffffffffu)
            return true;

    return false;
}

static void n3g_capture_q78_trace(int slot)
{
    n3g_q78_trace[slot][0] = *(volatile uint32_t *)(N3G_FMC_BASE + 0x74);
    n3g_q78_trace[slot][1] = *(volatile uint32_t *)(N3G_FMC_BASE + 0x78);
    n3g_q78_trace[slot][2] = *(volatile uint32_t *)(N3G_FMC_BASE + 0x7c);
}

static uint32_t n3g_fmctrl0_active(void)
{
    return (4u << 16) | (3u << 12) | (1u << 11) |
           (1u << (N3G_TEST_BANK + 1)) | 1u;
}

static uint32_t n3g_fmctrl0_active_bank(uint32_t bank)
{
    return (4u << 16) | (3u << 12) | (1u << 11) |
           (1u << (bank + 1)) | 1u;
}

static uint32_t n3g_fmctrl0_idle(void)
{
    return (4u << 16) | (3u << 12) | (1u << 11) |
           (1u << (N3G_TEST_BANK + 1));
}

static uint32_t n3g_fmctrl0_idle_bank(uint32_t bank)
{
    return (4u << 16) | (3u << 12) | (1u << 11) |
           (1u << (bank + 1));
}

static int n3g_wait(uint32_t mask)
{
    unsigned timeout = USEC_TIMER + 50000;

    while ((N3G_FMCSTAT & mask) != mask)
        if (TIME_AFTER(USEC_TIMER, timeout))
            return -1;

    N3G_FMCSTAT = mask;
    return 0;
}

static int n3g_wait_no_clear(uint32_t mask)
{
    unsigned timeout = USEC_TIMER + 50000;

    while ((N3G_FMCSTAT & mask) != mask)
        if (TIME_AFTER(USEC_TIMER, timeout))
            return -1;

    return 0;
}

static void n3g_ack_data_ready(void)
{
    N3G_FMCSTAT = 0x8;
}

static int n3g_wait_long(uint32_t mask)
{
    unsigned timeout = USEC_TIMER + 500000;

    while ((N3G_FMCSTAT & mask) != mask)
        if (TIME_AFTER(USEC_TIMER, timeout))
            return -1;

    N3G_FMCSTAT = mask;
    return 0;
}

static int n3g_wait_capture(uint32_t mask, uint32_t *seen)
{
    unsigned timeout = USEC_TIMER + 50000;

    while (((*seen = N3G_FMCSTAT) & mask) != mask)
        if (TIME_AFTER(USEC_TIMER, timeout))
            return -1;

    N3G_FMCSTAT = mask;
    return 0;
}

static int n3g_wait_long_capture(uint32_t mask, uint32_t *seen)
{
    unsigned timeout = USEC_TIMER + 500000;

    while (((*seen = N3G_FMCSTAT) & mask) != mask)
        if (TIME_AFTER(USEC_TIMER, timeout))
            return -1;

    N3G_FMCSTAT = mask;
    return 0;
}

static int n3g_wait_ready(uint32_t *stat)
{
    unsigned timeout = USEC_TIMER + 50000;

    while (((*stat = N3G_FMCSTAT) & 0x1010) == 0)
        if (TIME_AFTER(USEC_TIMER, timeout))
            return -1;

    return 0;
}

static void n3g_gpio_setup_profile(uint32_t profile)
{
    switch (profile)
    {
    default:
    case 0:
        /* Current Nano3G BootROM-helper reconstruction. */
        PCON(8) = 0x22222222;
        PCON(9) = (PCON(9) & ~0x000f000fu) | 0x00020002u;
        PCON(10) = (PCON(10) & 0xffff0000u) | 0x00002222u;
        break;
    case 1:
        /* S5L8702 6G CE-ATA storage mux profile. */
        PCON(8) = 0x33333333;
        PCON(9) = (PCON(9) & ~0x000000ffu) | 0x00000033u;
        PCON(11) |= 0xfu;
        break;
    case 2:
        /* S5L8702 6G parallel ATA storage mux profile. */
        PCON(7) = 0x44444444;
        PCON(8) = 0x44444444;
        PCON(9) = 0x44444444;
        PCON(10) = (PCON(10) & ~0xffffu) | 0x4444u;
        break;
    case 3:
        /* Extended alt-2 NAND-style profile across the adjacent groups. */
        PCON(7) = 0x22222222;
        PCON(8) = 0x22222222;
        PCON(9) = 0x22222222;
        PCON(10) = (PCON(10) & ~0xffffu) | 0x2222u;
        PCON(11) |= 0xfu;
        break;
    case 4:
        /* Minimal alt-2 data/command profile with explicit low group cleanup. */
        PCON(7) = 0;
        PCON(8) = 0x22222222;
        PCON(9) = (PCON(9) & ~0x000f000fu) | 0x00020002u;
        PCON(10) = (PCON(10) & ~0xffffu) | 0x2222u;
        PCON(11) &= ~0xfu;
        break;
    }
}

static void n3g_gpio_setup(void)
{
    n3g_gpio_setup_profile(0);
}

static void n3g_fmc_setup(void)
{
    n3g_misc_before = N3G_PLATFORM_MISC;
    if (N3G_MISC_MODE_FORCE)
        N3G_PLATFORM_MISC = (n3g_misc_before & ~0x7u) | N3G_MISC_MODE_VALUE;
    else
        N3G_PLATFORM_MISC = n3g_misc_before | N3G_MISC_MODE_VALUE;
    n3g_misc_after = N3G_PLATFORM_MISC;

    n3g_gpio_setup();
    /*
     * DFU state capture shows the working BootROM path does not require the
     * old broad power-mask clear. Preserve existing gates and only ensure the
     * NAND clocks themselves are enabled.
     */
    clockgate_enable(CLOCKGATE_NAND, true);
    clockgate_enable(CLOCKGATE_NANDECC, true);
    N3G_FMCTRL0 = n3g_fmctrl0_active();
    N3G_FMCTRL1 = 0x1c0;
}

static void n3g_fmc_setup_bank(uint32_t bank)
{
    n3g_fmc_setup_bank_profile(bank, 0);
}

static void n3g_fmc_setup_bank_profile(uint32_t bank, uint32_t profile)
{
    n3g_misc_before = N3G_PLATFORM_MISC;
    if (N3G_MISC_MODE_FORCE)
        N3G_PLATFORM_MISC = (n3g_misc_before & ~0x7u) | N3G_MISC_MODE_VALUE;
    else
        N3G_PLATFORM_MISC = n3g_misc_before | N3G_MISC_MODE_VALUE;
    n3g_misc_after = N3G_PLATFORM_MISC;

    n3g_gpio_setup_profile(profile);
    /*
     * DFU state capture shows the working BootROM path does not require the
     * old broad power-mask clear. Preserve existing gates and only ensure the
     * NAND clocks themselves are enabled.
     */
    clockgate_enable(CLOCKGATE_NAND, true);
    clockgate_enable(CLOCKGATE_NANDECC, true);
    N3G_FMCTRL0 = n3g_fmctrl0_active_bank(bank);
    N3G_FMCTRL1 = 0x1c0;
}

static int n3g_rom_init_bank(uint32_t bank)
{
    int rc;
    uint32_t stat = 0;

    N3G_NAND_DEBUG("N3G_INIT_BANK_START bank=%lu", (unsigned long)bank);
    if (bank >= N3G_NAND_BANKS)
        return -1;
    if (n3g_rom_bank_init_mask & (1u << bank))
        return 0;
    if (n3g_rom_bank_tried_mask & (1u << bank))
        return n3g_rom_bank_init_rc[bank];

    if (!n3g_rom_icache_disabled)
    {
        /*
         * wInd3x disables I-cache before executing tiny DFU payloads, but the
         * BootROM helper can hang when called from the relocated Rockbox
         * bootloader context. Leave Rockbox's cache state alone here.
         */
        N3G_NAND_DEBUG("N3G_INIT_DISABLE_ICACHE_SKIP");
        n3g_rom_icache_disabled = 1;
    }

    N3G_NAND_DEBUG("N3G_INIT_CLOCKS_SKIP");
    N3G_NAND_DEBUG("N3G_INIT_GPIO_SKIP");
    N3G_NAND_DEBUG("N3G_INIT_RESET_LOCAL");

    /*
     * The BootROM reset helper can block when called from the loaded
     * bootloader context. Use the local register-level setup/reset here,
     * then call only the BootROM page read helper for the data phase.
     */
    n3g_rom_bank_tried_mask |= 1u << bank;
    rc = n3g_reset_bank_idx(bank, &stat);
    n3g_diag_last_reset_stat = stat;
    N3G_NAND_DEBUG("N3G_INIT_RESET_DONE rc=%ld", (long)rc);
    n3g_rom_bank_init_rc[bank] = rc;
    if (rc == 0)
        n3g_rom_bank_init_mask |= 1u << bank;

    return rc;
}

static int n3g_rom_read_page_bank(uint32_t bank, uint32_t page,
                                  uint32_t *data, uint32_t *extra)
{
    int rc;
    int init_rc;
    uint32_t *rom_data = N3G_ROM_DATA_ADDR;
    uint32_t *rom_extra = N3G_ROM_EXTRA_ADDR;

    n3g_diag_last_bank = bank;
    n3g_diag_last_page = page;
    n3g_diag_last_rom_rc = 0x7fffffff;
    n3g_diag_last_fail_stage = 0;

    init_rc = n3g_rom_init_bank(bank);
    n3g_diag_last_init_rc = init_rc;
    if (init_rc != 0)
    {
        n3g_diag_last_fail_stage = 1;
        return -1;
    }

    /*
     * Match wInd3x's Nano 3G BootROM NAND call convention exactly:
     * spare/OOB at 0x22000000 and page data at 0x22000100.
     */
    memset(rom_data, 0, N3G_ROM_DATA_SIZE);
    memset(rom_extra, 0, N3G_ROM_EXTRA_SIZE);
    commit_discard_dcache_range(rom_data, N3G_ROM_DATA_SIZE);
    commit_discard_dcache_range(rom_extra, N3G_ROM_EXTRA_SIZE);

    N3G_NAND_DEBUG("N3G_ROM_READ_CALL bank=%lu page=%lu",
                   (unsigned long)bank, (unsigned long)page);
    rc = N3G_ROM_NAND_READ_PAGE(0, bank, page, rom_data, rom_extra);
    n3g_diag_last_rom_rc = rc;
    N3G_NAND_DEBUG("N3G_ROM_READ_DONE rc=%ld", (long)rc);

    if (rc != 0)
    {
        n3g_diag_last_fail_stage = 2;
        return rc;
    }

    if (data != NULL)
        memcpy(data, rom_data, N3G_ROM_DATA_SIZE);
    if (extra != NULL)
        memcpy(extra, rom_extra, N3G_ROM_EXTRA_SIZE);

    return 0;
}

static int n3g_reset_bank(uint32_t *stat)
{
    return n3g_reset_bank_idx(N3G_TEST_BANK, stat);
}

int32_t nano3g_nand_diag_last_init_rc(void)
{
    return n3g_diag_last_init_rc;
}

int32_t nano3g_nand_diag_last_rom_rc(void)
{
    return n3g_diag_last_rom_rc;
}

uint32_t nano3g_nand_diag_last_bank(void)
{
    return n3g_diag_last_bank;
}

uint32_t nano3g_nand_diag_last_page(void)
{
    return n3g_diag_last_page;
}

uint32_t nano3g_nand_diag_last_reset_stat(void)
{
    return n3g_diag_last_reset_stat;
}

uint32_t nano3g_nand_diag_last_fail_stage(void)
{
    return n3g_diag_last_fail_stage;
}

int32_t nano3g_nand_diag_read_id(uint32_t *id)
{
    int32_t rc;

    *id = 0;
    rc = n3g_read_id_exact(id);
    n3g_diag_last_rom_rc = rc;
    return rc;
}

int32_t nano3g_nand_diag_read_id_current(uint32_t *id)
{
    int32_t rc;

    n3g_diag_last_bank = N3G_TEST_BANK;
    n3g_diag_last_page = 0;
    n3g_diag_last_init_rc = 0x7fffffff;
    n3g_diag_last_rom_rc = 0x7fffffff;
    n3g_diag_last_fail_stage = 0;

    rc = n3g_read_id_current_state(id);
    n3g_diag_last_rom_rc = rc;
    if (rc != 0)
        n3g_diag_last_fail_stage = 2;

    return rc;
}

int32_t nano3g_nand_diag_read_id_variant(uint32_t bank, uint32_t variant,
                                         uint32_t *ctrl0, uint32_t *id)
{
    static const uint32_t timing[][2] =
    {
        {4, 3},
        {3, 2},
        {7, 7},
        {2, 2},
    };
    uint32_t t = variant & 3u;
    uint32_t bankbit;

    if ((variant & 4u) != 0)
        bankbit = 1u << bank;
    else
        bankbit = 1u << (bank + 1);

    *ctrl0 = (timing[t][0] << 16) | (timing[t][1] << 12)
           | (1u << 11) | bankbit | 1u;
    *id = 0;
    return n3g_read_id_ctrl0(bank, *ctrl0, id);
}

int32_t nano3g_nand_diag_gpio_id_variant(uint32_t bank, uint32_t variant,
                                         uint32_t *ctrl0, uint32_t *id,
                                         uint32_t *pcon7, uint32_t *pcon8,
                                         uint32_t *pcon9, uint32_t *pcon10,
                                         uint32_t *pcon11)
{
    *ctrl0 = n3g_fmctrl0_active_bank(bank);
    *id = 0;
    int32_t rc = n3g_read_id_gpio_profile(bank, variant, *ctrl0, id);

    *pcon7 = PCON(7);
    *pcon8 = PCON(8);
    *pcon9 = PCON(9);
    *pcon10 = PCON(10);
    *pcon11 = PCON(11);
    return rc;
}

int32_t nano3g_nand_diag_clock_id_variant(uint32_t bank, uint32_t variant,
                                          uint32_t *id,
                                          uint32_t *clk_before,
                                          uint32_t *clk_during,
                                          uint32_t *clk_after)
{
    if (variant >= 4)
        return -1;

    return n3g_read_id_clock_profile(bank, variant, id, clk_before,
                                     clk_during, clk_after);
}

int32_t nano3g_nand_diag_cp15_id(uint32_t bank, uint32_t *id,
                                 uint32_t *cp_before,
                                 uint32_t *cp_during,
                                 uint32_t *cp_after)
{
    return n3g_read_id_cp15_profile(bank, id, cp_before, cp_during,
                                    cp_after);
}

int32_t nano3g_nand_diag_pmu_id_variant(uint32_t variant, uint32_t *pmu10,
                                        uint32_t *pmu15, uint32_t *id)
{
    static const uint8_t pmu10_values[] =
    {
        0x08,
        0x18,
        0x10,
        0x38,
        0x00,
    };
    uint8_t old10 = pmu_rd(0x10);
    uint8_t old15 = pmu_rd(0x15);
    uint8_t val10;
    int32_t rc;

    if (variant >= ARRAYLEN(pmu10_values))
        return -1;

    val10 = (old10 & 0xc7u) | pmu10_values[variant];
    pmu_wr(0x15, 0x14);
    pmu_wr(0x10, val10);
    sleep(HZ / 20);

    *pmu10 = pmu_rd(0x10);
    *pmu15 = pmu_rd(0x15);
    *id = 0;
    rc = n3g_read_id_exact(id);

    pmu_wr(0x15, old15);
    pmu_wr(0x10, old10);
    return rc;
}

int32_t nano3g_nand_diag_init_only(uint32_t bank)
{
    int32_t rc;

    n3g_diag_last_bank = bank;
    n3g_diag_last_page = 0;
    n3g_diag_last_rom_rc = 0x7fffffff;
    n3g_diag_last_fail_stage = 0;
    rc = n3g_rom_init_bank(bank);
    n3g_diag_last_init_rc = rc;
    if (rc != 0)
        n3g_diag_last_fail_stage = 1;

    return rc;
}

int32_t nano3g_nand_diag_local_read(uint32_t bank, uint32_t page,
                                    uint32_t offset, uint32_t *buf,
                                    uint32_t words)
{
    int32_t rc;
    int32_t init_rc;

    n3g_diag_last_bank = bank;
    n3g_diag_last_page = page;
    n3g_diag_last_rom_rc = 0x7fffffff;
    n3g_diag_last_fail_stage = 0;

    init_rc = n3g_rom_init_bank(bank);
    n3g_diag_last_init_rc = init_rc;
    if (init_rc != 0)
    {
        n3g_diag_last_fail_stage = 1;
        return init_rc;
    }

    rc = n3g_read_page_exact_bank(bank, page, offset, buf, words);
    n3g_diag_last_rom_rc = rc;
    if (rc != 0)
        n3g_diag_last_fail_stage = 3;

    return rc;
}

int32_t nano3g_nand_diag_rom_read(uint32_t bank, uint32_t page,
                                  uint32_t *data, uint32_t *extra)
{
    return n3g_rom_read_page_bank(bank, page, data, extra);
}

void nano3g_nand_diag_regs(struct nano3g_nand_reg_diag *diag)
{
    diag->fmctrl0 = N3G_FMCTRL0;
    diag->fmctrl1 = N3G_FMCTRL1;
    diag->fmcmd = N3G_FMCMD;
    diag->fmaddr0 = N3G_FMADDR0;
    diag->fmaddr1 = N3G_FMADDR1;
    diag->fmaddr2 = N3G_FMADDR2;
    diag->fmanum = N3G_FMANUM;
    diag->fmdnum = N3G_FMDNUM;
    diag->fmcstat = N3G_FMCSTAT;
    diag->fmstage0 = N3G_FMSTAGE0;
    diag->fmstage1 = N3G_FMSTAGE1;
    diag->fmstage2 = N3G_FMSTAGE2;
    diag->fmstagecmd = N3G_FMSTAGECMD;
    diag->fmstagectrl = N3G_FMSTAGECTRL;
    diag->pwr0 = PWRCON(0);
    diag->pwr1 = PWRCON(1);
    diag->clkcon1 = CLKCON1;
    diag->pcon7 = PCON(7);
    diag->pcon8 = PCON(8);
    diag->pcon9 = PCON(9);
    diag->pcon10 = PCON(10);
    diag->pcon11 = PCON(11);
    diag->gpiocmd = GPIOCMD;
    diag->misc = N3G_PLATFORM_MISC;
}

static int n3g_reset_bank_idx(uint32_t bank, uint32_t *stat)
{
    N3G_NAND_DEBUG("N3G_RESET_SETUP_BANK");
    n3g_fmc_setup_bank(bank);
    N3G_NAND_DEBUG("N3G_RESET_CMD_FF");
    N3G_FMCTRL0 = n3g_fmctrl0_active_bank(bank);
    N3G_FMCMD = 0xff;
    if (n3g_wait(0x2) != 0)
        return -1;

    N3G_NAND_DEBUG("N3G_RESET_STATUS_70");
    N3G_FMCTRL1 = 0x001000e0;
    N3G_FMADDR2 = 1;
    N3G_FMCMD = 0x70;
    if (n3g_wait(0x2) != 0)
        return -2;

    N3G_NAND_DEBUG("N3G_RESET_WAIT_READY");
    N3G_FMCTRL1 = 0x2a;
    if (n3g_wait_long(N3G_FMCSTAT_BANK_READY(bank)) != 0)
    {
        *stat = N3G_FMCSTAT;
        N3G_FMADDR2 = 0;
        N3G_FMCTRL1 = 0xe0;
        return -3;
    }

    N3G_FMCSTAT = N3G_FMCSTAT_BANK_READY(bank);
    N3G_FMADDR2 = 0;
    N3G_FMCTRL1 = 0xe0;
    *stat = N3G_FMCSTAT;
    N3G_NAND_DEBUG("N3G_RESET_OK");
    return 0;
}

static int n3g_read_fifo(uint32_t *buf, uint32_t words)
{
    n3g_ack_data_ready();

    N3G_FMDNUM = words * 4 - 1;
    N3G_FMADDR2 = 1;
    N3G_FMCTRL1 = 0x1c2;

    if (n3g_wait_no_clear(0x8) != 0)
        return -1;

    N3G_FMADDR2 = 0x100;
    N3G_FMCTRL1 = 0x340;

    for (uint32_t i = 0; i < words; i++)
        buf[i] = N3G_FMFIFO;

    n3g_ack_data_ready();
    return 0;
}

static int n3g_entry_wait(uint32_t mask, uint32_t clear, uint32_t *seen)
{
    uint32_t stat = 0;

    for (uint32_t i = 0; i < 0x100000; i++)
    {
        stat = N3G_FMCSTAT;
        if ((stat & mask) == mask)
        {
            if (clear)
                N3G_FMCSTAT = mask;
            *seen = stat;
            return 0;
        }
    }

    *seen = stat;
    return -1;
}

static int n3g_entry_read_id(uint32_t *id, uint32_t *last_stat)
{
    *id = 0;
    n3g_ack_data_ready();

    N3G_FMCMD = 0x90;
    if (n3g_entry_wait(0x2, 1, last_stat) != 0)
        return -1;

    N3G_FMANUM = 0;
    N3G_FMADDR0 = 0;
    N3G_FMCTRL1 = 1;
    if (n3g_entry_wait(0x4, 1, last_stat) != 0)
        return -2;

    N3G_FMDNUM = 7;
    N3G_FMADDR2 = 1;
    N3G_FMCTRL1 = 0x1c2;
    if (n3g_entry_wait(0x8, 0, last_stat) != 0)
        return -3;

    N3G_FMADDR2 = 0x100;
    N3G_FMCTRL1 = 0x340;
    *id = N3G_FMFIFO;
    n3g_ack_data_ready();
    return 0;
}

static void n3g_entry_min_setup(void)
{
    PCON(8) = 0x22222222;
    PCON(9) = (PCON(9) & ~0x000f000fu) | 0x00020002u;
    PCON(10) = (PCON(10) & 0xffff0000u) | 0x00002222u;
    PWRCON(0) &= ~((1u << CLOCKGATE_NAND) | (1u << CLOCKGATE_NANDECC));
    N3G_PLATFORM_MISC = (N3G_PLATFORM_MISC & ~0x7u) | N3G_MISC_MODE_VALUE;
    N3G_FMCTRL0 = n3g_fmctrl0_active_bank(0);
}

void nano3g_nand_entry_diag_run(void)
{
    uint32_t id0 = 0;
    uint32_t id1 = 0;
    uint32_t stat0 = 0;
    uint32_t stat1 = 0;
    int rc0;
    int rc1;

    nano3g_nand_entry_diag[1] = 0xeeee0001;
    nano3g_nand_entry_diag[2] = N3G_FMCTRL0;
    nano3g_nand_entry_diag[3] = N3G_FMCSTAT;

    rc0 = n3g_entry_read_id(&id0, &stat0);

    n3g_entry_min_setup();
    rc1 = n3g_entry_read_id(&id1, &stat1);

    nano3g_nand_entry_diag[1] = 0xeeee0002;
    nano3g_nand_entry_diag[2] = id0;
    nano3g_nand_entry_diag[3] = id1;
    nano3g_nand_entry_diag[4] = (uint32_t)rc0;
    nano3g_nand_entry_diag[5] = (uint32_t)rc1;
    nano3g_nand_entry_diag[6] = stat0;
    nano3g_nand_entry_diag[7] = stat1;
    nano3g_nand_entry_diag[8] = N3G_FMCTRL0;
    nano3g_nand_entry_diag[9] = N3G_FMCSTAT;
}

void nano3g_nand_entry_diag_get(uint32_t *out, uint32_t words)
{
    uint32_t limit = words < 10 ? words : 10;

    for (uint32_t i = 0; i < limit; i++)
        out[i] = nano3g_nand_entry_diag[i];
}

void nano3g_nand_stage_diag_run(uint32_t stage)
{
    uint32_t id = 0;
    uint32_t stat = 0;
    int rc;

    if (stage >= 16)
        return;

    n3g_entry_min_setup();
    rc = n3g_entry_read_id(&id, &stat);

    nano3g_nand_stage_diag[1] |= 1u << stage;
    if (rc == 0 && id != 0)
        nano3g_nand_stage_diag[2] |= 1u << stage;
    else if (nano3g_nand_stage_diag[3] == 0xffffffffu)
        nano3g_nand_stage_diag[3] = stage;

    nano3g_nand_stage_diag[4] = id;
    nano3g_nand_stage_diag[5] = (uint32_t)rc;
    nano3g_nand_stage_diag[6] = stat;
    nano3g_nand_stage_diag[7 + stage] = id;
    nano3g_nand_stage_diag[17 + stage] = (uint32_t)rc;
}

void nano3g_nand_stage_diag_get(uint32_t *out, uint32_t words)
{
    uint32_t limit = words < 39 ? words : 39;

    for (uint32_t i = 0; i < limit; i++)
        out[i] = nano3g_nand_stage_diag[i];
}

static int n3g_wait_read_status(void)
{
    N3G_FMCTRL1 = 0x001000e0;
    N3G_FMADDR2 = 1;
    N3G_FMCMD = 0x70;
    if (n3g_wait_capture(0x2, &n3g_read_stat_cmd70) != 0)
        return -1;

    N3G_FMCTRL1 = 0x2a;
    if (n3g_wait_long_capture(0x800000, &n3g_read_stat_wait) != 0)
        return -2;

    N3G_FMCSTAT = 0x800000;
    N3G_FMADDR2 = 0;
    N3G_FMCTRL1 = 0xe0;
    return 0;
}

static int n3g_read_id_exact(uint32_t *id)
{
    n3g_fmc_setup();
    return n3g_read_id_ctrl0(N3G_TEST_BANK, N3G_FMCTRL0, id);
}

static const struct nand_device_info_type *n3g_select_nand_type(void)
{
    uint32_t id = 0;

    if (n3g_selected_nand_type_done)
        return n3g_selected_nand_type;

    n3g_selected_nand_type_done = 1;
    if (n3g_read_id_exact(&id) == 0)
    {
        for (uint32_t i = 0; i < ARRAYLEN(n3g_nand_deviceinfotable); i++)
        {
            if (n3g_nand_deviceinfotable[i].id == id)
            {
                n3g_selected_nand_type = &n3g_nand_deviceinfotable[i];
                return n3g_selected_nand_type;
            }
        }
    }

    n3g_selected_nand_type = &n3g_nand_deviceinfotable[0];
    return n3g_selected_nand_type;
}

static int n3g_read_id_current_state(uint32_t *id)
{
    *id = 0;
    n3g_ack_data_ready();

    N3G_FMCMD = 0x90;
    if (n3g_wait(0x2) != 0)
        return -1;

    N3G_FMANUM = 0;
    N3G_FMADDR0 = 0;
    N3G_FMCTRL1 = 1;
    if (n3g_wait(0x4) != 0)
        return -2;

    N3G_FMDNUM = 7;
    N3G_FMADDR2 = 1;
    N3G_FMCTRL1 = 0x1c2;
    if (n3g_wait_no_clear(0x8) != 0)
        return -3;

    N3G_FMADDR2 = 0x100;
    N3G_FMCTRL1 = 0x340;
    *id = N3G_FMFIFO;
    n3g_ack_data_ready();
    return 0;
}

static int n3g_read_id_gpio_profile(uint32_t bank, uint32_t profile,
                                    uint32_t ctrl0, uint32_t *id)
{
    n3g_fmc_setup_bank_profile(bank, profile);
    N3G_FMCTRL0 = ctrl0;
    n3g_ack_data_ready();
    N3G_FMCMD = 0x90;
    if (n3g_wait(0x2) != 0)
        return -1;

    N3G_FMANUM = 0;
    N3G_FMADDR0 = 0;
    N3G_FMCTRL1 = 1;
    if (n3g_wait(0x4) != 0)
        return -2;

    N3G_FMDNUM = 7;
    N3G_FMADDR2 = 1;
    N3G_FMCTRL1 = 0x1c2;
    if (n3g_wait_no_clear(0x8) != 0)
        return -3;

    N3G_FMADDR2 = 0x100;
    N3G_FMCTRL1 = 0x340;
    *id = N3G_FMFIFO;
    n3g_ack_data_ready();
    return 0;
}

static int n3g_read_id_clock_profile(uint32_t bank, uint32_t profile,
                                     uint32_t *id, uint32_t *clk_before,
                                     uint32_t *clk_during,
                                     uint32_t *clk_after)
{
    uint32_t old_clkcon1 = CLKCON1;
    int rc;

    *id = 0;
    *clk_before = old_clkcon1;

    switch (profile)
    {
    case 0:
        soc_set_system_divs(2, 4, 2);  /* approximate BootROM 108/54/27 */
        break;
    case 1:
        soc_set_system_divs(1, 4, 2);  /* keep CPU fast, halve H/P */
        break;
    case 2:
        soc_set_system_divs(2, 2, 2);  /* halve CPU, current H/P shape */
        break;
    default:
        soc_set_system_divs(1, 2, 4);  /* current H, slower PClk */
        break;
    }

    *clk_during = CLKCON1;
    rc = n3g_read_id_ctrl0(bank, n3g_fmctrl0_active_bank(bank), id);

    CLKCON1 = old_clkcon1;
    while ((CLKCON1 >> 8) != (old_clkcon1 >> 8));
    *clk_after = CLKCON1;
    return rc;
}

static inline uint32_t n3g_cp15_control_read(void)
{
    uint32_t v;

    asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(v));
    return v;
}

static inline void n3g_cp15_control_write(uint32_t v)
{
    asm volatile("mcr p15, 0, %0, c1, c0, 0\n"
                 "nop\n"
                 "nop\n"
                 "nop\n"
                 "nop\n" : : "r"(v) : "memory");
}

static int n3g_read_id_cp15_profile(uint32_t bank, uint32_t *id,
                                    uint32_t *cp_before,
                                    uint32_t *cp_during,
                                    uint32_t *cp_after)
{
    uint32_t old_ctl = n3g_cp15_control_read();
    uint32_t new_ctl = old_ctl & ~((1u << 2) | (1u << 12));
    int rc;

    *id = 0;
    *cp_before = old_ctl;
    commit_discard_dcache_range(id, sizeof(*id));

    n3g_cp15_control_write(new_ctl);
    *cp_during = n3g_cp15_control_read();
    rc = n3g_read_id_ctrl0(bank, n3g_fmctrl0_active_bank(bank), id);
    n3g_cp15_control_write(old_ctl);

    discard_dcache_range(id, sizeof(*id));
    *cp_after = n3g_cp15_control_read();
    return rc;
}

static int n3g_read_id_ctrl0(uint32_t bank, uint32_t ctrl0, uint32_t *id)
{
    n3g_fmc_setup_bank(bank);
    N3G_FMCTRL0 = ctrl0;
    n3g_ack_data_ready();
    N3G_FMCMD = 0x90;
    if (n3g_wait(0x2) != 0)
        return -1;

    N3G_FMANUM = 0;
    N3G_FMADDR0 = 0;
    N3G_FMCTRL1 = 1;
    if (n3g_wait(0x4) != 0)
        return -2;

    N3G_FMDNUM = 7;
    N3G_FMADDR2 = 1;
    N3G_FMCTRL1 = 0x1c2;
    if (n3g_wait_no_clear(0x8) != 0)
        return -3;

    N3G_FMADDR2 = 0x100;
    N3G_FMCTRL1 = 0x340;
    *id = N3G_FMFIFO;
    n3g_ack_data_ready();
    return 0;
}

static int n3g_issue_read(uint32_t page, uint32_t offset)
{
    return n3g_issue_read_bank(N3G_TEST_BANK, page, offset);
}

static int n3g_issue_read_bank(uint32_t bank, uint32_t page, uint32_t offset)
{
    n3g_fmc_setup_bank(bank);
    N3G_FMCTRL1 = 0x0001f0e0;
    n3g_ctrl_before_read0 = N3G_FMCTRL1;
    N3G_FMCTRL0 = n3g_fmctrl0_active_bank(bank);
    N3G_FMCMD = 0x00;
    if (n3g_wait_capture(0x2, &n3g_read_stat_cmd0) != 0)
        return -1;

    N3G_FMANUM = 4;
    N3G_FMADDR0 = (page << 16) | offset;
    N3G_FMADDR1 = (page >> 16) & 0xff;
    n3g_read_anum = N3G_FMANUM;
    n3g_read_addr0 = N3G_FMADDR0;
    n3g_read_addr1 = N3G_FMADDR1;
    N3G_FMCTRL1 = 1;
    n3g_ctrl_before_addr = N3G_FMCTRL1;
    if (n3g_wait_capture(0x4, &n3g_read_stat_addr) != 0)
        return -2;

    n3g_ctrl_before_read30 = N3G_FMCTRL1;
    N3G_FMCMD = 0x30;
    if (n3g_wait_capture(0x2, &n3g_read_stat_cmd30) != 0)
        return -3;

    return 0;
}

static int n3g_internal_page_xfer(uint32_t *buf)
{
    return n3g_internal_page_xfer_bank(N3G_TEST_BANK, buf);
}

static int n3g_internal_page_xfer_bank(uint32_t bank, uint32_t *buf)
{
    uint8_t *dma_scratch = (uint8_t *)buf;

    memset(dma_scratch, 0, 0x800);
    commit_discard_dcache_range(dma_scratch, 0x800);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 3; j++)
            n3g_q78_trace[i][j] = 0;

    N3G_FMUNK840 = 0x7f;
    for (uint32_t i = 0; i < 4; i++)
    {
        uint32_t phase = i & 1;

        N3G_FMCSTAT = 0x08000000;
        N3G_FMDNUM = 0x0f;
        N3G_FMADDR2 = 1u << (phase + 4);
        N3G_FMADDR6 = (i >= 2) ? 0x10 : 0;
        N3G_FMCTRL1 = 0x32;
        if (n3g_wait_no_clear(0x8) != 0)
            return -1;
        N3G_FMCSTAT = 0x8;

        N3G_FMDNUM = 0x1ff;
        N3G_FMADDR2 = 1u << phase;
        N3G_FMADDR6 = 0;
        N3G_FMCTRL1 = 0x22;
        if (n3g_wait_no_clear(0x8) != 0)
            return -2;
        N3G_FMCSTAT = 0x8;

        N3G_FMDATAW0 = (uint32_t)(dma_scratch + i * 0x200);
        N3G_FMDATAW1 = 7;
        n3g_last_dataw0 = N3G_FMDATAW0;
        n3g_last_dataw1 = N3G_FMDATAW1;
        N3G_FMCTRL0 = (N3G_FMCTRL0 & ~0x400u) | 0x01000001u;
        N3G_FMADDR2 = 1u << (phase + 8);
        if (i == 0)
            n3g_capture_q78_trace(0);
        N3G_FMCTRL1 = 0x1a0;
        if (i == 0)
            n3g_capture_q78_trace(1);
        if (n3g_wait_long(0x100000) != 0)
        {
            n3g_last_xfer_stat = N3G_FMCSTAT;
            n3g_last_xfer_ctrl0 = N3G_FMCTRL0;
            n3g_last_xfer_ctrl1 = N3G_FMCTRL1;
            n3g_last_xfer_addr2 = N3G_FMADDR2;
            n3g_last_xfer_dnum = N3G_FMDNUM;
            return -3;
        }
    }

    n3g_extract_stage_spare();

    volatile uint32_t q78_sink = 0;
    for (int i = 0; i < 16; i++)
        q78_sink ^= N3G_FMSTAGECMD;
    n3g_capture_q78_trace(2);
    for (int i = 0; i < 16; i++)
        q78_sink ^= N3G_FMSTAGECMD;
    n3g_capture_q78_trace(3);

    n3g_ctrl_before_copy = N3G_FMCTRL1;
    n3g_stat_before_copy = N3G_FMCSTAT;
    n3g_scratch_addr = N3G_FMC_BASE + 0x60;

    volatile uint32_t *sources[9] =
    {
        (volatile uint32_t *)(N3G_FMC_BASE + 0x60),
        (volatile uint32_t *)(N3G_FMC_BASE + 0x64),
        (volatile uint32_t *)(N3G_FMC_BASE + 0x68),
        (volatile uint32_t *)(N3G_FMC_BASE + 0x6c),
        (volatile uint32_t *)(N3G_FMC_BASE + 0x70),
        (volatile uint32_t *)(N3G_FMC_BASE + 0x74),
        (volatile uint32_t *)(N3G_FMC_BASE + 0x78),
        (volatile uint32_t *)(N3G_FMC_BASE + 0x7c),
        (volatile uint32_t *)(N3G_FMC_BASE + 0x80),
    };

    for (int i = 0; i < 9; i++)
    {
        n3g_source_sweep[i][0] = *sources[i];
        n3g_source_sweep[i][1] = *sources[i];
    }

    n3g_scratch0 = n3g_source_sweep[0][0];
    n3g_scratch1 = n3g_source_sweep[0][1];
    n3g_last_dataw0 = q78_sink;
    for (int i = 0; i < 16; i++)
        n3g_scratch_words[i] = buf[i];
    n3g_last_dataw1 = N3G_FMSTAGE2;
    n3g_last_xfer_stat = N3G_FMCSTAT;
    n3g_last_xfer_ctrl0 = N3G_FMCTRL0;
    n3g_last_xfer_ctrl1 = N3G_FMCTRL1;
    n3g_last_xfer_addr2 = N3G_FMADDR2;
    n3g_last_xfer_dnum = N3G_FMDNUM;
    N3G_FMCTRL0 = n3g_fmctrl0_idle_bank(bank);
    discard_dcache_range(dma_scratch, 0x800);
    return 0;
}

static int n3g_read_page_bootrom_xfer(uint32_t page, uint32_t *buf)
{
    return n3g_read_page_bootrom_xfer_bank(N3G_TEST_BANK, page, buf);
}

static int n3g_read_page_bootrom_xfer_bank(uint32_t bank, uint32_t page,
                                           uint32_t *buf)
{
    int rc = n3g_issue_read_bank(bank, page, 0);
    if (rc != 0)
    {
        n3g_last_page_status_rc = 0x7fffffff;
        n3g_last_page_xfer_rc = 0x7fffffff;
        return rc;
    }

    n3g_last_page_status_rc = n3g_wait_read_status();

    N3G_FMCMD = 0x00;
    n3g_read_post0_rc = n3g_wait_capture(0x2, &n3g_read_stat_post0);

    n3g_last_page_xfer_rc = n3g_internal_page_xfer_bank(bank, buf);
    return n3g_last_page_xfer_rc;
}

static void n3g_scan_iram_for_pattern(uint32_t pattern, uint32_t hits[8])
{
    volatile uint32_t *p = (volatile uint32_t *)0x20000000;
    volatile uint32_t *end = (volatile uint32_t *)0x21000000;
    int n = 0;

    for (int i = 0; i < 8; i++)
        hits[i] = 0;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 16; j++)
            n3g_scan_dumps[i][j] = 0;
    n3g_scan_count = 0;

    if (pattern == 0)
        return;

    while (p < end)
    {
        if (*p == pattern)
        {
            if (n < 8)
                hits[n] = (uint32_t)p;
            if (n < 4)
            {
                volatile uint32_t *dump = p - 8;

                if (dump < (volatile uint32_t *)0x20000000)
                    dump = (volatile uint32_t *)0x20000000;
                if (dump + 16 > end)
                    dump = end - 16;

                for (int i = 0; i < 16; i++)
                    n3g_scan_dumps[n][i] = dump[i];
            }
            n++;
        }
        p++;
    }
    n3g_scan_count = n;
}

static int n3g_read_page_exact(uint32_t page, uint32_t offset,
                               uint32_t *buf, uint32_t words)
{
    return n3g_read_page_exact_bank(N3G_TEST_BANK, page, offset, buf, words);
}

static int n3g_read_page_exact_bank(uint32_t bank, uint32_t page,
                                    uint32_t offset, uint32_t *buf,
                                    uint32_t words)
{
    if (offset == 0 && words == 0x200)
        return n3g_read_page_bootrom_xfer_bank(bank, page, buf);

    if (offset == 0x800 && words <= 0x10)
    {
        uint32_t spare[0x10];
        int rc = n3g_read_page_bootrom_xfer_bank(bank, page, nano3g_pagebuf);
        if (rc != 0)
            return rc;

        /*
         * Stage spare can be stale/zero on a standalone OOB-only diagnostic
         * read immediately after controller setup. A second identical page
         * transfer makes the stage registers match the spare data returned by
         * the normal body+spare nand_read_page() path.
         */
        rc = n3g_read_page_bootrom_xfer_bank(bank, page, nano3g_pagebuf);
        if (rc != 0)
            return rc;

        n3g_copy_stage_spare(spare);
        memcpy(buf, spare, words * sizeof(*buf));
        return 0;
    }

    int rc = n3g_issue_read_bank(bank, page, offset);
    if (rc != 0)
        return rc;

    if (n3g_read_fifo(buf, words) != 0)
        return -4;

    return 0;
}

static void n3g_copy_stage_spare(uint32_t *spare)
{
    /*
     * Nano 3G BootROM page reads expose the Whimory spare metadata through
     * the FMC stage registers after the data transfer, not through NAND
     * column 0x800. Only the first three words are meaningful here.
     */
    spare[0] = n3g_stage_spare[0];
    spare[1] = n3g_stage_spare[1];
    spare[2] = n3g_stage_spare[2];
    for (uint32_t i = 3; i < 0x10; i++)
        spare[i] = 0xffffffffu;
}

static void n3g_extract_stage_spare(void)
{
    unsigned timeout;

    N3G_FMSTAGECMD = 0x5140;
    N3G_FMSTAGECTRL = 2;

    timeout = USEC_TIMER + 50000;
    while (N3G_FMSTAGECTRL & 2)
    {
        if (TIME_AFTER(USEC_TIMER, timeout))
        {
            n3g_stage_spare[0] = 0;
            n3g_stage_spare[1] = 0;
            n3g_stage_spare[2] = 0;
            return;
        }
    }

    n3g_stage_spare[0] = N3G_FMSTAGE0;
    n3g_stage_spare[1] = N3G_FMSTAGE1;
    n3g_stage_spare[2] = N3G_FMSTAGE2;
}

static bool n3g_page_empty(const uint32_t *data, const uint32_t *spare)
{
    if (data != NULL)
        for (int i = 0; i < 0x200; i++)
            if (data[i] != 0xffffffffu)
                return false;

    if (spare != NULL)
        for (int i = 0; i < 0x10; i++)
            if (spare[i] != 0xffffffffu)
                return false;

    return true;
}

static int n3g_ensure_ftl(void)
{
    if (!n3g_ftl_init_done)
    {
        n3g_ftl_init_rc = (int32_t)ftl_init();
        n3g_ftl_init_done = 1;
    }

    return n3g_ftl_init_rc;
}

void nano3g_nand_direct_diag(struct nano3g_nand_direct_diag *diag)
{
    uint32_t csum1 = 0xaabbccdd;
    uint32_t csum2 = 0xaabbccdd;

    memset(diag, 0, sizeof(*diag));
    memset(nano3g_pagebuf, 0, sizeof(nano3g_pagebuf));
    memset(nano3g_sparebuf, 0, sizeof(nano3g_sparebuf));
    memset(nano3g_sector0buf, 0, sizeof(nano3g_sector0buf));

    n3g_fmc_setup();
    diag->stat0 = N3G_FMCSTAT;
    diag->reset_rc = n3g_reset_bank(&diag->stat_reset);
    diag->page_rc = n3g_read_page_exact(0x80, 0, nano3g_pagebuf, 0x200);
    for (int i = 0; i < 16; i++)
        n3g_page_imm_words[i] = nano3g_pagebuf[i];
    diag->spare_rc = n3g_read_page_exact(0x80, 0x800, nano3g_sparebuf, 0x10);

    static const uint32_t pages[N3G_PAGE_SCAN_COUNT] =
        { 0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };

    diag->first_non_ff_page = 0xffffffffu;
    diag->first_mbr_page = 0xffffffffu;
    for (int p = 0; p < N3G_PAGE_SCAN_COUNT; p++)
    {
        memset(nano3g_pagebuf, 0, sizeof(nano3g_pagebuf));
        diag->dump_pages[p] = pages[p];
        diag->dump_rc[p] = n3g_read_page_exact(pages[p], 0,
                                               nano3g_pagebuf, 0x200);
        diag->dump_sig510[p] = n3g_mbr_sig510(nano3g_pagebuf);
        diag->dump_non_ff[p] = n3g_page_has_non_ff(nano3g_pagebuf) ? 1 : 0;
        if (diag->dump_non_ff[p] && diag->first_non_ff_page == 0xffffffffu)
            diag->first_non_ff_page = pages[p];
        if ((diag->dump_sig510[p] & 0xffff0000u) == 0xaa550000u &&
            diag->first_mbr_page == 0xffffffffu)
            diag->first_mbr_page = pages[p];
        for (int i = 0; i < 16; i++)
            diag->dump_words[p][i] = nano3g_pagebuf[i];
    }

    diag->page_rc = n3g_read_page_exact(0x80, 0, nano3g_pagebuf, 0x200);

    diag->sector0_rc = -1;
    diag->sector0_init_rc = -1;
    diag->sector0_sig510 = 0;
    diag->sector0_non_ff = 0;

    diag->stat1 = N3G_FMCSTAT;
    diag->id_rc = n3g_read_id_exact(&diag->id);

    diag->page0 = nano3g_pagebuf[0];
    diag->page1 = nano3g_pagebuf[1];
    diag->spare0 = nano3g_sparebuf[0];
    diag->spare1 = nano3g_sparebuf[1];
    diag->csum1 = nano3g_pagebuf[0x1fe];
    diag->csum2 = nano3g_pagebuf[0x1ff];
    diag->pcon8 = PCON(8);
    diag->pcon9 = PCON(9);
    diag->pcon10 = PCON(10);
    diag->gpiocmd = GPIOCMD;
    diag->pwr0 = PWRCON(0);
    diag->pwr1 = PWRCON(1);
    diag->clk1 = CLKCON1;
    diag->page_status_rc = n3g_last_page_status_rc;
    diag->page_xfer_rc = n3g_last_page_xfer_rc;
    diag->xfer_stat = n3g_last_xfer_stat;
    diag->xfer_ctrl0 = n3g_last_xfer_ctrl0;
    diag->xfer_ctrl1 = n3g_last_xfer_ctrl1;
    diag->xfer_addr2 = n3g_last_xfer_addr2;
    diag->xfer_dnum = n3g_last_xfer_dnum;
    diag->read_stat_cmd0 = n3g_read_stat_cmd0;
    diag->read_stat_addr = n3g_read_stat_addr;
    diag->read_stat_cmd30 = n3g_read_stat_cmd30;
    diag->read_stat_cmd70 = n3g_read_stat_cmd70;
    diag->read_stat_wait = n3g_read_stat_wait;
    diag->read_stat_post0 = n3g_read_stat_post0;
    diag->read_post0_rc = n3g_read_post0_rc;
    diag->scratch_addr = n3g_scratch_addr;
    diag->scratch0 = n3g_scratch0;
    diag->scratch1 = n3g_scratch1;
    diag->test_bank = N3G_TEST_BANK;
    diag->test_ctrl0 = n3g_fmctrl0_active();
    diag->misc_before = n3g_misc_before;
    diag->misc_after = n3g_misc_after;
    diag->misc_force = N3G_MISC_MODE_FORCE;
    diag->misc_mode = N3G_MISC_MODE_VALUE;
    for (int i = 0; i < 16; i++)
    {
        diag->scratch_words[i] = n3g_scratch_words[i];
        diag->page_imm_words[i] = n3g_page_imm_words[i];
    }
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 16; j++)
            diag->scan_dumps[i][j] = n3g_scan_dumps[i][j];
    diag->dataw0 = n3g_last_dataw0;
    diag->dataw1 = n3g_last_dataw1;
    diag->read_addr0 = n3g_read_addr0;
    diag->read_addr1 = n3g_read_addr1;
    diag->read_anum = n3g_read_anum;
    diag->ctrl_before_read0 = n3g_ctrl_before_read0;
    diag->ctrl_before_addr = n3g_ctrl_before_addr;
    diag->ctrl_before_read30 = n3g_ctrl_before_read30;
    diag->ctrl_before_copy = n3g_ctrl_before_copy;
    diag->stat_before_copy = n3g_stat_before_copy;
    for (int i = 0; i < 9; i++)
    {
        diag->source_sweep[i][0] = n3g_source_sweep[i][0];
        diag->source_sweep[i][1] = n3g_source_sweep[i][1];
    }
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 3; j++)
            diag->q78_trace[i][j] = n3g_q78_trace[i][j];

    for (int i = 0; i < 0x1fe; i++)
    {
        csum1 += nano3g_pagebuf[i];
        csum2 ^= nano3g_pagebuf[i];
    }

    diag->csum1_calc = csum1;
    diag->csum2_calc = csum2;
}

int nand_init(void)
{
    return n3g_ensure_ftl();
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

    if (incount == 0)
        return 0;

    if (n3g_ensure_ftl() != 0)
    {
        memset(inbuf, 0, (size_t)incount * SECTOR_SIZE);
        return -1;
    }

    return (int)ftl_read((uint32_t)start, (uint32_t)incount, inbuf);
}

uint32_t nand_read_page(uint32_t bank, uint32_t page, void* databuffer,
                        void* sparebuffer, uint32_t doecc,
                        uint32_t checkempty)
{
    uint32_t rc = 0;
    uint32_t local_data[0x200] STORAGE_ALIGN_ATTR;
    uint32_t local_spare[0x10] STORAGE_ALIGN_ATTR;
    uint32_t *data = databuffer;
    uint32_t *spare = sparebuffer;
    (void)doecc;

    if (bank >= N3G_NAND_BANKS)
        return 1;

    if (databuffer != NULL && ((uint32_t)databuffer & 0xf))
        data = local_data;
    if (data == NULL)
        data = local_data;
    if (sparebuffer != NULL && ((uint32_t)sparebuffer & 0xf))
        spare = local_spare;

    if (n3g_read_page_bootrom_xfer_bank(bank, page, data) != 0)
        return 1;
    if (sparebuffer != NULL)
        n3g_copy_stage_spare(spare);

    if (databuffer != NULL && data != databuffer)
        memcpy(databuffer, data, 0x800);
    if (sparebuffer != NULL && spare != sparebuffer)
        memcpy(sparebuffer, spare, 0x40);

    if (checkempty && n3g_page_empty(databuffer, sparebuffer))
        rc |= 2;

    return rc;
}

uint32_t nand_read_page_fast(uint32_t page, void* databuffer,
                             void* sparebuffer, uint32_t doecc,
                             uint32_t checkempty)
{
    uint32_t rc = 0;

    for (uint32_t bank = 0; bank < N3G_NAND_BANKS; bank++)
    {
        void *data = databuffer ? (uint8_t *)databuffer + 0x800 * bank : NULL;
        void *spare = sparebuffer ? (uint8_t *)sparebuffer + 0x40 * bank : NULL;
        uint32_t ret = nand_read_page(bank, page, data, spare,
                                      doecc, checkempty);
        if (ret & 1)
            rc |= 1u << (bank << 2);
        if (ret & 2)
            rc |= 2u << (bank << 2);
        if (ret & 0x10)
            rc |= 4u << (bank << 2);
        if (ret & 0x100)
            rc |= 8u << (bank << 2);
    }

    return rc;
}

const struct nand_device_info_type* nand_get_device_type(uint32_t bank)
{
    if (bank >= N3G_NAND_BANKS)
        return NULL;

    return n3g_select_nand_type();
}

uint32_t nand_reset(uint32_t bank)
{
    uint32_t stat;

    if (bank >= N3G_NAND_BANKS)
        return 1;

    if (n3g_rom_init_bank(bank) == 0)
        return 0;

    return n3g_reset_bank_idx(bank, &stat) == 0 ? 0 : 1;
}

int nand_device_init(void)
{
#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    N3G_NAND_DEBUG("N3G_NAND_DEVICE_INIT_SKIP");
    return 0;
#else
    for (uint32_t bank = 0; bank < N3G_NAND_BANKS; bank++)
        n3g_rom_init_bank(bank);

    return 0;
#endif
}

uint32_t nand_write_page(uint32_t bank, uint32_t page, void* databuffer,
                         void* sparebuffer, uint32_t doecc)
{
    (void)bank;
    (void)page;
    (void)databuffer;
    (void)sparebuffer;
    (void)doecc;
    return 1;
}

uint32_t nand_write_page_start(uint32_t bank, uint32_t page, void* databuffer,
                               void* sparebuffer, uint32_t doecc)
{
    return nand_write_page(bank, page, databuffer, sparebuffer, doecc);
}

uint32_t nand_write_page_collect(uint32_t bank)
{
    (void)bank;
    return 1;
}

uint32_t nand_block_erase(uint32_t bank, uint32_t page)
{
    (void)bank;
    (void)page;
    return 1;
}

void nand_set_active(void)
{
}

long nand_last_activity(void)
{
    return 0;
}

void nand_power_up(void)
{
    n3g_fmc_setup();
}

void nand_power_down(void)
{
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
#if defined(IPOD_NANO3G) && defined(BOOTLOADER)
    if (n3g_ensure_ftl() == 0)
    {
        const struct nand_device_info_type *type = n3g_select_nand_type();
        info->num_sectors = (sector_t)type->userblocks
                           * type->pagesperblock * N3G_NAND_BANKS;
    }
#endif
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
