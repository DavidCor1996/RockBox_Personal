/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * LCD driver for iPod Video
 *
 * Based on code from the ipodlinux project - http://ipodlinux.org/
 * Adapted for Rockbox in December 2005
 *
 * Original file: linux/arch/armnommu/mach-ipod/fb.c
 *
 * Copyright (c) 2003-2005 Bernard Leach (leachbj@bouncycastle.org)
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

#include <sys/types.h> /* off_t */
#include "config.h"
#include "cpu.h"
#include "lcd.h"
#include "kernel.h"
#include "system.h"
#include "bcm2722.h"
#ifdef HAVE_LCD_SLEEP
/* Included only for lcd_awake() prototype */
#include "backlight-target.h"
#endif

/* The BCM bus width is 16 bits. But since the low address bits aren't decoded
 * by the chip (the 3 BCM address bits are mapped to address bits 16..18 of the
 * PP5022), writing 32 bits (and even more, using 'stmia') at once works. */
#define BCM_DATA      (*(volatile unsigned short*)(0x30000000))
#define BCM_DATA32    (*(volatile unsigned long *)(0x30000000))
#define BCM_WR_ADDR   (*(volatile unsigned short*)(0x30010000))
#define BCM_WR_ADDR32 (*(volatile unsigned long *)(0x30010000))
#define BCM_RD_ADDR   (*(volatile unsigned short*)(0x30020000))
#define BCM_RD_ADDR32 (*(volatile unsigned long *)(0x30020000))
#define BCM_CONTROL   (*(volatile unsigned short*)(0x30030000))

#define BCM_ALT_DATA      (*(volatile unsigned short*)(0x30040000))
#define BCM_ALT_DATA32    (*(volatile unsigned long *)(0x30040000))
#define BCM_ALT_WR_ADDR   (*(volatile unsigned short*)(0x30050000))
#define BCM_ALT_WR_ADDR32 (*(volatile unsigned long *)(0x30050000))
#define BCM_ALT_RD_ADDR   (*(volatile unsigned short*)(0x30060000))
#define BCM_ALT_RD_ADDR32 (*(volatile unsigned long *)(0x30060000))
#define BCM_ALT_CONTROL   (*(volatile unsigned short*)(0x30070000))

/* Time until the BCM is considered stalled and will be re-kicked.
 * Must be guaranteed to be >~ 20ms. */
#define BCM_UPDATE_TIMEOUT (HZ/20)
/* An LCD update command done while the LCD is off needs >~ 200ms */
#define BCM_LCDINIT_TIMEOUT (HZ/2)
#define BCM_PLAYER_BUS_TIMEOUT  (HZ * 2)
#define BCM_PLAYER_START_TIMEOUT (HZ * 10)

/* Addresses within BCM */
#define BCMA_SRAM_BASE   0
#define BCMA_COMMAND     0x1F8
#define BCMA_STATUS      0x1FC
#define BCMA_CMDPARAM    0xE0000    /* Parameters/data for commands */
#define BCMA_SDRAM_BASE  0xC0000000
#define BCMA_TV_FB       0xC0000000 /* TV out framebuffer */
#define BCMA_TV_BMPDATA  0xC0200000 /* BMP data for TV out functions */

/* BCM commands.  Write them to BCMA_COMMAND.  Note BCM_CMD encoding. */
#define BCM_CMD(x) ((~((unsigned long)x) << 16) | ((unsigned long)x))
#define BCMCMD_LCD_UPDATE     BCM_CMD(0)
/* Execute "M25 Diagnostics".  Status displayed on LCD.  Takes <40s */
#define BCMCMD_SELFTEST       BCM_CMD(1)
#define BCMCMD_TV_PALBMP      BCM_CMD(2)
#define BCMCMD_TV_NTSCBMP     BCM_CMD(3)
/* BCM_CMD(4) may be another TV-related command */
/* The following might do more depending on word at 0xE00000 */
#define BCMCMD_LCD_UPDATERECT BCM_CMD(5)
#define BCMCMD_LCD_SLEEP      BCM_CMD(8)
/* BCM_CMD(12) involved in shutdown */
/* Macrovision analog copy prevention is on by default on TV output.
   Execute this command after enabling TV out to turn it off.
 */
#define BCMCMD_TV_MVOFF       BCM_CMD(14)

enum lcd_status
{
    LCD_IDLE,
    LCD_INITIAL,
    LCD_NEED_UPDATE,
    LCD_UPDATING
};

struct
{
    long update_timeout;  /* also used to ensure BCM stays off for >= 50 ms */
    enum lcd_status state;
    bool blocked;
#if NUM_CORES > 1
    struct corelock cl;   /* inter-core sync */
#endif
#ifdef HAVE_LCD_SLEEP
    bool display_on;
    bool waking;
    struct semaphore initwakeup;
#endif
} lcd_state IBSS_ATTR;

#ifndef BOOTLOADER
static bool bcm2722_player_active;
static bool bcm2722_player_fault;
static const char *bcm2722_player_error = "VideoCore not started";
static enum bcm2722_video_stage bcm2722_player_stage =
    BCM2722_VIDEO_STAGE_OFF;

/* Resident-VMCS composite policy.  Values intentionally match the shared
 * Off/Auto/On setting without making the target driver depend on apps/. */
#define BCM_VIDEOOUT_OFF             0
#define BCM_VIDEOOUT_AUTO            1
#define BCM_VIDEOOUT_ON              2
#define BCM_TV_NTSC_WIDTH          LCD_WIDTH
#define BCM_TV_NTSC_HEIGHT         LCD_HEIGHT
#define BCM_TV_NTSC_ROW_BYTES     (BCM_TV_NTSC_WIDTH * 3)
#define BCM_TV_NTSC_IMAGE_BYTES   (BCM_TV_NTSC_ROW_BYTES * \
                                   BCM_TV_NTSC_HEIGHT)
#define BCM_TV_NTSC_FILE_BYTES    (54u + BCM_TV_NTSC_IMAGE_BYTES + 2u)
#define BCM_TV_CACHE_FLUSH_LINES   20
/* Match the physically qualified 6G mirror's centered 90% NTSC safe area.
 * The resident 5G VMCS exposes no scaler, so this path scales in the small
 * target-owned line workspace before streaming the row to VideoCore. */
#define BCM_TV_SAFE_X              16
#define BCM_TV_SAFE_Y              12
#define BCM_TV_SAFE_WIDTH         288
#define BCM_TV_SAFE_HEIGHT        216
#define BCM_TV_MAP_INVALID_X   0xffffu
#define BCM_TV_MAP_INVALID_Y     0xffu

static int bcm2722_videoout_mode = BCM_VIDEOOUT_OFF;
static bool bcm2722_videoout_signal_active;
static bool bcm2722_videoout_updating;
static long bcm2722_videoout_next_update;
static unsigned long bcm2722_videoout_zero[64] CACHEALIGN_ATTR;
static unsigned long bcm2722_videoout_line[BCM_TV_NTSC_ROW_BYTES /
                                           sizeof(unsigned long)]
                                           CACHEALIGN_ATTR;
static unsigned short bcm2722_videoout_xmap[BCM_TV_NTSC_WIDTH];
static unsigned char bcm2722_videoout_ymap[BCM_TV_NTSC_HEIGHT];
static bool bcm2722_videoout_maps_ready;

static void bcm2722_videoout_mirror_if_due(bool force, int x, int y,
                                            int width, int height);
#endif

#ifdef HAVE_LCD_SLEEP
const fb_data *flash_vmcs_offset;
unsigned flash_vmcs_length;

#define ROM_BASE        0x20000000
#define ROM_ID(a,b,c,d) (unsigned int)(  ((unsigned int)(d))        | \
                                        (((unsigned int)(c)) << 8)  | \
                                        (((unsigned int)(b)) << 16) | \
                                        (((unsigned int)(a)) << 24) )

/* Get address and length of iPod flash section.
   Based on part of FS#6721.  This may belong elsewhere.
   (BCM initialization uploads the vmcs section to the BCM.)
 */
