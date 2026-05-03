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
#define N3G_PAGE_SCAN_COUNT 12

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

#if defined(IPOD_NANO3G)
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

    usec_timer_init();

#ifdef S5L87XX_DEVELOPMENT_BOOTLOADER
    piezo_seq(alive);
#endif

    /* Configure I2C0 */
    i2c_preinit(0);

    if (pmu_is_hibernated()) {
        rc = launch_onb(1); /* 27/2 = 13.5 MHz. */
    }

    system_preinit();
    memory_init();
    /*
     * XXX: BSS is initialized here, do not use .bss before this line
     */
    bss_init();

    system_init();
    kernel_init();
    i2c_init();
    power_init();

    enable_irq();

#ifdef HAVE_SERIAL
    serial_setup();
#endif

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
        if ((btn == BUTTON_MENU)
                || (btn == (BUTTON_SELECT|BUTTON_LEFT))
                || (btn == (BUTTON_SELECT|BUTTON_PLAY))) {
            rc = kernel_launch_onb();
        }
    }

    lcd_init();
    lcd_set_foreground(LCD_WHITE);
    lcd_set_background(LCD_BLACK);
    lcd_clear_display();
    font_init();
    lcd_setfont(FONT_SYSFIXED);

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

    backlight_init(); /* Turns on the backlight */

#ifdef IPOD_NANO3G
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
        for (int i = 0; i < 16; i += 4)
        {
            printf("pg%lu.%02d %08lx %08lx %08lx %08lx",
                   (unsigned long)ndiag.dump_pages[p], i,
                   (unsigned long)ndiag.dump_words[p][i],
                   (unsigned long)ndiag.dump_words[p][i + 1],
                   (unsigned long)ndiag.dump_words[p][i + 2],
                   (unsigned long)ndiag.dump_words[p][i + 3]);
        }
    }
    printf("first_nf %08lx mbr %08lx",
           (unsigned long)ndiag.first_non_ff_page,
           (unsigned long)ndiag.first_mbr_page);
    printf("sec0 rc %ld init %ld nf %lu sig %08lx",
           (long)ndiag.sector0_rc,
           (long)ndiag.sector0_init_rc,
           (unsigned long)ndiag.sector0_non_ff,
           (unsigned long)ndiag.sector0_sig510);
    for (int i = 0; i < 128; i += 4)
    {
        printf("sec0.%03d %08lx %08lx %08lx %08lx", i,
               (unsigned long)ndiag.sector0_words[i],
               (unsigned long)ndiag.sector0_words[i + 1],
               (unsigned long)ndiag.sector0_words[i + 2],
               (unsigned long)ndiag.sector0_words[i + 3]);
    }
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

        rc = storage_init();
        if (rc != 0) {
            printf("Storage error: %d", rc);
            fatal_error(ERR_STORAGE);
        }

        filesystem_init();

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

    printf("Loading Rockbox...");
    unsigned char *loadbuffer = (unsigned char *)DRAM_ORIG;
    rc = load_firmware(loadbuffer, BOOTFILE, MAX_LOADSIZE);

    if (rc <= EFILE_EMPTY) {
        printf("Error!");
        printf("Can't load " BOOTFILE ": ");
        printf(loader_strerror(rc));
        fatal_error(ERR_RB);
    }

    printf("Rockbox loaded.");

    /* If we get here, we have a new firmware image at 0x08000000, run it */
    disable_irq();

    int (*kernel_entry)(void) = (void*)loadbuffer;
    commit_discard_idcache();
    rc = kernel_entry();

    /* End stop - should not get here */
    enable_irq();
    printf("ERR: Failed to boot");
    while(1);
#endif
}
