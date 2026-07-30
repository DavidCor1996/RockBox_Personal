/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 Björn Stenberg
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
#ifdef SIMULATOR
#include <stdlib.h>
#include <stdio.h>
#endif

#include "version.h"
extern const char rbversion[];

#include "gcc_extensions.h"
#include "storage.h"
#include "disk.h"
#include "file.h"
#include "file_internal.h"
#include "lcd.h"
#include "rtc.h"
#include "debug.h"
#include "led.h"
#include "../kernel-internal.h"
#include "button.h"
#include "core_keymap.h"
#include "tree.h"
#include "filetypes.h"
#include "panic.h"
#include "menu.h"
#include "usb.h"
#include "wifi.h"
#include "powermgmt.h"
#include "adc.h"
#include "i2c.h"
#ifndef DEBUG
#include "serial.h"
#endif
#include "audio.h"
#include "settings.h"
#include "backlight.h"
#ifdef IPOD_NANO3G
#include "bringup-nano3g.h"
#endif
#if defined(IPOD_NANO3G) && NANO3G_NATIVE_PRESTOR_ONLY
#include "backlight-target.h"
#endif
#include "status.h"
#include "debug_menu.h"
#include "font.h"
#include "language.h"
#include "wps.h"
#include "playlist.h"
#include "core_alloc.h"
#include "rolo.h"
#include "screens.h"
#include "usb_screen.h"
#include "power.h"
#include "talk.h"
#include "plugin.h"
#ifdef SIMULATOR
#include "open_plugin.h"
#endif
#include "misc.h"
#include "dircache.h"
#ifdef HAVE_TAGCACHE
#include "tagcache.h"
#include "tagtree.h"
#endif
#include "lang.h"
#include "string.h"
#include "splash.h"
#include "eeprom_settings.h"
#include "icon.h"
#include "viewport.h"
#include "skin_engine/skin_engine.h"
#include "statusbar-skinned.h"
#include "bootchart.h"
#include "logdiskf.h"
#include "bootdata.h"
#if defined(HAVE_DEVICEDATA)
#include "devicedata.h"
#endif

#if (CONFIG_PLATFORM & PLATFORM_ANDROID)
#include "notification.h"
#endif
#include "shortcuts.h"

#ifdef IPOD_ACCESSORY_PROTOCOL
#include "iap.h"
#endif

#include "audio_thread.h"
#include "playback.h"
#include "tdspeed.h"
#if defined(HAVE_RECORDING) && !defined(SIMULATOR)
#include "pcm_record.h"
#endif

#ifdef BUTTON_REC
    #define SETTINGS_RESET BUTTON_REC
#elif (CONFIG_KEYPAD == GIGABEAT_PAD)
    #define SETTINGS_RESET BUTTON_A
#endif

#if CONFIG_TUNER
#include "radio.h"
#endif
#if (CONFIG_STORAGE & STORAGE_MMC)
#include "ata_mmc.h"
#endif

#ifdef HAVE_REMOTE_LCD
#include "lcd-remote.h"
#endif

#if CONFIG_USBOTG == USBOTG_ISP1362
#include "isp1362.h"
#endif

#if CONFIG_USBOTG == USBOTG_M5636
#include "m5636.h"
#endif

#ifdef HAVE_HARDWARE_CLICK
#include "piezo.h"
#endif

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
#define MAIN_NORETURN_ATTR NORETURN_ATTR
#else
/* gcc adds an implicit 'return 0;' at the end of main(), causing a warning
 * with noreturn attribute */
#define MAIN_NORETURN_ATTR
#endif

#if (CONFIG_PLATFORM & PLATFORM_HOSTED)
#ifdef HAVE_MULTIVOLUME
#include "pathfuncs.h" /* for init_volume_names */
#endif
#endif

#if (CONFIG_PLATFORM & PLATFORM_SDL)
#ifdef SIMULATOR
#include "sim_tasks.h"
#endif
#include "system-sdl.h"
#define HAVE_ARGV_MAIN
/* Don't use SDL_main on windows -> no more stdio redirection */
#if defined(WIN32)
#undef main
#endif
#endif /* SDL */

/*#define AUTOROCK*/ /* define this to check for "autostart.rock" on boot */

static void init(void);
/* main(), and various functions called by main() and init() may be
 * be INIT_ATTR. These functions must not be called after the final call
 * to root_menu() at the end of main()
 * see definition of INIT_ATTR in config.h */
