/***************************************************************************
 * Exact, fixed-memory access to private iPod classic RetailOS resources.
 *
 * The preparation tool admits only Apple's hash-pinned iPod35 2.0.4 image.
 * This runtime performs no resizing, interpolation, recoloring, or repair.
 * Every loader validates the complete expected file before publishing it to
 * a cached draw path.  Missing or partial private assets therefore fail
 * closed and cannot masquerade as a complete RetailOS surface.
 ***************************************************************************/

#ifdef PLUGIN
#include "plugin.h"
#define snprintf rb->snprintf
#define strstr rb->strstr
#define memcmp rb->memcmp
#define memset rb->memset
#define open rb->open
#define read rb->read
#define lseek rb->lseek
#define close rb->close
#define filesize rb->filesize
#define IPODJS_RETAILOS_MAIN_SCREEN (rb->screens[SCREEN_MAIN])
#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
static fb_data *ipodjs_retailos_framebuffer_at(int x, int y)
{
    struct viewport *vp =
        *rb->screens[SCREEN_MAIN]->current_viewport;
    return vp->buffer->fb_ptr + y * vp->buffer->stride + x;
}
#define IPODJS_RETAILOS_FBADDR(x, y) ipodjs_retailos_framebuffer_at(x, y)
#endif
#else
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "file.h"
#include "lcd.h"
#include "rbpaths.h"
#include "screen_access.h"
#include "system.h"
#define IPODJS_RETAILOS_MAIN_SCREEN (&screens[SCREEN_MAIN])
#define IPODJS_RETAILOS_FBADDR(x, y) FBADDR(x, y)
#endif
#include "ipodjs_retailos.h"

#define IPODJS_RETAILOS_DIR \
    ROCKBOX_DIR "/ipodjs/apple/retailos-2.0.4"
#define IPODJS_RETAILOS_RGA_HEADER_SIZE 8u
#define IPODJS_RETAILOS_IAF_HEADER_SIZE 24u
#define IPODJS_RETAILOS_IAF_VERSION 1u
#define IPODJS_RETAILOS_FORMAT_4 0x0004u
#define IPODJS_RETAILOS_FORMAT_8 0x0008u

static const struct ipodjs_retailos_animation_spec
    ipodjs_retailos_animations[IPODJS_RETAILOS_ANIMATION_COUNT] =
{
    {
        "statusbar-white-battery",
        "frames/statusbar-white-battery.rga", NULL,
        26, 13, 0, 0x0064, 25, 0, 0x0dad010f,
    },
    {
        "statusbar-black-battery",
        "frames/statusbar-black-battery.rga", NULL,
        26, 13, 0, 0x0064, 25, 0, 0x0dad0128,
    },
    {
        "clock-hours", "frames/clock-hours.rga",
        "frames/clock-hours.iaf",
        73, 73, 48, 0x0004, 30, 3504, 0x0dad045c,
    },
    {
        "clock-minutes", "frames/clock-minutes.rga",
        "frames/clock-minutes.iaf",
        73, 73, 48, 0x0004, 16, 3504, 0x0dad047a,
    },
    {
        "clock-seconds", "frames/clock-seconds.rga",
        "frames/clock-seconds.iaf",
        73, 73, 48, 0x0004, 16, 3504, 0x0dad048a,
    },
    {
        "now-playing-idle-battery",
        "frames/now-playing-idle-battery.rga", NULL,
        72, 40, 0, 0x0064, 8, 0, 0x0dad0863,
    },
    {
        "radio-scanning", "frames/radio-scanning.rga", NULL,
        19, 15, 0, 0x0064, 7, 0, 0x0dad0a62,
    },
    {
        "now-playing-equalizer", "frames/now-playing-equalizer.rga",
        "frames/now-playing-equalizer.iaf",
        130, 79, 76, 0x0004, 22, 6004, 0x0dad0bd9,
    },
    {
        "stopwatch-minutes", "frames/stopwatch-minutes.rga",
        "frames/stopwatch-minutes.iaf",
        18, 18, 20, 0x0004, 30, 360, 0x0dad0c9b,
    },
    {
        "stopwatch-seconds", "frames/stopwatch-seconds.rga",
        "frames/stopwatch-seconds.iaf",
        124, 124, 72, 0x0004, 30, 8928, 0x0dad0cb9,
    },
    {
        "disk-mode-sync-arrows", "frames/disk-mode-sync-arrows.rga",
        "frames/disk-mode-sync-arrows.iaf",
        76, 76, 48, 0x0004, 18, 3648, 0x0dad0dbb,
    },
};