static bool flash_get_section(const unsigned int imageid,
                              void **offset,
                              unsigned int *length)
{
    unsigned long *p = (unsigned long*)(ROM_BASE + 0xffe00);
    unsigned char *csp, *csend;
    unsigned long checksum;

    /* Find the image in the directory */
    while (1)
    {
        if (p[0] != ROM_ID('f','l','s','h'))
            return false;
        if (p[1] == imageid)
            break;
        p += 10;
    }

    *offset = (void *)(ROM_BASE + p[3]);
    *length = p[4];

    /* Verify checksum.  Probably unnecessary, but it's fast. */
    checksum = 0;
    csend = (unsigned char *)(ROM_BASE + p[3] + p[4]);
    for(csp = (unsigned char *)(ROM_BASE + p[3]); csp < csend; csp++)
    {
        checksum += *csp;
    }

    return checksum == p[7];
}
#endif /* HAVE_LCD_SLEEP */

static inline void bcm_write_addr(unsigned address)
{
    BCM_WR_ADDR32 = address;       /* write destination address */

    while (!(BCM_CONTROL & 0x2));  /* wait for it to be write ready */
}

static inline void bcm_write32(unsigned address, unsigned value)
{

    bcm_write_addr(address);       /* set destination address */

    BCM_DATA32 = value;            /* write value */
}

static inline unsigned bcm_read32(unsigned address)
{
    while (!(BCM_RD_ADDR & 1));

    BCM_RD_ADDR32 = address;       /* write source address */

    while (!(BCM_CONTROL & 0x10)); /* wait for it to be read ready */

    return BCM_DATA32;             /* read value */
}

#ifdef HAVE_LCD_SLEEP
static void continue_lcd_awake(void)
{
    lcd_state.waking = false;
    semaphore_release(&(lcd_state.initwakeup));
}
#endif

#ifndef BOOTLOADER
static void lcd_tick(void)
{
    /* No core level interrupt mask - already in interrupt context */
#if NUM_CORES > 1
    corelock_lock(&lcd_state.cl);
#endif

    if (!lcd_state.blocked && lcd_state.state >= LCD_NEED_UPDATE)
    {
        unsigned data = bcm_read32(BCMA_COMMAND);
        bool bcm_is_busy = (data == BCMCMD_LCD_UPDATE || data == 0xFFFF);

        if (((lcd_state.state == LCD_NEED_UPDATE) && !bcm_is_busy)
            /* Update requested and BCM is no longer busy. */
         || (TIME_AFTER(current_tick, lcd_state.update_timeout) && bcm_is_busy))
            /* BCM still busy after timeout, i.e. stalled. */
        {
            bcm_write32(BCMA_COMMAND, BCMCMD_LCD_UPDATE);  /* Kick off update */
            BCM_CONTROL = 0x31;
            lcd_state.update_timeout = current_tick + BCM_UPDATE_TIMEOUT;
            lcd_state.state = LCD_UPDATING;
#ifdef HAVE_LCD_SLEEP
            if (lcd_state.waking)
                continue_lcd_awake();
#endif
        }
        else if ((lcd_state.state == LCD_UPDATING) && !bcm_is_busy)
        {
            /* Update finished properly and no new update pending. */
            lcd_state.state = LCD_IDLE;
#ifdef HAVE_LCD_SLEEP
            if (lcd_state.waking)
                continue_lcd_awake();
#endif
        }
    }
#if NUM_CORES > 1
    corelock_unlock(&lcd_state.cl);
#endif
}

static inline void lcd_block_tick(void)
{
    int oldlevel = disable_irq_save();

#if NUM_CORES > 1
    corelock_lock(&lcd_state.cl);
    lcd_state.blocked = true;
    corelock_unlock(&lcd_state.cl);
#else
    lcd_state.blocked = true;
#endif
    restore_irq(oldlevel);
}

static void lcd_unblock_and_update(void)
{
    unsigned data;
    bool bcm_is_busy;
    int oldlevel = disable_irq_save();

#if NUM_CORES > 1
    corelock_lock(&lcd_state.cl);
#endif
    data = bcm_read32(BCMA_COMMAND);
    bcm_is_busy = (data == BCMCMD_LCD_UPDATE || data == 0xFFFF);

    if (!bcm_is_busy || (lcd_state.state == LCD_INITIAL) ||
        TIME_AFTER(current_tick, lcd_state.update_timeout))
    {
        bcm_write32(BCMA_COMMAND, BCMCMD_LCD_UPDATE);  /* Kick off update */
        BCM_CONTROL = 0x31;
        lcd_state.update_timeout = current_tick + BCM_UPDATE_TIMEOUT;
        lcd_state.state = LCD_UPDATING;
#ifdef HAVE_LCD_SLEEP
        if (lcd_state.waking)
            continue_lcd_awake();
#endif
    }
    else
    {
         lcd_state.state = LCD_NEED_UPDATE; /* Post update request */
    }
    lcd_state.blocked = false;

#if NUM_CORES > 1
    corelock_unlock(&lcd_state.cl);
#endif
    restore_irq(oldlevel);
}

#else /* BOOTLOADER */

#define lcd_block_tick()

static void lcd_unblock_and_update(void)
{
    unsigned data;

    if (lcd_state.state != LCD_INITIAL)
    {
        data = bcm_read32(BCMA_COMMAND);
        while (data == BCMCMD_LCD_UPDATE || data == 0xFFFF)
        {
            yield();
            data = bcm_read32(BCMA_COMMAND);
        }
    }
    bcm_write32(BCMA_COMMAND, BCMCMD_LCD_UPDATE);  /* Kick off update */
    BCM_CONTROL = 0x31;
    lcd_state.state = LCD_IDLE;
}
#endif /* BOOTLOADER */

/*** hardware configuration ***/

void lcd_set_contrast(int val)
{
  /* TODO: Implement lcd_set_contrast() */
  (void)val;
}

void lcd_set_invert_display(bool yesno)
{
  /* TODO: Implement lcd_set_invert_display() */
  (void)yesno;
}

/* turn the display upside down (call lcd_update() afterwards) */
void lcd_set_flip(bool yesno)
{
  /* TODO: Implement lcd_set_flip() */
  (void)yesno;
}

/* LCD init */
void lcd_init_device(void)
{
    /* These port initializations are supposed to be done when initializing
       the BCM.  None of it is changed when shutting down the BCM.
     */
    GPO32_ENABLE |= 0xC000;
    GPIO_CLEAR_BITWISE(GPIOC_ENABLE, 0x80);
    /* This pin is used for BCM interrupts */
    GPIOC_ENABLE |= 0x40;
    GPIOC_OUTPUT_EN &= ~0x40;
    GPO32_ENABLE &= ~1;

    lcd_state.blocked = false;
    lcd_state.state = LCD_INITIAL;
#ifndef BOOTLOADER
#if NUM_CORES > 1
    corelock_init(&lcd_state.cl);
#endif
#ifdef HAVE_LCD_SLEEP
    if (!flash_get_section(ROM_ID('v', 'm', 'c', 's'),
                           (void **)(&flash_vmcs_offset), &flash_vmcs_length))
        /* BCM cannot be shut down because firmware wasn't found */
        flash_vmcs_length = 0;
    else
    {
        /* lcd_write_data needs an even number of 16 bit values */
        flash_vmcs_length = ((flash_vmcs_length + 3) >> 1) & ~1;
    }
    semaphore_init(&(lcd_state.initwakeup), 1, 0);
    lcd_state.waking = false;

    if (GPO32_VAL & 0x4000)
    {
        /* BCM is powered.  Assume it is initialized. */
        lcd_state.display_on = true;
        tick_add_task(&lcd_tick);
    }
    else
    {
        /* BCM is not powered, so it needs to be initialized.
           This can only happen when loading Rockbox via ROLO.
         */
        lcd_state.update_timeout = current_tick;
        lcd_state.display_on = false;
        lcd_awake();
    }
#else /* !HAVE_LCD_SLEEP */
    tick_add_task(&lcd_tick);
#endif
#endif /* !BOOTLOADER */
}

/*** update functions ***/

#ifdef HAVE_IPODJS_UI
static fb_data lcd_overlay_line[LCD_WIDTH] CACHEALIGN_ATTR;
#endif

