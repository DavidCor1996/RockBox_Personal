/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 by Dave Chapman
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"

#include "inttypes.h"
#include "cpu.h"
#include "system.h"
#include "lcd.h"
#include "../kernel-internal.h"
#include "file_internal.h"
#include "storage.h"
#include "nand.h"
#include "disk.h"
#include "font.h"
#include "backlight.h"
#include "backlight-target.h"
#include "button.h"
#include "panic.h"
#include "power.h"
#include "file.h"
#include "common.h"
#include "rb-loader.h"
#include "loader_strerror.h"
#include "version.h"
#include "powermgmt.h"
#include "usb.h"
#ifdef HAVE_SERIAL
#include "serial.h"
#endif

#include "s5l87xx.h"
#include "clocking-s5l8702.h"
#include "spi-s5l8702.h"
#include "i2c-s5l8702.h"
#include "gpio-s5l8702.h"
#include "pmu-target.h"
#if defined(IPOD_6G) || defined(IPOD_NANO3G)
#include "norboot-target.h"
#endif
#ifdef IPOD_NANO3G
#include "bringup-nano3g.h"
#endif

#ifdef IPOD_NANO3G
#define N3G_PAGE_SCAN_COUNT 12

static volatile uint32_t n3g_probe_saved_cpsr;
static volatile uint32_t n3g_probe_saved_sp;

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

extern void nano3g_nand_direct_diag(struct nano3g_nand_direct_diag *diag);
extern void nano3g_nand_entry_diag_run(void);
extern void nano3g_nand_stage_diag_run(uint32_t stage);

static void n3g_storage_direct_lba_diag(void)
{
    static unsigned char buf[0x800] STORAGE_ALIGN_ATTR;
    int rc;
    uint16_t sig;
    uint32_t start;
    uint32_t size;
    uint32_t bps;
    uint32_t rsvd;
    uint32_t total16;
    uint32_t total32;
    uint32_t fatsz16;
    uint32_t fatsz32;
    uint32_t fsinfo;
    uint32_t scale;
    const unsigned char *fs;

    memset(buf, 0, sizeof(buf));
    rc = storage_read_sectors(IF_MD(0,) 0, 1, buf);
    sig = (uint16_t)buf[0x1fe] | ((uint16_t)buf[0x1ff] << 8);
    start = (uint32_t)buf[0x1be + 8]
          | ((uint32_t)buf[0x1be + 9] << 8)
          | ((uint32_t)buf[0x1be + 10] << 16)
          | ((uint32_t)buf[0x1be + 11] << 24);
    size = (uint32_t)buf[0x1be + 12]
         | ((uint32_t)buf[0x1be + 13] << 8)
         | ((uint32_t)buf[0x1be + 14] << 16)
         | ((uint32_t)buf[0x1be + 15] << 24);
    printf("N3G_DIRECT_LBA0 rc=%d sig=%04x p0t=%02x p0st=%08lx p0sz=%08lx",
           rc, sig, buf[0x1be + 4], (unsigned long)start,
           (unsigned long)size);

    memset(buf, 0, sizeof(buf));
    rc = storage_read_sectors(IF_MD(0,) start, 1, buf);
    sig = (uint16_t)buf[0x1fe] | ((uint16_t)buf[0x1ff] << 8);
    bps = (uint32_t)buf[0x0b] | ((uint32_t)buf[0x0c] << 8);
    rsvd = (uint32_t)buf[0x0e] | ((uint32_t)buf[0x0f] << 8);
    total16 = (uint32_t)buf[0x13] | ((uint32_t)buf[0x14] << 8);
    total32 = (uint32_t)buf[0x20]
            | ((uint32_t)buf[0x21] << 8)
            | ((uint32_t)buf[0x22] << 16)
            | ((uint32_t)buf[0x23] << 24);
    fatsz16 = (uint32_t)buf[0x16] | ((uint32_t)buf[0x17] << 8);
    fatsz32 = (uint32_t)buf[0x24]
            | ((uint32_t)buf[0x25] << 8)
            | ((uint32_t)buf[0x26] << 16)
            | ((uint32_t)buf[0x27] << 24);
    fs = (buf[0x52] || buf[0x53] || buf[0x54]) ? &buf[0x52] : &buf[0x36];
    printf("N3G_DIRECT_BOOT rc=%d sig=%04x bps=%lu spc=%u rs=%lu nf=%u fz=%lu ts=%lu fatstr=%c%c%c%c%c%c%c%c",
           rc, sig, (unsigned long)bps, buf[0x0d], (unsigned long)rsvd,
           buf[0x10], (unsigned long)(fatsz16 != 0 ? fatsz16 : fatsz32),
           (unsigned long)(total16 != 0 ? total16 : total32),
           fs[0], fs[1], fs[2], fs[3], fs[4], fs[5], fs[6], fs[7]);

    scale = (bps >= 512 && (bps % 512) == 0) ? bps / 512 : 1;
    fsinfo = (uint32_t)buf[0x30] | ((uint32_t)buf[0x31] << 8);
    memset(buf, 0, sizeof(buf));
    rc = storage_read_sectors(IF_MD(0,) start + fsinfo * scale, 1, buf);
    printf("N3G_DIRECT_FSINFO rc=%d lba=%08lx sig=%08lx free=%08lx next=%08lx",
           rc, (unsigned long)(start + fsinfo * scale),
           (unsigned long)((uint32_t)buf[0]
               | ((uint32_t)buf[1] << 8)
               | ((uint32_t)buf[2] << 16)
               | ((uint32_t)buf[3] << 24)),
           (unsigned long)((uint32_t)buf[0x1e8]
               | ((uint32_t)buf[0x1e9] << 8)
               | ((uint32_t)buf[0x1ea] << 16)
               | ((uint32_t)buf[0x1eb] << 24)),
           (unsigned long)((uint32_t)buf[0x1ec]
               | ((uint32_t)buf[0x1ed] << 8)
               | ((uint32_t)buf[0x1ee] << 16)
               | ((uint32_t)buf[0x1ef] << 24)));
}
#endif


#define ERR_RB      0
#define ERR_OF      1
#define ERR_STORAGE 2
#define ERR_LBA28   3

/* Safety measure - maximum allowed firmware image size.
   The largest known current (October 2009) firmware is about 6.2MB so
   we set this to 8MB.
*/
#define MAX_LOADSIZE (8*1024*1024)

#define LCD_RBYELLOW    LCD_RGBPACK(255,192,0)
#define LCD_REDORANGE   LCD_RGBPACK(255,70,0)
#define LCD_GREEN       LCD_RGBPACK(0,255,0)

extern void bss_init(void);
extern uint32_t _movestart;
extern uint32_t start_loc;

extern int line;

#ifndef S5L87XX_DEVELOPMENT_BOOTLOADER
#ifdef HAVE_BOOTLOADER_USB_MODE
static void usb_mode(void)
{
    int button;

    verbose = true;

    printf("Entering USB mode...");

    powermgmt_init();

    /* The code will ask for the maximum possible value */
    usb_charging_enable(USB_CHARGING_ENABLE);

    usb_init();
    usb_start_monitoring();

    /* Wait until USB is plugged */
    while (usb_detect() != USB_INSERTED)
    {
        printf("Plug USB cable");
        line--;
        sleep(HZ/10);
    }

    while(1)
    {
        button = button_get_w_tmo(HZ/10);

        if (button == SYS_USB_CONNECTED)
            break; /* Hit */

        if (usb_detect() == USB_EXTRACTED)
            break; /* Cable pulled */

        /* Wait for threads to connect or cable is pulled */
        printf("USB: Connecting...");
        line--;
    }

    if (button == SYS_USB_CONNECTED)
    {
        /* Got the message - wait for disconnect */
        printf("Bootloader USB mode");

        /* Ack the SYS_USB_CONNECTED polled from the button queue */
        usb_acknowledge(SYS_USB_CONNECTED_ACK, button_get_data());

        while(1)
        {
            button = button_get_w_tmo(HZ/2);
            if (button == SYS_USB_DISCONNECTED)
                break;
        }
    }

    /* We don't want the HDD to spin up if the USB is attached again */
    usb_close();
    printf("USB mode exit     ");
}
#endif /* HAVE_BOOTLOADER_USB_MODE */

void fatal_error(int err)
{
    verbose = true;

    /* System font is 6 pixels wide */
    line++;
    switch (err)
    {
        case ERR_RB:
#ifdef HAVE_BOOTLOADER_USB_MODE
            usb_mode();
            printf("Hold MENU+SELECT to reboot");
            break;
#endif
        case ERR_STORAGE:
            printf("Hold MENU+SELECT to reboot");
            printf("then SELECT+PLAY for disk mode");
            break;
        case ERR_OF:
            printf("Hold MENU+SELECT to reboot");
            printf("and enter Rockbox firmware");
            break;
        case ERR_LBA28:
            printf("Hold MENU+SELECT to reboot");
            printf("and LEFT if you are REALLY sure");
            break;
    }

#if (CONFIG_STORAGE & STORAGE_ATA)
    if (ide_powered())
        ata_sleepnow(); /* Immediately spindown the disk. */
#endif

    line++;
    lcd_set_foreground(LCD_REDORANGE);
    while (1) {
        lcd_puts(0, line, button_hold() ? "Hold switch on!"
                                        : "               ");
        lcd_update();
    }
}