#ifdef HAVE_ARGV_MAIN
int main(int argc, char *argv[]) INIT_ATTR MAIN_NORETURN_ATTR ;
int main(int argc, char *argv[])
{
    sys_handle_argv(argc, argv);
#else
int main(void) INIT_ATTR MAIN_NORETURN_ATTR;
int main(void)
{
#endif
    CHART(">init");
    init();
    CHART("<init");
#if defined(IPOD_NANO3G) && NANO3G_NATIVE_PRESTOR_ONLY
    /* init() deliberately stops at the visible pre-storage checkpoint. */
    while (1)
        ;
#endif
    FOR_NB_SCREENS(i)
    {
        screens[i].clear_display();
        screens[i].update();
    }
    list_init();
    tree_init();
#if defined(HAVE_DEVICEDATA) && !defined(BOOTLOADER) /* SIMULATOR */
    verify_device_data();
#endif
    /* Keep the order of this 3
     * Must be done before any code uses the multi-screen API */
#ifdef HAVE_USBSTACK
    /* All threads should be created and public queues registered by now */
    usb_start_monitoring();
#endif

#if !defined(DISABLE_ACTION_REMAP) && defined(CORE_KEYREMAP_FILE)
    if (file_exists(CORE_KEYREMAP_FILE))
    {
        int mapct = core_load_key_remap(CORE_KEYREMAP_FILE);
        if (mapct <= 0)
            splashf(HZ, "key remap failed: %d,  %s", mapct, CORE_KEYREMAP_FILE);
    }
#endif

#if !defined(BOOTLOADER)
    allocate_playback_log();
    if (!file_exists(ROCKBOX_DIR"/playername.txt"))
    {
        int fd = open(ROCKBOX_DIR"/playername.txt", O_CREAT|O_WRONLY|O_TRUNC, 0666);
        if(fd >= 0)
        {
            fdprintf(fd, "%s", MODEL_NAME);
            close(fd);
        }
    }
#endif

#ifdef AUTOROCK
    {
        char filename[MAX_PATH];
        const char *file =
#ifdef APPLICATION
                                ROCKBOX_DIR
#else
                                PLUGIN_APPS_DIR
#endif
                                    "/autostart.rock";
        if(file_exists(file)) /* no complaint if it doesn't exist */
        {
            plugin_load(file, NULL); /* start if it does */
        }
    }
#endif /* #ifdef AUTOROCK */

#ifdef SIMULATOR
    {
        const char *sim_plugin = getenv("ROCKBOX_SIM_PLUGIN");
        const char *sim_plugin_param = getenv("ROCKBOX_SIM_PLUGIN_PARAM");

        if (sim_plugin && sim_plugin[0]) {
            int sim_plugin_rc;
            int sim_plugin_loops = 100;
            const char *next_plugin = sim_plugin;
            const char *next_param =
                sim_plugin_param && sim_plugin_param[0] ?
                sim_plugin_param : NULL;
            char next_plugin_buf[MAX_PATH];
            char next_param_buf[MAX_PATH];

            fprintf(stderr, "ROCKBOX_SIM_PLUGIN: %s param=%s\n",
                    next_plugin, next_param ? next_param : "(null)");
            do {
                sim_plugin_rc = plugin_load(next_plugin, next_param);
                if (sim_plugin_rc == PLUGIN_GOTO_PLUGIN) {
                    struct open_plugin_entry_t *entry =
                        open_plugin_get_entry();

                    if (!entry || !entry->path[0])
                        break;
                    strmemccpy(next_plugin_buf, entry->path,
                               sizeof(next_plugin_buf));
                    strmemccpy(next_param_buf, entry->param,
                               sizeof(next_param_buf));
                    next_plugin = next_plugin_buf;
                    next_param = next_param_buf[0] ? next_param_buf : NULL;
                    fprintf(stderr,
                            "ROCKBOX_SIM_PLUGIN chain: %s param=%s\n",
                            next_plugin,
                            next_param ? next_param : "(null)");
                }
            } while (sim_plugin_rc == PLUGIN_GOTO_PLUGIN &&
                     --sim_plugin_loops > 0);
            fprintf(stderr, "ROCKBOX_SIM_PLUGIN rc=%d\n", sim_plugin_rc);
            if (getenv("ROCKBOX_SIM_PLUGIN_EXIT"))
                sys_poweroff();
        }
    }
#endif

    global_status.last_volume_change = 0;
    /* no calls INIT_ATTR functions after this point anymore!
     * see definition of INIT_ATTR in config.h */
    CHART(">root_menu");
    root_menu();
}

/* The disk isn't ready at boot, rblogo is stored in bin and erased after boot */
int show_logo_boot( void ) INIT_ATTR;
int show_logo_boot( void )
{
    unsigned char version[32];
    int font_h, ver_w;
    snprintf(version, sizeof(version), "Ver. %s", rbversion);
    ver_w = font_getstringsize(version, NULL, &font_h, FONT_SYSFIXED);
    (void)ver_w;
    lcd_clear_display();
    lcd_setfont(FONT_SYSFIXED);
#if ((LCD_WIDTH >= 320) && (LCD_HEIGHT >= 240) && (LCD_DEPTH >= 16)) || \
    defined(IPOD_NANO2G)
    lcd_bmp(&bm_rockboxlogo, 0, 0);
#else
#if defined(SANSA_CLIP) || defined(SANSA_CLIPV2) || defined(SANSA_CLIPPLUS)
    /* display the logo in the blue area of the screen (bottom 48 pixels) */
    if (ver_w > LCD_WIDTH)
        lcd_putsxy(0, 0, rbversion);
    else
        lcd_putsxy((LCD_WIDTH/2) - (ver_w/2), 0, version);
    lcd_bmp(&bm_rockboxlogo, (LCD_WIDTH - BMPWIDTH_rockboxlogo) / 2, 16);
#else
    lcd_bmp(&bm_rockboxlogo, (LCD_WIDTH - BMPWIDTH_rockboxlogo) / 2, 10);
    if (ver_w > LCD_WIDTH)
        lcd_putsxy(0, LCD_HEIGHT-font_h, rbversion);
    else
        lcd_putsxy((LCD_WIDTH/2) - (ver_w/2), LCD_HEIGHT-font_h, version);
#endif
#endif
    lcd_setfont(FONT_UI);
    lcd_update();
#ifdef HAVE_REMOTE_LCD
    lcd_remote_clear_display();
    lcd_remote_bmp(&bm_remote_rockboxlogo, 0, 10);
    lcd_remote_setfont(FONT_SYSFIXED);
    if (ver_w > LCD_REMOTE_WIDTH)
        lcd_remote_putsxy(0, LCD_REMOTE_HEIGHT-font_h, rbversion);
    else
        lcd_remote_putsxy((LCD_REMOTE_WIDTH/2) - (ver_w/2),
                      LCD_REMOTE_HEIGHT-font_h, version);
    lcd_remote_setfont(FONT_UI);
    lcd_remote_update();
#endif
#ifdef SIMULATOR
    sleep(HZ); /* sim is too fast to see logo */
#endif
    return 0;
}

#ifdef HAVE_DIRCACHE
static int INIT_ATTR init_dircache(bool preinit)
{
    if (preinit)
        dircache_init(MAX(global_status.dircache_size, 0));

    if (!global_settings.dircache)
        return -1;

    int result = -1;

#ifdef HAVE_EEPROM_SETTINGS
    if (firmware_settings.initialized &&
        firmware_settings.disk_clean &&
        preinit)
    {
        result = dircache_load();
        if (result < 0)
            firmware_settings.disk_clean = false;
    }
    else
#endif /* HAVE_EEPROM_SETTINGS */
    if (!preinit)
    {
        result = dircache_enable();
        if (result != 0)
        {
            if (result > 0)
            {
                /* Print "Scanning disk..." to the display. */
                splash(0, str(LANG_SCANNING_DISK));
                dircache_wait();
                backlight_on();
                show_logo_boot();
            }

            struct dircache_info info;
            dircache_get_info(&info);
            global_status.dircache_size = info.size;
            status_save(true);
        }
        /* else don't wait or already enabled by load */
    }

    return result;
}
#endif /* HAVE_DIRCACHE */

#ifdef HAVE_TAGCACHE
static void init_tagcache(void) INIT_ATTR;
static void init_tagcache(void)
{
    tagcache_init();

    while (!tagcache_is_initialized())
        sleep(HZ/4);
    tagtree_init();
}
#endif /* HAVE_TAGCACHE */

#if (CONFIG_PLATFORM & PLATFORM_HOSTED)

static void init(void)
{
    system_init();
    core_allocator_init();
    kernel_init();
#ifdef APPLICATION
    paths_init();
#endif
    enable_irq();
    lcd_init();
#ifdef HAVE_REMOTE_LCD
    lcd_remote_init();
#endif
    FOR_NB_SCREENS(i)
        global_status.font_id[i] = FONT_SYSFIXED;
    font_init();
    show_logo_boot();
    button_init();
    powermgmt_init();
    backlight_init();
    unicode_init();
#ifdef HAVE_MULTIVOLUME
    init_volume_names();
#endif
#ifdef SIMULATOR
    sim_tasks_init();
#endif
#if (CONFIG_PLATFORM & PLATFORM_ANDROID)
    notification_init();
#endif
    lang_init(core_language_builtin, language_strings,
              LANG_LAST_INDEX_IN_ARRAY);
#ifdef DEBUG
    debug_init();
#endif
#if CONFIG_TUNER
    radio_init();
#endif
    /* Keep the order of this 3 (viewportmanager handles statusbars)
     * Must be done before any code uses the multi-screen API */
    gui_syncstatusbar_init(&statusbars);
    gui_sync_skin_init();
    sb_skin_init();
    viewportmanager_init();

    storage_init();
    pcm_init();
    dsp_init();
    settings_reset();
    settings_load();
    settings_apply(true);
    init_battery_tables();
#ifdef HAVE_DIRCACHE
    init_dircache(true);
    init_dircache(false);
#endif
#ifdef HAVE_TAGCACHE
    init_tagcache();
#endif
    tree_mem_init();
    filetype_init();
    playlist_init();
    shortcuts_init();

    audio_init();
    talk_announce_voice_invalid(); /* notify user w/ voice prompt if voice file invalid */
    settings_apply_skins();

/* do USB last so prompt (if enabled) can work correctly if USB was inserted with device off,
 * also doesn't hurt that it will display the nice pretty backdrop this way too. */
#ifndef USB_NONE
    usb_init();
    usb_start_monitoring();
#endif
}

#else /* ! (CONFIG_PLATFORM & PLATFORM_HOSTED) */

#include "errno.h"

#if defined(IPOD_NANO3G) && NANO3G_NATIVE_STORAGE_PROBE
extern void nano3g_native_storage_trace_reset(void);
extern void nano3g_native_storage_trace_get(unsigned int *details);
#if NANO3G_NATIVE_LANE_PROBE
extern int32_t nano3g_nand_diag_bank_read(uint32_t bank, uint32_t page,
                                          uint32_t *data, uint32_t *extra);
extern int32_t nano3g_nand_diag_scan_map_entries(
                                      const unsigned short *entries,
                                      uint32_t count, uint32_t target_base,
                                      uint32_t result[6]);
static uint32_t nano3g_native_lane_data[0x200] STORAGE_ALIGN_ATTR;
static uint32_t nano3g_native_lane_extra[0x10] STORAGE_ALIGN_ATTR;
#endif

static const unsigned char nano3g_hex_glyph[16][5] =
{
    { 7, 5, 5, 5, 7 }, { 2, 6, 2, 2, 7 },
    { 7, 1, 7, 4, 7 }, { 7, 1, 7, 1, 7 },
    { 5, 5, 7, 1, 1 }, { 7, 4, 7, 1, 7 },
    { 7, 4, 7, 5, 7 }, { 7, 1, 1, 1, 1 },
    { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 },
    { 7, 5, 7, 5, 5 }, { 4, 4, 7, 5, 7 },
    { 7, 4, 4, 4, 7 }, { 1, 1, 7, 5, 7 },
    { 7, 4, 7, 4, 7 }, { 7, 4, 7, 4, 4 },
};

static void nano3g_native_draw_hex(unsigned int value, unsigned int digits,
                                    int x, int y)
{
    const int scale = 4;

    for (unsigned int digit = 0; digit < digits; digit++)
    {
        unsigned int shift = (digits - digit - 1) * 4;
        unsigned int glyph = (value >> shift) & 0xf;

        for (unsigned int row = 0; row < 5; row++)
            for (unsigned int col = 0; col < 3; col++)
                if (nano3g_hex_glyph[glyph][row] & (4u >> col))
                    lcd_fillrect(x + digit * 16 + col * scale,
                                 y + row * scale, scale, scale);
    }
}

static unsigned int nano3g_native_pack_hex4(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16)
         | ((unsigned int)p[2] << 8) | p[3];
}