/* Update a fraction of the display. */
void lcd_update_rect(int x, int y, int width, int height)
{
    const fb_data *addr;
    unsigned bcmaddr;
#ifndef BOOTLOADER
    int mirror_x;
    int mirror_y;
    int mirror_width;
    int mirror_height;
#endif

#ifdef HAVE_LCD_SLEEP
    if (!lcd_state.display_on)
        return;
#endif

    if (x + width >= LCD_WIDTH)
        width = LCD_WIDTH - x;
    if (y + height >= LCD_HEIGHT)
        height = LCD_HEIGHT - y;

    if ((width <= 0) || (height <= 0))
        return; /* Nothing left to do. */

#ifndef BOOTLOADER
    mirror_x = x;
    mirror_y = y;
    mirror_width = width;
    mirror_height = height;
#endif

    /* Ensure x and width are both even. The BCM doesn't like small unaligned
     * writes and would just ignore them. */
    width = (width + (x & 1) + 1) & ~1;
    x &= ~1;

    /* Prevent the tick from triggering BCM updates while we're writing. */
    lcd_block_tick();

    addr = FBADDR(x, y);
    bcmaddr = BCMA_CMDPARAM + (LCD_WIDTH*2) * y + (x << 1);

    if (width == LCD_WIDTH)
    {
#ifdef HAVE_IPODJS_UI
        int row = 0;

        while (row < height)
        {
            int run;

            if (lcd_compose_overlay_row(y + row, x, width,
                                        lcd_overlay_line))
            {
                bcm_write_addr(bcmaddr + (LCD_WIDTH * 2) * row);
                lcd_write_data(lcd_overlay_line, width);
                row++;
                continue;
            }
            run = 1;
            while (row + run < height &&
                   !lcd_compose_overlay_row(y + row + run, x, width,
                                            lcd_overlay_line))
                run++;
            bcm_write_addr(bcmaddr + (LCD_WIDTH * 2) * row);
            lcd_write_data(addr + LCD_WIDTH * row, width * run);
            row += run;
        }
#else
        bcm_write_addr(bcmaddr);
        lcd_write_data(addr, width * height);
#endif
    }
    else
    {
        int row_y = y;
        do
        {
            bcm_write_addr(bcmaddr);
            bcmaddr += (LCD_WIDTH*2);
#ifdef HAVE_IPODJS_UI
            if (lcd_compose_overlay_row(row_y, x, width,
                                        lcd_overlay_line))
                lcd_write_data(lcd_overlay_line, width);
            else
#endif
            lcd_write_data(addr, width);
            addr += LCD_WIDTH;
            row_y++;
        }
        while (--height > 0);
    }
    lcd_unblock_and_update();
#ifndef BOOTLOADER
    bcm2722_videoout_mirror_if_due(false, mirror_x, mirror_y,
                                    mirror_width, mirror_height);
#endif
}

/* Update the display.
   This must be called after all other LCD functions that change the display. */
void lcd_update(void)
{
    lcd_update_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
}

/* Line write helper function for lcd_yuv_blit. Writes two lines of yuv420. */
extern void lcd_write_yuv420_lines(unsigned char const * const src[3],
                                   unsigned bcmaddr,
                                   int width,
                                   int stride);

/* Performance function to blit a YUV bitmap directly to the LCD */
void lcd_blit_yuv(unsigned char * const src[3],
                  int src_x, int src_y, int stride,
                  int x, int y, int width, int height)
{
    unsigned bcmaddr;
    int destination_y = y;
    int destination_height = height;
    off_t z;
    unsigned char const * yuv_src[3];

#ifdef HAVE_LCD_SLEEP
    if (!lcd_state.display_on)
        return;
#endif

    /* Sorry, but width and height must be >= 2 or else */
    width &= ~1;

    z = stride * src_y;
    yuv_src[0] = src[0] + z + src_x;
    yuv_src[1] = src[1] + (z >> 2) + (src_x >> 1);
    yuv_src[2] = src[2] + (yuv_src[1] - src[1]);

    /* Prevent the tick from triggering BCM updates while we're writing. */
    lcd_block_tick();

    bcmaddr = BCMA_CMDPARAM + (LCD_WIDTH*2) * y + (x << 1);
    height >>= 1;

    do
    {
        lcd_write_yuv420_lines(yuv_src, bcmaddr, width, stride);
        bcmaddr += (LCD_WIDTH*4);  /* Skip up two lines */
        yuv_src[0] += stride << 1;
        yuv_src[1] += stride >> 1; /* Skip down one chroma line */
        yuv_src[2] += stride >> 1;
    }
    while (--height > 0);

#ifdef HAVE_IPODJS_UI
    {
        int row;

        for (row = 0; row < destination_height; ++row)
        {
            if (!lcd_compose_overlay_row(destination_y + row, x, width,
                                         lcd_overlay_line))
                continue;
            bcm_write_addr(BCMA_CMDPARAM +
                           (LCD_WIDTH * 2) * (destination_y + row) +
                           (x << 1));
            lcd_write_data(lcd_overlay_line, width);
        }
    }
#endif

    lcd_unblock_and_update();
#ifndef BOOTLOADER
    bcm2722_videoout_mirror_if_due(false, x, destination_y, width,
                                    destination_height);
#endif
}

#ifdef HAVE_LCD_SLEEP
/* Executes a BCM command immediately and waits for it to complete.
   Other BCM commands (eg. LCD updates or lcd_tick) must not interfere.
 */
static void bcm_command(unsigned cmd)
{
    unsigned status;

    bcm_write32(BCMA_COMMAND,  cmd);

    BCM_CONTROL = 0x31;

    while (1)
    {
        status = bcm_read32(BCMA_COMMAND);
        if (status != cmd && status != 0xFFFF)
            break;
        yield();
    }
}

static void bcm_powerdown(void)
{
    /* Immediately switch off the backlight to avoid flashing. */
    _backlight_hw_enable(false);
    
    /* Not sure what this does. */
    bcm_write32(0x10001400, bcm_read32(0x10001400) & ~0xF0);

    /* Blanks the LCD and decreases power consumption
       below what clearing the LCD would achieve.
       Executing an LCD update command wakes it.
     */
    bcm_command(BCMCMD_LCD_SLEEP);

    /* Not sure if this does anything */
    bcm_command(BCM_CMD(0xC));

    /* Further cuts power use, probably by powering down BCM.
       After this point, BCM needs to be bootstrapped
     */
    GPO32_VAL &= ~0x4000;
}

/* Apple's retail VMCS image does not implement the small Rockbox/NOR LCD
 * command set used by bcm_powerdown().  When the media player owns the
 * VideoCore, stop it at the power rail and let lcd_awake() perform the same
 * cold bootstrap used after normal LCD sleep. */
static void bcm_retail_powerdown(void)
{
    _backlight_hw_enable(false);
    GPO32_VAL &= ~0x4000;
}

/* Data written to BCM_CONTROL and BCM_ALT_CONTROL */
const unsigned char bcm_bootstrapdata[] =
{
    0xA1, 0x81, 0x91, 0x02, 0x12, 0x22, 0x72, 0x62
};

static void bcm_init_image(const void *image, unsigned length)
{
    int i;

    /* Power up BCM */
    GPO32_VAL |= 0x4000;
    sleep(HZ/20);

    /* Bootstrap stage 1 */

    STRAP_OPT_A &= ~0xF00;
    outl(0x1313, 0x70000040);

    /* Interrupt-related code for future use
       GPIOC_INT_LEV |= 0x40;
       GPIOC_INT_EN |= 0x40;
       CPU_HI_INT_EN |= 0x40000;
    */

    /* Bootstrap stage 2 */

    while (BCM_ALT_CONTROL & 0x80);
    while (!(BCM_ALT_CONTROL & 0x40));

    for (i = 0; i < 8; i++)
    {
        BCM_CONTROL = bcm_bootstrapdata[i];
    }

    for (i = 3; i < 8; i++)
    {
        BCM_ALT_CONTROL = bcm_bootstrapdata[i];
    }

    while ((BCM_RD_ADDR & 1) == 0 || (BCM_ALT_RD_ADDR & 1) == 0);

    (void)BCM_WR_ADDR;
    (void)BCM_ALT_WR_ADDR;

    /* Bootstrap stage 3: upload firmware */

    while (BCM_ALT_CONTROL & 0x80);
    while (!(BCM_ALT_CONTROL & 0x40));

    /* Upload firmware to BCM SRAM */
    bcm_write_addr(BCMA_SRAM_BASE);
    lcd_write_data(image, length / sizeof(unsigned short));

    bcm_write32(BCMA_COMMAND,  0);
    bcm_write32(0x10000C00, 0xC0000000);

    while (!(bcm_read32(0x10000C00) & 1));

    bcm_write32(0x10000C00, 0);
    bcm_write32(0x10000400, 0xA5A50002);

    while (bcm_read32(BCMA_COMMAND) == 0)
        yield();

    /* sleep(HZ/2) apparently unneeded */
}

