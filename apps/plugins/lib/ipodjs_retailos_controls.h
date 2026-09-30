#ifndef IPODJS_RETAILOS_CONTROLS_H
#define IPODJS_RETAILOS_CONTROLS_H
#include "ipodjs_retailos.h"

#define IPODJS_RETAILOS_PIN_PANEL_BYTES (74 * 70 * 3)
#define IPODJS_RETAILOS_PIN_FIELD_BYTES (3 * 10 * 26 * 3)
#define IPODJS_RETAILOS_PIN_SELECTED_BYTES (3 * 8 * 29 * 3)

struct ipodjs_retailos_pin_cache
{
    struct ipodjs_retailos_image panel;
    struct ipodjs_retailos_image field[3];
    struct ipodjs_retailos_image selected[3];
    unsigned char *panel_data;
    unsigned char *field_data;
    unsigned char *selected_data;
    bool tried;
    bool valid;
};

bool ipodjs_retailos_prepare_pin(struct ipodjs_retailos_pin_cache *cache);
bool ipodjs_retailos_draw_nine_slice(
    struct screen *display, const struct ipodjs_retailos_image *image,
    int x, int y, int width, int height, int border);
bool ipodjs_retailos_draw_parts(
    struct screen *display, const struct ipodjs_retailos_image parts[3],
    int x, int y, int width, int height);
#endif
