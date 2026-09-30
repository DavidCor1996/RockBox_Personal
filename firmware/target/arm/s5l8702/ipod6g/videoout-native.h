/* Preserve decoded detail while retaining the player's LCD composition.
 * Callers validate all dimensions. No allocation or source ownership. */
#ifndef VIDEOOUT_NATIVE_H
#define VIDEOOUT_NATIVE_H

/* Match video_scale_plane's fixed-point sampling exactly when deciding
 * whether a 2x2 LCD tile was changed by captions or other composition. */
static bool videoout_native_tile(const struct videoout_frame *f,
                                unsigned char * const lcd[3], int x, int y,
                                const unsigned step_x[3],
                                const unsigned step_y[3])
{
    for (int p = 0; p < 3; p++)
    {
        int div = p ? 2 : 1;
        unsigned sx = step_x[p], sy = step_y[p];
        for (int j = 0; j < 2 / div; j++)
            for (int i = 0; i < 2 / div; i++)
            {
                unsigned xx = x / div + i, yy = y / div + j;
                uint8_t original = f->planes[p][
                    ((yy * sy) >> 16) * (f->stride / div) +
                    ((xx * sx) >> 16)];
                if (lcd[p][(f->y / div + yy) * (LCD_WIDTH / div) +
                           f->x / div + xx] != original)
                    return false;
            }
    }
    return true;
}

static uint8_t videoout_native_sample(const uint8_t *src, int stride,
                                     int width, int height,
                                     unsigned x, unsigned y)
{
    unsigned ix = x >> 16, iy = y >> 16;
    unsigned fx = (x >> 8) & 255, fy = (y >> 8) & 255;
    const uint8_t *row = src + iy * stride;
    if (!(fx | fy))
        return row[ix];
    unsigned nx = ix + 1 < (unsigned)width ? ix + 1 : ix;
    const uint8_t *next = row + (iy + 1 < (unsigned)height ? stride : 0);
    unsigned a = row[ix] * (256 - fx) + row[nx] * fx;
    unsigned b = next[ix] * (256 - fx) + next[nx] * fx;
    return (a * (256 - fy) + b * fy + 32768) >> 16;
}

static void videoout_native_compose(uint8_t *dst[3], const int strides[3],
                                   const struct videoout_frame *f,
                                   unsigned char * const lcd[3])
{
    unsigned step_x[3], step_y[3], native_x[3], native_y[3];
    for (int p = 0; p < 3; p++)
    {
        int div = p ? 2 : 1;
        step_x[p] = ((unsigned)(f->width / div) << 16) /
                    (f->display_width / div);
        step_y[p] = ((unsigned)(f->height / div) << 16) /
                    (f->display_height / div);
        native_x[p] = ((unsigned)(f->width / div) << 16) /
                      (f->display_width * 2 / div);
        native_y[p] = ((unsigned)(f->height / div) << 16) /
                      (f->display_height * 2 / div);
    }
    /* Compose each tile once. Changed tiles retain all three LCD planes,
     * including translucent controls; other movie tiles use decoded detail. */
    for (int y = 0; y < LCD_HEIGHT; y += 2)
        for (int x = 0; x < LCD_WIDTH; x += 2)
        {
            int mx = x - f->x, my = y - f->y;
            bool native = mx >= 0 && my >= 0 &&
                mx < f->display_width && my < f->display_height &&
                videoout_native_tile(f, lcd, mx, my, step_x, step_y);
            for (int p = 0; p < 3; p++)
            {
                int div = p ? 2 : 1;
                const uint8_t *src = native ? f->planes[p] : lcd[p];
                int ss = (native ? f->stride : LCD_WIDTH) / div;
                int w = (native ? f->width : LCD_WIDTH) / div;
                int h = (native ? f->height : LCD_HEIGHT) / div;
                unsigned sx = native ? native_x[p] : 32768;
                unsigned sy = native ? native_y[p] : 32768;
                unsigned ax = (native ? mx : x) * 2 / div;
                unsigned ay = (native ? my : y) * 2 / div;
                for (int j = 0; j < 4 / div; j++)
                    for (int i = 0; i < 4 / div; i++)
                        dst[p][(y * 2 / div + j) * strides[p] +
                               x * 2 / div + i] =
                            videoout_native_sample(src, ss, w, h,
                                (ax + i) * sx, (ay + j) * sy);
            }
        }
}
#endif
