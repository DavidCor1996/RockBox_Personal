#ifndef VIDEOOUT_6G_H
#define VIDEOOUT_6G_H

#include <stdbool.h>

enum ipod6g_videoout_mode
{
    IPOD6G_VIDEOOUT_OFF = 0,
    IPOD6G_VIDEOOUT_AUTO,
    IPOD6G_VIDEOOUT_ON,
};

enum ipod6g_videoout_accessory
{
    IPOD6G_VIDEOOUT_ACCESSORY_NONE = 0,
    IPOD6G_VIDEOOUT_ACCESSORY_PENDING,
    IPOD6G_VIDEOOUT_ACCESSORY_VIDEO,
    IPOD6G_VIDEOOUT_ACCESSORY_BLOCKED,
};

bool ipod6g_videoout_enable_sync(void);
bool ipod6g_videoout_show_framebuffer(const void *framebuffer,
                                      int width, int height);
void ipod6g_videoout_refresh(const void *framebuffer);
bool ipod6g_videoout_disable(void);
bool ipod6g_videoout_active(void);
void ipod6g_videoout_mirror_rgb565(const void *source, int x, int y,
                                   int width, int height, int stride);
void ipod6g_videoout_set_mode(enum ipod6g_videoout_mode mode,
                              const void *framebuffer,
                              int width, int height);
void ipod6g_videoout_accessory_state(
    enum ipod6g_videoout_accessory accessory);

#endif /* VIDEOOUT_6G_H */
