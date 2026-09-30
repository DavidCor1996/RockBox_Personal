/* Transport-independent RGB565 surface. GPL-2.0-or-later. */
#include "desktop_surface.h"
bool desktop_surface_init(struct desktop_surface *s, uint16_t *pixels,
    size_t bytes, int w, int h, int stride, desktop_surface_sink sink, void *ctx)
{
    if (!s || !pixels || !sink || w <= 0 || h <= 0 || stride < w ||
        w > 4096 || h > 4096 || stride > 4096 ||
        (size_t)stride * h > bytes / sizeof(*pixels)) return false;
    *s = (struct desktop_surface){pixels, w, h, stride, 0,
        {0, 0, w, h}, sink, ctx};
    return true;
}
void desktop_surface_damage(struct desktop_surface *s, struct desktop_rect r)
{
    /* Bounds checked before addition to avoid hostile coordinate overflow. */
    int64_t right = (int64_t)r.x + r.width;
    int64_t bottom = (int64_t)r.y + r.height;
    if (r.width <= 0 || r.height <= 0 || right <= 0 || bottom <= 0 ||
        r.x >= s->width || r.y >= s->height) return;
    if (r.x < 0) r.x = 0;
    if (r.y < 0) r.y = 0;
    if (right > s->width) right = s->width;
    if (bottom > s->height) bottom = s->height;
    r.width = right - r.x; r.height = bottom - r.y;
    desktop_damage(&s->damage, r);
}
bool desktop_surface_present(struct desktop_surface *s)
{
    if (!s->damage.width || !s->damage.height) return true;
    if (!s->present(s->context, s, s->damage)) return false;
    s->damage = (struct desktop_rect){0};
    s->generation++;
    return true;
}
