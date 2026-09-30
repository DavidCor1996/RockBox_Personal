/* Desktop geometry and navigation. GPL-2.0-or-later. */
#include "desktop_model.h"

bool desktop_hit(struct desktop_rect r, int x, int y)
{
    return r.width > 0 && r.height > 0 && x >= r.x && y >= r.y &&
           (int64_t)x < (int64_t)r.x + r.width &&
           (int64_t)y < (int64_t)r.y + r.height;
}

void desktop_damage(struct desktop_rect *r, struct desktop_rect a)
{
    if (a.width <= 0 || a.height <= 0)
        return;
    if (r->width <= 0 || r->height <= 0)
        *r = a;
    else
    {
        int right = r->x + r->width, bottom = r->y + r->height;
        if (a.x + a.width > right) right = a.x + a.width;
        if (a.y + a.height > bottom) bottom = a.y + a.height;
        if (a.x < r->x) r->x = a.x;
        if (a.y < r->y) r->y = a.y;
        r->width = right - r->x;
        r->height = bottom - r->y;
    }
}

bool desktop_image_layout(int sw, int sh, int dw, int dh,
                          enum desktop_scale mode,
                          struct desktop_rect *src, struct desktop_rect *dst)
{
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0 ||
        sw > 65535 || sh > 65535 || dw > 4096 || dh > 4096 ||
        mode < DESKTOP_FILL || mode > DESKTOP_CENTER)
        return false;
    *src = (struct desktop_rect){0, 0, sw, sh};
    *dst = (struct desktop_rect){0, 0, dw, dh};
    if (mode == DESKTOP_CENTER)
    {
        src->width = dst->width = sw < dw ? sw : dw;
        src->height = dst->height = sh < dh ? sh : dh;
    }
    else if (mode == DESKTOP_FILL)
    {
        if ((int64_t)sw * dh > (int64_t)sh * dw)
            src->width = (int64_t)sh * dw / dh;
        else
            src->height = (int64_t)sw * dh / dw;
    }
    else
    {
        if ((int64_t)sw * dh > (int64_t)sh * dw)
            dst->height = (int64_t)sh * dw / sw;
        else
            dst->width = (int64_t)sw * dh / sh;
    }
    if (src->width < 1) src->width = 1;
    if (src->height < 1) src->height = 1;
    if (dst->width < 1) dst->width = 1;
    if (dst->height < 1) dst->height = 1;
    src->x = (sw - src->width) / 2;
    src->y = (sh - src->height) / 2;
    dst->x = (dw - dst->width) / 2;
    dst->y = (dh - dst->height) / 2;
    return true;
}

void desktop_library_root(struct desktop_library *l, enum desktop_media source)
{
    l->depth = 0;
    l->current = (struct desktop_library_level){source, -1, -1, -1, 0, 0};
}

bool desktop_library_open(struct desktop_library *l, int32_t id)
{
    struct desktop_library_level next = l->current;
    if (id < 0 || l->depth >= 3)
        return false;
    switch (next.source)
    {
        case DESKTOP_ARTISTS:
            next.artist = id;
            next.source = DESKTOP_ALBUMS;
            break;
        case DESKTOP_ALBUMS:
            next.album = id;
            next.source = DESKTOP_SONGS;
            break;
        case DESKTOP_GENRES:
            next.genre = id;
            next.source = DESKTOP_ARTISTS;
            break;
        case DESKTOP_PLAYLISTS:
            next.source = DESKTOP_SONGS;
            break;
        default:
            return false;
    }
    l->history[l->depth++] = l->current;
    next.page = next.selected = 0;
    l->current = next;
    return true;
}

bool desktop_library_back(struct desktop_library *l)
{
    if (!l->depth)
        return false;
    l->current = l->history[--l->depth];
    return true;
}