#if NANO3G_NATIVE_LANE_PROBE
static void nano3g_native_lane_sample(unsigned int sample,
                                      unsigned int bank, unsigned int page,
                                      unsigned int result[7])
{
    unsigned int count = 0;
    unsigned int first;
    int32_t rc;

    memset(nano3g_native_lane_data, 0xff,
           sizeof(nano3g_native_lane_data));
    memset(nano3g_native_lane_extra, 0xff,
           sizeof(nano3g_native_lane_extra));
    rc = nano3g_nand_diag_bank_read(bank, page, nano3g_native_lane_data,
                                    nano3g_native_lane_extra);

    first = nano3g_native_lane_data[0];
    for (unsigned int i = 0; i < ARRAYLEN(nano3g_native_lane_data); i++)
    {
        unsigned int word = nano3g_native_lane_data[i];

        if (word != 0 && word != 0xffffffffu)
        {
            if (count == 0)
                first = word;
            count++;
        }
    }

    result[0] = sample;
    result[1] = bank;
    result[2] = (unsigned int)rc & 0xf;
    result[3] = page & 0x7f;
    result[4] = ((unsigned char *)nano3g_native_lane_extra)[9];
    result[5] = count;
    result[6] = first;
}

static void nano3g_native_lane_probe(void)
{
    unsigned int result[3][7];
    unsigned int oob[3];
    uint32_t scan[6];
    unsigned int map_rc;
    unsigned int map_v;
    unsigned int pblock;
    int32_t scan_rc;
    long last_phase = -1;

    /* Keep the known-live control.  The current context's two consecutive
     * type-0x44 map pointers occupy the bank-0/1 lanes of physical block
     * 0x14c3, page 0x10.  Host BootROM reads return zeros for bank 1, so read
     * that map half through the proven native path.  Its hardware result
     * proved that the map body is pool ordered rather than logically indexed,
     * so reverse-scan its valid vblocks by exact page-zero OOB hyperblock
     * `0x1d1400`.  The already verified FAT boot sector fixes the Disk Mode
     * translation at raw = 0x1407e + 2 * absolute_4K_LBA, making the current
     * file header raw `0x1d1518`.  Sample 1 encodes match count, the high/low
     * pieces of the winning pool index, and its vblock on the first row, then
     * raw LPN and USN.  Sample 2 follows the newest match to lane 0/row 0x23. */
    nano3g_native_lane_sample(0, 0, 0x08cbu * 0x80u, result[0]);
    oob[0] = nano3g_native_lane_extra[0];
    nano3g_native_lane_sample(1, 1, 0x14c3u * 0x80u + 0x10u,
                              result[1]);
    map_rc = result[1][2];
    memset(scan, 0xff, sizeof(scan));
    scan_rc = nano3g_nand_diag_scan_map_entries(
                              (const unsigned short *)nano3g_native_lane_data,
                              0x400u, 0x001d1400u, scan);
    map_v = scan[3];
    result[1][0] = 1;
    result[1][1] = scan[1] & 0xfu;
    result[1][2] = map_rc != 0 ? map_rc : (unsigned int)scan_rc & 0xfu;
    result[1][3] = (scan[2] >> 8) & 0xffu;
    result[1][4] = scan[2] & 0xffu;
    result[1][5] = map_v & 0xfffu;
    oob[1] = scan[4];
    result[1][6] = scan[5];

    if (scan[1] == 0 || map_v == 0 || map_v == 0xffffu)
        pblock = 0x0788u;
    else
        pblock = ((map_v & 0xfffu) + 0x1a9u) & ~1u;
    nano3g_native_lane_sample(2, 0, pblock * 0x80u + 0x23u,
                              result[2]);
    oob[2] = nano3g_native_lane_extra[0];

    while (1)
    {
        long phase = (current_tick / (HZ / 2)) & 1;

        if (phase != last_phase)
        {
            lcd_set_background(LCD_BLACK);
            lcd_set_foreground(LCD_BLACK);
            lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
            lcd_set_foreground(LCD_WHITE);

            for (unsigned int i = 0; i < ARRAYLEN(result); i++)
            {
                int y = i * 65;

                /* sample, bank, rc, page offset, OOB type, nontrivial words */
                nano3g_native_draw_hex(result[i][0], 1, 0, y);
                nano3g_native_draw_hex(result[i][1], 1, 18, y);
                nano3g_native_draw_hex(result[i][2], 1, 36, y);
                nano3g_native_draw_hex(result[i][3], 2, 54, y);
                nano3g_native_draw_hex(result[i][4], 2, 88, y);
                nano3g_native_draw_hex(result[i][5], 3, 122, y);
                nano3g_native_draw_hex(oob[i], 8, 2, y + 21);
                nano3g_native_draw_hex(result[i][6], 8, 2, y + 42);
            }

            if (phase)
                lcd_fillrect(0, LCD_HEIGHT - 20, LCD_WIDTH, 20);
            lcd_update();
            last_phase = phase;
        }
        sleep(1);
    }
}
#endif

