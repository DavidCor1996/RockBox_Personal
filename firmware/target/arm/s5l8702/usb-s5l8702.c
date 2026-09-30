/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id$
*
* Copyright (C) 2014 Michael Sparmann
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
#include <inttypes.h>

#include "config.h"
#include "usb.h"
#include "usb_drv.h"
#ifdef HAVE_USBSTACK
#include "usb_core.h"
#endif

#include "s5l87xx.h"
#include "clocking-s5l8702.h"
#include "usb-designware.h"
#ifdef HAVE_USB_DW_CAPABILITY_SNAPSHOT
#include "usb_dw_capabilities.h"

static struct usb_dw_capabilities capability_snapshot;

static void usb_dw_read_capabilities(struct usb_dw_capabilities *caps)
{
    caps->core_id = DWC_GSNPSID;
    caps->hwcfg[0] = DWC_GHWCFG1;
    caps->hwcfg[1] = DWC_GHWCFG2;
    caps->hwcfg[2] = DWC_GHWCFG3;
    caps->hwcfg[3] = DWC_GHWCFG4;
}

void usb_dw_target_capture_capabilities(void)
{
    struct usb_dw_capabilities first = {0}, second = {0};
    usb_dw_read_capabilities(&first);
    usb_dw_read_capabilities(&second);
    usb_dw_capabilities_record(&capability_snapshot, &first, &second);
}

void usb_dw_get_capabilities(struct usb_dw_capabilities *caps)
{
    /* Copy RAM only. Prevent a USB worker context switch during the copy. */
    int oldlevel = disable_irq_save();
    *caps = capability_snapshot;
    restore_irq(oldlevel);
}
#endif
#include "ipodnano3g/bringup-nano3g.h"
#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
#include "ipod6g/hibernate-6g.h"
#endif


const struct usb_dw_config usb_dw_config =
{
    .phytype = DWC_PHYTYPE_UTMI_16,

    /* Available FIFO memory: 0x820 words */
    .rx_fifosz   = 0x360,
    .nptx_fifosz = 0x40,   /* 1 dedicated FIFO for IN0 */
    .ptx_fifosz  = 0x180,  /* 3 dedicated FIFOs for IN1,IN3,IN5 */

#ifdef USB_DW_ARCH_SLAVE
    .disable_double_buffering = false,
#else
    .ahb_burst_len = HBSTLEN_INCR8,
    .ahb_threshold = 8,
#endif
};

void usb_dw_target_enable_clocks()
{
#if defined(IPOD_NANO3G)
    if (nano3g_safe_mode_enabled() && !NANO3G_NATIVE_USB_RETURN)
        return;
#endif

    clockgate_enable(CLOCKGATE_USBOTG, true);
    clockgate_enable(CLOCKGATE_USBPHY, true);

    OPHYPWR = 0;  /* PHY: Power up */
    udelay(10);
    OPHYUNK1 = 1;
    OPHYUNK2 = 0xe3f;
    ORSTCON = 1;  /* PHY: Assert Software Reset */
    udelay(10);
    ORSTCON = 0;  /* PHY: Deassert Software Reset */
    udelay(10);
    OPHYUNK3 = 0x600;
    OPHYCLK = USB_DW_CLOCK;
    udelay(400);
}

void usb_dw_target_disable_clocks()
{
#if defined(IPOD_NANO3G)
    if (nano3g_safe_mode_enabled() && !NANO3G_NATIVE_USB_RETURN)
        return;
#endif

#if (CONFIG_CPU == S5L8702)
    OPHYPWR = 0xf;  /* PHY: Power down */
    udelay(10);
    ORSTCON = 7;  /* PHY: Assert Software Reset */
    udelay(10);
#elif (CONFIG_CPU == S5L8720)
    OPHYPWR = 0x1f;  /* PHY: Power down */
    ORSTCON = 1;  /* PHY: Assert Software Reset */
    udelay(1000);
#endif

    clockgate_enable(CLOCKGATE_USBOTG, false);
    clockgate_enable(CLOCKGATE_USBPHY, false);
}

void usb_dw_target_enable_irq()
{
#if defined(IPOD_NANO3G)
    if (nano3g_safe_mode_enabled() && !NANO3G_NATIVE_USB_RETURN)
        return;
#endif

    VICINTENABLE(IRQ_USB_FUNC >> 5) = 1 << (IRQ_USB_FUNC & 0x1f);
}

void usb_dw_target_disable_irq()
{
#if defined(IPOD_NANO3G)
    if (nano3g_safe_mode_enabled())
        return;
#endif

    VICINTENCLEAR(IRQ_USB_FUNC >> 5) = 1 << (IRQ_USB_FUNC & 0x1f);
}

void usb_dw_target_clear_irq()
{
}

/* RB API */
static int usb_status = USB_EXTRACTED;

void usb_enable(bool on)
{
#ifdef HAVE_USBSTACK
#if defined(IPOD_NANO3G)
    if (nano3g_safe_mode_enabled())
        return;
#endif

    if (on)
    {
#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
        ipod6g_hibernate_runtime_checkpoint(
                IPOD6G_HIBERNATE_DIAG_USB_CORE_ENTER);
#endif
        usb_core_init();
#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
        ipod6g_hibernate_runtime_checkpoint(
                IPOD6G_HIBERNATE_DIAG_USB_CORE_READY);
#endif
    }
    else
    {
#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
        ipod6g_hibernate_runtime_checkpoint(
                IPOD6G_HIBERNATE_DIAG_USB_CORE_EXIT);
#endif
        usb_core_exit();
#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
        ipod6g_hibernate_runtime_checkpoint(
                IPOD6G_HIBERNATE_DIAG_USB_CORE_OFF);
#endif
    }
#else
    (void)on;
#endif
}

int usb_detect(void)
{
    return usb_status;
}

void usb_insert_int(void)
{
    usb_status = USB_INSERTED;
#ifdef USB_STATUS_BY_EVENT
    usb_status_event(USB_INSERTED);
#endif
}

void usb_remove_int(void)
{
    usb_status = USB_EXTRACTED;
#ifdef USB_STATUS_BY_EVENT
    usb_status_event(USB_EXTRACTED);
#endif
}

void usb_init_device(void)
{
#if defined(IPOD_NANO3G)
    if (nano3g_safe_mode_enabled() && !NANO3G_NATIVE_USB_RETURN)
    {
        usb_status = USB_EXTRACTED;
        return;
    }
#endif

    /* Power up the core clocks to allow writing
       to some registers needed to power it down */
    usb_dw_target_disable_irq();
    usb_dw_target_enable_clocks();

    usb_drv_exit();
}

#if defined(IPOD_6G) && defined(IPOD6G_HIBERNATE_STAGE3) && \
        IPOD6G_HIBERNATE_STAGE3 && !defined(BOOTLOADER)
void usb_hibernate_resume(void)
{
    /* Standby loses the PHY/controller state while the retained USB thread,
     * queue and endpoint objects remain valid. Put hardware alone into the
     * same known-off state used at cold boot; the deferred insertion event
     * will start it through the normal retained USB thread. */
    usb_init_device();
}
#endif