static uint16_t ipodjs_retailos_le16(const unsigned char *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

static uint32_t ipodjs_retailos_le32(const unsigned char *source)
{
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8) |
           ((uint32_t)source[2] << 16) | ((uint32_t)source[3] << 24);
}

static bool ipodjs_retailos_component_name(const char *name)
{
    const unsigned char *cursor = (const unsigned char *)name;

    if (!cursor || !*cursor)
        return false;
    while (*cursor)
    {
        if (!((*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= '0' && *cursor <= '9') || *cursor == '-'))
            return false;
        cursor++;
    }
    return true;
}

static bool ipodjs_retailos_read_exact(const char *relative,
                                       unsigned char *header,
                                       size_t header_size, void *storage,
                                       size_t payload_size)
{
    char path[MAX_PATH];
    unsigned char *payload = storage;
    size_t done = 0;
    int fd;

    if (!relative || relative[0] == '/' || strstr(relative, "..") ||
        snprintf(path, sizeof(path), "%s/%s", IPODJS_RETAILOS_DIR,
                 relative) >= (int)sizeof(path))
        return false;
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    if (filesize(fd) != (off_t)(header_size + payload_size) ||
        read(fd, header, header_size) != (ssize_t)header_size)
    {
        close(fd);
        return false;
    }
    while (done < payload_size)
    {
        ssize_t count = read(fd, payload + done, payload_size - done);

        if (count <= 0)
        {
            close(fd);
            return false;
        }
        done += (size_t)count;
    }
    close(fd);
    return true;
}

static bool ipodjs_retailos_load_rga(
    const char *relative, void *storage, size_t storage_size,
    uint16_t width, uint16_t height, struct ipodjs_retailos_image *image)
{
    unsigned char header[IPODJS_RETAILOS_RGA_HEADER_SIZE];
    size_t payload_size = IPODJS_RETAILOS_RGA_BYTES(width, height);

    if (!storage || !image || storage_size < payload_size ||
        !ipodjs_retailos_read_exact(relative, header, sizeof(header),
                                    storage, payload_size) ||
        memcmp(header, "RGA1", 4) != 0 ||
        ipodjs_retailos_le16(header + 4) != width ||
        ipodjs_retailos_le16(header + 6) != height)
        return false;
    image->width = width;
    image->height = height;
    image->pixels = storage;
    return true;
}

static bool ipodjs_retailos_load_raw(
    const char *relative, void *storage, size_t storage_size,
    uint16_t width, uint16_t height, uint16_t row_bytes,
    uint16_t format, uint16_t frame_count, uint32_t frame_bytes,
    uint32_t first_token, struct ipodjs_retailos_frame_pack *pack)
{
    unsigned char header[IPODJS_RETAILOS_IAF_HEADER_SIZE];
    size_t payload_size = (size_t)frame_count * frame_bytes;

    if (!storage || !pack || storage_size < payload_size ||
        !ipodjs_retailos_read_exact(relative, header, sizeof(header),
                                    storage, payload_size) ||
        memcmp(header, "IAF1", 4) != 0 ||
        ipodjs_retailos_le16(header + 4) != IPODJS_RETAILOS_IAF_VERSION ||
        ipodjs_retailos_le16(header + 6) != width ||
        ipodjs_retailos_le16(header + 8) != height ||
        ipodjs_retailos_le16(header + 10) != row_bytes ||
        ipodjs_retailos_le16(header + 12) != format ||
        ipodjs_retailos_le16(header + 14) != frame_count ||
        ipodjs_retailos_le32(header + 16) != frame_bytes ||
        ipodjs_retailos_le32(header + 20) != first_token)
        return false;
    pack->width = width;
    pack->height = height;
    pack->row_bytes = row_bytes;
    pack->format = format;
    pack->frame_count = frame_count;
    pack->frame_bytes = frame_bytes;
    pack->first_token = first_token;
    pack->frames = storage;
    return true;
}