void nano3g_native_storage_probe_stage(unsigned int stage)
{
    unsigned color;

    /*
     * NAND initialization currently leaves the Nano 3G LCD DMA completion
     * path unable to survive a series of diagnostic refreshes.  Render only
     * the selected one-shot checkpoint so earlier probes cannot stop FTL
     * execution merely by reporting progress.
     */
    if (stage != NANO3G_NATIVE_STORAGE_SCREEN_STAGE)
        return;

    switch (stage)
    {
        case 1: color = LCD_RGBPACK(255, 255, 255); break;
        case 2: color = LCD_RGBPACK(0, 255, 255); break;
        case 3: color = LCD_RGBPACK(0, 0, 255); break;
        case 4: color = LCD_RGBPACK(255, 0, 255); break;
        case 5: color = LCD_RGBPACK(255, 128, 0); break;
        case 6: color = LCD_RGBPACK(0, 128, 128); break;
        case 7: color = LCD_RGBPACK(128, 128, 128); break;
        case 8: color = LCD_RGBPACK(128, 255, 0); break;
        case 9: color = LCD_RGBPACK(255, 128, 192); break;
        case 10: color = LCD_RGBPACK(128, 64, 0); break;
        case 11: color = LCD_RGBPACK(128, 128, 0); break;
        case 12: color = LCD_RGBPACK(0, 0, 128); break;
        case 13: color = LCD_RGBPACK(128, 0, 128); break;
        case 14: color = LCD_RGBPACK(0, 128, 255); break;
        case 15: color = LCD_RGBPACK(128, 255, 128); break;
        case 16: color = LCD_RGBPACK(192, 192, 192); break;
        default: color = LCD_RGBPACK(255, 0, 0); break;
    }

    lcd_set_foreground(color);
    lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    lcd_update();
}
#endif

