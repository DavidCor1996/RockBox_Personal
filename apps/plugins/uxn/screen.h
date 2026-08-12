/* Varvara screen device adapted from the official Uxn C emulator. */

#ifndef UXN_SCREEN_H
#define UXN_SCREEN_H

#include "plugin.h"
#include "uxn.h"

#define UXN_SCREEN_MAX_WIDTH 640
#define UXN_SCREEN_MAX_HEIGHT 480
#define UXN_SCREEN_MARGIN 8
#define UXN_SCREEN_STRIDE \
    (UXN_SCREEN_MAX_WIDTH + UXN_SCREEN_MARGIN * 2)
#define UXN_SCREEN_LAYER_SIZE \
    (UXN_SCREEN_STRIDE * (UXN_SCREEN_MAX_HEIGHT + UXN_SCREEN_MARGIN * 2))
#define UXN_SCREEN_OUTPUT_SIZE (LCD_WIDTH * LCD_HEIGHT * sizeof(fb_data))

struct uxn_screen
{
    int width;
    int height;
    int vector;
    int x;
    int y;
    int addr;
    int move_x;
    int move_y;
    int move_addr;
    int length;
    int delta_x;
    int delta_y;
    bool dirty;
    uint8_t *bg;
    uint8_t *fg;
    fb_data *output;
    fb_data palette[16];
};

extern struct uxn_screen uxn_screen;

void screen_init(uint8_t *memory);
void screen_palette(void);
void screen_redraw(void);
uint8_t screen_dei(uint8_t addr);
void screen_deo(uint8_t addr);

#endif
