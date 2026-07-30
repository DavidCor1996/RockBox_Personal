/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/                \/
 *
 * Flash SWF viewer/runtime bring-up plugin.
 *
 ****************************************************************************/

#include "lib/plugin_cxx_compat.h"
#include "base/tu_file.h"
#include "gameswf/gameswf.h"
#include "gameswf/gameswf_player.h"
#include "gameswf/gameswf_render.h"
#include "gameswf/gameswf_root.h"
#include "gameswf/gameswf_sprite.h"
#include "gameswf/gameswf_movie_def.h"
#include "base/image.h"
#include "ipod_engine.h"

#include <stdarg.h>
#ifdef SIMULATOR
extern "C" char *getenv(const char *name);
#endif

extern "C" {
#include "lib/plugin_cxx.h"
#include "lib/jpeg_mem.h"
#include "tinf.h"
}

namespace gameswf {
    void clear_shared_fonts();
}

enum flash_input_profile {
    FLASH_INPUT_PROFILE_DEFAULT,
    FLASH_INPUT_PROFILE_STICKRPG,
    FLASH_INPUT_PROFILE_ANTCITY,
};

#define FLASH_BITMAP_TEXT_MAX_SPANS 192
#define FLASH_BITMAP_TEXT_MAX_CHARS 95

struct FlashBitmapTextSpan {
    float stage_x;
    float stage_baseline;
    float stage_height;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
    char text[FLASH_BITMAP_TEXT_MAX_CHARS + 1];
    int length;
};

static FlashBitmapTextSpan g_bitmap_text_spans[FLASH_BITMAP_TEXT_MAX_SPANS];
static int g_bitmap_text_span_count;
static int g_bitmap_text_current = -1;
static bool g_bitmap_text_enabled;

image::rgb *flashplayer_decode_jpeg_rgb(unsigned char *data, unsigned long len)
{
    struct dim size;
    struct bitmap bm;
    image::rgb *im = NULL;
    unsigned char *buffer = NULL;
    int maxsize;
    int rc;

    if (!data || len == 0)
        return NULL;

    if (get_jpeg_dim_mem(data, len, &size) < 0 ||
        size.width <= 0 || size.height <= 0)
        return NULL;

    maxsize = BM_SIZE(size.width, size.height, FORMAT_NATIVE, 0) +
              JPEG_DECODE_OVERHEAD;
    buffer = new unsigned char[maxsize];
    if (!buffer)
        return NULL;

    rb->memset(&bm, 0, sizeof(bm));
    bm.width = size.width;
    bm.height = size.height;
    bm.format = FORMAT_NATIVE;
    bm.data = buffer;

    rc = decode_jpeg_mem(data, len, &bm, maxsize, FORMAT_NATIVE, NULL);
    if (rc >= 0)
    {
        im = image::create_rgb(bm.width, bm.height);
    }

    if (im)
    {
        fb_data *src = (fb_data *)bm.data;
        int stride = BM_WIDTH(bm.width, FORMAT_NATIVE, 0);
        for (int y = 0; y < bm.height; y++)
        {
            unsigned char *dst = im->m_data + y * im->m_pitch;
            for (int x = 0; x < bm.width; x++)
            {
                fb_data px = src[y * stride + x];
                dst[x * 3 + 0] = FB_UNPACK_RED(px);
                dst[x * 3 + 1] = FB_UNPACK_GREEN(px);
                dst[x * 3 + 2] = FB_UNPACK_BLUE(px);
            }
        }
    }

    delete [] buffer;
    return im;
}

#define FLASH_DIR      ROCKBOX_DIR "/flash"
#define STICK_DIR      FLASH_DIR "/stickrpg"
#define FLASH_SHARED_DIR FLASH_DIR "/shared"
#define DEFAULT_SWF    STICK_DIR "/stickrpg.swf"
#define STICK_RPG_URL  "http://www.xgenstudios.com/srpgcompletexgen.swf"
#define FLASH_LOG_PATH FLASH_DIR "/flashplayer.log"
#define FLASH_PROGRESS_PATH FLASH_DIR "/flashplayer.progress"
#define FLASH_CACHE_SUFFIX ".gsc"
#define FLASH_DEF_CACHE_SUFFIX ".gsd"
#define FLASH_FWS_CACHE_SUFFIX ".fws"
#define FLASH_LOADING_BMP ROCKBOX_DIR "/ipodjs/stickrpg/loading.320x240x24.bmp"
#define FLASH_LOADING_BMP_FALLBACK STICK_DIR "/loading.320x240x24.bmp"
#define SIM_SWF        "/home/david/Downloads/stickrpg.swf"
#define FLASH_TRIANGLE_BUDGET 32000
#define FLASH_SHAPE_MESH_BUDGET 4096
#define FLASH_STARTUP_PRERENDER_FRAMES 4
#define FLASH_ALPHA_CUTOFF 2
#define FLASH_MIN_TRIANGLE_AREA2 1

static void flash_logf(const char *fmt, ...);

static int flash_log_fixed(float value, int scale)
{
    float scaled = value * scale;

    return (int)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

#if defined(HAVE_LCD_COLOR)
#define COL_BG      LCD_RGBPACK(14, 16, 18)
#define COL_PANEL   LCD_RGBPACK(32, 36, 40)
#define COL_PANEL2  LCD_RGBPACK(47, 53, 58)
#define COL_INK     LCD_RGBPACK(236, 239, 232)
#define COL_DIM     LCD_RGBPACK(159, 168, 160)
#define COL_ACCENT  LCD_RGBPACK(245, 190, 54)
#define COL_OK      LCD_RGBPACK(91, 186, 102)
#define COL_WARN    LCD_RGBPACK(230, 93, 74)
#define COL_FLASH   LCD_RGBPACK(221, 56, 42)
#define COL_BAR     LCD_RGBPACK(74, 144, 226)
#define SET_FG(c)   rb->lcd_set_foreground(c)
#define SET_BG(c)   rb->lcd_set_background(c)
#else
#define COL_BG      LCD_WHITE
#define COL_PANEL   LCD_BLACK
#define COL_PANEL2  LCD_DARKGRAY
#define COL_INK     LCD_BLACK
#define COL_DIM     LCD_DARKGRAY
#define COL_ACCENT  LCD_BLACK
#define COL_OK      LCD_BLACK
#define COL_WARN    LCD_BLACK
#define COL_FLASH   LCD_BLACK
#define COL_BAR     LCD_BLACK
#define SET_FG(c)   rb->lcd_set_foreground(c)
#define SET_BG(c)   rb->lcd_set_background(c)
#endif

static unsigned rb_color_from_rgba(const gameswf::rgba& c)
{
#if defined(HAVE_LCD_COLOR)
    return LCD_RGBPACK(c.m_r, c.m_g, c.m_b);
#else
    return c.m_r + c.m_g + c.m_b > 384 ? LCD_WHITE : LCD_BLACK;
#endif
}

struct RockboxBitmapInfo : public gameswf::bitmap_info {
    int w;
    int h;
    int bpp;
    int pitch;
    unsigned char *data;
    bool owns_data;

    RockboxBitmapInfo() : w(0), h(0), bpp(0), pitch(0), data(NULL), owns_data(false) {}
    RockboxBitmapInfo(int width, int height, int bytes_per_pixel,
                      int row_pitch, unsigned char *src)
        : w(width), h(height), bpp(bytes_per_pixel),
          pitch(row_pitch), data(NULL), owns_data(false)
    {
        int size;
        if (w <= 0 || h <= 0 || bpp <= 0 || pitch <= 0 || !src)
            return;

        size = pitch * h;
        data = new unsigned char[size];
        if (data) {
            rb->memcpy(data, src, size);
            owns_data = true;
        } else {
            w = h = bpp = pitch = 0;
        }
    }

    virtual ~RockboxBitmapInfo()
    {
        if (owns_data)
            delete [] data;
    }

    virtual int get_width() const { return w; }
    virtual int get_height() const { return h; }
    virtual unsigned char *get_data() const { return data; }
    virtual int get_bpp() const { return bpp; }
};

struct RockboxRenderHandler : public gameswf::render_handler {
    struct FillState {
        gameswf::rgba color;
        gameswf::bitmap_info *bitmap;
        gameswf::matrix bitmap_matrix;
        gameswf::cxform bitmap_cx;
        bitmap_wrap_mode bitmap_wrap;
        bool is_bitmap;
        bool enabled;

        FillState()
            : color(180, 180, 180, 255), bitmap(NULL),
              bitmap_wrap(WRAP_CLAMP), is_bitmap(false), enabled(false)
        {
        }
    };

    fb_data *framebuf;
    unsigned char *maskbuf;
    gameswf::matrix mat;
    gameswf::cxform cx;
    FillState fills[2];
    gameswf::rgba fill_color;
    gameswf::rgba line_color;
    gameswf::bitmap_info *fill_bitmap;
    gameswf::matrix fill_bitmap_matrix;
    gameswf::cxform fill_bitmap_cx;
    bitmap_wrap_mode fill_bitmap_wrap;
    bool fill_is_bitmap;
    bool fill_enabled;
    int active_fill_side;
    bool line_enabled;
    int line_width;
    float sx;
    float sy;
    float tx;
    float ty;
    float display_x0;
    float display_x1;
    float display_y0;
    float display_y1;
    int triangles;
    int bitmaps;
    int lines;
    int begins;
    int solid_fills;
    int bitmap_fills;
    int mask_tests;
    int mask_begins;
    int mask_ends;
    int mask_disables;
    int alpha_skips;
    int alpha_blends;
    int opaque_pixels;
    int span_pixels;
    int tiny_triangles;
    int max_tri_w;
    int max_tri_h;
    int max_tri_area;
    int fill_trace_count;
    int triangle_trace_count;
    int display_count;
    bool triangle_budget_hit;
    bool mask_submitting;
    bool mask_active;
    bool discard_render;
    gameswf::rgba last_solid;
    gameswf::rgba last_bitmap_sample;

    RockboxRenderHandler()
        : framebuf(NULL), maskbuf(NULL),
          fill_color(220, 220, 220, 255), line_color(30, 30, 30, 255),
          fill_bitmap(NULL), fill_bitmap_wrap(WRAP_CLAMP), fill_is_bitmap(false),
          fill_enabled(false), active_fill_side(0), line_enabled(false),
          line_width(1), sx(1.0f), sy(1.0f), tx(0.0f), ty(0.0f),
          display_x0(0.0f), display_x1(1.0f),
          display_y0(0.0f), display_y1(1.0f),
          triangles(0), bitmaps(0), lines(0), begins(0),
          solid_fills(0), bitmap_fills(0), mask_tests(0), mask_begins(0),
          mask_ends(0), mask_disables(0), alpha_skips(0), alpha_blends(0),
          opaque_pixels(0), span_pixels(0), tiny_triangles(0),
          max_tri_w(0), max_tri_h(0), max_tri_area(0),
          fill_trace_count(0), triangle_trace_count(0),
          display_count(0),
          triangle_budget_hit(false), mask_submitting(false), mask_active(false),
          discard_render(false),
          last_solid(0, 0, 0, 255),
          last_bitmap_sample(0, 0, 0, 255)
    {
        framebuf = new fb_data[LCD_WIDTH * LCD_HEIGHT];
        maskbuf = new unsigned char[LCD_WIDTH * LCD_HEIGHT];
        if (maskbuf)
            rb->memset(maskbuf, 0, LCD_WIDTH * LCD_HEIGHT);
    }

    virtual ~RockboxRenderHandler()
    {
        delete [] maskbuf;
        delete [] framebuf;
    }

    virtual gameswf::bitmap_info *create_bitmap_info_empty()
    {
        return new RockboxBitmapInfo;
    }

    virtual gameswf::bitmap_info *create_bitmap_info_alpha(int w, int h,
                                                           unsigned char *data)
    {
        return new RockboxBitmapInfo(w, h, 1, w, data);
    }

    virtual gameswf::bitmap_info *create_bitmap_info_rgb(image::rgb *im)
    {
        return new RockboxBitmapInfo(im ? im->m_width : 0,
                                     im ? im->m_height : 0, 3,
                                     im ? im->m_pitch : 0,
                                     im ? (unsigned char *)im->m_data : NULL);
    }

    virtual gameswf::bitmap_info *create_bitmap_info_rgba(image::rgba *im)
    {
        return new RockboxBitmapInfo(im ? im->m_width : 0,
                                     im ? im->m_height : 0, 4,
                                     im ? im->m_pitch : 0,
                                     im ? (unsigned char *)im->m_data : NULL);
    }

    virtual gameswf::video_handler *create_video_handler()
    {
        return NULL;
    }

    virtual void begin_display(gameswf::rgba background_color,
                               int viewport_x0, int viewport_y0,
                               int viewport_width, int viewport_height,
                               float x0, float x1, float y0, float y1)
    {
        float sw = x1 - x0;
        float sh = y1 - y0;
        triangles = 0;
        bitmaps = 0;
        lines = 0;
        solid_fills = 0;
        bitmap_fills = 0;
        mask_tests = 0;
        mask_begins = 0;
        mask_ends = 0;
        mask_disables = 0;
        alpha_skips = 0;
        alpha_blends = 0;
        opaque_pixels = 0;
        span_pixels = 0;
        tiny_triangles = 0;
        max_tri_w = 0;
        max_tri_h = 0;
        max_tri_area = 0;
        fill_trace_count = 0;
        triangle_trace_count = 0;
        triangle_budget_hit = false;
        fill_enabled = false;
        active_fill_side = 0;
        line_enabled = false;
        g_bitmap_text_span_count = 0;
        g_bitmap_text_current = -1;
        begins++;
        sx = sw != 0.0f ? viewport_width / sw : 1.0f;
        sy = sh != 0.0f ? viewport_height / sh : 1.0f;
        tx = viewport_x0 - x0 * sx;
        ty = viewport_y0 - y0 * sy;
        display_x0 = x0 < x1 ? x0 : x1;
        display_x1 = x0 < x1 ? x1 : x0;
        display_y0 = y0 < y1 ? y0 : y1;
        display_y1 = y0 < y1 ? y1 : y0;
        rb->lcd_set_viewport(NULL);
        if (discard_render)
            return;
#ifdef SIMULATOR
        {
            const char *trace = getenv("FLASHPLAYER_TRACE_FILLS");
            if (trace && trace[0] && rb->atoi(trace) != 0)
                flash_logf("display begin bg=%u,%u,%u,%u viewport=%d,%d %dx%d stage=%.1f,%.1f-%.1f,%.1f",
                           background_color.m_r, background_color.m_g,
                           background_color.m_b, background_color.m_a,
                           viewport_x0, viewport_y0, viewport_width,
                           viewport_height, x0, y0, x1, y1);
        }
#endif
        {
            fb_data bg = (fb_data)rb_color_from_rgba(background_color);
            int i;
            if (framebuf) {
                for (i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++)
                    framebuf[i] = bg;
            } else {
                rb->lcd_set_foreground(bg);
                rb->lcd_fillrect(viewport_x0, viewport_y0,
                                 viewport_width, viewport_height);
            }
        }
    }

    virtual void end_display()
    {
        rb->lcd_set_viewport(NULL);
        if (discard_render)
            return;
        if (framebuf)
            rb->lcd_bitmap(framebuf, 0, 0, LCD_WIDTH, LCD_HEIGHT);
#ifdef SIMULATOR
        maybe_dump_framebuffer_ppm();
#endif
    }

#ifdef SIMULATOR
    void maybe_dump_framebuffer_ppm()
    {
        static bool dumped = false;
        const char *env;
        int dump_display;
        int fd;
        char header[64];
        int header_len;
        unsigned char *row;

        display_count++;

        if (dumped || !framebuf)
            return;

        env = getenv("FLASHPLAYER_DUMP_FRAME");
        if (!env || !env[0] || rb->atoi(env) == 0)
            return;
        dump_display = rb->atoi(env);
        if (dump_display < 0)
            dump_display = 1;
        if (display_count < dump_display)
            return;

        rb->mkdir(FLASH_DIR);
        fd = rb->open(FLASH_DIR "/frame.ppm",
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0)
            return;

        header_len = rb->snprintf(header, sizeof(header),
                                  "P6\n%d %d\n255\n", LCD_WIDTH, LCD_HEIGHT);
        rb->write(fd, header, header_len);

        row = new unsigned char[LCD_WIDTH * 3];
        if (row) {
            int y;
            for (y = 0; y < LCD_HEIGHT; y++) {
                int x;
                for (x = 0; x < LCD_WIDTH; x++) {
                    fb_data px = framebuf[y * LCD_WIDTH + x];
                    row[x * 3 + 0] = FB_UNPACK_RED(px);
                    row[x * 3 + 1] = FB_UNPACK_GREEN(px);
                    row[x * 3 + 2] = FB_UNPACK_BLUE(px);
                }
                rb->write(fd, row, LCD_WIDTH * 3);
            }
            delete [] row;
        }

        rb->close(fd);
        dumped = true;
        flash_logf("frame dump path=%s display=%d", FLASH_DIR "/frame.ppm",
                   display_count);
    }
#endif

    virtual void set_matrix(const gameswf::matrix& m) { mat = m; }
    virtual void set_cxform(const gameswf::cxform& c) { cx = c; }

    static int round_pixel(float v)
    {
        /* AVM1 projection code can legitimately place geometry far beyond
         * the viewport.  Keep the float-to-int conversion defined and leave
         * the final rejection to the line/triangle clippers. */
        if (v != v)
            return 0;
        if (v > 32768.0f)
            return 32768;
        if (v < -32768.0f)
            return -32768;
        return (int)(v >= 0.0f ? v + 0.5f : v - 0.5f);
    }

    static int line_outcode(int x, int y)
    {
        int code = 0;
        if (x < 0) code |= 1;
        else if (x >= LCD_WIDTH) code |= 2;
        if (y < 0) code |= 4;
        else if (y >= LCD_HEIGHT) code |= 8;
        return code;
    }

    static bool clip_line_to_lcd(int *x0, int *y0, int *x1, int *y1)
    {
        int c0 = line_outcode(*x0, *y0);
        int c1 = line_outcode(*x1, *y1);
        int guard = 0;

        while (guard++ < 8) {
            int out;
            int x;
            int y;
            long long dx;
            long long dy;

            if ((c0 | c1) == 0)
                return true;
            if (c0 & c1)
                return false;

            out = c0 ? c0 : c1;
            dx = (long long)*x1 - *x0;
            dy = (long long)*y1 - *y0;

            if (out & 8) {
                if (dy == 0) return false;
                y = LCD_HEIGHT - 1;
                x = *x0 + (int)(dx * (y - *y0) / dy);
            } else if (out & 4) {
                if (dy == 0) return false;
                y = 0;
                x = *x0 + (int)(dx * (y - *y0) / dy);
            } else if (out & 2) {
                if (dx == 0) return false;
                x = LCD_WIDTH - 1;
                y = *y0 + (int)(dy * (x - *x0) / dx);
            } else {
                if (dx == 0) return false;
                x = 0;
                y = *y0 + (int)(dy * (x - *x0) / dx);
            }

            if (out == c0) {
                *x0 = x;
                *y0 = y;
                c0 = line_outcode(x, y);
            } else {
                *x1 = x;
                *y1 = y;
                c1 = line_outcode(x, y);
            }
        }
        return false;
    }

    void transform_point(float x, float y, int *ox, int *oy)
    {
        float mx = mat.m_[0][0] * x + mat.m_[0][1] * y + mat.m_[0][2];
        float my = mat.m_[1][0] * x + mat.m_[1][1] * y + mat.m_[1][2];
        *ox = round_pixel(mx * sx + tx);
        *oy = round_pixel(my * sy + ty);
    }

    void transform_point_screen(float x, float y, float *ox, float *oy)
    {
        float mx = mat.m_[0][0] * x + mat.m_[0][1] * y + mat.m_[0][2];
        float my = mat.m_[1][0] * x + mat.m_[1][1] * y + mat.m_[1][2];
        *ox = mx * sx + tx;
        *oy = my * sy + ty;
    }

    void draw_pixel_rgba(int x, int y, gameswf::rgba c)
    {
        int a;

        if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT)
            return;

        a = c.m_a;
        if (a < FLASH_ALPHA_CUTOFF)
        {
            alpha_skips++;
            return;
        }

        if (mask_submitting) {
            if (maskbuf)
                maskbuf[y * LCD_WIDTH + x] = 1;
            return;
        }
        if (mask_active && (!maskbuf || !maskbuf[y * LCD_WIDTH + x]))
            return;

        if (framebuf) {
            fb_data *dst = framebuf + y * LCD_WIDTH + x;
            if (a >= 255) {
                *dst = (fb_data)rb_color_from_rgba(c);
                opaque_pixels++;
            } else {
#if defined(HAVE_LCD_COLOR)
                int inv = 255 - a;
                int r = (c.m_r * a + RGB_UNPACK_RED(*dst) * inv + 127) / 255;
                int g = (c.m_g * a + RGB_UNPACK_GREEN(*dst) * inv + 127) / 255;
                int b = (c.m_b * a + RGB_UNPACK_BLUE(*dst) * inv + 127) / 255;
                *dst = (fb_data)LCD_RGBPACK(r, g, b);
#else
                *dst = (c.m_r + c.m_g + c.m_b > 384) ? LCD_WHITE : LCD_BLACK;
#endif
                alpha_blends++;
            }
        } else {
            rb->lcd_set_foreground(rb_color_from_rgba(c));
            rb->lcd_drawpixel(x, y);
        }
    }

    void put_packed_pixel(int x, int y, fb_data color)
    {
        if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT)
            return;
        if (mask_submitting) {
            if (maskbuf)
                maskbuf[y * LCD_WIDTH + x] = 1;
            return;
        }
        if (mask_active && (!maskbuf || !maskbuf[y * LCD_WIDTH + x]))
            return;
        if (framebuf)
            framebuf[y * LCD_WIDTH + x] = color;
        else {
            rb->lcd_set_foreground(color);
            rb->lcd_drawpixel(x, y);
        }
    }

    void fill_span_rgba(int y, int xa, int xb, gameswf::rgba color)
    {
        int count;
        int a = color.m_a;

        if (y < 0 || y >= LCD_HEIGHT || xb < 0 || xa >= LCD_WIDTH)
            return;
        if (xa < 0) xa = 0;
        if (xb >= LCD_WIDTH) xb = LCD_WIDTH - 1;
        if (xa > xb)
            return;
        if (a < FLASH_ALPHA_CUTOFF) {
            alpha_skips += xb - xa + 1;
            return;
        }

        count = xb - xa + 1;
        if (mask_submitting) {
            int x;
            if (maskbuf) {
                for (x = xa; x <= xb; x++)
                    maskbuf[y * LCD_WIDTH + x] = 1;
            }
            return;
        }
        if (mask_active) {
            int x;
            for (x = xa; x <= xb; x++)
                draw_pixel_rgba(x, y, color);
            return;
        }

        span_pixels += count;
        if (framebuf) {
            fb_data *dst = framebuf + y * LCD_WIDTH + xa;
            if (a >= 255) {
                fb_data packed = (fb_data)rb_color_from_rgba(color);
                int i;
                for (i = 0; i < count; i++)
                    dst[i] = packed;
                opaque_pixels += count;
            } else {
#if defined(HAVE_LCD_COLOR)
                int inv = 255 - a;
                int i;
                for (i = 0; i < count; i++) {
                    int r = (color.m_r * a + RGB_UNPACK_RED(dst[i]) * inv + 127) / 255;
                    int g = (color.m_g * a + RGB_UNPACK_GREEN(dst[i]) * inv + 127) / 255;
                    int b = (color.m_b * a + RGB_UNPACK_BLUE(dst[i]) * inv + 127) / 255;
                    dst[i] = (fb_data)LCD_RGBPACK(r, g, b);
                }
#else
                int i;
                fb_data packed = (color.m_r + color.m_g + color.m_b > 384) ?
                    LCD_WHITE : LCD_BLACK;
                for (i = 0; i < count; i++)
                    dst[i] = packed;
#endif
                alpha_blends += count;
            }
        } else {
            rb->lcd_set_foreground(rb_color_from_rgba(color));
            rb->lcd_hline(xa, xb, y);
        }
    }

    void draw_packed_line(int x0, int y0, int x1, int y1, fb_data color)
    {
        if (!clip_line_to_lcd(&x0, &y0, &x1, &y1))
            return;

        int dx = x1 > x0 ? x1 - x0 : x0 - x1;
        int sxp = x0 < x1 ? 1 : -1;
        int dy = y1 > y0 ? y0 - y1 : y1 - y0;
        int syp = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true) {
            put_packed_pixel(x0, y0, color);
            if (x0 == x1 && y0 == y1)
                break;
            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sxp;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += syp;
            }
        }
    }

    void fill_tiny_solid(int x0, int y0, int x1, int y1, int x2, int y2,
                         gameswf::rgba color)
    {
        int minx = MIN(x0, MIN(x1, x2));
        int maxx = MAX(x0, MAX(x1, x2));
        int miny = MIN(y0, MIN(y1, y2));
        int maxy = MAX(y0, MAX(y1, y2));
        int x, y;

        if (color.m_a < FLASH_ALPHA_CUTOFF)
            return;
        if (minx < 0) minx = 0;
        if (miny < 0) miny = 0;
        if (maxx >= LCD_WIDTH) maxx = LCD_WIDTH - 1;
        if (maxy >= LCD_HEIGHT) maxy = LCD_HEIGHT - 1;
        if (maxx < minx || maxy < miny)
            return;

        if (maxx - minx > 10 || maxy - miny > 10) {
            fb_data packed = (fb_data)rb_color_from_rgba(color);
            draw_packed_line(x0, y0, x1, y1, packed);
            draw_packed_line(x1, y1, x2, y2, packed);
            draw_packed_line(x2, y2, x0, y0, packed);
            return;
        }

        for (y = miny; y <= maxy; y++)
            for (x = minx; x <= maxx; x++)
                draw_pixel_rgba(x, y, color);
    }

    static float edgef(float ax, float ay, float bx, float by,
                       float px, float py)
    {
        return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
    }

    void fill_screen_triangle_float(float x0, float y0, float x1, float y1,
                                    float x2, float y2, bool allow_fallback)
    {
        float fminx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
        float fmaxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
        float fminy = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
        float fmaxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
        int minx = (int)(fminx - 1.0f);
        int maxx = (int)(fmaxx + 1.0f);
        int miny = (int)(fminy - 1.0f);
        int maxy = (int)(fmaxy + 1.0f);
        int drew = 0;
        int x, y;

        if (minx < 0) minx = 0;
        if (miny < 0) miny = 0;
        if (maxx >= LCD_WIDTH) maxx = LCD_WIDTH - 1;
        if (maxy >= LCD_HEIGHT) maxy = LCD_HEIGHT - 1;
        if (maxx < minx || maxy < miny)
            return;

        for (y = miny; y <= maxy; y++) {
            for (x = minx; x <= maxx; x++) {
                float px = (float)x + 0.5f;
                float py = (float)y + 0.5f;
                float e0 = edgef(x0, y0, x1, y1, px, py);
                float e1 = edgef(x1, y1, x2, y2, px, py);
                float e2 = edgef(x2, y2, x0, y0, px, py);
                bool neg = e0 < 0.0f || e1 < 0.0f || e2 < 0.0f;
                bool pos = e0 > 0.0f || e1 > 0.0f || e2 > 0.0f;

                if (!(neg && pos)) {
                    if (fill_is_bitmap) {
                        gameswf::point stage_pt(((float)x - tx) / sx,
                                                 ((float)y - ty) / sy);
                        gameswf::point object_pt;
                        mat.transform_by_inverse(&object_pt, stage_pt);
                        draw_pixel_rgba(x, y,
                            sample_bitmap_rgba(object_pt.m_x, object_pt.m_y));
                    } else {
                        draw_pixel_rgba(x, y, cx.transform(fill_color));
                    }
                    drew++;
                }
            }
        }

        if (drew == 0 && allow_fallback) {
            int cxp = round_pixel((x0 + x1 + x2) / 3.0f);
            int cyp = round_pixel((y0 + y1 + y2) / 3.0f);
            if (fill_is_bitmap) {
                gameswf::point stage_pt(((float)cxp - tx) / sx,
                                         ((float)cyp - ty) / sy);
                gameswf::point object_pt;
                mat.transform_by_inverse(&object_pt, stage_pt);
                draw_pixel_rgba(cxp, cyp,
                    sample_bitmap_rgba(object_pt.m_x, object_pt.m_y));
            } else {
                draw_pixel_rgba(cxp, cyp, cx.transform(fill_color));
            }
        }
    }

    void select_fill_side(int fill_side)
    {
        if (fill_side < 0 || fill_side >= 2)
            fill_side = 0;

        active_fill_side = fill_side;
        fill_enabled = fills[fill_side].enabled;
        fill_is_bitmap = fills[fill_side].is_bitmap;
        fill_bitmap = fills[fill_side].bitmap;
        fill_bitmap_matrix = fills[fill_side].bitmap_matrix;
        fill_bitmap_cx = fills[fill_side].bitmap_cx;
        fill_bitmap_wrap = fills[fill_side].bitmap_wrap;
        fill_color = fills[fill_side].color;
    }

    gameswf::rgba sample_bitmap_rgba(float local_x, float local_y)
    {
        RockboxBitmapInfo *bi = (RockboxBitmapInfo *)fill_bitmap;
        gameswf::point uv;
        int x;
        int y;
        unsigned char *p;
        gameswf::rgba c;

        if (!bi || !bi->data || bi->w <= 0 || bi->h <= 0)
            return cx.transform(fill_color);

        fill_bitmap_matrix.transform_by_inverse(&uv,
                                                gameswf::point(local_x,
                                                               local_y));
        x = (int)floorf(uv.m_x + 0.5f);
        y = (int)floorf(uv.m_y + 0.5f);

        if (fill_bitmap_wrap == WRAP_REPEAT) {
            x %= bi->w;
            y %= bi->h;
            if (x < 0) x += bi->w;
            if (y < 0) y += bi->h;
        } else {
            if (x < 0)
                x = 0;
            else if (x >= bi->w)
                x = bi->w - 1;
            if (y < 0)
                y = 0;
            else if (y >= bi->h)
                y = bi->h - 1;
        }

        p = bi->data + y * bi->pitch + x * bi->bpp;
        if (bi->bpp >= 3)
            c = gameswf::rgba(p[0], p[1], p[2], bi->bpp >= 4 ? p[3] : 255);
        else
            c = gameswf::rgba(p[0], p[0], p[0], 255);
        last_bitmap_sample = fill_bitmap_cx.transform(c);
        return last_bitmap_sample;
    }

    static void swap_int(int *a, int *b)
    {
        int t = *a;
        *a = *b;
        *b = t;
    }

    void draw_solid_triangle(int x0, int y0, int x1, int y1, int x2, int y2,
                             gameswf::rgba color)
    {
        int miny;
        int maxy;
        int y;

        if (color.m_a < FLASH_ALPHA_CUTOFF)
            return;

        if (y1 < y0) { swap_int(&x0, &x1); swap_int(&y0, &y1); }
        if (y2 < y1) { swap_int(&x1, &x2); swap_int(&y1, &y2); }
        if (y1 < y0) { swap_int(&x0, &x1); swap_int(&y0, &y1); }

        miny = y0 < 0 ? 0 : y0;
        maxy = y2 >= LCD_HEIGHT ? LCD_HEIGHT - 1 : y2;
        rb->lcd_set_foreground(rb_color_from_rgba(color));

        for (y = miny; y <= maxy; y++) {
            float xs[3];
            int n = 0;

            if (y1 != y0 && y >= y0 && y < y1)
                xs[n++] = x0 + (float)(x1 - x0) * (float)(y - y0) /
                    (float)(y1 - y0);
            if (y2 != y1 && y >= y1 && y <= y2)
                xs[n++] = x1 + (float)(x2 - x1) * (float)(y - y1) /
                    (float)(y2 - y1);
            if (y2 != y0 && y >= y0 && y <= y2)
                xs[n++] = x0 + (float)(x2 - x0) * (float)(y - y0) /
                    (float)(y2 - y0);

            if (n >= 2) {
                int xa = (int)(xs[0] < xs[1] ? xs[0] : xs[1]);
                int xb = (int)(xs[0] < xs[1] ? xs[1] : xs[0]);
                if (xa < 0) xa = 0;
                if (xb >= LCD_WIDTH) xb = LCD_WIDTH - 1;
                if (xa <= xb)
                    fill_span_rgba(y, xa, xb, color);
            }
        }
    }

    void draw_triangle(float ax, float ay, float bx, float by, float cxp, float cyp)
    {
        int x0, y0, x1, y1, x2, y2;
        int minx, maxx, miny, maxy;
        int x, y;

        transform_point(ax, ay, &x0, &y0);
        transform_point(bx, by, &x1, &y1);
        transform_point(cxp, cyp, &x2, &y2);

        if (!fill_enabled)
            return;

        if (discard_render) {
            triangles++;
            return;
        }

        {
            int area2 = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
            if (area2 < 0) area2 = -area2;
            if (area2 < FLASH_MIN_TRIANGLE_AREA2) {
                /* The vertices collapse to a zero-area triangle at the
                 * 320x240 output resolution. Stick RPG produces roughly ten
                 * thousand of these per street frame, so the floating-point
                 * fallback was spending most of the frame on invisible
                 * subpixels. */
                tiny_triangles++;
                return;
            }
        }

        triangles++;
        if (triangles > FLASH_TRIANGLE_BUDGET) {
            triangle_budget_hit = true;
            return;
        }
        minx = MIN(x0, MIN(x1, x2));
        maxx = MAX(x0, MAX(x1, x2));
        miny = MIN(y0, MIN(y1, y2));
        maxy = MAX(y0, MAX(y1, y2));
        {
            int bw = maxx - minx + 1;
            int bh = maxy - miny + 1;
            int ba = bw * bh;
            if (ba > max_tri_area) {
                max_tri_area = ba;
                max_tri_w = bw;
                max_tri_h = bh;
            }
#if 0
            if ((triangle_trace_count < 180 ||
                 (triangles > 4300 && triangle_trace_count < 260)) &&
                ba > 200) {
                gameswf::rgba tc = cx.transform(fill_color);
                triangle_trace_count++;
                flash_logf("tri trace n=%d ba=%d bbox=%d,%d-%d,%d color=%u,%u,%u,%u bitmap=%d",
                           triangle_trace_count, ba, minx, miny, maxx, maxy,
                           tc.m_r, tc.m_g, tc.m_b, tc.m_a,
                           fill_is_bitmap ? 1 : 0);
            }
#endif
        }
        if (minx < 0) minx = 0;
        if (miny < 0) miny = 0;
        if (maxx >= LCD_WIDTH) maxx = LCD_WIDTH - 1;
        if (maxy >= LCD_HEIGHT) maxy = LCD_HEIGHT - 1;

        {
            float fx0, fy0, fx1, fy1, fx2, fy2;
            transform_point_screen(ax, ay, &fx0, &fy0);
            transform_point_screen(bx, by, &fx1, &fy1);
            transform_point_screen(cxp, cyp, &fx2, &fy2);
            fill_screen_triangle_float(fx0, fy0, fx1, fy1, fx2, fy2, false);
        }
    }

    virtual void draw_mesh_strip(const void *coords, int vertex_count)
    {
        const coord_component *c = (const coord_component *)coords;
        int i;
        select_fill_side(0);
        for (i = 0; i + 2 < vertex_count; i++)
            draw_triangle(c[i * 2], c[i * 2 + 1],
                          c[(i + 1) * 2], c[(i + 1) * 2 + 1],
                          c[(i + 2) * 2], c[(i + 2) * 2 + 1]);
    }

    virtual void draw_triangle_list(const void *coords, int vertex_count)
    {
        const coord_component *c = (const coord_component *)coords;
        int i;
        select_fill_side(0);
        for (i = 0; i + 2 < vertex_count; i += 3)
            draw_triangle(c[i * 2], c[i * 2 + 1],
                          c[(i + 1) * 2], c[(i + 1) * 2 + 1],
                          c[(i + 2) * 2], c[(i + 2) * 2 + 1]);
    }

    virtual void draw_line_strip(const void *coords, int vertex_count)
    {
        const coord_component *c = (const coord_component *)coords;
        int i;
        int x0, y0, x1, y1;

        if (!line_enabled)
            return;
        if (discard_render) {
            lines += vertex_count > 1 ? vertex_count - 1 : 0;
            return;
        }

        {
        fb_data packed = (fb_data)rb_color_from_rgba(cx.transform(line_color));
        rb->lcd_set_foreground(packed);
        for (i = 0; i + 1 < vertex_count; i++) {
            lines++;
            transform_point(c[i * 2], c[i * 2 + 1], &x0, &y0);
            transform_point(c[(i + 1) * 2], c[(i + 1) * 2 + 1], &x1, &y1);
            if (framebuf)
                draw_packed_line(x0, y0, x1, y1, packed);
            else if (clip_line_to_lcd(&x0, &y0, &x1, &y1))
                rb->lcd_drawline(x0, y0, x1, y1);
        }
        }
    }

    virtual void fill_style_disable(int fill_side)
    {
        if (fill_side >= 0 && fill_side < 2) {
            fills[fill_side].enabled = false;
            fills[fill_side].is_bitmap = false;
            fills[fill_side].bitmap = NULL;
            if (fill_side == active_fill_side)
                select_fill_side(fill_side);
        }
    }
    virtual void fill_style_color(int fill_side, const gameswf::rgba& color)
    {
        if (fill_side < 0 || fill_side >= 2)
            return;
        fills[fill_side].enabled = true;
        fills[fill_side].is_bitmap = false;
        fills[fill_side].bitmap = NULL;
        fills[fill_side].color = color;
        if (fill_side == active_fill_side)
            select_fill_side(fill_side);
        last_solid = cx.transform(color);
        solid_fills++;
#ifdef SIMULATOR
        {
        const char *trace = getenv("FLASHPLAYER_TRACE_FILLS");
        if (trace && trace[0] && rb->atoi(trace) != 0 &&
            fill_trace_count < 80) {
            gameswf::rgba tc = cx.transform(color);
            fill_trace_count++;
            flash_logf("fill trace n=%d side=%d color=%u,%u,%u,%u transformed=%u,%u,%u,%u cx=%.2f,%.2f,%.2f,%.2f add=%.1f,%.1f,%.1f,%.1f tri=%d",
                       fill_trace_count,
                       fill_side,
                       color.m_r, color.m_g, color.m_b, color.m_a,
                       tc.m_r, tc.m_g, tc.m_b, tc.m_a,
                       (double)cx.m_[0][0], (double)cx.m_[1][0],
                       (double)cx.m_[2][0], (double)cx.m_[3][0],
                       (double)cx.m_[0][1], (double)cx.m_[1][1],
                       (double)cx.m_[2][1], (double)cx.m_[3][1],
                       triangles);
        }
        }
#endif
    }
    virtual void fill_style_bitmap(int fill_side, gameswf::bitmap_info *bi,
                                   const gameswf::matrix& m,
                                   bitmap_wrap_mode wm, bitmap_blend_mode bm)
    {
        (void)bm;
        if (fill_side < 0 || fill_side >= 2)
            return;
        fills[fill_side].enabled = bi != NULL;
        fills[fill_side].is_bitmap = bi != NULL;
        fills[fill_side].bitmap = bi;
        fills[fill_side].bitmap_matrix = m;
        fills[fill_side].bitmap_cx = cx;
        fills[fill_side].bitmap_wrap = wm;
        fills[fill_side].color = gameswf::rgba(180, 180, 180, 255);
        if (fill_side == active_fill_side)
            select_fill_side(fill_side);
        bitmap_fills++;
    }
    virtual void line_style_disable() { line_enabled = false; }
    virtual void line_style_color(gameswf::rgba color) { line_enabled = true; line_color = color; }
    virtual void line_style_width(float width) { line_width = (int)width; }
    virtual void draw_bitmap(const gameswf::matrix& m, gameswf::bitmap_info *bi,
                             const gameswf::rect& coords,
                             const gameswf::rect& uv_coords,
                             gameswf::rgba color)
    {
        RockboxBitmapInfo *rbi = (RockboxBitmapInfo *)bi;
        gameswf::point a;
        gameswf::point b;
        int x0;
        int y0;
        int x1;
        int y1;
        int x;
        int y;
        bitmaps++;
        if (discard_render)
            return;
        if (!rbi || !rbi->data || rbi->w <= 0 || rbi->h <= 0)
            return;

        m.transform(&a, gameswf::point(coords.m_x_min, coords.m_y_min));
        m.transform(&b, gameswf::point(coords.m_x_max, coords.m_y_max));
        x0 = (int)(a.m_x * sx + tx);
        y0 = (int)(a.m_y * sy + ty);
        x1 = (int)(b.m_x * sx + tx);
        y1 = (int)(b.m_y * sy + ty);
        if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
        if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 >= LCD_WIDTH) x1 = LCD_WIDTH - 1;
        if (y1 >= LCD_HEIGHT) y1 = LCD_HEIGHT - 1;

        for (y = y0; y <= y1; y++) {
            for (x = x0; x <= x1; x++) {
                int u = (x1 == x0) ? 0 :
                    (int)(uv_coords.m_x_min * rbi->w +
                          (uv_coords.width() * rbi->w * (x - x0)) /
                          (x1 - x0));
                int v = (y1 == y0) ? 0 :
                    (int)(uv_coords.m_y_min * rbi->h +
                          (uv_coords.height() * rbi->h * (y - y0)) /
                          (y1 - y0));
                unsigned char *p;
                gameswf::rgba c;
                if (u < 0) u = 0;
                else if (u >= rbi->w) u = rbi->w - 1;
                if (v < 0) v = 0;
                else if (v >= rbi->h) v = rbi->h - 1;
                p = rbi->data + v * rbi->pitch + u * rbi->bpp;
                if (rbi->bpp >= 3)
                    c = gameswf::rgba(p[0], p[1], p[2],
                                      rbi->bpp >= 4 ? p[3] : color.m_a);
                else
                    c = gameswf::rgba(color.m_r, color.m_g, color.m_b, p[0]);
                draw_pixel_rgba(x, y, cx.transform(c));
            }
        }
    }
    virtual void set_antialiased(bool enable) { (void)enable; }
    virtual bool test_stencil_buffer(const gameswf::rect& bound, Uint8 pattern)
    {
        (void)bound; (void)pattern;
        mask_tests++;
        return true;
    }
    virtual void begin_submit_mask()
    {
        mask_begins++;
        if (maskbuf)
            rb->memset(maskbuf, 0, LCD_WIDTH * LCD_HEIGHT);
        mask_submitting = true;
        mask_active = false;
    }
    virtual void end_submit_mask()
    {
        mask_ends++;
        mask_submitting = false;
        mask_active = true;
    }
    virtual void disable_mask()
    {
        mask_disables++;
        mask_submitting = false;
        mask_active = false;
    }
    virtual bool is_visible(const gameswf::rect& bound)
    {
        return bound.m_x_max >= display_x0 && bound.m_y_max >= display_y0 &&
               bound.m_x_min < display_x1 && bound.m_y_min < display_y1;
    }
    virtual void open() {}
};