const struct ipodjs_retailos_animation_spec *
ipodjs_retailos_animation_spec(enum ipodjs_retailos_animation animation)
{
    if (animation < 0 || animation >= IPODJS_RETAILOS_ANIMATION_COUNT)
        return NULL;
    return &ipodjs_retailos_animations[animation];
}

bool ipodjs_retailos_load_named_rga(
    const char *name, void *storage, size_t storage_size,
    uint16_t width, uint16_t height, struct ipodjs_retailos_image *image)
{
    char relative[96];

    if (!ipodjs_retailos_component_name(name) ||
        snprintf(relative, sizeof(relative), "named/%s.rga", name) >=
            (int)sizeof(relative))
        return false;
    return ipodjs_retailos_load_rga(relative, storage, storage_size,
                                    width, height, image);
}

bool ipodjs_retailos_load_resource_rga(
    unsigned int ordinal, void *storage, size_t storage_size,
    uint16_t width, uint16_t height, struct ipodjs_retailos_image *image)
{
    char relative[32];

    if (ordinal >= IPODJS_RETAILOS_RESOURCE_COUNT)
        return false;
    snprintf(relative, sizeof(relative), "resources/%03u.rga", ordinal);
    return ipodjs_retailos_load_rga(relative, storage, storage_size,
                                    width, height, image);
}

bool ipodjs_retailos_load_resource_crop(
    unsigned int ordinal, void *storage, size_t storage_size,
    uint16_t source_width, uint16_t source_height, uint16_t x, uint16_t y,
    uint16_t width, uint16_t height, struct ipodjs_retailos_image *image)
{
    char path[MAX_PATH];
    unsigned char header[IPODJS_RETAILOS_RGA_HEADER_SIZE];
    unsigned char *pixels = storage;
    bool valid = false;
    int fd;

    if (ordinal >= IPODJS_RETAILOS_RESOURCE_COUNT || !image || !storage ||
        !width || !height || x + width > source_width ||
        y + height > source_height ||
        storage_size < IPODJS_RETAILOS_RGA_BYTES(width, height))
        return false;
    snprintf(path, sizeof(path), IPODJS_RETAILOS_DIR "/resources/%03u.rga",
             ordinal);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    if (filesize(fd) != (off_t)(sizeof(header) +
            IPODJS_RETAILOS_RGA_BYTES(source_width, source_height)) ||
        read(fd, header, sizeof(header)) != sizeof(header) ||
        memcmp(header, "RGA1", 4) ||
        ipodjs_retailos_le16(header + 4) != source_width ||
        ipodjs_retailos_le16(header + 6) != source_height)
        goto out;
    for (unsigned row = 0; row < height; row++)
    {
        off_t offset = sizeof(header) +
            ((off_t)(y + row) * source_width + x) * 3;
        size_t bytes = width * 3u;
        if (lseek(fd, offset, SEEK_SET) != offset ||
            read(fd, pixels + row * bytes, bytes) != (ssize_t)bytes)
            goto out;
    }
    image->width = width;
    image->height = height;
    image->pixels = storage;
    valid = true;
out:
    close(fd);
    return valid;
}

bool ipodjs_retailos_load_animation_rga(
    enum ipodjs_retailos_animation animation, void *storage,
    size_t storage_size, struct ipodjs_retailos_image *atlas)
{
    const struct ipodjs_retailos_animation_spec *spec =
        ipodjs_retailos_animation_spec(animation);

    if (!spec)
        return false;
    return ipodjs_retailos_load_rga(
        spec->rga_path, storage, storage_size, spec->width,
        spec->height * spec->frame_count, atlas);
}

