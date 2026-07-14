/***************************************************************************
 * Fast iPod 320x240 renderer with native and fullscreen paths.
 ***************************************************************************/

#include "snes_lite.h"
#ifdef SIMULATOR
#include <stdlib.h>
#endif

static fb_data *main_framebuffer(void)
{
    struct viewport *viewport = *(rb->screens[SCREEN_MAIN]->current_viewport);
    return viewport->buffer->fb_ptr;
}

static bool lcd_updates_suppressed(void)
{
#ifdef SIMULATOR
    if (getenv("SNES_LITE_TEST_UNTHROTTLED"))
        return true;
#endif
    return false;
}

static void update_lcd(void)
{
    if (lcd_updates_suppressed())
        return;
    rb->lcd_update();
}

static void update_lcd_rect(int x, int y, int width, int height)
{
    if (lcd_updates_suppressed())
        return;
    rb->lcd_update_rect(x, y, width, height);
}

#ifdef SIMULATOR
static void dump_frame_ppm(const char *path)
{
    fb_data *framebuffer = main_framebuffer();
    unsigned char row[LCD_WIDTH * 3];
    int fd;
    int y;

    if (!path || !path[0])
        return;
    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "P6\n%d %d\n255\n", LCD_WIDTH, LCD_HEIGHT);
    for (y = 0; y < LCD_HEIGHT; y++)
    {
        int x;

        for (x = 0; x < LCD_WIDTH; x++)
        {
            uint16_t pixel = framebuffer[y * LCD_WIDTH + x];
            unsigned red = (pixel >> 11) & 0x1f;
            unsigned green = (pixel >> 5) & 0x3f;
            unsigned blue = pixel & 0x1f;

            row[x * 3] = (red << 3) | (red >> 2);
            row[x * 3 + 1] = (green << 2) | (green >> 4);
            row[x * 3 + 2] = (blue << 3) | (blue >> 2);
        }
        rb->write(fd, row, sizeof(row));
    }
    rb->close(fd);
}

static void dump_test_frame(void)
{
    static bool dumped;
    const char *path = getenv("SNES_LITE_TEST_DUMP_PPM");
    const char *prefix = getenv("SNES_LITE_TEST_DUMP_PREFIX");
    const char *every_text = getenv("SNES_LITE_TEST_DUMP_EVERY");
    const char *frame_text = getenv("SNES_LITE_TEST_DUMP_FRAME");
    unsigned long frame = frame_text ? strtoul(frame_text, NULL, 10) : 30;
    unsigned long every = every_text ? strtoul(every_text, NULL, 10) : 0;

    if (prefix && every && snes_lite.profile_frames >= frame &&
        snes_lite.profile_frames % every == 0)
    {
        char numbered_path[MAX_PATH];

        rb->snprintf(numbered_path, sizeof(numbered_path), "%s-%06lu.ppm",
                     prefix, snes_lite.profile_frames);
        dump_frame_ppm(numbered_path);
    }

    if (!path || dumped)
        return;
    if (snes_lite.profile_frames < frame)
        return;
    dump_frame_ppm(path);
    dumped = true;
}
#endif

static inline fb_data snes_pixel_to_fb(uint16_t pixel)
{
#if LCD_PIXELFORMAT == RGB565
    /* The libretro core advertises native-endian RGB565, exactly matching
     * the iPod 6G framebuffer. Preserve it byte-for-byte. */
    return (fb_data)pixel;
#else
    unsigned red = (pixel >> 11) & 0x1f;
    unsigned green = (pixel >> 6) & 0x1f;
    unsigned blue = pixel & 0x1f;

    green = (green << 1) | (green >> 4);
    return LCD_RGBPACK_LCD(red, green, blue);
#endif
}

