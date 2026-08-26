#ifndef VIDEOOUT_6G_H
#define VIDEOOUT_6G_H

#include <stdbool.h>
#include <stdint.h>

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

struct ipod6g_videoout_diagnostics
{
    uint32_t mixer_status;
    uint32_t mixer_config;
    uint32_t graphic_config;
    uint32_t graphic_base;
    uint32_t graphic_position;
    uint32_t graphic_size;
    uint32_t graphic_formats;
    uint32_t graphic_destination;
    uint32_t background;
    uint32_t video_enable;
    uint32_t video_mode;
    uint32_t video_image_width;
    uint32_t video_image_height;
    uint32_t video_plane0;
    uint32_t video_plane1;
    uint32_t video_plane2;
    uint32_t video_unused;
    uint32_t video_plane_mode;
    uint32_t video_spans;
    uint32_t video_live_plane0;
    uint32_t video_live_plane1;
    uint32_t video_live_plane2;
    uint32_t video_source;
    uint32_t video_destination;
    uint32_t video_ratio;
    uint32_t sdo_clock;
    uint32_t sdo_config;
    uint32_t sdo_dac;
    uint32_t sdo_field;
};

bool ipod6g_videoout_enable_sync(void);
bool ipod6g_videoout_show_background(uint32_t ycbcr);
bool ipod6g_videoout_show_framebuffer(const void *framebuffer,
                                      int width, int height);
bool ipod6g_videoout_test_config(const void *framebuffer,
                                 int width, int height,
                                 uint32_t mixer_config, uint32_t alpha);
bool ipod6g_videoout_test_xrgb(const void *framebuffer,
                               int width, int height);
bool ipod6g_videoout_test_p420_pattern(void);
bool ipod6g_videoout_test_p420_geometry_pattern(
    unsigned destination_x, unsigned destination_y,
    unsigned destination_width, unsigned destination_height,
    unsigned source_height, unsigned vertical_ratio);
bool ipod6g_videoout_test_p420_framebuffer(const void *framebuffer,
                                           int width, int height);
bool ipod6g_videoout_test_p420_framebuffer_window(
    const void *framebuffer, int width, int height,
    unsigned destination_x, unsigned destination_y,
    unsigned destination_width, unsigned destination_height);
bool ipod6g_videoout_test_p420_framebuffer_geometry(
    const void *framebuffer, int width, int height,
    unsigned destination_x, unsigned destination_y,
    unsigned destination_width, unsigned destination_height,
    unsigned source_height, unsigned vertical_ratio,
    bool mirror_updates);
void ipod6g_videoout_refresh(const void *framebuffer);
void ipod6g_videoout_get_diagnostics(
    struct ipod6g_videoout_diagnostics *diagnostics);
bool ipod6g_videoout_disable(void);
bool ipod6g_videoout_active(void);
bool ipod6g_videoout_lcd_clock_required(void);
void ipod6g_videoout_mirror_rgb565(const void *source, int x, int y,
                                   int width, int height, int stride);
void ipod6g_videoout_set_mode(enum ipod6g_videoout_mode mode,
                              const void *framebuffer,
                              int width, int height);
void ipod6g_videoout_accessory_state(
    enum ipod6g_videoout_accessory accessory);

#endif /* VIDEOOUT_6G_H */