bool ipodjs_retailos_load_named_raw(
    const char *name, void *storage, size_t storage_size,
    uint16_t width, uint16_t height, uint16_t row_bytes,
    uint16_t format, uint32_t frame_bytes, uint32_t first_token,
    struct ipodjs_retailos_frame_pack *pack)
{
    char relative[96];

    if (!ipodjs_retailos_component_name(name) ||
        snprintf(relative, sizeof(relative), "named/%s.iaf", name) >=
            (int)sizeof(relative))
        return false;
    return ipodjs_retailos_load_raw(
        relative, storage, storage_size, width, height, row_bytes,
        format, 1, frame_bytes, first_token, pack);
}

bool ipodjs_retailos_load_animation_raw(
    enum ipodjs_retailos_animation animation, void *storage,
    size_t storage_size, struct ipodjs_retailos_frame_pack *pack)
{
    const struct ipodjs_retailos_animation_spec *spec =
        ipodjs_retailos_animation_spec(animation);

    if (!spec || !spec->raw_path)
        return false;
    return ipodjs_retailos_load_raw(
        spec->raw_path, storage, storage_size, spec->width, spec->height,
        spec->row_bytes, spec->format, spec->frame_count,
        spec->frame_bytes, spec->first_token, pack);
}

#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
static fb_data ipodjs_retailos_rgb565(const unsigned char *source)
{
    uint16_t value = ipodjs_retailos_le16(source);
    unsigned int red = ((value >> 11) & 0x1f) * 255 / 31;
    unsigned int green = ((value >> 5) & 0x3f) * 255 / 63;
    unsigned int blue = (value & 0x1f) * 255 / 31;

    return FB_RGBPACK(red, green, blue);
}

static fb_data ipodjs_retailos_blend(fb_data background,
                                     fb_data foreground,
                                     unsigned int alpha)
{
    unsigned int inverse;

    if (alpha >= 255)
        return foreground;
    if (alpha == 0)
        return background;
    inverse = 255 - alpha;
    return FB_RGBPACK(
        (FB_UNPACK_RED(background) * inverse +
         FB_UNPACK_RED(foreground) * alpha + 127) / 255,
        (FB_UNPACK_GREEN(background) * inverse +
         FB_UNPACK_GREEN(foreground) * alpha + 127) / 255,
        (FB_UNPACK_BLUE(background) * inverse +
         FB_UNPACK_BLUE(foreground) * alpha + 127) / 255);
}
#endif

/* Opaque backgrounds do not need a retained alpha plane. Read in 32-pixel
 * chunks, never a full row or framebuffer on the main-thread stack. */
bool ipodjs_retailos_load_opaque(unsigned ordinal, uint16_t *pixels,
    size_t count, unsigned width, unsigned height)
{
    char path[MAX_PATH];
    unsigned char chunk[96], header[8];
    bool valid = false;
    if (!pixels || !width || !height || width > LCD_WIDTH ||
        height > LCD_HEIGHT || count < width * height ||
        ordinal >= IPODJS_RETAILOS_RESOURCE_COUNT)
        return false;
    snprintf(path, sizeof(path), IPODJS_RETAILOS_DIR "/resources/%03u.rga",
        ordinal);
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    if (filesize(fd) != (off_t)(8 + width * height * 3) ||
        read(fd, header, 8) != 8 || memcmp(header, "RGA1", 4) ||
        ipodjs_retailos_le16(header + 4) != width ||
        ipodjs_retailos_le16(header + 6) != height)
        goto out;
    for (unsigned offset = 0; offset < width * height; offset += 32)
    {
        unsigned size = MIN(32, width * height - offset);
        if (read(fd, chunk, size * 3) != (ssize_t)(size * 3))
            goto out;
        for (unsigned i = 0; i < size; i++)
        {
            if (chunk[i * 3 + 2] != 255)
                goto out;
            pixels[offset + i] = ipodjs_retailos_le16(chunk + i * 3);
        }
    }
    valid = true;
out:
    close(fd);
    return valid;
}