static void bcm_init(void)
{
    bcm_init_image(flash_vmcs_offset,
                   flash_vmcs_length * sizeof(unsigned short));
}

void lcd_awake(void)
{
    if (!lcd_state.display_on && flash_vmcs_length != 0)
    {
        /* Ensure BCM has been off for >= 50 ms */
        long sleepwait = lcd_state.update_timeout + HZ/20 - current_tick;
        if (sleepwait > 0 && sleepwait <= HZ/20)
            sleep(sleepwait);

        bcm_init();

        /* Start the first LCD update, which also initializes the LCD */
        lcd_state.state = LCD_INITIAL;
        lcd_state.display_on = true;
        lcd_update();
        lcd_state.update_timeout = current_tick + BCM_LCDINIT_TIMEOUT;

        /* Wait for end of first LCD update, so LCD isn't white
           when the backlight turns on.
         */
        lcd_state.waking = true;
        tick_add_task(&lcd_tick);
        semaphore_wait(&(lcd_state.initwakeup), TIMEOUT_BLOCK);

        send_event(LCD_EVENT_ACTIVATION, NULL);
    }
}

void lcd_sleep(void)
{
#ifndef BOOTLOADER
    /* The resident VMCS drives the LCD and composite DAC together.  Keep the
     * controller alive while an enabled TV signal is scanning; the ordinary
     * backlight path can still turn the lamp itself off. */
    if (bcm2722_videoout_signal_active &&
        (bcm2722_videoout_mode == BCM_VIDEOOUT_ON ||
         (bcm2722_videoout_mode == BCM_VIDEOOUT_AUTO &&
          (GPIOA_INPUT_VAL & 0x10) != 0)))
        return;
#endif

    if (lcd_state.display_on && flash_vmcs_length != 0)
    {
        lcd_state.display_on = false;

        /* Wait for BCM to finish work */
        while (lcd_state.state != LCD_INITIAL && lcd_state.state != LCD_IDLE)
            yield();

        tick_remove_task(&lcd_tick);
        bcm_powerdown();

        /* Remember time to ensure BCM stays off for >= 50 ms */
        lcd_state.update_timeout = current_tick;
    }
}

bool lcd_active(void)
{
    return lcd_state.display_on;
}

#ifdef HAVE_LCD_SHUTDOWN
void lcd_shutdown(void)
{
    lcd_sleep();
}
#endif /* HAVE_LCD_SHUTDOWN */
#endif /* HAVE_LCD_SLEEP */

#ifndef BOOTLOADER
static bool bcm_player_wait(volatile unsigned short *reg, unsigned mask,
                            unsigned expected, long timeout)
{
    long deadline = current_tick + timeout;

    while (((unsigned)*reg & mask) != expected)
    {
        if (!TIME_BEFORE(current_tick, deadline))
            return false;
        yield();
    }
    return true;
}

/* Apple 5G Diagnostics (device-matched NOR, diagmode 0x1000ebc4) writes the
 * encoded TV command first, uploads a Windows BMP to 0xc0200000, notifies the
 * VideoCore with 0x31, polls 0x1f8, then consumes the signed status at 0x1fc.
 * Commands 2/3/4 are the only commands routed to the BMP staging address.
 * FS#9787 established that a 320x240 command-3 BMP arms the encoder for the
 * resident VMCS live framebuffer at 0xc0000000. */
