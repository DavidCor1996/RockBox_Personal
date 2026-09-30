/* Static source-resolution qualification chart; no framebuffer allocation. */
#ifndef VIDEOOUT_HIRES_PATTERN_H
#define VIDEOOUT_HIRES_PATTERN_H

#include <stdint.h>

#define SVID_TEST_WIDTH 640u
#define SVID_TEST_HEIGHT 480u
#define SVID_TEST_Y_SIZE (SVID_TEST_WIDTH * SVID_TEST_HEIGHT)
#define SVID_TEST_C_SIZE (SVID_TEST_Y_SIZE / 4u)
#define SVID_TEST_FRAME_SIZE (SVID_TEST_Y_SIZE + 2u * SVID_TEST_C_SIZE)

static uint16_t svid_hires_test_pixel(unsigned x, unsigned y)
{
    /* Three by five decimal glyphs, most significant row first. */
    static const uint16_t digits[10] = {
        0x7b6f, 0x2492, 0x73e7, 0x73cf, 0x5bc9,
        0x79cf, 0x79ef, 0x7249, 0x7bef, 0x7bcf,
    };
    static const uint16_t colors[8] = {
        0xffff, 0xffe0, 0x07ff, 0x07e0,
        0xf81f, 0xf800, 0x001f, 0x8410,
    };
    unsigned row = y / 30;
    unsigned local_y = y % 30;

    if (x < 4 || x >= SVID_TEST_WIDTH - 4 || y < 4 ||
        y >= SVID_TEST_HEIGHT - 4)
        return 0xffff;
    if (local_y == 0 || x == 80 || x == 320 || y == 240)
        return 0;
    if (x < 80)
    {
        unsigned gx = (x - 16) / 4;
        unsigned gy = (local_y - 5) / 4;
        if (x >= 16 && local_y >= 5 && gy < 5 && gx < 7 && gx != 3)
        {
            unsigned digit = gx < 3 ? row / 10 : row % 10;
            unsigned bit = 14 - gy * 3 - (gx < 3 ? gx : gx - 4);
            return (digits[digit] & (1u << bit)) ? 0xffff : 0;
        }
        return 0;
    }
    if (x < 320)
        return colors[((x - 80) / 30 + row) % 8];

    /* Top: one/two-pixel vertical detail. Bottom: one/two-pixel rows.
     * These samples are generated at 640x480, never doubled from the LCD. */
    int dx = (int)x - 400;
    int dy = (int)y - 120;
    int distance = dx * dx + dy * dy;
    if (distance >= 62 * 62 && distance <= 66 * 66)
        return 0x07e0;
    unsigned step = x < 480 ? 1 : 2;
    unsigned coordinate = y < 240 ? x : y;
    return (coordinate / step) & 1 ? 0xffff : 0;
}
#endif