#if LCD_WIDTH == 320 && LCD_HEIGHT == 240
static void scale_line_reference(fb_data *destination,
                                 const uint16_t *source)
{
    int group;

    for (group = 0; group < 64; group++)
    {
        fb_data pixel0 = snes_pixel_to_fb(source[0]);

        destination[0] = pixel0;
        destination[1] = pixel0;
        destination[2] = snes_pixel_to_fb(source[1]);
        destination[3] = snes_pixel_to_fb(source[2]);
        destination[4] = snes_pixel_to_fb(source[3]);
        source += 4;
        destination += 5;
    }
}

static void scale_line_256_to_320(fb_data *destination,
                                  const uint16_t *source)
{
#if defined(LSB_FIRST) && LCD_PIXELFORMAT == RGB565
    if (((uintptr_t)destination & 3) == 0)
    {
        uint32_t *output = (uint32_t *)destination;
        int group;

        for (group = 0; group < 32; group++)
        {
            uint16_t p0 = source[0];
            uint16_t p1 = source[1];
            uint16_t p2 = source[2];
            uint16_t p3 = source[3];
            uint16_t p4 = source[4];
            uint16_t p5 = source[5];
            uint16_t p6 = source[6];
            uint16_t p7 = source[7];

            output[0] = p0 | ((uint32_t)p0 << 16);
            output[1] = p1 | ((uint32_t)p2 << 16);
            output[2] = p3 | ((uint32_t)p4 << 16);
            output[3] = p4 | ((uint32_t)p5 << 16);
            output[4] = p6 | ((uint32_t)p7 << 16);
            source += 8;
            output += 5;
        }
        return;
    }
#endif
    scale_line_reference(destination, source);
}

static void draw_fullscreen_256x224(const uint16_t *source,
                                    unsigned source_stride)
{
    fb_data *framebuffer = main_framebuffer();
    unsigned source_y = 0;
    unsigned accumulator = 0;
    unsigned destination_y;
    unsigned previous_source_y = 0xffffffffu;

    for (destination_y = 0; destination_y < 240; destination_y++)
    {
        fb_data *destination = framebuffer + destination_y * 320;

        if (source_y == previous_source_y)
            rb->memcpy(destination, destination - 320,
                       320 * sizeof(*destination));
        else
            scale_line_256_to_320(destination,
                                  source + source_y * source_stride);
        previous_source_y = source_y;
        accumulator += 224;
        if (accumulator >= 240)
        {
            accumulator -= 240;
            source_y++;
        }
    }
}
#endif

static bool native_border_valid;
static int native_x;
static int native_y;
static unsigned native_width;
static unsigned native_height;
static bool native_fps_was_visible;

static bool draw_native(const uint16_t *source, unsigned width,
                        unsigned height, unsigned stride, int *draw_x,
                        int *draw_y, unsigned *width_out,
                        unsigned *height_out)
{
    unsigned draw_width = MIN(256u, width);
    unsigned draw_height = MIN(224u, height);
    int x = (LCD_WIDTH - draw_width) / 2;
    int y = (LCD_HEIGHT - draw_height) / 2;
    fb_data *framebuffer = main_framebuffer();
    bool cleared_border = !native_border_valid || x != native_x ||
                          y != native_y || draw_width != native_width ||
                          draw_height != native_height;
    unsigned row;

    if (cleared_border)
    {
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_clear_display();
        native_border_valid = true;
        native_x = x;
        native_y = y;
        native_width = draw_width;
        native_height = draw_height;
    }
    for (row = 0; row < draw_height; row++)
    {
        fb_data *destination = framebuffer + (y + row) * LCD_WIDTH + x;
        const uint16_t *input = source + row * stride;
#if LCD_PIXELFORMAT == RGB565
        rb->memcpy(destination, input, draw_width * sizeof(*destination));
#else
        unsigned column;

        for (column = 0; column < draw_width; column++)
            destination[column] = snes_pixel_to_fb(input[column]);
#endif
    }

    *draw_x = x;
    *draw_y = y;
    *width_out = draw_width;
    *height_out = draw_height;
    return cleared_border;
}

