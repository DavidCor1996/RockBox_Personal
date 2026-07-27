/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * mpegplayer video output routines
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "libmpeg2/mpeg2dec_config.h"

#include "plugin.h"
#include "mpegplayer.h"
#include "mpeg_settings.h"
#include "livetv.h"

#define VO_NON_NULL_RECT 0x1
#define VO_VISIBLE       0x2

struct vo_data
{
    int image_width;
    int image_height;
    int image_chroma_x;
    int image_chroma_y;
    int display_width;
    int display_height;
    int output_x;
    int output_y;
    int output_width;
    int output_height;
    int src_x;
    int src_y;
    unsigned flags;
    struct vo_rect rc_vid;
    struct vo_rect rc_clip;
    void (*post_draw_callback)(void);
};

#if NUM_CORES > 1
/* Cache aligned and padded to avoid clobbering other processors' cacheable
 * data */
static union {
	uint8_t __vo_data[CACHEALIGN_UP(sizeof(struct vo_data))];
	struct vo_data vo;
} vo_raw CACHEALIGN_ATTR;
#define vo vo_raw.vo
#else
static struct vo_data vo;
#endif

#if NUM_CORES > 1
static struct mutex vo_mtx SHAREDBSS_ATTR;
#endif

static inline void video_lock_init(void)
{
#if NUM_CORES > 1
    rb->mutex_init(&vo_mtx);
#endif
}

static inline void video_lock(void)
{
#if NUM_CORES > 1
    rb->mutex_lock(&vo_mtx);
#endif
}

static inline void video_unlock(void)
{
#if NUM_CORES > 1
    rb->mutex_unlock(&vo_mtx);
#endif
}


/* Draw a black rectangle if no video frame is available */
static void vo_draw_black(struct vo_rect *rc)
{
    int foreground;
    int x, y, w, h;

    video_lock();

    foreground = mylcd_get_foreground();

    mylcd_set_foreground(MYLCD_BLACK);

    if (rc)
    {
        x = rc->l;
        y = rc->t;
        w = rc->r - rc->l;
        h = rc->b - rc->t;
    }
    else
    {
#if LCD_WIDTH >= LCD_HEIGHT
        x = vo.output_x;
        y = vo.output_y;
        w = vo.output_width;
        h = vo.output_height;
#else
        x = LCD_WIDTH - vo.output_height - vo.output_y;
        y = vo.output_x;
        w = vo.output_height;
        h = vo.output_width;
#endif
    }

    mylcd_fillrect(x, y, w, h);
    mylcd_update_rect(x, y, w, h);

    mylcd_set_foreground(foreground);

    video_unlock();
}

static inline void yuv_blit(uint8_t * const * buf, int src_x, int src_y,
                            int stride, int x, int y, int width, int height)
{
    video_lock();

#ifdef HAVE_LCD_COLOR
    rb->lcd_blit_yuv(buf, src_x, src_y, stride, x, y , width, height);
#else
    grey_ub_gray_bitmap_part(buf[0], src_x, src_y, stride, x, y, width, height);
#endif

    video_unlock();
}

void stretch_image_plane(const uint8_t * src, uint8_t *dst, int stride,
                         int src_w, int src_h, int dst_w, int dst_h);

