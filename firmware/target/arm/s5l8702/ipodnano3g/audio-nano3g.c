/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: audio-nano2g.c 23095 2009-10-11 09:17:12Z dave $
 *
 * Copyright (C) 2006 by Michael Sevakis
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
#include "system.h"
#include "cpu.h"
#include "audio.h"
#include "sound.h"
#include "logf.h"

/*
 * Nano 3G playback output scaffold.
 *
 * These hooks intentionally avoid touching hardware registers until the real
 * DAC/power/clock path is confirmed on device. They are called by the shared
 * S5L8702 PCM driver for Nano 3G builds only.
 */
void ipodnano3g_audio_output_init(void)
{
    logf("nano3g audio scaffold: init");
}

void ipodnano3g_audio_output_start(const void *addr, size_t size)
{
    (void)addr;
    (void)size;
    logf("nano3g audio scaffold: start");
}

void ipodnano3g_audio_output_stop(void)
{
    logf("nano3g audio scaffold: stop");
}

void ipodnano3g_audio_output_submit(const void *addr, size_t size)
{
    (void)addr;
    (void)size;
}


#if INPUT_SRC_CAPS != 0
void audio_set_output_source(int source)
{
    if ((unsigned)source >= AUDIO_NUM_SOURCES)
        source = AUDIO_SRC_PLAYBACK;
} /* audio_set_output_source */

void audio_input_mux(int source, unsigned flags)
{
    (void)flags;
    /* Prevent pops from unneeded switching */
    static int last_source = AUDIO_SRC_PLAYBACK;

    switch (source)
    {
        default:                        /* playback - no recording */
            source = AUDIO_SRC_PLAYBACK;
        case AUDIO_SRC_PLAYBACK:
#ifdef HAVE_RECORDING
            if (source != last_source)
            {
                audiohw_set_monitor(false);
                audiohw_disable_recording();
            }
#endif
        break;

#ifdef HAVE_MIC_REC
        case AUDIO_SRC_MIC:             /* recording only */
            if (source != last_source)
            {
                audiohw_set_monitor(false);
                audiohw_enable_recording(true);  /* source mic */
            }
        break;
#endif

#ifdef HAVE_LINE_REC
        case AUDIO_SRC_LINEIN:          /* recording only */
            if (source != last_source)
            {
                audiohw_set_monitor(false);
                audiohw_enable_recording(false); /* source line */
            }
        break;
#endif
    } /* end switch */

    last_source = source;
} /* audio_input_mux */
#endif /* INPUT_SRC_CAPS != 0 */