bool snes_lite_video_selftest(void)
{
#if LCD_WIDTH == 320 && LCD_HEIGHT == 240 && LCD_PIXELFORMAT == RGB565
    static uint16_t source[256];
    static fb_data output[320];
    static fb_data reference[320];
    unsigned index;

    for (index = 0; index < ARRAYLEN(source); index++)
        source[index] = (uint16_t)(((index & 0x1f) << 11) |
                                  ((index & 0x1f) << 6) |
                                  (index & 0x1f));
    scale_line_256_to_320(output, source);
    scale_line_reference(reference, source);
    for (index = 0; index < ARRAYLEN(output); index++)
    {
        uint16_t input = source[(index * 4) / 5];
        fb_data expected = (fb_data)input;

        if (output[index] != expected || output[index] != reference[index])
            return false;
    }

#endif
    return true;
}

static void draw_fps_overlay(void)
{
    char text[40];

    if (!snes_lite.config.show_fps)
        return;
    rb->snprintf(text, sizeof(text), "E%d D%d S%d",
                 snes_lite.displayed_fps,
                 snes_lite.displayed_render_fps,
                 snes_lite.effective_frameskip);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_putsxy(2, 2, text);
}

static int fps_overlay_height(void)
{
    static int height;

    if (height == 0)
        rb->lcd_getstringsize("A", NULL, &height);
    return height;
}

void snes_lite_video_refresh(const void *data, unsigned width,
                             unsigned height, size_t pitch)
{
    unsigned stride;
    long start_tick;
    long lcd_tick;

    if (!data)
    {
        snes_lite.skipped_frames++;
        return;
    }
    stride = pitch / sizeof(uint16_t);
    snes_lite.last_video_data = data;
    snes_lite.last_video_width = width;
    snes_lite.last_video_height = height;
    snes_lite.last_video_pitch = pitch;
    snes_lite.rendered_frames++;
    start_tick = *rb->current_tick;

#if LCD_WIDTH == 320 && LCD_HEIGHT == 240
    if (snes_lite.config.video_mode == SNES_VIDEO_FULLSCREEN &&
        width == 256 && height >= 224)
    {
        native_border_valid = false;
        native_fps_was_visible = false;
        draw_fullscreen_256x224(data, stride);
        draw_fps_overlay();
#ifdef SIMULATOR
        dump_test_frame();
#endif
        lcd_tick = *rb->current_tick;
        snes_lite.video_scale_ticks += lcd_tick - start_tick;
        update_lcd();
        snes_lite.video_lcd_ticks += *rb->current_tick - lcd_tick;
        return;
    }
#endif
    {
        int draw_x;
        int draw_y;
        unsigned draw_width;
        unsigned draw_height;
        int overlay_height = fps_overlay_height() + 2;
        bool overlay_dirty = snes_lite.config.show_fps ||
                             native_fps_was_visible;
        bool full_update;

        if (overlay_dirty)
        {
            rb->lcd_set_foreground(LCD_BLACK);
            rb->lcd_fillrect(0, 0, LCD_WIDTH, overlay_height);
        }
        full_update = draw_native(data, width, height, stride, &draw_x,
                                  &draw_y, &draw_width, &draw_height);
        native_fps_was_visible = snes_lite.config.show_fps;
        if (snes_lite.config.show_fps)
            draw_fps_overlay();
#ifdef SIMULATOR
        dump_test_frame();
#endif
        lcd_tick = *rb->current_tick;
        snes_lite.video_scale_ticks += lcd_tick - start_tick;
        if (full_update)
        {
            update_lcd();
        }
        else
        {
            update_lcd_rect(draw_x, draw_y, draw_width, draw_height);
            if (overlay_dirty)
                update_lcd_rect(0, 0, LCD_WIDTH, overlay_height);
        }
        snes_lite.video_lcd_ticks += *rb->current_tick - lcd_tick;
    }
}

void snes_lite_video_redraw(void)
{
    if (!snes_lite.last_video_data)
        return;
    snes_lite_video_refresh(snes_lite.last_video_data,
                            snes_lite.last_video_width,
                            snes_lite.last_video_height,
                            snes_lite.last_video_pitch);
}
