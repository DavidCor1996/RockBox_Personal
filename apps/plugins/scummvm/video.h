/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ |__   _______  ___
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/            \/
 *
 * Copyright (C) 2026 Rockpod contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/

#ifndef SCUMMVM_VIDEO_H
#define SCUMMVM_VIDEO_H

#include "scummvm.h"

#define SCUMMVM_SURFACE_W 320
#define SCUMMVM_SURFACE_H 200

struct scummvm_video {
    fb_data *pixels;
    int width;
    int height;
    int screen_x;
    int screen_y;
};

#ifdef __cplusplus
extern "C" {
#endif

void scummvm_video_init(struct scummvm_video *video);
fb_data *scummvm_video_pixels(struct scummvm_video *video);
void scummvm_video_clear(struct scummvm_video *video, fb_data color);
void scummvm_video_put_pixel(struct scummvm_video *video, int x, int y,
                             fb_data color);
void scummvm_video_demo_pattern(struct scummvm_video *video);
void scummvm_video_present(const struct scummvm_video *video);

#ifdef __cplusplus
}
#endif

#endif
