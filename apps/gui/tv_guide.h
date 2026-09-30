/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TV_GUIDE_H
#define TV_GUIDE_H
#include <stdbool.h>
/* Synchronous, borrowed presentation data; never retain plugin pointers. */
enum tv_guide_command {
    TV_GUIDE_BEGIN, TV_GUIDE_FILL, TV_GUIDE_TEXT, TV_GUIDE_BITMAP,
    TV_GUIDE_PARAGRAPH, TV_GUIDE_PRESENT, TV_GUIDE_RELEASE, TV_GUIDE_PICTURE
};
struct tv_guide_picture {
    const unsigned char *planes[3];
    int width, height, stride, dar_n, dar_d;
};
bool tv_guide_render(enum tv_guide_command command, int x, int y,
                     int width, int height, unsigned color, const void *data);
#endif