static void init(void) INIT_ATTR;
static void init(void)
{
#if defined(IPOD_NANO3G) && NANO3G_NATIVE_PRESTOR_ONLY
    /*
     * Keep this transient DFU image small enough for the S5L8720 haxed-DFU
     * staging window.  This is still the normal Rockbox application crt0 and
     * main(), but deliberately retains only the proven pre-storage path.
     */
    system_init();
    kernel_init();
    i2c_init();

    lcd_set_viewport(NULL);
    lcd_init_device();
    lcd_set_background(LCD_BLACK);
    lcd_clear_display();
    (void)backlight_hw_init();

    /* Three solid bands are the native-application handoff checkpoint. */
    lcd_set_foreground(LCD_RGBPACK(255, 0, 0));
    lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT / 3);
    lcd_set_foreground(LCD_RGBPACK(0, 255, 0));
    lcd_fillrect(0, LCD_HEIGHT / 3, LCD_WIDTH, LCD_HEIGHT / 3);
    lcd_set_foreground(LCD_RGBPACK(0, 0, 255));
    lcd_fillrect(0, 2 * (LCD_HEIGHT / 3), LCD_WIDTH,
                 LCD_HEIGHT - 2 * (LCD_HEIGHT / 3));
    lcd_update();

#if NANO3G_NATIVE_INPUT_PROBE
    /*
     * Advance the native handoff checkpoint through the kernel tick and the
     * S5L8702 click-wheel interrupt path without touching storage.  Yellow
     * proves the tick IRQ completed; cyan plus a blinking bottom marker proves
     * click-wheel initialization returned and the tick remains live.
     */
    enable_irq();
    enable_fiq();
    sleep(HZ);

    lcd_set_foreground(LCD_RGBPACK(255, 255, 0));
    lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    lcd_update();

    button_init_device();

    extern int new_wheel_value;
    extern bool wheel_is_touched;
    int last_mode = -1;
    int last_wheel = -1;
    int wheel_direction = 0;
    long wheel_color_until = 0;
    long last_phase = -1;
    while (1)
    {
        int button = button_read_device();
        int wheel = new_wheel_value;
        int mode = button;
        long phase = (current_tick / (HZ / 2)) & 1;

        if (wheel_is_touched)
        {
            if (last_wheel >= 0 && wheel != last_wheel)
            {
                int delta = wheel - last_wheel;

                if (delta < -48)
                    delta += 96;
                else if (delta > 48)
                    delta -= 96;

                wheel_direction = delta > 0 ? 1 : -1;
                wheel_color_until = current_tick + HZ / 2;
            }
            last_wheel = wheel;
        }
        else
        {
            last_wheel = -1;
        }

        if (wheel_direction != 0
            && TIME_BEFORE(current_tick, wheel_color_until))
            mode = wheel_direction > 0 ? -2 : -3;

        if (mode != last_mode || phase != last_phase)
        {
            unsigned color = LCD_RGBPACK(0, 255, 255);

            if (mode == -2)
                color = LCD_RGBPACK(255, 255, 0);
            else if (mode == -3)
                color = LCD_RGBPACK(255, 128, 0);
            else if (button & BUTTON_SELECT)
                color = LCD_RGBPACK(255, 255, 255);
            else if (button & BUTTON_MENU)
                color = LCD_RGBPACK(255, 0, 0);
            else if (button & BUTTON_LEFT)
                color = LCD_RGBPACK(0, 0, 255);
            else if (button & BUTTON_RIGHT)
                color = LCD_RGBPACK(0, 255, 0);
            else if (button & BUTTON_PLAY)
                color = LCD_RGBPACK(255, 0, 255);
            lcd_set_foreground(color);
            lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
            lcd_set_foreground(phase ? LCD_WHITE : LCD_BLACK);
            lcd_fillrect(0, LCD_HEIGHT - 20, LCD_WIDTH, 20);
            lcd_update();

            last_mode = mode;
            last_phase = phase;
        }

        sleep(1);
    }
#endif

#if NANO3G_NATIVE_STORAGE_PROBE
    /*
     * Native read-only storage checkpoint. Nano 3G NAND program/erase and
     * sector-write entry points remain hard stubs that return failure.
     * Yellow means storage_init() was entered, green means success, and red
     * means it returned an error. A blinking bottom bar proves it returned.
     */
    enable_irq();
    enable_fiq();
    sleep(HZ);

    lcd_set_foreground(LCD_RGBPACK(255, 255, 0));
    lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
    lcd_update();

#if NANO3G_NATIVE_LANE_PROBE
    /* Bypass the FTL mount for this bounded, read-only lane survey. */
    nano3g_native_lane_probe();