#if (CONFIG_STORAGE & STORAGE_ATA)
extern unsigned short battery_level_disksafe;
static void battery_trap(void)
{
    int vbat, old_verb;
    int th = 50;

    old_verb = verbose;
    verbose = true;

    usb_charging_maxcurrent_change(100);

    while (1)
    {
        vbat = _battery_voltage();

        /*  Two reasons to use this threshold (may require adjustments):
         *  - when USB (or wall adaptor) is plugged/unplugged, Vbat readings
         *    differ as much as more than 200 mV when charge current is at
         *    maximum (~340 mA).
         *  - RB uses some sort of average/compensation for battery voltage
         *    measurements, battery icon blinks at battery_level_disksafe,
         *    when the HDD is used heavily (large database) the level drops
         *    to battery_level_shutoff quickly.
         */
        if (vbat >= battery_level_disksafe + th)
            break;
        th = 200;

        if (power_input_status() != POWER_INPUT_NONE) {
            lcd_set_foreground(LCD_RBYELLOW);
            printf("Low battery: %d mV, charging...     ", vbat);
            sleep(HZ*3);
        }
        else {
            /* Wait for the user to insert a charger */
            int tmo = 10;
            lcd_set_foreground(LCD_REDORANGE);
            while (1) {
                vbat = _battery_voltage();
                printf("Low battery: %d mV, power off in %d ", vbat, tmo);
                if (!tmo--) {
                    /* Raise Vsysok (hyst=0.02*Vsysok) to avoid PMU
                       standby<->active looping */
                    if (vbat < 3200)
                        pmu_write(PCF5063X_REG_SVMCTL, 0xA /*3200mV*/);
                    power_off();
                }
                sleep(HZ*1);
                if (power_input_status() != POWER_INPUT_NONE)
                    break;
                line--;
            }
        }
        line--;
    }

    verbose = old_verb;
    lcd_set_foreground(LCD_WHITE);
    printf("Battery status ok: %d mV            ", vbat);
}
#endif /* CONFIG_STORAGE & STORAGE_ATA */
#endif /* S5L87XX_DEVELOPMENT_BOOTLOADER */

static int launch_onb(int clkdiv)
{
#if defined(IPOD_6G) || defined(IPOD_NANO3G)
    /* SPI clock = PClk/(clkdiv+1) */
    spi_clkdiv(SPI_PORT, clkdiv);

    /* Actually IRAM1_ORIG contains current RB bootloader IM3 header,
       it will be replaced by ONB IM3 header, so this function must
       be called once!!! */
    struct Im3Info *hinfo = (struct Im3Info*)IRAM1_ORIG;

    /* Loads ONB in IRAM0, exception vector table is destroyed !!! */
    int rc = im3_read(
            NORBOOT_OFF + im3_nor_sz(hinfo), hinfo, (void*)IRAM0_ORIG);

    if (rc != 0) {
        /* Restore exception vector table */
        memcpy((void*)IRAM0_ORIG, &_movestart, 4*(&start_loc-&_movestart));
        commit_discard_idcache();
        return rc;
    }

    /* Disable all external interrupts */
    eint_init();

    commit_discard_idcache();

    /* Branch to start of IRAM */
    asm volatile("mov pc, %0"::"r"(IRAM0_ORIG));
    while(1);
#elif defined(IPOD_NANO4G)
    (void) clkdiv;

    lcd_set_foreground(LCD_REDORANGE);
    printf("Not implemented");

    line++;
    lcd_set_foreground(LCD_RBYELLOW);
    printf("Press SELECT to continue");
    while (button_status() != BUTTON_SELECT)
        sleep(HZ/100);

    return 0;
#endif
}

/* Launch OF when kernel mode is running */
static int kernel_launch_onb(void)
{
    disable_irq();
    int rc = launch_onb(3); /* 54/4 = 13.5 MHz. */
    enable_irq();
    return rc;
}

/*  The boot sequence is executed on power-on or reset. After power-up
 *  the device could come from a state of hibernation, OF hibernates
 *  the iPod after an inactive period of ~30 minutes, on this state the
 *  SDRAM is in self-refresh mode.
 *
 *  t0 = 0
 *     S5L8702 BOOTROM loads an IM3 image located at NOR:
 *     - IM3 header (first 0x800 bytes) is loaded at IRAM1_ORIG
 *     - IM3 body (decrypted RB bootloader) is loaded at IRAM0_ORIG
 *     The time needed to load the RB bootloader (~100 Kb) is estimated
 *     on 200~250 ms. Once executed, RB booloader moves itself from
 *     IRAM0_ORIG to IRAM1_ORIG+0x800, preserving current IM3 header
 *     that contains the NOR offset where the ONB (original NOR boot),
 *     is located (see dualboot.c for details).
 *
 *  t1 = ~250 ms.
 *     If the PMU is hibernated, decrypted ONB (size 128Kb) is loaded
 *       and executed, it takes ~120 ms. Then the ONB restores the
 *       iPod to the state prior to hibernation.
 *     If not, initialize system and RB kernel, wait for t2.
 *
 *  t2 = ~650 ms.
 *     Check user button selection.
 *     If OF, diagmode, or diskmode is selected then launch ONB.
 *     If not, wait for LCD initialization.
 *
 *  t3 = ~700,~900 ms. (lcd_type_01,lcd_type_23)
 *     LCD is initialized, baclight ON.
 *     Wait for HDD spin-up.
 *
 *  t4 = ~2600,~2800 ms.
 *     HDD is ready.
 *     If hold switch is locked, then load and launch ONB.
 *     If not, load rockbox.ipod file from HDD.
 *
 *  t5 = ~2800,~3000 ms.
 *     rockbox.ipod is executed.
 */

#ifdef S5L87XX_DEVELOPMENT_BOOTLOADER
#include "piezo.h"
#include "lcd-s5l8702.h"
extern int lcd_type;

static uint16_t alive[] = { 500,100,0, 0 };
static uint16_t alivelcd[] = { 2000,200,0, 0 };

#ifdef HAVE_LCD_SLEEP
static void sleep_test(void)
{
    int sleep_tmo = 5;
    int awake_tmo = 3;

    lcd_clear_display();
    lcd_set_foreground(LCD_WHITE);
    line = 0;

    printf("Entering LCD sleep mode in %d seconds,", sleep_tmo);
    printf("during sleep mode you will see a white");
    printf("screen for about %d seconds.", awake_tmo);
    while (sleep_tmo--) {
        printf("Sleep in %d...", sleep_tmo);
        sleep(HZ*1);
    }
    lcd_sleep();
    sleep(HZ*awake_tmo);
    lcd_awake();

    line++;
    printf("Awake!");

    line++;
    lcd_set_foreground(LCD_RBYELLOW);
    printf("Press SELECT to continue");
    while (button_status() != BUTTON_SELECT)
        sleep(HZ/100);
}
#endif

static void pmu_info(void)
{
    int loop = 0;

    lcd_clear_display();
    lcd_update();
    while (button_status() != BUTTON_NONE);

    while (1)
    {
        lcd_set_foreground(LCD_WHITE);
        lcd_clear_display();
        line = 0;
        printf("loop: %d", loop++);

        for (int i = 0; i < 128; i += 8)
        {
            unsigned char buf[8];

#if defined(IPOD_NANO3G) && 0
            if (i == 0) {
                static int flip = 0;
                if (flip) {
                    pmu_write(6, 0xff);
                    pmu_write(7, 0xff);
                }
                else {
                    pmu_write(6, 0xe7);
                    pmu_write(7, 0xfe);
                }
                flip ^= 1;
            }
#elif defined(IPOD_NANO4G)
            if (i == 120)
                for (int j = 0; j < 8; j++)
                    pmu_write(i+j, j);
#endif
            for (int j = 0; j < 8; j++)
                buf[j] = pmu_read(i+j);

            printf(" %2x: %2x %2x %2x %2x %2x %2x %2x %2x", i,
                    buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);
        }
        line++;
        printf("USB: %s    ", (usb_detect() == USB_INSERTED) ? "inserted" : "not inserted");
#if CONFIG_CHARGING
        printf("Firewire: %s    ", pmu_firewire_present() ? "inserted" : "not inserted");
#endif
#ifdef IPOD_ACCESSORY_PROTOCOL
        printf("Accessory: %s    ", pmu_accessory_present() ? "inserted" : "not inserted");
#endif
        printf("Hold Switch: %s  ", pmu_holdswitch_locked() ? "locked" : "unlocked");
        line++;
        lcd_set_foreground(LCD_RBYELLOW);
        printf("Press SELECT to continue");
        if (button_status() == BUTTON_SELECT)
            break;
        sleep(HZ/2);
    }
}

static void gpio_info(void)
{
    int loop = 0;

    lcd_clear_display();

    while (1)
    {
        lcd_set_foreground(LCD_WHITE);
        lcd_clear_display();
        line = 0;
        printf("loop: %d", loop++);
        for (int i = 0; i < GPIO_N_GROUPS; i ++)
        {
            printf(" %x: %8x %2x %4x %2x %2x", i,
                    PCON(i), PDAT(i), PUNA(i), PUNB(i), PUNC(i));
        }
        line++;
        lcd_set_foreground(LCD_RBYELLOW);
        printf("Press SELECT to continue");
        if (button_status() == BUTTON_SELECT)
            break;
        sleep(HZ/5);
    }
}

static void run_of(void)
{
    int tmo = 5;
    lcd_clear_display();
    lcd_set_foreground(LCD_WHITE);
    line = 0;
    while (tmo--) {
        printf("Booting OF in %d...", tmo);
        sleep(HZ*1);
    }

    int rc = kernel_launch_onb();
    printf("Load OF error: %d", rc);
    sleep(HZ*10);
}