static const unsigned char bcm_tv_ntsc_header[54] CACHEALIGN_ATTR =
{
    0x42, 0x4d, 0x38, 0x84, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
    0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xf0, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x84, 0x03, 0x00, 0x12, 0x0b,
    0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* FS#9787 measured the resident 5G TV framebuffer at 0xc0000000.  The
 * encoder consumes BGR24 but expects studio-swing component levels. */
static const unsigned char bcm_tv_level_6[64] ICONST_ATTR =
{
    16, 19, 23, 26, 30, 33, 37, 40, 44, 47, 51, 54, 58, 61, 65, 68,
    72, 75, 79, 82, 86, 89, 92, 96, 99, 103, 106, 110, 113, 117, 120,
    124, 127, 131, 134, 138, 141, 145, 148, 152, 155, 159, 162, 165,
    169, 172, 176, 179, 183, 186, 190, 193, 197, 200, 204, 207, 211,
    214, 218, 221, 225, 228, 232, 235
};

static const unsigned char bcm_tv_level_5[32] ICONST_ATTR =
{
    16, 23, 30, 37, 44, 51, 58, 65, 73, 80, 87, 94, 101, 108, 115,
    122, 129, 136, 143, 150, 157, 164, 171, 178, 186, 193, 200, 207,
    214, 221, 228, 235
};

static void bcm_videoout_init_maps(void)
{
    int i;

    if (bcm2722_videoout_maps_ready)
        return;

    for (i = 0; i < BCM_TV_NTSC_WIDTH; ++i)
    {
        if (i < BCM_TV_SAFE_X ||
            i >= BCM_TV_SAFE_X + BCM_TV_SAFE_WIDTH)
            bcm2722_videoout_xmap[i] = BCM_TV_MAP_INVALID_X;
        else
            bcm2722_videoout_xmap[i] =
                ((i - BCM_TV_SAFE_X) * LCD_WIDTH +
                 BCM_TV_SAFE_WIDTH / 2) / BCM_TV_SAFE_WIDTH;
    }
    for (i = 0; i < BCM_TV_NTSC_HEIGHT; ++i)
    {
        if (i < BCM_TV_SAFE_Y ||
            i >= BCM_TV_SAFE_Y + BCM_TV_SAFE_HEIGHT)
            bcm2722_videoout_ymap[i] = BCM_TV_MAP_INVALID_Y;
        else
            bcm2722_videoout_ymap[i] =
                ((i - BCM_TV_SAFE_Y) * LCD_HEIGHT +
                 BCM_TV_SAFE_HEIGHT / 2) / BCM_TV_SAFE_HEIGHT;
    }
    bcm2722_videoout_maps_ready = true;
}

static void bcm_videoout_make_line(int output_y, int output_x,
                                   int output_width)
{
    unsigned char *destination = (unsigned char *)bcm2722_videoout_line;
    unsigned source_y = bcm2722_videoout_ymap[output_y];
    const fb_data *source = NULL;
    int column;

    if (source_y != BCM_TV_MAP_INVALID_Y)
    {
        source = FBADDR(0, source_y);
#ifdef HAVE_IPODJS_UI
        if (lcd_compose_overlay_row(source_y, 0, LCD_WIDTH,
                                    lcd_overlay_line))
            source = lcd_overlay_line;
#endif
    }

    for (column = output_x; column < output_x + output_width; ++column)
    {
        unsigned source_x = bcm2722_videoout_xmap[column];
        unsigned pixel = source == NULL || source_x == BCM_TV_MAP_INVALID_X ?
                         0 : source[source_x];

        *destination++ = bcm_tv_level_5[pixel & 0x1f];
        *destination++ = bcm_tv_level_6[(pixel >> 5) & 0x3f];
        *destination++ = bcm_tv_level_5[(pixel >> 11) & 0x1f];
    }
}

static bool bcm_videoout_read32(unsigned address, unsigned *value)
{
    if (value == NULL ||
        !bcm_player_wait(&BCM_RD_ADDR, 0x1, 0x1,
                         BCM_PLAYER_BUS_TIMEOUT))
        return false;

    BCM_RD_ADDR = address;
    BCM_RD_ADDR = address >> 16;
    if (!bcm_player_wait(&BCM_CONTROL, 0x10, 0x10,
                         BCM_PLAYER_BUS_TIMEOUT))
        return false;

    *value = (unsigned)BCM_DATA;
    *value |= (unsigned)BCM_DATA << 16;
    (void)BCM_RD_ADDR;
    return true;
}

static void bcm_videoout_write_addr(unsigned address)
{
    BCM_WR_ADDR = address;
    BCM_WR_ADDR = address >> 16;
}

static bool bcm_videoout_write_addr_ready(unsigned address)
{
    bcm_videoout_write_addr(address);
    return bcm_player_wait(&BCM_CONTROL, 0x2, 0x2,
                           BCM_PLAYER_BUS_TIMEOUT);
}

static bool bcm_videoout_write32(unsigned address, unsigned value)
{
    if (!bcm_videoout_write_addr_ready(address))
        return false;
    BCM_DATA = value;
    BCM_DATA = value >> 16;
    return true;
}

/* diagmode 0x1000e564 waits for write-ready before every 16-byte burst.
 * The old Rockbox patch omitted this pacing and can overrun the host FIFO,
 * producing exactly the horizontal line corruption seen on hardware. */
static bool bcm_videoout_write_halfwords(const unsigned char *source,
                                         size_t length)
{
    while (length != 0)
    {
        size_t burst = length > 16 ? 16 : length;

        if (!bcm_player_wait(&BCM_CONTROL, 0x2, 0x2,
                             BCM_PLAYER_BUS_TIMEOUT))
            return false;
        length -= burst;
        while (burst != 0)
        {
            BCM_DATA = (unsigned)source[0] | ((unsigned)source[1] << 8);
            source += 2;
            burst -= 2;
        }
    }
    return true;
}

static bool bcm_videoout_wait_command(unsigned command, bool equal)
{
    long deadline = current_tick + BCM_PLAYER_BUS_TIMEOUT;

    while (TIME_BEFORE(current_tick, deadline))
    {
        unsigned value;

        if (!bcm_videoout_read32(BCMA_COMMAND, &value))
            return false;
        if (equal ? value == command :
                    (value != command && value != 0xffffu))
            return true;
        yield();
    }
    return false;
}

static bool bcm_videoout_write_zeros(size_t length)
{
    while (length != 0)
    {
        size_t chunk = length > sizeof(bcm2722_videoout_zero) ?
                       sizeof(bcm2722_videoout_zero) : length;

        if (!bcm_videoout_write_halfwords(
                (const unsigned char *)bcm2722_videoout_zero, chunk))
            return false;
        length -= chunk;
    }
    return true;
}

static bool bcm_videoout_should_enable(void)
{
    if (bcm2722_videoout_mode == BCM_VIDEOOUT_ON)
        return true;
    if (bcm2722_videoout_mode != BCM_VIDEOOUT_AUTO)
        return false;

    /* Same dock-present observation already used by the retail movie path.
     * On remains available for passive 30-pin/headphone composite leads. */
    return (GPIOA_INPUT_VAL & 0x10) != 0;
}

static bool bcm_videoout_send_ntsc_frame(void)
{
    unsigned command = BCMCMD_TV_NTSCBMP;
    unsigned route;
    unsigned status;

    if (!bcm_videoout_wait_command(BCMCMD_LCD_UPDATE, false))
        return false;

    /* diagmode 0x1000f728 is the required display route immediately before
     * the first TV command: select route bit 6, clear route bit 7, and point
     * the first display slot at the resident VMCS LCD workspace. */
    if (!bcm_videoout_read32(0x10002804, &route))
        return false;
    route = (route | 0x40u) & ~0x80u;
    if (!bcm_videoout_write32(0x10002804, route) ||
        !bcm_videoout_write32(0x10002810, 0x00080000u))
        return false;

    /* Command 3 with a 320x240 BGR24 BMP arms the encoder in the LCD's
     * native mode.  The two bytes after the image are intentional: they are
     * present in FS#9787's working header/image-size pair and flush the
     * final host word. */
    if (!bcm_videoout_write32(BCMA_COMMAND, command) ||
        !bcm_videoout_write_addr_ready(BCMA_TV_BMPDATA) ||
        !bcm_videoout_write_halfwords(bcm_tv_ntsc_header,
                                      sizeof(bcm_tv_ntsc_header)) ||
        !bcm_videoout_write_zeros(BCM_TV_NTSC_FILE_BYTES -
                                  sizeof(bcm_tv_ntsc_header)))
        return false;

    BCM_CONTROL = 0x31;
    if (!bcm_videoout_wait_command(command, false) ||
        !bcm_videoout_read32(BCMA_STATUS, &status) ||
        (short)status != 0)
        return false;

    /* The first NTSC arm of Apple's diagnostic stops here.  Command 14 is
     * issued only after an explicit PAL/NTSC change, following a complete
     * display power cycle.  Re-sending either command on every Rockbox LCD
     * update makes an attached television repeatedly lose and reacquire
     * sync. */
    return true;
}

static bool bcm_videoout_write_live_rect(int x, int y,
                                         int width, int height)
{
    int source_end_x;
    int source_end_y;
    int output_x = BCM_TV_NTSC_WIDTH;
    int output_end_x = 0;
    int output_y = BCM_TV_NTSC_HEIGHT;
    int output_end_y = 0;
    int column;
    int row;

    if (x < 0)
    {
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        height += y;
        y = 0;
    }
    if (x + width > LCD_WIDTH)
        width = LCD_WIDTH - x;
    if (y + height > LCD_HEIGHT)
        height = LCD_HEIGHT - y;
    if (width <= 0 || height <= 0)
        return true;

    bcm_videoout_init_maps();
    source_end_x = x + width;
    source_end_y = y + height;

    /* Find exactly which safe-area output pixels sample this dirty source
     * rectangle.  Full-width/full-height updates also repaint the black
     * border so the whole Rockbox UI remains visible through TV overscan. */
    if (x == 0 && source_end_x == LCD_WIDTH)
    {
        output_x = 0;
        output_end_x = BCM_TV_NTSC_WIDTH;
    }
    else
    {
        for (column = BCM_TV_SAFE_X;
             column < BCM_TV_SAFE_X + BCM_TV_SAFE_WIDTH; ++column)
        {
            unsigned source_x = bcm2722_videoout_xmap[column];

            if ((int)source_x >= x && (int)source_x < source_end_x)
            {
                if (output_x == BCM_TV_NTSC_WIDTH)
                    output_x = column;
                output_end_x = column + 1;
            }
        }
        output_x &= ~3;
        output_end_x = (output_end_x + 3) & ~3;
    }

    if (y == 0 && source_end_y == LCD_HEIGHT)
    {
        output_y = 0;
        output_end_y = BCM_TV_NTSC_HEIGHT;
    }
    else
    {
        for (row = BCM_TV_SAFE_Y;
             row < BCM_TV_SAFE_Y + BCM_TV_SAFE_HEIGHT; ++row)
        {
            unsigned source_y = bcm2722_videoout_ymap[row];

            if ((int)source_y >= y && (int)source_y < source_end_y)
            {
                if (output_y == BCM_TV_NTSC_HEIGHT)
                    output_y = row;
                output_end_y = row + 1;
            }
        }
    }

    if (output_x >= output_end_x || output_y >= output_end_y)
        return true;

    width = output_end_x - output_x;
    if (output_x == 0 && width == BCM_TV_NTSC_WIDTH &&
        !bcm_videoout_write_addr_ready(
            BCMA_TV_FB + BCM_TV_NTSC_ROW_BYTES * output_y))
        return false;

    for (row = output_y; row < output_end_y; ++row)
    {
        bcm_videoout_make_line(row, output_x, width);
        if ((output_x != 0 || width != BCM_TV_NTSC_WIDTH) &&
            !bcm_videoout_write_addr_ready(
                BCMA_TV_FB + BCM_TV_NTSC_ROW_BYTES * row + output_x * 3))
            return false;

        /* lcd_write_data is the PP5022 assembly streaming path.  Separating
         * conversion from the host-port copy removes tens of thousands of
         * per-pixel readiness calls and shortens the live-surface write far
         * enough to avoid the visible scan tearing of the old C loop. */
        lcd_write_data((const fb_data *)bcm2722_videoout_line,
                       width * 3 / sizeof(fb_data));
    }

    /* The VideoCore caches this SDRAM window.  FS#9787 found that touching
     * twenty scanlines beyond the framebuffer makes live writes visible. */
    if (!bcm_videoout_write_addr_ready(BCMA_TV_FB +
                                       BCM_TV_NTSC_IMAGE_BYTES))
        return false;
    for (row = 0; row < BCM_TV_NTSC_ROW_BYTES *
                        BCM_TV_CACHE_FLUSH_LINES /
                        (int)sizeof(bcm2722_videoout_zero); ++row)
        lcd_write_data((const fb_data *)bcm2722_videoout_zero,
                       sizeof(bcm2722_videoout_zero) / sizeof(fb_data));

    return true;
}

static void bcm2722_videoout_mirror_if_due(bool force, int x, int y,
                                            int width, int height)
{
    bool enabled;
    bool initialized = true;

    if (bcm2722_videoout_updating || bcm2722_player_active ||
        !lcd_state.display_on)
        return;

    enabled = bcm_videoout_should_enable();
    if (!enabled)
        return;
    if (!bcm2722_videoout_signal_active && !force &&
        TIME_BEFORE(current_tick, bcm2722_videoout_next_update))
        return;

    bcm2722_videoout_updating = true;
    cpu_boost(true);
    lcd_block_tick();
    if (!bcm2722_videoout_signal_active)
    {
        initialized = bcm_videoout_send_ntsc_frame();
        bcm2722_videoout_signal_active = initialized;
        bcm2722_videoout_next_update = current_tick + HZ * 2;
        /* The staging BMP is only the mode-set.  Every displayed frame after
         * that is written directly to the live framebuffer. */
        x = 0;
        y = 0;
        width = LCD_WIDTH;
        height = LCD_HEIGHT;
    }
    else if (!bcm_videoout_wait_command(BCMCMD_LCD_UPDATE, false))
        initialized = false;

    if (initialized)
        (void)bcm_videoout_write_live_rect(x, y, width, height);

    /* A completed TV command leaves the LCD tick's state bookkeeping one
     * transition behind.  Unblock it without posting an extra LCD command;
     * the next tick observes completion and returns the state to IDLE. */
    {
        int oldlevel = disable_irq_save();
#if NUM_CORES > 1
        corelock_lock(&lcd_state.cl);
#endif
        lcd_state.blocked = false;
#if NUM_CORES > 1
        corelock_unlock(&lcd_state.cl);
#endif
        restore_irq(oldlevel);
    }
    cpu_boost(false);
    bcm2722_videoout_updating = false;
}

bool bcm2722_videoout_set_mode(int mode)
{
    bool restarted = false;

    if (mode < BCM_VIDEOOUT_OFF || mode > BCM_VIDEOOUT_ON)
        mode = BCM_VIDEOOUT_AUTO;

    bcm2722_videoout_mode = mode;
    bcm2722_videoout_next_update = 0;

    if (bcm2722_player_active || !lcd_state.display_on)
        return false;

    if (bcm_videoout_should_enable())
    {
        bcm2722_videoout_mirror_if_due(true, 0, 0, LCD_WIDTH, LCD_HEIGHT);
        return false;
    }

    if (bcm2722_videoout_signal_active)
    {
        long deadline = current_tick + BCM_PLAYER_BUS_TIMEOUT;

        while (lcd_state.state != LCD_INITIAL &&
               lcd_state.state != LCD_IDLE &&
               TIME_BEFORE(current_tick, deadline))
            yield();

        if (lcd_state.state == LCD_INITIAL || lcd_state.state == LCD_IDLE)
        {
            /* Apple Diagnostics leaves TVOUT through commands 8 and 12,
             * drops GPO 0x4000, and performs a normal resident bootstrap.
             * bcm_powerdown()/lcd_awake() are Rockbox's equivalent sequence. */
            lcd_state.display_on = false;
            tick_remove_task(&lcd_tick);
            bcm_powerdown();
            lcd_state.update_timeout = current_tick;
            bcm2722_videoout_signal_active = false;
            lcd_awake();
            restarted = true;
        }
    }

    return restarted;
}

/* RetailOS uses the PP502x's second DMA controller for the VideoCore host
 * port.  Rockbox's audio DMA is the separate controller at 0x6000a000 /
 * 0x6000b000, so this does not borrow or reconfigure PCM's channel. */
#define BCM_DMA_MASTER_CONTROL (*(volatile unsigned long *)0x60008000)
#define BCM_DMA_CMD            (*(volatile unsigned long *)0x60009000)
#define BCM_DMA_STATUS         (*(volatile unsigned long *)0x60009004)
#define BCM_DMA_RAM_ADDR       (*(volatile unsigned long *)0x60009010)
#define BCM_DMA_RAM_CONFIG     (*(volatile unsigned long *)0x60009014)
#define BCM_DMA_PER_ADDR       (*(volatile unsigned long *)0x60009018)
#define BCM_DMA_PER_CONFIG     (*(volatile unsigned long *)0x6000901c)

#define BCM_DMA_IRQ_MASK       (1ul << 27)
#define BCM_DMA_CHUNK_BYTES    0x10000u
#define BCM_DMA_BULK_ALIGN     0x100u
#define BCM_DMA_RAM_CONFIG_32  0x22000000u
#define BCM_DMA_PER_CONFIG_32  0x26000000u
#define BCM_DMA_COMMAND        (DMA_CMD_INTR | DMA_CMD_RAM_TO_PER | \
                                DMA_CMD_SINGLE)

