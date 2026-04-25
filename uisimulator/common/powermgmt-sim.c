/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Heikki Hannikainen, Uwe Freese
 * Revisions copyright (C) 2005 by Gerald Van Baren
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
#include <time.h>
#include "kernel.h"
#include "powermgmt.h"
#include "power.h"

#define BATT_MINMVOLT   3300      /* minimum millivolts of battery */
#define BATT_MAXMVOLT   4300      /* maximum millivolts of battery */
#define BATT_MAXRUNTIME (10 * 60) /* maximum runtime with full battery in
                                     minutes */
/* Number of millivolts to discharge the battery every second */
#define BATT_DISCHARGE_STEP ((BATT_MAXMVOLT - BATT_MINMVOLT) / 100)
/* Number of millivolts to charge the battery every second */
#define BATT_CHARGE_STEP (BATT_DISCHARGE_STEP * 2)
#if CONFIG_CHARGING >= CHARGING_MONITOR
/* Lower battery discharge while simulated sleep is active. */
#define BATT_SLEEP_DISCHARGE_STEP (BATT_DISCHARGE_STEP / 4)
#endif

static bool charging = false;
static bool sim_usb_online = false;
static bool sim_main_online = false;
static bool sim_charge_enabled = true;
static bool sim_sleeping = false;
static unsigned int batt_millivolts = BATT_MAXMVOLT;
static unsigned int batt_percent = 100;
static unsigned int batt_runtime = BATT_MAXRUNTIME;
static unsigned int batt_current = 0;

void powermgmt_init_target(void)
{
    sim_usb_online = false;
    sim_main_online = false;
    sim_charge_enabled = true;
    sim_sleeping = false;
}

static void battery_status_update(void)
{
    /* Delay next battery update until tick */
    static long update_after_tick = 0;
    bool ext_power;
    unsigned int discharge_step;

    if(TIME_BEFORE(current_tick, update_after_tick))
        return;

    update_after_tick = current_tick + HZ;

    ext_power = sim_usb_online || sim_main_online;

#if CONFIG_CHARGING >= CHARGING_MONITOR
    charging = ext_power && sim_charge_enabled && batt_millivolts < BATT_MAXMVOLT;
#else
    charging = ext_power && sim_charge_enabled;
#endif

    if (charging)
    {
        batt_millivolts += BATT_CHARGE_STEP;
    }
    else if (!ext_power)
    {
        discharge_step = BATT_DISCHARGE_STEP;
#if CONFIG_CHARGING >= CHARGING_MONITOR
        if (sim_sleeping && BATT_SLEEP_DISCHARGE_STEP > 0)
            discharge_step = BATT_SLEEP_DISCHARGE_STEP;
#endif
        if (discharge_step == 0)
            discharge_step = 1;

        if (batt_millivolts > discharge_step)
            batt_millivolts -= discharge_step;
        else
            batt_millivolts = BATT_MINMVOLT;
    }

    if (batt_millivolts > BATT_MAXMVOLT)
        batt_millivolts = BATT_MAXMVOLT;
    if (batt_millivolts < BATT_MINMVOLT)
        batt_millivolts = BATT_MINMVOLT;

    batt_percent = ((float) (batt_millivolts - BATT_MINMVOLT) / (BATT_MAXMVOLT - BATT_MINMVOLT)) * 100;
    batt_runtime = batt_percent * BATT_MAXRUNTIME;
    if (charging)
        batt_current = BATT_CHARGE_STEP;
    else if (!ext_power)
        batt_current = sim_sleeping && BATT_SLEEP_DISCHARGE_STEP > 0
            ? BATT_SLEEP_DISCHARGE_STEP : BATT_DISCHARGE_STEP;
    else
        batt_current = 0;
}

unsigned short battery_level_disksafe = 3200;
unsigned short battery_level_shutoff = 3200;

/* make the simulated curve nicely linear */
unsigned short percent_to_volt_discharge[11] =
{ 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200, 4300 };
unsigned short percent_to_volt_charge[11] =
{ 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200, 4300  };

#if CONFIG_BATTERY_MEASURE & VOLTAGE_MEASURE
int _battery_voltage(void)
{
    battery_status_update();
    return batt_millivolts;
}
#endif

#if CONFIG_BATTERY_MEASURE & PERCENTAGE_MEASURE
int _battery_level(void)
{
    battery_status_update();
    return batt_percent;
}
#endif

#if (CONFIG_BATTERY_MEASURE & TIME_MEASURE)
int _battery_time(void)
{
    battery_status_update();
    return batt_runtime;
}
#endif

#if (CONFIG_BATTERY_MEASURE & CURRENT_MEASURE)
int _battery_current(void)
{
    battery_status_update();
    return batt_current;
}
#endif

#if CONFIG_CHARGING
unsigned int power_input_status(void)
{
    unsigned int status = POWER_INPUT_NONE;

    if (sim_main_online)
        status |= POWER_INPUT_MAIN;
    if (sim_usb_online)
        status |= POWER_INPUT_USB;

    if (sim_charge_enabled)
    {
        if (sim_main_online)
            status |= POWER_INPUT_MAIN_CHARGER;
        if (sim_usb_online)
            status |= POWER_INPUT_USB_CHARGER;
    }

#ifdef HAVE_BATTERY_SWITCH
    status |= POWER_INPUT_BATTERY;
#endif

    return status;
}

bool charging_state(void)
{
    battery_status_update();
    return charging;
}
#endif

void sim_power_set_usb_online(bool online)
{
    sim_usb_online = online;
}

bool sim_power_usb_online(void)
{
    return sim_usb_online;
}

void sim_power_set_main_online(bool online)
{
    sim_main_online = online;
}

bool sim_power_main_online(void)
{
    return sim_main_online;
}

void sim_power_set_charge_enabled(bool enabled)
{
    sim_charge_enabled = enabled;
}

bool sim_power_charge_enabled(void)
{
    return sim_charge_enabled;
}

void sim_power_set_sleeping(bool sleeping)
{
    sim_sleeping = sleeping;
}

bool sim_power_sleeping(void)
{
    return sim_sleeping;
}

#ifdef HAVE_ACCESSORY_SUPPLY
void accessory_supply_set(bool enable)
{
    (void)enable;
}
#endif

#ifdef HAVE_LINEOUT_POWEROFF
void lineout_set(bool enable)
{
    (void)enable;
}
#endif

#ifdef HAVE_REMOTE_LCD
bool remote_detect(void)
{
    return true;
}
#endif

#ifdef HAVE_BATTERY_SWITCH
unsigned int input_millivolts(void)
{
    if ((power_input_status() & POWER_INPUT_BATTERY) == 0) {
        /* Just return a safe value if battery isn't connected */
        return 4050;
    }

    return battery_voltage();
}
#endif