struct SwfInfo {
    char sig[4];
    int version;
    unsigned long declared_size;
    unsigned long actual_size;
    bool compressed;
    bool decompressed;
    int stage_w;
    int stage_h;
    int fps_x100;
    int frames;
};

struct TagStats {
    int total;
    int show_frame;
    int place;
    int remove;
    int shape;
    int sprite;
    int button;
    int text;
    int edit_text;
    int bitmap;
    int sound;
    int do_action;
    int do_init_action;
    int do_abc;
    int video;
    int max_depth;
};

struct FlashState {
    SwfInfo info;
    TagStats tags;
    char path[MAX_PATH];
    char status[80];
    unsigned char *raw;
    unsigned long raw_len;
    unsigned char *body;
    unsigned long body_len;
    unsigned char *fws;
    unsigned long fws_len;
    bool fws_file_ready;
    char fws_cache_path[MAX_PATH];
    size_t buffer_size;
    bool loaded;
    bool runtime_loaded;
    bool runtime_ready;
    void *cxx_buf;
    size_t cxx_size;
    void *cxx_heap;
    size_t cxx_heap_size;
    bool cxx_heap_ready;
    struct ipod_engine_memory engine_memory;
    struct ipod_engine_frame_clock frame_clock;
    int log_fd;
    int log_bytes;
    int runtime_frame;
    int rendered_frames;
    int shape_mesh_budget;
    int shape_mesh_budget_base;
    int shape_mesh_skipped;
    struct bitmap loading_bmp;
    unsigned char *loading_bmp_data;
    size_t loading_bmp_size;
    bool loading_bmp_tried;
    bool loading_bmp_loaded;
    int load_progress;
    long progress_checkpoint_tick;
    int progress_checkpoint_value;
    int load_stage_base;
    int load_stage_span;
    bool loading_overlay_full;
    bool prime_stickrpg;
    int startup_prerender_frames;
    long input_ignore_until;
    flash_input_profile input_profile;
#ifdef HAVE_WHEEL_POSITION
    bool wheel_touch_active;
    int wheel_touch_pos;
#endif
#ifdef SIMULATOR
    int autorun_frames;
    int autorun_click_count;
    int autorun_click_next;
    bool autorun_click_armed;
    int autorun_click_frame[8];
    int autorun_click_x[8];
    int autorun_click_y[8];
    bool debug_overlay;
    bool fast_autorun;
    int fast_draw_interval;
    int stickrpg_shortcut_frame;
    int stickrpg_person_frame_override;
    int right_down_frame;
    int right_up_frame;
    int autorun_scroll_frame;
    int autorun_scroll_direction;
    int autorun_select_frame;
    int autorun_call_save_frame;
#endif
    int cursor_x;
    int cursor_y;
    bool mouse_down;
    bool key_left_down;
    bool key_right_down;
    bool key_confirm_down;
    bool key_up_down;
    bool key_down_down;
    bool key_shift_down;
    int key_up_frames;
    int key_down_frames;
#ifdef HAVE_WHEEL_POSITION
    unsigned int stickrpg_wheel_dirs;
#endif
    bool stickrpg_name_prefilled;
    bool stickrpg_intro_hide_requested;
    bool stickrpg_post_create_forced;
    bool stickrpg_gameplay_shortcut_done;
    bool stickrpg_gameplay_shortcut_enabled;
    bool stickrpg_fast_load;
    bool stickrpg_realtime_clock_enabled;
    bool stickrpg_host_intro_done;
    bool stickrpg_host_intro_finalized;
    bool stickrpg_authored_ui_ready;
    bool stickrpg_authored_ui_presented;
    int stickrpg_authored_ui_signature;
    bool stickrpg_spawn_stabilized;
    bool stickrpg_scene_initialized;
    int stickrpg_previous_root_frame;
    bool gameswf_cache_checked;
    tu_file *shape_def_cache_in;
    tu_file *shape_def_cache_out;
    int click_frames;
    int page;
    int last_button;
    int log_count;
    int log_errors;
    int exec_tags;
    int exec_frame;
    int loader_hits;
    int loader_misses;
    int loader_last_tag;
    int loader_tag_count;
    int loader_last_pos;
    int loader_registers;
    long loader_start_tick;
    int loader_start_tag;
    int font_adds;
    int font_local_hits;
    int font_fallback_hits;
    int font_misses;
    int font_last_id;
    int mouse_trace_count;
    int button_trace_count;
    int action_trace_count;
    int action_verbose_left;
    int action_trace_button_id;
    int prime_root_start;
    int prime_root_load2;
    int prime_root_load;
    int prime_has_filmscreen;
    int prime_film_start;
    int prime_film_load2;
    int prime_film_load;
    int prime_film_frame;
    char last_log[96];
};

static FlashState g;
static RockboxRenderHandler *g_renderer;
static gameswf::player *g_player;
static gameswf::gc_ptr<gameswf::root> *g_root_ref;
static gameswf::gc_ptr<gameswf::sprite_instance> g_stickrpg_clock_ref;

extern "C" int flashplayer_bitmap_text_begin(float x, float y, float height,
                                               int red, int green, int blue,
                                               int alpha)
{
    FlashBitmapTextSpan *span;

    if (!g_bitmap_text_enabled || alpha < 24 ||
        g_bitmap_text_span_count >= FLASH_BITMAP_TEXT_MAX_SPANS)
        return 0;

    g_bitmap_text_current = g_bitmap_text_span_count++;
    span = &g_bitmap_text_spans[g_bitmap_text_current];
    span->stage_x = x;
    span->stage_baseline = y;
    span->stage_height = height;
    span->red = (unsigned char)red;
    span->green = (unsigned char)green;
    span->blue = (unsigned char)blue;
    span->alpha = (unsigned char)alpha;
    span->length = 0;
    span->text[0] = '\0';
    return 1;
}

extern "C" void flashplayer_bitmap_text_character(int code)
{
    FlashBitmapTextSpan *span;

    if (g_bitmap_text_current < 0 ||
        g_bitmap_text_current >= g_bitmap_text_span_count)
        return;

    span = &g_bitmap_text_spans[g_bitmap_text_current];
    if (span->length >= FLASH_BITMAP_TEXT_MAX_CHARS)
        return;

    /* Stick RPG is an AVM1-era English SWF. Rockbox's bitmap font is UTF-8,
     * but keeping this bridge to printable Latin-1 avoids malformed strings
     * when an old SWF contains an unmapped or control glyph. */
    if (code < 32 || code > 126)
        code = '?';
    span->text[span->length++] = (char)code;
    span->text[span->length] = '\0';
}

extern "C" void flashplayer_bitmap_text_end(void)
{
    if (g_bitmap_text_current >= 0 &&
        g_bitmap_text_spans[g_bitmap_text_current].length == 0)
        g_bitmap_text_span_count--;
    g_bitmap_text_current = -1;
}

class SilentSoundHandler : public gameswf::sound_handler
{
public:
    SilentSoundHandler() : next_id(1) {}

    virtual int create_sound(void *data, int data_bytes, int sample_count,
                             format_type format, int sample_rate, bool stereo)
    {
        (void)data;
        (void)data_bytes;
        (void)sample_count;
        (void)format;
        (void)sample_rate;
        (void)stereo;
        return next_id++;
    }

    virtual int load_sound(const char *url)
    {
        (void)url;
        return next_id++;
    }

    virtual void append_sound(int sound_handle, void *data, int data_bytes)
    {
        (void)sound_handle;
        (void)data;
        (void)data_bytes;
    }

    virtual void play_sound(gameswf::as_object *listener_obj,
                            int sound_handle, int loop_count)
    {
        (void)listener_obj;
        (void)sound_handle;
        (void)loop_count;
    }

    virtual void set_volume(int sound_handle, int volume)
    {
        (void)sound_handle;
        (void)volume;
    }

    virtual void set_max_volume(int vol)
    {
        (void)vol;
    }

    virtual void stop_sound(int sound_handle)
    {
        (void)sound_handle;
    }

    virtual void stop_all_sounds()
    {
    }

    virtual void delete_sound(int sound_handle)
    {
        (void)sound_handle;
    }

    virtual bool is_open()
    {
        return true;
    }

private:
    int next_id;
};

static SilentSoundHandler *g_sound_handler;

#define FLASH_MIN_CXX_HEAP     (512 * 1024)

#ifdef SIMULATOR
static void flash_sim_trace(const char *msg)
{
    fprintf(stderr, "flashplayer: %s\n", msg);
}
#else
static void flash_sim_trace(const char *msg)
{
    (void)msg;
}
#endif

