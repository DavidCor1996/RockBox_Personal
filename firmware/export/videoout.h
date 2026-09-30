/* Generic composite-output availability. */
#ifndef VIDEOOUT_H
#define VIDEOOUT_H
#include "config.h"
#include <stdbool.h>
#include <stdint.h>

/* Borrowed decoded YUV420 planes and their rectangle on the LCD canvas.
 * The presentation call consumes these synchronously; no pointer is retained.
 * Width, height, stride and rectangle coordinates must all be even. */
struct videoout_frame
{
    const uint8_t *planes[3];
    int width, height, stride;
    int x, y, display_width, display_height;
};

#if defined(HAVE_VIDEOOUT_NATIVE_YUV) && !defined(SIMULATOR)
/* Present the LCD composition and preserve decoded detail on the TV.
 * Modified composition tiles (captions/controls) retain LCD resolution. */
void videoout_blit_yuv(const struct videoout_frame *frame,
                      unsigned char * const lcd_planes[3]);
#endif
#include "videoout_geometry.h"
struct videoout_tv_frame {
    const unsigned char *planes[3];
    int width, height, stride, dar_n, dar_d;
    /* Borrowed only for the synchronous prepare call. Decoder storage is
     * never retained by scanout. Zero strides use the legacy Y/UV layout. */
    int strides[3];
    int coded_width, coded_height;
    struct videoout_rect visible;
    enum { VIDEOOUT_YUV420P } format;
    enum { VIDEOOUT_SD_LIMITED } color;
    uint32_t pts_ms, duration_ms;
    bool fill;
    const unsigned char *caption; /* 288x44 packed 2-bit grayscale */
    int caption_y, caption_canvas_width;
    const uint16_t *overlay;
    int overlay_width, overlay_y, overlay_height;
};
#if defined(HAVE_COMPOSITE_VIDEO_OUT) && !defined(SIMULATOR)
bool videoout_active(void);
void videoout_set_preferences(int screen, int overscan);
void videoout_ui_batch(bool enabled);
void videoout_ui_owner(bool enabled);
/* The caller has composed a full canvas using the selected TV display aspect. */
void videoout_set_video(bool tv_canvas);
/* Copy borrowed planes synchronously. Protect the presented picture from
 * LCD mirroring until explicitly released with NULL or replaced by a frame. */
void videoout_prepare_frame(const struct videoout_tv_frame *frame);
void videoout_present_ui(const uint16_t *pixels, int width, int height);

#else
static inline bool videoout_active(void) { return false; }
static inline void videoout_ui_batch(bool b) { (void)b; }
static inline void videoout_ui_owner(bool b) { (void)b; }
static inline void videoout_set_preferences(int s, int o) { (void)s; (void)o; }
static inline void videoout_set_video(bool v) { (void)v; }
static inline void videoout_prepare_frame(const struct videoout_tv_frame *f) { (void)f; }
static inline void videoout_present_ui(const uint16_t *p, int w, int h)
{ (void)p; (void)w; (void)h; }
#endif
/* Experimental host-prepared WPS pane, including its reflection. */
#define VIDEOOUT_ART_WIDTH 136
#define VIDEOOUT_ART_HEIGHT 186
#define VIDEOOUT_ART_SIZE (VIDEOOUT_ART_WIDTH * VIDEOOUT_ART_HEIGHT * 6u)
#if defined(VIDEOOUT_ENHANCED_TEST) && defined(HAVE_COMPOSITE_VIDEO_OUT) && !defined(SIMULATOR)
#include <stdint.h>
bool videoout_art_write(unsigned offset, const void *data, unsigned size);
bool videoout_art_finish(void);
void videoout_art_clear(void);
bool videoout_art_bind(const uint16_t *source, int stride, int x, int y);
#endif
#endif
