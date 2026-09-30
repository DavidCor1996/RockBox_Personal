/***************************************************************************
 * Exact, fixed-memory access to private iPod classic RetailOS resources.
 ***************************************************************************/

#ifndef IPODJS_RETAILOS_H
#define IPODJS_RETAILOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "screen_access.h"

#define IPODJS_RETAILOS_RESOURCE_COUNT 598
#define IPODJS_RETAILOS_ANIMATION_FRAME_COUNT 227
#define IPODJS_RETAILOS_RGA_BYTES(width, height) \
    ((size_t)(width) * (size_t)(height) * 3u)

struct ipodjs_retailos_image
{
    uint16_t width;
    uint16_t height;
    const unsigned char *pixels;
};

struct ipodjs_retailos_frame_pack
{
    uint16_t width;
    uint16_t height;
    uint16_t row_bytes;
    uint16_t format;
    uint16_t frame_count;
    uint32_t frame_bytes;
    uint32_t first_token;
    const unsigned char *frames;
};

enum ipodjs_retailos_animation
{
    IPODJS_RETAILOS_STATUSBAR_WHITE_BATTERY = 0,
    IPODJS_RETAILOS_STATUSBAR_BLACK_BATTERY,
    IPODJS_RETAILOS_CLOCK_HOURS,
    IPODJS_RETAILOS_CLOCK_MINUTES,
    IPODJS_RETAILOS_CLOCK_SECONDS,
    IPODJS_RETAILOS_NOW_PLAYING_IDLE_BATTERY,
    IPODJS_RETAILOS_RADIO_SCANNING,
    IPODJS_RETAILOS_NOW_PLAYING_EQUALIZER,
    IPODJS_RETAILOS_STOPWATCH_MINUTES,
    IPODJS_RETAILOS_STOPWATCH_SECONDS,
    IPODJS_RETAILOS_DISK_MODE_SYNC_ARROWS,
    IPODJS_RETAILOS_ANIMATION_COUNT,
};

struct ipodjs_retailos_animation_spec
{
    const char *name;
    const char *rga_path;
    const char *raw_path;
    uint16_t width;
    uint16_t height;
    uint16_t row_bytes;
    uint16_t format;
    uint16_t frame_count;
    uint32_t frame_bytes;
    uint32_t first_token;
};

const struct ipodjs_retailos_animation_spec *
ipodjs_retailos_animation_spec(enum ipodjs_retailos_animation animation);

bool ipodjs_retailos_load_named_rga(
    const char *name, void *storage, size_t storage_size,
    uint16_t width, uint16_t height, struct ipodjs_retailos_image *image);
bool ipodjs_retailos_load_opaque(unsigned ordinal, uint16_t *pixels,
    size_t count, unsigned width, unsigned height);
void ipodjs_retailos_blit_opaque(struct screen *display,
    const uint16_t *pixels, int stride, int x, int y, int width, int height);
bool ipodjs_retailos_load_resource_rga(
    unsigned int ordinal, void *storage, size_t storage_size,
    uint16_t width, uint16_t height, struct ipodjs_retailos_image *image);
/* Exact source crop, prepared outside painting; never scales pixels. */
bool ipodjs_retailos_load_resource_crop(
    unsigned int ordinal, void *storage, size_t storage_size,
    uint16_t source_width, uint16_t source_height, uint16_t x, uint16_t y,
    uint16_t width, uint16_t height, struct ipodjs_retailos_image *image);
bool ipodjs_retailos_load_animation_rga(
    enum ipodjs_retailos_animation animation, void *storage,
    size_t storage_size, struct ipodjs_retailos_image *atlas);
bool ipodjs_retailos_load_named_raw(
    const char *name, void *storage, size_t storage_size,
    uint16_t width, uint16_t height, uint16_t row_bytes,
    uint16_t format, uint32_t frame_bytes, uint32_t first_token,
    struct ipodjs_retailos_frame_pack *pack);
bool ipodjs_retailos_load_animation_raw(
    enum ipodjs_retailos_animation animation, void *storage,
    size_t storage_size, struct ipodjs_retailos_frame_pack *pack);

void ipodjs_retailos_blit(struct screen *display,
                          const struct ipodjs_retailos_image *image,
                          int x, int y);
void ipodjs_retailos_blit_color(struct screen *display,
                                const struct ipodjs_retailos_image *image,
                                int x, int y, fb_data color);
void ipodjs_retailos_blit_color_part(
    struct screen *display, const struct ipodjs_retailos_image *image,
    int source_x, int source_y, int x, int y, int width, int height,
    fb_data color);
void ipodjs_retailos_blit_tint_part(
    struct screen *display, const struct ipodjs_retailos_image *image,
    int source_x, int source_y, int x, int y, int width, int height,
    fb_data color);
void ipodjs_retailos_blit_part(struct screen *display,
                               const struct ipodjs_retailos_image *image,
                               int source_x, int source_y,
                               int x, int y, int width, int height);
void ipodjs_retailos_blit_mask_transform(
    struct screen *display, const struct ipodjs_retailos_frame_pack *pack,
    unsigned int frame, int x, int y, unsigned int quarter_turns,
    bool reflect_octant, fb_data color);
void ipodjs_retailos_blit_mask(
    struct screen *display, const struct ipodjs_retailos_frame_pack *pack,
    unsigned int frame, int x, int y, fb_data color);
void ipodjs_retailos_blit_mask_part(
    struct screen *display, const struct ipodjs_retailos_frame_pack *pack,
    unsigned int frame, int source_x, int source_y, int x, int y,
    int width, int height, fb_data color);
void ipodjs_retailos_blit_gray(
    struct screen *display, const struct ipodjs_retailos_frame_pack *pack,
    unsigned int frame, int x, int y);

#endif