void ipodjs_retailos_blit_opaque(struct screen *display,
    const uint16_t *pixels, int stride, int x, int y, int width, int height)
{
#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
    if (!display || display != IPODJS_RETAILOS_MAIN_SCREEN || !pixels ||
        x < 0 || y < 0 || width < 1 || height < 1 || stride < width ||
        x + width > display->lcdwidth || y + height > display->lcdheight)
        return;
    for (int row = 0; row < height; row++)
    {
        fb_data *dest = IPODJS_RETAILOS_FBADDR(x, y + row);
        for (int col = 0; col < width; col++)
        {
            unsigned value = pixels[row * stride + col];
            dest[col] = FB_RGBPACK(((value >> 11) & 31) * 255 / 31,
                ((value >> 5) & 63) * 255 / 63, (value & 31) * 255 / 31);
        }
    }
#else
    (void)display; (void)pixels; (void)stride; (void)x; (void)y;
    (void)width; (void)height;
#endif
}

void ipodjs_retailos_blit_part(struct screen *display,
                               const struct ipodjs_retailos_image *image,
                               int source_x, int source_y,
                               int x, int y, int width, int height)
{
#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
    int row;

    if (!display || display != IPODJS_RETAILOS_MAIN_SCREEN || !image ||
        !image->pixels || source_x < 0 || source_y < 0 || width <= 0 ||
        height <= 0 || source_x + width > image->width ||
        source_y + height > image->height)
        return;
    if (x < 0)
    {
        source_x -= x;
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        source_y -= y;
        height += y;
        y = 0;
    }
    width = MIN(width, display->lcdwidth - x);
    height = MIN(height, display->lcdheight - y);
    if (width <= 0 || height <= 0)
        return;
    for (row = 0; row < height; row++)
    {
        fb_data *destination = IPODJS_RETAILOS_FBADDR(x, y + row);
        size_t source_index =
            ((size_t)(source_y + row) * image->width + source_x) * 3u;
        int column;

        for (column = 0; column < width; column++)
        {
            const unsigned char *source = image->pixels + source_index;
            unsigned int alpha = source[2];

            if (alpha)
            {
                fb_data pixel = ipodjs_retailos_rgb565(source);

                destination[column] = ipodjs_retailos_blend(
                    destination[column], pixel, alpha);
            }
            source_index += 3;
        }
    }
#else
    (void)display;
    (void)image;
    (void)source_x;
    (void)source_y;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
#endif
}

void ipodjs_retailos_blit(struct screen *display,
                          const struct ipodjs_retailos_image *image,
                          int x, int y)
{
    if (!image)
        return;
    ipodjs_retailos_blit_part(display, image, 0, 0, x, y,
                              image->width, image->height);
}

/* Preserve Apple's original antialiased artwork and alpha while changing
 * only its foreground color for the active iPodJS accent palette. */
void ipodjs_retailos_blit_color_part(
    struct screen *display, const struct ipodjs_retailos_image *image,
    int source_x, int source_y, int x, int y, int width, int height,
    fb_data color)
{
#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
    if (!display || display != IPODJS_RETAILOS_MAIN_SCREEN || !image ||
        !image->pixels || source_x < 0 || source_y < 0 || width <= 0 ||
        height <= 0 || source_x + width > image->width ||
        source_y + height > image->height)
        return;

    if (x < 0)
    {
        source_x -= x;
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        source_y -= y;
        height += y;
        y = 0;
    }
    width = MIN(width, display->lcdwidth - x);
    height = MIN(height, display->lcdheight - y);
    if (width <= 0 || height <= 0)
        return;

    for (int row = 0; row < height; row++)
    {
        fb_data *destination = IPODJS_RETAILOS_FBADDR(x, y + row);
        size_t source_index =
            ((size_t)(source_y + row) * image->width + source_x) * 3u;

        for (int column = 0; column < width; column++)
        {
            const unsigned char *source = image->pixels + source_index;
            unsigned int alpha = source[2];

            if (alpha)
                destination[column] = ipodjs_retailos_blend(
                    destination[column], color, alpha);
            source_index += 3;
        }
    }
#else
    (void)display;
    (void)image;
    (void)x;
    (void)y;
    (void)color;
#endif
}