static bool vo_draw_frame_scaled(uint8_t * const * buf)
{
    size_t bufsize;
    uint8_t *yuv[3];
    void *mem;
    int scaled_w;
    int scaled_h;
    int crop_x;
    int crop_y;
    int uv_w;
    int uv_h;
    size_t y_size;
    size_t uv_size;
    size_t total;

    if (settings.display_mode != MPEG_VIDEO_DISPLAY_FILL)
        return false;

    if (vo.display_width >= SCREEN_WIDTH && vo.display_height >= SCREEN_HEIGHT)
        return false;

    scaled_w = SCREEN_WIDTH;
    scaled_h = (int)((int64_t)SCREEN_WIDTH * vo.display_height /
                     vo.display_width);

    if (scaled_h < SCREEN_HEIGHT)
    {
        scaled_h = SCREEN_HEIGHT;
        scaled_w = (int)((int64_t)SCREEN_HEIGHT * vo.display_width /
                         vo.display_height);
    }

    scaled_w = (scaled_w + 1) & ~1;
    scaled_h = (scaled_h + 1) & ~1;

    if (scaled_w < SCREEN_WIDTH || scaled_h < SCREEN_HEIGHT)
        return false;

    uv_w = scaled_w / 2;
    uv_h = scaled_h / 2;

    y_size = (size_t)scaled_w * scaled_h;
    uv_size = (size_t)uv_w * uv_h;
    total = y_size + 2u * uv_size;

    mem = mpeg2_get_buf(&bufsize);

    if (mem == NULL || bufsize < total)
        return false;

    yuv[0] = mem;
    yuv[1] = yuv[0] + y_size;
    yuv[2] = yuv[1] + uv_size;

    stretch_image_plane(buf[0], yuv[0], vo.image_width,
                        vo.display_width, vo.display_height,
                        scaled_w, scaled_h);

    stretch_image_plane(buf[1], yuv[1], vo.image_width / 2,
                        vo.display_width / 2, vo.display_height / 2,
                        uv_w, uv_h);

    stretch_image_plane(buf[2], yuv[2], vo.image_width / 2,
                        vo.display_width / 2, vo.display_height / 2,
                        uv_w, uv_h);

    crop_x = (scaled_w - SCREEN_WIDTH) / 2;
    crop_y = (scaled_h - SCREEN_HEIGHT) / 2;

    crop_x &= ~1;
    crop_y &= ~1;

    yuv_blit(yuv, crop_x + vo.output_x, crop_y + vo.output_y, scaled_w,
             vo.output_x, vo.output_y, vo.output_width, vo.output_height);

    return true;
}

void vo_draw_frame(uint8_t * const * buf)
{
    if ((vo.flags & (VO_NON_NULL_RECT | VO_VISIBLE)) !=
        (VO_NON_NULL_RECT | VO_VISIBLE))
    {
        /* Frame is hidden - either by being set invisible or is clipped
         * away - copout */
        DEBUGF("vo hidden\n");
    }
#ifdef HAVE_LCD_COLOR
    else if (mpegplayer_livetv_launch && mpegplayer_livetv_pig &&
             buf != NULL)
    {
        /* Picture in guide shows the whole picture shrunk into the corner
         * window. The ordinary path blits one screen pixel per source
         * pixel, which would show only the top left corner of the frame,
         * so scale it the way the seek thumbnail does. */
        vo_draw_frame_thumb(buf, &vo.rc_vid);
    }
#endif
    else if (buf == NULL)
    {
        /* No frame exists - draw black */
        vo_draw_black(NULL);
        DEBUGF("vo no frame\n");
    }
    else if (vo_draw_frame_scaled(buf))
    {
        /* Scaled fill handled */
    }
    else if (vo.src_x < 0 || vo.src_y < 0 ||
             vo.output_width <= 0 || vo.output_height <= 0 ||
             vo.src_x + vo.output_width > vo.image_width ||
             vo.src_y + vo.output_height > vo.image_height)
    {
        DEBUGF("vo invalid src/dst sx=%d sy=%d ow=%d oh=%d iw=%d ih=%d\n",
               vo.src_x, vo.src_y, vo.output_width, vo.output_height,
               vo.image_width, vo.image_height);
        vo_draw_black(NULL);
    }
    else
    {
        yuv_blit(buf, vo.src_x, vo.src_y, vo.image_width,
                 vo.output_x, vo.output_y, vo.output_width,
                 vo.output_height);
    }

    if (vo.post_draw_callback)
        vo.post_draw_callback();
}

static inline void vo_rect_clear_inl(struct vo_rect *rc)
{
    rc->l = rc->t = rc->r = rc->b = 0;
}

static inline bool vo_rect_empty_inl(const struct vo_rect *rc)
{
    return rc == NULL || rc->l >= rc->r || rc->t >= rc->b;
}

static inline bool vo_rects_intersect_inl(const struct vo_rect *rc1,
                                          const struct vo_rect *rc2)
{
    return !vo_rect_empty_inl(rc1) &&
           !vo_rect_empty_inl(rc2) &&
           rc1->l < rc2->r && rc1->r > rc2->l &&
           rc1->t < rc2->b && rc1->b > rc2->t;
}

/* Sets all coordinates of a vo_rect to 0 */
void vo_rect_clear(struct vo_rect *rc)
{
    vo_rect_clear_inl(rc);
}