#if defined(IPOD_6G) || defined(IPOD_NANO3G)
static void print_syscfg(void)
{
    lcd_clear_display();
    lcd_set_foreground(LCD_WHITE);
    line = 0;

    struct SysCfg syscfg;
    const ssize_t result = syscfg_read(&syscfg);

    if (result == -1) {
        printf("SCfg magic not found. NOR flash is corrupted.");
        goto end;
    }

    printf("Total size: %lu bytes, %lu entries", syscfg.header.size, syscfg.header.num_entries);

    if (result > 0) {
        printf("Wrong size: expected %ld, got %lu", result, syscfg.header.size);
    }

    if (syscfg.header.num_entries > SYSCFG_MAX_ENTRIES) {
        printf("Too many entries, showing only first %u", SYSCFG_MAX_ENTRIES);
    }

    const size_t syscfg_num_entries = MIN(syscfg.header.num_entries, SYSCFG_MAX_ENTRIES);

    for (size_t i = 0; i < syscfg_num_entries; i++) {
        const struct SysCfgEntry* entry = &syscfg.entries[i];
        const char* tag = (char *)&entry->tag;
        const uint32_t* data32 = (uint32_t *)entry->data;

        switch (entry->tag) {
        case SYSCFG_TAG_SRNM:
            printf("Serial number (SrNm): %s", entry->data);
            break;
        case SYSCFG_TAG_FWID:
            printf("Firmware ID (FwId): %07lX", data32[1] & 0x0FFFFFFF);
            break;
        case SYSCFG_TAG_HWID:
            printf("Hardware ID (HwId): %08lX", data32[0]);
            break;
        case SYSCFG_TAG_HWVR:
            printf("Hardware version (HwVr): %06lX", data32[1]);
            break;
        case SYSCFG_TAG_CODC:
            printf("Codec (Codc): %s", entry->data);
            break;
        case SYSCFG_TAG_SWVR:
            printf("Software version (SwVr): %s", entry->data);
            break;
        case SYSCFG_TAG_MLBN:
            printf("Logic board serial number (MLBN): %s", entry->data);
            break;
        case SYSCFG_TAG_MODN:
            printf("Model number (Mod#): %s", entry->data);
            break;
        case SYSCFG_TAG_REGN:
            printf("Sales region (Regn): %08lX %08lX", data32[0], data32[1]);
            break;
        default:
            printf("%c%c%c%c: %08lX %08lX %08lX %08lX",
                tag[3], tag[2], tag[1], tag[0],
                data32[0], data32[1], data32[2], data32[3]
            );
            break;
        }
    }

end:
    line++;
    lcd_set_foreground(LCD_RBYELLOW);
    printf("Press SELECT to continue");
    while (button_status() != BUTTON_SELECT)
        sleep(HZ/100);
}

static void print_bootloader_hash(void)
{
    lcd_clear_display();
    lcd_set_foreground(LCD_WHITE);
    line = 0;

    struct Im3Info hinfo;
    int rc = im3_read(NORBOOT_OFF, &hinfo, NULL);

    if (rc != 0) {
        printf("Error loading the primary bootloader: %d", rc);
        goto end;
    }

    unsigned char primary_hash[SIGN_SZ];

    memcpy(primary_hash, hinfo.u.enc12.data_sign, SIGN_SZ);
    hwkeyaes(HWKEYAES_DECRYPT, HWKEYAES_UKEY, primary_hash, SIGN_SZ);

    unsigned bl_nor_sz = im3_nor_sz(&hinfo);
    rc = im3_read(NORBOOT_OFF + bl_nor_sz, &hinfo, NULL);

    if (rc == 0) {
        // Rockbox bootloader is installed as primary
        // Stock bootloader is backed up
        unsigned char backup_hash[SIGN_SZ];
        memcpy(backup_hash, hinfo.u.enc12.data_sign, SIGN_SZ);
        hwkeyaes(HWKEYAES_DECRYPT, HWKEYAES_UKEY, backup_hash, SIGN_SZ);

        printf("Rockbox bootloader hash:");

        for (int i = 0; i < SIGN_SZ; i++) {
            lcd_putsf(i * 2, line, "%02X", primary_hash[i]);
        }

        line += 2;
        lcd_update();

        printf("Stock bootloader hash:");

        for (int i = 0; i < SIGN_SZ; i++) {
            lcd_putsf(i * 2, line, "%02X", backup_hash[i]);
        }

        line++;
        lcd_update();
    }
    else {
        // Stock bootloader is installed as primary
        // No backup bootloader
        printf("Rockbox bootloader is not installed!");
        line++;

        printf("Stock bootloader hash:");

        for (int i = 0; i < SIGN_SZ; i++) {
            lcd_putsf(i * 2, line, "%02X", primary_hash[i]);
        }

        line++;
        lcd_update();
    }

end:
    line++;
    while (button_status() != BUTTON_NONE)
        sleep(HZ/100);
    lcd_set_foreground(LCD_RBYELLOW);
    printf("Press SELECT to continue");
    while (button_status() != BUTTON_SELECT)
        sleep(HZ/100);
}

#ifdef HAVE_SERIAL

#define FLASH_PAGES (FLASH_SIZE >> 12)
#define FLASH_PAGE_SIZE (FLASH_SIZE >> 8)

static void dump_bootflash(void)
{
    lcd_clear_display();
    lcd_set_foreground(LCD_WHITE);
    line = 0;

    uint8_t page[FLASH_PAGE_SIZE];
    printf("Total pages: %d", FLASH_PAGES);

    bootflash_init(SPI_PORT);

    for (int i = 0; i < FLASH_PAGES; i++) {
        printf("Reading flash... %d", i + 1);
        bootflash_read(SPI_PORT, i << 12, FLASH_PAGE_SIZE, page);

        printf("Sending over UART... %d", i + 1);
        serial_tx_raw(page, FLASH_PAGE_SIZE);
        line -= 2;
    }

    bootflash_close(SPI_PORT);

    line += 2;
    printf("Done!");
    piezo_seq(alive);

    line++;
    lcd_set_foreground(LCD_RBYELLOW);
    printf("Press SELECT to continue");
    while (button_status() != BUTTON_SELECT)
        sleep(HZ/100);
}
#endif /* HAVE_SERIAL */
#endif /* IPOD_6G || IPOD_NANO3G */

static void devel_menu(void)
{
    const char *items[] = {
#ifdef HAVE_LCD_SLEEP
        "LCD sleep/awake test",
#endif
        "PMU info",
        "GPIO info",
#if defined(IPOD_6G) || defined(IPOD_NANO3G)
        "Show SysCfg",
        "Show bootloader hash",
#ifdef HAVE_SERIAL
        "Dump bootflash to UART",
#endif
#endif
        "Launch OF",
        //"Launch Rockbox",
        "Restart",
        "Power off",
    };
    void (*handlers[])(void) = {
#ifdef HAVE_LCD_SLEEP
        sleep_test,
#endif
        pmu_info,
        gpio_info,
#if defined(IPOD_6G) || defined(IPOD_NANO3G)
        print_syscfg,
        print_bootloader_hash,
#ifdef HAVE_SERIAL
        dump_bootflash,
#endif
#endif
        run_of,
        //run_rockbox,
        system_reboot,
        power_off,
    };
    const size_t items_count = sizeof(items) / sizeof(items[0]);
    unsigned char selected_item = 0;

    while (1)
    {
        lcd_clear_display();
        lcd_set_foreground(LCD_RBYELLOW);
        line = 0;
        printf("Development menu");

        for (size_t i = 0; i < items_count; i++) {
            lcd_set_foreground(i == selected_item ? LCD_GREEN : LCD_WHITE);
            printf(items[i]);
        }

        while (button_status() != BUTTON_NONE);

        bool done = false;
        while (!done)
        {
            switch (button_status())
            {
                case BUTTON_MENU:
                case BUTTON_LEFT:
                    if (selected_item > 0) {
                        selected_item--;
                        done = true;
                    }
                    else {
                        sleep(HZ/100);
                    }
                    break;
                case BUTTON_PLAY:
                case BUTTON_RIGHT:
                    if (selected_item < items_count - 1) {
                        selected_item++;
                        done = true;
                    }
                    else {
                        sleep(HZ/100);
                    }
                    break;
                case BUTTON_SELECT:
                    handlers[selected_item]();
                    done = true;
                    break;
                default:
                    sleep(HZ/100);
                    break;
            }
        }
    }
}
#endif /* S5L87XX_DEVELOPMENT_BOOTLOADER */