static bool bcm_player_write_addr(unsigned address)
{
    if (bcm2722_player_fault)
        return false;

    /* RetailOS 1.3's 0x10287be8 writes both halves, in little-endian order,
     * to the same 16-bit host register.  Do not use the normal LCD driver's
     * 32-bit shortcut here: Apple's player loader does not use it. */
    BCM_WR_ADDR = address;
    BCM_WR_ADDR = address >> 16;
    return true;
}

static bool bcm_player_write32(unsigned address, unsigned value)
{
    if (!bcm_player_write_addr(address))
        return false;
    BCM_DATA = value;
    BCM_DATA = value >> 16;
    return true;
}

static bool bcm_player_write_halfwords_paced(const unsigned char *source,
                                              size_t length)
{
    while (length != 0)
    {
        size_t burst = length > 16 ? 16 : length;

        if (!bcm_player_wait(&BCM_CONTROL, 0x2, 0x2,
                             BCM_PLAYER_BUS_TIMEOUT))
            goto fault;
        length -= burst;
        while (burst != 0)
        {
            BCM_DATA = (unsigned)source[0] | ((unsigned)source[1] << 8);
            source += 2;
            burst -= 2;
        }
    }
    return true;

fault:
    bcm2722_player_fault = true;
    return false;
}

static bool bcm_player_dma_write(const void *buffer, size_t length)
{
    unsigned long source = (unsigned long)buffer;
    bool restore_irq = (CPU_INT_EN_STAT & BCM_DMA_IRQ_MASK) != 0;
    long deadline;
    unsigned long status;

    if (length == 0 || length > BCM_DMA_CHUNK_BYTES ||
        (source & 3) != 0 || (length & 0xf) != 0)
        return false;

    /* Keep Apple's INTR command bit, but mask its RetailOS-only IRQ line and
     * poll START.  No Rockbox ISR owns IRQ 27. */
    CPU_INT_DIS = BCM_DMA_IRQ_MASK;
    BCM_DMA_MASTER_CONTROL |= DMA_MASTER_CONTROL_EN;
    BCM_DMA_CMD &= ~(DMA_CMD_START | DMA_CMD_INTR);
    (void)BCM_DMA_STATUS;

    deadline = current_tick + BCM_PLAYER_BUS_TIMEOUT;
    while ((BCM_DMA_STATUS & DMA_STATUS_BUSY) != 0)
    {
        if (!TIME_BEFORE(current_tick, deadline))
            goto fault;
        yield();
    }

    if (source < UNCACHED_BASE_ADDR)
    {
        commit_dcache();
        source = (unsigned long)UNCACHED_ADDR(source);
    }

    /* RetailOS initializes both sides to 0x02000000, selects 32-bit width
     * in bits 30..28, and applies opcode 8/value 4 to the peripheral side. */
    BCM_DMA_RAM_CONFIG = BCM_DMA_RAM_CONFIG_32;
    BCM_DMA_PER_CONFIG = BCM_DMA_PER_CONFIG_32;
    BCM_DMA_RAM_ADDR = source;
    BCM_DMA_PER_ADDR = (unsigned long)&BCM_DATA32;
    BCM_DMA_CMD = BCM_DMA_COMMAND | (length - 4) | DMA_CMD_START;

    deadline = current_tick + BCM_PLAYER_BUS_TIMEOUT;
    while ((BCM_DMA_CMD & DMA_CMD_START) != 0)
    {
        if (!TIME_BEFORE(current_tick, deadline))
            goto fault;
        yield();
    }

    status = BCM_DMA_STATUS; /* Read-to-clear completion latch. */
    BCM_DMA_CMD &= ~(DMA_CMD_START | DMA_CMD_INTR);
    if (restore_irq)
        CPU_INT_EN = BCM_DMA_IRQ_MASK;
    if ((status & DMA_STATUS_INTR) == 0)
        goto completed_without_irq;
    return true;

fault:
    BCM_DMA_CMD &= ~(DMA_CMD_START | DMA_CMD_INTR);
    (void)BCM_DMA_STATUS;
    if (restore_irq)
        CPU_INT_EN = BCM_DMA_IRQ_MASK;
completed_without_irq:
    bcm2722_player_fault = true;
    return false;
}