/* Returns true if left >= right or top >= bottom */
bool vo_rect_empty(const struct vo_rect *rc)
{
    return vo_rect_empty_inl(rc);
}

/* Initializes a vo_rect using upper-left corner and extents */
void vo_rect_set_ext(struct vo_rect *rc, int x, int y,
                     int width, int height)
{
    rc->l = x;
    rc->t = y;
    rc->r = x + width;
    rc->b = y + height;
}

/* Query if two rectangles intersect */
bool vo_rects_intersect(const struct vo_rect *rc1,
                        const struct vo_rect *rc2)
{
    return vo_rects_intersect_inl(rc1, rc2);
}

/* Intersect two rectangles, placing the result in rc_dst */
bool vo_rect_intersect(struct vo_rect *rc_dst,
                       const struct vo_rect *rc1,
                       const struct vo_rect *rc2)
{
    if (rc_dst != NULL)
    {
        if (vo_rects_intersect_inl(rc1, rc2))
        {
            rc_dst->l = MAX(rc1->l, rc2->l);
            rc_dst->r = MIN(rc1->r, rc2->r);
            rc_dst->t = MAX(rc1->t, rc2->t);
            rc_dst->b = MIN(rc1->b, rc2->b);
            return true;
        }

        vo_rect_clear_inl(rc_dst);
    }

    return false;
}

bool vo_rect_union(struct vo_rect *rc_dst,
                   const struct vo_rect *rc1,
                   const struct vo_rect *rc2)
{
    if (rc_dst != NULL)
    {
        if (!vo_rect_empty_inl(rc1))
        {
            if (!vo_rect_empty_inl(rc2))
            {
                rc_dst->l = MIN(rc1->l, rc2->l);
                rc_dst->t = MIN(rc1->t, rc2->t);
                rc_dst->r = MAX(rc1->r, rc2->r);
                rc_dst->b = MAX(rc1->b, rc2->b);
            }
            else
            {
                *rc_dst = *rc1;
            }

            return true;
        }
        else if (!vo_rect_empty_inl(rc2))
        {
            *rc_dst = *rc2;
            return true;
        }

        vo_rect_clear_inl(rc_dst);
    }

    return false;
}

void vo_rect_offset(struct vo_rect *rc, int dx, int dy)
{
    rc->l += dx;
    rc->t += dy;
    rc->r += dx;
    rc->b += dy;
}

/* Shink or stretch each axis - rotate counter-clockwise to retain upright
 * orientation on rotated displays (they rotate clockwise) */
void stretch_image_plane(const uint8_t * src, uint8_t *dst, int stride,
                         int src_w, int src_h, int dst_w, int dst_h)
{
    uint8_t *dst_end = dst + dst_w*dst_h;

#if LCD_WIDTH >= LCD_HEIGHT
    int src_w2 = src_w*2;        /* 2x dimensions (for rounding before division) */
    int dst_w2 = dst_w*2;
    int src_h2 = src_h*2;
    int dst_h2 = dst_h*2;
    int qw = src_w2 / dst_w2;    /* src-dst width ratio quotient */
    int rw = src_w2 - qw*dst_w2; /* src-dst width ratio remainder */
    int qh = src_h2 / dst_h2;    /* src-dst height ratio quotient */
    int rh = src_h2 - qh*dst_h2; /* src-dst height ratio remainder */
    int dw = dst_w;              /* Width error accumulator  */
    int dh = dst_h;              /* Height error accumulator */
#else
    int src_w2 = src_w*2;
    int dst_w2 = dst_h*2;
    int src_h2 = src_h*2;
    int dst_h2 = dst_w*2;
    int qw = src_h2 / dst_w2;
    int rw = src_h2 - qw*dst_w2;
    int qh = src_w2 / dst_h2;
    int rh = src_w2 - qh*dst_h2;
    int dw = dst_h;
    int dh = dst_w;

    src += src_w - 1;
#endif

    while (1)
    {
        const uint8_t *s = src;
#if LCD_WIDTH >= LCD_HEIGHT
        uint8_t * const dst_line_end = dst + dst_w;
#else
        uint8_t * const dst_line_end = dst + dst_h;
#endif
        while (1)
        {
            *dst++ = *s;

            if (dst >= dst_line_end)
            {
                dw = dst_w;
                break;
            }

#if LCD_WIDTH >= LCD_HEIGHT
            s += qw;
#else
            s += qw*stride;
#endif
            dw += rw;

            if (dw >= dst_w2)
            {
                dw -= dst_w2;
#if LCD_WIDTH >= LCD_HEIGHT
                s++;
#else
                s += stride;
#endif
            }
        }

        if (dst >= dst_end)
            break;
#if LCD_WIDTH >= LCD_HEIGHT
        src += qh*stride;
#else
        src -= qh;
#endif
        dh += rh;

        if (dh >= dst_h2)
        {
            dh -= dst_h2;
#if LCD_WIDTH >= LCD_HEIGHT
            src += stride;
#else
            src--;
#endif
        }
    }
}