void main(void)
{
    int rc = 0;
#ifdef IPOD_NANO3G
    bool n3g_lcd_ready = false;
#endif

    usec_timer_init();

#ifdef IPOD_NANO3G
    nano3g_nand_entry_diag_run();
#endif

#ifdef S5L87XX_DEVELOPMENT_BOOTLOADER
    piezo_seq(alive);
#endif

    /* Configure I2C0 */
    i2c_preinit(0);
#ifdef IPOD_NANO3G
    nano3g_nand_stage_diag_run(0);
#endif

    if (pmu_is_hibernated()) {
        rc = launch_onb(1); /* 27/2 = 13.5 MHz. */
    }
#ifdef IPOD_NANO3G
    nano3g_nand_stage_diag_run(1);
#endif

    system_preinit();
#ifdef IPOD_NANO3G
    nano3g_nand_stage_diag_run(2);
#endif
    memory_init();
#ifdef IPOD_NANO3G
    nano3g_nand_stage_diag_run(3);
#endif
    /*
     * XXX: BSS is initialized here, do not use .bss before this line
     */
    bss_init();
#ifdef IPOD_NANO3G
    nano3g_nand_stage_diag_run(4);
#endif

    system_init();
#ifdef IPOD_NANO3G
    nano3g_nand_stage_diag_run(5);
#endif
    kernel_init();
#ifdef IPOD_NANO3G
    nano3g_nand_stage_diag_run(6);
#endif
    i2c_init();
#ifdef IPOD_NANO3G
    nano3g_nand_stage_diag_run(7);
#endif
    power_init();
#ifdef IPOD_NANO3G
    nano3g_nand_stage_diag_run(8);

    /*
     * Transient DFU bring-up: make the display visible before button
     * selection or storage so black-screen failures tell us whether C ran.
     */
    lcd_init();
    lcd_set_foreground(LCD_WHITE);
    lcd_set_background(LCD_BLACK);
    lcd_clear_display();
    font_init();
    lcd_setfont(FONT_SYSFIXED);
    nano3g_boottrace_enable_lcd(true);
    backlight_init();
    verbose = true;
    printf("N3G_TRANSIENT_VIS");
    printf("N3G_BUILD 20260526_SKIPM34_M35");
    printf("N3G_VIS_OK_CONTINUE");
    lcd_update();
    n3g_lcd_ready = true;
#endif

    enable_irq();

#ifdef HAVE_SERIAL
    serial_setup();
#endif

#ifndef IPOD_NANO3G
    button_init();
    if (rc == 0) {
        /* User button selection timeout */
        while (USEC_TIMER < 400000);
        int btn = button_read_device();
        /* This prevents HDD spin-up when the user enters DFU */
        if (btn == (BUTTON_SELECT|BUTTON_MENU)) {
            while (button_read_device() == (BUTTON_SELECT|BUTTON_MENU))
                sleep(HZ/10);
            sleep(HZ);
            btn = button_read_device();
        }
        /* Enter OF, diagmode and diskmode using ONB */
        if (button_hold()
                || (btn == BUTTON_MENU)
                || (btn == (BUTTON_SELECT|BUTTON_LEFT))
                || (btn == (BUTTON_SELECT|BUTTON_PLAY))) {
            rc = kernel_launch_onb();
        }
    }
#else
    printf("N3G_SKIP_BUTTON_SELECT");
#endif

#ifdef IPOD_NANO3G
    if (!n3g_lcd_ready)
#endif
    {
        lcd_init();
        lcd_set_foreground(LCD_WHITE);
        lcd_set_background(LCD_BLACK);
        lcd_clear_display();
        font_init();
        lcd_setfont(FONT_SYSFIXED);
    }

    // TODO: see if removing this causes the nano3g LCD to initialize properly
#ifdef S5L87XX_DEVELOPMENT_BOOTLOADER
    sleep(HZ);
    for (int i = 0; i < lcd_type+1; i++) {
        sleep(HZ/2);
        piezo_seq(alivelcd);
    }
#endif

    lcd_update();
    sleep(HZ/40);  /* wait for lcd update */

    verbose = true;

    printf("Rockbox boot loader");
    printf("Version: %s", rbversion);
#ifdef IPOD_NANO3G
    printf("N3G_PRESTOR_CONTINUE_20260525");
    lcd_update();
#endif

#ifdef IPOD_NANO3G
    if (!n3g_lcd_ready)
#endif
    {
        backlight_init(); /* Turns on the backlight */
    }

#if defined(IPOD_NANO3G) && 0
    struct nano3g_nand_direct_diag ndiag;
    nano3g_nand_direct_diag(&ndiag);
    printf("DID rc %ld id %08lx", (long)ndiag.id_rc,
           (unsigned long)ndiag.id);
    printf("rst rc %ld st %08lx", (long)ndiag.reset_rc,
           (unsigned long)ndiag.stat_reset);
    printf("p80 rc %ld sp %ld", (long)ndiag.page_rc,
           (long)ndiag.spare_rc);
    printf("bk %lu c %08lx", (unsigned long)ndiag.test_bank,
           (unsigned long)ndiag.test_ctrl0);
    printf("mx %08lx %08lx", (unsigned long)ndiag.misc_before,
           (unsigned long)ndiag.misc_after);
    printf("mf %lu mm %lu", (unsigned long)ndiag.misc_force,
           (unsigned long)ndiag.misc_mode);
    printf("sr %ld xr %ld", (long)ndiag.page_status_rc,
           (long)ndiag.page_xfer_rc);
    printf("rs %08lx %08lx %08lx", (unsigned long)ndiag.read_stat_cmd0,
           (unsigned long)ndiag.read_stat_addr,
           (unsigned long)ndiag.read_stat_cmd30);
    printf("ss %08lx %08lx", (unsigned long)ndiag.read_stat_cmd70,
           (unsigned long)ndiag.read_stat_wait);
    printf("rz %ld %08lx", (long)ndiag.read_post0_rc,
           (unsigned long)ndiag.read_stat_post0);
    printf("sa %08lx", (unsigned long)ndiag.scratch_addr);
    printf("dw %08lx %08lx", (unsigned long)ndiag.dataw0,
           (unsigned long)ndiag.dataw1);
    printf("sw %08lx %08lx", (unsigned long)ndiag.scratch0,
           (unsigned long)ndiag.scratch1);
    printf("ad %08lx %08lx an %08lx", (unsigned long)ndiag.read_addr0,
           (unsigned long)ndiag.read_addr1,
           (unsigned long)ndiag.read_anum);
    printf("ct %08lx %08lx %08lx", (unsigned long)ndiag.ctrl_before_read0,
           (unsigned long)ndiag.ctrl_before_addr,
           (unsigned long)ndiag.ctrl_before_read30);
    printf("cc %08lx st %08lx", (unsigned long)ndiag.ctrl_before_copy,
           (unsigned long)ndiag.stat_before_copy);
    static const unsigned sweep_offs[9] =
        { 0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80 };
    for (int i = 0; i < 9; i++)
    {
        printf("q%02x %08lx %08lx", sweep_offs[i],
               (unsigned long)ndiag.source_sweep[i][0],
               (unsigned long)ndiag.source_sweep[i][1]);
    }
    printf("q78b %08lx %08lx %08lx",
           (unsigned long)ndiag.q78_trace[0][0],
           (unsigned long)ndiag.q78_trace[0][1],
           (unsigned long)ndiag.q78_trace[0][2]);
    printf("q78t %08lx %08lx %08lx",
           (unsigned long)ndiag.q78_trace[1][0],
           (unsigned long)ndiag.q78_trace[1][1],
           (unsigned long)ndiag.q78_trace[1][2]);
    printf("q7816 %08lx %08lx %08lx",
           (unsigned long)ndiag.q78_trace[2][0],
           (unsigned long)ndiag.q78_trace[2][1],
           (unsigned long)ndiag.q78_trace[2][2]);
    printf("q7832 %08lx %08lx %08lx",
           (unsigned long)ndiag.q78_trace[3][0],
           (unsigned long)ndiag.q78_trace[3][1],
           (unsigned long)ndiag.q78_trace[3][2]);
    for (int i = 0; i < 16; i += 4)
    {
        printf("s%02d %08lx %08lx %08lx %08lx", i,
               (unsigned long)ndiag.scratch_words[i],
               (unsigned long)ndiag.scratch_words[i + 1],
               (unsigned long)ndiag.scratch_words[i + 2],
               (unsigned long)ndiag.scratch_words[i + 3]);
    }
    for (int i = 0; i < 16; i += 4)
    {
        printf("i%02d %08lx %08lx %08lx %08lx", i,
               (unsigned long)ndiag.page_imm_words[i],
               (unsigned long)ndiag.page_imm_words[i + 1],
               (unsigned long)ndiag.page_imm_words[i + 2],
               (unsigned long)ndiag.page_imm_words[i + 3]);
    }
    for (int p = 0; p < N3G_PAGE_SCAN_COUNT; p++)
    {
        printf("pg%lu rc %ld nf %lu sig %08lx",
               (unsigned long)ndiag.dump_pages[p],
               (long)ndiag.dump_rc[p],
               (unsigned long)ndiag.dump_non_ff[p],
               (unsigned long)ndiag.dump_sig510[p]);
    }
    printf("first_nf %08lx mbr %08lx",
           (unsigned long)ndiag.first_non_ff_page,
           (unsigned long)ndiag.first_mbr_page);
    printf("p80 %08lx %08lx", (unsigned long)ndiag.page0,
           (unsigned long)ndiag.page1);
    printf("sp %08lx %08lx", (unsigned long)ndiag.spare0,
           (unsigned long)ndiag.spare1);
    printf("xst %08lx", (unsigned long)ndiag.xfer_stat);
    printf("xc %08lx %08lx", (unsigned long)ndiag.xfer_ctrl0,
           (unsigned long)ndiag.xfer_ctrl1);
    printf("xa %08lx dn %08lx", (unsigned long)ndiag.xfer_addr2,
           (unsigned long)ndiag.xfer_dnum);
    printf("pwr %08lx %08lx", (unsigned long)ndiag.pwr0,
           (unsigned long)ndiag.pwr1);
    printf("clk1 %08lx", (unsigned long)ndiag.clk1);
    sleep(HZ * 8);
#endif

#ifdef S5L87XX_DEVELOPMENT_BOOTLOADER
    line++;
    printf("lcd type: %d", lcd_type);
#ifdef S5L_LCD_WITH_READID
    extern unsigned char lcd_id[4];
    uint32_t* lcd_id_32 = (uint32_t *)lcd_id;
    printf("lcd id: 0x%x", *lcd_id_32);
#endif
#ifdef IPOD_NANO4G
    printf("boot cfg: 0x%x", pmu_read(0x7f));
#endif
    line++;
    printf("Press SELECT to continue");
    while (button_status() != BUTTON_SELECT)
        sleep(HZ/100);

    devel_menu();
#endif /* S5L87XX_DEVELOPMENT_BOOTLOADER */

#ifndef S5L87XX_DEVELOPMENT_BOOTLOADER
    if (rc == 0) {
#if (CONFIG_STORAGE & STORAGE_ATA)
        /* Wait until there is enought power to spin-up HDD */
        battery_trap();
#endif

#ifdef IPOD_NANO3G
        lcd_clear_display();
        line = 0;
        nano3g_nand_stage_diag_run(9);
        printf("N3G_BOOT_STORAGE_START");
#endif

        rc = storage_init();
#ifdef IPOD_NANO3G
        printf("N3G_STORAGE_RET rc=%d", rc);
#endif
        if (rc != 0) {
#ifdef IPOD_NANO3G
            int btn = button_read_device();
            if (button_hold()
                    || (btn == BUTTON_MENU)
                    || (btn == (BUTTON_SELECT|BUTTON_LEFT))
                    || (btn == (BUTTON_SELECT|BUTTON_PLAY))) {
                printf("Executing OF...");
                rc = kernel_launch_onb();
                if (rc == 0)
                    goto of_loaded;
            }
#endif
            printf("Storage error: %d", rc);
            fatal_error(ERR_STORAGE);
        }

#ifdef IPOD_NANO3G
        n3g_storage_direct_lba_diag();
#endif

        filesystem_init();
#ifdef IPOD_NANO3G
        printf("N3G_FS_RET");
#endif

        /* We wait until HDD spins up to check for hold button */
        if (button_hold()) {
#ifdef SYSCFG_MAX_ENTRIES
            bool lba48 = false;
            struct SysCfg syscfg;
            const ssize_t result = syscfg_read(&syscfg);
            if (result != -1) {
                const size_t syscfg_num_entries = MIN(syscfg.header.num_entries, SYSCFG_MAX_ENTRIES);
                for (size_t i = 0; i < syscfg_num_entries; i++) {
                    const struct SysCfgEntry* entry = &syscfg.entries[i];
                    const uint32_t* data32 = (uint32_t *)entry->data;
                    if (entry->tag == SYSCFG_TAG_HWVR) {
                        lba48 = (data32[1] >= 0x130200);
                        break;
                    }
                }

                int btn = button_read_device();

                struct storage_info sinfo;
                storage_get_info(0, &sinfo);
                if (sinfo.num_sectors < (1 << 28) || lba48 || btn & BUTTON_LEFT) {
                    printf("Executing OF...");
#if (CONFIG_STORAGE & STORAGE_ATA)
                    ata_sleepnow();
#endif
                    rc = kernel_launch_onb();
                } else {
                    printf("OF does not support LBA48");
                    fatal_error(ERR_LBA28);
                }
            }
#else
            printf("Executing OF...");
#if (CONFIG_STORAGE & STORAGE_ATA)
            ata_sleepnow();
#endif
            rc = kernel_launch_onb();
#endif /* SYSCFG_MAX_ENTRIES */
        }
    }

of_loaded:
    if (rc != 0) {
        printf("Load OF error: %d", rc);
        fatal_error(ERR_OF);
    }

#ifdef HAVE_BOOTLOADER_USB_MODE
    /* Enter USB mode if SELECT+RIGHT are pressed */
    if (button_read_device() == (BUTTON_SELECT|BUTTON_RIGHT)) {
#if defined(MAX_VIRT_SECTOR_SIZE) && defined(DEFAULT_VIRT_SECTOR_SIZE)
#ifdef HAVE_MULTIDRIVE
            for (int i = 0 ; i < NUM_DRIVES ; i++)
#endif
                disk_set_sector_multiplier(IF_MD(i,) DEFAULT_VIRT_SECTOR_SIZE/SECTOR_SIZE);
#endif
        usb_mode();
    }
#endif

    rc = disk_mount_all();
#ifdef IPOD_NANO3G
    printf("N3G_MOUNT_RET rc=%d", rc);
#endif
    if (rc <= 0) {
#ifdef STORAGE_GET_INFO
        struct storage_info sinfo;
        storage_get_info(0, &sinfo);
#ifdef MAX_PHYS_SECTOR_SIZE
        printf("id: '%s' s:%u*%u", sinfo.product, sinfo.sector_size, sinfo.phys_sector_mult);
#else
        printf("id: '%s' s:%u", sinfo.product, sinfo.sector_size);
#endif
#endif
        struct partinfo pinfo;
        printf("No partition found");
        for (int i = 0 ; i < NUM_VOLUMES ; i++) {
            disk_partinfo(i, &pinfo);
            if (pinfo.type)
                printf("P%d T%02x S%llx",
                       i, pinfo.type, (unsigned long long)pinfo.size);
        }
        fatal_error(ERR_RB);
    }

#if defined(IPOD_NANO3G) && 0
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_MOUNTSTOP_BF");
    lcd_puts(0, 1, "mounted no load");
    lcd_update();
    while (1)
        sleep(HZ);
#endif

    printf("Loading Rockbox...");
#if defined(IPOD_NANO3G) && 0
    printf("N3G_LOAD_PATH path=/" BOOTFILE);
#endif
    unsigned char *loadbuffer = (unsigned char *)DRAM_ORIG;
#if defined(IPOD_NANO3G) && 0
    rc = load_firmware(loadbuffer, "/" BOOTFILE, MAX_LOADSIZE);
#else
    rc = load_firmware(loadbuffer, BOOTFILE, MAX_LOADSIZE);
#endif

    if (rc <= EFILE_EMPTY) {
        printf("Error!");
        printf("Can't load " BOOTFILE ": ");
        printf(loader_strerror(rc));
        fatal_error(ERR_RB);
    }

    printf("Rockbox loaded.");
#if defined(IPOD_NANO3G) && 0
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_LOADSTOP_20260525");
    lcd_puts(0, 1, "loaded no handoff");
    printf("N3G_LOADVERIFY rc=%ld b0=%02x%02x%02x%02x%02x%02x%02x%02x b8=%02x%02x%02x%02x%02x%02x%02x%02x",
           (long)rc,
           loadbuffer[0], loadbuffer[1], loadbuffer[2], loadbuffer[3],
           loadbuffer[4], loadbuffer[5], loadbuffer[6], loadbuffer[7],
           loadbuffer[8], loadbuffer[9], loadbuffer[10], loadbuffer[11],
           loadbuffer[12], loadbuffer[13], loadbuffer[14], loadbuffer[15]);
    lcd_update();
    while (1)
        sleep(HZ);
#endif

    /* If we get here, we have a new firmware image at 0x08000000, run it */
#if defined(IPOD_NANO3G)
    printf("N3G_JPREP addr=%08lx rc=%ld", (unsigned long)loadbuffer,
           (long)rc);
    printf("N3G_MEMCALL a=08078160");
    lcd_update();
    commit_discard_idcache();
    ((void (*)(void))(loadbuffer + 0x78160))();
    printf("N3G_MEMRET");
    printf("N3G_CRT0MANUAL");
    printf("N3G_CRT0SKIPVEC");
    memset((void *)0x080d23a0, 0, 0x081c5538 - 0x080d23a0);
    printf("N3G_CRT0BSS");
    memcpy((void *)0x00000060, loadbuffer + 0x0cff68, 0x00002494 - 0x60);
    printf("N3G_CRT0IRAM");
    memset((void *)0x00002494, 0, 0x00007d50 - 0x00002494);
    printf("N3G_CRT0IBSS");
    /* Bypass the native storage path that loops after handoff. */
    *(volatile uint32_t *)(loadbuffer + 0x5e2c) = 0xe3a00000u;
    printf("N3G_PATCHSTOR a=08005e2c n=E3A00000");
	    *(volatile uint32_t *)(loadbuffer + 0x5e84) = 0xe3a00001u;
	    printf("N3G_PATCHMOUNT a=08005e84 n=E3A00001");
		    *(volatile uint32_t *)(loadbuffer + 0x77fac) = 0xe12fff1eu;
	    *(volatile uint32_t *)(loadbuffer + 0x780e8) = 0xe12fff1eu;
	    *(volatile uint32_t *)(loadbuffer + 0x77dec) = 0xe12fff1eu;
	    *(volatile uint32_t *)(loadbuffer + 0x83f48) = 0xe12fff1eu;
	    for (unsigned int n3g_irq_patch = 0x5db4; n3g_irq_patch <= 0x5dc8;
	         n3g_irq_patch += 4)
	        *(volatile uint32_t *)(loadbuffer + n3g_irq_patch) = 0xe1a00000u;
			    *(volatile uint32_t *)(loadbuffer + 0x5e30) = 0xe321f0dfu;
			    *(volatile uint32_t *)(loadbuffer + 0x5e34) = 0xea000012u;
			    *(volatile uint32_t *)(loadbuffer + 0x5e88) = 0xe3a06001u;
			    *(volatile uint32_t *)(loadbuffer + 0x5e8c) = 0xea000034u;
			    *(volatile uint32_t *)(loadbuffer + 0x5f6c) = 0xe1a00000u;
			    *(volatile uint32_t *)(loadbuffer + 0x5f70) = 0xe1a00000u;
			    *(volatile uint32_t *)(loadbuffer + 0x5f74) = 0xea00000au;
			    *(volatile uint32_t *)(loadbuffer + 0x5fa4) = 0xe5950024u;
			    *(volatile uint32_t *)(loadbuffer + 0x5fa8) = 0xe1c00fc0u;
			    *(volatile uint32_t *)(loadbuffer + 0x5fac) = 0xe1a00000u;
			    *(volatile uint32_t *)(loadbuffer + 0x5fb0) = 0xe1a00000u;
			    *(volatile uint32_t *)(loadbuffer + 0x5fb4) = 0xe1a00000u;
			    *(volatile uint32_t *)(loadbuffer + 0x5fb8) = 0xe1a00000u;
			    *(volatile uint32_t *)(loadbuffer + 0x5fbc) = 0xea000013u;
			    *(volatile uint32_t *)(loadbuffer + 0x6010) = 0xe59f8184u;
			    *(volatile uint32_t *)(loadbuffer + 0x6014) = 0xe59f91b4u;
			    *(volatile uint32_t *)(loadbuffer + 0x6018) = 0xe59fa1b4u;
			    *(volatile uint32_t *)(loadbuffer + 0x601c) = 0xe3a07000u;
			    *(volatile uint32_t *)(loadbuffer + 0x6020) = 0xe3a00001u;
			    *(volatile uint32_t *)(loadbuffer + 0x602c) = 0xea000039u;
			    *(volatile uint32_t *)(loadbuffer + 0x6118) = 0xe28dd04cu;
			    *(volatile uint32_t *)(loadbuffer + 0x611c) = 0xe8bd8880u;
		    commit_discard_idcache();
#if 1
{
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_CMAIN_GO");
    lcd_puts(0, 1, "loaded image");
    lcd_update();
    sleep(HZ / 2);

    ((void (*)(void))(loadbuffer + 0x667f4))();
    ((void (*)(void))(loadbuffer + 0x83d48))();
    ((void (*)(void))(loadbuffer + 0x82dc0))();
    ((void (*)(void))(loadbuffer + 0x84170))();
    ((void (*)(void))(loadbuffer + 0x6a878))();
    ((void (*)(uint32_t))(loadbuffer + 0x67800))(1);
    ((void (*)(void))(loadbuffer + 0x77754))();
    ((void (*)(void))(loadbuffer + 0x7faf4))();
    ((void (*)(void))(loadbuffer + 0x743e4))();
    ((void (*)(void))(loadbuffer + 0x723a4))();
    ((void (*)(void))(loadbuffer + 0x1af48))();
    ((void (*)(void))(loadbuffer + 0x5d0c))();
    ((void (*)(uint32_t, uint32_t, uint32_t))(loadbuffer + 0x5a84))
        (0x080c31dcu, 0x0812098cu, 884);
    ((void (*)(void))(loadbuffer + 0x81dc4))();
    ((void (*)(void))(loadbuffer + 0x81c74))();
    ((void (*)(void))(loadbuffer + 0x7fae4))();
    ((void (*)(void))(loadbuffer + 0x67c2c))();
    ((void (*)(void))(loadbuffer + 0x656e0))();
    ((void (*)(void))(loadbuffer + 0x7565c))();
    ((void (*)(uint32_t, uint32_t, uint32_t))(loadbuffer + 0x82dc4))
        (0x081658a4u, 1, 0);
    ((void (*)(void))(loadbuffer + 0x77a14))();
    (void)((uint32_t (*)(void))(loadbuffer + 0x75128))();
    uint32_t n3g_fast_r = ((uint32_t (*)(void))(loadbuffer + 0x75128))();
    *(volatile uint32_t *)0x08164690 = n3g_fast_r;
    ((void (*)(void))(loadbuffer + 0x6741c))();
    *(volatile uint32_t *)0x081646b0 = 0x080750f4u;
    ((void (*)(uint32_t))(loadbuffer + 0x840f8))(0x08075150u);
    ((void (*)(void))(loadbuffer + 0x67384))();
    ((void (*)(void))(loadbuffer + 0x7f5d0))();
    ((void (*)(uint32_t))(loadbuffer + 0x31b04))(0x081153a0u);
    ((void (*)(void))(loadbuffer + 0x32244))();
    ((void (*)(void))(loadbuffer + 0x348dc))();
    ((void (*)(void))(loadbuffer + 0x32ee0))();
    (void)((uint32_t (*)(void))(loadbuffer + 0x75770))();
    ((void (*)(void))(loadbuffer + 0x75ec0))();
    ((void (*)(void))(loadbuffer + 0x598e0))();
    ((void (*)(void))(loadbuffer + 0x1b310))();
    ((void (*)(void))(loadbuffer + 0x671a4))();
    volatile uint32_t *n3g_fast_state = (volatile uint32_t *)0x080f12b0u;
    ((void (*)(uint32_t))(loadbuffer + 0x6d948))
        (n3g_fast_state[9] & 0x7fffffffu);
    ((void (*)(uint32_t))(loadbuffer + 0x1abc0))(1);

    volatile uint16_t *n3g_fb = (volatile uint16_t *)0x0813d540u;
    for (unsigned int n3g_px = 0; n3g_px < 76800; n3g_px++)
        n3g_fb[n3g_px] = 0x07e0u;
    ((void (*)(void))(loadbuffer + 0x8f2a8))();
    commit_discard_idcache();

    asm volatile(
        "msr cpsr_c, %[sys_cpsr]\n"
        "mov sp, %[sys_sp]\n"
        "mov r4, %[state_a]\n"
        "mov r5, %[state_b]\n"
        "bx %[entry]\n"
        :
        : [sys_cpsr]"r"(0xdfu), [sys_sp]"r"(0x00009d50u),
          [state_a]"r"(0x080f0b34u), [state_b]"r"(0x080f12b0u),
          [entry]"r"(loadbuffer + 0x600c)
        : "r0", "r1", "r2", "r3", "r4", "r5", "r12", "lr", "cc",
          "memory");
    while (1)
        sleep(HZ);
}
#endif
#if 0
			    char n3g_line[32];
		    lcd_clear_display();
		    lcd_puts(0, 0, "N3G_EPI_GO");
		    snprintf(n3g_line, sizeof(n3g_line), "5e34 %08lx",
		             (unsigned long)*(volatile uint32_t *)(loadbuffer + 0x5e34));
		    lcd_puts(0, 1, n3g_line);
		    snprintf(n3g_line, sizeof(n3g_line), "6118 %08lx",
		             (unsigned long)*(volatile uint32_t *)(loadbuffer + 0x6118));
		    lcd_puts(0, 2, n3g_line);
			    lcd_update();
			    lcd_clear_display();
			    lcd_puts(0, 0, "N3G_77FAC_STOP");
			    snprintf(n3g_line, sizeof(n3g_line), "77fac %08lx",
			             (unsigned long)*(volatile uint32_t *)(loadbuffer + 0x77fac));
			    lcd_puts(0, 1, n3g_line);
			    lcd_update();
			    ((void (*)(void))(loadbuffer + 0x77fac))();
		    lcd_clear_display();
		    lcd_puts(0, 0, "N3G_77FAC_RET");
		    lcd_puts(0, 1, "direct ok");
		    lcd_update();
		    void *n3g_main_fn = loadbuffer + 0x5d7c;
		    asm volatile(
		        "mrs r6, cpsr\n"
		        "ldr r7, =n3g_probe_saved_cpsr\n"
		        "str r6, [r7]\n"
		        "mov r6, sp\n"
		        "ldr r7, =n3g_probe_saved_sp\n"
		        "str r6, [r7]\n"
		        "msr cpsr_c, %[sys_cpsr]\n"
		        "mov sp, %[sys_sp]\n"
		        "blx %[fn]\n"
		        "ldr r7, =n3g_probe_saved_cpsr\n"
		        "ldr r6, [r7]\n"
		        "msr cpsr_c, r6\n"
		        "ldr r7, =n3g_probe_saved_sp\n"
		        "ldr r6, [r7]\n"
		        "mov sp, r6\n"
		        :
		        : [sys_cpsr]"r"(0xdfu), [sys_sp]"r"(0x00009d50u),
		          [fn]"r"(n3g_main_fn)
		        : "r0", "r1", "r2", "r3", "r6", "r7", "r12", "lr", "cc",
		          "memory");
		    lcd_clear_display();
		    lcd_puts(0, 0, "N3G_EPI_RET");
		    lcd_puts(0, 1, "main returned");
		    lcd_update();
			    while (1)
			        sleep(HZ);
#endif
#if 0
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_MAINSTEP_GO");
    lcd_puts(0, 1, "manual crt0");
    lcd_puts(0, 2, "step native");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M0 skip=08077fac");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M0 skip");
    lcd_puts(0, 1, "08077fac");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M0 skipped");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M0 skipped");
    lcd_puts(0, 1, "M1 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M1 call=080667f4");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M1 call");
    lcd_puts(0, 1, "080667f4");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x667f4))();
    printf("N3G_M1 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M1 ret");
    lcd_puts(0, 1, "M2 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M2 call=08083d48");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M2 call");
    lcd_puts(0, 1, "08083d48");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x83d48))();
    printf("N3G_M2 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M2 ret");
    lcd_puts(0, 1, "M3 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M3 call=08082dc0");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M3 call");
    lcd_puts(0, 1, "08082dc0");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x82dc0))();
    printf("N3G_M3 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M3 ret");
    lcd_puts(0, 1, "M4 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M4 call=08084170");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M4 call");
    lcd_puts(0, 1, "08084170");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x84170))();
    printf("N3G_M4 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M4 ret");
    lcd_puts(0, 1, "M5 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M5 call=0806a878");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M5 call");
    lcd_puts(0, 1, "0806a878");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x6a878))();
    printf("N3G_M5 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M5 ret");
    lcd_puts(0, 1, "M6 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M6 skip=080780e8 arg=0337f980");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M6 skip");
    lcd_puts(0, 1, "080780e8");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M6 skipped");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M6 skipped");
    lcd_puts(0, 1, "M7 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M7 call=08067800 arg=1");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M7 call");
    lcd_puts(0, 1, "08067800");
    lcd_update();
    ((void (*)(uint32_t))(loadbuffer + 0x67800))(1);
    printf("N3G_M7 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M7 ret");
    lcd_puts(0, 1, "M8 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M8 call=08077754");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M8 call");
    lcd_puts(0, 1, "08077754");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x77754))();
    printf("N3G_M8 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M8 ret");
    lcd_puts(0, 1, "M9 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M9 call=0807faf4");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M9 call");
    lcd_puts(0, 1, "0807faf4");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x7faf4))();
    printf("N3G_M9 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M9 ret");
    lcd_puts(0, 1, "M10 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M10 call=080743e4");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M10 call");
    lcd_puts(0, 1, "080743e4");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x743e4))();
    printf("N3G_M10 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M10 ret");
    lcd_puts(0, 1, "M11 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M11 call=080723a4");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M11 call");
    lcd_puts(0, 1, "080723a4");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x723a4))();
    printf("N3G_M11 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M11 ret");
    lcd_puts(0, 1, "M12 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M12 call=0801af48");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M12 call");
    lcd_puts(0, 1, "0801af48");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x1af48))();
    printf("N3G_M12 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M12 ret");
    lcd_puts(0, 1, "M13 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M13 call=08005d0c");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M13 call");
    lcd_puts(0, 1, "08005d0c");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x5d0c))();
    printf("N3G_M13 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M13 ret");
    lcd_puts(0, 1, "M14 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M14 call=08005a84");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M14 call");
    lcd_puts(0, 1, "08005a84");
    lcd_update();
    ((void (*)(uint32_t, uint32_t, uint32_t))(loadbuffer + 0x5a84))
        (0x080c31dcu, 0x0812098cu, 884);
    printf("N3G_M14 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M14 ret");
    lcd_puts(0, 1, "M15 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M15 call=08081dc4");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M15 call");
    lcd_puts(0, 1, "08081dc4");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x81dc4))();
    printf("N3G_M15 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M15 ret");
    lcd_puts(0, 1, "M16 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M16 call=08081c74");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M16 call");
    lcd_puts(0, 1, "08081c74");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x81c74))();
    printf("N3G_M16 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M16 ret");
    lcd_puts(0, 1, "M17 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M17 call=0807fae4");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M17 call");
    lcd_puts(0, 1, "0807fae4");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x7fae4))();
    printf("N3G_M17 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M17 ret");
    lcd_puts(0, 1, "M18 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M18 call=08067c2c");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M18 call");
    lcd_puts(0, 1, "08067c2c");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x67c2c))();
    printf("N3G_M18 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M18 ret");
    lcd_puts(0, 1, "M19 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M19 call=080656e0");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M19 call");
    lcd_puts(0, 1, "080656e0");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x656e0))();
    printf("N3G_M19 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M19 ret");
    lcd_puts(0, 1, "M20 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M20a call=0807565c");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20a call");
    lcd_puts(0, 1, "0807565c");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x7565c))();
    printf("N3G_M20a ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20a ret");
    lcd_puts(0, 1, "M20b next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M20b0 call=08082dc4");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20b0 call");
    lcd_puts(0, 1, "08082dc4");
    lcd_update();
    ((void (*)(uint32_t, uint32_t, uint32_t))(loadbuffer + 0x82dc4))
        (0x081658a4u, 1, 0);
    printf("N3G_M20b0 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20b0 ret");
    lcd_puts(0, 1, "M20b1 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M20b1 call=08077a14");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20b1 call");
    lcd_puts(0, 1, "08077a14");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x77a14))();
    printf("N3G_M20b1 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20b1 ret");
    lcd_puts(0, 1, "M20b2 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M20b2 skip=08082ddc");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20b2 skip");
    lcd_puts(0, 1, "08082ddc");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M20b2 skipped");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20b2 skipped");
    lcd_puts(0, 1, "M20c next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M20c call=08075128");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20c call");
    lcd_puts(0, 1, "08075128");
    lcd_update();
    uint32_t n3g_m20_r = ((uint32_t (*)(void))(loadbuffer + 0x75128))();
    *(volatile uint32_t *)0x08164690 = n3g_m20_r;
    printf("N3G_M20c ret r=%08lx", (unsigned long)n3g_m20_r);
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20c ret");
    lcd_puts(0, 1, "M20d next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M20d call=0806741c");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20d call");
    lcd_puts(0, 1, "0806741c");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x6741c))();
    printf("N3G_M20d ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20d ret");
    lcd_puts(0, 1, "M20e next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M20e call=080840f8");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20e call");
    lcd_puts(0, 1, "080840f8");
    lcd_update();
    *(volatile uint32_t *)0x081646b0 = 0x080750f4u;
    ((void (*)(uint32_t))(loadbuffer + 0x840f8))(0x08075150u);
    printf("N3G_M20e ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M20e ret");
    lcd_puts(0, 1, "M21 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M21 call=08067384");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M21 call");
    lcd_puts(0, 1, "08067384");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x67384))();
    printf("N3G_M21 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M21 ret");
    lcd_puts(0, 1, "M22 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M22 call=0807f5d0");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M22 call");
    lcd_puts(0, 1, "0807f5d0");
    lcd_update();
    ((void (*)(void))(loadbuffer + 0x7f5d0))();
    printf("N3G_M22 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M22 ret");
    lcd_puts(0, 1, "M23 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M23 call=08031b04");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M23 call");
    lcd_puts(0, 1, "08031b04");
    lcd_update();
    ((void (*)(uint32_t))(loadbuffer + 0x31b04))(0x081153a0u);
    printf("N3G_M23 ret");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M23 ret");
    lcd_puts(0, 1, "M24 next");
    lcd_update();
    sleep(HZ / 2);
    printf("N3G_M24 call=08032244");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G_M24 call");
    lcd_puts(0, 1, "08032244");
	    lcd_update();
		    ((void (*)(void))(loadbuffer + 0x32244))();
		    printf("N3G_M24 ret");
		    lcd_clear_display();