/* Recolor an Apple blue asset while retaining its source luminance, shading,
 * antialiasing, and alpha.  The stock blue palette bypasses this path and
 * blits the untouched source pixels. */
void ipodjs_retailos_blit_tint_part(
    struct screen *display, const struct ipodjs_retailos_image *image,
    int source_x, int source_y, int x, int y, int width, int height,
    fb_data color)
{
#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
    if (!display || display != IPODJS_RETAILOS_MAIN_SCREEN || !image ||
        !image->pixels || source_x < 0 || source_y < 0 || width <= 0 ||
        height <= 0 || source_x + width > image->width ||
        source_y + height > image->height)
        return;

    if (x < 0)
    {
        source_x -= x;
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        source_y -= y;
        height += y;
        y = 0;
    }
    width = MIN(width, display->lcdwidth - x);
    height = MIN(height, display->lcdheight - y);
    if (width <= 0 || height <= 0)
        return;

    unsigned int tint_r = FB_UNPACK_RED(color);
    unsigned int tint_g = FB_UNPACK_GREEN(color);
    unsigned int tint_b = FB_UNPACK_BLUE(color);
    unsigned int tint_luma =
        (77 * tint_r + 150 * tint_g + 29 * tint_b + 128) >> 8;
    if (tint_luma == 0)
        tint_luma = 1;

    for (int row = 0; row < height; row++)
    {
        fb_data *destination = IPODJS_RETAILOS_FBADDR(x, y + row);
        size_t source_index =
            ((size_t)(source_y + row) * image->width + source_x) * 3u;

        for (int column = 0; column < width; column++)
        {
            const unsigned char *source = image->pixels + source_index;
            unsigned int alpha = source[2];

            if (alpha)
            {
                unsigned int source_r;
                unsigned int source_g;
                unsigned int source_b;
                unsigned int source_luma;
                fb_data tinted;

                /* RGA stores RGB565 in its first two bytes and alpha in the
                 * third; decode the native source so its gloss is retained. */
                uint16_t native = ipodjs_retailos_le16(source);
                source_r = ((native >> 11) & 0x1f) * 255 / 31;
                source_g = ((native >> 5) & 0x3f) * 255 / 63;
                source_b = (native & 0x1f) * 255 / 31;
                source_luma =
                    (77 * source_r + 150 * source_g + 29 * source_b + 128)
                    >> 8;
                tinted = FB_RGBPACK(
                    MIN(255, (tint_r * source_luma + tint_luma / 2) /
                             tint_luma),
                    MIN(255, (tint_g * source_luma + tint_luma / 2) /
                             tint_luma),
                    MIN(255, (tint_b * source_luma + tint_luma / 2) /
                             tint_luma));
                destination[column] = ipodjs_retailos_blend(
                    destination[column], tinted, alpha);
            }
            source_index += 3;
        }
    }
#else
    (void)display;
    (void)image;
    (void)source_x;
    (void)source_y;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
#endif
}

