/* Bounded 2x bilinear enlargement of one byte plane. Source edges are
 * replicated: callers need no padding beyond the supplied rectangle.
 * No allocation, shared audio state, or out-of-rectangle reads. */
#ifndef VIDEOOUT_SCALE2X_H
#define VIDEOOUT_SCALE2X_H
#include <stdint.h>
static void videoout_scale2x(uint8_t *dst, unsigned ds,
                            const uint8_t *src, unsigned ss,
                            unsigned width, unsigned height)
{
    for (unsigned y = 0; y < height; y++)
    {
        const uint8_t *row = src + y*ss;
        const uint8_t *next = row + (y+1 < height ? ss : 0);
        uint8_t *out = dst + y*2*ds;
        for (unsigned x = 0; x < width; x++)
        {
            unsigned nx = x+1 < width ? x+1 : x;
            unsigned a = row[x], b = row[nx];
            unsigned c = next[x], d = next[nx];
            out[x*2] = a;
            out[x*2+1] = (a+b+1)/2;
            out[ds+x*2] = (a+c+1)/2;
            out[ds+x*2+1] = (a+b+c+d+2)/4;
        }
    }
}
#endif