#endif
		    lcd_puts(0, 0, "N3G_FAST_GO");
	    lcd_puts(0, 1, "to M34");
	    lcd_update();
	    sleep(HZ / 2);

	    ((void (*)(void))(loadbuffer + 0x667f4))();
	    ((void (*)(void))(loadbuffer + 0x83d48))();
	    ((void (*)(void))(loadbuffer + 0x82dc0))();
	    ((void (*)(void))(loadbuffer + 0x84170))();
	    ((void (*)(void))(loadbuffer + 0x6a878))();
	    ((void (*)(uint32_t))(loadbuffer + 0x67800))(1);
	    ((void (*)(void))(loadbuffer + 0x77754))();
	    ((void (*)(void))(loadbuffer + 0x7faf4))();
	    ((void (*)(void))(loadbuffer + 0x743e4))();
	    ((void (*)(void))(loadbuffer + 0x723a4))();
	    ((void (*)(void))(loadbuffer + 0x1af48))();
	    ((void (*)(void))(loadbuffer + 0x5d0c))();
	    ((void (*)(uint32_t, uint32_t, uint32_t))(loadbuffer + 0x5a84))
	        (0x080c31dcu, 0x0812098cu, 884);
	    ((void (*)(void))(loadbuffer + 0x81dc4))();
	    ((void (*)(void))(loadbuffer + 0x81c74))();
	    ((void (*)(void))(loadbuffer + 0x7fae4))();
	    ((void (*)(void))(loadbuffer + 0x67c2c))();
	    ((void (*)(void))(loadbuffer + 0x656e0))();
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_WRAPSKIP_GO");
	    lcd_puts(0, 1, "skip wait");
	    lcd_update();
	    sleep(HZ / 2);
	    ((void (*)(void))(loadbuffer + 0x7565c))();
	    ((void (*)(uint32_t, uint32_t, uint32_t))(loadbuffer + 0x82dc4))
	        (0x081658a4u, 1, 0);
	    ((void (*)(void))(loadbuffer + 0x77a14))();
	    (void)((uint32_t (*)(void))(loadbuffer + 0x75128))();
	    uint32_t n3g_fast_r = ((uint32_t (*)(void))(loadbuffer + 0x75128))();
	    *(volatile uint32_t *)0x08164690 = n3g_fast_r;
	    ((void (*)(void))(loadbuffer + 0x6741c))();
	    *(volatile uint32_t *)0x081646b0 = 0x080750f4u;
	    ((void (*)(uint32_t))(loadbuffer + 0x840f8))(0x08075150u);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_WRAPSKIP_RET");
	    lcd_puts(0, 1, "next native");
	    lcd_update();
	    sleep(HZ / 2);
	    ((void (*)(void))(loadbuffer + 0x67384))();
	    ((void (*)(void))(loadbuffer + 0x7f5d0))();
	    ((void (*)(uint32_t))(loadbuffer + 0x31b04))(0x081153a0u);
	    ((void (*)(void))(loadbuffer + 0x32244))();
	    ((void (*)(void))(loadbuffer + 0x348dc))();
	    ((void (*)(void))(loadbuffer + 0x32ee0))();
	    (void)((uint32_t (*)(void))(loadbuffer + 0x75770))();
	    ((void (*)(void))(loadbuffer + 0x75ec0))();
	    ((void (*)(void))(loadbuffer + 0x598e0))();
	    ((void (*)(void))(loadbuffer + 0x1b310))();
	    ((void (*)(void))(loadbuffer + 0x671a4))();
	    volatile uint32_t *n3g_fast_state = (volatile uint32_t *)0x080f12b0u;
	    ((void (*)(uint32_t))(loadbuffer + 0x6d948))
	        (n3g_fast_state[9] & 0x7fffffffu);

	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M34 skip");
	    lcd_puts(0, 1, "tagcache");
	    lcd_update();
	    sleep(HZ / 2);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M35 call");
	    lcd_puts(0, 1, "0801abc0");
	    lcd_update();
	    ((void (*)(uint32_t))(loadbuffer + 0x1abc0))(1);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M35 ret");
	    lcd_puts(0, 1, "M36 next");
	    lcd_update();
	    sleep(HZ / 2);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M36 call");
	    lcd_puts(0, 1, "08050e84");
	    lcd_update();
	    ((void (*)(void))(loadbuffer + 0x50e84))();
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M36 ret");
	    lcd_puts(0, 1, "M37? next");
	    lcd_update();
	    sleep(HZ / 2);
	    uint32_t n3g_loop_ok = ((uint32_t (*)(void))(loadbuffer + 0x50f28))();
	    lcd_clear_display();
	    lcd_puts(0, 0, n3g_loop_ok ? "N3G_LOOP yes" : "N3G_LOOP no");
	    lcd_puts(0, 1, "M37 next");
	    lcd_update();
	    sleep(HZ / 2);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M37 call");
	    lcd_puts(0, 1, "080514b0");
	    lcd_update();
	    uint32_t n3g_m37_r = ((uint32_t (*)(void))(loadbuffer + 0x514b0))();
	    lcd_clear_display();
		    lcd_puts(0, 0, n3g_m37_r > 0 ? "N3G_M37 pos" : "N3G_M37 zero");
		    lcd_puts(0, 1, "sched stat");
		    lcd_update();
		    sleep(HZ / 2);
		    volatile uint32_t *n3g_sched = (volatile uint32_t *)0x000072b4u;
		    uint32_t n3g_ready0 = *(volatile uint32_t *)0x0819b37cu;
		    uint32_t n3g_tick = *(volatile uint32_t *)0x0819b3a4u;
		    uint32_t n3g_curid = ((uint32_t (*)(void))(loadbuffer + 0x83f34))();
		    printf("N3G_SCHED q=%08lx loop=%08lx id=%08lx",
		           (unsigned long)n3g_m37_r, (unsigned long)n3g_loop_ok,
		           (unsigned long)n3g_curid);
		    printf("N3G_SCHED2 run=%08lx cur=%08lx wake=%08lx ready=%08lx tick=%08lx",
		           (unsigned long)n3g_sched[0], (unsigned long)n3g_sched[3],
		           (unsigned long)n3g_sched[13], (unsigned long)n3g_ready0,
		           (unsigned long)n3g_tick);
		    lcd_clear_display();
		    lcd_puts(0, 0, "N3G_SCHEDSTAT");
		    lcd_puts(0, 1, n3g_m37_r > 0 ? "q pos" : "q zero");
		    lcd_puts(0, 2, n3g_sched[3] ? "cur set" : "cur null");
		    lcd_update();
		    sleep(HZ / 2);
		    volatile unsigned char *n3g_tg = (volatile unsigned char *)0x0811bd4cu;
		    uint32_t n3g_p28 = ((uint32_t (*)(void))(loadbuffer + 0x50f28))();
		    uint32_t n3g_p30 = ((uint32_t (*)(void))(loadbuffer + 0x50f30))();
		    uint32_t n3g_p40 = ((uint32_t (*)(void))(loadbuffer + 0x50f40))();
		    uint32_t n3g_p50 = ((uint32_t (*)(void))(loadbuffer + 0x50f50))();
		    printf("N3G_TGFLAGS f59=%02lx f5a=%02lx f5b=%02lx p28=%08lx p30=%08lx p40=%08lx p50=%08lx",
		           (unsigned long)n3g_tg[0x59],
		           (unsigned long)n3g_tg[0x5a],
		           (unsigned long)n3g_tg[0x5b],
		           (unsigned long)n3g_p28,
		           (unsigned long)n3g_p30,
		           (unsigned long)n3g_p40,
		           (unsigned long)n3g_p50);
		    char n3g_diag_line[32];
		    lcd_clear_display();
		    lcd_puts(0, 0, "N3G_TGFLAGS");
		    snprintf(n3g_diag_line, sizeof(n3g_diag_line), "59 %02lx 5A %02lx",
		             (unsigned long)n3g_tg[0x59],
		             (unsigned long)n3g_tg[0x5a]);
		    lcd_puts(0, 1, n3g_diag_line);
		    snprintf(n3g_diag_line, sizeof(n3g_diag_line), "5B %02lx P50 %lx",
		             (unsigned long)n3g_tg[0x5b],
		             (unsigned long)n3g_p50);
		    lcd_puts(0, 2, n3g_diag_line);
		    snprintf(n3g_diag_line, sizeof(n3g_diag_line), "P28 %lx P30 %lx",
		             (unsigned long)n3g_p28,
		             (unsigned long)n3g_p30);
		    lcd_puts(0, 3, n3g_diag_line);
		    lcd_update();
		    sleep(HZ / 2);
		    lcd_clear_display();
		    lcd_puts(0, 0, "N3G_NO_SLEEP");
		    lcd_puts(0, 1, "mainstep stop");
		    lcd_update();
	    while (1)
	        sleep(HZ);