static unsigned long read_le32(const unsigned char *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static size_t align16_size(size_t value)
{
    return (value + 15) & ~(size_t)15;
}

static int read_le16(const unsigned char *p)
{
    return p[0] | (p[1] << 8);
}

static gameswf::root *flash_root(void)
{
    return g_root_ref ? g_root_ref->get_ptr() : NULL;
}

/*
 * The official srpgcompletexgen.swf is the inner game movie.  Its 3-D intro
 * ends by calling _parent.doneIntro(), but that host callback is deliberately
 * absent from the SWF itself.  The original web/standalone container supplied
 * the callback.  Preserve that loading contract here instead of skipping or
 * rewriting the authored intro timeline.
 */
static void stickrpg_host_done_intro(const gameswf::fn_call& fn)
{
    gameswf::character *root_movie =
        gameswf::cast_to<gameswf::character>(fn.this_ptr);
    gameswf::as_value pregame_value;
    gameswf::as_value black_value;
    gameswf::as_value intro_value;
    gameswf::character *pregame = NULL;
    gameswf::sprite_instance *black = NULL;
    gameswf::character *intro = NULL;

    if (!root_movie && flash_root())
        root_movie = flash_root()->get_root_movie();
    if (root_movie && root_movie->get_member("pregameMC", &pregame_value))
        pregame = gameswf::cast_to<gameswf::character>(
            pregame_value.to_object());
    if (root_movie && root_movie->get_member("black", &black_value))
        black = gameswf::cast_to<gameswf::sprite_instance>(
            black_value.to_object());
    if (root_movie && root_movie->get_member("introscreen", &intro_value))
        intro = gameswf::cast_to<gameswf::character>(
            intro_value.to_object());

    if (pregame) {
        pregame->set_visible(false);
        pregame->set_play_state(gameswf::character::STOP);
    }
    if (intro)
        intro->set_visible(true);
    if (black) {
        /*
         * black has authored stop points at Flash frames 1, 10, and 30.
         * The container transition starts its second, 20-frame fade, which
         * uncovers the menu after the pre-game movie is dismissed.
         */
        black->goto_frame(10);
        black->set_play_state(gameswf::character::PLAY);
    }

    if (!g.stickrpg_host_intro_done) {
        g.stickrpg_host_intro_done = true;
        flash_logf("stickrpg host doneIntro pregame=%d black=%d intro=%d frame=%d",
                   pregame ? 1 : 0, black ? 1 : 0, intro ? 1 : 0,
                   g.rendered_frames);
    }
}

static void finalize_stickrpg_host_intro(void)
{
    gameswf::character *root_movie;
    gameswf::sprite_instance *root_sprite;
    gameswf::as_value pregame_value;
    gameswf::as_value black_value;
    gameswf::character *pregame = NULL;
    gameswf::sprite_instance *black = NULL;

    if (!g.stickrpg_host_intro_done || !flash_root())
        return;

    root_movie = flash_root()->get_root_movie();
    root_sprite = gameswf::cast_to<gameswf::sprite_instance>(root_movie);
    if (!root_movie || !root_sprite)
        return;

    if (root_movie->get_member("pregameMC", &pregame_value))
        pregame = gameswf::cast_to<gameswf::character>(
            pregame_value.to_object());
    if (root_movie->get_member("black", &black_value))
        black = gameswf::cast_to<gameswf::sprite_instance>(
            black_value.to_object());

    /* Keep the completed pre-game movie below the active fade even if one of
     * its authored visibility writes runs after doneIntro(). */
    if (pregame) {
        pregame->set_visible(false);
        pregame->set_play_state(gameswf::character::STOP);
    }

    if (g.stickrpg_host_intro_finalized)
        return;

    if (!black || black->get_current_frame() < 29)
        return;

    black->set_visible(false);
    black->set_play_state(gameswf::character::STOP);
    g.stickrpg_host_intro_finalized = true;
    flash_logf("stickrpg host intro finalized black_frame=%d pregame=%d frame=%d",
               black->get_current_frame(), pregame ? 1 : 0,
               g.rendered_frames);
}

struct rb_tu_file
{
    int fd;
    long size;
    long pos;
};

static int rb_tu_read(void *dst, int bytes, void *appdata)
{
    struct rb_tu_file *f = (struct rb_tu_file *)appdata;
    unsigned char *out = (unsigned char *)dst;
    int total = 0;

    if (!f || f->fd < 0 || bytes <= 0)
        return 0;

    while (total < bytes) {
        int got = rb->read(f->fd, out + total, bytes - total);
        if (got <= 0)
            break;
        total += got;
        f->pos += got;
    }

    return total;
}

static int rb_tu_write(const void *src, int bytes, void *appdata)
{
    struct rb_tu_file *f = (struct rb_tu_file *)appdata;
    const unsigned char *in = (const unsigned char *)src;
    int total = 0;

    if (!f || f->fd < 0 || bytes <= 0)
        return 0;

    while (total < bytes) {
        int wrote = rb->write(f->fd, in + total, bytes - total);
        if (wrote <= 0)
            break;
        total += wrote;
        f->pos += wrote;
        if (f->pos > f->size)
            f->size = f->pos;
    }

    return total;
}

static int rb_tu_seek(int pos, void *appdata)
{
    struct rb_tu_file *f = (struct rb_tu_file *)appdata;
    off_t result;

    if (!f || f->fd < 0 || pos < 0)
        return TU_FILE_SEEK_ERROR;

    result = rb->lseek(f->fd, pos, SEEK_SET);
    if (result < 0)
        return TU_FILE_SEEK_ERROR;

    f->pos = pos;
    if (pos == 0)
        flash_logf("rb_tu_seek pos=%d result=%ld tracked=%ld size=%ld",
                   pos, (long)result, f->pos, f->size);
    return 0;
}

static int rb_tu_seek_to_end(void *appdata)
{
    struct rb_tu_file *f = (struct rb_tu_file *)appdata;
    off_t result;

    if (!f || f->fd < 0)
        return TU_FILE_SEEK_ERROR;

    result = rb->lseek(f->fd, 0, SEEK_END);
    if (result < 0)
        return TU_FILE_SEEK_ERROR;

    f->size = rb->filesize(f->fd);
    if (f->size < 0)
        f->size = 0;
    f->pos = f->size;
    if (f->pos > f->size)
        f->size = f->pos;
    return 0;
}

static int rb_tu_tell(const void *appdata)
{
    const struct rb_tu_file *f = (const struct rb_tu_file *)appdata;

    return f ? f->pos : -1;
}

static bool rb_tu_eof(void *appdata)
{
    struct rb_tu_file *f = (struct rb_tu_file *)appdata;

    return !f || f->fd < 0 || f->pos >= f->size;
}

static int rb_tu_close(void *appdata)
{
    struct rb_tu_file *f = (struct rb_tu_file *)appdata;

    if (!f)
        return TU_FILE_CLOSE_ERROR;

    if (f->fd >= 0)
        rb->close(f->fd);
    delete f;
    return 0;
}

static tu_file *rb_tu_open(const char *path, int flags)
{
    struct rb_tu_file *f;
    int fd;

    fd = rb->open(path, flags, 0666);
    if (fd < 0)
        return NULL;

    f = new rb_tu_file;
    if (!f) {
        rb->close(fd);
        return NULL;
    }

    f->fd = fd;
    f->pos = 0;
    f->size = rb->filesize(fd);
    if (f->size < 0)
        f->size = 0;

    return new tu_file(f, rb_tu_read, rb_tu_write, rb_tu_seek,
                       rb_tu_seek_to_end, rb_tu_tell, rb_tu_eof,
                       rb_tu_close);
}

static bool flash_cache_path_for_url(const char *url_or_path, char *path,
                                     size_t path_size)
{
    const char *suffix = FLASH_CACHE_SUFFIX;
    size_t len;
    size_t suffix_len;

    if (!url_or_path || !path || path_size == 0)
        return false;

    len = rb->strlen(url_or_path);
    suffix_len = rb->strlen(suffix);
    if (len < suffix_len ||
        rb->strcmp(url_or_path + len - suffix_len, suffix) != 0)
        return false;

    if (rb->strstr(url_or_path, "srpgcompletexgen") ||
        rb->strstr(url_or_path, "stickrpg")) {
        rb->snprintf(path, path_size, "%s%s", g.path, suffix);
        return true;
    }

    return false;
}

static bool flash_progress_checkpoint(const char *status, int progress)
{
#ifndef SIMULATOR
    int fd;
    char line[192];
    int len;
    long now = *rb->current_tick;
    bool phase_advance = g.progress_checkpoint_value < 0 ||
        progress >= g.progress_checkpoint_value + 4 || progress >= 100;

    if (!phase_advance &&
        TIME_BEFORE(now, g.progress_checkpoint_tick + HZ / 2))
        return false;

    g.progress_checkpoint_tick = now;
    g.progress_checkpoint_value = progress;

    rb->mkdir(FLASH_DIR);
    fd = rb->open(FLASH_PROGRESS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return true;

    len = rb->snprintf(line, sizeof(line),
                       "tick=%ld progress=%d tag=%d type=%d pos=%d "
                       "frame=%d ready=%d status=%s\n",
                       *rb->current_tick, progress,
                       g.loader_tag_count, g.loader_last_tag,
                       g.loader_last_pos, g.runtime_frame,
                       g.runtime_ready ? 1 : 0,
                       status ? status : "");
    if (len > 0) {
        if (len > (int)sizeof(line))
            len = sizeof(line);
        rb->write(fd, line, len);
    }
    rb->close(fd);
    return true;
#else
    (void)status;
    (void)progress;
    return true;
#endif
}

static void flash_logf(const char *fmt, ...)
{
    char buf[192];
    char line[224];
    va_list ap;
    int len;

    if (g.log_fd < 0)
        return;

    va_start(ap, fmt);
    rb->vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    len = rb->snprintf(line, sizeof(line), "[%08ld] %s\n",
                       *rb->current_tick, buf);
    if (len <= 0)
        return;
    if (len > (int)sizeof(line))
        len = sizeof(line);

#ifndef SIMULATOR
    if (!g.runtime_ready && g.log_bytes > 3300)
        return;
#endif

    rb->write(g.log_fd, line, len);
    g.log_bytes += len;
}

static void flash_log_open(void)
{
    rb->mkdir(FLASH_DIR);
    g.log_fd = rb->open(FLASH_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    g.log_bytes = 0;
    if (g.log_fd >= 0)
        flash_logf("session start");
}

static void flash_log_close(void)
{
    if (g.log_fd >= 0) {
        flash_logf("session end");
        rb->close(g.log_fd);
        g.log_fd = -1;
    }
}

static void flash_log_callback(bool error, const char *message)
{
    const char *src = message ? message : "";
    char *dst = g.last_log;
    size_t i;

    g.log_count++;
    if (error) {
        g.log_errors++;
        /* Optional missing AVM1 display objects can repeat hundreds of times
         * per gameplay frame. Preserve the first diagnostics without doing
         * synchronous FAT writes forever in the render loop. */
        if (g.log_errors > 32)
            return;
    }

    if (!error &&
        !rb->strstr(src, "export") &&
        !rb->strstr(src, "attach") &&
        !rb->strstr(src, "target") &&
        !rb->strstr(src, "undefined") &&
        !rb->strstr(src, "failed") &&
        !rb->strstr(src, "can't"))
        return;

    for (i = 0; i + 1 < sizeof(g.last_log) && src[i]; i++) {
        char c = src[i];
        dst[i] = (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
    }
    dst[i] = '\0';
    flash_logf("gameswf%s: %s", error ? " error" : "", g.last_log);
}

static int read_bits(const unsigned char *buf, int bit, int count)
{
    int value = 0;
    int i;

    for (i = 0; i < count; i++, bit++) {
        int byte = bit >> 3;
        int shift = 7 - (bit & 7);
        value = (value << 1) | ((buf[byte] >> shift) & 1);
    }

    return value;
}

static int swf_rect_bytes(const unsigned char *buf, unsigned long len,
                          int *w, int *h)
{
    if (len < 1)
        return -1;

    int nbits = read_bits(buf, 0, 5);
    int bit = 5;
    int xmin;
    int xmax;
    int ymin;
    int ymax;

    if ((unsigned long)((5 + nbits * 4 + 7) >> 3) > len)
        return -1;

    xmin = read_bits(buf, bit, nbits);
    bit += nbits;
    xmax = read_bits(buf, bit, nbits);
    bit += nbits;
    ymin = read_bits(buf, bit, nbits);
    bit += nbits;
    ymax = read_bits(buf, bit, nbits);
    bit += nbits;

    *w = (xmax - xmin) / 20;
    *h = (ymax - ymin) / 20;
    return (bit + 7) >> 3;
}

static void count_tag(TagStats *s, int code, int depth)
{
    s->total++;
    if (depth > s->max_depth)
        s->max_depth = depth;

    switch (code) {
    case 1:
        s->show_frame++;
        break;
    case 4:
    case 26:
    case 70:
        s->place++;
        break;
    case 5:
    case 28:
        s->remove++;
        break;
    case 2:
    case 22:
    case 32:
    case 46:
    case 83:
        s->shape++;
        break;
    case 7:
    case 34:
        s->button++;
        break;
    case 11:
    case 33:
        s->text++;
        break;
    case 37:
        s->edit_text++;
        break;
    case 6:
    case 20:
    case 21:
    case 35:
    case 36:
    case 90:
        s->bitmap++;
        break;
    case 14:
    case 15:
    case 18:
    case 19:
    case 45:
    case 89:
        s->sound++;
        break;
    case 12:
        s->do_action++;
        break;
    case 39:
        s->sprite++;
        break;
    case 59:
        s->do_init_action++;
        break;
    case 60:
    case 61:
        s->video++;
        break;
    case 82:
        s->do_abc++;
        break;
    default:
        break;
    }
}

static void scan_tags(const unsigned char *data, unsigned long start,
                      unsigned long end, int depth, TagStats *stats)
{
    unsigned long pos = start;

    while (pos + 2 <= end) {
        int hdr = read_le16(data + pos);
        int code = hdr >> 6;
        unsigned long len = hdr & 0x3f;
        unsigned long body;

        pos += 2;
        if (len == 0x3f) {
            if (pos + 4 > end)
                break;
            len = read_le32(data + pos);
            pos += 4;
        }

        body = pos;
        if (body + len > end)
            break;

        count_tag(stats, code, depth);

        if (code == 39 && len >= 4 && depth < 16)
            scan_tags(data, body + 4, body + len, depth + 1, stats);

        pos = body + len;
        if (code == 0)
            break;
    }
}

static void reserve_loading_bitmap_buffer(void)
{
#if defined(HAVE_LCD_COLOR) && LCD_WIDTH == 320 && LCD_HEIGHT == 240
    size_t needed = BM_SIZE(LCD_WIDTH, LCD_HEIGHT, FORMAT_NATIVE, false);

    if (g.cxx_buf && g.cxx_size > needed + (4 * 1024 * 1024))
    {
        g.cxx_size -= needed;
        g.loading_bmp_data = (unsigned char *)g.cxx_buf + g.cxx_size;
        g.loading_bmp_size = needed;
        return;
    }

    if (g.raw && g.buffer_size > needed + (3 * 1024 * 1024))
    {
        g.buffer_size -= needed;
        g.loading_bmp_data = g.raw + g.buffer_size;
        g.loading_bmp_size = needed;
    }
#endif
}

static bool load_loading_bitmap(void)
{
#if defined(HAVE_LCD_COLOR) && LCD_WIDTH == 320 && LCD_HEIGHT == 240
    static const char * const paths[] = {
        FLASH_LOADING_BMP,
        FLASH_LOADING_BMP_FALLBACK,
    };

    if (g.loading_bmp_loaded)
        return true;

    if (g.loading_bmp_tried || !g.loading_bmp_data)
        return false;

    g.loading_bmp_tried = true;
    rb->memset(&g.loading_bmp, 0, sizeof(g.loading_bmp));
    g.loading_bmp.width = LCD_WIDTH;
    g.loading_bmp.height = LCD_HEIGHT;
    g.loading_bmp.format = FORMAT_NATIVE;
    g.loading_bmp.data = g.loading_bmp_data;

    for (int i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); i++)
    {
        int rc;

        if (!rb->file_exists(paths[i]))
            continue;

        rc = rb->read_bmp_file(paths[i], &g.loading_bmp,
                               g.loading_bmp_size, FORMAT_NATIVE, NULL);
        if (rc >= 0 && g.loading_bmp.width == LCD_WIDTH &&
            g.loading_bmp.height == LCD_HEIGHT)
        {
            g.loading_bmp_loaded = true;
            flash_logf("loading bitmap=%s", paths[i]);
            return true;
        }
    }
#endif

    return false;
}

static void show_load_status_text(const char *status)
{
    int bar_x = 30;
    int bar_y = 174;
    int bar_w = LCD_WIDTH - 60;
    int bar_h = 14;
    int fill_w;
    int progress = g.load_progress;

    if (progress < 0)
        progress = 0;
    if (progress > 100)
        progress = 100;
    fill_w = (bar_w - 2) * progress / 100;

    rb->snprintf(g.status, sizeof(g.status), "%s", status ? status : "");
    SET_BG(COL_BG);
    SET_FG(COL_BG);
    rb->lcd_clear_display();

    SET_FG(COL_FLASH);
    rb->lcd_fillrect(38, 42, 58, 58);
    SET_FG(COL_INK);
    rb->lcd_putsxy(58, 62, "f");
    rb->lcd_putsxy(110, 54, "Flash Player");
    SET_FG(COL_DIM);
    rb->lcd_putsxy(110, 74, "Stick RPG");

    SET_FG(COL_PANEL2);
    rb->lcd_drawrect(bar_x, bar_y, bar_w, bar_h);
    SET_FG(COL_BAR);
    rb->lcd_fillrect(bar_x + 1, bar_y + 1, fill_w, bar_h - 2);
    SET_FG(COL_INK);
    rb->lcd_putsxyf(bar_x, bar_y - 20, "Loading %d%%", progress);
    SET_FG(COL_DIM);
    rb->lcd_putsxy(30, 202, g.status);
    rb->lcd_update();
    rb->yield();
}

static void draw_loading_overlay(const char *status)
{
    int bar_x = 30;
    int bar_y = 174;
    int bar_w = LCD_WIDTH - 60;
    int bar_h = 14;
    int progress = g.load_progress;
    int fill_w;

    if (progress < 0)
        progress = 0;
    if (progress > 100)
        progress = 100;
    fill_w = (bar_w - 2) * progress / 100;

    SET_BG(COL_BG);
    SET_FG(COL_BG);
    rb->lcd_clear_display();

    SET_FG(COL_FLASH);
    rb->lcd_fillrect(38, 42, 58, 58);
    SET_FG(COL_INK);
    rb->lcd_putsxy(58, 62, "f");
    rb->lcd_putsxy(110, 54, "Flash Player");
    SET_FG(COL_DIM);
    rb->lcd_putsxy(110, 74, "Stick RPG");

    SET_FG(COL_PANEL2);
    rb->lcd_drawrect(bar_x, bar_y, bar_w, bar_h);
    SET_FG(COL_BAR);
    rb->lcd_fillrect(bar_x + 1, bar_y + 1, fill_w, bar_h - 2);
    SET_FG(COL_INK);
    rb->lcd_putsxyf(bar_x, bar_y - 20, "Loading %d%%", progress);
    SET_FG(COL_DIM);
    rb->lcd_putsxy(30, 202, status ? status : "");
}

static void show_load_status_progress(const char *status, int progress)
{
    rb->snprintf(g.status, sizeof(g.status), "%s", status);
    if (progress >= 0) {
        if (progress > 100)
            progress = 100;
        if (progress > g.load_progress)
            g.load_progress = progress;
    }
    if (!flash_progress_checkpoint(g.status, g.load_progress))
        return;

    if (load_loading_bitmap())
    {
        rb->lcd_set_viewport(NULL);
        g.loading_overlay_full = false;
        draw_loading_overlay(g.status);
        rb->lcd_update();
        rb->yield();
        return;
    }

    g.loading_overlay_full = true;
    show_load_status_text(status);
}

static void show_load_status(const char *status)
{
    show_load_status_progress(status, -1);
}

extern "C" void flashplayer_trace_tag(int tag_count, int tag_type, int stream_pos)
{
    g.loader_tag_count = tag_count;
    g.loader_last_tag = tag_type;
    g.loader_last_pos = stream_pos;
}

extern "C" void flashplayer_trace_parse_progress(const char *scope,
                                                 int tag_count, int tag_type,
                                                 int stream_pos)
{
    g.loader_tag_count = tag_count;
    g.loader_last_tag = tag_type;
    g.loader_last_pos = stream_pos;

    if (g.runtime_ready)
        return;

    if ((tag_count & 127) == 0 || tag_type == 0) {
        int progress = g.load_stage_base;
        if (g.fws_len > 0 && g.load_stage_span > 0)
            progress += (int)(((long long)stream_pos *
                               g.load_stage_span) / g.fws_len);
        if (progress > g.load_progress)
            show_load_status_progress("reading SWF data", progress);
    }

    if ((tag_count & 31) == 0 || tag_type == 0)
        rb->yield();
}

extern "C" void flashplayer_trace_parse_start(const char *scope,
                                              int tag_count, int tag_type,
                                              int stream_pos)
{
    g.loader_last_tag = tag_type;
    g.loader_last_pos = stream_pos;
    g.loader_start_tick = *rb->current_tick;
    g.loader_start_tag = tag_type;

    (void)scope;
    (void)tag_count;
}

extern "C" void flashplayer_trace_parse_end(const char *scope,
                                            int tag_count, int tag_type,
                                            int stream_pos)
{
    long now = *rb->current_tick;
    long elapsed = now - g.loader_start_tick;

    if (!g.runtime_ready && g.loader_start_tick != 0 && elapsed >= HZ / 2) {
        flash_logf("slow tag scope=%s count=%d type=%d pos=%d elapsed=%ld",
                   scope ? scope : "swf", tag_count, tag_type,
                   stream_pos, elapsed);
    }

    (void)now;
}

extern "C" int flashplayer_should_stop_movie_load(int loading_frame,
                                                  int tag_count,
                                                  int stream_pos)
{
    int stop_frame = 0;

#ifdef SIMULATOR
    const char *env = getenv("FLASHPLAYER_STOP_AFTER_FRAME");
    if (env)
        stop_frame = rb->atoi(env);
#endif

    if (stop_frame <= 0)
        return 0;

    if (!g.stickrpg_fast_load)
        return 0;

    if (loading_frame >= stop_frame) {
        flash_logf("early movie load stop frame=%d tag=%d pos=%d",
                   loading_frame, tag_count, stream_pos);
        return 1;
    }

    return 0;
}

extern "C" int flashplayer_should_skip_movie_tag(int loading_frame,
                                                 int tag_count,
                                                 int tag_type,
                                                 int stream_pos)
{
    (void)loading_frame;
    (void)tag_count;
    (void)tag_type;
    (void)stream_pos;
    return 0;
}

extern "C" void flashplayer_trace_shape(int phase, int character_id, int stream_pos)
{
    (void)phase;
    (void)character_id;
    (void)stream_pos;
}

extern "C" void flashplayer_trace_shape_record(int record_count, int flags,
                                               int stream_pos)
{
    (void)record_count;
    (void)flags;
    (void)stream_pos;
}

extern "C" void flashplayer_trace_rect(int phase, int nbits, int stream_pos)
{
    (void)phase;
    (void)nbits;
    (void)stream_pos;
}

extern "C" void flashplayer_trace_text(int phase, int stream_pos, int a, int b)
{
    (void)phase;
    (void)stream_pos;
    (void)a;
    (void)b;
}

static bool trace_intro_name(const char *name)
{
    return name && (rb->strcmp(name, "_root") == 0 ||
                    rb->strcmp(name, "introscreen") == 0 ||
                    rb->strcmp(name, "newgame") == 0 ||
                    rb->strcmp(name, "makechar") == 0 ||
                    rb->strcmp(name, "instructions") == 0 ||
                    rb->strcmp(name, "startbutton") == 0 ||
                    rb->strcmp(name, "loadButton") == 0 ||
                    rb->strcmp(name, "sticktitle") == 0 ||
                    rb->strcmp(name, "copyright") == 0 ||
                    rb->strcmp(name, "instructionsbutton") == 0 ||
                    rb->strcmp(name, "textname") == 0 ||
                    rb->strcmp(name, "pname") == 0 ||
                    rb->strcmp(name, "intl") == 0 ||
                    rb->strcmp(name, "str") == 0 ||
                    rb->strcmp(name, "cha") == 0 ||
                    rb->strcmp(name, "personColor") == 0 ||
                    rb->strcmp(name, "personColor2") == 0 ||
                    rb->strcmp(name, "black") == 0 ||
                    rb->strcmp(name, "filmscreen") == 0 ||
                    rb->strcmp(name, "_visible") == 0);
}

extern "C" void flashplayer_trace_visible_set(const char *name, int value)
{
    if (g.stickrpg_name_prefilled && name &&
        rb->strcmp(name, "introscreen") == 0 && value == 0)
        g.stickrpg_intro_hide_requested = true;

#ifdef SIMULATOR
    if (g.action_verbose_left > 0 && trace_intro_name(name)) {
        g.action_verbose_left--;
        flash_logf("visible set frame=%d name=%s value=%d",
                   g.rendered_frames, name, value);
    }
#else
    (void)name;
    (void)value;
#endif
}

extern "C" void flashplayer_trace_do_actions(const char *owner, int count)
{
    (void)owner;
    (void)count;
}

extern "C" void flashplayer_trace_member_lookup(const char *owner,
                                                const char *name, int found,
                                                int object_result)
{
    (void)owner;
    (void)name;
    (void)found;
    (void)object_result;
}

extern "C" void flashplayer_trace_variable_lookup(const char *name, int source,
                                                  int object_result)
{
    static int sharedobject_lookups;

    if (name && rb->strcmp(name, "SharedObject") == 0 &&
        sharedobject_lookups < 8) {
        sharedobject_lookups++;
        flash_logf("SharedObject lookup source=%d object=%d frame=%d",
                   source, object_result, g.rendered_frames);
    }
}

extern "C" void flashplayer_trace_display_object(const char *name, int depth,
                                                  int id, int visible)
{
    (void)name;
    (void)depth;
    (void)id;
    (void)visible;
}

extern "C" void flashplayer_trace_execute_tag(int frame)
{
    g.exec_tags++;
    g.exec_frame = frame;
}

extern "C" void flashplayer_trace_loader(int hit, int tag_type)
{
    int total;

    if (hit)
        g.loader_hits++;
    else
        g.loader_misses++;
    g.loader_last_tag = tag_type;

    total = g.loader_hits + g.loader_misses;
    if (!g.runtime_ready && total > 0 && (total & 2047) == 0) {
        rb->snprintf(g.status, sizeof(g.status), "gameswf: tags %d", total);
        rb->yield();
    }
}

extern "C" void flashplayer_trace_loader_register(int tag_type)
{
    g.loader_registers++;
    g.loader_last_tag = tag_type;
}

extern "C" void flashplayer_trace_font(int op, int font_id)
{
    g.font_last_id = font_id;
    if (op == 0)
        g.font_adds++;
    else if (op == 1)
        g.font_local_hits++;
    else if (op == 2)
        g.font_fallback_hits++;
    else
        g.font_misses++;
}

extern "C" void flashplayer_trace_mouse_event(int event_id, int topmost,
                                               int active, int last_button,
                                               int current_button, int inside,
                                               int x, int y)
{
    (void)event_id;
    (void)topmost;
    (void)active;
    (void)last_button;
    (void)current_button;
    (void)inside;
    (void)x;
    (void)y;
}

extern "C" void flashplayer_trace_button_action(int button_id, int event_id,
                                                 int conditions, int mask,
                                                 int matched, int action_count)
{
    if (matched && action_count > 0 && g.button_trace_count < 32) {
        g.button_trace_count++;
        flash_logf("button action id=%d event=%d cond=%d mask=%d actions=%d frame=%d",
                   button_id, event_id, conditions, mask, action_count,
                   g.rendered_frames);
    }
    if (matched && action_count > 0 &&
        g.input_profile == FLASH_INPUT_PROFILE_STICKRPG) {
        if (button_id == 101) {
            /* START opens the duration panel; default to MEDIUM rather than
             * leaving the cursor on UNLIMITED (whose 9999-day sentinel looks
             * like corrupt HUD state). */
            g.cursor_x = LCD_WIDTH / 2;
            g.cursor_y = 112;
        } else if (button_id == 194) {
            g.cursor_y = 202; /* MEDIUM selected: move to DONE. */
        } else if (button_id == 187) {
            g.cursor_y = 206; /* Character panel: Create Character. */
        }
    }
}

extern "C" void flashplayer_trace_action(int phase, int opcode, int a, int b,
                                          const char *text)
{
    (void)phase;
    (void)opcode;
    (void)a;
    (void)b;
    (void)text;
}

extern "C" void flashplayer_trace_action_bytes(const unsigned char *bytes, int len)
{
    (void)bytes;
    (void)len;
}

extern "C" void flashplayer_trace_avm1_function(int phase, const char *name,
                                                 int pc, int aux)
{
    if (!name)
        return;

    if (rb->strcmp(name, "InitMovie") != 0 &&
        rb->strcmp(name, "InitScene") != 0 &&
        rb->strcmp(name, "RenderScene") != 0 &&
        rb->strcmp(name, "eraseScene") != 0)
        return;

    flash_logf("avm1 function phase=%d name=%s pc=%d aux=%d",
               phase, name, pc, aux);
}

extern "C" void flashplayer_trace_stickrpg_scene(const char *op,
                                                  const char *name,
                                                  const char *value,
                                                  int aux_a, int aux_b)
{
    if (!op || !name || !value)
        return;

    (void)op;
    (void)name;
    (void)value;
    (void)aux_a;
    (void)aux_b;
}

extern "C" void flashplayer_trace_movie_state(const char *name, int value,
                                               int aux_a, int aux_b)
{
    (void)value;
    (void)aux_a;
    (void)aux_b;

    if (g.runtime_ready)
        return;

    if (!name)
        return;

    /* These are high-frequency diagnostic probes from bring-up.  Writing
     * thousands of them to FAT dominated SWF startup time on real targets. */
    if (rb->strncmp(name, "inst_", 5) == 0 ||
        rb->strncmp(name, "create_root_", 12) == 0 ||
        rb->strncmp(name, "exec_", 5) == 0 ||
        rb->strncmp(name, "place_", 6) == 0 ||
        rb->strncmp(name, "font_", 5) == 0 ||
        rb->strncmp(name, "gsc_", 4) == 0 ||
        rb->strncmp(name, "read_loop_", 10) == 0 ||
        rb->strncmp(name, "stream_close_", 13) == 0 ||
        rb->strncmp(name, "action_read_", 12) == 0 ||
        rb->strncmp(name, "shape_def_cache", 15) == 0)
        return;

    if (rb->strncmp(name, "shape_def_cache_hit", 19) == 0 &&
        value != 24 && (value & 63) != 0) {
        return;
    } else if (rb->strncmp(name, "inst_", 5) == 0 ||
               rb->strncmp(name, "create_root_", 12) == 0 ||
               rb->strncmp(name, "exec_frame_", 11) == 0 ||
               rb->strncmp(name, "exec_post_", 10) == 0 ||
               rb->strncmp(name, "exec_sound_", 11) == 0 ||
               rb->strncmp(name, "exec_script_", 12) == 0 ||
               rb->strncmp(name, "exec_tag_", 9) == 0 ||
               rb->strncmp(name, "place_", 6) == 0 ||
               rb->strncmp(name, "font_", 5) == 0 ||
               rb->strncmp(name, "gsc_", 4) == 0 ||
               rb->strncmp(name, "read_loop_", 10) == 0 ||
               rb->strncmp(name, "stream_close_", 13) == 0 ||
               rb->strncmp(name, "action_read_", 12) == 0) {
        flash_logf("%s value=%d aux=%d/%d heap_free=%luK",
                   name, value, aux_a, aux_b,
                   (unsigned long)(plugin_cxx_available() / 1024));
#ifndef SIMULATOR
        if (rb->strncmp(name, "exec_tag_begin", 14) == 0) {
            rb->snprintf(g.status, sizeof(g.status), "frame %d tag %d/%d",
                         value, aux_a, aux_b);
            flash_progress_checkpoint(g.status, 54);
        } else if (rb->strncmp(name, "exec_tag_end", 12) == 0) {
            rb->snprintf(g.status, sizeof(g.status),
                         "frame %d tag %d/%d done", value, aux_a, aux_b);
            flash_progress_checkpoint(g.status, 54);
        } else if (rb->strncmp(name, "exec_frame_", 11) == 0 ||
                   rb->strncmp(name, "exec_post_", 10) == 0 ||
                   rb->strncmp(name, "exec_sound_", 11) == 0 ||
                   rb->strncmp(name, "exec_script_", 12) == 0) {
            rb->snprintf(g.status, sizeof(g.status), "%s f%d a%d b%d",
                         name, value, aux_a, aux_b);
            flash_progress_checkpoint(g.status, 55);
        } else if (rb->strncmp(name, "place_begin", 11) == 0) {
            rb->snprintf(g.status, sizeof(g.status), "place c%d d%d",
                         value, aux_a);
            flash_progress_checkpoint(g.status, 55);
        } else if (rb->strncmp(name, "action_read_", 12) == 0) {
            rb->snprintf(g.status, sizeof(g.status), "action %d/%d",
                         value, aux_b > 0 ? value + aux_b : value);
            flash_progress_checkpoint(g.status, 54);
        } else if (rb->strncmp(name, "gsc_", 4) == 0) {
            rb->snprintf(g.status, sizeof(g.status), "%s v%d a%d b%d",
                         name, value, aux_a, aux_b);
            flash_progress_checkpoint(g.status, 64);
        } else if (rb->strncmp(name, "inst_", 5) == 0 ||
                   rb->strncmp(name, "create_root_", 12) == 0) {
            rb->snprintf(g.status, sizeof(g.status), "%s v%d a%d b%d",
                         name, value, aux_a, aux_b);
            flash_progress_checkpoint(g.status, g.load_progress);
        } else {
            flash_progress_checkpoint(name, g.load_progress);
        }
#endif
    } else if (rb->strncmp(name, "shape_def_cache", 15) == 0) {
        flash_logf("%s value=%d aux=%d/%d heap_free=%luK",
                   name, value, aux_a, aux_b,
                   (unsigned long)(plugin_cxx_available() / 1024));
    } else if (rb->strcmp(name, "create_movie_open") == 0)
        show_load_status("gameswf: opened memory SWF");
    else if (rb->strcmp(name, "create_movie_read_begin") == 0) {
        flash_logf("gameswf milestone: read begin heap_free=%luK",
                   (unsigned long)(plugin_cxx_available() / 1024));
        show_load_status("gameswf: reading tags");
    } else if (rb->strcmp(name, "create_movie_read_end") == 0) {
        flash_logf("gameswf milestone: read end tags=%d pos=%d heap_free=%luK",
                   g.loader_tag_count, g.loader_last_pos,
                   (unsigned long)(plugin_cxx_available() / 1024));
        show_load_status("gameswf: tags read");
    } else if (rb->strcmp(name, "load_file_def_ready") == 0) {
        flash_logf("gameswf milestone: definition ready heap_free=%luK",
                   (unsigned long)(plugin_cxx_available() / 1024));
        show_load_status("gameswf: definition ready");
    } else if (rb->strcmp(name, "load_file_instance_ready") == 0) {
        flash_logf("gameswf milestone: instance ready heap_free=%luK",
                   (unsigned long)(plugin_cxx_available() / 1024));
        show_load_status("gameswf: instance ready");
    }
}

extern "C" void flashplayer_trace_shape_mesh(int id, int paths, int cached,
                                             int error_x100)
{
    (void)id;
    (void)paths;
    (void)cached;
    (void)error_x100;
}

extern "C" int flashplayer_consume_shape_mesh_budget(int id, int paths,
                                                     int cached,
                                                     int error_x100)
{
    int cost = 1 + paths / 20;

    if (cached > 0)
        return 1;

    if (g.shape_mesh_budget < cost) {
        g.shape_mesh_skipped++;
        g.shape_mesh_budget = 0;
        (void)id;
        (void)error_x100;
        return 0;
    }

    g.shape_mesh_budget -= cost;
    return 1;
}

extern "C" const char *flashplayer_get_movie_url(void)
{
    return STICK_RPG_URL;
}

static bool flashplayer_sharedobject_path(const char *name, char *path,
                                          size_t path_size)
{
    char safe[64];
    size_t si = 0;
    const char *p = name ? name : "";

    while (*p && si + 1 < sizeof(safe)) {
        char c = *p++;

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')
            safe[si++] = c;
        else if (c == '.' || c == '/' || c == '\\' || c == ':' || c == ' ')
            safe[si++] = '_';
    }
    safe[si] = '\0';

    if (si == 0)
        rb->strlcpy(safe, "default", sizeof(safe));

    rb->snprintf(path, path_size, "%s/%s.rbso", FLASH_SHARED_DIR, safe);
    return true;
}

extern "C" int flashplayer_sharedobject_read(const char *name, char *buffer,
                                             int buffer_size)
{
    char path[MAX_PATH];
    char backup[MAX_PATH];
    int fd;
    int got;

    if (!buffer || buffer_size <= 0)
        return -1;

    flashplayer_sharedobject_path(name, path, sizeof(path));
    fd = rb->open(path, O_RDONLY);
    if (fd < 0) {
        rb->snprintf(backup, sizeof(backup), "%s.bak", path);
        fd = rb->open(backup, O_RDONLY);
        if (fd < 0)
            return -1;
    }

    got = rb->read(fd, buffer, buffer_size);
    rb->close(fd);

    return got;
}

extern "C" int flashplayer_sharedobject_write(const char *name,
                                              const char *buffer,
                                              int buffer_size)
{
    char path[MAX_PATH];
    char temporary[MAX_PATH];
    char backup[MAX_PATH];
    int fd;
    int wrote;
    bool had_previous;

    if (!buffer || buffer_size < 0)
        return -1;

    rb->mkdir(FLASH_DIR);
    rb->mkdir(FLASH_SHARED_DIR);
    flashplayer_sharedobject_path(name, path, sizeof(path));
    rb->snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    rb->snprintf(backup, sizeof(backup), "%s.bak", path);
    rb->remove(temporary);
    fd = rb->open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return -1;

    wrote = rb->write(fd, buffer, buffer_size);
    rb->close(fd);
    if (wrote != buffer_size) {
        rb->remove(temporary);
        return wrote;
    }

    rb->remove(backup);
    had_previous = rb->rename(path, backup) >= 0;
    if (rb->rename(temporary, path) < 0) {
        if (had_previous)
            rb->rename(backup, path);
        rb->remove(temporary);
        return -1;
    }
    rb->remove(backup);
    return wrote;
}

extern "C" void flashplayer_trace_sharedobject(const char *op,
                                               const char *name, int a, int b)
{
    flash_logf("sharedobject %s name=%s a=%d b=%d",
               op ? op : "?", name ? name : "", a, b);
}

extern "C" void *flashplayer_tu_realloc(void *ptr, size_t new_size,
                                        size_t old_size)
{
    void *new_ptr;
    size_t copy_size;

    if (new_size == 0) {
        operator delete(ptr);
        return NULL;
    }

    new_ptr = operator new(new_size);
    if (!new_ptr)
        return NULL;

    if (ptr) {
        copy_size = old_size < new_size ? old_size : new_size;
        rb->memcpy(new_ptr, ptr, copy_size);
        operator delete(ptr);
    }

    return new_ptr;
}

static bool read_file_into_buffer(const char *path, unsigned char *buffer,
                                  size_t buffer_size, unsigned long *out_len)
{
    int fd = rb->open(path, O_RDONLY);
    long size;
    long got;

    if (fd < 0) {
        rb->snprintf(g.status, sizeof(g.status), "open failed: %s", path);
        return false;
    }

    size = rb->filesize(fd);
    if (size <= 8 || (size_t)size > buffer_size) {
        rb->close(fd);
        rb->snprintf(g.status, sizeof(g.status), "file too large: %ldK", size / 1024);
        return false;
    }

    got = rb->read(fd, buffer, size);
    rb->close(fd);

    if (got != size) {
        rb->snprintf(g.status, sizeof(g.status), "short read");
        return false;
    }

    *out_len = (unsigned long)size;
    return true;
}

static bool read_swf_header(const char *path, unsigned char *header,
                            long *out_size)
{
    int fd = rb->open(path, O_RDONLY);
    long size;
    long got;

    if (fd < 0) {
        rb->snprintf(g.status, sizeof(g.status), "open failed: %s", path);
        return false;
    }

    size = rb->filesize(fd);
    if (size <= 8) {
        rb->close(fd);
        rb->snprintf(g.status, sizeof(g.status), "bad SWF file");
        return false;
    }

    got = rb->read(fd, header, 8);
    rb->close(fd);
    if (got != 8) {
        rb->snprintf(g.status, sizeof(g.status), "short SWF header");
        return false;
    }

    *out_size = size;
    return true;
}

static bool write_file_from_buffer(const char *path, const unsigned char *buffer,
                                   size_t buffer_size)
{
    int fd;
    long wrote;

    if (!path || !buffer || buffer_size == 0)
        return false;

    fd = rb->open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;

    wrote = rb->write(fd, buffer, buffer_size);
    rb->close(fd);

    return wrote == (long)buffer_size;
}

static bool fws_cache_path_for_swf(const char *path, char *out,
                                   size_t out_size)
{
    if (!path || !out || out_size == 0)
        return false;

    rb->snprintf(out, out_size, "%s%s", path, FLASH_FWS_CACHE_SUFFIX);
    return true;
}

static bool validate_fws_cache(const char *path, unsigned long declared_size,
                               int version)
{
    unsigned char header[8];
    long size;

    if (!read_swf_header(path, header, &size))
        return false;

    if ((unsigned long)size != declared_size)
        return false;

    if (header[0] != 'F' || header[1] != 'W' || header[2] != 'S')
        return false;

    if (header[3] != (unsigned char)version)
        return false;

    return read_le32(header + 4) == declared_size;
}

static flash_input_profile detect_input_profile(const char *path)
{
    if (!path)
        return FLASH_INPUT_PROFILE_DEFAULT;

    if (rb->strstr(path, "stickrpg") != NULL)
        return FLASH_INPUT_PROFILE_STICKRPG;

    if (rb->strstr(path, "antcity") != NULL ||
        rb->strstr(path, "Burn_their_ass") != NULL)
        return FLASH_INPUT_PROFILE_ANTCITY;

    return FLASH_INPUT_PROFILE_DEFAULT;
}

static bool load_swf(const char *path)
{
    unsigned char *src;
    unsigned long src_len;
    unsigned char *audio;
    unsigned char *inflate_out;
    unsigned long inflate_room;
    unsigned int out_len;
    unsigned char header[8];
    long file_size;
    int rect_len;
    if (!path || !path[0])
        path = DEFAULT_SWF;

    rb->memset(&g.info, 0, sizeof(g.info));
    rb->memset(&g.tags, 0, sizeof(g.tags));
    g.loaded = false;
    g.runtime_ready = false;
    g.body = NULL;
    g.body_len = 0;
    g.fws = NULL;
    g.fws_len = 0;
    g.fws_file_ready = false;
    g.fws_cache_path[0] = '\0';
    g.runtime_loaded = false;
    g.runtime_frame = -1;
    g.input_profile = detect_input_profile(path);
    g_bitmap_text_enabled = g.input_profile == FLASH_INPUT_PROFILE_STICKRPG;
    g_bitmap_text_span_count = 0;
    g_bitmap_text_current = -1;
    g.rendered_frames = 0;
    g.shape_mesh_budget = 0;
    g.shape_mesh_skipped = 0;
    g.cursor_x = LCD_WIDTH / 2;
    /* Stick RPG Complete's real START hit region is centred at LCD y=150
     * after its 550x400 stage is fitted to the 6G screen.  Starting there
     * makes the first centre-button press useful while retaining free cursor
     * movement for the rest of the mouse-driven UI. */
    g.cursor_y = g.input_profile == FLASH_INPUT_PROFILE_STICKRPG
        ? 150 : LCD_HEIGHT / 2;
    g.mouse_down = false;
    g.key_left_down = false;
    g.key_right_down = false;
    g.key_confirm_down = false;
    g.key_up_down = false;
    g.key_down_down = false;
    g.key_shift_down = false;
    g.key_up_frames = 0;
    g.key_down_frames = 0;
    g.stickrpg_name_prefilled = false;
    g.stickrpg_intro_hide_requested = false;
    g.stickrpg_post_create_forced = false;
    g.stickrpg_gameplay_shortcut_done = false;
    g.stickrpg_scene_initialized = false;
    g.stickrpg_previous_root_frame = -1;
    g.gameswf_cache_checked = false;
    g.shape_def_cache_in = NULL;
    g.shape_def_cache_out = NULL;
    g.stickrpg_fast_load = g.input_profile == FLASH_INPUT_PROFILE_STICKRPG;
    /*
     * Fast loading selects the validated FWS/GSC/GSD cache path.  It must not
     * alter the authored movie.  The simulator already defaulted these two
     * diagnostic shortcuts to false when their environment variables were
     * absent, while hardware kept them enabled because getenv() is simulator
     * only.  That made the iPod remove the original intro layers and arm a
     * forced gameplay jump before the real menu could appear.
     */
    g.prime_stickrpg = false;
    g.stickrpg_gameplay_shortcut_enabled = false;
    g.startup_prerender_frames = g.stickrpg_fast_load ?
        FLASH_STARTUP_PRERENDER_FRAMES : 0;
#ifdef SIMULATOR
    {
        const char *prime = getenv("FLASHPLAYER_PRIME_STICKRPG");
        const char *shortcut = getenv("FLASHPLAYER_STICKRPG_SHORTCUT");
        if (prime && prime[0])
            g.prime_stickrpg = rb->atoi(prime) != 0;
        if (shortcut && shortcut[0])
            g.stickrpg_gameplay_shortcut_enabled =
                rb->atoi(shortcut) != 0;
    }
    g.fast_draw_interval = 1;
#endif
    flash_logf("stickrpg policy profile=%d fast=%d prime=%d "
               "shortcut=%d prerender=%d",
               (int)g.input_profile, g.stickrpg_fast_load ? 1 : 0,
               g.prime_stickrpg ? 1 : 0,
               g.stickrpg_gameplay_shortcut_enabled ? 1 : 0,
               g.startup_prerender_frames);
    g.click_frames = 0;
    g.exec_tags = 0;
    g.exec_frame = 0;
    g.loader_hits = 0;
    g.loader_misses = 0;
    g.loader_last_tag = 0;
    g.loader_tag_count = 0;
    g.loader_last_pos = 0;
    g.loader_registers = 0;
    g.loader_start_tick = 0;
    g.loader_start_tag = 0;
    g.font_adds = 0;
    g.font_local_hits = 0;
    g.font_fallback_hits = 0;
    g.font_misses = 0;
    g.font_last_id = 0;
    g.mouse_trace_count = 0;
    g.button_trace_count = 0;
    g.action_trace_count = 0;
    g.action_verbose_left = 0;
    g.action_trace_button_id = -1;
    g_stickrpg_clock_ref = NULL;
    rb->strlcpy(g.path, path, sizeof(g.path));
    flash_logf("load path=%s plugin_buf=%luK audio_buf=%luK",
               g.path, (unsigned long)(g.buffer_size / 1024),
               (unsigned long)(g.cxx_size / 1024));

    g.load_progress = 1;
    g.load_stage_base = 1;
    g.load_stage_span = 1;
    show_load_status_progress("opening SWF", 2);
    if (!read_swf_header(path, header, &file_size))
        return false;

    show_load_status_progress("reading SWF header", 5);
    g.raw_len = (unsigned long)file_size;

    if ((header[0] != 'F' && header[0] != 'C') || header[1] != 'W' ||
        header[2] != 'S') {
        rb->snprintf(g.status, sizeof(g.status), "not an SWF");
        return false;
    }

    g.info.sig[0] = header[0];
    g.info.sig[1] = header[1];
    g.info.sig[2] = header[2];
    g.info.sig[3] = '\0';
    g.info.version = header[3];
    g.info.declared_size = read_le32(header + 4);
    g.info.actual_size = g.raw_len;
    g.info.compressed = header[0] == 'C';
    flash_logf("header sig=%s version=%d raw=%lu declared=%lu compressed=%d",
               g.info.sig, g.info.version, g.info.actual_size,
               g.info.declared_size, g.info.compressed ? 1 : 0);

    if (g.info.declared_size <= 8) {
        rb->snprintf(g.status, sizeof(g.status), "bad SWF size");
        return false;
    }

    audio = (unsigned char *)g.cxx_buf;
    if (g.info.compressed) {
        size_t fws_used = align16_size(g.info.declared_size);
        bool have_fws_cache = false;

        if (audio && g.cxx_size > fws_used + g.raw_len + FLASH_MIN_CXX_HEAP) {
            src = audio + fws_used;
            g.fws = audio;
            inflate_out = audio + 8;
            inflate_room = (unsigned long)(g.cxx_size - 8);
        } else {
            size_t raw_used = align16_size(g.raw_len);
            if (raw_used >= g.buffer_size) {
                rb->snprintf(g.status, sizeof(g.status),
                             "file too large: %luK", g.raw_len / 1024);
                return false;
            }
            src = g.raw;
            inflate_out = g.raw + raw_used;
            inflate_room = (unsigned long)(g.buffer_size - (inflate_out - g.raw));
            g.fws = inflate_out - 8;
        }

        if (fws_cache_path_for_swf(path, g.fws_cache_path,
                                   sizeof(g.fws_cache_path)) &&
            validate_fws_cache(g.fws_cache_path, g.info.declared_size,
                               g.info.version) &&
            read_file_into_buffer(g.fws_cache_path, g.fws, fws_used,
                                  &src_len) &&
            src_len == g.info.declared_size) {
            have_fws_cache = true;
            g.fws_len = src_len;
            g.body = g.fws + 8;
            g.body_len = g.fws_len - 8;
            g.info.decompressed = true;
            g.fws_file_ready = true;
            flash_logf("using FWS cache %s size=%lu",
                       g.fws_cache_path, g.fws_len);
        }

        if (!have_fws_cache) {
            show_load_status_progress("reading compressed SWF", 8);
            if (!read_file_into_buffer(path, src,
                                       src == g.raw ? g.buffer_size :
                                       (size_t)(g.cxx_size - (src - audio)),
                                       &src_len))
                return false;
            g.raw_len = src_len;
            out_len = g.info.declared_size - 8;

            show_load_status_progress("inflating CWS", 18);
            if (inflate_room < out_len) {
                rb->snprintf(g.status, sizeof(g.status), "need %luK FWS buffer",
                             g.info.declared_size / 1024);
                return false;
            }

            {
                unsigned int expected_out = g.info.declared_size - 8;
                int inflate_err = tinf_zlib_uncompress(inflate_out, &out_len,
                                                       src + 8,
                                                       g.raw_len - 8);
                flash_logf("CWS inflate result err=%d out=%u expected=%u in=%lu",
                           inflate_err, out_len, expected_out, g.raw_len - 8);
                if (inflate_err != TINF_OK || out_len != expected_out) {
                    rb->snprintf(g.status, sizeof(g.status),
                                 "CWS inflate short %u/%u",
                                 out_len, expected_out);
                    return false;
                }
            }

            g.body = inflate_out;
            g.body_len = out_len;
            g.fws[0] = 'F';
            g.fws[1] = 'W';
            g.fws[2] = 'S';
            g.fws[3] = g.info.version;
            g.fws[4] = header[4];
            g.fws[5] = header[5];
            g.fws[6] = header[6];
            g.fws[7] = header[7];
            g.fws_len = g.info.declared_size;
            g.info.decompressed = true;

            if (g.fws_cache_path[0] &&
                write_file_from_buffer(g.fws_cache_path, g.fws, g.fws_len)) {
                g.fws_file_ready = true;
                flash_logf("wrote FWS cache %s size=%lu",
                           g.fws_cache_path, g.fws_len);
            }
        }
    } else {
        if ((size_t)file_size <= g.buffer_size) {
            src = g.raw;
        } else if (audio && g.cxx_size > (size_t)file_size + FLASH_MIN_CXX_HEAP) {
            src = audio;
        } else {
            rb->snprintf(g.status, sizeof(g.status), "need %luK FWS buffer",
                         (unsigned long)file_size / 1024);
            return false;
        }
        show_load_status_progress("reading SWF", 8);
        if (!read_file_into_buffer(path, src,
                                   src == g.raw ? g.buffer_size : g.cxx_size,
                                   &src_len))
            return false;
        g.raw_len = src_len;
        g.body = src + 8;
        g.body_len = g.raw_len - 8;
        g.fws = src;
        g.fws_len = g.raw_len;
        g.info.decompressed = true;
    }

    show_load_status_progress("reading SWF metadata", 30);
    rect_len = swf_rect_bytes(g.body, g.body_len, &g.info.stage_w,
                              &g.info.stage_h);
    if (rect_len < 0 || (unsigned long)rect_len + 4 > g.body_len) {
        rb->snprintf(g.status, sizeof(g.status), "bad SWF RECT");
        return false;
    }

    g.info.fps_x100 = read_le16(g.body + rect_len) * 100 / 256;
    g.info.frames = read_le16(g.body + rect_len + 2);
    ipod_engine_frame_clock_reset(&g.frame_clock, g.info.fps_x100);

#ifdef SIMULATOR
    scan_tags(g.body, rect_len + 4, g.body_len, 0, &g.tags);
#endif
    flash_logf("parsed stage=%dx%d fps=%d.%02d frames=%d tags=%d",
               g.info.stage_w, g.info.stage_h, g.info.fps_x100 / 100,
               g.info.fps_x100 % 100, g.info.frames, g.tags.total);
#ifdef SIMULATOR
    flash_logf("tags show=%d place=%d remove=%d shape=%d sprite=%d button=%d",
               g.tags.show_frame, g.tags.place, g.tags.remove,
               g.tags.shape, g.tags.sprite, g.tags.button);
    flash_logf("tags text=%d edit=%d bitmap=%d sound=%d action=%d init=%d abc=%d video=%d depth=%d",
               g.tags.text, g.tags.edit_text, g.tags.bitmap, g.tags.sound,
               g.tags.do_action, g.tags.do_init_action, g.tags.do_abc,
               g.tags.video, g.tags.max_depth);
#endif
    show_load_status_progress("SWF parsed: AVM1 bring-up", 34);
    rb->snprintf(g.status, sizeof(g.status), "SWF parsed: AVM1 bring-up");
    g.loaded = true;
    return true;
}

static tu_file *flash_file_opener(const char *url_or_path)
{
    char cache_path[MAX_PATH];
    tu_file *cache_file;

    if (flash_cache_path_for_url(url_or_path, cache_path,
                                 sizeof(cache_path))) {
        cache_file = rb_tu_open(cache_path, O_RDONLY);
        if (cache_file) {
            flash_logf("gameswf cache open %s", cache_path);
            return cache_file;
        }
        flash_logf("gameswf cache missing %s", cache_path);
        return NULL;
    }

    if (!g.fws || g.fws_len < 8)
        return NULL;

    flash_logf("gameswf FWS memory open size=%lu file_ready=%d",
               g.fws_len, g.fws_file_ready ? 1 : 0);
    return new tu_file(tu_file::memory_buffer, (int)g.fws_len, g.fws);
}

static void force_stickrpg_post_create_state(void);
static void prime_stickrpg_post_advance(void);
static void prefill_stickrpg_character_name(void);
static void flash_key_notify(gameswf::key::code key, bool down);

static void load_gameswf_cache_after_instance(void)
{
    char path[MAX_PATH];
    tu_file *in;
    gameswf::movie_definition *movie_def;

    if (g.gameswf_cache_checked)
        return;
    g.gameswf_cache_checked = true;

    if (!flash_root())
        return;

    movie_def = flash_root()->get_movie_definition();
    if (!movie_def)
        return;

    rb->snprintf(path, sizeof(path), "%s%s", g.path, FLASH_CACHE_SUFFIX);
    in = rb_tu_open(path, O_RDONLY);
    if (!in) {
        flash_logf("gameswf delayed cache missing %s", path);
        return;
    }

    show_load_status_progress("loading gameplay cache", 76);
    flash_logf("gameswf delayed cache open %s", path);
    movie_def->input_cached_data(in);
    show_load_status_progress("gameplay cache ready", 84);
    flash_logf("gameswf delayed cache loaded pos=%d err=%d heap_free=%luK",
               in->get_position(), in->get_error(),
               (unsigned long)(plugin_cxx_available() / 1024));
    delete in;
}

#ifdef SIMULATOR
static void write_gameswf_cache_if_requested(void)
{
    const char *write_cache = getenv("FLASHPLAYER_WRITE_GSC");
    const char *precompute = getenv("FLASHPLAYER_PRECOMPUTE_GSC");
    const char *warm_gameplay = getenv("FLASHPLAYER_WARM_GSC");
    char path[MAX_PATH];
    tu_file *out;
    gameswf::movie_definition *movie_def;
    gameswf::cache_options options;

    if ((!write_cache || !write_cache[0] || rb->atoi(write_cache) == 0) &&
        (!precompute || !precompute[0] || rb->atoi(precompute) == 0) &&
        (!warm_gameplay || !warm_gameplay[0] ||
         rb->atoi(warm_gameplay) == 0))
        return;

    if (!flash_root())
        return;

    movie_def = flash_root()->get_movie_definition();
    if (!movie_def) {
        flash_logf("gameswf cache write skipped: no movie def");
        return;
    }

    if (precompute && precompute[0] && rb->atoi(precompute) != 0) {
        flash_logf("gameswf cache precompute begin");
        gameswf::precompute_cached_data(movie_def);
        flash_logf("gameswf cache precompute end");
    }

    if (warm_gameplay && warm_gameplay[0] &&
        rb->atoi(warm_gameplay) != 0 && g_renderer) {
        bool old_discard = g_renderer->discard_render;
        int old_budget = g.shape_mesh_budget_base;

        const char *warm_frames_env = getenv("FLASHPLAYER_WARM_GSC_FRAMES");
        int warm_frames = warm_frames_env && warm_frames_env[0] ?
            rb->atoi(warm_frames_env) : 8;
        if (warm_frames < 1)
            warm_frames = 1;
        if (warm_frames > 120)
            warm_frames = 120;

        flash_logf("gameswf cache gameplay warm begin frames=%d",
                   warm_frames);
        g.shape_mesh_budget_base = 1000000;
        g.shape_mesh_budget = g.shape_mesh_budget_base;
        g_renderer->discard_render = true;
        for (int i = 0; i < warm_frames; i++) {
            flash_logf("gameswf cache gameplay warm frame=%d begin", i);
            flash_root()->advance(i == 0 ? 0.0f : 1.0f / 35.0f);
            prime_stickrpg_post_advance();
            prefill_stickrpg_character_name();
            force_stickrpg_post_create_state();
            if (g.stickrpg_gameplay_shortcut_done &&
                (i == 2 || i == 8 || i == 14 || i == 20))
                flash_key_notify(gameswf::key::RIGHT, true);
            if (g.stickrpg_gameplay_shortcut_done &&
                (i == 5 || i == 11 || i == 17 || i == 23))
                flash_key_notify(gameswf::key::RIGHT, false);
            if (g.stickrpg_gameplay_shortcut_done &&
                (i == 6 || i == 16))
                flash_key_notify(gameswf::key::DOWN, true);
            if (g.stickrpg_gameplay_shortcut_done &&
                (i == 9 || i == 19))
                flash_key_notify(gameswf::key::DOWN, false);
            flash_root()->display();
            flash_logf("gameswf cache gameplay warm frame=%d end tri=%d sf=%d bf=%d heap_free=%luK",
                       i, g_renderer->triangles, g_renderer->solid_fills,
                       g_renderer->bitmap_fills,
                       (unsigned long)(plugin_cxx_available() / 1024));
        }
        flash_key_notify(gameswf::key::RIGHT, false);
        flash_key_notify(gameswf::key::DOWN, false);
        g_renderer->discard_render = old_discard;
        g.shape_mesh_budget_base = old_budget;
        g.shape_mesh_budget = old_budget;
        flash_logf("gameswf cache gameplay warm end tri=%d tiny=%d sf=%d bf=%d skipped=%d heap_free=%luK",
                   g_renderer->triangles, g_renderer->tiny_triangles,
                   g_renderer->solid_fills, g_renderer->bitmap_fills,
                   g.shape_mesh_skipped,
                   (unsigned long)(plugin_cxx_available() / 1024));
    }

    rb->snprintf(path, sizeof(path), "%s%s", g.path, FLASH_CACHE_SUFFIX);
    out = rb_tu_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (!out) {
        flash_logf("gameswf cache write open failed %s", path);
        return;
    }

    movie_def->output_cached_data(out, options);
    flash_logf("gameswf cache wrote %s size=%d err=%d", path,
               out->get_position(), out->get_error());
    delete out;
}
#endif

static void close_shape_def_cache_files(void)
{
    gameswf::set_shape_definition_cache_files(NULL, NULL);

    delete g.shape_def_cache_in;
    g.shape_def_cache_in = NULL;
    delete g.shape_def_cache_out;
    g.shape_def_cache_out = NULL;
}

static void open_shape_def_cache_files(void)
{
    char path[MAX_PATH];

    close_shape_def_cache_files();
    rb->snprintf(path, sizeof(path), "%s%s", g.path, FLASH_DEF_CACHE_SUFFIX);

#ifdef SIMULATOR
    {
        const char *write_cache = getenv("FLASHPLAYER_WRITE_GSD");
        const char *read_cache = getenv("FLASHPLAYER_READ_GSD");

        if (write_cache && write_cache[0] && rb->atoi(write_cache) != 0) {
            g.shape_def_cache_out =
                rb_tu_open(path, O_WRONLY | O_CREAT | O_TRUNC);
            if (g.shape_def_cache_out)
                flash_logf("shape def cache write open %s", path);
            else
                flash_logf("shape def cache write open failed %s", path);
        } else if (read_cache && read_cache[0] && rb->atoi(read_cache) != 0) {
            g.shape_def_cache_in = rb_tu_open(path, O_RDONLY);
            if (g.shape_def_cache_in)
                flash_logf("shape def cache read open %s", path);
            else
                flash_logf("shape def cache read missing %s", path);
        }
    }
#else
    g.shape_def_cache_in = rb_tu_open(path, O_RDONLY);
    if (g.shape_def_cache_in)
        flash_logf("shape def cache read open %s", path);
    else
        flash_logf("shape def cache read missing %s", path);
#endif

    gameswf::set_shape_definition_cache_files(g.shape_def_cache_in,
                                              g.shape_def_cache_out);
}

static void probe_gameswf_runtime(void)
{
    if (!g.loaded || !g.fws) {
        if (!g.status[0])
            rb->snprintf(g.status, sizeof(g.status), "runtime skipped: no SWF");
        return;
    }

    show_load_status_progress("gameswf: new player", 36);
    delete g_player;
    g_player = new gameswf::player;
    if (!g_player) {
        rb->snprintf(g.status, sizeof(g.status), "gameswf player alloc failed");
        flash_logf("gameswf player allocation failed heap_free=%luK",
                   (unsigned long)(plugin_cxx_available() / 1024));
        return;
    }
    flash_logf("gameswf player allocated heap_free=%luK",
               (unsigned long)(plugin_cxx_available() / 1024));
    g_player->set_separate_thread(false);
    g_player->set_force_realtime_framerate(false);

    delete g_sound_handler;
    g_sound_handler = new SilentSoundHandler;
    gameswf::set_sound_handler(g_sound_handler);

    g.load_stage_base = 38;
    g.load_stage_span = 34;
    show_load_status_progress("gameswf: load_file", 38);
    if (!g_renderer)
        g_renderer = new RockboxRenderHandler;
    if (!g_renderer) {
        rb->snprintf(g.status, sizeof(g.status), "renderer alloc failed");
        flash_logf("renderer allocation failed heap_free=%luK",
                   (unsigned long)(plugin_cxx_available() / 1024));
        return;
    }
    flash_logf("renderer ready fb=%d heap_free=%luK",
               g_renderer->framebuf ? 1 : 0,
               (unsigned long)(plugin_cxx_available() / 1024));

    gameswf::register_log_callback(flash_log_callback);
    gameswf::set_verbose_action(false);
    gameswf::set_render_handler(g_renderer);
    gameswf::register_file_opener_callback(flash_file_opener);
    gameswf::set_use_cache_files(g.stickrpg_fast_load);
    g.gameswf_cache_checked = g.stickrpg_fast_load;
    gameswf::clear_shared_fonts();
    if (!g_root_ref)
        g_root_ref = new gameswf::gc_ptr<gameswf::root>;
    if (!g_root_ref) {
        rb->snprintf(g.status, sizeof(g.status), "root ref alloc failed");
        flash_logf("root ref allocation failed heap_free=%luK",
                   (unsigned long)(plugin_cxx_available() / 1024));
        return;
    }
    open_shape_def_cache_files();
    *g_root_ref = g_player->load_file(STICK_RPG_URL);
    close_shape_def_cache_files();

    if (!flash_root()) {
        rb->snprintf(g.status, sizeof(g.status), "gameswf load failed");
        flash_logf("gameswf load failed hits=%d misses=%d last_tag=%d pos=%d heap_free=%luK last='%s'",
                   g.loader_hits, g.loader_misses, g.loader_last_tag,
                   g.loader_last_pos,
                   (unsigned long)(plugin_cxx_available() / 1024),
                   g.last_log);
        return;
    }

    if (g.input_profile == FLASH_INPUT_PROFILE_STICKRPG) {
        gameswf::character *root_movie = flash_root()->get_root_movie();
        gameswf::sprite_instance *root_sprite =
            gameswf::cast_to<gameswf::sprite_instance>(root_movie);
        gameswf::as_value existing_done_intro;

        if (root_movie &&
            !root_movie->get_member("doneIntro", &existing_done_intro)) {
            gameswf::as_value host_callback(
                g_player, stickrpg_host_done_intro);
            if (root_sprite && root_sprite->get_environment())
                root_sprite->get_environment()->set_local(
                    "doneIntro", host_callback);
            else
                root_movie->as_object::set_member(
                    "doneIntro", host_callback);
            bool virtual_found =
                root_movie->get_member("doneIntro", &existing_done_intro);
            flash_logf("stickrpg installed host doneIntro callback root=%p virtual=%d function=%d",
                       root_movie,
                       virtual_found ? 1 : 0,
                       existing_done_intro.is_function() ? 1 : 0);
        }
    }

    show_load_status_progress("gameswf: root ready", 74);
    flash_root()->set_display_viewport(0, 0, LCD_WIDTH, LCD_HEIGHT);
#ifdef SIMULATOR
    write_gameswf_cache_if_requested();
#endif
    g.runtime_ready = true;
    g.runtime_frame = flash_root()->get_current_frame();
    flash_logf("gameswf root ready frame=%d hits=%d misses=%d regs=%d fonts=%d/%d/%d/%d heap_free=%luK",
               g.runtime_frame, g.loader_hits, g.loader_misses,
               g.loader_registers, g.font_adds, g.font_local_hits,
               g.font_fallback_hits, g.font_misses,
               (unsigned long)(plugin_cxx_available() / 1024));
    rb->snprintf(g.status, sizeof(g.status), "VM ready: Select renders %d",
                 g.runtime_frame);
}

static void draw_panel(int x, int y, int w, int h)
{
    SET_FG(COL_PANEL);
    rb->lcd_fillrect(x, y, w, h);
    SET_FG(COL_PANEL2);
    rb->lcd_drawrect(x, y, w, h);
}

static void draw_page_summary(void)
{
    SET_FG(COL_INK);
    rb->lcd_putsxy(10, 28, "Official SWF loader");

    SET_FG(g.loaded ? COL_OK : COL_WARN);
    rb->lcd_putsxy(10, 46, g.status);

    SET_FG(COL_DIM);
    rb->lcd_putsxyf(10, 68, "File: %s", g.info.sig[0] ? g.info.sig : "---");
    rb->lcd_putsxyf(10, 84, "Flash v%d  %luK -> %luK",
                    g.info.version, g.info.actual_size / 1024,
                    g.info.declared_size / 1024);
    rb->lcd_putsxyf(10, 100, "Stage %dx%d  %d.%02dfps",
                    g.info.stage_w, g.info.stage_h,
                    g.info.fps_x100 / 100, g.info.fps_x100 % 100);
    rb->lcd_putsxyf(10, 116, "Frames %d  Tags %d",
                    g.info.frames, g.tags.total);
    rb->lcd_putsxyf(10, 132, "Actions %d  Sprites %d",
                    g.tags.do_action + g.tags.do_init_action,
                    g.tags.sprite);
    rb->lcd_putsxyf(10, 148, "No AVM2: %s  Video: %s",
                    g.tags.do_abc ? "no" : "yes",
                    g.tags.video ? "yes" : "none");
    rb->lcd_putsxyf(10, 164, "Runtime %s  frame %d",
                    g.runtime_loaded ? "loaded" : "pending",
                    g.runtime_frame);
    rb->lcd_putsxyf(10, 180, "Last button 0x%08x", g.last_button);
}

static void draw_page_tags(void)
{
    SET_FG(COL_INK);
    rb->lcd_putsxy(10, 28, "SWF tag workload");
    SET_FG(COL_DIM);
    rb->lcd_putsxyf(10, 50, "ShowFrame %d", g.tags.show_frame);
    rb->lcd_putsxyf(10, 66, "Place/Remove %d/%d", g.tags.place, g.tags.remove);
    rb->lcd_putsxyf(10, 82, "Shape/Text/Edit %d/%d/%d",
                    g.tags.shape, g.tags.text, g.tags.edit_text);
    rb->lcd_putsxyf(10, 98, "Button/Bitmap %d/%d",
                    g.tags.button, g.tags.bitmap);
    rb->lcd_putsxyf(10, 114, "Sound tags %d", g.tags.sound);
    rb->lcd_putsxyf(10, 130, "DoAction %d  DoInit %d",
                    g.tags.do_action, g.tags.do_init_action);
    rb->lcd_putsxyf(10, 146, "Nested depth %d", g.tags.max_depth);
}

static void draw_page_runtime(void)
{
    SET_FG(COL_INK);
    rb->lcd_putsxy(10, 28, "Runtime status");
    SET_FG(COL_DIM);
    rb->lcd_putsxy(10, 52, "CWS inflate: working");
    rb->lcd_putsxy(10, 68, "SWF parser: working");
    rb->lcd_putsxyf(10, 84, "AVM1 VM: %s",
                    g.runtime_loaded ? "loaded" : "not loaded");
    rb->lcd_putsxy(10, 100, "Renderer: next");
    rb->lcd_putsxy(10, 116, "Input bridge: planned");
    rb->lcd_putsxy(10, 132, "SharedObject save: planned");
    SET_FG(COL_ACCENT);
    rb->lcd_putsxy(10, 158, "This is Flash data, not a remake.");
}

static void redraw(void)
{
    SET_BG(COL_BG);
    SET_FG(COL_BG);
    rb->lcd_clear_display();

    SET_FG(COL_PANEL);
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 20);
    rb->lcd_fillrect(0, LCD_HEIGHT - 18, LCD_WIDTH, 18);
    SET_FG(COL_INK);
    rb->lcd_putsxy(6, 4, "Flash Player");
    SET_FG(COL_DIM);
    rb->lcd_putsxyf(120, 4, "page %d/3", g.page + 1);

    draw_panel(6, 24, LCD_WIDTH - 12, LCD_HEIGHT - 48);
    if (g.page == 0)
        draw_page_summary();
    else if (g.page == 1)
        draw_page_tags();
    else
        draw_page_runtime();

    SET_FG(COL_DIM);
    rb->lcd_putsxy(6, LCD_HEIGHT - 14, "Wheel page  Select reload  Menu quit");
    rb->lcd_update();
}

static void draw_runtime_cursor(void)
{
    int x = g.cursor_x;
    int y = g.cursor_y;

    if (g_renderer) {
        rb->lcd_set_viewport(NULL);
    }

    rb->lcd_set_foreground(g.mouse_down ? COL_ACCENT : COL_INK);
    rb->lcd_drawline(x - 5, y, x + 5, y);
    rb->lcd_drawline(x, y - 5, x, y + 5);
    rb->lcd_drawrect(x - 3, y - 3, 7, 7);
    rb->lcd_update_rect(x - 6, y - 6, 13, 13);
}

static void draw_bitmap_text_overlay(void)
{
    if (!g_bitmap_text_enabled || !g_renderer)
        return;

    rb->lcd_set_viewport(NULL);
    /* FONT_SYSFIXED is compiled into Rockbox and cannot disappear with a
     * theme/font change.  Embedded SWF outlines remain disabled for Stick RPG
     * because their tessellation is both unreadable and expensive on-device. */
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_drawmode(DRMODE_FG);
    for (int i = 0; i < g_bitmap_text_span_count; i++) {
        FlashBitmapTextSpan *span = &g_bitmap_text_spans[i];
        float screen_height = span->stage_height * g_renderer->sy;
        int font_id;
        int font_height;
        int x;
        int y;
        int luminance;

        if (span->length <= 0 || span->alpha < 24)
            continue;
        if (screen_height < 0.0f)
            screen_height = -screen_height;

        font_id = FONT_SYSFIXED;
        rb->font_getstringsize(span->text, NULL, &font_height, font_id);

        x = RockboxRenderHandler::round_pixel(
            span->stage_x * g_renderer->sx + g_renderer->tx);
        y = RockboxRenderHandler::round_pixel(
            span->stage_baseline * g_renderer->sy + g_renderer->ty) -
            font_height;

        /* Gameplay HUD values are redrawn from the live AVM1 variables below
         * so split SWF text records cannot overlap digits (17/17 becoming
         * 17/7, padded cash strings, etc.). */
        if (g.runtime_frame == 1 && y < 38)
            continue;

        if (x >= LCD_WIDTH || y >= LCD_HEIGHT || y + font_height < 0)
            continue;

        /* A one-pixel opposite-luminance shadow keeps text legible over both
         * Stick RPG's blue menus and its light gameplay panels. */
        luminance = span->red * 30 + span->green * 59 + span->blue * 11;
        rb->lcd_set_foreground(luminance < 12800
            ? LCD_RGBPACK(255, 255, 255) : LCD_RGBPACK(0, 0, 0));
        rb->lcd_putsxy(x + 1, y + 1, span->text);
        rb->lcd_set_foreground(LCD_RGBPACK(span->red, span->green, span->blue));
        rb->lcd_putsxy(x, y, span->text);
    }
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static void draw_stickrpg_hud_text(void)
{
    gameswf::character *root_movie;
    gameswf::as_value hp;
    gameswf::as_value hpmax;
    gameswf::as_value cash;
    gameswf::as_value day;

    if (g.input_profile != FLASH_INPUT_PROFILE_STICKRPG ||
        g.runtime_frame != 1 || !flash_root())
        return;

    root_movie = flash_root()->get_root_movie();
    if (!root_movie || !root_movie->get_member("hp", &hp) ||
        !root_movie->get_member("hpmax", &hpmax) ||
        !root_movie->get_member("cash", &cash) ||
        !root_movie->get_member("day", &day))
        return;

    rb->lcd_set_viewport(NULL);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_set_drawmode(DRMODE_FG);
    rb->lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
    rb->lcd_putsxyf(25, 8, "HP %d/%d", hp.to_int(), hpmax.to_int());
    rb->lcd_set_foreground(LCD_RGBPACK(255, 225, 32));
    rb->lcd_putsxyf(126, 8, "$%d", cash.to_int());
    rb->lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
    rb->lcd_putsxyf(218, 8, "DAY %d", day.to_int());
    rb->lcd_set_drawmode(DRMODE_SOLID);
}

static gameswf::character *stickrpg_person(void)
{
    gameswf::character *root_movie;
    gameswf::as_value value;

    if (g.input_profile != FLASH_INPUT_PROFILE_STICKRPG ||
        g.runtime_frame != 1 || !flash_root())
        return NULL;

    root_movie = flash_root()->get_root_movie();
    if (!root_movie || !root_movie->get_member("person", &value))
        return NULL;
    return gameswf::cast_to<gameswf::character>(value.to_object());
}

static void sync_stickrpg_clock(void)
{
    gameswf::character *root_movie;
    gameswf::sprite_instance *root_sprite;
    const char *time_text;
    int frame;

    if (g.input_profile != FLASH_INPUT_PROFILE_STICKRPG ||
        g.runtime_frame != 1 || !flash_root())
        return;

    root_movie = flash_root()->get_root_movie();
    root_sprite = gameswf::cast_to<gameswf::sprite_instance>(root_movie);
    if (!root_movie || !root_sprite)
        return;
    time_text = root_sprite->get_variable("time");
    if (!time_text || !time_text[0] ||
        rb->strcmp(time_text, "undefined") == 0)
        return;
    if (g_stickrpg_clock_ref == NULL) {
        gameswf::character *clock_character =
            root_sprite->m_display_list.get_character_at_depth(252);
        gameswf::sprite_instance *clock =
            gameswf::cast_to<gameswf::sprite_instance>(clock_character);
        if (!clock || clock->get_id() != 715 ||
            clock->get_frame_count() != 65)
            return;
        g_stickrpg_clock_ref = clock;
        flash_logf("stickrpg clock id=%d depth=%d frames=%d",
                   clock->get_id(), clock->get_depth(),
                   clock->get_frame_count());
    }

    /* This HUD movie has 65 quarter-hour frames spanning 08:00 through
     * midnight. It is meant to be addressed from _root.time; when left in
     * PLAY it drains the game's stamina clock in under two seconds. */
    frame = (rb->atoi(time_text) - 8) * 4;
    if (frame < 0)
        frame = 0;
    if (frame > 64)
        frame = 64;
    if (g_stickrpg_clock_ref->get_current_frame() != frame)
        g_stickrpg_clock_ref->goto_frame(frame);
    g_stickrpg_clock_ref->set_play_state(gameswf::character::STOP);
}

static void prepare_stickrpg_person(void)
{
    gameswf::character *person = stickrpg_person();
    gameswf::character *root_movie;
    gameswf::as_value xmove;
    gameswf::as_value ymove;

    if (!person)
        return;
#ifdef SIMULATOR
    if (g.stickrpg_person_frame_override >= 0) {
        person->goto_frame(g.stickrpg_person_frame_override);
        person->set_play_state(gameswf::character::STOP);
        return;
    }
#endif
    if (g.input_profile != FLASH_INPUT_PROFILE_STICKRPG ||
        g.runtime_frame != 1 || !flash_root())
        return;

    root_movie = flash_root()->get_root_movie();
    if (root_movie && root_movie->get_member("xmove", &xmove) &&
        root_movie->get_member("ymove", &ymove) &&
        (xmove.to_int() != 0 || ymove.to_int() != 0)) {
        int frame = person->get_current_frame();

        /* The normal walking cycle is authored at Flash frames 275-302.
         * GameSWF can miss the terminal gotoAndPlay(275) after a root-frame
         * change and otherwise runs into the skateboard/car/death artwork. */
        if (frame < 274 || frame > 301) {
            person->goto_frame(274);
            person->set_play_state(gameswf::character::PLAY);
        }
    }
}

static void runtime_mouse_stage(int *x, int *y)
{
    int sw = g.info.stage_w > 0 ? g.info.stage_w : LCD_WIDTH;
    int sh = g.info.stage_h > 0 ? g.info.stage_h : LCD_HEIGHT;

    *x = g.cursor_x * sw / LCD_WIDTH;
    *y = g.cursor_y * sh / LCD_HEIGHT;
}

struct FlashMouseTarget
{
    gameswf::character *entity;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int hits;
};

static bool stickrpg_authored_cursor_mode(void)
{
    return g.input_profile == FLASH_INPUT_PROFILE_STICKRPG &&
        g.stickrpg_authored_ui_ready && g.runtime_frame == 0;
}

static int collect_runtime_mouse_targets(FlashMouseTarget *targets,
                                         int capacity)
{
    gameswf::character *root_movie;
    int count = 0;
    int sw;
    int sh;

    if (!targets || capacity <= 0 || !flash_root())
        return 0;

    root_movie = flash_root()->get_root_movie();
    if (!root_movie)
        return 0;

    sw = g.info.stage_w > 0 ? g.info.stage_w : LCD_WIDTH;
    sh = g.info.stage_h > 0 ? g.info.stage_h : LCD_HEIGHT;

    /*
     * Ask GameSWF's real hit tester which authored entity owns each sample.
     * Grouping by the returned character makes this work across the title,
     * difficulty, character, instruction, and in-game dialog screens without
     * hard-coding Stick RPG's button coordinates or replacing its UI.
     */
    for (int lcd_y = 3; lcd_y < LCD_HEIGHT; lcd_y += 6) {
        int stage_y = lcd_y * sh / LCD_HEIGHT;
        for (int lcd_x = 5; lcd_x < LCD_WIDTH; lcd_x += 10) {
            int stage_x = lcd_x * sw / LCD_WIDTH;
            gameswf::character *entity = NULL;
            int index;

            if (!root_movie->get_topmost_mouse_entity(
                    entity, PIXELS_TO_TWIPS((float)stage_x),
                    PIXELS_TO_TWIPS((float)stage_y)) ||
                !entity || !entity->can_handle_mouse_event())
                continue;

            for (index = 0; index < count; index++)
                if (targets[index].entity == entity)
                    break;

            if (index == count) {
                if (count >= capacity)
                    continue;
                targets[index].entity = entity;
                targets[index].min_x = lcd_x;
                targets[index].max_x = lcd_x;
                targets[index].min_y = lcd_y;
                targets[index].max_y = lcd_y;
                targets[index].hits = 0;
                count++;
            }

            if (lcd_x < targets[index].min_x)
                targets[index].min_x = lcd_x;
            if (lcd_x > targets[index].max_x)
                targets[index].max_x = lcd_x;
            if (lcd_y < targets[index].min_y)
                targets[index].min_y = lcd_y;
            if (lcd_y > targets[index].max_y)
                targets[index].max_y = lcd_y;
            targets[index].hits++;
        }
    }

    return count;
}

static bool snap_stickrpg_cursor(int direction_x, int direction_y)
{
    FlashMouseTarget targets[48];
    int count;
    int best = -1;
    long best_score = 0x7fffffffL;
    bool wrapped = false;

    if (!stickrpg_authored_cursor_mode())
        return false;

    count = collect_runtime_mouse_targets(
        targets, (int)(sizeof(targets) / sizeof(targets[0])));
    if (count <= 0) {
        flash_logf("stickrpg cursor snap miss dir=%d,%d targets=0",
                   direction_x, direction_y);
        return false;
    }

retry_wrapped:
    for (int i = 0; i < count; i++) {
        int center_x = (targets[i].min_x + targets[i].max_x) / 2;
        int center_y = (targets[i].min_y + targets[i].max_y) / 2;
        int delta_x = center_x - g.cursor_x;
        int delta_y = center_y - g.cursor_y;
        long score;

        if (direction_y != 0) {
            if (!wrapped && delta_y * direction_y <= 2)
                continue;
            score = (wrapped ?
                (direction_y > 0 ? center_y : LCD_HEIGHT - center_y) :
                delta_y * direction_y) * 1024L +
                (delta_x < 0 ? -delta_x : delta_x);
        } else {
            if (!wrapped && delta_x * direction_x <= 2)
                continue;
            score = (wrapped ?
                (direction_x > 0 ? center_x : LCD_WIDTH - center_x) :
                delta_x * direction_x) * 1024L +
                (delta_y < 0 ? -delta_y : delta_y);
        }

        if (score < best_score) {
            best_score = score;
            best = i;
        }
    }

    if (best < 0 && !wrapped) {
        wrapped = true;
        goto retry_wrapped;
    }
    if (best < 0) {
        flash_logf("stickrpg cursor snap miss dir=%d,%d targets=%d",
                   direction_x, direction_y, count);
        return false;
    }

    g.cursor_x = (targets[best].min_x + targets[best].max_x) / 2;
    g.cursor_y = (targets[best].min_y + targets[best].max_y) / 2;
    flash_logf("stickrpg cursor snap dir=%d,%d lcd=%d,%d target=%s hits=%d",
               direction_x, direction_y, g.cursor_x, g.cursor_y,
               targets[best].entity->get_name().c_str(),
               targets[best].hits);
    return true;
}

static void present_cached_runtime_frame(void)
{
    if (!g.runtime_loaded || !g_renderer || !g_renderer->framebuf)
        return;

    /* Reusing the completed software framebuffer makes click-wheel cursor
     * feedback independent of the expensive SWF vector render. This matters
     * on the 6G, where Stick RPG's menu can take several seconds to redraw. */
    rb->lcd_set_viewport(NULL);
    rb->lcd_bitmap(g_renderer->framebuf, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    draw_bitmap_text_overlay();
    draw_stickrpg_hud_text();
    draw_runtime_cursor();
    rb->lcd_update();
}

static void dispatch_runtime_mouse_click(void)
{
    int x;
    int y;

    if (!g.runtime_loaded || !flash_root())
        return;

    runtime_mouse_stage(&x, &y);
    g.mouse_down = true;
    flash_root()->notify_mouse_state(x, y, 1);
    flash_root()->advance(0.0f);
    g.mouse_down = false;
    flash_root()->notify_mouse_state(x, y, 0);
    flash_root()->advance(0.0f);
    g.click_frames = 0;
    flash_logf("direct click lcd=%d,%d stage=%d,%d frame=%d",
               g.cursor_x, g.cursor_y, x, y, g.rendered_frames);
}

#ifdef SIMULATOR
static bool parse_autorun_int(const char **spec, int *value)
{
    const char *p = *spec;
    int sign = 1;
    int v = 0;
    bool any = false;

    if (*p == '-') {
        sign = -1;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
        any = true;
    }
    if (!any)
        return false;

    *value = v * sign;
    *spec = p;
    return true;
}

static void parse_autorun_clicks(const char *spec)
{
    const char *p = spec;

    while (p && *p && g.autorun_click_count < 8) {
        int frame;
        int x;
        int y;

        while (*p == ' ' || *p == ';' || *p == ',')
            p++;
        if (!parse_autorun_int(&p, &frame) || *p++ != ':' ||
            !parse_autorun_int(&p, &x) || *p++ != ':' ||
            !parse_autorun_int(&p, &y)) {
            break;
        }

        if (x < 0)
            x = 0;
        if (x >= LCD_WIDTH)
            x = LCD_WIDTH - 1;
        if (y < 0)
            y = 0;
        if (y >= LCD_HEIGHT)
            y = LCD_HEIGHT - 1;

        g.autorun_click_frame[g.autorun_click_count] = frame;
        g.autorun_click_x[g.autorun_click_count] = x;
        g.autorun_click_y[g.autorun_click_count] = y;
        g.autorun_click_count++;
    }

    if (g.autorun_click_count > 0)
        flash_logf("autorun clicks=%d", g.autorun_click_count);

    g.autorun_click_armed = false;
}

static void run_autorun_click_script(void)
{
    if (g.autorun_click_next >= g.autorun_click_count)
        return;

    if (g.rendered_frames < g.autorun_click_frame[g.autorun_click_next])
        return;

    g.cursor_x = g.autorun_click_x[g.autorun_click_next];
    g.cursor_y = g.autorun_click_y[g.autorun_click_next];

    if (!g.autorun_click_armed) {
        g.mouse_down = false;
        g.click_frames = 0;
        g.autorun_click_armed = true;
        flash_logf("autorun hover frame=%d lcd=%d,%d",
                   g.rendered_frames, g.cursor_x, g.cursor_y);
        return;
    }

    g.mouse_down = true;
    g.click_frames = 1;
    flash_logf("autorun click frame=%d lcd=%d,%d",
               g.rendered_frames, g.cursor_x, g.cursor_y);
    g.autorun_click_armed = false;
    g.autorun_click_next++;
}

static void run_autorun_save_call(void)
{
    gameswf::character *root_movie;
    gameswf::sprite_instance *root_sprite;
    gameswf::as_environment *environment;
    gameswf::as_value function;
    array<gameswf::with_stack_entry> with_stack;
    int stack_size;

    if (g.autorun_call_save_frame <= 0 ||
        g.rendered_frames != g.autorun_call_save_frame ||
        !flash_root() || !g_player)
        return;

    root_movie = flash_root()->get_root_movie();
    root_sprite = gameswf::cast_to<gameswf::sprite_instance>(root_movie);
    environment = root_sprite ? root_sprite->get_environment() : NULL;
    if (!environment)
        return;

    function = environment->get_variable("saveGame", with_stack);
    if (!function.is_function()) {
        flash_logf("autorun saveGame missing frame=%d",
                   g.rendered_frames);
        return;
    }

    stack_size = environment->get_stack_size();
    gameswf::call_method(
        function, environment, root_sprite, 0,
        environment->get_top_index());
    environment->set_stack_size(stack_size);
    flash_logf("autorun saveGame called frame=%d", g.rendered_frames);
}
#endif

static void detect_stickrpg_authored_ui(void)
{
    gameswf::character *root_movie;
    gameswf::as_value val;
    gameswf::character *intro;
    gameswf::as_value newgame_val;
    gameswf::as_value makechar_val;
    gameswf::as_value inst_val;
    gameswf::as_value start_val;
    gameswf::as_value load_val;
    gameswf::character *newgame;
    gameswf::character *makechar;
    gameswf::character *instructions;
    gameswf::character *startbutton;
    gameswf::character *loadbutton;
    int start_visible;
    int load_visible;
    int new_visible;
    int make_visible;
    int instructions_visible;
    int signature;

    if (g.input_profile != FLASH_INPUT_PROFILE_STICKRPG ||
        !g.stickrpg_host_intro_finalized ||
        !flash_root())
        return;

    root_movie = flash_root()->get_root_movie();
    if (root_movie && root_movie->get_current_frame() != 0)
        return;
    if (!root_movie || !root_movie->get_member("introscreen", &val))
        return;

    intro = gameswf::cast_to<gameswf::character>(val.to_object());
    if (!intro)
        return;

    newgame = intro->get_member("newgame", &newgame_val) ?
        gameswf::cast_to<gameswf::character>(newgame_val.to_object()) : NULL;
    makechar = intro->get_member("makechar", &makechar_val) ?
        gameswf::cast_to<gameswf::character>(makechar_val.to_object()) : NULL;
    instructions = intro->get_member("instructions", &inst_val) ?
        gameswf::cast_to<gameswf::character>(inst_val.to_object()) : NULL;
    startbutton = intro->get_member("startbutton", &start_val) ?
        gameswf::cast_to<gameswf::character>(start_val.to_object()) : NULL;
    loadbutton = intro->get_member("loadButton", &load_val) ?
        gameswf::cast_to<gameswf::character>(load_val.to_object()) : NULL;

    start_visible = startbutton && startbutton->get_visible() ? 1 : 0;
    load_visible = loadbutton && loadbutton->get_visible() ? 1 : 0;
    new_visible = newgame && newgame->get_visible() ? 1 : 0;
    make_visible = makechar && makechar->get_visible() ? 1 : 0;
    instructions_visible =
        instructions && instructions->get_visible() ? 1 : 0;
    signature = start_visible |
        (load_visible << 1) |
        (new_visible << 2) |
        (make_visible << 3) |
        (instructions_visible << 4);

    if (!intro->get_visible() ||
        signature == 0)
        return;

    if (!g.stickrpg_authored_ui_ready) {
        g.stickrpg_authored_ui_ready = true;
        flash_logf("stickrpg authored UI ready frame=%d intro=1 start=%d load=%d new=%d make=%d inst=%d errors=%d",
                   g.rendered_frames, start_visible, load_visible,
                   new_visible, make_visible, instructions_visible,
                   g.log_errors);
        flash_progress_checkpoint("authored menu clips ready", 100);
    } else if (signature != g.stickrpg_authored_ui_signature) {
        flash_logf("stickrpg authored UI state frame=%d start=%d load=%d new=%d make=%d inst=%d errors=%d",
                   g.rendered_frames, start_visible, load_visible,
                   new_visible, make_visible, instructions_visible,
                   g.log_errors);
    }
    g.stickrpg_authored_ui_signature = signature;
}

static void prime_stickrpg_post_advance(void)
{
    gameswf::character *root_movie;
    gameswf::sprite_instance *root_sprite;
    gameswf::as_value val;

    if (!g.prime_stickrpg || !flash_root())
        return;

    root_movie = flash_root()->get_root_movie();
    root_sprite = gameswf::cast_to<gameswf::sprite_instance>(root_movie);
    if (root_sprite) {
        root_sprite->set_display_object_visible("pregameMC", false);
        root_sprite->set_display_object_visible("filmscreen", false);
        root_sprite->set_display_object_visible("black", false);
        root_sprite->set_display_object_visible("loaderText", false);
    }
    if (!g.stickrpg_intro_hide_requested && root_movie &&
        root_movie->get_member("introscreen", &val)) {
        gameswf::character *intro =
            gameswf::cast_to<gameswf::character>(val.to_object());
        if (intro)
            intro->set_visible(true);
    }
}

static void prefill_stickrpg_character_name(void)
{
    gameswf::character *root_movie;
    gameswf::as_value val;
    gameswf::as_value makechar_val;
    gameswf::as_value name_val;

    if (g.stickrpg_name_prefilled || !flash_root())
        return;

    root_movie = flash_root()->get_root_movie();
    if (!root_movie || !root_movie->get_member("introscreen", &val))
        return;

    gameswf::character *intro =
        gameswf::cast_to<gameswf::character>(val.to_object());
    if (!intro || !intro->get_visible())
        return;

    gameswf::character *makechar = intro->get_member("makechar",
        &makechar_val) ?
        gameswf::cast_to<gameswf::character>(makechar_val.to_object()) : NULL;
    if (!makechar || !makechar->get_visible())
        return;

    name_val.set_string("David");
    root_movie->set_member("textname", name_val);
    root_movie->set_member("pname", name_val);
    intro->set_member("textname", name_val);
    makechar->set_member("textname", name_val);
    g.stickrpg_name_prefilled = true;
    flash_logf("stickrpg prefilled character name=David");
}

static void log_stickrpg_scene_vars(const char *where,
                                    gameswf::character *root_movie,
                                    gameswf::sprite_instance *root_sprite)
{
    static const char * const names[] = {
        "Scene", "mapx", "mapy", "yPos", "mapstate", "dartPos",
        "person", "map_outside_1", "Map_Outside_1",
        "hp", "hpmax", "hpmax2", "cash", "bankcash", "day", "time",
        "energy", "energymax", "strength", "intelligence", "charm",
        "karma", "walkspeed", "xmove", "ymove", "kLeft", "kUp",
        "kRight", "kDown", "nowmoving", "prevmoving", "prevwalkspeed"
    };
    size_t i;

    if (!where || !root_movie || !root_sprite)
        return;

    for (i = 0; i < ARRAYLEN(names); i++) {
        gameswf::as_value val;
        if (root_movie->get_member(names[i], &val)) {
            const tu_string text = val.to_tu_string();
            gameswf::character *ch =
                gameswf::cast_to<gameswf::character>(val.to_object());
            flash_logf("stickrpg root %s member %s=%s obj=%d undef=%d",
                       where, names[i], text.c_str(),
                       val.is_object() ? 1 : 0,
                       val.is_undefined() ? 1 : 0);
            if (ch) {
                const gameswf::matrix& m = ch->get_matrix();
                flash_logf("stickrpg root %s clip %s vis=%d depth=%d id=%d frame=%d/%d pos10=%d,%d scale1000=%d,%d",
                           where, names[i],
                           ch->get_visible() ? 1 : 0,
                           ch->get_depth(), ch->get_id(),
                           ch->get_current_frame(), ch->get_frame_count(),
                           flash_log_fixed(
                               TWIPS_TO_PIXELS(m.m_[0][2]), 10),
                           flash_log_fixed(
                               TWIPS_TO_PIXELS(m.m_[1][2]), 10),
                           flash_log_fixed(float(m.m_[0][0]), 1000),
                           flash_log_fixed(float(m.m_[1][1]), 1000));
            }
        } else {
            const char *value = root_sprite->get_variable(names[i]);
            flash_logf("stickrpg root %s var %s=%s",
                       where, names[i], value ? value : "(null)");
        }
    }
}

static void stabilize_stickrpg_spawn(gameswf::character *root_movie)
{
    gameswf::as_value hpmax;
    gameswf::sprite_instance *root_sprite;

    if (!root_movie || g.stickrpg_spawn_stabilized)
        return;

    /* These are the original new-game values shown by Stick RPG Complete.
     * GameSWF exposes the film's formatting sentinels (0000000/9999/999)
     * when the 850-frame intro is fast-forwarded, which otherwise produces
     * a nonsensical HUD and an immediately exhausted character. */
    if (root_movie->get_member("hpmax", &hpmax) && hpmax.to_int() > 0)
        root_movie->set_member("hp", gameswf::as_value(hpmax.to_int()));
    else {
        root_movie->set_member("hpmax", gameswf::as_value(17));
        root_movie->set_member("hp", gameswf::as_value(17));
    }
    root_movie->set_member("cash", gameswf::as_value(100));
    root_movie->set_member("day", gameswf::as_value(1));
    root_movie->set_member("karma", gameswf::as_value(0));

    /* GameSWF's root goto leaves the title background and cloud animation
     * alive at depths 1 and 2. They are the solid blue lower strip and blue
     * speckles seen over the street on hardware. The real gameplay frame no
     * longer displays either layer. */
    root_sprite = gameswf::cast_to<gameswf::sprite_instance>(root_movie);
    if (root_sprite) {
        gameswf::character *title_bg =
            root_sprite->m_display_list.get_character_at_depth(1);
        gameswf::character *clouds =
            root_sprite->m_display_list.get_character_at_depth(2);
        if (title_bg && title_bg->get_id() == 269)
            title_bg->set_visible(false);
        if (clouds && clouds->get_name() == "clouds")
            clouds->set_visible(false);
        gameswf::character *sleep_mark =
            root_sprite->m_display_list.get_character_at_depth(98);
        gameswf::character *sleep_text =
            root_sprite->m_display_list.get_character_at_depth(99);
        if (sleep_mark && sleep_mark->get_id() == 619)
            sleep_mark->set_visible(false);
        if (sleep_text && sleep_text->get_id() == 620)
            sleep_text->set_visible(false);
    }
    if (flash_root())
        flash_root()->set_background_color(gameswf::rgba(104, 104, 104, 255));

    g.stickrpg_spawn_stabilized = true;
    sync_stickrpg_clock();
    flash_logf("stickrpg spawn stabilized hp=%d cash=100 day=1 karma=0",
               hpmax.to_int() > 0 ? hpmax.to_int() : 17);
}

static void sync_stickrpg_world(gameswf::character *root_movie)
{
    static const char *const names[] = {
        "Map_Outside_1", "Map_Outside_2", "Map_Outside_3",
        "Map_Outside_4", "Map_Outside_5", "Map_Outside_6",
        "Map_Outside_7"
    };
    static const float x_offsets[] = {
        -300.0f, -64.7f, -20.0f, -61.0f, 229.8f, -8.0f, -16.3f
    };
    static const float y_offsets[] = {
        -709.0f, -440.6f, -201.9f, 48.1f, 325.1f, 612.7f, 865.2f
    };
    gameswf::sprite_instance *root_sprite;
    gameswf::as_value mapx_value;
    gameswf::as_value mapy_value;
    float mapx;
    float mapy;
    bool visible[7];

    if (!root_movie || g.input_profile != FLASH_INPUT_PROFILE_STICKRPG ||
        g.runtime_frame != 1 ||
        !root_movie->get_member("mapx", &mapx_value) ||
        !root_movie->get_member("mapy", &mapy_value))
        return;

    root_sprite = gameswf::cast_to<gameswf::sprite_instance>(root_movie);
    if (!root_sprite)
        return;

    mapx = mapx_value.to_float();
    mapy = mapy_value.to_float();
    visible[0] = mapy > 537.0f;
    visible[1] = mapy > 315.0f && mapy < 956.0f;
    visible[2] = mapy > 69.0f && mapy < 734.0f;
    visible[3] = mapy > -186.0f && mapy < 488.0f;
    visible[4] = mapy > -486.0f && mapy < 236.0f;
    visible[5] = mapy < -64.0f;
    visible[6] = mapy < -337.0f;

    /* Stick RPG updates these clips through _root.map_outside_N.  Some AVM1
     * files keep that alias separate from the placed display object, so make
     * the authoritative root coordinates visible on the actual scene clips. */
    for (int i = 0; i < 7; i++) {
        gameswf::character *map =
            root_sprite->m_display_list.get_character_by_name_i(names[i]);
        if (map) {
            gameswf::matrix matrix = map->get_matrix();
            matrix.m_[0][2] = PIXELS_TO_TWIPS(mapx + x_offsets[i]);
            matrix.m_[1][2] = PIXELS_TO_TWIPS(mapy + y_offsets[i]);
            map->set_matrix(matrix);
            map->set_visible(visible[i]);
        }
    }

    /* GameSWF loses the cars' authoring-axis transform and renders them
     * sideways; rotating these complex vector clips also produces invalid
     * bounds in the software renderer.  Keep traffic parked off-screen until
     * that renderer path can preserve the authored transform safely. */
    for (int i = 0; i < 2; i++) {
        const char *car_name = i == 0 ? "car1" : "car2";
        gameswf::character *car =
            root_sprite->m_display_list.get_character_by_name_i(car_name);
        root_movie->set_member(i == 0 ? "car1s" : "car2s",
                               gameswf::as_value(2));
        root_movie->set_member(i == 0 ? "car1y" : "car2y",
                               gameswf::as_value(i == 0 ? -2000 : 2000));
        if (car) {
            gameswf::matrix matrix = car->get_matrix();
            matrix.m_[1][2] = PIXELS_TO_TWIPS(i == 0 ? -2000 : 2000);
            car->set_matrix(matrix);
            car->set_visible(true);
        }
    }
}

static void cancel_stickrpg_unsafe_damage_transition(
    gameswf::character *root_movie)
{
    gameswf::sprite_instance *root_sprite;
    int frame = g.runtime_frame;

    if (!root_movie || g.input_profile != FLASH_INPUT_PROFILE_STICKRPG ||
        !g.stickrpg_gameplay_shortcut_done)
        return;

    root_sprite = gameswf::cast_to<gameswf::sprite_instance>(root_movie);
    if (!root_sprite)
        return;

    /* Frames 3 and 4 are the traffic/boundary damage loop.  This GameSWF
     * port cannot safely re-enter that loop after the shortened startup:
     * removed character-creation objects remain referenced by its actions.
     * Cancel the transition and move traffic back to its off-screen spawn
     * points so holding a direction cannot immediately retrigger it. */
    if (g.stickrpg_previous_root_frame == 1 &&
        (frame == 2 || frame == 3)) {
        gameswf::as_value hpmax;

        root_movie->set_member("xmove", gameswf::as_value(0));
        root_movie->set_member("ymove", gameswf::as_value(0));
        root_movie->set_member("prevmoving", gameswf::as_value(0));
        root_movie->set_member("car1s", gameswf::as_value(0));
        root_movie->set_member("car1y", gameswf::as_value(-800));
        root_movie->set_member("car2s", gameswf::as_value(1));
        root_movie->set_member("car2y", gameswf::as_value(825));
        if (root_movie->get_member("hpmax", &hpmax))
            root_movie->set_member("hp", hpmax);
        root_sprite->goto_frame(1);
        root_sprite->m_action_list.clear();
        root_sprite->m_goto_frame_action_list.clear();
        root_sprite->set_play_state(gameswf::character::STOP);
        flash_root()->stop_drag();
        frame = 1;
        g.runtime_frame = 1;
    }

    g.stickrpg_previous_root_frame = frame;
}

static void log_stickrpg_root_layers(gameswf::sprite_instance *root_sprite)
{
    if (!root_sprite)
        return;

    for (int i = 0; i < root_sprite->m_display_list.size(); i++) {
        gameswf::character *ch = root_sprite->m_display_list.get_character(i);
        if (!ch || (!ch->get_visible() && ch->get_depth() < 250))
            continue;
        gameswf::rect bound;
        ch->get_bound(&bound);
        flash_logf("stickrpg layer i=%d depth=%d id=%d name=%s vis=%d frame=%d/%d bound10=%d,%d-%d,%d",
                   i, ch->get_depth(), ch->get_id(), ch->get_name().c_str(),
                   ch->get_visible() ? 1 : 0, ch->get_current_frame(),
                   ch->get_frame_count(),
                   flash_log_fixed(TWIPS_TO_PIXELS(bound.m_x_min), 10),
                   flash_log_fixed(TWIPS_TO_PIXELS(bound.m_y_min), 10),
                   flash_log_fixed(TWIPS_TO_PIXELS(bound.m_x_max), 10),
                   flash_log_fixed(TWIPS_TO_PIXELS(bound.m_y_max), 10));
    }
}

static void flash_key_notify(gameswf::key::code key, bool down)
{
    if (g_player && flash_root())
        g_player->notify_key_event(key, down);
}

enum
{
    STICKRPG_DIR_LEFT  = 1 << 0,
    STICKRPG_DIR_RIGHT = 1 << 1,
    STICKRPG_DIR_UP    = 1 << 2,
    STICKRPG_DIR_DOWN  = 1 << 3,
};

static bool runtime_input_ignored(void);

static void sync_stickrpg_direction_key(bool desired, bool *current,
                                        gameswf::key::code key)
{
    if (desired == *current)
        return;
    flash_key_notify(key, desired);
    *current = desired;
}

static void poll_stickrpg_direction_controls(void)
{
    unsigned int directions = 0;
    int held;

    if (g.input_profile != FLASH_INPUT_PROFILE_STICKRPG ||
        !g.runtime_loaded)
        return;

    if (!runtime_input_ignored() && g.runtime_frame == 1) {
        held = rb->button_status();
        if (held & BUTTON_LEFT)
            directions |= STICKRPG_DIR_LEFT;
        if (held & BUTTON_RIGHT)
            directions |= STICKRPG_DIR_RIGHT;
        if (held & BUTTON_MENU)
            directions |= STICKRPG_DIR_UP;
        if (held & BUTTON_PLAY)
            directions |= STICKRPG_DIR_DOWN;

#ifdef HAVE_WHEEL_POSITION
        {
            int position = rb->wheel_status();
            unsigned int wheel_directions = 0;

            if (position >= 0) {
                int zone = ((position + 6) / 12) & 7;
                static const unsigned char wheel_map[8] = {
                    STICKRPG_DIR_UP,
                    STICKRPG_DIR_UP | STICKRPG_DIR_RIGHT,
                    STICKRPG_DIR_RIGHT,
                    STICKRPG_DIR_RIGHT | STICKRPG_DIR_DOWN,
                    STICKRPG_DIR_DOWN,
                    STICKRPG_DIR_DOWN | STICKRPG_DIR_LEFT,
                    STICKRPG_DIR_LEFT,
                    STICKRPG_DIR_LEFT | STICKRPG_DIR_UP,
                };
                wheel_directions = wheel_map[zone];
            }
            g.stickrpg_wheel_dirs = wheel_directions;
            directions |= wheel_directions;
        }
#endif
    }
#ifdef HAVE_WHEEL_POSITION
    else {
        g.stickrpg_wheel_dirs = 0;
    }
#endif

    sync_stickrpg_direction_key(
        (directions & STICKRPG_DIR_LEFT) != 0,
        &g.key_left_down, gameswf::key::LEFT);
    sync_stickrpg_direction_key(
        (directions & STICKRPG_DIR_RIGHT) != 0,
        &g.key_right_down, gameswf::key::RIGHT);
    sync_stickrpg_direction_key(
        (directions & STICKRPG_DIR_UP) != 0,
        &g.key_up_down, gameswf::key::UP);
    sync_stickrpg_direction_key(
        (directions & STICKRPG_DIR_DOWN) != 0,
        &g.key_down_down, gameswf::key::DOWN);
}

static void release_runtime_keys(void)
{
#ifdef HAVE_WHEEL_POSITION
    g.stickrpg_wheel_dirs = 0;
#endif
    if (g.key_left_down) {
        flash_key_notify(gameswf::key::LEFT, false);
        g.key_left_down = false;
    }
    if (g.key_right_down) {
        flash_key_notify(gameswf::key::RIGHT, false);
        g.key_right_down = false;
    }
    if (g.key_confirm_down) {
        flash_key_notify(gameswf::key::ENTER, false);
        g.key_confirm_down = false;
    }
    if (g.key_up_down) {
        flash_key_notify(gameswf::key::UP, false);
        g.key_up_down = false;
    }
    if (g.key_down_down) {
        flash_key_notify(gameswf::key::DOWN, false);
        g.key_down_down = false;
    }
    if (g.key_shift_down) {
        flash_key_notify(gameswf::key::SHIFT, false);
        g.key_shift_down = false;
    }
    if (g.key_up_frames > 0) {
        if (!g.key_up_down)
            flash_key_notify(gameswf::key::UP, false);
        g.key_up_frames = 0;
    }
    if (g.key_down_frames > 0) {
        if (!g.key_down_down)
            flash_key_notify(gameswf::key::DOWN, false);
        g.key_down_frames = 0;
    }
}

static bool runtime_input_ignored(void)
{
    return g.runtime_loaded && TIME_BEFORE(*rb->current_tick,
                                          g.input_ignore_until);
}

static void begin_runtime_input_grace(void)
{
    g.input_ignore_until = *rb->current_tick + HZ;
    g.mouse_down = false;
    g.click_frames = 0;
    release_runtime_keys();
    rb->button_clear_queue();
#ifdef HAVE_WHEEL_POSITION
    g.wheel_touch_active = rb->wheel_status() >= 0;
#endif
    flash_logf("input grace until=%ld", g.input_ignore_until);
}

static void tick_transient_runtime_keys(void)
{
    if (g.key_up_frames > 0 && --g.key_up_frames == 0)
        if (!g.key_up_down)
            flash_key_notify(gameswf::key::UP, false);
    if (g.key_down_frames > 0 && --g.key_down_frames == 0)
        if (!g.key_down_down)
            flash_key_notify(gameswf::key::DOWN, false);
}

static void force_stickrpg_post_create_state(void)
{
    gameswf::character *root_movie;
    gameswf::sprite_instance *root_sprite;

    if (!g.stickrpg_gameplay_shortcut_enabled ||
        !g.stickrpg_intro_hide_requested ||
        g.stickrpg_gameplay_shortcut_done || !flash_root())
        return;

    root_movie = flash_root()->get_root_movie();
    root_sprite = gameswf::cast_to<gameswf::sprite_instance>(root_movie);
    if (!root_sprite)
        return;

    /* Character creation normally starts an 850-frame filmscreen whose
     * terminal action is exactly _root.gotoAndStop(2).  The transition clip
     * must be removed for GameSWF hit testing, so apply its terminal state
     * immediately after the real Create Character button hides introscreen. */
    root_sprite->goto_frame(1);
    root_sprite->set_play_state(gameswf::character::STOP);
    root_sprite->set_display_object_visible("introscreen", false);
    root_sprite->set_display_object_visible("filmscreen", false);
    root_sprite->set_display_object_visible("black", false);
    root_sprite->set_display_object_visible("pregameMC", false);
    /* The skipped character-creation film normally issues stopDrag before
     * removing its interactive clips.  Clear that state explicitly so the
     * next gameplay advance cannot follow a clip that no longer exists. */
    flash_root()->stop_drag();
    g.stickrpg_gameplay_shortcut_done = true;
    g.stickrpg_post_create_forced = true;
    flash_logf("stickrpg entered gameplay after character creation");
}

static void render_runtime_frame(float dt)
{
    int mx;
    int my;
    bool first_runtime_frame;
#ifdef SIMULATOR
    bool skip_draw;
#endif

    if (!g.runtime_loaded || !flash_root())
        return;

    first_runtime_frame = g.rendered_frames == 0;
    if (first_runtime_frame)
        flash_progress_checkpoint("first advance begin", 95);

#ifdef SIMULATOR
    run_autorun_click_script();
    run_autorun_save_call();
    if (g.autorun_select_frame > 0 &&
        g.rendered_frames == g.autorun_select_frame)
        dispatch_runtime_mouse_click();
    if (g.right_down_frame > 0 &&
        g.rendered_frames == g.right_down_frame &&
        !g.key_right_down) {
        flash_key_notify(gameswf::key::RIGHT, true);
        g.key_right_down = true;
        flash_logf("autorun right down frame=%d", g.rendered_frames);
    }
    if (g.right_up_frame > 0 &&
        g.rendered_frames == g.right_up_frame &&
        g.key_right_down) {
        flash_key_notify(gameswf::key::RIGHT, false);
        g.key_right_down = false;
        flash_logf("autorun right up frame=%d", g.rendered_frames);
    }
    skip_draw = g.fast_autorun && g.autorun_click_count > 0 &&
        (g.autorun_click_next < g.autorun_click_count ||
         g.autorun_click_armed || g.click_frames > 0);
    if (!skip_draw && g.fast_draw_interval > 1 &&
        g.autorun_click_count > 0 &&
        g.autorun_click_next >= g.autorun_click_count &&
        (g.rendered_frames % g.fast_draw_interval) != 0)
        skip_draw = true;
#endif
    runtime_mouse_stage(&mx, &my);
    poll_stickrpg_direction_controls();
    flash_root()->notify_mouse_state(mx, my, g.mouse_down ? 1 : 0);
    flash_root()->advance(dt);
    if (first_runtime_frame)
        flash_progress_checkpoint("first advance complete", 96);
    finalize_stickrpg_host_intro();
    detect_stickrpg_authored_ui();
    /* GameSWF roots begin with a one-second bootstrap remainder.  Let the
     * first advance perform its normal one-frame load initialization before
     * enabling catch-up; otherwise realtime mode would execute ~35 startup
     * frames in one hardware iteration. */
    if (g.input_profile == FLASH_INPUT_PROFILE_STICKRPG &&
        !g.stickrpg_realtime_clock_enabled && g_player) {
        g_player->set_force_realtime_framerate(true);
        g.stickrpg_realtime_clock_enabled = true;
        flash_logf("native SWF clock enabled fps=%d.%02d",
                   g.info.fps_x100 / 100, g.info.fps_x100 % 100);
    }
    prime_stickrpg_post_advance();
    prefill_stickrpg_character_name();
    {
        bool shortcut_was_done = g.stickrpg_gameplay_shortcut_done;
        force_stickrpg_post_create_state();
        if (!shortcut_was_done && g.stickrpg_gameplay_shortcut_done) {
            flash_root()->advance(0.0f);
            prime_stickrpg_post_advance();
            prefill_stickrpg_character_name();
            gameswf::character *spawn_root = flash_root()->get_root_movie();
            gameswf::sprite_instance *spawn_sprite =
                gameswf::cast_to<gameswf::sprite_instance>(spawn_root);
            if (spawn_root && spawn_sprite)
                log_stickrpg_scene_vars("spawn", spawn_root, spawn_sprite);
            log_stickrpg_root_layers(spawn_sprite);
            stabilize_stickrpg_spawn(spawn_root);
        }
    }
    load_gameswf_cache_after_instance();
    tick_transient_runtime_keys();
    g.runtime_frame = flash_root()->get_current_frame();
    sync_stickrpg_clock();
    if (g.input_profile == FLASH_INPUT_PROFILE_STICKRPG &&
        g.runtime_frame == 1 && !g.stickrpg_scene_initialized) {
        gameswf::character *scene_root = flash_root()->get_root_movie();
        gameswf::sprite_instance *scene_sprite =
            gameswf::cast_to<gameswf::sprite_instance>(scene_root);
        if (scene_root && scene_sprite) {
            log_stickrpg_scene_vars("natural", scene_root, scene_sprite);
            stabilize_stickrpg_spawn(scene_root);
            g.stickrpg_scene_initialized = true;
        }
    }
    cancel_stickrpg_unsafe_damage_transition(
        flash_root()->get_root_movie());
    sync_stickrpg_world(flash_root()->get_root_movie());
    g.rendered_frames++;
#ifdef SIMULATOR
    if (g.autorun_scroll_frame > 0 &&
        g.rendered_frames == g.autorun_scroll_frame)
        snap_stickrpg_cursor(
            0, g.autorun_scroll_direction < 0 ? -1 : 1);
    if (skip_draw) {
        if ((g.rendered_frames % 25) == 1 || g.mouse_down) {
            flash_logf("fast frame n=%d root=%d mouse_lcd=%d,%d mouse_stage=%d,%d down=%d",
                       g.rendered_frames, g.runtime_frame, g.cursor_x,
                       g.cursor_y, mx, my, g.mouse_down ? 1 : 0);
        }
        if (g.click_frames > 0 && --g.click_frames == 0) {
            g.mouse_down = false;
            if (g.autorun_click_count > 0)
                flash_logf("autorun release frame=%d lcd=%d,%d",
                           g.rendered_frames, g.cursor_x, g.cursor_y);
        }
        return;
    }
#endif
    g.shape_mesh_budget = g.shape_mesh_budget_base;
    g.shape_mesh_skipped = 0;
    if (g.startup_prerender_frames > 0 && g_renderer) {
        bool old_discard = g_renderer->discard_render;
        int warm_index = FLASH_STARTUP_PRERENDER_FRAMES -
            g.startup_prerender_frames + 1;

        rb->snprintf(g.status, sizeof(g.status),
                     "warming gameplay %d/%d",
                     warm_index, FLASH_STARTUP_PRERENDER_FRAMES);
        show_load_status_progress(g.status, 88 +
                                  (warm_index * 8) /
                                  FLASH_STARTUP_PRERENDER_FRAMES);
        flash_logf("startup prerender frame=%d begin",
                   warm_index);
        g_renderer->discard_render = true;
        flash_root()->display();
        g_renderer->discard_render = old_discard;
        g.startup_prerender_frames--;
        flash_logf("startup prerender frame=%d end tri=%d sf=%d bf=%d heap_free=%luK",
                   warm_index, g_renderer->triangles,
                   g_renderer->solid_fills, g_renderer->bitmap_fills,
                   (unsigned long)(plugin_cxx_available() / 1024));
        show_load_status_progress("warming gameplay cache", 94);
        if (g.click_frames > 0 && --g.click_frames == 0)
            g.mouse_down = false;
        rb->yield();
        return;
    }
    if (g.rendered_frames < 3) {
        rb->snprintf(g.status, sizeof(g.status), "rendering frame %d",
                     g.rendered_frames + 1);
        show_load_status_progress(g.status, 88 + g.rendered_frames * 4);
    }
    prepare_stickrpg_person();
    if (first_runtime_frame)
        flash_progress_checkpoint("first display begin", 97);
    flash_root()->display();
    if (first_runtime_frame)
        flash_progress_checkpoint("first display complete", 98);
    rb->yield();
    draw_bitmap_text_overlay();
    draw_stickrpg_hud_text();
    draw_runtime_cursor();
#if defined(SIMULATOR)
    if (g.debug_overlay && g_renderer) {
        rb->lcd_set_foreground(LCD_RGBPACK(0, 0, 0));
        rb->lcd_fillrect(0, LCD_HEIGHT - 30, LCD_WIDTH, 30);
        rb->lcd_set_foreground(LCD_RGBPACK(255, 255, 255));
        rb->lcd_putsxyf(2, LCD_HEIGHT - 28, "fr %d tri %d/%d tiny %d sf %d bf %d%s",
                        g.runtime_frame,
                        g_renderer->triangles, FLASH_TRIANGLE_BUDGET,
                        g_renderer->tiny_triangles, g_renderer->solid_fills,
                        g_renderer->bitmap_fills,
                        g_renderer->triangle_budget_hit ? " cap" : "");
        rb->lcd_putsxyf(2, LCD_HEIGHT - 14, "pix o%d b%d s%d sp%d sol %d,%d,%d,%d",
                        g_renderer->opaque_pixels, g_renderer->alpha_blends,
                        g_renderer->alpha_skips, g_renderer->span_pixels,
                        g_renderer->last_solid.m_r, g_renderer->last_solid.m_g,
                        g_renderer->last_solid.m_b, g_renderer->last_solid.m_a,
                        g_renderer->last_bitmap_sample.m_r);
        rb->lcd_putsxyf(2, 2, "prime r%d%d%d fs%d f%d%d%d ff%d",
                        g.prime_root_start, g.prime_root_load2,
                        g.prime_root_load, g.prime_has_filmscreen,
                        g.prime_film_start, g.prime_film_load2,
                        g.prime_film_load, g.prime_film_frame);
    }
#endif
    if (first_runtime_frame)
        flash_progress_checkpoint("first lcd update begin", 99);
    rb->lcd_update();
    if (first_runtime_frame)
        flash_progress_checkpoint("first lcd update complete", 100);
    if (g.stickrpg_authored_ui_ready &&
        !g.stickrpg_authored_ui_presented) {
        g.stickrpg_authored_ui_presented = true;
        flash_logf("stickrpg authored UI presented frame=%d",
                   g.rendered_frames);
        flash_progress_checkpoint("authored menu presented", 100);
    }
    if (g_renderer &&
        ((g.rendered_frames % 35) == 1 || g.mouse_down ||
         g_renderer->triangle_budget_hit)) {
        gameswf::character *person = stickrpg_person();
        const gameswf::matrix *person_matrix =
            person ? &person->get_matrix() : NULL;
        flash_logf("frame n=%d root=%d ml=%d,%d ms=%d,%d d=%d k=%d%d%d%d p10=%d@%d,%d tri=%d tiny=%d max=%dx%d fill=%d/%d skip=%d line=%d bmp=%d mask=%d/%d/%d/%d pix=%d/%d/%d heap=%luK",
                   g.rendered_frames, g.runtime_frame, g.cursor_x, g.cursor_y,
                   mx, my, g.mouse_down ? 1 : 0,
                   g.key_left_down ? 1 : 0, g.key_up_down ? 1 : 0,
                   g.key_right_down ? 1 : 0, g.key_down_down ? 1 : 0,
                   person ? person->get_current_frame() : -1,
                   person_matrix ?
                       flash_log_fixed(TWIPS_TO_PIXELS(
                           person_matrix->m_[0][2]), 10) : 0,
                   person_matrix ?
                       flash_log_fixed(TWIPS_TO_PIXELS(
                           person_matrix->m_[1][2]), 10) : 0,
                   g_renderer->triangles,
                   g_renderer->tiny_triangles, g_renderer->max_tri_w,
                   g_renderer->max_tri_h, g_renderer->solid_fills,
                   g_renderer->bitmap_fills, g.shape_mesh_skipped,
                   g_renderer->lines,
                   g_renderer->bitmaps,
                   g_renderer->mask_tests, g_renderer->mask_begins,
                   g_renderer->mask_ends, g_renderer->mask_disables,
                   g_renderer->opaque_pixels, g_renderer->alpha_blends,
                   g_renderer->alpha_skips,
                   (unsigned long)(plugin_cxx_available() / 1024));
    }
#ifdef SIMULATOR
    if (g.input_profile == FLASH_INPUT_PROFILE_STICKRPG &&
        g.runtime_frame == 1 && (g.rendered_frames % 10) == 0) {
        gameswf::character *root_movie = flash_root()->get_root_movie();
        gameswf::sprite_instance *root_sprite =
            gameswf::cast_to<gameswf::sprite_instance>(root_movie);
        gameswf::character *person = stickrpg_person();
        gameswf::as_value map_value;
        gameswf::as_value mapx_value;
        gameswf::as_value mapy_value;
        gameswf::as_value xmove_value;
        gameswf::as_value ymove_value;
        gameswf::character *map = root_movie &&
            root_movie->get_member("Map_Outside_1", &map_value) ?
            gameswf::cast_to<gameswf::character>(map_value.to_object()) : NULL;
        const gameswf::matrix *pm = person ? &person->get_matrix() : NULL;
        const gameswf::matrix *mm = map ? &map->get_matrix() : NULL;
        char mapx[24] = "?";
        char mapy[24] = "?";
        char xmove[24] = "?";
        char ymove[24] = "?";
        if (root_sprite) {
            rb->strlcpy(mapx, root_sprite->get_variable("mapx"),
                        sizeof(mapx));
            rb->strlcpy(mapy, root_sprite->get_variable("mapy"),
                        sizeof(mapy));
            rb->strlcpy(xmove, root_sprite->get_variable("xmove"),
                        sizeof(xmove));
            rb->strlcpy(ymove, root_sprite->get_variable("ymove"),
                        sizeof(ymove));
        }
        bool has_mapx = root_movie &&
            root_movie->get_member("mapx", &mapx_value);
        bool has_mapy = root_movie &&
            root_movie->get_member("mapy", &mapy_value);
        bool has_xmove = root_movie &&
            root_movie->get_member("xmove", &xmove_value);
        bool has_ymove = root_movie &&
            root_movie->get_member("ymove", &ymove_value);
        flash_logf("move n=%d keys=%d%d%d%d p=%.1f,%.1f pf=%d map=%.1f,%.1f vars=%s%d,%s%d move=%s%d,%s%d env=%s,%s/%s,%s",
                   g.rendered_frames,
                   g.key_left_down ? 1 : 0, g.key_up_down ? 1 : 0,
                   g.key_right_down ? 1 : 0, g.key_down_down ? 1 : 0,
                   pm ? TWIPS_TO_PIXELS(pm->m_[0][2]) : 0.0f,
                   pm ? TWIPS_TO_PIXELS(pm->m_[1][2]) : 0.0f,
                   person ? person->get_current_frame() : -1,
                   mm ? TWIPS_TO_PIXELS(mm->m_[0][2]) : 0.0f,
                   mm ? TWIPS_TO_PIXELS(mm->m_[1][2]) : 0.0f,
                   has_mapx ? "m" : "e", has_mapx ? mapx_value.to_int() : 0,
                   has_mapy ? "m" : "e", has_mapy ? mapy_value.to_int() : 0,
                   has_xmove ? "m" : "e", has_xmove ? xmove_value.to_int() : 0,
                   has_ymove ? "m" : "e", has_ymove ? ymove_value.to_int() : 0,
                   mapx, mapy, xmove, ymove);
    }
#endif
    if (g.click_frames > 0 && --g.click_frames == 0) {
        g.mouse_down = false;
#ifdef SIMULATOR
        if (g.autorun_click_count > 0)
            flash_logf("autorun release frame=%d lcd=%d,%d",
                       g.rendered_frames, g.cursor_x, g.cursor_y);
#endif
    }
}

static void init_runtime_heap(void);

#ifdef HAVE_WHEEL_POSITION
static void wheel_pos_to_cursor(int pos, int *x, int *y)
{
    static const int xs[8] = {160, 238, 286, 238, 160, 82, 34, 82};
    static const int ys[8] = {28, 52, 120, 188, 212, 188, 120, 52};
    int sector;

    if (pos < 0)
        pos = 0;
    sector = ((pos + 6) % 96) / 12;
    if (sector < 0)
        sector = 0;
    if (sector > 7)
        sector = 7;

    *x = xs[sector];
    *y = ys[sector];
}

static void poll_wheel_mouse_tap(void)
{
    int pos;

    if (!g.runtime_loaded)
        return;

    pos = rb->wheel_status();
    if (runtime_input_ignored()) {
        g.wheel_touch_active = pos >= 0;
        g.mouse_down = false;
        g.click_frames = 0;
        return;
    }

    if (pos >= 0) {
        wheel_pos_to_cursor(pos, &g.cursor_x, &g.cursor_y);
        if (!g.wheel_touch_active) {
            g.mouse_down = true;
            g.click_frames = 2;
            g.wheel_touch_pos = pos;
            flash_logf("wheel tap pos=%d lcd=%d,%d",
                       pos, g.cursor_x, g.cursor_y);
        }
        g.wheel_touch_active = true;
    } else {
        g.wheel_touch_active = false;
    }
}
#endif

static void prime_stickrpg_filmscreen(void)
{
    if (!g.prime_stickrpg)
        return;

    gameswf::character *root_movie;
    gameswf::as_value val;
    gameswf::sprite_instance *root_sprite;
    static const char *const overlay_names[] = {
        "filmscreen", "fpsShower", "black", "loaderText", "pregameMC"
    };
    static const char *const intro_panel_names[] = {
        "makechar", "instructions", "newgame"
    };
    static const int overlay_depths[] = { 282, 285, 287, 290, 292 };
    int i;

    if (!flash_root())
        return;

    flash_root()->set_play_state(gameswf::character::PLAY);
    g.prime_root_start = 0;
    g.prime_root_load2 = 0;
    g.prime_root_load = 0;

    root_movie = flash_root()->get_root_movie();
    root_sprite = gameswf::cast_to<gameswf::sprite_instance>(root_movie);

    for (i = 0; i < (int)(sizeof(overlay_names) / sizeof(overlay_names[0])); i++) {
        gameswf::character *ch;

        if (!root_movie || !root_movie->get_member(overlay_names[i], &val))
            continue;

        ch = gameswf::cast_to<gameswf::character>(val.to_object());
        if (!ch)
            continue;

        ch->set_visible(false);
        if (rb->strcmp(overlay_names[i], "filmscreen") == 0) {
            g.prime_has_filmscreen = 1;
            g.prime_film_frame = ch->get_current_frame();
        }
    }

    if (root_sprite) {
        for (i = 0; i < (int)(sizeof(overlay_depths) / sizeof(overlay_depths[0])); i++) {
            int id = root_sprite->get_id_at_depth(overlay_depths[i]);
            if (id >= 0)
                root_sprite->remove_display_object(overlay_depths[i], id);
        }
    }

    if (root_movie && root_movie->get_member("introscreen", &val)) {
        gameswf::character *intro = gameswf::cast_to<gameswf::character>(val.to_object());
        if (intro) {
            intro->set_visible(true);
            for (i = 0; i < (int)(sizeof(intro_panel_names) / sizeof(intro_panel_names[0])); i++) {
                gameswf::as_value panel_val;
                gameswf::character *panel;

                if (!intro->get_member(intro_panel_names[i], &panel_val))
                    continue;

                panel = gameswf::cast_to<gameswf::character>(panel_val.to_object());
                if (panel)
                    panel->set_visible(false);
            }
        }
    }

}

static bool start_runtime(void)
{
    if (g.runtime_loaded)
        return true;

    if (!g.runtime_ready && g.loaded) {
        rb->snprintf(g.status, sizeof(g.status), "starting AVM1 VM");
        show_load_status_progress(g.status, 35);
        init_runtime_heap();
        probe_gameswf_runtime();
    }

    if (g.runtime_ready && flash_root()) {
        g.runtime_frame = flash_root()->get_current_frame();
        g.runtime_loaded = true;
        ipod_engine_frame_clock_reset(&g.frame_clock, g.info.fps_x100);
        rb->snprintf(g.status, sizeof(g.status), "VM running: frame %d",
                     g.runtime_frame);
        show_load_status_progress(g.status, 86);
        begin_runtime_input_grace();
        flash_logf("runtime loaded frame=%d heap_free=%luK",
                   g.runtime_frame,
                   (unsigned long)(plugin_cxx_available() / 1024));
        prime_stickrpg_filmscreen();
        if (!g.stickrpg_fast_load) {
            flash_root()->display();
            rb->yield();
            draw_runtime_cursor();
            rb->lcd_update();
        }
        return true;
    }

    return false;
}

static void init_runtime_heap(void)
{
    unsigned char *audio = (unsigned char *)g.cxx_buf;
    unsigned char *audio_heap = NULL;
    unsigned char *raw_heap = NULL;
    size_t audio_heap_size = 0;
    size_t raw_heap_size = 0;
    size_t raw_used = 0;
    bool fws_in_audio = false;

    if (g.cxx_heap_ready)
        return;

    if (audio && g.fws >= audio && g.fws < audio + g.cxx_size) {
        size_t used = align16_size((size_t)(g.fws - audio) + g.fws_len);
        fws_in_audio = true;
        if (used < g.cxx_size) {
            audio_heap = audio + used;
            audio_heap_size = g.cxx_size - used;
        }
    } else if (g.cxx_buf && g.cxx_size >= FLASH_MIN_CXX_HEAP) {
        audio_heap = audio;
        audio_heap_size = g.cxx_size;
    }

    if (g.raw && g.buffer_size >= FLASH_MIN_CXX_HEAP) {
        raw_used = align16_size(g.raw_len);
        if (g.fws && g.fws >= g.raw && g.fws < g.raw + g.buffer_size) {
            size_t fws_end = (size_t)(g.fws - g.raw) + g.fws_len;
            if (fws_end > raw_used)
                raw_used = fws_end;
        }
        raw_used = align16_size(raw_used);
        if (raw_used < g.buffer_size) {
            raw_heap = g.raw + raw_used;
            raw_heap_size = g.buffer_size - raw_used;
        }
    }

    if (audio_heap && audio_heap_size >= FLASH_MIN_CXX_HEAP) {
        plugin_cxx_init(audio_heap, audio_heap_size);
        g.cxx_heap = audio_heap;
        g.cxx_heap_size = audio_heap_size;
        g.cxx_heap_ready = plugin_cxx_available() > 0;
        flash_logf("heap audio_tail start=0x%08lx size=%luK free=%luK fws_in_audio=%d",
                   (unsigned long)audio_heap,
                   (unsigned long)(audio_heap_size / 1024),
                   (unsigned long)(plugin_cxx_available() / 1024),
                   fws_in_audio ? 1 : 0);
        if (g.cxx_heap_ready)
            return;
    }

    if (raw_heap && raw_heap_size >= FLASH_MIN_CXX_HEAP) {
        plugin_cxx_init(raw_heap, raw_heap_size);
        g.cxx_heap = raw_heap;
        g.cxx_heap_size = raw_heap_size;
        g.cxx_heap_ready = plugin_cxx_available() > 0;
        flash_logf("heap plugin_tail start=0x%08lx size=%luK free=%luK raw_used=%luK",
                   (unsigned long)raw_heap,
                   (unsigned long)(raw_heap_size / 1024),
                   (unsigned long)(plugin_cxx_available() / 1024),
                   (unsigned long)(raw_used / 1024));
        if (g.cxx_heap_ready)
            return;
    }

    flash_logf("heap unavailable audio_tail=%luK raw_tail=%luK min=%luK",
               (unsigned long)(audio_heap_size / 1024),
               (unsigned long)(raw_heap_size / 1024),
               (unsigned long)(FLASH_MIN_CXX_HEAP / 1024));
    rb->snprintf(g.status, sizeof(g.status), "no C++ heap");
}

static void shutdown_runtime(void)
{
    release_runtime_keys();

    flash_logf("shutdown loaded=%d ready=%d frame=%d rendered=%d heap_free=%luK logs=%d errors=%d last='%s'",
               g.runtime_loaded ? 1 : 0, g.runtime_ready ? 1 : 0,
               g.runtime_frame, g.rendered_frames,
               (unsigned long)(plugin_cxx_available() / 1024),
               g.log_count, g.log_errors, g.last_log);

    if (g_root_ref) {
        *g_root_ref = (gameswf::root *)NULL;
        delete g_root_ref;
        g_root_ref = NULL;
    }
    delete g_player;
    g_player = NULL;
    gameswf::set_sound_handler(NULL);
    delete g_sound_handler;
    g_sound_handler = NULL;
    gameswf::set_render_handler(NULL);
    delete g_renderer;
    g_renderer = NULL;
    close_shape_def_cache_files();

    ipod_engine_release_memory(&g.engine_memory);
    g.raw = NULL;
    g.buffer_size = 0;
    g.cxx_buf = NULL;
    g.cxx_size = 0;

    flash_log_close();
}

extern "C" enum plugin_status plugin_start(const void *parameter)
{
    const char *path = parameter ? (const char *)parameter : DEFAULT_SWF;
    bool quit = false;
    struct ipod_engine_profile engine_profile;

    flash_sim_trace("plugin_start entered");
    rb->memset(&g, 0, sizeof(g));
    g.log_fd = -1;
    g.shape_mesh_budget_base = FLASH_SHAPE_MESH_BUDGET;
    g.load_progress = 0;
    g.progress_checkpoint_value = -1;
#ifdef HAVE_WHEEL_POSITION
    g.wheel_touch_pos = -1;
#endif
    ipod_engine_get_profile(&engine_profile);
    if (!engine_profile.supported) {
        rb->splash(HZ * 2, "Flash: iPod 6G+ only");
        return PLUGIN_ERROR;
    }
    flash_sim_trace("getting plugin buffer");
    if (!ipod_engine_acquire_memory(&g.engine_memory)) {
        rb->splash(HZ * 2, "Flash: no runtime memory");
        return PLUGIN_ERROR;
    }
    g.raw = g.engine_memory.plugin;
    g.buffer_size = g.engine_memory.plugin_size;
    flash_sim_trace("getting audio buffer");
    g.cxx_buf = g.engine_memory.shared;
    g.cxx_size = g.engine_memory.shared_size;
    reserve_loading_bitmap_buffer();
    flash_sim_trace("audio buffer acquired");
    rb->lcd_setfont(FONT_SYSFIXED);
    flash_log_open();
    flash_sim_trace("persistent log opened");
    flash_logf("buffers plugin=0x%08lx/%luK audio=0x%08lx/%luK",
               (unsigned long)g.raw, (unsigned long)(g.buffer_size / 1024),
               (unsigned long)g.cxx_buf, (unsigned long)(g.cxx_size / 1024));
    flash_logf("engine target=%s model='%s' lcd=%dx%d min_plugin=%luK",
               engine_profile.target, engine_profile.model,
               engine_profile.lcd_width, engine_profile.lcd_height,
               (unsigned long)(engine_profile.plugin_buffer_min / 1024));
    gameswf::set_curve_max_pixel_error(24.0f);
    flash_logf("curve max pixel error x100=%d",
               flash_log_fixed(gameswf::get_curve_max_pixel_error(), 100));
#ifdef SIMULATOR
    {
        g.stickrpg_shortcut_frame = 1;
        g.stickrpg_person_frame_override = -1;
        g.right_down_frame = -1;
        g.right_up_frame = -1;
        g.autorun_scroll_frame = -1;
        g.autorun_scroll_direction = 1;
        g.autorun_select_frame = -1;
        g.autorun_call_save_frame = -1;
        const char *autorun = getenv("FLASHPLAYER_AUTORUN_FRAMES");
        if (autorun && autorun[0]) {
            g.autorun_frames = rb->atoi(autorun);
            flash_logf("autorun frames=%d", g.autorun_frames);
        }
        {
            const char *person_frame = getenv("FLASHPLAYER_PERSON_FRAME");
            if (person_frame && person_frame[0]) {
                g.stickrpg_person_frame_override = rb->atoi(person_frame);
                flash_logf("person frame override=%d",
                           g.stickrpg_person_frame_override);
            }
        }
        {
            const char *down_frame = getenv("FLASHPLAYER_RIGHT_DOWN_FRAME");
            const char *up_frame = getenv("FLASHPLAYER_RIGHT_UP_FRAME");
            if (down_frame && down_frame[0])
                g.right_down_frame = rb->atoi(down_frame);
            if (up_frame && up_frame[0])
                g.right_up_frame = rb->atoi(up_frame);
        }
        {
            const char *scroll_frame =
                getenv("FLASHPLAYER_AUTORUN_SCROLL_FRAME");
            const char *scroll_direction =
                getenv("FLASHPLAYER_AUTORUN_SCROLL_DIRECTION");
            const char *select_frame =
                getenv("FLASHPLAYER_AUTORUN_SELECT_FRAME");
            const char *save_frame =
                getenv("FLASHPLAYER_AUTORUN_CALL_SAVE_FRAME");
            if (scroll_frame && scroll_frame[0])
                g.autorun_scroll_frame = rb->atoi(scroll_frame);
            if (scroll_direction && scroll_direction[0])
                g.autorun_scroll_direction = rb->atoi(scroll_direction);
            if (select_frame && select_frame[0])
                g.autorun_select_frame = rb->atoi(select_frame);
            if (save_frame && save_frame[0])
                g.autorun_call_save_frame = rb->atoi(save_frame);
        }
        {
            const char *budget = getenv("FLASHPLAYER_SHAPE_MESH_BUDGET");
            if (budget && budget[0]) {
                g.shape_mesh_budget_base = rb->atoi(budget);
                if (g.shape_mesh_budget_base < 1)
                    g.shape_mesh_budget_base = FLASH_SHAPE_MESH_BUDGET;
                flash_logf("shape mesh budget=%d",
                           g.shape_mesh_budget_base);
            }
        }
        {
            const char *overlay = getenv("FLASHPLAYER_DEBUG_OVERLAY");
            g.debug_overlay = overlay && overlay[0] && rb->atoi(overlay) != 0;
        }
        {
            const char *prime = getenv("FLASHPLAYER_PRIME_STICKRPG");
            g.prime_stickrpg = prime && prime[0] && rb->atoi(prime) != 0;
        }
        {
            const char *fast = getenv("FLASHPLAYER_FAST_AUTORUN");
            g.fast_autorun = fast && fast[0] && rb->atoi(fast) != 0;
            if (g.fast_autorun)
                flash_logf("fast autorun=1");
        }
        {
            const char *shortcut = getenv("FLASHPLAYER_STICKRPG_SHORTCUT");
            g.stickrpg_gameplay_shortcut_enabled =
                shortcut && shortcut[0] && rb->atoi(shortcut) != 0;
            if (g.stickrpg_gameplay_shortcut_enabled)
                flash_logf("stickrpg shortcut=1");
        }
        {
            const char *frame = getenv("FLASHPLAYER_STICKRPG_SHORTCUT_FRAME");
            if (frame && frame[0]) {
                g.stickrpg_shortcut_frame = rb->atoi(frame);
                if (g.stickrpg_shortcut_frame < 0)
                    g.stickrpg_shortcut_frame = 0;
                flash_logf("stickrpg shortcut frame=%d",
                           g.stickrpg_shortcut_frame);
            }
        }
        {
            const char *interval = getenv("FLASHPLAYER_FAST_DRAW_INTERVAL");
            if (interval && interval[0]) {
                g.fast_draw_interval = rb->atoi(interval);
                if (g.fast_draw_interval < 1)
                    g.fast_draw_interval = 1;
                flash_logf("fast draw interval=%d", g.fast_draw_interval);
            }
        }
        {
            const char *clicks = getenv("FLASHPLAYER_AUTORUN_CLICKS");
            if (clicks && clicks[0])
                parse_autorun_clicks(clicks);
        }
    }
#endif

    if (!path || !path[0])
        path = DEFAULT_SWF;

    if (!path || !path[0]) {
        rb->snprintf(g.status, sizeof(g.status), "no SWF path");
    } else if (!load_swf(path)) {
#ifdef SIMULATOR
        if (path != SIM_SWF)
            load_swf(SIM_SWF);
#endif
    }

    if (g.loaded)
        rb->snprintf(g.status, sizeof(g.status), "SWF loaded: starting VM");
    redraw();
    start_runtime();

    while (!quit) {
        int timeout = g.runtime_loaded ?
            ipod_engine_frame_timeout(&g.frame_clock) : HZ / 8;
#ifdef HAS_BUTTON_HOLD
        if (rb->button_hold()) {
            flash_logf("hold exit");
            quit = true;
            continue;
        }
#endif
#ifdef SIMULATOR
        if (g.fast_autorun && g.runtime_loaded &&
            g.autorun_click_count > 0 &&
            (g.autorun_click_next < g.autorun_click_count ||
             g.autorun_click_armed || g.click_frames > 0))
            timeout = 0;
#endif
        int button = rb->button_get_w_tmo(timeout);
        int bare = button & ~(BUTTON_REPEAT | BUTTON_REL);

#ifdef HAS_BUTTON_HOLD
        if (rb->button_hold()) {
            flash_logf("hold exit");
            quit = true;
            continue;
        }
#endif
#ifdef HAVE_WHEEL_POSITION
        /* A 6G wheel touch reports an absolute capacitive position even when
         * the user only intends to scroll.  Treating that contact as a click
         * made Stick RPG's cursor jump and press arbitrary menu items.  Its
         * cursor is driven by normal wheel/button events below instead. */
        if (g.input_profile != FLASH_INPUT_PROFILE_STICKRPG)
            poll_wheel_mouse_tap();
#endif
        if (button == SYS_USB_CONNECTED)
        {
            shutdown_runtime();
            return PLUGIN_USB_CONNECTED;
        }
#ifdef SIMULATOR
        if (g.runtime_loaded && g.autorun_click_count > 0 &&
            (g.autorun_click_next < g.autorun_click_count ||
             g.autorun_click_armed || g.click_frames > 0) &&
            button != BUTTON_NONE) {
            continue;
        }
#endif
        if (button == BUTTON_NONE) {
            if (g.runtime_loaded) {
                render_runtime_frame(ipod_engine_frame_advance(
                    &g.frame_clock));
#ifdef SIMULATOR
                if (g.autorun_frames > 0 &&
                    g.rendered_frames >= g.autorun_frames)
                    quit = true;
#endif
            } else {
                rb->yield();
            }
            continue;
        }

        g.last_button = button;
        if (runtime_input_ignored()) {
            g.mouse_down = false;
            g.click_frames = 0;
            continue;
        }

        if (button & BUTTON_REL) {
            if ((bare == BUTTON_SELECT || bare == BUTTON_PLAY) &&
                g.runtime_loaded && g.key_confirm_down) {
                g.key_confirm_down = false;
                g.mouse_down = false;
            }
            if ((bare == BUTTON_SELECT || bare == BUTTON_PLAY) &&
                g.runtime_loaded && g.click_frames <= 0)
                g.mouse_down = false;
            if ((bare & BUTTON_LEFT) && g.runtime_loaded &&
                g.key_left_down &&
                g.input_profile != FLASH_INPUT_PROFILE_STICKRPG) {
                flash_key_notify(gameswf::key::LEFT, false);
                g.key_left_down = false;
            }
            if ((bare & BUTTON_RIGHT) && g.runtime_loaded &&
                g.key_right_down &&
                g.input_profile != FLASH_INPUT_PROFILE_STICKRPG) {
                flash_key_notify(gameswf::key::RIGHT, false);
                g.key_right_down = false;
            }
            if ((bare & BUTTON_SCROLL_FWD) && g.runtime_loaded &&
                g.key_up_down &&
                g.input_profile != FLASH_INPUT_PROFILE_STICKRPG) {
                flash_key_notify(gameswf::key::UP, false);
                g.key_up_down = false;
            }
            if ((bare & BUTTON_SCROLL_BACK) && g.runtime_loaded &&
                g.input_profile != FLASH_INPUT_PROFILE_ANTCITY &&
                g.input_profile != FLASH_INPUT_PROFILE_STICKRPG &&
                g.key_down_down) {
                flash_key_notify(gameswf::key::DOWN, false);
                g.key_down_down = false;
            }
            if ((bare & BUTTON_MENU) && g.runtime_loaded &&
                g.key_up_down &&
                g.input_profile != FLASH_INPUT_PROFILE_STICKRPG) {
                flash_key_notify(gameswf::key::UP, false);
                g.key_up_down = false;
            }
            if ((bare & BUTTON_PLAY) && g.runtime_loaded &&
                g.key_down_down &&
                g.input_profile != FLASH_INPUT_PROFILE_STICKRPG) {
                flash_key_notify(gameswf::key::DOWN, false);
                g.key_down_down = false;
            }
            if ((bare & BUTTON_SELECT) && g.runtime_loaded &&
                g.key_shift_down) {
                flash_key_notify(gameswf::key::SHIFT, false);
                g.key_shift_down = false;
            }
            if (!g.runtime_loaded)
                redraw();
            else
                present_cached_runtime_frame();
            continue;
        }

        if ((button & BUTTON_REPEAT) &&
            bare == BUTTON_MENU &&
            g.input_profile == FLASH_INPUT_PROFILE_STICKRPG) {
            flash_logf("stickrpg long Menu exit");
            quit = true;
        } else if ((button & BUTTON_REPEAT) && bare == BUTTON_SELECT) {
#ifdef SIMULATOR
            quit = true;
#endif
        } else if (g.input_profile == FLASH_INPUT_PROFILE_ANTCITY &&
                   (bare == BUTTON_SELECT || bare == BUTTON_PLAY)) {
            if (g.runtime_loaded) {
                if (!g.key_confirm_down) {
                    g.key_confirm_down = true;
                }
                g.mouse_down = true;
                g.click_frames = 4;
            } else if (start_runtime()) {
                /* Runtime started. */
            } else if (load_swf(g.path)) {
                rb->snprintf(g.status, sizeof(g.status), "SWF loaded: starting VM");
                start_runtime();
            }
            if (!g.runtime_loaded)
                redraw();
        } else if ((bare & BUTTON_SELECT) && bare != BUTTON_SELECT &&
                   g.runtime_loaded &&
                   g.input_profile != FLASH_INPUT_PROFILE_ANTCITY) {
#ifdef SIMULATOR
            if (!g.key_shift_down) {
                flash_key_notify(gameswf::key::SHIFT, true);
                g.key_shift_down = true;
            }
#endif
            if ((bare & BUTTON_LEFT) && !g.key_left_down) {
                flash_key_notify(gameswf::key::LEFT, true);
                g.key_left_down = true;
            }
            if ((bare & BUTTON_RIGHT) && !g.key_right_down) {
                flash_key_notify(gameswf::key::RIGHT, true);
                g.key_right_down = true;
            }
            if ((bare & BUTTON_MENU) && !g.key_up_down) {
                flash_key_notify(gameswf::key::UP, true);
                g.key_up_down = true;
            }
            if ((bare & BUTTON_PLAY) && !g.key_down_down) {
                flash_key_notify(gameswf::key::DOWN, true);
                g.key_down_down = true;
            }
        } else if (bare == BUTTON_MENU && g.runtime_loaded) {
            if (g.input_profile == FLASH_INPUT_PROFILE_ANTCITY) {
                quit = true;
            } else if (stickrpg_authored_cursor_mode()) {
                snap_stickrpg_cursor(0, -1);
            } else {
                if (!g.key_up_down) {
                    flash_key_notify(gameswf::key::UP, true);
                    g.key_up_down = true;
                }
            }
        } else if (bare == BUTTON_PLAY && g.runtime_loaded) {
            if (g.input_profile == FLASH_INPUT_PROFILE_ANTCITY) {
                if (!g.key_confirm_down) {
                    g.key_confirm_down = true;
                }
                g.mouse_down = true;
                g.click_frames = 4;
            } else if (stickrpg_authored_cursor_mode()) {
                snap_stickrpg_cursor(0, 1);
            } else if (!g.key_down_down) {
                flash_key_notify(gameswf::key::DOWN, true);
                g.key_down_down = true;
            }
        } else if (bare == BUTTON_SELECT) {
            if (g.runtime_loaded) {
                if (g.input_profile == FLASH_INPUT_PROFILE_STICKRPG)
                    dispatch_runtime_mouse_click();
                else {
                    g.mouse_down = true;
                    g.click_frames = 2;
                }
            } else if (start_runtime()) {
                /* Runtime started. */
            } else if (load_swf(g.path)) {
                rb->snprintf(g.status, sizeof(g.status), "SWF loaded: starting VM");
                start_runtime();
            }
            if (!g.runtime_loaded)
                redraw();
        } else if (bare == BUTTON_SCROLL_FWD || bare == BUTTON_RIGHT) {
            if (g.runtime_loaded) {
                if (stickrpg_authored_cursor_mode()) {
                    if (bare == BUTTON_RIGHT)
                        snap_stickrpg_cursor(1, 0);
                    else
                        snap_stickrpg_cursor(0, 1);
                } else if (bare == BUTTON_RIGHT) {
                    if (g.input_profile != FLASH_INPUT_PROFILE_ANTCITY &&
                        !g.key_right_down) {
                        flash_key_notify(gameswf::key::RIGHT, true);
                        g.key_right_down = true;
                    }
                    g.cursor_x += (button & BUTTON_REPEAT) ? 10 : 4;
                    if (g.cursor_x >= LCD_WIDTH)
                        g.cursor_x = LCD_WIDTH - 1;
                } else {
                    g.cursor_y += (button & BUTTON_REPEAT) ? 10 : 4;
                    if (g.cursor_y >= LCD_HEIGHT)
                        g.cursor_y = LCD_HEIGHT - 1;
                }
            } else {
                g.page = (g.page + 1) % 3;
                redraw();
            }
        } else if (bare == BUTTON_SCROLL_BACK || bare == BUTTON_LEFT) {
            if (g.runtime_loaded) {
                if (stickrpg_authored_cursor_mode()) {
                    if (bare == BUTTON_LEFT)
                        snap_stickrpg_cursor(-1, 0);
                    else
                        snap_stickrpg_cursor(0, -1);
                } else if (bare == BUTTON_LEFT) {
                    if (g.input_profile != FLASH_INPUT_PROFILE_ANTCITY &&
                        !g.key_left_down) {
                        flash_key_notify(gameswf::key::LEFT, true);
                        g.key_left_down = true;
                    }
                    g.cursor_x -= (button & BUTTON_REPEAT) ? 10 : 4;
                    if (g.cursor_x < 0)
                        g.cursor_x = 0;
                } else {
                    g.cursor_y -= (button & BUTTON_REPEAT) ? 10 : 4;
                    if (g.cursor_y < 0)
                        g.cursor_y = 0;
                }
            } else {
                g.page = (g.page + 2) % 3;
                redraw();
            }
        } else if (!g.runtime_loaded) {
            rb->snprintf(g.status, sizeof(g.status), "button 0x%08x", button);
            redraw();
        }

        if (g.runtime_loaded)
            present_cached_runtime_frame();

#ifdef SIMULATOR
        if (g.autorun_frames > 0 &&
            g.runtime_loaded && g.rendered_frames >= g.autorun_frames)
            quit = true;
#endif
    }

    {
        enum plugin_status result = g.loaded ? PLUGIN_OK : PLUGIN_ERROR;
        shutdown_runtime();
        return result;
    }
}
