/* Transport-independent RGB565 surface. GPL-2.0-or-later. */
#ifndef DESKTOP_SURFACE_H
#define DESKTOP_SURFACE_H
#include <stddef.h>
#include "desktop_model.h"
struct desktop_surface;
/* A sink must copy/encode synchronously, or return false when busy. It must
 * not retain pixels after returning; the compositor can paint again. */
typedef bool (*desktop_surface_sink)(void *context,
    const struct desktop_surface *surface, struct desktop_rect damage);
struct desktop_surface
{
    uint16_t *pixels;
    int width, height, stride;
    uint32_t generation;
    struct desktop_rect damage;
    desktop_surface_sink present;
    void *context;
};
bool desktop_surface_init(struct desktop_surface *s, uint16_t *pixels,
    size_t bytes, int width, int height, int stride,
    desktop_surface_sink sink, void *context);
void desktop_surface_damage(struct desktop_surface *s, struct desktop_rect r);
bool desktop_surface_present(struct desktop_surface *s);
#endif