void ipodjs_retailos_blit_mask_transform(
    struct screen *display, const struct ipodjs_retailos_frame_pack *pack,
    unsigned int frame, int x, int y, unsigned int quarter_turns,
    bool reflect_octant, fb_data color)
{
#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
    const unsigned char *source;
    unsigned int side;
    unsigned int source_y;

    if (!display || display != IPODJS_RETAILOS_MAIN_SCREEN || !pack ||
        !pack->frames || frame >= pack->frame_count ||
        pack->width != pack->height ||
        (pack->format != IPODJS_RETAILOS_FORMAT_4 &&
         pack->format != IPODJS_RETAILOS_FORMAT_8))
        return;
    side = pack->width;
    source = pack->frames + (size_t)frame * pack->frame_bytes;
    quarter_turns &= 3;
    for (source_y = 0; source_y < side; source_y++)
    {
        const unsigned char *row = source + source_y * pack->row_bytes;
        unsigned int source_x;

        for (source_x = 0; source_x < side; source_x++)
        {
            unsigned int alpha;
            unsigned int target_x = source_x;
            unsigned int target_y = source_y;
            unsigned int temporary;
            int draw_x;
            int draw_y;

            if (pack->format == IPODJS_RETAILOS_FORMAT_4)
            {
                unsigned int packed = row[source_x / 2];

                alpha = source_x & 1 ? packed & 0x0f : packed >> 4;
                alpha *= 17;
            }
            else
                alpha = row[source_x];
            if (!alpha)
                continue;
            if (reflect_octant)
            {
                temporary = target_x;
                target_x = side - 1 - target_y;
                target_y = side - 1 - temporary;
            }
            if (quarter_turns == 1)
            {
                temporary = target_x;
                target_x = side - 1 - target_y;
                target_y = temporary;
            }
            else if (quarter_turns == 2)
            {
                target_x = side - 1 - target_x;
                target_y = side - 1 - target_y;
            }
            else if (quarter_turns == 3)
            {
                temporary = target_x;
                target_x = target_y;
                target_y = side - 1 - temporary;
            }
            draw_x = x + target_x;
            draw_y = y + target_y;
            if (draw_x >= 0 && draw_x < display->lcdwidth &&
                draw_y >= 0 && draw_y < display->lcdheight)
            {
                fb_data *destination = IPODJS_RETAILOS_FBADDR(draw_x, draw_y);

                *destination = ipodjs_retailos_blend(
                    *destination, color, alpha);
            }
        }
    }
#else
    (void)display;
    (void)pack;
    (void)frame;
    (void)x;
    (void)y;
    (void)quarter_turns;
    (void)reflect_octant;
    (void)color;
#endif
}

void ipodjs_retailos_blit_mask(
    struct screen *display, const struct ipodjs_retailos_frame_pack *pack,
    unsigned int frame, int x, int y, fb_data color)
{
#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
    const unsigned char *source;
    unsigned int source_y;

    if (!display || display != IPODJS_RETAILOS_MAIN_SCREEN || !pack ||
        !pack->frames || frame >= pack->frame_count ||
        (pack->format != IPODJS_RETAILOS_FORMAT_4 &&
         pack->format != IPODJS_RETAILOS_FORMAT_8))
        return;
    source = pack->frames + (size_t)frame * pack->frame_bytes;
    for (source_y = 0; source_y < pack->height; source_y++)
    {
        const unsigned char *row = source + source_y * pack->row_bytes;
        unsigned int source_x;

        for (source_x = 0; source_x < pack->width; source_x++)
        {
            unsigned int alpha;
            int draw_x = x + source_x;
            int draw_y = y + source_y;

            if (pack->format == IPODJS_RETAILOS_FORMAT_4)
            {
                unsigned int packed = row[source_x / 2];

                alpha = source_x & 1 ? packed & 0x0f : packed >> 4;
                alpha *= 17;
            }
            else
                alpha = row[source_x];
            if (!alpha || draw_x < 0 || draw_x >= display->lcdwidth ||
                draw_y < 0 || draw_y >= display->lcdheight)
                continue;
            {
                fb_data *destination = IPODJS_RETAILOS_FBADDR(draw_x, draw_y);

                *destination = ipodjs_retailos_blend(
                    *destination, color, alpha);
            }
        }
    }
#else
    (void)display;
    (void)pack;
    (void)frame;
    (void)x;
    (void)y;
    (void)color;
#endif
}