#if 0
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M24 ret");
	    lcd_puts(0, 1, "M25 next");
	    lcd_update();
	    sleep(HZ / 2);
	    printf("N3G_M25 call=080348dc");
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M25 call");
	    lcd_puts(0, 1, "080348dc");
	    lcd_update();
	    ((void (*)(void))(loadbuffer + 0x348dc))();
	    printf("N3G_M25 ret");
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M25 ret");
	    lcd_puts(0, 1, "M26 next");
	    lcd_update();
	    sleep(HZ / 2);
	    printf("N3G_M26 call=08032ee0");
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M26 call");
	    lcd_puts(0, 1, "08032ee0");
	    lcd_update();
	    ((void (*)(void))(loadbuffer + 0x32ee0))();
	    printf("N3G_M26 ret");
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M26 ret");
	    lcd_puts(0, 1, "M27 next");
	    lcd_update();
	    sleep(HZ / 2);
	    printf("N3G_M27 call=08075770");
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M27 call");
	    lcd_puts(0, 1, "08075770");
	    lcd_update();
	    uint32_t n3g_m27_r = ((uint32_t (*)(void))(loadbuffer + 0x75770))();
	    printf("N3G_M27 ret r=%08lx", (unsigned long)n3g_m27_r);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M27 ret");
	    lcd_puts(0, 1, "postmount next");
	    lcd_update();
	    sleep(HZ / 2);
	    (void)n3g_m27_r;
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M28 skip");
	    lcd_puts(0, 1, "mount r=1");
	    lcd_update();
	    sleep(HZ / 2);
	    volatile uint8_t *n3g_main_cfg = (volatile uint8_t *)0x080f0b34u;
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M29 call");
	    lcd_puts(0, 1, "08075ec0");
	    lcd_update();
	    ((void (*)(void))(loadbuffer + 0x75ec0))();
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M29 ret");
	    lcd_puts(0, 1, "M30 next");
	    lcd_update();
	    sleep(HZ / 2);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M30 call");
	    lcd_puts(0, 1, "080598e0");
	    lcd_update();
	    ((void (*)(void))(loadbuffer + 0x598e0))();
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M30 ret");
	    lcd_puts(0, 1, "M31 next");
	    lcd_update();
	    sleep(HZ / 2);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M31 call");
	    lcd_puts(0, 1, "0801b310");
	    lcd_update();
	    ((void (*)(void))(loadbuffer + 0x1b310))();
	    (void)n3g_main_cfg;
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M31 ret");
	    lcd_puts(0, 1, "M32 next");
	    lcd_update();
	    sleep(HZ / 2);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M32 call");
	    lcd_puts(0, 1, "080671a4");
	    lcd_update();
	    ((void (*)(void))(loadbuffer + 0x671a4))();
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M32 ret");
	    lcd_puts(0, 1, "M33 next");
	    lcd_update();
	    sleep(HZ / 2);
	    volatile uint32_t *n3g_main_state = (volatile uint32_t *)0x080f12b0u;
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M33 call");
	    lcd_puts(0, 1, "0806d948");
	    lcd_update();
	    ((void (*)(uint32_t))(loadbuffer + 0x6d948))
	        (n3g_main_state[9] & 0x7fffffffu);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M33 ret");
	    lcd_puts(0, 1, "M34 next");
	    lcd_update();
	    sleep(HZ / 2);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M34 call");
	    lcd_puts(0, 1, "0804ff68");
	    lcd_update();
	    ((void (*)(void))(loadbuffer + 0x4ff68))();
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M34 ret");
	    lcd_puts(0, 1, "M35 next");
	    lcd_update();
	    sleep(HZ / 2);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M35 call");
	    lcd_puts(0, 1, "0801abc0");
	    lcd_update();
	    ((void (*)(uint32_t))(loadbuffer + 0x1abc0))(1);
	    lcd_clear_display();
	    lcd_puts(0, 0, "N3G_M35 ret");
	    printf("N3G_MAINSTEP_STOP");
	    lcd_puts(0, 1, "mainstep stop");
	    lcd_update();
	    while (1)
	        sleep(HZ);
#endif
	    disable_irq();
    commit_discard_idcache();
    ((void (*)(void))(loadbuffer + 0x5d7c))();
    printf("N3G_MAINRET");
    while (1)
        sleep(HZ);
#endif
#if !defined(IPOD_NANO3G)
    disable_irq();
#endif

    int (*kernel_entry)(void) = (void*)loadbuffer;
    commit_discard_idcache();
    rc = kernel_entry();

    /* End stop - should not get here */
    enable_irq();
#if defined(IPOD_NANO3G)
    printf("N3G_RET rc=%ld", (long)rc);
#endif
    printf("ERR: Failed to boot");
    while(1);
#endif
}
