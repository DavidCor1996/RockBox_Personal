#include "plugin.h"
#include "smsgg_video.h"

static fb_data lcd_buf[LCD_WIDTH * LCD_HEIGHT];
static uint16_t palette[32];
static fb_data fb_palette[32];
static uint16_t xmap[LCD_WIDTH];
static uint16_t ymap[LCD_HEIGHT];
static int cached_src_w = -1;
static int cached_src_h = -1;
static int cached_dst_w = -1;
static int cached_dst_h = -1;
static uint16_t cached_palette[32];
static bool palette_valid;

static fb_data pal_to_fb(uint16_t rgb565)
{
    rgb565 = (rgb565 >> 8) | (rgb565 << 8);

    int r = (rgb565 >> 11) & 0x1f;
    int g = (rgb565 >> 5) & 0x3f;
    int b = rgb565 & 0x1f;

    return LCD_RGBPACK((r << 3) | (r >> 2),
                       (g << 2) | (g >> 4),
                       (b << 3) | (b >> 2));
}

void smsgg_video_init(void)
{
    palette_valid = false;
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
}

static void fill_rect(int x, int y, int w, int h, fb_data color)
{
    int row;

    if (x < 0)
    {
        w += x;
        x = 0;
    }
    if (y < 0)
    {
        h += y;
        y = 0;
    }
    if (x + w > LCD_WIDTH)
        w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT)
        h = LCD_HEIGHT - y;
    if (w <= 0 || h <= 0)
        return;

    for (row = 0; row < h; row++)
    {
        int col;
        fb_data *dst = lcd_buf + (y + row) * LCD_WIDTH + x;

        for (col = 0; col < w; col++)
            dst[col] = color;
    }
}

static void clear_borders(int dst_x, int dst_y, int dst_w, int dst_h)
{
    if (dst_y > 0)
        fill_rect(0, 0, LCD_WIDTH, dst_y, LCD_BLACK);
    if (dst_y + dst_h < LCD_HEIGHT)
        fill_rect(0, dst_y + dst_h, LCD_WIDTH,
                  LCD_HEIGHT - (dst_y + dst_h), LCD_BLACK);
    if (dst_x > 0)
        fill_rect(0, dst_y, dst_x, dst_h, LCD_BLACK);
    if (dst_x + dst_w < LCD_WIDTH)
        fill_rect(dst_x + dst_w, dst_y, LCD_WIDTH - (dst_x + dst_w),
                  dst_h, LCD_BLACK);
}

static void update_fb_palette(void)
{
    int i;
    bool changed = !palette_valid;

    render_copy_palette(palette);
    if (!changed)
    {
        for (i = 0; i < 32; i++)
        {
            if (palette[i] != cached_palette[i])
            {
                changed = true;
                break;
            }
        }
    }

    if (!changed)
        return;

    for (i = 0; i < 32; i++)
    {
        cached_palette[i] = palette[i];
        fb_palette[i] = pal_to_fb(palette[i]);
    }

    palette_valid = true;
}

static void blit_gg_fit_fast(void)
{
    int y;
    int last_sy = -1;

    update_fb_palette();

    for (y = 0; y < LCD_HEIGHT; y++)
    {
        int x;
        int sy = (y * 144) / LCD_HEIGHT + bitmap.viewport.y;
        uint8 *src;
        fb_data *dst = lcd_buf + y * LCD_WIDTH;

        if (sy == last_sy)
        {
            rb->memcpy(dst, dst - LCD_WIDTH, LCD_WIDTH * sizeof(fb_data));
            continue;
        }

        last_sy = sy;
        src = bitmap.data + sy * bitmap.pitch + bitmap.viewport.x;

        for (x = 0; x < 160; x++)
        {
            fb_data pixel = fb_palette[src[x] & 0x1f];
            dst[x * 2] = pixel;
            dst[x * 2 + 1] = pixel;
        }
    }
}