void ipodjs_retailos_blit_mask_part(
    struct screen *display, const struct ipodjs_retailos_frame_pack *pack,
    unsigned int frame, int source_x, int source_y, int x, int y,
    int width, int height, fb_data color)
{
#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
    const unsigned char *source;
    int row_index;

    if (!display || display != IPODJS_RETAILOS_MAIN_SCREEN || !pack ||
        !pack->frames || frame >= pack->frame_count || source_x < 0 ||
        source_y < 0 || width <= 0 || height <= 0 ||
        source_x + width > pack->width || source_y + height > pack->height ||
        (pack->format != IPODJS_RETAILOS_FORMAT_4 &&
         pack->format != IPODJS_RETAILOS_FORMAT_8))
        return;

    source = pack->frames + (size_t)frame * pack->frame_bytes;
    for (row_index = 0; row_index < height; row_index++)
    {
        int draw_y = y + row_index;
        const unsigned char *row =
            source + (size_t)(source_y + row_index) * pack->row_bytes;
        int column;

        if (draw_y < 0 || draw_y >= display->lcdheight)
            continue;
        for (column = 0; column < width; column++)
        {
            unsigned int px = source_x + column;
            unsigned int alpha;
            int draw_x = x + column;

            if (draw_x < 0 || draw_x >= display->lcdwidth)
                continue;
            if (pack->format == IPODJS_RETAILOS_FORMAT_4)
            {
                unsigned int packed = row[px / 2];

                alpha = px & 1 ? packed & 0x0f : packed >> 4;
                alpha *= 17;
            }
            else
                alpha = row[px];
            if (alpha)
            {
                fb_data *destination = IPODJS_RETAILOS_FBADDR(draw_x, draw_y);

                *destination = ipodjs_retailos_blend(
                    *destination, color, alpha);
            }
        }
    }
#else
    (void)display;
    (void)pack;
    (void)frame;
    (void)source_x;
    (void)source_y;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
#endif
}

void ipodjs_retailos_blit_gray(
    struct screen *display, const struct ipodjs_retailos_frame_pack *pack,
    unsigned int frame, int x, int y)
{
#if LCD_DEPTH >= 16 && LCD_STRIDEFORMAT == HORIZONTAL_STRIDE
    const unsigned char *source;
    unsigned int source_y;

    if (!display || display != IPODJS_RETAILOS_MAIN_SCREEN || !pack ||
        !pack->frames || frame >= pack->frame_count ||
        (pack->format != IPODJS_RETAILOS_FORMAT_4 &&
         pack->format != IPODJS_RETAILOS_FORMAT_8))
        return;

    source = pack->frames + (size_t)frame * pack->frame_bytes;
    for (source_y = 0; source_y < pack->height; source_y++)
    {
        const unsigned char *row = source + source_y * pack->row_bytes;
        int draw_y = y + source_y;
        unsigned int source_x;

        if (draw_y < 0 || draw_y >= display->lcdheight)
            continue;
        for (source_x = 0; source_x < pack->width; source_x++)
        {
            unsigned int gray;
            int draw_x = x + source_x;

            if (draw_x < 0 || draw_x >= display->lcdwidth)
                continue;
            if (pack->format == IPODJS_RETAILOS_FORMAT_4)
            {
                unsigned int packed = row[source_x / 2];

                gray = source_x & 1 ? packed & 0x0f : packed >> 4;
                gray *= 17;
            }
            else
                gray = row[source_x];
            *IPODJS_RETAILOS_FBADDR(draw_x, draw_y) =
                FB_RGBPACK(gray, gray, gray);
        }
    }
#else
    (void)display;
    (void)pack;
    (void)frame;
    (void)source_x;
    (void)source_y;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
#endif
}

void ipodjs_retailos_blit_color(struct screen *display,
                                const struct ipodjs_retailos_image *image,
                                int x, int y, fb_data color)
{
    if (!image)
        return;
    ipodjs_retailos_blit_color_part(display, image, 0, 0, x, y,
                                    image->width, image->height, color);
}
