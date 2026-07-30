/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Live TV launcher. The program guide itself runs inside mpegplayer so the
 * tuned channel can keep decoding into the corner of the guide; see
 * docs/livetv-directv-guide-spec.md.
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

#include "plugin.h"

#define LIVETV_ROOT          "/Videos/LiveTV"
#define LIVETV_CHANNELS      LIVETV_ROOT "/channels.tsv"
#define LIVETV_GUIDE         LIVETV_ROOT "/guide.tsv"
#define LIVETV_PLAYER_PATH   VIEWERS_DIR "/mpegplayer.rock"
#define LIVETV_PARAM_PREFIX  "-livetv:"
#define LIVETV_DM_PARAM_PREFIX "-livetvdm:"

enum plugin_status plugin_start(const void *parameter)
{
    static char launch_param[MAX_PATH + 16];

    if (!rb->dir_exists(LIVETV_ROOT))
    {
        rb->splash(HZ * 3, "Sync Live TV from rockpod first");
        return PLUGIN_ERROR;
    }

    if (!rb->file_exists(LIVETV_CHANNELS) || !rb->file_exists(LIVETV_GUIDE))
    {
        rb->splash(HZ * 3, "No Live TV channel guide on this iPod");
        return PLUGIN_ERROR;
    }

    rb->snprintf(launch_param, sizeof(launch_param), "%s%s",
                 parameter && !rb->strcmp(parameter, "-desktop") ?
                    LIVETV_DM_PARAM_PREFIX : LIVETV_PARAM_PREFIX,
                 LIVETV_ROOT);

    return rb->plugin_open(LIVETV_PLAYER_PATH, launch_param);
}
