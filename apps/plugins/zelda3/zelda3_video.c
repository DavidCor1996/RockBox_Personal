/***************************************************************************
 * Full-screen SNES-aspect Zelda3 renderer for the iPod 320x240 LCD.
 ****************************************************************************/
#include "zelda3.h"
#include "tlsf.h"
#include "upstream/src/zelda_rtl.h"
#ifdef SIMULATOR
#include <stdlib.h>
#endif

#define ZELDA_WIDTH 256
#define ZELDA_HEIGHT 224

static uint32_t *source_frame;
static fb_data *lcd_frame;

static void update_lcd(void)
{
#ifdef SIMULATOR
    if (getenv("ZELDA3_TEST_UNTHROTTLED"))
        return;
#endif
    rb->lcd_update();
}

#ifdef SIMULATOR
static void dump_test_frame(void)
{
    static bool dumped;
    const char *path = getenv("ZELDA3_TEST_DUMP_PPM");
    const char *frame_text = getenv("ZELDA3_TEST_DUMP_FRAME");
    unsigned long frame = frame_text ? rb->atoi(frame_text) : 300;
    unsigned char row[LCD_WIDTH * 3];
    int fd;
    int y;

    if (!path || !path[0] || dumped || zelda3_rb.rendered < frame)
        return;
    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "P6\n%d %d\n255\n", LCD_WIDTH, LCD_HEIGHT);
    for (y = 0; y < LCD_HEIGHT; ++y)
    {
        int x;

        for (x = 0; x < LCD_WIDTH; ++x)
        {
            int source_y = y - (y + 14) / 15;
            int source_x = (x / 5) * 4 + (x % 5 ? x % 5 - 1 : 0);
            uint32_t pixel = source_frame[source_y * ZELDA_WIDTH + source_x];
            unsigned red = (pixel >> 16) & 0xff;
            unsigned green = (pixel >> 8) & 0xff;
            unsigned blue = pixel & 0xff;

            row[x * 3] = red;
            row[x * 3 + 1] = green;
            row[x * 3 + 2] = blue;
        }
        rb->write(fd, row, sizeof(row));
    }
    rb->close(fd);
    dumped = true;
}
#endif

static fb_data *main_framebuffer(void)
{
    struct viewport *viewport;

    rb->lcd_set_viewport(NULL);
    viewport = *(rb->screens[SCREEN_MAIN]->current_viewport);
    return viewport && viewport->buffer ? viewport->buffer->fb_ptr : NULL;
}

static inline fb_data pack_pixel(uint32_t pixel)
{
    return LCD_RGBPACK((pixel >> 16) & 0xff,
                       (pixel >> 8) & 0xff,
                       pixel & 0xff);
}

void zelda3_video_draw(void)
{
    int y;

    if (!source_frame)
    {
        source_frame = tlsf_malloc(ZELDA_WIDTH * ZELDA_HEIGHT *
                                   sizeof(*source_frame));
        lcd_frame = main_framebuffer();
        if (!source_frame || !lcd_frame)
        {
            zelda3_die("video allocation failed");
            return;
        }
        rb->lcd_set_backdrop(NULL);
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_clear_display();
        update_lcd();
    }

    ZeldaDrawPpuFrame((uint8 *)source_frame,
                      ZELDA_WIDTH * sizeof(*source_frame), 0);
    /* The SNES displayed Zelda's 256x224 active image at a 4:3 pixel aspect.
     * Expand 4 source pixels to 5 LCD pixels and 14 rows to 15 rows. This
     * fills 320x240, corrects the non-square SNES pixels, keeps the full game
     * image, and needs no second framebuffer or filtered-art replacement. */
    for (y = 0; y < LCD_HEIGHT; ++y)
    {
        int source_y = y - (y + 14) / 15;
        const uint32_t *source = source_frame + source_y * ZELDA_WIDTH;
        fb_data *destination = lcd_frame + y * LCD_WIDTH;
        int group;

        for (group = 0; group < ZELDA_WIDTH / 4; ++group)
        {
            fb_data pixel0 = pack_pixel(source[0]);

            destination[0] = pixel0;
            destination[1] = pixel0;
            destination[2] = pack_pixel(source[1]);
            destination[3] = pack_pixel(source[2]);
            destination[4] = pack_pixel(source[3]);
            source += 4;
            destination += 5;
        }
    }
#ifdef SIMULATOR
    dump_test_frame();
#endif
    update_lcd();
    zelda3_rb.rendered++;
}