static void blit_scaled(struct smsgg_core *core, int src_w, int src_h,
                        int dst_x, int dst_y, int dst_w, int dst_h)
{
    int y;
    int src_x = bitmap.viewport.x;
    int src_y = bitmap.viewport.y;

    update_fb_palette();
    if (src_x < 0)
        src_x = 0;
    if (src_y < 0)
        src_y = 0;
    if (src_w < 1)
        src_w = 1;
    if (src_h < 1)
        src_h = 1;
    if (src_x + src_w > bitmap.width)
        src_w = bitmap.width - src_x;
    if (src_y + src_h > bitmap.height)
        src_h = bitmap.height - src_y;

    if (src_w != cached_src_w || src_h != cached_src_h ||
        dst_w != cached_dst_w || dst_h != cached_dst_h)
    {
        int i;

        cached_src_w = src_w;
        cached_src_h = src_h;
        cached_dst_w = dst_w;
        cached_dst_h = dst_h;
        for (i = 0; i < dst_w; i++)
            xmap[i] = (i * src_w) / dst_w;
        for (i = 0; i < dst_h; i++)
            ymap[i] = (i * src_h) / dst_h;
    }

    for (y = 0; y < dst_h; y++)
    {
        int x;
        int sy = ymap[y] + src_y;
        uint8 *src = bitmap.data + sy * bitmap.pitch + src_x;
        fb_data *dst = lcd_buf + (dst_y + y) * LCD_WIDTH + dst_x;

        (void)core;
        for (x = 0; x < dst_w; x++)
            dst[x] = fb_palette[src[xmap[x]] & 0x1f];
    }
}

static void draw_overlay(bool show_fps, int fps, uint8 mapped_buttons,
                         int wheel_delta, int wheel_zone, bool input_debug)
{
    char line[64];

    if (!show_fps && !input_debug)
        return;

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);

    if (show_fps)
    {
        rb->snprintf(line, sizeof(line), "FPS %d", fps);
        rb->lcd_putsxy(2, 2, line);
    }

    if (input_debug)
    {
        rb->snprintf(line, sizeof(line), "B %02x W %d Z %d",
                     mapped_buttons, wheel_delta, wheel_zone);
        rb->lcd_putsxy(2, LCD_HEIGHT - 12, line);
    }
}

void smsgg_video_draw(struct smsgg_core *core, enum smsgg_scaling_mode mode,
                      bool show_fps, int fps, uint8 mapped_buttons,
                      int wheel_delta, int wheel_zone, bool input_debug)
{
    int src_w;
    int src_h;
    int dst_w = LCD_WIDTH;
    int dst_h = LCD_HEIGHT;
    int dst_x = 0;
    int dst_y = 0;

    src_w = bitmap.viewport.w;
    src_h = bitmap.viewport.h;

    if (src_w < 1 || src_h < 1)
    {
        src_w = 256;
        src_h = 192;
    }

    if (!core->is_gg)
    {
        if (mode == SMSGG_SCALE_SMS_320X230)
        {
            dst_w = 320;
            dst_h = 230;
            dst_y = 5;
        }
        else if (mode == SMSGG_SCALE_FIT && src_h == 192)
        {
            dst_w = 320;
            dst_h = 230;
            dst_y = 5;
        }
    }

    if (mode == SMSGG_SCALE_CONSERVATIVE && !core->is_gg)
    {
        dst_w = 300;
        dst_h = 225;
        dst_x = (LCD_WIDTH - dst_w) / 2;
        dst_y = (LCD_HEIGHT - dst_h) / 2;
    }

    if (core->is_gg && src_w == 160 && src_h == 144 &&
        dst_x == 0 && dst_y == 0 && dst_w == LCD_WIDTH &&
        dst_h == LCD_HEIGHT)
    {
        blit_gg_fit_fast();
        rb->lcd_bitmap(lcd_buf, 0, 0, LCD_WIDTH, LCD_HEIGHT);
        draw_overlay(show_fps, fps, mapped_buttons, wheel_delta,
                     wheel_zone, input_debug);
        rb->lcd_update();
        return;
    }

    clear_borders(dst_x, dst_y, dst_w, dst_h);
    blit_scaled(core, src_w, src_h, dst_x, dst_y, dst_w, dst_h);
    rb->lcd_bitmap(lcd_buf, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    draw_overlay(show_fps, fps, mapped_buttons, wheel_delta, wheel_zone,
                 input_debug);
    rb->lcd_update();
}
