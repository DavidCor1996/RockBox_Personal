#include "sm64_rockbox.h"
#include "tlsf.h"
#include "upstream/src/pc/gfx/gfx_window_manager_api.h"
#include "upstream/src/pc/gfx/gfx_soft.h"

#define SM64_RENDER_WIDTH 160
#define SM64_RENDER_HEIGHT 120

static bool render_allowed = true;
static fb_data *lcd_fb;
static fb_data *lcd_stage;

static fb_data *select_lcd_framebuffer(void)
{
    struct viewport *viewport;

    /* lcd_set_viewport() returns the viewport that was active before the
     * call.  In particular, that can be the Games launcher's private
     * viewport, whose storage is no longer ours after the overlay starts.
     * Select the main viewport first, then read back the newly-current one. */
    rb->lcd_set_viewport(NULL);
    viewport = *(rb->screens[SCREEN_MAIN]->current_viewport);
    return viewport && viewport->buffer ? viewport->buffer->fb_ptr : NULL;
}

static void video_init(const char *game_name, bool fullscreen)
{
    (void)game_name;
    (void)fullscreen;
    lcd_fb = select_lcd_framebuffer();
    lcd_stage = tlsf_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(*lcd_stage));
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_clear_display();
    rb->lcd_update();
}

static void video_set_keyboard_callbacks(bool (*down)(int), bool (*up)(int),
                                         void (*all_up)(void))
{
    (void)down;
    (void)up;
    (void)all_up;
}

static void video_set_fullscreen_callback(void (*callback)(bool))
{
    (void)callback;
}

static void video_set_fullscreen(bool enable)
{
    (void)enable;
}

static void video_main_loop(void (*run_one_game_iter)(void))
{
    run_one_game_iter();
}

static void video_get_dimensions(uint32_t *width, uint32_t *height)
{
    *width = SM64_RENDER_WIDTH;
    *height = SM64_RENDER_HEIGHT;
}

static void video_handle_events(void)
{
}

static bool video_start_frame(void)
{
    return render_allowed;
}

static fb_data *get_lcd_framebuffer(void)
{
    if (!lcd_fb)
        lcd_fb = select_lcd_framebuffer();
    return lcd_fb;
}

static inline fb_data pack_pixel(uint32_t pixel)
{
    unsigned red = pixel & 0xff;
    unsigned green = (pixel >> 8) & 0xff;
    unsigned blue = (pixel >> 16) & 0xff;
    return LCD_RGBPACK(red, green, blue);
}

static void video_swap_begin(void)
{
    fb_data *destination = lcd_stage ? lcd_stage : get_lcd_framebuffer();
    unsigned long active_pixels = 0;
    int y;

    if (!gfx_output || !destination)
        return;

#if LCD_WIDTH == 320 && LCD_HEIGHT == 240
    for (y = 0; y < SM64_RENDER_HEIGHT; ++y)
    {
        const uint32_t *source = gfx_output + y * SM64_RENDER_WIDTH;
        fb_data *top = destination + (y * 2) * LCD_WIDTH;
        fb_data *bottom = top + LCD_WIDTH;
        int x;
        for (x = 0; x < SM64_RENDER_WIDTH; ++x)
        {
            uint32_t pixel = source[x];
            fb_data color = pack_pixel(pixel);
            int output_x = x * 2;
            if ((pixel & 0x00ffffff) != 0)
                active_pixels++;
            top[output_x] = color;
            top[output_x + 1] = color;
            bottom[output_x] = color;
            bottom[output_x + 1] = color;
        }
    }
#else
    for (y = 0; y < LCD_HEIGHT; ++y)
    {
        const uint32_t *source = gfx_output +
            (y * SM64_RENDER_HEIGHT / LCD_HEIGHT) * SM64_RENDER_WIDTH;
        fb_data *line = destination + y * LCD_WIDTH;
        int x;
        for (x = 0; x < LCD_WIDTH; ++x)
        {
            uint32_t pixel = source[x * SM64_RENDER_WIDTH / LCD_WIDTH];
            if ((pixel & 0x00ffffff) != 0)
                active_pixels++;
            line[x] = pack_pixel(pixel);
        }
    }
#endif

    if (lcd_stage)
    {
        rb->lcd_set_viewport(NULL);
        rb->lcd_bitmap(lcd_stage, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    }
#if CONFIG_PLATFORM & PLATFORM_NATIVE
    if (sm64_rb.rendered_frames < 120)
    {
        char status[48];

        rb->snprintf(status, sizeof(status), "Frame %lu  pixels %lu",
                     sm64_rb.rendered_frames + 1, active_pixels);
        rb->lcd_setfont(FONT_SYSFIXED);
        rb->lcd_set_drawmode(DRMODE_SOLID);
        rb->lcd_set_background(LCD_BLACK);
        rb->lcd_set_foreground(LCD_WHITE);
        rb->lcd_putsxy(2, 2, status);
    }
    if (sm64_rb.rendered_frames < 128 &&
        ((sm64_rb.rendered_frames & (sm64_rb.rendered_frames + 1)) == 0))
        sm64_logf("frame %lu active_pixels=%lu", sm64_rb.rendered_frames + 1,
                  active_pixels);
#else
    (void)active_pixels;
#endif
    rb->lcd_update();
    sm64_rb.rendered_frames++;
}

static void video_swap_end(void)
{
}

static double video_get_time(void)
{
    return (double)*rb->current_tick / (double)HZ;
}

static void video_shutdown(void)
{
    if (lcd_stage)
    {
        tlsf_free(lcd_stage);
        lcd_stage = NULL;
    }
    lcd_fb = NULL;
}

void sm64_video_set_render_allowed(bool allowed)
{
    render_allowed = allowed;
}

void sm64_video_dump_test_frame(void)
{
#ifdef SIMULATOR
    const char *path = getenv("SM64_TEST_DUMP");
    int fd;
    int y;
    if (!path || !*path || !gfx_output)
        return;
    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    rb->fdprintf(fd, "P6\n%d %d\n255\n", SM64_RENDER_WIDTH,
                 SM64_RENDER_HEIGHT);
    for (y = 0; y < SM64_RENDER_HEIGHT; ++y)
    {
        int x;
        for (x = 0; x < SM64_RENDER_WIDTH; ++x)
        {
            uint32_t pixel = gfx_output[y * SM64_RENDER_WIDTH + x];
            unsigned char rgb[3] = {
                pixel & 0xff, (pixel >> 8) & 0xff, (pixel >> 16) & 0xff
            };
            rb->write(fd, rgb, sizeof(rgb));
        }
    }
    rb->close(fd);
#endif
}

struct GfxWindowManagerAPI sm64_window_api = {
    video_init,
    video_set_keyboard_callbacks,
    video_set_fullscreen_callback,
    video_set_fullscreen,
    video_main_loop,
    video_get_dimensions,
    video_handle_events,
    video_start_frame,
    video_swap_begin,
    video_swap_end,
    video_get_time,
    video_shutdown,
};
