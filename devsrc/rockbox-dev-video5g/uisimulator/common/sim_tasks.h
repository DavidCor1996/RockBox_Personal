/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2009 by Jens Arnold
 *
 * Rockbox simulator specific tasks
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


#ifndef __SIM_TASKS_H__
#define __SIM_TASKS_H__

#include <stdbool.h>

void sim_tasks_init(void);
void sim_trigger_screendump(void);
void sim_trigger_usb(bool inserted);
void sim_trigger_usb_powered_only(bool enabled);
bool sim_usb_inserted(void);
bool sim_usb_powered_only(void);
#ifdef HAVE_HOTSWAP
void sim_trigger_external(bool inserted);
#endif
void sim_trigger_hp(bool inserted);
void sim_trigger_lo(bool inserted);

/* Simulator power model controls (Phase 9). */
void sim_power_set_usb_online(bool online);
bool sim_power_usb_online(void);
void sim_power_set_main_online(bool online);
bool sim_power_main_online(void);
void sim_power_set_charge_enabled(bool enabled);
bool sim_power_charge_enabled(void);
void sim_power_set_sleeping(bool sleeping);
bool sim_power_sleeping(void);
#endif
