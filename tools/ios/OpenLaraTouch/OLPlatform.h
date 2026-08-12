#ifndef OPENLARA_TOUCH_PLATFORM_H
#define OPENLARA_TOUCH_PLATFORM_H

#include <stdint.h>

enum ol_input_key {
    OL_KEY_UP     = 1u << 0,
    OL_KEY_RIGHT  = 1u << 1,
    OL_KEY_DOWN   = 1u << 2,
    OL_KEY_LEFT   = 1u << 3,
    OL_KEY_A      = 1u << 4,
    OL_KEY_B      = 1u << 5,
    OL_KEY_C      = 1u << 6,
    OL_KEY_X      = 1u << 7,
    OL_KEY_Y      = 1u << 8,
    OL_KEY_Z      = 1u << 9,
    OL_KEY_START  = 1u << 14,
    OL_KEY_SELECT = 1u << 15
};

#ifdef __cplusplus
extern "C" {
#endif

uint32_t ol_icade_update(uint32_t state, uint16_t character,
                         int *recognized);
uint32_t ol_touch_keys(float x, float y, float width, float height);
void ol_palette_rgba(uint16_t color, uint8_t rgba[4]);

#ifdef __cplusplus
}
#endif

#endif