#endif

    int storage_rc = storage_init();
    int mount_rc = -1;
    unsigned root_file_result = 0;
    unsigned dot_file_result = 0;
    unsigned char root_header[8] = { 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff };
    unsigned char dot_header[8] = { 0xff, 0xff, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xff };
    unsigned char root_chunk[0x200];
    uint32_t root_bytes = 0;
    uint32_t root_expected_checksum = 0xffffffffu;
    uint32_t root_computed_checksum = 117u;
    unsigned root_trace[9] = { 0, 0xffffffff, 0xffffffff, 0xffff,
                               0xffffffff, 0xffffffff, 0xffffffff,
                               0xffffffff, 0xffffffff };
    unsigned dot_trace[9] = { 0, 0xffffffff, 0xffffffff, 0xffff,
                              0xffffffff, 0xffffffff, 0xffffffff,
                              0xffffffff, 0xffffffff };

    if (storage_rc == 0)
    {
        filesystem_init();
        mount_rc = disk_mount_all();

        if (mount_rc > 0)
        {
            int fd;

            nano3g_native_storage_trace_reset();
            fd = open("/rockbox.ipod", O_RDONLY);
            if (fd < 0)
                root_file_result = 1;
            else
            {
                ssize_t got;

                while ((got = read(fd, root_chunk, sizeof(root_chunk))) > 0)
                {
                    for (ssize_t i = 0; i < got; i++)
                    {
                        uint32_t offset = root_bytes + (uint32_t)i;

                        if (offset < sizeof(root_header))
                            root_header[offset] = root_chunk[i];
                        else
                            root_computed_checksum += root_chunk[i];
                    }
                    root_bytes += (uint32_t)got;
                }

                if (root_bytes >= sizeof(root_header))
                    root_expected_checksum =
                          ((uint32_t)root_header[0] << 24)
                        | ((uint32_t)root_header[1] << 16)
                        | ((uint32_t)root_header[2] << 8)
                        | (uint32_t)root_header[3];

                if (got < 0)
                    root_file_result = 2;
                else if (root_bytes == 854812u
                      && root_header[4] == 'n' && root_header[5] == 'n'
                      && root_header[6] == '3' && root_header[7] == 'g'
                      && root_computed_checksum == root_expected_checksum)
                    root_file_result = 3;
                else
                    root_file_result = 6;
                close(fd);
            }
            nano3g_native_storage_trace_get(root_trace);

            nano3g_native_storage_trace_reset();
            fd = open("/.rockbox/rockbox.ipod", O_RDONLY);
            if (fd < 0)
                dot_file_result = 1;
            else
            {
                dot_file_result = read(fd, dot_header, sizeof(dot_header))
                                == (ssize_t)sizeof(dot_header) ? 3 : 2;
                close(fd);
            }
            nano3g_native_storage_trace_get(dot_trace);
        }
    }

    if (storage_rc != 0)
    {
        root_file_result = 4;
        dot_file_result = 4;
    }
    else if (mount_rc <= 0)
    {
        root_file_result = 5;
        dot_file_result = 5;
    }

    long storage_phase = -1;

    while (1)
    {
        long phase = (current_tick / (HZ / 2)) & 1;

        if (phase != storage_phase)
        {
            lcd_set_foreground(LCD_BLACK);
            lcd_fillrect(0, 0, LCD_WIDTH, LCD_HEIGHT);
            lcd_set_foreground(LCD_WHITE);
            nano3g_native_draw_hex(root_file_result, 1, 10, 2);
            nano3g_native_draw_hex(dot_file_result, 1, 42, 2);
            nano3g_native_draw_hex(root_bytes, 8, 10, 27);
            nano3g_native_draw_hex(root_expected_checksum, 8, 10, 52);
            nano3g_native_draw_hex(root_computed_checksum, 8, 10, 77);
            lcd_fillrect(0, 105, LCD_WIDTH, 2);
            nano3g_native_draw_hex(nano3g_native_pack_hex4(root_header),
                                   8, 10, 112);
            nano3g_native_draw_hex(nano3g_native_pack_hex4(root_header + 4),
                                   8, 10, 137);
            nano3g_native_draw_hex(root_trace[0], 1, 10, 162);
            nano3g_native_draw_hex(dot_trace[0], 1, 42, 162);

            if (phase)
                lcd_fillrect(0, LCD_HEIGHT - 20, LCD_WIDTH, 20);
            lcd_update();
            storage_phase = phase;
        }

        sleep(1);
    }
#endif

#if NANO3G_NATIVE_DRAM_PERSIST_PROBE
    /*
     * Unlike the bare IRAM probe, this runs after the proven native crt0 and
     * system initialization path.  Clean the marker from D-cache before the
     * watchdog reset so a subsequent DFU session can test DRAM retention.
     */
    volatile uint32_t *marker = (volatile uint32_t *)0x08100000;
    marker[0] = 0x33475244; /* "DRG3" */
    marker[1] = 0x53524550; /* "PERS" */
    marker[2] = 0x53545349; /* "ISTS" */
    marker[3] = 0x214b4f3f; /* "?OK!" */
    commit_dcache_range((const void *)marker, 16);
    system_reboot();
#endif

    while (1)
        ;
#else
    int rc;
    bool mounted = false;

    system_init();
    core_allocator_init();
    kernel_init();

#if defined(HAVE_BOOTDATA) && !defined(BOOTLOADER)
    verify_boot_data();
#endif

#if defined(HAVE_DEVICEDATA) && !defined(BOOTLOADER)
    verify_device_data();
#endif

    /* early early early! */
    filesystem_init();

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
#if !defined(IPOD_NANO3G) || !NANO3G_NATIVE_SAFE_BOOT
    set_cpu_frequency(CPUFREQ_NORMAL);
#ifdef CPU_COLDFIRE
    coldfire_set_pllcr_audio_bits(DEFAULT_PLLCR_AUDIO_BITS);
#endif
    cpu_boost(true);
#endif
#endif

    i2c_init();

