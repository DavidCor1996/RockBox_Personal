/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: power-nano2g.c 28190 2010-10-01 18:09:10Z Buschel $
 *
 * Copyright © 2009 Bertrik Sikken
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
#include <stdbool.h>
#include "config.h"
#include "inttypes.h"
#include "s5l87xx.h"
#include "power.h"
#include "panic.h"
#include "pmu-target.h"
#include "usb_core.h"   /* for usb_charging_maxcurrent_change */
#include "bringup-nano3g.h"

void power_init(void)
{
    nano3g_boottrace_log("power_init");

    pmu_init();

    if (!nano3g_safe_mode_enabled())
        pmu_set_usblimit(false);  /* limit to 100mA */
}

void power_off(void)
{
    nano3g_boottrace_log("power_off");

    if (nano3g_safe_mode_enabled())
        nano3g_failsafe_halt("power_off in safe bringup");

    pmu_enter_standby();
    while(1);
}

#if CONFIG_CHARGING

#ifdef HAVE_USB_CHARGING_ENABLE
void usb_charging_maxcurrent_change(int maxcurrent)
{
    if (nano3g_safe_mode_enabled())
        return;

    bool fast_charge = (maxcurrent >= 500);
    pmu_set_usblimit(fast_charge);
}
#endif

unsigned int power_input_status(void)
{
    unsigned int status = POWER_INPUT_NONE;
    if (usb_detect() == USB_INSERTED)
        status |= POWER_INPUT_USB_CHARGER;
    if (pmu_firewire_present())
        status |= POWER_INPUT_MAIN_CHARGER;
    return status;
}

bool charging_state(void)
{
    /* Hardware charge-status signal is still unknown on Nano 3G.
     * Keep a conservative scaffold: report charging whenever an
     * external charging-capable source is present. */
    return (power_input_status() & POWER_INPUT_CHARGER) != 0;
}
#endif /* CONFIG_CHARGING */
