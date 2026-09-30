#ifndef VIDEOOUT_6G_H
#define VIDEOOUT_6G_H

#include <stdbool.h>
#include <stdint.h>
#include "videoout.h"

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

void ipod6g_videoout_set_preferences(int screen, int overscan);
void ipod6g_videoout_ui_batch(bool enabled);
void ipod6g_videoout_ui_owner(bool enabled);
void ipod6g_videoout_set_video(bool tv_canvas);
struct videoout_tv_frame;
void ipod6g_videoout_prepare_frame(const struct videoout_tv_frame *frame);
void ipod6g_videoout_present_ui(const uint16_t *p, int w, int h);
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
#ifdef IPOD6G_VIDEOOUT_HIRES_TEST
bool ipod6g_videoout_test_hires_pattern(void);
#endif
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
bool ipod6g_videoout_hibernate_suspend(void);
bool ipod6g_videoout_hibernate_resume(void);
bool ipod6g_videoout_mirror_yuv420(const unsigned char *luma,
                                   const unsigned char *cb,
                                   const unsigned char *cr,
                                   int source_x, int source_y,
                                   int source_stride,
                                   int x, int y, int width, int height);
void ipod6g_videoout_mirror_rgb565(const void *source, int x, int y,
                                   int width, int height, int stride);
void ipod6g_videoout_set_mode(enum ipod6g_videoout_mode mode,
                              const void *framebuffer,
                              int width, int height);
void ipod6g_videoout_accessory_state(
    enum ipod6g_videoout_accessory accessory);

#ifdef VIDEOOUT_ENHANCED_TEST
bool ipod6g_videoout_native_yuv(const struct videoout_frame *frame,
                              unsigned char * const lcd_planes[3]);
bool ipod6g_videoout_art_write(unsigned offset, const void *data, unsigned size);
bool ipod6g_videoout_art_finish(void);
void ipod6g_videoout_art_clear(void);
bool ipod6g_videoout_art_bind(const uint16_t *source, int stride, int x, int y);
#endif
#endif /* VIDEOOUT_6G_H */