#if !defined(IPOD_NANO3G) || !NANO3G_NATIVE_SAFE_BOOT
    power_init();
#endif

    enable_irq();
#if defined(CPU_ARM_CLASSIC)
    enable_fiq();
#endif
    /* current_tick should be ticking by now */
    CHART("ticking");

    unicode_init();
    lcd_init();
#ifdef HAVE_REMOTE_LCD
    lcd_remote_init();
#endif
    FOR_NB_SCREENS(i)
        global_status.font_id[i] = FONT_SYSFIXED;
    font_init();

    settings_reset();

    CHART(">show_logo");
    show_logo_boot();
    CHART("<show_logo");
    lang_init(core_language_builtin, language_strings,
              LANG_LAST_INDEX_IN_ARRAY);

#ifdef DEBUG
    debug_init();
#else
#ifdef HAVE_SERIAL
    serial_setup();
#endif
#endif

#if CONFIG_RTC
    rtc_init();
#endif

    adc_init();

    usb_init();
#if CONFIG_USBOTG == USBOTG_ISP1362
    isp1362_init();
#elif CONFIG_USBOTG == USBOTG_M5636
    m5636_init();
#endif

    backlight_init();

    button_init();

    /* Don't initialize power management here if it could incorrectly
     * measure battery voltage, and it's not needed for charging. */
#if !defined(NEED_ATA_POWER_BATT_MEASURE) || \
    (CONFIG_CHARGING > CHARGING_MONITOR)
#if !defined(IPOD_NANO3G) || !NANO3G_NATIVE_SAFE_BOOT
    powermgmt_init();
#endif
#endif

#if CONFIG_TUNER
    radio_init();
#endif

#ifdef HAVE_HARDWARE_CLICK
    piezo_init();
#endif

    /* Keep the order of this 3 (viewportmanager handles statusbars)
     * Must be done before any code uses the multi-screen API */
    CHART(">gui_syncstatusbar_init");
    gui_syncstatusbar_init(&statusbars);
    CHART("<gui_syncstatusbar_init");
    CHART(">sb_skin_init");
    sb_skin_init();
    CHART("<sb_skin_init");
    CHART(">gui_sync_wps_init");
    gui_sync_skin_init();
    CHART("<gui_sync_wps_init");
    CHART(">viewportmanager_init");
    viewportmanager_init();
    CHART("<viewportmanager_init");

    CHART(">storage_init");
    rc = storage_init();
    CHART("<storage_init");
    if(rc)
    {
        lcd_clear_display();
        lcd_putsf(0, 1, "ATA error: %d", rc);
        lcd_puts(0, 3, "Press button to debug");
        lcd_update();
        while(!(button_get(true) & BUTTON_REL)); /* DO NOT CHANGE TO ACTION SYSTEM */
        dbg_ports();
        panicf("ata: %d", rc);
    }

#if defined(IPOD_NANO3G) && NANO3G_NATIVE_SAFE_BOOT
    nano3g_boottrace_log("storage_init ok");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G FULL STORAGE OK");
    lcd_puts(0, 1, "continuing init");
    lcd_update();
    sleep(HZ);
#endif

#if defined(NEED_ATA_POWER_BATT_MEASURE) && \
    (CONFIG_CHARGING <= CHARGING_MONITOR)
    /* After storage_init(), ATA power must be on, so battery voltage
     * can be measured. Initialize power management if it was delayed. */
#if !defined(IPOD_NANO3G) || !NANO3G_NATIVE_SAFE_BOOT
    powermgmt_init();
#endif
#endif
#ifdef HAVE_EEPROM_SETTINGS
    CHART(">eeprom_settings_init");
    eeprom_settings_init();
    CHART("<eeprom_settings_init");
#endif

#ifndef HAVE_USBSTACK
    usb_start_monitoring();
    while (usb_detect() == USB_INSERTED)
    {
#ifdef HAVE_EEPROM_SETTINGS
        firmware_settings.disk_clean = false;
#endif
        /* enter USB mode early, before trying to mount */
        if (button_get_w_tmo(HZ/10) == SYS_USB_CONNECTED)
#if (CONFIG_STORAGE & STORAGE_MMC)
            if (!mmc_touched() ||
                (mmc_remove_request() == SYS_HOTSWAP_EXTRACTED))
#endif
            {
                gui_usb_screen_run(true, button_get_data());
                mounted = true; /* mounting done @ end of USB mode */
            }
#ifdef HAVE_USB_POWER
        /* if there is no host or user requested no USB, skip this */
        if (usb_powered_only())
            break;
#endif
    }
