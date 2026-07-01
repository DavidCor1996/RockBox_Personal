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

#include "video.h"

static fb_data scummvm_framebuffer[SCUMMVM_SURFACE_W * SCUMMVM_SURFACE_H];

void scummvm_video_init(struct scummvm_video *video)
{
    video->pixels = scummvm_framebuffer;
    video->width = SCUMMVM_SURFACE_W;
    video->height = SCUMMVM_SURFACE_H;
    video->screen_x = (LCD_WIDTH - SCUMMVM_SURFACE_W) / 2;
    video->screen_y = (LCD_HEIGHT - SCUMMVM_SURFACE_H) / 2;
    scummvm_video_clear(video, LCD_RGBPACK(0, 0, 0));
}

fb_data *scummvm_video_pixels(struct scummvm_video *video)
{
    return video->pixels;
}

void scummvm_video_clear(struct scummvm_video *video, fb_data color)
{
    int i;
    int count = video->width * video->height;

    for (i = 0; i < count; i++)
        video->pixels[i] = color;
}

void scummvm_video_put_pixel(struct scummvm_video *video, int x, int y,
                             fb_data color)
{
    if (x < 0 || y < 0 || x >= video->width || y >= video->height)
        return;

    video->pixels[y * video->width + x] = color;
}

void scummvm_video_demo_pattern(struct scummvm_video *video)
{
    int x;
    int y;

    for (y = 0; y < video->height; y++) {
        for (x = 0; x < video->width; x++) {
            int r = (x * 255) / (video->width - 1);
            int g = (y * 255) / (video->height - 1);
            int b = ((x ^ y) & 0x3f) * 4;
            video->pixels[y * video->width + x] = LCD_RGBPACK(r, g, b);
        }
    }
}

void scummvm_video_present(const struct scummvm_video *video)
{
    rb->lcd_bitmap(video->pixels, video->screen_x, video->screen_y,
                   video->width, video->height);
}
