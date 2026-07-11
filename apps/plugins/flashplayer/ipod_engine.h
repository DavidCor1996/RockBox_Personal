/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_ /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/                \/
 *
 * iPod-focused runtime services for the Flash player plugin.
 *
 ****************************************************************************/

#ifndef FLASHPLAYER_IPOD_ENGINE_H
#define FLASHPLAYER_IPOD_ENGINE_H

#include <stddef.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

struct ipod_engine_profile {
    const char *target;
    const char *model;
    int lcd_width;
    int lcd_height;
    size_t plugin_buffer_min;
    bool supported;
};

struct ipod_engine_memory {
    unsigned char *plugin;
    size_t plugin_size;
    void *shared;
    size_t shared_size;
    bool shared_acquired;
    bool cpu_boosted;
};

struct ipod_engine_frame_clock {
    int fps_x100;
    long start_tick;
    long last_tick;
    int frame_index;
};

void ipod_engine_get_profile(struct ipod_engine_profile *profile);
bool ipod_engine_supported_target(void);

void ipod_engine_memory_init(struct ipod_engine_memory *memory);
bool ipod_engine_acquire_memory(struct ipod_engine_memory *memory);
void ipod_engine_release_memory(struct ipod_engine_memory *memory);

void ipod_engine_frame_clock_reset(struct ipod_engine_frame_clock *clock,
                                   int fps_x100);
int ipod_engine_frame_timeout(const struct ipod_engine_frame_clock *clock);
float ipod_engine_frame_advance(struct ipod_engine_frame_clock *clock);

#endif