static bool bcm_player_write_buffer_raw(unsigned address, const void *buffer,
                                        size_t length)
{
    const unsigned char *source = buffer;
    size_t bulk;

    if (buffer == NULL || (address & 1) != 0 || (length & 1) != 0 ||
        ((unsigned long)buffer & 1) != 0 || !bcm_player_write_addr(address))
        return false;

    /* RetailOS 1.3 0x10287be8 aligns the source with one halfword, sends the
     * largest 256-byte-aligned span through 0x10286fb4, then writes the tail
     * as paced halfwords.  vmcs.bin is therefore 3*64 KiB + 0x1200 by DMA,
     * followed by a 0xa0-byte CPU tail. */
    if (((unsigned long)source & 3) == 2 && length != 0)
    {
        if (!bcm_player_write_halfwords_paced(source, 2))
            return false;
        source += 2;
        length -= 2;
    }

    bulk = length & ~(BCM_DMA_BULK_ALIGN - 1);
    while (bulk != 0)
    {
        size_t chunk = bulk > BCM_DMA_CHUNK_BYTES ?
                       BCM_DMA_CHUNK_BYTES : bulk;

        if (!bcm_player_dma_write(source, chunk))
            return false;
        source += chunk;
        length -= chunk;
        bulk -= chunk;
    }

    if (!bcm_player_write_halfwords_paced(source, length))
        return false;
    return true;
}

static bool bcm_player_read32(unsigned address, unsigned *value)
{
    if (value == NULL || bcm2722_player_fault)
        return false;
    if (!bcm_player_wait(&BCM_RD_ADDR, 0x1, 0x1,
                         BCM_PLAYER_BUS_TIMEOUT))
        goto fault;

    BCM_RD_ADDR = address;
    BCM_RD_ADDR = address >> 16;
    if (!bcm_player_wait(&BCM_CONTROL, 0x10, 0x10,
                         BCM_PLAYER_BUS_TIMEOUT))
        goto fault;

    *value = (unsigned)BCM_DATA;
    *value |= (unsigned)BCM_DATA << 16;
    /* RetailOS finishes every read request by consuming this register. */
    (void)BCM_RD_ADDR;
    return true;

fault:
    bcm2722_player_fault = true;
    return false;
}

static bool bcm_player_wait32(unsigned address, unsigned mask,
                              unsigned expected, long timeout)
{
    long deadline = current_tick + timeout;

    while (TIME_BEFORE(current_tick, deadline))
    {
        unsigned value;

        if (!bcm_player_read32(address, &value))
            return false;
        if ((value & mask) == expected)
            return true;
        yield();
    }
    bcm2722_player_fault = true;
    return false;
}

static bool bcm_player_wait32_nonzero(unsigned address, long timeout)
{
    long deadline = current_tick + timeout;

    while (TIME_BEFORE(current_tick, deadline))
    {
        unsigned value;

        if (!bcm_player_read32(address, &value))
            return false;
        if (value != 0)
            return true;
        yield();
    }
    bcm2722_player_fault = true;
    return false;
}

static bool bcm_player_bootstrap_retail(void)
{
    unsigned i;

    /* RetailOS 1.3's 0x10287998 two-channel bootstrap. */
    for (i = 0; i < 8; i++)
        BCM_CONTROL = bcm_bootstrapdata[i];
    for (i = 3; i < 8; i++)
        BCM_ALT_CONTROL = bcm_bootstrapdata[i];

    if (!bcm_player_wait(&BCM_RD_ADDR, 0x1, 0x1,
                         BCM_PLAYER_BUS_TIMEOUT) ||
        !bcm_player_wait(&BCM_ALT_RD_ADDR, 0x1, 0x1,
                         BCM_PLAYER_BUS_TIMEOUT))
        goto fault;

    (void)BCM_WR_ADDR;
    (void)BCM_ALT_WR_ADDR;

    return true;

fault:
    bcm2722_player_fault = true;
    return false;
}

static bool bcm_player_wait_initial_upload_ready(void)
{
    /* RetailOS 1.3 0x102876cc..0x102876e4 performs this gate immediately
     * after the first 0x10287998 bootstrap.  Its later 0x10288058 runtime
     * bootstrap deliberately does not repeat it. */
    if (!bcm_player_wait(&BCM_ALT_CONTROL, 0x80, 0,
                         BCM_PLAYER_BUS_TIMEOUT) ||
        !bcm_player_wait(&BCM_ALT_CONTROL, 0x40, 0x40,
                         BCM_PLAYER_BUS_TIMEOUT))
    {
        bcm2722_player_fault = true;
        return false;
    }
    return true;
}

static bool bcm_player_read_buffer_raw(unsigned address, void *buffer,
                                       size_t length)
{
    unsigned char *destination = buffer;

    if (buffer == NULL || (address & 1) != 0 || (length & 3) != 0 ||
        bcm2722_player_fault ||
        !bcm_player_wait(&BCM_RD_ADDR, 0x1, 0x1,
                         BCM_PLAYER_BUS_TIMEOUT))
        goto fault;

    /* RetailOS 1.3's 0x10287a6c latches one source address, then consumes
     * two 16-bit data-port reads for each 32-bit word. */
    BCM_RD_ADDR = address;
    BCM_RD_ADDR = address >> 16;
    while (length != 0)
    {
        unsigned value;

        if (!bcm_player_wait(&BCM_CONTROL, 0x10, 0x10,
                             BCM_PLAYER_BUS_TIMEOUT))
            goto fault;
        value = (unsigned)BCM_DATA;
        value |= (unsigned)BCM_DATA << 16;
        destination[0] = value;
        destination[1] = value >> 8;
        destination[2] = value >> 16;
        destination[3] = value >> 24;
        destination += 4;
        length -= 4;
    }
    (void)BCM_RD_ADDR;
    return true;

fault:
    bcm2722_player_fault = true;
    return false;
}

static bool bcm_player_verify_buffer_raw(unsigned address,
                                         const void *buffer, size_t length)
{
    const unsigned char *source = buffer;
    size_t offset;

    /* This is RetailOS 1.3's optional verification branch at
     * 0x102877cc..0x10287838: read and compare one little-endian word at a
     * time.  Stock MPlayer disables it for speed; qualification builds use
     * it so a completed DMA command cannot masquerade as a valid upload. */
    for (offset = 0; offset < length; offset += 4)
    {
        unsigned expected = (unsigned)source[offset] |
                            ((unsigned)source[offset + 1] << 8) |
                            ((unsigned)source[offset + 2] << 16) |
                            ((unsigned)source[offset + 3] << 24);
        unsigned actual;

        if (!bcm_player_read32(address + offset, &actual))
            return false;
        if (actual != expected)
        {
            if (offset < 0x10000)
                bcm2722_player_error = "Apple upload verify chunk 1 mismatch";
            else if (offset < 0x20000)
                bcm2722_player_error = "Apple upload verify chunk 2 mismatch";
            else if (offset < 0x30000)
                bcm2722_player_error = "Apple upload verify chunk 3 mismatch";
            else
                bcm2722_player_error = "Apple upload verify tail mismatch";
            bcm2722_player_fault = true;
            return false;
        }
    }
    return true;
}