bool vo_draw_frame_thumb(uint8_t * const * buf, const struct vo_rect *rc)
{
    void *mem;
    size_t bufsize = 0;
    uint8_t *yuv[3];
    struct vo_rect thumb_rc;
    int thumb_width, thumb_height;
#ifdef HAVE_LCD_COLOR
    int thumb_uv_width, thumb_uv_height;
#endif

    /* Obtain rectangle as clipped to the screen */
    vo_rect_set_ext(&thumb_rc, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    if (!vo_rect_intersect(&thumb_rc, rc, &thumb_rc))
        return true;

    if (buf == NULL)
        goto no_thumb_exit;

    DEBUGF("thumb_rc: %d, %d, %d, %d\n", thumb_rc.l, thumb_rc.t,
           thumb_rc.r, thumb_rc.b);

    thumb_width = rc->r - rc->l;
    thumb_height = rc->b - rc->t;
#ifdef HAVE_LCD_COLOR
    thumb_uv_width = thumb_width / 2;
    thumb_uv_height = thumb_height / 2;

    DEBUGF("thumb: w: %d h: %d uvw: %d uvh: %d\n", thumb_width,
           thumb_height, thumb_uv_width, thumb_uv_height);
#else
    DEBUGF("thumb: w: %d h: %d\n", thumb_width, thumb_height);
#endif

    /* Use remaining mpeg2 buffer as temp space */
    mem = mpeg2_get_buf(&bufsize);

    if (bufsize < (size_t)(thumb_width*thumb_height)
#ifdef HAVE_LCD_COLOR
            + 2u*(thumb_uv_width * thumb_uv_height)
#endif
            )
    {
        DEBUGF("thumb: insufficient buffer\n");
        goto no_thumb_exit;
    }

    yuv[0] = mem;
    stretch_image_plane(buf[0], yuv[0], vo.image_width,
                        vo.display_width, vo.display_height,
                        thumb_width, thumb_height);

#ifdef HAVE_LCD_COLOR
    yuv[1] = yuv[0] + thumb_width*thumb_height;
    yuv[2] = yuv[1] + thumb_uv_width*thumb_uv_height;

    stretch_image_plane(buf[1], yuv[1], vo.image_width / 2,
                        vo.display_width / 2, vo.display_height / 2,
                        thumb_uv_width, thumb_uv_height);

    stretch_image_plane(buf[2], yuv[2], vo.image_width / 2,
                        vo.display_width / 2, vo.display_height / 2,
                        thumb_uv_width, thumb_uv_height);
#endif

#if LCD_WIDTH >= LCD_HEIGHT
    yuv_blit(yuv, 0, 0, thumb_width,
             thumb_rc.l, thumb_rc.t,
             thumb_rc.r - thumb_rc.l,
             thumb_rc.b - thumb_rc.t);
#else
    yuv_blit(yuv, 0, 0, thumb_height,
             thumb_rc.t, thumb_rc.l,
             thumb_rc.b - thumb_rc.t,
             thumb_rc.r - thumb_rc.l);
#endif /* LCD_WIDTH >= LCD_HEIGHT */

    return true;

no_thumb_exit:
    vo_draw_black(&thumb_rc);
    return false;
}

void vo_setup(const mpeg2_sequence_t * sequence)
{
    int scaled_w, scaled_h;
    bool youtube_embedded = mpegplayer_youtube_launch &&
                            mpegplayer_youtube_embedded;
#ifdef HAVE_LCD_COLOR
    /* Live TV picture in guide: the channel keeps decoding into the small
     * window in the corner of the guide, as on a DIRECTV receiver. */
    bool livetv_pig = mpegplayer_livetv_launch && mpegplayer_livetv_pig;
#else
    const bool livetv_pig = false;
#endif

    vo.image_width = sequence->width;
    vo.image_height = sequence->height;
    vo.display_width = sequence->display_width;
    vo.display_height = sequence->display_height;

    DEBUGF("vo_setup - w:%d h:%d\n", vo.display_width, vo.display_height);

    vo.image_chroma_x = vo.image_width / sequence->chroma_width;
    vo.image_chroma_y = vo.image_height / sequence->chroma_height;

    if (livetv_pig)
    {
        scaled_w = LIVETV_PIG_W;
        scaled_h = LIVETV_PIG_H;
        vo.src_x = 0;
        vo.src_y = 0;
    }
    else if (youtube_embedded)
    {
        scaled_w = 210;
        scaled_h = 158;
        vo.src_x = 0;
        vo.src_y = 0;
    }
    else switch (settings.display_mode)
    {
    default:
    case MPEG_VIDEO_DISPLAY_FIT:
    {
        int64_t wr = (int64_t)SCREEN_WIDTH * vo.display_height;
        int64_t hr = (int64_t)SCREEN_HEIGHT * vo.display_width;

        if (wr <= hr)
        {
            scaled_w = SCREEN_WIDTH;
            scaled_h = (int)((int64_t)SCREEN_WIDTH * vo.display_height /
                             vo.display_width);
        }
        else
        {
            scaled_h = SCREEN_HEIGHT;
            scaled_w = (int)((int64_t)SCREEN_HEIGHT * vo.display_width /
                             vo.display_height);
        }

        if (vo.display_width > scaled_w)
            vo.src_x = (vo.display_width - scaled_w) / 2;
        else
            vo.src_x = 0;

        if (vo.display_height > scaled_h)
            vo.src_y = (vo.display_height - scaled_h) / 2;
        else
            vo.src_y = 0;

        DEBUGF("FIT mode: source=%dx%d scaled=%dx%d screen=%dx%d\n",
               vo.display_width, vo.display_height,
               scaled_w, scaled_h,
               SCREEN_WIDTH, SCREEN_HEIGHT);
        break;
    }

    case MPEG_VIDEO_DISPLAY_FILL:
    {
        /* Fill mode: scale video to fill screen (preserving aspect ratio)
         * with slight overscale to ensure full coverage and visual distinction
         * from NATIVE mode. Choose the larger dimension to guarantee fill. */
        int64_t wr = (int64_t)SCREEN_WIDTH * vo.display_height;
        int64_t hr = (int64_t)SCREEN_HEIGHT * vo.display_width;

        if (wr >= hr)
        {
            /* Width-limited: height will exceed screen */
            scaled_w = SCREEN_WIDTH;
            scaled_h = (int)((int64_t)SCREEN_WIDTH * vo.display_height /
                             vo.display_width);
        }
        else
        {
            /* Height-limited: width will exceed screen */
            scaled_h = SCREEN_HEIGHT;
            scaled_w = (int)((int64_t)SCREEN_HEIGHT * vo.display_width /
                             vo.display_height);
        }

        /* Apply 5% overscale to create visible distinction and ensure complete
         * screen coverage even with rounding errors */
        scaled_w = (scaled_w * 105) / 100;
        scaled_h = (scaled_h * 105) / 100;

        /* Crop excess video from center */
        if (vo.display_width > scaled_w)
            vo.src_x = (vo.display_width - scaled_w) / 2;
        else
            vo.src_x = 0;

        if (vo.display_height > scaled_h)
            vo.src_y = (vo.display_height - scaled_h) / 2;
        else
            vo.src_y = 0;

        DEBUGF("FILL mode: source=%dx%d scaled=%dx%d screen=%dx%d\n",
               vo.display_width, vo.display_height,
               scaled_w, scaled_h,
               SCREEN_WIDTH, SCREEN_HEIGHT);
        break;
    }

    case MPEG_VIDEO_DISPLAY_NATIVE:
        scaled_w = MIN(vo.display_width, SCREEN_WIDTH);
        scaled_h = MIN(vo.display_height, SCREEN_HEIGHT);

        if (vo.display_width > SCREEN_WIDTH)
            vo.src_x = (vo.display_width - SCREEN_WIDTH) / 2;
        else
            vo.src_x = 0;

        if (vo.display_height > SCREEN_HEIGHT)
            vo.src_y = (vo.display_height - SCREEN_HEIGHT) / 2;
        else
            vo.src_y = 0;

        DEBUGF("NATIVE mode: source=%dx%d scaled=%dx%d screen=%dx%d\n",
               vo.display_width, vo.display_height,
               scaled_w, scaled_h,
               SCREEN_WIDTH, SCREEN_HEIGHT);
        break;
    }

    scaled_w = MIN(scaled_w, vo.display_width - vo.src_x);
    scaled_h = MIN(scaled_h, vo.display_height - vo.src_y);

#ifdef HAVE_LCD_COLOR
    scaled_w &= ~1;
    scaled_h &= ~1;
    vo.src_x &= ~1;
    vo.src_y &= ~1;
#endif

    if (scaled_w <= 0 || scaled_h <= 0)
    {
        vo_rect_clear(&vo.rc_vid);
        vo.flags &= ~VO_NON_NULL_RECT;
        return;
    }

    scaled_w = MAX(scaled_w, 2);
    scaled_h = MAX(scaled_h, 2);

    if (livetv_pig)
    {
        vo.rc_vid.l = LIVETV_PIG_X;
        vo.rc_vid.t = LIVETV_PIG_Y;
    }
    else if (youtube_embedded)
    {
        vo.rc_vid.l = 4;
        vo.rc_vid.t = 62;
    }
    else
    {
        vo.rc_vid.l = (SCREEN_WIDTH - scaled_w) / 2;
        vo.rc_vid.t = (SCREEN_HEIGHT - scaled_h) / 2;
    }
#ifdef HAVE_LCD_COLOR
    vo.rc_vid.l &= ~1;
    vo.rc_vid.t &= ~1;
#endif
    vo.rc_vid.r = vo.rc_vid.l + scaled_w;
    vo.rc_vid.b = vo.rc_vid.t + scaled_h;

    DEBUGF("Final output rect: (%d,%d) %dx%d  src_offset=(%d,%d)\n",
           vo.rc_vid.l, vo.rc_vid.t, scaled_w, scaled_h,
           vo.src_x, vo.src_y);

    vo_set_clip_rect(&vo.rc_clip);
}

void vo_set_display_mode(int mode)
{
    if (mode < 0 || mode >= MPEG_VIDEO_DISPLAY_NUM_MODES)
        mode = MPEG_VIDEO_DISPLAY_FIT;

    settings.display_mode = mode;
}

void vo_dimensions(struct vo_ext *sz)
{
    sz->w = vo.display_width;
    sz->h = vo.display_height;
}

bool vo_init(void)
{
    vo.flags = 0;
    vo_rect_set_ext(&vo.rc_clip, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    video_lock_init();
    return true;
}

bool vo_show(bool show)
{
    bool vis = vo.flags & VO_VISIBLE;

    if (show)
        vo.flags |= VO_VISIBLE;
    else
        vo.flags &= ~VO_VISIBLE;

    return vis;
}

bool vo_is_visible(void)
{
    return vo.flags & VO_VISIBLE;
}

void vo_cleanup(void)
{
    vo.flags = 0;
}

void vo_set_clip_rect(const struct vo_rect *rc)
{
    struct vo_rect rc_out;

    if (rc == NULL)
        vo_rect_set_ext(&vo.rc_clip, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    else
        vo.rc_clip = *rc;

    if (!vo_rect_intersect(&rc_out, &vo.rc_vid, &vo.rc_clip))
        vo.flags &= ~VO_NON_NULL_RECT;
    else
        vo.flags |= VO_NON_NULL_RECT;

    vo.output_x = rc_out.l;
    vo.output_y = rc_out.t;
    vo.output_width = rc_out.r - rc_out.l;
    vo.output_height = rc_out.b - rc_out.t;
}

bool vo_get_clip_rect(struct vo_rect *rc)
{
    rc->l = vo.output_x;
    rc->t = vo.output_y;
    rc->r = rc->l + vo.output_width;
    rc->b = rc->t + vo.output_height;
    return (vo.flags & VO_NON_NULL_RECT) != 0;
}

void vo_set_post_draw_callback(void (*cb)(void))
{
    vo.post_draw_callback = cb;
}

#if NUM_CORES > 1
void vo_lock(void)
{
    video_lock();
}

void vo_unlock(void)
{
    video_unlock();
}
#endif