#endif

    if (!mounted)
    {
        CHART(">disk_mount_all");
        rc = disk_mount_all();
        CHART("<disk_mount_all");
        if (rc<=0)
        {
            int line=0;
            lcd_clear_display();
            lcd_putsf(0, line++, "No partition found (%d).", rc);
#ifndef USB_NONE
            lcd_puts(0, line++, "Insert USB cable");
            lcd_puts(0, line++, "and fix it.");
#elif !defined(DEBUG) && !(CONFIG_STORAGE & STORAGE_RAMDISK)
            lcd_puts(0, line++, "Rebooting in 5s");
#endif
            lcd_puts(0, line++, rbversion);

#ifdef STORAGE_GET_INFO
            struct storage_info sinfo;
            storage_get_info(0, &sinfo);
#ifdef MAX_PHYS_SECTOR_SIZE
            lcd_putsf(0, line++, "id: '%s' s:%u*%u", sinfo.product, sinfo.sector_size, sinfo.phys_sector_mult);
#else
            lcd_putsf(0, line++, "id: '%s' s:%u", sinfo.product, sinfo.sector_size);
#endif
#endif
            struct partinfo pinfo;
            for (int i = 0 ; i < NUM_VOLUMES ; i++) {
                disk_partinfo(i, &pinfo);
                if (pinfo.type)
                    lcd_putsf(0, line++, "P%d T%02x S%llx",
                              i, pinfo.type, (unsigned long long)pinfo.size);
            }
            lcd_update();

#if defined(MAX_VIRT_SECTOR_SIZE) && defined(DEFAULT_VIRT_SECTOR_SIZE)
#ifdef HAVE_MULTIDRIVE
            for (int i = 0 ; i < NUM_DRIVES ; i++)
#endif
                disk_set_sector_multiplier(IF_MD(i,) DEFAULT_VIRT_SECTOR_SIZE/SECTOR_SIZE);
#endif

#ifndef USB_NONE
            usb_start_monitoring();
            while(button_get(true) != SYS_USB_CONNECTED) {};
            gui_usb_screen_run(true, button_get_data());
#elif !defined(DEBUG) && !(CONFIG_STORAGE & STORAGE_RAMDISK)
            sleep(HZ*5);
#endif

#if !defined(DEBUG) && !(CONFIG_STORAGE & STORAGE_RAMDISK)
            system_reboot();
#else
            rc = disk_mount_all();
            if (rc <= 0) {
                lcd_putsf(0, 4, "Error mounting: %08x", rc);
                lcd_update();
                sleep(HZ*5);
                system_reboot();
            }
#endif
        }
    }

    pcm_init();
    dsp_init();

    CHART(">settings_load");
    settings_load();
    CHART("<settings_load");

#if defined(BUTTON_REC) || \
    (CONFIG_KEYPAD == GIGABEAT_PAD) || \
    (CONFIG_KEYPAD == IPOD_4G_PAD) || \
    (CONFIG_KEYPAD == IRIVER_H10_PAD)
    if (global_settings.clear_settings_on_hold &&
#ifdef SETTINGS_RESET
    /* Reset settings if holding the reset button. (Rec on Archos,
       A on Gigabeat) */
    ((button_status() & SETTINGS_RESET) == SETTINGS_RESET))
#else
    /* Reset settings if the hold button is turned on */
    (button_hold()))
#endif
    {
        splash(HZ*2, str(LANG_RESET_DONE_CLEAR));
        settings_reset();
    }
#endif
    CHART(">init_battery_tables");
    init_battery_tables();
    CHART("<init_battery_tables");
#ifdef HAVE_DIRCACHE
    CHART(">init_dircache(true)");
    rc = init_dircache(true);
    CHART("<init_dircache(true)");
#ifdef HAVE_TAGCACHE
    if (rc < 0)
        tagcache_remove_statefile();
#endif /* HAVE_TAGCACHE */
#endif /* HAVE_DIRCACHE */

    CHART(">settings_apply(true)");
    settings_apply(true);
    CHART("<settings_apply(true)");
#ifdef HAVE_DIRCACHE
    CHART(">init_dircache(false)");
    init_dircache(false);
    CHART("<init_dircache(false)");
#endif
#ifdef HAVE_TAGCACHE
    CHART(">init_tagcache");
    init_tagcache();
    CHART("<init_tagcache");
#endif

#ifdef HAVE_EEPROM_SETTINGS
    if (firmware_settings.initialized)
    {
        /* In case we crash. */
        firmware_settings.disk_clean = false;
        CHART(">eeprom_settings_store");
        eeprom_settings_store();
        CHART("<eeprom_settings_store");
    }
#endif
    playlist_init();
    tree_mem_init();
    filetype_init();

    shortcuts_init();

    CHART(">audio_init");
    audio_init();
    CHART("<audio_init");
    talk_announce_voice_invalid(); /* notify user w/ voice prompt if voice file invalid */

#ifdef HAVE_WIFI
    wifi_init();
#endif

    /* runtime database has to be initialized after audio_init() */
#if !defined(IPOD_NANO3G) || !NANO3G_NATIVE_SAFE_BOOT
    cpu_boost(false);
#endif

#if CONFIG_CHARGING
    car_adapter_mode_init();
#endif
#ifdef IPOD_ACCESSORY_PROTOCOL
    iap_setup(global_settings.serial_bitrate);
#endif
#ifdef HAVE_ACCESSORY_SUPPLY
    accessory_supply_set(global_settings.accessory_supply);
#endif
#ifdef HAVE_LINEOUT_POWEROFF
    lineout_set(global_settings.lineout_active);
#endif
#ifdef HAVE_HOTSWAP_STORAGE_AS_MAIN
    CHART("<check_bootfile(false)");
    check_bootfile(false); /* remember write time and filesize */
    CHART(">check_bootfile(false)");
#endif
    CHART("<settings_apply_skins");
    settings_apply_skins();
    CHART(">settings_apply_skins");
#if defined(IPOD_NANO3G) && NANO3G_NATIVE_SAFE_BOOT
    nano3g_boottrace_log("full init ok");
    lcd_clear_display();
    lcd_puts(0, 0, "N3G FULL INIT OK");
    lcd_puts(0, 1, "entering Rockbox");
    lcd_update();
    sleep(HZ);
#endif
#endif /* IPOD_NANO3G && NANO3G_NATIVE_PRESTOR_ONLY */
}

#ifdef CPU_PP
void cop_main(void) MAIN_NORETURN_ATTR;
void cop_main(void)
{
/* This is the entry point for the coprocessor
   Anyone not running an upgraded bootloader will never reach this point,
   so it should not be assumed that the coprocessor be usable even on
   platforms which support it.

   A kernel thread is initially setup on the coprocessor and immediately
   destroyed for purposes of continuity. The cop sits idle until at least
   one thread exists on it. */

#if NUM_CORES > 1
    system_init();
    kernel_init();
    /* This should never be reached */
#endif
    while(1) {
        sleep_core(COP);
    }
}
#endif /* CPU_PP */

#endif /* CONFIG_PLATFORM & PLATFORM_HOSTED */