static bool bcm_player_start_retail_image(const void *image, size_t length)
{
    unsigned char runtime_header[16];
    unsigned runtime_ready;
    unsigned service_base;

    /* RetailOS 1.3's 0x10287698 loader, reduced to the flat vmcs.bin path.
     * The stock movie initializer calls this while the display VMCS is still
     * powered and passes zero for the optional read-back verification flag.
     * In particular, it does not issue the NOR sleep commands, cycle GPO
     * 0x4000, or reprogram the PP5022 boot straps before this bootstrap. */
    bcm2722_player_stage = BCM2722_VIDEO_STAGE_BOOTSTRAP;
    bcm2722_player_error = "Apple VideoCore bootstrap timed out";
    if (!bcm_player_bootstrap_retail())
        return false;

    bcm2722_player_error = "Apple VideoCore upload gate timed out";
    if (!bcm_player_wait_initial_upload_ready())
        return false;

    bcm2722_player_stage = BCM2722_VIDEO_STAGE_UPLOAD;
    bcm2722_player_error = "Apple VideoCore upload failed";
    if (!bcm_player_write_buffer_raw(BCMA_SRAM_BASE, image, length))
        return false;

    bcm2722_player_stage = BCM2722_VIDEO_STAGE_VERIFY;
    bcm2722_player_error = "Apple upload verification timed out";
    if (!bcm_player_verify_buffer_raw(BCMA_SRAM_BASE, image, length))
        return false;

    bcm2722_player_stage = BCM2722_VIDEO_STAGE_START;
    bcm2722_player_error = "Apple command clear failed";
    if (!bcm_player_write32(BCMA_COMMAND, 0))
        return false;
    bcm2722_player_error = "Apple start mailbox write failed";
    if (!bcm_player_write32(0x10000C00, 0xC0000000))
        return false;
    bcm2722_player_error = "Apple start mailbox arm timed out";
    if (!bcm_player_wait32(0x10000C00, 1, 1,
                           BCM_PLAYER_START_TIMEOUT))
        return false;
    bcm2722_player_error = "Apple start mailbox clear failed";
    if (!bcm_player_write32(0x10000C00, 0))
        return false;
    bcm2722_player_error = "Apple VideoCore launch write failed";
    if (!bcm_player_write32(0x10000400, 0xA5A50002))
        return false;
    bcm2722_player_error = "Apple VideoCore ready timed out";
    if (!bcm_player_wait32_nonzero(BCMA_COMMAND,
                                   BCM_PLAYER_START_TIMEOUT))
        return false;

    /* RetailOS transitions its loader state from 2 to 3 and calls
     * 0x10288058.  That routine re-runs only the common two-channel
     * bootstrap, reads one 16-byte block at 0x1f0, requires word 2 to be
     * exactly 1, and accepts word 3 only when non-zero and 4-byte aligned. */
    bcm2722_player_stage = BCM2722_VIDEO_STAGE_RUNTIME_BOOTSTRAP;
    bcm2722_player_error = "Apple runtime host interface timed out";
    if (!bcm_player_bootstrap_retail())
        return false;

    bcm2722_player_error = "Apple runtime service header timed out";
    if (!bcm_player_read_buffer_raw(0x1f0, runtime_header,
                                    sizeof(runtime_header)))
        return false;

    runtime_ready = (unsigned)runtime_header[8] |
                    ((unsigned)runtime_header[9] << 8) |
                    ((unsigned)runtime_header[10] << 16) |
                    ((unsigned)runtime_header[11] << 24);
    service_base = (unsigned)runtime_header[12] |
                   ((unsigned)runtime_header[13] << 8) |
                   ((unsigned)runtime_header[14] << 16) |
                   ((unsigned)runtime_header[15] << 24);
    if (runtime_ready != 1)
    {
        bcm2722_player_error = "Apple runtime did not become ready";
        return false;
    }
    if (service_base == 0 || (service_base & 3) != 0)
    {
        bcm2722_player_error = "Apple runtime service table invalid";
        return false;
    }
    return true;
}

static void bcm_player_restore_lcd(void)
{
    const char *error = bcm2722_player_error;

    bcm2722_player_active = false;
    bcm2722_player_fault = false;
    bcm_retail_powerdown();
    lcd_state.update_timeout = current_tick;
    lcd_awake();

    /* bcm_retail_powerdown() disables the backlight circuit directly.  A
     * bare lcd_awake() restores the NOR VMCS but cannot re-enable that
     * circuit, which was the source of the persistent black/no-backlight
     * failure after an unsuccessful movie launch. */
    _backlight_hw_enable(true);
    _backlight_led_on();
    bcm2722_videoout_signal_active = false;
    if (bcm_videoout_should_enable())
        bcm2722_videoout_mirror_if_due(true, 0, 0,
                                       LCD_WIDTH, LCD_HEIGHT);
    bcm2722_player_fault = false;
    bcm2722_player_error = error;
}

bool bcm2722_video_start(const void *vmcs, size_t length)
{
    long deadline;

    bcm2722_player_stage = BCM2722_VIDEO_STAGE_VALIDATE;
    if (vmcs == NULL || length == 0 || (length & 3) != 0)
    {
        bcm2722_player_error = "Apple VideoCore image invalid";
        return false;
    }
    if (flash_vmcs_length == 0)
    {
        bcm2722_player_error = "iPod VideoCore boot image unavailable";
        return false;
    }
    if (bcm2722_player_active)
    {
        bcm2722_player_error = "VideoCore already active";
        return false;
    }

    bcm2722_player_fault = false;
    bcm2722_player_stage = BCM2722_VIDEO_STAGE_LCD_HANDOFF;
    bcm2722_player_error = "LCD handoff failed";

    if (!lcd_state.display_on)
        lcd_awake();
    if (!lcd_state.display_on)
        return false;

    deadline = current_tick + BCM_PLAYER_BUS_TIMEOUT;
    while (lcd_state.state != LCD_IDLE)
    {
        if (!TIME_BEFORE(current_tick, deadline))
        {
            bcm2722_player_error = "LCD handoff timed out";
            return false;
        }
        yield();
    }

    lcd_state.display_on = false;
    tick_remove_task(&lcd_tick);
    /* The retail movie VMCS replaces the resident display VMCS and owns the
     * encoder directly; a later resident restore must arm its TV route anew. */
    bcm2722_videoout_signal_active = false;

    if (!bcm_player_start_retail_image(vmcs, length))
    {
        bcm_player_restore_lcd();
        return false;
    }
    bcm2722_player_active = true;
    bcm2722_player_stage = BCM2722_VIDEO_STAGE_READY;
    bcm2722_player_error = "VideoCore ready";
    return true;
}

void bcm2722_video_stop(void)
{
    if (!bcm2722_player_active)
        return;

    bcm2722_player_stage = BCM2722_VIDEO_STAGE_STOPPED;
    bcm2722_player_error = "VideoCore stopped";
    bcm_player_restore_lcd();
}

bool bcm2722_video_active(void)
{
    return bcm2722_player_active;
}

bool bcm2722_video_faulted(void)
{
    return bcm2722_player_fault;
}

const char *bcm2722_video_error(void)
{
    return bcm2722_player_error;
}

enum bcm2722_video_stage bcm2722_video_get_stage(void)
{
    return bcm2722_player_stage;
}

bool bcm2722_read32(uint32_t address, uint32_t *value)
{
    unsigned result;

    if (value == NULL || !bcm2722_player_active ||
        !bcm_player_read32(address, &result))
        return false;
    *value = result;
    return true;
}

bool bcm2722_write32(uint32_t address, uint32_t value)
{
    return bcm2722_player_active && bcm_player_write32(address, value);
}

bool bcm2722_read16(uint32_t address, uint16_t *value)
{
    unsigned word;

    if (value == NULL || !bcm2722_player_active ||
        !bcm_player_read32(address & ~3u, &word))
        return false;
    *value = address & 2 ? word >> 16 : word;
    return true;
}

bool bcm2722_write16(uint32_t address, uint16_t value)
{
    if (!bcm2722_player_active || !bcm_player_write_addr(address))
        return false;
    BCM_DATA = value;
    return true;
}

bool bcm2722_read_buffer(uint32_t address, void *buffer, size_t length)
{
    uint8_t *destination = buffer;

    if (buffer == NULL || (address & 3) != 0 || (length & 3) != 0 ||
        !bcm2722_player_active)
        return false;

    while (length != 0)
    {
        unsigned value;

        if (!bcm_player_read32(address, &value))
            return false;

        destination[0] = value;
        destination[1] = value >> 8;
        destination[2] = value >> 16;
        destination[3] = value >> 24;
        destination += 4;
        address += 4;
        length -= 4;
    }
    return true;
}

bool bcm2722_write_buffer(uint32_t address, const void *buffer,
                          size_t length)
{
    if (buffer == NULL || (address & 1) != 0 || (length & 3) != 0 ||
        !bcm2722_player_active)
        return false;

    return bcm_player_write_buffer_raw(address, buffer, length);
}

bool bcm2722_notify(void)
{
    if (!bcm2722_player_active || bcm2722_player_fault)
        return false;
    BCM_CONTROL = 0x31;
    return true;
}
#endif /* !BOOTLOADER */
